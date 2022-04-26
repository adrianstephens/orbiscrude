#ifndef SIMD_VECTOR_H
#define SIMD_VECTOR_H

#include "_simd.h"
#include "_maths.h"
#include "bits.h"

namespace iso {

template<typename T, typename A, typename B> SIMD_FUNC auto	to(pair<A, B> f) {
	return simd::to<T>((A)f.a, (B)f.b);
}

template<typename T> struct T_int_ops<T, enable_if_t<simd::is_vec<T>>>		: T_builtin_int<simd::element_type<T>> {};
template<typename X> struct T_signed<X,  true,	enable_if_t<simd::is_vec<X>>>	: T_type<simd::vec<signed_t<  simd::element_type<X>>, num_elements_v<X>>>	{};
template<typename X> struct T_signed<X,  false,	enable_if_t<simd::is_vec<X>>>	: T_type<simd::vec<unsigned_t<simd::element_type<X>>, num_elements_v<X>>>	{};

// cause ambiguities - don't enable
//template<typename V> enable_if_t<simd::is_simd<V>, simd::element_type<V>*> begin(const V &v) 	{ return (simd::element_type<V>*)&v; }
//template<typename V> enable_if_t<simd::is_simd<V>, simd::element_type<V>*> end(const V &v) 		{ return (simd::element_type<V>*)&v + num_elements_v<V>; }

namespace simd {
	using iso::pi;
	using iso::one;
	using iso::zero;
	using iso::two;
	using iso::half;
	using iso::horner;
	using iso::square;
	using iso::atan2;
	using iso::abs;
	
#if defined __ARM_NEON__
	template<typename E> static const int neon_type =  0;
	template<> static const int neon_type<int8> 	=  0;
	template<> static const int neon_type<int16> 	=  1;
	template<> static const int neon_type<int32> 	=  2;
	template<> static const int neon_type<int64> 	=  3;
  #ifdef USE_FP16
	template<> static const int neon_type<hfloat> 	=  8;
  #endif
	template<> static const int neon_type<float> 	=  9;
	template<> static const int neon_type<double> 	= 10;
	template<> static const int neon_type<uint8> 	= 16;
	template<> static const int neon_type<uint16> 	= 17;
	template<> static const int neon_type<uint32> 	= 18;
	template<> static const int neon_type<uint64> 	= 19;
#endif

	template<typename T, typename TI, TI...I>	SIMD_FUNC auto to(meta::value_list<TI, I...>)		{ return to<T>(I...); }
	template<typename T, typename...K>			SIMD_FUNC auto to(meta::type_list<K...>)			{ return to<T>(K()...); }

	template<typename V, int N, int...I>	 constexpr auto _swizzle(meta::array<int, N> a, meta::value_list<int, I...>, const V &v)	{ return swizzle<a[I]...>(v); }
	template<typename V, int N>	 constexpr auto swizzle(meta::array<int, N> a, const V &v)	{ return _swizzle(a, meta::make_value_sequence<N>(), v); }

	template<typename T>	using	vec_type	= vec<element_type<T>, num_elements_v<T>>;

	template<typename T>	SIMD_FUNC enable_if_t<is_vec<T>, T>				as_vec(T t)		{ return t; }
	template<typename T>	SIMD_FUNC auto	as_int(T t)	{ return iso::as_signed(t); }


	//-----------------------------------------------------------------------------
	// swizzles
	// 	   rotate<+1>(v): v.yzwx
	// 	   rotate<-1>(v): v.wxyz
	//-----------------------------------------------------------------------------

	template<typename P0, typename...P> SIMD_FUNC constexpr auto	concat(P0 p0, P...p) {
		return to<element_type<P0>>(p0, p...);
	}

	template<int R, typename T> auto rotate(const T &v) {
		static const int N = num_elements_v<T>;
		return swizzle(meta::roll<(R % N + N) % N, meta::make_value_sequence<N>>(), v);
	}
	template<typename T> auto rotate(const T &v) {
		return rotate<1>(v);
	}

	template<int N, int M> struct rotate_s {
		template<typename T> static SIMD_FUNC T f(T v, int i) {
			return rotate_s<N, M / 2>::f(i & M ? swizzle(meta::roll<M, meta::make_value_sequence<N>>(), v) : v, i);
		}
	};
	template<int N> struct rotate_s<N, 1> {
		template<typename T> static SIMD_FUNC T f(T v, int i) {
			return i & 1 ? swizzle(meta::roll<1, meta::make_value_sequence<N>>(), v) : v;
		}
	};
	template<typename T> SIMD_FUNC T rotate(T v, int i) {
		static const int N = num_elements_v<T>;
		return rotate_s<N, pow2_ceil<N> / 2>::f(v, i < 0 ? i + N : i);
	}

	template<int N, int N2> struct extend_s {
		template<typename V> static auto	left(V v)	{ return concat(vec<element_type<V>, N2 - N>(zero), v); }
		template<typename V> static auto	right(V v)	{ return concat(v, vec<element_type<V>, N2 - N>(zero)); }
	};
	template<int N> struct extend_s<N, N> {
		template<typename V> static V		left(V v)	{ return v; }
		template<typename V> static V		right(V v)	{ return v; }
	};

	template<int N2, typename V>	auto extend_left(V v)	{ return extend_s<num_elements_v<V>, N2>::left(v); }
	template<int N2, typename V>	auto extend_right(V v)	{ return extend_s<num_elements_v<V>, N2>::right(v); }

	//-----------------------------------------------------------------------------
	// make_mask
	//-----------------------------------------------------------------------------

	template<typename E, int N, typename=void> struct make_mask_s;
	template<int N> struct make_mask_s<int8, N, enable_if_t<(N <= 8)>> {
		static vec<int8, N>	m(int M) { return shrink<N>(as<int8>(part_bits<1,7,N>(M) * 0xff)); }
		static vec<int8, N>	s(int M) { return shrink<N>(as<int8>(part_bits<1,7,N>(M) * 0x80)); }
	};
	template<int N> struct make_mask_s<int8, N, enable_if_t<(N > 8)>> {
		static vec<int8, N>	m(int M) { return concat(make_mask_s<int8, 8>::m(M), make_mask_s<int8, N - 8>::m(M >> 8)); }
		static vec<int8, N>	s(int M) { return concat(make_mask_s<int8, 8>::s(M), make_mask_s<int8, N - 8>::s(M >> 8)); }
	};
	template<typename E, int N, typename> struct make_mask_s {
		static vec<E, N>	m(int M) { return to<E>(make_mask_s<int8, N>::m(M)); }
		static vec<E, N>	s(int M) { return to<E>(make_mask_s<int8, N>::m(M)) & 0x8000; }
	};

	template<typename E, int N, int M, int...I> SIMD_FUNC vec<E, N> _make_mask(meta::value_list<int, I...>) {
		return {E(-(((M) >> I) & 1))...};
	}
	template<typename E, int N, int M> vec<E, N> SIMD_FUNC make_mask() {
		return _make_mask<E, N, M>(meta::make_value_sequence<N>());
	}
	//so we can use make_mask in macros (e.g. _mm_mask_i32gather_epi32)
	#define MAKE_MASK(E, N, M)	make_mask<E,N,M>()
	template<typename E, int N> vec<E, N> SIMD_FUNC make_mask(int m) { return make_mask_s<E, N>::m(m); }
	template<typename E, int N, typename M> enable_if_t<is_simd<M>, vec<E, N>> SIMD_FUNC make_mask(M m) { return as<E>(m); }

	template<typename E, int N, int M, int...I> vec<E, N> SIMD_FUNC _make_sign_mask(meta::value_list<int, I...>) {
		static const int	BITS = sizeof(E) * 8;
		return {E((M & (1<<I)) << (BITS - I - 1))...};
	}
	template<typename E, int N, int M> vec<E, N> SIMD_FUNC make_sign_mask() {
		return _make_sign_mask<E, N, M>(meta::make_value_sequence<N>());
	}
	template<typename E, int N> vec<E, N> SIMD_FUNC make_sign_mask(int M) 		{ return make_mask_s<E, N>::s(M); }
	template<typename E, int N> vec<E, N> SIMD_FUNC make_sign_mask(vec<E, N> M)	{ return M & (E(1) << (sizeof(E) * 8 - 1)); }

	//-----------------------------------------------------------------------------
	// load/stores
	//-----------------------------------------------------------------------------
#if defined __ARM_NEON__
	template<typename E> constexpr int 	fullnum 	= 16 / sizeof(E);
	template<typename E> using 			fullvec 	= vec<E, fullnum<E>>;

	template<int N> struct load_row_s;

#ifdef __clang__
	template<> struct load_row_s<1> 	{ template<int R, typename E> static inline void f(fullvec<E> *m, E *p) { m[0] = __builtin_neon_vld1q_lane_v(p, m[0], R, neon_type<E>|0x20); } };
	template<> struct load_row_s<2> 	{ template<int R, typename E> static inline void f(fullvec<E> *m, E *p) { __builtin_neon_vld2q_lane_v(m, p, m[0], m[1], R, neon_type<E>|0x20); } };
	template<> struct load_row_s<3> 	{ template<int R, typename E> static inline void f(fullvec<E> *m, E *p) { __builtin_neon_vld3q_lane_v(m, p, m[0], m[1], m[2], R, neon_type<E>|0x20); } };
	template<> struct load_row_s<4> 	{ template<int R, typename E> static inline void f(fullvec<E> *m, E *p) { __builtin_neon_vld4q_lane_v(m, p, m[0], m[1], m[2], m[3], R, neon_type<E>|0x20); } };
	template<int N> struct load_row_s 	{ template<int R, typename E> static inline void f(fullvec<E> *m, E *p) { load_row_s<4>::template f<R>(m, p); load_row_s<N - 4>::template f<R>(m + 4, p + 4); } };
	template<typename E, int M, int R> inline void load_row(fullvec<E> *m, E *p) { load_row_s<M>::template f<R>(m, p); };

	template<typename E, int M, int N, typename I, typename=void> struct load_rows_s {
		template<int...J> static void f2(fullvec<E> *m, I p, meta::value_list<int, J...>) { discard((load_row_s<M>::template f<J, E>(m, p[J]), 0)...); }
		static void f(fullvec<E> *m, I p) { return f2(m, p, meta::make_value_sequence<N>()); }
	};
	template<typename E, int N> struct load_rows_s<E, 2, N, const E(*)[2]> {
		static void f(fullvec<E> *m, const E (*p)[2]) { __builtin_neon_vld2q_v(m, p[0], neon_type<E>|0x20); }
	};
	template<typename E, int N> struct load_rows_s<E, 3, N, const E(*)[3]> {
		static void f(fullvec<E> *m, const E (*p)[3]) { __builtin_neon_vld3q_v(m, p[0], neon_type<E>|0x20); }
	};
	template<typename E, int N> struct load_rows_s<E, 4, N, const E(*)[4]> {
		static void f(fullvec<E> *m, const E (*p)[4]) { __builtin_neon_vld4q_v(m, p[0], neon_type<E>|0x20); }
	};
	template<typename E, int M, int N> struct load_rows_s<E, M, N, E(*)[M]> : load_rows_s<E, M, N, const E(*)[M]> {};
	template<typename E, int M, int N, typename I> inline void load_rows(fullvec<E> *m, I p) { load_rows_s<E, M, N, I>::f(m, p); }
#endif

	template<int N, typename=void> struct load_vec_s;

	template<> struct load_vec_s<8> {
#ifdef __clang__
		template<typename E> force_inline static void load(void *t, const E *p)		{ *(vec<E, 8 / sizeof(E)>*)t = __builtin_neon_vld1_v(p, neon_type<E>); }
		template<typename E> force_inline static void store(const void *t, E *p)	{ __builtin_neon_vst1_v(p, *(const vec<E, 8 / sizeof(E)>*)t, neon_type<E>); }
#else
		force_inline static void load(void *t, const int8  *p)	{ *(__n64*)t = neon_ld1m_8(p); }
		force_inline static void load(void *t, const int16 *p)	{ *(__n64*)t = neon_ld1m_16(p); }
		force_inline static void load(void *t, const int32 *p)	{ *(__n64*)t = neon_ld1m_32(p); }
		force_inline static void store(const void *t, int8  *p)	{ neon_st1m_8 (p, *(__n64*)t); }
		force_inline static void store(const void *t, int16 *p)	{ neon_st1m_16(p, *(__n64*)t); }
		force_inline static void store(const void *t, int32 *p)	{ neon_st1m_32(p, *(__n64*)t); }
#endif
	};
	template<> struct load_vec_s<16> {
#ifdef __clang__
		template<typename E> force_inline static void load(void *t, const E *p)		{ *(fullvec<E>*)t = __builtin_neon_vld1q_v(p, neon_type<E>|0x20); }
		template<typename E> force_inline static void store(const void *t, E *p)	{ __builtin_neon_vst1q_v(p, *(const fullvec<E>*)t, neon_type<E>|0x20); }
#else
		force_inline static void load(void *t, const int8  *p)	{ *(__n128*)t = neon_ld1m_q8(p); }
		force_inline static void load(void *t, const int16 *p)	{ *(__n128*)t = neon_ld1m_q16(p); }
		force_inline static void load(void *t, const int32 *p)	{ *(__n128*)t = neon_ld1m_q32(p); }
		force_inline static void store(const void *t, int8  *p)	{ neon_st1m_q8 (p, *(__n128*)t); }
		force_inline static void store(const void *t, int16 *p)	{ neon_st1m_q16(p, *(__n128*)t); }
		force_inline static void store(const void *t, int32 *p)	{ neon_st1m_q32(p, *(__n128*)t); }
#endif
	};
	template<> struct load_vec_s<32> {
#ifdef __clang__
		template<typename E> force_inline static void load(void *t, const E *p)		{ __builtin_neon_vld1q_x2_v((fullvec<E>*)t, p, neon_type<E>|0x20); }
		template<typename E> force_inline static void store(const void *_t, E *p)	{ auto t = (const fullvec<E>*)_t; __builtin_neon_vst1q_x2_v(p, t[0], t[1], neon_type<E>|0x20); }
#else
		force_inline static void load(void *t, const int8  *p)	{ *(__n128x2*)t = neon_ld1m2_q8(p); }
		force_inline static void load(void *t, const int16 *p)	{ *(__n128x2*)t = neon_ld1m2_q16(p); }
		force_inline static void load(void *t, const int32 *p)	{ *(__n128x2*)t = neon_ld1m2_q32(p); }
		force_inline static void store(const void *t, int8  *p)	{ neon_st1m2_q8 (p, *(__n128x2*)t); }
		force_inline static void store(const void *t, int16 *p)	{ neon_st1m2_q16(p, *(__n128x2*)t); }
		force_inline static void store(const void *t, int32 *p)	{ neon_st1m2_q32(p, *(__n128x2*)t); }
#endif
	};
	template<> struct load_vec_s<48> {
#ifdef __clang__
		template<typename E> force_inline static void load(void *t, const E *p)		{ __builtin_neon_vld1q_x3_v((fullvec<E>*)t, p, neon_type<E>|0x20); }
		template<typename E> force_inline static void store(const void *_t, E *p)	{ auto t = (const fullvec<E>*)_t; __builtin_neon_vst1q_x3_v(p, t[0], t[1], t[2], neon_type<E>|0x20); }
#else
		force_inline static void load(void *t, const int8  *p)	{ *(__n128x3*)t = neon_ld1m3_q8(p); }
		force_inline static void load(void *t, const int16 *p)	{ *(__n128x3*)t = neon_ld1m3_q16(p); }
		force_inline static void load(void *t, const int32 *p)	{ *(__n128x3*)t = neon_ld1m3_q32(p); }
		force_inline static void store(const void *t, int8  *p)	{ neon_st1m3_q8 (p, *(__n128x3*)t); }
		force_inline static void store(const void *t, int16 *p)	{ neon_st1m3_q16(p, *(__n128x3*)t); }
		force_inline static void store(const void *t, int32 *p)	{ neon_st1m3_q32(p, *(__n128x3*)t); }
#endif
	};
	template<> struct load_vec_s<64> {
#ifdef __clang__
		template<typename E> force_inline static void load(void *t, const E *p)		{ __builtin_neon_vld1q_x4_v((fullvec<E>*)t, p, neon_type<E>|0x20); }
		template<typename E> force_inline static void store(const void *_t, E *p)	{ auto t = (const fullvec<E>*)_t; __builtin_neon_vst1q_x4_v(p, t[0], t[1], t[2], t[3], neon_type<E>|0x20); }
#else
		force_inline static void load(void *t, const int8  *p)	{ *(__n128x4*)t = neon_ld1m4_q8(p); }
		force_inline static void load(void *t, const int16 *p)	{ *(__n128x4*)t = neon_ld1m4_q16(p); }
		force_inline static void load(void *t, const int32 *p)	{ *(__n128x4*)t = neon_ld1m4_q32(p); }
		force_inline static void store(const void *t, int8  *p)	{ neon_st1m4_q8 (p, *(__n128x4*)t); }
		force_inline static void store(const void *t, int16 *p)	{ neon_st1m4_q16(p, *(__n128x4*)t); }
		force_inline static void store(const void *t, int32 *p)	{ neon_st1m4_q32(p, *(__n128x4*)t); }
#endif
	};
	
	template<int N> struct load_vec_s<N, enable_if_t<(N < 64)>>  {
		template<typename E> force_inline static void load(void *t, const E *p)		{ load_vec_s<pow2_ceil<N>>::load(t, p); }
		template<typename E> force_inline static void store(const void *t, E *p) 	{ memcpy(p, t, N); }
	};
	template<int N> struct load_vec_s<N, enable_if_t<(N > 64)>>  {
		template<typename E> force_inline static void load(void *t, const E *p) 	{ load_vec_s<64>::load(t, p); load_vec_s<N - 64>::load((int8*)t + 64, p + 4 * fullnum<E>); }
		template<typename E> force_inline static void store(const void *t, E *p) 	{ load_vec_s<64>::store(t, p); load_vec_s<N - 64>::store((const fullvec<E>*)t + 4, p + 4 * fullnum<E>); }
	};

#ifdef __clang__
	template<typename V, typename E> void load_vec(V &t, const E *p) 	{ load_vec_s<sizeof(V)>::load(&t, p); }
	template<typename V, typename E> void store_vec(const V &t, E *p) 	{ load_vec_s<sizeof(E) * num_elements_v<V>>::store(&t, p); }
#else
	template<typename V, typename E> void load_vec(V &t, const E *p) 	{ load_vec_s<sizeof(V)>::load(&t, (sint_for_t<E>*)p); }
	template<typename V, typename E> void store_vec(const V &t, E *p) 	{ load_vec_s<sizeof(E) * num_elements_v<V>>::store(&t, (sint_for_t<E>*)p); }
#endif
	
#elif defined __SSE2__

	template<int S=4> force_inline vec<int32, 4>	load_gather(const int32 *p, vec<int32, 4> indices)	{ return _mm_i32gather_epi32(p, indices, S); }
	template<int S=8> force_inline vec<int64, 2>	load_gather(const int64 *p, vec<int32, 2> indices)	{ return _mm_i32gather_epi64(p, grow<4>(indices), S); }
	template<int S=4> force_inline vec<int32, 8>	load_gather(const int32 *p, vec<int32, 8> indices)	{ return _mm256_i32gather_epi32(p, indices, S); }
	template<int S=8> force_inline vec<int64, 4>	load_gather(const int64 *p, vec<int32, 4> indices)	{ return _mm256_i32gather_epi64(p, indices, S); }
  #ifdef __AVX512F__
	template<int S=4> force_inline vec<int32, 16>	load_gather(const int32 *p, vec<int32, 16> indices)	{ return _mm512_i32gather_epi32(indices, p, S); }
	template<int S=8> force_inline vec<int64, 8>	load_gather(const int64 *p, vec<int32, 8> indices)	{ return _mm512_i32gather_epi64(indices, p, S); }
  #endif
  
  #ifdef __AVX512F__
	template<int M, int S=4> force_inline vec<int32, 4>	load_gather_mask(const int32 *p, vec<int32, 4> indices)	{ return _mm_mmask_i32gather_epi32(_mm_undefined_si128(), 		p, indices, 			MAKE_MASK(int32, 4, M), S); }
	template<int M, int S=8> force_inline vec<int64, 2>	load_gather_mask(const int64 *p, vec<int32, 2> indices)	{ return _mm_mmask_i32gather_epi64(_mm_undefined_si128(), 		p, grow<4>(indices), 	MAKE_MASK(int64, 2, M), S); }
	template<int M, int S=4> force_inline vec<int32, 8>	load_gather_mask(const int32 *p, vec<int32, 8> indices)	{ return _mm256_mmask_i32gather_epi32(_mm256_undefined_si256(), p, indices, 			MAKE_MASK(int32, 8, M), S); }
	template<int M, int S=8> force_inline vec<int64, 4>	load_gather_mask(const int64 *p, vec<int32, 4> indices)	{ return _mm256_mmask_i32gather_epi64(_mm256_undefined_si256(), p, indices, 			MAKE_MASK(int64, 4, N), S); }
	template<int M, int S=4> force_inline vec<int32,16>	load_gather_mask(const int32 *p, vec<int32,16> indices)	{ return _mm512_mask_i32gather_epi32(_mm512_undefined_epi32(), N, indices, p, S); }
	template<int M, int S=8> force_inline vec<int64, 8>	load_gather_mask(const int64 *p, vec<int32, 8> indices)	{ return _mm512_mask_i32gather_epi64(_mm512_undefined_epi32(), N, indices, p, S); }
  #else
	template<int M, int S=4> force_inline vec<int32, 4>	load_gather_mask(const int32 *p, vec<int32, 4> indices)	{ return _mm_mask_i32gather_epi32(_mm_undefined_si128(), 		p, indices, 			MAKE_MASK(int32, 4, M), S); }
	template<int M, int S=8> force_inline vec<int64, 2>	load_gather_mask(const int64 *p, vec<int32, 2> indices)	{ return _mm_mask_i32gather_epi64(_mm_undefined_si128(), 		p, grow<4>(indices), 	MAKE_MASK(int64, 2, M), S); }
	template<int M, int S=4> force_inline vec<int32, 8>	load_gather_mask(const int32 *p, vec<int32, 8> indices)	{ return _mm256_mask_i32gather_epi32(_mm256_undefined_si256(), 	p, indices, 			MAKE_MASK(int32, 8, M), S); }
	template<int M, int S=8> force_inline vec<int64, 4>	load_gather_mask(const int64 *p, vec<int32, 4> indices)	{ return _mm256_mask_i32gather_epi64(_mm256_undefined_si256(), 	p, indices, 			MAKE_MASK(int64, 4, M), S); }
  #endif

	template<int S, typename E, typename I> 		force_inline auto	load_gather(const E *p, I indices)	{
		return simd_lo_hi(load_gather<S>(p, indices.lo), load_gather<S>(p, indices.hi));
	}
	template<int M, int S, typename E, typename I> 	force_inline auto	load_gather_mask(const E *p, I indices)	{
		return simd_lo_hi(load_gather<S>(p, indices.lo), load_gather_mask<(M >> (num_elements_v<I> / 2)), S>(p, indices.hi));
	}

	template<typename E, int N, int S, typename=void> struct load_gather_s;
	template<typename E, int N, int S> struct load_gather_s<E, N, S, enable_if_t<sizeof(E) >= 4 && meta::is_pow2<N>>> {
		typedef sint_for_t<E>		I;
		static force_inline vec<E, N> f(const E *p, vec<int32, N> indices)	{ return as<E>(load_gather<S>((const I*)p, indices)); }
	};
	template<typename E, int N, int S> struct load_gather_s<E, N, S, enable_if_t<sizeof(E) >= 4 && !meta::is_pow2<N>>> {
		typedef sint_for_t<E>		I;
		static const int N2 = pow2_ceil<N>;
		static force_inline vec<E, N> f(const E *p, vec<int32, N> indices)	{ return shrink<N>(as<E>(load_gather_mask<(1 << N) - 1, S>((const I*)p, grow<N2>(indices)))); }
	};
	template<typename E, int N, int S> struct load_gather_s<E, N, S, enable_if_t<sizeof(E) < 4>> {
		static force_inline vec<E, N> f(const E *p, vec<int32, N> indices)	{ return to<E>(load_gather_s<int32, N, S>::f((const int32*)p, indices)); }
	};
	template<typename E, typename I> force_inline auto	load_gather(const E *p, I indices)	{
		return load_gather_s<E, num_elements_v<I>, sizeof(E)>::f(p, indices);
	}

	force_inline void load_vec(vec<int, 1>  &t, const int *p)	{ t = *(vec<int, 1>*)p; }
	force_inline void load_vec(vec<int, 2>  &t, const int *p)	{ t = *(vec<int, 2>*)p; }
	force_inline void load_vec(vec<int, 4>  &t, const int *p)	{ t = _mm_loadu_si128((__m128i*)p); }
  #ifdef __AVX2__
	force_inline void load_vec(vec<int, 8>  &t, const int *p)	{ t = _mm256_loadu_si256((__m256i*)p); }
  #endif
  #ifdef __AVX512F__
	force_inline void load_vec(vec<int, 16> &t, const int *p)	{ t = _mm512_loadu_epi32(p); }
  #endif

	template<typename V, typename E> force_inline enable_if_t<(sizeof(V) * 8 <= max_register_bits)> load_vec(V &t, const E *p) {
		load_vec((vec<int, pow2_ceil<num_elements_v<V>> * sizeof(E) / sizeof(int)>&)t, (const int*)p);
	}

	template<typename V, typename E> force_inline enable_if_t<(sizeof(V) * 8 > max_register_bits)> load_vec(V &t, const E *p) {
		static constexpr int N = num_elements_v<V>;
		static constexpr int N2 = pow2_ceil<N> / 2;
		load_vec((vec<E, N2>&)t, p);
		load_vec(*(vec<E, N - N2>*)((E*)&t + N2), p + N2);
	}

	force_inline void store_vec(const vec<int, 1>  &t, int *p)	{ *(vec<int, 1>*)p = t; }
	force_inline void store_vec(const vec<int, 2>  &t, int *p)	{ *(vec<int, 2>*)p = t; }
	force_inline void store_vec(const vec<int, 4>  &t, int *p)	{ _mm_storeu_si128((__m128i*)p, t); }
  #ifdef __AVX2__
	force_inline void store_vec(const vec<int, 8>  &t, int *p)	{ _mm256_storeu_si256((__m256i*)p, t); }
  #endif
  #ifdef __AVX512F__
	force_inline void store_vec(const vec<int, 16> &t, int *p)	{ _mm512_storeu_epi32(p, t); }
  #endif

	constexpr int double_bits(int N) { return N * N + N * 2; }

	template<int N> force_inline void store_vec_mask(const vec<int8,  4>  &t, int8 *p) 	{ _m_maskmovq(grow<8>(t), 	make_mask<int8, 8, N>(), 				(char*)p); }
	template<int N> force_inline void store_vec_mask(const vec<int16, 2>  &t, int8 *p) 	{ _m_maskmovq(grow<4>(t), 	make_mask<int8, 8, double_bits(N)>(),	(char*)p); }
	template<int N> force_inline void store_vec_mask(const vec<int8,  8>  &t, int8 *p)	{ _m_maskmovq(t, 			make_mask<int8, 8, N>(), 				(char*)p); }
	template<int N> force_inline void store_vec_mask(const vec<int16, 4>  &t, int8 *p)	{ _m_maskmovq(t, 			make_mask<int8, 8, double_bits(N)>(), 	(char*)p); }
	template<int N> force_inline void store_vec_mask(const vec<int32, 2>  &t, int8 *p)	{ _m_maskmovq(t, 			make_mask<int8, 8, double_bits(double_bits(N))>(), (char*)p); }
  #ifdef __AVX512F__
	template<int N> force_inline void store_vec_mask(const vec<int8, 16>  &t, int8  *p)	{ _mm_mask_storeu_epi8 (p, N, *(__m256i*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int16, 8>  &t, int16 *p)	{ _mm_mask_storeu_epi16(p, N, *(__m256i*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int32, 4>  &t, int32 *p)	{ _mm_mask_storeu_epi32(p, N, *(__m256i*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int64, 2>  &t, int64 *p)	{ _mm_mask_storeu_epi64(p, N, *(__m256i*)t); }

	template<int N> force_inline void store_vec_mask(const vec<int8, 32>  &t, int8  *p)	{ _mm256_mask_storeu_epi8 (p, N, *(__m256i*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int16,16>  &t, int16 *p)	{ _mm256_mask_storeu_epi16(p, N, *(__m256i*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int32, 8>  &t, int32 *p)	{ _mm256_mask_storeu_epi32(p, N, *(__m256i*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int64, 4>  &t, int64 *p)	{ _mm256_mask_storeu_epi64(p, N, *(__m256i*)t); }

	template<int N> force_inline void store_vec_mask(const vec<int8, 32>  &t, int8  *p)	{ _mm512_mask_storeu_epi8 (p, N, *(__m512*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int16,16>  &t, int16 *p)	{ _mm512_mask_storeu_epi16(p, N, *(__m512*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int32, 8>  &t, int32 *p)	{ _mm512_mask_storeu_epi32(p, N, *(__m512i*)t); }
	template<int N> force_inline void store_vec_mask(const vec<int64, 4>  &t, int64 *p)	{ _mm512_mask_storeu_epi64(p, N, *(__m512i*)t); }
  #else
	template<int N> force_inline void store_vec_mask(const vec<int8, 16>  &t, int8  *p)	{ _mm_maskmoveu_si128(t, 	make_mask<int8, 16, N>(), (char*)p); }
	template<int N> force_inline void store_vec_mask(const vec<int16, 8>  &t, int16 *p)	{ _mm_maskmoveu_si128(t, 	make_mask<int8, 16, N>(), (char*)p); }
	template<int N> force_inline void store_vec_mask(const vec<int8, 32>  &t, int8  *p)	{ store_vec(as<int>(t.lo), (int*)p); store_vec_mask<(N >> 16)>(t.hi, p + 16); }
	template<int N> force_inline void store_vec_mask(const vec<int16,16>  &t, int16 *p)	{ store_vec(as<int>(t.lo), (int*)p); store_vec_mask<(N >>  8)>(t.hi, p + 8); }
	#ifdef __AVX2__
	template<int N> force_inline void store_vec_mask(const vec<int32, 4>  &t, int32 *p)	{ _mm_maskstore_epi32(p, 	make_mask<int32, 4, N>(), t); }
	template<int N> force_inline void store_vec_mask(const vec<int64, 2>  &t, int64 *p)	{ _mm_maskstore_epi64(p, 	make_mask<int64, 2, N>(), t); }
	template<int N> force_inline void store_vec_mask(const vec<int32, 8>  &t, int32 *p)	{ _mm256_maskstore_epi32(p,	make_mask<int32, 8, N>(), t); }
	template<int N> force_inline void store_vec_mask(const vec<int64, 4>  &t, int64 *p)	{ _mm256_maskstore_epi64(p,	make_mask<int64, 4, N>(), t); }
	#else
	template<int N> force_inline void store_vec_mask(const vec<int32, 4>  &t, int32 *p)	{ _mm_maskmoveu_si128(as<int8>(t), make_mask<int32, 4, N>(), (char*)p); }
	template<int N> force_inline void store_vec_mask(const vec<int64, 2>  &t, int64 *p)	{ _mm_maskmoveu_si128(as<int8>(t), make_mask<int64, 2, N>(), (char*)p); }
	#endif
  #endif
  
  	template<int M, typename V, typename E> force_inline auto	store_vec_mask(const V &t, E *p)	{
		static const int N2 = num_elements_v<V> / 2;
		store_vec(as<int>(t.lo), (int*)p);
		store_vec_mask<(M >> N2)>(t.hi, p + N2);
	}

	template<typename E, int N, typename=void> struct store_vec_s;
	template<typename E, int N> struct store_vec_s<E, N, enable_if_t<meta::is_pow2<N>>> {
		static void f(const vec<E, N> &t, E *p) { store_vec(as<int>(t), (int*)p); }
	};
	template<typename E, int N> struct store_vec_s<E, N, enable_if_t<!meta::is_pow2<N>>> {
		typedef sint_for_t<E>		I;
		static const int N2 	= pow2_ceil<N>;
		static void f(const vec<E, N> &t, E *p) { store_vec_mask<(1 << N) - 1>(as<I>(grow<N2>(t)), (I*)p); }
	};
	template<typename V, typename E> void store_vec(const V &t, E *p) {
		store_vec_s<E, num_elements_v<V>>::f(t, p);
	}
#endif

	//-----------------------------------------------------------------------------
	// bit_mask:	bit-per-field
	//-----------------------------------------------------------------------------

#if defined __SSE2__

	SIMD_FUNC int bit_mask(int8x16 m)	{ return _mm_movemask_epi8(m); }
	SIMD_FUNC int bit_mask(int32x4 m)	{ return _mm_movemask_ps(_mm_castsi128_ps(m)); }
	SIMD_FUNC int bit_mask(int64x2 m)	{ return _mm_movemask_pd(_mm_castsi128_pd(m)); }

  #ifdef __AVX__
	SIMD_FUNC int bit_mask(int8x32 m)	{ return _mm256_movemask_epi8(m); }
	SIMD_FUNC int bit_mask(int32x8 m)	{ return _mm256_movemask_ps(_mm256_castsi256_ps(m)); }
	SIMD_FUNC int bit_mask(int64x4 m)	{ return _mm256_movemask_pd(_mm256_castsi256_pd(m)); }
  #endif

#elif defined __ARM_NEON__
	SIMD_FUNC int bit_mask(int8x16 input) {
		uint16x8 high_bits	= (uint16x8)(vshrq_n_u8(input, 7));
		uint32x4 paired16	= (uint32x4)vsraq_n_u16(high_bits, high_bits, 7);
		uint64x2 paired32 	= (uint64x2)vsraq_n_u32(paired16, paired16, 14);
		uint8x16 paired64 	= (uint8x16)vsraq_n_u64(paired32, paired32, 28);
		return vgetq_lane_u8(paired64, 0) | ((int)vgetq_lane_u8(paired64, 8) << 8);
	}
	SIMD_FUNC int bit_mask(int16x8 input) {
		uint32x4 high_bits	= (uint32x4)(vshrq_n_u16(input, 15));
		uint64x2 paired32 	= (uint64x2)vsraq_n_u32(high_bits, high_bits, 15);
		uint8x16 paired64 	= (uint8x16)vsraq_n_u64(paired32, paired32, 30);
		return vgetq_lane_u8(paired64, 0) | ((int)vgetq_lane_u8(paired64, 8) << 4);
	}
	SIMD_FUNC int bit_mask(int32x4 input) {
		uint64x2 high_bits	= (uint64x2)(vshrq_n_u32(input, 31));
		uint8x16 paired64 	= (uint8x16)vsraq_n_u64(high_bits, high_bits, 31);
		return vgetq_lane_u8(paired64, 0) | ((int)vgetq_lane_u8(paired64, 8) << 2);
	}
	SIMD_FUNC int bit_mask(int64x2 input) {
		uint64x2 high_bits	= (uint64x2)(vshrq_n_u64(input, 63));
		return vgetq_lane_u8(high_bits, 0) | ((int)vgetq_lane_u8(high_bits, 8) << 2);
	}
#endif

	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, int>	bit_mask(X x);
	template<int N> SIMD_FUNC int bit_mask(vec<bool, N> m)	{ return m.v; }
	template<int N> SIMD_FUNC int bit_mask(vec<int16,N> m)	{ return bit_mask(to<int8>(m)); }
	template<int N> SIMD_FUNC int bit_mask(vec<uint16,N> m)	{ return bit_mask(to<int8>(m)); }
	template<int N> SIMD_FUNC int bit_mask(vec<uint8,N> m)	{ return bit_mask(as<int8>(m)); }
	template<int N> SIMD_FUNC int bit_mask(vec<uint32,N> m)	{ return bit_mask(as<int32>(m)); }
	template<int N> SIMD_FUNC int bit_mask(vec<uint64,N> m)	{ return bit_mask(as<int64>(m)); }
	template<int N> SIMD_FUNC int bit_mask(vec<char,N> m)	{ return bit_mask(as<int8>(m)); }

//	template<typename X>		SIMD_FUNC int	bit_mask(X x);
	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, int>	bit_mask(X x);

	template<int N, typename X>	SIMD_FUNC enable_if_t<(sizeof(X) > max_register_bits), int>	bit_mask(X x) {
		return bit_mask(x.lo) | (bit_mask(x.hi) << (num_elements_v<X> / 2));
	}
	template<int N, typename X>	SIMD_FUNC enable_if_t<(sizeof(X) <=max_register_bits && !same_v<element_type<X>, bool>), int> bit_mask(X x)	{
		return bit_mask(grow<pow2_ceil<N + 1>>(x)) & ((1ull << N) - 1);
	}
	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, int>	bit_mask(X x)	{ return bit_mask<num_elements_v<X>>(x); }
	template<typename X>		SIMD_FUNC enable_if_t<is_int<X>, int>	bit_mask(X x)	{ return x; }

	//-----------------------------------------------------------------------------
	// any
	//-----------------------------------------------------------------------------

	template<int N, typename T>	SIMD_FUNC oversized_t<T, N, bool>		any(vec<T,N> x)	{ return any(x.lo | x.hi); }
	template<int N, typename T>	SIMD_FUNC not_oversized_t<T, N, bool>	any(vec<T,N> x)	{ return bit_mask(x) != 0; }

	template<int N, typename X>	SIMD_FUNC bool	any(X x)		{ return any<N, element_type<X>>(x); }
	template<typename X>		SIMD_FUNC bool 	any(X x)		{ return any<num_elements_v<X>>(x); }


	template<int N>	SIMD_FUNC bool any(vec<uint8,N> x)	{ return any((vec<char, N>)x); }
	template<int N>	SIMD_FUNC bool any(vec<uint16,N> x)	{ return any((vec<short, N>)x); }
	template<int N>	SIMD_FUNC bool any(vec<uint32,N> x)	{ return any((vec<int, N>)x); }
	template<int N>	SIMD_FUNC bool any(vec<uint64,N> x)	{ return any((vec<int64, N>)x); }

#if defined __SSE2__

	template<int N>	SIMD_FUNC bool any(vec<int16,N> x)	{ return bit_mask(as<char>(x)) & 0xaaaaaaaa; }

#elif defined __ARM_NEON__

	static SIMD_FUNC bool any(int8x8 x)		{ return vmaxv_u8(x) & 0x80; }
	static SIMD_FUNC bool any(int8x4 x)		{ return as_int(x) & 0x80808080; }	//any(x.xyzwxyzw); }
	static SIMD_FUNC bool any(int8x2 x)		{ return as_int(x) & 0x8080; }		//any(x.xyxy); }
	static SIMD_FUNC bool any(int8x3 x)		{ return as_int(x) & 0x808080; }	//any(x.xyzz); }
	static SIMD_FUNC bool any(int8x16 x)	{ return vmaxvq_u8(x) & 0x80; }

	static SIMD_FUNC bool any(int16x4 x)	{ return vmaxv_u16(x) & 0x8000; }
	static SIMD_FUNC bool any(int16x2 x)	{ return any(x.xyxy); }
	static SIMD_FUNC bool any(int16x3 x)	{ return any(x.xyzz); }
	static SIMD_FUNC bool any(int16x8 x)	{ return vmaxvq_u16(x) & 0x8000; }

	static SIMD_FUNC bool any(int32x2 x)	{ return vmaxv_u32(x) & 0x80000000; }
	static SIMD_FUNC bool any(int32x4 x)	{ return vmaxvq_u32(x) & 0x80000000; }
	static SIMD_FUNC bool any(int32x3 x)	{ return any(x.xyzz); }

	static SIMD_FUNC bool any(int64x2 x)	{ return (x.x | x.y) & 0x8000000000000000U; }

#else

	static SIMD_FUNC bool any(int8x8 x)		{ return as_int(x) & 0x8080808080808080; }
	static SIMD_FUNC bool any(int8x4 x)		{ return as_int(x) & 0x80808080; }
	static SIMD_FUNC bool any(int8x2 x)		{ return as_int(x) & 0x8080; }
	static SIMD_FUNC bool any(int8x3 x)		{ return as_int(x) & 0x808080; }
	static SIMD_FUNC bool any(int16x4 x)	{ return as_int(x) & 0x8000800080008000; }
	static SIMD_FUNC bool any(int16x2 x)	{ return as_int(x) & 0x80008000; }
	static SIMD_FUNC bool any(int16x3 x)	{ return as_int(x) & 0x800080008000; }
	static SIMD_FUNC bool any(int32x2 x)	{ return as_int(x) & 0x8000000080000000; }
	static SIMD_FUNC bool any(int64x2 x)	{ return (x.x | x.y) & 0x8000000000000000U; }

#endif

	// contains zero: use bits shortcut for smaller vectors
	template<typename V> inline enable_if_t<(BIT_COUNT<V> >= min_register_bits), bool> contains_zero(V x) {
		return any(x == 0);
	}
	template<typename V> inline enable_if_t<(BIT_COUNT<V> < min_register_bits), bool> contains_zero(V x) {
		typedef uint_t<sizeof(V)>	I;
		return iso::contains_zero<I, BIT_COUNT<element_type<V>>>(reinterpret_cast<I&>(x));
	}

	//-----------------------------------------------------------------------------
	// all
	//-----------------------------------------------------------------------------

	template<int N, typename T>	SIMD_FUNC oversized_t<T, N, bool>		all(vec<T,N> x)	{ return all(x.lo & x.hi); }
	template<int N, typename T>	SIMD_FUNC not_oversized_t<T, N, bool>	all(vec<T,N> x)	{ return bit_mask(x) == ((N < 32 ? (1 << N) : 0) - 1); }

	template<int N, typename X>	SIMD_FUNC bool all(X x)	{ return all<N, element_type<X>>(x); }
	template<typename X>		SIMD_FUNC bool all(X x)	{ return all<num_elements_v<X>>(x); }

	template<int N>	SIMD_FUNC bool all(vec<uint8,N> x)	{ return all((vec<char, N>)x); }
	template<int N>	SIMD_FUNC bool all(vec<uint16,N> x)	{ return all((vec<short, N>)x); }
	template<int N>	SIMD_FUNC bool all(vec<uint32,N> x)	{ return all((vec<int, N>)x); }
	template<int N>	SIMD_FUNC bool all(vec<uint64,N> x)	{ return all((vec<int64, N>)x); }


#if defined __SSE2__
	template<int N>	SIMD_FUNC bool all(vec<int16,N> x)	{ int64 mask = ((1 << (N * 2)) / 3) * 2; return (bit_mask(as<char>(x)) & mask) == mask; }

#elif defined __ARM_NEON__

	static SIMD_FUNC bool all(int8x8 x)		{ return vminv_u8(x) & 0x80;}
	static SIMD_FUNC bool all(int8x4 x)		{ return (as_int(x) & 0x80808080) == 0x80808080;}	//{ return all(x.xyzwxyzw);}
	static SIMD_FUNC bool all(int8x2 x)		{ return (as_int(x) & 0x8080) == 0x8080;}			//{ return all(x.xyxy);}
	static SIMD_FUNC bool all(int8x3 x)		{ return (as_int(x) & 0x808080) == 0x808080;}		//{ return all(x.xyzz);}
	static SIMD_FUNC bool all(int8x16 x)	{ return vminvq_u8(x) & 0x80;}

	static SIMD_FUNC bool all(int16x4 x)	{ return vminv_u16(x) & 0x8000;}
	static SIMD_FUNC bool all(int16x2 x)	{ return all(x.xyxy);}
	static SIMD_FUNC bool all(int16x3 x)	{ return all(x.xyzz);}

	static SIMD_FUNC bool all(int16x8 x)	{ return vminvq_u16(x) & 0x8000;}
	static SIMD_FUNC bool all(int32x2 x)	{ return vminv_u32(x) & 0x80000000;}
	static SIMD_FUNC bool all(int32x4 x)	{ return vminvq_u32(x) & 0x80000000;}
	static SIMD_FUNC bool all(int32x3 x)	{ return all(x.xyzz);}

	static SIMD_FUNC bool all(int64x2 x)	{ return (x.x & x.y) & 0x8000000000000000U;}

#else

	static SIMD_FUNC bool all(int8x8 x)		{ return (as_int(x) & 0x8080808080808080) == 0x8080808080808080;}
	static SIMD_FUNC bool all(int8x4 x)		{ return (as_int(x) & 0x80808080) == 0x80808080;}
	static SIMD_FUNC bool all(int8x2 x)		{ return (as_int(x) & 0x8080) == 0x8080;}
	static SIMD_FUNC bool all(int8x3 x)		{ return (as_int(x) & 0x808080) == 0x808080;}

	static SIMD_FUNC bool all(int16x4 x)	{ return (as_int(x) & 0x8000800080008000) == 0x8000800080008000;}
	static SIMD_FUNC bool all(int16x2 x)	{ return (as_int(x) & 0x80008000) == 0x80008000;}
	static SIMD_FUNC bool all(int16x3 x)	{ return (as_int(x) & 0x800080008000) == 0x800080008000;}

	static SIMD_FUNC bool all(int32x2 x)	{ return (as_int(x) & 0x8000000080000000) == 0x8000000080000000;}
	static SIMD_FUNC bool all(int32x3 x)	{ return (x.x & x.y & x.z) & 0x80000000;}

	static SIMD_FUNC bool all(int64x2 x)	{ return (x.x & x.y) & 0x8000000000000000U;}
	static SIMD_FUNC bool all(int64x3 x)	{ return (x.x & x.y & x.z) & 0x8000000000000000U;}

#endif

	// bitselect: select bits of x or y based on a mask
	template<typename T, int N, typename M> static SIMD_FUNC vec<T,N> bitselect(vec<T,N> x, vec<T,N> y, M mask) {
		return as<T>((as<iso::sint_for_t<T>>(x) & ~mask) | (as<iso::sint_for_t<T>>(y) & mask));
	}

	template<typename X, typename Y, typename M> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>> bitselect(X x, Y y, M mask) {
		return bitselect<element_type<X>, num_elements_v<X>>(x, y, mask);
	}

#ifdef __SSE4_1__
	SIMD_FUNC int8x16 vblend(int8x16 x, int8x16 y, int8x16 m) { return _mm_blendv_epi8(x, y, m); }
  #ifdef __AVX2__
	SIMD_FUNC int8x32 vblend(int8x32 x, int8x32 y, int8x32 m) { return _mm256_blendv_epi8(x, y, m); }
  #endif
	template<typename T>	SIMD_FUNC auto	vblend(T x, T y, T m);

	template<int N>	SIMD_FUNC normalsized_t<int8, N, vec<int8, N>>	vblend(vec<int8, N> x, vec<int8, N> y, vec<int8, N> m)	{
		return vblend(x, y, m);
	}
	template<int N>	SIMD_FUNC oversized_t<int8, N, vec<int8, N>>	vblend(vec<int8, N> x, vec<int8, N> y, vec<int8, N> m)	{
		return simd_lo_hi(vblend(x.lo, y.lo, m.lo), vblend(x.hi, y.hi, m.hi));
	}
	template<int N>	SIMD_FUNC undersized_t<int8, N, vec<int8, N>>	vblend(vec<int8, N> x, vec<int8, N> y, vec<int8, N> m)	{
		static const int N2 = pow2_ceil<N + 1>;
		return shrink<N>(vblend(grow<N2>(x), grow<N2>(y), grow<N2>(m)));
	}
	template<typename T>	SIMD_FUNC auto	vblend(T x, T y, T m)	{ return vblend<num_elements_v<T>>(x, y, m); }

	template<typename X, typename M> static SIMD_FUNC X _boolselect(X x, X y, M mask) {
		return as<element_type<X>>(vblend(as<int8>(x), as<int8>(y), as<int8>(make_mask<sint_for_t<element_type<X>>, num_elements_v<X>>(mask))));
	}
#else
	template<typename X, typename M> static SIMD_FUNC X _boolselect(X x, X y, M mask) {
		return bitselect(x, y, make_mask<sint_for_t<element_type<X>>, num_elements_v<X>>(mask));
	}
#endif

	template<typename X, typename Y, typename M> static SIMD_FUNC enable_if_t< is_vec<X>, vec_type<X>> select(M mask, X x, Y y) {
		return _boolselect((vec_type<X>)y, (vec_type<X>)x, mask);
	}
	template<typename X, typename Y, typename M> static SIMD_FUNC enable_if_t<!is_vec<X>, vec_type<Y>> select(M mask, X x, Y y) {
		return _boolselect((vec_type<Y>)y, (vec_type<Y>)x, mask);
	}

	//-----------------------------------------------------------------------------
	// abs
	//-----------------------------------------------------------------------------

	template<int N> static SIMD_FUNC vec<float,N>	abs(vec<float,N> x)		{ return as<float>(as<int>(x) & 0x7fffffff); }
	template<int N> static SIMD_FUNC vec<double,N>	abs(vec<double,N> x)	{ return as<double>(as<int64>(x) & 0x7fffffffffffffffull); }

#if defined __SSE4_1__
	static SIMD_FUNC int8x16		abs(int8x16 x)		{ return _mm_abs_epi8(x); }
	static SIMD_FUNC int16x8		abs(int16x8 x)		{ return _mm_abs_epi16(x); }
	static SIMD_FUNC int32x4		abs(int32x4 x)		{ return _mm_abs_epi32(x); }
#ifdef	__AVX512VL__
	static SIMD_FUNC int64x2		abs(int64x2 x)		{ return _mm_abs_epi64(x); }
	static SIMD_FUNC int64x4		abs(int64x4 x)		{ return _mm256_abs_epi64(x); }
#endif
#if defined __AVX2__
	static SIMD_FUNC int8x32		abs(int8x32 x)		{ return _mm256_abs_epi8(x); }
	static SIMD_FUNC int16x16		abs(int16x16 x)		{ return _mm256_abs_epi16(x); }
	static SIMD_FUNC int32x8		abs(int32x8 x)		{ return _mm256_abs_epi32(x); }
#endif

#if defined __AVX512BW__
	static SIMD_FUNC int8x64		abs(int8x64 x)		{ return _mm512_abs_epi8(x); }
	static SIMD_FUNC int16x32		abs(int16x32 x)		{ return _mm512_abs_epi16(x); }
	static SIMD_FUNC int32x16		abs(int32x16 x)		{ return _mm512_abs_epi32(x); }
	static SIMD_FUNC int64x8		abs(int64x8 x)		{ return _mm512_abs_epi64(x); }
#endif

#elif defined __ARM_NEON__

	static SIMD_FUNC int8x16		abs(int8x16 x)		{ return vabsq_s8(x); }
	static SIMD_FUNC int8x8			abs(int8x8 x)		{ return vabs_s8(x); }
	static SIMD_FUNC int16x8		abs(int16x8 x)		{ return vabsq_s16(x); }
	static SIMD_FUNC int16x4		abs(int16x4 x)		{ return vabs_s16(x); }
	static SIMD_FUNC int32x4		abs(int32x4 x)		{ return vabsq_s32(x); }
	static SIMD_FUNC int32x2		abs(int32x2 x)		{ return vabs_s32(x); }
	static SIMD_FUNC int64x2		abs(int64x2 x)		{ return vabsq_s64(x); }

#endif

#if defined __SSE4_1__ || defined __ARM_NEON__
	template<int N, typename X>	SIMD_FUNC oversized_t<element_type<X>, N, X>	abs(X x)	{ return simd_lo_hi(abs(x.lo), abs(x.hi)); }
	template<int N, typename X>	SIMD_FUNC undersized_t<element_type<X>, N, X>	abs(X x)	{ return shrink<N>(abs(grow<pow2_ceil<N + 1>>(x))); }
#else
	template<int N, typename V> static SIMD_FUNC V abs(V x) {
		V mask = x >> (sizeof(element_type<V>) * 8 - 1);
		return (x ^ mask) - mask;
	}
#endif
	template<typename X>		static SIMD_FUNC auto	abs(X x)->enable_if_t<is_vec<X>, decltype(abs<num_elements_v<X>>(x))>	{ return abs<num_elements_v<X>>(x); }


	//-----------------------------------------------------------------------------
	// min/max
	//-----------------------------------------------------------------------------

#if defined __SSE4_1__
	static SIMD_FUNC int8x16	min(int8x16 x, int8x16 y)		{ return _mm_min_epi8(x, y); }
	static SIMD_FUNC int8x16	max(int8x16 x, int8x16 y)		{ return _mm_max_epi8(x, y); }
	static SIMD_FUNC uint8x16	min(uint8x16 x, uint8x16 y)		{ return _mm_min_epu8(x, y); }
	static SIMD_FUNC uint8x16	max(uint8x16 x, uint8x16 y)		{ return _mm_max_epu8(x, y); }
	static SIMD_FUNC int16x8	min(int16x8 x, int16x8 y)		{ return _mm_min_epi16(x, y); }
	static SIMD_FUNC int16x8	max(int16x8 x, int16x8 y)		{ return _mm_max_epi16(x, y); }
	static SIMD_FUNC uint16x8	min(uint16x8 x, uint16x8 y)		{ return _mm_min_epu16(x, y); }
	static SIMD_FUNC uint16x8	max(uint16x8 x, uint16x8 y)		{ return _mm_max_epu16(x, y); }
	static SIMD_FUNC int32x4	min(int32x4 x, int32x4 y)		{ return _mm_min_epi32(x, y); }
	static SIMD_FUNC int32x4	max(int32x4 x, int32x4 y)		{ return _mm_max_epi32(x, y); }
	static SIMD_FUNC uint32x4	min(uint32x4 x, uint32x4 y)		{ return _mm_min_epu32(x, y); }
	static SIMD_FUNC uint32x4	max(uint32x4 x, uint32x4 y)		{ return _mm_max_epu32(x, y); }
	static SIMD_FUNC double2	min(double2 x, double2 y)		{ return _mm_min_pd(x, y); }
	static SIMD_FUNC double2	max(double2 x, double2 y)		{ return _mm_max_pd(x, y); }

#if defined __AVX__
	static SIMD_FUNC float8		min(float8 x, float8 y)			{ return _mm256_min_ps(x, y); }
	static SIMD_FUNC float8		max(float8 x, float8 y)			{ return _mm256_max_ps(x, y); }
	static SIMD_FUNC double4	min(double4 x, double4 y)		{ return _mm256_min_pd(x, y); }
	static SIMD_FUNC double4	max(double4 x, double4 y)		{ return _mm256_max_pd(x, y); }
#endif

#if defined __AVX2__
	static SIMD_FUNC int8x32	min(int8x32 x, int8x32 y)		{ return _mm256_min_epi8(x, y); }
	static SIMD_FUNC int8x32	max(int8x32 x, int8x32 y)		{ return _mm256_max_epi8(x, y); }
	static SIMD_FUNC uint8x32	min(uint8x32 x, uint8x32 y)		{ return _mm256_min_epu8(x, y); }
	static SIMD_FUNC uint8x32	max(uint8x32 x, uint8x32 y)		{ return _mm256_max_epu8(x, y); }
	static SIMD_FUNC int16x16	min(int16x16 x, int16x16 y)		{ return _mm256_min_epi16(x, y); }
	static SIMD_FUNC int16x16	max(int16x16 x, int16x16 y)		{ return _mm256_max_epi16(x, y); }
	static SIMD_FUNC uint16x16	min(uint16x16 x, uint16x16 y)	{ return _mm256_min_epu16(x, y); }
	static SIMD_FUNC uint16x16	max(uint16x16 x, uint16x16 y)	{ return _mm256_max_epu16(x, y); }
	static SIMD_FUNC int32x8	min(int32x8 x, int32x8 y)		{ return _mm256_min_epi32(x, y); }
	static SIMD_FUNC int32x8	max(int32x8 x, int32x8 y)		{ return _mm256_max_epi32(x, y); }
	static SIMD_FUNC uint32x8	min(uint32x8 x, uint32x8 y)		{ return _mm256_min_epu32(x, y); }
	static SIMD_FUNC uint32x8	max(uint32x8 x, uint32x8 y)		{ return _mm256_max_epu32(x, y); }
#endif

#if defined __AVX512F__
	static SIMD_FUNC int32x16	min(int32x16 x, int32x16 y)		{ return _mm512_min_epi32(x, y);}
	static SIMD_FUNC int32x16	max(int32x16 x, int32x16 y)		{ return _mm512_max_epi32(x, y);}
	static SIMD_FUNC uint32x16	min(uint32x16 x, uint32x16 y)	{ return _mm512_min_epu32(x, y); }
	static SIMD_FUNC uint32x16	max(uint32x16 x, uint32x16 y)	{ return _mm512_max_epu32(x, y); }
	static SIMD_FUNC int64x8	min(int64x8 x, int64x8 y)		{ return _mm512_min_epi64(x, y); }
	static SIMD_FUNC int64x8	max(int64x8 x, int64x8 y)		{ return _mm512_max_epi64(x, y); }
	static SIMD_FUNC uint64x8	min(uint64x8 x, uint64x8 y)		{ return _mm512_min_epu64(x, y); }
	static SIMD_FUNC uint64x8	max(uint64x8 x, uint64x8 y)		{ return _mm512_max_epu64(x, y); }
	static SIMD_FUNC float4		min(float4 x, float4 y)			{ return _mm_range_ps(x, y, 4); }
	static SIMD_FUNC float4		max(float4 x, float4 y)			{ return _mm_range_ps(x, y, 4); }
	static SIMD_FUNC floatx16	min(floatx16 x, floatx16 y)		{ return _mm512_min_ps(x, y); }
	static SIMD_FUNC floatx16	max(floatx16 x, floatx16 y)		{ return _mm512_max_ps(x, y); }
	static SIMD_FUNC double8	min(double8 x, double8 y)		{ return _mm512_min_pd(x, y); }
	static SIMD_FUNC double8	max(double8 x, double8 y)		{ return _mm512_max_pd(x, y); }
#else
	static SIMD_FUNC float4		min(float4 x, float4 y)			{ return _mm_min_ps(x, y); }
	static SIMD_FUNC float4		max(float4 x, float4 y)			{ return _mm_max_ps(x, y); }
#endif

#if defined __AVX512VL__
	static SIMD_FUNC int64x2	min(int64x2 x, int64x2 y)		{ return _mm_min_epi64(x, y); }
	static SIMD_FUNC int64x2	max(int64x2 x, int64x2 y)		{ return _mm_max_epi64(x, y); }
	static SIMD_FUNC uint64x2	min(uint64x2 x, uint64x2 y)		{ return _mm_min_epu64(x, y); }
	static SIMD_FUNC uint64x2	max(uint64x2 x, uint64x2 y)		{ return _mm_max_epu64(x, y); }
	static SIMD_FUNC int64x4	min(int64x4 x, int64x4 y)		{ return _mm256_min_epi64(x, y); }
	static SIMD_FUNC int64x4	max(int64x4 x, int64x4 y)		{ return _mm256_max_epi64(x, y); }
	static SIMD_FUNC uint64x4	min(uint64x4 x, uint64x4 y)		{ return _mm256_min_epu64(x, y); }
	static SIMD_FUNC uint64x4	max(uint64x4 x, uint64x4 y)		{ return _mm256_max_epu64(x, y); }
#else
	static SIMD_FUNC int64x2	min(int64x2 x, int64x2 y)		{ return select(y > x, x, y); }
	static SIMD_FUNC int64x2	max(int64x2 x, int64x2 y)		{ return select(y > x, y, x); }
	static SIMD_FUNC uint64x2	min(uint64x2 x, uint64x2 y)		{ return select(y > x, x, y); }
	static SIMD_FUNC uint64x2	max(uint64x2 x, uint64x2 y)		{ return select(y > x, y, x); }
  #if defined __AVX2__
	static SIMD_FUNC int64x4	min(int64x4 x, int64x4 y)		{ return select(y > x, x, y); }
	static SIMD_FUNC int64x4	max(int64x4 x, int64x4 y)		{ return select(y > x, y, x); }
	static SIMD_FUNC uint64x4	min(uint64x4 x, uint64x4 y)		{ return select(y > x, x, y); }
	static SIMD_FUNC uint64x4	max(uint64x4 x, uint64x4 y)		{ return select(y > x, y, x); }
  #endif
#endif

#if defined __AVX512BW__
	static SIMD_FUNC int8x64	min(int8x64 x, int8x64 y)		{ return _mm512_min_epi8(x, y); }
	static SIMD_FUNC int8x64	max(int8x64 x, int8x64 y)		{ return _mm512_max_epi8(x, y); }
	static SIMD_FUNC uint8x64	min(uint8x64 x, uint8x64 y)		{ return _mm512_min_epu8(x, y); }
	static SIMD_FUNC uint8x64	max(uint8x64 x, uint8x64 y)		{ return _mm512_max_epu8(x, y); }
	static SIMD_FUNC int16x32	min(int16x32 x, int16x32 y)		{ return _mm512_min_epi16(x, y); }
	static SIMD_FUNC int16x32	max(int16x32 x, int16x32 y)		{ return _mm512_max_epi16(x, y); }
	static SIMD_FUNC uint16x32	min(uint16x32 x, uint16x32 y)	{ return _mm512_min_epu16(x, y); }
	static SIMD_FUNC uint16x32	max(uint16x32 x, uint16x32 y)	{ return _mm512_max_epu16(x, y); }
#endif

#elif defined __ARM_NEON__
	static SIMD_FUNC int8x16	min(int8x16 x, int8x16 y)		{ return vminq_s8(x, y); }
	static SIMD_FUNC int8x16	max(int8x16 x, int8x16 y)		{ return vmaxq_s8(x, y); }
	static SIMD_FUNC int8x8		min(int8x8 x, int8x8 y)			{ return vmin_s8(x, y); }
	static SIMD_FUNC int8x8		max(int8x8 x, int8x8 y)			{ return vmax_s8(x, y); }
	static SIMD_FUNC uint8x16	min(uint8x16 x, uint8x16 y)		{ return vminq_u8(x, y); }
	static SIMD_FUNC uint8x16	max(uint8x16 x, uint8x16 y)		{ return vmaxq_u8(x, y); }
	static SIMD_FUNC uint8x8	min(uint8x8 x, uint8x8 y)		{ return vmin_u8(x, y); }
	static SIMD_FUNC uint8x8	max(uint8x8 x, uint8x8 y)		{ return vmax_u8(x, y); }
	static SIMD_FUNC int16x8	min(int16x8 x, int16x8 y)		{ return vminq_s16(x, y); }
	static SIMD_FUNC int16x8	max(int16x8 x, int16x8 y)		{ return vmaxq_s16(x, y); }
	static SIMD_FUNC int16x4	min(int16x4 x, int16x4 y)		{ return vmin_s16(x, y); }
	static SIMD_FUNC int16x4	max(int16x4 x, int16x4 y)		{ return vmax_s16(x, y); }
	static SIMD_FUNC uint16x8	min(uint16x8 x, uint16x8 y)		{ return vminq_u16(x, y); }
	static SIMD_FUNC uint16x8	max(uint16x8 x, uint16x8 y)		{ return vmaxq_u16(x, y); }
	static SIMD_FUNC uint16x4	min(uint16x4 x, uint16x4 y)		{ return vmin_u16(x, y); }
	static SIMD_FUNC uint16x4	max(uint16x4 x, uint16x4 y)		{ return vmax_u16(x, y); }
	static SIMD_FUNC int32x4	min(int32x4 x, int32x4 y)		{ return vminq_s32(x, y); }
	static SIMD_FUNC int32x4	max(int32x4 x, int32x4 y)		{ return vmaxq_s32(x, y); }
	static SIMD_FUNC int32x2	min(int32x2 x, int32x2 y)		{ return vmin_s32(x, y); }
	static SIMD_FUNC int32x2	max(int32x2 x, int32x2 y)		{ return vmax_s32(x, y); }
	static SIMD_FUNC uint32x4	min(uint32x4 x, uint32x4 y)		{ return vminq_u32(x, y); }
	static SIMD_FUNC uint32x4	max(uint32x4 x, uint32x4 y)		{ return vmaxq_u32(x, y); }
	static SIMD_FUNC uint32x2	min(uint32x2 x, uint32x2 y)		{ return vmin_u32(x, y); }
	static SIMD_FUNC uint32x2	max(uint32x2 x, uint32x2 y)		{ return vmax_u32(x, y); }
	static SIMD_FUNC float4		min(float4 x, float4 y)			{ return vminnmq_f32(x, y); }
	static SIMD_FUNC float4		max(float4 x, float4 y)			{ return vmaxnmq_f32(x, y); }
	static SIMD_FUNC float2		min(float2 x, float2 y)			{ return vminnm_f32(x, y); }
	static SIMD_FUNC float2		max(float2 x, float2 y)			{ return vmaxnm_f32(x, y); }
  #if defined __arm64__
	static SIMD_FUNC double2	min(double2 x, double2 y)		{ return vminnmq_f64(x, y); }
	static SIMD_FUNC double2	max(double2 x, double2 y)		{ return vmaxnmq_f64(x, y); }
  #endif
#endif

#if defined __ARM_NEON__ || defined __SSE4_1__
	template<typename X, typename Y, typename = enable_if_t<is_vec<X>||is_vec<Y>>>	SIMD_FUNC auto	min(X x, Y y);
	template<typename X, typename Y, typename = enable_if_t<is_vec<X>||is_vec<Y>>>	SIMD_FUNC auto	max(X x, Y y);

	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<T,N>>	min1(vec<T,N> x, vec<T,N> y)	{ return simd_lo_hi(min(x.lo, y.lo), min(x.hi, y.hi)); }
	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<T,N>>	max1(vec<T,N> x, vec<T,N> y)	{ return simd_lo_hi(max(x.lo, y.lo), max(x.hi, y.hi)); }
	template<typename T, int N>	SIMD_FUNC undersized_t<T, N, vec<T,N>>	min1(vec<T,N> x, vec<T,N> y)	{ return shrink<N>(min(grow<pow2_ceil<N + 1>>(x), grow<pow2_ceil<N + 1>>(y))); }
	template<typename T, int N>	SIMD_FUNC undersized_t<T, N, vec<T,N>>	max1(vec<T,N> x, vec<T,N> y)	{ return shrink<N>(max(grow<pow2_ceil<N + 1>>(x), grow<pow2_ceil<N + 1>>(y))); }
	template<typename T, int N>	SIMD_FUNC normalsized_t<T, N, vec<T,N>>	min1(vec<T,N> x, vec<T,N> y)	{ return select(y < x, y, x); }
	template<typename T, int N>	SIMD_FUNC normalsized_t<T, N, vec<T,N>>	max1(vec<T,N> x, vec<T,N> y)	{ return select(y < x, x, y); }

	template<typename X, typename Y, typename>	SIMD_FUNC auto	min(X x, Y y) { return min1<element_type<X>, num_elements_v<X>>(x, y); }
	template<typename X, typename Y, typename>	SIMD_FUNC auto	max(X x, Y y) { return max1<element_type<X>, num_elements_v<X>>(x, y); }
#else
	template<typename X, typename Y, typename = enable_if_t<is_vec<X>||is_vec<Y>>>	SIMD_FUNC auto	min(X x, Y y) {	return select(y < x, y, x); }
	template<typename X, typename Y, typename = enable_if_t<is_vec<X>||is_vec<Y>>>	SIMD_FUNC auto	max(X x, Y y) { return select(y < x, x, y); }
#endif

	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, int>	min_component_index(X x);
	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, int>	max_component_index(X x);

	template<typename T>		SIMD_FUNC int	min_component_index(vec<T, 2> v)	{ return int(v.y < v.x); }
	template<typename T>		SIMD_FUNC int	min_component_index(vec<T, 3> v)	{ return (0x1a10 >> (bit_mask(v < v.yzx) * 2)) & 3; }
	template<typename T>		SIMD_FUNC int	min_component_index(vec<T, 4> v)	{ return (0x1aff1a10 >> (bit_mask(v <= v.yzwx & v <= v.zwxy) * 2)) & 3; }
	template<typename T, int N>	SIMD_FUNC int	min_component_index(vec<T, N> v) 	{ int i = max_component_index(max(v.lo, v.hi)); return v[i + N / 2] > v[i] ? i + N / 2 : i; }

	template<typename T>		SIMD_FUNC int	max_component_index(vec<T, 2> v)	{ return int(v.y > v.x); }
	template<typename T>		SIMD_FUNC int	max_component_index(vec<T, 3> v)	{ return (0x1a10 >> (bit_mask(v > v.yzx) * 2)) & 3; }
	template<typename T>		SIMD_FUNC int	max_component_index(vec<T, 4> v)	{ return (0x1aff1a10 >> (bit_mask(v >= v.yzwx & v >= v.zwxy) * 2)) & 3; }
	template<typename T, int N>	SIMD_FUNC int	max_component_index(vec<T, N> v) 	{ int i = max_component_index(max(v.lo, v.hi)); return v[i + N / 2] > v[i] ? i + N / 2 : i; }
	template<typename T, typename X>	SIMD_FUNC int	max_component_index(X x)	{ return max_component_index<T, num_elements_v<X>>(x); }

	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, int>	min_component_index(X x)	{ return min_component_index<element_type<X>>(x); }
	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, int>	max_component_index(X x)	{ return max_component_index<element_type<X>>(x); }

	//-----------------------------------------------------------------------------
	// saturated conversion
	//-----------------------------------------------------------------------------

	template<typename F, typename T, typename = void> struct to_sat_s;
	template<typename T, typename F>		SIMD_FUNC vec<T, num_elements_v<F>>	to_sat(F f)		{ return to_sat_s<element_type<F>, T>::f(f); }

	// no saturation
	template<typename F, typename T> struct to_sat_s<F, T, enable_if_t<is_float<T> || (is_int<F> && (same_v<F, T> || sizeof(F) < sizeof(T)))>> {
		template<typename V> static SIMD_FUNC auto f(V v) { return to<T>(v); }
	};
	// saturate signed F
	template<typename F, typename T> struct to_sat_s<F, T, enable_if_t<(is_int<T> && is_signed<F> && (!is_int<F> || sizeof(F) > sizeof(T)))>> {
		template<typename V> static SIMD_FUNC auto f(V v) { return to<T>(min(max(v, T(minimum)), T(maximum))); }
	};
	// saturate signed F to unsigned F
	template<typename F> struct to_sat_s<F, unsigned_t<F>> {
		template<typename V> static SIMD_FUNC auto f(V v) { return as<unsigned_t<F>>(max(v, zero)); }
	};
	// saturate unsigned F
	template<typename F, typename T> struct to_sat_s<F, T, enable_if_t<(is_int<T> && !is_signed<F> && (!is_int<F> || sizeof(F) >= sizeof(T)))>> {
		template<typename V> static SIMD_FUNC auto f(V v) { return to<T>(min(v, T(maximum))); }
	};

	template<typename F, typename T> struct to_sat_s_via { template<typename V> static SIMD_FUNC auto f(V v); };

#ifdef __SSE4_1__
	template<typename T, typename F, int N> SIMD_FUNC oversized_t<smallest_t<T, F>, N, vec<T, N>>	to_sat_split(vec<F, N> v)	{ return simd_lo_hi(to_sat<T>(v.lo), to_sat<T>(v.hi)); }
	template<typename T, typename F, int N> SIMD_FUNC undersized_t<biggest_t<T, F>, N, vec<T, N>>	to_sat_split(vec<F, N> v)	{ return shrink<N>(to_sat<T>(grow<pow2_ceil<N + 1>>(v))); }
	template<typename T, typename F, int N> SIMD_FUNC enable_if_t<!is_oversized<smallest_t<T, F>, N> && !is_undersized<biggest_t<T, F>,  N>, vec<T,N>>	to_sat_split(vec<F, N> v) {
		return to_sat<T>(v);
	}
	template<typename F, typename T> struct to_sat_s_split {
		template<typename V> static SIMD_FUNC auto f(V v) { return to_sat_split<T>(v); }
	};
	template<> struct to_sat_s<int32, int16>	: to_sat_s_split<int32, int16> {};
	template<> struct to_sat_s<int32, uint16>	: to_sat_s_split<int32, uint16> {};

	// saturated conversion from 32 bit to 8 bit goes via int16
	template<> struct to_sat_s<int32, int8>		: to_sat_s_via<int16, int8> {};
	template<> struct to_sat_s<int32, uint8>	: to_sat_s_via<int16, uint8> {};
	template<> struct to_sat_s<uint32, int8>	: to_sat_s_via<int16, int8> {};
	template<> struct to_sat_s<uint32, uint8>	: to_sat_s_via<int16, uint8> {};
	template<> struct to_sat_s<uint32, int16>	: to_sat_s_via<int32, int16> {};
	template<> struct to_sat_s<uint32, uint16>	: to_sat_s_via<int32, uint16> {};

	template<> SIMD_FUNC int16x4	to_sat<int16>(int32x4 x)	{ return shrink<4>(int16x8(_mm_packs_epi32(x, x))); }
	template<> SIMD_FUNC uint16x4	to_sat<uint16>(int32x4 x)	{ return shrink<4>(uint16x8(_mm_packus_epi32(x, x))); }
	template<> SIMD_FUNC int8x8		to_sat<int8>(int16x8 x)		{ return shrink<8>(int8x16(_mm_packs_epi16(x, x))); }
	template<> SIMD_FUNC uint8x8	to_sat<uint8>(int16x8 x)	{ return shrink<8>(uint8x16(_mm_packus_epi16(x, x))); }

	template<> SIMD_FUNC int16x8	to_sat<int16>(int32x8 x)	{ return _mm_packs_epi32(x.lo, x.hi); }
	template<> SIMD_FUNC uint16x8	to_sat<uint16>(int32x8 x)	{ return _mm_packus_epi32(x.lo, x.hi); }
	template<> SIMD_FUNC int8x16	to_sat<int8>(int16x16 x)	{ return _mm_packs_epi16(x.lo, x.hi); }
	template<> SIMD_FUNC uint8x16	to_sat<uint8>(int16x16 x)	{ return _mm_packus_epi16(x.lo, x.hi); }

#ifdef __AVX2__
	template<> SIMD_FUNC int16x16	to_sat<int16>(int32x16 x)	{ return _mm256_packs_epi32(x.lo, x.hi); }
	template<> SIMD_FUNC uint16x16	to_sat<uint16>(int32x16 x)	{ return _mm256_packus_epi32(x.lo, x.hi); }
	template<> SIMD_FUNC int8x32	to_sat<int8>(int16x32 x)	{ return _mm256_packs_epi16(x.lo, x.hi); }
	template<> SIMD_FUNC uint8x32	to_sat<uint8>(int16x32 x)	{ return _mm256_packus_epi16(x.lo, x.hi); }
#endif

#ifdef __AVX512F__
	template<> SIMD_FUNC int8x4		to_sat<int8>(int32x4 x)		{ return shrink<4>(int8x8(_mm_cvtsepi32_epi8(x))); }
	template<> SIMD_FUNC int8x8		to_sat<int8>(int32x8 x)		{ return shrink<8>(int8x16(_mm256_cvtsepi32_epi8(x))); }
	template<> SIMD_FUNC int8x16	to_sat<int8>(int32x16 x)	{ return shrink<16>(int8x32(_mm512_cvtsepi32_epi8(x))); }

	template<> SIMD_FUNC int16x32	to_sat<int16>(int32x32 x)	{ return _mm512_packs_epi32(x.lo, x.hi); }
	template<> SIMD_FUNC uint16x32	to_sat<uint16>(int32x32 x)	{ return _mm512_packus_epi32(x.lo, x.hi); }
	template<> SIMD_FUNC int8x64	to_sat<int8>(int16x64 x)	{ return _mm512_packs_epi16(x.lo, x.hi); }
	template<> SIMD_FUNC uint8x64	to_sat<uint8>(int16x64 x)	{ return _mm512_packus_epi16(x.lo, x.hi); }
#endif

#elif defined __ARM_NEON__
	template<typename T, typename F, int N> SIMD_FUNC oversized_t<biggest_t<T, F>, N, vec<T, N>>	to_sat_split(vec<F, N> v) { return simd_lo_hi(to_sat<T>(v.lo), to_sat<T>(v.hi)); }
	template<typename T, typename F, int N> SIMD_FUNC undersized_t<biggest_t<T, F>, N, vec<T, N>>	to_sat_split(vec<F, N> v) { return shrink<N>(to_sat<T>(grow<pow2_ceil<N + 1>>(v))); }
	template<typename T, typename F, int N> SIMD_FUNC normalsized_t<biggest_t<T, F>, N, vec<T,N>>	to_sat_split(vec<F, N> v) { return to_sat<T>(v); }
	//template<typename T, typename F> SIMD_FUNC auto	to_sat_split(F v) { return to_sat_split<T, element_type<F>, num_elements_v<F>>(v); }

	template<typename F, typename T> struct to_sat_s_split {
		template<typename V> static SIMD_FUNC auto f(V v) { return to_sat_split<T>(v); }
	};
	template<> struct to_sat_s<int16, int8>		: to_sat_s_split<int16, int8> {};
	template<> struct to_sat_s<int32, int16>	: to_sat_s_split<int32, int16> {};
	template<> struct to_sat_s<int64, int32>	: to_sat_s_split<int64, int32> {};

	template<> struct to_sat_s<uint16, uint8>	: to_sat_s_split<uint16, uint8> {};
	template<> struct to_sat_s<uint32, uint16>	: to_sat_s_split<uint32, uint16> {};
	template<> struct to_sat_s<uint64, uint32>	: to_sat_s_split<uint64, uint32> {};

	template<> struct to_sat_s<int16, uint8>	: to_sat_s_split<int16, uint8> {};
	template<> struct to_sat_s<int32, uint16>	: to_sat_s_split<int32, uint16> {};
	template<> struct to_sat_s<int64, uint32>	: to_sat_s_split<int64, uint32> {};

	// saturated conversion from float goes via 32 bit int
	template<typename T> struct to_sat_s<float, T>	: to_sat_s_via<int32, T> {};
	template<typename T> struct to_sat_s<double, T>	: to_sat_s_via<int32, T> {};
	
	// saturated conversion from 32 bit to 8 bit goes via int16
	template<> struct to_sat_s<int32, int8>		: to_sat_s_via<int16, int8> {};
	template<> struct to_sat_s<int32, uint8>	: to_sat_s_via<uint16, uint8> {};
	template<> struct to_sat_s<uint32, int8>	: to_sat_s_via<int16, int8> {};
	template<> struct to_sat_s<uint32, uint8>	: to_sat_s_via<uint16, uint8> {};

	// saturated conversion from 64 bit goes via int32
	template<typename T> struct to_sat_s<int64, T>		: to_sat_s_via<int32, T> {};
	template<typename T> struct to_sat_s<uint64, T>		: to_sat_s_via<uint32, T> {};

	template<> SIMD_FUNC int8x8		to_sat<int8>(int16x8 x)		{ return vqmovn_s16(x); }
	template<> SIMD_FUNC int16x4	to_sat<int16>(int32x4 x)	{ return vqmovn_s32(x); }
	template<> SIMD_FUNC int32x2	to_sat<int32>(int64x2 x)	{ return vqmovn_s64(x); }

	template<> SIMD_FUNC uint8x8	to_sat<uint8>(uint16x8 x)	{ return vqmovn_u16(x); }
	template<> SIMD_FUNC uint16x4	to_sat<uint16>(uint32x4 x)	{ return vqmovn_u32(x); }
	template<> SIMD_FUNC uint32x2	to_sat<uint32>(uint64x2 x)	{ return vqmovn_u64(x); }

	template<> SIMD_FUNC uint8x8	to_sat<uint8>(int16x8 x)	{ return vqmovun_s16(x); }
	template<> SIMD_FUNC uint16x4	to_sat<uint16>(int32x4 x)	{ return vqmovun_s32(x); }
	template<> SIMD_FUNC uint32x2	to_sat<uint32>(int64x2 x)	{ return vqmovun_s64(x); }
#endif

#if defined __SSE4_1__ || defined __ARM_NEON__
	template<typename T> struct to_sat_s_via<T, T> {
		template<typename V> static SIMD_FUNC auto f(V v) {
			return to_sat_split<T, element_type<V>, num_elements_v<V>>(v);
		}
	};
	template<typename F, typename T> template<typename V> SIMD_FUNC auto to_sat_s_via<F,T>::f(V v) {
		static const int N = num_elements_v<V>;
		return to_sat_split<T, F, N>(to_sat_split<F, element_type<V>, N>(v));
	}
//	template<typename T, typename F>		SIMD_FUNC vec<T, num_elements_v<F>>	to_sat(F f)		{ return to_sat_split<T, element_type<F>, num_elements_v<F>>(f); }
#endif

	//-----------------------------------------------------------------------------
	// copysign
	//-----------------------------------------------------------------------------

	template<int N> static SIMD_FUNC vec<float,N>		copysign(vec<float,N> x, vec<float,N> y)	{ return bitselect(x, y, 0x80000000); }
	template<int N> static SIMD_FUNC vec<double,N>		copysign(vec<double,N> x, vec<double,N> y)	{ return bitselect(x, y, 0x8000000000000000L); }

	template<typename X, typename Y, typename=enable_if_t<is_vec<X>||is_vec<Y>>> static SIMD_FUNC auto	copysign(X x, Y y)	{
		return copysign<num_elements_v<X> == 1 ? num_elements_v<Y> : num_elements_v<X>>(x, y);
	}

	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, X>	sign1(X x)	{ return copysign(one, x); }
	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, X>	sign(X x)	{ return select(x == zero, x, copysign(one, x)); }

	//-----------------------------------------------------------------------------
	// trunc/ceil/floor/frac
	//-----------------------------------------------------------------------------

#if defined __SSE4_1__
	static SIMD_FUNC float4		trunc(float4 x)		{ return _mm_round_ps(x, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC float4		ceil(float4 x)		{ return _mm_round_ps(x, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC float4		floor(float4 x)		{ return _mm_round_ps(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC float4		round(float4 x)		{ return _mm_round_ps(x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }

	static SIMD_FUNC double2	trunc(double2 x)	{ return _mm_round_pd(x, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double2	ceil(double2 x)		{ return _mm_round_pd(x, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double2	floor(double2 x)	{ return _mm_round_pd(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double2	round(double2 x)	{ return _mm_round_pd(x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }

#if defined __AVX__
	static SIMD_FUNC float8		trunc(float8 x)		{ return _mm256_round_ps(x, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC float8		ceil(float8 x)		{ return _mm256_round_ps(x, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC float8		floor(float8 x)		{ return _mm256_round_ps(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC float8		round(float8 x)		{ return _mm256_round_ps(x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }

	static SIMD_FUNC double4	trunc(double4 x)	{ return _mm256_round_pd(x, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double4	ceil(double4 x)		{ return _mm256_round_pd(x, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double4	floor(double4 x)	{ return _mm256_round_pd(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double4	round(double4 x)	{ return _mm256_round_pd(x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }
#endif
#if defined __AVX512F__
	static SIMD_FUNC floatx16	trunc(floatx16 x)	{ return _mm512_roundscale_ps(x, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC floatx16	ceil(floatx16 x)	{ return _mm512_roundscale_ps(x, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC floatx16	floor(floatx16 x)	{ return _mm512_roundscale_ps(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC floatx16	round(floatx16 x)	{ return _mm512_roundscale_ps(x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }

	static SIMD_FUNC double8	trunc(double8 x)	{ return _mm512_roundscale_pd(x, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double8	ceil(double8 x)		{ return _mm512_roundscale_pd(x, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double8	floor(double8 x)	{ return _mm512_roundscale_pd(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC); }
	static SIMD_FUNC double8	round(double8 x)	{ return _mm512_roundscale_pd(x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }
#endif

#elif defined __ARM_NEON__

	static SIMD_FUNC float4		trunc(float4 x)		{ return vrndq_f32(x); }
	static SIMD_FUNC float4		ceil(float4 x)		{ return vrndpq_f32(x); }
	static SIMD_FUNC float4		floor(float4 x)		{ return vrndmq_f32(x); }
	static SIMD_FUNC float4		round(float4 x)		{ return vrndnq_f32(x); }

	static SIMD_FUNC float2		trunc(float2 x)		{ return vrnd_f32(x); }
	static SIMD_FUNC float2		ceil(float2 x)		{ return vrndp_f32(x); }
	static SIMD_FUNC float2		floor(float2 x)		{ return vrndm_f32(x); }
	static SIMD_FUNC float2		round(float2 x)		{ return vrndn_f32(x); }

	static SIMD_FUNC double2	trunc(double2 x)	{ return vrndq_f64(x); }
	static SIMD_FUNC double2	ceil(double2 x)		{ return vrndpq_f64(x); }
	static SIMD_FUNC double2	floor(double2 x)	{ return vrndmq_f64(x); }
	static SIMD_FUNC double2	round(double2 x)	{ return vrndnq_f64(x); }

#else
	static SIMD_FUNC float4 trunc(float4 x) {
		float4 binade = as<float>(as<int>(x) & 0x7f800000);
		return bitselect(x, bitselect((float4)0, x, as<int>(min(1 - 2 * binade, -0.f))), binade < (1 << 23));
	}
	static SIMD_FUNC float4 ceil(float4 x) {
		float4 truncated = trunc(x);
		return copysign(truncated + bitselect((float4)0, 1, truncated < x), x);
	}
	static SIMD_FUNC float4 floor(float4 x) {
		float4 truncated = trunc(x);
		return truncated - bitselect((float4)0, 1, truncated > x);
	}
	static SIMD_FUNC float4 round(float4 x) {
		return trunc(x + half);
	}

	static SIMD_FUNC double2 trunc(double2 x) {
		double2 binade = as<double>(as<int64>(x) & 0x7ff0000000000000);
		return bitselect(x, bitselect((double2)0, x, as<int64>(min(1 - 2 * binade, -0.))), binade < (1ull << 52));
	}
	static SIMD_FUNC double2 ceil(double2 x) {
		double2 truncated = trunc(x);
		return copysign(truncated + bitselect((double2)0, 1, truncated < x), x);
	}
	static SIMD_FUNC double2 floor(double2 x) {
		double2 truncated = trunc(x);
		return truncated - bitselect((double2)0, 1, truncated > x);
	}
	static SIMD_FUNC double2 round(double2 x) {
		return trunc(x + half);
	}

#endif

	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	trunc(X x);
	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	ceil (X x);
	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	floor(X x);
	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	round(X x);

	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<T,N>>	trunc(vec<T,N> x)	{ return simd_lo_hi(trunc(x.lo), trunc(x.hi)); }
	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<T,N>>	ceil (vec<T,N> x)	{ return simd_lo_hi(ceil (x.lo), ceil (x.hi)); }
	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<T,N>>	floor(vec<T,N> x)	{ return simd_lo_hi(floor(x.lo), floor(x.hi)); }
	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<T,N>>	round(vec<T,N> x)	{ return simd_lo_hi(round(x.lo), round(x.hi)); }

	template<typename T, int N>	SIMD_FUNC undersized_t<T, N, vec<T,N>>	trunc(vec<T,N> x)	{ return shrink<N>(trunc(grow<pow2_ceil<N + 1>>(x))); }
	template<typename T, int N>	SIMD_FUNC undersized_t<T, N, vec<T,N>>	ceil (vec<T,N> x)	{ return shrink<N>(ceil (grow<pow2_ceil<N + 1>>(x))); }
	template<typename T, int N>	SIMD_FUNC undersized_t<T, N, vec<T,N>>	floor(vec<T,N> x)	{ return shrink<N>(floor(grow<pow2_ceil<N + 1>>(x))); }
	template<typename T, int N>	SIMD_FUNC undersized_t<T, N, vec<T,N>>	round(vec<T,N> x)	{ return shrink<N>(round(grow<pow2_ceil<N + 1>>(x))); }

	template<typename T, int N>	SIMD_FUNC normalsized_t<T, N, vec<T,N>>	trunc(vec<T,N> x)	{ return trunc(x); }
	template<typename T, int N>	SIMD_FUNC normalsized_t<T, N, vec<T,N>>	ceil (vec<T,N> x)	{ return ceil (x); }
	template<typename T, int N>	SIMD_FUNC normalsized_t<T, N, vec<T,N>>	floor(vec<T,N> x)	{ return floor(x); }
	template<typename T, int N>	SIMD_FUNC normalsized_t<T, N, vec<T,N>>	round(vec<T,N> x)	{ return round(x); }

	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	trunc(X x)	{ return trunc<element_type<X>, num_elements_v<X>>(x); }
	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	ceil (X x)	{ return ceil <element_type<X>, num_elements_v<X>>(x); }
	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	floor(X x)	{ return floor<element_type<X>, num_elements_v<X>>(x); }
	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	round(X x)	{ return round<element_type<X>, num_elements_v<X>>(x); }

	template<typename X> static SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	frac(X x)	{ return x - floor(x); }
	template<typename X, typename Y> static SIMD_FUNC X	mod(const X &x, const Y &y)	{ return nmsub(trunc(div(x, y)), y, x); }

	//-----------------------------------------------------------------------------
	// sqrt
	//-----------------------------------------------------------------------------

#if defined __SSE2__
	static SIMD_FUNC float4		sqrt(float4 x)	{ return _mm_sqrt_ps(x); }
	static SIMD_FUNC double2	sqrt(double2 x) { return _mm_sqrt_pd(x); }

#if defined __AVX__
	static SIMD_FUNC float8		sqrt(float8 x)	{ return _mm256_sqrt_ps(x); }
	static SIMD_FUNC double4	sqrt(double4 x)	{ return _mm256_sqrt_pd(x); }
#endif
#if defined __AVX512F__
	static SIMD_FUNC floatx16	sqrt(floatx16 x){ return _mm512_sqrt_ps(x); }
	static SIMD_FUNC double8	sqrt(double8 x)	{ return _mm512_sqrt_pd(x); }
#endif

#elif defined __ARM_NEON__
	static SIMD_FUNC float4		sqrt(float4 x)	{ return vsqrtq_f32(x); }
	static SIMD_FUNC float2		sqrt(float2 x)	{ return vsqrt_f32(x); }
	static SIMD_FUNC double2	sqrt(double2 x)	{ return vsqrtq_f64(x); }
#else
	static SIMD_FUNC float4		sqrt(float2 x)	{ return make<float4>(sqrtf(x.x), sqrtf(x.y)); }
	static SIMD_FUNC float3		sqrt(float3 x)	{ return make<float3>(sqrtf(x.x), sqrtf(x.y), sqrtf(x.z)); }
	static SIMD_FUNC double2	sqrt(double2 x)	{ return make<double2>(::sqrt(x.x), ::sqrt(x.y)); }
	static SIMD_FUNC double3	sqrt(double3 x)	{ return make<double3>(::sqrt(x.x), ::sqrt(x.y), ::sqrt(x.z)); }
#endif

	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, vec_type<X>>	sqrt(X x);
	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<T,N>>			sqrt(vec<T,N> x)	{ return simd_lo_hi(sqrt(x.lo), sqrt(x.hi)); }
	template<typename T, int N>	SIMD_FUNC undersized_t<T, N, vec<T,N>>			sqrt(vec<T,N> x)	{ return shrink<N>(sqrt(grow<pow2_ceil<N + 1>>(x))); }
	template<typename T, int N>	SIMD_FUNC normalsized_t<T, N, vec<T,N>>			sqrt(vec<T,N> x)	{ return sqrt(x); }
	template<typename X>		SIMD_FUNC enable_if_t<is_vec<X>, vec<element_type<X>, num_elements_v<X>>>	sqrt(X x)	{ return sqrt<element_type<X>, num_elements_v<X>>(x); }

	//-----------------------------------------------------------------------------
	// reciprocal / rsqrt
	//-----------------------------------------------------------------------------

#if defined __ARM_NEON__
	inline float2 refine_recip(float2 x, float2 r) { return r * vrecps_f32(x, r); }
	inline float4 refine_recip(float4 x, float4 r) { return r * vrecpsq_f32(x, r); }
	inline float2 refine_rsqrt(float2 x, float2 r) { return r * vrsqrts_f32(x, r * r); }
	inline float4 refine_rsqrt(float4 x, float4 r) { return r * vrsqrtsq_f32(x, r * r); }
#else
	template<typename T> inline T refine_recip(T x, T r) { return r * (2 - x * r); }
	template<typename T> inline T refine_rsqrt(T x, T r) { return r * (3 - x * r * r) / 2; }
#endif

	namespace fast {
		template<int N> static SIMD_FUNC vec<float,N>	reciprocal(vec<float,N> x)	{ return 1 / x; }
		template<int N> static SIMD_FUNC vec<double,N>	reciprocal(vec<double,N> x)	{ return 1 / x; }
		template<int N> static SIMD_FUNC vec<float,N>	rsqrt(vec<float,N> x)		{ return reciprocal(sqrt(x)); }
		template<int N> static SIMD_FUNC vec<double,N>	rsqrt(vec<double,N> x)		{ return reciprocal(sqrt(x)); }
		//static SIMD_FUNC double	rsqrt(double x)		{ return 1 / ::sqrt(x); }

#if defined __SSE__
#if defined __AVX512VL__
		static SIMD_FUNC float4		reciprocal(float4 x)	{ return _mm_rcp14_ps(x); }
		static SIMD_FUNC float8		reciprocal(float8 x)	{ return _mm256_rcp14_ps(x); }
		static SIMD_FUNC float		reciprocal(float x)		{ float4 x4 = float4(x); return ((float4)_mm_rcp14_ss(x4, x4)).x; }
		static SIMD_FUNC float4		rsqrt(float4 x)			{ return _mm_rsqrt14_ps(x); }
		static SIMD_FUNC float8		rsqrt(float8 x)			{ return _mm256_rsqrt14_ps(x); }
		//static SIMD_FUNC float	rsqrt(float x)			{ float4 x4 = float4(x); return ((float4)_mm_rsqrt14_ss(x4, x4)).x; }
#else
		static SIMD_FUNC float4		reciprocal(float4 x)	{ return _mm_rcp_ps(x); }
		static SIMD_FUNC float		reciprocal(float x)		{ return ((float4)_mm_rcp_ss(float4(x))).x; }
		static SIMD_FUNC float4		rsqrt(float4 x)			{ return _mm_rsqrt_ps(x); }
		//static SIMD_FUNC float	rsqrt(float x)			{ return ((float4)_mm_rsqrt_ss(float4(x))).x; }
		static SIMD_FUNC float2		reciprocal(float2 x)	{ return reciprocal(grow<4>(x)).xy; }
		static SIMD_FUNC float2		rsqrt(float2 x)			{ return rsqrt(grow<4>(x)).xy; }
#ifdef __AVX__
		static SIMD_FUNC float8		reciprocal(float8 x)	{ return _mm256_rcp_ps(x); }
		static SIMD_FUNC float8		rsqrt(float8 x)			{ return _mm256_rsqrt_ps(x); }
#endif
#endif
#if defined __AVX512F__
		static SIMD_FUNC floatx16	reciprocal(floatx16 x)	{ return _mm512_rcp14_ps(x); }
		static SIMD_FUNC floatx16	rsqrt(floatx16 x)		{ return _mm512_rsqrt14_ps(x); }
#endif

#elif defined __ARM_NEON__
		static SIMD_FUNC float4		reciprocal(float4 x)	{ return refine_recip(x, vrecpeq_f32(x)); }
		static SIMD_FUNC float2		reciprocal(float2 x)	{ return refine_recip(x, vrecpe_f32(x)); }
		//static SIMD_FUNC float	reciprocal(float x)		{ return reciprocal(grow<2>(x)).x; }
		static SIMD_FUNC float4		rsqrt(float4 x)			{ return refine_rsqrt(x, vrsqrteq_f32(x)); }
		static SIMD_FUNC float2		rsqrt(float2 x)			{ return refine_rsqrt(x, vrsqrte_f32(x)); }
		//static SIMD_FUNC float	rsqrt(float x)			{ return rsqrt(grow<2>(x)).x; }
#endif

		static SIMD_FUNC float3		reciprocal(float3 x)	{ return reciprocal(grow<4>(x)).xyz; }
		static SIMD_FUNC float3		rsqrt(float3 x)			{ return rsqrt(grow<4>(x)).xyz; }
	}

	namespace precise {
		template<int N> static SIMD_FUNC vec<float,N>	reciprocal(vec<float,N> x)	{ return 1 / x; }
		template<int N> static SIMD_FUNC vec<double,N>	reciprocal(vec<double,N> x)	{ return 1 / x; }
		template<int N> static SIMD_FUNC vec<float,N>	rsqrt(vec<float,N> x)		{ return reciprocal(sqrt(x)); }
		template<int N> static SIMD_FUNC vec<double,N>	rsqrt(vec<double,N> x)		{ return reciprocal(sqrt(x)); }
		//static SIMD_FUNC double	rsqrt(double x)		{ return 1 / ::sqrt(x); }

#if defined __SSE__ || defined __ARM_NEON__
		static SIMD_FUNC float4		reciprocal(float4 x)	{ return refine_recip(x, fast::reciprocal(x)); }
		static SIMD_FUNC float2		reciprocal(float2 x)	{ return refine_recip(x, fast::reciprocal<2>(x)); }
		static SIMD_FUNC float4		rsqrt(float4 x)			{ return refine_rsqrt(x, fast::rsqrt(x)); }
		static SIMD_FUNC float2		rsqrt(float2 x)			{ return rsqrt(grow<4>(x)).xy; }
#endif
#if defined __AVX__
		static SIMD_FUNC float8		reciprocal(float8 x)	{ return refine_recip(x, fast::reciprocal(x)); }
		static SIMD_FUNC float8		rsqrt(float8 x)			{ return refine_rsqrt(x, fast::rsqrt(x)); }
#endif
#if defined __AVX512F__
		static SIMD_FUNC floatx16	reciprocal(floatx16 x)	{ return refine_recip(x, fast::reciprocal(x)); }
		static SIMD_FUNC floatx16	rsqrt(floatx16 x)		{ return refine_rsqrt(x, fast::rsqrt(x)); }
#endif

/*
#if defined __SSE__
		static SIMD_FUNC float		reciprocal(float x)		{ return refine_recip(x, fast::reciprocal(x)); }
		static SIMD_FUNC float		rsqrt(float x)			{ return refine_rsqrt(x, fast::rsqrt(x)); }
#elif defined __ARM_NEON__
		static SIMD_FUNC float		reciprocal(float x)		{ return reciprocal(grow<2>(x)).x; }
		static SIMD_FUNC float		rsqrt(float x)			{ return rsqrt(grow<2>(x)).x; }
#endif
*/
		static SIMD_FUNC float3		reciprocal(float3 x)	{ return reciprocal(grow<4>(x)).xyz; }
		static SIMD_FUNC float3		rsqrt(float3 x)			{ return rsqrt(grow<4>(x)).xyz; }
	}

	// default to using precise versions of reciprocal/rsqrt/normalize
	using namespace precise;

	//-----------------------------------------------------------------------------
	// reductions
	//-----------------------------------------------------------------------------

	// reduce_add
	template<typename X>				static SIMD_FUNC auto	reduce_add(X x);
#ifdef __clang__
	template<typename T>				static SIMD_FUNC T		reduce_add(vec<T, 1> x)	{ return x; }
#else
	template<typename T>				static SIMD_FUNC T		reduce_add(vec<T, 1> x)	{ return x.x; }
#endif
	template<typename T>				static SIMD_FUNC T		reduce_add(vec<T, 2> x)	{ return x.x + x.y; }
	template<typename T>				static SIMD_FUNC T		reduce_add(vec<T, 3> x)	{ return x.x + x.y + x.z; }
	template<typename T, int N> 		static SIMD_FUNC T		reduce_add(vec<T, N> x)	{ return reduce_add(x.lo + x.hi); }
	template<typename T, typename X> 	static SIMD_FUNC auto	reduce_add(X x)			{ return reduce_add<T, num_elements_v<X>>(x); }
	template<typename X>				static SIMD_FUNC auto	reduce_add(X x)			{ return reduce_add<element_type<X>>(x); }

	// reduce_mul
	template<typename X>				static SIMD_FUNC auto	reduce_mul(X x);
	template<typename T>				static SIMD_FUNC T		reduce_mul(vec<T, 2> x)	{ return x.x * x.y; }
	template<typename T>				static SIMD_FUNC T		reduce_mul(vec<T, 3> x)	{ return x.x * x.y * x.z; }
	template<typename T, int N> 		static SIMD_FUNC T		reduce_mul(vec<T, N> x)	{ return reduce_mul(x.lo * x.hi); }
	template<typename T, typename X> 	static SIMD_FUNC auto	reduce_mul(X x)			{ return reduce_mul<T, num_elements_v<X>>(x); }
	template<typename X>				static SIMD_FUNC auto	reduce_mul(X x)			{ return reduce_mul<element_type<X>>(x); }

	// reduce_min
	template<typename X>				static SIMD_FUNC auto	reduce_min(X x);
	template<typename T>				static SIMD_FUNC T		reduce_min(vec<T, 2> x)	{ return x.y < x.x ? x.y : x.x; }
	template<typename T>				static SIMD_FUNC T		reduce_min(vec<T, 3> x)	{ T t = x.z < x.x ? x.z : x.x; return x.y < t ? x.y : t; }
	template<typename T, int N> 		static SIMD_FUNC T		reduce_min(vec<T, N> x)	{ return reduce_min(min(x.lo, x.hi)); }
	template<typename T, typename X> 	static SIMD_FUNC auto	reduce_min(X x)			{ return reduce_min<T, num_elements_v<X>>(x); }
	template<typename X>				static SIMD_FUNC auto	reduce_min(X x)			{ return reduce_min<element_type<X>>(x); }

	// reduce_max
	template<typename X>				static SIMD_FUNC auto	reduce_max(X x);
	template<typename T>				static SIMD_FUNC T		_reduce_max(vec<T, 1> x)	{ return x; }
	template<typename T>				static SIMD_FUNC T		_reduce_max(vec<T, 2> x)	{ return x.y > x.x ? x.y : x.x; }
	template<typename T>				static SIMD_FUNC T		_reduce_max(vec<T, 3> x)	{ T t = x.z > x.x ? x.z : x.x; return x.y > t ? x.y : t; }
	template<typename T, int N> 		static SIMD_FUNC T		_reduce_max(vec<T, N> x)	{ return reduce_max(max(x.lo, x.hi)); }
	template<typename T, typename X> 	static SIMD_FUNC auto	_reduce_max(X x)			{ return _reduce_max<T, num_elements_v<X>>(x); }
	template<typename X>				static SIMD_FUNC auto	reduce_max(X x)			{ return _reduce_max<element_type<X>>(x); }

	// is_uniform
	template<typename X>				static SIMD_FUNC bool	is_uniform(X x);
	template<typename T>				static SIMD_FUNC T		is_uniform(vec<T, 2> x)	{ return x.x == x.y; }
	template<typename T>				static SIMD_FUNC bool	is_uniform(vec<T, 3> x)	{ return all(x.x == x.yz); }
	template<typename T, int N> 		static SIMD_FUNC bool	is_uniform(vec<T, N> x)	{ return is_uniform(x.lo) && all(x.lo == x.hi); }
	template<typename T, typename X> 	static SIMD_FUNC bool	is_uniform(X x)			{ return is_uniform<T, num_elements_v<X>>(x); }
	template<typename X>				static SIMD_FUNC bool	is_uniform(X x)			{ return is_uniform<element_type<X>>(x); }

	//-----------------------------------------------------------------------------
	// fullmul
	//-----------------------------------------------------------------------------

	template<typename T>	struct fullmul_type 			{ typedef T type; };
	template<>				struct fullmul_type<int8>		{ typedef int16 type; };
	template<>				struct fullmul_type<uint8>		{ typedef uint16 type; };
	template<>				struct fullmul_type<int16>		{ typedef int32 type; };
	template<>				struct fullmul_type<uint16>		{ typedef uint32 type; };
	template<>				struct fullmul_type<int32>		{ typedef int64 type; };
	template<>				struct fullmul_type<uint32>		{ typedef uint64 type; };
	template<typename T>	using fullmul_t = typename fullmul_type<T>::type;

	template<int N>	static SIMD_FUNC auto	fullmul(vec<float, N> x, vec<float, N> y)	{ return x * y; }
	template<int N>	static SIMD_FUNC auto	fullmul(vec<double, N> x, vec<double, N> y)	{ return x * y; }

	template<int N>	static SIMD_FUNC auto	fullmul(vec<int8, N> x, vec<int8, N> y)		{ return to<int16>(x)	* to<int16>(y); }
	template<int N>	static SIMD_FUNC auto	fullmul(vec<uint8, N> x, vec<uint8, N> y)	{ return to<uint16>(x)	* to<uint16>(y); }
	template<int N>	static SIMD_FUNC auto	fullmul(vec<int16, N> x, vec<int16, N> y)	{ return to<int32>(x)	* to<int32>(y); }
	template<int N>	static SIMD_FUNC auto	fullmul(vec<uint16, N> x, vec<uint16, N> y)	{ return to<uint32>(x)	* to<uint32>(y); }
	template<int N>	static SIMD_FUNC auto	fullmul(vec<int32, N> x, vec<int32, N> y)	{ return to<int64>(x)	* to<int64>(y); }
	template<int N>	static SIMD_FUNC auto	fullmul(vec<uint32, N> x, vec<uint32, N> y)	{ return to<uint64>(x)	* to<uint64>(y); }

#ifdef __SSE__
#ifdef __SSE4_1__
	static SIMD_FUNC vec<int64,2>	fullmul(int32x2 x, int32x2 y)		{ return _mm_mul_epi32((int32x4)swizzle<0,0,1,1>(x), (int32x4)swizzle<0,0,1,1>(y)); }
#endif
	static SIMD_FUNC vec<uint64,2>	fullmul(uint32x2 x, uint32x2 y)		{ return _mm_mul_epu32((uint32x4)swizzle<0,0,1,1>(x), (uint32x4)swizzle<0,0,1,1>(y)); }
#ifdef	__AVX__
	static SIMD_FUNC vec<int64,4>	fullmul(int32x4 x, int32x4 y)		{ return _mm256_mul_epi32(_mm256_cvtepu32_epi64(x), _mm256_cvtepu32_epi64(y)); }
	static SIMD_FUNC vec<uint64,4>	fullmul(uint32x4 x, uint32x4 y)		{ return _mm256_mul_epu32(_mm256_cvtepu32_epi64(x), _mm256_cvtepu32_epi64(y)); }
#endif
#if defined __AVX512F__
	static SIMD_FUNC vec<int64,8>	fullmul(int32x8 x, int32x8 y)		{ return _mm512_mul_epi32(_mm512_cvtepu32_epi64(x), _mm512_cvtepu32_epi64(y)); }
	static SIMD_FUNC vec<uint64,8>	fullmul(uint32x8 x, uint32x8 y)		{ return _mm512_mul_epu32(_mm512_cvtepu32_epi64(x), _mm512_cvtepu32_epi64(y)); }
#endif
#endif

	template<typename T, int N>	SIMD_FUNC oversized_t<T, N, vec<fullmul_t<T>, N>>		fullmul(vec<T,N> x, vec<T,N> y)	{
		return simd_lo_hi(fullmul(x.lo, y.lo), fullmul(x.hi, y.hi));
	}
	template<typename T, int N>	SIMD_FUNC not_oversized_t<T, N, vec<fullmul_t<T>, N>>	fullmul(vec<T,N> x, vec<T,N> y)	{
		static const int N2 = pow2_ceil<N + 1>;
		return shrink<N>(fullmul<N2>(grow<N2>(x), grow<N2>(y)));
	}
	template<typename X, typename Y>	SIMD_FUNC auto	fullmul(X x, Y y)->enable_if_t<is_vec<X>, decltype(fullmul<element_type<X>, num_elements_v<X>>(x, y))>	{
		return fullmul<element_type<X>, num_elements_v<X>>(x, y);
	}

	//-----------------------------------------------------------------------------
	// dynamic swizzle
	//-----------------------------------------------------------------------------
	
	template<typename I, typename V> inline vec<element_type<V>, num_elements_v<I>> dynamic_swizzle(I i, V v);

	template<int D> struct fix_indices;
	
	template<> struct fix_indices<1> {
		template<typename X, typename I> static inline auto f(I i) { return to<X>(i); }
	};
	template<> struct fix_indices<2> {
		template<typename X, typename I> static inline auto f(I i) { return as<X>(to<sint_bits_t<sizeof(X) * 16>>(i) * 0x0202 + 0x100); }
	};
	template<> struct fix_indices<4> {
		template<typename X, typename I> static inline auto f(I i) { return as<X>(to<sint_bits_t<sizeof(X) * 32>>(i) * 0x04040404 + 0x03020100); }
	};

//	dynamic_swizzle_size<E> 	number of indices for element type E
//	dynamic_swizzle_max<I> 		max number of indices of type I
//	dynamic_swizzle_index_t<E>	type of indices for element type E


#ifdef _SSE__
 #ifdef __SSSE3__
	template<typename E> 		constexpr int	dynamic_swizzle_size 	= sizeof(E) == 1 ? 16 : 8;
	template<typename I> 		constexpr int	dynamic_swizzle_max 	= num_elements_v<I>;
	template<typename E> 		using			dynamic_swizzle_index_t	= if_t<sizeof(E) < 4, int8, int32>;

	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x16 v) { return _mm_shuffle_epi8(v, i); }
	SIMD_FUNC int32x8 dynamic_swizzle(int32x8 i, int32x8 v) { return _mm256_permutevar8x32_epi32(v, i); }
 #else
	template<typename E> 		constexpr int	dynamic_swizzle_size 	= 16;
	template<typename I> 		constexpr int	dynamic_swizzle_max 	= 16;
	template<typename E> 		using			dynamic_swizzle_index_t	= int8;

	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x16 v) { return _mm_shuffle_epi8(v, i); }
 #endif

#elif defined __ARM_NEON__
	template<typename E> 		constexpr int	dynamic_swizzle_size	= 16;
	template<typename I> 		constexpr int	dynamic_swizzle_max 	= 64;
	template<typename E> 		using			dynamic_swizzle_index_t	= int8;
 #ifdef __clang__
	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x16 v) { return __builtin_neon_vqtbl1q_v(v, i, neon_type<int8>|32); }
	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x32 v) { return __builtin_neon_vqtbl2q_v(v.lo, v.hi, i, neon_type<int8>|32); }
	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x64 v) { return __builtin_neon_vqtbl4q_v(v.lo.lo, v.lo.hi, v.hi.lo, v.hi.hi, i, neon_type<int8>|32); }
 #else
	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x16 v) { return neon_tbl1_qq8(v, i); }
	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x32 v) { return neon_tbl2_qq8(reinterpret_cast<__n128x2&>(v), i); }
	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x64 v) { return neon_tbl4_qq8(reinterpret_cast<__n128x4&>(v), i); }
 #endif

#else //generic
	template<typename E> 		constexpr int	dynamic_swizzle_size 	= 16;
	template<typename I> 		constexpr int	dynamic_swizzle_max 	= 16;
	template<typename E> 		using			dynamic_swizzle_index_t	= int8;

	SIMD_FUNC int8x16 dynamic_swizzle(int8x16 i, int8x16 v) {
		return {
			v[i[ 0]], v[i[ 1]], v[i[ 2]], v[i[ 3]], 
			v[i[ 4]], v[i[ 5]], v[i[ 6]], v[i[ 7]], 
			v[i[ 8]], v[i[ 9]], v[i[10]], v[i[11]], 
			v[i[12]], v[i[13]], v[i[14]], v[i[15]]
		};
	}
#endif

	template<typename I, typename V> SIMD_FUNC enable_if_t<(num_elements_v<V> <= dynamic_swizzle_max<I>), I> dynamic_swizzle2(I i, V v) {
		return dynamic_swizzle(i, v);
	}
	template<typename I, typename V> SIMD_FUNC enable_if_t<(num_elements_v<V> > dynamic_swizzle_max<I>), I> dynamic_swizzle2(I i, V v) {
		static const int N = num_elements_v<I>;
		return select(i < N, dynamic_swizzle2(i, get(v.lo)), dynamic_swizzle2(i - N, get(v.hi)));
	}

	template<typename I, typename V> SIMD_FUNC enable_if_t<(num_elements_v<I> <= dynamic_swizzle_size<element_type<V>>), I> dynamic_swizzle1(I i, V v) {
		return shrink<num_elements_v<I>>(dynamic_swizzle2(grow<dynamic_swizzle_size<element_type<V>>>(i), v));
	}
	template<typename I, typename V> SIMD_FUNC enable_if_t<(num_elements_v<I> >  dynamic_swizzle_size<element_type<V>>), I> dynamic_swizzle1(I i, V v) {
		return concat(dynamic_swizzle1(get(i.lo), v), dynamic_swizzle1(get(i.hi), v));
	}

	template<typename I, typename V> SIMD_FUNC vec<element_type<V>, num_elements_v<I>> dynamic_swizzle(I i, V v) {
		typedef element_type<V>				E;
		typedef dynamic_swizzle_index_t<E>	EI;
		return as<E>(dynamic_swizzle1(fix_indices<sizeof(E) / sizeof(EI)>::template f<EI>(i), as<EI>(v)));
	}

	//-----------------------------------------------------------------------------
	// masked ops
	//-----------------------------------------------------------------------------

 #ifdef __AVX512F__
	SIMD_FUNC int8x16	masked_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_add_epi8(s, m, a, b); }
	SIMD_FUNC int8x16	masked_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_sub_epi8(s, m, a, b); }
	SIMD_FUNC int8x32	masked_add(int m, int8x32  s, int8x32  a, int8x32  b) 	{ return _mm256_mask_add_epi8(s, m, a, b); }
	SIMD_FUNC int8x32	masked_sub(int m, int8x32  s, int8x32  a, int8x32  b) 	{ return _mm256_mask_sub_epi8(s, m, a, b); }
	SIMD_FUNC int8x64	masked_add(int m, int8x64  s, int8x64  a, int8x64  b) 	{ return _mm512_mask_add_epi8(s, m, a, b); }
	SIMD_FUNC int8x64	masked_sub(int m, int8x64  s, int8x64  a, int8x64  b) 	{ return _mm512_mask_sub_epi8(s, m, a, b); }

	SIMD_FUNC int16x8  	masked_add(int m, int16x8  s, int16x8  a, int16x8  b) 	{ return _mm_mask_add_epi16(s, m, a, b); }
	SIMD_FUNC int16x8  	masked_sub(int m, int16x8  s, int16x8  a, int16x8  b) 	{ return _mm_mask_sub_epi16(s, m, a, b); }
	SIMD_FUNC int16x16 	masked_add(int m, int16x16 s, int16x16 a, int16x16 b) 	{ return _mm256_mask_add_epi16(s, m, a, b); }
	SIMD_FUNC int16x16 	masked_sub(int m, int16x16 s, int16x16 a, int16x16 b) 	{ return _mm256_mask_sub_epi16(s, m, a, b); }
	SIMD_FUNC int16x32 	masked_add(int m, int16x32 s, int16x32 a, int16x32 b) 	{ return _mm512_mask_add_epi16(s, m, a, b); }
	SIMD_FUNC int16x32 	masked_sub(int m, int16x32 s, int16x32 a, int16x32 b) 	{ return _mm512_mask_sub_epi16(s, m, a, b); }

	SIMD_FUNC int32x4  	masked_add(int m, int32x4  s, int32x4  a, int32x4  b) 	{ return _mm_mask_add_epi32(s, m, a, b); }
	SIMD_FUNC int32x4  	masked_sub(int m, int32x4  s, int32x4  a, int32x4  b) 	{ return _mm_mask_sub_epi32(s, m, a, b); }
	SIMD_FUNC int32x4  	masked_mul(int m, int32x4  s, int32x4  a, int32x4  b) 	{ return _mm_mask_mul_epi32(s, m, a, b); }
	SIMD_FUNC int32x8  	masked_add(int m, int32x8  s, int32x8  a, int32x8  b) 	{ return _mm256_mask_add_epi32(s, m, a, b); }
	SIMD_FUNC int32x8  	masked_sub(int m, int32x8  s, int32x8  a, int32x8  b) 	{ return _mm256_mask_sub_epi32(s, m, a, b); }
	SIMD_FUNC int32x8  	masked_mul(int m, int32x8  s, int32x8  a, int32x8  b) 	{ return _mm256_mask_mul_epi32(s, m, a, b); }
	SIMD_FUNC int32x16 	masked_add(int m, int32x16 s, int32x16 a, int32x16 b) 	{ return _mm512_mask_add_epi32(s, m, a, b); }
	SIMD_FUNC int32x16 	masked_sub(int m, int32x16 s, int32x16 a, int32x16 b) 	{ return _mm512_mask_sub_epi32(s, m, a, b); }
	SIMD_FUNC int32x16 	masked_mul(int m, int32x16 s, int32x16 a, int32x16 b) 	{ return _mm512_mask_mul_epi32(s, m, a, b); }

	SIMD_FUNC int64x2  	masked_add(int m, int64x2  s, int64x2  a, int64x2  b) 	{ return _mm_mask_add_epi64(s, m, a, b); }
	SIMD_FUNC int64x2  	masked_sub(int m, int64x2  s, int64x2  a, int64x2  b) 	{ return _mm_mask_sub_epi64(s, m, a, b); }
	SIMD_FUNC int64x4  	masked_add(int m, int64x4  s, int64x4  a, int64x4  b) 	{ return _mm256_mask_add_epi64(s, m, a, b); }
	SIMD_FUNC int64x4  	masked_sub(int m, int64x4  s, int64x4  a, int64x4  b) 	{ return _mm256_mask_sub_epi64(s, m, a, b); }
	SIMD_FUNC int64x8  	masked_add(int m, int64x8  s, int64x8  a, int64x8  b) 	{ return _mm512_mask_add_epi64(s, m, a, b); }
	SIMD_FUNC int64x8  	masked_sub(int m, int64x8  s, int64x8  a, int64x8  b) 	{ return _mm512_mask_sub_epi64(s, m, a, b); }

	SIMD_FUNC floatx4  	masked_add(int m, float4   s, float4   a, float4   b) 	{ return _mm_mask_add_ps(s, m, a, b); }
	SIMD_FUNC floatx4  	masked_sub(int m, float4   s, float4   a, float4   b) 	{ return _mm_mask_sub_ps(s, m, a, b); }
	SIMD_FUNC floatx4  	masked_mul(int m, float4   s, float4   a, float4   b) 	{ return _mm_mask_mul_ps(s, m, a, b); }
	SIMD_FUNC floatx4  	masked_div(int m, float4   s, float4   a, float4   b) 	{ return _mm_mask_div_ps(s, m, a, b); }
	SIMD_FUNC floatx8  	masked_add(int m, float8   s, float8   a, float8   b) 	{ return _mm256_mask_add_ps(s, m, a, b); }
	SIMD_FUNC floatx8  	masked_sub(int m, float8   s, float8   a, float8   b) 	{ return _mm256_mask_sub_ps(s, m, a, b); }
	SIMD_FUNC floatx8  	masked_mul(int m, float8   s, float8   a, float8   b) 	{ return _mm256_mask_mul_ps(s, m, a, b); }
	SIMD_FUNC floatx8  	masked_div(int m, float8   s, float8   a, float8   b) 	{ return _mm256_mask_div_ps(s, m, a, b); }
	SIMD_FUNC floatx16 	masked_add(int m, floatx16 s, floatx16 a, floatx16 b) 	{ return _mm512_mask_add_ps(s, m, a, b); }
	SIMD_FUNC floatx16 	masked_sub(int m, floatx16 s, floatx16 a, floatx16 b) 	{ return _mm512_mask_sub_ps(s, m, a, b); }
	SIMD_FUNC floatx16 	masked_mul(int m, floatx16 s, floatx16 a, floatx16 b) 	{ return _mm512_mask_mul_ps(s, m, a, b); }
	SIMD_FUNC floatx16 	masked_div(int m, floatx16 s, floatx16 a, floatx16 b) 	{ return _mm512_mask_div_ps(s, m, a, b); }

	SIMD_FUNC double2  	masked_add(int m, double2  s, double2  a, double2  b) 	{ return _mm_mask_add_pd(s, m, a, b); }
	SIMD_FUNC double2  	masked_sub(int m, double2  s, double2  a, double2  b) 	{ return _mm_mask_sub_pd(s, m, a, b); }
	SIMD_FUNC double2  	masked_mul(int m, double2  s, double2  a, double2  b) 	{ return _mm_mask_mul_pd(s, m, a, b); }
	SIMD_FUNC double2  	masked_div(int m, double2  s, double2  a, double2  b) 	{ return _mm_mask_div_pd(s, m, a, b); }
	SIMD_FUNC double4  	masked_add(int m, double4  s, double4  a, double4  b) 	{ return _mm256_mask_add_pd(s, m, a, b); }
	SIMD_FUNC double4  	masked_sub(int m, double4  s, double4  a, double4  b) 	{ return _mm256_mask_sub_pd(s, m, a, b); }
	SIMD_FUNC doublex4 	masked_mul(int m, double4  s, double4  a, double4  b) 	{ return _mm256_mask_mul_pd(s, m, a, b); }
	SIMD_FUNC doublex4 	masked_div(int m, double4  s, double4  a, double4  b) 	{ return _mm256_mask_div_pd(s, m, a, b); }
	SIMD_FUNC double8  	masked_add(int m, double8  s, double8  a, double8  b) 	{ return _mm512_mask_add_pd(s, m, a, b); }
	SIMD_FUNC double8  	masked_sub(int m, double8  s, double8  a, double8  b) 	{ return _mm512_mask_sub_pd(s, m, a, b); }
	SIMD_FUNC doublex8 	masked_mul(int m, double8  s, double8  a, double8  b) 	{ return _mm512_mask_mul_pd(s, m, a, b); }
	SIMD_FUNC doublex8 	masked_div(int m, double8  s, double8  a, double8  b) 	{ return _mm512_mask_div_pd(s, m, a, b); }
	
	SIMD_FUNC int8x16	masked_pck(int m, int8x16  s, int8x16  a)	{ return _mm_mask_compress_epi8(s, m, a); }
	SIMD_FUNC int8x32	masked_pck(int m, int8x32  s, int8x32  a)	{ return _mm256_mask_compress_epi8(s, m, a); }
	SIMD_FUNC int8x64	masked_pck(int m, int8x64  s, int8x64  a)	{ return _mm512_mask_compress_epi8(s, m, a); }

	SIMD_FUNC int16x8  	masked_pck(int m, int16x8  s, int16x8  a)	{ return _mm_mask_compress_epi16(s, m, a); }
	SIMD_FUNC int16x16 	masked_pck(int m, int16x16 s, int16x16 a)	{ return _mm256_mask_compress_epi16(s, m, a); }
	SIMD_FUNC int16x32 	masked_pck(int m, int16x32 s, int16x32 a)	{ return _mm512_mask_compress_epi16(s, m, a); }

	SIMD_FUNC int32x4  	masked_pck(int m, int32x4  s, int32x4  a)	{ return _mm_mask_compress_epi32(s, m, a); }
	SIMD_FUNC int32x8  	masked_pck(int m, int32x8  s, int32x8  a)	{ return _mm256_mask_compress_epi32(s, m, a); }
	SIMD_FUNC int32x16 	masked_pck(int m, int32x16 s, int32x16 a)	{ return _mm512_mask_compress_epi32(s, m, a); }

	SIMD_FUNC int64x2  	masked_pck(int m, int64x2  s, int64x2  a)	{ return _mm_mask_compress_epi64(s, m, a); }
	SIMD_FUNC int64x4  	masked_pck(int m, int64x4  s, int64x4  a)	{ return _mm256_mask_compress_epi64(s, m, a); }
	SIMD_FUNC int64x8  	masked_pck(int m, int64x8  s, int64x8  a)	{ return _mm512_mask_compress_epi64(s, m, a); }

	SIMD_FUNC floatx4  	masked_pck(int m, float4   s, float4   a)	{ return _mm_mask_compress_ps(s, m, a); }
	SIMD_FUNC floatx8  	masked_pck(int m, float8   s, float8   a)	{ return _mm256_mask_compress_ps(s, m, a); }
	SIMD_FUNC floatx16 	masked_pck(int m, floatx16 s, floatx16 a)	{ return _mm512_mask_compress_ps(s, m, a); }

	SIMD_FUNC double2  	masked_pck(int m, double2  s, double2  a)	{ return _mm_mask_compress_pd(s, m, a); }
	SIMD_FUNC doublex4 	masked_pck(int m, double4  s, double4  a)	{ return _mm256_mask_compress_pd(s, m, a); }
	SIMD_FUNC doublex8 	masked_pck(int m, double8  s, double8  a)	{ return _mm512_mask_compress_pd(s, m, a); }
	
#elif defined __AVX2__

	template<typename M> int8x4 masked_pck(M m, int8x4 a) {
		return as<int8>(_pext_u32(as<uint32>(a), as<uint32>(make_mask<int8, 4>(m))));
	}
	template<typename M> int16x2 masked_pck(M m, int16x2 a) {
		return as<int16>(_pext_u32(as<uint32>(a), as<uint32>(make_mask<int16, 2>(m))));
	}
	template<typename M> int8x8 masked_pck(M m, int8x8 a) {
		return as<int8>(_pext_u64(as<uint64>(a), as<uint64>(make_mask<int8, 8>(m))));
	}
	template<typename M> int16x4 masked_pck(M m, int16x4 a) {
		return as<int16>(_pext_u64(as<uint64>(a), as<uint64>(make_mask<int16, 4>(m))));
	}

#endif

	template<typename E, int N, typename M> SIMD_FUNC vec<E, N> masked_add(M m, vec<E,N> s, vec<E,N> a, vec<E,N> b)	{ return select(m, a + b, s); }
	template<typename E, int N, typename M> SIMD_FUNC vec<E, N> masked_sub(M m, vec<E,N> s, vec<E,N> a, vec<E,N> b)	{ return select(m, a - b, s); }
	template<typename E, int N, typename M> SIMD_FUNC vec<E, N> masked_mul(M m, vec<E,N> s, vec<E,N> a, vec<E,N> b)	{ return select(m, a * b, s); }
	template<typename E, int N, typename M> SIMD_FUNC vec<E, N> masked_div(M m, vec<E,N> s, vec<E,N> a, vec<E,N> b)	{ return select(m, a / b, s); }

	template<typename M, typename S, typename A, typename B> 	SIMD_FUNC auto masked_add(M m, S s, A a, B b)	{ return masked_add<element_type<S>, num_elements_v<S>>(m, s, a, b); }
	template<typename M, typename S, typename A, typename B> 	SIMD_FUNC auto masked_sub(M m, S s, A a, B b)	{ return masked_sub<element_type<S>, num_elements_v<S>>(m, s, a, b); }
	template<typename M, typename S, typename A, typename B> 	SIMD_FUNC auto masked_mul(M m, S s, A a, B b)	{ return masked_mul<element_type<S>, num_elements_v<S>>(m, s, a, b); }
	template<typename M, typename S, typename A, typename B> 	SIMD_FUNC auto masked_div(M m, S s, A a, B b)	{ return masked_div<element_type<S>, num_elements_v<S>>(m, s, a, b); }

	template<typename V, typename M> SIMD_FUNC auto masked_pck(M m, V a);

	template<typename A, typename B> SIMD_FUNC auto concat_at(A a, B b, int i)	{
		static const int N = num_elements_v<A> + num_elements_v<B>;
		return select(bits(i), grow<N>(a), rotate(grow<N>(b), -i));
	}

#ifdef __AVX2__

	template<int N> inline vec<int8, N> make_indices(uint64 m) {
		uint64	p0 = _pext_u64(~chunkmask<uint64, 1>, m);
		uint64	p1 = _pext_u64(~chunkmask<uint64, 2>, m);
		uint64	p2 = _pext_u64(~chunkmask<uint64, 4>, m);
		uint64	p3 = _pext_u64(~chunkmask<uint64, 8>, m);
		uint64	p4 = _pext_u64(~chunkmask<uint64,16>, m);
		uint64	p5 = _pext_u64(~chunkmask<uint64,32>, m);
		int8x32	i = zero;
		i = masked_add(p0, i, i, 1);
		i = masked_add(p1, i, i, 2);
		i = masked_add(p2, i, i, 4);
		i = masked_add(p3, i, i, 8);
		i = masked_add(p4, i, i, 16);
		i = masked_add(p5, i, i, 32);
		return shrink<N>(i);
	}

  #ifdef __AVX512F__
	template<typename E, int N> using masked_pck_oversize_t 	= oversized_t<E, N, vec<E, N>>;
	template<typename E, int N> using masked_pck_not_oversize_t	= not_oversized_t<E, N, vec<E, N>>;
  #else
	template<typename V, typename M> enable_if_t<(sizeof(element_type<V>) > 2), V> masked_pck1(M m, V a) {
		return dynamic_swizzle(make_indices<num_elements_v<V>>(bit_mask(m)), a);
	}

	template<typename E, int N> using masked_pck_oversize_t 	= enable_if_t<(sizeof(E) <= 2 && N >  (16 >> sizeof(E))), vec<E, N>>;
	template<typename E, int N> using masked_pck_not_oversize_t	= enable_if_t<(sizeof(E) <= 2 && N <= (16 >> sizeof(E))), vec<E, N>>;
  #endif

	template<typename V, typename M> masked_pck_oversize_t<element_type<V>, num_elements_v<V>> masked_pck1(M m, V a) {
		static const int N = num_elements_v<V>;
		return concat_at(masked_pck(m, a.lo), masked_pck(m >> (N / 2), a.hi), count_bits(m & ((1 << (N / 2)) - 1)));
	}
	template<typename V, typename M> masked_pck_not_oversize_t<element_type<V>, num_elements_v<V>> masked_pck1(M m, V a) {
		return masked_pck(m, a);
	}

	template<typename V, typename M> SIMD_FUNC auto masked_pck(M m, V a) {
		typedef element_type<V>		E;
		typedef sint_for_t<E>		I;
		static const int N = num_elements_v<V>;
		return as<E>(shrink<N>(masked_pck1(m, grow<pow2_ceil<N>>(as<I>(a)))));
	}

#else

	inline vec<int8, 8> _make_indices(uint8 i) {
		static const uint32 table[256] = {
			0x00000000,0x00000000,0x00000001,0x00000010,0x00000002,0x00000020,0x00000021,0x00000210,	0x00000003,0x00000030,0x00000031,0x00000310,0x00000032,0x00000320,0x00000321,0x00003210,
			0x00000004,0x00000040,0x00000041,0x00000410,0x00000042,0x00000420,0x00000421,0x00004210,	0x00000043,0x00000430,0x00000431,0x00004310,0x00000432,0x00004320,0x00004321,0x00043210,
			0x00000005,0x00000050,0x00000051,0x00000510,0x00000052,0x00000520,0x00000521,0x00005210,	0x00000053,0x00000530,0x00000531,0x00005310,0x00000532,0x00005320,0x00005321,0x00053210,
			0x00000054,0x00000540,0x00000541,0x00005410,0x00000542,0x00005420,0x00005421,0x00054210,	0x00000543,0x00005430,0x00005431,0x00054310,0x00005432,0x00054320,0x00054321,0x00543210,
			0x00000006,0x00000060,0x00000061,0x00000610,0x00000062,0x00000620,0x00000621,0x00006210,	0x00000063,0x00000630,0x00000631,0x00006310,0x00000632,0x00006320,0x00006321,0x00063210,
			0x00000064,0x00000640,0x00000641,0x00006410,0x00000642,0x00006420,0x00006421,0x00064210,	0x00000643,0x00006430,0x00006431,0x00064310,0x00006432,0x00064320,0x00064321,0x00643210,
			0x00000065,0x00000650,0x00000651,0x00006510,0x00000652,0x00006520,0x00006521,0x00065210,	0x00000653,0x00006530,0x00006531,0x00065310,0x00006532,0x00065320,0x00065321,0x00653210,
			0x00000654,0x00006540,0x00006541,0x00065410,0x00006542,0x00065420,0x00065421,0x00654210,	0x00006543,0x00065430,0x00065431,0x00654310,0x00065432,0x00654320,0x00654321,0x06543210,
			0x00000007,0x00000070,0x00000071,0x00000710,0x00000072,0x00000720,0x00000721,0x00007210,	0x00000073,0x00000730,0x00000731,0x00007310,0x00000732,0x00007320,0x00007321,0x00073210,
			0x00000074,0x00000740,0x00000741,0x00007410,0x00000742,0x00007420,0x00007421,0x00074210,	0x00000743,0x00007430,0x00007431,0x00074310,0x00007432,0x00074320,0x00074321,0x00743210,
			0x00000075,0x00000750,0x00000751,0x00007510,0x00000752,0x00007520,0x00007521,0x00075210,	0x00000753,0x00007530,0x00007531,0x00075310,0x00007532,0x00075320,0x00075321,0x00753210,
			0x00000754,0x00007540,0x00007541,0x00075410,0x00007542,0x00075420,0x00075421,0x00754210,	0x00007543,0x00075430,0x00075431,0x00754310,0x00075432,0x00754320,0x00754321,0x07543210,
			0x00000076,0x00000760,0x00000761,0x00007610,0x00000762,0x00007620,0x00007621,0x00076210,	0x00000763,0x00007630,0x00007631,0x00076310,0x00007632,0x00076320,0x00076321,0x00763210,
			0x00000764,0x00007640,0x00007641,0x00076410,0x00007642,0x00076420,0x00076421,0x00764210,	0x00007643,0x00076430,0x00076431,0x00764310,0x00076432,0x00764320,0x00764321,0x07643210,
			0x00000765,0x00007650,0x00007651,0x00076510,0x00007652,0x00076520,0x00076521,0x00765210,	0x00007653,0x00076530,0x00076531,0x00765310,0x00076532,0x00765320,0x00765321,0x07653210,
			0x00007654,0x00076540,0x00076541,0x00765410,0x00076542,0x00765420,0x00765421,0x07654210,	0x00076543,0x00765430,0x00765431,0x07654310,0x00765432,0x07654320,0x07654321,0x76543210,
		};
		return as<int8>(part_bits<4,4>(table[i]));
	}
	template<int N> inline enable_if_t<(N <= 8), vec<int8, N>> make_indices(uint64 i, int offset = 0) {
		return shrink<N>(_make_indices((uint8)i) + offset);
	}
	template<int N> inline enable_if_t<(N >  8), vec<int8, N>> make_indices(uint64 i, int offset = 0) {
		int	k = count_bits((uint8)i);
		return concat_at(_make_indices((uint8)i) + offset, make_indices<N - 8>(i >> 8, offset + 8), k);
	}

	template<typename V, typename M> SIMD_FUNC V masked_pck(M m, V a) {
		return dynamic_swizzle(make_indices<num_elements_v<V>>(bit_mask(m)), a);
	}
#endif

	template<typename M, typename S, typename A>	SIMD_FUNC auto masked_pck(M m, S s, A a) {
		return select(bits(count_bits(bit_mask(m))), masked_pck(m, a), s);
	}

	template<typename E, int N, bool FLT = is_float<E>> struct vmodifier_helper {
		typedef sint_for_t<E>		I;
		auto static neg(vec<E, N> t, vec<I,N> n)					{ return as<E>(as<I>(t) ^ n); }
		auto static abs(vec<E, N> t, vec<I,N> a)					{ return as<E>(as<I>(t) & ~a); }
		auto static neg_abs(vec<E, N> t, vec<I,N> n, vec<I,N> a)	{ return as<E>((as<I>(t) & ~a) ^ n); }
		auto static force_neg(vec<E, N> t, vec<I,N> n)				{ return as<E>(as<I>(t) | n); }
	};

	template<typename E, int N> struct vmodifier_helper<E, N, false> {
		typedef sint_for_t<E>		I;
		static vec<E,N> common(vec<E, N> t, vec<I,N> s) {
			auto m  = s < zero;//> (sizeof(I) * 8 - 1);
			return as<E>((as<I>(t) ^ m) - m);
		}
		auto static neg(vec<E, N> t, vec<I,N> n)					{ return common(t, n); }
		auto static abs(vec<E, N> t, vec<I,N> a)					{ return common(t, as<I>(t) & a); }
		auto static neg_abs(vec<E, N> t, vec<I,N> n, vec<I,N> a)	{ return common(t, (as<I>(t) & a) ^ n); }
		auto static force_neg(vec<E, N> t, vec<I,N> n)				{ return common(t, as<I>(t) & n); }
	};

	// dispatch on combination of ABS & NEG:
	// signs = (signs & ~ABS) ^ NEG
	// ABS = 0:		signs = signs ^ NEG
	// NEG = 0:		signs = signs & ~ABS
	// NEG = ABS:	signs = signs | NEG

	template<int ABS, int NEG> struct modifier_helper {
		template<typename E, int N> auto static f(vec<E, N> t) {
			return vmodifier_helper<E, N>::neg_abs(t,
				make_mask<sint_for_t<E>, N, NEG>(),
				make_mask<sint_for_t<E>, N, ABS>()
			);
		}
	};
	// |x|	abs
	template<int ABS> struct modifier_helper<ABS, 0> {
		template<typename E, int N> auto static f(vec<E, N> t) {
			return vmodifier_helper<E, N>::abs(t, make_sign_mask<sint_for_t<E>, N, ABS>());
		}
	};
	// -x	negate
	template<int NEG> struct modifier_helper<0, NEG> {
		template<typename E, int N> auto static f(vec<E, N> t) {
			return vmodifier_helper<E, N>::neg(t, make_sign_mask<sint_for_t<E>, N, NEG>());
		}
	};
	// -|x|	make negative
	template<int NEG> struct modifier_helper<NEG, NEG> {
		template<typename E, int N> auto static f(vec<E, N> t) {
			return vmodifier_helper<E, N>::force_neg(t, make_sign_mask<sint_for_t<E>, N, NEG>());
		}
	};
	// nothing
	template<> struct modifier_helper<0, 0> {
		template<typename E, int N> constexpr auto static f(vec<E, N> t) { return t; }
	};

	static struct masked_s {
		template<typename M, typename T> struct vholder {
			typedef noref_cv_t<T>		T0;
			typedef element_type<T0>	E;
			typedef sint_for_t<E>		I;
			static const int N =  num_elements_v<T0>;
			M	m;
			T	t;
			constexpr vholder(M &&m, T &&t) : m(forward<M>(m)), t(forward<T>(t)) {}
			auto		operator-() const 		{ return vmodifier_helper<E, N>::neg(t, make_sign_mask<I, N>(m)); }
			friend auto abs(const vholder &h) 	{ return vmodifier_helper<E, N>::abs(h.t, make_sign_mask<I, N>(h.m)); }
			friend auto pack(const vholder &h, const T0 &s = zero) 	{ return masked_pck(h.m, s, h.t); }

			T		operator--() 	const		{ return t = *this - one; }
			T		operator++() 	const		{ return t = *this + one; }
			T0		operator--(int) const		{ T0 t0 = t; operator--(); return t0; }
			T0		operator++(int) const		{ T0 t0 = t; operator++(); return t0; }

			template<typename B> auto	operator+(B b) const { return masked_add(m, t, t, b); }
			template<typename B> auto	operator-(B b) const { return masked_sub(m, t, t, b); }
			template<typename B> auto	operator*(B b) const { return masked_mul(m, t, t, b); }
			template<typename B> auto	operator/(B b) const { return masked_div(m, t, t, b); }

			template<typename B> auto	operator+=(B b) const { return t = *this + b; }
			template<typename B> auto	operator-=(B b) const { return t = *this - b; }
			template<typename B> auto	operator*=(B b) const { return t = *this * b; }
			template<typename B> auto	operator/=(B b) const { return t = *this / b; }
		};

		template<int ABS, int NEG, typename T> struct modified {
			typedef noref_cv_t<T>		T0;
			typedef element_type<T0>	E;
			typedef sint_for_t<E>		I;
			static const int N =  num_elements_v<T0>;
			T	t;
			constexpr modified(T t) : t(t) {}
			constexpr operator vec<E,N>() const { return modifier_helper<ABS, NEG>::template f<E, N>(t); }
			constexpr modified<ABS, (NEG ^ ((1<<N) - 1)) | ABS, T>	operator-() const 		{ return t; }
			friend constexpr modified<(1 << N)-1, NEG & ~ABS, T> 	abs(const modified &h) 	{ return h.t; }
		};

		template<int M, typename T> struct holder {
			typedef noref_cv_t<T>		T0;
			typedef element_type<T0>	E;
			typedef sint_for_t<E>		I;
			static const int N =  num_elements_v<T0>;
			static constexpr int pack_f(int i) { return nth_set_index(M|(~0u << N), i); }
			T	t;
			constexpr holder(T &&t) : t(forward<T>(t)) {}

			constexpr modified<0, M, T>			operator-() const 			{ return t; }
			friend constexpr modified<M, 0, T> 	abs(const holder &h) 		{ return h.t; }
//			friend auto pack(const holder &h, vec<E,N> s = zero)	{ return swizzle(meta::apply<meta::make_value_sequence<N>, pack_f>(), h.t, s); }

			T		operator--() 	const	{ return t = *this - one; }
			T		operator++() 	const	{ return t = *this + one; }
			T0		operator--(int) const	{ T0 t0 = t; operator--(); return t0; }
			T0		operator++(int) const	{ T0 t0 = t; operator++(); return t0; }


			template<typename B> enable_if_t<!is_vec<B>, T0>	operator+(B b) const { return masked_add(M, t, t, b); }
			template<typename B> enable_if_t<!is_vec<B>, T0>	operator-(B b) const { return masked_sub(M, t, t, b); }
			template<typename B> enable_if_t<!is_vec<B>, T0>	operator*(B b) const { return masked_mul(M, t, t, b); }
			template<typename B> enable_if_t<!is_vec<B>, T0>	operator/(B b) const { return masked_div(M, t, t, b); }

			template<typename B> enable_if_t<is_vec<B> && (bits(num_elements_v<B>) & M), T0>	operator+(B b) const { return masked_add(M, t, t, grow<N>(b)); }
			template<typename B> enable_if_t<is_vec<B> && (bits(num_elements_v<B>) & M), T0>	operator-(B b) const { return masked_sub(M, t, t, grow<N>(b)); }
			template<typename B> enable_if_t<is_vec<B> && (bits(num_elements_v<B>) & M), T0>	operator*(B b) const { return masked_mul(M, t, t, grow<N>(b)); }
			template<typename B> enable_if_t<is_vec<B> && (bits(num_elements_v<B>) & M), T0>	operator/(B b) const { return masked_div(M, t, t, grow<N>(b)); }

			template<typename B> auto	operator+=(B b) const { return t = *this + b; }
			template<typename B> auto	operator-=(B b) const { return t = *this - b; }
			template<typename B> auto	operator*=(B b) const { return t = *this * b; }
			template<typename B> auto	operator/=(B b) const { return t = *this / b; }
		};

		template<int M, int ABS, int NEG, typename T> struct holder<M, modified<ABS, NEG, T>> {
			typedef noref_cv_t<T>		T0;
			typedef element_type<T0>	E;
			typedef sint_for_t<E>		I;
			static const int N =  num_elements_v<T0>;
			T	t;
			constexpr holder(const modified<ABS, NEG, T> &t) : t(t.t) {}
			auto	get() const { return modifier_helper<ABS, NEG>::template f<E, N>(t); }

			modified<ABS, (NEG & ~ABS) ^ M, T>		operator-() const 		{ return t; }
			friend modified<ABS | M, NEG & ~ABS, T> abs(const holder &h) 	{ return h.t; }

			template<typename B> auto	operator+(B b) const { auto t0 = get(); return select(make_mask<I, N, M>(), t0 + b, t0); }
			template<typename B> auto	operator-(B b) const { auto t0 = get(); return select(make_mask<I, N, M>(), t0 - b, t0); }
			template<typename B> auto	operator*(B b) const { auto t0 = get(); return select(make_mask<I, N, M>(), t0 * b, t0); }
			template<typename B> auto	operator/(B b) const { auto t0 = get(); return select(make_mask<I, N, M>(), t0 / b, t0); }
		};

		template<int M> struct modifier {
			template<typename T> constexpr holder<M, T>	operator()(T&& t) const { return forward<T>(t); }
		};
		modifier< 1>	x;
		modifier< 2>	y;
		modifier< 3>	xy;
		modifier< 4>	z;
		modifier< 5>	xz;
		modifier< 6>	yz;
		modifier< 7>	xyz;
		modifier< 8>	w;
		modifier< 9>	xw;
		modifier<10>	yw;
		modifier<11>	xyw;
		modifier<12>	zw;
		modifier<13>	xzw;
		modifier<14>	yzw;
		modifier<15>	xyzw;

		template<typename M, typename T>	constexpr vholder<M, T>	operator()(M&& m, T&& t)			const { return {forward<M>(m), forward<T>(t)}; }
		template<int M, typename T>			constexpr holder<M, T>	operator()(constant_int<M>, T&& t)	const { return forward<T>(t); }
	} masked;

	template<int M, typename T>			constexpr auto	maskedi(T&& t)			{ return masked_s::holder<M, T>(forward<T>(t)); }
	template<typename M, typename T>	constexpr auto	set_sign(T&& t, M&& m)	{ return vmodifier_helper<element_type<T>, num_elements_v<T>>::force_neg(t, m); }

	//-----------------------------------------------------------------------------
	// saturated ops
	//-----------------------------------------------------------------------------

#ifdef __SSE2__
#ifdef _M_IX86
	SIMD_FUNC int16x4	saturated_add(int16x4	a, int16x4		b)	{ return _mm_adds_pi16(a, b); }
	SIMD_FUNC int8x8	saturated_add(int8x8	a, int8x8		b)	{ return _mm_adds_pi8(a, b); }
	SIMD_FUNC uint16x4	saturated_add(uint16x4	a, uint16x4		b)	{ return _mm_adds_pu16(a, b); }
	SIMD_FUNC uint8x8	saturated_add(uint8x8	a, uint8x8		b)	{ return _mm_adds_pu8(a, b); }
	SIMD_FUNC int16x4	saturated_sub(int16x4	a, int16x4		b)	{ return _mm_subs_pi16(a, b); }
	SIMD_FUNC int8x8	saturated_sub(int8x8	a, int8x8		b)	{ return _mm_subs_pi8(a, b); }
	SIMD_FUNC uint16x4	saturated_sub(uint16x4	a, uint16x4		b)	{ return _mm_subs_pu16(a, b); }
	SIMD_FUNC uint8x8	saturated_sub(uint8x8	a, uint8x8		b)	{ return _mm_subs_pu8(a, b); }
#endif
#ifdef __SSE2__
	SIMD_FUNC int16x8	saturated_add(int16x8	a, int16x8		b)	{ return _mm_adds_epi16(a, b); }
	SIMD_FUNC int8x16	saturated_add(int8x16	a, int8x16		b)	{ return _mm_adds_epi8 (a, b); }
	SIMD_FUNC uint16x8	saturated_add(uint16x8	a, uint16x8		b)	{ return _mm_adds_epu16(a, b); }
	SIMD_FUNC uint8x16	saturated_add(uint8x16	a, uint8x16		b)	{ return _mm_adds_epu8 (a, b); }
	SIMD_FUNC int16x8	saturated_sub(int16x8	a, int16x8		b)	{ return _mm_subs_epi16(a, b); }
	SIMD_FUNC int8x16	saturated_sub(int8x16	a, int8x16		b)	{ return _mm_subs_epi8 (a, b); }
	SIMD_FUNC uint16x8	saturated_sub(uint16x8	a, uint16x8		b)	{ return _mm_subs_epu16(a, b); }
	SIMD_FUNC uint8x16	saturated_sub(uint8x16	a, uint8x16		b)	{ return _mm_subs_epu8 (a, b); }

#ifdef __AVX2__
	SIMD_FUNC int16x16	saturated_add(int16x16	a, int16x16		b)	{ return _mm256_adds_epi16(a, b); }
	SIMD_FUNC int8x32	saturated_add(int8x32	a, int8x32		b)	{ return _mm256_adds_epi8 (a, b); }
	SIMD_FUNC uint16x16	saturated_add(uint16x16	a, uint16x16	b)	{ return _mm256_adds_epu16(a, b); }
	SIMD_FUNC uint8x32	saturated_add(uint8x32	a, uint8x32		b)	{ return _mm256_adds_epu8 (a, b); }
	SIMD_FUNC int16x16	saturated_sub(int16x16	a, int16x16		b)	{ return _mm256_subs_epi16(a, b); }
	SIMD_FUNC int8x32	saturated_sub(int8x32	a, int8x32		b)	{ return _mm256_subs_epi8 (a, b); }
	SIMD_FUNC uint16x16	saturated_sub(uint16x16	a, uint16x16	b)	{ return _mm256_subs_epu16(a, b); }
	SIMD_FUNC uint8x32	saturated_sub(uint8x32	a, uint8x32		b)	{ return _mm256_subs_epu8 (a, b); }

#ifdef __AVX512BW__
	SIMD_FUNC uint16x32	saturated_add(uint16x32	a, uint16x32	b) 	{ return _mm512_adds_epi16(a, b); }
	SIMD_FUNC uint8x64	saturated_add(uint8x64	a, uint8x64		b) 	{ return _mm512_adds_epi8 (a, b); }
	SIMD_FUNC uint16x32	saturated_add(uint16x32	a, uint16x32	b) 	{ return _mm512_adds_epu16(a, b); }
	SIMD_FUNC uint8x64	saturated_add(uint8x64	a, uint8x64		b) 	{ return _mm512_adds_epu8 (a, b); }
	SIMD_FUNC uint16x32	saturated_sub(uint16x32	a, uint16x32	b) 	{ return _mm512_subs_epi16(a, b); }
	SIMD_FUNC uint8x64	saturated_sub(uint8x64	a, uint8x64		b) 	{ return _mm512_subs_epi8 (a, b); }
	SIMD_FUNC uint16x32	saturated_sub(uint16x32	a, uint16x32	b) 	{ return _mm512_subs_epu16(a, b); }
	SIMD_FUNC uint8x64	saturated_sub(uint8x64	a, uint8x64		b) 	{ return _mm512_subs_epu8 (a, b); }

	SIMD_FUNC int16x32	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_adds_epi16	(s, m, a, b); }
	SIMD_FUNC int16x32	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_adds_epi16	(s, m, a, b); }
	SIMD_FUNC int18x32	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_adds_epi16	(s, m, a, b); }
	SIMD_FUNC int8x64	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_adds_epi8		(s, m, a, b); }
	SIMD_FUNC int8x64	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_adds_epi8	(s, m, a, b); }
	SIMD_FUNC int8x64	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_adds_epi8	(s, m, a, b); }
	SIMD_FUNC uint16x32	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_adds_epu16	(s, m, a, b); }
	SIMD_FUNC uint16x32	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_adds_epu16	(s, m, a, b); }
	SIMD_FUNC uint16x32	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_adds_epu16	(s, m, a, b); }
	SIMD_FUNC uint8x64	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_adds_epu8		(s, m, a, b); }
	SIMD_FUNC uint8x64	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_adds_epu8	(s, m, a, b); }
	SIMD_FUNC uint8x64	saturated_add(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_adds_epu8	(s, m, a, b); }
	SIMD_FUNC int16x32	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_subs_epi16	(s, m, a, b); }
	SIMD_FUNC int16x32	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_subs_epi16	(s, m, a, b); }
	SIMD_FUNC int18x32	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_subs_epi16	(s, m, a, b); }
	SIMD_FUNC int8x64	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_subs_epi8		(s, m, a, b); }
	SIMD_FUNC int8x64	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_subs_epi8	(s, m, a, b); }
	SIMD_FUNC int8x64	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_subs_epi8	(s, m, a, b); }
	SIMD_FUNC uint16x32	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_subs_epu16	(s, m, a, b); }
	SIMD_FUNC uint16x32	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_subs_epu16	(s, m, a, b); }
	SIMD_FUNC uint16x32	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_subs_epu16	(s, m, a, b); }
	SIMD_FUNC uint8x64	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm_mask_subs_epu8		(s, m, a, b); }
	SIMD_FUNC uint8x64	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm256_mask_subs_epu8	(s, m, a, b); }
	SIMD_FUNC uint8x64	saturated_sub(int m, int8x16  s, int8x16  a, int8x16  b) 	{ return _mm512_mask_subs_epu8	(s, m, a, b); }
#endif
#endif
#endif
#elif defined __ARM_NEON__
#endif

template<typename A, typename B> 	SIMD_FUNC auto saturated_add(A a, B b);
template<typename A, typename B> 	SIMD_FUNC auto saturated_sub(A a, B b);

#if defined __ARM_NEON__ || defined __SSE4_1__

	template<typename E, int N>	SIMD_FUNC oversized_t<E, N, vec<E,N>>	saturated_add1(vec<E,N> x, vec<E,N> y)	{ return simd_lo_hi(saturated_add(x.lo, y.lo), saturated_add(x.hi, y.hi)); }
	template<typename E, int N>	SIMD_FUNC undersized_t<E, N, vec<E,N>>	saturated_add1(vec<E,N> x, vec<E,N> y)	{ return shrink<N>(saturated_add(grow<pow2_ceil<N + 1>>(x), grow<pow2_ceil<N + 1>>(y))); }
	template<typename E, int N>	SIMD_FUNC normalsized_t<E, N, vec<E,N>>	saturated_add1(vec<E,N> x, vec<E,N> y)	{ typedef int_t<sizeof(E) * 2, is_signed<E>> E2; return to_sat<E>(to<E2>(x) + to<E2>(y)); }

	template<typename E, int N>	SIMD_FUNC oversized_t<E, N, vec<E,N>>	saturated_sub1(vec<E,N> x, vec<E,N> y)	{ return simd_lo_hi(saturated_sub(x.lo, y.lo), saturated_sub(x.hi, y.hi)); }
	template<typename E, int N>	SIMD_FUNC undersized_t<E, N, vec<E,N>>	saturated_sub1(vec<E,N> x, vec<E,N> y)	{ return shrink<N>(saturated_sub(grow<pow2_ceil<N + 1>>(x), grow<pow2_ceil<N + 1>>(y))); }
	template<typename E, int N>	SIMD_FUNC normalsized_t<E, N, vec<E,N>>	saturated_sub1(vec<E,N> x, vec<E,N> y)	{ typedef int_t<sizeof(E) * 2, is_signed<E>> E2; return to_sat<E>(to<E2>(x) - to<E2>(y)); }

#else

	template<typename E, int N> SIMD_FUNC vec<E, N> saturated_add1(vec<E,N> a, vec<E,N> b)	{ typedef int_t<sizeof(E) * 2, is_signed<E>> E2; return to_sat<E>(to<E2>(a) + to<E2>(b)); }
	template<typename E, int N> SIMD_FUNC vec<E, N> saturated_sub1(vec<E,N> a, vec<E,N> b)	{ typedef int_t<sizeof(E) * 2, is_signed<E>> E2; return to_sat<E>(to<E2>(a) - to<E2>(b)); }

#endif

	template<typename A, typename B> 	SIMD_FUNC auto saturated_add(A a, B b)	{ return saturated_add1<element_type<A>, num_elements_v<A>>(a, b); }
	template<typename A, typename B> 	SIMD_FUNC auto saturated_sub(A a, B b)	{ return saturated_sub1<element_type<A>, num_elements_v<A>>(a, b); }
	template<typename A, typename B> 	SIMD_FUNC auto saturated_mul(A a, B b)	{
		typedef element_type<A>	E;
		typedef int_t<sizeof(E) * 2, is_signed<E>> E2; 
		return to_sat<E>(fullmul(a, b));
	}

	template<typename E, int N, typename M> SIMD_FUNC vec<E, N> saturated_add(M m, vec<E,N> s, vec<E,N> a, vec<E,N> b)	{ return select(m, saturated_add(a, b), s); }
	template<typename E, int N, typename M> SIMD_FUNC vec<E, N> saturated_sub(M m, vec<E,N> s, vec<E,N> a, vec<E,N> b)	{ return select(m, saturated_sub(a, b), s); }
	template<typename E, int N, typename M> SIMD_FUNC vec<E, N> saturated_mul(M m, vec<E,N> s, vec<E,N> a, vec<E,N> b)	{ return select(m, saturated_mul(a, b), s); }

	template<typename M, typename S, typename A, typename B> SIMD_FUNC auto saturated_add(M m, S s, A a, B b)	{ return saturated_add<element_type<S>, num_elements_v<S>>(m, s, a, b); }
	template<typename M, typename S, typename A, typename B> SIMD_FUNC auto saturated_sub(M m, S s, A a, B b)	{ return saturated_sub<element_type<S>, num_elements_v<S>>(m, s, a, b); }
	template<typename M, typename S, typename A, typename B> SIMD_FUNC auto saturated_mul(M m, S s, A a, B b)	{ return saturated_mul<element_type<S>, num_elements_v<S>>(m, s, a, b); }

	static struct saturated_s {
		template<typename T> struct holder0 {
			typedef noref_cv_t<T>		T0;
			typedef element_type<T0>	E;
			typedef sint_for_t<E>		I;
			static const int N =  num_elements_v<T0>;
			T	t;

			constexpr holder0(T &&t) : t(forward<T>(t)) {}
			T		operator--() 	const	{ return t = *this - one; }
			T		operator++() 	const	{ return t = *this + one; }
			T0		operator--(int) const	{ T0 t0 = t; operator--(); return t0; }
			T0		operator++(int) const	{ T0 t0 = t; operator++(); return t0; }

			template<typename B> auto	operator+(B b) const { return saturated_add(t, b); }
			template<typename B> auto	operator-(B b) const { return saturated_sub(t, b); }
			template<typename B> auto	operator*(B b) const { return saturated_mul(t, b); }

			template<typename B> auto	operator+=(B b) const { return t = *this + b; }
			template<typename B> auto	operator-=(B b) const { return t = *this - b; }
			template<typename B> auto	operator*=(B b) const { return t = *this * b; }
		};

		template<int M, typename T> struct holder {
			typedef noref_cv_t<T>		T0;
			typedef element_type<T0>	E;
			typedef sint_for_t<E>		I;
			static const int N =  num_elements_v<T0>;
			static constexpr int pack_f(int i) { return nth_set_index(M|(~0u << N), i); }
			T	t;

			constexpr holder(T &&t) : t(forward<T>(t)) {}
			T		operator--() 	const	{ return t = *this - one; }
			T		operator++() 	const	{ return t = *this + one; }
			T0		operator--(int) const	{ T0 t0 = t; operator--(); return t0; }
			T0		operator++(int) const	{ T0 t0 = t; operator++(); return t0; }

			template<typename B> enable_if_t<!is_vec<B>, T0>	operator+(B b) const { return saturated_add(M, t + b, t, b); }
			template<typename B> enable_if_t<!is_vec<B>, T0>	operator-(B b) const { return saturated_sub(M, t - b, t, b); }
			template<typename B> enable_if_t<!is_vec<B>, T0>	operator*(B b) const { return saturated_mul(M, t * b, t, b); }

			template<typename B> enable_if_t<is_vec<B> && (bits(num_elements_v<B>) & M), T0>	operator+(B b) const { return saturated_add(M, t + grow<N>(b), t, grow<N>(b)); }
			template<typename B> enable_if_t<is_vec<B> && (bits(num_elements_v<B>) & M), T0>	operator-(B b) const { return saturated_sub(M, t - grow<N>(b), t, grow<N>(b)); }
			template<typename B> enable_if_t<is_vec<B> && (bits(num_elements_v<B>) & M), T0>	operator*(B b) const { return saturated_mul(M, t * grow<N>(b), t, grow<N>(b)); }

			template<typename B> auto	operator+=(B b) const { return t = *this + b; }
			template<typename B> auto	operator-=(B b) const { return t = *this - b; }
			template<typename B> auto	operator*=(B b) const { return t = *this * b; }
			template<typename B> auto	operator/=(B b) const { return t = *this / b; }
		};

		template<typename M, typename T> struct vholder {
			typedef noref_cv_t<T>		T0;
			typedef element_type<T0>	E;
			typedef sint_for_t<E>		I;
			static const int N =  num_elements_v<T0>;
			M	m;
			T	t;

			constexpr vholder(M &&m, T &&t) : m(forward<M>(m)), t(forward<T>(t)) {}
			T		operator--() 	const		{ return t = *this - one; }
			T		operator++() 	const		{ return t = *this + one; }
			T0		operator--(int) const		{ T0 t0 = t; operator--(); return t0; }
			T0		operator++(int) const		{ T0 t0 = t; operator++(); return t0; }

			template<typename B> auto	operator+(B b) const { return saturated_add(m, t + b, t, b); }
			template<typename B> auto	operator-(B b) const { return saturated_sub(m, t - b, t, b); }
			template<typename B> auto	operator*(B b) const { return saturated_mul(m, t * b, t, b); }

			template<typename B> auto	operator+=(B b) const { return t = *this + b; }
			template<typename B> auto	operator-=(B b) const { return t = *this - b; }
			template<typename B> auto	operator*=(B b) const { return t = *this * b; }
		};

		template<int M> struct modifier {
			template<typename T> constexpr holder<M, T>	operator()(T&& t) const { return forward<T>(t); }
		};
		modifier< 1>	x;
		modifier< 2>	y;
		modifier< 3>	xy;
		modifier< 4>	z;
		modifier< 5>	xz;
		modifier< 6>	yz;
		modifier< 7>	xyz;
		modifier< 8>	w;
		modifier< 9>	xw;
		modifier<10>	yw;
		modifier<11>	xyw;
		modifier<12>	zw;
		modifier<13>	xzw;
		modifier<14>	yzw;
		modifier<15>	xyzw;

		template<typename T>				constexpr holder0<T>	operator()(T&& t)					const { return forward<T>(t); }
		template<typename M, typename T>	constexpr vholder<M, T>	operator()(M&& m, T&& t)			const { return {forward<M>(m), forward<T>(t)}; }
		template<int M, typename T>			constexpr holder<M, T>	operator()(constant_int<M>, T&& t)	const { return forward<T>(t); }
	} saturated;

	//-----------------------------------------------------------------------------
	//	trigonometry
	//-----------------------------------------------------------------------------

	//	float

	template<int N>	no_inline void _sincos(vec<float, N> x, vec<float, N> *sin, vec<float, N> *cos) {
		auto	qf	= round(x * (two / pi));
		auto	q	= to<int>(qf);

		// Remainder in range [-pi/4..pi/4]
		auto	x1	= x - qf * (pi / two) - qf * 7.54978995489e-8f;
		auto	x2	= x1 * x1;

		// Compute both the sin and cos of the angles using a polynomial expression
		auto	cx	= horner(x2, one, -0.4999990225f, 0.0416566950f, -0.0013602249f);
		auto	sx	= horner(x2, one, -0.1666665247f, 0.0083320758f, -0.0001950727f) * x1;

		// Use the cosine where the offset is odd and the sin where the offset is even
		auto	sc	= !(q & 1);

		// Flip the sign of the result when (offset mod 4) = 1 or 2
		*sin = as<float>(as<int>(select(sc, sx, cx)) ^ (((q & 2) != 0) & 0x80000000));
		*cos = as<float>(as<int>(select(sc, cx, sx)) ^ ((((q + 1) & 2) != 0) & 0x80000000));
	}

	template<int N> no_inline vec<float, N> sin(vec<float, N> x) {
		auto	qf	= round(x * (two / pi));
		auto	q	= to<int>(qf);

		// Remainder in range [-pi/4..pi/4]
		auto	x1	= x - qf * (pi / two) - qf * 7.54978995489e-8f;
		auto	x2	= x1 * x1;

		// Compute both the sin and cos of the angles using a polynomial expression
		auto	cx	= horner(x2, one, -0.4999990225f, 0.0416566950f, -0.0013602249f);
		auto	sx	= horner(x2, one, -0.1666665247f, 0.0083320758f, -0.0001950727f) * x1;

		return as<float>(as<int>(select(!(q & 1), sx, cx)) ^ (((q & 2) != 0) & 0x80000000));
	}

	template<int N> no_inline vec<float, N> asin(vec<float, N> x) {
		auto	absx	= abs(x);
		auto	lt_half	= absx < half;
		auto	g		= select(lt_half, square(absx), (one - absx) * half);
		auto	R		= select(lt_half, absx, sqrt(g) * -2)
			* (one + ((-0.504400557f * g) + 0.933933258f) * g / ((g + -5.54846723f) * g + 5.603603363f));
		return copysign(select(lt_half, R, R + pi / two), x);
	}

	template<int N> SIMD_FUNC vec<float, N> _atan(vec<float, N> x) {
		auto	x2 = square(x);
		auto	hi = horner(x2, 0.1065626393f,	-0.0752896400f, 0.0429096138f, -0.0161657367f, 0.0028662257f);
		auto	lo = horner(x2, one,			-0.3333314528f, 0.1999355085f, -0.1420889944f);
		return (lo + hi * square(square(x2))) * x;
	}

	//	double

	template<int N>	no_inline void _sincos(vec<double, N> x, vec<double, N> *sin, vec<double, N> *cos) {
		static const double SC[]	= {
			-1.6666666666666666666667e-01, // 1/3!
			+8.3333333333333333333333e-03, // 1/5!
			-1.9841269841269841269841e-04, // 1/7!
			+2.7557319223985890652557e-06, // 1/9!
			-2.5052108385441718775052e-08, // 1/11!
			+1.6059043836821614599392e-10, // 1/13!
			-7.6471637318198164759011e-13, // 1/15!
			+2.8114572543455207631989e-15, // 1/17!
		};
		static const double CC[]	= {
			-5.0000000000000000000000e-01, // 1/2!
			+4.1666666666666666666667e-02, // 1/4!
			-1.3888888888888888888889e-03, // 1/6!
			+2.4801587301587301587302e-05, // 1/8!
			-2.7557319223985890652557e-07, // 1/10!
			+2.0876756987868098979210e-09, // 1/12!
			-1.1470745597729724713852e-11, // 1/14!
			+4.7794773323873852974382e-14, // 1/16!
		};
		auto	absx	= abs(x);
		auto	cycles	= round(absx / pi);
		auto	nx		= absx - pi * cycles;

		auto	y		= square(nx);
		auto	taylors	= horner(y, 0, SC[0], SC[1], SC[2], SC[3], SC[4], SC[5], SC[6], SC[7]);
		auto	taylorc	= horner(y, 0, CC[0], CC[1], CC[2], CC[3], CC[4], CC[5], CC[6], CC[7]);

		auto	sign	= trunc(cycles * 0.5) > (cycles * 0.5);
		*sin = as<double>(as<int64>(nx + nx * taylors) ^ ((as<int64>(x) ^ sign) & 0x8000000000000000ull));
		*cos = as<double>(as<int64>(taylorc + one) ^ (sign & 0x8000000000000000ull));
	}

	template<int N>	no_inline vec<double, N> sin(vec<double, N> x) {
		static const double SC[]	= {
			-1.6666666666666666666667e-01, // 1/3!
			+8.3333333333333333333333e-03, // 1/5!
			-1.9841269841269841269841e-04, // 1/7!
			+2.7557319223985890652557e-06, // 1/9!
			-2.5052108385441718775052e-08, // 1/11!
			+1.6059043836821614599392e-10, // 1/13!
			-7.6471637318198164759011e-13, // 1/15!
			+2.8114572543455207631989e-15, // 1/17!
		};
		auto	absx	= abs(x);
		auto	cycles	= round(absx / pi);
		auto	nx		= absx - pi * cycles;

		auto	y		= square(nx);
		auto	taylors	= horner(y, 0, SC[0], SC[1], SC[2], SC[3], SC[4], SC[5], SC[6], SC[7]);
		auto	sign	= trunc(cycles * 0.5) > (cycles * 0.5);
		return as<double>(as<int64>(nx + nx * taylors) ^ (as<int64>(x) & sign & 0x8000000000000000ull));
	}

	template<int N>	no_inline vec<double, N> asin(vec<double, N> x) {
		static const double PC[]	= {
			-2.7368494524164255994e+01,
			+5.7208227877891731407e+01,
			-3.9688862997404877339e+01,
			+1.0152522233806463645e+01,
			-6.9674573447350646411e-01,
		};
		static const double QC[]	= {
			-1.6421096714498560795e+02,
			+4.1714430248260412556e+02,
			-3.8186303361750149284e+02,
			+1.5095270841030604719e+02,
			-2.3823859153670238830e+01,
		};

		auto	absx	= abs(x);
		auto	lt_half	= absx < half;
		auto	g		= select(lt_half, square(absx), (one - absx) * half);
		auto	R		= select(lt_half, absx, sqrt(g) * -2)
			* (one + horner(g, PC[0], PC[1], PC[2], PC[3], PC[4]) * g / horner(g, QC[0], QC[1], QC[2], QC[3], QC[4]));
		return copysign(select(lt_half, R, R + pi / two), x);
	}

	template<int N>	SIMD_FUNC vec<double, N> _atan(vec<double, N> x) {
		auto	x2 = square(x);
		auto	hi = horner(x2, 0.1065626393f,	-0.0752896400f, 0.0429096138f, -0.0161657367f, 0.0028662257f);
		auto	lo = horner(x2, one,			-0.3333314528f, 0.1999355085f, -0.1420889944f);
		return (lo + hi * square(square(x2))) * x;
	}

	//	dispatchers

	template<typename X, typename=enable_if_t<is_vec<X>>> SIMD_FUNC auto sin(X x)		{ return sin<num_elements_v<X>>(x); }
	template<typename X, typename=enable_if_t<is_vec<X>>> SIMD_FUNC auto cos(X x)		{ return sin<num_elements_v<X>>(x + pi / two); }
	template<typename X> SIMD_FUNC auto sincos(X x)		{ return sin(concat(x + pi / two, x)); }
	template<typename X> SIMD_FUNC auto tan(X x)		{ auto sc = sincos(x); return sc.hi / sc.lo; }

	template<typename X, typename=enable_if_t<is_vec<X>>> SIMD_FUNC auto asin(X x)		{ return asin<num_elements_v<X>>(x); }
	template<typename X, typename=enable_if_t<is_vec<X>>> SIMD_FUNC auto acos(X x)		{ return pi / two - asin<num_elements_v<X>>(x); }

	template<typename E, int N> SIMD_FUNC auto atan(vec<E, N> x) {
		auto			gt_one	= abs(x) > one;
		auto			bias	= copysign(to<E>(gt_one & to<iso::sint_for_t<E>>(vec<E, N>(pi / two))), x);
		return _atan<N>(select(gt_one, -reciprocal(x), x)) + bias;
	}
	template<typename X>		SIMD_FUNC auto atan(X x)			{ return atan<element_type<X>, num_elements_v<X>>(x); }

	template<typename E, int N> no_inline vec<E, N> atan2(vec<E, N> y, vec<E, N> x) {
		auto	y_gt_x	= abs(y) > abs(x);
		auto	bias	= copysign(select(y_gt_x,
			to<E>(to<iso::sint_for_t<E>>(vec<E, N>(pi)) & (x < 0)),
			vec<E, N>(pi / two)
			), y);
		auto	res		= _atan<N>(select(y_gt_x, y, x) / select(y_gt_x, x, -y)) + bias;
		return select(abs(x) + abs(y) != 0, res, zero);
	}

	template<typename E, int N> SIMD_FUNC auto atan2(vec<E, N> x)	{ return atan2(x.lo, x.hi); }
	template<typename X>		SIMD_FUNC auto atan2(X x)			{ return atan2<element_type<X>, num_elements_v<X>>(x); }
	template<typename Y, typename X, typename=enable_if_t<is_vec<Y>>> SIMD_FUNC auto atan2(Y y, X x) { return atan2<element_type<Y>, num_elements_v<Y>>(x, y); }

	template<typename X> SIMD_FUNC	auto sincos(X x, decltype(sin(iso::declval<X>())) *sin, decltype(cos(iso::declval<X>())) *cos) {
		return _sincos<num_elements_v<X>>(x, sin, cos);
	}

	//-----------------------------------------------------------------------------
	// logs/powers
	//-----------------------------------------------------------------------------

	template<int N> no_inline vec<float, N> exp2(vec<float, N> x) {
		auto	i	= min(max(to<int>(x - half), -126), 126);
		auto	f	= x - to<float>(i);
		return select(x == -infinity, zero, as<float>((i + 127) << 23) * horner(f, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f));
	}

	//https://github.com/akohlmey/fastermath
	template<int N> no_inline vec<double, N> exp2(vec<double, N> x) {
		auto	i	= to<int64>(x - half);
		auto	f	= x - to<double>(i);
		auto	x2 	= f * f;
		auto	p 	= horner(x2, 1.51390680115615096133e3, 2.02020656693165307700e1, 2.30933477057345225087e-2) * f;
		auto	q 	= horner(x2, 4.36821166879210612817e3, 2.33184211722314911771e2, one);
		return select(x == -infinity, zero, as<double>((i + 1023) << 52) * (p / (q - p) * two + one));
	}

	//https://tech.ebayinc.com/engineering/fast-approximate-logarithms-part-iii-the-formulas/
	template<int N> no_inline vec<float, N> log2(vec<float, N> x) {
		auto	u	= as<uint32>(x);
		auto	gt	= (u & 0x00400000) * 2;
		auto	e 	= as<int>(((u + gt) >> 23)) - 127;
		auto	y	= as<float>((u & 0x007fffff) | ((127 << 23) - gt)) - one;

		const float a = 4.418408f;
		const float b = 9.143698f;
		const float c = 6.232189f;
		const float d = 6.337977f;
		
		return  select(x == zero, -infinity, to<float>(e) + y * (a * y + b) / (y * (y + c) + d));
	}
	
	//https://github.com/akohlmey/fastermath
	template<int N> no_inline vec<double, N> log2(vec<double, N> x) {
		auto	u	= as<int64>(x);
		auto	y	= as<double>((u & ((int64(1) << 52) - 1)) | (int64(1023) << 52));
		
		// want sqrt(0.5) < y < sqrt(2)
		auto	gt	= y > sqrt2;
		masked(gt, y) *= half;
		auto	e 	= (u >> 52) - 1023 - gt;

		y -= 1.0;

		auto	p = horner(y,
			7.70838733755885391666e0,
			1.79368678507819816313e1,
			1.44989225341610930846e1,
			4.70579119878881725854e0,
			4.97494994976747001425e-1,
			1.01875663804580931796e-4
		);
		auto	q = horner(y,
			2.31251620126765340583e1,
			7.11544750618563894466e1,
			8.29875266912776603211e1,
			4.52279145837532221105e1,
			1.12873587189167450590e1,
			one
		);

		auto	z = y * y;
		return select(x == zero, -infinity, to<double>(e) + (y * z * p / q - z * half + y) / log(two));
	}

	template<typename X>				SIMD_FUNC enable_if_t<is_vec<X>,X>	log2(const X &x)	{ return log2<num_elements_v<X>>(x); }
	template<typename X>				SIMD_FUNC enable_if_t<is_vec<X>,X>	exp2(const X &x)	{ return exp2<num_elements_v<X>>(x); }
	template<typename X>				SIMD_FUNC enable_if_t<is_vec<X>,X>	ln	(const X &x)	{ return log2<num_elements_v<X>>(x) * log(two); }
	template<typename X>				SIMD_FUNC enable_if_t<is_vec<X>,X>	exp	(const X &x)	{ return exp2<num_elements_v<X>>(x / log(two)); }

	template<int N>						SIMD_FUNC vec<float, N>				pow(vec<float, N> x, vec<float, N> y) 	{ return exp2<N>(log2<N>(x) * y); }
	template<int N>						SIMD_FUNC vec<double, N>			pow(vec<double, N> x, vec<double, N> y)	{ return exp2<N>(log2<N>(x) * y); }
	template<typename X, typename Y>	SIMD_FUNC enable_if_t<is_vec<X>,X>	pow(const X &x, const Y &y)				{ return pow<num_elements_v<X>>(x, y); }

} // namespace simd
} // namespace iso
#endif
