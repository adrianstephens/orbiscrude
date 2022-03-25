#ifdef NEW_VECTOR
#include "new_geometry.h"
#elif !defined GEOMETRY_H
#define GEOMETRY_H

#include "base/vector.h"
#include "base/array.h"
#include "base/functions.h"
#include "base/bits.h"

#undef small
#undef major
#undef minor

namespace iso {

enum RECT_CORNER {
	CORNER_MM	= 0,
	CORNER_PM	= 1,
	CORNER_MP	= 2,
	CORNER_PP	= 3,
	_CORNER_END2,
};

enum CUBE_PLANE {
	PLANE_MINUS_X	= 0,
	PLANE_PLUS_X	= 1,
	PLANE_MINUS_Y	= 2,
	PLANE_PLUS_Y	= 3,
	PLANE_MINUS_Z	= 4,
	PLANE_PLUS_Z	= 5,
	_PLANE_END,
};

enum CUBE_CORNER {
	CORNER_MMM	= 0,
	CORNER_PMM	= 1,
	CORNER_MPM	= 2,
	CORNER_PPM	= 3,
	CORNER_MMP	= 4,
	CORNER_PMP	= 5,
	CORNER_MPP	= 6,
	CORNER_PPP	= 7,
	_CORNER_END3,
};

enum WINDING_RULE {
	WINDING_NONZERO,
	WINDING_ODD,
	WINDING_POSITIVE,
	WINDING_NEGATIVE,
	WINDING_ABS_GEQ_TWO,
};

inline bool test_winding(WINDING_RULE r, int n) {
	switch (r) {
		case WINDING_NONZERO:		return n != 0;
		case WINDING_ODD:			return !!(n & 1);
		case WINDING_POSITIVE:		return n > 0;
		case WINDING_NEGATIVE:		return n < 0;
		case WINDING_ABS_GEQ_TWO:	return n >= 2 || n <= -2;
		default:					return false;
	}
};

enum SIGN {NEG = 3, ZERO = 0, POS = 1};

inline SIGN get_sign1(float x)			{ return x < 0 ? NEG : SIGN(x > 0); }
inline SIGN operator-(SIGN s)			{ return SIGN(-(int)s & 3); }
inline SIGN operator*(SIGN a, SIGN b)	{ return SIGN(((int)a * (int)b) & 3); }

inline RECT_CORNER clockwise(int i)	{
	return RECT_CORNER(i ^ (i >> 1));
}
inline RECT_CORNER counter_clockwise(int i)	{
	return RECT_CORNER(i ^ (i >> 1) ^ 1);
}

inline range<int_iterator<RECT_CORNER> > corners2() {
	return range<int_iterator<RECT_CORNER> >(CORNER_MM, _CORNER_END2);
}

inline range<const RECT_CORNER*> corners2_cw() {
	static const RECT_CORNER c[] = {CORNER_MM, CORNER_PM, CORNER_PP, CORNER_MP };
	return make_range(c);
}

inline range<const RECT_CORNER*> corners2_ccw() {
	static const RECT_CORNER c[] = {CORNER_PM, CORNER_MM, CORNER_MP, CORNER_PP };
	return make_range(c);
}

inline range<int_iterator<CUBE_CORNER> > corners3() {
	return range<int_iterator<CUBE_CORNER> >(CORNER_MMM, _CORNER_END3);
}
inline range<int_iterator<CUBE_PLANE> > planes() {
	return range<int_iterator<CUBE_PLANE> >(PLANE_MINUS_X, _PLANE_END);
}

template<typename T> vec<T,2> corner(const interval<vec<T,2> > &ext, RECT_CORNER i) {
	return select(uint8(i), ext.a, ext.b);
}
template<typename T> vec<T,3> corner(const interval<vec<T,3> > &ext, CUBE_CORNER i) {
	return select(uint8(i), ext.a, ext.b);
}

template<typename T> inline T inc_mod(T i, T n) { return i + 1 == n ? 0 : i + 1; }
template<typename T> inline T dec_mod(T i, T n) { return i == 0 ? n - 1 : i - 1; }

#if 0
template<typename T> auto	corners(const interval<vec<T,2> > &ext) {
	return transformc(corners2(), [&ext](RECT_CORNER i) { return corner(ext, i); });
}
template<typename T> auto	corners_cw(const interval<vec<T,2> > &ext) {
	return transformc(corners2_cw(), [&ext](RECT_CORNER i) { return corner(ext, i); });
}
template<typename T> auto	corners_ccw(const interval<vec<T,2> > &ext) {
	return transformc(corners2_ccw(), [&ext](RECT_CORNER i) { return corner(ext, i); });
}
template<typename T> auto	corners(const interval<vec<T,3> > &ext) {
	return transformc(corners3(), [&ext](CUBE_CORNER i) { return corner(ext, i); });
}
#endif

template<typename I> typename iterator_traits<I>::element centroid(I i0, I i1) {
	if (i0 == i1)
		return typename iterator_traits<I>::element(zero);
	auto		total	= *i0;
	size_t		n		= distance(i0, i1);
	for (++i0; i0 != i1; ++i0)
		total += *i0;
	return total / int(n);
}

template<typename C> typename container_traits<C>::element centroid(const C &c) {
	return centroid(begin(c), end(c));
}

template<typename I, typename V> I support(I i0, I i1, V v) {
	auto		imax	= i0;

	if (i0 != i1) {
		auto		dmax	= dot(v, *i0);
		for (++i0; i0 != i1; ++i0) {
			auto	d	= dot(v, *i0);
			if (d > dmax) {
				dmax = d;
				imax = i0;
			}
		}
	}
	return imax;
}

template<typename C, typename V> auto support(const C &c, V v) {
	return support(begin(c), end(c), v);
}

template<typename I, typename V> I convex_support(I i0, I i1, V v) {
	I	i = i0;
	for (auto n = i1 - i0; n; n >>= 1) {
		I	m0	= i + (n >> 1);
		I	m1	= m0;
		if (++m1 == i1)
			m1 = i0;
		if (dot(v, *m0) < dot(v, *m1)) {
			i = m1;
			--n;
		}
	}
	return i;
}

template<typename C, typename V> auto convex_support(const C &c, V v) {
	return convex_support(begin(c), end(c), v);
}

//-----------------------------------------------------------------------------
// generators
//-----------------------------------------------------------------------------

template<typename X, typename Y> auto generate2(X x, Y y) {
	return [x, y](int k, int n) { return float2(x(k, n), y(k, n)); };
}
template<typename X, typename Y, typename Z> auto generate3(X x, Y y, Z z) {
	return [x, y, z](int k, int n) { return float3(x(k, n), y(k, n), z(k, n)); };
}

inline float			linear(int k, int n)	{ return (k + half) / n; }
inline auto				halton(int base)		{ return [base](int k, int n) { return corput(base, k); }; }
template<int B1> float	halton(int k, int n)	{ return corput<B1>(k); }

inline auto				hammersley2(int b1)					{ return generate2(linear, halton(b1)); }
inline auto				halton2(int b1, int b2)				{ return generate2(halton(b1), halton(b2)); }
template<int B1> inline auto hammersley2(int k, int n)		{ return float2(linear(k, n), corput<B1>(k)); }
template<int B1, int B2> inline auto halton2(int k, int n)	{ return float2(corput<B1>(k), corput<B2>(k)); }


inline auto				hammersley3(int b1, int b2)					{ return generate3(linear, halton(b1), halton(b2)); }
inline auto				halton3(int b1, int b2, int b3)				{ return generate3(halton(b1), halton(b2), halton(b3)); }
template<int B1, int B2> inline auto hammersley3(int k, int n)		{ return float3(linear(k, n), corput<B1>(k), corput<B2>(k)); }
template<int B1, int B2, int B3> inline auto halton3(int k, int n)	{ return float3(corput<B1>(k), corput<B2>(k), corput<B3>(k)); }

// 2d perimeter
template<typename T, typename G, typename R> void generate_perimeter(const T &shape, G generator, R result, uint32 n) {
	for (uint32 k = 0; k < n; k++)
		*result++ = shape.uniform_perimeter(generator(k, n));
}
// 2d or 3d interior
template<typename T, typename G, typename R> void generate_interior(const T &shape, G generator, R result, uint32 n) {
	for (uint32 k = 0; k < n; k++)
		*result++ = shape.uniform_interior(generator(k, n));
}
// 3d surface
template<typename T, typename G, typename R> void generate_surface(const T &shape, G generator, R result, uint32 n) {
	for (uint32 k = 0; k < n; k++)
		*result++ = shape.uniform_surface(generator(k, n));
}

template<typename T, typename G> auto generate_interior(const T &shape, G generator, uint32 n) {
	return transformc(int_range(n), [shape, generator, n](uint32 k) { return shape.uniform_interior(generator(k, n)); });
}


//-----------------------------------------------------------------------------
// coordinate systems
//-----------------------------------------------------------------------------

struct polar_coord {
	union {
		_v1<float,3,0>		theta;
		_v1<float,3,1>		r;
		_v2<float,3,0,1>	v2;
	};
	polar_coord() {}
	explicit							polar_coord(param(position2) d)			{ r = len(d); theta = iso::atan2(d.y, d.x); }
	template<typename A> explicit		polar_coord(const A &a)					{ v2 = a; }
	template<typename A, typename B>	polar_coord(const A &a, const B &b)		{ v2 = pair<A,B>(a, b); }

	operator position2()	const {
		return position2(sincos(theta) * r);
	}
};

struct spherical_dir {
	union {
		_v1<float,3,0>		theta;
		_v1<float,3,1>		phi;
		_v2<float,3,0,1>	v2;
	};
	spherical_dir() {}
	explicit							spherical_dir(param(vector3) d)			{ v2 = iso::atan2(float2(len(d.xy), d.y), float2(d.zx)); }
	template<typename A> explicit		spherical_dir(const A &a)				{ v2 = a; }
	template<typename A, typename B>	spherical_dir(const A &a, const B &b)	{ v2 = pair<A,B>(a, b); }

	static spherical_dir uniform(param(float2) d) {
		return spherical_dir(two * acos(sqrt(one - d.x)), two * pi * d.y);
	}
	operator vector3()	const {
		float4 t = sin((v2, v2 + pi * half));
		return vector3(t.wy * t.x, t.z);
	}
};

struct spherical_coord {
	union {
		_v1<float,7,0>		theta;
		_v1<float,7,1>		phi;
		_v1<float,7,2>		r;
		_v2<float,7,0,1>	v2;
		_v3<float,7,0,1,2>	v3;
	};
	spherical_coord() {}
	spherical_coord(param(spherical_dir) dir, float _r)		{ v2 = dir.v2; r = _r; }
	explicit		spherical_coord(param(position3) d)		{ r = len(d); theta = acos(d.z / r); phi = atan2(d.y, d.x); }

	spherical_dir	dir()	const { return spherical_dir(v2); }
	operator position3()	const {
		float4 t = sin((v2, v2 + pi * half));
		return position3(vector3(t.wy * t.x, t.z) * r);
	}
};

//-----------------------------------------------------------------------------
//
//						2D
//
//-----------------------------------------------------------------------------

class rectangle;

inline bool colinear(param(float2) p1, param(float2) p2) {
	return cross(p1, p2) == zero;
}
inline bool colinear(param(position2) p1, param(position2) p2, param(position2) p3) {
	return cross(p1 - p3, p2 - p3) == zero;
}
inline bool colinear(param(float2) p1, param(float2) p2, float epsilon) {
	return square(cross(p1, p2)) <= len2(p1) * len2(p2) * square(epsilon);
}
inline bool colinear(param(position2) p1, param(position2) p2, param(position2) p3, float epsilon) {
	return colinear(float2(p1 - p3), float2(p2 - p3), epsilon);
}

float2	solve_line(param(float2) pt0, param(float2) pt1);	// solves y = mx + b for line to intersect two points

template<typename T> float segment_distance2(T a, T b, T p) {
	auto	d	= b - a;
	auto	dl	= len2(d);
	auto	x	= dl == zero ? a : a + d * clamp(dot(p - a, d) / dl, zero, one);
	return len2(x - p);
}

inline float segment_distance(param(position2) a, param(position2) b, param(position2) p) {
	return sqrt(segment_distance2(a, b, p));
}

// compute signed area of triangle (a,b,p)
inline auto signed_area(param(position2) a, param(position2) b, param(position2) p) {
	return cross(a - p, b - p);
}

// compute angle of oriented edge (a,b) relative to point p (for generalised winding number)
inline auto edge_angle(param(position2) a, param(position2) b, param(position2) p) {
	vector2 da = a - p;
	vector2 db = b - p;
	return atan2(cross(da, db), dot(da, db));
}

//-----------------------------------------------------------------------------
// ray2
//-----------------------------------------------------------------------------

class ray2 {
public:
	union {
		_v2<float,15,0,1>	p;
		_v2<float,15,2,3>	d;
		_v4<float,0,1,2,3>	v4;
	};
				ray2()	{}
				ray2(const _zero&)								{ v4 = zero; }
				ray2(param(position2) _p, param(vector2) _d)	{ v4 = make_pair(_p, _d); }
				ray2(param(position2) p0, param(position2) p1)	{ v4 = make_pair(p0, p1 - p0); }
	template<typename A, typename B> static	force_inline ray2	centre_half(const A &a, const B &b)	{ return ray2(position2(a - b), position2(a + b)); }
	position2	pt0()								const		{ return p;		}
	position2	pt1()								const		{ return p + d;	}
	vector2		dir()								const		{ return d;		}
	position2	from_parametric(param(float1) t)	const		{ return p + d * t;		}
	position2	centre()							const		{ return p + d * half;	}
	float1		project_param(param(position2) v)	const		{ return dot(v - p, d) / len2(d);	}
	float1		dist2(param(position2) v)			const		{ return cross(v - p, d) / len2(d);	}
	float1		dist(param(position2) v)			const		{ return sqrt(dist2(v));			}
	position2	closest(param(position2) p)			const		{ return from_parametric(clamp(project_param(p), zero, one)); }
	bool		approx_on(param(position2) v, float tol = ISO_TOLERANCE) const	{ float2 t = v - p; return approx_equal(len(t) + len(t - d), len(d), tol); }

	force_inline float2x3	matrix()				const		{ return float2x3(perp(d), d, p); }
	force_inline float2x3	inv_matrix()			const		{ return inverse(matrix()); }

	force_inline float4&		as_float4()							{ return *(float4*)this;	}
	force_inline float4		as_float4()				const		{ return *(float4*)this;	}
	ray2					operator-()				const		{ return ray2(p, float2(-d)); }

	friend position2	operator&(param(ray2) r1, param(ray2) r2)	{ return (r1.d * cross(r2.p, r2.d) - r2.d * cross(r1.p, r1.d)) / cross(r1.d, r2.d); }
	template<typename M> friend IF_MAT(M, ray2) operator*(const M &m, param(ray2) r) { return ray2(m * position2(r.p), m * float2(r.d)); }
};

//-----------------------------------------------------------------------------
// line2	- 2d analog of plane
//-----------------------------------------------------------------------------

struct line2 : float3 {
	line2()		{}
	template<typename A> explicit line2(const A &a) : float3(a)	{}
	line2(param(vector2) normal, const float dist)		: float3(normal, -dist)				{}
	line2(param(vector2) normal, param(position2) pos)	: float3(normal, -dot(pos, normal))	{}
	line2(param(position2) p1, param(position2) p2, bool norm = true)	{ xy = perp(p2 - p1); if (norm) xy = normalise(xy); z = -dot(p1, xy); }
	line2(param(ray2) r, bool norm = true)								{ xy = perp(r.dir()); if (norm) xy = normalise(xy); z = -dot(r.pt0(), xy); }
	template<int A> line2(const axis_s<A>&)	: float3(axis_s<A>::v)		{}

	force_inline line2		operator-()					const	{ return line2(-xyz); }
	force_inline const float3& as_float3()				const	{ return *this;	}
	force_inline float2		normal()					const	{ return xy;	}
	force_inline float1		dist()						const	{ return z;	}
	force_inline float2		dir()						const	{ return perp(xy); }
	force_inline position2	pt()						const	{ return -xy * z; }

	force_inline float1		dist(param(position2) p)	const	{ return dot(xy, p) + z; }
	force_inline float1		dist(param(float3) p)		const	{ return dot(xyz, p); }
	force_inline bool		test(param(position2) p)	const	{ return dot(xyz, float3(p, one)) >= zero;	}
	force_inline bool		test(param(float3) p)		const	{ return dot(xyz, p) * p.z >= zero;			}
	// only use if not constructed normalised
	force_inline float1		normalised_dist(param(position2) p)	const	{ return dist(p) * rsqrt(len2(xy)); }
	force_inline float1		normalised_dist(param(float3) p)	const	{ return dist(p) * rsqrt(len2(xy)); }

	force_inline position2	project(param(position2) p)	const	{ return p - xy * dist(p);	}
	bool					clip(position2 &p0, position2 &p1)	const;
	bool					check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal)	const;
	force_inline	bool		check_ray(param(ray2) r, float &t, vector2 *normal)	const { return check_ray(r.p, r.d, t, normal);	}

	force_inline float2x3	to_x_axis()					const	{ return float2x3(-yx, float2(x, -y), position2(zero, z)); }
	force_inline float2x3	from_x_axis()				const	{ return float2x3(float2(-y, x), -xy, -xy * z); }

	friend bool			operator==(const line2 &a, const line2 &b)								{ return cross(a.v, b.v) == zero; }
	friend position2	operator&(param(line2) a, param(line2) b)								{ return iso::project(cross(a.v, b.v)); }
	friend bool			approx_equal(const line2 &a, const line2 &b, float tol = ISO_TOLERANCE)	{ return approx_equal(float3(a.xyz * b.yzx), float3(b.xyz * a.yzx), tol); }

	template<typename M> friend IF_MAT(M, line2) operator*(const M &m, param(line2) p)		{ return line2(cofactors(m) * p.as_float3()); }
	template<typename M> friend IF_MAT(M, line2) operator/(param(line2) p, const M &m)		{ return line2(transpose(m) * p.as_float3()); }
	template<typename T> friend line2 operator*(const translate_s<T> &m, param(line2) p)	{ return line2(float3(p.normal(), p.dist(position2(-m.t)))); }
	template<typename T> friend line2 operator/(param(line2) p, const translate_s<T> &m)	{ return line2(float3(p.normal(), p.dist(m.t))); }
	friend line2 normalise(param(line2) p)													{ return line2(p.as_float3() * rlen(p.normal())); }

	friend line2 bisector(param(line2) a, param(line2) b);
	friend float2x3 reflect(param(line2) p);
	friend bool intersection(const line2 &a, const line2 &b, float2 &intersection, float tol = ISO_TOLERANCE) {
		float3	t	= cross(a.v, b.v);
		if (approx_equal(t.z, 0, tol))
			return false;
		intersection = t.xy / t.z;
		return true;
	}

};


//-----------------------------------------------------------------------------
// conic
//-----------------------------------------------------------------------------

// d3x.x.x + d3y.y.y + d3z.w.w + 2 ox.x.y + 2 oy.y.w + 2 oz.x.w = 0

class conic : public symmetrical3 {
public:
	enum TYPE {HYPERBOLA = NEG, PARABOLA = ZERO, ELLIPSE = POS, TRIVIAL = 2};

	struct info {
		TYPE	type;
		SIGN	orientation;
		bool	empty;
		bool	degenerate;
		info(TYPE _type, SIGN _orientation, bool _none, bool _degenerate) : type(_type), orientation(_orientation), empty(_none), degenerate(_degenerate) {}
	};

	static void two_linepairs(conic &c1, conic &c2, const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4);
	static conic through(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4, const position2 &p5);
	static conic circle_through(const position2 &p1, const position2 &p2, const position2 &p3);
	static conic ellipse_through(const position2 &p1, const position2 &p2, const position2 &p3);
	static conic ellipse_through(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4);
	static conic line_pair(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4);
	static conic line_pair(const line2 &p1, const line2 &p2);
	static conic line(const line2 &p1);
	static conic from_matrix(const float3x3 &m);

	static conic unit_circle() { return  conic(float3(one, one, -one), zero); }

	conic() {}
	conic(param(_triangular3) m) : symmetrical3(m)	{}
	template<typename D, typename O> force_inline conic(const D &_d, const O &_o) : symmetrical3(_d, _o)	{}

	SIGN		vol_derivative(const conic &c2)	const;
	float		vol_minimum(const conic &c2)	const;

	info		analyse()						const;
	float		evaluate(param(position2) p)	const	{ return dot(p * p, d3.xy) + two * (dot(p, o.zy) + o.x * p.x * p.y) + d3.z; }
	position2	centre()						const	{ return iso::project(cofactors(*this).column<2>()); }
	position2	centre(conic &x)				const	{ position2 c = centre(); x = conic(float3(d3.xy, evaluate(c)), float3(o.x,zero,zero)); return c; }
	float		area()							const	{ auto t = det2x2(); return pi * det() / t * rsqrt(t); }
	bool		is_circle()						const	{ return d3.x == d3.y && o.x == zero; }
	bool		is_ellipse()					const	{ return det2x2() > zero; }
	void		flip()									{ d3 = -d3; o = -o; }
	conic		yx()							const	{ return yxz(); }

	bool		check_ray(param(ray2) r, float &t, vector2 *normal);
	rectangle	get_rect()						const;
	float2x3	get_tangents()					const;

	position2	support(param(float2) v)		const;

	interval<float>	operator&(const axis_s<0>&)	const	{ float2 t = float2(-o.z, sqrt(square(o.z) - d3.x * d3.z)) / d3.x; return interval<float>(t.x - t.y, t.x + t.y); }
	interval<float>	operator&(const axis_s<1>&)	const	{ float2 t = float2(-o.y, sqrt(square(o.y) - d3.y * d3.z)) / d3.y; return interval<float>(t.x - t.y, t.x + t.y); }
	interval<float>	operator&(param(line2) p)	const	{ return (*this / p.from_x_axis()) & x_axis; }

	friend conic operator-(const conic &a)					{ return a._neg(); }
	friend conic operator+(const conic &a, const conic &b)	{ return a._add(b); }
	friend conic operator-(const conic &a, const conic &b)	{ return a._sub(b); }
	template<typename T> friend enable_if_t<vget_hasr<T>::value,conic> operator*(param(conic) a, const T &b)	{ return a._mul(b); }

//	template<typename M> conic operator/(const M &m) const	{ return mul_mulT(transpose(m), *this); }
	template<typename M> friend conic operator*(const inverse_s<M> &m, const conic &c)	{ return mul_mulT(transpose(m), c); }
	template<typename M> friend IF_MAT(M, conic) operator*(const M &m, const conic &c)	{ return mul_mulT(cofactors(m), c); }
};

//-----------------------------------------------------------------------------
// circle
//-----------------------------------------------------------------------------

class circle : float3 {
public:
	circle() {}
	circle(const _none&)	: float3(float2(zero), -one)			{}
	circle(const _one&)		: float3(float2(zero), one)				{}
	template<typename A> explicit circle(const A &a) : float3(a)	{}
	template<typename C, typename R> circle(const C &c, const R &r) : float3(c, r * r) {}

	template<typename C, typename R> static circle	with_r2(const C &c, const R &r2) { return circle(float3(c, r2)); }
	static circle			through(param(position2) a, param(position2) b);
	static circle			through(param(position2) a, param(position2) b, param(position2) c);
	static circle			small(const position2 *p, uint32 n);
	static bool				through_contains(param(position2) p, param(position2) q, param(position2) r, param(position2) t);

	force_inline position2	centre()				const	{ return xy; }
	force_inline float3::z_t	radius2()				const	{ return z; }
	force_inline float3::z_t	radius()				const	{ return sqrt(z); }
	force_inline float1		area()					const	{ return z * pi; }
	force_inline float1		perimeter()				const	{ return radius() * (two * pi); }
	force_inline bool		is_empty()				const	{ return z < zero; }
	force_inline const float3& as_float3()			const	{ return *this; }
	force_inline const circle& circumscribe()		const	{ return *this; }
	force_inline const circle& inscribe()			const	{ return *this; }

	circle					operator| (param(position2) p)	const;
	circle					operator| (param(circle) b)		const;
	circle&					operator|=(param(position2) p)	{ return *this = *this | p; }
	circle&					operator|=(param(circle) b)		{ return *this = *this | b; }
	circle&					operator*=(param(float2x3) m)	{ xy = m * centre(); z *= len2(m.x); return *this; }

	bool					contains(param(position2) pos)	const	{ return len2(pos - centre()) < radius2(); }
	bool					check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) const;
	bool					check_ray(param(ray2) r, float &t, vector2 *normal)	const { return check_ray(r.p(), r.d(), t, normal);	}
	position2				uniform_perimeter(float x)			const;
	position2				uniform_interior(param(float2) t)	const;

	float2x3				matrix()				const	{ return translate(centre()) * scale(radius()); }
	float2x3				inv_matrix()			const	{ return scale(reciprocal(radius())) * translate(position2(-centre())); }
	rectangle				get_rect()				const;

	operator conic()	const { return matrix() * conic::unit_circle(); }

	position2		closest(param(position2) p)		const	{ return centre() + normalise(p - centre()) * radius(); }
	position2		support(param(float2) v)		const	{ return centre() + normalise(v) * radius(); }
};

class _unit_circle : public circle {
public:
	_unit_circle() : circle(one)	{}
	static bool				contains(param(position2) pos)		{ return len2(pos) < one; }
	static bool				check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal);
	static bool				check_ray(param(ray2) r, float &t, vector2 *normal)	{ return check_ray(r.p(), r.d(), t, normal);	}
	static position2		uniform_perimeter(float x)			{ return sincos(x * (pi * two)); }
	static position2		uniform_interior(param(float2) t)	{ return uniform_perimeter(t.x) * sqrt(t.y); }
	static position2		closest(param(position2) p)			{ return normalise(p); }
	static position2		support(param(float2) v)			{ return normalise(v); }
};
extern const _unit_circle unit_circle;

force_inline position2 circle::uniform_perimeter(float x) const {
	return _unit_circle::uniform_perimeter(x) * radius() + centre();
};

force_inline position2 circle::uniform_interior(param(float2) t) const {
	return _unit_circle::uniform_interior(t) * radius() + centre();
}

//-----------------------------------------------------------------------------
// ellipse
//-----------------------------------------------------------------------------

class ellipse {
public:
	float4	v;// centre, axis
	float	ratio;

	ellipse() {}
	template<typename C, typename A, typename R> ellipse(const C &c, const A &a, const R &r) : v(c, a), ratio(r) {}
	ellipse(const circle &c) : v(c.centre(), c.radius(), zero), ratio(one)		{}
	ellipse(const conic &c);
	explicit ellipse(const float2x3 &m);

	static ellipse			through(param(position2) p1, param(position2) p2);
	static ellipse			through(param(position2) p1, param(position2) p2, param(position2) p3);
	static ellipse			through(param(position2) p1, param(position2) p2, param(position2) p3, param(position2) p4);
	static ellipse			through(param(position2) p1, param(position2) p2, param(position2) p3, param(position2) p4, param(position2) p5);

	force_inline position2	centre()				const	{ return v.xy; }
	force_inline float2		major()					const	{ return v.zw; }
	force_inline float2		minor()					const	{ return perp(major()) * ratio; }
	force_inline float1		area()					const	{ return len2(major()) * ratio * pi;	}
	force_inline circle		circumscribe()			const	{ return circle::with_r2(centre(), len2(major()) * max(ratio, one)); }
	force_inline circle		inscribe()				const	{ return circle::with_r2(centre(), len2(major()) * min(ratio, one)); }
	float1					perimeter()				const;

	force_inline bool		contains(param(position2) pos)	const	{ return len2(pos / matrix()) < one; }
	force_inline	bool		check_ray(param(ray2) r, float &t, vector2 *normal)	const;

	force_inline float2x3	matrix()				const	{ return float2x3(major(), minor(), centre()); }
	force_inline inverse_s<float2x3>	inv_matrix()	const	{ return inverse(matrix()); }
	force_inline rectangle	get_rect()				const;

	ellipse&				operator*=(param(float2x3) m)	{ return *this = ellipse(m * matrix()); }
	operator conic()	const { return matrix() * conic::unit_circle(); }

	friend ellipse operator+(param(ellipse) a, param(ellipse) b);	//minkowski sum
	friend ellipse operator-(param(ellipse) a, param(ellipse) b);	//minkowski difference
	friend ellipse operator*(param(float2x3) m, param(ellipse) b)	{ return ellipse(m * b.matrix()); }
	friend ellipse operator*(param(float2x3) m, param(circle) b)	{ return ellipse(m * b.matrix()); }

	position2		closest(param(position2) p)			const;
	position2		support(param(float2) v)			const;
	position2		uniform_interior(param(float2) t)	const	{ return matrix() * unit_circle.uniform_interior(t); }
	position2		uniform_perimeter(float x)			const;
};

class shear_ellipse : public float2x3 {
public:
	explicit shear_ellipse(const float2x3 &m) : float2x3(m) {}
	shear_ellipse(const conic &c) : float2x3(
		float2(sqrt(-c.det() / (c.d3.x * c.det2x2())), zero),
		float2(-c.o.x, c.d3.x) * sqrt(-c.det() / c.d3.x) / c.det2x2(),
		c.centre()
	) {}

	force_inline position2	centre()				const	{ return translation(); }
	force_inline float1		area()					const	{ return xx * yy * pi;	}
	bool					contains(param(position2) pos)	const	{ return len2(pos / matrix()) < one; }
	force_inline float2x3	matrix()				const	{ return *this; }
	force_inline inverse_s<float2x3>	inv_matrix()	const	{ return inverse(matrix()); }
};

//-----------------------------------------------------------------------------
// rectangle
//-----------------------------------------------------------------------------

class rectangle : float4 {
public:
	force_inline	rectangle()																		{}
	force_inline	rectangle(const _none&)	: float4(float2(+FLT_MAX), float2(-FLT_MAX))			{}
	force_inline	rectangle(const _one&)	: float4(float2(-one), float2(+one))					{}
	force_inline	rectangle(const _zero&)	: float4(zero)											{}
	template<typename A> force_inline explicit	rectangle(const A &a)	: float4(a)				{}
	template<typename A, typename B> force_inline							rectangle(const A &a, const B &b) : float4(a, b) {}
	template<typename A, typename B, typename C, typename D> force_inline	rectangle(const A &a, const B &b, const C &c, const D &d) : float4(a, b, c, d) {}
	template<typename A, typename B> static	force_inline rectangle	min_max(const A &a, const B &b)		{ return rectangle(a, b); }
	template<typename A, typename B> static	force_inline rectangle	corner_size(const A &a, const B &b)	{ return rectangle(a, a + b); }
	template<typename A, typename B> static	force_inline rectangle	centre_half(const A &a, const B &b)	{ return rectangle(a - b, a + b); }

	rectangle&				set_pos(param(float2) b)				{ xyzw	= float4(b, b + zw - xy); return *this; }
	rectangle&				set_centre(param(float2) b)				{ return set_pos(b - half_extent()); }
	rectangle&				set_size(param(float2) b)				{ zw	= xy + b; return *this;		}

	force_inline float4&		as_float4()								{ return *(float4*)this;			}
	force_inline x_t&		min_x()									{ return x;							}
	force_inline y_t&		min_y()									{ return y;							}
	force_inline z_t&		max_x()									{ return z;							}
	force_inline w_t&		max_y()									{ return w;							}
	force_inline xy_t&		pt0()									{ return xy;						}
	force_inline zw_t&		pt1()									{ return zw;						}

	force_inline const float4 as_float4()					const	{ return *(float4*)this;			}
	force_inline const x_t	min_x()							const	{ return x;							}
	force_inline const y_t	min_y()							const	{ return y;							}
	force_inline const z_t	max_x()							const	{ return z;							}
	force_inline const w_t	max_y()							const	{ return w;							}
	force_inline const xy_t	pt0()							const	{ return xy;						}
	force_inline const zw_t	pt1()							const	{ return zw;						}

	force_inline position2	centre()						const	{ return (pt1() + pt0()) * half;	}
	force_inline float2		extent()						const	{ return (pt1() - pt0());			}
	force_inline z_t			width()							const	{ return z - x;						}
	force_inline w_t			height()						const	{ return w - y;						}
	force_inline float2		half_extent()					const	{ return extent() * half;			}
	force_inline position2	corner(RECT_CORNER i)			const	{ return i == 0 ? position2(xy) : i == 1 ? position2(zy) : i == 2 ? position2(xw) : position2(zw); }

	force_inline rectangle	operator| (param(rectangle) b)	const	{ return rectangle(min(xy, b.xy), max(zw, b.zw));	}
	force_inline rectangle&	operator|=(param(rectangle) b)			{ *this = *this | b; return *this;					}
	force_inline rectangle	operator| (param(position2) b)	const	{ return rectangle(min(xy, b), max(zw, b));			}
	force_inline rectangle&	operator|=(param(position2) b)			{ *this = *this | b; return *this;					}
	force_inline rectangle	operator& (param(rectangle) b)	const	{ return rectangle(max(xy, b.xy), min(zw, b.zw));	}
	force_inline rectangle&	operator&=(param(rectangle) b)			{ *this = *this & b; return *this;					}

	force_inline rectangle	operator+ (param(float2) b)		const	{ return rectangle(xyzw + b.xyxy);					}
	force_inline rectangle&	operator+=(param(float2) b)				{ return *this = *this + b;							}
	force_inline rectangle	operator- (param(float2) b)		const	{ return rectangle(xyzw - b.xyxy);					}
	force_inline rectangle&	operator-=(param(float2) b)				{ *this = *this - b; return *this;					}

	force_inline rectangle&	operator*=(param(float2x3) m)			{ xy += m.z; z *= len2(m.x); return *this; }

	force_inline rectangle	expand(param(float2) b)			const	{ return rectangle(pt0() - b, pt1() + b);			}
	force_inline bool		contains(param(rectangle) b)	const	{ return all(pt0() <= b.pt0()) && all(pt1() >= b.pt1());	}
	force_inline bool		contains(param(position2) b)	const	{ return all(b <= pt1()) && all(b >= pt0());		}
	force_inline bool		is_empty()						const	{ return any(pt1() < pt0());						}
	force_inline float1		area()							const	{ return prod_components(extent());					}
	force_inline float1		perimeter()						const	{ return sum_components(extent()) * two;			}
	bool					check_ray(param(position2) p, param(vector2) d) const;
	bool					check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) const;
	force_inline	bool		check_ray(param(ray2) r)							const	{ return check_ray(r.p(), r.d());				}
	force_inline	bool		check_ray(param(ray2) r, float &t, vector2 *normal)	const	{ return check_ray(r.p(), r.d(), t, normal);	}
	bool					clip(position2 &p1, position2 &p2)	const;

	force_inline float2x3	matrix()						const	{ return translate(centre()) * scale(half_extent()); }
	force_inline float2x3	inv_matrix()					const	{ return scale(reciprocal(half_extent())) * translate(position2(-centre())); }
	fixed_array<position2, 4>	corners()					const	{ return { xy, zy, xw, zw }; }
	fixed_array<position2, 4>	corners_cw()				const	{ return { xy, zy, zw, xw }; }

	position2				uniform_interior(param(float2) t)	const	{ return pt0() + extent() * t; }
	position2				uniform_perimeter(float x)			const;

	friend int			max_component_index(param(rectangle) r)			{ return max_component_index(r.extent()); }
	friend rectangle	operator*(param(rectangle) r, float f)			{ return rectangle(r.as_float4() * f); }
	friend rectangle	operator/(param(rectangle) r, float f)			{ return rectangle(r.as_float4() / f); }
	friend rectangle	operator*(param(rectangle) r, param(float2) s)	{ return rectangle(r.as_float4() * float4(s, s)); }
	friend rectangle	operator/(param(rectangle) r, param(float2) s)	{ return rectangle(r.as_float4() / float4(s, s)); }
};

force_inline bool overlap(param(rectangle) a, param(rectangle) b)		{ return all(a.pt1() >= b.pt0()) && all(a.pt0() <= b.pt1()); }
force_inline bool strict_overlap(param(rectangle) a, param(rectangle) b)	{ return all(a.pt1() > b.pt0()) && all(a.pt0() < b.pt1()); }

class _unit_rect : public rectangle {
public:
	force_inline static float2		extent()						{ return float2(2);				}
	force_inline static bool			contains(param(position2) b)	{ return all(abs(b) <= one);	}
	force_inline static bool			contains(param(vector3) b)		{ return all(abs(b.xy) <= b.z);	}
	force_inline static bool			is_empty()						{ return false;					}
	force_inline static float1		area()							{ return 4;						}
	force_inline static float1		perimeter()						{ return 8;						}
	force_inline static _identity	matrix()						{ return _identity();			}
	force_inline static _identity	inv_matrix()					{ return _identity();			}

	_unit_rect() : rectangle(one)	{}
	static bool			clip_test(param(position2) a, param(position2) b);
	static int			clip_test(param(position2) a, param(position2) b, float2 &t);
	static int			clip_test(param(vector3) a, param(vector3) b, float2 &t);
	static float4		barycentric(param(float2) p);
	static position2	uniform_interior(param(float2) t)			{ return position2(t - half) * two; }
	static position2	uniform_perimeter(float x);
	static position2	closest(param(position2) p)					{ return position2((abs(p) > one).select(sign1(p), p)); }
	static position2	support(param(float2) v)					{ return sign1(v); }
};
extern const _unit_rect unit_rect;

template<typename I> rectangle get_rect(I i0, I i1) {
	rectangle	r(empty);
	for (; i0 != i1; ++i0)
		r	|= *i0;
	return r;
}
template<typename C> rectangle get_rect(const C &c) {
	return get_rect(c.begin(), c.end());
}

force_inline rectangle	circle::get_rect()	const {
	float1	r = radius();
	return rectangle(centre() - r, centre() + r);
}

force_inline rectangle	ellipse::get_rect()	const {
	float2	r = sqrt(square(major()) + square(minor()));
	return rectangle(centre() - r, centre() + r);
}

//-----------------------------------------------------------------------------
// parallelogram
//-----------------------------------------------------------------------------

class parallelogram : public float2x3 {
public:
	force_inline parallelogram()	{}
	force_inline parallelogram(param(rectangle) r)	: float2x3(r.matrix())	{}
	force_inline parallelogram(param(position2) a, param(vector2) b, param(vector2) c) : float2x3(b * half, c * half, a + (b + c) * half) {}
	force_inline explicit parallelogram(const float2x3 &m) : float2x3(m)	{}

	force_inline const float2x3&	matrix()					const	{ return *this;								}
	force_inline inverse_s<float2x3&>	inv_matrix()		const	{ return inverse(matrix());					}
	force_inline position2		centre()					const	{ return translation();						}
	force_inline vector2			half_extent()				const	{ return float2x2::scale();					}
	force_inline vector2			extent()					const	{ return half_extent() * 2;					}
	force_inline float1			area()						const	{ return det();								}
	force_inline float1			perimeter()					const	{ return sum_components(scale()) * two;		}
	force_inline parallelogram&	right()								{ if (det() < 0) y = -y; return *this;		}

	force_inline float2		parametric(param(position2) p)	const	{ return p / matrix();				}
	force_inline position2	from_parametric(param(float2) p)const	{ return matrix() * position2(p);	}
	force_inline bool		contains(param(parallelogram) b)const	{ float2x3 m = b.matrix() / inv_matrix(); return all(abs(m.x)+abs(m.y)+abs(m.z) <= one); }
	force_inline bool		contains(param(position2) p)	const	{ return all(abs(parametric(p)) <= one);	}
	force_inline position2	corner(RECT_CORNER i)			const	{ return z + select(!(i & 1), -x, x) + select(!(i & 2), -y, y); }
	fixed_array<position2, 4>	corners()					const	{ return { z - x - y, z + x - y, z -x + y, z + x + y }; }
	fixed_array<position2, 4>	corners_cw()				const	{ return { z - x - y, z + x - y, z + x + y, z - x + y }; }

	bool					check_ray(param(position2) p, param(vector2) d)	const;
	bool					check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal)	const;
	force_inline	bool		check_ray(param(ray2) r)	const							{ return check_ray(r.p(), r.d());				}
	force_inline	bool		check_ray(param(ray2) r, float &t, vector2 *normal)	const	{ return check_ray(r.p(), r.d(), t, normal);	}
	bool					clip(position2 &p1, position2 &p2) const;

//	force_inline circle		inscribe()						const	{ return circle(centre(), min_component(half_extent()));	}
	force_inline circle		circumscribe()					const	{ return circle(centre(), len(x + y));						}
	force_inline rectangle	get_rect()						const	{ vector2 h = abs(x) + abs(y); return rectangle(centre() - h, centre() + h); }
	position2				uniform_interior(param(float2) t)const	{ return matrix() * unit_rect.uniform_interior(t); }
	position2				uniform_concentric(param(float2) t) const {
		float2 u = t * two - one;
		return dot(u, u) == zero
			? float2(zero)
			: abs(u.x) > abs(u.y)
			? sincos((pi / 4) * u.y / u.x) * u.x
			: sincos(pi / 2 - (pi / 4) * u.x / u.y) * u.y;
	}
	position2				uniform_perimeter(float x)		const;
	position2				closest(param(position2) p)		const;
	position2				support(param(float2) v)		const	{ return matrix() * _unit_rect::support(inv_matrix() * v); }

	void					operator*=(param(float2x3) m)			{ float2x3::operator=(m * matrix()); }
	friend parallelogram	operator*(param(float2x3) m, param(parallelogram) b)	{ return parallelogram(m * b.matrix()); }
};

bool overlap(param(parallelogram) a, param(parallelogram) b);
force_inline parallelogram fullmul(param(float2x3) m, param(rectangle) b)		{ return parallelogram(m * translate(b.centre()) * scale(b.half_extent())); }

//-----------------------------------------------------------------------------
// obb2
//-----------------------------------------------------------------------------

class obb2 : public parallelogram {
public:
	obb2()	{}
	force_inline explicit obb2(param(rectangle) r)		: parallelogram(r.matrix())	{}
	force_inline explicit obb2(const float2x3 &m)		: parallelogram(m)	{}
	force_inline explicit obb2(const parallelogram &m)	: parallelogram(m)	{}
	position2				closest(param(position2) p)	const	{ return matrix() * _unit_rect::closest(inv_matrix() * p); }
	force_inline circle		inscribe()					const	{ return circle(centre(), min_component(half_extent()));	}
};

force_inline obb2 operator*(param(float2x3) m, param(rectangle) b)	{ return obb2(m * translate(b.centre()) * scale(b.half_extent())); }

//-----------------------------------------------------------------------------
// triangle
//-----------------------------------------------------------------------------

class triangle : float2x3 {
public:
	triangle()	{}
	triangle(param(position2) a, param(position2) b, param(position2) c) : float2x3(b - a, c - a, a)	{}
	explicit triangle(param(float2x3) m) : float2x3(m)	{}

	float2		parametric(param(position2) p)		const	{ return p / *(const float2x3*)this;			}
	float3		barycentric(param(position2) p)		const	{ float2 t = parametric(p); return float3(t, one - sum_components(t)); }
	position2	from_parametric(param(float2) p)	const	{ return position2(x * p.x + y * p.y + z);		}
	position2	corner(int i)						const	{ return select(i < 2, z + (*this)[i], z);		}
	line2		edge(int i)							const	{ return line2(corner(dec_mod(i, 3)), corner(inc_mod(i, 3))); }
	bool		contains(param(position2) p)		const	{ return all(barycentric(p) >= zero);			}
	position2	centre()							const	{ return position2((x + y) / 3 + z);			}
	float1		area()								const	{ return cross(x, y) * half;					}
	float1		perimeter()							const	{ return len(x) + len(y) + len(y - x);			}
	const float2x3&	matrix()						const	{ return *this; }
	inverse_s<float2x3&> inv_matrix()				const	{ return inverse(matrix()); }
	fixed_array<position2, 3>	corners()			const	{ return { z, z + x, z + y }; }

	void		flip()										{ swap(x, y); }

	bool		check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal) const;
	bool		check_ray(param(ray2) r, float &t, vector2 *normal)	const { return check_ray(r.p(), r.d(), t, normal);	}

	circle		inscribe()		const {
		float1	len0	= len(x), len1 = len(y), len2 = len(y - x), perimeter = len0 + len1 + len2;
		return circle(from_parametric(float2(len2, len1) / perimeter), cross(x, y) / perimeter);
	}
	circle		circumscribe()	const {
		vector2	r	= (x * len2(y) - y * len2(x)) / (cross(x, y) * two);
		return circle::with_r2(z + perp(r), len2(r));
	}
	rectangle	get_rect()							const	{ return rectangle(min(x, y, zero) + z, max(x, y, zero) + z); }
//	position2	uniform_interior(param(float2) t)	const	{ return uniform_subdivide(uint32(float(t.x) * (1ull << 22))); }
	position2	uniform_interior(param(float2) t)	const	{ return from_parametric(sqrt(t.x) * float2(one - t.y, t.y)); }
	position2	uniform_subdivide(uint32 u)			const;
	position2	uniform_perimeter(float x)			const;
	position2	closest(param(position2) p)			const;
	position2	support(param(float2) v)			const;

	void		operator*=(param(float2x3) m)			{ float2x3::operator=(m * matrix()); }
	friend triangle		operator*(param(float2x3) m, param(triangle) b)	{ return triangle(m * b.matrix()); }
};
class _unit_tri : public triangle {
	static const fixed_array<position2, 3>	_corners;
public:
	_unit_tri() : triangle(identity) {}
	force_inline static float2		parametric(param(position2) p)		{ return p; }
	force_inline static float3		barycentric(param(position2) p)		{ return float3(p, one - sum_components(p)); }
	force_inline static position2	from_parametric(param(float2) p)	{ return p; }
	force_inline static bool			contains(param(position2) p)		{ return all(barycentric(p) >= zero); }
	force_inline static float1		area()								{ return half; }
	force_inline static float1		perimeter()							{ return two + sqrt2; }
	force_inline static _identity	matrix()							{ return _identity(); }
	force_inline static _identity	inv_matrix()						{ return _identity(); }
	force_inline static const fixed_array<position2, 3>&	corners()		{ return _corners; }
	force_inline static const position2&	corner(uint8 i)					{ return _corners[i]; }

	static bool			check_ray(param(position2) p, param(vector2) d, float &t, vector2 *normal);
	static bool			check_ray(param(ray2) r, float &t, vector2 *normal) { return check_ray(r.p(), r.d(), t, normal); }

	static position2	uniform_interior(param(float2) t)	{ return sqrt(t.x) * float2(one - t.y, t.y); }
	static position2	uniform_perimeter(float x);
	static position2	support(param(float2) v);
};
extern const _unit_tri unit_tri;

//-----------------------------------------------------------------------------
// quadrilateral
//-----------------------------------------------------------------------------

class quadrilateral {
	float4	p01, p23;
public:
	quadrilateral(param(position2) a, param(position2) b, param(position2) c, param(position2) d) : p01(a, b), p23(c, d)	{}
	quadrilateral(param(rectangle) r) : p01(perm<0,1,2,1>(r.as_float4())), p23(perm<0,3,2,3>(r.as_float4()))	{}
	quadrilateral(param(parallelogram) p) {
		float4		x(p.x, -p.x);
		p01 = perm<0,1,0,1>(p.z - p.y) + x;
		p23 = perm<0,1,0,1>(p.z + p.y) + x;
	}
	explicit quadrilateral(param(float3x3) m) {
		p01	= (project(m.z - m.x - m.y), project(m.z + m.x - m.y));
		p23	= (project(m.z - m.x + m.y), project(m.z + m.x + m.y));
	}

	position2	pt0()								const	{ return p01.xy;	}
	position2	pt1()								const	{ return p01.zw;	}
	position2	pt2()								const	{ return p23.xy;	}
	position2	pt3()								const	{ return p23.zw;	}
	float1		area()								const	{ return diff(p01.xy * p01.wz + p01.zw * p23.yx + p23.xy * p23.wz + p23.zw * p01.yx) * half; }
	float1		perimeter()							const	{ return len(p01.xy - p01.zw) + len(p01.zw - p23.xy) + len(p23.xy - p23.zw) + len(p23.zw - p01.xy);	}
	float2		extent()							const	{ float4 a = min(p01, p23), b = max(p01, p23); return max(b.xy, b.zw) - min(a.xy, a.zw); }
	position2	corner(RECT_CORNER i)				const	{ return position2((const float*)this + i * 2);	}
	position2	operator[](int i)					const	{ return position2((const float*)this + i * 2);	}

	bool		contains(param(position2) pos)		const	{ return unit_rect.contains(inv_matrix() * pos); }
	position2	centre()							const;
	float4		barycentric(param(float2) p)		const;
	float3x3	inv_matrix()						const;
	float3x3	matrix()							const;
	rectangle	get_rect()							const	{ float4 a = min(p01, p23), b = max(p01, p23); return rectangle(min(a.xy, a.zw), max(b.xy, b.zw)); }
	fixed_array<position2, 4>	corners()			const	{ return { p01.xy, p01.zw, p23.xy, p23.zw }; }
	fixed_array<position2, 4>	corners_cw()		const	{ return { p01.xy, p01.zw, p23.zw, p23.xy }; }

	position2	uniform_interior(param(float2) t)	const;
	position2	uniform_perimeter(float x)			const;
	position2	closest(param(position2) p)			const;
	position2	support(param(float2) v)			const;

	force_inline void operator*=(param(float2x3) m)	{ p01 = (m * pt0(), m * pt1()); p23 = (m * pt2(), m * pt3()); }
};

force_inline quadrilateral operator*(param(float2x3) m, param(quadrilateral) b)	{ return quadrilateral(m * b.pt0(), m * b.pt1(), m * b.pt2(), m * b.pt3()); }
force_inline quadrilateral operator*(param(float3x3) m, param(obb2) b)			{ return quadrilateral(m * float3x3(b.matrix())); }
force_inline quadrilateral operator*(param(float3x3) m, param(rectangle) b)		{ return quadrilateral(m * float3x3(b.matrix())); }

//-----------------------------------------------------------------------------
// wedge2
//	infinite 2d wedge defined by a point and two normals
//-----------------------------------------------------------------------------

struct wedge2 : float2x3 {
	wedge2()	{}
	wedge2(param(position2) a, param(float2) x, param(float2) y) : float2x3(x, y, a)	{}

	bool	contains(param(position2) b) const {
		return dot(x, b - z) >= zero && dot(y, b - z) >= zero;
	}

	template<typename S> bool contains_shape(const S &s) const {
		float2	v1	= s.support(x) - z;
		if (dot(x, v1) < zero)
			return false;

		float2	v2	= s.support(y) - z;
		if (dot(y, v2) < zero)
			return false;

		return cross(v1, v2 - v1) > zero || s.contains(z);
	}
};

//-----------------------------------------------------------------------------
// quedge2
//	(infinite) double wedge defined by two rays
//-----------------------------------------------------------------------------

struct quedge2 {
	ray2		r0, r1;
	quedge2()	{}
	quedge2(param(ray2) r0) : r0(r0), r1(zero)	{}
	quedge2(param(ray2) r0, param(ray2) r1) : r0(r0), r1(r1)	{}
	quedge2(param(position2) p, param(float2) d0, param(float2) d1) : r0(p, d0), r1(p, d1)	{}

	bool	contains(param(position2) b) const {
		return cross(r0.d, b - r0.p) >= zero && cross(r1.d, b - r1.p) >= zero && cross(r0.p - r1.p, b - r0.p) >= zero;
	}

	template<typename S> bool contains_shape(const S &s) const {
		float2	d0	= perp(r0.d);
		float2	v0	= s.support(d0);
		if (dot(d0, v0 - r0.p) < zero)
			return false;

		// single line
		if (all(r1.d == zero))
			return true;

		float2	d1	= perp(r1.d);
		float2	v1	= s.support(d1);
		if (dot(d1, v1 - r1.p) < zero)
			return false;

		// single wedge
		if (all(r0.p == r1.p))
			return cross(v0 - r0.p, v0 - v1) < zero || s.contains(r0.p);

		// double wedge
		float2	d2	= perp(r0.p - r1.p);
		float2	v2	= s.support(d2);
		if (dot(d2, v2 - r0.p) < zero)
			return false;

		if (cross(r0.d, r1.d) <= zero)
			return (cross(v0 - r0.p, v0 - v2) > zero || s.contains(r0.p))
				&& (cross(v1 - r1.p, v1 - v2) < zero || s.contains(r1.p));

		// triangle
		if (cross(v0 - r0.p, v0 - v2) < zero)
			return s.contains(r0.p);

		if (cross(v1 - r1.p, v1 - v2) > zero)
			return s.contains(r1.p);

		//intersection of rays
		position2	x = r0 & r1;
		return cross(v0 - x, v0 - v1) < zero || s.contains(x);
	}
};

//-----------------------------------------------------------------------------
//
//						3D
//
//-----------------------------------------------------------------------------

struct cuboid;

inline bool colinear(param(float3) p1, param(float3) p2) {
	return all(cross(p1, p2) == zero);
}
inline bool colinear(param(position3) p1, param(position3) p2, param(position3) p3) {
	return colinear(float3(p1 - p3), float3(p2 - p3));
}
inline bool colinear(param(float3) p1, param(float3) p2, float epsilon) {
	return float(dot(p1, p2) * rsqrt(len2(p1) * len2(p2))) > 1 - epsilon;
}
inline bool colinear(param(position3) p1, param(position3) p2, param(position3) p3, float epsilon) {
	return colinear(float3(p1 - p3), float3(p2 - p3), epsilon);
}

// compute signed volume of tetrahedron (a,b,c,origin)
inline auto triple_product(param(position3) a, param(position3) b, param(position3) c) {
	return dot(a, cross(b, c));
}

// compute signed volume of tetrahedron (a,b,c,p)
inline auto signed_volume(param(position3) a, param(position3) b, param(position3) c, param(position3) p) {
	return triple_product(a - p, b - p, c - p);
}

// compute solid angle of oriented triangle (a,b,c) relative to origin (for generalised winding number)
inline auto triangle_solid_angle(param(float3) a, param(float3) b, param(float3) c) {
	float3	len	= sqrt(float3(dot(a, a), dot(b, b), dot(c,c)));
	return atan2(
		dot(a, cross(b, c)),
		len.x * len.y * len.z + dot(a, b) * len.z + dot(b, c) * len.x + dot(c, a) * len.y
	) * two;
}

// compute solid angle of oriented triangle (a,b,c) relative to point p (for generalised winding number)
inline auto triangle_solid_angle(param(position3) a, param(position3) b, param(position3) c, param(position3) p) {
	return triangle_solid_angle(a - p, b - p, c - p);
}

//-----------------------------------------------------------------------------
// line3
//-----------------------------------------------------------------------------

class line3 : public aligner<16> {
public:
	position3	p;
	vector3		d;
				line3() {}
				line3(param(position3) p, param(vector3) d) : p(p), d(d)			{}
				line3(param(position3) p0, param(position3) p1) : p(p0), d(p1 - p0)	{}
				line3(param(line2) l2) : p(l2.pt(), zero), d(l2.dir(), zero)		{}
	position3	from_parametric(param(float1) t)	const	{ return p + d * t; }
	float1		project_param(param(position3) v)	const	{ return dot(v - p, d) / len2(d); }
	position3	closest(param(position3) p)			const	{ return from_parametric(project_param(p)); }
	float1		dist2(param(position3) v)			const	{ return len2(cross(v - p, d)) / len2(d); }
	float1		dist(param(position3) v)			const	{ return sqrt(dist2(v)); }
	vector3		dir()								const	{ return d; }

	friend line3	operator*(param(float3x4) m, param(line3) r)	{ return line3(m * r.p, m * r.d); }
	friend float2	closest_params(param(line3) r1, param(line3) r2);
};

class normalised_line3 : public line3 {
public:
	normalised_line3(param(position3) p, param(vector3) d)		: line3(p, d)					{}
	normalised_line3(param(position3) p0, param(position3) p1)	: line3(p0, (float3)normalise(p1 - p0))	{}
	float1		project_param(param(position3) v)	const	{ return dot(v - p, d); }
	position3	closest(param(position3) p)			const	{ return from_parametric(project_param(p)); }
	float1		dist2(param(position3) v)			const	{ return len2(cross(v - p, d)); }
	float1		dist(param(position3) v)			const	{ return sqrt(dist2(v)); }
};

//-----------------------------------------------------------------------------
// ray3
//-----------------------------------------------------------------------------

class ray3 : public line3 {
public:
				ray3() {}
				ray3(param(position3) _p, param(vector3) _d) : line3(_p, _d)		{}
				ray3(param(position3) p0, param(position3) p1) : line3(p0, p1)	{}
	template<typename A, typename B> static	force_inline ray3	centre_half(const A &a, const B &b)	{ return ray3(position3(a - b), position3(a + b)); }
	position3	centre()					const	{ return p + d * half;	}
	position3	pt0()						const	{ return p;		}
	position3	pt1()						const	{ return p + d; }
	cuboid		get_box()					const;
	position3	closest(param(position3) p)	const	{ return from_parametric(clamp(project_param(p), zero, one)); }

	force_inline float3x4	matrix()		const	{ return translate(p) * transpose(look_along_z(d)); }
	force_inline float3x4	inv_matrix()	const	{ return look_along_z(d) * translate(-p); }

	friend ray3		operator*(param(float3x4) m, param(ray3) r)		{ return ray3(m * r.p, m * r.d); }
	friend float2	closest_params(param(ray3) r1, param(ray3) r2)	{ return clamp(closest_params((const line3&)r1, (const line3&)r2), zero, one); }
};

//-----------------------------------------------------------------------------
// plane
//-----------------------------------------------------------------------------

class plane : float4 {
public:
	static force_inline plane	infinity()			{ return plane(perm<0,0,0,1>(zero, one)); }
	static const plane&			unit_cube(CUBE_PLANE i);

	force_inline plane() {}
	template<typename A> explicit plane(const A &a) : float4(a)	{}
	force_inline plane(param(vector3) normal, const float dist)		: float4(normal, -dist)				{}
	force_inline plane(param(vector3) normal, param(position3) pos)	: float4(normal, -dot(pos, normal))	{}
	plane(param(position3) p0, param(position3) p1, param(position3) p2, bool normalise = true);
	template<int A> plane(const axis_s<A>&)	: float4(axis_s<A>::v)	{}

	force_inline plane		operator-()					const	{ return plane(-xyzw); }
	force_inline float4		as_float4()					const	{ return *this;						}
	force_inline float3		normal()					const	{ return xyz;						}
	force_inline float1		dist()						const	{ return w;							}
	force_inline float1		dist(param(position3) p)	const	{ return dot(xyzw, float4(p, one));	}
	force_inline float1		dist(param(float4) p)		const	{ return dot(xyzw, p);	}
	force_inline bool		test(param(position3) p)	const	{ return dot(xyzw, float4(p, one)) >= zero;	}
	force_inline bool		test(param(float4) p)		const	{ return dot(xyzw, p) * p.w >= zero;		}
	// only use if not constructed normalised
	force_inline float1		normalised_dist(param(position3) p)	const	{ return dist(p) * rsqrt(len2(xyz)); }
	force_inline float1		normalised_dist(param(float4) p)	const	{ return dist(p) * rsqrt(len2(xyz)); }

	force_inline position3	project(param(position3) p)	const	{ return p - normal() * dist(p);	}
	force_inline float4		project(param(float4) p1, param(float4) p2)	const { return p1 * dot(p2, xyzw) - p2 * dot(p1, xyzw);	}

	force_inline float3x4	to_xy_plane()				const	{ float3 t = normalise(perp(xyz)); float3x3 m(t, cross(xyz, t), xyz); return float3x4(transpose(m), position3(zero, zero, w)); }
	force_inline float3x4	from_xy_plane()				const	{ float3 t = normalise(perp(xyz)); return float3x4(t, cross(xyz, t), xyz, -xyz * w); }

	bool					clip(position3 &p0, position3 &p1)	const;
	bool					clip(vector4 &p0, vector4 &p1)		const;
	bool					check_ray(param(float4) p0, param(float4) p1, float &t, vector3 *normal)	const;
	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal)	const;
	force_inline	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const { return check_ray(r.p, r.d, t, normal);	}

	friend position3	intersect(param(plane) a, param(plane) b, param(plane) c);
	friend float3x4		reflect(param(plane) p);
	friend line3		operator&(param(plane) a, param(plane) b);
	friend position3	operator&(param(line3) a, param(plane) b)								{ return a.p - a.d * b.dist(a.p) / dot(a.d, b.normal()); }
	friend line2		operator&(param(plane) a, const axis_s<2>&)								{ return line2(a.xyw); }
	friend plane		operator*(const float3x4 &m, param(plane) p)							{ return translate(m.translation()) * ((const float3x3&)m * p); }
	friend plane		operator/(param(plane) p, const float3x4 &m)							{ return p / translate(m.translation()) / (const float3x3&)m; }
	friend plane		bisector(param(plane) a, param(plane) b)								{ return plane(normalise(a).as_float4() - normalise(b).as_float4()); }
	friend bool			coplanar(param(plane) a, param(plane) b)								{ return colinear(a.normal(), b.normal()); }
	friend bool			coincident(param(plane) a, param(plane) b)								{ return all(a.xyzw * b.yzwx == a.yzwx * b.xyzw) && all(a.xyz * b.zww == a.zww * b.xyz); }
	friend bool			approx_equal(const plane &a, const plane &b, float tol = ISO_TOLERANCE)	{ return approx_equal(float4(a.xyzw * b.yzwx), float4(b.xyzw * a.yzwx), tol); }
	friend plane		normalise(param(plane) p)												{ return plane(p.as_float4() * rlen(p.normal())); }

	template<typename M> friend IF_MAT(M, plane) operator*(const M &m, param(plane) p)			{ return plane(cofactors(m) * p.as_float4()); }
	template<typename M> friend IF_MAT(M, plane) operator/(param(plane) p, const M &m)			{ return plane(transpose(m) * p.as_float4()); }
	template<typename T> friend plane operator*(const translate_s<T> &m, param(plane) p)		{ return plane(float4(p.normal(), p.dist(position3(-m.t)))); }
	template<typename T> friend plane operator/(param(plane) p, const translate_s<T> &m)		{ return plane(float4(p.normal(), p.dist(m.t))); }
};

//-----------------------------------------------------------------------------
// sphere
//-----------------------------------------------------------------------------
class sphere : float4 {
public:
	sphere() {}
	sphere(const _none&)	: float4(float3(zero), -one)			{}
	sphere(const _one&)		: float4(float3(zero), one)				{}
	template<typename A> explicit sphere(const A &a) : float4(a)	{}
	template<typename A, typename B> sphere(const A &a, const B &b) : float4(a, b) {}

	static sphere			through(param(position3) a, param(position3) b);
	static sphere			through(param(position3) a, param(position3) b, param(position3) c);
	static sphere			through(param(position3) a, param(position3) b, param(position3) c, param(position3) d);
	static sphere			small(const position3 *p, uint32 n);
	static bool				through_contains(param(position3) p, param(position3) q, param(position3) r, param(position3) s, param(position3) t);

	force_inline float4		as_float4()				const	{ return *(float4*)this;}
	force_inline position3	centre()				const	{ return xyz;			}
	force_inline w_t			radius()				const	{ return w;				}
	force_inline w_t&		radius()						{ return w;				}
	force_inline w_t			radius2()				const	{ return square(w);		}
	force_inline float1		volume()				const	{ return cube(w) * (pi * (4.0f / 3.0f));	}
	force_inline bool		is_empty()				const	{ return w < zero;		}

	sphere					operator| (param(position3) p)	const;
	force_inline sphere&		operator|=(param(position3) p)	{ return *this = *this | p; }
	sphere					operator| (param(sphere) b)		const;
	force_inline sphere&		operator|=(param(sphere) b)		{ return *this = *this | b; }

	force_inline float3x4	matrix()				const	{ return translate(centre()) * scale(radius()); }
	force_inline float3x4	inv_matrix()			const	{ return scale(reciprocal(radius())) * translate(position3(-centre())); }

	bool					contains(param(position3) pos)	const	{ return len2(pos - centre()) < radius2(); }
	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const;
	force_inline	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const { return check_ray(r.p, r.d, t, normal);	}
	bool					is_visible(param(float4x4) m)				const;
	bool					is_within(param(float4x4) m)				const;
	cuboid					get_box()									const;
	const sphere&			circumscribe()								const	{ return *this;	}
	const sphere&			inscribe()									const	{ return *this;	}

	position3				uniform_surface(param(float2) t)			const;
	position3				uniform_interior(param(float3) t)			const;
	position3				closest(param(position3) p)					const	{ return centre() + normalise(p - centre()) * radius(); }
	position3				support(param(float3) v)					const	{ return centre() + normalise(v) * radius(); }

	void					operator*=(param(float3x4) m)	{ xyz = m * centre(); w *= len(m.x); }

	friend float1			dist(param(sphere) s, param(plane) p)			{ return p.dist(s.centre()) - s.radius(); }
};

float	projected_area(const sphere &s, const float4x4 &m);
conic	projection(param(sphere) s, const float4x4 &m);
int		check_occlusion(const sphere &sa, const sphere &sb, const position3 &camera);

class _unit_sphere : public sphere {
public:
	_unit_sphere() : sphere(one)	{}
	static _identity		matrix()							{ return _identity(); }
	static _identity		inv_matrix()						{ return _identity(); }
	static bool				contains(param(position3) pos)		{ return len2(pos) < one; }
	static bool				check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal);
	static bool				check_ray(param(ray3) r, float &t, vector3 *normal)	{ return check_ray(r.p, r.d, t, normal); }
	static bool				is_visible(param(float4x4) m);
	static bool				is_within(param(float4x4) m);
	static position3		uniform_surface(param(float2) t);
	static position3		uniform_interior(param(float3) t);
	static position3		closest(param(position3) p)			{ return normalise(p); }
	static position3		support(param(float3) v)			{ return normalise(v); }
};
extern const _unit_sphere unit_sphere;

//-----------------------------------------------------------------------------
// ellipsoid
//-----------------------------------------------------------------------------

class ellipsoid {
	position3	c;
	float3		a1;
	float4		a2r;	//axis2 + size of axis3
public:
	ellipsoid() {}
	ellipsoid(param(position3) &_centre, param(float3) _axis1, param(float3) _axis2, float radius3) : c(_centre), a1(_axis1), a2r(_axis2, radius3) {}
	ellipsoid(const sphere &c) : c(c.centre()), a1(c.radius(), zero, zero), a2r(zero, c.radius(), zero, c.radius())		{}
	explicit ellipsoid(const float3x4 &m);

	force_inline position3	centre()				const	{ return c;	}
	force_inline float3		axis1()					const	{ return a1;	}
	force_inline float3		axis2()					const	{ return a2r.xyz;	}
	force_inline float3		axis3()					const	{ return normalise(cross(a1, a2r.xyz)) * a2r.w; }
	force_inline float1		volume()				const	{ return len(cross(a1, a2r.xyz)) * a2r.w * (pi * (4.0f / 3.0f)); }

	force_inline float3x4	matrix()				const	{ return float3x4(axis1(), axis2(), axis3(), centre()); }
	force_inline inverse_s<float3x4>	inv_matrix()	const	{ return inverse(matrix()); }
	force_inline cuboid		get_box()				const;
	force_inline sphere		circumscribe()			const	{ return sphere(centre(), len(a1));	}
	force_inline sphere		inscribe()				const	{ return sphere(centre(), a2r.w);	}

	bool					check_ray(param(ray3) r, float &t, vector3 *normal)	const;
	bool					contains(param(position3) pos)	const { return len2(pos / matrix()) < one; }
	bool					is_visible(param(float4x4) m)	const { return _unit_sphere::is_visible(m * inv_matrix()); }
	bool					is_within(param(float4x4) m)	const { return _unit_sphere::is_within(m * inv_matrix()); }
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;

	void					operator*=(param(float3x4) m)	{ *this = ellipsoid(m * matrix()); }

	friend ellipsoid		operator*(param(float3x4) m, param(ellipsoid) b)	{ return ellipsoid(m * b.matrix()); }
	friend ellipsoid		operator*(param(float3x4) m, param(sphere) b)		{ return ellipsoid(m * b.matrix()); }

	friend float1			dist(param(ellipsoid) e, param(plane) p);
};

//-----------------------------------------------------------------------------
// plane_shape - 2d shapes defined on a plane
//-----------------------------------------------------------------------------

template<typename S> struct plane_shape : S {
	plane	pl;

	template<typename T> static auto plane_shape_param(const float3x4 &m, T &&t) { return forward<T>(t); }
	static position2 plane_shape_param(const float3x4 &m, param(position3) p) { return (m * p).xy; }

	plane_shape() {}
	plane_shape(param(plane) p, paramT(S) s) : S(s), pl(p) {}
	template<typename...PP> plane_shape(param(position3) pos, param(vector3) norm, PP... pp) : pl(norm, pos) { new(this) S((pl.to_xy_plane() * pos).xy, pp...); }
	template<typename...PP> plane_shape(param(position3) p0, param(position3) p1, param(position3) p2, PP... pp) : pl(p0, p1, p2) { float3x4 m = pl.to_xy_plane(); new(this) S((m * p0).xy, (m * p1).xy, (m * p2).xy, plane_shape_param(m, forward<PP>(pp))...); }

	iso::plane				plane()					const	{ return pl; }
	float3					normal()				const	{ return pl.normal(); }
	const S&				shape()					const	{ return *this; }
	force_inline position3	centre()				const	{ return pl.from_xy_plane() * position3(S::centre(), zero);	}
	force_inline float3x4	matrix()				const	{ return pl.from_xy_plane() * float3x4(S::matrix()); }
	force_inline float3x4	inv_matrix()			const	{ return float3x4(S::inv_matrix()) * pl.to_xy_plane(); }

	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal = 0) const;
	bool					check_ray(param(ray3) r, float &t, vector3 *normal = 0)	const { return check_ray(r.p, r.d, t, normal);	}
	bool					is_visible(param(float4x4) matrix)	const;
	bool					is_within(param(float4x4) matrix)	const;
	cuboid					get_box()				const;

	float3		parametric(param(position3) p)		const;
	float3		barycentric(param(position3) p)		const;
	position3	from_parametric(param(float2) p)	const;
	position3	corner(uint8 i)						const;

	bool		contains(param(position3) p)		const;
	position3	uniform_interior(param(float2) t)	const;
	position3	uniform_perimeter(float x)			const;
	sphere		circumscribe()						const;
	sphere		inscribe()							const	{ return sphere(centre(), zero); }

	auto		corners()							const { auto m = pl.from_xy_plane(); return transformc(S::corners(), [this, m](param(position2) p) { return m * position3(p); }); }
	auto		corners_cw()						const { auto m = pl.from_xy_plane(); return transformc(S::corners_cw(), [this, m](param(position2) p) { return m * position3(p); }); }
	position3	closest(param(position3) p)			const { return pl.from_xy_plane() * position3(S::closest((pl.to_xy_plane() * p).xy)); }
	position3	support(param(float3) v)			const { return pl.from_xy_plane() * position3(S::support((pl.to_xy_plane() * v).xy)); }

	void		operator*=(param(float3x4) m)	{
		float3x4	m2 = m * pl.from_xy_plane();
		pl	= iso::plane(m2.z, m2.translation());
		float3x4	m3 = pl.to_xy_plane() * m2;
		S::operator*=(strip_z(m3));
	}
	friend plane_shape	operator*(param(float3x4) m, const plane_shape &s) {
		float3x4	m2 = m * s.pl.from_xy_plane();
		auto		p2 = iso::plane(m2.z, m2.translation());
		float3x4	m3 = p2.to_xy_plane() * m2;
		return plane_shape(p2, strip_z(m3) * s.shape());
	}
};

template<typename S> inline float3		plane_shape<S>::parametric(param(position3) p)		const	{ position3 p2 = pl.to_xy_plane() * p; return float3(S::parametric(p2.xy), p2.z);	}
template<typename S> inline float3		plane_shape<S>::barycentric(param(position3) p)		const	{ float2 t = parametric(p).xy; return float3(t, one - sum_components(t)); }
template<typename S> inline position3	plane_shape<S>::from_parametric(param(float2) p)	const	{ return pl.from_xy_plane() * position3(S::from_parametrix(p)); }
template<typename S> inline position3	plane_shape<S>::corner(uint8 i)						const	{ return pl.from_xy_plane() * position3(S::corner(i)); }
template<typename S> inline bool		plane_shape<S>::contains(param(position3) p)		const	{ return S::contains((pl.to_xy_plane() * p).xy); }
template<typename S> inline position3	plane_shape<S>::uniform_interior(param(float2) t)	const	{ return pl.from_xy_plane() * position3(S::uniform_interior(t)); }
template<typename S> inline position3	plane_shape<S>::uniform_perimeter(float x)			const	{ return pl.from_xy_plane() * position3(S::uniform_perimeter(x)); }
template<typename S> inline sphere		plane_shape<S>::circumscribe()						const	{ circle c = S::circumscribe(); return sphere(pl.from_xy_plane() * position3(c.centre()), c.radius()); }

template<typename S> bool plane_shape<S>::check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float3x4	m	= pl.from_xy_plane();
	m.z = -d;

	float3		r	= p / m;
	if (r.z >= zero && S::contains(r.xy)) {
		t = r.z;
		if (normal)
			*normal = this->normal();
		return true;
	}
	return false;
}

template<typename S> float1 dist(paramT(plane_shape<S>) s, param(plane) p)	{
	return dist(s.shape(), s.pl.to_xy_plane() * (p & s.plane()));
}

//-----------------------------------------------------------------------------
// circle3
//-----------------------------------------------------------------------------

typedef plane_shape<circle> circle3;

inline ellipse project(param(circle3) c) {
	float3x4	m = c.inv_matrix();
	return conic::from_matrix(float3x3(m.x, m.y, m.w));
}

inline circle3	project(param(sphere) s, param(plane) p) {
	return circle3(p, circle((p.to_xy_plane() * s.centre()).xy, s.radius()));
}

circle3 seen_from(param(sphere) s, param(position3) pos);
circle3	operator&(param(sphere) a, param(plane) b);

bool circle_through_contains(param(position3) p, param(position3) q, param(position3) r, param(position3) t);

//-----------------------------------------------------------------------------
// triangle3
//-----------------------------------------------------------------------------

typedef plane_shape<triangle>		triangle3;
typedef plane_shape<parallelogram>	parallelogram3;


//-----------------------------------------------------------------------------
// plane_shape2 - 2d shapes defined on a plane
//-----------------------------------------------------------------------------

struct plane_shape2 {
	float3			x;
	float3			y;
	position3		z;
	plane_shape2(param(float3) x, param(float3) y, param(position3) z) : x(x), y(y), z(z) {}

	float3			normal()				const	{ return cross(x, y); }
	iso::plane		plane()					const	{ return iso::plane(normal(), z); }
	float3x4		matrix()				const	{ return float3x4(x, y, normal(), z); }
	float3x4		inv_matrix()			const	{ return inverse(matrix()); }
};

struct triangle3b : plane_shape2 {
	triangle3b(param(position3) a, param(position3) b, param(position3) c) : plane_shape2(b - a, c - a, a) {}

	position3		corner(int i)			const	{ return select(i < 2, z + (&x)[i], z);		}
	position3		centre()				const	{ return (x + y) / 3 + z; }
	cuboid			get_box()				const;

	bool			contains(param(position3) p) const {
		float3		r	= p / matrix();
		return approx_equal(r.z, zero) && r.x >= zero && r.y >= zero && r.x + r.y <= one;
	}
	bool			check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal = 0) const {
		float3x4	m	= float3x4(x, y, -d, z);
		float3		r	= p / m;
		if (r.z >= zero && r.x >= zero && r.y >= zero && r.x + r.y < one) {
			t = r.z;
			if (normal)
				*normal = this->normal();
			return true;
		}
		return false;
	}
	bool			check_ray(param(ray3) r, float &t, vector3 *normal = 0)	const {
		return check_ray(r.p, r.d, t, normal);
	}
	auto			solid_angle(param(position3) p)		const { float3 t = z - p; return triangle_solid_angle(t, t + x, t + y); }
	auto			signed_volume(param(position3) p)	const { return dot(cross(x, y), z - p);	}
	auto			signed_volume()						const { return dot(cross(x, y), z);	}

	interval<float>	projection(param(float3) n) const {
		auto dz = dot(n, z);
		auto dx = dot(n, x) + dz;
		auto dy = dot(n, y) + dz;
		return {min(dx, dy, dz), max(dx, dy, dz)};
	}
	interval<float>	projection(param(iso::plane) p) const {
		auto dz = p.dist(z);
		auto dx = dz + dot(p.normal(), x);
		auto dy = dz + dot(p.normal(), y);
		return {min(dx, dy, dz), max(dx, dy, dz)};
	}
	interval<float>	intersection(const line3 &line) const;
};

bool	intersect(const triangle3b &a, const triangle3b &b);


struct parallelogram3b : plane_shape2 {
//	parallelogram3b(param(position3) a, param(position3) b, param(position3) c) : x(b - a), y(c - a), z(a) {}

	position3		corner(RECT_CORNER i)	const	{ return z + select(!(i & 1), -x, x) + select(!(i & 2), -y, y); }
	position3		centre()				const	{ return z; }
	cuboid			get_box()				const;

	bool			contains(param(position3) p) const {
		float3		r	= p / matrix();
		return approx_equal(r.z, zero) && abs(r.x) <= one && abs(r.y) <= one;
	}
	bool			check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal = 0) const {
		float3x4	m	= float3x4(x, y, -d, z);
		float3		r	= p / m;
		if (r.z >= zero && abs(r.x) <= one && abs(r.y) <= one) {
			t = r.z;
			if (normal)
				*normal = this->normal();
			return true;
		}
		return false;
	}
	bool			check_ray(param(ray3) r, float &t, vector3 *normal = 0)	const {
		return check_ray(r.p, r.d, t, normal);
	}
	auto			solid_angle(param(position3) p)		const {
		float3 t = z - p;
		return	triangle_solid_angle(t - x - y, t + x - y, t - x + y)
			+	triangle_solid_angle(t - x + y, t + x - y, t + x + y);
	}
	interval<float>	projection(param(float3) n) const {
		float	dz = dot(n, z), h = dot(abs(n), abs(x) + abs(y));
		return {dz - h, dz + h};
	}
	interval<float>	projection(param(iso::plane) p) const {
		float	dz = p.dist(z), h = dot(abs(p.normal()), abs(x) + abs(y));
		return {dz - h, dz + h};
	}
	interval<float>	intersection(const line3 &line) const;
};

//-----------------------------------------------------------------------------
// quadric
//-----------------------------------------------------------------------------

// d4x.x.x + d4y.y.y + d4z.z.z + d4w.w.w + 2 d3x.x.y + 2 d3y.y.z + 2 d3z.z.w + 2 ox.x.z + 2 oy.y.w + 2 oz.w.x = 0

class quadric : public symmetrical4 {
public:
	enum TYPE {ELLIPSOID, PARABOLOID, HYPERBOLOID1, HYPERBOLOID2, SADDLE, CYLINDER, CONE, HYPERBOLA, PARABOLA};

	struct info {
		TYPE	type;
		SIGN	orientation;
		bool	empty;
		bool	degenerate;
		info(TYPE _type, SIGN _orientation, bool _none, bool _degenerate) : type(_type), orientation(_orientation), empty(_none), degenerate(_degenerate) {}
	};

	static quadric unit_sphere()		{ return quadric(float4(one, one, one, -one), zero, zero); }
	static quadric unit_paraboloid()	{ return quadric(float4(one, one, zero, zero), float3(zero, zero, -one), zero); }
	static quadric unit_saddle()		{ return quadric(float4(one, -one, zero, zero), float3(zero, zero, -one), zero); }
	static quadric unit_hyperboloid1()	{ return quadric(float4(one, one, -one, one), zero, zero); }
	static quadric unit_hyperboloid2()	{ return quadric(float4(one, one, -one, -one), zero, zero); }

	//degenerates
	static quadric unit_cylinder()		{ return quadric(float4(one, one, zero, -one), zero, zero); }
	static quadric unit_cone()			{ return quadric(float4(one, one, -one, zero), zero, zero); }
	static quadric unit_hyperbola()		{ return quadric(float4(one, -one, zero, one), zero, zero); }
	static quadric unit_parabola()		{ return quadric(float4(one, zero, zero, zero), zero, float3(zero, one, zero)); }
	static quadric xy_plane()			{ return quadric(zero, float3(zero, zero, one), zero); }
	static quadric z_axis()				{ return quadric(float4(one, one, zero, zero), zero, zero); }

	static quadric plane_pair(const iso::plane &p1, const iso::plane &p2);
	static quadric plane(const iso::plane &p1);

	quadric() {}
	quadric(param(_triangular4) m) : symmetrical4(m)	{}

	explicit quadric(const _unit_sphere&)	: quadric(unit_sphere()) {}
	explicit quadric(const sphere &s)		: quadric(s.matrix() * unit_sphere()) {}
	explicit quadric(const ellipsoid &e)	: quadric(e.matrix() * unit_sphere()) {}


	template<typename D, typename O1, typename O2> force_inline quadric(const D &d, const O1 &o1, const O2 &o2) : symmetrical4(d, o1, o2)	{}

	info		analyse()						const;
//	float		evaluate(param(float4) p)		const	{ return dot(p, float4x4(*this) * p); }
//	float		evaluate(param(position3) p)	const	{ return evaluate(float4(p)); }
	float		evaluate(param(float4) p)		const	{ return dot(p * p, d4) + two * (dot(float3(o.zy, d3.z), p.xyz * p.w) + dot(float3(d3.xy, o.x), p.xyz * p.yzx)); }
	float		evaluate(param(position3) p)	const	{ return dot(p * p, d4.xyz) + two * (dot(float3(o.zy, d3.z), p) + dot(float3(d3.xy, o.x), p.xyz * p.yzx)) + d4.w; }

	float4		centre4()						const	{ return cofactors(*this).column<3>(); }
	position3	centre()						const	{ return iso::project(centre4()); }
	position3	centre(quadric &q)				const	{ position3 c = centre(); q = quadric(float4(d4.xyz, evaluate(c)), float3(d3.xy, zero), float3(o.x,zero,zero)); return c; }
	float		volume()						const	{ auto t = strip_z(*this).det(); return pi * det() / t * rsqrt(t); }	// guess!
	void		flip()									{ d4 = -d4; d3 = -d3; o = -o; }

	quadric&	add_param(param(float4) p)				{ d4 += p.xyzw * p.xyzw; d3 += p.xyz * p.yzw; o += p.xyw * p.zwx; return *this; }
	quadric&	add_plane_error(param(iso::plane) p, float weight) { return add_param(p.as_float4() * weight); }

	bool		check_ray(param(ray3) r, float &t, vector3 *normal);
	cuboid		get_box()						const;
	float3x4	get_tangents()					const;
	conic		project_z()						const;
	position3	support(param(float3) v)		const;
	conic		project(param(iso::plane) p)	const	{ return (*this / p.from_xy_plane()).project_z(); }
	conic		operator&(const axis_s<0>&)		const	{ return strip_x(*this); }
	conic		operator&(const axis_s<1>&)		const	{ return strip_y(*this); }
	conic		operator&(const axis_s<2>&)		const	{ return strip_z(*this); }
	conic		operator&(param(iso::plane) p)	const	{ return strip_z(*this / p.from_xy_plane()); }

	friend quadric operator-(const quadric &a)						{ return a._neg(); }
	friend quadric operator+(const quadric &a, const quadric &b)	{ return a._add(b); }
	friend quadric operator-(const quadric &a, const quadric &b)	{ return a._sub(b); }
	template<typename T> friend enable_if_t<vget_hasr<T>::value,conic> operator*(param(quadric) a, const T &b)	{ return a._mul(b); }

	//	template<typename M> quadric operator/(const M &m) const	{ return mul_mulT(transpose(m), *this); }
	//	template<typename M> friend force_inline IF_MAT(M, quadric) operator/(param(quadric) q, const M &m) { return mul_mulT(transpose(m), q); }
	//	template<typename M> friend force_inline IF_MAT(M, quadric) operator*(const M &m, param(quadric) q) { return q / inverse(m); }
	template<typename M> friend quadric operator*(const inverse_s<M> &m, const quadric &q)	{ return mul_mulT(transpose(m), q); }
	template<typename M> friend IF_MAT(M, quadric) operator*(const M &m, const quadric &q)	{ return mul_mulT(cofactors(m), q); }
};

//	New Quadric Metric for Simplifying Meshes with Appearance Attributes (Hugues Hoppe)
//	extends a quadric to hold information about N additional parameters (e.g. colour, uvs)
//
// [   Q     Q     Q     Q	-g0.x -g1.x	... -gn.x]
// [   Q     Q     Q	 Q	-g0.y -g1.y	... -gn.y]
// [   Q     Q     Q	 Q	-g0.z -g1.z	... -gn.z]
// [   Q     Q     Q	 Q	 g0.w  g1.w	...  gn.w]
// [ -g0.x -g0.y -g0.z  g0.w  a    0	...   0  ]
// [ -g1.x -g1.y -g1.z  g1.w  0    a	...   0  ]
// [ ...				      0    0	...   0  ]
// [ -gn.x -gn.y -gn.z	gn.w  0    0	...   a  ]

template<int N> struct quadric_params : quadric {
	float4	g[N];
	float	a;

	quadric_params() { clear(*this); }

	quadric		get_grad_quadric() const {
		symmetrical4	s(zero);
		for (auto &i : g)
			s = s + outer_product(i);
		return s / a;
	}

	// return gradient matrix
	float4x4	add_triangle(param(position3) p0, param(position3) p1, param(position3) p2) {
		vector3	normal	= cross(p1 - p0, p2 - p0);
		float	weight	= len(normal);

		quadric::add_param(float4(normal, -dot(p0, normal)));
		a += weight;

		float4x4	iAt	= float4x4(p0, p1, p2, float4(normal, zero));
		return get(inverse(transpose(iAt))) * weight;
	}

	// grad.v gives value for all v homogeneous points
	void	add_gradient(int i, param(float4) grad) {
		g[i] += grad;
		quadric::add_param(grad);
	}

	float	get_param(int i, param(float4) pos) {
		return dot(g[i], pos) / a;
	}

	float	evaluate(param(float4) pos, const float *vals) const {
		float4	A	= float4x4((const symmetrical4&)*this) * pos;
		float	B	= 0;
		for (int i = 0; i < N; i++) {
			A	-= g[i] * vals[i];
			B	+= vals[i] * (a * vals[i] - dot(g[i], pos));
		}
		return dot(pos, A) + B;
	}

	quadric_params& operator+=(const quadric_params &b) {
		d4 += b.d4;
		d3 += b.d3;
		o += b.o;
		for (int i = 0; i < N; i++)
			g[i] += b.g[i];
		a += b.a;
		return *this;
	}
	friend quadric_params operator+(const quadric_params &a, const quadric_params &b) {
		return quadric_params(a) += b;
	}
};

//-----------------------------------------------------------------------------
// tetrahedron
//-----------------------------------------------------------------------------

class tetrahedron : float3x4 {
public:
	tetrahedron() {}
	tetrahedron(param(position3) a, param(position3) b, param(position3) c, param(position3) d) : float3x4(b - a, c - a, d - a, a)	{}
	tetrahedron(const position3 *p) : float3x4(p[1] - p[0], p[2] - p[0], p[3] - p[0], p[0])	{}

	position3	centre()							const	{ return w; }
	float3		parametric(param(position3) p)		const	{ return p / *(const float3x4*)this;			}
	float4		barycentric(param(position3) p)		const	{ float3 t = parametric(p); return float4(t, one - sum_components(t)); }
	position3	from_parametric(param(float3) p)	const	{ return *(const float3x4*)this * position3(p);	}
	position3	corner(int i)						const	{ return select(i < 3, w + (*this)[i], w); }
	iso::plane	plane(int i)						const	{ return i < 3 ? iso::plane(cross(column(inc_mod(i, 3)), column(dec_mod(i, 3))), w) : iso::plane(cross(z - x, y - x), position3(w + x)); }
	triangle3	face(int i)							const	{ return triangle3(corner((i + 1) & 3), corner((i + 2) & 3), corner((i + 3) & 3)); }
	float1		volume()							const	{ return det() / 6.f; }

	const float3x4&	matrix()						const	{ return *this; }
	inverse_s<float3x4&>	inv_matrix()			const	{ return inverse(matrix()); }

	// columns are planes
	float4x4	planes_matrix()						const	{
		float4x4	m	= transpose(inv_matrix());
		float4		s	= m.x + m.y + m.z;
		m.w	= float4(-s.xyz, one);
		return m;
	}

	bool		contains(param(position3) p)		const	{ return all(barycentric(p) > zero); }
	bool		check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const;
	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const { return check_ray(r.p, r.d, t, normal);	}
	bool		is_visible(param(float4x4) m)		const;
	bool		is_within(param(float4x4) m)		const;
	position3	closest(param(position3) p)			const;
	position3	support(param(float3) v)			const;

	sphere		inscribe()				const;
	sphere		circumscribe()			const;
	cuboid		get_box()				const;

	fixed_array<position3,4>	corners() const { return {w + x, w + y, w + z, w}; }
	fixed_array<iso::plane,4>	planes() const {
		float4x4	m = planes_matrix();
		return {iso::plane(m.x), iso::plane(m.y), iso::plane(m.z), iso::plane(m.w)};
	}

	void					operator*=(param(float3x4) m)	{ float3x4::operator=(m * matrix()); }
	friend float1			dist(param(tetrahedron) t, param(iso::plane) p) {
		float1	dx	= dot(p.normal(), t.x);
		float1	dy	= dot(p.normal(), t.y);
		float1	dz	= dot(p.normal(), t.z);
		return p.dist(t.centre()) + min(min(min(dx, dy), dz), zero);
	}
};

//-----------------------------------------------------------------------------
// cuboid
//-----------------------------------------------------------------------------

force_inline bool overlap(const interval<position3> &x, const interval<position3> &y) {
	return all(x.b >= y.a) && all(x.a <= y.b);
}

struct cuboid : interval<position3> {
	typedef interval<position3> B;

	force_inline cuboid()		{}
	force_inline	cuboid(const _none&)								: B(position3((float)iso::maximum), position3((float)iso::minimum))	{}
	force_inline	cuboid(const _zero&)								: B(position3(zero), position3(zero))		{}
	force_inline cuboid(const _one&)									: B(position3(-one), position3(one))		{}
	force_inline cuboid(param(position3) a, param(position3) b)	: B(a, b)				{}
	force_inline cuboid(param(position3) c, param(vector3) h)		: B(c - h, c + h)	{}
	force_inline cuboid(param(vector3) h)							: B(-h, h)			{}
	force_inline cuboid(const interval<position3> &i)				: B(i)				{}
	template<typename A, typename B> static	force_inline cuboid	corner_size(const A &a, const B &b)	{ return cuboid(position3(a), position3(a + b)); }
	template<typename A, typename B> static	force_inline cuboid	centre_half(const A &a, const B &b)	{ return cuboid(position3(a - b), position3(a + b)); }

	static cuboid			minimum(position3 *p, uint32 n);

//	force_inline position3&	pt0()									{ return a; }
//	force_inline position3&	pt1()									{ return b; }
//	force_inline position3	pt0()							const	{ return a; }
//	force_inline position3	pt1()							const	{ return b; }
	force_inline position3	centre()						const	{ return (a + b) * half;	}
	force_inline vector3		extent()						const	{ return b - a;			}
	force_inline vector3		half_extent()					const	{ return extent() * half;	}
	force_inline float3x4	matrix()						const	{ return translate(centre()) * scale(half_extent()); }
	force_inline float3x4	inv_matrix()					const	{ return scale(reciprocal(half_extent())) * translate(-centre()); }
	force_inline position3	corner(CUBE_CORNER i)			const	{ return select(uint8(i), a, b);	}
	force_inline iso::plane	plane(CUBE_PLANE i)				const	{ int axis = i >> 1; bool side = !!(i & 1); return iso::plane(float4(float3(select(side, one, -one), zero, zero) >> axis, float3(select(side, -b, a) << axis).x)); }
	force_inline float1		volume()						const	{ return prod_components(extent()); }

	force_inline rectangle	rect_xy()						const	{ return rectangle(a.xy, b.xy);	}
	force_inline rectangle	rect_xz()						const	{ return rectangle(a.xz, b.xz);	}
	force_inline rectangle	rect_yz()						const	{ return rectangle(a.yz, b.yz);	}

	force_inline cuboid		operator| (param(cuboid) x)		const	{ return cuboid((position3)min(a, x.a), (position3)max(b, x.b)); }
	force_inline cuboid&		operator|=(param(cuboid) x)				{ a = min(a, x.a); b = max(b, x.b); return *this; }
	force_inline cuboid		operator| (param(position3) x)	const	{ return cuboid((position3)min(a, x), (position3)max(b, x)); }
	force_inline cuboid&		operator|=(param(position3) x)			{ a = min(a, x); b = max(b, x); return *this;		}
//	force_inline cuboid&		operator|=(param(float4) x)				{ return operator|=(project(x));	}
	force_inline cuboid		operator& (param(cuboid) x)		const	{ return cuboid((position3)max(a, x.a), (position3)min(b, x.b));	}
	force_inline cuboid&		operator&=(param(cuboid) x)				{ a = max(a, x.a); b = min(b, x.b); return *this;	}

	force_inline cuboid		operator+ (param(float3) x)		const	{ return cuboid(position3(a + x), position3(b + x));	}
	force_inline cuboid&		operator+=(param(float3) x)				{ return *this = *this + x;;							}
	force_inline cuboid		operator- (param(float3) x)		const	{ return cuboid(position3(a - x), position3(b - x));	}
	force_inline cuboid&		operator-=(param(float3) x)				{ return *this = *this - x;;							}
	force_inline cuboid		operator* (param(float3) s)		const	{ position3 a1 = a * s, b1 = b * s; return cuboid((position3)min(a1, b1), (position3)max(a1, b1)); }
	force_inline cuboid&		operator*=(param(float3) s)				{ return *this = *this * s;								}
	force_inline cuboid		operator/ (param(float3) s)		const	{ return *this * reciprocal(s);							}
	force_inline cuboid&		operator/=(param(float3) s)				{ return *this *= reciprocal(s);						}

	force_inline cuboid		expand(param(vector3) x)		const	{ return cuboid(position3(a - x), position3(b + x));	}
//	force_inline bool		overlaps(param(cuboid) x)		const	{ return all(b >= x.a) && all(a <= x.b); }
	force_inline bool		contains(param(cuboid) x)		const	{ return all(x.a >= a) && all(x.b <= b); }
	force_inline bool		contains(param(position3) x)	const	{ return all(x <= b) && all(x >= a); }
	force_inline bool		is_empty()						const	{ return any(a >= b); }
	force_inline	const cuboid&	get_box()					const	{ return *this; }

	force_inline	bool		is_visible(param(float4x4) m)	const;
	force_inline bool		is_within(param(float4x4) m)	const;
	force_inline	uint32		visibility_flags(param(float4x4) m, uint32 flags)	const;
	bool					check_ray(param(position3) p, param(vector3) d) const;
	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const;
	force_inline	bool		check_ray(param(ray3) r)							const	{ return check_ray(r.p, r.d);				}
	force_inline	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const	{ return check_ray(r.p, r.d, t, normal);	}
	bool					clip(position3 &a, position3 &b)	const;

	force_inline sphere		inscribe()						const	{ return sphere(centre(), min_component(half_extent()));	}
	force_inline sphere		circumscribe()					const	{ return sphere(centre(), len(half_extent()));				}

	parallelogram3			face(CUBE_PLANE i) const {
		int	a = i / 4, x = (i & 1) * 3;
		return parallelogram3(
			corner((CUBE_CORNER)rotate_left(0 ^ x, a, 3)),
			corner((CUBE_CORNER)rotate_left(2 ^ x, a, 3)),
			corner((CUBE_CORNER)rotate_left(4 ^ x, a, 3))
//			corner((CUBE_CORNER)rotate_left(6 ^ x, a, 3))
		);
	}

	fixed_array<position3,8>	corners() const {
		return {
			perm<0,1,2>(a, b),
			perm<3,1,2>(a, b),
			perm<0,4,2>(a, b),
			perm<3,4,2>(a, b),
			perm<0,1,5>(a, b),
			perm<3,1,5>(a, b),
			perm<0,4,5>(a, b),
			perm<3,4,5>(a, b),
		};
	}
	fixed_array<iso::plane,6>	planes() const {
		return {
			iso::plane(float4( one, zero, zero,  a.x)),
			iso::plane(float4(-one, zero, zero, -b.x)),
			iso::plane(float4(zero,  one, zero,  a.y)),
			iso::plane(float4(zero, zero, -one, -b.y)),
			iso::plane(float4(zero, zero,  one,  a.z)),
			iso::plane(float4(zero, zero, -one, -b.z)),
		};
	}

	friend int max_component_index(param(cuboid) c)	{ return max_component_index(c.extent()); }
};

class _unit_cube : public cuboid {
public:
	static bool				is_visible(param(float4x4) m);
	static bool				is_within(param(float4x4) m);
	static bool				is_visible2(param(float4x4) m);
	static int				visibility_flags(param(float4x4) m, uint32 flags);
	static int				visibility_flags2(param(float4x4) m, uint32 flags);
	static bool				clip_test(param(position3) a, param(position3) b);
	static int				clip_test(param(position3) a, param(position3) b, float2 &t);
	static int				clip_test(param(vector4) a, param(vector4) b, float2 &t);
	static bool				clip(position3 &a, position3 &b);
	static bool				clip(vector4 &a, vector4 &b);

	_unit_cube() : cuboid(one)	{}
	force_inline static vector3		extent()						{ return vector3(2); }
	force_inline static bool			contains(param(position3) b)	{ return all(abs(b) <= one); }
	force_inline static bool			contains(param(vector4) b)		{ float4 t = abs(b); return all(t.xyz <= t.w); }
	force_inline static bool			is_empty()						{ return false; }
	force_inline static float1		volume()						{ return one; }
	force_inline static _identity	matrix()						{ return _identity(); }
	force_inline static _identity	inv_matrix()					{ return _identity(); }
};
extern const _unit_cube unit_cube;

force_inline	bool		cuboid::is_visible(param(float4x4) m)						const { return unit_cube.is_visible(m * matrix()); }
force_inline	bool		cuboid::is_within(param(float4x4) m)						const { return unit_cube.is_within(m * matrix()); }
force_inline	uint32		cuboid::visibility_flags(param(float4x4) m, uint32 flags)	const { return flags ? unit_cube.visibility_flags(m * matrix(), flags) : 0; }

template<typename I> cuboid get_box(I i0, I i1) {
	cuboid	r(empty);
	for (; i0 != i1; ++i0)
		r	|= *i0;
	return r;
}
template<typename C> cuboid get_box(const C &c) {
	return get_box(c.begin(), c.end());
}

force_inline				cuboid		ray3::get_box()				const { return cuboid::centre_half(centre(), abs(d) * half); }
//template<> force_inline cuboid		circle3::get_box()			const { return cuboid::centre_half(centre(), (abs(shift<1>(pl.normal())) + abs(shift<2>(pl.normal()))) * radius()); }
template<> force_inline	cuboid		circle3::get_box()			const { return cuboid::centre_half(centre(), sqrt((float3(one) - square(normal()) / len2(normal())) * radius2())); }
force_inline				cuboid		sphere::get_box()			const { return cuboid(position3(centre() - radius()), position3(centre() + radius())); }
force_inline				cuboid		ellipsoid::get_box()		const { return cuboid(centre(), vector3(sqrt(square(axis1()) + square(axis2()) + square(axis3())))); }
template<> force_inline	cuboid		triangle3::get_box()		const { float3x4 m = matrix(); return cuboid(position3(min(m.x, m.y, zero) + m.w), position3(max(m.x, m.y, zero) + m.w)); }
template<>force_inline	cuboid		parallelogram3::get_box()	const { float3x4 m = matrix(); float3 h = abs(m.x) + abs(m.y); return cuboid(m.w - h, m.w + h); }
force_inline				cuboid		tetrahedron::get_box()		const { return cuboid(position3(min(x, y, z, zero) + w), position3(max(x, y, z, zero) + w)); }
force_inline				cuboid		triangle3b::get_box()		const { return cuboid(position3(min(x, y, zero) + z), position3(max(x, y, zero) + z)); }
force_inline				cuboid		parallelogram3b::get_box()	const { float3 h = abs(x) + abs(y); return cuboid(z - h, z + h); }

//-----------------------------------------------------------------------------
// parallelepiped
//-----------------------------------------------------------------------------

class parallelepiped : public float3x4 {
public:
	force_inline parallelepiped()			{}
	force_inline explicit parallelepiped(const _one &)		: float3x4(identity)	{}
	force_inline explicit parallelepiped(const float3x4 &m)	: float3x4(m)			{}
	force_inline parallelepiped(param(cuboid) cuboid) 		: float3x4(cuboid.matrix())	{}
	force_inline parallelepiped(param(position3) a, param(vector3) b, param(vector3) c, param(vector3) d)
		: float3x4(b * half, c * half, d * half, a + (b + c + d) * half)	{}

	force_inline cuboid		get_box()						const	{ return cuboid(w, vector3(abs(x) + abs(y) + abs(z))); }
	force_inline sphere		circumscribe()					const	{ return sphere(w, len(x + y + z));	}
	force_inline sphere		inscribe()						const	{ return sphere(centre(), min_component(half_extent())); }

	force_inline const float3x4&	matrix()					const	{ return *this; }
	force_inline inverse_s<float3x4&>	inv_matrix()		const	{ return inverse(matrix()); }
	force_inline position3	centre()						const	{ return translation(); }
	force_inline vector3		half_extent()					const	{ return float3x3::scale(); }
	force_inline vector3		extent()						const	{ return half_extent() * 2; }
	force_inline float1		volume()						const	{ return det(); }

	force_inline parallelepiped&		right()							{ if (det() < 0) z = -z; return *this; }
	force_inline bool		contains(param(parallelepiped) b)const	{ float3x4 m = b.matrix() / inv_matrix(); return all(abs(m.x)+abs(m.y)+abs(m.z)+abs(m.w) <= one); }
	force_inline bool		contains(param(position3) p)	const	{ position3 t = p / matrix(); return all(abs(t) <= one); }
	force_inline position3	corner(CUBE_CORNER i)			const	{ return w + select(!(i & 1), -x, x) + select(!(i & 2), -y, y) + select(!(i & 4), -z, z); }
	force_inline iso::plane	plane(CUBE_PLANE i)				const	{ return matrix() * plane::unit_cube(i); }

	force_inline	bool		is_visible(param(float4x4) m)	const	{ return unit_cube.is_visible(m * matrix()); }
	force_inline bool		is_within(param(float4x4) m)	const	{ return unit_cube.is_within(m * matrix()); }
	force_inline	uint32		visibility_flags(param(float4x4) m, uint32 flags)	const { return flags ? unit_cube.visibility_flags(m * matrix(), flags) : 0; }
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const	{ return matrix() * position3(sign1(inv_matrix() * v)); }

	bool					check_ray(param(position3) p, param(vector3) d)	const;
	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal)	const;
	force_inline	bool		check_ray(param(ray3) r)							const	{ return check_ray(r.p, r.d); }
	force_inline	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const	{ return check_ray(r.p, r.d, t, normal); }
	bool					clip(position3 &p1, position3 &p2) const;

	parallelogram3			face(CUBE_PLANE i) const {
		int	a = i / 4, x = (i & 1) * 3;
		return parallelogram3(
			corner((CUBE_CORNER)rotate_left(0 ^ x, a, 3)),
			corner((CUBE_CORNER)rotate_left(2 ^ x, a, 3)),
			corner((CUBE_CORNER)rotate_left(4 ^ x, a, 3))
		);
	}
	fixed_array<position3,8>	corners() const {
		return {
			w - x - y - z,
			w + x - y - z,
			w - x + y - z,
			w + x + y - z,
			w - x - y + z,
			w + x - y + z,
			w - x + y + z,
			w + x + y + z,
		};
	}
	fixed_array<iso::plane,6>	planes() const {
		float3x3	m	= cofactors((float3x3)matrix());
		return {
			iso::plane(-m.x, w - x),
			iso::plane( m.x, w + x),
			iso::plane(-m.y, w - y),
			iso::plane( m.y, w + y),
			iso::plane(-m.z, w - z),
			iso::plane( m.z, w + z),
		};
	}
	void					operator*=(param(float3x4) m)			{ float3x4::operator=(m * matrix()); }
	friend parallelepiped	operator*(param(float3x4) m, param(parallelepiped) b)	{ return parallelepiped(m * b.matrix()); }
	friend bool				quick_test(param(parallelepiped) a, param(parallelepiped) b);

	friend float1			dist(param(parallelepiped) x, param(iso::plane) p) {
		float1	dx	= abs(dot(p.normal(), x.x));
		float1	dy	= abs(dot(p.normal(), x.y));
		float1	dz	= abs(dot(p.normal(), x.z));
		return p.dist(x.centre()) - dx - dy - dz;
	}
};

force_inline parallelepiped fullmul(param(float3x4) m, param(cuboid) b)				{ return parallelepiped(m * translate(b.centre()) * scale(b.half_extent())); }

//-----------------------------------------------------------------------------
// obb3
//-----------------------------------------------------------------------------

class obb3 : public parallelepiped {
public:
	force_inline obb3()			{}
	force_inline explicit obb3(const _one &)			: parallelepiped(identity)	{}
	force_inline explicit obb3(const float3x4 &m)	: parallelepiped(m)			{}
	force_inline explicit obb3(param(cuboid) cuboid) : parallelepiped(cuboid.matrix())	{}

	static obb3	minimum(position3 *p, uint32 n);

	force_inline obb3&		right()							{ if (det() < 0) z = -z; return *this; }
};

force_inline parallelepiped fullmul(param(float3x4) m, param(obb3) b)	{ return parallelepiped(m * b.matrix()); }
force_inline obb3 operator*(param(float3x4) m, param(obb3) b)			{ return obb3(m * b.matrix()); }
force_inline obb3 operator*(param(float3x4) m, param(cuboid) b)			{ return obb3(m * translate(b.centre()) * scale(b.half_extent())); }

//-----------------------------------------------------------------------------
// frustum
//-----------------------------------------------------------------------------

class frustum : public float4x4 {
public:
	static frustum	from_planes(plane p[6]) {
		float4	cw = (p[0].as_float4() + p[1].as_float4()) * -half;
		return frustum(cofactors(float4x4(p[1].as_float4() + cw, p[3].as_float4() + cw, p[5].as_float4() + cw, cw)));
	}

	force_inline frustum()			{}
	force_inline explicit frustum(const float4x4 &m) : float4x4(m)	{}

	force_inline const float4x4&		matrix()				const	{ return *this; }
	force_inline inverse_s<float4x4&>	inv_matrix()		const	{ return inverse(matrix());	}

//	force_inline bool		contains(param(frustum) obb2)	const	{ float3x4 m = obb2.matrix() / inv_matrix(); return all(abs(m.x)+abs(m.y)+abs(m.z)+abs(m.w) <= one); }
	force_inline bool		contains(param(position3) p)	const	{ float4 t = p / matrix(); return all(abs(t.xyz) <= t.w); }
	force_inline float4		corner(CUBE_CORNER i)			const	{ return w + select(!(i & 1), -x, x) + select(!(i & 2), -y, y) + select(!(i & 4), -z, z); }
	force_inline float4		origin()						const	{ return -z; }
	force_inline iso::plane	plane(CUBE_PLANE i)				const	{ return matrix() * plane::unit_cube(i); }
	force_inline iso::plane	inv_plane(CUBE_PLANE i)			const	{ return plane::unit_cube(i) / matrix(); }

	force_inline	bool		is_visible(param(float4x4) m)						const { return unit_cube.is_visible(m * matrix()); }
	force_inline bool		is_within(param(float4x4) m)						const { return unit_cube.is_within(m * matrix()); }
	force_inline	uint32		visibility_flags(param(float4x4) m, uint32 flags)	const { return unit_cube.visibility_flags(m * matrix(), flags); }

	cuboid					get_box()										const;
	bool					clip(vector4 &p1, vector4 &p2)					const;
	bool					check_ray(param(position3) p, param(vector3) d) const;
	force_inline	bool		check_ray(param(ray3) r)						const	{ return check_ray(r.p, r.d); }

	fixed_array<float4,8>	corners() const {
		return {
			w - x - y - z,
			w + x - y - z,
			w - x + y - z,
			w + x + y - z,
			w - x - y + z,
			w + x - y + z,
			w - x + y + z,
			w + x + y + z,
		};
	}
	fixed_array<iso::plane,6>	planes() const {
		float4x4	m = cofactors(matrix());
		return {
			iso::plane(m.w - m.x),
			iso::plane(m.w + m.x),
			iso::plane(m.w - m.y),
			iso::plane(m.w + m.y),
			iso::plane(m.w - m.z),
			iso::plane(m.w + m.z),
		};
	}
};

force_inline frustum operator*(param(float4x4) m, param(cuboid) b)			{ return frustum(m * b.matrix()); }
force_inline frustum operator*(param(float4x4) m, param(parallelepiped) b)	{ return frustum(m * b.matrix()); }

//-----------------------------------------------------------------------------
// baseclass for cylinder, cone, capsule
//-----------------------------------------------------------------------------

class _directed_shape {
protected:
	float4		v;
	vector3		d;
	force_inline _directed_shape(param(position3) pos, param(vector3) dir, float r) : v(pos, r), d(dir)	{}
	_directed_shape		mul(param(float3x4) m)	const { return _directed_shape(m * centre(), m * d, radius() * len(m.x)); }
public:
	force_inline position3	centre()	const	{ return v.xyz;	}
	force_inline vector3		dir()		const	{ return d;		}
	force_inline float1		radius()	const	{ return v.w;	}
	force_inline float3x4	matrix()	const	{ return translate(centre()) * look_along_z(dir()) * scale(float3(radius().xx, len(dir()))); }
	force_inline inverse_s<float3x4>	inv_matrix()	const	{ return inverse(matrix());	}

	void					operator*=(param(float3x4) m)	{ v = float4(m * centre(), radius() * len(m.x)); d = m * d; }
};

//-----------------------------------------------------------------------------
// cylinder
//-----------------------------------------------------------------------------

class cylinder : public _directed_shape {
	force_inline explicit cylinder(const _directed_shape &s) : _directed_shape(s)	{}
public:
	force_inline cylinder(param(position3) pos, param(vector3) dir, float r) : _directed_shape(pos, dir, r)	{}
	force_inline cylinder(param(position3) p1, param(position3) p2, float r) : _directed_shape((p1 + p2) / 2, (p2 - p1) / 2, r)	{}
//	force_inline cuboid		get_box()		const	{ return cuboid(centre(), vector3(abs(d) + (abs(shift<1>(d)) + abs(shift<2>(d))) * (radius() / len(d)))); }
	force_inline	cuboid		get_box()		const	{ return cuboid::centre_half(centre(), abs(d) + sqrt(float3(one) - square(d) / len2(d))* radius()); }

	force_inline sphere		circumscribe()	const	{ return sphere(centre(), sqrt(len2(d) + square(radius())));	}
	force_inline sphere		inscribe()		const	{ return sphere(centre(), min(radius(), len(d))); }
	force_inline float1		volume()		const	{ return len(d) * square(radius()) * pi;	}

	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const;
	force_inline	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const	{ return check_ray(r.p, r.d, t, normal);	}
	bool					is_visible(param(float4x4) m)	const;
	bool					is_within(param(float4x4) m)	const;
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;
	bool					contains(param(position3) p)	const;

	operator quadric() const { return matrix() * quadric::unit_cylinder(); }

	friend float1			dist(param(cylinder) c, param(plane) p) {
		float1	dz	= abs(dot(p.normal(), c.dir()));
		float1	dx	= sqrt(one - square(dz) / len2(c.dir())) * c.radius();
		return p.dist(c.centre() - dz - dx);
	}

	friend cylinder operator*(param(float3x4) m, param(cylinder) c) { return cylinder(c.mul(m)); }
};

//-----------------------------------------------------------------------------
// cone
//-----------------------------------------------------------------------------

class cone : public _directed_shape {
	force_inline explicit cone(const _directed_shape &s) : _directed_shape(s)	{}
public:
	force_inline cone(param(position3) pos, param(vector3) dir, float r) : _directed_shape(pos, dir, r)	{}
	force_inline cuboid		get_box()		const	{
		vector3 v = (abs(shift<1>(d)) + abs(shift<2>(d))) * (radius() / len(d));
		return cuboid(position3(centre() + min(d, -v - d)), position3(centre() + max(d, v - d)));
	}
	force_inline sphere		circumscribe()	const	{
		float	h2	= len2(d);
		float	r2	= square(radius() * half);
		return r2 > h2 ? sphere(centre() - d, radius()) : sphere(centre() - d * (r2 / h2), sqrt(h2) * (1 + (r2 / h2)));
	}
	force_inline sphere		inscribe()		const	{
		float	h2	= len2(d), rh = radius() * half;
		float	r	= (sqrt(h2 + square(rh)) + rh) / (sqrt(h2 + square(rh)) + rh);
		return sphere(centre() + dir() * (r - 1), r * rsqrt(h2));
	}
	force_inline float1		volume()		const	{ return pi / 3.0f * 2 * len(d) * square(radius());	}
	force_inline circle3		base()			const	{ return circle3(centre() - dir(), dir(), radius()); }
	force_inline position3	apex()			const	{ return centre() + dir();	}

	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const;
	force_inline	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const	{ return check_ray(r.p, r.d, t, normal);	}
	bool					is_visible(param(float4x4) m)	const;
	bool					is_within(param(float4x4) m)	const;
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;
	bool					contains(param(position3) p)	const;

	operator quadric() const {
		float3x4	m = translate(apex()) * look_along_z(dir()) * scale(float3(radius().xx, len(dir()) * two));
		return m * quadric::unit_cone();
	}

	friend float1			dist(param(cone) c, param(plane) p) {
		float1	dz	= dot(p.normal(), c.dir());
		if (dz > 0)
			return p.dist(c.centre() - dz);

		float1	dx	= sqrt(one - square(dz) / len2(c.dir())) * c.radius();
		return p.dist(c.centre() + dz + dx);
	}

	friend cone operator*(param(float3x4) m, param(cone) c) { return cone(c.mul(m)); }
};

//-----------------------------------------------------------------------------
// capsule
//-----------------------------------------------------------------------------

class capsule : public _directed_shape {
	force_inline explicit capsule(const _directed_shape &s) : _directed_shape(s)	{}
public:
	force_inline capsule(param(position3) pos, param(vector3) dir, float r) : _directed_shape(pos, dir, r)	{}
	force_inline cuboid		get_box()		const	{ return cuboid(centre(), vector3(abs(vector3(d)) + radius()));	}
	force_inline sphere		circumscribe()	const	{ return sphere(centre(), len(d) + radius()); }
	force_inline sphere		inscribe()		const	{ return sphere(centre(), radius()); }
	force_inline float1		volume()		const	{ return pi * len(d) * square(radius()) + (pi * 4.0f / 3.0f) * cube(radius());	}

	bool					check_ray(param(position3) p, param(vector3) d, float &t, vector3 *normal) const;
	force_inline	bool		check_ray(param(ray3) r, float &t, vector3 *normal)	const	{ return check_ray(r.p, r.d, t, normal);	}
	bool					is_visible(param(float4x4) m)	const;
	bool					is_within(param(float4x4) m)	const;
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;
	bool					contains(param(position3) p)	const;

	friend float1			dist(param(capsule) c, param(plane) p) {
		float1	dz	= abs(dot(p.normal(), c.dir()));
		return p.dist(c.centre() - dz - c.radius());
	}

	friend capsule operator*(param(float3x4) m, param(capsule) c) { return capsule(c.mul(m)); }
};

//-----------------------------------------------------------------------------
// wedge3
//	infinite 3d wedge defined by a point and three normals
//-----------------------------------------------------------------------------

struct wedge3 : float3x4 {
	wedge3()	{}
	wedge3(param(position3) a, param(float3) x, param(float3) y, param(float3) z) : float3x4(x, y, z, a)	{}

	bool	contains(param(position3) b) const {
		return dot(x, b - w) >= zero && dot(y, b - w) >= zero && dot(z, b - w) >= zero;
	}

	template<typename S> bool contains_shape(const S &s) const {
		float3	v1	= s.support(x) - w;
		if (dot(x, v1) < zero)
			return false;

		float3	v2	= s.support(y) - w;
		if (dot(y, v2) < zero)
			return false;

		float3	v3	= s.support(z) - w;
		if (dot(z, v3) < zero)
			return false;

		return dot(v1, cross(v2 - v1, v3 - v1)) > zero || s.contains(w);
	}
};

//-----------------------------------------------------------------------------
// sphere_visibility
//-----------------------------------------------------------------------------

struct sphere_visibility {
	float4x4	matrix;
	vector3		cull3;

	static float3 check_clip(param(float4) p) {
		float4	a = abs(p);
		return a.xyz - a.w;
	}

	sphere_visibility(const float4x4 &_matrix) : matrix(_matrix) {
		cull3	= sqrt(
			square(matrix.x.xyz + matrix.x.w)
		+	square(matrix.y.xyz + matrix.y.w)
		+	square(matrix.z.xyz + matrix.z.w)
		);
	}
	bool is_visible(const sphere &s) const {
		float3	t	= check_clip(matrix * s.centre());
		return all(t < cull3 * s.radius());
	}
	bool is_visible(const cylinder &s) const {
		float4x4	m2	= matrix * s.matrix();
		vector3		c	= sqrt(
			square(m2.x.xyz + m2.x.w)
		+	square(m2.y.xyz + m2.y.w)
		);
		float3		a	= check_clip(m2.z) + c;
		return all(check_clip(m2.w) < a);
	}
	bool is_visible(const cone &s) const {
		float4x4	m2	= matrix * s.matrix();
		float4		a	= m2.w + m2.z;	// apex

		//apex in frustum?
		if (all(check_clip(a) <= zero))
			return true;

		vector3		c	= sqrt(
			square(m2.x.xyz + m2.x.w)
		+	square(m2.y.xyz + m2.y.w)
		);

		//base in frustum?
		float4		b	= m2.w - m2.z;	// base
		if (all(check_clip(b) <= c))
			return true;

		if (any((a.xyz >  a.w) & (b.xyz - c > b.w)))
			return false;

		if (any((a.xyz < -a.w) & (b.xyz + c < b.w)))
			return false;

		return true;
	}
	bool is_visible(const capsule &s) const {
		float3x4	mat	= translate(s.centre()) * look_along_z(s.dir());
		float4x4	m2	= matrix * mat;
		float3		a	= check_clip(m2.z * len(s.dir())) + cull3 * s.radius();
		return all(check_clip(m2.w) < a);
	}
};

//-----------------------------------------------------------------------------
// rectangles
//-----------------------------------------------------------------------------

int sub_rectangle(param(rectangle) r, rectangle *rects, int num, int max);
int add_rectangle(param(rectangle) r, rectangle *rects, int num, int max, int add);
int opt_rectangles(rectangle *rects, int num, float within);

struct rectangles0 : trailing_array<rectangles0, rectangle> {
	int				num;
	int				size()	const	{ return num; }
	iterator		end()			{ return &array(num);	}
	const_iterator	end()	const	{ return &array(num);	}
};

template<int N> struct rectangles : rectangles0 {
	rectangle	rects[N];

	rectangles()								{ num = 0; }
	rectangles(param(rectangle) r)				{ rects[0] = r; num = 1; }
	rectangles&	operator-=(param(rectangle) r)	{ num = sub_rectangle(r, rects, num, N); return *this; }
	rectangles&	operator+=(param(rectangle) r)	{ num = add_rectangle(r, rects, num, N, num); return *this; }
	int	optimise(float within = 0)				{ return num = opt_rectangles(rects, num, within); }
};

//-----------------------------------------------------------------------------
} //namespace iso

#endif	// GEOMETRY_H
