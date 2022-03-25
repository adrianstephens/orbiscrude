#ifndef SCENETREE_H
#define SCENETREE_H

#include "maths/geometry.h"
#include "base/array.h"
#include "base/list.h"

#define RESTRICT __restrict
//#define RESTRICT

#define	USE_STPROFILER	(USE_PROFILER && USE_TWEAKS)

namespace iso {

struct SceneNode : aligner<16>, e_link<SceneNode> {
	SceneNode*	parent;	// root if in list
	void*		data;	// 0 if interior node
	cuboid		box;

	SceneNode*	left() 				const { return next;		}
	SceneNode*	right() 			const { return prev;		}
	SceneNode*	child(int i)		const { return (&next)[i];	}
	SceneNode*	other(SceneNode *c)	const { return (&next)[c == next];	}
	int			side(SceneNode *c)	const { return int(c == prev);		}

	bool		InTree()			const { return !next;	}
	bool		IsLeaf()			const { return !!data;	}

	void	SetChildren(SceneNode *left, SceneNode *right) {
		if (prev = left)
			left->parent = this;
		if (next = right)
			right->parent = this;
		data = NULL;
	}
	void	SetChild(int i, SceneNode *n) {
		if ((&next)[i] = n)
			n->parent = this;
	}

	SceneNode *RemoveFromTree();
	void	TreeToList(e_list<SceneNode> &list, SceneNode *parent);

	bool	Move(param(cuboid) box);
	void	Destroy();

	static SceneNode	*Make(param(cuboid) box, void *data);
};

class SceneTree : protected SceneNode {
public:
	using SceneNode::operator delete;

	struct List : e_list<SceneNode> {
		size_t		ToArray(dynamic_array<SceneNode*> &array);
		SceneNode*	ToTree();
		cuboid		GetBox() const;
	};

	List			list;

	typedef dynamic_array<void*>				items;
	typedef dynamic_array<pair<void*,void*> >	intersections;

	struct iterator {
		SceneNode		*n;
	public:
		iterator(SceneNode *_n) : n(_n)						{}
		SceneNode*	operator*()						const	{ return n;			}
		SceneNode*	operator->()					const	{ return n;			}
		bool		operator==(const iterator i2)	const	{ return n == i2.n; }
		bool		operator!=(const iterator i2)	const	{ return n != i2.n; }
		iterator	operator++() {
			if (SceneNode *left = n->left()) {
				n = left;
			} else if (SceneNode *right = n->right()) {
				n = right;
			} else for (SceneNode *prev = n; n = n->parent; prev = n) {
				SceneNode *right = n->right();
				if (prev != right) {
					n = right;
					break;
				}
			}
			return *this;
		}
	};
	iterator			begin()	{ return this;	}
	iterator			end()	{ return 0;		}

	SceneTree();
	~SceneTree();

	static void			CollectAllBigEnough(param(float4x4) frustum, const SceneNode *node, items &list, float size);
	static void			CollectAll(const SceneNode *node, items &list);
	static void 		CollectTreeTreeIntersect(intersections &intersections, const SceneNode * RESTRICT node0, const SceneNode * RESTRICT node1);

	void				CreateTree();
	SceneNode* 			Insert(param(cuboid) box, void *data);
	bool 				Move(param(cuboid) box, SceneNode *n);

	void				CollectFrustum(param(float4x4) frustum, items &vis_list, items &comp_vis_list, int flags = 0x3f);
	void 				CollectTreeIntersect_Statics(const SceneTree &other, intersections &intersections) const;
	void 				CollectTreeIntersect_Dynamics(SceneTree &other, intersections &intersections);

	bool				Empty()				const	{ return !left() && !right() && list.empty(); }
	const cuboid&		GetBox()			const	{ return box;	}
	const SceneNode*	GetRoot()			const	{ return left() && right() ? this : left() ? left() : right(); }
	List&				GetList()					{ return list;	}
};

#if 0
template<typename T> struct QuadNode : e_treenode<QuadNode<T> > {
	T*			data;	// 0 if interior node
	rectangle	box;
	QuadNode() : data(0) {}
};

template<typename T> struct QuadTree : e_tree<QuadNode<T> > {
	dynamic_array<QuadNode<T> >			nodes;

	template<typename C> QuadTree(C &data);
	~QuadTree()	{}
	const rectangle&	GetBox()	const	{ return root->get().box;	}
};

template<typename T> template<typename C> QuadTree<T>::QuadTree(C &data) {
	int		n			= data.size();
	int		max_leaves	= n;
	int		max_nodes	= (1 << log2_ceil(max_leaves)) - 1;

	nodes.reserve(max_leaves + max_nodes);

	dynamic_array<int>			indices(n);
	dynamic_array<rectangle>	rects(n);

	copy(int_range(0, n), indices);

	rectangle	*r = rects.begin();
	for (auto &i : data) {
		auto	x = i.get_extent();
		*r++ = rectangle(x.minimum(), x.maximum()).expand(float2(8, 8));
	}

	struct entry {
		int			*begin, *end;
		QuadNode<T>	*node;
	};

	entry	stack[32], *sp = stack;
	sp->begin	= indices.begin();
	sp->end		= indices.end();
	sp->node	= new(nodes) QuadNode<T>;
	++sp;

	rectangle	box;

	while (sp > stack) {
		--sp;
		int			*begin	= sp->begin, *end = sp->end;
		QuadNode<T>	*node	= sp->node;

		int			*i		= begin;
		box	= rects[*i++];
		while (i < end)
			box	|= rects[*i++];

		node->box	= box;
		int	axis	= max_component_index(box);

		int			*pivot = median(begin, end, [&](int a, int b) {
			return rects[a].centre()[axis] < rects[b].centre()[axis];
		});

		QuadNode<T>	*node2	= new(nodes) QuadNode<T>;
		node->child[0]		= node2;

		if (pivot - begin == 1) {
			node2->data	= &data[*begin];
			node2->box	= rects[*begin];
		} else {
			sp->node	= node2;
			sp->begin	= begin;
			sp->end		= pivot;
			++sp;
		}

		node2 = new(nodes) QuadNode<T>;
		node->child[1] = node2;

		if (end - pivot == 1) {
			node2->data	= &data[*pivot];
			node2->box	= rects[*pivot];
		} else {
			sp->node	= node2;
			sp->begin	= pivot;
			sp->end		= end;
			++sp;
		}
	}
	ISO_ASSERT(nodes.size() <= max_leaves + max_nodes);
}


template<typename T, typename F> void intersect(QuadTree<T> &t0, QuadTree<T> &t1, F f) {
	typedef QuadNode<T>	N;
	N	*n0 = t0.nodes.begin();
	N	*n1 = t1.nodes.begin();

	struct {
		e_treenode<N>	*a, *b;
	} stack[64], *sp = stack;

	for (;;) {
		if (overlap(n0->box, n1->box)) {
			if (n0->data)	{
				if (n1->data) {
					if (f(n0->data, n1->data)) {
						auto		x = n0->data->get_extent();
						rectangle	r = rectangle(x.minimum(), x.maximum()).expand(float2(8, 8));
						n0->box	= r;
						for (auto *s = sp; s-- > stack;)
							static_cast<N*>(s->a)->box |= r;
					}
				} else {
					sp->a = n0;
					sp->b = n1->child[1];
					sp++;
					n1 = static_cast<N*>(n1->child[0]);
					continue;
				}
			} else if (n1->data) {
				sp->a = n0->child[1];
				sp->b = n1;
				sp++;
				n0 = static_cast<N*>(n0->child[0]);
				continue;
			} else {
				sp->a = n0->child[1];
				sp->b = n1->child[1];
				sp++;
				if (n0 != n1) {
					sp->a = n0->child[1];
					sp->b = n1->child[0];
					sp++;
				}
				sp->a = n0->child[0];
				sp->b = n1->child[1];
				sp++;
				n0 = static_cast<N*>(n0->child[0]);
				n1 = static_cast<N*>(n1->child[0]);
				continue;
			}
		}
		if (sp == stack)
			break;
		--sp;
		n0 = static_cast<N*>(sp->a);
		n1 = static_cast<N*>(sp->b);
	}
}

template<typename K, typename V, typename F> void intersect(interval_tree<K, V> &t0, interval_tree<K, V> &t1, F f) {
	typedef interval_tree<K, V>::node_type	N;
	N	*n0 = t0.root;
	N	*n1 = t1.root;
	if (!n0 || !n1)
		return;

	struct {
		N	*a, *b;
	} stack[64], *sp = stack;

	for (;;) {
		if (overlap(n0->tree_interval(), n1->tree_interval())) {
			if (N *c0 = n0->child[1]) {
				if (n1->child[1]) {
					sp->a = c0;
					sp->b = n1->child[1];
					sp++;
				}
				if (n0 != n1 && n1->child[0]) {
					sp->a = c0;
					sp->b = n1->child[0];
					sp++;
				}
			}
			if (n0->child[0] && n1->child[1]) {
				sp->a = n0->child[0];
				sp->b = n1->child[1];
				sp++;
			}
			if (f(n0->val, n1->val)) {
				//fix:
				N	*n	= n0;
				K	m	= n->b;
				if (m > n->max) {
					for (auto *s = sp; s-- > stack && m > s->a->max;)
						s->a->max = m;
				}/* else {
					for (auto *s = sp; s-- > stack && m < s->a->max;) {
						if ((n = s->a->child[side]) && n->max > m)
							m = n->max;
					}
				}*/
			}
			n0 = n0->child[0];
			n1 = n1->child[0];
			if (n0 && n1)
				continue;
		}
		if (sp == stack)
			break;
		--sp;
		n0 = sp->a;
		n1 = sp->b;
	}
}
#endif


} // namespace iso
#endif	// SCENETREE_H
