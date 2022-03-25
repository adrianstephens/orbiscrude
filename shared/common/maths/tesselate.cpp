#include "tesselate.h"
#include "base/algorithm.h"
#include "base/strings.h"
#include "geometry.h"
#include "comp_geom.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Sweep
//-----------------------------------------------------------------------------

struct Sweep {

	struct Vertex : position2 {
		Vertex() {}
		Vertex(param(position2) p) : position2(p) {}
		void	set(param(position2) p) { *this = p; }
		Vertex	trans()			const { return position2(v.yx); }

		friend bool operator==(const Vertex& u, const Vertex& v) { return all(u.v == v.v); }
		friend bool operator<=(const Vertex& u, const Vertex& v) { return u.v.x < v.v.x || (u.v.x == v.v.x && u.v.y <= v.v.y); }
	};

	// Given three vertices u,v,w such that u <= v && v <= w, evaluates the t-coord of the edge uw at the v.x-coord of the vertex v.
	// Returns v.t - (uw)(v.v.x), ie. the signed distance from uw to v. If uw is vertical (and thus passes thru v), the result is zero.
	static float	EdgeEval(const Vertex &u, const Vertex &v, const Vertex &w) {
		ISO_ASSERT(u <= v && v <= w);
#if 1
		float2	d	= w - u;
		if (d.x == 0)
			return 0;	// vertical line

		float2	dl	= v - u;
		float2	dr	= w - v;
		return dl.x < dr.x
			?  dl.y - d.y * (dl.x / d.x)
			: -dr.y + d.y * (dr.x / d.x);
#else
		float	dl = v.v.x - u.v.x;
		float	dr = w.v.x - v.v.x;
		if (dl + dr > 0) {
			return dl < dr
				? (v.v.y - u.v.y) + (u.v.y - w.v.y) * (dl / (dl + dr))
				: (v.v.y - w.v.y) + (w.v.y - u.v.y) * (dr / (dl + dr));
		}
		return 0;	// vertical line
#endif
	}

	// Returns a number whose sign matches EdgeEval(u,v,w) but which is cheaper to evaluate.
	// Returns > 0, == 0 , or < 0 as v is above, on, or below the edge uw
	static float	EdgeSign(const Vertex &u, const Vertex &v, const Vertex &w) {
		ISO_ASSERT(u <= v && v <= w);
#if 1
		return cross(w - v, v - u);
#else
		float	dl = v.v.x - u.v.x;
		float	dr = w.v.x - v.v.x;
		if (dl + dr > 0)
			return (v.v.y - w.v.y) * dl + (v.v.y - u.v.y) * dr;
		return 0;	// vertical line
#endif
	}

	// For almost-degenerate situations, the results are not reliable
	static bool	ccw(const Vertex &u, const Vertex &v, const Vertex &w) {
#if 1
		return signed_area(u, v, w) >= zero;
#else
		return	u.v.x * (v.v.y - w.v.y)
			+	v.v.x * (w.v.y - u.v.y)
			+	w.v.x * (u.v.y - v.v.y)
			>= 0;
#endif
	}

	static float intersect0(Vertex o1, Vertex d1, Vertex o2, Vertex d2) {
		if (!(o1 <= d1))
			swap(o1, d1);
		if (!(o2 <= d2))
			swap(o2, d2);
		if (!(o1 <= o2)) {
			swap(o1, o2);
			swap(d1, d2);
		}

		if (!(o2 <= d1)) {
			// Technically, no intersection -- do our best
			return (o2.v.x + d1.v.x) / 2;

		} else if (d1 <= d2) {
			// Interpolate between o2 and d1
			float	z1 = EdgeEval(o1, o2, d1);
			float	z2 = EdgeEval(o2, d1, d2);
			if (z1 + z2 < 0) {
				z1 = -z1;
				z2 = -z2;
			}
			return stable_interpolate(z1, o2.v.x, z2, d1.v.x);

		} else {
			// Interpolate between o2 and d2
			float	z1 = EdgeSign(o1, o2, d1);
			float	z2 = -EdgeSign(o1, d2, d1);
			if (z1 + z2 < 0) {
				z1 = -z1;
				z2 = -z2;
			}
			return stable_interpolate(z1, o2.v.x, z2, d2.v.x);
		}
	}

	static Vertex intersect(Vertex o1, Vertex d1, Vertex o2, Vertex d2) {
		return position2(
			intersect0(o1, d1, o2, d2),
			intersect0(o1.trans(), d1.trans(), o2.trans(), d2.trans())
		);
	}

	struct Region;

	struct EdgeState {
		Region*	region	= nullptr;	// a region with this upper edge
		int		winding	= 0;		// change in winding number when crossing from the right face to the left face
	};
	struct FaceState {
		bool	inside	= false;	// this face is in the polygon interior
	};

	typedef cg::mesh<Vertex, EdgeState, FaceState>	mesh_t;
	typedef mesh_t::vertex	MeshVertex;
	typedef mesh_t::face	MeshFace;
	typedef mesh_t::edge	Edge;

	struct Region : e_link<Region> {
		Edge		*up;			// upper edge, directed right to left
		int			winding;		// used to determine which regions are inside the polygon
//		bool		inside;			// is this region inside the polygon?
		bool		dirty;			// marks regions where the upper or lower edge has changed, but we haven't checked whether they intersect yet
		bool		fix_up;			// marks temporary edges introduced when we process a "right vertex" (one without any edges leaving to the right)

		Region(Edge *up) : up(up), winding(0),/* inside(false),*/ dirty(false), fix_up(false) {
			up->region = this;
		}
		~Region() {
			up->region = 0;
		}

		Region	*below()	{ return prev; }
		Region	*above()	{ return next; }

		// Replace an upper edge which needs fixing (see ConnectRightVertex).
		void	FixUpperEdge(Edge *e) {
			ISO_ASSERT(fix_up);
			mesh_t::delete_edge(up);
			fix_up		= false;
			up			= e;
			e->region	= this;
		}

		Region *TopLeft() {
			// Find the region above the uppermost edge with the same origin
			Region		*reg	= this;
			MeshVertex	*v		= up->v;
			do {
				reg = reg->above();
			} while (reg->up->v == v);

			// If the edge above was a temporary edge introduced by ConnectRightVertex, now is the time to fix it
			if (reg->fix_up) {
				Edge *e = mesh_t::connect_edges(reg->below()->up->flip(), reg->up->fnext);
				reg->FixUpperEdge(e);
				reg = reg->above();
			}
			return reg;
		}

		Region *TopRight() {
			// Find the region above the uppermost edge with the same destination
			Region		*reg	= this;
			MeshVertex	*v		= up->v1();
			do
				reg = reg->above();
			while (reg->up->v1() == v);
			return reg;
		}

		void ComputeWinding(WINDING_RULE rule) {
			winding = above()->winding + up->winding;
			//inside	= test_winding(rule, winding);
		}
	};

	struct Regions : e_list<Region> {
		static bool compare_edges(MeshVertex *event, Edge *e1, Edge *e2) {
			if (e1->v1() == event) {
				if (e2->v1() == event) {
					// Two edges right of the sweep line which meet at the sweep event - sort them by slope
					return *static_cast<Vertex*>(e1->v0()) <= *static_cast<Vertex*>(e2->v0())
						? EdgeSign(*e2->v1(), *e1->v0(), *e2->v0()) <= 0
						: EdgeSign(*e1->v1(), *e2->v0(), *e1->v0()) >= 0;
				}
				return EdgeSign(*e2->v1(), *event, *e2->v0()) <= 0;

			}
			if (e2->v1() == event)
				return EdgeSign(*e1->v1(), *event, *e1->v0()) >= 0;

			// General case - compute signed distance *from* e1, e2 to event
			return EdgeEval(*e1->v1(), *event, *e1->v0()) >= EdgeEval(*e2->v1(), *event, *e2->v0());
		}
		Region	*search(MeshVertex *event, Edge *edge) {
			auto i = begin();
			while (!compare_edges(event, edge, i->up))
				++i;
			return i.get();
		}
		Region	*insert_before(MeshVertex *event, Region *node, Edge *e) {
			do
				node = node->prev;
			while (!compare_edges(event, node->up, e));
			Region	*r = new Region(e);
			node->insert_after(r);
			return r;
		}
	};

	struct MeshVertexRef {
		MeshVertex	*v;
		MeshVertexRef(MeshVertex *v) : v(v) {}
		operator MeshVertex*() const { return v; }
		friend bool operator<(const MeshVertexRef &a, const MeshVertexRef &b) { return *a.v <= *b.v; }
	};

	static Edge*	mesh_splitedge(Edge *e) {
		Edge	*enew			= mesh_t::split_edge(e);
		enew->winding			= e->winding;
		enew->flip()->winding	= e->flip()->winding;
		return enew;
	}

	static void		AddWinding(Edge *e0, const Edge *e1) {
		e0->winding				+= e1->winding;
		e0->flip()->winding		+= e1->flip()->winding;
	}

	mesh_t			mesh;
	WINDING_RULE	rule;

private:
	Regions			dict;
	priority_queue<dynamic_array<MeshVertexRef>>	pq;

	Edge*			FinishLeftRegions(Region *rbegin, Region *rend);
	bool			CheckForLeftSplice(Region *rup);
	bool			CheckForRightSplice(Region *rup);
	void			SweepEvent(MeshVertex *event);
	Region*			AddRightEdges(Region *rup, Edge *ebegin, Edge *eend, Edge *eTopLeft, MeshVertex *event);
	Region*			CheckForIntersect(Region *rup, MeshVertex *event);
	void			WalkDirtyRegions(Region *r, MeshVertex *event);
	void			TessellateMonoRegion(MeshFace *face);
	void			DoSweep();

public:
	MeshFace*		AddContour(const position2 *p, int n);
	void			ComputeInterior();
	void			TessellateInterior();

	Sweep(WINDING_RULE rule) : rule(rule) {}
};

// Check the upper and lower edge of rup to make sure that the eup->v is above elo or elo->v is below eup (depending on which origin is leftmost)
// The main purpose is to splice right-going edges with the same dest vertex and nearly identical slopes, however the splicing can also help us to recover from numerical errors
// e.g. suppose at one point we checked eup and elo, and decided that eup->v is barely above elo, then later we split elo into two edges; this can change the result of our test so that now eup->v is incident to elo, or barely below it
// We must correct this condition to maintain the dictionary invariants

// One possibility is to check these edges for intersection again (ie. CheckForIntersect); this is what we do if possible
// However CheckForIntersect requires that tess->event lies between eup and elo, so that it has something to fall back on when the intersection calculation gives us an unusable answer
// So for those cases this routine fixes the problem by just splicing the offending vertex into the other edge

bool Sweep::CheckForRightSplice(Region *rup) {
	Region	*rlo = rup->below();
	Edge	*eup = rup->up;
	Edge	*elo = rlo->up;

	if (*eup->v <= *elo->v) {
		if (EdgeSign(*elo->v1(), *eup->v, *elo->v) > 0)
			return false;

		// eup->v appears to be below elo
		if (!(*eup->v == *elo->v)) {
			// Splice eup->v into elo
			mesh_splitedge(elo->flip());
			mesh_t::splice_edges(eup, elo->flip()->fnext);
			rup->dirty = rlo->dirty = true;

		} else if (eup->v != elo->v) {
			// merge the two vertices, discarding eup->v
			auto i = pq.find(eup->v, [](const MeshVertex *a, const MeshVertex *b) {
				return a == b;
			});
			ISO_ASSERT(i != pq.container().end());
			pq.remove(i);
			mesh_t::splice_edges(elo->flip()->fnext, eup);
		}
	} else {
		if (EdgeSign(*eup->v1(), *elo->v, *eup->v) < 0)
			return false;

		// elo->v appears to be above eup, so splice elo->v into eup
		rup->dirty = true;
		if (rup->above() != dict.end().get())
			rup->above()->dirty = true;

		mesh_splitedge(eup->flip());
		mesh_t::splice_edges(elo->flip()->fnext, eup);
	}
	return true;
}

// Check the upper and lower edge of rup to make sure that the eup->v1 is above elo, or elo->v1 is below eup (depending on which destination is rightmost)
// Theoretically, this should always be true, but splitting an edge into two pieces can change the results of previous tests
// We must correct this condition to maintain the dictionary invariants; we just splice the offending vertex into the other edge

bool Sweep::CheckForLeftSplice(Region *rup) {
	Region	*rlo = rup->below();
	Edge	*eup = rup->up;
	Edge	*elo = rlo->up;

	ISO_ASSERT(!(*eup->v1() == *elo->v1()));

	bool	inside	= test_winding(rule, rup->winding);

	if (*eup->v1() <= *elo->v1()) {
		if (EdgeSign(*eup->v1(), *elo->v1(), *eup->v) < 0)
			return false;

		// elo->v1 is above eup, so splice elo->v1 into eup
		rup->dirty = true;
		if (rup->above() != dict.end().get())
			rup->above()->dirty = true;
		Edge	*e	= mesh_splitedge(eup);
		mesh_t::splice_edges(elo->flip(), e);
		e->f->inside = inside;

	} else {
		if (EdgeSign(*elo->v1(), *eup->v1(), *elo->v) > 0)
			return false;

		// eup->v1 is below elo, so splice eup->v1 into elo
		rup->dirty = rlo->dirty = true;
		Edge	*e = mesh_splitedge(elo);
		mesh_t::splice_edges(eup->fnext, elo->flip());
		e->flip()->f->inside = inside;
	}
	return true;
}

// Starting at rbegin->up, we walk down deleting all regions where both edges have the same origin
// At the same time we copy the "inside" flag from the active region to the face, since at this point each face will belong to at most one region
// The walk stops at the region above rend; if rend is NULL we walk as far as possible
// At the same time we relink the mesh if necessary, so that the ordering of edges around v0 is the same as in the dictionary

Sweep::Edge *Sweep::FinishLeftRegions(Region *rbegin, Region *rend) {
	Region	*rprev	= rbegin;
	Edge	*eprev	= rbegin->up;

	while (rbegin != rend) {
		rprev->fix_up	= false;
		Region	*reg	= rprev->below();
		Edge	*e		= reg->up;
		if (e->v != eprev->v) {
			if (!reg->fix_up) {
				MeshFace	*f = rprev->up->f;
				f->inside	= test_winding(rule, rprev->winding);
				f->e		= rprev->up;
				delete rprev;
				break;
			}
			// If the edge below was a temporary edge introduced by ConnectRightVertex fix it now
			e = mesh_t::connect_edges(eprev->f0_prev(), e->flip());
			reg->FixUpperEdge(e);
		}

		// relink edges so that eprev->Onext == e
		if (eprev->vnext != e) {
			mesh_t::splice_edges(e->v0_prev(), e);
			mesh_t::splice_edges(eprev, e);
		}

		MeshFace	*f = rprev->up->f;
		f->inside	= test_winding(rule, rprev->winding);
		f->e		= rprev->up;
		delete rprev;
		eprev	= reg->up;
		rprev	= reg;
	}
	return eprev;
}

// Insert right-going edges into the edge dictionary, and update winding numbers and mesh connectivity appropriately
// All right-going edges share a common origin. Edges are inserted CCW starting at ebegin; the last edge inserted is eend->Oprev
// If vOrg has any left-going edges already processed, then eTopLeft must be the edge such that an imaginary upward vertical segment from vOrg would be contained between eTopLeft->Oprev and eTopLeft; otherwise eTopLeft should be NULL
// Returns bottom affected region

Sweep::Region *Sweep::AddRightEdges(Region *rup, Edge *ebegin, Edge *eend, Edge *eTopLeft, MeshVertex *event) {
	// Insert the new right-going edges in the dictionary
	Edge *e = ebegin;
	do {
		ISO_ASSERT(*e->v0() <= *e->v1());
		dict.insert_before(event, rup, e->flip());
		e = e->vnext;
	} while (e != eend);

	// Walk *all* right-going edges from e->v0(), in the dictionary order, updating the winding numbers of each region and re-linking the mesh edges to match the dictionary ordering (if necessary)
	Region	*reg;
	Region	*rprev	= rup;
	Edge	*eprev	= eTopLeft ? eTopLeft : rup->below()->up->f1_prev();

	for (bool first_time = true; ;first_time = false, rprev = reg, eprev = e) {
		reg = rprev->below();
		e	= reg->up->flip();
		if (e->v != eprev->v)
			break;

		if (e->vnext != eprev) {
			// Unlink e from its current position, and relink below eprev
			mesh_t::splice_edges(e->v0_prev(), e);
			mesh_t::splice_edges(eprev->v0_prev(), e);
		}
		// Compute the winding number and "inside" flag for the new regions
		reg->winding	= rprev->winding - e->winding;
//		reg->inside		= test_winding(rule, reg->winding);

		// Check for two outgoing edges with same slope -- process these before any intersection tests
		rprev->dirty	= true;
		if (!first_time && CheckForRightSplice(rprev)) {
			AddWinding(e, eprev);
			delete rprev;
			mesh_t::delete_edge(eprev);
		}
	}
	rprev->dirty = true;
	ISO_ASSERT(rprev->winding - e->winding == reg->winding);
	return rprev;
}

// Check the upper and lower edges of the given region to see if they intersect
// If so, create the intersection and add it to the data structures
// Returns bottom newly dirty region from AddRightEdges

Sweep::Region *Sweep::CheckForIntersect(Region *rup, MeshVertex *event) {
	Region		*rlo	= rup->below();
	Edge		*eup	= rup->up;
	Edge		*elo	= rlo->up;

	if (eup->v == elo->v)
		return 0;	// right endpoints are the same

	Vertex		orgUp	= *eup->v;
	Vertex		orgLo	= *elo->v;
	Vertex		dstUp	= *eup->v1();
	Vertex		dstLo	= *elo->v1();
	Vertex		ev		= *event;

	ISO_ASSERT(!(dstLo == dstUp));
	ISO_ASSERT(EdgeSign(dstUp, ev, orgUp) <= 0);
	ISO_ASSERT(EdgeSign(dstLo, ev, orgLo) >= 0);
	ISO_ASSERT(!rup->fix_up && !rlo->fix_up);

	float	tMinUp = min(orgUp.v.y, dstUp.v.y);
	float	tMaxLo = max(orgLo.v.y, dstLo.v.y);
	if (tMinUp > tMaxLo)
		return 0;	// t ranges do not overlap

	if (orgUp <= orgLo
		? (EdgeSign(dstLo, orgUp, orgLo) > 0)
		: (EdgeSign(dstUp, orgLo, orgUp) < 0)
	)
		return 0;

	// At this point the edges intersect, at least marginally

	Vertex	isect = intersect(dstUp, orgUp, dstLo, orgLo);

	// The following properties are guaranteed:
	ISO_ASSERT(min(orgUp.v.y, dstUp.v.y) <= isect.v.y && isect.v.y <= max(orgLo.v.y, dstLo.v.y));
	ISO_ASSERT(min(dstLo.v.x, dstUp.v.x) <= isect.v.x && isect.v.x <= max(orgLo.v.x, orgUp.v.x));

	if (isect <= ev)	// The intersection point lies slightly to the left of the sweep line, so move it until it's slightly to the right of the sweep line.
		isect = ev;

	// Similarly, if the computed intersection lies to the right of the rightmost origin, it can cause unbelievable inefficiency on sufficiently degenerate inputs.
	Vertex	orgMin = orgUp <= orgLo ? orgUp : orgLo;
	if (orgMin <= isect)
		isect = orgMin;

	if (isect == orgUp || isect == orgLo) {
		// Easy case -- intersection at one of the right endpoints
		(void)CheckForRightSplice(rup);
		return 0;
	}

	if ((!(dstUp == ev) && EdgeSign(dstUp, ev, isect) >= 0)
	||  (!(dstLo == ev) && EdgeSign(dstLo, ev, isect) <= 0)
	) {
		// Very unusual -- the new upper or lower edge would pass on the wrong side of the sweep event, or through it
		if (elo->v1() == event) {
			// Splice dstLo into eup, and process the new region(s)
			mesh_splitedge(eup->flip());
			mesh_t::splice_edges(elo->flip(), eup);
			rup		= rup->TopLeft();
			eup		= rup->below()->up;
			FinishLeftRegions(rup->below(), rlo);
			return AddRightEdges(rup, eup->v0_prev(), eup, eup, event);
		}
		if (eup->v1() == event) {
			// Splice dstUp into elo, and process the new region(s)
			mesh_splitedge(elo->flip());
			mesh_t::splice_edges(eup->fnext, elo->v0_prev());
			rlo		= rup;
			rup		= rup->TopRight();
			Edge *e	= rup->below()->up->f1_prev();
			rlo->up	= elo->v0_prev();
			elo		= FinishLeftRegions(rlo, NULL);
			return AddRightEdges(rup, elo->v0_next(), eup->f1_prev(), e, event);
		}
		// Special case: called from ConnectRightVertex. If either edge passes on the wrong side of tess->event, split it (and wait for ConnectRightVertex to splice it appropriately).
		if (EdgeSign(dstUp, ev, isect) >= 0) {
			rup->above()->dirty = rup->dirty = true;
			mesh_splitedge(eup->flip());
			eup->v->v = event->v;
		}
		if (EdgeSign(dstLo, ev, isect) <= 0) {
			rup->dirty	= rlo->dirty = true;
			mesh_splitedge(elo->flip());
			elo->v->v = event->v;
		}
		// leave the rest for ConnectRightVertex

	} else {
		// General case -- split both edges, splice into new vertex
		// When we do the splice operation, the order of the arguments is arbitrary, but when the operation creates a new face, the work done is proportional to the size of the new face.
		// We expect the faces in the processed part of the mesh (ie. eup->Lface) to be smaller than the faces in the unprocessed original contours (which will be elo->Oprev->Lface).
		mesh_splitedge(eup->flip());
		mesh_splitedge(elo->flip());
		mesh_t::splice_edges(elo->v0_prev(), eup);
		eup->v->v	= isect.v;
		pq.push(eup->v);
		rup->above()->dirty = rup->dirty = rlo->dirty = true;
	}
	return 0;
}

// This routine walks through all the dirty regions and makes sure that the dictionary invariants are satisfied
void Sweep::WalkDirtyRegions(Region *rup, MeshVertex *event) {
	Region	*rlo = rup->below();

	for (;;) {
		// find the lowest dirty region (we walk from the bottom up)
		while (rlo->dirty) {
			rup = rlo;
			rlo = rlo->below();
		}
		if (!rup->dirty) {
			rlo = rup;
			rup = rup->above();
			if (rup == dict.end().get() || !rup->dirty)	// we've walked all the dirty regions
				return;
		}

		rup->dirty = false;

		Edge	*eup = rup->up;
		Edge	*elo = rlo->up;

		if (eup->v1() != elo->v1()) {
			// check that the edge ordering is obeyed at the v1 vertices
			if (CheckForLeftSplice(rup)) {
				// if the upper or lower edge was marked fix_up, then we no longer need it (since these edges are needed only for vertices which otherwise have no right-going edges)
				if (rlo->fix_up) {
					delete rlo;
					mesh_t::delete_edge(elo);
					rlo = rup->below();
					elo = rlo->up;
				} else if (rup->fix_up) {
					delete rup;
					mesh_t::delete_edge(eup);
					rup = rlo->above();
					eup = rup->up;
				}
			}
		}
		if (eup->v0() != elo->v0()) {
			if (eup->v1() != elo->v1() && !rup->fix_up && !rlo->fix_up && (eup->v1() == event || elo->v1() == event)) {
				// when all else fails in CheckForIntersect, it uses event as the intersection location
				// this requires that event lie between the upper and lower edges, and also that neither of these is marked fix_up
				if (Region *r = CheckForIntersect(rup, event)) {
					// got new dirty regions
					rup = r;
					rlo = rup->below();
				}
			} else {
				// Even though we can't use CheckForIntersect(), the v0 vertices may violate the dictionary edge ordering - check and correct this
				(void)CheckForRightSplice(rup);
			}
		}
		if (eup->v == elo->v && eup->v1() == elo->v1()) {
			// a degenerate face consisting of only two edges - delete it
			AddWinding(elo, eup);
			delete rup;
			mesh_t::delete_edge(eup);
			rup = rlo->above();
		}
	}
}

void Sweep::SweepEvent(MeshVertex *event) {
//	static int nsweep = 0;
//	ISO_TRACEF("Event %i 0x%08x: %g, %g\n", nsweep++, uint32(uintptr_t(event)), event->s, event->t);

	// check if this vertex is the right endpoint of an edge that is already in the dictionary
	// in this case we don't need to waste time searching for the location to insert new edges
	Edge	*e = event->e;
	while (!e->region) {
		e = e->vnext;
		if (e == event->e) {
			// all edges go right -- not incident to any processed edges
			// ConnectLeftVertex:
			Region	*rup	= dict.search(event, e->flip());
			Region	*rlo	= rup->below();
			Edge	*eup	= rup->up;
			Edge	*elo	= rlo->up;

			// try merging with U or L first
			if (EdgeSign(*eup->v1(), *event, *eup->v0()) == 0) {
				//ConnectLeftDegenerate:
				// event vertex lies exacty on an already-processed edge or vertex, so adding the new vertex involves splicing it into the already-processed part of the mesh
				if (*eup->v == *event) {
					// eup->v0 is an unprocessed vertex; just combine them and wait for eup->v0 to be pulled from the queue
					mesh_t::splice_edges(eup, event->e);
					return;
				}

				if (*eup->v1() == *event) {
					// event coincides with eup->v1 which has already been processed; splice in the additional right-going edges
					rup					= rup->TopRight();
					Region	*reg		= rup->below();
					Edge	*eTopRight	= reg->up->flip();
					Edge	*eTopLeft	= eTopRight->vnext, *eend = eTopLeft;
					if (reg->fix_up) {
						// eup->v1 has only a single fixable edge going right; we can delete it since now we have some real right-going edges
						ISO_ASSERT(eTopLeft != eTopRight);	// there are some left edges too
						delete reg;
						mesh_t::delete_edge(eTopRight);
						eTopRight = eTopLeft->v0_prev();
					}

					mesh_t::splice_edges(event->e, eTopRight);

					if (!(*eTopLeft->v1() <= *eTopLeft->v0()))
						eTopLeft = NULL;					// e->v1 had no left-going edges -- tell AddRightEdges

					WalkDirtyRegions(AddRightEdges(rup, eTopRight->vnext, eend, eTopLeft, event), event);
					return;
				}

				// General case -- splice event into edge eup which passes through it
				mesh_splitedge(eup->flip());
				if (rup->fix_up) {
					// This edge was fixable -- delete unused portion of original edge
					mesh_t::delete_edge(eup->vnext);
					rup->fix_up = false;
				}
				mesh_t::splice_edges(event->e, eup);

			} else {
				// Connect event to rightmost processed vertex of either chain; e->v1 is the vertex that we will connect to
				Region *reg = *elo->v1() <= *eup->v1() ? rup : rlo;
				//if (!rup->inside && !reg->fix_up) {
				if (!test_winding(rule, rup->winding) && !reg->fix_up) {
					// the new vertex is in a region which does not belong to the polygon, so we don't need to connect this vertex to the rest of the mesh
					WalkDirtyRegions(AddRightEdges(rup, e, e, NULL, event), event);
					return;
				}

				Edge	*enew = reg == rup
					? mesh_t::connect_edges(e->flip(), eup->fnext)
					: mesh_t::connect_edges(elo->flip()->vnext->flip(), event->e)->flip();

				if (reg->fix_up) {
					reg->FixUpperEdge(enew);
				} else {
					Region	*rnew = dict.insert_before(event, rup, enew);
					rnew->ComputeWinding(rule);
				}
			}

			// loop
			e = event->e;
		}
	}

	// processing consists of two phases: first we "finish" all the active regions where both the upper and lower edges terminate at event
	// we mark these faces "inside" or "outside" the polygon according to their winding number, and delete the edges from the dictionary
	Region	*rup			= e->region->TopLeft();
	Region	*rlo			= rup->below();
	Edge	*eTopLeft		= rlo->up;
	Edge	*eBottomLeft	= FinishLeftRegions(rlo, NULL);

	// Next we process all the right-going edges from event
	// this involves adding the edges to the dictionary and creating the associated "active regions" which record information about the regions between adjacent dictionary edges
	if (eBottomLeft->vnext == eTopLeft) {
		//no right-going edges -- add a temporary "fixable" edge
		//ConnectRightVertex:
		rlo					= rup->below();
		Edge	*eup		= rup->up;
		Edge	*elo		= rlo->up;
		bool	degenerate	= false;

		if (eup->v1() != elo->v1()) {
			if (Region *r = CheckForIntersect(rup, event))
				WalkDirtyRegions(r, event);
		}

		// possible new degeneracies: upper or lower edge of rup may pass through event or may coincide with new intersection vertex
		if (*eup->v0() == *event) {
			mesh_t::splice_edges(eTopLeft->v0_prev(), eup);
			rup			= rup->TopLeft();
			eTopLeft	= rup->below()->up;
			FinishLeftRegions(rup->below(), rlo);
			degenerate	= true;
		}
		if (*elo->v0() == *event) {
			mesh_t::splice_edges(eBottomLeft, elo->v0_prev());
			eBottomLeft = FinishLeftRegions(rlo, NULL);
			degenerate = true;
		}
		if (degenerate) {
			rup		= AddRightEdges(rup, eBottomLeft->vnext, eTopLeft, eTopLeft, event);

		} else {
			// non-degenerate situation -- need to add a temporary, fixable, edge and connect to the closer of elo->v, up->v
			Edge *e	= mesh_t::connect_edges(eBottomLeft->f0_prev(), *elo->v0() <= *eup->v0() ? elo->v0_prev() : eup);
			rup		= AddRightEdges(rup, e, e->vnext, e->vnext, event);
			e->flip()->region->fix_up = true;
		}

	} else {
		rup = AddRightEdges(rup, eBottomLeft->vnext, eTopLeft, eTopLeft, event);

	}

	WalkDirtyRegions(rup, event);
}

// Tessellate a monotone region:
// The region must consist of a single loop of half-edges oriented CCW, and any vertical line intersects the interior of the region in a single interval
// We add interior edges to split the region into non-overlapping triangles

// There are two edge chains, an upper chain and a lower chain. We process all vertices from both chains in order, from right to left
// The algorithm ensures that the following invariant holds after each vertex is processed:
//	the untessellated region consists of two chains, where one chain is a single edge, and the other chain is concave
//	the left vertex of the single edge is always to the left of all vertices in the concave chain
// Each step consists of adding the rightmost unprocessed vertex to one of the two chains, and forming a fan of triangles from the rightmost of two chain endpoints
// Determining whether we can add each triangle to the fan is a simple orientation test. By making the fan as large as possible, we restore the invariant

void Sweep::TessellateMonoRegion(MeshFace *face) {
	Edge *up = face->e;
	ISO_ASSERT(up->fnext != up && up->fnext->fnext != up);

	while (*up->v1() <= *up->v0())
		up = up->f0_prev();
	while (*up->v0() <= *up->v1())
		up = up->f0_next();

	Edge *lo = up->f0_prev();

	while (up->fnext != lo) {
		if (*up->v1() <= *lo->v0()) {
			// up->v1() is on the left; we can form triangles from lo->v0()
			// The EdgeGoesLeft test guarantees progress even when some triangles are CW, given that the upper and lower chains are truly monotone
			while (lo->fnext != up && (lo->fnext->v1() <= lo->fnext->v0() || EdgeSign(*lo->v0(), *lo->v1(), *lo->fnext->v1()) <= 0))
				lo = mesh_t::connect_edges(lo->fnext, lo)->flip();
			lo = lo->f0_prev();
		} else {
			// lo->v0() is on the left; we can form triangles from up->v1()
			while (lo->fnext != up && ((up->f0_prev()->v0() <= up->f0_prev()->v1()) || EdgeSign(*up->v1(), *up->v0(), *up->f0_prev()->v0()) >= 0))
				up = mesh_t::connect_edges(up, up->f0_prev())->flip();
			up = up->fnext;
		}
	}

	// lo->v0() == up->v1() == the leftmost vertex, and the remaining region can be tessellated in a fan from this leftmost vertex
	ISO_ASSERT(lo->fnext != up);
	while (lo->fnext->fnext != up)
		lo = mesh_t::connect_edges(lo->fnext, lo)->flip();
}

void Sweep::DoSweep() {
	mesh.remove_degenerate_edges();

	for (auto &v : mesh.verts)
		pq.push(&v);

	// add two sentinel edges above and below all other edges, to avoid special cases at the top and bottom.
	Edge	*e;
	e = mesh.make_edge();
	e->v0()->set({ maximum, minimum });
	e->v1()->set({ minimum, minimum });
	dict.push_back(new Region(e));

	e = mesh.make_edge();
	e->v0()->set({ maximum, maximum });
	e->v1()->set({ minimum, maximum });
	dict.push_back(new Region(e));

	// each vertex defines an event for our sweep line
	while(!pq.empty()) {
		MeshVertex *v	= pq.pop_value();
		while (!pq.empty() && *pq.top() == *v) {
			MeshVertex	*next = pq.pop_value();
			mesh_t::splice_edges(v->e, next->e);
		}
		SweepEvent(v);
	}

	// delete any degenerate faces with only two edges
	for (auto i = mesh.faces.begin(); i != mesh.faces.end();) {
		Edge	*e = i++->e;
		ISO_ASSERT(e->fnext != e);

		if (e->fnext->fnext == e) {
			AddWinding(e->vnext, e);
			mesh.delete_edge(e);
		}
	}
}

void Sweep::ComputeInterior() {
	DoSweep();

	// delete edges which do not separate an interior region from an exterior one
	for (auto i = mesh.edges.begin(); i != mesh.edges.end();) {
		Edge	&e = *i++;
		if (e.f1()->inside == e.f0()->inside) {
			mesh.delete_edge(&e);
		} else {
			//e->winding = e->f->inside ? 1 : -1;
		}
	}

	// zap all faces which are not marked "inside" the polygon
	for (auto i = mesh.faces.begin(); i != mesh.faces.end();) {
		MeshFace &f = *i++;
		if (!f.inside)
			f.remove();
	}
}

// Tessellates each region of the mesh which is marked "inside" the polygon; each such region must be monotone.
void Sweep::TessellateInterior() {
	DoSweep();

	for (auto i = mesh.faces.begin(); i != mesh.faces.end();) {
		MeshFace	&f = *i++;
		if (f.inside)
			TessellateMonoRegion(&f);
		else
			f.remove();
	}
}

Sweep::MeshFace *Sweep::AddContour(const position2 *p, int n) {
	Edge *e	= 0;
	while (n--) {
		if (!e) {
			// Make a self-loop (one vertex, one edge)
			e = mesh.make_edge();
			mesh_t::splice_edges(e, e->flip());
		} else {
			// Create a new vertex and edge which immediately follow e in the ordering around the left face.
			e = mesh_t::split_edge(e);
		}

		// the winding of an edge says how the winding number changes as we cross from the edge's right face to its left face
		// we add the vertices in such an order that a CCW contour will add +1 to the winding number of the region inside the contour
		e->winding			= 1;
		e->flip()->winding	= -1;

		e->v->v = p->v;
		++p;
	}
	return e->f;
}

int find_contour(WINDING_RULE rule, const position2 *p, int n, position2 *result, int *lengths) {
	Sweep	sweep(rule);
	sweep.AddContour(p, n);
	sweep.ComputeInterior();

	position2	*d		= result;
	int			*lenp	= lengths;
	for (auto &f : sweep.mesh.faces) {
		if (f.inside) {
			position2	*d0	= d;
			Sweep::Edge *e	= f.e;
			do {
				*d++ = *e->v;
				e = e->fnext;
			} while (e != f.e);
			*lenp++ = d - d0;
		}
	}
	return lenp - lengths;
}

} // namespace iso