#ifndef LF_ALLOCATOR_H
#define LF_ALLOCATOR_H

#include "allocator.h"
#include "base/atomic.h"

namespace iso {

template<> class atomic<linear_allocator> : public allocator_mixin<atomic<linear_allocator> > {
protected:
	atomic<void*>	p;
public:
	atomic(void *_p) : p(_p)	{}
	void	*_alloc(size_t size, size_t align = 4) {
		void	*p0;
		char	*t;
		do {
			p0	= p;
			t	= (char*)((intptr_t(p0) + align - 1) & -intptr_t(align));
		} while (!p.cas(p0, t + size));
		return t;
	}
	void	*getp() const { return p; }
};

template<> class atomic<checking_linear_allocator> : public allocator_mixin<atomic<checking_linear_allocator> > {
protected:
	atomic<void*>	p;
	const void		*end;
public:
	atomic(const memory_block &m) : p(m), end(m.end()) {}
	void	*_alloc(size_t size, size_t align = 4) {
		void	*p0;
		char	*t;
		do {
			p0	= p;
			t	= (char*)((intptr_t(p0) + align - 1) & -intptr_t(align));
			if (t + size > end)
				return 0;
		} while (!p.cas(p0, t + size));
		return t;
	}
	void	*getp()		const	{ return p; }
	size_t	remaining()	const	{ return (char*)end - (char*)p.get(); }
};

template<> class atomic<circular_allocator> : public allocator_mixin<atomic<circular_allocator> > {
protected:
	uint8			*a, *b, *g;
	atomic<uint8*>	p;
public:
	atomic() : a(0), b(0), g(0), p(0) {}
	atomic(const memory_block &m)		{ init(m); }
	void	init(const memory_block &m) { a = m; b = m.end(); p = g = a; }

	void	*_alloc(size_t size, size_t align = 4) {
		uint8 *t, *p0;
		do {
			p0	= p;
			t	= (uint8*)((intptr_t(p0) + align - 1) & -intptr_t(align));
			if (p0 < g || t + size >= b) {
				if (p0 >= g)
					t = (uint8*)((intptr_t(a) + align - 1) & -intptr_t(align));
				if (t + size >= g)
					return 0;
			}

		} while (!p.cas(p0, t + size));
		return t;
	}

	memory_block alloc_upto(size_t size, size_t align = 4) {
		uint8	*t, *p0;
		size_t	avail;
		do {
			p0		= p;
			t		= (uint8*)((intptr_t(p0) + align - 1) & -intptr_t(align));
			if (t == b)
				t = (uint8*)((intptr_t(a) + align - 1) & -intptr_t(align));
			avail	= min((t < g ? g : b) - t, size);
		} while (!p.cas(p0, t + avail));
		return memory_block(t, avail);
	}

	void	*getp()					const	{ return p; }
	void	set_get(void *_g)				{ g = (uint8*)_g; }
	uint32	to_offset(const void *p)const	{ return uint32((uint8*)p - (uint8*)a); }
	void	*to_pointer(uint32 x)	const	{ return (uint8*)a + x; }
	uint32	get_offset()			const	{ return to_offset(p); }
	void	set_get_offset(uint32 _g)		{ g = (uint8*)a + _g; }

	ptrdiff_t	relocate(void *x) {
		ptrdiff_t	d = (uint8*)x - a;
		a	= (uint8*)x;
		b	= b + d;
		p	= p.get() + d;
		g	= g + d;
		return d;
	}
	size_t	remaining()			const	{ return p < g ? g - p : g - p + b - a; }
	size_t	remaining_linear()	const	{ return (p < g ? g : b) - p; }
};

//-----------------------------------------------------------------------------
//	class circular_allocator2
//	allows free
//-----------------------------------------------------------------------------

class circular_allocator2 : public allocator_mixin<circular_allocator2> {
protected:
	typedef uint32	U;
	static const U	USED = 1u << 31;

	void			*a, *b;
	atomic<void*>	p;
	void			*g;
public:
	circular_allocator2() {}
	circular_allocator2(const memory_block &m) : a(m), b(m.end()), p(a), g(a) {}

	void	*_alloc_slow(size_t size) {
		U		nu	= U((size + sizeof(U) * 2 - 1) / sizeof(U));
		U		*t = (U*)g, *u = 0;

		for (;;) {
			while (t + nu < b && (*t & USED))
				t += *t & ~USED;

			if (t + nu >= b) {
				t = (U*)a;
				continue;
			}

			u = t + *t;
			while (u - t < nu && !(*u & USED))
				u += *u;
			if (u - t >= nu)
				break;
			t = u;
		}
		*t	= nu | USED;
		if (U nf = U(u - t - nu))
			t[nu] = nf;
		return t + 1;
	}

	void	*_alloc(size_t size) {
		U		nu	= U((size + sizeof(U) * 2 - 1) / sizeof(U));
		U		*t;
		void	*p0;
		do {
			while (!(p0 = p));
			t	= (U*)p0;
			if (t < g || t + nu >= b) {
				if (t >= g)
					t = (U*)a;
				if (t + nu >= g) {
					if (!p.cas(p0, 0))
						continue;
					void *r = _alloc_slow(size);
					p = p0;
					return r;
				}
			}

		} while (!p.cas(p0, 0));
		*t	= nu | USED;
		if (t != p0)
			*(U*)p0 = U((U*)b - (U*)p0);
		p	= t + nu;
		return t + 1;
	}

	void	*_alloc(size_t size, size_t align) {
		return _alloc(size);
	}
	bool	_free(void *m) {
		U	*t = (U*)m - 1;
		if (t != g) {
			*t &= ~USED;

			t	= (U*)g;
			if (*t & USED)
				return true;
		}
		t += *t & ~USED;
		if (t == b)
			t = (U*)a;

		void	*p0;
		while (!(p0 = p));

		while (t != p0 && !(*t & USED)) {
			t += *t;
			if (t == b)
				t = (U*)a;
		}
		g	= t;
		return true;
	}
	bool	_free(void *m, size_t size) {
		return _free(m);
	}
};

//-----------------------------------------------------------------------------
//	atomic<block_fifo>
//	allocate blocks from a ringbuffer
//-----------------------------------------------------------------------------

class atomic_block_fifo_base {
	class temp {
	protected:
		atomic<uint32>	*pos;
		uint32	t, n, *p;
	public:
		force_inline constexpr temp() : pos(0), t(0), n(0), p(0) {}
		force_inline constexpr temp(atomic<uint32> *pos, uint32 t, uint32 n, uint32 *p) : pos(pos), t(t), n(n), p(p) {}
		force_inline constexpr size_t size()						const { return ((n & 0x00ffffff) - (n >> 24)) * sizeof(uint32); }
		explicit force_inline constexpr operator bool()				const { return n != 0; }
		template<typename T> force_inline constexpr operator T*()	const { return (T*)p; }
	};
	class updater : public temp {
	public:
		force_inline constexpr updater() {}
		force_inline constexpr updater(atomic<uint32> &pos, uint32 t, uint32 n, uint32 *p) : temp(&pos, t, n, p) {}
		force_inline updater(updater &&b) : temp(b) { b.n = 0; }
		~updater() { if (n) no_lock_update(*pos, t, t + (n & 0x00ffffff)); }
		updater& operator=(updater &&b) { swap((temp&)*this, (temp&)b); return *this; }
	};

protected:
	atomic<uint32>	head, head2, tail, tail2, tail3;

public:
	typedef updater	putter, getter;

	atomic_block_fifo_base() : head(0), head2(0), tail(0), tail2(0), tail3(0) {}
	void		clear()			{ tail = tail2 = head = head2 = 0; }
	bool		empty()	const	{ return tail == head; }

	bool		stop_queuing() {
		uint32	t = exchange(tail, ~0u);
		if (t != ~0u)
			tail3 = t;
		return tail3 == head;
	}
	void		start_queuing() {
		ISO_CHEAPASSERT(tail == ~0u && tail2 == tail3 && head == tail3);
		tail3 = 0;
		tail = tail2;
	}

};

template<int N> class atomic<block_fifo<N>> : public atomic_block_fifo_base {
	alignas(16) uint32	buffer[N];

public:
	atomic() {}

	bool	pop_front(getter &g) {
		uint32	t = head, n;
		do {
			if (t == tail2)
				return false;

			uint32	t1 = t % N;
			n = buffer[t1];
		} while (!head.cas_exch(t, t + (n & 0x00ffffff)));

		g = getter(head2, t, n, buffer + (t + (n >> 24)) % N);
		return true;
	}

	putter	push_back(uint32 size, uint32 align) {
		size	/= sizeof(uint32);
		align	/= sizeof(uint32);

		uint32	t = tail, t1, n, skip;
		do {
			t1 = t % N;
			skip = t1 + size > N
				? N - t1							// extra for wrapping
				: 1 + (-(t1 + 1) & (align - 1));	// extra for alignment (inc. size field)

			n = size + skip;
			if (t == ~0 || t + n >= head2 + N)
				return putter(tail2, t, 0, 0);	// too full (or stopped)

		} while (!tail.cas_exch(t, t + n));

		n |= skip << 24;
		buffer[t1] = n;
		return putter(tail2, t, n, buffer + (t + skip) % N);
	}

	template<typename T> auto push_back(T &&t) {
		typedef noref_t<T>	T2;
		if (putter p = push_back(sizeof(T2), alignof(T2)))
			return new(p) T2(forward<T>(t));
		return nullptr;
	}
};

} // namespace iso

#endif // LF_ALLOCATOR_H
