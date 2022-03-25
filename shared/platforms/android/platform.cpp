#include "base/defs.h"
#include <android/log.h>

namespace iso {

void _iso_dump_heap(unsigned int i) {}

void __iso_debug_print(void *p, const char *s)	{
	__android_log_write(ANDROID_LOG_DEBUG, "isopod", s);
}


//-----------------------------------------------------------------------------
//	unaligned allocs
//-----------------------------------------------------------------------------

void	free(void *p) {
	::free(p);
}

void*	malloc(size_t size) {
	return size ? ::malloc(size) : 0;
}

void*	realloc(void *p, size_t size) {
	return ::realloc(p, size);
}

void*	resize(void *p, size_t size) {
	if (size == 0)
		::free(p);
	else if (size < malloc_usable_size(p))
		return p;
	return 0;
}


//-----------------------------------------------------------------------------
//	aligned allocs
//-----------------------------------------------------------------------------

void aligned_free(void *p) {
	::free(p);
}

void* aligned_alloc(size_t size, size_t align) {
	void	*p;
	return posix_memalign(&p, align, size) == 0 ? p : 0;
}

void* aligned_alloc_unchecked(size_t size, size_t align) {
	void	*p;
	return posix_memalign(&p, align, size) == 0 ? p : 0;
}

void* aligned_realloc(void *p, size_t size, size_t align) {
	if (size == 0) {
		::free(p);
		return 0;
	}
	size_t usable = malloc_usable_size(p);
	if (size < usable && (intptr_t(p) & (align - 1)) == 0)
		return p;

	void *p2 = aligned_alloc(size, align);
	memcpy(p2, p, usable);
	::free(p);
	return p2;
}

void* aligned_resize(void *p, size_t size, size_t align) {
	if (size == 0)
		::free(p);
	else if (size < malloc_usable_size(p) && (intptr_t(p) & (align - 1)) == 0)
		return p;
	return 0;
}

} // iso
