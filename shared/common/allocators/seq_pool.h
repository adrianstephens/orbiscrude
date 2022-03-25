#include "base/atomic.h"
#include "base/list.h"

using namespace iso;

#define PAGE_SIZE			(4096)
#define DEFAULT_BLOCK_CLASS	(100)
#define MAX_CORE_ID			(8)

/* Configurations */
#define	CHUNK_DATA_SIZE		(16*PAGE_SIZE)
#define	ALLOC_UNIT			(4*1024*1024)
#define	MAX_FREE_SIZE		(4*1024*1024)
#define	RAW_POOL_START		((char*)((0x600000000000/CHUNK_SIZE+1)*CHUNK_SIZE))
#define	BLOCK_BUF_CNT		(16)

// #define RETURN_MEMORY
// #define DEBUG

/* Other */
#define	DCHUNK_SIZE			(256)
#define	CHUNK_SIZE			(CHUNK_DATA_SIZE+DCHUNK_SIZE)
#define	CHUNK_MASK			(~(CHUNK_SIZE-1))
#define	LARGE_CLASS			(100)
#define	DUMMY_CLASS			(101)
#define	MAX_FREE_CHUNK		(MAX_FREE_SIZE/CHUNK_SIZE)
#define	LARGE_OWNER			((lheap*)0xDEAD)

struct lifo_node {
	lifo_node	*next;
};

// Sequential LIFO Queue
struct seq_lifo {
	lifo_node	*head;

	void	init() {
		head = 0;
	}
	void	push(lifo_node *element) {
		element->next	= head;
		head			= element;
	}
	lifo_node*	pop() {
		lifo_node *old_addr = head;
		if (old_addr)
			head = old_addr->next;
		return old_addr;
	}
};

// Data	chunk header
template<int N> struct pool_chunk : e_link<pool_chunk> {
	// Read	Area
	pool			*owner;

	// Local Write Area
	uint16			free_count;
	uint16			count;
	uint16			free_mem;
	uint16			free_link;

	pool_chunk(pool *_owner) : owner(_owner) {
		uint16			free_count;
		uint16			count;
		uint16			free_mem;
		uint16			free;
	}

	void change_cls(int cls) {
		size_cls		= cls;
		block_size		= mappings.cls2size[cls];
		count			= free_count = CHUNK_DATA_SIZE / block_size;
		free_mem		= DCHUNK_SIZE;
		free_lifo.init();
	}
	void collect_garbage() {
		uint32	n;
		free_lifo.head	= remote_free_head.pop_all(&n);
		free_count		= n;
	}
	void *alloc_obj() {
		lifo_node *node = free_lifo.head;
		if (unlikely(!node)) {
			void	*p	= (char*)this + free_mem;
			free_mem	+= block_size;
			return p;
		} else {
			free_lifo.head	= node->next;
			return node;
		}
	}
	static dchunk *get(void *ptr) {
		return (dchunk*)((uintptr_t)ptr - ((uintptr_t)ptr % CHUNK_SIZE));
	}
};

// Per-thread data chunk pool
struct pool {
	seq_lifo	free_lifo;
	uint32		free_dcs;

	list		chunks;

	pool() {
	}

	void replace_foreground() {
		dchunk *dc;
		if (dc = (dchunk*)free_lifo.pop()) {
			free_dcs--;
		} else {
			dc	= new(global_pool.acquire_chunk()) dchunk(this);
		}
		//Set the foreground chunk
		chunks.set_single(dc);
	}

	void *alloc() {
		for (;;) {
			dchunk	*dc		= (dchunk*)chunks.head;
			void	*ret	= dc->alloc_obj();

			// Check if	the	datachunk is full
			if (unlikely(--dc->free_count == 0)) {
				chunks.remove(dc);
				if (!chunks.head)
					replace_foreground();
			}
			return ret;
		}
	}

	void local_free(dchunk *dc, void *ptr) {
		uint32 free_count = ++dc->free_count;
		dc->free_lifo.push((lifo_node*)ptr);
		
		if (dc != chunks.head) {
			if (unlikely(free_count == 1)) {	// was full?
				dclist.head->insert_after(dc);
			} else if (unlikely(free_count == dc->count)) {//now empty?
				dclist.remove(dc);
				if (free_dcs >=	MAX_FREE_CHUNK)	{
					global_pool.release_chunk(dc);
				} else {
					free_lifo.push((lifo_node*)dc);
					++free_dcs;
				}
			}
		}
	}

};

void *malloc(size_t	size) {
	lheap	*lh	= lheap::get();

	// Deal	with zero-size allocation
	size +=	(size == 0);

	int	cls = mappings.size2cls(size);
	return likely(cls < DEFAULT_BLOCK_CLASS)
		? lh->alloc(cls)
		: large_malloc(size);
}

void free(void *ptr) {
	if (ptr	== NULL)
		return;

	dchunk *dc			= dchunk::get(ptr);
	lheap	*lh			= lheap::get();
	lheap	*target_lh	= dc->owner;

	if (likely(target_lh ==	lh)) {
		lh->local_free(dc, ptr);
	} else if (likely(target_lh	!= LARGE_OWNER)) {
		lh->remote_free(dc,	ptr);
	} else {
		large_free(ptr);
	}
}
