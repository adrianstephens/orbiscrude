#ifndef VM_H
#define VM_H

#include "base/defs.h"
#include <sys/mman.h>

struct vmem {
	enum STATE {
		NONE			= 0,
		READ			= PROT_READ			<< 8,
		WRITE			= PROT_WRITE		<< 8,
		READWRITE		= READ | WRITE,
	};
	
	vmem(void *p) {
	}

	void	*base() const	{ return 0; }
	size_t	size()	const	{ return 0; }
	STATE	state()	const	{ return NONE; }
	
	static inline void*	reserve(size_t size)					{ return mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0); }
	static inline void*	commit(void* p, size_t size)			{ return p; }
	static inline void*	reserve_commit(size_t size)				{ return commit(reserve(size), size); }
	static inline bool	decommit(void* p, size_t size)			{ return true; }
	static inline bool	release(void* p, size_t size)			{ return munmap(p, size) == 0; }
	static inline bool	decommit_release(void* p, size_t size)	{ return decommit(p, size) && release(p, size); }
};


#endif // VM_H
