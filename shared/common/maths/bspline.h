#ifndef BSPLINE_H
#define BSPLINE_H

#include "polynomial.h"
#include "utilities.h"
#include "base/algorithm.h"

namespace iso {

//float4x4 NURBS3GetKnotMatrix(float *k);
//float4x4 NURBS3GetControlMatrix(float4 *c);
//position3 NURBS3Evaluate(float *k, int n, float4 *cp, float t);
//position3 NURBS3Evaluate2D(float *ku, int nu, float *kv, int nv, float4 *cp, float2 uv);


template<int P> struct bspline_poly {
	static constexpr auto factor(float d) { return d == 0 ? 0 : 1 / d; }
	static constexpr auto f(const float *k) {
		return	polynomial<float, 1>(k[0], 1)		* bspline_poly<P - 1>::f(k)		* factor(k[P] - k[0])
			+	polynomial<float, 1>(k[P + 1], -1)	* bspline_poly<P - 1>::f(k + 1)	* factor(k[P + 1] - k[1]);
	}
};

template<> struct bspline_poly<0> {
	static constexpr auto f(const float *k) { return one; }
};

template<int P, int R> struct bspline_poly2 {
	static constexpr auto f(const float *k) {
		return	lerp(bspline_poly2<P, R - 1>::f(k - 1), bspline_poly2<P, R - 1>::f(k), polynomial<float, 1>(-k[0], 1) / (k[P + 1 - R] - k[0]));
	}
};

template<int P> struct bspline_poly2<P, 0> {
	static constexpr auto f(const float *k) { return polynomial<vec<float,P+1>, 0>(); }
};


struct bspline_knot {
	float	u	= 0; // The evaluated knot
	uint32	k	= 0; // The index [k, k+1
	uint32	s	= 0; // Multiplicity of u
};

template<typename T> struct deBoorNet : bspline_knot {
	dynamic_array<T>	points;
	deBoorNet(const bspline_knot &knot) : bspline_knot(knot) {}
	auto&	result()	const { return points.back(); }
};

//	d needs room for (n + 1) * (n + 2) / 2 entries
template<typename T> void deBoors(float u, T *d, const float *k, int n, int deg) {
	int	j = 0;
	int	o = n + 1;
	for (int r = 0; r < n; r++) {
		for (int i = r + 1; i <= n; i++, j++)
			d[o++]	= lerp(d[j], d[j + 1], (u - k[i]) / (k[i + deg - r] - k[i]));
		j++;
	}
}

//	d needs room for (n + 1) entries
template<typename T> void deBoorsCompact(float u, T *d, const float *k, int n, int deg) {
	for (int r = 0; r < n; r++) {
		for (int i = n; i > r; --i)
			d[i] = lerp(d[i - 1], d[i], (u - k[i]) / (k[i + deg - r] - k[i]));
	}
}

template<typename T, int N> struct bspline {
	dynamic_array<T>		c;		//control points
	dynamic_array<float>	k;		//knot positions

	bspline() {}
	bspline(int num_cp)				: c(num_cp), k(num_cp + N + 1) {}
	bspline(dynamic_array<T> &&_c)	: c(move(_c)), k(c.size() + N + 1) {}
	bspline(dynamic_array<T> &&_c, dynamic_array<float> &&_k) : c(move(_c)), k(move(_k)) {
		ISO_ASSERT(k.size() == c.size() + N + 1);
	}

	void			resize(int num_cp)			{ c.resize(num_cp); k.resize(num_cp + N + 1); }
	auto			get_box()			const	{ return get_extent(c); }
	interval<float> domain()			const	{ return {k[N], k.back(N)}; }
	bspline_knot	knot_info(float u)	const;
	deBoorNet<T>	evaluate2(float u)	const;
	bspline			insert_knot(const deBoorNet<T> &net, uint32 n) const;
	template<int E> bspline<T, N + E> elevate(float epsilon) const;

	T	evaluate(float u) const {
		//auto			ki	= upper_boundc(k.slice_to(-N), u) - N - 1;
		auto			ki	= u > k[0] ? lower_boundc(k, u) - N - 1 : k.begin();
		temp_array<T>	d	= make_range_n(c + k.index_of(ki), N + 1);
		deBoorsCompact<T>(u, d, ki, N, N);
		return d[N];
	}
};

template<typename T, int N> struct rational<bspline<T, N>> : bspline<T, N> {
	typedef bspline<T, N>	B;
	using B::B;
	auto	evaluate(float u) const { return project(B::evaluate(u)); }
};


template<typename T, int N> bspline_knot bspline<T, N>::knot_info(float u) const {
	u = domain().clamp(u);
	auto	ki	= lower_boundc(k, u);
	float	u1	= *ki;

	// Get multiplicity
	auto	m = ki + 1, end = min(m + N, k.end());
	while (m < end && approx_equal(u1, *m))
		++m;

	if (approx_equal(u, u1))
		return 	{u1, k.index_of(m - 1), m - ki};

	return 	{u, k.index_of(ki - 1), 1};
}

template<typename T, int N> deBoorNet<T> bspline<T, N>::evaluate2(float u) const {
	deBoorNet<T>	net(knot_info(u));

	// if multiplicity == order
	if (net.s == N + 1) {
		if (net.k == N)
			net.points = {c[0]};// only first
		else if (net.k == k.size() - 1)
			net.points = {c[net.k - net.s]}; // only the last
		else
			net.points = {c[net.k - net.s], c[net.k - net.s + 1]};

	} else {
		// use de boor algorithm to find point P(u)
		uint32	n	= N + 1 - net.s;
	#if 1
		net.points.resize((n + 1) * (n + 2) / 2);
		copy_n(c + net.k - N, net.points.begin(), n + 1);
		deBoors<T>(u, net.points, k + net.k - N, n, N);
	#else
		net.points = make_range_n(c + net.k - N, n + 1);
		deBoors<T>(u, net.points, k + net.k - N, n, N);
	#endif
	}
	return net;
}

template<typename T, int N> bspline<T, N> bspline<T, N>::insert_knot(const deBoorNet<T> &net, uint32 n) const {
	if (n == 0)
		return *this;

	const uint32	ki	= net.k;
	const uint32	nn	= N + 2 - net.s;
	ISO_ASSERT(n < nn);

	bspline<T, N>	result(c.size() + n);

	copy_n(c.begin(), result.c.begin(), ki - N);													// Copy left hand side control points
	copy_n(c + (ki - N + nn), result.c + (ki - N + nn + n), result.c.size() - n - (ki - N + nn));	// Copy right hand side control points
	copy_n(k.begin(), result.k.begin(), ki + 1);													// Copy left hand side knots
	copy_n(k + ki + 1, result.k + ki + 1 + n, result.k.size() - n - (ki + 1));						// Copy right hand side knots

	// Copy the relevant control points and knots from net
#if 1
	T*	from	= net.points;
	T*	to		= result.c + (ki - N);

	// Copy left hand side control points from net
	int	stride	= nn;
	for (uint32 i = 0; i < n; i++) {
		*to++	= *from;
		from	+= stride--;
	}
	// Copy middle part control points from net
	copy_n(from, to, nn - n);

	// Copy right hand side control points from net
	--from;
	to		+= nn - n;
	stride	= -int(nn - n + 1);
	for (uint32 i = 0; i < n; i++) {
		*to++	= *from;
		from	+= stride--;
	}
#else
	auto	to = result.c + (ki - N);
	copy_n(net.points + 0,		to,		nn);
	copy_n(net.points + nn - n,	to + nn,	n);
#endif
	// Copy knot from net
	fill_n(result.k + ki + 1, n, net.u);
	return result;
}

template<typename T, int N> bspline<T, N> split(const bspline<T, N> &spline, float knot, uint32* k) {
	auto	net		= spline.evaluate2(knot);
	auto	split	= spline.insert_knot(net, N - net.s + 1);
	*k = net.k + N - net.s + 1;
	return split;
}

template<typename T, int N> bspline<T, N> to_beziers(const bspline<T, N> &spline) {
	uint32		k;
	bspline<T, N>	tmp	= spline;
	auto		dom	= spline.domain();

	// Fix first control point if necessary
	if (!approx_equal(tmp.k[0], dom.a)) {
		tmp	= split(tmp, dom.a, &k);
		tmp.c.erase(tmp.c.slice_to(k - N));
		tmp.k.erase(tmp.k.slice_to(k - N));
	}

	// Fix last control point if necessary
	if (!approx_equal(tmp.k.back(), dom.b)) {
		tmp	= split(tmp, dom.b, &k);
		tmp.c.erase(tmp.c.slice(k));
		tmp.k.erase(tmp.k.slice(k));
	}

	// Split internal knots
	k = N + 1;
	while (k + N + 1 < tmp.k.size()) {
		tmp	= split(tmp, tmp.k[k], &k);
		k++;
	}

	return tmp;
}

template<typename T, int N> bspline<T, N - 1> deriv(const bspline<T, N> &spline, float epsilon = 0) {
	temp_array<T>		c(spline.c.size() - 1);
	temp_array<float>	k(spline.k.size() - 2);
	T					*dc = c;
	float				*dk = k;

#if 1
	for (uint32 i = 1; i < N + 1; i++)
		*dk++ = spline.k[i];

	size_t		num_c	= spline.c.size();
	T			prev	= spline.c[0];

	for (uint32 i = 1; i < num_c; i++) {
		// Check and, if possible, fix discontinuity
		if (i > N && i < num_c - N && approx_equal(spline.k[i], spline.k[i + N])) {
			ISO_ASSERT(epsilon <= 0 || len(spline.c[i - 1] - spline.c[i]) < epsilon);
			//i += N;
		} else {
			float	k0		= spline.k[i + N];
			float	span	= max(k0 - dk[-N], 1e-4);

			T next	= spline.c[i];
			*dc++	= (next - prev) * N / span;
			*dk++	= k0;
			prev	= next;
		}
	}
	num_c = dc - c;

#else
	for (uint32 i = 1; i < N; i++)
		*dk++ = spline.k[i];

	// Check and, if possible, fix discontinuity
	size_t		num_c	= spline.c.size();
	for (uint32 i = 0; i < num_c; i++) {
		if (i > N && i < num_c - N && approx_equal(spline.k[i], spline.k[i + N])) {
			ISO_ASSERT(epsilon == 0 || len(spline.c[i - 1] - spline.c[i]) < epsilon);
			//i += N;
		} else {
			*dc++ = spline.c[i];
			*dk++ = spline.k[i + N];
		}
	}

	// Derive continuous worker
	num_c = dc - c - 1;
	for (uint32 i = 0; i < num_c; i++) {
		float	span	= max(k[i + N] - k[i], 1e-4);
		c[i]	= (c[i + 1] - c[i]) * N / span;
	}
#endif
	return {c.slice_to(num_c), k.slice_to(dk)};
}

template<typename T> bool is_closed(const bspline<T, 0> &spline, float epsilon) {
	return true;
}

template<typename T, int N> bool is_closed(const bspline<T, N> &spline, float epsilon) {
	auto	dom		= spline.domain();
	return approx_equal(spline.evaluate(dom.a), spline.evaluate(dom.b), epsilon)
		&& is_closed(deriv(spline, 0), epsilon);
}

template<typename T, int N> bspline<T, N> sub_spline(const bspline<T, N> &spline, float knot0, float knot1) {
	bool	reverse = knot0 > knot1;
	if (reverse)
		swap(knot0, knot1);

	auto			dom	= spline.domain();
	bspline<T, N>	worker;
	uint32			k0, k1;

	if (!approx_equal(knot0, dom.a)) {
		worker	= split(spline, knot0, &k0);
	} else {
		worker	= spline;
		k0		= N;
	}

	if (!approx_equal(knot1, dom.b)) {
		worker	= split(worker, knot1, &k1);
	} else {
		k1		= worker.k.size() - 1;
	}

	uint32	c0 = k0 - N;
	uint32	nc = k1 - k0;

	copy_n(worker.c + c0, worker.c, nc);
	copy_n(worker.k  + c0, worker.k, nc + N + 1);
	worker.resize(nc);

	if (reverse)
		iso::reverse(worker.c);

	return worker;
}

template<typename T, int N> template<int E> bspline<T, N + E> bspline<T, N>::elevate(float epsilon) const {
	bspline<T, N>		bezier		= to_beziers(*this);
	auto				num_beziers = bezier.c.size() / (N + 1);

	temp_array<T>		c((num_beziers + 1) * (N + E + 1));
	temp_array<float>	k((num_beziers + 2) * (N + E + 1));

	// Move all but the first bezier curve to their new location in the control point array so that the additional control points can be inserted without overwriting the others.
	for (uint32 i = num_beziers - 1; i > 0; i--) {
		copy_n(c + i * (N + 1), c + i * (N + E + 1), N + 1);
		copy_n(k + i * (N + 1), k + i * (N + E + 1), N + 1);
	}

	// The following formulas are based on: https://pages.mtu.edu/~shene/COURSES/cs3621/NOTES/spline/Bezier/bezier-elev.html
	for (uint32 order = N + 1; a < N + E + 1; a++) {
		for (uint32 i = 0; i < num_beziers; i++) {
			uint32	offset = i * (N + E + 1);
			c[offset + order]	= c[offset + order - 1];	// Duplicate last control point to the new end position (next control point)
			k[offset + order]	= k[offset];				// Duplicate knot

			// All but the outer control points must be recalculated
			for (uint32 c = order - 1; c > 0; c--) {
				uint32	idx	= offset + c;
				c[idx]	= lerp(c[idx], c[idx - 1], (float)c / order);
			}
		}
	}

	// Combine bezier curves
	T		*dc	= c, *sc = dc;
	float	*dk	= k, *sk = dk;
	for (uint32	i = 0; i < num_beziers - 1; i++) {
		for (int j = 0; j < N + E; j++) {
			*dc++ = *sc++;
			*dk++ = *sk++;
		}

		// Is the last control point of bezier curve i equal to the first control point of bezier curve i+1?
		if (len(s[0] - s[1]) > epsilon) {
			*dc++ = *sc;
			*dk++ = *sk;
		}

		sc++;
		sk++;
	}
	for (int j = 0; j < N + E; j++)
		*dk++ = *sk;

	return {c.slice_to(dc), k.slice_to(dk)};
}


} // namespace iso

#endif
