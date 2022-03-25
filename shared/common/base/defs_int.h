#ifndef DEFS_INT_H
#define DEFS_INT_H

#include "defs_base.h"

#if defined __ARM_NEON__
# include <arm_neon.h>
#elif defined(_M_IX86) || defined(_M_X64)
#include <immintrin.h>
#endif


namespace iso {

template<typename T, int N, bool = (N < BIT_COUNT<T>)> constexpr T kbit = T(1) << N;
template<typename T, int N>				constexpr T kbit<T,N,false>		= 0;
template<typename T, int N, int S = 0>	constexpr T kbits				= kbit<T, S + N> - kbit<T, S>;

//-----------------------------------------------------------------------------
//	constants
//-----------------------------------------------------------------------------
template<int N>	struct __int	{ template<typename R> static constexpr R as() { return (R)N; } };
template<int N> struct constant_type<__int<N>>	{ typedef int type; };
template<int N> using constant_int	= constant<__int<N>>;
typedef constant_int<0>				_zero;
typedef constant_int<1>				_one, _identity;

extern _zero						zero;
extern _one							one, identity;

//-----------------------------------------------------------------------------
//	user-defined literals
//-----------------------------------------------------------------------------

constexpr uint8		operator"" _u8(unsigned long long int x)	{ return uint8(x); }
constexpr uint16	operator"" _u16(unsigned long long int x)	{ return uint16(x); }
constexpr uint32	operator"" _u32(unsigned long long int x)	{ return uint32(x); }
constexpr uint64	operator"" _u64(unsigned long long int x)	{ return uint64(x); }

#ifdef ISO_BIGENDIAN
constexpr uint32	operator"" _u16(const char* s, size_t len)	{ return s[1] + (s[0] << 8); }
constexpr uint32	operator"" _u32(const char* s, size_t len)	{ return s[3] + (s[2] << 8) + (s[1] << 16) + (s[0] << 24); }
constexpr uint64	operator"" _u64(const char* s, size_t len)	{ return (uint64(operator""_u32(s, len)) << 32) + operator""_u32(s + 4, len - 4); }
#else
constexpr uint32	operator"" _u16(const char* s, size_t len)	{ return s[0] + (s[1] << 8); }
constexpr uint32	operator"" _u32(const char* s, size_t len)	{ return s[0] + (s[1] << 8) + (s[2] << 16) + (s[3] << 24); }
constexpr uint64	operator"" _u64(const char* s, size_t len)	{ return operator""_u32(s, 4) + (uint64(operator""_u32(s + 4, len - 4)) << 32); }
#endif

template<int B, uint64 I, char... C>					struct digits_reader;
template<int B, uint64 I, char C0, char... C>			struct digits_reader<B, I, C0, C...> : digits_reader<B, I * B + (C0 > '9' ? (C0 + 9) & 31 : C0 & 15), C...> {};
template<int B, uint64 I>								struct digits_reader<B, I>			{ static const uint64 value = I; };
template<char... C>	struct int_reader					: digits_reader<10, 0, C...> {};
template<char... C>	struct int_reader<'0', C...>		: digits_reader<8,  0, C...> {};
template<char... C>	struct int_reader<'0', 'x', C...>	: digits_reader<16, 0, C...> {};
template<char... C>	struct int_reader<'0', 'X', C...>	: digits_reader<16, 0, C...> {};
template<char... C>	struct int_reader<'0', 'b', C...>	: digits_reader<2,  0, C...> {};
template<char... C>	struct int_reader<'0', 'B', C...>	: digits_reader<2,  0, C...> {};

template<uint64 L, uint64 H>							struct digits_result128 {};
template<int B, uint64 L, uint64 H, char... C>			struct digits_reader128;
template<int B, uint64 L, uint64 H, char C0, char... C>	struct digits_reader128<B, L, H, C0, C...>	: digits_reader128<B, L * B + (C0 > '9' ? (C0 + 9) & 31 : C0 & 15), H * B + (((L>>32) * B)>>32), C...> {};
template<int B, uint64 L, uint64 H>						struct digits_reader128<B, L, H>			: digits_result128<L, H>	{};
template<char... C>	struct int_reader128					: digits_reader128<10, 0, 0, C...> {};
template<char... C>	struct int_reader128<'0', C...>			: digits_reader128<8,  0, 0, C...> {};
template<char... C>	struct int_reader128<'0', 'x', C...>	: digits_reader128<16, 0, 0, C...> {};
template<char... C>	struct int_reader128<'0', 'X', C...>	: digits_reader128<16, 0, 0, C...> {};
template<char... C>	struct int_reader128<'0', 'b', C...>	: digits_reader128<2,  0, 0, C...> {};
template<char... C>	struct int_reader128<'0', 'B', C...>	: digits_reader128<2,  0, 0, C...> {};

template<char... C> auto	operator"" _k()	{ return constant_int<int_reader<C...>::value>(); }
template<int K>		auto	literal_int = constant_int<K>();

//-----------------------------------------------------------------------------
//	compile-time logs and powers
//-----------------------------------------------------------------------------

template<int I, bool = ((I & (I - 1)) == 0)>	constexpr int pow2_ceil = pow2_ceil<I + (I & -I)>;
template<int I>	constexpr int pow2_ceil<I, true> = I;

template<int B, int N> 		constexpr uint64 kpow 		= B * kpow<B, N - 1>;
template<int B> 			constexpr uint64 kpow<B, 0>	= 1;
#define POW(B, N)		kpow<B, N>

template<uint64 X, int N=32>constexpr int klog2			= (X >> N) ? N + klog2<(X >> N), N / 2> : klog2<X, N / 2>;
template<uint64 X>			constexpr int klog2<X, 0>	= 0;
#define LOG2(X)			klog2<X>
#define LOG2_CEIL(X)	klog2<X - 1> + 1

template<typename T> constexpr size_t log2alignment	= klog2<alignof(T)>;

template<uint64 B, uint64 N, bool OK = (N / B >= B)> struct _LOG_BASE {
	typedef _LOG_BASE<B * B, N> P;
	static const uint64
		value		= P::value * 2 + (P::remainder >= B),
		remainder	= P::remainder >= B ? P::remainder / B : P::remainder;
};
template<uint64 B, uint64 N> struct _LOG_BASE<B, N, false> {
	static const uint64
		value		= N >= B,
		remainder	= N >= B ? N / B : N;
};
template<uint64 B, uint64 N>	constexpr uint64	klog		= _LOG_BASE<B,N>::value;
template<uint64 B, uint64 N>	constexpr uint64	klog_ceil	= _LOG_BASE<B,N>::value + (_LOG_BASE<B,N>::remainder != 0);

// fractional part of log
template<uint64 B, uint64 N, uint64 D, int BITS> struct _FLOG_BASE {
	static constexpr uint64 shift_sq(uint64 x, int s) { return s < 0 ? (x * x) >> -s : (x * x) << s; }
	enum {
		test	= N >= B * D,
		s		= 30 - klog2<N> * 2,
		value	= _FLOG_BASE<B, shift_sq(N, s), shift_sq(test ? D * B : D, s), BITS - 1>::value | (test << BITS),
	};
};
template<uint64 B, uint64 N, uint64 D> struct _FLOG_BASE<B, N, D, 0> { enum { value = N >= B * D }; };
#define FLOG_BASE(B, N, BITS)	_FLOG_BASE<B,N,1,BITS>::value

template<uint64 B, uint64 N>	constexpr uint64	klog_round	= (_FLOG_BASE<B,N,1,1>::value + 1) / 2;

template<typename T> constexpr T constexpr_gcd1(T a, T b)	{ return b % a ? constexpr_gcd1(b % a, a) : a; }
template<typename T> constexpr T constexpr_gcd(T a, T b) 	{ return a > b ? constexpr_gcd1(b, a) : constexpr_gcd1(a, b); }

//-----------------------------------------------------------------------------
//	integer types
//-----------------------------------------------------------------------------

template<typename T>	constexpr T	ones = T(~T(0));

template<int BYTES>		struct T_sint_type;
template<>				struct T_sint_type<1>	: T_type<int8>		{};
template<>				struct T_sint_type<2>	: T_type<int16>		{};
template<>				struct T_sint_type<4>	: T_type<int32>		{};
template<>				struct T_sint_type<8>	: T_type<int64>		{};
template<int BYTES>		using sint_t			= typename T_sint_type<pow2_ceil<BYTES>>::type;
template<int BITS>		using sint_bits_t		= sint_t<(BITS + 7) / 8>;
template<typename T>	using sint_for_t		= sint_t<sizeof(T)>;
template<typename T>	constexpr sint_for_t<T>	as_signed(T x)	{ return (sint_for_t<T>&)x; }

template<int BYTES>		struct T_uint_type;
template<>				struct T_uint_type<1>	: T_type<uint8>		{};
template<>				struct T_uint_type<2>	: T_type<uint16>	{};
template<>				struct T_uint_type<4>	: T_type<uint32>	{};
template<>				struct T_uint_type<8>	: T_type<uint64>	{};
template<int BYTES>		using uint_t			= typename T_uint_type<pow2_ceil<BYTES>>::type;
template<int BITS>		using uint_bits_t		= uint_t<(BITS + 7) / 8>;
template<typename T>	using uint_for_t		= uint_t<sizeof(T)>;
template<typename T>	constexpr uint_for_t<T>	as_unsigned(T x)	{ return (uint_for_t<T>&)x; }
template<typename T, typename TMIN>	using	uint_type_tmin	= uint_t<meta::max<sizeof(T), sizeof(TMIN)>>;

template<typename T, bool S, typename=void> struct T_signed	: T_type<T> {};
template<> struct T_signed<uint8,  true>		: T_type<int8>		{};
template<> struct T_signed<uint16, true>		: T_type<int16>		{};
template<> struct T_signed<uint32, true>		: T_type<int32>		{};
template<> struct T_signed<uint64, true>		: T_type<int64>		{};
template<> struct T_signed<int8,  false>		: T_type<uint8>		{};
template<> struct T_signed<int16, false>		: T_type<uint16>	{};
template<> struct T_signed<int32, false>		: T_type<uint32>	{};
template<> struct T_signed<int64, false>		: T_type<uint64>	{};

template<typename T, bool S=true>	using signed_t		= typename T_signed<T, S>::type;
template<typename T>				using unsigned_t	= typename T_signed<T, false>::type;
template<typename T>	auto	make_unsigned(T t)	{ return unsigned_t<T>(t); }

template<int BYTES, bool S>	struct T_int_type				: T_sint_type<BYTES> {};
template<int BYTES>			struct T_int_type<BYTES, false>	: T_uint_type<BYTES> {};
template<int BYTES, bool S>	using int_t			= typename T_int_type<pow2_ceil<BYTES>, S>::type;
template<int BITS, bool S>	using int_bits_t	= int_t<(BITS + 7) / 8, S>;

template<typename T, bool S = is_signed<T>> struct absint {
	uint_for_t<T>	u;
	bool			neg;
	absint(T t) : u(t < 0 ? -t : t), neg(t < 0) {}
};

template<typename T> struct absint<T, false> {
	T				u;
	static const bool neg = false;
	absint(T t) : u(t) {}
};

//-----------------------------------------------------------------------------
//	baseint - associate a number base with an int for printing
//-----------------------------------------------------------------------------

template<int B, typename T> struct baseint {
	enum {digits = sizeof(T) * 32 / klog2<B*B*B*B> + 1};
	T	i;
	operator T()	const	{ return i; }
	template<typename T2> baseint&	operator= (T2 j)	{ i  = (T)j; return *this;	}
	template<typename T2> baseint&	operator+=(T2 j)	{ i += (T)j; return *this;	}
	template<typename T2> baseint&	operator-=(T2 j)	{ i -= (T)j; return *this;	}
	template<typename T2> baseint&	operator*=(T2 j)	{ i *= (T)j; return *this;	}
	template<typename T2> baseint&	operator/=(T2 j)	{ i /= (T)j; return *this;	}
	template<typename T2> baseint&	operator|=(T2 j)	{ i |= (T)j; return *this;	}
	template<typename T2> baseint&	operator&=(T2 j)	{ i &= (T)j; return *this;	}
	template<typename T2> baseint&	operator^=(T2 j)	{ i ^= (T)j; return *this;	}
	friend T	get(const baseint &a)	{ return a; }
};
template<int B, typename T> struct T_underlying<baseint<B, T> > : T_underlying<T> {};
template<int B, typename T> struct T_isint<baseint<B, T> >		: T_isint<T> {};

typedef constructable<baseint<16,uint8> >	xint8;
typedef constructable<baseint<16,uint16> >	xint16;
typedef constructable<baseint<16,uint32> >	xint32;
typedef constructable<baseint<16,uint64> >	xint64;

template<typename T> inline constructable<baseint<16, typename T_underlying<T>::type> >	hex(T t)	{ return (typename T_underlying<T>::type)t; }
template<typename T> inline constructable<baseint<8,  typename T_underlying<T>::type> >	oct(T t)	{ return get(t); }
template<typename T> inline constructable<baseint<2,  typename T_underlying<T>::type> >	bin(T t)	{ return get(t); }

//-----------------------------------------------------------------------------
//	rawint - to prevent endian swapping
//-----------------------------------------------------------------------------

template<typename T> struct rawint {
	T	i;
	rawint()			{}
	rawint(T i) : i(i)	{}
	operator noarray_t<T>()	const	{ return i; }
};
typedef rawint<uint8>	rint8;
typedef rawint<uint16>	rint16;
typedef rawint<uint32>	rint32;
typedef rawint<uint64>	rint64;

template<int B, typename T> struct num_traits<baseint<B,T> > : num_traits<T> {};

//-----------------------------------------------------------------------------
//	arbitrary length unsigned integers (always unaligned)
//-----------------------------------------------------------------------------

template<int N, bool BE=iso_bigendian> class uintn {
	typedef uint_t<N>	I;
	uint8	a[N];
	force_inline uint8					low1()	const	{ return a[BE ? N - 1 : 0]; }
	force_inline uint8					&low1()			{ return a[BE ? N - 1 : 0]; }
	force_inline const uintn<N-1, BE>	&high()	const	{ return (uintn<N-1, BE>&)a[1 - BE]; }
	force_inline uintn<N-1, BE>			&high()			{ return (uintn<N-1, BE>&)a[1 - BE]; }

	force_inline uint8					high1()	const	{ return a[BE ? 0 : N - 1]; }
	force_inline uint8					&high1()		{ return a[BE ? 0 : N - 1]; }
	force_inline const uintn<N-1, BE>	&low()	const	{ return (uintn<N-1, BE>&)a[BE]; }
	force_inline uintn<N-1, BE>			&low()			{ return (uintn<N-1, BE>&)a[BE]; }
public:
	uintn()	{}
	template<typename J> constexpr uintn(J x)	{ low1() = uint8(x); high() = x >> 8; }
	template<typename J> void operator=(J x)	{ low1() = uint8(x); high() = x >> 8; }
	force_inline operator I() const	{ return low1() | (high() << 8); }
	friend I	get(const uintn& u)	{ return u; }
	friend I	put(uintn& u)		{ return u; }
};

template<bool BE> class uintn<1,BE> {
	uint8		a;
public:
	constexpr uintn() {}
	template<typename J> constexpr uintn(J x) : a(uint8(x)) {}
	template<typename J> void operator=(J x)	{ a = uint8(x);	}
	operator uint8() const		{ return a;	}
	friend uint8	get(const uintn& u) { return u; }
	friend uint8	put(uintn& u)		{ return u; }
};

template<int N, bool BE = iso_bigendian> class _intn : uintn<N, BE> {
	typedef uintn<N, BE>					B;
	typedef typename T_sint_type<N>::type	I;
	template<typename T> static constexpr signed_t<T>	sign_extend(T x)	{ return x - ((x << 1) & (T(1) << (N * 8))); }
public:
	force_inline void operator=(I x)	{ B::operator=(x); }
	force_inline operator I() const	{ return sign_extend(B::operator I()); }
};

template<int N, bool BE> auto	swap_endian(const uintn<N,BE> &u) {
	uintn<N, !BE>	r;
	for (int i = 0; i < N; i++)
		((uint8*)&r)[i] = ((uint8*)&u)[N - 1 - i];
	return r;
}

template<int N, bool BE> struct T_swap_endian_type<uintn<N,BE> >	: T_type<uintn<N,!BE>> {};
template<int N, bool BE> struct T_underlying<uintn<N,BE> >			: T_type<uint_t<N>> {};
template<int N, bool BE> struct T_underlying<_intn<N,BE> >			: T_type<sint_t<N>> {};
//template<int N, bool BE=iso_bigendian> using uintn = constructable<uintn<N,BE>>;
//template<int N, bool BE=iso_bigendian> using intn  = constructable<_intn<N,BE>>;

template<int N, bool BE> struct T_isint<uintn<N,BE>> : T_true {};

typedef uintn<3, false>	uint24le;
typedef uintn<3, true>	uint24be;

//-----------------------------------------------------------------------------
// simple bit stuff (more in bits.h)
//-----------------------------------------------------------------------------

template<typename T> constexpr T	bit(uint32 n)					{ return n < BIT_COUNT<T> ? T(1) << n : 0; }
template<typename T> constexpr T	bits(uint32 n, uint32 s = 0)	{ return bit<T>(s + n) - bit<T>(s); }

constexpr uint32					bit(uint32 n)					{ return bit<uint32>(n); }
constexpr uint64					bit64(uint32 n)					{ return bit<uint64>(n); }
constexpr uint32					bits(uint32 n, uint32 s = 0)	{ return bits<uint32>(n, s); }
constexpr uint64					bits64(uint32 n, uint32 s = 0)	{ return bits<uint64>(n, s); }

template<int I> struct highedge_s {
	template<typename T> static constexpr T f(T t) { return highedge_s<I / 2>::f(t | (t >> (I / 2))); }
};
template<> struct highedge_s<1> {
	template<typename T> static constexpr T f(T t) { return t; }
};
template<typename T> constexpr auto	_highedge(T x)					{ return highedge_s<BIT_COUNT<T>>::f(as_unsigned(x)); }
template<typename T> constexpr T	_highedge_to_bit(T x)			{ return  x - (x >> 1); }

template<typename T> constexpr T	lowest_set(T x)					{ return x & -x; }
template<typename T> constexpr T	highest_set(T x)				{ return _highedge_to_bit(_highedge(x));}
template<typename T> constexpr T	clear_lowest(T x)				{ return x & (x - 1); }
template<typename T> constexpr T	clear_highest(T x)				{ return x & _highedge(x >> 1); }

template<typename T> constexpr T	lowest_clear(T x)				{ return lowest_set(~x); }
template<typename T> constexpr T	highest_clear(T x)				{ return highest_set(~x);}
template<typename T> constexpr T	set_lowest(T x)					{ return ~clear_lowest(~x); }
template<typename T> constexpr T	set_highest(T x)				{ return ~clear_highest(~x); }

constexpr uint32					highest_set2(uint8 x)			{ return min(x, 2);}
constexpr uint32					highest_set4(uint8 x)			{ return _highedge_to_bit(highedge_s<4>::f(x)); }
constexpr int						lowest_set_index4(uint8 x)		{ return x & 3 ? (x + 1) & 1 : x & 12 ? (x & 4 ? 2 : 3) : -1; }
constexpr int						lowest_clear_index4(uint8 x)	{ return lowest_set_index4(x ^ 15); }
constexpr int						highest_set_index4(uint8 x)		{ return x & 12 ? (x >> 2) + 1 : x & 3 ? x - 1 : -1; }
constexpr int						highest_clear_index4(uint8 x)	{ return highest_set_index4(x ^ 15); }

template<typename T> constexpr uint32 _count_bits2(T i) { return (((i + (i >> 4)) & (T(~T(0)) / 0x11)) * (T(~T(0)) / 0xff)) >> (BIT_COUNT<T> - 8); }
template<typename T> constexpr uint32 _count_bits1(T i) { return _count_bits2<T>((i & (T(~T(0)) / 5)) + ((i >> 2) & (T(~T(0)) / 5))); }
template<uint64 X> constexpr uint32 count_bits_v	= _count_bits1(X - ((X >> 1) & (~uint64(0) / 3)));

#if defined(__GNUC__) || defined PLAT_CLANG
//------------------------------------
//	gcc/clang
//------------------------------------

typedef __uint128_t	uint128;
typedef __int128_t	int128;

constexpr int						leading_zeros(uint128 x);
constexpr int						highest_set_index(uint128 x);
constexpr int						lowest_set_index(uint128 x);

constexpr int 						count_bits(uint32 x) 			{ return __builtin_popcount(x); }
constexpr int 						count_bits(uint64 x) 			{ return __builtin_popcountll(x); }
template<typename T> constexpr int	count_bits(T x)					{ return count_bits(uint_type_tmin<T, int>(x)); }

constexpr int						leading_zeros(uint8 x)			{ return x ? __builtin_clz(x) - 24 : 8; }
constexpr int						leading_zeros(uint16 x)			{ return x ? __builtin_clz(x) - 16 : 16; }
constexpr int						leading_zeros(uint32 x)			{ return x ? __builtin_clz(x) : 32; }

#ifdef PLAT_PS3
constexpr int						leading_zeros(uint64 x)			{ return (x >> 32) ? leading_zeros(uint32(x >> 32)) : leading_zeros(uint32(x)) + 32; }
template<typename T> constexpr int	lowest_set_index(T x)			{ return BIT_COUNT<T> - 1 - leading_zeros(x & -x);	}
#else
constexpr int						leading_zeros(uint64 x)			{ return x ? __builtin_clzll(x) : 64; }
template<typename T> constexpr int	lowest_set_index(T x)			{ return __builtin_ffs(x) - 1; }
constexpr int						lowest_set_index(uint64 x)		{ return __builtin_ffsll(x) - 1; }
#endif

template<typename T> constexpr enable_if_t<!same_v<uint_for_t<T>, T>, int> leading_zeros(T x)	{ return leading_zeros(uint_for_t<T>(x)); }
template<typename T> constexpr int	highest_set_index(T x)			{ return BIT_COUNT<T> - 1 - leading_zeros(x); }

#elif defined(PLAT_X360)
//------------------------------------
//	X360
//------------------------------------

constexpr int						leading_zeros(uint8 x)			{ return _CountLeadingZeros(x) - 24; }
constexpr int						leading_zeros(uint16 x)			{ return _CountLeadingZeros(x) - 16; }
constexpr int						leading_zeros(uint32 x)			{ return _CountLeadingZeros(x); }
constexpr int						leading_zeros(uint64 x)			{ return _CountLeadingZeros64(x); }
template<typename T> constexpr int	leading_zeros(T x)				{ return leading_zeros(uint_for<T>(x)); }
template<typename T> constexpr int	highest_set_index(T x)			{ return BIT_COUNT<T> - 1 - leading_zeros(x); }
template<typename T> constexpr int	lowest_set_index(T x)			{ return highest_set_index(x & -x); }

#elif defined(_M_IX86) || defined(_M_X64)

//------------------------------------
//	MSVC
//------------------------------------
force_inline int 					count_bits(uint16 x) 			{ return __popcnt16(x); }
force_inline int 					count_bits(uint32 x) 			{ return __popcnt(x); }
force_inline int 					count_bits(uint64 x) 			{ return __popcnt64(x); }
template<typename T> force_inline int count_bits(T x)				{ return count_bits(uint_type_tmin<T, int16>(x)); }

inline int							lowest_set_index(uint32 x)		{ DWORD i; return _BitScanForward(&i, x) ? i : -1;	}
inline int							highest_set_index(uint32 x)		{ DWORD i; return _BitScanReverse(&i, x) ? i : -1;	}

#if defined(_M_IA64) || defined(_M_X64)
inline int							lowest_set_index(uint64 x)		{ DWORD i; return _BitScanForward64(&i, x) ? i : -1;}
inline int							highest_set_index(uint64 x)		{ DWORD i; return _BitScanReverse64(&i, x) ? i : -1;}
#else
inline int							lowest_set_index(uint64 x)		{ DWORD i; return _BitScanForward(&i, uint32(x)) ? i : _BitScanForward(&i, uint32(x>>32)) ? i + 32 : -1;	}
inline int							highest_set_index(uint64 x)		{ DWORD i; return _BitScanReverse(&i, uint32(x>>32)) ? i + 32 : _BitScanReverse(&i, uint32(x)) ? i : -1;	}
#endif

template<typename T> inline int		lowest_set_index(T x)			{ return lowest_set_index(uint_type_tmin<T, int>(x)); }
template<typename T> inline int		highest_set_index(T x)			{ return highest_set_index(uint_type_tmin<T, int>(x)); }
template<typename T> constexpr int	leading_zeros(T x)				{ return BIT_COUNT<T> - 1 - highest_set_index(x); }

#else
//------------------------------------
//generic
//------------------------------------

template<typename T> constexpr enable_if_t<!is_signed<T>, uint32> count_bits(T i)	{ return _count_bits1<T>(i - ((i >> 1) & (T(~T(0)) / 3))); }
template<typename T> constexpr enable_if_t< is_signed<T>, uint32> count_bits(T i)	{ return count_bits(uint_for_t<T>(i)); }

constexpr int	lowest_set_index(uint32 x)	{
	constexpr const int8 table[32] = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
	return table[((uint32)((x & -x) * 0x077CB531U)) >> 27];
}

template<typename T> constexpr int	highest_set_index(T x) 	{ return lowest_set_index(_highedge(x) + 1) - 1; }
template<typename T> constexpr int	leading_zeros(T x) 		{ return BIT_COUNT<T> - 1 - highest_set_index(x); }

#endif

template<typename T> constexpr int num_zero_bytes(T x) {
#if ISO_BIGENDIAN
	return highest_set_index(x) >> 3;
#else
	return lowest_set_index(x) >> 3;
#endif
}

template<typename T> constexpr int		lowest_clear_index(T x)		{ return lowest_set_index(~x); }
template<typename T> constexpr int		highest_clear_index(T x)	{ return highest_set_index(~x); }

template<typename T> constexpr uint32	log2_floor(T x)				{ return highest_set_index(x); }

template<typename T> constexpr uint32	log2_ceil(T x)				{ return log2_floor(x - 1) + 1; }
template<typename T> constexpr enable_if_t<is_int<T>, uint32> log2(T x)	{ return log2_floor(x - 1) + 1; }
template<typename T> constexpr bool		is_pow2(T x)				{ return (x & (x - 1)) == 0; }
template<typename T> constexpr T		next_pow2(T x)				{ return T(1) << (highest_set_index(x - 1) + 1); }

template<typename T> constexpr auto		lo(const T &a)				{ return uint_t<sizeof(T) / 2>(a); }
template<typename T> constexpr auto		hi(const T &a)				{ return uint_t<sizeof(T) / 2>(a >> (sizeof(T) * 4)); }
template<typename T> constexpr auto&	lo(T &a)					{ return ((uint_t<sizeof(T) / 2>*)&a)[ iso_bigendian]; }
template<typename T> constexpr auto&	hi(T &a)					{ return ((uint_t<sizeof(T) / 2>*)&a)[!iso_bigendian]; }
template<typename T> constexpr auto		lo_hi(T lo, T hi)			{ return (uint_t<sizeof(T) * 2>(hi) << (sizeof(T) * 8)) + lo; }

template<typename T> constexpr bool		get_sign(T a)				{ return a < 0; }
template<typename T> constexpr auto		set_sign(T a, bool neg)		{ return neg ? -(signed_t<T>)a : (signed_t<T>)a; }

constexpr int count_bits1(uint8 i) 	{ return i & 1; }
constexpr int count_bits2(uint8 i) 	{ return (i - (i >> 1)) & 3; }
constexpr int _count_bits4(uint8 i)	{ return (i + (i >> 2)) & 3; }
constexpr int count_bits4(uint8 i) 	{ return _count_bits4(i - (i >> 1) & 5); }

//-----------------------------------------------------------------------------
// sign extend
//-----------------------------------------------------------------------------

template<int S, typename T> constexpr signed_t<T>	sign_extend(T x)				{ return signed_t<T>(x - ((x << 1) & bit<T>(S))); }
template<typename T>		constexpr signed_t<T>	sign_extend(T x, uint32 s)		{ return signed_t<T>(x - ((x << 1) & bit<T>(s))); }
template<typename T>		constexpr signed_t<T>	_mask_sign_extend(T x, T m)		{ return signed_t<T>((x & (m - 1)) - ((x << 1) & m)); }
template<int S, typename T> constexpr signed_t<T>	mask_sign_extend(T x)			{ return signed_t<T>(_mask_sign_extend(x, bit<T>(S))); }
template<typename T>		constexpr signed_t<T>	mask_sign_extend(T x, uint32 s)	{ return signed_t<T>(_mask_sign_extend(x, bit<T>(s))); }

template<bool> struct s_extend;
template<> struct s_extend<false> {	// unsigned
	template<typename T> static constexpr T f(T x, uint32 s)	{ return x; }
	template<typename T> static constexpr T mask(T x, uint32 s)	{ return x & bits<T>(s); }
};
template<> struct s_extend<true> {	// signed
	template<typename T> static constexpr T f(T x, uint32 s)	{ return sign_extend(x, s); }
	template<typename T> static constexpr T mask(T x, uint32 s)	{ return mask_sign_extend(x, s); }
};

//-----------------------------------------------------------------------------
// shifts/rotates (more in bits.h)
//-----------------------------------------------------------------------------

template<typename T> constexpr auto		BIT_SIGN	= uint_for_t<T>(1) << (BIT_COUNT<T> - 1);

template<typename T> constexpr auto shift_left(T x, uint32 y)		{ return x << y; }
template<typename T> constexpr auto shift_left_check(T x, uint32 y)	{ return y < BIT_COUNT<T> ? shift_left(x, y) : T(zero); }
template<typename T> constexpr auto shift_right(T x, uint32 y)		{ return x >> y; }
template<typename T> constexpr auto shift_right_check(T x, uint32 y){ return y < BIT_COUNT<T> ? shift_right(x, y) : T(zero); }
template<typename T> constexpr auto shift_right_round(T x, uint32 y){ return shift_right(x + bit<T>(y - 1), y); }
template<typename T> constexpr auto shift_right_ceil(T x, uint32 y)	{ return shift_right(x + bits<T>(y), y); }
template<typename T> constexpr auto shift_bits(T x, int y)			{ return y < 0 ? shift_right(x, -y) : shift_left(x, y); }
template<typename T> constexpr auto ashift_right(T x, uint32 y)		{ return ((x ^ BIT_SIGN<T>) >> y) - (BIT_SIGN<T> >> y); }
template<typename T> constexpr auto shift_bits_round(T x, int y)	{ return y < 0 ? shift_right_round(x, -y) : shift_left(x, y); }
template<typename T> constexpr auto shift_bits_ceil(T x, int y)		{ return y < 0 ? shift_right_ceil(x, -y)  : shift_left(x, y); }
template<typename T> constexpr auto ashift_bits(T x, int y)			{ return y < 0 ? ashift_right(x, -y) : shift_left(x, y); }
template<typename T> constexpr auto rotate_left(T x, uint32 y)		{ return shift_left(x, y) | shift_right(x, (BIT_COUNT<T> - y)); }
template<typename T> constexpr auto rotate_right(T x, uint32 y)		{ return rotate_left(x, BIT_COUNT<T> - y); }
template<typename T> constexpr auto rotate_bits(T x, int y)			{ return rotate_left(x, y & (BIT_COUNT<T> - 1)); }

template<typename T> static constexpr T _double_rotate(T a0, T a1, uint32 c) {
	return c ? shift_left_check(a1, c) | (a0 >> (BIT_COUNT<T> - c)) : a1;
}
template<typename T> static constexpr T double_shift_left_hi(T a0, T a1, uint32 c) {
	return c < BIT_COUNT<T> ? _double_rotate(a0, a1, c) : a0 << (c - BIT_COUNT<T>);
}
template<typename T> static constexpr T double_shift_right_lo(T a0, T a1, uint32 c) {
	return c < BIT_COUNT<T> ? _double_rotate(a1, a0, c - BIT_COUNT<T>) : a1 >> (c - BIT_COUNT<T>);
}
template<typename T> static constexpr T double_rotate_left_hi(T a0, T a1, uint32 c) {
	return c & BIT_COUNT<T> ? _double_rotate(a1, a0, c & (BIT_COUNT<T> - 1)) : _double_rotate(a0, a1, c & (BIT_COUNT<T> - 1));
}

//fixed number of bits
template<typename T, unsigned N> struct s_fixed_shift {
	static constexpr T left(T a)	{ return a << N; }
	static constexpr T right(T a)	{ return a >> N; }
};

template<int N, typename T> constexpr auto shift_left(T x)			{ return s_fixed_shift<T,N>::left(x); }
template<int N, typename T> constexpr auto shift_right(T x)			{ return s_fixed_shift<T,N>::right(x); }
template<int N, typename T> constexpr auto shift_right_round(T x)	{ return s_fixed_shift<T, 1>::right(x + bit<T>(N - 1)); }
template<int N, typename T> constexpr auto shift_right_ceil(T x)	{ return s_fixed_shift<T, N>::right(x + bits<T>(N)); }
template<int N, typename T> constexpr auto ashift_right(T x)		{ return shift_right<N>(x ^ BIT_SIGN<T>) - kbit<T, BIT_COUNT<T> - 1 - N>; }
template<int N, typename T> constexpr auto rotate_left(T x)			{ return shift_left<N>(x) | shift_right<BIT_COUNT<T> - N>(x); }
template<int N, typename T> constexpr auto rotate_right(T x)		{ return shift_right<N>(x) | shift_left<BIT_COUNT<T> - N>(x); }
template<int N, typename T> constexpr auto rotate_bits(T x)			{ return rotate_left<N & (BIT_COUNT<T> - 1)>(x); }

//fixed +- bits
template<int N, bool = (N < 0)> struct s_fixed_shift1;
template<int N> struct s_fixed_shift1<N, false>	{
	template<typename T> static constexpr auto f(T x)	{ return s_fixed_shift<T,N>::left(x); }
	template<typename T> static constexpr auto a(T x)	{ return s_fixed_shift<T,N>::left(x); }
	template<typename T> static constexpr auto r(T x)	{ return s_fixed_shift<T,N>::left(x); }
	template<typename T> static constexpr auto c(T x)	{ return s_fixed_shift<T,N>::left(x); }
};
template<int N>	struct s_fixed_shift1<N, true>		{
	template<typename T> static constexpr auto f(T x)	{ return s_fixed_shift<T,-N>::right(x); }
	template<typename T> static constexpr auto a(T x)	{ return f(x) | -(f(x & (T(1) << (BIT_COUNT<T> - 1)))); }
	template<typename T> static constexpr auto r(T x)	{ return s_fixed_shift<T, -N>::right(x + bit<T>(~N)); }
	template<typename T> static constexpr auto c(T x)	{ return s_fixed_shift<T, -N>::right(x + bits<T>(-N)); }
};

template<int N, typename T> constexpr auto shift_bits(T x)			{ return s_fixed_shift1<N>::f(x); }
template<int N, typename T> constexpr auto shift_bits_round(T x)	{ return s_fixed_shift1<N>::r(x); }
template<int N, typename T> constexpr auto shift_bits_ceil(T x)		{ return s_fixed_shift1<N>::c(x); }
template<int N, typename T> constexpr auto ashift_bits(T x)			{ return s_fixed_shift1<N>::a(x); }

//shift to different (sized) type
template<typename D> struct s_shift_to {
	template<typename S> static constexpr D		left(S x, uint32 y)				{ return shift_left(D(x), y); }
	template<typename S> static constexpr D		right(S x, uint32 y)			{ return (D)shift_right(x, y); }
	template<typename S> static constexpr D		right_round(S x, uint32 y)		{ return (D)shift_right_round(x, y); }
	template<typename S> static constexpr D		right_ceil(S x, uint32 y)		{ return (D)shift_right_ceil(x, y); }
};

template<typename D, typename S> constexpr D	shift_left2(S x, uint32 y)		{ return s_shift_to<D>::left(x, y); }
template<typename D, typename S> constexpr D	shift_right2(S x, uint32 y)		{ return s_shift_to<D>::right(x, y); }
template<typename D, typename S> constexpr D	shift_right_round2(S x, uint32 y){ return s_shift_to<D>::right_round(x, y); }
template<typename D, typename S> constexpr D	shift_right_ceil2(S x, uint32 y){ return s_shift_to<D>::right_ceil(x, y); }
template<typename D, typename S> constexpr D	shift_bits2(S x, int y)			{ return y < 0 ? shift_right2<D>(x, -y)			: shift_left2<D>(x, y); }
template<typename D, typename S> constexpr D	shift_bits_round2(S x, int y)	{ return y < 0 ? shift_right_round2<D>(x, -y)	: shift_left2<D>(x, y); }
template<typename D, typename S> constexpr D	shift_bits_ceil2(S x, int y)	{ return y < 0 ? shift_right_ceil2<D>(x, -y)	: shift_left2<D>(x, y); }

template<typename D, typename S, typename R = D> using if_bigger_t	= enable_if_t<(sizeof(D) > sizeof(S)), D>;
template<typename D, typename S, typename R = D> using if_smaller_t	= enable_if_t<(sizeof(D) <= sizeof(S)), D>;

//-----------------------------------------------------------------------------
// carry operations
//-----------------------------------------------------------------------------

template<typename T> inline bool addc(T a, T b, bool c, T &r) {
	T	t = a + b + T(c);
	r = t;
	return t < a || (c && t == a);
}
template<typename T> inline bool subc(T a, T b, bool c, T &r) {
	r = a - b - T(c);
	return b > a || (c && b == a);
}

template<typename A, typename B> A divmod(A a, B b, B &mod) {
	mod		= a % b;
	return a / b;
}

// mod in a
template<typename A, typename B> A divmod(A &a, B b) {
	A	d	= a / b;
	a = a % b;
	return d;
}

// returns full result in larger int
template<typename A, typename B, typename=enable_if_t<is_int<A> && is_int<B>>> inline uint_t<sizeof(A) + sizeof(B)> fullmul(A a, B b) {
	return uint_t<sizeof(A) + sizeof(B)>(a) * b;
}


#if (defined(__GNUC__) && defined(__SIZEOF_INT128__)) || ((defined(PLAT_PC) || defined(PLAT_XONE)) && defined(_M_AMD64))
#define NATIVE_MUL	64
#else
#define NATIVE_MUL	32
#endif
#define NATIVE_DIV	32

template<typename T, bool SIGNED = is_signed<T>, bool BIG = (BIT_COUNT<T> > NATIVE_MUL)> struct s_mulc;
template<typename T, bool SIGNED = is_signed<T>, bool BIG = (BIT_COUNT<T> > NATIVE_DIV)> struct s_divc;
template<typename A, typename B, typename C, bool BIG = (is_int<A> && is_int<B> && BIT_COUNT<A> + BIT_COUNT<B> > NATIVE_MUL)> struct s_muldivc;

// big, unsigned
template<typename T> struct s_mulc<T, false, true> {
	enum { HBITS = sizeof(T) * 4 };
	typedef	uint_bits_t<HBITS>	H;

	static inline T mul(T a, T b, T &r) {
		H	a0 = lo(a), a1 = hi(a);
		H	b0 = lo(b), b1 = hi(b);
		T	r0 = fullmul(a0, b0), r1 = fullmul(a0, b1) + fullmul(b0, a1),  r2 = fullmul(a1, b1);
		r = r0 + (r1 << HBITS);
		return (r1 >> HBITS) + r2;
	}
	static inline T madd(T a, T b, T c, T &r) {
		H	a0 = lo(a), a1 = hi(a);
		H	b0 = lo(b), b1 = hi(b);
		T	r0 = fullmul(a0, b0) + c, r1 = fullmul(a0, b1) + fullmul(b0, a1), r2 = fullmul(a1, b1);
		r = r0 + (r1 << HBITS);
		return (r1 >> HBITS) + r2;
	}
	static inline T mul(T a, H b, H &r) {
		T	r1 = fullmul(b, H(a));
		r = H(r1);
		return fullmul(b, H(a >> HBITS)) + (r1 >> HBITS);
	}
};

// big, signed
template<typename T> struct s_mulc<T, true, true> {
	typedef	uint_for_t<T>			U;
	typedef s_mulc<U, false, true>	B;

	static inline T mul(U a, T b, T &r)			{ return B::mul(a, (U)b, (U&)r); }
	static inline T madd(U a, T b, T c, T &r)	{ return B::madd(a, (U)b, (U)c, (U&)r); }
};

// not big
template<typename T, bool SIGNED> struct s_mulc<T, SIGNED, false> {
	enum { HBITS = sizeof(T) * 4 };
	typedef	uint_bits_t<HBITS>	H;

	static inline T mul(T a, T b, T &r) {
		auto x = fullmul(a, b);
		r	= lo(x);
		return hi(x);
	}
	static inline T madd(T a, T b, T c, T &r) {
		auto x	= fullmul(a, b) + c;
		r	= lo(x);
		return hi(x);
	}
	static inline T mul(T a, H b, H &r) {
		auto x = fullmul(a, T(b));
		r	= H(x);
		return x >> HBITS;
	}
};


// big, unsigned
template<typename T> struct s_divc<T, false, true> {
	enum { HBITS = sizeof(T) * 4 };
	typedef	uint_bits_t<HBITS>	H;

	static inline H divmod1(H u0, T u1, T v, T *mod) {
		if (u1 >= v)
			return mod ? *mod = H(~H(0)) : H(~H(0));
		u1		= (u1 << HBITS) | u0;
		return mod ? divmod(u1, v, *mod) : u1 / v;
	}
	static inline T fulldivmod(H u0, T u1, T v, T *mod) {
		if (u1 >= v)
			return mod ? *mod = H(~H(0)) : H(~H(0));

		T	q1	= divmod(u1, v);
		u1		= (u1 << HBITS) | u0;
		T	q0	= mod ? divmod(u1, v, *mod) : u1 / v;
		return q0 | (q1 << HBITS);
	}

	static inline T divmod1(T u0, T u1, T v, T *mod) {
		if (u1 >= v)
			return mod ? *mod = T(~T(0)) : T(~T(0));

		u1		= (u1 << HBITS) | hi(u0);
		T	q1	= divmod(u1, v);

		u1		= (u1 << HBITS) | lo(u0);
		T	q0	= mod ? divmod(u1, v, *mod) : u1 / v;
		return q0 | (q1 << HBITS);
	}

	static inline  T fulldivmod(T u0, T u1, T v, T *mod) {
		// overflow
		if (u1 >= v)
			return mod ? *mod = T(~T(0)) : T(~T(0));

		// normalise divisor
		int s = leading_zeros(v);
		if (s) {
			v	= v << s;
			u1	= (u1 << s) | (u0 >> (HBITS * 2 - s));
			u0	= u0 << s;
		}

		H	vn1 = hi(v),	vn0 = lo(v);
		H	un1 = hi(u0),	un0 = lo(u0);

		// Compute the first quotient digit, q1
		T	q1	= divmod(u1, vn1);	//u1 gets remainder
		u0		= q1 * vn0;
		while (!!hi(q1) || u0 > (u1 << HBITS) + un1) {
			--q1;
			u0 -= vn0;
			u1 += vn1;
			if (!!hi(u1))
				break;
		}

		// adjust
		u1 = (u1 << HBITS) + un1 - u0;

		// Compute the second quotient digit
		T	q0	= divmod(u1, vn1);	//u1 gets remainder
		u0		= q0 * vn0;
		while (!!hi(q0) || u0 > (u1 << HBITS) + un0) {
			--q0;
			u0 -= vn0;
			u1 += vn1;
			if (!!hi(u1))
				break;
		}

		// remainder
		if (mod)
			*mod = ((u1 << HBITS) + un0 - u0) >> s;

		return (q1 << HBITS) + q0;
	}

	static inline  T _reciprocal(T y) {
		H	t0	= s_divc<H>::reciprocal(H(y >> HBITS)), lo;
		T	hi	= mulc(y, t0, lo);
		return mulc(-hi, t0, lo) << 1;
	}
	static inline  T reciprocal(T y) {
		return !(y << 1) ? y : _reciprocal(y);
	}
};

// not big, unsigned
template<typename T> struct s_divc<T, false, false> {
	static inline T divmod1(T a0, T a1, T b, T *mod) {
		if (a1 >= b)
			return mod ? *mod = T(~T(0)) : T(~T(0));
		auto	x = lo_hi(a0, a1);
		if (mod)
			*mod = x % b;
		return T(x / b);
	}
	static inline  T fulldivmod(T a0, T a1, T b, T *mod) {
		if (a1 >= b)
			return mod ? *mod = T(~T(0)) : T(~T(0));
		auto	x = lo_hi(a0, a1);
		if (mod)
			*mod = x % b;
		return x / b;
	}
	static inline  T _reciprocal(T y) {
		typedef	uint_t<sizeof(T) * 2>	D;
		return (D(1) << (BIT_COUNT<D> - 1)) / y;
	}
	static inline  T reciprocal(T y) {
		return _reciprocal(y);
	}
};

// signed
template<typename T, bool BIG> struct s_divc<T, true, BIG> {
	typedef	uint_for_t<T>			U;
	typedef s_divc<U, false, BIG>	B;

	static inline T copysign(U o, T i)					{ return get_sign(i) ? -(T)o : (T)o; }
	static inline T copysign(U o, T *r, T i)			{ if (r && get_sign(i)) *r = -*r; return copysign(o, i); }

	static inline T divmod1(U a0, T a1, T b, T *mod)	{ return copysign(B::divmod1(a0, (U)abs(a1), (U)abs(b), (U*)mod), mod, a1 ^ b); }
	static inline T fulldivmod(U a0, T a1, T b, T *mod)	{ return copysign(B::fulldivmod(a0, (U)abs(a1), (U)abs(b), (U*)mod), mod, a1 ^ b); }
	static inline T reciprocal(T y)						{ return copysign(B::_reciprocal((U)abs(y)), y); }
};

template<typename L, typename H> using divmod_t		= signed_t<biggest_t<L, H>, is_signed<H>>;

// mulc = hi(a * b), r = lo(a * b)
template<typename A, typename B> inline A mulc(A a, B b, B &r) {
	return s_mulc<A>::mul(a, b, r);
}

// maddc = hi(a * b) + c, r = lo(a * b)
template<typename A, typename B> inline A maddc(A a, B b, A c, B &r) {
	return s_mulc<A>::madd(a, b, c, r);
}

// divc = [a0, a1] / b	(ignore top half of a1)
template<typename A0, typename A1, typename B> inline auto divc(A0 a0, A1 a1, B b) {
	return s_divc<divmod_t<A0,A1>>::divmod1(a0, a1, b, nullptr);
}

// divmodc = [a0, a1] / b, r = [a0, a1] % b	(ignore top half of a1)
template<typename A0, typename A1, typename B> inline auto divmodc(A0 a0, A1 a1, B b, B &mod) {
	return s_divc<divmod_t<A0,A1>>::divmod1(a0, a1, b, &mod);
}

// fulldivc = [a0, a1] / b	(using all of a1)
template<typename A0, typename A1, typename B> inline auto fulldivc(A0 a0, A1 a1, B b) {
	return s_divc<divmod_t<A0,A1>>::fulldivmod(a0, a1, b, nullptr);
}

// fullmoddivc = [a0, a1] / b, r = [a0, a1] % b	(using all of a1)
template<typename A0, typename A1, typename B> inline auto fulldivmodc(A0 a0, A1 a1, B b, B &mod) {
	return s_divc<divmod_t<A0,A1>>::fulldivmod(make_unsigned(a0), a1, b, &mod);
}

template<typename T> inline T reciprocalc(T t) {
	return s_divc<T>::reciprocal(t);
}

template<typename A, typename B, typename C> struct s_muldivc<A, B, C, true> {
	typedef	uint_t<sizeof(A) + sizeof(B) - sizeof(C)>	R;
	static R	mul_div(A a, B b, C c)	{
		B	l;
		A	h = mulc(a, b, l);
		return fulldivc(l, h, c);
	}
};

template<typename A, typename B, typename C> struct s_muldivc<A, B, C, false> {
	enum { AI = is_int<A>, I = AI || is_int<B> };
	typedef	if_t<I, uint_t<sizeof(A) + sizeof(B) - sizeof(C)>, if_t<AI, B, A>>	R;
	typedef	if_t<I, uint_t<sizeof(A) + sizeof(B)>, R>							M;
	static constexpr R	mul_div(A a, B b, C c)	{ return M(a) * b / c; }
};

template<typename A, typename B, typename C> constexpr auto mul_div(A a, B b, C c) {
	return s_muldivc<A, B, C>::mul_div(a, b, c);
}

template<typename N, typename D> inline N div_check(const N &n, const D &d)	{ return d ? n / d : 0; }
template<typename N, typename D> inline N mod_check(const N &n, const D &d)	{ return d ? n % d : n; }

template<typename A, typename B> constexpr enable_if_t<has_int_ops<A>, A>	div_round(A a, B b)			{ return (a * 2 + b) / (b * 2); }
template<typename A, typename B> constexpr enable_if_t<has_int_ops<A>, A>	div_round_down(A a, B b)	{ return a / b; }
template<typename A, typename B> constexpr enable_if_t<has_int_ops<A>, A>	div_round_up(A a, B b)		{ return (a + b - 1) / b; }

// divide by 2^N-1
template<int N, typename T> T div_round_bits(T t)			{ return (t + kbit<int, N - 1> + (t >> N)) >> N; }
template<typename T>		T div_round_bits(T t, int N)	{ return (t + bit(N - 1) + (t >> N)) >> N; }

// multiply by 2^N-1
template<int N, typename T> T mul_bits(T t)			{ return (t << N) - t; }
template<typename T>		T mul_bits(T t, int N)	{ return (t << N) - t; }

//-----------------------------------------------------------------------------
// saturating operations
//-----------------------------------------------------------------------------

template<typename T, bool SIGNED = is_signed<T>> struct s_sat;

//unsigned
template<typename T> struct s_sat<T, false> {
	static constexpr T _fix_add(T r, T a)	{ return r | -(r < a); }
	static constexpr T add(T a, T b)		{ return _fix_add(a + b, a); }
	static constexpr T _fix_sub(T r, T a)	{ return r & -(r <= a); }
	static constexpr T sub(T a, T b)		{ return _fix_sub(a - b, a); }
	template<typename T2> static constexpr T _fix_mul(T2 r)		{ return lo(r) | -!!hi(r); }
	static constexpr T mul(T a, T b)		{ return _fix_mul(fullmul(a, b)); }
};
//signed
template<typename T> struct s_sat<T, true> {
	typedef unsigned_t<T>	U;
	static constexpr int	B = BIT_COUNT<T> - 1;
	static constexpr T		M = kbits<T, B>;
	static constexpr T _fix_add(T r, T o, T b)	{ return T(T((o ^ b) | ~(b ^ r)) >= 0 ? o : r); }
	static constexpr T add(T a, T b)			{ return _fix_add(U(a) + U(b), (U(a) >> B) + M, b); }
	static constexpr T _fix_sub(T r, T o, T b)	{ return T(T((o ^ b) & (o ^ r)) < 0 ? o : r); }
	static constexpr T sub(T a, T b)			{ return _fix_sub(U(a) - U(b), (U(a) >> B) + M, b); }
	template<typename T2> static constexpr T _fix_mul(T2 r, T r2)	{ return hi(r) != (T(r) >> B) ? r2 : r; }
	static constexpr T mul(T a, T b)			{ return _fix_mul(fullmul(a, b), (U(a ^ b) >> B) + M); }
};

template<typename T> constexpr T add_sat(T a, T b) { return s_sat<T>::add(a, b); }
template<typename T> constexpr T sub_sat(T a, T b) { return s_sat<T>::sub(a, b); }
template<typename T> constexpr T mul_sat(T a, T b) { return s_sat<T>::mul(a, b); }

//-----------------------------------------------------------------------------
// extend bits
//-----------------------------------------------------------------------------

template<bool, int> struct s_extend_reduce;
template<int R> struct s_extend_reduce<false, R> {	//reduce
	template<int S, typename T> static constexpr T div(T t) {
		//(t + ((t + (t >> S)) >> S) + kbit<int, S - 1>) >> S;
		return (t + ((t) >> S) + kbit<int, S - 1>) >> S;
	}
	template<int S, int D, typename T> static constexpr T f(T x) {
		return div<S>(x * kbits<int, D>);
		//return (x * (kbits<int, D> * (1 + kbit<int, S> + kbit<int, S * 2>)) + kbit<int, S * 3 - 1>) >> (S * 3);
	}
};
template<int R> struct s_extend_reduce<true, R> {	//extend
	template<int S, int D, typename T> static constexpr T f(T x) {
		return x * (kbits<int, D> / kbits<int, S>) + (x >> R);
	}
};
template<> struct s_extend_reduce<true, 0> {	//extend
	template<int S, int D, typename T> static constexpr T f(T x) {
		return x * (kbits<int, D> / kbits<int, S>);
	}
};

template<int S, int D, typename T> constexpr T extend_bits(T x) {
	return s_extend_reduce<(S < D), S % D>::template f<S, D>(x);
}

template<typename T> constexpr T extend_bits(T x, int S, int D) {
	return x * ((bits(D) / bits(S)) & ~1) + (x >> ((D - S) % S));
}
template<typename T> constexpr T reduce_bits(T x, int S, int D) {
	return (x * (bits(D) * (1 + bit(S) + bit(S * 2))) + bit(S * 3 - 1)) >> (S * 3);
}

//-----------------------------------------------------------------------------
// double_int
//-----------------------------------------------------------------------------

template<typename T> struct double_int;

template<typename T> struct num_traits<double_int<T>> {
	enum {bits = sizeof(T) * 16};
	static const bool	is_signed	= num_traits<T>::is_signed;
	static const bool	is_float	= false;
};

template<typename T> struct double_int {
	T lo, hi;
	constexpr double_int()									{}
	constexpr double_int(const _zero&)	: lo(zero), hi(zero){}
	constexpr double_int(T i)			: lo(i), hi(zero)	{}
	constexpr double_int(T lo, T hi)	: lo(lo), hi(hi)	{}
	explicit operator bool()					const	{ return lo || hi; }
	explicit operator T()						const	{ return lo; }
	template<typename I> explicit operator I()	const	{ return (I)lo; }

	friend int compare(const double_int &a, const double_int &b) {
		return a.hi != b.hi ? (a.hi < b.hi ? -1 : 1) : (a.lo < b.lo ? -1 : int(a.lo != b.lo));
	}

//	bool		operator! ()					const	{ return !lo && !hi; }
	double_int	operator~ ()					const	{ return double_int(~lo, ~hi); }
	double_int	operator- ()					const	{ return double_int(-lo, -(hi + T(!lo))); }
	double_int	operator+ (const double_int &b)	const	{ T a = lo + b.lo; return double_int(a, hi + b.hi + T(a < lo)); }
	double_int	operator- (const double_int &b)	const	{ return double_int(lo - b.lo, hi - b.hi - T(lo < b.lo)); }
	double_int	operator& (const double_int &b)	const	{ return double_int(lo & b.lo, hi & b.hi); }
	double_int	operator| (const double_int &b)	const	{ return double_int(lo | b.lo, hi | b.hi); }
	double_int	operator^ (const double_int &b)	const	{ return double_int(lo ^ b.lo, hi ^ b.hi); }
	double_int	operator<<(unsigned b)			const	{ return {shift_left_check(lo, b), double_shift_left_hi(lo, hi, b)}; }
	double_int	operator>>(unsigned b)			const	{ return {double_shift_right_lo(lo, hi, b), shift_right_check(hi, b)}; }

	double_int& operator++()							{ if (!++lo) ++hi; return *this; }
	double_int& operator--()							{ if (!lo--) --hi; return *this; }
	double_int	operator++(int)							{ double_int t = *this; ++*this; return t; }
	double_int	operator--(int)							{ double_int t = *this; --*this; return t; }

	double_int&	operator+=(const double_int &b)			{ return *this = *this +  b; }
	double_int&	operator-=(const double_int &b)			{ return *this = *this -  b; }
	double_int&	operator*=(const double_int &b)			{ return *this = *this *  b; }
	double_int&	operator/=(const double_int &b)			{ return *this = *this /  b; }
	double_int&	operator*=(const T &b)					{ return *this = *this *  b; }
	double_int&	operator/=(const T &b)					{ return *this = *this /  b; }
	double_int&	operator&=(const double_int &b)			{ return *this = *this &  b; }
	double_int&	operator|=(const double_int &b)			{ return *this = *this |  b; }
	double_int&	operator^=(const double_int &b)			{ return *this = *this ^  b; }
	double_int&	operator<<=(unsigned b)					{ return *this = *this << b; }
	double_int&	operator>>=(unsigned b)					{ return *this = *this >> b; }

	bool		operator==(const double_int &b)	const	{ return hi == b.hi && lo == b.lo; }
	bool		operator< (const double_int &b)	const	{ return hi < b.hi || (hi == b.hi && lo < b.lo); }
	bool		operator<=(const double_int &b)	const	{ return hi < b.hi || (hi == b.hi && lo <= b.lo); }
	bool		operator!=(const double_int &b)	const	{ return !(*this == b); }
	bool		operator>=(const double_int &b)	const	{ return b <= *this; }
	bool		operator> (const double_int &b)	const	{ return b <  *this; }

	friend constexpr int	leading_zeros(const double_int &x)		{ return x.hi ? leading_zeros(x.hi) : leading_zeros(x.lo) + BIT_COUNT<T>; }
	friend constexpr int	highest_set_index(const double_int &x)	{ return x.hi ? highest_set_index(x.hi) + BIT_COUNT<T> : highest_set_index(x.lo); }
	friend constexpr int	lowest_set_index(const double_int &x)	{ return x.lo ? lowest_set_index(x.lo) : lowest_set_index(x.hi) + BIT_COUNT<T>; }
	friend constexpr double_int	clear_lowest(const double_int &x)	{ return x.lo ? double_int(clear_lowest(x.lo), x.hi) : double_int(x.lo, clear_lowest(x.hi)); }

	friend constexpr T		lo(const double_int &a)					{ return a.lo; }
	friend constexpr T		hi(const double_int &a)					{ return a.hi; }

	friend constexpr double_int	rotate_left(double_int x, uint32 y)	{ return {double_rotate_left_hi(x.lo, x.hi, y), double_rotate_left_hi(x.hi, x.lo, y)}; }
};

template<typename T> constexpr auto make_double_int(T t) { return (double_int<uint_t<sizeof(T) / 2>>&)t; }

template<typename T, typename B> double_int<T> operator*(const double_int<T> &a, const B b) {
	T	r0, r1	= mulc(a.lo, (T)b, r0);
	if (T h = a.hi)
		maddc(h, (T)b, r1, r1);
	return {r0, r1};
}
template<typename T, typename B> double_int<T> operator*(B a, const double_int<T> &b) {
	return b * a;
}
template<typename T, typename B> double_int<T> operator/(double_int<T> a, B b) {
	T	h = divmod(a.hi, b);
	return {fulldivc(a.lo, a.hi, (T)b), h};
}
template<typename T, typename B> T operator%(double_int<T> a, B b) {
	T	t;
	divmod(a.hi, b);
	divmodc(a.lo, a.hi, b, t);
	return t;
}
template<typename T, typename B> double_int<T> divmod(double_int<T> &a, B b) {
	T	rhi = divmod(a.hi, b), rlo = divmodc(a.lo, a.hi, b, a.lo);
	a.hi = 0;
	return {rlo, rhi};
}
template<typename T> double_int<T> divmod(double_int<T> a, T b, T &mod) {
	T	rhi = divmod(a.hi, b, mod), rlo = divmodc(a.lo, mod, b, mod);
	return {rlo, rhi};
}
template<typename T> double_int<T> operator*(const double_int<T> &a, const double_int<T> &b) {
	T	r0, r1	= mulc(a.lo, a.lo, r0);
	if (T h = b.hi)
		maddc(a.lo, h, r1, r1);
	if (T h = hi(a))
		maddc(b.lo, h, r1, r1);
	return {r0, r1};
}
template<typename T> double_int<T> operator/(const double_int<T> &a, const double_int<T> &b) {
	return fulldivc(a, double_int<T>(zero), b);
}
template<typename T> double_int<T> operator%(const double_int<T> &a, const double_int<T> &b) {
	double_int<T>	mod;
	fulldivmodc(a, double_int<T>(zero), b, mod);
	return mod;
}
template<typename T> double_int<T> divmod(double_int<T> &a, const double_int<T> &b) {
	return fulldivmodc(a, double_int<T>(zero), b, a);
}

template<typename D, typename S> constexpr if_smaller_t<D,double_int<S>> shift_right2(const double_int<S> &x, uint32 y) {
	return y < BIT_COUNT<S> ? shift_right2<D>(x.lo, y) : shift_right2<D>(x.hi, y - BIT_COUNT<S>);
}

template<typename I> struct s_shift_to<double_int<I>> {
	typedef double_int<I>	D;
	template<typename S> static constexpr D		left(S x, uint32 y) {
		return {y < BIT_COUNT<I> ? shift_left2<I>(x, y) : zero, y > BIT_COUNT<I> - BIT_COUNT<S> ? shift_bits2<I>(x, y - BIT_COUNT<I>) : zero};
	}
	template<typename S> static constexpr D		right(S x, uint32 y)			{ return (D)shift_right(x, y); }
	template<typename S> static constexpr D		right_round(S x, uint32 y)		{ return (D)shift_right_round(x, y); }
	template<typename S> static constexpr D		right_ceil(S x, uint32 y)		{ return (D)shift_right_ceil(x, y); }
};
//-----------------------------------------------------------------------------
// 128-bit types support
//-----------------------------------------------------------------------------

#if defined PLAT_CLANG

constexpr int	leading_zeros(uint128 x)		{ return hi(x) ? leading_zeros(hi(x)) : leading_zeros(lo(x)) + 64; }
constexpr int	highest_set_index(uint128 x)	{ return hi(x) ? highest_set_index(hi(x)) + 64 : highest_set_index(lo(x)); }
constexpr int	lowest_set_index(uint128 x)		{ return lo(x) ? lowest_set_index(lo(x)) : lowest_set_index(hi(x)) + 64; }

template<> struct num_traits<uint128> {
	enum {bits = sizeof(uint128) * 8};
	static const bool	is_signed	= false;
	static const bool	is_float	= false;
};

template<> struct num_traits<int128> {
	enum {bits = sizeof(int128) * 8};
	static const bool	is_signed	= true;
	static const bool	is_float	= false;
};

#elif defined(_M_IX86) || defined(_M_X64)

//-----------------------------------------------------------------------------
// MSVC uint128
//-----------------------------------------------------------------------------
//#if (defined(_M_IX86) || defined(_M_X64)) && _MSC_VER >= 1924 && !defined(__clang__)
#if _MSC_VER >= 1924
force_inline uint64 divc(uint64 a0, uint64 a1, uint64 b)						{ uint64 mod; return _udiv128(a1, a0, b, &mod); }
force_inline uint64 divmodc(uint64 a0, uint64 a1, uint64 b, uint64 &mod)		{ return _udiv128(a1, a0, b, &mod); }
force_inline uint64 fulldivc(uint64 a0, uint64 a1, uint64 b)					{ uint64 mod; return _udiv128(a1, a0, b, &mod); }
force_inline uint64 fulldivmodc(uint64 a0, uint64 a1, uint64 b, uint64 &mod)	{ return _udiv128(a1, a0, b, &mod); }

force_inline int64 divc(uint64 a0, int64 a1, int64 b)							{ int64 mod; return _div128(a1, a0, b, &mod); }
force_inline int64 divmodc(uint64 a0, int64 a1, int64 b, int64 &mod)			{ return _div128(a1, a0, b, &mod); }
force_inline int64 fulldivc(uint64 a0, int64 a1, int64 b)						{ int64 mod; return _div128(a1, a0, b, &mod); }
force_inline int64 fulldivmodc(uint64 a0, int64 a1, int64 b, int64 &mod)		{ return _div128(a1, a0, b, &mod); }
#endif

inline __m128i _mm_cmpgt_epu64(__m128i a, __m128i b) {
	__m128i sign64	= _mm_set1_epi64x(0x8000000000000000L);
	return _mm_cmpgt_epi64(_mm_xor_si128(a, sign64), _mm_xor_si128(b, sign64));
}

inline __m128i _mm_add_si128(__m128i a, __m128i b) {
	__m128i	s = _mm_add_epi64(a, b);
	return _mm_add_epi64(s, _mm_set_epi64x((uint64)_mm_extract_epi64(a, 0) > (uint64)_mm_extract_epi64(s, 0), 0));
}

inline __m128i _mm_sub_si128(__m128i a, __m128i b) {
	return _mm_sub_epi64(_mm_sub_epi64(a, b), _mm_set_epi64x((uint64)_mm_extract_epi64(b, 0) > (uint64)_mm_extract_epi64(a, 0), 0));
}

inline int _mm_cmp_su128(__m128i a, __m128i b) {
	__m128i	eq	= _mm_cmpeq_epi64(a, b);
	__m128i	gt	= _mm_cmpgt_epu64(a, b);
	return !_mm_extract_epi64(eq, 1)
		? (_mm_extract_epi64(gt, 1) & 2) - 1
		: ~_mm_extract_epi64(eq, 0) & ((_mm_extract_epi64(gt, 0) & 2) - 1);
}

inline int _mm_cmp_si128(__m128i a, __m128i b) {
	__m128i	eq	= _mm_cmpeq_epi64(a, b);
	__m128i	gt	= _mm_cmpgt_epi64(a, b);
	return !_mm_extract_epi64(eq, 1)
		? (_mm_extract_epi64(gt, 1) & 2) - 1
		: ~_mm_extract_epi64(eq, 0) & ((_mm_extract_epi64(gt, 0) & 2) - 1);
}

struct uint128;
struct int128;

template<> struct num_traits<uint128> {
	enum {bits = 128};
	static const bool	is_signed	= false;
	static const bool	is_float	= false;
};

template<> struct num_traits<int128> {
	enum {bits = 128};
	static const bool	is_signed	= true;
	static const bool	is_float	= false;
};

template<>	struct T_uint_type<16>	: T_type<uint128>	{};
template<>	struct T_sint_type<16>	: T_type<int128>	{};

template<> struct T_signed<uint128, true>	: T_type<int128> {};
template<> struct T_signed<int128,  false>	: T_type<uint128> {};

struct uint128 {
	__m128i	m;
	static uint128 zero()	{ return _mm_setzero_si128(); }

	uint128() {}
	constexpr uint128(__m128i _m)	: m(_m) {}
	uint128(uint64 i)				: m(_mm_cvtsi64_si128(i))	{}
	uint128(uint64 lo, uint64 hi)	: m(_mm_set_epi64x(hi, lo)) {}
	template<uint64 L, uint64 H> uint128(const digits_result128<L, H>&) : m(_mm_set_epi64x(H, L)) {}
	explicit inline uint128(const int128&);

	constexpr operator __m128i&()					{ return m; }
	constexpr operator __m128i()	const			{ return m; }
	constexpr operator __m128i()	volatile const	{ return const_cast<const __m128i&>(m); }
	explicit operator uint64()		const			{ return _mm_extract_epi64(m, 0); }
	explicit operator uint32()		const			{ return _mm_extract_epi64(m, 0); }
	explicit operator int()			const			{ return _mm_extract_epi64(m, 0); }
	explicit operator bool()		const			{ return !_mm_testz_si128(m, m); }

	uint128& operator=(uint128 b)			{ m = b.m; return *this; }
	void operator=(uint128 b) volatile		{ const_cast<__m128i&>(m) = b.m; }

	uint128	operator+ (uint128 b)	const	{ return _mm_add_si128(m, b.m); }
	uint128	operator- (uint128 b)	const	{ return _mm_sub_si128(m, b.m); }
	bool	operator! ()			const	{ return _mm_testz_si128(m, m); }
	uint128	operator~ ()			const	{ return _mm_xor_si128(m, _mm_set1_epi32(-1)); }
	uint128	operator- ()			const	{ return zero() - *this; }
	uint128	operator& (uint128 b)	const	{ return _mm_and_si128(m, b.m); }
	uint128	operator| (uint128 b)	const	{ return _mm_or_si128(m, b.m); }
	uint128	operator^ (uint128 b)	const	{ return _mm_xor_si128(m, b.m); }
	uint128	operator<<(unsigned b)	const	{ return b < 64 ? _mm_xor_si128(_mm_slli_epi64(m, b), _mm_slli_si128(_mm_srli_epi64(m, 64 - b), 8)) : _mm_slli_epi64(_mm_slli_si128(m, 8), b - 64); }
	uint128	operator>>(unsigned b)	const	{ return b < 64 ? _mm_xor_si128(_mm_srli_epi64(m, b), _mm_srli_si128(_mm_slli_epi64(m, 64 - b), 8)) : _mm_srli_epi64(_mm_srli_si128(m, 8), b - 64); }

	uint128 operator*(uint64 b) const {
		uint64	r0, r1	= mulc(lo(*this), b, r0);
		if (uint64 h = hi(*this))
			maddc(h, b, r1, r1);
		return {r0, r1};
	}
	uint128 operator/(uint64 b) const {
		uint64	mod, h = divmod(hi(*this), b, mod);
		return {fulldivc(lo(*this), mod, b), h};
	}
	uint64 operator%(uint64 b) const {
		uint64	mod;
		divmod(hi(*this), b, mod);
		fulldivmodc(lo(*this), mod, b, mod);
		return mod;
	}
	uint128 operator*(uint128 b) const {
		uint64	r0, r1	= mulc(lo(*this), lo(b), r0);
		if (uint64 h = hi(b))
			maddc(lo(*this), h, r1, r1);
		if (uint64 h = hi(*this))
			maddc(lo(b), h, r1, r1);
		return {r0, r1};
	}
	uint128 operator/(uint128 b) const {
		return fulldivc(*this, uint128::zero(), b);
	}
	uint128 operator%(uint128 b) const {
		uint128	mod;
		fulldivmodc(*this, zero(), b, mod);
		return mod;
	}

	uint128& operator++()					{ if (!++lo(*this)) ++hi(*this); return *this; }
	uint128& operator--()					{ if (!lo(*this)--) --hi(*this); return *this; }
	uint128	operator++(int)					{ uint128 t = *this; ++*this; return t; }
	uint128	operator--(int)					{ uint128 t = *this; --*this; return t; }

	uint128& operator+= (uint128 b)			{ return *this = *this +  b; }
	uint128& operator-= (uint128 b)			{ return *this = *this -  b; }
	uint128& operator*= (uint128 b)			{ return *this = *this *  b; }
	uint128& operator/= (uint128 b)			{ return *this = *this /  b; }
	uint128& operator*= (uint64 b)			{ return *this = *this *  b; }
	uint128& operator/= (uint64 b)			{ return *this = *this /  b; }
	uint128& operator&= (uint128 b)			{ return *this = *this &  b; }
	uint128& operator|= (uint128 b)			{ return *this = *this |  b; }
	uint128& operator^= (uint128 b)			{ return *this = *this ^  b; }
	uint128& operator<<=(int b)				{ return *this = *this << b; }
	uint128& operator>>=(int b)				{ return *this = *this >> b; }

	friend int compare(const uint128 &a, const uint128 &b) { return _mm_cmp_su128(a.m, b.m); }

	bool operator==(uint128 b)	const		{ __m128i t = _mm_xor_si128(m, b.m); return _mm_testz_si128(t, t); }
	bool operator!=(uint128 b)	const		{ return !(*this == b); }
	bool operator< (uint128 b)	const		{ return compare(*this, b) < 0; }
	bool operator<=(uint128 b)	const		{ return compare(*this, b) <= 0; }
	bool operator>=(uint128 b)	const		{ return compare(*this, b) >= 0; }
	bool operator> (uint128 b)	const		{ return compare(*this, b) > 0; }

	friend uint64&	lo(uint128 &a)			{ return ((uint64*)&a)[0]; }
	friend uint64&	hi(uint128 &a)			{ return ((uint64*)&a)[1]; }
	friend uint64	lo(const uint128 &a)	{ return (uint64)_mm_extract_epi64(a.m, 0); }
	friend uint64	hi(const uint128 &a)	{ return (uint64)_mm_extract_epi64(a.m, 1); }

	friend int leading_zeros(uint128 x)		{ return hi(x) ? leading_zeros(hi(x)) : leading_zeros(lo(x)) + 64; }
	friend int highest_set_index(uint128 x)	{ return hi(x) ? highest_set_index(hi(x)) + 64 : highest_set_index(lo(x)); }
	friend int lowest_set_index(uint128 x)	{ return lo(x) ? lowest_set_index(lo(x)) : lowest_set_index(hi(x)) + 64; }

	friend uint128 operator*(uint64 a, const uint128 &b) {
		return b * a;
	}
};

//-----------------------------------------------------------------------------
// MSVC int128
//-----------------------------------------------------------------------------

struct int128 {
	__m128i	m;
	static int128 zero()					{ return _mm_setzero_si128(); }
	friend bool		get_sign(int128 a)		{ return hi(a) < 0; }
	friend int128	get_sign128(int128 a)	{
		__m128i m = _mm_cmpgt_epi64(_mm_setzero_si128(), a);
		return _mm_unpackhi_epi64(m, m);
	}
	friend int128	apply_sign128(__m128i a, int128 m) {
		__m128i	t = _mm_xor_si128(a, m);
		__m128i	c = _mm_unpacklo_epi64(m, _mm_and_si128(_mm_cmpeq_epi64(t, m), m));
		return _mm_sub_epi64(t, c);
	}
	friend int128	abs(int128 a) {
		return apply_sign128(a, get_sign128(a));
	}

	int128() {}
	constexpr int128(__m128i m)			: m(m)		{}
	int128(int64 i)						: m(_mm_set_epi64x(i>>63, i))	{}
	int128(uint64 i)					: m(_mm_set_epi64x(0, i))		{}
	int128(int64 lo, int64 hi)			: m(_mm_set_epi64x(hi, lo))		{}
	template<uint64 L, uint64 H> int128(const digits_result128<L, H>&) : m(_mm_set_epi64x(H, L)) {}
	explicit int128(const uint128 &u)	: m(u.m)	{}

	constexpr operator __m128i&()					{ return m; }
	constexpr operator __m128i()	const			{ return m; }
	constexpr operator __m128i()	volatile const	{ return const_cast<const __m128i&>(m); }
	explicit operator bool()		const			{ return !_mm_testz_si128(m, m); }

	int128& operator=(int128 b)				{ m = b.m; return *this; }
	void operator=(int128 b) volatile		{ const_cast<__m128i&>(m) = b.m; }

	int128	operator+ (int128 b)		const	{ return _mm_add_si128(m, b.m); }
	int128	operator- (int128 b)		const	{ return _mm_sub_si128(m, b.m); }
	bool	operator! ()				const	{ return _mm_testz_si128(m, m); }
	int128	operator~ ()				const	{ return _mm_xor_si128(m, _mm_set1_epi32(-1)); }
	int128	operator- ()				const	{ return apply_sign128(*this, _mm_set1_epi32(-1)); }
	int128	operator& (int128 b)		const	{ return _mm_and_si128(m, b.m); }
	int128	operator| (int128 b)		const	{ return _mm_or_si128(m, b.m); }
	int128	operator^ (int128 b)		const	{ return _mm_xor_si128(m, b.m); }
	int128	operator<<(unsigned b)		const	{ return b < 64 ? _mm_xor_si128(_mm_slli_epi64(m, b), _mm_slli_si128(_mm_srli_epi64(m, 64 - b), 8)) : _mm_slli_epi64(_mm_slli_si128(m, 8), b - 64); }
	int128	operator>>(unsigned b)		const	{
		auto neg	= _mm_unpackhi_epi64(_mm_setzero_si128(), _mm_cmpgt_epi64(_mm_setzero_si128(), m));
		if (b < 64) {
			return _mm_or_si128(_mm_xor_si128(_mm_srli_epi64(m, b), _mm_srli_si128(_mm_slli_epi64(m, 64 - b), 8)), _mm_slli_epi64(neg, 64 - b));
		} else {
			auto t	= _mm_xor_si128(_mm_srli_epi64(_mm_unpackhi_epi64(m, neg), b - 64), neg);
			return _mm_or_si128(_mm_unpacklo_epi64(t, _mm_setzero_si128()), _mm_unpackhi_epi64(t, neg));
		}
	}

	int128 operator*(int64 b)	const	{ return int128(uint128(*this) * uint64(b)); }

	int128 operator/(int64 b) const {
		int64	mod, rhi = divmod(hi(*this), b, mod), rlo = fulldivc(lo(*this), mod, b);
		return {rlo, rhi + (rlo >> 63)};
	}
	int64 operator%(int64 b) const {
		int64	mod;
		divmod(hi(*this), b, mod);
		fulldivmodc(lo(*this), mod, b, mod);
		return mod;
	}

	int128 operator*(int128 b)	const	{ return int128(uint128(*this) * uint128(b)); }
	int128 operator/(int128 b)	const	{ return apply_sign128((uint128)abs(*this) / (uint128)abs(b), get_sign128(*this ^ b)); }
	int128 operator%(int128 b)	const	{ return apply_sign128((uint128)abs(*this) % (uint128)abs(b), get_sign128(*this ^ b)); }

	int128& operator++()				{ if (!++lo(*this)) ++hi(*this); return *this; }
	int128& operator--()				{ if (!lo(*this)--) --hi(*this); return *this; }
	int128	operator++(int)				{ int128 t = *this; ++*this; return t; }
	int128	operator--(int)				{ int128 t = *this; --*this; return t; }

	int128& operator+= (int128 b)		{ return *this = *this +  b; }
	int128& operator-= (int128 b)		{ return *this = *this -  b; }
	int128& operator*= (int128 b)		{ return *this = *this *  b; }
	int128& operator/= (int128 b)		{ return *this = *this /  b; }
	int128& operator*= (uint64 b)		{ return *this = *this *  b; }
	int128& operator/= (uint64 b)		{ return *this = *this /  b; }
	int128& operator&= (int128 b)		{ return *this = *this &  b; }
	int128& operator|= (int128 b)		{ return *this = *this |  b; }
	int128& operator^= (int128 b)		{ return *this = *this ^  b; }
	int128& operator<<=(int b)			{ return *this = *this << b; }
	int128& operator>>=(int b)			{ return *this = *this >> b; }

	friend int compare(const int128 &a, const int128 &b) { return _mm_cmp_si128(a.m, b.m); }

	bool operator==(int128 b)	const	{ __m128i t = _mm_xor_si128(m, b.m); return _mm_testz_si128(t, t); }
	bool operator!=(int128 b)	const	{ return !(*this == b); }
	bool operator< (int128 b)	const	{ return compare(*this, b) < 0; }
	bool operator<=(int128 b)	const	{ return compare(*this, b) <= 0; }
	bool operator>=(int128 b)	const	{ return compare(*this, b) >= 0; }
	bool operator> (int128 b)	const	{ return compare(*this, b) > 0; }

	friend uint64&	lo(int128 &a)		{ return ((uint64*)&a)[0]; }
	friend int64&	hi(int128 &a)		{ return ((int64*)&a)[1]; }
	friend uint64	lo(const int128 &a)	{ return (uint64)_mm_extract_epi64(a.m, 0); }
	friend int64	hi(const int128 &a)	{ return (int64)_mm_extract_epi64(a.m, 1); }

	friend int128 operator*(int64 a, int128 b) {
		return b * a;
	}
	friend int128 divmod(int128 &a, int128 b) {
		return fulldivmodc(a, zero(), b, a);
	}
	friend int128 divmod(int128 a, int128 b, int128 &mod) {
		return fulldivmodc(a, zero(), b, mod);
	}
};

uint128::uint128(const int128 &i) : m(i.m) {}

inline int128	set_sign(uint128 a, bool neg) {
	return apply_sign128(a, _mm_set1_epi32(-int64(neg)));
}

//-----------------------------------------------------------------------------
// MSVC 128 bit shifts
//-----------------------------------------------------------------------------

template<int N, bool mult8, bool lt64> struct shift128_s {
	force_inline static uint128	left(const uint128 &a)	{ return _mm_slli_si128(a, N >> 3); }
	force_inline static uint128	right(const uint128 &a)	{ return _mm_srli_si128(a, N >> 3); }
	force_inline static uint128	right(const int128 &a)	{
		auto t	= _mm_cmpgt_epi64(_mm_setzero_si128(), a);
		t		= _mm_unpackhi_epi64(t, t);
		return _mm_or_si128(_mm_slli_si128(t, (-(N >> 3)) & 15), _mm_srli_si128(a, N >> 3)); }
};
template<int N> struct shift128_s<N, false, true> {
	force_inline static __m128i	raw_right(__m128i a)	{ return _mm_or_si128(_mm_srli_epi64(a, N), _mm_srli_si128(_mm_slli_epi64(a, 64 - N), 8)); }
	force_inline static uint128	left(const uint128 &a)	{ return _mm_or_si128(_mm_slli_epi64(a, N), _mm_slli_si128(_mm_srli_epi64(a, 64 - N), 8)); }
	force_inline static uint128	right(const uint128 &a)	{ return raw_right(a); }
	force_inline static uint128	right(const int128 &a)	{
		auto t	= _mm_cmpgt_epi64(_mm_setzero_si128(), a);
		t		= _mm_unpackhi_epi64(t, _mm_setzero_si128());
		return _mm_or_si128(raw_right(a), _mm_slli_epi64(t, 64 - N));
	}
};
template<int N> struct shift128_s<N, false, false> {
	force_inline static uint128	left(const uint128 &a)	{ return _mm_slli_epi64(_mm_slli_si128(a, N >> 3), N & 7); }
	force_inline static uint128	right(const uint128 &a)	{ return _mm_srli_epi64(_mm_srli_si128(a, N >> 3), N & 7); }
	//	force_inline static uint128	right(const int128 &a)	{
	//		auto t	= _mm_cmpgt_epi64(_mm_setzero_si128(), a);
	//		t		= _mm_unpackhi_epi64(t, a);
	//		auto t2	= _mm_srli_epi64(_mm_srli_si128(t, (N - 64) >> 3), N & 7);
	//	}
};

template<unsigned N> struct s_fixed_shift<uint128, N>	: shift128_s<N, (N & 7) == 0, (N < 64)> {};
template<unsigned N> struct s_fixed_shift<int128, N>	: shift128_s<N, (N & 7) == 0, (N < 64)> {};

#ifdef _M_AMD64
force_inline uint128	fullmul(uint64 a, uint64 b)	{ uint64 hi, lo = _umul128(a, b, &hi);	return uint128(lo, hi); }
force_inline int128		fullmul(int64 a, int64 b)	{ int64	 hi, lo = _mul128(a, b, &hi);	return int128( lo, hi); }
#endif

#else

//-----------------------------------------------------------------------------
// 128-bit support through double_int
//-----------------------------------------------------------------------------

typedef double_int<int64>	int128;
typedef double_int<uint64>	uint128;

#endif

template<> struct T_builtin_int<uint128>	: T_true {};
template<> struct T_builtin_int<int128>		: T_true {};

#if defined(__clang__) || (!defined(_M_IX86) && !defined(_M_X64))
template<>	struct T_uint_type<16>	: T_type<uint128>	{};
template<>	struct T_sint_type<16>	: T_type<int128>	{};
#endif

inline uint128 divmod(uint128 &a, uint64 b) {
	uint64	rhi = divmod(hi(a), b), rlo = fulldivmodc(lo(a), hi(a), b, lo(a));
	hi(a) = 0;
	return lo_hi(rlo, rhi);
}
inline uint128 divmod(uint128 a, uint64 b, uint64 &mod) {
	uint64	rhi = divmod(hi(a), b, mod), rlo = fulldivmodc(lo(a), mod, b, mod);
	return lo_hi(rlo, rhi);
}

template<char... C> uint128	operator"" _u128()	{ return int_reader128<C...>(); }
template<char... C> int128	operator"" _i128()	{ return int_reader128<C...>(); }


//-----------------------------------------------------------------------------
// run time logs
//-----------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4723 4309)
#endif

template<typename T> struct _log_base_result {
	T value, remainder;
	constexpr _log_base_result(T value, T remainder) : value(value), remainder(remainder) {}
	template<T B> static force_inline constexpr _log_base_result make(T N) {
		return N / B >= B
			? make<B * B>(N).template next<B>()
			: _log_base_result(T(N >= B), N / B);
	}

	template<T B> constexpr _log_base_result	next() {
		return _log_base_result(value * 2 + (remainder >= B), remainder >= B ? remainder / B : remainder);
	}
};

template<int B> constexpr uint32 _log_base(uint128 v);

template<typename T, T B, bool X = (num_traits<T>::max() / B >= B)> struct _log_base_result1;
template<typename T, T B> struct _log_base_result1<T, B, false> : _log_base_result<T> {
	_log_base_result1(T N) : _log_base_result<T>(T(N >= B), N / B) {}
};
template<typename T, T B> struct _log_base_result1<T, B, true> : _log_base_result<T> {
	_log_base_result1(T N) : _log_base_result<T>(N / B >= B ? _log_base_result1<T, B * B>(N).template next<B>() : _log_base_result<T>(T(N >= B), N / B)) {}
};
template<int B, typename T> constexpr T 	_log_base(T v)		{ return _log_base_result1<T, B>(v).value; }
template<int B, typename T> constexpr uint32 log_base(T v)		{ return (uint32)_log_base<B>(uint_type_tmin<T, int>(v)); }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template<typename T>inline uint32 _log_base(uint32 B, T v) {
	uint32 r;
	for (r = 0; v >= B; ++r)
		v /= B;
	return r;
}

template<typename T> inline uint32 log_base(uint32 B, T v)	{ return _log_base(B, uint_for_t<T>(v)); }


template<int B> constexpr uint32 _log_base(uint128 v) {
	return hi(v) == 0 ? log_base<B>(lo(v)) : log_base<B>(fulldivc(lo(v), hi(v), kpow<B, klog<B, ~uint64(0)>>)) + klog<B, ~uint64(0)>;
}

//-----------------------------------------------------------------------------
// isqrt and triangles
//-----------------------------------------------------------------------------

template<typename T> constexpr T isqrt_helper(T x, T r) {
	return r * r > x ? isqrt_helper(x, (r + x / r) >> 1) : r;
}
template<typename T> constexpr T isqrt(T x)		{
	typedef uint_for_t<T> U;
	return x < 2 ? x : isqrt_helper<U>(U(x), min(U(x) / 2 + 1, bits<U>(sizeof(T) * 4)));
}

//with [i,i] index
constexpr int triangle_number(int n)			{ return n * (n + 1) / 2; }
constexpr int triangle_index(int a, int b)		{ return a < b ? triangle_number(b) + a : triangle_number(a) + b; }
constexpr int triangle_row(int i)				{ return (isqrt(8 * i + 1) - 1) / 2; }
constexpr int triangle_col(int i)				{ return i - triangle_number(triangle_row(i)); }
constexpr int triangle_deindex(int n, int i)	{ int r = triangle_row(i); return r * n + i - triangle_number(r); }

//no [i,i] index
constexpr int triangle_number_excl(int n)		{ return triangle_number(n - 1); }
constexpr int triangle_index_excl(int a, int b) { return a < b ? triangle_number(b - 1) + a : triangle_number(a - 1) + b; }

}//namespace iso

#endif // DEFS_INT_H
