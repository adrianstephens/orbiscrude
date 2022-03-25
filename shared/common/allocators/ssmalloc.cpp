#include "base/atomic.h"
#include "thread.h"
#include "pool.h"
#include "vm.h"

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

int		get_core_id() {
	return Thread::processor();
}

struct Mappings {
	int		cls2size[128];
	char	sizemap[256];
	char	sizemap2[128];
	
	Mappings() {
		int	cls	= 0;
		for	(int size =	8; size <= 64; size += 4)
			cls2size[cls++]	= size;
		for	(int size =	64 + 16; size <= 128; size += 16)
			cls2size[cls++]	= size;
		for	(int size =	128	+ 32; size <= 256; size	+= 32)
			cls2size[cls++]	= size;
		for	(int size =	256; size <	65536; size	<<=	1) {
			cls2size[cls++] = size + (size >>	1);
			cls2size[cls++] = size << 1;
		}

		cls = 0;
		// init	sizemap
		for	(int size = 4; size <= 1024; size += 4)	{
			if (size > cls2size[cls])
				cls++;
			sizemap[(size -	1) >> 2] = cls;
		}
		// init	sizemap2
		for	(int size = 1024; size <= 65536; size += 512) {
			if (size > cls2size[cls])
				cls++;
			sizemap2[(size - 1) >> 9] = cls;
		}
	}
	
	int size2cls(size_t size) {
		return likely(size <= 1024)	? sizemap[(size	- 1) >>	2]
			: size	<= 65536		? sizemap2[(size - 1) >> 9]
			: LARGE_CLASS;
	}

} mappings;

struct lheap;
struct dchunk;

struct chunk : link_base<chunk> {
	uint32		numa_node;
};

// Data	chunk header
struct dchunk : chunk {
	// Read	Area
	lheap			*owner;
	uint8			size_cls;
	uint16			block_size;

	// Local Write Area
	uint16			free_count;
	uint16			count;
	uint16			free_mem;
	freelist<void>	free_lifo;

	// Remote Write	Area
	atomic<freelist<void>>	remote_free_head;

	dchunk(lheap *_owner, int cls) : owner(_owner) {
		change_cls(cls);
	}

	void change_cls(int cls) {
		size_cls		= cls;
		block_size		= mappings.cls2size[cls];
		count			= free_count = CHUNK_DATA_SIZE / block_size;
		free_mem		= DCHUNK_SIZE;
		free_lifo.reset();
	}
	void collect_garbage() {
		auto	h		= remote_free_head.reset();
		free_lifo.reset(h);
		free_count		= h.b;
	}
	void *alloc() {
		void *node = free_lifo.alloc();
		if (unlikely(!node)) {
			if (free_mem > DCHUNK_SIZE - block_size)
				return 0;

			node		= (char*)this + free_mem;
			free_mem	+= block_size;
		}
		--free_count;
		return node;
	}
	static dchunk *get(void *ptr) {
		return (dchunk*)((uintptr_t)ptr - ((uintptr_t)ptr % CHUNK_SIZE));
	}
};

struct gpool {
	Mutex			lock;
	char*			pool_start;
	atomic<char*>	pool_end;
	atomic<char*>	free_start;
	atomic<freelist<void>>	free_dc_head[MAX_CORE_ID];
	atomic<freelist<void>>	free_lh_head[MAX_CORE_ID];
	int				last_alloc;

	// Initialize the global memory	pool
	gpool() : last_alloc(8) {
		//pthread_key_create(&destructor,	thread_exit);
	#ifdef RETURN_MEMORY
		pthread_create(&gpool_gc_thread, NULL, gpool_gc, gpool);
	#endif

		pool_start	= RAW_POOL_START;
		pool_end	= RAW_POOL_START;
		free_start	= RAW_POOL_START;

		grow();
	}

	int		grow() {
		// Enlarge the raw memory pool
		int	alloc_size = ALLOC_UNIT	* last_alloc;
		if (last_alloc < 32)
			last_alloc *= 2;

		void *mem =	vmem::commit(vmem::reserve((void*)pool_end, alloc_size, PAGE_SIZE), alloc_size);
		if (!mem) {
			exit(-1);
			return -1;
		}

		// Increase	the	global pool	size
		pool_end +=	alloc_size;
		return 0;
	}

	// increase the global pool size
	chunk	*make_raw_chunk()	{
		char	*p = free_start += CHUNK_SIZE;
		if (pool_end <=	p)	{
			// Global Pool Full
			lock.lock();
			while (pool_end	<= p)
				grow();
			lock.unlock();
		}

		//touch memory
		for	(char *ptr = p - CHUNK_SIZE, *end = p; ptr < end; ptr += PAGE_SIZE)
			*ptr = 0;

		return (chunk*)(p - CHUNK_SIZE);
	}
	
	chunk	*acquire_chunk() {
		// Try to alloc	a freed	chunk from the free	list
		chunk *c = (chunk*)free_dc_head[get_core_id()].alloc();
		if (!c) {
			c				= make_raw_chunk();
			c->numa_node	= get_core_id();
		}
		return c;
	}
	void	release_chunk(chunk *c) {
		free_dc_head[c->numa_node].release(c);
	}
	lheap	*acquire_lheap() {
		return (lheap*)free_lh_head[get_core_id()].alloc();
	}
	void	release_lheap(chunk *c) {
		free_lh_head[c->numa_node].release(c);
	}
};

// Global metadata
gpool			global_pool;


// Per-thread data chunk pool
struct lheap : chunk {

	struct obj_buf {
		dchunk			*dc;
		freelist<void>	free_lifo;
		obj_buf() : dc(0) {}
		void flush() {
			void *prev	= dc->remote_free_head.reset(free_lifo.reset());
			// If I am the first thread	done remote	free in	this memory	chunk
			if (!prev)
				dc->owner->need_gc[dc->size_cls].release(dc);
			dc			= 0;
			return;
		}
		void put(dchunk *_dc, void *ptr) {
			if (unlikely(dc != _dc)) {
				if (dc)
					flush();
				dc	= _dc;
			}
			free_lifo.release(ptr);
		}
		bool empty() const { return free_lifo.empty(); }
	};

	freelist<void>	free_lifo;
	uint32			free_dcs;
	uint32			remote_cnt;

	list_base<chunk>		background[DEFAULT_BLOCK_CLASS];
	obj_buf					block_bufs[BLOCK_BUF_CNT];
	atomic<freelist<void>>	need_gc[DEFAULT_BLOCK_CLASS];

	static thread_local	lheap	*instance;

	void replace_foreground(int cls) {
		dchunk *dc;
		// Try to acquire a	block in the remote	freed list
		if (dc = (dchunk*)need_gc[cls].alloc()) {
			dc->collect_garbage();

		// Try to acquire the chunk	from local pool
		} else if (dc = (dchunk*)free_lifo.alloc()) {
			free_dcs--;
			dc->change_cls(cls);

		// Acquire the chunk from global pool
		} else {
			dc	= new(global_pool.acquire_chunk()) dchunk(this, cls);
		}
		//Set the foreground chunk
		background[cls].push_front(dc);	//set_single
	}

	void flush_all() {
		for	(auto &i : block_bufs)	{
			if (!i.empty())
				i.flush();
		}
	}

	void *alloc(int cls) {
		auto &dclist = background[cls];

		for (;;) {
			dchunk	*dc	= (dchunk*)&dclist.front();

			if (!dc) {
				replace_foreground(cls);
				continue;
			}

			if (void *ret = dc->alloc())
				return ret;

			// datachunk is full
			dc->unlink();
		}
	}

	void local_free(dchunk *dc, void *ptr) {
		uint32 free_count = ++dc->free_count;
		dc->free_lifo.release(ptr);
		
		auto &dclist = background[dc->size_cls];

		if (dc != &dclist.front()) {
			if (unlikely(free_count == 1)) {	// was full?
				dclist.push_front(dc);
			} else if (unlikely(free_count == dc->count)) {//now empty?
				dc->unlink();
				if (free_dcs >=	MAX_FREE_CHUNK)	{
					global_pool.release_chunk(dc);
				} else {
					free_lifo.release(dc);
					++free_dcs;
				}
			}
		}
	}

	void remote_free(dchunk *dc, void *ptr) {
		// Put the object in a local buffer	rather than	return it to owner
		int		tag		= ((uint64)dc / CHUNK_SIZE)	% BLOCK_BUF_CNT;
		block_bufs[tag].put(dc, ptr);

		// Periodically	flush buffered remote objects
		if ((++remote_cnt & 0xFFFF) == 0)
			flush_all();
	}

	static lheap	*get() {
		if (unlikely(!instance)) {
			// Try to alloc	a freed	thread pool	from the list
			lheap	*lh = global_pool.acquire_lheap();
			if (!lh)
				lh = new(global_pool.acquire_chunk()) lheap;
			instance = lh;
		}
		return instance;
	}
};


struct large_header {
	size_t	alloc_size;
	void	*mem;
};

inline static void *large_malloc(size_t	size) {
	size_t	alloc_size	= align(size + sizeof(large_header), PAGE_SIZE);
	void	*mem		= vmem::reserve_commit(alloc_size);

	large_header *header = (large_header*)mem;
	header->mem			= mem;
	header->alloc_size	= alloc_size;
	return header + 1;
}

static void	*large_aligned_alloc(size_t	size, size_t _align) {
	size_t	alloc_size	= align(align(size + sizeof(large_header), _align), PAGE_SIZE);
	void	*mem		= vmem::reserve_commit(alloc_size);

	large_header *header = align((large_header*)mem + 1, _align) - 1;
	header->mem			= mem;
	header->alloc_size	= alloc_size;
	return header + 1;
}

void *ss_alloc(size_t size) {
	if (size == 0)
		return 0;

	int	cls	= mappings.size2cls(size);
	return likely(cls < DEFAULT_BLOCK_CLASS)
		? lheap::get()->alloc(cls)
		: large_malloc(size);
}

void *ss_aligned_alloc(size_t size, size_t align) {
	if (align <= 256 && size <= 65536)
		// In this case, we	handle it as small allocations
		return lheap::get()->alloc(mappings.size2cls(max(align, size)));

	// Handle it as a special large	allocation
	return large_aligned_alloc(size, align);
}

void ss_free(void *ptr) {
	if (!ptr)
		return;

	if (between(ptr, global_pool.pool_start, global_pool.pool_end)) {
		dchunk	*dc	= dchunk::get(ptr);
		lheap	*lh	= lheap::get();

		if (likely(dc->owner ==	lh))
			lh->local_free(dc, ptr);
		else
			lh->remote_free(dc,	ptr);

	} else {
		//large_free(ptr);
		large_header *header = (large_header*)ptr - 1;
		vmem::decommit_release(header->mem, header->alloc_size);
	}
}

size_t ss_size(void *ptr) {
	if (ptr	== NULL)
		return 0;

	if (between(ptr, global_pool.pool_start, global_pool.pool_end)) {
		dchunk *dc = dchunk::get(ptr);
		return mappings.cls2size[dc->size_cls];

	} else {
		large_header *header = (large_header*)ptr - 1;
		return header->alloc_size - ((uintptr_t)ptr - (uintptr_t)header->mem);
	}
}

void *ss_realloc(void *ptr, size_t size) {
	// Handle special cases
	if (ptr	== NULL)
		return ss_alloc(size);

	if (size ==	0) {
		ss_free(ptr);
		return 0;
	}

	if (between(ptr, global_pool.pool_start, global_pool.pool_end)) {
		dchunk *dc = dchunk::get(ptr);
		int	old_size = mappings.cls2size[dc->size_cls];

		// Not exceed the current size,	return
		if (size <=	old_size)
			return ptr;

		// Alloc a new block
		void *new_ptr =	ss_alloc(size);
		memcpy(new_ptr,	ptr, old_size);
		ss_free(ptr);
		return new_ptr;

	} else {
		large_header	*header = (large_header*)ptr - 1;
		size_t			alloc_size	= header->alloc_size;
		void			*mem		= header->mem;
		size_t			offset		= (uintptr_t)ptr - (uintptr_t)mem;
		size_t			old_size	= alloc_size - offset;

		// doesn't exceed the current size,return
		if (size <=	old_size)
			return ptr;

		// Try to remap
		int		new_size	= align(size + sizeof(large_header), PAGE_SIZE);
		if (vmem::decommit(mem, alloc_size) && vmem::commit(NULL, new_size)) {
			header			= (large_header*)mem - 1;
			header->alloc_size	= new_size;
			header->mem			= mem;
			return header - 1;
		}

		void* new_ptr =	large_malloc(size);
		memcpy(new_ptr,	ptr, old_size);
		vmem::decommit_release(mem, alloc_size);
		return new_ptr;
	}
}

