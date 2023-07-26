#if !defined SIMD_CLANG_H
#define SIMD_CLANG_H

#include "base/defs.h"
#include "base/constants.h"

#if defined __ARM_NEON__
# include <arm_neon.h>
#elif defined __i386__ || defined __x86_64__
# include <immintrin.h>
#endif

//#define SIMD_NODEBUG __attribute__((__nodebug__))
#define SIMD_FUNC  inline __attribute__((__const__))

namespace iso { namespace simd {

#ifdef USE_FP16
typedef __fp16 hfloat;
#endif

static const int min_register_bits = 128;
#ifdef __AVX512F__
static const int max_register_bits = 512;
#elif defined __AVX2__
static const int max_register_bits = 256;
#else
static const int max_register_bits = 128;
#endif

template<typename T, int N>	static constexpr bool is_undersized = (N & (N-1)) != 0 || sizeof(T) * N * 8 < min_register_bits;
template<typename T, int N>	static constexpr bool is_oversized  = (N & (N-1)) == 0 && sizeof(T) * N * 8 > max_register_bits;

template<typename T, int N, typename U=void>	using undersized_t		= enable_if_t< is_undersized<T, N>,	U>;
template<typename T, int N, typename U=void>	using oversized_t		= enable_if_t< is_oversized<T,  N>,	U>;
template<typename T, int N, typename U=void>	using not_undersized_t	= enable_if_t<!is_undersized<T, N>,	U>;
template<typename T, int N, typename U=void>	using not_oversized_t	= enable_if_t<!is_oversized<T,  N>,	U>;
template<typename T, int N, typename U=void>	using normalsized_t		= enable_if_t<!is_oversized<T,  N> && !is_undersized<T,  N>,	U>;

template<int S>	struct T_bool_type		: T_sint_type<S> {};
template<>		struct T_bool_type<1>	: T_type<char> {};
template<typename T>	using bool_for_t = typename T_bool_type<sizeof(T)>::type;

//-----------------------------------------------------------------------------
// supported vector types
//-----------------------------------------------------------------------------

#ifdef USE_FP16
template<typename T> static constexpr bool is_vec_element = iso::is_builtin_num<T> || iso::same_v<T, hfloat>;
#else
template<typename T> static constexpr bool is_vec_element = iso::is_builtin_num<T>;
#endif

template<typename T, int N, size_t A, bool B = is_vec_element<T>> struct get_type {
	typedef __attribute__((__ext_vector_type__(N),__aligned__(A))) T type;
//	typedef __attribute__((__vector_size__(N * sizeof(T)))) T type;
};

template<typename T, int N, size_t A> struct get_type<T, N, A, false>	{};
//template<typename T, int N, size_t A> struct get_type<T, N, A, false> { typedef void type; };

template<typename T, size_t A> struct get_type<T, 1, A, true> { typedef T	type; };

template<typename T, int N> using vec 			= typename get_type<T, N, min(pow2_ceil<sizeof(T) * N>, max_register_bits / 8)>::type;
template<typename T, int N> using packed_simd 	= typename get_type<T, N, sizeof(T)>::type;

typedef vec<int8, 2>	int8x2;
typedef vec<int8, 3>	int8x3;
typedef vec<int8, 4>	int8x4;
typedef vec<int8, 8>	int8x8;
typedef vec<int8, 16>	int8x16;
typedef vec<int8, 32>	int8x32;
typedef vec<int8, 64>	int8x64;
typedef vec<uint8, 2>	uint8x2;
typedef vec<uint8, 3>	uint8x3;
typedef vec<uint8, 4>	uint8x4;
typedef vec<uint8, 8>	uint8x8;
typedef vec<uint8, 16>	uint8x16;
typedef vec<uint8, 32>	uint8x32;
typedef vec<uint8, 64>	uint8x64;
typedef vec<int16, 2>	int16x2;
typedef vec<int16, 3>	int16x3;
typedef vec<int16, 4>	int16x4;
typedef vec<int16, 8>	int16x8;
typedef vec<int16, 16>	int16x16;
typedef vec<int16, 32>	int16x32;
typedef vec<uint16, 2>	uint16x2;
typedef vec<uint16, 3>	uint16x3;
typedef vec<uint16, 4>	uint16x4;
typedef vec<uint16, 8>	uint16x8;
typedef vec<uint16, 16>	uint16x16;
typedef vec<uint16, 32>	uint16x32;
typedef vec<int32, 2>	int32x2;
typedef vec<int32, 3>	int32x3;
typedef vec<int32, 4>	int32x4;
typedef vec<int32, 8>	int32x8;
typedef vec<int32, 16>	int32x16;
typedef vec<uint32, 2>	uint32x2;
typedef vec<uint32, 3>	uint32x3;
typedef vec<uint32, 4>	uint32x4;
typedef vec<uint32, 8>	uint32x8;
typedef vec<uint32, 16>	uint32x16;
typedef vec<int64, 2>	int64x2;
typedef vec<int64, 3>	int64x3;
typedef vec<int64, 4>	int64x4;
typedef vec<int64, 8>	int64x8;
typedef vec<uint64, 2>	uint64x2;
typedef vec<uint64, 3>	uint64x3;
typedef vec<uint64, 4>	uint64x4;
typedef vec<uint64, 8>	uint64x8;
typedef vec<float, 2>	float2;
typedef vec<float, 3>	float3;
typedef vec<float, 4>	float4;
typedef vec<float, 8>	float8;
typedef vec<float, 16>	floatx16;
typedef vec<double, 2>	double2;
typedef vec<double, 3>	double3;
typedef vec<double, 4>	double4;
typedef vec<double, 8>	double8;

#ifdef USE_FP16
typedef vec<hfloat, 2>	hfloat2;
typedef vec<hfloat, 3>	hfloat3;
typedef vec<hfloat, 4>	hfloat4;
typedef vec<hfloat, 8>	hfloat8;
typedef vec<hfloat, 16>	hfloat16;
typedef vec<hfloat, 32>	hfloat32;
#endif

//-----------------------------------------------------------------------------
// meta: extract info about a vector
//-----------------------------------------------------------------------------

template<typename V, bool can_index> struct element_type_s1	{ typedef iso::noref_t<decltype(iso::declval<V>()[0])> type; };
template<typename V> struct element_type_s1<V, false>		{ typedef V type; };
template<typename V> struct	element_type_s		: element_type_s1<V, can_index<V>> {};
template<typename V> struct	raw_element_type_s	: element_type_s1<V, can_index<V>> {};
template<typename V> using	element_type		= typename element_type_s<V>::type;
template<typename V> using	raw_element_type	= typename raw_element_type_s<V>::type;

template<typename V, typename T, int N> static constexpr bool is_simdN = iso::same_v<V, vec<T, N>> || iso::same_v<V, packed_simd<T, N>>;
template<typename V> struct not_simd { static const int value = sizeof(element_type<V>) ? sizeof(V) / sizeof(element_type<V>) : 0; static const bool simd = false; };

template<typename V, int N, bool B = is_simdN<V, element_type<V>, N>> 				struct num_els_s3	{ static const int value = N; static const bool simd = N > 1; };
template<typename V, int N, bool B = sizeof(V) == sizeof(vec<element_type<V>, N>)> 	struct num_els_s2	: num_els_s3<V, N> {};
template<typename V, int N> 														struct num_els_s1	: num_els_s2<V, N> {};
template<typename V, bool B = is_vec_element<element_type<V>>> 						struct num_els_s	: num_els_s1<V, (sizeof(V) < 1024 ? sizeof(V) / sizeof(element_type<V>) : 1)> {};

template<typename V, int N> struct num_els_s3<V, N, false> : num_els_s2<V, N - 1> {};

template<typename V>		struct num_els_s3<V, 1, false>	: not_simd<V> {};
template<typename V, int N> struct num_els_s2<V, N, false>	: not_simd<V> {};
template<typename V> 		struct num_els_s1<V, 0>			: not_simd<V> {};
template<typename V> 		struct num_els_s1<V, 1>			: not_simd<V> {};
template<typename V> 		struct num_els_s<V, false> 		: not_simd<V> {};

template<typename V> 		constexpr bool	is_vec	= num_els_s<V>::simd;
template<typename V> 		constexpr bool	is_simd	= num_els_s<V>::simd;
template<typename V, int N> constexpr bool	is_vecN	= num_els_s<V>::simd && num_els_s<V>::value == N;
}

template<typename V> constexpr int 	num_elements_v<V, enable_if_t<simd::is_simd<V>>>	= simd::num_els_s<V>::value;
template<typename A, typename B>constexpr int num_elements_v<pair<A, B>>				= num_elements_v<A> + num_elements_v<B>;

namespace simd {
//-----------------------------------------------------------------------------
// conversions
//-----------------------------------------------------------------------------

template<typename V, int...I> SIMD_FUNC vec<element_type<V>, sizeof...(I)> swizzle(meta::value_list<int, I...>, const V &a, const V &b) {
	return __builtin_shufflevector(a, b, I...);
}

template<typename V, int I> SIMD_FUNC auto swizzle(meta::value_list<int, I>, const V &a, const V &b) {
	return a[I];
}

template<int F, int T, typename = void>	struct resize_s;
template<int F, int T>	struct resize_s<F, T, enable_if_t<(F > 1 && F < T)>> {
//	template<typename V>	static SIMD_FUNC auto f(const V &a) { return swizzle(meta::VL_concat<meta::make_value_sequence<F>, meta::muladd<0, -1, meta::make_value_sequence<T-F>>>(), a, a); }
	template<typename V>	static SIMD_FUNC auto f(const V &a) { return swizzle(concat(meta::make_value_sequence<F>(), meta::make_value_sequence<T - F>() * 0_kk - 1_kk), a, a); }
};
template<int F, int T>	struct resize_s<F, T, enable_if_t<(T > 1 && F > T)>> {
	template<typename V>	static SIMD_FUNC auto f(const V &a) { return swizzle(meta::make_value_sequence<T>(), a, a); }
};
template<int F>			struct resize_s<F,F> {
	template<typename V>	static SIMD_FUNC auto f(const V &a)	{ return a; }
};
template<int T>			struct resize_s<1,T> {
	template<typename V>	static SIMD_FUNC auto f(const V &a)	{ return vec<element_type<V>, T>(a); }
};
template<int F>			struct resize_s<F, 1> {
	template<typename V>	static SIMD_FUNC auto f(const V &a)	{ return a.x; }
};
template<> 				struct resize_s<1, 1> {
	template<typename V>	static SIMD_FUNC auto f(const V &a)	{ return a; }
};

template<int S, typename V>	SIMD_FUNC	auto	shrink_top(const V &a)	{ return swizzle(meta::make_value_sequence<S>() + meta::int_constant<num_elements_v<V> - S>(), a, a); }
template<int S, typename V>	SIMD_FUNC	auto	shrink(const V &a)		{ return resize_s<num_elements_v<V>, S>::f(a); }
template<int S, typename V>	SIMD_FUNC	auto	grow(const V &a)		{ return resize_s<num_elements_v<V>, S>::f(a); }

template<typename V>		SIMD_FUNC	auto 	simd_lo_hi(V lo, V hi)	{
	static const int N = num_elements_v<V>;
	return swizzle(meta::VL_concat<meta::make_value_sequence<N>, meta::muladd<1, N, meta::make_value_sequence<N>>>(), lo, hi);
}

template<typename T, typename F> SIMD_FUNC auto		as(const F &p)	{ return reinterpret_cast<const typename get_type<T,num_elements_v<F> * sizeof(raw_element_type<F>) / sizeof(T), alignof(F)>::type&>(p); }
template<typename T, typename F> SIMD_FUNC auto&	as(F &p)		{ return reinterpret_cast<typename get_type<T,num_elements_v<F> * sizeof(raw_element_type<F>) / sizeof(T), alignof(F)>::type&>(p); }

template<typename T, typename V> SIMD_FUNC enable_if_t<is_simd<V>, vec<T, num_elements_v<V>>> 	to(V v)	{ return __builtin_convertvector(v, vec<T, num_elements_v<V>>); }
template<typename T, typename V> SIMD_FUNC enable_if_t<(num_elements_v<V> == 1), T>				to(V v)	{ return T(v); }

template<typename T, typename P, typename V = void> struct vec_maker1 {
	T	t;
	vec_maker1(P p) : t(p) {}
};

template<typename T, typename P> struct vec_maker1<T, P,  enable_if_t<(num_elements_v<P> > 1)> > {
	T	t[num_elements_v<P>];
	vec_maker1(P p) : vec_maker1(iso::force_cast<vec_maker1>(to<T>(p))) {}
};

template<typename T, typename P0, typename...P> struct vec_maker {
	vec_maker1<T, P0>	m0;
	vec_maker<T, P...>	m;
	vec_maker(P0 p0, P... p) : m0(p0), m(p...) {}
};
template<typename T, typename P0> struct vec_maker<T, P0> : vec_maker1<T, P0> {
	vec_maker(P0 p0) : vec_maker1<T, P0>(p0) {}
};

template<typename T, typename P0, typename P1, typename...P> SIMD_FUNC auto	to(P0 p0, P1 p1, P...p)	{
	return iso::force_cast<vec<T, sizeof(vec_maker<T, P0, P1, P...>) / sizeof(T)>>(vec_maker<T, P0, P1, P...>(p0, p1, p...));
}

template<int I, typename V, typename=enable_if_t<is_simd<V>>>					SIMD_FUNC auto swizzle(const V &v) { return v[I]; }
template<int I0, int I1, int...I, typename V, typename=enable_if_t<is_simd<V>>>	SIMD_FUNC auto swizzle(const V &v) { return __builtin_shufflevector(v, v, I0, I1, I...); }

template<int I0, int...I, typename A, typename B> SIMD_FUNC vec<element_type<A>, sizeof...(I) + 1> swizzle(const A &a, const B &b) {
	return __builtin_shufflevector(vec<element_type<A>, num_elements_v<A>>(a), grow<num_elements_v<A>>(to<element_type<A>>(b)), I0, I...);
}
template<int...I, typename...P> SIMD_FUNC auto swizzle(meta::value_list<int, I...>, const P&...p) {
	return swizzle<I...>(p...);
}

template<typename T, int...I, typename P0, typename P1, typename...P>	SIMD_FUNC auto swizzle(const P0 &p0, const P1 &p1, const P&...p) {
	return swizzle<I...>(to<T>(p0, p1, p...));
}
template<int I0, int...I, typename P0, typename P1, typename...P>	SIMD_FUNC auto swizzle(const P0 &p0, const P1 &p1, const P&...p) {
	return swizzle<element_type<P0>, I0, I...>(p0, p1, p...);
}

} // namespace simd

//enable_if_t<simd::is_vec<V>>
template<typename V> constexpr bool use_constants<V, enable_if_t<simd::is_vec<V>>> = true;
template<typename K, typename V> struct constant_cast<K, V, enable_if_t<simd::is_simd<V>> >	{ static const V f() { return K::template as<simd::element_type<V>>(); } };
template<typename K, typename V> struct co_type<K, V, enable_if_t<same_v<simd::element_type<V>, double>>> { typedef double type; };

} // namespace iso

#endif
