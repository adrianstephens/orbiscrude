//-----------------------------------------------------------------------------
//
//	based on Generic Polygon Clipper:
//		A new algorithm for calculating the difference, intersection, exclusive-or or union of arbitrary polygon sets.
//		Alan Murta (email: gpc@cs.man.ac.uk)
//-----------------------------------------------------------------------------

#include "gpc.h"
#include "base/list.h"
#include "base/algorithm.h"

using namespace iso;

enum SIDE : uint8 {
	LEFT	= 0,
	RIGHT	= 1,
};
inline void operator^=(SIDE &a, bool b) {
	(uint8&)a ^= uint8(b);
}

enum VDIR {
	ABOVE	= 0,
	BELOW	= 1,
};

enum TYPE : uint8 {
	CLIP	= 0,
	SUBJ	= 1,
};

TYPE operator!(TYPE t) {
	return TYPE(1 - t);
}

// Edge intersection classes
// bit 0: tr
// bit 1: tl
// bit 2: br
// bit 3: bl
enum vertex_type {
	NUL, // Empty non-intersection
	EMX, // External maximum
	ELI, // External left intermediate
	TED, // Top edge

	ERI, // External right intermediate
	RED, // Right edge
	IMM, // Internal maximum and minimum
	IMN, // Internal minimum

	EMN, // External minimum
	EMM, // External maximum and minimum
	LED, // Left edge
	ILI, // Internal left intermediate

	BED, // Bottom edge
	IRI, // Internal right intermediate
	IMX, // Internal maximum
	FUL	 // Full non-intersection
};

inline vertex_type make_vertex_type(bool tr, bool tl, bool br, bool bl)	{ return vertex_type(tr + tl * 2 + br * 4 + bl * 8); }
inline vertex_type operator^(vertex_type a, bool b)						{ return vertex_type((int)a ^ (b * 15)); }
inline vertex_type operator^(vertex_type a, vertex_type b)				{ return vertex_type((int)a ^ (int)b); }
inline vertex_type operator&(vertex_type a, vertex_type b)				{ return vertex_type((int)a & (int)b); }
inline vertex_type operator|(vertex_type a, vertex_type b)				{ return vertex_type((int)a | (int)b); }

inline vertex_type combine(gpc_op op, vertex_type a, vertex_type b) {
	return	op == GPC_XOR	? a ^ b
		:	op == GPC_UNION	? a | b
		:	a & b;
}

// Horizontal edge states
enum h_state : uint8 {
	NH,			// No horizontal edge
	BH,			// Bottom horizontal edge
	TH			// Top horizontal edge
};

// Edge bundle state
enum bundle_state {
	UNBUNDLED,	 // Isolated edge not within a bundle
	BUNDLE_HEAD, // Bundle head node
	BUNDLE_TAIL	 // Passive bundle tail node
};

// Internal vertex list datatype
struct vertex_node {
	gpc_vertex	v;
	vertex_node* next;
	vertex_node(const gpc_vertex &x, vertex_node* next = nullptr) : v(x), next(next) {}
};

// Internal contour / tristrip type
struct polygon_node {
	int				active;		// Active flag / vertex count
	bool			hole;		// Hole / external contour flag
	vertex_node*	v[2];		// Left and right vertex list ptrs (2 lists for strips, front + back for polys)
	polygon_node*	proxy;		// Pointer to actual structure used

	polygon_node() : active(1), proxy(nullptr) {
		v[LEFT]		= nullptr;
		v[RIGHT]	= nullptr;
	}

	//polys
	void add_left(double x, double y) {
		proxy->v[LEFT] = new vertex_node(gpc_vertex{x, y}, proxy->v[LEFT]);
	}

	//polys
	void add_right(double x, double y) {
		vertex_node* nv = new vertex_node(gpc_vertex{x, y});
		proxy->v[RIGHT]->next = nv;
		proxy->v[RIGHT] = nv;
	}

	//strips
	void add_vertex(SIDE xdir, double x, double y) {
		vertex_node	**p = &v[xdir], *n = *p;
		while (n) {
			p = &n->next;
			n = n->next;
		}
		*p = new vertex_node(gpc_vertex{x, y});
		++active;
	}
};

struct polygon_list : slist<polygon_node> {

	//polys
	int count_contours() {
		int		 nc = 0;
		for (auto &i : *this) {
			if (i.active) {
				// Count the vertices in the current contour
				int	nv = 0;
				for (vertex_node *v = i.proxy->v[LEFT]; v; v = v->next)
					nv++;

				// Record valid vertex counts in the active field
				if (nv > 2) {
					i.active = nv;
					nc++;
				} else {
					// Invalid contour: just free the heap
					for (vertex_node *v = i.proxy->v[LEFT], *nextv; v; v = nextv) {
						nextv = v->next;
						delete v;
					}
					i.active = 0;
				}
			}
		}
		return nc;
	}

	polygon_node* add_local_min(double x, double y) {
		auto	&p	= push_front();
		p.proxy		= &p;			// Initialise proxy to point to p itself
		p.v[RIGHT]	= p.v[LEFT] = new vertex_node(gpc_vertex{x, y});
		return &p;
	}

	// Redirect any p->proxy references to q->proxy
	void redirect(polygon_node *from, polygon_node *to) {
		for (auto &i : *this) {
			if (i.proxy == from) {
				i.active = false;
				i.proxy	 = to;
			}
		}
	}

	//strips
	int count_tristrips() const {
		int total = 0;
		for (auto &i : *this)
			if (i.active > 2)
				++total;
		return total;
	}

	polygon_node *add_tristrip(double x, double y) {
		auto	&p	= push_front();
		p.v[LEFT] = new vertex_node(gpc_vertex{x, y});
		return &p;
	}
};


struct edge_node : e_link<edge_node> {
	gpc_vertex		bot;			// Edge lower (x, y) coordinate
	gpc_vertex		top;			// Edge upper (x, y) coordinate
	double			xb;				// Scanbeam bottom x coordinate
	double			xt;				// Scanbeam top x coordinate
	double			dx;				// Change in x for a unit y increase
	TYPE			type;			// Clip / subject edge flag

	struct bside_info {
		SIDE	clip, subj;
		//bool&	operator[](TYPE i) { return (&clip)[i]; }
	};
	struct bundle_info {
		bool			clip, subj;
		bundle_state	state;
		bool&	operator[](TYPE i) { return (&clip)[i]; }
	};
	bundle_info		bundle[2];		// Bundle edge flags
	bside_info		bside;			// Bundle left / right indicators
	polygon_node*	outp[2];		// Output polygon / tristrip pointer
	edge_node*		succ;			// Edge connected at the upper end (= this + 1, or null)
	edge_node*		next_bound;		// Pointer to next bound in LMT

	void init(const gpc_vertex &v0, const gpc_vertex &v1, TYPE _type, gpc_op op) {
		bot			= v0;
		top			= v1;
		xb			= v0.x;
		dx			= (v1.x - v0.x) / (v1.y - v0.y);
		type		= _type;
		outp[ABOVE]	= NULL;
		outp[BELOW]	= NULL;
		next		= NULL;
		prev		= NULL;
		succ		= this + 1;
		next_bound	= NULL;
		bside.clip	= op == GPC_DIFF ? RIGHT : LEFT;
		bside.subj	= LEFT;
	}

	// Set up bundle fields (e0 is previous edge)
	edge_node *setup_bundles(edge_node *e0, double y) {
		bool	cross			= top.y != y;
		bundle[ABOVE].clip		= type == CLIP && cross;
		bundle[ABOVE].subj		= type == SUBJ && cross;
		bundle[ABOVE].state		= UNBUNDLED;

		// Bundle edges above the scanbeam boundary if they coincide
		if (cross) {
			if (e0 && e0->top.y != y && abs(e0->xb - xb) <= GPC_EPSILON && abs(e0->dx - dx) <= GPC_EPSILON) {
				bundle[ABOVE][type]		= !e0->bundle[ABOVE][type];
				bundle[ABOVE][!type]	= e0->bundle[ABOVE][!type];
				bundle[ABOVE].state		= BUNDLE_HEAD;
				e0->bundle[ABOVE].clip	= false;
				e0->bundle[ABOVE].subj	= false;
				e0->bundle[ABOVE].state	= BUNDLE_TAIL;
			}
			return this;
		}
		return e0;
	}

	//tristrips
	inline void add_vertex(VDIR vdir, SIDE xdir, double x, double y) {
		outp[vdir]->add_vertex(xdir, x, y);
	}

	//tristrips
	edge_node *prev_edge(VDIR vdir, double &i, double j) {
		auto	e = this;
		do
			e = e->prev;
		while (!e->outp[vdir]);
		i = e->bot.x + e->dx * (j - e->bot.y);
		return e;
	}

	//tristrips
	edge_node *next_edge(VDIR vdir, double &i, double j) {
		auto	e = this;
		do
			e = e->next;
		while (!e->outp[vdir]);
		i = e->bot.x + e->dx * (j - e->bot.y);
		return e;
	}
};

struct edge_list : e_list<edge_node> {
	void add_edge(edge_node* edge) {
		auto	i = lower_boundc(*this, edge, [](const edge_node  &n, edge_node* edge) {
			return n.xb < edge->xb || (n.xb == edge->xb && n.dx < edge->dx);
		});
		insert_before(i, edge);
	}
};

struct edge_bound_list {
	edge_node*	start;
	edge_bound_list() : start(nullptr) {}

	void insert_bound(edge_node* e) {
		edge_node *p = 0, *n = start;
		while (n && e->bot.x >= n->bot.x && (e->bot.x != n->bot.x || e->dx < n->dx)) {
			p = n;
			n = n->next_bound;
		}
		e->next_bound = n;
		(p ? p->next_bound : start) = e;
	}
};

struct lmt_node {
	double		y;			// Y coordinate at local minimum
	edge_bound_list	first_bound;// bound list
	lmt_node(double y) : y(y) {}
};

// Local minima table
struct lmt_list : slist<lmt_node> {
	typedef lmt_node node;

	edge_bound_list& bound_list(double y) {
		auto	i = lower_boundc(insertable(), y, [](const lmt_node &n, double y) { return n.y < y; });
		if (i == end() || i->y > y)
			insert(i, y);

		return i->first_bound;
	}

};

// Scanbeam tree - just for sorting vertical positions
struct sb_tree {
	struct node {
		double	 y;			// Scanbeam node y value
		node*	less;		// Pointer to nodes with lower y
		node*	more;		// Pointer to nodes with higher y
		node(double y) : y(y), less(nullptr), more(nullptr) {}
		~node() { delete less; delete more; }
	};
	node	*root;
	sb_tree() : root(0) {}
	~sb_tree() { delete root; }

	bool add(double y) {
		node **p = &root;
		while (node *n = *p) {
			if (n->y == y)
				return false;
			p = y < n->y ? &n->less : &n->more;
		}
		*p = new node(y);
		return true;
	}

	dynamic_array<double> flatten() {
		dynamic_array<double>	out;
		node	*stack[32], **sp = stack;
		for (node *n = root; n;) {
			while (n->less) {
				*sp++ = n;
				n = n->less;
			}
			out.push_back(n->y);
			while (!n->more && sp > stack) {
				n = *--sp;
				out.push_back(n->y);
			}
			n = n->more;
		}
		return out;
	}
};

inline bool contribution(const edge_node *e0, const edge_node *e1) {
	return (e0->bundle[ABOVE].clip || e0->bundle[ABOVE].subj)
		&& (e1->bundle[ABOVE].clip || e1->bundle[ABOVE].subj);
}

inline bool in_clip(const edge_node *e0, const edge_node *e1) {
	return (e0->bundle[ABOVE].clip && !e0->bside.clip)
		|| (e1->bundle[ABOVE].clip && e1->bside.clip)
		|| (!e0->bundle[ABOVE].clip && !e1->bundle[ABOVE].clip && e0->bside.clip && e1->bside.clip);
}
inline bool in_subj(const edge_node *e0, const edge_node *e1) {
	return (e0->bundle[ABOVE].subj && !e0->bside.subj)
		|| (e1->bundle[ABOVE].subj && e1->bside.subj)
		|| (!e0->bundle[ABOVE].subj && !e1->bundle[ABOVE].subj && e0->bside.subj && e1->bside.subj);
}

// Intersection
struct it_node {
	gpc_vertex	v;
	edge_node	*e0, *e1;		// Intersecting edge (bundle) pair
	it_node(edge_node* e0, edge_node* e1, double x, double y) : v{x, y}, e0(e0), e1(e1) {}

};

struct it_list : slist<it_node> {
	// Sorted edge node
	struct st_node {
		edge_node*	edge;		// Pointer to AET edge
		double		xb;			// Scanbeam bottom x coordinate
		double		xt;			// Scanbeam top x coordinate
		double		dx;			// Change in x for a unit y increase
								//	st_node*	prev;		// Previous edge in sorted list
		st_node(edge_node* edge) : edge(edge), xb(edge->xb), xt(edge->xt), dx(edge->dx) {}
	};

	void add_intersection(edge_node* edge0, edge_node* edge1, double x, double y) {
		auto	i = lower_boundc(insertable(), y, [](const it_node &n, double y) { return n.v.y < y; });
		emplace(i, edge0, edge1, x, y);
	}

	// Build intersection table for the current scanbeam
	it_list(edge_list &aet, double dy) {
		slist<st_node>	sorted;
		for (auto &e : aet) {
			if (e.bundle[ABOVE].state == BUNDLE_HEAD || e.bundle[ABOVE].clip || e.bundle[ABOVE].subj) {
				auto	i = lower_boundc(sorted.insertable(), e, [this, dy](const st_node &st, edge_node &e) {
					double	den = (st.xt - st.xb) - (e.xt - e.xb);

					// If new edge and ST edge don't cross, insert edge here
					if (e.xt >= st.xt || e.dx == st.dx || abs(den) <= GPC_EPSILON)
						return false;

					// Compute intersection between new edge and ST edge
					double	r = (e.xb - st.xb) / den;
					double	x = st.xb + r * (st.xt - st.xb);
					double	y = r * dy;

					// Insert the edge pointers and the intersection point in the IT
					add_intersection(st.edge, &e, x, y);
					return true;
				});

				sorted.insert(i, &e);
			}
		}
	}
};

// Horizontal edge state transitions within scanbeam boundary
const h_state next_h_state[3][4][2] = {
//			NONE		ABOVE		BELOW		CROSS
//			L   R		L   R		L   R		L   R
/* NH */ {	{NH, NH},	{BH, TH},	{TH, BH},	{NH, NH}	},
/* BH */ {	{BH, BH},	{NH, NH},	{NH, NH},	{TH, TH}	},
/* TH */ {	{TH, TH},	{NH, NH},	{NH, NH},	{BH, BH}	}
};

inline bool OPTIMAL(const gpc_vertex_list &list, const gpc_vertex *i)	{ return prev_wrap(list, i)->y != i->y || next_wrap(list, i)->y != i->y; }

static dynamic_array<edge_node> build_lmt(lmt_list &lmt, sb_tree &sbtree, const gpc_polygon &p, TYPE type, gpc_op op) {
	int		total_vertices = 0;
	for (auto &contour : p) {
		if (!(contour.flag & gpc_vertex_list_flag::IGNORE)) {
			for (auto &i : contour) {
				if (OPTIMAL(contour, &i))	// Ignore superfluous vertices embedded in horizontal edges
					++total_vertices;
			}
		}
	}

	// Create the entire input polygon edge table in one go
	gpc_vertex_list				edge_verts;
	dynamic_array<edge_node>	edge_table(total_vertices);
	edge_node					*e = edge_table;

	for (auto &contour : p) {
		if (!(contour.flag & gpc_vertex_list_flag::IGNORE)) {
			// Perform contour optimisation
			edge_verts.clear();

			for (auto &i : contour) {
				if (OPTIMAL(contour, &i)) {
					edge_verts.push_back(i);
					// Record vertex in the scanbeam table
					sbtree.add(i.y);
				}
			}

			// Do the contour forward pass
			for (auto &min : make_const(edge_verts)) {
				// If a forward local minimum...
				auto	v	= &min;
				auto	v1	= next_wrap(edge_verts, v);
				if (v1->y > min.y && prev_wrap(edge_verts, &min)->y >= min.y) {
					auto	e0	= e;
					e0->bundle[BELOW].state	= UNBUNDLED;
					e0->bundle[BELOW].clip	= false;
					e0->bundle[BELOW].subj	= false;

					// Search for the next local maximum...
					while (v1->y > v->y) {
						e++->init(*v, *v1, type, op);
						v	= v1;
						v1	= next_wrap(edge_verts, v);
					}
					e[-1].succ	= nullptr;
					lmt.bound_list(min.y).insert_bound(e0);
				}
			}

			// Do the contour reverse pass
			for (auto &min : make_const(edge_verts)) {
				// If a reverse local minimum...
				auto	v	= &min;
				auto	v1	= prev_wrap(edge_verts, &min);
				if (v1->y > min.y && next_wrap(edge_verts, &min)->y >= min.y) {
					auto	e0	= e;
					e0->bundle[BELOW].state	= UNBUNDLED;
					e0->bundle[BELOW].clip	= false;
					e0->bundle[BELOW].subj	= false;

					// Search for the previous local maximum...
					while (v1->y > v->y) {
						e++->init(*v, *v1, type, op);
						v	= v1;
						v1	= prev_wrap(edge_verts, v);
					}
					e[-1].succ	= nullptr;
					lmt.bound_list(min.y).insert_bound(e0);
				}
			}
		}
	}
	return edge_table;
}



static void merge_left(polygon_node *p, polygon_node *q, polygon_list &list) {
	// Label contour as a hole
	q->proxy->hole = true;

	if (p->proxy != q->proxy) {
		// Assign p's vertex list to the left end of q's list
		p->proxy->v[RIGHT]->next = q->proxy->v[LEFT];
		q->proxy->v[LEFT]		 = p->proxy->v[LEFT];

		// Redirect any p->proxy references to q->proxy
		list.redirect(p->proxy, q->proxy);
	}
}

static void merge_right(polygon_node *p, polygon_node *q, polygon_list &list) {
	// Label contour as external
	q->proxy->hole = false;

	if (p->proxy != q->proxy) {
		// Assign p's vertex list to the right end of q's list
		q->proxy->v[RIGHT]->next = p->proxy->v[LEFT];
		q->proxy->v[RIGHT]		 = p->proxy->v[RIGHT];

		// Redirect any p->proxy references to q->proxy
		list.redirect(p->proxy, q->proxy);
	}
}

static void minimax_test(gpc_op op, const gpc_polygon &subj, const gpc_polygon &clip) {
	typedef interval<gpc_vertex>	bbox;

	dynamic_array<bbox>	s_bbox	= transformc(subj, [](const gpc_vertex_list_flag &a) { return get_extent(a); });
	dynamic_array<bbox>	c_bbox	= transformc(clip, [](const gpc_vertex_list_flag &a) { return get_extent(a); });
	bool	*o_table = new bool[subj.size() * clip.size()];

	// Check all subject contour bounding boxes against clip boxes
	for (int s = 0; s < subj.size(); s++)
		for (int c = 0; c < clip.size(); c++)
			o_table[c * subj.size() + s] = overlap(s_bbox[s], c_bbox[c]);

	// For each clip contour, search for any subject contour overlaps
	for (int c = 0; c < clip.size(); c++) {
		bool	overlap = 0;
		for (int s = 0; !overlap && s < subj.size(); s++)
			overlap = o_table[c * subj.size() + s];

		if (!overlap)
			// Flag non contributing status by negating vertex count
			clip[c].flag |= gpc_vertex_list_flag::IGNORE;
	}

	if (op == GPC_INT) {
		// For each subject contour, search for any clip contour overlaps
		for (int s = 0; s < subj.size(); s++) {
			bool	overlap = 0;
			for (int c = 0; !overlap && c < clip.size(); c++)
				overlap = o_table[c * subj.size() + s];

			if (!overlap)
				// Flag non contributing status by negating vertex count
				subj[s].flag |= gpc_vertex_list_flag::IGNORE;
		}
	}

	delete[] o_table;
}

//-----------------------------------------------------------------------------
//							 gpc_polygon_clip
//-----------------------------------------------------------------------------

gpc_polygon iso::gpc_polygon_clip(gpc_op op, const gpc_polygon &subj, const gpc_polygon &clip) {

	gpc_polygon	result;

	// Test for trivial NULL result cases
	if ((subj.empty() && clip.empty())
	|| (subj.empty() && (op == GPC_INT || op == GPC_DIFF))
	|| (clip.empty() && op == GPC_INT)
	)
		return result;

	// Identify potentially contributing contours
	if ((op == GPC_INT || op == GPC_DIFF) && subj && clip)
		minimax_test(op, subj, clip);

	// Build LMT
	sb_tree			sbtree;
	lmt_list		lmt;
	auto			s_heap = build_lmt(lmt, sbtree, subj, SUBJ, op);
	auto			c_heap = build_lmt(lmt, sbtree, clip, CLIP, op);

	// Return a nothing if no contours contribute
	if (lmt.empty())
		return result;

	auto			sbt			= sbtree.flatten();
	SIDE			parity_clip	= op == GPC_DIFF ? RIGHT : LEFT;
	SIDE			parity_subj	= LEFT;
	auto			local_min	= lmt.begin();
	polygon_node	*cf			= NULL;
	edge_list		aet;
	polygon_list	out_poly;

	// Process each scanbeam
	for (int scanbeam = 0; scanbeam < sbt.size();) {
		// Set yb and yt to the bottom and top of the scanbeam
		double	yb = sbt[scanbeam++];
		double	yt = scanbeam < sbt.size() ? sbt[scanbeam] : yb;

		// === SCANBEAM BOUNDARY PROCESSING ================================

		// If LMT node corresponding to yb exists
		if (local_min != lmt.end() && local_min->y == yb) {
			// Add edges starting at this local minimum to the AET
			for (edge_node *edge = local_min->first_bound.start; edge; edge = edge->next_bound)
				aet.add_edge(edge);

			++local_min;
		}

		// Create bundles within AET
		edge_node	*e0 = 0;
		for (auto &e : aet)
			e0 = e.setup_bundles(e0, yb);

		h_state		horiz_clip = NH;
		h_state		horiz_subj = NH;

		// Set dummy previous x value
		double	px = -maximum;

		// Process each edge at this scanbeam boundary
		for (edge_node &e : aet) {
			int	exists_clip = e.bundle[ABOVE].clip + (e.bundle[BELOW].clip << 1);
			int	exists_subj = e.bundle[ABOVE].subj + (e.bundle[BELOW].subj << 1);

			if (exists_clip || exists_subj) {
				// Determine contributing status and quadrant occupancies
				vertex_type vtype_clip		= make_vertex_type(horiz_clip != NH, (horiz_clip != NH) ^ e.bundle[BELOW].clip, false, e.bundle[ABOVE].clip) ^ (bool)parity_clip;
				vertex_type vtype_subj		= make_vertex_type(horiz_subj != NH, (horiz_subj != NH) ^ e.bundle[BELOW].subj, false, e.bundle[ABOVE].subj) ^ (bool)parity_subj;
				bool		contributing	= op == GPC_XOR
					|| (exists_clip && (horiz_subj || (parity_subj ^ (op == GPC_UNION))))
					|| (exists_subj && (horiz_clip || (parity_clip ^ (op == GPC_UNION))))
					|| (exists_clip && exists_subj && parity_clip == parity_subj);

				// Set bundle side
				e.bside.clip = parity_clip;
				e.bside.subj = parity_subj;

				// Update parity
				parity_clip ^= e.bundle[ABOVE].clip;
				parity_subj ^= e.bundle[ABOVE].subj;

				// Update horizontal state
				horiz_clip = next_h_state[horiz_clip][exists_clip][parity_clip];
				horiz_subj = next_h_state[horiz_subj][exists_subj][parity_subj];

				if (contributing) {
					double	xb = e.xb;

					switch (combine(op, vtype_clip, vtype_subj)) {
						case EMN:
						case IMN:
							cf = e.outp[ABOVE] = out_poly.add_local_min(xb, yb);
							px = xb;
							break;
						case ERI:
							if (xb != px) {
								cf->add_right(xb, yb);
								px = xb;
							}
							e.outp[ABOVE] = exchange(cf, nullptr);
							break;
						case ELI:
							e.outp[BELOW]->add_left(xb, yb);
 							px = xb;
							cf = e.outp[BELOW];
							break;
						case EMX:
							if (xb != px) {
								cf->add_left(xb, yb);
								px = xb;
							}
							merge_right(cf, e.outp[BELOW], out_poly);
							cf = NULL;
							break;
						case ILI:
							if (xb != px) {
								cf->add_left(xb, yb);
								px = xb;
							}
							e.outp[ABOVE] = exchange(cf, nullptr);
							break;
						case IRI:
							e.outp[BELOW]->add_right(xb, yb);
							px = xb;
							cf = exchange(e.outp[BELOW], nullptr);
							break;
						case IMX:
							if (xb != px) {
								cf->add_right(xb, yb);
								px = xb;
							}
							merge_left(cf, e.outp[BELOW], out_poly);
							cf = e.outp[BELOW] = nullptr;
							break;
						case IMM:
							if (xb != px) {
								cf->add_right(xb, yb);
								px = xb;
							}
							merge_left(cf, e.outp[BELOW], out_poly);
							e.outp[BELOW] = NULL;
							cf = e.outp[ABOVE] = out_poly.add_local_min(xb, yb);
							break;
						case EMM:
							if (xb != px) {
								cf->add_left(xb, yb);
								px = xb;
							}
							merge_right(cf, e.outp[BELOW], out_poly);
							e.outp[BELOW] = NULL;
							cf = e.outp[ABOVE] = out_poly.add_local_min(xb, yb);
							break;
						case LED:
							if (e.bot.y == yb)
								e.outp[BELOW]->add_left(xb, yb);
							e.outp[ABOVE] = e.outp[BELOW];
							px = xb;
							break;
						case RED:
							if (e.bot.y == yb)
								e.outp[BELOW]->add_right(xb, yb);
							e.outp[ABOVE] = e.outp[BELOW];
							px = xb;
							break;
						default: break;
					}
				}
			}
		} // End of AET loop

		// Delete terminating edges from the AET, otherwise compute xt
		e0 = 0;
		for (auto &i : with_iterator(aet)) {
			auto	&e = *i;
			if (e.top.y == yb) {
				// Copy bundle head state to the adjacent tail edge if required
				if (e.bundle[BELOW].state == BUNDLE_HEAD && e0 && e0->bundle[BELOW].state == BUNDLE_TAIL) {
					e0->outp[BELOW]	 = e.outp[BELOW];
					e0->bundle[BELOW].state = UNBUNDLED;
					if (e0->prev && e0->prev->bundle[BELOW].state == BUNDLE_TAIL)
						e0->bundle[BELOW].state = BUNDLE_HEAD;
				}
				i.remove();

			} else {
				e.xt = e.top.y == yt ? e.top.x : e.bot.x + e.dx * (yt - e.bot.y);
			}
			e0 = &e;
		}

		if (scanbeam < sbt.size()) {
			// === SCANBEAM INTERIOR PROCESSING ==============================

			it_list		it(aet, yt - yb);

			// Process each node in the intersection table
			for (auto &intersect : it) {
				edge_node		*e0 = intersect.e0;
				edge_node		*e1 = intersect.e1;
				// Only generate output for contributing intersections
				if (contribution(e0, e1)) {
					polygon_node	*p	= e0->outp[ABOVE];
					polygon_node	*q	= e1->outp[ABOVE];
					double			ix	= intersect.v.x;
					double			iy	= intersect.v.y + yb;

					// Determine quadrant occupancies
					vertex_type vtype_clip	= make_vertex_type(false, e1->bundle[ABOVE].clip, e0->bundle[ABOVE].clip, e1->bundle[ABOVE].clip ^ e0->bundle[ABOVE].clip) ^ in_clip(e0, e1);
					vertex_type vtype_subj	= make_vertex_type(false, e1->bundle[ABOVE].subj, e0->bundle[ABOVE].subj, e1->bundle[ABOVE].subj ^ e0->bundle[ABOVE].subj) ^ in_subj(e0, e1);

					switch (combine(op, vtype_clip, vtype_subj)) {
						case EMN:
							e1->outp[ABOVE] = e0->outp[ABOVE] = out_poly.add_local_min(ix, iy);
							break;
						case ERI:
							if (p) {
								p->add_right(ix, iy);
								e1->outp[ABOVE] = p;
								e0->outp[ABOVE] = NULL;
							}
							break;
						case ELI:
							if (q) {
								q->add_left(ix, iy);
								e0->outp[ABOVE] = q;
								e1->outp[ABOVE] = NULL;
							}
							break;
						case EMX:
							if (p && q) {
								p->add_left(ix, iy);
								merge_right(p, q, out_poly);
								e0->outp[ABOVE] = e1->outp[ABOVE] = NULL;
							}
							break;
						case IMN:
							e1->outp[ABOVE] = e0->outp[ABOVE] = out_poly.add_local_min(ix, iy);
							break;
						case ILI:
							if (p) {
								p->add_left(ix, iy);
								e1->outp[ABOVE] = p;
								e0->outp[ABOVE] = NULL;
							}
							break;
						case IRI:
							if (q) {
								q->add_right(ix, iy);
								e0->outp[ABOVE] = q;
								e1->outp[ABOVE] = NULL;
							}
							break;
						case IMX:
							if (p && q) {
								p->add_right(ix, iy);
								merge_left(p, q, out_poly);
								e0->outp[ABOVE] = e1->outp[ABOVE] = NULL;
							}
							break;
						case IMM:
							if (p && q) {
								p->add_right(ix, iy);
								merge_left(p, q, out_poly);
								e1->outp[ABOVE] = e0->outp[ABOVE] = out_poly.add_local_min(ix, iy);
							}
							break;
						case EMM:
							if (p && q) {
								p->add_left(ix, iy);
								merge_right(p, q, out_poly);
								e1->outp[ABOVE] = e0->outp[ABOVE] = out_poly.add_local_min(ix, iy);
							}
							break;
						default: break;
					} // End of switch
				} // End of contributing intersection conditional

				// Swap bundle sides in response to edge crossing
				e1->bside.clip ^= e0->bundle[ABOVE].clip;
				e0->bside.clip ^= e1->bundle[ABOVE].clip;
				e1->bside.subj ^= e0->bundle[ABOVE].subj;
				e0->bside.subj ^= e1->bundle[ABOVE].subj;

				// Swap e0 and e1 bundles in the AET
				edge_node	*prev = e0->prev;
				if (e0->bundle[ABOVE].state == BUNDLE_HEAD) {
					while (prev != aet.begin().get() && prev->bundle[ABOVE].state == BUNDLE_TAIL)
						prev = prev->prev;
				}

				aet.erase(prev->next->iterator(), e0->next->iterator());
				e1->iterator().insert_after(prev->next->iterator(), e0->next->iterator());

			} // End of intersection loop

			// Prepare for next scanbeam
			for (auto &i : with_iterator(aet)) {
				auto		&e	= *i;
				edge_node	*succ_edge = e.succ;

				if (e.top.y == yt && succ_edge) {
					// Replace AET edge by its successor
					succ_edge->outp[BELOW]			= e.outp[ABOVE];
					succ_edge->bundle[BELOW].state	= e.bundle[ABOVE].state;
					succ_edge->bundle[BELOW].clip	= e.bundle[ABOVE].clip;
					succ_edge->bundle[BELOW].subj	= e.bundle[ABOVE].subj;
					i.replace(succ_edge);

				} else {
					// Update this edge
					e.outp[BELOW]			= e.outp[ABOVE];
					e.bundle[BELOW].state	= e.bundle[ABOVE].state;
					e.bundle[BELOW].clip	= e.bundle[ABOVE].clip;
					e.bundle[BELOW].subj	= e.bundle[ABOVE].subj;
					e.xb					= e.xt;
				}
				e.outp[ABOVE] = NULL;
			}
		}
	} // === END OF SCANBEAM PROCESSING ==================================

	// Generate result polygon from out_poly
	if (auto n = out_poly.count_contours()) {
		result.resize(n);

		int	c = 0;
		for (auto &poly : out_poly) {
			if (poly.active) {
				result[c].flag	= poly.proxy->hole;
				for (vertex_node *vtx = poly.proxy->v[LEFT], *nextv; vtx; vtx = nextv) {
					nextv = vtx->next;
					result[c].emplace_back(vtx->v);
					delete vtx;
				}
				c++;
			}
		}

	}

	return result;
}

//-----------------------------------------------------------------------------
//							 gpc_tristrip_clip
//-----------------------------------------------------------------------------

gpc_tristrip iso::gpc_tristrip_clip(gpc_op op, const gpc_polygon &subj, const gpc_polygon &clip) {
	gpc_tristrip	result;

	// Test for trivial NULL result cases
	if ((subj.empty() && clip.empty())
	|| (subj.empty() && (op == GPC_INT || op == GPC_DIFF))
	|| (clip.empty() && op == GPC_INT)
	)
		return result;

	// Identify potentially contributing contours
	if ((op == GPC_INT || op == GPC_DIFF) && subj && clip)
		minimax_test(op, subj, clip);

	// Build LMT
	sb_tree			sbtree;
	lmt_list		lmt;
	auto			s_heap = build_lmt(lmt, sbtree, subj, SUBJ, op);
	auto			c_heap = build_lmt(lmt, sbtree, clip, CLIP, op);

	// Return a NULL result if no contours contribute
	if (lmt.empty())
		return result;

	auto			sbt			= sbtree.flatten();
	SIDE			parity_clip	= op == GPC_DIFF ? RIGHT : LEFT;
	SIDE			parity_subj	= LEFT;
	auto			local_min	= lmt.begin();
	edge_list		aet;
	polygon_list	tlist;
	vertex_type		cft;

	// Process each scanbeam
	for (int scanbeam = 0; scanbeam < sbt.size();) {
		// Set yb and yt to the bottom and top of the scanbeam
		double	yb = sbt[scanbeam++];
		double	yt = scanbeam < sbt.size() ? sbt[scanbeam] : yb;

		// === SCANBEAM BOUNDARY PROCESSING ================================

		// If LMT node corresponding to yb exists
		if (local_min != lmt.end() && local_min->y == yb) {
			// Add edges starting at this local minimum to the AET
			for (edge_node *edge = local_min->first_bound.start; edge; edge = edge->next_bound)
				aet.add_edge(edge);

			++local_min;
		}

		// Create bundles within AET
		edge_node	*e0 = 0;
		for (auto &e : aet)
			e0 = e.setup_bundles(e0, yb);

		h_state		horiz_clip = NH;
		h_state		horiz_subj = NH;

		// Process each edge at this scanbeam boundary
		for (edge_node &e : aet) {
			int	exists_clip = e.bundle[ABOVE].clip + (e.bundle[BELOW].clip << 1);
			int	exists_subj = e.bundle[ABOVE].subj + (e.bundle[BELOW].subj << 1);

			if (exists_clip || exists_subj) {
				// Determine contributing status and quadrant occupancies
				vertex_type vtype_clip		= make_vertex_type(horiz_clip != NH, (horiz_clip != NH) ^ e.bundle[BELOW].clip, false, e.bundle[ABOVE].clip) ^ (bool)parity_clip;
				vertex_type vtype_subj		= make_vertex_type(horiz_subj != NH, (horiz_subj != NH) ^ e.bundle[BELOW].subj, false, e.bundle[ABOVE].subj) ^ (bool)parity_subj;
				bool		contributing	= op == GPC_XOR
					|| (exists_clip && (horiz_subj || (parity_subj ^ (op == GPC_UNION))))
					|| (exists_subj && (horiz_clip || (parity_clip ^ (op == GPC_UNION))))
					|| (exists_clip && exists_subj && parity_clip == parity_subj);

				// Set bundle side
				e.bside.clip = parity_clip;
				e.bside.subj = parity_subj;

				// Update parity
				parity_clip ^= e.bundle[ABOVE].clip;
				parity_subj ^= e.bundle[ABOVE].subj;

				// Update horizontal state
				horiz_clip = next_h_state[horiz_clip][exists_clip][parity_clip];
				horiz_subj = next_h_state[horiz_subj][exists_subj][parity_subj];

				if (contributing) {
					double		xb = e.xb;
					edge_node	*cf = 0;

					switch (combine(op, vtype_clip, vtype_subj)) {
						case EMN:
							e.outp[ABOVE] = tlist.add_tristrip(xb, yb);
							cf	= &e;
							break;
						case ERI:
							e.outp[ABOVE] = cf->outp[ABOVE];
							if (xb != cf->xb)
								e.add_vertex(ABOVE, RIGHT, xb, yb);
							cf = NULL;
							break;
						case ELI:
							e.add_vertex(BELOW, LEFT, xb, yb);
							e.outp[ABOVE] = NULL;
							cf	= &e;
							break;
						case EMX:
							if (xb != cf->xb)
								e.add_vertex(BELOW, RIGHT, xb, yb);
							e.outp[ABOVE] = NULL;
							cf	= NULL;
							break;
						case IMN:
							if (cft == LED) {
								if (cf->bot.y != yb)
									cf->add_vertex(BELOW, LEFT, cf->xb, yb);
								cf->outp[ABOVE] = tlist.add_tristrip(cf->xb, yb);
							}
							e.outp[ABOVE] = cf->outp[ABOVE];
							e.add_vertex(ABOVE, RIGHT, xb, yb);
							break;
						case ILI:
							e.outp[ABOVE] = tlist.add_tristrip(xb, yb);
							cf	= &e;
							cft = ILI;
							break;
						case IRI:
							if (cft == LED) {
								if (cf->bot.y != yb)
									cf->add_vertex(BELOW, LEFT, cf->xb, yb);
								cf->outp[ABOVE] = tlist.add_tristrip(cf->xb, yb);
							}
							e.add_vertex(BELOW, RIGHT, xb, yb);
							e.outp[ABOVE] = NULL;
							break;
						case IMX:
							e.add_vertex(BELOW, LEFT, xb, yb);
							e.outp[ABOVE] = NULL;
							cft	= IMX;
							break;
						case IMM:
							e.add_vertex(BELOW, LEFT, xb, yb);
							e.outp[ABOVE] = cf->outp[ABOVE];
							if (xb != cf->xb)
								cf->add_vertex(ABOVE, RIGHT, xb, yb);
							cf	= &e;
							break;
						case EMM:
							e.add_vertex(BELOW, RIGHT, xb, yb);
							e.outp[ABOVE] = NULL;
							e.outp[ABOVE] = tlist.add_tristrip(xb, yb);
							cf	= &e;
							break;
						case LED:
							if (e.bot.y == yb)
								e.add_vertex(BELOW, LEFT, xb, yb);
							e.outp[ABOVE] = e.outp[BELOW];
							cf	= &e;
							cft	= LED;
							break;
						case RED:
							e.outp[ABOVE] = cf->outp[ABOVE];
							if (cft == LED) {
								if (cf->bot.y == yb) {
									e.add_vertex(BELOW, RIGHT, xb, yb);
								} else {
									if (e.bot.y == yb) {
										cf->add_vertex(BELOW, LEFT, cf->xb, yb);
										e.add_vertex(BELOW, RIGHT, xb, yb);
									}
								}
							} else {
								e.add_vertex(BELOW, RIGHT, xb, yb);
								e.add_vertex(ABOVE, RIGHT, xb, yb);
							}
							cf = NULL;
							break;
						default: break;
					}
				}
			}
		} // End of AET loop

		// Delete terminating edges from the AET, otherwise compute xt
		e0 = 0;
		for (auto &i : with_iterator(aet)) {
			auto	&e = *i;
			if (e.top.y == yb) {
				// Copy bundle head state to the adjacent tail edge if required
				if (e.bundle[BELOW].state == BUNDLE_HEAD && e0 && e0->bundle[BELOW].state == BUNDLE_TAIL) {
					e0->outp[BELOW]			= e.outp[BELOW];
					e0->bundle[BELOW].state	= UNBUNDLED;
					if (e0->prev && e0->prev->bundle[BELOW].state == BUNDLE_TAIL)
						e0->bundle[BELOW].state = BUNDLE_HEAD;
				}
				i.remove();

			} else {
				e.xt = e.top.y == yt ? e.top.x : e.bot.x + e.dx * (yt - e.bot.y);
			}
		}

		if (scanbeam < sbt.size()) {
			// === SCANBEAM INTERIOR PROCESSING ==============================

			it_list		it(aet, yt - yb);

			// Set dummy previous x value
			double	px = -maximum;

			// Process each node in the intersection table
			for (auto &intersect : it) {
				// Only generate output for contributing intersections
				edge_node		*e0 = intersect.e0;
				edge_node		*e1 = intersect.e1;
				if (contribution(e0, e1)) {
					polygon_node	*p	= e0->outp[ABOVE];
					polygon_node	*q	= e1->outp[ABOVE];
					double			ix	= intersect.v.x;
					double			iy	= intersect.v.y + yb;

					// Determine quadrant occupancies
					vertex_type vtype_clip	= make_vertex_type(false, e1->bundle[ABOVE].clip, e0->bundle[ABOVE].clip, e1->bundle[ABOVE].clip ^ e0->bundle[ABOVE].clip) ^ in_clip(e0, e1);
					vertex_type vtype_subj	= make_vertex_type(false, e1->bundle[ABOVE].subj, e0->bundle[ABOVE].subj, e1->bundle[ABOVE].subj ^ e0->bundle[ABOVE].subj) ^ in_subj(e0, e1);

					double	nx;
					switch (combine(op, vtype_clip, vtype_subj)) {
						case EMN:
							e1->outp[ABOVE] = tlist.add_tristrip(ix, iy);
							e0->outp[ABOVE] = e1->outp[ABOVE];
							break;
						case ERI:
							if (p) {
								edge_node *prev = e0->prev_edge(ABOVE, px, iy);
								prev->add_vertex(ABOVE, LEFT, px, iy);
								e0->add_vertex(ABOVE, RIGHT, ix, iy);
								e1->outp[ABOVE] = exchange(e0->outp[ABOVE], nullptr);
							}
							break;
						case ELI:
							if (q) {
								edge_node *next = e1->next_edge(ABOVE, nx, iy);
								e1->add_vertex(ABOVE, LEFT, ix, iy);
								next->add_vertex(ABOVE, RIGHT, nx, iy);
								e0->outp[ABOVE] = exchange(e1->outp[ABOVE], nullptr);
							}
							break;
						case EMX:
							if (p && q) {
								e0->add_vertex(ABOVE, LEFT, ix, iy);
								e0->outp[ABOVE] = e1->outp[ABOVE] = NULL;
							}
							break;
						case IMN: {
							edge_node *prev = e0->prev_edge(ABOVE, px, iy);
							prev->add_vertex(ABOVE, LEFT, px, iy);
							edge_node *next = e1->next_edge(ABOVE, nx, iy);
							next->add_vertex(ABOVE, RIGHT, nx, iy);
							e1->outp[ABOVE] = prev->outp[ABOVE] = tlist.add_tristrip(px, iy);
							e1->add_vertex(ABOVE, RIGHT, ix, iy);
							next->outp[ABOVE] = e0->outp[ABOVE] = tlist.add_tristrip(ix, iy);
							next->add_vertex(ABOVE, RIGHT, nx, iy);
							break;
						}
						case ILI:
							if (p) {
								e0->add_vertex(ABOVE, LEFT, ix, iy);
								edge_node *next = e1->next_edge(ABOVE, nx, iy);
								next->add_vertex(ABOVE, RIGHT, nx, iy);
								e1->outp[ABOVE] = exchange(e0->outp[ABOVE], nullptr);
							}
							break;
						case IRI:
							if (q) {
								e1->add_vertex(ABOVE, RIGHT, ix, iy);
								edge_node *prev = e0->prev_edge(ABOVE, px, iy);
								prev->add_vertex(ABOVE, LEFT, px, iy);
								e0->outp[ABOVE] = exchange(e1->outp[ABOVE], nullptr);
							}
							break;
						case IMX:
							if (p && q) {
								e0->add_vertex(ABOVE, RIGHT, ix, iy);
								e1->add_vertex(ABOVE, LEFT, ix, iy);
								e0->outp[ABOVE] = e1->outp[ABOVE] = nullptr;
								edge_node *prev = e0->prev_edge(ABOVE, px, iy);
								prev->add_vertex(ABOVE, LEFT, px, iy);
								prev->outp[ABOVE] = tlist.add_tristrip(px, iy);
								edge_node *next = e1->next_edge(ABOVE, nx, iy);
								next->add_vertex(ABOVE, RIGHT, nx, iy);
								next->outp[ABOVE] = prev->outp[ABOVE];
								next->add_vertex(ABOVE, RIGHT, nx, iy);
							}
							break;
						case IMM:
							if (p && q) {
								e0->add_vertex(ABOVE, RIGHT, ix, iy);
								e1->add_vertex(ABOVE, LEFT, ix, iy);
								edge_node *prev = e0->prev_edge(ABOVE, px, iy);
								prev->add_vertex(ABOVE, LEFT, px, iy);
								prev->outp[ABOVE] = tlist.add_tristrip(px, iy);
								edge_node *next = e1->next_edge(ABOVE, nx, iy);
								next->add_vertex(ABOVE, RIGHT, nx, iy);
								e1->outp[ABOVE] = prev->outp[ABOVE];
								e1->add_vertex(ABOVE, RIGHT, ix, iy);
								e0->outp[ABOVE] = tlist.add_tristrip(ix, iy);
								next->outp[ABOVE] = e0->outp[ABOVE];
								next->add_vertex(ABOVE, RIGHT, nx, iy);
							}
							break;
						case EMM:
							if (p && q) {
								e0->add_vertex(ABOVE, LEFT, ix, iy);
								e0->outp[ABOVE] = e1->outp[ABOVE] = tlist.add_tristrip(ix, iy);
							}
							break;
						default: break;
					}
				}

				// Swap bundle sides in response to edge crossing
				e1->bside.clip ^= e0->bundle[ABOVE].clip;
				e0->bside.clip ^= e1->bundle[ABOVE].clip;
				e1->bside.subj ^= e0->bundle[ABOVE].subj;
				e0->bside.subj ^= e1->bundle[ABOVE].subj;

				// Swap e0 and e1 bundles in the AET
				edge_node	*prev = e0->prev;
				if (e0->bundle[ABOVE].state == BUNDLE_HEAD) {
//					while (prev != aet.begin() && prev->bundle[ABOVE].state == BUNDLE_TAIL)
					while (prev != aet.begin().get() && !prev->bundle[ABOVE].clip && !prev->bundle[ABOVE].subj && prev->bundle[ABOVE].state != BUNDLE_HEAD)
						prev = prev->prev;
				}
				aet.erase(prev->next->iterator(), e0->next->iterator());
				e1->iterator().insert_after(prev->next->iterator(), e0->next->iterator());

			} // End of intersection loop

			// Prepare for next scanbeam
			for (auto &i : with_iterator(aet)) {
				auto		&e	= *i;
				edge_node	*succ_edge = e.succ;

				if (e.top.y == yt && succ_edge) {
					// Replace AET edge by its successor
					succ_edge->outp[BELOW]			= e.outp[ABOVE];
					succ_edge->bundle[BELOW].state	= e.bundle[ABOVE].state;
					succ_edge->bundle[BELOW].clip	= e.bundle[ABOVE].clip;
					succ_edge->bundle[BELOW].subj	= e.bundle[ABOVE].subj;
					i.replace(succ_edge);

				} else {
					// Update this edge
					e.outp[BELOW]			= e.outp[ABOVE];
					e.bundle[BELOW].state	= e.bundle[ABOVE].state;
					e.bundle[BELOW].clip	= e.bundle[ABOVE].clip;
					e.bundle[BELOW].subj	= e.bundle[ABOVE].subj;
					e.xb					= e.xt;
				}
				e.outp[ABOVE] = NULL;
			}
		}
	} // === END OF SCANBEAM PROCESSING ==================================

	// Generate result tristrip from tlist
	if (auto n = tlist.count_tristrips()) {
		result.resize(n);

		int	s = 0;
		for (auto &tn : tlist) {
			if (tn.active > 2) {
				// Valid tristrip: copy the vertices
				gpc_vertex	*v = result[s++].resize(tn.active);
				for (vertex_node *lt = tn.v[LEFT], *rt = tn.v[RIGHT]; lt || rt; ) {
					if (lt) {
						*v++ = lt->v;
						lt = lt->next;
					}
					if (rt) {
						*v++ = rt->v;
						rt = rt->next;
					}
				}
			}
			// free the heap
			for (vertex_node *lt = tn.v[LEFT], *ltn; lt; lt = ltn) {
				ltn = lt->next;
				delete lt;
			}
			for (vertex_node *rt = tn.v[RIGHT], *rtn; rt; rt = rtn) {
				rtn = rt->next;
				delete rt;
			}
		}
	}

	return result;
}

gpc_tristrip iso::gpc_polygon_to_tristrip(const gpc_polygon &s) {
	gpc_polygon c;
	return gpc_tristrip_clip(GPC_DIFF, s, c);
}
