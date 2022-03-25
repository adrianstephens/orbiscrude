#ifndef PLATFORM_H
#define PLATFORM_H

#define ISO_PLATFORM	ANDROID
#define ISO_PREFIX		OGL

#ifndef PLAT_ANDROID
#define PLAT_ANDROID
#endif

#define PLAT_OPENGL

#define USE_VARIADIC_TEMPLATES
#define USE_RVALUE_REFS

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"


#if TARGET_RT_BIG_ENDIAN
#define	ISO_BIGENDIAN
#endif

#ifdef __EXCEPTIONS
#define USE_EXCEPTIONS
#endif

#define USE_LONG		0

#define force_inline			inline
#define iso_unaligned(T)	T
#define iso_aligned(T, N)	T __attribute__((__aligned__(N)))
#define prefetch(i)			(void)0					//__pld(i)
#define probably(a, b)		__builtin_expect(b, a)
#define unreachable()		__builtin_unreachable()
#define	restrict			__restrict

#if defined(__LP64__)
#define __int64	long
#else
#define __int64 long long
#endif

typedef decltype(nullptr) nullptr_t;
#define _iso_break()

namespace iso {


static inline int		abs(int n)		{ return n < 0 ? -n : n; }
static inline __int64	abs(__int64 n)	{ return n < 0 ? -n : n; }
static inline unsigned __int64	random_seed()	{ return time(nullptr); }

inline timespec operator-(const timespec &a, const timespec &b) {
	timespec	t;
	t.tv_sec	= a.tv_sec - b.tv_sec;
	t.tv_nsec	= a.tv_nsec - b.tv_nsec;
	if (a.tv_nsec < b.tv_nsec) {
		t.tv_nsec += 1000000000;
		--t.tv_sec;
	}
	return t;
}

inline timespec operator+(const timespec &a, const timespec &b) {
	timespec	t;
	t.tv_sec	= a.tv_sec + b.tv_sec;
	t.tv_nsec	= a.tv_nsec + b.tv_nsec;
	if (t.tv_nsec >= 1000000000) {
		t.tv_nsec -= 1000000000;
		++t.tv_sec;
	}
	return t;
}

inline bool operator==(const timespec &a, const timespec &b) {
	return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}
inline bool operator<(const timespec &a, const timespec &b) {
	return a.tv_sec < b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_nsec < b.tv_nsec);
}
inline bool operator<=(const timespec &a, const timespec &b) {
	return a.tv_sec < b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_nsec <= b.tv_nsec);
}

struct _time {
	typedef timespec type;

	static type		now() {
		struct timespec res;
		clock_gettime(CLOCK_REALTIME, &res);
		return res;
	}
	static __int64	get_freq()			{ return 1000000000;	}
	static float	to_secs(type t)		{ return t.tv_sec + (float)t.tv_nsec / 1e9; }
	static type		from_secs(float f)	{ type t; t.tv_sec = int(f); t.tv_nsec = (f - int(f)) * 1e9f; return t; }
};

}

#endif
