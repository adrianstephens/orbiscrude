#include "debug.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	CallStacks
//-----------------------------------------------------------------------------

void **CallStacks::FindEnd(void **addr, uint32 maxdepth) {
	void	**end = addr + maxdepth;
	while (addr < end && *addr)
		++addr;
	return addr;
}

int	CallStacks::FindDivergence(void **a, void **aend, void **b, void **bend) {
	while (--aend > a && --bend > b && *aend == *bend)
		;
	return int(aend - a);
}

//-----------------------------------------------------------------------------
//	CompressedCallStacks
//-----------------------------------------------------------------------------

CompressedCallStacks::stack_node* CompressedCallStacks::Compress(void **addr, int len) {
	stack_node			*node	= 0;
	stack_node			*prev	= 0;
	e_rbtree<stack_node>	*s	= &stacks;
	for (int i = len; i--;) {
		void	*a	= addr[i];
		auto	j	= lower_boundc(*s, a);
		if (!j || a < *j)
			s->insert(j, node = new stack_node(a, prev));
		else
			node	= j;
		prev	= node;
		s		= &node->next;
	}
	return node;
}

int CompressedCallStacks::Decompress(stack_node *node, void **addr, int maxlen) const {
	void	**p = addr, **e = p + maxlen;
	while (node && p < e) {
		*p++ = node->addr;
		node = node->prev;
	}
	return int(p - addr);
};


//-----------------------------------------------------------------------------
//	Symbolicator
//-----------------------------------------------------------------------------

Symbolicator::Symbolicator(Module process, uint32 options, const char *search_path) : options(options), search_path(search_path) {
}

Symbolicator::Symbolicator(uint32 options) {
}

void Symbolicator::SetOptions(uint32 _options, const char *_search_path)  {
	options		= _options;
	search_path	= _search_path;
}

void Symbolicator::AddModule(const char *fn, uint64 a, uint64 b) {
}

void Symbolicator::AddModule(Module file, uint64 base) {
}

const char *Symbolicator::GetModule(uint64 addr) {
	return 0;
}


