#ifndef CSG_H
#define CSG_H

#include "base/array.h"
#include "base/algorithm.h"
#include "maths/geometry.h"

namespace iso {

//#define DEFER_INVERT

// A BSP tree is built from a collection of polygons by picking a polygon to split along
// That polygon (and all other coplanar polygons) are added directly to that csg_node and the other polygons are added to the front and/or back subtrees

struct csgnode {
#if 1
	struct plane2 {
		vector3		n;
		position3	p;
		plane2(const _zero&)	: n(zero)	{}
		plane2(param(vector3) n, param(position3) p) : n(n), p(p) {}
		float3	normal()					const	{ return n;	}
		float	dist(param(position3) x)	const	{ return dot(x - p, n); }
		plane2 	operator-()					const	{ return plane2(-n, p); }
		auto operator/(const float3x4 &m) 	const 	{ return plane(transpose((float3x3)m) * n, p / m); }
	};
#else
	typedef plane plane2;
#endif

	// coplanar and convex - stored as parametric values of original triangle3
	struct polygon {
		float3x4				matrix;
		intptr_t				face_id;
		dynamic_array<float2,8>	verts;
		bool					flipped;

		polygon(intptr_t face_id, const triangle3 &tri) : matrix(tri.matrix()), face_id(face_id), verts(3), flipped(false) {
			verts[0]	= float2{0, 0};
			verts[1]	= float2{1, 0};
			verts[2]	= float2{0, 1};
		}
		polygon(intptr_t face_id, const parallelogram3 &rect) : matrix(rect.matrix()), face_id(face_id), verts(4), flipped(false) {
			verts[0]	= float2{0, 0};
			verts[1]	= float2{1, 0};
			verts[2]	= float2{0, 1};
			verts[3]	= float2{1, 1};
		}
		polygon(intptr_t face_id, param(position3) p0, param(position3) p1, param(position3) p2) : matrix(p1 - p0, p2 - p0, normalise(cross(p1 - p0, p2 - p0)), p0), face_id(face_id), verts(3), flipped(false) {
			verts[0]	= float2{0, 0};
			verts[1]	= float2{1, 0};
			verts[2]	= float2{0, 1};
		}
		polygon(const polygon &b, dynamic_array<float2> &&verts) : matrix(b.matrix), face_id(b.face_id), verts(move(verts)), flipped(b.flipped) {}
		void		flip()	{
			swap(matrix.x, matrix.y);
			matrix.z = -matrix.z;
			for (auto a = verts.begin(), b = verts.end(); a < b--; ++a) {
				float2	t = *a;
				*a	= b->yx;
				*b	= t.yx;
			}
			flipped = !flipped;
		}
		iso::plane		plane()		const	{ return iso::plane(matrix.z, get_trans(matrix)); }
		float3			normal()	const	{ return matrix.z; }
		interval<float>	quick_projection(param(float3) v) const;
		interval<float>	projection(param(float3) v) const;
		interval<float>	intersection(const line3 &line) const;
		void		split(const plane2 &splitplane, float epsilon, dynamic_array<polygon> &coplanarFront, dynamic_array<polygon> &coplanarBack, dynamic_array<polygon> &front, dynamic_array<polygon> &back) const;
	};

	plane2		p;
	dynamic_array<polygon> polygons;
	uint32		total;
	csgnode		*front;
	csgnode		*back;

	csgnode() 					: p(zero), total(0), front(0), back(0) {}
	csgnode(csgnode &&x) 		: p(x.p), polygons(move(x.polygons)), total(x.total), front(x.front), back(x.back) { x.front = x.back = 0; }
	csgnode(const csgnode &x) 	: p(x.p), polygons(x.polygons), total(x.total), front(x.front), back(x.back) {}
};

struct csg : csgnode {
	float	epsilon;

	csg() : epsilon(0.001f) {}
	csg(const csg &x);
	csg(csg &&x) : csgnode(move(x)), epsilon(x.epsilon) {}
	csg(dynamic_array<polygon> &&list, float epsilon = 0.001f);
	csg(const parallelepiped &box, float epsilon = 0.001f);
	~csg();

	csg&	operator+=(dynamic_array<polygon> &&list);
	csg& 	operator=(csg &&x) {
		epsilon	= x.epsilon;
		p		= x.p;
		total	= x.total;
		swap(polygons, 	x.polygons);
		swap(front, 	x.front);
		swap(back, 		x.back);
		return *this;
	}

#ifdef DEFER_INVERT
	csg(const _not<csg> &b);
	_not<csg>	invert()	const		{ return *this; }
	_not<csg>	operator~() const		{ return *this; }
#else
	csg& invert();	// Convert solid space to empty space and empty space to solid space (modifies this)
	friend csg operator~(const csg &n)	{ return csg(n).invert(); }
	friend csg operator~(csg &&n)		{ return csg(move(n)).invert(); }
#endif

	dynamic_array<polygon> polys()		const;
};

bool intersect(const csgnode::polygon &p0, const csgnode::polygon &p1);


csg operator|(const csg &a1, const csg &b1);
csg operator-(const csg &a1, const csg &b1);
csg operator&(const csg &a1, const csg &b1);

} // namespace iso

#endif //CSG_H
