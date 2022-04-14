#ifndef _THREAD_H
#define _THREAD_H

#include "base/defs.h"
#include "base/functions.h"
#include <windows.h>
#include <mmsystem.h>
#undef min
#undef max

#define THREAD_STACK_DEFAULT 1024*1024//32768//8192

namespace iso {

struct Timeout {
	DWORD	u;
	constexpr Timeout()						: u(INFINITE)	{}
//	constexpr Timeout(decltype(infinity))	: u(INFINITE)	{}
	constexpr Timeout(float s)				: u(DWORD(s * 1000))	{}
	operator DWORD() const { return u; }
};

//-----------------------------------------------------------------------------
// Thread Local Storage
//-----------------------------------------------------------------------------

class _TLS {
	DWORD	tls;
protected:
	void	set(void *p)	const	{ TlsSetValue(tls, p); }
	void	*get()			const	{ return TlsGetValue(tls); }
	_TLS()		: tls(TlsAlloc())	{}
};

template<class T> class TLS : _TLS {
	struct temp {
		TLS	&tls;
		T	t;
		temp(TLS &_tls) : tls(_tls), t(_tls) {}
		~temp()						{ tls = t; }
		T* operator->()				{ return &t; }
	};
public:
	TLS()							{}
	TLS(const T &t)					{ set((void*&)t); }
	const T &operator=(const T &t)	{ set((void*&)t); return t; }
	operator const T()				{ return arbitrary(get()); }
	temp operator->()				{ return temp(*this); }
};

template<class T> class TLS<T*> : _TLS {
public:
	TLS()							{}
	TLS(T *t)						{ set(t); }
	T *operator=(T *t)				{ set(t); return t; }
	operator T*()					{ return (T*)get(); }
	T *operator->()					{ return (T*)get(); }
	T &operator *()					{ return *(T*)get(); }
};

//-----------------------------------------------------------------------------
// Thread
//-----------------------------------------------------------------------------

struct ThreadName {
	enum {EXCEPTION = 0x406D1388};

	DWORD	dwType;		// Must be 0x1000
	LPCSTR	szName;		// Pointer to name (in user address space)
	DWORD	dwThreadID;	// Thread ID (-1 for caller thread)
	DWORD	dwFlags;	// Reserved for future use; must be zero
	ThreadName(DWORD id, LPCSTR name) : dwType(0x1000), szName(name), dwThreadID(id), dwFlags(0) {
		__try {
			RaiseException(EXCEPTION, 0, sizeof(*this)/sizeof(DWORD), (ULONG_PTR*)this);
		} __except(EXCEPTION_CONTINUE_EXECUTION) {
		}
	}
};

 enum class ThreadPriority {
	HIGHEST	= THREAD_PRIORITY_HIGHEST,
	HIGH	= THREAD_PRIORITY_ABOVE_NORMAL,
	DEFAULT	= THREAD_PRIORITY_NORMAL,
	LOW		= THREAD_PRIORITY_BELOW_NORMAL,
	LOWEST	= THREAD_PRIORITY_LOWEST
};

class Thread : Win32Handle {
	static TLS<Thread*> curr;
	template<class T>	static DWORD WINAPI proc(T *t) { curr = t; return (*t)(); }
	void	_create(const char *name, int stack_size, ThreadPriority priority, void *p, LPTHREAD_START_ROUTINE proc);
public:
	static Thread*	current()			{ return curr; }
	static uint64	current_id()		{ return (uint64)::GetCurrentThreadId(); }
	static void		sleep(float t = 0)	{ Sleep(DWORD(t * 1000)); }
	static void		yield()				{ ::SwitchToThread(); }
	static void		exit(int ret)		{ ExitThread(ret); }
	static int		processors()		{ SYSTEM_INFO info; GetSystemInfo(&info); return info.dwNumberOfProcessors; }
	static int		processor()			{ return GetCurrentProcessorNumber(); }

	void			SetAffinity(uint64 affinity)	{ SetThreadAffinityMask(h, affinity); }
	void			SetProcessor(int processor)		{ SetAffinity(uint64(1) << processor); }
	bool			Start()							{ return h && ResumeThread(h) != (DWORD)-1; }
	void			Suspend()						{ SuspendThread(h); }
	int				Join(Timeout timeout = Timeout());

	template<class T> Thread(T *t, const char *name = 0, int stack_size = THREAD_STACK_DEFAULT, ThreadPriority priority = ThreadPriority::DEFAULT) {
		_create(name, stack_size, priority, t, (LPTHREAD_START_ROUTINE)proc<T>);
	}
	template<class T> Thread(T *t, const char *name, ThreadPriority priority) {
		_create(name, THREAD_STACK_DEFAULT, priority, t, (LPTHREAD_START_ROUTINE)proc<T>);
	}
};

class thread {
	template<typename F, typename... P> struct record {
		F			f;
		tuple<P...>	t;
		record(F &&f, P&&...p) : f(f), t(forward<P>(p)...) {}
		void	run()	{ call(f, t); }
		static DWORD WINAPI run(void *x) {
			auto	rp 	= (record*)x;
			record	r 	= move(*rp);
			delete rp;
			r.run();
			return 0;
		}
	};
	thread(HANDLE id) : id(id) {}

public:
	Win32Handle	id;

	class settings {
		const char		*name;
		ThreadPriority	priority;
		size_t			stack_size;
		HANDLE create(void *p, DWORD WINAPI (*proc)(void*)) {
			DWORD	id;
			HANDLE	h;
			if (h = CreateThread(NULL, stack_size, proc, p, 0, &id)) {
				SetThreadPriority(h, (int)priority);
				ThreadName(id, name);
			}
			return h;
		}
	public:
		settings(const char *name = 0, ThreadPriority priority = ThreadPriority::DEFAULT, size_t stack_size = THREAD_STACK_DEFAULT) : name(name), priority(priority), stack_size(stack_size) {}
		settings(ThreadPriority priority, size_t stack_size = THREAD_STACK_DEFAULT) : name(0), priority(priority), stack_size(stack_size) {}
		settings(size_t stack_size) : name(0), priority(ThreadPriority::DEFAULT), stack_size(stack_size) {}
		settings& affinity(uint64 processors) { return *this; }
		template<typename F, typename... P> thread create(F &&f, P&&...p) {
			return create(new record<F,P...> (forward<F>(f), forward<P>(p)...), record<F,P...> ::run);
		}
	};

	thread() 			: id() {}
	thread(thread &&b)	: id(move(b.id))	{}
	template<typename F, typename... P> thread(F &&f, P&&...p) : thread(settings().create(forward<F>(f), forward<P>(p)...)) {}
	~thread() 								{}
	thread& operator=(thread &&b)			{ swap(id, b.id); return *this; }
	bool	joinable()	const				{ return id.Valid(); }
	void	join()							{ WaitForSingleObject(id.detach(), INFINITE); }
	void	detach() 						{ id.detach(); }
	friend void swap(thread &a, thread &b) 	{ swap(a.id, b.id); }
};

//-----------------------------------------------------------------------------
// Event
//-----------------------------------------------------------------------------

class Event : public Win32Handle {
public:
	Event(bool manual_reset = false, bool set = false)	: Win32Handle(CreateEvent(nullptr, manual_reset, set, nullptr)) {}
	bool	signal()							const	{ return !!SetEvent(h); }
	bool	clear()								const	{ return !!ResetEvent(h); }
	bool	wait(Timeout timeout = Timeout())	const	{ return  WaitForSingleObject(h, timeout) == WAIT_OBJECT_0; }
};

//-----------------------------------------------------------------------------
// CriticalSection
//-----------------------------------------------------------------------------
class CriticalSection {
	friend class ConditionVariable;
	CRITICAL_SECTION	cs;
public:
	CriticalSection(int spincount)						{ InitializeCriticalSectionAndSpinCount(&cs, spincount); }
	CriticalSection()									{ InitializeCriticalSection(&cs); }
	~CriticalSection()									{ DeleteCriticalSection(&cs); }
	void	SetSpinCount(int count)						{ SetCriticalSectionSpinCount(&cs, count); }
	void	lock()										{ EnterCriticalSection(&cs); }
	bool	try_lock()									{ return !!TryEnterCriticalSection(&cs); }
	void	unlock()									{ LeaveCriticalSection(&cs); }
};

//-----------------------------------------------------------------------------
// Mutex
//-----------------------------------------------------------------------------
class Mutex : public Win32Handle {
public:
	Mutex(bool locked = false) : Win32Handle(CreateMutex(NULL, locked, NULL))	{}
	bool	lock(Timeout timeout = Timeout())	const	{ return WaitForSingleObject(h, timeout) == WAIT_OBJECT_0; }
	bool	try_lock()							const	{ return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }
	bool	unlock()							const	{ return !!ReleaseMutex(h); }
};

//-----------------------------------------------------------------------------
// Semaphore
//-----------------------------------------------------------------------------
class Semaphore : public Win32Handle {
	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;
public:
	static HANDLE Shared(int init_count = 1, int max_count = 0x7fffffff)	{
		SECURITY_ATTRIBUTES	sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
		return CreateSemaphore(&sa, init_count, max_count, NULL);
	}
	Semaphore(HANDLE h)			: Win32Handle(h)	{}
	Semaphore(int init_count = 1, int max_count = 0x7fffffff) : Win32Handle(CreateSemaphore(NULL, init_count, max_count, NULL))	{}
	Semaphore(Semaphore&&)		= default;
	bool	lock(Timeout timeout = Timeout())	const	{ return WaitForSingleObject(h, timeout) == WAIT_OBJECT_0; }
	bool	try_lock()							const	{ return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }
	bool	unlock(int count = 1)				const	{ return !!ReleaseSemaphore(h, count, NULL); }
};

//-----------------------------------------------------------------------------
// ConditionVariable
//-----------------------------------------------------------------------------

class ConditionVariable {
	CONDITION_VARIABLE cv;
public:
	ConditionVariable()							{ InitializeConditionVariable(&cv); }
	bool	lock(CriticalSection &cs, Timeout timeout = Timeout())	{ return !!SleepConditionVariableCS(&cv, &cs.cs, timeout); }
//	bool	lock(SRWLock &lock, Timeout timeout = Timeout(), bool shared = false)	{ SleepConditionVariableSRW(&cv, &lock, timeout, shared ? CONDITION_VARIABLE_LOCKMODE_SHARED : 0); }
	void	unLock_all()						{ WakeAllConditionVariable(&cv); }
	void	unlock()							{ WakeConditionVariable(&cv); }
};

//-----------------------------------------------------------------------------
// Fiber
//-----------------------------------------------------------------------------

#ifndef PLAT_WINRT

#undef Yield

class Fiber {

	static thread_local void *primary;

	static void _entry(void *params) {
		auto	entry = (uint64(*)(void*))params;
		entry(0);
		unreachable();
		SwitchToFiber(primary);
	}
	void	*p;
//	void	*next;
public:
	struct Internal {
		void		*data;
		void		*unk;
		void		*stackBase, *stackLimit, *stackReserved;
		uint32		unk3[2];
		_CONTEXT	context;
		uint64		unk4;
		uint64		ActiveFrame;
		uint64		tls4e, tls2e;
		uint64		cookie;	//stackbase ^ BasepFiberCookie
	};

	constexpr Fiber(void *p) : p(p) {}
	Fiber(uint64(*entry)(void *params), void *params, int stack_size = THREAD_STACK_DEFAULT) : p(CreateFiber(stack_size, _entry, (void*)entry)) {
		Internal	*i = (Internal*)p;
		if (!primary)
			primary = ConvertThreadToFiber(0);
//		next = GetCurrentFiber();
	}
	void			Switch()	const			{ SwitchToFiber(p); }

	static Fiber	Primary()					{ return primary; }
//	static Fiber	Current()					{ return GetCurrentFiber(); }
	static void*	CurrentData()				{ return GetFiberData(); }
};
#endif

} //namespace iso
#endif // _THREAD_H
