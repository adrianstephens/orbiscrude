#ifndef COROUTINE_H
#define COROUTINE_H

#include "base/defs.h"
#include "thread.h"

//#include <experimental/coroutine>

#ifdef PLAT_CLANG

#define _coro_resume	__builtin_coro_resume
#define _coro_destroy	__builtin_coro_destroy
#define _coro_done		__builtin_coro_done

#else
// intrinsics used in implementation of coroutine_handle
extern "C" size_t	_coro_resume(void *);
extern "C" void		_coro_destroy(void *);
extern "C" size_t	_coro_done(void *);
#pragma intrinsic(_coro_resume)
#pragma intrinsic(_coro_destroy)
#pragma intrinsic(_coro_done)

#endif

namespace std { namespace experimental {
	using namespace iso;

#ifdef PLAT_CLANG
	struct coroutine_frame {
		void		(*resume)(coroutine_frame*);
		void		(*destroy)(coroutine_frame*);
	};
#else
	struct __declspec(align(sizeof(void*) * 2)) coroutine_frame {
		typedef		void(__cdecl *F)(void*);
		F			_Fn;
		uint16		_Index;
		uint16		_Flags;
		coroutine_frame(void *_f, int _flags) : _Fn((F)_f), _Index(2), _Flags(_flags) {}
	};
#endif

	template<typename P = void> struct coroutine_handle;

	template<> struct coroutine_handle<void> {
		coroutine_frame *_Ptr;

		static coroutine_handle	from_address(void *f)	noexcept	{ return (coroutine_frame*)f; }

		coroutine_handle()			: _Ptr(nullptr) {}
		coroutine_handle(nullptr_t)	: _Ptr(nullptr) {}
		coroutine_handle(coroutine_frame *p)	: _Ptr(p) {}

		void*	address()			const	{ return _Ptr; }
		void	operator()()		const	{ resume(); }
		explicit operator bool()	const	{ return !!_Ptr; }
		void	resume()			const	{ _coro_resume(_Ptr); }
#ifdef PLAT_CLANG
		bool	done()				const	{ return _coro_done(_Ptr); }//return _Ptr->_Index == 0; }
#else
		bool	done()				const	{ return _Ptr->_Index == 0; }
#endif
		void	destroy()					{ _coro_destroy(_Ptr); }
		friend bool operator==(coroutine_handle<> a, coroutine_handle<> b) { return a._Ptr == b._Ptr; }
		friend bool operator!=(coroutine_handle<> a, coroutine_handle<> b) { return a._Ptr != b._Ptr; }
	};

	template<typename P> struct coroutine_handle : coroutine_handle<> {
#ifdef PLAT_CLANG
		struct P2 : coroutine_frame, P {};
//		static P *promise(coroutine_frame *p)	{ return (P*)__builtin_coro_promise(p, alignof(P), false); }
//		static coroutine_frame *frame(P *p)		{ return (coroutine_frame*)__builtin_coro_promise(p, alignof(P), true); }
#else
		struct P2 : P, coroutine_frame {
			P2(void *f, int flags) : P(), coroutine_frame(f, flags) {}
		};
#endif
		static P *promise(coroutine_frame *p)	{ return static_cast<P2*>(p); }
		static coroutine_frame *frame(P *p)		{ return static_cast<P2*>(p); }

		coroutine_handle(P *promise)			: coroutine_handle<>(frame(promise)) {}
		coroutine_handle(nullptr_t)				: coroutine_handle<>(nullptr) {}
		coroutine_handle(coroutine_frame *p)	: coroutine_handle<>(p) {}

		coroutine_handle &operator=(nullptr_t)  { _Ptr = nullptr; return *this; }
		P *promise()		const				{ return promise(_Ptr); }
	};

	// trivial awaiters:

	struct suspend_if {
		bool ready;
		suspend_if(bool suspend) : ready(!suspend) {}
		bool await_ready()	noexcept	{ return ready; }
		void await_resume()	noexcept	{}
		void await_suspend(std::experimental::coroutine_handle<>) noexcept	{}
	};

	struct suspend_always {
		bool await_ready()	noexcept	{ return false; }
		void await_resume()	noexcept	{}
		void await_suspend(std::experimental::coroutine_handle<>) noexcept	{}
	};

	struct suspend_never {
		bool await_ready()	noexcept	{ return true; }
		void await_resume()	noexcept	{}
		void await_suspend(std::experimental::coroutine_handle<>) noexcept	{}
	};

	template<typename R, typename... TT> struct coroutine_traits {
		typedef typename R::promise_type	promise_type;
	};

	template<typename... TT> struct coroutine_traits<void, TT...> {
		struct promise_type {
			promise_type	get_return_object()		{ return *this; }
			auto			initial_suspend()		{ return suspend_never(); }
			auto			final_suspend()			{ return suspend_never(); }
			void			unhandled_exception()	{}
			void			return_void()			{}

			void* operator new(size_t sz, void *p)		{ return p; }
			void* operator new(size_t sz)				{ return iso::malloc(sz); }
			void operator delete(void* p, size_t sz)	{ iso::free(p); }
		};
	};

	template<typename R, typename... TT> struct _Resumable_helper_traits {
		using traits	= coroutine_traits<R, TT...>;
		using P			= typename traits::promise_type;
		using H			= coroutine_handle<P>;

		static P *_Promise_from_frame(void *addr) noexcept {
			return H::promise(reinterpret_cast<coroutine_frame*>(addr));
		}
		static H _Handle_from_frame(void *addr) noexcept {
			return reinterpret_cast<coroutine_frame*>(addr);
		}
//		static void _Set_exception(void *addr) {
//			_Promise_from_frame(addr)->set_exception(_STD current_exception());
//		}
		static void _ConstructPromise(void *addr, void *resume_addr, int heap_elision) {
			new(_Promise_from_frame(addr)) H::P2(resume_addr, !heap_elision);
		}
		static void _DestructPromise(void *addr) {
			_Promise_from_frame(addr)->~P();
		}
	};

}}

namespace iso {

// trivial awaiters:

using std::experimental::suspend_if;
using std::experimental::suspend_always;
using std::experimental::suspend_never;

// future:

template<typename T> struct future {
	struct promise_type {
		T		t;
		std::experimental::coroutine_handle<>	awaiter;

		promise_type()					{}
		future	get_return_object()		{ return this; }
		auto	initial_suspend()		{ return suspend_never(); }
		auto	final_suspend()			{ return suspend_always(); }
		void	unhandled_exception()	{}
		template<typename T2> void return_value(T2 &&t2) {
			t = forward<T2>(t2);
			if (awaiter)
				awaiter.resume();
		}
	};

	std::experimental::coroutine_handle<promise_type>	h;

public:
	future(promise_type *p) : h(p) {}
	~future()				{ if (h) h.destroy(); }

	bool	await_ready()	{ return h.done(); }
	T		await_resume()	{ return h.promise()->t; }
	void	await_suspend(std::experimental::coroutine_handle<> w) {
		h.promise()->awaiter = w;
	}
	T		get() {
		while (!await_ready())
			Thread::sleep();
		return h.promise()->t;
	}
};

template<> struct future<void> {
	struct promise_type {
		std::experimental::coroutine_handle<>	awaiter;

		promise_type()					{}
		future	get_return_object()		{ return this; }
		auto	initial_suspend()		{ return suspend_never(); }
		auto	final_suspend()			{ return suspend_never(); }
		void	unhandled_exception()	{}
		void	return_void()			{
			if (awaiter)
				awaiter.resume();
		}
	};

	std::experimental::coroutine_handle<promise_type>	h;

public:
	future(promise_type *p) : h(p) {}
//	~future()				{ if (h) h.destroy(); }
	bool	await_ready()	{ return h.done(); }
	void	await_resume()	{}
	void	await_suspend(std::experimental::coroutine_handle<> w) {
		h.promise()->awaiter = w;
	}
};

template<typename T> struct generator {
	struct promise_type {
		T const *current;
		generator get_return_object()	noexcept { return this; }
		auto	initial_suspend()		noexcept { return suspend_always(); }
		auto	final_suspend()			noexcept { return suspend_always(); }
		auto	yield_value(const T &v)	noexcept { current = &v; return suspend_always(); }
		void	unhandled_exception()	noexcept {}
		void	return_void()	const	noexcept {}
		//template<typename T2> T2 &&await_transform(T2 &&_Whatever) { return forward<T2>(t2); }
	};

	typedef std::experimental::coroutine_handle<promise_type>	H;
	typedef const T element, &reference;

	struct iterator {
		typedef const T element, &reference;
		H	h;

		iterator(nullptr_t) : h(nullptr) {}
		iterator(H _h)		: h(_h) {}

		iterator &operator++() {
			h.resume();
			if (h.done())
				h = nullptr;
			return *this;
		}
		const T&	operator*()		const { return *h.promise()->current; }
		const T*	operator->()	const { return h.promise()->current; }

		bool operator==(iterator const &b) const { return h == b.h; }
		bool operator!=(iterator const &b) const { return h != b.h; }
	};
	typedef iterator const_iterator;

public:
	H h;

	iterator begin() const {
		if (h) {
			h.resume();
			if (h.done())
				return nullptr;
		}

		return h;
	}

	iterator end() const {
		return nullptr;
	}

	element	first_or_default(element &&v = element()) const {
		auto i = begin();
		return i != end() ? *i : move(v);
	}

	generator(promise_type *p)	: h(p)		{}
	generator(generator &&b)	: h(b.h)	{ b.h = nullptr; }
	~generator()							{ if (h) h.destroy(); }
	generator &operator=(generator &&b)		{ swap(h, b.h); return *this; }
};

// switches to a background thread

struct background {
	bool await_ready()	const { return false; }
	void await_resume() const {}

	void await_suspend(std::experimental::coroutine_handle<> handle) const {
		RunThread([=]() {
			handle.resume();
		});
	}
};

// waits

struct delay {
	float	s;
	bool await_ready()	const { return false; }
	void await_resume() const {}

	void await_suspend(std::experimental::coroutine_handle<> handle) const {
		RunThread([handle, s = s]() {
			Thread::sleep(s);
			handle.resume();
		});
	}
	delay(float _s) : s(_s) {}
};

}// namespace iso

#endif // COROUTINE_H
