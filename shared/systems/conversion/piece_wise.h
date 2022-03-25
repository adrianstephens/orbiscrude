#ifndef PIECE_WISE_H
#define PIECE_WISE_H

#include "base/array.h"

namespace iso {

inline float len(float x)	{ return abs(x); }
template<typename T> dynamic_array<pair<T, float> > &Optimise(dynamic_array<pair<T, float> > &a, float eps) {
	float	t0;
	T		v0;
	int		n2	= 0;
	for (int i = 1, i0 = 0, n = int(a.size()); i <= n; i++) {
		bool	output	= i == 1 || i == n;
		if (!output) {
			T		v1		= a[i].a;
			float	t1		= a[i].b;
			T		slope	= (v1 - v0) / (t1 - t0);
			for (int k = i0 + 1; k < i; k++) {
				if (output = len(a[k].a - v0 - slope * (a[k].b - t0)) > eps)
					break;
			}
		}

		if (output) {
			if (i == n && n2 == 1 && len(a[n - 1].a - v0) <= eps)
				continue;
			i0		= i - 1;
			a[n2].a = v0 = a[i0].a;
			a[n2].b = t0 = a[i0].b;
			n2++;
		}
	}

	a.resize(n2);
	return a;
}

template<typename T> float dist(const T &a, const T &b) {
	return b - a;
}
template<typename T> float dist2(const T &a, const T &b) {
	return square(dist(a, b));
}
template<typename T> float line_length(const pair<T, float> &a, const pair<T, float> &b) {
	return sqrt(square(b.b - a.b) + dist2(a.a, b.a));
}
template<typename T> float line_dist(const pair<T, float> &a, const pair<T, float> &b, const pair<T, float> &t) {
	return (a.a - t.a) * dist(t.b, b.b) - (b.a - t.a) * dist(t.b, a.b);
}

template<typename T> T line_slope(const pair<T, float> &a, const pair<T, float> &b) {
	return (b.a - a.a) / (b.b - a.b);
}

inline float line_length(param(position2) a, param(position2) b) {
	return len(b - a);
}
inline float line_dist(param(position2) a, param(position2) b, param(position2) t) {
	return cross(a - t, b - t);
}


template<typename T> T *simplify_forward(T *d, T *p, int n, float eps) {
	T	*s	= p;
	*d++ = *p++;

	for (T *e = p + n; p != e; ++p) {
		bool	over = false;
		for (T *k = s + 1; k != p; k++) {
			if (over = line_dist(*s, *p, *k) > eps)
				break;
		}

		if (over) {
			s		= p - 1;
			*d++	= *s;
		}
	}
	*d++ = *p;

	return d;
}


template<typename T> T *simplify_rdp(T *d, const T *p, int n, float eps) {
	int stack[32], *sp = stack;

	for (;;) {
		// Find the point with the maximum distance
		float		dmax	= 0;
		int			imax	= 0;
		T			a		= p[0], b = p[n - 1];

		for (int i = 1; i < n - 2; i++) {
			T			t	= p[i];
			float		d	= abs(line_dist(a, b, t));
			if (d > dmax) {
				imax	= i;
				dmax	= d;
			}
		}

		// If max distance is greater than epsilon, recursively simplify
		if (dmax > eps * line_length(a, b)) {
			*sp++	= n - imax;
			n		= imax + 1;

		} else {
			*d++ = p[0];
			if (sp == stack)
				return d;
			p += n - 1;
			n = *--sp;
		}
	}
	*d++ = p[n - 1];
	return d;
}

template<typename T> float Extent(dynamic_array<pair<T, float> > &a) {
	float	d	= 0;
	T		v0	= a[0].a;
	for (size_t i = 1, n = a.size(); i < n; i++)
		d = max(d, len(a[i].a - v0));
	return d;
}

}	// namespace iso

#endif	// PIECE_WISE_H

