#ifndef CSG2_H
#define CSG2_H

#include "base/array.h"
#include "base/algorithm.h"
#include "maths/geometry.h"

namespace iso {

//#define DEFER_INVERT

// A BSP tree is built from a collection of polygons by picking a polygon to split along
// That polygon (and all other coplanar polygons) are added directly to that csg_node and the other polygons are added to the front and/or back subtrees

struct csgnode2 {
	// coplanar and convex - stored as parametric values of original triangle3
	struct polygon {
		float3x4				matrix;
		intptr_t				face_id;
		dynamic_array<float2,8>	verts;
		bool					flipped;
		static float min_area;

//		template<typename V> polygon(const uint16 *face, const V &v) : tri(position3(v[_face[0]]), position3(v[_face[1]]), position3(v[_face[2]])), verts(3), face(face), flipped(false) {
		polygon(intptr_t face_id, const triangle3 &tri) : matrix(tri.matrix()), face_id(face_id), verts(3), flipped(false) {
			verts[0]	= float2{0, 0};
			verts[1]	= float2{1, 0};
			verts[2]	= float2{0, 1};
			min_area = min(min_area, tri.area());
		}
		polygon(intptr_t face_id, const parallelogram3 &rect) : matrix(rect.matrix()), face_id(face_id), verts(4), flipped(false) {
			verts[0]	= float2{-1, -1};
			verts[1]	= float2{+1, -1};
			verts[2]	= float2{-1, +1};
			verts[3]	= float2{+1, +1};
		}
		polygon(intptr_t face_id, param(position3) p0, param(position3) p1, param(position3) p2) : matrix(p1 - p0, p2 - p0, normalise(cross(p1 - p0, p2 - p0)), p0), face_id(face_id), verts(3), flipped(false) {
			verts[0]	= float2{0, 0};
			verts[1]	= float2{1, 0};
			verts[2]	= float2{0, 1};
		}
		polygon(const polygon &b, const dynamic_array<float2> &list) : matrix(b.matrix), face_id(b.face_id), verts(list), flipped(b.flipped) {}
		void		flip()	{
			swap(matrix.x, matrix.y);
			for (auto a = verts.begin(), b = verts.end(); a < b--; ++a) {
				float2	t = *a;
				*a	= b->yx;
				*b	= t.yx;
			}
			flipped = !flipped;
		}
	};

	dynamic_array<polygon> polygons;
	uint32		total;
	csgnode2	*child[2];

	csgnode2() { clear(*this); }
};

} // namespace iso

#endif //CSG2_H
