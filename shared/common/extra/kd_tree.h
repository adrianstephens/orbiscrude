#ifndef KD_TREE_H
#define KD_TREE_H

#include "base/defs.h"
#include "base/vector.h"
#include "base/interval.h"

namespace iso {

//-----------------------------------------------------------------------------
//	k-kd_tree
//-----------------------------------------------------------------------------

struct kd_leaf {
	int			axis;
	range<int*>	indices;
	kd_leaf(int *begin, int *end) : axis(-1), indices(begin, end) {}
};

template<typename C, typename V=element_t<noconst_t<noref_t<C>>>, typename T=element_t<V>> struct kd_tree {
	struct kd_node {
		int		axis;
		T		split;
		kd_node *child[2];
	};

	struct iterator {
		int		*index	= nullptr;
		kd_leaf	*leaf	= nullptr;
		const kd_node	*stack[32], **sp = stack;

		iterator(const kd_node *node, const C &data, const V &x);
		iterator(const kd_node *node, const C &data, uint32 index);
		int		operator*()	const { return *index; }
		void	remove(int threshold = 16) {
			leaf->indices.erase_unordered(index);
			index = nullptr;
			if (leaf->indices.size32() < threshold) {
				//merge down?
			}
		}
	};

	C						data;
	dynamic_array<int>		indices;
	dynamic_array<kd_leaf>	leaves;
	dynamic_array<kd_node>	nodes;

	kd_tree() {}
	template<typename C2> kd_tree(const C2 &_data, int threshold = 16, bool use_median = false) : data(unconst(_data)) {
		init(threshold, use_median);
	}
	template<typename C2> void init(const C2 &_data, int threshold = 16, bool use_median = false) {
		data = _data;
		init(threshold, use_median);
	}
	void	init(int threshold = 16, bool use_median = false);
	int		nearest_neighbour(const V &x, float &_min_dist) const;
	auto	find(const V& x)	const { return iterator(nodes.begin(), data, x); }
	auto	find(uint32 index)	const { return iterator(nodes.begin(), data, index); }
};

template<typename C> kd_tree<C> make_kd_tree(C &&c, int threshold = 16) { return {forward<C>(c), threshold}; }

template<typename C, typename V, typename T> void kd_tree<C,V,T>::init(int threshold, bool use_median) {
	int		n			= num_elements32(data);
	int		max_leaves	= div_round_up(n, (threshold + 1) / 2);
	int		max_nodes	= (1 << log2_ceil(max_leaves)) - 1;

	leaves.reserve(max_leaves);
	nodes.reserve(max_nodes);
	indices = int_range(0, n);

	struct entry {
		range<int*>	indices;
		kd_node*	node;
	};

	entry	stack[32], *sp = stack;
	sp->indices	= indices;
	sp->node	= &nodes.push_back();
	++sp;

	while (sp > stack) {
		--sp;

		auto	begin	= sp->indices.begin();
		auto	end		= sp->indices.end();

		int		axis;
		T		split;
		int*	pivot;

		if (use_median) {
			auto	box		= get_extent<V>(make_indexed_container(data, sp->indices));
			// split along axis with largest extent
			axis	= max_component_index(box);
			pivot	= median(sp->indices, [&](int a, int b) { return data[a][axis] < data[b][axis]; });
			split	= data[pivot[-1]][axis];
			while (pivot < end - 1 && data[*pivot][axis] == split)
				++pivot;
		
			split	= (split + data[*pivot][axis]) / 2;

		} else {
			// gather statistics on the points in the subtree using Welford's algorithm
			V	mean	= zero, var = zero;
			int	x		= 0;
			for (auto i : sp->indices) {
				auto	delta	= data[i] - mean;
				mean	+= delta / ++x;
				var		+= delta * (data[i] - mean);
			}

			// split along axis with largest variance
			axis	= max_component_index(var);
			split	= mean[axis];
			pivot	= partition(sp->indices,  [&](int i) { return data[i][axis] > split; });
		}

		kd_node	*node	= sp->node;
		node->axis		= axis;
		node->split		= split;

		if (end - pivot <= threshold) {
			node->child[1]	= (kd_node*)&leaves.emplace_back(pivot, end);
		} else {
			sp->node		= node->child[1] = &nodes.push_back();
			sp->indices		= {pivot, end};
			++sp;
		}

		if (pivot - begin <= threshold) {
			node->child[0]	= (kd_node*)&leaves.emplace_back(begin, pivot);
		} else {
			sp->node		= node->child[0] = &nodes.push_back();
			sp->indices		= {begin, pivot};
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

		kd_leaf	*leaf = (kd_leaf*)node;
		for (auto &i : leaf->indices) {
			T	dist = dist2(data[i], x);
			if (dist < min_dist) {
				min_dist	= dist;
				min_index	= i;
			}
		}

		do {
			if (sp == stack) {
				_min_dist = min_dist;
				return min_index;
			}
			node = *--sp;
		} while	(square(x[node->axis] - node->split) >= min_dist);

		node = node->child[x[node->axis] < node->split];
	}
}

template<typename C, typename V, typename T> kd_tree<C,V,T>::iterator::iterator(const kd_node *node, const C &data, const V &x) {
	T	min_dist	= maximum;

	for (;;) {
		while (node->axis >= 0) {
			*sp++	= node;
			node	= node->child[x[node->axis] > node->split];
		}

		for (auto &i : ((kd_leaf*)node)->indices) {
			T	dist = dist2(data[i], x);
			if (dist <= min_dist) {
				min_dist	= dist;
				index		= &i;
				leaf		= (kd_leaf*)node;
			}
		}

		do {
			if (sp == stack)
				return;
			node = *--sp;
		} while	(square(x[node->axis] - node->split) > min_dist);

		node = node->child[x[node->axis] <= node->split];
	}
}

template<typename C, typename V, typename T> kd_tree<C,V,T>::iterator::iterator(const kd_node *node, const C &data, uint32 ix) {
	V	x			= data[ix];

	while (node->axis >= 0) {
		*sp++	= node;
		node	= node->child[x[node->axis] > node->split];
	}

	for (auto &i : ((kd_leaf*)node)->indices) {
		if (i == ix) {
			index	= &i;
			leaf	= (kd_leaf*)node;
			return;
		}
	}
}


} // namespace iso
#endif // KD_TREE_H
