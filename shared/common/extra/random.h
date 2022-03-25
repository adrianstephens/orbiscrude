#ifndef RANDOM_H
#define RANDOM_H

#include "base/defs.h"
#include "base/maths.h"
#include "base/functions.h"
#include "base/bits.h"
#include "base/interval.h"

namespace iso {

//-----------------------------------------------------------------------------
//	random interface
//-----------------------------------------------------------------------------

template<class A, int N, int M = (N > A::BITS ? 1 : N == A::BITS ? 0 : -1)> struct get_bits_s {
	typedef	uint_bits_t<N>	I;
	static I	f(A *a)		{ return a->next() | (I(get_bits_s<A, N - A::BITS>::f(a)) << A::BITS); }
};
template<class A, int N> struct get_bits_s<A, N, 0> {
	typedef	uint_bits_t<A::BITS>	I;
	static I	f(A *a)		{ return I(a->next()); }
};
template<class A, int N> struct get_bits_s<A, N, -1> {
	typedef	uint_bits_t<N>	I;
	static I	f(A *a)		{ return I(a->next()) & bits<I>(N); }
};

template<typename T, typename=void> struct random_get_s;
template<typename T, typename=void> struct random_to_s;

template<typename T> struct random_get_s<T, enable_if_t<is_int<T>>> {
	template<class A> static T	f(A *a)	{ return force_cast<T>(get_bits_s<A, BIT_COUNT<T>>::f(a)); }
};
template<typename T> struct random_get_s<T, enable_if_t<is_float<T>>> {
	typedef float_components<T>	F;
	template<class A> static T	f(A *a)	{ return F(get_bits_s<A, F::M>::f(a), F::E_OFF, 0).f() - one; }
};

template<typename T> struct random_to_s<T, enable_if_t<is_int<T>>> {
	template<typename A> static T	f(A *a, T x)	{ return T(fullmul(x, a->next()) >> A::BITS); }
};
template<typename T> struct random_to_s<T, enable_if_t<is_float<T>>> {
	template<typename A> static T	f(A *a, T x)	{ return T(x * random_get_s<T>::f(a)); }
};

#ifdef VECTOR_H
template<class A, int N, int M = pow2_ceil<N>, bool = (M > A::BITS)> struct get_bits2_s {
	static void	f(A *a, void *p)	{
		get_bits2_s<A, M / 2, M / 2>::f(a, p);
		get_bits2_s<A, N - M / 2, M / 2>::f(a, (uint8*)p + M / 16);
	}
};
template<class A, int N, int M> struct get_bits2_s<A, N, M, false> {
	static void	f(A *a, void *p)	{ *(uint_bits_t<N>*)p = a->next() ; }
};

template<class A, int M> struct get_bits2_s<A, 0, M, false> {
	static void	f(A *a, void *p)	{}
};

template<typename T> struct random_to_s<T, enable_if_t<is_vec<T>>> {
	template<class A, int...I> static T f(A *a, T x, meta::value_list<int, I...>) { return T{a->to(x[I])...}; }
	template<typename A> static T	f(A *a, T x)	{ return f(a, x, meta::make_value_sequence<num_elements_v<T>>()); }
};
template<typename T> struct random_get_s<T, enable_if_t<is_vec<T> && is_int<element_type<T>>>> {
	template<typename A> static T	f(A *a)	{
		T	t;
		get_bits2_s<A, BIT_COUNT<element_type<T>> * num_elements_v<T>>::f(a, &t);
		return t;
	}
};

template<typename T> struct random_get_s<T, enable_if_t<is_vec<T> && is_float<element_type<T>>>> {
	template<typename A> static T	f(A *a)	{
		typedef element_type<T>	E;
		typedef sint_for_t<E>	I;
		typedef float_components<E>	F;
		auto	i = a->template get<vec<I, num_elements_v<T>>>();
		return as<E>((i & bits<I>(F::M)) | (F::E_OFF << F::M)) - one;
	}
};
#endif

template<class A> class random_mixin {
	A*					a()								{ return static_cast<A*>(this); }
public:
	getter<random_mixin>	get()						{ return *this; }
	template<typename T> T	get()						{ return random_get_s<T>::f(a()); }
//	template<typename T>	operator T()				{ return random_get_s<T>();	}
	template<typename T, typename U = typename T_enable_if<is_num<T>, T>::type> operator T() { return get<T>();	}
	template<class T> T		to(T x)						{ return random_to_s<T>::f(a(), x); }
	template<class T> T		from(T x0, T x1)			{ return x0 + to(x1 - x0); }
	template<class T> T		from(const interval<T> &i)	{ return from(i.a, i.b); }
	template<class T> typename container_traits<T>::reference		fromc(T &t)			{ return t[to(num_elements(t))]; }
	template<class T> typename container_traits<const T>::reference	fromc(const T &t)	{ return t[to(num_elements(t))]; }
	template<class T> T		operator()(T x)				{ return to(x); }
	template<class T> T&	set(T &t)					{ return t = get<T>(); }
	template<class T> void	fill(T &t)					{ for (auto &i : t) set(i); }
	template<class T> void	fill(const T &t)			{ for (auto &i : t) set(i); }
};

template<class A> struct rng : A, random_mixin<rng<A> > {
	rng() : A(random_seed()) {}
	template<typename T> rng(const T &t) : A(t) {}
};

struct vrng : random_mixin<vrng> {
	static const int BITS = 32;
	callback<uint32()>	cb;
	int		next()	{ return cb(); }
//	template<class A> vrng(A &a) { cb.bind((uint32 (A::*)())&A::template get<uint32>, &a); }
	template<class A> vrng(A &a) : cb(make_callback(&a, &A::template get<uint32>))	{}
//	template<class A> vrng(A &a) { cb.bind<A, (uint32(A::*)())&A::template get<uint32>>(&a); }
};

//-----------------------------------------------------------------------------
//	random generators
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	Linear congruential generator
//-----------------------------------------------------------------------------

template<typename T, T M, T A, T C, int B, int B0 = 0> class LCG {
	typedef	uint_t<(B + 7) / 8>	I;
	T		x;
public:
	static const int	BITS = B;
	I		next() {
		x = mod_check(A * x + C, CINT(M));
		return I(x >> B0) & bits<I>(B);
	}
	LCG(T x) : x(x)	{}
};


/*
														m(mod)			a(mult)						c(add)						output
Numerical Recipes										2^32			1664525						1013904223
Borland													2^32			22695477					1							bits 30..16 in rand(), 30..0 in lrand()
glib													2^31 - 1		1103515245					12345						bits 30..0
ANSI C													2^31			1103515245					12345						bits 30..16
Delphi, Virtual Pascal									2^32			134775813					1							bits 63..32 of (seed * L)
Turbo Pascal											2^32			134775813	(0x8088405)		1
Microsoft Visual/Quick C/C++							2^32			214013		(0x343FD)		2531011		(0x269EC3)		bits 30..16
Microsoft Visual Basic (6 and earlier)					2^24			1140671485	(0x43FD43FD)	12820163	(0xC39EC3)
RtlUniform from Native API								2^31 - 1		2147483629	(0x7FFFFFED)	2147483587	(0x7FFFFFC3)
Apple CarbonLib, C++11's minstd_rand0					2^31 - 1		16807						0							see MINSTD
C++11's minstd_rand										2^31 - 1		48271						0							see MINSTD
MMIX by Donald Knuth									2^64			6364136223846793005			1442695040888963407
Newlib, Musl											2^64			6364136223846793005			1							bits 63...32
VMS's MTH$RANDOM, old versions of glibc					2^32			69069		(0x10DCD)		1
Java's java.util.Random, POSIX rand48, glibc rand48		2^48			25214903917 (0x5DEECE66D)	11							bits 47...16
random0 (lowest bit oscillates at each step)			134456(2^3x7^5)	8121						28411						Xn/134456
POSIX rand48, glibc rand48								2^48			25214903917 (0x5DEECE66D)	11							bits 47...15
POSIX rand48, glibc rand48								2^48			25214903917 (0x5DEECE66D)	11							bits 47...0
cc65													2^23			65793		(0x10101)		4282663		(0x415927)		bits 22...8
cc65													2^32			16843009	(0x1010101)		826366247	(0x31415927)	bits 31...16
RANDU 													2^31			65539						0
*/

typedef LCG<uint64,0,1664525,1013904223,32,0> simple_random;

//-----------------------------------------------------------------------------
//	Xorshift
//-----------------------------------------------------------------------------

template<int N> class xorshift;

template<> class xorshift<32> {
	uint32 state;
public:
	static const int	BITS = 32;
	uint32	curr() {
		return state;
	}
	uint32	next() {
		uint32	x	= state;
		x	^= x << 13;
		x	^= x >> 17;
		x	^= x << 5;
		return state = x;
	}
	xorshift(int64 x) : state((uint32)x) {}
};

template<> class xorshift<64> {
	uint64	state;
public:
	static const int	BITS = 64;
	uint64	curr() {
		return state;
	}
	uint64	next() {
		uint64 x = state;
		x ^= x >> 12;
		x ^= x << 25;
		x ^= x >> 27;
		return state = x;
	}
	xorshift(int64 x) : state(x) {}
};

template<> class xorshift<128> {
	uint64	state[2];
public:
	static const int	BITS = 64;
	uint64	curr() {
		return state[1];
	}
	uint64	next() {
		uint64 x = state[0];
		uint64 y = state[1];
		state[0] = y;
		x		^= x << 23;
		return state[1] = x ^ y ^ (x >> 17) ^ (y >> 26);
	}
	xorshift(int64 x) {
		state[0] = state[1] = x;
	}
};

template<> class xorshift<1024> {
	uint64	state[16];
	int		p;
public:
	static const int	BITS = 64;
	uint64 curr() {
		return state[p];
	}
	uint64 next() {
		uint64	s0	= state[p];
		uint64	s1	= state[p = (p + 1) & 15];
		s1			^= s1 << 31;
		return state[p]	= s1 ^ s0 ^ (s1 >> 11) ^ (s0 >> 30);
	}
	xorshift(int64 x) {
		for (int i = 0; i < 16; i++)
			state[i] = x;
	}
};

template<int N> class xorshiftplus : public xorshift<N> {
public:
	uint_t<(xorshift<N>::BITS + 7) / 8> next() {
		return xorshift<N>::curr() + xorshift<N>::next();
	}
	xorshiftplus(int64 x) : xorshift<N>(x) {}
};
template<int N, uint_t<(xorshift<N>::BITS + 7) / 8> M> class xorshiftstar : public xorshift<N> {
public:
	uint_t<(xorshift<N>::BITS + 7) / 8> next() {
		return xorshift<N>::next() * M;
	}
	xorshiftstar(int64 x) : xorshift<N>(x) {}
};
typedef xorshiftstar<64, 0x2545F4914F6CDD1DLL>		xorshift64star;
typedef xorshiftstar<1024, 1181783497276652981LL>	xorshift1024star;

//-----------------------------------------------------------------------------
//	mersenne twister
//-----------------------------------------------------------------------------

template<typename T, unsigned N, unsigned M, unsigned R, T A, unsigned S1, T M1, unsigned S2, T M2, unsigned S3, T M3, unsigned L, T F> struct mersenne_twisterT {
	typedef		T		element;
	static const int	BITS	= sizeof(T) * 8;
	static const T		MASK	= (T(1) << R) - 1;

	T		x[N];
	int		index;

	mersenne_twisterT()		{}
	mersenne_twisterT(T s)	{ seed(s); }

	void	seed(T s) {
		index	= N;
		T	a	= x[0] = s;
		for (unsigned i = 1; i < N; i++)
			x[i] = a = F * (a ^ (a >> 30)) + i;
	}

	static T twist(T u, T v) {
		return (((u & ~MASK) | (v & MASK)) >> 1) ^ (v & 1 ? A : 0);
	}
#if 0
	void twist() {
		for (unsigned i = 0; i < N; i++)
			x[i] = x[(i + M) % N] ^ twist(x[i], x[(i + 1) % N]);
	}
#else
	void twist() {
		for (unsigned i = 0; i < N - M; i++)
			x[i] = x[i + M] ^ twist(x[i], x[i + 1]);

		for (unsigned i = N - M; i < N - 1; i++)
			x[i] = x[i + (M - N)] ^ twist(x[i], x[i + 1]);

		x[N - 1] = x[M - 1] ^ twist(x[N - 1], x[0]);
	}
#endif
	T	next() {
		if (index >= N) {
			twist();
			index = 0;
		}
		T y = x[index++];
		y ^= (y >> S1) & M1;
		y ^= (y << S2) & M2;
		y ^= (y << S3) & M3;
		return y ^ (y >> L);
	}
};

struct mersenne_twister32_19937 : mersenne_twisterT<uint32,
	624, 397, 31,
	0x9908B0DF,
	11, 0xFFFFFFFF,
	7,	0x9D2C5680,
	15, 0xEFC60000,
	18,
	1812433253
> {
	mersenne_twister32_19937(uint32 s) { seed(s); }
};

struct mersenne_twister64_19937 : mersenne_twisterT<uint64,
	312, 156, 31,
	0xB5026F5AA96619E9ull,
	29, 0x5555555555555555ull,
	17, 0x71D67FFFEDA60000ull,
	37, 0xFFF7EEE000000000ull,
	43,
	6364136223846793005ull
> {
	mersenne_twister64_19937(uint64 s) { seed(s); }
};

// tiny_mersenne_twister

//r = 31
//a1 = 2406486510, a2 = 4235788063, a3 = 932445695
//s0 = 1, s1 = 10
//f	= 1812433253

template<typename T, int R, uint32 A1, uint32 A2, T A3, int S0, int S1, T F> struct tiny_mersenne_twister {
	typedef	T			element;
	static const int	BITS	= sizeof(T) * 8;
	static const T		MASK	= (T(1) << R) - 1;
	uint32	x[4];

	tiny_mersenne_twister()		{}
	tiny_mersenne_twister(T s)	{ seed(s); }

	void seed(T s) {
		x[0] = s;
		x[1] = A1;
		x[2] = A2;
		x[3] = A3;
		for (int i = 1; i < 8; i++)
			x[i & 3] ^= i + F * (x[(i - 1) & 3] ^ (x[(i - 1) & 3] >> 30));
	}

	void next_state() {
		T	y = x[3];
		T	z = (x[0] & MASK) ^ x[1] ^ x[2];
		z ^= (z << S0);
		y ^= (y >> S0) ^ z;
		x[0] = x[1];
		x[1] = x[2] ^ (-(y & 1) & A1);
		x[2] = (z ^ (y << S1)) ^ (-(y & 1) & A2);
		x[3] = y;
	}

	T	next() {
		T	t0 = x[3];
	#if defined(LINEARITY_CHECK)
		T	t1 = x[0] ^ (x[2] >> 8);
	#else
		T	t1 = x[0] + (x[2] >> 8);
	#endif
		return t0 ^ t1 ^ (-(t1 & 1) & A3);
	}
};

//mat1 = 2406486510, mat2 = 4235788063, tmat = 932445695
//r = 63
//s0 = 12, s1 = 11
//f = 6364136223846793005

template<int R, uint32 A1, uint32 A2, uint64 A3, int S0, int S1, uint64 F> struct tiny_mersenne_twister<uint64, R, A1, A2, A3, S0, S1, F> {
	typedef uint64		element;
	static const int	BITS	= 64;
	static const uint64	MASK	= (uint64(1) << R) - 1;
	uint64 x[2];

	tiny_mersenne_twister()			{}
	tiny_mersenne_twister(uint64 s)	{ seed(s); }

	void seed(uint64 s) {
		x[0] = s ^ (uint64(A1) << 32);
		x[1] = A2 ^ A3;
		for (int i = 1; i < 8; i++)
			x[i & 1] ^= i + F * (x[(i - 1) & 1] ^ (x[(i - 1) & 1] >> 62));
	}

	void next_state() {
		x[0] &= MASK;
		uint64 y = x[0] ^ x[1];
		y ^= y << S0;
		y ^= y >> 32;
		y ^= y << 32;
		y ^= y << S1;
		x[0] = x[1]	^ (-(y & 1) & A1);
		x[1] = y	^ (-(y & 1) & (uint64(A2) << 32));
	}

	uint64 next() {
	#if defined(LINEARITY_CHECK)
		uint64 v = x[0] ^ x[1];
	#else
		uint64 v = x[0] + x[1];
	#endif
		v ^= x[0] >> 8;
		return v ^ -(v & 1) & A3;
	}
};

//-----------------------------------------------------------------------------
//	modifiers
//-----------------------------------------------------------------------------

// Re-seed from the platform RNG after generating RESEED bytes (and then discard DISCARD bytes)
// (requires add_entropy)
template<typename R, int RESEED = 1600000, int DISCARD = 12 * 256> struct random_stir : R {
	int	count;

	uint8 next() {
		if (!--count) {
			auto	t = time::now();
			R::add_entropy(const_memory_block(&t));

			// Discard early keystream
			for (int i = DISCARD; i--;)
				(void)R::next();

			count = RESEED;
		}
		return R::next();
	}

	random_stir(uint64 seed = time::now()) : count(RESEED) {
		R::add_entropy(const_memory_block(&seed));
	}
};

// turn RNG into encryptor
template<typename R> struct xor_crypt_mixin {
	void process(const uint8 *input, uint8 *output, size_t length) {
		typedef uint_bits_t<R::BITS>	T;

		auto	r	= static_cast<R*>(this);
		auto	o	= (T*)output;
		for (auto i = (const T*)input, e = (const T*)(input + length); i < e; ++i)
			*o++ = *i++ ^ r->next();
	}
	void encrypt(uint8 *data, size_t length) { process(data, data, length); }
	void decrypt(uint8 *data, size_t length) { process(data, data, length); }
};

//-----------------------------------------------------------------------------
//	distributions
//-----------------------------------------------------------------------------

template<class A> struct normal_distribution {
	A		*a;
	float	mean, sigma;
	float	x2;

	float	next()		{ uint32 i = (a->next() & 0x7fffff) | 0x40000000; return (float&)i - 3; }

	operator float() {	// Knuth, vol. 2, p. 122, alg. P
		float	r = x2;
		if (r) {
			x2 = 0;
		} else {
			float	v1, v2, sx;
			do {
				v1 = next();
				v2 = next();
				sx = v1 * v1 + v2 * v2;
			} while (sx >= 1);

			float	fx = sqrt(-2 * log(sx) / sx);
			x2	= fx * v2;
			r	= fx * v1;
		}
		return r * sigma + mean;
	}
	normal_distribution(A *a, float mean, float sigma) : a(a), mean(mean), sigma(sigma), x2(0) {}
};

template<class A> struct poisson_distribution {
	A		*a;
	float	mean, sigma;
	float	logm, g0, g1;

	float	next()		{ uint32 i = (a->next() & 0x7fffff) | 0x3f800000; return (float&)i - 1; }

	operator float() {
		float	r, v;
		if (mean < 12) {
			for (r = 0, v = 1; ; ++r) {
				v *= next();
				if (v <= g0)
					break;
			}
		} else {
			for (;;) {
				do {
					v	= tan(pi * next());
					r	= sigma * v + mean;
				} while (r <= 0);
				if (next() <= 0.9f * (1 + square(v)) * exp(r * logm) - lgamma(r + 1) - g1)
					break;
			}
		}
		return r;
	}

	poisson_distribution(A *a, float mean) : a(a), mean(mean), sigma(sqrt(mean * 2)), logm(log(mean)), g0(exp(-mean)), g1(mean * logm - lgamma(mean + 1)) {}
};

template<class A> struct ziggurat_normal {
	A		*a;
	uint32	k[128];
	float	w[128], f[128];

	float	next()		{ uint32 i = (a->next() & 0x7fffff) | 0x3f800000; return (float&)i - 1; }

	operator float() {
		static const float r = 3.442620f;	// The start of the right tail

		int		h = a->next();
		int		i = h & 127;
		float	x = h * w[i];

		while (abs(h) >= k[i]) {
			if (i == 0) {
				float	y;
				do {
					x = -log(next()) / r;
					y = -log(next());
				} while (y + y < x * x);
				return h > 0 ? r + x : -r - x;
			}
			if (f[i] + next() * (f[i - 1] - f[i]) < exp(-.5 * x * x))
				return x;

			h = a->next();
			i = h & 127;
			x = h * w[i];
		}
		return x;
	}

	ziggurat_normal(A *a) : a(a) {
		static const double m = uint64(1) << 31;
		static const double v = 9.91256303526217e-3;

		double	d	= 3.442619855899;
		double	q	= v / exp(-.5 * d * d);

		k[0]	= d / q * m;
		k[1]	= 0;

		w[0]	= q / m;
		w[127]	= d / m;

		f[0]	= 1;
		f[127]	= exp(-.5 * d * d);

		for (int i = 126; i >= 1; i--) {
			double	t	= d;
			d			= sqrt(-2 * log(v / d + exp(-.5 * d * d)));
			k[i + 1]	= d / t * m;
			f[i]		= exp(-.5 * d * d);
			w[i]		= d / m;
		}
	}
};

template<class A> struct ziggurat_exp {
	A		*a;
	uint32	k[256];
	float	w[256], f[256];

	float	next()		{ uint32 i = (a->next() & 0x7fffff) | 0x3f800000; return (float&)i - 1; }

	operator float() {
		uint32	h = a->next();
		uint32	i = h & 255;
		float	x = h * w[i];

		while (h >= k[i]) {
			if (i == 0)
				return 7.69711f - log(next());

			if (f[i] + next() * (f[i - 1] - f[i]) < exp(-x))
				return x;

			h = a->next();
			i = h & 255;
			x = h * w[i];
		}
		return x;
	}

	ziggurat_exp(A *a) : a(a) {
		static const double m = uint64(1) << 32;
		static const double v = 3.949659822581572e-3;

		double	d = 7.697117470131487;
		double	q = v / exp(-d);

		k[0]	= d / q * m;
		k[1]	= 0;

		w[0]	= q / m;
		w[255]	= d / m;

		f[0]	= 1;
		f[255]	= exp(-d);

		for (int i = 254; i >= 1; i--) {
			double	t	= d;
			d			= -log(v / d + exp(-d));
			k[i + 1]	= d / t * m;
			f[i]		= exp(-d);
			w[i]		= d / m;
		}
	}
};

//-----------------------------------------------------------------------------
//	functions
//-----------------------------------------------------------------------------

// fisher-yates
template<typename C, typename R> void shuffle(C &c, R &r) {
	for (auto i = begin(c), e = end(c); i != e; ++i) {
		auto j = r.from(i, e);
		if (j != i)
			swap(*i, *j);
	}
}
// from a source
template<typename C, typename I, typename R> void shuffle(C &c, I s, R &r) {
	for (auto b = begin(c), i = begin(c), e = end(c); i != e; ++i, ++s) {
		auto j = r.from(b, i + 1);
		if (j != i)
			*i = *j;
		*j = *s;
	}
}

// as above, but always creates a single cycle (sattolo's algorithm)
template<typename C, typename R> void shuffle_cycle(C &c, R &r) {
	for (auto i = begin(c), e = end(c); i != e; ++i) {
		auto j = r.from(i + 1, e);
		swap(*i, *j);
	}
}
template<typename C, typename I, typename R> void shuffle_cycle(C &c, I s, R &r) {
	for (auto b = begin(c), i = begin(c), e = end(c); i != e; ++i, ++s) {
		auto j = r.from(b, i);
		*i = *j;
		*j = *s;
	}
}

} // namespace iso

#endif // RANDOM_H
