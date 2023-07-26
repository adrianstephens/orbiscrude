#include "bezier.h"
#include "base/strings.h"

namespace iso {

//-----------------------------------------------------------------------------
//	new bezier_spline
//-----------------------------------------------------------------------------

template<> float3x3	bezier_helpers<2>::blend(
	float3{ 1,  0,  0},	//1
	float3{-2,  2,  0},	//t
	float3{ 1, -2,  1} 	//t^2
);
template<> float3x2	bezier_helpers<2>::tangentblend(
	float3{-2,  2,  0},	//1
	float3{ 2, -4,  2} 	//t
);

template<> float4x4	bezier_helpers<3>::blend(
	float4{ 1,  0,  0,  0},	//1
	float4{-3,  3,  0,  0},	//t
	float4{ 3, -6,  3,  0},	//t^2
	float4{-1,  3, -3,  1}	//t^3
);

template<> mat<float, 4,3>	bezier_helpers<3>::tangentblend(
	float4{-3,  3,  0,  0},	//1
	float4{ 6,-12,  6,  0},	//t
	float4{-3,  9, -9,  3} 	//t^2
);

float2 *reduce_simple(const bezier_splineT<float2,3> &b3, float2 *p, float2 *end, float tol2) {
	bezier_splineT<float2, 2>	b2;
	
	if (approx_equal(b3[0], b3[1])) {
		b2 = {b3[0], b3[2], b3[3]};

	} else if (approx_equal(b3[2], b3[3])) {
		b2 = {b3[0], b3[1], b3[3]};

	} else {
		line2	t0	= {position2(b3[0]), position2(b3[1])};
		line2	t1	= {position2(b3[3]), position2(b3[2])};

		if (colinear(t0.dir(), -t1.dir())) {
			b2 = {b3[0], (b3[0] + b3[3]) * half, b3[3]};

		} else {
			b2 = {b3[0], t0 & t1, b3[3]};
		}
	}

	if (p + 2 * 2 < end) {
		//auto	p3		= b3.evaluate(half);
		auto	p2		= b2.evaluate(half);

		auto	s		= deriv(len2(b3.spline() - p2));
		auto	root	= refine_roots(s, 0.5f, 2);
		auto	p3		= b3.evaluate(root);

		if (len2(p3 - p2) > tol2) {
			auto	chain	= split(b3);
			p	= reduce_simple(chain[0], p, p + (end - p) / 2, tol2);
			return reduce_simple(chain[1], p, end, tol2);
		}
	}
	*p++ = b2[1];
	*p++ = b2[2];
	return p;
}

float2 *reduce_check(const bezier_splineT<float2,3> &b3, float2 *p, float2 *end, float tol2) {
	if (p + 2 * 2 < end) {
		float2	t0	= b3.tangent(zero);
		auto	s	= b3.tangent_spline();
		auto	s2	= dot(s, t0);
		float2	roots;
		int		n	= s2.roots(roots);
		if (n > 0) {
			float	root = n > 1 && roots.x < zero ? roots.y : roots.x;
			if (root > 0 && root < 0.9f) {
				auto	chain	= split(b3, root);
				p	= reduce_simple(chain[0], p, p + (end - p) / 2, tol2);
				return reduce_check(chain[1], p, end, tol2);
			}
		}
	}
	return reduce_simple(b3, p, end, tol2);
}

template<> float2 *reduce_spline<float2, 3>(const bezier_splineT<float2, 3> &b, float2 *p, float2 *end, float tol) {
	auto	t = inflection(b);
	if (t.x > 0.05f && t.x < 0.95f) {
		auto	chain = split(b, t.x);

		if (t.y < 0.95f) {
			p		= reduce_check(chain[0], p, p + (end - p) / 3, square(tol));
			chain	= split(chain[1], (t.y - t.x) / (1 - t.x));
			p		= reduce_check(chain[0], p, p + (end - p) / 2, square(tol));
			return reduce_check(chain[1], p, end, square(tol));

		} else {
			p		= reduce_check(chain[0], p, p + (end - p) / 2, square(tol));
			return reduce_check(chain[1], p, end, square(tol));
		}

	} else if (t.y > 0.05f && t.y < 0.95f) {
		auto	chain = split(b, t.y);
		p	= reduce_check(chain[0], p, p + (end - p) / 2, square(tol));
		return reduce_check(chain[1], p, end, square(tol));
	}
	return reduce_check(b, p, end, square(tol));
}

/*
auto fix_line(position2 a, position2 b, position2 c) {
	line<float,2>	x(a, b);
	auto	t = x.unnormalised_dist(c);
	return line<float,2>(as_vec(x) / t);
}

void ConicBezier::axes(line2 &axis1, line2 &axis2) const {
	line<float,2>	lp = fix_line(P, Q, R), lq = fix_line(R, P, Q), lr = fix_line(Q, R, P);
	float	p		= len2(Q - P), q = len2(R - P), r = len2(Q - R);
	float	w2		= square(w);

	if (p == r) {
		axis1	= line2(as_vec(lp) - as_vec(lr));
		axis2	= line2(w2 * (as_vec(lp) + as_vec(lr)) + as_vec(lq));

	} else {
		auto	poly	= make_polynomial(1.f, (w2 * 2 * (r + p) - q) / (p - r),  w2 * (w2 - 1));
		float2	rho;
		int		n		= poly.roots(rho);
		axis1	= line2((w2 * rho.x + 1) * as_vec(lp) + (w2 * rho.x - 1) * as_vec(lr) + rho.x * as_vec(lq));
		axis2	= line2((w2 * rho.y + 1) * as_vec(lp) + (w2 * rho.y - 1) * as_vec(lr) + rho.y * as_vec(lq));
	}
}
*/
void ConicBezier::axes(line2 &axis1, line2 &axis2) const {
	float3	a1, a2;
	parametric_axes(a1, a2);

	auto		m	= cofactors(*this);
	axis1	= line2(m * position2(a1));
	axis2	= line2(m * position2(a2));
}

void ConicBezier::parametric_axes(float3 &axis1, float3 &axis2) const {
	float	p		= len2(x), q = len2(y), r = len2(x - y);
	float	w2		= square(w);

	if (p == r) {
		axis1	= float3{1, 2, -1};
		axis2	= float3{1 - w2, 0, w2};

	} else {
		auto	poly	= make_polynomial(1.f, (w2 * 2 * (r + p) - q) / (p - r),  w2 * (w2 - 1));
		float2	rho;
		int		n		= poly.roots(rho);

		axis1	= float3{(1 - w2) * rho.x + 1, 2, w2 * rho.x - 1};
		axis2	= float3{(1 - w2) * rho.y + 1, 2, w2 * rho.y - 1};
	}
}


template<> float rational<bezier_splineT<float3, 2>>::support_param(V v) const {
	auto	s	= spline();
	auto	s1	= deriv(s);

	auto	f	= maski<3>(s);
	auto	f1	= maski<3>(s1);
	auto	w	= maski<4>(s);
	auto	w1	= maski<4>(s1);
	
	return maxima_from_deriv(dot(f1 * w - f * w1, v), zero, one);
}

template<> position2 rational<bezier_splineT<float3, 2>>::closest(position2 q, float* _t, float* _d) const {
	auto	s	= spline();
	auto	s1	= deriv(s);

	auto	f	= maski<3>(s);
	auto	f1	= maski<3>(s1);
	auto	w	= maski<4>(s);
	auto	w1	= maski<4>(s1);

	auto	wq		= make_polynomial(outer_product(q.v, w.c));
	auto	poly	= dot(f - wq, f1 * w - f * w1);

	vec<float,5>	roots;
	int		num_roots		= poly.roots(roots);

	if (auto mask = bit_mask(roots >= zero & roots <= one) & bits(num_roots)) {
		float	t	= roots[lowest_set_index(mask)];
		auto	v	= position2(s(t));
		float	d	= len2(v - q);

		while (mask = clear_lowest(mask)) {
			float	t1	= roots[lowest_set_index(mask)];
			auto	v1	= position2(s(t1));
			float	d1	= len2(v1 - q);
			if (d1 < d) {
				v	= v1;
				d	= d1;
				t	= t1;
			}
		}
		if (_t)
			*_t = t;
		if (_d)
			*_d = sqrt(d);
		return v;
	}
	if (_d)
		*_d = maximum;
	return {nan, nan};
}

template<> bool rational<bezier_splineT<float3, 2>>::ray_check(param(ray2) r, float &t, vector2 *normal) const {
	ConicBezier	conb(c[0], c[1], c[2]);
	conic	con		= conb;
	auto	tcon	= con / r.matrix();
	auto	roots	= tcon & y_axis;

	t = roots.a < 0 ? roots.b : roots.a;
	
	auto	pos	= r.from_parametric(t);
	if (!conb.contains(pos))
		return false;

	if (normal)
		*normal = con.tangent(pos).normal();
	return true;
}

//-----------------------------------------------------------------------------
//	ArcParams
//-----------------------------------------------------------------------------

// 2 off-curve
ArcParams::ArcParams(float2 p0, float2 p1, float2 p2, float2 p3) {
	float2x2	m0	= {p3 - p0, p1 - p0};		//chord, incoming direction
	auto		d1t	= float2(p2 - p3) / m0;		//outgoing direction
	float		s	= -d1t.x / (2 * d1t.y);		//shear to make d1, d2 reflections of each other
	float		r	= sqrt(1 + square(s)) / 2;	//radius
	float2x2	mi	= m0 * float2x2(float2{1, 0}, float2{-s, 1});
	float2		sc	= max_circle_point(mi);

	float2		axis1	= mi * sc;
	float2		axis2	= mi * perp(sc);

	angle		= -atan2(axis1);
	radii		= float2{len(axis2), len(axis1)} * r;
	clockwise	= m0.det() < zero;
	big			= s < 0;
}

// 1 off-curve
ArcParams::ArcParams(float2 p0, float2 p1, float2 p2) : angle(0) {
	float2	y	= p2 - p0;	//chord
	float2	d	= p1 - p0;	//incoming direction
	auto	x	= cross(y, d);

	radii		= dot(y, y) / (abs(x) * rlen(d) * two);
	clockwise	= x < zero;
	big			= dot(y, d) < zero;
}

void ArcParams::fix_radii(float2 p0, float2 p1) {
	float2		d1	= ((p1 - p0) / 2) / matrix();
	float		d2	= len2(d1);
	if (d2 > 1)
		radii *= sqrt(d2);
}

shear_ellipse ArcParams::ellipse(float2 p0, float2 p1) const {
	float2x2	m	= matrix();
	float2		d1	= ((p1 - p0) / 2) / m;
	float		d2	= len2(d1);
	float2		mid	= (p0 + p1) / 2;

	if (d2 > 1) {
		m = scale(sqrt(d2)) * m;
		return shear_ellipse(float2x3(m, mid));
	}
	float	y	= plus_minus(sqrt(1 - d2), !big ^ clockwise);
	return shear_ellipse(float2x3(m, mid + m * (perp(d1 * rsqrt(d2)) * y)));
}

float2x2 ArcParams::control_points(float2 p0, float2 p1) const {
	auto	e	= ellipse(p0, p1);
	auto	c0	= position2(p0) / e.matrix();
	auto	c1	= position2(p1) / e.matrix();
	c0	-= plus_minus(perp(c0.v), clockwise) * 2;
	c1	+= plus_minus(perp(c1.v), clockwise) * 2;
	return {e.matrix() * c0, e.matrix() * c1};
}

//-----------------------------------------------------------------------------
//	tests
//-----------------------------------------------------------------------------
/*
static struct test {
	test() {

	}
} _test;
*/
}	//namespace iso
