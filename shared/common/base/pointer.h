#ifndef POINTER_H
#define POINTER_H

#include "defs.h"
#undef check

namespace iso {

template<class T> class pointer {
	T			*t;
public:
	typedef T			element, *iterator;
	typedef	const T*	const_iterator;
	typedef typename T_traits<T>::ref	reference;

	constexpr pointer()				: t(0)		{}
	constexpr pointer(T *t)			: t(t)		{}
	pointer&		operator=(T *t2)		{ t = t2; return *this; }
	constexpr T*	get()			const	{ return t; }
	constexpr T*	begin()			const	{ return t; }
	constexpr T*	operator->()	const	{ return t;	}
	constexpr operator	T*()		const	{ return t;	}
	friend T*	get(const pointer &a)	{ return a; }
};

template<typename T, typename O, typename P=void, bool nulls=true> struct offset_pointer {
	O		offset;
	constexpr T	*get(const P *base) const	{ return offset ? (T*)((char*)base + offset) : 0; }
	void		set(T *t, const P *base)	{ offset = t ? (char*)t - (char*)base : 0; }
};

template<typename T, typename O, typename P> struct offset_pointer<T,O,P,false> {
	O		offset;
	constexpr T	*get(const P *base) const	{ return (T*)((char*)base + offset); }
	void		set(T *t, const P *base)	{ offset = (char*)t - (char*)base; }
};

template<typename T, typename O, typename P=void, bool nulls=true> struct offset_iterator {
	typedef random_access_iterator_t iterator_category;
	typedef const T	element, *pointer, &reference;
	const P									*base;
	const offset_pointer<T, O, P, nulls>	*p;
	constexpr offset_iterator(const void *_base, const offset_pointer<T, O, P, nulls> *p) : base(_base), p(p) {}
	offset_iterator&	operator++()						{ ++p; return *this; }
	offset_iterator&	operator+=(size_t n)				{ p += n; return *this; }
	offset_iterator&	operator-=(size_t n)				{ p -= n; return *this; }
	constexpr bool	operator==(const offset_iterator &b)	const	{ return p == b.p; }
	constexpr bool	operator!=(const offset_iterator &b)	const	{ return p != b.p; }
	constexpr T&		operator*()							const	{ return *p->get(base); }
	constexpr T*		operator->()						const	{ return p->get(base); }
	constexpr T&		operator[](int i)					const	{ return *p[i].get(base); }
	friend constexpr int operator-(const offset_iterator &a, const offset_iterator &b) {
		return a.p - b.p;
	}
	friend constexpr offset_iterator operator+(const offset_iterator &a, size_t n) {
		return offset_iterator(a.base, a.p + n);
	}
};
template<typename T, typename O, typename P, bool nulls> constexpr offset_iterator<T,O,P,nulls> make_offset_iterator(const P *base, const offset_pointer<T,O,P,nulls> *p) {
	return offset_iterator<T,O,P,nulls>(base, p);
}

template<typename T, typename O, typename P=void, bool nulls=true> struct offset_iterator_ptr : offset_iterator<T,O,P,nulls> {
	typedef offset_iterator<T,O,P,nulls>	B;
	typedef const T	*element, **pointer, *reference;
	constexpr offset_iterator_ptr(const void *base, const offset_pointer<T,O,P,nulls> *p) : B(base, p) {}
	offset_iterator_ptr& operator++()						{ ++B::p; return *this; }
	constexpr T*		operator*()					const	{ return B::p->get(B::base); }
	constexpr T*		operator[](int i)			const	{ return B::p[i].get(B::base); }
	friend constexpr offset_iterator_ptr operator+(const offset_iterator_ptr &a, size_t n) {
		return offset_iterator_ptr(a.base, a.p + n);
	}
};
template<typename T, typename O, typename P=void, bool nulls> constexpr offset_iterator_ptr<T,O,P,nulls> make_offset_iterator_ptr(const void *base, const offset_pointer<T,O,P,nulls> *p) {
	return offset_iterator_ptr<T,O,P,nulls>(base, p);
}

template<typename T, typename O, typename P, bool nulls> force_inline range<offset_iterator<T,O,P,nulls> > make_range_n(const void *base, const offset_pointer<T,O,P,nulls> *p, size_t n) {
	return make_range_n(offset_iterator<T,O,P,nulls>(base, p), n);
}
template<typename T, typename O, typename P, bool nulls, int N> force_inline range<offset_iterator<T,O,P,nulls> > make_range(const void *base, const offset_pointer<T,O,P,nulls> (&a)[N]) {
	return make_range_n(offset_iterator<T,O,P,nulls>(base, a), N);
}
//-----------------------------------------------------------------------------
//	bases for soft_pointer
//-----------------------------------------------------------------------------

struct base_direct {
	void			*ptr;

	static constexpr bool	check(const void *p)	{ return true; }

	constexpr base_direct()	: ptr(0)						{}
	constexpr base_direct(const void *p) : ptr((void*)p)	{}
	void			set(const void *p)	{ ptr = (void*)p; }
	constexpr void*	get()		const	{ return ptr; }
	constexpr bool	operator!()	const	{ return !ptr; }
};

template<typename T> struct base_absolute {
	T				offset;

	static constexpr bool	check(const void *p)		{ return T(p) == intptr_t(p); }
	static constexpr T		assert_check(const void *p)	{ return ISO_CHEAPVERIFY(check(p)), T(p); }
	static constexpr T		calc_check(const void *p)	{ return assert_check(p); }

	constexpr base_absolute()				: offset(0)				{}
	constexpr base_absolute(const void *p)	: offset(calc_check(p)) {}
	void			set(const void *p)	{ offset = calc_check(p); }
	constexpr void*	get()		const	{ return offset ? (void*)intptr_t(offset) : 0; }
	constexpr bool	operator!()	const	{ return !offset; }
};

template<typename T> struct base_relative {
	T				offset;

	static constexpr bool	_check(intptr_t t)			{ return T(t) == t; }
	static constexpr T		assert_check(intptr_t t)	{ return ISO_CHEAPVERIFY(_check(t)), T(t); }
	constexpr intptr_t		calc(const void *p)	 const	{ return p ? (char*)p - (char*)this : 0; }
	constexpr bool			check(const void *p) const	{ return _check(calc(p)); }
	constexpr T				calc_check(const void *p) const { return assert_check(calc(p)); }

	constexpr base_relative()						: offset(0)						{}
	constexpr base_relative(const base_relative &b)	: offset(calc_check(b.get()))	{}
	constexpr base_relative(const void *p)			: offset(calc_check(p))			{}
	void	operator=(const base_relative &b)	{ set(b.get()); }
	void	set(const void *p)					{ offset = calc_check(p); }
	constexpr void*	get()		const			{ return offset ? (char*)this + offset : 0; }
	constexpr bool	operator!()	const			{ return !offset; }
};

template<typename T, typename B> class soft_pointer : B {
public:
	typedef T			element, *iterator;
	typedef	const T*	const_iterator;
	typedef typename T_traits<T>::ref		reference;

	constexpr soft_pointer()					{}
	soft_pointer(T *t)							{ B::set(t); }
	soft_pointer& operator=(T *t)				{ B::set(t); return *this; }
	constexpr T*		get()			const	{ return (T*)B::get(); }
	constexpr T*		begin()			const	{ return get(); }
	constexpr T*		operator->()	const	{ return get(); }
	constexpr operator	T*()			const	{ return get(); }
	constexpr bool		operator!()		const	{ return B::operator!(); }
//	template<typename T2> operator T2*() const	{ return static_cast<T2*>(get()); }
	friend constexpr T*	get(const soft_pointer &a)	{ return a; }
};

template<typename T> struct rel_pointer : soft_pointer<T, base_relative<int32> > {
	constexpr rel_pointer()				{}
	constexpr rel_pointer(T *_t)		: soft_pointer<T, base_relative<int32> >(_t) {}
	rel_pointer&	operator=(T *_t)	{ soft_pointer<T, base_relative<int32> >::operator=(_t); return *this; }
};

iso_export void*		get32(int32 v);
iso_export int32		put32(const void *p);

struct base32 {
	int32			offset;

	constexpr base32()		: offset(0)			{}
	base32(const void *p)	: offset(put32(p))	{}
	void	set(const void *p)			{ offset = put32(p); }
	void*	get()				const	{ return offset ? get32(offset) : 0; }
	bool	operator!()			const	{ return !offset; }
};

template<typename T> using pointer32 = soft_pointer<T, base32>;

template<typename T>			inline size_t	to_string(char *s, const pointer<T> &t)			{ return to_string(s, (const T*)t);}
template<typename T, typename B>inline size_t	to_string(char *s, const soft_pointer<T, B> &t)	{ return to_string(s, (const T*)t);}

}//namespace iso

#endif // POINTER_H
