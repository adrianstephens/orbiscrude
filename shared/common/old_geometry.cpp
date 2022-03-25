#ifndef NEW_VECTOR
#include "geometry.h"
#include "base/strings.h"

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
	float1 m = (d.x == zero).select(999.f, d.y / d.x);
	float1 b = pt0.y - m * pt0.x;
	return float2(m, b);
}

position2 intersect_lines(param(position2) pos0, param(float2) dir0, param(position2) pos1, param(float2) dir1) {
	float	d = cross(dir0, dir1);
	return d == 0
		? pos1
//		: (dir0 * cross(pos1, dir1) - dir1 * cross(pos0, dir0)) / d;
		: pos0 + dir0 * (cross(pos1 - pos0, dir1) / d);
}

bool line2::check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) const {
	float1	d0 = dist(p);
	float1	d1 = dist(p + d);
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		t = -d0 / (d1 - d0);
	} else if (d1 < zero) {
		t = -d1 / (d1 - d0);
	}
	if (normal)
		*normal = xy;
	return true;
}

bool line2::clip(position2 &p0, position2 &p1) const {
	float1	d0 = dist(p0);
	float1	d1 = dist(p0);
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		p0 -= (p1 - p0) * d0 / (d1 - d0);
	} else if (d1 < zero) {
		p1 -= (p1 - p0) * d1 / (d1 - d0);
	}
	return true;
}

line2 bisector(param(line2) a, param(line2) b) {
//	float1	d	= dot(a.as_float4(), b.as_float4());
	return line2(a.as_float3() * rlen(a.normal()) - b.as_float3() * rlen(b.normal()));
}

float2x3 reflect(param(line2) p) {
	float3		v = normalise(p).as_float3();
	float2		t = v.xy * 2;
	float2x3	m = identity;
	m.x -= t * v.x;
	m.y -= t * v.y;
	m.z -= t * v.z;
	return m;
}
//-----------------------------------------------------------------------------
// conic
//-----------------------------------------------------------------------------

float best_value(const float *values, int num_roots,
	float a2, float a1, float a0,				// quadratic coeffs
	float b3, float b2, float b1, float b0		// cubic coeffs
) {
	float max_det = 0, best = 0;
	for (int i = 0; i < num_roots; ++i) {
		float	x	= values[i];
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
	return best;
}

static float conic_combine(float a, const conic &c1, const conic &c2) {
	return	c1.d3.z * a
		-	c1.o.z * c1.o.z * c2.d3.y
		-	c1.o.y * c1.o.y * c2.d3.x
		+	c1.o.z * c1.o.y * c2.o.x * two;
}

SIGN conic::vol_derivative(const conic &c2) const {
	float	a1	= d3.x * c2.d3.y + c2.d3.x * d3.y - two * o.x * c2.o.x;
	float	a0	= det2x2();

	float	b1	= a0 * c2.d3.z
				- two * (o.z * c2.o.z * d3.y + o.y * c2.o.y * d3.x)
				+ conic_combine(a1, *this, c2)
				+ two * (o.z * c2.o.y + c2.o.z * o.y) * o.x;
	float	b0	= conic_combine(a0, *this, *this);

	float	c0	= -two * a0 * b1 + 3 * a1 * b0;

	return get_sign1(-c0);
}

float conic::vol_minimum(const conic &b) const {
	float	a2	= b.det2x2();
	float	a1	= d3.x * b.d3.y + b.d3.x * d3.y - two * o.x * b.o.x;
	float	a0	= det2x2();

	float	b3	= conic_combine(a2, b, b);
	float	b2	= a2 * d3.z
				- two * (o.z * b.o.z * b.d3.y + o.y * b.o.y * b.d3.x)
				+ conic_combine(a1, b, *this)
				+ two * (o.z * b.o.y + b.o.z * o.y) * b.o.x;
	float	b1	= a0 * b.d3.z
				- two * (o.z * b.o.z * d3.y + o.y * b.o.y * d3.x)
				+ conic_combine(a1, *this, b)
				+ two * (o.z * b.o.y + b.o.z * o.y) * o.x;
	float	b0	= conic_combine(a0, *this, *this);

	polynomial<3>	cubic = float4(
		-two * a0 * b1 + 3 * a1 * b0,
		-four * a0 * b2 + a1 * b1 + 6 * a2 * b0,
		-6 * a0 * b3 - a1 * b2 + four * a2 * b1,
		-3 * a1 * b3 + two * a2 * b2
	);
	float3	roots;
	int		num_roots = cubic.roots(roots);
	return best_value((float*)&roots, abs(num_roots), a2, a1, a0, b2, b2, b1, b0);
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

	float3 sqr(dot(p1, p1), dot(p2, p2), dot(p3, p3));

	return conic(
		float3(
			det,
			det,
			dot(float3(cross(p3, p2), cross(p1, p3), cross(p2, p1)), sqr)
		),
		float3(
			zero,
			dot(float3(p2.x - p3.x, p3.x - p1.x, p1.x - p2.x), sqr) * half,	//o.y
			dot(float3(p3.y - p2.y, p1.y - p3.y, p2.y - p1.y), sqr) * half	//o.z
		)
	);
}

conic conic::line_pair(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4) {
	ISO_ASSERT(p1 != p2 && p3 != p4);

	float2	d12		= p2 - p1;
	float2	d34		= p4 - p3;
	float	cross12	= cross(p1, p2);
	float	cross34	= cross(p3, p4);

	return conic(
		float3(
			d12.y * d34.y,
			d12.x * d34.x,
			cross12 * cross34
		),
		float3(
			-(d12.x * d34.y + d12.y * d34.x),
			cross12 * d34.x + d12.x * cross34,
			-(cross12 * d34.y + d12.y * cross34)
		) * half
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

conic conic::ellipse_through(const position2 &p1, const position2 &p2, const position2 &p3) {
	// precondition: p1, p2, p3 not colinear
	float det = cross(p1 - p3, p2 - p3);
	ISO_ASSERT(det != 0);

	float3	x(p1.x, p2.x, p3.x), y(p1.y, p2.y, p3.y);

	float2	dyx	= p1 * (p1 - p2 - p3) + p2 * (p2 - p3) + p3 * p3;
	float3	d	= float3(dyx.yx,
		cross(p2, p1 * p3.x + p3 * p2.x) * p1.y
	+	cross(p3, p1 * p3.x + p2 * p1.x) * p2.y
	+	cross(p1, p2 * p1.x + p3 * p2.x) * p3.y
	);

	float3	t(
		cross(p1, p2 + p3),
		cross(p2, p1 + p3),
		cross(p3, p1 + p2)
	);

	float2	oyz	= p1 * t.x + p2 * t.y + p3 * t.z;
	float3	o	= float3(
		p1.x * (p2.y + p3.y - two * p1.y) + p2.x * (p3.y + p1.y - two * p2.y) + p3.x * (p1.y + p2.y - two * p3.y),
		-oyz.x, oyz.y
//		-(x.x * p1.x + x.y * p2.x + x.z * p3.x),
//		 x.x * p1.y + x.y * p2.y + x.z * p3.y
	);

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
	return lerp(c1, c2, c1.vol_minimum(c2 - c1));
}

conic conic::line(const line2 &p1) {
	return conic(float3(zero, zero, p1.z), float3(zero, p1.yx * half));
}

conic conic::line_pair(const line2 &p1, const line2 &p2) {
	return conic(p1.xyz * p2.xyz, (p1.xyz * p2.yzx + p2.xyz * p1.yzx) * half);
}

conic conic::from_matrix(const float3x3 &m) {
	float3	x2	= square(m.x), y2 = square(m.y), z2 = square(m.z);
	float3	d(
		x2.x + x2.y - x2.z,
		y2.x + y2.y - y2.z,
		z2.x + z2.y - z2.z
	);

	float3	xy	= m.x * m.y, xz = m.x * m.z, yz = m.y * m.z;
	float3	o(
		xy.x + xy.y - xy.z,
		yz.x + yz.y - yz.z,
		xz.x + xz.y - xz.z
	);
	return conic(d, o / 4);
}

conic::info conic::analyse() const {
	float	det		= det2x2();
	TYPE	type	= (TYPE)get_sign1(det);
	bool	empty	= false;
	SIGN	orientation;
	bool	degenerate;

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
	return info(type, orientation, empty, degenerate);
}

bool conic::check_ray(param(ray2) r, float &t, vector2 *normal)	{
	float3x3	Q		= *this;
	float2		p1		= (Q * float3(r.p, one)).xy;
	float2		d1		= (Q * float3(r.d, zero)).xy;
	float1		dp		= dot(r.d, p1);
	float1		dd		= dot(r.d, d1);
	float1		discr	= square(dp) - dd * dot(r.p, p1);
	if (discr < zero)
		return false;
	t = (-dp + sqrt(discr)) / dd;
	if (normal) {
		float3	p = float3(r.from_parametric(t), one);
		*normal = float2(dot(p, Q.x), dot(p, Q.y));
	}
	return true;
}

rectangle conic::get_rect() const {
	return rectangle::centre_half(centre(), sqrt(-d3.yx * det()) / det2x2());
}

// tangent points to x, y axes given by M * [+-1,0], M * [0,+-1]
float2x3 conic::get_tangents() const {
	float2	v	= sqrt(det() / -d3.yx) / det2x2();
	return float2x3(
		float2(d3.y, -o.x) * v.x,
		float2(-o.x, d3.x) * v.y,
		centre()
	);
}

position2 conic::support(param(float2) v) const {
	float2x2	m	= _rotate(normalise(v));
	conic		c	= *this / float3x3(m);
	auto		t	= c.get_tangents();
	return m * (t.translation() + t.x);
}

//-----------------------------------------------------------------------------
// ray2
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// rectangle
//-----------------------------------------------------------------------------
const _unit_rect	unit_rect;

bool _unit_rect::clip_test(param(position2) a, param(position2) b) {
	position2	m		= a + b;
	vector2		d		= b - a;
	vector2		absd	= abs(d);
//	float2		two(2);
	return all(abs(m) <= absd + two)
		&& abs(cross(m, d)) <= (absd.y + absd.x) * two;
}

int _unit_rect::clip_test(param(position2) a, param(position2) b, float2 &t) {
	if (any(min(a, b) > one) || any(max(a, b) < -one))			//entirely outside box
		return -1;

	vector2	d	= a - b;
	vector2	q	= one / d;
	vector2	va	= q + a * q;
	vector2	vb	= q - a * q;
	vector2	vt0	= (d == zero).select(zero, (d > zero).select(vb, va));
	vector2	vt1	= (d == zero).select(one,  (d > zero).select(va, vb));

	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);

	if (t0 > t1)
		return -1;

	t = float2(t0, t1);
	return max_component_index(vt0);
}

int _unit_rect::clip_test(param(vector3) a, param(vector3) b, float2 &t) {
	if (any(a.xy >  a.z & b.xy >  b.z)
	||  any(a.xy < -a.z & b.xy < -b.z)
	)
		return -1;

	float3	d	= a - b;
	float2	va	= (a.xy + a.z) / (d.xy + d.z);
	float2	vb	= (a.xy - a.z) / (d.xy - d.z);
	vector2	vt0	= (a.xy < -a.z).select(va, (a.xy > a.z).select(vb, zero));
	vector2	vt1	= (b.xy < -b.z).select(va, (b.xy > b.z).select(vb, one));

	float1	t0	= max_component(vt0);
	float1	t1	= min_component(vt1);

	if (t0 > t1)
		return -1;

	t = float2(t0, t1);
	return max_component_index(vt0);
}

position2 _unit_rect::uniform_perimeter(float x)	{
	float	s = floor(x * 4);
	int		i = int(s);
	x = (x - s - .5f) * 2;
	return i & 1
		? position2(i & 2 ? 1 : -1, x)
		: position2(x, i & 2 ? 1 : -1);
}

//	http://vcg.isti.cnr.it/publications/papers/quadrendering.pdf
float4 mean_value_coords(param(float4) x, param(float4) y) {
	float4	A	= x * y.yzwx - y * x.yzwx;	//	s[i] x s[i+1]
	float4	D	= x * x.yzwx + y * y.yzwx;	//	s[i] . s[i+1]
	float4	r	= sqrt(x * x + y * y);		//	|s[i]|

	float4	rA	= A.yzwx * A.zwxy * A.wxyz;		//	avoid dividing by A by multiplying by all other A's
	float4	t	= (r * r.yzwx - D) * rA;		//	(r[i] r[i+1] - D[i]) / A[i]

	float4	mu	= select(r != zero, (t + t.wxyz) / r, one);	//	(t[i-1] + t[i]) / r[i]
	return mu / sum_components(mu);
}

float4 _unit_rect::barycentric(param(float2) p) {
	return mean_value_coords(float4(-1,+1,+1,-1) - p.x, float4(-1,-1,+1,+1) - p.y);
}

bool rectangle::check_ray(param(position2) p, param(vector2) d) const {
	vector2		e		= extent();
	position2	m		= p + p + d - xy - zw;
	vector2		absd	= abs(d);
	return all(abs(m) <= e + absd) && abs(cross(m, d)) <= sum_components(e * absd.yx);
}

bool rectangle::check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) const {
	if (any(min(p, p + d) > zw) || any(max(p, p + d) < xy))			//entirely outside box
		return false;

	vector2	q	= one / d;
	vector4	v	= (xyzw - (p,p)) * (q,q);
	vector2	vt0	= (d == zero).select(zero, (d < zero).select(v.zw, v.xy));
	vector2	vt1	= (d == zero).select(one,  (d < zero).select(v.xy, v.zw));
	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);
	if (t0 > t1)
		return false;
	t = t0;
	if (normal) switch (max_component_index(vt0)) {
		case 0:	*normal = vector2(select(d.x < 0, one, -one), zero); break;
		case 1:	*normal = vector2(zero, select(d.y < 0, one, -one)); break;
	}
	return true;
}

bool rectangle::clip(position2 &p1, position2 &p2) const {
	if (any(min(p1, p2) > zw) || any(max(p1, p2) < xy))			//entirely outside box
		return false;

	vector2	d	= p2 - p1;
	vector2	q	= one / d;
	vector4	v	= (xyzw - (p1,p1)) * (q,q);
	vector2	vt0	= (d == zero).select(zero, (d < zero).select(v.zw, v.xy));
	vector2	vt1	= (d == zero).select(one,  (d < zero).select(v.xy, v.zw));
	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);
	if (t0 > t1)
		return false;
	p2	= p1 + d * t1;
	p1	= p1 + d * t0;
	return true;
}

position2 rectangle::uniform_perimeter(float x) const {
	float2	e = extent();
	x	*= (sum_components(e) * two);
	if (x < e.x)
		return pt0() + float2(x, 0);

	x -= e.x;
	if (x < e.y)
		return pt0() + float2(0, x);

	x -= e.y;
	if (x < e.x)
		return pt1() - float2(x, 0);

	x -= e.x;
	return pt1() - float2(0, x);
}

//-----------------------------------------------------------------------------
// parallelogram
//-----------------------------------------------------------------------------

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
	float2	e = scale();
	float	s = sum_components(e);

	v *= s * 2;
	bool	a = v > s;
	if (a)
		v -= s;

	return matrix() * (v > e.x ? position2(a ? 1 : -1, (v - e.x) / e.y) : position2(v / e.x, a ? 1 : -1));
}

position2 parallelogram::closest(param(position2) p) const {
	position2	a = inv_matrix() * p;
	return matrix() * position2(clamp(a, -one, one));
	int			i = (abs(a) > one).mask();

	switch (i) {
		default:	//inside
			return a.x < a.y
				? ray2::centre_half(z + x * sign1(a.x), y).closest(p)
				: ray2::centre_half(z + y * sign1(a.y), x).closest(p);
		case 1:	// outside vertical edge
			return ray2::centre_half(z + x * sign1(a.x), y).closest(p);
		case 2:	// outside horizontal edge
			return ray2::centre_half(z + y * sign1(a.y), x).closest(p);
		case 3:	// outside two edges - closest is along common point
			return z + x * sign1(a.x) + y * sign1(a.y);
	}
}

//-----------------------------------------------------------------------------
// obb2
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// circle
//-----------------------------------------------------------------------------
const _unit_circle unit_circle;

bool _unit_circle::check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) {
	float1		b = dot(d, p);
	if (b < zero)			// closest point is 'before' p
		return false;

	float1		c = dot(p, p) - one;
	if (c < zero)			// p is inside circle
		return false;

	float1		a = dot(d, d);
	float1		x = b * b - a * c;
	if (x < zero)			// line misses circle
		return false;

	x = b - sqrt(x);
	if (x < zero || x > a)
		return false;		// intersection of line is off ends of segment

	t = x / a;
	if (normal)
		*normal = (d * t - p);

	return true;
}

circle circle::through(param(position2) a, param(position2) b) {
	vector2		r	= (b - a) * half;
	return circle::with_r2(a + r, len2(r));
}

circle circle::through(param(position2) a, param(position2) b, param(position2) c) {
	vector2	x	= b - a;
	vector2	y	= c - a;
	float2	r	= (x * len2(y) - y * len2(x)) / (cross(x, y) * two);
	return circle::with_r2(a + perp(r), len2(r));
}

circle circle::small(const position2 *p, uint32 n) {
	position2	c	= centroid(p, p + n);
	float1		r2	= 0;
	for (uint32 i = 0; i < n; i++)
		r2 = max(r2, len2(c - p[i]));
	return circle::with_r2(c, r2);
}

circle circle::operator|(param(position2) p) const {
	if (is_empty())
		return circle(p, zero);

	vector2	v	= p - centre();
	float1	d2	= len2(v);
	if (d2 < radius2())
		return *this;

	float1	d	= sqrt(d2);
	float1	r	= radius();
	return circle(centre() + v * ((d - r) / d * half), (d + r) * half);
}

circle circle::operator|(param(circle) b) const {
	vector2 v	= b.centre() - centre();
	float1	d	= len(v);
	float1	r0	= radius();
	float1	r1	= b.radius();
	float1	r	= (max(r0, d + r1) - min(-r0, d - r1)) * half;
	return circle(centre() + v * ((r - r0) / d), r);
}

bool circle::check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) const {
	vector2		v = centre() - p;

	float1		b = dot(d, v);
	if (b < zero)			// closest point is 'before' p
		return false;

	float1		c = dot(v, v) - radius2();
	if (c < zero)			// p is inside circle
		return false;

	float1		a = dot(d, d);
	float1		x = b * b - a * c;
	if (x < zero)			// line misses circle
		return false;

	x = b - sqrt(x);
	if (x < zero || x > a)
		return false;		// intersection of line is off ends of segment

	t = x / a;
	if (normal)
		*normal = (d * t - v) / radius();

	return true;
}

bool circle::through_contains(param(position2) p, param(position2) q, param(position2) r, param(position2) t) {
	float2 qp = q - p;
	float2 rp = r - p;
	float2 tp = t - p;
	return float2x2(float4(
		cross(qp, tp), dot(tp, t - q),
		cross(qp, rp), dot(rp, r - q)
	)).det() < 0;
}

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
	float4	t		= m.v * m.v;
	float2	sc2		= normalise(float2(t.x + t.y - t.z - t.w, dot(m.x, m.y) * two));
	float2	sc		= sincos_half(clamp(sc2, -one, one));

	float2	axis1	= m * sc;
	float2	axis2	= m * perp(sc);
	v		= float4(m.z, axis1);
	ratio	= sqrt(len2(axis2) / len2(axis1));
#endif
}

ellipse::ellipse(const conic &c) {
	position2	centre	= c.centre();
#if 1
	float	k		= -c.evaluate(centre);
	float3	m		= float3(c.d3.xy, c.o.x) / k;
	float	d		= sqrt(square(m.x - m.y) + square(m.z) * 4);
	float2	e		= (float2(1, -1) * d + m.x + m.y) / 2;
	float2	major	= normalise(m.y > m.x ? float2(e.y - m.y, m.z) : float2(m.z, e.y - m.x));

	v		= float4(centre, major * rsqrt(e.y));
	ratio	= sqrt(e.y / e.x);

#elif 1
	float	k		= -c.det() / c.det2x2();		//== c.evaluate(centre);
	float2	e;
	int		n		= polynomial_roots1(c.characteristic2x2(), e);//X = float2(det2x2(), -trace2x2())
	float2	a1		= normalise(float2(e.y - c.d3.y, c.o.x)) * sqrt(k / e.y);
	v		= float4(centre, a1);
	ratio	= sqrt(e.y / e.x);
#else
	float	w2		= c.evaluate(centre);
	float2	sc2		= float2(c.d3.x - c.d3.y, two * c.o.x);
	float	x		= len(sc2);
	float2	sc		= sincos_half(clamp(sc2 / x, -one, one));
	float2	axes	= sqrt(-two * w2 / float2(c.d3.x + c.d3.y + x, c.d3.x + c.d3.y - x));

	ISO_TRACEF("w2 = ") << w2 << ", k=" << k << ", x=" << x << '\n';

	v		= float4(centre, sc * axes.x);
	ratio	= axes.y / axes.x;
#endif
}

ellipse ellipse::through(param(position2) a, param(position2) b) {
	vector2		r	= (b - a) * half;
	return ellipse(a + r, a, zero);
}

ellipse ellipse::through(param(position2) p1, param(position2) p2, param(position2) p3) {
	float2	d1 = p1 - p3, d2 = p2 - p3;
	float det = cross(d1, d2);
	if (det)
		return conic::ellipse_through(p1, p2, p3);

	if (dot(d1, d2) < zero)
		return through(p1, p2);

	if (len2(d1) > len2(d2))
		return through(p1, p3);

	return through(p2, p3);
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

bool ellipse::check_ray(param(ray2) r, float &t, vector2 *normal) const {
	float2x3	m = inv_matrix();
	bool ret = _unit_circle::check_ray(m * r, t, normal);
	if (ret && normal)
		*normal = transpose((float2x2)m) * *normal;
	return ret;
}

#if 1

position2 ellipse_closest(param(float2) p0, float ratio) {
	//http://wet-robots.ghost.io/simple-method-for-distance-to-ellipse/
	float2	a	= float2(1, ratio);
	float2	ec	= float2(1 - square(ratio), (square(ratio) - 1) / ratio);
	float2	p	= abs(p0);

	float2	d	= float2(rsqrt2, rsqrt2);
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
	return copysign(a * d, p0);
}

#else

template<typename T> float2 plus_minus(T t) { return float2(t, -t); }

//http://www.iquilezles.org/www/articles/ellipsedist/ellipsedist.htm
float2 ellipse_closest(param(float2) p, float ratio) {
	float2	ab		= float2(1, ratio);
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
		float2	r	= float2(-t.x - t.y - c * 4 + m2.x * 2, (t.x - t.y) * sqrt3);
		float	rm	= len(r);
		co = (r.y / sqrt(rm - r.x) + g * 2 / rm - m.x) / 2;
	}

	float2	c2 = ab * float2(co, sqrt(one - co * co));
	if (swap)
		c2 = c2.yx;
	return copysign(c2, p);
}
#endif

position2 ellipse::closest(param(position2) p) const {
	float2		v	= p - centre();
	float2		maj = major();
	float2		c2 = ellipse_closest(float2(dot(maj, v), cross(maj, v)) / len2(maj), ratio);
	// back to original space
	return (maj * c2.x + perp(maj) * c2.y) + centre();
}

position2 ellipse::support(param(float2) v) const {
	float2x2	m	= _rotate(normalise(v));
	conic		c	= conic(*this) / float3x3(m);
	auto		t	= c.get_tangents();
	return m * (t.translation() + t.x);

	float2		maj = major();
	float2		c2 = ellipse_closest(float2(dot(maj, v), cross(maj, v)) * rlen(maj), ratio);
	// back to original space
	return (maj * c2.x + perp(maj) * c2.y) + centre();
}

float1 ellipse::perimeter() const {
	float	h = square((1 - ratio) / (1 + ratio));
	//pade 2/1
	return pi * 4 * len(major()) * (1 + ratio) * (64 - 3 * h * h) / (64 - 16 * h);
}

//WRONG! TBD
position2 ellipse::uniform_perimeter(float x) const {
	return matrix() * unit_circle.uniform_perimeter(x);
}

//-----------------------------------------------------------------------------
// triangle
//-----------------------------------------------------------------------------

const _unit_tri unit_tri;
const fixed_array<position2, 3>	_unit_tri::_corners = { zero, x_axis, y_axis };

bool _unit_tri::check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) {
	float3	b0	= barycentric(p), b1 = barycentric(p + d);
	if (any(max(b0, b1) < zero))
		return false;

	float3	t3 = -b0 / (b1 - b0);
	t = min_component(t3);

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
	int		i	= (v > zero).mask();
	if (i == 3)
		i = v.x < v.y ? 2 : 1;
	return corner(i);
}

bool triangle::check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) const {
	float3	b0	= barycentric(p), b1 = barycentric(p + d);
	if (any(max(b0, b1) < zero))
		return false;

	float3	t3 = -b0 / (b1 - b0);
	t = min_component(t3);

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

position2 triangle::uniform_perimeter(float v) const {
	float	l0 = len(x), l1 = len(y), l2 = len(y - x);
	v *= l0 + l1 + l2;
	return	v < l0		? position2(z + x * (v / l0))
		:	v < l0 + l1	? position2(z + y * ((v - l0) / l1))
		:	position2(z + x + (y - x) * ((v - l0 - l1) / l2));
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
	return from_parametric(float2(A & 0xffff, A >> 16) / s);
	//	return (A + B + C) / (3 << 16);
}


position2 triangle::closest(param(position2) p) const {
	float3	a = barycentric(p);
	int		i = (a < zero).mask();
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
	int		i	= (a2 > zero).mask();
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
	return (r1 & r2) * half;
}

float4 quadrilateral::barycentric(param(float2) p) const {
	return mean_value_coords(float4(p01.xz, p23.zx) - p.x, float4(p01.yw, p23.wy) - p.y);
}

float3x3 quadrilateral::inv_matrix() const {
	static const float3x3	matP(
		float3( one,	-one,	one),
		float3(-one,	 one,	one),
		float3(-two,	-two,	two)
	);
	float3x3	matC(
		float3(p01.zw,		one),
		float3(p23.xy,		one),
		float3(p01.xy * 2,	2)
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
		float3(half,	zero,	-quarter),
		float3(zero,	half,	-quarter),
		float3(half,	half,	zero)
	);
	float3x3	matC(
		float3(p01.zw,		one),
		float3(p23.xy,		one),
		float3(p01.xy * 2,	2)
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

position2 quadrilateral::uniform_perimeter(float v) const {
	float	l0 = len(p01.xy - p01.zw), l1 = len(p01.zw - p23.xy), l2 = len(p23.xy - p23.zw), l3 = len(p23.zw - p01.xy);

	v *= l0 + l1 + l2 + l3;
	return	v < l0				? position2(p01.xy + (p01.zw - p01.xy) * (v / l0))
		:	v < l0 + l1			? position2(p01.zw + (p23.xy - p01.zx) * ((v - l0) / l1))
		:	v < l0 + l1 + l2	? position2(p23.xy + (p23.zw - p23.xy) * ((v - l0 - l1) / l2))
		:	position2(p23.zw + (p01.xy - p23.zw) * ((v - l0 - l1 - l2) / l3));
}

position2 quadrilateral::uniform_interior(param(float2) t)	const {
	float	area1 = diff(p01.xy * p01.wz + p01.zw * p23.wz + p23.zw * p01.yx);
	float	area2 = diff(p01.xy * p23.wz + p23.zw * p23.yx + p23.xy * p01.yx);
	float	x	= t.x * (area1 + area2);
	return x < area1
		? triangle(pt0(), pt1(), pt3()).uniform_interior(float2(x / area1, t.y))
		: triangle(pt0(), pt3(), pt2()).uniform_interior(float2((x - area1) / area2, t.y));
}


position2 quadrilateral::closest(param(position2) p) const {
	//TBD
	return centre();
}

position2 quadrilateral::support(param(float2) v) const {
	//TBD
	return centre();
}

//-----------------------------------------------------------------------------
//
//						3D
//
//-----------------------------------------------------------------------------

// return -ve where p is inside unit cube
inline float3 check_clip(param(float4) p) {
	float4	a = abs(p);
	return a.xyz - a.w;
}

//-----------------------------------------------------------------------------
// line3
//-----------------------------------------------------------------------------

float2 closest_params(param(line3) r1, param(line3) r2) {
	float3	w = r1.p - r2.p;
	float1	a = dot(r1.d, r1.d);
	float1	b = dot(r1.d, r2.d);
	float1	c = dot(r2.d, r2.d);
	float1	d = dot(r1.d, w);
	float1	e = dot(r2.d, w);
	float1	D = a * c - b * b;

	// compute the line parameters of the two closest points
	return D < epsilon	// the lines are almost parallel
		? float2(zero, b > c ? d / b : e / c)	// use the largest denominator
		: float2(b * e - c * d, a * e - b * d) / D;
}

//-----------------------------------------------------------------------------
// ray3
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// plane
//-----------------------------------------------------------------------------

plane::plane(param(position3) p0, param(position3) p1, param(position3) p2, bool norm) {
	vector3	normal	= cross(p1 - p0, p2 - p0);
	if (norm)
		normal = normalise(normal);
	xyzw = (normal, -dot(p0, normal));
}

bool plane::clip(position3 &p0, position3 &p1) const {
	float1	d0 = dist(p0);
	float1	d1 = dist(p0);
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		p0 -= (p1 - p0) * d0 / (d1 - d0);
	} else if (d1 < zero) {
		p1 -= (p1 - p0) * d1 / (d1 - d0);
	}
	return true;
}
bool plane::clip(vector4 &p0, vector4 &p1) const {
	float1	d0 = dot((p0.w < 0).select(-p0, p0), as_float4());
	float1	d1 = dot((p1.w < 0).select(-p1, p1), as_float4());
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		p0 -= (p1 - p0) * d0 / (d1 - d0);
	} else if (d1 < zero) {
		p1 -= (p1 - p0) * d1 / (d1 - d0);
	}
	return true;
}

bool plane::check_ray(param(float4) p0, param(float4) p1, float &t, vector3 *normal) const {
	float1	d0 = dist(p0);
	float1	d1 = dist(p1);
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		t = -d0 / (d1 - d0);
	} else {
		if (d1 >= zero)
			return false;
		t = -d1 / (d1 - d0);
	}
	if (normal)
		*normal = this->normal();
	return true;
}

bool plane::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float1	d0 = dist(p);
	float1	d1 = dist(p + d);
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		t = -d0 / (d1 - d0);
	} else {
		if (d1 >= zero)
			return false;
		t = -d1 / (d1 - d0);
	}
	if (normal)
		*normal = this->normal();
	return true;
}

const plane& plane::unit_cube(CUBE_PLANE i) {
	static const float4 planes[] = {
		float4(-1,0,0,1),
		float4(+1,0,0,1),
		float4(0,-1,0,1),
		float4(0,+1,0,1),
		float4(0,0,-1,1),
		float4(0,0,+1,1),
	};
	return reinterpret_cast<const plane&>(planes[i]);
}

float3x4 reflect(param(plane) p) {
	float4		v = normalise(p).as_float4();
	float3		t = v.xyz * 2;
	float3x4	m = identity;
	m.x -= t * v.x;
	m.y -= t * v.y;
	m.z -= t * v.z;
	m.w -= t * v.w;
	return m;
}

//http://geomalgorithms.com/a05-_intersect-1.html

line3 operator&(param(plane) a, param(plane) b) {
	float3	d	= cross(a.normal(), b.normal());
#if 0
	switch (max_component_index(abs(d))) {
		default:
		case 0:	return line3(position3(zero, project(cross(a.yzw, b.yzw))), d);
		case 1:	return line3(position3(perm<0,2,1>(project(cross(a.xzw, b.xzw)), zero)), d);
		case 2:	return line3(position3(project(cross(a.xyw, b.xyw)), zero), d);
	}
#else
	return line3(cross(a.normal() * b.dist() - b.normal() * a.dist(), d) / dot(d, d), d);
#endif
}

position3 intersect(param(plane) a, param(plane) b, param(plane) c) {
	float3	ab	= cross(a.normal(), b.normal());
	float3	bc	= cross(b.normal(), c.normal());
	float3	ca	= cross(c.normal(), a.normal());
	return position3(-(bc * a.dist() + ca * b.dist() + ab * c.dist()) / dot(a.normal(), bc));
}

//-----------------------------------------------------------------------------
// quadric
//-----------------------------------------------------------------------------

bool quadric::check_ray(param(ray3) r, float &t, vector3 *normal) {
	float4x4	Q		= *this;
	float4		p0		= float4(r.p, one);
	float4		d0		= float4(r.d, zero);
	float4		p1		= Q * p0;
	float4		d1		= Q * d0;

	float1		a		= dot(d0, d1);
	float1		b		= dot(p0, d1) + dot(d0, p1);
	float1		c		= dot(p0, p1);

	float1		discr	= square(b) - a * c * four;
	if (discr < zero)
		return false;

	t = (-b - sqrt(discr)) / (a * two);

	if (normal) {
		float4	p = float4(r.from_parametric(t), one);
		*normal = float3(dot(p, column<0>()), dot(p, column<1>()), dot(p, column<2>()));
	}

	return true;
}

conic quadric::project_z() const {
#if 1
	float4x4		m	= float4x4(x_axis, y_axis, -float4(o.x, d3.y, -one, d3.z) / d4.z, w_axis);
	symmetrical4	s2	= mul_mulT(m, *this);
	return conic(strip_z(s2));
#else
	float3	z	= float3(o.x, d3.yz);
	return conic(
		d4.xyw * d4.z - square(z),
		float3(d3.x, o.yz) * d4.z - float3(d3.yz, o.x) * z
	);
#endif
}

cuboid quadric::get_box() const {
	float3	d = square(float3(d3.y, o.x, d3.x)) - d4.yzx * d4.zxy;
	return cuboid::centre_half(centre(), sqrt(d * det()) / strip_w(*this).det());
}

// tangent points to x, y, z axes given by M * [+-1,0,0], M * [0,+-1,0], M * [0,0,+-1]
float3x4 quadric::get_tangents() const {
	symmetrical3	s3	= strip_w(*this);
	symmetrical3	co	= cofactors(s3);
	float3			v	= sqrt(det() / -co.d3) / s3.det();
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
	return m * (t.translation() + t.z);
}

quadric quadric::plane(const iso::plane &p1) {
	float4	p = p1.as_float4();
	return quadric(float4(float3(zero), p.w), float3(float2(zero), p.z * half), float3(zero, p.yx * half));
}

quadric quadric::plane_pair(const iso::plane &p1, const iso::plane &p2) {
	float4	p = p1.as_float4();
	float4	q = p2.as_float4();
	return quadric(p * q,
		(p.xyz * q.yzw + q.xyz * p.yzw) * half,
		(p.xyw * q.zwx + q.xyw * p.zwx) * half
	);
}

quadric::info quadric::analyse() const {
	float	det			= det3x3();
	bool	empty		= false;
	SIGN	orientation	= POS;
	bool	degenerate	= false;
	TYPE	type;

	auto	d = diagonalise(*this);


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
	return info(type, orientation, empty, degenerate);
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
			v0.norm.store(vals + 0);
			v0.col.store(vals + 3);
			error = q.evaluate(v0.pos, vals);
		}

		position3	minv	= q.centre();
		float		params[6];
		for (int i = 0; i < 6; i++)
			params[i] = q.get_param(i, minv);

		float	errors[6];
		for (auto &i : v) {
			float	vals[6];
			i.norm.store(vals + 0);
			i.col.store(vals + 3);
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
	float1	d	= b.dist(a.centre());
	float1	r2	= max(a.radius2() - d * d, zero);
	return circle3(b, circle::with_r2((b.to_xy_plane() * a.centre()).xy, r2));
}

template<> bool circle3::is_visible(param(float4x4) m) const {
	float4x4	m2	= m * pl.to_xy_plane();
	float3		t	= check_clip(m2 * position3(-circle::centre(), zero));
	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);
	return all(t <= s * radius());
}

template<> bool circle3::is_within(param(float4x4) m) const {
	float4x4	m2	= m * pl.to_xy_plane();
	float3		t	= check_clip(m2 * position3(-circle::centre(), zero));
	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);
	return all(t <= s * -radius());
}

bool circle_through_contains(param(position3) p, param(position3) q, param(position3) r, param(position3) t) {
// compute side_of_bounded_sphere(p,q,r,t+v,t), with v = pq ^ pr.
	float3	pt	= p - t;
	float3	qt	= q - t;
	float3	rt	= r - t;
	float3	v	= cross(q - p, r - p);

	return float4x4(
		float4(pt, len2(pt)),
		float4(rt, len2(rt)),
		float4(qt, len2(qt)),
		float4(v, len2(v))
	).det() < 0;
}

//-----------------------------------------------------------------------------
// sphere
//-----------------------------------------------------------------------------

const _unit_sphere unit_sphere;

position3 _unit_sphere::uniform_surface(param(float2) t) {
	float1	cos_theta	= one - t.x * two;
	float1	sin_theta	= sin_cos(cos_theta);
	return position3(sincos(t.y * (pi * two)) * sin_theta, cos_theta);
};

position3 _unit_sphere::uniform_interior(param(float3) t) {
	return uniform_surface(t.xy) * pow(t.z, third);
}

bool _unit_sphere::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) {
	float1		b = dot(d, p);
	if (b > zero)			// closest point is 'before' p
		return false;

	float1		c = dot(p, p) - one;
	if (c < zero)			// p is inside sphere
		return false;

	float1		a = dot(d, d);
	float1		x = b * b - a * c;
	if (x < zero)			// line misses sphere
		return false;

	x = -b - sqrt(x);
	if (x < zero || x > a)
		return false;		// intersection of line is off ends of segment

	t = x / a;
	if (normal)
		*normal = d * t - p;

	return true;
}

bool _unit_sphere::is_visible(const float4x4 &matrix) {
	vector3	s	= sqrt(
		square(matrix.x.xyz + matrix.x.w)
	+	square(matrix.y.xyz + matrix.y.w)
	+	square(matrix.z.xyz + matrix.z.w)
	);
	return all(check_clip(matrix.w) < s);
}

bool _unit_sphere::is_within(const float4x4 &matrix) {
	vector3	s	= sqrt(
		square(matrix.x.xyz + matrix.x.w)
	+	square(matrix.y.xyz + matrix.y.w)
	+	square(matrix.z.xyz + matrix.z.w)
	);
	return all(check_clip(matrix.w) < -s);
}

bool sphere::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float3		v = centre() - p;
	float1		b = dot(d, v);
	if (b < zero)			// closest point is 'before' p
		return false;

	float1		c = dot(v, v) - square(radius());
	if (c < zero)			// p is inside sphere
		return false;

	float1		a = dot(d, d);
	float1		x = b * b - a * c;
	if (x < zero)			// line misses sphere
		return false;

	x = b - sqrt(x);
	if (x < zero || x > a)
		return false;		// intersection of line is off ends of segment

	t = x / a;
	if (normal)
		*normal = (d * t - v) / radius();

	return true;
}

sphere sphere::operator|(param(position3) pos) const {
	if (is_empty())
		return sphere(pos, zero);

	if (contains(pos))
		return *this;

	vector3		v = pos - centre();
	float1		l = len(v);
	return sphere(centre() + v * ((l - radius()) / l * half), (l + radius()) * half);
}

sphere sphere::operator|(param(sphere) b) const {
	vector3 offset	= b.centre() - centre();
	float1	dist	= len(offset);
	float1	r		= (max(radius(), dist + b.radius()) - min(-radius(), dist - b.radius())) * half;
	return sphere(centre() + offset * ((r - radius()) / dist), r);
}

bool sphere::is_visible(const float4x4 &m) const {
	vector3	s	= rsqrt(
		square(m.x.xyz + m.x.w)
	+	square(m.y.xyz + m.y.w)
	+	square(m.z.xyz + m.z.w)
	);
	float3	t	= check_clip(m * centre());
	return all(t * s < radius());
}

bool sphere::is_within(const float4x4 &m) const {
	vector3	s	= rsqrt(
		square(m.x.xyz + m.x.w)
	+	square(m.y.xyz + m.y.w)
	+	square(m.z.xyz + m.z.w)
	);
	float3	t	= check_clip(m * centre());
	return all(t * s < -radius());
}

sphere sphere::through(param(position3) a, param(position3) b) {
	vector3	r	= (b - a) * half;
	return sphere(a + r, len(r));
}

sphere sphere::through(param(position3) a, param(position3) b, param(position3) c) {
	vector3	x	= b - a;
	vector3	y	= c - a;
	vector3	t	= cross(x, y);
	vector3	r	= (cross(t, x) * len2(y) + cross(y, t) * len2(x)) / (len2(t) * 2);
	return sphere(a + r, len(r));
}

sphere sphere::through(param(position3) a, param(position3) b, param(position3) c, param(position3) d) {
	vector3	x	= b - a;
	vector3	y	= c - a;
	vector3	z	= d - a;
#if 0
	vector3	n1	= normalise(cross(x, y));
	vector3	n2	= normalise(z - n1 * dot(z, n1));
	vector3	n3	= cross(n1, n2);

	circle	c1	= circle::through(
		position2(zero),
		position2(dot(x, n2), dot(x, n3)),
		position2(dot(y, n2), dot(y, n3))
	);
	circle	c2	= circle::through(
		position2(c1.centre().x - c1.radius(), zero),
		position2(c1.centre().x + c1.radius(), zero),
		position2(dot(z, n2), dot(z, n1))
	);

	return sphere(a + position3(n2 * c2.centre().x + n1 * c2.centre().y()), c2.radius());
#else
	float1	det	= dot(x, cross(y, z));
	vector3	r	= (cross(y, z) * len2(x) + cross(z, x) * len2(y) + cross(x, y) * len2(z)) / (det * 2);
	return sphere(a + r, len(r));
#endif
}

sphere sphere::small(const position3 *p, uint32 n) {
	position3	centre	= centroid(p, p + n);
	float1		radius2 = 0;
	for (uint32 i = 0; i < n; i++)
		radius2 = max(radius2, len2(centre - p[i]));
	return sphere(centre, sqrt(radius2));
}

position3 sphere::uniform_surface(param(float2) t) const {
	return _unit_sphere::uniform_surface(t) * radius() + centre();
};

position3 sphere::uniform_interior(param(float3) t) const {
	return _unit_sphere::uniform_interior(t) * radius() + centre();
}

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
	return (m * quadric(s)).project_z();
}

float projected_area(const sphere &s, const float4x4 &m) {
	float3	o	= perm<0,1,3>(m * s.centre());
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

//
bool sphere::through_contains(param(position3) p, param(position3) q, param(position3) r, param(position3) s, param(position3) t) {
	float3	pt	= p - t;
	float3	qt	= q - t;
	float3	rt	= r - t;
	float3	st	= s - t;
	return float4x4(
		float4(pt, len2(pt)),
		float4(rt, len2(rt)),
		float4(qt, len2(qt)),
		float4(st, len2(st))
	).det() < 0;
}

//-----------------------------------------------------------------------------
// ellipsoid
//-----------------------------------------------------------------------------

ellipsoid::ellipsoid(const float3x4 &m) : c(m.translation()), a1(m.x), a2r(m.y, len(m.z)) {}

bool ellipsoid::check_ray(param(ray3) r, float &t, vector3 *normal) const {
	float3x4	m = inv_matrix();
	bool ret = _unit_sphere::check_ray(m * r, t, normal);
	if (ret && normal)
		*normal = transpose((float3x3)m) * *normal;
	return ret;
}

position3 ellipsoid::closest(param(position3) p) const {
	float3x4	m		= matrix();
	float		len2maj	= len2(m.x);

	float3		pr		= p - m.w;
	pr	= float3(dot(m.x, pr), dot(m.y, pr), dot(m.z, pr)) / len2maj;

	float2		cy = ellipse_closest(pr.xy, sqrt(len2maj / len2(m.y)));
	float2		cz = ellipse_closest(pr.xz, sqrt(len2maj / len2(m.z)));

	return m * position3(cy, cz.y);
}

position3 ellipsoid::support(param(float3) v) const {
	float3x4	m		= matrix();
	float		len2maj	= len2(m.x);

	float3		pr	= float3(dot(m.x, v), dot(m.y, v), dot(m.z, v)) / len2maj;

	float2		cy	= ellipse_closest(pr.xy, sqrt(len2maj / len2(m.y)));
	float2		cz	= ellipse_closest(pr.xz, sqrt(len2maj / len2(m.z)));

	return m * position3(cy, cz.y);
}

//-----------------------------------------------------------------------------
// triangle3
//-----------------------------------------------------------------------------

template<> bool triangle3::is_visible(param(float4x4) m) const {
	float4x4	m2	= m * matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;

	return all(min(min(check_clip(p0), check_clip(p1)), check_clip(p2)) < zero);
}

template<> bool triangle3::is_within(param(float4x4) m) const {
	float4x4	m2	= m * matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;

	return all(max(max(check_clip(p0), check_clip(p1)), check_clip(p2)) <= zero);
}

float1 dist(param(triangle3) t, param(iso::plane) p) {
	float3x4	m	= t.matrix();
	float1	dx	= dot(p.normal(), m.x);
	float1	dy	= dot(p.normal(), m.y);
	return p.dist(position3(m.z)) + min(min(dx, dy), zero);
}

//-----------------------------------------------------------------------------
// triangle3b
//-----------------------------------------------------------------------------

interval<float>	triangle3b::intersection(const line3 &line) const {
	float3x4	mat1	= matrix();
	float3x4	basis(line.dir(), cross(mat1.z, line.dir()), mat1.z, line.p);
	float3x4	mat2	= mat1 / basis;
	position3	da		= mat2 * z;
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

bool intersect(const triangle3b &t0, const triangle3b &t1) {
	float3x3	e0(t0.x, t0.y, t0.x - t0.y);
	float3		n0	= cross(e0.x, e0.y);

	if (!t1.projection(n0).contains(dot(n0, t0.z)))
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
		if (!t0.projection(n1).contains(dot(n1, t1.z)))
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

bool intersect(const triangle3b &t0, const triangle3b &t1, param(float3) velocity, float tmax) {
	interval<float>	time(0, tmax);

	float3x3	e0(t0.x, t0.y, t0.x - t0.y);
	float3		n0	= cross(e0.x, e0.y);

	if (!velocity_overlap(interval<float>(dot(n0, t0.z)), t1.projection(n0), dot(velocity, n0), time))
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
		if (!velocity_overlap(t0.projection(n1), interval<float>(dot(n1, t1.z)), dot(velocity, n1), time))
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
// tetrahedron
//-----------------------------------------------------------------------------

bool tetrahedron::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float3x4	i	= inv_matrix();
	position3	p1	= i * p;
	float3		d1	= i * d;

	float4		p2(p1, one - sum_components(p1));
	float4		d2(d1, -sum_components(d1));

	if (all(p2 > zero)) {
		t = 0;
		return true;
	}

	float4	q	= -p2 / d2;
	float4	q0	= (p2 < zero).select(q, zero);
	float1	t0	= max_component(q0);
	float4	p3	= p2 + d2 * t0;
	p3 = (q0 == t0).select(zero, p3);
	if (any(p3 < zero) || any(p3 > one))
		return false;

	t = t0;
	if (normal) {
		int a = max_component_index(q0);
		*normal = normalise(cross(column(dec_mod(a, 3)), column(inc_mod(a, 3))));
	}
	return true;
}

sphere tetrahedron::inscribe() const {
	float3		n0 = normalise(cross(z - x, y - x));
	float3		n1 = normalise(cross(y, z));
	float3		n2 = normalise(cross(z, x));
	float3		n3 = normalise(cross(x, y));
	float3x3	m(n1 - n0, n2 - n0, n3 - n0);
	float3		sol = inverse(m) * float3(zero, zero, -dot(n3, z));
	return sphere(w + sol, abs(dot(n0, sol)));
}

sphere tetrahedron::circumscribe() const {
	float4	d(
		plane(0).normalised_dist(corner(0)),
		plane(1).normalised_dist(corner(1)),
		plane(2).normalised_dist(corner(2)),
		plane(3).normalised_dist(corner(3))
	);

	int	i = min_component_index(abs(d));
	return face(i).circumscribe();

//	float1	d	= det();
//	vector3	r	= (cross(y, z) * len2(x) + cross(z, x) * len2(y) + cross(x, y) * len2(z)) / (d * two);
//	return sphere(w + r, len(r));
}

bool tetrahedron::is_visible(param(float4x4) m) const {
	float4x4	m2	= m * matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;
	float4		p3	= m2.w + m2.z;

	return all(min(min(min(check_clip(p0), check_clip(p1)), check_clip(p2)), check_clip(p3)) < zero);
}

bool tetrahedron::is_within(param(float4x4) m) const {
	float4x4	m2	= m * matrix();
	float4		p0	= m2.w;
	float4		p1	= m2.w + m2.x;
	float4		p2	= m2.w + m2.y;
	float4		p3	= m2.w + m2.z;

	return all(max(max(max(check_clip(p0), check_clip(p1)), check_clip(p2)), check_clip(p3)) <= zero);
}

position3 tetrahedron::closest(param(position3) p) const {
	float4x4	m = planes_matrix();
	float4		a = transpose(m) * p;
	int			i = (a < zero).mask();

	switch (count_bits4(i)) {
		default: {	//inside
			int	j = min_component_index(a);
			return p - (m[j].xyz * a[j]) / len2(m[j].xyz);
		}
		case 1:	{	// outside one facet
			int	j = lowest_set_index4(i);
			position3	p2	= p - (m[j].xyz * a[j]) / len2(m[j].xyz);
			float4		a2	= transpose(m) * p2;
			int			i2	= (a2 < zero).mask() & ~i;
			if (i2 == 0)
				return p2;

			i |= i2;
			if (!is_pow2(i2))
				return corner(lowest_set_index4(i^15));

		}
		// fall through
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
	float4		a(a3, one - sum_components(a3));
	return corner(max_component_index(abs(a)));
}

//-----------------------------------------------------------------------------
// _unit_cube
//-----------------------------------------------------------------------------

const _unit_cube	unit_cube;

bool _unit_cube::is_visible(param(float4x4) m) {
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

bool _unit_cube::is_visible2(param(float4x4) m) {
	return is_visible(m) && is_visible(inverse(m));
};

int _unit_cube::visibility_flags(param(float4x4) m, uint32 flags) {
	if (flags & (7<<0)) {
		// a is max dot products of positive planes' normals
		float4	a	= abs(m.x - m.x.w) + abs(m.y - m.y.w) + abs(m.z - m.z.w);
		float4	t	= m.w - m.w.w;		// plane distances from origin (anything less is out of frustum)
		if (any(a.xyz < t.xyz))
			return -1;

		flags &= ((-a.xyz <= t.xyz).mask() << 0) | ~(7 << 0);
	}

	if (flags & (7<<3)) {
		// a is max dot products of negative planes' normals
		float4	a	= abs(m.x + m.x.w) + abs(m.y + m.y.w) + abs(m.z + m.z.w);
		float4	t	= m.w + m.w.w;	// plane distances from origin (anything less is out of frustum)
		if (any(a.xyz < -t.xyz))
			return -1;

		flags &= ((a.xyz >= t.xyz).mask() << 3) | ~(7 << 3);
	}

	return flags;
}

int _unit_cube::visibility_flags2(param(float4x4) m, uint32 flags) {
	flags = visibility_flags(m, flags);
	if (flags >= (63<<6)) {
		int	flags2 = visibility_flags(inverse(m), flags>>6);
		if (flags2 < 0)
			return flags2;
		flags = (flags & 63) | (flags2 << 6);
	}
	return flags;
}

bool _unit_cube::is_within(param(float4x4) m) {
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

bool _unit_cube::clip_test(param(position3) a, param(position3) b) {
	position3	m		= a + b;
	vector3		d		= b - a;
	vector3		absd	= abs(d);
	float3		two(2);
	return all(abs(m) <= absd + two)
		&& all(abs(cross(m, d)) <= (absd.zxy + absd.yzx) * two);
}

int _unit_cube::clip_test(param(position3) a, param(position3) b, float2 &t) {
	if (any(min(a, b) > one) || any(max(a, b) < -one))
		return -1;			//entirely outside box

	vector3	d	= b - a;
	vector3	q	= one / d;
	vector3	vt0	= (d == zero).select(zero, -abs(q) - a * q);
	vector3	vt1	= (d == zero).select(one,   abs(q) - a * q);

	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);

	if (t0 > t1)
		return -1;

	t = float2(t0, t1);
	return max_component_index(vt0);
}

int _unit_cube::clip_test(param(vector4) a, param(vector4) b, float2 &t) {
	if (any(a.xyz > a.w & b.xyz > b.w) || any(a.xyz < -a.w & b.xyz < -b.w))
		return -1;			//entirely outside box

	float4	d	= b - a;
	float3	va	= -(a.xyz + a.w) / (d.xyz + d.w);
	float3	vb	= -(a.xyz - a.w) / (d.xyz - d.w);
	vector3	vt0	= (a.xyz < -a.w).select(va, (a.xyz > a.w).select(vb, zero));
	vector3	vt1	= (b.xyz < -b.w).select(va, (b.xyz > b.w).select(vb, one));

	float1	t0	= max_component(vt0);
	float1	t1	= min_component(vt1);

	if (t0 > t1)
		return -1;

	t = float2(t0, t1);
	return max_component_index(vt0);
}

bool _unit_cube::clip(position3 &a, position3 &b) {
	if (any(min(a, b) > one) || any(max(a, b) < -one))
		return false;		//entirely outside box

	vector3	d	= b - a;
	vector3	q	= one / d;
	vector3	vt0	= (d == zero).select(zero, -abs(q) - a * q);
	vector3	vt1	= (d == zero).select(one,   abs(q) - a * q);

	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);

	if (t0 > t1)
		return false;

	b	= a + d * t1;
	a	= a + d * t0;
	return true;
}

bool _unit_cube::clip(vector4 &a, vector4 &b) {
	if (a.w < 0)
		a = -a;
	if (b.w < 0)
		b = -b;
	if (any(a.xyz > a.w & b.xyz > b.w) || any(a.xyz < -a.w & b.xyz < -b.w))
		return false;		//entirely outside box

	float4	d	= b - a;
	float3	va	= -(a.xyz + a.w) / (d.xyz + d.w);
	float3	vb	= -(a.xyz - a.w) / (d.xyz - d.w);

	vector3	vt0	= (a.xyz < -a.w).select(va, (a.xyz > a.w).select(vb, zero));
	vector3	vt1	= (b.xyz < -b.w).select(va, (b.xyz > b.w).select(vb, one));

	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);
	if (t0 > t1)
		return false;

	b	= a + d * t1;
	a	= a + d * t0;
	return true;
}

//-----------------------------------------------------------------------------
// cuboid
//-----------------------------------------------------------------------------

bool cuboid::check_ray(param(position3) p, param(vector3) d) const {
	vector3		e		= extent();
	position3	m		= p + p + d - a - b;
	vector3		absd	= abs(d);

	return all(abs(m) <= e + absd)
		&& all(abs(cross(m, d)) <= e.yzx * absd.zxy + e.zxy * absd.yzx);
}

bool cuboid::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	if (any(min(p, p + d) > b) || any(max(p, p + d) < a))
		return false;			//entirely outside box

	vector3	q	= one / d;
	vector3	va	= (a - p) * q;
	vector3	vb	= (b - p) * q;
	vector3	vt0	= (d == zero).select(zero, (d < zero).select(vb, va));
	vector3	vt1	= (d == zero).select(one,  (d < zero).select(va, vb));
	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);
	if (t0 > t1)
		return false;
	t = t0;
	if (normal) switch (max_component_index(vt0)) {
		case 0:	*normal = vector3(select(d.x < 0, one, -one), zero, zero); break;
		case 1:	*normal = vector3(zero, select(d.y < 0, one, -one), zero); break;
		case 2:	*normal = vector3(zero, zero, select(d.z < 0, one, -one)); break;
	}
	return true;
}

bool cuboid::clip(position3 &x, position3 &y) const {
	if (any(min(x, y) > b) || any(max(x, y) < a))
		return false;			//entirely outside box

	vector3	d	= y - x;
	vector3	q	= one / d;
	vector3	va	= (a - x) * q;
	vector3	vb	= (b - x) * q;
	vector3	vt0	= (d == zero).select(zero, (d < zero).select(vb, va));
	vector3	vt1	= (d == zero).select(one,  (d < zero).select(va, vb));
	float1	t0	= max(max_component(vt0), zero);
	float1	t1	= min(min_component(vt1), one);
	if (t0 > t1)
		return false;
	y	= x + d * t1;
	x	= x + d * t0;
	return true;
}

cuboid cuboid::minimum(position3 *p, uint32 n) {
	position3	a = *p, b = a;
	for (unsigned i = 1; i < n; i++) {
		a = min(a, p[i]);
		b = max(b, p[i]);
	}
	return cuboid(a, b);
}
//-----------------------------------------------------------------------------
// parallelepiped
//-----------------------------------------------------------------------------

bool parallelepiped::check_ray(param(position3) p, param(vector3) d) const {
	float3x4	im = inv_matrix();
	position3	p1 = im * p;
	vector3		d1 = im * d;
	return unit_cube.clip_test(p1, p1 + d1);
}

bool parallelepiped::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float3x4	im = inv_matrix();
	position3	p1 = im * p;
	vector3		d1 = im * d;
	float2		t2;
	int ret = unit_cube.clip_test(p1, p1 + d1, t2);
	if (ret < 0)
		return false;
	if (normal) switch (ret) {
		case 0:	*normal = (d1.x < 0).select(x, -x); break;
		case 1:	*normal = (d1.y < 0).select(y, -y); break;
		case 2:	*normal = (d1.z < 0).select(z, -z); break;
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
	position3	a	= inv_matrix() * p;
	float3		as	= sign1(a);
	int			i	= (a > one).mask();

	switch (count_bits4(i)) {
		default: {	//inside
			int	j	= min_component_index(a);
			iso::plane	pl = iso::plane(float3(zero, zero, as[j]) << j, one);
			return pl.project(p);
		}
		case 1:	{	// outside one plane
			int	j	= lowest_set_index4(i);
			iso::plane	pl = iso::plane(float3(zero, zero, as[j]) << j, one);
			return pl.project(p);
		}
		case 2: {	// outside two planes - closest is along common edge
			int	j	= lowest_set_index4(i), k = lowest_set_index4(clear_lowest(i));
			position3	p0	= w + x[j] * as[j], p1 = p0 + x[k] * as[k];
			return ray3(p0, p1).closest(p);
		}
		case 3:	// outside two planes - closest is along common point
			return w + x * as.x + y * as.y + z * as.z;
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

//-----------------------------------------------------------------------------
// obb3
//-----------------------------------------------------------------------------

obb3 obb3::minimum(position3 *p, uint32 n) {
	float3	c = centroid(p, p + n);

	symmetrical3 covariance(zero, zero);
	for (int i = 0; i < n; i++) {
		float3		a	= p[i] - c;
		covariance.d3	+= a * a;
		covariance.o	+= float3(a.xyx) * a.yzz;
	}

	float3		v	= principal_component(covariance);
	quaternion	q	= quaternion::between(float3(1,0,0), v);
//	quaternion	q	= diagonalise(float3x3(covariance));
	float3x4	m	= inverse(q) * translate(-c);

	float3	a = m * p[0], b = a;
	for (unsigned i = 1; i < n; i++) {
		float3	t = m * p[i];
		a = min(a, t);
		b = max(b, t);
	}

	return obb3(translate(c) * q * translate((a + b) * half) * iso::scale((b - a) * half));
}

//-----------------------------------------------------------------------------
// frustum
//-----------------------------------------------------------------------------

cuboid frustum::get_box() const {
	cuboid		c(empty);
	for (int i = 0; i < 8; i++)
		c |= project(corner(CUBE_CORNER(i)));
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

bool frustum::check_ray(param(position3) p, param(vector3) d) const {
	float4x4	im = inv_matrix();
	float2		t;
	return unit_cube.clip_test(im * p, im * position3(p + d), t) >= 0;
}
/*
bool frustum::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float4x4	im = inv_matrix();
	vector4		p1 = im * p;
	vector4		d1 = im * d;
	float2		t2;
	int			ret = unit_cube_clip_test(p1, p1 + d1, t2);
	if (ret < 0)
		return false;
	if (normal) switch (ret) {
		case 0:	*normal = (d1.x < 0).select(x, -x); break;
		case 1:	*normal = (d1.y < 0).select(y, -y); break;
		case 2:	*normal = (d1.z < 0).select(z, -z); break;
	}
	t = t2.x;
	return true;
}
*/
//-----------------------------------------------------------------------------
// cylinder
//-----------------------------------------------------------------------------

bool cylinder::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
//	quadric	q = *this;
//	return q.check_ray(ray3(p, d), t, normal);

	vector3		v	= p - centre();
	float		h2	= len2(dir());
	float		ph	= dot(v, dir());
	float		dh	= dot(d, dir());

	// check caps
	if (dh) {
		float		th	= (-copysign(h2, dh) - ph) / dh;
		if (len2(cross(v + d * th, dir())) < square(radius()) * h2) {
			if (normal)
				*normal = normalise(dir() * sign1(-dh));
			t		= th;
			return true;
		}
	}

	vector3		p2	= v - dir() * (ph / h2);
	vector3		d2	= d - dir() * (dh / h2);

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

bool cylinder::is_visible(param(float4x4) m) const {
	float4x4	m2	= m * matrix();

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	);

	float3		a	= check_clip(m2.z) + s;
	return all(check_clip(m2.w) < a);
}

bool cylinder::is_within(param(float4x4) m) const {
	float4x4	m2	= m * matrix();

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
	return abs(c.z) < one && len2(c.xy) < one;
}

//-----------------------------------------------------------------------------
// cone
//-----------------------------------------------------------------------------

bool cone::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
//	quadric	q = *this;
//	return q.check_ray(ray3(p, d), t, normal);

	vector3	v = p - centre() - dir();
	float	ph = dot(v, dir());
	float	dh = dot(d, dir());
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

	float	a = dh * dh - dot(d, d) * cos2;
	float	b = dh * ph - dot(d, v) * cos2;
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
	    *normal = normalise(normalise(v + d * t) * h - dir());

	t = t0 < 0 ? t1 : min(t0, t1);
	return true;
}

bool cone::is_visible(param(float4x4) m) const {
	float4x4	m2	= m * matrix();
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

bool cone::is_within(param(float4x4) m) const {
	float4x4	m2	= m * matrix();
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
	return abs(c.z) < one && len2(c.xy) < (c.z + one) * half;
}

//-----------------------------------------------------------------------------
// capsule
//-----------------------------------------------------------------------------

bool capsule::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	vector3		v	= p - centre();
	float		h2	= len2(dir());
	float		ph	= dot(v, dir());
	float		dh	= dot(d, dir());

	vector3		p2	= v - dir() * (ph / h2);
	vector3		d2	=  d - dir() * (dh / h2);

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
		return sphere(centre() + dir() * sign1(ph + tc * dh), radius()).check_ray(p, d, t, normal);

	if (normal)
		*normal = normalise(p2 + d2 * tc);
	t = tc;
	return true;
}

bool capsule::is_visible(param(float4x4) m) const {
	float3x4	mat	= translate(centre()) * look_along_z(dir());
	float4x4	m2	= m * mat;

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	+	square(m2.z.xyz + m2.z.w)
	) * radius();

	float3		a	= check_clip(m2.z * len(dir())) + s;
	return all(check_clip(m2.w) < a);
}

bool capsule::is_within(param(float4x4) m) const {
	float3x4	mat	= translate(centre()) * look_along_z(dir());
	float4x4	m2	= m * mat;

	vector3		s	= sqrt(
		square(m2.x.xyz + m2.x.w)
	+	square(m2.y.xyz + m2.y.w)
	+	square(m2.z.xyz + m2.z.w)
	) * radius();

	float3		a	= check_clip(m2.z * len(dir())) + s;
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
	c.z = min(abs(c.z) - one, zero);
	return len2(c.xy) < one;
}

//-----------------------------------------------------------------------------
// rectangles
//-----------------------------------------------------------------------------

int sub_rectangle(param(rectangle) r, rectangle *rects, int num, int max) {
	float4	b		= r.as_float4();
	int		num0	= num;
	for (int i = 0; i < num0; i++) {
		float4		a	= rects[i].as_float4();
		if ((a < b.zwxy).mask() == 3 && all(a != b.zwxy)) {
			int	j		= i;
			if (a.y < b.y) {
				rects[j] = rectangle(a.xyz, b.y);
				a.y = b.y();
				j	= num++;
			}
			if (a.w > b.w) {
				rects[j] = rectangle(perm<0,3,1,2>(a.xzw, b.w));
				a.w = b.w();
				j	= num++;
			}
			if (a.x < b.x) {
				rects[j] = rectangle(perm<0,1,3,2>(a.xyw, b.x));
				j	= num++;
			}
			if (a.z > b.z) {
				rects[j] = rectangle(perm<3,0,1,2>(a.yzw, b.z));
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
	float4	b = r.as_float4();
	for (int i = 0, n = num; i < n;) {
		float4		&ra	= rects[i++].as_float4(), a = ra;
		if ((a < b.zwxy).mask() == 3) {
			switch ((a < b).mask()) {
				case 0:
					add = add_rectangle(rectangle(b.xyz, a.y), rects + i, num - i, max - i, add) + i;
					b = perm<0,2,3,1>(b.xw, a.xy);
					break;
				case 1:	b.w = a.y; break;
				case 2:	b.z = a.x; break;
				case 3:	return add;
				case 4: ra.y = b.w; break;
				case 5:
					add = add_rectangle(rectangle(b.xyz, a.y), rects + i, num - i, max - i, add) + i;
					b	= (a.zy, b.zw);
					break;
				case 6:
					add = add_rectangle(rectangle(perm<0,1,3,2>(b.xyw, a.x)), rects + i, num - i, max - i, add) + i;
					b	= (a.z, b.yzw);
					break;
				case 7:	b.x = a.z; break;
				case 8:	ra.x = b.z; break;
				case 9:
					add = add_rectangle(rectangle(b.xyz, a.y), rects + i, num - i, max - i, add) + i;
					b	= perm<0,3,1,2>(b.xzw, a.w);
					break;
				case 10:
					add = add_rectangle(rectangle(perm<0,1,3,2>(b.xyw, a.x)), rects + i, num - i, max - i, add) + i;
					b	= (a.xy, b.zw);
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
					add = add_rectangle(rectangle(a.z, b.zyw), rects + i, num - i, max - i, add) + i;
					b	= perm<0,3,2,1>(b.xw, a.zw);
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
			float4		fi	= ri.as_float4();

			for (int j = i + 1; j < num; j++) {
				rectangle	rj	= rects[j];

				if (strict_overlap(ri, rj)) {
					rects[i] = ri |= rj;
					rects[j--] = rects[--num];
					changed	= true;

				} else {
					float4	fj		= rj.as_float4();
					int		mask1	= (abs(fi - fj) <= within).mask();
					if ((mask1 & 5) == 5)
						mask1 = 5;
					else if ((mask1 & 10) == 10)
						mask1 = 10;
					else
						continue;

					if ((abs(fi - fj.zwxy) <= within).mask() & ~mask1) {
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
#endif