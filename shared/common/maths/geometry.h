#ifndef NEW_GEOMETRY_H
#define NEW_GEOMETRY_H

#include "base/vector.h"
#include "base/array.h"
#include "base/functions.h"
#include "base/bits.h"

#undef small
#undef major
#undef minor
#undef cone

namespace iso {

#undef IF_SCALAR
#define IF_SCALAR(T,R)	enable_if_t<!is_mat<T> && num_elements_v<T> == 1, R>

struct curve_vertex {
	float	x, y;
	uint32	flags;
};

enum PLANE {
	PLANE_MINUS_X	= 0,
	PLANE_PLUS_X	= 1,
	PLANE_MINUS_Y	= 2,
	PLANE_PLUS_Y	= 3,
	PLANE_MINUS_Z	= 4,
	PLANE_PLUS_Z	= 5,
};

template<int A> static constexpr PLANE PLANE_MINUS	= PLANE(A * 2 + 0);
template<int A> static constexpr PLANE PLANE_PLUS	= PLANE(A * 2 + 1);

enum CORNER {
	CORNER_MM	= 0,
	CORNER_PM	= 1,
	CORNER_MP	= 2,
	CORNER_PP	= 3,

	CORNER_MMM	= 0,
	CORNER_PMM	= 1,
	CORNER_MPM	= 2,
	CORNER_PPM	= 3,
	CORNER_MMP	= 4,
	CORNER_PMP	= 5,
	CORNER_MPP	= 6,
	CORNER_PPP	= 7,
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
}

enum SIGN {NEG = 3, ZERO = 0, POS = 1};

inline SIGN get_sign1(float x)			{ return x < 0 ? NEG : SIGN(x > 0); }
inline SIGN operator-(SIGN s)			{ return SIGN(-(int)s & 3); }
inline SIGN operator*(SIGN a, SIGN b)	{ return SIGN(((int)a * (int)b) & 3); }

inline CORNER clockwise(int i)			{ return CORNER(i ^ (i >> 1)); }
inline CORNER counter_clockwise(int i)	{ return CORNER(i ^ (i >> 1) ^ 1); }

template<int N> inline auto corners() 	{ return int_range(CORNER(1 << N)); }
template<int N> inline auto planes() 	{ return int_range(PLANE(2 * N)); }

inline auto corners2_cw() {
	static const CORNER c[] = {CORNER_MM, CORNER_PM, CORNER_PP, CORNER_MP };
	return make_range(c);
}

inline auto corners2_ccw() {
	static const CORNER c[] = {CORNER_PM, CORNER_MM, CORNER_MP, CORNER_PP };
	return make_range(c);
}

template<typename T> auto corner(const interval<T> &ext, CORNER i) {
	return select((int)i, ext.a, ext.b);
}

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

template<typename I> auto centroid(I i0, I i1) {
	typedef noref_t<decltype(*i0)>	R;
	typedef decltype(*i0 - *i1)		T;
	T		total(zero);

	if (i0 == i1)
		return R(total);

	size_t		n		= distance(i0, i1);
	for (; i0 != i1; ++i0)
		total += T(*i0);
	return R(total / int(n));
}

template<typename C> auto centroid(const C &c) {
	return centroid(begin(c), end(c));
}

template<typename I, typename V> I support(I i0, I i1, V v) {
	auto		imax	= i0;

	if (i0 != i1) {
		auto		dmax	= dot(v, (V)*i0);
		for (++i0; i0 != i1; ++i0) {
			auto	d	= dot(v, (V)*i0);
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

template<typename S> auto	any_point(const S &s)	{ return s.centre(); }

//-----------------------------------------------------------------------------
// generators
//-----------------------------------------------------------------------------

template<typename X, typename Y> auto generate2(X x, Y y) {
	return [x, y](int k, int n) { return float2{x(k, n), y(k, n)}; };
}
template<typename X, typename Y, typename Z> auto generate3(X x, Y y, Z z) {
	return [x, y, z](int k, int n) { return float3{x(k, n), y(k, n), z(k, n)}; };
}

inline float			linear(int k, int n)	{ return (k + half) / n; }
inline auto				halton(int base)		{ return [base](int k, int n) { return corput(base, k); }; }
template<int B1> float	halton(int k, int n)	{ return corput<B1>(k); }

inline auto				hammersley2(int b1)					{ return generate2(linear, halton(b1)); }
inline auto				halton2(int b1, int b2)				{ return generate2(halton(b1), halton(b2)); }
template<int B1> inline auto hammersley2(int k, int n)		{ return float2{linear(k, n), corput<B1>(k)}; }
template<int B1, int B2> inline auto halton2(int k, int n)	{ return float2{corput<B1>(k), corput<B2>(k)}; }


inline auto				hammersley3(int b1, int b2)					{ return generate3(linear, halton(b1), halton(b2)); }
inline auto				halton3(int b1, int b2, int b3)				{ return generate3(halton(b1), halton(b2), halton(b3)); }
template<int B1, int B2> inline auto hammersley3(int k, int n)		{ return float3{linear(k, n), corput<B1>(k), corput<B2>(k)}; }
template<int B1, int B2, int B3> inline auto halton3(int k, int n)	{ return float3{corput<B1>(k), corput<B2>(k), corput<B3>(k)}; }

// 2d perimeter
template<typename T> auto perimeter_optimised(const T &shape) { return [&shape](float t) { return uniform_perimeter(shape, t); }; }
template<typename T, typename G, typename R> void generate_perimeter(const T &shape, G&& generator, R result, uint32 n) {
	auto&&	perimeter = perimeter_optimised(shape);
	for (uint32 k = 0; k < n; k++)
		*result++ = perimeter(generator(k, n));
}
template<typename T, typename G> auto generate_perimeter(const T &shape, G&& generator, uint32 n) {
	return transformc(int_range(n), [perimeter = perimeter_optimised(shape), generator, n](uint32 k) { return perimeter(generator(k, n)); });
}

// 2d or 3d interior
template<typename S> struct interior_optimised_s {
	const S	&shape;
	interior_optimised_s(const S &shape) : shape(shape) {}
	template<typename T> auto	operator()(T t) { return  uniform_interior(shape, t); };
};
template<typename T> auto interior_optimised(const T &shape) {
	return interior_optimised_s<T>(shape);
	//return [&shape](param(float2) t) { return uniform_interior(shape, t); };
}
template<typename T, typename G, typename R> void generate_interior(const T &shape, G&& generator, R result, uint32 n) {
	auto&&	interior = interior_optimised(shape);
	for (uint32 k = 0; k < n; k++)
		*result++ = interior(generator(k, n));
}
template<typename T, typename G> auto generate_interior(const T &shape, G&& generator, uint32 n) {
	return transformc(int_range(n), [interior = interior_optimised(shape), generator, n](uint32 k) { return interior(generator(k, n)); });
}

// 3d surface
template<typename T> auto surface_optimised(const T &shape) { return [&shape](param(float2) t) { return uniform_surface(shape, t); }; }
template<typename T, typename G, typename R> void generate_surface(const T &shape, G&& generator, R result, uint32 n) {
	auto&&	surface = surface_optimised(shape);
	for (uint32 k = 0; k < n; k++)
		*result++ = surface(generator(k, n));
}
template<typename T, typename G> auto generate_surface(const T &shape, G&& generator, uint32 n) {
	return transformc(int_range(n), [surface = surface_optimised(shape), generator, n](uint32 k) { return surface(generator(k, n)); });
}

//-----------------------------------------------------------------------------
// coordinate systems
//-----------------------------------------------------------------------------

struct polar_coord {
	union {
		struct { float	theta, r; };
		float2	v;
	};
	polar_coord() {}
	explicit	polar_coord(position2 d)	: theta(atan2(d.v.y, d.v.x)), r(len(d.v)) {}
	explicit	polar_coord(float2 v)		: v(v) {}
	polar_coord(float theta, float r)		: theta(theta), r(r)	{}
	polar_coord& operator=(const polar_coord &b)	{ v = b.v; return *this; }
	operator position2()	const { return position2(sincos(theta) * r); }
};

struct spherical_dir {
	union {
		struct { float	theta, phi; };
		float2	v;
	};
	static spherical_dir uniform(param(float2) d) { return spherical_dir(two * acos(sqrt(one - d.x)), two * pi * d.y); }

	spherical_dir() {}
	explicit	spherical_dir(vector3 d)	: v(atan2(float2{len(d.xy), d.y}, d.zx)) {}
	explicit	spherical_dir(float2 v)		: v(v) {}
	spherical_dir(float theta, float phi)	: theta(theta), phi(phi) {}
	spherical_dir& operator=(const spherical_dir &b)	{ v = b.v; return *this; }
	operator vector3()		const { float4 t = sin(concat(v, v + pi / two)); return concat(t.wy * t.x, t.z); }

};

struct spherical_coord : spherical_dir {
	float	r;
	spherical_coord() {}
	spherical_coord(param(spherical_dir) dir, float r)		: spherical_dir(dir), r(r) {}
	explicit		spherical_coord(param(position3) d)		: spherical_dir(acos(d.v.z / len(d.v)), atan2(d.v.y, d.v.x)), r(len(d.v)) {}
	spherical_dir	dir()	const { return *this; }
	operator position3()	const { return position3(operator vector3() * r); }
};

//-----------------------------------------------------------------------------
// ray
//-----------------------------------------------------------------------------

template<typename T> class normalised {
	T	t;
public:
	normalised(const T &t) : t(normalise(t)) {}
	operator const T&()	const { return t; }
};

template<typename T> auto	normalise(const normalised<T> &r) { return r; }

template<typename E> force_inline float2x3	ray_matrix(pos<E,2> p, vec<E, 2> d)			{ return float2x3(perp(d), d, p.v); }
template<typename E> force_inline float3x4	ray_matrix(pos<E,3> p, vec<E, 3> d)			{ return translate(p) * transpose(look_along_z(d)); }

template<typename E> force_inline float2x3	ray_inv_matrix(pos<E,2> p, vec<E, 2> d)		{ return inverse(ray_matrix(p, d)); }
template<typename E> force_inline float3x4	ray_inv_matrix(pos<E,3> p, vec<E, 3> d)		{ return look_along_z(d) * translate(-p); }

template<typename E, int N> class ray {
protected:
	typedef pos<E, N>	P;
	typedef vec<E, N>	D;
public:
	P		p;
	D		d;

	static	force_inline ray	with_centre(P p, D d)	{ return ray(p - d, p + d); }
	ray() {}
	ray(decltype(zero)) : p(zero), d(zero)	{}
	ray(P p, D d)		: p(p), d(d)		{}
	ray(P p0, P p1)		: p(p0), d(p1 - p0)	{}
	P		pt0()								const		{ return p; }
	P		pt1()								const		{ return p + d;	}
	D		dir()								const		{ return d;	}
	P		from_parametric(float t)			const		{ return p + d * t;	}
	P		centre()							const		{ return p + d * half; }
	float	project_param(P v)					const		{ return dot(v - p, d) / len2(d); }
	float	dist(P v)							const		{ return len(cross(v - p, d)) / len(d); }
	P		closest(P p)						const		{ return from_parametric(clamp(project_param(p), zero, one)); }
	bool	approx_on(P v, float tol = ISO_TOLERANCE) const	{ auto t = v - p; return approx_equal((float)len(t) + (float)len(t - d), (float)len(d), tol); }
	ray		operator-()							const		{ return {p, -d}; }

	auto	get_box()							const;
	force_inline auto	matrix()				const		{ return ray_matrix(p, d); }
	force_inline auto	inv_matrix()			const		{ return ray_inv_matrix(p, d); }

	template<typename M> friend IF_MAT(M, ray) operator*(const M &m, const ray &r) { return {m * r.p, m * r.d}; }
//	friend P	operator&(param(ray) r1, param(ray) r2)	{ return P((r1.d * cross(r2.p.v, r2.d) - r2.d * cross(r1.p.v, r1.d)) / cross(r1.d, r2.d)); }
};

template<typename E, int N> ray<E, N> make_ray(pos<E, N> p, vec<E, N> x) { return {p, x}; }

template<typename E, int N> class normalised<ray<E,N>> : public ray<E, N> {
	typedef ray<E, N>	B;
	using typename B::P;
	using D = normalised<typename B::D>;
public:
	using B::p; using B::d;
	normalised(const B &r)	: B(r.p, normalise(r.d))	{}
//	normalised(P p, D d)	: B(p, normalise(d))		{}
	normalised(P p, D d)	: B(p, d)					{}
	normalised(P p0, P p1)	: B(p0, normalise(p1 - p0))	{}
	float		project_param(P v)	const	{ return dot(v - p, d); }
	P			closest(P p)		const	{ return from_parametric(project_param(p)); }
	float		dist2(P v)			const	{ return len2(cross(v - p, d)); }
	float		dist(P v)			const	{ return sqrt(dist2(v)); }
};

template<typename E, int N> normalised<ray<E,N>>	normalise(const ray<E,N> &r) { return r; }

// gives t parameter for r1
template<typename E> E intersection_param(const ray<E,2> &r1, const ray<E,2> &r2) {
	return cross(r2.p.v - r1.p.v, r2.d) / cross(r1.d, r2.d);
}

template<typename E> auto	operator&(const ray<E,2> &r1, const ray<E,2> &r2)	{
	return pos<E,2>((r1.d * cross(r2.p.v, r2.d) - r2.d * cross(r1.p.v, r1.d)) / cross(r1.d, r2.d));
}

//-----------------------------------------------------------------------------
// line (but 2d uses n_plane)
//-----------------------------------------------------------------------------

template<typename E, int N> class line : public ray<E, N> {
	typedef ray<E, N> B;
public:
	using B::B;
	line(line<E, N - 1> b) : B(pos<E,N>(b.pt0()), vec<E,N>(b.dir(b)))	{}
};

template<typename E, int N> class normalised<line<E, N>> : public normalised<ray<E, N>> {
	typedef normalised<ray<E, N>>	B;
public:
	using B::B;
};

// compute the line parameters of the two closest points
template<typename E, int N> vec<E, 2> closest_params(const line<E, N> &r1, const line<E, N> &r2) {
	auto	w = r1.p - r2.p;
	auto	a = dot(r1.d, r1.d);
	auto	b = dot(r1.d, r2.d);
	auto	c = dot(r2.d, r2.d);
	auto	d = dot(r1.d, w);
	auto	e = dot(r2.d, w);
	auto	D = a * c - b * b;

	if (D < epsilon)	// if the lines are almost parallel use the largest denominator
		return {zero, b > c ? d / b : e / c};
	return vec<E, 2>{b * e - c * d, a * e - b * d} / D;
}


//-----------------------------------------------------------------------------
// n_plane
//-----------------------------------------------------------------------------

template<typename _E, int _N> class n_plane {
protected:
	static const int N	=	_N;
	typedef	_E				E;
	typedef vec<E, N + 1>	V;
	typedef pos<E, N>		P;
	typedef vec<E, N>		D;
	V	v;
public:
	force_inline 	n_plane() {}
	explicit 		n_plane(V v)						: v(v)	{}
	explicit 		n_plane(PLANE i) 					: v(concat(select(1<<(i>>1), D(E(i & 1) * two - one), zero), -one)) {}
	explicit 		n_plane(decltype(infinity))			: v(axis_s<N>()) {}
	force_inline 	n_plane(D normal, const float dist)	: v(concat(normal, -dist))	{}
	force_inline 	n_plane(D normal, P pos)			: v(concat(normal, -dot(pos.v, normal))) {}
	template<int A> n_plane(const axis_s<A> &a)			: v(a)	{}

	n_plane(P p0, P p1, P p2)	: n_plane(cross(p1 - p0, p2 - p0), p0) {}	// 3d only
	n_plane(P p0, P p1)			: n_plane(perp(p1 - p0), p0) {}				// 2d only

	force_inline n_plane operator-()			const	{ return n_plane(-v); }
	force_inline D		normal()				const	{ return shrink<N>(v); }
	force_inline bool	test(P p)				const	{ return dot(v, V(p)) >= zero;	}
	force_inline bool	test(V p)				const	{ return dot(v, p) * p[N] >= zero; }
	// only use if not constructed normalised
	force_inline E		unnormalised_dist()		const	{ return -v[N]; }
	force_inline E		unnormalised_dist(P p)	const	{ return dot(v, V(p));	}
	force_inline E		unnormalised_dist(V p)	const	{ return dot(v, p);	}
	force_inline E		normalised_dist(P p)	const	{ return unnormalised_dist(p) * rsqrt(len2(normal())); }
	force_inline E		normalised_dist(V p)	const	{ return unnormalised_dist(p) * rsqrt(len2(normal())); }

	force_inline V		project(V p1, V p2)		const	{ return p1 * dot(p2, v) - p2 * dot(p1, v);	}

	bool				clip(P &p0, P &p1)		const;
	bool				clip(V &p0, V &p1)		const;
	bool				ray_check(ray<E, N> r, float &t, D *normal)	const;
	bool				approx_on(P v, float tol = ISO_TOLERANCE)	const	{ return approx_equal(dot(normal(), v), unnormalised_dist(), tol); }

	friend const V&		as_vec(const n_plane &p)							{ return p.v;	}

	friend bool			coplanar(paramT(n_plane) a, paramT(n_plane) b)						{ return colinear(a.normal(), b.normal()); }
	friend bool			approx_equal(const n_plane &a, const n_plane &b, float tol = ISO_TOLERANCE)	{ return approx_equal(a.v * rotate(b.v), rotate(a.v) * b.v, tol); }

//	friend n_plane		operator*(const float3x4 &m, paramT(n_plane) p)						{ return translate(get_trans(m)) * (get_rot(m) * p); }
//	friend n_plane		operator/(param(n_plane) p, const float3x4 &m)						{ return p / translate(get_trans(m)); }//ERROR / get_rot(m); }
	friend bool			operator==(const n_plane &a, const n_plane &b)						{ return all(a.v * rotate(b.v) == b.v * rotate(a.v)); }
	template<typename M> friend IF_MAT(M, n_plane) operator*(const M &m, paramT(n_plane) p)	{ return n_plane(cofactors(m) * p.v); }
	template<typename M> friend IF_MAT(M, n_plane) operator/(paramT(n_plane) p, const M &m)	{ return n_plane(transpose(m) * p.v); }
	friend n_plane		operator*(const translate_s<E, N> &m, paramT(n_plane) p)			{ return n_plane(concat(p.normal(), p.unnormalised_dist(P(-m.t)))); }
	friend n_plane		operator/(paramT(n_plane) p, const translate_s<E,N> &m)				{ return n_plane(concat(p.normal(), p.unnormalised_dist(m.t))); }
};

template<typename D, typename X> n_plane<element_type<D>, num_elements_v<D>> make_plane(D n, X x) { return {n, x}; }

template<typename E, int N> bool n_plane<E, N>::clip(P &p0, P &p1) const {
	auto	d0 = unnormalised_dist(p0);
	auto	d1 = unnormalised_dist(p0);
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		p0 -= (p1 - p0) * d0 / (d1 - d0);
	} else if (d1 < zero) {
		p1 -= (p1 - p0) * d1 / (d1 - d0);
	}
	return true;
}

template<typename E, int N> bool n_plane<E, N>::clip(V &p0, V &p1) const {
	auto	d0 = dot(select(p0[N] < 0, -p0, p0), v);
	auto	d1 = dot(select(p1[N] < 0, -p1, p1), v);
	if (d0 < zero) {
		if (d1 < zero)
			return false;
		p0 -= (p1 - p0) * d0 / (d1 - d0);
	} else if (d1 < zero) {
		p1 -= (p1 - p0) * d1 / (d1 - d0);
	}
	return true;
}

template<typename E, int N> bool n_plane<E, N>::ray_check(ray<E, N> r, float &t, D *normal) const {
	auto	d0 = unnormalised_dist(r.p);
	auto	d1 = unnormalised_dist(r.p + r.d);
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

template<typename E>	pos<E,2>	operator&(const n_plane<E,2> &a, const n_plane<E,2> &b)	{
	return pos<E,2>(cross(as_vec(a), as_vec(b)));
}

template<typename E, int N> class normalised<n_plane<E,N>> : public n_plane<E, N> {
	typedef n_plane<E, N>	B;
	using typename B::V;
	using typename B::P;
	// using typename B::D;
	using D = normalised<typename B::D>;
	using B::v;
	static V _normalise(V v) {
		auto	d = len2(v.xyz);
		return d ? v * rsqrt(d) : v;
	}
public:
	using B::B;
	force_inline 	normalised(B b)							: B(_normalise(as_vec(b))) {}
//	force_inline 	normalised(D normal, const float dist)	: B(normalise(normal), dist) {}
//	force_inline 	normalised(D normal, P pos)				: B(normalise(normal), pos)	{}
	force_inline 	normalised(D normal, const float dist)	: B(normal, dist) {}
	force_inline 	normalised(D normal, P pos)				: B(normal, pos)	{}
	normalised(P p0, P p1, P p2)	: B(normalise(cross(p1 - p0, p2 - p0)), p0) {}	// 3d only
	normalised(P p0, P p1)			: B(normalise(perp(p1 - p0)), p0) {}			// 2d only

	force_inline E		dist()				const	{ return -v[N]; }
	force_inline P		pt0()				const	{ return P(B::normal() * -v[N]); }
	force_inline E		dist(P p)			const	{ return B::unnormalised_dist(p); }
	force_inline E		dist(V p)			const	{ return B::unnormalised_dist(p); }
	force_inline P		project(P p)		const	{ return p - B::normal() * dist(p); }

	friend normalised	bisector(paramT(normalised) a, paramT(normalised) b)	{ return normalised(a.v - b.v); }
	friend P			operator&(const ray<E,N> &a, paramT(normalised) b)		{ return a.p - a.d * b.dist(a.p) / dot(a.d, b.normal()); }
};

template<typename E, int N> normalised<n_plane<E,N>>	normalise(const n_plane<E,N> &r) { return r; }


template<bool NEG> struct maybe_neg;
template<> struct maybe_neg<false>	{ template<typename T> static const T&	f(const T& t) { return t; }	};
template<> struct maybe_neg<true>	{ template<typename T> static auto		f(const T& t) { return -t; }};

template<PLANE P> struct unit_plane {
	typedef maybe_neg<!(P & 1)>	sign;
	static auto	normal()			{ return sign::f(axis_s<P / 2>()); }
	static auto	dist()				{ return one; }
	static auto	unnormalised_dist()	{ return one; }
	template<typename E, int N> operator n_plane<E, N>() { return P; }

	template<typename V>		static auto	unnormalised_dist(V v)			{ return sign::f(v[P / 2]) - one; }
	template<typename E, int N> static auto	unnormalised_dist(pos<E, N> p)	{ return unnormalised_dist(p.v); }
	template<typename E, int N> static auto	unnormalised_dist(homo<E, N> p)	{ return sign::f(p.v[P / 2]) - p.v[N]; }

	template<typename E, int N> friend auto	operator&(const ray<E,N> &a, const unit_plane&)	{ return a.from_parametric((sign::f(one) - a.p.v[P / 2]) / a.d[P / 2]); }
};

//http://geomalgorithms.com/a05-_intersect-1.html
template<typename E> line<E,3>		operator&(const normalised<n_plane<E,3>> &a, const normalised<n_plane<E,3>> &b) {
	auto	d	= cross(a.normal(), b.normal());
	return {pos<E,3>(cross(d, a.normal() * b.dist() - b.normal() * a.dist()) / dot(d, d)), d};
}

template<typename E, PLANE P> line<E,3>	operator&(const normalised<n_plane<E,3>> &a, const unit_plane<P> &b) {
	auto	d	= cross(a.normal(), b.normal());
	return {pos<E,3>(cross(d, a.normal() - vec<E,3>(b.normal()) * a.dist()) / dot(d, d)), d};
}

// 2d line is n_plane

template<typename E> class line<E, 2> : public n_plane<E, 2> {
	typedef n_plane<E, 2> B;
public:
	using B::B;
	vec<E,2>	dir()						const	{ return perp(B::normal()); }
};

template<typename E> class normalised<line<E, 2>> : public normalised<n_plane<E, 2>> {
	typedef normalised<n_plane<E, 2>>	B;
public:
	using B::B;
	vec<E,2>	dir()						const	{ return perp(B::normal()); }
};

//template<typename E> auto	operator&(const normalised<n_plane<E,3>> &a, const axis_s<2>&)	{ return normalised<line<E,2>>(as_vec(a).xyw); }
template<typename E> auto	operator&(const n_plane<E,3> &a, const axis_s<2>&)	{ return line<E,2>(as_vec(a).xyw); }

//-----------------------------------------------------------------------------
// n_sphere
//-----------------------------------------------------------------------------

template<typename E, int N, typename R> bool n_sphere_check_ray(vec<E,N> v, vec<E,N> d, float &t, vec<E,N> *normal, R r2) {
	auto	b = -dot(d, v);
	if (b < zero)			// closest point is 'before' p
		return false;

	auto	c = dot(v, v) - r2;
	if (c < zero)			// p is inside
		return false;

	auto	a = dot(d, d);
	auto	x = b * b - a * c;
	if (x < zero)			// line misses
		return false;

	x = b - sqrt(x);
	if (x < zero || x > a)
		return false;		// intersection of line is off ends of segment

	t = x / a;
	if (normal)
		*normal = (d * t + v) * rsqrt(r2);

	return true;
}

template<typename E, int N, typename R> E n_sphere_ray_closest(vec<E,N> v, vec<E,N> d, R r2) {
	auto	b = dot(d, v);
	auto	c = dot(v, v) - r2;
//	if (c < zero)			// p is inside
//		return zero;

	auto	a = dot(d, d);
	auto	x = b * b - a * c;
	return x < zero	? -b / a : (-b + sign(b) * sqrt(x)) / a;
}

template<typename E, int N> class n_sphere {
protected:
	typedef vec<E, N+1>		V;
	typedef pos<E, N>		P;
	typedef vec<E, N>		D;
	typedef mat<E, N, N+1>	M;
	V	v;
public:
	static n_sphere	with_r2(P c, float r2) { return n_sphere(concat(c.v, r2)); }
	n_sphere() {}
	n_sphere(const _none&)		: v(concat(D(zero), -one))	{}
	n_sphere(const _one&)		: v(concat(D(zero), one))	{}
	explicit n_sphere(V v)		: v(v)	{}
	n_sphere(const P c, float r): v(concat(c.v, square(r))) {}

	static n_sphere			through(P a, P b);
	static n_sphere			through(P a, P b, P c);
	static n_sphere			through(P a, P b, P c, P d);				//3d only
	static n_sphere			small(const P *p, uint32 n);
	static bool				through_contains(P p, P q, P r, P t);		//2d only
	static bool				through_contains(P p, P q, P r, P s, P t);	//3d only

	force_inline P			centre()				const	{ return P(shrink<N>(v)); }
	force_inline E			radius2()				const	{ return v[N]; }
	force_inline E			radius()				const	{ return sqrt(radius2()); }
	force_inline bool		empty()					const	{ return radius2() < zero; }
	auto					get_box()				const;
	force_inline const n_sphere& circumscribe()		const	{ return *this; }
	force_inline const n_sphere& inscribe()			const	{ return *this; }

	n_sphere				operator| (P p)			const;
	n_sphere				operator| (n_sphere b)	const;
	force_inline n_sphere&	operator|=(P p)					{ return *this = *this | p; }
	force_inline n_sphere&	operator|=(n_sphere b)			{ return *this = *this | b; }

	force_inline M			matrix()				const	{ return translate(centre()) * scale(radius()); }
	force_inline M			inv_matrix()			const	{ return scale(reciprocal(radius())) * translate(P(-v.xyz)); }

	bool					contains(P pos)			const	{ return len2(pos - centre()) < radius2(); }
	bool					ray_check(const ray<E,N> &r, float &t, D *normal)	const { return n_sphere_check_ray<E, N>(r.p - centre(), r.d, t, normal, radius2()); }
	E						ray_closest(const ray<E,N> &r)						const { return n_sphere_ray_closest<E, N>(r.p - centre(), r.d, radius2()); }

	P						closest(P p)			const	{ return centre() + normalise(p - centre()) * radius(); }
	P						support(D v)			const	{ return centre() + normalise(v) * radius(); }

	n_sphere				operator*=(const M &m)				{ v = concat((m * centre()).v, radius2() * len2(m.x)); return *this; }
	template<typename B> IF_SCALAR(B,n_sphere&)	operator*=(B b)	{ v[N] *= square(b); return *this; }

	friend n_sphere			operator*(const M &m, const n_sphere &s)	{ return with_r2(m * s.centre(), s.radius2() * len2(m.x)); }
	friend float			dist(n_sphere s, n_plane<E, N> p)			{ return p.dist(s.centre()) - s.radius(); }
	friend float			dist(n_sphere s, P p)						{ return len(p - s.centre()) - s.radius(); }
	friend float			dist(n_sphere s, ray<E, N> r)				{ return max(r.dist(s.centre()) - s.radius(), zero); }
};

template<typename E> force_inline E	area(const n_sphere<E, 2> &c)			{ return c.radius2() * pi; }
template<typename E> force_inline E	volume(const n_sphere<E,3> &s)			{ return cube(s.radius()) * (pi * 4 / 3); }
template<typename E> force_inline E	surface_area(const n_sphere<E,3> &s)	{ return s.radius2() * (pi * 4); }

template<typename E, int N> n_sphere<E, N> n_sphere<E, N>::through(P a, P b) {
	auto	r	= (b - a) * half;
	return with_r2(a + r, len2(r));
}
#if 0
template<typename E, int N> n_sphere<E, N> n_sphere<E, N>::through(P a, P b, P c) {
	auto	x	= b - a;
	auto	y	= c - a;
	auto	t	= cross(x, y);
	float2	r	= (x * len2(y) - y * len2(x)) / (t * two);
	return with_r2(a + perp(r), len2(r));
}
#endif
template<typename E, int N> n_sphere<E, N> n_sphere<E, N>::through(P a, P b, P c) {
	auto	x	= b - a;
	auto	y	= c - a;
	auto	t	= cross(x, y);
	auto	r	= (cross(t, x) * len2(y) + cross(y, t) * len2(x)) / (len2(t) * 2);
	return with_r2(a + r, len2(r));
}

//N==3 only
template<typename E, int N> n_sphere<E, N> n_sphere<E, N>::through(P a, P b, P c, P d) {
	auto	x	= b - a;
	auto	y	= c - a;
	auto	z	= d - a;
	float	det	= dot(x, cross(y, z));
	auto	r	= (cross(y, z) * len2(x) + cross(z, x) * len2(y) + cross(x, y) * len2(z)) / (det * 2);
	return with_r2(a + r, len2(r));
}

template<typename E, int N> n_sphere<E, N> n_sphere<E, N>::small(const P *p, uint32 n) {
	P		c	= centroid(p, p + n);
	E		r2	= 0;
	for (uint32 i = 0; i < n; i++)
		r2 = max(r2, len2(c - p[i]));
	return n_sphere<E, N>::with_r2(c, r2);
}

//N==2 only
template<typename E, int N> bool n_sphere<E, N>::through_contains(P p, P q, P r, P t) {
	auto	qp = q - p;
	auto	rp = r - p;
	auto	tp = t - p;
	return float2x2(float4{
		cross(qp, tp), dot(tp, t - q),
		cross(qp, rp), dot(rp, r - q)
	}).det() < 0;
}

//N==3 only
template<typename E, int N> bool n_sphere<E, N>::through_contains(P p, P q, P r, P s, P t) {
	auto	pt	= p - t;
	auto	qt	= q - t;
	auto	rt	= r - t;
	auto	st	= s - t;
	return float4x4(
		concat(pt, len2(pt)),
		concat(rt, len2(rt)),
		concat(qt, len2(qt)),
		concat(st, len2(st))
	).det() < 0;
}

template<typename E, int N> n_sphere<E, N> n_sphere<E, N>::operator|(P p) const {
	if (empty())
		return {p, zero};

	if (contains(p))
		return *this;

	auto	v	= p - centre();
	auto	d	= len(v);
	auto	r	= radius();
	return {centre() + v * ((d - r) / d * half), (d + r) * half};
}

template<typename E, int N> n_sphere<E, N> n_sphere<E, N>::operator|(n_sphere b) const {
	auto	v	= b.centre() - centre();
	auto	d	= len(v);
	auto	r0	= radius();
	auto	r1	= b.radius();
	auto	r	= (max(r0, d + r1) - min(-r0, d - r1)) * half;
	return {centre() + v * ((r - r0) / d), r};
}

template<typename E, int N> class unit_n_sphere : public n_sphere<E, N> {
	typedef n_sphere<E, N>	B;
	using typename B::P;
	using typename B::D;
public:
	unit_n_sphere() : n_sphere<E, N>(one)	{}
	static bool		contains(P pos)			{ return len2(pos.v) < one; }
	static bool		ray_check(const ray<E, N> &r, float &t, D *normal)	{ return n_sphere_check_ray<E, N>(r.p, r.d, t, normal, one); }
	static E		ray_closest(const ray<E,N> &r)						{ return n_sphere_ray_closest<E, N>(r.p, r.d, one); }
	static P		closest(P p)			{ return P(normalise(p.v)); }
	static P		support(D d)			{ return P(normalise(d)); }
};

//-----------------------------------------------------------------------------
// aabb
//-----------------------------------------------------------------------------

template<typename E, int N> class aabb : public interval<pos<E, N>> {
protected:
	typedef interval<pos<E, N>>	B;
	typedef pos<E,N>		P;
	typedef vec<E,N>		D;
	typedef mat<E, N, N+1>	M;
public:
	using B::B;

	force_inline aabb()	{}
	force_inline aabb(const B& b) : B(b) {}
	force_inline explicit aabb(const vec<E, N * 2>& b) : B(P(b.lo), P(b.hi)) {}

	force_inline M			matrix()			const	{ return translate(B::centre()) * scale(B::half_extent()); }
	force_inline M			inv_matrix()		const	{ return scale(reciprocal(B::half_extent())) * translate(-B::centre()); }
	force_inline const aabb& get_box()			const	{ return *this; }

	bool				ray_check(const ray<E,N> &r)						const;
	bool				ray_check(const ray<E,N> &r, float &t, D *normal)	const;
	E					ray_closest(const ray<E,N> &r)						const;
	P					support(D d)										const { return P(select(d < zero, B::a, B::b)); }
	P					closest(P p)			const	{ return P(clamp(p, B::a, B::b)); }

	bool				clip(P &p1, P &p2)		const;

	n_sphere<E,N>		inscribe()				const	{ return {B::centre(), reduce_min(B::half_extent())}; }
	n_sphere<E,N>		circumscribe()			const	{ return {B::centre(), len(B::half_extent())}; }

	force_inline P		corner(CORNER i)		const	{ return P(select((int)i, B::a.v, B::b.v));	}
	force_inline auto	plane(PLANE i)			const	{ return normalised<n_plane<E,N>>(select(1<<(i>>1), D(E(i & 1) * two - one), zero), i & 1 ? B::b : B::a); }
	auto				corners()				const	{ return transformc(iso::corners<N>(), [this](CORNER i)	{ return corner(i); }); }
	auto				planes()				const	{ return transformc(iso::planes<N>(), [this](PLANE i)	{ return plane(i); }); }

	friend vec<E,N * 2>	as_vec(aabb r)							{ return concat(r.a.v, r.b.v); }
	friend P			uniform_interior(const aabb &s, D t)	{ return s.a + s.extent() * t; }
	friend aabb			operator*(const translate_s<E, N> &m, const aabb &b) { return b + m.t; }
};


template<typename E, int N> aabb<E, N>	as_aabb(const interval<pos<E, N>> &i)	{ return i; }
template<typename T> auto				as_aabb(const interval<T> &i)			{ return aabb<element_type<T>, num_elements_v<T>>(i); }

template<typename E> force_inline	E	area(const aabb<E,2> &a)	{ return reduce_mul(a.extent()); }
template<typename E> force_inline	E	volume(const aabb<E,3> &a)	{ return reduce_mul(a.extent()); }

template<typename E> force_inline vec<E,3>	abs_cross(vec<E,3> x, vec<E,3> y)	{ return (x.zxy * y + x * y.zxy).zxy; }
template<typename E> force_inline E			abs_cross(vec<E,2> x, vec<E,2> y)	{ return reduce_add(x * y.yx); }

template<typename E, int N> bool aabb<E, N>::ray_check(const ray<E,N> &r) const {
	auto	e		= B::extent();
	auto	m		= (r.p - B::a) + (r.p - B::b) + r.d;
	auto	absd	= abs(r.d);
	return all(abs(m) <= e + absd)
		&& abs(cross(m, r.d)) <= abs_cross(e, absd);
}

template<typename E, int N> bool aabb<E, N>::ray_check(const ray<E,N> &r, float &t, D *normal) const {
	if (any(min(r.p, r.p + r.d) > B::b) || any(max(r.p, r.p + r.d) < B::a))			//entirely outside box
		return false;

	auto	q	= one / r.d;
	auto	vt0	= select(r.d == zero, zero, (select(r.d < zero, B::b, B::a) - (D)r.p) * q);
	auto	vt1	= select(r.d == zero, one,  (select(r.d < zero, B::a, B::b) - (D)r.p) * q);
	auto	t0	= max(reduce_max(vt0), zero);
	auto	t1	= min(reduce_min(vt1), one);
	if (t0 > t1)
		return false;
	t = t0;
	if (normal) {
		int	i = max_component_index(vt0);
		*normal = select(1 << i, -sign(r.d), zero);
	}
	return true;
}

template<typename E, int N> E aabb<E, N>::ray_closest(const ray<E,N> &r) const {
	auto	q	= one / r.d;

	auto	vt0	= select(r.d == zero, zero, (select(r.d < zero, B::b, B::a) - (D)r.p) * q);
	auto	vt1	= select(r.d == zero, one,  (select(r.d < zero, B::a, B::b) - (D)r.p) * q);
	auto	t0	= reduce_max(vt0);
	auto	t1	= reduce_min(vt1);
	if (t0 <= t1)
		return t0 < 0 ? t1 : t0;

	auto	dp	= perp(r.d);
	if (dot(B::centre() - r.p, dp) > zero)
		dp = -dp;
	return dot(support(dp) - r.p, r.d) / len2(r.d);
}

template<typename E, int N> bool aabb<E, N>::clip(P &p1, P &p2) const {
	if (any(min(p1, p2) > B::b) || any(max(p1, p2) < B::a))			//entirely outside box
		return false;

	auto	d	= p2 - p1;
	auto	q	= one / d;
	auto	vt0	= select(d == zero, zero, (select(d < zero, B::b, B::a) - (D)p1) * q);
	auto	vt1	= select(d == zero, one,  (select(d < zero, B::a, B::b) - (D)p1) * q);
	auto	t0	= max(reduce_max(vt0), zero);
	auto	t1	= min(reduce_min(vt1), one);
	if (t0 > t1)
		return false;
	p2	= p1 + d * t1;
	p1	= p1 + d * t0;
	return true;
}

template<typename E, int N> class unit_aabb : public aabb<E, N> {
	typedef aabb<E, N>	B;
	using typename B::P;
	using typename B::D;
public:
	force_inline static D			extent()				{ return two; }
	force_inline static bool		contains(P b)			{ return all(abs(b.v) <= one); }
	force_inline static bool		contains(homo<E,N> b)	{ return all(abs(b.real) <= b.scale); }
	force_inline static bool		is_empty()				{ return false; }
	force_inline static _identity	matrix()				{ return _identity(); }
	force_inline static _identity	inv_matrix()			{ return _identity(); }

	unit_aabb() : B(P(-one), P(one)) {}
	static bool				clip_test(P a, P b);
	static int				clip_test(P a, P b, vec<E,2> &t);
	static int				clip_test(homo<E,N> a, homo<E,N> b, vec<E,2> &t);
	static bool				clip(P a, P b);
	static bool				clip(homo<E,N> &a, homo<E,N> &b);
	static bool				ray_check(const ray<E,N> &r)	{ return clip_test(r.pt0(), r.pt1()); }
	static bool				ray_check(const ray<E,N> &r, float &t, D *normal) {
		float2	t2;
		int		a = clip_test(r.pt0(), r.pt1(), t2);
		if (a < 0)
			return false;
		if (normal)
			*normal = select(1 << a, sign1(-r.d[a]), D(zero));
		t = t2.x;
		return true;
	}
	static E				ray_closest(const ray<E,N> &r) {
		auto	q	= one / r.d;
		auto	sd	= sign(r.d);
		auto	vt0	= select(r.d == zero, zero, (-sd - (D)r.p) * q);
		auto	vt1	= select(r.d == zero, one,  (sd  - (D)r.p) * q);
		auto	t0	= reduce_max(vt0);
		auto	t1	= reduce_min(vt1);
		if (t0 <= t1)
			return t0 < 0 ? t1 : t0;

		auto	dp	= perp(r.d);
		if (dot(r.p, dp) < zero)
			dp = -dp;
		return dot(support(dp) - r.p, r.p.v) / len2(r.p.v);
	}
	static P				uniform_interior(D t)		{ return P(t * two - one); }
	static P				closest(P p)				{ return P(select(abs(p.v) > one, sign1(p.v), p.v)); }
	static P				support(D v)				{ return P(sign1(v)); }
	static n_sphere<E,N>	inscribe()					{ return one; }
	static n_sphere<E,N>	circumscribe()				{ return sqrt<N>; }

	friend P				uniform_interior(const unit_aabb&, D t)		{ return uniform_interior(t); }
};

template<typename E> auto clip_test_helper(vec<E, 2> v) { return v.y + v.x; }
template<typename E> auto clip_test_helper(vec<E, 3> v) { return v.zxy + v.yzx; }

template<typename E, int N> bool unit_aabb<E, N>::clip_test(P a, P b) {
	auto		m		= a.v + b.v;
	auto		d		= b - a;
	auto		absd	= abs(d);
	return all(abs(m) <= absd + two)
		&& all(abs(cross(m, d)) <= clip_test_helper<E>(absd) * two);
}

template<typename E, int N> int unit_aabb<E, N>::clip_test(P a, P b, vec<E,2> &t) {
	if (any(min(a.v, b.v) > one) || any(max(a.v, b.v) < -one))			//entirely outside box
		return -1;

	auto	d	= a - b;
	auto	q	= one / d;
	auto	vt0	= select(d == zero, zero, -sign(d) * a.v * q + q);
	auto	vt1	= select(d == zero, one,   sign(d) * a.v * q + q);

	auto	t0	= max(reduce_max(vt0), zero);
	auto	t1	= min(reduce_min(vt1), one);

	if (t0 > t1)
		return -1;

	t = {t0, t1};
	return max_component_index(vt0);
}

template<typename E, int N> int unit_aabb<E, N>::clip_test(homo<E,N> a, homo<E,N> b, vec<E,2> &t) {
	if (any(a.real > a.scale & b.real >  b.scale) ||  any(a.real < -a.scale & b.real < -b.scale))
		return -1;

	auto	d	= a.v - b.v;
	auto	va	= (a.real + a.scale) / (shrink<N>(d) + d[N]);
	auto	vb	= (a.real - a.scale) / (shrink<N>(d) - d[N]);
	auto	vt0	= select(a.real < -a.scale, va, select(a.real > a.scale, vb, zero));
	auto	vt1	= select(b.real < -b.scale, va, select(b.real > b.scale, vb, one));

	auto	t0	= reduce_max(vt0);
	auto	t1	= reduce_min(vt1);

	if (t0 > t1)
		return -1;

	t = {t0, t1};
	return max_component_index(vt0);
}

template<typename E, int N> bool unit_aabb<E, N>::clip(P a, P b) {
	if (any(min(a.v, b.v) > one) || any(max(a.v, b.v) < -one))
		return false;		//entirely outside box

	auto	d	= b - a;
	auto	q	= one / d;
	auto	vt0	= select(d == zero, zero, -abs(q) - a.v * q);
	auto	vt1	= select(d == zero, one,   abs(q) - a.v * q);

	auto	t0	= max(reduce_max(vt0), zero);
	auto	t1	= min(reduce_min(vt1), one);

	if (t0 > t1)
		return false;

	b.v	= a.v + d * t1;
	a.v	= a.v + d * t0;
	return true;
}

template<typename E, int N> bool unit_aabb<E, N>::clip(homo<E,N> &a, homo<E,N> &b) {
	if (a.scale < 0)
		a.v = -a.v;
	if (b.scale < 0)
		b.v = -b.v;
	if (any(a.real > a.scale & b.real > b.scale) || any(a.real < -a.scale & b.real < -b.scale))
		return false;		//entirely outside box

	auto	d	= b.v - a.v;
	auto	va	= -(a.vec + a.scale) / (shrink<N>(d) + d[N]);
	auto	vb	= -(a.vec - a.scale) / (shrink<N>(d) - d[N]);

	auto	vt0	= select(a.real < -a.scale, va, select(a.real > a.scale, vb, zero));
	auto	vt1	= select(b.real < -b.scale, va, select(b.real > b.scale, vb, one));

	auto	t0	= max(reduce_max(vt0), zero);
	auto	t1	= min(reduce_min(vt1), one);

	if (t0 > t1)
		return false;

	b.v	= a.v + d * t1;
	a.v	= a.v + d * t0;
	return true;
}

template<typename E, int N>	auto	ray<E, N>::get_box()		const	{ return aabb<E, N>::with_length(p, d); }
template<typename E, int N>	auto	n_sphere<E, N>::get_box()	const	{ return aabb<E, N>::with_centre(centre(), radius()); }

//-----------------------------------------------------------------------------
// n_frustum
//-----------------------------------------------------------------------------

template<typename E, int N> class n_frustum : public mat<E, N+1, N+1> {
protected:
	typedef pos<E,N>		P;
	typedef vec<E,N>		D;
	typedef mat<E,N+1, N+1>	M;
public:
	force_inline n_frustum()			{}
	force_inline explicit n_frustum(const M &m) : M(m)	{}

	force_inline const M&	matrix()				const	{ return *this; }
	force_inline auto		inv_matrix()			const	{ return inverse(matrix());	}

	force_inline bool		contains(P p)			const	{ auto t = p / matrix(); return all(abs(t.xyz) <= t.w); }
	force_inline homo<E,N>	corner(CORNER i)		const	{ return matrix() * P(select((int)i, D(one), D(-one))); }
	force_inline homo<E,N>	origin()				const	{ return M::column(N); }// was -z
	force_inline auto		plane(PLANE i)			const	{ return matrix() * normalised<n_plane<E, N>>(i); }
	force_inline auto		inv_plane(PLANE i)		const	{ return n_plane<E, N>(i) / matrix(); }

	aabb<E,N>				get_box()							const;
	bool					clip(homo<E,N> &p1, homo<E,N> &p2)	const;
	bool					ray_check(const ray<E,N> &r)		const;

	friend auto get_planes(const n_frustum& f) {
		return transformc(planes<N>(), [mat = transpose(cofactors(f.matrix()))](PLANE i) { return n_plane<E, N>(i) / mat; });
	}
	friend auto get_inv_planes(const n_frustum& f) {
		return transformc(planes<N>(), [mat = f.matrix()](PLANE i) { return n_plane<E, N>(i) / mat; });
	}
};

template<typename E, int N> aabb<E,N> n_frustum<E,N>::get_box() const {
	aabb<E,N>		c(empty);
	for (auto i : corners<N>())
		c |= corner(i);
	return c;
}

template<typename E, int N> bool n_frustum<E,N>::clip(homo<E,N> &p1, homo<E,N> &p2) const {
	float2		t;
	auto		im	= inv_matrix();
	int			ret	= unit_aabb<E,N>::clip_test(im * p1, im * p2, t);
	if (ret < 0)
		return false;

	auto	d	= p2.v - p1.v;
	p2	= p1.v + d * t.y;
	p1	= p1.v + d * t.x;
	return true;
}

template<typename E, int N> bool n_frustum<E,N>::ray_check(const ray<E,N> &r) const {
	float2		t;
	auto		im = inv_matrix();
	return unit_aabb<E,N>::clip_test(im * r.pt0(), im * r.ptr1(), t) >= 0;
}

template<typename E, int N> force_inline auto operator*(const mat<E, N+1, N+1> &m, const interval<pos<E,N>> &b)	{
	return n_frustum<E,N>(m * b.matrix());
}

//-----------------------------------------------------------------------------
// n_parallelotope
//-----------------------------------------------------------------------------

template<typename E, int N> class n_parallelotope : public mat<E, N, N+1> {
protected:
	typedef pos<E,N>		P;
	typedef vec<E,N>		D;
	typedef mat<E, N, N+1>	M;
public:
	force_inline n_parallelotope()	{}
	force_inline n_parallelotope(const aabb<E, N> &b)	: M(b.matrix())	{}
//	force_inline n_parallelotope(P a, P b, P c) 		: M((b - a), (c - a), a) {}						//2d
	force_inline n_parallelotope(P a, D b, D c) 		: M(b * half, c * half, a + (b + c) * half) {}	//2d
	force_inline n_parallelotope(P a, D b, D c, D d) 	: M(b * half, c * half, d * half, a + (b + c + d) * half)	{} //3d

	force_inline explicit n_parallelotope(const M &m) : M(m)	{}

	force_inline const M& 	matrix()					const	{ return *this; }
	force_inline auto		inv_matrix()				const	{ return inverse(matrix()); }
	force_inline P			centre()					const	{ return get_trans(*this); }
	force_inline D			half_extent()				const	{ return get_scale(matrix()); }
	force_inline D			extent()					const	{ return half_extent() * 2; }
	force_inline n_parallelotope&	right()						{ if (M::det() < 0) M::column(N-1) = -M::column(N-1); return *this; }

	force_inline D			parametric(P p)				const	{ return (p / matrix()).v; }
	force_inline P			from_parametric(D p)		const	{ return matrix() * P(p); }
	force_inline bool		contains(const n_parallelotope &b)const	{ M m = b.matrix() / inv_matrix(); return all(sum_cols(abs(matrix())) <= one); }
	force_inline bool		contains(P p)				const	{ return all(abs(parametric(p)) <= one);	}
	force_inline P			corner(CORNER i)			const	{ return matrix() * P(select((int)i, D(one), D(-one))); }
	force_inline auto		plane(PLANE i)				const	{ return matrix() * normalised<n_plane<E, N>>(i); }

	bool					ray_check(const ray<E,N> &r)						const	{ return unit_aabb<E, N>::ray_check(get(inv_matrix()) * r); }
	bool					ray_check(const ray<E,N> &r, float &t, D *normal)	const;
	E						ray_closest(const ray<E,N> &r)						const	{ return unit_aabb<E, N>().ray_closest(get(inv_matrix()) * r); }
	bool					clip(P &p1, P &p2) 									const;

//	force_inline n_sphere<E,N>	inscribe()				const	{ return circle(centre(), reduce_min(half_extent())); }
	force_inline n_sphere<E,N>	circumscribe()			const	{ return {centre(), len(sum_cols(get_rot(matrix())))}; }
	force_inline aabb<E, N>		get_box()				const	{ return aabb<E, N>::with_centre(centre(), sum_cols(abs(get_rot(matrix())))); }

	P						closest(P p)				const 	{ return matrix() * P(clamp(inv_matrix() * p, P(-one), P(one))); }
	P						support(D v)				const	{ return matrix() * unit_aabb<E,N>::support(inv_matrix() * v); }
	friend P				uniform_interior(const n_parallelotope &s, D t)	{ return s.from_parametric(t * two - one); }

	void					operator*=(const M &m)							{ M::operator=(m * matrix()); }
	friend n_parallelotope	operator*(const M &m, const n_parallelotope &b)	{ return n_parallelotope(m * b.matrix()); }
	friend auto 			operator*(const mat<E, N+1,N+1> &m, const n_parallelotope &b)	{ return n_frustum<E, N>(m * b.matrix()); }
};

template<typename E> force_inline	E	area(const n_parallelotope<E,2> &a)		{ return a.det(); }
template<typename E> force_inline	E	volume(const n_parallelotope<E,3> &a)	{ return a.det(); }

template<typename E, int N> bool n_parallelotope<E,N>::ray_check(const ray<E,N> &r, float &t, D *normal) const {
	auto	r1 = get(inv_matrix()) * r;
	float2	t2;
	int		a = unit_aabb<E, N>::clip_test(r1.pt0(), r1.pt1(), t2);
	if (a < 0)
		return false;
	if (normal)
		*normal = sign1(-r1.d[a]) * (*this)[a];
	t = t2.x;
	return true;
}

template<typename E, int N> bool n_parallelotope<E,N>::clip(P &p1, P &p2) const {
	float2	t;
	M		im	= get(inv_matrix());
	int		ret	= unit_aabb<E, N>::clip_test(im * p1, im * p2, t);
	if (ret < 0)
		return false;

	D	d = p2 - p1;
	p2	= p1 + d * t.y;
	p1	= p1 + d * t.x;
	return true;
}

template<typename E, int N> bool overlap(const n_parallelotope<E,N> &a, const n_parallelotope<E,N> &b) {
	auto	m1	= a.inv_matrix() * b.matrix();
	auto	a1	= sum_cols(abs(get_rot(m1)));
	if (any(abs(get_trans(m1).v) - a1 > one))
		return false;

	auto	m2	= inverse(m1);//b.inv_matrix() * matrix();
	auto	a2	= sum_cols(abs(get_rot(m2)));
	if (any(abs(get_trans(m2).v) - a2 > one))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// obb
//-----------------------------------------------------------------------------

template<typename E, int N> class obb : public n_parallelotope<E, N> {
	typedef	n_parallelotope<E, N> B;
	typedef mat<E, N, N+1>	M;
//	using typename B::M;
	using typename B::P;
public:
	force_inline obb() {}
	force_inline explicit obb(const _one&)			: B(identity)	{}
	force_inline explicit obb(const M &b)			: B(b)			{}
	force_inline explicit obb(const aabb<E, N> &b)	: B(b.matrix())	{}
	force_inline explicit obb(const B &b)			: B(b)			{}

	P							closest(P p)	const	{ return B::matrix() * unit_aabb<E,N>::closest(B::inv_matrix() * p); }
	force_inline n_sphere<E,N>	inscribe()		const	{ return {B::centre(), reduce_min(B::half_extent())}; }
	force_inline obb&			right()					{ B::right(); return *this; }

	friend B 	fullmul(const M &m, const obb &b)			{ return B(m * b.matrix()); }
	friend obb 	operator*(const M &m, const obb &b)			{ return obb(m * b.matrix()); }
};

template<typename E> force_inline	E	area(const obb<E,2> &a)		{ return a.det(); }
template<typename E> force_inline	E	volume(const obb<E,3> &a)	{ return a.det(); }

template<typename E, int N> auto operator*(const mat<E, N, N> &m, const interval<pos<E, N>> &b) {
	return obb<E, N>(m * translate(b.centre()) * scale(b.half_extent()));
}
template<typename E, int N> auto fullmul(const mat<E, N, N> &m, const interval<pos<E, N>> &b) {
	return n_parallelotope<E, N>(m * translate(b.centre()) * scale(b.half_extent()));
}
template<typename E, int N> auto operator*(const mat<E, N, N+1> &m, const interval<pos<E, N>> &b) {
	return obb<E, N>(m * translate(b.centre()) * scale(b.half_extent()));
}
template<typename E, int N> auto fullmul(const mat<E, N, N+1> &m, const interval<pos<E, N>> &b) {
	return n_parallelotope<E, N>(m * translate(b.centre()) * scale(b.half_extent()));
}
template<typename E, int N> auto operator*(const mat<E, N+1,N+1> &m, const obb<E, N> &b) {
	return n_frustum<E, N>(m * b.matrix());
}

//-----------------------------------------------------------------------------
// generic functions
//-----------------------------------------------------------------------------

//dist between shape and point
template<typename S, typename E, int N>		auto	dist(const S &s, const pos<E, N> &p)->decltype(len(s.closest(p)-p)) {
	return len(s.closest(p) - p);
}

//dist between shape and ray
template<typename S, typename E, int N>		auto	dist(const S &s, const ray<E, N> &r) {
	auto	dp	= normalise(perp(r.d));
	if (dot(dp, anypoint(s) - r.p) > 0)
		dp = -dp;
	return max(dot(-dp, s.support(dp)), zero);
}

//dist between shape and ray
template<typename S, typename E, int N>		auto	dist(const S &s, const ray<E, N> &r, decltype(r.p) *pos) {
	auto	t	= s.ray_closest(r);
	auto	p1	= r.from_parametric(t);
	if (pos)
		*pos = p1;
	return dist(s, p1);
}

template<typename T> auto	circumscribe(const T &t)	{ return t.circumscribe(); }
template<typename T> auto	inscribe(const T &t)		{ return t.inscribe(); }

//-----------------------------------------------------------------------------
//
//						2D
//
//-----------------------------------------------------------------------------

typedef ray<float, 2>			ray2;
typedef n_sphere<float, 2>		circle;
typedef unit_n_sphere<float, 2>	unit_circle_t;
typedef normalised<line<float, 2>>	line2;
typedef aabb<float, 2>			rectangle;
typedef unit_aabb<float, 2>		unit_rect_t;
typedef obb<float, 2>			obb2;

extern const unit_circle_t		unit_circle;
extern const unit_rect_t		unit_rect;

template<typename T> float		area(const T &t)	{ return t.area(); }

template<typename T, typename = decltype(declval<T>().plane(PLANE_MINUS_X))> auto get_planes(const T &t) {
	return transformc(planes<3>(), [t](PLANE i) { return t.plane(i); });
}

template<typename S> float	dist(const S &s, param(ray2) r, position2 *pos = nullptr) {
	auto	t	= s.ray_closest(r);
	auto	p1	= r.from_parametric(t);
	if (pos)
		*pos = p1;
	return dist(s, p1);
}

inline bool colinear(param(float2) p1, param(float2) p2) {
	return cross(p1, p2) == zero;
}
inline bool colinear(param(position2) p1, param(position2) p2, param(position2) p3) {
	return colinear(p1 - p3, p2 - p3);
}
inline bool colinear(param(float2) p1, param(float2) p2, float epsilon) {
	return square(cross(p1, p2)) <= len2(p1) * len2(p2) * square(epsilon);
}
inline bool colinear(param(position2) p1, param(position2) p2, param(position2) p3, float epsilon) {
	return colinear(p1 - p3, p2 - p3, epsilon);
}

float2		solve_line(param(float2) pt0, param(float2) pt1);	// solves y = mx + b for line to intersect two points
position2	intersect_lines(param(position2) pos0, param(float2) dir0, param(position2) pos1, param(float2) dir1);

template<typename T> auto segment_distance2(T a, T b, T p) {
	auto	d	= b - a;
	auto	dl	= len2(d);
	auto	x	= dl == zero ? a : a + d * clamp(dot(p - a, d) / dl, zero, one);
	return len2(x - p);
}

template<typename T> auto segment_distance(T a, T b, T p) {
	return sqrt(segment_distance2(a, b, p));
}

// compute signed area of triangle (a,b,c)
//inline auto signed_area(param(position2) a, param(position2) b, param(position2) c) {
template<typename T> auto signed_area(T a, T b, T c) {
	return cross(a - c, b - c);
}

// compute angle of oriented edge (a,b) relative to point p (for generalised winding number)
inline auto edge_angle(param(position2) a, param(position2) b, param(position2) p) {
	vector2 da = a - p;
	vector2 db = b - p;
	return atan2(cross(da, db), dot(da, db));
}

//-----------------------------------------------------------------------------
// rectangle	(aabb<2>)
//-----------------------------------------------------------------------------

template<typename E> pos<E,2> uniform_perimeter(const unit_aabb<E, 2>&, E t)	{
	E		s = floor(t * 4);
	int		i = int(s);
	E		u = (i & 2) - one;
	t = (t - s - half) * 2;
	return i & 1 ? P(u, t) : P(t, u);
}

template<typename E> pos<E,2> uniform_perimeter(const aabb<E, 2> &r, E t) {
	auto	e = r.extent();
	if (t < half) {
		t	*= reduce_add(e) * two;
		return r.a + select(t < e.x, vec<E,2>{t, 0}, vec<E,2>{0, t});;
	} else {
		t	= (t - half) * reduce_add(e) * two;
		return r.b - select(t < e.x, vec<E,2>{t, 0}, vec<E,2>{0, t});;
	}
}

//-----------------------------------------------------------------------------
// ray2
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// line2	- 2d analog of plane
//-----------------------------------------------------------------------------

force_inline float2x3	to_x_axis(param(line2) p)		{ auto v = as_vec(p); return float2x3(-v.yx, float2{v.x, -v.y}, float2{zero, v.z}); }
force_inline float2x3	from_x_axis(param(line2) p)		{ auto v = as_vec(p); return float2x3(float2{-v.y, v.x}, -v.xy, -v.xy * v.z); }
bool		intersection(const line2 &a, const line2 &b, float2 &intersection, float tol = ISO_TOLERANCE);
float2x3	reflect(param(line2) p);

//-----------------------------------------------------------------------------
// conic
//-----------------------------------------------------------------------------

// dx.x.x + dy.y.y + dz.w.w + 2 ox.x.y + 2 oy.y.w + 2 oz.x.w = 0

class conic : public symmetrical3 {
public:
	using iso::diagonal<float, 3>::d;
	enum TYPE {HYPERBOLA = NEG, PARABOLA = ZERO, ELLIPSE = POS, TRIVIAL = 2};

	struct info {
		TYPE	type;
		SIGN	orientation;
		bool	empty;
		bool	degenerate;
		info(TYPE _type, SIGN _orientation, bool _none, bool _degenerate) : type(_type), orientation(_orientation), empty(_none), degenerate(_degenerate) {}
	};

	static void		two_linepairs(conic &c1, conic &c2, const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4);
	friend float	vol_derivative(const conic &c1, const conic &c2);
	friend float	vol_minimum(const conic &c1, const conic &c2);

	static conic through(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4, const position2 &p5);
	static conic circle_through(const position2 &p1, const position2 &p2, const position2 &p3);
	static conic ellipse_through(const position2 &p1, const position2 &p2, const position2 &p3);
	static conic ellipse_through(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4);
	static conic line_pair(const position2 &p1, const position2 &p2, const position2 &p3, const position2 &p4);
	static conic line_pair(const line2 &p1, const line2 &p2);
	static conic line(const line2 &p1);
	static conic from_matrix(const float3x3 &m);

	static conic unit_circle() { return  conic(float3{one, one, -one}, zero); }

	conic() {}
	conic(const _triangular<float, 3> &m) : symmetrical3(m)	{}
	conic(float3 d, float3 o)	: symmetrical3(d, o)	{}
	explicit conic(const circle &c)		: symmetrical3(c.matrix() * conic::unit_circle()) {}

	float		det2x2()						const	{ return swizzle<0,1>(*(symmetrical3*)this).det(); }

	info		analyse()						const;
	float		evaluate(param(position2) p)	const	{ return dot(p.v * p.v, d.xy) + two * (dot(p.v, o.zy) + o.x * p.v.x * p.v.y) + d.z; }
	position2	centre()						const	{ return position2(cofactors(*this).column<2>()); }
	position2	centre(conic &x)				const	{ position2 c = centre(); x = conic(concat(d.xy, evaluate(c)), float3{o.x,zero,zero}); return c; }
	float		area()							const	{ auto t = det2x2(); return pi * det() / t * rsqrt(t); }
	auto		centre_form(position2 &c)		const	{ c = centre(); auto q = translate(-c) * *this; return swizzle<0,1>(q) / -q.d.z; }
	bool		is_circle()						const	{ return d.x == d.y && o.x == zero; }
	bool		is_ellipse()					const	{ return det2x2() > zero; }
	void		flip()									{ d = -d; o = -o; }

	bool		ray_check(param(ray2) r, float &t, vector2 *normal);
	rectangle	get_box()						const;
	float2x3	get_tangents()					const;

	position2	support(param(float2) v)		const;

	interval<float>	operator&(const axis_s<0>&)	const	{ float2 t = float2{-o.z, sqrt(square(o.z) - d.x * d.z)} / d.x; return interval<float>(t.x - t.y, t.x + t.y); }
	interval<float>	operator&(const axis_s<1>&)	const	{ float2 t = float2{-o.y, sqrt(square(o.y) - d.y * d.z)} / d.y; return interval<float>(t.x - t.y, t.x + t.y); }
	interval<float>	operator&(param(line2) p)	const	{ return (*this / from_x_axis(p)) & x_axis; }

	friend conic operator-(const conic &a)					{ return a._neg(); }
	friend conic operator+(const conic &a, const conic &b)	{ return a._add(b); }
	friend conic operator-(const conic &a, const conic &b)	{ return a._sub(b); }
	template<typename T> friend IF_SCALAR(T, conic) operator*(param(conic) a, const T &b)	{ return a._smul(b); }
	template<typename T> friend IF_SCALAR(T, conic) operator*(const T &a, param(conic) b)	{ return b._smul(a); }

//	template<typename M> conic operator/(const M &m) const	{ return mul_mulT(transpose(m), *this); }
	template<typename M> friend conic operator*(const inverse_s<M> &m, const conic &c)	{ return mul_mulT(transpose(m.m), (const symmetrical3&)c); }
	template<typename M> friend IF_MAT(M, conic) operator*(const M &m, const conic &c)	{ return mul_mulT(cofactors(m), (const symmetrical3&)c); }
};

//-----------------------------------------------------------------------------
// circle
//-----------------------------------------------------------------------------

template<typename E> pos<E,2>	unit_circle_uniform_perimeter(E t)			{ return pos<E,2>(sincos(t * (pi * two))); }
template<typename E> pos<E,2>	unit_circle_uniform_interior(vec<E,2> t)	{ return pos<E,2>(unit_circle_uniform_perimeter(t.x) * sqrt(t.y)); }

template<typename E> pos<E,2>	uniform_perimeter(const n_sphere<E, 2> &c, float t)			{ return c.centre() + unit_circle_uniform_perimeter(t) * c.radius(); }
template<typename E> pos<E,2>	uniform_interior(const n_sphere<E, 2> &c, vec<float,2> t)	{ return c.centre() + unit_circle_uniform_interior<E>(t) * c.radius(); }

//-----------------------------------------------------------------------------
// ellipse
//-----------------------------------------------------------------------------

inline float ellipse_perimeter_approx(float major, float ratio) {
	float	h = square((1 - ratio) / (1 + ratio));
	//pade 2/1
	return pi * 4 * major * (1 + ratio) * (64 - 3 * h * h) / (64 - 16 * h);
}

class ellipse {
public:
	float4	v;// centre, axis
	float	ratio;

	ellipse() {}
	ellipse(position2 c, float2 a, float r) : v(concat(c.v, a)), ratio(r) {}
	ellipse(const circle &c) : v(concat(c.centre().v, c.radius(), zero)), ratio(one)	{}
	ellipse(const conic &c);
	explicit ellipse(const float2x3 &m);

	static ellipse			through(param(position2) p1, param(position2) p2);
	static ellipse			through(param(position2) p1, param(position2) p2, param(position2) p3);
	static ellipse			through(param(position2) p1, param(position2) p2, param(position2) p3, param(position2) p4);
	static ellipse			through(param(position2) p1, param(position2) p2, param(position2) p3, param(position2) p4, param(position2) p5);

	force_inline position2	centre()					const	{ return position2(v.xy); }
	force_inline float2		major()						const	{ return v.zw; }
	force_inline float2		minor()						const	{ return perp(major()) * ratio; }
	force_inline float		area()						const	{ return len2(major()) * ratio * pi;	}
	force_inline circle		circumscribe()				const	{ return circle::with_r2(centre(), len2(major()) * max(ratio, one)); }
	force_inline circle		inscribe()					const	{ return circle::with_r2(centre(), len2(major()) * min(ratio, one)); }
	float					perimeter()					const	{ return ellipse_perimeter_approx(len(major()), ratio); }

	bool					contains(param(position2) pos)	const	{ return len2(float2(pos / matrix())) < one; }
	bool					ray_check(param(ray2) r, float &t, vector2 *normal)	const;
	float					ray_closest(param(ray2) r)	const;

	force_inline float2x3	matrix()					const	{ return float2x3(major(), minor(), centre()); }
	force_inline inverse_s<float2x3>	inv_matrix()	const	{ return inverse(matrix()); }
	force_inline rectangle	get_box()					const	{ return rectangle::with_centre(centre(), sqrt(square(major()) + square(minor()))); }

	ellipse&				operator*=(param(float2x3) m)	{ return *this = ellipse(m * matrix()); }
	operator conic()	const { return matrix() * conic::unit_circle(); }

	//friend ellipse operator+(param(ellipse) a, param(ellipse) b);	//minkowski sum
	//friend ellipse operator-(param(ellipse) a, param(ellipse) b);	//minkowski difference
	friend ellipse operator*(param(float2x3) m, param(ellipse) b)	{ return ellipse(m * b.matrix()); }

	position2			closest(param(position2) p)		const;
	position2			support(param(float2) v)		const;
	friend position2	uniform_interior(const ellipse &e, param(float2) t)	{ return e.matrix() * unit_circle_uniform_interior<float>(t); }
	friend position2	uniform_perimeter(const ellipse &e, float x);
};

inline ellipse fullmul(param(float2x3) m, param(circle) b)	{ return ellipse(m * b.matrix()); }

class shear_ellipse : public float2x3 {
public:
	explicit shear_ellipse(const float2x3 &m) : float2x3(m) {}
	shear_ellipse(const conic &c) : float2x3(
		float2{sqrt(-c.det() / (c.d.x * c.det2x2())), zero},
		float2{-c.o.x, c.d.x} * sqrt(-c.det() / c.d.x) / c.det2x2(),
		c.centre()
	) {}

	force_inline position2	centre()				const	{ return position2(z); }
	force_inline float		area()					const	{ return xx * yy * pi;	}
	bool					contains(param(position2) pos)	const	{ return len2(float2(pos / matrix())) < one; }
	force_inline float2x3	matrix()				const	{ return *this; }
	force_inline auto		inv_matrix()			const	{ return inverse(matrix()); }
};

//-----------------------------------------------------------------------------
// parallelogram
//-----------------------------------------------------------------------------

typedef n_parallelotope<float, 2> parallelogram;

template<typename E> pos<E,2> uniform_concentric(const n_parallelotope<E, 2> &p, float2 t) {
	float2 u = t * two - one;
	return {
		  dot(u, u) == zero		? float2(zero)
		: abs(u.x) > abs(u.y)	? sincos((pi / 4) * u.y / u.x) * u.x
		: sincos(pi / 2 - (pi / 4) * u.x / u.y) * u.y
	};
}

template<typename E> pos<E,2> uniform_perimeter(const n_parallelotope<E, 2> &p, float t) {
	auto	e = get_scale(p.matrix());
	auto	s = reduce_add(e) * frac(t * two);
	E	u = int(t > half);// * two - one;

	return p.matrix() * ((s > e.x ? pos<E,2>(u, (s - e.x) / e.y) : pos<E,2>(s / e.x, u)) * 2 - 1);
}

template<typename E> auto perimeter_optimised(const n_parallelotope<E, 2> &shape) {
	auto	e	= get_scale(shape.matrix());
	float	x	= e.x / reduce_add(e);
	return [&shape, x](float t) {
		auto	s = frac(t * two);
		E		u = int(t > half);
		return shape.matrix() * ((s > x ? pos<E,2>(u, (s - x) / (one - x)) : pos<E,2>(s / x, u)) * 2 - 1);
	};
}

template<typename E> auto perimeter_optimised(const obb<E, 2> &shape) {
	return perimeter_optimised((const n_parallelotope<E, 2>&)shape);
}

//-----------------------------------------------------------------------------
// triangle
//-----------------------------------------------------------------------------

class triangle : float2x3 {
public:
	triangle()	{}
	triangle(param(position2) a, param(position2) b, param(position2) c) : float2x3(b - a, c - a, a)	{}
	explicit triangle(param(float2x3) m) : float2x3(m)	{}

	float2		parametric(param(position2) p)		const	{ return p / matrix();			}
	float3		barycentric(param(position2) p)		const	{ return iso::barycentric(parametric(p)); }
	position2	from_parametric(param(float2) p)	const	{ return position2(x * p.x + y * p.y + z);		}
	position2	corner(int i)						const	{ return position2(select(i > 0, z + (*this)[i - 1], z)); }
	line2		edge(int i)							const	{ return line2(corner(dec_mod(i, 3)), corner(inc_mod(i, 3))); }
	bool		contains(param(position2) p)		const	{ return all(barycentric(p) >= zero);			}
	position2	centre()							const	{ return position2(z + (x + y) / 3);			}
	float		area()								const	{ return cross(x, y) * half;					}
	float		perimeter()							const	{ return len(x) + len(y) + len(y - x);			}
	const float2x3&	matrix()						const	{ return *this; }
	auto		inv_matrix()						const	{ return inverse(matrix()); }
	array<position2, 3>	corners()			const	{ return { position2(z), position2(z + x), position2(z + y) }; }

	void		flip()										{ swap(x, y); }

	float		ray_closest(param(ray2) r)							const;
	bool		ray_check(param(ray2) r, float &t, vector2 *normal)	const;
	bool		ray_check(param(ray2) r)							const;

	circle		inscribe()		const {
		float	len0	= len(x), len1 = len(y), len2 = len(y - x), perimeter = len0 + len1 + len2;
		return circle(from_parametric(float2{len2, len1} / perimeter), cross(x, y) / perimeter);
	}
	circle		circumscribe()	const {
		vector2	r	= (x * len2(y) - y * len2(x)) / (cross(x, y) * two);
		return circle::with_r2(position2(z) + perp(r), len2(r));
	}
	rectangle	get_box()							const	{ return rectangle(position2(z) + min(min(x, y), zero), position2(z) + max(max(x, y), zero)); }
	position2	support(param(float2) v)			const;
	position2	closest(param(position2) p)			const;
	position2	uniform_subdivide(uint32 u)			const;

	void			operator*=(param(float2x3) m)	{ float2x3::operator=(m * matrix()); }

	friend triangle	operator*(param(float2x3) m, param(triangle) b)	{ return triangle(m * b.matrix()); }
	//	position2	uniform_interior(param(float2) t)	const	{ return uniform_subdivide(uint32(float(t.x) * (1ull << 22))); }
	friend position2	uniform_interior(const triangle &s, param(float2) t)	{
		return s.from_parametric(t.x + t.y > one ? one - t : t);
	//	return s.from_parametric(sqrt(t.x) * float2{one - t.y, t.y});
	}

	friend position2	uniform_perimeter(const triangle &s, float x);
};
class _unit_tri : public triangle {
	static const array<position2, 3>	_corners;
public:
	_unit_tri() : triangle(identity) {}
	force_inline static float2		parametric(param(position2) p)		{ return p.v; }
	force_inline static float3		barycentric(param(position2) p)		{ return iso::barycentric(p.v); }
	force_inline static position2	from_parametric(param(float2) p)	{ return position2(p); }
	force_inline static bool		contains(param(position2) p)		{ return all(barycentric(p) >= zero); }
	force_inline static float		area()								{ return half; }
	force_inline static float		perimeter()							{ return two + sqrt2; }
	force_inline static _identity	matrix()							{ return _identity(); }
	force_inline static _identity	inv_matrix()						{ return _identity(); }
	force_inline static const array<position2, 3>&	corners()	{ return _corners; }
	force_inline static const position2&	corner(uint8 i)				{ return _corners[i]; }

	static bool			ray_check(param(ray2) r, float &t, vector2 *normal);

	static position2	uniform_interior(param(float2) t)	{ return position2(sqrt(t.x) * float2{one - t.y, t.y}); }
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
	quadrilateral(param(position2) a, param(position2) b, param(position2) c, param(position2) d) : p01(concat(a.v, b.v)), p23(concat(c.v, d.v))	{}
	quadrilateral(param(rectangle) r) : p01(as_vec(r).xyzx), p23(as_vec(r).xwzw)	{}
	quadrilateral(param(parallelogram) p) {
		float4		x = concat(p.x, -p.x);
		p01 = swizzle<0,1,0,1>(p.z - p.y) + x;
		p23 = swizzle<0,1,0,1>(p.z + p.y) + x;
	}
	explicit quadrilateral(param(float3x3) m) {
		p01	= concat(position2(m.z - m.x - m.y).v, position2(m.z + m.x - m.y).v);
		p23	= concat(position2(m.z - m.x + m.y).v, position2(m.z + m.x + m.y).v);
	}

	position2	pt0()								const	{ return position2(p01.xy);	}
	position2	pt1()								const	{ return position2(p01.zw);	}
	position2	pt2()								const	{ return position2(p23.xy);	}
	position2	pt3()								const	{ return position2(p23.zw);	}
	float		area()								const	{ return diff(p01.xy * p01.wz + p01.zw * p23.yx + p23.xy * p23.wz + p23.zw * p01.yx) * half; }
	float		perimeter()							const	{ return len(p01.xy - p01.zw) + len(p01.zw - p23.xy) + len(p23.xy - p23.zw) + len(p23.zw - p01.xy);	}
	float2		extent()							const	{ float4 a = min(p01, p23), b = max(p01, p23); return max(b.xy, b.zw) - min(a.xy, a.zw); }
	position2	corner(CORNER i)					const	{ return get(((const packed<position2>*)this)[i]);	}
	position2	operator[](int i)					const	{ return get(((const packed<position2>*)this)[i]);	}

	bool		contains(param(position2) pos)		const	{ return unit_rect.contains(inv_matrix() * pos); }
	position2	centre()							const;
	float4		barycentric(param(float2) p)		const;
	float3x3	inv_matrix()						const;
	float3x3	matrix()							const;
	rectangle	get_box()							const	{ float4 a = min(p01, p23), b = max(p01, p23); return rectangle(position2(min(a.xy, a.zw)), position2(max(b.xy, b.zw))); }
	array<position2, 4>	corners()			const	{ return { position2(p01.xy), position2(p01.zw), position2(p23.xy), position2(p23.zw) }; }
	array<position2, 4>	corners_cw()		const	{ return { position2(p01.xy), position2(p01.zw), position2(p23.zw), position2(p23.xy) }; }

	position2	closest(param(position2) p)			const;
	position2	support(param(float2) v)			const;
	bool		ray_check(param(ray2) r) 			const;
	bool		ray_check(param(ray2) r, float &t, vector2 *normal) const;
	float		ray_closest(param(ray2) d)			const;

	force_inline void operator*=(param(float2x3) m)								{ *this = m * *this; }

	friend quadrilateral operator*(param(float2x3) m, param(quadrilateral) b)	{ return quadrilateral(m * b.pt0(), m * b.pt1(), m * b.pt2(), m * b.pt3()); }
	friend position2	uniform_interior(const quadrilateral &s, param(float2) t);
	friend position2	uniform_perimeter(const quadrilateral &s, float x);
};

//force_inline quadrilateral operator*(param(float3x3) m, param(obb2) b)			{ return quadrilateral(m * float3x3(b.matrix())); }
//force_inline quadrilateral operator*(param(float3x3) m, param(rectangle) b)		{ return quadrilateral(m * float3x3(b.matrix())); }


//-----------------------------------------------------------------------------
// wedge2
//	infinite 2d wedge defined by a point and two normals
//-----------------------------------------------------------------------------

struct wedge2 : float2x3 {
	wedge2()	{}
	wedge2(param(position2) a, param(float2) x, param(float2) y) : float2x3(x, y, a)	{}

	bool	contains(param(position2) b) const {
		return dot(x, b.v - z) >= zero && dot(y, b.v - z) >= zero;
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
		float2		d0	= perp(r0.d);
		position2	v0	= s.support(d0);
		if (dot(d0, v0 - r0.p) < zero)
			return false;

		// single line
		if (all(r1.d == zero))
			return true;

		float2		d1	= perp(r1.d);
		position2	v1	= s.support(d1);
		if (dot(d1, v1 - r1.p) < zero)
			return false;

		// single wedge
		if (all(r0.p.v == r1.p.v))
			return cross(v0 - r0.p, v0 - v1) < zero || s.contains(r0.p);

		// double wedge
		float2		d2	= perp(r0.p - r1.p);
		position2	v2	= s.support(d2);
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

typedef ray<float, 3>			ray3;
typedef n_sphere<float, 3>		sphere;
typedef unit_n_sphere<float, 3>	unit_sphere_t;
typedef normalised<n_plane<float, 3>>	plane;
typedef aabb<float, 3>			cuboid;
typedef unit_aabb<float, 3>		unit_cube_t;
typedef obb<float, 3>			obb3;
typedef line<float, 3>			line3;


extern const unit_sphere_t		unit_sphere;
extern const unit_cube_t		unit_cube;

// return -ve where p is inside unit cube
inline float3 check_clip(param(float4) p) {
	float4	a = abs(p);
	return a.xyz - a.w;
}

bool	unit_cube_is_visible(const float4x4 &m);
bool	unit_cube_is_within(const float4x4 &m);
int		unit_cube_visibility_flags(const float4x4 &m, uint32 flags);

template<typename T, typename E> force_inline	bool	is_visible(const T &t, const mat<E, 4,4> &m)	{ return unit_cube_is_visible(m * t.matrix()); }
template<typename T, typename E> force_inline	bool	is_within(const T &t, const mat<E, 4,4> &m)		{ return unit_cube_is_within(m * t.matrix()); }
template<typename T, typename E> force_inline	uint32	visibility_flags(const T &t, const mat<E, 4,4> &m, uint32 flags)	{ return flags ? unit_cube_visibility_flags(m * t.matrix(), flags) : 0; }
template<typename T> force_inline				auto	volume(const T &t)	{ return t.volume(); }

inline bool colinear(param(float3) p1, param(float3) p2) {
	return all(cross(p1, p2) == zero);
}
inline bool colinear(param(position3) p1, param(position3) p2, param(position3) p3) {
	return colinear(p1 - p3, p2 - p3);
}
inline bool colinear(param(float3) p1, param(float3) p2, float epsilon) {
	return float(dot(p1, p2) * rsqrt(len2(p1) * len2(p2))) > 1 - epsilon;
}
inline bool colinear(param(position3) p1, param(position3) p2, param(position3) p3, float epsilon) {
	return colinear(p1 - p3, p2 - p3, epsilon);
}

// compute signed volume of tetrahedron (a,b,c,origin)
inline auto triple_product(param(float3) a, param(float3) b, param(float3) c) {
	return dot(a, cross(b, c));
}

// compute signed volume of tetrahedron (a,b,c,p)
inline auto signed_volume(param(position3) a, param(position3) b, param(position3) c, param(position3) p) {
	return triple_product(a - p, b - p, c - p);
}

// compute solid angle of oriented triangle (a,b,c) relative to origin (for generalised winding number)
inline auto triangle_solid_angle(param(float3) a, param(float3) b, param(float3) c) {
	float3	len	= sqrt(float3{dot(a, a), dot(b, b), dot(c,c)});
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
// cuboid	(aabb<3>)
//-----------------------------------------------------------------------------

position3	uniform_surface(param(cuboid) c, param(float2) t);

//-----------------------------------------------------------------------------
// plane
//-----------------------------------------------------------------------------

force_inline float3x4	to_xy_plane(param(plane) p)		{ auto v = as_vec(p); float3 t = normalise(perp(v.xyz)); float3x3 m(t, cross(v.xyz, t), v.xyz); return float3x4(transpose(m), extend_left<3>(v.w)); }
force_inline float3x4	from_xy_plane(param(plane) p)	{ auto v = as_vec(p); float3 t = normalise(perp(v.xyz)); return float3x4(t, cross(v.xyz, t), v.xyz, -v.xyz * v.w); }

float3x4		reflect(param(plane) p);
position3		intersect(param(plane) a, param(plane) b, param(plane) c);
bool			coincident(param(plane) a, param(plane) b);

//-----------------------------------------------------------------------------
// frustum
//-----------------------------------------------------------------------------

typedef n_frustum<float,3> frustum;

inline frustum	frustum_from_planes(plane p[6]) {
	float4	cw = (as_vec(p[0]) + as_vec(p[1])) * -half;
	return frustum(cofactors(float4x4(as_vec(p[1]) + cw, as_vec(p[3]) + cw, as_vec(p[5]) + cw, cw)));
}
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

	static quadric unit_sphere()		{ return quadric(float4{1,  1,  1, -1}); }
	static quadric unit_paraboloid()	{ return quadric(float4{1,  1,  0,  0}, float3{0, 0, -1}); }
	static quadric unit_saddle()		{ return quadric(float4{1, -1,  0,  0}, float3{0, 0, -1}); }
	static quadric unit_hyperboloid1()	{ return quadric(float4{1,  1, -1,  1}); }
	static quadric unit_hyperboloid2()	{ return quadric(float4{1,  1, -1, -1}); }

	//degenerates
	static quadric unit_cylinder()		{ return quadric(float4{1,  1,  0, -1}); }
	static quadric unit_cone()			{ return quadric(float4{1,  1, -1,  0}); }
	static quadric unit_hyperbola()		{ return quadric(float4{1, -1,  0,  1}); }
	static quadric unit_parabola()		{ return quadric(float4{1,  0,  0,  0}, zero, float3{0, 1, 0}); }
	static quadric xy_plane()			{ return quadric(zero, float3{0, 0, 1}); }
	static quadric z_axis()				{ return quadric(float4{1, 1, 0, 0}); }

	static quadric plane_pair(const iso::plane &p1, const iso::plane &p2);
	static quadric plane(const iso::plane &p1);

	static quadric sphere_through(const position3 &p1, const position3 &p2, const position3 &p3, const position3 &p4);
	static quadric ellipsoid_through(const position3 &p1, const position3 &p2, const position3 &p3, const position3 &p4, const position3 &p5);
	static quadric ellipsoid_through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6);
	static quadric ellipsoid_through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7);
	static quadric ellipsoid_through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7, param(position3) p8);
	static quadric through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7, param(position3) p8, param(position3) p9);


	quadric() {}
	quadric(const _triangular<float, 4> &m) : symmetrical4(m)	{}

	explicit quadric(const unit_sphere_t&)	: quadric(unit_sphere()) {}
	explicit quadric(const sphere &s)		: quadric(s.matrix() * unit_sphere()) {}
	explicit quadric(const class ellipsoid &e);

	force_inline quadric(float4 d, float3 o1 = zero, float3 o2 = zero) : symmetrical4(d, o1, o2)	{}

	info		analyse()						const;
	//	float		evaluate(param(float4) p)		const	{ return dot(p, float4x4(*this) * p); }
	//	float		evaluate(param(position3) p)	const	{ return evaluate(float4(p)); }
	float		evaluate(param(float4) p)		const	{ auto d3 = diagonal<1>().d; return dot(p * p, D::d) + two * (dot(concat(o.zy, d3.z), p.xyz * p.w) + dot(concat(d3.xy, o.x), p.xyz * p.yzx)); }
	float		evaluate(param(position3) p)	const	{ auto d3 = diagonal<1>().d; return dot(p.v * p.v, D::d.xyz) + two * (dot(concat(o.zy, d3.z), p.v) + dot(concat(d3.xy, o.x), p.v.xyz * p.v.yzx)) + D::d.w; }

	float4		centre4()						const	{ return cofactors(*this).column<3>(); }
	position3	centre()						const	{ return position3(centre4()); }
	position3	centre(quadric &q)				const	{ position3 c = centre(); q = quadric(concat(D::d.xyz, evaluate(c)), concat(diagonal<1>().d.xy, zero), float3{o.x,zero,zero}); return c; }
	float		volume()						const	{ auto t = swizzle<0,1,3>(*(symmetrical4*)this).det(); return pi * det() / t * rsqrt(t); }	// guess!
	auto		centre_form(position3 &c)		const	{ c = centre(); auto q = translate(-c) * *this; return swizzle<0,1,2>(q) / -q.d.w; }
	void		flip()									{ auto &d3 = diagonal<1>().d; D::d = -D::d; d3 = -d3; o = -o; }

	quadric&	add_param(param(float4) p)				{ auto &d3 = diagonal<1>().d; D::d += p.xyzw * p.xyzw; d3 += p.xyz * p.yzw; o += p.xyw * p.zwx; return *this; }
	quadric&	add_plane_error(param(iso::plane) p, float weight) { return add_param(as_vec(p) * weight); }

	bool		ray_check(param(ray3) r, float &t, vector3 *normal);
	cuboid		get_box()						const;
	float3x4	get_tangents()					const;
	conic		project_z()						const;
	position3	support(param(float3) v)		const;
	conic		project(param(iso::plane) p)	const	{ return (*this / from_xy_plane(p)).project_z(); }
	conic		operator&(const axis_s<0>&)		const	{ return swizzle<1,2,3>(*this); }
	conic		operator&(const axis_s<1>&)		const	{ return swizzle<0,2,3>(*this); }
	conic		operator&(const axis_s<2>&)		const	{ return swizzle<0,1,3>(*this); }
	conic		operator&(param(iso::plane) p)	const	{ return swizzle<0,1,3>(*this / from_xy_plane(p)); }

	friend quadric operator-(const quadric &a)						{ return a._neg(); }
	friend quadric operator+(const quadric &a, const quadric &b)	{ return a._add(b); }
	friend quadric operator-(const quadric &a, const quadric &b)	{ return a._sub(b); }
	template<typename T> friend IF_SCALAR(T,quadric) operator*(param(quadric) a, const T &b)	{ return a._smul(b); }
	template<typename T> friend IF_SCALAR(T,quadric) operator*(const T &a, param(quadric) b)	{ return b._smul(a); }

	//	template<typename M> quadric operator/(const M &m) const	{ return mul_mulT(transpose(m), *this); }
	//	template<typename M> friend force_inline IF_MAT(M, quadric) operator/(param(quadric) q, const M &m) { return mul_mulT(transpose(m), q); }
	//	template<typename M> friend force_inline IF_MAT(M, quadric) operator*(const M &m, param(quadric) q) { return q / inverse(m); }
	template<typename M> friend quadric operator*(const inverse_s<M> &m, const quadric &q)	{ return mul_mulT(transpose(m.m), (const symmetrical4&)q); }
	template<typename M> friend IF_MAT(M, quadric) operator*(const M &m, const quadric &q)	{ return mul_mulT(cofactors(m), (const symmetrical4&)q); }

	template<int X, int Y, int Z>			friend conic	swizzle(const quadric &q) { return swizzle<X, Y, Z>((const symmetrical4&)q); }
	template<int X, int Y, int Z, int W>	friend quadric	swizzle(const quadric &q) { return swizzle<X, Y, Z, W>((const symmetrical4&)q); }
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

		quadric::add_param(concat(normal, -dot(p0, normal)));
		a += weight;

		float4x4	iAt	= float4x4(p0, p1, p2, concat(normal, zero));
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
		d += b.d;
		diagonal<1>().d += b.diagonal<1>().d;
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
// sphere
//-----------------------------------------------------------------------------

float	projected_area(const sphere &s, const float4x4 &m);
conic	projection(param(sphere) s, const float4x4 &m);
int		check_occlusion(const sphere &sa, const sphere &sb, const position3 &camera);

template<typename E> pos<E,3> unit_sphere_uniform_surface(vec<E,2> t) {
	float	cos_theta	= one - t.x * two;
	float	sin_theta	= sin_cos(cos_theta);
	return position3(sincos(t.y * (pi * two)) * sin_theta, cos_theta);
}
template<typename E> pos<E,3> unit_sphere_uniform_interior(vec<E,3> t) {
	return unit_sphere_uniform_surface<E>(t.xy) * pow(t.z, third);
}
template<typename E> pos<E,3> uniform_surface(const n_sphere<E, 3> &s, vec<E,2> t) {
	return s.centre() + unit_sphere_uniform_surface<E>(t).v * s.radius();
}
template<typename E> pos<E,3> uniform_interior(const n_sphere<E, 3> &s, vec<E,3> t) {
	return s.centre() + unit_sphere_uniform_interior<E>(t).v * s.radius();
}

template<typename E> bool	unit_sphere_is_visible(const mat<E,4,4> &m) {
	auto	s	= sqrt(square(m.x.xyz + m.x.w) + square(m.y.xyz + m.y.w) +	square(m.z.xyz + m.z.w));
	return all(check_clip(m.w) < s);
}

template<typename E> bool	unit_sphere_is_within(const mat<E,4,4> &m) {
	auto	s	= sqrt(square(m.x.xyz + m.x.w) + square(m.y.xyz + m.y.w) + square(m.z.xyz + m.z.w));
	return all(check_clip(m.w) < -s);
}

template<typename E> bool is_visible(const n_sphere<E,3> &s, const mat<E,4,4> &m) {
	auto	r	= rsqrt(square(m.x.xyz + m.x.w) + square(m.y.xyz + m.y.w) + square(m.z.xyz + m.z.w));
	auto	t	= check_clip(m * s.centre());
	return all(t * r < s.radius());
}

template<typename E> bool is_within(const n_sphere<E,3> &s, const mat<E,4,4> &m) {
	auto	r	= rsqrt(square(m.x.xyz + m.x.w) + square(m.y.xyz + m.y.w) + square(m.z.xyz + m.z.w));
	auto	t	= check_clip(m * s.centre());
	return all(t * r < -s.radius());
}

//-----------------------------------------------------------------------------
// ellipsoid
//-----------------------------------------------------------------------------

class quadric;

class ellipsoid {
	position3	c;
	float3		a1;
	float4		a2r3;	//axis2 + size of axis3
public:
	ellipsoid() {}
	ellipsoid(param(position3) &centre, param(float3) axis1, param(float3) axis2, float radius3) : c(centre), a1(axis1), a2r3(concat(axis2, radius3)) {}
	ellipsoid(const sphere &c) : c(c.centre()), a1(concat(c.radius(), zero, zero)), a2r3(to<float>(zero, c.radius(), zero, c.radius()))		{}
	ellipsoid(const quadric &c);
	explicit ellipsoid(const float3x4 &m) : c(get_trans(m)), a1(m.x), a2r3(concat(m.y, len(m.z))) {}

	static ellipsoid		through(param(position3) p1, param(position3) p2)	{ return sphere::through(p1, p2); }
	static ellipsoid		through(param(position3) p1, param(position3) p2, param(position3) p3)	{ return sphere::through(p1, p2, p3); }
	static ellipsoid		through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4)	{ return sphere::through(p1, p2, p3, p4); }
	static ellipsoid		through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5) { return quadric::ellipsoid_through(p1, p2, p3, p4, p5); }
	static ellipsoid		through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6) { return quadric::ellipsoid_through(p1, p2, p3, p4, p5, p6); }
	static ellipsoid		through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7) { return quadric::ellipsoid_through(p1, p2, p3, p4, p5, p6, p7); }
	static ellipsoid		through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7, param(position3) p8) { return quadric::ellipsoid_through(p1, p2, p3, p4, p5, p6, p7, p8); }
	static ellipsoid		through(param(position3) p1, param(position3) p2, param(position3) p3, param(position3) p4, param(position3) p5, param(position3) p6, param(position3) p7, param(position3) p8, param(position3) p9) { return quadric::through(p1, p2, p3, p4, p5, p6, p7, p8, p9); }

	force_inline position3	centre()		const	{ return c;	}
	force_inline float3		axis1()			const	{ return a1;	}
	force_inline float3		axis2()			const	{ return a2r3.xyz;	}
	force_inline float3		axis3()			const	{ return normalise(cross(a1, a2r3.xyz)) * a2r3.w; }
	force_inline float		volume()		const	{ return len(cross(a1, a2r3.xyz)) * a2r3.w * (pi * 4 / 3.f); }

	force_inline float3x4	matrix()		const	{ return float3x4(axis1(), axis2(), axis3(), centre()); }
	force_inline auto		inv_matrix()	const	{ return inverse(matrix()); }
	force_inline cuboid		get_box()		const	{ return cuboid::with_centre(centre(), vector3(sqrt(square(axis1()) + square(axis2()) + square(axis3())))); }
	force_inline sphere		circumscribe()	const	{ return sphere(centre(), len(a1));	}
	force_inline sphere		inscribe()		const	{ return sphere(centre(), a2r3.w);	}	// expects r3 to be smallest

	bool					ray_check(param(ray3) r, float &t, vector3 *normal)	const;
	float					ray_closest(param(ray3) r)		const;
	bool					contains(param(position3) pos)	const { return len2((pos / matrix()).v) < one; }
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;

	void					operator*=(param(float3x4) m)	{ *this = ellipsoid(m * matrix()); }
	friend ellipsoid		operator*(param(float3x4) m, param(ellipsoid) b)	{ return ellipsoid(m * b.matrix()); }
	friend float			dist(param(ellipsoid) e, param(plane) p);
	friend position3		uniform_surface(const ellipsoid &s, float2 t)	{ return s.matrix() * unit_sphere_uniform_surface<float>(t); }
	friend position3		uniform_interior(const ellipsoid &s, float3 t)	{ return s.matrix() * unit_sphere_uniform_interior<float>(t); }
};

inline ellipsoid		fullmul(param(float3x4) m, param(sphere) b) { return ellipsoid(m * b.matrix()); }
inline quadric::quadric(const ellipsoid &e)	: quadric(e.matrix() * unit_sphere()) {}

inline bool is_visible(const ellipsoid &e, param(float4x4) m)	{ return unit_sphere_is_visible(m * e.inv_matrix()); }
inline bool is_within(const ellipsoid &e, param(float4x4) m)	{ return unit_sphere_is_within(m * e.inv_matrix()); }

//-----------------------------------------------------------------------------
// plane_shape - 2d shapes defined on a plane
//-----------------------------------------------------------------------------

template<typename S> struct plane_shape : S {
	iso::plane	pl;

	template<typename T> static auto plane_shape_param(const float3x4 &m, T &&t) { return forward<T>(t); }
	static position2 plane_shape_param(const float3x4 &m, param(position3) p) { return position2((m * p).v.xy); }

	plane_shape() {}
	plane_shape(param(iso::plane) p, paramT(S) s) : S(s), pl(p) {}
	template<typename...PP> plane_shape(param(position3) pos, param(vector3) norm, PP... pp) : pl(norm, pos) {
		new(this) S(position2((to_xy_plane(pl) * pos).v.xy), pp...);
	}
	template<typename...PP> plane_shape(param(position3) p0, param(position3) p1, param(position3) p2, PP... pp) : pl(p0, p1, p2) {
		float3x4 m = to_xy_plane(pl);
		new(this) S(position2((m * p0).v.xy), position2((m * p1).v.xy), position2((m * p2).v.xy), plane_shape_param(m, forward<PP>(pp))...);
	}

	iso::plane				plane()			const	{ return pl; }
	float3					normal()		const	{ return pl.normal(); }
	const S&				shape()			const	{ return *this; }
	force_inline position3	centre()		const	{ return from_xy_plane(pl) * position3(S::centre().v, zero);	}
	force_inline float3x4	matrix()		const	{ return from_xy_plane(pl) * float3x4(S::matrix()); }
	force_inline float3x4	inv_matrix()	const	{ return float3x4(S::inv_matrix()) * to_xy_plane(pl); }

	bool					ray_check(param(position3) p, param(vector3) d, float &t, vector3 *normal = 0) const;
	bool					ray_check(param(ray3) r, float &t, vector3 *normal = 0)	const { return ray_check(r.p, r.d, t, normal);	}
	float					ray_closest(param(position3) p, param(vector3) d)	const;
	float					ray_closest(param(ray3) r)	const	{ return ray_closest(r.p, r.d); }
	cuboid					get_box()		const;

	float3		parametric(param(position3) p)		const;
	float3		barycentric(param(position3) p)		const;
	position3	from_parametric(param(float2) p)	const;
	position3	corner(uint8 i)						const;

	bool		contains(param(position3) p)		const;
	sphere		inscribe()							const	{ return sphere(centre(), zero); }

	auto		corners()							const { auto m = from_xy_plane(pl); return transformc(S::corners(), [m](param(position2) p) { return m * position3(p); }); }
	auto		corners_cw()						const { auto m = from_xy_plane(pl); return transformc(S::corners_cw(), [m](param(position2) p) { return m * position3(p); }); }
	position3	closest(param(position3) p)			const { return from_xy_plane(pl) * position3(S::closest(position2((to_xy_plane(pl) * p).v.xy))); }
	position3	support(param(float3) v)			const { return from_xy_plane(pl) * position3(S::support((to_xy_plane(pl) * v).xy)); }

	void		operator*=(param(float3x4) m)	{ *this = m * *this; }

	friend plane_shape	operator*(param(float3x4) m, const plane_shape &s) {
		float3x4	m2 = m * from_xy_plane(s.pl);
		auto		p2 = iso::plane(m2.z, get_trans(m2));
		float3x4	m3 = to_xy_plane(p2) * m2;
		return plane_shape(p2, rows<0,1>(cols<0,1,3>(m3)) * s.shape());
	}
};

template<typename S> inline float3		plane_shape<S>::parametric(param(position3) p)		const	{ position3 p2 = to_xy_plane(pl) * p; return float3(S::parametric(p2.v.xy), p2.v.z);	}
template<typename S> inline float3		plane_shape<S>::barycentric(param(position3) p)		const	{ return iso::barycentric(parametric(p).xy); }
template<typename S> inline position3	plane_shape<S>::from_parametric(param(float2) p)	const	{ return from_xy_plane(pl) * position3(S::from_parametrix(p)); }
template<typename S> inline position3	plane_shape<S>::corner(uint8 i)						const	{ return from_xy_plane(pl) * position3(S::corner(i)); }
template<typename S> inline bool		plane_shape<S>::contains(param(position3) p)		const	{ return S::contains(position2((to_xy_plane(pl) * p).v.xy)); }
template<typename S> inline position3	uniform_interior(const plane_shape<S> &s, param(float2) t)	{ return from_xy_plane(s.pl) * position3(uniform_interior((const S&)s, t)); }
template<typename S> inline position3	uniform_interior(const plane_shape<S> &s, param(float3) t)	{ return uniform_interior(s, t.xy); }
template<typename S> inline position3	uniform_surface(const plane_shape<S> &s, param(float2) t)	{ return uniform_interior(s, t.xy); }
template<typename S> inline position3	uniform_perimeter(const plane_shape<S> &s, float x)			{ return from_xy_plane(s.pl) * position3(uniform_perimeter((const S&)s, x)); }
template<typename S> inline sphere		circumscribe(const plane_shape<S> &s)						{ circle c = circumscribe((const S&)s); return sphere(from_xy_plane(s.pl) * position3(c.centre()), c.radius()); }
template<typename S> inline bool		is_visible(const plane_shape<S> &s, param(float4x4) matrix);
template<typename S> inline bool		is_within(const plane_shape<S> &s, param(float4x4) matrix);

template<typename S> bool plane_shape<S>::ray_check(param(position3) p, param(vector3) d, float &t, vector3 *normal) const {
	float3x4	m	= from_xy_plane(pl);
	m.z = -d;

	float3		r	= p / m;
	if (r.z >= zero && S::contains(position2(r.xy))) {
		t = r.z;
		if (normal)
			*normal = this->normal();
		return true;
	}
	return false;
}

// this is wrong
template<typename S> float plane_shape<S>::ray_closest(param(position3) p, param(vector3) d) const {
	float3x4	m	= from_xy_plane(pl);
	m.z = -d;
	float3		r = p / m;
	return r.z;
}
template<typename S> float dist(paramT(plane_shape<S>) s, param(plane) p)	{
	return dist(s.shape(), to_xy_plane(s.pl) * (p & s.plane()));
}

//-----------------------------------------------------------------------------
// circle3
//-----------------------------------------------------------------------------

typedef plane_shape<circle> circle3;

//template<> force_inline	cuboid		circle3::get_box()			const { return cuboid::with_centre(centre(), (abs(shift<1>(pl.normal())) + abs(shift<2>(pl.normal()))) * radius()); }
//template<> force_inline	cuboid		circle3::get_box()			const { return cuboid::with_centre(centre(), sqrt((one - square(normal()) / len2(normal())) * radius2())); }
template<> force_inline		cuboid		circle3::get_box()			const { return cuboid::with_centre(centre(), sqrt(((one - square(normal()) * radius2()) / len2(normal())))); }

inline ellipse project(param(circle3) c) {
	float3x4	m = c.inv_matrix();
	return conic::from_matrix(float3x3(m.x, m.y, m.w));
}

inline circle3	project(param(sphere) s, param(plane) p) {
	return circle3(p, circle(position2((to_xy_plane(p) * s.centre()).v.xy), s.radius()));
}

circle3 seen_from(param(sphere) s, param(position3) pos);
circle3	operator&(param(sphere) a, param(plane) b);

bool circle_through_contains(param(position3) p, param(position3) q, param(position3) r, param(position3) t);

//-----------------------------------------------------------------------------
// plane_shape2 - 2d shapes defined on a plane
//-----------------------------------------------------------------------------

struct plane_shape2 {
	float3			x;
	float3			y;
	position3		z;
	plane_shape2() {}
	plane_shape2(param(float3) x, param(float3) y, param(position3) z) : x(x), y(y), z(z) {}

	float3			normal()							const	{ return cross(x, y); }
	iso::plane		plane()								const	{ return iso::plane(normal(), z); }
	float3x4		matrix()							const	{ return float3x4(x, y, normal(), z); }
	float3x4		inv_matrix()						const	{ return inverse(matrix()); }
	float3			parametric(param(position3) p)		const	{ return p / matrix(); }
	position3		from_parametric(param(float2) p)	const	{ return z + x * p.x + y * p.y; }
};

struct triangle3 : plane_shape2 {
	triangle3() {}
	triangle3(param(position3) a, param(position3) b, param(position3) c) : plane_shape2(b - a, c - a, a) {}

	position3		corner(int i)						const	{ return (position3)select(i < 2, z.v + (&x)[i], z.v);		}
	position3		centre()							const	{ return z + (x + y) / 3; }
	cuboid			get_box()							const	{ return cuboid(z + min(min(x, y), zero), z + max(max(x, y), zero)); }
	float			area()								const	{ return len(cross(x, y)) * half;	}
	float3			barycentric(param(position3) p)		const	{ return iso::barycentric(parametric(p).xy); }

	bool			contains(param(position3) p) const {
		float3		r	= parametric(p);
		return approx_equal(r.z, zero) && all(iso::barycentric(r.xy) <= one);
	}

	bool			ray_check(param(ray3) r, float &t, vector3 *normal = 0) const {
		float3x4	m	= float3x4(x, y, -r.d, z);
		float3		u	= float3(r.p / m);
		if (u.z >= zero && u.x >= zero && u.y >= zero && u.x + u.y < one) {
			t = u.z;
			if (normal)
				*normal = this->normal();
			return true;
		}
		return false;
	}
	auto			solid_angle(param(position3) p)		const { float3 t = z - p; return triangle_solid_angle(t, t + x, t + y); }
	auto			signed_volume(param(position3) p)	const { return dot(cross(x, y), z - p);	}
	auto			signed_volume()						const { return dot(cross(x, y), z.v);	}

	interval<float>	projection(param(float3) n) const {
		auto dz = dot(n, z.v);
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

	friend sphere		circumscribe(const triangle3 &s)						{ return sphere::through(s.z, s.z + s.x, s.z + s.y); }
	friend position3	uniform_interior(const triangle3 &s, param(float2) t)	{
		return s.from_parametric(t.x + t.y > one ? one - t : t);
		//return s.from_parametric(sqrt(t.x) * float2{one - t.y, t.y});
	}
};

bool	intersect(const triangle3 &a, const triangle3 &b);

struct parallelogram3 : plane_shape2 {
	parallelogram3(param(float3) x, param(float3) y, param(position3) z) : plane_shape2(x, y, z) {}

	position3		corner(CORNER i)	const	{ return z + select((i & 1), x, zero) + select((i & 2), y, zero); }
	position3		centre()			const	{ return z + (x + y) * half; }
	cuboid			get_box()			const	{ return cuboid(z, z + x + y); }
	float			area()				const	{ return len(cross(x, y));	}

	bool			contains(param(position3) p) const {
		float3		r	= parametric(p);
		return approx_equal(r.z, zero) && all(iso::barycentric(r.xy) <= one);
	}
	bool			ray_check(param(ray3) r, float &t, vector3 *normal = 0) const {
		float3x4	m	= float3x4(x, y, -r.d, z);
		float3		u	= float3(r.p / m);
		if (u.z >= zero && all(iso::barycentric(u.xy) <= one)) {
			t = u.z;
			if (normal)
				*normal = this->normal();
			return true;
		}
		return false;
	}
	auto			solid_angle(param(position3) p)	const {
		float3 t = z - p;
		return	triangle_solid_angle(t, t + x, t + y)
			+	triangle_solid_angle(t + y, t + x, t + x + y);
	}
	interval<float>	projection(param(float3) n) const {
		float	dz = dot(n, z.v), h = dot(n, x + y);
		return {dz, dz + h};
	}
	interval<float>	projection(param(iso::plane) p) const {
		float	dz = p.dist(z), h = dot(p.normal(), x + y);
		return {dz, dz + h};
	}
	interval<float>	intersection(const line3 &line) const;

	friend sphere		circumscribe(const parallelogram3 &s)						{ return sphere::with_r2(s.centre(), len2(s.x + s.y)); }
	friend position3	uniform_interior(const parallelogram3 &s, param(float2) t)	{ return s.from_parametric(t); }
};

class ellipse3 : plane_shape2 {
public:
	ellipse3(param(float3) x, param(float3) y, param(position3) z) : plane_shape2(x, y, z) {}
	force_inline position3	centre()		const	{ return z; }
	force_inline float3		major()			const	{ return x; }
	force_inline float3		minor()			const	{ return y; }
	force_inline float		area()			const	{ return len(cross(x, y)) * pi;	}
	float					perimeter()		const	{ return ellipse_perimeter_approx(len(x), len(y) / len(x)); }

	bool			contains(param(position3) p) const {
		float3		r	= float3(p / matrix());
		return approx_equal(r.z, zero) && square(r.x) + square(r.y) <= one;
	}
	bool			ray_check(param(ray3) r, float &t, vector3 *normal = 0) const {
		float3x4	m	= float3x4(x, y, -r.d, z);
		float3		u	= float3(r.p / m);
		if (square(u.x) + square(u.y) <= one) {
			t = u.z;
			if (normal)
				*normal = this->normal();
			return true;
		}
		return false;
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
	tetrahedron(param(triangle3) t, param(position3) p) : float3x4(t.x, t.y, p - t.z, t.z)	{}

	position3	centre()							const	{ return position3(w + (x + y + z) / 4); }
	float3		parametric(param(position3) p)		const	{ return float3(p / *(const float3x4*)this); }
	float4		barycentric(param(position3) p)		const	{ return iso::barycentric(parametric(p)); }
	position3	from_parametric(param(float3) p)	const	{ return *(const float3x4*)this * position3(p);	}
	position3	corner(int i)						const	{ return position3(select(i < 3, w + (*this)[i], w)); }
	iso::plane	plane(int i)						const	{ return i < 3 ? iso::plane(cross(column(inc_mod(i, 3)), column(dec_mod(i, 3))), centre()) : iso::plane(cross(z - x, y - x), centre() + x); }
	triangle3	face(int i)							const	{ return triangle3(corner((i + 1) & 3), corner((i + 2) & 3), corner((i + 3) & 3)); }
	float		volume()							const	{ return det() / 6; }

	const float3x4&	matrix()						const	{ return *this; }
	auto			inv_matrix()					const	{ return inverse(matrix()); }

	// columns are planes
	float4x4	planes_matrix()						const	{
		float4x4	m	= transpose(inv_matrix());
		float4		s	= m.x + m.y + m.z;
		m.w	= concat(-s.xyz, one);
		return m;
	}

	bool		contains(param(position3) p)		const	{ return all(barycentric(p) > zero); }
	bool		ray_check(param(position3) p, param(vector3) d, float &t, vector3 *normal) const;
	bool		ray_check(param(ray3) r, float &t, vector3 *normal)	const { return ray_check(r.p, r.d, t, normal);	}
	float		ray_closest(param(position3) p, param(vector3) d)	const;
	float		ray_closest(param(ray3) r)			const	{ return ray_closest(r.p, r.d); }
	position3	closest(param(position3) p)			const;
	position3	support(param(float3) v)			const;

	sphere		inscribe()				const;
	sphere		circumscribe()			const;
	cuboid		get_box()				const { return cuboid(position3(min(min(min(x, y), z), zero) + w), position3(max(max(max(x, y), z), zero) + w)); }

	array<position3,4>	corners() const { return {centre() + x, centre() + y, centre() + z, centre()}; }

	void					operator*=(param(float3x4) m)	{ float3x4::operator=(m * matrix()); }
	friend float			dist(param(tetrahedron) t, param(iso::plane) p) {
		float	dx	= dot(p.normal(), t.x);
		float	dy	= dot(p.normal(), t.y);
		float	dz	= dot(p.normal(), t.z);
		return p.dist(t.centre()) + min(min(min(dx, dy), dz), zero);
	}
	friend position3	uniform_surface(param(tetrahedron) c, param(float2) t);
	friend position3	uniform_interior(param(tetrahedron) c, param(float3) t);

	friend array<iso::plane,4>	get_planes(const tetrahedron &t) {
		float4x4	m = t.planes_matrix();
		return {iso::plane(m.x), iso::plane(m.y), iso::plane(m.z), iso::plane(m.w)};
	}
};

bool is_visible(const tetrahedron &t, param(float4x4) m);
bool is_within(const tetrahedron &t, param(float4x4) m);

//-----------------------------------------------------------------------------
// parallelepiped
//-----------------------------------------------------------------------------

typedef n_parallelotope<float, 3> parallelepiped;
template<> position3 parallelepiped::closest(position3 p) const;

template<typename E> inline parallelogram3	get_face(const n_parallelotope<E, 3> &p, PLANE i) {
	int	a = i / 4, x = (i & 1) * 3;
	return parallelogram3(
		p.corner((CORNER)rotate_left(0 ^ x, a, 3)),
		p.corner((CORNER)rotate_left(2 ^ x, a, 3)),
		p.corner((CORNER)rotate_left(4 ^ x, a, 3))
	);
}

template<typename E> pos<E,3> uniform_surface(const n_parallelotope<E, 3> &s, vec<E,2> t) {
	vec<E,3>	areas	= {
		area(get_face(s, PLANE_PLUS_X)),
		area(get_face(s, PLANE_PLUS_Y)),
		area(get_face(s, PLANE_PLUS_Z))
	};
	areas	/= reduce_add(areas);
	E	x	= t.x;
	E	y	= sign1(x - half);
	x		= frac(x * two);
	pos<E,3>	t2
		= x < areas.x			? pos<E,3>(y, t.y, x / areas.x)
		: x < areas.x + areas.y	? pos<E,3>((x - areas.x) / areas.y, y, t.y)
		: pos<E,3>(t.y, (x - areas.x - areas.y) / areas.z, y);
	return s.matrix() * t2;
}


//-----------------------------------------------------------------------------
// obb3
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// baseclass for cylinder, cone, capsule
//-----------------------------------------------------------------------------

class _directed_shape {
protected:
	float4		v;
	vector3		d;
	force_inline _directed_shape(param(position3) pos, param(vector3) dir, float r) : v(concat(pos.v, r)), d(dir)	{}
	_directed_shape		mul(param(float3x4) m)	const { return _directed_shape(m * centre(), m * d, radius() * len(m.x)); }
public:
	force_inline position3	centre()		const	{ return position3(v.xyz);	}
	force_inline vector3	dir()			const	{ return d;		}
	force_inline float		radius()		const	{ return v.w;	}
//	force_inline float3x4	matrix()		const	{ return translate(centre()) * look_along_z(dir()) * scale(concat(float2(radius()), len(dir()))); }
	force_inline float3x4	matrix()		const	{ return translate(centre()) * look_along_z_scaled(dir(), radius()); }
	force_inline auto		inv_matrix()	const	{ return inverse(matrix());	}

	void					operator*=(param(float3x4) m)	{ v = concat((m * centre()).v, radius() * len(m.x)); d = m * d; }
};

//-----------------------------------------------------------------------------
// cylinder
//-----------------------------------------------------------------------------

class cylinder : public _directed_shape {
	force_inline explicit cylinder(const _directed_shape &s) : _directed_shape(s)	{}
public:
	force_inline cylinder(param(position3) pos, param(vector3) dir, float r) : _directed_shape(pos, dir, r)	{}
	force_inline cylinder(param(position3) p1, param(position3) p2, float r) : _directed_shape(position3((p1.v + p2.v) / 2), (p2 - p1) / 2, r)	{}
	force_inline cuboid		get_box()		const	{ return cuboid::with_centre(centre(), abs(d) + sqrt((one - square(d)) / len2(d)) * radius()); }

	force_inline sphere		circumscribe()	const	{ return sphere(centre(), sqrt(len2(d) + square(radius()))); }
	force_inline sphere		inscribe()		const	{ return sphere(centre(), min(radius(), len(d))); }
	force_inline float		volume()		const	{ return len(d) * square(radius()) * pi; }
	force_inline float		surface_area()	const	{ return (radius() + len(d)) * radius() * (two * pi); }

	bool					ray_check(param(ray3) r, float &t, vector3 *normal)	const;
	float					ray_closest(param(ray3) r)		const;
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;
	bool					contains(param(position3) p)	const;

	operator quadric() const { return matrix() * quadric::unit_cylinder(); }

	friend float			dist(param(cylinder) c, param(plane) p) {
		float	dz	= abs(dot(p.normal(), c.dir()));
		float	dx	= sqrt(one - square(dz) / len2(c.dir())) * c.radius();
		return p.dist(c.centre() - float3(dz + dx));
		return 0;
	}

	friend cylinder operator*(param(float3x4) m, param(cylinder) c) { return cylinder(c.mul(m)); }
	friend position3	uniform_surface(param(cylinder) c, param(float2) t);
	friend position3	uniform_interior(param(cylinder) c, param(float3) t);
};

bool is_visible(const cylinder &c, param(float4x4) m);
bool is_within(const cylinder &c, param(float4x4) m);

//-----------------------------------------------------------------------------
// cone
//-----------------------------------------------------------------------------

class cone : public _directed_shape {
	force_inline explicit cone(const _directed_shape &s) : _directed_shape(s)	{}
public:
	force_inline cone(param(position3) pos, param(vector3) dir, float r) : _directed_shape(pos, dir, r)	{}
	force_inline cuboid		get_box()		const	{
		vector3 v = (abs(d.yzx) + abs(d.zxy)) * (radius() / len(d));
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
	force_inline float		volume()		const	{ return pi / 3.0f * 2 * len(d) * square(radius());	}
	force_inline circle3	base()			const	{ return circle3(centre() - dir(), dir(), radius()); }
	force_inline position3	apex()			const	{ return centre() + dir();	}

	bool					ray_check(param(ray3) r, float &t, vector3 *normal)	const;
	float					ray_closest(param(ray3) r)		const;
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;
	bool					contains(param(position3) p)	const;

	operator quadric() const {
		float3x4	m = translate(apex()) * look_along_z(dir()) * scale(concat(float2(radius()), len(dir()) * two));
		return m * quadric::unit_cone();
	}

	friend float			dist(param(cone) c, param(plane) p) {
		float	dz	= dot(p.normal(), c.dir());
		if (dz > 0)
			return p.dist(c.centre() - float3(dz));

		float	dx	= sqrt(one - square(dz) / len2(c.dir())) * c.radius();
		return p.dist(c.centre() + dz + dx);
	}

	friend cone operator*(param(float3x4) m, param(cone) c) { return cone(c.mul(m)); }
	friend position3	uniform_surface(param(cone) c, param(float2) t);
	friend position3	uniform_interior(param(cone) c, param(float3) t);
};

bool is_visible(const cone &c, param(float4x4) m);
bool is_within(const cone &c, param(float4x4) m);


//-----------------------------------------------------------------------------
// capsule
//-----------------------------------------------------------------------------

class capsule : public _directed_shape {
	force_inline explicit capsule(const _directed_shape &s) : _directed_shape(s)	{}
public:
	force_inline capsule(param(position3) pos, param(vector3) dir, float r) : _directed_shape(pos, dir, r)	{}
	force_inline cuboid		get_box()		const	{ return cuboid::with_centre(centre(), vector3(abs(vector3(d)) + radius()));	}
	force_inline sphere		circumscribe()	const	{ return sphere(centre(), len(d) + radius()); }
	force_inline sphere		inscribe()		const	{ return sphere(centre(), radius()); }
	force_inline float		volume()		const	{ return pi * len(d) * square(radius()) + (pi * 4.0f / 3.0f) * cube(radius());	}

	bool					ray_check(param(ray3) r, float &t, vector3 *normal)	const;
	float					ray_closest(param(ray3) r)		const;
	position3				closest(param(position3) p)		const;
	position3				support(param(float3) v)		const;
	bool					contains(param(position3) p)	const;

	friend float			dist(param(capsule) c, param(plane) p) {
		float	dz	= abs(dot(p.normal(), c.dir()));
		return p.dist(c.centre() - float3(dz + c.radius()));
	}
	friend capsule operator*(param(float3x4) m, param(capsule) c) { return capsule(c.mul(m)); }
	friend position3	uniform_surface(param(capsule) c, param(float2) t);
	friend position3	uniform_interior(param(capsule) c, param(float3) t);
};

bool is_visible(const capsule &c, param(float4x4) m);
bool is_within(const capsule &c, param(float4x4) m);

//-----------------------------------------------------------------------------
// wedge3
//	infinite 3d wedge defined by a point and three normals
//-----------------------------------------------------------------------------

struct wedge3 : float3x4 {
	wedge3()	{}
	wedge3(param(position3) a, param(float3) x, param(float3) y, param(float3) z) : float3x4(x, y, z, a)	{}

	bool	contains(param(position3) b) const {
		return dot(x, b.v - w) >= zero && dot(y, b.v - w) >= zero && dot(z, b.v - w) >= zero;
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

		return dot(v1, cross(v2 - v1, v3 - v1)) > zero || s.contains(position3(w));
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

struct rectangles0 : trailing_array2<rectangles0, rectangle> {
	int		num;
	int		size()	const	{ return num; }
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
