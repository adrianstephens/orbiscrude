#ifndef TRIANGULATE_H
#define TRIANGULATE_H

#include "base/array.h"
#include "maths/polygon.h"
#include "comp_geom.h"

namespace iso {

struct Triangulator {
	struct Tri;

	struct Edge : cg::tri_edge<Tri, Edge> {
		int		vert;
		int		end()	const	{ return next->vert; }
		int		adj()	const	{ return prev->vert; }
		Edge() {}
	};

	struct Tri : cg::tri_face<Tri, Edge>, aligner<16> {
		circle	c;
		Tri		*next;
	};

	dynamic_array<float4>	verts;
	dynamic_array<Tri>		tris;

	static Tri *DelaunayTestFlip(Tri *tri0, float4 *pts);

	Triangulator() {}
	template<typename I> Triangulator(I i0, I i1) {
		AddVerts(i0, i1);
		Triangulate();
	}
	template<typename C> Triangulator(const C &c) {
		AddVerts(c);
		Triangulate();
	}

	template<typename I> void AddVerts(I i0, I i1) {
		int		i	= verts.size32();
		float4	*d	= verts.append(distance(i0, i1));
		while (i0 != i1)
			*d++ = concat(float2(*i0++), zero, i++);
	}
	template<typename C> void AddVerts(const C &c) {
		AddVerts(begin(c), end(c));
	}

	void	Triangulate();
	void	TriangulateMonoChain(Edge *e);
	Edge*	FindEdge(int v, Tri *from);
	Tri*	FindTriangle(int v, Tri *from);

	int		InternalIndex(int a)		const	{ return verts[a].z;	}
	int		OriginalIndex(int a)		const	{ return verts[a].w;	}
	int		NumTriangles()				const	{ return tris.size32(); }
	int		GetTriangles(int *output)	const;

	bool	DeleteTriangle(Tri *t);
	bool	DeletePoint(int i);
	bool	MakeEdge(int a, int b);
};

int Triangulate(iso::position2 *pts, int npts, int *output);

float closest_point(const point_cloud<range<position2*>> &a, const point_cloud<range<position2*>> &b, int &va, int &vb);
float closest_point(const convex_polygon<range<position2*>> &a, const convex_polygon<range<position2*>> &b, int &va, int &vb);

}

#endif	//TRIANGULATE_H