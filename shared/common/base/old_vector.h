#ifdef NEW_VECTOR
#include "new_vector.h"
#elif !defined VECTOR_H

#define VECTOR_H

#include "maths.h"
#include "_vector.h"

namespace iso {

//-----------------------------------------------------------------------------
//	macros & forward declarations
//-----------------------------------------------------------------------------
template<typename E, int N, int A>					class _v1;
template<typename E, int N, int A, int B>			class _v2;
template<typename E, int N, int A, int B, int C>	class _v3;
template<typename E, int A, int B, int C, int D>	class _v4;

template<typename E, int N> class vec;
template<typename E, int N> class pos;
template<typename E, int N> struct float_components<vec<E,N> > : float_components<E> {};
template<typename E, int N> struct vec_traits<vec<E,N> > { enum {dims = N, simd = true}; };
template<typename E, int N, int A> struct num_traits<_v1<E,N, A> > : num_traits<E> {};
template<typename E, int N> struct vec_traits<pos<E,N> >: vec_traits<vec<E,N> > {};

class colour;
template<typename E> class quat;

//class quaternion;

template<int A>	struct axis_s;
extern axis_s<0> x_axis;
extern axis_s<1> y_axis;
extern axis_s<2> z_axis, xy_plane;
extern axis_s<3> w_axis;

template<typename E, int N, int A>					struct T_param<_v1<E,N,A>		>	: V_param<_v1<E,N,A>	 > {};
template<typename E, int N, int A, int B>			struct T_param<_v2<E,N,A,B>		>	: V_param<_v2<E,N,A,B>	 > {};
template<typename E, int N, int A, int B, int C>	struct T_param<_v3<E,N,A,B,C>	>	: V_param<_v3<E,N,A,B,C> > {};
template<typename E, int A, int B, int C, int D>	struct T_param<_v4<E,A,B,C,D>	>	: V_param<_v4<E,A,B,C,D> > {};

template<typename E, int N> struct T_param<vec<E, N> >	: V_param<vec<E, N> >	{};
template<typename E, int N> struct T_param<pos<E, N> >	: V_param<pos<E, N> >	{};

template<> struct T_param<colour>				: V_param<colour>	{};
template<typename E> struct T_param<quat<E>>	: V_param<quat<E>>	{};

template<int D> struct vdims { enum {dims = D}; };

#if 0
template<> struct vget<void> {};

template<typename U>			vget<U>			get_vget(const U *t, meta::num<vget<U>::dims> *dummy = 0);
template<typename E, int N>		vget<vec<E, N>>	get_vget(const vec<E, N> *t);
vget<void>		get_vget(...);

template<typename T> struct vget : decltype(get_vget((T*)0)) {
	typedef T O;
};
#else
template<typename T> struct vget	{ typedef T O; };
#endif

template<typename T> struct vget<const T>	: vget<T> {};
template<typename T> struct vget<T&>		: vget<T> {};
template<typename T> struct vget<T&&>		: vget<T> {};

#define VLTYPE(T)		typename vget<T>::L
#define VRTYPE(T)		typename vget<T>::R
#define VBTYPE(T)		typename vget<T>::B
#define VHASL(T)		vget_hasl<T>::value
#define VHASR(T)		vget_hasr<T>::value
#define VLTYPE0(X,Y)	typename T_enable_if<VHASR(Y), VLTYPE(X)>::type

#if 1
template<typename T, typename V = void> struct vget_hasl : T_false {};
template<typename T> struct vget_hasl<T, void_t<typename vget<T>::L> > : T_true {};
#else
template<typename T> struct vget_hasl : yesno {
	template<typename U> static yes		test(VLTYPE(U)*);
	template<typename U> static no		test(...);
	static const bool value = sizeof(test<T>(0)) == sizeof(yes);
};
#endif
template<typename T> struct vget_hasr : yesno {
	template<typename U> static yes		test(VRTYPE(U)*);
	template<typename U> static no		test(...);
	static const bool value = sizeof(test<T>(0)) == sizeof(yes);
};

template<typename T, bool L, bool R>	struct _vgetl;
template<typename T, bool R>			struct _vgetl<T, true, R>	: vget<T>	{};
template<typename T>					struct _vgetl<T, false, true>			{ typedef VRTYPE(T) L; static force_inline L l(const T &t) { return vget<T>::r(t); } };
template<typename T>					struct _vgetl<T, false, false>			{};
template<typename T>					struct vgetl : _vgetl<T, VHASL(T), VHASR(T)> {};
template<typename T>					struct vgetl<const T> : vgetl<T> {};
#define VLTYPE1(T)		typename vgetl<T>::L

template<typename X, typename Y>				struct vnonscalar					{ typedef X		R; };
template<typename E, int N, int A, typename Y>	struct vnonscalar<_v1<E,N,A>, Y>	{ typedef Y		R; };
#define VLTYPE2(X,Y)	typename T_enable_if<VHASR(X) && VHASR(Y), typename vnonscalar<VLTYPE1(X),VLTYPE1(Y)>::R>::type

template<typename T>								struct _velement;
template<typename E, int N, int A>					struct _velement<_v1<E, N,A> >		{ typedef E type; };
template<typename E, int N, int A, int B>			struct _velement<_v2<E, N,A,B> >	{ typedef E type; };
template<typename E, int N, int A, int B, int C>	struct _velement<_v3<E, N,A,B,C> >	{ typedef E type; };
template<typename E, int A, int B, int C, int D>	struct _velement<_v4<E, A,B,C,D> >	{ typedef E type; };

template<typename T> struct velement				: _velement<typename vget<T>::L> {};
template<typename X, typename Y> struct velement2	: velement<typename T_if<VHASL(X), X, Y>::type> {};

template<typename T> struct vis_scalar : T_false {};
template<typename E, int N, int A>	struct vis_scalar<_v1<E, N,A> >	: T_true	{};
#define IS_SCALAR(T)		vis_scalar<VRTYPE(T)>::value
#define IF_SCALAR(T,R)		typename T_enable_if<IS_SCALAR(T), R>::type

template<class R, typename E, int N> bool custom_read(R &r, vec<E, N> &v) {
	return check_readbuff(r, &v, sizeof(E) * N);
}
template<class W, typename E, int N> bool custom_write(W &w, const vec<E, N> &v) {
	return check_writebuff(w, &v, sizeof(E) * N);
}

//-----------------------------------------------------------------------------
//	pairs of quads
//-----------------------------------------------------------------------------

#define VP_USE(X)	(X<0?0:1<<(X&7))

template<typename T, int A, int B> struct _v2p {
	enum {USE = VP_USE(A) | VP_USE(B) };
	typedef typename vstorage<T, 0, (USE & 15)>::type	Q1;
	typedef typename vstorage<T, 0, (USE >> 4)>::type	Q2;
	Q1	q1;
	Q2	q2;
	template<typename X, typename Y> _v2p(const X &q1, const Y &q2) : q1(vconvert<Q1>(q1)), q2(vconvert<Q2>(q2)) {}
};
template<typename T, int A, int B, int C> struct _v3p {
	enum {USE = VP_USE(A) | VP_USE(B) | VP_USE(C) };
	typedef typename vstorage<T, 0, (USE & 15)>::type	Q1;
	typedef typename vstorage<T, 0, (USE >> 4)>::type	Q2;
	Q1	q1;
	Q2	q2;
	template<typename X, typename Y> _v3p(const X &q1, const Y &q2) : q1(vconvert<Q1>(q1)), q2(vconvert<Q2>(q2)) {}
};
template<typename T, int A, int B, int C, int D> struct _v4p {
	enum {USE = VP_USE(A) | VP_USE(B) | VP_USE(C) | VP_USE(D) };
	typedef typename vstorage<T, 0, (USE & 15)>::type	Q1;
	typedef typename vstorage<T, 0, (USE >> 4)>::type	Q2;
	Q1	q1;
	Q2	q2;
	template<typename X, typename Y> _v4p(const X &q1, const Y &q2) : q1(vconvert<Q1>(q1)), q2(vconvert<Q2>(q2)) {}
};

#undef VP_USE

//-----------------------------------------------------------------------------
//	_v1 - any 1-element vector
//-----------------------------------------------------------------------------

template<typename E, int N, int A> class VEC_ATTR0 _v1 {
public:
	enum { FIELDMASK = (1<<A) };
	typedef	typename vstorage<E, N, FIELDMASK>::type	storage;
	typedef	typename vstorage<bool, 0, FIELDMASK>::type	bstorage;
	storage	q;
private:
	template<int A2, typename T> static force_inline storage align1(const T &a) {
		return swizzle<storage,
			(A==0?A2:-1),
			(A==1?A2:-1),
			(A==2?A2:-1),
			(A==3?A2:-1)
		>(vconverte<E>(a));
	}
	template<int A2, typename T1, typename T2> static force_inline storage align2(const T1 &a, const T2 &b) {
		return swizzle<storage,
			(A==0?A2:(N&1)?0:-1),
			(A==1?A2:(N&2)?1:-1),
			(A==2?A2:(N&4)?2:-1),
			(A==3?A2:(N&8)?3:-1)
		>(vconverte<E>(a), vconverte<E>(b));
	}
	template<typename T>								static force_inline storage align0(const T &b)							{ return align1<0>(b); }
	template<typename E2, int N2, int A2>				static force_inline storage align0(const _v1<E2,N2,A2> &b)				{ return align1<A2>(b.q); }
	template<typename T1, typename T2>					static force_inline storage align0(const T1 &a, const T2 &b)				{ return align2<4>(a, b); }
	template<typename E2, int N2, int A2, typename T>	static force_inline storage align0(const T &a, const _v1<E2,N2,A2> &b)	{ return align2<A2+4>(a, b.q); }
public:
	template<typename T>				static force_inline storage align(const T &t)				{ return align0(vget<T>::r(t)); }
	template<typename T1, typename T2>	static force_inline storage align(const T1 &a, const T2 &t)	{ return align0(a, vget<T2>::r(t)); }
	template<typename K>				static force_inline auto kalign(const constant<K> &t)		{ return t; };
	template<typename T>				static force_inline auto kalign(const T &t)					{ return align0(vget<T>::r(t)); }

	template<typename T>	force_inline _v1&	operator =(const T &t)	{ q = align0(q, vget<T>::r(t)); return *this; }
	template<typename T>	force_inline _v1&	operator+=(const T &t)	{ q = add(q, align(zero, t)); return *this; }
	template<typename T>	force_inline _v1&	operator-=(const T &t)	{ q = sub(q, align(zero, t)); return *this; }
	template<typename T>	force_inline _v1&	operator*=(const T &t)	{ q = mul(q, align(one, t)); return *this; }
	template<typename T>	force_inline _v1&	operator/=(const T &t)	{ q = div(q, align(one, t)); return *this; }

	_v1<E,0,A>			operator()()			const	{ return force_cast<_v1<E,0,A> >(q); }
	template<int N2>	operator _v1<E,N2,A>()	const	{ return force_cast<_v1<E,N2,A> >(q); }
	operator			E()						const	{ return vextract<A>(q); }

	friend void swap(_v1 &v1, _v1 &v2) {
		vu4	q = vand(vxor((vu4)v1.q, (vu4)v2.q), force_cast<vu4>(swizzle<
			(A==0?4:0),
			(A==1?4:0),
			(A==2?4:0),
			(A==3?4:0)
		>(zero, nan)));
		v1.q = (vf4)vxor((vu4)v1.q, q);
		v2.q = (vf4)vxor((vu4)v2.q, q);
	}
} VEC_ATTR;

//-----------------------------------------------------------------------------
//	_v2 - any 2-element vector
//-----------------------------------------------------------------------------

template<typename E, int N, int A, int B> class VEC_ATTR0 _v2 {
public:
	enum { FIELDMASK = (1<<A) | (1<<B) };
	typedef	typename vstorage<E, N, FIELDMASK>::type		storage;
	typedef	typename vstorage<bool, 0, FIELDMASK>::type		bstorage;
	storage	q;
private:
	template<typename E2, int N2, int A2, int B2> friend class _v2;
	template<int A2, int B2, typename T> static force_inline storage align1(const T &a) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:-1),
			(A==1?A2:B==1?B2:-1),
			(A==2?A2:B==2?B2:-1),
			(A==3?A2:B==3?B2:-1)
		>(vconverte<E>(a));
	}
	template<int A2, int B2, typename T1, typename T2> static force_inline storage align2(const T1 &a, const T2 &b) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:(N&1)?0:-1),
			(A==1?A2:B==1?B2:(N&2)?1:-1),
			(A==2?A2:B==2?B2:(N&4)?2:-1),
			(A==3?A2:B==3?B2:(N&8)?3:-1)
		>(vconverte<E>(a), vconverte<E>(b));
	}
	template<typename T>										static force_inline storage align0(const T &b)							{ return align1<0,1>(b); }
	template<typename E2, int N2, int A2>						static force_inline storage align0(const _v1<E2,N2,A2> &b)				{ return align1<A2,A2>(b.q); }
	template<typename E2, int N2, int A2, int B2>				static force_inline storage align0(const _v2<E2,N2,A2,B2> &b)			{ return align1<A2,B2>(b.q); }
	template<typename E2, int A2, int B2>						static force_inline auto align0(const _v2p<E2,A2,B2> &b)					{ return _v2<E,0,A,B>::template align2<A2,B2>(b.q1, b.q2); }
	template<typename T1, typename T2>							static force_inline storage align0(const T1 &a, const T2 &b)				{ return align2<4,5>(a, b); }
	template<typename E2, int N2, int A2, typename T>			static force_inline storage align0(const T &a, const _v1<E2,N2,A2> &b)	{ return align2<A2+4,A2+4>(a, b.q); }
	template<typename E2, int N2, int A2, int B2, typename T>	static force_inline storage align0(const T &a, const _v2<E2,N2,A2,B2> &b){ return align2<A2+4,B2+4>(a, b.q); }
	template<typename E2, int A2, int B2, typename T>			static force_inline storage align0(const T &a, const _v2p<E2,A2,B2> &b)	{ return align2<A+4,B+4>(a, align(b)); }

//	template<typename T>										static force_inline storage fill0(const T &b)							{ return swizzle<storage,0,0,0,0>(b); }
//	template<typename E2, int N2, int A2>						static force_inline storage fill0(const _v1<E2,N2,A2> &b)				{ return swizzle<storage,A2,A2,A2,A2>(b.q); }
//	template<typename E2, int N2, int A2, int B2>				static force_inline storage fill0(const _v2<E2,N2,A2,B2> &b)				{ return swizzle<storage,A==0?A2:B2,A==1?A2:B2,A==2?A2:B2,A==3?A2:B2>(b.q); }
//	template<typename E2, int A2, int B2>						static force_inline storage fill0(const _v2p<E2,A2,B2> &b)				{ return swizzle<storage,A==0?A2:B2,A==1?A2:B2,A==2?A2:B2,A==3?A2:B2>(b.q1, b.q2); }
public:
	template<typename T>				static force_inline auto align(const T &t)				{ return align0(vget<T>::r(t)); }
	template<typename T1, typename T2>	static force_inline auto align(const T1 &a, const T2 &t)	{ return align0(a, vget<T2>::r(t)); }
	template<typename K>				static force_inline auto kalign(const constant<K> &t)	{ return t; };
	template<typename T>				static force_inline auto kalign(const T &t)				{ return align0(vget<T>::r(t)); }
//	template<typename T>				static force_inline storage fill(const T &t)				{ return fill0(vget<T>::r(t)); }

	template<typename T>	force_inline _v2&	operator =(const T &t)	{ q = align(q, t); return *this; }
	template<typename T>	force_inline _v2&	operator+=(const T &t)	{ q = add(q, align(zero, t)); return *this; }
	template<typename T>	force_inline _v2&	operator-=(const T &t)	{ q = sub(q, align(zero, t)); return *this; }
	template<typename T>	force_inline _v2&	operator*=(const T &t)	{ q = mul(q, align(one, t)); return *this; }
	template<typename T>	force_inline _v2&	operator/=(const T &t)	{ q = div(q, align(one, t)); return *this; }

	_v2<E,0,A,B>			operator()() const	{ return force_cast<_v2<E,0,A,B> >(q); }

	friend void swap(_v2 &v1, _v2 &v2) {
		vu4	q = vand(vxor((vu4)v1.q, (vu4)v2.q), force_cast<vu4>(swizzle<
			(A==0||B==0?4:0),
			(A==1||B==1?4:0),
			(A==2||B==2?4:0),
			(A==3||B==3?4:0)
		>(zero, nan)));
		v1.q = (vf4)vxor((vu4)v1.q, q);
		v2.q = (vf4)vxor((vu4)v2.q, q);
	}
} VEC_ATTR;

//-----------------------------------------------------------------------------
//	_v3 - any 3-element vector
//-----------------------------------------------------------------------------

template<typename E, int N, int A, int B, int C> class VEC_ATTR0 _v3 {
public:
	enum { FIELDMASK = (1<<A) | (1<<B) | (1<<C) };
	typedef	typename vstorage<E, N, FIELDMASK>::type	storage;
	typedef	typename vstorage<bool, 0, FIELDMASK>::type	bstorage;
	storage	q;
private:
	template<typename E2, int N2, int A2, int B2, int C2> friend class _v3;
	template<int A2, int B2, int C2, typename T> static force_inline storage align1(const T &a) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:C==0?C2:-1),
			(A==1?A2:B==1?B2:C==1?C2:-1),
			(A==2?A2:B==2?B2:C==2?C2:-1),
			(A==3?A2:B==3?B2:C==3?C2:-1)
		>(vconverte<E>(a));
	}
	template<int A2, int B2, int C2, typename T1, typename T2> static force_inline storage align2(const T1 &a, const T2 &b) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:C==0?C2:(N&1)?0:-1),
			(A==1?A2:B==1?B2:C==1?C2:(N&2)?1:-1),
			(A==2?A2:B==2?B2:C==2?C2:(N&4)?2:-1),
			(A==3?A2:B==3?B2:C==3?C2:(N&8)?3:-1)
		>(vconverte<E>(a), vconverte<E>(b));
	}

	template<typename T>											static force_inline storage align0(const T &b)								{ return align1<0,1,2>(b); }
	template<typename E2, int N2, int A2>							static force_inline storage align0(const _v1<E2,N2,A2> &b)					{ return align1<A2,A2,A2>(b.q); }
	template<typename E2, int N2, int A2, int B2>					static force_inline storage align0(const _v2<E2,N2,A2,B2> &b)				{ return align2<A2,B2,4>(b.q, zero); }
	template<typename E2, int N2, int A2, int B2, int C2>			static force_inline storage align0(const _v3<E2,N2,A2,B2,C2> &b)				{ return align1<A2,B2,C2>(b.q); }
	template<typename E2, int A2, int B2, int C2>					static force_inline storage align0(const _v3p<E2,A2,B2,C2> &b)				{ return _v3<E,0,A,B,C>::template align2<A2,B2,C2>(b.q1, b.q2); }
	template<typename T1, typename T2>								static force_inline storage align0(const T1 &a, const T2 &b)					{ return align2<4,5,6>(a, b); }
	template<typename E2, int N2, int A2, typename T>				static force_inline storage align0(const T &a, const _v1<E2,N2,A2> &b)		{ return align2<A2+4,A2+4,A2+4>(a, b.q); }
	template<typename E2, int N2, int A2, int B2, int C2, typename T>static force_inline storage align0(const T &a, const _v3<E2,N2,A2,B2,C2> &b){ return align2<A2+4,B2+4,C2+4>(a, b.q); }
	template<typename E2, int A2, int B2, int C2, typename T>		static force_inline storage align0(const T &a, const _v3p<E2,A2,B2,C2> &b)	{ return align2<A+4,B+4,C+4>(a, align(b)); }

//	template<typename T>											static force_inline storage fill0(const T &b)								{ return swizzle<storage,0,0,0,0>(b); }
//	template<typename E2, int N2, int A2>							static force_inline storage fill0(const _v1<E2,N2,A2> &b)					{ return swizzle<storage,A2,A2,A2,A2>(b.q); }
//	template<typename E2, int N2, int A2, int B2, int C2>			static force_inline storage fill0(const _v3<E2,N2,A2,B2,C2> &b)				{ return swizzle<storage,A==0?A2:B==0?B2:C2,A==1?A2:B==1?B2:C2,A==2?A2:B==2?B2:C2,A==3?A2:B==3?B2:C2>(b.q); }
//	template<typename E2, int A2, int B2, int C2>					static force_inline storage fill0(const _v3p<E2,A2,B2,C2> &b)				{ return swizzle<storage,A==0?A2:B==0?B2:C2,A==1?A2:B==1?B2:C2,A==2?A2:B==2?B2:C2,A==3?A2:B==3?B2:C2>(b.q1, b.q2); }
public:

	template<typename T>				static force_inline storage align(const T &t)				{ return align0(vget<T>::r(t)); }
	template<typename T1, typename T2>	static force_inline storage align(const T1 &a, const T2 &t)	{ return align0(a, vget<T2>::r(t)); }
	template<typename K>				static force_inline auto kalign(const constant<K> &t)		{ return t; };
	template<typename T>				static force_inline auto kalign(const T &t)					{ return align0(vget<T>::r(t)); }
//	template<typename T>				static force_inline storage fill(const T &t)					{ return fill0(vget<T>::r(t)); }

	template<typename T>	force_inline _v3&	operator =(const T &t)			{ q = align(q, t); return *this; }
	template<typename T>	force_inline _v3&	operator+=(const T &t)			{ q = add(q, align(zero, t)); return *this; }
	template<typename T>	force_inline _v3&	operator-=(const T &t)			{ q = sub(q, align(zero, t)); return *this; }
	template<typename T>	force_inline _v3&	operator*=(const T &t)			{ q = mul(q, align(one, t)); return *this; }
	template<typename T>	force_inline _v3&	operator/=(const T &t)			{ q = div(q, align(one, t)); return *this; }

	_v3<E,0,A,B,C>			operator()() const	{ return force_cast<_v3<E,0,A,B,C> >(q); }

	friend void swap(_v3 &v1, _v3 &v2) {
		vu4	q = vand(vxor((vu4)v1.q, (vu4)v2.q), force_cast<vu4>(swizzle<
			(A==0||B==0||C==0?4:0),
			(A==1||B==1||C==1?4:0),
			(A==2||B==2||C==2?4:0),
			(A==3||B==3||C==3?4:0)
		>(zero, nan)));
		v1.q = (vf4)vxor((vu4)v1.q, q);
		v2.q = (vf4)vxor((vu4)v2.q, q);
	}
} VEC_ATTR;

#ifdef __MWERKS__
force_inline _v3<0, 0, 1, 2>	operator+(const _v3<0, 0, 1, 2> &a, const _v3<0, 0, 1, 2> &b)	{ _v3<0, 0, 1, 2> v; wii::add((float3*)&v, (float3*)&a, (float3*)&b); return v; }
force_inline _v3<0, 0, 1, 2>	operator-(const _v3<0, 0, 1, 2> &a, const _v3<0, 0, 1, 2> &b)	{ _v3<0, 0, 1, 2> v; wii::sub((float3*)&v, (float3*)&a, (float3*)&b); return v; }
#endif

//-----------------------------------------------------------------------------
//	_v4 - any 4-element vector
//-----------------------------------------------------------------------------

template<typename E, int A, int B, int C, int D> class VEC_ATTR0 _v4 {
public:
	enum { FIELDMASK = (1<<A) | (1<<B) | (1<<C) | (1<<D) };
	typedef	typename vstorage<E, 15, FIELDMASK>::type	storage;
	typedef	typename vstorage<bool, 0, FIELDMASK>::type	bstorage;
	storage	q;
private:
	template<int A2, int B2, int C2, int D2, typename T> static force_inline storage align1(const T &a) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:C==0?C2:D2),
			(A==1?A2:B==1?B2:C==1?C2:D2),
			(A==2?A2:B==2?B2:C==2?C2:D2),
			(A==3?A2:B==3?B2:C==3?C2:D2)
		>(vconverte<E>(a));
	}
	template<int A2, int B2, int C2, int D2, typename T1, typename T2> static force_inline storage align2(const T1 &a, const T2 &b) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:C==0?C2:D2),
			(A==1?A2:B==1?B2:C==1?C2:D2),
			(A==2?A2:B==2?B2:C==2?C2:D2),
			(A==3?A2:B==3?B2:C==3?C2:D2)
		>(vconverte<E>(a), vconverte<E>(b));
	}
	template<typename T>									static force_inline storage align0(const T &b)					{ return align1<0,1,2,3>(b); }
	template<typename E2, int N2, int A2>					static force_inline storage align0(const _v1<E2,N2,A2> &b)		{ return align1<A2,A2,A2,A2>(b.q); }
	template<typename E2, int N2, int A2, int B2>			static force_inline storage align0(const _v2<E2,N2,A2,B2> &b)	{ return align2<A2,B2,4,4>(b.q, zero); }
	template<typename E2, int N2, int A2, int B2, int C2>	static force_inline storage align0(const _v3<E2,N2,A2,B2,C2> &b)	{ return align2<A2,B2,C2,4>(b.q, zero); }
	template<typename E2, int A2, int B2, int C2, int D2>	static force_inline storage align0(const _v4<E2,A2,B2,C2,D2> &b)	{ return align1<A2,B2,C2,D2>(b.q); }
	template<typename E2, int A2, int B2, int C2, int D2>	static force_inline storage align0(const _v4p<E2,A2,B2,C2,D2> &b){ return align2<A2,B2,C2,D2>(b.q1, b.q2); }
public:
	template<typename T>	static force_inline storage align(const T &t)			{ return align0(vget<T>::r(t)); }
	template<typename K>	static force_inline auto kalign(const constant<K> &t)	{ return t; };
	template<typename T>	static force_inline auto kalign(const T &t)				{ return align0(vget<T>::r(t)); }
//	template<typename T>	static force_inline storage fill(const T &t)		{ return align0(vget<T>::r(t)); }

	force_inline	_v4 set(E a, E b, E c, E d) {
		q = vload(
			A==0?a:B==0?b:C==0?c:d,
			A==1?a:B==1?b:C==1?c:d,
			A==2?a:B==2?b:C==2?c:d,
			A==3?a:B==3?b:C==3?c:d
		);
		return *this;
	}
	template<typename T>	force_inline _v4&	operator =(const T &t)		{ q = align(t); return *this; }
	template<typename T>	force_inline _v4&	operator+=(const T &t)		{ q = add(q, align(t)); return *this; }
	template<typename T>	force_inline _v4&	operator-=(const T &t)		{ q = sub(q, align(t)); return *this; }
	template<typename T>	force_inline _v4&	operator*=(const T &t)		{ q = mul(q, align(t)); return *this; }
	template<typename T>	force_inline _v4&	operator/=(const T &t)		{ q = div(q, align(t)); return *this; }

	_v4<E,A,B,C,D>			operator()() const	{ *this; }
} VEC_ATTR;

#ifdef __MWERKS__
force_inline _v4<0, 1, 2, 3>	operator+(const _v4<0, 1, 2, 3> &a, const _v4<0, 1, 2, 3> &b)	{ _v4<0, 1, 2, 3> v; wii::add((float4*)&v, (float4*)&a, (float4*)&b); return v; }
force_inline _v4<0, 1, 2, 3>	operator-(const _v4<0, 1, 2, 3> &a, const _v4<0, 1, 2, 3> &b)	{ _v4<0, 1, 2, 3> v; wii::sub((float4*)&v, (float4*)&a, (float4*)&b); return v; }
#endif

//-----------------------------------------------------------------------------
//	boolean vectors
//-----------------------------------------------------------------------------

template<typename X, typename Y> force_inline VLTYPE1(X) select(bool b, const X &x, const Y &y) {
	typedef VLTYPE1(X) L;
	return force_cast<L>(vsel(L::align(y), vgetl<X>::l(x).q, vloadb<typename L::bstorage>(b)));
}
template<typename X, typename Y> force_inline VLTYPE1(X) select(uint8 m, const X &x, const Y &y) {
	typedef VLTYPE1(X) L;
	return force_cast<L>(vsel(L::align(y), vgetl<X>::l(x).q, vloadb<typename L::bstorage>(m)));
}

template<int N, int A> class _v1<bool, N, A> {
public:
	enum { FIELDMASK = (1<<A) };
	typedef	typename vstorage<bool,0,FIELDMASK>::type	storage;
	storage	q;
private:
	template<int A2, typename T>	static force_inline storage align(const T &a) {
		return swizzle<storage,
			(A==0?A2:4),
			(A==1?A2:4),
			(A==2?A2:4),
			(A==3?A2:4)
		>(a);
	}
public:
	template<typename T>		static force_inline storage align(const T &t)					{ return align(vget<T>::b(t)); }
	template<int N2, int A2>	static force_inline storage align(const _v1<bool, N2, A2> &b)	{ return align<A2>(b.q); }
	force_inline	operator bool() const	{ return vextract<A>(q); }

	template<typename T, typename F> struct select_type {
		typedef VLTYPE1(T) type;
	};
	template<typename T, typename F> force_inline typename select_type<T,F>::type select(const T &t, const F &f) const {
		typedef typename select_type<T,F>::type R;
		typedef	typename vstorage<bool,0,15>::type	storage2;
		return force_cast<R>(vsel(R::align(f), R::align(t), swizzle<storage2,A,A,A,A>(q)));
	}
};

template<int N, int A, int B> class _v2<bool, N, A, B> {
public:
	enum { FIELDMASK = (1<<A) | (1<<B) };
	typedef	typename vstorage<bool,0,FIELDMASK>::type	storage;
	storage	q;
private:
	template<int A2, int B2, typename T> static force_inline storage align(const T &a) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:-1),
			(A==1?A2:B==1?B2:-1),
			(A==2?A2:B==2?B2:-1),
			(A==3?A2:B==3?B2:-1)
		>(a);
	}
public:
	template<typename T>				static force_inline storage align(const T &t)					{ return align(vget<T>::b(t)); }
	template<int N2, int A2>			static force_inline storage align(const _v1<bool, N2, A2> &b)	{ return align<A2,A2>(b.q); }
	template<int N2, int A2, int B2>	static force_inline storage align(const _v2<bool, N2, A2,B2> &b)	{ return align<A2,B2>(b.q); }
	uint8	mask() const {
		return vmask<
			(A==0?0:B==0?1:4),
			(A==1?0:B==1?1:4),
			(A==2?0:B==2?1:4),
			(A==3?0:B==3?1:4)
		>(q);
	}
	friend bool	all(const _v2 &v)	{ return int(vmask(v.q) & FIELDMASK) == int(FIELDMASK); }
	friend bool	any(const _v2 &v)	{ return (vmask(v.q) & FIELDMASK) != 0; }

	template<typename T, typename F> struct select_type {
		typedef _v2<typename velement2<T,F>::type,0,A,B> type;
	};
	template<typename T, typename F> force_inline typename select_type<T,F>::type select(const T &t, const F &f) const {
		typedef typename select_type<T,F>::type R;
		return force_cast<R>(vsel(R::align(f), R::align(t), q));
	}
};

template<int N, int A, int B, int C> class _v3<bool, N, A, B, C> {
public:
	enum { FIELDMASK = (1<<A) | (1<<B) | (1<<C) };
	typedef	typename vstorage<bool,0,FIELDMASK>::type	storage;
	storage	q;
private:
	template<int A2, int B2, int C2, typename T> static force_inline storage align(const T &a) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:C==0?C2:-1),
			(A==1?A2:B==1?B2:C==1?C2:-1),
			(A==2?A2:B==2?B2:C==2?C2:-1),
			(A==3?A2:B==3?B2:C==3?C2:-1)
		>(a);
	}
public:
	template<typename T>						static force_inline storage align(const T &t)						{ return align(vget<T>::b(t)); }
	template<int N2, int A2>					static force_inline storage align(const _v1<bool, N2, A2> &b)		{ return align<A2,A2,A2>(b.q); }
	template<int N2, int A2, int B2, int C2>	static force_inline storage align(const _v3<bool, N2, A2,B2,C2> &b)	{ return align<A2,B2,C2>(b.q); }
	uint8	mask() const {
		return vmask<
			(A==0?0:B==0?1:C==0?2:4),
			(A==1?0:B==1?1:C==1?2:4),
			(A==2?0:B==2?1:C==2?2:4),
			(A==3?0:B==3?1:C==3?2:4)
		>(q);
	}
	friend bool	all(const _v3 &v)	{ return int(vmask(v.q) & FIELDMASK) == int(FIELDMASK); }
	friend bool	any(const _v3 &v)	{ return (vmask(v.q) & FIELDMASK) != 0; }

	template<typename T, typename F> struct select_type {
		typedef _v3<typename velement2<T,F>::type,0,A,B,C> type;
	};
	template<typename T, typename F> force_inline typename select_type<T,F>::type select(const T &t, const F &f) const {
		typedef typename select_type<T,F>::type R;
		return force_cast<R>(vsel(R::align(f), R::align(t), q));
	}
};

template<int A, int B, int C, int D> class _v4<bool, A, B, C, D> {
public:
	enum { FIELDMASK = (1<<A) | (1<<B) | (1<<C) | (1<<D) };
	typedef	typename vstorage<bool,0,FIELDMASK>::type	storage;
	storage	q;
private:
	template<int A2, int B2, int C2, int D2, typename T> static force_inline storage align(const T &a) {
		return swizzle<storage,
			(A==0?A2:B==0?B2:C==0?C2:D2),
			(A==1?A2:B==1?B2:C==1?C2:D2),
			(A==2?A2:B==2?B2:C==2?C2:D2),
			(A==3?A2:B==3?B2:C==3?C2:D2)
		>(a);
	}
public:
	template<typename T>						static force_inline storage align(const T &t)						{ return align(vget<T>::b(t)); }
	template<int N2, int A2>					static force_inline storage align(const _v1<bool, N2, A2> &b)		{ return align<A2,A2,A2,A2>(b.q); }
	template<int A2, int B2, int C2, int D2>	static force_inline storage align(const _v4<bool, A2,B2,C2,D2> &b)	{ return align<A2,B2,C2,D2>(b.q); }
	uint8	mask() const {
		return vmask<
			(A==0?0:B==0?1:C==0?2:D==0?3:4),
			(A==1?0:B==1?1:C==1?2:D==1?3:4),
			(A==2?0:B==2?1:C==2?2:D==2?3:4),
			(A==3?0:B==3?1:C==3?2:D==3?3:4)
		>(q);
	}
	friend bool	all(const _v4 &v)	{ return int(vmask(v.q) & FIELDMASK) == int(FIELDMASK); }
	friend bool	any(const _v4 &v)	{ return (vmask(v.q) & FIELDMASK) != 0; }

	template<typename T, typename F> struct select_type {
		typedef _v4<typename velement2<T,F>::type,A,B,C,D> type;
	};
	template<typename T, typename F> force_inline typename select_type<T,F>::type select(const T &t, const F &f) const {
		typedef typename select_type<T,F>::type R;
		return force_cast<R>(vsel(R::align(f), R::align(t), q));
	}
};

//-----------------------------------------------------------------------------
//	deferred boolean vectors
//-----------------------------------------------------------------------------

#ifdef VEC_ALL_TESTS
template<typename P> struct vall_test;
template<> struct vall_test<equal_to>		{ template<typename A, typename B> static force_inline bool all(const A &a, const B &b)	{ return alleq(a, b); } };
template<> struct vall_test<not_equal_to>	{ template<typename A, typename B> static force_inline bool all(const A &a, const B &b)	{ return !anyeq(a, b); } };
template<> struct vall_test<less_equal>		{ template<typename A, typename B> static force_inline bool all(const A &a, const B &b)	{ return allle(a, b); } };
template<> struct vall_test<greater>		{ template<typename A, typename B> static force_inline bool all(const A &a, const B &b)	{ return !anyle(a, b); } };
template<> struct vall_test<greater_equal>	{ template<typename A, typename B> static force_inline bool all(const A &a, const B &b)	{ return allge(a, b); } };
template<> struct vall_test<less>			{ template<typename A, typename B> static force_inline bool all(const A &a, const B &b)	{ return !anyge(a, b); } };
#endif

template<typename X, typename Y, typename P> struct to_bool_s {};

template<typename X, typename Y, typename P> class _vbd {
	typedef _vbd	type;
	typedef typename vget<_vbd<X,Y,P> >::B B;
	X				x;
	const	Y		&y;
public:
	_vbd(const X &x, const Y &y) : x(x), y(y)	{}
	force_inline auto	getvb()			const	{ return vcmp<P>(x.q, x.align(y)); }
	force_inline uint8	mask()			const	{ return force_cast<B>(getvb()).mask(); }
	force_inline	auto	operator!()		const	{ return _vbd<X,Y,typename P::not_t>(x, y); }
	force_inline	operator bool()			const	{ return to_bool_s<X,Y,P>::f(*this); }
	template<typename T, typename F> force_inline typename B::template select_type<T,F>::type select(const T &t, const F &f) const { return force_cast<B>(getvb()).select(t, f); }

#ifdef VEC_ALL_TESTS
	friend bool	all(const _vbd &v)	{ return vall_test<P>::all(X::fill(v.x), X::fill(v.y)); }
	friend bool	any(const _vbd &v)	{ return !vall_test<typename P::not_t>::all(X::fill(v.x), X::fill(v.y)); }
#else
	friend bool	all(const _vbd &v)	{ return int(vmask(v.getvb()) & X::FIELDMASK) == int(X::FIELDMASK); }
	friend bool	any(const _vbd &v)	{ return (vmask(v.getvb()) & X::FIELDMASK) != 0; }
#endif
};

template<typename X, typename Y> struct to_bool_s<X,Y,equal_to>		{ static force_inline bool f(const _vbd<X,Y,equal_to> &b)		{ return all(b); } };
template<typename X, typename Y> struct to_bool_s<X,Y,not_equal_to> { static force_inline bool f(const _vbd<X,Y,not_equal_to> &b)	{ return any(b); } };

template<typename E, int N, int A, typename Y, typename P> class _vbd<_v1<E,N,A>, Y, P> {
	typedef _vbd			type;
	typedef _v1<bool, N, A>	B;
	_v1<E,N,A>		x;
	const	Y		&y;
public:
	_vbd(const _v1<E,N,A> &x, const Y &t) : x(x), y(t)	{}
	auto		getvb()			const	{ return vcmp<P>(x.q, x.align(y)); }
	template<typename T, typename F> force_inline VLTYPE1(T) select(const T &t, const F &f) const { return force_cast<B>(getvb()).select(t, f); }

#ifdef VEC_ALL_TESTS
	force_inline	operator bool() const	{ return vall_test<P>::all(_v4<0,1,2,3>::align(x), _v4<0,1,2,3>::align(y));}
#else
	force_inline	operator bool() const	{ return vextract<A>(getvb()); }
#endif
};

template<typename X, typename Y, typename P, typename T, typename F> force_inline VLTYPE1(T) select(const _vbd<X,Y,P> &m, const T &t, const F &f) {
	return m.select(t, f);
}

//-----------------------------------------------------------------------------
//	quad-pair generation
//-----------------------------------------------------------------------------

template<typename E1, typename E2>	struct vgetbest					{ typedef E1 type; };
template<typename E1>				struct vgetbest<E1,float>		{ typedef float type; };
template<typename E1>				struct vgetbest<E1,double>		{ typedef double type; };
template<>							struct vgetbest<double,float>	{ typedef double type; };

template<typename T0, typename... T>	struct vgetetype	: vgetbest<typename vgetetype<T0>::type, typename vgetetype<T...>::type> {};
template<typename T>					struct vgetetype<T>	: vgetetype<VLTYPE1(T)> {};
template<typename E, int N, int A>					struct vgetetype<_v1<E,N,A>		>	{ typedef E type; };
template<typename E, int N, int A, int B>			struct vgetetype<_v2<E,N,A,B>	>	{ typedef E type; };
template<typename E, int N, int A, int B, int C>	struct vgetetype<_v3<E,N,A,B,C>	>	{ typedef E type; };
template<typename E, int A, int B, int C, int D>	struct vgetetype<_v4<E,A,B,C,D>	>	{ typedef E type; };

template<typename _R> struct vgetpair_l {
	typedef _R			R;
	typedef VLTYPE(R) 	L, O;
	template<typename A, typename B>	static force_inline R	r(const pair<A,B> &p)	{ return R(vget<A>::r(p.a).q, vget<B>::r(p.b).q); }
	template<typename T>				static force_inline L	l(const T &p)			{ return vget<R>::l(r(p)); }
};

template<typename A, typename B>	struct vgetpair;
template<typename e, typename E, int n, int N, int a, int A>				struct vgetpair<_v1<e,n,a>,		_v1<E,N,A>		>	: vgetpair_l<_v2p<typename vgetbest<e,E>::type, a, A+4>				> {};
template<typename e, typename E, int n, int N, int a, int b, int A>			struct vgetpair<_v2<e,n,a,b>,	_v1<E,N,A>		>	: vgetpair_l<_v3p<typename vgetbest<e,E>::type, a, b, A+4>			> {};
template<typename e, typename E, int n, int N, int a, int b, int c, int A>	struct vgetpair<_v3<e,n,a,b,c>, _v1<E,N,A>		>	: vgetpair_l<_v4p<typename vgetbest<e,E>::type, a, b, c, A+4>		> {};
template<typename e, typename E, int n, int N, int a, int A, int B>			struct vgetpair<_v1<e,n,a>,		_v2<E,N,A,B>	>	: vgetpair_l<_v3p<typename vgetbest<e,E>::type, a, A+4, B+4> 		> {};
template<typename e, typename E, int n, int N, int a, int b, int A, int B>	struct vgetpair<_v2<e,n,a,b>,	_v2<E,N,A,B>	>	: vgetpair_l<_v4p<typename vgetbest<e,E>::type, a, b, A+4, B+4>		> {};
template<typename e, typename E, int n, int N, int a, int A, int B, int C>	struct vgetpair<_v1<e,n,a>,		_v3<E,N,A,B,C>	>	: vgetpair_l<_v4p<typename vgetbest<e,E>::type, a, A+4, B+4, C+4>	> {};

template<int X, typename A>		struct vgetindex : vgetindex<X, VLTYPE1(A)> {};
template<int X, typename E, int N, int A>				struct vgetindex<X, _v1<E,N,A> >		{ enum { value = X == 0 ? A : -1}; };
template<int X, typename E, int N, int A, int B>		struct vgetindex<X, _v2<E,N,A,B> >		{ enum { value = X == 0 ? A : X == 1 ? B : -1}; };
template<int X, typename E, int N, int A, int B, int C>	struct vgetindex<X, _v3<E,N,A,B,C> >	{ enum { value = X == 0 ? A : X == 1 ? B : X == 2 ? C : -1}; };
template<int X, typename E, int A, int B, int C, int D>	struct vgetindex<X, _v4<E,A,B,C,D> >	{ enum { value = X == 0 ? A : X == 1 ? B : X == 2 ? C : X == 3 ? D : -1}; };

template<int X, typename A, typename B>		struct vgetindex2 {
	enum {
		AD		= max(vget<A>::dims, 1),
		value	= X < AD ? vgetindex<X, A>::value : vgetindex<X - AD, B>::value + 4
	};
};

template<typename X, typename Y> force_inline
typename T_enable_if<VHASR(X) && VHASR(Y), typename vgetpair<VLTYPE(X), VLTYPE(Y)>::R>::type
operator,(const X &x, const Y &y) { return typename vgetpair<VLTYPE(X), VLTYPE(Y)>::R(vgetl<X>::l(x).q, vgetl<Y>::l(y).q); }

template<typename T> struct dims_s : meta::num<vget<T>::dims> {};
template<typename A, typename B> struct dims_s<pair<A, B>> : meta::num<dims_s<A>::value + dims_s<B>::value> {};

template<typename A> force_inline auto				max_pairs(const A &a)				{ return a; }
template<typename A, typename B> force_inline auto	max_pairs(const pair<A,B> &p)		{ return max(max_pairs(p.a), max_pairs(p.b)); }

template<typename A> force_inline auto				min_pairs(const A &a)				{ return a; }
template<typename A, typename B> force_inline auto	min_pairs(const pair<A,B> &p)		{ return min(min_pairs(p.a), min_pairs(p.b)); }

//-----------------------------------------------------------------------------
//	vget
//-----------------------------------------------------------------------------

// scalars
template<typename T> struct vget_scalar : vdims<1> {
	typedef T			O;
	typedef _v1<T,0,0>	R;
	static force_inline R r(T t)	{ return force_cast<R>(vload(t)); }
};

template<> struct vget<int>		: vget_scalar<int>		{};
template<> struct vget<uint32>	: vget<int>				{};
template<> struct vget<uint16>	: vget<int>				{};
template<> struct vget<float>	: vget_scalar<float>	{};
template<> struct vget<double>	: vget_scalar<double>	{};

#if USE_LONG
template<> struct vget<long>	: vget<float> {};
#endif

// boolean vectors
template<int N, int A> struct vget<_v1<bool,N,A> > : vdims<1> {
	typedef _v1<bool,N,A> T, B;
	static force_inline B	b(const T &t)	{ return t; }
};
template<int N, int A, int BB> struct vget<_v2<bool,N,A,BB> > : vdims<2> {
	typedef _v2<bool,N,A,BB> T, B;
	static force_inline B	b(const T &t)	{ return t; }
};
template<int N, int A, int BB, int C> struct vget<_v3<bool,N,A,BB,C> > : vdims<3> {
	typedef _v3<bool,N,A,BB,C> T, B;
	static force_inline B	b(const T &t)	{ return t; }
};
template<int A, int BB, int C, int D> struct vget<_v4<bool,A,BB,C,D> > : vdims<4> {
	typedef _v4<bool,A,BB,C,D> T, B;
	static force_inline B	b(const T &t)	{ return t; }
};

// deferred boolean vectors
template<typename E, int N, int A, typename Y, typename P> struct vget<_vbd<_v1<E,N,A>,Y,P> > : vdims<1> {
	typedef _vbd<_v1<E,N,A>,Y,P>		T;
	typedef _v1<bool,N,A>				B;
	static force_inline B b(const T &t)	{ return force_cast<B>(t.getvb()); }
};
template<typename E, int N, int A, int BB, typename Y, typename P> struct vget<_vbd<_v2<E,N,A,BB>,Y,P> > : vdims<2> {
	typedef _vbd<_v2<E,N,A,BB>,Y,P>		T;
	typedef _v2<bool,N,A,BB>			B;
	static force_inline B b(const T &t)	{ return force_cast<B>(t.getvb()); }
};
template<typename E, int N, int A, int BB, int C, typename Y, typename P> struct vget<_vbd<_v3<E,N,A,BB,C>,Y,P> > : vdims<3> {
	typedef _vbd<_v3<E,N,A,BB,C>,Y,P>	T;
	typedef _v3<bool,N,A,BB,C>			B;
	static force_inline B b(const T &t)	{ return force_cast<B>(t.getvb()); }
};
template<typename E, int A, int BB, int C, int D, typename Y, typename P> struct vget<_vbd<_v4<E,A,BB,C,D>,Y,P> > : vdims<4> {
	typedef _vbd<_v4<E,A,BB,C,D>,Y,P>	T;
	typedef _v4<bool,A,BB,C,D>			B;
	static force_inline B b(const T &t)	{ return force_cast<B>(t.getvb()); }
};

// typed vectors
template<typename E, int N, int A> struct vget<_v1<E,N,A> > : vdims<1> {
	typedef _v1<E,N,A> T;
	typedef _v1<E,0,A> R, L, O;
	static force_inline R	r(const T &t)	{ return force_cast<L>(t); }
	static force_inline L	l(const T &t)	{ return force_cast<L>(t); }
};

template<typename E, int N, int A, int B, bool DUPES> struct vget_v2 {
	typedef _v2<E,0,A,B> L, O;
	static force_inline L	l(const _v2<E,N,A,B> &t)	{ return force_cast<L>(t); }
};
template<typename E, int N, int A, int B> struct vget_v2<E,N,A,B,true> {
	typedef	_v2<E,0,0,1> L, O;
	static force_inline L	l(const _v2<E,N,A,B> &t)	{ return force_cast<L>(L::align(t)); }
};
template<typename E, int N, int A, int B> struct vget<_v2<E,N,A,B> > : vget_v2<E,N,A,B,A==B>, vdims<2> {
	typedef _v2<E,0,A,B> R;
	static force_inline R	r(const _v2<E,N,A,B> &t)	{ return force_cast<R>(t); }
};

template<typename E, int N, int A, int B, int C, bool DUPES> struct vget_v3 {
	typedef _v3<E,0,A,B,C> L, O;
	static force_inline L	l(const _v3<E,N,A,B,C> &t)	{ return force_cast<L>(t); }
};
template<typename E, int N, int A, int B, int C> struct vget_v3<E,N,A,B,C,true> {
	typedef	_v3<E,0,0,1,2> L, O;
	static force_inline L	l(const _v3<E,N,A,B,C> &t)	{ return force_cast<L>(L::align(t)); }
};
template<typename E, int N, int A, int B, int C> struct vget<_v3<E,N,A,B,C> > : vget_v3<E,N,A,B,C,A==B||A==C||B==C>, vdims<3> {
	typedef _v3<E,0,A,B,C> R;
	static force_inline R	r(const _v3<E,N,A,B,C> &t)	{ return force_cast<R>(t); }
};

template<typename E, int A, int B, int C, int D, bool DUPES> struct vget_v4 {
	typedef	_v4<E,A,B,C,D> L, O;
	static force_inline L	l(const _v4<E,A,B,C,D> &t)	{ return t; }
};
template<typename E, int A, int B, int C, int D> struct vget_v4<E,A,B,C,D,true> {
	typedef	_v4<E,0,1,2,3> L, O;
	static force_inline L	l(const _v4<E,A,B,C,D> &t)	{ return force_cast<L>(L::align(t)); }
};
template<typename E, int A, int B, int C, int D> struct vget<_v4<E,A,B,C,D> > : vget_v4<E,A,B,C,D,A==B||A==C||A==D||B==C||B==D||C==D>, vdims<4> {
	typedef _v4<E,A,B,C,D>	R;
	static force_inline R	r(const R &t)	{ return t; }
};

// paired vectors
template<typename E, int A, int B> struct vget<_v2p<E,A,B> > : vdims<2> {
	typedef _v2p<E,A,B> 	R;
	typedef _v2<E,0,0,1>	L, O;
	static force_inline R	r(const R &t)	{ return t; }
	static force_inline L	l(const R &t)	{ return force_cast<L>(swizzle<typename L::storage,A,B,-1,-1>(t.q1, t.q2)); }
};
template<typename E, int A, int B, int C> struct vget<_v3p<E,A,B,C> > : vdims<3> {
	typedef _v3p<E,A,B,C>	R;
	typedef _v3<E,0,0,1,2>	L, O;
	static force_inline R	r(const R &t)	{ return t; }
	static force_inline L	l(const R &t)	{ return force_cast<L>(swizzle<typename L::storage,A,B,C,-1>(t.q1, t.q2)); }
};
template<typename E, int A, int B, int C, int D> struct vget<_v4p<E,A,B,C,D> > : vdims<4> {
	typedef _v4p<E,A,B,C,D> R;
	typedef _v4<E,0,1,2,3>	L, O;
	static force_inline R	r(const R &t)	{ return t; }
	static force_inline L	l(const R &t)	{ return force_cast<L>(swizzle<typename L::storage,A,B,C,D>(t.q1, t.q2)); }
};

// pairs
template<typename A, typename B, typename V = void>	struct _vgetpair {};
template<typename A, typename B>	struct _vgetpair<A, B, void_t<typename vgetl<A>::L, typename vgetl<B>::L>> : vgetpair<VLTYPE1(A), VLTYPE1(B)> {
//template<typename A, typename B>	struct _vgetpair<A, B, same_t<A, uint32, void>> : vgetpair<VLTYPE1(A), VLTYPE1(B)> {
	typedef vgetpair<VLTYPE1(A), VLTYPE1(B)>	BASE;
	using typename BASE::L;
	using typename BASE::R;
	static force_inline R	r(const pair<A, B> &p)	{ return R(vgetl<A>::l(p.a).q, vgetl<B>::l(p.b).q); }
	static force_inline L	l(const pair<A, B> &p)	{ return vget<R>::l(R(vgetl<A>::l(p.a).q, vgetl<B>::l(p.b).q)); }
};

template<typename A, typename B> struct vget<pair<A, B> > : _vgetpair<A, B> {};

template<> struct vget<pair<float, float> > : vdims<2> {
	typedef	_v2<float,0,0,1>		L, R, O;
	static force_inline R	r(const pair<float, float> &p)							{ return force_cast<R>(vload<2>(&p.a)); }
	static force_inline L	l(const pair<float, float> &p)							{ return r(p); }
};
template<> struct vget<pair<pair<float, float>, float> > : vdims<3> {
	typedef	_v3<float,0,0,1,2>	L, R, O;
	static force_inline R	r(const pair<pair<float, float>, float> &p)				{ return force_cast<R>(vload<3>(&p.a.a)); }
	static force_inline L	l(const pair<pair<float, float>, float> &p)				{ return r(p); }
};
template<> struct vget<pair<pair<pair<float, float>, float>, float> > : vdims<4> {
	typedef	_v4<float,0,1,2,3>	L, R, O;
	static force_inline R	r(const pair<pair<pair<float, float>, float>, float> &p)	{ return force_cast<R>(vload<4>(&p.a.a.a)); }
	static force_inline L	l(const pair<pair<pair<float, float>, float>, float> &p)	{ return r(p); }
};

template<> struct vget<pair<int, int> > : vdims<2> {
	typedef	_v2<int,0,0,1>		L, R, O;
	static force_inline R	r(const pair<int, int> &p)							{ return force_cast<R>(vload<2>(&p.a)); }
	static force_inline L	l(const pair<int, int> &p)							{ return r(p); }
};
template<> struct vget<pair<pair<int, int>, int> > : vdims<3> {
	typedef	_v3<int,0,0,1,2>	L, R, O;
	static force_inline R	r(const pair<pair<int, int>, int> &p)				{ return force_cast<R>(vload<3>(&p.a.a)); }
	static force_inline L	l(const pair<pair<int, int>, int> &p)				{ return r(p); }
};
template<> struct vget<pair<pair<pair<int, int>, int>, int> > : vdims<4> {
	typedef	_v4<int,0,1,2,3>	L, R, O;
	static force_inline R	r(const pair<pair<pair<int, int>, int>, int> &p)	{ return force_cast<R>(vload<4>(&p.a.a.a)); }
	static force_inline L	l(const pair<pair<pair<int, int>, int>, int> &p)	{ return r(p); }
};

template<typename T, int N> struct vget<aligned<T,N> > : public vget<T> {};

template<> struct vget<float4_const> {
	typedef vf4	R, L, O;
	static force_inline R r(const float4_const &t)	{ return t; }
	static force_inline L l(const float4_const &t)	{ return t; }
};

template<> struct vget<vf4> {
	typedef vf4	R, L, O;
	static force_inline R r(param_t<vf4> t)	{ return t; }
	static force_inline L l(param_t<vf4> t)	{ return t; }
};

// constants
//template<typename K> struct vget<constant0<K> > : vdims<0>	{
//	typedef vf4				R;
//	typedef	_v1<float,0,0>	L, O;
//	static force_inline R r(const constant0<K> &t)	{ return constant_cast<K, vf4>::f(); }
//	static force_inline L l(const constant0<K> &t)	{ return force_cast<L>(r(t)); }
//};
template<typename K> struct vget<constant<K> > : vdims<0>	{
	typedef vf4				R;
	typedef	_v1<float,0,0>	L, O;
	static force_inline R r(const constant<K> &t)	{ return constant_cast<K, vf4>::f(); }
	static force_inline L l(const constant<K> &t)	{ return force_cast<L>(r(t)); }
};

//-----------------------------------------------------------------------------
//	component rearrangement
//-----------------------------------------------------------------------------
//perms
//4
template<int X, int Y, int Z, int W, typename T> struct PERM4 {
	typedef _v4<typename vgetetype<T>::type, vgetindex<X, T>::value, vgetindex<Y, T>::value, vgetindex<Z, T>::value, vgetindex<W, T>::value> R;
};
template<int X, int Y, int Z, int W, typename T> force_inline typename PERM4<X,Y,Z,W,VLTYPE(T)>::R perm(const T &t) {
	return force_cast<typename PERM4<X,Y,Z,W,VLTYPE(T)>::R>(vget<T>::l(t));
}

template<int X, int Y, int Z, int W, typename A, typename B> struct PERM4<X,Y,Z,W, pair<A,B> > {
	typedef typename vgetetype<A, B>::type	E;
	typedef	_v4<E, 0,1,2,3>					R;

	static R	get(const A &a, const B &b)	{
		return force_cast<R>(R::align(_v4p<E,
			vgetindex2<X, A, B>::value,
			vgetindex2<Y, A, B>::value,
			vgetindex2<Z, A, B>::value,
			vgetindex2<W, A, B>::value
		>(vgetl<A>::l(a).q, vgetl<B>::l(b).q)));
	}
	static R	get(const pair<A,B> &p)	{ return get(p.a, p.b); }
};
template<int X, int Y, int Z, int W, typename A, typename B> force_inline typename PERM4<X,Y,Z,W, pair<A,B> >::R perm(const A &a, const B &b) {
	return PERM4<X,Y,Z,W,pair<A,B> >::get(a, b);
}

template<int X, int Y, int Z, int W, typename A, typename B, typename C> struct PERM4<X,Y,Z,W, pair<pair<A,B>, C> > {
	enum	{ BD = max(vget<A>::dims, 1) + max(vget<B>::dims, 1) };
	typedef	_v4<typename vgetetype<A, B, C>::type, 0,1,2,3>	R;

	static R	get(const A &a, const B &b, const C &c)	{
		auto	t1 = perm<X < BD ? X : -1, Y < BD ? Y : -1, Z < BD ? Z : -1, W < BD ? W : -1>(a, b);
		return perm<X < BD ? 0 : X + 4 - BD, Y < BD ? 1 : Y + 4 - BD, Z < BD ? 2 : Z + 4 - BD, W < BD ? 3 : W + 4 - BD>(t1, c);
	};
	static R	get(const pair<A,B> &p, const C &c)	{ return get(p.a, p.b, c); };
	static R	get(const pair<pair<A,B>,C> &p)		{ return get(p.a.a, p.a.b, p.b); };
};
template<int X, int Y, int Z, int W, typename A, typename B, typename C> force_inline typename PERM4<X,Y,Z,W,pair<pair<A,B>,C> >::R perm(const A &a, const B &b, const C &c) {
	return PERM4<X,Y,Z,W, pair<pair<A,B>,C> >::get(a, b, c);
}

template<int X, int Y, int Z, int W, typename A, typename B, typename C, typename D> struct PERM4<X,Y,Z,W, pair<pair<pair<A,B>, C>, D> > {
	enum	{ BD = max(vget<A>::dims, 1) + max(vget<B>::dims, 1), CD = BD + max(vget<C>::dims, 1) };
	typedef	_v4<typename vgetetype<A, B, C, D>::type, 0,1,2,3>	R;

	static R	get(const A &a, const B &b, const C &c, const D &d)	{
		auto	t1 = perm<X < BD ? X : -1, Y < BD ? Y : -1, Z < BD ? Z : -1>(a, b);
		auto	t2 = perm<X < BD ? 0 : X < CD ? X + 4 - BD : -1, Y < BD ? 1 : Y < CD ? Y + 4 - BD : -1, Z < BD ? 2 : Z < CD ? Z + 4 - BD : -1, W < BD ? 3 : W < CD ? W + 4 - BD : -1>(t1, c);
		return perm<X < CD ? 0 : X + 4 - CD, Y < CD ? 1 : Y + 4 - CD, Z < CD ? 2 : Z + 4 - CD, W < CD ? 3 : W + 4 - CD>(t2, d);
	};
	static R	get(const pair<A,B> &p, const C &c, const D &d)	{ return get(p.a, p.b, c, d); };
	static R	get(const pair<pair<A,B>,C> &p,  const D &d)	{ return get(p.a.a, p.a.b, p.b, d); };
	static R	get(const pair<pair<pair<A,B>,C>,D> &p)			{ return get(p.a.a.a, p.a.a.b, p.a.b, p.b); };
};
template<int X, int Y, int Z, int W, typename A, typename B, typename C, typename D> force_inline typename PERM4<X,Y,Z,W,typename vget<pair<pair<pair<A,B>,C>,D> >::L>::R perm(const A &a, const B &b, const C &c, const D &d) {
	return PERM4<X,Y,Z,W, pair<pair<pair<A,B>,C>,D> >::get(a, b, c);
}

//3
template<int X, int Y, int Z, typename T> struct PERM3 {
	typedef _v3<typename vgetetype<T>::type, 0, vgetindex<X, T>::value, vgetindex<Y, T>::value, vgetindex<Z, T>::value> R;
};
template<int X, int Y, int Z, typename T> force_inline typename PERM3<X,Y,Z,VLTYPE(T)>::R perm(const T &t) {
	return force_cast<typename PERM3<X,Y,Z,VLTYPE(T)>::R>(vget<T>::l(t));
}

template<int X, int Y, int Z, typename A, typename B> struct PERM3<X,Y,Z,pair<A,B> > {
	typedef typename vgetetype<A, B>::type	E;
	typedef	_v3<E, 0, 0,1,2>				R;

	static R	get(const A &a, const B &b)	{
		return force_cast<R>(R::align(_v3p<E,
			vgetindex2<X, A, B>::value,
			vgetindex2<Y, A, B>::value,
			vgetindex2<Z, A, B>::value
		>(vgetl<A>::l(a).q, vgetl<B>::l(b).q)));
	}
	static R	get(const pair<A,B> &p)	{ return get(p.a, p.b); }
};
template<int X, int Y, int Z, typename A, typename B> force_inline typename PERM3<X,Y,Z,pair<A,B> >::R perm(const A &a, const B &b) {
	return PERM3<X,Y,Z,pair<A,B> >::get(a, b);
}

template<int X, int Y, int Z, typename A, typename B, typename C> struct PERM3<X,Y,Z,pair<pair<A,B>, C> > {
	enum	{ BD = max(vget<A>::dims, 1) + max(vget<B>::dims, 1) };
	typedef	_v3<typename vgetetype<A, B, C>::type, 0, 0,1,2>	R;

	static R	get(const A &a, const B &b, const C &c)	{
		auto	t1 = perm<X < BD ? X : -1, Y < BD ? Y : -1, Z < BD ? Z : -1>(a, b);
		return perm<X < BD ? 0 : X + 3 - BD, Y < BD ? 1 : Y + 3 - BD, Z < BD ? 2 : Z + 3 - BD>(t1, c);
	};
	static R	get(const pair<A,B> &p, const C &c)	{ return get(p.a, p.b, c); };
	static R	get(const pair<pair<A,B>,C> &p)		{ return get(p.a.a, p.a.b, p.b); };
};
template<int X, int Y, int Z, typename A, typename B, typename C> force_inline typename PERM3<X,Y,Z,pair<pair<A,B>,C> >::R perm(const A &a, const B &b, const C &c) {
	return PERM3<X,Y,Z,pair<pair<A,B>,C> >::get(a, b, c);
}

//2
template<int X, int Y, typename T> struct PERM2 {
	typedef _v2<typename vgetetype<T>::type, 0, vgetindex<X, T>::value, vgetindex<Y, T>::value> R;
};
template<int X, int Y, typename T> force_inline typename PERM2<X,Y,VLTYPE(T)>::R perm(const T &t) {
	return force_cast<typename PERM2<X,Y,VLTYPE(T)>::R>(vget<T>::l(t));
}

template<int X, int Y, typename A, typename B> struct PERM2<X,Y,pair<A,B> > {
	typedef typename vgetetype<A, B>::type	E;
	typedef	_v2<E, 0, 0,1>					R;

	static R	get(const A &a, const B &b)	{
		return force_cast<R>(R::align(_v2p<E,
			vgetindex2<X, A, B>::value,
			vgetindex2<Y, A, B>::value
		>(vgetl<A>::l(a).q, vgetl<B>::l(b).q)));
	}
};
template<int X, int Y, typename A, typename B> force_inline typename PERM2<X,Y,pair<A,B> >::R perm(const A &a, const B &b) {
	return PERM2<X,Y,pair<A,B> >::get(a, b);
}

//1
template<int X, typename T> struct PERM1 {
	typedef _v1<typename vgetetype<T>::type, 0, vgetindex<X, T>::value> R;
};

template<int X, typename T> force_inline typename PERM1<X,VLTYPE(T)>::R perm(const T &t) {
	return force_cast<typename PERM1<X,VLTYPE(T)>::R>(vget<T>::l(t));
}

//shifts
template<int S, typename X> struct SHIFT;
template<int S, typename E, int N, int A, int B> struct SHIFT<S, _v2<E,N,A,B> > {
	static const int T = S & 1;
	typedef	_v2<E,0,(T==0?A:B), (T==0?B:A)> R;
};
template<int S, typename E, int N, int A, int B, int C> struct SHIFT<S, _v3<E,N,A,B,C> > {
	static const int T = (S < 0 ? 3 - (-S % 3) : S) % 3;
	typedef	_v3<E,0,(T==0?A:T==1?B:C), (T==0?B:T==1?C:A), (T==0?C:T==1?A:B)> R;
};
template<int S, typename E, int A, int B, int C, int D> struct SHIFT<S, _v4<E,A,B,C,D> > {
	static const int T = S & 3;
	typedef	_v4<E,(T==0?A:T==1?B:T==2?C:D), (T==0?B:T==1?C:T==2?D:A), (T==0?C:T==1?D:T==2?A:B), (T==0?D:T==1?A:T==2?B:C)>	R;
};
template<int S, typename X> force_inline typename SHIFT<S, VLTYPE(X)>::R shift(const X &x) {
	return force_cast<typename SHIFT<S, VLTYPE(X)>::R>(vget<X>::l(x));
}

template<typename E, int N, int A, int B> _v2<E,0,A,B> force_inline _shift(const _v2<E,N,A,B> &v, int S) {
	return select((S & 1) == 0, v, shift<1>(v));
};
template<typename E, int N, int A, int B, int C> _v3<E,0,A,B,C> force_inline _shift(const _v3<E,N,A,B,C> &v, int S) {
	int T = (S % 3 + 3) % 3;
	return select(T == 0, shift<0>(v), select(T == 1, shift<1>(v), shift<2>(v)));
};
template<typename E, int A, int B, int C, int D> _v4<E,A,B,C,D> force_inline _shift(const _v4<E,A,B,C,D> &v, int S) {
	_v4<E,A,B,C,D> t = select((S & 2) == 0, v, shift<2>(v));
	return select((S & 1) == 0, t, shift<1>(t));
};

template<typename X> force_inline VLTYPE(X) operator<<(const X &x, int S) { return _shift(vget<X>::l(x), +S); }
template<typename X> force_inline VLTYPE(X) operator>>(const X &x, int S) { return _shift(vget<X>::l(x), -S); }

//-----------------------------------------------------------------------------
//	arithmetic
//-----------------------------------------------------------------------------

//unary functions/operators
template<typename X> 				force_inline VLTYPE(X)	operator-	(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(neg		(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	reciprocal	(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(reciprocal(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	abs			(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(abs		(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	rsqrt		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(rsqrt	(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	sqrt		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(sqrt	(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	floor		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(floor	(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	ceil		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(ceil	(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	trunc		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(trunc	(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	frac		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(frac	(vget<X>::l(x).q)); }
template<typename X>				force_inline VLTYPE(X)	sign1		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(vsel	((typename L::storage)one, vget<X>::l(x).q, (typename L::storage)sign_mask)); }
template<typename X>				force_inline VLTYPE(X)	sign		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(vsel	(vand(vcmp<not_equal_to>(vget<X>::l(x).q, zero), (typename L::storage)one), vget<X>::l(x).q, (typename L::storage)sign_mask)); }
template<typename X>				force_inline VLTYPE(X)	log2		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(log2	(vget<X>::l(x).q)); }
template<typename X>				force_inline VLTYPE(X)	exp2		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(exp2	(vget<X>::l(x).q)); }
template<typename X>				force_inline VLTYPE(X)	ln			(const X &x)				{ return log2(x) * 0.6931471806f; }
template<typename X>				force_inline VLTYPE(X)	exp			(const X &x)				{ return exp2(x * 1.442695041f); }

template<typename X> 				force_inline typename T_enable_if<VHASL(X),X>::type&	operator++(X &x)		{ return x += one; }
template<typename X> 				force_inline typename T_enable_if<VHASL(X),X>::type&	operator--(X &x)		{ return x -= one; }
template<typename X> 				force_inline typename T_enable_if<VHASL(X),X>::type	operator++(X &x,int)	{ X t = x; x += one; return t; }
template<typename X> 				force_inline typename T_enable_if<VHASL(X),X>::type	operator--(X &x,int)	{ X t = x; x -= one; return t; }

//binary functions/operators
template<typename X, typename Y>	force_inline VLTYPE0(X,Y)operator+	(const X &x, const Y &y)	{ typedef VLTYPE(X) L; return force_cast<L>(add	(vget<X>::l(x).q, L::align(y))); }
template<typename X, typename Y>	force_inline VLTYPE0(X,Y)operator-	(const X &x, const Y &y)	{ typedef VLTYPE(X) L; return force_cast<L>(sub	(vget<X>::l(x).q, L::align(y))); }
template<typename X, typename Y>	force_inline VLTYPE2(X,Y)operator/	(const X &x, const Y &y)	{ typedef VLTYPE2(X,Y) L; return force_cast<L>(div	(L::align(x), L::kalign(y))); }
template<typename X, typename Y>	force_inline VLTYPE0(X,Y)mod			(const X &x, const Y &y)	{ typedef VLTYPE(X) L; return force_cast<L>(mod	(vget<X>::l(x).q, L::align(y))); }
template<typename X, typename Y>	force_inline typename T_enable_if<VHASL(X), X>::type min(const X &x, const Y &y)	{ typedef VLTYPE(X) L; X r; r = force_cast<L>(min(vget<X>::l(x).q, L::align(y))); return r; }
template<typename X, typename Y>	force_inline typename T_enable_if<VHASL(X), X>::type max(const X &x, const Y &y)	{ typedef VLTYPE(X) L; X r; r = force_cast<L>(max(vget<X>::l(x).q, L::align(y))); return r; }
template<typename X, typename Y>	force_inline VLTYPE0(X,Y)copysign	(const X &x, const Y &y)	{ typedef VLTYPE(X) L; return force_cast<L>(vsel(vget<X>::l(x).q, L::align(y), (typename L::storage)sign_mask)); }
template<typename X, typename Y>	force_inline VLTYPE0(X,Y)pow			(const X &x, const Y &y)	{ typedef VLTYPE(X) L; return force_cast<L>(pow	(vget<X>::l(x).q, L::align(y))); }

// switch operands for scalar * vector
template<typename X, typename Y>	force_inline typename T_enable_if<VHASR(Y) && (!IS_SCALAR(X) || IS_SCALAR(Y)), VLTYPE(X)>::type	operator*(const X &x, const Y &y) {
	typedef VLTYPE(X) L; return force_cast<L>(mul	(vget<X>::l(x).q, L::align(y)));
}
template<typename X, typename Y>	force_inline typename T_enable_if<VHASR(Y) && IS_SCALAR(X) && !IS_SCALAR(Y), VLTYPE(Y)>::type	operator*(const X &x, const Y &y) {
	typedef VLTYPE(Y) L; return force_cast<L>(mul	(vget<Y>::l(y).q, L::align(x)));
}

//	boolean
template<typename X, typename Y>	force_inline VBTYPE(X)	operator~	(const X &x)				{ typedef VBTYPE(X) B; return force_cast<B>(vnot(vget<X>::b(x).q)); }
template<typename X, typename Y>	force_inline VBTYPE(X)	operator&	(const X &x, const Y &y)	{ typedef VBTYPE(X) B; return force_cast<B>(vand(vget<X>::b(x).q, B::align(y))); }
template<typename X, typename Y>	force_inline VBTYPE(X)	operator|	(const X &x, const Y &y)	{ typedef VBTYPE(X) B; return force_cast<B>(vor	(vget<X>::b(x).q, B::align(y))); }
template<typename X, typename Y>	force_inline VBTYPE(X)	operator^	(const X &x, const Y &y)	{ typedef VBTYPE(X) B; return force_cast<B>(vxor(vget<X>::b(x).q, B::align(y))); }

//	trigonometry
template<typename X> 				force_inline VLTYPE(X)	sin			(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(sin(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	cos			(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(cos(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	tan			(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(tan(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	asin		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(asin(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	acos		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(acos(vget<X>::l(x).q)); }
template<typename X> 				force_inline VLTYPE(X)	atan		(const X &x)				{ typedef VLTYPE(X) L; return force_cast<L>(atan(vget<X>::l(x).q)); }
template<typename X, typename Y>	force_inline VLTYPE(Y)	atan2		(const Y &y, const X &x)	{ typedef VLTYPE(Y) L; return force_cast<L>(atan2(vget<Y>::l(y).q, L::align(x))); }

template<typename T1, typename T2> force_inline	void		sincos(const T1 &a, T2 *s, T2 *c)	{ vf4 _s, _c; _sincos(vget<T1>::l(a).q, &_s, &_c); *s = force_cast<T2>(_s); *c = force_cast<T2>(_c); }
template<typename T> force_inline	_v2<float,0,0,1>		sincos(const T &t)					{ return sin(make_pair(t + pi * half, t)); }
template<typename E,int N, int A, int B>	force_inline _v1<float,0,A>	atan2(const _v2<E,N,A,B> &x)	{ return atan2(perm<0>(x), perm<1>(x)); }

//	comparisons
#define VEC_COND_RET(X,Y,P) typename T_enable_if<VHASL(X), _vbd<VLTYPE(X),Y,P> >::type	// seems to be necessary for MS compilers - or it matches everything!)
template<typename X, typename Y>	force_inline VEC_COND_RET(X,Y,equal_to		) operator==(const X &x, const Y &y) { return _vbd<VLTYPE(X),Y,equal_to>		(vget<X>::l(x), y); }
template<typename X, typename Y>	force_inline VEC_COND_RET(X,Y,not_equal_to	) operator!=(const X &x, const Y &y) { return _vbd<VLTYPE(X),Y,not_equal_to>	(vget<X>::l(x), y); }
template<typename X, typename Y>	force_inline VEC_COND_RET(X,Y,less_equal	) operator<=(const X &x, const Y &y) { return _vbd<VLTYPE(X),Y,less_equal>		(vget<X>::l(x), y); }
template<typename X, typename Y>	force_inline VEC_COND_RET(X,Y,greater		) operator> (const X &x, const Y &y) { return _vbd<VLTYPE(X),Y,greater>			(vget<X>::l(x), y); }
template<typename X, typename Y>	force_inline VEC_COND_RET(X,Y,less			) operator< (const X &x, const Y &y) { return _vbd<VLTYPE(X),Y,less>			(vget<X>::l(x), y); }
template<typename X, typename Y>	force_inline VEC_COND_RET(X,Y,greater_equal	) operator>=(const X &x, const Y &y) { return _vbd<VLTYPE(X),Y,greater_equal>	(vget<X>::l(x), y); }
#undef VEC_COND_RET

//cross-wise functions
template<typename X> struct across {
	typedef VLTYPE(X) L;
	typedef typename across<L>::R R;
	typedef typename across<L>::DOT DOT;
	template<class OP> static force_inline R f(OP op, const X &x)	{ return across<L>::f(op, vget<X>::l(x)); }
};
template<typename E, int N, int A, int B>		struct across<_v2<E,N,A,B> >	{
	typedef _v1<E,0,A> R;
	typedef _v1<E,0, min(A, B)> DOT;
	template<class OP> static force_inline R f(OP op, const _v2<E,N,A,B>		&x)	{ return op(perm<0>(x), perm<1>(x)); }
};
template<typename E, int N, int A, int B, int C>struct across<_v3<E,N,A,B,C> >	{
	typedef _v1<E,0,A> R;
	typedef _v1<E,0, min(min(A, B), C)> DOT;
	template<class OP> static force_inline R f(OP op, const _v3<E,N,A,B,C>	&x)	{ return op(op(perm<0>(x), perm<1>(x)), perm<2>(x)); }
};
template<typename E, int A, int B, int C, int D>struct across<_v4<E,A,B,C,D> >	{
	typedef _v1<E,0,A> R;
	typedef _v1<E,0, min(min(A, B), min(C, D))> DOT;
	template<class OP> static force_inline R f(OP op, const _v4<E,A,B,C,D>	&x)	{ _v2<E,0,A,B> t = op(perm<0,1>(x), perm<2,3>(x)); return op(perm<0>(t), perm<1>(t)); }
};

template<typename E, int N, int A, int B> 	force_inline _v1<E,0,A>	diff(const _v2<E,N,A,B> &x)		{ return perm<0>(x) - perm<1>(x); }

template<typename X> 	force_inline typename across<X>::R	sum_components(const X &x)				{ return across<X>::f(op_add(), x); }
template<typename X> 	force_inline typename across<X>::R	prod_components(const X &x)				{ return across<X>::f(op_mul(), x); }
template<typename X> 	force_inline typename across<X>::R	min_component(const X &x)				{ return across<X>::f(op_min(), x); }
template<typename X> 	force_inline typename across<X>::R	max_component(const X &x)				{ return across<X>::f(op_max(), x); }

template<typename E,int N, int A, int B>		force_inline int	_min_component_index(const _v2<E,N,A,B>		&x)	{ return int(perm<1>(x) < perm<0>(x)); }
template<typename E,int N, int A, int B, int C>	force_inline int	_min_component_index(const _v3<E,N,A,B,C>	&x)	{ return (0x1a10 >> ((x < shift<1>(x)).mask() * 2)) & 3; }
template<typename E,int A, int B, int C, int D>	force_inline int	_min_component_index(const _v4<E,A,B,C,D>	&x)	{ return (0x1aff1a10 >> ((x <= shift<1>(x) & x <= shift<2>(x)).mask() * 2)) & 3; }
template<typename X> force_inline int	min_component_index(const X &x)		{ return _min_component_index(vget<X>::r(x)); }

template<typename E,int N, int A, int B>		force_inline int	_max_component_index(const _v2<E,N,A,B>		&x)	{ return int(perm<1>(x) > perm<0>(x)); }
template<typename E,int N, int A, int B, int C>	force_inline int	_max_component_index(const _v3<E,N,A,B,C>	&x)	{ return (0x1a10 >> ((x > shift<1>(x)).mask() * 2)) & 3; }
template<typename E,int A, int B, int C, int D>	force_inline int	_max_component_index(const _v4<E,A,B,C,D>	&x)	{ return (0x1aff1a10 >> ((x >= shift<1>(x) & x >= shift<2>(x)).mask() * 2)) & 3; }
template<typename X> force_inline int	max_component_index(const X &x)		{ return _max_component_index(vget<X>::r(x)); }

template<typename A, typename B> force_inline typename across<A>::R	min_component(const pair<A,B> &p)		{ return min_component(min(min_pairs(p.a), min_pairs(p.b))); }
template<typename A, typename B> force_inline typename across<A>::R	max_component(const pair<A,B> &p)		{ return max_component(max(max_pairs(p.a), max_pairs(p.b))); }

template<typename A>			 force_inline int	which_min_component_index(const A &a, int i)			{ return i; }
template<typename A, typename B> force_inline int	which_min_component_index(const pair<A,B> &p, int i)	{ return (min_pairs(p.b) < min_pairs(p.a)).mask(1 << i) ? which_min_component_index(p.b, i) + dims_s<A>::value : which_min_component_index(p.a, i); }
template<typename A, typename B> force_inline int	min_component_index(const pair<A,B> &p) {
	auto a = min_pairs(p.a);
	auto b = min_pairs(p.b);
	int i = min_component_index(min(a, b));
	return (b < a).mask(1 << i) ? which_min_component_index(p.b, i) + dims_s<A>::value : which_min_component_index(p.a, i);
}

template<typename A>			 force_inline int	which_max_component_index(const A &a, int i)			{ return i; }
template<typename A, typename B> force_inline int	which_max_component_index(const pair<A,B> &p, int i)	{ return (max_pairs(p.b) > max_pairs(p.a)).mask() & (1 << i) ? which_max_component_index(p.b, i) + dims_s<A>::value : which_max_component_index(p.a, i); }
template<typename A, typename B> force_inline int	max_component_index(const pair<A,B> &p) {
	auto a = max_pairs(p.a);
	auto b = max_pairs(p.b);
	int i = max_component_index(max(a, b));
	return (a > b).mask() & (1 << i) ? which_max_component_index(p.a, i) : which_max_component_index(p.b, i) + dims_s<A>::value;
}

//derived functions

#ifdef USE_VECDOT
template<typename X, typename Y>	force_inline typename T_enable_if<VHASL(X), typename across<VLTYPE(X)>::DOT>::type		dot(const X &x, const Y &y)	{
	typedef VLTYPE(X) L;
	return force_cast<typename across<VLTYPE(X)>::DOT>(vdot<L::FIELDMASK>(vget<X>::l(x).q, L::align(y)));
}
#else
template<typename X, typename Y>	force_inline typename T_enable_if<VHASL(X), typename across<VLTYPE(X)>::R>::type			dot(const X &x, const Y &y)	{
	return sum_components(x * y);
}
#endif

template<typename X> 				force_inline auto				len2(const X &x)->decltype(dot(x,x))	{ return dot(x, x); }
template<typename X> 				force_inline auto				len	(const X &x)->decltype(len2(x))		{ return sqrt(len2(x)); }
template<typename X> 				force_inline auto				rlen(const X &x)->decltype(len2(x))		{ return rsqrt(len2(x)); }
template<typename X, typename Y>	force_inline auto				dist2(const X &x, const Y &y)->decltype(len2(x-y))	{ return len2(x - y); }
template<typename X, typename Y>	force_inline auto				dist(const X &x, const Y &y)->decltype(len(x-y))	{ return len(x - y); }

template<int D0, int D1> struct cross_s;
template<> struct cross_s<2, 2> { template<typename A, typename B> static auto f(A a, B b) { return diff(a * perm<1,0>(b)); } };
template<> struct cross_s<1, 2> { template<typename A, typename B> static auto f(A a, B b) { return perm<0,1>(perm<1>(b), -perm<0>(b)) * a; } };
template<> struct cross_s<2, 1> { template<typename A, typename B> static auto f(A a, B b) { return perm<0,1>(-perm<1>(a), perm<0>(b)) * a; } };
template<> struct cross_s<3, 3> { template<typename A, typename B> static auto f(A a, B b) { return perm<1,2,0>(a) * perm<2,0,1>(b) - perm<1,2,0>(b) * perm<2,0,1>(a); } };

template<typename A, typename B> 	force_inline auto 	cross(A a, B b) { return cross_s<vget<A>::dims,vget<A>::dims>::f(a, b); }

template<typename X> 				force_inline VLTYPE(X)			normalise		(const X &x)				{ return x * rlen(x); }
template<typename X, typename Y>	force_inline VLTYPE(Y)			project			(const X &x, const Y &y)	{ return y * (dot(x, y) / dot(y, y)); }
template<typename X, typename Y>	force_inline VLTYPE(Y)			reflect			(const X &x, const Y &y)	{ return project(x, y) * 2 - x; }
template<typename X, typename Y>	force_inline VLTYPE(X)			orthogonalise	(const X &x, const Y &y)	{ return x - project(x, y); }
template<typename X> 				force_inline VLTYPE(X)			safe_normalise	(const X &x)				{ typename across<X>::R y = len2(x); return y == zero ? force_cast<VLTYPE(X)>(x) : x * rsqrt(y); }

template<typename E, int A, int B, int C, int D>force_inline _v3<E,0,A,B,C>	project	(const _v4<E,A,B,C,D> &x)	{ return perm<0,1,2>(x) / perm<3>(x); }
template<typename E, int N, int A, int B, int C>force_inline _v2<E,0,A,B>	project	(const _v3<E,N,A,B,C> &x)	{ return perm<0,1>(x) / perm<2>(x); }

template<typename E, int M, int A, int B> inline _v2<E,0,0,1> sort(_v2<E,M,A,B> v) {
	auto	t = perm<1,0>(v);
	return perm<0, 2>(min(v, t), max(t, v));
}

template<typename E, int N, int A, int B, int C> _v3<E,0,0,1,2> sort(_v3<E,N,A,B,C> v) {
	auto	x		= perm<0>(v);
	auto	y		= perm<1>(v);
	auto	minxy	= min(x, y);
	auto	maxxy	= max(x, y);
	auto	z		= perm<2>(v);
	return perm<0,1,2>(min(minxy, z), max(minxy, min(maxxy, z)), max(maxxy, z));
}

template<typename E, int A, int B, int C, int D> inline _v4<E,0,1,2,3> sort(_v4<E,A,B,C,D> v) {
	auto	a0		= perm<0,1>(v);
	auto	b0		= perm<2,3>(v);
	auto	min0	= min(a0, b0);
	auto	max0	= max(a0, b0);

	auto	a1		= perm<0,2>(min0, max0);
	auto	b1		= perm<1,3>(min0, max0);
	auto	min1	= min(a1, b1);
	auto	max1	= max(a1, b1);
	auto	t		= perm<0,1,2,3>(min1, max1);
	auto	mid		= sort(perm<1,2>(t));

	return perm<0,4,5,3>(t, mid);
}

//-----------------------------------------------------------------------------
//	vec1
//-----------------------------------------------------------------------------

template<typename E> class VEC_ATTR0 vec<E,1> {
public:
	union {
		// LHS + RHS
		_v1<E, 1,0>					x;
		// RHS only
		VEC_CONST _v2<E,0,0,0>		xx;
		VEC_CONST _v3<E,0,0,0,0>	xxx;
		VEC_CONST _v4<E,0,0,0,0>	xxxx;
	};
	vec()	{}
	template<typename T> force_inline vec(const T &b)				{ x = b; }
	template<typename T> force_inline vec& operator =(const T &b)	{ x  = b; return *this; }
	template<typename T> force_inline vec& operator+=(const T &b)	{ x += b; return *this; }
	template<typename T> force_inline vec& operator-=(const T &b)	{ x -= b; return *this; }
	template<typename T> force_inline vec& operator*=(const T &b)	{ x *= b; return *this; }
	template<typename T> force_inline vec& operator/=(const T &b)	{ x /= b; return *this; }
	operator E()	{ return x; }

	force_inline vec&	operator++()			{ return *this += one; }
	force_inline vec		operator++(int)			{ vec t = *this; operator++(); return t; }
	force_inline vec&	operator--()			{ return *this -= one; }
	force_inline vec		operator--(int)			{ vec t = *this; operator--(); return t; }
	force_inline vec&	operator=(const vec &b)	{ x  = b.x; return *this; }
} VEC_ATTR;

template<typename E> struct vget<vec<E,1> > : vdims<1> {
	typedef _v1<E,0,0>	R, L;
	typedef	vec<E,1>	O;
	static force_inline R r(const O &t)	{ return force_cast<R>(t); }
	static force_inline L l(const O &t)	{ return force_cast<L>(t); }
};

//-----------------------------------------------------------------------------
//	vec2
//-----------------------------------------------------------------------------

template<typename E> class VEC_ATTR0 vec<E,2> {
public:
	union {
		_v2<E,3,0,1>	v;
		// LHS + RHS
		_v1<E,3,0>		x;
		_v1<E,3,1>		y;

		_v2<E,3,0,1>	xy;
		_v2<E,3,1,0>	yx;

		// RHS only
		VEC_CONST _v2<E,0,0,0>		xx;
		VEC_CONST _v2<E,0,1,1>		yy;

		VEC_CONST _v3<E,0,0,0,0>	xxx;
		VEC_CONST _v3<E,0,0,0,1>	xxy;
		VEC_CONST _v3<E,0,0,1,0>	xyx;
		VEC_CONST _v3<E,0,0,1,1>	xyy;
		VEC_CONST _v3<E,0,1,0,0>	yxx;
		VEC_CONST _v3<E,0,1,0,1>	yxy;
		VEC_CONST _v3<E,0,1,1,0>	yyx;
		VEC_CONST _v3<E,0,1,1,1>	yyy;

		VEC_CONST _v4<E,0,0,0,0>	xxxx;
		VEC_CONST _v4<E,0,0,0,1>	xxxy;
		VEC_CONST _v4<E,0,0,1,0>	xxyx;
		VEC_CONST _v4<E,0,0,1,1>	xxyy;
		VEC_CONST _v4<E,0,1,0,0>	xyxx;
		VEC_CONST _v4<E,0,1,0,1>	xyxy;
		VEC_CONST _v4<E,0,1,1,0>	xyyx;
		VEC_CONST _v4<E,0,1,1,1>	xyyy;
		VEC_CONST _v4<E,1,0,0,0>	yxxx;
		VEC_CONST _v4<E,1,0,0,1>	yxxy;
		VEC_CONST _v4<E,1,0,1,0>	yxyx;
		VEC_CONST _v4<E,1,0,1,1>	yxyy;
		VEC_CONST _v4<E,1,1,0,0>	yyxx;
		VEC_CONST _v4<E,1,1,0,1>	yyxy;
		VEC_CONST _v4<E,1,1,1,0>	yyyx;
		VEC_CONST _v4<E,1,1,1,1>	yyyy;
	};
	vec()								{}
	force_inline vec(const E *p)			{ xy.q = vload<2>(p); }
	force_inline vec(E *p)				{ xy.q = vload<2>(p); }
#ifdef __GNUC__
	force_inline vec(const E (&p)[2])	{ xy.q = vload<2>(p); }
#endif
	template<int N, int A, int B> 		force_inline vec(const _v2<E,N,A,B> &a)	{ xy = a; }
	template<typename A> explicit		force_inline vec(const A &a)				{ xy = a; }
	template<typename A, typename B>	force_inline vec(const A &a, const B &b)	{ xy = pair<A,B>(a, b); }

	template<typename T> force_inline vec& operator =(const T &b)	{ xy  = b; return *this; }
	template<typename T> force_inline vec& operator+=(const T &b)	{ xy += b; return *this; }
	template<typename T> force_inline vec& operator-=(const T &b)	{ xy -= b; return *this; }
	template<typename T> force_inline vec& operator*=(const T &b)	{ xy *= b; return *this; }
	template<typename T> force_inline vec& operator/=(const T &b)	{ xy /= b; return *this; }

	force_inline vec&	operator++()	{ return *this += one; }
	force_inline vec		operator++(int)	{ vec t = *this; operator++(); return t; }
	force_inline vec&	operator--()	{ return *this -= one; }
	force_inline vec		operator--(int)	{ vec t = *this; operator--(); return t; }

	vec&	operator=(const vec &a)		{ xy.q = a.xy.q; return *this; }
	E		operator[](int i)	const	{ return ((E*)this)[i]; }
	E&		operator[](int i)			{ return ((E*)this)[i]; }
	void	store(E *p)			const	{ vstore<2>(p, xy.q); }
} VEC_ATTR;

template<typename E> struct vget<vec<E,2> > : vdims<2> {
	typedef _v2<E,0,0,1>	R, L;
	typedef	vec<E,2>		O;
	static force_inline R r(const O &t)	{ return force_cast<R>(t); }
	static force_inline L l(const O &t)	{ return force_cast<L>(t); }
};

template<typename E> inline E area(const interval<vec<E,2> > &i) {
	return prod_components(i.b - i.a);
}

template<typename E> force_inline _v1<E,0,0>						diff(const vec<E,2> &a)		{ return a.x - a.y; }
template<typename E> force_inline vec<E,2>						perp(vec<E,2> a)			{ return vec<E,2>(-a.y, a.x); }
template<typename E, int N, int A, int B> force_inline vec<E,2>	perp(const _v2<E,N,A,B> &a)	{ return vec<E,2>(-perm<1>(a), perm<0>(a)); }
template<typename E> force_inline vec<E,2>						sort(vec<E,2> v) { return sort(v.v); }

//-----------------------------------------------------------------------------
//	vec3
//-----------------------------------------------------------------------------

template<typename E> class VEC_ATTR0 vec<E,3> {
public:
	typedef	_v1<E,7,0>		x_t;
	typedef	_v1<E,7,1>		y_t;
	typedef	_v1<E,7,2>		z_t;
	union {
		_v3<E,7,0,1,2>	v;
		// LHS + RHS
		_v1<E,7,0>		x;
		_v1<E,7,1>		y;
		_v1<E,7,2>		z;

		_v2<E,7,0,1>	xy;
		_v2<E,7,0,2>	xz;
		_v2<E,7,1,0>	yx;
		_v2<E,7,1,2>	yz;
		_v2<E,7,2,0>	zx;
		_v2<E,7,2,1>	zy;

		_v3<E,7,0,1,2>	xyz;
		_v3<E,7,0,2,1>	xzy;
		_v3<E,7,1,0,2>	yxz;
		_v3<E,7,1,2,0>	yzx;
		_v3<E,7,2,0,1>	zxy;
		_v3<E,7,2,1,0>	zyx;

		// RHS only
		VEC_CONST _v2<E,0,0,0>		xx;
		VEC_CONST _v2<E,0,1,1>		yy;
		VEC_CONST _v2<E,0,2,2>		zz;

		VEC_CONST _v3<E,0,0,0,0>	xxx;
		VEC_CONST _v3<E,0,0,0,1>	xxy;
		VEC_CONST _v3<E,0,0,0,2>	xxz;
		VEC_CONST _v3<E,0,0,1,0>	xyx;
		VEC_CONST _v3<E,0,0,1,1>	xyy;
		VEC_CONST _v3<E,0,0,2,0>	xzx;
		VEC_CONST _v3<E,0,0,2,2>	xzz;

		VEC_CONST _v3<E,0,1,0,0>	yxx;
		VEC_CONST _v3<E,0,1,0,1>	yxy;
		VEC_CONST _v3<E,0,1,1,0>	yyx;
		VEC_CONST _v3<E,0,1,1,1>	yyy;
		VEC_CONST _v3<E,0,1,1,2>	yyz;
		VEC_CONST _v3<E,0,1,2,1>	yzy;
		VEC_CONST _v3<E,0,1,2,2>	yzz;

		VEC_CONST _v3<E,0,2,0,0>	zxx;
		VEC_CONST _v3<E,0,2,0,2>	zxz;
		VEC_CONST _v3<E,0,2,1,1>	zyy;
		VEC_CONST _v3<E,0,2,1,2>	zyz;
		VEC_CONST _v3<E,0,2,2,0>	zzx;
		VEC_CONST _v3<E,0,2,2,1>	zzy;
		VEC_CONST _v3<E,0,2,2,2>	zzz;

		VEC_CONST _v4<E,0,0,0,0>	xxxx;
		VEC_CONST _v4<E,0,0,0,1>	xxxy;
		VEC_CONST _v4<E,0,0,0,2>	xxxz;
		VEC_CONST _v4<E,0,0,1,0>	xxyx;
		VEC_CONST _v4<E,0,0,1,1>	xxyy;
		VEC_CONST _v4<E,0,0,1,2>	xxyz;
		VEC_CONST _v4<E,0,0,2,0>	xxzx;
		VEC_CONST _v4<E,0,0,2,1>	xxzy;
		VEC_CONST _v4<E,0,0,2,2>	xxzz;
		VEC_CONST _v4<E,0,1,0,0>	xyxx;
		VEC_CONST _v4<E,0,1,0,1>	xyxy;
		VEC_CONST _v4<E,0,1,0,2>	xyxz;
		VEC_CONST _v4<E,0,1,1,0>	xyyx;
		VEC_CONST _v4<E,0,1,1,1>	xyyy;
		VEC_CONST _v4<E,0,1,1,2>	xyyz;
		VEC_CONST _v4<E,0,1,2,0>	xyzx;
		VEC_CONST _v4<E,0,1,2,1>	xyzy;
		VEC_CONST _v4<E,0,1,2,2>	xyzz;
		VEC_CONST _v4<E,0,2,0,0>	xzxx;
		VEC_CONST _v4<E,0,2,0,1>	xzxy;
		VEC_CONST _v4<E,0,2,0,2>	xzxz;
		VEC_CONST _v4<E,0,2,1,0>	xzyx;
		VEC_CONST _v4<E,0,2,1,1>	xzyy;
		VEC_CONST _v4<E,0,2,1,2>	xzyz;
		VEC_CONST _v4<E,0,2,2,0>	xzzx;
		VEC_CONST _v4<E,0,2,2,1>	xzzy;
		VEC_CONST _v4<E,0,2,2,2>	xzzz;

		VEC_CONST _v4<E,1,0,0,0>	yxxx;
		VEC_CONST _v4<E,1,0,0,1>	yxxy;
		VEC_CONST _v4<E,1,0,0,2>	yxxz;
		VEC_CONST _v4<E,1,0,1,0>	yxyx;
		VEC_CONST _v4<E,1,0,1,1>	yxyy;
		VEC_CONST _v4<E,1,0,1,2>	yxyz;
		VEC_CONST _v4<E,1,0,2,0>	yxzx;
		VEC_CONST _v4<E,1,0,2,1>	yxzy;
		VEC_CONST _v4<E,1,0,2,2>	yxzz;
		VEC_CONST _v4<E,1,1,0,0>	yyxx;
		VEC_CONST _v4<E,1,1,0,1>	yyxy;
		VEC_CONST _v4<E,1,1,0,2>	yyxz;
		VEC_CONST _v4<E,1,1,1,0>	yyyx;
		VEC_CONST _v4<E,1,1,1,1>	yyyy;
		VEC_CONST _v4<E,1,1,1,2>	yyyz;
		VEC_CONST _v4<E,1,1,2,0>	yyzx;
		VEC_CONST _v4<E,1,1,2,1>	yyzy;
		VEC_CONST _v4<E,1,1,2,2>	yyzz;
		VEC_CONST _v4<E,1,2,0,0>	yzxx;
		VEC_CONST _v4<E,1,2,0,1>	yzxy;
		VEC_CONST _v4<E,1,2,0,2>	yzxz;
		VEC_CONST _v4<E,1,2,1,0>	yzyx;
		VEC_CONST _v4<E,1,2,1,1>	yzyy;
		VEC_CONST _v4<E,1,2,1,2>	yzyz;
		VEC_CONST _v4<E,1,2,2,0>	yzzx;
		VEC_CONST _v4<E,1,2,2,1>	yzzy;
		VEC_CONST _v4<E,1,2,2,2>	yzzz;

		VEC_CONST _v4<E,2,0,0,0>	zxxx;
		VEC_CONST _v4<E,2,0,0,1>	zxxy;
		VEC_CONST _v4<E,2,0,0,2>	zxxz;
		VEC_CONST _v4<E,2,0,1,0>	zxyx;
		VEC_CONST _v4<E,2,0,1,1>	zxyy;
		VEC_CONST _v4<E,2,0,1,2>	zxyz;
		VEC_CONST _v4<E,2,0,2,0>	zxzx;
		VEC_CONST _v4<E,2,0,2,1>	zxzy;
		VEC_CONST _v4<E,2,0,2,2>	zxzz;
		VEC_CONST _v4<E,2,1,0,0>	zyxx;
		VEC_CONST _v4<E,2,1,0,1>	zyxy;
		VEC_CONST _v4<E,2,1,0,2>	zyxz;
		VEC_CONST _v4<E,2,1,1,0>	zyyx;
		VEC_CONST _v4<E,2,1,1,1>	zyyy;
		VEC_CONST _v4<E,2,1,1,2>	zyyz;
		VEC_CONST _v4<E,2,1,2,0>	zyzx;
		VEC_CONST _v4<E,2,1,2,1>	zyzy;
		VEC_CONST _v4<E,2,1,2,2>	zyzz;
		VEC_CONST _v4<E,2,2,0,0>	zzxx;
		VEC_CONST _v4<E,2,2,0,1>	zzxy;
		VEC_CONST _v4<E,2,2,0,2>	zzxz;
		VEC_CONST _v4<E,2,2,1,0>	zzyx;
		VEC_CONST _v4<E,2,2,1,1>	zzyy;
		VEC_CONST _v4<E,2,2,1,2>	zzyz;
		VEC_CONST _v4<E,2,2,2,0>	zzzx;
		VEC_CONST _v4<E,2,2,2,1>	zzzy;
		VEC_CONST _v4<E,2,2,2,2>	zzzz;
	};
	vec()	{}
	force_inline vec(const E *p)				{ xyz.q = vload<3>(p); }
	force_inline vec(E *p)					{ xyz.q = vload<3>(p); }
	force_inline vec(const pos<E,2> &a)		{ xyz = a; }
#ifdef __GNUC__
	force_inline vec(const E (&p)[3])		{ xyz.q = vload<3>(p); }
#endif
	template<int N, int A, int B, int C> 			force_inline vec(const _v3<E,N,A,B,C> &a)			{ xyz = a; }
	template<typename A> explicit					force_inline vec(const A &a)							{ xyz = a; }
	template<typename A, typename B>				force_inline vec(const A &a, const B &b)				{ xyz = pair<A,B>(a, b); }
	template<typename A, typename B, typename C>	force_inline vec(const A &a, const B &b, const C &c)	{ xyz = make_pair(pair<A,B>(a, b), c); }

	template<typename T> force_inline vec& operator =(const T &b)	{ xyz  = b;	return *this; }
	template<typename T> force_inline vec& operator+=(const T &b)	{ xyz += b;	return *this; }
	template<typename T> force_inline vec& operator-=(const T &b)	{ xyz -= b;	return *this; }
	template<typename T> force_inline vec& operator*=(const T &b)	{ xyz *= b;	return *this; }
	template<typename T> force_inline vec& operator/=(const T &b)	{ xyz /= b;	return *this; }

	force_inline vec&	operator++()	{ return *this += one; }
	force_inline vec		operator++(int)	{ vec t = *this; operator++(); return t; }
	force_inline vec&	operator--()	{ return *this -= one; }
	force_inline vec		operator--(int)	{ vec t = *this; operator--(); return t; }

	vec&	operator=(const vec &a)		{ xyz.q = a.xyz.q; return *this; }
	E		operator[](int i)	const	{ return ((E*)this)[i]; }
	E&		operator[](int i)			{ return ((E*)this)[i]; }
	void	store(E *p)			const	{ vstore<3>(p, xyz.q); }
} VEC_ATTR;

template<typename E> struct vget<vec<E, 3> > : vdims<3> {
	typedef _v3<E,0,0,1,2>	R, L;
	typedef	vec<E, 3>		O;
	static force_inline R r(const O &t)	{ return force_cast<R>(t); }
	static force_inline L l(const O &t)	{ return force_cast<L>(t); }
};

template<typename E> inline E volume(const interval<vec<E, 3> > &i) {
	return prod_components(i.b - i.a);
}

template<typename E> force_inline vec<E,3>								perp(vec<E,3> v)		{ return cross(vec<E,3>(x_axis) >> min_component_index(abs(v)), v); }
template<typename E, int N, int A, int B, int C>	force_inline	auto	perp(_v3<E,N,A,B,C> v)	{ return cross(vec<E,3>(x_axis) >> min_component_index(abs(v)), v); }
template<typename E> force_inline vec<E,3>								sort(vec<E,3> v)		{ return sort(v.v); }

//-----------------------------------------------------------------------------
//	vec4
//-----------------------------------------------------------------------------

template<typename E> class VEC_ATTR0 vec<E,4> {
public:
	typedef	_v1<E,15,0>		x_t;
	typedef	_v1<E,15,1>		y_t;
	typedef	_v1<E,15,2>		z_t;
	typedef	_v1<E,15,3>		w_t;
	typedef	_v2<E,15,0,1>	xy_t;
	typedef	_v2<E,15,2,3>	zw_t;
	typedef	_v3<E,15,0,1,2>	xyz_t;
	union {
		_v4<E,0,1,2,3>	v;
		// LHS + RHS
		_v1<E,15,0>		x;
		_v1<E,15,1>		y;
		_v1<E,15,2>		z;
		_v1<E,15,3>		w;

		_v2<E,15,0,1>	xy;
		_v2<E,15,0,2>	xz;
		_v2<E,15,0,3>	xw;
		_v2<E,15,1,0>	yx;
		_v2<E,15,1,2>	yz;
		_v2<E,15,1,3>	yw;
		_v2<E,15,2,0>	zx;
		_v2<E,15,2,1>	zy;
		_v2<E,15,2,3>	zw;
		_v2<E,15,3,0>	wx;
		_v2<E,15,3,1>	wy;
		_v2<E,15,3,2>	wz;

		_v3<E,15,0,1,2>	xyz;
		_v3<E,15,0,1,3>	xyw;
		_v3<E,15,0,2,1>	xzy;
		_v3<E,15,0,2,3>	xzw;
		_v3<E,15,0,3,1>	xwy;
		_v3<E,15,0,3,2>	xwz;
		_v3<E,15,1,0,2>	yxz;
		_v3<E,15,1,0,3>	yxw;
		_v3<E,15,1,2,0>	yzx;
		_v3<E,15,1,2,3>	yzw;
		_v3<E,15,1,3,0>	ywx;
		_v3<E,15,1,3,2>	ywz;
		_v3<E,15,2,0,1>	zxy;
		_v3<E,15,2,0,3>	zxw;
		_v3<E,15,2,1,0>	zyx;
		_v3<E,15,2,1,3>	zyw;
		_v3<E,15,2,3,0>	zwx;
		_v3<E,15,2,3,1>	zwy;
		_v3<E,15,3,0,1>	wxy;
		_v3<E,15,3,0,2>	wxz;
		_v3<E,15,3,1,0>	wyx;
		_v3<E,15,3,1,2>	wyz;
		_v3<E,15,3,2,0>	wzx;
		_v3<E,15,3,2,1>	wzy;

		_v4<E,0,1,2,3>	xyzw;
		_v4<E,0,1,3,2>	xywz;
		_v4<E,0,2,1,3>	xzyw;
		_v4<E,0,2,3,1>	xzwy;
		_v4<E,0,3,1,2>	xwyz;
		_v4<E,0,3,2,1>	xwzy;
		_v4<E,1,0,2,3>	yxzw;
		_v4<E,1,0,3,2>	yxwz;
		_v4<E,1,2,0,3>	yzxw;
		_v4<E,1,2,3,0>	yzwx;
		_v4<E,1,3,0,2>	ywxz;
		_v4<E,1,3,2,0>	ywzw;
		_v4<E,2,0,1,3>	zxyw;
		_v4<E,2,0,3,1>	zxwy;
		_v4<E,2,1,0,3>	zyxw;
		_v4<E,2,1,3,0>	zywx;
		_v4<E,2,3,0,1>	zwxy;
		_v4<E,2,3,1,0>	zwyx;
		_v4<E,3,0,1,2>	wxyz;
		_v4<E,3,0,2,1>	wxzy;
		_v4<E,3,1,0,2>	wyxz;
		_v4<E,3,1,2,0>	wyzx;
		_v4<E,3,2,0,1>	wzxy;
		_v4<E,3,2,1,0>	wzyx;

		// RHS only
		VEC_CONST _v2<E,0,0,0>		xx;
		VEC_CONST _v2<E,0,1,1>		yy;
		VEC_CONST _v2<E,0,2,2>		zz;
		VEC_CONST _v2<E,0,3,3>		ww;

		VEC_CONST _v3<E,0,0,0,0>	xxx;
		VEC_CONST _v3<E,0,0,1,0>	xyx;
		VEC_CONST _v3<E,0,0,2,0>	xzx;
		VEC_CONST _v3<E,0,0,3,0>	xwx;
		VEC_CONST _v3<E,0,0,0,1>	xxy;
		VEC_CONST _v3<E,0,0,1,1>	xyy;
		VEC_CONST _v3<E,0,0,0,2>	xxz;
		VEC_CONST _v3<E,0,0,2,2>	xzz;
		VEC_CONST _v3<E,0,0,0,3>	xxw;
		VEC_CONST _v3<E,0,0,3,3>	xww;

		VEC_CONST _v3<E,0,1,0,0>	yxx;
		VEC_CONST _v3<E,0,1,1,0>	yyx;
		VEC_CONST _v3<E,0,1,0,1>	yxy;
		VEC_CONST _v3<E,0,1,1,1>	yyy;
		VEC_CONST _v3<E,0,1,2,1>	yzy;
		VEC_CONST _v3<E,0,1,3,1>	ywy;
		VEC_CONST _v3<E,0,1,1,2>	yyz;
		VEC_CONST _v3<E,0,1,2,2>	yzz;
		VEC_CONST _v3<E,0,1,1,3>	yyw;
		VEC_CONST _v3<E,0,1,3,3>	yww;

		VEC_CONST _v3<E,0,2,0,0>	zxx;
		VEC_CONST _v3<E,0,2,2,0>	zzx;
		VEC_CONST _v3<E,0,2,1,1>	zyy;
		VEC_CONST _v3<E,0,2,2,1>	zzy;
		VEC_CONST _v3<E,0,2,0,2>	zxz;
		VEC_CONST _v3<E,0,2,1,2>	zyz;
		VEC_CONST _v3<E,0,2,2,2>	zzz;
		VEC_CONST _v3<E,0,2,3,2>	zwz;
		VEC_CONST _v3<E,0,2,2,3>	zzw;
		VEC_CONST _v3<E,0,2,3,3>	zww;

		VEC_CONST _v3<E,0,3,0,0>	wxx;
		VEC_CONST _v3<E,0,3,3,0>	wwx;
		VEC_CONST _v3<E,0,3,1,1>	wyy;
		VEC_CONST _v3<E,0,3,3,1>	wwy;
		VEC_CONST _v3<E,0,3,2,2>	wzz;
		VEC_CONST _v3<E,0,3,3,2>	wwz;
		VEC_CONST _v3<E,0,3,0,3>	wxw;
		VEC_CONST _v3<E,0,3,1,3>	wyw;
		VEC_CONST _v3<E,0,3,2,3>	wzw;
		VEC_CONST _v3<E,0,3,3,3>	www;

		VEC_CONST _v4<E,0,0,0,0>	xxxx;
		VEC_CONST _v4<E,0,0,0,1>	xxxy;
		VEC_CONST _v4<E,0,0,0,2>	xxxz;
		VEC_CONST _v4<E,0,0,0,3>	xxxw;
		VEC_CONST _v4<E,0,0,1,0>	xxyx;
		VEC_CONST _v4<E,0,0,1,1>	xxyy;
		VEC_CONST _v4<E,0,0,1,2>	xxyz;
		VEC_CONST _v4<E,0,0,1,3>	xxyw;
		VEC_CONST _v4<E,0,0,2,0>	xxzx;
		VEC_CONST _v4<E,0,0,2,1>	xxzy;
		VEC_CONST _v4<E,0,0,2,2>	xxzz;
		VEC_CONST _v4<E,0,0,2,3>	xxzw;
		VEC_CONST _v4<E,0,0,3,0>	xxwx;
		VEC_CONST _v4<E,0,0,3,1>	xxwy;
		VEC_CONST _v4<E,0,0,3,2>	xxwz;
		VEC_CONST _v4<E,0,0,3,3>	xxww;
		VEC_CONST _v4<E,0,1,0,0>	xyxx;
		VEC_CONST _v4<E,0,1,0,1>	xyxy;
		VEC_CONST _v4<E,0,1,0,2>	xyxz;
		VEC_CONST _v4<E,0,1,0,3>	xyxw;
		VEC_CONST _v4<E,0,1,1,0>	xyyx;
		VEC_CONST _v4<E,0,1,1,1>	xyyy;
		VEC_CONST _v4<E,0,1,1,2>	xyyz;
		VEC_CONST _v4<E,0,1,1,3>	xyyw;
		VEC_CONST _v4<E,0,1,2,0>	xyzx;
		VEC_CONST _v4<E,0,1,2,1>	xyzy;
		VEC_CONST _v4<E,0,1,2,2>	xyzz;
		VEC_CONST _v4<E,0,1,3,0>	xywx;
		VEC_CONST _v4<E,0,1,3,1>	xywy;
		VEC_CONST _v4<E,0,1,3,3>	xyww;
		VEC_CONST _v4<E,0,2,0,0>	xzxx;
		VEC_CONST _v4<E,0,2,0,1>	xzxy;
		VEC_CONST _v4<E,0,2,0,2>	xzxz;
		VEC_CONST _v4<E,0,2,0,3>	xzxw;
		VEC_CONST _v4<E,0,2,1,0>	xzyx;
		VEC_CONST _v4<E,0,2,1,1>	xzyy;
		VEC_CONST _v4<E,0,2,1,2>	xzyz;
		VEC_CONST _v4<E,0,2,2,0>	xzzx;
		VEC_CONST _v4<E,0,2,2,1>	xzzy;
		VEC_CONST _v4<E,0,2,2,2>	xzzz;
		VEC_CONST _v4<E,0,2,2,3>	xzzw;
		VEC_CONST _v4<E,0,2,3,0>	xzwx;
		VEC_CONST _v4<E,0,2,3,2>	xzwz;
		VEC_CONST _v4<E,0,2,3,3>	xzww;
		VEC_CONST _v4<E,0,3,0,0>	xwxx;
		VEC_CONST _v4<E,0,3,0,1>	xwxy;
		VEC_CONST _v4<E,0,3,0,2>	xwxz;
		VEC_CONST _v4<E,0,3,0,3>	xwxw;
		VEC_CONST _v4<E,0,3,1,0>	xwyx;
		VEC_CONST _v4<E,0,3,1,1>	xwyy;
		VEC_CONST _v4<E,0,3,1,3>	xwyw;
		VEC_CONST _v4<E,0,3,2,0>	xwzx;
		VEC_CONST _v4<E,0,3,2,2>	xwzz;
		VEC_CONST _v4<E,0,3,2,3>	xwzw;
		VEC_CONST _v4<E,0,3,3,0>	xwwx;
		VEC_CONST _v4<E,0,3,3,1>	xwwy;
		VEC_CONST _v4<E,0,3,3,2>	xwwz;
		VEC_CONST _v4<E,0,3,3,3>	xwww;

		VEC_CONST _v4<E,1,0,0,0>	yxxx;
		VEC_CONST _v4<E,1,0,0,1>	yxxy;
		VEC_CONST _v4<E,1,0,0,2>	yxxz;
		VEC_CONST _v4<E,1,0,0,3>	yxxw;
		VEC_CONST _v4<E,1,0,1,0>	yxyx;
		VEC_CONST _v4<E,1,0,1,1>	yxyy;
		VEC_CONST _v4<E,1,0,1,2>	yxyz;
		VEC_CONST _v4<E,1,0,1,3>	yxyw;
		VEC_CONST _v4<E,1,0,2,0>	yxzx;
		VEC_CONST _v4<E,1,0,2,1>	yxzy;
		VEC_CONST _v4<E,1,0,2,2>	yxzz;
		VEC_CONST _v4<E,1,0,3,0>	yxwx;
		VEC_CONST _v4<E,1,0,3,1>	yxwy;
		VEC_CONST _v4<E,1,0,3,3>	yxww;
		VEC_CONST _v4<E,1,1,0,0>	yyxx;
		VEC_CONST _v4<E,1,1,0,1>	yyxy;
		VEC_CONST _v4<E,1,1,0,2>	yyxz;
		VEC_CONST _v4<E,1,1,0,3>	yyxw;
		VEC_CONST _v4<E,1,1,1,0>	yyyx;
		VEC_CONST _v4<E,1,1,1,1>	yyyy;
		VEC_CONST _v4<E,1,1,1,2>	yyyz;
		VEC_CONST _v4<E,1,1,1,3>	yyyw;
		VEC_CONST _v4<E,1,1,2,0>	yyzx;
		VEC_CONST _v4<E,1,1,2,1>	yyzy;
		VEC_CONST _v4<E,1,1,2,2>	yyzz;
		VEC_CONST _v4<E,1,1,2,3>	yyzw;
		VEC_CONST _v4<E,1,1,3,0>	yywx;
		VEC_CONST _v4<E,1,1,3,1>	yywy;
		VEC_CONST _v4<E,1,1,3,2>	yywz;
		VEC_CONST _v4<E,1,1,3,3>	yyww;
		VEC_CONST _v4<E,1,2,0,0>	yzxx;
		VEC_CONST _v4<E,1,2,0,1>	yzxy;
		VEC_CONST _v4<E,1,2,0,2>	yzxz;
		VEC_CONST _v4<E,1,2,1,0>	yzyx;
		VEC_CONST _v4<E,1,2,1,1>	yzyy;
		VEC_CONST _v4<E,1,2,1,2>	yzyz;
		VEC_CONST _v4<E,1,2,1,3>	yzyw;
		VEC_CONST _v4<E,1,2,2,0>	yzzx;
		VEC_CONST _v4<E,1,2,2,1>	yzzy;
		VEC_CONST _v4<E,1,2,2,2>	yzzz;
		VEC_CONST _v4<E,1,2,2,3>	yzzw;
		VEC_CONST _v4<E,1,2,3,1>	yzwy;
		VEC_CONST _v4<E,1,2,3,2>	yzwz;
		VEC_CONST _v4<E,1,2,3,3>	yzww;
		VEC_CONST _v4<E,1,3,0,0>	ywxx;
		VEC_CONST _v4<E,1,3,0,1>	ywxy;
		VEC_CONST _v4<E,1,3,0,3>	ywxw;
		VEC_CONST _v4<E,1,3,1,0>	ywyx;
		VEC_CONST _v4<E,1,3,1,1>	ywyy;
		VEC_CONST _v4<E,1,3,1,2>	ywyz;
		VEC_CONST _v4<E,1,3,1,3>	ywyw;
		VEC_CONST _v4<E,1,3,2,0>	ywzx;
		VEC_CONST _v4<E,1,3,2,1>	ywzy;
		VEC_CONST _v4<E,1,3,2,2>	ywzz;
		VEC_CONST _v4<E,1,3,3,0>	ywwx;
		VEC_CONST _v4<E,1,3,3,1>	ywwy;
		VEC_CONST _v4<E,1,3,3,2>	ywwz;
		VEC_CONST _v4<E,1,3,3,3>	ywww;

		VEC_CONST _v4<E,2,0,0,0>	zxxx;
		VEC_CONST _v4<E,2,0,0,1>	zxxy;
		VEC_CONST _v4<E,2,0,0,2>	zxxz;
		VEC_CONST _v4<E,2,0,0,3>	zxxw;
		VEC_CONST _v4<E,2,0,1,0>	zxyx;
		VEC_CONST _v4<E,2,0,1,1>	zxyy;
		VEC_CONST _v4<E,2,0,1,2>	zxyz;
		VEC_CONST _v4<E,2,0,2,0>	zxzx;
		VEC_CONST _v4<E,2,0,2,1>	zxzy;
		VEC_CONST _v4<E,2,0,2,2>	zxzz;
		VEC_CONST _v4<E,2,0,2,3>	zxzw;
		VEC_CONST _v4<E,2,0,3,0>	zxwx;
		VEC_CONST _v4<E,2,0,3,2>	zxwz;
		VEC_CONST _v4<E,2,0,3,3>	zxww;
		VEC_CONST _v4<E,2,1,0,0>	zyxx;
		VEC_CONST _v4<E,2,1,0,1>	zyxy;
		VEC_CONST _v4<E,2,1,0,2>	zyxz;
		VEC_CONST _v4<E,2,1,1,0>	zyyx;
		VEC_CONST _v4<E,2,1,1,1>	zyyy;
		VEC_CONST _v4<E,2,1,1,2>	zyyz;
		VEC_CONST _v4<E,2,1,1,3>	zyyw;
		VEC_CONST _v4<E,2,1,2,0>	zyzx;
		VEC_CONST _v4<E,2,1,2,1>	zyzy;
		VEC_CONST _v4<E,2,1,2,2>	zyzz;
		VEC_CONST _v4<E,2,1,2,3>	zyzw;
		VEC_CONST _v4<E,2,1,3,1>	zywy;
		VEC_CONST _v4<E,2,1,3,2>	zywz;
		VEC_CONST _v4<E,2,1,3,3>	zyww;
		VEC_CONST _v4<E,2,2,0,0>	zzxx;
		VEC_CONST _v4<E,2,2,0,1>	zzxy;
		VEC_CONST _v4<E,2,2,0,2>	zzxz;
		VEC_CONST _v4<E,2,2,0,3>	zzxw;
		VEC_CONST _v4<E,2,2,1,0>	zzyx;
		VEC_CONST _v4<E,2,2,1,1>	zzyy;
		VEC_CONST _v4<E,2,2,1,2>	zzyz;
		VEC_CONST _v4<E,2,2,1,3>	zzyw;
		VEC_CONST _v4<E,2,2,2,0>	zzzx;
		VEC_CONST _v4<E,2,2,2,1>	zzzy;
		VEC_CONST _v4<E,2,2,2,2>	zzzz;
		VEC_CONST _v4<E,2,2,2,3>	zzzw;
		VEC_CONST _v4<E,2,2,3,0>	zzwx;
		VEC_CONST _v4<E,2,2,3,1>	zzwy;
		VEC_CONST _v4<E,2,2,3,2>	zzwz;
		VEC_CONST _v4<E,2,2,3,3>	zzww;
		VEC_CONST _v4<E,2,3,0,0>	zwxx;
		VEC_CONST _v4<E,2,3,0,2>	zwxz;
		VEC_CONST _v4<E,2,3,0,3>	zwxw;
		VEC_CONST _v4<E,2,3,1,1>	zwyy;
		VEC_CONST _v4<E,2,3,1,2>	zwyz;
		VEC_CONST _v4<E,2,3,1,3>	zwyw;
		VEC_CONST _v4<E,2,3,2,0>	zwzx;
		VEC_CONST _v4<E,2,3,2,1>	zwzy;
		VEC_CONST _v4<E,2,3,2,2>	zwzz;
		VEC_CONST _v4<E,2,3,2,3>	zwzw;
		VEC_CONST _v4<E,2,3,3,0>	zwwx;
		VEC_CONST _v4<E,2,3,3,1>	zwwy;
		VEC_CONST _v4<E,2,3,3,2>	zwwz;
		VEC_CONST _v4<E,2,3,3,3>	zwww;

		VEC_CONST _v4<E,3,0,0,0>	wxxx;
		VEC_CONST _v4<E,3,0,0,1>	wxxy;
		VEC_CONST _v4<E,3,0,0,2>	wxxz;
		VEC_CONST _v4<E,3,0,0,3>	wxxw;
		VEC_CONST _v4<E,3,0,1,0>	wxyx;
		VEC_CONST _v4<E,3,0,1,1>	wxyy;
		VEC_CONST _v4<E,3,0,1,3>	wxyw;
		VEC_CONST _v4<E,3,0,2,0>	wxzx;
		VEC_CONST _v4<E,3,0,2,2>	wxzz;
		VEC_CONST _v4<E,3,0,2,3>	wxzw;
		VEC_CONST _v4<E,3,0,3,0>	wxwx;
		VEC_CONST _v4<E,3,0,3,1>	wxwy;
		VEC_CONST _v4<E,3,0,3,2>	wxwz;
		VEC_CONST _v4<E,3,0,3,3>	wxww;
		VEC_CONST _v4<E,3,1,0,0>	wyxx;
		VEC_CONST _v4<E,3,1,0,1>	wyxy;
		VEC_CONST _v4<E,3,1,0,3>	wyxw;
		VEC_CONST _v4<E,3,1,1,0>	wyyx;
		VEC_CONST _v4<E,3,1,1,1>	wyyy;
		VEC_CONST _v4<E,3,1,1,2>	wyyz;
		VEC_CONST _v4<E,3,1,1,3>	wyyw;
		VEC_CONST _v4<E,3,1,2,1>	wyzy;
		VEC_CONST _v4<E,3,1,2,2>	wyzz;
		VEC_CONST _v4<E,3,1,2,3>	wyzw;
		VEC_CONST _v4<E,3,1,3,0>	wywx;
		VEC_CONST _v4<E,3,1,3,1>	wywy;
		VEC_CONST _v4<E,3,1,3,2>	wywz;
		VEC_CONST _v4<E,3,1,3,3>	wyww;
		VEC_CONST _v4<E,3,2,0,0>	wzxx;
		VEC_CONST _v4<E,3,2,0,2>	wzxz;
		VEC_CONST _v4<E,3,2,0,3>	wzxw;
		VEC_CONST _v4<E,3,2,1,1>	wzyy;
		VEC_CONST _v4<E,3,2,1,2>	wzyz;
		VEC_CONST _v4<E,3,2,1,3>	wzyw;
		VEC_CONST _v4<E,3,2,2,0>	wzzx;
		VEC_CONST _v4<E,3,2,2,1>	wzzy;
		VEC_CONST _v4<E,3,2,2,2>	wzzz;
		VEC_CONST _v4<E,3,2,2,3>	wzzw;
		VEC_CONST _v4<E,3,2,3,0>	wzwx;
		VEC_CONST _v4<E,3,2,3,1>	wzwy;
		VEC_CONST _v4<E,3,2,3,2>	wzwz;
		VEC_CONST _v4<E,3,2,3,3>	wzww;
		VEC_CONST _v4<E,3,3,0,0>	wwxx;
		VEC_CONST _v4<E,3,3,0,1>	wwxy;
		VEC_CONST _v4<E,3,3,0,2>	wwxz;
		VEC_CONST _v4<E,3,3,0,3>	wwxw;
		VEC_CONST _v4<E,3,3,1,0>	wwyx;
		VEC_CONST _v4<E,3,3,1,1>	wwyy;
		VEC_CONST _v4<E,3,3,1,2>	wwyz;
		VEC_CONST _v4<E,3,3,1,3>	wwyw;
		VEC_CONST _v4<E,3,3,2,0>	wwzx;
		VEC_CONST _v4<E,3,3,2,1>	wwzy;
		VEC_CONST _v4<E,3,3,2,2>	wwzz;
		VEC_CONST _v4<E,3,3,2,3>	wwzw;
		VEC_CONST _v4<E,3,3,3,0>	wwwx;
		VEC_CONST _v4<E,3,3,3,1>	wwwy;
		VEC_CONST _v4<E,3,3,3,2>	wwwz;
		VEC_CONST _v4<E,3,3,3,3>	wwww;
	};

	force_inline vec()					{}
	force_inline vec(const E *p)			{ xyzw.q = vload<4>(p); }
	force_inline vec(E *p)				{ xyzw.q = vload<4>(p); }
	force_inline vec(const pos<E,3> &a)	{ xyzw = a; }
#ifdef __GNUC__
//	force_inline vec(const E (&p)[4])	{ xyzw.q = vload<4>(p); }
#endif
	template<int A, int B, int C, int D> 						force_inline vec(const _v4<E,A,B,C,D> &a)						{ xyzw = a; }
	template<int A, int B, int C, int D> 						force_inline vec(const _v4p<E,A,B,C,D> &a)						{ xyzw = a; }
	template<typename A> explicit								force_inline vec(const A &a)										{ xyzw = a; }
	template<typename A, typename B>							force_inline vec(const A &a, const B &b)							{ xyzw = pair<A,B>(a, b); }
	template<typename A, typename B, typename C>				force_inline vec(const A &a, const B &b, const C &c)				{ xyzw = make_pair(pair<A,B>(a, b), c); }
	template<typename A, typename B, typename C, typename D>	force_inline vec(const A &a, const B &b, const C &c, const D &d)	{ xyzw = make_pair(make_pair(pair<A,B>(a, b), c), d); }

	template<typename T> force_inline vec& operator =(const T &b)	{ xyzw  = b; return *this; }
	template<typename T> force_inline vec& operator+=(const T &b)	{ xyzw += b; return *this; }
	template<typename T> force_inline vec& operator-=(const T &b)	{ xyzw -= b; return *this; }
	template<typename T> force_inline vec& operator*=(const T &b)	{ xyzw *= b; return *this; }
	template<typename T> force_inline vec& operator/=(const T &b)	{ xyzw /= b; return *this; }

	force_inline vec&	operator++()	{ ++xyzw; return *this; }
	force_inline vec		operator++(int)	{ vec t = *this; ++xyzw; return t; }
	force_inline vec&	operator--()	{ --xyzw; return *this; }
	force_inline vec		operator--(int)	{ vec t = *this; --xyzw; return t; }

	vec&	operator=(const vec &a)		{ xyzw.q = a.xyzw.q; return *this; }
	E		operator[](int i)	const	{ return ((E*)this)[i]; }
	E&		operator[](int i)			{ return ((E*)this)[i]; }
	void	store(E *p)			const	{ vstore<4>(p, xyzw.q); }
} VEC_ATTR;

template<typename E> struct vget<vec<E,4> > : vdims<4> {
	typedef _v4<E,0,1,2,3>	R, L;
	typedef	vec<E,4>		O;
	static force_inline R r(const O &t)	{ return t.xyzw; }
	static force_inline L l(const O &t)	{ return t.xyzw; }
};

template<typename E> inline vec<E,4> sort(vec<E,4> v) {
	vec<E,2> min1_2 = min(v.xz, v.yw);
	vec<E,2> max1_2 = max(v.xz, v.yw);

	vec<E,1>	maxmin = max(min1_2.x, min1_2.y);
	vec<E,1>	minmax = min(max1_2.x, max1_2.y);

	return vec<E,4>(
		min(min1_2.x, min1_2.y),
		min(maxmin, minmax),
		max(minmax, maxmin),
		max(max1_2.x, max1_2.y)
	);
}

//-----------------------------------------------------------------------------

template<typename E, int N> inline bool approx_equal(const vec<E,N> &a, const vec<E,N> &b, E tol = ISO_TOLERANCE) {
	return all(abs(a - b) <= max(max(abs(a), abs(b)), one) * tol);
}

template<typename E, int N> inline bool find_scalar_multiple(const vec<E,N> &a, const vec<E,N> &b, float &scale) {
	vec<E,N>	d	= b / a;
	int			i	= max_component_index(a);
	E			e	= d[i];
	scale = e;
	return all(d == e | d != d);
}

//-----------------------------------------------------------------------------
//	pos
//-----------------------------------------------------------------------------

template<typename E, int N1, int N2>	struct to_pos		{ template<typename T> static const T&	f(const T &t)	{ return t; } };

template<typename E, int N> class pos : public vec<E,N> {
	typedef vec<E,N>	B;
	using	B::v;
public:
	using B::vec;
	force_inline pos()	{}
//	template<typename T> pos(const T &b, enable_if_t<vget<T>::dims!=0>*)	: B(to_pos<E, vget<T>::dims, N>::f(b)) {}
//	template<typename T> pos(const T &b, ...)								: B(b) {}
	force_inline pos(param_t<B> b)	: B(b)						{}
	force_inline pos(const vec<E, N+1> &a)	: B(project(a.v))	{}
	template<int N2> force_inline pos(param_t<pos<E, N2>> b)			: B(to_pos<E, N, N2>(b))	{}

	force_inline pos operator+()	const { return *this; }

	template<typename T> force_inline pos& operator =(const T &b)	{ v = to_pos<E, vget<T>::dims, N>::f(b); return *this; }
	template<typename T> force_inline pos& operator+=(const T &b)	{ v += b; return *this; }
	template<typename T> force_inline pos& operator-=(const T &b)	{ v -= b; return *this; }

	template<typename T> force_inline typename T_enable_if<VHASR(T),pos&>::type operator*=(const T &b)	{ v *= b; return *this; }
	template<typename T> force_inline typename T_enable_if<VHASR(T),pos&>::type operator/=(const T &b)	{ v /= b; return *this; }
	template<typename M> IF_MAT(M, pos) operator*=(const M &m) { v = m * *this; return *this; }
	template<typename M> IF_MAT(M, pos) operator/=(const M &m) { v = inverse(m) * *this; return *this; }

	template<typename T>	friend	pos	operator+(const pos &a, const T &b)		{ return a.v + b; }
	template<typename T>	friend	pos	operator-(const pos &a, const T &b)		{ return a.v - b; }
							friend	B	operator-(const pos &a, const pos &b)	{ return a.v - b.v; }

} VEC_ATTR;

template<typename E> struct vget<pos<E,2>> : vdims<2> {
	typedef _v2<E,0,0,1>	R, L;
	typedef	pos<E,2>		O;
	static force_inline R r(param_t<O> t)	{ return force_cast<R>(t); }
	static force_inline L l(param_t<O> t)	{ return force_cast<L>(t); }
};

template<typename E> struct vget<pos<E,3>> : vdims<3> {
	typedef _v3<E,0,0,1,2>	R, L;
	typedef	pos<E,3>		O;
	static force_inline R r(param_t<O> t)	{ return force_cast<R>(t); }
	static force_inline L l(param_t<O> t)	{ return force_cast<L>(t); }
};

template<typename E, int N> force_inline	pos<E,N - 1>	project(const vec<E,N> &a)		{ return project(a.v); }

template<typename E> struct to_pos<E, 4,3>	{ template<typename T> static auto f(const T &t) { return project(vget<T>::r(t)); } };
template<typename E> struct to_pos<E, 3,2>	{ template<typename T> static auto f(const T &t) { return project(t); } };
template<typename E> struct to_pos<E, 2,3>	{ template<typename T> static auto f(const T &t) { return vec<E,3>(t, zero); } };

template<typename E> inline vec<E,1> area(const interval<pos<E,2>> &i) {
	return prod_components(i.b - i.a);
}

template<typename E> inline vec<E,1> volume(const interval<pos<E,3>> &i) {
	return prod_components(i.b - i.a);
}

template<typename E, int N> class homo : public vec<E,N+1> {
	typedef vec<E,N+1>	B;
	typedef pos<E,N>	P;
	using	B::v;
public:
//	using typename B::vec<E,N+1>;
	force_inline operator P() const	{ return project(v); }
} VEC_ATTR;

template<int S, typename E> force_inline homo<E, 2> shift(const homo<E, 2> &x) {
	return perm<S & 1, (S + 1) & 1, 2>(x);
}
template<int S, typename E> force_inline homo<E, 3> shift(const homo<E, 3> &x) {
	return perm<(S + 3) % 3, (S + 4) %3, (S + 5) %3, 3>(x);
}

template<typename E> homo<E,2> operator+(homo<E,2> a, homo<E,2> b)		{ return select(a.z == b.z, a.xyz + b.xyz, a.xyz * b.z + vec<E,3>(b.xy * a.z, zero)); }
template<typename E> homo<E,2> operator-(homo<E,2> a, homo<E,2> b)		{ return select(a.z == b.z, a.xyz - b.xyz, a.xyz * b.z - vec<E,3>(b.xy * a.z, zero)); }
template<typename E> homo<E,2> operator+(homo<E,2> a, pos<E,2> b)		{ return a.xyz + vec<E,3>(b * a.z, zero); }
template<typename E> homo<E,2> operator-(homo<E,2> a, pos<E,2> b)		{ return a.xyz - vec<E,3>(b * a.z, zero); }
template<typename E> bool approx_equal(homo<E,2> a, homo<E,2> b, E tol = ISO_TOLERANCE) { return approx_equal(vec<E,2>(b.xy * a.z), vec<E,2>(a.xy * b.z), tol); }

template<typename E> homo<E,3> operator+(homo<E,3> a, homo<E,3> b)		{ return select(a.w == b.w, a.xyzw + b.xyzw, a.xyzw * b.w + vec<E,4>(b.xyz * a.w, zero)); }
template<typename E> homo<E,3> operator-(homo<E,3> a, homo<E,3> b)		{ return select(a.w == b.w, a.xyzw - b.xyzw, a.xyzw * b.w - vec<E,4>(b.xyz * a.w, zero)); }
template<typename E> homo<E,3> operator+(homo<E,3> a, pos<E,3> b)		{ return a.xyzw + vec<E,4>(b * a.w, zero); }
template<typename E> homo<E,3> operator-(homo<E,3> a, pos<E,3> b)		{ return a.xyzw - vec<E,4>(b * a.w, zero); }
template<typename E> bool approx_equal(homo<E,3> a, homo<E,3> b, E tol = ISO_TOLERANCE) { return approx_equal(vec<E,3>(b.xyz * a.w), vec<E,3>(a.xyz * b.w), tol); }

//-----------------------------------------------------------------------------
//	int1/float1/double1
//-----------------------------------------------------------------------------

typedef vec<int,1>		int1;
typedef vec<uint32,1>	uint1;
typedef vec<float,1>	float1;
#ifdef VEC_DOUBLE
typedef vec<double,1>	double1;
#endif

//-----------------------------------------------------------------------------
//	int2/float2/double2/position2
//-----------------------------------------------------------------------------

typedef vec<int,2>		int2;//, point;
typedef vec<uint32,2>	uint2;
typedef vec<float,2>	float2, vector2;
typedef pos<float,2>	position2;
#ifdef VEC_DOUBLE
typedef vec<double,2>	double2;
#endif

template<> template<> force_inline _v3<float,7,0,1,2>&	_v3<float,7,0,1,2>::operator=(const position2 &t)			{ return operator=((t, one)); }

inline float2 sincos_twice(param_t<float2> sc) {
	return (sc.xx + sc.yx) * (sc.xy - float2(sc.y, zero));
}

inline float2 sincos_half(param_t<float2> sc) {
	float2 h = sqrt(float2(half, -half) * (float2(one, -one) + sc.x));
	h.y = copysign(h.y, sc.y);
	return h;
}

template<typename E> inline int2 rank_sort(vec<E,2> x) {
	int		is	= x.y > x.x;
	return int2(is, 1 - is);
}

inline uint2	split_index(uint32 i, uint32 n) { return uint2(i % n, i / n); }
inline uint32	flat_index(uint2 i, uint32 n)	{ return i.x + n * i.y; }

//-----------------------------------------------------------------------------
//	int3/float3/double3/position3
//	homo2
//-----------------------------------------------------------------------------

typedef vec<int,3>		int3;
typedef vec<uint32,3>	uint3;
typedef vec<float,3>	float3, vector3;
typedef pos<float,3>	position3;
typedef homo<float,2>	homo2;
#ifdef VEC_DOUBLE
typedef vec<double,3>	double3;
#endif

template<> template<> force_inline _v4<float,0,1,2,3>&	_v4<float,0,1,2,3>::operator=(const position3 &t)	{ return operator=((t, one)); }

template<typename E> inline int3 rank_sort(vec<E,3> x) {
	int3	is	= x.yzz > x.xxy;
	return int3(is.x + is.y, 1 - is.x + is.z, 2 - is.y - is.z);
}

inline uint3	split_index(uint32 i, uint2 n)	{ return uint3(i % n.x, (i / n.x) % n.y, i / (n.x * n.y)); }
inline uint32	flat_index(uint3 i, uint2 n)	{ return i.x + n.x * (i.y  + n.y * i.z); }

//-----------------------------------------------------------------------------
//	int4/float4/double4
//	homo3
//-----------------------------------------------------------------------------

typedef vec<int,4>		int4;
typedef vec<uint32,4>	uint4;
typedef vec<float,4>	float4, vector4;
typedef homo<float,3>	homo3;
#ifdef VEC_DOUBLE
typedef vec<double,4>	double4;
#endif

template<typename E> int4 rank_sort(vec<E,4> x) {
	int3	isX		= x.yzw > x.xxx;
	int3	isYZ	= x.zww > x.yyz;

	int4	i;
	i.x		= isX.x + isX.y + isX.z;
	i.yzw	= int3(1) - isX;
	i.y		+= isYZ.x + isYZ.y;
	i.zw	+= int2(1) - isYZ.xy;
	i.z		+= isYZ.z;
	i.w		+= 1 - isYZ.z;
	return i;
}

inline uint4	split_index(uint32 i, uint3 n)	{ return uint4(i % n.x, (i / n.x) % n.y, (i / (n.x * n.y)) % n.z, i / (n.x * n.y * n.z)); }
inline uint32	flat_index(uint4 i, uint3 n)	{ return i.x + n.x * (i.y  + n.y * (i.z + n.z * i.w)); }

//-----------------------------------------------------------------------------
//	colour
//-----------------------------------------------------------------------------

class colour {
public:
	union rgb_t {
		_v1<float,15,0>			r;
		_v1<float,15,1>			g;
		_v1<float,15,2>			b;
		_v3<float,15,0,1,2>		rgb;

		float3	to_hsv() const {
			float	v = max(r, g, b);
			float	c = v - min(r, g, b);
			return	float3(
				c == 0 ? 0.f : (
					v == r ? (g - b) / c
				:	v == g ? (b - r) / c + 2
				:	(r - g) / c + 4
				) / 6.f,
				v == 0 ? 0.f : c / v,
				v
			);
		}
	};
	union hsv_t {
		_v1<float,7,0>			h;
		_v1<float,7,1>			s;
		_v1<float,7,2>			v;
		_v3<float,7,0,1,2>		hsv;

		float3 to_rgb() const {
			float	c = v * s;
			float	m = v - c;
			int		i = int(h * 6);
			float	x = c * (1 - abs(h * 6 - (i | 1))) + m;
			c += m;
			switch (i) {
				case 0:		return float3(c, x, m);
				case 1:		return float3(x, c, m);
				case 2:		return float3(m, c, x);
				case 3:		return float3(m, x, c);
				case 4:		return float3(x, m, c);
				default:	return float3(c, m, x);
			}
		}
	};
	struct _hsv {
		rgb_t			rgb;
		hsv_t			get()			const	{ return force_cast<hsv_t>(rgb.to_hsv()); }
		operator		float3()		const	{ return rgb.to_hsv(); }
		void		operator=(const hsv_t &t)	{ rgb.rgb = t.to_rgb(); }
		template<typename T> void operator=(const T &t)	{ hsv_t x; x.hsv = t; rgb.rgb = x.to_rgb(); }
	};
	template<int I> struct _hsv_comp {
		_hsv			hsv;
		_v1<float,7,I>	get()			const	{ return perm<I>(hsv.get().hsv); }
		operator		float()			const	{ return get(); }
		template<typename T> void operator=(const T &t)	{ hsv_t x = hsv.get(); (_v1<float,7,I>&)x = t; hsv = x; }
	};
	union {
		_v1<float,15,0>			r;
		_v1<float,15,1>			g;
		_v1<float,15,2>			b;
		_v1<float,15,3>			a;
		_v3<float,15,0,1,2>		rgb;
		_v4<float,0,1,2,3>		rgba;
		_hsv					hsv;
		_hsv_comp<0>			h;
		_hsv_comp<1>			s;
		_hsv_comp<2>			v;
	};

	colour()					{}
	explicit colour(uint32 c)	: rgba(float4(int((c >> 24) & 255), int((c >> 16) & 255), int((c >> 8) & 255), int(c & 255)) / 255.f) {}
																force_inline colour(const _v4<float,0,1,2,3> &a)						{ rgba = a; }
	template<typename A>	explicit							force_inline colour(const A &a)										{ rgba = a; }
	explicit													force_inline colour(param_t<float3> a)								{ rgba = (a, one); }
	template<typename E, int N, int A, int B, int C> explicit	force_inline colour(const _v3<E,N,A,B,C> &a)							{ rgba = (a, one); }
	template<typename A, typename B>							force_inline colour(const A &a, const B &b)							{ rgba = pair<A,B>(a, b); }
	template<typename A, typename B, typename C>				force_inline colour(const A &a, const B &b, const C &c)				{ rgba = make_pair(make_pair(pair<A,B>(a, b), c), one); }
	template<typename A, typename B, typename C, typename D>	force_inline colour(const A &a, const B &b, const C &c, const D &d)	{ rgba = make_pair(make_pair(pair<A,B>(a, b), c), d); }

	template<typename T> force_inline colour& operator =(const T &b)	{ rgba  = b; return *this; }
	template<typename T> force_inline colour& operator+=(const T &b)	{ rgba += b; return *this; }
	template<typename T> force_inline colour& operator-=(const T &b)	{ rgba -= b; return *this; }
	template<typename T> force_inline colour& operator*=(const T &b)	{ rgba *= b; return *this; }
	template<typename T> force_inline colour& operator/=(const T &b)	{ rgba /= b; return *this; }
} VEC_ATTR;

template<> struct vget<colour> : vdims<4> {
	typedef _v4<float,0,1,2,3>	R, L;
	typedef	colour				O;
	static force_inline R r(const colour &t)					{ return t.rgba; }
	static force_inline L l(const colour &t)					{ return t.rgba; }
};

force_inline colour blend(param_t<colour> c)		{ return colour(c.rgb * c.a, c.a); }
force_inline colour additive(param_t<colour> c)	{ return colour(c.rgb * c.a, zero); }
inline float srgb_to_linear(float r) 	{ return r <= 0.04045f ? r / 12.92f : r <= 1 ? pow((r + 0.055f) / 1.055f, 2.4f) : r; }
inline float linear_to_srgb(float r) 	{ return r <= 0.0031308f ? r * 12.92f : r <= 1 ? 1.055f * pow(r, (1.0f / 2.4f)) - 0.055f : r; }
inline float3 srgb_to_linear(float3 r) 	{ return select(r <= 0.04045f, r / 12.92f, select(r <= 1, pow((r + 0.055f) / 1.055f, 2.4f), r)); }
inline float3 linear_to_srgb(float3 r) 	{ return select(r <= 0.0031308f, r * 12.92f, select(r <= 1, 1.055f * pow(r, (1.0f / 2.4f)) - 0.055f, r)); }

template<typename T> struct srgb {
	T	t;
	srgb()	{}
	template<typename T2> srgb(const srgb<T2> &b) 	: t(b) {}
	template<typename T2> srgb(const T2 &b) 		: t(linear_to_srgb(b)) {}
	operator colour() { return colour(srgb_to_linear(t)); }
};

/*

struct rect {
	union {
		_v1<int,15,0>			x0;
		_v1<int,15,1>			y0;
		_v1<int,15,2>			x1;
		_v1<int,15,3>			y1;
		_v2<int,15,0,1>			pos0;
		_v2<int,15,2,3>			pos1;
		_v4<int,0,1,2,3>		v;
	};

	rect()	{}
	rect(int x, int y, int w, int h) { v = int4(x, y, x + w, y + h); }
	rect(param(point) pos, param(point) size) { v = int4(pos, pos + size); }
	int				Left()			const	{ return x0; }
	int				Top()			const	{ return y0; }
	int				Width()			const	{ return x1 - x0;	}
	int				Height()		const	{ return y1 - y0;	}
	int				Right()			const	{ return x1;	}
	int				Bottom()		const	{ return y1;	}
	point			Size()			const	{ return pos1 - pos0;	}
	point			TopLeft()		const	{ return pos0; }
	point			BottomRight()	const	{ return pos1; }

	point&			TopLeft()				{ return ((point*)this)[0]; }
	rect&			SetPos(param(point) p)	{ pos0 = p; return *this;	}
	rect&			SetSize(param(point) p)	{ pos1 = pos0 + p; return *this;	}
	rect&			SetPos(int x, int y)	{ return SetPos(point(x, y)); }
	rect&			SetSize(int w, int h)	{ return SetSize(point(w, h)); }

	inline rect& operator+=(param(point) p)	{ v += p.xyxy; return *this; }
	inline rect& operator-=(param(point) p)	{ v -= p.xyxy; return *this; }
	inline rect& operator|=(param(point) p)	{ pos0 = min(pos0, p); pos1 = max(pos1, p); return *this; }
	inline rect& operator|=(param(rect) r)	{ pos0 = min(pos0, r.pos0); pos1 = max(pos1, r.pos1); return *this; }
	friend rect operator+(param(rect) r, param(point) p)	{ return rect(r) += p; }
	friend rect operator-(param(rect) r, param(point) p)	{ return rect(r) -= p; }
	friend rect operator|(param(rect) r, param(point) p)	{ return rect(r) |= p; }
	friend rect operator|(param(rect) r1, param(rect) r2)	{ return rect(r1)|= r2; }
};
*/

//-----------------------------------------------------------------------------
// polynomials
//-----------------------------------------------------------------------------

template<int N> struct polynomial : vec<float, N + 1> {
	template<typename C> polynomial(C c) : vec<float, N + 1>(c) {}
	template<typename T> T		eval(const T &x)			const;
	int							roots(vec<float, N> &r)		const;
	template<typename T> inline typename T_enable_if<mat_traits<T>::is_mat>::type eval(const T &t) const {
		return matrix_polynomial(*this % t.characteristic(), t);
	}
};
template<int N> struct normalised_polynomial : vec<float, N> {
	template<typename C> normalised_polynomial(C c) : vec<float, N>(c) {}
	template<typename T> T	eval(const T &x)			const;
	template<typename T> inline typename T_enable_if<mat_traits<T>::is_mat>::type eval(const T &t) const {
		return matrix_polynomial(*this % t.characteristic(), t);
	}
	int							roots(vec<float, N> &r)		const;
};

template<int N, typename T> inline T matrix_polynomial(const normalised_polynomial<N> &r, const T &t);

template<int N> normalised_polynomial<N> normalise(const polynomial<N> &p) {
	return project(p);
}

template<> struct polynomial<4> : polynomial<3> {
	float	c0;
	polynomial(float c0, param_t<float4> c) : polynomial<3>(c), c0(c0) {}
	template<typename T> T		eval(const T &t)			const	{ return polynomial<3>::eval(t) * t + c0; }
	int							roots(float4 &r)			const;
};
template<> inline normalised_polynomial<4> normalise(const polynomial<4> &p) {
	return float4(float4(p.c0, p.xyz) / p.w);
}

template<> struct polynomial<5>	: float4 {
	float	c0, c1;
	polynomial(float c0, float c1, param_t<float4> c) : float4(c), c0(c0), c1(c1) {}
	int							roots(float4 &r)			const;
};

template<> struct normalised_polynomial<5>	: normalised_polynomial<4> {
	float	c0;
	normalised_polynomial(float c0, param_t<float4> c) : normalised_polynomial<4>(c), c0(c0) {}
	template<typename T> T		eval(const T &t)			const	{ return normalised_polynomial<4>::eval(t) * t + c0; }
	int							roots(float4 &r)			const;
};
template<> inline normalised_polynomial<5> normalise(const polynomial<5> &p) {
	return normalised_polynomial<5>(p.c0 / p.w, float4(p.c1, p.xyz) / p.w);
}

template<> template<typename T> inline T polynomial<1>::eval(const T &t) const { return t * y + x; }
template<> template<typename T> inline T polynomial<2>::eval(const T &t) const { return t * (t * z + y) + x; }
template<> template<typename T> inline T polynomial<3>::eval(const T &t) const { return t * (t * (t * w + z) + y) + x; }

template<> template<typename T> inline T normalised_polynomial<1>::eval(const T &t) const { return t + x; }
template<> template<typename T> inline T normalised_polynomial<2>::eval(const T &t) const { return t * (t + y) + x; }
template<> template<typename T> inline T normalised_polynomial<3>::eval(const T &t) const { return t * (t * (t + z) + y) + x; }
template<> template<typename T> inline T normalised_polynomial<4>::eval(const T &t) const { return t * (t * (t * (t + w) + z) + y) + x; }

inline polynomial<0>			deriv(const polynomial<1> &f)						{ return float1(f.y); }
inline polynomial<1>			deriv(const polynomial<2> &f)						{ return f.yz * float2(1,2); }
inline polynomial<2>			deriv(const polynomial<3> &f)						{ return f.yzw * float3(1,2,3); }
inline polynomial<3>			deriv(const polynomial<4> &f)						{ return f.xyzw * float4(1,2,3,4); }
inline polynomial<4>			deriv(const polynomial<5> &f)						{ return polynomial<4>(f.c1, f.xyzw * float4(2,3,4,5)); }

inline polynomial<0>			deriv(const normalised_polynomial<1> &f)			{ return one; }
inline polynomial<1>			deriv(const normalised_polynomial<2> &f)			{ return float2(f.y, 2); }
inline polynomial<2>			deriv(const normalised_polynomial<3> &f)			{ return float3(f.yz * float2(1,2), 3); }
inline polynomial<3>			deriv(const normalised_polynomial<4> &f)			{ return float4(f.yzw * float3(1,2,3), 4); }
inline polynomial<4>			deriv(const normalised_polynomial<5> &f)			{ return polynomial<4>(f.x, float4(f.yzw * float3(2,3,4), 5)); }

inline normalised_polynomial<1>	normalised_deriv(const normalised_polynomial<2> &f) { return f.y * half; }
inline normalised_polynomial<2>	normalised_deriv(const normalised_polynomial<3> &f) { return f.yz * float2(1.f / 3, 2.f / 3); }
inline normalised_polynomial<3>	normalised_deriv(const normalised_polynomial<4> &f) { return f.yzw * float3(1.f / 4, 2.f / 4, 3.f / 4); }
inline normalised_polynomial<4>	normalised_deriv(const normalised_polynomial<5> &f)	{ return f.xyzw * float4(1, 2, 3, 4) / 5; }

//halley's method to refine roots
template<int N, typename T> T halley(const normalised_polynomial<N> &poly, const T &x) {
	auto	poly1	= deriv(poly);
	auto	poly2	= deriv(poly1);
	T		f		= poly.eval(x);
	T		f1		= poly1.eval(x);
	T		f2		= poly2.eval(x);
	return x - (f * f1 * two) / (f1 * f1 * two - f * f2);
}

template<int N, int D> normalised_polynomial<D> operator%(const normalised_polynomial<N> &num, const normalised_polynomial<D> &div);

//-----------------------------------------------------------------------------
//	matrices
//-----------------------------------------------------------------------------

template<typename E, int N, int M>	class mat;
template<typename E, int N>			class diagonal;
template<typename E, int N>			class upper;
template<typename E, int N>			class lower;
template<typename E, int N>			class symmetrical;

template<typename E, int N, int M>	struct mat_traits<mat<E,N,M>>			{ enum {is_mat = true}; };
template<typename E, int N>			struct mat_traits<upper<E,N>>			{ enum {is_mat = true}; };
template<typename E, int N>			struct mat_traits<lower<E,N>>			{ enum {is_mat = true}; };
template<typename E, int N>			struct mat_traits<diagonal<E,N>>		{ enum {is_mat = true}; };
template<typename E, int N>			struct mat_traits<symmetrical<E,N>>		{ enum {is_mat = true}; };

template<typename E, int N, int M, int L>	struct mul_t<mat<E,N,M>, mat<E,M,L>>	{ typedef mat<E,N,L>	type; };

template<typename E>				struct mul_t<mat<E,2,2>, mat<E,2,3>>			{ typedef mat<E,2,3>	type; };
template<typename E>				struct mul_t<mat<E,2,3>, mat<E,2,2>>			{ typedef mat<E,2,3>	type; };
template<typename E>				struct mul_t<mat<E,2,3>, mat<E,2,3>>			{ typedef mat<E,2,3>	type; };
//template<typename E, typename B>	struct mul_t<mat<E,2,3>, B, typename T_void<typename mul_t<mat<E,3,3>, B>::type>::type>	: mul_t<mat<E,3,3>, B> {};
template<typename E>				struct mul_t<mat<E,2,3>, mat<E,3,3>>			{ typedef mat<E,3,3>	type; };
template<typename E>				struct mul_t<mat<E,3,3>, mat<E,2,3>>			{ typedef mat<E,3,3>	type; };

template<typename E>				struct mul_t<mat<E,3,3>, mat<E,3,4>>			{ typedef mat<E,3,4>	type; };
template<typename E>				struct mul_t<mat<E,3,4>, mat<E,3,3>>			{ typedef mat<E,3,4>	type; };
template<typename E>				struct mul_t<mat<E,3,4>, mat<E,3,4>>			{ typedef mat<E,3,4>	type; };
//template<typename E, typename B>	struct mul_t<mat<E,3,4>, B, T_void<mul_t<mat<E,4,4>, B>>>	: mul_t<mat<E,4,4>, B> {};
template<typename E>				struct mul_t<mat<E,3,4>, mat<E,4,4>>			{ typedef mat<E,4,4>	type; };
template<typename E>				struct mul_t<mat<E,4,4>, mat<E,3,4>>			{ typedef mat<E,4,4>	type; };

template<typename E, int N>			struct mul_t<mat<E,N,N>, symmetrical<E,N>>		{ typedef mat<E,N,N>	type; };
template<typename E, int N>			struct mul_t<symmetrical<E,N>, mat<E,N,N>>		{ typedef mat<E,N,N>	type; };

template<typename E, int N>			struct mul_t<symmetrical<E,N>, symmetrical<E,N>>{ typedef symmetrical<E,N>	type; };
template<typename E, int N>			struct mul_t<diagonal<E,N>, diagonal<E,N>>		{ typedef diagonal<E,N>	type; };

template<typename E, int N, int M>	struct mul_t<mat<E,N,M>, vec<E,N>>				{ typedef vec<E,M>		type; };
template<typename E, int N>			struct mul_t<mat<E,N,N>, pos<E,N-1>>			{ typedef vec<E,N>		type; };
template<typename E, int N>			struct mul_t<mat<E,N,N>, pos<E,N>>				{ typedef pos<E,N>		type; };
template<typename E, int N>			struct mul_t<mat<E,N,N+1>, pos<E,N>>			{ typedef pos<E,N>		type; };
template<typename E, int N>			struct mul_t<mat<E,N,N+1>, vec<E,N>>			{ typedef vec<E,N>		type; };
template<typename E, int N>			struct mul_t<mat<E,N,N>, vec<E,N+1>>			{ typedef vec<E,N+1>	type; };

template<typename E, int N>				force_inline	vec<E,N>	vmul(const mat<E,N,2> &a, const _one&)		{ return a.x + a.y; }
template<typename E, int N>				force_inline	vec<E,N>	vmul(const mat<E,N,3> &a, const _one&)		{ return a.x + a.y + a.z; }
template<typename E, int N>				force_inline	vec<E,N>	vmul(const mat<E,N,4> &a, const _one&)		{ return a.x + a.y + a.z + a.w; }
template<typename E, int N, typename B>	force_inline	vec<E,N>	vmul(const mat<E,N,N> &a, const B &b)		{ return a * vec<E,N>(b); }

//template<typename E, int N, typename B>		struct mul_t<mat<E, N, N>, B, IF_MAT(B,void)>	{ typedef mat<E, N, N>	type; };

template<typename T> struct transpose_s {
	const T m;
	explicit transpose_s(const T &t)	: m(t) {}
	operator	T()		const			{ return get_transpose(m); }
	friend T	get(transpose_s &&t)	{ return t; }
};
template<typename T> struct transpose_s<T&> {
	const T &m;
	explicit transpose_s(const T &t)	: m(t) {}
	operator	T()		const			{ return get_transpose(m); }
	friend T	get(transpose_s &&t)	{ return t; }
};
template<typename E, int N, int M> struct transpose_s<mat<E,N,M>> {
	typedef mat<E,N,M> T;
	typedef mat<E,M,N> R;
	const T m;
	explicit transpose_s(const T &t)	: m(t) {}
	operator	R()		const			{ return get_transpose(m); }
	friend R	get(transpose_s &&t)	{ return t; }
};

template<typename T> struct inverse_s	{
	const T m;
	explicit inverse_s(const T &t)	: m(t) {}
	operator	T()		const		{ return get_inverse(m); }
	friend T	get(inverse_s &&t)	{ return t; }
};
template<typename T> struct inverse_s<T&>	{
	const T &m;
	explicit inverse_s(const T &t)	: m(t) {}
	operator	T()		const		{ return get_inverse(m); }
	friend T	get(inverse_s &&t)	{ return t; }
};
template<typename T> struct inverse_s<transpose_s<T> > {
	const T m;
	explicit inverse_s(const transpose_s<T> &t)	: m(t.m) {}
	operator	T()		const		{ return get_inverse_transpose(m); }
	friend T	get(inverse_s &&t)	{ return t; }
};
template<typename T> struct inverse_s<transpose_s<T&> > {
	const T &m;
	explicit inverse_s(const transpose_s<T&> &t) : m(t.m) {}
	operator	T()		const		{ return get_inverse_transpose(m); }
	friend T	get(inverse_s &&t)	{ return t; }
};

template<typename T> struct mat_traits<inverse_s<T> >				{ enum {is_mat = true}; };
template<typename T> struct mat_traits<transpose_s<T> >				{ enum {is_mat = true}; };
template<typename T> struct mat_traits<inverse_s<transpose_s<T> > >	{ enum {is_mat = true}; };

template<typename T> force_inline auto	inverse(const T &m)								{ return inverse_s<T&>(m); }
template<typename T> force_inline auto	inverse(T &&m)									{ return inverse_s<T>(m); }
template<typename T> force_inline auto	transpose(const T &m)							{ return transpose_s<T&>(m); }
template<typename T> force_inline auto	transpose(T &&m)								{ return transpose_s<T>(m); }
template<typename T> force_inline auto	inverse(const inverse_s<T> &i)					{ return i.m; }
template<typename T> force_inline auto	inverse(inverse_s<T> &&i)						{ return i.m; }
template<typename T> force_inline auto	inverse(const transpose_s<T> &m)				{ return inverse_s<transpose_s<T>>(m); }
template<typename T> force_inline auto	inverse(transpose_s<T> &&m)						{ return inverse_s<transpose_s<T>>(m); }
template<typename T> force_inline auto	transpose(const transpose_s<T> &i)				{ return i.m; }
template<typename T> force_inline auto	transpose(transpose_s<T> &&i)					{ return i.m; }
template<typename T> force_inline auto	transpose(const inverse_s<T> &i)				{ return inverse(transpose(i.m)); }
template<typename T> force_inline auto	transpose(inverse_s<T> &&i)						{ return inverse(transpose(i.m)); }
template<typename T> force_inline auto	transpose(const inverse_s<transpose_s<T> > &i)	{ return inverse(i.m); }
template<typename T> force_inline auto	transpose(inverse_s<transpose_s<T> > &&i)		{ return inverse(i.m); }

template<typename A, typename B> struct mul_t<inverse_s<A>, inverse_s<B> >		{ typedef inverse_s<typename mul_noref_t<B,A>::type> type; };
template<typename A, typename B> struct mul_t<transpose_s<A>, transpose_s<B> >	{ typedef transpose_s<typename mul_noref_t<B,A>::type> type; };
template<typename A, typename B> struct mul_t<inverse_s<A>, B>		: mul_noref_t<A,B> {};
template<typename A, typename B> struct mul_t<transpose_s<A>, B>	: mul_noref_t<A,B> {};
template<typename A, typename B> struct mul_t<A, inverse_s<B> >		: mul_noref_t<A,B> {};
template<typename A, typename B> struct mul_t<A, transpose_s<B> >	: mul_noref_t<A,B> {};

#if 0
template<typename A, typename B> force_inline typename mul_noref_t<A,B>::type				operator*(const A &a, const inverse_s<B> &b)								{ return a * get_inverse(b.m); }
template<typename A, typename B> force_inline typename mul_noref_t<A,B>::type				operator*(const A &a, const transpose_s<B> &b)								{ return a * get_transpose(b.m); }
template<typename A, typename B> force_inline typename mul_noref_t<A,B>::type				operator*(const A &a, const inverse_s<transpose_s<B>> &b)					{ return a * get_inverse_transpose(b.m); }

template<typename A, typename B> force_inline typename mul_noref_t<A,B>::type				operator*(const inverse_s<A> &a, const B &b)								{ return get_inverse(a.m) * b; }
template<typename A, typename B> force_inline inverse_s<typename mul_noref_t<B,A>::type>		operator*(const inverse_s<A> &a, const inverse_s<B> &b)						{ return inverse(b.m * a.m); }
template<typename A, typename B> force_inline typename mul_noref_t<A,B>::type				operator*(const inverse_s<A> &a, const transpose_s<B> &b)					{ return get_inverse(a.m) * get_transpose(b.m); }
template<typename A, typename B> force_inline inverse_s<typename mul_noref_t<B,A>::type>		operator*(const inverse_s<A> &a, const inverse_s<transpose_s<B>> &b)		{ return inverse(transpose(b.m) * a.m); }

template<typename A, typename B> force_inline typename mul_noref_t<A,B>::type				operator*(const transpose_s<A> &a, const B &b)								{ return get_transpose(a.m) * b; }
template<typename A, typename B> force_inline typename mul_noref_t<A,B>::type				operator*(const transpose_s<A> &a, const inverse_s<B> &b)					{ return get_transpose(a.m) * get_inverse(b.m); }
template<typename A, typename B> force_inline transpose_s<typename mul_noref_t<B,A>::type>	operator*(const transpose_s<A> &a, const transpose_s<B> &b)					{ return transpose(b.m * a.m); }
template<typename A, typename B> force_inline transpose_s<typename mul_noref_t<B,A>::type>	operator*(const transpose_s<A> &a, const inverse_s<transpose_s<B>> &b)		{ return transpose(inverse(b) * a.m); }

template<typename A, typename B> force_inline transpose_s<typename mul_noref_t<B,A>::type>	operator*(const inverse_s<transpose_s<A>> &a, const B &b)					{ return transpose(transpose(b) * inverse(a.m)); }//get_inverse_transpose(a.m) * b; }
template<typename A, typename B> force_inline inverse_s<typename mul_noref_t<B,A>::type>		operator*(const inverse_s<transpose_s<A>> &a, const inverse_s<B> &b)		{ return inverse(b.m * transpose(a.m)); }
template<typename A, typename B> force_inline transpose_s<typename mul_noref_t<B,A>::type>	operator*(const inverse_s<transpose_s<A>> &a, const transpose_s<B> &b)		{ return transpose(b.m * get_inverse(a.m)); }
template<typename A, typename B> force_inline inverse_s<typename mul_noref_t<A,B>::type>		operator*(const inverse_s<transpose_s<A>> &a, const inverse_s<transpose_s<B>> &b)	{ return inverse(transpose(a.m * b.m)); }
#else
template<typename A, typename B> force_inline auto	operator*(const A &a, const inverse_s<B> &b)								{ return a * get_inverse(b.m); }
template<typename A, typename B> force_inline auto	operator*(const A &a, const transpose_s<B> &b)								{ return a * get_transpose(b.m); }
template<typename A, typename B> force_inline auto	operator*(const A &a, const inverse_s<transpose_s<B>> &b)					{ return a * get_inverse_transpose(b.m); }

template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const B &b)								{ return get_inverse(a.m) * b; }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const inverse_s<B> &b)						{ return inverse(b.m * a.m); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const transpose_s<B> &b)					{ return get_inverse(a.m) * get_transpose(b.m); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const inverse_s<transpose_s<B>> &b)		{ return inverse(transpose(b.m) * a.m); }

template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const B &b)								{ return get_transpose(a.m) * b; }
template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const inverse_s<B> &b)					{ return get_transpose(a.m) * get_inverse(b.m); }
template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const transpose_s<B> &b)					{ return transpose(b.m * a.m); }
template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const inverse_s<transpose_s<B>> &b)		{ return transpose(inverse(b) * a.m); }

template<typename A, typename B> force_inline transpose_s<decltype(declval<IF_MAT(B,B)>() * declval<A>())>	operator*(const inverse_s<transpose_s<A>> &a, const B &b) {
	return transpose(transpose(b) * inverse(a.m));	//get_inverse_transpose(a.m) * b; }
}
template<typename A, typename B> force_inline auto	operator*(const inverse_s<transpose_s<A>> &a, const inverse_s<B> &b)		{ return inverse(b.m * transpose(a.m)); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<transpose_s<A>> &a, const transpose_s<B> &b)		{ return transpose(b.m * get_inverse(a.m)); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<transpose_s<A>> &a, const inverse_s<transpose_s<B>> &b)	{ return inverse(transpose(a.m * b.m)); }
#endif

//template<typename A, typename B> force_inline typename T_enable_if<mat_traits<B>::is_mat, typename mul_noref_t<B,A>::type>::type operator/(const A &a, const B &b)		{ return inverse(b) * a; }
//template<typename A, typename B> force_inline typename mul_noref_t<IF_MAT(B,B), A>::type operator/(const A &a, const B &b)				{ return inverse(b) * a; }
template<typename A, typename B> force_inline decltype(declval<IF_MAT(B,B)>() * declval<A>()) operator/(const A &a, const B &b)				{ return inverse(b) * a; }
//template<typename A, typename B> force_inline typename mul_noref_t<typename T_enable_if<mat_traits<B>::is_mat, B>::type, typename T_enable_if<!mat_traits<A>::is_mat, A>::type>::type operator*(const A &a, const B &b)		{ return transpose(b) * a; }
//template<typename A, typename B> force_inline typename mul_noref_t<IF_MAT(B,B), IF_NOTMAT(A,A)>::type operator*(const A &a, const B &b)	{ return transpose(b) * a; }
template<typename A, typename B> force_inline decltype(declval<IF_MAT(B,B)>() * declval<IF_NOTMAT(A,A)>()) operator*(const A &a, const B &b)	{ return transpose(b) * a; }

template<typename E, int N, int M> inline bool approx_equal(const mat<E,N,M> &a, const mat<E,N,M> &b, E tol = ISO_TOLERANCE) {
	for (int i = 0; i < M; i++) {
		if (!approx_equal(a[i], b[i], tol))
			return false;
	}
	return true;
}

template<typename E, int N, int M> bool bigger(const mat<E,N,M> &a, const mat<E,N,M> &b) {
	for (int i = 0; i < M; i++)
		if (!bigger(a[i], b[i]))
			return false;
	return true;
}

template<typename E, int N, int M> bool find_scalar_multiple(const mat<E,N,M> &a, const mat<E,N,M> &b, float &scale) {
	bool	got	= false;
	for (int i = 0; i < M; i++) {
		if (!got) {
			if (!find_scalar_multiple(a[i], b[i], scale))
				return false;
			got = scale != 0;
		} else if (!is_scalar_multiple(a[i], b[i], scale)) {
			return false;
		}
	}
	return true;
}

template<typename E, int N, int M> bool is_scalar_multiple(const mat<E,N,M> &a, const mat<E,N,M> &b, float scale) {
	for (int i = 0; i < M; i++) {
		if (!is_scalar_multiple(a[i], b[i], scale))
			return false;
	}
	return true;
}

template<typename E, int N, int M> auto begin(const mat<E,N,M> &a)	{ return make_indexed_iterator(a, int_iterator<int>(0)); }
template<typename E, int N, int M> auto end(const mat<E,N,M> &a)	{ return make_indexed_iterator(a, int_iterator<int>(M)); }

template<typename E, int N> diagonal<E,N> diagonalise(const symmetrical<E,N> &s) {
	vec<E,N>	r;
	int	n = s.characteristic().roots(r);
	return diagonal<E,N>(r);
}

//-----------------------------------------------------------------------------
//	generic mats
//-----------------------------------------------------------------------------

template<typename E, int N, int M, size_t... I>auto _get_transpose(const mat<E, N, M> &m, index_list<I...>) {
	return mat<E, M, N>(m.row(I)...);
}

template<typename E, int A, int B, int C, size_t... I>	auto _mul(const mat<E, B, A> &a, const mat<E, A, C> &b, index_list<I...>) {
	return mat<E, B, C>(a * b.column(I)...);
}

template<typename E, int N> class mat<E,N,2> {
public:
	vec<E,N>	x, y;

	force_inline mat(const _identity&)				{ vec<E,N> t(one, vec<E, N-1>(zero)); x = t; y = t << 1; }
	force_inline mat(const _zero&)					: x(zero), y(zero)			{}
	force_inline mat(const mat &m)					: x(m.x), y(m.y)			{}
	force_inline explicit mat(const mat<E,N-1,2> &m)	: x(m.x,zero), y(m.y,one)	{}
	template<typename X, typename Y> force_inline mat(const X &x, const Y &y) : x(x), y(y)	{}

	const vec<E, N>&	column(int i)	const	{ return (&x)[i]; }
	vec<E, 2>			row(int i)		const	{ return {x[i], y[i]}; }

	friend bool operator==(const mat &a, const mat &b)	{ return a.x == b.x && a.y == b.y; }
	friend bool operator!=(const mat &a, const mat &b)	{ return a.x != b.x || a.y != b.y; }
	friend mat operator+(const mat &a, const mat &b)	{ return mat(a.x + b.x, a.y + b.y); }
	friend mat operator-(const mat &a, const mat &b)	{ return mat(a.x - b.x, a.y - b.y); }

	friend mat<E,2,N> get_transpose(const mat &m) { return _get_transpose(m, meta::make_index_list<N>()); }
};

template<typename E, int N> class mat<E,N,3> {
public:
	vec<E,N>	x, y, z;

	force_inline mat(const _identity&)				{ vec<E,N> t(one, vec<E, N-1>(zero)); x = t; y = t << 1; z = t << 2; }
	force_inline mat(const _zero&)					: x(zero), y(zero), z(zero)		{}
	force_inline mat(const mat &m)					: x(m.x), y(m.y), z(m.z)		{}
	force_inline explicit mat(const mat<E,N-1,2> &m) : x(m.x,zero), y(m.y,zero), z(vec<E,N-1>(zero), one)	{}
	force_inline explicit mat(const mat<E,N-1,3> &m, param(float3) z = float3(z_axis))	: x(m.x, z.x), y(m.y, z.y), z(m.z, z.z)	{}
	template<typename X, typename Y, typename Z> force_inline mat(const X &x, const Y &y, const Z &z) : x(x), y(y), z(z)	{}

	const vec<E, N>&	column(int i)	const	{ return (&x)[i]; }
	vec<E, 3>			row(int i)		const	{ return {x[i], y[i], z[i]}; }

	friend bool operator==(const mat &a, const mat &b)	{ return a.x == b.x && a.y == b.y && a.z == b.z; }
	friend bool operator!=(const mat &a, const mat &b)	{ return a.x != b.x || a.y != b.y || a.z != b.z; }
	friend mat operator+(const mat &a, const mat &b)	{ return mat(a.x + b.x, a.y + b.y, a.z + b.z); }
	friend mat operator-(const mat &a, const mat &b)	{ return mat(a.x - b.x, a.y - b.y, a.z - b.z); }

	friend mat<E,3,N> get_transpose(const mat &m) { return _get_transpose(m, meta::make_index_list<N>()); }
};

template<typename E, int N> class mat<E,N,4> {
public:
	vec<E,N>	x, y, z, w;

	force_inline mat(const _identity&)				{ vec<E,N> t(one, vec<E, N-1>(zero)); x = t; y = t << 1; z = t << 2; w = t << 3; }
	force_inline mat(const _zero&)					: x(zero), y(zero), z(zero), w(zero)	{}
	force_inline mat(const mat &m)					: x(m.x), y(m.y), z(m.z), w(m.w)		{}
	force_inline explicit mat(const mat<E,N-1,3> &m) : x(m.x,zero), y(m.y,zero), z(m.z,zero), w(vec<E,N-1>(zero), one)	{}
	force_inline explicit mat(const mat<E,N-1,4> &m, param(float4) w = float4(w_axis))	: x(m.x, w.x), y(m.y, w.y), z(m.z, w.z), w(m.w, w.w)	{}
	template<typename X, typename Y, typename Z, typename W> force_inline mat(const X &x, const Y &y, const Z &z, const W &w) : x(x), y(y), z(z), w(w) {}

	const vec<E, N>&	column(int i)	const	{ return (&x)[i]; }
	vec<E, 4>			row(int i)		const	{ return {x[i], y[i], z[i], w[i]}; }

	friend bool operator==(const mat &a, const mat &b)	{ return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
	friend bool operator!=(const mat &a, const mat &b)	{ return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w; }
	friend mat operator+(const mat &a, const mat &b)	{ return mat(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
	friend mat operator-(const mat &a, const mat &b)	{ return mat(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }

	friend mat<E,4,N> get_transpose(const mat &m) { return _get_transpose(m, meta::make_index_list<N>()); }
};

//template<typename E, int A, int B, int C>	struct mul_t<mat<E,B,A>, mat<E,A,C>>	{ typedef mat<E,B,C>	type; };
template<typename E, int A, int B, int C>	mat<E, B, C>	operator*(const mat<E, B, A> &a, const mat<E, A, C> &b) {
	return _mul(a, b, meta::make_index_list<C>());
}


//-----------------------------------------------------------------------------

template<typename E> class diagonal<E,2> {
public:
	vec<E,2>	d2;
	force_inline diagonal()	{}
	template<typename D> explicit force_inline diagonal(const D &d) : d2(d)	{}

	force_inline vec<E,1>	det()							const	{ return prod_components(d2); }
	force_inline vec<E,1>	trace()							const	{ return sum_components(d2); }
	force_inline normalised_polynomial<2> characteristic()	const	{ return float2(det(), -trace()); }
	force_inline vec<E,2>	column(int i)					const	{ return select(uint8(1 << i), d2, zero); }

	template<typename T> friend IF_SCALAR(T,diagonal) operator*(const diagonal &a, const T &b)	{ return diagonal(a.d2 * b); }
	template<typename T> friend IF_SCALAR(T,diagonal) operator/(const diagonal &a, const T &b)	{ return a * reciprocal(b); }
	friend diagonal			operator-(const diagonal &a)						{ return diagonal(-a.d2); }
	friend diagonal			operator+(const diagonal &a, const diagonal &b)		{ return diagonal(a.d2 + b.d2); }
	friend diagonal			operator-(const diagonal &a, const diagonal &b)		{ return diagonal(a.d2 - b.d2); }
	friend const diagonal&	transpose(const diagonal& m)						{ return m; }
	friend diagonal&&		transpose(diagonal&& m)								{ return (diagonal&&)m; }
	friend diagonal			get_inverse(const diagonal &m)						{ return diagonal(reciprocal(m.d2)); }
};
typedef diagonal<float,2> diagonal2;

template<typename E> struct _triangular2 {
	vec<E,3>	o;
	force_inline _triangular2() {}
	template<typename O> explicit force_inline _triangular2(const O &o)					: o(o)		{}
	template<typename D, typename O> force_inline _triangular2(const D &d, const O &o)	: o(d, o)	{}

	force_inline	vec<E,2>	diagonal()						const	{ return o.xy; }
	force_inline	vec<E,1>	det()							const	{ return o.x * o.y; }
	force_inline	vec<E,1>	trace()							const	{ return o.x + o.y; }
	force_inline normalised_polynomial<2> characteristic()	const	{ return vec<E,2>(det(), -trace()); }

protected:
	_triangular2		_adj()								const	{ return _triangular2(o.yxz); }
	_triangular2		_abs()								const	{ return _triangular2(abs(o)); }
	_triangular2		_neg()								const	{ return _triangular2(-o); }
	_triangular2		_add(const _triangular2 &b)			const	{ return _triangular2(o + b.o); }
	_triangular2		_sub(const _triangular2 &b)			const	{ return _triangular2(o - b.o); }
	template<typename T> _triangular2 _mul(const T &b)		const	{ return _triangular2(o * b); }
};

template<typename E> class lower<E,2> : public _triangular2<E> {
	typedef _triangular2<E>	B;
public:
	force_inline lower() {}
	force_inline lower(const _triangular2<E> &t)					: B(t) {}
	template<typename O> explicit force_inline lower(const O &o)	: B(o) {}

	force_inline	vec<E,2>		column(int i) const { return i & 1 ? vec<E,2>(zero, B::o.y) : vec<E,2>(B::o.xz); }

	template<typename T> friend IF_SCALAR(T,lower) operator*(const lower &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,lower) operator/(const lower &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend lower				operator~(const lower &m)					{ return m._adj(); }
	friend lower				operator-(const lower &m)					{ return m._neg(); }
	friend lower				operator+(const lower &a, const lower &b)	{ return a._add(b); }
	friend lower				operator-(const lower &a, const lower &b)	{ return a._sub(b); }
	friend lower				get_inverse(const lower &a)					{ return ~a * reciprocal(a.det()); }
	friend lower				abs(const lower &a)							{ return a._abs(); }
	friend const upper<E,2>&	transpose(const lower &a)					{ return reinterpret_cast<const upper<E,2>&>(a); }
	friend upper<E,2>&&			transpose(lower &&a)						{ return reinterpret_cast<upper<E,2>&&>(a); }
	friend upper<E,2>			cofactors(const lower &a);
};
typedef lower<float,2> lower2;

template<typename E> class upper<E,2> : public _triangular2<E> {
	typedef _triangular2<E>	B;
public:
	force_inline upper()						{}
	force_inline upper(const _triangular2<E> &t)					: B(t) {}
	template<typename O> explicit force_inline upper(const O &o)	: B(o) {}

	force_inline	vec<E,2>		column(int i) const { return i & 1 ? vec<E,2>(B::o.yz) : vec<E,2>(B::o.x, zero); }

	template<typename T> friend IF_SCALAR(T,upper) operator*(const upper &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,upper) operator/(const upper &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend upper				operator~(const upper &a)					{ return a._adj(); }
	friend upper				operator-(const upper &a)					{ return a._neg(); }
	friend upper				operator+(const upper &a, const upper &b)	{ return a._add(b); }
	friend upper				operator-(const upper &a, const upper &b)	{ return a._sub(b); }
	friend upper				get_inverse(const upper &a)					{ return ~a * reciprocal(a.det()); }
	friend upper				abs(const upper &a)							{ return a._abs(); }
	friend const lower<E,2>&	transpose(const upper &a)					{ return reinterpret_cast<const lower<E,2>&>(a); }
	friend lower<E,2>&&			transpose(upper &&a)						{ return reinterpret_cast<lower<E,2>&&>(a); }
	friend lower<E,2>			cofactors(const upper &a)					{ return a._adj(); }
};
typedef upper<float,2> upper2;

template<typename E> inline upper<E,2>	cofactors(const lower<E,2> &m)	{ return m._adj(); }

template<typename E> class symmetrical<E,2> : public _triangular2<E> {
	typedef _triangular2<E>	B;
public:
	force_inline symmetrical()						{}
	force_inline symmetrical(const _triangular2<E> &t)					: B(t) {}
	force_inline symmetrical(const iso::diagonal<E,2> &t)				: B(t.d2, zero) {}
	template<typename O> explicit force_inline symmetrical(const O &o)	: B(o)	{}

	force_inline	vec<E,1>			det()					const	{ return B::o.x * B::o.y - B::o.z * B::o.z; }
	force_inline normalised_polynomial<2> characteristic()	const	{ return vec<E,2>(det(), -B::trace()); }
	force_inline	vec<E,2>			column(int i)			const	{ return i & 1 ? vec<E,2>(B::o.zy) : vec<E,2>(B::o.xz); }

	template<typename T> friend IF_SCALAR(T,symmetrical) operator*(const symmetrical &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,symmetrical) operator/(const symmetrical &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend symmetrical			operator~(const symmetrical &a)							{ return a._adj(); }
	friend symmetrical			operator-(const symmetrical &a)							{ return a._neg(); }
	friend symmetrical			operator+(const symmetrical &a, const symmetrical &b)	{ return a._add(b); }
	friend symmetrical			operator-(const symmetrical &a, const symmetrical &b)	{ return a._sub(b); }
	friend const symmetrical&	transpose(const symmetrical &m)							{ return m; }
	friend symmetrical&&		transpose(symmetrical &&m)								{ return (symmetrical&&)m; }
	friend symmetrical			get_inverse(const symmetrical &a)						{ return ~a * reciprocal(a.det()); }
	friend symmetrical			abs(const symmetrical &a)								{ return a._abs(); }
	friend int					max_component_index(const symmetrical &a)				{ return max_component_index(a.o); }
};

typedef symmetrical<float,2> symmetrical2;

template<typename E> vec<E,2> principal_component(const symmetrical<E,2> &s) {
	vec<E,2>	r;
	int			n = s.characteristic().roots(r);

	if (n == 1)
		return vec<E,2>(one); // only one root, which implies we have a multiple of the identity

	if (n < 0) {
		//2 imaginary
		symmetrical<E,2>	m	= s - diagonal<E,2>(r.y);
		switch (max_component_index(abs(m))) {	// pick the first eigenvector based on biggest index
			default:return vec<E,2>(-m.o.z, zero);
			case 1: return vec<E,2>(zero, -m.o.y);
		}
	}

	vec<E,1>			e	= vec<E,1>(max_component(abs(r)));
	symmetrical<E,2>	m	= s - diagonal<E,2>(e);
	symmetrical<E,2>	u	= ~m;
	return u.column(max_component_index(abs(u)));
}

template<typename E, int N, int A, int B> force_inline symmetrical<E,2> outer_product(const _v2<E,N,A,B> &v) {
	return symmetrical<E,2>(perm<0,1,0>(v) * perm<0,1,1>(v));
}
template<typename E> symmetrical<E,2> outer_product(param_t<vec<E,2>> &v) {
	return v.xyx * v.xyy;
}

//-----------------------------------------------------------------------------

template<typename E> class mat<E,2,2> {
	typedef vec<E,4>	v4;
public:
	union {
		_v4<E,0,1,2,3>	v;
		_v2<E,15,0,1>	x;
		_v2<E,15,2,3>	y;
		_v1<E,15,0>		xx;
		_v1<E,15,1>		xy;
		_v1<E,15,2>		yx;
		_v1<E,15,3>		yy;
	};
	force_inline mat()						{}
	force_inline mat(const _zero&)			: v(zero)	{}
	force_inline mat(const _identity&)		{ v = perm<1,0,0,1>(zero, one); }
	force_inline mat(const mat &a)			{ v = a.v; }
	force_inline mat(const diagonal2 &m)		{ v = (m.d2.x, zero, zero, m.d2.y); }
	force_inline mat(const upper2 &m)		{ v = (m.o.x, zero, m.o.yz); }
	force_inline mat(const lower2 &m)		{ v = (m.o.xz, zero, m.o.y); }
	force_inline mat(const symmetrical2 &m)	{ v = (m.o.xzzy); }
	force_inline mat(param_t<v4> a)			{ v = a; }
	template<int A, int B, int C, int D> 	force_inline mat(const _v4<E,A,B,C,D> &a)	{ v = a; }
	template<typename A, typename B>		force_inline mat(const A &a, const B &b)		{ v = make_pair(vec<E,2>(a), vec<E,2>(b)); }
	force_inline explicit mat(const mat<E,4,4> &m);

	force_inline vec<E,2>	operator[](int i)	const		{ return vec<E,2>((const E*)this + i * 2); }
	force_inline	vec<E,2>	diagonal()			const		{ return perm<0,3>(v); }
	force_inline	vec<E,1>	det()				const		{ return diff(perm<0,2>(v) * perm<3,1>(v)); }
	force_inline	vec<E,1>	trace()				const		{ return sum_components(perm<0,3>(v)); }
	force_inline normalised_polynomial<2> characteristic() const { return vec<E,2>(det(), -trace()); }
	force_inline vec<E,2>	scale()				const		{ vec<E,4> t = v * v; return sqrt(t.xy + t.zw); }
	force_inline vec<E,2>	pre_scale()			const		{ vec<E,4> t = v * v; return sqrt(t.xz + t.yw); }
	force_inline vec<E,1>	euler()				const		{ return atan2(perm<1,0>(v)); }

	force_inline _v2<E,0,0,2>	row0()			const		{ return perm<0,2>(v); }
	force_inline _v2<E,0,1,3>	row1()			const		{ return perm<1,3>(v); }

	friend mat		operator+(const mat &a, const mat &b)	{ return mat(a.v + b.v); }
	friend mat		operator-(const mat &a, const mat &b)	{ return mat(a.v - b.v); }
	template<int N, int A>	friend mat operator*(const mat &a, const _v1<E,N,A> &b)	{ return mat(a.v * b); }
	template<int N, int A>	friend mat operator/(const mat &a, const _v1<E,N,A> &b)	{ return mat(a.v / b); }

	friend mat		cofactors(const mat &m)		{ return mat(perm<3,2,1,0>(m.v) * vec<E,4>(one,-one,-one,one)); }
	friend mat		operator~(const mat &m)		{ return mat(perm<3,1,2,0>(m.v) * vec<E,4>(one,-one,-one,one)); }
	friend mat		get_inverse(const mat &m)	{ return mat((~m).v * reciprocal(m.det())); }
	friend mat		get_transpose(const mat &m)	{ return mat(perm<0,2,1,3>(m.v)); }
};
typedef mat<float,2,2> float2x2;

template<typename E> mat<E,2,2> pow(const mat<E,2,2> &x, int y) {
	auto	r = poly_div(y, 1, x.characteristic());
	return x * r.y + diagonal<E,2>(r.x);
}

template<typename E, int N, int A, int B>	struct mul_t<mat<E,2,2>, _v2<E,N,A,B> > { typedef vec<E,2>	type; };

template<typename E> force_inline vec<E,2>		operator*(const mat<E,2,2> &m, vec<E,2> v)	{ vec<E,4> t = m.v * v.xxyy; return t.xy + t.zw; }
template<typename E> force_inline pos<E,2>		operator*(const mat<E,2,2> &m, pos<E,2> v)	{ vec<E,4> t = m.v * v.xxyy; return t.xy + t.zw; }
template<typename E, int N, int A, int B> force_inline vec<E,2> operator*(const mat<E,2,2> &m, const _v2<E,N,A,B> &v)	{ vec<E,4> t = m.v * perm<0,0,1,1>(v); return t.xy + t.zw; }
template<typename E> force_inline mat<E,2,2>	operator*(const mat<E,2,2> &a, const mat<E,2,2> &b)	{ return mat<E,2,2>(a.v * perm<0,0,3,3>(b.v) + perm<2,3,0,1>(a.v) * perm<1,1,2,2>(b.v)); }

//-----------------------------------------------------------------------------

template<typename E> class mat<E,2,3> : public mat<E,2,2> {
	typedef vec<E,2>	v2;
public:
	pos<E,2>		z;

	force_inline mat()																{}
	force_inline mat(const _identity &i)					: mat<E,2,2>(i), z(zero)	{}
	force_inline mat(const _zero &i)						: mat<E,2,2>(i), z(zero)	{}
	force_inline mat(const mat<E,2,2> &m)				: mat<E,2,2>(m), z(zero)	{}
	force_inline mat(const mat<E,2,2> &m, param_t<v2> z)	: mat<E,2,2>(m), z(z)		{}
	template<typename X, typename Y, typename Z> force_inline mat(const X &x, const Y &y, const Z &z) : mat<E,2,2>(x, y), z(z) {}
	force_inline explicit mat(const mat<E,4,4> &m);

	force_inline	const pos<E,2>&		translation()	const			{ return z; }
	force_inline	const mat<E,2,2>&	get2x2()		const			{ return *this; }

	friend bool operator==(const mat &a, const mat &b)	{ return a.v == b.v && a.z == b.z; }
	friend bool operator!=(const mat &a, const mat &b)	{ return a.v != b.v || a.z != b.z; }
	friend mat operator+(const mat &a, const mat &b)	{ return mat(mat<E,2,2>(a.v + b.v), a.z + b.z); }
	friend mat operator-(const mat &a, const mat &b)	{ return mat(mat<E,2,2>(a.v - b.v), a.z - b.z); }
	friend mat get_inverse(const mat &m)				{ mat<E,2,2> r = get_inverse((const mat<E,2,2>&)m); return mat(r, r * -m.z); }
};
typedef mat<float,2,3> float2x3;

template<typename E> force_inline vec<E,2>		operator*(const mat<E,2,3> &a, param_t<vec<E,2>> b)		{ return (const mat<E,2,2>&)a * b; }
template<typename E> force_inline pos<E,2>		operator*(const mat<E,2,3> &a, param_t<pos<E,2>> b)		{ return (const mat<E,2,2>&)a * (const vec<E,2>&)b + a.z; }
template<typename E> force_inline mat<E,2,3>	operator*(const mat<E,2,3> &a, const mat<E,2,3> &b)		{ return mat<E,2,3>((const mat<E,2,2>&)a * (const mat<E,2,2>&)b, a * b.z); }
template<typename E> force_inline mat<E,2,3>	operator*(const mat<E,2,3> &a, const mat<E,2,2> &b)		{ return mat<E,2,3>((const mat<E,2,2>&)a * b, a.z); }
template<typename E> force_inline mat<E,2,3>	operator*(const mat<E,2,2> &a, const mat<E,2,3> &b)		{ return mat<E,2,3>(a * (const mat<E,2,2>&)b, a * b.z); }

struct float3x2 {
	float3	x, y;
	force_inline float3x2()		{}
	force_inline float3x2(const _identity &i)	: x(one), y(zero) {}
	template<typename X, typename Y> force_inline float3x2(const X &x, const Y &y) : x(x), y(y) {}
};

//-----------------------------------------------------------------------------

template<typename E> class diagonal<E,3> {
public:
	vec<E,3>	d3;
	force_inline diagonal()							{}
	template<typename D> explicit force_inline diagonal(const D &d) : d3(d)		{}
	force_inline	vec<E,1>	det()				const	{ return prod_components(d3); }
	force_inline	vec<E,1>	trace()				const	{ return sum_components(d3); }
	force_inline normalised_polynomial<3> characteristic() const	{ return -vec<E,3>(det(), sum_components(d3.xyz * d3.yzx), trace()); }

	force_inline	vec<E,1>	det2x2()			const	{ return d3.x * d3.y; }
	force_inline	vec<E,1>	trace2x2()			const	{ return d3.x + d3.y; }
	force_inline normalised_polynomial<2> characteristic2x2() const	{ return vec<E,2>(det2x2(), -trace2x2()); }

	force_inline	vec<E,3>	column(int i)		const	{ return select(uint8(1 << i), d3, zero); }

	template<typename T> friend IF_SCALAR(T,diagonal) operator*(const diagonal &a, const T &b)	{ return diagonal(a.d3 * b); }
	template<typename T> friend IF_SCALAR(T,diagonal) operator/(const diagonal &a, const T &b)	{ return a * reciprocal(b); }
	friend diagonal			operator-(const diagonal &a)					{ return diagonal(-a.d3); }
	friend diagonal			operator+(const diagonal &a, const diagonal &b)	{ return diagonal(a.d3 + b.d3); }
	friend diagonal			operator-(const diagonal &a, const diagonal &b)	{ return diagonal(a.d3 - b.d3); }
	friend const diagonal&	transpose(const diagonal &a)					{ return a; }
	friend diagonal&&		transpose(diagonal &&a)							{ return (diagonal&&)a; }
	friend diagonal			get_inverse(const diagonal &a)					{ return diagonal(reciprocal(a.d3)); }
};

template<typename E> auto strip_x(const diagonal<E,3> &m)	{ return diagonal<E,2>(m.d3.yz); }
template<typename E> auto strip_y(const diagonal<E,3> &m)	{ return diagonal<E,2>(m.d3.xz); }
template<typename E> auto strip_z(const diagonal<E,3> &m)	{ return diagonal<E,2>(m.d3.xy); }

typedef diagonal<float,3> diagonal3;

template<typename E> struct _triangular3 : diagonal<E,3> {
	vec<E,3>	o;
	using diagonal<E,3>::d3;
	force_inline _triangular3() {}
	force_inline _triangular3(const _zero&)	: iso::diagonal<E,3>(zero), o(zero) {}
	template<typename D, typename O> force_inline _triangular3(const D &d, const O &o) : iso::diagonal<E,3>(d), o(o)	{}
	force_inline	vec<E,3>	diagonal()			const	{ return d3; }

protected:
	_triangular3		_adj()							const	{ return _triangular3(d3.yzx * d3.zxy, vec<E,3>(zero, zero, o.x * o.y) - o.xyz * d3.zxy); }
	_triangular3		_abs()							const	{ return _triangular3(abs(d3), abs(o)); }
	_triangular3		_neg()							const	{ return _triangular3(-d3, -o); }
	_triangular3		_add(const _triangular3 &b)		const	{ return _triangular3(d3 + b.d3, o + b.o); }
	_triangular3		_sub(const _triangular3 &b)		const	{ return _triangular3(d3 - b.d3, o - b.o); }
	template<typename T> _triangular3 _mul(const T &b)	const	{ return _triangular3(d3 * b, o * b); }
	_triangular3		yxz()							const	{ return _triangular3(d3.yxz, o.xzy); }
};

template<typename T, int N> struct column_s;

template<typename E> class lower<E,3> : public _triangular3<E> {
	typedef _triangular3<E>	B;
public:
	force_inline lower() {}
	force_inline lower(const B &t) : B(t) {}
	template<typename D, typename O> force_inline lower(const D &_d, const O &_o) : B(_d, _o)	{}

	template<int I> force_inline	vec<E,3>	column()	const	{ return column_s<lower,I>::f(*this); }
	force_inline	vec<E,3>		column(int i)			const {
		switch (i) {
			case 0:		return column<0>();
			case 1:		return column<1>();
			default:	return column<2>();
		}
	}
	template<typename T> friend IF_SCALAR(T,lower) operator*(const lower &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,lower) operator/(const lower &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend lower				operator~(const lower &a)					{ return a._adj(); }
	friend lower				operator-(const lower &a)					{ return a._neg(); }
	friend lower				operator+(const lower &a, const lower &b)	{ return a._add(b); }
	friend lower				operator-(const lower &a, const lower &b)	{ return a._sub(b); }
	friend lower				get_inverse(const lower &a)					{ return ~a * reciprocal(a.det()); }
	friend lower				abs(const lower &a)							{ return a._abs(); }
	friend const upper<E,3>&	transpose(const lower &m)					{ return reinterpret_cast<const upper<E,3>&>(m); }
	friend upper<E,3>&&			transpose(lower &&m)						{ return reinterpret_cast<upper<E,3>&&>(m); }
	friend upper<E,3>			cofactors(const lower &m);
};
typedef lower<float,3> lower3;

template<typename E> struct column_s<lower<E,3>, 0> { static force_inline vec<E,3> f(const lower<E,3> &a) { return vec<E,3>(a.d3.x, a.o.xz); } };
template<typename E> struct column_s<lower<E,3>, 1> { static force_inline vec<E,3> f(const lower<E,3> &a) { return vec<E,3>(zero, a.d3.y, a.o.y); } };
template<typename E> struct column_s<lower<E,3>, 2> { static force_inline vec<E,3> f(const lower<E,3> &a) { return vec<E,3>(zero, zero, a.d3.z); } };

template<typename E> class upper<E,3> : public _triangular3<E> {
	typedef _triangular3<E>	B;
public:
	force_inline upper()						{}
	force_inline upper(const B &t) : B(t) {}
	template<typename D, typename O> force_inline upper(const D &_d, const O &_o) : B(_d, _o)	{}

	template<int I> force_inline	vec<E,3>	column()	const	{ return column_s<upper,I>::f(*this); }
	force_inline	vec<E,3>		column(int i)			const {
		switch (i) {
			case 0:		return column<0>();
			case 1:		return column<1>();
			default:	return column<2>();
		}
	}

	template<typename T> friend IF_SCALAR(T,upper) operator*(const upper &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,upper) operator/(const upper &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend upper				operator~(const upper &a)					{ return a._adj(); }
	friend upper				operator-(const upper &a)					{ return a._neg(); }
	friend upper				operator+(const upper &a, const upper &b)	{ return a._add(b); }
	friend upper				operator-(const upper &a, const upper &b)	{ return a._sub(b); }
	friend upper				get_inverse(const upper &a)					{ return ~a * reciprocal(a.det()); }
	friend upper				abs(const upper &a)							{ return a._abs(); }
	friend const lower<E,3>&	transpose(const upper &a)					{ return reinterpret_cast<const lower<E,3>&>(a); }
	friend lower<E,3>&&			transpose(upper &&a)						{ return reinterpret_cast<lower<E,3>&&>(a); }
	friend lower<E,3>			cofactors(const upper &a)					{ return a._adj(); }
};
typedef upper<float,3> upper3;

template<typename E> struct column_s<upper<E,3>, 0> { static force_inline vec<E,3> f(const upper<E,3> &a) { return vec<E,3>(a.d3.x, zero, zero); } };
template<typename E> struct column_s<upper<E,3>, 1> { static force_inline vec<E,3> f(const upper<E,3> &a) { return vec<E,3>(a.o.x, a.d3.y, zero); } };
template<typename E> struct column_s<upper<E,3>, 2> { static force_inline vec<E,3> f(const upper<E,3> &a) { return vec<E,3>(a.o.zy, a.d3.z); } };

template<typename E> inline upper<E,3>	cofactors(const lower<E,3> &m)	{ return m._adj(); }

template<typename E> class symmetrical<E,3> : public _triangular3<E> {
	typedef _triangular3<E>	B;
public:
	using B::o; using B::d3;
	static symmetrical	make(const mat<E,3,3> &m);
	force_inline symmetrical()						{}
	force_inline symmetrical(const B &t)			: B(t) {}
	force_inline symmetrical(const iso::diagonal<E,3> &t)	: B(t.d3, zero) {}
	template<typename D, typename O> force_inline symmetrical(const D &_d, const O &_o) : B(_d, _o)	{}

	force_inline vec<E,1>					det()				const	{ return prod_components(d3) + prod_components(o) * 2 - sum_components(d3.zxy * o * o); }
	force_inline normalised_polynomial<3>	characteristic()	const	{ return -vec<E,3>(det(), sum_components(d3.xyz * d3.yzx) - sum_components(o * o), B::trace()); }
	force_inline vec<E,1>					det2x2()			const	{ return d3.x * d3.y - square(o.x); }
	force_inline normalised_polynomial<2>	characteristic2x2()	const	{ return vec<E,2>(det2x2(), -B::trace2x2()); }
	force_inline symmetrical				yxz()				const	{ return B::yxz(); }

	template<int I> force_inline vec<E,3>	column()			const	{ return column_s<symmetrical,I>::f(*this); }
	force_inline	vec<E,3>				column(int i)		const {	// (also, column containing ith element)
		switch (i) {
			case 0:	return column<0>();
			case 1: case 3: return column<1>();
			default: return column<2>();
		}
	}

	friend symmetrical operator~(const symmetrical &a) {
		return symmetrical(
			a.d3.yzx * a.d3.zxy - a.o.yzx * a.o.yzx,
			a.o.yzx * a.o.zxy - a.o.xyz * a.d3.zxy
		);
	}
	template<typename T> friend IF_SCALAR(T,symmetrical) operator*(const symmetrical &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,symmetrical) operator/(const symmetrical &a, const T &b)	{ return a._mul(reciprocal(b)); }

	friend symmetrical			operator-(const symmetrical &a)							{ return a._neg(); }
	friend symmetrical			operator+(const symmetrical &a, const symmetrical &b)	{ return a._add(b); }
	friend symmetrical			operator-(const symmetrical &a, const symmetrical &b)	{ return a._sub(b); }
	friend symmetrical			cofactors(const symmetrical &a)							{ return ~a; }
	friend const symmetrical&	transpose(const symmetrical &a)							{ return a; }
	friend symmetrical&&		transpose(symmetrical &&m)								{ return (symmetrical&&)m; }
	friend symmetrical			get_inverse(const symmetrical &a)						{ return ~a * reciprocal(a.det()); }
	friend symmetrical			abs(const symmetrical &a)								{ return a._abs(); }
	friend int					max_component_index(const symmetrical &a)				{ return max_component_index(pair<vec<E,3>, vec<E,3>>(a.d3, a.o)); }
};

template<typename E> auto	strip_x(const symmetrical<E,3> &m)	{ return symmetrical<E,2>(m.d3.yz, m.o.y); }
template<typename E> auto	strip_y(const symmetrical<E,3> &m)	{ return symmetrical<E,2>(m.d3.xz, m.o.z); }
template<typename E> auto	strip_z(const symmetrical<E,3> &m)	{ return symmetrical<E,2>(m.d3.xy, m.o.x); }

typedef symmetrical<float,3> symmetrical3;

template<typename E> struct column_s<symmetrical<E,3>, 0> { static force_inline vec<E,3> f(const symmetrical<E,3> &a) { return vec<E,3>(a.d3.x, a.o.xz); } };
template<typename E> struct column_s<symmetrical<E,3>, 1> { static force_inline vec<E,3> f(const symmetrical<E,3> &a) { return vec<E,3>(a.o.x, a.d3.y, a.o.y); } };
template<typename E> struct column_s<symmetrical<E,3>, 2> { static force_inline vec<E,3> f(const symmetrical<E,3> &a) { return vec<E,3>(a.o.zy, a.d3.z); } };

template<typename E> vec<E,3> principal_component(const symmetrical<E,3> &s) {
	vec<E,3>	r;
	int			n = s.characteristic().roots(r);

	if (n == 1)
		return vec<E,3>(one); // only one root, which implies we have a multiple of the identity

	if (n == -1 && abs(r.y) > abs(r.x)) {
		//1 real root, 2 imaginary
		symmetrical<E,3>	m	= symmetrical<E,3>(s.d3 - r.y, s.o);
		switch (max_component_index(abs(m))) {	// pick the first eigenvector based on biggest index
			case 0:
			case 3: return vec<E,3>(-m.o.x, m.d3.x, zero);
			case 1:
			case 4:	return vec<E,3>(zero, -m.o.y, m.d3.y);
			case 5:	return vec<E,3>(m.o.z, zero, -m.d3.x);
			default:return vec<E,3>(zero, -m.d3.z, m.o.y);
		}
	}

	vec<E,1>			e	= n == -1 ? r.x : vec<E,1>(max_component(abs(r)));
	symmetrical<E,3>	m	= symmetrical<E,3>(s.d3 - e, s.o);
	symmetrical<E,3>	u	= ~m;
	return u.column(max_component_index(abs(u)));
}

//-----------------------------------------------------------------------------

template<typename E> class mat<E,3,3> {
protected:
	typedef vec<E,3>	v3;
public:
	v3	x, y, z;
	force_inline mat()	{}
	force_inline mat(const _zero&)				: x(zero), y(zero), z(zero) {}
	force_inline mat(const _identity&) { vec<E,2> t(zero, one); x = t.yxx; y = t.xyx; z = t.xxy; }
	force_inline mat(const mat &m) : x(m.x), y(m.y), z(m.z) {}
	force_inline explicit mat(const mat<E,2,3> &m) : x(m.x, zero), y(m.y, zero), z(m.z) {}
	force_inline mat(const diagonal<E,3> &m)	{ vec<E,4> s(m.d3, zero); x = s.xww; y = s.wyw; z = s.wwz; }
	force_inline mat(const upper<E,3> &m)		: x(m.d3.x, vec<E,2>(zero)), y(m.o.x, m.d3.y, zero), z(m.o.zy, m.d3.z)	{}
	force_inline mat(const lower<E,3> &m)		: x(m.d3.x, m.o.xz), y(zero, m.d3.y, m.o.y), z(vec<E,2>(zero), m.d3.z)	{}
	force_inline mat(const symmetrical<E,3> &m)	: x(m.d3.x, m.o.xz), y(m.o.x, m.d3.y, m.o.y), z(m.o.zy, m.d3.z)	{}
	template<typename X, typename Y, typename Z> force_inline mat(const X &x, const Y &y, const Z &z) : x(x), y(y), z(z) {}
//	force_inline explicit mat(const mat<E,2,2> &m)	: x(m.x, zero), y(m.y, zero), z(zero, zero, one) {}
	force_inline explicit mat(const mat<E,4,4> &m);

	force_inline v3&			operator[](int i)			{ return (&x)[i]; }
	force_inline v3			operator[](int i)	const	{ return (&x)[i]; }
	force_inline	v3&			column(int i)				{ return (&x)[i]; }
	force_inline	v3			column(int i)		const	{ return (&x)[i]; }

	force_inline	v3			diagonal()			const	{ return v3(x.x, y.y, z.z); }
	force_inline vec<E,1>	det()				const	{ return dot(x, cross(y, z)); }
	force_inline	vec<E,1>	trace()				const	{ return x.x + y.y + z.z; }
	force_inline normalised_polynomial<3>	characteristic()	const	{ v3 d = diagonal(); return v3(-det(), sum_components(d.xyz * d.yzx - v3(y.x, z.xy) * v3(x.yz, y.z)), -sum_components(d)); }
	force_inline v3			scale()				const	{ return sqrt(x * x + y * y + z * z); }
	force_inline v3			pre_scale()			const	{ return sqrt(v3(len2(x), len2(y), len2(z))); }
	v3						euler()				const;

	force_inline v3			row0()				const	{ return v3(x.x, y.x, z.x); }
	force_inline v3			row1()				const	{ return v3(x.y, y.y, z.y); }
	force_inline v3			row2()				const	{ return v3(x.z, y.z, z.z); }

	static mat	between(param_t<v3> a, param_t<v3> b);
	friend bool	operator==(const mat &a, const mat &b)		{ return a.x == b.x && a.y == b.y && a.z == b.z; }
	friend bool	operator!=(const mat &a, const mat &b)		{ return a.x != b.x || a.y != b.y || a.z != b.z; }
	friend mat	operator+(const mat &a, const mat &b)		{ return mat(a.x + b.x, a.y + b.y, a.z + b.z); }
	friend mat	operator-(const mat &a, const mat &b)		{ return mat(a.x - b.x, a.y - b.y, a.z - b.z); }
	friend mat	operator+(const mat &a, const diagonal3 &b)	{ vec<E,4> s(b.d3, zero); return mat(a.x + s.xww, a.y + s.wyw, a.z + s.wwz); }
	friend mat	operator-(const mat &a, const diagonal3 &b)	{ vec<E,4> s(b.d3, zero); return mat(a.x - s.xww, a.y - s.wyw, a.z - s.wwz); }
	template<int N, int A>	friend mat operator*(const mat &a, const _v1<E,N,A> &b)	{ return mat(a.x * b, a.y * b, a.z * b); }
	template<int N, int A>	friend mat operator/(const mat &a, const _v1<E,N,A> &b)	{ return a * reciprocal(b); }

#ifdef VEC_INLINE_INVERSE
	friend mat cofactors(const mat &m) {
		return mat(
			cross(m.y, m.z),
			cross(m.z, m.x),
			cross(m.x, m.y)
		);
	}
	friend mat get_inverse_transpose(const mat &m) {
		mat	c = cofactors(m);
		return c * reciprocal(dot(m.x, c.x));
	}
#else
	friend mat cofactors(const mat &m);
	friend mat get_inverse_transpose(const mat &m);
#endif
	friend transpose_s<mat> operator~(const mat &a)		{ return transpose(cofactors(a)); }
	friend transpose_s<mat> get_inverse(const mat &a)	{ return transpose(get_inverse_transpose(a)); }

	friend mat get_transpose(const mat &m) {
		vec<E,4>	t0 = perm<0,2,1,3>((m.x.xy, m.y.xy));
		return mat(
			(t0.xy, m.z.x),
			(t0.zw, m.z.y),
			(m.x.z, m.y.z, m.z.z)
		);
	}
};

template<typename E> auto	strip_x(const mat<E,3,3> &m)	{ return mat<E,2,2>(m.y.yz, m.z.yz); }
template<typename E> auto	strip_y(const mat<E,3,3> &m)	{ return mat<E,2,2>(m.x.xz, m.z.xz); }
template<typename E> auto	strip_z(const mat<E,3,3> &m)	{ return mat<E,2,2>(m.x.xy, m.y.xy); }

typedef mat<float,3,3> float3x3;

template<typename E> mat<E,3,3> pow(const mat<E,3,3> &x, int y) {
	vec<E,3>	r = poly_div(y, 1, x.characteristic());
	return x * x * r.z + x * r.y + diagonal<E,3>(r.x);
}

template<typename E> vec<E,3> mat<E,3,3>::euler() const {
	E	pitch	= asin(y.z);
	return abs(y.z) <= 0.99f
		? vec<E,3>(pitch, atan2(-y.x, y.y), atan2(-x.z, z.z))	//pitch, roll, yaw
		: vec<E,3>(pitch, atan2(x.y, x.x), zero);				//pitch, roll, yaw
}

// ZYZ:
//.x: rotation around the Z axis
//.y: rotation around the X axis
//.z: rotation around the new Z axis
template<typename E> vec<E,3> to_zyz(const mat<E,3,3> &m) {
	return	m.z.z == one	? vec<E,3>(atan2(m.x.y, m.x.x), zero, zero)
		:	m.z.z == -one	? vec<E,3>(atan2(-m.x.y, m.x.x), pi, zero)
		:	vec<E,3>(atan2(m.z.y, m.z.x), acos(m.z.z), atan2(m.y.z, -m.x.z));
}

template<typename E> mat<E,3,3> mat<E,3,3>::between(param_t<vec<E,3>> a, param_t<vec<E,3>> b) {
	vec<E,3>	c = cross(a, b);

	if (dot(c, c) == zero) {
		if (dot(a, b) > zero)
			return identity;
		return rotation_pi(perp(a));
	}

	vec<E,3>	an = normalise(a);
	vec<E,3>	bn = normalise(b);
	vec<E,3>	cn = normalise(c);

	mat<E,3,3>	r1(an, cn, cross(cn, an));
	mat<E,3,3>	r2(bn, cn, cross(cn, bn));
	return r2 * transpose(r1);
}

template<typename E> symmetrical<E,3> mul_mulT(const mat<E,3,3> &m, diagonal<E,3> s) {
//	float3x3	t = m * s * transpose(m);
	return symmetrical<E,3>(
		square(m.x) * s.d3.x + square(m.y) * s.d3.y + square(m.z) * s.d3.z,
			m.x * (m.x << 1) * s.d3.x
		+	m.y * (m.y << 1) * s.d3.y
		+	m.z * (m.z << 1) * s.d3.z
	);
}

template<typename E> symmetrical<E,4> mul_mulT(const mat<E,4,4> &m, diagonal<E,4> s) {
//	float4x4	t = m * s * transpose(m);
	return symmetrical<E,4>(
		square(m.x) * s.d4.x + square(m.y) * s.d4.y + square(m.z) * s.d4.z + square(m.w) * s.d4.w,

		  m.x.xyz * m.x.yzw * s.d4.x
		+ m.y.xyz * m.y.yzw * s.d4.y
		+ m.z.xyz * m.z.yzw * s.d4.z
		+ m.w.xyz * m.w.yzw * s.d4.w,

		  m.x.xyx * m.x.zww * s.d4.x
		+ m.y.xyx * m.y.zww * s.d4.y
		+ m.z.xyx * m.z.zww * s.d4.z
		+ m.w.xyx * m.w.zww * s.d4.w
	);
}

template<typename E, int N> symmetrical<E,N>				mul_mulT(const mat<E,N,N> &m, diagonal<E,N> s);
template<typename E, int N, typename M> symmetrical<E,N>	mul_mulT(const M &m, diagonal<E,N> s)			{ return mul_mulT(mat<E,3,3>(m), s); }
template<typename E, int N, typename M> symmetrical<E,N>	mul_mulT(const M &m, const symmetrical<E,N> &s)	{ return symmetrical<E,N>::make(m * s * transpose(m)); }

template<typename E> inline symmetrical<E,3>	symmetrical<E,3>::make(const mat<E,3,3> &m)	{ return symmetrical<E,3>(m.diagonal(), vec<E,3>(m.y.x, m.z.y, m.z.x)); }

template<typename E> inline	transpose_s<mat<E,3,3>>	transpose(const mat<E,2,3> &m)				{ return transpose_s<mat<E,3,3>>((mat<E,3,3>)m); }
template<typename E> inline	auto					transpose(const inverse_s<mat<E,2,3>> &m)	{ return transpose(inverse((mat<E,3,3>)m.m)); }
template<typename E> inline	transpose_s<mat<E,3,3>>	get_transpose(const mat<E,2,3> &m)			{ return transpose((mat<E,3,3>)m); }

template<typename E, int N, int A, int B, int C> force_inline symmetrical<E,3> outer_product(_v3<E,N,A,B,C> v) {
	return symmetrical<E,3>(v * v, v * perm<1,2,0>(v));
}
template<typename E> force_inline symmetrical<E,3> outer_product(vec<E,3> v) {
	return symmetrical<E,3>(v * v, v.xyz * v.yzx);
}
template<typename E, int N, int A, int B, int C> force_inline symmetrical<E,3> operator~(_v3<E,N,A,B,C> v) {
	return symmetrical<E,3>(zero, perm<2,0,1>(-v));
}
template<typename E> inline symmetrical<E,3> operator~(vec<E,3> v) { return ~v.xyz; }

template<typename E> mat<E,3,3> cofactors(const mat<E,2,3> &m) {
	mat<E,2,2> c = cofactors((const mat<E,2,2>&)m);
	return mat<E,3,3>(
		vec<E,3>(c.x, dot(c.x, -m.z)),
		vec<E,3>(c.y, dot(c.y, -m.z)),
		vec<E,3>(zero, zero, dot(c.x, m.x))
	);
}

#ifdef __MWERKS__
template<typename E> force_inline vec<E,3>		operator*(const mat<E,3,3> &a, param_t<vec<E,3>> b)					{ vec<E,3> v; wii::mul(&v, &a, &b); return v; }
#else
template<typename E> force_inline vec<E,3>		operator*(const mat<E,3,3> &a, param_t<vec<E,3>> b)					{ return a.x * b.x + a.y * b.y + a.z * b.z; }
#endif
template<typename E> force_inline vec<E,3>		operator*(const mat<E,3,3> &a, param_t<pos<E,2>> b)					{ return a.x * b.x + a.y * b.y + a.z; }
template<typename E> force_inline pos<E,3>		operator*(const mat<E,3,3> &a, param_t<pos<E,3>> b)					{ return a.x * b.x + a.y * b.y + a.z * b.z; }
template<typename E> force_inline vec<E,4>		operator*(const mat<E,3,3> &a, param_t<vec<E,4>> b)					{ return vec<E,4>(a.x * b.x + a.y * b.y + a.z * b.z, b.w); }
template<typename E> force_inline mat<E,3,3>		operator*(const mat<E,3,3> &a, const mat<E,3,3> &b)					{ return mat<E,3,3>(a * b.x, a * b.y, a * b.z); }
template<typename E> force_inline mat<E,3,3>		operator*(const mat<E,3,3> &a, const symmetrical<E,3> &b)			{ return a * mat<E,3,3>(b); }
template<typename E> force_inline mat<E,3,3>		operator*(const symmetrical<E,3> &a, const mat<E,3,3> &b)			{ return mat<E,3,3>(a) * b; }
template<typename E, int N, int A, int B, int C> force_inline vec<E,3> operator*(const mat<E,3,3> &m, const _v3<E,N,A,B,C> &v)	{ return m.x * perm<0>(v) + m.y * perm<1>(v) + m.z * perm<2>(v); }

#ifdef USE_VECDOT
template<typename E> force_inline vec<E,3>		operator*(const transpose_s<mat<E,3,3>&> &a, param_t<vec<E,3>> b)	{ return vec<E,3>(dot(a.m.x,b), dot(a.m.y,b), dot(a.m.z,b)); }
template<typename E> force_inline vec<E,4>		operator*(const transpose_s<mat<E,3,3>&> &a, param_t<vec<E,4>> b)	{ return vec<E,4>(dot(a.m.x,b.xyz), dot(a.m.y,b.xyz), dot(a.m.z,b.xyz), b.w); }
template<typename E> force_inline mat<E,3,3>		operator*(const transpose_s<mat<E,3,3>&> &a, const mat<E,3,3> &b)	{ return mat<E,3,3>(a * b.x, a * b.y, a * b.z); }
template<typename E> force_inline vec<E,3>		operator*(const transpose_s<mat<E,3,3>> &a, param_t<vec<E,3>> b)	{ return vec<E,3>(dot(a.m.x,b), dot(a.m.y,b), dot(a.m.z,b)); }
template<typename E> force_inline vec<E,4>		operator*(const transpose_s<mat<E,3,3>> &a, param_t<vec<E,4>> b)	{ return vec<E,4>(dot(a.m.x,b.xyz), dot(a.m.y,b.xyz), dot(a.m.z,b.xyz), b.w); }
template<typename E> force_inline mat<E,3,3>		operator*(const transpose_s<mat<E,3,3>> &a, const mat<E,3,3> &b)	{ return mat<E,3,3>(a * b.x, a * b.y, a * b.z); }
#endif

//-----------------------------------------------------------------------------

template<typename E> class mat<E,3,4> : public mat<E,3,3> {
	typedef mat<E,3,3> B;
public:
	using B::x; using B::y; using B::z;
	pos<E,3>		w;

	force_inline mat()														{}
	force_inline mat(const _identity &i)					: B(i), w(zero)		{}
	force_inline mat(const _zero &i)						: B(i), w(zero)		{}
	force_inline mat(const mat &b)						: B(b), w(b.w)		{}
	force_inline mat(const B &m)						: B(m), w(zero)	{}
	force_inline mat(const B &m, param_t<vec<E,3>> w)	: B(m), w(w)		{}
	force_inline explicit mat(const mat<E,2,3> &m)		: B((const mat<E,2,2>&)m), w(m.z, zero) {}
	force_inline explicit mat(const mat<E,4,4> &m);
	template<typename X, typename Y, typename Z, typename W> force_inline mat(const X &x, const Y &y, const Z &z, const W &w) : B(x, y, z), w(w) {}

	force_inline	const pos<E,3>&		translation()	const	{ return w; }
	force_inline	const B&			get3x3()		const	{ return *this; }

	force_inline vec<E,4>			row0()			const	{ return vec<E,4>(x.x, y.x, z.x, w.x); }
	force_inline vec<E,4>			row1()			const	{ return vec<E,4>(x.y, y.y, z.y, w.y); }
	force_inline vec<E,4>			row2()			const	{ return vec<E,4>(x.z, y.z, z.z, w.z); }

	friend bool	operator==(const mat &a, const mat &b)		{ return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
	friend bool	operator!=(const mat &a, const mat &b)		{ return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w; }
	friend mat	operator+(const mat &a, const mat &b)		{ return mat(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
	friend mat	operator-(const mat &a, const mat &b)		{ return mat(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
	template<typename T> friend IF_SCALAR(T,mat) operator*(const mat &a, const T &b)	{ return mat(a.x * b, a.y * b, a.z * b, a.w * b); }

	friend mat	get_inverse(const mat &m)	{
		B r = get_inverse((const B&)m);
		return mat(r, r * -m.w);
	}
};
typedef mat<float,3,4> float3x4;

template<typename E> auto	strip_x(const mat<E,3,4> &m)	{ return mat<E,2,3>(m.y.yz, m.z.yz, m.w.yz); }
template<typename E> auto	strip_y(const mat<E,3,4> &m)	{ return mat<E,2,3>(m.x.xz, m.z.xz, m.w.xy); }
template<typename E> auto	strip_z(const mat<E,3,4> &m)	{ return mat<E,2,3>(m.x.xy, m.y.xy, m.w.xy); }

template<typename E, int N, int A, int B, int C>struct mul_t<mat<E,3,4>, _v3<E,N,A,B,C> >{ typedef vec<E,3> type; };

template<typename E> force_inline vec<E,3>	operator*(const mat<E,3,4> &a, param_t<vec<E,3>> b)		{ return (const mat<E,3,3>&)a * b; }
template<typename E> force_inline pos<E,3>	operator*(const mat<E,3,4> &a, param_t<pos<E,3>> b)		{ return (const mat<E,3,3>&)a * (const vec<E,3>&)b + a.w; }
template<typename E> force_inline vec<E,4>	operator*(const mat<E,3,4> &a, param_t<vec<E,4>> b)		{ return vec<E,4>((const mat<E,3,3>&)a * b.xyz + a.w * b.w, b.w); }
template<typename E> force_inline mat<E,3,4>	operator*(const mat<E,3,3> &a, const mat<E,3,4> &b)		{ return mat<E,3,4>(a * (const mat<E,3,3>&)b, a * (const vec<E,3>&)b.w); }
template<typename E> force_inline mat<E,3,4>	operator*(const mat<E,3,4> &a, const mat<E,3,3> &b)		{ return mat<E,3,4>((const mat<E,3,3>&)a * b, a.w); }
template<typename E, int N, int A, int B, int C> force_inline vec<E,3> operator*(const mat<E,3,4> &m, const _v3<E,N,A,B,C> &v)	{ return m.x * perm<0>(v) + m.y * perm<1>(v) + m.z * perm<2>(v) + m.w; }

template<typename E> force_inline vec<E,3>	operator*(const inverse_s<mat<E,3,4>&> &a, param_t<vec<E,3>> b)		{ return inverse((const mat<E,3,3>&)a.m) * b; }
template<typename E> force_inline pos<E,3>	operator*(const inverse_s<mat<E,3,4>&> &a, param_t<pos<E,3>> b)		{ return inverse((const mat<E,3,3>&)a.m) * (const vec<E,3>&)(b - a.m.w); }


#ifdef __MWERKS__
template<typename E> force_inline void		composite(mat<E,3,4> &m, const mat<E,3,4> &a, const mat<E,3,4> &b)	{ wii::mul(&m, &a, &b); }
template<typename E> force_inline mat<E,3,4>	operator*(const mat<E,3,4> &a, const mat<E,3,4> &b)					{ mat<E,3,4> m; wii::mul(&m, &a, &b); return m; }
#else
template<typename E> force_inline mat<E,3,4>	operator*(const mat<E,3,4> &a, const mat<E,3,4> &b)					{ return mat<E,3,4>((const mat<E,3,3>&)a * (const mat<E,3,3>&)b, a * b.w); }
template<typename E> force_inline void		composite(mat<E,3,4> &m, const mat<E,3,4> &a, const mat<E,3,4> &b)	{ m = a * b; }
#endif

//-----------------------------------------------------------------------------

template<typename E> class diagonal<E,4> {
public:
	vec<E,4>	d4;
	force_inline diagonal()							{}
	template<typename D> explicit force_inline diagonal(const D &d) : d4(d) {}
	force_inline	vec<E,1>	det()				const	{ return prod_components(d4); }
	force_inline	vec<E,1>	trace()				const	{ return sum_components(d4); }
//	force_inline normalised_polynomial<4> characteristic()	const	{ return vec<E,2>(det(), -trace()); }
	force_inline	vec<E,1>	det2x2()					const	{ return d4.x * d4.y; }
	force_inline	vec<E,1>	trace2x2()					const	{ return d4.x + d4.y; }
	force_inline normalised_polynomial<2> characteristic2x2() const	{ return vec<E,2>(det2x2(), -trace2x2()); }
	force_inline	vec<E,1>	det3x3()			const	{ return prod_components(d4.xyz); }
	force_inline	vec<E,1>	trace3x3()			const	{ return sum_components(d4.xyz); }
	force_inline normalised_polynomial<3> characteristic3x3() const	{ return -vec<E,3>(det3x3(), sum_components(d4.xyz * d4.yzx), trace3x3()); }

	force_inline	vec<E,4>	column(int i)		const	{ return select(uint8(1 << i), d4, zero); }

	template<typename T> friend IF_SCALAR(T,diagonal) operator*(const diagonal &a, const T &b)	{ return diagonal(a.d4 * b); }
	template<typename T> friend IF_SCALAR(T,diagonal) operator/(const diagonal &a, const T &b)	{ return a * reciprocal(b); }
	friend diagonal			operator-(const diagonal &a)						{ return diagonal(-a.d4); }
	friend diagonal			operator+(const diagonal &a, const diagonal &b)		{ return diagonal(a.d4 + b.d4); }
	friend diagonal			operator-(const diagonal &a, const diagonal &b)		{ return diagonal(a.d4 - b.d4); }
	friend const diagonal&	transpose(const diagonal &m)						{ return m; }
	friend diagonal&&		transpose(diagonal &&m)								{ return (diagonal&&)m; }
	friend diagonal			get_inverse(const diagonal &m)						{ return diagonal(reciprocal(m.d4)); }
};

template<typename E> auto strip_x(const diagonal<E,4> &m)	{ return diagonal<E,3>(m.d4.yzw); }
template<typename E> auto strip_y(const diagonal<E,4> &m)	{ return diagonal<E,3>(m.d4.xzw); }
template<typename E> auto strip_z(const diagonal<E,4> &m)	{ return diagonal<E,3>(m.d4.xyw); }
template<typename E> auto strip_w(const diagonal<E,4> &m)	{ return diagonal<E,3>(m.d4.xyz); }

typedef diagonal<float,4> diagonal4;

template<typename E> struct _triangular4 : diagonal<E,4>, _triangular3<E> {
	typedef diagonal<E,4> B;
	using B::d4;
	using _triangular3<E>::d3;
	using _triangular3<E>::o;

	force_inline _triangular4() {}
	force_inline _triangular4(const _zero&)	: B(zero), _triangular3<E>(zero) {}
	template<typename D> force_inline _triangular4(const D &d, const _triangular3<E> &t) : iso::diagonal<E,4>(d), _triangular3<E>(t)	{}
	template<typename D, typename O1, typename O2> force_inline _triangular4(const D &d, const O1 &o1, const O2 &o2) : iso::diagonal<E,4>(d), _triangular3<E>(o1, o2)	{}
	force_inline	vec<E,4>		diagonal()				const	{ return d4; }

protected:
	force_inline normalised_polynomial<4> characteristic() const;
	_triangular4			_adj()						const;
	_triangular4			_abs()						const	{ return _triangular4(abs(d4), _triangular3<E>::_abs()); }
	_triangular4			_neg()						const	{ return _triangular4(-d4, _triangular3<E>::_neg()); }
	_triangular4			_add(const _triangular4 &b)	const	{ return _triangular4(d4 + b.d4, _triangular3<E>::_add(b)); }
	_triangular4			_sub(const _triangular4 &b)	const	{ return _triangular4(d4 - b.d4, _triangular3<E>::_sub(b)); }
	template<typename T> _triangular4 _mul(const T &b)	const	{ return _triangular4(d4 * b, _triangular3<E>::_mul(b)); }
	_triangular4			yxzw()						const	{ return _triangular4(d4.yxzw, perm<0,3,2>(d3, o), perm<1,5,4>(d3, o)); }
	_triangular4			xywz()						const	{ return _triangular4(d4.xzwz, perm<0,5,2>(d3, o), perm<4,1,3>(d3, o)); }
};

template<typename E> _triangular4<E> _triangular4<E>::_adj() const {
	return _triangular4<E>(
		d4.yzxx * d4.zxyy * d4.wwwz,
		d4.zxy * d4.wwx * d3,
		vec<E,3>(
			(d3.xy * d3.yz - d4.yz * o.xy) * d4.wx,
			(d3.z * o.x - d4.z * o.z) * d4.y + (d4.z * o.y - d3.y * d3.z) * d3.x
		)
	);
}

template<typename E> class lower<E,4> : public _triangular4<E> {
	typedef _triangular4<E> B;
public:
	force_inline lower() {}
	force_inline lower(const B &t) : B(t) {}
	template<typename D> force_inline lower(const D &d, const _triangular3<E> &t) : B(d, t)	{}
	template<typename D, typename O1, typename O2> force_inline lower(const D &d, const O1 &o1, const O2 &o2) : B(d, o1, o2)	{}

	template<int I> force_inline	vec<E,4>	column() const	{ return column_s<lower,I>::f(*this); }
	force_inline	vec<E,4>	column(int i) const {
		switch (i) {
			case 0:		return column<0>();
			case 1:		return column<1>();
			case 2:		return column<2>();
			default:	return column<3>();
		}
	}
	template<typename T> friend IF_SCALAR(T,lower) operator*(const lower &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,lower) operator/(const lower &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend lower				operator~(const lower &a)					{ return a._adj(); }
	friend lower				operator-(const lower &a)					{ return a._neg(); }
	friend lower				operator+(const lower &a, const lower &b)	{ return a._add(b); }
	friend lower				operator-(const lower &a, const lower &b)	{ return a._sub(b); }
	friend lower				get_inverse(const lower &a)					{ return ~a * reciprocal(a.det()); }
	friend lower				abs(const lower &a)							{ return a._abs(); }
	friend const upper<E,4>&	transpose(const lower &a)					{ return reinterpret_cast<const upper<E,4>&>(a); }
	friend upper<E,4>&&			transpose(lower &&a)						{ return reinterpret_cast<upper<E,4>&&>(a); }
	friend upper<E,4>			cofactors(const lower &a);
};
typedef upper<float,4> upper4;

template<typename E> struct column_s<lower<E,4>, 0> { static force_inline vec<E,4> f(const lower<E,4> &a) { return vec<E,4>(a.d4.x, a.d3.x, a.o.xz); } };
template<typename E> struct column_s<lower<E,4>, 1> { static force_inline vec<E,4> f(const lower<E,4> &a) { return vec<E,4>(zero, a.d4.y, a.d3.y, a.o.y); } };
template<typename E> struct column_s<lower<E,4>, 2> { static force_inline vec<E,4> f(const lower<E,4> &a) { return vec<E,4>(zero, zero, a.d4.z, a.d3.z); } };
template<typename E> struct column_s<lower<E,4>, 3> { static force_inline vec<E,4> f(const lower<E,4> &a) { return vec<E,4>(zero, zero, zero, a.d4.w); } };

template<typename E> class upper<E,4> : public _triangular4<E> {
	typedef _triangular4<E> B;
public:
	force_inline upper() {}
	force_inline upper(const B &t) : B(t) {}
	template<typename D> force_inline upper(const D &d, const _triangular3<E> &t) : B(d, t)	{}
	template<typename D, typename O1, typename O2> force_inline upper(const D &d, const O1 &o1, const O2 &o2) : B(d, o1, o2)	{}

	template<int I> force_inline	vec<E,4>	column() const	{ return column_s<upper,I>::f(*this); }
	force_inline	vec<E,4>	column(int i) const{
		switch (i) {
			case 0:		return column<0>();
			case 1:		return column<1>();
			case 2:		return column<2>();
			default:	return column<3>();
		}
	}
	template<typename T> friend IF_SCALAR(T,upper) operator*(const upper &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,upper) operator/(const upper &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend upper				operator~(const upper &a)					{ return a._adj(); }
	friend upper				operator-(const upper &a)					{ return a._neg(); }
	friend upper				operator+(const upper &a, const upper &b)	{ return a._add(b); }
	friend upper				operator-(const upper &a, const upper &b)	{ return a._sub(b); }
	friend upper				get_inverse(const upper &a)					{ return ~a * reciprocal(a.det()); }
	friend upper				abs(const upper &a)							{ return a._abs(); }
	friend const lower<E,4>&	transpose(const upper &a)					{ return reinterpret_cast<const lower<E,4>&>(a); }
	friend lower<E,4>&&			transpose(upper &&a)						{ return reinterpret_cast<lower<E,4>&&>(a); }
	friend lower<E,4>			cofactors(const upper &a)					{ return a._adj(); }
};
typedef lower<float,4> lower4;

template<typename E> struct column_s<upper<E,4>, 0> { static force_inline vec<E,4> f(const upper<E,4> &a) { return vec<E,4>(a.d4.x, zero, zero, zero); } };
template<typename E> struct column_s<upper<E,4>, 1> { static force_inline vec<E,4> f(const upper<E,4> &a) { return vec<E,4>(a.d3.x, a.d4.y, zero, zero); } };
template<typename E> struct column_s<upper<E,4>, 2> { static force_inline vec<E,4> f(const upper<E,4> &a) { return vec<E,4>(a.o.x, a.d3.y, a.d4.z, zero); } };
template<typename E> struct column_s<upper<E,4>, 3> { static force_inline vec<E,4> f(const upper<E,4> &a) { return vec<E,4>(a.o.zy, a.d3.z, a.d4.w); } };

template<typename E> inline upper<E,4>	cofactors(const lower<E,4> &a)	{ return a._adj(); }

template<typename E> class symmetrical<E,4> : public _triangular4<E> {
	typedef _triangular4<E> B;
public:
	using B::d4;
	using B::d3;
	static symmetrical	make(const mat<E,4,4> &m);
	force_inline symmetrical() {}
	force_inline symmetrical(const B &t)		: B(t) {}
	force_inline symmetrical(const iso::diagonal<E,4> &t)	: B(t.d4, zero, zero) {}
	template<typename D> force_inline symmetrical(const D &d, const _triangular3<E> &t) : B(d, t)	{}
	template<typename D, typename O1, typename O2> force_inline symmetrical(const D &d, const O1 &o1, const O2 &o2) : B(d, o1, o2)	{}

	vec<E,1>								det()				const;
	normalised_polynomial<4>				characteristic()	const;
	force_inline vec<E,1>					det2x2()			const	{ return d4.x * d4.y - square(d3.x); }
	force_inline normalised_polynomial<2>	characteristic2x2()	const	{ return vec<E,2>(det2x2(), -B::trace2x2()); }

	template<int I> force_inline	vec<E,4>	column() const	{ return column_s<symmetrical,I>::f(*this); }
	force_inline	vec<E,4> column(int i) const {	// (also, column containing ith element)
		switch (i) {
			case 0:					return column<0>();
			case 1:	case 4:			return column<1>();
			case 2:	case 5:	case 7: return column<2>();
			default:				return column<3>();
		}
	}
	symmetrical				yxzw()		const { return B::yxzw(); }
	symmetrical				xywz()		const { return B::xywz(); }

	friend symmetrical			operator~(const symmetrical &a)							{ return cofactors(a); }
	template<typename T> friend IF_SCALAR(T,symmetrical) operator*(const symmetrical &a, const T &b)	{ return a._mul(b); }
	template<typename T> friend IF_SCALAR(T,symmetrical) operator/(const symmetrical &a, const T &b)	{ return a._mul(reciprocal(b)); }
	friend symmetrical			operator-(const symmetrical &a)							{ return a._neg(); }
	friend symmetrical			operator+(const symmetrical &a, const symmetrical &b)	{ return a._add(b); }
	friend symmetrical			operator-(const symmetrical &a, const symmetrical &b)	{ return a._sub(b); }
	friend const symmetrical&	transpose(const symmetrical &a)							{ return a; }
	friend symmetrical&&		transpose(symmetrical &&a)								{ return (symmetrical&&)a; }
	friend symmetrical			get_inverse(const symmetrical &a)						{ return ~a * reciprocal(a.det()); }
	friend symmetrical			abs(const symmetrical &a)								{ return a._abs(); }
	friend int					max_component_index(const symmetrical &a)				{ return max_component_index(make_pair(a.d4, pair<vec<E,3>, vec<E,3>>(a.d3, a.o))); }
//	friend int					max_component_index(const symmetrical &a)				{ return max_component_index(pair<vec<E,3>, vec<E,3>>(a.d3, a.o)); }
};

template<typename E> auto	strip_x(const symmetrical<E,4> &m)	{ return symmetrical<E,3>(m.d4.yzw, vec<E,3>(m.d3.yz, m.o.y)); }
template<typename E> auto	strip_y(const symmetrical<E,4> &m)	{ return symmetrical<E,3>(m.d4.xzw, vec<E,3>(m.o.x, m.d3.z, m.o.z)); }
template<typename E> auto	strip_z(const symmetrical<E,4> &m)	{ return symmetrical<E,3>(m.d4.xyw, vec<E,3>(m.d3.x, m.o.yz)); }
template<typename E> auto	strip_w(const symmetrical<E,4> &m)	{ return symmetrical<E,3>(m.d4.xyz, vec<E,3>(m.d3.xy, m.o.x)); }

typedef symmetrical<float,4> symmetrical4;

template<typename E> struct column_s<symmetrical<E,4>, 0> { static force_inline vec<E,4> f(const symmetrical<E,4> &a) { return vec<E,4>(a.d4.x, a.d3.x, a.o.xz); } };
template<typename E> struct column_s<symmetrical<E,4>, 1> { static force_inline vec<E,4> f(const symmetrical<E,4> &a) { return vec<E,4>(a.d3.x, a.d4.y, a.d3.y, a.o.y); } };
template<typename E> struct column_s<symmetrical<E,4>, 2> { static force_inline vec<E,4> f(const symmetrical<E,4> &a) { return vec<E,4>(a.o.x, a.d3.y, a.d4.z, a.d3.z); } };
template<typename E> struct column_s<symmetrical<E,4>, 3> { static force_inline vec<E,4> f(const symmetrical<E,4> &a) { return vec<E,4>(a.o.zy, a.d3.z, a.d4.w); } };

template<typename E> symmetrical<E,4> cofactors(const symmetrical<E,4> &m) {
	vec<E,3>	x3(m.d4.x, m.d3.x, m.o.x), y3(m.d3.x, m.d4.y, m.d3.y), z3(m.o.x, m.d3.y, m.d4.z), w3(m.o.zy, m.d3.z);

	vec<E,3>	c0	= cross(z3, w3);
	vec<E,3>	c1	= z3 * m.d4.w - w3 * m.d3.z;
	vec<E,3>	dxy	= vec<E,3>(y3.y, x3.yz) * c1.zzx - vec<E,3>(y3.z, x3.zx) * c1.yyz + c0.xxy * m.o.yzz;

	vec<E,3>	c2	= cross(x3, y3);
	vec<E,3>	c3	= x3 * m.o.y - y3 * m.o.z;
	vec<E,3>	dz	= cross(w3, c3) + c2 * m.d4.w;
	vec<E,3>	dw	= cross(c3, z3) - c2 * m.d3.z;
#if 1
	//d4.y, d3.x wrong
	return _triangular4<E>(
		vec<E,4>(dxy.x, -dxy.z, dz.z, dot(c2, z3)),
		vec<E,3>(-dxy.y, dz.y, dw.z),
		vec<E,3>(dz.x, dw.y, dw.x)
	);
#else
	vec<E,3>	dx = cross(y3, c1) + c0 * m.o.y;
	vec<E,3>	dy = cross(x3, c1) + c0 * m.o.z;

	return _triangular4<E>(
		vec<E,4>(dx.x, -dy.y, dz.z, dot(c2, z3)),
		vec<E,3>(-dy.x, dz.y, dw.z),
		vec<E,3>(dz.x, dw.y, dw.x)
	);
#endif
}

template<typename E> vec<E,4> principal_component(const symmetrical<E,4> &s) {
	vec<E,4>	r;
	int	n = s.characteristic().roots(r);
	if (n == 0)
		return vec<E,4>(zero);

	vec<E,1>			e	= max_component(abs(r));
	symmetrical<E,4>	m	= symmetrical<E,4>(s.d4 - e, s.d3, s.o);
	symmetrical<E,4>	u	= ~m;
	return u.column(max_component_index(abs(u)));
}

template<typename E, int A, int B, int C, int D> force_inline symmetrical<E,4> outer_product(_v4<E,A,B,C,D> v) {
	return symmetrical<E,4>(v * v, perm<0,1,2>(v) * perm<1,2,3>(v), perm<0,1,3>(v) * perm<2,3,0>(v));
}
template<typename E> force_inline symmetrical<E,4> outer_product(vec<E,4> v) {
	return symmetrical<E,4>(v * v, v.xyz * v.yzw, v.xyw * v.zwx);
}

//-----------------------------------------------------------------------------

template<typename E> class mat<E,4,4> {
public:
	vec<E,4>	x, y, z, w;
	force_inline mat()	{}
	force_inline mat(const _identity&)				{ vec<E,2> t(zero, one); x = t.yxxx; y = t.xyxx; z = t.xxyx; w = t.xxxy; }
	force_inline mat(const diagonal<E,4> &m)			{ vec<E,4> s(m.d4.xyz, zero); x = s.xwww; y = s.wyww; z = s.wwzw; w = vec<E,2>(zero, m.d4.w).xxxy; }
	force_inline mat(const _zero&)					: x(zero), y(zero), z(zero), w(zero)	{}
	force_inline mat(const mat &m)					: x(m.x), y(m.y), z(m.z), w(m.w)		{}
	force_inline mat(const upper<E,4> &m)			: x(m.d4.x, vec<E,3>(zero)), y(m.d3.x, m.d4.y, vec<E,2>(zero)), z(m.o.x, m.d3.y, m.d4.z, zero), w(m.o.zy, m.d3.z, m.d4.w)	{}
	force_inline mat(const lower<E,4> &m)			: x(m.d4.x, m.d3.x, m.o.xz), y(zero, m.d4.y, m.d3.y, m.o.y), z(vec<E,2>(zero), m.d4.z, m.d3.z), w(vec<E,3>(zero), m.d4.w)	{}
	force_inline mat(const symmetrical<E,4> &m)		: x(m.d4.x, m.d3.x, m.o.xz), y(m.d3.x, m.d4.y, m.d3.y, m.o.y), z(m.o.x, m.d3.y, m.d4.z, m.d3.z), w(m.o.zy, m.d3.z, m.d4.w)	{}
	force_inline explicit mat(const mat<E,3,3> &m)	: x(m.x,zero), y(m.y,zero), z(m.z,zero), w(perm<0,0,0,1>(zero,one))		{}
	force_inline explicit mat(const mat<E,3,4> &m)	: x(m.x,zero), y(m.y,zero), z(m.z,zero), w(m.w,one)						{}
	template<typename X, typename Y, typename Z, typename W> force_inline mat(const X &x, const Y &y, const Z &z, const W &w) : x(x), y(y), z(z), w(w) {}

	//force_inline	mat&	operator=(const mat &m) { x = m.x; y = m.y; z = m.z; w = m.w; return *this; }
	force_inline vec<E,4>&	operator[](int i)			{ return (&x)[i]; }
	force_inline vec<E,4>	operator[](int i)	const	{ return (&x)[i]; }
	force_inline	vec<E,4>&	column(int i)				{ return (&x)[i]; }
	force_inline	vec<E,4>	column(int i)		const	{ return (&x)[i]; }

	vec<E,1>				det()				const;
	force_inline	vec<E,4>	diagonal()			const	{ return vec<E,4>(x.x, y.y, z.z, w.w); }
	force_inline	vec<E,1>	trace()				const	{ return x.x + y.y + z.z + w.w; }
	normalised_polynomial<4> characteristic()	const;
	force_inline vec<E,4>	scale()				const	{ return sqrt(x * x + y * y + z * z + w * w); }
	force_inline vec<E,4>	pre_scale()			const	{ return sqrt(vec<E,4>(len2(x), len2(y), len2(z), len2(w))); }

	force_inline vec<E,4>	row0()				const	{ return vec<E,4>(x.x, y.x, z.x, w.x); }
	force_inline vec<E,4>	row1()				const	{ return vec<E,4>(x.y, y.y, z.y, w.y); }
	force_inline vec<E,4>	row2()				const	{ return vec<E,4>(x.z, y.z, z.z, w.z); }
	force_inline vec<E,4>	row3()				const	{ return vec<E,4>(x.w, y.w, z.w, w.w); }

	friend bool operator==(const mat &a, const mat &b)		{ return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
	friend bool operator!=(const mat &a, const mat &b)		{ return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w; }
	friend mat operator+(const mat &a, const mat &b)		{ return mat(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
	friend mat operator-(const mat &a, const mat &b)		{ return mat(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
	friend mat operator+(const mat &a, const iso::diagonal<E,4> &b)	{ vec<E,4> s(b.d4.xyz, zero); return mat(a.x + s.xwww, a.y + s.wyww, a.z + s.wwzw, a.w + vec<E,2>(zero, b.d4.w).xxxy); }
	friend mat operator-(const mat &a, const iso::diagonal<E,4> &b)	{ vec<E,4> s(b.d4.xyz, zero); return mat(a.x - s.xwww, a.y - s.wyww, a.z - s.wwzw, a.w - vec<E,2>(zero, b.d4.w).xxxy); }
	friend mat	operator*(const mat &a, E b)				{ return a * vget<E>::r(b); }
	template<int N, int A>	friend mat operator*(const mat &a, const _v1<E,N,A> &b)	{ return mat(a.x * b, a.y * b, a.z * b, a.w * b); }
	template<int N, int A>	friend mat operator/(const mat &a, const _v1<E,N,A> &b)	{ return a * reciprocal(b); }

	friend mat get_transpose(const mat &m) {
		vec<E,4>	t0 = perm<0,2,1,3>(m.x.xy, m.z.xy);
		vec<E,4>	t1 = perm<0,2,1,3>(m.y.xy, m.w.xy);
		vec<E,4>	t2 = perm<0,2,1,3>(m.x.zw, m.z.zw);
		vec<E,4>	t3 = perm<0,2,1,3>(m.y.zw, m.w.zw);
		return mat(
			perm<0,2,1,3>(t0.xy, t1.xy),
			perm<0,2,1,3>(t0.zw, t1.zw),
			perm<0,2,1,3>(t2.xy, t3.xy),
			perm<0,2,1,3>(t2.zw, t3.zw)
		);
	}

#ifdef VEC_INLINE_INVERSE
	friend mat cofactors(const mat &m) {
		vec<E,3>	c0 = cross(m.z.xyz, m.w.xyz);
		vec<E,3>	c1 = m.z.xyz * m.w.w - m.w.xyz * m.z.w;
		vec<E,4>	dx = vec<E,4>(cross(m.y.xyz, c1) + c0 * m.y.w, -dot(c0, m.y.xyz));
		vec<E,4>	dy = vec<E,4>(cross(c1, m.x.xyz) - c0 * m.x.w,  dot(c0, m.x.xyz));

		vec<E,3>	c2 = cross(m.x.xyz, m.y.xyz);
		vec<E,3>	c3 = m.x.xyz * m.y.w - m.y.xyz * m.x.w;
		vec<E,4>	dz = vec<E,4>(cross(m.w.xyz, c3) + c2 * m.w.w, -dot(c2, m.w.xyz));
		vec<E,4>	dw = vec<E,4>(cross(c3, m.z.xyz) - c2 * m.z.w,  dot(c2, m.z.xyz));

		return mat(dx, dy, dz, dw);
	}

	friend mat	get_inverse_transpose(const mat &m) {
		mat	c = cofactors(m);
		return c * reciprocal(dot(m.x, c.x));
	}

#else
	friend mat cofactors(const mat &m);
	friend mat	get_inverse_transpose(const mat &m);
#endif

	friend transpose_s<mat> get_inverse(const mat &a)	{ return transpose(get_inverse_transpose(a)); }
	friend transpose_s<mat> operator~(const mat &a)		{ return transpose(cofactors(a)); }
};

template<typename E> auto	strip_x(const mat<E,4,4> &m)	{ return mat<E,3,3>(m.y.yzw, m.z.yzw, m.w.yzw); }
template<typename E> auto	strip_y(const mat<E,4,4> &m)	{ return mat<E,3,3>(m.x.xzw, m.z.xzw, m.w.xzw); }
template<typename E> auto	strip_z(const mat<E,4,4> &m)	{ return mat<E,3,3>(m.x.xyw, m.y.xyw, m.w.xyw); }
template<typename E> auto&	strip_w(const mat<E,4,4> &m)	{ return (const mat<E,3,3>&)m; }

typedef mat<float,4,4> float4x4;

template<typename E> vec<E,1> mat<E,4,4>::det() const {
	vec<E,3>	c0 = cross(z.xyz, w.xyz);
	vec<E,3>	c1 = z.xyz * w.w - w.xyz * z.w;
	vec<E,4>	dx = vec<E,4>(cross(y.xyz, c1) + c0 * y.w, -dot(c0, y.xyz));
	return dot(x, dx);
}

template<typename E>  transpose_s<mat<E,4,4>>	transpose(const mat<E,3,4> &m)			{ return transpose_s<mat<E,4,4>>((mat<E,4,4>)m); }
template<typename E>  auto						transpose(const inverse_s<mat<E,3,4>> &m)	{ return transpose(inverse((mat<E,4,4>)m.m)); }
template<typename E>  transpose_s<mat<E,4,4>>	get_transpose(const mat<E,3,4> &m)		{ return transpose((mat<E,4,4>)m); }

template<typename E> mat<E,4,4> pow(const mat<E,4,4> &x, int y) {
	vec<E,4>	r	= poly_div(y, 1, x.characteristic());
	mat<E,4,4>	x2	= x * x;
	return (x2 * x) * r.w + x2 * r.z + x * r.y + diagonal<E,4>(r.x);
}

template<typename E> inline symmetrical<E,4>			symmetrical<E,4>::make(const mat<E,4,4> &m)	{ return symmetrical<E,4>(m.diagonal(), vec<E,3>(m.y.x, m.z.y, m.w.z), vec<E,3>(m.z.x, m.w.y, m.w.x)); }
template<typename E> inline vec<E,1>					symmetrical<E,4>::det()				const	{ return mat<E,4,4>(*this).det(); }
template<typename E> inline normalised_polynomial<4>	symmetrical<E,4>::characteristic()	const	{ return mat<E,4,4>(*this).characteristic(); }

template<typename E> force_inline mat<E,2,2>::mat(const mat<E,4,4> &m) { v = (m.x.xy, m.y.xy); }
template<typename E> force_inline mat<E,2,3>::mat(const mat<E,4,4> &m) : mat<E,2,2>(m), z(m.w.xy)			{}
template<typename E> force_inline mat<E,3,3>::mat(const mat<E,4,4> &m) : x(m.x.xyz), y(m.y.xyz), z(m.z.xyz)	{}
template<typename E> force_inline mat<E,3,4>::mat(const mat<E,4,4> &m) : mat<E,3,3>(m), w(m.w.xyz)			{}

template<typename E> mat<E,4,4> cofactors(const mat<E,3,4> &m) {
	mat<E,3,3> c = cofactors((const mat<E,3,3>&)m);
	return mat<E,4,4>(
		vec<E,4>(c.x, dot(c.x, -m.w)),
		vec<E,4>(c.y, dot(c.y, -m.w)),
		vec<E,4>(c.z, dot(c.z, -m.w)),
		vec<E,4>(zero, zero, zero, dot(c.x, m.x))
	);
}

//template<typename B,typename E>	struct mul_t<mat<E,4,4>, B, IF_MAT(B,void)>		{ typedef mat<E,4,4>	type; };
//template<typename B,typename E>	struct mul_t<B, mat<E,4,4>, IF_MAT(B,void)>		{ typedef mat<E,4,4>	type; };
template<typename B,typename E>	struct mul_t<symmetrical<E,4>, B, IF_MAT(B,void)>	: mul_t<mat<E,4,4>, B> {};
template<typename E>			struct mul_t<mat<E,3,4>, symmetrical<E,4>>		{ typedef mat<E,4,4>	type; };

template<typename E, int N, int A, int B, int C> struct mul_t<mat<E,4,4>, _v3<E,N,A,B,C> >{ typedef vec<E,4> type; };
template<typename E, int A, int B, int C, int D> struct mul_t<mat<E,4,4>, _v4<E,A,B,C,D> >{ typedef vec<E,4> type; };

#ifdef __MWERKS__
template<typename E> force_inline vec<E,4>	operator*(const mat<E,4,4> &m, param_t<vec<E,3>> v)		{ vec<E,4> mv; wii::mul(&mv, &m, &v); return mv; }
template<typename E> force_inline vec<E,4>	operator*(const mat<E,4,4> &m, param_t<position3> v)		{ vec<E,4> mv; wii::mul(&mv, &m, &v); return mv; }
template<typename E> force_inline vec<E,4>	operator*(const mat<E,4,4> &m, param_t<vec<E,4>> v)		{ vec<E,4> mv; wii::mul(&mv, &m, &v); return mv; }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const mat<E,4,4> &b)		{ mat<E,4,4> m; wii::mul(&m, &a, &b); return m; }
template<typename E> force_inline void		composite(mat<E,4,4> &m, const mat<E,4,4> &a, const mat<E,4,4> &b)	{ m = a * b; }
#else
template<typename E> force_inline vec<E,4>	operator*(const mat<E,4,4> &m, vec<E,3> v)				{ return m.x * v.x + m.y * v.y + m.z * v.z; }
template<typename E> force_inline vec<E,4>	operator*(const mat<E,4,4> &m, pos<E,3> v)				{ return m.x * v.x + m.y * v.y + m.z * v.z + m.w; }
template<typename E> force_inline vec<E,4>	operator*(const mat<E,4,4> &m, vec<E,4> v)				{ return m.x * v.x + m.y * v.y + m.z * v.z + m.w * v.w; }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const mat<E,4,4> &b)		{ return mat<E,4,4>(a * b.x, a * b.y, a * b.z, a * b.w); }
template<typename E> force_inline void		composite(mat<E,4,4> &m, const mat<E,4,4> &a, const mat<E,4,4> &b)	{ m = a * b; }
#endif

template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const mat<E,3,4> &b)			{ return mat<E,4,4>(a * b.x, a * b.y, a * b.z, a * b.w); }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,3,3> &a, const mat<E,4,4> &b)			{ return mat<E,4,4>((a * b.x.xyz, b.x.w), (a * b.y.xyz, b.y.w), (a * b.z.xyz, b.z.w), (a * b.w.xyz, b.w.w)); }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const mat<E,3,3> &b)			{ return mat<E,4,4>(a * b.x, a * b.y, a * b.z, a.w); }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,3,4> &a, const mat<E,4,4> &b)			{ return mat<E,4,4>(a * b.x, a * b.y, a * b.z, a * b.w); }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const symmetrical<E,4> &b)	{ return a * mat<E,4,4>(b); }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const diagonal<E,4> &b)		{ return mat<E,4,4>(a.x * b.d4.x, a.y * b.d4.y, a.z * b.d4.z, a.w * b.d4.w); }
template<typename E> force_inline mat<E,4,4>	operator*(const diagonal<E,4> &a, const mat<E,4,4> &b)		{ return mat<E,4,4>(b.x * a.d4, b.y * a.d4, b.z * a.d4, b.w * a.d4); }

template<typename E, int N, int A, int B, int C> force_inline vec<E,4> operator*(const mat<E,4,4> &m, const _v3<E,N,A,B,C> &v)	{ return m.x * perm<0>(v) + m.y * perm<1>(v) + m.z * perm<2>(v); }
template<typename E, int A, int B, int C, int D> force_inline vec<E,4> operator*(const mat<E,4,4> &m, const _v4<E,A,B,C,D> &v)	{ return m.x * perm<0>(v) + m.y * perm<1>(v) + m.z * perm<2>(v) + m.w * perm<2>(v); }

#ifdef USE_VECDOT
template<typename E> force_inline vec<E,4>	operator*(const transpose_s<mat<E,4,4>&> &a, vec<E,3> b)			{ return vec<E,4>(dot(a.m.x.xyz,b), dot(a.m.y.xyz,b), dot(a.m.z.xyz,b), dot(a.m.w.xyz,b)); }
template<typename E> force_inline vec<E,4>	operator*(const transpose_s<mat<E,4,4>&> &a, vec<E,4> b)			{ return vec<E,4>(dot(a.m.x,b), dot(a.m.y,b), dot(a.m.z,b), dot(a.m.w,b)); }
template<typename E> force_inline mat<E,4,4>	operator*(const transpose_s<mat<E,4,4>&> &a, const mat<E,4,4> &b)	{ return mat<E,4,4>(a * b.x, a * b.y, a * b.z, a * b.w); }
template<typename E> force_inline vec<E,4>	operator*(const transpose_s<mat<E,4,4>> &a, vec<E,3> b)				{ return vec<E,4>(dot(a.m.x.xyz,b), dot(a.m.y.xyz,b), dot(a.m.z.xyz,b), dot(a.m.w.xyz,b)); }
template<typename E> force_inline vec<E,4>	operator*(const transpose_s<mat<E,4,4>> &a, vec<E,4> b)				{ return vec<E,4>(dot(a.m.x,b), dot(a.m.y,b), dot(a.m.z,b), dot(a.m.w,b)); }
template<typename E> force_inline mat<E,4,4>	operator*(const transpose_s<mat<E,4,4>> &a, const mat<E,4,4> &b)	{ return mat<E,4,4>(a * b.x, a * b.y, a * b.z, a * b.w); }
#endif

template<typename E> normalised_polynomial<4> mat<E,4,4>::characteristic() const {
	vec<E,1>	t1	= trace();
	mat<E,4,4>	m2	= *this * *this;
	vec<E,1>	t2	= m2.trace();
	mat<E,4,4>	m3	= m2 * *this;
	vec<E,1>	t3	= m3.trace();
	mat<E,4,4>	m4	= m3 * *this;
	vec<E,1>	t4	= m4.trace();

	return float4(
		(t1 * (t1 * (square(t1) - t2 * 6) + t3 * 8) + square(t2) * 3 - t4 * 6) / 24,	//should=det
		(t1 * (-square(t1) + t2 * 3) - t3 * 2) / 6,
		(square(t1) - t2) / 2,
		-t1
	);
}

//-----------------------------------------------------------------------------
// quaternion
//-----------------------------------------------------------------------------

template<typename E> vec<E,4> quat_from_mat(const mat<E,3,3> &mat) {
	vec<E,3>	x	= mat.x,  y  = mat.y,  z  = mat.z;
	vec<E,4>	a	= vec<E,4>(one, z.y, x.z, y.x);
	vec<E,4>	b	= perm<0,1,2,2>((x.x, -y.z, zero));
	vec<E,4>	c	= perm<0,2,1,2>((y.y, -z.x, zero));
	vec<E,4>	d	= perm<0,2,2,1>((z.z, -x.y, zero));

	vec<E,4>	q;
	q	= a + b + c + d;
	if (q.x > one)
		return perm<1,2,3,0>(q) * (half * rsqrt(q.x));

	q	= a + b - c - d;
	if (q.x > one)
		return perm<0,3,2,1>(q) * (half * rsqrt(q.x));

	q	= a - b + c - d;
	if (q.x > one)
		return perm<3,0,1,2>(q) * (half * rsqrt(q.x));

	q	= a - b - c + d;
	return perm<2,1,0,3>(q) * (half * rsqrt(q.x));
}

template<typename E> class quat : public vec<E,4> {
	typedef vec<E,4>	V;
public:
	force_inline	quat() 									{}
	force_inline quat(const _identity&)	: V(perm<0,0,0,1>(zero, one))	{}
	force_inline	quat(param_t<V> a)		: V(a)			{}
	template<int A, int B, int C, int D>						force_inline quat(const _v4<E,A,B,C,D> &a)	: V(a) {}
	template<typename A, typename B>							force_inline	quat(const A &a, const B &b)	: V(a, b)	{}
	template<typename A, typename B, typename C>				force_inline quat(const A &a, const B &b, const C &c)	: V(a,b,c) {}
	template<typename A, typename B, typename C, typename D>	force_inline quat(const A &a, const B &b, const C &c, const D &d)	: V(a,b,c,d) {}
	force_inline quat(const float3x3 &mat) : V(quat_from_mat(mat))	{}

	force_inline quat&	operator*=(param_t<quat> b)		{ V::xyzw = V(b.xyz * V::w + V::xyz * b.w - cross(V::xyz, b.xyz), V::w * b.w - dot(V::xyz, b.xyz)); return *this; }

	force_inline quat	closest(param_t<quat> q) const	{ return V::xyzw * sign1(dot(V::xyzw, q.xyzw)); }

	static quat	between(param_t<vec<E,3>> a, param_t<vec<E,3>> b) {
		auto	half	= a + b;
		auto	w		= dot(half, a);
		return abs(w) < 1e-4f ? V(perm<1,0,0,0>(zero, one)) : V(normalise((cross(half, a), w)));
	}
	static quat	from_euler(param_t<vec<E,3>> v);

	operator		mat<E,3,3>()	const;
	quat			operator-()		const				{ return -V::xyzw; }
	quat			operator~()		const				{ return quat(-V::xyz, V::w); }

	friend V		operator+(param_t<quat> a, param_t<quat> b)	{ return a.xyzw + b.xyzw; }
	friend V		operator-(param_t<quat> a, param_t<quat> b)	{ return a.xyzw - b.xyzw; }
	friend quat		normalise(param_t<quat> a)					{ return normalise((const V&)a); }
	friend float1	norm2(param_t<quat> a)						{ return dot(a.xyzw, a.xyzw); }
	friend float1	norm(param_t<quat> a)						{ return sqrt(norm2(a)); }
	friend float1	cosang(param_t<quat> a, param_t<quat> b)	{ return dot(a.xyzw, b.xyzw); }
	friend quat		inverse(param_t<quat> a)					{ return ~a; }

	template<typename Y> friend quat	pow(param_t<quat> q, const Y &y) {
		auto	t	= len(q.xyz);
		auto	s	= sin(asin(t) * y);
		return quat(q.xyz * (t ? s / t : 1), sin_cos(s));
	}
	template<typename Y> friend quat	exp(param_t<quat> q) {
		auto		t	= len(q.xyz);
		vec<E,2>	sc	= sincos(t);
		return V(q.xyz * sc.y / t, sc.x) * exp(q.w);
	}
};
typedef quat<float>	quaternion;

template<typename E> struct vec_traits<quat<E>>		: vec_traits<vec<E,4>> {};

template<typename E> struct vget<quat<E>> {
	typedef _v4<E,0,1,2,3>	L, R;
	typedef	quat<E>		O;
	static force_inline L l(param_t<O> t)			{ return t.xyzw; }
	static force_inline R r(param_t<O> t)			{ return t.xyzw; }
};

template<typename E> quat<E>::operator mat<E,3,3>() const {
	vec<E,4>	v	= V::xyzw + V::xyzw;
	vec<E,3>	d	= v.xyz * V::xyz;
	vec<E,3>	t	= v.xyz * V::yzx;
	vec<E,3>	u	= v.xyz * V::w;
	vec<E,3>	a	= t.xyz - u.zxy;
	vec<E,3>	b	= t.zxy + u.yzx;

	d	= vec<E,3>(one) - d.yzx - d.zxy;
	return mat<E,3,3>(
		float3(d.x, a.x, b.x),
		float3(b.y, d.y, a.y),
		float3(a.z, b.z, d.z)
	);
}

//template<typename E, typename T> force_inline IF_SCALAR(T,vec<E,4>) operator*(quat<E> a, const T &b)	{ return a.xyzw * b; }

template<typename E> struct mul_t<quat<E>, quat<E>>		{ typedef quat<E>		type; };
template<typename E> struct mul_t<quat<E>, vec<E,3>>	{ typedef vec<E,3>		type; };
template<typename E> struct mul_t<quat<E>, mat<E,3,3>>	{ typedef mat<E,3,3>	type; };
template<typename E> struct mul_t<mat<E,3,3>, quat<E>>	{ typedef mat<E,3,3>	type; };
template<typename E> struct mul_t<mat<E,3,4>, quat<E>>	{ typedef mat<E,3,4>	type; };
template<typename E> struct mul_t<quat<E>, mat<E,3,4>>	{ typedef mat<E,3,4>	type; };
template<typename E> struct mul_t<quat<E>, mat<E,4,4>>	{ typedef mat<E,4,4>	type; };
template<typename E> struct mul_t<mat<E,4,4>, quat<E>>	{ typedef mat<E,4,4>	type; };
template<typename E, int N, int A, int B, int C> struct mul_t<quat<E>, _v3<E,N,A,B,C> >{ typedef vec<E,3> type; };

template<typename E> force_inline quat<E>	operator*(quat<E> a, quat<E> b)				{ return quat<E>(b.xyz * a.w + a.xyz * b.w - cross(a.xyz, b.xyz), a.w * b.w - dot(a.xyz, b.xyz)); }
template<typename E> force_inline vec<E,3>	operator*(quat<E> a, param_t<vec<E,3>> b)	{ return b + twice(cross(a.xyz, cross(a.xyz, b) - b * a.w)); }
template<typename E> force_inline pos<E,3>	operator*(quat<E> a, param_t<pos<E,3>> b)	{ return b + twice(cross(a.xyz, cross(a.xyz, b) - b * a.w)); }
template<typename E> force_inline mat<E,3,3>	operator*(quat<E> a, const mat<E,3,3> &b)	{ return a.operator mat<E,3,3>() * b; }
template<typename E> force_inline mat<E,3,3>	operator*(const mat<E,3,3> &a, quat<E> b)	{ return a * b.operator mat<E,3,3>(); }
template<typename E> force_inline mat<E,3,4>	operator*(quat<E> a, const mat<E,3,4> &b)	{ return a.operator mat<E,3,3>() * b; }
template<typename E> force_inline mat<E,3,4>	operator*(const mat<E,3,4> &a, quat<E> b)	{ return a * b.operator mat<E,3,3>(); }
template<typename E> force_inline mat<E,4,4>	operator*(quat<E> a, const mat<E,4,4> &b)	{ return a.operator mat<E,3,3>() * b; }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, quat<E> b)	{ return a * b.operator mat<E,3,3>(); }
template<typename E, int N, int A, int B, int C> force_inline vec<E,3> operator*(quat<E> a, const _v3<E,N,A,B,C> &b)	{ return b + twice(cross(a.xyz, cross(a.xyz, b) - b * a.w)); }
template<typename E> force_inline quat<E>	operator/(quat<E> a, quat<E> b)	{ return ~b * a; }

template<typename E> quat<E> quat<E>::from_euler(param_t<vec<E,3>> v) {
	vec<E,3> s, c;
	sincos(v * -half, &s, &c);
	return	quat<E>(zero, s.y, zero, c.y)
		*	quat<E>(s.x, zero, zero, c.x)
		*	quat<E>(zero, zero, s.z, c.z);
}

template<typename E> force_inline quat<E>	lerp_check(quat<E> a, quat<E> b, float t) { return lerp(a, b.closest(a), t); }

template<typename E, typename T> quat<E>	slerp(quat<E> a, quat<E> b, T t) {
	E			cosom = cosang(a, b);
	vec<E,2>	scale(one - t, sign1(cosom) * t);
	cosom = abs(cosom);
	if (cosom < 0.99f) {
		scale	= sin(scale * acos(cosom)) * rsqrt(one - square(cosom));
		return quat<E>(a * scale.x + b * scale.y);
	}
	return quat<E>(normalise(a * scale.x + b * scale.y));
}

template<typename E, typename T> quat<E>	squad(quat<E> q0, quat<E> a, quat<E> b, quat<E> q1, T t) {
	return slerp(slerp(q0, q1, t), slerp(a, b, t), 2 * (1 - t) * t);
}

template<typename E> quat<E>				diagonalise(param_t<mat<E,3,3>> m);
template<typename E> quat<E>				diagonalise(param_t<symmetrical<E,3>> m);

//force_inline vec<float,3>	operator*(param_t<quaternion> a, param_t<vec<float,3>> b);//	{ return b + twice(cross(a.xyz, cross(a.xyz, b) - b * a.w)); }
//force_inline quaternion	operator*(quaternion a, quaternion b);//	{ return b + twice(cross(a.xyz, cross(a.xyz, b) - b * a.w)); }
//template<typename E>  quat<E>	operator*(quat<E> a, quat<E> b);//	{ return b + twice(cross(a.xyz, cross(a.xyz, b) - b * a.w)); }
//template<typename E>  quat<E>	operator*(typename T_param<quat<E>>::type a, typename T_param<quat<E>>::type b);//	{ return b + twice(cross(a.xyz, cross(a.xyz, b) - b * a.w)); }

//-----------------------------------------------------------------------------
// rotations
//-----------------------------------------------------------------------------

template<typename T> force_inline float2x2 _rotate(const T &sc) {
	return float2x2(perm<0,1,3,0>(sc, -sc));
}
template<typename T> force_inline float2x2 rotate(const T &t) {
	return _rotate(sincos(t));
}

inline float2x2 quadrant_rotate(int q) {
	float4	v = q & 1 ? float4(perm<0,1,2,0>(zero, one, -one)) : float4(perm<1,0,0,1>(zero, one));
	return q & 2 ? float4(-v) : v;
}

// holds rotation as axis * sin(theta)
template<typename E> struct rotvec {
	vec<E,3> v;
	rotvec()							{}
	rotvec(const _identity&) : v(zero)	{}
	rotvec(param_t<vec<E,3>> v) : v(v)	{}
	template<int N, int A, int B, int C> rotvec(const _v3<E,N,A,B,C> &v)	: v(v) {}
	rotvec(quat<E> q)	: v(q.xyz * q.w * two)		{}
	rotvec(const mat<E,3,3> &m);

	operator mat<E,3,3>() const;
	operator quat<E>() const {
		return quat<E>(normalise((v, two)));
	}
	static inline rotvec between(param_t<vec<E,3>> a, param_t<vec<E,3>> b) {
		return cross(normalise(b), normalise(a));
	}
};
typedef rotvec<float>	rotation_vector;

template<typename E> rotvec<E>::rotvec(const mat<E,3,3> &m) {
	auto		cosang	= (m.trace() - 1) * half;
	vec<E,3>	r		= vec<E,3>(m.z.y - m.y.z, m.x.z - m.z.x, m.y.x - m.x.y) * half;

	if (abs(cosang) > rsqrt2) {
		v = r;

	} else {
		vec<E,3>	d	= m.diagonal() - cosang;
		vec<E,3>	t	= (vec<E,3>(m.x.y, m.y.z, m.z.x) + vec<E,3>(m.y.x, m.z.y, m.x.z)) * half;
		vec<E,3>	r2;
		switch (max_component_index(d * d)) {
			case 0:		r2 = vec<E,3>(d.x, t.x, t.z);
			case 1:		r2 = vec<E,3>(t.x, d.y, t.y);
			default:	r2 = vec<E,3>(t.z, t.y, d.z);
		}

		auto	sinang	= len(r);
		if (dot(r2, r) < zero)
			sinang = -sinang;

		v = normalise(r2) * sinang;
	}
}

//rodrigues formula
template<typename E> rotvec<E>::operator mat<E,3,3>() const {
	auto	sin2	= len2(v);
	auto	scale	= one / (one + sqrt(one - sin2));
	auto	v2		= v * v;

	vec<E,3>	d	= vec<E,3>(one) - (v2.yzx + v2.zxy) * scale;
	vec<E,3>	a	= v;
	vec<E,3>	b	= v.xyz * v.yzx * scale;

	return mat<E,3,3>(
		vec<E,3>(d.x, b.x - a.z, b.z + a.y),
		vec<E,3>(b.x + a.z, d.y, b.y - a.x),
		vec<E,3>(b.z - a.y, b.y + a.x, d.z)
	);
}

// holds rotation as axis * theta, which corresponds to log(rotvec)
// (e.g. adding is equivalent to multiplying rotations, etc)
template<typename E> struct logrot {
	vec<E,3> v;
	logrot()							{}
	logrot(const _zero&) : v(zero)		{}
	logrot(param_t<vec<E,3>> v) : v(v)	{}
	template<int N, int A, int B, int C> logrot(const _v3<E,N,A,B,C> &v)	: v(v) {}

	friend quaternion	exp(const logrot &x) {
		auto		t	= len(x.v);
		vec<E,2>	sc	= sincos(t);
		return quat<E>(t > sc.y ? x.v * sc.y / t : x.v, sc.x);
	}
	friend logrot operator-(const logrot &x)							{ return -x.v; }
	friend logrot operator+(const logrot &x, const logrot &y)			{ return x.v + y.v; }
	friend logrot operator-(const logrot &x, const logrot &y)			{ return x.v - y.v; }
	template<typename Y> friend logrot operator*(const logrot &x, Y y)	{ return x.v * y;}
	friend bool operator==(const logrot &x, const logrot &y)			{ return all(x.v == y.v); }
	friend bool operator!=(const logrot &x, const logrot &y)			{ return !(x == y); }
};
typedef logrot<float>	log_rotation;

template<typename E> logrot<E> log(const rotvec<E> &r) {
//	auto	s	= len(r.v);
//	return s + one > one ? r.v * (asin(s) / s * half) : r.v;
	auto	s	= len(r.v);
	return s ? r.v * atan2(s, 2) / s : r.v;
//	return log(r.operator quat<E>());
}
template<typename E> logrot<E> log(quat<E> q) {
	auto	s	= len(q.xyz);
	return s ? q.xyz * atan2(s, q.w) / s : float3(zero);
}
template<typename E> logrot<E> log(const mat<E,3,3> &m) {
	auto		cosang	= (m.trace() - 1) * half;
	vec<E,3>	r		= float3(m.z.y - m.y.z, m.x.z - m.z.x, m.y.x - m.x.y) * half;
	auto		sinang	= len(r);

	if (cosang > rsqrt2)
		return sinang > zero ? r * (asin(sinang) / sinang * half) : vec<E,3>(zero);

	if (cosang > -rsqrt2)
		return r * (acos(cosang) / sinang * half);

	vec<E,3>	d		= m.diagonal() - cosang;
	vec<E,3>	t		= (vec<E,3>(m.x.y, m.y.z, m.z.x) + vec<E,3>(m.y.x, m.z.y, m.x.z)) * half;
	vec<E,3>	r2;
	switch (max_component_index(d * d)) {
		case 0:		r2 = vec<E,3>(d.x, t.x, t.z);
		case 1:		r2 = vec<E,3>(t.x, d.y, t.y);
		default:	r2 = vec<E,3>(t.z, t.y, d.z);
	}

	auto	angle	= pi - asin(sinang);
	if (dot(r2, r) < zero)
		angle = -angle;

	return normalise(r2) * (angle * half);
}

template<typename E> quat<E> squadseg(const quat<E> *qb, quat<E> q0, quat<E> q1, const quat<E> *qa, E t) {
	auto a = qb ? q0 * exp(logrot<E>((q1 * log(q0).v + *qb * log(q0).v) * .25f)) : q0;
	auto b = qa ? q1 * exp(logrot<E>((*qa * log(q1).v + q0 * log(q1).v) * .25f)) : q1;
	return squad(q0, a, b, q1, t);
}

symmetrical3 rotation_pi(param_t<float3> v);

template<>	struct axis_s<0> {
	static const float4_const	v;
	template<typename T>				static force_inline	float3x3		mat(const T &sc)				{ return float3x3(perm<1,0,0>(zero, one), perm<0,1,2>(zero, sc), (zero, -perm<1>(sc), perm<0>(sc))); }
	template<typename T>				static force_inline	quaternion		quat(const T &sc)				{ return quaternion(perm<2,0,0,1>(zero, sc)); }
	template<typename T>				static force_inline	log_rotation	log(const T &a)					{ return log_rotation(perm<1,0,0>(zero, a)); }
	template<typename T>				static force_inline	rotation_vector	rot(const T &s)					{ return rotation_vector(perm<1,0,0>(zero, s)); }
	template<typename T, typename V>	static force_inline	V				rot(const V &v, const T &sc)	{ V v2(v); v2.yz = v2.yz * perm<0>(sc) + v.zy * (perm<1>(sc), -perm<1>(sc)); return v2; }
	force_inline	operator float3()	const	{ return float4(v).xyz; }
	force_inline	operator float2()	const	{ return float4(v).xy; }
};
template<>	struct axis_s<1> {
	static const float4_const	v;
	template<typename T>				static force_inline	float3x3		mat(const T &sc)				{ return float3x3((perm<0>(sc), zero, -perm<1>(sc)), perm<0,1,0>(zero, one), perm<2,0,1>(zero, sc)); }
	template<typename T>				static force_inline	quaternion		quat(const T &sc)				{ return quaternion(perm<0,2,0,1>(zero, sc)); }
	template<typename T>				static force_inline	log_rotation	log(const T &a)					{ return log_rotation(perm<0,1,0>(zero, a)); }
	template<typename T>				static force_inline	rotation_vector	rot(const T &s)					{ return rotation_vector(perm<0,1,0>(zero, s)); }
	template<typename T, typename V>	static force_inline	V				rot(const V &v, const T &sc)	{ V v2(v); v2.zx = v2.zx * perm<0>(sc) + v.zx * (-perm<1>(sc), perm<1>(sc)); return v2; }
	force_inline	operator float3()	const	{ return float4(v).xyz; }
	force_inline	operator float2()	const	{ return float4(v).xy; }
};
template<>	struct axis_s<2> {
	static const float4_const	v;
	template<typename T>				static force_inline	float3x3		mat(const T &sc)				{ return float3x3(perm<1,2,0>(zero, sc), (-perm<1>(sc), perm<0>(sc), zero), perm<0,0,1>(zero, one)); }
	template<typename T>				static force_inline	quaternion		quat(const T &sc)				{ return quaternion(perm<0,0,2,1>(zero, sc)); }
	template<typename T>				static force_inline	log_rotation	log(const T &a)					{ return log_rotation(perm<0,0,1>(zero, a)); }
	template<typename T>				static force_inline	rotation_vector	rot(const T &s)					{ return rotation_vector(perm<0,0,1>(zero, s)); }
	template<typename T, typename V>	static force_inline	V				rot(const V &v, const T &sc)	{ V v2(v); v2.xy = v2.xy * perm<0>(sc) + v.yx * (perm<1>(sc), -perm<1>(sc)); return v2; }
	force_inline	operator float3()	const	{ return float4(v).xyz; }
};
template<>	struct axis_s<3> {
	static const float4_const	v;
};

template<int A> struct vget<axis_s<A> > : vdims<0> {
	typedef vf4	R;
	static force_inline R r(const axis_s<A>&)	{ return axis_s<A>::v; }
};

template<int A>	struct neg_axis_s {
	force_inline	operator float3()		const	{ return -float4(axis_s<A>::v).xyz; }
};

template<int A> force_inline	neg_axis_s<A>	operator-(const axis_s<A>&)		{ return neg_axis_s<A>(); }
template<int A> force_inline	axis_s<A>		operator-(const neg_axis_s<A>&)	{ return axis_s<A>(); }

template<int A> struct vget<neg_axis_s<A> > {
	typedef vf4	R;
	static force_inline R r(const axis_s<A>&)	{ return -neg_axis_s<A>::v; }
};

template<typename T> inline auto dot(const T &t, const axis_s<0>&) { return t.x; }
template<typename T> inline auto dot(const T &t, const axis_s<1>&) { return t.y; }
template<typename T> inline auto dot(const T &t, const axis_s<2>&) { return t.z; }
template<typename T> inline auto dot(const T &t, const axis_s<3>&) { return t.w; }

template<typename T> inline auto dot(const T &t, const neg_axis_s<0>&) { return -t.x; }
template<typename T> inline auto dot(const T &t, const neg_axis_s<1>&) { return -t.y; }
template<typename T> inline auto dot(const T &t, const neg_axis_s<2>&) { return -t.z; }
template<typename T> inline auto dot(const T &t, const neg_axis_s<3>&) { return -t.w; }

template<int A, typename T> struct rotate_in_axis {
	const T	&t;
	force_inline	rotate_in_axis(const T &t) : t(t)	{}
	force_inline	operator		quaternion()		const { return axis_s<A>::quat(sincos(t * -half)); }
	force_inline	operator		float3x3()			const { return axis_s<A>::mat(sincos(t)); }
	force_inline	operator		rotation_vector()	const { return axis_s<A>::rot(sin(t)); }
	force_inline	friend log_rotation	log(const rotate_in_axis &a)	{ return axis_s<A>::log(a.t * -half); }
};

template<typename T> force_inline rotate_in_axis<0,T>	rotate_in_x(const T &t) { return t; }
template<typename T> force_inline rotate_in_axis<1,T>	rotate_in_y(const T &t) { return t; }
template<typename T> force_inline rotate_in_axis<2,T>	rotate_in_z(const T &t) { return t; }

template<typename T> force_inline quaternion	_rotate_axis(param_t<float3> axis, const T &sc)	{ return quaternion(axis * perm<1>(sc), perm<0>(sc)); }
template<typename T> force_inline quaternion	rotate_axis(param_t<float3> axis, const T &t)	{ return _rotate_axis(axis, sincos(t * -half)); }

template<int A, typename T> force_inline rotate_in_axis<A,T> get_inverse(const rotate_in_axis<A,T> &m) { return rotate_in_axis<A,T>(-m.t); }

template<>											struct mul_t<rotation_vector, rotation_vector>				{ typedef quaternion			type; };
template<>											struct mul_t<quaternion, rotation_vector>					{ typedef quaternion			type; };
template<>											struct mul_t<rotation_vector, quaternion>					{ typedef quaternion			type; };
template<int A, typename T>							struct mul_t<rotate_in_axis<A,T>, rotation_vector>			{ typedef quaternion			type; };
template<int A, typename T>							struct mul_t<rotation_vector, rotate_in_axis<A,T> >			{ typedef quaternion			type; };

template<int A, typename T1, typename T2>			struct mul_t<rotate_in_axis<A,T1>, rotate_in_axis<A,T2> >	{ typedef rotate_in_axis<A,T1>	type; };
template<int A1, int A2, typename T1, typename T2>	struct mul_t<rotate_in_axis<A1,T1>, rotate_in_axis<A2,T2> >	{ typedef quaternion			type; };
template<typename E, int A, typename T>				struct mul_t<rotate_in_axis<A,T>, quat<E>>					{ typedef quat<E>				type; };
template<typename E, int A, typename T>				struct mul_t<quat<E>, rotate_in_axis<A,T> >					{ typedef quat<E>				type; };
template<typename E, int A, typename T>				struct mul_t<rotate_in_axis<A,T>, vec<E,3>>					{ typedef vec<E,3>				type; };
template<typename E, int A, typename T>				struct mul_t<rotate_in_axis<A,T>, pos<E,3>>					{ typedef pos<E,3>				type; };
template<typename E, int A, typename T>				struct mul_t<rotate_in_axis<A,T>, vec<E,4>>					{ typedef vec<E,4>				type; };
template<typename E, int A, typename T>				struct mul_t<rotate_in_axis<A,T>, mat<E,3,3>>				{ typedef mat<E,3,3>			type; };
template<typename E, int A, typename T>				struct mul_t<mat<E,3,3>, rotate_in_axis<A,T> >				{ typedef mat<E,3,3>			type; };
template<typename E, int A, typename T>				struct mul_t<rotate_in_axis<A,T>, mat<E,3,4>>				{ typedef mat<E,3,4>			type; };
template<typename E, int A, typename T>				struct mul_t<mat<E,3,4>,rotate_in_axis<A,T> >				{ typedef mat<E,3,4>			type; };
template<typename E, int A, typename T>				struct mul_t<rotate_in_axis<A,T>, mat<E,4,4>>				{ typedef mat<E,4,4>			type; };
template<typename E, int A, typename T>				struct mul_t<mat<E,4,4>, rotate_in_axis<A,T> >				{ typedef mat<E,4,4>			type; };

force_inline quaternion		operator*(const rotation_vector &a, const rotation_vector &b)	{ return a.operator quaternion() * b.operator quaternion(); }
force_inline quaternion		operator*(param_t<quaternion> a, const rotation_vector &b)		{ return a * b.operator quaternion(); }
force_inline quaternion		operator*(const rotation_vector &a, param_t<quaternion> b)		{ return a.operator quaternion() * b; }
template<int A, typename T>	force_inline	quaternion		operator*(const rotate_in_axis<A,T> &a, const rotation_vector &b)	{ return a * b.operator quaternion(); }
template<int A, typename T> force_inline	quaternion		operator*(const rotation_vector &a, const rotate_in_axis<A,T> &b)	{ return a.operator quaternion() * b; }

template<int A, typename T1, typename T2>			force_inline	rotate_in_axis<A,T1>	operator*(const rotate_in_axis<A,T1> &a, const rotate_in_axis<A,T2> &b)		{ return rotate_in_axis<A,T1>(a.t + b.t); }
template<int A1, int A2, typename T1, typename T2>	force_inline	quaternion				operator*(const rotate_in_axis<A1,T1> &a, const rotate_in_axis<A2,T2> &b)	{ return a.operator quaternion() * b.operator quaternion(); }
template<typename E, int A, typename T> force_inline vec<E,3>		operator*(const rotate_in_axis<A,T> &a, vec<E,3> b)				{ return axis_s<A>::rot(b, sincos(a.t)); }
template<typename E, int A, typename T> force_inline pos<E,3>		operator*(const rotate_in_axis<A,T> &a, pos<E,3> b)				{ return axis_s<A>::rot(b, sincos(a.t)); }
template<typename E, int A, typename T> force_inline vec<E,4>		operator*(const rotate_in_axis<A,T> &a, vec<E,4> b)				{ return axis_s<A>::rot(b, sincos(a.t)); }
template<typename E, int A, typename T> force_inline quat<E>			operator*(const rotate_in_axis<A,T> &a, quat<E> b)				{ return (quat<E>)a * b; }
template<typename E, int A, typename T> force_inline quat<E>			operator*(quat<E> a, const rotate_in_axis<A,T> &b)				{ return a * b.operator quat<E>(); }
template<typename E, int A, typename T> force_inline mat<E,3,3>		operator*(const rotate_in_axis<A,T> &a, const mat<E,3,3> &b)	{ return (mat<E,3,3>)a * b; }
template<typename E, int A, typename T> force_inline mat<E,3,3>		operator*(const mat<E,3,3> &a, const rotate_in_axis<A,T> &b)	{ return a * (mat<E,3,3>)b; }
template<typename E, int A, typename T> force_inline mat<E,3,4>		operator*(const rotate_in_axis<A,T> &a, const mat<E,3,4> &b)	{ return (mat<E,3,3>)a * b; }
template<typename E, int A, typename T> force_inline mat<E,3,4>		operator*(const mat<E,3,4> &a, const rotate_in_axis<A,T> &b)	{ return a * (mat<E,3,3>)b; }
template<typename E, int A, typename T> force_inline mat<E,4,4>		operator*(const rotate_in_axis<A,T> &a, const float4x4 &b)		{ return (mat<E,3,3>)a * b; }
template<typename E, int A, typename T> force_inline mat<E,4,4>		operator*(const mat<E,4,4> &a, const rotate_in_axis<A,T> &b)	{ return a * (mat<E,3,3>)b; }

//-----------------------------------------------------------------------------
// scales
//-----------------------------------------------------------------------------

template<typename T> struct scale_s {
	const T	t;
	force_inline	scale_s(const T &t) : t(t)	{}
	template<typename E> force_inline	operator mat<E,2,2>()	const { return mat<E,2,2>(perm<0,2,2,1>((vec<E,2>(t), zero))); }
	template<typename E> force_inline	operator mat<E,3,3>()	const { vec<E,4> s(vec<E,3>(t), zero); return mat<E,3,3>(s.xww, s.wyw, s.wwz); }
	friend const scale_s&	transpose(const scale_s &m)	{ return m; }
	friend scale_s&&		transpose(scale_s &&m)		{ return (scale_s&&)m; }
};

template<typename T> struct mat_traits<scale_s<T> > { enum {is_mat = true}; };

template<typename T>							force_inline scale_s<T>		scale(const T &t)							{ return scale_s<T>(t); }
template<typename A, typename B>				force_inline scale_s<float2>	scale(const A &a, const B &b)				{ return scale(float2(a, b)); }
template<typename A, typename B, typename C>	force_inline scale_s<float3>	scale(const A &a, const B &b, const C &c)	{ return scale(float3(a, b, c)); }

template<typename T> force_inline scale_s<T>	get_inverse(const scale_s<T> &m) { return scale_s<T>(reciprocal(m.t)); }

template<typename T> force_inline const inverse_s<scale_s<T> >&	transpose(const inverse_s<scale_s<T> > &m)	{ return m; }
template<typename T> force_inline inverse_s<scale_s<T> >&&		transpose(inverse_s<scale_s<T> > &&m)		{ return move(m); }

template<typename E, int N, int M, typename S>			struct mul_t<scale_s<S>, mat<E, N, M> >			{ typedef mat<E, N, M> type; };
template<typename E, int N, int M, typename S>			struct mul_t<mat<E, N, M>, scale_s<S>>			{ typedef mat<E, N, M> type; };

template<typename T1, typename T2>			struct mul_t<scale_s<T1>, transpose_s<T2> >			: T_noref<T2> {};
template<typename T1, typename T2>			struct mul_t<scale_s<T1>, scale_s<T2> >				{ typedef scale_s<T1>	type; };
template<typename T1, int A, typename T2>	struct mul_t<scale_s<T1>, rotate_in_axis<A,T2> >	{ typedef float3x3		type; };
template<int A, typename T1, typename T2>	struct mul_t<rotate_in_axis<A,T2>, scale_s<T1> >	{ typedef float3x3		type; };

template<typename T1, typename T2>						force_inline scale_s<T1>	operator*(const scale_s<T1> &a, const scale_s<T2> &b)			{ return a.t * b.t; }
template<typename T1, int A, typename T2>				force_inline float3x3	operator*(const scale_s<T1> &a, const rotate_in_axis<A,T2> &b)	{ return a * (float3x3)b; }
template<typename E, int A, typename T1, typename T2>	force_inline float3x3	operator*(const rotate_in_axis<A,T2> &a, const scale_s<T1> &b)	{ return (float3x3)a * b; }

template<typename E, typename T> force_inline vec<E,2>	operator*(const scale_s<T> &a, param_t<vec<E,2>> b)		{ return vec<E,2>(a.t) * b; }
template<typename E, typename T> force_inline position2	operator*(const scale_s<T> &a, param_t<vec<E,2>> b)		{ return vec<E,2>(a.t) * b; }
template<typename E, typename T> force_inline vec<E,3>	operator*(const scale_s<T> &a, param_t<vec<E,3>> b)		{ return vec<E,3>(a.t) * b; }
template<typename E, typename T> force_inline position3	operator*(const scale_s<T> &a, param_t<position3> b)	{ return vec<E,3>(a.t) * b; }
template<typename E, typename T> force_inline vec<E,4>	operator*(const scale_s<T> &a, param_t<vec<E,4>> b)		{ return vec<E,4>(vec<E,3>(a.t), one) * b; }
template<typename E, typename T> force_inline mat<E,2,2>	operator*(const scale_s<T> &a, const mat<E,2,2> &b)		{ return mat<E,2,2>(b.v * perm<0,1,0,1>(vec<E,2>(a.t))); }
template<typename E, typename T> force_inline mat<E,2,2>	operator*(const mat<E,2,2> &a, const scale_s<T> &b)		{ return mat<E,2,2>(a.v * perm<0,0,1,1>(vec<E,2>(b.t))); }
template<typename E, typename T> force_inline mat<E,2,3>	operator*(const scale_s<T> &a, const mat<E,2,3> &b)		{ return mat<E,2,3>(a * (const mat<E,2,2>&)b, vec<E,2>(a.t) * b.z); }
template<typename E, typename T> force_inline mat<E,2,3>	operator*(const mat<E,2,3> &a, const scale_s<T> &b)		{ return mat<E,2,3>((const mat<E,2,2>&)a * b, a.z); }
template<typename E, typename T> force_inline mat<E,3,3>	operator*(const scale_s<T> &a, const mat<E,3,3> &b)		{ vec<E,3> s(a.t); return mat<E,3,3>(b.x * s, b.y * s, b.z * s); }
template<typename E, typename T> force_inline mat<E,3,3>	operator*(const mat<E,3,3> &a, const scale_s<T> &b)		{ vec<E,3> s(b.t); return mat<E,3,3>(a.x * s.x, a.y * s.y, a.z * s.z);}
template<typename E, typename T> force_inline mat<E,3,4>	operator*(const scale_s<T> &a, const mat<E,3,4> &b)		{ return mat<E,3,4>(a * (const mat<E,3,3>&)b, vec<E,3>(a.t) * b.w); }
template<typename E, typename T> force_inline mat<E,3,4>	operator*(const mat<E,3,4> &a, const scale_s<T> &b)		{ return mat<E,3,4>((const mat<E,3,3>&)a * b, a.w); }
template<typename E, typename T> force_inline mat<E,4,4>	operator*(const scale_s<T> &a, const mat<E,4,4> &b)		{ vec<E,4> s(vec<E,3>(a.t),one); return mat<E,4,4>(b.x * s, b.y * s, b.z * s, b.w * s); }
template<typename E, typename T> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const scale_s<T> &b)		{ vec<E,3> s(b.t); return mat<E,4,4>(a.x * s.x, a.y * s.y, a.z * s.z, a.w); }
template<typename E, typename T> force_inline mat<E,3,3>	operator*(const scale_s<T> &a, quat<E> b)				{ return a * b.operator mat<E,3,3>(); }
template<typename E, typename T> force_inline mat<E,3,3>	operator*(quat<E> a, const scale_s<T> &b)				{ return a.operator mat<E,3,3>() * b; }

//-----------------------------------------------------------------------------
// translations
//-----------------------------------------------------------------------------

template<typename T> struct translate_s {
	const T	t;
	force_inline	translate_s(const T &t) : t(t)	{}
	force_inline	operator float2x3() const { return float2x3(identity, t); }
	force_inline	operator float3x4() const { return float3x4(identity, t); }
	friend translate_s	get_inverse(const translate_s &m) { return translate_s(-m.t); }
	friend auto			cofactors(const translate_s &m)		{ return transpose(inverse(m)); }
};

template<typename T> struct mat_traits<translate_s<T> > { enum {is_mat = true}; };

template<typename A>							force_inline translate_s<A>			translate(const A &a)							{ return translate_s<A>(a); }
template<typename A, typename B>				force_inline translate_s<position2>	translate(const A &a, const B &b)				{ return translate(position2(a, b)); }
template<typename A, typename B, typename C>	force_inline translate_s<position3>	translate(const A &a, const B &b, const C &c)	{ return translate(position3(a, b, c)); }

template<typename T1,typename B>	struct mul_t<translate_s<T1>, B, IF_MAT(B,void)>		: mul_t<float3x4, B>	{};
template<typename B>				struct mul_t<translate_s<position2>, B, IF_MAT(B,void)>	: mul_t<float2x3, B>	{};
template<typename T1, typename T2>	struct mul_t<translate_s<T1>, translate_s<T2> >									{ typedef translate_s<T1>	type; };

template<typename A, typename B> force_inline auto	operator*(const transpose_s<translate_s<A>&> &a, const B &b)	{ return transpose(transpose(b) * a.m); }

template<typename T1, typename T2>	force_inline translate_s<T1> operator*(const translate_s<T1> &a, const translate_s<T2> &b)	{ return translate<T1>(a.t + b.t); }
template<typename E, typename T> force_inline pos<E,2>	operator*(const translate_s<T> &a, param_t<position2> b)	{ return b + a.t; }
template<typename E, typename T> force_inline pos<E,3>	operator*(const translate_s<T> &a, param_t<position3> b)	{ return b + a.t; }
template<typename E, typename T> force_inline vec<E,4>	operator*(const translate_s<T> &a, param_t<vec<E,4>> b)		{ return b + (a.t * b.w, zero); }
template<typename E, typename T> force_inline mat<E,2,3>	operator*(const translate_s<T> &a, const mat<E,2,2> &b)		{ return mat<E,2,3>(b, a.t); }
template<typename E, typename T> force_inline mat<E,2,3>	operator*(const mat<E,2,2> &a, const translate_s<T> &b)		{ return mat<E,2,3>(a, vmul(a, b.t)); }
template<typename E, typename T> force_inline mat<E,2,3>	operator*(const translate_s<T> &a, const mat<E,2,3> &b)		{ return mat<E,2,3>((const mat<E,2,2>&)b, b.z + a.t); }
template<typename E, typename T> force_inline mat<E,2,3>	operator*(const mat<E,2,3> &a, const translate_s<T> &b)		{ return mat<E,2,3>((const mat<E,2,2>&)a, a.z + vmul(a, b.t)); }
template<typename E, typename T> force_inline mat<E,3,4>	operator*(const translate_s<T> &a, const mat<E,3,3> &b)		{ return mat<E,3,4>(b, position3(a.t)); }
template<typename E, typename T> force_inline mat<E,3,4>	operator*(const mat<E,3,3> &a, const translate_s<T> &b)		{ return mat<E,3,4>(a, vmul(a, b.t)); }
template<typename E, typename T> force_inline mat<E,3,4>	operator*(const translate_s<T> &a, const mat<E,3,4> &b)		{ return mat<E,3,4>((const mat<E,3,3>&)b, b.w + a.t); }
template<typename E, typename T> force_inline mat<E,3,4>	operator*(const mat<E,3,4> &a, const translate_s<T> &b)		{ return mat<E,3,4>((const mat<E,3,3>&)a, a.w + vmul(a, b.t)); }
template<typename E, typename T> force_inline mat<E,4,4>	operator*(const translate_s<T> &a, const mat<E,4,4> &b)		{ vec<E,4> t(a.t, zero); return mat<E,4,4>(b.x + t * b.x.w, b.y + t * b.y.w, b.z + t * b.z.w, b.w + t * b.w.w); }
template<typename E, typename T> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const translate_s<T> &b)		{ position3	t(b.t); return mat<E,4,4>(a.x, a.y, a.z, a.x * t.x + a.y * t.y + a.z * t.z + a.w); }
template<typename E, typename T> force_inline mat<E,3,4>	operator*(const translate_s<T> &a, quat<E> b)				{ return a * b.operator mat<E,3,3>(); }
template<typename E, typename T> force_inline mat<E,3,4>	operator*(quat<E> a, const translate_s<T> &b)				{ return a.operator mat<E,3,3>() * b; }
template<typename E, typename T> force_inline mat<E,4,4>	operator*(const symmetrical<E,4> &a, const translate_s<T> &b)	{ return mat<E,4,4>(a) * b; }
template<typename E, typename T> force_inline mat<E,4,4>	operator*(const translate_s<T> &b, const symmetrical<E,4> &a)	{ return b * mat<E,4,4>(a); }
template<typename T1, int A, typename T2>	force_inline float3x4 operator*(const translate_s<T1> &a, const rotate_in_axis<A,T2> &b)	{ return a * (float3x4)b; }
template<typename T1, int A, typename T2>	force_inline float3x4 operator*(const rotate_in_axis<A,T2> &a, const translate_s<T1> &b)	{ return (float3x4)a * b; }

template<typename T, typename S> force_inline typename T_if<vget<T>::dims==2, float2x3, float3x4>::type operator*(const translate_s<T> &t, const scale_s<S> &s) {
	return t * s.operator typename T_if<vget<T>::dims==2, float2x2, float3x3>::type();
}
template<typename T, typename S> force_inline typename T_if<vget<T>::dims==2, float2x3, float3x4>::type operator*(const scale_s<S> &s, const translate_s<T> &t) {
	return s.operator typename T_if<vget<T>::dims==2, float2x2, float3x3>::type() * t;
}

//-----------------------------------------------------------------------------
// rot_trans, scale_rot_trans
//-----------------------------------------------------------------------------

struct rot_trans : aligner<16> {
	quaternion	rot;
	float4		trans4;

	rot_trans() {}
	rot_trans(const _one&) : rot(identity), trans4(zero) {}
	rot_trans(param(quaternion) rot, param(float4) trans4) : rot(rot), trans4(trans4) {}
	rot_trans(param_t<float3x4> m) : rot(m), trans4(m.translation()) {}

	void reset() {
		rot		= identity;
		trans4	= zero;
	}

	position3	get_trans() const { return trans4.xyz; }
	quaternion	get_rot()	const { return rot; }
	operator	float3x4()	const { return translate(trans4.xyz) * rot; }

	void		set_trans(position3 t)  { trans4.xyz = t; }
	void		set_rot(quaternion q)	{ rot = q; }

	rot_trans			operator*(const float3x4 &m)						const	{ return operator float3x4() * m; }
	rot_trans&			operator=(param_t<float3x4> m)								{ trans4.xyz = m.translation(); rot = m; return *this; }

	rot_trans&			operator*=(param_t<quaternion> q)							{ rot = normalise(q * rot); return *this; }
	rot_trans			operator*(param_t<quaternion> q)					const	{ return {normalise(q * rot), trans4}; }

	template<typename X>rot_trans&	operator*=(translate_s<X> t)					{ trans4.xyz += t.t; return *this; }
	template<typename X>rot_trans	operator*(translate_s<X> t)				const	{ return {rot, trans4 + t.t}; }

	friend rot_trans 	operator*(param_t<translate_s<float3>> t, const rot_trans &x)	{ return {x.rot, x * position3(t.t)}; }
	friend rot_trans	operator*(param_t<quaternion> q, const rot_trans &x)		{ return {normalise(q * x.rot), q * x.get_trans()}; }
	friend rot_trans	operator*(const float3x4 &m, const rot_trans &x)			{ return m * x.operator float3x4(); }

	friend float3		operator*(const rot_trans &x, param_t<float3> p)	{ return x.rot * p; }
	friend position3	operator*(const rot_trans &x, param_t<position3> p)	{ return x.rot * (float3)p + x.trans4.xyz; }

	friend rot_trans	get_inverse(const rot_trans &x)	{
		auto rot = ~x.rot;
		return {rot, float4(rot * -x.trans4.xyz)};
	}
	friend bool			approx_equal(const rot_trans &a, const rot_trans &b, float tol = ISO_TOLERANCE) {
		return approx_equal((float3)a.trans4.xyz, (float3)b.trans4.xyz, tol) && approx_equal(abs(cosang(a.rot, b.rot)), 1, tol);
	}
	friend rot_trans	rotate_around_to(rot_trans &rt, param(position3) centre, param(quaternion) rot) {
		return rot_trans(rot, rt * centre - rot * float3(centre));
	}
	friend rot_trans	rotate_around(rot_trans &rt, param(position3) c, param(quaternion) r) {
		return rotate_around_to(rt, c, normalise(r * rt.rot));
	}
	friend rot_trans	lerp(rot_trans &a, const rot_trans &b, float rate) {
		return rot_trans(slerp(a.rot, b.rot, rate),  iso::lerp(a.trans4, b.trans4, rate));
	}
};

template<>	struct mat_traits<rot_trans>	{ enum {is_mat = true}; };

// note, not invertable
struct scale_rot_trans : rot_trans {
	float3 	scale;

	scale_rot_trans() {}
	scale_rot_trans(const _one&) : rot_trans(identity), scale(one) {}
	scale_rot_trans(param(float3) scale, param(quaternion) rot, param(float4) trans4) : rot_trans(rot, trans4), scale(scale) {}
	scale_rot_trans(param_t<float3x4> m) : rot_trans(m), scale(m.scale()) {}

	void reset() {
		rot_trans::reset();
		scale	= float3(one);
	}

	operator	float3x4() const { return rot_trans::operator float3x4() * iso::scale(scale); }
	auto&		operator=(param_t<float3x4> m) {
		scale	= m.scale();
		if (m.det() < zero)
			scale = -scale;
		rot_trans::operator=(m / iso::scale(one / scale));
		return *this;
	}

	template<typename T> auto&	operator*=(const translate_s<T> &t)			{ rot_trans::operator*=(t); return *this; }
	auto&						operator*=(param_t<quaternion> q)			{ rot_trans::operator*=(q); return *this; }

	template<typename T> auto	operator*(const scale_s<T> &t)		const	{ return scale_rot_trans(scale * t.t, rot, trans4); }
	template<typename T> auto	operator*(const translate_s<T> &t)	const	{ return scale_rot_trans(scale, rot, trans4 + t.t); }
	auto						operator*(param_t<quaternion> q)	const	{ return scale_rot_trans(scale, normalise(q * rot), trans4); }
	template<typename T> friend auto operator*(const scale_s<T> &t, const scale_rot_trans &x)		{ return scale_rot_trans(x.scale * t.t, x.rot, t.t * x.trans4.xyz); }
	template<typename T> friend auto operator*(const translate_s<T> &t, const scale_rot_trans &x)	{ return scale_rot_trans(x.scale, x.rot, x * position3(t.t)); }
	friend auto					operator*(param_t<quaternion> q, const scale_rot_trans &x)			{ return scale_rot_trans(x.scale, normalise(q * x.rot), q * x.get_trans()); }

	friend float3		operator*(const scale_rot_trans &x, param_t<float3> p)		{ return x.rot * (x.scale * p); }
	friend position3	operator*(const scale_rot_trans &x, param_t<position3> p)	{ return x * (float3)p + x.trans4.xyz; }

	friend bool		approx_equal(const scale_rot_trans &a, const scale_rot_trans &b, float tol = ISO_TOLERANCE) {
		return approx_equal((const rot_trans&)a, (const rot_trans&)b) && approx_equal(a.scale, b.scale, tol);
	}
};


//-----------------------------------------------------------------------------
// dual_quaternion
//-----------------------------------------------------------------------------

struct dual_quaternion {
	quaternion	r, t;

	static inline quaternion mul(param_t<float3> v, param_t<quaternion> q) {
		return quaternion(v * q.w - cross(v, q.xyz), -dot(v, q.xyz));
	}

	dual_quaternion() {}
	dual_quaternion(param_t<quaternion> r, param_t<quaternion> t) : r(r), t(t)							{}
	dual_quaternion(param_t<float3> v)							: r(identity), t(float4(v * half, zero))	{}
	dual_quaternion(param_t<quaternion> q)						: r(q), t(identity)							{}
	dual_quaternion(param_t<float3> v, param_t<quaternion> q)	: r(q), t(mul(v * half, q))					{}
	dual_quaternion(param_t<float3x4> t)						: r(t), t(mul(t.translation() * half, r))	{}

	inline position3	translation()	const	{ return (t * ~r).xyz * two; }
	inline operator		float3x4()		const	{ return translate(translation()) * r; }
};

force_inline dual_quaternion	normalise(param_t<dual_quaternion> d)	{
	float1		rn	= norm(d.r);
	quaternion	r	= d.r * rn;
	quaternion	t	= d.t * rn;
	return dual_quaternion(r, t - r * cosang(r, t));
}

force_inline dual_quaternion	operator~(param_t<dual_quaternion> d)	{
	return dual_quaternion(~d.r, ~d.t);
}

force_inline dual_quaternion	operator*(param_t<dual_quaternion> a, param_t<dual_quaternion> b) {
	return dual_quaternion(a.r * b.r, a.r * b.t + a.t * b.r);
}

force_inline float3 operator*(param_t<dual_quaternion> d, param_t<float3> v) {
	return d.r * v;
}

force_inline position3 operator*(param_t<dual_quaternion> d, param_t<position3> v) {
#if 0
	float1		rn	= norm(d.r);
	quaternion	q	= d.r * rn;
	float3		t	= (d.t * ~q).xyz * rn * two;
	return q * float3(v) + t;
#else
	return d.r * float3(v) + (d.t * ~d.r).xyz * two;
#endif
}

//-----------------------------------------------------------------------------
//	Matrix creators
//-----------------------------------------------------------------------------

float2x3	fov_matrix(param_t<float4> fov);

float4x4	perspective_projection(float sx, float sy, float nearz, float farz);
float4x4	perspective_projection(float sx, float sy, float nearz);
float4x4	perspective_projection_rect(float left, float right, float bottom, float top, float nearz, float farz);
float4x4	perspective_projection_rect(float left, float right, float bottom, float top, float nearz);
float4x4	perspective_projection_fov(float left, float right, float bottom, float top, float nearz, float farz);
float4x4	perspective_projection_fov(float left, float right, float bottom, float top, float nearz);
float4x4	perspective_projection_fov(param_t<float4> fov, float nearz, float farz);
float4x4	perspective_projection_fov(param_t<float4> fov, float nearz);
float4x4	perspective_projection_angle(float theta, float aspect, float nearz, float farz);
float4x4	perspective_projection_angle(float theta, float aspect, float nearz);
float4x4	set_perspective_z(param_t<float4x4> proj, float new_nearz, float new_farz);

float4x4	parallel_projection(float sx, float sy, float nearz, float farz);
float4x4	parallel_projection_rect(param_t<float3> xyz0, param_t<float3> xyz1);
float4x4	parallel_projection_rect(param_t<float2> xy0, param_t<float2> xy1, float z0, float z1);
float4x4	parallel_projection_rect(float x0, float x1, float y0, float y1, float z0, float z1);
float4x4	parallel_projection_fov(param_t<float4> fov, float scale, float z0, float z1);

float3x4	stereo_skew(float separation, float focus, float shift = 0);
float3x3	look_along_x(param_t<float3> dir);
float3x3	look_along_y(param_t<float3> dir);
float3x3	look_along_z(param_t<float3> dir);
float3x4	look_at(const float3x4 &at, param_t<float3> pos);
float4x4	find_projection(float4 in[5], float4 out[5]);
float3x3	find_projection(float3 in[4], float3 out[4]);

//-----------------------------------------------------------------------------
//	Matrix dcomposition
//-----------------------------------------------------------------------------

void QR(const float2x2 &A, float2x2 &Q, float2x2 &R);
void QR(const float3x3 &A, float3x3 &Q, float3x3 &R);
void QR(const float4x4 &A, float4x4 &Q, float4x4 &R);

template<typename M> void LQ(const M &A, M &L, M &Q) {
	QR(transpose(A), Q, L);
	Q = transpose(Q);
	L = transpose(L);
}

#ifdef __MWERKS__
//-----------------------------------------------------------------------------
// Template Specializations for Wii
//-----------------------------------------------------------------------------
force_inline float	len2(const _v3<float,0, 0, 1, 2> &x)	{ return wii::dot((float3*)&x, (float3*)&x); }
force_inline float	len2(param_t<float3> x)					{ return wii::dot(&x, &x); }
force_inline float	len(const _v3<float,0, 0, 1, 2> &x)		{ return sqrt(wii::dot((float3*)&x, (float3*)&x)); }
force_inline float	len(param_t<float3> x)					{ return sqrt(wii::dot(&x, &x)); }
force_inline float	rlen(const _v3<float,0, 0, 1, 2> &x)	{ return rsqrt(wii::dot((float3*)&x, (float3*)&x)); }
force_inline float	rlen(param_t<float3> x)					{ return rsqrt(wii::dot(&x, &x)); }
force_inline float	dot(param_t<float3> a, param_t<float3> b)	{ return wii::dot(&a, &b); }
force_inline float	dot(param_t<float4> a, param_t<float4> b)	{ return wii::dot(&a, &b); }
#endif

}//namespace iso

#undef VLTYPE
#undef VRTYPE
#undef VBTYPE
#undef VLTYPE1
#undef VLTYPE2

#endif	// VECTOR_H
