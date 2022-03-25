#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

namespace cgc {

// default alignment and chunksize, if called with 0 arguments
#define CHUNK	(64*1024)
#define ALIGN	8

struct chunk {
	chunk	*next;
};

struct cleanup {
	cleanup	*next;
	void	(*fn)(void*);
	void	*arg;
	
	cleanup(MemoryPoolCleanup *_fn, void *_arg) : fn(_fn), arg(_arg) {}
};

struct MemoryPool : chunk {
	uintptr_t	free, end;
	size_t		chunksize;
	unsigned	align;
	cleanup		*cleanups;
	
	MemoryPool(size_t _chunksize, unsigned _align) : cleanups(0), chunksize(_chunksize), align(_align - 1) {
		next	= 0;
		free	= ((uintptr_t)(this + 1) + align) & ~uintptr_t(align);
		end		= (uintptr_t)this + chunksize;
	}
	
	~MemoryPool() {
		for (cleanup *c = cleanups; c; c = c->next)
			c->fn(c->arg);
		for (chunk *p = next, *n; p; p = n) {
			n = p->next;
			delete p;
		}
	}

	void *Alloc(size_t size);
	void *Realloc(void *old, size_t oldsize, size_t newsize);
	bool AddCleanup(MemoryPoolCleanup *fn, void *arg);
};

void *MemoryPool::Alloc(size_t size) {
    size = (size + align) & ~size_t(align);
    if (size <= 0)
		size = align;
		
    uintptr_t rv = free;
	free += size;
	
    if (free > end) {
		free	= rv;
        size_t	minreq = (size + sizeof(chunk) + align) & ~size_t(align);
		chunk	*ch;
        if (minreq >= chunksize) {
            // request size is too big for the chunksize, so allocate it as a single chunk of the right size
            ch = new(minreq) chunk;
            if (!ch)
				return 0;
        } else {
            ch = new (chunksize) chunk;
            if (!ch)
				return 0;
            free	= (uintptr_t)ch + minreq;
            end		= (uintptr_t)ch + chunksize;
        }
        ch->next	= next;
        next		= ch;
        rv			= ((uintptr_t)(ch + 1) + align) & ~uintptr_t(align);
    }
    return (void*)rv;
}

void *MemoryPool::Realloc(void *old, size_t oldsize, size_t newsize) {
    oldsize = (oldsize + align) & ~size_t(align);
    newsize = (newsize + align) & ~size_t(align);
	
    uintptr_t	oldfree = free;

    if ((uintptr_t)old + oldsize == free) {
        // this was the last chunk allocated, so deallocate it first
        // but remember in case the later alloc fails...
        free	= (uintptr_t)old;
    }
	void	*newp = Alloc(newsize);
	if (newp) {
		if (newp != old)
			memcpy(newp, old, oldsize);

	} else {
		free = oldfree;
	}
	return newp;
}

bool MemoryPool::AddCleanup(MemoryPoolCleanup *fn, void *arg) {
	free = (free + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
	cleanup * c = new(this) cleanup(fn, arg);
	if (!c)
		return false;
	c->next		= cleanups;
	cleanups	= c;
	return true;
}

MemoryPool *mem_CreatePool(size_t chunk, unsigned align) {
	if (align == 0)
		align = ALIGN;
	if (chunk == 0)
		chunk = CHUNK;
	if ((align & (align-1)) || chunk < sizeof(MemoryPool) || (chunk & (align-1)))
		return 0;
		
	return new(chunk) MemoryPool(chunk, align);
}

void mem_FreePool(MemoryPool *pool) {
	delete pool;
}

void *mem_Alloc(MemoryPool *pool, size_t size) {
	return pool->Alloc(size);
}

void *mem_Realloc(MemoryPool *pool, void *old, size_t oldsize, size_t newsize) {
	return pool->Realloc(old, oldsize, newsize);
}

bool mem_AddCleanup(MemoryPool *pool, MemoryPoolCleanup *fn, void *arg) {
	return pool->AddCleanup(fn, arg);
}

} //namespace cgc
