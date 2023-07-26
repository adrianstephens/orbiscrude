#include "octree.h"
#include "base/bits.h"
#include "base/algorithm.h"
#include "utilities.h"

namespace iso {

//-----------------------------------------------------------------------------
//	generic
//-----------------------------------------------------------------------------

template<typename N, typename F> float octree_find_point(N *root, param(float3) p, param(float3) scale, float maxd, F process) {
	float	d		= maxd;
	float3	centre	= zero;
	float3	size	= scale;

	struct entry {
		float3	centre, size;
		N		*n;
		entry() {}
		entry(param(float3) centre, param(float3) size, N *n) : centre(centre), size(size), n(n) {}
	} stack[32], *sp = stack;

	for (N *n = root;;) {
//		if (all(abs(p - centre) < size + d)) {	// didn't seem to help
			process(n, d);
			int		m = bit_mask(p - centre > d) | (bit_mask(centre - p > d) << 3);
			int		a = bit_mask(p < centre);	// order so best child goes to top of stack

			size /= 2;
			for (uint8 c = 0; c < 8; c++) {
				uint8	c1 = c ^ a;
				if (N* child = n->child[c1]) {
					if (!(m & ((c1 + 1) * 7)))
						*sp++ = entry(centre + select(c1, size, -size), size, child);
				}
			}
//		}
		if (sp == stack)
			break;

		--sp;
		n		= sp->n;
		centre	= sp->centre;
		size	= sp->size;
	}
	return d;
}

template<typename N, typename F> float octree_shoot_ray(N *root, param(float3) p, param(float3) d, param(float3) scale, F process) {
	uint8	a	= bit_mask(d < zero);
	float3	p1	= p * sign1(d);
	float3	div	= reciprocal(abs(d));
	div			= select(div == div, div, infinity);

	float3	t0	= (-scale - p1) * div;
	float3	t1	= ( scale - p1) * div;

	float	t	= reduce_min(t1);
	if (reduce_max(t0) >= t)
		return -1;

	struct entry {
		float3	t0, t1;
		N		*n;
		int		child;
		entry() {}
		entry(param(float3) t0, param(float3) t1, N *n, int child) : t0(t0), t1(t1), n(n), child(child) {}
	} stack[32], *sp = stack;

	for (N *n = root;;) {
		float3	tm;
		uint8	child;

		if (n) {
			process(n, t);

			// find first child
			tm		= (t0 + t1) * half;
			tm		= select(tm == tm, tm, infinity);

			switch (max_component_index(t0)) {
				case 0:		child = (tm.y < t0.x ? 2 : 0) | (tm.z < t0.x ? 4 : 0); break;	// PLANE YZ
				case 1:		child = (tm.x < t0.y ? 4 : 0) | (tm.z < t0.y ? 1 : 0); break;	// PLANE XZ
				default:	child = (tm.x < t0.z ? 1 : 0) | (tm.y < t0.z ? 2 : 0); break;	// PLANE XY
			}

		} else {
			if (sp == stack)
				break;

			// pop stack
			--sp;
			t0		= sp->t0;
			t1		= sp->t1;
			n		= sp->n;
			child	= sp->child;
			tm		= (t0 + t1) * half;
			tm		= select(tm == tm, tm, infinity);
		}

		// find next sibling
		float3	t2		= select(child, t1, tm);
		int		next	= 1 << min_component_index(t2);
		if (!(child & next)) {
			ISO_ASSERT(sp < end(stack));
			*sp++ = entry(t0, t1, n, child | next);
		}

		t0	= select(child, tm, t0);
		t1	= t2;
		n	= all(t1 >= zero) ? n->child[child ^ a] : 0;
	}
	return t;
}

template<typename N, typename F> float octree_shoot_ray(N *root, param(float3) p, param(float3) d, param(float3) scale, float slack, F process) {
	uint8	a	= bit_mask(d < zero);
	float3	p1	= p * sign1(d);
	float3	d1	= abs(d);

	float3	div	= reciprocal(d1);
	float3	t0	= (-scale - p1) * div;
	float3	t1	= ( scale - p1) * div;

	p1	+= d1 * reduce_min(t0);

	t0	= (-scale - p1) * div;
	t1	= ( scale - p1) * div;

	float	t	= reduce_min(t1);
	if (reduce_max(t0) >= t)
		return -1;

	struct entry {
		float3	t0, t1;
		N		*n;
		entry() {}
		entry(param(float3) t0, param(float3) t1, N *n) : t0(t0), t1(t1), n(n) {}
	} stack[64], *sp = stack;

	for (N *n = root;;) {
		process(n, t);

		float3	tm	= (t0 + t1) * half;

		float3	t0x	= (slack + 1) * t0 - slack * t1;
		float3	t1x	= (slack + 1) * t1 - slack * t0;

		float3	tm0	= t0 * (half + slack) + t1 * (half - slack);
		float3	tm1	= t0 * (half - slack) + t1 * (half + slack);

		for (uint8 c = 0; c < 8; c++) {
			if (N *child = n->child[c ^ a]) {
				if (reduce_max(select(c, tm0, t0x)) < reduce_min(select(c, t1x, tm1))) {
					ISO_ASSERT(sp < end(stack));
					*sp++ = entry(select(c, tm, t0), select(c, t1, tm), child);
				}
			}
		}

		if (sp == stack)
			break;

		--sp;
		t0		= sp->t0;
		t1		= sp->t1;
		n		= sp->n;
	}
	return t;
}

float4 dir(param(float4) a, param(float4) b) {
	return b * a.w - a * b.w;
}

struct rec {
	static constexpr float	minv = 2;
	static constexpr float	maxv = 0x3.fffffcp0;
	
	static uint64	interleaved1(uint32 i)	{
		return part_bits<1, 2, 19>(i >> 4);
	}
	static uint64	interleaved(param(float3) p) {
		iorf	*i = (iorf*)&p;
		return ((interleaved1(i[0].m) << 5) | (interleaved1(i[1].m) << 6) | (interleaved1(i[2].m) << 7));
	}
	static uint64	interleaved(param(float3) p, int depth) {
		return (interleaved(p) & ~bits64((19 - depth) * 3 + 5)) | depth;
	}

	uint64	val;	// 19 * 3 + 5
	rec() {}
	rec(const iso::cuboid& box) : val(interleaved(box.centre() + 3, max(-iorf(reduce_max(box.extent())).get_exp(), 0))) {};
	rec(param(float3) v) : val(interleaved(max(min(v + 3, maxv), minv))) {}

	uint8	depth() const {
		return val & 31;
	}
	uint8	child(int d) const {
		return (val >> ((18 - d) * 3 + 5)) & 7;
	}
	operator uint64() const {
		return val;
	}
	bool operator==(const rec &b) const {
		return val == b.val;
	}
};

//-----------------------------------------------------------------------------
//	octree
//-----------------------------------------------------------------------------

bool check_octree(octree::node *n, param(position3) centre, param(float3) scale, float slack, cuboid *exts) {

	cuboid	ext	= cuboid::with_centre(centre, scale * (one + slack));

	for (auto &i : n->indices) {
		if (!ext.contains(exts[i]))
			return false;
	}

	float3	child_scale	= scale * half;
	for (uint8 c = 0; c < 8; c++) {
		if (auto *child = n->child[c]) {
			if (!check_octree(child, centre + select(c, child_scale, -child_scale), child_scale, slack, exts))
				return false;
		}
	}

	return true;
}

void octree::init(cuboid *exts, int num) {
	if (num == 0)
		return;

	max_depth	= (log2_ceil(num) + 1) / 2;
	indices		= int_range(num);

	cuboid	ext(empty);
	for (int i = 0; i < num; i++)
		ext |= exts[i];

	centre	= ext.centre();
	scale	= ext.half_extent();
	scale	= select(scale != zero, scale, one);

	temp_array<rec>	recs	= transformc(make_range_n(exts, num), [centre=centre, iscale = reciprocal(scale)](const iso::cuboid &box) {
		return (box - centre) * iscale;
	});
		
	sort(make_indexed_container(recs.begin(), indices));

	nodes.resize(num * 2);
	node	*free_node	= nodes.begin() + 1;
	for (int i = 0; i < num;) {
		int			i0		= i++;
		rec			r		= recs[indices[i0]];
		while (i < num && recs[indices[i]] == r)
			++i;

		node		*n		= nodes.begin();
		for (int d = 0, d1 = min(r.depth(), max_depth); d < d1; d++) {
			int	c	= r.child(d);
			if (!n->child[c])
				n->child[c] = free_node++;
			n = n->child[c];
		}
		n->indices	= indices.slice(i0, i - i0);
	}
	nodes.resize(free_node - nodes.begin());

	ISO_ASSERT(check_octree(nodes.begin(), centre, scale, 1.f, exts));
	root = nodes.begin();
}

int octree::find(param(position3) p, const cb_check_dist &check_dist, float maxd) {
	int		besti	= -1;
	octree_find_point(root, p - centre, scale, maxd, [&](const node *n, float &d) {
		for (auto &i : n->indices) {
			if (check_dist(i, p, d))
				besti = i;
		}
	});

	return besti;
}


int	octree::shoot_ray(param(ray3) ray, const cb_calc_dist &calc_dist) const {
	int		besti	= -1;
	octree_shoot_ray(root, ray.p - centre, ray.d, scale,
		[&](node *n, float &t) {
		for (auto &i : n->indices) {
			if (calc_dist(i, ray, t))
				besti = i;
		}
	});

	return besti;
}

int	octree::shoot_ray(param(float4x4) mat, const cb_calc_dist &calc_dist) const {
	float4x4	imat = inverse(mat);
	return shoot_ray(ray3(project(imat.w), float3(dir(imat.w, imat.z).xyz)), calc_dist);
}

int	octree::shoot_ray(param(ray3) ray, float slack, const cb_calc_dist &calc_dist) const {
	int		besti	= -1;
	octree_shoot_ray(root, ray.p - centre, ray.d, scale, slack,
		[&](node *n, float &t) {
		for (auto &i : n->indices) {
			if (calc_dist(i, ray, t))
				besti = i;
		}
	});

	return besti;
}

int	octree::shoot_ray(param(float4x4) mat, float slack, const cb_calc_dist &calc_dist) const {
	float4x4	imat = inverse(mat);
	float		neg	= sign1(imat.w.w);
	return shoot_ray(ray3(project(imat.w - imat.z), float3(dir(imat.w - imat.z, imat.z).xyz * neg)), slack, calc_dist);
}

//-----------------------------------------------------------------------------
//	dynamic_octree
//-----------------------------------------------------------------------------

dynamic_octree::dynamic_octree(const cuboid &ext) {
	centre		= ext.centre();
	scale		= ext.half_extent();
}

void dynamic_octree::add(const cuboid &ext, void *item) {
	rec	r	= rec(cuboid::with_centre(position3((ext.centre() - centre) / scale), float3(ext.extent() / scale)));

	node	*n	= &root;
	for (int d = 0; d < r.depth(); d++) {
		int	c	= r.child(d);
		if (!n->child[c])
			n->child[c] = new node;
		n = n->child[c];
	}
	n->items.push_back(item);
}

void dynamic_octree::add(param(position3) v, void *item) {
	rec	r	= rec((v - centre) / scale);

	node	*n	= &root;
	for (int d = 0;; d++) {
		int	c	= r.child(d);
		if (!n->child[c]) {
			if (n->items.size() < 8) {
				n->items.push_back(item);
				break;
			}
			n->child[c] = new node;
		}
		n = n->child[c];
	}
}

void* dynamic_octree::find(param(position3) p, const cb_check_dist &check_dist, float maxd) {
	void	*besti	= 0;
	octree_find_point(&root, p - centre, scale, maxd, [&](const node *n, float &d) {
		for (auto &i : n->items) {
			if (check_dist(i, p, d))
				besti = i;
		}
	});

	return besti;
}


void *dynamic_octree::shoot_ray(param(ray3) ray, const cb_calc_dist &calc_dist) const {
	void	*besti	= 0;
	octree_shoot_ray(&root, ray.p - centre, ray.d, scale, [&](const node *n, float &t) {
		for (auto &i : n->items) {
			if (calc_dist(i, ray, t))
				besti = i;
		}
	});

	return besti;
}

void *dynamic_octree::shoot_ray(param(float4x4) mat, const cb_calc_dist &calc_dist) const {
	float4x4	imat = inverse(mat);
	return shoot_ray(ray3(project(imat.w), float3(dir(imat.w, imat.z).xyz)), calc_dist);
}

void *dynamic_octree::shoot_ray(param(ray3) ray, float slack, const cb_calc_dist &calc_dist) const {
	void	*besti	= 0;
	octree_shoot_ray(&root, ray.p - centre, ray.d, scale, slack, [&](const node *n, float &t) {
		for (auto &i : n->items) {
			if (calc_dist(i, ray, t))
				besti = i;
		}
	});

	return besti;
}

void *dynamic_octree::shoot_ray(param(float4x4) mat, float slack, const cb_calc_dist &calc_dist) const {
	float4x4	imat = inverse(mat);
	return shoot_ray(ray3(project(imat.w), float3(dir(imat.w, imat.z).xyz)), slack, calc_dist);
}

} // namespace iso
