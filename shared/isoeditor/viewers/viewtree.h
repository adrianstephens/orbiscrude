#include "maths/geometry.h"
#include "base/array.h"

namespace app {
using namespace iso;

//-----------------------------------------------------------------------------

struct TreeNode : rectangle, aligner<16> {
protected:
	TreeNode*	parent, *child[2];
public:
	TreeNode() : rectangle(float4(zero))				{ child[0] = child[1] = parent = 0; }
	TreeNode(param(rectangle) r) : rectangle(r)	{ child[0] = child[1] = parent = 0; }

	void		set_pos(param(position2) p)				{ b = p + extent(); a = p; }
	void		set_centre(param(position2) p)			{ set_pos(b - half_extent()); }

	int			side(TreeNode *c)	const { return int(c == child[1]);		}
	TreeNode*	other(TreeNode *c)	const { return child[c == child[0]];	}
	bool		IsLeaf()			const { return !child[0];				}
	TreeNode*	Child(int i)		const { return child[i];				}
	TreeNode*	Parent(int i)		const { return parent;					}

	void	Remove() {
		TreeNode *p	= parent;
		if (TreeNode *g = p->parent) {
			g->SetChild(g->side(p), p->other(this));
			delete p;
		} else {
			p->SetChild(p->side(this), 0);
		}
	}
	void	Insert(TreeNode *n) {
		float2	c	= n->centre();
		for (TreeNode *p = this, *p2; ; p = p2) {
			float2	d0	= p->child[0]->centre() - c;
			float2	d1	= p->child[1]->centre() - c;
			int		s	= abs(d0.x) + abs(d0.y) > abs(d1.x) + abs(d1.y);

			*p	|= *n;
			p2	= p->child[s];

			if (p2->IsLeaf()) {
				TreeNode	*p3	= new TreeNode(*p2 | *n);
				p3->SetChildren(p2, n);
				p->SetChild(s, p3);
				return;
			}
		}
	}
	TreeNode	*Next1() {
		for (TreeNode *p = this, *n; n = p->parent; p = n) {
			TreeNode *c = n->child[1];
			if (p != c)
				return c;
		}
		return 0;
	}
	TreeNode	*Next0() {
		return child[0] ? child[0] : child[1] ? child[1] : Next1();
	}

	void	SetChildren(TreeNode *left, TreeNode *right) {
		if (child[0] = left)
			left->parent = this;
		if (child[1] = right)
			right->parent = this;
	}
	void	SetChild(int i, TreeNode *n) {
		if (child[i] = n)
			n->parent = this;
	}

	struct sort_x {	bool operator()(TreeNode *a, TreeNode *b)	{ return a->centre().v.x < b->centre().v.x; } };
	struct sort_y {	bool operator()(TreeNode *a, TreeNode *b)	{ return a->centre().v.y < b->centre().v.y; } };
};
//-----------------------------------------------------------------------------


struct Tree : TreeNode {
	typedef dynamic_array<pair<const TreeNode*, const TreeNode*> >	intersections;

	struct iterator {
		TreeNode	*n;
	public:
		iterator(TreeNode *_n) : n(_n)						{}
		operator TreeNode*()						const	{ return n;			}
		TreeNode*	operator->()					const	{ return n;			}
		bool		operator==(const iterator i2)	const	{ return n == i2.n; }
		bool		operator!=(const iterator i2)	const	{ return n != i2.n; }
		iterator	skip()			{ n = n->Next1(); return *this; }
		iterator	operator++()	{ n = n->Next0(); return *this; }
	};
	iterator			begin()	{ return this;	}
	iterator			end()	{ return 0;		}

	static TreeNode*	Init(TreeNode **begin, TreeNode **end);
	static TreeNode*	Partition(TreeNode *node, TreeNode **begin, TreeNode **end);

	Tree() {}

	void	Init(TreeNode **leafs, int n) {
		TreeNode	*stack[64], **sp = stack;
		for (TreeNode *i = this;;) {
			if (i->IsLeaf()) {
				if (sp == stack)
					break;
				i		= *--sp;
			} else {
				TreeNode *p = i;
				*sp++	= i->Child(1);
				i		= i->Child(0);
				if (p == this)
					p->SetChildren(0, 0);
				else
					delete p;
			}
		}

		if (n == 1) {
			SetChild(0, *leafs);
			*(rectangle*)this = *(rectangle*)*leafs;
		} else if (n) {
			Partition(this, leafs, leafs + n);
		}
	}
	void	Remove(TreeNode *n) { n->Remove(); }

	TreeNode	*Find(param(position2) p);
	void		CollectIntersections(intersections &i, param(float2) exp) const;
};

} // namespace app