#ifndef KD_TREE_H
#define KD_TREE_H

#include "base/defs.h"
#include "base/vector.h"
#include "base/interval.h"

namespace iso {

//-----------------------------------------------------------------------------
//	k-kd_tree
//-----------------------------------------------------------------------------

template<typename C, typename V=typename container_traits<typename T_noconst<typename T_noref<C>::type>::type>::element, typename T=typename container_traits<V>::element> struct kd_tree {
	struct kd_node {
		int		axis;
		T		split;
		kd_node *child[2];
	};
	struct kd_leaf {
		int			axis;
		range<int*>	indices;
		kd_leaf(int *begin, int *end) : axis(-1), indices(begin, end) {}
	};

	C						data;
	dynamic_array<int>		indices;
	dynamic_array<kd_leaf>	leaves;
	dynamic_array<kd_node>	nodes;

	kd_tree() {}
	template<typename C2> kd_tree(const C2 &_data, int threshold = 16) : data(unconst(_data)) {
		init(threshold);
	}
	template<typename C2> void init(const C2 &_data, int threshold = 16) {
		data = _data;
		init(threshold);
	}
	void	init(int threshold = 16);
	int		nearest_neighbour(const V &x, float &_min_dist) const;
};

template<typename C, typename V, typename T> void kd_tree<C,V,T>::init(int threshold) {
	int		n			= data.size32();
	int		max_leaves	= div_round_up(n, (threshold + 1) / 2);
	int		max_nodes	= (1 << log2_ceil(max_leaves)) - 1;// max_leaves - 1;//log2_ceil(max_leaves);

	indices.resize(n);
	leaves.reserve(max_leaves);
	nodes.reserve(max_nodes);

	copy(int_range(0, n), indices);

	struct entry {
		int		*begin, *end;
		kd_node	*node;
	};

	entry	stack[32], *sp = stack;
	sp->begin	= indices.begin();
	sp->end		= indices.end();
	sp->node	= new(nodes) kd_node;
	++sp;

	interval<V>	box;

	while (sp > stack) {
		--sp;
		int		*begin	= sp->begin, *end = sp->end;
		kd_node	*node = sp->node;

		int		*i		= begin;
		box	= interval<V>(data[*i++]);
		while (i < end)
			box	|= data[*i++];

		int	axis	= max_component_index(box);

		int	*pivot = median(begin, end, [&](int a, int b) {
			return data[a][axis] < data[b][axis];
		});

		T	v0 = data[pivot[-1]][axis];
		while (pivot < end - 1 && data[*pivot][axis] == v0)
			++pivot;

		node->axis	= axis;
		node->split	= (v0 + data[*pivot][axis]) / 2;
//		node->split	= data[*pivot][axis];

		if (pivot - begin <= threshold) {
			node->child[0] = (kd_node*)&leaves.emplace_back(begin, pivot);
		} else {
			sp->node	= node->child[0] = new(nodes) kd_node;
			sp->begin	= begin;
			sp->end		= pivot;
			++sp;
		}

		if (end - pivot <= threshold) {
			node->child[1] = (kd_node*)&leaves.emplace_back(pivot, end);
		} else {
			sp->node	= node->child[1] = new(nodes) kd_node;
			sp->begin	= pivot;
			sp->end		= end;
			++sp;
		}
	}
	ISO_ASSERT(leaves.size() <= max_leaves);
	ISO_ASSERT(nodes.size() <= max_nodes);
}

template<typename C, typename V, typename T> int kd_tree<C,V,T>::nearest_neighbour(const V &x, float &_min_dist) const {
	T		min_dist	= maximum;
	int		min_index	= -1;

	const kd_node	*stack[32], **sp = stack;
	const kd_node	*node = nodes.begin();

	for (;;) {
		while (node->axis >= 0) {
			*sp++	= node;
			node	= node->child[x[node->axis] >= node->split];
		}

		if (node->axis < 0) {
			kd_leaf	*leaf = (kd_leaf*)node;
			for (auto &i : leaf->indices) {
				T	dist = dist2(data[i], x);
				if (dist < min_dist) {
					min_dist	= dist;
					min_index	= i;
				}
			}
		}

		do {
			if (sp == stack) {
				_min_dist = min_dist;
				return min_index;
			}
			node = *--sp;
		} while	(node->axis >= 0 && square(x[node->axis] - node->split) >= min_dist);

		node = node->child[x[node->axis] < node->split];
	}
}
} // namespace iso
#endif // KD_TREE_H
