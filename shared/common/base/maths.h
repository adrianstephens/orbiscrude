#ifndef MATHS_H
#define MATHS_H

#include "base/defs.h"
#include "base/deferred.h"
#include "base/constants.h"
#include "_maths.h"

namespace iso {

static const float ISO_TOLERANCE = 1e-5f;

inline float	sqrt(int i)				{ return sqrt(float(i)); }
inline float	sqrt(uint32 i)			{ return sqrt(float(i)); }
inline float	reciprocal(int i)		{ return reciprocal(float(i)); }
inline float	reciprocal(uint32 i)	{ return reciprocal(float(i)); }

#if USE_LONG
inline float	sqrt(long i)			{ return sqrt(float(i)); }
inline float	reciprocal(long i)		{ return reciprocal(float(i)); }
inline float	sqrt(ulong i)			{ return sqrt(float(i)); }
inline float	reciprocal(ulong i)		{ return reciprocal(float(i)); }
#endif
inline float	sqrt(int64 i)			{ return sqrt(float(i)); }
inline float	reciprocal(int64 i)		{ return reciprocal(float(i)); }
inline float	sqrt(uint64 i)			{ return sqrt(float(i)); }
inline float	reciprocal(uint64 i)	{ return reciprocal(float(i)); }
#ifdef __clang__
inline float	sqrt(int128 i)			{ return sqrt(float(i)); }
inline float	sqrt(uint128 i)			{ return sqrt(float(i)); }
#endif

template<typename T> constexpr T sqrtNR(T x, T curr, T prev) {
	return curr == prev ? curr : sqrtNR(x, (curr + x / curr) * half, curr);
}
template<typename T> constexpr T constexpr_sqrt(T x) {
	return sqrtNR<T>(x, x, zero);
}

//fused a*b + c*d
template<typename T> inline T fmma(T a, T b, T c, T d) {
	T	w = c * d;
	return fma(a, b, w) + fma(c, d, -w);
}

//-----------------------------------------------------------------------------
//	constants
//-----------------------------------------------------------------------------

template<typename N>				struct __sqrt	{ template<typename R> static constexpr R as() { return constexpr_sqrt(N::template as<R>()); } };
template<typename N, typename B>	struct __log	{ template<typename R> static inline 	R as() { static R r = log(constant<N>()) / log(constant<B>()); return r; } };

//template<>	struct __sqrt<_maximum>	{ template<typename R> static constexpr R as() { return float_components<R>(0, 3 << (float_components<R>::E - 2), 0).f(); } };
//template<>	struct __sqrt<_minimum>	{ template<typename R> static constexpr R as() { return float_components<R>(0, 1 << (float_components<R>::E - 2), 0).f(); } };

#define CINT(X)	constant<__int<X> >()

template<> template<typename R> constexpr R	__sqrt<__int<2> >::as()		{ return R(1.4142135623730950488); }
template<> template<typename R> constexpr R	__sqrt<__int<3> >::as()		{ return R(1.7320508075688772935); }
template<> template<typename R> R			__log<__int<2>, __e>::as()	{ return R(0.6931471805599453094); }
template<> template<typename F> constexpr F	__neg<_infinity>::as()		{ return force_cast<F>(float_components<F>::neg_inf()); }
template<typename N> struct __log<N, N>		: _one {};

//-----------------------------------------------------------------------------
//	constant functions
//-----------------------------------------------------------------------------

template<typename N> inline N div_check(const N &n, const _zero&) { return 0; }
template<typename N> inline N mod_check(const N &n, const _zero&) { return n; }

template<typename A> force_inline _one		pow(const A &a, const _zero&)			{ return one; }
template<typename A> force_inline const A	pow(const A &a, decltype(half))			{ return sqrt(a); }
template<typename A> force_inline const A	pow(const A &a, decltype(-half))		{ return rsqrt(a); }
template<typename A> force_inline const A&	pow(const A &a, const _one&)			{ return a; }
template<typename A> force_inline const A	pow(const A &a, decltype(-one))			{ return reciprocal(a); }
template<typename A> force_inline const A	pow(const A &a, decltype( one / three))	{ return copysign(pow(abs(a), 1/3.f), a); }
template<typename A> force_inline const A	pow(const A &a, decltype(-one / three))	{ return copysign(pow(abs(a), -1/3.f), a); }
template<typename A> force_inline const A	pow(const _one&, const A&)				{ return A(one); }
template<typename A> force_inline const A	pow(decltype(two), const A &a)			{ return exp2(a); }
template<typename A> force_inline const A	pow(const _e&, const A &a)				{ return exp(a); }
template<typename A> force_inline const A	pow(decltype(half), const A &a)			{ return reciprocal(exp2(a)); }

template<uint32 N, bool odd = N & 1> struct pow_s;
template<>			struct pow_s<0, false>		{ template<typename T> constexpr auto static f(T v) { return one; } };
template<>			struct pow_s<1, true>		{ template<typename T> constexpr auto static f(T v) { return v; } };
template<>			struct pow_s<2, false>		{ template<typename T> constexpr auto static f(T v) { return square(v); } };
template<uint32 N>	struct pow_s<N, true>		{ template<typename T> constexpr auto static f(T v) { return v * pow_s<N / 2>::f(square(v)); } };
template<uint32 N>	struct pow_s<N, false>		{ template<typename T> constexpr auto static f(T v) { return pow_s<N / 2>::f(square(v)); } };
template<uint32 N, typename T> constexpr auto pow(T v)	{ return pow_s<N>::f(v); }

//-----------------------------------------------------------------------------
//	deferred derivatives
//-----------------------------------------------------------------------------

template<int I, typename D> struct op_param_d : deferred_mixin<op_param_d<I, D>> {
	D	d;
	op_param_d(const D &d) : d(d) {}
};

template<int I, typename D> auto	deriv(const op_param<I> &f, const D &d)				{ return op_param_d<I, D>(d); }
template<int I> auto				deriv(const op_param<I> &f, const op_param<I> &d)	{ return one; }
template<int I1, int I2> auto		deriv(const op_param<I1> &f, const op_param<I2> &d)	{ return zero; }

template<typename A, typename D> auto deriv(const deferred<op_neg, A> &f, const D &d) {
	return -deriv(f.a, d);
}
template<typename A, typename B, typename D> auto deriv(const deferred<op_add, A, B> &f, const D &d) {
	return deriv(f.a, d) + deriv(f.b, d);
}
template<typename A, typename B, typename D> auto deriv(const deferred<op_sub, A, B> &f, const D &d) {
	return deriv(f.a, d) - deriv(f.b, d);
}
template<typename A, typename B, typename D> auto deriv(const deferred<op_mul, A, B> &f, const D &d) {
	return deriv(f.a, d) * f.b + deriv(f.b, d) * f.a;
}
template<typename A, typename B, typename D> auto deriv(const deferred<op_div, A, B> &f, const D &d) {
	return (deriv(f.b, d) * f.a - deriv(f.a, d) * f.b) / square(f.b);
}

//chain rule
//template<typename F, typename A, typename B> auto deriv(const deferred<F, A, B> &f) {
//	return make_deferred(deriv((const F&)f), f.a, f.b) * deriv(f.a) * deriv(f.b);
//}
//template<typename F, typename A, typename D> auto deriv(const deferred<F, A> &f, const D &d) {
//	return make_deferred(deriv((const F&)f), f.a) * deriv(f.a);
//}

//-----------------------------------------------------------------------------
//	is_square
//-----------------------------------------------------------------------------

extern bool check_square_mod128(uint32 a);
extern bool check_square_mod3_5_7(uint32 a);

inline bool check_square_mod11_13_17_19_23_29_31(uint32 c) {
	return !(
		(bit(c % 11) & 0x5C4)
	||  (bit(c % 13) & 0x9E4)
	||  (bit(c % 17) & 0x5CE8)
	||  (bit(c % 19) & 0x4F50C)
	||  (bit(c % 23) & 0x7ACCA0)
	||  (bit(c % 29) & 0xC2EDD0C)
	||  (bit(c % 31) & 0x6DE2B848)
	);
}

template<typename T> bool check_square_mod11_13_17_19_23_29_31(T a) {
	return check_square_mod11_13_17_19_23_29_31(uint32(a %  uint32(11 * 13 * 17 * 19 * 23 * 29 * 31)));
}

template<typename T> bool is_square(T a) {
	if (!a)
		return true;

	return !get_sign(a)
		&& check_square_mod128(a & 127)
		&& check_square_mod3_5_7(a % uint32(3 * 5 * 7))
		&& check_square_mod11_13_17_19_23_29_31(a)
		&& a == square(sqrt(a));
}


//-----------------------------------------------------------------------------
//	rational
//-----------------------------------------------------------------------------

template<typename T> T gcd(T a, T b) {
	if (a > b)
		swap(a, b);
	if (!a)
		return b;
	while (T r = b % a) {
		b = a;
		a = r;
	}
	return a;
}

//	returns gcd(a, b)
//	s, t are bezout coefficients: a.s + b.t = gcd(a,b)
template<typename T> T extended_euclid(T a, T b, T &s, T &t) {
	T	r0	= a,		r1 = b;
	T	s0	= one,		s1 = zero;
	T	t0	= zero,		t1 = one;
	T	q;

	for (;;) {
		q	= divmod(r0, r1);
		if (!r0) {
			s = s1;
			t = t1;
			return r1;
		}
		s0	-= q * s1;
		t0	-= q * t1;

		q	= divmod(r1, r0);
		if (!r1) {
			s = s0;
			t = t0;
			return r0;//break;	//gcd is r0, inv_mod(a, n) is s0
		}
		s1	-= q * s0;
		t1	-= q * t0;
	}
}

// returns modular inverse a^-1 mod n
template<typename T> T inv_mod(T a, T n) {
	T	r0	= a,		r1 = n;
	T	s0	= one,		s1 = zero;
	T	q;

	for (;;) {
		q	= divmod(r0, r1);
		if (!r0)
			return s1 < zero ? s1 + n : s1;
		s0	-= q * s1;

		q	= divmod(r1, r0);
		if (!r1)
			return s0 < zero ? s0 + n : s0;
		s1	-= q * s0;
	}
}


template<typename T> struct rational {
	T	n, d;

	static rational normalised(T n, T d) {
		T	g = gcd(n, d);
		if (g > 1) {
			n	/= g;
			d	/= g;
		}
		return rational(n, d);
	}
	template<typename F> static rational from_float(F f, F epsilon, int max_iter) {
		F		f0	= frac(abs(f)), z = f0;
		T		d0	= 0, d = 1, n = 0;

		for (int i = 0; i < max_iter && abs(F(n) / F(d) - f0) > epsilon; i++) {
			z 	= one / frac(z);
			T	t = d * T(z) + d0;
			d0	= d;
			d	= t;
			n 	= T(f0 * F(d) + 0.5);
		}

		n += T(abs(f)) * d;
		return rational(f < 0 ? -n : n, d);
	}

	rational() {}
	template<typename T1,enable_if_t<is_signed<T1>, bool> = true>	rational(T1 n, T1 d) : n(d < 0 ? -n : n), d(abs(d)) {}
	template<typename T1,enable_if_t<!is_signed<T1>, bool> = true>	rational(T1 n, T1 d) : n(n), d(d) {}
	explicit rational(T n) : n(n), d(1) {}

	operator	float()	const	{ return float(n) / d;	}

	template<typename B>	friend rational	operator+(const rational &a, const rational<B> &b)	{ return rational(a.n * b.d + b.n * a.d, a.d * b.d); }
	template<typename B>	friend rational	operator-(const rational &a, const rational<B> &b)	{ return rational(a.n * b.d - b.n * a.d, a.d * b.d); }
	template<typename B>	friend rational	operator*(const rational &a, const rational<B> &b)	{ return rational(a.n * b.n, a.d * b.d); }
	template<typename B>	friend rational	operator/(const rational &a, const rational<B> &b)	{ return rational(a.n * b.d, a.d * b.n); }
	template<typename B>	friend rational	operator+(const rational &a, const B &b)			{ return rational(a.n + b * a.d, a.d); }
	template<typename B>	friend rational	operator-(const rational &a, const B &b)			{ return rational(a.n - b * a.d, a.d); }
	template<typename B>	friend rational	operator*(const rational &a, const B &b)			{ return rational(a.n * b, a.d); }
	template<typename B>	friend rational	operator/(const rational &a, const B &b)			{ return rational(a.n, a.d * b); }

	friend rational reciprocal(const rational &r)						{ return rational(r.d, r.n); }
	friend rational sfrac(const rational<T> &r)							{ return rational(r.n % r.d, r.d); }
	friend T		trunc(const rational<T> &r)							{ return r.n / r.d;	}
	friend T		round(const rational<T> &r)							{ return (r.n + r.d / 2) / r.d;	}
	friend rational mod(const rational<T> &a, const rational<T> &b)		{ return a - b * trunc(a / b); }
};

//-----------------------------------------------------------------------------
//	float functions
//-----------------------------------------------------------------------------

template<typename A, typename B> constexpr enable_if_t<!has_int_ops<A>, A>	div_round(A a, B b)			{ return round(a / b); }
template<typename A, typename B> constexpr enable_if_t<!has_int_ops<A>, A>	div_round_down(A a, B b)	{ return floor(a / b); }
template<typename A, typename B> constexpr enable_if_t<!has_int_ops<A>, A>	div_round_up(A a, B b)		{ return ceil(a / b); }

//constexpr float		select(bool b, float t, float f)	{ return b ? t : f;	}
//constexpr double	select(bool b, double t, double f)	{ return b ? t : f;	}

template<typename T> inline T sinc(const T &x) { return select(abs(x) < 1e-4f, one, sin(x) / x); }
template<typename T> inline T sinh(const T &x) { T ex = exp(x); return (ex - reciprocal(ex)) * half; }
template<typename T> inline T cosh(const T &x) { T ex = exp(x); return (ex + reciprocal(ex)) * half; }
template<typename T> inline T tanh(const T &x) { T ex = exp(x * two); return (ex - 1) / (ex + 1); }

// also known as logistic
template<typename T> inline T sigmoid(const T &x)	{ return reciprocal(one + exp(-x)); }
template<typename T> inline T softplus(const T &x)	{ return log(one + exp(x)); }

template<typename B>				inline float wrap(float a, const B &b)				{ return a - floor(a / b) * b;		}
template<typename B, typename C>	inline float wrap(float a, const B &b, const C &c)	{ return wrap(a - b, c - b) + b;	}

template<typename T> force_inline enable_if_t<is_num<T>, T> round(const T &v)	{ return floor(v + half); }

template<typename T>		force_inline bool is_inf(const T &f)	{ return get_components(f).is_inf(); }
template<typename T>		force_inline bool is_nan(const T &f)	{ return any(f != f); }
template<typename T, int N>	force_inline bool is_nan(const T (&f)[N]) {
	for (int i = 0; i < N; i++) {
		if (is_nan(f[i]))
			return true;
	}
	return false;
}

template<typename T> constexpr T pow(T v, int n) {
	return n < 0 ? reciprocal(pow(v, uint32(-n))) : pow(v, uint32(n));
}

// like mod, but returns non-negative remainder (i.e., 0 <= r < |b| always holds)
template<typename T> T nnmod(const T &a, const T &b) {
	T	r = a % b;
	if (get_sign(r))
		r += abs(b);
	return r;
}

//implemented in defs.cpp
float	lgamma(float x);
float	gamma(float x);
double	lgamma(double x);
double	gamma(double x);
double	digamma(double x);

float	erf(float x);
float	erfc(float x);
double	erf(double x);
double	erfc(double x);

//-----------------------------------------------------------------------------
// lerps, etc
//-----------------------------------------------------------------------------

template<typename X, typename Y, typename T>				force_inline auto	lerp(const X &x, const Y &y, const T &t) { return x + (y - x) * t; }
template<typename X, typename Y, typename Z, typename T>	force_inline auto	bilerp(const X &x, const Y &y, const Z &z, const T &t) { return x + (y - x) * t.x + (z - x) * t.y; }
template<typename T> force_inline int			lerp(int x, int y, const T &t)							{ return int(lerp(float(x), float(y), t) + 0.5f); }
template<typename T> force_inline int			bilerp(int x, int y, int z, const T &t)					{ return int(bilerp(float(x), float(y), float(z), t) + 0.5f); }

template<typename T> force_inline T				plerp(const T& a, const T& b, float f, float dt)		{ return lerp(b, a, pow(one - f, dt)); }
template<typename T> force_inline T				plerpr(const T& a, const T& b, float f, float dt)		{ return lerp(b, a, pow(f, dt)); }

template<typename T> force_inline T				hermite(const T &t)										{ return t * t * (3 - 2 * t); }
template<typename T> force_inline T				cheap_curve(const T &t)									{ return one - square(one - square(t)); }

template<typename T> force_inline T				linearstep(const T& min, const T& max, const T &t)		{ return (t - min) / (max - min); }
template<typename T> force_inline T				smoothstep(const T& min, const T& max, const T &t)		{ return hermite(linearstep(min, max, t)); }
template<typename T> force_inline T				cheapstep(const T& min, const T& max, const T &t)		{ return cheap_curve(linearstep(min, max, t)); }

template<typename X, typename T> force_inline T	smoothlerp(const X& a, const X& b, const T &t)			{ return lerp(a, b, hermite(t)); }
template<typename X, typename T> force_inline T	cheaplerp(const X& a, const X& b, const T &t)			{ return lerp(a, b, cheap_curve(t)); }

//-----------------------------------------------------------------------------
// trigonometry
//-----------------------------------------------------------------------------

template<typename X, typename Y> inline	X hypot(X x, Y y)	{ return sqrt(square(x) + square(y)); }
template<typename X, typename Y> inline	X rhypot(X x, Y y)	{ return rsqrt(square(x) + square(y)); }

namespace careful {
	template<typename X, typename Y> constexpr	X hypot_imp2(X x, Y y)	{ return x * sqrt(square(y / x) + one); }
	template<typename X, typename Y> constexpr	X hypot_imp1(X x, Y y)	{ return x > y ? hypot_imp2(x, y) : hypot_imp2(y, x); }
	template<typename X, typename Y> constexpr	X hypot(X x, Y y)		{ return hypot_imp1(abs(x), abs(y)); }

	template<typename X, typename Y> constexpr	X rhypot_imp2(X x, Y y)	{ return rsqrt(square(y / x) + one) / x; }
	template<typename X, typename Y> constexpr	X rhypot_imp1(X x, Y y)	{ return x > y ? rhypot_imp2(x, y) : rhypot_imp2(y, x); }
	template<typename X, typename Y> constexpr	X rhypot(X x, Y y)		{ return rhypot_imp1(abs(x), abs(y)); }
}

template<typename T> inline T sin_cos(T x)		{ return sqrt((one - x) * (one + x)); }

// sin(a+b) = sin(a)cos(b)+cos(a)sin(b)
template<typename T> inline T sin_add(T sina, T sinb) {
	return sina * sqrt((one - sinb) * (one + sinb)) + sqrt((one - sina) * (one + sina)) * sinb;
}
// sin(a-b) = sin(a)cos(b)-cos(a)sin(b)
template<typename T> inline T sin_sub(T sina, T sinb) {
	return sina * sqrt((one - sinb) * (one + sinb)) - sqrt((one - sina) * (one + sina)) * sinb;
}
// cos(a+b) = cos(a)cos(b)-sin(a)sin(b)
template<typename T> inline T cos_add(T cosa, T cosb) {
	return cosa * cosb - sqrt((one - cosa) * (one + cosa) * (one - cosb) * (one + cosb));
}
// cos(a-b) = cos(a)cos(b)+sin(a)sin(b)
template<typename T> inline T cos_sub(T cosa, T cosb) {
	return cosa * cosb + sqrt((one - cosa) * (one + cosa) * (one - cosb) * (one + cosb));
}

// double angle formulae
// sin(2a) = 2sin(a)cos(a)
template<typename T> inline T sin_twice(T sina) {
	return sina * sqrt((one - sina) * (one + sina)) * two;
}
// cos(2a) = 2cos2(a)-1
template<typename T> inline T cos_twice(T cosa) {
	return square(cosa) * two - one;
}
// tan(2a) = 2tan(a)/(1 - tan2(a))
template<typename T> inline T tan_twice(T tana) {
	return 2 * tana / ((one - tana) * (one + tana));
}

// half angle forumlae
// sin(a/2) = +-sqrt((1-cosa)/2)
template<typename T> inline auto sin_half(T sina) {
	return sqrt((one - sqrt((one - sina) * (one + sina))) * half);
}
// cos(a/2) = +-sqrt((1+cosa)/2)
template<typename T> inline auto cos_half(T cosa) {
	return sqrt((one + cosa) * half);
}

// triple angle formulae
// sin(3a) = 3 sin(a) - 4 sin3(a)
template<typename T> inline T sin_triple(T sina) {
	return sina * 3 - cube(sina) * 4;
}
// cos(3a) = -3 cos(a) + 4 cos3(a)
template<typename T> inline T cos_triple(T cosa) {
	return cosa * -3 + cube(cosa) * 4;
}
// tan(3a) = (3 tan(a) - tan3(a)) / (1 - 3 tan2(a))
template<typename T> inline T tan_triple(T tana) {
	return (tana * 3 - cube(tana)) / (one - square(tana) * 3);
}

//-----------------------------------------------------------------------------
//	constant trig
//-----------------------------------------------------------------------------

template<typename T> constexpr auto	degrees(const T &t)		{ return t * (pi / 180_k); }
template<typename T> constexpr auto	to_degrees(const T &t)	{ return t * (180_k / pi); }

force_inline	auto	cos(_zero)					{ return  one; 		}
force_inline	auto	cos(decltype( pi))			{ return -one; 		}
force_inline	auto	cos(decltype(-pi))			{ return -one; 		}
force_inline	auto	cos(decltype( pi/two))		{ return  zero; 	}
force_inline	auto	cos(decltype(-pi/two))		{ return  zero; 	}
force_inline	auto	cos(decltype( pi/four))		{ return  rsqrt2;	}
force_inline	auto	cos(decltype(-pi/four))		{ return -rsqrt2; 	}
force_inline	auto	cos(decltype( pi/8_k))		{ return  cos_half(cos(pi/four));	}

force_inline	auto	sin(_zero)					{ return  zero; 	}
force_inline	auto	sin(decltype( pi))			{ return  zero; 	}
force_inline	auto	sin(decltype(-pi))			{ return  zero; 	}
force_inline	auto	sin(decltype( pi/two))		{ return  one; 		}
force_inline	auto	sin(decltype(-pi/two))		{ return -one; 		}
force_inline	auto	sin(decltype( pi/four))		{ return  rsqrt2; 	}
force_inline	auto	sin(decltype(-pi/four))		{ return  rsqrt2; 	}
force_inline	auto	sin(decltype( pi/8_k))		{ return  sin_half(sin(pi/four));	}

template<typename X> force_inline	auto	tan(constant<X> x)		{ return sin(x) / cos(x); }
template<typename X> force_inline	auto	sincos(constant<X> x)	{ return make_pair(cos(x), sin(x)); }

//-----------------------------------------------------------------------------
//	complex
//-----------------------------------------------------------------------------

template<typename T> struct imaginary {
	T	i;
	imaginary()									{}
	explicit constexpr imaginary(T i) : i(i)	{}
};

template<typename T> struct complex {
	T	r, i;

	complex()										{}
	explicit constexpr complex(T r) : r(r), i(zero)	{}
	constexpr complex(T r, T i) : r(r), i(i)		{}

	constexpr T	mag2()			const	{ return square(r) + square(i); }
	constexpr T	mag()			const	{ return sqrt(mag2()); }
	constexpr T	arg()			const	{ return atan2(i, r); }

	template<typename B>	complex& operator+=(const complex<B> &b)	{ r += b.r; i += b.i; return *this; }
	template<typename B>	complex& operator-=(const complex<B> &b)	{ r += b.r; i -= b.i; return *this; }
	template<typename B>	complex& operator*=(const complex<B> &b)	{ return *this = *this * b; }
	template<typename B>	complex& operator/=(const complex<B> &b)	{ return *this = *this / b; }
	template<typename B>	complex& operator+=(const B &b)				{ r += b; return *this; }
	template<typename B>	complex& operator-=(const B &b)				{ r -= b; return *this; }
	template<typename B>	complex& operator*=(const B &b)				{ r *= b; i *= b; return *this; }
	template<typename B>	complex& operator/=(const B &b)				{ r /= b; i /= b; return *this; }
};

typedef imaginary<_one> _i;
extern _i	i;

constexpr _i	sqrt(decltype(-one))	{ return i; }
constexpr _i	rsqrt(decltype(-one))	{ return i; }

template<typename R, typename I> struct best_complex					{ typedef R type; };
template<typename R, typename I> struct best_complex<constant<R>, I>	{ typedef I type; };

template<typename T>				constexpr auto	make_imaginary(const T &t)				{ return imaginary<T>(t); }
template<typename R, typename I>	constexpr auto	make_complex(const R &r, const I &i)	{ return complex<typename best_complex<R,I>::type>(r, i); }
template<typename R>				constexpr auto	make_complex(const R &r)				{ return complex<R>(r, zero); }

template<typename T> bool approx_equal(const complex<T> &a, const complex<T> &b, T tol = ISO_TOLERANCE) {
	return approx_equal(a.r, b.r, tol) && approx_equal(a.i, b.i, tol);
}

template<typename A, typename B>	constexpr auto operator==(const imaginary<A> &a, const imaginary<B> &b)	{ return a.i == b.i;}
template<typename A, typename B>	constexpr auto operator==(const complex<A> &a, const imaginary<B> &b)	{ return a.r == zero && a.i == b.i;}
template<typename A, typename B>	constexpr auto operator==(const imaginary<A> &a, const complex<B> &b)	{ return b.i == zero && a.i == b.i;}
template<typename A, typename B>	constexpr auto operator==(const complex<A> &a, const complex<B> &b)		{ return a.r == b.r  && a.i == b.i;}

template<typename A, typename B>	constexpr auto operator!=(const imaginary<A> &a, const imaginary<B> &b)	{ return a.i != b.i;}
template<typename A, typename B>	constexpr auto operator!=(const complex<A> &a, const imaginary<B> &b)	{ return a.r != zero || a.i != b.i;}
template<typename A, typename B>	constexpr auto operator!=(const imaginary<A> &a, const complex<B> &b)	{ return b.i != zero || a.i != b.i;}
template<typename A, typename B>	constexpr auto operator!=(const complex<A> &a, const complex<B> &b)		{ return a.r != b.r  || a.i != b.i;}

template<typename A>				constexpr auto operator~(const imaginary<A> &a)							{ return make_imaginary(-a.i);}
template<typename A>				constexpr auto operator-(const imaginary<A> &a)							{ return make_imaginary(-a.i);}
template<typename A, typename B>	constexpr auto operator+(const imaginary<A> &a, const imaginary<B> &b)	{ return make_imaginary(a.i + b.i);}
template<typename A, typename B>	constexpr auto operator-(const imaginary<A> &a, const imaginary<B> &b)	{ return make_imaginary(a.i - b.i);}
template<typename A, typename B>	constexpr auto operator*(const imaginary<A> &a, const imaginary<B> &b)	{ return -a.i * b.i;}
template<typename A, typename B>	constexpr auto operator/(const imaginary<A> &a, const imaginary<B> &b)	{ return a.i / b.i; }
template<typename A, typename B>	constexpr auto operator+(const imaginary<A> &a, const B &b)				{ return make_complex(b, a.i); }
template<typename A, typename B>	constexpr auto operator-(const imaginary<A> &a, const B &b)				{ return make_complex(-b, a.i); }
template<typename A, typename B>	constexpr auto operator*(const imaginary<A> &a, const B &b)				{ return make_imaginary(a.i * b); }
template<typename A, typename B>	constexpr auto operator/(const imaginary<A> &a, const B &b)				{ return make_imaginary(a.i / b); }
template<typename A, typename B>	constexpr auto operator+(const A &a, const imaginary<B> &b)				{ return make_complex(a, b.i); }
template<typename A, typename B>	constexpr auto operator-(const A &a, const imaginary<B> &b)				{ return make_complex(a, -b.i); }
template<typename A, typename B>	constexpr auto operator*(const A &a, const imaginary<B> &b)				{ return make_imaginary(a * b.i); }
template<typename A, typename B>	constexpr auto operator/(const A &a, const imaginary<B> &b)				{ return make_imaginary(-a / b.i); }

template<typename A>				constexpr auto operator~(const complex<A> &a)							{ return make_complex(a.r, -a.i); }
template<typename A, typename B>	constexpr auto operator-(const complex<A> &a)							{ return make_complex(-a.r, - a.i);}
template<typename A, typename B>	constexpr auto operator+(const complex<A> &a, const complex<B> &b)		{ return make_complex(a.r + b.r, a.i + b.i);}
template<typename A, typename B>	constexpr auto operator-(const complex<A> &a, const complex<B> &b)		{ return make_complex(a.r - b.r, a.i - b.i);}
template<typename A, typename B>	constexpr auto operator*(const complex<A> &a, const complex<B> &b)		{ return make_complex(a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r);}
template<typename A, typename B>	constexpr auto operator/(const complex<A> &a, const complex<B> &b)		{ auto d = b.mag2(); return make_complex((b.r * a.r + b.i * a.i) / d, (b.r * a.i - b.i * a.r) / d); }
template<typename A, typename B>	constexpr auto operator+(const complex<A> &a, const imaginary<B> &b)	{ return make_complex(a.r, a.i + b.i);}
template<typename A, typename B>	constexpr auto operator-(const complex<A> &a, const imaginary<B> &b)	{ return make_complex(a.r, a.i - b.i);}
template<typename A, typename B>	constexpr auto operator*(const complex<A> &a, const imaginary<B> &b)	{ return make_complex(-a.i * b.i, a.r * b.i);}
template<typename A, typename B>	constexpr auto operator/(const complex<A> &a, const imaginary<B> &b)	{ return make_complex(a.i / b.i, -a.r / b.i); }
template<typename A, typename B>	constexpr auto operator+(const imaginary<A> &a, const complex<B> &b)	{ return make_complex(b.r, a.i + b.i);}
template<typename A, typename B>	constexpr auto operator-(const imaginary<A> &a, const complex<B> &b)	{ return make_complex(b.r, a.i - b.i);}
template<typename A, typename B>	constexpr auto operator*(const imaginary<A> &a, const complex<B> &b)	{ return make_complex(-a.i * b.i, a.i * b.r);}
template<typename A, typename B>	constexpr auto operator/(const imaginary<A> &a, const complex<B> &b)	{ auto t = a.i / b.mag2(); return make_complex(b.i * t, b.r * t); }
template<typename A, typename B>	constexpr auto operator+(const complex<A> &a, const B &b)				{ return make_complex(a.r + b, a.i); }
template<typename A, typename B>	constexpr auto operator-(const complex<A> &a, const B &b)				{ return make_complex(a.r - b, a.i); }
template<typename A, typename B>	constexpr auto operator*(const complex<A> &a, const B &b)				{ return make_complex(a.r * b, a.i * b); }
template<typename A, typename B>	constexpr auto operator/(const complex<A> &a, const B &b)				{ return make_complex(a.r / b, a.i / b); }

template<typename T>	auto	exp(const imaginary<T> &a)	{ T c, s; sincos(a.i, &s, &c); return complex<T>(c, s); }
template<typename T>	auto	ln(const imaginary<T> &a)	{ return complex<T>(ln(a.mag()), pi / two); }
template<typename T>	auto	sin(const imaginary<T> &a)	{ T e = exp(a.i), re = reciprocal(e); return make_imaginary((e - re) * half); }
template<typename T>	auto	cos(const imaginary<T> &a)	{ T e = exp(a.i), re = reciprocal(e); return (e + re) * half; }
template<typename T>	auto	tan(const imaginary<T> &a)	{ T e = exp(a.i), re = reciprocal(e); return make_imaginary((e - re) / (e + re)); }
template<typename T>	auto	sinh(const imaginary<T> &a)	{ return make_imaginary(sin(a.i)); }
template<typename T>	auto	tanh(const imaginary<T> &a)	{ return tan(a.i); }
template<typename T>	auto	cosh(const imaginary<T> &a)	{ T c, s; sincos(a.i, &s, &c); return complex<T>(c, s); }

template<typename T>	auto	ln(const complex<T> &a)		{ return complex<T>(ln(a.mag()), a.arg()); }
template<typename T>	auto	exp(const complex<T> &a)	{ T c, s, e = exp(a.r); sincos(a.i, &s, &c); return complex<T>(c * e, s * e); }
template<typename T>	auto	sin(const complex<T> &a)	{ T c, s, e = exp(a.i), re = reciprocal(e); sincos(a.r, &s, &c); return complex<T>(s * (e + re) * half,  c * (e - re) * half); }
template<typename T>	auto	cos(const complex<T> &a)	{ T c, s, e = exp(a.i), re = reciprocal(e); sincos(a.r, &s, &c); return complex<T>(c * (e + re) * half, -s * (e - re) * half); }
template<typename T>	auto	tan(const complex<T> &a)	{ T c, s, e = exp(a.i), re = reciprocal(e); sincos(a.r, &s, &c); return complex<T>(s * (e + re),  c * (e - re)) / complex<T>(c * (e + re), -s * (e - re)); }
template<typename T>	auto	sinh(const complex<T> &a)	{ T c, s, e = exp(a.r), re = reciprocal(e); sincos(a.i, &s, &c); return complex<T>((e - re) * c * half, (e + re) * s * half); }
template<typename T>	auto	cosh(const complex<T> &a)	{ T c, s, e = exp(a.r), re = reciprocal(e); sincos(a.i, &s, &c); return complex<T>((e + re) * c * half, (e + re) * s * half); }
template<typename T>	auto	tanh(const complex<T> &a)	{ T c, s, e = exp(a.r), re = reciprocal(e); sincos(a.i, &s, &c); return complex<T>(s * (e + re),  c * (e - re)) / complex<T>(c * (e + re), -s * (e - re)); }

//-----------------------------------------------------------------------------
//	misc
//-----------------------------------------------------------------------------

template<typename T> bool bigger(const T &a, const T &b) { return !any(a < b); }

template<typename T> T stable_interpolate(float a, T x, float b, T y) {
	a = max(a, 0);
	b = max(b, 0);
	return a > b ? y + (x - y) * (b / (a + b))
		: b == 0 ? (x + y) / 2
		: x + (y - x) * (a / (a + b));
}

inline bool approx_equal(float a, float b, float tol = ISO_TOLERANCE)			{ return abs(a - b) <= max(max(abs(a), abs(b)), 1) * tol; }
inline bool approx_greater_equal(float a, float b, float tol = ISO_TOLERANCE)	{ return a > b || approx_equal(a, b, tol); }
inline bool approx_less_equal(float a, float b, float tol = ISO_TOLERANCE)		{ return a < b || approx_equal(a, b, tol); }

inline bool approx_equal(double a, double b, double tol = ISO_TOLERANCE)		{ return abs(a - b) <= max(max(abs(a), abs(b)), 1) * tol; }
inline bool approx_greater_equal(double a, double b, double tol = ISO_TOLERANCE){ return a > b || approx_equal(a, b, tol); }
inline bool approx_less_equal(double a, double b, double tol = ISO_TOLERANCE)	{ return a < b || approx_equal(a, b, tol); }

template<typename T, int RTOL = 100000> struct _approx {
	T	t;
	_approx(T &&t) : t(forward<T>(t))	{}
	operator T()	const				{ return t;}
	friend bool	operator==(_approx a, const T &b)		{ return approx_equal(a.t, b, 1.f / RTOL); }
	friend bool	operator==(const T &a, const _approx b)	{ return approx_equal(a, b.t, 1.f / RTOL); }
	friend bool	operator<=(_approx a, const T &b)		{ return approx_less_equal(a.t, b, 1.f / RTOL); }
	friend bool	operator<=(const T &a, const _approx b)	{ return approx_less_equal(a, b.t, 1.f / RTOL); }
	friend bool	operator>=(_approx a, const T &b)		{ return approx_greater_equal(a.t, b, 1.f / RTOL); }
	friend bool	operator>=(const T &a, const _approx b)	{ return approx_greater_equal(a, b.t, 1.f / RTOL); }
};

template<typename T> inline _approx<T> approx(T &&t) { return forward<T>(t); }

template<typename T> bool find_scalar_multiple(T a, T b, float &scale) {
	if (a == zero) {
		if (b != zero)
			return false;
		scale = 0;
	} else {
		scale = b / a;
	}
	return true;
}

template<typename T> bool is_scalar_multiple(T a, T b, float scale) {
	return approx_equal(T(a * scale), b);
}


// From http://www.math.uic.edu/~jan/mcs471/Lec9/gss.pdf
// golden section search for minima
template<typename X, typename F> auto gss(X x0, X x3, X threshold, F&& func) {
	static const X	phi = (sqrt(5) - 1) / 2;	 // Golden Ratio

	X		x1		= phi * x0 + (1 - phi) * x3;
	auto	fx1		= func(x1);
	X		x2		= (1 - phi) * x0 + phi * x3;
	auto	fx2		= func(x2);

	while (abs(x3 - x0) > threshold) {
		if (fx1 < fx2) {
			x3		  = x2;
			x2		  = x1;
			fx2		  = fx1;
			x1		  = phi * x0 + (1 - phi) * x3;
			fx1		  = func(x1);
		} else {
			x0		  = x1;
			x1		  = x2;
			fx1		  = fx2;
			x2		  = (1 - phi) * x0 + phi * x3;
			fx2		  = func(x2);
		}
	}
	return make_pair(min(fx1, fx2), (x3 + x0) / 2);
}


// defined in defs.cpp
extern const uint16 primes[2048];

}	//namespace iso

#endif	// MATHS_H
