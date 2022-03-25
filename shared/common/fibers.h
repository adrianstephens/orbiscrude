#ifndef FIBERS_H
#define FIBERS_H

#include "base/defs.h"
#include "base/interval.h"
#include "thread.h"
#include "lockfree.h"

namespace iso {

//-----------------------------------------------------------------------------
// fiber - using fcontext
//-----------------------------------------------------------------------------

typedef void *fcontext_t;

struct transfer_t {
	fcontext_t	ctx;
	void		*data;
};

#ifdef PLAT_PS4
#define	make_fcontext		_make_fcontext
#define	jump_fcontext		_jump_fcontext
#define	ontop_fcontext		_ontop_fcontext
#endif

extern "C" fcontext_t make_fcontext(void *sp, void *stackbase, void (*fn)(transfer_t));
extern "C" transfer_t jump_fcontext(const fcontext_t ctx, void *data);
extern "C" transfer_t ontop_fcontext(const fcontext_t ctx, void *data, transfer_t (*fn)(transfer_t));


struct stack {
	struct block;
	static atomic<freelist<block>>	pool[8];
	static atomic<int>	inuse;

	void	*p;
	size_t	size;

	static int		get_pool(size_t size) {
		return clamp(log2_ceil(size), 15, 22) - 15;
	}
	static int		get_size(int i) {
		return 1 << (i + 15);
	}
	static stack	acquire(size_t size = THREAD_STACK_DEFAULT) {
		++inuse;
		int	i	= get_pool(size);
		size	= get_size(i);
		if (block *s = pool[i].alloc())
			return {s, size};
		return {malloc(size), size};
	}
	void			release() {
		--inuse;
		int	i = get_pool(size);
		pool[i].release((block*)p);
	}

	stack(void *p, size_t size) : p(p), size(size) {}
	template<typename T> void*	get_space() const {
		return align_down((char*)p + size - sizeof(T), 256);
	}
};

struct fiber {
//	static thread_local fiber 	primary;

	struct _primary {
		static thread_local fcontext_t ctx;

		void*	resume(void *data = nullptr) {
			transfer_t t = jump_fcontext(ctx, data);
			ctx = t.ctx;
			return t.data;
		}
//		void*	resume(RESUME ret) {
//			return resume((void*)ret);
//		}
		template<typename L> void resume_with(L &&lambda) {
			ISO_CHEAPASSERT((uintptr_t)ctx > 0x1000);
			auto t = ontop_fcontext(
				ctx,
				&lambda,
				[](transfer_t t) {
					forward<L>(*(noref_t<L>*)t.data)(fiber(t.ctx));
					return transfer_t{nullptr, nullptr};
				}
			);
			ctx = t.ctx;
		}
	};

	static _primary primary;


	template<typename L> static exists_t<decltype(declval<L>()())> _call(L &lambda, fcontext_t ctx) {
		primary.ctx = ctx;
		return lambda();
	}
	template<typename L> static exists_t<decltype(declval<L>()(declval<fcontext_t>()))> _call(L &lambda, fcontext_t ctx) {
		return lambda(ctx);
	}

	template<typename L> class record {
		stack		stk;
		L			lambda;
		static void	destroy(record *me) {
			me->~record();
			me->stk.release();
		}
	public:
		record(stack stk, L &&lambda) : stk(stk), lambda(forward<L>(lambda)) {}
		void run(fcontext_t ctx) {
			_call(lambda, ctx);
			//ISO_CHEAPASSERT((uintptr_t)primary.ctx > 0x1000);
			ontop_fcontext(primary.ctx, this, [](transfer_t t) {
				destroy((record*)t.data);
				return transfer_t{ nullptr, nullptr };
			});
		}
	};

	template<typename L> static fcontext_t create(L &&lambda, stack stk) {
		typedef record<L>	R;
		void	*s	= stk.get_space<R>();
		fcontext_t ctx = make_fcontext(s, stk.p, [](transfer_t t) {
		   R	*r 	= static_cast<R*>(t.data);
		   t 		= jump_fcontext(t.ctx, nullptr);
		   r->run(t.ctx);
		});
		return jump_fcontext(ctx, new(s) R(stk, forward<L>(lambda))).ctx;
	}
	template<typename L> static transfer_t run(L &&lambda, stack stk) {
		typedef record<L>	R;
		void	*s	= stk.get_space<R>();
		fcontext_t ctx = make_fcontext(s, stk.p, [](transfer_t t) {
			((R*)t.data)->run(t.ctx);
		});
		return jump_fcontext(ctx, new(s) R(stk, forward<L>(lambda)));
	}
	template<typename L> static transfer_t run(L &&lambda, size_t size = THREAD_STACK_DEFAULT) {
		return run(forward<L>(lambda), stack::acquire(size));
	}

	fcontext_t		ctx;

	fiber() 						: ctx(nullptr) {}
	explicit fiber(fcontext_t ctx) 	: ctx(ctx) {}
	fiber(fiber &&f) 				: ctx(exchange(f.ctx, nullptr)) {}
	fiber(fiber &f) 	= delete;
	template<typename L> fiber(L &&lambda, stack stk) : ctx(create(forward<L>(lambda), stk)) {}
	template<typename L> fiber(L &&lambda, size_t size = THREAD_STACK_DEFAULT) : ctx(create(forward<L>(lambda), stack::acquire(size))) {}

	fiber& operator=(fiber &&f) { swap(ctx, f.ctx); return *this; }
	operator bool() const { return !!ctx; }

	void*	resume(void *data = nullptr) {
		ISO_CHEAPASSERT((uintptr_t)ctx > 0x1000);
		transfer_t t = jump_fcontext(ctx, data);
		ctx = t.ctx;
		return t.data;
	}
	//void*	resume(RESUME ret) {
	//	return resume((void*)ret);
	//}
	template<typename L> void resume_with(L &&lambda) {
		ISO_CHEAPASSERT((uintptr_t)ctx > 0x1000);
		auto t = ontop_fcontext(
			ctx,
			&lambda,
			[](transfer_t t) {
				forward<L>(*(noref_t<L>*)t.data)(fiber(t.ctx));
				return transfer_t{nullptr, nullptr};
			}
		);
		ctx = t.ctx;
    }

	static void *yield() {
		return primary.resume();
	}
	template<typename T> static void *yield(const T &t) {
		return primary.resume((void*)&t);
	}
};

//-----------------------------------------------------------------------------
// fiber_generator
//-----------------------------------------------------------------------------

template<typename T> struct fiber_generator {
	typedef const T element, &reference;
	enum RESUME {
		RESUME_END		= 0,
		RESUME_CONT		= 1,
	};


	struct iterator {
		typedef const T element, &reference;
		fiber	f;
		void	*result;

		iterator() {}
		iterator(fiber &&_f)	: f(move(_f)) 	{ result = f.resume(); }
		iterator(iterator &&i)	: f(move(i.f)) 	{}
		~iterator() {
			if (f)
				f.resume((void*)RESUME_END);
		}

		iterator &operator++() {
			result = f.resume((void*)RESUME_CONT);
			return *this;
		}
		constexpr T&	operator*()						const { return *(T*)result; }
		constexpr T*	operator->()					const { return (T*)result; }
		constexpr bool	operator==(const iterator &b)	const { return f == b.f; }
		constexpr bool	operator!=(const iterator &b)	const { return f != b.f; }
	};
	typedef iterator const_iterator;

	fiber	f;

public:
	iterator	begin()		{ return move(f); }
	iterator	end() const { return {}; }
	template<typename L> fiber_generator(L &&lambda, size_t size = THREAD_STACK_DEFAULT) : f(forward<L>(lambda), stack::acquire(size)) {
	}
	~fiber_generator() {
		if (f)
			f.resume((void*)RESUME_END);
	}
};

//-----------------------------------------------------------------------------
// FiberJobs
//-----------------------------------------------------------------------------

class FiberJobs : lf_block_fifo_list<1024> {
	typedef lf_block_fifo_list<1024> B;

	static FiberJobs						*me;
	static lf_array_queue_list<fiber, 256>	ready_fibers;
//	static atomic<int>						ready_queued, ready_dequeued;
	static Benaphore						ben;

	typedef void fiber_func(void*);
	struct fiber_job {
		fiber_func	*f;
		fiber_job(fiber_func *f) : f(f) {}
	};

	template<typename L> struct lambda_job : fiber_job {
		L		x;
		static void thunk(void *gp) {
			L		y(move(((lambda_job*)getter(move(*(getter*)gp)))->x));
			y();
		}
		lambda_job(L &&x)	: fiber_job(thunk), x(move(x)) {}
	};

	template<typename L> void put(L &&t) {
		new(placement(B::put(sizeof(L), alignof(L)))) L(move(t));
		ben.unlock();
	}

public:
	FiberJobs(uint64 processors) {
		me	= this;
		ben.lock();
		for (processors &= bits64(Thread::processors()); processors; processors = clear_lowest(processors))
			thread::settings("fiber_jobs").affinity(lowest_set(processors)).create([this]() {
				ben.lock();
				for (;;) {
					// new jobs
					getter	g;
					if (lf_block_fifo_list<1024>::get(g)) {
						fiber_job	*j = g;
						fiber::run(callback<void()>(&g, j->f));
						ben.lock();
					}

					ISO_CHEAPASSERT(!g);
					// existing fibers
					fiber	f;
					if (ready_fibers.get(f)) {
						f.resume();
						ben.lock();
					}
				}
			});
	}
	~FiberJobs()												{ ISO_OUTPUT("~FiberJobs\n"); }
	template<typename L> void add(L &&t)						{ put(lambda_job<L>(forward<L>(t))); }
	template<typename... P> void add(void(*f)(P... p), P&&...p)	{ add([f, p...]() { f(forward<P>(p)...); }); }
	template<typename P, void(*F)(P*)> void add(P *p)			{ add([p]() { F(p); }); }

	static void ready(fiber &&f) {
		ISO_CHEAPASSERT((uintptr_t)f.ctx > 0x1000);
		ready_fibers.put(move(f));
		ben.unlock();
	}

	static FiberJobs	*get() { return me; }
};

//-----------------------------------------------------------------------------
//	fiber_event
//-----------------------------------------------------------------------------

class fiber_wait {
	atomic<fcontext_t>	held;
public:
	fiber_wait() {}
	~fiber_wait() { ISO_CHEAPASSERT(held == (fcontext_t)1); }

	bool	ready() {
		return held.get();
	}
	void	wait() {
		if (held.get())
			return;

		fiber::primary.resume_with([this](fiber &&f) {
			if (fcontext_t p = exchange(held, f.ctx)) {
				ISO_CHEAPASSERT(p == (fcontext_t)1);
				held = p;
				FiberJobs::ready(move(f));
			}
		});
	}
	void	signal() {
		if (fcontext_t ctx = exchange(held, fcontext_t(1)))
			FiberJobs::ready(fiber(ctx));
	}
	void	signal_end() {
		if (fcontext_t ctx = exchange(held, (fcontext_t)1)) {
			FiberJobs::ready(fiber(ctx));
//			fiber(ctx).resume();
		}
	}
};

struct fiber_queue : lf_array_queue<fiber,256> {
	bool	released;

	fiber_queue() : released(false) {}

	void	hold() {
		if (released)
			return;

		fiber::primary.resume_with([this](fiber &&f) {
			push_back(move(f));
			if (released)
				release_all();
		});
	}
	void	release() {
		fiber f;
		if (pop_front(f))
			FiberJobs::ready(move(f));
	}
	void	release_all() {
		released = true;
		fiber f;
		while (pop_front(f))
			FiberJobs::ready(move(f));
	}
};

//-----------------------------------------------------------------------------
//	fiber_semaphore
//-----------------------------------------------------------------------------

class fiber_semaphore : fiber_queue {
	atomic<int>	count;
public:
	fiber_semaphore(int count = 0) : count(count)	{}
	void	lock()		{ if (--count < 0) hold(); }
	void	unlock()	{ if (++count <= 0) release(); }
};

//-----------------------------------------------------------------------------
//	fiber_count
//-----------------------------------------------------------------------------

class fiber_count : fiber_queue {
	atomic<int>	count;
public:
	fiber_count(int count) : count(count)	{}
	void	wait()		{ if (count) hold(); }
	bool	try_wait()	{ return count == 0; }
	void	signal()	{ if (--count == 0) release_all(); }
};

//-----------------------------------------------------------------------------
//	fork
//-----------------------------------------------------------------------------

template<typename F1, typename F2> void fork(FiberJobs &q, F1 &&f1, F2 &&f2) {
	fiber_wait	fw;
	q.add([&]() {
		f1();
		fw.signal();
	});
	f2();
	fw.wait();
}

template<typename T> class fiber_future : fiber_wait {
	T		result;
public:
	using fiber_wait::ready;

	template<typename...P> fiber_future(T(*fn)(fcontext_t, P...), P&&... p) {
		fiber::run([this, fn, p...](fcontext_t ctx) {
			result = fn(ctx, p...);
			signal_end();
		});
	}
	T	wait() {
		fiber_wait::wait();
		return result;
	}
	operator T() {
		return wait();
	}
};

template<> class fiber_future<void> : fiber_wait {
public:
	using fiber_wait::ready;
	using fiber_wait::wait;

	template<typename...P> fiber_future(void(*f)(P...), P&&... p) {
		run([this, f, p...]() {
			f(p...);
			signal_end();
		});
	}
};

//-----------------------------------------------------------------------------
//	parallel_for
//-----------------------------------------------------------------------------

template<class I, class F> void parallel_for(FiberJobs &q, I i, I end, F f) {
	fiber_count	cd(end - i);

	while (i != end) {
		q.add([i, &f, &cd]() {
			f(*i);
			cd.signal();
		});
		++i;
	}

	cd.wait();
}

template<class I, class F> void parallel_for_block(FiberJobs &q, I i, I end, F f, int nt) {
	auto	n	= end - i;
	if (nt > n)
		nt = int(n);

	fiber_count	cd(nt);

	auto	d	= n / nt;
	int		m	= int(n % nt);
	int		e	= -nt / 2;

	for (I a = i, b = i + d; a != end; a = b, b += d) {
		if ((e += m) >= 0) {
			++b;
			e -= nt;
		}
		q.add([a, b, &f, &cd]() {
			for (I i = a; i != b; ++i)
				f(*i);
			cd.signal();
		});
	}

	cd.wait();
}

template<class C, class F> void parallel_for(FiberJobs &q, C &&c, F f) {
	parallel_for(q, begin(c), end(c), f);
}

template<class C, class F> void parallel_for_block(FiberJobs &q, C &&c, F f, int nt = 64) {
	parallel_for_block(q, begin(c), end(c), f, nt);
}

//-----------------------------------------------------------------------------
//	fiber_sleep
//-----------------------------------------------------------------------------

void fiber_sleep(float t, fiber parent);

} // namespace iso

#endif // FIBERS_H

