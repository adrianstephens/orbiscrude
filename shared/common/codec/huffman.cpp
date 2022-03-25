#include "huffman.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Dynamic Huffman
//-----------------------------------------------------------------------------

DYNHUFF_base::DYNHUFF_base(BLOCK *pool, int n) {
	next_avail		= pool;

	while (--n) {
		pool->free = pool + 1;
		++pool;
	}
	pool->free = 0;
}

void DYNHUFF_base::start(ENTRY *entry, ENTRY **node, uint32 nc) {
	//leafs
	BLOCK	*block	= alloc_block();
	auto	p		= entry + nc * 2 - 2;
	for (int i = 0; i < nc; i++, p--) {
		node[i]		= p;
		p->freq		= 1;
		p->child	= ~i;
		p->block	= block;
	}
	block->first	= p + 1;

	//combine pairs
	for (auto i = entry + nc * 2 - 2; p >= entry; i -= 2, --p) {
		int		f	= i[0].freq + i[-1].freq;
		p->freq		= f;
		p->child	= i - entry;
		i[0].parent = i[-1].parent = p;
		p->start_block(f == p[1].freq ? p[1].block : alloc_block());
	}
}

void DYNHUFF_base::remake(ENTRY *entry, ENTRY **node, uint32 nc) {
	auto	end = entry + nc * 2 - 2;
	ENTRY	*p	= entry;

	// gather leaf nodes at start, halve frequencies and free blocks
	for (auto i = entry; i <= end; i++) {
		int	k = i->child;
		if (k < 0) {
			p->freq		= (i->freq + 1) / 2;
			p->child	= k;
			++p;
		}
		if (i->block->first == i)
			free_block(i->block);
	}

	// make parents and sort leaves in with them
	--p;
	for (auto i = end, j = end; i >= entry; --i, j -= 2) {
		//put leaves
		while (i > j - 2) {
			i->freq		= p->freq;
			i->child	= p->child;
			--i;
			--p;
		}
		uint32	f	= j[0].freq + j[-1].freq;
		ENTRY	*k	= entry;
		while (f < k->freq)
			++k;

		//put leaves
		while (p >= k) {
			i->freq		= p->freq;
			i->child	= p->child;
			--i;
			--p;
		}
		//put parent
		i->freq		= f;
		i->child	= j - entry;
	}

	// set parents and assign blocks
	uint32	lastf	= 0;
	BLOCK	*lastb	= 0;
	for (auto i = entry; i <= end; i++) {
		set_parent(entry, node, i->child, i);

		if (i->freq == lastf) {
			i->block = lastb;
		} else {
			lastb	= i->start_block(alloc_block());
			lastf	= i->freq;
		}
	}
}

// increment freq and adjust blocks
DYNHUFF_base::ENTRY *DYNHUFF_base::increment(ENTRY *entry, ENTRY **node, ENTRY *p) {
	BLOCK	*block	= p->block;
	auto	first	= block->first;

	if (first != p) {
		// swap for leader
		int	r			= p->child;
		int	s			= first->child;
		p->child		= s;
		first->child	= r;

		set_parent(entry, node, r, first);
		set_parent(entry, node, s, p);

		p = first;

		block->first = first + 1;
		// merge with previous?
		if (++p->freq == p[-1].freq)
			p->block = p[-1].block;
		else
			p->start_block(alloc_block());

	} else if (block == p[1].block) {
		// not just me in block
		block->first = first + 1;
		// merge with previous?
		if (++p->freq == p[-1].freq)
			p->block = p[-1].block;
		else
			p->start_block(alloc_block());

	} else if (++p->freq == p[-1].freq) {
		// merge with previous
		free_block(block);
		p->block = p[-1].block;
	}

	return p->parent;
}

void DYNHUFF_base::add_leaf(ENTRY *entry, ENTRY **node, uint32 c, uint32 nc) {
	auto	*parent = entry + nc * 2 - 2;
	auto	*child	= parent + 1;

	int		leaf	= parent->child;
	node[~leaf]		= child + 0;
	node[c]			= child + 1;
	parent->child	= nc * 2;	// child1

	child[0].child	= leaf;
	child[0].freq	= parent->freq;
	child[0].block	= parent->block;

	child[1].child	= ~c;
	child[1].freq	= 0;
	child[1].start_block(alloc_block());

	child[0].parent	= child[1].parent = parent;
}


} // namespace iso