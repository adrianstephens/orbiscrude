#include "base/vector.h"
#include "iso/iso_convert.h"
#include "vision/contours.h"
#include "extra/random.h"
#include "extra/indexer.h"
#include "maths/graph.h"
//#include "extra/gjk.h"
#include "maths/simplex.h"
#include "bitmap/bitmap.h"
#include "filetypes/3d/model_utils.h"
#include "maths/statistics.h"

namespace iso {
template<typename T> ISO_DEFUSERCOMPT(contour, T, 2) {
	ISO_SETBASE(0, hierarchy<contour<T> >);
	ISO_SETFIELD(1, points);
}};
}
using namespace iso;

template<typename T> void FillVMips(block<T, 2> &dest) {
	int	mips	= MaxMips(dest);
	for (int mx = 1; mx < mips; ++mx) {
		for (int my = 1; my < mips - mx; ++my)
			BoxFilter(GetMip2(dest, mx, my - 1), GetMip2(dest, mx, my), false);
	}
}

void VBoxFilter(const block<ISO_rgba, 2> &srce, const block<ISO_rgba, 2> &dest) {
	int	w = max(srce.size<1>(), 1u), h0 = max(srce.size<2>(), 1u), h1 = max(h0 >> 1, 1);
	for (int y = 0; y < h1; y++) {
		ISO_rgba	*s0 = srce[y * 2].begin(), * s1 = srce[y * 2 + int(y * 2 < h0 - 1)].begin();
		ISO_rgba	*d	= dest[y * 1].begin();

		for (int x = w; x--; d++, ++s0, ++s1) {
			*d = ISO_rgba(
				(s0[0].r + s1[0].r) / 2,
				(s0[0].g + s1[0].g) / 2,
				(s0[0].b + s1[0].b) / 2,
				(s0[0].a + s1[0].a) / 2
			);
		}
	}
}

template<typename T> block<T, 2> GetVMip(const block<T, 2> &b, int n, int t) {
	int	w = b.template size<1>() / t, h = b.template size<2>();
	return b.template sub<1>(w * n, w).template sub<2>(0, max(h >> n, 1));
}

/*
template<typename D, typename S, int N> void copyb(const block<S, N> &s, D &d) {
	auto	j = d.begin();
	for (auto i = s.begin(), ie = i + min(s.size(), d.size()); i != ie; ++i, ++j)
		copyb(*i, *j);
}

template<typename D, typename S> void copyb(const block<S, 1> &s, D &d) {
	copy(s.begin(), s.begin() + min(s.size(), d.size()), d.begin());
}
*/

template<typename T> T sample(const block<T, 2> &b, param(float2) p) {
	float4	w	= bilinear_weights(frac(p));
	int		x	= int(p.x), y = int(p.y);

	return	HDRpixel(
			b[y + 0][x + 0] * w.x
		+	b[y + 0][x + 1] * w.y
		+	b[y + 1][x + 0] * w.z
		+	b[y + 1][x + 1] * w.w
	);
}

template<typename S, typename D> void draw(block<D, 2> &dest, block<S, 2> &srce, param(float2x3) mat) {
	position2	size	= position2(int(srce.template size<1>()), int(srce.template size<2>()));
	float2		size1	= size - one;
	obb2		o		= mat * rectangle(position2(zero), size);
	rectangle	r		= o.get_box();// & rectangle(float2(zero), float2(int(dest.size<1>()), int(dest.size<2>())));
	int			x0		= r.a.v.x, x1 = r.b.v.x;
	int			y0		= r.a.v.y, y1 = r.b.v.y;
	int			dw		= dest.template size<1>(), dh = dest.template size<2>();

	float2x3	imat	= inverse(mat);

	for (int y = y0; y < y1; y++) {
		int	y2 = wrap(y, dh);
		for (int x = x0; x < x1; x++) {
			position2	p = imat * position2(x, y);
			if (all(p.v < size.v) && all(p.v >= -half)) {
				dest[y2][wrap(x, dw)] *= sample(srce, clamp(p.v, zero, size1));
			}
		}
	}
}

double get_average(const block<ISO_rgba, 2> &b) {
	uint64	total = 0;
	for_each(b, [&total](const ISO_rgba &p) {
		total += 255 - p.r;
	});
	return double(total) / double(b.size<1>() * b.size<2>());
}

struct StrokeParams {
	bitmap			*bm;
	int				count;
	stats0<float>	angle;
	stats0<float2>	size;

	StrokeParams(bitmap *_bm) : bm(_bm), count(0) {}
	void	add(const obb2 &o) {
		angle.add(get_euler(o));
		size.add(o.extent());
		++count;
	}
};

struct Stroke {
	bitmap	bm;
	uint64	blackness;

	Stroke(const param_element<contour<position2>&, StrokeParams&> &x) {
		const contour<position2>	&c	= x;
		obb2				o	= make_point_cloud(dynamic_array<float2>(c.points)).get_obb();
		x.p.add(o);

		float4	t	= square(o.v());
		float2	ext	= sqrt(t.xz + t.yw);
		int		w	= ext.x, h = ext.y;
		bm.Create(w * 8, h);
		fill(bm.All(), ISO_rgba(255,255,255,255));

		auto	srce = x.p.bm->All();
		auto	dest = bm.All();
		float2x3	mat	= o.matrix() * translate(position2(-one)) * scale(2) * inverse(scale(ext));
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				dest[y][x] = sample(srce, mat * position2(x, y));
			}
		}

		//vertical 'mips'
		for (int m = 1; m < 8; ++m)
			VBoxFilter(GetVMip(bm.All(), m - 1, 8), GetVMip(bm.All(), m, 8));

		uint64	total = 0;
		for_each(GetMip(0), [&total](const ISO_rgba &p) {
			total += 255 - p.r;
		});
		blackness = total;
	}
	block<ISO_rgba,2>	GetMip(int mip) const	{ return GetVMip(bm.All(), mip, 8); }
	uint32				Length()		const	{ return bm.Height(); }
};

ISO_DEFUSERCOMPV(Stroke, bm);

struct Candidate {
	int		s;			// stroke
	float	x, y, a;	//pos, angle
	Candidate(int _s, float _x, float _y, float _a) : s(_s), x(_x), y(_y), a(_a) {}
	float2x3	mat()	const { return translate(x, y) * rotate2D(a); }
};

ISO_ptr<void> hatching(ISO_ptr<bitmap> bm, int size, int levels, int candidates) {
	if (size == 0)
		size = 256;
	if (levels == 0)
		levels = 16;
	if (candidates == 0)
		candidates = 100;

	auto_block<int,2>	image = make_auto_block<int>(bm->Width(), bm->Height());
	for_each2(bm->All(), (block<int,2>&)image, [](const ISO_rgba &s, int &d) {
		d = s.r <= 240;
	});

	contour<position2>	root	= translate(float2(half)) * ExpandContour(SuzukiContour(image), 0.5f);
	MergeCloseContours(root, 4);

	// remove strokes smaller than maximum / 10
	CleanByArea(root, GetMaxArea(root) / 10);
//	return MakePtr(0, root);

	StrokeParams			params(bm);
//	dynamic_array<Stroke>	strokes = with_param_ref(root.children, params);
	dynamic_array<Stroke>	strokes = with_param(root.children, params);
//	return MakePtr(0, strokes);

	rng<simple_random>	random;
	float				ang_mean	= params.angle.mean(params.count);
	float				ang_sigma	= params.angle.sigma(params.count);
	normal_distribution<simple_random>	norms(&random, ang_mean, ang_sigma);

	ISO_ptr<bitmap>	bm2(0);
	bm2->Create(size, size * levels, BMF_MIPS, levels);
	fill(bm2->Slice(0), ISO_rgba(255,255,255,255));

	auto	test	= make_auto_block<ISO_rgba>(size * 2, size);
	int		mips	= log2_ceil(size);

	for (int i = 1; i < levels; i++) {
		int	candidates1	= candidates * levels / (i * 8 + levels);
		copy(bm2->Slice(i - 1), bm2->Slice(i));

		for (int m = mips; m--; ) {
			auto		dest	= GetMip(bm2->Slice(i), m);

			while (get_average(GetMip(bm2->Slice(i), m)) * levels < i * 255) {

				Candidate	best_cand(0,0,0,0);
				double		best_contrib	= 0;
				double		prev_contrib	= 0;

				for (int m2 = m; m2 < mips; ++m2)
					prev_contrib	+= get_average(GetMip2(bm2->Slice(i - 1), m, m2 - m));

				for (int c = 0; c < candidates1; ++c) {
					Candidate	cand(random(strokes.size32()), random, random, norms);
					Stroke		&stroke			= strokes[cand.s];
					double		contribution	= -prev_contrib;

					for (int m2 = m; m2 < mips; ++m2) {
						auto		dest1	= GetMip2(test.get(), m, m2 - m);
						copy(GetMip2(bm2->Slice(i), m, m2 - m), dest1);
						auto		srce1	= stroke.GetMip(m2);
						float2x3	mat2	= cand.mat();
						mat2.z *= float2{float(dest1.size<1>()), float(dest1.size<2>())};

						draw(dest1, srce1, mat2);
						contribution	+= get_average(dest1);
					}

					contribution	/= stroke.blackness;//Length();
					if (contribution > best_contrib) {
						best_cand = cand;
						best_contrib = contribution;
					}
				}

				for (int m2 = m; m2 >= 0; --m2) {
					auto		dest1	= GetMip(bm2->Slice(i), m2);
					auto		srce1	= strokes[best_cand.s].GetMip(m2);
					float2x3	mat2	= best_cand.mat();
					mat2.z *= float2{float(dest1.size<1>()), float(dest1.size<2>())};
					draw(dest1, srce1, mat2);
				}
			}
		}
	}

	return bm2;
}

//-----------------------------------------------------------------------------
// edge detection
//-----------------------------------------------------------------------------

#include "systems/mesh/model_iso.h"

struct cull_doublecone {
	float3p	origin;
	float3p	normal;
	float	seperation;
	dynamic_array<array<uint16,2> > edges;

	cull_doublecone() {}
//	cull_doublecone(const cull_doublecone &b) : origin(b.o), d(b.d)	{}
	cull_doublecone(param(position3) pos, param(vector3) dir, float cos_theta, float sep) { origin = pos.v; normal = dir / max(cos_theta, 1e-3f); seperation = sep; }
	/*
	int	contains(param(position3) p) const	{	// +1, -1, or 0
		auto	a = p - o;
		auto	b = dot(a, d.xyz);
		return	abs(b) >= d.w && square(abs(b) - d.w) >= len2(a) ? int(sign(b)) : 0;
	}*/
};

struct cull_doublecone_tree : hierarchy<cull_doublecone_tree> {
	cull_doublecone	cone;
	cull_doublecone_tree() {}
	template<typename B> cull_doublecone_tree(const B &b) : hierarchy<cull_doublecone_tree>(b), cone(b) {
		for (auto &i : b.edges)
			cone.edges.push_back(i->as_array());
	}
	template<typename B> void operator=(const B &b) {
		hierarchy<cull_doublecone_tree>::operator=(b);
		cone = b;
	}
};

ISO_DEFUSERCOMPV(cull_doublecone, origin, normal, seperation, edges);
ISO_DEFUSERCOMPV(cull_doublecone_tree, children, cone);

static const float ka_ke = 4 / 3.f;

float cos_theta_volume(float cos_theta) {
	return 1 - cos_theta;
}

position3 calc_origin(const dynamic_array<plane> &planes, param(float3) normal) {
	halfspace_intersection<float,3>	hull(planes);//, planes.size32());
	position3		*points	= alloc_auto(position3, hull.max_verts());
	auto			cloud	= hull.points(points);

	position3		origin(zero);
	float			maxd	= 0;
	for (auto &i : cloud.points()) {
		float	d = dot(i, normal);
		if (d > maxd) {
			maxd	= d;
			origin	= i;
		}
	}
	return origin;
}

struct GJK_Normals : public dynamic_array<float3> {
	mutable int	prev_i;

	position3	operator()(param(float3) v) const {
		auto	*s = iso::support(this->begin(), this->end(), v);
		prev_i	= index_of(s);
		return position3(*s);
	}
	GJK_Normals() : prev_i(0)	{}
};


struct GJK_origin3 {
	position3	operator()(param(float3) v) const	{ return position3(zero); }
};

float calc_cos_theta(GJK_Normals &normals, float3 &normal) {
	simplex_difference<3>	simp;
	GJK_origin3	origin;

	normal	= normals[0];
	position3	pa, pb;
	float		dist;
	simp.gjk(normals, origin, normals[0], 0, 0, 0, pa, pb, dist);

//	if (gjk.intersect(normals, origin, normal, 0, 1))
//		return 0;

	normal	= normalise(normal);
	return sin_cos(dot(normal, normalise(simp.get_vertex())));	// any should work
}

float calc_cos_theta(const dynamic_array<plane> &planes, float3 &normal) {
	GJK_Normals	normals;
	for (auto &i : planes)
		normals.push_back(i.normal());
	return calc_cos_theta(normals, normal);
}

struct cone_tree_builder {
	enum JOIN { NONE, PARENT, ADOPT, MERGE };

	struct vert;
	struct face;
	struct node;

	struct edge : cg::edge_mixin<edge> {
		vert*		v;		// origin vertex
		face*		f;		// left f
		int			id;
		node		*n;
		edge(edge *e) : cg::edge_mixin<edge>(e), v(0), f(0), n(0) {}

		vert*	vert0()	const	{ return v; }
		vert*	vert1()	const	{ return this->flip()->v; }
		face*	face0()	const	{ return f; }
		face*	face1()	const	{ return this->flip()->f; }
		operator array<uint16,2>() const { return { vert0()->id, vert1()->id}; }
		array<uint16,2> as_array() const { return {vert0()->id, vert1()->id}; }
	};

	struct vert : position3 {
		edge	*e;
		int		id;
		vert(float3p &p) : position3(p), e(0) {}
	};

	struct face : plane {
		edge		*e;
		int			id;
		face() : e(0) {}

		int	num_edges() const {
			int	n = 0;
			edge *i = e;
			do {
				i = i->fnext;
				++n;
			} while (i != e);
			return n;
		}
	};

	struct candidate {
		static int next_id;
		int			id;
		float		cost;
		node		*n1, *n2;
		JOIN		op;
		candidate() : id(next_id++), cost(0), n1(0), n2(0), op(NONE) {}
		candidate(float _cost, node *_n1, node *_n2, JOIN _op) : id(next_id++), cost(_cost), n1(_n1), n2(_n2), op(_op) {}
		operator float() const { return cost; }
		void		update_cost();
		~candidate() {}
	};

	struct node : hierarchy<node>, graph_edges0<candidate> {
		int						id;
		float3					normal;
		float					cos_theta;
		position3				fpos, bpos;
		dynamic_array<cone_tree_builder::edge*>	edges;

		static float	children_cost(float cc0, uint32 num_children, uint32 num_edges, float volume) {
			return cc0 + (ka_ke * num_children + num_edges) * volume;
		}
		static float	cost(float children_cost, float parent_vol) {
			return ka_ke + children_cost / parent_vol;
		}
		static float	pair_cost(float children_cost1, float children_cost2, float parent_vol) {
			return ka_ke * 2 + (children_cost1 + children_cost2) / parent_vol;
		}
		static float	adopt_cost(const node *n1, const node *n2, float cc1, float cc2, float vol, float pvol) {
			return cost(children_cost(cc1 + n2->children_cost(cc2), n1->num_children() + 1, n1->num_edges(), vol), pvol);
		}
		static float	parent_cost(const node *n1, const node *n2, float cc1, float cc2, float vol, float pvol) {
			return pair_cost(n1->children_cost(cc1), n2->children_cost(cc2), vol);
		}
		static float	merge_cost(const node *n1, const node *n2, float cc1, float cc2, float vol, float pvol) {
			return cost(children_cost(cc1 + cc2, n1->num_children() +  n2->num_children(), n1->num_edges() + n2->num_edges(), vol), pvol) * n1->volume() / 2;
		}

		node() : cos_theta(-1) {}

		void	update_theta();
		float	volume() const	{
			return cos_theta_volume(cos_theta);
		}
		uint32	num_edges() const {
			return edges.size32();
		}
		float	children_cost0() const {
			float	cost	= 0;
			for (auto &i : children)
				cost += i.children_cost();
			return cost;
		}
		float	children_cost(float cc0) const {
			return children_cost(cc0, num_children(), num_edges(), volume());
		}
		float	children_cost() const {
			return children_cost(children_cost0());
		}
		float	cost(float parent_vol) const {
			return cost(children_cost(), parent_vol);
		}
		void	calc_origins();
		void	merge_graph(node *n2);
		void	update_costs();

		operator cull_doublecone() const {
			return cull_doublecone(mid(fpos, bpos), normal, cos_theta, len(fpos - bpos));
		}
		~node() {
			ISO_TRACEF("delete node %p\n", this);
		}
	};

	dynamic_array<vert>			verts;
	dynamic_array<face>			faces;
	dynamic_array<edge::pair_t>	edges;

	node						root;
	dynamic_array<candidate*>	candidates;
	dynamic_array<node*>		nodes;

	cone_tree_builder() {}
	void	init(SubMesh *sm);
	edge	*make_edge(vert *v0, vert *e1);
	void	add_candidates(node *n1, node *n2);
	void	join(JOIN op, node *n1, node *n2);
	bool	validate();
	float	total_cost() const { return root.children_cost() / 2; }
};

int cone_tree_builder::candidate::next_id;

bool cone_tree_builder::validate() {
	for (auto &t : faces) {
		if (edge *e = t.e) {
			do {
				if (!e->validate())
					return false;
				if (e->f != &t)
					return false;
				e = e->fnext;
			} while (e != t.e);
		}
	}

	for (auto &v : verts) {
		if (edge *e = v.e) {
			do {
				if (!e->validate())
					return false;
				if (e->v != &v)
					return false;
				e = e->vnext;
			} while (e != v.e);
		}
	}

	for (auto &e2 : edges) {
		if (!e2.h0.validate())
			return false;
		if (!e2.h1.validate())
			return false;
	}

	for (auto &n : root.depth_first()) {
		for (auto e = n.outgoing.begin(); e != n.outgoing.end(); ++e) {
			auto *f = e.link()->flip();
			if (e->n1 != &n)
				return false;
			bool found = false;
			for (auto e2 = e->n2->incoming.begin(); !found && e2 != e->n2->incoming.end(); ++e2)
				found = e2.link() == f;
			if (!found)
				return false;
		}
		for (auto e = n.incoming.begin(); e != n.incoming.end(); ++e) {
			auto *f = e.link()->flip();
			if (e->n2 != &n)
				return false;
			bool found = false;
			for (auto e2 = e->n1->outgoing.begin(); !found && e2 != e->n1->outgoing.end(); ++e2)
				found = e2.link() == f;
			if (!found)
				return false;
		}
	}
	return true;
}

cone_tree_builder::edge *cone_tree_builder::make_edge(vert *v0, vert *v1) {
	auto	&p	= edges.push_back();
	p.h1.id = -(p.h0.id = edges.size32());
	p.h0.v	= v0;
	p.h1.v	= v1;

	if (edge *e = v0->e) {
		while (e->face0())
			e = e->vnext;
		splice(&p.h0, e);
	} else {
		v0->e	= &p.h0;
	}

	if (edge *e = v1->e) {
		while (e->face0())
			e = e->vnext;
		splice(&p.h1, e);
	}
	v1->e	= &p.h1;

	return &p.h1;
}

float node_cost(const cone_tree_builder::node &n, float pvol) {
	float	vol		= cos_theta_volume(n.cos_theta);
	float	cost	= n.edges.size32();
	for (auto &i : n.children)
		cost += node_cost(i, vol);

	return ka_ke + cost * vol / pvol;
}

float forest_cost(const cone_tree_builder::node &n) {
	float	cost = 0;
	for (auto &i : n.children)
		cost += node_cost(i, 2);
	return cost;
}


void cone_tree_builder::node::merge_graph(node *n2) {
	while (!n2->outgoing.empty()) {
		auto i			= n2->outgoing.pop_front();
		if (i->e.n2 == this) {
			i->e.op		= i->flip()->e.op	= NONE;
			i->e.cost	= i->flip()->e.cost = 0;
			//incoming.prev(i->flip())->unlink_next();
		}// else {
			i->e.n1		= i->flip()->e.n1	= this;
		//}
			outgoing.push_front(i);
	}
	while (!n2->incoming.empty()) {
		auto i			= n2->incoming.pop_front();
		if (i->e.n1 == this) {
			i->e.op		= i->flip()->e.op	= NONE;
			i->e.cost	= i->flip()->e.cost = 0;
			//outgoing.prev(i->flip())->unlink_next();
		}// else {
			i->e.n2		= i->flip()->e.n2	= this;
		//}
			incoming.push_front(i);
	}
}

void cone_tree_builder::node::update_theta() {
	GJK_Normals	normals;
	for (auto &n : depth_first()) {
		for (auto &i : n.edges) {
			normals.push_back(normalise(i->face0()->normal()));
			normals.push_back(normalise(i->face1()->normal()));
		}
	}
	cos_theta = calc_cos_theta(normals, normal);
}

void cone_tree_builder::node::calc_origins() {
	struct origins {
		dynamic_array<plane>	planes2[2];
		hash_set<face*>			set;
		void	add(param(float3) normal, face *f) {
			if (!set.check_insert(f))
				planes2[dot(normal, f->normal()) < zero].push_back(normalise(*f));
		}
	} o;

	for (auto &i : edges) {
		o.add(normal, i->face0());
		o.add(normal, i->face1());
	}
	fpos = calc_origin(o.planes2[0], normal);
	bpos = calc_origin(o.planes2[1], -normal);
}

void cone_tree_builder::node::update_costs() {
	for (auto &i : outgoing)
		i.update_cost();
	for (auto &i : incoming)
		i.update_cost();
}

void cone_tree_builder::candidate::update_cost() {
	if (n1 == n2) {
		op		= NONE;
		cost	= 0;
		return;
	}
	if (op == NONE)
		return;

	float	pvol	= n1->parent->volume();
	float	cc1		= n1->children_cost0();
	float	cc2		= n2->children_cost0();
	float	cost0	= node::pair_cost(n1->children_cost(cc1), n1->children_cost(cc2), pvol) * pvol / 2;

	GJK_Normals	normals;
	for (auto &n : n1->depth_first()) {
		for (auto &i : n.edges) {
			normals.push_back(i->face0()->normal());
			normals.push_back(i->face1()->normal());
		}
	}
	for (auto &n : n2->depth_first()) {
		for (auto &i : n.edges) {
			normals.push_back(i->face0()->normal());
			normals.push_back(i->face1()->normal());
		}
	}

	float3	normal;
	float	vol		= cos_theta_volume(calc_cos_theta(normals, normal));

	switch (op) {
		case PARENT:	cost = node::parent_cost(n1, n2, cc1, cc2, vol, pvol) - cost0; break;
		case ADOPT:		cost = node::adopt_cost(n1, n2, cc1, cc2, vol, pvol) - cost0; break;
		case MERGE:		cost = node::merge_cost(n1, n2, cc1, cc2, vol, pvol) - cost0; break;
	}
}

void cone_tree_builder::add_candidates(node *n1, node *n2) {
	float	pvol	= n1->parent->volume();//n1->parent ? n1->parent->volume() : 2;
	float	cc1		= n1->children_cost0();
	float	cc2		= n2->children_cost0();
	float	cost0	= node::pair_cost(n1->children_cost(cc1), n1->children_cost(cc2), pvol) * pvol / 2;

	GJK_Normals	normals;
	for (auto &n : n1->depth_first()) {
		for (auto &i : n.edges) {
			normals.push_back(normalise(i->face0()->normal()));
			normals.push_back(normalise(i->face1()->normal()));
		}
	}
	for (auto &n : n2->depth_first()) {
		for (auto &i : n.edges) {
			normals.push_back(normalise(i->face0()->normal()));
			normals.push_back(normalise(i->face1()->normal()));
		}
	}

	float3	normal;
	float	vol		= cos_theta_volume(calc_cos_theta(normals, normal));

	float	costa	= node::adopt_cost(n1, n2, cc1, cc2, vol, pvol);
	float	costb	= node::adopt_cost(n2, n1, cc2, cc1, vol, pvol);
	float	costp	= node::parent_cost(n1, n2, cc1, cc2, vol, pvol);
	float	costm	= node::merge_cost(n1, n2, cc1, cc2, vol, pvol);

	auto p1 = add(n1, n2, candidate(costa - cost0, n1, n2, ADOPT), candidate(costp - cost0, n1, n2, PARENT));
	candidates.push_back(&p1->h0.e);
	candidates.push_back(&p1->h1.e);

	auto p2 = add(n2, n1, candidate(costb - cost0, n2, n1, ADOPT), candidate(costm - cost0, n2, n1, MERGE));
	candidates.push_back(&p2->h0.e);
	candidates.push_back(&p2->h1.e);
}

void cone_tree_builder::join(JOIN op, node *n1, node *n2) {
	ISO_ASSERT(n1->parent == n2->parent);
	switch (op) {
		case PARENT: {
			node	*n = new node;
			nodes.push_back(n);
			n->id	= nodes.size32();
			if (node *p = n1->parent) {
				n1->detach();
				n2->detach();
				p->push_front(n);
			}
			n->push_front(n1);
			n->push_front(n2);

			n->merge_graph(n1);
			n->merge_graph(n2);

			n->update_theta();
			for (auto &i : n->children)
				i.update_costs();
			break;
		}
		case ADOPT: {
			n2->detach();
			n1->push_front(n2);
			n1->merge_graph(n2);

			n1->update_theta();
			n1->update_costs();
			for (auto &i : n1->children)
				i.update_costs();
			break;
		}
		case MERGE: {
			n1->edges.append(n2->edges);
			n1->attach(move(n2->children));
			n1->merge_graph(n2);
			delete n2;

			n1->update_theta();
			n1->update_costs();
			for (auto &i : n1->children)
				i.update_costs();
			break;
		}
	}
}

template<typename I, typename D> I unique_indices(I begin, I end, D d) {
	if (begin == end)
		return end;

	I	i		= begin;
	I	prev	= i;

	d[i.index()] = prev - begin;

	while (++i != end) {
		if (any(*prev != *i))
			*++prev = *i;
		d[i.index()] = prev - begin;
	}
	return ++prev;
}

void cone_tree_builder::init(SubMesh *sm) {
	auto	tris		= sm->indices.begin();
	int		num_faces	= sm->indices.Count();
	uint32	num_verts	= sm->NumVerts();

#if 1
	dynamic_array<int>		indices		= int_range(0u, num_verts);

	auto	c	= make_indexed_container(sm->VertComponentData<float3p>(0), indices);

	sort(c, [](const float3p &_a, const float3p &_b) {
		float3	a = _a, b = _b;
		uint8	same = bit_mask(a == b), less = bit_mask(a < b);
		return !!(less & lowest_set(~same));
	});

	dynamic_array<int>		rev_indices(num_verts);
	num_verts	= unique_indices(c.begin(), c.end(), rev_indices.begin()) - c.begin();
	verts = make_range_n(c.begin(), num_verts);

#else
	dynamic_array<int>		rev_indices	= int_range(0u, num_verts);
	verts = make_range_n(sm->VertComponentData<float3p>(0), num_verts);
#endif

	faces.resize(num_faces);

	//v - e + f = 2
	//e = v + f - 2
	edges.reserve(verts.size() + num_faces);

	for (auto &i : verts)
		i.id = verts.index_of(i);

	for (auto &i : faces)
		i.id = faces.index_of(i);

	for (auto &i : edges)
		i.h1.id = -(i.h0.id = edges.index_of(i));

	for (int i = 0; i < num_faces; i++) {
		face		&t	= faces[i];
		auto		&v	= tris[i];

		vert	*v1 = &verts[rev_indices[v[2]]];
		edge	*e1 = 0;

		for (int j = 0; j < 3; j++) {
			vert	*v0	= v1;
			edge	*e0 = v0->e;

			v1		= &verts[rev_indices[v[j]]];
			e1		= 0;

			if (e0) {
				edge	*e = e0;
				do {
					if (e->vert1() == v1) {
						e1 = e->flip();
						break;
					}
					e = e->vnext;
				} while (e != e0);
			}

			if (!e1)
				e1 = make_edge(v0, v1);

			ISO_ASSERT(!e1->f);
		}
		t.e			= e1;
		(plane&)t	= plane(verts[rev_indices[v[0]]], verts[rev_indices[v[1]]], verts[rev_indices[v[2]]]);

		ISO_ASSERT(t.num_edges() == 3);
		do {
			e1->f	= &t;
			e1 = e1->fnext;
		} while (e1 != t.e);
		validate();
	}
}

ISO_ptr<void> cullcones(ISO_ptr<Model3> model, float) {
	int	ns	= model->submeshes.Count();
	ISO_ptr<dynamic_array<cull_doublecone_tree> >	p(0, ns);
	for (int i = 0; i < ns; i++) {
		cone_tree_builder	builder;
		builder.init(model->submeshes[i]);

		for (auto &e : builder.edges) {
			auto	*n	= new cone_tree_builder::node;
			n->id		= e.h0.id;
			builder.nodes.push_back(n);
			n->edges.push_back(&e.h0);
			n->update_theta();
			e.h0.n		= e.h1.n = n;
			builder.root.push_front(n);
		}

		for (auto &v : builder.verts) {
			auto	*e0	= v.e;
			auto	*e	= e0->vnext;
			do {
				if (e->n->outgoing.empty())
					builder.add_candidates(e0->n, e->n);
				e = e->vnext;
			} while (e != e0);
		}

		float	total_cost = builder.total_cost();
		float	total_cost2 = forest_cost(builder.root);

		while (!builder.candidates.empty()) {
			builder.validate();
//			heap_make(reversed(builder.candidates), deref(less()));
			heap_make(reversed(builder.candidates), [](const cone_tree_builder::candidate *a, const cone_tree_builder::candidate *b) { return *a < *b; });

			cone_tree_builder::candidate	*c = builder.candidates.pop_back_value();
			float	cost = c->cost;
			if (cost >= 0)
				break;

//			builder.candidates.erase_unordered(builder.candidates.begin());
			builder.join(c->op, c->n1, c->n2);
			float	prev_cost = total_cost;
			total_cost	= builder.total_cost();
			total_cost2 = forest_cost(builder.root);
//			ISO_ASSERT(total_cost - prev_cost == cost);
		}

		builder.root.update_theta();
		for (auto &n : builder.root.depth_first())
			n.calc_origins();

		(*p)[i] = builder.root;
		builder.validate();
	}
	return p;
}

static initialise init(
	ISO_get_operation(hatching),
	ISO_get_operation(cullcones)
);
