#include <float.h>

#undef min
#undef max

//#define inline_fast(R)	__forceinline R __fastcall
#define inline_fast(R)	__forceinline R
#define FLT_NAN			iorf(uint32(0x7fffffff)).f

namespace iso {

//-----------------------------------------------------------------------------
//		arithmetic
//-----------------------------------------------------------------------------
struct x87 {
	enum {
		PREC_MASK		= 3<<8,
		PREC_24			= 0<<8,
		PREC_53			= 2<<8,
		PREC_64			= 3<<8,
		ROUND_MASK		= 3<<10,
		ROUND_NEAREST	= 0<<10,
		ROUND_DOWN		= 1<<10,
		ROUND_UP		= 2<<10,
		ROUND_CHOP		= 3<<10,
	};
	uint16	save_cw;
	inline_fast(void)	save()					{ uint16 t; __asm {fstcw t} save_cw = t;		}
	inline_fast(void)	set(uint32 v, uint32 m) { uint16 t = (save_cw & ~m) | v; __asm fldcw t }
	static inline_fast(float)	round(float f)	{ __asm fld f __asm frndint }
	static inline_fast(double)	round(double f)	{ __asm fld f __asm frndint }

	force_inline x87()							{ save(); }
	force_inline x87(uint32 v, uint32 m)			{ save(); set(v, m);	}
	force_inline ~x87()							{ uint32 t = save_cw; __asm fldcw t	}
};

//#define FPU_RESULT
#define FPU_RESULT	float r; __asm {fstp r} return r;
#define FPU_RESULTD	double r; __asm {fstp r} return r;

//-------------------------------------
// doubles
//-------------------------------------

inline_fast(double)	fsel		(double x, double y, double z) { return x >= 0? y : z;	}
inline_fast(double)	mod			(double n, double d)	{ {__asm fld d __asm fld n __asm fprem}		FPU_RESULTD }
inline_fast(double)	reciprocal	(double f)				{ return 1.0 / f; }
inline_fast(double)	abs			(double f)				{ {__asm fld f __asm fabs}					FPU_RESULTD }
inline_fast(double)	sqrt		(double f)				{ {__asm fld f __asm fsqrt}					FPU_RESULTD }

inline_fast(double)	floor		(double f)				{ return x87(x87::ROUND_DOWN,	x87::ROUND_MASK).round(f);	}
inline_fast(double)	ceil		(double f)				{ return x87(x87::ROUND_UP,		x87::ROUND_MASK).round(f);	}
inline_fast(double)	trunc		(double f)				{ return x87(x87::ROUND_CHOP,	x87::ROUND_MASK).round(f);	}
inline_fast(double)	round		(double f)				{ return x87::round(f); }
inline_fast(double)	frac		(double f)				{ return f - trunc(f);	}
inline_fast(double)	copysign	(double a, double b)	{ return iord(a).set_sign(iord(b).get_sign()).f; }
inline_fast(double)	sign1		(double f)				{ return copysign(1.0, f);		}

inline_fast(double)	sin			(double f)				{ {__asm fld f __asm fsin}					FPU_RESULTD }
inline_fast(double)	cos			(double f)				{ {__asm fld f __asm fcos}					FPU_RESULTD }
inline_fast(double)	tan			(double f)				{ {__asm fld f __asm fptan __asm fmul}		FPU_RESULTD }
inline_fast(double)	atan		(double f)				{ {__asm fld f __asm fld1 __asm fpatan}		FPU_RESULTD }
inline_fast(double)	atan2		(double s, double c)	{ {__asm fld s __asm fld c __asm fpatan}	FPU_RESULTD }
inline_fast(double)	asin		(double f)				{ return atan2(f, sqrt(1 - f * f));		}
inline_fast(double)	acos		(double f)				{ return atan2(sqrt(1 - f * f), f);		}

inline_fast(void)	sincos(double x, double *s, double *c)	{ double _s, _c; {__asm fld x __asm fsincos __asm fstp _c __asm fstp _s} *s = _s; *c = _c; }

inline_fast(double) exp2(double f) {
	x87	x(x87::ROUND_CHOP,	x87::ROUND_MASK);
	double	dummy;
	__asm {
		fld		f
		fld		st(0)			;Duplicate tos.
		frndint					;Compute integer portion.
		fxch					;Swap whole and int values.
		fsub	st(0), st(1)    ;Compute fractional part.

		f2xm1
		fld1
		fadd
		fscale
		fxch					;Swap result and int values.
		fstp	dummy			;get rid of unwanted int
	}
	FPU_RESULTD
}
inline_fast(double)	exp(double f) {
	x87	x(x87::ROUND_CHOP,	x87::ROUND_MASK);
	double	dummy;
	__asm {
		fldl2e
		fld		f
		fmul
		fld		st(0)			;Duplicate tos.
		frndint					;Compute integer portion.
		fxch					;Swap whole and int values.
		fsub	st(0), st(1)    ;Compute fractional part.

		f2xm1
		fld1
		fadd
		fscale
		fxch					;Swap result and int values.
		fstp	dummy			;get rid of unwanted int
	}
	FPU_RESULTD
}
inline_fast(double)	nlog2(double n, double f)	{ {__asm fld n __asm fld f __asm fyl2x}		FPU_RESULTD }
inline_fast(double)	log2(double f)				{ {__asm fld1 __asm fld f __asm fyl2x}		FPU_RESULTD }
inline_fast(double)	ln(double f)				{ {__asm fldl2e __asm fld f __asm fyl2x}	FPU_RESULTD }
inline_fast(double)	log10(double f)				{ {__asm fldl2t __asm fld f __asm fyl2x}	FPU_RESULTD }
inline_fast(double)	pow(double f, double p)		{ return exp2(nlog2(p, f));	}

//-------------------------------------
// floats
//-------------------------------------

inline_fast(float)	fsel		(float x, float y, float z)	{ return x >= 0 ? y : z;	}
inline_fast(float)	mod			(float n, float d)		{ {__asm fld d __asm fld n __asm fprem}		FPU_RESULT }
inline_fast(float)	reciprocal	(float f)				{ return 1.0f / f;			}
inline_fast(float)	abs			(float f)				{ {__asm fld f __asm fabs}					FPU_RESULT }
inline_fast(float)	sqrt		(float f)				{ {__asm fld f __asm fsqrt}					FPU_RESULT }
inline_fast(float)	rsqrt		(float f)				{ return 1 / sqrt(f);		}

inline_fast(float)	floor		(float f)				{ return x87(x87::ROUND_DOWN,	x87::ROUND_MASK).round(f);	}
inline_fast(float)	ceil		(float f)				{ return x87(x87::ROUND_UP,		x87::ROUND_MASK).round(f);	}
inline_fast(float)	trunc		(float f)				{ return x87(x87::ROUND_CHOP,	x87::ROUND_MASK).round(f);	}
inline_fast(float)	round		(float f)				{ return x87::round(f);		}
inline_fast(float)	frac		(float f)				{ return f - trunc(f);		}
inline_fast(float)	copysign	(float a, float b)		{ return iorf(a).set_sign(iorf(b).get_sign()).f; }
inline_fast(float)	sign1		(float f)				{ return copysign(1.f, f);	}

inline_fast(float)	sin			(float f)				{ {__asm fld f __asm fsin}					FPU_RESULT }
inline_fast(float)	cos			(float f)				{ {__asm fld f __asm fcos}					FPU_RESULT }
inline_fast(float)	tan			(float f)				{ {__asm fld f __asm fptan __asm fmul}		FPU_RESULT }
inline_fast(float)	atan		(float f)				{ {__asm fld f __asm fld1 __asm fpatan}		FPU_RESULT }
inline_fast(float)	atan2		(float s, float c)		{ {__asm fld s __asm fld c __asm fpatan}	FPU_RESULT }
inline_fast(float)	asin		(float f)				{ return atan2(f, sqrt(1 - f * f));		}
inline_fast(float)	acos		(float f)				{ return atan2(sqrt(1 - f * f), f);		}
inline_fast(void)	sincos(float x, float *s, float *c)	{ float _s, _c; {__asm fld x __asm fsincos __asm fstp _c __asm fstp _s} *s = _s; *c = _c; }

inline_fast(float)	exp2(float f) {
	x87	x(x87::ROUND_CHOP,	x87::ROUND_MASK);
	float	dummy;
	__asm {
		fld		f
		fld		st(0)			;Duplicate tos.
		frndint					;Compute integer portion.
		fxch					;Swap whole and int values.
		fsub	st(0), st(1)    ;Compute fractional part.

		f2xm1
		fld1
		fadd
		fscale
		fxch					;Swap result and int values.
		fstp	dummy			;get rid of unwanted int
	}
	FPU_RESULT
}
inline_fast(float)	exp(float f) {
	x87	x(x87::ROUND_CHOP,	x87::ROUND_MASK);
	double	dummy;
	__asm {
		fldl2e
		fld		f
		fmul
		fld		st(0)			;Duplicate tos.
		frndint					;Compute integer portion.
		fxch					;Swap whole and int values.
		fsub	st(0), st(1)    ;Compute fractional part.

		f2xm1
		fld1
		fadd
		fscale
		fxch					;Swap result and int values.
		fstp	dummy			;get rid of unwanted int
	}
	FPU_RESULT
}
inline_fast(float)	nlog2(float n, float f)	{ {__asm fld n __asm fld f __asm fyl2x}		FPU_RESULT }
inline_fast(float)	log2(float f)			{ {__asm fld1 __asm fld f __asm fyl2x}		FPU_RESULT }
inline_fast(float)	ln(float f)				{ {__asm fldln2 __asm fld f __asm fyl2x}	FPU_RESULT }
inline_fast(float)	log10(float f)			{ {__asm fldlg2 __asm fld f __asm fyl2x}	FPU_RESULT }
inline_fast(float)	pow(float f, float p)	{ return exp2(nlog2(p, f)); }

}//namespace iso
