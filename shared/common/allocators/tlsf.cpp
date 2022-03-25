#include "tlsf.h"
#include "base/bits.h"
#include "base/list.h"
#include <stddef.h>

namespace tlsf {

//#define CHECK_HEAP	ISO_ASSERT(check_heap(tlsf) == 0)
#define CHECK_HEAP

//template<typename B, typename A, A B::*M> inline B *enclosing_cast(A *a) {
//	return (B*)(intptr_t(a) - intptr_t(&(((B*)0)->*M)));
//}

//-----------------------------------------------------------------------------
//	TLSF HEAP
//-----------------------------------------------------------------------------
// Block header structure.
// - The prev_phys_block field is only valid if the previous block is free.
// - The prev_phys_block field is actually stored at the end of the previous block.
// - The next_free / prev_free fields are only valid if the block is free.

struct block_header {
	enum {
		free_bit		= 1 << 0,	//whether block is free
		prev_free_bit	= 1 << 1,	//whether previous block is free
		overhead		= sizeof(size_t),
		offset			= sizeof(block_header*) + overhead,	// User data starts directly after the size field in a used block.
	};

	block_header*	prev_phys_block;	// Points to the previous physical block
	size_t			block_size;			// The size of this block, excluding the block header
	block_header*	next_free;			// next free block
	block_header*	prev_free;			// previous free block

	// Return location of next block after block of given size
	static block_header* offset_to_block(const void* p, size_t size)	{ return (block_header*)(uintptr_t(p) + size); }
	static block_header* from_ptr(const void* p)						{ return (block_header*)(uintptr_t(p) - offset); }

	void*	to_ptr()		const	{ return (char*)this + offset; }
	size_t	size()			const	{ return block_size & ~(free_bit | prev_free_bit); }
	bool	is_last()		const	{ return size() == 0; }
	bool	is_free()		const	{ return !!(block_size & free_bit);	}
	bool	is_prev_free()	const	{ return !!(block_size & prev_free_bit); }

	void	set_size(size_t size)	{ block_size = size | (block_size & (free_bit | prev_free_bit)); }
	void	set_free()				{ block_size |= free_bit; }
	void	set_used()				{ block_size &= ~free_bit; }
	void	set_prev_free()			{ block_size |= prev_free_bit; }
	void	set_prev_used()			{ block_size &= ~prev_free_bit; }
	void	set_both_free()			{ block_size |= free_bit | prev_free_bit; }

	void	set_size(size_t size, bool free, bool prev_free)	{ block_size = size | (free ? free_bit : 0) | (prev_free ? prev_free_bit : 0); }

	// next/prev
	block_header* prev()	const	{ return prev_phys_block; }
	block_header* next()	const	{ ISO_ASSERT(!is_last()); return offset_to_block(to_ptr(), size() - overhead); }

	block_header* link_next(block_header* n) {
		n->prev_phys_block = this;
		return n;
	}
	block_header* link_next() {
		return link_next(next());
	}
	void mark_as_free() {
		link_next()->set_prev_free();
		set_free();
	}
	void mark_as_used() {
		next()->set_prev_used();
		set_used();
	}

	// split/join
	bool can_split(size_t split) const {
		return size() >= sizeof(block_header) + split;
	}
	block_header* split(size_t split) {
		block_header*	remaining	= offset_to_block(to_ptr(), split - overhead);
		const size_t	remain_size	= size() - (split + overhead);

		ISO_ASSERT2(remaining->to_ptr() == align(remaining->to_ptr(), sizeof(size_t)), "remaining block not aligned properly");
		ISO_ASSERT(size() == remain_size + split + overhead);

		remaining->set_size(remain_size);
 		set_size(split);
		return remaining;
	}
	void absorb_next(block_header *n) {
		ISO_ASSERT(n == next());
		block_size += n->size() + overhead;
		//link_next();
	}
};

struct heap : TLSF<block_header, 31, 5> {
	enum {
		BLOCK_MIN	= sizeof(block_header) - sizeof(block_header*),
	};

	static size_t	adjust_request_size(size_t size, uint32 align = ALIGN_SIZE) {
		return size && size < MAX_BLOCK_SIZE ? max(iso::align(size, align), (size_t)BLOCK_MIN) : 0;
	}
	void			remove(block_header* block) {
		remove_free_block(block, block->size());
	}
	void			insert(block_header* block) {
		ISO_ASSERT2(block->to_ptr() == align(block->to_ptr(), ALIGN_SIZE), "block not aligned properly");
		insert_free_block(block, block->size());
	}
	void			merge_next(block_header* block) {
		block_header* next = block->next();
		if (next->is_free()) {
			ISO_ASSERT2(!block->is_last(), "previous block can't be last!");
			remove(next);
			block->absorb_next(next);
			block->link_next();
		}
	}

	// Trim any trailing block space off the end of a block, return to heap.
	void			trim_free(block_header* block, size_t size) {
		ISO_ASSERT2(block->is_free(), "block must be free");
		if (block->can_split(size)) {
			block_header* remaining_block = block->split(size);
			remaining_block->mark_as_free();
			block->link_next(remaining_block);
			remaining_block->set_prev_free();
			insert(remaining_block);
		}
	}
	// Trim any trailing block space off the end of a used block, return to heap.
	void			trim_used(block_header* block, size_t size) {
		ISO_ASSERT2(!block->is_free(), "block must be used");
		if (block->can_split(size)) {
			// If the next block is free, we must coalesce.
			block_header* remaining_block = block->split(size);
			remaining_block->mark_as_free();
			remaining_block->set_prev_used();
			merge_next(remaining_block);
			insert(remaining_block);
		} else {
			block->next()->set_prev_used();
		}
	}
	// Trim any leading block space off the end of a block, return to heap.
	block_header*	trim_free_leading(block_header* block, size_t size) {
		ISO_ASSERT2(block->is_free(), "block must be free");
		block_header* remaining_block = block;
		if (block->can_split(size - block_header::overhead)) {
			// We want the 2nd block.
			remaining_block = block->split(size - block_header::overhead);
			remaining_block->set_both_free();
			block->link_next(remaining_block);
			insert(block);
		}
		return remaining_block;
	}

	block_header*	alloc(size_t size);
	block_header*	alloc(size_t size, uint32 align);
	block_header*	realloc(block_header* block, size_t size);
	void			free(block_header* p);

	void			walk_heap(walker *w, void* user);
};

block_header* heap::alloc(size_t size) {
	const size_t adjust = adjust_request_size(size);
	block_header* block = adjust ? allocate_block(adjust) : 0;
	if (block) {
		ISO_ASSERT(block->size() >= adjust);
		trim_free(block, adjust);
		block->mark_as_used();
	}
	return block;
}

block_header* heap::alloc(size_t size, uint32 align) {
//	check_heap(this);

	if (align <= ALIGN_SIZE)
		return alloc(size);

	// We must allocate an additional minimum block size bytes so that if our free block will leave an alignment gap which is smaller, we can
	// trim a leading free block and release it back to the heap. We must do this because the previous physical block is in use, therefore
	// the prev_phys_block field is not valid, and we can't simply adjust the size of that block.
	if (const size_t adjust = adjust_request_size(size)) {
		const size_t gap_minimum	= sizeof(block_header);
		const size_t aligned_size	= adjust_request_size(adjust + align + gap_minimum, align);

		if (block_header* block = aligned_size ? allocate_block(aligned_size) : 0) {
			ISO_ASSERT(block->size() >= aligned_size && block->is_free());
			void*	p		= block->to_ptr();
	#if 1
			void*	aligned	= (void*)align_down((ptrdiff_t)p + block->size() - adjust, align);
			ptrdiff_t	gap	= (ptrdiff_t)aligned - (ptrdiff_t)p;
	#else
			void* aligned	= align(p, align);
			ptrdiff_t gap	= (ptrdiff_t)aligned - (ptrdiff_t)p;

			// If gap size is too small, offset to next aligned boundary.
			if (gap && gap < gap_minimum) {
				const size_t offset		= max(gap_minimum - gap, align);
				void* next_aligned		= (void*)(ptrdiff_t(aligned) + offset);

				aligned = align(next_aligned, align);
				gap		= (ptrdiff_t)aligned - (ptrdiff_t)p;
			}
	#endif

			if (gap) {
				ISO_ASSERT2(gap >= gap_minimum, "gap size too small");
				block = trim_free_leading(block, gap);
			}
			trim_free(block, adjust);
			block->mark_as_used();
			return block;
		}
	}

	return 0;
}

block_header* heap::realloc(block_header* block, size_t size) {
//	check_heap(this);

	const size_t cursize	= block->size();
	const size_t adjust		= adjust_request_size(size);

	if (size <= cursize) {
		trim_used(block, adjust);
		return block;
	}

	const void		*p		= block->to_ptr();
	block_header	*next	= block->next();

	// Try combining with next
	if (next->is_free()) {
		remove(next);
		block->absorb_next(next);
		block->link_next();

		if (adjust <= block->size()) {
			trim_used(block, adjust);
			return block;
		}
	}

	// Try combining with prev
	if (block->is_prev_free()) {
		block_header* prev	= block->prev();
		remove(prev);
		prev->absorb_next(block);	//don't link_next - could corrupt end of block
		prev->mark_as_used();
		block = prev;

		if (adjust <= block->size()) {
			memcpy(block->to_ptr(), p, cursize);
			trim_used(block, adjust);
			return block;
		}
	}

	// Allocate new block (but old block only needs inserting now)
	block_header *newblock = alloc(size);
	if (newblock) {
		memcpy(newblock->to_ptr(), p, cursize);
		block->mark_as_free();
		insert(block);
	}
	return newblock;
}

void heap::free(block_header* block) {
//	check_heap(this);

	block->mark_as_free();

	// merge prev
	if (block->is_prev_free()) {
		block_header* prev = block->prev();
		remove(prev);
		prev->absorb_next(block);
		prev->link_next();
		block = prev;
	}

	merge_next(block);
	insert(block);
}

void heap::walk_heap(walker *w, void* user) {
	for (block_header* block = block_header::offset_to_block(this, sizeof(heap) - block_header::overhead); block && !block->is_last(); block = block->next())
		w(block->to_ptr(), block->size(), !block->is_free(), user);
}

//-----------------------------------------------------------------------------
//	interface stubs
//-----------------------------------------------------------------------------

void add_region(heap *tlsf, void* mem, size_t size) {
	char		*end	= (char*)mem + size - block_header::offset;
	block_header* block	= block_header::offset_to_block(align(mem, heap::ALIGN_SIZE), -block_header::overhead);

	bool	prev_free = false;
	while (end - (char*)block > heap::MAX_BLOCK_SIZE) {
		block->set_size(heap::MAX_BLOCK_SIZE - block_header::overhead, true, prev_free);
		prev_free = true;
		tlsf->insert(block);
		block = block->link_next();
	}

	size	= align_down(end - (char*)block - block_header::overhead, tlsf->ALIGN_SIZE);
	block->set_size(size, true, prev_free);
	tlsf->insert(block);

	// Split the block to create a zero-size heap sentinel block.
	block = block->link_next();
	block->set_size(0, false, true);
}

heap *create(void* mem, size_t size) {
	const size_t overhead	= sizeof(heap) + block_header::overhead * 2;

	if (size - overhead < heap::BLOCK_MIN || size - overhead > heap::MAX_BLOCK_SIZE)
		return 0;

	// Construct a valid heap object.
	heap	*p = new(mem) heap;
	add_region(p, p + 1, size - sizeof(heap));
	return p;
}

void destroy(heap *tlsf) {
}

void* alloc(heap *tlsf, size_t size) {
	block_header *block = tlsf->alloc(size);
	if (block) {
		CHECK_HEAP;

		void	*p = block->to_ptr();
		memset(p, 0xaa, size);
		return p;
	}
	return 0;
//	return block ? block->to_ptr() : 0;
}

void* alloc(heap *tlsf, size_t size, size_t align) {
	if (size == 0)
		return 0;
	block_header *block = tlsf->alloc(size, (uint32)align);
	return block ? block->to_ptr() : 0;
}

void free(heap *tlsf, void* p) {
	CHECK_HEAP;
	if (p)
		tlsf->free(block_header::from_ptr(p));
	CHECK_HEAP;
}

void* realloc(heap *tlsf, void* p, size_t size) {
	block_header*	m = 0;

	if (p && size == 0)
		tlsf->free(block_header::from_ptr(p));
	else if (!p)
		m = tlsf->alloc(size);
	else
		m = tlsf->realloc(block_header::from_ptr(p), size);

	CHECK_HEAP;

	return m ? m->to_ptr() : 0;
}

size_t block_size(void* p) {
	return p ? block_header::from_ptr(p)->size() : 0;
}

// Debugging utilities.

void walk_heap(heap *tlsf, walker *w, void* user) {
	return tlsf->walk_heap(w, user);
}

struct integrity_t {
	bool	prev_free;
	int		errors;
};

#define tlsf_insist(x, m) { ISO_ASSERT2(x, m); if (!(x)) { errors--; } }

static void integrity_walker(void* p, size_t size, bool used, void* user) {
	block_header*	block	= block_header::from_ptr(p);
	integrity_t*	integ	= (integrity_t*)user;
	int&			errors	= integ->errors;

	tlsf_insist(integ->prev_free == block->is_prev_free(), "prev status incorrect");
	tlsf_insist(size == block->size(), "block size incorrect");

	integ->prev_free	= block->is_free();
}

int check_heap(heap *tlsf) {
	// Check that the blocks are physically correct.
	integrity_t integ = { false, 0 };
	tlsf->walk_heap(integrity_walker, &integ);
	int			errors = integ.errors;

	// Check that the free lists and bitmaps are accurate.
	for (int i = 0; i < heap::L1_INDEX_COUNT; ++i) {
		for (int j = 0; j < heap::L2_INDEX_COUNT; ++j) {
			const int l1_map			= tlsf->bitmap1 & (1 << i);
			const int l2_list			= tlsf->bitmap2[i];
			const int l2_map			= l2_list & (1 << j);
			const block_header* block	= tlsf->blocks[heap::mapping(i, j)];

			// Check that first- and second-level lists agree.
			if (!l1_map)
				tlsf_insist(!l2_map, "second-level map must be null");

			if (!l2_map) {
				tlsf_insist(block == 0, "block list must be null");
				continue;
			}

			// Check that there is at least one free block.
			tlsf_insist(l2_list, "no free blocks in second-level map");
			tlsf_insist(block != 0, "block should not be null");

			while (block) {
				tlsf_insist(block->is_free(), "block should be free");
				tlsf_insist(!block->is_prev_free(), "blocks should have been coalesced");
				tlsf_insist(!block->next()->is_free(), "blocks should have been coalesced");
				tlsf_insist(block->next()->is_prev_free(), "block should be free");
				tlsf_insist(block->size() >= heap::BLOCK_MIN, "block below minimum size");

				heap::mapping m(block->size());
				tlsf_insist(m.l1 == i && m.l2 == j, "block size indexed in wrong list");
				block = block->next_free;
			}
		}
	}

	return errors;
}

#undef tlsf_insist
} //namespace tlsf

#if 0
//-----------------------------------------------------------------------------
//	test
//-----------------------------------------------------------------------------

#include "utilities.h"

inline void *operator new(size_t size, tlsf_pool heap) {
	return tlsf_alloc(heap, size);
}
inline void *operator new[](size_t size, tlsf_pool heap) {
	return tlsf_alloc(heap, size);
}

int test_tlsf() {

	void		*mem	= malloc(0x100000);
	tlsf_pool	heap	= tlsf_create(mem, 0x100000);

	void		*array[1000];
	int			order[1000];
//	for (;;) {
		for (int i = 0; i < 1000; i++) {
			order[i] = i;
			array[i] = tlsf::alloc(heap, (uint32)pow(1024.f, (float)iso::random));
		}
		for (int i = 0; i < 1000; i++)
			swap(order[i], order[iso::random.to(1000)]);

		for (int i = 0; i < 1000; i++) {
			tlsf_free(heap, array[order[i]]);
		}
//	}

	void		*test	= tlsf_memalign(heap, 16, 123);
//void* tlsf_memalign(tlsf_pool heap, size_t align, size_t bytes);
//void* tlsf_realloc(tlsf_pool heap, void* p, size_t size);
	tlsf_free(heap, test);

	tlsf_destroy(heap);
	free(mem);

	return 0;
}

static int a = test_tlsf();
#endif
