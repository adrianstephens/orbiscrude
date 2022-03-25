#ifndef OBJC_H
#define OBJC_H

#include "base/defs.h"

#include <CoreFoundation/CFBase.h>

namespace iso {

template<typename T> struct CFptr {
	T	*t;
	force_inline CFptr() 				: t(nullptr) {}
	force_inline CFptr(T *t)				: t(t) 		{}
	force_inline CFptr(const CFptr &b)	: t(b.t) 	{ if (t) CFRetain(t); }
	force_inline CFptr(CFptr&& b)		: t(b.t) 	{ b.t = 0; }
	force_inline CFptr& operator=(const CFptr &b) 	{ if (t) CFRelease(t); if (t = b.t) CFRetain(t); return *this; }
	force_inline CFptr& operator=(CFptr &&b) 		{ if (t) CFRelease(t); t = b.t; b.t = 0; return *this; }
	force_inline ~CFptr()							{ if (t) CFRelease(t); }
	force_inline operator T*() const 				{ return t; }
	force_inline	T **operator&()					{ if (t) CFRelease(t); t = 0; return &t; }
};

template<typename T> using CFobj = CFptr<deref_t<T>>;

#ifdef __OBJC__

	#define OBJC_FORWRD(T)		@class T
	#define OBJC_CLASS(T)		@class T
	#define OBJC_INTRFACE(T)	@class T
	#define OBJC_PROTOCOL(T)	@protocol T

	template<typename T> using objc = T*;
	
	template<typename T> struct objc_noretain {
		void	*p;
		force_inline objc_noretain() 			: p(0) {}
		force_inline objc_noretain(T *t)			: p((__bridge void*)t)	{}
		force_inline objc_noretain& operator=(T *t) 	{ p = (__bridge void*)t; return *this; }
		force_inline operator T*() const			{ return (__bridge T*)p; }
		force_inline T*		get() const			{ return (__bridge T*)p; }
		force_inline T*		operator->() const	{ return (__bridge T*)p; }
		force_inline T*		operator+() const	{ return (__bridge T*)p; }
	};

#else

	#define OBJC_FORWRD(T)		struct T : iso::objc_object {}
	#define OBJC_CLASS(T)		struct T : iso::objc_object
	#define OBJC_INTRFACE(T)	struct T : iso::objc_object {}
	#define OBJC_PROTOCOL(T)	struct T : iso::objc_object {}

	typedef struct objc_class *Class;
	struct objc_object {
		Class isa  __attribute__((deprecated));
	};
	
	template<typename T> using objc	= CFptr<T>;

	template<typename T> struct objc_noretain {
		T	*t;
		force_inline objc_noretain(T *t = nullptr) : t(t) {}
		force_inline operator T*() const 		{ return t; }
	};

	template<typename T> using id 	= objc<T>;

#endif

}
#endif
