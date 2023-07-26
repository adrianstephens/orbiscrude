#include "triangulate.h"
#include "base/algorithm.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	2D
//-----------------------------------------------------------------------------

struct sort3 {
	bool operator()(param(float4) a, param(float4) b) const {
		return a.z != b.z ? bool(a.z < b.z) : a.x != b.x ? bool(a.x < b.x) : bool(a.y < b.y);
		uint8	eq = bit_mask(a == b), lt = bit_mask(a < b);
		if (eq & 8) {
			if (eq & 1)
				return !!(lt & 2);
			return !!(lt & 1);
		}
		return !!(lt & 8);
//		return a.z == b.z ? bool(a.x < b.x) : bool(a.z < b.z);
	}
};

Triangulator::Tri *Triangulator::DelaunayTestFlip(Tri *tri0, float4 *pts) {
	for (int i = 0; i < 3; i++) {
		Edge	*e0 = &(*tri0)[i];
		Edge	*e1 = e0->flip;
		if (e1) {
			int		v1 = e1->prev->vert;
			if (tri0->c.contains(position2(pts[v1].xy))) {	// not valid in the Delaunay sense
				Tri	*tri1	= e1->tri;
				Edge	*e0n	= e0->next;
				Edge	*e1n	= e1->next;
				int				v0		= e0->prev->vert;

				e0->set_flip(e1n->flip);
				e1->set_flip(e0n->flip);
				e0n->set_flip(e1n);
				e0n->vert	= v1;
				e1n->vert	= v0;

				tri0->c		= circle::through(position2(pts[e0->vert].xy), position2(pts[v1].xy), position2(pts[v0].xy));
				tri1->c		= circle::through(position2(pts[e1->vert].xy), position2(pts[v0].xy), position2(pts[v1].xy));
				return tri1;
			}
		}
	}
	return 0;
}

void Triangulator::Triangulate() {
	uint32		npts	= verts.size32();
	if (npts == 0)
		return;

	float4		*pts	= verts.begin();
	tris.reserve(npts * 2);	// maximum

	position2		p0 = position2(pts[0].xy);
	for (int i = 1; i < npts; ++i)
		pts[i].z = len2(position2(pts[i].xy) - p0);

	sort(pts, pts + npts, sort3());
	npts = unique(pts, pts + npts, [](param(float4) a, param(float4) b) {
		return all(a.xy == b.xy);
	}) - pts;

	position2		p1		= position2(pts[1].xy);
	position2		centre	= p0;
	int				mid		= -1;
	float			minr	= 1e38f;

	for (int k = 2; k < npts; k++) {
		circle	c = circle::through(p0, p1, position2(pts[k].xy));
		if (c.radius2() < minr) {
			mid		= k;
			minr	= c.radius2();
			centre	= c.centre();
		} else if (minr * 4 < pts[k].z) {
			break;
		}
	}

	swap(pts[2], pts[mid]);
	if (cross(p1 - p0, position2(pts[2].xy) - p1) > 0)
		swap(pts[1], pts[2]);

	for (int i = 3; i < npts; i++)
		pts[i].z = len2(position2(pts[i].xy) - centre);
	sort(pts + 3, pts + npts, sort3());

	Tri	&tri	= tris.push_back();
	tri.c		= circle::with_r2(centre, minr);
	tri[0].vert = 0;
	tri[1].vert = 1;
	tri[2].vert = 2;

	Edge	*hull	= &tri[0];

	for (int i = 3; i < npts; i++) {
		// find edges hidden by the next vertex
		Edge	*e0	= hull->prev, *e1 = hull;
		position2		p	= position2(pts[i].xy);
		position2		h0	= position2(pts[e0->vert].xy);
		position2		h1	= position2(pts[e1->vert].xy);

		if (cross(p - h0, h1 - h0) < 0) {
			// scan back for first visible
			do {
				h1	= h0;
				e0	= e0->prev;
				h0	= position2(pts[e0->vert].xy);
			} while (cross(p - h0, h1 - h0) < 0);
			e0	= e0->next;
			h1	= position2(pts[e1->vert].xy);
		} else {
			// scan forward for first visible
			do {
				h0	= h1;
				e1	= e1->next;
				h1	= position2(pts[e1->vert].xy);
			} while (cross(p - h0, h1 - h0) >= 0);
			e0 = e1->prev;
		}
		// scan forward for last visible
		do {
			h0	= h1;
			e1	= e1->next;
			h1	= position2(pts[e1->vert].xy);
		} while (cross(p - h0, h1 - h0) < 0);

		e1 = e1->prev;

		// remove edges e0 to e1 from hull, and make tris
		Edge	*ef		= &tris.end()[0][0];
		Edge	*em		= 0;
		for (Edge *ei = e0; ei != e1; ei = ei->next) {
			Tri	&tri = tris.push_back();
			int		v0	= ei->vert, v2 = ei->next->vert;
			tri[0].vert	= v0;
			tri[1].vert	= i;
			tri[2].vert	= v2;
			tri.c		= circle::through(position2(pts[v0].xy), p, position2(pts[v2].xy));
			tri[0].set_flip(em);
			tri[2].set_flip(ei);
			em			= &tri[1];
		}
		// link the hull back together
		e0->prev->next	= ef; ef->prev	= e0->prev;
		ef->next		= em; em->prev	= ef;
		em->next		= e1; e1->prev	= em;
		hull			= e1;
	}

	// initialise triangle edge links
	for (Tri *i = tris.begin(); i < tris.end(); i++) {
		i->next = 0;
		i->init_links();
	}

	// delaunay swap
	Tri		*redo = (Tri*)1;
	for (Tri *i = tris.begin(); i < tris.end(); i++) {
		if (Tri *j = DelaunayTestFlip(i, pts)) {
			if (!i->next) {
				i->next = redo;
				redo = i;
			}
			if (!j->next) {
				j->next = redo;
				redo = j;
			}
		}
	}

	// re-delaunay swap any changed triangles
	for (int it = 1; redo != (Tri*)1 && it < 20; it++) {
		Tri	*i = redo;
		redo	= (Tri*)1;
		do {
			Tri *n = i->next;
			i->next	= 0;
			if (Tri *j = DelaunayTestFlip(i, pts)) {
				i->next = redo;
				redo	= i;
				if (!j->next) {
					j->next = redo;
					redo	= j;
				}
			}
			i = n;
		} while (i != (Tri*)1);
	}

	// create reverse mapping
	for (int i = 0; i < npts; i++)
		pts[int(pts[i].w)].z = i;
}

bool Triangulator::DeleteTriangle(Tri *t) {
	if (t->next)
		return false;
	for (auto &e : *t) {
		if (Edge *e1 = e.flip)
			e1->flip = 0;
	}
	t->next = (Tri*)1;	// mark as dead
	return true;
}

Triangulator::Edge *Triangulator::FindEdge(int v, Tri *from) {
	for (Tri *t = from; t < tris.end(); t++) {
		for (auto &e : *t) {
			if (e.vert == v)
				return &e;
		}
	}
	return nullptr;
}

Triangulator::Tri *Triangulator::FindTriangle(int v, Tri *from) {
	if (Edge *e = FindEdge(v, from))
		return e->tri;
	return 0;
}

bool Triangulator::DeletePoint(int i) {
	int	v = int(verts[i].z);
	for (Tri *t = tris.begin(); t = FindTriangle(v, t); t++)
		DeleteTriangle(t);
	return true;
}

bool Triangulator::MakeEdge(int a, int b) {
	if (a == b)
		return false;

	int	va = int(verts[a].z);
	int	vb = int(verts[b].z);

	Edge	*ea = FindEdge(va, tris.begin());
	if (!ea)
		return false;

	position2	pa	= position2(verts[va].xy);
	position2	pb	= position2(verts[vb].xy);
	vector2		d	= pb - pa;

	Edge::viterator e(ea);
	auto	ep	= e;
	if (cross(d, position2(verts[e->end()].xy) - pa) < 0) {
		do {
			ep = e;
			--e;
		} while (e && cross(d, position2(verts[e->end()].xy) - pa) < 0);
		if (!e)
			e = ep->prev;
	} else {
		do {
			ep = e;
			++e;
		} while (e && cross(d, position2(verts[e->end()].xy) - pa) >= 0);
		e = ep;
	}

	Edge	*mono = 0;

	while (e->end() != vb) {
		DeleteTriangle(e->tri);

		e = e->next;
		if (cross(d, position2(verts[e->end()].xy) - pa) < 0) {
			e->next->prev = mono;
			mono		= e->next;
		} else {
			e			= e->next;
		}

		e = e->flip;
	}

//	TriangulateMonoChain(mono);

	return true;
}

void Triangulator::TriangulateMonoChain(Edge *e) {
	Edge	*start = e, *end = e;
	e = e->next;
	while (Edge	*n = e->next) {
		position2	p	= position2(verts[end->vert].xy);
		position2	pa	= position2(verts[e->vert].xy), pb = position2(verts[e->end()].xy);
		if (cross(pa - p, pb - p) < 0) {
			Tri	*tri = e->tri;
			tri->next		= 0;	// resurrect
			e->next			= e->prev->prev;
			e->prev->vert	= end->vert;
			end = e->next;
		}

		e = n;
	}
}

int Triangulator::GetTriangles(int *output) const {
	int	n = 0;
	for (const Tri *i = tris.begin(); i < tris.end(); i++) {
		//if (!i->next) {
			*output++ = OriginalIndex((*i)[0].vert);
			*output++ = OriginalIndex((*i)[1].vert);
			*output++ = OriginalIndex((*i)[2].vert);
			n++;
		//}
	}
	return n;
}

int iso::Triangulate(position2 *pts, int npts, int *output) {
	Triangulator	tri;
	tri.AddVerts(pts, pts + npts);
	tri.Triangulate();
	return tri.GetTriangles(output);
}

float iso::closest_point(const point_cloud<range<position2*>> &a, const point_cloud<range<position2*>> &b, int &va, int &vb) {
	Triangulator	t;
	t.AddVerts(a.points());
	t.AddVerts(b.points());
	t.Triangulate();

	int		na	= a.points().size32();
	int		i0	= 0, i1 = na;
	float	best_dist2	= maximum;

	for (auto &i : t.tris) {
		float4		v1	= t.verts[i[2].vert];

		for (int j = 0; j < 3; j++) {
			float4	v0	= v1;
			v1			= t.verts[i[j].vert];
			if ((int(v0.w) < na) ^ (int(v1.w) < na)) {
				float	dist2 = len2(v0.xy - v1.xy);
				if (dist2 < best_dist2) {
					best_dist2	= dist2;
					i0			= int(v0.w);
					i1			= int(v1.w);
				}
			}
		}
	}

	if (i0 < na) {
		va	= i0;
		vb	= i1 - na;
	} else {
		va	= i1;
		vb	= i0 - na;
	}
	return sqrt(best_dist2);
}

//-----------------------------------------------------------------------------
//	3D
//-----------------------------------------------------------------------------

struct Triangulator3D {
	struct Tri {
		int		id, keep;
		int		a, b, c;
		int		ab, bc, ac;		// adjacent edges index to neighbouring triangle.
		float3	normal;			// visible normal to triangular facet.

		Tri(int _id, int _keep, int _a, int _b, int _c, param(float3) _normal) : id(_id), keep(_keep),a(_a), b(_b), c(_c), ab(-1), bc(-1), ac(-1), normal(_normal) {}
		bool	SetEdge(int A, int B, int eid) {
			if ((a == A && b == B) || (a == B && b == A))
				ab = eid;
			else if ((a == A && c == B) || (a == B && c == A))
				ac = eid;
			else if ((b == A && c == B) || (b == B && c == A))
				bc = eid;
			else
				return false;
			return true;
		}
		void	SetEdge(int i, int eid) {
			(i == 0 ? ab : i == 1 ? bc : ac) = eid;
		}
	};
	struct Snork {
		int id, a, b;
		Snork() : id(-1), a(0), b(0) {}
		Snork(int _id, int _a, int _b) : id(_id), a(_a), b(_b) {}
		friend bool operator<(const Snork &a, const Snork &b) { return a.a == b.a ? a.b < b.b : a.a < b.a; };
	};
	dynamic_array<Tri>		hull;

	Triangulator3D(float4 *pts, int nump) {
		// sort into descending order (for use in corner responce ranking)
		sort(pts, pts + nump, [](param(float4) a, param(float4) b) {
			return	a.z != b.z ? bool(a.z < b.z)
				:	a.x != b.x ? bool(a.x < b.x)
				:	bool(a.y < b.y);
		});
		init(pts, nump);
	}

	bool	init(float4 *pts, int nump);
	void	add_coplanar(float4 *pts, int id);

	bool	GetDelaunay(dynamic_array<Tri> &hulk);
	bool	GetHull(dynamic_array<Tri> &hulk);
};

// To create a Delaunay triangulation from a 2D point set:
// make the 'z' coordinate = (x*x + y*y),
// find the convex hull in float4 of a point cloud using a sweep-hull algorithm called the Newton Apple Wrapper
// discard the facets that are not downward facing.

bool Triangulator3D::GetDelaunay(dynamic_array<Tri> &hulk) {
	int		numh	= hull.size32();
	int		cnt		= 0;

	dynamic_array<int>	taken(numh, -1);

	for (int t = 0; t < numh; t++) {	// create an index from old tri-id to new tri-id.
		if (hull[t].keep > 0)			// point index remains unchanged.
			taken[t] = cnt++;
	}

	for (int t = 0; t < numh; t++) {	// create an index from old tri-id to new tri-id.
		if (hull[t].keep > 0) {			// point index remains unchanged.
			Tri T	= hull[t];

			T.id	= taken[t];
			T.ab	= taken[T.ab];
			T.bc	= taken[T.bc];
			T.ac	= taken[T.ac];

			if (T.ab < 0 || T.bc < 0 || T.ac < 0)
				return false;

			// look at the normal to the triangle
			if (T.normal.z < 0)
				hulk.push_back(T);
		}
	}
	return true;
}

// Find the convex hull in R3 of a point cloud using a sweep-hull algorithm called the Newton Apple Wrapper
bool Triangulator3D::GetHull(dynamic_array<Tri> &hulk) {
	int		numh	= hull.size32();
	int		cnt		= 0;

	dynamic_array<int>	taken(numh, -1);

	for (int t = 0; t < numh; t++) {	// create an index from old tri-id to new tri-id.
		if (hull[t].keep > 0)			// point index remains unchanged.
			taken[t] = cnt++;
	}

	for (int t = 0; t < numh; t++) {	// create an index from old tri-id to new tri-id.
		if (hull[t].keep > 0) {			// point index remains unchanged.
			Tri T	= hull[t];
			T.id	= taken[t];

			T.id	= taken[t];
			T.ab	= taken[T.ab];
			T.bc	= taken[T.bc];
			T.ac	= taken[T.ac];

			if (T.ab < 0 || T.bc < 0 || T.ac < 0)
				return false;

			hulk.push_back(T);
		}
	}
	return true;
}

bool Triangulator3D::init(float4 *pts, int nump) {
	float3	p0		= pts[0].xyz;
	float3	p1		= pts[1].xyz;
	float3	p2		= pts[2].xyz;
	float3	normal	= cross(p1 - p0, p2 - p0);

	// check for colinearity
	if (len2(normal) == 0)
		return false;

	Tri		&T1		= hull.emplace_back(0, 1, 0, 1, 2, normal);
	Tri		&T2		= hull.emplace_back(1, 1, 0, 1, 2, -normal);
	T1.ab = T1.ac = T1.bc = 1;	// adjacent facet id number
	T2.ab = T2.ac = T2.bc = 0;	// adjacent facet id number

	dynamic_array<int> xlist;
	float3	M		= p0 + p1 + p2;

	for (int p = 3; p < nump; p++) {
		float3 pt	= pts[p].xyz;
		M	+= pt;
		float3	m	= M / (p + 1);	// centroid

		// find the first visible plane.
		int		numh = hull.size32();
		int		hvis = -1;
		xlist.clear();

		for (int h = numh - 1; h >= 0; h--) {
			Tri &t = hull[h];
			if (dot(pt - pts[t.a].xyz, t.normal) > 0) {
				hvis			= h;
				hull[h].keep	= 0;
				xlist.push_back(hvis);
				break;
			}
		}

		if (hvis < 0) {
			add_coplanar(pts, p);
			continue;
		}

		// new triangular facets are formed from neighbouring invisible planes.
		for (int x = 0; x < xlist.size32(); x++) {
			int		xid		= xlist[x];
			int		A		= hull[xid].a;
			int		B		= hull[xid].b;
			int		C		= hull[xid].c;

			// first side of the struck out triangle

			int		ab		= hull[xid].ab;		// facet adjacent to line ab
			Tri		&tAB	= hull[ab];

			// point on next triangle
			if (dot(pt - pts[tAB.a].xyz, tAB.normal) > 0) { // add to xlist.
				if (tAB.keep == 1) {
					tAB.keep = 0;
					xlist.push_back(ab);
				}
			} else {
				float3	normal = cross(pts[p].xyz - pts[A].xyz, pts[p].xyz - pts[B].xyz);
				if (dot(m - pt, normal) > 0)
					normal = -normal;

				// update the touching triangle tAB
				int	nid		= hull.size32();
				if (!tAB.SetEdge(A, B, nid))
					return false;				//numeric stability issue

				Tri	&Tnew	= hull.emplace_back(nid, 2, p, A, B, normal);
				Tnew.bc		= ab;
			}

			// second side of the struck out triangle

			int		ac		= hull[xid].ac;		// facet adjacent to line ac
			Tri		&tAC	= hull[ac];

			// point on next triangle
			if (dot(pt - pts[tAC.a].xyz, tAC.normal) > 0) { // add to xlist.
				if (tAC.keep == 1) {
					tAC.keep = 0;
					xlist.push_back(ac);
				}
			} else {
				float3 normal = cross(pts[p].xyz - pts[A].xyz, pts[p].xyz - pts[C].xyz);
				if (dot(m - pt, normal) > 0)
					normal = -normal;

				// update the touching triangle tAC
				int	nid		= hull.size32();
				if (!tAC.SetEdge(A, C, nid))
					return false;				//numeric stability issue

				Tri	&Tnew	= hull.emplace_back(nid, 2, p, A, C, normal);
				Tnew.bc = ac;
			}

			// third side of the struck out triangle

			int		bc		= hull[xid].bc;		// facet adjacent to line ac
			Tri		&tBC	= hull[bc];

			// point on next triangle
			if (dot(pt - pts[tBC.a].xyz, tBC.normal) > 0) { // add to xlist.
				if (tBC.keep == 1) {
					tBC.keep = 0;
					xlist.push_back(bc);
				}
			} else {
				float3	normal = cross(pts[p].xyz - pts[B].xyz, pts[p].xyz - pts[C].xyz);
				if (dot(m - pt, normal) > 0)
					normal = -normal;

				// update the touching triangle tBC
				int	nid		= hull.size32();
				if (!tBC.SetEdge(B, C, nid))
					return false;				//numeric stability issue

				Tri	&Tnew	= hull.emplace_back(nid, 2, p, B, C, normal);
				Tnew.bc = bc;
			}
		}

		// patch up the new triangles in hull.

		dynamic_array<Snork> norts;
		for (int q = hull.size32() - 1; q >= numh; q--) {
			if (hull[q].keep > 1) {
				norts.emplace_back(q, hull[q].b, 1);
				norts.emplace_back(q, hull[q].c, 0);
				hull[q].keep = 1;
			}
		}

		sort(norts);

		int nums = norts.size32();
		if (nums >= 2) {
			for (int s = 0; s < nums - 1; s++) {
				if (norts[s].a == norts[s + 1].a) {
					// link triangle sides.
					hull[norts[s].id].SetEdge(norts[s].b ? 0 : 2, norts[s + 1].id);
					hull[norts[s + 1].id].SetEdge(norts[s + 1].b ? 0 : 2, norts[s].id);
				}
			}
		}
	}
	return true;
}

// Add a point coplanar to the existing planar hull in 3D
// this is not an efficient routine and should only be used to add in duff (coplanar) pts at the start
// it should not be called in doing a Delaunay triangulation of a 2D set raised into 3D.

// cross product relative sign test
static float cross_test(float4 *pts, int A, int B, int C, int X, float3 &normal) {
	float3	AB	= pts[B].xyz - pts[A].xyz;
	float3	AC	= pts[C].xyz - pts[A].xyz;
	float3	AX	= pts[X].xyz - pts[A].xyz;

	normal = cross(AB, AX);
	return dot(cross(AB, AC), normal);
}

void Triangulator3D::add_coplanar(float4 *pts, int id) {
	int		numh = hull.size32();

	for (int k = 0; k < numh; k++) {
		//find visible edges. from external edges.
		int		A		= hull[k].a;
		int		B		= hull[k].b;
		int		C		= hull[k].c;
		float3	normal;

		if (C == hull[hull[k].ab].c) { // -> ab is an external edge.
			// test this edge for visibility from new point pts[id].

			if (cross_test(pts, A, B, C, id, normal) < 0) { // visible edge facet, create 2 new hull plates.
				int	nid		= hull.size32();
				Tri &up		= hull.emplace_back(nid + 0, 2, id, A, C, normal);
				Tri &down	= hull.emplace_back(nid + 1, 2, id, A, B, -normal);

				if (dot(hull[k].normal, normal) > 0) {
					up.bc				= k;
					down.bc				= hull[k].ab;
					hull[k].ab			= up.id;
					hull[down.bc].ab	= down.id;
				} else {
					down.bc			= k;
					up.bc				= hull[k].ab;
					hull[k].ab			= down.id;
					hull[up.bc].ab		= up.id;
				}
			}
		}

		if (A == hull[hull[k].bc].a) {	// bc is an external edge.
			// test this edge for visibility from new point pts[id].
			if (cross_test(pts, B, C, A, id, normal) < 0) { // visible edge facet, create 2 new hull plates.
				int	nid		= hull.size32();
				Tri &up		= hull.emplace_back(nid + 0, 2, id, B, C, normal);
				Tri &down	= hull.emplace_back(nid + 1, 2, id, B, C, -normal);

				if (dot(hull[k].normal, normal) > 0) {
					up.bc				= k;
					down.bc				= hull[k].bc;
					hull[k].bc			= up.id;
					hull[down.bc].bc	= down.id;
				} else {
					down.bc				= k;
					up.bc				= hull[k].bc;
					hull[k].bc			= down.id;
					hull[up.bc].bc		= up.id;
				}
			}
		}

		if (B == hull[hull[k].ac].b) {	// ac is an external edge.
			// test this edge for visibility from new point pts[id].
			if (cross_test(pts, A, C, B, id, normal) < 0) { // visible edge facet, create 2 new hull plates.
				int	nid		= hull.size32();
				Tri &up		= hull.emplace_back(nid + 0, 2, id, A, C, normal);
				Tri &down	= hull.emplace_back(nid + 1, 2, id, A, C, -normal);

				if (dot(hull[k].normal, normal) > 0) {
					up.bc				= k;
					down.bc				= hull[k].ac;
					hull[k].ac			= up.id;
					hull[down.bc].ac	= down.id;
				} else {
					down.bc				= k;
					up.bc				= hull[k].ac;
					hull[k].ac			= down.id;
					hull[up.bc].ac		= up.id;
				}
			}
		}
	}

	// fix up the non asigned hull adjacencies (correctly).
	dynamic_array<Snork>	norts;
	for (int q = hull.size32() - 1; q >= numh; q--) {
		if (hull[q].keep > 1) {
			norts.emplace_back(q, hull[q].b, 1);
			norts.emplace_back(q, hull[q].c, 0);
			hull[q].keep = 1;
		}
	}

	sort(norts);
	int		nums = norts.size32();
	norts.emplace_back(-1, -1, -1);
	norts.emplace_back(-2, -2, -2);

	if (nums >= 2) {
		for (int s = 0; s < nums - 1; s++) {
			if (norts[s].a == norts[s + 1].a) {
				// link triangle sides.
				if (norts[s].a != norts[s + 2].a) {
					// edge of figure case
					hull[norts[s].id].SetEdge(norts[s].b ? 0 : 2, norts[s + 1].id);
					hull[norts[s + 1].id].SetEdge(norts[s + 1].b ? 0 : 2, norts[s].id);
					s++;

				} else {
					// internal figure boundary 4 junction case.
					int		s1	= s + 1, s2 = s + 2, s3 = s + 3;
					int		id	= norts[s].id;
					int		id1 = norts[s1].id;
					int		id2 = norts[s2].id;
					int		id3 = norts[s3].id;

					// check normal directions of id and id1..3
					if (dot(hull[id].normal, hull[id1].normal) <= 0) {
						if (dot(hull[id].normal, hull[id2].normal) > 0) {
							swap(id1, id2);
							swap(s1, s2);
						} else if (dot(hull[id].normal, hull[id3].normal) > 0) {
							swap(id1, id3);
							swap(s1, s3);
						}
					}

					hull[id ].SetEdge(norts[s ].b ? 0 : 2, id1);
					hull[id1].SetEdge(norts[s1].b ? 0 : 2, id );

					// use s2 and s3
					hull[id2].SetEdge(norts[s2].b ? 0 : 2, id3);
					hull[id3].SetEdge(norts[s3].b ? 0 : 2, id2);

					s += 3;
				}
			}
		}
	}
}
