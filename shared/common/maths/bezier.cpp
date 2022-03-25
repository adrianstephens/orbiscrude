#include "bezier.h"
#include "polynomial.h"
#include "polygon.h"

namespace iso {

//-----------------------------------------------------------------------------
//	bezier_spline
//-----------------------------------------------------------------------------

float4x4	bezier_spline::blend(
	float4{-1,  3, -3,  1},
	float4{ 3, -6,  3,  0},
	float4{-3,  3,  0,  0},
	float4{ 1,  0,  0,  0}
);
float4x4	bezier_spline::tangentblend(
	float4{ 0,  0,  0,  0},
	float4{-3,  9, -9,  3},
	float4{ 6,-12,  6,  0},
	float4{-3,  3,  0,  0}
);
float4x4	bezier_spline::normalblend(
	float4{ 0,  0,  0,  0},
	float4{ 0,  0,  0,  0},
	float4{-6, 18,-18,  6},
	float4{ 6,-12,  6,  0}
);

void bezier_spline::split(float _t, bezier_spline &bl, bezier_spline &br) const {
	float	t = _t;
	float	u = one - t;
	float4	h	= c1  * u + c2  * t,
			cl1 = c0  * u + c1  * t,
			cr2 = c3  * t + c2  * u,
			cl2 = cl1 * u + h   * t,
			cr1 = cr2 * t + h   * u,
			cs	= cr1 * t + cl2 * u;
	bl.c0	= c0;
	br.c3	= c3;

	bl.c1	= cl1;
	bl.c2	= cl2;
	bl.c3	= cs;

	br.c0	= cs;
	br.c1	= cr1;
	br.c2	= cr2;
}

bezier_spline bezier_spline::split_left(float _t) const {
	float	t = _t;
	float	u = one - t;
	float4	h	= c1  * u + c2  * t,
			cl1 = c0  * u + c1  * t,
			cr2 = c3  * t + c2  * u,
			cl2 = cl1 * u + h   * t,
			cr1 = cr2 * t + h   * u,
			cs	= cr1 * t + cl2 * u;
	return bezier_spline(c0, cl1, cl2, cs);
}

bezier_spline bezier_spline::split_right(float _t) const {
	float	t = _t;
	float	u = one - t;
	float4	h	= c1  * u + c2  * t,
			cl1 = c0  * u + c1  * t,
			cr2 = c3  * t + c2  * u,
			cl2 = cl1 * u + h   * t,
			cr1 = cr2 * t + h   * u,
			cs	= cr1 * t + cl2 * u;
	return bezier_spline(cs, cr1, cr2, c3);
}

void bezier_spline::split(bezier_spline &bl, bezier_spline &br) const {
	float4	h	= (c1  + c2) * half,
			cl1 = (c0  + c1) * half,
			cr2 = (c3  + c2) * half,
			cl2 = (cl1 + h) * half,
			cr1 = (cr2 + h) * half,
			cs	= (cr1 + cl2) * half;

	bl.c0	= c0;
	br.c3	= c3;

	bl.c1	= cl1;
	bl.c2	= cl2;
	bl.c3	= cs;

	br.c0	= cs;
	br.c1	= cr1;
	br.c2	= cr2;
}

bezier_spline bezier_spline::split_left() const {
	float4	h	= (c1  + c2) * half,
			cl1 = (c0  + c1) * half,
			cr2 = (c3  + c2) * half,
			cl2 = (cl1 + h) * half,
			cr1 = (cr2 + h) * half,
			cs	= (cr1 + cl2) * half;
	return bezier_spline(c0, cl1, cl2, cs);
}

bezier_spline bezier_spline::split_right() const {
	float4	h	= (c1  + c2) * half,
			cl1 = (c0  + c1) * half,
			cr2 = (c3  + c2) * half,
			cl2 = (cl1 + h) * half,
			cr1 = (cr2 + h) * half,
			cs	= (cr1 + cl2) * half;
	return bezier_spline(cs, cr1, cr2, c3);
}

bezier_spline bezier_spline::flip() const {
	return bezier_spline(c3, c2, c1, c0);
}

bezier_spline bezier_spline::middle(float t0, float t1) const {
	float4	c0 = evaluate(t0);
	float4	c3 = evaluate(t1);
	return bezier_spline(
		c0,
		c0 + tangent(t0) * ((t1 - t0) / 3),
		c3 - tangent(t1) * ((t1 - t0) / 3),
		c3
	);
}

//Solve a cubic equation to get the intersection count of a ray from the current point to infinity and parallel to the x axis
//Also compute the intersection count with the tangent in the end points of the curve

bool bezier_spline::side_2d(param(ray2) r) const {
	float4x4	a = float3x4(r.inv_matrix()) * float4x4(c0, c1, c2, c3) * blend;
	polynomial<float,3>	x	= a.row(0).wzyx;
	polynomial<float,3>	y	= a.row(1).wzyx;

	float3	roots;
	int		num_roots	= x.roots(roots);
	int		num_ints	= 0;

	for (int i = 0; i < num_roots; i++) {
		float	r = roots[i];
		if (y.eval(r) >= zero)
			num_ints++;
	}

	return num_ints & 1;
}

float4 bezier_spline::closest_point(param(position2) pos, float *_t, float *_d) const {
	float4x4	a = float4x4(c0, c1, c2, c3) * blend;
	a.w.xy	-= pos.v;

	polynomial<float,3>	x	= a.row(0).wzyx;
	polynomial<float,3>	y	= a.row(1).wzyx;

	vec<float,5>	roots;
	int		num_roots = deriv(x * x + y * y).roots(roots);

	//find closest root
	roots = clamp(roots, zero, one);

	float	t	= roots.x;
	float4	v	= evaluate(roots.x);
	float	d	= len2(pos - v.xy);

	if (num_roots > 1) {
		float4	v1	= evaluate(roots.y);
		float	d1 = len2(pos - v1.xy);
		if (d1 < d) {
			v	= v1;
			d	= d1;
			t	= roots.y;
		}
		if (num_roots > 2) {
			float4	v1	= evaluate(roots.z);
			float	d1 = len2(pos - v1.xy);
			if (d1 < d) {
				v	= v1;
				d	= d1;
				t	= roots.z;
			}
		}
	}

	if (_t)
		*_t = t;
	if (_d)
		*_d = sqrt(d);
	return v;
}

float4 bezier_spline::closest_point(param(position3) pos, float *_t, float *_d) const {
#if 0
	float3	a3	= c3.xyz - c0.xyz + (c1.xyz - c2.xyz) * 3;
	float3	a2	= (c0.xyz + c2.xyz) * 3 - c1.xyz * 6;
	float3	a1	= (c1.xyz - c0.xyz) * 3;
	float3	a0	= c0.xyz - pos.v;

	//compute polynomial describing distance to pos dependent on a parameter t
	float	bc6 = dot(a3, a3);
	float	bc5 = dot(a3, a2) * two;
	float	bc4 = dot(a2, a2) + dot(a1, a3) * two;
	float	bc3 = (dot(a1, a2) + dot(a0, a3)) * two;
	float	bc2 = dot(a1, a1) + dot(a0, a2) * two;
	float	bc1 = dot(a0, a1) * two;

	//get roots of derivative of this polynomial
	vec<float,5>	roots;
	int		num_roots = polynomial<float, 5>(vec<float,6>{bc1, bc2 * two, 3 * bc3, 4 * bc4, 5 * bc5, 6 * bc6}).roots(roots);

#else
	float4x4	a = float4x4(c0, c1, c2, c3) * blend;
	a.w.xyz	-= pos.v;

	polynomial<float,3>	x	= a.row(0).wzyx;
	polynomial<float,3>	y	= a.row(1).wzyx;
	polynomial<float,3>	z	= a.row(2).wzyx;

	vec<float,5>	roots;
	int		num_roots = deriv(x * x + y * y + z * z).roots(roots);

#endif
	//find closest root
	roots = clamp(roots, zero, one);

	float	t	= roots.x;
	float4	v	= evaluate(roots.x);
	float	d	= len2(pos - v.xyz);

	if (num_roots > 1) {
		float4	v1	= evaluate(roots.y);
		float	d1 = len2(pos - v1.xyz);
		if (d1 < d) {
			v	= v1;
			d	= d1;
			t	= roots.y;
		}
		if (num_roots > 2) {
			float4	v1	= evaluate(roots.z);
			float	d1 = len2(pos - v1.xyz);
			if (d1 < d) {
				v	= v1;
				d	= d1;
				t	= roots.z;
			}
		}
	}

	if (_t)
		*_t = t;
	if (_d)
		*_d = sqrt(d);
	return v;
}

float4 bezier_spline::closest_point(param(ray3) ray, float *_t, float *_d) const {
	float			t;
	bezier_spline	b2	= ray.inv_matrix() * *this;
	b2.closest_point(position2(zero), &t, _d);
	if (_t)
		*_t = t;
	return evaluate(t);
}


float len(param(bezier_spline) b, int level) {
	if (level) {
		bezier_spline bl, br;
		b.split(bl, br);
		return len(bl, level - 1) + len(br, level - 1);
	}
	return (len((b.c1.xyz - b.c0.xyz)) + len((b.c2.xyz - b.c1.xyz)) + len((b.c3.xyz - b.c2.xyz)) + len((b.c3.xyz - b.c0.xyz))) * half;
}


float len_to(param(bezier_spline) b, float t) {
	// Evaluate the length of a Hermite spline segment.
	// This calculates the integral of |dP/dt| dt, where P(t) is the spline equation with components (x(t), y(t), z(t)).
	// This isn't solvable analytically, so we use a numerical method (Legendre-Gauss quadrature) which performs very well
	// with functions of this type, even with very few samples.  In this case, just 5 samples is sufficient to yield a
	// reasonable result.

	static const float2 LegendreGaussCoefficients[] = {
		{ (0.0f			+ 1) / 2,	0.5688889f  / 2},
		{ (-0.5384693f	+ 1) / 2,	0.47862867f / 2},
		{ (0.5384693f	+ 1) / 2,	0.47862867f / 2},
		{ (-0.90617985f	+ 1) / 2,	0.23692688f / 2},
		{ (0.90617985f	+ 1) / 2,	0.23692688f / 2}
	};

	const auto& StartPoint = b.c0;
	const auto& EndPoint = b.c3;

	auto P0 = b.c0;
	auto T0 = b.c1 - b.c0;
	auto P1 = b.c3;
	auto T1 = b.c3 - b.c2;

	// Cache the coefficients to be fed into the function to calculate the spline derivative at each sample point as they are constant.
	float4x4	m = float4x4(b.c0, b.c1, b.c2, b.c3) * bezier_spline::tangentblend;
	auto Coeff1 = ((P0 - P1) * 2 + T0 + T1) * 3;
	auto Coeff2 = (P1 - P0) * 6 - T0 * 4 - T1 * 2;
	auto Coeff3 = T0;

	// Calculate derivative at each Legendre-Gauss sample, and perform a weighted sum
	float total = zero;
	for (auto i : LegendreGaussCoefficients)
		total	+= len(horner(i.x, Coeff3, Coeff2, Coeff1)) * i.y;
	return total * t;
}

#if 0
static bool root_2d(param(bezier_spline) b, float &root, param(float) epsilon = 1e-3f) {
	// check
	float4 c(b.c0.y, b.c1.y, b.c2.y, b.c3.y);
	if (all(c > zero) || all(c < zero))
		return false;
	// iterate
	float t = half;
	for (int i = 0; i < 16; ++i) {
		cubic_param	t2(t);
		float2 v = b.evaluate(t2).xy;
		if (abs(v.y) < epsilon) {
			root = v.x;
			return true;
		}
		t = t - v.y / b.tangent(t2).y;
	}
	return false;
}

static bool ray_contains(param(ray2) r, param(position2) p) {
	float t = dot(p - r.p(), r.d());
	return t >= zero && t <= len2(r.d());
}

bool intersect(param(bezier_spline) b, param(ray2) r, float4 *_p, float *_t) {
	// hull check
#if 1
	float4 x	= float4(b.c0.x, b.c1.x, b.c2.x, b.c3.x) - r.pt0().x;
	float4 y	= float4(b.c0.y, b.c1.y, b.c2.y, b.c3.y) - r.pt0().y;

	float4 range	= x * r.dir().x + y * r.dir().y;
	if (all(range < zero) || all(range > len2(r.d())))
		return false;

	float2 n		= normalise(perp(r.d()));
	float4 dist		= x * n.x + y * n.y;
	if (all(dist > zero) || all(dist < zero))
		return false;
#else
	float4 range = float4(
		dot(b.c0.xy - r.p(), r.d()),
		dot(b.c1.xy - r.p(), r.d()),
		dot(b.c2.xy - r.p(), r.d()),
		dot(b.c3.xy - r.p(), r.d())
	) * reciprocal(len2(r.d()));
	if (all(range < zero) || all(range > one))
		return false;

	float2 n = normalise(float2(-r.d().y, r.d().x));
	float4 dist(
		dot(b.c0.xy - r.p(), n),
		dot(b.c1.xy - r.p(), n),
		dot(b.c2.xy - r.p(), n),
		dot(b.c3.xy - r.p(), n)
	);
	if (all(dist > zero) || all(dist < zero))
		return false;
#endif
	// line distance parametrization
	bezier_spline db(
		float3(zero, dist.x, zero),
		float3(1.0f / 3.0f, dist.y, zero),
		float3(2.0f / 3.0f, dist.z, zero),
		float3(one, dist.w, zero)
	);

	// extrema
	float2 t;
	switch (polynomial<2>((transpose(bezier_spline::tangentblend) * dist).wzy).roots(t)) {
		case 2: {
			// 3 intervals
			if (t.y < t.x)
				t = t.yx;
			return
				(t.x > zero	&& root_2d(db.split_left(t.x),  *_t) && ray_contains(r, (*_p = b.evaluate(*_t)).xy)) ||
				(t.y > t.x	&& root_2d(db.middle(t.x, t.y), *_t) && ray_contains(r, (*_p = b.evaluate(*_t)).xy)) ||
				(t.y < one	&& root_2d(db.split_right(t.y), *_t) && ray_contains(r, (*_p = b.evaluate(*_t)).xy));
		}
		case 1: {
			// 2 intervals
			return
				(t.x > zero	&& root_2d(db.split_left(t.x),  *_t) && ray_contains(r, (*_p = b.evaluate(*_t)).xy)) ||
				(t.y < one	&& root_2d(db.split_right(t.y), *_t) && ray_contains(r, (*_p = b.evaluate(*_t)).xy));
		}
		default:
			// trivial case
			return root_2d(db, *_t) && ray_contains(r, (*_p = b.evaluate(*_t)).xy);
	}
}
#endif

bool bezier_spline::ray_check(param(ray2) r, float &t, vector2 *normal) const {
	float4x4	a = float3x4(r.inv_matrix()) * float4x4(c0, c1, c2, c3) * blend;
	polynomial<float,3>	x	= a.row(0).wzyx;
	polynomial<float,3>	y	= a.row(1).wzyx;

	float3	roots;
	int		num_roots	= abs(x.roots(roots));
	float	minr, miny	= maximum;

	for (int i = 0; i < num_roots; i++) {
		float	r = roots[i];
		if (between(r, zero, one)) {
			float	t = y.eval(r);
			if (between(t, zero, one) && t < miny) {
				miny	= t;
				minr	= r;
			}
		}
	}
	if (miny == float(maximum))
		return false;

	t = miny;
	if (normal)
		*normal = perp(tangent(minr).xy);
	return true;
}

position2 bezier_spline::support(param(float2) v) const {
	float2	a2	= (c3.xy - c0.xy) * 3 + (c1.xy - c2.xy) * 9;
	float2	a1	= (c0.xy + c2.xy) * 6 - c1.xy * 12;
	float2	a0	= (c1.xy - c0.xy) * 3;

	polynomial<float, 2>	x	= float3{dot(a0, v), dot(a1, v), dot(a2, v)};
	float2			roots;

	if (x.roots(roots) > 0) {
		float	r = deriv(x).eval(roots.x) < zero ? float(roots.x) : float(roots.y);
		return position2(evaluate(clamp(r, 0, 1)).xy);
	}
	return position2(dot(c0.xy, v) > dot(c3.xy, v) ? c0.xy : c3.xy);
}

position3 bezier_spline::support(param(float3) v) const {
	float3	a2	= (c3.xyz - c0.xyz) * 3 + (c1.xyz - c2.xyz) * 9;
	float3	a1	= (c0.xyz + c2.xyz) * 6 - c1.xyz * 12;
	float3	a0	= (c1.xyz - c0.xyz) * 3;

	polynomial<float, 2>	x	= float3{dot(a0, v), dot(a1, v), dot(a2, v)};
	float2			roots;

	if (x.roots(roots) > 0) {
		float	r = deriv(x).eval(roots.x) < zero ? float(roots.x) : float(roots.y);
		return position3(evaluate(clamp(r, 0, 1)).xyz);
	}
	return position3(dot(c0.xyz, v) > dot(c3.xyz, v) ? c0.xyz : c3.xyz);
}

//-----------------------------------------------------------------------------
// bezier_patch
//-----------------------------------------------------------------------------

bezier_patch::bezier_patch(const rectangle &r) {
	float2	a = r.a;
	float2	b = r.b;
	for (unsigned i = 0; i < 4; i++) {
		for (unsigned j = 0; j < 4; j++)
			row(i)[j] = {lerp(a.x, b.x, uint2float(i) / 3.f), lerp(a.y, b.y, uint2float(j) / 3.f), zero, one};
	}
}

bezier_patch operator*(param(float4x4) m, const bezier_patch &p) {
	bezier_patch	p2;
	for (int i = 0; i < 4; i++)
		p2[i] = m * p[i];
	return p2;
}

bezier_patch operator*(param(float3x4) m, const bezier_patch &p) {
	bezier_patch	p2;
	for (int i = 0; i < 4; i++)
		p2[i] = m * p[i];
	return p2;
}

bezier_patch bezier_patch::middle(float u0, float u1, float v0, float v1) const {
	bezier_patch		p(
		col(0).middle(v0, v1),
		col(1).middle(v0, v1),
		col(2).middle(v0, v1),
		col(3).middle(v0, v1)
	);

	return bezier_patch(
		p.row(0).middle(u0, u1),
		p.row(1).middle(u0, u1),
		p.row(2).middle(u0, u1),
		p.row(3).middle(u0, u1)
	);
}

bool bezier_patch::calc_params_nr(param(position3) p, float4 &result, vector3 *normal, param(float2) _uv) const {
	float2	uv	= _uv;
	for (int i = 0; i < 10; i++) {
		bezier_spline		bezu(evaluate_u(uv.x));
		bezier_spline		bezv(evaluate_v(uv.y));

		vector4		vp	= bezu.evaluate(uv.y);
		vector3		du	= bezv.tangent(uv.x).xyz;
		vector3		dv	= bezu.tangent(uv.y).xyz;

		if (len2(du) == zero) {
			du = bezv.c3.xyz - bezv.c0.xyz;
		}
		if (len2(dv) == zero) {
			dv = bezu.c3.xyz - bezu.c0.xyz;
		}
		vector3		n	= normalise(cross(du, dv));
		vector3		off	= p - vp.xyz;
		if (abs(abs(dot(normalise(off), n)) - one) < (i == 9 ? .02f : .001f)) {
			if (all(uv >= select(i == 9, -0.1f, zero)) && all(uv <= select(i == 9, 1.1f, one))) {
				result	= concat(uv, dot(off, n), vp.w);
				if (normal) {
					*normal	= n;
					ISO_ASSERT(!is_nan(*normal));
				}
				return true;
			}
		}

		float2 dotuv{dot(off, du), dot(off, dv)};
		uv += select(dotuv != zero, dotuv / (len2(du), len2(dv)), zero);

		if (any(uv < -one) || any(uv > 2))
			break;
	}
	return false;
}

bool bezier_patch::calc_params_nr(param(float2) pos, float4 &result, vector3 *normal, param(float2) _uv) const {
	float2	uv	= _uv;
	for (int i = 0; i < 10; i++) {
		bezier_spline		bezu(evaluate_u(uv.x));
		bezier_spline		bezv(evaluate_v(uv.y));

		float4		vp	= bezu.evaluate(uv.y);
		float2		off	= pos - vp.xy;
		float3		du	= bezv.tangent(uv.x).xyz;
		float3		dv	= bezu.tangent(uv.y).xyz;
		if (len2(du) == 0.0f) {
			du = bezv.c3.xyz - bezv.c0.xyz;
		}
		if (len2(dv) == 0.0f) {
			dv = bezu.c3.xyz - bezu.c0.xyz;
		}

		if (float(len2(off)) < (i == 9 ? .02f : .001f)) {
			if (all(uv >= select(i == 9, -0.1f, zero)) && all(uv <= select(i == 9, 1.1f, one))) {
				result	= concat(uv, vp.zw);
				if (normal)
					*normal	= normalise(cross(du, dv));
				return true;
			}
		}

		float2 dotuv{dot(off.xy, du.xy), dot(off.xy, dv.xy)};
		uv += select(dotuv != zero, dotuv / (len2(du.xy), len2(dv.xy)), zero);

		if (any(uv < -one) || any(uv > 2))
			break;
	}
	return false;
}

float2 estimate_bezier_patch_params(const bezier_patch &patch, param(position2) pos) {
	float2	diag	= patch.r3.c0.xy - patch.r0.c3.xy;
	if (cross(pos.v - patch.r0.c3.xy, diag) * cross(patch.r0.c0.xy  - patch.r0.c3.xy, diag) > 0.f)
		return triangle(position2(patch.r0.c0.xy), position2(patch.r0.c3.xy), position2(patch.r3.c0.xy)).parametric(pos);
	else
		return float2(one) - triangle(position2(patch.r3.c3.xy), position2(patch.r3.c0.xy), position2(patch.r0.c0.xy)).parametric(pos);
}

class bezier_patch_quarter {
	float4	temp[7 * 7];
public:
	bezier_patch_quarter(const bezier_patch &patch);
	void get(bezier_patch &patch, int i) {
		float4	*a	= &temp[((i >> 1) * 7 + (i & 1)) * 3];
		for (int j = 0; j < 4; j++)
			patch[j] = *(bezier_spline*)(a + j * 7);
	}
};

bezier_patch_quarter::bezier_patch_quarter(const bezier_patch &patch) {
	// subdivide to 7x4
	for (int i = 0; i < 4; i++) {
		const bezier_spline &s = patch[i];
		float4	h	= (s.c1  + s.c2) * half,
				cl1 = (s.c0  + s.c1) * half,
				cr2 = (s.c3  + s.c2) * half,
				cl2 = (cl1 + h) * half,
				cr1 = (cr2 + h) * half,
				cs	= (cr1 + cl2) * half;
		float4*	t	= &temp[i * 14];
		t[0] = s.c0;
		t[1] = cl1;
		t[2] = cl2;
		t[3] = cs;
		t[4] = cr1;
		t[5] = cr2;
		t[6] = s.c3;
	}
	// subdivide the other way to reach 7x7
	for (int i = 0; i < 7; i++) {
		float4*	t	= &temp[i];
		float4	h	= (t[1*14]  + t[2*14]) * half,
				cl1 = (t[0*14]  + t[1*14]) * half,
				cr2 = (t[3*14]  + t[2*14]) * half,
				cl2 = (cl1 + h) * half,
				cr1 = (cr2 + h) * half,
				cs	= (cr1 + cl2) * half;
		t[1*7] = cl1;
		t[2*7] = cl2;
		t[3*7] = cs;
		t[4*7] = cr1;
		t[5*7] = cr2;
	}
}

bool bezier_patch::ray_check(param(ray3) r, float &t, vector3 *normal) const {
	bezier_patch	stack[32], *patch = stack;

	if (len2(r.d) == zero)
		return false;

	float3x3	m = look_along_z(r.d);
	float3x3	im = transpose(m);

	for (int i = 0; i < 16; i++)
		patch->cp(i) = concat((im * position3(cp(i).xyz - r.p.v)).v, one);

	cuboid	ext = patch->get_box();
	if (any(ext.a.v.xy > zero) || any(ext.b.v.xy < zero))
		return false;
	if (!check_inside_hull((position2*)patch, (position2*)patch + 16, position2(zero)))
		return false;

	float	maxsize	= len2(r.d) * 1000000.f;
	float4	result;
	if (len2(ext.extent().xy) < maxsize) {
		float2	uv	= estimate_bezier_patch_params(*patch, position2(zero));
		if (patch->calc_params_nr(float2(zero), result, normal, uv)) {
			if (any(result.xy < zero) || any(result.xy > one)) {
				float3	p = evaluate(clamp(result.xy, zero, one)).xyz;
				if (len(p.xy - r.p.v.xy) > 0.1f)
					return false;
			}
			t = result.z / len(r.d);
			*normal = m * *normal;
			ISO_ASSERT(!is_nan(*normal));
			return true;
		}
	}

	do {
		bezier_patch_quarter	pq(*patch);
		// push appropriate subpatch(es) on stack
		for (int i = 0; i < 4; i++) {
			pq.get(*patch, i);
			if (check_inside_hull((position2*)patch, (position2*)patch + 16, position2(zero))) {
				if (len2(patch->r3.c3.xy - patch->r0.c0.xy) < maxsize) {
				// try Newton Raphson
					float2	uv = clamp(estimate_bezier_patch_params(*patch, position2(zero)), zero, one);
					if (patch->calc_params_nr(float2(zero), result, normal, uv)) {
						t = result.z / len(r.d);
						*normal = m * *normal;
						return true;
					}
				}
				patch++;
				ISO_ASSERT(patch < &stack[32]);
			}
		}
	} while (patch-- > stack);
	return false;
}

bool bezier_patch::test(param(sphere) s, float &t, vector3 *normal) const {
	float4	result;
	if (calc_params_nr(s.centre(), result, normal, float2(half)) && abs(result.z) < s.radius()) {
		t = s.radius() - result.z;
		return true;
	}
	return false;
}

bool bezier_patch::test(param(obb3) obb, float &t, position3 *position, vector3 *normal) const {
	bezier_patch	stack[32], *patch = stack;

	float3x4		im = obb.inv_matrix();
	ISO_ASSERT(!is_nan(im));
	*patch = (float4x4)im * *this;

	cuboid	ext = patch->get_box();
	if (!overlap(ext, unit_cube))
		return false;

	plane	p(position3(patch->r0.c0.xyz), position3(patch->r0.c3.xyz), position3(patch->r3.c0.xyz));
	float	mind = zero, maxd = zero;
	for (int i = 1; i < 16; i++) {
		float	d = p.dist(position3(patch->cp(i).xyz));
		maxd = max(maxd, d);
		mind = min(mind, d);
	}

	if (reduce_add(abs(p.normal())) < -(p.dist() + maxd))
		return false;

	if (maxd - mind < 0.1f) {
		float3	clipped[10];
		clipped[0]	= patch->r0.c0.xyz;
		clipped[1]	= patch->r0.c3.xyz;
		clipped[2]	= patch->r3.c3.xyz;
		clipped[3]	= patch->r3.c0.xyz;

		if (auto n = clip_poly(clipped, clipped + 4, clipped) - clipped) {
			float3	pos = clipped[0];
			for (int i = 1; i < n; i++)
				pos += clipped[i];
			pos /= n;

			float	x	= reduce_min((pos - sign1(p.normal())) / p.normal());
			*normal		= normalise((p / im).normal());
			*position	= obb.matrix() * position3(pos);
			t			= len(obb.matrix() * p.normal()) * x;
			return true;
		}
	}

	t = 0;
	do {
		bezier_patch_quarter	pq(*patch);
		for (int i = 0; i < 4; i++) {
			pq.get(*patch, i);
			cuboid	ext = patch->get_box();
			if (overlap(ext, unit_cube)) {
				plane	plane(position3(patch->r0.c0.xyz), position3(patch->r0.c3.xyz), position3(patch->r3.c0.xyz));
				float	mind = zero, maxd = zero;
				for (int i = 1; i < 16; i++) {
					float	d = plane.dist(position3(patch->cp(i).xyz));
					maxd = max(maxd, d);
					mind = min(mind, d);
				}

				if (reduce_add(abs(plane.normal())) < -(plane.dist() + maxd))
					continue;

				if (maxd - mind < 0.1f) {
					float3	clipped[10];
					clipped[0]	= patch->r0.c0.xyz;
					clipped[1]	= patch->r0.c3.xyz;
					clipped[2]	= patch->r3.c3.xyz;
					clipped[3]	= patch->r3.c0.xyz;

					if (auto n = clip_poly(clipped, clipped + 4, clipped) - clipped) {
						float3	pos = clipped[0];
						for (int i = 1; i < n; i++)
							pos += clipped[i];
						pos /= n;

						vector3		c	= -sign1(p.normal());
						float		x	= reduce_min(select(p.normal() == zero, 1e6f, (pos - c) / p.normal()));
						float		pen	= len(obb.matrix() * p.normal()) * x;
						if (pen > t) {
							*normal		= normalise((plane / im).normal());
							*position	= obb.matrix() * position3(pos);
							t			= pen;
						}
					}

					continue;
				}
				patch++;
				ISO_ASSERT(patch < &stack[32]);
			}
		}
	} while (patch-- > stack);
	return t != 0;
}

cuboid bezier_patch::get_box() const {
	float3	a = cp(0).xyz, b = a;
	for (int i = 1; i < 16; i++) {
		a = min(a, cp(i).xyz);
		b = max(b, cp(i).xyz);
	}
	return cuboid(position3(a), position3(b));
}

}	//namespace iso
