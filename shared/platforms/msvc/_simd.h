#if !defined SIMD_MSVC_H && defined _WIN32
#define SIMD_MSVC_H

#include "base/defs.h"
#include "base/soft_float.h"
#include "base/constants.h"
#include "base/swizzle.h"

#define SIMD_FUNC __forceinline

#if defined(_M_IX86) || defined(_M_X64)
#include <immintrin.h>

#define __SSE__
//8 128-bit XMM registers
//Single precision floating point vector instructions
//PREFETCH, SFENCE, FXSAVE, FXRSTOR, MOVNTQ, MOVNTPS

#define __SSE2__
//8, 16, 32, 64-bit and double precision vector instructions
//64-bit integer arithmetics in the MMX registers
//MOVNTI, MOVNTPD, PAUSE, LFENCE, MFENCE

#define __SSE3__
//FISTTP , LDDQU, MOVDDUP , MOVSHDUP , MOVSLDUP , ADDSUBPS, ADDSUPPD, HADDPS, HADDPD, HSUBPS, HSUBPD

#define __SSSE3__
//PSHUFB, PHADDW, PHADDSW, PHADDD, PMADDUBSW, PHSUBW, PHSUBSW, PHSUBD, PSIGNB, PSIGNW, PSIGND, PMULHRSW, PABSB, PABSW, PABSD, PALIGNR

#define __SSE4_1__
//MPSADBW, PHMINPOSUW, PMULDQ, PMULLD, DPPS, DPPD,
//BLEND.., PMIN.., PMAX.., ROUND.., INSERT.., EXTRACT.., PMOVSX.., PMOVZX..
//PTEST, PCMPEQQ, PACKUSDW, MOVNTDQA

#define __SSE4_2__
//CRC32, PCMPESTRI, PCMPESTRM, PCMPISTRI, PCMPISTRM, PCMPGTQ, POPCNT

//#define __AVX__
//16 256-bit YMM registers
//256-bit floating point vector instructions
//Almost all vector instructions now have versions with zero-extension into 256-bits, which have three operands in most cases
//VBROADCASTSS, VBROADCASTSD, VEXTRACTF128, VINSERTF128, VLDMXCSR, VMASKMOVPS, VMASKMOVPD, VPERMILPD, VPERMIL2PD, VPERMILPS, VPERMIL2PS, VPERM2F128, VSTMXCSR, VZEROALL, VZEROUPPER

//#define __AVX2__
//256-bit integer vector instructions
//ANDN, BEXTR, BLSI, BLSMSK, BLSR, BZHI, INVPCID, LZCNT, MULX, PEXT, PDEP, RORX, SARX, SHLX, SHRX, TZCNT,
//VBROADCASTI128, VBROADCASTSS, VBROADCASTSD, VEXTRACTI128,
//VGATHERDPD, VGATHERQPD, VGATHERDPS, VGATHERQPS, VPGATHERDD, VPGATHERQD, VPGATHERDQ, VPGATHERQQ,
//VINSERTI128, VPERM2I128, VPERMD, VPERMPD, VPERMPS, VPERMQ, VPMASKMOVD, VPMASKMOVQ, VPSLLVD, VPSLLVQ, VPSRAVD, VPSRLVD, VPSRLVQ

//FMA3
//Fused multiply and add instructions:
//VFMADDxxxxx, VFMADDSUBxxxxx, VFMSUBxxxxx, VFNMADDxxxxx, VFNMSUBxxxxx

//FMA4
//as FMA3, but with 4 different operands according to a preliminary Intel specification which is now supported only by AMD

//#define __AVX512F__
//32 512 bit ZMM registers
//Single and double precision floating point vector instructions
//Masked vector instructions using 8 vector mask registers k0 – k7
//Many new instructions

//#define __AVX512BW__	//ZMM support for 8 and 16-bit integers
//#define __AVX512DQ__	//additional ZMM instructions for 32-bit and 64-bit integers
//#define __AVX512VL__	//can apply AVX512 instructions to 128 and 256-bit vectors
//#define __AVX512CD__	//conflict detection instructions
//#define __AVX512ER__	//approximate exponential function, reciprocal and reciprocal square root
//#define __AVX512PF__	//gather and scatter prefetch

#elif defined _M_ARM64 || defined _M_ARM

#include <arm_neon.h>
#define __ARM_NEON__

#endif

namespace iso {

namespace simd {
template<typename T, int N, typename V = void>	struct vec;
template<typename T, int... I>					struct perm;
}

template<typename T, int N>		constexpr int num_elements_v<simd::vec<T, N>>		= N;
template<typename T, int...I>	constexpr int num_elements_v<simd::perm<T, I...>>	= sizeof...(I);
template<typename A, typename B>constexpr int num_elements_v<pair<A, B>>			= num_elements_v<A> + num_elements_v<B>;

namespace simd {

#ifdef __ARM_NEON__
static const int min_register_bits = 64;
#else
static const int min_register_bits = 128;
#endif

#ifdef __AVX512F__
static const int max_register_bits = 512;
#elif defined __AVX2__
static const int max_register_bits = 256;
#else
static const int max_register_bits = 128;
#endif


//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

template<int...I>			struct min_max;
template<int I0>			struct min_max<I0>	{ enum { min = I0, max = I0 }; };
template<int I0, int...I>	struct min_max<I0, I...> {
	typedef min_max<I...>R;
	enum {
		min = I0 < R::min ? I0 : R::min,
		max = I0 > R::max ? I0 : R::max
	};
};

template<typename T, int N>	static constexpr bool is_undersized = (N & (N-1)) != 0 || sizeof(T) * N * 8 < min_register_bits;
template<typename T, int N>	static constexpr bool is_oversized  = (N & (N-1)) == 0 && sizeof(T) * N * 8 > max_register_bits;

template<typename T, int N, typename U=void>	using undersized_t		= enable_if_t< is_undersized<T, N>,	U>;
template<typename T, int N, typename U=void>	using oversized_t		= enable_if_t< is_oversized<T,  N>,	U>;
template<typename T, int N, typename U=void>	using not_undersized_t	= enable_if_t<!is_undersized<T, N>,	U>;
template<typename T, int N, typename U=void>	using not_oversized_t	= enable_if_t<!is_oversized<T,  N>,	U>;
template<typename T, int N, typename U=void>	using normalsized_t		= enable_if_t<!is_oversized<T,  N> && !is_undersized<T,  N>,	U>;

//-----------------------------------------------------------------------------
// supported base types
//-----------------------------------------------------------------------------

typedef float16	hfloat;

template<typename T> static constexpr bool is_vec_element = is_builtin_num<T> || same_v<T, hfloat>;


//-----------------------------------------------------------------------------
// get internal (register) type for vec
//-----------------------------------------------------------------------------

template<int N> struct vtype_size;

template<int N> struct vtypeN {
	static const int N2 = pow2_ceil<N>;

	typedef typename vtype_size<N2 / 2>::type H;
	H lo, hi;
	vtypeN()	{}
	vtypeN(H lo, H hi) : lo(lo), hi(hi) {}
	friend vtypeN vnot(vtypeN a)			{ return {vnot(a.lo), vnot(a.hi)}; }
	friend vtypeN vand(vtypeN a, vtypeN b)	{ return {vand(a.lo, b.lo), vand(a.hi, b.hi)}; }
	friend vtypeN vandc(vtypeN a, vtypeN b) { return {vandc(a.lo, b.lo), vandc(a.hi, b.hi)}; }
	friend vtypeN vor(vtypeN a, vtypeN b)	{ return {vor(a.lo, b.lo), vor(a.hi, b.hi)}; }
	friend vtypeN vxor(vtypeN a, vtypeN b)	{ return {vxor(a.lo, b.lo), vxor(a.hi, b.hi)}; }
};


template<int N> struct vtype_size	{ typedef vtypeN<N> type; };

template<typename T, int N> struct vtype : vtype_size<sizeof(T) * 8 * N> {};

//-----------------------------------------------------------------------------
// underlying (register) types for simd vectors
//-----------------------------------------------------------------------------
#ifdef __SSE__

// for consistency, and so we can differentiate int from _m32i, etc
union __m16i;
union __m32i;
union __m64i;

union __m16i {
	uint16		u16;
	struct {uint8	lo, hi;};
	__m16i() {}
	constexpr __m16i(int16 u16)				: u16(u16) {}
	constexpr __m16i(__m128i m)				: u16(m.m128i_u16[0]) {}
	constexpr __m16i(__m32i m);
	constexpr __m16i(__m64i m);
	constexpr __m16i(uint8 lo, uint8 hi)	: lo(lo), hi(hi) {}
	operator __m128i() const { return _mm_cvtsi32_si128(u16); }
	operator __m128()	const { return _mm_castsi128_ps(*this); }

	friend __m16i vnot(__m16i a)			{ return ~a.u16; }
	friend __m16i vand(__m16i a, __m16i b)	{ return a.u16 & b.u16; }
	friend __m16i vandc(__m16i a, __m16i b)	{ return a.u16 & ~b.u16; }
	friend __m16i vor(__m16i a, __m16i b)	{ return a.u16 | b.u16; }
	friend __m16i vxor(__m16i a, __m16i b)	{ return a.u16 ^ b.u16; }
};

union __m32i {
	uint32		u32;
	struct {uint16	lo, hi;};
	__m32i() {}
	constexpr __m32i(uint32 u32)			: u32(u32) {}
	constexpr __m32i(__m16i m)				: u32(m.u16) {}
	constexpr __m32i(__m64i m);
	constexpr __m32i(__m128i m)				: u32(m.m128i_u32[0]) {}
	constexpr __m32i(__m16i lo, __m16i hi): lo(lo.u16), hi(hi.u16) {}
	operator __m128i()	const { return _mm_cvtsi32_si128(u32); }
	operator __m128()	const { return _mm_castsi128_ps(*this); }

	friend __m32i vnot(__m32i a)			{ return ~a.u32; }
	friend __m32i vand(__m32i a, __m32i b)	{ return a.u32 & b.u32; }
	friend __m32i vandc(__m32i a, __m32i b)	{ return a.u32 & ~b.u32; }
	friend __m32i vor(__m32i a, __m32i b)	{ return a.u32 | b.u32; }
	friend __m32i vxor(__m32i a, __m32i b)	{ return a.u32 ^ b.u32; }
};

constexpr __m16i::__m16i(__m32i m)	: u16(m.lo) {}


union __m64i {
	uint64		u64;
	struct {uint32 lo, hi; };
	__m64i() {}
	constexpr __m64i(uint64 u64)			: u64(u64) {}
	constexpr __m64i(__m16i m)				: lo(m.u16) {}
	constexpr __m64i(__m32i m)				: lo(m.u32) {}
	constexpr __m64i(__m128 m)				: u64(m.m128_u64[0]) {}
	constexpr __m64i(__m128i m)				: u64(m.m128i_u64[0]) {}
	//	constexpr __m64i(__m256i m)				: u64(m.m256i_u64[0]) {}
	constexpr __m64i(__m32i lo, __m32i hi)	: lo(lo.u32), hi(hi.u32) {}
	operator __m128i()	const { return _mm_cvtsi64_si128(u64); }
	operator __m128()	const { return _mm_castsi128_ps(*this); }

	friend __m64i vnot(__m64i a)			{ return ~a.u64; }
	friend __m64i vand(__m64i a, __m64i b)	{ return a.u64 & b.u64; }
	friend __m64i vandc(__m64i a, __m64i b) { return a.u64 & ~b.u64; }
	friend __m64i vor(__m64i a, __m64i b)	{ return a.u64 | b.u64; }
	friend __m64i vxor(__m64i a, __m64i b)	{ return a.u64 ^ b.u64; }
};

constexpr __m16i::__m16i(__m64i m)	: u16(uint16(m.lo)) {}
constexpr __m32i::__m32i(__m64i m)	: u32(m.lo) {}

template<> struct vtype_size<8>		{ typedef __m16i type; };
template<> struct vtype_size<16>	{ typedef __m16i type; };
template<> struct vtype_size<32>	{ typedef __m32i type; };
template<> struct vtype_size<64>	{ typedef __m64i type; };
template<> struct vtype_size<128>	{ typedef __m128i type; };

template<> struct vtype<float, 4>	{ typedef __m128 type; };
template<> struct vtype<double, 2>	{ typedef __m128d type; };

template<int N> struct vtype<bool, N>	{ typedef sint_bits_t<N> type; };


#ifdef __AVX2__
template<> struct vtype_size<256>	{ typedef __m256i type; };
template<> struct vtype<float, 8>	{ typedef __m256 type; };
template<> struct vtype<double, 4>	{ typedef __m256d type; };
#endif

#ifdef __AVX512F__
template<> struct vtype_size<512>	{ typedef __m512i type; };
template<> struct vtype<float, 16>	{ typedef __m512 type; };
template<> struct vtype<double, 8>	{ typedef __m512d type; };

template<> struct vtype<bool, 8>	{ typedef __mmask8 type; };
template<> struct vtype<bool, 16>	{ typedef __mmask16 type; };
template<> struct vtype<bool, 32>	{ typedef __mmask32 type; };
template<> struct vtype<bool, 64>	{ typedef __mmask64 type; };
#endif


//-----------------------------------------------------------------------------
// vcast: no-op casting of register types
//-----------------------------------------------------------------------------

template<typename T, typename U> SIMD_FUNC T vcast(U x)		{ return x; }
template<> SIMD_FUNC __m128		vcast<__m128>(__m128i x)	{ return _mm_castsi128_ps(x); }
template<> SIMD_FUNC __m128i	vcast<__m128i>(__m128 x)	{ return _mm_castps_si128(x); }
template<> SIMD_FUNC __m128i	vcast<__m128i>(__m128d x)	{ return _mm_castpd_si128(x); }
template<> SIMD_FUNC __m128d	vcast<__m128d>(__m128i x)	{ return _mm_castsi128_pd(x); }

template<> SIMD_FUNC __m256		vcast<__m256>(__m256i x)	{ return _mm256_castsi256_ps(x); }
template<> SIMD_FUNC __m256i	vcast<__m256i>(__m256 x)	{ return _mm256_castps_si256(x); }
template<> SIMD_FUNC __m256i	vcast<__m256i>(__m256d x)	{ return _mm256_castpd_si256(x); }
template<> SIMD_FUNC __m256d	vcast<__m256d>(__m256i x)	{ return _mm256_castsi256_pd(x); }


template<> SIMD_FUNC __m128		vcast<__m128>(__m256 x)		{ return _mm256_castps256_ps128(x); }
template<> SIMD_FUNC __m128i	vcast<__m128i>(__m256i x)	{ return _mm256_castsi256_si128(x); }
template<> SIMD_FUNC __m128d	vcast<__m128d>(__m256d x)	{ return _mm256_castpd256_pd128(x); }
template<> SIMD_FUNC __m128i	vcast<__m128i>(__m256 x)	{ return vcast<__m128i>(vcast<__m128>(x)); }
template<> SIMD_FUNC __m128		vcast<__m128>(__m256i x)	{ return vcast<__m128>(vcast<__m128i>(x)); }
template<> SIMD_FUNC __m128i	vcast<__m128i>(__m256d x)	{ return vcast<__m128i>(vcast<__m128d>(x)); }
template<> SIMD_FUNC __m128d	vcast<__m128d>(__m256i x)	{ return vcast<__m128d>(vcast<__m256d>(x)); }

template<> SIMD_FUNC __m256i	vcast<__m256i>(__m128i x)	{ return _mm256_castsi128_si256(x); }
template<> SIMD_FUNC __m256d	vcast<__m256d>(__m128i x)	{ return vcast<__m256d>(vcast<__m256i>(x)); }

template<> SIMD_FUNC __m32i		vcast<__m32i>(__m256 x)		{ return vcast<__m128i>(x); }
template<> SIMD_FUNC __m32i		vcast<__m32i>(__m256i x)	{ return vcast<__m128i>(x); }
template<> SIMD_FUNC __m32i		vcast<__m32i>(__m256d x)	{ return vcast<__m128i>(x); }

template<> SIMD_FUNC __m64i		vcast<__m64i>(__m256 x)		{ return vcast<__m128i>(x); }
template<> SIMD_FUNC __m64i		vcast<__m64i>(__m256i x)	{ return vcast<__m128i>(x); }
template<> SIMD_FUNC __m64i		vcast<__m64i>(__m256d x)	{ return vcast<__m128i>(x); }


#ifdef __AVX512F__
template<> SIMD_FUNC __m32i		vcast<__m32i>(__m512i x)	{ return vcast<__m128i>(x); }
template<> SIMD_FUNC __m64i		vcast<__m64i>(__m512i x)	{ return vcast<__m128i>(x); }
template<> SIMD_FUNC __m512i	vcast<__m512i>(__m512 x)	{ return _mm512_castps_si512(x); }
template<> SIMD_FUNC __m512i	vcast<__m512i>(__m512d x)	{ return _mm512_castpd_si512(x); }
template<> SIMD_FUNC __m512		vcast<__m512>(__m512i x)	{ return _mm512_castsi512_ps(x); }
template<> SIMD_FUNC __m512d	vcast<__m512d>(__m512i x)	{ return _mm512_castsi512_pd(x); }
#endif

template<typename T, int N> SIMD_FUNC T vcast(vtypeN<N> x)	{ return *(T*)&x; }

SIMD_FUNC __m128i interleave32(__m64i a, __m64i b)		{ return _mm_unpacklo_epi32(a, b); }
SIMD_FUNC __m256i interleave32(__m128i a, __m128i b)	{ return _mm256_unpacklo_epi32(_mm256_castsi128_si256(a), _mm256_castsi128_si256(b)); }

//-----------------------------------------------------------------------------
// boolean ops do not depend on field boundaries; these implement them generically
//-----------------------------------------------------------------------------

template<typename T> SIMD_FUNC T vnot(T a) { return ~a; }

SIMD_FUNC __m128i vnot(__m128i a)				{ return _mm_xor_si128(a, _mm_set1_epi32(-1)); }
SIMD_FUNC __m128i vand(__m128i a, __m128i b)	{ return _mm_and_si128(a, b); }
SIMD_FUNC __m128i vandc(__m128i a, __m128i b)	{ return _mm_andnot_si128(b, a); }
SIMD_FUNC __m128i vor(__m128i a, __m128i b)		{ return _mm_or_si128(a, b); }
SIMD_FUNC __m128i vxor(__m128i a, __m128i b)	{ return _mm_xor_si128(a, b); }

#ifdef __AVX2__
SIMD_FUNC __m256i vnot(__m256i a)				{ return _mm256_xor_si256(a, _mm256_set1_epi32(-1)); }
SIMD_FUNC __m256i vand(__m256i a, __m256i b)	{ return _mm256_and_si256(a, b); }
SIMD_FUNC __m256i vandc(__m256i a, __m256i b)	{ return _mm256_andnot_si256(b, a); }
SIMD_FUNC __m256i vor(__m256i a, __m256i b)		{ return _mm256_or_si256(a, b); }
SIMD_FUNC __m256i vxor(__m256i a, __m256i b)	{ return _mm256_xor_si256(a, b); }
#endif

#ifdef __AVX512F__
SIMD_FUNC __m512i vnot(__m512i a)				{ return _mm512_xor_si512(a, _mm512_set1_epi32(-1)); }
SIMD_FUNC __m512i vand(__m512i a, __m512i b)	{ return _mm512_and_si512(a, b); }
SIMD_FUNC __m512i vandc(__m512i a, __m512i b)	{ return _mm512_andnot_si512(b, a); }
SIMD_FUNC __m512i vor(__m512i a, __m512i b)		{ return _mm512_or_si512(a, b); }
SIMD_FUNC __m512i vxor(__m512i a, __m512i b)	{ return _mm512_xor_si512(a, b); }
#endif

//end of __SSE__

#elif defined __ARM_NEON__
template<> struct vtype_size<8>		{ typedef __n8 type; };
template<> struct vtype_size<16>	{ typedef __n16 type; };
template<> struct vtype_size<32>	{ typedef __n32 type; };
template<> struct vtype_size<64>	{ typedef __n64 type; };
template<> struct vtype_size<128>	{ typedef __n128 type; };
template<> struct vtype_size<256>	{ typedef vtypeN<256> type; };
template<> struct vtype_size<512>	{ typedef vtypeN<512> type; };

template<int N> struct vtype<bool, N>	{ typedef sint_bits_t<N> type; };

//-----------------------------------------------------------------------------
// vcast: no-op casting of register types
//-----------------------------------------------------------------------------

template<typename T, typename U> SIMD_FUNC T vcast(U x)		{ return *(T*)&x; }
//template<typename T, int N> SIMD_FUNC T vcast(vtypeN<N> x)	{ return *(T*)&x; }

//SIMD_FUNC __m128i interleave32(__m64i a, __m64i b)		{ return _mm_unpacklo_epi32(a, b); }
//SIMD_FUNC __m256i interleave32(__m128i a, __m128i b)	{ return _mm256_unpacklo_epi32(_mm256_castsi128_si256(a), _mm256_castsi128_si256(b)); }

//-----------------------------------------------------------------------------
// boolean ops do not depend on field boundaries; these implement them generically
//-----------------------------------------------------------------------------

template<typename T> SIMD_FUNC T vnot(T a) { return ~a; }

SIMD_FUNC __n64 vnot(__n64 a)				{ return neon_not(a); }
SIMD_FUNC __n64 vand(__n64 a, __n64 b)		{ return neon_and(a, b); }
SIMD_FUNC __n64 vandc(__n64 a, __n64 b)		{ return neon_bic(a, b); }
SIMD_FUNC __n64 vor(__n64 a, __n64 b)		{ return neon_orr(a, b); }
SIMD_FUNC __n64 vxor(__n64 a, __n64 b)		{ return neon_eor(a, b); }

SIMD_FUNC __n128 vnot(__n128 a)				{ return neon_notq(a); }
SIMD_FUNC __n128 vand(__n128 a, __n128 b)	{ return neon_andq(a, b); }
SIMD_FUNC __n128 vandc(__n128 a, __n128 b)	{ return neon_bicq(a, b); }
SIMD_FUNC __n128 vor(__n128 a, __n128 b)	{ return neon_orrq(a, b); }
SIMD_FUNC __n128 vxor(__n128 a, __n128 b)	{ return neon_eorq(a, b); }

#endif

template<typename T, int N> using vtype_t = typename vtype<T, N>::type;

//-----------------------------------------------------------------------------
// struct perm<T, I...>: swizzled vector
//-----------------------------------------------------------------------------

template<typename T, int...I>	struct permT2	{ using type = perm<T, I...>; };
template<typename T>			struct permT1	{ template<int...I > using type = typename permT2<T, I...>::type; };
template<typename T, int...I>	using permT	= typename permT2<T, I...>::type;

template<template<int...> class P, typename I>	struct gen_s;
template<template<int...> class P, size_t... I>	struct gen_s<P, meta::value_list<int, I...>> { typedef P<I...> type; };
template<template<int...> class P, typename I>	using gen = typename gen_s<P, I>::type;

//-----------------------------------------------------------------------------
// struct vec_maker
//-----------------------------------------------------------------------------

template<typename T, typename P0, typename...P> struct vec_maker;

//-----------------------------------------------------------------------------
// struct vec_base<T, N, V>: storage for a vec
//-----------------------------------------------------------------------------

// generic case
// defines lo, hi, even, odd, x, y, z, w, xy, xyz

template<int N, typename _V, template<int...> class P, typename X, typename Y = X, typename Z = Y, typename W = Z> struct vec_base {
	typedef _V	V;
	union {
		struct { X x; Y y; Z z; W w; };
		V	v;
		gen<P, meta::make_value_sequence<N / 2>>						lo;
		gen<P, meta::muladd<1, N/2, meta::make_value_sequence<N / 2>>>	hi;
		gen<P, meta::muladd<2, 0, meta::make_value_sequence<N / 2>>>	even;
		gen<P, meta::muladd<2, 1, meta::make_value_sequence<N / 2>>>	odd;

		//P<0>			x;			P<1>			y;			P<2>			z;			P<3>			w;
		P<0, 1>			xy;
		P<0, 1, 2>		xyz;
		P<0, 1, 2, 3>	xyzw;
	};
	vec_base() {}

	constexpr vec_base(V v) : v(v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend SIMD_FUNC auto& swizzle(const vec_base &p)	{ return reinterpret_cast<const P<I...>&>(p); }
	template<int...I> friend SIMD_FUNC auto& swizzle(vec_base &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

//-----------------------------------------------------------------------------
// struct vec<T, N>: approximation to clang's extended vector types
//-----------------------------------------------------------------------------

template<int S, typename T, int N>	SIMD_FUNC	vec<T, S>	shrink(vec<T, N> a)		{ return vcast<typename vec<T, S>::V>(a.v); }
template<int S, typename T, int N>	SIMD_FUNC	vec<T, S>	grow(vec<T, N> a)		{ return vcast<typename vec<T, S>::V>(a.v); }
template<int S, typename T, int...I>SIMD_FUNC	auto		shrink(perm<T, I...> a)	{ return (gen<permT1<T>::type, meta::left<S, meta::value_list<int, I...>>>&)a; }

template<typename T, int N>			SIMD_FUNC	vec<T, N * 2>	simd_lo_hi(vec<T, N> lo, vec<T, N> hi)	{ return {lo, hi}; }

template<typename T, int N> using vec_base1 = vec_base<N, vtype_t<T, pow2_ceil<N>>, typename permT1<T>::type, T>;

// general vec
template<typename T, int N, typename V> struct vec;

// normal-sized
template<typename T, int N> struct vec<T, N, enable_if_t<!is_oversized<T,  N> && !is_undersized<T,  N> && !same_v<T, hfloat> && !same_v<T, bool>>> : vec_base1<T, N> {
	typedef vec_base1<T, N>		base;
	typedef typename base::V	V;

	vec() {}
	SIMD_FUNC vec(V v)	: base(v)	{}
	SIMD_FUNC vec(T x);
	template<int I> SIMD_FUNC vec(perm<T,I> x) : vec(get(x)) {}
	template<typename P0, typename P1, typename...P> vec(P0 p0, P1 p1, P... p) : base(force_cast<base>(vec_maker<T, P0, P1, P...>(p0, p1, p...))) {}
	template<typename K>			constexpr explicit vec(const constant<K>&) : vec(K::template as<T>()) {}

	SIMD_FUNC vec&	operator=(const vec& b)		{ base::v = b.v; return *this; }

	SIMD_FUNC vec	operator-()			const;
	SIMD_FUNC vec	operator+ (vec b)	const;
	SIMD_FUNC vec	operator- (vec b)	const;
	SIMD_FUNC vec	operator* (vec b)	const;
	SIMD_FUNC vec	operator/ (vec b)	const;

	SIMD_FUNC auto	operator==(vec b)	const;
	SIMD_FUNC auto	operator> (vec b)	const;
	SIMD_FUNC auto	operator< (vec b)	const	{ return b > *this; }
	SIMD_FUNC auto	operator!=(vec b)	const	{ return ~(*this == b); }
	SIMD_FUNC auto	operator<=(vec b)	const	{ return ~(*this > b); }
	SIMD_FUNC auto	operator>=(vec b)	const	{ return ~(*this < b); }

	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator~()			const	{ return vnot(v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator& (vec b)	const	{ return vand(v, b.v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator| (vec b)	const	{ return vor(v, b.v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator^ (vec b)	const	{ return vxor(v, b.v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator/ (T b)		const	{ return idiv(*this, b); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator% (T b)		const	{ return imod(*this, b); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator% (vec b)	const	{ return imod(*this, b); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator<<(int b)	const	{ return shl(*this, b); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator>>(int b)	const	{ return shr(*this, b); }

	constexpr T operator[](intptr_t i)	const	{ return ((T*)this)[i]; }
	T&			operator[](intptr_t i)			{ return ((T*)this)[i]; }
};

// undersized vec
//template<typename T, int N> struct vec<T, N, undersized_t<T, N>> : vec_base1<T, N> {
template<typename T, int N> struct vec<T, N, enable_if_t<is_undersized<T, N> && !same_v<T, hfloat> && !same_v<T, bool>>> : vec_base1<T, N> {
	typedef vec_base1<T, N>		base;
	typedef typename base::V	V;
	static const int N2 = meta::max<pow2_ceil<N>, min_register_bits / (sizeof(T) * 8)>;

	SIMD_FUNC auto	boost()	const		{ return grow<N2>(*this); }

	vec() {}
	SIMD_FUNC vec(V v)	: base(v)	{}
	SIMD_FUNC vec(T x)	: base(vcast<V>(vec<T,N2>(x).v)) {}
//	template<int O> SIMD_FUNC vec(offset_type<T,O> x) : vec(x.t) {}
	template<int I> SIMD_FUNC vec(perm<T,I> x) : vec(get(x)) {}
	template<typename P0, typename P1, typename...P> vec(P0 p0, P1 p1, P... p) : base(force_cast<base>(vec_maker<T, P0, P1, P...>(p0, p1, p...))) {}
	template<typename K>			constexpr explicit vec(const constant<K>&) : vec(K::template as<T>()) {}

	SIMD_FUNC vec&	operator=(const vec& b)		{ v = b.v; return *this; }

	SIMD_FUNC auto	operator-()			const	{ return shrink<N>(-boost()); }

	SIMD_FUNC auto	operator+ (vec b)	const	{ return shrink<N>(boost() + b.boost()); }
	SIMD_FUNC auto	operator- (vec b)	const	{ return shrink<N>(boost() - b.boost()); }
	SIMD_FUNC auto	operator* (vec b)	const	{ return shrink<N>(boost() * b.boost()); }
	SIMD_FUNC auto	operator/ (vec b)	const	{ return shrink<N>(boost() / b.boost()); }

	SIMD_FUNC auto	operator==(vec b)	const	{ return shrink<N>(boost() == b.boost()); }
	SIMD_FUNC auto	operator< (vec b)	const	{ return shrink<N>(boost() <  b.boost()); }
	SIMD_FUNC auto	operator> (vec b)	const	{ return shrink<N>(boost() >  b.boost()); }
	SIMD_FUNC auto	operator!=(vec b)	const	{ return ~(*this == b); }
	SIMD_FUNC auto	operator<=(vec b)	const	{ return ~(*this > b); }
	SIMD_FUNC auto	operator>=(vec b)	const	{ return ~(*this < b); }

	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator~()			const	{ return vnot(v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator& (vec b)	const	{ return vand(v, b.v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator| (vec b)	const	{ return vor(v, b.v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator^ (vec b)	const	{ return vxor(v, b.v); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator/ (T b)		const	{ return shrink<N>(boost() / b); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator% (T b)		const	{ return shrink<N>(boost() % b); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator% (vec b)	const	{ return shrink<N>(boost() % b.boost()); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator<<(int b)	const	{ return shrink<N>(boost() << b); }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator>>(int b)	const	{ return shrink<N>(boost() >> b); }

	constexpr T operator[](intptr_t i)	const	{ return ((T*)this)[i]; }
	T&			operator[](intptr_t i)			{ return ((T*)this)[i]; }
};

// oversized vec
//template<typename T, int N> struct vec<T, N, oversized_t<T, N>> : vec_base1<T, N> {
template<typename T, int N> struct vec<T, N, enable_if_t<is_oversized<T,  N> && !same_v<T, hfloat> && !same_v<T, bool>>> : vec_base1<T, N> {
	typedef vec_base1<T, N>		base;
	typedef typename base::V	V;

	vec() {}
	SIMD_FUNC vec(V v)	: base(v)	{}
	SIMD_FUNC vec(T x)	{ lo = x; hi = x; }
//	template<int O> SIMD_FUNC vec(offset_type<T,O> x) : vec(x.t) {}
	template<int I> SIMD_FUNC vec(perm<T,I> x) : vec(get(x)) {}
	template<typename P0, typename P1, typename...P> vec(P0 p0, P1 p1, P... p) : base(force_cast<base>(vec_maker<T, P0, P1, P...>(p0, p1, p...))) {}
	template<typename K> explicit vec(const constant<K>&) : vec(K::template as<T>()) {}

	SIMD_FUNC vec&	operator=(const vec& b)		{ v = b.v; return *this; }

	SIMD_FUNC auto	operator!()			const	{ return simd_lo_hi(lo == T(0), hi == T(0)); }
	SIMD_FUNC vec	operator-()			const	{ return {-lo, -hi}; }

	SIMD_FUNC vec	operator+ (vec b)	const	{ return {lo + b.lo, hi + b.hi}; }
	SIMD_FUNC vec	operator- (vec b)	const	{ return {lo - b.lo, hi - b.hi}; }
	SIMD_FUNC vec	operator* (vec b)	const	{ return {lo * b.lo, hi * b.hi}; }
	SIMD_FUNC vec	operator/ (vec b)	const	{ return {lo / b.lo, hi / b.hi}; }

	SIMD_FUNC auto	operator==(vec b)	const	{ return simd_lo_hi(lo == b.lo, hi == b.hi); }
	SIMD_FUNC auto	operator< (vec b)	const	{ return simd_lo_hi(lo < b.lo, hi < b.hi); }
	SIMD_FUNC auto	operator> (vec b)	const	{ return simd_lo_hi(lo > b.lo, hi > b.hi); }
	SIMD_FUNC auto	operator!=(vec b)	const	{ return ~(*this == b); }
	SIMD_FUNC auto	operator<=(vec b)	const	{ return ~(*this > b); }
	SIMD_FUNC auto	operator>=(vec b)	const	{ return ~(*this < b); }

	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator~()			const	{ return {~lo, ~hi}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator& (vec b)	const	{ return {lo & b.lo, hi & b.hi}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator| (vec b)	const	{ return {lo | b.lo, hi | b.hi}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator^ (vec b)	const	{ return {lo ^ b.lo, hi ^ b.hi}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator/ (T b)		const	{ return {lo / b, hi / b}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator% (T b)		const	{ return {lo % b, hi % b}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator% (vec b)	const	{ return {lo % b.lo, hi % b.hi}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator<<(int b)	const	{ return {lo << b, hi << b}; }
	template<typename = enable_if_t<is_int<T>>> SIMD_FUNC vec	operator>>(int b)	const	{ return {lo >> b, hi >> b}; }

	constexpr T operator[](intptr_t i)	const	{ return ((T*)this)[i]; }
	T&			operator[](intptr_t i)			{ return ((T*)this)[i]; }
};

// vectors of hfloat

template<int N> struct vec<hfloat, N> : vec_base1<hfloat, N> {
	typedef vec_base1<hfloat, N> base;

	vec<int16, N> as_int16s()	const { return vec<int16, N>(v); }
	vec<float, N> as_floats()	const { return to<float>(*this); }
	vec<int16, N> compare_fix() const { return as_int16s() ^ ((as_int16s() >> 15) & int16(0x7fff)); }

	SIMD_FUNC vec()		{}
	SIMD_FUNC vec(V v)		: base(v)	{}
	SIMD_FUNC vec(hfloat x) : vec(vec<int16,N>(x.u)) {}
	SIMD_FUNC vec(vec<float, N> x) : vec(to<hfloat>(x)) {}
	template<typename P0, typename P1, typename...P> vec(P0 p0, P1 p1, P... p) : base(force_cast<base>(vec_maker<hfloat, P0, P1, P...>(p0, p1, p...))) {}

	SIMD_FUNC vec&	operator=(const vec& b)				{ v = b.v; return *this; }
	SIMD_FUNC vec&	operator=(vec<float, N> a)			{ v = to<hfloat>(a); return *this; }
	SIMD_FUNC operator vec<float, N>()			const	{ return to<float>(*this); }

	SIMD_FUNC auto	operator+ (vec<float, N> b)	const	{ return as_floats() + b; }
	SIMD_FUNC auto	operator- (vec<float, N> b)	const	{ return as_floats() - b; }
	SIMD_FUNC auto	operator* (vec<float, N> b)	const	{ return as_floats() * b; }
	SIMD_FUNC auto	operator/ (vec<float, N> b)	const	{ return as_floats() / b; }

	SIMD_FUNC vec	operator- ()				const	{ return (as_int16s() ^ int16(0x8000)).v; }
	SIMD_FUNC auto	operator==(vec b)			const	{ return as_int16s() == b.as_int16s(); }
	SIMD_FUNC auto	operator< (vec b)			const	{ return compare_fix() < b.compare_fix(); }
	SIMD_FUNC auto	operator> (vec b)			const	{ return compare_fix() > b.compare_fix(); }

	constexpr hfloat operator[](intptr_t i)		const	{ return ((T*)this)[i]; }
	hfloat&			operator[](intptr_t i)				{ return ((T*)this)[i]; }
};

// bool vecs
// 1 bit per field, as generated by _mm_.._mask intrinsics
template<int N> struct vec<bool, N> {
	vtype_t<bool, N> v;
	vec() {}
	constexpr vec(vtype_t<bool, N> v) : v(v) {}
	constexpr operator vtype_t<bool, N>()	const	{ return v; }

	constexpr vec	operator~()				const	{ return vnot(v); }
	constexpr vec	operator!()				const	{ return vnot(v); }
	constexpr bool	operator[](intptr_t i)	const	{ return (v >> i) & 1; }
};

//-----------------------------------------------------------------------------
// specialised vec_base for 0-4 components
//-----------------------------------------------------------------------------

template<typename _V, template<int...> class P, typename X> struct vec_base<0, _V, P, X> {
	typedef	_V	V;
	vec_base() {}
};

// vec<T,1> case: not expected to be explicitly used, but crops up in some template-generated code
template<typename _V, template<int...> class P, typename X> struct vec_base<1, _V, P, X> {
	typedef	_V	V;
	union {
		V	v;
		X	x;
	};
	vec_base() {}
	constexpr vec_base(V v)	: v(v) {}
	constexpr operator V() const { return v; }
	template<int...I> friend SIMD_FUNC auto& swizzle(const vec_base &p)	{ return reinterpret_cast<PC<I...>&>(p); }
	template<int...I> friend SIMD_FUNC auto& swizzle(vec_base &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

// vec<T,2>
template<typename _V, template<int...> class P, typename X, typename Y> struct vec_base<2, _V, P, X, Y> {
	typedef	_V	V;
	template<int... I> using PC = const P<I...>;
	union {
		V	v;
		struct { X x; Y y; };
		struct { X lo; Y hi; };

		// LHS + RHS
		//P<0>			x;			P<1>			y;
		P<0, 1>			xy;			P<1, 0>			yx;

		// RHS only
		PC<0, 0>		xx;			PC<1, 1>		yy;

		PC<0, 0, 0>		xxx;		PC<0, 0, 1>		xxy;		PC<0, 1, 0>		xyx;		PC<0, 1, 1>		xyy;
		PC<1, 0, 0>		yxx;		PC<1, 0, 1>		yxy;		PC<1, 1, 0>		yyx;		PC<1, 1, 1>		yyy;

		PC<0, 0, 0, 0>	xxxx;		PC<0, 0, 0, 1>	xxxy;		PC<0, 0, 1, 0>	xxyx;		PC<0, 0, 1, 1>	xxyy;
		PC<0, 1, 0, 0>	xyxx;		PC<0, 1, 0, 1>	xyxy;		PC<0, 1, 1, 0>	xyyx;		PC<0, 1, 1, 1>	xyyy;
		PC<1, 0, 0, 0>	yxxx;		PC<1, 0, 0, 1>	yxxy;		PC<1, 0, 1, 0>	yxyx;		PC<1, 0, 1, 1>	yxyy;
		PC<1, 1, 0, 0>	yyxx;		PC<1, 1, 0, 1>	yyxy;		PC<1, 1, 1, 0>	yyyx;		PC<1, 1, 1, 1>	yyyy;
	};

	vec_base() {}
	constexpr vec_base(V v)	: v(v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend SIMD_FUNC auto& swizzle(const vec_base &p)	{ return reinterpret_cast<PC<I...>&>(p); }
	template<int...I> friend SIMD_FUNC auto& swizzle(vec_base &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

// vec<T,3>
// assumed to use same storage as vec<T,4>
template<typename _V, template<int...> class P, typename X, typename Y, typename Z> struct vec_base<3, _V, P, X, Y, Z> {
	typedef	_V	V;
	template<int... I> using PC = const P<I...>;
	union {
		V	v;
		struct { X x; Y y; Z z; };
		// LHS + RHS
		//P<0>			x;			P<1>			y;			P<2>		z;

		P<0, 1>			xy;			P<0, 2>			xz;
		P<1, 0>			yx;			P<1, 2>			yz;
		P<2, 0>			zx;			P<2, 1>			zy;

		P<0, 1, 2>		xyz;		P<0, 2, 1>		xzy;
		P<1, 0, 2>		yxz;		P<1, 2, 0>		yzx;
		P<2, 0, 1>		zxy;		P<2, 1, 0>		zyx;

		// RHS only
		PC<0, 0>		xx;			PC<1, 1>		yy;			PC<2, 2>		zz;

		PC<0, 0, 0>		xxx;		PC<0, 0, 1>		xxy;		PC<0, 0, 2>		xxz;		PC<0, 1, 0>		xyx;
		PC<0, 1, 1>		xyy;		PC<0, 2, 0>		xzx;		PC<0, 2, 2>		xzz;

		PC<1, 0, 0>		yxx;		PC<1, 0, 1>		yxy;		PC<1, 1, 0>		yyx;		PC<1, 1, 1>		yyy;
		PC<1, 1, 2>		yyz;		PC<1, 2, 1>		yzy;		PC<1, 2, 2>		yzz;

		PC<2, 0, 0>		zxx;		PC<2, 0, 2>		zxz;		PC<2, 1, 1>		zyy;		PC<2, 1, 2>		zyz;
		PC<2, 2, 0>		zzx;		PC<2, 2, 1>		zzy;		PC<2, 2, 2>		zzz;

		PC<0, 0, 0, 0>	xxxx;		PC<0, 0, 0, 1>	xxxy;		PC<0, 0, 0, 2>	xxxz;		PC<0, 0, 1, 0>	xxyx;
		PC<0, 0, 1, 1>	xxyy;		PC<0, 0, 1, 2>	xxyz;		PC<0, 0, 2, 0>	xxzx;		PC<0, 0, 2, 1>	xxzy;
		PC<0, 0, 2, 2>	xxzz;		PC<0, 1, 0, 0>	xyxx;		PC<0, 1, 0, 1>	xyxy;		PC<0, 1, 0, 2>	xyxz;
		PC<0, 1, 1, 0>	xyyx;		PC<0, 1, 1, 1>	xyyy;		PC<0, 1, 1, 2>	xyyz;		PC<0, 1, 2, 0>	xyzx;
		PC<0, 1, 2, 1>	xyzy;		PC<0, 1, 2, 2>	xyzz;		PC<0, 2, 0, 0>	xzxx;		PC<0, 2, 0, 1>	xzxy;
		PC<0, 2, 0, 2>	xzxz;		PC<0, 2, 1, 0>	xzyx;		PC<0, 2, 1, 1>	xzyy;		PC<0, 2, 1, 2>	xzyz;
		PC<0, 2, 2, 0>	xzzx;		PC<0, 2, 2, 1>	xzzy;		PC<0, 2, 2, 2>	xzzz;

		PC<1, 0, 0, 0>	yxxx;		PC<1, 0, 0, 1>	yxxy;		PC<1, 0, 0, 2>	yxxz;		PC<1, 0, 1, 0>	yxyx;
		PC<1, 0, 1, 1>	yxyy;		PC<1, 0, 1, 2>	yxyz;		PC<1, 0, 2, 0>	yxzx;		PC<1, 0, 2, 1>	yxzy;
		PC<1, 0, 2, 2>	yxzz;		PC<1, 1, 0, 0>	yyxx;		PC<1, 1, 0, 1>	yyxy;		PC<1, 1, 0, 2>	yyxz;
		PC<1, 1, 1, 0>	yyyx;		PC<1, 1, 1, 1>	yyyy;		PC<1, 1, 1, 2>	yyyz;		PC<1, 1, 2, 0>	yyzx;
		PC<1, 1, 2, 1>	yyzy;		PC<1, 1, 2, 2>	yyzz;		PC<1, 2, 0, 0>	yzxx;		PC<1, 2, 0, 1>	yzxy;
		PC<1, 2, 0, 2>	yzxz;		PC<1, 2, 1, 0>	yzyx;		PC<1, 2, 1, 1>	yzyy;		PC<1, 2, 1, 2>	yzyz;
		PC<1, 2, 2, 0>	yzzx;		PC<1, 2, 2, 1>	yzzy;		PC<1, 2, 2, 2>	yzzz;

		PC<2, 0, 0, 0>	zxxx;		PC<2, 0, 0, 1>	zxxy;		PC<2, 0, 0, 2>	zxxz;		PC<2, 0, 1, 0>	zxyx;
		PC<2, 0, 1, 1>	zxyy;		PC<2, 0, 1, 2>	zxyz;		PC<2, 0, 2, 0>	zxzx;		PC<2, 0, 2, 1>	zxzy;
		PC<2, 0, 2, 2>	zxzz;		PC<2, 1, 0, 0>	zyxx;		PC<2, 1, 0, 1>	zyxy;		PC<2, 1, 0, 2>	zyxz;
		PC<2, 1, 1, 0>	zyyx;		PC<2, 1, 1, 1>	zyyy;		PC<2, 1, 1, 2>	zyyz;		PC<2, 1, 2, 0>	zyzx;
		PC<2, 1, 2, 1>	zyzy;		PC<2, 1, 2, 2>	zyzz;		PC<2, 2, 0, 0>	zzxx;		PC<2, 2, 0, 1>	zzxy;
		PC<2, 2, 0, 2>	zzxz;		PC<2, 2, 1, 0>	zzyx;		PC<2, 2, 1, 1>	zzyy;		PC<2, 2, 1, 2>	zzyz;
		PC<2, 2, 2, 0>	zzzx;		PC<2, 2, 2, 1>	zzzy;		PC<2, 2, 2, 2>	zzzz;
	};
	vec_base() {}
	constexpr vec_base(V v)	: v(v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend SIMD_FUNC auto& swizzle(const vec_base &p)	{ return reinterpret_cast<PC<I...>&>(p); }
	template<int...I> friend SIMD_FUNC auto& swizzle(vec_base &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

// vec<T,4>
template<typename _V, template<int...> class P, typename X, typename Y, typename Z, typename W> struct vec_base<4, _V, P, X, Y, Z, W> {
	typedef	_V	V;
	template<int... I> using PC = const P<I...>;
	union {
		V	v;
		struct { X x; Y y; Z z; W w; };
		P<0, 2>			even;
		P<1, 3>			odd;
		P<0, 1>			lo;
		P<2, 3>			hi;

		// LHS + RHS
		//P<0>			x;			P<1>			y;			P<2>			z;			P<3>			w;

		P<0, 1>			xy;			P<0, 2>			xz;			P<0, 3>			xw;
		P<1, 0>			yx;			P<1, 2>			yz;			P<1, 3>			yw;
		P<2, 0>			zx;			P<2, 1>			zy;			P<2, 3>			zw;
		P<3, 0>			wx;			P<3, 1>			wy;			P<3, 2>			wz;

		P<0, 1, 2>		xyz;		P<0, 1, 3>		xyw;		P<0, 2, 1>		xzy;		P<0, 2, 3>		xzw;
		P<0, 3, 1>		xwy;		P<0, 3, 2>		xwz;		P<1, 0, 2>		yxz;		P<1, 0, 3>		yxw;
		P<1, 2, 0>		yzx;		P<1, 2, 3>		yzw;		P<1, 3, 0>		ywx;		P<1, 3, 2>		ywz;
		P<2, 0, 1>		zxy;		P<2, 0, 3>		zxw;		P<2, 1, 0>		zyx;		P<2, 1, 3>		zyw;
		P<2, 3, 0>		zwx;		P<2, 3, 1>		zwy;		P<3, 0, 1>		wxy;		P<3, 0, 2>		wxz;
		P<3, 1, 0>		wyx;		P<3, 1, 2>		wyz;		P<3, 2, 0>		wzx;		P<3, 2, 1>		wzy;

		P<0, 1, 2, 3>	xyzw;		P<0, 1, 3, 2>	xywz;		P<0, 2, 1, 3>	xzyw;		P<0, 2, 3, 1>	xzwy;
		P<0, 3, 1, 2>	xwyz;		P<0, 3, 2, 1>	xwzy;		P<1, 0, 2, 3>	yxzw;		P<1, 0, 3, 2>	yxwz;
		P<1, 2, 0, 3>	yzxw;		P<1, 2, 3, 0>	yzwx;		P<1, 3, 0, 2>	ywxz;		P<1, 3, 2, 0>	ywzw;
		P<2, 0, 1, 3>	zxyw;		P<2, 0, 3, 1>	zxwy;		P<2, 1, 0, 3>	zyxw;		P<2, 1, 3, 0>	zywx;
		P<2, 3, 0, 1>	zwxy;		P<2, 3, 1, 0>	zwyx;		P<3, 0, 1, 2>	wxyz;		P<3, 0, 2, 1>	wxzy;
		P<3, 1, 0, 2>	wyxz;		P<3, 1, 2, 0>	wyzx;		P<3, 2, 0, 1>	wzxy;		P<3, 2, 1, 0>	wzyx;

		// RHS only
		PC<0, 0>		xx;			PC<1, 1>	yy;				PC<2, 2>		zz;			PC<3, 3>	ww;

		PC<0, 0, 0>		xxx;		PC<0, 1, 0>		xyx;		PC<0, 2, 0>		xzx;		PC<0, 3, 0>		xwx;
		PC<0, 0, 1>		xxy;		PC<0, 1, 1>		xyy;		PC<0, 0, 2>		xxz;		PC<0, 2, 2>		xzz;
		PC<0, 0, 3>		xxw;		PC<0, 3, 3>		xww;

		PC<1, 0, 0>		yxx;		PC<1, 1, 0>		yyx;		PC<1, 0, 1>		yxy;		PC<1, 1, 1>		yyy;
		PC<1, 2, 1>		yzy;		PC<1, 3, 1>		ywy;		PC<1, 1, 2>		yyz;		PC<1, 2, 2>		yzz;
		PC<1, 1, 3>		yyw;		PC<1, 3, 3>		yww;

		PC<2, 0, 0>		zxx;		PC<2, 2, 0>		zzx;		PC<2, 1, 1>		zyy;		PC<2, 2, 1>		zzy;
		PC<2, 0, 2>		zxz;		PC<2, 1, 2>		zyz;		PC<2, 2, 2>		zzz;		PC<2, 3, 2>		zwz;
		PC<2, 2, 3>		zzw;		PC<2, 3, 3>		zww;

		PC<3, 0, 0>		wxx;		PC<3, 3, 0>		wwx;		PC<3, 1, 1>		wyy;		PC<3, 3, 1>		wwy;
		PC<3, 2, 2>		wzz;		PC<3, 3, 2>		wwz;		PC<3, 0, 3>		wxw;		PC<3, 1, 3>		wyw;
		PC<3, 2, 3>		wzw;		PC<3, 3, 3>		www;

		PC<0, 0, 0, 0>	xxxx;		PC<0, 0, 0, 1>	xxxy;		PC<0, 0, 0, 2>	xxxz;		PC<0, 0, 0, 3>	xxxw;
		PC<0, 0, 1, 0>	xxyx;		PC<0, 0, 1, 1>	xxyy;		PC<0, 0, 1, 2>	xxyz;		PC<0, 0, 1, 3>	xxyw;
		PC<0, 0, 2, 0>	xxzx;		PC<0, 0, 2, 1>	xxzy;		PC<0, 0, 2, 2>	xxzz;		PC<0, 0, 2, 3>	xxzw;
		PC<0, 0, 3, 0>	xxwx;		PC<0, 0, 3, 1>	xxwy;		PC<0, 0, 3, 2>	xxwz;		PC<0, 0, 3, 3>	xxww;
		PC<0, 1, 0, 0>	xyxx;		PC<0, 1, 0, 1>	xyxy;		PC<0, 1, 0, 2>	xyxz;		PC<0, 1, 0, 3>	xyxw;
		PC<0, 1, 1, 0>	xyyx;		PC<0, 1, 1, 1>	xyyy;		PC<0, 1, 1, 2>	xyyz;		PC<0, 1, 1, 3>	xyyw;
		PC<0, 1, 2, 0>	xyzx;		PC<0, 1, 2, 1>	xyzy;		PC<0, 1, 2, 2>	xyzz;		PC<0, 1, 3, 0>	xywx;
		PC<0, 1, 3, 1>	xywy;		PC<0, 1, 3, 3>	xyww;		PC<0, 2, 0, 0>	xzxx;		PC<0, 2, 0, 1>	xzxy;
		PC<0, 2, 0, 2>	xzxz;		PC<0, 2, 0, 3>	xzxw;		PC<0, 2, 1, 0>	xzyx;		PC<0, 2, 1, 1>	xzyy;
		PC<0, 2, 1, 2>	xzyz;		PC<0, 2, 2, 0>	xzzx;		PC<0, 2, 2, 1>	xzzy;		PC<0, 2, 2, 2>	xzzz;
		PC<0, 2, 2, 3>	xzzw;		PC<0, 2, 3, 0>	xzwx;		PC<0, 2, 3, 2>	xzwz;		PC<0, 2, 3, 3>	xzww;
		PC<0, 3, 0, 0>	xwxx;		PC<0, 3, 0, 1>	xwxy;		PC<0, 3, 0, 2>	xwxz;		PC<0, 3, 0, 3>	xwxw;
		PC<0, 3, 1, 0>	xwyx;		PC<0, 3, 1, 1>	xwyy;		PC<0, 3, 1, 3>	xwyw;		PC<0, 3, 2, 0>	xwzx;
		PC<0, 3, 2, 2>	xwzz;		PC<0, 3, 2, 3>	xwzw;		PC<0, 3, 3, 0>	xwwx;		PC<0, 3, 3, 1>	xwwy;
		PC<0, 3, 3, 2>	xwwz;		PC<0, 3, 3, 3>	xwww;

		PC<1, 0, 0, 0>	yxxx;		PC<1, 0, 0, 1>	yxxy;		PC<1, 0, 0, 2>	yxxz;		PC<1, 0, 0, 3>	yxxw;
		PC<1, 0, 1, 0>	yxyx;		PC<1, 0, 1, 1>	yxyy;		PC<1, 0, 1, 2>	yxyz;		PC<1, 0, 1, 3>	yxyw;
		PC<1, 0, 2, 0>	yxzx;		PC<1, 0, 2, 1>	yxzy;		PC<1, 0, 2, 2>	yxzz;		PC<1, 0, 3, 0>	yxwx;
		PC<1, 0, 3, 1>	yxwy;		PC<1, 0, 3, 3>	yxww;		PC<1, 1, 0, 0>	yyxx;		PC<1, 1, 0, 1>	yyxy;
		PC<1, 1, 0, 2>	yyxz;		PC<1, 1, 0, 3>	yyxw;		PC<1, 1, 1, 0>	yyyx;		PC<1, 1, 1, 1>	yyyy;
		PC<1, 1, 1, 2>	yyyz;		PC<1, 1, 1, 3>	yyyw;		PC<1, 1, 2, 0>	yyzx;		PC<1, 1, 2, 1>	yyzy;
		PC<1, 1, 2, 2>	yyzz;		PC<1, 1, 2, 3>	yyzw;		PC<1, 1, 3, 0>	yywx;		PC<1, 1, 3, 1>	yywy;
		PC<1, 1, 3, 2>	yywz;		PC<1, 1, 3, 3>	yyww;		PC<1, 2, 0, 0>	yzxx;		PC<1, 2, 0, 1>	yzxy;
		PC<1, 2, 0, 2>	yzxz;		PC<1, 2, 1, 0>	yzyx;		PC<1, 2, 1, 1>	yzyy;		PC<1, 2, 1, 2>	yzyz;
		PC<1, 2, 1, 3>	yzyw;		PC<1, 2, 2, 0>	yzzx;		PC<1, 2, 2, 1>	yzzy;		PC<1, 2, 2, 2>	yzzz;
		PC<1, 2, 2, 3>	yzzw;		PC<1, 2, 3, 1>	yzwy;		PC<1, 2, 3, 2>	yzwz;		PC<1, 2, 3, 3>	yzww;
		PC<1, 3, 0, 0>	ywxx;		PC<1, 3, 0, 1>	ywxy;		PC<1, 3, 0, 3>	ywxw;		PC<1, 3, 1, 0>	ywyx;
		PC<1, 3, 1, 1>	ywyy;		PC<1, 3, 1, 2>	ywyz;		PC<1, 3, 1, 3>	ywyw;		PC<1, 3, 2, 0>	ywzx;
		PC<1, 3, 2, 1>	ywzy;		PC<1, 3, 2, 2>	ywzz;		PC<1, 3, 3, 0>	ywwx;		PC<1, 3, 3, 1>	ywwy;
		PC<1, 3, 3, 2>	ywwz;		PC<1, 3, 3, 3>	ywww;

		PC<2, 0, 0, 0>	zxxx;		PC<2, 0, 0, 1>	zxxy;		PC<2, 0, 0, 2>	zxxz;		PC<2, 0, 0, 3>	zxxw;
		PC<2, 0, 1, 0>	zxyx;		PC<2, 0, 1, 1>	zxyy;		PC<2, 0, 1, 2>	zxyz;		PC<2, 0, 2, 0>	zxzx;
		PC<2, 0, 2, 1>	zxzy;		PC<2, 0, 2, 2>	zxzz;		PC<2, 0, 2, 3>	zxzw;		PC<2, 0, 3, 0>	zxwx;
		PC<2, 0, 3, 2>	zxwz;		PC<2, 0, 3, 3>	zxww;		PC<2, 1, 0, 0>	zyxx;		PC<2, 1, 0, 1>	zyxy;
		PC<2, 1, 0, 2>	zyxz;		PC<2, 1, 1, 0>	zyyx;		PC<2, 1, 1, 1>	zyyy;		PC<2, 1, 1, 2>	zyyz;
		PC<2, 1, 1, 3>	zyyw;		PC<2, 1, 2, 0>	zyzx;		PC<2, 1, 2, 1>	zyzy;		PC<2, 1, 2, 2>	zyzz;
		PC<2, 1, 2, 3>	zyzw;		PC<2, 1, 3, 1>	zywy;		PC<2, 1, 3, 2>	zywz;		PC<2, 1, 3, 3>	zyww;
		PC<2, 2, 0, 0>	zzxx;		PC<2, 2, 0, 1>	zzxy;		PC<2, 2, 0, 2>	zzxz;		PC<2, 2, 0, 3>	zzxw;
		PC<2, 2, 1, 0>	zzyx;		PC<2, 2, 1, 1>	zzyy;		PC<2, 2, 1, 2>	zzyz;		PC<2, 2, 1, 3>	zzyw;
		PC<2, 2, 2, 0>	zzzx;		PC<2, 2, 2, 1>	zzzy;		PC<2, 2, 2, 2>	zzzz;		PC<2, 2, 2, 3>	zzzw;
		PC<2, 2, 3, 0>	zzwx;		PC<2, 2, 3, 1>	zzwy;		PC<2, 2, 3, 2>	zzwz;		PC<2, 2, 3, 3>	zzww;
		PC<2, 3, 0, 0>	zwxx;		PC<2, 3, 0, 2>	zwxz;		PC<2, 3, 0, 3>	zwxw;		PC<2, 3, 1, 1>	zwyy;
		PC<2, 3, 1, 2>	zwyz;		PC<2, 3, 1, 3>	zwyw;		PC<2, 3, 2, 0>	zwzx;		PC<2, 3, 2, 1>	zwzy;
		PC<2, 3, 2, 2>	zwzz;		PC<2, 3, 2, 3>	zwzw;		PC<2, 3, 3, 0>	zwwx;		PC<2, 3, 3, 1>	zwwy;
		PC<2, 3, 3, 2>	zwwz;		PC<2, 3, 3, 3>	zwww;

		PC<3, 0, 0, 0>	wxxx;		PC<3, 0, 0, 1>	wxxy;		PC<3, 0, 0, 2>	wxxz;		PC<3, 0, 0, 3>	wxxw;
		PC<3, 0, 1, 0>	wxyx;		PC<3, 0, 1, 1>	wxyy;		PC<3, 0, 1, 3>	wxyw;		PC<3, 0, 2, 0>	wxzx;
		PC<3, 0, 2, 2>	wxzz;		PC<3, 0, 2, 3>	wxzw;		PC<3, 0, 3, 0>	wxwx;		PC<3, 0, 3, 1>	wxwy;
		PC<3, 0, 3, 2>	wxwz;		PC<3, 0, 3, 3>	wxww;		PC<3, 1, 0, 0>	wyxx;		PC<3, 1, 0, 1>	wyxy;
		PC<3, 1, 0, 3>	wyxw;		PC<3, 1, 1, 0>	wyyx;		PC<3, 1, 1, 1>	wyyy;		PC<3, 1, 1, 2>	wyyz;
		PC<3, 1, 1, 3>	wyyw;		PC<3, 1, 2, 1>	wyzy;		PC<3, 1, 2, 2>	wyzz;		PC<3, 1, 2, 3>	wyzw;
		PC<3, 1, 3, 0>	wywx;		PC<3, 1, 3, 1>	wywy;		PC<3, 1, 3, 2>	wywz;		PC<3, 1, 3, 3>	wyww;
		PC<3, 2, 0, 0>	wzxx;		PC<3, 2, 0, 2>	wzxz;		PC<3, 2, 0, 3>	wzxw;		PC<3, 2, 1, 1>	wzyy;
		PC<3, 2, 1, 2>	wzyz;		PC<3, 2, 1, 3>	wzyw;		PC<3, 2, 2, 0>	wzzx;		PC<3, 2, 2, 1>	wzzy;
		PC<3, 2, 2, 2>	wzzz;		PC<3, 2, 2, 3>	wzzw;		PC<3, 2, 3, 0>	wzwx;		PC<3, 2, 3, 1>	wzwy;
		PC<3, 2, 3, 2>	wzwz;		PC<3, 2, 3, 3>	wzww;		PC<3, 3, 0, 0>	wwxx;		PC<3, 3, 0, 1>	wwxy;
		PC<3, 3, 0, 2>	wwxz;		PC<3, 3, 0, 3>	wwxw;		PC<3, 3, 1, 0>	wwyx;		PC<3, 3, 1, 1>	wwyy;
		PC<3, 3, 1, 2>	wwyz;		PC<3, 3, 1, 3>	wwyw;		PC<3, 3, 2, 0>	wwzx;		PC<3, 3, 2, 1>	wwzy;
		PC<3, 3, 2, 2>	wwzz;		PC<3, 3, 2, 3>	wwzw;		PC<3, 3, 3, 0>	wwwx;		PC<3, 3, 3, 1>	wwwy;
		PC<3, 3, 3, 2>	wwwz;		PC<3, 3, 3, 3>	wwww;
	};
	vec_base() {}
	constexpr vec_base(V v)	: v(v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend SIMD_FUNC auto& swizzle(const vec_base &p)	{ return reinterpret_cast<PC<I...>&>(p); }
	template<int...I> friend SIMD_FUNC auto& swizzle(vec_base &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

//-----------------------------------------------------------------------------
// struct perm<T, I...>: swizzled vector specialisation
//-----------------------------------------------------------------------------

template<int BPE> struct vshuffle_s;/* {
	typedef vtype_t<I, 128 / BPE>	V;
	typedef vshuffle_s<V, BPE>		B;
	template<int...I> static auto get(T a)	{ return B::get<I...>(vcast<V>(a)); }
};*/

template<int BPE, int... I, typename T>	auto vshuffle(T a)								{ return vshuffle_s<BPE>::get<I...>(a); }
template<int BPE, int... I, typename T>	auto vshuffle(T a, meta::value_list<int, I...>)	{ return vshuffle_s<BPE>::get<I...>(a); }

template<int I0, int I1, int...I, typename T> void scatter_write(T *d, const T *s) {
	d[I0] = *s;
	scatter_write<I1, I...>(d, s + 1);
}
template<int I0, typename T> static void scatter_write(T *d, const T *s) {
	d[I0] = *s;
}

template<int...DI, int...SI, typename T> void scatter_write(T *d, const T *s, meta::value_list<int, DI...>, meta::value_list<int, SI...>) {
	auto dummy = {(d[DI] = s[SI])...};
}

#ifdef __SSE__

template<bool split> static __m256i shuffle256helper(__m256i a, __m256i m) {
	return _mm256_shuffle_epi8(a, m);
}
template<> static __m256i shuffle256helper<true>(__m256i a, __m256i m) {
	auto	lo = _mm256_shuffle_epi8(a, m);
	auto	hi = _mm256_shuffle_epi8(_mm256_permute2x128_si256(a, a, 0x01), m);
	return _mm256_blendv_epi8(lo, hi,  _mm256_sll_epi16(m, _mm_cvtsi64_si128(3)));
}

#define SWIZ_PAIR(X)			((X) * 0x202 + 0x0100)
#define SHUFMASK2(X, Y, Z, W)	((X & 3) | ((Y & 3) << 2) | ((Z & 3) << 4) | ((W & 3) << 6))

#endif

//-----------------------------------------------------------------------------
// 8 bit swizzles
//-----------------------------------------------------------------------------

template<> struct vshuffle_s<8> {
#ifdef __SSE__
	template<
		int8 I0 = 0, int8 I1 = 1, int8 I2  = 2,  int8 I3  = 3,  int8 I4  = 4,  int8 I5  = 5,  int8  I6 = 6,  int8 I7  = 7,
		int8 I8 = 8, int8 I9 = 9, int8 I10 = 10, int8 I11 = 11, int8 I12 = 12, int8 I13 = 13, int8 I14 = 14, int8 I15 = 15
	> static __m128i get(__m128i a) {
		return _mm_shuffle_epi8(a, _mm_set_epi8(I15, I14, I13, I12, I11, I10, I9, I8, I7, I6, I5, I4, I3, I2, I1, I0));
	}
	template<> static __m128i get<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15>(__m128i a) {
		return a;
	}
	
	template<
		int8 I0  = 0,  int8 I1  = 1,  int8 I2  = 2,  int8 I3  = 3,  int8 I4  = 4,  int8 I5  = 5,  int8 I6  = 6,  int8 I7  = 7,
		int8 I8  = 8,  int8 I9  = 9,  int8 I10 = 10, int8 I11 = 11, int8 I12 = 12, int8 I13 = 13, int8 I14 = 14, int8 I15 = 15,
		int8 I16 = 16, int8 I17 = 17, int8 I18 = 18, int8 I19 = 19, int8 I20 = 20, int8 I21 = 21, int8 I22 = 22, int8 I23 = 23,
		int8 I24 = 24, int8 I25 = 25, int8 I26 = 26, int8 I27 = 27, int8 I28 = 28, int8 I29 = 29, int8 I30 = 30, int8 I31 = 31
	> static __m256i get(__m256i a) {
		return shuffle256helper<!!((meta::reduce_or<I0,  I1,  I2,  I3,  I4,  I5,  I6,  I7,  I8,  I9,  I10, I11, I12, I13, I14, I15> | ~meta::reduce_and<I16, I17, I18, I19, I20, I21, I22, I23, I24, I25, I25, I27, I28, I29, I30, I31>) & 16)>(
			a,
			_mm256_set_epi8(
				I31^16, I30^16, I29^16, I28^16, I27^16, I26^16, I25^16, I24^16, I23^16, I22^16, I21^16, I20^16, I19^16, I18^16, I17^16, I16^16,
				I15, I14, I13, I12, I11, I10, I9,  I8,  I7,  I6,  I5,  I4,  I3,  I2,  I1,  I0
			)
		);
	}
	template<> static __m256i get<
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	>(__m256i a) {
		return a;
	}
	
	template<int X, int Y, typename T> static __m16i get(const T& a) {
		auto* i = (const uint8*)&a;
		return {i[X], i[Y]};
	}
	template<int X, int Y, int Z, int W = 3, typename T> static __m32i get(const T& a) {
		return {get<X,Y>(a), get<Z,W>(a)};
	}
	template<int I0, int I1, int I2, int I3, int I4, int I5, int I6, int I7, typename T> static __m64i get(const T& a) {
		return {get<I0, I1, I2, I3>(a), get<I4, I5, I6, I7>(a)};
	}
	template<
		int I0, int I1, int I2,  int I3,  int I4,  int I5,  int I6,  int I7,
		int I8, int I9, int I10, int I11, int I12, int I13, int I14, int I15,
		typename T
	> static __m128i get(const T& a) {
		auto* i = (const int8*)&a;
		return _mm_set_epi8(
			i[I15], i[I14], i[I13], i[I12], i[I11], i[I10], i[I9], i[I8],
			i[I7],  i[I6],  i[I5],  i[I4],  i[I3],  i[I2],  i[I1], i[I0]
		);
	}
	template<
		int I0,  int I1,  int I2,  int I3,  int I4,  int I5,  int I6,  int I7,
		int I8,  int I9,  int I10, int I11, int I12, int I13, int I14, int I15,
		int I16, int I17, int I18, int I19, int I20, int I21, int I22, int I23,
		int I24, int I25, int I26, int I27, int I28, int I29, int I30, int I31,
		typename T
	> static __m128i get(const T& a) {
		auto* i = (const int8*)&a;
		return _mm256_set_epi8(
			i[I31], i[I30], i[I29], i[I28], i[I27], i[I26], i[I25], i[I24],
			i[I23], i[I22], i[I21], i[I20], i[I19], i[I18], i[I17], i[I16],
			i[I15], i[I14], i[I13], i[I12], i[I11], i[I10], i[I9],  i[I8],
			i[I7],  i[I6],  i[I5],  i[I4],  i[I3],  i[I2],  i[I1],  i[I0]
		);
	}
#elif defined __ARM_NEON__
	template<int...I, typename T> static auto get(const T& a) {
		auto* i = (const int8*)&a;
		return (vtype_t<int8, pow2_ceil<sizeof...(I)>>&)meta::array<int8, sizeof...(I)>{{i[I]...}};
	}
#endif
};

//-----------------------------------------------------------------------------
// 16 bit swizzles
//-----------------------------------------------------------------------------

template<int> struct T_int {};

template<> struct vshuffle_s<16> {
#ifdef __SSE__
	template<int X, int Y = 1, int Z = 2, int W = 3> static __m64i get(__m64i a) {
		return _mm_shufflelo_epi16(a, SHUFMASK2(X, Y, Z, W));
	}
	template<> static __m64i get<0, 1, 2, 3>(__m64i a) {
		return a;
	}

	template<int X, int Y, int Z, int W> static __m128i get(__m128i a, T_int<0>) {
		return _mm_shufflelo_epi16(a, SHUFMASK2(X, Y, Z, W));
	}
	template<int X, int Y, int Z, int W> static __m128i get(__m128i a, T_int<1>) {
		return _mm_shufflehi_epi16(a, SHUFMASK2(X, Y, Z, W));
	}
	template<int X, int Y, int Z, int W> static __m128i get(__m128i a, T_int<2>) {
		return get<X, Y, Z, W, 4>(a);
	}
	template<int X, int Y = 1, int Z = 2, int W = 3> static __m128i get(__m128i a) {
		return get<X, Y, Z, W>(a, T_int<((X | Y | Z | W) < 4) ? 0 : 2/*((X & Y & Z & W) >= 4) ? 1 : 2*/>());
	}
	template<> static __m128i get<0, 1, 2, 3>(__m128i a) {
		return a;
	}
	template<
		int8 I0, int8 I1, int8 I2, int8 I3, int8 I4, int8 I5 = 5, int8 I6 = 6, int8 I7 = 7
	> static __m128i get(__m128i a) {
		return _mm_shuffle_epi8(a, _mm_set_epi16(
			SWIZ_PAIR(I7), SWIZ_PAIR(I6), SWIZ_PAIR(I5), SWIZ_PAIR(I4), SWIZ_PAIR(I3), SWIZ_PAIR(I2), SWIZ_PAIR(I1), SWIZ_PAIR(I0)
		));
	}
	template<> static __m128i get<0, 1, 2, 3, 4, 5, 6, 7>(__m128i a) {
		return a;
	}
	
	template<
		int8 I0,	 int8 I1,	  int8 I2  = 2,  int8 I3  = 3,  int8 I4  = 4,  int8 I5  = 5,  int8 I6  = 6,  int8 I7  = 7,
		int8 I8 = 8, int8 I9 = 9, int8 I10 = 10, int8 I11 = 11, int8 I12 = 12, int8 I13 = 13, int8 I14 = 14, int8 I15 = 15
	> static __m256i get(__m256i a) {
		return shuffle256helper<!!((meta::reduce_or<I0,  I1,  I2,  I3,  I4,  I5,  I6,  I7> | ~meta::reduce_and<I8,  I9,  I10, I11, I12, I13, I14, I15>) & 8)>(
			a,
			_mm256_set_epi16(
				SWIZ_PAIR(I15^8), SWIZ_PAIR(I14^8), SWIZ_PAIR(I13^8), SWIZ_PAIR(I12^8), SWIZ_PAIR(I11^8), SWIZ_PAIR(I10^8), SWIZ_PAIR(I9^8), SWIZ_PAIR(I8^8),
				SWIZ_PAIR(I7),  SWIZ_PAIR(I6),  SWIZ_PAIR(I5),  SWIZ_PAIR(I4),  SWIZ_PAIR(I3),  SWIZ_PAIR(I2),  SWIZ_PAIR(I1), SWIZ_PAIR(I0)
			)
		);
	}
	template<> static __m256i get<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15>(__m256i a) {
		return a;
	}
	
	template<int X, int Y, typename T> static __m32i get(const T& a) {
		auto* i = (const int16*)&a;
		return i[X] | (i[Y] << 16);
	}
	template<int X, int Y, int Z, int W = 3, typename T> static __m64i get(const T& a) {
		auto* i = (const int16*)&a;
		return i[X] | (i[Y] << 16) | (int64(i | (i[W] << 8)) << 32);
	}
	template<int I0, int I1, int I2, int I3, int I4, int I5, int I6, int I7, typename T> static auto get(const T& a) {
		auto* i = (const int16*)&a;
		return _mm_set_epi16(i[I7], i[I6], i[I5], i[I4], i[I3], i[I2], i[I1], i[I0]);
	}
	template<
		int I0, int I1, int I2, int I3, int I4, int I5, int I6, int I7,
		int I8, int I9, int I10, int I11, int I12, int I13, int I14, int I15,
		typename T
	> static auto get(const T& a) {
		auto* i = (const int16*)&a;
		return _mm256_set_epi16(
			i[I15], i[I14], i[I13], i[I12], i[I11], i[I10], i[I9], i[I8],
			i[I7],  i[I6],  i[I5],  i[I4],  i[I3],  i[I2],  i[I1], i[I0]
		);
	}
#elif defined __ARM_NEON__
	template<int...I, typename T> static auto get(const T& a) {
		auto* i = (const int16*)&a;
		return (vtype_t<int16, pow2_ceil<sizeof...(I)>>&)meta::array<int16, sizeof...(I)>{{i[I]...}};
	}
#endif
	template<
		int I0, int I1, int I2,  int I3,  int I4,  int I5,  int I6,  int I7,
		int I8, int I9, int I10, int I11, int I12, int I13, int I14, int I15,
		int I16, int ... I,
		typename T
	> static auto get(const T& a) {
		static const int N2 = pow2_ceil<sizeof...(I) + 17>;
		vtypeN<N2 * 16> b;
		b.lo = vshuffle<16>(a, left<N2 / 2, meta::value_list<int, I0, I1, I2, I3, I4, I5, I6, I7, I8, I9, I10, I11, I12, I13, I14, I15, I16, I...>());
		b.hi = vshuffle<16>(a, right<sizeof...(I) + 17 - N2 / 2, meta::value_list<int, I16, I...>());
		return b;
	}
};

//-----------------------------------------------------------------------------
// 32 bit swizzles
//-----------------------------------------------------------------------------

template<> struct vshuffle_s<32> {
#ifdef __SSE__
	template<int X, int Y = 1, int Z = 2, int W = 3> static __m128i get(__m128i a) {
		return _mm_shuffle_epi32(a, SHUFMASK2(X, Y, Z, W));
	}
	template<> static __m128i get<0, 1, 2, 3>(__m128i a) {
		return a;
	}
	
	template<
		int8 I0 = 0, int8 I1 = 1, int8 I2 = 2, int8 I3 = 3, int8 I4 = 4, int8 I5 = 5, int8 I6 = 6, int8 I7 = 7
	> static __m256i get(__m256i a) {
		return _mm256_permutevar8x32_epi32(a, _mm256_set_epi32(I7, I6, I5, I4, I3, I2, I1, I0));
	}
	template<> static __m256i get<0, 1, 2, 3, 4, 5, 6, 7>(__m256i a) {
		return a;
	}
//	template<> static __m256i get<0, 4, 1, 5, 4, 5, 6, 7>(__m256i a) {
//		return vcast<__m256i>(_mm_unpacklo_epi32(vcast<__m128i>(a), _mm256_extracti128_si256(a, 1)));
//	}
//	template<> static __m256i get<2, 6, 3, 7, 4, 5, 6, 7>(__m256i a) {
//		return vcast<__m256i>(_mm_unpackhi_epi32(vcast<__m128i>(a), _mm256_extracti128_si256(a, 1)));
//	}
	
	template<int X, int Y, int Z, int W = 3, typename T> static __m128i get(const T& a) {
		auto* i = (const int32*)&a;
		return _mm_set_epi32(i[W], i[Z], i[Y], i[X]);
	}
	template<int I0, int I1, int I2, int I3, int I4, int I5, int I6, int I7, typename T> static __m256i get(const T& a) {
		auto* i = (const int32*)&a;
		return _mm256_set_epi32(i[I7], i[I6], i[I5], i[I4], i[I3], i[I2], i[I1], i[I0]);
	}
#elif defined __ARM_NEON__
	template<int...I, typename T> static auto get(const T& a) {
		auto* i = (const int32*)&a;
		return (vtype_t<int32, pow2_ceil<sizeof...(I)>>&)meta::array<int32, sizeof...(I)>{{i[I]...}};
	}
#endif
	template<
		int I0, int I1, int I2, int I3, int I4, int I5, int I6, int I7,
		int I8, int ... I,
		typename T
	> static auto get(const T& a) {
		static const int N2 = pow2_ceil<sizeof...(I) + 9>;
		vtypeN<N2 * 32> b;
		b.lo = vshuffle<32>(a, meta::left<N2 / 2, meta::value_list<int, I0, I1, I2, I3, I4, I5, I6, I7, I8, I...>>());
		b.hi = vshuffle<32>(a, meta::right<sizeof...(I) + 9 - N2 / 2, meta::value_list<int, I8, I...>>());
		return b;
	}
};

//-----------------------------------------------------------------------------
// 64 bit swizzles
//-----------------------------------------------------------------------------

template<> struct vshuffle_s<64> {
#ifdef __SSE__
	template<int X, int Y = 1> static __m128i get(__m128i a) {
		return _mm_shuffle_epi32(a, SHUFMASK2(X * 2, X * 2 + 1, Y * 2, Y * 2 + 1));
	}
	template<> static __m128i get<0, 1>(__m128i a) {
		return a;
	}
	
	template<int X, int Y, int Z = 2, int W = 3> static __m256i get(__m256i a) {
		return _mm256_permute4x64_epi64(a, SHUFMASK2(X, Y, Z, W));
	}
	template<> static __m256i get<0, 1, 2, 3>(__m256i a) {
		return a;
	}

	template<int X, int Y, typename T> static __m128i get(const T& a) {
		auto* i = (const int64*)&a;
		return _mm_set_epi64x(i[Y], i[X]);
	}
	template<int X, int Y, int Z, int W = 3, typename T> static __m256i get(const T& a) {
		auto* i = (const int64*)&a;
		return _mm256_set_epi64x(i[W], i[Z], i[Y], i[X]);
	}
#elif defined __ARM_NEON__
	template<int...I, typename T> static auto get(const T& a) {
		auto* i = (const int64*)&a;
		return (vtype_t<int64, pow2_ceil<sizeof...(I)>>&)meta::array<int64, sizeof...(I)>{{i[I]...}};
	}
#endif
	template<
		int I0, int I1, int I2, int I3,
		int I4, int ... I,
		typename T
	> static auto get(const T& a) {
		static const int N2 = pow2_ceil<sizeof...(I) + 5>;
		vtypeN<N2 * 64> b;
		b.lo = vshuffle<64>(a, left<N2 / 2, meta::value_list<int, I0, I1, I2, I3, I4, I...>>());
		b.hi = vshuffle<64>(a, right<sizeof...(I) + 5 - N2 / 2, meta::value_list<int, I4, I...>>());
		return b;
	}
};

#ifdef __SSE__
#undef SHUFMASK2
#undef SWIZ_PAIR
#endif

//-----------------------------------------------------------------------------
// vblend_s
//-----------------------------------------------------------------------------

template<typename T, int BPE, int M, bool ALL> struct vblend_s;

template<typename T, int BPE> struct vblend_s<T, BPE, 0, false> {
	static void blend(T &a, T b)	{}
};
template<typename T, int BPE, int M> struct vblend_s<T, BPE, M, true> {
	static void blend(T &a, T b)	{ a = b; }
};

template<int BPE, int M, typename T> void _vblend(T &a, const T &b)	{
	vblend_s<T, BPE, M, (M == (1 << (sizeof(T) * 8 / BPE)) - 1)>::blend(a, b);
}

template<int BPE, int M, int N> void _vblend(vtypeN<N> &a, const vtypeN<N> &b) {
	_vblend<BPE, M & ((1 << (N / (BPE * 2))) - 1)>(a.lo, b.lo);
	_vblend<BPE, (M >> (N / (BPE * 2)))			>(a.hi, b.hi);
}

#ifdef __SSE__

template<typename T, int BPE, int M, bool ALL> struct vblend_s {
	static void blend(T &a, T b) {
		auto	a1 = vcast<__m128i>(a);
		_vblend<BPE, M>(a1, vcast<__m128i>(b));
		a = a1;
	}
};

//8 bit
template<int M> struct vblend_s<__m128i, 8, M, false> {
	static void blend(__m128i &a, __m128i b)	{
		a = _mm_blendv_epi8(b, a, _mm_set_epi8(M<<7, M<<6, M<<5, M<<4, M<<3, M<<2, M<<1, M<<0, M>>1, M>>2, M>>3, M>>4, M>>5, M>>6, M>>7, M>>8));
	}
};
template<int M> struct vblend_s<__m256i, 8, M, false> {
	static void blend(__m256i &a, __m256i b) {
		a = _mm_blendv_epi8(b, a, _mm256_set_epi8(
			M<<7, M<<6, M<<5, M<<4, M<<3, M<<2, M<<1, M<<0, M>>1, M>>2, M>>3, M>>4, M>>5, M>>6, M>>7, M>>8,
			M>>9, M>>10, M>>11, M>>12, M>>13, M>>14, M>>15, M>>16, M>>17, M>>18, M>>19, M>>20, M>>21, M>>22, M>>23, M>>24
		));
	}
};

//16 bit
template<int M> struct vblend_s<__m128i, 16, M, false> {
	static void blend(__m128i &a, __m128i b)	{ a = _mm_blend_epi16(a, b, M); }
};
template<int M> struct vblend_s<__m256i, 16, M, false> {
	static void blend(__m256i &a, __m256i b)	{ a = _mm256_blend_epi16(a, b, M); }	//!! same for top+bottom
};

//32 bit
template<int M> struct vblend_s<__m128i, 32, M, false> {
	static void blend(__m128i &a, __m128i b)	{ a = _mm_blend_epi32(a, b, M); }
};
template<int M> struct vblend_s<__m256i, 32, M, false> {
	static void blend(__m256i &a, __m256i b)	{ a = _mm256_blend_epi32(a, b, M); }
};

//64 bit
template<int M> struct vblend_s<__m128i, 64, M, false> {
	static void blend(__m128i &a, __m128i b)	{ a = _mm_blend_epi32(a, b, ((M&1)|((M&2)<<1)) * 3); }
};
template<int M> struct vblend_s<__m256i, 64, M, false> {
	static void blend(__m256i &a, __m256i b)	{ a = _mm256_blend_epi32(a, b, ((M&1)|((M&2)<<1)|((M&4)<<2)|((M&8)<<3)) * 3); }
};

#elif defined __ARM_NEON__

template<typename T, int BPE, int M, bool ALL> struct vblend_s {
	typedef sint_bits_t<BPE>	I;
	static const int N = sizeof(T) * 8 / BPE;
	template<int...J> static T make_mask(meta::value_list<int, J...>) {
		return (T&)meta::array<I, N>{{(M & (1<<J) ? -1 : 0)...}};
	}
	static void blend(T &a, T b) {
		auto	mask = make_mask(meta::make_value_sequence<N>());
		a = vor(vandc(a, mask), vand(b, mask));
	}
};

#endif //__SSE__

//-----------------------------------------------------------------------------
// vset_s
//-----------------------------------------------------------------------------

template<typename T, int BPE> struct vset_s {
	template<int N, int...I, typename U> static T set(T a, U b) {
		_vblend<BPE, meta::reduce_or<(1 << I)...>>(a, vcast<T>(vshuffle<BPE>(vcast<T>(b), meta::inverse<meta::value_list<int, I...>, meta::make_value_sequence<N>>())));
		return a;
	}
};
/*
template<int N, int BPE> struct vset_s<vtypeN<N>, BPE> {
	template<int N, int...I, typename U> static vtypeN<N> set(vtypeN<N> a, const U &b) {
		scatter_write<I...>((sint_bits_t<BPE>*)&a, (const sint_bits_t<BPE>*)&b);
		return a;
	}
};
*/

//-----------------------------------------------------------------------------
// struct perm<T, I...>: swizzled vector specialisation
//-----------------------------------------------------------------------------

// if MAX * BPE >= 128, and MIN and MAX in same 128, use 128
template<int BPE, int MIN, int MAX, bool USEMIN = ((MAX + 1) * BPE >= min_register_bits && ((MAX * BPE) & -min_register_bits) == ((MIN * BPE) & -min_register_bits))> static constexpr int perm_data_num = min_register_bits / BPE;
// otherwise use smallest block
template<int BPE, int MIN, int MAX> static constexpr int perm_data_num<BPE, MIN, MAX, false> = pow2_ceil<(MAX & ~MIN) + 1>;

template<typename T, int... I> struct perm {
	static const int N = sizeof...(I), BPE = sizeof(T) * 8;
	typedef min_max<I...> stats;

	enum { DN = perm_data_num<BPE, stats::min, stats::max>, data_offset = stats::min / DN, index_offset = data_offset * DN };
	typedef vtype_t<sint_bits_t<BPE>, DN>	D;
	D		data[data_offset + 1];

	const D&	raw()		const	{ return data[data_offset]; }
	D&			raw()				{ return data[data_offset]; }
	vec<T, N>	get()		const	{ return vcast<vtype_t<T, pow2_ceil<N>>>(vshuffle_s<BPE>::get<(I - index_offset)...>(raw())); }
//	vec<T, N>	get()		const	{ return vcast<vtype_t<T, pow2_ceil<N>>>(raw()); }

	operator vec<T, N>()	const	{ return get(); }
	operator D()			const	{ return raw(); }

	perm& operator=(vec<T, N> b)	{
		raw() = vset_s<D, BPE>::set<stats::max - index_offset + 1, (I - index_offset)...>(raw(), b.v);
		return *this;
	}
	perm& operator=(const perm &b)	{
		_vblend<BPE, meta::reduce_or<(1 << (I - index_offset))...>>(raw(), b.raw());
		return *this;
	}
	template<int...J> perm& operator=(const perm<T, J...> &b)	{
		_vblend<BPE, meta::reduce_or<(1 << (I - index_offset))...>>(
			raw(),
			vshuffle<BPE>(b.raw(),
				meta::perm<
					meta::value_list<int, (J - perm<T, J...>::index_offset)...>,
					meta::inverse<meta::value_list<int, (I - index_offset)...>, meta::make_value_sequence<stats::max - index_offset + 1>>
				>()
			)
		);
		return *this;
	}

	friend vec<T,N> as_vec(const perm &a)	{ return a; }
	friend vec<T,N> get(const perm &a)		{ return a; }
};

template<typename T, int I>	struct perm<T, I> : spacer<I * sizeof(T)> {
	T			t;
	T	get()	const	{ return this->t; }

	operator T() const			{ return this->t; }
	T& operator=(T t2)			{ return t = t2; }
	T& operator=(const vec<T,1> &b)	{ return t = b.x; }

	friend T get(const perm &p) { return p; }
	friend T put(perm &&p)		{ return p; }
};

template<int...J, typename T, int... I> SIMD_FUNC auto& swizzle(const perm<T,I...> &p) {
	return reinterpret_cast<const gen<permT1<T>::type, meta::perm<meta::value_list<int, I...>, meta::value_list<int, J...>> > &>(p);
}
template<int...J, typename T, int... I> SIMD_FUNC auto& swizzle(perm<T,I...> &p) {
	return reinterpret_cast<gen<permT1<T>::type, meta::perm<meta::value_list<int, I...>, meta::value_list<int, J...>> > &>(p);
}

//-----------------------------------------------------------------------------
// meta: extract info about a vector
//-----------------------------------------------------------------------------

template<typename T>			constexpr bool is_simd				= false;
template<typename T, int N>		constexpr bool is_simd<vec<T,N>>	= true;
template<typename T, int I0, int I1, int...I>	constexpr bool is_simd<perm<T, I0, I1, I...>>	= true;

template<typename T>			constexpr bool is_vec				= is_simd<T>;

template<typename V, bool can_index>	struct element_type_s1				{ typedef noref_t<decltype(declval<V>()[0])> type; };
template<typename V>					struct element_type_s1<V, false>	{ typedef V	type; };

template<typename V>			struct element_type_s : element_type_s1<V, can_index<V>> {};
template<typename T, int...I>	struct element_type_s<perm<T, I...>>	{ typedef T type; };
template<typename V>			using element_type = typename element_type_s<V>::type;

template<typename T, int N>		constexpr bool is_vecN				= is_vec<T> && num_elements_v<T> == N;

//-----------------------------------------------------------------------------
// supported vector types
//-----------------------------------------------------------------------------

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

typedef vec<hfloat, 2>	hfloat2;
typedef vec<hfloat, 3>	hfloat3;
typedef vec<hfloat, 4>	hfloat4;
typedef vec<hfloat, 8>	hfloat8;
typedef vec<hfloat, 16>	hfloat16;
typedef vec<hfloat, 32>	hfloat32;

typedef vec<bool, 8>	boolx8;
typedef vec<bool, 16>	boolx16;
typedef vec<bool, 32>	boolx32;
typedef vec<bool, 64>	boolx64;

//-----------------------------------------------------------------------------
// conversions
//-----------------------------------------------------------------------------

template<typename A, typename B>	SIMD_FUNC	vec<element_type<A>, num_elements_v<A>+num_elements_v<B>>	simd_lo_hi(A lo, B hi)	{ return {lo, hi}; }

template<typename T, typename F, int N>		SIMD_FUNC auto	as(vec<F, N> p)		{ typedef vec<T,N * sizeof(F) / sizeof(T)> R; return R(vcast<typename R::V>(p.v)); }
template<typename T, typename F, int...I>	SIMD_FUNC auto	as(perm<F, I...> p)	{ return as<T>(as_vec(p)); }
template<typename T, typename F>			SIMD_FUNC auto	as(const F &p)		{ return reinterpret_cast<const vec<T,num_elements_v<F> * sizeof(element_type<F>) / sizeof(T)>&>(p); }
template<typename T, typename F>			SIMD_FUNC auto&	as(F &p)			{ return reinterpret_cast<vec<T,num_elements_v<F> * sizeof(element_type<F>) / sizeof(T)>&>(p); }

template<int...I, typename V> SIMD_FUNC auto& swizzle(meta::value_list<int, I...>, V& v)	{ return swizzle<I...>(v); }

template<typename F, typename T, typename V = void> struct to_s {
	template<typename V> static SIMD_FUNC vec<T, num_elements_v<V>> f(V v) {
		return shrink<num_elements_v<V>>(to<T>(grow<pow2_ceil<num_elements_v<V> + 1>>(v)));
	}
};

struct to_s_same {
	template<typename V> static auto f(V v) { return v; }
};
template<typename F, typename T> struct to_s_via {
	template<typename V> static SIMD_FUNC auto f(V v) { return to<T>(to<F>(v)); }
};
template<int M, typename F, typename T> struct to_s_every {
	template<typename V> static SIMD_FUNC auto f(const V &v) { return swizzle(meta::muladd<M, 0, meta::make_value_sequence<num_elements_v<V>>>(), as<T>(v)); }
};

template<typename T, typename F> SIMD_FUNC enable_if_t<is_vec<F>, vec<T, num_elements_v<F>>>		to(F f)			{ return to<T, element_type<F>, num_elements_v<F>>(f); }
template<typename T, typename F> SIMD_FUNC enable_if_t<num_elements_v<F> == 1, T>					to(F f)			{ return (T)f; }
template<typename T, typename F> SIMD_FUNC enable_if_t<(!is_vec<F> && num_elements_v<F> > 1), vec<T, num_elements_v<F>>> to(F f)		{ return to<T>(as_vec(f)); }

template<typename T> struct to_s<T, T> : to_s_same {};

// conversion to float goes via int/uint
template<> struct to_s<int8,	float>	: to_s_via<int,		float> {};
template<> struct to_s<int16,	float>	: to_s_via<int,		float> {};
template<> struct to_s<uint8,	float>	: to_s_via<uint32,	float> {};
template<> struct to_s<uint16,	float>	: to_s_via<uint32,	float> {};

// conversion from float goes via int/uint
template<> struct to_s<float,	int8>	: to_s_via<int,		int8> {};
template<> struct to_s<float,	int16>	: to_s_via<int,		int16> {};
template<> struct to_s<float,	uint8>	: to_s_via<uint32,	uint8> {};
template<> struct to_s<float,	uint16>	: to_s_via<uint32,	uint16> {};

// conversion from double goes via int/uint
template<> struct to_s<double,	int8>	: to_s_via<int,		int8> {};
template<> struct to_s<double,	int16>	: to_s_via<int,		int16> {};
template<> struct to_s<double,	uint8>	: to_s_via<uint32,	uint8> {};
template<> struct to_s<double,	uint16>	: to_s_via<uint32,	uint16> {};

// conversion from bigger to smaller - just low parts
template<> struct to_s<int16,	int8>	: to_s_every<2, int16,	int8> {};
template<> struct to_s<int,		int16>	: to_s_every<2, int,	int16> {};
template<> struct to_s<int64,	int>	: to_s_every<2, long,	int> {};
template<> struct to_s<int,		int8>	: to_s_every<4, int,	int8> {};
template<> struct to_s<int64,	int16>	: to_s_every<4, int64,	int16> {};
template<> struct to_s<int64,	int8>	: to_s_every<8, int64,	int8> {};


// conversion to unsigned is same as signed
template<typename F, typename T> struct to_s<F, T, enable_if_t<is_signed<F> && !is_signed<T>>> {
	template<typename V> static SIMD_FUNC auto f(V v) { return as<T>(to<signed_t<T>>(v)); }
};
// conversion from unsigned is same as signed
template<typename F, typename T> struct to_s<F, T, enable_if_t<!is_signed<F> && is_signed<T>>> {
	template<typename V> static SIMD_FUNC auto f(V v) { return to<T>(as<signed_t<F>>(v)); }
};
template<typename F, typename T> struct to_s<F, T, enable_if_t<!is_signed<F> && !is_signed<T> && sizeof(F) != sizeof(T)>> {
	template<typename V> static SIMD_FUNC auto f(V v) { return as<T>(to<signed_t<T>>(as<signed_t<F>>(v))); }
};

// special cases
#ifdef __SSE__

template<typename T, typename F, int N> SIMD_FUNC oversized_t<biggest_t<T, F>, N, vec<T, N>>		to(vec<F, N> v)	{ return concat(to<T>(v.lo), to<T>(v.hi)); }
template<typename T, typename F, int N> SIMD_FUNC not_oversized_t<biggest_t<T, F>, N, vec<T, N>>	to(vec<F, N> v) { return to_s<F, T>::f(v); }

// conversion to double goes via int/uint
template<> struct to_s<int8,	double>	: to_s_via<int,		double> {};
template<> struct to_s<int16,	double>	: to_s_via<int,		double> {};
template<> struct to_s<uint8,	double>	: to_s_via<uint32,	double> {};
template<> struct to_s<uint16,	double>	: to_s_via<uint32,	double> {};

// conversion to/from hfloat goes via int
template<typename T> struct to_s<hfloat, T, enable_if_t<!same_v<T,float>>>	: to_s_via<float, T> {};
template<typename F> struct to_s<F, hfloat, enable_if_t<!same_v<F,float>>>	: to_s_via<float, hfloat> {};
template<> struct to_s<hfloat, hfloat>		: to_s_same {};

template<> SIMD_FUNC int16x4	to<int16>(int8x4 x)		{ return _mm_cvtepi8_epi16(x.v); }
template<> SIMD_FUNC int16x4	to<int16>(uint8x4 x)	{ return _mm_cvtepu8_epi16(x.v); }
template<> SIMD_FUNC int16x8	to<int16>(int8x8 x)		{ return _mm_cvtepi8_epi16(x.v); }
template<> SIMD_FUNC int16x8	to<int16>(uint8x8 x)	{ return _mm_cvtepu8_epi16(x.v); }

template<> SIMD_FUNC int32x2	to<int32>(double2 x)	{ return _mm_cvttpd_epi32(x.v); }

template<> SIMD_FUNC int32x4	to<int32>(float4 x)		{ return _mm_cvttps_epi32(x.v); }
template<> SIMD_FUNC int32x4	to<int32>(int8x4 x)		{ return _mm_cvtepi8_epi32(x.v); }
template<> SIMD_FUNC int32x4	to<int32>(uint8x4 x)	{ return _mm_cvtepu8_epi32(x.v); }
template<> SIMD_FUNC int32x4	to<int32>(int16x4 x)	{ return _mm_cvtepi16_epi32(x.v); }
template<> SIMD_FUNC int32x4	to<int32>(uint16x4 x)	{ return _mm_cvtepu16_epi32(x.v); }

template<> SIMD_FUNC float2		to<float>(double2 x)	{ return _mm_cvtpd_ps(x.v); }
template<> SIMD_FUNC float4		to<float>(hfloat4 x)	{ return _mm_cvtph_ps(x.v); }
template<> SIMD_FUNC float4		to<float>(int32x4 x)	{ return _mm_cvtepi32_ps(x.v); }

template<> SIMD_FUNC double2	to<double>(float2 x)	{ return _mm_cvtps_pd(x.v); }

template<> SIMD_FUNC hfloat2	to<hfloat>(float2 x)	{ return _mm_cvtps_ph(x.v, _MM_FROUND_TO_NEAREST_INT); }
template<> SIMD_FUNC hfloat4	to<hfloat>(float4 x)	{ return _mm_cvtps_ph(x.v, _MM_FROUND_TO_NEAREST_INT); }
#endif

#ifdef __AVX2__
template<> SIMD_FUNC int16x16	to<int16>(int8x16 x)	{ return _mm256_cvtepi8_epi16(x.v); }
template<> SIMD_FUNC int16x16	to<int16>(uint8x16 x)	{ return _mm256_cvtepu8_epi16(x.v); }

template<> SIMD_FUNC int32x4	to<int32>(double4 x)	{ return _mm256_cvttpd_epi32(x.v); }

template<> SIMD_FUNC int32x8	to<int32>(float8 x)		{ return _mm256_cvttps_epi32(x.v); }
template<> SIMD_FUNC int32x8	to<int32>(int8x8 x)		{ return _mm256_cvtepi8_epi32(x.v); }
template<> SIMD_FUNC int32x8	to<int32>(uint8x8 x)	{ return _mm256_cvtepu8_epi32(x.v); }
template<> SIMD_FUNC int32x8	to<int32>(int16x8 x)	{ return _mm256_cvtepi16_epi32(x.v); }
template<> SIMD_FUNC int32x8	to<int32>(uint16x8 x)	{ return _mm256_cvtepu16_epi32(x.v); }

template<> SIMD_FUNC int64x4	to<int64>(int8x4 x)		{ return _mm256_cvtepi8_epi64(x.v); }
template<> SIMD_FUNC int64x4	to<int64>(uint8x4 x)	{ return _mm256_cvtepu8_epi64(x.v); }
template<> SIMD_FUNC int64x4	to<int64>(int32x4 x)	{ return _mm256_cvtepi32_epi64(x.v); }
template<> SIMD_FUNC int64x4	to<int64>(uint32x4 x)	{ return _mm256_cvtepu32_epi64(x.v); }
template<> SIMD_FUNC int64x4	to<int64>(int16x4 x)	{ return _mm256_cvtepi16_epi64(x.v); }
template<> SIMD_FUNC int64x4	to<int64>(uint16x4 x)	{ return _mm256_cvtepu16_epi64(x.v); }

template<> SIMD_FUNC float4		to<float>(double4 x)	{ return _mm256_cvtpd_ps(x.v); }
template<> SIMD_FUNC float8		to<float>(int32x8 x)	{ return _mm256_cvtepi32_ps(x.v); }
template<> SIMD_FUNC float8		to<float>(hfloat8 x)	{ return _mm256_cvtph_ps(x.v); }

template<> SIMD_FUNC double4	to<double>(float4 x)	{ return _mm256_cvtps_pd(x.v); }
template<> SIMD_FUNC double4	to<double>(int32x4 x)	{ return _mm256_cvtepi32_pd(x.v); }

template<> SIMD_FUNC hfloat8	to<hfloat>(float8 x)	{ return _mm256_cvtps_ph(x.v, _MM_FROUND_TO_NEAREST_INT); }
#endif

#ifdef __AVX512F__
template<> SIMD_FUNC int8x4		to<int8>(int16x4 x)		{ return _mm_cvtepi16_epi8(x.v); }
template<> SIMD_FUNC int8x4		to<int8>(int32x4 x)		{ return _mm_cvtepi32_epi8(x.v); }
template<> SIMD_FUNC int8x4		to<int8>(int64x4 x)		{ return _mm256_cvtepi64_epi8(x.v); }
template<> SIMD_FUNC int8x8		to<int8>(int16x8 x)		{ return _mm_cvtepi16_epi8(x.v); }
template<> SIMD_FUNC int8x8		to<int8>(int32x8 x)		{ return _mm256_cvtepi32_epi8(x.v); }
template<> SIMD_FUNC int8x16	to<int8>(int16x16 x)	{ return _mm256_cvtepi16_epi8(x.v); }

template<> SIMD_FUNC int16x4	to<int16>(int32x4 x)	{ return _mm_cvtepi32_epi16(x.v); }
template<> SIMD_FUNC int16x4	to<int16>(int64x4 x)	{ return _mm256_cvtepi64_epi16(x.v); }
template<> SIMD_FUNC int16x8	to<int16>(int32x8 x)	{ return _mm256_cvtepi32_epi16(x.v); }

template<> SIMD_FUNC uint32x4	to<uint32>(float4 x)	{ return _mm_cvtps_epu32(x.v); }
template<> SIMD_FUNC uint32x4	to<uint32>(double4 x)	{ return _mm256_cvtpd_epu32(x.v); }
template<> SIMD_FUNC uint32x8	to<uint32>(float8 x)	{ return _mm256_cvtps_epu32(x.v); }

template<> SIMD_FUNC int64x4	to<int64>(float4 x)		{ return _mm256_cvtps_epi64(x.v); }
template<> SIMD_FUNC int64x4	to<int64>(double4 x)	{ return _mm256_cvtpd_epi64(x.v); }

template<> SIMD_FUNC uint64x4	to<uint64>(float4 x)	{ return _mm256_cvtps_epu64(x.v); }
template<> SIMD_FUNC uint64x4	to<uint64>(double4 x)	{ return _mm256_cvtpd_epu64(x.v); }
#elif defined __SSE__
template<> SIMD_FUNC int64x4 to<int64>(float4 x) {
	auto	s = _mm_set1_ps(1ull << 32);
	float4	hi = _mm_round_ps(_mm_div_ps(x.v, s), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
	float4	lo = _mm_sub_ps(x, _mm_mul_ps(hi, s));
	return interleave32(_mm_cvtps_epi32(lo.v), _mm_cvtps_epi32(hi.v));
}
template<> SIMD_FUNC int64x4 to<int64>(double4 x) {
	auto	s = _mm256_set1_pd(1ull << 32);
	double4 hi = _mm256_round_pd(_mm256_div_pd(x.v, s), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
	double4 lo = _mm256_sub_pd(x, _mm256_mul_pd(hi, s));
	return interleave32(_mm256_cvtpd_epi32(lo.v), _mm256_cvtpd_epi32(hi.v));
}
template<> SIMD_FUNC uint64x4 to<uint64>(float4 x) {
	auto	s = _mm_set1_ps(1ull << 32);
	float4	hi = _mm_round_ps(_mm_div_ps(x.v, s), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
	float4	lo = _mm_sub_ps(x, _mm_mul_ps(hi, s));
	return interleave32(_mm_cvtps_epi32(lo.v), _mm_cvtps_epi32(hi.v));
}
template<> SIMD_FUNC uint64x4 to<uint64>(double4 x) {
	auto	s = _mm256_set1_pd(1ull << 32);
	double4 hi = _mm256_round_pd(_mm256_div_pd(x.v, s), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
	double4 lo = _mm256_sub_pd(x, _mm256_mul_pd(hi, s));
	return interleave32(_mm256_cvtpd_epi32(lo.v), _mm256_cvtpd_epi32(hi.v));
}
#endif

#ifdef __AVX512F__
template<> SIMD_FUNC int8x32	to<int8>(int16x32 x)	{ return _mm512_cvtepi16_epi8(x.v); }
template<> SIMD_FUNC int8x16	to<int8>(int32x16 x)	{ return _mm512_cvtepi32_epi8(x.v); }
template<> SIMD_FUNC int8x8		to<int8>(int64x8 x)		{ return _mm512_castsi512_si128(_mm512_shuffle_epi8(x.v, _mm512_set1_epi64(0x14100800))); }

template<> SIMD_FUNC int16x16	to<int16>(int32x16 x)	{ return _mm512_cvtepi32_epi16(x.v); }
template<> SIMD_FUNC int16x32	to<int16>(int8x32 x)	{ return _mm512_cvtepi8_epi16(x.v); }

template<> SIMD_FUNC uint16x16	to<uint16>(int32x16 x)	{ return _mm512_cvtepi32_epi16(x.v); }

template<> SIMD_FUNC int32x8	to<int32>(double8 x)	{ return _mm512_cvtpd_epi32(x.v); }
template<> SIMD_FUNC int32x8	to<int32>(int64x8 x)	{ return _mm512_cvtepi64_epi32(x.v); }

template<> SIMD_FUNC int32x16	to<int32>(floatx16 x)	{ return _mm512_cvtps_epi32(x.v); }
template<> SIMD_FUNC int32x16	to<int32>(int8x16 x)	{ return _mm512_cvtepi8_epi32(x.v); }
template<> SIMD_FUNC int32x16	to<int32>(uint8x16 x)	{ return _mm512_cvtepu8_epi32(x.v); }
template<> SIMD_FUNC int32x16	to<int32>(int16x16 x)	{ return _mm512_cvtepi16_epi32(x.v); }
template<> SIMD_FUNC int32x16	to<int32>(uint16x16 x)	{ return _mm512_cvtepu16_epi32(x.v); }

template<> SIMD_FUNC uint32x8	to<uint32>(double8 x)	{ return _mm512_cvtpd_epu32(x.v); }
template<> SIMD_FUNC uint32x16	to<uint32>(floatx16 x)	{ return _mm512_cvtps_epu32(x.v); }

template<> SIMD_FUNC int64x8	to<int64>(float8 x)		{ return _mm512_cvtps_epi64(x.v); }
template<> SIMD_FUNC int64x8	to<int64>(double8 x)	{ return _mm512_cvtpd_epi64(x.v); }
template<> SIMD_FUNC int64x8	to<int64>(int8x8 x)		{ return _mm512_cvtepi8_epi64(x.v); }
template<> SIMD_FUNC int64x8	to<int64>(uint8x8 x)	{ return _mm512_cvtepu8_epi64(x.v); }
template<> SIMD_FUNC int64x8	to<int64>(int32x8 x)	{ return _mm512_cvtepi32_epi64(x.v); }
template<> SIMD_FUNC int64x8	to<int64>(uint32x8 x)	{ return _mm512_cvtepu32_epi64(x.v); }
template<> SIMD_FUNC int64x8	to<int64>(int16x8 x)	{ return _mm512_cvtepi16_epi64(x.v); }
template<> SIMD_FUNC int64x8	to<int64>(uint16x8 x)	{ return _mm512_cvtepu16_epi64(x.v); }

template<> SIMD_FUNC uint64x8	to<uint64>(float8 x)	{ return _mm512_cvtps_epu64(x.v); }
template<> SIMD_FUNC uint64x8	to<uint64>(double8 x)	{ return _mm512_cvtpd_epu64(x.v); }

template<> SIMD_FUNC float8		to<float>(double8 x)	{ return _mm512_cvtpd_ps(x.v); }
template<> SIMD_FUNC floatx16	to<float>(hfloat16 x)	{ return _mm512_cvtph_ps(x.v); }
template<> SIMD_FUNC floatx16	to<float>(int32x16 x)	{ return _mm512_cvtepi32_ps(x.v); }
template<> SIMD_FUNC double8	to<double>(int32x8 x)	{ return _mm512_cvtepi32_pd(x.v); }
template<> SIMD_FUNC double8	to<double>(float8 x)	{ return _mm512_cvtps_pd(x.v); }

template<> SIMD_FUNC hfloat16	to<hfloat>(floatx16 x)	{ return _mm512_cvtps_ph(x.v, _MM_FROUND_TO_NEAREST_INT); }
#endif

#ifdef __ARM_NEON__

template<typename T, typename F, int N> SIMD_FUNC oversized_t<smallest_t<T, F>, N, vec<T, N>>		to(vec<F, N> v)	{ return concat(to<T>(v.lo), to<T>(v.hi)); }
template<typename T, typename F, int N> SIMD_FUNC not_oversized_t<smallest_t<T, F>, N, vec<T, N>>	to(vec<F, N> v) { return to_s<F, T>::f(v); }

// conversion to double goes via float
template<> struct to_s<int8,	double>	: to_s_via<float,	double> {};
template<> struct to_s<int16,	double>	: to_s_via<float,	double> {};
template<> struct to_s<uint8,	double>	: to_s_via<float,	double> {};
template<> struct to_s<uint16,	double>	: to_s_via<float,	double> {};

template<> struct to_s<int32,	double>	: to_s_via<int64,	double> {};
template<> struct to_s<uint32,	double>	: to_s_via<uint64,	double> {};

// conversion from 32 bit to 8 bit goes via int16
template<> struct to_s<int8, int32>		: to_s_via<int16, int32> {};
template<> struct to_s<int8, uint32>	: to_s_via<int16, uint32> {};
template<> struct to_s<uint8, int32>	: to_s_via<uint16, int32> {};
template<> struct to_s<uint8, uint32>	: to_s_via<uint16, uint32> {};

template<> SIMD_FUNC int16x4	to<int16>(int8x4 x)		{ return vcast<__n64>(vshll_n_s8(vcast<__n64>(x.v), 0)); }
template<> SIMD_FUNC int16x4	to<int16>(uint8x4 x)	{ return vcast<__n64>(vshll_n_u8(vcast<__n64>(x.v), 0)); }
template<> SIMD_FUNC int16x8	to<int16>(int8x8 x)		{ return vshll_n_s8(x.v, 0); }
template<> SIMD_FUNC int16x8	to<int16>(uint8x8 x)	{ return vshll_n_u8(x.v, 0); }
template<> SIMD_FUNC int16x16	to<int16>(int8x16 x)	{ return vtype_t<int16,16>{vshll_n_s8(x.v.s.low64, 0), vshll_high_n_s8(x.v, 0)}; }
template<> SIMD_FUNC int16x16	to<int16>(uint8x16 x)	{ return vtype_t<int16,16>{vshll_n_u8(x.v.s.low64, 0), vshll_high_n_u8(x.v, 0)}; }

template<> SIMD_FUNC int32x4	to<int32>(int16x4 x)	{ return vshll_n_s16(x, 0); }
template<> SIMD_FUNC int32x4	to<int32>(uint16x4 x)	{ return vshll_n_u16(x, 0); }
template<> SIMD_FUNC int32x8	to<int32>(int16x8 x)	{ return vtype_t<int32,8>{vshll_n_s16(x.v.s.low64, 0), vshll_high_n_s16(x.v, 0)}; }
template<> SIMD_FUNC int32x8	to<int32>(uint16x8 x)	{ return vtype_t<int32,8>{vshll_n_u16(x.v.s.low64, 0), vshll_high_n_u16(x.v, 0)}; }
template<> SIMD_FUNC int32x2	to<int32>(float2 x)		{ return vcvt_s32_f32(x.v); }
template<> SIMD_FUNC int32x4	to<int32>(float4 x)		{ return vcvtq_s32_f32(x.v); }

template<> SIMD_FUNC uint32x2	to<uint32>(float2 x)	{ return vcvt_u32_f32(x.v); }
template<> SIMD_FUNC uint32x4	to<uint32>(float4 x)	{ return vcvtq_u32_f32(x.v); }

template<> SIMD_FUNC int64x2	to<int64>(int32x2 x)	{ return vshll_n_s32(x, 0); }
template<> SIMD_FUNC int64x2	to<int64>(uint32x2 x)	{ return vshll_n_u32(x, 0); }
template<> SIMD_FUNC int64x4	to<int64>(int32x4 x)	{ return vtype_t<int64,4>{vshll_n_s32(x.v.s.low64, 0), vshll_high_n_s32(x.v, 0)}; }
template<> SIMD_FUNC int64x4	to<int64>(uint32x4 x)	{ return vtype_t<int64,4>{vshll_n_u32(x.v.s.low64, 0), vshll_high_n_u32(x.v, 0)}; }
template<> SIMD_FUNC int64x2	to<int64>(double2 x)	{ return vcvtq_s64_f64(x.v); }

template<> SIMD_FUNC uint64x2	to<uint64>(double2 x)	{ return vcvtq_u64_f64(x.v); }

template<> SIMD_FUNC float2		to<float>(int32x2 x)	{ return vcvt_f32_s32(x.v); }
template<> SIMD_FUNC float2		to<float>(uint32x2 x)	{ return vcvt_f32_u32(x.v); }
template<> SIMD_FUNC float2		to<float>(double2 x)	{ return vcvt_f32_f64(x.v); }
template<> SIMD_FUNC float4		to<float>(hfloat4 x)	{ return vcvt_f32_f16(x.v); }
template<> SIMD_FUNC float4		to<float>(int32x4 x)	{ return vcvtq_f32_s32(x.v); }
template<> SIMD_FUNC float4		to<float>(uint32x4 x)	{ return vcvtq_f32_u32(x.v); }

template<> SIMD_FUNC double2	to<double>(int64x2 x)	{ return vcvtq_f64_s64(x.v); }
template<> SIMD_FUNC double2	to<double>(uint64x2 x)	{ return vcvtq_f64_u64(x.v); }
template<> SIMD_FUNC double2	to<double>(float2 x)	{ return vcvt_f64_f32(x.v); }

//template<> SIMD_FUNC int16x4	to<int16>(hfloat4 x)	{ return vcvt_s16_f16(x.v); }
//template<> SIMD_FUNC int16x8	to<int16>(hfloat8 x)	{ return vcvtq_s16_f16(x.v); }

//template<> SIMD_FUNC uint16x4	to<uint16>(hfloat4 x)	{ return vcvt_u16_f16(x.v); }
//template<> SIMD_FUNC uint16x8	to<uint16>(hfloat8 x)	{ return vcvtq_u16_f16(x.v); }

#endif

//-----------------------------------------------------------------------------
// make vector
//-----------------------------------------------------------------------------

template<typename T, typename P, bool B = (num_elements_v<P> > 1)> struct vec_maker1 {
	T	t;
	vec_maker1(P p) : t(p) {}
};
template<typename T, typename A, typename B> struct vec_maker1<T, pair<A, B>, true> {
	vec_maker1<T, A>	a;
	vec_maker1<T, B>	b;
	vec_maker1(pair<A, B> p) : a(p.a), b(p.b) {}
};
template<typename T, typename P> struct vec_maker1<T, vec<P, 1>, false> {
	T t;
	vec_maker1(const vec<P, 1> &p) : t(p.x) {}
};
template<typename T, typename P> struct vec_maker1<T, P, true> {
	T	t[num_elements_v<P>];
	vec_maker1(P p) : vec_maker1(force_cast<vec_maker1>(to<T>(p))) {}
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
	return force_cast<vec<T, sizeof(vec_maker<T, P0, P1, P...>) / sizeof(T)>>(vec_maker<T, P0, P1, P...>(p0, p1, p...));
}

template<typename T, int...I, typename P0, typename P1, typename...P>	SIMD_FUNC auto swizzle(const P0 &p0, const P1 &p1, const P&...p) {
	return swizzle<I...>(to<T>(p0, p1, p...));
}
template<int I0, int...I, typename P0, typename P1, typename...P>		SIMD_FUNC auto swizzle(const P0 &p0, const P1 &p1, const P&...p) {
	return swizzle<element_type<P0>, I0, I...>(p0, p1, p...);
}

//-----------------------------------------------------------------------------
// generic operators
//-----------------------------------------------------------------------------

template<int I> struct irecip_refiner {
	template<typename T> static T f(T a, T r) { return irecip_refiner<I / 2>::f(a, (r * (2 - a * r))); }
};
template<> struct irecip_refiner<1> {
	template<typename T> static T f(T a, T r) { return r * (2 - a * r); }
};

template<typename T, int N>		vec<T,N>	irecip(vec<T, N> a)	{ return irecip_refiner<sizeof(T) * 2>::f(a, ((a * a) + a - 1)); }
template<typename T> constexpr	T			irecip(T x)			{ return (~T(0) / x) + (x & (x - 1) ? 1 : 2); }

template<typename T, int N> SIMD_FUNC vec<T, N>	mulhi(vec<T, N> a, vec<T, N> b)	{ return as<T>(fullmul(a, b)).odd; }


template<typename T, int N> SIMD_FUNC enable_if_t<!is_signed<T>, vec<T, N>>	idiv(vec<T,N> a, T b)			{ return mulhi(a, vec<T,N>(irecip(b))); }
template<typename T, int N> SIMD_FUNC enable_if_t<!is_signed<T>, vec<T, N>>	imod(vec<T,N> a, T b)			{ return mulhi(a * irecip(b), vec<T,N>(b)); }
template<typename T, int N> SIMD_FUNC enable_if_t<!is_signed<T>, vec<T, N>>	idiv(vec<T,N> a, vec<T,N> b)	{ return mulhi(a,  irecip(b)); }
template<typename T, int N> SIMD_FUNC enable_if_t<!is_signed<T>, vec<T, N>>	imod(vec<T,N> a, vec<T,N> b)	{ return mulhi(a * irecip(b), b); }

template<typename T, int N> SIMD_FUNC enable_if_t<is_signed<T>, vec<T, N>>	idiv(vec<T,N> a, T b)			{ typedef unsigned_t<T> U; return as<T>(idiv(as<U>(a), U(b))); }
template<typename T, int N> SIMD_FUNC enable_if_t<is_signed<T>, vec<T, N>>	imod(vec<T,N> a, T b)			{ typedef unsigned_t<T> U; return as<T>(imod(as<U>(a), U(b))); }
template<typename T, int N> SIMD_FUNC enable_if_t<is_signed<T>, vec<T, N>>	idiv(vec<T,N> a, vec<T,N> b)	{ return mulhi(a,  irecip(b)); }
template<typename T, int N> SIMD_FUNC enable_if_t<is_signed<T>, vec<T, N>>	imod(vec<T,N> a, vec<T,N> b)	{ return mulhi(a * irecip(b), b); }

template<typename T, int N>	SIMD_FUNC auto		operator!(vec<T, N> a)		{ return a == T(0); }

template<typename A, typename T, int N>	SIMD_FUNC enable_if_t<is_num<A>, vec<T, N>>	operator+ (A a, vec<T, N> b)	{ return b + a; }
template<typename A, typename T, int N>	SIMD_FUNC enable_if_t<is_num<A>, vec<T, N>>	operator- (A a, vec<T, N> b)	{ return vec<T, N>(a) - b; }//-b + a; }
template<typename A, typename T, int N>	SIMD_FUNC enable_if_t<is_num<A>, vec<T, N>>	operator* (A a, vec<T, N> b)	{ return b * a; }
template<typename A, typename T, int N>	SIMD_FUNC enable_if_t<is_num<A>, vec<T, N>>	operator/ (A a, vec<T, N> b)	{ return vec<T, N>(a) / b; }
template<typename A, typename T, int N>	SIMD_FUNC enable_if_t<is_num<A>, vec<T, N>>	operator% (A a, vec<T, N> b)	{ return vec<T, N>(a) % b; }

// assign ops
template<typename T, int N, typename B>	SIMD_FUNC vec<T, N>&	operator+=(vec<T, N> &a, B b)				{ return a = a + b; }
template<typename T, int N, typename B>	SIMD_FUNC vec<T, N>&	operator-=(vec<T, N> &a, B b)				{ return a = a - b; }
template<typename T, int N, typename B>	SIMD_FUNC vec<T, N>&	operator*=(vec<T, N> &a, B b)				{ return a = a * b; }
template<typename T, int N, typename B>	SIMD_FUNC vec<T, N>&	operator/=(vec<T, N> &a, B b)				{ return a = a / b; }

template<typename T, int N, typename B>	SIMD_FUNC enable_if_t<is_int<T>, vec<T, N>&>	operator&= (vec<T, N> &a, B b)		{ return a = a & b; }
template<typename T, int N, typename B>	SIMD_FUNC enable_if_t<is_int<T>, vec<T, N>&>	operator|= (vec<T, N> &a, B b)		{ return a = a | b; }
template<typename T, int N, typename B>	SIMD_FUNC enable_if_t<is_int<T>, vec<T, N>&>	operator^= (vec<T, N> &a, B b)		{ return a = a ^ b; }
template<typename T, int N, typename B>	SIMD_FUNC enable_if_t<is_int<T>, vec<T, N>&>	operator%= (vec<T, N> &a, B b)		{ return a = a % b; }
template<typename T, int N>				SIMD_FUNC enable_if_t<is_int<T>, vec<T, N>&>	operator<<=(vec<T, N> &a, int b)	{ return a = a << b; }
template<typename T, int N>				SIMD_FUNC enable_if_t<is_int<T>, vec<T, N>&>	operator>>=(vec<T, N> &a, int b)	{ return a = a >> b; }

template<typename T, int N>				SIMD_FUNC vec<T, N>&	operator++(vec<T, N> &a)					{ return a += T(1); }
template<typename T, int N>				SIMD_FUNC vec<T, N>&	operator--(vec<T, N> &a)					{ return a -= T(1); }
template<typename T, int N>				SIMD_FUNC vec<T, N>		operator++(vec<T, N> &a, int)				{ auto t = a; ++a; return t; }
template<typename T, int N>				SIMD_FUNC vec<T, N>		operator--(vec<T, N> &a, int)				{ auto t = a; --a; return t; }

// perm ops
template<typename T, int... I> SIMD_FUNC auto	operator-(const perm<T, I...> &a)	{ return -a.get(); }
template<typename T, int... I> SIMD_FUNC auto	operator~(const perm<T, I...> &a)	{ return ~a.get(); }
template<typename T, int... I> SIMD_FUNC auto	operator!(const perm<T, I...> &a)	{ return !a.get(); }

template<typename T, int... I>	SIMD_FUNC auto	operator<<(perm<T, I...> a, int b)	{ return a.get() << b; }
template<typename T, int... I>	SIMD_FUNC auto	operator>>(perm<T, I...> a, int b)	{ return a.get() >> b; }

template<typename T>			using rperm_op_t	= enable_if_t<is_num<T>>;
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator& (A a, perm<T, I...> b)	{ return a &  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator| (A a, perm<T, I...> b)	{ return a |  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator^ (A a, perm<T, I...> b)	{ return a ^  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator+ (A a, perm<T, I...> b)	{ return a +  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator- (A a, perm<T, I...> b)	{ return a -  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator* (A a, perm<T, I...> b)	{ return a *  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator/ (A a, perm<T, I...> b)	{ return a /  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator==(A a, perm<T, I...> b)	{ return a == b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator< (A a, perm<T, I...> b)	{ return a <  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator> (A a, perm<T, I...> b)	{ return a >  b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator!=(A a, perm<T, I...> b)	{ return a != b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator<=(A a, perm<T, I...> b)	{ return a <= b.get(); }
template<typename A, typename T, int... I, typename=rperm_op_t<A>>	SIMD_FUNC auto	operator>=(A a, perm<T, I...> b)	{ return a >= b.get(); }

template<typename R, typename B, typename V = void> struct perm_result;
template<typename R, typename B>			struct perm_result<R, B, enable_if_t<is_num<B>>>	{ typedef R type; };
template<typename R, typename T, int...I>	struct perm_result<R, perm<T, I...>>				{ typedef R type; };
template<typename R, typename T, int N>		struct perm_result<R, vec<T, N>>					{ typedef R type; };
template<typename B>			using perm_op_t	= typename perm_result<void, B>::type;
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator& (perm<T, I...> a, B b)	{ return a.get() &  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator| (perm<T, I...> a, B b)	{ return a.get() |  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator^ (perm<T, I...> a, B b)	{ return a.get() ^  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator+ (perm<T, I...> a, B b)	{ return a.get() +  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator- (perm<T, I...> a, B b)	{ return a.get() -  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator* (perm<T, I...> a, B b)	{ return a.get() *  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator/ (perm<T, I...> a, B b)	{ return a.get() /  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator==(perm<T, I...> a, B b)	{ return a.get() == b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator< (perm<T, I...> a, B b)	{ return a.get() <  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator> (perm<T, I...> a, B b)	{ return a.get() >  b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator!=(perm<T, I...> a, B b)	{ return a.get() != b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator<=(perm<T, I...> a, B b)	{ return a.get() <= b; }
template<typename T, int... I, typename B, typename=perm_op_t<B>>	SIMD_FUNC auto	operator>=(perm<T, I...> a, B b)	{ return a.get() >= b; }

// assign ops
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator&=(perm<T,I...> &a, B b)	{ return a = a.get() & b; }
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator|=(perm<T,I...> &a, B b)	{ return a = a.get() | b; }
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator^=(perm<T,I...> &a, B b)	{ return a = a.get() ^ b; }
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator+=(perm<T,I...> &a, B b)	{ return a = a.get() + b; }
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator-=(perm<T,I...> &a, B b)	{ return a = a.get() - b; }
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator*=(perm<T,I...> &a, B b)	{ return a = a.get() * b; }
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator/=(perm<T,I...> &a, B b)	{ return a = a.get() / b; }
template<typename T, int... I, typename B>	SIMD_FUNC auto&	operator%=(perm<T,I...> &a, B b)	{ return a = a.get() % b; }

template<typename T, int... I>	SIMD_FUNC auto&	operator<<=(perm<T,I...> &a, int b)				{ return a = a.get() << b; }
template<typename T, int... I>	SIMD_FUNC auto&	operator>>=(perm<T,I...> &a, int b)				{ return a = a.get() >> b; }
template<typename T, int... I>	SIMD_FUNC auto&	operator++ (perm<T,I...> &a)					{ return a += T(1); }
template<typename T, int... I>	SIMD_FUNC auto&	operator-- (perm<T,I...> &a)					{ return a -= T(1); }
template<typename T, int... I>	SIMD_FUNC auto	operator++ (perm<T,I...> &a, int)				{ auto t = a; ++a; return t; }
template<typename T, int... I>	SIMD_FUNC auto	operator-- (perm<T,I...> &a, int)				{ auto t = a; --a; return t; }

//-----------------------------------------------------------------------------
// vectors of chars
//-----------------------------------------------------------------------------

#ifdef __SSE__
template<int N> SIMD_FUNC vec<int8, N>	shl(vec<int8, N> a, int b)	{ return as<int8>(shl(as<uint16>(a), b)) & int8(-(1 << b)); }
template<int N> SIMD_FUNC vec<int8, N>	shr(vec<int8, N> a, int b)	{ return as<int8>(shr(as<uint8>(a) ^ 0x80, b) - (0x80 >> b)); }

template<int N> SIMD_FUNC vec<uint8, N>	shl(vec<uint8, N> a, int b)	{ return as<uint8>(as<uint16>(a) << b) & uint8(-(1 << b)); }
template<int N> SIMD_FUNC vec<uint8, N>	shr(vec<uint8, N> a, int b)	{ return as<uint8>(as<uint16>(a) >> b) & uint8(0xff >> b); }

SIMD_FUNC auto mul8x8(__m128i a, __m128i b) {
	__m128i mask = _mm_set1_epi16(0xff);
	return vor(vand(_mm_mullo_epi16(vand(a, mask), vand(b, mask)), mask), _mm_slli_epi16(_mm_mulhi_epi16(vandc(a, mask), vandc(b, mask)), 8));
}

// int8x16
template<> SIMD_FUNC int8x16::vec(int8 x) : base(_mm_set1_epi8(x)) {}
SIMD_FUNC int8x16	int8x16::operator- ()				const	{ return _mm_sub_epi8(_mm_setzero_si128(), v); }
SIMD_FUNC int8x16	int8x16::operator+ (int8x16 b)		const	{ return _mm_add_epi8(v, b.v); }
SIMD_FUNC int8x16	int8x16::operator- (int8x16 b)		const	{ return _mm_sub_epi8(v, b.v); }
SIMD_FUNC int8x16	int8x16::operator* (int8x16 b)		const	{ return mul8x8(v, b.v); }
SIMD_FUNC auto		int8x16::operator==(int8x16 b)		const	{ return int8x16(_mm_cmpeq_epi8(v, b.v)); }
SIMD_FUNC auto		int8x16::operator< (int8x16 b)		const	{ return int8x16(_mm_cmplt_epi8(v, b.v)); }
SIMD_FUNC auto		int8x16::operator> (int8x16 b)		const	{ return int8x16(_mm_cmpgt_epi8(v, b.v)); }

// uint8x16
template<> SIMD_FUNC uint8x16::vec(uint8 x) : base(_mm_set1_epi8(x)) {}
SIMD_FUNC uint8x16	uint8x16::operator+ (uint8x16 b)	const	{ return _mm_add_epi8(v, b.v); }
SIMD_FUNC uint8x16	uint8x16::operator- (uint8x16 b)	const	{ return _mm_sub_epi8(v, b.v); }
SIMD_FUNC uint8x16	uint8x16::operator* (uint8x16 b)	const	{ return mul8x8(v, b.v); }
SIMD_FUNC auto		uint8x16::operator==(uint8x16 b)	const	{ return uint8x16(_mm_cmpeq_epi8(v, b.v)); }
SIMD_FUNC auto		uint8x16::operator> (uint8x16 b)	const	{ return as<int8>(*this ^ 0x80) > as<int8>(b ^ 0x80); }
#endif

#ifdef __AVX2__
SIMD_FUNC auto mul8x8(__m256i a, __m256i b) {
	__m256i mask = _mm256_set1_epi16(0xff);
	return vor(vand(_mm256_mullo_epi16(vand(a, mask), vand(b, mask)), mask), _mm256_slli_epi16(_mm256_mulhi_epi16(vandc(a, mask), vandc(b, mask)), 8));
}

// int8x32
template<> SIMD_FUNC int8x32::vec(int8 x) : base(_mm256_set1_epi8(x)) {}
SIMD_FUNC int8x32	int8x32::operator- ()				const	{ return _mm256_sub_epi8(_mm256_setzero_si256(), v); }
SIMD_FUNC int8x32	int8x32::operator+ (int8x32 b)		const	{ return _mm256_add_epi8(v, b.v); }
SIMD_FUNC int8x32	int8x32::operator- (int8x32 b)		const	{ return _mm256_sub_epi8(v, b.v); }
SIMD_FUNC int8x32	int8x32::operator* (int8x32 b)		const	{ return mul8x8(v, b.v); }
SIMD_FUNC auto		int8x32::operator==(int8x32 b)		const	{ return int8x32(_mm256_cmpeq_epi8(v, b.v)); }
SIMD_FUNC auto		int8x32::operator> (int8x32 b)		const	{ return int8x32(_mm256_cmpgt_epi8(v, b.v)); }

// uint8x32
template<> SIMD_FUNC uint8x32::vec(uint8 x) : base(_mm256_set1_epi8(x)) {}
SIMD_FUNC uint8x32	uint8x32::operator+ (uint8x32 b)	const	{ return _mm256_add_epi8(v, b.v); }
SIMD_FUNC uint8x32	uint8x32::operator- (uint8x32 b)	const	{ return _mm256_sub_epi8(v, b.v); }
SIMD_FUNC uint8x32	uint8x32::operator* (uint8x32 b)	const	{ return mul8x8(v, b.v); }
SIMD_FUNC auto		uint8x32::operator==(uint8x32 b)	const	{ return int8x32(_mm256_cmpeq_epi8(v, b.v)); }
SIMD_FUNC auto		uint8x32::operator> (uint8x32 b)	const	{ return as<int8>(*this ^ 0x80) > as<int8>(b ^ 0x80); }
#endif

#ifdef __AVX512F__
static SIMD_FUNC int8x32	_boolselect(int8x32 x, int8x32 y, boolx32 mask)		{ return _mm256_mask_mov_epi8(x.v, mask, y.v); }
static SIMD_FUNC uint8x32	_boolselect(uint8x32 x, uint8x32 y, boolx32 mask)	{ return _mm256_mask_mov_epi8(x.v, mask, y.v); }

SIMD_FUNC auto mul8x8(__m512i a, __m512i b) {
	__m512i mask = _mm512_set1_epi16(0xff);
	return vor(vand(_mm512_mullo_epi16(vand(a, mask), vand(b, mask)), mask), _mm512_slli_epi16(_mm512_mulhi_epi16(vandc(a, mask), vandc(b, mask)), 8));
}

// int8x64
SIMD_FUNC int8x64::vec(int8 x) : base(_mm512_set1_epi8(x)) {}
SIMD_FUNC int8x64	int8x64::operator- ()				const	{ return _mm512_sub_epi8(_mm512_setzero_si512(), v); }
SIMD_FUNC int8x64	int8x64::operator+ (int8x64 b)		const	{ return _mm512_add_epi8(v, b.v); }
SIMD_FUNC int8x64	int8x64::operator- (int8x64 b)		const	{ return _mm512_sub_epi8(v, b.v); }
SIMD_FUNC int8x64	int8x64::operator* (int8x64 b)		const	{ return mul8x8(a, b); }
SIMD_FUNC auto		int8x64::operator==(int8x64 b)		const	{ return boolx64(_mm512_cmpeq_epi8_mask(v, b.v)); }
SIMD_FUNC auto		int8x64::operator> (int8x64 b)		const	{ return boolx64(_mm512_cmpgt_epi8_mask(v, b.v)); }
SIMD_FUNC auto		int8x64::operator< (int8x64 b)		const	{ return boolx64(_mm512_cmplt_epi8_mask(v, b.v)); }
SIMD_FUNC auto		int8x64::operator!=(int8x64 b)		const	{ return boolx64(_mm512_cmpneq_epi8_mask(v, b.v)); }
SIMD_FUNC auto		int8x64::operator<=(int8x64 b)		const	{ return boolx64(_mm512_cmple_epi8_mask(v, b.v)); }
SIMD_FUNC auto		int8x64::operator>=(int8x64 b)		const	{ return boolx64(_mm512_cmpge_epi8_mask(v, b.v)); }
static SIMD_FUNC int8x64 _boolselect(int8x64 x, int8x64 y, boolx64 mask) { return _mm512_mask_mov_epi8(x.v, mask, y.v); }

// uint8x64
SIMD_FUNC uint8x64::vec(uint8 x) : base(_mm512_set1_epi8(x)){}
SIMD_FUNC uint8x64	uint8x64::operator+ (uint8x64 b)	const	{ return _mm512_add_epi8(v, b.v); }
SIMD_FUNC uint8x64	uint8x64::operator- (uint8x64 b)	const	{ return _mm512_sub_epi8(v, b.v); }
SIMD_FUNC uint8x64	uint8x64::operator* (uint8x64 b)	const	{ return mul8x8(a, b); }
SIMD_FUNC auto		uint8x64::operator==(uint8x64 b)	const	{ return boolx64(_mm512_cmpeq_epu8_mask(v, b.v)); }
SIMD_FUNC auto		uint8x64::operator> (uint8x64 b)	const	{ return boolx64(_mm512_cmpgt_epu8_mask(v, b.v)); }
SIMD_FUNC auto		uint8x64::operator< (uint8x64 b)	const	{ return boolx64(_mm512_cmplt_epu8_mask(v, b.v)); }
SIMD_FUNC auto		uint8x64::operator!=(uint8x64 b)	const	{ return boolx64(_mm512_cmpneq_epu8_mask(v, b.v)); }
SIMD_FUNC auto		uint8x64::operator<=(uint8x64 b)	const	{ return boolx64(_mm512_cmple_epu8_mask(v, b.v)); }
SIMD_FUNC auto		uint8x64::operator>=(uint8x64 b)	const	{ return boolx64(_mm512_cmpge_epu8_mask(v, b.v)); }
static SIMD_FUNC uint8x64 _boolselect(uint8x64 x, uint8x64 y, boolx64 mask) { return _mm512_mask_mov_epi8(x.v, mask, y.v); }
#endif

#ifdef __ARM_NEON__
// int8x8
template<> SIMD_FUNC int8x8::vec(int8 x) : base(vdup_n_s8(x)) {}
SIMD_FUNC int8x8	int8x8::operator- ()				const	{ return vneg_s8(v); }
SIMD_FUNC int8x8	int8x8::operator+ (int8x8 b)		const	{ return vadd_s8(v, b.v); }
SIMD_FUNC int8x8	int8x8::operator- (int8x8 b)		const	{ return vsub_s8(v, b.v); }
SIMD_FUNC int8x8	int8x8::operator* (int8x8 b)		const	{ return vmul_s8(v, b.v); }
SIMD_FUNC auto		int8x8::operator==(int8x8 b)		const	{ return int8x8(vceq_s8(v, b.v)); }
SIMD_FUNC auto		int8x8::operator< (int8x8 b)		const	{ return int8x8(vclt_s8(v, b.v)); }
SIMD_FUNC auto		int8x8::operator> (int8x8 b)		const	{ return int8x8(vcgt_s8(v, b.v)); }
SIMD_FUNC int8x8	shr(int8x8 a, int b)						{ return vshl_s8(a, vdup_n_s8(-b)); }	// can use vshr_n_s8 if b is known
SIMD_FUNC int8x8	shl(int8x8 a, int b)						{ return vshl_s8(a, vdup_n_s8(b)); }	// can use vshl_n_s8 if b is known

// uint8x8
template<> SIMD_FUNC uint8x8::vec(uint8 x) : base(vdup_n_u8(x)) {}
SIMD_FUNC uint8x8	uint8x8::operator+ (uint8x8 b)		const	{ return vadd_u8(v, b.v); }
SIMD_FUNC uint8x8	uint8x8::operator- (uint8x8 b)		const	{ return vsub_u8(v, b.v); }
SIMD_FUNC uint8x8	uint8x8::operator* (uint8x8 b)		const	{ return vmul_u8(v, b.v); }
SIMD_FUNC auto		uint8x8::operator==(uint8x8 b)		const	{ return int8x8(vceq_u8(v, b.v)); }
SIMD_FUNC auto		uint8x8::operator> (uint8x8 b)		const	{ return int8x8(vcgt_u8(v, b.v)); }
SIMD_FUNC uint8x8	shr(uint8x8 a, int b)						{ return vshl_u8(a, vdup_n_s8(-b)); }	// can use vshr_n_u8 if b is known
SIMD_FUNC uint8x8	shl(uint8x8 a, int b)						{ return vshl_u8(a, vdup_n_s8(b)); }	// can use vshl_n_u8 if b is known
																										
// int8x16
template<> SIMD_FUNC int8x16::vec(int8 x) : base(vdupq_n_s8(x)) {}
SIMD_FUNC int8x16	int8x16::operator- ()				const	{ return vnegq_s8(v); }
SIMD_FUNC int8x16	int8x16::operator+ (int8x16 b)		const	{ return vaddq_s8(v, b.v); }
SIMD_FUNC int8x16	int8x16::operator- (int8x16 b)		const	{ return vsubq_s8(v, b.v); }
SIMD_FUNC int8x16	int8x16::operator* (int8x16 b)		const	{ return vmulq_s8(v, b.v); }
SIMD_FUNC auto		int8x16::operator==(int8x16 b)		const	{ return int8x16(vceqq_s8(v, b.v)); }
SIMD_FUNC auto		int8x16::operator< (int8x16 b)		const	{ return int8x16(vcltq_s8(v, b.v)); }
SIMD_FUNC auto		int8x16::operator> (int8x16 b)		const	{ return int8x16(vcgtq_s8(v, b.v)); }
SIMD_FUNC int8x16	shr(int8x16 a, int b)						{ return vshlq_s8(a, vdupq_n_s8(-b)); }	// can use vshrq_n_s8 if b is known
SIMD_FUNC int8x16	shl(int8x16 a, int b)						{ return vshlq_s8(a, vdupq_n_s8(b)); }	// can use vshlq_n_s8 if b is known

// uint8x16
template<> SIMD_FUNC uint8x16::vec(uint8 x) : base(vdupq_n_u8(x)) {}
SIMD_FUNC uint8x16	uint8x16::operator+ (uint8x16 b)	const	{ return vaddq_u8(v, b.v); }
SIMD_FUNC uint8x16	uint8x16::operator- (uint8x16 b)	const	{ return vsubq_u8(v, b.v); }
SIMD_FUNC uint8x16	uint8x16::operator* (uint8x16 b)	const	{ return vmulq_u8(v, b.v); }
SIMD_FUNC auto		uint8x16::operator==(uint8x16 b)	const	{ return int8x16(vceqq_u8(v, b.v)); }
SIMD_FUNC auto		uint8x16::operator> (uint8x16 b)	const	{ return int8x16(vcgtq_u8(v, b.v)); }
SIMD_FUNC uint8x16	shr(uint8x16 a, int b)						{ return vshlq_u8(a, vdupq_n_s8(-b)); }	// can use vshrq_n_u8 if b is known
SIMD_FUNC uint8x16	shl(uint8x16 a, int b)						{ return vshlq_u8(a, vdupq_n_s8(b)); }	// can use vshlq_n_u8 if b is known
#endif

//-----------------------------------------------------------------------------
// vectors of int16s
//-----------------------------------------------------------------------------

#ifdef __SSE__
// int16x8
template<> SIMD_FUNC int16x8::vec(int16 x) : base(_mm_set1_epi16(x)) {}
template<> SIMD_FUNC int16x8	int16x8::operator- ()				const	{ return _mm_sub_epi16(_mm_setzero_si128(), v); }
template<> SIMD_FUNC int16x8	int16x8::operator+ (int16x8 b)		const	{ return _mm_add_epi16(v, b.v); }
template<> SIMD_FUNC int16x8	int16x8::operator- (int16x8 b)		const	{ return _mm_sub_epi16(v, b.v); }
template<> SIMD_FUNC int16x8	int16x8::operator* (int16x8 b)		const	{ return _mm_mullo_epi16(v, b.v); }
template<> SIMD_FUNC auto		int16x8::operator==(int16x8 b)		const	{ return int16x8(_mm_cmpeq_epi16(v, b.v)); }
template<> SIMD_FUNC auto		int16x8::operator< (int16x8 b)		const	{ return int16x8(_mm_cmplt_epi16(v, b.v)); }
template<> SIMD_FUNC auto		int16x8::operator> (int16x8 b)		const	{ return int16x8(_mm_cmpgt_epi16(v, b.v)); }
SIMD_FUNC int16x8	shr(int16x8   a, int b)									{ return _mm_sra_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int16x8	shl(int16x8   a, int b)									{ return _mm_sll_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int16x8	mulhi(int16x8 a, int16x8 b)								{ return _mm_mulhi_epi16(a.v, b.v); }

// uint16x8
template<> SIMD_FUNC uint16x8::vec(uint16 x) : base(_mm_set1_epi16(x)) {}
template<> SIMD_FUNC uint16x8	uint16x8::operator+ (uint16x8 b)	const	{ return _mm_add_epi16(v, b.v); }
template<> SIMD_FUNC uint16x8	uint16x8::operator- (uint16x8 b)	const	{ return _mm_sub_epi16(v, b.v); }
template<> SIMD_FUNC uint16x8	uint16x8::operator* (uint16x8 b)	const	{ return _mm_mullo_epi16(v, b.v); }
template<> SIMD_FUNC auto		uint16x8::operator==(uint16x8 b)	const	{ return int16x8(_mm_cmpeq_epi16(v, b.v)); }
template<> SIMD_FUNC auto		uint16x8::operator> (uint16x8 b)	const	{ return as<int16>(*this ^ 0x8000) > as<int16>(b ^ 0x8000); }
SIMD_FUNC uint16x8	shr(uint16x8  a, int b)									{ return _mm_srl_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint16x8	shl(uint16x8  a, int b)									{ return _mm_sll_epi16(a.v, _mm_cvtsi64_si128(b)); }
#endif

#ifdef __AVX2__
// int16x16
template<> SIMD_FUNC int16x16::vec(int16 x) : base(_mm256_set1_epi16(x)) {}
template<> SIMD_FUNC int16x16	int16x16::operator- ()				const	{ return _mm256_sub_epi16(_mm256_setzero_si256(), v); }
template<> SIMD_FUNC int16x16	int16x16::operator+ (int16x16 b)	const	{ return _mm256_add_epi16(v, b.v); }
template<> SIMD_FUNC int16x16	int16x16::operator- (int16x16 b)	const	{ return _mm256_sub_epi16(v, b.v); }
template<> SIMD_FUNC int16x16	int16x16::operator* (int16x16 b)	const	{ return _mm256_mullo_epi16(v, b.v); }
template<> SIMD_FUNC auto		int16x16::operator==(int16x16 b)	const	{ return int16x16(_mm256_cmpeq_epi16(v, b.v)); }
template<> SIMD_FUNC auto		int16x16::operator> (int16x16 b)	const	{ return int16x16(_mm256_cmpgt_epi16(v, b.v)); }
SIMD_FUNC int16x16	shr(int16x16  a, int b)									{ return _mm256_sra_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int16x16	shl(int16x16  a, int b)									{ return _mm256_sll_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int16x16	mulhi(int16x16 a, int16x16 b)							{ return _mm256_mulhi_epi16(a.v, b.v); }


// uint16x16
template<> SIMD_FUNC uint16x16::vec(uint16 x) : base(_mm256_set1_epi16(x)) {}
template<> SIMD_FUNC uint16x16	uint16x16::operator+ (uint16x16 b)	const	{ return _mm256_add_epi16(v, b.v); }
template<> SIMD_FUNC uint16x16	uint16x16::operator- (uint16x16 b)	const	{ return _mm256_sub_epi16(v, b.v); }
template<> SIMD_FUNC uint16x16	uint16x16::operator* (uint16x16 b)	const	{ return _mm256_mullo_epi16(v, b.v); }
template<> SIMD_FUNC auto		uint16x16::operator==(uint16x16 b)	const	{ return int16x16(_mm256_cmpeq_epi16(v, b.v)); }
template<> SIMD_FUNC auto		uint16x16::operator> (uint16x16 b)	const	{ return as<int16>(*this ^ 0x8000) > as<int16>(b ^ 0x8000); }
SIMD_FUNC uint16x16	shr(uint16x16 a, int b)									{ return _mm256_srl_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint16x16	shl(uint16x16 a, int b)									{ return _mm256_sll_epi16(a.v, _mm_cvtsi64_si128(b)); }
#endif

#ifdef __AVX512F__
static SIMD_FUNC int16x16	_boolselect(int16x16 x, int16x16 y, boolx16 mask)	{ return _mm256_mask_mov_epi16(x.v, mask, y.v); }
static SIMD_FUNC uint16x16	_boolselect(uint16x16 x, uint16x16 y, boolx16 mask)	{ return _mm256_mask_mov_epi16(x.v, mask, y.v); }

// int16x32
SIMD_FUNC int16x32::vec(int16 x) : base(_mm512_set1_epi16(x)) {}
SIMD_FUNC int16x32	int16x32::operator- ()				const	{ return _mm512_sub_epi16(_mm512_setzero_si512(), v); }
SIMD_FUNC int16x32	int16x32::operator+ (int16x32 b)	const	{ return _mm512_add_epi16(v, b.v); }
SIMD_FUNC int16x32	int16x32::operator- (int16x32 b)	const	{ return _mm512_sub_epi16(v, b.v); }
SIMD_FUNC int16x32	int16x32::operator* (int16x32 b)	const	{ return _mm512_mullo_epi16(v, b.v); }
SIMD_FUNC boolx32	int16x32::operator==(int16x32 b)	const	{ return _mm512_cmpeq_epi16_mask(v, b.v); }
SIMD_FUNC boolx32	int16x32::operator> (int16x32 b)	const	{ return _mm512_cmpgt_epi16_mask(v, b.v); }
SIMD_FUNC boolx32	int16x32::operator< (int16x32 b)	const	{ return _mm512_cmplt_epi16_mask(v, b.v); }
SIMD_FUNC boolx32	int16x32::operator!=(int16x32 b)	const	{ return _mm512_cmpneq_epi16_mask(v, b.v); }
SIMD_FUNC boolx32	int16x32::operator<=(int16x32 b)	const	{ return _mm512_cmple_epi16_mask(v, b.v); }
SIMD_FUNC boolx32	int16x32::operator>=(int16x32 b)	const	{ return _mm512_cmpge_epi16_mask(v, b.v); }
SIMD_FUNC int16x32	shr(int16x32 a, int b)						{ return _mm512_sra_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int16x32	shl(int16x32 a, int b)						{ return _mm512_sll_epi16(a.v, _mm_cvtsi64_si128(b)); }
static SIMD_FUNC int16x32 _boolselect(int16x32 x, int16x32 y, boolx32 mask) { return _mm512_mask_mov_epi16(x.v, mask, y.v); }

// uint16x32
SIMD_FUNC uint16x32::vec(uint16 x) : v(_mm512_set1_epi16(x)) {}
SIMD_FUNC uint16x32	uint16x32::operator+ (uuint16x32 b)	const	{ return _mm512_add_epi16(v, b.v); }
SIMD_FUNC uint16x32	uint16x32::operator- (uuint16x32 b)	const	{ return _mm512_sub_epi16(v, b.v); }
SIMD_FUNC uint16x32	uint16x32::operator* (uuint16x32 b)	const	{ return _mm512_mullo_epi16(v, b.v); }
SIMD_FUNC boolx32	uint16x32::operator==(uuint16x32 b)	const	{ return _mm512_cmpeq_epu16_mask(v, b.v); }
SIMD_FUNC boolx32	uint16x32::operator> (uuint16x32 b)	const	{ return _mm512_cmpgt_epu16_mask(v, b.v); }
SIMD_FUNC boolx32	uint16x32::operator< (uuint16x32 b)	const	{ return _mm512_cmplt_epu16_mask(v, b.v); }
SIMD_FUNC boolx32	uint16x32::operator!=(uuint16x32 b)	const	{ return _mm512_cmpneq_epu16_mask(v, b.v); }
SIMD_FUNC boolx32	uint16x32::operator<=(uuint16x32 b)	const	{ return _mm512_cmple_epu16_mask(v, b.v); }
SIMD_FUNC boolx32	uint16x32::operator>=(uuint16x32 b)	const	{ return _mm512_cmpge_epu16_mask(v, b.v); }
SIMD_FUNC uint16x32	shr(uint16x32 a, uint b)					{ return _mm512_srl_epi16(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint16x32	shl(uint16x32 a, uint b)					{ return _mm512_sll_epi16(a.v, _mm_cvtsi64_si128(b)); }
static SIMD_FUNC uint16x32 _boolselect(uint16x32 x, uint16x32 y, boolx32 mask) { return _mm512_mask_mov_epi16(x.v, mask, y.v); }
#endif

#ifdef __ARM_NEON__
// int16x4
template<> SIMD_FUNC int16x4::vec(int16 x) : base(vdup_n_s16(x)) {}
SIMD_FUNC int16x4	int16x4::operator- ()				const	{ return vneg_s16(v); }
SIMD_FUNC int16x4	int16x4::operator+ (int16x4 b)		const	{ return vadd_s16(v, b.v); }
SIMD_FUNC int16x4	int16x4::operator- (int16x4 b)		const	{ return vsub_s16(v, b.v); }
SIMD_FUNC int16x4	int16x4::operator* (int16x4 b)		const	{ return vmul_s16(v, b.v); }
SIMD_FUNC auto		int16x4::operator==(int16x4 b)		const	{ return int16x4(vceq_s16(v, b.v)); }
SIMD_FUNC auto		int16x4::operator< (int16x4 b)		const	{ return int16x4(vclt_s16(v, b.v)); }
SIMD_FUNC auto		int16x4::operator> (int16x4 b)		const	{ return int16x4(vcgt_s16(v, b.v)); }
SIMD_FUNC int16x4	shr(int16x4 a, int b)						{ return vshl_s16(a, vdup_n_s16(-b)); }	// can use vshr_n_s16 if b is known
SIMD_FUNC int16x4	shl(int16x4 a, int b)						{ return vshl_s16(a, vdup_n_s16(b)); }	// can use vshl_n_s16 if b is known

// uint16x4
template<> SIMD_FUNC uint16x4::vec(uint16 x) : base(vdup_n_u16(x)) {}
SIMD_FUNC uint16x4	uint16x4::operator+ (uint16x4 b)	const	{ return vadd_u16(v, b.v); }
SIMD_FUNC uint16x4	uint16x4::operator- (uint16x4 b)	const	{ return vsub_u16(v, b.v); }
SIMD_FUNC uint16x4	uint16x4::operator* (uint16x4 b)	const	{ return vmul_u16(v, b.v); }
SIMD_FUNC auto		uint16x4::operator==(uint16x4 b)	const	{ return int16x4(vceq_u16(v, b.v)); }
SIMD_FUNC auto		uint16x4::operator> (uint16x4 b)	const	{ return int16x4(vcgt_u16(v, b.v)); }
SIMD_FUNC uint16x4	shr(uint16x4 a, int b)						{ return vshl_u16(a, vdup_n_s16(-b)); }	// can use vshr_n_u16 if b is known
SIMD_FUNC uint16x4	shl(uint16x4 a, int b)						{ return vshl_u16(a, vdup_n_s16(b)); }	// can use vshl_n_u16 if b is known
																											
// int16x8
template<> SIMD_FUNC int16x8::vec(int16 x) : base(vdupq_n_s16(x)) {}
SIMD_FUNC int16x8	int16x8::operator- ()				const	{ return vnegq_s16(v); }
SIMD_FUNC int16x8	int16x8::operator+ (int16x8 b)		const	{ return vaddq_s16(v, b.v); }
SIMD_FUNC int16x8	int16x8::operator- (int16x8 b)		const	{ return vsubq_s16(v, b.v); }
SIMD_FUNC int16x8	int16x8::operator* (int16x8 b)		const	{ return vmulq_s16(v, b.v); }
SIMD_FUNC auto		int16x8::operator==(int16x8 b)		const	{ return int16x8(vceqq_s16(v, b.v)); }
SIMD_FUNC auto		int16x8::operator< (int16x8 b)		const	{ return int16x8(vcltq_s16(v, b.v)); }
SIMD_FUNC auto		int16x8::operator> (int16x8 b)		const	{ return int16x8(vcgtq_s16(v, b.v)); }
SIMD_FUNC int16x8	shr(int16x8 a, int b)						{ return vshlq_s16(a, vdupq_n_s16(-b)); }	// can use vshrq_n_s16 if b is known
SIMD_FUNC int16x8	shl(int16x8 a, int b)						{ return vshlq_s16(a, vdupq_n_s16(b)); }	// can use vshlq_n_s16 if b is known

// uint16x8
template<> SIMD_FUNC uint16x8::vec(uint16 x) : base(vdupq_n_u16(x)) {}
SIMD_FUNC uint16x8	uint16x8::operator+ (uint16x8 b)	const	{ return vaddq_u16(v, b.v); }
SIMD_FUNC uint16x8	uint16x8::operator- (uint16x8 b)	const	{ return vsubq_u16(v, b.v); }
SIMD_FUNC uint16x8	uint16x8::operator* (uint16x8 b)	const	{ return vmulq_u16(v, b.v); }
SIMD_FUNC auto		uint16x8::operator==(uint16x8 b)	const	{ return int16x8(vceqq_u16(v, b.v)); }
SIMD_FUNC auto		uint16x8::operator> (uint16x8 b)	const	{ return int16x8(vcgtq_u16(v, b.v)); }
SIMD_FUNC uint16x8	shr(uint16x8 a, int b)						{ return vshlq_u16(a, vdupq_n_s16(-b)); }	// can use vshrq_n_u16 if b is known
SIMD_FUNC uint16x8	shl(uint16x8 a, int b)						{ return vshlq_u16(a, vdupq_n_s16(b)); }	// can use vshlq_n_u16 if b is known

#endif
//-----------------------------------------------------------------------------
// vectors of ints
//-----------------------------------------------------------------------------

#ifdef __SSE__
// int32x4
SIMD_FUNC int32x4::vec(int32 x)		: base(_mm_set1_epi32(x))	{}
SIMD_FUNC int32x4	int32x4::operator- ()				const	{ return _mm_sub_epi32(_mm_setzero_si128(), v); }
SIMD_FUNC int32x4	int32x4::operator+ (int32x4 b)		const	{ return _mm_add_epi32(v, b.v); }
SIMD_FUNC int32x4	int32x4::operator- (int32x4 b)		const	{ return _mm_sub_epi32(v, b.v); }
SIMD_FUNC int32x4	int32x4::operator* (int32x4 b)		const	{ return _mm_mullo_epi32(v, b.v); }
SIMD_FUNC auto		int32x4::operator==(int32x4 b)		const	{ return int32x4(_mm_cmpeq_epi32(v, b.v)); }
SIMD_FUNC auto		int32x4::operator< (int32x4 b)		const	{ return int32x4(_mm_cmplt_epi32(v, b.v)); }
SIMD_FUNC auto		int32x4::operator> (int32x4 b)		const	{ return int32x4(_mm_cmpgt_epi32(v, b.v)); }
SIMD_FUNC int32x4	shr(int32x4  a, int b)						{ return _mm_sra_epi32(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int32x4	shl(int32x4  a, int b)						{ return _mm_sll_epi32(a.v, _mm_cvtsi64_si128(b)); }

// uint32x4
SIMD_FUNC uint32x4::vec(uint32 x)	: base(_mm_set1_epi32(x))	{}
SIMD_FUNC uint32x4	uint32x4::operator+ (uint32x4 b)	const	{ return _mm_add_epi32(v, b.v); }
SIMD_FUNC uint32x4	uint32x4::operator- (uint32x4 b)	const	{ return _mm_sub_epi32(v, b.v); }
SIMD_FUNC uint32x4	uint32x4::operator* (uint32x4 b)	const	{ return _mm_mullo_epi32(v, b.v); }
SIMD_FUNC auto		uint32x4::operator==(uint32x4 b)	const	{ return int32x4(_mm_cmpeq_epi32(v, b.v)); }
SIMD_FUNC auto		uint32x4::operator> (uint32x4 b)	const	{ return as<int32>(*this ^ 0x80000000) > as<int32>(b ^ 0x80000000); }
SIMD_FUNC uint32x4	shr(uint32x4 a, int b)						{ return _mm_srl_epi32(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint32x4	shl(uint32x4 a, int b)						{ return _mm_sll_epi32(a.v, _mm_cvtsi64_si128(b)); }
#endif

#ifdef __AVX2__
// int32x8
SIMD_FUNC int32x8::vec(int32 x) : base(_mm256_set1_epi32(x)) {}
SIMD_FUNC int32x8	int32x8::operator- ()				const	{ return _mm256_sub_epi32(_mm256_setzero_si256(), v); }
SIMD_FUNC int32x8	int32x8::operator+ (int32x8 b)		const	{ return _mm256_add_epi32(v, b.v); }
SIMD_FUNC int32x8	int32x8::operator- (int32x8 b)		const	{ return _mm256_sub_epi32(v, b.v); }
SIMD_FUNC int32x8	int32x8::operator* (int32x8 b)		const	{ return _mm256_mullo_epi32(v, b.v); }
SIMD_FUNC auto		int32x8::operator==(int32x8 b)		const	{ return int32x8(_mm256_cmpeq_epi32(v, b.v)); }
SIMD_FUNC auto		int32x8::operator> (int32x8 b)		const	{ return int32x8(_mm256_cmpgt_epi32(v, b.v)); }
SIMD_FUNC int32x8	shr(int32x8  a, int b)						{ return _mm256_sra_epi32(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int32x8	shl(int32x8  a, int b)						{ return _mm256_sll_epi32(a.v, _mm_cvtsi64_si128(b)); }

// uint32x8
SIMD_FUNC uint32x8::vec(uint32 x) : base(_mm256_set1_epi32(x)) {}
SIMD_FUNC uint32x8	uint32x8::operator+ (uint32x8 b)	const	{ return _mm256_add_epi32(v, b.v); }
SIMD_FUNC uint32x8	uint32x8::operator- (uint32x8 b)	const	{ return _mm256_sub_epi32(v, b.v); }
SIMD_FUNC uint32x8	uint32x8::operator* (uint32x8 b)	const	{ return _mm256_mullo_epi32(v, b.v); }
SIMD_FUNC auto		uint32x8::operator==(uint32x8 b)	const	{ return int32x8(_mm256_cmpeq_epi32(v, b.v)); }
SIMD_FUNC auto		uint32x8::operator> (uint32x8 b)	const	{ return as<int32>(*this ^ 0x80000000) > as<int32>(b ^ 0x80000000); }
SIMD_FUNC uint32x8	shr(uint32x8 a, int b)						{ return _mm256_srl_epi32(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint32x8	shl(uint32x8 a, int b)						{ return _mm256_sll_epi32(a.v, _mm_cvtsi64_si128(b)); }
#endif

#ifdef __AVX512F__
static SIMD_FUNC int32x8	_boolselect(int32x8 x, int32x8 y, boolx8 mask)		{ return _mm256_mask_mov_epi32(x.v, mask, y.v); }
static SIMD_FUNC uint32x8	_boolselect(uint32x8 x, uint32x8 y, boolx8 mask)	{ return _mm256_mask_mov_epi32(x.v, mask, y.v); }

// int32x16
SIMD_FUNC int32x16::vec(int32 x) : base(_mm512_set1_epi32(x)) {}
SIMD_FUNC int32x16	int32x16::operator- ()				const	{ return _mm512_sub_epi32(_mm512_setzero_si512(), v); }
SIMD_FUNC int32x16	int32x16::operator+ (int32x16 b)	const	{ return _mm512_add_epi32(v, b.v); }
SIMD_FUNC int32x16	int32x16::operator- (int32x16 b)	const	{ return _mm512_sub_epi32(v, b.v); }
SIMD_FUNC int32x16	int32x16::operator* (int32x16 b)	const	{ return _mm512_mullo_epi32(v, b.v); }
SIMD_FUNC boolx16	int32x16::operator==(int32x16 b)	const	{ return _mm512_cmpeq_epi32_mask(v, b.v); }
SIMD_FUNC boolx16	int32x16::operator> (int32x16 b)	const	{ return _mm512_cmpgt_epi32_mask(v, b.v); }
SIMD_FUNC boolx16	int32x16::operator< (int32x16 b)	const	{ return _mm512_cmplt_epi32_mask(v, b.v); }
SIMD_FUNC boolx16	int32x16::operator!=(int32x16 b)	const	{ return _mm512_cmpneq_epi32_mask(v, b.v); }
SIMD_FUNC boolx16	int32x16::operator<=(int32x16 b)	const	{ return _mm512_cmple_epi32_mask(v, b.v); }
SIMD_FUNC boolx16	int32x16::operator>=(int32x16 b)	const	{ return _mm512_cmpge_epi32_mask(v, b.v); }
SIMD_FUNC int32x16	shr(int32x16 a, int b)						{ return _mm512_sra_epi32(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int32x16	shl(int32x16 a, int b)						{ return _mm512_sll_epi32(a.v, _mm_cvtsi64_si128(b)); }
static SIMD_FUNC int32x16 _boolselect(int32x16 x, int32x16 y, boolx16 mask) { return _mm512_mask_mov_epi32(x.v, mask, y.v); }

// uint32x16
SIMD_FUNC uint32x16::vec(uint32 x) : base(_mm512_set1_epi32(x)) {}
SIMD_FUNC uint32x16	uint32x16::operator+ (uint32x16 b)	const	{ return _mm512_add_epi32(v, b.v); }
SIMD_FUNC uint32x16	uint32x16::operator- (uint32x16 b)	const	{ return _mm512_sub_epi32(v, b.v); }
SIMD_FUNC uint32x16	uint32x16::operator* (uint32x16 b)	const	{ return _mm512_mullo_epi32(v, b.v); }
SIMD_FUNC boolx16	uint32x16::operator==(uint32x16 b)	const	{ return _mm512_cmpeq_epu32_mask(v, b.v); }
SIMD_FUNC boolx16	uint32x16::operator> (uint32x16 b)	const	{ return _mm512_cmpgt_epu32_mask(v, b.v); }
SIMD_FUNC boolx16	uint32x16::operator< (uint32x16 b)	const	{ return _mm512_cmplt_epu32_mask(v, b.v); }
SIMD_FUNC boolx16	uint32x16::operator!=(uint32x16 b)	const	{ return _mm512_cmpneq_epu32_mask(v, b.v); }
SIMD_FUNC boolx16	uint32x16::operator<=(uint32x16 b)	const	{ return _mm512_cmple_epu32_mask(v, b.v); }
SIMD_FUNC boolx16	uint32x16::operator>=(uint32x16 b)	const	{ return _mm512_cmpge_epu32_mask(v, b.v); }
SIMD_FUNC uint32x16	shr(uint32x16 a, int b)						{ return _mm512_srl_epi32(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint32x16	shl(uint32x16 a, int b)						{ return _mm512_sll_epi32(a.v, _mm_cvtsi64_si128(b)); }
static SIMD_FUNC uint32x16 _boolselect(uint32x16 x, uint32x16 y, boolx16 mask) { return _mm512_mask_mov_epi32(x.v, mask, y.v); }
#endif

#ifdef __ARM_NEON__
// int32x16
template<> SIMD_FUNC int32x2::vec(int32 x) : base(vdup_n_s32(x)) {}
SIMD_FUNC int32x2	int32x2::operator- ()				const	{ return vneg_s32(v); }
SIMD_FUNC int32x2	int32x2::operator+ (int32x2 b)		const	{ return vadd_s32(v, b.v); }
SIMD_FUNC int32x2	int32x2::operator- (int32x2 b)		const	{ return vsub_s32(v, b.v); }
SIMD_FUNC int32x2	int32x2::operator* (int32x2 b)		const	{ return vmul_s32(v, b.v); }
SIMD_FUNC auto		int32x2::operator==(int32x2 b)		const	{ return int32x2(vceq_s32(v, b.v)); }
SIMD_FUNC auto		int32x2::operator< (int32x2 b)		const	{ return int32x2(vclt_s32(v, b.v)); }
SIMD_FUNC auto		int32x2::operator> (int32x2 b)		const	{ return int32x2(vcgt_s32(v, b.v)); }
SIMD_FUNC int32x2	shr(int32x2 a, int b)						{ return vshl_s32(a, vdup_n_s32(-b)); }	// can use vshr_n_s32 if b is known
SIMD_FUNC int32x2	shl(int32x2 a, int b)						{ return vshl_s32(a, vdup_n_s32(b)); }	// can use vshl_n_s32 if b is known

// uint32x2
template<> SIMD_FUNC uint32x2::vec(uint32 x) : base(vdup_n_u32(x)) {}
SIMD_FUNC uint32x2	uint32x2::operator+ (uint32x2 b)	const	{ return vadd_u32(v, b.v); }
SIMD_FUNC uint32x2	uint32x2::operator- (uint32x2 b)	const	{ return vsub_u32(v, b.v); }
SIMD_FUNC uint32x2	uint32x2::operator* (uint32x2 b)	const	{ return vmul_u32(v, b.v); }
SIMD_FUNC auto		uint32x2::operator==(uint32x2 b)	const	{ return int32x2(vceq_u32(v, b.v)); }
SIMD_FUNC auto		uint32x2::operator> (uint32x2 b)	const	{ return int32x2(vcgt_u32(v, b.v)); }
SIMD_FUNC uint32x2	shr(uint32x2 a, int b)						{ return vshl_u32(a, vdup_n_s32(-b)); }	// can use vshr_n_u32 if b is known
SIMD_FUNC uint32x2	shl(uint32x2 a, int b)						{ return vshl_u32(a, vdup_n_s32(b)); }	// can use vshl_n_u32 if b is known
																											
// int32x4
template<> SIMD_FUNC int32x4::vec(int32 x) : base(vdupq_n_s32(x)) {}
SIMD_FUNC int32x4	int32x4::operator- ()				const	{ return vnegq_s32(v); }
SIMD_FUNC int32x4	int32x4::operator+ (int32x4 b)		const	{ return vaddq_s32(v, b.v); }
SIMD_FUNC int32x4	int32x4::operator- (int32x4 b)		const	{ return vsubq_s32(v, b.v); }
SIMD_FUNC int32x4	int32x4::operator* (int32x4 b)		const	{ return vmulq_s32(v, b.v); }
SIMD_FUNC auto		int32x4::operator==(int32x4 b)		const	{ return int32x4(vceqq_s32(v, b.v)); }
SIMD_FUNC auto		int32x4::operator< (int32x4 b)		const	{ return int32x4(vcltq_s32(v, b.v)); }
SIMD_FUNC auto		int32x4::operator> (int32x4 b)		const	{ return int32x4(vcgtq_s32(v, b.v)); }
SIMD_FUNC int32x4	shr(int32x4 a, int b)						{ return vshlq_s32(a, vdupq_n_s32(-b)); }	// can use vshrq_n_s32 if b is known
SIMD_FUNC int32x4	shl(int32x4 a, int b)						{ return vshlq_s32(a, vdupq_n_s32(b)); }	// can use vshlq_n_s32 if b is known

// uint32x4
template<> SIMD_FUNC uint32x4::vec(uint32 x) : base(vdupq_n_u32(x)) {}
SIMD_FUNC uint32x4	uint32x4::operator+ (uint32x4 b)	const	{ return vaddq_u32(v, b.v); }
SIMD_FUNC uint32x4	uint32x4::operator- (uint32x4 b)	const	{ return vsubq_u32(v, b.v); }
SIMD_FUNC uint32x4	uint32x4::operator* (uint32x4 b)	const	{ return vmulq_u32(v, b.v); }
SIMD_FUNC auto		uint32x4::operator==(uint32x4 b)	const	{ return int32x4(vceqq_u32(v, b.v)); }
SIMD_FUNC auto		uint32x4::operator> (uint32x4 b)	const	{ return int32x4(vcgtq_u32(v, b.v)); }
SIMD_FUNC uint32x4	shr(uint32x4 a, int b)						{ return vshlq_u32(a, vdupq_n_s32(-b)); }	// can use vshrq_n_u32 if b is known
SIMD_FUNC uint32x4	shl(uint32x4 a, int b)						{ return vshlq_u32(a, vdupq_n_s32(b)); }	// can use vshlq_n_u32 if b is known
#endif

//-----------------------------------------------------------------------------
// vectors of longs
//-----------------------------------------------------------------------------

#ifdef __SSE__
// int64x2
SIMD_FUNC int64x2::vec(int64 x)	: base(_mm_set1_epi64x(x)) {}
SIMD_FUNC int64x2	int64x2::operator- ()				const	{ return _mm_sub_epi64(_mm_setzero_si128(), v); }
SIMD_FUNC int64x2	int64x2::operator+ (int64x2 b)		const	{ return _mm_add_epi64(v, b.v); }
SIMD_FUNC int64x2	int64x2::operator- (int64x2 b)		const	{ return _mm_sub_epi64(v, b.v); }
SIMD_FUNC auto		int64x2::operator==(int64x2 b)		const	{ return int64x2(_mm_cmpeq_epi64(v, b.v)); }
SIMD_FUNC auto		int64x2::operator> (int64x2 b)		const	{ return int64x2(_mm_cmpgt_epi64(v, b.v)); }
SIMD_FUNC auto		int64x2::operator< (int64x2 b)		const	{ return b > *this; }
SIMD_FUNC int64x2	shr(int64x2  a, int b)						{ return _mm_sra_epi64(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int64x2	shl(int64x2  a, int b)						{ return _mm_sll_epi64(a.v, _mm_cvtsi64_si128(b)); }

// uint64x2
SIMD_FUNC uint64x2::vec(uint64 x) : base(_mm_set1_epi64x(x)) {}
SIMD_FUNC uint64x2	uint64x2::operator+ (uint64x2 b)	const	{ return _mm_add_epi64(v, b.v); }
SIMD_FUNC uint64x2	uint64x2::operator- (uint64x2 b)	const	{ return _mm_sub_epi64(v, b.v); }
SIMD_FUNC auto		uint64x2::operator==(uint64x2 b)	const	{ return int64x2(_mm_cmpeq_epi64(v, b.v)); }
SIMD_FUNC auto		uint64x2::operator> (uint64x2 b)	const	{ return as<int64>(*this ^ (int64(1) << 63)) > as<int64>(b ^ (int64(1) << 63)); }
SIMD_FUNC auto		uint64x2::operator< (uint64x2 b)	const	{ return b > *this; }
SIMD_FUNC uint64x2	shr(uint64x2 a, int b)						{ return _mm_srl_epi64(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint64x2	shl(uint64x2 a, int b)						{ return _mm_sll_epi64(a.v, _mm_cvtsi64_si128(b)); }
#endif

#ifdef __AVX2__
// int64x4
SIMD_FUNC int64x4::vec(int64 x) : base(_mm256_set1_epi64x(x)) {}
SIMD_FUNC int64x4	int64x4::operator- ()				const	{ return _mm256_sub_epi64(_mm256_setzero_si256(), v); }
SIMD_FUNC int64x4	int64x4::operator+ (int64x4 b)		const	{ return _mm256_add_epi64(v, b.v); }
SIMD_FUNC int64x4	int64x4::operator- (int64x4 b)		const	{ return _mm256_sub_epi64(v, b.v); }
SIMD_FUNC auto		int64x4::operator==(int64x4 b)		const	{ return int64x4(_mm256_cmpeq_epi64(v, b.v)); }
SIMD_FUNC auto		int64x4::operator> (int64x4 b)		const	{ return int64x4(_mm256_cmpgt_epi64(v, b.v)); }
SIMD_FUNC auto		int64x4::operator< (int64x4 b)		const	{ return b > *this; }
SIMD_FUNC int64x4	shr(int64x4  a, int b)						{ return _mm256_sra_epi64(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int64x4	shl(int64x4  a, int b)						{ return _mm256_sll_epi64(a.v, _mm_cvtsi64_si128(b)); }

// uint64x4
SIMD_FUNC uint64x4::vec(uint64 x) : base(_mm256_set1_epi64x(x)) {}
SIMD_FUNC uint64x4	uint64x4::operator+ (uint64x4 b)	const	{ return _mm256_add_epi64(v, b.v); }
SIMD_FUNC uint64x4	uint64x4::operator- (uint64x4 b)	const	{ return _mm256_sub_epi64(v, b.v); }
SIMD_FUNC auto		uint64x4::operator==(uint64x4 b)	const	{ return int64x4(_mm256_cmpeq_epi64(v, b.v)); }
SIMD_FUNC auto		uint64x4::operator> (uint64x4 b)	const	{ return as<int64>(*this ^ (int64(1) << 63)) > as<int64>(b ^ (int64(1) << 63)); }
SIMD_FUNC auto		uint64x4::operator< (uint64x4 b)	const	{ return b > *this; }
SIMD_FUNC uint64x4	shr(uint64x4 a, int b)						{ return _mm256_srl_epi64(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint64x4	shl(uint64x4 a, int b)						{ return _mm256_sll_epi64(a.v, _mm_cvtsi64_si128(b)); }
#endif

#ifdef __AVX512F__
static SIMD_FUNC int64x4	_boolselect(int64x4 x, int64x4 y, boolx4 mask)	{ return _mm256_mask_mov_epi64(x.v, mask, y.v); }
static SIMD_FUNC uint64x4	_boolselect(int64x4 x, int64x4 y, boolx4 mask)	{ return _mm256_mask_mov_epi64(x.v, mask, y.v); }

SIMD_FUNC int64x2	int64x2::operator* (int64x2 b)		const	{ return _mm_mullo_epi64(v, b.v); }
SIMD_FUNC uint64x2	uint64x2::operator*(uint64x2 b)		const	{ return _mm_mullo_epi64(v, b.v); }
SIMD_FUNC int64x4	int64x4::operator* (int64x4 b)		const	{ return _mm256_mullo_epi64(v, b.v); }
SIMD_FUNC uint64x4	uint64x4::operator*(uint64x4 b)		const	{ return _mm256_mullo_epi64(v, b.v); }

// int64x8
SIMD_FUNC int64x8::vec(int64 x) : v(_mm512_set1_epi64(x)) {}
SIMD_FUNC int64x8	int64x8::operator- ()				const	{ return _mm512_sub_epi64(_mm512_setzero_si512(), v); }
SIMD_FUNC int64x8	int64x8::operator+ (int64x8 b)		const	{ return _mm512_add_epi64(v, b.v); }
SIMD_FUNC int64x8	int64x8::operator- (int64x8 b)		const	{ return _mm512_sub_epi64(v, b.v); }
SIMD_FUNC auto		int64x8::operator==(int64x8 b)		const	{ return boolx8(_mm512_cmpeq_epi64_mask(v, b.v)); }
SIMD_FUNC auto		int64x8::operator> (int64x8 b)		const	{ return boolx8(_mm512_cmpgt_epi64_mask(v, b.v)); }
SIMD_FUNC auto		int64x8::operator< (int64x8 b)		const	{ return boolx8(_mm512_cmplt_epi64_mask(v, b.v)); }
SIMD_FUNC auto		int64x8::operator!=(int64x8 b)		const	{ return boolx8(_mm512_cmpneq_epi64_mask(v, b.v)); }
SIMD_FUNC auto		int64x8::operator<=(int64x8 b)		const	{ return boolx8(_mm512_cmple_epi64_mask(v, b.v)); }
SIMD_FUNC auto		int64x8::operator>=(int64x8 b)		const	{ return boolx8(_mm512_cmpge_epi64_mask(v, b.v)); }
SIMD_FUNC int64x8	shr(int64x8 a, int b)						{ return _mm512_sra_epi64(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC int64x8	shl(int64x8 a, int b)						{ return _mm512_sll_epi64(a.v, _mm_cvtsi64_si128(b)); }
static SIMD_FUNC int64x8 _boolselect(int64x8 x, int64x8 y, boolx8 mask) { return _mm512_mask_mov_epi64(x.v, mask, y.v); }

// uint64x8
SIMD_FUNC uint64x8::vec(uint64 x) : v(_mm512_set1_epi64(x)) {}
SIMD_FUNC uint64x8	uint64x8::operator+ (uint64x8 b)	const	{ return _mm512_add_epi64(v, b.v); }
SIMD_FUNC uint64x8	uint64x8::operator- (uint64x8 b)	const	{ return _mm512_sub_epi64(v, b.v); }
SIMD_FUNC auto		uint64x8::operator==(uint64x8 b)	const	{ return boolx8(_mm512_cmpeq_epu64_mask(v, b.v)); }
SIMD_FUNC auto		uint64x8::operator> (uint64x8 b)	const	{ return boolx8(_mm512_cmpgt_epu64_mask(v, b.v)); }
SIMD_FUNC auto		uint64x8::operator< (uint64x8 b)	const	{ return boolx8(_mm512_cmplt_epu64_mask(v, b.v)); }
SIMD_FUNC auto		uint64x8::operator!=(uint64x8 b)	const	{ return boolx8(_mm512_cmpneq_epu64_mask(v, b.v)); }
SIMD_FUNC auto		uint64x8::operator<=(uint64x8 b)	const	{ return boolx8(_mm512_cmple_epu64_mask(v, b.v)); }
SIMD_FUNC auto		uint64x8::operator>=(uint64x8 b)	const	{ return boolx8(_mm512_cmpge_epu64_mask(v, b.v)); }
SIMD_FUNC uint64x8	shr(uint64x8 a, int b)						{ return _mm512_srl_epi64(a.v, _mm_cvtsi64_si128(b)); }
SIMD_FUNC uint64x8	shl(uint64x8 a, int b)						{ return _mm512_sll_epi64(a.v, _mm_cvtsi64_si128(b)); }
static SIMD_FUNC	uint64x8 _boolselect(uint64x8 x, uint64x8 y, boolx8 mask) { return _mm512_mask_mov_epi64(x.v, mask, y.v); }
#elif defined __SSE__
SIMD_FUNC int64x2	int64x2::operator* (int64x2 b)		const	{ return _mm_add_epi64(_mm_mul_epu32(v, b.v), _mm_add_epi64(_mm_mul_epu32(v, _mm_srli_si128(b.v, 4)), _mm_mul_epu32(_mm_srli_si128(v, 4), b.v))); }
SIMD_FUNC uint64x2	uint64x2::operator*(uint64x2 b)		const	{ return _mm_add_epi64(_mm_mul_epu32(v, b.v), _mm_add_epi64(_mm_mul_epu32(v, _mm_srli_si128(b.v, 4)), _mm_mul_epu32(_mm_srli_si128(v, 4), b.v))); }
SIMD_FUNC int64x4	int64x4::operator* (int64x4 b)		const	{ return _mm256_add_epi64(_mm256_mul_epu32(v, b.v), _mm256_add_epi64(_mm256_mul_epu32(v, _mm256_srli_si256(b.v, 4)), _mm256_mul_epu32(_mm256_srli_si256(v, 4), b.v))); }
SIMD_FUNC uint64x4	uint64x4::operator*(uint64x4 b)		const	{ return _mm256_add_epi64(_mm256_mul_epu32(v, b.v), _mm256_add_epi64(_mm256_mul_epu32(v, _mm256_srli_si256(b.v, 4)), _mm256_mul_epu32(_mm256_srli_si256(v, 4), b.v))); }
#endif

#ifdef __ARM_NEON__
// int64x2
template<> SIMD_FUNC int64x2::vec(int64 x) : base(vdupq_n_s64(x)) {}
SIMD_FUNC int64x2	int64x2::operator- ()				const	{ return vnegq_s64(v); }
SIMD_FUNC int64x2	int64x2::operator+ (int64x2 b)		const	{ return vaddq_s64(v, b.v); }
SIMD_FUNC int64x2	int64x2::operator- (int64x2 b)		const	{ return vsubq_s64(v, b.v); }
//SIMD_FUNC int64x2	int64x2::operator* (int64x2 b)		const	{ return vmulq_s64(v, b.v); }
SIMD_FUNC auto		int64x2::operator==(int64x2 b)		const	{ return int64x2(vceqq_s64(v, b.v)); }
SIMD_FUNC auto		int64x2::operator< (int64x2 b)		const	{ return int64x2(vcltq_s64(v, b.v)); }
SIMD_FUNC auto		int64x2::operator> (int64x2 b)		const	{ return int64x2(vcgtq_s64(v, b.v)); }
SIMD_FUNC int64x2	shr(int64x2 a, int b)						{ return vshlq_s64(a, vdupq_n_s64(-b)); }	// can use vshrq_n_s64 if b is known
SIMD_FUNC int64x2	shl(int64x2 a, int b)						{ return vshlq_s64(a, vdupq_n_s64(b)); }	// can use vshlq_n_s64 if b is known

// uint64x2
template<> SIMD_FUNC uint64x2::vec(uint64 x) : base(vdupq_n_u64(x)) {}
SIMD_FUNC uint64x2	uint64x2::operator+ (uint64x2 b)	const	{ return vaddq_u64(v, b.v); }
SIMD_FUNC uint64x2	uint64x2::operator- (uint64x2 b)	const	{ return vsubq_u64(v, b.v); }
//SIMD_FUNC uint64x2	uint64x2::operator* (uint64x2 b)	const	{ return vmulq_u64(v, b.v); }
SIMD_FUNC auto		uint64x2::operator==(uint64x2 b)	const	{ return int64x2(vceqq_u64(v, b.v)); }
SIMD_FUNC auto		uint64x2::operator> (uint64x2 b)	const	{ return int64x2(vcgtq_u64(v, b.v)); }
SIMD_FUNC uint64x2	shr(uint64x2 a, int b)						{ return vshlq_u64(a, vdupq_n_s64(-b)); }	// can use vshrq_n_u64 if b is known
SIMD_FUNC uint64x2	shl(uint64x2 a, int b)						{ return vshlq_u64(a, vdupq_n_s64(b)); }	// can use vshlq_n_u64 if b is known
#endif

//-----------------------------------------------------------------------------
//	vectors of floats
//-----------------------------------------------------------------------------

#ifdef __SSE__
// float4
SIMD_FUNC float4::vec(float x) : base(_mm_set1_ps(x)) {}
SIMD_FUNC float4	float4::operator- ()			const	{ return _mm_sub_ps(_mm_setzero_ps(), v); }
SIMD_FUNC float4	float4::operator+ (float4 b)	const	{ return _mm_add_ps(v, b.v); }
SIMD_FUNC float4	float4::operator- (float4 b)	const	{ return _mm_sub_ps(v, b.v); }
SIMD_FUNC float4	float4::operator* (float4 b)	const	{ return _mm_mul_ps(v, b.v); }
SIMD_FUNC float4	float4::operator/ (float4 b)	const	{ return _mm_div_ps(v, b.v); }
SIMD_FUNC auto		float4::operator==(float4 b)	const	{ return int32x4(_mm_castps_si128(_mm_cmpeq_ps(v, b.v))); }
SIMD_FUNC auto		float4::operator!=(float4 b)	const	{ return int32x4(_mm_castps_si128(_mm_cmpneq_ps(v, b.v))); }
SIMD_FUNC auto		float4::operator<=(float4 b)	const	{ return int32x4(_mm_castps_si128(_mm_cmple_ps(v, b.v))); }
SIMD_FUNC auto		float4::operator< (float4 b)	const	{ return int32x4(_mm_castps_si128(_mm_cmplt_ps(v, b.v))); }
SIMD_FUNC auto		float4::operator> (float4 b)	const	{ return int32x4(_mm_castps_si128(_mm_cmpgt_ps(v, b.v))); }
SIMD_FUNC auto		float4::operator>=(float4 b)	const	{ return int32x4(_mm_castps_si128(_mm_cmpge_ps(v, b.v))); }
#endif

#ifdef __AVX2__
// float8
SIMD_FUNC float8::vec(float x) : base(_mm256_set1_ps(x)) {}
SIMD_FUNC float8	float8::operator- ()			const	{ return _mm256_sub_ps(_mm256_setzero_ps(), v); }
SIMD_FUNC float8	float8::operator+ (float8 b)	const	{ return _mm256_add_ps(v, b.v); }
SIMD_FUNC float8	float8::operator- (float8 b)	const	{ return _mm256_sub_ps(v, b.v); }
SIMD_FUNC float8	float8::operator* (float8 b)	const	{ return _mm256_mul_ps(v, b.v); }
SIMD_FUNC float8	float8::operator/ (float8 b)	const	{ return _mm256_div_ps(v, b.v); }
SIMD_FUNC auto		float8::operator==(float8 b)	const	{ return int32x8(_mm256_castps_si256(_mm256_cmp_ps(v, b.v, _CMP_EQ_OQ))); }
SIMD_FUNC auto		float8::operator!=(float8 b)	const	{ return int32x8(_mm256_castps_si256(_mm256_cmp_ps(v, b.v, _CMP_NEQ_UQ))); }
SIMD_FUNC auto		float8::operator<=(float8 b)	const	{ return int32x8(_mm256_castps_si256(_mm256_cmp_ps(v, b.v, _CMP_LE_OQ))); }
SIMD_FUNC auto		float8::operator< (float8 b)	const	{ return int32x8(_mm256_castps_si256(_mm256_cmp_ps(v, b.v, _CMP_LT_OQ))); }
SIMD_FUNC auto		float8::operator> (float8 b)	const	{ return int32x8(_mm256_castps_si256(_mm256_cmp_ps(v, b.v, _CMP_GT_OQ))); }
SIMD_FUNC auto		float8::operator>=(float8 b)	const	{ return int32x8(_mm256_castps_si256(_mm256_cmp_ps(v, b.v, _CMP_GE_OQ))); }
#endif

#ifdef __AVX512F__
static SIMD_FUNC float8 _boolselect(float8 x, float8 y, boolx8 mask) { return _mm256_mask_mov_ps(x.v, mask, y.v); }

// floatx16
SIMD_FUNC floatx16::vec(float x) : base(_mm512_set1_ps(x)) {}
SIMD_FUNC floatx16	floatx16::operator- ()				const	{ return _mm512_sub_ps(_mm512_setzero_ps(), v); }
SIMD_FUNC floatx16	floatx16::operator+ (floatx16 b)	const	{ return _mm512_add_ps(v, b.v); }
SIMD_FUNC floatx16	floatx16::operator- (floatx16 b)	const	{ return _mm512_sub_ps(v, b.v); }
SIMD_FUNC floatx16	floatx16::operator* (floatx16 b)	const	{ return _mm512_mul_ps(v, b.v); }
SIMD_FUNC floatx16	floatx16::operator/ (floatx16 b)	const	{ return _mm512_div_ps(v, b.v); }
SIMD_FUNC auto		floatx16::operator==(floatx16 b)	const	{ return boolx16(_mm512_cmp_ps_mask(v, b.v, _CMP_EQ_OQ)); }
SIMD_FUNC auto		floatx16::operator!=(floatx16 b)	const	{ return boolx16(_mm512_cmp_ps_mask(v, b.v, _CMP_NEQ_UQ)); }
SIMD_FUNC auto		floatx16::operator<=(floatx16 b)	const	{ return boolx16(_mm512_cmp_ps_mask(v, b.v, _CMP_LE_OQ)); }
SIMD_FUNC auto		floatx16::operator< (floatx16 b)	const	{ return boolx16(_mm512_cmp_ps_mask(v, b.v, _CMP_LT_OQ)); }
SIMD_FUNC auto		floatx16::operator> (floatx16 b)	const	{ return boolx16(_mm512_cmp_ps_mask(v, b.v, _CMP_GT_OQ)); }
SIMD_FUNC auto		floatx16::operator>=(floatx16 b)	const	{ return boolx16(_mm512_cmp_ps_mask(v, b.v, _CMP_GE_OQ)); }
static SIMD_FUNC floatx16 _boolselect(floatx16 x, floatx16 y, boolx16 mask) { return _mm512_mask_mov_ps(x.v, mask, y.v); }
#endif

#ifdef __ARM_NEON__
// float2
SIMD_FUNC float2::vec(float x) : base(vdup_n_f32(x)) {}
SIMD_FUNC float2	float2::operator- ()			const	{ return vneg_f32(v); }
SIMD_FUNC float2	float2::operator+ (float2 b)	const	{ return vadd_f32(v, b.v); }
SIMD_FUNC float2	float2::operator- (float2 b)	const	{ return vsub_f32(v, b.v); }
SIMD_FUNC float2	float2::operator* (float2 b)	const	{ return vmul_f32(v, b.v); }
SIMD_FUNC float2	float2::operator/ (float2 b)	const	{ return vdiv_f32(v, b.v); }
SIMD_FUNC auto		float2::operator==(float2 b)	const	{ return int32x2(vceq_f32(v, b.v)); }
SIMD_FUNC auto		float2::operator<=(float2 b)	const	{ return int32x2(vcle_f32(v, b.v)); }
SIMD_FUNC auto		float2::operator< (float2 b)	const	{ return int32x2(vclt_f32(v, b.v)); }
SIMD_FUNC auto		float2::operator> (float2 b)	const	{ return int32x2(vcgt_f32(v, b.v)); }
SIMD_FUNC auto		float2::operator>=(float2 b)	const	{ return int32x2(vcge_f32(v, b.v)); }

// float4
SIMD_FUNC float4::vec(float x) : base(vdupq_n_f32(x)) {}
SIMD_FUNC float4	float4::operator- ()			const	{ return vnegq_f32(v); }
SIMD_FUNC float4	float4::operator+ (float4 b)	const	{ return vaddq_f32(v, b.v); }
SIMD_FUNC float4	float4::operator- (float4 b)	const	{ return vsubq_f32(v, b.v); }
SIMD_FUNC float4	float4::operator* (float4 b)	const	{ return vmulq_f32(v, b.v); }
SIMD_FUNC float4	float4::operator/ (float4 b)	const	{ return vdivq_f32(v, b.v); }
SIMD_FUNC auto		float4::operator==(float4 b)	const	{ return int32x4(vceqq_f32(v, b.v)); }
SIMD_FUNC auto		float4::operator<=(float4 b)	const	{ return int32x4(vcleq_f32(v, b.v)); }
SIMD_FUNC auto		float4::operator< (float4 b)	const	{ return int32x4(vcltq_f32(v, b.v)); }
SIMD_FUNC auto		float4::operator> (float4 b)	const	{ return int32x4(vcgtq_f32(v, b.v)); }
SIMD_FUNC auto		float4::operator>=(float4 b)	const	{ return int32x4(vcgeq_f32(v, b.v)); }
#endif

//-----------------------------------------------------------------------------
//	vectors of doubles
//-----------------------------------------------------------------------------

#ifdef __SSE__
// double2
SIMD_FUNC double2::vec(double x) : base(_mm_set1_pd(x)) {}
SIMD_FUNC double2	double2::operator- ()			const	{ return _mm_sub_pd(_mm_setzero_pd(), v); }
SIMD_FUNC double2	double2::operator+ (double2 b)	const	{ return _mm_add_pd(v, b.v); }
SIMD_FUNC double2	double2::operator- (double2 b)	const	{ return _mm_sub_pd(v, b.v); }
SIMD_FUNC double2	double2::operator* (double2 b)	const	{ return _mm_mul_pd(v, b.v); }
SIMD_FUNC double2	double2::operator/ (double2 b)	const	{ return _mm_div_pd(v, b.v); }
SIMD_FUNC auto		double2::operator==(double2 b)	const	{ return int64x2(_mm_castpd_si128(_mm_cmpeq_pd(v, b.v))); }
SIMD_FUNC auto		double2::operator!=(double2 b)	const	{ return int64x2(_mm_castpd_si128(_mm_cmpneq_pd(v, b.v))); }
SIMD_FUNC auto		double2::operator<=(double2 b)	const	{ return int64x2(_mm_castpd_si128(_mm_cmple_pd(v, b.v))); }
SIMD_FUNC auto		double2::operator< (double2 b)	const	{ return int64x2(_mm_castpd_si128(_mm_cmplt_pd(v, b.v))); }
SIMD_FUNC auto		double2::operator> (double2 b)	const	{ return int64x2(_mm_castpd_si128(_mm_cmpgt_pd(v, b.v))); }
SIMD_FUNC auto		double2::operator>=(double2 b)	const	{ return int64x2(_mm_castpd_si128(_mm_cmpge_pd(v, b.v))); }
#endif

#ifdef __AVX2__
// double4
SIMD_FUNC double4::vec(double x) : base(_mm256_set1_pd(x)) {}
SIMD_FUNC double4	double4::operator- ()			const	{ return _mm256_sub_pd(_mm256_setzero_pd(), v); }
SIMD_FUNC double4	double4::operator+ (double4 b)	const	{ return _mm256_add_pd(v, b.v); }
SIMD_FUNC double4	double4::operator- (double4 b)	const	{ return _mm256_sub_pd(v, b.v); }
SIMD_FUNC double4	double4::operator* (double4 b)	const	{ return _mm256_mul_pd(v, b.v); }
SIMD_FUNC double4	double4::operator/ (double4 b)	const	{ return _mm256_div_pd(v, b.v); }
SIMD_FUNC auto		double4::operator==(double4 b)	const	{ return int64x4(_mm256_castpd_si256(_mm256_cmp_pd(v, b.v, _CMP_EQ_OQ))); }
SIMD_FUNC auto		double4::operator!=(double4 b)	const	{ return int64x4(_mm256_castpd_si256(_mm256_cmp_pd(v, b.v, _CMP_NEQ_UQ))); }
SIMD_FUNC auto		double4::operator<=(double4 b)	const	{ return int64x4(_mm256_castpd_si256(_mm256_cmp_pd(v, b.v, _CMP_LE_OQ))); }
SIMD_FUNC auto		double4::operator< (double4 b)	const	{ return int64x4(_mm256_castpd_si256(_mm256_cmp_pd(v, b.v, _CMP_LT_OQ))); }
SIMD_FUNC auto		double4::operator> (double4 b)	const	{ return int64x4(_mm256_castpd_si256(_mm256_cmp_pd(v, b.v, _CMP_GT_OQ))); }
SIMD_FUNC auto		double4::operator>=(double4 b)	const	{ return int64x4(_mm256_castpd_si256(_mm256_cmp_pd(v, b.v, _CMP_GE_OQ))); }
#endif

#ifdef __AVX512F__
static SIMD_FUNC double4 _boolselect(double4 x, double4 y, boolx4 mask) { return _mm256_mask_mov_pd(x.v, mask, y.v); }

// double8
SIMD_FUNC double8::vec(double x) : base(_mm512_set1_pd(x)) {}
SIMD_FUNC double8	double8::operator- ()			const	{ return _mm512_sub_pd(_mm512_setzero_pd(), v); }
SIMD_FUNC double8	double8::operator+ (double8 b)	const	{ return _mm512_add_pd(v, b.v); }
SIMD_FUNC double8	double8::operator- (double8 b)	const	{ return _mm512_sub_pd(v, b.v); }
SIMD_FUNC double8	double8::operator* (double8 b)	const	{ return _mm512_mul_pd(v, b.v); }
SIMD_FUNC double8	double8::operator/ (double8 b)	const	{ return _mm512_div_pd(v, b.v); }
SIMD_FUNC auto		double8::operator==(double8 b)	const	{ return boolx8(_mm512_cmp_pd_mask(v, b.v, _CMP_EQ_OQ)); }
SIMD_FUNC auto		double8::operator!=(double8 b)	const	{ return boolx8(_mm512_cmp_pd_mask(v, b.v, _CMP_NEQ_UQ)); }
SIMD_FUNC auto		double8::operator<=(double8 b)	const	{ return boolx8(_mm512_cmp_pd_mask(v, b.v, _CMP_LE_OQ)); }
SIMD_FUNC auto		double8::operator< (double8 b)	const	{ return boolx8(_mm512_cmp_pd_mask(v, b.v, _CMP_LT_OQ)); }
SIMD_FUNC auto		double8::operator> (double8 b)	const	{ return boolx8(_mm512_cmp_pd_mask(v, b.v, _CMP_GT_OQ)); }
SIMD_FUNC auto		double8::operator>=(double8 b)	const	{ return boolx8(_mm512_cmp_pd_mask(v, b.v, _CMP_GE_OQ)); }
static SIMD_FUNC double8 _boolselect(double8 x, double8 y, boolx8 mask) { return _mm512_mask_mov_pd(x.v, mask, y.v); }
#endif

#ifdef __ARM_NEON__
// double4
SIMD_FUNC double2::vec(double x) : base(vdupq_n_f64(x)) {}
SIMD_FUNC double2	double2::operator- ()			const	{ return vnegq_f64(v); }
SIMD_FUNC double2	double2::operator+ (double2 b)	const	{ return vaddq_f64(v, b.v); }
SIMD_FUNC double2	double2::operator- (double2 b)	const	{ return vsubq_f64(v, b.v); }
SIMD_FUNC double2	double2::operator* (double2 b)	const	{ return vmulq_f64(v, b.v); }
SIMD_FUNC double2	double2::operator/ (double2 b)	const	{ return vdivq_f64(v, b.v); }
SIMD_FUNC auto		double2::operator==(double2 b)	const	{ return int64x2(vceqq_f64(v, b.v)); }
SIMD_FUNC auto		double2::operator<=(double2 b)	const	{ return int64x2(vcleq_f64(v, b.v)); }
SIMD_FUNC auto		double2::operator< (double2 b)	const	{ return int64x2(vcltq_f64(v, b.v)); }
SIMD_FUNC auto		double2::operator> (double2 b)	const	{ return int64x2(vcgtq_f64(v, b.v)); }
SIMD_FUNC auto		double2::operator>=(double2 b)	const	{ return int64x2(vcgeq_f64(v, b.v)); }
#endif

} // namespace simd

template<typename K, typename T, int N> struct constant_cast<K, simd::vec<T, N> >	{ static const simd::vec<T, N> f() { return K::template as<T>(); } };
template<typename E, int N>		constexpr bool use_constants<simd::vec<E,N>>		= true;
template<typename E, int...I>	constexpr bool use_constants<simd::perm<E,I...>>	= true;

} // namespace iso

#endif
