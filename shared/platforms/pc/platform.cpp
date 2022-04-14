#include <excpt.h>
#include <windows.h>
#include "base/defs.h"
#include "base/pointer.h"
#include "base/strings.h"
#include "windows/nt.h"
#include "hook.h"

//#define HEAP_VALIDATION

#if defined _MSC_VER && _MSC_VER >= 1900
#include "stdio.h"
#pragma comment(lib, "legacy_stdio_definitions")
//extern "C" FILE * __cdecl __iob_func(void) {
//	static FILE _iob[] = { *stdin, *stdout, *stderr };
//	return _iob;
//}
#endif

#include "delayimp.h"

extern "C"
FARPROC WINAPI __delayLoadHelper2(PCImgDelayDescr pidd, FARPROC *ppfnIATEntry) {
	FARPROC pfnRet = NULL;
	return pfnRet;
}

namespace iso {

void __break() {
	for (;;);
}

void __iso_debug_print(void*, const char *s)		{ OutputDebugStringA(s); }

HINSTANCE	hinst	= GetLocalInstance();
void		SetDefaultInstance(HINSTANCE h)			{ hinst = h;			}
HINSTANCE	GetDefaultInstance()					{ return hinst;			}
HINSTANCE	GetInstance(HINSTANCE h)				{ return h ? h : hinst; }

const void *WalkHeap(HANDLE h) {
	PROCESS_HEAP_ENTRY Entry;
	if (!HeapLock(h))
		return (void*)1;

	Entry.lpData = NULL;
	while (HeapWalk(h, &Entry)) {
		if (Entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
			if (!HeapValidate(h, 0, Entry.lpData))
				return Entry.lpData;
		}
	}

	auto	LastError = GetLastError();
	if (!HeapUnlock(h))
		return (void*)2;

	return LastError == ERROR_NO_MORE_ITEMS ? nullptr : (void*)LastError;
}

struct HeapValidator {
#ifdef HEAP_VALIDATION
	static int mode;
	
	static void Validate(HANDLE h, int mode) {
		switch (mode) {
			case 1:
				ISO_ALWAYS_ASSERT(HeapValidate(h, 0, 0));
				break;
			case 2: {
				auto	err = WalkHeap(h);
				ISO_ALWAYS_ASSERT(!err);
			}
			default:
				break;
		}
	}
	
	HANDLE h;

	HeapValidator(HANDLE h = GetProcessHeap()) : h(h) {
		Validate(h, mode & 15);
	}
	~HeapValidator() {
		Validate(h, mode >> 4);
	}
#else
	HeapValidator()			{}
	HeapValidator(HANDLE h)	{}
#endif
};

#ifdef HEAP_VALIDATION
int HeapValidator::mode = 0;
#endif


#ifdef PLAT_WINRT
iso_export void _iso_dump_heap(uint32 flags)		{}
#else
//iso_export void _iso_dump_heap(uint32 flags)		{ _CrtCheckMemory();	}
iso_export void _iso_dump_heap(uint32 flags)		{ ISO_ALWAYS_ASSERT(HeapValidate(GetProcessHeap(), 0, 0));	}
#endif

struct RtlpHeapFailureInfo {
	uint32	a0, a1;	//0x00
	uint64	b;		//0x08
	uint64	c;		//0x10
	uint64	addr;	//0x18
	uint64	e;		//0x20
	uint64	f;		//0x28
	uint64	a;		//0x30
	uint64	addr2;	//0x38
};

//-----------------------------------------------------------------------------
//	unaligned allocs
//-----------------------------------------------------------------------------

struct HEAP_ENTRY1 {
	HEAP_ENTRY1	*next;
	HEAP_ENTRY1	*prev;
	uint64		a, b;
	uint32		c, d;
};

struct HEAP_ENTRY : NT::_HEAP_UNPACKED_ENTRY {
	size_t	fix_size(uint32 t) const {
		size_t	unused = UnusedBytes & 0x40				? ((const NT::_HEAP_EXTENDED_ENTRY*)this + (UnusedBytes & 0x3f))->UnusedBytesLength
					:	(UnusedBytes & 0x3f) == 0x3f	? (size_t)(this + t)->PreviousBlockPrivateData
					:	UnusedBytes & 0x3f;
		return (t << 4) - unused;
	}

#ifndef PLAT_WINRT
	size_t	get_size(NT::_HEAP *heap) const {
		static dll_variable<uint32>	RtlpLFHKey("ntdll.dll", 4, 0x000074F8 - 0x1000);

		if (UnusedBytes == 5)
			return (this - SegmentOffset)->get_size(heap);

		uint32	code = SubSegmentCode;

		if (UnusedBytes == 4) {
			if (heap->EncodeFlagMask & code)
				code ^= heap->Encoding.SubSegmentCode;
			return ((uint64*)this)[-2] - (code & 0xffff);
		}

		ISO_ASSERT((UnusedBytes & 0x3f) != 0);

		if (UnusedBytes & 0x80) {
			code ^= uint32(intptr_t(this) >> 4);
			code ^= RtlpLFHKey;
			code ^= (uint32)intptr_t(heap);

			ISO_ASSERT((code & 0xffff) == 0);				//07FFDE963C8B3h

			auto	*p1 = (NT::_HEAP_SUBSEGMENT**)((char*)this - (code >> 12));
			auto	*p2 = *p1;
			return fix_size(p2->BlockSize);
		}

		if (heap->EncodeFlagMask & code)
			code ^= heap->Encoding.SubSegmentCode;
		return fix_size(code & 0xffff);

	}
#endif
};

size_t	alloc_size(void *p) {
	return HeapSize(GetProcessHeap(), 0, p);
}

__declspec(allocator) void*	malloc(size_t size) {
	HeapValidator	validator;
	return size ? HeapAlloc(GetProcessHeap(), 0, size) : nullptr;
}

iso_export void free(void *p) {
	if (p) {
		HeapValidator	validator;
		memset(p, 0xcd, alloc_size(p));
		HeapFree(GetProcessHeap(), 0, p);
	}
}

iso_export void free(void* p, size_t) {
	if (p) {
		HeapValidator validator;
		memset(p, 0xcd, alloc_size(p));
		HeapFree(GetProcessHeap(), 0, p);
	}
}

iso_export void* realloc(void* p, size_t size) {
	if (!p)
		return malloc(size);

	if (!size) {
		free(p);
		return nullptr;
	}

	HeapValidator	validator;
//	size_t	size0	= HeapSize(GetProcessHeap(), 0, p);
//	if (size0 >= size)
//		return p;

	void *p0 = p;
	p = HeapReAlloc(GetProcessHeap(), 0, p, size);
	if (!p)
		HeapFree(GetProcessHeap(), 0, p0);
	return p;
}

iso_export void* resize(void *p, size_t size) {
	HeapValidator	validator;
	return p && HeapReAlloc(GetProcessHeap(), HEAP_REALLOC_IN_PLACE_ONLY, p, size) ? p : 0;
}

//-----------------------------------------------------------------------------
//	aligned allocs
//-----------------------------------------------------------------------------

static inline size_t adjust_size(size_t size, size_t align) {
	return size ? size + 8 + max(align, 8) - 1 : 0;
}

static inline void	*get_alloc_block(void *p) {
	return ((void**)p)[-1];
}

static inline void	*get_aligned_block(void *p, size_t align) {
	if (p) {
		size_t	mask	= max(align, 8) - 1;
		void	**p2 = (void**)(((uintptr_t)p + 8 + mask) & ~mask);
		p2[-1]	= p;
		p		= p2;
	}
	return p;
}

void aligned_free(void *p) {
	HeapValidator	validator;
	if (p)
		free(get_alloc_block(p));
}

__declspec(allocator) void*	aligned_alloc_unchecked(size_t size, size_t align) {
	return size ? get_aligned_block(HeapAlloc(GetProcessHeap(), 0, adjust_size(size, align)), align) : 0;
}

__declspec(allocator) void*	aligned_alloc(size_t size, size_t align) {
	HeapValidator	validator;
	return size ? get_aligned_block(HeapAlloc(GetProcessHeap(), 0, adjust_size(size, align)), align) : 0;
}

void* aligned_realloc(void *p, size_t size, size_t align) {
	HeapValidator	validator;
	if (!p) {
		p = HeapAlloc(GetProcessHeap(), 0, adjust_size(size, align));
	} else {
		void	*p0		= get_alloc_block(p);
		size_t	size0	= HeapSize(GetProcessHeap(), 0, p0);
		if ((char*)p0 + size0 >= (char*)p + size)
			return p;

		p = HeapReAlloc(GetProcessHeap(), 0, p0, adjust_size(size, align));
		if (!p) {
			HeapFree(GetProcessHeap(), 0, p0);
			return p;
		}
	}
	return get_aligned_block(p, align);
}

void* aligned_resize(void *p, size_t size, size_t align) {
	HeapValidator	validator;
	if (!p)
		return 0;
	void	*p0		= get_alloc_block(p);
	size_t	size0	= HeapSize(GetProcessHeap(), 0, p0);
	size_t	size1	= ((char*)p - (char*)p0) + size;
	return size0 >= size1 || HeapReAlloc(GetProcessHeap(), HEAP_REALLOC_IN_PLACE_ONLY, p0, size1) ? p : 0;
}

//-----------------------------------------------------------------------------
//	errors
//-----------------------------------------------------------------------------

size_t to_string(char *s, const Win32Error &v)	{
	return FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		v.err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		s, 256,
		v.args
	);
}

string_accum &operator<<(string_accum &sa, const Win32Error &v)	{
	char	*msg;
	if (FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		v.err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&msg, 0,
		v.args
	)) {
		sa << msg;
		LocalFree(msg);
	}
	return sa;
}

iso_export void com_error_handler(long hr) {
	ISO_TRACEF("FAIL with HRESULT 0x%08x: ", hr) << Win32Error(hr) << '\n';
}

void Win32ErrorPrint(const Win32Error &v) {
	ISO_TRACEF() << v;
}

void throw_hresult(HRESULT hr) {
	throw_accum(Win32Error(hr));
}

//-----------------------------------------------------------------------------
//	misc
//-----------------------------------------------------------------------------

bool IsProcessElevated() {
	Win32Handle		h;
	TOKEN_ELEVATION	elevation;
	DWORD			size;
	return OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h)
		&& GetTokenInformation(h, TokenElevation, &elevation, sizeof(elevation), &size)
		&& elevation.TokenIsElevated;
}

uint64	random_seed()	{
	FILETIME t;
	GetSystemTimeAsFileTime(&t);
	return (uint64&)t;
}

} // namespace iso
