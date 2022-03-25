#ifndef STATISTICS_H
#define STATISTICS_H

#include "base/defs.h"
#include "base/maths.h"

namespace iso {

// collect statistics

template<typename T> struct stats0 {
	T		sum, sum2;
	stats0() : sum(0), sum2(0) {}
	void	add(T x)				{ sum += x; sum2 += x * x; }
	void	add(T x, int n)			{ sum += x * n; sum2 += x * x * n; }
	T		mean(int n)		const	{ return sum / n; }
	T		sigma2(int n)	const	{ return (sum2 - sum * sum / n) / (n - 1); }
	T		sigma(int n)	const	{ return sqrt(sigma2(n)); }
	T		rms(int n)		const	{ return sqrt(sum2 / n); }

	stats0&	operator+=(const stats0 &x) {
		sum		+= x.sum;
		sum2	+= x.sum2;
		return *this;
	}
};

template<typename T> struct stats : stats0<T> {
	int		n;
	stats() : n(0) {}
	void	add(T x)			{ stats0<T>::add(x); ++n; }
	void	add(T x, int _n)	{ stats0<T>::add(x, _n); n += _n; }
	T		mean()		const	{ return stats0<T>::mean(n); }
	T		sigma2()	const	{ return stats0<T>::sigma2(n); }
	T		sigma()		const	{ return stats0<T>::sigma(n); }
	T		rms()		const	{ return stats0<T>::rms(n); }

	stats&	operator+=(const stats &x) {
		stats0<T>::operator+=(x);
		n		+= x.n;
		return *this;
	}
};

template<int N, typename T> struct covariance_stats0 {
	T		sum[N], prod[N * N];

	void	add(const T (&x)[N]) {
		for (int i = 0; i < N; i++) {
			sum[i]	+= x[i];
			for (int j = 0; j < N; j++)
				prod[i * N + j] += x[i] * x[j];
		}
	}

	void	mean(T (&x)[N], int n) const {
		for (int i = 0; i < N; i++)
			x[i] = sum / n;
	}

	void	covariance(T (&c)[N * N], int n) const {
		for (int i = 0; i < N; i++) {
			for (int j = 0; j < N; j++)
				c[i * N + j] = (prod[i * N + j] - sum[i] * sum[j] / n) / n;
		}
	}
	covariance_stats0() { clear(*this); }

	covariance_stats0&	operator+=(const covariance_stats0 &x) {
		for (int i = 0; i < N; i++) {
			sum[i]	+= x.sum[i];
			for (int j = 0; j < N; j++)
				prod[i * N + j] += x.prod[i * N + j];
		}
		return *this;
	}
};

template<typename T> struct stats2 : stats<T>, interval<T> {
	stats2() : interval<T>(iso::maximum, iso::minimum) {}
	void	add(T x) {
		stats<T>::add(x);
		interval<T>::operator|=(x);
	}
};
template<typename C> C standardise(C &&range) {
	stats<typename container_traits<C>::element> s;
	for (auto &i : range)
		s.add(i);

	auto mean	= s.mean();
	auto rsigma	= reciprocal(s.sigma());
	for (auto &i : range)
		i = (i - mean) * rsigma;

	return range;
}

template<typename C> C standardise_rms(C &&range) {
	stats<typename container_traits<C>::element> s;
	for (auto &i : range)
		s.add(i);

	auto	rrms = reciprocal(s.rms());
	for (auto &i : range)
		i *= rrms;

	return range;
}

inline float phi_approx(float x) {
	return (1 + sign(x) * sqrt(1 - exp(-2 * x * x / pi))) * half;
}

inline float phi_approx2(float x) {
	float	phi = phi_approx(x);
	return	phi > .821f ?  1.0032f * pow( x, 1.0362f)
		:	phi < .179f ? -1.0032f * pow(-x, 1.0362f)
		:	phi;
}

inline float invphi_approx(float x) {
	return sqrt(ln(1 - square(x * 2 - 1)) * -pi * half) * sign(x - half);
}

} // namespace iso

#endif // STATISTICS_H
