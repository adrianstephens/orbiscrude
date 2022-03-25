#ifndef _MATHS_H
#define _MATHS_H

#include "base/defs.h"

#undef min
#undef max

#define FLT_NAN			force_cast<float>(iorf::nan())

namespace iso {

static const double	ln2 = 0.693147180559945309417232121458;

//-------------------------------------
// doubles
//-------------------------------------

force_inline double	abs			(double a)				{ return __builtin_fabs(a); }
force_inline double	sqrt		(double a)				{ return __builtin_sqrt(a); }
force_inline double	floor		(double a)				{ return __builtin_floor(a); }
force_inline double	ceil		(double a)				{ return __builtin_ceil(a); }
force_inline double	sin			(double a)				{ return __builtin_sin(a); }
force_inline double	cos			(double a)				{ return __builtin_cos(a); }
force_inline double	tan			(double a)				{ return __builtin_tan(a); }
force_inline double	atan		(double a)				{ return __builtin_atan(a); }
force_inline double	asin		(double a)				{ return __builtin_asin(a); }
force_inline double	acos		(double a)				{ return __builtin_acos(a); }
force_inline double	exp			(double a)				{ return __builtin_exp(a); }
force_inline double	log			(double a)				{ return __builtin_log(a); }
force_inline double	log10		(double a)				{ return __builtin_log10(a); }
force_inline double	mod			(double a, double b)	{ return __builtin_fmod(a, b); }
force_inline double	atan2		(double a, double b)	{ return __builtin_atan2(a, b); }
force_inline double	pow			(double a, double b)	{ return __builtin_pow(a, b); }
	
force_inline double	copysign	(double a, double b)	{ return iord(a).set_sign(iord(b).get_sign()).f(); }
force_inline double	sign1		(double f)				{ return copysign(1.0, f);			}
force_inline double	sign		(double f)				{ return copysign(double(f!=0), f);	}

force_inline double	fsel		(double x, double y, double z) { return x >= 0 ? y : z;	}
force_inline double	reciprocal	(double f)				{ return 1 / f; }
force_inline double	rsqrt		(double f)				{ return 1 / sqrt(f); }

force_inline double	trunc		(double f)				{ return iord(f).trunc().f(); }
force_inline double	round		(double f)				{ return iord(f).round_ne().f(); }
force_inline double	frac		(double f)				{ return f - trunc(f); }

force_inline void	sincos(double x, double *s, double *c)	{ *s = sin(x); *c = cos(x); }

force_inline double	exp2		(double f)				{ return exp(f * ln2); }
force_inline double	nlog2		(double n, double f)	{ return log(f) * n / ln2; }
force_inline double	log2		(double f)				{ return log(f) / ln2; }
force_inline double	ln			(double f)				{ return log(f); }

//-------------------------------------
// floats
//-------------------------------------

force_inline float	abs			(float a)				{ return __builtin_fabsf(a); }
force_inline float	sqrt		(float a)				{ return __builtin_sqrtf(a); }
force_inline float	floor		(float a)				{ return __builtin_floorf(a); }
force_inline float	ceil		(float a)				{ return __builtin_ceilf(a); }
force_inline float	sin			(float a)				{ return __builtin_sinf(a); }
force_inline float	cos			(float a)				{ return __builtin_cosf(a); }
force_inline float	tan			(float a)				{ return __builtin_tanf(a); }
force_inline float	atan		(float a)				{ return __builtin_atanf(a); }
force_inline float	asin		(float a)				{ return __builtin_asinf(a); }
force_inline float	acos		(float a)				{ return __builtin_acosf(a); }
force_inline float	exp			(float a)				{ return __builtin_expf(a); }
force_inline float	log			(float a)				{ return __builtin_logf(a); }
force_inline float	log10		(float a)				{ return __builtin_log10f(a); }
force_inline float	mod			(float a, float b)		{ return __builtin_fmodf(a, b); }
force_inline float	atan2		(float a, float b)		{ return __builtin_atan2f(a, b); }
force_inline float	pow			(float a, float b)		{ return __builtin_powf(a, b); }

force_inline float	copysign	(float a, float b)		{ return iorf(a).set_sign(iorf(b).get_sign()).f(); }
force_inline float	sign1		(float f)				{ return copysign(1.f, f);			}
force_inline float	sign		(float f)				{ return copysign(float(f!=0), f);	}

force_inline float	fsel		(float x, float y, float z)	{ return x >= 0 ? y : z;}
force_inline float	reciprocal	(float f)				{ return 1 / f;				}
force_inline float	rsqrt		(float f)				{ return 1 / sqrt(f);		}

force_inline float	trunc		(float f)				{ return iorf(f).trunc().f(); }
force_inline float	round		(float f)				{ return iorf(f).round_ne().f(); }
//force_inline float)trunc		(float f)				{ return copysign(floor(abs(f)), f);		}
//force_inline float)round		(float f)				{ return copysign(floor(abs(f) + 0.5f), f);	}
force_inline float	frac		(float f)				{ return f - trunc(f);		}

force_inline void	sincos(float x, float *s, float *c)	{ *s = sin(x); *c = cos(x); }

force_inline float	exp2		(float f)				{ return exp(f * float(ln2)); }
force_inline float	nlog2		(float n, float f)		{ return log(f) * n / float(ln2); }
force_inline float	log2		(float f)				{ return log(f) / float(ln2); }
force_inline float	ln			(float f)				{ return log(f); }

}//namespace iso

#endif
