#include "spherical_harmonics.h"

namespace iso {

double factorial(int n) {
	static struct Factorials {
		double table[100];
		Factorials() {
			table[0] = 1;
			for (int i = 1; i < 100; i++)
				table[i] = table[i - 1] * i;
		}
	} factorials;
	return factorials.table[n];
}

namespace sh {

#ifndef REFACTOR
float3 uvw(int l, int m, int n) {
	int	a	= abs(m);
	return sqrt(
		float3{
			float((l + m)			* (l - m)),
			float((l + a - 1)		* (l + a)),
			float(a ? (l - a - 1)	* (l - a) : 0.f)
		} / (abs(n) == l ? (2 * l) * (2 * l - 1) : (l + n) * (l - n))
	) * float3{1, m == 0 ? -sqrt2 * 0.5f : 0.5f, -0.5f};
}
#endif

double P(int l, int m, double x) {
	double pmm	= 1;
	double fact	= 1;
	if (m > 0) {
		double omx2	= (1 - x) * (1 + x);
		for (int n = m / 2; n--;) {
			pmm	 *= fact * (fact + 2) * omx2;
			fact += 4;
		}
		if (m & 1) {
			pmm *= -fact * sqrt(omx2);
			fact += 2;
		}
	}

	if (l == m)
		return pmm;

	double pmmp1 = x * fact * pmm;
	for (int ll = m + 2; ll <= l; ++ll) {
		fact		+= 2;
		double pll	= (fact * x * pmmp1 - (ll + m - 1) * pmm) / (ll - m);
		pmm			= pmmp1;
		pmmp1		= pll;
	}
	return pmmp1;
}

double K(int l, int m) {
	return sqrt(((2 * l + 1) * factorial(l - m)) / (four * pi * factorial(l + m)));
}

double SH(int l, int m, double theta, double phi) {
	const double sqrt2 = sqrt(2.0);
	double	x = cos(theta);
	return m == 0
		? P(l, 0, x)
		: sqrt2 * (m > 0
			? cos( m * phi) * P(l,  m, x)
			: sin(-m * phi) * P(l, -m, x)
		);
}

// hemispherical harmonics

double HK(int l, int m) {
	return sqrt(((2 * l + 1) * factorial(l - m)) / (two * pi * factorial(l + m)));
}

double HSH(int l, int m, double theta, double phi) {
	const double sqrt2 = sqrt(2.0);
	double	x = max(cos(theta), 0.0) * 2 - 1;
	return m == 0
		? P(l, 0, x)
		: sqrt2 * (m > 0
			? cos( m * phi) * P(l,  m, x)
			: sin(-m * phi) * P(l, -m, x)
		);
}

} }// namespace iso::sh
