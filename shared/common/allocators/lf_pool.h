#ifndef LF_POOL_H
#define LF_POOL_H

#include "pool.h"
#include "base/atomic.h"

//-----------------------------------------------------------------------------
//	atomic versions
//-----------------------------------------------------------------------------

namespace iso {

template<typename T> class atomic<freelist<T>> {
protected:
	atomic<tagged_pointer<T>>	avail;
public:
	atomic() : avail(nullptr) {}
	atomic(atomic &&b) : avail(exchange(b.avail, nullptr)) {}

	bool	empty() const {
		return !avail;
	}
	tagged_pointer<T> reset(T *t = nullptr) {
		tagged_pointer<T>	a = avail;
		while (!avail.cas_exch(a, {t, a.b + 1}));
		return a;
	}
	void release(T *t, int n) {
		T	*t1 = t;
		while (--n) {
			*(T**)t1 = t1 + 1;
			++t1;
		}
		tagged_pointer<T>	a = avail;
		do
			*(T**)t1 = a;
		while (!avail.cas_exch(a, {t, a.b + n}));
	}
	void	release(T *t) {
		tagged_pointer<T>	a = avail;
		do {
			*(T**)t = a;
		} while (!avail.cas_exch(a, {t, a.b + 1}));
	}
	T* alloc() {
		tagged_pointer<T>	a = avail;
		while (a) {
			T	*n = *(T**)(T*)a;
			if (avail.cas_exch(a, {n, a.b + 1}))
				break;
		}
		return a;
	}
};


template<typename T, int N> class atomic<fixed_pool<T, N> > {
	typedef spacer<sizeof(T) / alignof(T), alignof(T)> S;
	atomic<uint32>	avail;
	S				array[N];
public:
	typedef T	element;
	atomic() {
		for (int i = 0; i < N - 1; i++)
			*(uint32*)(array + i) = i + 2;
		*(uint32*)(array + N - 1) = 0;
		avail	= 1;
	}

	int		index_of(const T *t) const	{ return (S*)t - array + 1; }
	T*		by_index(int i)				{ return (T*)(array + i - 1); }

	T*	alloc() {
		for (uint32 prev = avail; prev;) {
			T		*t		= by_index(prev);
			uint32	next	= *(uint32*)t;
			if (~next && avail.cas_exch(prev, next))
				return t;
		}
		return 0;
	}
	void	release(T *t) {
		uint32	next = index_of(t);
		*(uint32*)t	= ~0;
		*(uint32*)t	= exchange(avail, next);
	}
};

template<typename T, int N> class atomic<growing_pool<T,N>> : public atomic<freelist<T>> {
public:
	T*	alloc() {
		T	*t = atomic<freelist<T>>::alloc();
		if (!t) {
			t = allocate<T>(N);
			this->release(t + 1, N - 1);
		}
		return t;
	}
};

}//namespace iso

template<typename T, int N> inline void *operator new(size_t size, iso::atomic<iso::fixed_pool<T, N> > &a)		{ return a.alloc(); }
template<typename T, int N> inline void operator delete(void *p, iso::atomic<iso::fixed_pool<T, N> > &a)		{ a.release((T*)p); }
template<typename T, int N> inline void *operator new(size_t size, iso::atomic<iso::growing_pool<T, N> > &a)	{ return a.alloc(); }
template<typename T, int N> inline void operator delete(void *p, iso::atomic<iso::growing_pool<T, N> > &a)		{ a.release((T*)p); }

#endif //LF_POOL_H
