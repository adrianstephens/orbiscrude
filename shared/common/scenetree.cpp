#include "scenetree.h"
#include "profiler.h"
#include "base/algorithm.h"
#include "allocators/pool.h"

namespace iso {

growing_pool<SceneNode>		node_pool;
//dynamic_array<SceneNode*>	temp_array;

//-----------------------------------------------------------------------------
//	SceneNode
//-----------------------------------------------------------------------------

SceneNode *SceneNode::RemoveFromTree() {
	ISO_ASSERT(IsLeaf() && InTree());

	if (parent) {
		if (SceneNode *grandparent = parent->parent) {
			grandparent->SetChild(grandparent->side(parent), parent->other(this));
			node_pool.release(parent);
			return grandparent;
		}

		parent->SetChild(parent->side(this), 0);
	}
	return parent;
}

void SceneNode::TreeToList(e_list<SceneNode> &list, SceneNode *parent) {
	SceneNode	*stack[64], **sp = stack;
	for (SceneNode *n = this;;) {
		if (n->IsLeaf()) {
			list.push_back(n);
			n->parent = parent;
			if (sp == stack)
				break;
			n	= *--sp;
		} else {
			SceneNode	*p = n;
			*sp++	= n->right();
			n		= n->left();
			node_pool.release(p);
		}
	}
}

SceneNode *SceneNode::Make(param(cuboid) box, void *data) {
	SceneNode *n = node_pool.alloc();
	n->prev		= n->next = n->parent = NULL;
	n->data		= data;
	n->box		= box;
	return n;
}

bool SceneNode::Move(param(cuboid) _box) {
	return false;
	box = _box;
	if (InTree()) {
		SceneNode	*p = RemoveFromTree();
		while (SceneNode *pp = p->parent)
			p = pp;
		parent = p;
		((SceneTree*)p)->GetList().push_back(this);
		return true;
	}

	SceneTree	*tree = (SceneTree*)parent;

	if (!tree->GetList().empty())
		tree->GetList().push_back(unlink());
	return false;
}

void SceneNode::Destroy() {
	if (InTree())
		RemoveFromTree();
	else
		unlink(); // remove from dynamics
	node_pool.release(this);
}

//-----------------------------------------------------------------------------
//	SceneTree::List
//-----------------------------------------------------------------------------

SceneNode *Init(SceneNode **begin, SceneNode **end);

void Partition(SceneNode &node, SceneNode **begin, SceneNode **end) {
	cuboid	box(empty);
	for (SceneNode **i = begin; i != end; ++i)
		box |= (*i)->box;

	float3		extent = box.extent();
	SceneNode	**pivot;
	if (extent.x > extent.y && extent.x > extent.z)
		pivot = median(begin, end, [](SceneNode *a, SceneNode *b) { return a->box.centre().v.x < b->box.centre().v.x; });
	else if (extent.y > extent.z)
		pivot = median(begin, end, [](SceneNode *a, SceneNode *b) { return a->box.centre().v.y < b->box.centre().v.y; });
	else
		pivot = median(begin, end, [](SceneNode *a, SceneNode *b) { return a->box.centre().v.z < b->box.centre().v.z; });

	node.box		= box;
	node.SetChildren(
		Init(begin, pivot),
		Init(pivot, end)
	);
}

SceneNode *Init(SceneNode **begin, SceneNode **end) {
	if (end - begin == 1) {
		SceneNode	*n = *begin;
		n->unlink();
		n->prev	= 0;
		return n;
	} else {
		SceneNode	*n = node_pool.alloc();
		Partition(*n, begin, end);
		return n;
	}
}

SceneNode *SceneTree::List::ToTree() {
	if (size_t n = size()) {
		SceneNode	**temp_array = alloc_auto(SceneNode*, n), **p = temp_array;
		for (auto &i : *this)
			*p++ = &i;
		SceneNode	*root = iso::Init(temp_array, p);
		root->parent = 0;
		return root;
	}
	return 0;
}

cuboid SceneTree::List::GetBox() const {
	cuboid	box(iso::empty);
	for (const_iterator i = begin(), e = end(); i != e; ++i)
		box |= i->box;
	return box;
}


//-----------------------------------------------------------------------------
//	SceneTree
//-----------------------------------------------------------------------------

SceneTree::SceneTree() {
	clear(*(SceneNode*)this);
}

SceneTree::~SceneTree() {
	if (SceneNode *child = left())
		child->TreeToList(list, 0);
	if (SceneNode *child = right())
		child->TreeToList(list, 0);
	next = 0;
}

bool SceneTree::Move(param(cuboid) box, SceneNode *n) {
	n->box = box;
	if (n->InTree()) {
		n->RemoveFromTree();
		n->parent = this;
		list.push_back(n);
		return true;
	} else if (!list.empty()) {
		list.push_back(n->unlink());
	}
	return false;
}

SceneNode *SceneTree::Insert(param(cuboid) box, void *data) {
	SceneNode *n = SceneNode::Make(box, data);
	n->parent = this;
	list.push_back(n);
	return n;
}

void SceneTree::CreateTree() {
	if (SceneNode *child = left())
		child->TreeToList(list, this);
	if (SceneNode *child = right())
		child->TreeToList(list, this);

	clear(box);

	if (size_t n = list.size()) {
		if (n == 1) {
			SceneNode	*n = list.pop_front();
			n->prev		= 0;
			SetChild(0, n);
			box	= n->box;
		} else {
			SceneNode	**temp_array = alloc_auto(SceneNode*, n), **p = temp_array;
			for (auto &i : list)
				*p++ = &i;
			Partition(*this, temp_array, p);
		}
	}
}

void SceneTree::CollectAll(const SceneNode *node, items &list) {
	SceneNode	*stack[64], **sp = stack;
	for (;;) {
		if (node->IsLeaf()) {
			list.push_back(node->data);
			if (sp == stack)
				break;
			node	= *--sp;
		} else {
			*sp++	= node->right();
			node	= node->left();
		}
	}
}

void SceneTree::CollectAllBigEnough(param(float4x4) frustum, const SceneNode *node, items &list, float size) {
	SceneNode	*stack[64], **sp = stack;
	for (;;) {
		float4x4	mat = frustum * node->box.matrix();
		if (any(abs(mat.x.xyz) + abs(mat.y.xyz) + abs(mat.z.xyz) > abs(mat.w.w) * size)) {
			if (node->IsLeaf()) {
				list.push_back(node->data);
			} else {
				*sp++	= node->right();
				node	= node->left();
				continue;
			}
		}

		if (sp == stack)
			break;
		node	= *--sp;
	}
}

void SceneTree::CollectFrustum(param(float4x4) frustum, items &vis_list, items &comp_vis_list, int flags) {
	vis_list.clear();
	comp_vis_list.clear();

	pair<const SceneNode*,int> stack[64], *sp = stack;

	if (const SceneNode *RESTRICT node = GetRoot()) for (;;) {
		if (node->IsLeaf()) {
			if (is_visible(node->box, frustum))
				vis_list.push_back(node->data);
		} else {
			flags	= visibility_flags(node->box, frustum, flags);
			if (flags == 0) {
				CollectAllBigEnough(frustum, node, comp_vis_list, 0.02f);
			} else if (flags >= 0) {
				sp->a	= node->right();
				sp->b	= flags;
				sp++;
				node	= node->left();
				continue;
			}
		}
		if (sp == stack)
			break;
		--sp;
		node	= sp->a;
		flags	= sp->b;
	}
}

void SceneTree::CollectTreeTreeIntersect(intersections &intersect_list, const SceneNode * RESTRICT node0, const SceneNode * RESTRICT node1) {
	if (!node0 || !node1)
		return;

	pair<const SceneNode*, const SceneNode*>	stack[64], *sp = stack;
	for (;;) {
		if (overlap(node0->box, node1->box)) {
			if (node0->IsLeaf())	{
				if (node1->IsLeaf()) {
					intersect_list.push_back(make_pair(node0->data, node1->data));
				} else {
					sp->a = node0;
					sp->b = node1->right();
					sp++;
					node1 = node1->left();
					continue;
				}
			} else if (node1->IsLeaf()) {
				sp->a = node0->right();
				sp->b = node1;
				sp++;
				node0 = node0->left();
				continue;
			} else {
				sp->a = node0->right();
				sp->b = node1->right();
				sp++;
				if (node0 != node1) {
					sp->a = node0->right();
					sp->b = node1->left();
					sp++;
				}
				sp->a = node0->left();
				sp->b = node1->right();
				sp++;
				node0 = node0->left();
				node1 = node1->left();
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

void SceneTree::CollectTreeIntersect_Statics(const SceneTree &other, intersections &intersect_list) const {
	CollectTreeTreeIntersect(intersect_list, GetRoot(), other.GetRoot());
}

void SceneTree::CollectTreeIntersect_Dynamics(SceneTree &other, intersections &intersect_list) {
	const SceneNode *root0 = GetRoot();
	SceneNode *root1		= list.ToTree();
	if (&other == this) {
		CollectTreeTreeIntersect(intersect_list, root0, root1);
		CollectTreeTreeIntersect(intersect_list, root1, root1);
		if (root1)
			root1->TreeToList(list, 0);
	} else {
		const SceneNode *root2 = other.GetRoot();
		SceneNode *root3		= other.list.ToTree();
		CollectTreeTreeIntersect(intersect_list, root1, root2);
		CollectTreeTreeIntersect(intersect_list, root0, root3);
		CollectTreeTreeIntersect(intersect_list, root1, root3);
		if (root1)
			root1->TreeToList(list, 0);
		if (root3)
			root3->TreeToList(other.list, 0);
	}
}


}//namespace iso
