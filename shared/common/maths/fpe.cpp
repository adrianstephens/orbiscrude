#ifndef FPE_H
#define FPE_H

#include "base/defs.h"
#include "base/strings.h"

//-----------------------------------------------------------------------------
//	floats with error analysis
//-----------------------------------------------------------------------------

namespace iso {

template<typename T> static constexpr T eps = T(0.5) / (1 << num_traits<T>::mantissa_bits);

//-------------------------------------
//	epoly - 4 polynomial coefficients
//-------------------------------------

template<int K0 = 0, int K1 = 0, int K2 = 0, int K3 = 0> struct epoly {
	template<typename T> static constexpr T value = K0 + eps<T> * (K1 + eps<T> * (K2 + K3 * eps<T>));
};

template<int I0, int I1, int I2, int I3, int J0, int J1, int J2, int J3> constexpr bool operator>(epoly<I0, I1, I2, I3>, epoly<J0, J1, J2, J3>) {
	return I0 > J0 || (I0 == J0 && (I1 > J1 || I1 == J1 && (I2 > J2 || (I2 == J2 && I3 > J3))));
}

template<typename I, typename J> if_t<(I() > J()), I, J> constexpr max_epoly(I, J) { return {}; }

template<int I0, int I1, int I2, int I3, int J0, int J1, int J2, int J3> constexpr epoly<
	I0 + J0,
	I1 + J1,
	I2 + J2,
	I3 + J3
>	operator+(epoly<I0,I1,I2,I3>, epoly<J0,J1,J2,J3>) { return {}; }

template<int I0, int I1, int I2, int I3, int J0, int J1, int J2, int J3> constexpr epoly<
	I0 * J0,
	I0 * J1 + I1 * J0,
	I0 * J2 + I1 * J1 + I2 * J0,
	I0 * J3 + I1 * J2 + I2 * J1 + I3 * J0 + int((I1 && J3) || (I2 && (J2 || J3)) || (I3 && (J1 || J2 || J3)))
>	operator*(epoly<I0,I1,I2,I3>, epoly<J0,J1,J2,J3>) { return {}; }

//-------------------------------------
//	fpe - float with accumulated e and P error terms
//-------------------------------------

template<typename T, typename P = epoly<>> struct fpe {
	T	x, e;
	constexpr fpe(T x)			: x(x), e(abs(x)) {}
	constexpr fpe(T x, T e)	: x(x), e(e) {}
	constexpr T	error() const	{ return P::value<T> * e; }
};

//precise
template<typename T> struct fpe<T, epoly<>> {
	T	x;
	constexpr fpe(T x)			: x(x) {}
	constexpr T	error() const	{ return 0; }
};

//void
template<typename P> struct fpe<void, P> {
	template<typename T> constexpr T	error() const	{ return P::value<T>; }
};

template<> struct fpe<void, epoly<>> {};

//-------------------------------------
//	abs
//-------------------------------------

//precise
template<typename T> fpe<T> abs(const fpe<T> &i) {
	return abs(i.x);
}

//not precise
template<typename T, typename Pi> fpe<T, Pi> abs(const fpe<T, Pi> &i) {
	return {abs(i.x), i.e};
}

//void
template<typename Pi> fpe<void, Pi> abs(const fpe<void, Pi>) { return {}; }
fpe<void> abs(const fpe<void>) { return {}; }

//-------------------------------------
//	neg
//-------------------------------------

//precise
template<typename T> fpe<T> operator-(const fpe<T> &i) {
	return -i.x;
}

//not precise
template<typename T, typename Pi> fpe<T, Pi> operator-(const fpe<T, Pi> &i) {
	return {-i.x, i.e};
}

//void
template<typename Pi> fpe<void, Pi> operator-(const fpe<void, Pi>) { return {}; }
fpe<void> operator-(const fpe<void>) { return {}; }

//-------------------------------------
//	add
//-------------------------------------

//both precise
template<typename T> fpe<T, epoly<0, 1>> operator+(const fpe<T> &i, const fpe<T> &j) {
	return i.x + j.x;
}

//one precise
template<typename T, typename Pi> fpe<T, decltype(Pi() * epoly<1, 1>())> operator+(const fpe<T, Pi> &i, const fpe<T> &j) {
	return {i.x + j.x, abs(i.x + j.x) + i.e};
}
template<typename T, typename Pj> auto operator+(const fpe<T> &i, const fpe<T,Pj> &j) {
	return j + i;
}

//no precise
template<typename T, typename Pi, typename Pj> fpe<T, decltype((epoly<0, 1>() + max_epoly(Pi(), Pj())) * epoly<1, 1>())> operator+(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	return {i.x + j.x, i.e + j.e};
}

//void
constexpr fpe<void, epoly<0, 1>> operator+(fpe<void>, fpe<void>) { return {}; }
template<typename Pi> fpe<void, decltype(Pi() * epoly<1, 1>())> operator+(fpe<void, Pi>, fpe<void>) { return {}; }
template<typename Pi, typename Pj> fpe<void, decltype((epoly<0, 1>() + max_epoly(Pi(), Pj())) * epoly<1, 1>())> operator+(fpe<void,Pi>, fpe<void,Pj>) { return {}; }

//-------------------------------------
//	subtract
//-------------------------------------

template<typename T, typename Pi, typename Pj> auto operator-(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	return i + -j;
}

//-------------------------------------
//	multiply
//-------------------------------------

//both precise
template<typename T> fpe<epoly<0, 1>> operator*(const fpe<T> &i, const fpe<T> &j) {
	return i.x * j.x;
}

//one precise
template<typename T, typename Pi> fpe<T, decltype(epoly<0, 1>() + epoly<1, 1>() * Pi())> operator*(const fpe<T,Pi> &i, const fpe<T> &j) {
	return {i.x * j.x, abs(i.x * j.x) * i.e};
}
template<typename T, typename Pj> auto operator*(const fpe<T> &i, const fpe<T,Pj> &j) {
	return j * i;
}

//no precise
template<typename T, typename Pi, typename Pj> fpe<T, decltype(epoly<0, 1>() + epoly<1, 1>() * (Pi() + Pj() + Pi() * Pj()))> operator*(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	return {i.x * j.x, i.e * j.e};
}

//void
constexpr fpe<void, epoly<0, 1>> operator*(fpe<void>, fpe<void>) { return {}; }
template<typename Pi> fpe<void, decltype(epoly<0, 1>() + epoly<1, 1>() * Pi())> operator*(fpe<void, Pi>, fpe<void>) { return {}; }
template<typename Pi, typename Pj> fpe<void, decltype(epoly<0, 1>() + epoly<1, 1>() * (Pi() + Pj() + Pi() * Pj()))> operator*(fpe<void,Pi>, fpe<void,Pj>) { return {}; }

//-------------------------------------
//	compare with zero
//-------------------------------------

enum CZ {
	CZ_IND = 0,
	CZ_NEG = -1,
	CZ_POS = 1,
};

constexpr CZ operator*(CZ i, CZ j) {
	return CZ(int(i) * int(j));
}

template<typename T> constexpr CZ cz(T x) {
	return x < 0 ? CZ_NEG : CZ_POS;
}

template<typename T> constexpr CZ cz(T x, T D) {
	return abs(x) <= D ? CZ_IND : cz(x);
}

template<typename T> constexpr CZ cz(const fpe<T> &i) {
	return cz(i.x);
}

template<typename T, typename Pi> constexpr CZ cz(const fpe<T,Pi> &i) {
	return cz(i.x, i.error());
}

//-------------------------------------
//	mul_cz
//-------------------------------------

template<typename T, typename Pi, typename Pj> auto mul_cz(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	return cz(i) * cz(j);
}

//-------------------------------------
//	add_cz
//-------------------------------------

template<typename T, typename Pi, typename Pj> auto add_cz(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	constexpr auto	Pi1 = epoly<1,2,1>() * epoly<1,1,1,1>() * Pi();
	constexpr auto	Pj1 = epoly<1,2,1>() * epoly<1,1,1,1>() * Pj();
	return cz(i.x + j.x, Pi1.value<T> * i.e + Pj1.value<T> * j.e);
}

template<typename T, typename P> auto add_cz(const fpe<T,P> &i, const fpe<T,P> &j) {
	constexpr auto	P1 = epoly<1,2,1>() * epoly<1,1,1,1>() * P();
	return cz(i.x + j.x,  P1.value<T> * (i.e + j.e));
}


//-------------------------------------
//	compares
//-------------------------------------

template<typename T, typename Pi, typename Pj> CZ compare(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	return add_cz(i, -j);
}

template<typename T, typename Pi, typename Pj> fpe<T, decltype(max_epoly(
	epoly<1,2,1>() * epoly<1,1,1,1>() * Pi(),
	epoly<1,2,1>() * epoly<1,1,1,1>() * Pj()
))> operator<(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	return {i.x - j.x, i.e + j.e};
}

template<typename Pi, typename Pj> fpe<void, decltype(max_epoly(
	epoly<1,2,1>() * epoly<1,1,1,1>() * Pi(),
	epoly<1,2,1>() * epoly<1,1,1,1>() * Pj()
))> operator<(fpe<void,Pi>, fpe<void,Pj>) {
	return {};
}

template<typename T, typename Pi, typename Pj> auto operator>(const fpe<T,Pi> &i, const fpe<T,Pj> &j) {
	return j < i;
}

//-----------------------------------------------------------------------------
//	multi-precision floats
//-----------------------------------------------------------------------------

template<typename T, int N> struct fp {
	union {
		T	t[N];
		struct {
			fp<T, N / 2>		lo;
			fp<T, N - N / 2>	hi;
		};
	};
	constexpr fp(const fp&) = default;
	template<typename... V> constexpr fp(V...v) : t{v...} {}
	constexpr operator const T*()	const	{ return t; }
	constexpr T operator[](int i)	const	{ return t[i]; }
	T			top()				const	{ return t[N - 1]; }

	template<int N2> auto&	slice(int O)	const { return *(const fp<T, N2>*)(t + O); }
	template<int N2> auto&	low()			const { return slice<N2>(0); }
	template<int N2> auto&	high()			const { return slice<N2>(N - N2); }

	template<int N2> auto&	slice(int O)	{ return *(fp<T, N2>*)(t + O); }
	template<int N2> auto&	low()			{ return slice<N2>(0); }
	template<int N2> auto&	high()			{ return slice<N2>(N - N2); }
};

template<typename T> struct fp<T,1> {
	T	t;
	fp() {}
	constexpr fp(T t) : t(t) {}
	constexpr operator const T*()	const	{ return &t; }
	operator T()					const	{ return t; }
};

template<typename T> constexpr fp<T,2> split(T a) {
	static constexpr T splitter		= (1 << ((num_traits<T>::mantissa_bits + 2) / 2)) + 1;
	T	c		= T(splitter * a);
	T	abig	= T(c - a);
	T	ahi		= c - abig;
	return {a - ahi, ahi};
}

template<typename T, int... N> auto join(const fp<T, N>&...x) {
	fp<T, meta::reduce_add<N...>>	r;
	int	i = 0;
	int	dummy[] = {((r.slice<N>(i) = x), i += N)...};
	return r;
}

//-----------------------------------------------------------------------------
//	multi-precision float helpers
//-----------------------------------------------------------------------------

template<typename T> constexpr fp<T,2> extend_add_fast(T a, T b, T x) {
	T	bvirt = x - a;
	return {b - bvirt, x};
}
template<typename T> constexpr fp<T,2> add_fast(T a, T b) {
	return extend_add_fast(a, b, a + b);
}

template<typename T> constexpr fp<T,2> extend_sub_fast(T a, T b, T x) {
	T	bvirt = a - x;
	return {bvirt - b, x};
}
template<typename T> constexpr fp<T,2> sub_fast(T a, T b) {
	return extend_sub_fast(a, b, a - b);
}

template<typename T> constexpr fp<T,2> extend_add(T a, T b, T x) {
	T	bvirt  = x - a;
	T	avirt  = x - bvirt;
	T	bround = b - bvirt;
	T	around = a - avirt;
	return {around + bround, x};
}

template<typename T> constexpr fp<T,2> extend_sub(T a, T b, T x) {
	T	bvirt  = a - x;
	T	avirt  = x + bvirt;
	T	bround = bvirt - b;
	T	around = a - avirt;
	return {around + bround, x};
}

template<typename T> constexpr fp<T,2> extend_mul(fp<T,2> as, fp<T,2> bs, T x) {
	T	err1 = x - T(as[1] * bs[1]);
	T	err2 = err1 - T(as[0] * bs[1]);
	T	err3 = err2 - T(as[1] * bs[0]);
	return {T(as[0] * bs[0]) - err3, x};
}

template<typename T> constexpr fp<T,2> extend_mul(T a, T b, T x) {
	return extend_mul(split(a), split(b), x);
}

template<typename T> constexpr fp<T,2> extend_square(fp<T,2> as, T x) {
	T		err1 = x - square(as[1]);
	T		err3 = err1 - T((as[1] + as[1]) * as[0]);
	return {square(as[0]) - err3, x};
}

//-----------------------------------------------------------------------------
//	fixed multi-precision float operators
//-----------------------------------------------------------------------------

// specialised 1-element

template<typename T> constexpr fp<T,1>	operator-(fp<T,1> a)			{ return -a.t; }
template<typename T> constexpr auto		operator+(fp<T,1> a, fp<T,1> b)	{ return extend_add(a.t, b.t, a.t + b.t); }
template<typename T> constexpr auto		operator-(fp<T,1> a, fp<T,1> b)	{ return extend_sub(a.t, b.t, a.t - b.t); }
template<typename T> constexpr auto		operator*(fp<T,1> a, fp<T,1> b)	{ return extend_mul(split(a.t), split(b.t), a.t * b.t); }
template<typename T> constexpr auto		square(fp<T,1> a)				{ return extend_square(split(a.t), square(a.t)); }

template<typename T> constexpr fp<T,1>	operator*(fp<T,1> a, decltype(two))	{ return a.t * two; }

// specialised sums

template<int NA, int NB> constexpr bool use_unrolled		= NA < 8 && NB < 8;
template<typename T, int NA, int NB> using unrolled_t		= enable_if_t<use_unrolled<NA, NB>, fp<T,NA+NB>>;
template<typename T, int NA, int NB> using not_unrolled_t	= enable_if_t<!use_unrolled<NA, NB>, fp<T,NA+NB>>;

template<typename T, int N> constexpr unrolled_t<T, N, 1> operator+(const fp<T,N> &a, fp<T,1> b) {
	auto	i = a.lo + b;
	return join(i.low<N / 2>(), a.hi + i.top());
}

template<typename T, int NA, int NB> constexpr unrolled_t<T, NA, NB> operator+(const fp<T,NA> &a, const fp<T,NB> &b) {
	auto	i = a + b.lo;
	auto	j = i.high<NA>() + b.hi;
	return join(i.low<NB / 2>(), j);
}

template<typename T, int N> constexpr unrolled_t<T, N, 1> operator-(const fp<T,N> &a, fp<T,1> b) {
	auto	i = a.lo - b;
	return join(i.low<N / 2>(), a.hi + i.top());
}

template<typename T, int NA, int NB> constexpr unrolled_t<T, NA, NB> operator-(const fp<T,NA> &a, const fp<T,NB> &b) {
	auto	i = a - b.lo;
	auto	j = i.high<NA>() - b.hi;
	return join(i.low<NB / 2>(), j);
}

// specialised multiplies

template<typename T> constexpr fp<T,2> mul(T a, T b, fp<T,2> bs) {
	return extend_mul(split(a), bs, (T)(a * b));
}
template<typename T> constexpr fp<T,2> mul(T a, fp<T,2> as, T b, fp<T,2> bs) {
	return extend_mul(as, bs, (T)(a * b));
}

template<typename T> constexpr fp<T,3> accumulate(T a, fp<T,1> b, fp<T,2> bs, fp<T,1> i) {
	auto	j	= mul(a, b.t, bs);
	auto	k	= i + j.lo;
	return join(k.lo, add_fast(j[1], k[1]));
}

template<typename T> constexpr fp<T,4> operator*(fp<T,2> a, fp<T,1> b) {
	auto	bs	= split(b.t);
	auto	r0	= mul(a[0], b.t, bs);
	auto	r1	= accumulate(a[1], b, bs, r0.hi);
	return join(r0.lo, r1);
}

template<typename T> constexpr fp<T,8> operator*(fp<T,4> a, fp<T,1> b) {
	auto	bs	= split(b.t);
	auto	r0	= mul(a[0], b.t, bs);
	auto	r1	= accumulate(a[1], b, bs, r0.hi);
	auto	r2	= accumulate(a[2], b, bs, r1.high<1>());
	auto	r3	= accumulate(a[3], b, bs, r2.high<1>());
	return join(r0.lo, r1.low<2>(), r2.low<2>(), r3);
}

//-----------------------------------------------------------------------------
//	multi-precision float helpers
//-----------------------------------------------------------------------------
// eliminate zeros
template<typename T> int zeroelim(int N, const T *a, T *h) {
	int	j = 0;
	for (int i = 0; i < N; ++i) {
		if (a[i])
			h[j++] = a[i];
	}
	return j;
}

// produce a one-word estimate of an expansion's value
template<typename T> T estimate(int N, const T *a) {
	T   Q = 0;
	for (int i = 0; i < N; i++)
		Q += a[i];
	return Q;
}

// Negate an expansion
template<typename T> int negate_expansion(int N, const T *a, T *h) {
	for (int i = 0; i < N; i++)
		h[i] = -a[i];
	return N;
}

// Add a scalar to an expansion
// maintains the nonoverlapping property; a and h can be the same
template<typename T> int grow_expansion(int N, const T *a, T b, T *h) {
	fp<T,1>		Q = b;
	for (int i = 0; i < N; ++i) {
		auto	S = Q + a[i];
		Q		= S.hi;
		h[i]	= S.lo;
	}
	h[N] = Q;
	return N + 1;
}

// Add a scalar to an expansion, eliminating zero components
// maintains the nonoverlapping property; a and h can be the same
template<typename T> int grow_expansion_zeroelim(int N, const T *a, T b, T *h) {
	T	*h0		= h;
	fp<T,1>	Q	= b;
	for (int i = 0; i < N; ++i) {
		auto	S = Q + a[i];
		Q		= S.hi;
		if (S.lo)
			*h++ = S.lo;
	}
	if (Q)
		*h++ = Q;
	return h - h0;
}

// Sum two expansions
// maintains the nonoverlapping property; a and h can be the same, but b and h cannot
template<typename T> int expansion_sum(int NA, const T *a, int NB, T *b, T *h) {
	for (int i = 0; i < NB; ++i) {
		NA	= grow_expansion(NA, a, b[i], h);
		a	= h;
	}
	return NA;
}

// Sum two expansions, eliminating zero components
// maintains the nonoverlapping property; a and h can be the same, but b and h cannot
template<typename T> int expansion_sum_zeroelim(int NA, const T *a, int NB, const T *b, T *h) {
	for (int i = 0; i < NB; ++i) {
		NA	= grow_expansion_zeroelim(NA, a, b[i], h);
		a	= h;
	}
	return NA;
}


// Sum two expansions
// if round-to-even is used, maintains the strongly nonoverlapping property; does NOT maintain the nonoverlapping or nonadjacent properties; h cannot be a or b
template<typename T> int fast_expansion_sum(int NA, const T *a, int NB, const T *b, T *h) {
	T	*h0		= h;
	int	ai		= 0,	bi		= 0;
	T	anow	= a[0],	bnow	= b[0];

	fp<T,1>	Q;
	if ((bnow > anow) == (bnow > -anow)) {
		Q		= anow;
		anow	= a[++ai];
	} else {
		Q		= bnow;
		bnow	= b[++bi];
	}

	if (ai < NA && bi < NB) {
		fp<T,2>	S;
		if ((bnow > anow) == (bnow > -anow)) {
			S		= add_fast<T>(anow, Q);
			anow	= a[++ai];
		} else {
			S		= add_fast<T>(bnow, Q);
			bnow	= b[++bi];
		}
		Q = S.hi;
		*h++ = S.lo;

		while (ai < NA && bi < NB) {
			if ((bnow > anow) == (bnow > -anow)) {
				S		= Q + anow;
				anow	= a[++ai];
			} else {
				S		= Q + bnow;
				bnow	= b[++bi];
			}
			Q = S.hi;
			*h++ = S.lo;
		}
	}
	while (ai < NA) {
		auto S	= Q + anow;
		Q		= S.hi;
		*h++	= S.lo;
		anow	= a[++ai];
	}
	while (bi < NB) {
		auto S	= Q + bnow;
		Q		= S.hi;
		*h++	= S.lo;
		bnow	= b[++bi];
	}
	*h++ = Q;
	return h - h0;
}

// Sum two expansions, eliminating zero components
// if round-to-even is used, maintains the strongly nonoverlapping property; does NOT maintain the nonoverlapping or nonadjacent properties; h cannot be a or b
template<typename T> int fast_expansion_sum_zeroelim(int NA, const T *a, int NB, const T *b, T *h) {
	T	*h0		= h;
	int	ai		= 0,	bi		= 0;
	T	anow	= a[0],	bnow	= b[0];

	fp<T,1>	Q;
	if (ai < NA && (bi >= NB || (bnow > anow) == (bnow > -anow))) {
		Q		= anow;
		anow	= a[++ai];
	} else if (bi < NB) {
		Q		= bnow;
		bnow	= b[++bi];
	} else {
		return 0;
	}

	if (ai < NA && bi < NB) {
		fp<T,2>	S;
		if ((bnow > anow) == (bnow > -anow)) {
			S		= add_fast<T>(anow, Q);
			anow	= a[++ai];
		} else {
			S		= add_fast<T>(bnow, Q);
			bnow	= b[++bi];
		}
		Q = S.hi;
		if (S.lo)
			*h++ = S.lo;

		while ((ai < NA) && (bi < NB)) {
			if ((bnow > anow) == (bnow > -anow)) {
				S		= Q + anow;
				anow	= a[++ai];
			} else {
				S		= Q + bnow;
				bnow	= b[++bi];
			}
			Q = S.hi;
			if (S.lo)
				*h++ = S.lo;
		}
	}
	while (ai < NA) {
		auto S	= Q + anow;
		Q		= S.hi;
		if (S.lo)
			*h++	= S.lo;
		anow	= a[++ai];
	}
	while (bi < NB) {
		auto S	= Q + bnow;
		Q		= S.hi;
		if (S.lo)
			*h++	= S.lo;
		bnow	= b[++bi];
	}
	if (Q)
		*h++ = Q;
	return h - h0;
}

// Sum two expansions (as fast_expansion_sum, but also works for non-round-to-even)
// maintains the nonoverlapping property; h cannot be a or b
template<typename T> int linear_expansion_sum(int NA, const T *a, int NB, const T *b, T *h)  {
	int ai      = 0,    bi      = 0;
	T   anow    = a[0],	bnow    = b[0];

    T   q;
	if ((bnow > anow) == (bnow > -anow)) {
		q		= anow;
		anow	= a[++ai];
	} else {
		q		= bnow;
		bnow	= b[++bi];
	}

	fp<T,2>	S;
	if (ai < NA && (bi >= NB || (bnow > anow) == (bnow > -anow))) {
		S		= add_fast(anow, q);
		anow	= a[++ai];
	} else {
		S		= add_fast(bnow, q);
		bnow	= b[++bi];
	}

	for (int i = 0; i < NA + NB - 2; ++i) {
		auto	Q	= S.hi;
		q			= S.lo;
		if (ai < NA && (bi >= NB || (bnow > anow) == (bnow > -anow))) {
			S       = add_fast(anow, q);
			anow    = a[++ai];
		} else {
			S       = add_fast(bnow, q);
			bnow    = b[++bi];
		}
        *h++ = S.lo;
		S = Q + S.hi;
	}
	h[0] = S.lo;
	h[1] = S.hi;
	return NA + NB;
}

// Sum two expansions, eliminating zero components (as fast_expansion_sum_zeroelim, but also works for non-round-to-even)
// maintains the nonoverlapping property; h cannot be a or b
template<typename T> int linear_expansion_sum_zeroelim(int NA, const T *a, int NB, const T *b, T *h)  {
	int ai      = 0,    bi      = 0;
	T   anow    = a[0],	bnow    = b[0];

    T   q;
    if (a1 < NA && (bi >= NB || (bnow > anow) == (bnow > -anow))) {
        q		= anow;
        anow	= a[++ai];
    } else if (bi < NB) {
        q		= bnow;
        bnow	= b[++bi];
	} else {
		return 0;
	}
	
	fp<T,2>	S;
	if (ai < NA && (bi >= NB || (bnow > anow) == (bnow > -anow))) {
        S		= add_fast(anow, q);
        anow	= a[++ai];
    } else {
        S		= add_fast(bnow, q);
        bnow	= b[++bi];
    }

    T   *h0 = h;
    for (int i = 0; i < NA + NB - 2; ++i) {
		auto	Q	= S.hi;
		q			= S.lo;
		if (ai < NA && (bi >= NB || (bnow > anow) == (bnow > -anow))) {
            S       = add_fast(anow, q);
            anow    = a[++ai];
        } else {
            S       = add_fast(bnow, q);
            bnow    = b[++bi];
        }
        if (S.lo)
            *h++ = S.lo;
        S = Q + S.hi;
    }
    if (S.lo)
        *h++ = S.lo;
    if (S.hi)
        *h++ = S.hi;
    return h - h0;
}

// Multiply an expansion by a scalar
// maintains the nonoverlapping property; a and h cannot be the same
template<typename T> int scale_expansion(int N, const T *a, T b, T *h) {
	if (N == 0)
		return 0;

	auto    bs  = split(b);
	auto    S   = mul(a[0], b, bs);
	auto    Q   = S.hi;
	*h++ = S.lo;

	for (int i = 1; i < N; i++) {
		auto S  = mul(a[i], b, bs);
		auto X  = Q + S.lo;
		*h++    = X.lo;
		auto Y  = S.hi + X.hi;
		*h++    = Y.lo;
		Q       = Y.hi;
	}
	*h = Q;
    return N + N;
}

// Multiply an expansion by a scalar, eliminating zero components
// maintains the nonoverlapping property; a and h cannot be the same
template<typename T> int scale_expansion_zeroelim(int N, const T *a, T b, T *h) {
	if (N == 0)
		return 0;

    T       *h0 = h;
    auto    bs  = split(b);
    auto    S   = mul(a[0], b, bs);
    auto    Q   = S.hi;
    if (S.lo)
        *h++ = S.lo;
    
	for (int i = 1; i < N; i++) {
        auto S  = mul(a[i], b, bs);
        auto X  = Q + S.lo;
        if (X.lo)
            *h++    = X.lo;
        auto Y  = S.hi + X.hi;
        if (Y.lo)
            *h++    = Y.lo;
        Q       = Y.hi;
    }
    if (Q)
        *h++ = Q;
    return h - h0;
}


// Multiply an expansion by a scalar which is +/- a power of 2 (so no accuracy issues)
template<typename T> int scale_expansion_pow2(int N, const T *a, T b, T *h) {
	for (int i = 0; i < N; i++)
		h[i] = a[i] * b;
	return N;
}

template<typename T> int expansion_mul(int NA, const T *a, int NB, const T *b, T *h, T *scratch) {
	if (NB == 1)
		return scale_expansion(NA, a, *b, h);
	int	n0 = expansion_mul(NA, a, NB / 2, b, scratch, h);
	int	n1 = expansion_mul(NA, a, NB - NB / 2, b + NB / 2, scratch + n0, h);
	return fast_expansion_sum(n0, scratch, n1, scratch + n0, h);
}

template<typename T> int expansion_mul_zeroelim(int NA, const T *a, int NB, const T *b, T *h, T *scratch) {
	if (NB == 1)
		return scale_expansion_zeroelim(NA, a, *b, h);
	int	n0 = expansion_mul_zeroelim(NA, a, NB / 2, b, scratch, h);
	int	n1 = expansion_mul_zeroelim(NA, a, NB - NB / 2, b + NB / 2, scratch + n0, h);
	return fast_expansion_sum_zeroelim(n0, scratch, n1, scratch + n0, h);
}

template<typename T> int expansion_square(int N, const T *a, T *h, T *scratch) {
	if (N == 1) {
		*(fp<T,2>*)h = extend_square(split(*a), square(*a));
		return 2;
	}

	int	n0	= expansion_square(N / 2, a, h, scratch);
	int	n2	= expansion_square(N - N / 2, a + N / 2, h + n0, scratch);
	int	n02	= fast_expansion_sum(n0, h, n2, h + n0, scratch);

	int	n1	= scale_expansion_pow2(expansion_mul(N / 2, a, N - N / 2, a + N / 2, scratch + n02, h), scratch + n02, T(2), scratch + n02);
	return fast_expansion_sum(n02, scratch, n1, scratch + n02, h);
}

template<typename T> int expansion_square_zeroelim(int N, const T *a, T *h, T *scratch) {
	if (N == 1) {
		*(fp<T,2>*)h = extend_square(split(*a), square(*a));
		if (h[0])
			return 2;
		h[0] = h[1];
		return 1;
	}

	int	n0	= expansion_square_zeroelim(N / 2, a, h, scratch);
	int	n2	= expansion_square_zeroelim(N - N / 2, a + N / 2, h + n0, scratch);
	int	n02	= fast_expansion_sum_zeroelim(n0, h, n2, h + n0, scratch);

	int	n1	= scale_expansion_pow2(expansion_mul_zeroelim(N / 2, a, N - N / 2, a + N / 2, scratch + n02, h), scratch + n02, T(2), scratch + n02);
	return fast_expansion_sum_zeroelim(n02, scratch, n1, scratch + n02, h);
}


// Compress an expansion
// Maintains the nonoverlapping property; a and h may be the same
template<typename T> int compress(int N, const T *a, T *h) {
	if (N == 0)
		return 0;

	int bottom = N - 1;
    fp<T,1> Q = a[bottom];
    for (int i = N - 2; i >= 0; i--) {
        auto S = add_fast<T>(Q, a[i]);
        if (S.lo) {
            h[bottom--] = S.hi;
            Q = S.lo;
        } else {
            Q = S.hi;
        }
    }
	T	*h0 = h;
    for (int i = bottom + 1; i < N; i++) {
        auto S = add_fast<T>(h[i], Q);
        if (S.lo)
            *h++ = S.lo;
        Q = S.hi;
    }
	if (Q)
		*h++ = Q;
    return h - h0;
}

// Compress if NA > ND, otherwise copy
template<typename T> int maybe_compress(int NA, const T *a, T *h, int ND) {
	if (NA > ND)
		return compress(NA, a, h);

	for (int i = 0; i < NA; i++)
		*h++ = a[i];
	return NA;
}

//-----------------------------------------------------------------------------
//	multi-precision float operators
//-----------------------------------------------------------------------------

template<typename T, int N> T sign(const fp<T,N> &a) {
	return sign(a.top());
}

template<typename T, int N> T estimate(const fp<T,N> &a) {
	return estimate<T>(N, a);
}

template<typename T, int N> inline auto operator-(const fp<T,N> &a) {
	fp<T, N>	r;
	negate_expansion<T>(N, a, r.t);
	return r;
}
template<typename T, int N> inline not_unrolled_t<T, N, 1> operator+(const fp<T,N> &a, fp<T,1> b) {
	fp<T, N + 1>	r;
	grow_expansion<T>(N, a, b, r.t);
	return r;
}

template<typename T, int NA, int NB> inline not_unrolled_t<T, NA, NB> operator+(const fp<T,NA> &a, const fp<T,NB> &b) {
	fp<T, NA + NB>	r;
	fast_expansion_sum<T>(NA, a, NB, b, r.t);
	return r;
}

template<typename T, int NA, int NB> inline not_unrolled_t<T, NA, NB> operator-(const fp<T,NA> &a, const fp<T,NB> &b) {
	return a + -b;
}

template<typename T, int N> inline auto operator*(const fp<T,N> &a, fp<T,1> b) {
	fp<T, N + N>	r;
	scale_expansion<T>(N, a, b, r.t);
	return r;
}

template<typename T, int NA, int NB> inline auto operator*(const fp<T,NA> &a, const fp<T,NB> &b) {
	return a * b.lo + a * b.hi;
}

template<typename T, int N> inline auto operator+(const fp<T,N> &a, T b)			{ return a + fp<T,1>(b); }
template<typename T, int N> inline auto operator*(const fp<T,N> &a, T b)			{ return a * fp<T,1>(b); }
template<typename T, int N> inline auto operator*(const fp<T,N> &a, decltype(two))	{ fp<T,N> r; scale_expansion_pow2<T>(N, a, T(2), r.t); return r; }
template<typename T, int N> inline auto operator<<(const fp<T,N> &a, int b)			{ fp<T,N> r; scale_expansion_pow2<T>(N, a, T(1 << b), r.t); return r; }
template<typename T, int N> inline auto operator>>(const fp<T,N> &a, int b)			{ fp<T,N> r; scale_expansion_pow2<T>(N, a, T(1) / T(1 << -b), r.t); return r; }

template<typename T, int N> constexpr auto square(const fp<T,N> &a) {
	return square(a.lo) + (a.lo * a.hi) * two + square(a.hi);
}

//-----------------------------------------------------------------------------
//	multi-precision floats with variable length
//-----------------------------------------------------------------------------

template<typename T, int N> struct fpv {
	T	t[N];
	int	n;
	fpv()		: n(0)	{}
	fpv(T x)	: n(1)	{ t[0] = x; }
	template<int N2> fpv(const fpv<T, N2> &x)									: n(maybe_compress<T>(x.n, x, t, N))	{}
	template<int N2, typename = enable_if_t<(N2 <= N)>> fpv(const fp<T,N2> &x)	: n(zeroelim<T>(N2, x, t))			{}
	
	fpv&	operator=(T x)			{ t[0] = x; n = 1; return *this; }
	fpv&	operator=(fp<T,1> x)	{ t[0] = x.t; n = 1; return *this; }

	template<int N2> fpv& operator=(const fpv<T,N2> &x) { n = maybe_compress<T>(x.n, x.t, t, N); return *this; }
	template<int N2> fpv& operator=(const fp<T,N2> &x)	{ n = zeroelim<T>(N2, x, t); return *this; }

	constexpr operator const T*()	const	{ return t; }
	T		top()					const	{ return n ? t[n - 1] : 0; }

	friend T estimate(const fpv &a)	{ return estimate(a.n, a.t); }
	friend T sign(const fpv &a)		{ return sign(a.top()); }
};

template<typename T, int N> fpv<T,N> zero_elim(const fp<T, N> &a) {
	return a;
}


template<typename T, int N> inline auto operator+(const fpv<T,N> &a, fp<T,1> b) {
	fpv<T, N + 1>	r;
	r.n = grow_expansion_zeroelim<T>(a.n, a.t, b, r.t);
	return r;
}

template<typename T, int NA, int NB> inline auto operator+(const fpv<T,NA> &a, const fpv<T,NB> &b) {
	fpv<T, NA + NB>	r;
	r.n = fast_expansion_sum_zeroelim<T>(a.n, a.t, b.n, b.t, r.t);
	return r;
}
template<typename T, int NA, int NB> inline auto operator+(const fpv<T,NA> &a, const fp<T,NB> &b) {
	fpv<T, NA + NB>	r;
	r.n = fast_expansion_sum_zeroelim<T>(a.n, a.t, NB, b, r.t);
	return r;
}
template<typename T, int NA, int NB> inline auto operator+(const fp<T,NA> &a, const fpv<T,NB> &b) {
	fpv<T, NA + NB>	r;
	r.n = fast_expansion_sum_zeroelim<T>(NA, a, b.n, b.t, r.t);
	return r;
}

template<typename T, int NA, typename B> inline auto operator-(const fpv<T,NA> &a, const B &b) {
	return a + -b;
}

template<typename T, int N> inline auto operator*(const fpv<T,N> &a, fp<T,1> b) {
	fpv<T, N + N>	r;
	r.n = scale_expansion_zeroelim<T>(a.n, a.t, b.t, r.t);
	return r;
}

template<typename T, int NA, int NB> inline auto operator*(const fpv<T,NA> &a, fpv<T,NB> b) {
	fpv<T, NA * NB * 2>	r;
	T	scratch[NA * NB * 2];
	r.n = expansion_mul_zeroelim(a.n, a.t, b.n, b.t, r.t, scratch);
	return r;
}

template<typename T, int N> inline auto& operator+=(fpv<T,N> &a, fpv<T,1> b) {
	a.n = grow_expansion_zeroelim<T>(a.n, a.t, b, a.t);
	return a;
}

template<typename T, int N> inline auto		operator+(const fpv<T,N> &a, T b)			{ return a + fp<T,1>(b); }
template<typename T, int N> inline auto		operator*(const fpv<T,N> &a, T b)			{ return a * fp<T,1>(b); }
template<typename T, int N> inline auto		operator*(const fpv<T,N> &a, decltype(two))	{ fpv<T,N> r; scale_expansion_pow2<T>(a.n, a, T(2), r.t); return r; }
template<typename T, int N> inline auto		operator<<(const fpv<T,N> &a, int b)		{ fpv<T,N> r; scale_expansion_pow2<T>(a.n, a, T(1 << b), r.t); return r; }
template<typename T, int N> inline auto		operator>>(const fpv<T,N> &a, int b)		{ fpv<T,N> r; scale_expansion_pow2<T>(a.n, a, T(1) / T(1 << b), r.t); return r; }
template<typename T, int N> inline auto&	operator+=(fpv<T,N> &a, T b)				{ return a += fp<T,1>(b); }

template<int N> constexpr int Nsquare = Nsquare<N / 2> + Nsquare<N - N / 2> + (N * N / 2);
template<> constexpr int Nsquare<1> = 2;
template<typename T, int N> constexpr auto square(const fpv<T,N> &a) {
	constexpr int N2 = Nsquare<N>;
	fpv<T, N2>	r;
	T	scratch[N2];
	r.n = expansion_square_zeroelim(a.n, a.t, r.t, scratch);
	return r;
}

extern char	decimal_point;

template<typename T> string_accum& expansion_string(string_accum &a, int N, const T *acc) {
	typedef float_components<T> comp_t;
	typedef	comp_t::I	I;
	constexpr int M = sizeof(I) * 8 - 5;

	const T	*accp	= acc + N - 1;

	auto	info0	= get_print_info<10>(acc[0]);
	int		exp0	= info0.exp;
	auto	digits0	= get_floatlen<10, M>(shift_bits(info0.mant, M - info0.frac), I(0));
	if (exp0 > 0)
		digits0 += exp0;

	auto	info	= get_print_info<10>(*accp);
	int		digits	= info.exp - exp0 + digits0;

	int		exp		= info.exp;
	auto	m		= shift_bits(info.mant, M - info.frac);

	int		len		= int(info.sign) + (exp < 0 ? digits + 1 - exp : digits > exp + 1 ? digits + 1 : exp + 1);
	auto	p		= a.getp(len);

	if (exp < 0) {
		*p++ = '0';
		*p++ = decimal_point;
		for (int i = exp; i < -1; ++i)
			*p++ = '0';
	}

	while (accp > acc) {
		info	= get_print_info<10>(*--accp);
		digits	= exp - info.exp;

		//test for carry
		auto	m1	= shift_bits(info.mant, M - info.frac);
		if (!info.sign && digits <= klog_base<10, I(~0)>) {
			auto	d	= pow(10, digits);
			auto	m2	= div_round_up(m1, d);
			m	+= m2;
			m1	-= m2 * d;
		}


		if (exp >= 0 && exp < digits) {
			p = put_float_digits_n<10, M>(p, m, exp + 1);
			*p++ = decimal_point;
			digits -= exp + 1;
		}
		p		= put_float_digits_n<10, M>(p, m, digits);

		if (info.sign)
			m	-= m1;
		else
			m	+= m1;

		ISO_ASSERT((m >> M) < 10);
		exp		= info.exp;
	}

	// last component
	if (exp >= 0 && exp < digits0) {
		p = put_float_digits_n<10, M>(p, m, exp + 1);
		*p++ = decimal_point;
		digits0 -= exp + 1;
	}
	p		= put_float_digits_n<10, M>(p, m, digits0);

	return a;
}

template<typename T, int N> string_accum& operator<<(string_accum &a, const fpv<T, N>& x) {
	T	acc[N];
	T	f	= 0;
	int	n	= x.n;
	for (int i = 0; i < n; i++) {
		f += x[i];
		acc[i] = f;
	}
	return expansion_string(a, x.n, (const T*)x);
}

template<typename T, int N> string_accum& operator<<(string_accum &a, const fp<T, N>& x) {
	return a << zero_elim(x);
}

//-----------------------------------------------------------------------------
//	multi-precision floats with variable length and double buffer
//-----------------------------------------------------------------------------

template<typename T, int N> class fpv2 {
	T	*t;
	int	n;
	T	scratch[N * 2];

	T	*next_buffer() {
		T	*t0 = t;
		t = t0 == scratch ? scratch + N : scratch;
	}
public:
	fpv2()		: t(scratch), n(0)	{}
	fpv2(T x)	: t(scratch), n(1)	{ t[0] = x; }
	template<int N2> fpv2(const fpv<T, N2> &x)									: t(scratch), n(maybe_compress<T>(x.n, x, t, N)) {}
	template<int N2, typename = enable_if_t<(N2 <= N)>> fpv2(const fp<T,N2> &x)	: t(scratch), n(zeroelim<T>(N2, x, t)) {}

	fpv2&	operator=(T x)			{ t[0] = x; n = 1; return *this; }
	fpv2&	operator=(fp<T,1> x)	{ t[0] = x.t; n = 1; return *this; }
	template<int N2> fpv2&	operator=(const fpv<T,N2> &x)	{ n = maybe_compress<T>(x.n, x, t, N); return *this; }
	template<int N2> fpv2&	operator=(const fp<T,N2> &x)	{ n = zeroelim<T>(N2, x, t); return *this; }

	fpv2& operator+=(fpv<T,1> b) {
		if (n == N)
			n = compress(n, t, t);
		n = grow_expansion_zeroelim<T>(n, t, b, t);
		return *this;
	}
	template<int NB> fpv2& operator+=(const fpv<T,NB> &b) {
		if (b.n) {
			if (n + b.n > N)
				n = compress(n, t, t);
			T	*t0 = t;
			t = t0 == scratch ? scratch + N : scratch;
			n = fast_expansion_sum_zeroelim<T>(n, t0, b.n, b.t, t);
		}
		return *this;
	}
	template<int NB> fpv2& operator+=(const fp<T,NB> &b) {
		if (n + NB > N)
			n = compress(n, t, t);
		T	*t0 = t;
		t = t0 == scratch ? scratch + N : scratch;
		n = fast_expansion_sum_zeroelim<T>(n, t0, NB, b.t, t);
		return *this;
	}
	template<typename B> fpv2& operator-=(const B &b) {
		return operator+=(-b);
	}

	constexpr operator const T*()	const	{ return t; }
	T		top()					const	{ return t[n - 1]; }

	friend T estimate(const fpv2 &a)	{ return estimate(a.n, a.t); }
	friend T sign(const fpv2 &a)		{ return sign(a.top()); }
};

#if 1
//-----------------------------------------------------------------------------
//  orient2d
//-----------------------------------------------------------------------------

template<typename T> int orient2dfast(T *pa, T *pb, T *pc) {
	auto	v	= (pa[0] - pc[0]) * (pb[1] - pc[1])
				- (pa[1] - pc[1]) * (pb[0] - pc[0]);
	return sign(v);
}

template<typename T> int orient2dexact(T *_pa, T *_pb, T *_pc) {
	auto	pa		= (fp<T,1>*)_pa;
	auto	pb		= (fp<T,1>*)_pb;
	auto	pc		= (fp<T,1>*)_pc;

	auto	v = (pa[0] * pb[1]) - (pa[0] * pc[1])
			+	(pb[0] * pc[1]) - (pb[0] * pa[1])
			+	(pc[0] * pa[1]) - (pc[0] * pb[1]);

	return sign(v);
}

template<typename T> int orient2d(T *_pa, T *_pb, T *_pc) {
	static constexpr T resulterrbound	= (3 + 8 * eps<T>) * eps<T>;
//	static constexpr T ccwerrboundA	= ((F - F) * (F - F) < (F - F) * (F - F)).error<T>();//(3 + 16 * eps<T>) * eps<T>;
	static constexpr T ccwerrboundB	= (2 + 12 * eps<T>) * eps<T>;
	static constexpr T ccwerrboundC	= (9 + 64 * eps<T>) * eps<T> * eps<T>;

	auto	pa		= (fpe<T>*)_pa;
	auto	pb		= (fpe<T>*)_pb;
	auto	pc		= (fpe<T>*)_pc;

	auto	acx		= pa[0] - pc[0];
	auto	bcx		= pb[0] - pc[0];
	auto	acy		= pa[1] - pc[1];
	auto	bcy		= pb[1] - pc[1];

	auto	detleft		= acx * bcy;
	auto	detright	= acy * bcx;
	if (auto cz = compare(detleft, detright))
		return cz;

	T		detsum	= detleft.e + detright.e;

	auto	B		= extend_mul(acx.x, bcy.x, detleft.x) - extend_mul(acy.x, bcx.x, detright.x);
	T		det		= estimate(B);

	if (auto c = cz(det, ccwerrboundB * detsum))
		return c;

	auto	acx2	= extend_sub(pa[0].x, pc[0].x, acx.x);
	auto	bcx2	= extend_sub(pb[0].x, pc[0].x, bcx.x);
	auto	acy2	= extend_sub(pa[1].x, pc[1].x, acy.x);
	auto	bcy2	= extend_sub(pb[1].x, pc[1].x, bcy.x);

	if (acx2.lo == 0 && acy2.lo == 0 && bcx2.lo == 0 && bcy2.lo == 0)
		return sign(det);

	T		errbound = ccwerrboundC * detsum + resulterrbound * abs(det);
	det += (acx2[1] * bcy2[0] + bcy2[1] * acx2[0])
		-  (acy2[1] * bcx2[0] + bcx2[1] * acy2[0]);

	if (auto c = cz(det, errbound))
		return c;

	auto	d = zero_elim(B)
		+ (acx2.lo * bcy2.hi	- acy2.lo * bcx2.hi)
		+ (acx2.hi * bcy2.lo	- acy2.hi * bcx2.lo)
		+ (acx2.lo * bcy2.lo	- acy2.lo * bcx2.lo);

	return sign(d);
}

//-----------------------------------------------------------------------------
//  orient3d
//-----------------------------------------------------------------------------

template<typename T> int orient3dfast(T *pa, T *pb, T *pc, T *pd) {
	T	adx = pa[0] - pd[0];
	T	bdx = pb[0] - pd[0];
	T	cdx = pc[0] - pd[0];
	T	ady = pa[1] - pd[1];
	T	bdy = pb[1] - pd[1];
	T	cdy = pc[1] - pd[1];
	T	adz = pa[2] - pd[2];
	T	bdz = pb[2] - pd[2];
	T	cdz = pc[2] - pd[2];

	return	adx * (bdy * cdz - bdz * cdy)
		+	bdx * (cdy * adz - cdz * ady)
		+	cdx * (ady * bdz - adz * bdy);
}

template<typename T> int orient3dexact(T *_pa, T *_pb, T *_pc, T *_pd) {
	auto	pa		= (fp<T,1>*)_pa;
	auto	pb		= (fp<T,1>*)_pb;
	auto	pc		= (fp<T,1>*)_pc;
	auto	pd		= (fp<T,1>*)_pd;

	auto	ab		= (pa[0] * pb[1]) - (pb[0] * pa[1]);
	auto	bc		= (pb[0] * pc[1]) - (pc[0] * pb[1]);
	auto	cd		= (pc[0] * pd[1]) - (pd[0] * pc[1]);
	auto	da		= (pd[0] * pa[1]) - (pa[0] * pd[1]);
	auto	ac		= (pa[0] * pc[1]) - (pc[0] * pa[1]);
	auto	bd		= (pb[0] * pd[1]) - (pd[0] * pb[1]);

	auto	cda		= zero_elim(cd) + da + ac;
	auto	dab		= zero_elim(da) + ab + bd;

	auto	abc		= zero_elim(ab) + bc - ac;
	auto	bcd		= zero_elim(bc) + cd - bd;

	auto	adet	= bcd *  pa[2];
	auto	bdet	= cda * -pb[2];
	auto	cdet	= dab *  pc[2];
	auto	ddet	= abc * -pd[2];

	auto	det		= (adet + bdet) + (cdet + ddet);
	return sign(det);
}

template<typename T> int orient3d(T *_pa, T *_pb, T *_pc, T *_pd) {
	static constexpr T resulterrbound	= (3 + 8 * eps<T>) * eps<T>;
	static constexpr T o3derrboundA		= (7 + 56 * eps<T>) * eps<T>;
	static constexpr T o3derrboundB		= (3 + 28 * eps<T>) * eps<T>;
	static constexpr T o3derrboundC		= (26 + 288 * eps<T>) * eps<T> * eps<T>;

	auto	pa		= (fpe<T>*)_pa;
	auto	pb		= (fpe<T>*)_pb;
	auto	pc		= (fpe<T>*)_pc;
	auto	pd		= (fpe<T>*)_pd;

	auto	adx		= pa[0] - pd[0];
	auto	bdx		= pb[0] - pd[0];
	auto	cdx		= pc[0] - pd[0];
	auto	ady		= pa[1] - pd[1];
	auto	bdy		= pb[1] - pd[1];
	auto	cdy		= pc[1] - pd[1];
	auto	adz		= pa[2] - pd[2];
	auto	bdz		= pb[2] - pd[2];
	auto	cdz		= pc[2] - pd[2];

	auto	bdxcdy	= bdx * cdy;
	auto	cdxbdy	= cdx * bdy;

	auto	cdxady	= cdx * ady;
	auto	adxcdy	= adx * cdy;

	auto	adxbdy	= adx * bdy;
	auto	bdxady	= bdx * ady;

	auto	adet = adz * (bdxcdy - cdxbdy);
	auto	bdet = bdz * (cdxady - adxcdy);
	auto	cdet = cdz * (adxbdy - bdxady);

	if (auto cz = add_cz(adet + bdet, cdet))
		return cz;

	auto	detsum	= adet.e + bdet.e + cdet.e;

	auto	bc		= zero_elim(extend_mul(bdx.x, cdy.x, bdxcdy.x) - extend_mul(cdx.x, bdy.x, cdxbdy.x));
	auto	ca		= zero_elim(extend_mul(cdx.x, ady.x, cdxady.x) - extend_mul(adx.x, cdy.x, adxcdy.x));
	auto	ab		= zero_elim(extend_mul(adx.x, bdy.x, adxbdy.x) - extend_mul(bdx.x, ady.x, bdxady.x));

//	fpv2<T,192>	fin	= bc * adz.x + ca * bdz.x + ab * cdz.x;
	fpv2<T,8>	fin	= bc * adz.x + ca * bdz.x + ab * cdz.x;
	
	auto	det		= estimate(fin);
	if (auto c = cz(det, o3derrboundB * detsum))
		return c;

	auto	adx2	= extend_sub(pa[0].x, pd[0].x, adx.x);
	auto	bdx2	= extend_sub(pb[0].x, pd[0].x, bdx.x);
	auto	cdx2	= extend_sub(pc[0].x, pd[0].x, cdx.x);
	auto	ady2	= extend_sub(pa[1].x, pd[1].x, ady.x);
	auto	bdy2	= extend_sub(pb[1].x, pd[1].x, bdy.x);
	auto	cdy2	= extend_sub(pc[1].x, pd[1].x, cdy.x);
	auto	adz2	= extend_sub(pa[2].x, pd[2].x, adz.x);
	auto	bdz2	= extend_sub(pb[2].x, pd[2].x, bdz.x);
	auto	cdz2	= extend_sub(pc[2].x, pd[2].x, cdz.x);

	if (adx2.lo == 0 && bdx2.lo == 0 && cdx2.lo == 0
	&&  ady2.lo == 0 && bdy2.lo == 0 && cdy2.lo == 0
	&&  adz2.lo == 0 && bdz2.lo == 0 && cdz2.lo == 0
	)
		return sign(det);

	det += (adz2[1] * ((bdx2[1] * cdy2[0] + cdy2[1] * bdx2[0]) - (bdy2[1] * cdx2[0] + cdx2[1] * bdy2[0])) + adz2[0] * (bdx2[1] * cdy2[1] - bdy2[1] * cdx2[1]))
		+  (bdz2[1] * ((cdx2[1] * ady2[0] + ady2[1] * cdx2[0]) - (cdy2[1] * adx2[0] + adx2[1] * cdy2[0])) + bdz2[0] * (cdx2[1] * ady2[1] - cdy2[1] * adx2[1]))
		+  (cdz2[1] * ((adx2[1] * bdy2[0] + bdy2[1] * adx2[0]) - (ady2[1] * bdx2[0] + bdx2[1] * ady2[0])) + cdz2[0] * (adx2[1] * bdy2[1] - ady2[1] * bdx2[1]));

	if (auto c = cz(det, o3derrboundC * detsum + resulterrbound * abs(det)))
		return sign(det);

	fpv<T,4> at_b, at_c, bt_c, bt_a, ct_a, ct_b;

	if (adx2.lo == 0) {
		if (ady2.lo != 0) {
			at_b = -ady2.lo * bdx2.hi;
			at_c =  ady2.lo * cdx2.hi;
		}
	} else if (ady2.lo == 0) {
		at_b =  adx2.lo * bdy2.hi;
		at_c = -adx2.lo * cdy2.hi;
	} else {
		at_b = adx2.lo * bdy2.hi - ady2.lo * bdx2.hi;
		at_c = ady2.lo * cdx2.hi - adx2.lo * cdy2.hi;
	}

	if (bdx2.lo == 0) {
		if (bdy2.lo != 0) {
			bt_c = -bdy2.lo * cdx2.hi;
			bt_a =  bdy2.lo * adx2.hi;
		}
	} else if (bdy2.lo == 0) {
		bt_c =  bdx2.lo * cdy2.hi;
		bt_a = -bdx2.lo * ady2.hi;
	} else {
		bt_c = bdx2.lo * cdy2.hi - bdy2.lo * cdx2.hi;
		bt_a = bdy2.lo * adx2.hi - bdx2.lo * ady2.hi;
	}

	if (cdx2.lo == 0) {
		if (cdy2.lo != 0) {
			ct_a = -cdy2.lo * adx2.hi;
			ct_b =  cdy2.lo * bdx2.hi;
		}
	} else if (cdy2.lo == 0) {
		ct_a =  cdx2.lo * ady2.hi;
		ct_b = -cdx2.lo * bdy2.hi;
	} else {
		ct_a = cdx2.lo * ady2.hi - cdy2.lo * adx2.hi;
		ct_b = cdy2.lo * bdx2.hi - cdx2.lo * bdy2.hi;
	}

	auto	bct = bt_c + ct_b;
	fin += bct * adz2.hi;

	auto	cat = ct_a + at_c;
	fin += cat * bdz2.hi;

	auto	abt = at_b + bt_a;
	fin += abt * cdz2.hi;

	if (adz2.lo != 0)
		fin	+= bc * adz2.lo;

	if (bdz2.lo != 0)
		fin += ca * bdz2.lo;

	if (cdz2.lo != 0)
		fin += ab * cdz2.lo;

	if (adx2.lo != 0) {
		if (bdy2.lo != 0) {
			auto	t = adx2.lo * bdy2.lo;
			fin += t * cdz2.hi;
			if (cdz2.lo != 0)
				fin += t * cdz2.lo;
		}
		if (cdy2.lo != 0) {
			auto	t = -adx2.lo * cdy2.lo;
			fin += t * bdz2.hi;
			if (bdz2.lo != 0)
				fin += t * bdz2.lo;
		}
	}
	if (bdx2.lo != 0) {
		if (cdy2.lo != 0) {
			auto	t = bdx2.lo * cdy2.lo;
			fin += t * adz2.hi;
			if (adz2.lo != 0)
				fin += t * adz2.lo;
		}
		if (ady2.lo != 0) {
			auto	t = -bdx2.lo * ady2.lo;
			fin += t * cdz2.hi;
			if (cdz2.lo != 0)
				fin += t * cdz2.lo;
		}
	}
	if (cdx2.lo != 0) {
		if (ady2.lo != 0) {
			auto	t = cdx2.lo * ady2.lo;
			fin += t * bdz2.hi;
			if (bdz2.lo != 0)
				fin += t * bdz2.lo;
		}
		if (bdy2.lo != 0) {
			auto	t = -cdx2.lo * bdy2.lo;
			fin += t * adz2.hi;
			if (adz2.lo != 0)
				fin += t * adz2.lo;
		}
	}

	if (adz2.lo != 0)
		fin += bct * adz2.lo;

	if (bdz2.lo != 0)
		fin += cat * bdz2.lo;

	if (cdz2.lo != 0)
		fin += abt * cdz2.lo;

	return sign(fin);
}

//-----------------------------------------------------------------------------
//  incircle
//-----------------------------------------------------------------------------

template<typename T> int incirclefast(T *pa, T *pb, T *pc, T *pd) {
	T	adx = pa[0] - pd[0];
	T	ady = pa[1] - pd[1];
	T	bdx = pb[0] - pd[0];
	T	bdy = pb[1] - pd[1];
	T	cdx = pc[0] - pd[0];
	T	cdy = pc[1] - pd[1];

	T	abdet = adx * bdy - bdx * ady;
	T	bcdet = bdx * cdy - cdx * bdy;
	T	cadet = cdx * ady - adx * cdy;
	T	alift = adx * adx + ady * ady;
	T	blift = bdx * bdx + bdy * bdy;
	T	clift = cdx * cdx + cdy * cdy;

	return sign(alift * bcdet + blift * cadet + clift * abdet);
}

template<typename T> int incircleexact(T *_pa, T *_pb, T *_pc, T *_pd) {
	auto	pa	= (fpe<T>*)_pa;
	auto	pb	= (fpe<T>*)_pb;
	auto	pc	= (fpe<T>*)_pc;
	auto	pd	= (fpe<T>*)_pd;

	auto	ab	= zero_elim(pa[0] * pb[1] - pb[0] * pa[1]);
	auto	bc	= zero_elim(pb[0] * pc[1] - pc[0] * pb[1]);
	auto	cd	= zero_elim(pc[0] * pd[1] - pd[0] * pc[1]);
	auto	da	= zero_elim(pd[0] * pa[1] - pa[0] * pd[1]);
	auto	ac	= zero_elim(pa[0] * pc[1] - pc[0] * pa[1]);
	auto	bd	= zero_elim(pb[0] * pd[1] - pd[0] * pb[1]);

	auto	cda	= cd + da + ac;
	auto	dab	= da + ab + bd;
	auto	abc	= ab + bc - ac;
	auto	bcd = bc + cd - bd;

	auto	det =
		(
			(bcd * pa[0] *  pa[0] + bcd * pa[1] *  pa[1])
		+	(cda * pb[0] * -pb[0] + cda * pb[1] * -pb[1])
		) + (
			(dab * pc[0] * -pc[0] + dab * pc[1] *  pc[1])
		+	(abc * pd[0] * -pd[0] + abc * pd[1] * -pd[1])
		);

	return sign(det);
}


template<typename T> int incircle(T *_pa, T *_pb, T *_pc, T *_pd) {
	static constexpr T resulterrbound	= (3 + 8 * eps<T>) * eps<T>;
	static constexpr T iccerrboundA	= (10 + 96 * eps<T>) * eps<T>;
	static constexpr T iccerrboundB	= (4 + 48 * eps<T>) * eps<T>;
	static constexpr T iccerrboundC	= (44 + 576 * eps<T>) * eps<T> * eps<T>;

	auto	pa		= (fpe<T>*)_pa;
	auto	pb		= (fpe<T>*)_pb;
	auto	pc		= (fpe<T>*)_pc;
	auto	pd		= (fpe<T>*)_pd;

	auto	adx		= pa[0] - pd[0];
	auto	bdx		= pb[0] - pd[0];
	auto	cdx		= pc[0] - pd[0];
	auto	ady		= pa[1] - pd[1];
	auto	bdy		= pb[1] - pd[1];
	auto	cdy		= pc[1] - pd[1];

	auto	bdxcdy	= bdx * cdy;
	auto	cdxbdy	= cdx * bdy;

	auto	cdxady	= cdx * ady;
	auto	adxcdy	= adx * cdy;

	auto	adxbdy	= adx * bdy;
	auto	bdxady	= bdx * ady;

	auto	adet	= (adx * adx + ady * ady) * (bdxcdy - cdxbdy);
	auto	bdet	= (bdx * bdx + bdy * bdy) * (cdxady - adxcdy);
	auto	cdet	= (cdx * cdx + cdy * cdy) * (adxbdy - bdxady);

	if (auto c = add_cz(adet + bdet, cdet))
		return c;

	auto	detsum	= adet.e + bdet.e + cdet.e;

	auto	bc		= zero_elim(extend_mul(bdx.x, cdy.x, bdxcdy.x) - extend_mul(cdx.x, bdy.x, cdxbdy.x));
	auto	ca		= zero_elim(extend_mul(cdx.x, ady.x, cdxady.x) - extend_mul(adx.x, cdy.x, adxcdy.x));
	auto	ab		= zero_elim(extend_mul(adx.x, bdy.x, adxbdy.x) - extend_mul(bdx.x, ady.x, bdxady.x));

	fpv2<T, 1152>	fin = bc * adx.x * adx.x + bc * ady.x * ady.x
						+ ca * bdx.x * bdx.x + ca * bdy.x * bdy.x
						+ ab * cdx.x * cdx.x + ab * cdy.x * cdy.x;

	auto	det = estimate(fin);
	if (auto c = cz(det, iccerrboundB * detsum))
		return c;

	auto	adx2 = extend_sub(pa[0].x, pd[0].x, adx.x);
	auto	ady2 = extend_sub(pa[1].x, pd[1].x, ady.x);
	auto	bdx2 = extend_sub(pb[0].x, pd[0].x, bdx.x);
	auto	bdy2 = extend_sub(pb[1].x, pd[1].x, bdy.x);
	auto	cdx2 = extend_sub(pc[0].x, pd[0].x, cdx.x);
	auto	cdy2 = extend_sub(pc[1].x, pd[1].x, cdy.x);

	if (adx2.lo == 0 && bdx2.lo == 0 && cdx2.lo == 0 && ady2.lo == 0 && bdy2.lo == 0 && cdy2.lo == 0)
		return sign(det);

	det += ((adx2[1] * adx2[1] + ady2[1] * ady2[1]) * ((bdx2[1] * cdy2[0] + cdy2[1] * bdx2[0])	- (bdy2[1] * cdx2[0] + cdx2[1] * bdy2[0])) + 2 * (adx2[1] * adx2[0] + ady2[1] * ady2[0]) * (bdx2[1] * cdy2[1] - bdy2[1] * cdx2[1]))
		+  ((bdx2[1] * bdx2[1] + bdy2[1] * bdy2[1]) * ((cdx2[1] * ady2[0] + ady2[1] * cdx2[0])	- (cdy2[1] * adx2[0] + adx2[1] * cdy2[0])) + 2 * (bdx2[1] * bdx2[0] + bdy2[1] * bdy2[0]) * (cdx2[1] * ady2[1] - cdy2[1] * adx2[1]))
		+  ((cdx2[1] * cdx2[1] + cdy2[1] * cdy2[1]) * ((adx2[1] * bdy2[0] + bdy2[1] * adx2[0])	- (ady2[1] * bdx2[0] + bdx2[1] * ady2[0])) + 2 * (cdx2[1] * cdx2[0] + cdy2[1] * cdy2[0]) * (adx2[1] * bdy2[1] - ady2[1] * bdx2[1]));

	if (auto c = cz(det, iccerrboundC * detsum + resulterrbound * abs(det)))
		return c;

	fpv<T,4>	aa, bb, cc;

	if (bdx2.lo != 0 || bdy2.lo != 0 || cdx2.lo != 0 || cdy2.lo != 0)
		aa	= square(adx2.hi) + square(ady2.hi);

	if (cdx2.lo != 0 || cdy2.lo != 0 || adx2.lo != 0 || ady2.lo != 0)
		bb	= square(bdx2.hi) + square(bdy2.hi);

	if (adx2.lo != 0 || ady2.lo != 0 || bdx2.lo != 0 || bdy2.lo != 0)
		cc	= square(cdx2.hi) + square(cdy2.hi);

	fpv<T,8>	axtbc, aytbc, bxtca, bytca, cxtab, cytab;

	if (adx2.lo != 0) {
		axtbc	= bc * adx2.lo;
		fin		+= axtbc * (adx2.hi * two) + cc * adx2.lo * bdy2.hi + bb * adx2.lo * -cdy2.hi;
	}
	if (ady2.lo != 0) {
		aytbc	= bc * ady2.lo;
		fin		+= aytbc * (adx2.hi * two) + bb * ady2.lo * cdx2.hi + cc * ady2.lo * -bdx2.hi;
	}
	if (bdx2.lo != 0) {
		bxtca	= ca * bdx2.lo;
		fin		+= bxtca * (bdx2.hi * two) + aa * bdx2.lo * cdy2.hi + cc * bdx2.lo * -ady2.hi;
	}
	if (bdy2.lo != 0) {
		bytca	= ca * bdy2.lo;
		fin		+= bytca * (bdy2.hi * two) + cc * bdy2.lo * adx2.hi + aa * bdy2.lo * -cdx2.hi;
	}
	if (cdx2.lo != 0) {
		cxtab	= ab * cdx2.lo;
		fin		+= cxtab * (cdx2.hi * two) + bb * cdx2.lo * ady2.hi + aa * cdx2.lo * -bdy2.hi;
	}
	if (cdy2.lo != 0) {
		cytab	= ab * cdy2.lo;
		fin		+= cytab * (cdy2.hi * two) + aa * cdy2.lo * bdx2.hi + bb * cdy2.lo * -adx2.hi;
	}

	if (adx2.lo != 0 || ady2.lo != 0) {
		fpv<T,8>	bct;
		fpv<T,4>	bctt;

		if (bdx2.lo != 0 || bdy2.lo != 0 || cdx2.lo != 0 || cdy2.lo != 0) {
			bct		= zero_elim(bdx2.lo * cdy2.hi + bdx2.hi * cdy2.lo) + cdx2.lo * -bdy2.hi + cdx2.hi * -bdy2.lo;
			bctt	= bdx2.lo * cdy2.lo - cdx2.lo * bdy2.lo;
		}

		if (adx2.lo != 0) {
			auto	t = bct * adx2.lo;
			fin += axtbc * adx2.lo + t * (adx2.hi * two);

			if (bdy2.lo != 0)
				fin	+= cc * adx2.lo * bdy2.lo;

			if (cdy2.lo != 0)
				fin += bb * -adx2.lo * cdy2.lo;

			auto	tt = bctt * adx2.lo;
			fin += t * adx2.lo + (tt * (adx2.hi * two) + tt * adx2.lo);
		}
		if (ady2.lo != 0) {
			auto	t = bct * ady2.lo;
			fin += aytbc * ady2.lo + t * (ady2.hi * two);

			auto	tt = bctt * ady2.lo;
			fin += tt * (ady2.hi * two) + tt * ady2.lo + t * ady2.lo;
		}
	}

	if (bdx2.lo != 0 || bdy2.lo != 0) {
		fpv<T,8>	cat;
		fpv<T,4>	catt;

		if (cdx2.lo != 0 || cdy2.lo != 0 || adx2.lo != 0 || ady2.lo != 0) {
			cat		= zero_elim(cdx2.lo * ady2.hi + cdx2.hi * ady2.lo) + (adx2.lo * -cdy2.hi + adx2.hi * -cdy2.lo);
			catt	= cdx2.lo *  ady2.lo - adx2.lo * cdy2.lo;
		}

		if (bdx2.lo != 0) {
			auto	t = cat * bdx2.lo;
			fin += bxtca * bdx2.lo + t * (bdx2.hi * two);

			if (cdy2.lo != 0)
				fin += aa * bdx2.lo * cdy2.lo;

			if (ady2.lo != 0)
				fin += cc * bdx2.lo * ady2.lo;

			auto	tt = catt * bdx2.lo;
			fin += tt * (bdx2.hi * two) + tt * bdx2.lo + t * bdx2.lo;
		}
		if (bdy2.lo != 0) {
			auto	t = cat * bdy2.lo;
			fin += bytca * bdy2.lo + t * (bdy2.hi * two);

			auto	tt = catt *  bdy2.lo;
			fin += tt * (bdy2.hi * two) + tt * bdy2.lo + t * bdy2.lo;
		}
	}

	if (cdx2.lo != 0 || cdy2.lo != 0) {
		fpv<T,8>	abt;
		fpv<T,4>	abtt;

		if (adx2.lo != 0 || ady2.lo != 0 || bdx2.lo != 0 || bdy2.lo != 0) {
			abt		= adx2.lo * bdy2.hi + adx2.hi * bdy2.lo + bdx2.lo * -ady2.hi + bdx2.hi * -ady2.lo;
			abtt	= adx2.lo * bdy2.lo - bdx2.lo * ady2.lo;
		}

		if (cdx2.lo != 0) {
			auto	t = abt * cdx2.lo;
			fin += cxtab * cdx2.lo + t * (cdx2.hi * two);

			if (ady2.lo != 0)
				fin += bb * cdx2.lo * ady2.lo;

			if (bdy2.lo != 0)
				fin += aa * -cdx2.lo * bdy2.lo;

			auto	tt = abtt * cdx2.lo;
			fin += tt * (cdx2.hi * two) + tt * cdx2.lo + t * cdx2.lo;
		}
		if (cdy2.lo != 0) {
			auto	t = abt * cdy2.lo;
			fin += cytab * cdy2.lo + t * (cdy2.hi * two);

			auto	tt = abtt * cdy2.lo;
			fin += tt * (cdy2.hi * two) + tt * cdy2.lo + t * cdy2.lo;
		}
	}

	return sign(fin);
}


//-----------------------------------------------------------------------------
//  insphere
//-----------------------------------------------------------------------------

template<typename T> static constexpr T isperrboundA	= (16 + 224 * eps<T>) * eps<T>;
template<typename T> static constexpr T isperrboundB	= (5 + 72 * eps<T>) * eps<T>;
template<typename T> static constexpr T isperrboundC	= (71 + 1408 * eps<T>) * eps<T> * eps<T>;
#endif
}	 // namespace iso

#endif	// FPE_H
