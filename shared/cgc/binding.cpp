#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "binding.h"
#include "memory.h"
#include "errors.h"

namespace cgc {

Binding::Binding(int _gname, int _lname, BindingKinds _kind) {
	memset(this, 0, sizeof(*this));
	gname	= _gname;
	lname	= _lname;
	kind	= _kind;
}

Binding *DupBinding(MemoryPool *pool, Binding *fBind) {
	return new(pool) Binding(*fBind);
}

Binding *NewConstDefaultBinding(MemoryPool *pool, int gname, int sname, int count, int rname, int regno, float *fval) {
	Binding *bind				= new(pool) Binding(gname, sname, BK_CONSTANT);
	bind->size					= count;
	bind->rname					= rname;
	bind->regno					= regno;
	bind->constdef.val			= (float*)malloc(count * sizeof(float));
	for (int i = 0; i < count; i++)
		bind->constdef.val[i] = fval[i];
	return bind;
}

BindingTree *NewRegArrayBindingTree(MemoryPool *pool, SourceLoc &loc, int pname, int aname, int rname, int regno, int count) {
	BindingTree	*tree			= new(pool) BindingTree(loc, pname, aname, BK_REGARRAY);
	tree->binding.rname			= rname;
	tree->binding.regno			= regno;
	tree->binding.reg.count		= count;
	tree->binding.reg.parent	= 0;
	return tree;
}

BindingTree *NewTexunitBindingTree(MemoryPool *pool, SourceLoc &loc, int pname, int aname, int unitno) {
	BindingTree	*tree			= new(pool) BindingTree(loc, pname, aname, BK_TEXUNIT);
	tree->binding.regno			= unitno;
	return tree;
}

BindingTree *NewConstDefaultBindingTree(MemoryPool *pool, SourceLoc &loc, BindingKinds kind, int pname, int aname, int count, float *fval) {
	if (count > 4)
		count = 4;

	BindingTree	*tree			= new(pool) BindingTree(loc, pname, aname, kind);
	tree->binding.size			= count;
	tree->binding.rname			= 0;
	tree->binding.regno			= 0;
	tree->binding.constdef.val	= new(pool) float[count];
	for (int i = 0; i < count; i++)
		tree->binding.constdef.val[i] = fval[i];
	return tree;
}

BindingTree *LookupBinding(BindingTree *bindings, int gname, int lname) {
	for (BindingTree *tree = bindings; tree; tree = tree->nextc) {
		if (tree->binding.gname == gname) {
			do {
				if (tree->binding.lname == lname)
					return tree;
				tree = tree->nextm;
			} while (tree);
			return NULL;
		}
	}
	return NULL;
}

void AddBinding(BindingList **bindings, BindingList *b) {
	if (*bindings) {
		while ((*bindings)->next)
			bindings = &(*bindings)->next;
		(*bindings)->next = b;
	} else {
		*bindings = b;
	}
}

void AddBinding(BindingTree	**bindings, BindingTree *b) {
	for (BindingTree *tree = *bindings; tree; tree = tree->nextc) {
		if (tree->binding.gname == b->binding.gname) {
			b->nextm	= tree->nextm;
			tree->nextm	= b;
			return;
		}
	}
	b->nextc	= *bindings;
	*bindings	= b;
}

} //namespace cgc
