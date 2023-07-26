#include "geometry.h"
#include "base/strings.h"
#include "maths/polynomial.h"

namespace iso {

// Constant velocity separating axis test
bool velocity_overlap(interval<float> u, interval<float> v, float speed, interval<float> &t) {
	if (v.b < u.a) {
		// V on left of U
		if (speed <= 0)							// V moving away from U
			return false;

		t.a = max(t.a, (u.a - v.b) / speed);	// first time of contact on this axis
		if (t.a > t.b)							// intersection after desired time interval
			return false;

	} else if (u.b < v.a) {
		// V on right of U
		if (speed >= 0)							// V moving away from U
			return false;

		t.a = max(t.a, (u.b - v.a) / speed);	// first time of contact on this axis
		if (t.a > t.b)							// intersection after desired time interval
			return false;

	}
	// V and U on overlapping interval
	if (speed)
		t.b = min(t.b, (speed > 0 ? u.b - v.a : u.a - v.b) / speed);	// last time of contact on this axis

	return t.a <= t.b;
}

//-----------------------------------------------------------------------------
//
//						2D
//
//-----------------------------------------------------------------------------

float2 solve_line(param(float2) pt0, param(float2) pt1) {
	float2 d = pt1 - pt0;
	float m = select(d.x == zero, 999.f, d.y / d.x);
	float b = pt0.y - m * pt0.x;
	return float2{m, b};
}

position2 intersect_lines(param(position2) pos0, param(float2) dir0, param(position2) pos1, param(float2) dir1) {
	float	d = cross(dir0, dir1);
	return d == 0
		? pos1
//		: (dir0 * cross(pos1, dir1) - dir1 * cross(pos0, dir0)) / d;
		: pos0 + dir0 * (cross(pos1 - pos0, dir1) / d);
}

float2x3 reflect(param(line2) p) {
	float3		v = as_vec(normalise(p));
	float2		t = v.xy * 2;
	float2x3	m = identity;
	m.x -= t * v.x;
	m.y -= t * v.y;
	m.z -= t * v.z;
	return m;
}

bool intersection(const line2 &a, const line2 &b, float2 &intersection, float tol) {
	float3	t	= cross(as_vec(a), as_vec(b));
	if (approx_equal(t.z, 0, tol))
		return false;
	intersection = t.xy / t.z;
	return true;
}

//-----------------------------------------------------------------------------
// conic
//-----------------------------------------------------------------------------

static float conic_combine(float a, const conic &c1, const conic &c2) {
	return	c1.d.z * a
		-	c1.o.z * c1.o.z * c2.d.y
		-	c1.o.y * c1.o.y * c2.d.x
		+	c1.o.z * c1.o.y * c2.o.x * two;
}

float vol_derivative(const conic &c1, const conic &c2) {
	float	a1	= c1.d.x * c2.d.y + c2.d.x * c1.d.y - two * c1.o.x * c2.o.x;
	float	a0	= c1.det2x2();

	float	b1	= a0 * c2.d.z
				- two * (c1.o.z * c2.o.z * c1.d.y + c1.o.y * c2.o.y * c1.d.x)
				+ conic_combine(a1, c1, c2)
				+ two * (c1.o.z * c2.o.y + c2.o.z * c1.o.y) * c1.o.x;
	float	b0	= conic_combine(a0, c1, c1);

	return two * a0 * b1 - 3 * a1 * b0;
}

float vol_minimum(const conic &c1, const conic &c2) {
	auto	m	= swizzle<0,1>((const symmetrical3&)c1);
	auto	n	= swizzle<0,1>((const symmetrical3&)c2);

	float	_a2	= n.det();
	float	_a1	= dot(m.d.d.xy, n.d.d.yx) - two * m.d1.d * n.d1.d;
	float	_a0	= m.det();

	float	a2	= c2.det2x2();
	float	a1	= c1.d.x * c2.d.y + c2.d.x * c1.d.y - two * c1.o.x * c2.o.x;
	float	a0	= c1.det2x2();

	ISO_ASSERT(a2 == _a2 && a1 == _a1 && a0 == _a0);

	float	b3	= conic_combine(a2, c2, c2);
	float	b2	= a2 * c1.d.z
				- two * (c1.o.z * c2.o.z * c2.d.y + c1.o.y * c2.o.y * c2.d.x)
				+ conic_combine(a1, c2, c1)
				+ two * (c1.o.z * c2.o.y + c2.o.z * c1.o.y) * c2.o.x;
	float	b1	= a0 * c2.d.z
				- two * (c1.o.z * c2.o.z * c1.d.y + c1.o.y * c2.o.y * c1.d.x)
				+ conic_combine(a1, c1, c2)
				+ two * (c1.o.z * c2.o.y + c2.o.z * c1.o.y) * c1.o.x;
	float	b0	= conic_combine(a0, c1, c1);

	polynomial<float, 3>	cubic = float4{
		-two * a0 * b1 + 3 * a1 * b0,
		-four * a0 * b2 + a1 * b1 + 6 * a2 * b0,
		-6 * a0 * b3 - a1 * b2 + four * a2 * b1,
		-3 * a1 * b3 + two * a2 * b2
	};
	float3	roots;
	int		num_roots	= cubic.roots(roots);
	float	best		= roots.x;

	if (num_roots == 3) {
		float max_det = 0;
		for (int i = 0; i < 3; ++i) {
			float	x	= roots[i];
			float	q	= ((b3 * x + b2) * x + b1) * x + b0;
			if (q != 0) {
				float	det	= cube((a2 * x + a1) * x + a0) / square(q);
				if (det > max_det) {
					max_det = det;
					best	= x;
				}
			}
		}
		ISO_ASSERT(max_det > 0);
	}
	return best;
}

conic conic::line_pair(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4) {
//	ISO_ASSERT(any(p1.v != p2.v & p3.v != p4.v));

	float2	d12		= p2 - p1;
	float2	d34		= p4 - p3;
	float	cross12	= cross(p1, p2);
	float	cross34	= cross(p3, p4);

	return conic(
		float3{
			d12.y * d34.y,
			d12.x * d34.x,
			cross12 * cross34
		},
		float3{
			-(d12.x * d34.y + d12.y * d34.x),
			cross12 * d34.x + d12.x * cross34,
			-(cross12 * d34.y + d12.y * cross34)
		} * half
	);
}

void conic::two_linepairs(conic &c1, conic &c2, const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4) {
	SIGN	side1_24 = get_sign1(cross(p1, p2) + cross(p2, p4) + cross(p4, p1));
	SIGN	side3_24 = get_sign1(cross(p2, p4) + cross(p4, p3) + cross(p3, p2));

	if (side1_24 != side3_24) {
		// (counter)clockwise order
		c1 = line_pair(p1, p2, p3, p4);
		c2 = line_pair(p2, p3, p4, p1);
	} else {
		SIGN	side1_32 = get_sign1(cross(p1, p3) + cross(p3, p2) + cross(p2, p1));
		if (side1_32 != side3_24) {
			// p1, p2 need to be swapped
			c1 = line_pair(p2, p1, p3, p4);
			c2 = line_pair(p1, p3, p4, p2);
		} else {
			// p2, p3 need to be swapped
			c1 = line_pair(p1, p3, p2, p4);
			c2 = line_pair(p3, p2, p4, p1);
		}
	}
}

conic conic::through(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4, const position2 &p5) {
	conic	c1 = line_pair(p1, p2, p3, p4);
	conic	c2 = line_pair(p1, p4, p2, p3);
	return c1 * c2.evaluate(p5) - c2 * c1.evaluate(p5);
}

conic conic::circle_through(const position2 &p1, const position2 &p2, const position2 &p3) {
	// precondition: p1, p2, p3 not colinear
	float det = cross(p1 - p3, p2 - p3);
	ISO_ASSERT(det != 0);

	float3 sqr{dot(p1, p1), dot(p2, p2), dot(p3, p3)};
	return conic(
		float3{
			det,
			det,
			dot(float3{cross(p3.v, p2.v), cross(p1.v, p3.v), cross(p2.v, p1.v)}, sqr)
		},
		float3{
			zero,
			dot(float3{p2.v.x - p3.v.x, p3.v.x - p1.v.x, p1.v.x - p2.v.x}, sqr) * half,	//o.y
			dot(float3{p3.v.y - p2.v.y, p1.v.y - p3.v.y, p2.v.y - p1.v.y}, sqr) * half	//o.z
		}
	);
}

conic conic::ellipse_through(const position2 &p1, const position2 &p2, const position2 &p3) {
	// precondition: p1, p2, p3 not colinear
	float det = cross(p1 - p3, p2 - p3);
	ISO_ASSERT(det != 0);

	float3	x{p1.v.x, p2.v.x, p3.v.x}, y{p1.v.y, p2.v.y, p3.v.y};

	float2	dyx	= p1.v * (p1.v - p2.v - p3.v) + p2.v * (p2.v - p3.v) + p3.v * p3.v;
	float3	d	= concat(dyx.yx,
		cross(p2.v, p1.v * p3.v.x + p3.v * p2.v.x) * p1.v.y
	+	cross(p3.v, p1.v * p3.v.x + p2.v * p1.v.x) * p2.v.y
	+	cross(p1.v, p2.v * p1.v.x + p3.v * p2.v.x) * p3.v.y
	);

	float3	t{
		cross(p1.v, p2.v + p3.v),
		cross(p2.v, p1.v + p3.v),
		cross(p3.v, p1.v + p2.v)
	};

	float2	oyz	= p1.v * t.x + p2.v * t.y + p3.v * t.z;
	float3	o	= float3{
		p1.v.x * (p2.v.y + p3.v.y - two * p1.v.y) + p2.v.x * (p3.v.y + p1.v.y - two * p2.v.y) + p3.v.x * (p1.v.y + p2.v.y - two * p3.v.y),
		-oyz.x, oyz.y
//		-(x.x * p1.x + x.y * p2.x + x.z * p3.x),
//		 x.x * p1.y + x.y * p2.y + x.z * p3.y
	};

	dot(x, y.yzx + y.zxy - y * two);

	if (det > 0) {
		d	= -d;
		o	= -o;
	}
	return conic(d, o * half);
}

conic conic::ellipse_through(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4) {
	conic	c1, c2;
	two_linepairs(c1, c2, p1, p2, p3, p4);
	return lerp(c1, c2, vol_minimum(c1, c2 - c1));
}

conic conic::line(const iso::line<float, 2> &p1) {
	auto	v1 = as_vec(p1);
#if 1
	return conic(v1.xyz * v1.xyz, v1.xyz * v1.yzx);
#else
	return conic(extend_left<3>(v1.z), extend_left<3>(v1.yx * half));
#endif
}

conic conic::line_pair(const iso::line<float, 2> &p1, const iso::line<float, 2> &p2) {
	auto	v1 = as_vec(p1);
	auto	v2 = as_vec(p2);
	return conic(v1.xyz * v2.xyz, (v1.xyz * v2.yzx + v2.xyz * v1.yzx) * half);
}

conic conic::from_matrix(const float3x3 &m) {
	float3	x2	= square(m.x), y2 = square(m.y), z2 = square(m.z);
	float3	d{
		x2.x + x2.y - x2.z,
		y2.x + y2.y - y2.z,
		z2.x + z2.y - z2.z
	};

	float3	xy	= m.x * m.y, xz = m.x * m.z, yz = m.y * m.z;
	float3	o{
		xy.x + xy.y - xy.z,
		yz.x + yz.y - yz.z,
		xz.x + xz.y - xz.z
	};
	return conic(d, o / 4);
}

conic::info conic::analyse() const {
	float	det2		= det2x2();
	TYPE	type		= (TYPE)get_sign1(det2);
	bool	empty		= false;
	SIGN	orientation	= ZERO;
	bool	degenerate;

	if (type == PARABOLA) {
		float3	d = this->d, o = this->o;
		if (d.x == 0 && d.y == 0) {
			degenerate		= true;
			if (o.z == 0 && o.y == 0) {
				if (d.z == 0) {
					type		= TRIVIAL;
				} else {
					empty		= true;
					orientation	= get_sign1(d.z);
				}
			}

		} else {
			if (d.x == 0) {
				d = d.yxz;
				o = o.xzy;
			}

			if (degenerate = o.x * o.z == d.x * o.y) {
				float discr = square(o.y) - square(d.y);
				if (discr < 0) {
					empty		= true;
					orientation = get_sign1(d.z);
				} else if (discr == 0) {
					orientation = get_sign1(d.x);
				} else {
					orientation = NEG;
				}
			} else {
				orientation = get_sign1(-d.x);
			}
		}

	} else {
		orientation	= get_sign1(conic_combine(det2, *this, *this));
		empty		= type == ELLIPSE && get_sign1(d.x) == orientation;
		degenerate	= empty || orientation == ZERO;
	}
	return {type, orientation, empty, degenerate};
}

#if 0
//shear transform:
float	t		= sqrt(-det() / (d.x * det2));
m = {
	float2{t, zero},
	float2{-o.x, d.x} * t,
	centre()
};
#endif

conic::info conic::analyse(float2x3 &m) const {
	float	epsilon		= reduce_max(max(abs(o), abs(d))) / 100.f;
	float	epsilon2	= square(epsilon);
	float	det2		= det2x2();
//	float	epsilon		= abs(det()) / 100.f;
	TYPE	type		= (TYPE)get_sign1(det2, epsilon2);
	bool	empty		= false;
	SIGN	orientation	= ZERO;
	bool	degenerate;

	float2x2	m1	= identity;

	if (o.x) {
		float	ac	= d.x - d.y;
	//	auto	sc	= concat((-ac - sqrt(square(ac) + square(o.x) * 4)) * half, -o.x);
		auto	sc	= concat((ac + sqrt(square(ac) + square(o.x) * 4)) * half, o.x);
		m1	= _rotate2D(normalise(sc));
	}

	if (type == PARABOLA) {
		auto	c1	= *this / float3x3(m1);
		degenerate	= abs(c1.o.y) < epsilon;
		orientation = get_sign1(-c1.d.x, epsilon2);

		if (orientation == ZERO) {
			orientation		= get_sign1(c1.d.z, epsilon2);
			if (degenerate) {
				if (orientation == ZERO)
					type	= TRIVIAL;
				else
					empty	= true;
			}
			degenerate	= true;
			m			= float2x3(m1);

		} else {
			if (degenerate)
				empty = c1.d.x * c1.d.z > 0;

			float	tx	= -c1.o.z / c1.d.x;
			float	ty	= (square(c1.o.z) / c1.d.x - c1.d.z) / (c1.o.y * 2);
			auto	m2	= m1 * translate(tx, ty);
			c1			= *this / m2;
			m			= m2 * scale(sign1(c1.d.x), abs(c1.d.x) / c1.o.y);
		}
		
	} else {
		m			= translate(centre()) * m1;
		auto	c1	= *this / m;
		orientation	= get_sign1(c1.d.z, epsilon2);
		empty		= type == ELLIPSE && c1.d.x * c1.d.z > 0;
		degenerate	= empty || orientation == ZERO;
		
		if (orientation != ZERO) {
			float2	s	= c1.d.xy / c1.d.z;
			m			= m * scale(sign1(c1.d.x) * rsqrt(abs(s)));
			if (type == HYPERBOLA && s.x > 0)
				m	= m * rotate2D(pi / two);
		}
	}

	auto	verify	= *this / m;	//verification
	return {type, orientation, empty, degenerate};
}


int	intersection(const conic& a, const conic& b, position2* results) {
	auto	A	= ((const symmetrical3&)a * inverse(b));
	auto	e	= get_eigenvalues(A);

	auto	degen	= a - e.x * b;
	auto	intx	= degen & x_axis, inty = degen & y_axis;

	line2	z		= line2(position2(intx.a, 0), position2(0, inty.a));
	auto	intz	= a & z;

	results[0] = from_x_axis(z) * position2(intz.a, 0);
	results[1] = from_x_axis(z) * position2(intz.b, 0);
	return 2;
}


// tangent points to x, y axes given by M * [+-1,0], M * [0,+-1]
float2x3 conic::get_tangents() const {
	float2	v	= sqrt(det() / -d.yx) / det2x2();
	return float2x3(
		float2{d.y, -o.x} * v.x,
		float2{-o.x, d.x} * v.y,
		centre()
	);
}

position2 conic::support(param(float2) v) const {
	float2x2	m	= _rotate2D(normalise(v));
	conic		c	= *this / float3x3(m);
	auto		t	= c.get_tangents();
	return m * (get_trans(t) + t.x);
}

position2 conic::closest(param(position2) p) const {
	conic	c2(
		float3{
			+o.x,
			-o.x,
			cross(o.zy, p.v)
		},
		float3{
			d.y - d.x,
			cross(concat(o.x, d.y), p.v) - o.z,
			cross(concat(d.x, o.x), p.v) + o.y
		} / two
	);

	return  {nan, nan};
	position2	results[4];
	int			num_int = intersection(*this, c2, results);
	return results[0];
}

bool conic::ray_check(param(ray2) r, float &t, vector2 *normal)	const {
	float3x3	Q		= *this;
	float2		p1		= (Q * concat(r.p.v, one)).xy;
	float2		d1		= (Q * concat(r.d, zero)).xy;
	float		dp		= dot(r.d, p1);
	float		dd		= dot(r.d, d1);
	float		discr	= square(dp) - dd * dot(r.p.v, p1);
	if (discr < zero)
		return false;
	t = (-dp + sqrt(discr)) / dd;
	if (normal) {
		float3	p = concat(r.from_parametric(t).v, one);
		*normal = float2{dot(p, Q.x), dot(p, Q.y)};
	}
	return true;
}

//-----------------------------------------------------------------------------
// rectangle
//-----------------------------------------------------------------------------
const unit_rect_t	unit_rect;

//	http://vcg.isti.cnr.it/publications/papers/quadrendering.pdf
float4 mean_value_coords(param(float4) x, param(float4) y) {
	float4	A	= x * y.yzwx - y * x.yzwx;	//	s[i] x s[i+1]
	float4	D	= x * x.yzwx + y * y.yzwx;	//	s[i] . s[i+1]
	float4	r	= sqrt(x * x + y * y);		//	|s[i]|

	float4	rA	= A.yzwx * A.zwxy * A.wxyz;		//	avoid dividing by A by multiplying by all other A's
	float4	t	= (r * r.yzwx - D) * rA;		//	(r[i] r[i+1] - D[i]) / A[i]

	float4	mu	= select(r == zero, one, (t + t.wxyz) / r);	//	(t[i-1] + t[i]) / r[i]
	return mu / reduce_add(mu);
}

float4 unit_rect_barycentric(param(float2) p) {
	return mean_value_coords(float4{-1,+1,+1,-1} - p.x, float4{-1,-1,+1,+1} - p.y);
}

//-----------------------------------------------------------------------------
// parallelogram
//-----------------------------------------------------------------------------
/*
bool overlap(param(parallelogram) a, param(parallelogram) b) {
	float2x3	m1	= a.inv_matrix() * b.matrix();
	vector2		a1	= abs(m1.x) + abs(m1.y);
	if (any(abs(m1.z.xy) - a1 > one))
		return false;

	float2x3	m2	= inverse(m1);//b.inv_matrix() * matrix();
	vector2		a2	= abs(m2.x) + abs(m2.y);
	if (any(abs(m2.z.xy) - a2 > one))
		return false;

	return true;
}

position2 parallelogram::uniform_perimeter(float v) const {
	float2	e = get_scale(*this);
	float	s = reduce_add(e);

	v *= s * 2;
	bool	a = v > s;
	if (a)
		v -= s;

	return matrix() * (v > e.x ? position2(a ? 1 : -1, (v - e.x) / e.y) : position2(v / e.x, a ? 1 : -1));
}
position2 parallelogram::closest(param(position2) p) const {
	float2	a = float2(inv_matrix() * p);
#if 1
	return matrix() * position2(clamp(a, -one, one));
#else
	int			i = bit_mask(abs(a) > one);

	switch (i) {
		default:	//inside
			return a.x < a.y
				? ray2::with_centre(v.z + v.x * sign1(a.x), y).closest(p)
				: ray2::with_centre(v.z + v.y * sign1(a.y), x).closest(p);
		case 1:	// outside vertical edge
			return ray2::with_centre(v.z + v.x * sign1(a.x), y).closest(p);
		case 2:	// outside horizontal edge
			return ray2::with_centre(v.z + v.y * sign1(a.y), x).closest(p);
		case 3:	// outside two edges - closest is along common point
			return centre() + v.x * sign1(a.x) + v.y * sign1(a.y);
	}
#endif
}
*/

//-----------------------------------------------------------------------------
// obb2
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// circle
//-----------------------------------------------------------------------------
const unit_n_sphere<float, 2> unit_circle;

//-----------------------------------------------------------------------------
// ellipse
//-----------------------------------------------------------------------------

ellipse::ellipse(const float2x3 &m) {
#if 0
	conic	c;
	float2x3 i = inverse(m);
	c.r	= dot(i.x, i.x);
	c.s = dot(i.y, i.y);
	c.t = two * dot(i.x, i.y);
	c.u = two * dot(i.x, i.z);
	c.v = two * dot(i.y, i.z);
	c.w = dot(i.z, i.z) - one;
	*this = c;
	return;
#else
	float2	sc		= max_circle_point((float2x2)m);
	float2	axis1	= m * sc;
	float2	axis2	= m * perp(sc);
	v		= concat(m.z, axis1);
	ratio	= sqrt(len2(axis2) / len2(axis1));
#endif
}

ellipse::ellipse(const conic &c) {
	position2	centre;
	auto	s	= c.centre_form(centre);
	float2	e	= get_eigenvalues(s);

	v		= concat(centre.v, normalise(get_eigenvector(s, e.x)) * rsqrt(e.x));
	ratio	= sqrt(e.x / e.y);
}

ellipse ellipse::through(param(position2) a, param(position2) b) {
	vector2		r	= (b - a) * half;
	return ellipse(a + r, a.v, zero);
}

ellipse ellipse::through(param(position2) p1, param(position2) p2, param(position2) p3) {
	float2	d1 = p1 - p3, d2 = p2 - p3;
	return !colinear(d1, d2)	? ellipse(conic::ellipse_through(p1, p2, p3))
		:	dot(d1, d2) < zero	? through(p1, p2)
		:	len2(d1) > len2(d2)	? through(p1, p3)
		:	through(p2, p3);
}

ellipse ellipse::through(param(position2) p1, param(position2) p2, param(position2) p3, param(position2) p4) {
	return conic::ellipse_through(p1, p2, p3, p4);
}

ellipse ellipse::through(param(position2) p1, param(position2) p2, param(position2) p3, param(position2) p4, param(position2) p5) {
	return conic::through(p1, p2, p3, p4, p5);
}

//minkowski sum
ellipse operator+(param(ellipse) a, param(ellipse) b) {
	ellipse	a1	= b.inv_matrix() * a;
	float	m	= len(a1.major());
	ellipse	a2	= ellipse(a1.centre(), a1.major() * (m + one) / m, (m * a1.ratio + one) / (m + one));
	return b.matrix() * a2;
}

//minkowski difference
ellipse operator-(param(ellipse) a, param(ellipse) b) {
	ellipse	a1	= b.inv_matrix() * a;
	float	m	= len(a1.major());
	ellipse	a2	= ellipse(a1.centre(), a1.major() * (m - one) / m, (m * a1.ratio - one) / (m - one));
	return b.matrix() * a2;
}

bool ellipse::ray_check(param(ray2) r, float &t, vector2 *normal) const {
	float2x3	m = inv_matrix();
	bool ret = unit_circle.ray_check(m * r, t, normal);
	if (ret && normal)
		*normal = transpose((float2x2)m) * *normal;
	return ret;
}

float ellipse::ray_closest(param(ray2) r) const {
	float2x3	m = inv_matrix();
	return unit_circle.ray_closest(m * r);
}

#if 0

position2 ellipse_closest(param(float2) p0, float ratio) {
	//http://wet-robots.ghost.io/simple-method-for-distance-to-ellipse/
	float2	a	= float2{1, ratio};
	float2	ec	= float2{1 - square(ratio), (square(ratio) - 1) / ratio};
	float2	p	= abs(p0);

	float2	d	= float2(rsqrt2);
#if 1
	for (int i = 0; i < 3; i++) {
		float2	e	= ec * cube(d);				//evolute - centre of approximating circle
		float2	q	= (p - e) * rsqrt(len2(p - e) / len2(a * d - e)) + e;
		d	= normalise(max(q / a, zero));
	}
#else
	for (int i = 0; i < 3; i++) {
		float2	x	= a * d;					//position on ellipse
		float2	e	= ec * cube(d);				//evolute - centre of approximating circle
		float	r	= len(x - e);				//radius of circle
		float2	q	= normalise(p - e) * r + e;	//closest point on circle
		d	= normalise(max(q / a, zero));
	}
#endif
	return position2(copysign(a * d, p0));
}

#else

//http://www.iquilezles.org/www/articles/ellipsedist/ellipsedist.htm
position2 ellipse_closest(param(float2) p, float ratio) {
	float2	ab		= float2{1, ratio};
	float2	pa		= abs(p);
	bool	swap	= pa.x > pa.y;

	if (swap) {
		pa	= pa.yx;
		ab	= ab.yx;
	}

	float	l	= ab.y * ab.y - ab.x * ab.x;
	float2	m	= ab * pa / l;
	float2	m2	= square(m);
	float	c	= (m2.x + m2.y - one) / 3;
	float	c3	= cube(c);
	float	q	= c3 + m2.x * m2.y * 2;
	float	d	= c3 + m2.x * m2.y;
	float	g	= m.x + m.x * m2.y;

	float co;

	if (d < 0) {
		float2	t	= sincos(acos(q / c3) / 3);
		float2	r	= sqrt((plus_minus(t.y) * sqrt3 + t.x + 2) * -c + m2.x);
		co = (r.y + copysign(r.x, l) + abs(g) / (r.x * r.y) - m.x) / 2;
	} else {
		float2	t	= pow(plus_minus(m.x * m.y * sqrt(d) * 2) + q, third);
		float2	r	= float2{-t.x - t.y - c * 4 + m2.x * 2, (t.x - t.y) * sqrt3};
		float	rm	= len(r);
		co = (r.y / sqrt(rm - r.x) + g * 2 / rm - m.x) / 2;
	}

	float2	c2 = ab * float2{co, sqrt(one - co * co)};
	if (swap)
		c2 = c2.yx;
	return position2(copysign(c2, p));
}
#endif

position2 ellipse::closest(param(position2) p) const {
	float2		v	= p - centre();
	float2		maj = major();
	position2	c2 = ellipse_closest(float2{dot(maj, v), cross(maj, v)} / len2(maj), ratio);
	// back to original space
	return centre() + (maj * c2.v.x + perp(maj) * c2.v.y);
}

position2 ellipse::support(param(float2) v) const {
	float2x2	m	= _rotate2D(normalise(v));
	conic		c	= conic(*this) / float3x3(m);
	auto		t	= c.get_tangents();
	return m * (get_trans(t) + t.x);

	float2		maj = major();
	position2	c2 = ellipse_closest(float2{dot(maj, v), cross(maj, v)} * rlen(maj), ratio);
	// back to original space
	return centre() + (maj * c2.v.x + perp(maj) * c2.v.y);
}

position2 uniform_perimeter(const ellipse &s, float x) {
	float	t	= x * 2 * pi;
	float	e2	= one - square(s.ratio), e4 = e2 * e2, e6 = e4 * e2;
	float	th	= t
				- (e2 / 8 + e4 / 16 + e6 * 71 / 2048)	* sin(t * 2)
				+ (e4 + e6) * 5 / 256					* sin(t * 4)
				+ e6 * 29 / 6144						* sin(t * 6);
	return s.matrix() * unit_circle_uniform_perimeter(frac(th / (2 * pi) + 0.25f));
}

//-----------------------------------------------------------------------------
// triangle
//-----------------------------------------------------------------------------

const _unit_tri unit_tri;
const array<position2, 3>	_unit_tri::_corners = { position2((float2)zero), position2((float2)x_axis), position2((float2)y_axis) };

bool _unit_tri::ray_check(param(ray2) r, float &t, vector2 *normal) {
	float3	b0	= barycentric(r.pt0()), b1 = barycentric(r.pt1());
	if (any(max(b0, b1) < zero))
		return false;

	float3	t3 = -b0 / (b1 - b0);
	t = reduce_min(t3);

	if (normal) {
		switch (min_component_index(t3)) {
			case 0: *normal = y_axis; break;
			case 1: *normal = -float2(x_axis); break;
			case 2: *normal = float2(-rsqrt2); break;
		}
	}

	return true;
}

position2 _unit_tri::uniform_perimeter(float v) {
	return	v < third		? position2(v * 3, 0)
		:	v < third * 2	? position2(0, v * 3 - 1)
		:	position2(v * 3 - 2, 3 - v * 3);
}

position2 _unit_tri::support(param(float2) v) {
	int		i	= bit_mask(v > zero);
	if (i == 3)
		i = v.x < v.y ? 2 : 1;
	return corner(i);
}

bool triangle::ray_check(param(ray2) r) const {
	float3	b0	= barycentric(r.pt0()), b1 = barycentric(r.pt1());
	return all(max(b0, b1) >= zero);
}

bool triangle::ray_check(param(ray2) r, float &t, vector2 *normal) const {
	float3	b0	= barycentric(r.pt0()), b1 = barycentric(r.pt1());
	if (any(max(b0, b1) < zero))
		return false;

	b1	-= b0;
	float3	t3	= -b0 / b1;
	float3	bt	= b0.yzx + b1.yzx * t3.xyz;
	
	if (all(bt < zero | bt > one))
		return false;

	t = reduce_min(select(bt >= zero & bt <= one, t3, infinity));

	if (normal) {
		float2	n;
		switch (min_component_index(t3)) {
			case 0: n = perp(x); break;
			case 1: n = perp(y); break;
			case 2: n = perp(y - x); break;
		}
		*normal = normalise(n);
	}
	return true;
}

float triangle::ray_closest(param(ray2) r) const {
	float3	b0	= barycentric(r.pt0()), b1 = barycentric(r.pt1()) - b0;
	float3	t3		= -b0 / b1;
	float3	bt		= b0.yzx + b1.yzx * t3.xyz;
	auto	mask	= bt >= zero & bt <= one;
	if (any(mask))
		return reduce_min(select(mask, t3, infinity));

	auto	c	= z - as_vec(r.p);
	auto	dp	= perp(r.d);
	return dot(support(dot(c, dp) < zero ? dp : -dp) - r.p, r.d) / len2(r.d);
}

position2 uniform_perimeter(const triangle &s, float v) {
	float	l0 = len(s.x), l1 = len(s.y), l2 = len(s.y - s.x);
	v *= l0 + l1 + l2;
	return	v < l0		? position2(s.z + s.x * (v / l0))
		:	v < l0 + l1	? position2(s.z + s.y * ((v - l0) / l1))
		:	position2(s.z + s.x + (s.y - s.x) * ((v - l0 - l1) / l2));
}

position2 triangle::uniform_subdivide(uint32 u) const {
	//float2	A(1, 0), B(0, 1), C(0, 0);	// Barycentrics
	uint32	A = 0x00000001, B = 0x00010000, C = 0x00000000;
	uint32	s = 3;

	while (u) {
		switch (u & 3) {
			case 0: {
				uint32	C0 = C;
				C = A + B;
				A += C0;
				B += C0;
				break;
			}
			case 1:
				B += A;
				C += A;
				A += A;
				break;

			case 2:
				A += B;
				C += B;
				B += B;
				break;

			case 3:
				A += C;
				B += C;
				C += C;
				break;
		}
		u >>= 2;
		s <<= 1;
	}
	A += B + C;
	return from_parametric(float2{float(A & 0xffff), float(A >> 16)} / s);
	//	return (A + B + C) / (3 << 16);
}


position2 triangle::closest(param(position2) p) const {
	float3	a = barycentric(p);
	int		i = bit_mask(a < zero);
	switch (count_bits4(i)) {
		default: {	//inside
			i = min_component_index(a);
			position2	p0	= corner(inc_mod(i, 3));
			position2	p1	= corner(dec_mod(i, 3));
			return line2(p0, p1).project(p);
		}
		case 1:	{	// outside one edge
			i = lowest_set_index4(i);
			position2	p0	= corner(inc_mod(i, 3));
			position2	p1	= corner(dec_mod(i, 3));
			return ray2(p0, p1).closest(p);
		}
		case 2:	// outside two edges
			return corner(lowest_clear_index4(i));
	}
}

position2 triangle::support(param(float2) v) const {
	float2	a2	= v / matrix();
	int		i	= bit_mask(a2 > zero);
	if (i == 3)
		i = a2.x < a2.y ? 2 : 1;
	return corner(i);
}

//-----------------------------------------------------------------------------
// quadrilateral
//-----------------------------------------------------------------------------

position2 quadrilateral::centre() const {
	float4	t = p01 + p23;
	ray2	r1(position2(p01.xy + p01.zw), position2(p23.xy + p23.zw));
	ray2	r2(position2(t.xy), position2(t.zw));
	return position2((r1 & r2).v * half);
}

float4 quadrilateral::barycentric(param(float2) p) const {
	return mean_value_coords(concat(p01.xz, p23.zx) - p.x, concat(p01.yw, p23.wy) - p.y);
}

float3x3 quadrilateral::inv_matrix() const {
	static const float3x3	matP(
		float3{ one,	-one,	one},
		float3{-one,	 one,	one},
		float3{-two,	-two,	two}
	);
	float3x3	matC(
		concat(p01.zw,			one),
		concat(p23.xy,			one),
		concat(p01.xy * two,	two)
	);

	float3x3	invC	= inverse(matC);
	float3		v		= invC * position2(p23.zw);

	return float3x3(
		matP.x / v.x,
		matP.y / v.y,
		matP.z / (v.z * -two)
	) * invC;
}

float3x3 quadrilateral::matrix() const {
	static const float3x3	invP(
		float3{half,	zero,	-quarter},
		float3{zero,	half,	-quarter},
		float3{half,	half,	zero	}
	);
	float3x3	matC(
		concat(p01.zw,			one),
		concat(p23.xy,			one),
		concat(p01.xy * two,	two)
	);
//	float3x3	matP(
//		float3( one,	-one,	one),
//		float3(-one,	 one,	one),
//		float3(-two,	-two,	two)
//	);
//	float3x3	invP	= inverse(matP);

	float3x3	invC	= inverse(matC);
	float3		v		= invC * position2(p23.zw);

	return float3x3(
		matC.x * v.x,
		matC.y * v.y,
		matC.z * (v.z * -two)
	) * invP;
}

position2 uniform_perimeter(const quadrilateral &s, float v) {
	float	l0 = len(s.p01.xy - s.p01.zw), l1 = len(s.p01.zw - s.p23.xy), l2 = len(s.p23.xy - s.p23.zw), l3 = len(s.p23.zw - s.p01.xy);

	v *= l0 + l1 + l2 + l3;
	return	position2(
			v < l0				? lerp(s.p01.xy, s.p01.zw, v / l0)
		:	v < l0 + l1			? lerp(s.p01.zw, s.p23.xy, (v - l0) / l1)
		:	v < l0 + l1 + l2	? lerp(s.p23.xy, s.p23.zw, (v - l0 - l1) / l2)
		:	lerp(s.p23.zw, s.p01.xy, (v - l0 - l1 - l2) / l3)
	);
}

position2 uniform_interior(const quadrilateral &s, param(float2) t) {
	float	area1 = s.tri012().area();
	float	area2 = s.tri321().area();
	float	x	= t.x * (area1 + area2);
	return x < area1
		? uniform_interior(s.tri012(), float2{x / area1, t.y})
		: uniform_interior(s.tri321(), float2{(x - area1) / area2, t.y});
}


position2 quadrilateral::closest(param(position2) p) const {
	//TBD
	return centre();
}

position2 quadrilateral::support(param(float2) v) const {
	//TBD
	return centre();
}
bool		quadrilateral::ray_check(param(ray2) r) 	const {
	//TBD
	return false;
}
bool		quadrilateral::ray_check(param(ray2) r, float& t, vector2* normal) const {
	//TBD
	return false;
}
float quadrilateral::ray_closest(param(ray2) r) const {
	//TBD
	return 0;
}

//-----------------------------------------------------------------------------
// simplex2
//-----------------------------------------------------------------------------
/*
simplex2 simplex2::solve2(param(float2) v1, param(float2) v2) {
	float2	e12		= v2 - v1;
	float	d12_2	= dot(v1, e12);
	if (d12_2 > zero)
		return v1;				// w1 region

	float	d12_1	= dot(v2, e12);
	if (d12_1 <= zero)
		return v2;				// w2 region

	// Must be in e12 region
	return simplex2(concat(v1, d12_1 / (d12_1 - d12_2)), concat(v2, zero));
}

simplex2 simplex2::solve3(param(float2) v1, param(float2) v2, param(float2) v3) {
	// Edge12
	float2	e12		= v2 - v1;
	float	d12_1	= dot(v2, e12);
	float	d12_2	= dot(v1, e12);

	// Edge13
	float2	e13		= v3 - v1;
	float	d13_1	= dot(v3, e13);
	float	d13_2	= dot(v1, e13);

	// Edge23
	float2	e23		= v3 - v2;
	float	d23_1	= dot(v3, e23);
	float	d23_2	= dot(v2, e23);

	// Triangle123
	float	n123	= cross(e12, e13);
	float	d123_1	= n123 * cross(v2, v3);
	float	d123_2	= n123 * cross(v3, v1);
	float	d123_3	= n123 * cross(v1, v2);

	if (d12_2 > zero && d13_2 > zero)
		return v1;																	// w1 region

	if (d12_1 > zero && d12_2 <= zero && d123_3 <= zero)
		return simplex2(concat(v1, d12_1 / (d12_1 - d12_2)), concat(v2, zero));		// e12

	if (d13_1 > zero && d13_2 <= zero && d123_2 <= zero)
		return simplex2(concat(v1, d13_1 / (d13_1 - d13_2)), concat(v3, zero));		// e13

	if (d12_1 <= zero && d23_2 > zero)
		return v2;																	// w2 region

	if (d13_1 <= zero && d23_1 <= zero)
		return v3;																	// w3 region

	if (d23_1 > zero && d23_2 <= zero && d123_1 <= zero)
		return simplex2(concat(v3, -d23_2 / (d23_1 - d23_2)), concat(v2, zero));	// e23

	// Must be in triangle123
	float inv_d123 = 1 / (d123_1 + d123_2 + d123_3);
	return simplex2(concat(v1, d123_1 * inv_d123), concat(v2, d123_2 * inv_d123), concat(v3, zero));
}
*/
//-----------------------------------------------------------------------------
//
//						3D
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// line3
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// ray3
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// plane
//-----------------------------------------------------------------------------
/*
const plane& unit_cube_plane(CUBE_PLANE i) {
	static const float4 planes[] = {
		{-1,0,0,1},
		{+1,0,0,1},
		{0,-1,0,1},
		{0,+1,0,1},
		{0,0,-1,1},
		{0,0,+1,1},
	};
	return reinterpret_cast<const plane&>(planes[i]);
}
*/
float3x4 reflect(param(plane) p) {
	float4		v = as_vec(normalise(p));
	float3		t = v.xyz * 2;
	float3x4	m = identity;
	m.x -= t * v.x;
	m.y -= t * v.y;
	m.z -= t * v.z;
	m.w -= t * v.w;
	return m;
}

position3 intersect(param(plane) a, param(plane) b, param(plane) c) {
	float3	ab	= cross(a.normal(), b.normal());
	float3	bc	= cross(b.normal(), c.normal());
	float3	ca	= cross(c.normal(), a.normal());
	return position3((bc * a.dist() + ca * b.dist() + ab * c.dist()) / dot(a.normal(), bc));
}

bool coincident(param(plane) a, param(plane) b)	{
	auto av = as_vec(a), bv = as_vec(b);
	return all(av * rotate(bv) == rotate(av) * bv) && all(av.xyz * bv.zww == av.zww * bv.xyz);
}

//-----------------------------------------------------------------------------
// quadric
//-----------------------------------------------------------------------------

bool quadric::ray_check(param(ray3) r, float &t, vector3 *normal) {
	float4x4	Q		= *this;
	position3	p0		= r.p;
	float3		d0		= r.d;
	float4		p1		= Q * p0;
	float4		d1		= Q * d0;

	float		a		= dot(d0, d1.xyz);
	float		b		= dot(p0, d1) + dot(d0, p1.xyz);
	float		c		= dot(p0, p1);

	float		discr	= square(b) - a * c * four;
	if (discr < zero)
		return false;

	t = (-b - sqrt(discr)) / (a * two);

	if (normal) {
		float4	p = float4(r.from_parametric(t));
		*normal = float3{dot(p, column<0>()), dot(p, column<1>()), dot(p, column<2>())};
	}

	return true;
}

conic quadric::project_z() const {
	auto			d3	= diagonal<1>().d;
	float4x4		m	= float4x4(x_axis, y_axis, float4{-o.x, -d3.y, one, -d3.z} / d.z, w_axis);
	symmetrical4	s2	= mul_mulT(m, *this);
	return conic(swizzle<0,1,3>(s2));
}

cuboid quadric::get_box() const {
	auto	d3	= diagonal<1>().d;
	float3	t	= square(float3{d3.y, o.x, d3.x}) - d.yzx * d.zxy;
	return cuboid::with_centre(centre(), sqrt(t * det()) / swizzle<0,1,2>(*(symmetrical4*)this).det());
}

// tangent points to x, y, z axes given by M * [+-1,0,0], M * [0,+-1,0], M * [0,0,+-1]
float3x4 quadric::get_tangents() const {
	symmetrical3	s3	= swizzle<0,1,2>(*(symmetrical4*)this);
	symmetrical3	co	= cofactors(s3);
	float3			v	= sqrt(det() / -co.diagonal().d) / s3.det();
	return float3x4(
		co.column<0>() * v.x,
		co.column<1>() * v.y,
		co.column<2>() * v.z,
		centre()
	);
}

position3 quadric::support(param(float3) v) const {
	float3x3	m	= look_along_z(v);
	quadric		q	= *this / float4x4(m);
	auto		t	= q.get_tangents();
	return position3(m * (t.w + t.z));
}

quadric quadric::plane(const iso::plane &p1) {
	float4	p = as_vec(p1);
	return quadric(extend_left<4>(p.w), extend_left<3>(p.z * half), extend_left<3>(p.yx * half));
}

quadric quadric::plane_pair(const iso::plane &p1, const iso::plane &p2) {
	float4	p = as_vec(p1);
	float4	q = as_vec(p2);
	return quadric(p * q,
		(p.xyz * q.yzw + q.xyz * p.yzw) * half,
		(p.xyw * q.zwx + q.xyw * p.zwx) * half
	);
}

quadric quadric::sphere_through(const position3 &p1, const position3 &p2, const position3 &p3, const position3 &p4) {
	return sphere::through(p1, p2, p3, p4).matrix() * standard_sphere();
}


quadric quadric::ellipsoid_through(const position3 &p1, const position3 &p2, const position3 &p3, const position3 &p4, const position3 &p5) {
	TBD
	return {};
}
quadric quadric::ellipsoid_through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6) {
	TBD
	return {};
}
quadric quadric::ellipsoid_through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7) {
	TBD
	return {};
}
quadric quadric::ellipsoid_through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7, param(position3) p8) {
	TBD
	return {};
}
quadric quadric::through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7, param(position3) p8, param(position3) p9) {
	TBD
	return {};
}


quadric::info quadric::analyse() const {
	float	det			= swizzle<0,1,2>(*(symmetrical4*)this).det();
	bool	empty		= false;
	SIGN	orientation	= POS;
	bool	degenerate	= false;
	TYPE	type;

//	auto	d = diagonalise(*this);


	if (det < 0) {
		// det < 0: paraboloid, cone, hyperboloid1,2
		type = HYPERBOLOID1;

	} else if (det > 0) {
		// det > 0: ellipsoid, saddle
		type = ELLIPSOID;

	} else {
		// det == 0: cylinder,...
		type = CYLINDER;
	}
#if 0

	if (type == PARABOLA) {
		float3	d = d3, o = this->o;
		if (d.x == 0 && d.y == 0) {
			degenerate		= true;
			if (o.z == 0 && o.y == 0) {
				if (d.z == 0) {
					type		= TRIVIAL;
					orientation	= POS;
				} else {
					empty		= true;
					orientation	= get_sign1(d.z);
				}
			} else {
				orientation		= ZERO;
			}

		} else {
			if (d3.x == 0) {
				d = d.yxz;
				o = o.xzy;
			}

			if (degenerate = o.x * o.z == d.x * o.y) {
				float discr = o.y * o.y - d3.y * d3.z;
				if (discr < 0) {
					empty		= true;
					orientation = get_sign1(d.z);
				} else if (discr == 0) {
					orientation = get_sign1(d.x);
				} else {
					orientation = NEG;
				}
			} else {
				orientation = get_sign1(-d.x);
			}
		}

	} else {
		orientation	= get_sign1(conic_combine(det, *this, *this));
		degenerate	= orientation == ZERO;

		if (type == ELLIPSE) {
			empty		= get_sign1(d3.x) == orientation;
			degenerate	|= empty;
		}
	}
#endif
	return {type, orientation, empty, degenerate};
}

static struct test_quadric_params {
	test_quadric_params() {
		quadric_params<6>	q;

		struct vertex {
			position3	pos;
			float3		norm;
			float3		col;
		};
		vertex	v[] = {
			{{.5f,0,0}, {0,0,1}, {1,0,1}},
			{{-1,-1,1}, {0,0,1}, {0,0,1}},
			{{+1,-1,1}, {0,0,1}, {0,0,1}},
			{{+1,+1,1}, {0,0,1}, {0,0,1}},
			{{-1,+1,1}, {0,0,1}, {0,0,1}},
		};

		float	error;
		for (int i = 1; i < num_elements(v); i++) {
			auto	&v0 = v[0];
			auto	&v1 = v[i];
			auto	&v2 = v[i == num_elements(v) - 1 ? 1 : (i + 1)];

			float4x4	A	= q.add_triangle(v0.pos, v1.pos, v2.pos);
#if 1
			// normals
			auto	gN	= A * mat<float,4,3>(transpose(float3x3(v0.norm, v1.norm, v2.norm)), float3(zero));
			q.add_gradient(0, gN.x);	// gradient of x component of normal
			q.add_gradient(1, gN.y);	// gradient of y component of normal
			q.add_gradient(2, gN.z);	// gradient of z component of normal

											// colours
			auto	gC	= A * mat<float,4,3>(transpose(float3x3(v0.col, v1.col, v2.col)), float3(zero));
			q.add_gradient(3, gC.x);	// gradient of x component of colour
			q.add_gradient(4, gC.y);	// gradient of y component of colour
			q.add_gradient(5, gC.z);	// gradient of z component of colour
#endif

			float		params[6];
			for (int i = 0; i < 6; i++)
				params[i] = q.get_param(i, v0.pos);

			float	vals[6];
			store(v0.norm, vals + 0);
			store(v0.col, vals + 3);
			error = q.evaluate(v0.pos, vals);
		}

		position3	minv	= q.centre();
		float		params[6];
		for (int i = 0; i < 6; i++)
			params[i] = q.get_param(i, minv);

		float	errors[6];
		for (auto &i : v) {
			float	vals[6];
			store(i.norm, vals + 0);
			store(i.col, vals + 3);
			errors[&i - v] = q.evaluate(i.pos, vals);
		}

//		auto	e = q.error(position3(zero), vals);
//		auto	r = q.get_param(0, position3(zero));
	}
} test;

//-----------------------------------------------------------------------------
// circle3
//-----------------------------------------------------------------------------

circle3 operator&(param(sphere) a, param(plane) b) {
	float	d	= b.dist(a.centre());
	float	r2	= max(a.radius2() - d * d, zero);
	return circle3(b, circle::with_r2(position2((to_xy_plane(b) * a.centre()).v.xy), r2));
}

template<> bool is_visible(const circle3 &c, param(float4x4) m) {
	float4x4	m2	= m * to_xy_plane(c.pl);
	float3		t	= check_clip(m2 * position3(-c.circle::centre()));
	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);
	return all(t <= s * c.radius());
}

template<> bool is_within(const circle3 &c, param(float4x4) m) {
	float4x4	m2	= m * to_xy_plane(c.pl);
	float3		t	= check_clip(m2 * position3(-c.circle::centre()));
	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);
	return all(t <= s * -c.radius());
}

bool circle_through_contains(param(position3) p, param(position3) q, param(position3) r, param(position3) t) {
// compute side_of_bounded_sphere(p,q,r,t+v,t), with v = pq ^ pr.
	float3	pt	= p - t;
	float3	qt	= q - t;
	float3	rt	= r - t;
	float3	v	= cross(q - p, r - p);

	return float4x4(
		concat(pt, len2(pt)),
		concat(rt, len2(rt)),
		concat(qt, len2(qt)),
		concat(v, len2(v))
	).det() < 0;
}

//-----------------------------------------------------------------------------
// sphere
//-----------------------------------------------------------------------------

const unit_sphere_t unit_sphere;

int check_occlusion(const sphere &sa, const sphere &sb, const position3 &camera) {
	float3	ac	= sa.centre() - camera;
	float3	bc	= sb.centre() - camera;
#if 1
	float	ia	= rsqrt(len2(ac));
	float	ib	= rsqrt(len2(bc));

	float	k0	= dot(ac, bc) * ia * ib;
	float	k1	= sa.radius() * ia;
	float	k2	= sb.radius() * ib;

	float	t0	= k0 * k0 + k1 * k1 + k2 * k2;
	float	t1	= two * k0 * k1 * k2;

	return	t0 + t1 < one	? 0	// no overlap
		:	t0 - t1 < one	? 1	// partial overlap
		:	2;					// complete overlap
#else
	float	a2	= dot(ac, ac);
	float	b2	= dot(bc, bc);
	float	d	= dot(ac, bc);

	float	t0	= d * d + sa.radius2() * b2 + sb.radius2() * a2 - a2 * b2;
	float	t1	= two * d * sa.radius() * sb.radius();

	return	t0 + t1 < 0 ? 0	// no overlap
		:	t0 - t1 < 0 ? 1	// partial overlap
		:	2;				// complete overlap
#endif
}

conic projection(param(sphere) s, const float4x4 &m) {
	return ((m * s.matrix()) * quadric::standard_sphere()).project_z();
}

float projected_area(const sphere &s, const float4x4 &m) {
	float3	o	= float4(m * s.centre()).xyw;
	float	r2	= s.radius2();
	float	z2	= o.z * o.z;
	float	l2	= dot(o, o);

	return pi * r2 * sqrt(abs((l2 - r2) / (z2 - r2))) / (z2 - r2);
}

circle3 seen_from(param(sphere) s, param(position3) pos) {
	float3	d	= pos - s.centre();
	float	d2	= len2(d);
	float	id	= rsqrt(d2);
	float	R	= s.radius() * sqrt((d2 - s.radius2())) * id;
	float	D	= (d2 - s.radius2()) * id;
	float3	n	= d * id;
	return circle3(pos + D * n, n, R);
}


//-----------------------------------------------------------------------------
// ellipsoid
//-----------------------------------------------------------------------------

ellipsoid::ellipsoid(const quadric& q) {
	auto	s		= q.centre_form(c);
	float3	e		= get_eigenvalues(s);
	float3	lens	= rsqrt(abs(e));

	a1		= normalise(get_eigenvector(s, e.x)) * lens.x;
	a2r3	= concat(normalise(get_eigenvector(s, e.y)) * lens.y, lens.z);
}

bool ellipsoid::ray_check(param(ray3) r, float &t, vector3 *normal) const {
	float3x4	m = inv_matrix();
	bool ret = unit_sphere.ray_check(m * r, t, normal);
	if (ret && normal)
		*normal = transpose((float3x3)m) * *normal;
	return ret;
}

float ellipsoid::ray_closest(param(ray3) r) const {
	float3x4	m = inv_matrix();
	return unit_sphere.ray_closest(m * r);
}

position3 ellipsoid::closest(param(position3) p) const {
	float3x4	m		= matrix();
	float		len2maj	= len2(m.x);

	float3		pr		= p - get_trans(m);
	pr	= float3{dot(m.x, pr), dot(m.y, pr), dot(m.z, pr)} / len2maj;

	position2	cy = ellipse_closest(pr.xy, sqrt(len2maj / len2(m.y)));
	position2	cz = ellipse_closest(pr.xz, sqrt(len2maj / len2(m.z)));

	return m * position3(cy.v, cz.v.y);
}

position3 ellipsoid::support(param(float3) v) const {
#if 1
	auto		m	= look_along_z(v);
	quadric		q	= quadric(*this) / float4x4(m);
	auto		t	= q.get_tangents();
	return m * (get_trans(t) + t.z);
#else
	float3x4	m		= matrix();
	float		len2maj	= len2(m.x);

	float3		pr	= float3{dot(m.x, v), dot(m.y, v), dot(m.z, v)} / len2maj;

	position2	cy	= ellipse_closest(pr.xy, sqrt(len2maj / len2(m.y)));
	position2	cz	= ellipse_closest(pr.xz, sqrt(len2maj / len2(m.z)));

	return m * position3(cy.v, cz.v.y);
#endif
}

//-----------------------------------------------------------------------------
// triangle3
//-----------------------------------------------------------------------------

template<> bool is_visible(const triangle3 &t, param(float4x4) m) {
	float4x4	m2	= m * t.matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;

	return all(min(min(check_clip(p0), check_clip(p1)), check_clip(p2)) < zero);
}

template<> bool is_within(const triangle3 &t, param(float4x4) m) {
	float4x4	m2	= m * t.matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;

	return all(max(max(check_clip(p0), check_clip(p1)), check_clip(p2)) <= zero);
}

float dist(param(triangle3) t, param(iso::plane) p) {
	float3x4	m	= t.matrix();
	float	dx	= dot(p.normal(), m.x);
	float	dy	= dot(p.normal(), m.y);
	return p.dist(position3(m.z)) + min(min(dx, dy), zero);
}

interval<float>	triangle3::intersection(const line3 &line) const {
	float3x4	mat1	= matrix();
	float3x4	basis(line.dir(), cross(mat1.z, line.dir()), mat1.z, line.p);
	float3x4	mat2	= mat1 / basis;
	float3		da		= float3(mat2 * z);
	float3		db		= mat2 * x + da;
	float3		dc		= mat2 * y + da;

	interval<float>	r(none);

	if (da.y * db.y <= zero)
		r |= (da.x * db.y + db.x * da.y) / (da.y - db.y);
	if (db.y * dc.y <= zero)
		r |= (db.x * dc.y + dc.x * db.y) / (db.y - dc.y);
	if (dc.y * da.y <= zero)
		r |= (dc.x * da.y + da.x * dc.y) / (dc.y - da.y);
	return r;
}

bool intersect(const triangle3 &t0, const triangle3 &t1) {
	float3x3	e0(t0.x, t0.y, t0.x - t0.y);
	float3		n0	= cross(e0.x, e0.y);

	if (!t1.projection(n0).contains(dot(t0.z, n0)))
		return false;

	float3x3	e1(t1.x, t1.y, t1.x - t1.y);
	float3		n1	= cross(e1.x, e1.y);

	if (colinear(n0, n1)) {
		// Directions n0 x e0[i]
		for (int i = 0; i < 3; ++i) {
			float3	dir = cross(n0, e0[i]);
			if (!overlap(t0.projection(dir), t1.projection(dir)))
				return false;
		}

		// Directions n1 x e1[i]
		for (int i = 0; i < 3; ++i) {
			float3	dir = cross(n1, e1[i]);
			if (!overlap(t0.projection(dir), t1.projection(dir)))
				return false;
		}

	} else {
		if (!t0.projection(n1).contains(dot(t1.z, n1)))
			return false;

		// Directions e0[i] x e1[j]
		for (int j = 0; j < 3; ++j) {
			for (int i = 0; i < 3; ++i) {
				float3	dir = cross(e0[i], e1[j]);
				if (!overlap(t0.projection(dir), t1.projection(dir)))
					return false;
			}
		}
	}
	return true;
}

bool intersect(const triangle3 &t0, const triangle3 &t1, param(float3) velocity, float tmax) {
	interval<float>	time(0, tmax);

	float3x3	e0(t0.x, t0.y, t0.x - t0.y);
	float3		n0	= cross(e0.x, e0.y);

	if (!velocity_overlap(interval<float>(dot(t0.z, n0)), t1.projection(n0), dot(velocity, n0), time))
		return false;

	float3x3	e1(t1.x, t1.y, t1.x - t1.y);
	float3		n1	= cross(e1.x, e1.y);

	if (colinear(n0, n1)) {
		// Directions n0 x e0[i]
		for (int i = 0; i < 3; ++i) {
			float3	dir = cross(n0, e0[i]);
			if (!velocity_overlap(t0.projection(dir), t1.projection(dir), dot(velocity, dir), time))
				return false;
		}

		// Directions n1 x e1[i]
		for (int i = 0; i < 3; ++i) {
			float3	dir = cross(n1, e1[i]);
			if (!velocity_overlap(t0.projection(dir), t1.projection(dir), dot(velocity, dir), time))
				return false;
		}

	} else {
		if (!velocity_overlap(t0.projection(n1), interval<float>(dot(t1.z, n1)), dot(velocity, n1), time))
			return false;

		// Directions e0[i] x e1[j]
		for (int j = 0; j < 3; ++j) {
			for (int i = 0; i < 3; ++i) {
				float3	dir = cross(e0[i], e1[j]);
				if (!velocity_overlap(t0.projection(dir), t1.projection(dir), dot(velocity, dir), time))
					return false;
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// quadrilateral3
//-----------------------------------------------------------------------------

position3 uniform_perimeter(const quadrilateral3 &s, float v) {
	float	l0 = len(s[0] - s[1]), l1 = len(s[1] - s[2]), l2 = len(s[2] - s[3]), l3 = len(s[3] - s[0]);

	v *= l0 + l1 + l2 + l3;
	return	v < l0				? lerp(s[0], s[1], v / l0)
		:	v < l0 + l1			? lerp(s[1], s[2], (v - l0) / l1)
		:	v < l0 + l1 + l2	? lerp(s[2], s[3], (v - l0 - l1) / l2)
		:	lerp(s[3], s[0], (v - l0 - l1 - l2) / l3);
}

position3 uniform_interior(const quadrilateral3 &s, param(float2) t) {
	float	area1 = s.tri012().area();
	float	area2 = s.tri321().area();
	float	x	= t.x * (area1 + area2);
	return x < area1
		? uniform_interior(s.tri012(), float2{x / area1, t.y})
		: uniform_interior(s.tri321(), float2{(x - area1) / area2, t.y});
}

//-----------------------------------------------------------------------------
// tetrahedron
//-----------------------------------------------------------------------------

bool tetrahedron::ray_check(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float3x4	i	= inv_matrix();
	position3	p1	= i * p;
	float3		d1	= i * d;

	float4		p2	= concat(p1.v, one - reduce_add(p1.v));
	float4		d2	= concat(d1, -reduce_add(d1));

	if (all(p2 > zero)) {
		t = 0;
		return true;
	}

	float4	q	= -p2 / d2;
	float4	q0	= select(p2 < zero, q, zero);
	float	t0	= reduce_max(q0);
	float4	p3	= p2 + d2 * t0;
	p3 = select(q0 == t0, zero, p3);
	if (any(p3 < zero) || any(p3 > one))
		return false;

	t = t0;
	if (normal) {
		int a = max_component_index(q0);
		*normal = normalise(cross(column(dec_mod(a, 3)), column(inc_mod(a, 3))));
	}
	return true;
}

float tetrahedron::ray_closest(param(position3) p, param(vector3) d) const {
	//TBD
	return 0;
}

sphere tetrahedron::inscribe() const {
	float3		n0 = normalise(cross(z - x, y - x));
	float3		n1 = normalise(cross(y, z));
	float3		n2 = normalise(cross(z, x));
	float3		n3 = normalise(cross(x, y));
	float3x3	m(n1 - n0, n2 - n0, n3 - n0);
	float3		sol = inverse(m) * float3{zero, zero, -dot(n3, z)};
	return sphere(centre() + sol, abs(dot(n0, sol)));
}

sphere tetrahedron::circumscribe() const {
	float4	d{
		plane(0).normalised_dist(corner(0)),
		plane(1).normalised_dist(corner(1)),
		plane(2).normalised_dist(corner(2)),
		plane(3).normalised_dist(corner(3))
	};

	int	i = min_component_index(abs(d));
	using iso::circumscribe;
	return circumscribe(face(i));

//	float	d	= det();
//	vector3	r	= (cross(y, z) * len2(x) + cross(z, x) * len2(y) + cross(x, y) * len2(z)) / (d * two);
//	return sphere(w + r, len(r));
}

bool is_visible(const tetrahedron &t, param(float4x4) m) {
	float4x4	m2	= m * t.matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;
	float4		p3	= m2.w + m2.z;

	return all(min(min(min(check_clip(p0), check_clip(p1)), check_clip(p2)), check_clip(p3)) < zero);
}

bool is_within(const tetrahedron &t, param(float4x4) m) {
	float4x4	m2	= m * t.matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;
	float4		p3	= m2.w + m2.z;

	return all(max(max(max(check_clip(p0), check_clip(p1)), check_clip(p2)), check_clip(p3)) <= zero);
}

position3 tetrahedron::closest(param(position3) p) const {
	float4x4	m = planes_matrix();
	float4		a = transpose(m) * p;
	int			i = bit_mask(a < zero);

	switch (count_bits4(i)) {
		default: {	//inside
			int	j = min_component_index(a);
			return p - (m[j].xyz * a[j]) / len2(m[j].xyz);
		}
		case 1:	{	// outside one facet
			int	j = lowest_set_index4(i);
			position3	p2	= p - (m[j].xyz * a[j]) / len2(m[j].xyz);
			float4		a2	= transpose(m) * p2;
			int			i2	= bit_mask(a2 < zero) & ~i;
			if (i2 == 0)
				return p2;

			i |= i2;
			if (!is_pow2(i2))
				return corner(lowest_set_index4(i^15));

		}
		//fallthrough
		case 2:	{	// outside two facets - closest is along common edge
			i ^= 15;
			position3	p0	= corner(lowest_set_index4(i));
			position3	p1	= corner(lowest_set_index4(clear_lowest(i)));
			return ray3(p0, p1).closest(p);
		}
		case 3:	// outside three facets - closest is common point
			return corner(lowest_set_index4(i^15));
	}
}

position3 tetrahedron::support(param(float3) v) const {
	float3		a3 = v / inv_matrix();
	float4		a = concat(a3, one - reduce_add(a3));
	return corner(max_component_index(abs(a)));
}

position3 uniform_surface(param(tetrahedron) c, param(float2) t) {
	//TBD
	return position3(zero);
}
position3 uniform_interior(param(tetrahedron) c, param(float3) t) {
	// cut'n fold the cube into a prism
	float3	t2	= t.x + t.y > one ? concat(one - t.xy, t.z) : t;

	// cut'n fold the prism into a tetrahedron
	t2 =	t2.y + t2.z > one			? concat(t2.x, one - t2.z, one - t2.x - t2.y)
		:	t2.x + t2.y + t2.z > one	? concat(one - t2.y - t2.z, t2.y, t.x + t.y + t.z - one)
		:	t2;

	return c.from_parametric(t2);
}

//-----------------------------------------------------------------------------
// cuboid
//-----------------------------------------------------------------------------

position3	uniform_surface(param(cuboid) c, param(float2) t) {
	auto	v		= c.extent();
	auto	areas	= v.yzx * v.zxy;
	areas	/= reduce_add(areas);
	float	x	= t.x;
	float	y	= sign1(x - half);
	x		= frac(x * two);
	position3	t2
		= x < areas.x			? position3(y, t.y, x / areas.x)
		: x < areas.x + areas.y	? position3((x - areas.x) / areas.y, y, t.y)
		: position3(t.y, (x - areas.x - areas.y) / areas.z, y);
	return c.matrix() * t2;
}

const unit_cube_t	unit_cube;

bool unit_cube_is_visible(param(float4x4) m) {
	// a is max dot products of positive planes' normals
	float4	a	= abs(m.x - m.x.w) + abs(m.y - m.y.w) + abs(m.z - m.z.w);
	float4	t	= m.w - m.w.w;
	if (any(a.xyz < t.xyz))
		return false;

	// a is max dot products of negative planes' normals
	float4	b	= abs(m.x + m.x.w) + abs(m.y + m.y.w) + abs(m.z + m.z.w);
	float4	u	= -(m.w + m.w.w);
	return all(b.xyz >= u.xyz);
}

bool unit_cube_is_visible2(param(float4x4) m) {
	return unit_cube_is_visible(m) && unit_cube_is_visible(inverse(m));
}

int unit_cube_visibility_flags(param(float4x4) m, uint32 flags) {
	if (flags & (7<<0)) {
		// a is max dot products of positive planes' normals
		float4	a	= abs(m.x - m.x.w) + abs(m.y - m.y.w) + abs(m.z - m.z.w);
		float4	t	= m.w - m.w.w;		// plane distances from origin (anything less is out of frustum)
		if (any(a.xyz < t.xyz))
			return -1;

		flags &= (bit_mask(-a.xyz <= t.xyz) << 0) | ~(7 << 0);
	}

	if (flags & (7<<3)) {
		// a is max dot products of negative planes' normals
		float4	a	= abs(m.x + m.x.w) + abs(m.y + m.y.w) + abs(m.z + m.z.w);
		float4	t	= m.w + m.w.w;	// plane distances from origin (anything less is out of frustum)
		if (any(a.xyz < -t.xyz))
			return -1;

		flags &= (bit_mask(a.xyz >= t.xyz) << 3) | ~(7 << 3);
	}

	return flags;
}

int unit_cube_visibility_flags2(param(float4x4) m, uint32 flags) {
	flags = unit_cube_visibility_flags(m, flags);
	if (flags >= (63<<6)) {
		int	flags2 = unit_cube_visibility_flags(inverse(m), flags>>6);
		if (flags2 < 0)
			return flags2;
		flags = (flags & 63) | (flags2 << 6);
	}
	return flags;
}

bool unit_cube_is_within(param(float4x4) m) {
	return all(max(max(max(max(max(max(max(
		check_clip(m.w + m.x + m.y + m.z),
		check_clip(m.w - m.x + m.y + m.z)),
		check_clip(m.w + m.x - m.y + m.z)),
		check_clip(m.w - m.x - m.y + m.z)),
		check_clip(m.w + m.x + m.y - m.z)),
		check_clip(m.w - m.x + m.y - m.z)),
		check_clip(m.w + m.x - m.y - m.z)),
		check_clip(m.w - m.x - m.y - m.z)) <= zero);
}

//-----------------------------------------------------------------------------
// parallelepiped
//-----------------------------------------------------------------------------

/*
bool parallelepiped::ray_check(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float3x4	im = inv_matrix();
	position3	p1 = im * p;
	vector3		d1 = im * d;
	float2		t2;
	int ret = unit_cube.clip_test(p1, p1 + d1, t2);
	if (ret < 0)
		return false;
	if (normal) switch (ret) {
		case 0:	*normal = select(d1.x < 0, x, -x); break;
		case 1:	*normal = select(d1.y < 0, y, -y); break;
		case 2:	*normal = select(d1.z < 0, z, -z); break;
	}
	t = t2.x;
	return true;
}

bool parallelepiped::clip(position3 &p1, position3 &p2) const {
	float2		t;
	float3x4	im	= inv_matrix();
	int			ret	= unit_cube.clip_test(im * p1, im * p2, t);
	if (ret < 0)
		return false;

	float3		d = p2 - p1;
	p2	= p1 + d * t.y;
	p1	= p1 + d * t.x;
	return true;
}

position3 parallelepiped::closest(param(position3) p) const {
	float3		a	= float3(inv_matrix() * p);
	float3		as	= sign1(a);
	int			i	= bit_mask(a > one);

	switch (count_bits4(i)) {
		default: {	//inside
			int	j	= min_component_index(a);
			iso::plane	pl = iso::plane(rotate(float3{zero, zero, as[j]}, j), one);
			return pl.project(p);
		}
		case 1:	{	// outside one plane
			int	j	= lowest_set_index4(i);
			iso::plane	pl = iso::plane(rotate(float3{zero, zero, as[j]}, j), one);
			return pl.project(p);
		}
		case 2: {	// outside two planes - closest is along common edge
			int	j	= lowest_set_index4(i), k = lowest_set_index4(clear_lowest(i));
			position3	p0	= centre() + x[j] * as[j], p1 = p0 + x[k] * as[k];
			return ray3(p0, p1).closest(p);
		}
		case 3:	// outside two planes - closest is along common point
			return centre() + x * as.x + y * as.y + z * as.z;
	}
}

bool quick_test(param(parallelepiped) a, param(parallelepiped) b) {
	float3x4	m1	= a.inv_matrix() * b.matrix();
	vector3		a1	= abs(m1.x) + abs(m1.y) + abs(m1.z);
	if (any(abs(m1.w.xyz) - a1 > one))
		return false;

	float3x4	m2	= inverse(m1);//b.inv_matrix() * a.matrix();
	vector3		a2	= abs(m2.x) + abs(m2.y) + abs(m2.z);
	if (any(abs(m2.w.xyz) - a2 > one))
		return false;

	// check edges here
	return true;
}
*/

template<> position3 parallelepiped::closest(position3 p) const {
	float3		a	= float3(inv_matrix() * p);
	float3		as	= sign1(a);
	int			i	= bit_mask(a > one);

	switch (count_bits4(i)) {
		default: {	//inside
			int	j	= min_component_index(a);
			iso::plane	pl = iso::plane(rotate(float3{zero, zero, as[j]}, j), one);
			return pl.project(p);
		}
		case 1:	{	// outside one plane
			int	j	= lowest_set_index4(i);
			iso::plane	pl = iso::plane(rotate(float3{zero, zero, as[j]}, j), one);
			return pl.project(p);
		}
		case 2: {	// outside two planes - closest is along common edge
			int	j	= lowest_set_index4(i), k = lowest_set_index4(clear_lowest(i));
			position3	p0	= centre() + x[j] * as[j], p1 = p0 + x[k] * as[k];
			return ray3(p0, p1).closest(p);
		}
		case 3:	// outside two planes - closest is along common point
			return centre() + x * as.x + y * as.y + z * as.z;
	}
}


//-----------------------------------------------------------------------------
// obb3
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// frustum
//-----------------------------------------------------------------------------
/*
cuboid frustum::get_box() const {
	cuboid		c(empty);
	for (auto i : iso::corners<3>())
		c |= position3(corner(i));
	return c;
}

bool frustum::clip(vector4 &p1, vector4 &p2) const {
	float2		t;
	float4x4	im	= inv_matrix();
	int			ret	= unit_cube.clip_test(im * p1, im * p2, t);
	if (ret < 0)
		return false;

	float4		d	= p2 - p1;
	p2	= p1 + d * t.y;
	p1	= p1 + d * t.x;
	return true;
}

bool frustum::ray_check(param(position3) p, param(vector3) d) const {
	float4x4	im = inv_matrix();
	float2		t;
	return unit_cube.clip_test(im * p, im * position3(p + d), t) >= 0;
}

bool frustum::ray_check(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float4x4	im = inv_matrix();
	vector4		p1 = im * p;
	vector4		d1 = im * d;
	float2		t2;
	int			ret = unit_cube_clip_test(p1, p1 + d1, t2);
	if (ret < 0)
		return false;
	if (normal) switch (ret) {
		case 0:	*normal = select(d1.x < 0, x, -x); break;
		case 1:	*normal = select(d1.y < 0, y, -y); break;
		case 2:	*normal = select(d1.z < 0, z, -z); break;
	}
	t = t2.x;
	return true;
}
*/
//-----------------------------------------------------------------------------
// cylinder
//-----------------------------------------------------------------------------

bool cylinder::ray_check(param(ray3) r, float &t, vector3 *normal) const {
//	quadric	q = *this;
//	return q.ray_check(ray3(p, d), t, normal);

	vector3		v	= r.p - centre();
	float		h2	= len2(dir());
	float		ph	= dot(v, dir());
	float		dh	= dot(r.d, dir());

	// check caps
	if (dh) {
		float		th	= (-copysign(h2, dh) - ph) / dh;
		if (len2(cross(v + r.d * th, dir())) < square(radius()) * h2) {
			if (normal)
				*normal = normalise(dir() * sign1(-dh));
			t		= th;
			return true;
		}
	}

	vector3		p2	= v - dir() * (ph / h2);
	vector3		d2	= r.d - dir() * (dh / h2);

	float		c	= len2(p2) - square(radius());
	float		b	= dot(p2, d2);

	if (c > 0 && b >= 0)
		return false;

	float		a	= dot(d2, d2);
	float		y	= b * b - a * c;

	if (y < 0)
		return false;

	float		tc	= (-b - sqrt(y)) / a;

	if (tc > 1 || abs(ph + tc * dh) > abs(h2))
		return false;

	if (normal)
		*normal = normalise(p2 + d2 * tc);
	t = tc;
	return true;
}

float cylinder::ray_closest(param(ray3) r) const {
	//TBD
	return 0;
}

bool is_visible(const cylinder &c, param(float4x4) m) {
	float4x4	m2	= m * c.matrix();

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);

	float3		a	= check_clip(m2.z) + s;
	return all(check_clip(m2.w) < a);
}

bool is_within(const cylinder &c, param(float4x4) m) {
	float4x4	m2	= m * c.matrix();

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);

	float3		a	= check_clip(m2.z) + s;
	return all(check_clip(m2.w) < -a);
}

position3 cylinder::closest(param(position3) p) const {
	vector3		v	= p - centre();
	float		ph	= dot(v, dir()) / len2(dir());
	vector3		v1	= normalise(v - dir() * ph) * radius();
	return centre() + dir() * clamp(ph, -1, 1) + v1;
}

position3 cylinder::support(param(float3) v) const {
	float		ph	= dot(v, dir()) / len2(dir());
	vector3		v1	= normalise(v - dir() * ph) * radius();
	return centre() + dir() * sign1(ph) + v1;
}

bool cylinder::contains(param(position3) p) const {
	position3	c = inv_matrix() * p;
	return abs(c.v.z) < one && len2(c.v.xy) < one;
}

position3 uniform_surface(param(cylinder) c, param(float2) t) {
	float	rel_areas	= c.radius() / (c.radius() + len(c.d));
	if (t.y < rel_areas) {
		// caps
		return c.matrix() * position3(concat(unit_circle_uniform_interior<float>(float2{frac(t.x * two), t.y / rel_areas}), sign(t.x - half)));
	}
	return c.matrix() * position3(concat(unit_circle_uniform_perimeter<float>(t.y), (t.y - rel_areas) / (1 - rel_areas)) * two - one);
}
position3 uniform_interior(param(cylinder) c, param(float3) t) {
	return c.matrix() * position3(concat(unit_circle_uniform_interior<float>(t.xy), t.z * two - one));
}


//-----------------------------------------------------------------------------
// cone
//-----------------------------------------------------------------------------

bool cone::ray_check(param(ray3) r, float &t, vector3 *normal) const {
//	quadric	q = *this;
//	return q.ray_check(ray3(p, d), t, normal);

	vector3	v = r.p - centre() - dir();
	float	ph = dot(v, dir());
	float	dh = dot(r.d, dir());
	float	h2 = len2(dir());

	// check base
	if (dh) {
		float		th = (-h2 * 2 - ph) / dh;
		if (len2(cross(v + d * th, dir())) < square(radius()) * h2) {
			if (normal)
				*normal = normalise(dir() * sign1(-dh));
			t = th;
			return true;
		}
	}
	if (dot(v - dir(), dir()) > 0)
		return false;

	float	cos2 = h2 / (h2 + square(radius() * half));

	float	a = dh * dh - dot(r.d, r.d) * cos2;
	float	b = dh * ph - dot(r.d, v) * cos2;
	float	c = ph * ph - dot(v, v) * cos2;

	float	disc = b * b - a * c;
	if (disc < 0)
		return false;

	disc = sqrt(disc);
	float	t0 = (-b + disc) / a;
	float	t1 = (-b - disc) / a;

	if (min(t0, t1) > 1 || max(t0, t1) < 0)
		return false;

	float	h = ph + t0 * dh;
	if (h > 0 || h < -h2 * 2)
		return false;

	if (normal)
	    *normal = normalise(normalise(v + r.d * t) * h - dir());

	t = t0 < 0 ? t1 : min(t0, t1);
	return true;
}

float cone::ray_closest(param(ray3) r) const {
	//TBD
	return 0;
}

bool is_visible(const cone &c, param(float4x4) m) {
	float4x4	m2	= m * c.matrix();
	float4		a	= m2.w + m2.z;	// apex
	float4		b	= m2.w - m2.z;	// base

	//apex in frustum?
	if (all(check_clip(a) <= zero))
		return true;

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);

	//base in frustum?
	if (all(check_clip(b) <= s))
		return true;

	if (any((a.xyz > a.w) & (b.xyz - s > b.w)))
		return false;

	if (any((a.xyz < -a.w) & (b.xyz + s < b.w)))
		return false;

	return true;
}

bool is_within(const cone &c, param(float4x4) m) {
	float4x4	m2	= m * c.matrix();
	float4		a	= m2.w + m2.z;	// apex
	float4		b	= m2.w - m2.z;	// base

	//apex in frustum?
	if (any(check_clip(a) > zero))
		return false;

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);

	//base in frustum?
	return all(check_clip(b) <= -s);
}

//TBD
position3 cone::closest(param(position3) p) const {
	return centre();
}
position3 cone::support(param(float3) v) const {
	float	h2		= len2(dir());
	float	cos2	= h2 / (h2 + square(radius() * half));

	float	ph		= dot(v, dir()) / h2;
	if (square(ph) > cos2)
		return apex();

	vector3	v1		= normalise(v - dir() * ph) * radius();
	return centre() + dir() + v1;
}

bool cone::contains(param(position3) p) const {
	position3	c = inv_matrix() * p;
	return abs(c.v.z) < one && len2(c.v.xy) < (c.v.z + one) * half;
}
position3 uniform_surface(param(cone) c, param(float2) t) {
	//TBD
	return position3(zero);
}
position3 uniform_interior(param(cone) c, param(float3) t) {
	//TBD
	return position3(zero);
}

//-----------------------------------------------------------------------------
// capsule
//-----------------------------------------------------------------------------

bool capsule::ray_check(param(ray3) r, float &t, vector3 *normal) const {
	vector3		v	= r.p - centre();
	float		h2	= len2(dir());
	float		ph	= dot(v, dir());
	float		dh	= dot(r.d, dir());

	vector3		p2	= v   - dir() * (ph / h2);
	vector3		d2	= r.d - dir() * (dh / h2);

	float		c	= len2(p2) - square(radius());
	float		b	= dot(p2, d2);

	if (c > 0 && b >= 0)
		return false;

	float		a	= dot(d2, d2);
	float		y	= b * b - a * c;

	if (y < 0)
		return false;

	float		tc	= (-b - sqrt(y)) / a;

	if (tc > 1)
		return false;

	if (abs(ph + tc * dh) > abs(h2))
		return sphere(centre() + dir() * sign1(ph + tc * dh), radius()).ray_check(r, t, normal);

	if (normal)
		*normal = normalise(p2 + d2 * tc);
	t = tc;
	return true;
}

float capsule::ray_closest(param(ray3) r) const {
	vector3		v	= r.p - centre();
	float		h2	= len2(dir());
	float		ph	= dot(v, dir());
	float		dh	= dot(d, dir());

	vector3		p2	= v   - dir() * (ph / h2);
	vector3		d2	= r.d - dir() * (dh / h2);

	float		c	= len2(p2) - square(radius());
	float		b	= dot(p2, d2);

	float		a	= dot(d2, d2);
	float		y	= b * b - a * c;

	return y < 0 ? -b / a : (-b - sqrt(y)) / a;
}

bool is_visible(const capsule &c, param(float4x4) m) {
	float3x4	mat	= translate(c.centre()) * look_along_z(c.dir());
	float4x4	m2	= m * mat;

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	+	square(m2.z.xyz + m2.z.w)
	) * c.radius();

	float3		a	= check_clip(m2.z * len(c.dir())) + s;
	return all(check_clip(m2.w) < a);
}

bool is_within(const capsule &c, param(float4x4) m) {
	float3x4	mat	= translate(c.centre()) * look_along_z(c.dir());
	float4x4	m2	= m * mat;

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	+	square(m2.z.xyz + m2.z.w)
	) * c.radius();

	float3		a	= check_clip(m2.z * len(c.dir())) + s;
	return all(check_clip(m2.w) < -a);
}

position3 capsule::closest(param(position3) p) const {
	vector3		v	= p - centre();
	float		ph	= clamp(dot(v, dir()) / len2(dir()), -1, 1);
	return centre() + dir() * ph + normalise(v - dir() * ph) * radius();
}

position3 capsule::support(param(float3) v) const {
	return centre() + dir() * sign1(dot(v, dir())) + v * radius();
}

bool capsule::contains(param(position3) p) const {
	position3	c = inv_matrix() * p;
	c.v.z = min(abs(c.v.z) - one, zero);
	return len2(c.v.xy) < one;
}
position3 uniform_surface(param(capsule) c, param(float2) t) {
	float	rel_areas	= c.radius() / (c.radius() + len(c.d) * half);
	if (t.y < rel_areas) {
		position3	t2 = unit_sphere_uniform_surface<float>(concat(t.x, t.y / rel_areas));
		t2.v.z += sign(t2.v.z);
		return c.matrix() * t2;
	}
	return c.matrix() * position3(concat(unit_circle_uniform_perimeter<float>(t.x), (t.y - rel_areas) / (1 - rel_areas) * two - one));
}
position3 uniform_interior(param(capsule) c, param(float3) t) {
	float	rel_vols	= c.radius() / (c.radius() + len(c.d) * two);
	if (t.z < rel_vols) {
		position3	t2 = unit_sphere_uniform_interior<float>(concat(t.xy, t.z / rel_vols));
		t2.v.z += sign(t2.v.z);
		return c.matrix() * t2;
	}
	return c.matrix() * position3(concat(unit_circle_uniform_interior<float>(t.xy), (t.z - rel_vols) / (1 - rel_vols) * two - one));
}

//-----------------------------------------------------------------------------
// rectangles
//-----------------------------------------------------------------------------

int sub_rectangle(param(rectangle) r, rectangle *rects, int num, int max) {
	float4	b		= as_vec(r);
	int		num0	= num;
	for (int i = 0; i < num0; i++) {
		float4		a	= as_vec(rects[i]);
		if (bit_mask(a < b.zwxy) == 3 && all(a != b.zwxy)) {
			int	j		= i;
			if (a.y < b.y) {
				rects[j] = rectangle(concat(a.xyz, b.y));
				a.y = b.y;
				j	= num++;
			}
			if (a.w > b.w) {
				rects[j] = rectangle(swizzle<0,3,1,2>(a.xzw, b.w));
				a.w = b.w;
				j	= num++;
			}
			if (a.x < b.x) {
				rects[j] = rectangle(swizzle<0,1,3,2>(a.xyw, b.x));
				j	= num++;
			}
			if (a.z > b.z) {
				rects[j] = rectangle(swizzle<3,0,1,2>(a.yzw, b.z));
			} else {
				num--;
				if (j == i) {
					rects[i] = rects[num];
					if (num < num0) {
						num0 = num;
						i--;
					}
				}
			}
		}
	}
	return num;
}

int add_rectangle(param(rectangle) r, rectangle *rects, int num, int max, int add) {
	float4	b = as_vec(r);
	for (int i = 0, n = num; i < n;) {
		float4		&ra	= (float4&)rects[i++], a = ra;
		if (bit_mask(a < b.zwxy) == 3) {
			switch (bit_mask(a < b)) {
				case 0:
					add = add_rectangle(rectangle(concat(b.xyz, a.y)), rects + i, num - i, max - i, add) + i;
					b = swizzle<0,2,3,1>(b.xw, a.xy);
					break;
				case 1:	b.w = a.y; break;
				case 2:	b.z = a.x; break;
				case 3:	return add;
				case 4: ra.y = b.w; break;
				case 5:
					add = add_rectangle(rectangle(concat(b.xyz, a.y)), rects + i, num - i, max - i, add) + i;
					b	= concat(a.zy, b.zw);
					break;
				case 6:
					add = add_rectangle(rectangle(swizzle<0,1,3,2>(b.xyw, a.x)), rects + i, num - i, max - i, add) + i;
					b	= concat(a.z, b.yzw);
					break;
				case 7:	b.x = a.z; break;
				case 8:	ra.x = b.z; break;
				case 9:
					add = add_rectangle(rectangle(concat(b.xyz, a.y)), rects + i, num - i, max - i, add) + i;
					b	= swizzle<0,3,1,2>(b.xzw, a.w);
					break;
				case 10:
					add = add_rectangle(rectangle(swizzle<0,1,3,2>(b.xyw, a.x)), rects + i, num - i, max - i, add) + i;
					b	= concat(a.xy, b.zw);
					break;
				case 11: b.y = a.w; break;
				case 12:
					for (int j = i; j < add; j++)
						rects[j - 1] = rects[j];
					add--;
					n--;
					break;
				case 13: ra.z = b.x; break;
				case 14: ra.w = b.y; break;
				case 15:
					add = add_rectangle(rectangle(concat(a.z, b.zyw)), rects + i, num - i, max - i, add) + i;
					b	= swizzle<0,3,2,1>(b.xw, a.zw);
					break;
			}
		}
	}
	rects[add++] = rectangle(b);
	return add;
}

int opt_rectangles(rectangle *rects, int num, float within) {
#if 0
	int	num0 = num;
	num = 0;
	for (int i = 0; i < num0; i++) {
		rectangle	r	= rects[i];
		if (r.width() > 0 && r.height() > 0)
			rects[num++] = r;
	}

	for (int i = 0; i < num; i++) {
		rectangle	ri	= rects[i];
		for (int j = i + 1; j < num; j++) {
			rectangle	rj	= rects[j];
			ISO_ASSERT(!ri.overlaps_strict(rj));
		}
	}
#endif

	for (bool changed = true; changed;) {
		changed = false;
		for (int i = 0; i < num; i++) {
			rectangle	ri	= rects[i];
			float4		fi	= as_vec(ri);

			for (int j = i + 1; j < num; j++) {
				rectangle	rj	= rects[j];

				if (strict_overlap(ri, rj)) {
					rects[i] = ri |= rj;
					rects[j--] = rects[--num];
					changed	= true;

				} else {
					float4	fj		= as_vec(rj);
					int		mask1	= bit_mask(abs(fi - fj) <= within);
					if ((mask1 & 5) == 5)
						mask1 = 5;
					else if ((mask1 & 10) == 10)
						mask1 = 10;
					else
						continue;

					if (bit_mask(abs(fi - fj.zwxy) <= within) & ~mask1) {
						rects[i] = ri |= rj;
						rects[j--] = rects[--num];
						changed	= true;
					}
				}
			}
		}
	}

	return num;
}

}	//namespace iso
