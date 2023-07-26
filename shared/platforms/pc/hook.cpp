#include "hook.h"
#include "base/array.h"
#include "base/tree.h"
#include "base/algorithm.h"
#include "base/bits.h"
#include "base/functions.h"
#include "filename.h"

#include <winternl.h>
#include <winnt.h>
#include <minidumpapiset.h>
//#include <dbghelp.h>

//#pragma comment(lib, "dbghelp")

namespace iso {

HMODULE FindModule(DWORD pid, const char *name, filename &fn) {
	ModuleSnapshot	snapshot(pid, TH32CS_SNAPMODULE);
	for (auto &i : snapshot.modules()) {
		if (istr(i.szModule) == name) {
			fn = i.szExePath;
			return i.hModule;
		}
	}
	return 0;
}

filename FindModulePath(DWORD pid, const char *name) {
	ModuleSnapshot	snapshot(pid, TH32CS_SNAPMODULE);
	for (auto &i : snapshot.modules()) {
		if (i.szModule == istr(name))
			return i.szExePath;
	}
	return "";
}

ModuleSnapshot::modules_t::iterator ModuleSnapshot::find(const void *p) {
	for (auto i = modules().begin(); i; ++i) {
		if (between(p, i->modBaseAddr, i->modBaseAddr + i->modBaseSize))
			return i;
	}
	return modules().end();
}

HMODULE GetModuleKernel32() {
	static HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
	return kernel32;
}

HMODULE GetModuleNT() {
	static HMODULE nt = GetModuleHandleA("ntdll");
	return nt;
}

PROCESS_BASIC_INFORMATION Process::GetBasicInformation() const {
	PROCESS_BASIC_INFORMATION pbi;
	dll_call<LONG>(GetModuleNT(), "NtQueryInformationProcess", hProcess, DWORD(0), &pbi, DWORD(sizeof(pbi)), (PDWORD)NULL);
	return pbi;
}

void Process::InjectDLL(const char *lib) {
	fixed_string<MAX_PATH + 1, char16>	lib16 = str(lib);
	RunRemote(hProcess, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleKernel32(), "LoadLibraryW"), lib16);
}

void Process::TerminateSafe(uint32 code) {
	CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleKernel32(), "ExitProcess"), (void*)code, 0, NULL);
}

void Process::Suspend() {
	dll_call<void>(GetModuleNT(), "NtSuspendProcess", hProcess);
}

void Process::Resume() {
	dll_call<void>(GetModuleNT(), "NtResumeProcess", hProcess);
}


struct SID;
template<> struct deleter<SID> { void operator()(void *p) { FreeSid((SID*)p); } };

// Determine if the current thread is running as a user that is a member of the local admins group.
// Based on code from KB #Q118626

bool IsCurrentUserLocalAdministrator() {
	// Create a security descriptor that has a DACL which has an ACE that allows only local administrators access and call AccessCheck with the current thread's token and the security descriptor
	// It will say whether the user could access an object if it had that security descriptor
	const DWORD ACCESS_READ	 = 1;
	const DWORD ACCESS_WRITE = 2;

	try {
		// AccessCheck() requires an impersonation token, so get a primary token and then create a duplicate impersonation token
		// The impersonation token is only used in the call to AccessCheck, so this function itself never impersonates, but does use the identity of the thread
		// If the thread was impersonating already, this function uses that impersonation context
		Win32Handle	hToken;
		if (!OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE | TOKEN_QUERY, TRUE, hToken)) {
			if (GetLastError() != ERROR_NO_TOKEN || !OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, hToken))
				return false;
		}

		Win32Handle	hImpersonationToken;
		if (!DuplicateToken(hToken, SecurityImpersonation, hImpersonationToken))
			return false;

		// Create the binary representation of the well-known SID that represents the local administrators group...
		SID_IDENTIFIER_AUTHORITY	SystemSidAuthority = SECURITY_NT_AUTHORITY;
		unique_ptr<SID>				sid;
		if (!AllocateAndInitializeSid(&SystemSidAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, (void**)&sid))
			return false;

		// ...then create the security descriptor and DACL with an ACE that allows only local admins access
		DWORD				acl_size	= sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(sid) - sizeof(DWORD);
		ACL					*acl		= (ACL*)alloca(acl_size);
		SECURITY_DESCRIPTOR	admin;
		if (	!InitializeAcl(acl, acl_size, ACL_REVISION2)
			||	!AddAccessAllowedAce(acl, ACL_REVISION2, ACCESS_READ | ACCESS_WRITE, sid)
			||	!InitializeSecurityDescriptor(&admin, SECURITY_DESCRIPTOR_REVISION)
			||	!SetSecurityDescriptorDacl(&admin, TRUE, acl, FALSE)
			||	!SetSecurityDescriptorGroup(&admin, sid, FALSE)
			||	!SetSecurityDescriptorOwner(&admin, sid, FALSE)
			||	!IsValidSecurityDescriptor(&admin)
		)
			return false;

		// ...then perform the access check to determine whether the current user is a local admin
		GENERIC_MAPPING GenericMapping	= {ACCESS_READ, ACCESS_WRITE, 0, ACCESS_READ | ACCESS_WRITE};
		PRIVILEGE_SET	ps;
		DWORD			ps_size = sizeof(PRIVILEGE_SET);
		DWORD			status;
		BOOL			ret;
		return AccessCheck(&admin, hImpersonationToken, ACCESS_READ, &GenericMapping, &ps, &ps_size, &status, &ret) && ret;

	} catch (...) {
		return false;
	}
}

#pragma pack(push,8)

// http://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/thread.htm
// Size = 0x40 for Win32
// Size = 0x50 for Win64
struct SYSTEM_THREAD {
	enum KWAIT_REASON {
		Executive,
		FreePage,
		PageIn,
		PoolAllocation,
		DelayExecution,
		Suspended,
		UserRequest,
		WrExecutive,
		WrFreePage,
		WrPageIn,
		WrPoolAllocation,
		WrDelayExecution,
		WrSuspended,
		WrUserRequest,
		WrEventPair,
		WrQueue,
		WrLpcReceive,
		WrLpcReply,
		WrVirtualMemory,
		WrPageOut,
		WrRendezvous,
		Spare2,
		Spare3,
		Spare4,
		Spare5,
		Spare6,
		WrKernel,
		MaximumWaitReason
	};
	enum THREAD_STATE {
		Running = 2,
		Waiting = 5,
	};

	LARGE_INTEGER	KernelTime;
	LARGE_INTEGER	UserTime;
	LARGE_INTEGER	CreateTime;
	ULONG			WaitTime;
	PVOID			StartAddress;
	NT::_CLIENT_ID	ClientID;           // process/thread ids
	LONG			Priority;
	LONG			BasePriority;
	ULONG			ContextSwitches;
	THREAD_STATE	ThreadState;
	KWAIT_REASON	WaitReason;

	uint32			ID()			const	{ return (uint32)(uintptr_t)ClientID.UniqueThread; }
	bool			IsSuspended()	const	{ return ThreadState == Waiting && WaitReason == Suspended; }
};

struct VM_COUNTERS { // virtual memory of process
	ULONG_PTR		PeakVirtualSize;
	ULONG_PTR		VirtualSize;
	ULONG			PageFaultCount;
	ULONG_PTR		PeakWorkingSetSize;
	ULONG_PTR		WorkingSetSize;
	ULONG_PTR		QuotaPeakPagedPoolUsage;
	ULONG_PTR		QuotaPagedPoolUsage;
	ULONG_PTR		QuotaPeakNonPagedPoolUsage;
	ULONG_PTR		QuotaNonPagedPoolUsage;
	ULONG_PTR		PagefileUsage;
	ULONG_PTR		PeakPagefileUsage;
};

// http://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/process.htm
// See also SYSTEM_PROCESS_INROMATION in Winternl.h
// Size = 0x00B8 for Win32
// Size = 0x0100 for Win64
struct SYSTEM_PROCESS {
	ULONG          NextEntryOffset; // relative offset
	ULONG          ThreadCount;
	LARGE_INTEGER  WorkingSetPrivateSize;
	ULONG          HardFaultCount;
	ULONG          NumberOfThreadsHighWatermark;
	ULONGLONG      CycleTime;
	LARGE_INTEGER  CreateTime;
	LARGE_INTEGER  UserTime;
	LARGE_INTEGER  KernelTime;
	UNICODE_STRING ImageName;
	LONG           BasePriority;
	PVOID          UniqueProcessId;
	PVOID          InheritedFromUniqueProcessId;
	ULONG          HandleCount;
	ULONG          SessionId;
	ULONG_PTR      UniqueProcessKey;
	VM_COUNTERS    VmCounters;
	ULONG_PTR      PrivatePageCount;
	IO_COUNTERS    IoCounters;   // defined in winnt.h

	uint32			ID() const	{ return (uint32)(uintptr_t)UniqueProcessId; }

	auto			Threads() const {
		return make_range_n((const SYSTEM_THREAD*)(this + 1), ThreadCount);
	}
	const SYSTEM_THREAD* FindThreadByTid(DWORD id) {
		for (auto &i : Threads()) {
			if (i.ID() == id)
				return &i;
		}
		return 0;
	}

	bool			IsSuspended()	const	{
		for (auto &t : Threads()) {
			if (t.ThreadState != SYSTEM_THREAD::Waiting)
				return false;
		}
		return true;
	}

	const SYSTEM_PROCESS	*next() const {
		return NextEntryOffset ? (SYSTEM_PROCESS*)((BYTE*)this + NextEntryOffset) : 0;
	}
};

#pragma pack(pop)

class ProcessInfo {
	malloc_block	data;
public:
	ProcessInfo() {
		enum { STATUS_INFO_LENGTH_MISMATCH = 0xC0000004 };
		dll_function<LONG WINAPI(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG)>	NtQuerySystemInformation(GetModuleNT(), "NtQuerySystemInformation");
		if (NtQuerySystemInformation) {
			LONG	status;
			ULONG	needed = 0;
			while ((status = NtQuerySystemInformation(SystemProcessInformation, data, data.size32(), &needed)) == STATUS_INFO_LENGTH_MISMATCH)
				data.create(needed + 4000);

			if (status != 0)
				data.clear();
		}
	}

	auto	Processes() const {
		return make_next_range<const SYSTEM_PROCESS>(data, 0);
	}

	const SYSTEM_PROCESS* FindProcess(DWORD id) const {
		for (auto &p : Processes()) {
			if (p.ID() == id)
				return &p;
		}
		return 0;
	}
};

void Process2::_GetMainThread() {
#if 1
	ProcessInfo	pi;
	if (auto p = pi.FindProcess(dwProcessId))
		dwThreadId	= p->Threads().front().ID();
	else
		dwThreadId	= 0;
#else
	ModuleSnapshot	snapshot(0, TH32CS_SNAPTHREAD);
	for (auto &e : snapshot.threads()) {
		if (e.th32OwnerProcessID == dwProcessId) {
			dwThreadId = e.th32ThreadID;
			break;
		}
	}
#endif
	hThread		= dwThreadId ? OpenThread(READ_CONTROL , FALSE, dwThreadId) : INVALID_HANDLE_VALUE;
}

bool Process2::IsSuspended() const {
	return ProcessInfo().FindProcess(dwProcessId)->IsSuspended();
}

uint32 Process2::_Open(const filename &app, const char *dir, const char *args, uint32 flags) {
	STARTUPINFOA	si	= {sizeof(STARTUPINFOA), 0};
	string			cmd	= "\"" + app + "\"";

	if (args)
		cmd += " " + str(args);

	return CreateProcessA(
		NULL,									// ApplicationName
		cmd,									// CommandLine
		NULL,									// ProcessAttributes
		NULL,									// ThreadAttributes
		false,									// InheritHandles
		flags,									// CreationFlags
		NULL,									// Environment
		dir && dir[0] ? dir : app.dir().begin(),// CurrentDirectory
		&si,									// StartupInfo
		(PROCESS_INFORMATION*)this				// ProcessInformation
	) ? dwProcessId : 0;
}

bool MiniDump(EXCEPTION_POINTERS *ep, const char *fn) {
	MINIDUMP_EXCEPTION_INFORMATION mdi;
	clear(mdi);
	mdi.ThreadId			= GetCurrentThreadId();
	mdi.ExceptionPointers	= ep;

	HANDLE		hFile		= CreateFileA(fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	bool	ret = MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		MiniDumpWithIndirectlyReferencedMemory,
		&mdi,
		NULL,
		NULL
	);
	CloseHandle(hFile);
	return ret;
}

//-----------------------------------------------------------------------------
//	Hooks
//-----------------------------------------------------------------------------

void *ApplyHook(void **IATentry, void *hook) {
	void	*orig = *IATentry;
	if (orig != hook) {
		DWORD	prot;
		CriticalSection	cs;
		if (!VirtualProtect(IATentry, sizeof(void*), PAGE_READWRITE, &prot))
			return 0;

		*IATentry = hook;
		if (!VirtualProtect(IATentry, sizeof(void*), prot, &prot))
			return 0;
	}
	return orig;
}

bool RemoveHook(void **IATentry, void *orig) {
	if (!orig)
		return false;

	DWORD prot;
	if (!VirtualProtect(IATentry, sizeof(void*), PAGE_READWRITE, &prot))
		return false;

	*IATentry = orig;
	return !!VirtualProtect(IATentry, sizeof(void*), prot, &prot);
}

template<typename T> T *byte_offset(T *t, intptr_t i) {
	return (T*)((uint8*)t + i);
}

void *HookImmediate(const char *function, const char *module_name, void *dest) {
	filename	fn;
	HMODULE		hmod = LoadLibraryA("module_name");
	FARPROC		orig = GetProcAddress(hmod, function);

	if (orig) {
		for (auto &i : ModuleSnapshot(GetCurrentProcessId(), TH32CS_SNAPMODULE).modules()) {
			if (Module mod = i) {
				if (auto iat = mod.GetMem(mod.DataDir(IMAGE_DIRECTORY_ENTRY_IMPORT))) {
					for (const IMAGE_IMPORT_DESCRIPTOR *imp = iat; imp->FirstThunk; imp++) {
						if (imp->OriginalFirstThunk && str(module_name) == (const ichar*)mod.offset(imp->Name)) {
							for (const IMAGE_THUNK_DATA *orig = mod.offset(imp->OriginalFirstThunk); orig->u1.AddressOfData; orig++) {
								// check for ordinal
								if (!(rotate_left(orig->u1.AddressOfData, 1) & 1)) {
									const char *name = ((const IMAGE_IMPORT_BY_NAME*)mod.offset(orig->u1.AddressOfData))->Name;

									if (str(function) == name) {
										auto	*thunk	= byte_offset(orig, imp->FirstThunk - imp->OriginalFirstThunk);
										void	**entry = (void**)&thunk->u1.AddressOfData;
										ApplyHook(entry, dest);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return (void*)orig;
}

bool Hook::Apply(void **IATentry) {
	if (void *old = ApplyHook(IATentry, dest)) {
		if (!orig)
			orig = old;
		return true;
	}
	return false;
}

bool Hook::Remove(void **IATentry) {
	return RemoveHook(IATentry, orig);
}

struct ModuleHooks : Module {
	e_list<Hook>	hooks;
	ModuleHooks() {}

	void	Add(Hook *hook) {
		hooks.push_back(hook);
	}
//	void	Remove(const char *f, void **o, void *d) {
//		Hook	&h = hooks[f];
//		if (h.orig)
//			h.dest = *h.orig;
//	}

	Hook	*Find(const char *function) {
		auto	found = find_if(hooks, [function](Hook &h) { return str8(h.name) == function; });
		return found != hooks.end() ? found.get() : nullptr;
	}

};

class Hooks {
public:
	map<string, ModuleHooks>	modules;
	static bool					any;

	bool	Process(HMODULE module, bool apply);
	bool	ProcessAll(bool apply);

	FARPROC GetFunctionAddress(HMODULE mod, const char *function);

	void	Add(const char *module_name, Hook *hook) {
		modules[to_lower(module_name)].Add(hook);
		any = true;
	}
//	void	Remove(void **orig, const char *module_name, const char *function, void *dest) {
//		modules[to_lower(module_name)].Remove(function, orig, dest);
//	}

	Hooks();
};

bool	Hooks::any = false;
singleton<Hooks> hooks;

HMODULE WINAPI Hooked_LoadLibraryExA(const char *lib, HANDLE fileHandle, DWORD flags) {
	HMODULE mod = get_orig(LoadLibraryExA)(lib, fileHandle, flags);
	if (mod)
		hooks->ProcessAll(true);
	return mod;
}

HMODULE WINAPI Hooked_LoadLibraryExW(const char16 *lib, HANDLE fileHandle, DWORD flags) {
	HMODULE mod = get_orig(LoadLibraryExW)(lib, fileHandle, flags);
	if (mod)
		hooks->ProcessAll(true);
	return mod;
}

HMODULE WINAPI Hooked_LoadLibraryA(const char *lib) {
	HMODULE mod = get_orig(LoadLibraryA)(lib);
	if (mod)
		hooks->ProcessAll(true);
	return mod;
}

HMODULE WINAPI Hooked_LoadLibraryW(const char16 *lib) {
	HMODULE mod = get_orig(LoadLibraryW)(lib);
	if (mod)
		hooks->ProcessAll(true);
	return mod;
}

FARPROC WINAPI Hooked_GetProcAddress(HMODULE mod, const char *func) {
	return hooks->GetFunctionAddress(mod, func);
}

FARPROC WINAPI Hooked_GetProcAddressForCaller(HMODULE mod, const char *func, void *) {
	return hooks->GetFunctionAddress(mod, func);
}

#define hook0(f, module_name)	Add(module_name, get_hook(f)->set(#f, Hooked_##f))

Hooks::Hooks() {
	static Hook	hGetProcAddressForCaller;
	hook0(LoadLibraryExA,	"kernel32.dll");
	hook0(LoadLibraryExW,	"kernel32.dll");
	hook0(LoadLibraryA,		"kernel32.dll");
	hook0(LoadLibraryW,		"kernel32.dll");
	hook0(GetProcAddress,	"kernel32.dll");

	Add("kernel32.dll", hGetProcAddressForCaller.set("GetProcAddressForCaller", (void*)Hooked_GetProcAddressForCaller));

	/*
	AddAlias((void**)&get_orig(LoadLibraryExW),	"api-ms-win-core-libraryloader-l1-2-0.dll", "LoadLibraryExW",	&Hooked_LoadLibraryExW);
	AddAlias((void**)&get_orig(GetProcAddress),	"api-ms-win-core-libraryloader-l1-2-0.dll", "GetProcAddress",	&Hooked_GetProcAddress);

	AddAlias((void**)&get_orig(GetProcAddress),	"api-ms-win-core-libraryloader-l1-2-1.dll", "LoadLibraryA",	&Hooked_LoadLibraryA);
	AddAlias((void**)&get_orig(GetProcAddress),	"api-ms-win-core-libraryloader-l1-2-1.dll", "LoadLibraryW",	&Hooked_LoadLibraryW);
	*/
}

struct ModuleCopy : Module {
	ModuleCopy(HMODULE _h) {
		wchar_t path[1024] = {0};
		GetModuleFileNameW(_h, path, 1023);
		h	= get_orig(LoadLibraryExW) ? get_orig(LoadLibraryExW)(path, NULL, 0) : LoadLibraryExW(path, NULL, 0);
	}
	~ModuleCopy() {
		FreeLibrary(h);
	}
};

bool Hooks::Process(HMODULE h, bool apply) {
	ModuleCopy		mod(h);	// increment the module reference count
	if (!mod.valid())
		return false;

	if (auto iat = mod.Imports()) {
		char	ordinal[16] = "Ordinal";

		for (auto &imp : iat) {
			if (imp.OriginalFirstThunk) {
				const char *import = mod.offset(imp.Name);

				if (auto i = find(modules, (const ichar*)import)) {
					ModuleHooks	&hooks	= *i;
					for (auto &orig : imp.entries(mod)) {
						const char *name;
						// check for ordinal
						if (orig.IsOrdinal()) {
							if (!(name = hooks.OrdinalToName(orig.Ordinal()))) {
								to_string(ordinal + 7, orig.Ordinal());
								name = ordinal;
							}
						} else {
							name = orig.Name(mod);
						}

						if (auto found = hooks.Find(name)) {
							// don't patch if just forwarding (already moved)
							//if (i->Imports().Find(i, name))
							//	continue;
							auto	*thunk	= byte_offset(&orig, int32(imp.FirstThunk - imp.OriginalFirstThunk));
							void	**entry = (void**)&thunk->u1.AddressOfData;
							if (!(apply ? found->Apply(entry) : found->Remove(entry)))
								return false;
						}
					}
				}
			}
		}
	}
	return true;
}

bool Hooks::ProcessAll(bool apply) {
	ModuleSnapshot	snapshot(GetCurrentProcessId(), TH32CS_SNAPMODULE);

	// not sure if ok to add this here
	for (auto i = modules.begin(); i != modules.end(); ++i) {
		if (i->h == NULL && !(i->h = GetModuleHandleA(i.key())))
			continue;

		//for any hooks that this module is just forwarding, move hook to forwarded dll
		Module	mod(i->h);
		auto	exp = mod.Exports();
		for (auto hi = i->hooks.begin(), he = i->hooks.end(); hi != he; ++hi) {
			for (auto &imp : mod.Imports()) {
				if (auto thunk = imp.Find(mod, exp, hi->name)) {
					Add(mod.offset(imp.Name), hi.remove());
					break;
				}
			}
		}
	}

	// for any that were added
	for (auto i = modules.begin(); i != modules.end(); ++i) {
		if (i->h == NULL)
			i->h = GetModuleHandleA(i.key());
	}

	for (auto &i : snapshot.modules())
		Process(i.hModule, apply);

	return true;
}

FARPROC Hooks::GetFunctionAddress(HMODULE mod, const char *function) {
	if (uintptr_t(function) <= 0xffff) {
		if (const char *name = Module(mod).OrdinalToName(uintptr_t(function)))
			function = name;
	}

	if (/*mod != module && */uintptr_t(function) > 0xffff) {
		for (auto i = modules.begin(); i != modules.end(); ++i) {
			if (i->h == NULL)
				i->h = GetModuleHandleA(i.key());

			if (mod == i->h) {
				if (auto found = i->Find(function)) {
					if (!found->orig)
						found->orig = (void*)get_orig(GetProcAddress)(mod, function);

					return (FARPROC)found->dest;
				}
			}
		}
	}
	return get_orig(GetProcAddress)(mod, function);
}

void ApplyHooks() {
	if (Hooks::any)
		hooks->ProcessAll(true);
}

void AddHook(const char *module_name, Hook *hook) {
	hooks->Add(module_name, hook);
}

//void RemoveHook(void **orig, const char *function, const char *module_name, void *dest) {
//	hooks->Remove(orig, module_name, function, dest);
//}

} // namespace iso