#ifndef SPHERICAL_HARMONICS_H
#define SPHERICAL_HARMONICS_H

#include "base/vector.h"

namespace iso {

double factorial(int n);

namespace sh {

template<typename T, int N> struct vector {
	T	v[N];
	vector()							{}
	vector(_zero&)						{ clear(*this); }
	T&			operator[](int i)		{ return v[i]; }
	const T&	operator[](int i) const	{ return v[i]; }
};

template<typename T, int N> T dot(const vector<T,N> &a, const vector<T,N> &b) {
	T	t(zero);
	for (int i = 0; i < N; i++)
		t += a[i] * b[i];
	return t;
}

template<typename T, int N, int M> struct matrix : vector<vector<T,N>,M> {};

template<typename T, int N> struct centred_vector : vector<T, N * 2 + 1> {
	T&			operator[](int i)		{ return (this->v)[i + N]; }
	const T&	operator[](int i) const	{ return (this->v)[i + N]; }
};

template<typename T, int N, int M> struct centred_matrix : centred_vector<centred_vector<T,N>,M> {};

template<int L> class harmonics;
template<int L> class rotation;

template<> class rotation<1> : public centred_matrix<float,1,1> {
friend class rotation<2>;
	float P(int i, int a, int b) const {
		return 	b == -2	? (*this)[i][1] * (*this)[a][-1] + (*this)[i][-1] * (*this)[a][ 1]
			:	b ==  2	? (*this)[i][1] * (*this)[a][ 1] - (*this)[i][-1] * (*this)[a][-1]
			:	(*this)[i][0] * (*this)[a][b];
	}
public:
	rotation(param(float3x3) m) {
		v[0]	= force_cast<centred_vector<float,1> >(float3(m.y.yzx));
		v[1]	= force_cast<centred_vector<float,1> >(float3(m.z.yzx));
		v[2]	= force_cast<centred_vector<float,1> >(float3(m.x.yzx));
	}
};
template<int L> class rotation : rotation<L-1>, centred_matrix<float,L,L> {
friend class rotation<L + 1>;
friend class harmonics<L>;
	float P(int i, int a, int b) const {
		const rotation<1>	&R	= *this;
		return 	b == -L - 1	? R[i][1] * (*this)[a][-L] + R[i][-1] * (*this)[a][ L]
			:	b ==  L + 1	? R[i][1] * (*this)[a][ L] - R[i][-1] * (*this)[a][-L]
			:	R[i][0] * (*this)[a][b];
	}
public:
	rotation(param(float3x3) m);
	centred_vector<float,L>&		operator[](int i)		{ return centred_matrix<float,L,L>::operator[](i); }
	const centred_vector<float,L>&	operator[](int i) const	{ return centred_matrix<float,L,L>::operator[](i); }
};

#ifdef REFACTOR
template<int L> rotation<L>::rotation(param(float3x3) m) : rotation<L-1>(m) {
	const rotation<L-1> &M	= *this;

	for (int m = -L; m <= L; ++m) {
		int		a	= abs(m);
		int		s	= m < 0 ? -1 : 1;
		float3	t	= sqrt(float3(
			(L + a) * (L - a),
			(L + a - 1) * (L + a) / float(a > 1 ? 4 : 2),
			a && a < L - 1 ? (L - a - 1) * (L - a) / float(4) : 0
		));
		float	u	= t.x;
		float	v	= t.y;
		float	w	= t.z;
		for (int n = -L; n <= L; ++n) {
			(*this)[m][n] =
				(u ? u * M.P(0, m, n) : 0)
			+	(v * (a == 0
					? -M.P(1, 1, n) - M.P(-1, -1, n)
					:  M.P(s, a - 1, n) - (a > 1 ? s * M.P(-s, 1 - a, n) : 0)
				))
			+	(w ? w * (M.P(1, m + s, n) + s * M.P(-1, -m - s, n)) : 0);
		}
	}

	for (int n = -L; n <= L; ++n) {
		float	s = rsqrt(abs(n) == L ? (2 * L) * (2 * L - 1) : (L + n) * (L - n));
		for (int m = -L; m <= L; ++m)
			(*this)[m][n] *= s;
	}
}

#else

float3 uvw(int l, int m, int n);

template<int L> rotation<L>::rotation(param(float3x3) m) : rotation<L-1>(m) {
	const rotation<L-1> &M	= *this;

	for (int m = -L; m <= L; ++m) {
		for (int n = -L; n <= L; ++n) {
			float	t	= 0;
			float3	s = uvw(L, m, n);

			if (s.x)
				t += s.x * M.P(0, m, n);

			if (s.y) {
				float	v;
				if (m == 0) {
					v = M.P(1, 1, n) + M.P(-1, -1, n);
				} else if (m > 0) {
					v = M.P(1,  m - 1, n);
					if (m == 1)
						v *= sqrt2;
					else
						v -= M.P(-1, -m + 1, n);
				} else {
					v = M.P(-1, -m - 1, n);
					if (m == -1)
						v *= sqrt2;
					else
						v += M.P( 1,  m + 1, n);
				}
				t += s.y * v;
			}

			if (s.z) {
				t += s.z * (m > 0
					? M.P(1, m + 1, n) + M.P(-1, -m - 1, n)
					: M.P(1, m - 1, n) - M.P(-1, -m + 1, n)
				);
			}
			(*this)[m][n] = t;
		}
	}
}
#endif

template<int L> class harmonics : public harmonics<L-1>, centred_vector<float,L> {
	friend class harmonics<L+1>;
	friend harmonics<L> operator*(const rotation<L> &r, const harmonics<L> &h) {
		return harmonics<L>(r, h);
	}
	harmonics(const rotation<L> &r, const harmonics<L> &h) : harmonics<L-1>(r, h) {
		for (int m = -L; m <= L; m++) {
			float	t = 0;
			for (int n = -L; n <= L; ++n)
				t += r[m][n] * h[n];
			(*this)[m] = t;
		}
	}
public:
	harmonics() {}
	float&			operator[](int i)		{ return centred_vector<float,L>::operator[](i); }
	const float&	operator[](int i) const	{ return centred_vector<float,L>::operator[](i); }
};
template<> class harmonics<0> : centred_vector<float,0> {
	friend class harmonics<1>;
	harmonics(const rotation<1> &m, const harmonics<1> &h) {}
public:
	harmonics()	{}
};

// evaluate an Associated Legendre Polynomial P(l,m,x) at x
double P(int l, int m, double x);

// renormalisation constant for SH function
double K(int l, int m);

// return an (unscaled) point sample of a Spherical Harmonic basis function
// l is the band, range [0..N]
// m in the range [-l..l]
// theta in the range [0..Pi]
// phi in the range [0..2*Pi]
double SH(int l, int m, double theta, double phi);

// hemispherical harmonics

// renormalisation constant for HSH function
double HK(int l, int m);
// return an (unscaled) point sample of a Hemispherical Harmonic basis function
// l is the band, range [0..N]
// m in the range [-l..l]
// theta in the range [0..Pi/2]
// phi in the range [0..2*Pi]
double HSH(int l, int m, double theta, double phi);

} }// namespace iso::sh

#endif //SPHERICAL_HARMONICS_H