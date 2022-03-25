#ifndef TLSF_H
#define TLSF_H

#include "base/defs.h"
#include "base/bits.h"

namespace tlsf {
using namespace iso;

	// Generic TLSF structure
	template<typename B, int L1, int L2_LOG2, int MIN_LOG2 = 3> struct TLSF {
		enum {
			L2_INDEX_COUNT		= 1 << L2_LOG2,
			L1_INDEX_SHIFT		= L2_LOG2 + MIN_LOG2,
			L1_INDEX_COUNT		= L1 - L1_INDEX_SHIFT + 1,

			ALIGN_SIZE			= 1 << MIN_LOG2,
			SMALL_BLOCK_SIZE	= 1 << L1_INDEX_SHIFT,
		};
		static const size_t	MAX_BLOCK_SIZE		= size_t(1) << L1;

		uint32	bitmap1, bitmap2[L1_INDEX_COUNT];
		B		*blocks[L1_INDEX_COUNT * L2_INDEX_COUNT];

		struct mapping {
			union {
				uint16	u;
				struct {
					uint16	l2:L2_LOG2, l1:(16 - L2_LOG2);
				};
			};
			mapping(size_t size) {
				if (size < SMALL_BLOCK_SIZE) {
					// Store small blocks in first list.
					l1 = 0;
					l2 = int(size) >> MIN_LOG2;
				} else {
					int	t = highest_set_index(size);
					l2 = int(size >> (t - L2_LOG2)) ^ (1 << L2_LOG2);
					l1 = t - L1_INDEX_SHIFT + 1;
					ISO_ASSERT(l1 < L1_INDEX_COUNT);
				}
			}
			mapping(int l1, int l2) : l2(l2), l1(l1)  {}
			mapping&	operator++()	{ ++u; return *this; }
			operator int() const		{ return u; }
		};

		TLSF()	{
			clear(*this);
		}

		void remove_free_block(B* block, const mapping &m) {
			B	*prev = block->prev_free;
			B	*next = block->next_free;
			if (next)
				next->prev_free = prev;
			if (prev) {
				prev->next_free = next;
			} else {
				// this block was the head of the free list, set new head.
				ISO_ASSERT(blocks[m.u] == block);
				blocks[m.u] = next;

				// If the new head is null, clear the bitmap.
				if (!next) {
					bitmap2[m.l1] &= ~(1 << m.l2);

					// If the second bitmap is now empty, clear the l1 bitmap.
					if (!bitmap2[m.l1])
						bitmap1 &= ~(1 << m.l1);
				}
			}
		}

		void insert_free_block(B* block, const mapping &m) {
			//ISO_ASSERT(block->is_free());
			B	*next			= blocks[m.u];
			block->next_free	= next;
			block->prev_free	= 0;
			if (next)
				next->prev_free = block;

			// Insert the new block at the head of the list, and mark the first- and second-level bitmaps appropriately.
			blocks[m.u]			= block;
			bitmap1			|= 1 << m.l1;
			bitmap2[m.l1]		|= 1 << m.l2;
		}

		B* allocate_block(size_t size) {
			mapping	m(size);
			++m;

			uint32	l2_map	= bitmap2[m.l1] & (~0 << m.l2);
			if (!l2_map) {
				// no block exists - search in the next largest first-level list
				uint32 l1_map = bitmap1 & (~0 << (m.l1 + 1));
				if (!l1_map) // no free blocks available; memory has been exhausted
					return 0;

				l2_map	= bitmap2[m.l1 = lowest_set_index(l1_map)];
			}

			ISO_ASSERT2(l2_map, "internal error - second level bitmap is null");
			m.l2	= lowest_set_index(l2_map);

			// return the first block in the free list
			B* block = blocks[m.u];
			remove_free_block(block, m);
			return block;
		}
	};

	struct	heap;
	typedef void walker(void *p, size_t size, bool used, void* user);

	heap	*create(void* mem, size_t size);
	void	destroy(heap *tlsf);
	void	add_region(heap *tlsf, void* mem, size_t size);

	void*	alloc(heap *tlsf, size_t size);
	void*	alloc(heap *tlsf, size_t size, size_t align);
	void*	realloc(heap *tlsf, void *p, size_t size);
	void	free(heap *tlsf, void *p);

	void	walk_heap(heap *tlsf, walker *w, void* user);
	int		check_heap(heap *tlsf);
	size_t	block_size(void *p);

} //namespace tlsf

#endif // TLSF_H
