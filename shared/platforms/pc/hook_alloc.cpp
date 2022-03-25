#include "base/atomic.h"
#include "base/strings.h"
#include "base/skip_list.h"
#include "base/tree.h"
#include "hook.h"
#include "hook_stack.h"
#include "dbghelp.h"
#include "thread.h"
#include "filename.h"
#include "windows/registry.h"
#include "com.h"
#include "windows/nt.h"

#include <intrin.h>

using namespace iso;

#ifdef _M_X64
#define ADDRESSFORMAT	"0x%I64x"	// Format string for 64-bit addresses
#else
#define ADDRESSFORMAT	"0x%.8X"	// Format string for 32-bit addresses
#endif

#undef _malloc_dbg
#undef _calloc_dbg
#undef _realloc_dbg
#undef _recalloc_dbg
#undef _expand_dbg
#undef _free_dbg
#undef _aligned_malloc_dbg
#undef _aligned_realloc_dbg
#undef _aligned_recalloc_dbg
#undef _aligned_free_dbg
#undef _aligned_offset_malloc_dbg
#undef _aligned_offset_realloc_dbg
#undef _aligned_offset_recalloc_dbg

//-----------------------------------------------------------------------------
//	windows structures
//-----------------------------------------------------------------------------

typedef LONG		NTSTATUS;

// Memory block header structure used internally by the debug CRT
struct crtdbgblockheader {
	enum {
		FREE		= 0,	// This block has been freed.
		NORMAL		= 1,	// This is a normal (user) block.
		INTERNAL	= 2,	// This block is used internally by the CRT.
		IGNORED		= 3,	// This block is a specially tagged block that is ignored during some debug error checking.
		CLIENT		= 4,	// This block is a specially tagged block with special use defined by the user application.
	};
	crtdbgblockheader	*next, *prev;
	char				*file;
	int					line;
#ifdef _M_X64
	int					use;
	size_t				size;
#else
	size_t				size;
	int					use;
#endif
	long				request;	// This block's "request" number. Basically a serial number.
	uint8				gap[4];
};

//-----------------------------------------------------------------------------
//	MallocHooks
//-----------------------------------------------------------------------------

#define STACK_COMPRESSION
struct patchentry {
	const char	*name;
	void		*dest;
};

struct MallocHooks : public IMalloc {
	enum FLAGS {
		ENABLED				= 1 << 0,
		INITIALISED			= 1 << 1,
		OUTPUT_EVENTS		= 1 << 2,
		OUTPUT_ERRORS		= 1 << 3,
		DUMP_ALLOC_ALLOCD	= 1 << 4,
		DUMP_FREE_UNKNOWN	= 1 << 5,
		DUMP_FREE_FREED		= 1 << 6,
	};

	struct tls_data {
		bool			in_use;
		tls_data() : in_use(false) {}
		bool	use() {
			if (in_use)
				return false;
			return in_use = true;
		}
	};

	struct heap_val {
		static HANDLE	heap;
		void	*operator new(size_t size)	{ return HeapAlloc(heap, 0, size); }
		void	operator delete(void *p)	{ HeapFree(heap, 0, p); }
	};

	struct entry : heap_val {
		entry	*next;
		size_t	size;
		uint32	event;
#ifdef STACK_COMPRESSION
		CompressedCallStacks::Stack<64>	stack;
#else
		CallStacks::Stack<32>			stack;
#endif
		entry(entry *_next, uint32 _event, size_t _size) : next(_next), size(_size), event(_event) {}
		bool	is_free() const { return size == 0; }
	};
	
	struct entry_list : heap_val {
		entry	*last;
		entry_list() : last(0) {}
	};

	static MallocHooks		*me;
	static TLS<tls_data>	tls;
	IMalloc					*imalloc;
	atomic<uint32>			event;
	uint32					dump_event;
#ifdef STACK_COMPRESSION
	CompressedCallStacks	cs;
#else
	CallStacks				cs;
#endif
	CallStackDumper			stack_dumper;
	skip_list<void*, entry_list>	allocs;
	CriticalSection			allocsLock;
	flags<FLAGS>			flags;

	struct tls_use {
		bool		ok;
		tls_use() : ok(tls->use() && me->flags.test(ENABLED))	{}
		~tls_use()					{ if (ok) tls->in_use = false; }
		operator bool() const		{ return ok; }
	};

	struct kernel32 {
		static patchentry patches[];
		HANDLE		(__stdcall*	HeapCreate)			(DWORD options, size_t initsize, size_t maxsize);
		BOOL		(__stdcall*	HeapDestroy)		(HANDLE heap);
		void*		(__stdcall*	HeapAlloc)			(HANDLE heap, DWORD flags, size_t size);
		BOOL		(__stdcall*	HeapFree)			(HANDLE heap, DWORD flags, void *mem);
		void*		(__stdcall*	HeapReAlloc)		(HANDLE heap, DWORD flags, void *mem, size_t size);
		//just to skip lower callbacks
		HRSRC		(__stdcall*	FindResourceA)		(HMODULE hModule, const char *type, const char *name);
		HRSRC		(__stdcall*	FindResourceW)		(HMODULE hModule, const wchar_t *type, const wchar_t *name);
		HRSRC		(__stdcall*	FindResourceExA)	(HMODULE hModule, const char *type, const char *name, WORD language);
		HRSRC		(__stdcall*	FindResourceExW)	(HMODULE hModule, const wchar_t *type, const wchar_t *name, WORD language);
		HMODULE		(__stdcall*	LoadLibraryExA)		(const char *path, HANDLE file, DWORD flags);
		HMODULE		(__stdcall*	LoadLibraryExW)		(const wchar_t *path, HANDLE file, DWORD flags);
		HMODULE		(__stdcall*	LoadLibraryA)		(const char *path);
		HMODULE		(__stdcall*	LoadLibraryW)		(const wchar_t *path);
		BOOL		(__stdcall*	FreeLibrary)		(HMODULE hModule);
		FARPROC		(__stdcall*	GetProcAddress)		(HMODULE hModule, const char *name);
		DWORD		(__stdcall*	GetModuleFileNameA)	(HMODULE hModule, char *filename, DWORD size);
		DWORD		(__stdcall*	GetModuleFileNameW)	(HMODULE hModule, wchar_t *filename, DWORD size);
		HANDLE		(__stdcall*	FindFirstVolumeA)	(const char *name, DWORD length);
		HANDLE		(__stdcall*	FindFirstVolumeW)	(const wchar_t *name, DWORD length);
		BOOL		(__stdcall*	FindVolumeClose)	(HANDLE hFindVolume);
		HANDLE		(__stdcall*	CreateActCtxA)		(ACTCTXA *ctx);
		HANDLE		(__stdcall*	CreateActCtxW)		(ACTCTXW *ctx);

		void		(__stdcall*	ExitProcess)		(UINT code);
	} _kernel32;

	struct ntdll {
		static patchentry patches[];
		void*		(__stdcall*	RtlAllocateHeap)(HANDLE heap, DWORD flags, size_t size);
		BYTE		(__stdcall*	RtlFreeHeap)(HANDLE heap, DWORD flags, void *mem);
		void*		(__stdcall*	RtlReAllocateHeap)(HANDLE heap, DWORD flags, void *mem, size_t size);
		void*		(__stdcall*	RtlDebugAllocateHeap)(HANDLE heap, DWORD flags, size_t size);
		BYTE		(__stdcall*	RtlDebugFreeHeap)(HANDLE heap, DWORD flags, void *mem);
		void*		(__stdcall*	RtlDebugReAllocateHeap)(HANDLE heap, DWORD flags, void *mem, size_t size);
		
	} _ntdll;

	struct ole32 {
		static patchentry patches[];
		HRESULT		(__stdcall*	CoGetMalloc)(DWORD context, IMalloc **imalloc);
		void*		(__stdcall*	CoTaskMemAlloc)(size_t size);
		void*		(__stdcall*	CoTaskMemRealloc)(void *mem, size_t size);
	} _ole32;

	struct uxtheme {
		static patchentry patches[];
		HANDLE		(__stdcall*	OpenThemeData)(HWND hwnd, const wchar_t *list);
		HANDLE		(__stdcall*	OpenThemeDataEx)(HWND hwnd, const wchar_t *list, DWORD flags);
		HRESULT		(__stdcall*	CloseThemeData)(HANDLE hTheme);
		HRESULT		(__stdcall*	GetThemeBackgroundExtent)(HANDLE hTheme, HDC hdc, int part, int state, const RECT *content, RECT *extent);
		BOOL		(__stdcall*	GetThemeSysBool)(HANDLE hTheme, int id);
	} _uxtheme;

	struct msvcr {
		void		(__cdecl*	_free_dbg)(void *mem, char const *file, int line);
		void*		(__cdecl*	_calloc_dbg)(size_t num, size_t size, int type, char const *file, int line);
		void*		(__cdecl*	_malloc_dbg)(size_t size, int type, const char *file, int line);
		void*		(__cdecl*	_realloc_dbg)(void *mem, size_t size, int type, char const *file, int line);
		void*		(__cdecl*	_recalloc_dbg)(void *mem, size_t num, size_t size, int type, char const *file, int line);
		void*		(__cdecl*	_scalar_new_dbg)(size_t size, int type, char const *file, int line);
		void*		(__cdecl*	_vector_new_dbg)(size_t size, int type, char const *file, int line);

		void		(__cdecl*	free)(void *mem);
		void*		(__cdecl*	calloc)(size_t num, size_t size);
		void*		(__cdecl*	malloc)(size_t size);
		void*		(__cdecl*	realloc)(void *mem, size_t size);
		void*		(__cdecl*	_recalloc)(void *mem, size_t num, size_t size);
		void*		(__cdecl*	scalar_new)(size_t size);
		void*		(__cdecl*	vector_new)(size_t size);
		void		(__cdecl*	scalar_delete)(void *mem);
		void		(__cdecl*	vector_delete)(void *mem);

		void		(__cdecl*	_aligned_free_dbg)(void *mem, const char *file, int line);
		void*		(__cdecl*	_aligned_malloc_dbg)(size_t size, size_t alignment, int type, const char *file, int line);
		void*		(__cdecl*	_aligned_offset_malloc_dbg)(size_t size, size_t alignment, size_t offset, int type, const char *file, int line);
		void*		(__cdecl*	_aligned_realloc_dbg)(void *mem, size_t size, size_t alignment, int type, char const *file, int line);
		void*		(__cdecl*	_aligned_offset_realloc_dbg)(void *mem, size_t size, size_t alignment, size_t offset, int type, char const *file, int line);
		void*		(__cdecl*	_aligned_recalloc_dbg)(void *mem, size_t num, size_t size, size_t alignment, int type, char const *file, int line);
		void*		(__cdecl*	_aligned_offset_recalloc_dbg)(void *mem, size_t num, size_t size, size_t alignment, size_t offset, int type, char const *file, int line);

		void		(__cdecl*	_aligned_free)(void *mem);
		void*		(__cdecl*	_aligned_malloc)(size_t size, size_t alignment);
		void*		(__cdecl*	_aligned_offset_malloc)(size_t size, size_t alignment, size_t offset);
		void*		(__cdecl*	_aligned_realloc)(void *mem, size_t size, size_t alignment);
		void*		(__cdecl*	_aligned_offset_realloc)(void *mem, size_t size, size_t alignment, size_t offset);
		void*		(__cdecl*	_aligned_recalloc)(void *mem, size_t num, size_t size, size_t alignment);
		void*		(__cdecl*	_aligned_offset_recalloc)(void *mem, size_t num, size_t size, size_t alignment, size_t offset);
	} _msvcr[8];

	template<int N> struct hooked_msvcr {
		static patchentry patches[];
		static void		__cdecl		_free_dbg(void *mem, char const *file, int line);
		static void*	__cdecl		_calloc_dbg(size_t num, size_t size, int type, char const *file, int line);
		static void*	__cdecl		_malloc_dbg(size_t size, int type, const char *file, int line);
		static void*	__cdecl		_realloc_dbg(void *mem, size_t size, int type, char const *file, int line);
		static void*	__cdecl		_recalloc_dbg(void *mem, size_t num, size_t size, int type, char const *file, int line);
		static void*	__cdecl		scalar_new_dbg(size_t size, int type, char const *file, int line);
		static void*	__cdecl		vector_new_dbg(size_t size, int type, char const *file, int line);

		static void		__cdecl		free(void *mem);
		static void*	__cdecl		calloc(size_t num, size_t size);
		static void*	__cdecl		malloc(size_t size);
		static void*	__cdecl		realloc(void *mem, size_t size);
		static void*	__cdecl		_recalloc(void *mem, size_t num, size_t size);
		static void*	__cdecl		scalar_new(size_t size);
		static void*	__cdecl		vector_new(size_t size);
		static void		__cdecl		scalar_delete(void *mem);
		static void		__cdecl		vector_delete(void *mem);

		static void		__cdecl		_aligned_free_dbg(void *mem, const char *file, int line);
		static void*	__cdecl		_aligned_malloc_dbg(size_t size, size_t alignment, int type, const char *file, int line);
		static void*	__cdecl		_aligned_offset_malloc_dbg(size_t size, size_t alignment, size_t offset, int type, const char *file, int line);
		static void*	__cdecl		_aligned_realloc_dbg(void *mem, size_t size, size_t alignment, int type, char const *file, int line);
		static void*	__cdecl		_aligned_offset_realloc_dbg(void *mem, size_t size, size_t alignment, size_t offset, int type, char const *file, int line);
		static void*	__cdecl		_aligned_recalloc_dbg(void *mem, size_t num, size_t size, size_t alignment, int type, char const *file, int line);
		static void*	__cdecl		_aligned_offset_recalloc_dbg(void *mem, size_t num, size_t size, size_t alignment, size_t offset, int type, char const *file, int line);

		static void		__cdecl		_aligned_free(void *mem);
		static void*	__cdecl		_aligned_malloc(size_t size, size_t alignment);
		static void*	__cdecl		_aligned_offset_malloc(size_t size, size_t alignment, size_t offset);
		static void*	__cdecl		_aligned_realloc(void *mem, size_t size, size_t alignment);
		static void*	__cdecl		_aligned_offset_realloc(void *mem, size_t size, size_t alignment, size_t offset);
		static void*	__cdecl		_aligned_recalloc(void *mem, size_t num, size_t size, size_t alignment);
		static void*	__cdecl		_aligned_offset_recalloc(void *mem, size_t num, size_t size, size_t alignment, size_t offset);

	};

	static const patchentry *msvc_patches[];
	string					msvc_dlls[8];

	// kernel32
	static HANDLE	__stdcall	hooked_HeapCreate(DWORD options, size_t initsize, size_t maxsize);
	static BOOL		__stdcall	hooked_HeapDestroy(HANDLE heap);
	static void*	__stdcall	hooked_HeapAlloc(HANDLE heap, DWORD flags, size_t size);
	static BOOL		__stdcall	hooked_HeapFree(HANDLE heap, DWORD flags, void *mem);
	static void*	__stdcall	hooked_HeapReAlloc(HANDLE heap, DWORD flags, void *mem, size_t size);
	//just to skip lower callbacks
	static HRSRC	__stdcall	hooked_FindResourceA(HMODULE hModule, const char *type, const char *name)							{ return tls_use(), me->_kernel32.FindResourceA(hModule, type, name); }
	static HRSRC	__stdcall	hooked_FindResourceW(HMODULE hModule, const wchar_t *type, const wchar_t *name)						{ return tls_use(), me->_kernel32.FindResourceW(hModule, type, name); }
	static HRSRC	__stdcall	hooked_FindResourceExA(HMODULE hModule, const char *type, const char *name, WORD language)			{ return tls_use(), me->_kernel32.FindResourceExA(hModule, type, name, language); }
	static HRSRC	__stdcall	hooked_FindResourceExW(HMODULE hModule, const wchar_t *type, const wchar_t *name, WORD language)	{ return tls_use(), me->_kernel32.FindResourceExW(hModule, type, name, language); }
	static HMODULE	__stdcall	hooked_LoadLibraryExA(const char *path, HANDLE file, DWORD flags)									{ return tls_use(), me->_kernel32.LoadLibraryExA(path, file, flags); }
	static HMODULE	__stdcall	hooked_LoadLibraryExW(const wchar_t *path, HANDLE file, DWORD flags)								{ return tls_use(), me->_kernel32.LoadLibraryExW(path, file, flags); }
	static HMODULE	__stdcall	hooked_LoadLibraryA(const char *path)																{ return tls_use(), me->_kernel32.LoadLibraryA(path); }
	static HMODULE	__stdcall	hooked_LoadLibraryW(const wchar_t *path)															{ return tls_use(), me->_kernel32.LoadLibraryW(path); }
	static BOOL		__stdcall	hooked_FreeLibrary(HMODULE hModule)																	{ return tls_use(), me->_kernel32.FreeLibrary(hModule); }
	static FARPROC	__stdcall	hooked_GetProcAddress(HMODULE hModule, const char *name)											{ return tls_use(), me->_kernel32.GetProcAddress(hModule, name); }
	static DWORD	__stdcall	hooked_GetModuleFileNameA(HMODULE hModule, char *filename, DWORD size)								{ return tls_use(), me->_kernel32.GetModuleFileNameA(hModule, filename, size); }
	static DWORD	__stdcall	hooked_GetModuleFileNameW(HMODULE hModule, wchar_t *filename, DWORD size)							{ return tls_use(), me->_kernel32.GetModuleFileNameW(hModule, filename, size); }
	static HANDLE	__stdcall	hooked_FindFirstVolumeA(const char *name, DWORD length)												{ return tls_use(), me->_kernel32.FindFirstVolumeA(name, length); }
	static HANDLE	__stdcall	hooked_FindFirstVolumeW(const wchar_t *name, DWORD length)											{ return tls_use(), me->_kernel32.FindFirstVolumeW(name, length); }
	static BOOL		__stdcall	hooked_FindVolumeClose(HANDLE hFindVolume)															{ return tls_use(), me->_kernel32.FindVolumeClose(hFindVolume); }
	static HANDLE	__stdcall	hooked_CreateActCtxA(ACTCTXA *ctx)																	{ return tls_use(), me->_kernel32.CreateActCtxA(ctx); }
	static HANDLE	__stdcall	hooked_CreateActCtxW(ACTCTXW *ctx)																	{ return tls_use(), me->_kernel32.CreateActCtxW(ctx); }

	static void		__stdcall	hooked_ExitProcess(UINT code);

	//ntdll
	static void*	__stdcall	hooked_RtlAllocateHeap(HANDLE heap, DWORD flags, size_t size);
	static BYTE		__stdcall	hooked_RtlFreeHeap(HANDLE heap, DWORD flags, void *mem);
	static void*	__stdcall	hooked_RtlReAllocateHeap(HANDLE heap, DWORD flags, void *mem, size_t size);
	static void*	__stdcall	hooked_RtlDebugAllocateHeap(HANDLE heap, DWORD flags, size_t size);
	static BYTE		__stdcall	hooked_RtlDebugFreeHeap(HANDLE heap, DWORD flags, void *mem);
	static void*	__stdcall	hooked_RtlDebugReAllocateHeap(HANDLE heap, DWORD flags, void *mem, size_t size);

	//ole32
	static HRESULT	__stdcall	hooked_CoGetMalloc(DWORD context, IMalloc **imalloc);
	static void*	__stdcall	hooked_CoTaskMemAlloc(size_t size);
	static void*	__stdcall	hooked_CoTaskMemRealloc(void *mem, size_t size);

	//uxtheme
	static HANDLE	__stdcall	hooked_OpenThemeData(HWND hwnd, const wchar_t *list)												{ return /*tls_use(), */me->_uxtheme.OpenThemeData(hwnd, list); }
	static HANDLE	__stdcall	hooked_OpenThemeDataEx(HWND hwnd, const wchar_t *list, DWORD flags)									{ return /*tls_use(), */me->_uxtheme.OpenThemeDataEx(hwnd, list, flags); }
	static HRESULT	__stdcall	hooked_CloseThemeData(HANDLE hTheme)																{ return /*tls_use(), */me->_uxtheme.CloseThemeData(hTheme); }
	static HRESULT	__stdcall	hooked_GetThemeBackgroundExtent(HANDLE hTheme, HDC hdc, int part, int state, const RECT *content, RECT *extent) { return tls_use(), me->_uxtheme.GetThemeBackgroundExtent(hTheme, hdc, part, state, content, extent); }
	static BOOL		__stdcall	hooked_GetThemeSysBool(HANDLE hTheme, int id)														{ return tls_use(), me->_uxtheme.GetThemeSysBool(hTheme, id); }

	void init(const patchentry *patches, size_t num, void **original, const char *module_name) {
		filename	fn;
		HMODULE		h = FindModule(format_string("*\\%s", module_name), fn);
		ISO_OUTPUTF("Adding allocation patches to ") << module_name << onlyif(!h, " (not loaded)") << '\n';
		for (; num--; patches++, original++) {
			if (h && !GetProcAddress(h, patches->name))
				ISO_OUTPUTF("Didn't find ") << patches->name << '\n';
			*original = 0;
			AddHook(original, patches->name, module_name, patches->dest);
		}
	}
	void remove(const patchentry *patches, size_t num, void **original, const char *module_name) {
		ISO_OUTPUTF("Removing allocation patches from ") << module_name << '\n';
		for (; num--; patches++, original++)
			RemoveHook(original, patches->name, module_name, patches->dest);
	}

	template<typename T> void init(T &t, const char *module_name) {
		init(t.patches, sizeof(t) / sizeof(void*), (void**)&t, module_name);
	}
	template<typename T> void remove(T &t, const char *module_name) {
		remove(t.patches, sizeof(t) / sizeof(void*), (void**)&t, module_name);
	}

	uint32	next_event(context &ctx);
	
	void*	add_alloc(void *p, size_t size, context &ctx);
	void	add_free(void *p, context &ctx);
	void*	add_realloc(void *newp, void *oldp, size_t size, context &ctx);

	void	add_alloc(const NT::UNICODE_STRING &p, context &ctx) {
		if (p.Buffer)
			add_alloc(p.Buffer, p.MaximumLength, ctx);
	}

	void	get_callstack(entry *e, context &ctx);
	void	dump_callstacks(const entry *e);
	void	dump_callstack(const entry *e);

	//IMalloc
	ULONG	__stdcall	AddRef()			{ return imalloc->AddRef(); }
	ULONG	__stdcall	Release()			{ return imalloc->Release(); }
	HRESULT	__stdcall	QueryInterface(REFIID iid, void **object)  { return imalloc->QueryInterface(iid, object); }
	size_t	__stdcall	GetSize(void *mem)	{ return imalloc->GetSize(mem); }
	VOID	__stdcall	HeapMinimize()		{ imalloc->HeapMinimize(); }
	INT		__stdcall	DidAlloc(void *mem)	{ return imalloc->DidAlloc(mem); }

	LPVOID	__stdcall	Alloc(size_t size) {
		tls_use	u;
		void	*p = imalloc->Alloc(size);
		return p && u ? me->add_alloc(p, size, context(0)) : p;
	}
	VOID	__stdcall	Free(void *mem)	{
		tls_use	u;
		imalloc->Free(mem);
		if (mem && u)
			me->add_free(mem, context(0));
	}
	LPVOID	__stdcall	Realloc(void *mem, size_t size) {
		tls_use	u;
		void	*p = imalloc->Realloc(mem, size);
		return u ? me->add_realloc(p, mem, size, context(0)) : p;
	}

	void	CheckHeap(HANDLE heap);
	void	CheckHeaps();
	size_t	GetHeapUsed(HANDLE heap);

	MallocHooks(uint32 _flags);
	~MallocHooks();
};

MallocHooks *dummy = new MallocHooks(MallocHooks::ENABLED);

MallocHooks*				MallocHooks::me;
TLS<MallocHooks::tls_data>	MallocHooks::tls;
HANDLE						MallocHooks::heap_val::heap;
const patchentry*			MallocHooks::msvc_patches[] = {
	hooked_msvcr<0>::patches,
	hooked_msvcr<1>::patches,
	hooked_msvcr<2>::patches,
	hooked_msvcr<3>::patches,
	hooked_msvcr<4>::patches,
	hooked_msvcr<5>::patches,
	hooked_msvcr<6>::patches,
	hooked_msvcr<7>::patches,
};

#define PATCH(x)		{#x, hooked_##x}

patchentry MallocHooks::kernel32::patches[] = {
	PATCH(HeapCreate),
	PATCH(HeapDestroy),
	PATCH(HeapAlloc),
	PATCH(HeapFree),
	PATCH(HeapReAlloc),
	PATCH(FindResourceA),
	PATCH(FindResourceW),
	PATCH(FindResourceExA),
	PATCH(FindResourceExW),
	PATCH(LoadLibraryExA),
	PATCH(LoadLibraryExW),
	PATCH(LoadLibraryA),
	PATCH(LoadLibraryW),
	PATCH(FreeLibrary),
	{0, hooked_GetProcAddress},//PATCH(GetProcAddress),
	PATCH(GetModuleFileNameA),
	PATCH(GetModuleFileNameW),
	PATCH(FindFirstVolumeA),
	PATCH(FindFirstVolumeW),
	PATCH(FindVolumeClose),
	PATCH(CreateActCtxA),
	PATCH(CreateActCtxW),
	PATCH(ExitProcess),
};

patchentry MallocHooks::ntdll::patches[] = {
	PATCH(RtlAllocateHeap),
	PATCH(RtlFreeHeap),
	PATCH(RtlReAllocateHeap),
	PATCH(RtlDebugAllocateHeap),
	PATCH(RtlDebugFreeHeap),
	PATCH(RtlDebugReAllocateHeap),
};

patchentry MallocHooks::ole32::patches[] = {
	PATCH(CoGetMalloc),
	PATCH(CoTaskMemAlloc),
	PATCH(CoTaskMemRealloc),
};

patchentry MallocHooks::uxtheme::patches[] = {
	PATCH(OpenThemeData),
	PATCH(OpenThemeDataEx),
	PATCH(CloseThemeData),
	PATCH(GetThemeBackgroundExtent),
	PATCH(GetThemeSysBool),
};

#undef PATCH
#define PATCH(x)	{#x, ##x}

template<int N> patchentry MallocHooks::hooked_msvcr<N>::patches[] = {
	PATCH(_free_dbg),
	PATCH(_calloc_dbg),
	PATCH(_malloc_dbg),
	PATCH(_realloc_dbg),
	PATCH(_recalloc_dbg),
#ifdef _M_X64
	"??2@YAPEAX_KHPEBDH@Z",		scalar_new_dbg,	// void * __ptr64 __cdecl operator new(unsigned __int64,int,char const * __ptr64,int)
	"??_U@YAPEAX_KHPEBDH@Z",	vector_new_dbg,	// void * __ptr64 __cdecl operator new[](unsigned __int64,int,char const * __ptr64,int)
#else
	"??2@YAPAXIHPBDH@Z",		scalar_new_dbg,	// void * __cdecl operator new(unsigned int,int,char const *,int)
	"??_U@YAPAXIHPBDH@Z",		vector_new_dbg,	// void * __cdecl operator new[](unsigned int,int,char const *,int)
#endif

	PATCH(free),
	PATCH(calloc),
	PATCH(malloc),
	PATCH(realloc),
	PATCH(_recalloc),
#ifdef _M_X64
	"??2@YAPEAX_K@Z",			scalar_new,		// void * __ptr64 __cdecl operator new(unsigned __int64)
	"??_U@YAPEAX_K@Z",			vector_new,		// void * __ptr64 __cdecl operator new[](unsigned __int64)
	"??3@YAXPEAX@Z",			scalar_delete,
	"??_V@YAXPEAX@Z",			vector_delete,
#else
	"??2@YAPAXI@Z",				scalar_new,		// void * __cdecl operator new(unsigned int)
	"??_U@YAPAXI@Z",			vector_new,		// void * __cdecl operator new[](unsigned int)
	"??3@YAXPAX@Z",				scalar_delete,
	"??_V@YAXPAX@Z",			vector_delete,
#endif

	PATCH(_aligned_free_dbg),
	PATCH(_aligned_malloc_dbg),
	PATCH(_aligned_offset_malloc_dbg),
	PATCH(_aligned_realloc_dbg),
	PATCH(_aligned_offset_realloc_dbg),
	PATCH(_aligned_recalloc_dbg),
	PATCH(_aligned_offset_recalloc_dbg),

	PATCH(_aligned_free),
	PATCH(_aligned_malloc),
	PATCH(_aligned_offset_malloc),
	PATCH(_aligned_realloc),
	PATCH(_aligned_offset_realloc),
	PATCH(_aligned_recalloc),
	PATCH(_aligned_offset_recalloc),
};

template<int S> struct read_gs;
template<> struct read_gs<1> { static uint8		f(uint32 offset) { return __readgsbyte(offset); } };
template<> struct read_gs<2> { static uint16	f(uint32 offset) { return __readgsword(offset); } };
template<> struct read_gs<4> { static uint32	f(uint32 offset) { return __readgsdword(offset); } };
template<> struct read_gs<8> { static uint64	f(uint32 offset) { return __readgsqword(offset); } };

template<typename T> struct GS {
	template<typename F> static F	get(F T::*f) {
		F	&o	= ((T*)0)->*f;
		return (F)read_gs<sizeof(F)>::f(uint32(uintptr_t(&o)));
	}
};

NT::_TEB *getTEB() {
	return (NT::_TEB*)GS<NT::_NT_TIB>::get(&NT::_NT_TIB::Self);
}

NT::_PEB *getPEB() {
	return GS<NT::_TEB>::get(&NT::_TEB::ProcessEnvironmentBlock);
}

void xor(char *r, const char *a, const char *b, size_t size) {
	while (size--)
		*r++ = *a++ ^ *b++;
}
template<typename T> T xor(const T &a, const T &b) {
	T	r;
	xor((char*)&r, (const char*)&a, (const char*)&b, sizeof(T));
	return r;
}

MallocHooks::MallocHooks(uint32 _flags) : flags(_flags), imalloc(0), event(1), dump_event(0), stack_dumper(GetCurrentProcess(), SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES, BuildSymbolSearchPath()) {
	me = this;

	NT::_TEB *teb = getTEB();
	NT::_PEB *peb = getPEB();

	void	*heap = peb->ProcessHeap;
	void	**heaps = peb->ProcessHeaps;
	int		n		= peb->NumberOfHeaps;
	for (int i = 0; i < n; i++) {
		NT::_HEAP	*h = (NT::_HEAP*)heaps[i];
		ISO_OUTPUTF("\nHeap %i:", i) << h << "\n";
		NT::LIST<NT::_HEAP_SEGMENT, &NT::_HEAP_SEGMENT::SegmentListEntry>	list(h->Segment);
		for (auto i = list.begin(), e = list.end(); i != e; ++i) {
			NT::_HEAP_SEGMENT	*s = i;
			ISO_OUTPUTF("Segment: ") << s << "\n";
			NT::_HEAP_ENTRY		*entry = &s->Entry;
#if 0
			for (;;) {
				NT::_HEAP_ENTRY		decoded_entry = xor(*entry, h->Encoding);
				ISO_OUTPUTF("Entry: ") << entry << "\n";
				entry += decoded_entry.Size;
			}
#endif
		}
		//CheckHeap((HANDLE)h);
	}
	HANDLE h = GetProcessHeap();

	if (flags.test(ENABLED)) {
		flags.set(INITIALISED);
		 heap_val::heap	= HeapCreate(0, 0, 0);

		init(_kernel32, "kernel32.dll");
		init(_ntdll, "ntdll.dll");
		init(_ole32, "ole32.dll");
		init(_uxtheme, "uxtheme.dll");

		ModuleSnapshot	snapshot(GetCurrentProcessId(), TH32CS_SNAPMODULE);
		int	n = 0;
		for (auto &i : snapshot.modules()) {
			if (matches(to_lower(i.szExePath), "*\\msvcr*.dll") || matches(to_lower(i.szExePath), "*\\ucrtbase*.dll")) {
				msvc_dlls[n] = i.szModule;
				init(msvc_patches[n], sizeof(msvcr) / sizeof(void*), (void**)&_msvcr[n], msvc_dlls[n]);
				n++;
			}
		}
	}
}

MallocHooks::~MallocHooks() {
	tls->use();
	if (flags.test(INITIALISED)) {
		remove(_kernel32, "kernel32.dll");
		remove(_ntdll, "ntdll.dll");
		remove(_ole32, "ole32.dll");
		remove(_uxtheme, "uxtheme.dll");

		for (int i = 0; i < num_elements(msvc_dlls) && msvc_dlls[i]; i++)
			remove(msvc_patches[i], sizeof(msvcr) / sizeof(void*), (void**)&_msvcr[i], msvc_dlls[i]);

		ApplyHooks();
		CheckHeaps();
		HeapDestroy(heap_val::heap);
	}
	me = 0;
}

size_t MallocHooks::GetHeapUsed(HANDLE heap) {
	HeapLock(heap);

	PROCESS_HEAP_ENTRY heap_entry;
	clear(heap_entry);

	size_t	used		= 0;
	size_t	committed	= 0;
	size_t	uncommitted	= 0;
	while (HeapWalk(heap, &heap_entry) != FALSE) {
		if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
			// Allocated block
			used += heap_entry.cbData;
		} else if ((heap_entry.wFlags & PROCESS_HEAP_REGION) != 0) {
			committed += heap_entry.Region.dwCommittedSize;
		} else if ((heap_entry.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) != 0) {
			uncommitted += heap_entry.cbData;
		}
	}

	HeapUnlock(heap);
	return used;
}

void MallocHooks::CheckHeap(HANDLE heap) {
	HeapLock(heap);

	PROCESS_HEAP_ENTRY heap_entry;
	clear(heap_entry);

	while (HeapWalk(heap, &heap_entry) != FALSE) {
		if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
			// Allocated block
			void	*p	= heap_entry.lpData;
			auto	i	= lower_boundc(allocs, p);
			entry	*e	= 0;

			if (i != allocs.end()) {
				if (i.key() == p) {
					e = i->last;
				} else if (i != allocs.begin()) {
					--i;
					if ((uint8*)p - (uint8*)i.key() < 16)
						e = i->last;
				}
			}

			if (e) {
				ISO_OUTPUTF("%i: Not freed: " ADDRESSFORMAT ":0x%0x\n", e->event, uintptr_t(i.key()), e->size);
				dump_callstack(e);
			} else {
				ISO_OUTPUTF("Not freed: " ADDRESSFORMAT ":0x%0x\n", uintptr_t(p), heap_entry.cbData);
			}
		} else if ((heap_entry.wFlags & PROCESS_HEAP_REGION) != 0) {
			ISO_OUTPUTF("Range: start: " ADDRESSFORMAT " size: " ADDRESSFORMAT "\n", heap_entry.Region.lpFirstBlock, heap_entry.Region.dwCommittedSize);
		} else if ((heap_entry.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) != 0) {
			//Uncommitted range
		} else {
			//Block
		}
	}

	HeapUnlock(heap);
}

void MallocHooks::CheckHeaps() {
	tls_use	u;

	ISO_OUTPUTF("Bookkeeping: " ADDRESSFORMAT "\n", GetHeapUsed(heap_val::heap));

	uint32	nheaps	= GetProcessHeaps(0, NULL);
	HANDLE	*heaps	= alloc_auto(HANDLE, nheaps);

	uint32	nheaps2 = GetProcessHeaps(nheaps, heaps);
	for (uint32 i = 0; i < nheaps; i++) {
		if (heaps[i] != heap_val::heap) {
			ISO_OUTPUTF("---- HEAP " ADDRESSFORMAT " ----\n", heaps[i]);
			CheckHeap(heaps[i]);
		}
	}
}

void MallocHooks::get_callstack(entry *e, context &ctx) {
	e->stack.init(cs, ctx);
}

void MallocHooks::dump_callstack(const entry *e) {
	stack_dumper.Dump(cs, e->stack);
}

void MallocHooks::dump_callstacks(const entry *e) {
	if (e->next) {
		ISO_OUTPUT("previous callstack:\n");
		dump_callstack(e->next);
	}
	ISO_OUTPUT("current callstack:\n");
	dump_callstack(e);
}

uint32 MallocHooks::next_event(context &ctx) {
	uint32	i = event++;
	if (i == dump_event) {
		ISO_OUTPUTF("%i: dump event callstack:\n", i);
		void	*callstack[32];
		stack_dumper.Dump(callstack, cs.GetStackTrace(ctx, callstack, 32));
	}
	return i;
}

void *MallocHooks::add_alloc(void *p, size_t size, context &ctx) {
	uint32	event = next_event(ctx);
	uint32	allocd;

	entry	*e	= 0;
	allocsLock.lock();
		entry_list	&r	= allocs[p];
		allocd		= r.last && !r.last->is_free() ? r.last->event : 0;
		r.last		= e = new entry(r.last, event, size);
	allocsLock.unlock();
	get_callstack(e, ctx);

	if (flags.test(OUTPUT_EVENTS) || (allocd && flags.test(OUTPUT_ERRORS)))
		ISO_OUTPUTF("(%i) %i: alloc: 0x%0I64x:0x%I64x", GetCurrentThreadId(), event, p, size)
		<< onlyif(allocd, format_string(" - already allocated:  %i", allocd))
		<< '\n';

	if (allocd && flags.test(DUMP_ALLOC_ALLOCD))
		dump_callstacks(e);

	return p;
}

void MallocHooks::add_free(void *p, context &ctx) {
	uint32	event = next_event(ctx);
	uint32	freed;
	entry	*e	= 0;

	allocsLock.lock();
		auto	i	= lower_boundc(allocs, p);
		if (i != allocs.end() && i.key() == p) {
			freed	= i->last && i->last->is_free() ? i->last->event : 0;
			i->last	= e = new entry(i->last, event, 0);
		} else {
			freed	= ~0;
			allocs[p].last = e	= new entry(0, event, 0);
		}
	allocsLock.unlock();
	get_callstack(e, ctx);

	if (flags.test(OUTPUT_EVENTS) || (freed && flags.test(OUTPUT_ERRORS)))
		ISO_OUTPUTF("(%i) %i: free: 0x%0I64x", GetCurrentThreadId(), event, p)
		<< onlyif(freed && ~freed, format_string(" - already freed:  %i", freed))
		<< onlyif(!~freed, " - unallocated")
		<< '\n';

	if (freed && flags.test(!~freed ? DUMP_FREE_UNKNOWN : DUMP_FREE_FREED))
		dump_callstacks(e);
}

void *MallocHooks::add_realloc(void *newp, void *oldp, size_t size, context &ctx) {
	uint32	event = next_event(ctx);
	uint32	freed = 0, allocd = 0;
	entry	*e1	= 0;
	entry	*e2	= 0;
	
	allocsLock.lock();
		if (oldp) {
			auto	i = lower_boundc(allocs, oldp);
			if (i != allocs.end() && i.key() == oldp) {
				freed	= i->last && i->last->is_free() ? i->last->event : 0;
				i->last	= e1 = new entry(i->last, event, 0);
			} else {
				freed	= ~0;
				allocs[oldp].last	= e1 = new entry(0, event, 0);
			}
		}
		if (newp) {
			entry_list	&r	= allocs[newp];
			allocd		= r.last && !r.last->is_free() ? r.last->event : 0;
			r.last		= e2 = new entry(r.last, event, size);
		}
	allocsLock.unlock();
	if (oldp)
		get_callstack(e1, ctx);
	if (newp)
		get_callstack(e2, ctx);

	if (flags.test(OUTPUT_EVENTS) || ((freed || allocd) && flags.test(OUTPUT_ERRORS)))
		ISO_OUTPUTF("(%i) %i: realloc: 0x%0I64x -> 0x%0I64x", GetCurrentThreadId(), event, oldp, newp)
		<< onlyif(freed && ~freed, format_string(" - already freed:  %i", freed))
		<< onlyif(!~freed, " - unallocated")
		<< onlyif(allocd, format_string(" - already allocated:  %i", allocd))
		<< '\n';
	
	if (freed && flags.test(!~freed ? DUMP_FREE_UNKNOWN : DUMP_FREE_FREED))
		dump_callstacks(e1);

	if (allocd && flags.test(DUMP_ALLOC_ALLOCD))
		dump_callstacks(e2);

	return newp;
}

//kernel32

HANDLE		MallocHooks::hooked_HeapCreate(DWORD options, size_t initsize, size_t maxsize) {
	return me->_kernel32.HeapCreate(options, initsize, maxsize);
}
BOOL		MallocHooks::hooked_HeapDestroy(HANDLE heap) {
	return me->_kernel32.HeapDestroy(heap);
}	
void*		MallocHooks::hooked_HeapAlloc(HANDLE heap, DWORD flags, size_t size) {
	tls_use	u;
	void	*p = me->_kernel32.HeapAlloc(heap, flags, size);
	return u ? me->add_alloc(p, size, context(me->_kernel32.HeapAlloc)) : p;
}	
BOOL		MallocHooks::hooked_HeapFree(HANDLE heap, DWORD flags, void *mem) {
	tls_use	u;
	BOOL	r = me->_kernel32.HeapFree(heap, flags, mem);
	if (r && mem && u)
		me->add_free(mem, context(me->_kernel32.HeapFree));
	return r;
}
void*		MallocHooks::hooked_HeapReAlloc(HANDLE heap, DWORD flags, void *mem, size_t size) {
	tls_use	u;
	void	*p = me->_kernel32.HeapReAlloc(heap, flags, mem, size);
	return u ? me->add_realloc(p, mem, size, context(me->_kernel32.HeapReAlloc)) : p;
}
void	MallocHooks::hooked_ExitProcess(UINT code) {
	me->CheckHeaps();
	me->_kernel32.ExitProcess(code);
}

//ntdll

void*		MallocHooks::hooked_RtlAllocateHeap(HANDLE heap, DWORD flags, size_t size) {
	tls_use	u;
	void	*p = me->_ntdll.RtlAllocateHeap(heap, flags, size);
	return u ? me->add_alloc(p, size, context(me->_ntdll.RtlAllocateHeap)) : p;
}
BYTE		MallocHooks::hooked_RtlFreeHeap(HANDLE heap, DWORD flags, void *mem) {
	tls_use	u;
	BYTE	r = me->_ntdll.RtlFreeHeap(heap, flags, mem);
	if (r && mem && u)
		me->add_free(mem, context(me->_ntdll.RtlFreeHeap));
	return r;
}	
void*		MallocHooks::hooked_RtlReAllocateHeap(HANDLE heap, DWORD flags, void *mem, size_t size) {
	tls_use	u;
	void	*p = me->_ntdll.RtlReAllocateHeap(heap, flags, mem, size);
	return u ? me->add_realloc(p, mem, size, context(me->_ntdll.RtlReAllocateHeap)) : p;
}
void*		MallocHooks::hooked_RtlDebugAllocateHeap(HANDLE heap, DWORD flags, size_t size) {
	tls_use	u;
	void	*p = me->_ntdll.RtlDebugAllocateHeap(heap, flags, size);
	return u ? me->add_alloc(p, size, context(me->_ntdll.RtlDebugAllocateHeap)) : p;
}
BYTE		MallocHooks::hooked_RtlDebugFreeHeap(HANDLE heap, DWORD flags, void *mem) {
	tls_use	u;
	BYTE	r = me->_ntdll.RtlDebugFreeHeap(heap, flags, mem);
	if (r && mem && u)
		me->add_free(mem, context(me->_ntdll.RtlDebugFreeHeap));
	return r;
}	
void*		MallocHooks::hooked_RtlDebugReAllocateHeap(HANDLE heap, DWORD flags, void *mem, size_t size) {
	tls_use	u;
	void	*p = me->_ntdll.RtlDebugReAllocateHeap(heap, flags, mem, size);
	return u ? me->add_realloc(p, mem, size, context(me->_ntdll.RtlDebugReAllocateHeap)) : p;
}
//ole32

HRESULT		MallocHooks::hooked_CoGetMalloc(DWORD context, IMalloc **imalloc) {
	HRESULT hr	= me->_ole32.CoGetMalloc(context, &me->imalloc);
	*imalloc = SUCCEEDED(hr) ? me : 0;
	return hr;

}	
void*		MallocHooks::hooked_CoTaskMemAlloc(size_t size) {
	tls_use	u;
	void	*p = me->_ole32.CoTaskMemAlloc(size);
	return u ? me->add_alloc(p, size, context(me->_ole32.CoTaskMemAlloc)) : p;
}	
void*		MallocHooks::hooked_CoTaskMemRealloc(void *mem, size_t size) {
	tls_use	u;
	void	*p = me->_ole32.CoTaskMemRealloc(mem, size);
	return u ? me->add_realloc(p, mem, size, context(me->_ole32.CoTaskMemRealloc)) : p;
}	

//msvcr

// dbg
template<int N> void MallocHooks::hooked_msvcr<N>::_free_dbg(void *mem, const char *file, int line) {
	tls_use	u;
	me->_msvcr[N]._free_dbg(mem, file, line);
	if (mem && u)
		me->add_free(mem, context(me->_msvcr[N]._free_dbg));
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_calloc_dbg(size_t num, size_t size, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._calloc_dbg(num, size, type, file, line);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._calloc_dbg)) : p;
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_malloc_dbg(size_t size, int type, const char *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._malloc_dbg(size, type, file, line);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._malloc_dbg)) : p;
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_realloc_dbg(void *mem, size_t size, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._realloc_dbg(mem, size, type, file, line);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._realloc_dbg)) : p;
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_recalloc_dbg(void *mem, size_t num, size_t size, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._recalloc_dbg(mem, num, size, type, file, line);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._recalloc_dbg)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::scalar_new_dbg(size_t size, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._scalar_new_dbg(size, type, file, line);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._scalar_new_dbg)) : p;
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::vector_new_dbg(size_t size, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._vector_new_dbg(size, type, file, line);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._vector_new_dbg)) : p;
}

// not dbg
template<int N> void	MallocHooks::hooked_msvcr<N>::free(void *mem) {
	tls_use	u;
	me->_msvcr[N].free(mem);
	if (mem && u)
		me->add_free(mem, context(me->_msvcr[N].free));
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::calloc(size_t num, size_t size) {
	tls_use	u;
	void	*p = me->_msvcr[N].calloc(num, size);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N].calloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::malloc(size_t size) {
	tls_use	u;
	void	*p = me->_msvcr[N].malloc(size);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N].malloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::realloc(void *mem, size_t size) {
	tls_use	u;
	void	*p = me->_msvcr[N].realloc(mem, size);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N].realloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_recalloc(void *mem, size_t num, size_t size) {
	tls_use	u;
	void	*p = me->_msvcr[N]._recalloc(mem, num, size);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._recalloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::scalar_new(size_t size) {
	tls_use	u;
	void	*p = me->_msvcr[N].scalar_new(size);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N].scalar_new)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::vector_new(size_t size) {
	tls_use	u;
	void	*p = me->_msvcr[N].vector_new(size);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N].vector_new)) : p;
}	
template<int N> void	MallocHooks::hooked_msvcr<N>::scalar_delete(void *mem) {
	tls_use	u;
	me->_msvcr[N].scalar_delete(mem);
	if (mem && u)
		me->add_free(mem, context(me->_msvcr[N].scalar_delete));
}	
template<int N> void	MallocHooks::hooked_msvcr<N>::vector_delete(void *mem) {
	tls_use	u;
	me->_msvcr[N].vector_delete(mem);
	if (mem && u)
		me->add_free(mem, context(me->_msvcr[N].vector_delete));
}	

// aligned_dbg
template<int N> void	MallocHooks::hooked_msvcr<N>::_aligned_free_dbg(void *mem, const char *file, int line) {
	tls_use	u;
	me->_msvcr[N]._aligned_free_dbg(mem, file, line);
	if (mem && u)
		me->add_free(mem, context(me->_msvcr[N]._aligned_free_dbg));
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_malloc_dbg(size_t size, size_t alignment, int type, const char *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_malloc_dbg(size, alignment, type, file, line);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._aligned_malloc_dbg)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_offset_malloc_dbg(size_t size, size_t alignment, size_t offset, int type, const char *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_offset_malloc_dbg(size, alignment, offset, type, file, line);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._aligned_offset_malloc_dbg)) : p;
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_realloc_dbg(void *mem, size_t size, size_t alignment, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_realloc_dbg(mem, size, alignment, type, file, line);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_realloc_dbg)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_offset_realloc_dbg(void *mem, size_t size, size_t alignment, size_t offset, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_offset_realloc_dbg(mem, size, alignment, offset, type, file, line);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_offset_realloc_dbg)) : p;
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_recalloc_dbg(void *mem, size_t num, size_t size, size_t alignment, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_recalloc_dbg(mem, num, size, alignment, type, file, line);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_recalloc_dbg)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_offset_recalloc_dbg(void *mem, size_t num, size_t size, size_t alignment, size_t offset, int type, char const *file, int line) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_offset_recalloc_dbg(mem, num, size, alignment, offset, type, file, line);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_offset_recalloc_dbg)) : p;
}	

// aligned
template<int N> void	MallocHooks::hooked_msvcr<N>::_aligned_free(void *mem) {
	tls_use	u;
	me->_msvcr[N]._aligned_free(mem);
	if (mem && u)
		me->add_free(mem, context(me->_msvcr[N]._aligned_free));
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_malloc(size_t size, size_t alignment) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_malloc(size, alignment);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._aligned_malloc)) : p;
}
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_offset_malloc(size_t size, size_t alignment, size_t offset) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_offset_malloc(size, alignment, offset);
	return p && u ? me->add_alloc(p, size, context(me->_msvcr[N]._aligned_offset_malloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_realloc(void *mem, size_t size, size_t alignment) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_realloc(mem, size, alignment);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_realloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_offset_realloc(void *mem, size_t size, size_t alignment, size_t offset) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_offset_realloc(mem, size, alignment, offset);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_offset_realloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_recalloc(void *mem, size_t num, size_t size, size_t alignment) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_recalloc(mem, num, size, alignment);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_recalloc)) : p;
}	
template<int N> void*	MallocHooks::hooked_msvcr<N>::_aligned_offset_recalloc(void *mem, size_t num, size_t size, size_t alignment, size_t offset) {
	tls_use	u;
	void	*p = me->_msvcr[N]._aligned_offset_recalloc(mem, num, size, alignment, offset);
	return u ? me->add_realloc(p, mem, size, context(me->_msvcr[N]._aligned_offset_recalloc)) : p;
}

namespace iso {
bool MallocTracking(bool enable) {
	return MallocHooks::me && MallocHooks::me->flags.test_set(MallocHooks::ENABLED, enable);
}
}
