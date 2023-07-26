#ifndef BITS_H
#define BITS_H

#include "defs.h"

namespace iso {

template<typename T> constexpr uint32	BIT_MASK	= BIT_COUNT<T> - 1;
template<typename T> constexpr uint32	BIT_HOLD(int N)	{ return (N + BIT_COUNT<T> - 1) / BIT_COUNT<T>; }

//-----------------------------------------------------------------------------
// masks
//-----------------------------------------------------------------------------

template<typename T> constexpr bool	bit_test(T v, T e)					{ return !!(v & e); }
template<typename T> constexpr bool	bit_test_all(T v, T e)				{ return (v & e) == e; }
template<typename T> inline bool	bit_test_set(T &v, T e)				{ return !!(v & e) | ((v |= e), false); }
template<typename T> inline bool	bit_test_set(T &v, T e, bool b)		{ return !!(v & e) | ((v ^= (v ^ -T(b)) & e), false); }
template<typename T> inline bool	bit_test_clear(T &v, T e)			{ return !!(v & e) | ((v &= ~e), false); }
template<typename T> inline bool	bit_test_flip(T &v, T e)			{ return !!(v & e) | ((v ^= e), false); }

template<typename T> inline T&		bit_set(T &v, T m)					{ return v |= m; }
template<typename T> inline T&		bit_set(T &v, T m, bool b)			{ return v = (v & ~m) | (-int(b) & m); }
template<typename T> inline T&		bit_clear(T &v, T m)				{ return v &= ~m; }
template<typename T> inline T&		bit_flip(T &v, T m)					{ return v ^= m; }
template<typename T> inline T&		bit_swap(T &v, T m1, T m2)			{ bool b1 = bit_test(v, m1); bit_set(m1, bit_test(v, m2)); return bit_set(m2, b1); }

template<typename T> inline void	bit_set(T *data, int i)				{ data[i / BIT_COUNT<T>] |= bit<T>(i & BIT_MASK<T>); }
template<typename T> inline void	bit_clear(T *data, int i)			{ data[i / BIT_COUNT<T>] &= ~bit<T>(i & BIT_MASK<T>); }
template<typename T> inline bool	bit_get(const T *data, int i)		{ return !!(data[i / BIT_COUNT<T>] & bit<T>(i & BIT_MASK<T>)); }

template<typename T> constexpr T	masked_write(T a, T b, T m)			{ return (a & ~m) | (b & m); }

//template<typename T, int N>		constexpr T bit_every				= ~T(0) / kbits<T, N>;
template<typename T, int N>			constexpr T bit_every				= N >= BIT_COUNT<T> ? 1 : (kbits<T, BIT_COUNT<T> / N * N> / kbits<T, N>) | kbit<T, BIT_COUNT<T> / N * N>;
template<typename T, int N, T V>	constexpr T val_every				= bit_every<T, N> * V;

template<typename T, int S, int N>	constexpr T bit_repeat				= kbits<T, N * S> / kbits<T, S>;
template<typename T, int S, int N, T V>	constexpr T val_repeat			= bit_repeat<T, S, N> * V;

// V bits every N
template<typename T, int N, int V>	constexpr T bitblocks				= bit_every<T,N> * kbits<T, V>;

// N bits every 2N
template<typename T, int N> constexpr T chunkmask = T(~T(0)) / (bit<T>(N) + 1);

template<int X, int Y> auto block_mask(int x, int y) {
	typedef	uint_bits_t<X * Y>	T;
	return (bits<T>(x) * bit_every<T, X>) & bits<T>(y * X);
}

template<int X, int Y, int Z> auto block_mask(int x, int y, int z) {
	typedef	uint_bits_t<X * Y * Z>	T;
	return (block_mask<X, Y>(x, y) * bit_every<T, X * Y>) & bits<T>(z * X * Y);
}


//-----------------------------------------------------------------------------
// shifts/rotates
//-----------------------------------------------------------------------------

template<typename T> constexpr T	bits_mask(T m, uint32 n)		{ return (m << n) - m; }
template<typename T> constexpr T	bits_dup(T m, uint32 n)			{ return m | (m << n); }

// rotate bits from s for n
template<typename T> constexpr auto	rotate_left(T x, uint32 y, uint32 n, uint32 s = 0)		{ return masked_write(x, ((x << y) | (x >> (n - y))), bits<T>(n, s)); }
template<typename T> constexpr auto	rotate_right(T x, uint32 y, uint32 n, uint32 s = 0)		{ return rotate_left(x, n - y, n, s); }
template<typename T> constexpr auto	rotate_bits(T x, int y, uint32 n, uint32 s = 0)			{ return rotate_left(x, wrap(y, n), n, s); }

// rotate chunks of bits using m as start bits
template<typename T> constexpr T	rotate_left_mask(T x, uint32 y, T m, uint32 n)	{
	return	(x & ~bits_mask(m, n))
		|	((x & bits_mask(m, n - y)) << y)
		|	((x >> (n - y)) & bits_mask(m, y));
};

template<typename T> constexpr T	swap_bits(T x, T m, uint32 y) {
	return x ^ bits_dup((((x >> y) ^ x) & m), y);
}

template<int Y, int N, typename T> constexpr enable_if_t<N == Y * 2, T> rotate_left_mask(T x, T m)	{ return swap_bits(x, bits_mask(m, Y), Y); }
template<int Y, int N, typename T> constexpr enable_if_t<N != Y * 2, T> rotate_left_mask(T x, T m)	{ return rotate_left_mask(x, Y, m, N); }

//-----------------------------------------------------------------------------
// count/index bits
//-----------------------------------------------------------------------------

// nth set index

#ifdef __BMI2__

inline uint64 lowest_n_set(uint64 x, unsigned n) { return _pdep_u64(bits<uint64>(n), x); }
inline uint32 lowest_n_set(uint32 x, unsigned n) { return _pdep_u32(bits<uint32>(n), x); }

inline uint64 nth_set(uint64 x, unsigned n) { return _pdep_u64(bit<uint64>(n), x); }
inline uint32 nth_set(uint32 x, unsigned n) { return _pdep_u32(bit<uint32>(n), x); }

inline uint64 nth_set_index(uint64 x, unsigned n) { return lowest_set_index(nth_set(x, n)); }
inline uint32 nth_set_index(uint32 x, unsigned n) { return lowest_set_index(nth_set(x, n)); }

#else

template<int I> struct nth_set_index_s;
template<> struct nth_set_index_s<8> {
	static constexpr uint32 get1(uint8 i1, unsigned n) 						{ return n || !(i1 & 2) ? 0 : 1; }
	static constexpr uint32 get2(uint8 i1, uint8 i2, unsigned n) 			{ return n < (i2 >> 2) ? get1(i1 >> 2, n) + 2 : get1(i1, n - (i2 >> 2)); }
	static constexpr uint32 get4(uint8 i1, uint8 i2, uint8 i4, unsigned n)	{ return n < (i4 >> 4) ? get2(i1 >> 4, i2 >> 4, n) + 4 : get2(i1, i2 & 0xf, n - (i4 >> 4)); }
	template<typename T> static constexpr uint32 f(T i1, T i2, T i4, T i8, unsigned n) {
		return n < uint8(i8) ? get4(uint8(i1), uint8(i2), uint8(i4), uint8(i8) - n - 1) : get4(uint8(i1 >> 8), uint8(i2 >> 8), uint8(i4 >> 8), uint8(i8 >> 8) - n - 1) + 8;
	}
};
template<int I> struct nth_set_index_s {
	template<typename T> static constexpr uint32 f(T i1, T i2, T i4, T i8, unsigned n) {
		return n >= uint8(i8 >> (I - 8)) ? nth_set_index_s<I / 2>::f(i1 >> I, i2 >> I, i4 >> I, i8 >> I, n) + I : nth_set_index_s<I / 2>::f(i1, i2, i4, i8, n);
	}
};

template<typename T> constexpr uint32 _nth_set_index(T i1, T i2, T i4, unsigned n) {
	return nth_set_index_s<sizeof(T) * 4>::f(i1, i2, i4, ((i4 + (i4 >> 4)) & (~T(0) / 0x11)) * (~T(0) / 0xff), n);
}
template<typename T> constexpr uint32 _nth_set_index(T i1, T i2, unsigned n) {
	return _nth_set_index(i1, i2, (i2 & (~T(0) / 5)) + ((i2 >> 2) & (~T(0) / 5)), n);
}
template<typename T> constexpr enable_if_t<!is_signed<T>, uint32> nth_set_index(T i, unsigned n) {
	return _nth_set_index(i, i - ((i >> 1) & (~T(0) / 3)), n);
}

template<typename T> constexpr T lowest_n_set(T x, unsigned n) { return x & bits<T>(nth_set_index(x, n)); }

#endif

template<typename T> constexpr enable_if_t< is_signed<T>, uint32> nth_set_index(T i, unsigned n) {
	return nth_set_index(uint_for_t<T>(i), n);
}

// lowest consecutive n bits (starting at bits set in starts)
template<typename T> inline int lowest_set_index(T x, int n, T starts = ~T(0)) {
	T	m	= bits<T>(n);
	int	s;
	while ((s = lowest_set_index(x & starts)) < BIT_COUNT<T> - n) {
		if (((x >> s) & m) == m)
			return s;
		x &= -highest_set(~x & (m << s));
	}
	return -1;
}

// lowest consecutive n bits
template<typename T> inline int highest_set_index(T x, int n, T starts = ~T(0)) {
	T	m	= bits<T>(n);
	int	s;
	while ((s = highest_set_index(x)) >= n) {
		if (((x >> (s - n)) & m) == m)
			return s;
		x &= (lowest_set(~x & (m << (s - n))) - 1);
	}
	return -1;
}

template<typename T> constexpr bool bits_subset(const T &a, const T &b)			{ return !(a & ~b); }
template<typename T> constexpr bool bits_proper_subset(const T &a, const T &b)	{ return a != b && bits_subset(a, b); }

// is there a B-bit zero  (at a B-bit boundary) - cf. simd::contains_zero
template<typename T, int B = 8> inline bool bits_contains_zero(T x) {
	return (x - val_every<T, B, 0x01>) & ~x & val_every<T, B, T(1) << (B - 1)>;
}

template<typename T, typename V> inline bool contains_value(T x, V v) {
	return contains_zero<T, BIT_COUNT<V>>(x ^ (bit_every<T, BIT_COUNT<V>> * v));
}

//-----------------------------------------------------------------------------
// reverse
//-----------------------------------------------------------------------------

template<typename T, int N, int N0 = 0> struct s_reverse_bits { static inline T f(T x) {
	return s_reverse_bits<T, N / 2, N0>::f(((x & chunkmask<T,N>) << N) | ((x >> N) & chunkmask<T,N>));
} };
template<typename T, int N0> struct s_reverse_bits<T, N0, N0> { static inline T f(T x) { return x; } };

template<typename T>				inline T reverse_bits(T x)			{ return s_reverse_bits<uint_for_t<T>, sizeof(T) * 4>::f(as_unsigned(x)); }
template<int N, typename T>			inline T reverse_bits(T x)			{ return s_reverse_bits<uint_for_t<T>, N / 2>::f(as_unsigned(x)); }
template<typename T>				inline T reverse_bits(T x, int N)	{ return reverse_bits(as_unsigned(x)) >> (BIT_COUNT<T> - N); }

template<int N0, typename T>		inline T reverse_chunks(T x)		{ return s_reverse_bits<uint_for_t<T>, sizeof(T) * 4, N0>::f(as_unsigned(x)); }
template<int N0, int N, typename T> inline T reverse_chunks(T x)		{ return s_reverse_bits<uint_for_t<T>, N / 2, N0>::f(as_unsigned(x)); }
template<int N0, typename T>		inline T reverse_chunks(T x, int N)	{ return reverse_chunks<N0>(as_unsigned(x)) >> (BIT_COUNT<T> - N); }

inline uint32 reverse_bits2(uint8 i) { return s_reverse_bits<uint8, 1>::f(i); }
inline uint32 reverse_bits4(uint8 i) { return s_reverse_bits<uint8, 2>::f(i); }

template<typename T> T inc_reverse(T x) {
	T	b = highest_clear(x);
	return (x & (b - 1)) | b;
}

template<typename T> T inc_reverse(T x, int bits) {
	T	b = highest_clear(x | -(1 << bits));
	return (x & (b - 1)) | b;
}


//-----------------------------------------------------------------------------
// spread/collect bits
// 
// spread_bits(T x, T mask)		spread bits of x according to mask
// collect_bits(T x, T mask)	collect bits of x according to mask
// 
// part_bits(T)					separate each N bits by M zeros (for B bits)
// unpart_bits(T)				un-separate each N bits separated by M zeros (starting from bit O) (for B bits)
// 
// part_by_1(T) (=part_bits<1,1>(T))
// even_bits(T) (=unpart_bits<1,1>(T))
//-----------------------------------------------------------------------------

#ifdef __BMI2__

inline auto spread_bits(uint32 x, uint32 mask)	{ return _pdep_u32(x, mask); }
inline auto collect_bits(uint32 x, uint32 mask)	{ return _pext_u32(x, mask); }
inline auto spread_bits(uint64 x, uint64 mask)	{ return _pdep_u64(x, mask); }
inline auto collect_bits(uint64 x, uint64 mask)	{ return _pext_u64(x, mask); }

template<typename X, typename M> inline auto spread_bits(X x, M mask)	{ typedef uint_type_tmin<biggest_t<X, M>, uint32> T; return spread_bits((T)x, (T)mask); }
template<typename X, typename M> inline auto collect_bits(X x, M mask)	{ typedef uint_type_tmin<biggest_t<X, M>, uint32> T; return collect_bits((T)x, (T)mask); }

template<typename T> inline auto part_by_1(T x) { return spread_bits(x, bit_every<uint_bits_t<sizeof(T) * 16>, 2>); }
template<typename T> inline auto even_bits(T x) { return collect_bits(x, bit_every<T, 2>); }
template<typename T> inline auto odd_bits(T x)	{ return collect_bits(x, bit_every<T, 2> << 1); }

template<typename T> inline T interleave(T x)	{ return part_by_1(lo(x)) | (part_by_1(hi(x)) << 1); }
template<typename T> inline T uninterleave(T x) { return lo_hi(even_bits(x), odd_bits(x)); }

template<int N, int M, int B, int O=0, typename T> inline auto part_bits(T x) {
	static const int B2 = B / N * (N + M) + B % N + O;
	typedef uint_bits_t<B2> T2;
	return (T2)spread_bits(x, (bitblocks<T2, N + M, N> << O) & kbits<T2, B2>);
}
template<int N, int M, int B, int O=0, typename T> inline auto unpart_bits(T x) {
	static const int B2 = B / (N + M) * N + B % (N + M) + O;
	typedef uint_bits_t<B2> T2;
	return (T2)collect_bits(x, bitblocks<T, N + M, N> << O);
}

#else

//part_by_1(T) (=part_bits<1,1>(T))
template<typename T, int N> struct s_part_by_1		{ static constexpr T f(T x) { return s_part_by_1<T, N / 2>::f((x ^ (x << N)) & chunkmask<T,N>); } };
template<typename T> struct s_part_by_1<T,0>		{ static constexpr T f(T x) { return x; } };
template<typename T> constexpr auto part_by_1(T x)	{ return s_part_by_1<uint_t<sizeof(T) * 2>, sizeof(T) * 4>::f(x); }

//even_bits(T) (=unpart_bits<1,1>(T))
template<typename T, int N> struct s_even_bits		{ static constexpr T f(T x) { x = s_even_bits<T, N / 2>::f(x) & chunkmask<T,N>; return x | (x >> N); } };
template<typename T> struct s_even_bits<T,0>		{ static constexpr T f(T x) { return x; } };
template<typename T> constexpr auto even_bits(T x)	{ return s_even_bits<T, sizeof(T) * 2>::f(x); }
template<typename T> constexpr auto odd_bits(T x)	{ return even_bits(x >> 1); }

//interleave(T) & uninterleave(T)
template<typename T, int N> struct s_interleave {
	static constexpr T	m = chunkmask<T, N> << N;
	static constexpr T f(T x) { return s_interleave<T, N / 2>::f((x & ~(m | (m << N))) | ((x & m) << N) | ((x >> N) & m)); }
	static constexpr T g(T x) { x = s_interleave<T, N / 2>::g(x); return (x & ~(m | (m << N))) | ((x & m) << N) | ((x >> N) & m); }
};
template<typename T> struct s_interleave<T,0>	{
	static constexpr T f(T x) { return x; }
	static constexpr T g(T x) { return x; }
};
template<typename T> constexpr T interleave(T x)	{ return s_interleave<uint_for_t<T>, sizeof(T) * 2>::f(as_unsigned(x)); }
template<typename T> constexpr T uninterleave(T x)	{ return s_interleave<uint_for_t<T>, sizeof(T) * 2>::g(as_unsigned(x)); }

//part_bits(T) & unpart_bits(T)
template<typename T, int N, int M, int I, bool = (M < N)> struct s_part_bits {
	static const T	m = bitblocks<T, (N + M) << I, N << I>;
	static constexpr T f(T x) { return s_part_bits<T, N, M, I - 1>::f((x ^ (x << (M << I))) & m); }
	static constexpr T g(T x) { x = s_part_bits<T, N, M, I - 1>::g(x) & m; return x ^ (x >> (M << I)); }
};
template<typename T, int N, int M> struct s_part_bits<T, N, M, 0, false> {
	static const T	m = bitblocks<T, N + M, N>;
	static constexpr T f(T x) { return (x ^ (x << M)) & m; }
	static constexpr T g(T x) { x &= m; return x ^ (x >> M); }
};
template<typename T, int N, int M, int I> struct s_part_bits<T, N, M, I, true> {
	static const T	m0	= bitblocks<T, (N + M) << (I + 1), N << I>, m1 = m0 << (N << I);
	static constexpr T f(T x) { return s_part_bits<T, N, M, I - 1>::f((x & m0) ^ ((x & m1) << (M << I))); }
	static constexpr T g(T x) { x = s_part_bits<T, N, M, I - 1>::g(x); return (x & m0) ^ ((x >> (M << I)) & m1); }
};
template<typename T, int N, int M> struct s_part_bits<T, N, M, 0, true> {
	static const T	m0 = bitblocks<T, (N + M) * 2, N>, m1 = m0 << N;
	static constexpr T f(T x) { return (x & m0) ^ ((x & m1) << M); }
	static constexpr T g(T x) { return (x & m0) ^ ((x >> M) & m1); }
};

template<int N, int M, int B, int O=0, typename T> constexpr auto part_bits(T x) {
	return s_part_bits<uint_bits_t<B / N * (N + M) + B % N>, N, M, klog2<(B - 1) / N>>::f(x) << O;
}
template<int N, int M, int B, int O=0, typename T> constexpr auto unpart_bits(T x) {
	return (uint_bits_t<B / (N + M) * N + B % (N + M)>)s_part_bits<T, N, M, klog2<(B - 1) / N>>::g(x >> O);//M<N was forced to true?
}

template<typename T> inline auto spread_bits(T x, T mask) {
	T	t = 0;
	for (T m = mask, a, b; m && x; x /= b / a, m &= -b) {
		a	= lowest_set(m);
		b	= lowest_set(~m & -a);
		t	|= (x * a) & (b - 1);
	}
	return t;
}

template<typename T> inline auto collect_bits(T x, T mask) {
	T	t = 0;
	for (T m = mask, s = 1, a, b; m & x; s *= b / a, m &= -b) {
		a	= lowest_set(m);
		b	= lowest_set(~m & -a);
		t	|= (x & (b - a)) / a * s;
	}
	return t;
}

#endif

template<int N, int M, typename T> constexpr auto part_bits(T x) {
	return part_bits<N, M, BIT_COUNT<T>, 0, T>(x);
}
template<int N, int M, typename T> constexpr auto unpart_bits(T x) {
	return unpart_bits<N, M, (BIT_COUNT<T> + N + M - 1) / (N + M) * N, 0, T>(x);
}

template<typename T, T mask> constexpr auto spread_bits(T x)	{ return spread_bits(x, mask);  }
template<typename T, T mask> constexpr auto collect_bits(T x)	{ return collect_bits(x, mask); }

//-----------------------------------------------------------------------------
// transpose
//-----------------------------------------------------------------------------

template<int X, int Y, int N, int S> struct s_transpose;

template<int N, int S> struct s_transpose<2, 2, N, S> {
	template<typename T> static T f(T x) {
		return x ^ ((((x >>  (S - N)) ^ x) & val_every<T, S * 2, val_repeat<T, N * 2, S / (N * 2), kbits<T, N> << N>>) * (1 + (1 << (S - N))));
	}
};
template<> struct s_transpose<4, 4, 1, 4> {
	template<typename T> static T f(T x) {
		x ^= (((x >> 14) ^ x) & val_every<T, 32, 0xcccc>)	* (1 + (1 << 14));
		x ^= (((x >>  7) ^ x) & val_every<T, 16, 0xaa>)		* (1 + (1 <<  7));
		return x;
	}
};
template<int N, int S> struct s_transpose<4, 4, N, S> {
	template<typename T> static T f(T x) {
		return s_transpose<2, 2, N * 4, S>::f(s_transpose<2, 2, N, S / 2>::f(x));
	}
};
template<> struct s_transpose<8, 8, 1, 8> {
	template<typename T> static T f(T x) {
		x ^= (((x >> 28) ^ x) & val_every<T, 64, 0xf0f0f0f0>)	* (1 + (1 << 28));
		x ^= (((x >> 14) ^ x) & val_every<T, 32, 0xcccc>)		* (1 + (1 << 14));
		x ^= (((x >>  7) ^ x) & val_every<T, 16, 0xaa>)			* (1 + (1 <<  7));
		return x;
	}
};

template<int X, int Y, int N = 1, int S = X * N, typename T> T transpose(T x) {
	return s_transpose<X, Y, N, S>::f(x);
}

//-----------------------------------------------------------------------------
//	bit_container
//-----------------------------------------------------------------------------

template<typename T> struct bit_container {
	T	t;

	struct iterator {
		T	t;
		iterator(T t) : t(t) {}
		auto&	operator++()						{ t = clear_lowest(t); return *this; }
		auto	operator*()					const	{ return lowest_set_index(t); }
		bool operator!=(const iterator &b)	const	{ return t != b.t; }
	};
	bit_container(T t) : t(t) {}
	iterator	begin()	const { return t; }
	iterator	end()	const { return T(0); }
	auto		size()	const { return count_bits(t); }
};
template<typename T> bit_container<T>	make_bit_container(T t) { return t; }

//-----------------------------------------------------------------------------
//	bit_pointer, bit_reference
//-----------------------------------------------------------------------------

struct bit_address {
	intptr_t	p;
	template<typename T> constexpr T*		ptr()	const	{ return (T*)((p & ~intptr_t(BIT_MASK<T>)) >> 3); }
	template<typename T> constexpr uint32	shift()	const	{ return p & BIT_MASK<T>; }
	template<typename T> constexpr T		mask()	const	{ return bit<T>(p & BIT_MASK<T>); }
	template<typename T> constexpr T		rmask()	const	{ return -bit<T>(p & BIT_MASK<T>); }	//inclusive of this bit
	template<typename T> constexpr T		lmask() const	{ return ~T(0) >> (~p & BIT_MASK<T>); }	//inclusive of this bit
	template<typename T> constexpr T		xlmask()const	{ return bits<T>(p & BIT_MASK<T>); }	//exclusive of this bit

	bit_address(const void *p)			: p(intptr_t(p) << 3) {}
	constexpr bit_address(intptr_t p)	: p(p) {}

	bit_address&	operator+=(intptr_t a)						{ p += a; return *this; }
	bit_address&	operator-=(intptr_t a)						{ p -= a; return *this; }

	bit_address		operator+(intptr_t a)				const	{ return bit_address(p + a); }
	bit_address		operator-(intptr_t a)				const	{ return bit_address(p - a); }
	intptr_t		operator- (const bit_address& b)	const	{ return p - b.p; }
	bool			operator==(const bit_address& b)	const	{ return p == b.p; }
	bool			operator!=(const bit_address& b)	const	{ return p != b.p; }
	bool			operator< (const bit_address& b)	const	{ return p <  b.p; }
	bool			operator<=(const bit_address& b)	const	{ return p <= b.p; }
	bool			operator> (const bit_address& b)	const	{ return p >  b.p; }
	bool			operator>=(const bit_address& b)	const	{ return p >= b.p; }
};

template<typename T> struct bit_reference;

template<typename T> struct bit_pointer : bit_address {
	using bit_address::operator-;
	constexpr T*		ptr()	const	{ return bit_address::ptr<T>(); }
	constexpr uint32	shift()	const	{ return bit_address::shift<T>(); }
	constexpr T			mask()	const	{ return bit_address::mask<T>(); }
	constexpr T			rmask()	const	{ return bit_address::rmask<T>(); }
	constexpr T			lmask() const	{ return bit_address::lmask<T>(); }
	constexpr T			xlmask()const	{ return bit_address::xlmask<T>(); }

	explicit constexpr bit_pointer(bit_address p)	: bit_address(p) {}
	bit_pointer(T *p)								: bit_address(p) {}
	explicit constexpr operator bool()		const	{ return !!p; }
	auto			operator*()				const	{ return bit_reference<T>(*this); }
	auto			operator[](intptr_t i)	const	{ return *(*this + i); }
	bit_pointer		operator+(intptr_t i)	const	{ return bit_pointer(p + i); }
	bit_pointer		operator-(intptr_t i)	const	{ return bit_pointer(p - i); }
	bit_pointer&	operator+=(intptr_t i)			{ p += i; return *this; }
	bit_pointer&	operator-=(intptr_t i)			{ p -= i; return *this; }
	bit_pointer&	operator--()					{ --p; return *this; }
	bit_pointer&	operator++()					{ ++p; return *this; }
	bit_pointer		operator--(int)					{ auto t = *this; --p; return t; }
	bit_pointer		operator++(int)					{ auto t = *this; ++p; return t; }

	operator placement_helper<bit_reference<T>, bool>()	const	{ return operator*(); }

	friend bit_pointer min(bit_pointer a, bit_pointer b) { return bit_pointer(min(a.p, b.p)); }
	friend bit_pointer max(bit_pointer a, bit_pointer b) { return bit_pointer(max(a.p, b.p)); }
};

template<typename T> struct bit_reference : bit_address {
	constexpr T*	ptr()		const	{ return bit_address::ptr<T>(); }
	constexpr T		mask()		const	{ return bit_address::mask<T>(); }
public:
	explicit constexpr bit_reference(bit_address p) : bit_address(p)	{}
	operator	bool()			const	{ return !!(*ptr() & mask()); }
	auto		operator&()		const	{ return bit_pointer<T>(*this); }
	void		clear()					{ *ptr() &= ~mask(); }
	void		set()					{ *ptr() |= mask(); }
	void		flip()					{ *ptr() ^= mask(); }
	bool		test_flip()				{ return iso::test_flip(*ptr(), mask()); }
	bool		test_set()				{ return iso::test_set(*ptr(), mask()); }
	bool		test_clear()			{ return iso::test_clear(*ptr(), mask()); }
	bool		operator=(const bit_reference &b)	{ return operator=((bool)b); }
	bool		operator=(bool b)		{ auto d = ptr(); *d = masked_write(*d, T(-T(b)), mask()); return b; }
	bool		operator++()			{ set(); return true; }
	bool		operator--()			{ clear(); return false; }
	bool		operator++(int)			{ return test_set(); }
	bool		operator--(int)			{ return test_clear(); }
	friend bool	exchange(bit_reference a, bool b)	{ return iso::test_set(*a.ptr(), a.mask(), b); }
};

template<typename T> struct bit_reference<const T> : bit_address {
	explicit constexpr bit_reference(bit_address p) : bit_address(p)	{}
	operator	bool()			const	{ return !!(*ptr<T>() & mask<T>()); }
	auto		operator&()		const	{ return bit_pointer<const T>(*this); }
};

//-----------------------------------------------------------------------------
//	bitfields
//-----------------------------------------------------------------------------

template<typename T> inline auto read2(const T *p)					{ return lo_hi(p[0], p[1]); }
template<typename T> inline auto read2(const T_swap_endian<T> *p)	{ return lo_hi((T)p[1], (T)p[0]); }
template<typename T> inline auto read2(const packed<T> *p)			{ return read2((T)*p); }
template<typename T> inline auto read2(const constructable<T> *p)	{ return read2((T*)p); }

template<int S, typename T>	constexpr int	swap_endian_bit = S ^ (BIT_COUNT<T> - 8);

template<typename T>						constexpr native_endian_t<T>	extract_bits(T i, uint32 S, uint32 N)							{ return s_extend<num_traits<T>::is_signed>::mask(i >> S, N); }
template<int N, typename T>					constexpr native_endian_t<T>	extract_bits(T i, uint32 S)										{ return extract_bits(i, S, N); }
template<int S, int N, typename T>			constexpr native_endian_t<T>	extract_bits(T i)												{ return extract_bits(i, S, N); }
template<>									constexpr uint8					extract_bits<0, 8>(uint8 i)										{ return i; }
template<>									constexpr uint16				extract_bits<0, 16>(uint16 i)									{ return i; }
template<>									constexpr uint32				extract_bits<0, 32>(uint32 i)									{ return i; }
template<>									constexpr uint64				extract_bits<0, 64>(uint64 i)									{ return i; }
template<int S, int N, typename T>			constexpr enable_if_t<((S & 7) + N <= 8), T>	extract_bits(T_swap_endian<T> i)				{ return extract_bits<swap_endian_bit<S, T>, N>(i.raw()); }
template<int S, int N, typename T>			constexpr auto					extract_bits(packed<T> i)										{ return extract_bits<S, N>(T(i)); }
template<int S, int N, typename T>			constexpr auto					extract_bits(constructable<T> i)								{ return extract_bits<S, N>(T(i)); }

template<typename T>						constexpr native_endian_t<T>	copy_bits(T s, T d, uint32 S, uint32 D, uint32 N)				{ return masked_write(d, (T)shift_bits(s, int(D-S)), bits<T>(N, D)); }
template<int N, typename T>					constexpr native_endian_t<T>	copy_bits(T s, T d, uint32 S, uint32 D)							{ return masked_write(d, (T)shift_bits(s, int(D-S)), bits<T>(N, D)); }
template<int S, int D, int N, typename T>	constexpr native_endian_t<T>	copy_bits(T s, T d)												{ return masked_write(d, (T)shift_bits<D-S>(s), bits<T>(N, D)); }
template<>									constexpr uint8					copy_bits<0, 0, 8>(uint8 s, uint8 d)							{ return s; }
template<>									constexpr uint16				copy_bits<0, 0, 16>(uint16 s, uint16 d)							{ return s; }
template<>									constexpr uint32				copy_bits<0, 0, 32>(uint32 s, uint32 d)							{ return s; }
template<>									constexpr uint64				copy_bits<0, 0, 64>(uint64 s, uint64 d)							{ return s; }
template<typename T>						constexpr T						copy_bits(T s, T_swap_endian<T> d, uint32 S, uint32 D, uint32 N){ return copy_bits(s, T(d), S, BIT_COUNT<T> - D - N, N); }
template<int N, typename T>					constexpr T						copy_bits(T s, T_swap_endian<T> d, uint32 S, uint32 D)			{ return copy_bits<N>(s, T(d), S, BIT_COUNT<T> - D - N); }
template<int S, int D, int N, typename T>	constexpr enable_if_t<((S & 7) + N <= 8), T>	copy_bits(T s, T_swap_endian<T> d)				{ return copy_bits<S, swap_endian_bit<D, T>, N>(s, d.raw()); }
template<int S, int D, int N, typename T>	constexpr enable_if_t<((S & 7) + N >  8), T>	copy_bits(T s, T_swap_endian<T> d)				{ return copy_bits<S, D, N>(s, T(d)); }
template<int S, int D, int N, typename T>	constexpr T						copy_bits(T s, packed<T> d)										{ return copy_bits<S, D, N>(s, T(d)); }
template<int S, int D, int N, typename T>	constexpr T						copy_bits(T s, constructable<T> d)								{ return copy_bits<S, D, N>(s, T(d)); }

template<typename T>				inline void			write_bits(void *p, T x, uint32 D, uint32 N)	{ p = (T*)p + D / BIT_COUNT<T>; *(T*)p = copy_bits(x, *(T*)p, 0, D % BIT_COUNT<T>, N); }
template<int N, typename T>			inline void			write_bits(void *p, T x, uint32 D)				{ p = (T*)p + D / BIT_COUNT<T>; *(T*)p = copy_bits<N>(x, *(T*)p, 0, D % BIT_COUNT<T>); }
template<int D, int N, typename T>	inline void			write_bits(void *p, T x)						{ p = (T*)p + D / BIT_COUNT<T>; *(T*)p = copy_bits<0, D % BIT_COUNT<T>, N>(x, *(T*)p); }

template<typename T>				inline auto			read_bits(const T *p, uint32 S, uint32 N)		{ p += S / BIT_COUNT<T>; S %= BIT_COUNT<T>; return S + N <= BIT_COUNT<T> ? extract_bits(*p, S, N) : native_endian_t<T>(extract_bits(read2(p), S, N)); }
template<int N, typename T>			inline auto			read_bits(const T *p, uint32 S)					{ p += S / BIT_COUNT<T>; S %= BIT_COUNT<T>; return S + N <= BIT_COUNT<T> ? extract_bits<N>(*p, S) : native_endian_t<T>(extract_bits<N>(read2(p), S)); }
template<int S, int N, typename T>	inline enable_if_t<((S % BIT_COUNT<T> + N) <= BIT_COUNT<T>), native_endian_t<T>> read_bits(const T *p)	{ return extract_bits<S % BIT_COUNT<T>, N>(p[S / BIT_COUNT<T>]); }
template<int S, int N, typename T>	inline enable_if_t<((S % BIT_COUNT<T> + N) >  BIT_COUNT<T>), native_endian_t<T>> read_bits(const T *p)	{ return extract_bits<S % BIT_COUNT<T>, N>(read2(p + S / BIT_COUNT<T>)); }

template<int S, int D, typename T>			constexpr T	move_bit(T x)	{ return shift_bits(x & bit<T>(S), D - S);}
template<int S, int D, int N, typename T>	constexpr T	move_bits(T x)	{ return shift_bits(x & bits<T>(N, S), D - S); }

template<typename T>				inline auto			read_bits(bit_pointer<T> p, uint32 N)		{ return read_bits(p.ptr(), p.shift(), N); }
template<typename T>				inline void			write_bits(bit_pointer<T> p, T x, uint32 N)	{ return write_bits(p.ptr(), x, p.shift(), N); }

//-------------------------------------
// bitfield_pointer

template<typename T, typename U, int B> struct bitfield_reference;

template<typename T, typename U, int B, int P = B> struct bitfield_pointer : bit_address {
	explicit constexpr bitfield_pointer(bit_address p) : bit_address(p) {}
	constexpr auto			operator*()				const	{ return bitfield_reference<T, U, B>(p); }
	constexpr auto			operator[](intptr_t i)	const	{ return *(*this + i); }
	bitfield_pointer		operator+(intptr_t i)	const	{ return bitfield_pointer(p + i * P); }
	bitfield_pointer		operator-(intptr_t i)	const	{ return bitfield_pointer(p - i * P); }
	bitfield_pointer&		operator+=(intptr_t i)			{ p += i * P; return *this; }
	bitfield_pointer&		operator-=(intptr_t i)			{ p -= i * P; return *this; }
	bitfield_pointer&		operator++()					{ p += P; return *this; }
	bitfield_pointer&		operator--()					{ p -= P; return *this; }
	bitfield_pointer		operator++(int)					{ auto	t = *this; p += P; return t; }
	bitfield_pointer		operator--(int)					{ auto	t = *this; p -= P; return t; }
	intptr_t				operator-(const bitfield_pointer& b) const	{ return (p - b.p) / P; }
	ref_helper2<bitfield_reference<T, U, B>, T>	operator->()	const	{ return operator*(); }
	operator placement_helper<bitfield_reference<T, U, B>, T>()	const	{ return operator*(); }
};

//-------------------------------------
// bitfield_reference

template<typename T, typename U, int B> struct bitfield_reference : bit_address {
	explicit constexpr bitfield_reference(bit_address p) : bit_address(p) {}
	operator T()			const	{ return force_cast<T>(extract_bits<B>(*ptr<U>(), shift<U>())); }
	auto	operator&()		const	{ return bitfield_pointer<T, U, B>(p); }
	U		operator=(T a)	const	{ U* u = ptr<U>(); return *u = copy_bits<B>(force_cast<U>(a), *u, 0, shift<U>()); }
	U		operator&=(U a)	const	{ return *ptr<U>() &= ~((~a & kbits<U,B>) << shift<U>()); }
	U		operator|=(U a)	const	{ return *ptr<U>() |= (a & kbits<U,B>) << shift<U>(); }
	U		operator^=(U a)	const	{ return *ptr<U>() ^= (a & kbits<U,B>) << shift<U>(); }
	auto	operator->()	const	{ return make_ref_helper2(*this); }	// to help with field access

	friend constexpr T get(const bitfield_reference& a) { return a; }
	friend constexpr T put(bitfield_reference& a)		{ return a; }
};

template<typename T, typename U, int B> struct bitfield_reference<const T, U, B> : bit_address {
	explicit constexpr bitfield_reference(bit_address p) : bit_address(p) {}
	operator const T()		const	{ return force_cast<T>(extract_bits<B>(*ptr<U>(), shift<U>())); }
	auto	operator&()		const	{ return bitfield_pointer<const T, U, B>(p); }
	auto	operator->()	const	{ return make_ref_helper2(*this); }	// to help with field access

	friend constexpr const T get(const bitfield_reference& a) { return a; }
};

//-------------------------------------
// bitfield

template<typename T, int S, int N, bool REVERSE = (N < 0)> struct bitfield_base {
	enum { BITS = N };
	typedef uint_for_t<T>	U;
	static constexpr U		mask	= kbits<U, N, S>;
	T		data[BIT_HOLD<T>(S + N)];

	constexpr	U	get()	const	{ return extract_bits<S & BIT_MASK<T>, N>(data[S / BIT_COUNT<T>]); }
	void			set(U a)		{ T &u = data[S / BIT_COUNT<T>]; u = copy_bits<0, S & BIT_MASK<T>, N>(a, u); }
	U				operator&=(U a)	{ T &u = data[S / BIT_COUNT<T>]; return u &= (a << (S & BIT_MASK<T>)) | ~kbits<U, N, S & BIT_MASK<T>>; }
	U				operator|=(U a)	{ T &u = data[S / BIT_COUNT<T>]; return u |= (a << (S & BIT_MASK<T>)) &  kbits<U, N, S & BIT_MASK<T>>; }
	U				operator^=(U a)	{ T &u = data[S / BIT_COUNT<T>]; return u ^= (a << (S & BIT_MASK<T>)) &  kbits<U, N, S & BIT_MASK<T>>; }
};

template<typename T, int S, int N> struct bitfield_base<T, S, N, true> : bitfield_base<T, S, -N> {
	typedef bitfield_base<T, S, -N> B;
	using typename B::U;
	constexpr	U	get()	const	{ return reverse_bits<-N>(B::get()); }
	void			set(U a)		{ B::set(reverse_bits<-N>(a)); }
	U				operator&=(U a)	{ return B::operator&=(reverse_bits<-N>(a)); }
	U				operator|=(U a)	{ return B::operator|=(reverse_bits<-N>(a)); }
	U				operator^=(U a)	{ return B::operator^=(reverse_bits<-N>(a)); }
};

template<typename T, int S, int N> struct bitfield_holder_type : T_type<uint_t<N + (S & 7) <= 8 ? 1 : N + (S & 15) <= 16 ? 2 : N + (S & 31) <= 32 ? 4 : 8>> {};
template<typename T, int S, int N> struct bitfield_holder_type<packed<T>, S, N> : T_type<packed<typename bitfield_holder_type<T, S, N>::type>> {};

template<typename T, int S, int N, typename U = typename bitfield_holder_type<T, S, N>::type> struct bitfield : bitfield_base<U, S, N> {
	constexpr	operator T()			const	{ return T(this->get()); }
	void		operator=(T a)					{ this->set(force_cast<U>(a)); }
	auto		operator&()						{ return bitfield_pointer<T, U, N, N>(bit_address(this->data) + S); }
	auto		operator&()				const 	{ return bitfield_pointer<const T, U, N, N>(bit_address(this->data) + S); }
	auto		operator->()					{ return operator&(); }	// to help with field access
	auto		operator->()			const	{ return operator&(); }	// to help with field access

	friend constexpr T	get(const bitfield &a)	{ return a; }
	friend constexpr T	put(bitfield &a)		{ return a; }
};

template<typename T, int S, int N> struct bitfield<compact<T, N>, S, N> : bitfield<T, S, N> {
	using bitfield<T, S, N>::operator=;

	friend constexpr T	get(const bitfield &a)	{ return a; }
	friend constexpr T	put(bitfield &a)		{ return a; }
};

template<typename T, int S, int N, typename U> struct num_traits<bitfield<T, S, N, U>> : num_traits<T> {
	enum {bits = N};
};

template<typename T, int S, int N, typename U>	constexpr uint32 BIT_COUNT<bitfield<T, S, N, U>>	= N;
template<typename C, typename T, int S, int N, typename U>	constexpr uintptr_t	BIT_OFFSET(bitfield<T, S, N, U> C::*t)	{ return T_get_member_offset(t) * 8 + S; }

//-------------------------------------
// bitfield array

template<typename T, int B, int N, int S = 0, int P = B, typename U = uint_bits_t<(P & (P - 1)) == 0 ? B : B * 2>> struct bitfield_array {
	U		u[BIT_HOLD<U>(P * N + S)];
	auto			begin()						{ return bitfield_pointer<T, U, B, P>(bit_address(u) + S); }
	auto			end()						{ return begin() + N; }
	auto			begin()				const	{ return bitfield_pointer<const T, U, B, P>(bit_address(u) + S); }
	auto			end()				const	{ return begin() + N; }
	constexpr auto	operator[](int i)			{ return begin()[i]; }
	constexpr T		operator[](int i)	const	{ return begin()[i]; }
	bool			contains(bit_address e) const { return e >= begin() && e < end(); }
	intptr_t		index_of(bit_address e) const { return (e - bit_address(u) - S) / P; }
};

template<typename T, int B, int N, int S, int P, typename U> struct bitfield_array<compact<T, B>, B, N, S, P, U> : bitfield_array<T, B, N, S, P, U> {};

#ifdef ARRAY_H
template<typename T, int B, typename U> class _dynamic_bitfield_array {
	U		*data;
	size_t	curr_size, max_size;
	auto	_set_size(size_t n) { curr_size = n; }
	auto	_expand(size_t n) {
		if (curr_size + n > max_size) {
			max_size = max(curr_size + n, max_size ? max_size * 2 : 16);
			data = reallocate_move(data, BIT_HOLD<U>(B * curr_size), BIT_HOLD<U>(B * max_size));
		}
		auto	r = end();
		curr_size += n;
		return r;
	}
public:
	_dynamic_bitfield_array()								: data(0), curr_size(0), max_size(0)	{}
	_dynamic_bitfield_array(size_t n)						: data(allocate<U>(BIT_HOLD<U>(B * n))), curr_size(n), max_size(align(B * n, BIT_COUNT<U>) / B) {}
	_dynamic_bitfield_array(_dynamic_bitfield_array &&c)	: data(exchange(c.data, nullptr)), curr_size(c.curr_size), max_size(c.max_size)	{}
	~_dynamic_bitfield_array()	{ free(data); }

	void reset() {
		free(data);
		data = 0;
		max_size = curr_size = 0;
	}

	auto			begin()						{ return bitfield_pointer<T, U, B>(bit_address(data)); }
	auto			end()						{ return begin() + curr_size; }
	auto			begin()				const	{ return bitfield_pointer<const T, U, B>(bit_address(data)); }
	auto			end()				const	{ return begin() + curr_size; }
	constexpr auto	operator[](int i)			{ return begin()[i]; }
	constexpr T		operator[](int i)	const	{ return begin()[i]; }
	bool			contains(bit_address e) const { return e >= begin() && e < end(); }
	intptr_t		index_of(bit_address e) const { return (e - bit_address(data)) / B; }
};

template<typename T, int BITS, typename U = uint_bits_t<(BITS & (BITS - 1)) == 0 ? BITS : BITS * 2>> class dynamic_bitfield_array : public dynamic_mixout<_dynamic_bitfield_array<T, BITS, U>, T> {
	typedef dynamic_mixout<_dynamic_bitfield_array<T, BITS, U>, T> B;
	using B::data; using B::curr_size; using B::max_size;
public:
	dynamic_bitfield_array(dynamic_bitfield_array&&) = default;
	dynamic_bitfield_array(const dynamic_bitfield_array& c) : B(c.size())	{ copy_new_n(c.begin(), data, curr_size); }
	dynamic_bitfield_array(initializer_list<T> c)			: B(c.size())	{ copy_new_n(c.begin(), data, curr_size); }
	template<typename R, typename = is_reader_t<R>>	dynamic_bitfield_array(R &&r, size_t n)	: B(n) { for (auto &i : *this) i = r.template get<T>(); }

	dynamic_bitfield_array& reserve(size_t i) {
		if (i > max_size)
			data = reallocate_move(data, curr_size, max_size = i);
		return *this;
	}

	friend void swap(dynamic_bitfield_array &a, dynamic_bitfield_array &b) { raw_swap(a, b); }
};
#endif

//-------------------------------------
// bitfield tuples

template<int S, typename T, typename... TT>	struct bitfields0 {
	union {
		bitfield<T, S, BIT_COUNT<T>>		a;
		bitfields0<S + BIT_COUNT<T>, TT...>	b;
	};
};

template<int S, typename T>		struct bitfields0<S, T> : bitfield<T, S, BIT_COUNT<T>> {};
template<int S, typename...T>	struct bitfields0<S, type_list<T...>> : bitfields0<S, T...> {};
template<typename... TT>		using bitfields = bitfields0<0, TT...>;

template<>								static constexpr uint32 BIT_COUNT<type_list<>> = 0;
template<typename T0, typename... T>	static constexpr uint32 BIT_COUNT<type_list<T0, T...>> = BIT_COUNT<T0> + BIT_COUNT<type_list<T...>>;


//-------------------------------------
// separated bitfield

template<typename T, int S, int NS, int...B> struct bitfield_multi0 {
	static const int N = meta::abs_v<NS>;
	union {
		bitfield_base<T, S, NS>		lo;
		bitfield_multi0<T, B...>	hi;
	};
	enum { BITS = N + bitfield_multi0<T, B...>::BITS };
	typedef uint_for_t<T>	U;
	constexpr	U		get()	const	{ return lo.get() | (hi.get() << N); }
	void				set(U a)		{ lo.set(a); hi.set(a >> N); }
	U					operator&=(U a)	{ return (lo &= a) | (hi &= a >> N) << N; }
	U					operator|=(U a)	{ return (lo |= a) | (hi |= a >> N) << N; }
	U					operator^=(U a)	{ return (lo ^= a) | (hi ^= a >> N) << N; }
};

template<typename T, int S, int N> struct bitfield_multi0<T, S, N> : bitfield_base<T, S, N> {};

template<typename T, int...B> struct bitfield_multi : bitfield_multi0<T, B...> {
	typedef uint_for_t<T>	U;
	constexpr		operator U()	const				{ return this->get(); }
	void			operator=(U a)						{ this->set(a); }
	friend constexpr U	get(const bitfield_multi &a)	{ return a; }
	friend constexpr U	put(bitfield_multi &a)			{ return a; }
};

//-----------------------------------------------------------------------------
//	bit arrays
//-----------------------------------------------------------------------------

template<typename T> struct shifted_iterator {
	const T *p;
	int		shift;
	shifted_iterator(const T *p, int shift) : p(p), shift(shift) {}
	T	operator*() const {
		return (p[0] >> shift) | (p[1] << (BIT_COUNT<T> - shift));
	}
	auto&	operator++()	{ ++p; return *this; }
	auto	operator++(int) { return shifted_iterator(p++, shift); }
};

template<typename T> bit_pointer<T> bit_prev(bit_pointer<T> i, bit_pointer<T> begin, bool set) {
	if (i >= begin) {
		T	flip	= T(set) - 1;
		T*	p		= i.ptr();

		if (T t = (*p ^ flip) & i.lmask())
			return max(bit_pointer<T>(p) + highest_set_index(t), begin - 1);

		for (const T* beginp = begin.ptr(); p-- != beginp;) {
			if (T t = *p ^ flip)
				return max(bit_pointer<T>(p) + highest_set_index(t), begin - 1);
		}
	}
	return begin - 1;
}

template<typename T> bit_pointer<T> bit_next(bit_pointer<T> i, bit_pointer<T> end, bool set) {
	if (i < end) {
		T	flip	= T(set) - 1;
		T*	p		= i.ptr();

		if (auto t = (*p ^ flip) & i.rmask())
			return min(bit_pointer<T>(p) + lowest_set_index(t), end);

		for (const T* endp = end.ptr(); ++p < endp;) {
			if (T t = *p ^ flip)
				return min(bit_pointer<T>(p) + lowest_set_index(t), end);
		}
	}
	return end;
}

template<typename T> bit_pointer<T> bits_prev(bit_pointer<T> i, bit_pointer<T> begin, int n, bool set, T starts) {
	T		flip	= -T(set);
	const T	*endp	= i.ptr();
	const T	*beginp	= i.ptr();

	if (int nt = n / BIT_COUNT<T>) {
		// multiple Ts
		for (const T *p = endp; p-- > beginp + nt; ) {
			const T	*p0 = p;
			int		c	= nt;
			T		t;
			while (--c && (t = *p ^ flip) == 0)
				p--;
			if (c == 0) {
				uint32	s = p0 < endp - 1 ? lowest_set_index(((p0[1] ^ flip) & starts) | (p0 == endp - 2 ? -bit<T>(i & BIT_MASK<T>) : 0)) : 0;
				if ((t & bits<T>(BIT_COUNT<T> - (n - s) & BIT_MASK<T>)) == 0)
					return bit_pointer<T>(p0) + s;
			}
		}
	} else {
		// single T (though can straddle 2)
		T	m = i.lmask();
		for (const T *p = endp; p-- > beginp; m = ~T(0)) {
			if (T t = (*p ^ ~flip) & m) {
				int	s = highest_set_index(t, n, starts);
				if (s >= 0)
					return bit_pointer<T>(p) + s;
				if (p > beginp && (s = lowest_set_index(~t)) > 0 && ((p[-1] ^ flip) & -bit<T>(BIT_COUNT<T> - n + s)) == 0)
					return bit_pointer<T>(p) - n + s;
			}
		}
	}
	return begin - 1;
}

template<typename T> bit_pointer<T> bits_next(bit_pointer<T> i, bit_pointer<T> end, int n, bool set, T starts) {
	T		flip	= -T(set);
	const T	*endp	= (end - n).ptr();
	T		starts0	= starts;
	starts	&= ~i.lmask();

	if (int nt = n / BIT_COUNT<T>) {
		// multiple Ts
		for (const T *p = i.ptr(); p < endp; ++p, starts = starts0) {
			const T	*p0 = p;
			int		c	= nt;
			T		t;
			while (--c && (t = *p ^ flip) == 0)
				++p;
			if (c == 0) {
				uint32	s = p0 > i.ptr() ? BIT_COUNT<T> - 1 - highest_set_index((p0[-1] & starts) ^ flip) : 0;
				if ((t & bits<T>((n - s) & BIT_MASK<T>)) == 0)
					return bit_pointer<T>(p0) - s;
			}
		}
	} else {
		// single T (though can straddle 2)
		for (const T *p = i.ptr(); p < endp; ++p, starts = starts0) {
			if (T t = *p ^ ~flip) {
				int	s = lowest_set_index(t, n, starts);
				if (s >= 0)
					return bit_pointer<T>(p) + s;
				if (p < end - 1 && (s = highest_set_index(~t)) < BIT_COUNT<T> - 1 && ((p[1] ^ flip) & bits<T>(n + s - BIT_COUNT<T> + 1)) == 0)
					return bit_pointer<T>(p) + s + 1;
			}
		}
	}
	return end;
}

template<typename T> bool bits_all(bit_pointer<T> begin, bit_pointer<T> end, bool set) {
	if (begin == end)
		return true;

	--end;
	T		flip	= -T(set);
	auto	m0		= begin.rmask();
	auto	m1		= end.lmask();
	const T	*p		= begin.ptr();
	const T	*e		= end.ptr();

	if (p < e) {
		auto	all = (*p++ ^ flip) & m0;
		while (all == 0 && p < e)
			all |= *p++ ^ flip;
		return (all | ((*p ^ flip) & m1)) == 0;
	}
	return ((*p ^ flip) & (m0 & m1)) == 0;
}

template<typename T> uint32 bits_count_set(bit_pointer<T> begin, bit_pointer<T> end) {
	if (begin == end)
		return 0;

	--end;
	auto	m0	= begin.rmask();
	auto	m1	= end.lmask();
	const T	*p	= begin.ptr();
	const T	*e	= end.ptr();

	if (p == e)
		return count_bits(*p & m0 & m1);

#if 0
	uint32	total	= count_bits(*p++ & m0);
	while (p < e)
		total += count_bits(*p++);
	return total + count_bits(*p & m1);
#else
	// or together while no common bits
	uint32	total	= 0;
	auto	accum	= *p++ & m0;
	while (p < e) {
		T	x = *p++;
		T	t = accum & x;
		accum |= x;
		if (t) {
			total += count_bits(accum);
			accum = t;
		}
	}
	return total + count_bits(accum) + count_bits(*p & m1);
#endif
}

//-------------------------------------
//fills

struct mode_set {
	template<typename T> static void	f(T *a)				{ *a = ~T(0); }
	template<typename T> static void	f(T *a, T m)		{ *a |= m; }
	template<typename T> static void	c(T *a, T b)		{ *a = ~T(0); }
	template<typename T> static void	c(T *a, T b, T m)	{ *a |= m; }
};
struct mode_clear {
	template<typename T> static void	f(T *a)				{ *a = 0; }
	template<typename T> static void	f(T *a, T m)		{ *a &= ~m; }
	template<typename T> static void	c(T *a, T b)		{ *a = 0; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a &= ~m; }
};
struct mode_flip {
	template<typename T> static void	f(T *a)				{ *a = ~*a; }
	template<typename T> static void	f(T *a, T m)		{ *a ^= m; }
};

template<typename M, typename T> void bits_fill(bit_pointer<T> begin, bit_pointer<T> end) {
	if (begin != end) {
		--end;
		auto	m0	= begin.rmask();
		auto	m1	= end.lmask();
		T		*p	= begin.ptr();
		T		*e	= end.ptr();

		if (p < e) {
			M::f(p++, m0);
			while (p < e)
				M::f(p++);
			m0 = m1;
		}
		M::f(p, m0 & m1);
	}
}

template<typename T> void bits_fill(bit_pointer<T> begin, bit_pointer<T> end, bool set) {
	if (set)
		bits_fill<mode_set>(begin, end);
	else
		bits_fill<mode_clear>(begin, end);
}

//-------------------------------------
//compares

struct mode_equal {
	template<typename T> static bool	f(T a, T b)			{ return a == b; }
	template<typename T> static bool	f(T a, T b, T m)	{ return (a & m) == (b & m); }
};
struct mode_subset {
	template<typename T> static bool	f(T a, T b)			{ return !(a & ~b); }
	template<typename T> static bool	f(T a, T b, T m)	{ return !((a & m) & ~b); }
};

template<typename M, typename T, typename I> bool bits_compare(bit_pointer<T> begin0, bit_pointer<T> end0, I i1) {
	if (begin0 == end0)
		return true;

	--end0;
	T		*p0	= begin0.ptr();
	T		*e0	= end0.ptr();
	auto	m0	= begin0.rmask();
	auto	m1	= end0.lmask();

	if (p0 < e0) {
		if (!M::f(*p0++, *i1++, m0))
			return false;
		while (p0 < e0) {
			if (!M::f(*p0++, *i1++))
				return false;
		}
		m0 = m1;
	}
	return M::f(*p0, *i1, m0 & m1);
}

template<typename M, typename T> bool bits_compare(bit_pointer<T> begin0, bit_pointer<T> end0, bit_pointer<T> begin1) {
	int	shift	= (begin1 - begin0) & BIT_MASK<T>;
	return shift
		? bits_compare<M>(begin0, end0, shifted_iterator<T>(begin1.ptr(), shift))
		: bits_compare<M>(begin0, end0, begin1.ptr());
}

//-------------------------------------
//copies

struct mode_copy {
	template<typename T> static void	c(T *a, T b)		{ *a = b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a = masked_write(*a, b, m); }
};
struct mode_not {
	template<typename T> static void	c(T *a, T b)		{ *a = ~b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a = masked_write(*a, ~b, m); }
};
struct mode_and {
	template<typename T> static T		c(T  a, T b)		{ return a & b; }
	template<typename T> static void	c(T *a, T b)		{ *a &= b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a &= b | ~m; }
};
struct mode_or {
	template<typename T> static T		c(T  a, T b)		{ return a | b; }
	template<typename T> static void	c(T *a, T b)		{ *a |= b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a |= b & m; }
};
struct mode_xor {
	template<typename T> static T		c(T  a, T b)		{ return a ^ b; }
	template<typename T> static void	c(T *a, T b)		{ *a ^= b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a ^= b & m; }
};
struct mode_andnot {
	template<typename T> static T		c(T  a, T b)		{ return a & ~b; }
	template<typename T> static void	c(T *a, T b)		{ *a &= ~b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a &= ~(b & m); }
};
struct mode_ornot {
	template<typename T> static T		c(T  a, T b)		{ return a | ~b; }
	template<typename T> static void	c(T *a, T b)		{ *a |= ~b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a |= ~b & m; }
};
struct mode_xornot {
	template<typename T> static T		c(T  a, T b)		{ return a ^ ~b; }
	template<typename T> static void	c(T *a, T b)		{ *a ^= ~b; }
	template<typename T> static void	c(T *a, T b, T m)	{ *a ^= ~b & m; }
};

template<typename M, typename T, typename I1, typename I2> void bits_copy(bit_pointer<T> begin0, bit_pointer<T> end0, I1 i1, I2 i2) {
	if (begin0 != end0) {
		--end0;
		T		*p0	= begin0.ptr();
		T		*e0	= end0.ptr();
		auto	m0	= begin0.rmask();
		auto	m1	= end0.lmask();

		if (p0 < e0) {
			*p0 = masked_write(*p0, M::c(*i1++, *i2++), m0);
			++p0;
			while (p0 < e0)
				*p0++ = M::c(*i1++, *i2++);
			m0 = m1;
		}
		*p0 = masked_write(*p0, M::c(*i1, *i2), m0 & m1);
	}
}

template<typename M, typename T, typename S, typename I1> void bits_copy(bit_pointer<T> begin0, bit_pointer<T> end0, I1 i1, bit_pointer<S> begin2) {
	if (int shift = (begin2 - begin0) & BIT_MASK<T>)
		bits_copy<M>(begin0, end0, i1, shifted_iterator<S>(begin2.ptr(), shift));
	else
		bits_copy<M>(begin0, end0, i1, begin2.ptr());
}

template<typename M, typename T, typename S> void bits_copy(bit_pointer<T> begin0, bit_pointer<T> end0, bit_pointer<S> begin1, bit_pointer<S> begin2) {
	if (int shift = (begin1 - begin0) & BIT_MASK<T>)
		bits_copy<M>(begin0, end0, shifted_iterator<S>(begin1.ptr(), shift), begin2);
	else
		bits_copy<M>(begin0, end0, begin1.ptr(), begin2);
}

template<typename M, typename T, typename I> void bits_copy(bit_pointer<T> begin0, bit_pointer<T> end0, I i1) {
	if (begin0 != end0) {
		--end0;
		T		*p0	= begin0.ptr();
		T		*e0	= end0.ptr();
		auto	m0	= begin0.rmask();
		auto	m1	= end0.lmask();

		if (p0 < e0) {
			M::c(p0++, *i1++, m0);
			while (p0 < e0)
				M::c(p0++, *i1++);
			m0 = m1;
		}
		M::c(p0, *i1, m0 & m1);
	}
}
template<typename M, typename T, typename S> void bits_copy(bit_pointer<T> begin0, bit_pointer<T> end0, bit_pointer<S> begin1) {
	if (int shift = (begin1 - begin0) & BIT_MASK<T>)
		bits_copy<M>(begin0, end0, shifted_iterator<S>(begin1.ptr() - int(begin0.mask() > begin1.mask()), shift));
	else
		bits_copy<M>(begin0, end0, begin1.ptr());
}

template<typename T> inline	void move_n(bit_pointer<T> begin, bit_pointer<T> dest, size_t n) {
	bits_copy<mode_copy>(dest, dest + n, begin);
}

template<typename T> inline	void copy_new_n(bit_pointer<T> begin, bit_pointer<T> dest, size_t n) {
	bits_copy<mode_copy>(dest, dest + n, begin);
}

//-----------------------------------------------------------------------------
//	class where_container<T> - to iterate over set/unset bits
//	class where_container2<T> - to iterate over groups of set/unset bits
//-----------------------------------------------------------------------------

template<typename T> class where_container {
	bit_pointer<T>	a, b;
	bool			set;
public:
	class iterator {
		bit_pointer<T>	a, b, i;
		bool			set;
	public:
		constexpr iterator(bit_pointer<T> a, bit_pointer<T> b, bit_pointer<T> i, bool set) : a(a), b(b), i(i), set(set) {}
		iterator&		operator++()							{ i = bit_next(i + 1, b, set); return *this; }
		iterator&		operator--()							{ i = bit_prev(i - 1, a, set); return *this; }
		constexpr int	operator*()						const	{ return i - a; }
		constexpr bool	operator==(const iterator &b)	const	{ return i == b.i; }
		constexpr bool	operator!=(const iterator &b)	const	{ return i != b.i; }
	};

	constexpr where_container(bit_pointer<T> begin, bit_pointer<T> end, bool set) : a(begin), b(end), set(set)	{}
	constexpr iterator	begin() const { return iterator(a, b, bit_next(a, b, set), set); }
	constexpr iterator	end()	const { return iterator(a, b, b, set); }
};

template<typename T> where_container<T>		make_where_container(bit_pointer<T> begin, bit_pointer<T> end, bool set) { return { begin, end, set }; }

template<typename T> class where_container2 {
	bit_pointer<T>	a, b;
	int				n;
	bool			set;
	T				starts;
public:
	class iterator {
		bit_pointer<T>	a, b, i;
		int				n;
		bool			set;
		T				starts;
	public:
		constexpr iterator(bit_pointer<T> a, bit_pointer<T> b, bit_pointer<T> i, int n, bool set, T starts) : a(a), b(b), i(i), n(n), set(set), starts(starts) {}
		iterator&		operator++()							{ i = bits_next(i + 1, b, n, set, starts); return *this; }
		iterator&		operator--()							{ i = bits_prev(i - 1, a, n, set, starts); return *this; }
		constexpr int	operator*()						const	{ return i - a; }
		constexpr bool	operator==(const iterator &b)	const	{ return i == b.i; }
		constexpr bool	operator!=(const iterator &b)	const	{ return i != b.i; }
	};

	constexpr where_container2(bit_pointer<T> begin, bit_pointer<T> end, int n, bool set, T starts) : a(begin), b(end),  n(n), set(set), starts(starts)	{}
	constexpr iterator	begin() const { return iterator(a, b, bits_next(a, b, n, set, starts), n, set, starts); }
	constexpr iterator	end()	const { return iterator(a, b, b, n, set, starts); }
};

template<typename T> where_container2<T>	make_where_container(bit_pointer<T> begin, bit_pointer<T> end, int n, bool set, T starts) { return { begin, end, n, set, starts }; }

//-----------------------------------------------------------------------------
//	mixin class bits_mixout
// A must provide: begin, end
//-----------------------------------------------------------------------------

template<typename A> class bits_mixout : public A {
public:
	typedef decltype(declval<A>().begin().mask())	T;
	using A::A; using A::begin; using A::end;
	operator			auto()							{ return begin(); }
	operator			auto()					const	{ return begin(); }
	constexpr auto		size()					const	{ return end() - begin(); }
	constexpr uint32	size32()				const	{ return (uint32)size(); }
	constexpr auto		raw()					const	{ return make_range_n(begin().ptr(), BIT_HOLD<T>(size())); }
	auto				raw()							{ return make_range_n(begin().ptr(), BIT_HOLD<T>(size())); }

	constexpr auto		slice(int i, int n)				{ return iso::slice(*this, i, n); }
	constexpr auto		slice(int i, int n)		const	{ return iso::slice(*this, i, n); }

	constexpr bool		contains(bit_address e)	const	{ return e >= begin() && e < end(); }
	constexpr intptr_t	index_of(bit_address e)	const	{ return e - begin(); }

	constexpr auto		operator[](int i)				{ ISO_ASSERT(i >= 0 && i < size()); return begin()[i]; }
	constexpr auto		operator[](int i)		const	{ return begin()[i]; }
	constexpr auto		where(bool set)			const	{ return make_where_container(begin(), end(), set); }
	constexpr auto		where(int n, bool set, T starts = ~T(0))	const	{ return make_where_container(begin(), end(), n, set, starts); }

	//single bits
	constexpr bool	test(int i)					const	{ return begin()[i]; }
	constexpr void	set(int i)							{ return begin()[i].set(); }
	constexpr void	clear(int i)						{ return begin()[i].clear(); }
	int				lowest(bool set)			const	{ return bit_next(begin(), end(), set) - begin(); }
	int				highest(bool set)			const	{ return bit_prev(end() - 1, begin(), set) - begin(); }
	int				next(int i, bool set)		const	{ return bit_next(begin() + i, end(), set) - begin(); }
	int				prev(int i, bool set)		const	{ return bit_prev(begin() + i, begin(), set) - begin(); }

	//multiple bits
	bool			all(bool set)				const	{ return bits_all(begin(), end(), set); }
	uint32			count_set()					const	{ return bits_count_set(begin(), end()); }
	uint32			count_clear()				const	{ return size() - count_set(); }
	void			set_all()							{ bits_fill<mode_set>(begin(), end()); }
	void			clear_all()							{ bits_fill<mode_clear>(begin(), end()); }
	int				lowest(int n, bool set, T starts = ~T(0))		const	{ return bits_next(begin(), end(), n, set, starts) - begin(); }
	int				highest(int n, bool set, T starts = ~T(0))		const	{ return bits_prev(end(), begin(), n, set, starts) - begin(); }
	int				next(int i, int n, bool set, T starts = ~T(0))	const	{ return bits_next(begin() + i, end(), n, set, starts) - begin(); }
	int				prev(int i, int n, bool set, T starts = ~T(0))	const	{ return bits_prev(begin() + i, begin(), n, set, starts) - begin(); }
	
	_not<const bits_mixout&>	operator~()		const	{ return *this; }

	template<typename B> auto&	operator= (const bits_mixout<B> &b)			{ auto AN = size(); auto BN = b.size();		ISO_ASSERT(AN>=BN); bits_copy<mode_copy	 >(begin(), begin() + min(AN, BN), b.begin()); if (AN > BN) slice(BN, AN - BN).clear_all(); return *this; }
	template<typename B> auto&	operator&=(const bits_mixout<B> &b)			{ auto AN = size(); auto BN = b.size();							bits_copy<mode_and	 >(begin(), begin() + min(AN, BN), b.begin()); return *this; }
	template<typename B> auto&	operator|=(const bits_mixout<B> &b)			{ auto AN = size(); auto BN = b.size();		ISO_ASSERT(AN>=BN); bits_copy<mode_or	 >(begin(), begin() + min(AN, BN), b.begin()); return *this; }
	template<typename B> auto&	operator^=(const bits_mixout<B> &b)			{ auto AN = size(); auto BN = b.size();		ISO_ASSERT(AN>=BN); bits_copy<mode_xor	 >(begin(), begin() + min(AN, BN), b.begin()); return *this; }
	template<typename B> auto&	operator-=(const bits_mixout<B> &b)			{ auto AN = size(); auto BN = b.size();							bits_copy<mode_andnot>(begin(), begin() + min(AN, BN), b.begin()); return *this; }

	template<typename B> auto&	operator= (_not<const bits_mixout<B>&> b)	{ auto AN = size(); auto BN = b.t.size();	ISO_ASSERT(AN>=BN); bits_copy<mode_not	 >(begin(), begin() + min(AN, BN), b.t.begin());if (AN > BN) slice(BN, AN - BN).set_all(); return *this; }
	template<typename B> auto&	operator|=(_not<const bits_mixout<B>&> b)	{ auto AN = size(); auto BN = b.t.size();	ISO_ASSERT(AN>=BN); bits_copy<mode_ornot >(begin(), begin() + min(AN, BN), b.t.begin()); return *this; }
	template<typename B> auto&	operator^=(_not<const bits_mixout<B>&> b)	{ auto AN = size(); auto BN = b.t.size();	ISO_ASSERT(AN>=BN); bits_copy<mode_xornot>(begin(), begin() + min(AN, BN), b.t.begin()); return *this; }

	template<typename X> auto&	operator*=(const X &b)						{ return operator&=(b); }
	template<typename X> auto&	operator+=(const X &b)						{ return operator|=(b); }

	template<typename B> bool	operator==(const bits_mixout<B> &b)	const {
		auto AN = size(), BN = b.size();
		return bits_compare<mode_equal>(begin(), begin() + min(AN, BN), b.begin()) && (
			AN < BN ? b.slice(AN, BN - AN).all(false) : slice(BN, AN - BN).all(false)
		);
	}
	template<typename B> bool	operator<=(const bits_mixout<B> &b)	const {
		auto AN = size(), BN = b.size();
		return bits_compare<mode_subset>(begin(), begin() + min(AN, BN), b.begin()) && (
			AN < BN || slice(BN, AN - BN).all(false)
		);
	}
	template<typename B> bool	operator< (const bits_mixout<B> &b)	const {
		auto AN = size(), BN = b.size();
		return bits_compare<mode_subset>(begin(), begin() + min(AN, BN), b.begin()) && (
			bits_compare<mode_equal>(begin(), begin() + min(AN, BN), b.begin())
				? (AN < BN && !b.slice(AN, BN - AN).all(false))
				: (AN < BN ||  slice(BN, AN - BN).all(false))
		);
	}
	template<typename B> bool	operator!=(const bits_mixout<B> &b)	const	{ return !(*this == b); }
	template<typename B> bool	operator> (const bits_mixout<B> &b)	const	{ return b <  *this; }
	template<typename B> bool	operator>=(const bits_mixout<B> &b)	const	{ return b <= *this; }

	friend auto num_elements(const bits_mixout& b) { return b.size(); }
};

//-----------------------------------------------------------------------------
//	range<bit_pointer<T>>
//-----------------------------------------------------------------------------

template<typename T> class _bitrange {
protected:
	bit_pointer<T>	a, b;
public:
	constexpr _bitrange(bit_pointer<T> a, bit_pointer<T> b) : a(a), b(b) {}
	constexpr auto	begin()		const	{ return a; }
	constexpr auto	end()		const	{ return b; }
};

template<typename T> class range<bit_pointer<T>> : public bits_mixout<_bitrange<T>> {
public:
	struct range_assign : range {
		range_assign(const range &r) : range(r) {}
		template<typename C>range_assign &operator=(const C &c) {
			bits_copy<mode_copy>(this->a, this->b, c.begin());
			return *this;
		}
	};
	constexpr range(bit_pointer<T> a, bit_pointer<T> b) : bits_mixout<_bitrange<T>>(a, b) {}
	constexpr range_assign	operator*()	const	{ return *this;	}
};

//-----------------------------------------------------------------------------
//	class bitarray<T>
//-----------------------------------------------------------------------------

template<int N, typename T> class _bitarray {
protected:
	T	data[BIT_HOLD<T>(N)];
public:
	constexpr bit_pointer<const T>	begin()	const	{ return data; }
	constexpr bit_pointer<const T>	end()	const	{ return begin() + N; }
	constexpr bit_pointer<T>		begin()			{ return data; }
	constexpr bit_pointer<T>		end()			{ return begin() + N; }
	operator range<bit_pointer<T>>()		const	{ return {begin(), end()}; }
};

template<int N, typename T = uint32> class bitarray : public bits_mixout<_bitarray<N, T>> {
	typedef	bits_mixout<_bitarray<N, T>>	B;
public:
	using B::operator=;
	bitarray()						{ iso::clear(B::data); }
	bitarray(_not<const B&> b)		{ bits_copy<mode_not>(B::begin(), B::end(), b.t.begin()); }

	friend bitarray	operator&(const bitarray &a, const bitarray &b)	{ bitarray d; bits_copy<mode_and	>(d.begin(), a.begin(), b.begin()); return d; }
	friend bitarray	operator|(const bitarray &a, const bitarray &b)	{ bitarray d; bits_copy<mode_or		>(d.begin(), a.begin(), b.begin()); return d; }
	friend bitarray	operator^(const bitarray &a, const bitarray &b)	{ bitarray d; bits_copy<mode_xor	>(d.begin(), a.begin(), b.begin()); return d; }
	friend bitarray	operator-(const bitarray &a, const bitarray &b)	{ bitarray d; bits_copy<mode_andnot	>(d.begin(), a.begin(), b.begin()); return d; }

	friend bitarray	operator|(const bitarray &a, _not<const B&> b)	{ bitarray d; bits_copy<mode_ornot	>(d.begin(), a.begin(), b.t.begin()); return d; }
	friend bitarray	operator^(const bitarray &a, _not<const B&> b)	{ bitarray d; bits_copy<mode_xornot	>(d.begin(), a.begin(), b.t.begin()); return d; }

	//template<typename X> friend bitarray	operator+(const bitarray &a, const X &b)	{ return *a | b; }
	//template<typename X> friend bitarray	operator*(const bitarray &a, const X &b)	{ return *a & b; }
};

//-----------------------------------------------------------------------------
//	class dynamic_bitarray<T>
//-----------------------------------------------------------------------------

template<typename T> class _dynamic_bitarray {
protected:
	T*	data;
	int	N;

	auto	_expand(size_t n) {
		if (BIT_HOLD<T>(N + n) > BIT_HOLD<T>(N))
			data	= reallocate_move(data, BIT_HOLD<T>(N), BIT_HOLD<T>(N + n));
		auto	r = end();
		N += n;
		return r;
	}
	auto	_set_size(size_t n) {
		N = n;
	}
public:
	constexpr bit_pointer<const T>	begin()	const	{ return data; }
	constexpr bit_pointer<const T>	end()	const	{ return begin() + N; }
	constexpr bit_pointer<T>		begin()			{ return data; }
	constexpr bit_pointer<T>		end()			{ return begin() + N; }

	_dynamic_bitarray()								: data(0), N(0)								{}
	_dynamic_bitarray(int N)						: data(allocate<T>(BIT_HOLD<T>(N))), N(N)	{}
	_dynamic_bitarray(const _dynamic_bitarray &b)	: _dynamic_bitarray(b.N)					{ bits_copy<mode_copy>(begin(), end(), b.begin()); }
	_dynamic_bitarray(_dynamic_bitarray &&b)		: data(exchange(b.data, nullptr)), N(b.N)	{}
	template<typename C, typename=enable_if_t<has_begin_v<C>>> 		_dynamic_bitarray(C &&c)	: _dynamic_bitarray(num_elements(c))	{ bits_copy<mode_copy>(begin(), end(), b.begin()); }

	~_dynamic_bitarray()							{ deallocate(data, BIT_HOLD<T>(N)); }
	
	operator range<bit_pointer<const T>>() const	{ return {begin(), end()}; }
};

template<typename T = uint32> class dynamic_bitarray : public bits_mixout<_dynamic_bitarray<T>> {
	typedef bits_mixout<_dynamic_bitarray<T>>	B;
	using B::N;

	T*		grow0(int N1) {
		ISO_ASSERT(N1 > N);
		return BIT_HOLD<T>(N1) > BIT_HOLD<T>(N) ? exchange(B::data, allocate<T>(BIT_HOLD<T>(N1))) : nullptr;
	}
	void	grow1(int N1) {
		if (auto old = grow0(N1)) {
			bits_copy<mode_copy>(begin(), end(), old);
			deallocate(old, BIT_HOLD<T>(N));
		}
	}
	void	resize0(int N1) {
		if (N1 > N) {
			if (auto old = grow0(N1))
				deallocate(old, BIT_HOLD<T>(N));
		}
		N	= N1;
	}
	template<typename M, typename MX, typename B> void	combine(const B& b) {
		auto AN = this->size(), BN = b.size();
		if (BN > AN) {
			if (auto old = grow0(BN)) {
				bits_copy<M>(begin(), end(), old, b.begin());
				deallocate(old, BIT_HOLD<T>(AN));
			} else {
				bits_copy<M>(begin(), end(), b.begin());
			}
			N	= BN;
			bits_copy<MX>(begin() + AN, end(), b.begin() + AN);
		} else {
			bits_copy<M>(begin(), begin() + BN, b.begin());
		}
	}
	template<typename M, typename MX, typename B> static dynamic_bitarray	combine2(const dynamic_bitarray &a, const B& b) {
		auto AN = a.size(), BN = b.size(), CN = min(AN, BN);
		dynamic_bitarray r(max(AN, BN));
		bits_copy<M>(r.begin(), r.begin() + CN, a.begin(), b.begin());
		bits_copy<MX>(r.begin() + CN, r.end(), (AN < BN ? b.begin() : a.begin()) + CN);
		return r;
	}

public:
	using B::B;
	using B::begin; using B::end; using B::size; using B::data;

	dynamic_bitarray(dynamic_bitarray&&)		= default;
	dynamic_bitarray(const dynamic_bitarray&)	= default;
	dynamic_bitarray(int N, bool set)										: dynamic_bitarray(N)			{ bits_fill(begin(), end(), set); }
	template<typename B> dynamic_bitarray(const bits_mixout<B> &b)			: dynamic_bitarray(b.size())	{ bits_copy<mode_copy>(begin(), end(), b.begin()); }
	template<typename B> dynamic_bitarray(_not<const bits_mixout<B>&> b)	: dynamic_bitarray(b.t.size())	{ bits_copy<mode_not>(begin(), end(), b.t.begin()); }
	template<typename R, typename = is_reader_t<R>>	dynamic_bitarray(R &&r, int N)	: dynamic_bitarray(N)	{ read(r); }
	template<typename C, typename=enable_if_t<has_begin_v<C>>> dynamic_bitarray(C &&c)	: dynamic_bitarray(num_elements(c))	{
		T	*p	= this->data;
		T	t	= bit<T>(BIT_COUNT<T> - 1);
		for (bool i : c) {
			T	prev = exchange(t, (t >> 1) | (T(i) << (BIT_COUNT<T> - 1)));
			if (prev & 1)
				*p++ = exchange(t, bit<T>(BIT_COUNT<T> - 1));
		}
		if (t != bit<T>(BIT_COUNT<T> - 1))
			*p = t >> (lowest_set_index(t) + 1);
	}

	explicit operator bool() const { return !!data; }

	template<typename R> bool	read(R&& r) {
		return check_readbuff(r, data, (N + 7) / 8);
	}
	template<typename R> bool	read(R&& r, size_t size) {
		resize(size);
		return read(r);
	}
	template<typename W> bool	write(W&& w) const {
		return N == 0 || check_writebuff(w, data, (N + 7) / 8);
	}

	auto&	resize(int N1, bool set = false) {
		if (N1 > N) {
			grow1(N1);
			bits_fill(end(), begin() + N1, set);
		}
		N = N1;
		return *this;
	}
	auto&	grow(int N1, bool set = false) {
		if (N1 > N) {
			grow1(N1);
			bits_fill(end(), begin() + N1, set);
			N = N1;
		}
		return *this;
	}

	bool	back()			const	{ return test(N - 1); }
	bool	pop_back()				{ --N; }
	bool	pop_back_value()		{ return test(--N); }

	auto&						operator=(dynamic_bitarray &&b)				{ N = b.N; swap(data, b.data); return *this; }
	auto&						operator=(const dynamic_bitarray &b)		{ resize0(b.N); bits_copy<mode_copy>(begin(), end(), b.begin()); return *this; }

	template<typename B> auto&	operator= (const bits_mixout<B> &b)			{ resize0(b.N); bits_copy<mode_copy>(begin(), end(), b.begin()); return *this; }
	template<typename B> auto&	operator&=(const bits_mixout<B> &b)			{ N = min(size(), b.size()); bits_copy<mode_and>(begin(), end(), b.begin()); return *this; }
	template<typename B> auto&	operator|=(const bits_mixout<B> &b)			{ combine<mode_or, mode_copy>(b); return *this; }
	template<typename B> auto&	operator^=(const bits_mixout<B> &b)			{ combine<mode_xor, mode_copy>(b); return *this; }
	template<typename B> auto&	operator-=(const bits_mixout<B> &b)			{ bits_copy<mode_andnot>(begin(), begin() + min(size(), b.size()), b.begin()); return *this; }

	template<typename B> auto&	operator= (_not<const bits_mixout<B>&> b)	{ resize0(b.N); bits_copy<mode_not >(begin(), end(), b.t.begin()); return *this; }
	template<typename B> auto&	operator|=(_not<const bits_mixout<B>&> b)	{ combine<mode_ornot, mode_set>(b.t);	return *this; }
	template<typename B> auto&	operator^=(_not<const bits_mixout<B>&> b)	{ combine<mode_xornot, mode_not>(b.t);	return *this; }

	template<typename X> auto&	operator*=(const X &b)						{ return *this &= b; }
	template<typename X> auto&	operator+=(const X &b)						{ return *this |= b; }

	template<typename B> friend auto	operator|(const dynamic_bitarray& a, const bits_mixout<B> &b)	{ return combine2<mode_or, mode_copy>(a, b); }
	template<typename B> friend auto	operator^(const dynamic_bitarray& a, const bits_mixout<B> &b)	{ return combine2<mode_xor, mode_copy>(a, b); }
	template<typename B> friend auto	operator&(const dynamic_bitarray& a, const bits_mixout<B>& b) {
		dynamic_bitarray r(min(a.size(), b.size()));
		bits_copy<mode_and>(r.begin(), r.end(), a.begin(), b.begin());
		return r;
	}
	template<typename B> friend auto	operator-(const dynamic_bitarray& a, const bits_mixout<B> &b) {
		auto AN = a.size(), BN = b.size();
		dynamic_bitarray r(AN);
		bits_copy<mode_andnot>(r.begin(), r.begin() + min(AN, BN), a.begin(), b.begin());
		if (AN > BN)
			bits_copy<mode_copy>(r.begin() + BN, r.end(), a.begin() + BN);
		return r;
	}
	template<typename B> friend auto	operator|(const dynamic_bitarray& a, _not<const bits_mixout<B>&> b)		{ return combine2<mode_ornot, mode_set>(a, b.t); }
	template<typename B> friend auto	operator^(const dynamic_bitarray& a, _not<const bits_mixout<B>&> b)		{ return combine2<mode_xornot, mode_not>(a, b.t); }

	template<typename X> friend auto	operator+(const dynamic_bitarray &a, const X &b)	{ return a | b; }
	template<typename X> friend auto	operator*(const dynamic_bitarray &a, const X &b)	{ return a & b; }
};

#ifdef ARRAY_H
template<typename T = uint32> struct dynamic_bitarray2 : dynamic_mixout<dynamic_bitarray<T>, bool> {
	typedef dynamic_bitarray<T>		B0;
	typedef dynamic_mixout<B0, bool>	B;
	using B::B;
	using B0::resize;
	using B0::operator=;
	dynamic_bitarray2(dynamic_bitarray2&&)=default;
};
#endif

//-----------------------------------------------------------------------------
//	bitmatrix
//-----------------------------------------------------------------------------

template<int R, int C, typename T> class bitmatrix {
	T	data[BIT_HOLD<T>(R * C)];
public:
	auto	col(int i)			const	{ return make_range_n(bit_pointer<T>(data + BIT_HOLD<T>(R) * i), R); }
	auto	operator[](int i)	const	{ return col(i); }

	auto operator*(range<bit_pointer<T>> b) const {
		bitarray<C,T>	a;
		for (auto i : b.where(true))
			a += cow(i);
		return a;
	}
};

template<int R, int X, int C, typename T> auto operator*(const bitmatrix<R, X, T> &a, const bitmatrix<X, C, T> &b) {
	bitmatrix<R, C, T>	r;
	for (int i = 0; i < C; i++)
		*r[i] = a * b[i];
	return r;
}


template<typename T> struct bitmatrix_aligned {
	T		*p;
	int		r, c;
	bitmatrix_aligned() : p(0), r(0), c(0) {}
	bitmatrix_aligned(T *p, int r, int c) : p(p), r(r), c(c) {}

	auto	data()				const	{ return p; }
	auto	row_size()			const	{ return BIT_HOLD<T>(c); }
	auto	num_rows()			const	{ return r; }
	auto	num_cols()			const	{ return c; }
	auto	row(int i)			const	{ return make_range_n(bit_pointer<T>(p + BIT_HOLD<T>(c) * i), c); }
	auto	operator[](int i)	const	{ return row(i); }

	// warshall
	void transitive_closure() {
		for (int i = 0; i < r; i++) {
			for (int j = 0; j < r; ++j) {
				if (row(j)[i])
					row(j) += row(i);
			}
		}
	}
	void reflexive_transitive_closure() {
		transitive_closure();
		for (int i = 0; i < r; i++)
			row(i).set(i);
	}


	auto operator*(range<bit_pointer<T>> b) const {
		dynamic_bitarray<T>	a(r, false);
		for (auto i : b.where(true))
			a += row(i);
		return a;
	}
};

template<typename T> struct bitmatrix_aligned_own : bitmatrix_aligned<T>  {
	typedef bitmatrix_aligned<T> B;
	bitmatrix_aligned_own() {}
	bitmatrix_aligned_own(int r, int c) : bitmatrix_aligned<T>(allocate<T>(BIT_HOLD<T>(c) * r), r, c) {
		memset(B::p, 0, BIT_HOLD<T>(c) * r * sizeof(T));
	}
	bitmatrix_aligned_own(bitmatrix_aligned_own &&b) : B(b) {
		b.p = nullptr;
	}
	bitmatrix_aligned_own& operator=(bitmatrix_aligned_own &&b) {
		swap((B&)*this, (B&)b);
		return *this;
	}
	~bitmatrix_aligned_own() {
		aligned_free(B::p);
	}
	bitmatrix_aligned_own&	init(int r, int c) {
		return *this = bitmatrix_aligned_own(r, c);
	}
};

template<typename T> auto operator*(const bitmatrix_aligned<T> &a, const bitmatrix_aligned<T> &b) {
	bitmatrix_aligned_own<T>	r(a.num_cols(), b.num_rows());
	for (int i = 0; i < b.num_rows(); i++)
		*r[i] = a * b[i];
	return r;
}


//-----------------------------------------------------------------------------
// BCD
//-----------------------------------------------------------------------------

//from_bcd(T)
template<typename T, int N, int S, T P> struct s_bcd { static inline T f(T x) {
	return s_bcd<T, N * 2, S, P * P>::f((x & chunkmask<T, N>) + ((x >> N) & chunkmask<T, N>) * P);
} };
template<typename T, int S, T P> struct s_bcd<T,S,S,P> { static inline T f(T x) { return x; } };
template<typename T> inline T from_bcd(T x) { return s_bcd<uint_for_t<T>, 4, BIT_COUNT<T>, 10>::f(as_unsigned(x)); }

//implemented in defs.cpp
uint64			to_bcd(uint64 x, int n);
inline uint64	to_bcd(uint64 x)		{ int i = highest_set_index(x); return to_bcd(x << (63 - i), i + 1); }

uint64			dpd_to_bcd(uint64 x);
uint64			bcd_to_dpd(uint64 x);

inline uint32	bin_to_dpd(uint32 x)	{ return (uint32)bcd_to_dpd(to_bcd(x)); }
inline uint32	dpd_to_bin(uint32 x)	{ return (uint32)from_bcd(dpd_to_bcd(x)); }
uint64			bin_to_dpd(uint64 x);
uint64			dpd_to_bin(uint64 x);
uint128			bin_to_dpd(uint128 x);
uint128			dpd_to_bin(uint128 x);

template<typename T> inline T bcd_add(T a, T b) {
	a += ~T(0) / 0xf0 * 6;	// add 6 to each nibble (except top)
	T c = a ^ b;			// all binary carry bits
	a += b;
	return a - ((~c ^ a) & (~T(0) / 15)) / 16 * 6;	// remove 6 from non-propagating nibbles
}
template<typename T> inline T bcd_sub(T a, T b) {
	return bcd_add(a, ~T(0) / 0xf0 * 9 - b + 1);	// 9s complement + 1
}
template<typename T> inline T bcd_neg(T a) {
	T	c = ~a ^ -a ^ 1;							// all binary carry bits
	return -a - (~c & (~T(0) / 15)) / 16 * 6;		// remove 6 from non-propagating nbbles
}

//-----------------------------------------------------------------------------
// Gray code (single bit change on increment)
//-----------------------------------------------------------------------------

template<typename T> inline T to_gray(T x)		{ return x ^ (x >> 1); }
template<typename T, int N>	struct s_gray		{ static inline T f(T x) { return s_gray<T, N / 2>(x ^ (x >> N)); } };
template<typename T>		struct s_gray<T,1>	{ static inline T f(T x) { return x ^ (x >> 1); } };
template<typename T> inline T from_gray(T x)	{ return s_gray<uint_for_t<T>, sizeof(T) * 4>::f(as_unsigned(x)); }

//-----------------------------------------------------------------------------
//	corput sequences (digit flipping)
//-----------------------------------------------------------------------------

inline uint32 reverse_digits(uint32 base, uint32 x) {
	uint32	y = 0;
	while (x) {
		y = y * base + x % base;
		x /= base;
	}
	return y;
}

inline float corput(uint32 base, uint32 x) {
	uint32	y = 0, t = 1;
	while (x) {
		y = y * base + x % base;
		t = t * base;
		x /= base;
	}
	return y / float(t);
}

template<int B> inline float corput(uint32 x) {
	uint32	y = 0, t = 1;
	while (x) {
		y = y * B + x % B;
		t = t * B;
		x /= B;
	}
	return y / float(t);
}

template<> inline float corput<2>(uint32 x)		{ return (reverse_bits(x) >> 1) / float(0x80000000u);}
template<> inline float corput<4>(uint32 x)		{ return (reverse_chunks<1>(x) >> 1) / float(0x80000000u);}
template<> inline float corput<16>(uint32 x)	{ return (reverse_chunks<2>(x) >> 1) / float(0x80000000u);}

//-----------------------------------------------------------------------------
//	masked numbers
//-----------------------------------------------------------------------------

template<typename T, T M> struct fixed_masked {
	T		i;

	fixed_masked()		: i(0)	{}
	fixed_masked(T t)	: i(spread_bits<T, M>(t))	{}

	void			_set(T t)		{ i = masked_write(i, t, M); }
	T				get() const		{ return collect_bits<T, M>(i); }

	fixed_masked&	operator+=(fixed_masked &b)	{ _set((i | ~M) + (b.i & M)); return *this; }
	fixed_masked&	operator-=(fixed_masked &b)	{ _set((i &  M) - (b.i & M)); return *this; }

	fixed_masked&	operator++()	{ _set((i | ~M) + 1); return *this; }
	fixed_masked&	operator--()	{ _set((i &  M) - 1); return *this; }
	fixed_masked	operator++(int)	{ fixed_masked r(*this); operator++(); return r; }
	fixed_masked	operator--(int)	{ fixed_masked r(*this); operator--(); return r; }

	constexpr bool operator==(const fixed_masked &b) { return (i & M) == (b.i & M); }
	constexpr bool operator!=(const fixed_masked &b) { return (i & M) != (b.i & M); }
	constexpr bool operator< (const fixed_masked &b) { return (i & M) <  (b.i & M); }
	constexpr bool operator<=(const fixed_masked &b) { return (i & M) <= (b.i & M); }
	constexpr bool operator> (const fixed_masked &b) { return (i & M) >  (b.i & M); }
	constexpr bool operator>=(const fixed_masked &b) { return (i & M) >= (b.i & M); }

	//friend T		get(const fixed_masked &a)	{ return a; }
};

template<typename T> struct masked_number {
	const T	mask;
	T		i;
	
	masked_number(T mask, T i) : mask(mask), i(i)	{}

	void			_set(T t)		{ i = masked_write(i, t, mask); }
	void			set(T t)		{ _set(spread_bits(t, mask)); }
	T				get() const		{ return collect_bits(i, mask); }

	masked_number&	operator+=(masked_number &b)	{ _set((i | ~mask) + (b.i & mask)); return *this; }
	masked_number&	operator-=(masked_number &b)	{ _set((i &  mask) - (b.i & mask)); return *this; }

	masked_number&	operator++()	{ _set((i | ~mask) + 1); return *this; }
	masked_number&	operator--()	{ _set((i &  mask) - 1); return *this; }
	masked_number	operator++(int)	{ auto t(*this); operator++(); return t; }
	masked_number	operator--(int)	{ auto t(*this); operator--(); return t; }

	//friend T		get(const masked_number &a)	{ return a; }
};

} // namespace iso
#endif
