#ifndef VM_H
#define VM_H

#include "base/defs.h"

namespace iso {
struct vmem : MEMORY_BASIC_INFORMATION {
	enum STATE {
		COMMIT		= MEM_COMMIT,
		RESERVE		= MEM_RESERVE,
		FREE		= MEM_FREE,
		IMAGE		= MEM_IMAGE,
		PRIVATE		= MEM_PRIVATE,
		MAPPED		= MEM_MAPPED,
	};
	vmem(void *p)			{ VirtualQuery(p, this, sizeof(*this)); }
	void	*base() const	{ return BaseAddress; }
	size_t	size()	const	{ return RegionSize + ((char*)BaseAddress - (char*)AllocationBase); }
	STATE	state()	const	{ return STATE(State | Type); }

	static uint32 granularity() {
		SYSTEM_INFO	info;
		GetSystemInfo(&info);
		return info.dwAllocationGranularity;
	}

	static inline void*	reserve(void *p, size_t size, size_t align) { return ::VirtualAlloc(p, size, MEM_RESERVE, PAGE_READWRITE); }
	static inline void*	reserve(size_t size)					{ return ::VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE); }
	static inline void*	commit(void* p, size_t size)			{ return ::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE); }
	static inline void*	reserve_commit(size_t size)				{ return ::VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }
	static inline bool	decommit(void* p, size_t size)			{ return !!::VirtualFree(p, size, MEM_DECOMMIT); }
	static inline bool	release(void* p)						{ return !!::VirtualFree(p, 0, MEM_RELEASE); }
	static inline bool	decommit_release(void* p, size_t size)	{ vmem v(p); return (v.state() & COMMIT) && p == v.base() && release(p); }
};

class mapped_file : public memory_block {
	HANDLE	hPage;

	void	_open(HANDLE h, uint64 offset, uint64 length, bool write) {
		if (length == 0) {
			GetFileSizeEx(h, (LARGE_INTEGER*)&length);
			length -= offset;
		}
		hPage		= CreateFileMapping(h, 0, write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, 0);

		uint64	o	= offset & -uint64(vmem::granularity());
		p			= (char*)MapViewOfFile(hPage, write ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ, uint32(o >> 32), uint32(o), length + (offset - o)) + (offset - o);
		n			= length;
	}
	void	_open(const char *filename, uint64 offset, uint64 length, bool write) {
#ifndef PLAT_WINRT
		HANDLE h	= CreateFileA(filename, GENERIC_READ | (write ? GENERIC_WRITE : 0), FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		_open(h, offset, length, write);
		CloseHandle(h);
#endif
	}
	void	_close() {
		if (hPage) {
			UnmapViewOfFile((void*)(uintptr_t(p) & -uint64(vmem::granularity())));
			CloseHandle(hPage);
		} else {
			free(p);
		}
	}
	void	_dup(malloc_block &&m) {
		hPage = 0;
		*(memory_block*)this = m.detach();
	}
	void	_dup(const mapped_file &m) {
		if (m.hPage) {
			DuplicateHandle(GetCurrentProcess(), m.hPage, GetCurrentProcess(), &hPage, 0, FALSE, DUPLICATE_SAME_ACCESS);
			p		= MapViewOfFile(hPage, FILE_MAP_READ, 0, 0, 0);
			n		= m.n;
		} else {
			_dup(malloc_block((const memory_block&)m));
		}
	}
public:
	mapped_file() : hPage(0) {}
	mapped_file(const char *filename, uint64 offset = 0, uint64 length = 0, bool write = false)		{ _open(filename, offset, length, write); }
	mapped_file(HANDLE h, bool write = false)										{ uint64 offset = 0; SetFilePointerEx(h, (LARGE_INTEGER&)offset, (LARGE_INTEGER*)&offset, FILE_CURRENT); _open(h, offset, 0, write); }
	mapped_file(const mapped_file &m)												{ _dup(m); }
	mapped_file(mapped_file &&m) : memory_block((const memory_block&)m), hPage(m.hPage)	{ m.hPage = 0; m.p = 0; }
	mapped_file(malloc_block &&m)													{ _dup(move(m)); }
	~mapped_file()																	{ _close(); }
	mapped_file &operator=(const mapped_file &m)									{ _close(); _dup(m); return *this; }
	mapped_file &operator=(mapped_file &&m)											{ swap(*this, m); return *this; }
//	mapped_file	&open(const char *filename, uint64 offset = 0, bool write = false)	{ _close(); _open(filename, offset, write); return *this; }
//	mapped_file	&open(HANDLE h, uint64 offset = 0, bool write = false)				{ _close(); _open(h, offset, write); return *this; }
	friend void swap(mapped_file &a, mapped_file &b)								{ raw_swap(a, b); }
};

} //namespace iso

#endif // VM_H
