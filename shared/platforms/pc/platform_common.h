#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __clang__

#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-local-typedef"
#pragma GCC diagnostic ignored "-Wmicrosoft-template"
#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma GCC diagnostic ignored "-Wundefined-inline"
#pragma GCC diagnostic ignored "-Wint-to-void-pointer-cast"
#pragma GCC diagnostic ignored "-Wmultichar"
#pragma GCC diagnostic ignored "-Winvalid-token-paste"
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#pragma GCC diagnostic ignored "-Wundefined-var-template"
#pragma GCC diagnostic ignored "-Wmissing-braces"
//#pragma GCC diagnostic ignored "-Wundefined-var-template"
//#pragma GCC diagnostic ignored "-Wnonportable-include-path"
//#pragma GCC diagnostic ignored "-Wignored-pragma-intrinsic"
//#pragma GCC diagnostic ignored "-Wpragma-pack"
//#pragma GCC diagnostic ignored "-Wexpansion-to-defined"

#define specialised(X)
#define _FORCENAMELESSUNION
#else

#pragma warning(disable:4018 4146 4180 4200 4244 4291 4307 4311 4312 4344 4345 4351 4355 4503 4521 4522 4800)
#pragma warning(1:4308 4717)

#define specialised(X)	X

#endif

#define ISO_PLATFORM	PC

#ifndef PLAT_PC
#define PLAT_PC
#endif

#if !defined(PLAT_WINRT) && defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#define PLAT_WINRT
#endif

#if !defined PLAT_WIN32 && !defined PLAT_WINRT
#define PLAT_WIN32
#endif

#if defined _M_AMD64 || defined _M_ARM64
#define	ISO_PTR64
#endif

#ifdef _M_IX86
#define USE_STDCALL
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#define __PLACEMENT_NEW_INLINE
#define __PLACEMENT_VEC_NEW_INLINE
//inline void *operator new(size_t, void *p)		{ return p; }
//inline void operator delete(void*, void*)		{}
//inline void *operator new[](size_t, void *p)	{ return p; }
//inline void operator delete[](void*, void*)		{}

#ifndef USING_VSMATH
#define _INC_MATH
#endif

#define _CSTDLIB_
#define __D3DX9MATH_INL__
#define __XNAMATHVECTOR_INL__
#define __XNAMATHMATRIX_INL__
#define __XNAMATHMISC_INL__

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#endif

#ifdef _CPPUNWIND
#define USE_EXCEPTIONS
#endif

#ifdef _CPPRTTI
#define USE_RTTI
#endif

#if defined _MSC_VER && _MSC_VER >= 1900
#define USE_EXPLICIT_CONVERSIONS
#define USE_LAMBDAS
#define USE_RANGE_FOR
#define USE_RVALUE_REFS
#define USE_THREAD_LOCAL
#define USE_AUTO_TYPE
#define USE_STRONG_ENUMS
#define USE_VARIADIC_TEMPLATES
#endif

#if defined _M_ARM64 || defined _M_ARM
#define USE_SIGNEDCHAR
#endif

extern "C" {
long labs(long x);
long long llabs(long long x);
}

#if 0//ndef _CRT_ABS_DEFINED
namespace std {
	int abs(int x);
}
#define _CRT_ABS_DEFINED
#endif

#include <windows.h>
#include <crtdbg.h>
#include <string.h>
#include <stdlib.h>
#include <mmsystem.h>
#include <malloc.h>
#include <intrin.h>

#undef min
#undef max
#undef small

#define alloca				_alloca
#define _iso_break			__debugbreak
//#define _iso_break			iso::__break

#ifdef USE_IMPORTS
	#define iso_export		__declspec(dllimport)
	#define iso_local		__declspec(dllexport)
#elif defined(USE_EXPORTS)
	#define iso_export		__declspec(dllexport)
#else
	#define iso_export
#endif

#define __func__			__FUNCTION__

#if defined __clang__ && defined _DEBUG
#define force_inline		inline
#else
#define force_inline		__forceinline
#endif

#define no_inline			__declspec(noinline)
#define iso_unaligned(T)	UNALIGNED T
#define iso_aligned(T, N)	__declspec(align(N)) T
#define prefetch(i)			(void)0
#define probably(a, b)		(b)
#define unreachable()		__assume(0)
#define	restrict			__restrict
//#define fallthrough
#define DECL_ALLOCATOR		__declspec(allocator)

namespace iso {
using ::abs;

extern void __break();

extern "C" IMAGE_DOS_HEADER __ImageBase;
inline HINSTANCE		GetLocalInstance() { return reinterpret_cast<HINSTANCE>(&__ImageBase); }
iso_export void			SetDefaultInstance(HINSTANCE h);
iso_export HINSTANCE	GetDefaultInstance();
iso_export HINSTANCE	GetInstance(HINSTANCE h);

class Win32Handle {
protected:
	HANDLE	h;
public:
	Win32Handle(HANDLE h = INVALID_HANDLE_VALUE) : h(h)		{}
	Win32Handle(Win32Handle &&b) : h(b.detach())			{}
	~Win32Handle()							{ CloseHandle(h); }
	Win32Handle	&operator=(Win32Handle &&b)	{ auto t = h; h = b.h; b.h = t; return *this; }
	HANDLE	detach()			{ auto t = h; h = INVALID_HANDLE_VALUE; return t; }
	operator HANDLE*()			{ return &h; }
	//	HANDLE*	operator&()			{ return &h; }
	operator HANDLE()	const	{ return h; }
	bool	Valid()		const	{ return h != INVALID_HANDLE_VALUE; }
	bool	operator!()	const	{ return h == INVALID_HANDLE_VALUE; }
};

struct Win32Error {
	DWORD	err;
	va_list	*args;
	explicit Win32Error(DWORD _err, va_list *_args = 0) : err(_err), args(_args) {}
	Win32Error(va_list *_args = 0) : err(GetLastError()), args(_args) {}
};

void					Win32ErrorPrint(const Win32Error &v);
template<typename T> T	Win32ErrorCheck(const T &t,...)		{ if (!t) { va_list args; va_start(args, t); Win32ErrorPrint(&args); } return t; }
inline HINSTANCE		Win32ErrorCheck(HINSTANCE t,...)	{ if (!t) { va_list args; va_start(args, t); Win32ErrorPrint(&args); } return t; }
inline BOOL				Win32ErrorCheck(BOOL t,...)			{ if (!t) { va_list args; va_start(args, t); Win32ErrorPrint(&args); } return t; }

iso_export size_t		to_string(char *s, const Win32Error &v);

struct _time {
	typedef	__int64 type;
	static type		get_freq()			{ LARGE_INTEGER q; QueryPerformanceFrequency(&q); return q.QuadPart; }
	static type		now()				{ LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
	static float	to_secs(type t)		{ return float(t) / get_freq(); }
	static type		from_secs(float f)	{ return type(f * get_freq()); }
};

} // namespace iso

//-----------------------------------------------------------------------------
//	dll_function
//-----------------------------------------------------------------------------

#ifndef PLAT_WINRT
struct module {
	HMODULE m;
	module(HMODULE m) : m(m)	{}
	module(const char *name) : m(GetModuleHandleA(name)) {}
	constexpr operator HMODULE()	const	{ return m; }
	auto	find(const char *name)	const	{ return GetProcAddress(m, name); }
};

template<typename F> struct dll_function {
	F		*f;
	dll_function()	: f(0)	{}
	dll_function(module mod, const char *name) : f((F*)mod.find(name)) {}
	operator F*()						const	{ return f; }
	bool	bind(module mod, const char *name)	{ return !!(f = (F*)mod.find(name)); }
};

template<typename R, typename... PP> R dll_call(module mod, const char *name, PP... pp) {
	return dll_function<R NTAPI (PP...)>(mod, name)(pp...);
}
#endif

#endif
