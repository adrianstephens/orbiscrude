#ifndef HOOK_H
#define HOOK_H

#undef WINAPI_PARTITION_DESKTOP
#define WINAPI_PARTITION_DESKTOP 1

#include "base/defs.h"
#include "base/strings.h"
#include "base/array.h"
#include "base/list.h"
#include "base/pointer.h"
#include "thread.h"
#include "filename.h"
#include "windows\nt.h"
#include <TlHelp32.h>
#include <psapi.h>

namespace iso {
class filename;

bool MiniDump(EXCEPTION_POINTERS *ep, const char *fn);

#ifdef UNICODE
#undef MODULEENTRY32
#undef Module32First
#undef Module32Next
#endif

//-----------------------------------------------------------------------------
//	Module, RawModule
//	memory-mapped image of a DLL
//-----------------------------------------------------------------------------

struct DEBUG_DIRECTORY_CODEVIEW {
	uint32			Format;	//'RSDS'
	GUID			Guid;
	uint32			Age;
	embedded_string	fn;
};

struct ModuleBase {
	HMODULE	h;

	//-------------------------------------------------------------------------

	struct ResourceRoot;
	struct ResourceDir : _IMAGE_RESOURCE_DIRECTORY {
		struct Entry : IMAGE_RESOURCE_DIRECTORY_ENTRY {
			uint16				Type()								const { return NameIsString ? 0 : Id; }
			bool				IsDir()								const { return DataIsDirectory; }
			icount_string16		Name(const ResourceRoot *base)		const { if (!NameIsString) return icount_string16(); uint16 *p = (uint16*)((uint8*)base + NameOffset); return icount_string16((ichar16*)(p + 1), p[0]); }
			const ResourceDir*	SubDir(const ResourceRoot *base)	const { return DataIsDirectory ? (const ResourceDir*)((char*)base + OffsetToDirectory) : 0; }
			range<const Entry*>	SubEntries(const ResourceRoot *base)const { return DataIsDirectory ? ((const ResourceDir*)((char*)base + OffsetToDirectory))->entries() : none; }
			const_memory_block	Data(const ModuleBase &mod, const ResourceRoot *base) const {
				if (DataIsDirectory)
					return empty;
				const _IMAGE_RESOURCE_DATA_ENTRY	*data = (const _IMAGE_RESOURCE_DATA_ENTRY*)((char*)base + OffsetToData);
				return const_memory_block((char*)mod.h + data->OffsetToData, data->Size);
			}
//			const_memory_block	Data(const ModuleBase &mod)		const { return Data(mod, mod.Resources()); }
		};
		range<const Entry*> entries() const {
			return make_range_n((const Entry*)(this + 1), NumberOfIdEntries + NumberOfNamedEntries);
		}
	};
	struct ResourceRoot : ResourceDir {};

	//-------------------------------------------------------------------------

	struct ExportsDir {
		const IMAGE_EXPORT_DIRECTORY	*dir;
		const char *Find(const ModuleBase &mod, int ordinal) const {
			const uint32	*names		= mod.offset(dir->AddressOfNames);
			const uint16	*ordinals	= mod.offset(dir->AddressOfNameOrdinals);
			for (int i = 0; i < dir->NumberOfNames; i++) {
				if (ordinal == ordinals[i] + dir->Base)
					return mod.offset(names[i]);
			}
			return 0;
		}
		ExportsDir(const const_memory_block &dir) : dir(dir) {}
		explicit operator bool() const { return !!dir; }
	};

	//-------------------------------------------------------------------------

	struct ImportsDir {
		struct Thunk : IMAGE_THUNK_DATA {
			bool	IsOrdinal()						const	{ return rotate_left(u1.AddressOfData, 1) & 1; }
			uint16	Ordinal()						const	{ return u1.AddressOfData & 0xffff; }
			const char *Name(const ModuleBase &mod)	const	{ return ((const IMAGE_IMPORT_BY_NAME*)mod.offset(u1.AddressOfData))->Name; }
		};
		struct Entry : _IMAGE_IMPORT_DESCRIPTOR {
			struct iterator : iterator_wrapper<iterator, const Thunk*> {//}; : iso::iterator<const Thunk*, forward_iterator_t> {
				constexpr iterator(const Thunk *i)	: iterator_wrapper<iterator, const Thunk*>(i)	{}
				constexpr bool	operator!=(const iterator&)	const	{ return i->u1.AddressOfData != 0; }
			};
			auto entries(const ModuleBase &mod) const { return range<iterator>((const Thunk*)mod.offset(OriginalFirstThunk), (const Thunk*)0); }

			const Thunk *Find(const ModuleBase &mod, const ExportsDir &exp, const char *name) const {
				const char	*dll = mod.offset(Name);
				for (auto &p : entries(mod)) {
					const char *entry_name = 0;
					if (p.IsOrdinal()) {
						if (exp)
							entry_name = exp.Find(mod, p.Ordinal());
					} else {
						entry_name = p.Name(mod);
					}
					if (entry_name && strcmp(entry_name, name) == 0)
						return &p;
				}
				return nullptr;
			}

		};
		const Entry		*dir;

		struct iterator : iterator_wrapper<iterator, const Entry*> {
			constexpr iterator(const Entry *i)	: iterator_wrapper<iterator, const Entry*>(i)	{}
			constexpr bool	operator!=(const iterator&)	const	{ return i->Characteristics != 0; }
		};

		const Thunk *Find(const ModuleBase &mod, const ExportsDir &exp, const char *name) const {
			for (auto &i : *this) {
				if (auto thunk = i.Find(mod, exp, name))
					return thunk;
			}
			return nullptr;
		}
		iterator begin()	const { return dir; }
		iterator end()		const { return nullptr; }

		ImportsDir(const const_memory_block &dir) : dir(dir) {}
		explicit operator bool() const { return !!dir; }
	};

	//-------------------------------------------------------------------------

	struct DebugDir {
		struct Entry : _IMAGE_DEBUG_DIRECTORY {
			auto	data(const ModuleBase *mod, bool mapped)	const { return const_memory_block(mod->DataByRaw(PointerToRawData, mapped), SizeOfData); }
		};
		const Entry	*dir, *dir_end;

		const Entry *begin()	const { return dir; }
		const Entry *end()		const { return dir_end; }

		DebugDir(const const_memory_block &dir) : dir(dir), dir_end(dir.end()) {}
		explicit operator bool() const { return !!dir; }
	};

	//-------------------------------------------------------------------------

	static const IMAGE_SECTION_HEADER*	SectionByName(const range<const IMAGE_SECTION_HEADER*> &sections, const char *n) {
		for (auto &i : sections) {
			if (memcmp(i.Name, n, sizeof(i.Name)) == 0)
				return &i;
		}
		return 0;
	}
	static const IMAGE_SECTION_HEADER*	SectionByRVA(const range<const IMAGE_SECTION_HEADER*> &sections, uint32 rva) {
		for (auto &i : sections) {
			if (rva >= i.VirtualAddress && rva < (&i == &sections.back() ? i.VirtualAddress + i.SizeOfRawData : (&i)[1].VirtualAddress))
				return &i;
		}
		return 0;
	}
	static const IMAGE_SECTION_HEADER*	SectionByRaw(const range<const IMAGE_SECTION_HEADER*> &sections, uint32 addr) {
		for (auto &i : sections) {
			if (addr >= i.PointerToRawData && addr < i.PointerToRawData + i.SizeOfRawData)
				return &i;
		}
		return 0;
	}
	static uint32	RVA2Raw(const range<const IMAGE_SECTION_HEADER*> &sections, uint32 rva) {
		if (auto sect = SectionByRVA(sections, rva))
			return rva - sect->VirtualAddress + sect->PointerToRawData;
		return 0;
	}

	//-------------------------------------------------------------------------

	ModuleBase(HMODULE h = 0)			: h(h) {}
	ModuleBase(const MODULEENTRY32 &m)	: h(m.hModule) {}

	bool						valid()			const	{ return h && ((IMAGE_DOS_HEADER*)h)->e_magic == 0x5a4d; }
	explicit operator bool()					const	{ return valid(); }
	arbitrary_const_ptr			offset(DWORD x)	const	{ return x ? (char*)h + x : 0; }

	const IMAGE_FILE_HEADER*	FileHeader()	const	{ return offset(((IMAGE_DOS_HEADER*)h)->e_lfanew + 4); }
	const IMAGE_OPTIONAL_HEADER* OptHeader()	const	{ return (IMAGE_OPTIONAL_HEADER*)(FileHeader() + 1); }
	auto						DataDir(int i)	const	{ return OptHeader()->DataDirectory[i]; }
	auto						Sections()		const	{ auto *fh = FileHeader(); return make_range_n((const IMAGE_SECTION_HEADER*)((uint8*)(fh + 1) + fh->SizeOfOptionalHeader), fh->NumberOfSections); }
	const IMAGE_SECTION_HEADER&	Section(int i)	const	{ return Sections()[i]; }

	uint32						RVA2Raw(uint32 rva)										const { return RVA2Raw(Sections(), rva); }
	uint32						FixRVA(uint32 rva, bool mapped)							const { return mapped ? rva : RVA2Raw(rva); }
	uint32						FixRVA(uint32 rva, uint32 raw, bool mapped)				const { return mapped ? rva : raw; }
	const_memory_block			GetMem(const IMAGE_DATA_DIRECTORY &dir, bool mapped)	const { return const_memory_block(offset(FixRVA(dir.VirtualAddress, mapped)), dir.Size); }
	const_memory_block			GetMem(const IMAGE_SECTION_HEADER &sect, bool mapped)	const { return const_memory_block(offset(FixRVA(sect.VirtualAddress, sect.PointerToRawData, mapped)), sect.SizeOfRawData); }

	const IMAGE_SECTION_HEADER*	SectionByName(const char *n)	const	{ return SectionByName(Sections(), n); }
	const IMAGE_SECTION_HEADER*	SectionByRVA(uint32 rva)		const	{ return SectionByRVA(Sections(), rva); }
	const IMAGE_SECTION_HEADER*	SectionByRaw(uint32 addr)		const	{ return SectionByRaw(Sections(), addr); }

	arbitrary_const_ptr			DataByRVA(uint32 rva, bool mapped) const {
		if (auto *sect = SectionByRVA(rva))
			return GetMem(*sect, mapped) + (rva - sect->VirtualAddress);
		return nullptr;
	}
	arbitrary_const_ptr			DataByRaw(uint32 addr, bool mapped) const {
		if (auto *sect = SectionByRaw(addr))
			return GetMem(*sect, mapped) + (addr - sect->PointerToRawData);
		return nullptr;
	}

	uint64	calc_end() {
		uint64	end	= 0;
		for (auto &s : Sections())
			end = max(end, s.VirtualAddress + s.SizeOfRawData);
		return end;
	}

};

template<bool MAPPED> struct ModuleT : ModuleBase {
	const_memory_block		GetMem(const IMAGE_DATA_DIRECTORY &dir)		const { return ModuleBase::GetMem(dir, MAPPED); }
	const_memory_block		GetMem(const IMAGE_SECTION_HEADER &sect)	const { return ModuleBase::GetMem(sect, MAPPED); }

	uint32		FixRVA(uint32 rva)				const	{ return FixRVA(rva, MAPPED); }
	uint32		FixRVA(uint32 rva, uint32 raw)	const	{ return FixRVA(rva, raw, MAPPED); }

	arbitrary_const_ptr			DataByRaw(uint32 addr) const { return ModuleBase::DataByRaw(addr, MAPPED); }

	//DataDirs
	const ResourceRoot*			Resources()		const	{ return GetMem(DataDir(IMAGE_DIRECTORY_ENTRY_RESOURCE)); }
	ImportsDir					Imports()		const	{ return GetMem(DataDir(IMAGE_DIRECTORY_ENTRY_IMPORT)); }
	ExportsDir					Exports()		const	{ return GetMem(DataDir(IMAGE_DIRECTORY_ENTRY_EXPORT)); }
	DebugDir					Debug()			const	{ return GetMem(DataDir(IMAGE_DIRECTORY_ENTRY_DEBUG)); }

	const char *OrdinalToName(int ordinal) const {
		if (auto exp = Exports())
			return exp.Find(*this, ordinal);
		return 0;
	}

	ModuleT(HMODULE h = 0)			: ModuleBase(h) {}
	ModuleT(const MODULEENTRY32 &m)	: ModuleBase(m.hModule) {}

	static ModuleT	current() { return (HINSTANCE)&__ImageBase; }
};

typedef ModuleT<true>	Module;
typedef ModuleT<false>	RawModule;

//-----------------------------------------------------------------------------
//	ModuleSnapshot
//-----------------------------------------------------------------------------

struct ModuleSnapshot {
	HANDLE			h;

	template<typename E, BOOL WINAPI FIRST(HANDLE, E*), BOOL WINAPI NEXT(HANDLE, E*)> struct containerT {
		HANDLE		h;
		mutable E	e;

		struct iterator {
			typedef forward_iterator_t iterator_category;
			typedef	E element, &reference;
			HANDLE			h;
			E&				e;
			mutable int		state;
			int				get_state()						const	{ if (state == 0) state = FIRST(h, &e) ? 1 : 2; return state; }
			iterator(HANDLE h, E &e, int state) : h(h), e(e), state(state)	{}
			iterator		&operator++()							{ if (!NEXT(h, &e)) state = 2; return *this; }
			operator		bool()							const	{ return get_state() != 2; }
			bool			operator!=(const iterator &b)	const	{ return get_state() != b.state; }
			E*				operator->()					const	{ get_state(); return &e; }
			E&				operator*()						const	{ get_state(); return e; }
		};
		typedef iterator	const_iterator;

		containerT(HANDLE h) : h(h) { clear(e); e.dwSize = sizeof(E); }
		iterator	begin() const { return {h, e, 0}; }
		iterator	end()	const { return {h, e, 2}; }
	};

	ModuleSnapshot(DWORD id, uint32 flags) {
		for (int i = 0; i < 10 && (h = CreateToolhelp32Snapshot(flags, id)) == INVALID_HANDLE_VALUE; i++) {
			if (GetLastError() != ERROR_BAD_LENGTH)
				break;
		}
	}
	~ModuleSnapshot()	{ CloseHandle(h); }

	typedef containerT<PROCESSENTRY32,	Process32First, Process32Next>	processes_t;
	typedef containerT<MODULEENTRY32,	Module32First,	Module32Next>	modules_t;
	typedef containerT<THREADENTRY32,	Thread32First,	Thread32Next>	threads_t;
	typedef containerT<HEAPLIST32,		Heap32ListFirst,Heap32ListNext>	heaps_t;

	processes_t		processes() const { return h; }
	modules_t		modules()	const { return h; }
	threads_t		threads()	const { return h; }
	heaps_t			heaps()		const { return h; }

	modules_t::iterator	find(const void *p);
};

HMODULE		FindModule(DWORD pid, const char *name, filename &fn);

inline HMODULE FindModule(const char *name, filename &fn) {
	return FindModule(GetCurrentProcessId(), name, fn);
}

#ifndef PLAT_WINRT

template<typename T> struct dll_variable {
	const T	*p;
	dll_variable(module m, uint32 sect, uint32 offset) {
		Module	mod(m);
		p = mod.offset(mod.Section(sect).PointerToRawData + offset);
	}
	operator const T&() const {
		return *p;
	}
};

//-----------------------------------------------------------------------------
//	RemoteCall
//-----------------------------------------------------------------------------

struct RemoteCall {
	HANDLE	hProcess;
	HANDLE	hThread;
	void	*mem;
	RemoteCall() : hProcess(0), hThread(0), mem(0) {}
	RemoteCall(HANDLE hProcess, LPTHREAD_START_ROUTINE func, const void *params, size_t params_len, size_t result_len) : hProcess(hProcess), hThread(0) {
		mem	= VirtualAllocEx(hProcess, NULL, max(params_len, result_len), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		SIZE_T	count;
		if (WriteProcessMemory(hProcess, mem, params, params_len, &count))
			hThread = CreateRemoteThread(hProcess, NULL, 0, func, mem, 0, NULL);
	}
	template<typename T> RemoteCall(HANDLE hProcess, LPTHREAD_START_ROUTINE func, const T &t) : RemoteCall(hProcess, func, &t, sizeof(T), 0) {}

	~RemoteCall() {
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		if (mem)
			VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
	}
	bool			Wait(Timeout timeout = Timeout()) {
		return WaitForSingleObject(hThread, timeout) == 0;
	}
	malloc_block	Result(size_t result_len, Timeout timeout = Timeout()) {
		if (Wait(timeout)) {
			malloc_block	result(result_len);
			SIZE_T			count;
			if (ReadProcessMemory(hProcess, mem, result, result_len, &count))
				return result;
		}
		return empty;
	}
	bool	Result(void *result, size_t result_len, Timeout timeout = Timeout()) {
		if (Wait(timeout)) {
			SIZE_T	count;
			return !!ReadProcessMemory(hProcess, mem, result, result_len, &count);
		}
		return false;
	}
};

inline bool	RunRemote(HANDLE hProcess, LPTHREAD_START_ROUTINE func, const void *params, size_t params_len, void *result = 0, size_t result_len = 0, Timeout timeout = Timeout()) {
	return RemoteCall(hProcess, func, params, params_len, result_len).Result(result, result_len, timeout);
}
template<typename T> bool RunRemote(HANDLE hProcess, LPTHREAD_START_ROUTINE func, const T &t, Timeout timeout = Timeout()) {
	return RunRemote(hProcess, func, &t, sizeof(T), 0, 0, timeout);
}
template<typename T, typename R> bool RunRemote(HANDLE hProcess, LPTHREAD_START_ROUTINE func, const T &t, R &r, Timeout timeout = Timeout()) {
	return RunRemote(hProcess, func, &t, sizeof(T), &r, sizeof(R), timeout);
}

//-----------------------------------------------------------------------------
//	Process
//-----------------------------------------------------------------------------

struct PROCESS_BASIC_INFORMATION {
	LONG		ExitStatus;
	NT::_PEB	*PebBaseAddress;
	ULONG_PTR	AffinityMask;
	LONG		BasePriority;
	ULONG_PTR	UniqueProcessId;
	ULONG_PTR	ParentProcessId;
};

struct Process {
	HANDLE hProcess;

	Process() : hProcess(0) {}
	Process(HANDLE _hProcess) : hProcess(_hProcess) {}

	operator HANDLE()	const			{ return hProcess; }
	uint32	ID()		const			{ return GetProcessId(hProcess); }
	void	Wait()		const			{ WaitForSingleObject(hProcess, INFINITE); }
	bool	Terminate(uint32 code = 0)	{ return !!TerminateProcess(hProcess, code); }
	uint32	ExitCode()	const			{ DWORD code; return GetExitCodeProcess(hProcess, &code) ? code : 0; }
	bool	Active()	const			{ DWORD code; return GetExitCodeProcess(hProcess, &code) && code == STILL_ACTIVE; }

	PROCESS_BASIC_INFORMATION GetBasicInformation() const;
	void	Suspend();
	void	Resume();
	void	TerminateSafe(uint32 code = 0);
	void	InjectDLL(const char *lib);

	HMODULE	FindModule(const char *name, filename &fn) {
		return iso::FindModule(GetProcessId(hProcess), name, fn);
	}

	filename	Filename() const {
		filename	fn;
		GetModuleFileNameExA(hProcess, NULL, fn.begin(), sizeof(fn));
		return fn;
	}

	size_t			ReadMemory(void *dest, const void *srce, size_t size) const {
		SIZE_T	read;
		ReadProcessMemory(hProcess, srce, dest, size, &read);
		return read;
	}
	size_t			WriteMemory(void *dest, const void *srce, size_t size) const {
		SIZE_T	written;
		WriteProcessMemory(hProcess, dest, srce, size, &written);
		return written;
	}
	malloc_block	GetMemory(void *start, size_t size)				const { malloc_block m(size); ReadProcessMemory(hProcess, start, m, size, NULL); return m; }
	malloc_block	GetMemory(const memory_block &m)				const { return GetMemory(m, m.length()); }
	malloc_block	GetMemory(const const_memory_block &m)			const { return GetMemory(unconst((const void*)m.begin()), m.length()); }
	void			SetMemory(void *start, const memory_block &m)	const { WriteMemory(start, m, m.length()); }

	template<typename T> bool	Read(T &t, const void *addr)		const { return ReadMemory(&t, addr, sizeof(T)); }
	template<typename T> T		Read(const T *addr)					const { T t; Read(t, addr); return t; }
	template<typename T> bool	Write(const T &t, void *addr)		const { return WriteMemory(addr, &t, sizeof(T)); }

	string16		ReadString(const NT::_UNICODE_STRING &u) const {
		string16		s;
		if (u.Length) {
			ReadMemory(s.alloc(u.Length / 2), u.Buffer, u.Length);
			s[u.Length / 2] = 0;
		}
		return s;
	}
	string16		ReadString(const NT::_UNICODE_STRING *addr) const {
		return ReadString(Read(addr));
	}
};

struct Process2 : Process {
	HANDLE	hThread;
	DWORD	dwProcessId;
	DWORD	dwThreadId;

	Process2() : hThread(0), dwProcessId(0), dwThreadId(0) {}
	Process2(uint32 id)													{ _Open(id); }
	Process2(HANDLE _hProcess)											{ _Open(_hProcess); }
	Process2(const filename &app, const char *dir, const char *args, uint32 flags = CREATE_SUSPENDED)	{ _Open(app, dir, args, flags); }
	~Process2()															{ _Close(); }
	uint32	ID()	const	{ return dwProcessId; }

	void	_GetMainThread();

	HANDLE	_Open(uint32 id) {
		hProcess	= OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, id);
		dwProcessId = id;
		_GetMainThread();
		return hProcess;
	}
	uint32	_Open(HANDLE _hProcess) {
		DuplicateHandle(GetCurrentProcess(), _hProcess,  GetCurrentProcess(), &hProcess, 0, TRUE, DUPLICATE_SAME_ACCESS);
		hThread		= 0;
		dwThreadId	= 0;
		return dwProcessId = GetProcessId(_hProcess);
	}
	uint32	_Open(const filename &app, const char *dir, const char *args, uint32 flags);

	void	_Close() {
		CloseHandle(hThread);
		CloseHandle(hProcess);
	}
	HANDLE	Open(uint32 id) {
		_Close();
		return _Open(id);
	}
	uint32	Open(HANDLE _hProcess) {
		_Close();
		return _Open(_hProcess);
	}
	uint32	Open(const filename &app, const char *dir, const char *args, uint32 flags = CREATE_SUSPENDED) {
		_Close();
		return _Open(app, dir, args, flags);
	}
	HMODULE	FindModule(const char *name, filename &fn) {
		return iso::FindModule(dwProcessId, name, fn);
	}
	uint32 Run() {
		if (dwProcessId)
			ResumeThread(hThread);
		return dwProcessId;
	}

	bool	IsSuspended() const;
};

//-----------------------------------------------------------------------------
//	RemoteDLL
//-----------------------------------------------------------------------------
#if 0
template<typename T> struct flat_array : trailing_array2<flat_array<T>, T> {
	typedef trailing_array<flat_array<T>, T> B;
	uint32	curr_size;
	uint32	size()	const	{ return curr_size; }
	template<typename C> flat_array&	operator=(C &c)		{ using iso::begin; curr_size = num_elements32(c); copy_new_n(begin(c), begin(), curr_size); return *this; }
};

struct RemoteDLL {
	Process		process;
	HMODULE		dll_local;
	HMODULE		dll_remote;

	template<typename R, typename T> R Call(const char *func, const T &t) {
		R			r;
		if (auto func_local = (uintptr_t)GetProcAddress(dll_local, func))
			process.RunRemote((LPTHREAD_START_ROUTINE)(func_local + (uintptr_t)dll_remote - (uintptr_t)dll_local), t, r);
		else
			r = R();
		return		r;
	}
	template<typename...P> RemoteCall CallASync(const char *func, const P&...p) {
		if (auto func_local = (uintptr_t)GetProcAddress(dll_local, func))
			return process.RunRemoteASync((LPTHREAD_START_ROUTINE)(func_local + (uintptr_t)dll_remote - (uintptr_t)dll_local), tuple<P...>(p...));
		return {};
	}

	template<typename T> int	Call(const char *func, const T &t)	{ return Call<int, T>(func, t);	}
	template<typename R> R		Call(const char *func)				{ return Call<R>(func, 0);	}
	int							Call(const char *func)				{ return Call(func, 0);	}

	template<typename R, typename T> dynamic_array<R> CallArray(const char *func, size_t max, const T &t) {
		if (auto func_local = (uintptr_t)GetProcAddress(dll_local, func)) {
			malloc_block	r(flat_array<R>::calc_size((uint32)max));
			if (process.RunRemote((LPTHREAD_START_ROUTINE)(func_local + (uintptr_t)dll_remote - (uintptr_t)dll_local), &t, sizeof(T), r, r.length()))
				return *(flat_array<R>*)r;
		}
		return empty;
	}
	template<typename R> dynamic_array<R> CallArray(const char *func, size_t max) {
		return CallArray<R>(func, max, 0);
	}
	template<typename T> void CallBlock(const char *func, memory_block r, const T &t) {
		if (auto func_local = (uintptr_t)GetProcAddress(dll_local, func))
			process.RunRemote((LPTHREAD_START_ROUTINE)(func_local + (uintptr_t)dll_remote - (uintptr_t)dll_local), &t, sizeof(T), r, r.length());
	}

	bool	Open(Process _process, const char *dll_name) {
		process = _process;
		dll_local	= GetModuleHandleA(dll_name);
		if (!dll_local)
			dll_local = LoadLibraryA(dll_name);

		filename	path;
		dll_remote	= process.FindModule(dll_name, path);
		return !!dll_remote;
	}

	bool	Open(Process _process, const char *dll_name, const char *dll_path) {
		if (Open(_process, dll_name))
			return true;
		process.InjectDLL(dll_path);
		filename	path;
		dll_remote	= process.FindModule(dll_name, path);
		return !!dll_remote;
	}

	RemoteDLL() {}
	RemoteDLL(Process _process, const char *dll_name, const char *dll_path) { Open(_process, dll_name, dll_path); }
};
#endif
#endif

//-----------------------------------------------------------------------------
//	EHDATA
//-----------------------------------------------------------------------------

template<typename T> using exc_ptr = offset_pointer<T, int, void, false>;

struct TypeDescriptor {
	const void		*pVFTable;	// Field overloaded by RTTI
	void			*spare;		// reserved, possible for RTTI
	embedded_string	name;		// The decorated name of the type

	friend bool is_ellipsis(const TypeDescriptor *td) { return !td || !td->name[0]; }
};

// Description of the thrown object
// pExceptionObject (the dynamic part of the throw; see below) is always a reference, whether or not it is logically one
// If 'isSimpleType' is true, it is a reference to the simple type, which is 'size' bytes long
// If 'isReference' and 'isSimpleType' are both false, then it's a UDT or a pointer to any type (i.e. pException Object points to a pointer)
// If it's a pointer, copyFunction is NULL, otherwise it is a pointer to a copy constructor or copy constructor closure
// The ForwardCompat function pointer is intended to be filled in by future versions, so that if say a DLL built with a newer version (say C10) throws, and a C9 frame attempts a catch, the frame handler attempting the catch (C9) can let the version that knows all the latest stuff do the work

struct ThrowInfo {
	typedef exc_ptr<void __cdecl(void*)> PMFN;	// Image relative offset of Member Function

	// Pointer to Member Data
	struct PMD {
		int			mdisp;	// Offset of intended data within base
		int			pdisp;	// Displacement to virtual base pointer
		int			vdisp;	// Index within vbTable to offset of base
	};

	struct CatchableType {
		enum {
			SimpleType		= 0x00000001,	// type is a simple type
			ByReferenceOnly	= 0x00000002,	// type must be caught by reference
			HasVirtualBase	= 0x00000004,	// type is a class with virtual bases
			WinRTHandle		= 0x00000008,	// type is a winrt handle
			StdBadAlloc		= 0x00000010,	// type is a std::bad_alloc
		};
		unsigned 	properties;			// Catchable Type properties (Bit field)
		exc_ptr<TypeDescriptor>	type;	// Image relative offset of TypeDescriptor
		PMD 		thisDisplacement;	// Pointer to instance of catch type within thrown object.
		int			sizeOrOffset;		// Size of simple-type object or offset into buffer of 'this' pointer for catch object
		PMFN		copyFunction;		// Copy constructor or CC-closure
	};

	struct CatchableTypeArray {
		int						num;
		exc_ptr<CatchableType>	types[];
	};

	enum {
		IsConst		= 0x00000001,	// thrown object has const qualifier
		IsVolatile	= 0x00000002,	// thrown object has volatile qualifier
		IsUnaligned	= 0x00000004,	// thrown object has unaligned qualifier
		IsPure		= 0x00000008,	// object thrown from a pure module
		IsWinRT		= 0x00000010,	// object thrown is a WinRT Exception
	};
	unsigned int	attributes;

	PMFN			Unwind;								// Destructor to call when exception has been handled or aborted
	exc_ptr<int __cdecl(...)>		ForwardCompat;		// Image relative offset of Forward compatibility frame handler
	exc_ptr<CatchableTypeArray>		catchable_types;
};

extern "C" __declspec(noreturn) void __stdcall _CxxThrowException(void* pExceptionObject, ThrowInfo* pThrowInfo);
extern "C" int __cdecl __CxxExceptionFilter(void* ppExcept, void* pType, int adjectives, void *pBuildObj);

// Returns true if the object is really a C++ exception, and stores the previous exception in *storage, and saves the current one
extern "C" int __cdecl __CxxRegisterExceptionObject(void *exception, void *storage);

// Returns true if exception is a C++ rethrown exception, so Unregister can know whether or not to destroy the object
extern "C" int __cdecl __CxxDetectRethrow(void *exception);

// Returns the byte count of stack space required to store the exception info
extern "C" int __cdecl __CxxQueryExceptionSize(void);

// Pops the current exception, restoring the previous one from *storage; detects whether or not the exception object needs to be destroyed
extern "C" void __cdecl __CxxUnregisterExceptionObject(void *storage, int re_throw);

// HandlerType - description of a single 'catch'
struct HandlerType {
	enum {
		IsConst				= 0x00000001,	// type referenced is 'const' qualified
		IsVolatile			= 0x00000002,	// type referenced is 'volatile' qualified
		IsUnaligned			= 0x00000004,	// type referenced is 'unaligned' qualified
		IsReference			= 0x00000008,	// catch type is by reference
		IsResumable			= 0x00000010,	// the catch may choose to resume (Reserved)
		IsStdDotDot			= 0x00000040,	// the catch is std C++ catch(...) which is supposed to catch only C++ exceptions
		IsBadAllocCompat	= 0x00000080,	// the WinRT type can catch a std::bad_alloc
		IsComplusEh			= 0x80000000,	// Is handling within complus EH
	};
	unsigned				adjectives;			// Handler Type adjectives (bitfield)
	exc_ptr<TypeDescriptor>	type;				// Image relative offset of the corresponding type descriptor
	int						dispCatchObj;		// Displacement of catch object from base
	exc_ptr<void>			handler;			// Image relative offset of 'catch' code
	int						dispFrame;			// displacement of address of function frame wrt establisher frame
};

// HandlerMapEntry - associates a handler list (sequence of catches) with a range of eh-states
struct TryBlockMapEntry {
	int						tryLow;		// Lowest state index of try
	int						tryHigh;	// Highest state index of try
	int						catchHigh;	// Highest state index of any associated catch
	int						nCatches;	// Number of entries in array
	exc_ptr<HandlerType>	handlers;	// Image relative offset of list of handlers for this try
};

// Description of each state transition for unwinding	the stack (i.e. calling destructors).

// The unwind map is an array, indexed by current state.  Each entry specifies the state to go to during unwind, and the action required to get there.
// Note that states are represented by a signed integer, and that the 'blank' state is -1 so that the array remains 0-based (because by definition there is never any unwind action to be performed from state -1).
// It is also assumed that state indices will be dense, i.e. that there will be no gaps of unused state indices in a function

struct UnwindMapEntry {
	int						toState;	// State this action takes us to
	exc_ptr<void __cdecl()>	action;
};

struct IptoStateMapEntry {
	exc_ptr<void>	Ip;		// Image relative offset of IP
	int				State;
};

struct ESTypeList {
	int 					num;
	exc_ptr<HandlerType>	array;	// offset of list of types in exception specification
};

// all the information that describes a function with exception handling information.
struct FuncInfo {
	enum BBT_FLAGS {
		UNIQUE_FUNCINFO = 1,
	};
	enum EH_FLAGS {
		EHS_FLAG             = 0x00000001,
		DYNSTKALIGN_FLAG     = 0x00000002,
		EHNOEXCEPT_FLAG      = 0x00000004,
	};
	unsigned					magicNumber : 29;	// version of compiler
	unsigned					BBTflags : 3;		// flags that may be set by BBT processing
	int							maxState;			// Highest state number plus one (thus number of entries in unwind map)
	exc_ptr<UnwindMapEntry>		UnwindMap;			// Image relative offset of the unwind map
	unsigned					nTryBlocks;			// Number of 'try' blocks in this function
	exc_ptr<TryBlockMapEntry>	TryBlockMap;		// Image relative offset of the handler map
	unsigned					nIPMapEntries;		// # entries in the IP-to-state map. NYI (reserved)
	exc_ptr<IptoStateMapEntry>	IPtoStateMap;		// Image relative offset of the IP to state map
	int							dispUwindHelp;		// Displacement of unwind helpers from base
	exc_ptr<ESTypeList>			dispESTypeList;		// Image relative list of types for exception specifications
	int							EHflags;			// Flags for some features.
};

struct UNWIND_INFO {
	enum OP {
		PUSH_NONVOL		= 0,	//1 node		Push a nonvolatile integer register, decrementing RSP by 8. info indicates register: RAX=0,RCX=1,RDX=2,RBX=3,RSP=4,RBP=5,RSI=6,RDI=7,R8 to R15=8..15
		ALLOC_LARGE		= 1,	//2 or 3 nodes	Allocate area on the stack. info=0: size = next slot * 8; info=1: size = next two slots
		ALLOC_SMALL		= 2,	//1 node		Allocate area on the stack of info * 8 + 8
		SET_FPREG		= 3,	//1 node		Set frame pointer register as rsp + offset*16
		SAVE_NONVOL		= 4,	//2 nodes		Save a nonvolatile integer register on the stack. info = register, offset = next*8
		SAVE_NONVOL_FAR	= 5,	//3 nodes		Save a nonvolatile integer register on the stack. info = register, offset = next 2 slots
		SAVE_XMM128		= 8,	//2 nodes		Save all 128 bits of a nonvolatile XMM register on the stack. info = register, offset = next * 16
		SAVE_XMM128_FAR	= 9,	//3 nodes		Save all 128 bits of a nonvolatile XMM register on the stack. info = register, offset = next 2 slots
		PUSH_MACHFRAME	= 10,	//1 node		Push a machine frame. info = 0 => stack: RSP+32, SS, RSP+24, Old RSP, RSP+16, EFLAGS, RSP+8, CS, RSP, RIP; info = 1 => stack: RSP+40, SS, RSP+32, Old RSP, RSP+24, EFLAGS, RSP+16, CS, RSP+8, RIP, RSP, Error code
	};
	union CODE {
		struct {
			uint8	prolog_offset;
			uint8	unwind_code:4, info:4;
		};
		uint16	frame_offset;
	};

	enum FLAGS {
		EHANDLER	= 1 << 0,	// function has an exception handler - called when looking for functions that need to examine exceptions
		UHANDLER	= 1 << 1,	// function has a termination handler - called when unwinding an exception
		CHAINED		= 1 << 2,	// this is a copy of a previous EXCEPTION_DIRECTORY::ENTRY for chaining
	};
	uint8	version:3, flags:5;
	uint8	prolog_size;	// Length of the function prolog in bytes
	uint8	num_codes;
	uint8	frame_reg:4, frame_offset:4;
	CODE	codes[];
	/*  CODE MoreUnwindCode[((CountOfCodes+1)&~1)-1];
	 *  union {
	 *      OPTIONAL uint32 ExceptionHandler;
	 *      OPTIONAL uint32 FunctionEntry;
	 *  };
	 *  OPTIONAL uint32 ExceptionData[];
	 */
};


struct DispatcherContext {
	uint64					ControlPc;
	uint64					ImageBase;
	RUNTIME_FUNCTION		*FunctionEntry;
	uint64					EstablisherFrame;
	uint64					TargetIp;
	CONTEXT					*ContextRecord;
	void					*LanguageHandler;
	void					*HandlerData;
	UNWIND_HISTORY_TABLE	*HistoryTable;
};

// The NT Exception record that we use to pass information from the throw to the possible catches
struct EHExceptionRecord {
	enum {
		EXCEPTION_NUMBER	= ('msc' | 0xE0000000),	// The NT Exception # that we use
		MAGIC_NUMBER1		= 0x19930520,			// The original, still used in exception records for native or mixed C++ thrown objects
		MAGIC_NUMBER2		= 0x19930521,			// Used in the FuncInfo if exception specifications are supported and FuncInfo::pESTypeList is valid
		MAGIC_NUMBER3		= 0x19930522,			// Used in the FuncInfo if FuncInfo::EHFlags is valid, so we can check if the function was compiled -EHs or -EHa
		PURE_MAGIC_NUMBER1	= 0x01994000,

		EXCEPTION_PARAMETERS = 4,				// Number of parameters in exception record
	};

	uint32				ExceptionCode;			// The code of this exception. (= EH_EXCEPTION_NUMBER)
	uint32				ExceptionFlags;			// Flags determined by NT
	EHExceptionRecord*	ExceptionRecord;		// An extra exception record (not used)
	void*				ExceptionAddress;		// Address at which exception occurred
	uint32 				NumberParameters;		// Number of extended parameters. (= EH_EXCEPTION_PARAMETERS)

	struct EHParameters {
		uint32			magicNumber;		// = EH_MAGIC_NUMBER1
		void*			ExceptionObject;	// Pointer to the actual object thrown
		ThrowInfo*		ThrowInfo;			// Description of thrown object
		void*			ThrowImageBase;		// Image base of thrown object
	} params;

	bool is_msvc()	const {
		return ExceptionCode == EXCEPTION_NUMBER && NumberParameters == 4
			&& is_any(params.magicNumber, MAGIC_NUMBER1, MAGIC_NUMBER2, MAGIC_NUMBER3);
	}
	bool is_msvc_pure_or_native() const {
		return ExceptionCode == EXCEPTION_NUMBER && NumberParameters == 4
			&& is_any(params.magicNumber, MAGIC_NUMBER1, MAGIC_NUMBER2, MAGIC_NUMBER3, PURE_MAGIC_NUMBER1);
	}
	bool has_es() const {
		return params.magicNumber >= MAGIC_NUMBER2;
	}
};

//-----------------------------------------------------------------------------
//	Hook
//-----------------------------------------------------------------------------

struct Hook : link_base<Hook> {
	const char	*name;
	void		*orig;
	void		*dest;

	Hook *set(const char *_name, void *_dest) {
		name		= _name;
		dest		= _dest;
		orig		= 0;
		return this;
	}

	bool		Apply(void **IATentry);
	bool		Remove(void **IATentry);
};

template<typename F, F *f> struct T_Hook : Hook {
	F	*orig()								{ return (F*)Hook::orig; }
	Hook *set(const char *_name, F *_dest)	{ return Hook::set(_name, (void*)_dest); }
};

template<typename F> struct hook_helper {
	template<F *f> static auto get() {
		static T_Hook<F,f>	hook;
		return &hook;
	}
};

void		AddHook(const char *module_name, Hook *hook);
void		RemoveHook(void **orig, const char *function, const char *module_name, void *dest);
void*		HookImmediate(const char *function, const char *module_name, void *dest);
void		ApplyHooks();

#define get_hook(f)				hook_helper<decltype(f)>::get<f>()
#define get_orig(f)				hook_helper<decltype(f)>::get<f>()->orig()
#define hook(f, module_name)	AddHook(module_name, get_hook(f)->set(#f, Hooked_##f))

} // namespace iso

#endif // HOOK_H

