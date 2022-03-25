#ifndef GJK_H
#define GJK_H

#include "maths/geometry.h"
#include "maths/polygon.h"
#include "base/functions.h"

namespace iso {

template<typename T> struct GJK_shape;

//-----------------------------------------------------------------------------
//	2D
//-----------------------------------------------------------------------------

struct GJK2_Primitive : virtfunc<position2(param(float2))> {
public:
	template<typename T> GJK2_Primitive(T *t) : virtfunc<position2(param(float2))>(t)	{}
	inline position2	support(param(float2) v) { return (*this)(v); }
};

template<typename T> struct GJK2_PrimitiveT : GJK2_Primitive {
	T	t;
	position2	operator()(param(float2) v) const	{ return t.support(v); }
	GJK2_PrimitiveT(const T &t) : GJK2_Primitive(this), t(t) {}
};

template<typename T> GJK2_PrimitiveT<T> make_gjk_primitive(const T &t) { return t; }

struct GJK2_TransPrim : GJK2_Primitive {
	float2x3	mat;
	template<typename T> GJK2_TransPrim(T *t, const float2x3 &_mat) : GJK2_Primitive(t), mat(_mat) {}
};

template<typename T> struct GJK2_TransShapePrim : GJK2_TransPrim, GJK_shape<T> {
	position2	operator()(param(float2) v) const	{ return mat * GJK_shape<T>::operator()(v / (float2x2)mat); }
	GJK2_TransShapePrim(const T &t, const float2x3 &_mat) : GJK2_TransPrim(this, _mat * t.matrix()) {}
	GJK2_TransShapePrim(const T &t) : GJK2_TransPrim(this, t.matrix()) {}
};

struct GJK_origin2 : GJK2_Primitive {
	position2	operator()(param(float2) v) const { return position2(zero); }
	GJK_origin2() : GJK2_Primitive(this) {}
};

struct GJK_position2 : GJK2_Primitive {
	position2	p;
	position2	operator()(param(float2) v)	const { return p; }
	GJK_position2(param(position2) p) : GJK2_Primitive(this), p(p) {}
};

template<> struct GJK_shape<ray2> 		   { position2 operator()(param(float2) v)	{ return position2(sign1(v.x), zero); } };
template<> struct GJK_shape<circle> 	   { position2 operator()(param(float2) v)	{ return position2(normalise(v)); } };
template<> struct GJK_shape<ellipse> 	   { position2 operator()(param(float2) v); };
template<> struct GJK_shape<rectangle> 	   { position2 operator()(param(float2) v)	{ return position2(sign1(v)); } };
template<> struct GJK_shape<parallelogram> { position2 operator()(param(float2) v); };
template<> struct GJK_shape<triangle> 	   { position2 operator()(param(float2) v)	{ return v.x > zero ? position2(one, zero) : v.y > zero ? position2(zero, one) : position2(zero); } };
template<> struct GJK_shape<quadrilateral> { position2 operator()(param(float2) v); };

// GJK using Voronoi regions (Christer Ericson) and Barycentric coordinates.

class GJK2 {
	typedef callback<position2(param(float2))>	support;

	// z is barycentric coordinate for closest point; NB - only count-1 of these is actually updated
	float3	v1, v2, v3;

	static float2 get_dir(int count, param(float2) v1, param(float2) v2) {
		if (count == 1)
			return -v1;
		float2 d = v2 - v1;
		return perp(d) * sign(cross(v1, d));
	}

	float2 get_dir(int count) const {
		return count < 3 ? get_dir(count, v1.xy, v2.xy) : float2(zero);
	}

	float2 closest(int count) const {
		switch (count) {
			case 1:		return v1.xy;
			case 2:		return v1.xy * v1.z + v2.xy * (one - v1.z);
			default:	return float2(zero);
		}
	}

	int solve2();
	int solve3();
	bool run(GJK2_Primitive &a, GJK2_Primitive &b, int count, float2 *separation);
	bool run(support &a, support &b, int count, float2 *separation);

public:

	bool intersect(support &&a, support &&b, param(float2) witness1, param(float2) witness2, float2 *separation = nullptr) {
		v1.xy	= witness1;
		v2.xy	= witness2;
		return run(a, b, solve2(), separation);
	}
	bool intersect(support &&a, support &&b, param(float2) initial_dir, float2 *separation = nullptr) {
		v1.xy	= a(-initial_dir) - b(initial_dir);
		return run(a, b, 1, separation);
	}

	bool intersect(GJK2_Primitive &a, GJK2_Primitive &b, param(float2) initial_dir, float2 *separation = nullptr) {
		v1.xy	= b.support(initial_dir) - a.support(-initial_dir);
		return run(a, b, 1, separation);
	}

	bool intersect(GJK2_Primitive &&a, GJK2_Primitive &&b, param(float2) witness1, param(float2) witness2, float2 *separation = nullptr) {
		v1.xy	= witness1;
		v2.xy	= witness2;
		return run(a, b, solve2(), separation);
	}

	bool intersect(GJK2_TransPrim &a, GJK2_TransPrim &b, float2 *separation = nullptr) {
		return intersect(a, b, get_trans(a.mat) - get_trans(b.mat), separation);
	}
};

//-----------------------------------------------------------------------------
//	3D
//-----------------------------------------------------------------------------

struct GJK3_Primitive : virtfunc<position3(param(float3))> {
public:
	template<typename T> GJK3_Primitive(T *t) : virtfunc<position3(param(float3))>(t)	{}
	inline position3	support(param(float3) v)	{ return (*this)(v); }
};
struct GJK3_TransPrim : GJK3_Primitive {
	float3x4	mat, imat;
	template<typename T> GJK3_TransPrim(T *t, const float3x4 &_mat) : GJK3_Primitive(t), mat(_mat), imat(get(inverse(_mat))) {}
};
template<typename T> struct GJK3_ShapePrim : GJK3_Primitive, GJK_shape<T> {
	GJK3_ShapePrim() : GJK3_Primitive(this) {}
};
template<typename T> struct GJK3_TransShapePrim : GJK3_TransPrim, GJK_shape<T> {
	position3	operator()(param(float3) v) const	{ return mat * GJK_shape<T>::operator()(imat * v); }
	GJK3_TransShapePrim(const T &t, const float3x4 &_mat) : GJK3_TransPrim(this, _mat * t.matrix()) {}
};

struct GJK_origin3 : GJK3_Primitive {
	position3	operator()(param(float3) v) const	{ return position3(zero); }
	GJK_origin3() : GJK3_Primitive(this) {}
};

struct GJK_position3 : GJK3_Primitive {
	position3	p;
	position3	operator()(param(float3) v)	const { return p; }
	GJK_position3(param(position2) p) : GJK3_Primitive(this), p(p) {}
};

template<> struct GJK_shape<ray3> {
	position3	operator()(param(float3) v) const	{ return position3(sign1(v.x), zero, zero); }
};
template<> struct GJK_shape<sphere> {
	position3	operator()(param(float3) v) const	{ return position3(normalise(v)); }
};
template<> struct GJK_shape<obb3> {
	position3	operator()(param(float3) v) const	{ return position3(sign1(v)); }
};
template<> struct GJK_shape<capsule> {
	position3	operator()(param(float3) v) const	{ return position3(normalise(v.xy), sign1(v.z)); }
};
template<> struct GJK_shape<cylinder> {
	position3	operator()(param(float3) v) const	{ return position3(normalise(v.xy), sign1(v.z)); }
};
template<> struct GJK_shape<cone> {
	position3	operator()(param(float3) v) const	{ return position3(select(v.z > len(v) * rsqrt(5.0f), float3(z_axis), normalise(concat(v.xy, -one)))); }
};

struct GJK_Mesh {
	float3p		*p;
	int			n;
	intptr_t	prev_i;

	position3	operator()(param(float3) v) {
		float3p	*s = support(p, p + n, v);
		prev_i	= s - p;
		return position3(*s);
	}
	GJK_Mesh(float3p *p, int n) : p(p), n(n), prev_i(0)	{}
};

struct GJK_Normals : public GJK3_Primitive, dynamic_array<float3> {
	int			prev_i;

	position3	operator()(param(float3) v) {
		auto	*s = iso::support(this->begin(), this->end(), v);
		prev_i	= index_of(s);
		return position3(*s);
	}
	GJK_Normals() : GJK3_Primitive(this), prev_i(0)	{}
};

class GJK3 {
	position3	p[4];		// support points of object A in local coordinates
	float3		y[4];		// support points of A - B in world coordinates
	int			bits;		// identifies current simplex
	int			last;		// identifies last found support point
	int			last_bit;	// last_bit = 1<<last
	int			all_bits;	// all_bits = bits|last_bit
	float		det[16][4];	// cached sub-determinants
	float		dp[4][4];	// cached dot products
	float		max_sep;	// max separation

public:
	float		penetration;
	position3	position;
	float3		normal;

private:
	void		compute_det();
	bool		valid(int s) const;
	float3		compute_vector(const float3 *p, int bits1) const;
	bool		closest(float3 &v);
	bool		degenerate(param(float3) w, int bits);
	void		clear_old_dets();
public:
	GJK3() : bits(0), all_bits(0), max_sep(0) {
		for (int j = 0; j < 16; j++)
			for (int i = 0; i < 4; ++i)
				det[j][i] = FLT_NAN;
	}
	void	reset() {
		bits		= 0;
		all_bits	= 0;
		max_sep		= 0;
		for (int j = 0; j < 16; j++)
			for (int i = 0; i < 4; ++i)
				det[j][i] = FLT_NAN;
	}

	plane	separating_plane() const {
		return plane(normal, position);
	}
	position3	simplex_point(int i) const {
		return position3(p[i]);
	}

	bool	intersect(GJK3_Primitive &a, GJK3_Primitive &b, float3 &v, float sep = 0, float sep_thresh = 0);
	bool	intersect(GJK3_TransPrim &a, GJK3_TransPrim &b, float sep = 0, float sep_thresh = 0) {
		float3	v = get_trans(a.mat) - get_trans(b.mat);
		return intersect(a, b, v, sep, sep_thresh);
	}
	bool	collide(GJK3_TransPrim &a, GJK3_TransPrim &b, bool separate = true);
	bool	collide(GJK3_TransPrim &&a, GJK3_TransPrim &&b, bool separate = true) {
		return collide(a, b, separate);
	}
};

//-----------------------------------------------------------------------------
//	Wrappers
//-----------------------------------------------------------------------------
#if 0
struct DistanceOutput {
	position2	pointA;		// closest point on shapeA
	position2	pointB;		// closest point on shapeB
	float		distance;

	void	Set(param(position2) a, param(position2) b) {
		pointA		= a;
		pointB		= b;
		distance	= len(a - b);
	}

	void ApplyRadii(float rA, float rB) {
		if (distance > rA + rB && distance > (float)epsilon) {
			// shapes are still not overlapped - move the witness points to the outer surface
			float2 n	= normalise(pointB - pointA);
			distance	-= rA + rB;
			pointA		+= rA * n;
			pointB		-= rB * n;
		} else {
			// shapes are overlapped when radii are considered - move the witness points to the middle
			pointA		= pointB = (pointA + pointB) * half;
			distance	= 0;
		}
	}
};

template<typename A, typename B> void Distance(DistanceOutput* output, GJK2_TransShapePrim<A> &&a, GJK2_TransShapePrim<B> &&b) {
	GJK2	gjk;
	float2 	separation
	gjk.intersect(a, b, a.mat, b.mat, &separation);
	gjk.get_witnesses(output->pointA, output->pointB);
	output->distance = len(output->pointA - output->pointB);
}

template<> void Distance(DistanceOutput* output, GJK2_TransShapePrim<circle> &&a, GJK2_TransShapePrim<circle> &&b);

template<typename A, typename B> void Distance(DistanceOutput* output, const A &a, const B &b, param(float2x3) mata, param(float2x3) matb) {
	Distance(output, GJK2_TransShapePrim<A>(a, mata), GJK2_TransShapePrim<B>(b, matb));
}
#endif

//float closest_point(const convex_polygon<range<position2*>> &a, const convex_polygon<range<position2*>> &b, int &va, int &vb);
//float closest_point(const convex_polyhedron &a, const convex_polyhedron &b, int &va, int &vb);

} // namespace iso

#endif // GJK_H
