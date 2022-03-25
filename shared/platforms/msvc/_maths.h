#ifndef _MATHS_H
#define _MATHS_H

#include "base/defs.h"
#include <float.h>

#undef min
#undef max
#undef abs

#ifndef USING_VSMATH
extern "C" {
_CRT_JIT_INTRINSIC double __cdecl fabs(_In_ double f);
double __cdecl fmod(_In_ double n, _In_ double d);
_CRT_JIT_INTRINSIC double __cdecl sqrt(_In_ double f);
_CRTIMP double  __cdecl floor(_In_ double _X);
_CRTIMP double __cdecl ceil(_In_ double f);
double __cdecl sin(_In_ double f);
double __cdecl cos(_In_ double f);
double __cdecl tan(_In_ double f);
double __cdecl atan(_In_ double f);
double __cdecl atan2(_In_ double s, _In_ double c);
double __cdecl asin(_In_ double f);
double __cdecl acos(_In_ double f);
double __cdecl exp(_In_ double f);
double __cdecl log(_In_ double f);
double __cdecl log10(_In_ double f);
double __cdecl pow(_In_ double f, _In_ double p);

//float __cdecl fabsf(float f);
_CRTIMP float __cdecl fmodf(_In_ float n, _In_ float d);
_CRTIMP float __cdecl sqrtf(_In_ float f);
_CRTIMP float __cdecl floorf(_In_ float f);
_CRTIMP float __cdecl ceilf(_In_ float f);
_CRTIMP float __cdecl sinf(_In_ float f);
_CRTIMP float __cdecl cosf(_In_ float f);
_CRTIMP float __cdecl tanf(_In_ float f);
_CRTIMP float __cdecl atanf(_In_ float f);
_CRTIMP float __cdecl atan2f(_In_ float s, _In_ float c);
_CRTIMP float __cdecl asinf(_In_ float f);
_CRTIMP float __cdecl acosf(_In_ float f);
_CRTIMP float __cdecl expf(_In_ float f);
_CRTIMP float __cdecl logf(_In_ float f);
_CRTIMP float __cdecl log10f(_In_ float f);
_CRTIMP float __cdecl powf(_In_ float f, _In_ float p);
}
#ifdef _MSC_V
#pragma intrinsic(fabs, fmod, sqrt, floor, ceil, sin, cos, tan, atan, atan2, asin, acos, exp, log, log10, pow)
#pragma intrinsic(fmodf,sqrtf,floorf,ceilf,sinf,cosf,tanf,atanf,atan2f,asinf,acosf,expf,logf,log10f,powf)
#endif

#endif

#define inline_fast(R)	__forceinline R
#define FLT_NAN			iorf(uint32(0x7fffffff)).f()

namespace iso {
using ::fmod;
using ::sqrt;
using ::floor;
using ::ceil;
using ::sin;
using ::cos;
using ::tan;
using ::atan;
using ::atan2;
using ::asin;
using ::acos;
using ::exp;
using ::log;
using ::log10;
using ::pow;

//-------------------------------------
// doubles
//-------------------------------------

//#ifndef USING_VSMATH
inline_fast(double)	abs			(double a)				{ return fabs(a); }
//#endif

inline_fast(double)	copysign	(double a, double b)	{ return iord(a).set_sign(iord(b).get_sign()).f(); }
inline_fast(double)	sign1		(double f)				{ return copysign(1.0, f); }
inline_fast(double)	sign		(double f)				{ return copysign(double(f!=0), f); }

inline_fast(double)	fsel		(double x, double y, double z) { return x >= 0? y : z; }
inline_fast(double)	mod			(double n, double d)	{ return fmod(n, d); }
inline_fast(double)	reciprocal	(double f)				{ return 1 / f; }
inline_fast(double)	rsqrt		(double f)				{ return 1 / sqrt(f); }

inline_fast(double)	trunc		(double f)				{ return iord(f).trunc().f(); }
inline_fast(double)	round		(double f)				{ return iord(f).round_ne().f(); }
//inline_fast(double)trunc		(double f)				{ return copysign(floor(abs(f)), f); }
//inline_fast(double)round		(double f)				{ return copysign(floor(abs(f) + 0.5), f); }
inline_fast(double)	frac		(double f)				{ return f - trunc(f); }

inline_fast(void)	sincos(double x, double *s, double *c)	{ *s = sin(x); *c = cos(x); }

static const double	ln2 = 0.693147180559945309417232121458;
inline_fast(double)	exp2		(double f)				{ return exp(f * ln2); }
inline_fast(double)	nlog2		(double n, double f)	{ return log(f) * n / ln2; }
inline_fast(double)	log2		(double f)				{ return log(f) / ln2; }
inline_fast(double)	ln			(double f)				{ return log(f); }

//-------------------------------------
// floats
//-------------------------------------

inline_fast(float)	abs			(float a)				{ return (float)fabs(a); }
inline_fast(float)	pow			(float f, float p)		{ return powf(f, p); }

#ifndef USING_VSMATH
inline_fast(float)	sqrt		(float f)				{ return sqrtf(f); }
inline_fast(float)	floor		(float f)				{ return floorf(f); }
inline_fast(float)	ceil		(float f)				{ return ceilf(f); }
inline_fast(float)	sin			(float f)				{ return sinf(f); }
inline_fast(float)	cos			(float f)				{ return cosf(f); }
inline_fast(float)	tan			(float f)				{ return tanf(f); }
inline_fast(float)	atan		(float f)				{ return atanf(f); }
inline_fast(float)	atan2		(float s, float c)		{ return atan2f(s, c); }
inline_fast(float)	asin		(float f)				{ return asinf(f); }
inline_fast(float)	acos		(float f)				{ return acosf(f); }
inline_fast(float)	exp			(float f)				{ return expf(f); }
inline_fast(float)	log			(float f)				{ return logf(f); }
inline_fast(float)	log10		(float f)				{ return log10f(f); }
#endif

inline_fast(float)	copysign	(float a, float b)		{ return iorf(a).set_sign(iorf(b).get_sign()).f(); }
inline_fast(float)	sign1		(float f)				{ return copysign(1.f, f); }
inline_fast(float)	sign		(float f)				{ return copysign(float(f!=0), f); }

inline_fast(float)	fsel		(float x, float y, float z)	{ return x >= 0 ? y : z;}
inline_fast(float)	mod			(float n, float d)		{ return fmodf(n, d); }
inline_fast(float)	reciprocal	(float f)				{ return 1 / f; }
inline_fast(float)	rsqrt		(float f)				{ return 1 / sqrt(f); }

inline_fast(float)	trunc		(float f)				{ return iorf(f).trunc().f(); }
inline_fast(float)	round		(float f)				{ return iorf(f).round_ne().f(); }
//inline_fast(float)trunc		(float f)				{ return copysign(floor(abs(f)), f); }
//inline_fast(float)round		(float f)				{ return copysign(floor(abs(f) + 0.5f), f); }
inline_fast(float)	frac		(float f)				{ return f - trunc(f); }

inline_fast(void)	sincos(float x, float *s, float *c)	{ *s = sin(x); *c = cos(x); }

inline_fast(float)	exp2		(float f)				{ return exp(f * float(ln2)); }
inline_fast(float)	nlog2		(float n, float f)		{ return log(f) * n / float(ln2); }
inline_fast(float)	log2		(float f)				{ return log(f) / float(ln2); }
inline_fast(float)	ln			(float f)				{ return log(f); }

}//namespace iso

#endif // _MATHS_H