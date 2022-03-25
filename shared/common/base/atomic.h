#ifndef ATOMIC_H
#define ATOMIC_H

#include "defs.h"
#include "_atomic.h"

namespace iso {

template<typename T, typename A = typename _atomic_type<sizeof(T)>::type, bool PAD = (sizeof(T) != sizeof(A))> struct atomic_type {
	typedef A type;
	static const A get(const T &t) { A a = A(); (T&)a = t; return a; }
};

template<typename T, typename A> struct atomic_type<T, A, false> {
	typedef A type;
	static const A get(const T &t) { return reinterpret_cast<const A&>(t); }
};

// fallback implementations if not defined by platform

#ifndef HAS_CAS_EXCH
template<typename T> inline bool _cas_exch(T *p, T &a, T b)	{
	T		t	= _cas_val(p, a, b);
	if (t == a)
		return true;
	a = t;
	return false;
}
#endif

template<typename T> inline T _atomic_exch(T *p, T a) {
	T	x;
	do
		x = *p;
	while (!_cas(p, x, a));
	return x;
}

// add/sub/inc/dec

template<typename T> inline T _atomic_add(T *p, T a) {
	T	x, y;
	do
		y = (x = *p) + a;
	while (!_cas(p, x, y));
	return x;
}

// and/or/xor

template<typename T> inline T _atomic_and(T *p, T a) {
	T	x, y;
	do
		y = (x = *p) & a;
	while (!_cas(p, x, y));
	return x;
}
template<typename T> inline T _atomic_or(T *p, T a) {
	T	x, y;
	do
		y = (x = *p) | a;
	while (!_cas(p, x, y));
	return x;
}
template<typename T> inline T _atomic_xor(T *p, T a) {
	T	x, y;
	do
		y = (x = *p) ^ a;
	while (!_cas(p, x, y));
	return x;
}

// mul/div

template<typename T> inline T _atomic_mul(T *p, T a) {
	T	x, y;
	do
		y = (x = *p) * a;
	while (!_cas(p, x, y));
	return x;
}
template<typename T> inline T _atomic_div(T *p, T a) {
	T	x, y;
	do
		y = (x = *p) / a;
	while (!_cas(p, x, y));
	return x;
}

// bit test

template<typename T> inline bool	_atomic_test_set_bit(T *p, int bit) {
	return _atomic_or(p, T(1) << bit) & (T(1) << bit);
}
template<typename T> inline bool	_atomic_test_clear_bit(T *p, int bit) {
	return _atomic_and(p, ~(T(1) << bit)) & (T(1) << bit);
}
template<typename T> inline bool	_atomic_test_flip_bit(T *p, int bit) {
	return _atomic_xor(p, T(1) << bit) & (T(1) << bit);
}

// unsigned versions (so we use platform versions where possible)

template<typename T> inline T _atomic_inc(T *p);
template<typename T> inline T _atomic_dec(T *p);
template<typename T> inline T _atomic_sub(T *p, T a);

inline uint16	_atomic_inc(uint8  *p)				{ return (uint8) _atomic_inc((int8*)	p); }
inline uint16	_atomic_inc(uint16 *p)				{ return (uint16)_atomic_inc((int16*)	p); }
inline uint32 	_atomic_inc(uint32 *p)				{ return (uint32)_atomic_inc((int32*)	p); }
inline uint64	_atomic_inc(uint64 *p)				{ return (uint64)_atomic_inc((int64*)	p); }

inline uint16	_atomic_dec(uint8  *p)				{ return (uint8) _atomic_dec((int8*)	p); }
inline uint16	_atomic_dec(uint16 *p)				{ return (uint16)_atomic_dec((int16*)	p); }
inline uint32 	_atomic_dec(uint32 *p)				{ return (uint32)_atomic_dec((int32*)	p); }
inline uint64	_atomic_dec(uint64 *p)				{ return (uint64)_atomic_dec((int64*)	p); }

inline uint16	_atomic_add(uint8  *p, uint8 a)		{ return (uint8) _atomic_add((int8*)	p, (int8) a); }
inline uint16	_atomic_add(uint16 *p, uint16 a)	{ return (uint16)_atomic_add((int16*)	p, (int16)a); }
inline uint32 	_atomic_add(uint32 *p, uint32 a)	{ return (uint32)_atomic_add((int32*)	p, (int32)a); }
inline uint64	_atomic_add(uint64 *p, uint64 a)	{ return (uint64)_atomic_add((int64*)	p, (int64)a); }

inline uint16	_atomic_and(uint8  *p, uint8 a)		{ return (uint8) _atomic_and((int8*)	p, (int8) a); }
inline uint16	_atomic_and(uint16 *p, uint16 a)	{ return (uint16)_atomic_and((int16*)	p, (int16)a); }
inline uint32 	_atomic_and(uint32 *p, uint32 a)	{ return (uint32)_atomic_and((int32*)	p, (int32)a); }
inline uint64	_atomic_and(uint64 *p, uint64 a)	{ return (uint64)_atomic_and((int64*)	p, (int64)a); }

inline uint16	_atomic_or (uint8  *p, uint8 a)		{ return (uint8) _atomic_or ((int8*)	p, (int8) a); }
inline uint16	_atomic_or (uint16 *p, uint16 a)	{ return (uint16)_atomic_or ((int16*)	p, (int16)a); }
inline uint32 	_atomic_or (uint32 *p, uint32 a)	{ return (uint32)_atomic_or ((int32*)	p, (int32)a); }
inline uint64	_atomic_or (uint64 *p, uint64 a)	{ return (uint64)_atomic_or ((int64*)	p, (int64)a); }

inline uint16	_atomic_xor(uint8  *p, uint8 a)		{ return (uint8) _atomic_xor((int8*)	p, (int8) a); }
inline uint16	_atomic_xor(uint16 *p, uint16 a)	{ return (uint16)_atomic_xor((int16*)	p, (int16)a); }
inline uint32 	_atomic_xor(uint32 *p, uint32 a)	{ return (uint32)_atomic_xor((int32*)	p, (int32)a); }
inline uint64	_atomic_xor(uint64 *p, uint64 a)	{ return (uint64)_atomic_xor((int64*)	p, (int64)a); }

inline void		_atomic_store(uint8  *p, uint8 a)	{ _atomic_store((int8*)	p, (int8) a); }
inline void		_atomic_store(uint16 *p, uint16 a)	{ _atomic_store((int16*)p, (int16)a); }
inline void 	_atomic_store(uint32 *p, uint32 a)	{ _atomic_store((int32*)p, (int32)a); }
inline void		_atomic_store(uint64 *p, uint64 a)	{ _atomic_store((int64*)p, (int64)a); }

template<typename T> inline T _atomic_inc(T *p)			{ return _atomic_add(p, T(1)); }
template<typename T> inline T _atomic_dec(T *p)			{ return _atomic_add(p, T(-1)); }
template<typename T> inline T _atomic_sub(T *p, T a)	{ return _atomic_add(p, T(-a)); }

// pointer arithmetic
template<typename T> inline void _atomic_store(T **p, T *a)			{ _atomic_store((intptr_t*)p, (intptr_t)a); }
template<typename T> inline void _atomic_store(T **p, nullptr_t)	{ _atomic_store((intptr_t*)p, (intptr_t)0); }

template<typename T, typename A> inline T*	_atomic_add(T **p, A a)	{ return (T*)_atomic_add((intptr_t*)p, intptr_t(a * sizeof(T))); }
template<typename T, typename A> inline T*	_atomic_sub(T **p, A a)	{ return (T*)_atomic_sub((intptr_t*)p, intptr_t(a * sizeof(T))); }
template<typename T> inline T*				_atomic_inc(T **p)		{ return (T*)_atomic_add((intptr_t*)p, intptr_t(sizeof(T))); }
template<typename T> inline T*				_atomic_dec(T **p)		{ return (T*)_atomic_sub((intptr_t*)p, intptr_t(sizeof(T))); }

template<typename T, typename A> inline T	_atomic_add(T *p, A a)	{ return _atomic_add(p, T(a)); }
template<typename T, typename A> inline T	_atomic_sub(T *p, A a)	{ return _atomic_sub(p, T(a)); }

inline void _atomic_pause_n(int i) {
	while (i--)
		_atomic_pause();
}

inline void _atomic_pause_t(float t) {
	timer	sw;
	while (sw < t)
		_atomic_pause();
}

struct spinlock {
	typedef _atomic_type<1>::type	A;
	A	flag;
	spinlock() : flag(0) {}
	void	lock()	{
		do {
			while (flag)
				_atomic_pause();
		} while (_atomic_exch(&flag, A(true)));
	}
	void	unlock() {
		flag = 0;
	}
};

template<typename T, typename V = void> class atomic_base {
protected:
	struct _with {
		spinlock	&lock;
		_with(spinlock &_lock) : lock(_lock)	{ lock.lock(); }
		~_with()								{ lock.unlock(); }
	};

	struct post {
		atomic_base	&a;
		post(atomic_base &_a) : a(_a) {}
		template<typename T2> inline T	operator+=(const T2 &t)		{ _with w(a.lock); T v = a.v; a.v += t; return v; }
		template<typename T2> inline T	operator-=(const T2 &t)		{ _with w(a.lock); T v = a.v; a.v -= t; return v; }
		template<typename T2> inline T	operator*=(const T2 &t)		{ _with w(a.lock); T v = a.v; a.v *= t; return v; }
		template<typename T2> inline T	operator/=(const T2 &t)		{ _with w(a.lock); T v = a.v; a.v /= t; return v; }
		template<typename T2> inline T	operator&=(const T2 &t)		{ _with w(a.lock); T v = a.v; a.v &= t; return v; }
		template<typename T2> inline T	operator|=(const T2 &t)		{ _with w(a.lock); T v = a.v; a.v |= t; return v; }
		template<typename T2> inline T	operator^=(const T2 &t)		{ _with w(a.lock); T v = a.v; a.v ^= t; return v; }
	};

	T			v;
	spinlock	lock;

public:
	static constexpr bool is_lock_free()	{ return false; }

	inline atomic_base()					{}
	inline atomic_base(const T &t)	: v(t)	{}
	inline void		set(const T &t)			{ _with w(lock); memcpy((void*)&v, &t, sizeof(T)); }
	inline T		get()		const		{ T t; _with w(lock); memcpy((void*)&t, &v, sizeof(T)); return t; }
	inline T&		relaxed()				{ return v; }
	inline const T&	relaxed() const			{ return v; }

	inline bool		cas(const T &a, const T &b) {
		_with w(lock);
		if (memcmp((const void*)&v, &a, sizeof(T)))
			return false;
		memcpy((void*)&v, &b, sizeof(T));
		return true;
	}
	inline bool		cas_exch(T &a, const T &b) {
		_with w(lock);
		if (memcmp((const void*)&v, &a, sizeof(T))) {
			memcpy(&a, (const void*)&v, sizeof(T));
			return false;
		}
		memcpy((void*)&v, &b, sizeof(T));
		return true;
	}
	inline T		exch(const T &t)		{ _with w(lock); return exchange(v, t); }

	inline T		operator++()			{ _with w(lock); return ++v; }
	inline T		operator++(int)			{ _with w(lock); return v++; }
	inline T		operator--()			{ _with w(lock); return --v; }
	inline T		operator--(int)			{ _with w(lock); return v--; }

	template<typename T2> inline T	operator+=(const T2 &t)		{ _with w(lock); return v += t; }
	template<typename T2> inline T	operator-=(const T2 &t)		{ _with w(lock); return v -= t; }
	template<typename T2> inline T	operator*=(const T2 &t)		{ _with w(lock); return v *= t; }
	template<typename T2> inline T	operator/=(const T2 &t)		{ _with w(lock); return v /= t; }
	template<typename T2> inline T	operator&=(const T2 &t)		{ _with w(lock); return v &= t; }
	template<typename T2> inline T	operator|=(const T2 &t)		{ _with w(lock); return v |= t; }
	template<typename T2> inline T	operator^=(const T2 &t)		{ _with w(lock); return v ^= t; }

	bool		test_set_bit(int bit)			{ _with w(lock); return iso::test_set_bit(v, bit); }
	bool		test_clear_bit(int bit)			{ _with w(lock); return iso::test_clear_bit(v, bit); }
	bool		test_flip_bit(int bit)			{ _with w(lock); return iso::test_flip_bit(v, bit); }

};

template<typename T> class atomic_base<T, void_t<typename _atomic_type<sizeof(T)>::type>> {
protected:
	typedef atomic_type<T>		A;
	typedef typename A::type	AT;

	struct post {
		atomic_base	&a;
		post(atomic_base &_a) : a(_a) {}
		template<typename T2> inline T	operator+=(const T2 &t)		{ return _atomic_add(&a.v, t); }
		template<typename T2> inline T	operator-=(const T2 &t)		{ return _atomic_sub(&a.v, t); }
		template<typename T2> inline T	operator*=(const T2 &t)		{ return _atomic_mul(&a.v, t); }
		template<typename T2> inline T	operator/=(const T2 &t)		{ return _atomic_div(&a.v, t); }
		template<typename T2> inline T	operator&=(const T2 &t)		{ return _atomic_and(&a.v, T(t)); }
		template<typename T2> inline T	operator|=(const T2 &t)		{ return _atomic_or (&a.v, T(t)); }
		template<typename T2> inline T	operator^=(const T2 &t)		{ return _atomic_xor(&a.v, T(t)); }
	};

	union {
		AT		a;
		T		v;
	};

public:
	static constexpr bool is_lock_free()	{ return true; }

	inline atomic_base()					{ a = 0; new((void*)&v) T(); }
	inline atomic_base(const T &t)			{ a = 0; new((void*)&v) T(t); }
	inline T		get() volatile const	{ AT t = _atomic_load_acquire(&a); return reinterpret_cast<T&>(t);	}
	inline void		set(const T &t)			{ _atomic_store_release(&a, A::get(t)); }
	inline T&		relaxed()				{ return v; }
	inline const T&	relaxed() const			{ return v; }

	inline bool		cas(T x, T y)			{ return _cas(&a, A::get(x), A::get(y)); }
	inline bool		cas_acquire(T x, T y)	{ return _cas_acquire(&a, A::get(x), A::get(y)); }
	inline bool		cas_release(T x, T y)	{ return _cas_release(&a, A::get(x), A::get(y)); }
	inline bool		cas_exch(T &x, T y)		{ return _cas_exch(&a, reinterpret_cast<AT&>(x), A::get(y)); }
	inline T		exch(const T &t)		{ AT x = _atomic_exch(&a, A::get(t)); return reinterpret_cast<T&>(x); }

	inline T		operator++()			{ return _atomic_inc(&v) + 1; }
	inline T		operator++(int)			{ return _atomic_inc(&v); }
	inline T		operator--()			{ return _atomic_dec(&v) - 1; }
	inline T		operator--(int)			{ return _atomic_dec(&v); }

	template<typename T2> inline T	operator+=(const T2 &t)		{ return _atomic_add(&v, t) + t; }
	template<typename T2> inline T	operator-=(const T2 &t)		{ return _atomic_sub(&v, t) - t; }
	template<typename T2> inline T	operator*=(const T2 &t)		{ return _atomic_mul(&v, t) * t; }
	template<typename T2> inline T	operator/=(const T2 &t)		{ return _atomic_div(&v, t) / t; }
	template<typename T2> inline T	operator&=(const T2 &t)		{ return _atomic_and(&v, T(t)) & t; }
	template<typename T2> inline T	operator|=(const T2 &t)		{ return _atomic_or (&v, T(t)) | t; }
	template<typename T2> inline T	operator^=(const T2 &t)		{ return _atomic_xor(&v, T(t)) ^ t; }

	bool		test_set_bit(int bit)			{ return _atomic_test_set_bit(&v, bit); }
	bool		test_clear_bit(int bit)			{ return _atomic_test_clear_bit(&v, bit); }
	bool		test_flip_bit(int bit)			{ return _atomic_test_flip_bit(&v, bit); }
};

template<typename T> struct atomic : atomic_base<T> {
private:
	typedef atomic_base<T>	B;
	using B::v;
public:
	constexpr atomic()							{}
	constexpr atomic(const T &t)	: B(t)		{}
//	template<typename...P> atomic(P&&... p)	: B(T(forward<P>(p)...)) {}

//	operator T&()						{ return v;	}
	operator T()			const		{ return B::get(); }
	T		operator->()	const		{ return B::get(); }
	const T	&operator=(const T &t)		{ B::set(t); return t; }
	typename B::post	post()			{ return *this; }
	friend T	get(const atomic &a)	{ return a;	}

	template<typename B> friend T	exchange(atomic &a, const B &b)	{ return a.exch(b); }
	template<typename B> friend T	exchange(atomic &a, B &&b)		{ return a.exch(b); }

	friend bool		test_set_bit(atomic &a, int bit)			{ return a.test_set_bit(bit); }
	friend bool		test_set_bit(atomic &a, int bit, bool set)	{ return set ? a.test_set_bit(bit) : a.test_clear_bit(bit); }
	friend bool		test_clear_bit(atomic &a, int bit)			{ return a.test_clear_bit(bit); }
	friend bool		test_flip_bit(atomic &a, int bit)			{ return a.test_flip_bit(bit); }
};

template<typename T> inline void no_lock_update(atomic<T> &t, T a, T b) {
	while (t != a)
		_atomic_pause();
	t = b;
}

//-----------------------------------------------------------------------------
//	ref pointer helpers
//-----------------------------------------------------------------------------

struct ref_holder {
	atomic<int>	&refs;
	template<typename E> ref_holder(atomic<int> &refs, E& event) : refs(refs) {
		for (;;) {
			int	r = refs;
			if (r < 0)
				event.wait();
			else if (refs.cas(r, r + 1))
				break;
		}
	}
	~ref_holder()	{ --refs; }
};

template<typename T> struct external_refs {
	T		*p;
	uint32	ext_refs;
	external_refs()	{}
	external_refs(T *p) : p(p), ext_refs(0) {
		if (p)
			p->addref(1 << 16);
	}
	void addref() {
		++ext_refs;
	}
	void release() const {
		if (p)
			p->release((1 << 16) - ext_refs);
	}
	bool fix() const {
		ISO_ALWAYS_ASSERT(ext_refs <= (1 << 15));
		if (ext_refs > (1 << 15)) {
			p->addref(ext_refs);
			return true;
		}
		return false;
	}
	void unfix() const {
		p->addref(-ext_refs);
	}
};

template<typename T, typename R> class atomic<refs<T, R>> : public refs<T, atomic<R> > {
	friend external_refs<T>;
};

template<typename T> class atomic<ref_ptr<T> > {
	iso::atomic<external_refs<T> > p;

	void	set(T *t) {
		external_refs<T> p0, p1(t);
		do
			p0 = p;
		while (!p.cas(p0, p1));
		p0.release();
	}
	T *acquire() {
		external_refs<T> p0, p1;
		do {
			p0	= p;
			p1	= p0;
			p1.addref();
		} while (!p.cas(p0, p1));
	#if 1
		if (p1.fix()) {
			p0.ext_refs = 0;
			if (!p.cas(p1, p0))
				p1.unfix();
		}
	#endif
		return p1.p;
	}

public:
	atomic()				: p(0) {}
	atomic(T *t)			: p(t) {}
	atomic(atomic &b)		: p(b.acquire()) {}
	~atomic()						{ external_refs<T> p0 = p; p0.release(); }
	atomic&	operator=(atomic &b)	{ set(b.acquire()); return *this; }
	T*		operator=(T *t)			{ set(t); return t; }
	ref_ptr<T>	get()				{ return ref_ptr<T>::make_noref(acquire()); }
	operator	ref_ptr<T>()		{ return get(); }
	void		clear()				{ set(0); }

	bool		cas(T *x, T *y)		{
		external_refs<T> p0 = p, p1(y);
		if (p0.p == x && p.cas(p0, p1)) {
			p0.release();
			return true;
		}
		p1.release();
		return false;
	}

	template <typename ... TT> void emplace(TT&&... args) {
		set(new T(forward<TT>(args)...));
	}
	template <typename ... TT> bool try_emplace(T *old, TT&&... args) {
		T *t = new T(forward<TT>(args)...);
		if (cas(old, t))
			return true;
		delete t;
		return false;
	}
};

//-----------------------------------------------------------------------------
//	manual_static
//-----------------------------------------------------------------------------

template<typename T> struct manual_static {
	typedef _atomic_type<1>::type	A;
	space_for<T>	t;
	A				state;

	T&	get() {
		if (_cas(&state, A(0), A(1))) {
			new(&t) T;
			state = 2;
		}
		return *(T*)&t;
	}
	T*	operator&()	{
		return (T*)&t;
	}
};

} // namespace iso

#endif // ATOMIC_H
