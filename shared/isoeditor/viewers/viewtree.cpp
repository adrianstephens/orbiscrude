#include "viewtree.h"
#include "base/algorithm.h"

using namespace app;

TreeNode *Tree::Init(TreeNode **begin, TreeNode **end) {
	if (end - begin == 1)
		return *begin;
	return Partition(new TreeNode, begin, end);
}

TreeNode *Tree::Partition(TreeNode *node, TreeNode **begin, TreeNode **end) {
	rectangle	box = *(rectangle*)*begin;
	for (TreeNode **i = begin; i != end; ++i)
		box |= **i;

	TreeNode **pivot = box.extent().x > box.extent().y
		? median(begin, end, TreeNode::sort_x())
		: median(begin, end, TreeNode::sort_y());

	*(rectangle*)node	= box;
	node->SetChildren(
		Init(begin, pivot),
		Init(pivot, end)
	);
	return node;
}

TreeNode *Tree::Find(param(position2) p) {
	for (Tree::iterator i = begin(); i; ) {
		if (!i->contains(p)) {
			i.skip();
			continue;
		}
		if (i->IsLeaf())
			return i;
		++i;
	}
	return 0;
}

void Tree::CollectIntersections(intersections &i, param(float2) exp) const {
	pair<const TreeNode*, const TreeNode*>	stack[64], *sp = stack;

	const TreeNode *node0 = this, *node1 = this;

	for (;;) {
		if (overlap(node0->expand(exp), *node1)) {
			if (node0->IsLeaf())	{
				if (node1->IsLeaf()) {
					if (node1 != node0)
						i.push_back(make_pair(node0, node1));
				} else {
					sp->a = node0;
					sp->b = node1->Child(1);
					sp++;
					node1 = node1->Child(0);
					continue;
				}
			} else if (node1->IsLeaf()) {
				sp->a = node0->Child(1);
				sp->b = node1;
				sp++;
				node0 = node0->Child(0);
				continue;
			} else {
				sp->a = node0->Child(1);
				sp->b = node1->Child(1);
				sp++;
				if (node0 != node1) {
					sp->a = node0->Child(1);
					sp->b = node1->Child(0);
					sp++;
				}
				sp->a = node0->Child(0);
				sp->b = node1->Child(1);
				sp++;
				node0 = node0->Child(0);
				node1 = node1->Child(0);
				continue;
			}
		}
		if (sp == stack)
			break;
		--sp;
		node0 = sp->a;
		node1 = sp->b;
	}
}
