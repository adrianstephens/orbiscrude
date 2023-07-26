#ifndef VECTOR_H
#define VECTOR_H

#include "simd.h"
#include "swizzle.h"

namespace iso {

	// need to explicitly add functions that are also in iso
	using simd::to;

	using simd::min;
	using simd::max;
	using simd::abs;
	using simd::copysign;
	using simd::any;
	using simd::all;
	using simd::select;

	using simd::concat;
	using simd::swizzle;
	using simd::rotate;

	using simd::sign1;
	using simd::sign;

	using simd::trunc;
	using simd::ceil;
	using simd::floor;
	using simd::round;
	using simd::frac;

	using simd::sqrt;
	using simd::reciprocal;
	using simd::rsqrt;

	using simd::sin;
	using simd::cos;
	using simd::tan;
	using simd::asin;
	using simd::acos;
	using simd::atan;
	using simd::atan2;
	using simd::sincos;

	using simd::log2;
	using simd::exp2;
	using simd::ln;
	using simd::exp;
	using simd::pow;
	
	using simd::min_component_index;
	using simd::max_component_index;

	using simd::load;
	using simd::store;
}

#include "soft_float.h"
#include "maths.h"
#include "interval.h"

namespace iso {
using namespace simd;

template<typename V> struct container_iterator<V, enable_if_t<is_vec<V>>> : T_type<element_type<V>*> {};
template<typename T, int N> static constexpr bool has_N = num_elements_v<T> == N;

template<typename E, int N, int M>	class mat;

//-----------------------------------------------------------------------------
//	generic soft perm
//-----------------------------------------------------------------------------

template<typename T, typename P, int...I> struct soft_perm {
	static const int	N = sizeof...(I);
	typedef simd::vec<P, N>	unp;

	soft_perm(const soft_perm&) = delete;

	template<typename E, int...J> void set(simd::vec<E, N> b, meta::value_list<int, J...>) {
		discard{(swizzle<I>(*(T*)this) = b[J], false)...};
	}

	operator unp() const { return to<P>(*this); }
	template<typename E> force_inline soft_perm&	operator=(vec<E, N> b)	{ set(b, meta::make_value_sequence<N>()); return *this; }

	template<typename B> force_inline auto	operator+ (B b)	const	{ return (unp)*this +  b; }
	template<typename B> force_inline auto	operator- (B b)	const	{ return (unp)*this -  b; }
	template<typename B> force_inline auto	operator* (B b)	const	{ return (unp)*this *  b; }
	template<typename B> force_inline auto	operator/ (B b)	const	{ return (unp)*this /  b; }

	template<typename B> force_inline auto	operator==(B b)	const	{ return (unp)*this == b; }
	template<typename B> force_inline auto	operator< (B b)	const	{ return (unp)*this <  b; }
	template<typename B> force_inline auto	operator> (B b)	const	{ return (unp)*this >  b; }
	template<typename B> force_inline auto	operator!=(B b)	const	{ return (unp)*this != b; }
	template<typename B> force_inline auto	operator<=(B b)	const	{ return (unp)*this <= b; }
	template<typename B> force_inline auto	operator>=(B b)	const	{ return (unp)*this >= b; }

	friend unp as_vec(const soft_perm &a)	{ return a; }
	template<typename E> friend force_inline vec<E,N> to(const soft_perm &p) { return { E(get(swizzle<I>((const T&)p)))...}; }
};

template<int...J, typename T, typename P, int... I> force_inline auto& swizzle(const soft_perm<T,P,I...> &p) {
	return swizzle((const T&)p, meta::perm<meta::value_list<int, I...>, meta::value_list<int, J...>>());
}
template<int...J, typename T, typename P, int... I> force_inline auto& swizzle(soft_perm<T,P,I...> &p) {
	return swizzle((T&)p, meta::perm<meta::value_list<int, I...>, meta::value_list<int, J...>>());
}

template<typename B, typename T, typename P, int...I>	struct perm_maker2	{ using type = soft_perm<T, P, I...>; };
template<typename B, typename T, typename P>			struct perm_maker	{ template<int...I> using type = typename perm_maker2<B, T, P, I...>::type; };

//-----------------------------------------------------------------------------
//	generic soft vec
//-----------------------------------------------------------------------------

template<typename B, int N, typename V, typename P> struct soft_vec : B, swizzles<N, V, perm_maker<B, soft_vec<B, N, V, P>, P>::template type> {
	typedef simd::vec<P, N>	unp;

	template<typename T, int...I> void _set(meta::value_list<int, I...>, const T &b) {
		discard{(swizzle<I>(*this) = get(swizzle<I>(b)), false)...};
	}
	template<typename...T, int...I> void _set(meta::value_list<int, I...>, T...p) {
		discard{(swizzle<I>(*this) = p, false)...};
	}
	template<typename E, int...I> simd::vec<E, N> _get(meta::value_list<int, I...>) const {
		return {E(get(swizzle<I>(*this)))...};
	}

	soft_vec()										{}
	soft_vec(const soft_vec&)		= default;//				{ memcpy(this, &b, sizeof(*this)); }
	soft_vec(unp b)									{ _set(meta::make_value_sequence<N>(), b); }
	template<typename K> soft_vec(constant<K> k)	: soft_vec(unp(k)) {}
	template<typename T0, typename T1, typename...T> soft_vec(T0 p0, T1 p1, T...p)	{ _set(meta::make_value_sequence<N>(), p0, p1, p...); }

	force_inline soft_vec&	operator=(unp b)				{ _set(meta::make_value_sequence<N>(), b); return *this; }
	force_inline soft_vec&	operator=(const soft_vec &b)	{ this->v = b.v; return *this; }
	template<typename K> force_inline soft_vec&								operator=(const constant<K> &b)		{ return operator=(unp(b)); }
	template<typename T> force_inline enable_if_t<num_elements_v<T> == N, soft_vec&> 	operator=(const T &b)	{ _set(meta::make_value_sequence<N>(), as_vec(b)); return *this; }

	operator unp()								const { return _get<P>(meta::make_value_sequence<N>()); }
//	template<typename E> operator vec<E, N>()	const { return _get<E>(meta::make_value_sequence<N>()); }

	force_inline auto	operator-()	const	{ return -(unp)*this; }

	template<typename T> force_inline auto		operator==(T b)	const	{ return (unp)*this == b; }
	template<typename T> force_inline auto		operator< (T b)	const	{ return (unp)*this <  b; }
	template<typename T> force_inline auto		operator> (T b)	const	{ return (unp)*this >  b; }
	template<typename T> force_inline auto		operator!=(T b)	const	{ return (unp)*this != b; }
	template<typename T> force_inline auto		operator<=(T b)	const	{ return (unp)*this <= b; }
	template<typename T> force_inline auto		operator>=(T b)	const	{ return (unp)*this >= b; }

	template<typename T> force_inline auto		operator+ (T b)	const	{ return (unp)*this +  get(b); }
	template<typename T> force_inline auto		operator- (T b)	const	{ return (unp)*this -  get(b); }
	template<typename T> force_inline auto		operator* (T b)	const	{ return (unp)*this *  get(b); }
	template<typename T> force_inline auto		operator/ (T b)	const	{ return (unp)*this /  get(b); }

	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto	operator& (T b)		const { return (unp)*this & b; }
	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto	operator| (T b)		const { return (unp)*this | b; }
	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto	operator^ (T b)		const { return (unp)*this ^ b; }
	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto	operator% (T b)		const { return (unp)*this % b; }
	template<typename P1 = P, typename=enable_if_t<is_int<P1>>>				force_inline auto	operator<<(int b)	const { return (unp)*this << b; }
	template<typename P1 = P, typename=enable_if_t<is_int<P1>>>				force_inline auto	operator>>(int b)	const { return (unp)*this >> b; }

	template<typename T> force_inline soft_vec&	operator+= (T b)		{ return *this = *this + b; }
	template<typename T> force_inline soft_vec&	operator-= (T b)		{ return *this = *this - b; }
	template<typename T> force_inline soft_vec&	operator*= (T b)		{ return *this = *this * b; }
	template<typename T> force_inline soft_vec&	operator/= (T b)		{ return *this = *this / b; }

	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto&	operator&= (T b)	{ return *this = *this & b; }
	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto&	operator|= (T b)	{ return *this = *this | b; }
	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto&	operator^= (T b)	{ return *this = *this ^ b; }
	template<typename T, typename P1 = P, typename=enable_if_t<is_int<P1>>> force_inline auto&	operator%= (T b)	{ return *this = *this % b; }
	template<typename P1 = P, typename=enable_if_t<is_int<P1>>>				force_inline auto&	operator<<=(int b)	{ return *this = *this << b; }
	template<typename P1 = P, typename=enable_if_t<is_int<P1>>>				force_inline auto&	operator>>=(int b)	{ return *this = *this >> b; }

	force_inline soft_vec&	operator++()	{ return *this += P(1); }
	force_inline soft_vec&	operator--()	{ return *this -= P(1); }
	force_inline soft_vec	operator++(int)	{ auto t = *this; operator++(); return t; }
	force_inline soft_vec	operator--(int)	{ auto t = *this; operator--(); return t; }

	friend unp as_vec(const soft_vec &a)	{ return a; }

	template<typename E> friend force_inline vec<E,N> to(const soft_vec &p) { return p._get<E>(meta::make_value_sequence<N>()); }
};

template<typename B, int N, typename V, typename P> constexpr bool use_constants<soft_vec<B, N, V, P>> = false;

template<typename B, int N, typename V, typename P> constexpr int num_elements_v<soft_vec<B, N, V, P>> = N;
template<typename B, int N, typename V, typename P, int A> constexpr int num_elements_v<aligned<soft_vec<B, N, V, P>, A>> = N;
//template<typename K, typename B, int N, typename V, typename P> struct constant_cast<K, soft_vec<B, N, V, P> >	{ static const soft_vec<B, N, V, P> f() { return K::template as<P>(); } };

//-----------------------------------------------------------------------------
//	array_vec
//-----------------------------------------------------------------------------

template<typename X, int N> struct array_vec_base {
	auto&	operator[](int i)			{ return ((X*)this)[i]; }
	auto&	operator[](int i)	const	{ return ((X*)this)[i]; }
//	auto	begin()				const	{ return (X*)this; }
//	auto	end()				const	{ return (X*)this + N; }
};
template<typename X, int N> using array_vec = soft_vec<array_vec_base<X, N>, N, space_for<X[N]>, promoted_type<X>>;
template<typename V, typename P, int I, typename X, int N> struct perm_maker2<array_vec_base<X, N>, V, P, I> { using type = offset_type_t<X, I * sizeof(X)>; };

template<typename X, int N> struct T_swap_endian_type<array_vec<X, N> >	{ typedef array_vec<typename T_swap_endian_type<X>::type, N> type; };
//template<typename X, int N> struct load_s<array_vec<X, N>> : load_memcpy_s<array_vec<X, N>, X, N> {};

template<typename E, int N, typename I>	SIMD_FUNC	void	load(array_vec<E,N> &v, I p)		{ memcpy(&v, p, sizeof(v)); }
template<typename E, int N, typename I>	inline		void	store(const array_vec<E,N> &v, I p)	{ memcpy(p, &v, sizeof(v)); }

//-----------------------------------------------------------------------------
//	field_vec
//-----------------------------------------------------------------------------

template<typename...X> struct field_vec_base {};
template<typename...X> using field_vec = soft_vec<field_vec_base<X...>, sizeof...(X), tuple_offset_t<sizeof...(X) - 1, X...>, promoted_type<X...>>;
template<typename V, typename P, int I, typename...X> struct perm_maker2<field_vec_base<X...>, V, P, I> { using type = tuple_offset_t<I, X...>; };

template<typename...X> struct T_swap_endian_type<field_vec<X...> >	{ typedef field_vec<typename T_swap_endian_type<X>::type...> type; };

//-----------------------------------------------------------------------------
//	bitfield_vec
//-----------------------------------------------------------------------------

template<typename...X> struct bitfield_vec_base {};
template<typename...X> using bitfield_vec = soft_vec<bitfield_vec_base<X...>, sizeof...(X), bitfields<X...>, promoted_type<X...>>;

template<typename V, typename P, int I, typename... X> struct perm_maker2<bitfield_vec_base<X...>, V, P, I> {
	using T = meta::VT_index_t<I, X...>;
	using type = bitfield<T, BIT_COUNT<meta::VT_left_t<I, X...>>, BIT_COUNT<T>>;
};

//-----------------------------------------------------------------------------
//	simple_vec/soft_mat
//-----------------------------------------------------------------------------

template<typename T, int N> struct simple_vec_base;

template<typename T> struct simple_vec_base<T, 2> : array_vec_base<T, 2> { T x, y; };
template<typename T> struct simple_vec_base<T, 3> : array_vec_base<T, 3> { T x, y, z; };
template<typename T> struct simple_vec_base<T, 4> : array_vec_base<T, 4> { T x, y, z, w; };

template<typename T, int N> struct simple_vec : simple_vec_base<T, N> {
	template<typename B, int...I> void _set(meta::value_list<int, I...>, const B &b) {
		discard{((&this->x)[I] = b[I], false)...};
	}
	template<typename...P, int...I> void _set(meta::value_list<int, I...>, P...p) {
		discard{((&this->x)[I] = p, false)...};
	}
	template<typename E, int...I> E _get(meta::value_list<int, I...>) const {
		return {(&this->x)[I]...};
	}

	simple_vec()	{}
	template<typename U = T, typename = enable_if_t<can_index<U>>>  operator mat<element_type<T>, num_elements_v<T>, N>()	const { return _get<mat<element_type<T>, num_elements_v<T>, N>>(meta::make_value_sequence<N>()); }
	force_inline simple_vec& 	operator=(const mat<element_type<T>, num_elements_v<T>, N> &b)	{ _set(meta::make_value_sequence<N>(), b); return *this; }
};

template<typename T, int N, int M>	using soft_mat		= simple_vec<array_vec<T, N>, M>;

#if 0//def __clang__
//template<typename X, int N> using packed_vec = if_t<(is_vec_element<X> && (N & (N - 1)) == 0), simd::packed_simd<X, N>, array_vec<X, N>>;
template<typename X, int N, bool PACKED=is_vec_element<X> && (N & (N - 1)) == 0> struct get_packed_type2 : T_type<simd::packed_simd<X, N>> {};
template<typename X, int N> struct get_packed_type2<X, N, false> : T_type<array_vec<X, N>> {};
template<typename X, int N> using packed_vec = typename get_packed_type2<X, N>::type;

#else
template<typename X, int N> using packed_vec = array_vec<X, N>;
#endif

//template<typename E, int N>		constexpr bool use_constants<array_vec<E,N>>		= true;


//-----------------------------------------------------------------------------
//	common vectors
//-----------------------------------------------------------------------------

#if defined __clang__ && !defined USE_FP16
typedef packed_vec<float16, 2>	hfloat2;
typedef packed_vec<float16, 4>	hfloat4;
#endif

typedef packed_vec<float16, 2>	hfloat2p;
typedef packed_vec<float16, 4>	hfloat4p;

typedef packed_vec<float, 2>	float2p;
typedef packed_vec<float, 3>	float3p;
typedef packed_vec<float, 4>	float4p;
typedef packed_vec<uint32, 2>	uint2p;
typedef packed_vec<uint32, 3>	uint3p;
typedef packed_vec<uint32, 4>	uint4p;

typedef	packed_vec<double,2>	double2p;
typedef	packed_vec<double,3>	double3p;
typedef	packed_vec<double,4>	double4p;

typedef soft_mat<float, 2, 2>	float2x2p;
typedef soft_mat<float, 2, 3>	float2x3p;
typedef soft_mat<float, 3, 2>	float3x2p;
typedef soft_mat<float, 3, 3>	float3x3p;
typedef soft_mat<float, 3, 4>	float3x4p;
typedef soft_mat<float, 4, 3>	float4x3p;
typedef soft_mat<float, 4, 4>	float4x4p;

typedef	packed_vec<int, 2>		point;
typedef	packed_vec<int, 3>		point3;
typedef	packed_vec<int, 4>		point4;
typedef interval<point>			rect;
typedef interval<point3>		vol;

//-----------------------------------------------------------------------------
//	macros & forward declarations
//-----------------------------------------------------------------------------

template<class R, typename E, int N> bool custom_read(R &r, vec<E, N> &v) {
	return check_readbuff(r, &v, sizeof(E) * N);
}
template<class W, typename E, int N> bool custom_write(W &w, const vec<E, N> &v) {
	return check_writebuff(w, &v, sizeof(E) * N);
}
template<typename T> static constexpr bool is_mat	= false;

#define IF_VEC(T)		enable_if_t<is_vec<T>, T>
#define IF_SCALAR(T,R)	enable_if_t<is_num<T>, R>
#define IF_MAT(M,R)		iso::enable_if_t< iso::is_mat<M>, R>
#define IF_NOTMAT(M,R)	iso::enable_if_t<!iso::is_mat<M>, R>

//-----------------------------------------------------------------------------
// normalised
//-----------------------------------------------------------------------------

template<typename T> class normalised {
	T	t;
public:
	constexpr normalised(const T &t, bool)	: t(t) {}
	constexpr normalised(const T &t);
	constexpr operator const T&()	const { return t; }
};

template<typename T> auto	normalise(const normalised<T> &r) { return r; }

template<typename T> constexpr int num_elements_v<normalised<T>>	= num_elements_v<T>;
namespace simd {
template<typename T> struct element_type_s<normalised<T>>		: element_type_s<T> {};
}

//-----------------------------------------------------------------------------
//	axis
//-----------------------------------------------------------------------------

template<int A>	struct axis_s {
	static const float4	v;
	force_inline	operator float2()	const	{ return v.xy; }
	force_inline	operator float3()	const	{ return v.xyz; }
	force_inline	operator float4()	const	{ return v; }
	force_inline	operator normalised<float2>()	const	{ return {v.xy, true}; }
	force_inline	operator normalised<float3>()	const	{ return {v.xyz, true}; }
	force_inline	operator normalised<float4>()	const	{ return {v, true}; }
};

template<int A> force_inline	negated<axis_s<A>>	operator-(const axis_s<A> &a)			{ return a; }

template<int A> const float4 axis_s<A>::v = {A == 0, A == 1, A == 2, A == 3};

extern axis_s<0> x_axis;
extern axis_s<1> y_axis;
extern axis_s<2> z_axis, xy_plane;
extern axis_s<3> w_axis;

//-----------------------------------------------------------------------------
//derived functions
//-----------------------------------------------------------------------------

template<typename T> auto plus_minus(T t)			{ return concat(t, -t); }
template<typename T> auto plus_minus(T t, bool neg)	{ return select(neg, -t, t); }
template<typename T> auto barycentric(T t)			{ return concat(t, one - reduce_add(t)); }

template<typename T, typename E, int N, int...I> T	from_barycentric(const T *p, vec<E, N> lambdas, meta::value_list<int, I...>) {
	return sum(p[I] * lambdas[I]...);
}
template<typename T, typename E, int N> T			from_barycentric(const T *p, vec<E, N> lambdas) {
	return from_barycentric(p, lambdas,  meta::make_value_sequence<N>());
}

template<int N> static force_inline auto	step(vec<float, N> edge, vec<float, N> x)		{ return select(x < edge, (vec<float, N>)one, zero); }
template<int N> static force_inline auto	step(vec<double, N> edge, vec<double, N> x)		{ return select(x < edge, (vec<double, N>)one, zero); }

#if 0
template<typename E, int N>			force_inline auto		dot(vec<E,N> x, vec<E,N> y)	{ return reduce_add(x * y); }
template<typename X, typename Y>	force_inline auto		dot(X x, Y y)	{ static const int N = min(num_elements_v<X>, num_elements_v<Y>); return dot<element_type<X>, N>(shrink<N>(x), shrink<N>(y)); }
#else
template<typename X, int NX, typename Y, int NY> struct dot_s		{ static auto f(vec<X,NX> x, vec<Y,NY> y)	{ typedef promoted_type<X,Y> E; return dot_s<E,NX,E,NY>::f(to<E>(x), to<E>(y)); } };
template<typename X, int NX, int NY>	struct dot_s<X, NX, X, NY>	{ static auto f(vec<X,NX> x, vec<X,NY> y)	{ return reduce_add(shrink<min(NX,NY)>(x) * shrink<min(NX,NY)>(y)); } };
template<typename X, int N>				struct dot_s<X, N, X, N>	{ static auto f(vec<X,N> x, vec<X,N> y)		{ return reduce_add(x * y); } };
template<typename X, typename Y>	force_inline auto		dot(X x, Y y)		{ return dot_s<element_type<X>, num_elements_v<X>, element_type<Y>, num_elements_v<Y>>::f(x, y); }
#endif

template<int A, typename T>			inline auto dot(const T &t, const axis_s<A>&)		{ return t[A]; }
template<typename X, typename Y>	inline auto dot(const X &x, const negated<Y> &y)	{ return -dot(x, y.t); }

template<typename X, typename Y>	force_inline IF_VEC(Y)	project(const X &x, const Y &y)			{ return y * (dot(x, y) / dot(y, y)); }
template<typename X, typename Y>	force_inline IF_VEC(Y)	project_unit(const X &x, const Y &y)	{ return y * dot(x, y); }
template<typename X, typename Y>	force_inline IF_VEC(Y)	reflect(const X &x, const Y &y)			{ return project(x, y) * 2 - x; }
template<typename X, typename Y>	force_inline IF_VEC(X)	orthogonalise(const X &x, const Y &y)	{ return x - project(x, y); }

template<typename X>				force_inline auto		len2(X x)								{ return dot(x, x); }
template<typename X>				force_inline auto		len(X x)								{ return iso::sqrt(len2(x)); }
template<typename X>				force_inline auto		rlen(X x)								{ return iso::rsqrt(len2(x)); }

template<typename X, typename Y>	force_inline auto		dist2(X x, Y y)							{ return len2(x - y); }
template<typename X, typename Y>	force_inline auto		dist(X x, Y y)->decltype(len(x - y))	{ return len(x - y); }

template<typename V>				force_inline auto		diff(V a)								{ return a.lo - a.hi; }
template<typename E>				force_inline vec<E,2>	perp(const vec<E,2> &a)					{ return {-a.y, a.x}; }
template<typename A, typename B>	force_inline auto		perp(const pair<A,B> &v)				{ return make_pair(-v.b, v.a); }

//template<typename X>				force_inline auto		normalize(X x)					{ return x * iso::rsqrt(len2(x)); }
template<typename X>				force_inline enable_if_t<is_vec<X>, vec_type<X>>	normalise(X x)		{ return x * iso::rsqrt(len2(x)); }
template<typename X>				force_inline enable_if_t<is_vec<X>, vec_type<X>>	safe_normalise(X x) { auto x0 = reduce_max(abs(x)); return x0 == zero ? x : normalise(x / x0); }

template<typename E>				force_inline vec<E, 3>	cross(vec<E,3> x, vec<E,3> y)	{ return (x.zxy * y - x * y.zxy).zxy; }
template<typename E, int A>			force_inline vec<E, 3>	cross(vec<E,3> x, axis_s<A>)	{ return rotate<-A>(vec<E, 3>{zero, x[(A+2)%3], -x[(A+1)%3]}); }
template<typename E>				force_inline vec<E,2>	cross(E x, vec<E,2> y)			{ return perp<E>(y) * x; }
template<typename E>				force_inline vec<E,2>	cross(vec<E,2> x, E y)			{ return perp<E>(x) * -y; }
template<typename E>				force_inline E			cross(vec<E,2> x, vec<E,2> y)	{ return diff(x * y.yx); }
template<typename E>				force_inline E			cross(vec<E,2> x, axis_s<0>)	{ return -x.y; }
template<typename E>				force_inline E			cross(vec<E,2> x, axis_s<1>)	{ return x.x; }
template<typename X>				force_inline auto		cross(X x, X y)					{ return cross<element_type<X>>(x, y); }
//template<typename X, int A>			force_inline auto		cross(X x, axis_s<A> y)			{ return cross<element_type<X>>(x, y); }
template<typename X, int A>			force_inline auto		cross(axis_s<A> y, X x)			{ return -cross<element_type<X>>(x, y); }
template<typename X, typename Y>	force_inline auto		cross(X x, negated<Y> y)		{ return cross(y.t, x); }
template<typename X, typename Y>	force_inline auto		cross(negated<X> x, Y y)		{ return cross(y, x.t); }

template<typename E>				force_inline auto		perp(const vec<E,3> &v)			{ return cross(rotate(vec<E,3>(x_axis), -min_component_index(abs(v))), v); }
template<typename X>				force_inline auto		perp(const X &v)				{ return perp<element_type<X>>(v); }

template<typename T> constexpr normalised<T>::normalised(const T &t)		: t(normalise(t)) {}

template<typename E>	inline vec<E, 2> to_octohedral(const vec<E, 3>& v) {
	auto	v1 = v / reduce_add(abs(v));
	return select(v.z < zero, sign1(v.xy) - v1.xy, v1.xy);
}

template<typename E>	inline vec<E, 3> from_octohedral(const vec<E, 2>& v) {
	auto	v1 = concat(v, reduce_add(abs(v)));
	return normalise(select(v1.z > one, sign1(v1) - v, v1));
}

template<typename E, int N> inline bool approx_equal(const vec<E,N> &a, const vec<E,N> &b, E tol = ISO_TOLERANCE) {
	return all(abs(a - b) <= max(max(abs(a), abs(b)), one) * tol);
}

template<typename A, typename B> inline enable_if_t<is_vec<A>, bool> approx_equal(const A &a, const B &b, element_type<A> tol = ISO_TOLERANCE) {
	return approx_equal<element_type<A>, num_elements_v<A>>(a, b, tol);
}

template<typename E, int N> inline bool find_scalar_multiple(const vec<E,N> &a, const vec<E,N> &b, float &scale) {
	vec<E,N>	d	= b / a;
	E			e	= d[max_component_index(a)];
	scale = e;
	return all(d == e | d != d);
}

// based on Duff - requires  v is normal
template<typename E> inline normalised<vec<E,3>> perp(const normalised<vec<E,3>> &vn) {
	vec<E,3>	v = vn;
	auto s = copysign(1.f, v.z);
	auto a = -v.y / (s + v.z);
	return {vec<E,3>{v.x * a, s + v.y * a, -v.y}, true};
}

//-----------------------------------------------------------------------------
//	sort
//-----------------------------------------------------------------------------

template<int N, int S, int D> struct bitonic_types {
	static const int M = (S > 0 ? N : pow2_ceil<N>) / 2;
	typedef bitonic_types<M, 		S - 1, D - 1>	A;
	typedef bitonic_types<N - M, 	S - 1, D - 1>	B;
	typedef decltype(concat(typename A::swiz(), typename B::swiz() + meta::int_constant<M>()))			swiz;
	typedef decltype(concat(typename A::flip() ^ meta::int_constant<(S > 0)>(), typename B::flip()))	flip;
};

template<int N, int S> struct bitonic_types<N, S, 0> {
	static const int M = pow2_ceil<N> / 2;
	static constexpr int swizfunc(int i)	{ return i < N - M ? i + M : i < M ? i : i - M; }
	static constexpr int flipfunc(int i)	{ return i >= M; }

	typedef typename meta::make_value_sequence<N>::template apply<swizfunc>	swiz;
	typedef typename meta::make_value_sequence<N>::template apply<flipfunc>	flip;
};

template<typename V, int...S, int...F> static force_inline V bitonic_merge(V v, meta::value_list<int, S...>, meta::value_list<int, F...>) {
	auto	v2 	= swizzle<S...>(v);
	return select((v < v2) ^ vec<sint_for_t<element_type<V>>, sizeof...(F)>{-F...}, v, v2);
}

template<int N, int S, int D, bool = ((1 << D) < N)> struct bitonic_merger {
	typedef bitonic_types<N, S, D>	B;
	template<typename E> static force_inline vec<E, N> merge(vec<E, N> v) {
		return bitonic_merger<N, S, D + 1>::template merge<E>(bitonic_merge(v, typename B::swiz(), typename B::flip()));
	}
};
template<int N, int S, int D> struct bitonic_merger<N, S, D, false> {
	template<typename E> static force_inline vec<E, N> merge(vec<E, N> v)	{ return v; }
};

template<int N, int S, bool = ((1 << S) < N)> struct bitonic_sorter {
	template<typename E> static force_inline vec<E, N> sort(vec<E, N> v) {
		return bitonic_merger<N, S, S>::template merge<E>(bitonic_sorter<N, S + 1>::template sort<E>(v));
	}
};
template<int N, int S> struct bitonic_sorter<N, S, false> {
	template<typename E> static force_inline vec<E, N> sort(vec<E, N> v)	{ return v; }
};

template<typename V> inline enable_if_t<is_vec<V>, vec<element_type<V>, num_elements_v<V>>> sort(V v) {
	return bitonic_sorter<num_elements_v<V>, 0>::template sort<element_type<V>>(v);
}

template<typename E> inline int32x2 rank_sort(vec<E,2> x) {
	int		is	= x.y > x.x;
	return {is, 1 - is};
}
template<typename E> inline int32x3 rank_sort(vec<E,3> x) {
	int32x3	is	= x.yzz > x.xxy;
	return {is.x + is.y, 1 - is.x + is.z, 2 - is.y - is.z};
}
template<typename E> int32x4 rank_sort(vec<E,4> x) {
	int32x3	isX		= x.yzw > x.xxx;
	int32x3	isYZ	= x.zww > x.yyz;
	return concat(x, (one - reduce_add(isX)) + concat(isYZ.x + isYZ.y, (one - isYZ.xy) + concat(isYZ.z, one - isYZ.z)));
}

template<typename V> inline enable_if_t<is_vec<V>, vec<int, num_elements_v<V>>> rank_sort(V v) { return rank_sort<element_type<V>>(v); }

//-----------------------------------------------------------------------------
//	indexes
//-----------------------------------------------------------------------------

inline uint32x2	split_index(uint32 i, uint32 n) 	{ return uint32x2{i % n, i / n}; }
inline uint32	flat_index(uint32x2 i, uint32 n)	{ return i.x + n * i.y; }

inline uint32x3	split_index(uint32 i, uint32x2 n)	{ return uint32x3{i % n.x, (i / n.x) % n.y, i / (n.x * n.y)}; }
inline uint32	flat_index(uint32x3 i, uint32x2 n)	{ return i.x + n.x * (i.y  + n.y * i.z); }

inline uint32x4	split_index(uint32 i, uint32x3 n)	{ return uint32x4{i % n.x, (i / n.x) % n.y, (i / (n.x * n.y)) % n.z, i / (n.x * n.y * n.z)}; }
inline uint32	flat_index(uint32x4 i, uint32x3 n)	{ return i.x + n.x * (i.y  + n.y * (i.z + n.z * i.w)); }

//-----------------------------------------------------------------------------
//	pos - implicit one in last component
//-----------------------------------------------------------------------------

template<typename E, int N1, int N2> struct to_pos {
	template<typename T> static constexpr const T&	f(const T &t)	{ return t; }
};

template<typename E, int N> class pos {
	typedef vec<E,N>	V;
public:
	V		v;
	force_inline pos()	{}
	constexpr explicit pos(param_t<V> b)						: v(b) {}
	template<typename K> explicit pos(const iso::constant<K>&)	: v(K::template as<E>()) {}
	force_inline explicit pos(const vec<E, N+1> &a)				: v(simd::shrink<N>(a) / a[N]) {}
	force_inline explicit pos(const pos<E, N-1> &a)				: v(simd::grow<N>(a.v))	{}
	template<int N2> explicit force_inline pos(param_t<pos<E, N2>> b)	: v(to_pos<E, N, N2>(b)) {}
	template<typename P0, typename P1, typename...P> force_inline pos(P0 p0, P1 p1, P...p)	: v(to<E>(p0, p1, p...)) {}

	force_inline pos operator+()	const { return *this; }
	force_inline pos operator-()	const { return pos(-v); }

	template<typename T> force_inline pos& operator+=(const T &b)	{ *this = *this + b; return *this; }
	template<typename T> force_inline pos& operator-=(const T &b)	{ *this = *this - b; return *this; }

//	template<typename M> IF_MAT(M, pos) operator/(const M &m)	const	{ return inverse(m) * *this; }
	template<typename M> IF_MAT(M, pos) operator*=(const M &m)			{ v = m * *this; return *this; }
	template<typename M> IF_MAT(M, pos) operator/=(const M &m)			{ v = inverse(m) * *this; return *this; }

	operator vec<E,N+1>()		const	{ return to<E>(v, one); }
	operator V()				const	{ return v; }
	E	operator[](intptr_t i)	const	{ return v[i]; }

	friend	pos	operator+(const pos &a, const V &b)		{ return pos(a.v + b); }
	friend	pos	operator-(const pos &a, const V &b)		{ return pos(a.v - b); }
	friend	V	operator-(const pos &a, const pos &b)	{ return a.v - b.v; }
//	template<typename B> friend IF_SCALAR(B, pos) operator*(const pos &a, const B &b)	{ return pos(a.v * b); }
//	template<typename B> friend IF_SCALAR(B, pos) operator/(const pos &a, const B &b)	{ return a * reciprocal(b); }
	template<typename B> friend IF_NOTMAT(B, pos) operator*(const pos &a, const B &b)	{ return pos(a.v * b); }
	template<typename B> friend IF_NOTMAT(B, pos) operator/(const pos &a, const B &b)	{ return a * reciprocal(b); }

	auto	operator==(pos b) const	{ return v == b.v; }
	auto	operator!=(pos b) const	{ return v != b.v; }
	auto	operator< (pos b) const	{ return v <  b.v; }
	auto	operator> (pos b) const	{ return v >  b.v; }
	auto	operator<=(pos b) const	{ return v <= b.v; }
	auto	operator>=(pos b) const	{ return v >= b.v; }

	friend auto	mid(pos a, pos b)				{ return pos((a.v + b.v) * half); }
	friend auto cross(pos a, pos b)				{ return cross(a.v, b.v); }
	friend auto dot(pos a, pos b)				{ return dot(a.v, b.v); }
	friend auto dot(pos a, V b)					{ return dot(a.v, b); }
	friend auto dot(pos a, vec<E, N+1> b)		{ return dot(a.v, shrink<N>(b)) + b[N]; }
	friend pos	min(pos a, pos b)				{ return pos(min(a.v, b.v)); }
	friend pos	max(pos a, pos b)				{ return pos(max(a.v, b.v)); }
	friend bool approx_equal(pos a, pos b, E tol = ISO_TOLERANCE) { return approx_equal(a.v, b.v, tol); }

	friend const V&	as_vec(const pos &p)		{ return p.v; }
	template<typename E2> friend auto to(const pos &p) { return to<E2>(p.v); }

	friend void assign(pos &d, const vec<E, N> &s) 			{ d = pos(s); }
	friend void assign(pos &d, const vec<E, N + 1> &s) 		{ d = pos(s); }
	template<typename V> friend enable_if_t<is_vec<V>> assign(pos &d, const V &v) { assign(d, to<E>(v)); }
};

template<typename E, int N> constexpr int num_elements_v<pos<E,N>> = N;

template<typename E, int N> force_inline	pos<E,N - 1>	project(vec<E,N> a)		{ return pos<E, N - 1>(a); }
template<typename A>		force_inline	auto			project(A a)			{ return project<element_type<A>, num_elements_v<A>>(a); }

template<typename E> struct to_pos<E, 4,3>	{ template<typename T> static constexpr auto f(const T &t) { return project(t); } };
template<typename E> struct to_pos<E, 3,2>	{ template<typename T> static constexpr auto f(const T &t) { return project(t); } };
template<typename E> struct to_pos<E, 2,3>	{ template<typename T> static constexpr auto f(const T &t) { return vec<E,3>(t, zero); } };

template<typename E> inline auto area(const interval<pos<E,2>> &i) {
	return reduce_mul(i.b - i.a);
}

template<typename E> inline auto volume(const interval<pos<E,3>> &i) {
	return reduce_mul(i.b - i.a);
}

typedef pos<float,2>	position2;
typedef pos<float,3>	position3;
typedef vec<float, 2>	vector2;
typedef vec<float, 3>	vector3;
typedef vec<float, 4>	vector4;

namespace simd {
	template<typename E, int N> struct element_type_s<pos<E, N>>	: T_type<E> {};
}
//-----------------------------------------------------------------------------
//	homo - homogeneous coordinate
//-----------------------------------------------------------------------------

template<typename E, int N> class homo {
	typedef vec<E, N + 1>	V;
	typedef pos<E, N>		P;
public:
	union {
		V			v;
		vec<E, N>	real;
		struct { E _[N], scale; };
	};
	homo(V v) : v(v)	{}
	force_inline operator P() const	{ return P(v); }
	force_inline operator V() const	{ return v; }

	template<int R>	friend force_inline homo rotate(const homo &x) {
		return swizzle(meta::VL_concat<meta::roll<(R % N + N) % N, meta::make_value_sequence<N>>, meta::value_list<int, N>>(), x.v);
	}

	friend homo operator+(homo a, homo b)	{ return maskedi<bits(N)>(a.v * b.scale) + b.real * a.scale; }
	friend homo operator-(homo a, homo b)	{ return maskedi<bits(N)>(a.v * b.scale) - b.real * a.scale; }
	friend homo operator+(homo a, P b)		{ return maskedi<bits(N)>(a.v) + b.v * a.scale; }
	friend homo operator-(homo a, P b)		{ return maskedi<bits(N)>(a.v) - b.v * a.scale; }
//	friend homo operator+(homo a, P b)		{ return a.v + concat(b.v * a.scale, zero); }
//	friend homo operator-(homo a, P b)		{ return a.v - concat(b.v * a.scale, zero); }
	friend bool approx_equal(homo a, homo b, E tol = ISO_TOLERANCE) { return approx_equal(b.real * a.scale, a.real * b.scale, tol); }
	template<typename T>	friend homo	lerp(homo a, homo b, T t)	{ return lerp(a.v, b.v, t); }
};

template<typename V>  auto as_homo(V v) { return homo<element_type<V>, num_elements_v<V> - 1>(v); }

typedef homo<float,2>	homo2;
typedef homo<float,3>	homo3;

//-----------------------------------------------------------------------------
//	matrices
//-----------------------------------------------------------------------------

template<typename E, int N, int M>	class mat;
template<typename E, int N>			class diagonal;
template<typename E, int N>			class upper;
template<typename E, int N>			class lower;
template<typename E, int N>			class symmetrical;
template<typename E, int N>			class skew_symmetrical;

template<typename E, int N, int M>	constexpr int	num_elements_v<mat<E,N,M>> = M;

template<typename E, int N, int M>	static constexpr bool is_mat<mat<E,N,M>>			= true;
template<typename E, int N>			static constexpr bool is_mat<upper<E,N>>			= true;
template<typename E, int N>			static constexpr bool is_mat<lower<E,N>>			= true;
template<typename E, int N>			static constexpr bool is_mat<diagonal<E,N>>			= true;
template<typename E, int N>			static constexpr bool is_mat<symmetrical<E,N>>		= true;
template<typename E, int N>			static constexpr bool is_mat<skew_symmetrical<E,N>>	= true;

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

template<typename E, int N, int M> bool is_scalar_multiple(const mat<E,N,M> &a, const mat<E,N,M> &b, float scale) {
	for (int i = 0; i < M; i++) {
		if (!is_scalar_multiple(a[i], b[i], scale))
			return false;
	}
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

template<typename E, int N, int M> auto begin(const mat<E,N,M> &a)	{ return make_indexed_iterator(a, int_iterator<int>(0)); }
template<typename E, int N, int M> auto end(const mat<E,N,M> &a)	{ return make_indexed_iterator(a, int_iterator<int>(M)); }

//-------------------------------------
//	transpose/inverse
//-------------------------------------

template<typename T> struct transpose_s {
	const T m;
	typedef noref_t<T> T2;
	explicit transpose_s(T &&t)			: m(forward<T>(t)) {}
	operator	T2()	const			{ return get_transpose(m); }
	auto		column(int i)	const	{ return m.row(i); }
	auto		row(int i)		const	{ return m.column(i); }
	friend auto	get(const transpose_s &t) { return get_transpose(t.m); }
	friend int	max_component_index(const transpose_s &a)	{ return max_component_index(a.m); }//needs to be flipped?

};
template<typename T> struct inverse_s	{
	const T m;
	typedef noref_t<T> T2;
	explicit inverse_s(T &&t)			: m(forward<T>(t)) {}
	operator	T2()	const			{ return get_inverse(m); }
	friend auto	get(const inverse_s &t)	{ return get_inverse(t.m); }
};

template<typename T> struct inverse_s<transpose_s<T> > {
	const T m;
	typedef noref_t<T> T2;
	explicit inverse_s(const transpose_s<T> &t)	: m(t.m) {}
	operator	T2()	const			{ return get_inverse_transpose(m); }
	friend auto	get(const inverse_s &t)	{ return get_inverse_transpose(t.m); }
};

template<typename T> static constexpr bool is_mat<inverse_s<T> >				= true;
template<typename T> static constexpr bool is_mat<transpose_s<T> >				= true;
template<typename T> static constexpr bool is_mat<inverse_s<transpose_s<T> > >	= true;

template<typename T> force_inline auto	inverse(T &&m)									{ return inverse_s<T>(forward<T>(m)); }
template<typename T> force_inline auto	transpose(T &&m)								{ return transpose_s<T>(forward<T>(m)); }
template<typename T> force_inline auto	inverse(inverse_s<T> &&i)						{ return i.m; }
template<typename T> force_inline auto	inverse(transpose_s<T> &&m)						{ return inverse_s<transpose_s<T>>(m); }
template<typename T> force_inline auto	inverse(inverse_s<transpose_s<T>> &&i)			{ return transpose(i.m); }
template<typename T> force_inline auto	transpose(transpose_s<T> &&i)					{ return i.m; }
template<typename T> force_inline auto	transpose(inverse_s<T> &&i)						{ return inverse(transpose(i.m)); }
template<typename T> force_inline auto	transpose(inverse_s<transpose_s<T>> &&i)		{ return inverse(i.m); }

template<typename T> force_inline auto	inverse(const inverse_s<T> &i)					{ return i.m; }
template<typename T> force_inline auto	inverse(const transpose_s<T> &m)				{ return inverse_s<transpose_s<T>>(m); }
template<typename T> force_inline auto	inverse(const inverse_s<transpose_s<T>> &i)		{ return transpose(i.m); }
template<typename T> force_inline auto	transpose(const transpose_s<T> &i)				{ return i.m; }
template<typename T> force_inline auto	transpose(const inverse_s<T> &i)				{ return inverse(transpose(i.m)); }
template<typename T> force_inline auto	transpose(const inverse_s<transpose_s<T>> &i)	{ return inverse(i.m); }

template<typename A, typename B> force_inline auto	operator*(const A &a, const transpose_s<B> &b)	->IF_NOTMAT(A, decltype(transpose(b) * a))	{ return b.m * a; }
template<typename A, typename B> force_inline auto	operator*(const A &a, const transpose_s<B> &b)	->IF_MAT(A, decltype(a * get(b)))			{ return a * get(b); }

template<typename A, typename B> force_inline auto	operator*(const A &a, const inverse_s<B> &b)							{ return a * get(b); }
template<typename A, typename B> force_inline auto	operator*(const A &a, const inverse_s<transpose_s<B>> &b)				{ return a * get(b); }

template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const B &b)							{ return get(a) * b; }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const inverse_s<B> &b)					{ return inverse(b.m * a.m); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const transpose_s<B> &b)				{ return get(a) * get(b); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<A> &a, const inverse_s<transpose_s<B>> &b)	{ return inverse(transpose(b.m) * a.m); }

template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const B &b)							{ return get(a) * b; }
template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const inverse_s<B> &b)				{ return get(a) * get(b); }
template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const transpose_s<B> &b)				{ return transpose(b.m * a.m); }
template<typename A, typename B> force_inline auto	operator*(const transpose_s<A> &a, const inverse_s<transpose_s<B>> &b)	{ return transpose(inverse(b) * a.m); }

template<typename A, typename B> force_inline auto	operator*(const inverse_s<transpose_s<A>> &a, const B &b)->enable_if_t<is_mat<B>, decltype(get(a) * b)> { return get(a) * b; }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<transpose_s<A>> &a, const inverse_s<B> &b)				{ return inverse(b.m * transpose(a.m)); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<transpose_s<A>> &a, const transpose_s<B> &b)				{ return transpose(b.m * get_inverse(a.m)); }
template<typename A, typename B> force_inline auto	operator*(const inverse_s<transpose_s<A>> &a, const inverse_s<transpose_s<B>> &b)	{ return inverse(transpose(a.m * b.m)); }

template<typename A, typename B, typename=enable_if_t<is_vec<A> && is_mat<B>>> force_inline auto	operator*(const A &a, const B &b)	{ return transpose(b) * a; }

//template<typename A, typename B, typename=enable_if_t<is_mat<B>>> force_inline auto	operator/(const A &a, const B &b)	{ return inverse(b) * a; }
template<typename A, typename B> force_inline decltype(declval<IF_MAT(B,B)>() * declval<A>())	operator/(const A &a, const B &b)	{ return inverse(b) * a; }

template<typename T> auto get_inverse_transpose(const T &t) {
	return transpose(get_inverse(t));
}

template<typename T> force_inline auto	cofactors(const transpose_s<T> &m)			{ return transpose(cofactors(m.m)); }

template<typename M, typename I> inline void store(const transpose_s<M> &v, I p) 	{ store(get(v), p); }

//-----------------------------------------------------------------------------
//	generic mats
//-----------------------------------------------------------------------------

template<int A, int B> struct column_helper {
	static const int M = (A + B) / 2;
	template<typename T> static auto f(const T &m, int i) {
		return M < i ? column_helper<M + 1, B>::f(m, i) : column_helper<A, M>::f(m, i);
	}
};
template<int A> struct column_helper<A, A> {
	template<typename T> static auto f(const T &m, int i) {
		return m.template column<A>();
	}
};

//-------------------------------------
// diagonal + triangular
//-------------------------------------

template<typename E, int N> class diagonal {
protected:
	typedef diagonal<E,N>			D;
public:
	vec<E,N>	d;
	force_inline diagonal()							{}
	explicit constexpr diagonal(vec<E,N> d) : d(d)	{}
	force_inline E	det()	const	{ return reduce_mul(d); }
	force_inline E	trace()	const	{ return reduce_add(d); }

	template<int I> force_inline vec<E,N> column()	const	{ return extend_right<N>(extend_left<I + 1>(d[I])); }
	template<int I> force_inline vec<E,N> row()		const	{ return column<I>(); }
	force_inline vec<E,N>		column(int i)		const	{ return column_helper<0, N - 1>::f(*this, i); }
	force_inline vec<E,N>		row(int i)			const	{ return column_helper<0, N - 1>::f(*this, i); }
	force_inline E				operator[](int i)	const	{ return d[i]; }

	template<int...I> friend auto	swizzle(const diagonal &m)	{ return diagonal<E, sizeof...(I)>(swizzle<I...>(m.d)); }
};

template<typename E> class diagonal<E, 1> {
protected:
	typedef diagonal<E,1>			D;
public:
	E		d;
	force_inline diagonal()					{}
	explicit constexpr diagonal(E d) : d(d)	{}
	force_inline E	det()	const	{ return d; }
	force_inline E	trace()	const	{ return d; }

	template<int I> force_inline E column()			const	{ return d; }
	template<int I> force_inline E row()			const	{ return d; }
	force_inline E				column(int i)		const	{ return d; }
	force_inline E				row(int i)			const	{ return d; }
	force_inline E				operator[](int i)	const	{ return d; }

	template<int...I> friend auto	swizzle(const diagonal &m)	{ return m; }
};

template<typename E, int N, typename B> enable_if_t<is_num<B>, diagonal<E,N>> operator*(const diagonal<E,N> &a, const B &b)	{ return diagonal<E,N>(a.d * b); }
template<typename E, int N, typename B> enable_if_t<is_num<B>, diagonal<E,N>> operator/(const diagonal<E,N> &a, const B &b)	{ return a * reciprocal(b); }
template<typename E, int N> auto	operator-(const diagonal<E,N> &a)							{ return diagonal<E,N>(-a.d); }
template<typename E, int N> auto	operator+(const diagonal<E,N> &a, const diagonal<E,N> &b)	{ return diagonal<E,N>(a.d + b.d); }
template<typename E, int N> auto	operator-(const diagonal<E,N> &a, const diagonal<E,N> &b)	{ return diagonal<E,N>(a.d - b.d); }
template<typename E, int N> auto&	transpose(const diagonal<E,N> &m)							{ return m; }
template<typename E, int N> auto&&	transpose(diagonal<E,N> &&m)								{ return (diagonal<E,N>&&)m; }
template<typename E, int N> auto	get_inverse(const diagonal<E,N> &m)							{ return diagonal<E,N>(reciprocal(m.d)); }

template<typename M, typename V, size_t... I>	M 	_scale(const M &a, const V &b, index_list<I...>)			{ return {a.column(I) * b...}; }
template<typename M, typename V, size_t... I>	M 	_pre_scale(const M &a, const V &b, index_list<I...>) 		{ return {a.column(I) * b[I]...}; }

template<typename E>		force_inline mat<E,2,2>	operator*(const diagonal<E,2> &a, const mat<E,2,2> &b)		{ return mat<E,2,2>(b.v() * a.d.xyxy); }
template<typename E>		force_inline mat<E,2,2>	operator*(const mat<E,2,2> &a, const diagonal<E,2> &b)		{ return mat<E,2,2>(a.v() * b.d.xxyy); }
template<typename E, int N> force_inline mat<E,N,N>	operator*(const diagonal<E,N> &a, const mat<E,N,N> &b)		{ return _scale(b, a.d, meta::make_index_list<N>()); }
template<typename E, int N> force_inline mat<E,N,N>	operator*(const mat<E,N,N> &a, const diagonal<E,N> &b)		{ return _pre_scale(a, b.d, meta::make_index_list<N>()); }
template<typename E, int N> force_inline auto		operator*(const diagonal<E,N> &a, vec<E,N> b)				{ return a.d * b; }

template<int N2> struct tri_swizzle_s;

template<typename E, int N> class _triangular : protected diagonal<E,N>, protected _triangular<E, N - 1> {
	template<int N2> friend struct tri_swizzle_s;

protected:
	typedef diagonal<E,N>			D;
	typedef _triangular<E, N - 1>	T;
	_triangular			_abs()							const	{ return _triangular(abs(d), T::_abs()); }
	_triangular			_neg()							const	{ return _triangular(-d, T::_neg()); }
	_triangular			_add(const _triangular &b)		const	{ return _triangular(d + b.d, T::_add(b)); }
	_triangular			_sub(const _triangular &b)		const	{ return _triangular(d - b.d, T::_sub(b)); }
	_triangular			_sub(const D &b)				const	{ return _triangular(d - b.d, *(const T*)this); }
	_triangular			_mul(const _triangular &b)		const	{ return _triangular(d * b.d, T::_mul(b)); }
	template<typename B> _triangular _smul(const B &b)	const	{ return _triangular(d * b, T::_smul(b)); }
	auto				_flat()							const	{ return concat(d, T::_flat()); }

	template<size_t...L, size_t...R> constexpr auto	make_vec(index_list<L...>, index_list<R...>) const	{
		return to<E>(diagonal<L < R ? R - L : L - R>()[L < R ? L : R]...);
	}
	template<int...I> _triangular<E, sizeof...(I)>	_swizzle() const {
		return {swizzle<I...>(d), tri_swizzle_s<sizeof...(I) - 1>::f(*this, meta::left<-1, index_list<I...>>(), meta::right<-1, index_list<I...>>())};
	}

	template<int C, int O, int L> vec<E,L> get_col()	const { return make_vec(meta::muladd<0, C, meta::make_index_list<L>>(), meta::muladd<1, O, meta::make_index_list<L>>()); }

	template<int I> vec<E, N>		uvec()		const { return extend_right<N>(get_col<I, 0, I + 1>()); }
	template<int I> vec<E, N>		lvec()		const { return extend_left<N>(get_col<I, I, N - I>()); }
	template<int I> vec<E, N>		svec()		const { return get_col<I, 0, N>(); }
	template<int I> vec<E, N + 1>	ssvec()		const { return to<E>(get_col<I - 1, 0, I>(), zero, -get_col<I, I, N - I>()); }
	template<> vec<E,N + 1>			ssvec<0>()	const { return to<E>(zero, -get_col<0, 0, N>()); }
	template<> vec<E,N + 1>			ssvec<N>()	const { return to<E>(get_col<N - 1, 0, N>(), zero); }

public:
	using D::d;
	using _triangular<E,2>::o;
	using D::trace;
	_triangular()	{}
	constexpr _triangular(const _zero&)	: D(vec<E,N>(zero)), T(zero) {}
	template<typename P0, typename...P> constexpr _triangular(vec<E,N> d, const P0& p0, const P&...p)	: D(d), T(p0, p...)	{}

	constexpr		const D&	diagonal()			const	{ return *this; }
	constexpr		D&			diagonal()					{ return *this; }
	template<int O> auto&		diagonal()			const	{ return static_cast<const iso::diagonal<E, N - O>&>(*this); }
	template<int O> auto&		diagonal()					{ return static_cast<iso::diagonal<E, N - O>&>(*this); }
	template<int...I> auto		swizzled_diag()		const	{ return swizzle<I...>(d); }
	const _triangular<E, N-1>&	sub_triangular()	const	{ return *this; }

	friend int		max_component_index(const _triangular &a)	{ return max_component_index(a._flat()); }
};

template<typename E> class _triangular<E, 2> {
protected:
	typedef diagonal<E,2>			D;
	typedef diagonal<E,1>			D1;
public:
	union {
		vec<E,3>	o;
		struct { D d; D1 d1; };
	};
protected:
	_triangular() {}
	_triangular			_abs()							const	{ return _triangular(abs(o)); }
	_triangular			_neg()							const	{ return _triangular(-o); }
	_triangular			_add(const _triangular &b)		const	{ return _triangular(o + b.o); }
	_triangular			_sub(const _triangular &b)		const	{ return _triangular(o - b.o); }
	_triangular			_sub(const D &b)				const	{ return _triangular(d.d - b.d, o.z); }
//	_triangular			_sub(const D &b)				const	{ return _triangular(masked.xy(o) - b.d); }
	_triangular			_mul(const _triangular &b)		const	{ return _triangular(o * b.o); }
	template<typename B> _triangular _smul(const B &b)	const	{ return _triangular(o * b); }

	auto				_flat()							const	{ return o; }

	template<int I, int J>	auto	_swizzle()			const	{ return _triangular(swizzle<I, J, 2>(o)); }
	template<int I>			auto	_swizzle()			const	{ return o[I]; }

	template<int I> vec<E, 2>	uvec()		const;
	template<> vec<E, 2>		uvec<0>()	const { return {o.x, 0}; }
	template<> vec<E, 2>		uvec<1>()	const { return o.zy; }

	template<int I> vec<E, 2>	lvec()		const;
	template<> vec<E, 2>		lvec<0>()	const { return o.xz; }
	template<> vec<E, 2>		lvec<1>()	const { return {0, o.y}; }
	
	template<int I> vec<E, 2>	svec()		const;
	template<> vec<E, 2>		svec<0>()	const { return o.xz; }
	template<> vec<E, 2>		svec<1>()	const { return o.zy; }

	operator const D&()		const { return d; }
	operator const D1&()	const { return d1; }
public:
	constexpr _triangular(vec<E,3> o)				: o(o) {}
	constexpr _triangular(vec<E,2> d, vec<E,1> o)	: o(concat(d, o)) {}
	force_inline _triangular& operator=(const _triangular &b)	{ o = b.o; return *this; }

	force_inline const D&	diagonal()				{ return d; }
	force_inline D&			diagonal()		const	{ return d; }
	template<int O> const iso::diagonal<E, 2 - O>&	diagonal()	const	{ return *this; }

	force_inline E			det()			const	{ return o.x * o.y; }
	force_inline E			trace()			const	{ return o.x + o.y; }

	friend int		max_component_index(const _triangular &a)	{ return max_component_index(a._flat()); }
};

template<typename E> class _triangular<E, 1> : public diagonal<E,1> {};

template<int N2> struct tri_swizzle_s {
	template<typename E, int N, typename L, typename R> static _triangular<E, N2> f(const _triangular<E, N> &tri, const L &left, const R &right) {
		return {tri.make_vec(left, right), tri_swizzle_s<N2 - 1>::f(tri, meta::left<-1, L>(), meta::right<-1, R>())};
	}
};
template<> struct tri_swizzle_s<1> {
	template<typename E, int N, size_t L, size_t R> static E f(const _triangular<E,N> &tri, const index_list<L>&, const index_list<R>&) {
		return tri.diagonal<L < R ? R - L : L - R>()[L < R ? L : R];
	}
};

template<typename E, int N> class lower : public _triangular<E,N> {
	typedef _triangular<E, N> B;
	using typename B::D;
public:
	using B::_triangular;
	force_inline lower() {}
	constexpr lower(const B &b) : B(b) {}
	template<int I> force_inline vec<E,N> column()	const	{ return B::template lvec<I>(); }
	force_inline vec<E,N>		column(int i)		const	{ return column_helper<0, N - 1>::f(*this, i); }
	template<int I> force_inline vec<E,N> row()		const	{ return B::template uvec<I>(); }
	force_inline vec<E,N>		row(int i)			const	{ return column_helper<0, N - 1>::f(transpose(*this), i); }

	template<typename T> friend IF_SCALAR(T,lower) operator*(const lower &a, const T &b)	{ return a._smul(b); }
	template<typename T> friend IF_SCALAR(T,lower) operator/(const lower &a, const T &b)	{ return a._smul(reciprocal(b)); }
	friend lower				direct_mul(const lower &a, const lower &b)	{ return a._mul(b); }
	friend lower				operator~(const lower &a)					{ return cofactors(a); }
	friend lower				operator-(const lower &a)					{ return a._neg(); }
	friend lower				operator+(const lower &a, const lower &b)	{ return a._add(b); }
	friend lower				operator-(const lower &a, const lower &b)	{ return a._sub(b); }
	friend lower				operator-(const lower &a, const D &b)		{ return a._sub(b); }
	friend lower				get_inverse(const lower &a)					{ return ~a * reciprocal(a.det()); }
	friend lower				abs(const lower &a)							{ return a._abs(); }
	friend const upper<E,N>&	transpose(const lower &a)					{ return reinterpret_cast<const upper<E,N>&>(a); }
	friend upper<E,N>&&			transpose(lower &&a)						{ return reinterpret_cast<upper<E,N>&&>(a); }
	friend upper<E,N>			cofactors(const lower &a)					{ return get_adjoint(a); }
	template<int...I> friend lower<E, sizeof...(I)>	swizzle(const lower &m)	{ return m.template _swizzle<I...>(); }

	friend const upper<E,N>&	get_transpose(const lower &a)				{ return reinterpret_cast<const upper<E,N>&>(a); }
	friend mat<E,N,N>	operator*(const lower &a, const mat<E,N,N> &b)		{ return mat<E,N,N>(a) * b; }
	friend mat<E,N,N>	operator*(const mat<E,N,N> &a, const lower &b)		{ return a * mat<E,N,N>(b); }
};

template<typename E, int N>	vec<E,N>	operator*(const lower<E, N> &a, vec<E,N> b) {
	return a.diagonal<0>() * b + extend_left<N>(lower<E, N - 1>(a.sub_triangular()) * shrink<N-1>(b));
}
template<typename E>		vec<E,2>	operator*(const lower<E,2> &a, vec<E,2> b) {
	return a.o.xy * b + vec<E, 2>{zero, a.o.z* b.x};
}


template<typename E, int N> class upper : public _triangular<E, N> {
	typedef _triangular<E, N> B;
	using typename B::D;
public:
	using B::_triangular;
	force_inline upper() {}
	constexpr upper(const B &b) : B(b) {}
	template<int I> force_inline vec<E,N> column()	const	{ return B::template uvec<I>(); }
	force_inline vec<E,N>		column(int i)		const	{ return column_helper<0, N - 1>::f(*this, i); }
	template<int I> force_inline vec<E,N> row()		const	{ return B::template lvec<I>(); }
	force_inline vec<E,N>		row(int i)			const	{ return column_helper<0, N - 1>::f(transpose(*this), i); }

	template<typename T> friend IF_SCALAR(T,upper) operator*(const upper &a, const T &b)	{ return a._smul(b); }
	template<typename T> friend IF_SCALAR(T,upper) operator/(const upper &a, const T &b)	{ return a._smul(reciprocal(b)); }
	friend upper				direct_mul(const upper &a, const upper &b)	{ return a._mul(b); }
	friend upper				operator~(const upper &a)					{ return cofactors(a); }
	friend upper				operator-(const upper &a)					{ return a._neg(); }
	friend upper				operator+(const upper &a, const upper &b)	{ return a._add(b); }
	friend upper				operator-(const upper &a, const upper &b)	{ return a._sub(b); }
	friend upper				operator-(const upper &a, const D &b)		{ return a._sub(b); }
	friend upper				get_inverse(const upper &a)					{ return ~a * reciprocal(a.det()); }
	friend upper				abs(const upper &a)							{ return a._abs(); }
	friend const lower<E,N>&	transpose(const upper &a)					{ return reinterpret_cast<const lower<E,N>&>(a); }
	friend lower<E,N>&&			transpose(upper &&a)						{ return reinterpret_cast<lower<E,N>&&>(a); }
	friend lower<E,N>			cofactors(const upper &a)					{ return get_adjoint(a); }
	template<int...I> friend upper<E, sizeof...(I)>	swizzle(const upper &m)	{ return m.template _swizzle<I...>(); }

	friend const lower<E,N>&	get_transpose(const upper &a)				{ return reinterpret_cast<const lower<E,N>&>(a); }
	friend mat<E,N,N>	operator*(const upper &a, const mat<E,N,N> &b)		{ return mat<E,N,N>(a) * b; }
	friend mat<E,N,N>	operator*(const mat<E,N,N> &a, const upper &b)		{ return a * mat<E,N,N>(b); }
};

template<typename E, int N> vec<E,N>	operator*(const upper<E,N> &a, vec<E,N> b) {
	return a.diagonal<0>() * b + extend_right<N>(upper<E, N - 1>(a.sub_triangular()) * shrink<N-1>(rotate<1>(b)));
}
template<typename E>		vec<E,2>	operator*(const upper<E,2> &a, vec<E,2> b) {
	return a.o.xy * b + vec<E, 2>{a.o.z* b.y, zero};
}

template<typename E, int N> class symmetrical;
template<typename E, int N> symmetrical<E, N> 	get_adjoint(const symmetrical<E,N> &m);
template<typename E, int N> E					get_det(const symmetrical<E,N> &m);

template<typename E, int N> class symmetrical : public _triangular<E, N> {
protected:
	typedef _triangular<E, N>	B;
	using typename B::D;
	friend symmetrical 	get_adjoint<E,N>(const symmetrical &m);
	friend E			get_det<E,N>(const symmetrical &m);
public:
	using B::_triangular;
	force_inline symmetrical()					{}
	constexpr symmetrical(const B &b) : B(b) {}
	constexpr symmetrical(const D &t) : B(t, zero) {}
	template<int I> force_inline vec<E,N> column()	const	{ return B::template svec<I>(); }
	force_inline vec<E,N>		column(int i)		const	{ return column_helper<0, N - 1>::f(*this, i); }
	template<int I> force_inline vec<E,N> row()		const	{ return B::template svec<I>(); }
	force_inline vec<E,N>		row(int i)			const	{ return column_helper<0, N - 1>::f(*this, i); }
	E							det()				const	{ return get_det(*this); }

	friend symmetrical			operator~(const symmetrical &a)							{ return cofactors(a); }
	template<typename T> friend IF_SCALAR(T,symmetrical) operator*(const symmetrical &a, const T &b)	{ return a._smul(b); }
	template<typename T> friend IF_SCALAR(T,symmetrical) operator/(const symmetrical &a, const T &b)	{ return a._smul(reciprocal(b)); }
	friend symmetrical			direct_mul(const symmetrical &a, const symmetrical &b)	{ return a._mul(b); }
	friend symmetrical			operator-(const symmetrical &a)							{ return a._neg(); }
	friend symmetrical			operator+(const symmetrical &a, const symmetrical &b)	{ return a._add(b); }
	friend symmetrical			operator-(const symmetrical &a, const symmetrical &b)	{ return a._sub(b); }
	friend symmetrical			operator-(const symmetrical &a, const D &b)				{ return a._sub(b); }
	friend symmetrical			cofactors(const symmetrical &a)							{ return get_adjoint(a); }
	friend const symmetrical&	transpose(const symmetrical &a)							{ return a; }
	friend symmetrical&&		transpose(symmetrical &&a)								{ return (symmetrical&&)a; }
	friend symmetrical			get_inverse(const symmetrical &a)						{ return ~a * reciprocal(a.det()); }
	friend symmetrical			abs(const symmetrical &a)								{ return a._abs(); }
	template<int...I> friend symmetrical<E, sizeof...(I)> swizzle(const symmetrical &m)	{ return m.template _swizzle<I...>(); }

	friend mat<E,N,N>	operator*(const symmetrical &a, const mat<E,N,N> &b)	{ return mat<E,N,N>(a) * b; }
	friend mat<E,N,N>	operator*(const mat<E,N,N> &a, const symmetrical &b)	{ return a * mat<E,N,N>(b); }
	friend mat<E,N,N>	operator*(const symmetrical &a, const symmetrical &b)	{ return mat<E,N,N>(a) * mat<E,N,N>(b); }
	friend mat<E,N,N>	operator*(const diagonal<E,N> &a, const symmetrical &b)	{ return a * mat<E,N,N>(b); }
};

template<typename E, int N> vec<E,N>	operator*(const symmetrical<E,N> &a, vec<E,N> b) {
	return a.diagonal<0>() * b
		+ extend_left<N>(lower<E, N-1>(a.sub_triangular())  * shrink<N-1>(b))
		+ extend_right<N>(upper<E, N-1>(a.sub_triangular()) * shrink<N-1>(rotate<1>(b)));
}

template<typename E>		vec<E,2>	operator*(const symmetrical<E,2> &a, vec<E,2> b) {
	return a.o.xy * b + a.o.z * b;
}

template<typename E, int N> class skew_symmetrical : public _triangular<E, N - 1> {
	typedef _triangular<E, N - 1> B;
	using typename B::D;
	constexpr skew_symmetrical(const B &b) : B(b)	{}
public:
	using B::B;
	force_inline skew_symmetrical()					{}
	template<int I> force_inline auto column()		const	{ return B::template ssvec<I>(); }
	force_inline auto			column(int i)		const	{ return column_helper<0, N - 2>::f(*this, i); }
	template<int I> force_inline auto row()			const	{ return -B::template ssvec<I>(); }
	force_inline auto			row(int i)			const	{ return column_helper<0, N - 2>::f(*this, i); }
	E							det()				const	{ return get_det(*this); }
	force_inline auto			trace()				const	{ return zero; }

	friend skew_symmetrical			operator~(const skew_symmetrical &a)							{ return cofactors(a); }
	template<typename T> friend IF_SCALAR(T,skew_symmetrical) operator*(const skew_symmetrical &a, const T &b)	{ return a._smul(b); }
	template<typename T> friend IF_SCALAR(T,skew_symmetrical) operator/(const skew_symmetrical &a, const T &b)	{ return a._smul(reciprocal(b)); }
	friend skew_symmetrical			operator-(const skew_symmetrical &a)							{ return a._neg(); }
	friend skew_symmetrical			operator-(const skew_symmetrical &a, const D &b)				{ return a._sub(b); }
	friend skew_symmetrical			operator+(const skew_symmetrical &a, const skew_symmetrical &b)	{ return a._add(b); }
	friend skew_symmetrical			operator-(const skew_symmetrical &a, const skew_symmetrical &b)	{ return a._sub(b); }
	friend skew_symmetrical			cofactors(const skew_symmetrical &a)							{ return get_adjoint(a); }
	friend skew_symmetrical			get_transpose(const skew_symmetrical &a)						{ return -a; }
	friend skew_symmetrical			get_inverse(const skew_symmetrical &a)							{ return ~a * reciprocal(a.det()); }
	friend symmetrical<E,N>			abs(const skew_symmetrical &a)									{ return a._abs(); }
	template<int...I> friend skew_symmetrical<E, sizeof...(I)> swizzle(const skew_symmetrical &m)	{ return m.template _swizzle<I...>(); }
};

template<typename E, int N> enable_if_t<N & 1, _zero>	get_det(const skew_symmetrical<E,N>&)		{ return zero; }

//-------------------------------------
// mat_base
//-------------------------------------

template<typename E, int N, int M, typename=void> class mat_base : public mat_base<E, N, M - 1> {
	typedef mat_base<E, N, M - 1> 	B;
	vec<E, N>	_;
protected:
	mat_base	_abs()					const { return {B::_abs(), abs(_)}; }
	mat_base	_neg()					const { return {B::_neg(), -_}; }
	mat_base	_add(const mat_base &b)	const { return {B::_add(b), _ + b._}; }
	mat_base	_sub(const mat_base &b)	const { return {B::_sub(b), _ - b._}; }
	auto		_flat()					const { return concat(B::_flat(), _); }
	template<typename B> mat_base	_smul(const B &b) const { return {B::_smul(b), _ * b}; }

public:
	using B::column;
	using B::operator[];

	force_inline mat_base()						{}
	constexpr mat_base(const _identity&)		: B(identity), _(select(1<<(M - 1), vec<E,N>(one), zero)) {}
	constexpr mat_base(const _zero&)			: B(zero), _(zero) {}
	constexpr mat_base(const B &b, vec<E, N> _)	: B(b), _(_) {}
	template<typename T0, typename T1, typename... T> constexpr mat_base(T0&& t0, T1&& t1, T&&...t) { vec<E,N> array[] = {t0, t1, t...}; *this = *(mat_base*)array; }

	vec<E, M>		row(int i) const {
		vec<E, M> r;
		for (int j = 0; j < M; j++)
			r[j] = column(j)[i];
		return r;
	}

	friend bool operator==(const mat_base &a, const mat_base &b)	{ return (const B&)a == (const B&)b && a._ == b._; }
	friend bool operator!=(const mat_base &a, const mat_base &b)	{ return !(a == b); }
};

template<typename E, int N> class mat_base<E,N,1, void> {
protected:
	mat_base	_abs()					const { return abs(x); }
	mat_base	_neg()					const { return -x; }
	mat_base	_add(const mat_base &b)	const { return x + b.x; }
	mat_base	_sub(const mat_base &b)	const { return x - b.x; }
	auto		_flat()					const { return x; }
	template<typename B> mat_base	_smul(const B &b) const { return x * b; }
public:
	vec<E,N>	x;

	force_inline mat_base()					{}
	constexpr mat_base(const _identity&)	: x(select(1, vec<E,N>(one), zero)) {}
	constexpr mat_base(const _zero&)		: x(zero) {}
	constexpr mat_base(vec<E,N> x)			: x(x) {}

	template<int I> const vec<E,N>&	column()	const	{ return (&x)[I]; }
	template<int I> vec<E,N>&		column()			{ return (&x)[I]; }
	const vec<E,N>&	column(int i)		const	{ return (&x)[i]; }
	vec<E,N>&		column(int i)				{ return (&x)[i]; }
	const vec<E,N>&	operator[](int i)	const	{ return (&x)[i]; }
	vec<E,N>&		operator[](int i)			{ return (&x)[i]; }
	E				row(int i)			const	{ return x[i]; }

	friend bool operator==(const mat_base &a, const mat_base &b)	{ return all(a.x == b.x); }
	friend bool operator!=(const mat_base &a, const mat_base &b)	{ return !(a == b); }
};

template<typename E, int M> class mat_base<E,1,M, void> {
protected:
	vec<E,M>	r;
	mat_base	_abs()					const { return abs(r); }
	mat_base	_neg()					const { return -r; }
	mat_base	_add(const mat_base &b)	const { return r + b.r; }
	mat_base	_sub(const mat_base &b)	const { return r - b.r; }
	auto		_flat()					const { return r; }
	template<typename B> mat_base	_smul(const B &b) const { return r * b; }
public:
	force_inline mat_base()				{}
	constexpr mat_base(const _zero&)	: r(zero)	{}
	constexpr mat_base(vec<E,M> r)		: r(r)		{}
	template<typename T0, typename T1, typename... T> constexpr mat_base(T0&& t0, T1&& t1, T&&...t) : r{E(t0), E(t1), E(t)...} {}

	const E&		operator[](int i)	const	{ return ((E*)&r)[i]; }
	E&				operator[](int i)			{ return ((E*)&r)[i]; }
	vec<E,M>		row(int i)			const	{ return r; }
};

template<typename E, int N> class mat_base<E,N,2, enable_if_t<(N > 1)>> {
	typedef vec<E, N * 2>	V;
protected:
	mat_base	_abs()					const { return {abs(x), abs(y)}; }
	mat_base	_neg()					const { return {-x, -y}; }
	mat_base	_add(const mat_base &b)	const { return {x + b.x, y + b.y}; }
	mat_base	_sub(const mat_base &b)	const { return {x - b.x, y - b.y}; }
	auto		_flat()					const { return concat(x, y); }
	template<typename B> mat_base	_smul(const B &b) const { return {x * b, y * b}; }
public:
	vec<E,N>	x, y;

	force_inline mat_base()			{}
	constexpr mat_base(const _identity&)					: x(select(1, vec<E,N>(one), zero)), y(select(2, vec<E,N>(one), zero)) {}
	constexpr mat_base(const _zero&)						: x(zero), y(zero) {}
	constexpr explicit mat_base(const mat_base<E,N-1,2> &m, vec<E,2> y = zero) : x(concat(m.x,y.x)), y(concat(m.y,y.y)) {}
	constexpr mat_base(vec<E,N> x, vec<E,N> y)				: x(x), y(y) {}
	constexpr mat_base(V v)									: x(v.lo), y(v.hi) {}
	V				v()	const { return concat(x, y); }

	template<int I> const vec<E,N>&	column()	const	{ return (&x)[I]; }
	template<int I> vec<E,N>&		column()			{ return (&x)[I]; }
	const vec<E,N>&	column(int i)				const	{ return (&x)[i]; }
	vec<E,N>&		column(int i)						{ return (&x)[i]; }
	const vec<E,N>&	operator[](int i)			const	{ return (&x)[i]; }
	vec<E,N>&		operator[](int i)					{ return (&x)[i]; }
	vec<E,2>		row(int i)					const	{ return {x[i], y[i]}; }

	friend bool operator==(const mat_base &a, const mat_base &b)	{ return all(a.x == b.x & a.y == b.y); }
	friend bool operator!=(const mat_base &a, const mat_base &b)	{ return !(a == b); }
};

template<typename E, int N> class mat_base<E,N,3, enable_if_t<(N > 1)>> : public mat_base<E,N,2> {
	typedef mat_base<E,N,2> B;
protected:
	mat_base	_abs()					const { return {abs(x), abs(y), abs(z)}; }
	mat_base	_neg()					const { return {-x, -y, -z}; }
	mat_base	_add(const mat_base &b)	const { return {x + b.x, y + b.y, z + b.z}; }
	mat_base	_sub(const mat_base &b)	const { return {x - b.x, y - b.y, z - b.z}; }
	auto		_flat()					const { return concat(x, y, z); }
	template<typename B> mat_base	_smul(const B &b) const { return {x * b, y * b, z * b}; }
public:
	using B::x; using B::y;
	vec<E,N>	z;

	force_inline mat_base()			{}
	constexpr mat_base(const _identity&)					: B(identity), z(select(4, vec<E,N>(one), zero)) {}
	constexpr mat_base(const _zero&)						: B(zero), z(zero) {}
	constexpr mat_base(const B &b, vec<E,N> z)				: B(b), z(z) {}
	constexpr explicit mat_base(const B &b)					: B(b), z(z_axis) {}
	constexpr explicit mat_base(const mat_base<E,N-1,2> &m)	: B(concat(m.x, zero), concat(m.y, zero)), z(concat(vec<E,N-1>(zero), one))	{}
	constexpr explicit mat_base(const mat_base<E,N-1,3> &m, vec<E,3> z = z_axis) : B(concat(m.x, z.x), concat(m.y, z.y)), z(concat(m.z, z.z)) {}
//	explicit mat_base(const mat_base<E,N-1,3> &m, vec<E,3> z = z_axis) {}
	constexpr mat_base(vec<E,N> x, vec<E,N> y, vec<E,N> z)	: B(x, y), z(z)	{}

	vec<E, 3>		row(int i)			const	{ return {x[i], y[i], z[i]}; }

	friend bool operator==(const mat_base &a, const mat_base &b)	{ return (const B&)a == (const B&)b && all(a.z == b.z); }
	friend bool operator!=(const mat_base &a, const mat_base &b)	{ return !(a == b); }
};

template<typename E, int N> class mat_base<E,N,4, enable_if_t<(N > 1)>> : public mat_base<E,N,3> {
	typedef mat_base<E,N,3> B;
protected:
	mat_base	_abs()					const { return {abs(x), abs(y), abs(z), abs(w)}; }
	mat_base	_neg()					const { return {-x, -y, -z, -w}; }
	mat_base	_add(const mat_base &b)	const { return {x + b.x, y + b.y, z + b.z, w + b.w}; }
	mat_base	_sub(const mat_base &b)	const { return {x - b.x, y - b.y, z - b.z, w - b.w}; }
	auto		_flat()					const { return concat(x, y, z, w); }
	template<typename B> mat_base	_smul(const B &b) const { return {x * b, y * b, z * b, w * b}; }
public:
	using B::x; using B::y; using B::z;
	vec<E,N>	w;

	force_inline mat_base()			{}
	force_inline mat_base(const _identity&)						: B(identity), w(select(8, vec<E,N>(one), zero)) {}
	force_inline mat_base(const _zero&)							: B(zero), w(zero)	{}
	force_inline mat_base(const B &b, vec<E,N> w)				: B(b), w(w) {}
	force_inline explicit mat_base(const B &b)					: B(b), w(w_axis) {}
	force_inline explicit mat_base(const mat_base<E,N-1,3> &m)	: B(concat(m.x, zero), concat(m.y, zero), concat(m.z, N==3)), w(concat(vec<E,N-1>(zero), one)) {}
	force_inline explicit mat_base(const mat_base<E,N-1,4> &m, vec<E,4> w = w_axis) : B(concat(m.x, w.x), concat(m.y, w.y), concat(m.z, w.z)), w(concat(m.w, w.w)) {}
	force_inline mat_base(vec<E,N> x, vec<E,N> y, vec<E,N> z, vec<E,N> w) : B(x, y, z), w(w) {}

	vec<E, 4>		row(int i)			const	{ return {x[i], y[i], z[i], w[i]}; }

	friend bool operator==(const mat_base &a, const mat_base &b)	{ return (const B&)a == (const B&)b && all(a.w == b.w); }
	friend bool operator!=(const mat_base &a, const mat_base &b)	{ return !(a == b); }
};

//-------------------------------------
// mat swizzling
//-------------------------------------

template<int O, typename E, typename M, int... I> vec<E, sizeof...(I)> _get_diagonal(const M &m, meta::value_list<int, I...>) {
	return {m[O < 0 ? I : I + O][O < 0 ? I - O : I]...};
}

template<typename E, int N, int M, typename I, int...J> auto _biswizzle(const mat<E, N, M> &m, I i, meta::value_list<int, J...>)	{
	return mat<E, I::count, sizeof...(J)>(swizzle(i, m.template column<J>())...);
}

template<int...I, typename E, int N> auto swizzle(const mat<E, N, N> &m)	{
	return mat<E, sizeof...(I), sizeof...(I)>((swizzle<I...>(m.template column<I>()))...);
}

template<typename E, int N, int M, int...I>	struct swizzled_mat {
	const mat<E, N, M> &m;
	swizzled_mat(const mat<E, N, M> &m) : m(m) {}
	template<int...J>	friend auto rows(const swizzled_mat &m) { return _biswizzle(m.m, meta::value_list<int, J...>(), meta::value_list<int, I...>()); }
	operator mat<E, N, sizeof...(I)>() { return {m.template column<I>()...}; }
};
template<int...I, typename E, int N, int M>	constexpr swizzled_mat<E, N, M, I...> cols(const mat<E, N, M> &m) { return m; }
//template<int...I, typename E, int N, int M>	auto cols(const mat<E, N, M> &m) { return mat<E, N, sizeof...(I)>(m.template column<I>()...); }
template<int...I, typename E, int N, int M>	auto rows(const mat<E, N, M> &m) { return _biswizzle(m, meta::value_list<int, I...>(), meta::make_value_sequence<M>()); }

template<bool NltM> struct get_square_s			{ template<typename E, int N, int M> static mat<E, N, N> f(const mat<E, N, M> &m) { return (mat_base<E, N, N>)m; }};
template<>			struct get_square_s<false>	{ template<typename E, int N, int M> static mat<E, M, M> f(const mat<E, N, M> &m) { return m; }};

//-------------------------------------
// mat
//-------------------------------------

template<typename E, int N, int M> class mat : public mat_base<E, N, M> {
	typedef mat_base<E, N, M> B;
public:
	using B::B;
	mat()	{}
	mat(const mat &b)	: B(b)	{}
	mat(const B &b) 	: B(b)	{}
//	mat &operator=(const mat<E, N, M - 1> &b) { *(B*)this = b; return *this; }

	friend mat	operator-(const mat &a)	{ return a._neg(); }
	friend mat	abs(const mat &a)		{ return a._abs(); }
	mat	operator+(const mat &b)	const	{ return B::_add(b); }
	mat	operator-(const mat &b)	const	{ return B::_sub(b); }
	template<typename T> IF_SCALAR(T,mat) operator*(const T &b)	const	{ return B::_smul(b); }
	template<typename T> IF_SCALAR(T,mat) operator/(const T &b)	const	{ return B::_smul(reciprocal(b)); }

	E				det()			const	{ return get_det(get_square_s<(N < M)>::f(*this)); }
	auto			diagonal()		const	{ return _get_diagonal<0, E>(*this, meta::make_value_sequence<min(N, M)>()); }
	template<int O> auto diagonal()	const	{ return _get_diagonal<O, E>(*this, meta::make_value_sequence<min(N - max(O, 0), M - min(O, 0))>()); }

	friend auto flatten(const mat &m)	{ return m._flat(); }
};

template<typename E, int N, int M> int	max_component_index(const mat<E,N,M> &m)	{ return max_component_index(flatten(m)); }

// square

template<typename E, int N> class mat<E, N, N> : public mat_base<E, N, N> {
	typedef mat_base<E, N, N> B;
	template<typename M, size_t...I> mat(const M &m, index_list<I...>)	: mat(m.template column<I>()...) {}
public:
	using B::B;

	force_inline mat() {}
	constexpr mat(const mat &b)						: B(b) {}
	constexpr mat(const B &b)						: B(b) {}
	force_inline mat(const diagonal<E, N> &m)			: mat(m, meta::make_index_list<N>()) {}
	force_inline mat(const upper<E, N> &m)				: mat(m, meta::make_index_list<N>()) {}
	force_inline mat(const lower<E, N> &m)				: mat(m, meta::make_index_list<N>()) {}
	force_inline mat(const symmetrical<E, N> &m)		: mat(m, meta::make_index_list<N>()) {}
	force_inline mat(const skew_symmetrical<E, N> &m)	: mat(m, meta::make_index_list<N>()) {}

	friend mat	operator-(const mat &a)		{ return a._neg(); }
	friend mat	abs(const mat &a)			{ return a._abs(); }
	mat	operator+(const mat &b)	const 		{ return B::_add(b); }
	mat	operator-(const mat &b)	const 		{ return B::_sub(b); }
	template<typename T> IF_SCALAR(T,mat) operator*(const T &b)	const { return B::_smul(b); }
	template<typename T> IF_SCALAR(T,mat) operator/(const T &b)	const { return B::_smul(reciprocal(b)); }

	vec<E, N>		diagonal()		const	{ return _get_diagonal<0, E>(*this, meta::make_value_sequence<N>()); }
	template<int O> auto diagonal()	const	{ return _get_diagonal<O, E>(*this, meta::make_value_sequence<N - meta::abs_v<O>>()); }
	force_inline E	trace()			const	{ return reduce_add(diagonal()); }
	E				det()			const	{ return get_det(*this); }

	friend auto operator~(const mat &a)	{
		return transpose(cofactors(a));
	}
	friend mat get_inverse_transpose(const mat &m) {
		auto	c = cofactors(m);
		return c * reciprocal(dot(m.x, c.x));
	}
	friend auto get_inverse(const mat &a) {
		return transpose(get_inverse_transpose(a));
	}
	friend auto flatten(const mat &m)	{ return m._flat(); }

	template<int N2, int M2> explicit operator mat<E, N2, M2>() {
		return _biswizzle(*this, meta::make_value_sequence<N2>(), meta::make_value_sequence<M2>());
	}
};

template<typename E> class mat<E, 1, 1> {
public:
	E	x;
	force_inline mat() 		{}
	force_inline mat(E x)	: x(x) {}

	friend mat	operator-(const mat &a)		{ return -a.x; }
	friend mat	abs(const mat &a)			{ return abs(a.x); }
	mat	operator+(const mat &b)	const 		{ return x + b.x; }
	mat	operator-(const mat &b)	const 		{ return x - b.x; }
	template<typename T> IF_SCALAR(T,mat) operator*(const T &b)	const { return x * b; }
	template<typename T> IF_SCALAR(T,mat) operator/(const T &b)	const { return x / b; }

	E&			operator[](int i)			{ return x; }
	const E&	operator[](int i)	const	{ return x; }

	E				diagonal()		const	{ return x; }
	force_inline E	trace()			const	{ return x; }
	E				det()			const	{ return x; }
};

template<typename E, typename M, int...I> 	_triangular<E,sizeof...(I)>	_make_tri(const M &m, meta::value_list<int, I...>) {
	return {m.template diagonal<I>()...};
}

template<typename E, int N> upper<E,N>			get_upper(const mat<E,N,N> &m)	{ return _make_tri<E>(m, meta::make_value_sequence<N>()); }
template<typename E, int N> lower<E,N>			get_lower(const mat<E,N,N> &m)	{ return _make_tri<E>(m, meta::muladd<-1,0,meta::make_value_sequence<N>>()); }
template<typename E, int N> symmetrical<E, N>	make_sym(const mat<E,N,N> &m) 	{ return _make_tri<E>(m, meta::make_value_sequence<N>()); }
//template<typename E, int N> skew_symmetrical<E, N>		make_skew_sym(const mat_base<E,N,N> &m) 	{ return _make_tri<E>(m, meta::make_value_sequence<N>()); }

template<typename M> auto	get_upper(const transpose_s<M> &m) 	{ return transpose(get_lower(m.m)); }
template<typename M> auto	get_lower(const transpose_s<M> &m) 	{ return transpose(get_upper(m.m)); }
template<typename M> auto	make_sym(const transpose_s<M> &m) 	{ return make_sym(m.m); }

//-------------------------------------
// mat loading
//-------------------------------------

template<typename E, int N, int M, typename I, typename=void> struct load_mat_s {
	typedef mat<E, N, M>	T;
	template<int...J> static void	f2(T &t, I p, meta::value_list<int, J...>) 		{ discard((iso::load(t[J], p[J]), 0)...); }
	static void	f(T &t, I p)		{ f2(t, p, meta::make_value_sequence<M>()); }
};
//template<typename E, int N, int M> struct load_mat_s<E, N, M, const E(*)[N], enable_if_t<sizeof(vec<E,N>)==sizeof(E)*N>> {
//	static void	f(mat<E, N, M> &t, const E (*p)[N]) { load((vec<E, N * M>&)t, p[0]); }
//};
template<typename E, int N, int M> struct load_mat_s<E, N, M, E(*)[N]> : load_mat_s<E, N, M, const E(*)[N]> {};

template<typename E, int N, int M, typename I, typename=void> struct store_mat_s {
	typedef mat<E, N, M>	T;
	template<int...J> static void	f2(const T &t, I p, meta::value_list<int, J...>) { discard((iso::store(t[J], p[J]), 0)...); }
	static void	f(const T &t, I p)	{ f2(t, p, meta::make_value_sequence<M>()); }
};
template<typename E, int N, int M> struct store_mat_s<E, N, M, E(*)[N], enable_if_t<sizeof(vec<E,N>)==sizeof(E)*N>> {
	static void	f(const mat<E, N, M> &t, E (*p)[N])	{ store((const vec<E, N * M>&)t, p[0]); }
};
template<typename E, int N, int M> struct store_mat_s<E, N, M, E(*)[N], enable_if_t<sizeof(vec<E,N>)!=sizeof(E)*N>> {
	static void	f(const mat<E, N, M> &t, E (*p)[N])	{ store(flatten(t), p[0]); }
};
template<typename E, int N, int M, typename I>	inline	void	load(mat<E, N, M> &m, I p)			{ load_mat_s<E, N, M, I>::f(m, p); }
template<typename E, int N, int M, typename I>	inline	void	store(const mat<E, N, M> &m, I p)	{ store_mat_s<E, N, M, I>::f(m, p); }

//template<typename E, int N, int M> struct load_s<mat<E, N, M>> {
//	template<typename I> static void	load(mat<E, N, M> &t, I p)			{ load_mat_s<E, N, M, I>::f(t, p); }
//	template<typename I> static void	store(const mat<E, N, M> &t, I p)	{ store_mat_s<E, N, M, I>::f(t, p); }
//};

template<typename T> struct load_transpose_s;

template<typename E, int N, int M> struct load_transpose_s<mat<E,N,M>> {
	typedef mat<E, N, M>	T;
#ifdef __ARM_NEON__
	template<int R, int S, int... J> static void set_rows(T &t, const fullvec<E> *m, meta::value_list<int, J...>) {
		discard(*(vec<E,S>*)((E*)&t[J] + R) = shrink<S>(m[J])...);
	}
	template<int R, typename I> static void load_rows(T &t, I p) {
		static const int NR = meta::min<N - R, fullnum<E>>;
		fullvec<E>	m[M];
		simd::load_rows<E, M, NR>(m, p + R);
		set_rows<R, pow2_ceil<NR>>(t, m, meta::make_value_sequence<M>());
	}
	template<typename I, int...J> static T load_slabs(I p, meta::value_list<int, J...>) {
		T	t;
		discard((load_rows<J * fullnum<E>>(t, p), 0)...);
		return t;
	}
	template<typename I> static T load(I p) {
		return load_slabs(p, meta::make_value_sequence<(N + fullnum<E> - 1) / fullnum<E>>());
	}
#elif defined __SSE__
	template<typename I, int...C, int...R> static T load_cols(I p, meta::value_list<int, C...>, meta::value_list<int, R...>) {
		auto	indices = to<int>((&p[R][0] - &p[0][0])...);
		T	t;
		discard((t[C] = gather(p[0] + C, indices))...);
		return t;
	}
	template<typename I> static T load(I p) {
		return load_cols(p, meta::make_value_sequence<M>(), meta::make_value_sequence<N>());
	}
#else
	template<typename I> static T load(I p) {
		return transpose(load<mat>(p));
	}
#endif
};

template<typename M, typename I> M load_transpose(I p) {
	return load_transpose_s<M>::load(p);
}

template<typename T, typename F, int N>	vec<T, N> to(const mat<F, N, 1> &m) {
	return to<T>(m.x);
}
template<typename T, typename F, int N, int M>	mat<T, N, M> to(const mat<F, N, M> &m) {
	return {to<T>((const mat<F, N, M - 1>&)m), to<T>(m[M - 1])};
}

//-------------------------------------
// general M * V
//-------------------------------------

template<typename E, typename M, size_t... I>	auto _mul(const M &a, vec<E, sizeof...(I)> b, index_list<I...>) {
	return sum((a.column(I) * b[I])...);
}
template<typename E, int N, int M, typename B> 	auto 	operator*(const mat<E, N, M> &a, B b)->enable_if_t<is_vecN<B, M>, vec<E, N>>  {
	return _mul<E>(a, b, meta::make_index_list<M>());
}
template<typename E, int N, int M, typename B> 	auto 	operator*(const mat<E, N, M> &a, B b)->enable_if_t<is_vecN<B, M - 1>, vec<E, N>>  {
	return _mul<E>(a, b, meta::make_index_list<M - 1>());
}
template<typename E, int N> force_inline 		auto	operator*(const mat<E, N + 1, N + 1> &a, homo<E, N> b) {
	return homo<E,N>(a * b.v);
}
template<typename E, int N> force_inline 		auto	operator*(const mat<E, N + 1, N + 1> &a, pos<E, N> b) {
	return homo<E,N>(_mul<E>(a, b, meta::make_index_list<N>()) + a.template column<N>());
}
template<typename E, int N> force_inline 		auto	operator*(const mat<E, N, N + 1> &a, pos<E, N> b) {
	return pos<E,N>(_mul<E>(a, b, meta::make_index_list<N>()) + a.column(N));
}
template<typename E, int N> force_inline 		auto	operator*(const mat<E, N, N> &a, pos<E, N> b) {
	return pos<E,N>(a * b.v);
}
template<typename E, int N, int M, int A>  enable_if_t<(A <= M), vec<E, N>> operator*(const mat<E, N, M> &m, const axis_s<A>&)  {
	return m[A];
}

//-------------------------------------
// general M * M
//-------------------------------------

template<typename E, int N, typename M, int C, int D, size_t... I> auto _mul(const M &a, const mat<E, D, C> &b, index_list<I...>) {
	return mat<E, N, sizeof...(I)>(a * b.column(I)...);
}
template<typename E, int A, int B, int C> mat<E, B, C> operator*(const mat<E, B, A> &a, const mat<E, A, C> &b) {
	return _mul<E, B>(a, b, meta::make_index_list<C>());
}

template<typename E, int A, int B, int C> mat<E, B, C> operator*(const mat<E, B, A+1> &a, const mat<E, A, C> &b) {
	return {_mul<E, B>(a, b, meta::make_index_list<C>()), a[A]};
//	auto r = _mul<E, B>(a, b, meta::make_index_list<C>());
//	r[A] += a[A];
//	return r;
}

// (N,N+1) x (N,N+1)
template<typename E, int N> mat<E, N, N+1> operator*(const mat<E, N, N+1> &a, const mat<E, N, N+1> &b) {
	auto r = _mul<E, N>(a, b, meta::make_index_list<N+1>());
	r[N] += a[N];
	return r;
}
// (N,N+1) x (N,N)
template<typename E, int N> mat<E, N, N+1> operator*(const mat<E, N, N+1> &a, const mat<E, N, N> &b) {
	return {_mul<E, N>(a, b, meta::make_index_list<N>()), a[N]};
}
// (N,N) x (N-1,N)
template<typename E, int N> mat<E, N, N> operator*(const mat<E, N, N> &a, const mat<E, N-1, N> &b) {
	auto r = _mul<E, N>(a, b, meta::make_index_list<N>());
	r[N-1] += a[N-1];
	return r;
}
// (N-1,N) x (N,N) (pretend it's (N,N) x (N,N) to preserve bottom row of b)
template<typename E, int N> mat<E, N, N> operator*(const mat<E, N-1, N> &a, const mat<E, N, N> &b) {
	return mat<E, N, N>(_mul<E, N-1>(a, b, meta::make_index_list<N>()), b.row(N-1));
}
// (N+1,N+1) x (N,N)
template<typename E, int N> mat<E, N+1, N+1> operator*(const mat<E, N+1, N+1> &a, const mat<E, N, N> &b) {
	return {_mul<E, N+1>(a, b, meta::make_index_list<N>()), a[N]};
}
// (N,N) x (N+1,N+1)
template<typename E, int N> mat<E, N+1, N+1> operator*(const mat<E, N, N> &a, const mat<E, N+1, N+1> &b) {
	return mat<E, N+1, N+1>(_mul<E, N>(a, b, meta::make_index_list<N>()), b.row(N));
}

// general Mt * V
template<typename E, typename M, typename V, size_t... I>	vec<E, sizeof...(I)> _transpose_mul(const M &a, V b, index_list<I...>) {
	return {dot(a.column(I), b)...};
}
template<typename E, int N, int M, typename B> force_inline auto	operator*(const transpose_s<mat<E,N,M>&> &a, B b)->enable_if_t<is_vecN<B, N>, vec<E, M>> {
	return _transpose_mul<E>(a.m, b, meta::make_index_list<M>());
}
template<typename E, int N, typename B> force_inline auto			operator*(const transpose_s<mat<E,N,N>&> &a, vec<E,N-1> b)->enable_if_t<is_vecN<B, N-1>, vec<E, N>> {
	return _transpose_mul<E>(a.m, b, meta::make_index_list<N>());
}

template<typename E, int N, int M, typename B> force_inline auto	operator*(const transpose_s<mat<E,N,M>> &a, B b)->enable_if_t<is_vecN<B, N>, vec<E, M>> {
	return _transpose_mul<E>(a.m, b, meta::make_index_list<M>());
}
template<typename E, int N, typename B> force_inline auto			operator*(const transpose_s<mat<E,N,N>> &a, vec<E,N-1> b)->enable_if_t<is_vecN<B, N-1>, vec<E, N>> {
	return _transpose_mul<E>(a.m, b, meta::make_index_list<N>());
}

#ifdef USE_VECDOT
template<typename E, int A, int B, int C> force_inline mat<E,B,C>	operator*(const transpose_s<mat<E,A,B>&> &a, const mat<E,A,C> &b) {
	return _mul<E,B>(a, b, meta::make_index_list<C>());
}
template<typename E, int A, int B, int C> force_inline mat<E,B,C>	operator*(const transpose_s<mat<E,A,B>> &a, const mat<E,A,C> &b) {
	return _mul<E, B>(a, b, meta::make_index_list<C>());
}
#endif

//-------------------------------------
// transpose
//-------------------------------------

template<typename E, int N0, int M0, int N1, int M1, int...I, int...J> force_inline mat<E, N0 + N1, M0 + M1> mat_combine4(
	const mat<E, N0, M0> &a, const mat<E, N1, M0> &b, const mat<E, N0, M1> &c, const mat<E, N1, M1> &d,
	meta::value_list<int, I...>, meta::value_list<int, J...>
) {
	return {concat(a[I], b[I])..., concat(c[J], d[J])...};
}

#if 1
template<int RN, int CN> struct transpose_helper {
	template<typename E, int N, int R0> static force_inline mat<E, CN, RN> f(const vec<E, N> *m) {
		static const int C2 = CN / 2;
		static const int R2 = RN / 2;
		return mat_combine4(
			transpose_helper<R2, 		C2		>::template f<E, N, R0>(m),
			transpose_helper<R2, 		CN - C2	>::template f<E, N, R0>(m + C2),
			transpose_helper<RN - R2, 	C2		>::template f<E, N, R2>(m),
			transpose_helper<RN - R2, 	CN - C2	>::template f<E, N, R2>(m + C2),
			meta::make_value_sequence<R2>(),
			meta::make_value_sequence<RN - R2>()
		);
	}
};
template<> struct transpose_helper<1, 1> {
	template<typename E, int N, int R0> static force_inline mat<E, 1, 1> f(const vec<E, N> *m) { return {m[0][R0]}; }
};

template<int CN> struct transpose_helper<1, CN> {
	template<typename E, int N, int R0, int...I> static force_inline mat<E, CN, 1> f(const vec<E, N> *m, meta::value_list<int, I...>) { return mat<E, CN, 1>(swizzle<I * N + R0...>(m[I]...)); }
	template<typename E, int N, int R0> static force_inline mat<E, CN, 1> f(const vec<E, N> *m) { return f<E,N,R0>(m, meta::make_value_sequence<CN>()); }
};
template<int RN> struct transpose_helper<RN, 1> {
	template<typename E, int N, int R0, int...I> static force_inline mat<E, 1, RN> f(const vec<E, N> *m, meta::value_list<int, I...>) { return mat<E, 1, RN>(swizzle<I + R0...>(m[0])); }
	template<typename E, int N, int R0> static force_inline mat<E, 1, RN> f(const vec<E, N> *m) { return f<E,N,R0>(m, meta::make_value_sequence<RN>()); }
};

template<> struct transpose_helper<2, 2> {
	template<typename E, int N, int R0> static force_inline mat<E,2,2> f(const vec<E, N> *m) { return mat<E,2,2>(swizzle<R0, R0+N, R0+1, R0+N+1>(m[0], m[1])); }
};
template<typename E, int N, int M> force_inline mat<E, M, N> get_transpose(const mat<E, N, M> &m) {
	return transpose_helper<N, M>::template f<E, N, 0>(&m[0]);
}
#else
template<typename E, int N, int M, size_t... I>	force_inline mat<E, M, N> _get_transpose(const mat<E, N, M> &m, index_list<I...>) {
	return {m.row(I)...};
}
template<typename E, int N, int M> mat<E, M, N> force_inline get_transpose(const mat<E, N, M> &m)	{
	return _get_transpose(m, meta::make_index_list<N>());
}
#endif

//-------------------------------------
// A.B.At
//-------------------------------------

template<typename E, int N> symmetrical<E,N>				mul_mulT(const mat<E,N,N> &m, diagonal<E,N> s);
template<typename E, int N> symmetrical<E,N>				mul_mulT(diagonal<E,N> s, const mat<E,N,N> &m)	{ return mul_mulT(m, s); }
//template<typename E, int N, typename M> symmetrical<E,N>	mul_mulT(const M &m, diagonal<E,N> s)			{ return mul_mulT(mat<E,N,N>(m), s); }
template<typename E, int N, typename M> symmetrical<E,N>	mul_mulT(const M &m, const symmetrical<E,N> &s)	{ return make_sym(m * s * transpose(m)); }

template<typename E, int N, size_t... I> mat<E, N, sizeof...(I)> _outer_product(const vec<E, N> &a, const vec<E, sizeof...(I)> &b, index_list<I...>) {
	return {a * b[I]...};
}
template<typename A, typename B>	auto outer_product(const A &a, const B &b) {
	return _outer_product<element_type<A>, num_elements_v<A>>(a, b, meta::make_index_list<num_elements_v<B>>());
}

//-------------------------------------
//	get matrix components
//-------------------------------------

template<typename M, size_t...I>				auto					_sum_cols(const M &a, index_list<I...>)			{ return sum(a[I]...); }
template<typename M, size_t... I>				auto 					_get_scale2(const M &m, index_list<I...>) 		{ return sum(square(m[I])...); }
template<typename E, typename M, size_t... I>	vec<E, sizeof...(I)> 	_get_pre_scale2(const M &m, index_list<I...>) 	{ return {len2(m[I])...}; }

template<typename E, int N, int M>	auto		sum_cols(const mat<E,N,M> &a)		{ return _sum_cols(a, meta::make_index_list<M>()); }
template<typename E, int N, int M>	enable_if_t<(M != N+1), vec<E,N>>	get_scale2(const mat<E,N,M> &m)		{ return _get_scale2(m, meta::make_index_list<M>()); }
template<typename E, int N>			auto		get_scale2(const mat<E,N,N+1> &m)	{ return _get_scale2(m, meta::make_index_list<N>()); }
template<typename E, int N, int M>	auto		get_scale(const mat<E,N,M> &m)		{ return sqrt(get_scale2(m)); }
template<typename E, int N, int M>	auto		get_pre_scale2(const mat<E,N,M> &m)	{ return _get_pre_scale2<E>(m, meta::make_index_list<M>()); }
template<typename E, int N, int M>	auto		get_pre_scale(const mat<E,N,M> &m)	{ return sqrt(get_pre_scale2(m)); }
template<typename E, int N> const pos<E,N>&		get_trans(const mat<E,N,N+1> &m)	{ return reinterpret_cast<const pos<E,N>&>(m.column(N)); }
template<typename E, int N> const mat<E,N,N>&	get_rot(const mat<E,N,N+1> &m)		{ return reinterpret_cast<const mat<E,N,N>&>(m); }
template<typename E, int N> mat<E,N,N>&			get_rot(mat<E,N,N+1> &m)			{ return reinterpret_cast<mat<E,N,N>&>(m); }

template<typename E, int N> auto				get_eigenvector(const mat<E,N,N> &m, E eigenvalue) {
	auto	a	= ~(m - diagonal<E,N>(eigenvalue));
	return a.column(max_component_index(abs(a)));
}

template<typename E, int N>auto					get_eigenvector(const symmetrical<E,N> &m, E eigenvalue) {
	auto	a	= ~(m - diagonal<E,N>(eigenvalue));
	return a.column(max_component_index(abs(a)));
}

template<int X, typename E, int N, int M> mat<E, N, M + X> extend_right_by(const mat<E, N, M>& m) {
	return mat<E, N, M + X>(m);
}
template<int X, typename E, int N, int M> mat<E, N, M + X> extend_left_by(const mat<E, N, M>& m) {
	return mat<E, N, M + X>(m);
}


//-------------------------------------
//	2D	matrices
//-------------------------------------

typedef diagonal<float,2>		diagonal2;
typedef lower<float,2>			lower2;
typedef upper<float,2>			upper2;
typedef symmetrical<float,2>	symmetrical2;
typedef mat<float,2,2>			float2x2;
typedef mat<float,2,3>			float2x3;
typedef mat<float,3,2>			float3x2;

template<typename E> E						get_det(const mat<E,2,2> &m)			{ return diff(m.v().xz * m.v().wy); }
template<typename E> E						get_det(const symmetrical<E,2> &m)		{ return diff(m.o.xz * m.o.yz); }

template<typename E> symmetrical<E,2>		make_sym(const mat<E,2,2> &m)			{ return _triangular<E,2>(m.v().xwz); }
template<typename E> _triangular<E,2>		get_adjoint(const _triangular<E,2> &m)	{ return _triangular<E,2>(-masked.z(m.o.yxz)); }
template<typename E> symmetrical<E,2>		get_adjoint(const symmetrical<E,2> &m)	{ return get_adjoint((const _triangular<E,2>&)m); }
template<typename E> mat<E,2,2>				cofactors(const mat<E,2,2> &m)			{ return (vec<E,4>)-masked.yz(m.v().wzyx); }

template<typename E> mat<E,3,3> cofactors(const mat<E,2,3> &m) {
	mat<E,2,2> c = cofactors(get_rot(m));
	return {
		to<E>(c.x, dot(c.x, -m.z)),
		to<E>(c.y, dot(c.y, -m.z)),
		to<E>(zero, zero, dot(c.x, m.x))
	};
}

template<typename E> symmetrical<E,2>		outer_product(vec<E,2> v)				{ return v.xyx * v.xyy; }

template<typename E> force_inline E				get_euler(const mat<E,2,2> &m)		{ return atan2(m.x.yx); }
template<typename E> force_inline E				get_euler(const mat<E,2,3> &m)		{ return atan2(m.x.yx); }
//template<typename E> force_inline mat<E,2,2>	get_transpose(const mat<E,2,2> &m)	{ return mat<E,2,2>(m.v.xzyw); }

template<typename E> force_inline auto			get_transpose(const mat<E,2,3> &m)	{ return transpose((mat<E,3,3>)m); }
template<typename E> force_inline mat<E,2,3>	get_inverse(const mat<E,2,3> &m)	{ mat<E,2,2> r = get_inverse(get_rot(m)); return {r, r * -m.z}; }

//-------------------------------------
//	3D	matrices
//-------------------------------------

typedef diagonal<float,3>		diagonal3;
typedef lower<float,3>			lower3;
typedef upper<float,3>			upper3;
typedef symmetrical<float,3>	symmetrical3;
typedef mat<float,3,3>			float3x3;
typedef mat<float,3,4>			float3x4;

template<typename E> E					get_det(const mat<E,3,3> &m)			{ return dot(m.x, cross(m.y, m.z)); }
template<typename E> E					get_det(const symmetrical<E,3> &m)		{ return reduce_mul(m.d) + reduce_mul(m.o) * 2 - reduce_add(m.d.zxy * square(m.o)); }

//template<typename E> symmetrical<E,3>	make_sym(const mat<E,3,3> &m)			{ return _triangular<E,3>(m.diagonal(), vec<E,3>{m.y.x, m.z.y, m.z.x}); }
template<typename E> _triangular<E,3>	get_adjoint(const _triangular<E,3> &m)	{ return {m.d.yzx * m.d.zxy, vec<E,3>(zero, zero, m.o.x * m.o.y) - m.o.xyz * m.d.zxy}; }
template<typename E> symmetrical<E,3>	get_adjoint(const symmetrical<E,3> &m)	{ return {m.d.yzx * m.d.zxy - m.o.yzx * m.o.yzx, m.o.yzx * m.o.zxy - m.o.xyz * m.d.zxy}; }

template<typename E> mat<E,3,3> cofactors(const mat<E,3,3> &m) {
	return {
		cross(m.y, m.z),
		cross(m.z, m.x),
		cross(m.x, m.y)
	};
}

/*
template<typename E> force_inline mat<E,3,3> get_transpose(const mat<E,3,3> &m) {
	vec<E,4>	t0 = swizzle<0,2,1,3>(m.x.xy, m.y.xy);
	return {
		concat(t0.xy, m.z.x),
		concat(t0.zw, m.z.y),
		concat(m.x.z, m.y.z, m.z.z)
	};
}
*/
template<typename E> vec<E,3> get_euler(const mat<E,3,3> &m) {
	E	pitch	= asin(m.y.z);
	return abs(m.y.z) <= 0.99f
		? vec<E,3>{pitch, atan2(-m.y.x, m.y.y), atan2(-m.x.z, m.z.z)}	//pitch, roll, yaw
		: vec<E,3>{pitch, atan2( m.x.y, m.x.x), zero};					//pitch, roll, yaw
}


// ZYZ:
//.x: rotation around the Z axis
//.y: rotation around the X axis
//.z: rotation around the new Z axis
template<typename E> vec<E,3> to_zyz(const mat<E,3,3> &m) {
	return	m.z.z == one	? vec<E,3>{atan2(m.x.y, m.x.x), zero, zero}
		:	m.z.z == -one	? vec<E,3>{atan2(-m.x.y, m.x.x), pi, zero}
		:	vec<E,3>{atan2(m.z.y, m.z.x), acos(m.z.z), atan2(m.y.z, -m.x.z)};
}

template<typename E> mat<E,3,3> mat_between(param_t<vec<E,3>> a, param_t<vec<E,3>> b) {
	vec<E,3>	c = cross(a, b);

	if (len2(c) == zero) {
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
		square(m.x) * s.d.x + square(m.y) * s.d.y + square(m.z) * s.d.z,
			m.x * rotate(m.x) * s.d.x
		+	m.y * rotate(m.y) * s.d.y
		+	m.z * rotate(m.z) * s.d.z
	);
}

template<typename E> inline symmetrical<E,3>	outer_product(vec<E,3> v)			{ return symmetrical<E,3>(v * v, v.xyz * v.yzx); }
template<typename E> inline symmetrical<E,3>	skew(vec<E,3> v)					{ return {zero, -v.zxy}; }
template<typename V> inline auto				skew(V v)							{ return skew<element_type<V>>(v); }

template<typename E> mat<E,4,4> cofactors(const mat<E,3,4> &m) {
	mat<E,3,3> c = cofactors(get_rot(m));
	return {
		concat(c.x, dot(c.x, -m.w)),
		concat(c.y, dot(c.y, -m.w)),
		concat(c.z, dot(c.z, -m.w)),
		extend_left<4>(dot(c.x, m.x))
	};
}
template<typename E> force_inline vec<E,3> 		get_euler(const mat<E,3,4> &m) 			{ return get_euler(get_rot(m)); }
template<typename E> force_inline mat<E,3,4>	get_inverse(const mat<E,3,4> &m)		{ auto r = get_inverse(get_rot(m)); return {r, r * -m.w}; }
template<typename E> force_inline auto			transpose(const mat<E,3,4> &m)			{ return transpose((mat<E,4,4>)m); }
template<typename E> force_inline auto			get_transpose(const mat<E,3,4> &m)		{ return transpose((mat<E,4,4>)m); }

template<typename E> force_inline vec<E,3>		operator*(const inverse_s<mat<E,3,4>&> &a, vec<E,3> b)	{ return inverse(get_rot(a.m)) * b; }
template<typename E> force_inline pos<E,3>		operator*(const inverse_s<mat<E,3,4>&> &a, pos<E,3> b)	{ return inverse(get_rot(a.m)) * (b - get_trans(a.m)); }

template<typename E> force_inline void			composite(mat<E,3,4> &m, const mat<E,3,4> &a, const mat<E,3,4> &b)	{ m = a * b; }

//-------------------------------------
//	4D matrices
//-------------------------------------

typedef diagonal<float,4>		diagonal4;
typedef upper<float,4>			upper4;
typedef lower<float,4>			lower4;
typedef symmetrical<float,4>	symmetrical4;
typedef mat<float,4,4>			float4x4;

template<typename E> E	get_det(const mat<E,4,4> &m) {
	vec<E,3>	c0 = cross(m.z.xyz, m.w.xyz);
	vec<E,3>	c1 = m.z.xyz * m.w.w - m.w.xyz * m.z.w;
	vec<E,4>	dx = concat(cross(m.y.xyz, c1) + c0 * m.y.w, -dot(c0, m.y.xyz));
	return dot(m.x, dx);
}

template<typename E> E	get_det(const symmetrical<E,4> &a)	{
	return get_det(mat<E,4,4>(a));
}

//template<typename E> symmetrical<E,4>	make_sym(const mat<E,4,4> &m) {
//	return symmetrical<E,4>(m.diagonal(), vec<E,3>{m.y.x, m.z.y, m.w.z}, vec<E,3>{m.z.x, m.w.y, m.w.x});
//}

template<typename E> _triangular<E, 4>	get_adjoint(const _triangular<E, 4> &m) {
	auto	d3 = static_cast<const _triangular<E, 3>&>(m).d;
	return {
		m.d.yzxx * m.d.zxyy * m.d.wwwz,
		m.d.zxy * m.d.wwx * d3,
		concat(
			(d3.xy * d3.yz - m.d.yz * m.o.xy) * m.d.wx,
			(d3.z * m.o.x - m.d.z * m.o.z) * m.d.y + (m.d.z * m.o.y - d3.y * d3.z) * d3.x
		)
	};
}

template<typename E> symmetrical<E,4>	get_adjoint(const symmetrical<E,4> &m) {
	auto	d3 = m.diagonal<1>().d;//static_cast<const _triangular<E, 3>&>(m).d;
	vec<E,3>	x3 = {m.d.x, d3.x, m.o.x}, y3 = {d3.x, m.d.y, d3.y}, z3 = {m.o.x, d3.y, m.d.z}, w3 = concat(m.o.zy, d3.z);

	vec<E,3>	c0	= cross(z3, w3);
	vec<E,3>	c1	= z3 * m.d.w - w3 * d3.z;
	vec<E,3>	dxy	= to<E>(y3.y, x3.yz) * c1.zzx - to<E>(y3.z, x3.zx) * c1.yyz + c0.xxy * m.o.yzz;

	vec<E,3>	c2	= cross(x3, y3);
	vec<E,3>	c3	= x3 * m.o.y - y3 * m.o.z;
	vec<E,3>	dz	= cross(w3, c3) + c2 * m.d.w;
	vec<E,3>	dw	= cross(c3, z3) - c2 * d3.z;
#if 1
	//d4.y, d3.x wrong
	return _triangular<E,4>(
		to<E>(dxy.x, -dxy.z, dz.z, dot(c2, z3)),
		to<E>(-dxy.y, dz.y, dw.z),
		to<E>(dz.x, dw.y, dw.x)
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

template<typename E> mat<E,4,4> cofactors(const mat<E,4,4> &m) {
	vec<E,3>	c0 = cross(m.z.xyz, m.w.xyz);
	vec<E,3>	c1 = m.z.xyz * m.w.w - m.w.xyz * m.z.w;
	vec<E,4>	dx = concat(cross(m.y.xyz, c1) + c0 * m.y.w, -dot(c0, m.y.xyz));
	vec<E,4>	dy = concat(cross(c1, m.x.xyz) - c0 * m.x.w,  dot(c0, m.x.xyz));

	vec<E,3>	c2 = cross(m.x.xyz, m.y.xyz);
	vec<E,3>	c3 = m.x.xyz * m.y.w - m.y.xyz * m.x.w;
	vec<E,4>	dz = concat(cross(m.w.xyz, c3) + c2 * m.w.w, -dot(c2, m.w.xyz));
	vec<E,4>	dw = concat(cross(c3, m.z.xyz) - c2 * m.z.w,  dot(c2, m.z.xyz));

	return {dx, dy, dz, dw};
}

template<typename E> force_inline symmetrical<E,4> outer_product(vec<E,4> v) {
	return symmetrical<E,4>(v * v, v.xyz * v.yzw, v.xyw * v.zwx);
}
/*
template<typename E> force_inline mat<E,4,4> get_transpose(const mat<E,4,4> &m) {
	vec<E,4>	t0 = swizzle<0,2,1,3>(m.x.xy, m.z.xy);
	vec<E,4>	t1 = swizzle<0,2,1,3>(m.y.xy, m.w.xy);
	vec<E,4>	t2 = swizzle<0,2,1,3>(m.x.zw, m.z.zw);
	vec<E,4>	t3 = swizzle<0,2,1,3>(m.y.zw, m.w.zw);
	return {
		swizzle<0,2,1,3>(t0.xy, t1.xy),
		swizzle<0,2,1,3>(t0.zw, t1.zw),
		swizzle<0,2,1,3>(t2.xy, t3.xy),
		swizzle<0,2,1,3>(t2.zw, t3.zw)
	};
}
*/
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

template<typename E> force_inline void	composite(mat<E,4,4> &m, const mat<E,4,4> &a, const mat<E,4,4> &b)	{ m = a * b; }
template<typename V> force_inline auto	outer_product(V v)		{ return outer_product<element_type<V>>(v); }

//-----------------------------------------------------------------------------
// quaternion
//-----------------------------------------------------------------------------

template<typename E> class quat {
	typedef vec<E,4>	V;


	static V quat_from_mat(const mat<E,3,3> &mat) {
		vec<E,3>	x	= mat.x,  y  = mat.y,  z  = mat.z;

		V	a	= to<E>(one, z.y, x.z, y.x);
		V	b	= swizzle<E,0,1,2,2>(x.x, -y.z, zero);
		V	c	= swizzle<E,0,2,1,2>(y.y, -z.x, zero);
		V	d	= swizzle<E,0,2,2,1>(z.z, -x.y, zero);

		V	q;
		q	= a + b + c + d;
		if (q.x > one)
			return swizzle<1,2,3,0>(q) * (iso::half * rsqrt(q.x));

		q	= a + b - c - d;
		if (q.x > one)
			return swizzle<0,3,2,1>(q) * (iso::half * rsqrt(q.x));

		q	= a - b + c - d;
		if (q.x > one)
			return swizzle<3,0,1,2>(q) * (iso::half * rsqrt(q.x));

		q	= a - b - c + d;
		return swizzle<2,1,0,3>(q) * (iso::half * rsqrt(q.x));
	}
public:
	V	v;
	force_inline quat() 							{}
	force_inline quat(const _identity&)	: v(w_axis)	{}
	force_inline quat(V a)				: v(a)		{}
	template<typename P0, typename P1, typename...P>	force_inline quat(const P0 &p0, const P1 &p1, const P&...p)	: v(to<E>(p0, p1, p...))	{}
	force_inline quat(const float3x3 &mat)		: v(quat_from_mat(mat))	{}

	// equivalent to *this = b * *this
	force_inline quat&	operator*=(param_t<quat> b)		{ v = concat(b.v.xyz * v.w + v.xyz * b.v.w - cross(b.v.xyz, v.xyz), v.w * b.v.w - dot(v.xyz, b.v.xyz)); return *this; }

	force_inline quat	closest(param_t<quat> q) const	{ return v * sign1(dot(v, q.v)); }

	static quat	between(param_t<vec<E,3>> a, param_t<vec<E,3>> b) {
		auto	half	= a + b;
		auto	w		= dot(a, half);
		return abs(w) < 1e-4f ? V(x_axis) : V(normalise(concat(cross(half, a), w)));
	}
	static quat	from_euler(param_t<vec<E,3>> v);

	operator		mat<E,3,3>()	const;
	quat			operator-()		const						{ return -v; }
	quat			operator~()		const						{ return quat(-v.xyz, v.w); }
	template<typename B> force_inline IF_SCALAR(B,quat) operator*(const B &b)	{ return v * b; }

	auto			operator==(quat b) const					{ return v == b.v; }
	auto			operator!=(quat b) const					{ return v != b.v; }

	friend V		operator+(param_t<quat> a, param_t<quat> b)	{ return a.v + b.v; }
	friend V		operator-(param_t<quat> a, param_t<quat> b)	{ return a.v - b.v; }
	friend quat		normalise(param_t<quat> a)					{ return normalise(a.v); }
	friend E		norm2(param_t<quat> a)						{ return dot(a.v, a.v); }
	friend E		norm(param_t<quat> a)						{ return sqrt(norm2(a)); }
	friend E		cosang(param_t<quat> a, param_t<quat> b)	{ return dot(a.v, b.v); }
//	friend quat		inverse(param_t<quat> a)					{ return ~a; }
	friend quat		inverse(quat a)								{ return ~a; }

	template<typename Y> friend quat	pow(param_t<quat> q, const Y &y) {
		auto	t	= len(q.v.xyz);
		auto	s	= sin(asin(t) * y);
		return quat(q.v.xyz * (t ? s / t : 1), sin_cos(s));
	}
	template<typename Y> friend quat	exp(param_t<quat> q) {
		auto		t	= len(q.v.xyz);
		vec<E,2>	sc	= sincos(t);
		return V(q.v.xyz * sc.y / t, sc.x) * exp(q.v.w);
	}

};
typedef quat<float>	quaternion;

template<typename E> quat<E>::operator mat<E,3,3>() const {
	vec<E,4>	v2	= v + v;
	vec<E,3>	d	= v2.xyz * v.xyz;
	vec<E,3>	t	= v2.xyz * v.yzx;
	vec<E,3>	u	= v2.xyz * v.w;
	vec<E,3>	a	= t.xyz - u.zxy;
	vec<E,3>	b	= t.zxy + u.yzx;

	d	= vec<E,3>(one) - d.yzx - d.zxy;
	return {
		float3{d.x, a.x, b.x},
		float3{b.y, d.y, a.y},
		float3{a.z, b.z, d.z}
	};
}

template<typename E> force_inline quat<E>		operator*(quat<E> a, quat<E> b)				{ return quat<E>(b.v.xyz * a.v.w + a.v.xyz * b.v.w - cross(a.v.xyz, b.v.xyz), a.v.w * b.v.w - dot(a.v.xyz, b.v.xyz)); }
template<typename E> force_inline vec<E,3>		operator*(quat<E> a, vec<E,3> b)			{ return b + twice(cross(a.v.xyz, cross(a.v.xyz, b) - b * a.v.w)); }
template<typename E> force_inline pos<E,3>		operator*(quat<E> a, pos<E,3> b)			{ return b + twice(cross(a.v.xyz, cross(a.v.xyz, b.v) - b.v * a.v.w)); }
template<typename E> force_inline mat<E,3,3>	operator*(quat<E> a, const mat<E,3,3> &b)	{ return a.operator mat<E,3,3>() * b; }
template<typename E> force_inline mat<E,3,3>	operator*(const mat<E,3,3> &a, quat<E> b)	{ return a * b.operator mat<E,3,3>(); }
template<typename E> force_inline mat<E,3,4>	operator*(quat<E> a, const mat<E,3,4> &b)	{ return a.operator mat<E,3,3>() * b; }
template<typename E> force_inline mat<E,3,4>	operator*(const mat<E,3,4> &a, quat<E> b)	{ return a * b.operator mat<E,3,3>(); }
template<typename E> force_inline mat<E,4,4>	operator*(quat<E> a, const mat<E,4,4> &b)	{ return a.operator mat<E,3,3>() * b; }
template<typename E> force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, quat<E> b)	{ return a * b.operator mat<E,3,3>(); }
template<typename E> force_inline quat<E>		operator/(quat<E> a, quat<E> b)				{ return ~b * a; }

template<typename E> quat<E> quat<E>::from_euler(param_t<vec<E,3>> v) {
	vec<E,3> s, c;
	sincos(v * -half, &s, &c);
	return	quat<E>(zero, s.y, zero, c.y)
		*	quat<E>(s.x, zero, zero, c.x)
		*	quat<E>(zero, zero, s.z, c.z);
}

template<typename E> force_inline quat<E>	lerp_check(quat<E> a, quat<E> b, float t) {
	return lerp(a, b.closest(a), t);
}

template<typename E, typename T> quat<E>	slerp(quat<E> a, quat<E> b, T t) {
	E			cosom = cosang(a, b);
	vec<E,2>	scale{one - t, sign1(cosom) * t};
	if (abs(cosom) < 0.99f) {
		scale	= sin(scale * acos(cosom)) * rsqrt(one - square(cosom));
		return quat<E>(a.v * scale.x + b.v * scale.y);
	}
	return quat<E>(normalise(a.v * scale.x + b.v * scale.y));
}

template<typename E, typename T> quat<E>	squad(quat<E> q0, quat<E> a, quat<E> b, quat<E> q1, T t) {
	return slerp(slerp(q0, q1, t), slerp(a, b, t), 2 * (1 - t) * t);
}

template<typename TA, typename E> bool diagonalise_step(const TA &A, quat<E> &q) {
	mat<E,3,3>	Q	= q;
	auto	D		= transpose(Q) * A * Q;			// A = Q^T*D*Q
	auto	od		= to<E>(D.z.y, D.z.x, D.y.x);	// elements not on the diagonal
	int		k		= max_component_index(abs(od)); // find k - the index of largest element of offdiag
	auto	x		= od[k];
	if (x == zero)
		return true; // diagonal already

	int		k1		= (k + 1) % 3;
	int		k2		= (k + 2) % 3;
	auto	theta	= (D[k2][k2] - D[k1][k1]) * half / x;
	// let t = 1 / (|T|+sqrt(T^2+1)) but avoid numerical overflow
	auto	t		= abs(theta);
	t				= reciprocal(t + ((t < 1.e6f) ? sqrt(square(t) + one) : t));
	auto	c		= rsqrt(square(t) + one); // c= 1/(t^2+1) , t=s/c
	if (c == one)
		return true; // no room for improvement - reached machine precision.

	// using 1/2 angle identities: sin(a/2) = sqrt((1-cos(a))/2), cos(a/2) = sqrt((1-cos(a))/2)
	auto	cs = sqrt(to<E>(one + c, one - c) * half);
	q = q * quat<E>(rotate(concat(copysign(cs.y, theta), vec<E,2>(zero)), k), cs.x);
	return false;
}
template<typename E> quaternion diagonalise(const mat<E,3,3> &A) {
	quat<E> q	= identity;
	for (int i = 0; i < 24 && !diagonalise_step(A, q); i++);
	return q;
}
template<typename E> quaternion diagonalise(const symmetrical<E,3> &A) {
	quat<E> q	= identity;
	for (int i = 0; i < 24 && !diagonalise_step(A, q); i++);
	return q;
}

//-----------------------------------------------------------------------------
// rotations
//-----------------------------------------------------------------------------

template<typename E> E angle_between(vec<E,2> a, vec<E,2> b) {
	return atan2(cross(a, b), dot(a, b));
}

template<typename E> auto sincos_twice(vec<E,2> sc) {
	return (sc.xx + sc.yx) * (sc.xy - concat(sc.y, zero));
}

template<typename E> auto sincos_half(vec<E,2> sc) {
	auto h = sqrt(plus_minus<E>(half) * (plus_minus<E>(one) + sc.x));
	h.y = copysign(h.y, sc.y);
	return h;
}

// returns point on unit circle where |M.v| is largest
template<typename E> vec<E,2> max_circle_point(const mat<E, 2, 2> &m) {
	if (auto d = dot(m.x, m.y)) {
		auto	t		= square(m.v());
		auto	sc2		= normalise(concat(t.x + t.y - t.z - t.w, dot(m.x, m.y) * two));
		return sincos_half<E>(clamp(sc2, -one, one));
	}
	return {1, 0};
}

force_inline float2x2 _rotate2D(float2 sc) {
	return float2x2(swizzle<0,1,3,0>(sc, -sc));
}
template<typename T> force_inline float2x2 rotate2D(const T &t) {
	return _rotate2D(to<float>(sincos(t)));
}

inline float2x2 quadrant_rotate(int q) {
	float4	v = q & 1 ? float4{0, 1, -1, 0} : float4{1, 0, 0, 1};
	return q & 2 ? -v : v;
}

// holds rotation as axis * sin(theta)
template<typename E> struct rotvec {
	vec<E,3> v;
	rotvec()								{}
	explicit rotvec(vec<E,3> v)	: v(v)		{}
	rotvec(const _identity&)	: v(zero)	{}
	rotvec(quat<E> q)			: v(q.v.xyz * q.v.w * two) {}
	rotvec(const mat<E,3,3> &m);

	operator mat<E,3,3>() const;
	operator quat<E>() const {
		return quat<E>(normalise(to<E>(v, two)));
	}
	static inline rotvec between(param_t<vec<E,3>> a, param_t<vec<E,3>> b) {
		return cross(normalise(b), normalise(a));
	}
};
typedef rotvec<float>	rotation_vector;

template<typename E> rotvec<E>::rotvec(const mat<E,3,3> &m) {
	auto		cosang	= (m.trace() - 1) * half;
	vec<E,3>	r		= vec<E,3>{m.z.y - m.y.z, m.x.z - m.z.x, m.y.x - m.x.y} * half;

	if (abs(cosang) > rsqrt2) {
		v = r;

	} else {
		vec<E,3>	d	= m.diagonal() - cosang;
		vec<E,3>	t	= (vec<E,3>{m.x.y, m.y.z, m.z.x} + vec<E,3>{m.y.x, m.z.y, m.x.z}) * half;
		vec<E,3>	r2;
		switch (max_component_index(d * d)) {
			case 0:		r2 = {d.x, t.x, t.z};
			case 1:		r2 = {t.x, d.y, t.y};
			default:	r2 = {t.z, t.y, d.z};
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

	return {
		vec<E,3>(d.x, b.x - a.z, b.z + a.y),
		vec<E,3>(b.x + a.z, d.y, b.y - a.x),
		vec<E,3>(b.z - a.y, b.y + a.x, d.z)
	};
}

// holds rotation as axis * theta, which corresponds to log(rotvec)
// (e.g. adding is equivalent to multiplying rotations, etc)
template<typename E> struct logrot {
	vec<E,3> v;
	logrot()							{}
	logrot(const _zero&) : v(zero)		{}
	explicit logrot(vec<E,3> v) : v(v)	{}

	friend quaternion	exp(const logrot &x) {
		auto		t	= len(x.v);
		vec<E,2>	sc	= sincos(t);
		return quat<E>(t > sc.y ? x.v * sc.y / t : x.v, sc.x);
	}
	friend logrot operator-(const logrot &x)							{ return logrot(-x.v); }
	friend logrot operator+(const logrot &x, const logrot &y)			{ return logrot(x.v + y.v); }
	friend logrot operator-(const logrot &x, const logrot &y)			{ return logrot(x.v - y.v); }
	template<typename Y> friend logrot operator*(const logrot &x, Y y)	{ return logrot(x.v * y); }
	friend bool operator==(const logrot &x, const logrot &y)			{ return all(x.v == y.v); }
	friend bool operator!=(const logrot &x, const logrot &y)			{ return !(x == y); }
};
typedef logrot<float>	log_rotation;

template<typename E> logrot<E> log(const rotvec<E> &r) {
//	auto	s	= len(r.v);
//	return s + one > one ? r.v * (asin(s) / s * half) : r.v;
	auto	s	= len(r.v);
	return logrot<E>(s ? r.v * iso::atan2(s, two) / s : r.v);
//	return log(r.operator quat<E>());
}
template<typename E> logrot<E> log(quat<E> q) {
	auto	s	= len(q.xyz);
	return s ? logrot<E>(q.xyz * atan2(s, q.w) / s) : zero;
}
template<typename E> logrot<E> log(const mat<E,3,3> &m) {
	auto		cosang	= (m.trace() - 1) * half;
	vec<E,3>	r		= float3(m.z.y - m.y.z, m.x.z - m.z.x, m.y.x - m.x.y) * half;
	auto		sinang	= len(r);

	if (cosang > rsqrt2)
		return sinang > zero ? logrot<E>(r * (asin(sinang) / sinang * half)) : zero;

	if (cosang > -rsqrt2)
		return logrot<E>(r * (acos(cosang) / sinang * half));

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

	return logrot<E>(normalise(r2) * (angle * half));
}

template<typename E> quat<E> squadseg(const quat<E> *qb, quat<E> q0, quat<E> q1, const quat<E> *qa, E t) {
	auto a = qb ? q0 * exp(logrot<E>((q1 * log(q0).v + *qb * log(q0).v) * .25f)) : q0;
	auto b = qa ? q1 * exp(logrot<E>((*qa * log(q1).v + q0 * log(q1).v) * .25f)) : q1;
	return squad(q0, a, b, q1, t);
}

force_inline symmetrical3 rotation_pi(param_t<float3> v) {
	float3	w	= normalise(v) * sqrt2;
	float3	w2	= w * w;
	return symmetrical3(float3(one) - (w2.yzx + w2.zxy), w.xyz * w.yzx);
}

template<int N> struct axis_mat_s;
template<>	struct axis_mat_s<0> { template<typename E, typename T> static force_inline	mat<E,3,3> f(const T &sc) { return {x_axis, swizzle<E,0,1,2>(zero, sc), to<E>(zero, perp(sc))}; } };
template<>	struct axis_mat_s<1> { template<typename E, typename T> static force_inline	mat<E,3,3> f(const T &sc) { return {swizzle<E,2,0,1>(zero, perp(sc)), y_axis, swizzle<E,2,0,1>(zero, sc)}; } };
template<>	struct axis_mat_s<2> { template<typename E, typename T> static force_inline	mat<E,3,3> f(const T &sc) { return {swizzle<E,1,2,0>(zero, sc), to<E>(perp(sc), zero), z_axis}; } };

template<int A, typename T> struct rotate_in_axis {
	const T	&t;
	force_inline rotate_in_axis(const T &t) : t(t)	{}
	template<typename E> force_inline operator	quat<E>()		const	{ return (vec<E,4>)swizzle<E,A==0?2:0, A==1?2:0, A==2?2:0, 1>(zero, sincos(t * -half)); }
	template<typename E> force_inline operator	mat<E, 3, 3>()	const	{ return axis_mat_s<A>::template f<E>(sincos(t)); }
	template<typename E> force_inline operator	rotvec<E>()		const	{ return rotvec<E>(swizzle<E, A==0, A==1, A==2>(zero, sin(t))); }
	friend	log_rotation		log(const rotate_in_axis &a)			{ return log_rotation(swizzle<float, A==0, A==1, A==2>(zero,  a.t * -half)); }
	friend rotate_in_axis<A,T>	get_inverse(const rotate_in_axis &m)	{ return -m.t; }
};

template<typename T> force_inline rotate_in_axis<0,T>	rotate_in_x(const T &t) { return t; }
template<typename T> force_inline rotate_in_axis<1,T>	rotate_in_y(const T &t) { return t; }
template<typename T> force_inline rotate_in_axis<2,T>	rotate_in_z(const T &t) { return t; }

template<int A, typename T> force_inline rotate_in_axis<A,T>	rotate_axis(axis_s<A>, const T &t)				{ return t; }
template<typename A, typename T> force_inline auto				rotate_axis(const negated<A> &a, const T &t)	{ return rotate_axis(a.t, -t); }
template<typename T>		force_inline quaternion				_rotate_axis(param_t<float3> axis, const T &sc)	{ return quaternion(axis * sc.y, sc.x); }
//template<typename T>		force_inline quaternion				rotate_axis(param_t<float3> axis, const T &t)	{ return _rotate_axis(axis, simd::sincos(t * -half)); }

force_inline								quaternion		operator*(const rotation_vector &a, const rotation_vector &b)	{ return a.operator quaternion() * b.operator quaternion(); }
force_inline								quaternion		operator*(param_t<quaternion> a, const rotation_vector &b)		{ return a * b.operator quaternion(); }
force_inline								quaternion		operator*(const rotation_vector &a, param_t<quaternion> b)		{ return a.operator quaternion() * b; }
template<int A, typename T>	force_inline	quaternion		operator*(const rotate_in_axis<A,T> &a, const rotation_vector &b)	{ return a * b.operator quaternion(); }
template<int A, typename T> force_inline	quaternion		operator*(const rotation_vector &a, const rotate_in_axis<A,T> &b)	{ return a.operator quaternion() * b; }

template<int A, typename T1, typename T2>			force_inline	rotate_in_axis<A,T1>	operator*(const rotate_in_axis<A,T1> &a, const rotate_in_axis<A,T2> &b)		{ return a.t + b.t; }
template<int A1, int A2, typename T1, typename T2>	force_inline	quaternion				operator*(const rotate_in_axis<A1,T1> &a, const rotate_in_axis<A2,T2> &b)	{ return a.operator quaternion() * b.operator quaternion(); }
template<typename E, int A, typename T> force_inline vec<E,3>		operator*(const rotate_in_axis<A,T> &a, vec<E,3> b)				{ return axis_s<A>::rot(b, sincos(a.t)); }
template<typename E, int A, typename T> force_inline pos<E,3>		operator*(const rotate_in_axis<A,T> &a, pos<E,3> b)				{ return axis_s<A>::rot(b, sincos(a.t)); }
template<typename E, int A, typename T> force_inline vec<E,4>		operator*(const rotate_in_axis<A,T> &a, vec<E,4> b)				{ return axis_s<A>::rot(b, sincos(a.t)); }
template<typename E, int A, typename T> force_inline quat<E>		operator*(const rotate_in_axis<A,T> &a, quat<E> b)				{ return a.operator quat<E>() * b; }
template<typename E, int A, typename T> force_inline quat<E>		operator*(quat<E> a, const rotate_in_axis<A,T> &b)				{ return a * b.operator quat<E>(); }
template<typename E, int A, typename T> force_inline mat<E,3,3>		operator*(const rotate_in_axis<A,T> &a, const mat<E,3,3> &b)	{ return (mat<E,3,3>)a * b; }
template<typename E, int A, typename T> force_inline mat<E,3,3>		operator*(const mat<E,3,3> &a, const rotate_in_axis<A,T> &b)	{ return a * (mat<E,3,3>)b; }
template<typename E, int A, typename T> force_inline mat<E,3,4>		operator*(const rotate_in_axis<A,T> &a, const mat<E,3,4> &b)	{ return (mat<E,3,3>)a * b; }
template<typename E, int A, typename T> force_inline mat<E,3,4>		operator*(const mat<E,3,4> &a, const rotate_in_axis<A,T> &b)	{ return a * (mat<E,3,3>)b; }
template<typename E, int A, typename T> force_inline mat<E,4,4>		operator*(const rotate_in_axis<A,T> &a, const float4x4 &b)		{ return (mat<E,3,3>)a * b; }
template<typename E, int A, typename T> force_inline mat<E,4,4>		operator*(const mat<E,4,4> &a, const rotate_in_axis<A,T> &b)	{ return a * (mat<E,3,3>)b; }

// rotation about a base axis (for matrix solving)
template<typename E> struct axes_rotatation {
	vec<E, 2>	cs;
	int			i, j;

	static vec<E, 3> givens(E a, E b) {
		if (b == zero)
			return {sign1(a), zero, abs(a)};

		if (a == zero)
			return {zero, sign1(b), abs(b)};

		if (abs(a) > abs(b)) {
			E	t = b / a;
			E	u = copysign(a, rsqrt(square(t) + one));
			return {u, u * t, a / u};
		} else {
			E	t = a / b;
			E	u = copysign(b, rsqrt(square(t) + one));
			return {u * t, u, b / u};
		}
	}
		
	static vec<E,2> schur(E d0, E f0, E d1) {
		if (d0 == 0 || f0 == 0)
			return {1, 0};
		E	tau = (square(f0) + (d1 + d0) * (d1 - d0)) / (2 * d0 * f0);
		E	t	= sign(tau) / (abs(tau) + sqrt(1 + square(tau)));
		E	c1	= rsqrt(1 + square(t));
		return {c1, c1 * t};
	}

	static axes_rotatation givens(E a, E b, int i, int j)			{ return {givens(a, b).xy, i, j}; }
	static axes_rotatation schur(E d0, E f0, E d1, int i, int j)	{ return {schur(d0, f0, d1), i, j}; }
	
	axes_rotatation(int i, int j, vec<E,2> cs)	: i(i), j(j), cs(cs) {}

	template<typename V> enable_if_t<!is_mat<V>, void>	 apply(V& v) const {
		auto	a = v[i], b = v[j];
		v[i] = a * cs.x - b * cs.y;
		v[j] = a * cs.x + b * cs.y;
	}
	template<typename M> enable_if_t<is_mat<M>, void>	apply(M& m) const {
		for (auto &i : m.columns())
			apply(i);
	}
	template<typename M> enable_if_t<is_mat<M>, void>	post_apply(M& m) const {
		auto	a = m[i], b = m[j];
		m[i] = a * cs.x - b * cs.y;
		m[j] = a * cs.x + b * cs.y;
	}
};

// reflection about a base plane (for matrix solving)
template<typename V> struct plane_reflection {
	V		v;

	static plane_reflection householder(const V &v) { return normalise(v) * sign1(v[0]); }

	plane_reflection(const V &v) : v(v) {}

	template<typename V2> enable_if_t<!is_mat<V2>, void>	apply(V2& w) const {
		w -= v * dot(v, w);
	}
	template<typename M> enable_if_t<is_mat<M>, void>	apply(M& m) const {
		for (auto &i : m.columns())
			apply(i);
	}
	template<typename M> enable_if_t<is_mat<M>, void>	post_apply(M& m) const {
		for (auto &i : m.rows())
			apply(i);
	}
};

//-----------------------------------------------------------------------------
// scales
//-----------------------------------------------------------------------------

template<typename T> struct scale_s {
	const T	t;
	explicit force_inline	scale_s(const T &t) : t(t)	{}
	template<typename E, int N> force_inline	operator diagonal<E,N>()	const { return diagonal<E,N>(t); }
	template<typename E> force_inline	operator mat<E,2,3>()	const { return mat<E,2,3>((mat<E,2,2>)operator diagonal<E,2>()); }
	template<typename E> force_inline	operator mat<E,3,4>()	const { return mat<E,3,4>((mat<E,3,3>)operator diagonal<E,3>()); }
	friend const scale_s&				transpose(const scale_s &m)				{ return m; }
	friend scale_s&&					transpose(scale_s &&m)					{ return (scale_s&&)m; }
	friend const inverse_s<scale_s>&	transpose(const inverse_s<scale_s> &m)	{ return m; }
	friend inverse_s<scale_s>&&			transpose(inverse_s<scale_s> &&m)		{ return move(m); }
	friend auto							cofactors(const scale_s &m)				{ return inverse(m); }
	friend auto							get_inverse(const scale_s &m)			{ return scale_s(reciprocal(m.t)); }
};

template<typename T> static constexpr bool is_mat<scale_s<T>> = true;

template<typename T>							force_inline scale_s<T>			scale(const T &t)							{ return scale_s<T>(t); }
template<typename A, typename B>				force_inline scale_s<float2>	scale(const A &a, const B &b)				{ return scale(float2{a, b}); }
template<typename A, typename B, typename C>	force_inline scale_s<float3>	scale(const A &a, const B &b, const C &c)	{ return scale(float3{a, b, c}); }


template<typename T1, typename T2>						force_inline scale_s<T1>	operator*(const scale_s<T1> &a, const scale_s<T2> &b)			{ return a.t * b.t; }
template<typename T1, int A, typename T2>				force_inline float3x3		operator*(const scale_s<T1> &a, const rotate_in_axis<A,T2> &b)	{ return a * (float3x3)b; }
template<typename E, int A, typename T1, typename T2>	force_inline float3x3		operator*(const rotate_in_axis<A,T2> &a, const scale_s<T1> &b)	{ return (float3x3)a * b; }

template<typename E, int N, typename T> force_inline vec<E,N> operator*(const scale_s<T> &a, vec<E,N> b)			{ return vec<E,N>(a.t) * b; }
template<typename E, int N, typename T> force_inline pos<E,N> operator*(const scale_s<T> &a, pos<E,N> b)			{ return pos<E,N>(vec<E,N>(a.t) * b.v); }
template<typename E, typename T> force_inline vec<E,4>		operator*(const scale_s<T> &a, vec<E,4> b)				{ return concat(vec<E,3>(a.t), one) * b; }

template<typename E, int N, typename T> force_inline auto	operator*(const scale_s<T> &a, const mat<E,N,N> &b)		{ return a.operator diagonal<E, N>() * b; }
template<typename E, int N, typename T> force_inline auto	operator*(const mat<E,N,N> &a, const scale_s<T> &b)		{ return a * b.operator diagonal<E, N>(); }

template<typename E, int N, typename T> force_inline auto	operator*(const scale_s<T> &a, const mat<E,N,N+1> &b)	{ auto d = a.operator diagonal<E, N>(); return mat<E,N,N+1>(d * get_rot(b), d * get_trans(b)); }
template<typename E, int N, typename T> force_inline auto	operator*(const mat<E,N,N+1> &a, const scale_s<T> &b)	{ return mat<E,N,N+1>(get_rot(a) * b, get_trans(a)); }
template<typename E, typename T> force_inline mat<E,3,3>	operator*(const scale_s<T> &a, quat<E> b)				{ return a * b.operator mat<E,3,3>(); }
template<typename E, typename T> force_inline mat<E,3,3>	operator*(quat<E> a, const scale_s<T> &b)				{ return a.operator mat<E,3,3>() * b; }
template<typename E, int N, typename T> force_inline auto	operator*(const scale_s<T> &a, const symmetrical<E,N> &b)	{ return a.operator diagonal<E,N>() * b; }

//-----------------------------------------------------------------------------
// translations
//-----------------------------------------------------------------------------

template<typename E, int N> struct translate_s {
	const pos<E, N>	t;
	explicit force_inline	translate_s(const vec<E, N> &t) : t(t)	{}
	force_inline	operator mat<E, N, N + 1>() const		{ return {identity, t.v}; }
	friend translate_s	get_inverse(const translate_s &m)	{ return translate_s(-m.t); }
	friend auto			get_transpose(const translate_s &m)	{ return mat<E,N+1,N+1>(mat<E,N+1,N>(mat<E,N,N>(identity), m.t.v)); }
	friend auto			cofactors(const translate_s &m)		{ return transpose(inverse(m)); }
};

typedef translate_s<float, 2> translate2;
typedef translate_s<float, 3> translate3;

template<typename E, int N> static constexpr bool is_mat<translate_s<E, N>> = true;

template<typename T>							force_inline auto	translate(const T &t)							{ return translate_s<element_type<T>, num_elements_v<T>>(t); }
template<typename E, int N>						force_inline auto	translate(pos<E, N> a)							{ return translate_s<E, N>(a); }
template<typename A, typename B>				force_inline auto	translate(const A &a, const B &b)				{ return translate(position2(a, b)); }
template<typename A, typename B, typename C>	force_inline auto	translate(const A &a, const B &b, const C &c)	{ return translate(position3(a, b, c)); }

template<typename E, int N, int M, typename K>	force_inline	vec<E,N>	vmul(const mat<E,N,M> &a, const constant<K> &b)			{ return sum_cols(a) * b; }
template<typename E, int N, typename B>			force_inline	vec<E,N>	vmul(const mat<E,N,N> &a, const B &b)					{ return a * vec<E,N>(b); }

template<typename E, int N>				force_inline auto		operator*(const translate_s<E, N> &a, const translate_s<E, N> &b)	{ return translate<E, N>(a.t + b.t); }
template<typename E, int N>				force_inline pos<E,N>	operator*(const translate_s<E, N> &a, param_t<pos<E,N>> b)			{ return b + a.t; }
template<typename E, int N>				force_inline vec<E,N+1>	operator*(const translate_s<E, N> &a, param_t<vec<E,N+1>> b)		{ return b + concat(a.t * b[N], zero); }

template<typename E, int N, typename B> force_inline auto		operator*(const transpose_s<translate_s<E, N>&> &a, const B &b)		{ return transpose(transpose(b) * a.m); }
template<typename E, int N>				force_inline auto		operator*(const translate_s<E, N> &a, const mat<E,N,N> &b)			{ return mat<E, N, N + 1>(b, a.t.v); }
template<typename E, int N>				force_inline auto		operator*(const mat<E,N,N> &a, const translate_s<E, N> &b)			{ return mat<E, N, N + 1>(a, vmul(a, b.t)); }
template<typename E, int N>				force_inline auto		operator*(const translate_s<E, N> &a, const mat<E,N,N+1> &b)		{ return mat<E, N, N + 1>(get_rot(b), b[N] + a.t.v); }
template<typename E, int N>				force_inline auto		operator*(const mat<E,N,N+1> &a, const translate_s<E, N> &b)		{ return mat<E, N, N + 1>(get_rot(a), a[N] + vmul(get_rot(a), b.t)); }
//template<typename E, int N>			force_inline auto		operator*(const translate_s<E, N> &a, const mat<E,N+1,N+1> &b)		{ auto t = concat(a.t.v, zero); return mat<E,N+1,N+1>(b.x + t * b.x[N], b.y + t * b.y[N], b.z + t * b.z[N], b.w + t * b.w[N]); }
//template<typename E, int N>			force_inline auto		operator*(const mat<E,N+1,N+1> &a, const translate_s<E, N> &b)		{ return mat<E, N + 1, N + 1>(a.x, a.y, a.x * b.t.v.x + a.y * b.t.v.y + a.z); }
template<typename E, int N>				force_inline auto		operator*(const symmetrical<E,N> &a, const translate_s<E,N-1> &b)	{ return mat<E, N, N>(a) * b; }
template<typename E, int N>				force_inline auto		operator*(const translate_s<E,N-1> &b, const symmetrical<E,N> &a)	{ return b * mat<E, N, N>(a); }
template<typename E, int N, typename S>	force_inline auto		operator*(const translate_s<E,N> &t, const scale_s<S> &s)			{ return t * mat<E,N,N>(s.operator diagonal<E,N>()); }
template<typename E, int N, typename S>	force_inline auto		operator*(const scale_s<S> &s, const translate_s<E,N> &t)			{ return mat<E,N,N>(s.operator diagonal<E,N>()) * t; }

template<typename E>					force_inline mat<E,3,3>	operator*(const translate_s<E, 2> &a, const mat<E,3,3> &b)			{ vec<E,3> t = concat(a.t.v, zero); return mat<E,3,3>(b.x + t * b.x.z, b.y + t * b.y.z, b.z + t * b.z.z); }
template<typename E>					force_inline mat<E,3,3>	operator*(const mat<E,3,3> &a, const translate_s<E, 2> &b)			{ vec<E,2> t = b.t.v; return mat<E,3,3>(a.x, a.y, a.x * t.x + a.y * t.y + a.z); }
template<typename E>					force_inline mat<E,4,4>	operator*(const translate_s<E, 3> &a, const mat<E,4,4> &b)			{ vec<E,4> t = concat(a.t.v, zero); return mat<E,4,4>(b.x + t * b.x.w, b.y + t * b.y.w, b.z + t * b.z.w, b.w + t * b.w.w); }
template<typename E>					force_inline mat<E,4,4>	operator*(const mat<E,4,4> &a, const translate_s<E, 3> &b)			{ vec<E,3> t = b.t.v; return mat<E,4,4>(a.x, a.y, a.z, a.x * t.x + a.y * t.y + a.z * t.z + a.w); }

template<typename E>					force_inline mat<E,3,4>	operator*(const translate_s<E, 3> &a, quat<E> b)					{ return a * (mat<E,3,3>)b; }
template<typename E>					force_inline mat<E,3,4>	operator*(quat<E> a, const translate_s<E, 3> &b)					{ return (mat<E,3,3>)a * b; }
template<typename E, int A, typename T> force_inline auto		operator*(const translate_s<E, 3> &a, const rotate_in_axis<A,T> &b)	{ return a * (float3x3)b; }
template<typename E, int A, typename T> force_inline auto		operator*(const rotate_in_axis<A,T> &a, const translate_s<E, 3> &b)	{ return (float3x4)a * b; }

//-----------------------------------------------------------------------------
// rot_trans, scale_rot_trans
// combined transformations
//-----------------------------------------------------------------------------

struct rot_trans;
struct scale1_rot_trans;
struct scale_rot_trans;

template<> static constexpr bool is_mat<rot_trans>			= true;
template<> static constexpr bool is_mat<scale1_rot_trans>	= true;
template<> static constexpr bool is_mat<scale_rot_trans>	= true;

struct rot_trans : aligner<16> {
	quaternion	rot;
	float4		trans4;

	rot_trans() {}
	rot_trans(const _one&) : rot(identity), trans4(zero) {}
	rot_trans(param(quaternion) rot, param(float4) trans4) : rot(rot), trans4(trans4) {}
	rot_trans(param(quaternion) rot, param(position3) trans4) : rot(rot), trans4((float4)trans4) {}
	rot_trans(param_t<float3x4> m) : rot(iso::get_rot(m)), trans4(iso::get_trans(m)) {}

	void				reset() {
		rot		= identity;
		trans4	= float4(zero);
	}

	position3			get_trans()										const	{ return position3(trans4.xyz); }
	quaternion			get_rot()										const	{ return rot; }
	void				set_trans(position3 t)									{ trans4.xyz = t.v; }
	void				set_rot(quaternion q)									{ rot = q; }

	operator			float3x4()										const	{ return translate(get_trans()) * rot; }
	rot_trans&			operator=(param_t<float3x4> m)							{ trans4.xyz = iso::get_trans(m).v; rot = iso::get_rot(m); return *this; }
	float3x4			operator*(const float3x4 &m)					const	{ return operator float3x4() * m; }
	friend float3x4		operator*(const float3x4 &m, const rot_trans &x)		{ return m * x.operator float3x4(); }

	rot_trans			operator*(translate3 t)							const	{ return {rot, *this * t.t}; }
	friend rot_trans 	operator*(param_t<translate3> t, const rot_trans &x)	{ return {x.rot, x.trans4 + extend_right<4>(t.t.v)}; }
	rot_trans&			operator*=(translate3 t)								{ trans4.xyz += t.t.v; return *this; }	// *this = t * *this

	rot_trans			operator*(param_t<quaternion> q)				const	{ return {normalise(rot * q), trans4}; }
	friend rot_trans	operator*(param_t<quaternion> q, const rot_trans &x)	{ return {normalise(q * x.rot), q * x.get_trans()}; }

	rot_trans			operator*(const rot_trans &b)					const	{ return {normalise(rot * b.rot), *this * b.get_trans()}; }
	rot_trans			operator/(const rot_trans &b)					const	{ return {rot / b.rot, get_trans() / b}; }

	template<typename T> rot_trans&	operator*=(const T &b)						{ return *this = b * *this; }

	friend float3		operator*(const rot_trans &x, float3 p)					{ return x.rot * p; }
	friend position3	operator*(const rot_trans &x, position3 p)				{ return position3(x.rot * (float3)p + x.trans4.xyz); }

	friend rot_trans	get_inverse(const rot_trans &x)	{
		auto rot = ~x.rot;
		return {rot, rot * -x.get_trans()};
	}
	friend bool			approx_equal(const rot_trans &a, const rot_trans &b, float tol = ISO_TOLERANCE) {
		return approx_equal((float3)a.trans4.xyz, (float3)b.trans4.xyz, tol) && approx_equal(abs(cosang(a.rot, b.rot)), 1, tol);
	}
	friend rot_trans	rotate_around_to(const rot_trans &rt, param(position3) centre, param(quaternion) rot) {
		return rot_trans(rot, rt * centre - rot * float3(centre));
	}
	friend rot_trans	rotate_around(const rot_trans &rt, param(position3) centre, param(quaternion) rot) {
		return rotate_around_to(rt, centre, normalise(rot * rt.rot));
	}
	friend rot_trans	lerp(const rot_trans &a, const rot_trans &b, float rate) {
		return rot_trans(slerp(a.rot, b.rot, rate),  iso::lerp(a.trans4, b.trans4, rate));
	}
};

// with uniform scale
struct scale1_rot_trans : rot_trans {
	scale1_rot_trans() {}
	scale1_rot_trans(const _one&) : rot_trans(identity, w_axis) {}
	scale1_rot_trans(float scale, param(quaternion) rot, param(position3) trans) : rot_trans(rot, concat(trans.v, scale)) {}

	void				reset() {
		rot		= identity;
		trans4	= w_axis;
	}

	float				get_scale()									const			{ return trans4.w; }
	void				set_scale(float s)											{ trans4.w = s; }

	operator			float3x4()									const			{ return rot_trans::operator float3x4() * get_scale(); }
	float3x4			operator*(const float3x4 &m)				const			{ return operator float3x4() * m; }
	friend float3x4		operator*(const float3x4 &m, const scale1_rot_trans &x)		{ return m * x.operator float3x4(); }

	auto				operator*(const translate3 &t)				const			{ return scale1_rot_trans(get_scale(), rot, *this * t.t); }
	friend auto			operator*(const translate3 &t, const scale1_rot_trans &x)	{ return scale1_rot_trans(x.get_scale(), x.rot, x.get_trans() + t.t.v); }
	auto&				operator*=(const translate3 &t)								{ rot_trans::operator*=(t); return *this; }

	friend auto			operator*(param_t<quaternion> q, const scale1_rot_trans &x)	{ return scale1_rot_trans(x.get_scale(), normalise(q * x.rot), q * x.get_trans()); }

	scale1_rot_trans	operator*(const scale1_rot_trans &b)		const			{ return {get_scale() * b.get_scale(), normalise(rot * b.rot), *this * b.get_trans()}; }
	scale1_rot_trans	operator/(const scale1_rot_trans &b)		const			{ return {get_scale() / b.get_scale(), rot / b.rot, get_trans() / b}; }

	template<typename T> scale1_rot_trans&	operator*=(const T &b)					{ return *this = b * *this; }

	friend float3		operator*(const scale1_rot_trans &x, param_t<float3> p)		{ return x.rot * (x.get_scale() * p); }
	friend position3	operator*(const scale1_rot_trans &x, param_t<position3> p)	{ return position3(x * (float3)p + x.trans4.xyz); }

	friend auto			get_inverse(const scale1_rot_trans &x)	{
		auto	scale	= one / x.get_scale();
		auto	rot		= ~x.rot;
		return scale1_rot_trans(scale, rot, (rot * -x.get_trans()) * scale);
	}

	friend bool			approx_equal(const scale1_rot_trans &a, const scale1_rot_trans &b, float tol = ISO_TOLERANCE) {
		return approx_equal((const rot_trans&)a, (const rot_trans&)b) && approx_equal(a.get_scale(), b.get_scale(), tol);
	}
};

// with non-uniform scale
struct scale_rot_trans : rot_trans {
	float3 	scale;

	static quaternion mirror_quat(param(quaternion) rot, param(float3) scale) {
		return rot.v * sign1(concat(scale, reduce_mul(scale)));
	}

	scale_rot_trans() {}
	scale_rot_trans(const _one&) : rot_trans(identity), scale(one) {}
	scale_rot_trans(param(float3) scale, param(quaternion) rot, param(float4) trans4) : rot_trans(rot, trans4), scale(scale) {}
	scale_rot_trans(param(float3) scale, param(quaternion) rot, param(position3) trans4) : rot_trans(rot, trans4), scale(scale) {}
	scale_rot_trans(param_t<float3x4> m) : scale(get_pre_scale(iso::get_rot(m))) {
		if (m.det() < zero)
			scale = -scale;
		rot_trans::operator=(m * iso::scale(one / scale));
	}

	void				reset() {
		rot_trans::reset();
		scale	= float3(one);
	}

	float3				get_scale()									const			{ return scale; }
	void				set_scale(float3 s)											{ scale = s; }

	operator			float3x4()									const			{ return rot_trans::operator float3x4() * iso::scale(scale); }
	auto&				operator=(param_t<float3x4> m) {
		scale	= get_pre_scale(iso::get_rot(m));
		if (m.det() < zero)
			scale = -scale;
		rot_trans::operator=(m * iso::scale(one / scale));
		return *this;
	}
	float3x4			operator*(const float3x4 &m)				const			{ return operator float3x4() * m; }
	friend float3x4		operator*(const float3x4 &m, const scale_rot_trans &x)		{ return m * x.operator float3x4(); }

	auto				operator*(const translate3 &t)				const			{ return scale_rot_trans(scale, rot, *this * position3(t.t)); }
	friend auto			operator*(const translate3 &t, const scale_rot_trans &x)	{ return scale_rot_trans(x.scale, x.rot, x.trans4 + extend_right<4>(t.t.v)); }
	auto&				operator*=(const translate3 &t)								{ rot_trans::operator*=(t); return *this; }

	friend auto			operator*(param_t<quaternion> q, const scale_rot_trans &x)	{ return scale_rot_trans(x.scale, normalise(q * x.rot), q * x.get_trans()); }

	template<typename T> auto	operator*(const scale_s<T> &t)		const			{ return scale_rot_trans(scale * t.t, rot, trans4); }
	
	// not quite same as float3x4
	scale_rot_trans		operator*(const scale_rot_trans &b)			const {
		auto	scale2	= scale * b.scale;
		return scale_rot_trans(scale2, normalise(rot * mirror_quat(b.rot, scale2)), *this * b.get_trans());
	}
	scale_rot_trans		operator/(const scale_rot_trans &b)			const {
		auto	scale2	= scale / b.scale;
		return scale_rot_trans(scale2, mirror_quat(rot, scale2) / b.rot, get_trans() / b);
	}

	template<typename T> scale_rot_trans&	operator*=(const T &b)					{ return *this = b * *this; }

	friend float3		operator*(const scale_rot_trans &x, param_t<float3> p)		{ return x.rot * (x.scale * p); }
	friend position3	operator*(const scale_rot_trans &x, param_t<position3> p)	{ return position3(x * (float3)p + x.trans4.xyz); }

	friend auto			get_inverse(const scale_rot_trans &x)	{
		return get_inverse((const rot_trans&)x) / iso::scale(x.scale);
	}
	friend bool			approx_equal(const scale_rot_trans &a, const scale_rot_trans &b, float tol = ISO_TOLERANCE) {
		return approx_equal((const rot_trans&)a, (const rot_trans&)b) && approx_equal(a.scale, b.scale, tol);
	}
};

//-----------------------------------------------------------------------------
// dual_quaternion
//-----------------------------------------------------------------------------

struct dual_quaternion {
	quaternion	r, t;

	static inline quaternion mul(param_t<float3> v, param_t<quaternion> q) {
		return quaternion(v * q.v.w - cross(v, q.v.xyz), -dot(v, q.v.xyz));
	}

	dual_quaternion() {}
	dual_quaternion(param_t<quaternion> r, param_t<quaternion> t) : r(r), t(t)										{}
	dual_quaternion(param_t<float3> v)							: r(identity), t(to<float>(v * iso::half, zero))	{}
	dual_quaternion(param_t<quaternion> q)						: r(q), t(identity)									{}
	dual_quaternion(param_t<float3> v, param_t<quaternion> q)	: r(q), t(mul(v * (float3)iso::half, q))			{}
	dual_quaternion(param_t<float3x4> t)						: r(get_rot(t)), t(mul(get_trans(t).v * (float3)iso::half, r))	{}

	inline position3	translation()	const	{ return position3((t * ~r).v.xyz * (float3)two); }
	inline operator		float3x4()		const	{ return translate(translation()) * r; }

	friend dual_quaternion	normalise(param_t<dual_quaternion> d)	{
		auto		rn	= norm(d.r);
		quaternion	r	= d.r.v * rn;
		quaternion	t	= d.t.v * rn;
		return dual_quaternion(r, t.v - r.v * cosang(r, t));
	}

	friend dual_quaternion	operator~(param_t<dual_quaternion> d)	{
		return dual_quaternion(~d.r, ~d.t);
	}
	friend dual_quaternion	operator*(param_t<dual_quaternion> a, param_t<dual_quaternion> b) {
		return dual_quaternion(a.r * b.r, a.r * b.t + a.t * b.r);
	}
	friend float3 operator*(param_t<dual_quaternion> d, param_t<float3> v) {
		return d.r * v;
	}
	friend position3 operator*(param_t<dual_quaternion> d, param_t<position3> v) {
		return position3(d.r * float3(v) + (d.t * ~d.r).v.xyz * (float3)two);
	}
};

//-----------------------------------------------------------------------------
//	Matrix creators
//-----------------------------------------------------------------------------

template<typename E> mat<E,2,3> fov_matrix(vec<E,4> fov) {
	return translate(position2((fov.zw - fov.xy) * half)) * scale((fov.xy + fov.zw) * half);
}

template<typename E> mat<E,4,4> perspective_projection_offset(vec<E,4> p, E nearz, E farz) {
	return {
		swizzle<E,1,0,0,0>(zero, p.x),
		swizzle<E,0,1,0,0>(zero, p.y),
		to<E>(p.zw, (farz + nearz) / (farz - nearz), one),
		swizzle<E,0,0,1,0>(zero, -2 * farz * nearz / (farz - nearz))
	};
}
template<typename E> mat<E,4,4> perspective_projection_offset(vec<E,4> p, E nearz) {
	return {
		swizzle<E,1,0,0,0>(zero, p.x),
		swizzle<E,0,1,0,0>(zero, p.y),
		to<E>(p.zw, vec<E,2>(one)),
		swizzle<E,0,0,1,0>(zero, -2 * nearz)
	};
}

template<typename E> mat<E,4,4> perspective_projection(vec<E,2> sxy, E nearz, E farz)					{ return perspective_projection_offset(to<E>(sxy, vec<E,2>(zero)), nearz, farz); }
template<typename E> mat<E,4,4> perspective_projection(vec<E,2> sxy, E nearz)							{ return perspective_projection_offset(to<E>(sxy, vec<E,2>(zero)), nearz); }
template<typename E> mat<E,4,4> perspective_projection(E sx, E sy, E nearz, E farz)						{ return perspective_projection(vec<E,2>{sx, sy}, nearz, farz);}
template<typename E> mat<E,4,4> perspective_projection(E sx, E sy, E nearz)								{ return perspective_projection(vec<E,2>{sx, sy}, nearz);}
template<typename E> mat<E,4,4> perspective_projection_angle(E theta, E aspect, E nearz, E farz)		{ return perspective_projection(vec<E,2>(1, aspect) / tan(theta * half), nearz, farz);}
template<typename E> mat<E,4,4> perspective_projection_angle(E theta, E aspect, E nearz)				{ return perspective_projection(vec<E,2>(1, aspect) / tan(theta * half), nearz);}
template<typename E> mat<E,4,4> perspective_projection_offset(E sx, E sy, E ox, E oy, E nearz, E farz)	{ return perspective_projection_offset({sx, sy, ox, oy}, nearz, farz);}
template<typename E> mat<E,4,4> perspective_projection_offset(E sx, E sy, E ox, E oy, E nearz)			{ return perspective_projection_offset({sx, sy, ox, oy}, nearz);}

template<typename E> mat<E,4,4> perspective_projection_rect(E left, E right, E bottom, E top, E nearz, E farz) {
	return perspective_projection_offset(
		vec<E,4>(2 * nearz,		2 * nearz,		right + left,	top + bottom)
	/	vec<E,4>(right - left,	top - bottom,	left - right,	bottom - top),
		nearz, farz
	);
}
template<typename E> mat<E,4,4> perspective_projection_rect(E left, E right, E bottom, E top, E nearz) {
	return perspective_projection_offset(
		vec<E,4>(2 * nearz,		2 * nearz,		right + left,	top + bottom)
	/	vec<E,4>(right - left,	top - bottom,	left - right,	bottom - top),
		nearz
	);
}
template<typename E> mat<E,4,4> perspective_projection_fov(E left, E right, E bottom, E top, E nearz, E farz) {
	return perspective_projection_offset(
		vec<E,4>(2,				2,				left - right,	bottom - top)
	/	vec<E,4>(right + left,	top + bottom,	left + right,	bottom + top),
		nearz, farz
	);
}
template<typename E> mat<E,4,4> perspective_projection_fov(E left, E right, E bottom, E top, E nearz) {
	return perspective_projection_offset(
		vec<E,4>(2,				2,				left - right,	bottom - top)
	/	vec<E,4>(right + left,	top + bottom,	left + right,	bottom + top),
		nearz
	);
}
template<typename E> mat<E,4,4> perspective_projection_fov(vec<E,4> fov, E nearz, E farz) {
	return perspective_projection_offset(to<E>(2, 2, fov.xy - fov.zw) / (fov.xy + fov.zw).xyxy, nearz, farz);
}
template<typename E> mat<E,4,4> perspective_projection_fov(vec<E,4> fov, E nearz) { 
	return perspective_projection_offset(to<E>(2, 2, fov.xy - fov.zw) / (fov.xy + fov.zw).xyxy, nearz);
}

template<typename E> mat<E,4,4> parallel_projection(E sx, E sy, E nearz, E farz) {
	return {
		swizzle<E, 1,0,0,0>(zero, sx),
		swizzle<E, 0,1,0,0>(zero, sy),
		swizzle<E, 0,0,1,0>(zero, 2 / (farz - nearz)),
		swizzle<E, 0,0,1,2>(zero, -(farz + nearz) / (farz - nearz), one)
	};
}
template<typename E> mat<E,4,4> set_perspective_z(mat<E,4,4> proj, E new_nearz, E new_farz) {
	return parallel_projection(E(1), E(1), project(vec<E,4>(proj.z * new_nearz + proj.w)).v.z, project(vec<E,4>(proj.z * new_farz  + proj.w)).v.z) * proj;
}
template<typename E> mat<E,4,4>	parallel_projection_rect(vec<E,3> xyz0, vec<E,3> xyz1) {
	vec<E,3>	d	= xyz1 - xyz0;
	vec<E,4>	r2	= concat(2 / d, zero);
	if (d.z == zero) {
		r2.z	= -one;
		return mat<E,4,4>(r2.xwww, r2.wyww, r2.wwww, concat((xyz0 + xyz1) * r2.xyz * -half, one));
	}
	return mat<E,4,4>(r2.xwww, r2.wyww, r2.wwzw, concat((xyz0 + xyz1) * r2.xyz * -half, one));
}

template<typename E> mat<E,4,4> parallel_projection_rect(vec<E,2> xy0, vec<E,2> xy1, E z0 = -one, E z1 = one) {
	return parallel_projection_rect<E>(concat(xy0, z0), concat(xy1, z1));
}
template<typename E> mat<E,4,4> parallel_projection_rect(E x0, E x1, E y0, E y1, E z0, E z1) {
	return parallel_projection_rect<E>(vec<E,3>{x0, y0, z0}, vec<E,3>{x1, y1, z1});
}
template<typename E> mat<E,4,4> parallel_projection_fov(vec<E,4> fov, E scale, E z0 = -one, E z1 = one) {
	return parallel_projection_rect<E>(concat(fov.xy * -scale, z0), concat(fov.zw * scale, z1));
}
template<typename E> mat<E,4,4> projection_set_farz(mat<E,4,4> m, E z) {
	return	translate(vec<E,3>(zero, zero, -one))
		*	scale(vec<E,3>(one, one, (z + 1) / 2))
		*	m
		*	translate(vec<E,3>(zero, zero, one));
}

template<typename E> E calc_projected_z(mat<E,4,4> m, E z) {
	mat<E,4,4>	mt	= transpose(m);
	vec<E,4>	zr	= mt.z;
	E		a	= len(zr.xyz);
	E		b	= m.w.z - a * m.w.w;
	return (z * a + b) / z;
}

template<typename E> mat<E,3,4> stereo_skew(E offset, E focus, E shift = E(0)) {
	mat<E,3,4>	skew	= identity;
	skew.z.x = offset / focus;
	skew.w.x = shift - offset;
	return skew;
}

// [ Duff et al. 2017, "Building an Orthonormal Basis, Revisited" ]
template<typename V> auto make_basis(V z) {
	auto s = copysign(1.f, z.z);// >= 0 ? 1 : -1;
	auto a = -1 / (s + z.z);
	auto b = z.x * z.y * a;

	return mat<element_type<V>,3,3>(
		{1 + s * a * square(z.x), s * b, -s * z.x},
		{b, s + a * square(z.y), -z.y},
		z
	);
}

template<typename V> auto look_along_x(V dir) {
	auto	x = normalise(dir), y = normalise(perp(x));
	return mat<element_type<V>,3,3>(x, y, cross(x, y));
}
template<typename V> auto look_along_y(V dir) {
	auto	y = normalise(dir), z = normalise(perp(y));
	return mat<element_type<V>,3,3>(cross(y, z), y, z);
}
template<typename V> auto look_along_z(V dir) {
	auto	z = normalise(dir), x = normalise(perp(z));
	return mat<element_type<V>,3,3>(x, cross(z, x), z);
}

template<typename V> auto look_along_z_scaled(V z, element_type<V> scalexy) {
	auto	rz	= rsqrt(len2(z));
	auto	x	= perp(z) * rz * scalexy;
	return mat<element_type<V>,3,3>(x, cross(z, x) * rz, z);
}

template<typename E> mat<E,3,4> look_at(const mat<E,3,4> &at, vec<E,3> pos) {
	auto	t = at.translation();
	auto	d = normalise(pos - t);
	if (abs(dot(d, at.y)) < 0.98f) {
		auto	x = normalise(cross(at.y, d));
		return mat<E,3,4>(x, cross(d, x), d, t);
	} else {
		auto	y = normalise(cross(d, at.x));
		return mat<E,3,4>(cross(y, d), y, d, t);
	}
}

template<typename E> mat<E,4,4> find_projection(vec<E,4> in[5], vec<E,4> out[5]) {
	mat<E,4,4>	matC(out[0], out[1], out[2], out[3]);
	mat<E,4,4>	matP( in[0],  in[1],  in[2],  in[3]);

	mat<E,4,4>	invC = inverse(matC);
	mat<E,4,4>	invP = inverse(matP);
	auto		v	= (invC * out[4]) / (invP * in[4]);

	matC.x *= v.x;
	matC.y *= v.y;
	matC.z *= v.z;
	matC.w *= v.w;
	return matC * invP;
}

template<typename E> mat<E,3,3> find_projection(vec<E,3> in[4], vec<E,3> out[4]) {
	mat<E,3,3>	matC(out[0], out[1], out[2]);
	mat<E,3,3>	matP( in[0],  in[1],  in[2]);

	mat<E,3,3>	invC = inverse(matC);
	mat<E,3,3>	invP = inverse(matP);
	auto		v	= (invC * out[3]) / (invP * in[3]);

	matC.x *= v.x;
	matC.y *= v.y;
	matC.z *= v.z;
	return matC * invP;
}

//-----------------------------------------------------------------------------
//	Matrix decomposition
//-----------------------------------------------------------------------------

template<typename V, size_t...I> force_inline V _getQ1(V a, const V *Q, meta::index_list<I...>) {
	V	t = a;
	discard{(t -= project_unit(a, Q[I]))...};
	return normalise(t);
}

template<typename V, size_t...I> force_inline void _getQ(const V *A, V *Q, meta::index_list<I...>) {
	discard{Q[I] = _getQ1(A[I], &Q[0], meta::make_index_list<I>())...};
}

//template<typename E, int N> no_inline void QR(const mat<E,N,N> &A, mat<E,N,N> &Q, mat<E,N,N> &R) {
//	_getQ(&A[0], &Q[0], meta::make_index_list<N>());
//	R = transpose(Q) * A;
//}
//template<typename E, int N> no_inline void QR(const mat<E,N,N> &A, mat<E,N,N> &Q, upper<E,N> &R) {
//	mat<E,N, N> R2;
//	QR(&A[0], &Q[0], R2);
//	R = get_upper(R2);
//}

template<typename E, int N, int M> no_inline void QR(const mat<E,N,M> &A, mat<E,N,N> &Q, mat<E,M,M> &R) {
	_getQ(&A[0], &Q[0], meta::make_index_list<M>());
	R = transpose((const mat<E,N,M>&)Q) * A;
}
template<typename E, int N, int M> no_inline void QR(const mat<E,N,M> &A, mat<E,N,N> &Q, upper<E,M> &R) {
	mat<E,M, M> R2;
	QR(A, Q, R2);
	R = get_upper(R2);
}


template<typename E, int N, int M> no_inline void LQ(const mat<E,M,N> &A, mat<E,M,M> &L, mat<E,N,N> &Q) {
	mat<E,N,M> AT	= transpose(A);
	_getQ(&AT[0], &Q[0], meta::make_index_list<N>());
	L = A * Q;
	Q = transpose(Q);
}
template<typename E, int N, int M> no_inline void LQ(const mat<E,M,N> &A, lower<E,M> &L, mat<E,N,N> &Q) {
	mat<E,M,M> L2;
	LQ(A, L2, Q);
	L = get_lower(L2);
}

template<typename T> bool thomas_algorithm(const float* a, const float* b, const float* c, T* d, size_t num) {
	temp_array<float>	cc(num);

	// Forward sweep
	ISO_ASSERT(abs(b[0]) > abs(c[0]));
	cc[0] = c[0] / b[0];
	d[0] /= b[0];

	for (int i = 1; i < num; i++) {
		ISO_ASSERT(abs(b[i]) > abs(a[i]) + abs(c[i]));
		float	m = 1 / (b[i] - a[i] * cc[i - 1]);
		cc[i]	= c[i] * m;
		d[i]	= (d[i] - a[i] * d[i - 1]) * m;
	}

	// Back substitution
	for (int i = num - 1; i > 0; i--)
		d[i - 1] -= cc[i - 1] * d[i];

	return true;
}

//-----------------------------------------------------------------------------
//	simd namespace
//-----------------------------------------------------------------------------

namespace simd {

	template<typename B, int N, typename V, typename P> constexpr bool is_vec<soft_vec<B, N, V, P>>		= true;
	template<typename T, typename P, int...I>			constexpr bool is_vec<soft_perm<T, P, I...>>	= true;

	template<typename B, int N, typename V, typename P> struct element_type_s<soft_vec<B, N, V, P>>		{ typedef P type; };
	template<typename T, typename P, int...I>			struct element_type_s<soft_perm<T, P, I...>>	{ typedef P type; };

//	template<typename E, int N> struct element_type_s<pos<E, N>>	{ typedef E type; };
}

}//namespace iso


#endif	// VECTOR_H
