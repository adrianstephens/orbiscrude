#ifndef SAFE_ARRAY
#define SAFE_ARRAY

#include "base/array.h"

namespace iso {

//-----------------------------------------------------------------------------
// class safe_array - entries never get relocated
// random access
//-----------------------------------------------------------------------------

template<typename T> class safe_array {
	int			curr_size;
	int			curr_depth;
	void*		p;

	struct tree_node {
		void	*child[32];
		tree_node() { iso::clear(child); }
	};
	struct leaf_node {
		space_for<T>	entry[32];
		uint32			exist;
		leaf_node() : exist(0) {}
		~leaf_node() {
			for (uint32 i = exist; i; i = clear_lowest(i))
				destruct(entry[lowest_set_index(i)]);
		}
	};

	static constexpr int	depth0(int i)	{ return i < 32 ? 0 : i < 32 * 32 ? 1 : 2; }
	static constexpr int	depth(int i)	{ return i < 32 * 32 * 32 ? depth0(i) : depth0(i & (32 * 32 * 32 - 1)) + 3; }

	static void	free_block(void *b, int d) {
		if (d == 0) {
			delete (leaf_node*)b;
		} else {
			auto	n = (tree_node*)b;
			for (auto &i : n->child) {
				if (i)
					free_block(i, d - 1);
			}
			delete n;
		}
	}

	leaf_node	*make_leaf(int i) {
		int		d	= depth(i - 1);
		while (curr_depth < d) {
			auto	p1		= new tree_node;
			p1->child[0]	= p;
			p				= p1;
			curr_depth++;
		}

		if (void *p0 = p) {
			for (int dc = curr_depth; dc; dc--) {
				auto&	p1 = ((tree_node*)p0)->child[(i >> (dc * 5)) & 31];
				if (!p1)
					p1 = dc > 1 ? (void*)new tree_node : (void*)new leaf_node;
				p0 = p1;
			}
			return (leaf_node*)p0;
		}
		return (leaf_node*)(p = new leaf_node);
	}
	leaf_node	*get_leaf(int i) const {
		void *p0 = p;
		if (p0) {
			for (int dc = curr_depth; dc; dc--)
				p0 = ((tree_node*)p0)->child[(i >> (dc * 5)) & 31];
		}
		return (leaf_node*)p0;
	}

public:
	safe_array() : curr_size(0), curr_depth(0), p(0)	{}
	~safe_array()	{
		if (p)
			free_block(p, curr_depth);
	}
	void	clear() {
		if (p) {
			free_block(p, curr_depth);
			p = 0;
			curr_depth = 0;
		}
	}

	optional<T&>	get(int i) {
		if (auto leaf = get_leaf(i)) {
			i &= 31;
			if (leaf->exist & bit(i))
				return (T&)p1->entry[i];
		}
		return none;
	}

	template<typename...U> T&	put(int i, U&&...u) {
		auto	leaf = make_leaf(i);
		i &= 31;
		if (leaf->exist & bit(i))
			return (T&)leaf->entry[i] = T(forward<U>(u)...);

		leaf->exist |= bit(i);
		return *new(leaf->entry + i) T(forward<U>(u)...);
	}

	putter<safe_array,int,optional<T&>>	operator[](int i)	{ return {*this, i}; }
	auto			operator[](int i)	const	{ return get(i); }
};

} //namespace iso

#endif //SAFE_ARRAY
