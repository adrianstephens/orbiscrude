#ifndef SOFT_FLOAT_H
#define SOFT_FLOAT_H

#include "defs.h"
#include "bits.h"
#include "constants.h"

namespace iso {
#ifndef MATHS_H
	//float	abs(float);
	//double	abs(double);
	float	copysign(float, float);
#endif

//-----------------------------------------------------------------------------
//	encode/decode read/write floats
//-----------------------------------------------------------------------------

template<typename T, typename F> T encode_float(F f, uint32 M, uint32 E, bool S) {
	if (!S && f < 0)
		return 0;

	float_components<F>	t(f);
	int		e	= t.get_exp() + (1 << (E - 1)) - 1;

	if (e >= (1 << E) - 1)
		return bits(M + E) | (t.s << (M + E));

	T	m	= shift_bits_round(T(t.get_mant()), M - t.M);
	if (e < 0) {
		m >>= -e;
		e = 0;
	}
	return (m & bits<T>(M)) | ((e + (t.s << E)) << M);
}

template<typename F, typename T> F decode_float(T v, uint32 M, uint32 E, bool S) {
	typedef float_components<F>	C;

	T		m = v & bits<T>(M);
	int		e = (v >> M) & bits(E);
	int		s = (v >> (M + E)) & int(S);

	if (e == 0) {
		if (m == 0)
			return 0;

		int	h = highest_set_index(m);
		e	= h - M;
		m	= shift_bits_round(m, C::M - h);
	} else {
		m	= shift_bits_round(m | bit<T>(M), C::M - M);
	}

	e	+= C::E_OFF - ((1 << (E - 1)) - 1);
	if (e < 0) {
		m >>= -e;
		e = 0;
	}
	return C(m, e, s).f();
}

template<typename F> inline uint32	encode_float(F f, uint32 M, uint32 E, bool S) { return encode_float<uint32>(f, M, E, S); }
template<typename T> inline float	decode_float(T v, uint32 M, uint32 E, bool S) { return decode_float<float>(v, M, E, S); }

template<typename T, typename F> F read_float(const void *data, uint32 nbits, uint32 E, bool S) {
	return decode_float<F, T>(read_bytes<T>(data, (nbits + 7) / 8), nbits - E - int(S), E, S);
}
template<typename F> F read_float(const void *data, uint32 nbits, uint32 E, bool S) {
	return nbits > 32
		? read_float<uint64, F>(data, nbits, E, S)
		: read_float<uint32, F>(data, nbits, E, S);
}

template<typename T, typename F> void write_float(const void *data, F f, uint32 nbits, uint32 E, bool S) {
	write_bytes(data, encode_float<T>(f, nbits - E - int(S), E, S), (nbits + 7) / 8);
}
template<typename F> void write_float(const void *data, F f, uint32 nbits, uint32 E, bool S) {
	if (nbits > 32)
		write_float<uint64>(data, f, nbits, E, S);
	else
		write_float<uint32>(data, f, nbits, E, S);
}

//-----------------------------------------------------------------------------
//	user-defined literal helpers
//-----------------------------------------------------------------------------

template<char... C>	struct signed_reader			: digits_reader<10, 0, C...> {};
template<char... C>	struct signed_reader<'+', C...> : digits_reader<10, 0, C...> {};
template<char... C>	struct signed_reader<'-', C...> { static const int64 value = -(int64)digits_reader<10, 0, C...>::value; };

template<typename T, typename M, typename E> T make_float(T_type<T>, M mant, E exp);
template<typename T, typename M, typename E> T make_float(M mant, E exp) { return make_float(T_type<T>(), mant, exp); }

// (up to) 64 bit mantissa
template<typename T, uint64 I, int D, char... C>			struct float_reader;
template<typename T, uint64 I, int D, char C0, char... C>	struct float_reader<T, I, D, C0, C...> : float_reader<T, I * 10 + C0 - '0', D + 1, C...> {};

template<typename T, uint64 I, int D> struct float_reader<T, I, D> {
	static constexpr int		exp()	{ return -D; }
	static constexpr int64		mant()	{ return I; }
	static T					get()	{ return make_float<T>(I, 0); }
};
template<typename T, uint64 I, int D, char... C> struct float_reader<T, I, D, '.', C...> {
	static T		get()	{
		typedef float_reader<T, I, 0, C...> R;
		return make_float<T>(R::mant(), R::exp());
	}
};
template<typename T, uint64 I, int D, char... C> struct float_reader<T, I, D, 'e', C...> {
	static constexpr int		exp()	{ return signed_reader<C...>::value - D; }
	static constexpr int64		mant()	{ return I; }
	static T					get()	{ return make_float<T>(mant(), exp() + D); }
};

// 128 bit mantissa
template<typename T, uint64 I0, uint64 I1, int D, char... C>	struct float_reader128;
template<typename T, uint64 I0, uint64 I1, int D, char C0, char... C>	struct float_reader128<T, I0, I1, D, C0, C...> : float_reader128<T, I0 * 10 + C0 - '0', I1 * 10 + (((I0>>32) * 10)>>32), D + 1, C...> {};

template<typename T, uint64 I0, uint64 I1, int D> struct float_reader128<T, I0, I1, D> {
	static constexpr int		exp()	{ return -D; }
	static constexpr uint128	mant()	{ return iso::lo_hi(I0, I1); }
	static T					get()	{ return make_float<T>(mant(), 0); }
};
template<typename T, uint64 I0, uint64 I1, int D, char... C> struct float_reader128<T, I0, I1, D, '.', C...> {
	static T		get()	{
		typedef float_reader<T, I0, I1, 0, C...> R;
		return make_float<T>(R::mant(), R::exp());
	}
};
template<typename T, uint64 I0, uint64 I1, int D, char... C> struct float_reader128<T, I0, I1, D, 'e', C...> {
	static constexpr int		exp()	{ return signed_reader<C...>::value - D; }
	static constexpr uint128	mant()	{ return iso::lo_hi(I0, I1); }
	static T					get()	{ return make_float<T>(mant(), exp() + D); }
};

//-----------------------------------------------------------------------------
//	soft_float - arbitrary mantissa, exponent sizes
//-----------------------------------------------------------------------------

struct mantissa_low64 { uint64 m0; };

template<uint32 _M, uint32 E, bool S, uint32 T = int(S) + E + _M> struct soft_float_storage {
	typedef	uint_bits_t<T>	mant_t;
	enum { E_OFF = (1 << (E - 1)) - 1, M = _M, M64 = M, MB = sizeof(mant_t) * 8 };
	mant_t	m:_M, e:E, s:1;
	void	set(bool sign, int exp, mant_t mant) { m = mant; e = exp + E_OFF; s = sign; }
	constexpr bool		get_sign()		const	{ return s; }
	constexpr int		get_exp()		const	{ return e - E_OFF; }
	constexpr int		get_dexp()		const	{ return (e ? e : highest_set_index(m) - M) - E_OFF; }
	constexpr mant_t	get_mant()		const	{ return m | (e ? (mant_t(1) << M) : 0); }
	constexpr uint64	get_mant64()	const	{ return m; }
};

template<uint32 _M, uint32 E, bool S> struct soft_float_storage<_M, E, S, 128> : mantissa_low64, soft_float_storage<_M - 64, E, S, 64> {
	typedef soft_float_storage<_M - 64, E, S, 64>	B;
	enum { M = _M, M64 = 63, MB = 128 };
	typedef uint128	mant_t;
	void	set(bool sign, int exp, uint128 mant) { B::set(sign, exp, hi(mant)); this->m0 = lo(mant); }
	uint128	get_mant()		const	{ return lo_hi(this->m0, B::get_mant()); }
	uint64	get_mant64()	const	{ return hi(get_mant() << (127 - M)); }
};

template<uint32 M, uint32 E, bool S, bool BIG=(M > 52 || E > 11)> class soft_float_imp : comparisons<soft_float_imp<M, E, S>>, soft_float_storage<M, E, S> {
	typedef soft_float_storage<M, E, S>					B;
	typedef if_t<(M > 23 || E > 8), double, float>		F;
	template<uint32 M2, uint32 E2, bool S2, bool DPD>	friend class soft_decimal;
	using typename B::mant_t;

	void	check_set(bool sign, int exp, mant_t mant) {
		if (!mant || (!S && sign)) {
			clear(*this);
			return;
		}
		if (exp + B::E_OFF < 0) {
			mant	>>= -(exp + B::E_OFF);
			exp		= -B::E_OFF;

		} else if (exp + B::E_OFF >= (1 << E) - 1) {
			mant	= 0;
			exp		= (1 << E) - 1 - B::E_OFF;
		}
		this->set(sign, exp, mant);
	}
	void	normalise_set(bool sign, int exp, mant_t mant) {
		int	s	= M - highest_set_index(mant);
		check_set(sign, exp - s, shift_bits_round(mant, s));
	}

	// to allow us to specialise:
	static soft_float_imp add1(mant_t am, int ae, bool as, mant_t bm, bool bs) { // requires a >= b
		return {as, ae, as ^ bs ? am - bm : am + bm};
	}
	static soft_float_imp add(mant_t am, int ae, bool as, mant_t bm, int be, bool bs) {
		return ae < be || (ae == be && am < bm)
			? add1(bm, be, bs, am >> (be - ae), as)
			: add1(am, ae, as, bm >> (ae - be), bs);
	}
	static soft_float_imp mul(mant_t am, mant_t bm, int e, bool s) {
		mant_t	lo;
		return {s, e + 1, mulc(mant_t(am << (B::MB - M - 1)), bm, lo)};
	}
	static soft_float_imp div(mant_t am, mant_t bm, int e, bool s) {
		return {s, e - 1, fulldivc(mant_t(0), am, mant_t(bm << (B::MB - M - 1)))};
	}

	soft_float_imp(bool sign, int exp, mant_t mant)			{ normalise_set(sign, exp, mant); }
//	soft_float_imp(bool sign, int exp, mant_t mant, bool)	{ check_set(sign, exp, mant); }
public:
	static soft_float_imp make(mant_t mant, int exp)		{ return soft_float_imp(false, M, mant) * pow(soft_float_imp(10), exp); }

	force_inline soft_float_imp()							{}
	force_inline soft_float_imp(const _zero&)				{ clear(*this); }
	force_inline soft_float_imp(const _one&)				{ this->set(false, 0, mant_t(1) << M); }
	force_inline explicit soft_float_imp(int i)				{ int s = highest_set_index(i); normalise_set(i < 0, s, shift_bits_round<mant_t>(i < 0 ? -i : i, M - s)); }
	force_inline explicit soft_float_imp(int64 i)			{ int s = highest_set_index(i); check_set(i < 0, s, shift_bits_round<mant_t>(i < 0 ? -i : i, M - s)); }
	force_inline explicit soft_float_imp(F f)				{ auto t = get_components(f); normalise_set(f < 0, t.get_exp(), shift_bits_round<mant_t>(t.get_mant(), M - t.M)); }
	template<uint32 M2, uint32 E2, bool S2> force_inline soft_float_imp(const soft_float_imp<M2, E2, S2> &f)	{ normalise_set(f.get_sign(), f.get_exp(), shift_bits_round<mant_t>(f.get_mant(), M - M2)); }

	force_inline explicit operator F()		const			{ return float_components<F>(shift_bits_round(B::get_mant64(), float_components<F>::M - B::M64), B::get_exp() + float_components<F>::E_OFF, B::get_sign()).f(); }
	force_inline explicit operator bool()	const			{ return B::e != 0; }

	force_inline soft_float_imp&	operator=(F f)			{ return *this = (soft_float_imp)f; }

	force_inline soft_float_imp		operator+(const soft_float_imp &b) const { return add(B::get_mant(), B::get_exp(), B::get_sign(), b.get_mant(), b.get_exp(), b.get_sign()); }
	force_inline soft_float_imp		operator-(const soft_float_imp &b) const { return add(B::get_mant(), B::get_exp(), B::get_sign(), b.get_mant(), b.get_exp(), !b.get_sign()); }
	force_inline soft_float_imp		operator*(const soft_float_imp &b) const { return mul(B::get_mant(), b.get_mant(), B::get_exp() + b.get_exp(), B::get_sign() ^ b.get_sign()); }
	force_inline soft_float_imp		operator/(const soft_float_imp &b) const { return div(B::get_mant(), b.get_mant(), B::get_exp() - b.get_exp(), B::get_sign() ^ b.get_sign()); }

	force_inline soft_float_imp		operator+(F b) const	{ return operator+(soft_float_imp(b)); }
	force_inline soft_float_imp		operator-(F b) const	{ return operator-(soft_float_imp(b)); }
	force_inline soft_float_imp		operator*(F b) const	{ return operator*(soft_float_imp(b)); }
	force_inline soft_float_imp		operator/(F b) const	{ return operator/(soft_float_imp(b)); }

	template<typename B> force_inline auto&	operator+=(B b)	{ return *this = *this + b;	}
	template<typename B> force_inline auto&	operator-=(B b)	{ return *this = *this - b;	}
	template<typename B> force_inline auto&	operator*=(B b)	{ return *this = *this * b;	}
	template<typename B> force_inline auto&	operator/=(B b)	{ return *this = *this / b;	}
	force_inline auto&	operator++()						{ return *this += one; }
	force_inline auto&	operator--()						{ return *this -= one; }
	force_inline auto	operator++(int)						{ auto t = *this; operator++(); return t; }
	force_inline auto	operator--(int)						{ auto t = *this; operator--(); return t; }

	friend force_inline soft_float_imp operator-(soft_float_imp a)	{ a.s ^= 1; return a; }
	friend force_inline soft_float_imp abs(soft_float_imp a)		{ a.s = 0; return a; }

	friend soft_float_imp reciprocal(const soft_float_imp &a) {
		auto	m = a.get_mant();
		return soft_float_imp(a.get_sign(), -a.get_exp() - int(!is_pow2(m)), reciprocalc(mant_t(m << (B::MB - M - 1))) >> (B::MB - M - 1));
	}

	friend soft_float_imp fma(const soft_float_imp &a, const soft_float_imp &b, const soft_float_imp &c) {
		int		me = a.get_exp() + b.get_exp() + 1,	ce = c.get_exp();
		bool	ms = a.get_sign() ^ b.get_sign(),	cs = c.get_sign();

		mant_t	mm0, mm1 = mulc(mant_t(a.get_mant() << (B::MB - M - 1)), b.get_mant(), mm0);
		mant_t	cm	= c.get_mant();

		if (me == ce && ms != cs) {
			auto	m	= mm1 - cm;
			int		s	= M - highest_set_index(m);
			return {ms, me - s, double_shift_left_hi(mm0, m, s)};
		}

		return me < ce
			? add1(cm, ce, cs, mm1 >> (ce - me), ms)
			: add1(mm1, me, ms, cm >> (me - ce), cs);
	}

	friend soft_float_imp fmma(const soft_float_imp &a, const soft_float_imp &b, const soft_float_imp &c, const soft_float_imp &d) {
		int		e1 = a.get_exp() + b.get_exp() + 1,	e2 = c.get_exp() + d.get_exp() + 1;
		bool	s1 = a.get_sign() ^ b.get_sign(),	s2 = c.get_sign() ^ d.get_sign();

		mant_t	lo1, hi1 = mulc(mant_t(a.get_mant() << (B::MB - M - 1)), b.get_mant(), lo1);
		mant_t	lo2, hi2 = mulc(mant_t(c.get_mant() << (B::MB - M - 1)), d.get_mant(), lo2);

		if (e1 == e2 && s1 != s2) {
			hi1			= hi1 - hi2 - T(subc(lo1, lo2, false, lo1));
			int		s	= M - highest_set_index(hi1);
			return {s1, e1 - s, double_shift_left_hi(lo1, hi1, s)};
		}

		return e1 < e2 || (e1 == e2 && (hi1 < hi2 || hi1 == hi2 && lo1 < lo2))
			? add1(hi2, e2, s2, hi(lo_hi(lo1, hi1) >> (e2 - e1)), s1)
			: add1(hi1, e1, s1, hi(lo_hi(lo2, hi2) >> (e1 - e2)), s2);
	}

	friend force_inline int compare(const soft_float_imp &a, const soft_float_imp &b) {
		int	c = a.s != b.s ? 1 : simple_compare(a.get_exp(), b.get_exp());
		if (c == 0)
			c = compare(a.get_mant(), b.get_mant());
		return a.s ? -c : c;
	}
	friend soft_float_imp trunc(const soft_float_imp &a) {
		int	e		= a.get_exp();
		return e < 0 ? zero : soft_float_imp(a.get_sign(), e, a.get_mant() & ~bits<mant_t>(M - e));
	}
	friend soft_float_imp floor(const soft_float_imp &a) {
		int		e		= a.get_exp();
		mant_t	m		= a.get_mant();
		mant_t	mask	= e < M ? bits<mant_t>(M - max(e, 0)) : 0;
		soft_float_imp	b	= soft_float_imp(a.get_sign(), e, m & ~mask);
		return a.get_sign() && (m & mask) ? b - one : b;
	}
	friend force_inline float_category	get_category(const soft_float_imp &a)	{
		return a.e == (1 << E) - 1 ? (a.m ? FLOAT_QNAN : (a.s ? FLOAT_NEGINF : FLOAT_INF)) : FLOAT_NORMAL;
	}
	friend B	get_components(const soft_float_imp &a) { return a; }
};

//-------------------------------------
// special cases
//-------------------------------------
template<uint32 E, bool S, uint32 T> struct soft_float_storage<63, E, S, T> {
	enum			{ E_OFF = 16383, M = 63, M64 = M, MB = 64 };
	typedef uint64	mant_t;
	packed<uint64>	m;
	uint_bits_t<E + int(S)>	e:E, s:S;

	void	set(bool sign, int exp, uint64 mant) { m = mant; e = exp + E_OFF; s = sign; }
	constexpr bool		get_sign()		const	{ return s; }
	constexpr int		get_exp()		const	{ return e - E_OFF; }
	constexpr int		get_dexp()		const	{ return (e ? e : highest_set_index(m) - M) - E_OFF; }
	constexpr uint64	get_mant()		const	{ return m; }
	constexpr uint64	get_mant64()	const	{ return m; }
};

template<> soft_float_imp<63, 15, 1> inline soft_float_imp<63, 15, 1>::div(uint64 am, uint64 bm, int e, bool s) {
	return {s, e, fulldivc(am << 63, am >> 1, bm)};
}

template<> soft_float_imp<63, 15, 1> inline soft_float_imp<63, 15, 1>::add1(uint64 am, int ae, bool as, uint64 bm, bool bs) {
	return as ^ bs
		? soft_float_imp<63, 15, 1>(as, ae, am - bm)
		: addc(am, bm, false, am)
			? soft_float_imp<63, 15, 1>(as, ae + 1, (am >> 1) | bit64(63))
			: soft_float_imp<63, 15, 1>(as, ae, am);
}

//-------------------------------------
// small	- use built-in types
//-------------------------------------
template<uint32 M, uint32 E, bool S> class soft_float_imp<M, E, S, false> : soft_float_storage<M, E, S> {
	typedef soft_float_storage<M, E, S>				B;
	typedef if_t<(M > 23 || E > 8), double, float>	F;
	using typename B::mant_t;

	void	check_set(bool sign, int exp, mant_t mant) {
		if (!mant || (!S && sign)) {
			clear(*this);
			return;
		}
		if (exp + B::E_OFF < 0) {
			mant	>>= -(exp + B::E_OFF);
			exp		= -B::E_OFF;

		} else if (exp + B::E_OFF >= (1 << E) - 1) {
			mant	= 0;
			exp		= (1 << E) - 1 - B::E_OFF;
		}
		this->set(sign, exp, mant);
	}
	void	normalise_set(bool sign, int exp, mant_t mant) {
		int	s	= M - highest_set_index(mant);
		check_set(sign, exp - s, shift_bits_round(mant, s));
	}
public:
	soft_float_imp() {}
	force_inline constexpr soft_float_imp(F f)	{ auto t = get_components(f); normalise_set(f < 0, t.get_exp(), shift_bits_round(t.get_mant(), M - t.M)); }
	force_inline constexpr operator F() const	{ return float_components<F>(B::get_mant() << (float_components<F>::M - M), B::get_exp() + float_components<F>::E_OFF, B::get_sign()).f(); }

	template<typename B> force_inline auto&	operator+=(B b)	{ return *this = *this + b;	}
	template<typename B> force_inline auto&	operator-=(B b)	{ return *this = *this - b;	}
	template<typename B> force_inline auto&	operator*=(B b)	{ return *this = *this * b;	}
	template<typename B> force_inline auto&	operator/=(B b)	{ return *this = *this / b;	}
};

/*
template<uint32 M, uint32 E, bool S> soft_float_imp<M,E,S>::operator float() const {
	static struct soft_float_lookup {
		float	table[1 << (M + E + int(S))];
		soft_float_lookup() {
			void soft_float_make_table(float *p, uint32 mb, uint32 eb, bool sb);
			soft_float_make_table(table, M, E, S);
		}
	} table;
	return table.table[i];
}
*/

//-------------------------------------
//	traits
//-------------------------------------

template<uint32 M, uint32 E, bool S> struct num_traits<soft_float_imp<M,E,S> > : float_traits<M,E,S> {
	typedef soft_float_imp<M,E,S>	T;
	static constexpr T	max()		{ return force_cast<T>((1 << (M + E - 1)) - 1); }
	static constexpr T	min()		{ return S ? -max() : 0; }
	template<typename T2> static constexpr T cast(T2 t)	{ return T(t); }
};

template<typename T, uint32 M, uint32 E, bool S> struct constant_cast<T, soft_float_imp<M,E,S>> {
	static constexpr auto f()	{ return T::template as<soft_float_imp<M,E,S>>(); }
};

template<uint32 M, uint32 E, bool S> struct soft_float_type	: T_type<soft_float_imp<M, E, S>> {};
template<uint32 M, uint32 E, bool S> using soft_float = typename soft_float_type<M, E, S>::type;

template<> struct soft_float_type<23, 8, true>	: T_type<float>		{};
template<> struct soft_float_type<52, 11, true>	: T_type<double>	{};
#ifdef HAS_FP16
template<> struct soft_float_type<10,5, true>	: T_type<__fp16>	{};
#endif
#ifdef _M_I86
template<> struct soft_float_type<63,15,true>	: T_type<long double>	{};
#endif

typedef soft_float<10,5,true>	float16;
typedef soft_float<63,15,true>	float80;

typedef soft_float<32,15,true>	float48;
typedef soft_float<112,15,true>	float128;

typedef BE(float16)				float16be;
typedef LE(float16)				float16le;

typedef soft_float<11,5,false>	ufloat16;
typedef BE(ufloat16)			ufloat16be;
typedef LE(ufloat16)			ufloat16le;

template<uint32 M, uint32 E, bool S, typename T> soft_float<M, E, S> make_float(T_type<soft_float_imp<M, E, S>>, T mant, int exp) {
	return soft_float<M, E, S>::make(mant, exp);
}
inline float16		operator"" _f16(unsigned long long int x)	{ return float16((int64)x); }
inline float16		operator"" _f16(long double x)				{ return float16((float)x); }
inline float32		operator"" _f32(unsigned long long int x)	{ return x; }
inline float32		operator"" _f32(long double x)				{ return x; }
inline float64		operator"" _f64(unsigned long long int x)	{ return x; }
inline float64		operator"" _f64(long double x)				{ return x; }
template<char... C> float80		operator"" _f80()	{ return float_reader<float80, 0, 0, C...>::get(); }
template<char... C> float128	operator"" _f128()	{ return float_reader128<float128, 0, 0, 0, C...>::get(); }

template<uint32 M, uint32 E, bool S>	constexpr uint32 BIT_COUNT<soft_float_imp<M, E, S>>	= E + M + int(S);

//-----------------------------------------------------------------------------
//	soft_decimal
//-----------------------------------------------------------------------------

// in defs.cpp
template<typename T> size_t put_decimal(char *s, T m, int dp, bool sign);

template<typename T> struct soft_decimal_helpers {
	static bool mul10(T &m) {
		if (m > ~T(0) / 10)
			return false;
		m *= 10;
		return true;
	}
	static int mul_pow10(T &m, int n) {
		while (n > 0) {
			if (!mul10(m))
				break;
			--n;
		}
		return n;
	}
	static int div_pow10(T &m, uint32 n) {
		if (n == 0)
			return 0;
		T	pow10	= pow(T(10), n);
		T	mod		= m % 10;
		m			/= pow10;
		return (mod >= (pow10 >> 1)) * 2 + int(mod > 0 && mod != (pow10 >> 1));
	}
	static int div_pow10(T &m, T hi, uint32 n) {
		m = fulldivc(m, hi, pow(T(10), n));
		return 0;
	}
	static int compare_unsigned(T am, int ae, T bm, int be) {
		int	g	= ae < be ?  div_pow10(am, mul_pow10(bm, be - ae))
				: ae > be ? -div_pow10(bm, mul_pow10(am, ae - be))
				: 0;
		int	c	= simple_compare(am, bm);
		return c ? c : g;
	}

	template<int DIGITS> static typename T_enable_if<(DIGITS > 3), int>::type strip_zeros(uint128 &m, int e) {
		enum {D0 = DIGITS / 2, D1 = DIGITS - D0};
		if (e < D1 && !(m % kpow<10, D0>)) {
			m /= kpow<10, D0>;
			e += D0;
		}
		return strip_zeros<D1>(m, e - D0) + D0;
	}
	template<int DIGITS> static typename T_enable_if<(DIGITS <= 3), int>::type strip_zeros(uint128 &m, int e) {
		while (e < DIGITS && !(m % 10)) {
			m /= 10;
			++e;
		}
		return e;
	}

	static bool add(T &am, int &ae, bool as, T bm, int be, bool bs) {
		if (ae < be) {
			int	n = mul_pow10(bm, be - ae);
			div_pow10(am, n);
			ae = ae + n;
		} else if (ae > be) {
			int	n = mul_pow10(am, ae - be);
			div_pow10(bm, n);
			ae = be + n;
		}

		if (as != bs) {
			if (am < bm) {
				am = bm - am;
				as = bs;
			} else {
				am = am - bm;
			}
		} else {
			am += bm;
		}

		return as;
	}

	static int mul(T &am, T bm, int digits) {
		T	hi	= mulc(am, bm, am);
		if (!hi) {
			int	d	= highest_set_index(am) * 3 / 10 - digits + 1;
			if (d < 0)
				return 0;
			div_pow10(am, uint32(d));
			return d;
		}
		uint32	d	= (highest_set_index(hi) + sizeof(T) * 8) * 3 / 10 - digits + 1;
		div_pow10(am, hi, uint32(d));
		return d;
	}

	static int div(T &am, T bm, int digits) {
		int	d	= (highest_set_index(am) - highest_set_index(bm)) * 3 / 10 + 1;
		if (d < 0)
			am = am * pow(T(10), uint32(-d));
		T	lo, hi	= mulc(am, pow(T(10), uint32(digits - max(d, 0))), lo);
		am = fulldivc(lo, hi, bm);
		return d;
	}

};

template<> inline bool soft_decimal_helpers<uint128>::mul10(uint128 &m) {
	if (hi(m) > ~uint64(0) / 10)
		return false;
	uint64	tlo	= lo(m), thi = hi(m);
	uint64	t	= (thi << 2) | (tlo >> (64 - 2));
	thi = thi + t + addc(tlo, tlo << 2, false, tlo);
	m = iso::lo_hi(tlo << 1, (thi << 1) | (tlo >> (64 - 1)));
	return true;
}

template<> inline int soft_decimal_helpers<uint128>::div_pow10(uint128 &m, uint32 n) {
	if (n == 0)
		return 0;

	uint64	mod;
	bool	sticky	= false;
	while (n > 9) {
		uint64	pow10	= kpow<10, 9>;
		m				= divmod(m, pow10, mod);
		sticky			= sticky || !!mod;
		n -= 9;
	}
	uint64	pow10	= pow(10u, n);
	m				= divmod(m, pow10, mod);
	return (mod >= pow10 / 2) * 2 + int(sticky || (mod && mod != pow10 / 2));
}

template<uint32 M, uint32 E, bool S, bool DPD, typename T = uint_bits_t<int(S) + E + M>> struct soft_decimal_storage {
	typedef T	mant_t;
	enum { MC = M - 3, EC = E - 2, E_OFF = 3 << (EC - 1), MB = sizeof(mant_t) * 8, DIGITS = MC * 3 / 10 + 1, DIGITS64 = DIGITS };
	T	m:MC, e:EC, c:5, s:1;

	static const mant_t	max_store = kpow<10, DIGITS>;

	void	set(bool sign, int exp, T mant) {
		exp		+= E_OFF;
		int	mtop = (mant >> MC);
		c	= mtop | (mtop & 8 ? ((exp >> EC) << 1) | 0x10 : ((exp >> EC) << 3));
		s	= sign;
		e	= exp;
		m	= mant;
	}
	void	set_exp(int exp) {
		exp += E_OFF;
		e = exp;
		c = c >= 0x18 ? (c & ~6) | ((exp >> EC) << 1) : (c & ~0x18) | ((exp >> EC) << 3);
	}
	void	set64(bool sign, int exp, uint64 mant)	{ set(sign, exp, T(mant)); }
	bool	get_sign()		const					{ return s; }
	int		get_exp()		const					{ return int(e + (c >= 0x18 ? (c & 6) << (EC - 1) : (c & 0x18) << (EC - 3))) - E_OFF; }
	T		get_mant()		const					{ return m + (mant_t(c & (c >= 0x18 ? 9 : 7)) << MC); }
};

template<uint32 M, uint32 E, bool S, typename T> struct soft_decimal_storage<M, E, S, true, T> : soft_decimal_storage<M, E, S, false, T> {
	typedef soft_decimal_storage<M, E, S, false, T> B;
	void	set(bool sign, int exp, T mant)			{ B::set(sign, exp, (T)bin_to_dpd(mant)); }
	void	set64(bool sign, int exp, uint64 mant)	{ set(sign, exp, T(mant)); }
	T		get_mant()		const					{ return (T)dpd_to_bin(B::get_mant()); }
};

template<uint32 M, uint32 E, bool S> struct soft_decimal_storage<M, E, S, false, uint128> : mantissa_low64, soft_decimal_storage<M - 64, E, S, false, uint64> {
	typedef uint128							mant_t;
	typedef soft_decimal_storage<M - 64, E, S, false, uint64>	B;
	enum { MC = M - 3, MB = 128, DIGITS = MC * 3 / 10 + 1, DIGITS64 = (64 - 4) * 3 / 10 + 1 };

	static const uint128	max_store;

	void	set(bool sign, int exp, uint128 mant)	{ B::set(sign,  exp, hi(mant)); m0	= lo(mant);	}
	void	set64(bool sign, int exp, uint64 mant)	{ set(sign, exp + DIGITS - DIGITS64, uint128(mant)); }
	uint128	get_mant()	const						{ return uint128(m0, B::get_mant()); }
};

template<uint32 M, uint32 E, bool S> const uint128 soft_decimal_storage<M, E, S, false, uint128>::max_store = pow(uint128(10), uint32(soft_decimal_storage<M, E, S, false, uint128>::DIGITS));

template<uint32 M, uint32 E, bool S> struct soft_decimal_storage<M, E, S, true, uint128> : soft_decimal_storage<M, E, S, false, uint128> {
	typedef soft_decimal_storage<M, E, S, false, uint128>	B;

	void	set(bool sign, int exp, uint128 mant)	{ B::set(sign, exp, bin_to_dpd(mant)); }
	void	set64(bool sign, int exp, uint64 mant)	{ B::set(sign, exp + B::DIGITS - B::DIGITS64, uint128(bin_to_dpd(mant))); }
	uint128	get_mant()	const						{ return dpd_to_bin(B::get_mant()); }
};

template<uint32 M, uint32 E, bool S, bool DPD> class soft_decimal : soft_decimal_storage<M, E, S, DPD>, public comparisons<soft_decimal<M, E, S, DPD>> {
	template<uint32 M2, uint32 E2, bool S2, bool DPD2> friend class soft_decimal;

	typedef soft_decimal_storage<M, E, S, DPD>	B;
	using typename B::mant_t;
	using B::get_exp;
	using B::get_mant;
	typedef soft_decimal_helpers<mant_t>		helpers;

	soft_decimal(bool sign, int exp, mant_t mant) {
		while (mant >= B::max_store) {
			mant /= 10;
			++exp;
		}
		B::set(sign, exp, mant);
	}

public:
	static soft_decimal make(mant_t mant, int exp) { return soft_decimal(false, exp + B::DIGITS, mant); }

	soft_decimal()						{}
	soft_decimal(const _zero&)			{ clear(*this); }
	soft_decimal(const _one&)			{ B::set(false, B::DIGITS, 1); }
	explicit soft_decimal(int i)		{ B::set(i < 0, B::DIGITS, i < 0 ? -i : i); }
	explicit soft_decimal(int64 i)		{ B::set(i < 0, B::DIGITS, i < 0 ? -i : i); }
	explicit soft_decimal(float f)		{ int exp = ceil_pow2((iorf(f).get_exp() + 1) * 5050445, 24); B::set64(f < 0, exp, uint64(abs(f) * pow(10.f, B::DIGITS64 - exp))); }
	explicit soft_decimal(double f)		{ int exp = ceil_pow2((iord(f).get_exp() + 1) * 5050445, 24); B::set64(f < 0, exp, uint64(abs(f) * pow(10.0, B::DIGITS64 - exp))); }
	template<uint32 M2, uint32 E2, bool S2, bool DPD2> soft_decimal(const soft_decimal<M2, E2, S2, DPD2> &f) {
		B::set(f.get_sign(), f.get_exp() + B::DIGITS - f.DIGITS, f.get_mant());
	}
	template<uint32 M2, uint32 E2, bool S2> soft_decimal(const soft_float<M2, E2, S2> &f) {
		B::set(f.get_sign(), B::DIGITS, shift_bits_round<mant_t>(f.get_mant(), int(B::MC) - int(M2)));
		soft_decimal	e = pow(soft_decimal(2), f.get_exp() - int(B::MC));
		*this = *this * e;
	}
	soft_decimal&	mul_pow10(int i) {
		set_exp(get_exp() + i);
		return *this;
	}

	operator double()	const {
		auto	m	= get_mant();
		int		s	= highest_set_index(m);
		int		d	= 0;
		if (s >= 64) {
			d	= (s - 64) * 3 / 10 + 2;
			helpers::div_pow10(m, d);
		}
		return uint64(m) * pow(10.0, get_exp() + d - B::DIGITS);
	}
	operator float()	const {
		auto	m	= get_mant();
		int		s	= highest_set_index(m);
		int		d	= 0;
		if (s >= 64) {
			d	= (s - 64) * 3 / 10 + 2;
			helpers::div_pow10(m, d);
		}
		return uint64(m) * pow(10.0f, get_exp() + d - B::DIGITS);
	}

	soft_decimal operator+(const soft_decimal &b) const {
		int		e	= get_exp();
		auto	m	= get_mant();
		bool	s	= helpers::add(m, e, B::get_sign(), b.get_mant(), b.get_exp(), b.get_sign());
		return soft_decimal(s, e, m);
	}
	soft_decimal operator-(const soft_decimal &b) const {
		int		e	= get_exp();
		auto	m	= get_mant();
		bool	s	= helpers::add(m, e, B::get_sign(), b.get_mant(), b.get_exp(), !b.get_sign());
		return soft_decimal(s, e, m);
	}
	soft_decimal operator*(const soft_decimal &b) const {
		auto	m	= get_mant();
		int		d	= helpers::mul(m, b.get_mant(), B::DIGITS);
		return soft_decimal(B::get_sign() ^ b.get_sign(), get_exp() + b.get_exp() + d - B::DIGITS, m);
	}
	soft_decimal operator/(const soft_decimal &b) const {
		auto	m	= get_mant();
		int		d	= helpers::div(m, b.get_mant(), B::DIGITS);
		return soft_decimal(B::get_sign() ^ b.get_sign(), get_exp() - b.get_exp() + d, m);
	}

	friend soft_decimal	operator-(soft_decimal a)		{ a.s ^= 1; return a; }
	friend soft_decimal abs(soft_decimal a)				{ a.s  = 0; return a; }

	friend soft_decimal reciprocal(const soft_decimal &a) {
		mant_t	m	= a.get_mant(), lo;
		int		s	= leading_zeros(m);
		m	= reciprocalc(m << s);

		uint32	shift	= s + int(is_pow2(m)) + M + 2 - B::MB;
		uint32	d		= shift * 3 / 10;
		m	= mulc(m, B::max_store << (B::MB - M - 1), lo);
		m	= mulc(m, pow(mant_t(10), uint32(B::DIGITS - d)) << shift, lo);
		m	+= lo >> (B:: MB - 1);
		return soft_decimal(a.get_sign(), d - a.get_exp(), m);
	}

	soft_decimal& 	operator+=(const soft_decimal &b)	{ return *this = *this + b;	}
	soft_decimal& 	operator-=(const soft_decimal &b)	{ return *this = *this - b;	}
	soft_decimal& 	operator*=(const soft_decimal &b)	{ return *this = *this * b;	}
	soft_decimal& 	operator/=(const soft_decimal &b)	{ return *this = *this / b;	}
	soft_decimal& 	operator%=(const soft_decimal &b)	{ return *this = *this % b;	}
	soft_decimal& 	operator++()						{ return *this += one; }
	soft_decimal& 	operator--()						{ return *this -= one; }
	soft_decimal 	operator++(int)						{ auto t = *this; operator++(); return t; }
	soft_decimal 	operator--(int)						{ auto t = *this; operator--(); return t; }

	friend int compare(const soft_decimal &a, const soft_decimal &b) {
		int	r = a.s != b.s ? 1 : helpers::compare_unsigned(a.get_mant(), a.get_exp(), b.get_mant(), b.get_exp());
		return a.s ? -r : r;
	}
	friend soft_decimal trunc(const soft_decimal &a) {
		auto	m	= a.get_mant();
		helpers::div_pow10(m, B::DIGITS - a.get_exp());
		return soft_decimal(a.s, B::DIGITS, m);
	}
	friend soft_decimal floor(const soft_decimal &a) {
		auto	m	= a.get_mant();
		int		g	= helpers::div_pow10(m, B::DIGITS - a.get_exp());
		if (a.s && g)
			++m;
		return soft_decimal(a.s, B::DIGITS, m);
	}
	friend soft_decimal round(const soft_decimal &a, int decimals = 0) {
		int	e = B::DIGITS - decimals;
		if (e < a.get_exp())
			return a;
		auto	m	= a.get_mant();
		int		g	= helpers::div_pow10(m, e - a.get_exp());
		if (g + (decimals & 1) >= 3)
			++m;
		return soft_decimal(a.s, e, m);
	}
	friend float_category	get_category(const soft_decimal &a)	{
		return a.c == 31 ? FLOAT_QNAN : a.c == 30 ? (a.s ? FLOAT_NEGINF : FLOAT_INF) : FLOAT_NORMAL;
	}
	friend size_t to_string(char *s, const soft_decimal &a) {
		if (auto cat = get_category(a))
			return put_special(s, cat);
		return put_decimal(s, a.get_mant(), B::DIGITS - a.get_exp(), a.get_sign());
	}
};

typedef soft_decimal<23, 8, true, false>	decimal32;
typedef soft_decimal<53, 10,true, false>	decimal64;
typedef soft_decimal<113,14,true, false>	decimal128;

typedef soft_decimal<23, 8, true, true>		decimal32_dpd;
typedef soft_decimal<53, 10,true, true>		decimal64_dpd;
typedef soft_decimal<113,14,true, true>		decimal128_dpd;

template<uint32 M, uint32 E, bool S, bool DPD, typename T> soft_decimal<M, E, S, DPD> make_float(T_type<soft_decimal<M, E, S, DPD>>, T mant, int exp) {
	return soft_decimal<M, E, S, DPD>::make(mant, exp);
}

template<char... C> decimal32	operator"" _d32()	{ return float_reader<decimal32, 0, 0, C...>::get(); }
template<char... C> decimal64	operator"" _d64()	{ return float_reader<decimal64, 0, 0, C...>::get(); }
template<char... C> decimal128	operator"" _d128()	{ return float_reader128<decimal128, 0, 0, 0, C...>::get(); }

template<uint32 M, uint32 E, bool S, bool DPD>	constexpr uint32 BIT_COUNT<soft_decimal<M, E, S, DPD>>	= E + M + int(S);

//-----------------------------------------------------------------------------
//	scaled / fixed-point
//-----------------------------------------------------------------------------

template<typename I, int64 S> struct scaled {
	I	i;

	scaled()									{}
	template<typename I2, int64 S2> constexpr scaled(const scaled<I2,S2> &f)	{ i = f.i * S2 / S; }
	constexpr scaled(float f) : i(I(f * S + copysign(.5f, f))) {}
	operator	float()				const		{ return (float)i / S; }
	I			to_int()			const		{ return i / S; }
	scaled		operator-()			const		{ scaled t; t.i = -i;	return t; }
	scaled&		operator+=(scaled f)			{ i += f.i;				return *this; }
	scaled&		operator-=(scaled f)			{ i -= f.i;				return *this; }
	scaled&		operator*=(scaled f)			{ i = (i * f.i) / S;	return *this; }
	scaled&		operator/=(scaled f)			{ i = i * S / f.i;		return *this; }
	scaled&		operator++()					{ i += S;				return *this; }
	scaled&		operator--()					{ i -= S;				return *this; }
	scaled		operator++(int)					{ scaled t = *this; ++*this; return t; }
	scaled		operator--(int)					{ scaled t = *this; --*this; return t; }

	friend scaled abs(scaled f)	{ f.i = abs(f.i); return f; }
};

template<typename I, int64 S> struct num_traits<scaled<I,S> > : num_traits<I> {
	typedef scaled<I,S>	T;
	static const bool	has_frac	= true;
	static constexpr T	max()		{ return force_cast<T>(num_traits<I>::max()); }
	static constexpr T	min()		{ return force_cast<T>(num_traits<I>::min()); }
	template<typename T2> static constexpr T cast(T2 t)	{ return T(t); }
};

// scaledA	(A = Adjusted) negative values scaled by S+1

template<typename I, int64 S> struct scaledA {
	I	i;
	scaledA()									{}
	constexpr scaledA(float f)					{ i = I(f * (S + int(f < 0))); }
	operator	float()				const		{ return (float)i / (S + int(i < 0)); }
};

template<typename I, int64 S> struct num_traits<scaledA<I,S> > : num_traits<I> {
	typedef scaledA<I,S>	T;
	static const bool	has_frac	= true;
	static constexpr T	max()	{ return force_cast<T>(num_traits<I>::max()); }
	static constexpr T	min()	{ return force_cast<T>(num_traits<I>::min()); }
	template<typename T2> static constexpr T cast(T2 t)	{ return T(t); }
};

// fixed

template<int whole, int frac> struct fixed : scaled<sint_bits_t<whole + frac>, (1u<<frac)> {
	typedef scaled<sint_bits_t<whole + frac>, (1u<<frac)> B;
//	using B::scaled;
	fixed()			{}
//	constexpr fixed(B f)		: B(f)	{}
	constexpr fixed(float f)	: B(f)	{}
	template<typename I2, int64 S2> fixed(const scaled<I2,S2> &f) : B(f) {}
};

template<int whole, int frac> struct num_traits<fixed<whole,frac> > : num_traits<typename fixed<whole,frac>::B> {
	typedef fixed<whole,frac> T;
	enum { bits = whole + frac };
	static constexpr T	max()	{ return force_cast<T>((T::I(1) << (bits - 1)) - 1); }
	static constexpr T	min()	{ return force_cast<T>(T::I(1) << (bits - 1)); }
	template<typename T2> static constexpr T cast(T2 t)	{ return T(t); }
};

// ufixed

template<int whole, int frac> struct ufixed : scaled<uint_bits_t<whole + frac>, (1u<<frac)> {
	typedef scaled<uint_bits_t<whole + frac>, (1u<<frac)> B;
//	using B::scaled;
	ufixed()		{}
//	constexpr ufixed(B f)		: B(f)	{}
	constexpr ufixed(float f)	: B(f)	{}
	template<typename I2, int64 S2> ufixed(const scaled<I2,S2> &f) : B(f) {}
};
template<int whole, int frac> struct num_traits<ufixed<whole,frac> > : num_traits<typename ufixed<whole,frac>::B> {
	typedef ufixed<whole,frac> T;
	enum {bits = whole + frac};
	static constexpr T	max()	{ return force_cast<T>((T::I(1) << bits) - 1); }
	static constexpr T	min()	{ return 0; }
	template<typename T2> static constexpr T cast(T2 t)	{ return T(t); }
};

typedef scaled<int8,	0x7f>		norm8;
typedef scaled<int16,	0x7fff>		norm16;
typedef scaled<uint8,	0xff>		unorm8;
typedef scaled<uint16,	0xffff>		unorm16;
typedef scaled<int32,	0x7fffffff>	norm32;
typedef scaled<uint32,	0xffffffff>	unorm32;
typedef scaled<int64,	0x7fffffffffffffffll>	norm64;
typedef scaled<uint64,	0xffffffffffffffffll>	unorm64;
typedef BE(norm16)					norm16be;
typedef BE(unorm16)					unorm16be;
typedef BE(norm32)					norm32be;
typedef BE(unorm32)					unorm32be;

}// namespace iso
#endif // SOFT_FLOAT_H
