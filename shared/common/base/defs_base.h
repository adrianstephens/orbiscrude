#undef min
#undef max

#ifndef DEFS_BASE_H
#define DEFS_BASE_H

#include "platform.h"

#ifdef __clang__
#define PLAT_CLANG
#elif defined _MSC_VER
#define PLAT_MSVC
#endif

#ifndef ISO_PREFIX
#define ISO_PREFIX	ISO_PLATFORM
#endif

#ifndef iso_export
#define iso_export
#endif
#ifndef iso_local
#define iso_local
#endif

#ifndef CTOR_RETURN
#define CTOR_RETURN
#endif

#ifndef DECL_ALLOCATOR
#define DECL_ALLOCATOR
#endif

#ifdef USE_EXCEPTIONS
#define rethrow			throw
#define iso_throw(x)	throw x
#define catch_all()		catch(...)
#else
#define try				if (true)
#define catch(x)		else if (x=0)
#define catch_all()		else if (0)
#define rethrow
#define iso_throw(x)	iso::unused(x)
#endif

#define likely(expr)		probably(true,expr)
#define unlikely(expr)		probably(false,expr)
#define iso_offset(S,M)		(size_t(&(((S*)1)->M)) - 1)
#define sizeof32(T)			unsigned(sizeof(T))

#ifndef nodebug_inline
#ifdef PLAT_CLANG
#define nodebug_inline		inline __attribute__((__nodebug__))
#else
#define nodebug_inline		force_inline
#endif
#endif

#ifdef USE_RVALUE_REFS
#define CRREF(T)	T&&
#define RREF(T)		T&&
#else
#define CRREF(T)	const T&
#define RREF(T)		T&
#endif

//-----------------------------------------------------------------------------
//	macro helpers
//-----------------------------------------------------------------------------

#define COMMA				,
#define CONCAT(x, y)		x##y
#define CONCAT2(x, y)		CONCAT(x, y)
#define CONCAT3(x, y)		CONCAT2(x, y)
#define STRINGIFY(c)		#c
#define STRINGIFY2(c)		STRINGIFY(c)
#define LSTRINGIFY(c)		L""#c
#define EXPAND(X)			X


#define NO_PARENTHESES(...) __VA_ARGS__

#define DEPAREN(X)	ESC(ISH X)
#define ISH(...)	ISH __VA_ARGS__
#define ESC(...)	ESC_(__VA_ARGS__)
#define ESC_(...)	VAN ## __VA_ARGS__
#define VANISH

#define VA_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,N,...) N

#define VA_HEAD_(_0,...)	_0
#define VA_TAIL_0(...)
#define VA_TAIL_1(_0,...)	,__VA_ARGS__
#define VA_REST_(X,...)		__VA_ARGS__

//#ifdef PLAT_MSVC
#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL	// __VA_ARGS__ is always interpreted as a single parameter!
#define VA_N_(tuple)		VA_N tuple
#define VA_HEAD__(tuple)	VA_HEAD_ tuple
#define VA_TAIL__0(tuple)	VA_TAIL_0 tuple
#define VA_TAIL__1(tuple)	VA_TAIL_1 tuple
#define VA_REST__(tuple)	VA_REST_ tuple
#define VA_NUM(...)			VA_N_((__VA_ARGS__,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1))
#define VA_MORE(...)		VA_N_((__VA_ARGS__,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0))
#define VA_HEAD(...)		VA_HEAD__((__VA_ARGS__))
#define VA_TAIL(...)		CONCAT3(VA_TAIL__, VA_MORE(__VA_ARGS__))((__VA_ARGS__))
#define VA_REST(...)		VA_REST__((__VA_ARGS__))
#else
#define VA_NUM(...)			VA_N(__VA_ARGS__,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define VA_MORE(...)		VA_N(__VA_ARGS__,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0)
#define VA_HEAD(...)		VA_HEAD_(__VA_ARGS__,)
#define VA_TAIL(...)		CONCAT3(VA_TAIL_, VA_MORE(__VA_ARGS__))(__VA_ARGS__)
#endif

#define _VA1(M,X)			M(X)
#define _VA2(M,X, ...)		EXPAND(M(X)_VA1 (M,__VA_ARGS__))
#define _VA3(M,X, ...)		EXPAND(M(X)_VA2 (M,__VA_ARGS__))
#define _VA4(M,X, ...)		EXPAND(M(X)_VA3 (M,__VA_ARGS__))
#define _VA5(M,X, ...)		EXPAND(M(X)_VA4 (M,__VA_ARGS__))
#define _VA6(M,X, ...)		EXPAND(M(X)_VA5 (M,__VA_ARGS__))
#define _VA7(M,X, ...)		EXPAND(M(X)_VA6 (M,__VA_ARGS__))
#define _VA8(M,X, ...)		EXPAND(M(X)_VA7 (M,__VA_ARGS__))
#define _VA9(M,X, ...)		EXPAND(M(X)_VA8 (M,__VA_ARGS__))
#define _VA10(M,X, ...)		EXPAND(M(X)_VA9 (M,__VA_ARGS__))
#define _VA11(M,X, ...)		EXPAND(M(X)_VA10(M,__VA_ARGS__))
#define _VA12(M,X, ...)		EXPAND(M(X)_VA11(M,__VA_ARGS__))
#define _VA13(M,X, ...)		EXPAND(M(X)_VA12(M,__VA_ARGS__))
#define _VA14(M,X, ...)		EXPAND(M(X)_VA13(M,__VA_ARGS__))
#define _VA15(M,X, ...)		EXPAND(M(X)_VA14(M,__VA_ARGS__))
#define _VA16(M,X, ...)		EXPAND(M(X)_VA15(M,__VA_ARGS__))
#define _VA17(M,X, ...)		EXPAND(M(X)_VA16(M,__VA_ARGS__))
#define _VA18(M,X, ...)		EXPAND(M(X)_VA17(M,__VA_ARGS__))
#define _VA19(M,X, ...)		EXPAND(M(X)_VA18(M,__VA_ARGS__))
#define _VA20(M,X, ...)		EXPAND(M(X)_VA19(M,__VA_ARGS__))
#define _VA21(M,X, ...)		EXPAND(M(X)_VA20(M,__VA_ARGS__))
#define _VA22(M,X, ...)		EXPAND(M(X)_VA21(M,__VA_ARGS__))
#define _VA23(M,X, ...)		EXPAND(M(X)_VA22(M,__VA_ARGS__))
#define _VA24(M,X, ...)		EXPAND(M(X)_VA23(M,__VA_ARGS__))
#define _VA25(M,X, ...)		EXPAND(M(X)_VA24(M,__VA_ARGS__))
#define _VA26(M,X, ...)		EXPAND(M(X)_VA25(M,__VA_ARGS__))
#define _VA27(M,X, ...)		EXPAND(M(X)_VA26(M,__VA_ARGS__))
#define _VA28(M,X, ...)		EXPAND(M(X)_VA27(M,__VA_ARGS__))
#define _VA29(M,X, ...)		EXPAND(M(X)_VA28(M,__VA_ARGS__))
#define _VA30(M,X, ...)		EXPAND(M(X)_VA29(M,__VA_ARGS__))
#define _VA31(M,X, ...)		EXPAND(M(X)_VA30(M,__VA_ARGS__))
#define _VA32(M,X, ...)		EXPAND(M(X)_VA31(M,__VA_ARGS__))
#define _VA33(M,X, ...)		EXPAND(M(X)_VA32(M,__VA_ARGS__))
#define _VA34(M,X, ...)		EXPAND(M(X)_VA33(M,__VA_ARGS__))
#define _VA35(M,X, ...)		EXPAND(M(X)_VA34(M,__VA_ARGS__))
#define _VA36(M,X, ...)		EXPAND(M(X)_VA35(M,__VA_ARGS__))
#define _VA37(M,X, ...)		EXPAND(M(X)_VA36(M,__VA_ARGS__))
#define _VA38(M,X, ...)		EXPAND(M(X)_VA37(M,__VA_ARGS__))
#define _VA39(M,X, ...)		EXPAND(M(X)_VA38(M,__VA_ARGS__))
#define _VA40(M,X, ...)		EXPAND(M(X)_VA39(M,__VA_ARGS__))
#define _VA41(M,X, ...)		EXPAND(M(X)_VA40(M,__VA_ARGS__))
#define _VA42(M,X, ...)		EXPAND(M(X)_VA41(M,__VA_ARGS__))
#define _VA43(M,X, ...)		EXPAND(M(X)_VA42(M,__VA_ARGS__))
#define _VA44(M,X, ...)		EXPAND(M(X)_VA43(M,__VA_ARGS__))
#define _VA45(M,X, ...)		EXPAND(M(X)_VA44(M,__VA_ARGS__))
#define _VA46(M,X, ...)		EXPAND(M(X)_VA45(M,__VA_ARGS__))
#define _VA47(M,X, ...)		EXPAND(M(X)_VA46(M,__VA_ARGS__))
#define _VA48(M,X, ...)		EXPAND(M(X)_VA47(M,__VA_ARGS__))
#define _VA49(M,X, ...)		EXPAND(M(X)_VA48(M,__VA_ARGS__))
#define _VA50(M,X, ...)		EXPAND(M(X)_VA49(M,__VA_ARGS__))
#define _VA51(M,X, ...)		EXPAND(M(X)_VA50(M,__VA_ARGS__))
#define _VA52(M,X, ...)		EXPAND(M(X)_VA51(M,__VA_ARGS__))
#define _VA53(M,X, ...)		EXPAND(M(X)_VA52(M,__VA_ARGS__))
#define _VA54(M,X, ...)		EXPAND(M(X)_VA53(M,__VA_ARGS__))
#define _VA55(M,X, ...)		EXPAND(M(X)_VA54(M,__VA_ARGS__))
#define _VA56(M,X, ...)		EXPAND(M(X)_VA55(M,__VA_ARGS__))
#define _VA57(M,X, ...)		EXPAND(M(X)_VA56(M,__VA_ARGS__))
#define _VA58(M,X, ...)		EXPAND(M(X)_VA57(M,__VA_ARGS__))
#define _VA59(M,X, ...)		EXPAND(M(X)_VA58(M,__VA_ARGS__))
#define _VA60(M,X, ...)		EXPAND(M(X)_VA59(M,__VA_ARGS__))
#define _VA61(M,X, ...)		EXPAND(M(X)_VA60(M,__VA_ARGS__))
#define _VA62(M,X, ...)		EXPAND(M(X)_VA61(M,__VA_ARGS__))
#define _VA63(M,X, ...)		EXPAND(M(X)_VA62(M,__VA_ARGS__))
#define _VA64(M,X, ...)		EXPAND(M(X)_VA63(M,__VA_ARGS__))
#define VA_APPLY(M,...)		EXPAND(CONCAT3(_VA, VA_NUM(__VA_ARGS__))(M,__VA_ARGS__))

#define _VAP1(M, P,X)		M(P,X)
#define _VAP2(M, P,X, ...)	EXPAND(M(P,X)_VAP1 (M,P,__VA_ARGS__))
#define _VAP3(M, P,X, ...)	EXPAND(M(P,X)_VAP2 (M,P,__VA_ARGS__))
#define _VAP4(M, P,X, ...)	EXPAND(M(P,X)_VAP3 (M,P,__VA_ARGS__))
#define _VAP5(M, P,X, ...)	EXPAND(M(P,X)_VAP4 (M,P,__VA_ARGS__))
#define _VAP6(M, P,X, ...)	EXPAND(M(P,X)_VAP5 (M,P,__VA_ARGS__))
#define _VAP7(M, P,X, ...)	EXPAND(M(P,X)_VAP6 (M,P,__VA_ARGS__))
#define _VAP8(M, P,X, ...)	EXPAND(M(P,X)_VAP7 (M,P,__VA_ARGS__))
#define _VAP9(M, P,X, ...)	EXPAND(M(P,X)_VAP8 (M,P,__VA_ARGS__))
#define _VAP10(M,P,X, ...)	EXPAND(M(P,X)_VAP9 (M,P,__VA_ARGS__))
#define _VAP11(M,P,X, ...)	EXPAND(M(P,X)_VAP10(M,P,__VA_ARGS__))
#define _VAP12(M,P,X, ...)	EXPAND(M(P,X)_VAP11(M,P,__VA_ARGS__))
#define _VAP13(M,P,X, ...)	EXPAND(M(P,X)_VAP12(M,P,__VA_ARGS__))
#define _VAP14(M,P,X, ...)	EXPAND(M(P,X)_VAP13(M,P,__VA_ARGS__))
#define _VAP15(M,P,X, ...)	EXPAND(M(P,X)_VAP14(M,P,__VA_ARGS__))
#define _VAP16(M,P,X, ...)	EXPAND(M(P,X)_VAP15(M,P,__VA_ARGS__))
#define _VAP17(M,P,X, ...)	EXPAND(M(P,X)_VAP16(M,P,__VA_ARGS__))
#define _VAP18(M,P,X, ...)	EXPAND(M(P,X)_VAP17(M,P,__VA_ARGS__))
#define _VAP19(M,P,X, ...)	EXPAND(M(P,X)_VAP18(M,P,__VA_ARGS__))
#define _VAP20(M,P,X, ...)	EXPAND(M(P,X)_VAP19(M,P,__VA_ARGS__))
#define _VAP21(M,P,X, ...)	EXPAND(M(P,X)_VAP20(M,P,__VA_ARGS__))
#define _VAP22(M,P,X, ...)	EXPAND(M(P,X)_VAP21(M,P,__VA_ARGS__))
#define _VAP23(M,P,X, ...)	EXPAND(M(P,X)_VAP22(M,P,__VA_ARGS__))
#define _VAP24(M,P,X, ...)	EXPAND(M(P,X)_VAP23(M,P,__VA_ARGS__))
#define _VAP25(M,P,X, ...)	EXPAND(M(P,X)_VAP24(M,P,__VA_ARGS__))
#define _VAP26(M,P,X, ...)	EXPAND(M(P,X)_VAP25(M,P,__VA_ARGS__))
#define _VAP27(M,P,X, ...)	EXPAND(M(P,X)_VAP26(M,P,__VA_ARGS__))
#define _VAP28(M,P,X, ...)	EXPAND(M(P,X)_VAP27(M,P,__VA_ARGS__))
#define _VAP29(M,P,X, ...)	EXPAND(M(P,X)_VAP28(M,P,__VA_ARGS__))
#define _VAP30(M,P,X, ...)	EXPAND(M(P,X)_VAP29(M,P,__VA_ARGS__))
#define _VAP31(M,P,X, ...)	EXPAND(M(P,X)_VAP30(M,P,__VA_ARGS__))
#define _VAP32(M,P,X, ...)	EXPAND(M(P,X)_VAP31(M,P,__VA_ARGS__))
#define _VAP33(M,P,X, ...)	EXPAND(M(P,X)_VAP32(M,P,__VA_ARGS__))
#define _VAP34(M,P,X, ...)	EXPAND(M(P,X)_VAP33(M,P,__VA_ARGS__))
#define _VAP35(M,P,X, ...)	EXPAND(M(P,X)_VAP34(M,P,__VA_ARGS__))
#define _VAP36(M,P,X, ...)	EXPAND(M(P,X)_VAP35(M,P,__VA_ARGS__))
#define _VAP37(M,P,X, ...)	EXPAND(M(P,X)_VAP36(M,P,__VA_ARGS__))
#define _VAP38(M,P,X, ...)	EXPAND(M(P,X)_VAP37(M,P,__VA_ARGS__))
#define _VAP39(M,P,X, ...)	EXPAND(M(P,X)_VAP38(M,P,__VA_ARGS__))
#define _VAP40(M,P,X, ...)	EXPAND(M(P,X)_VAP39(M,P,__VA_ARGS__))
#define _VAP41(M,P,X, ...)	EXPAND(M(P,X)_VAP40(M,P,__VA_ARGS__))
#define _VAP42(M,P,X, ...)	EXPAND(M(P,X)_VAP41(M,P,__VA_ARGS__))
#define _VAP43(M,P,X, ...)	EXPAND(M(P,X)_VAP42(M,P,__VA_ARGS__))
#define _VAP44(M,P,X, ...)	EXPAND(M(P,X)_VAP43(M,P,__VA_ARGS__))
#define _VAP45(M,P,X, ...)	EXPAND(M(P,X)_VAP44(M,P,__VA_ARGS__))
#define _VAP46(M,P,X, ...)	EXPAND(M(P,X)_VAP45(M,P,__VA_ARGS__))
#define _VAP47(M,P,X, ...)	EXPAND(M(P,X)_VAP46(M,P,__VA_ARGS__))
#define _VAP48(M,P,X, ...)	EXPAND(M(P,X)_VAP47(M,P,__VA_ARGS__))
#define _VAP49(M,P,X, ...)	EXPAND(M(P,X)_VAP48(M,P,__VA_ARGS__))
#define _VAP50(M,P,X, ...)	EXPAND(M(P,X)_VAP49(M,P,__VA_ARGS__))
#define _VAP51(M,P,X, ...)	EXPAND(M(P,X)_VAP50(M,P,__VA_ARGS__))
#define _VAP52(M,P,X, ...)	EXPAND(M(P,X)_VAP51(M,P,__VA_ARGS__))
#define _VAP53(M,P,X, ...)	EXPAND(M(P,X)_VAP52(M,P,__VA_ARGS__))
#define _VAP54(M,P,X, ...)	EXPAND(M(P,X)_VAP53(M,P,__VA_ARGS__))
#define _VAP55(M,P,X, ...)	EXPAND(M(P,X)_VAP54(M,P,__VA_ARGS__))
#define _VAP56(M,P,X, ...)	EXPAND(M(P,X)_VAP55(M,P,__VA_ARGS__))
#define _VAP57(M,P,X, ...)	EXPAND(M(P,X)_VAP56(M,P,__VA_ARGS__))
#define _VAP58(M,P,X, ...)	EXPAND(M(P,X)_VAP57(M,P,__VA_ARGS__))
#define _VAP59(M,P,X, ...)	EXPAND(M(P,X)_VAP58(M,P,__VA_ARGS__))
#define _VAP60(M,P,X, ...)	EXPAND(M(P,X)_VAP59(M,P,__VA_ARGS__))
#define _VAP61(M,P,X, ...)	EXPAND(M(P,X)_VAP60(M,P,__VA_ARGS__))
#define _VAP62(M,P,X, ...)	EXPAND(M(P,X)_VAP61(M,P,__VA_ARGS__))
#define _VAP63(M,P,X, ...)	EXPAND(M(P,X)_VAP62(M,P,__VA_ARGS__))
#define _VAP64(M,P,X, ...)	EXPAND(M(P,X)_VAP63(M,P,__VA_ARGS__))

#define VA_APPLYP(M,P,...)	EXPAND(CONCAT3(_VAP, VA_NUM(__VA_ARGS__))(M,P,__VA_ARGS__))

template<typename>		struct RemoveParentheses;
template<typename T>	struct RemoveParentheses<void(T)>	{ typedef T type; };
template<>				struct RemoveParentheses<void()>	{ typedef void type; };
#define REMOVE_PARENTHESES(X) RemoveParentheses<void(X)>::type

//-----------------------------------------------------------------------------
//	bitfield helpers
//-----------------------------------------------------------------------------

#ifdef	ISO_BIGENDIAN
#define iso_bigendian	true
#define ISO_BIT(n,b)	(1 << ((((n) - 1 - (b)) & ~7) + ((b) & 7)))
#define ISO_BITFIELDS2(a,b)									a,b
#define ISO_BITFIELDS3(a,b,c)								a,b,c
#define ISO_BITFIELDS4(a,b,c,d)								a,b,c,d
#define ISO_BITFIELDS5(a,b,c,d,e)							a,b,c,d,e
#define ISO_BITFIELDS6(a,b,c,d,e,f)							a,b,c,d,e,f
#define ISO_BITFIELDS7(a,b,c,d,e,f,g)						a,b,c,d,e,f,g
#define ISO_BITFIELDS8(a,b,c,d,e,f,g,h)						a,b,c,d,e,f,g,h
#define ISO_BITFIELDS9(a,b,c,d,e,f,g,h,i)					a,b,c,d,e,f,g,h,i
#define ISO_BITFIELDS10(a,b,c,d,e,f,g,h,i,j)				a,b,c,d,e,f,g,h,i,j
#define ISO_BITFIELDS11(a,b,c,d,e,f,g,h,i,j,k)				a,b,c,d,e,f,g,h,i,j,k
#define ISO_BITFIELDS12(a,b,c,d,e,f,g,h,i,j,k,l)			a,b,c,d,e,f,g,h,i,j,k,l
#define ISO_BITFIELDS13(a,b,c,d,e,f,g,h,i,j,k,l,m)			a,b,c,d,e,f,g,h,i,j,k,l,m
#define ISO_BITFIELDS14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)		a,b,c,d,e,f,g,h,i,j,k,l,m,n
#define ISO_BITFIELDS15(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o)		a,b,c,d,e,f,g,h,i,j,k,l,m,n,o
#define ISO_BITFIELDS16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p
#else
#define iso_bigendian	false
#define ISO_BIT(n,b)	(1 << (b))
#define ISO_BITFIELDS2(a,b)									b,a
#define ISO_BITFIELDS3(a,b,c)								c,b,a
#define ISO_BITFIELDS4(a,b,c,d)								d,c,b,a
#define ISO_BITFIELDS5(a,b,c,d,e)							e,d,c,b,a
#define ISO_BITFIELDS6(a,b,c,d,e,f)							f,e,d,c,b,a
#define ISO_BITFIELDS7(a,b,c,d,e,f,g)						g,f,e,d,c,b,a
#define ISO_BITFIELDS8(a,b,c,d,e,f,g,h)						h,g,f,e,d,c,b,a
#define ISO_BITFIELDS9(a,b,c,d,e,f,g,h,i)					i,h,g,f,e,d,c,b,a
#define ISO_BITFIELDS10(a,b,c,d,e,f,g,h,i,j)				j,i,h,g,f,e,d,c,b,a
#define ISO_BITFIELDS11(a,b,c,d,e,f,g,h,i,j,k)				k,j,i,h,g,f,e,d,c,b,a
#define ISO_BITFIELDS12(a,b,c,d,e,f,g,h,i,j,k,l)			l,k,j,i,h,g,f,e,d,c,b,a
#define ISO_BITFIELDS13(a,b,c,d,e,f,g,h,i,j,k,l,m)			m,l,k,j,i,h,g,f,e,d,c,b,a
#define ISO_BITFIELDS14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)		n,m,l,k,j,i,h,g,f,e,d,c,b,a
#define ISO_BITFIELDS15(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o)		o,n,m,l,k,j,i,h,g,f,e,d,c,b,a
#define ISO_BITFIELDS16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	p,o,n,m,l,k,j,i,h,g,f,e,d,c,b,a
#endif

//-----------------------------------------------------------------------------
//	new/delete
//-----------------------------------------------------------------------------

struct placement {
	void	*p;
	constexpr placement(void *p) : p(p) {}
};
struct sized_placement {
	void	*p;
	size_t	size;
	constexpr sized_placement(void *p, size_t size) : p(p), size(size) {}
};

void	operator delete(void *p)					noexcept;
void	operator delete[](void *p)					noexcept;
void	operator delete(void *p, size_t size)		noexcept;
void	operator delete[](void *p, size_t size)		noexcept;

inline void*	operator new(size_t size, const placement &h)		{ return h.p; }
inline void		operator delete(void *p, const placement &h)		{}
inline void*	operator new(size_t size, const sized_placement &h)	{ /*ISO_ASSERT(size == h.size);*/ return h.p; }
inline void		operator delete(void *p, const sized_placement &h)	{}

#if 0
#ifndef __PLACEMENT_NEW_INLINE
force_inline void *operator new(size_t, void *p)	{ return p; }
force_inline void operator delete(void*, void*)		{}
#define __PLACEMENT_NEW_INLINE
#endif

#ifndef __PLACEMENT_VEC_NEW_INLINE
force_inline void *operator new[](size_t, void *p)	{ return p; }
force_inline void operator delete[](void*, void*)	{}
#define __PLACEMENT_VEC_NEW_INLINE
#endif
#endif

struct xyz;	//here so no namespace crud

namespace iso {
//-----------------------------------------------------------------------------
//	basic types
//-----------------------------------------------------------------------------

#ifndef USE_64BITREGS
#define USE_64BITREGS	1
#endif
#ifndef USE_LONG
#define USE_LONG		1
#endif
#ifndef USE_DOUBLE
#define USE_DOUBLE		1
#endif

#ifdef USE_SIGNEDCHAR
typedef	char				int8;
#else
typedef	signed char			int8;
#endif

typedef	signed short		int16;
typedef	int					int32;
typedef	__int64				int64;

typedef	unsigned char		uint8;
typedef	unsigned short		uint16;
typedef	unsigned int		uint32;
typedef	unsigned __int64	uint64;
typedef	unsigned long		ulong;

typedef float				float32;
typedef double				float64;

template<typename T> static constexpr uint32	BIT_COUNT	= sizeof(T) * 8;
static constexpr uint32 ISO_PTRBITS = BIT_COUNT<void*>;

struct _none {
	bool operator==(const _none&)	const	{ return true; }
	bool operator!=(const _none&)	const	{ return false; }
	bool operator< (const _none&)	const	{ return false; }
	bool operator> (const _none&)	const	{ return false; }
	bool operator<=(const _none&)	const	{ return true; }
	bool operator>=(const _none&)	const	{ return true; }
};
extern _none	none, terminate, empty;

struct T_false	{ static const bool value = false; };
struct T_true	{ static const bool value = true; };

template<typename T>							struct T_type 				{ typedef T	type; };
template<bool b, typename T, typename F>		struct T_if					: T_type<F> {};
template<typename T, typename F>				struct T_if<true, T, F>		: T_type<T> {};
template<bool b, typename T, typename F>		using if_t					= typename T_if<b, T, F>::type;

template<typename T>							struct T_true_type : T_true, T_type<T> {};

template<typename A,typename B,typename R=A>	struct T_same				: T_false {};
template<typename A, typename R>				struct T_same<A,A,R>		: T_true_type<R> {};
template<typename A,typename B,typename R=A>	using same_t				= typename T_same<A, B, R>::type;
template<typename A, typename B, typename...T>	constexpr bool same_v		= T_same<A, B>::value && same_v<A, T...>;
template<typename A,typename B>					constexpr bool same_v<A,B>	= T_same<A, B>::value;

namespace meta {
//-----------------------------------------------------------------------------
//	meta programming templates
//-----------------------------------------------------------------------------

template<template<class> class M, typename T>			struct map					: M<T> {};
template<template<class> class M, typename T>			using map_t					= typename map<M, T>::type;

template<template<class> class M, typename T>			struct map<M, const T>		: T_type<const map_t<M, T>> {};
template<template<class> class M, typename T>			struct map<M, T*>			: M<map_t<M,T>*> {};
template<template<class> class M, typename T, int N>	struct map<M, T[N]>			: M<map_t<M,T>[N]> {};
template<template<class> class M, typename T, int N>	struct map<M, const T[N]>	: M<map_t<M,T>[N]> {};
template<template<class> class M>						struct map<M, _none>		: T_type<_none> {};

template<typename T, T N>					struct constant { static const T value = N; operator T() const { return N; } };
template<int i>								using num	= constant<int, i>;

template<bool b, int T, int F>				struct ifnum				: num<F>	{};
template<int T, int F>						struct ifnum<true, T, F>	: num<T>	{};

template<int A, int B>						static constexpr int max = A > B ? A : B;
template<int A, int B>						static constexpr int min = A < B ? A : B;
template<int A>								static constexpr int abs = A < 0 ? -A : A;
template<int A>								static constexpr bool is_pow2 = (A & (A - 1)) == 0;

//-----------------------------------------------------------------------------
//	value list	- list of values (of same type)
//	constant	- constant value
//-----------------------------------------------------------------------------

template<typename T, T... I>				struct value_list { enum {count = sizeof...(I)}; };

template<typename T, T I0, T... I>			using VL_tail = value_list<T, I...>;
template<typename T>						static constexpr auto VL_head = 0;
template<typename T, T I0, T... I>			static constexpr auto VL_head<value_list<T, I0, I...>> = I0;

#if 1
template<size_t N, typename T = int>		using make_value_sequence	= __make_integer_seq<value_list, T, N>;
#else
template<typename T, int N, T... I>			struct make_value_sequence_imp : make_val_list_imp<T, N - 1, N - 1, I...> {};
template<typename T, T... I>				struct make_value_sequence_imp<T, 0, I...> : T_type<value_list<T, I...>> {};
template<int N, typename T=int>				using make_value_sequence	= typename make_value_sequence_imp<T, N>::type;
#endif

template<size_t...II>						using index_list			= value_list<size_t, II...>;
template<size_t N>							using make_index_list		= make_value_sequence<N, size_t>;

template<typename I>						struct VL_element_imp;
template<typename T, T... I>				struct VL_element_imp<value_list<T, I...>> : T_type<T> {};
template<typename I>						using VL_element = typename VL_element_imp<I>::type;

template<size_t N, typename I>				struct VL_index;
template<size_t N, typename T, T I0, T... I>struct VL_index<N, value_list<T, I0, I...>>		: VL_index<N - 1, value_list<T, I...>> {};
template<typename T, T I0, T... I>			struct VL_index<0, value_list<T, I0, I...>>		: constant<T, I0> {};

//	concatenate list of values
template<typename...X>						struct	VL_concat_imp;
template<typename...X>						using	VL_concat = typename VL_concat_imp<X...>::type;
template<typename X0, typename...X>			struct	VL_concat_imp<X0, X...> : VL_concat_imp<X0, VL_concat<X...>> {};
template<typename T, T...I, T ...J>			struct	VL_concat_imp<value_list<T, I...>, value_list<T, J...>> : T_type<value_list<T, I..., J...>> {};

template<typename T, T...I, T ...J>			constexpr value_list<T, I..., J...> concat(value_list<T, I...>, value_list<T, J...>) { return {}; }

template<typename T, T N>					constexpr constant<T, -N> 				operator- (constant<T, N>) 					{ return {}; }
template<typename T, T N>					constexpr constant<T, ~N> 				operator~ (constant<T, N>) 					{ return {}; }
template<typename T, T N>					constexpr constant<T, !N> 				operator! (constant<T, N>) 					{ return {}; }
//template<typename T, T N>					constexpr constant<T, N < 0 ? - N : N>	abs(constant<T, N>) 						{ return {}; }

template<typename T, T A, T B>				constexpr constant<T, (A +  B)> 		operator+ (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A -  B)> 		operator- (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A *  B)> 		operator* (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A /  B)> 		operator/ (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A %  B)> 		operator% (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A &  B)> 		operator& (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A |  B)> 		operator| (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A ^  B)> 		operator^ (constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A << B)> 		operator<<(constant<T, A>, constant<T, B>) 	{ return {}; }
template<typename T, T A, T B>				constexpr constant<T, (A >> B)> 		operator>>(constant<T, A>, constant<T, B>) 	{ return {}; }
//template<typename T, T A, T B>				constexpr constant<T, A > B ? A : B>	max(constant<T, A>, constant<T, B>) 		{ return {}; }
//template<typename T, T A, T B>				constexpr constant<T, A < B ? A : B>	min(constant<T, A>, constant<T, B>) 		{ return {}; }

template<typename T, T...I>					constexpr value_list<T, -I...> 			operator- (value_list<T, I...>) 			{ return {}; }
template<typename T, T...I>					constexpr value_list<T, ~I...> 			operator~ (value_list<T, I...>) 			{ return {}; }
template<typename T, T...I>					constexpr value_list<T, !I...> 			operator! (value_list<T, I...>) 			{ return {}; }

template<typename T, T...A, T...B>			constexpr value_list<T, (A +  B)...> 	operator+ (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A -  B)...> 	operator- (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A *  B)...> 	operator* (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A /  B)...> 	operator/ (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A %  B)...> 	operator% (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A &  B)...> 	operator& (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A |  B)...> 	operator| (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A ^  B)...> 	operator^ (value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A << B)...> 	operator<<(value_list<T, A...>, value_list<T, B...>) { return {}; }
template<typename T, T...A, T...B>			constexpr value_list<T, (A >> B)...> 	operator>>(value_list<T, A...>, value_list<T, B...>) { return {}; }
//template<typename T, T...A, T...B>			constexpr value_list<T, (A > B ? A : B)...>	max(value_list<T, A...>, value_list<T, B...>) 	{ return {}; }
//template<typename T, T...A, T...B>			constexpr value_list<T, (A < B ? A : B)...>	min(value_list<T, A...>, value_list<T, B...>) 	{ return {}; }

template<typename T, T...A, T B>			constexpr value_list<T, (A +  B)...> 	operator+ (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A -  B)...> 	operator- (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A *  B)...> 	operator* (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A /  B)...> 	operator/ (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A %  B)...> 	operator% (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A &  B)...> 	operator& (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A |  B)...> 	operator| (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A ^  B)...> 	operator^ (value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A << B)...> 	operator<<(value_list<T, A...>, constant<T, B>) { return {}; }
template<typename T, T...A, T B>			constexpr value_list<T, (A >> B)...> 	operator>>(value_list<T, A...>, constant<T, B>) { return {}; }
//template<typename T, T...A, T B>			constexpr value_list<T, (A > B ? A : B)...>	max(value_list<T, A...>, constant<T, B>) 	{ return {}; }
//template<typename T, T...A, T B>			constexpr value_list<T, (A < B ? A : B)...>	min(value_list<T, A...>, constant<T, B>) 	{ return {}; }

template<typename T, T...B, T A>			constexpr value_list<T, (A +  B)...> 	operator+ (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A -  B)...> 	operator- (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A *  B)...> 	operator* (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A /  B)...> 	operator/ (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A %  B)...> 	operator% (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A &  B)...> 	operator& (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A |  B)...> 	operator| (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A ^  B)...> 	operator^ (constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A << B)...> 	operator<<(constant<T, A>, value_list<T, B...>) { return {}; }
template<typename T, T...B, T A>			constexpr value_list<T, (A >> B)...> 	operator>>(constant<T, A>, value_list<T, B...>) { return {}; }
//template<typename T, T...B, T A>			constexpr value_list<T, (A > B ? A : B)...>	max(constant<T, A>, value_list<T, B...>) 	{ return {}; }
//template<typename T, T...B, T A>			constexpr value_list<T, (A < B ? A : B)...>	min(constant<T, A>, value_list<T, B...>) 	{ return {}; }

template<typename I, typename F, F f>			struct apply_imp;
template<typename T, T...I, typename F, F f>	struct apply_imp<value_list<T, I...>, F, f> : T_type<value_list<T, f(I)...>> {};
template<typename T, VL_element<T> (*f)(VL_element<T>)>		using	apply = typename apply_imp<T, VL_element<T> (*)(VL_element<T>), f>::type;
template<typename T, typename F, F f>			using	apply_lambda = typename apply_imp<T, F, f>::type;
#define VL_APPLY(VL, F)							meta::apply_lambda<VL, decltype(F), F>

template<typename T, typename F, F f, T I0, T...I> struct filter_imp {
	typedef typename filter_imp<T, F, f, I...>::type				type1;
	typedef if_t<f(I0), VL_concat<value_list<T, I0>, type1>, type1>	type;
};
template<typename T, typename F, F f, T I0>		struct filter_imp<T, F, f, I0> : T_if<f(I0), value_list<T, I0>, value_list<T>> {};
template<typename I, typename F, F f>			struct filter_imp2;
template<typename T, T...I, typename F, F f>	struct filter_imp2<value_list<T, I...>, F, f> : filter_imp<T, F, f, I...> {};

template<typename T, bool (*f)(VL_element<T>)>	using	filter = typename filter_imp2<T, bool(*)(VL_element<T>), f>::type;
template<typename T, typename F, F f>			using	filter_lambda = typename filter_imp2<T, F, f>::type;
#define VL_FILTER(VL, F)						meta::filter_lambda<VL, decltype(F), F>


template<int M, int A, typename I>			struct	muladd_imp;
template<typename T, int M, int A, T...I>	struct	muladd_imp<M, A, value_list<T, I...>> : T_type<value_list<T, I * M + A...>> {};
template<int M, int A, typename I>			using	muladd = typename muladd_imp<M, A, I>::type;

template<int A, int B, typename I>			struct	clamp_imp;
template<int A, int B, typename T, T...I>	struct	clamp_imp<A, B, value_list<T, I...>> : T_type<value_list<T, min<max<I, A>, B>...>> {};
template<int A, int B, typename I>			using	clamp = typename clamp_imp<A, B, I>::type;

template<int I0, int...I>					constexpr int reduce_add		= I0 + reduce_add<I...>;
template<int I0>							constexpr int reduce_add<I0>	= I0;

template<int I0, int...I>					constexpr int reduce_mul		= I0 * reduce_mul<I...>;
template<int I0>							constexpr int reduce_mul<I0>	= I0;

template<int I0, int...I>					constexpr int reduce_or			= I0 | reduce_or<I...>;
template<int I0>							constexpr int reduce_or<I0>		= I0;

template<int I0, int...I>					constexpr int reduce_and		= I0 | reduce_and<I...>;
template<int I0>							constexpr int reduce_and<I0>	= I0;

template<int I0, int...I>					constexpr int reduce_min		= min<I0, reduce_min<I...>>;
template<int I0>							constexpr int reduce_min<I0>	= I0;

template<int I0, int...I>					constexpr int reduce_max		= max<I0, reduce_max<I...>>;
template<int I0>							constexpr int reduce_max<I0>	= I0;

// permute
template<typename I, typename J>			struct	perm_imp;
template<typename IT, IT...I, typename JT, JT...J> struct perm_imp<value_list<IT, I...>, value_list<JT, J...>> {
	static constexpr IT values[sizeof...(I)] = {I...};
	typedef value_list<IT, (int)J < 0 ? -1 : values[J]...> type;
};
template<typename I, typename J>			using	perm	= typename perm_imp<I, J>::type;

template<int N, typename I>					using	roll	= perm<I, VL_concat<muladd<1, N, make_value_sequence<I::count - N>>, make_value_sequence<N>>>;
template<int N, typename I>					using	left	= perm<I, make_value_sequence<(N < 0 ? I::count + N : N)>>;
template<int N, typename I>					using	right	= perm<I, muladd<1, (N < 0 ? -N : I::count - N), make_value_sequence<(N < 0 ? I::count + N : N)>>>;

template<int I0, int I1, int...I>			constexpr int find(int i)						{ return I0 == i ? 0 : find<I1, I...>(i) + 1; }
template<int I0>							constexpr int find(int i)						{ return I0 == i ? 0 : -0x80000000; }
template<typename T, int...I>				constexpr int find(int i, value_list<T, I...>)	{ return find<I...>(i); }

template<typename J, typename I>			struct invert_imp;
template<typename T, T...J, typename I>		struct invert_imp<value_list<T, J...>, I>		: T_type<value_list<T, find(J, I())...>> {};
template<typename I, typename J = make_value_sequence<I::count>>	using inverse	= typename invert_imp<J, I>::type;

template<template<typename...> class X, typename... T> struct T_reverse;
template<template<typename...> class X, typename T0, typename... T> struct T_reverse<X, T0, T...> {
	template<typename... U> using _type = typename T_reverse<X, T...>::template _type<T0, U...>;
	using type = _type<>;
};
template<template<typename...> class X> struct T_reverse<X> {
	template<typename... UU> using _type = X<UU...>;
};
//-----------------------------------------------------------------------------
//	type list
//-----------------------------------------------------------------------------

template<size_t I, typename T = void>	struct tagged_type : T_type<T> {};
template<typename I, typename...T>				struct tagged_type_list;
template<size_t...I, typename...T>				struct tagged_type_list<index_list<I...>, T...> : tagged_type<I, T>... {};
template<size_t I, typename T>					static tagged_type<I, T> select(tagged_type<I, T>);

template<size_t N, typename = make_index_list<N>> struct param_index_impl;
template <size_t N, size_t ...ignore> struct param_index_impl<N, index_list<ignore...>> {
    template<typename T> static T *f(decltype((void*)ignore)..., T *t, ...) { return t; }
};

template<typename... T>							struct type_list { enum {count = sizeof...(T)}; };
template<template<class> class M, typename... T> struct map<M, type_list<T...>> : T_type<type_list<map_t<M, T>...>> {};

template<typename T>							struct TL_make_index_list;
template<typename...T>							struct TL_make_index_list<type_list<T...>> : make_index_list<sizeof...(T)> {};

template<size_t I, typename...T>				using VT_index_t = typename decltype(select<I>(tagged_type_list<make_index_list<sizeof...(T)>, T...>{}))::type;
template<size_t I, typename TL>					struct TL_index;
template<size_t I, typename... T>				struct TL_index<I, type_list<T...>> : T_type<VT_index_t<I, T...>> {};
template<size_t I, typename TL>					using TL_index_t = typename TL_index<I, TL>::type;

template<typename... T>							struct VT_head;
template<typename T0, typename... T>			struct VT_head<T0, T...>		: T_type<T0> {};
template<typename... T>							using VT_head_t = typename VT_head<T...>::type;
template<typename TL>							struct TL_head;
template<typename... T>							struct TL_head<type_list<T...>> : VT_head<T...> {};
template<typename TL>							using TL_head_t = typename TL_head<TL>::type;

template<typename... T>							struct VT_tail;
template<typename T0, typename... T>			struct VT_tail<T0, T...>		: T_type<type_list<T...>> {};
template<typename... T>							using VT_tail_t = typename VT_tail<T...>::type;
template<typename TL>							struct TL_tail;
template<typename... T>							struct TL_tail<type_list<T...>> : VT_tail<T...> {};
template<typename TL>							using TL_tail_t = typename TL_tail<TL>::type;

//	concatenate types A... and B...
template<typename...T>							struct TL_concat;
template<typename A, typename...B>				struct TL_concat<A, B...>		: T_type<type_list<A, typename TL_concat<B...>::type>> {};
template<typename...A, typename...B>			struct TL_concat<type_list<A...>, B...> : TL_concat<type_list<A...>, typename TL_concat<B...>::type> {};
template<typename...A, typename...B>			struct TL_concat<type_list<A...>, type_list<B...> > : T_type<type_list<A..., B...>> {};
template<typename...T>							using TL_concat_t	= typename TL_concat<T...>::type;

//	left N types from type_list
template<int N, typename R, typename... T>		struct VT_left;
template<int N, typename... R, typename T0, typename... T>	struct VT_left<N, type_list<R...>, T0, T...> : VT_left<N - 1, type_list<R..., T0>, T...> {};
template<typename... R, typename T0, typename... T>	struct VT_left<0, type_list<R...>, T0, T...> : T_type<type_list<R...>> {};
template<typename R, typename... T>				struct VT_left<0, R, T...> : T_type<R> {};
template<int N, typename...T>					using VT_left_t		= typename VT_left<(N < 0 ? sizeof...(T) + N : N), type_list<>, T...>::type;
template<int N, typename T>						struct TL_left;
template<int N, typename... T>					struct TL_left<N, type_list<T...>>	: T_type<VT_left_t<N, T...>> {};
template<int N, typename TL>					using TL_left_t		= typename TL_left<N, TL>::type;

//	right N types from type_list
template<size_t N, typename... T>				struct VT_right;
template<size_t N, typename T0, typename... T>	struct VT_right<N, T0, T...>	: VT_right<N - 1, T...>	{};
template<typename T0, typename... T>			struct VT_right<0, T0, T...>	: T_type<type_list<T0, T...>> {};
template<typename... T>							struct VT_right<0, T...>		: T_type<type_list<T...>> {};
template<int N, typename... T>					using VT_right_t	= typename VT_right<(N < 0 ? -N : sizeof...(T) - N), T...>::type;
template<int N, typename TL>					struct TL_right;
template<int N, typename... T>					struct TL_right<N, type_list<T...>>	: T_type<VT_right_t<N, T...>> {};
template<int N, typename TL>					using TL_right_t	= typename TL_right<N, TL>::type;

template<typename X, typename T0, typename...T>	constexpr int VT_find			= same_v<X, T0> ? 0 : VT_find<X, T...> + 1;
template<typename X, typename T0>				constexpr int VT_find<X, T0>	= same_v<X, T0> ? 0 : 1;

template<typename X, typename TL>				constexpr int TL_find			= -1;
template<typename X, typename...T>				constexpr int TL_find<X, type_list<T...>> = VT_find<X, T...>;

//-----------------------------------------------------------------------------
//	tuple
//-----------------------------------------------------------------------------

template<typename... T> struct tuple;
template<typename T0, typename... T> struct tuple<T0, T...> {
	const T0		head;
	tuple<T...>		tail;
	constexpr tuple(const T0& t0, const T&... t) : head(t0), tail(t...) {}
};
template<> struct tuple<> {};

template<typename... T> constexpr tuple<T...> make_tuple(const T&... t) { return {t...}; }

//-----------------------------------------------------------------------------
//	array
//-----------------------------------------------------------------------------

template<typename T, size_t N> struct array {
	T t[N];
	constexpr size_t	size()					const	{ return N; }
	constexpr uint32	size32()				const	{ return uint32(N); }

	constexpr const T*	begin()					const	{ return t; }
	constexpr const T*	end()					const	{ return t + N; }
	constexpr const T&	front()					const	{ return t[0]; }
	constexpr const T&	back()					const	{ return t[N - 1]; }
	constexpr const T&	at(size_t i)			const	{ return t[i]; }
	constexpr const T&	operator[](size_t i)	const	{ return t[i]; }

	constexpr T*		begin()							{ return t; }
	constexpr T*		end()							{ return t + N; }
	constexpr T&		front()							{ return t[0]; }
	constexpr T&		back()							{ return t[N - 1]; }
	constexpr T&		at(size_t i)					{ return t[i]; }
	constexpr T&		operator[](size_t i)			{ return t[i]; }

	constexpr bool		contains(const T *e)	const	{ return e && e >= begin() && e < end(); }
	constexpr bool		contains(const T &e)	const	{ return &e >= begin() && &e < end(); }
	constexpr int		index_of(const T *e)	const	{ return e ? e - begin() : -1; }
	constexpr int		index_of(const T &e)	const	{ return &e - begin(); }

	constexpr bool	operator==(const array &b)	const	{ return memcmp(this, &b, sizeof(*this)) == 0; }
	constexpr bool	operator!=(const array &b)	const	{ return memcmp(this, &b, sizeof(*this)) != 0; }

	template<typename R> bool read(R &r)				{ return readn(r, begin(), N); }
	friend constexpr size_t num_elements(const array &t) { return N; }
};

template<typename T> struct array<T, 0> {
	const T* t = nullptr;
	constexpr size_t	size()					const	{ return 0; }
	constexpr uint32	size32()				const	{ return 0; }
	constexpr const T*	begin()					const	{ return nullptr; }
	constexpr const T*	end()					const	{ return nullptr; }

	constexpr bool		contains(const T *e)	const	{ return false; }
	constexpr bool		contains(const T &e)	const	{ return false; }
	constexpr int		index_of(const T *e)	const	{ return -1; }
	constexpr int		index_of(const T &e)	const	{ return -1; }
};


template<typename T, typename T2, T2... v>				constexpr array<T, sizeof...(v)>	make_array(value_list<T2, v...>)	{ return {{T(v)...}}; }
template<typename T, T... v>							constexpr array<T, sizeof...(v)>	make_array(value_list<T, v...>)		{ return {{v...}}; }
template<typename T, typename... T2>					constexpr array<T, sizeof...(T2)>	make_array0(T2... v)				{ return {{T(v)...}}; }
template<typename T, typename T2, size_t... I>			constexpr array<T, sizeof...(I)>	make_array(const T2 *p, const index_list<I...>, size_t offset)	{ return {{T(p[I + offset])...}}; }
template<typename T, typename T2, size_t... I>			constexpr array<T, sizeof...(I)>	make_array(const T2 *p, const index_list<I...>)	{ return {{T(p[I])...}}; }

template<typename T, size_t N>							constexpr auto	make_array(const T (&v)[N])			{ return make_array<T>(v, make_index_list<N>()); }
template<typename T, typename T2, size_t N>				constexpr auto	make_array(const T2 (&v)[N])		{ return make_array<T>(v, make_index_list<N>()); }
template<typename T, size_t N, typename T2, size_t N2>	constexpr auto	make_array(const T2 (&v)[N2])		{ return make_array<T>(v, make_index_list<N>()); }

template<typename T, size_t N>							constexpr auto	make_array(const array<T,N> &v)		{ return v; }
template<typename T, typename T2, size_t N>				constexpr auto	make_array(const array<T2,N> &v)	{ return make_array<T>(v.begin(), make_index_list<N>()); }

template<typename T, size_t... I1, size_t... I2, typename T1, typename T2> constexpr array<T, sizeof...(I1) + sizeof...(I2)> concat(const T1 *p1, index_list<I1...>, const T2 *p2, const index_list<I2...>) {
	return {{T(p1[I1])..., T(p2[I2])...}};
}

template<typename T, size_t N1>				constexpr auto	operator+(const array<T, N1> &a1, const array<T, 0>&)		{ return a1; }
template<typename T, size_t N1, size_t N2>	constexpr auto	operator+(const array<T, N1> &a1, const array<T, N2> &a2)	{ return concat<T>(a1.begin(), make_index_list<N1>(),  a2.begin(), make_index_list<N2>()); }
template<typename T, size_t N>				constexpr auto	operator+(const array<T, N> &a, T v)						{ return concat<T>(a.begin(), make_index_list<N>(), &v, make_index_list<1>()); }

template<int O, typename T, size_t N>		constexpr auto	trim_front(const T (&v)[N])					{ return make_array<T>(v, muladd<1, (O < 0 ? N + O : O), make_index_list<(O < 0 ? -O : N - O)>>()); }
template<int O, typename T, size_t N>		constexpr auto	trim_back(const T (&v)[N])					{ return make_array<T>(v, make_index_list<(O < 0 ? -O : N - O)>()); }

template<int O, typename T, size_t N>		constexpr auto	trim_front(const array<T,N> &v)				{ return trim_front<O>(v.t); }
template<int O, typename T, size_t N>		constexpr auto	trim_back(const array<T,N> &v)				{ return trim_back<O>(v.t); }
template<size_t N, typename T, size_t N2>	constexpr auto	slice(const array<T,N2> &v, size_t offset)	{ return make_array<T>(v.t, make_index_list<N>(), offset); }

template<typename T, int N, typename T2, size_t... I>	inline void put(T2 *p, const array<T, N> &a, const index_list<I...>)	{ unused((p[I] = a[I])...); }
template<typename T, int N, typename T2>				inline void put(T2 *p, const array<T, N> &a)							{ put(p, a, make_index_list<N>()); }

template<typename T, typename S, typename I>	struct explode_imp;
template<typename T, typename S, size_t... I>	struct explode_imp<T, S, index_list<I...>> : T_type<value_list<T, S::array[I]...>> {};
template<typename T, typename S>				using explode = typename explode_imp<T, S, make_index_list<sizeof(S::array)/sizeof(S::array[0])>>::type;

//-----------------------------------------------------------------------------
//	string
//-----------------------------------------------------------------------------

struct string {
	const char* s;
	size_t 		len;
	template<size_t N> constexpr string(const char (&s)[N]) 	: s(s), len(N - 1) {}
	constexpr operator const char*() const { return s; }
};

template<class T> constexpr auto _name() {
#ifdef __FUNCSIG__
	return make_array(__FUNCSIG__);
#else
	return make_array(__PRETTY_FUNCTION__);
#endif
}

template<int N, char... C>			struct find_name2;
template<int N, char C0, char... C>	struct find_name2<N, C0, C...> : find_name2<N + 1, C...>{};
template<int N, char... C>			struct find_name2<N, 'x','y','z', C...> { static const int front = N, back = sizeof...(C); };
template<typename U>				struct find_name;
template<char... C>					struct find_name<value_list<char, C...>> : find_name2<0, C...> {};

template<typename T> struct name_test_s { constexpr static auto array{_name<T>()}; };
using name_location = find_name<explode<char, name_test_s<xyz>>>;

template<class T> constexpr auto name() {
#if 1
	return trim_back<name_location::back>(trim_front<name_location::front>(_name<T>()));
#else
#ifdef __FUNCSIG__
	return trim_back<8>(trim_front<29>(__FUNCSIG__));
#else
	return trim_back<2>(trim_front<28>(__PRETTY_FUNCTION__));
#endif
#endif
}

template<char... C>		struct fix_name2										: value_list<char, C...> {};
template<char... C>		struct fix_name2<'c','l','a','s','s',' ',		C...>	: value_list<char, C...> {};
template<char... C>		struct fix_name2<'s','t','r','u','c','t', ' ',	C...>	: value_list<char, C...> {};

template<typename U>	struct fix_name;
template<char... C>		struct fix_name<value_list<char, C...>> : fix_name2<C...> {};

template<typename T> struct name_s { constexpr static auto array{name<T>()}; };

template<class T> constexpr auto fixed_name() { return make_array(fix_name<explode<char, name_s<T>>>()) + '\0'; }

//-----------------------------------------------------------------------------
//	field
//-----------------------------------------------------------------------------
template<typename T, T t> struct field {
	const char *name;
	constexpr field(const char *name) : name(name) {}
};

#define MAKE_FIELD(f, name)	meta::field<decltype(f), f>(name)
#define FIELD(S, f)			meta::field<decltype(&S::f), &S::f>(#f)
#define FIELDS(S, ...)		VA_APPLYP(FIELD, S, __VA_ARGS__)

} // namespace meta

using meta::value_list;
using meta::index_list;
using meta::type_list;
using meta::map_t;
//using meta::array;

//-----------------------------------------------------------------------------
//	debug stuff
//-----------------------------------------------------------------------------

struct _iso_debug_print_t {
	typedef void func(void*, const char*);
	func	*f;
	void	*p;
	void operator()(const char *s) const { f(p, s); }
};

extern iso_export _iso_debug_print_t	_iso_debug_print;
extern iso_export _iso_debug_print_t	_iso_set_debug_print(const _iso_debug_print_t &f);
iso_export	void _iso_dump_heap(uint32 flags);
iso_export	void _iso_assert_msg(const char *filename, int line, const char *expr);

template<typename T> force_inline T _iso_cheapverify(T t) {
	if (!t)
		_iso_break();
	return t;
}
template<typename T> force_inline T _iso_verify(T &&t, const char *filename, int line, const char *expr) {
	if (!t) {
		_iso_assert_msg(filename, line, expr);
		_iso_break();
	}
	return t;
}

#if defined NDEBUG && !defined ISO_RELEASE
#define ISO_RELEASE
#endif
#if !defined ISO_DEBUG && defined _DEBUG
#define ISO_DEBUG
#endif

#ifdef ISO_DEBUG
#define ISO_ON_DEBUG(x)			x
#define ISO_ON_DEBUG_STMT(x)	x
#define ISO_NOT_DEBUG(x)
#else
#define ISO_ON_DEBUG(x)
#define ISO_ON_DEBUG_STMT(x)	((void)0)
#define ISO_NOT_DEBUG(x)		x
#endif

#define DO(x)							do x while(0)
#define ISO_OUTPUT(x)					iso::_iso_debug_print(x)
#define ISO_ALWAYS_VERIFY(exp)			iso::_iso_verify(exp, __FILE__, __LINE__, #exp)
#define ISO_ALWAYS_CHEAPVERIFY(exp)		iso::_iso_cheapverify(exp)
#define ISO_ALWAYS_CHECKHEAP(flags)		iso::_iso_dump_heap(flags)
#define ISO_ALWAYS_CHEAPASSERT(exp)		DO({if (!(exp)) _iso_break(); })
#define ISO_ALWAYS_ASSERT2(exp, msg)	DO({if (!(exp)) { iso::_iso_assert_msg(__FILE__, __LINE__, msg); _iso_break(); } })
#define ISO_ALWAYS_ASSERT(exp)			ISO_ALWAYS_ASSERT2(exp, #exp)
#define ISO_ALWAYS_ASSUME(exp, msg)		DO({if (!(exp)) iso::_iso_debug_print(msg); })

#ifdef ISO_RELEASE
#define ISO_NOT_RELEASE(x)
#define ISO_NOT_RELEASE_STMT(x)	((void)0)
#define ISO_VERIFY(exp)			(exp)
#define ISO_CHEAPVERIFY(exp)	(exp)
#else
#define ISO_NOT_RELEASE(x)		x
#define ISO_NOT_RELEASE_STMT(x)	x
#define ISO_VERIFY(exp)			ISO_ALWAYS_VERIFY(exp)
#define ISO_CHEAPVERIFY(exp)	ISO_ALWAYS_CHEAPVERIFY(exp)
#endif

#define ISO_CHEAPASSERT(exp)	ISO_NOT_RELEASE_STMT(ISO_ALWAYS_CHEAPASSERT(exp))
#define ISO_ASSERT(exp)			ISO_NOT_RELEASE_STMT(ISO_ALWAYS_ASSERT(exp))
#define ISO_ASSERT2(exp, msg)	ISO_NOT_RELEASE_STMT(ISO_ALWAYS_ASSERT2(exp, msg))
#define ISO_ASSUME(exp, x)		ISO_NOT_RELEASE_STMT(ISO_ALWAYS_ASSUME(exp, x))
#define ISO_CHECKHEAP(flags)	ISO_NOT_RELEASE_STMT(ISO_ALWAYS_CHECKHEAP(flags))
#define ISO_TRACE(x)			ISO_NOT_RELEASE_STMT(ISO_OUTPUT(x))

//#define ISO_COMPILEASSERT(exp)	typedef iso::T_enable_if<!!(exp)> iso_compileassert_typedef##_LINE_
#define ISO_COMPILEASSERT(exp)	static_assert(exp, "error")

#define TBD						_iso_break();

//-----------------------------------------------------------------------------
//	type traits
//-----------------------------------------------------------------------------
template<typename T> constexpr bool is_union						= __is_union(T);
template<typename T> constexpr bool is_class						= __is_class(T);
template<typename T> constexpr bool is_enum							= __is_enum(T);
template<typename T> constexpr bool is_pod							= __is_pod(T);
template<typename T> constexpr bool is_trivially_copyable_v			= __is_trivially_copyable(T);
template<typename T> constexpr bool is_trivially_destructible_v		= __is_trivially_destructible(T);
template<typename T> constexpr bool is_trivially_constructible_v	= __is_trivially_constructible(T);

template<typename T, T>						struct T_checktype;
template<bool b, typename T = void>			struct T_enable_if				{};
template<typename T>						struct T_enable_if<true, T>		: T_type<T> {};
template<bool b, typename T = void>			using enable_if_t				= typename T_enable_if<b, T>::type;

//template<typename A,typename B,typename R=A>struct T_same					: T_false {};
//template<typename A, typename R>			struct T_same<A,A,R>			: T_true_type<R> {};
//template<typename A,typename B,typename R=A>using same_t					= typename T_same<A, B, R>::type;
//template<typename A, typename B, typename...T>	constexpr bool same_v		= T_same<A, B>::value && same_v<A, T...>;
//template<typename A,typename B>				constexpr bool same_v<A,B>		= T_same<A, B>::value;

template<typename T, typename R=T>			struct T_exists					: T_true_type<R> {};
template<typename T, typename R=T>			using exists_t					= typename T_exists<T, R>::type;
template<typename T> 						constexpr bool exists_v			= T_exists<T>::value;

template<typename T>						struct T_is_pointer				: T_false	{};
template<typename T>						struct T_is_pointer<T*>			: T_true	{};
template<typename T>						constexpr bool is_pointer_v		= T_is_pointer<T>::value;

template<typename T>						struct T_is_lvalue				: T_false	{};
template<typename T>						struct T_is_lvalue<const T&>	: T_false	{};
template<typename T>						struct T_is_lvalue<T&>			: T_true	{};
template<typename T>						constexpr bool is_lvalue_v		= T_is_lvalue<T>::value;

template<typename...T>						struct T_void					: T_type<void> {};
template<typename...T>						using void_t					= typename T_void<T...>::type;

template<typename T, typename = void>		struct T_ref					: T_type<T> {};
template<typename T>						struct T_ref<T, void_t<T&>>		: T_type<T&> {};

template<typename T, typename = void>		struct T_rref					: T_type<T> {};
template<typename T>						struct T_rref<T, void_t<T&>>	: T_type<T&&> {};

template<typename T>						struct T_reftoptr				: T_type<T> {};
template<typename T>						struct T_reftoptr<T&>			: T_type<T*> {};
template<typename T>						struct T_reftoptr<const T&>		: T_type<const T *> {};

template<typename T>						struct T_noconst				: T_type<T> {};
template<typename T>						struct T_noconst<const T>		: T_type<T> {};
template<typename T>						using noconst_t		= typename T_noconst<T>::type;

template<typename T>						struct T_nocv					: T_type<T> {};
template<typename T>						struct T_nocv<const T>			: T_type<T> {};
template<typename T>						struct T_nocv<volatile T>		: T_type<T> {};
template<typename T>						struct T_nocv<const volatile T>	: T_type<T> {};
template<typename T>						using nocv_t		= typename T_nocv<T>::type;

template<typename T>						struct T_noref					: T_type<T> {};
template<typename T>						struct T_noref<T&>				: T_type<T> {};
#ifdef USE_RVALUE_REFS
template<typename T>						struct T_noref<T&&>				: T_type<T> {};
#endif
template<typename T>						using noref_t		= typename T_noref<T>::type;
template<typename T>						using noref_cv_t	= nocv_t<noref_t<T>>;

template<typename T>						struct T_noarray				: T_type<T> {};
template<typename T, int N>					struct T_noarray<T[N]>			: T_type<T*> {};
template<typename T>						using noarray_t		= typename T_noarray<T>::type;

template<typename A, typename B>			struct T_copyconst				: T_type<B> {};
template<typename A, typename B>			struct T_copyconst<const A, B>	: T_type<const B> {};
template<typename A, typename B>			struct T_copyconst<const A, B&>	: T_type<const B&> {};
template<typename A, typename B>			struct T_copyconst<const A&, B>	: T_type<const B> {};
template<typename A, typename B>			struct T_copyconst<const A&, B&>: T_type<const B&> {};
template<typename A, typename B>			using copy_const_t				= typename T_copyconst<A, B>::type;

template<typename T>						struct T_param					: T_type<const T&> {};
template<typename T>						struct T_param<T&>				: T_type<T&> {};
template<typename T>						using param_t		= typename T_param<T>::type;

template<typename T>						struct T_rref_param				: T_type<T&&> {};
template<typename T>						struct T_rref_param<T&>			: T_type<T&> {};
template<typename T> noref_t<T>&			declval() noexcept;
//template<typename T> typename T_rref<T>::type	declval() noexcept;

template<typename T>						struct T_lderef					: T_type<decltype(*declval<T>())> {};
template<typename T>						using lderef_t		= typename T_lderef<T>::type;

template<typename T>						struct T_deref					: T_type<noref_t<decltype(*declval<T>())>> {};
template<>									struct T_deref<void*>			: T_type<void> {};
template<>									struct T_deref<const void*>		: T_type<void> {};
template<typename C, typename T>			struct T_deref<T C::*>			: T_type<T> {};
template<typename C, typename T>			struct T_deref<const T C::*>	: T_type<const T> {};
template<typename T>						using deref_t		= typename T_deref<T>::type;

template<typename A, typename B> using biggest_t	= if_t<(sizeof(A) > sizeof(B)), A, B>;
template<typename A, typename B> using smallest_t	= if_t<(sizeof(A) < sizeof(B)), A, B>;

struct yesno {
	typedef char yes;
	struct no { yes c[2]; };
	template<int N>			struct num	{ num(int); };
	template<typename T>	struct type { type(int); };
};

template<typename T> struct T_isclass : yesno {
	template<typename C>	static yes	test(int C::*);
	template<typename C>	static no	test(...);
	enum { value = sizeof(test<T>(0)) == sizeof(yes) };
};

template<typename T, bool = T_isclass<T>::value> struct T_isempty {
	struct helper : nocv_t<T> { int i; };
	static const bool value = sizeof(helper) == sizeof(int);
};
template<typename T> struct T_isempty<T, false> : T_false {};

template<typename T> struct T_isabstract : yesno {
	// Deduction fails if T is void, function type, reference type or an abstract class type
	template<typename C>	static no	test(C(*)[1]);
	template<typename C>	static yes	test(...);
	enum { value = sizeof(test<T>(0)) == sizeof(yes) };
};


template<typename A, typename B> class T_conversion : yesno {
	static yes	test(B);
	static no	test(...);
public:
	enum {
		exists		= sizeof(test(declval<A>())) == sizeof(yes),
		exists2way	= exists && T_conversion<B, A>::exists,
		sametype	= false
	};
};
template<typename T> struct T_conversion<T, T> {
	enum { exists = true, exists2way = true, sametype = true };
};

template<typename B, typename D> class T_is_base_of : yesno {
	static yes	test(const volatile B*);
	static no	test(const volatile void*);
public:
	enum { value = sizeof(test(declval<D*>())) == sizeof(yes) };
};

template<typename A, typename B> struct T_constructable : yesno {
	template<typename U>	static yes	&test(U);
	template<typename>		static no	&test(...);
	static bool const value = sizeof(test<A>(declval<B>())) == sizeof(yes);
};
template<typename A, typename B> static constexpr bool constructable_v = T_constructable<A, B>::value;

template<typename A, typename B, typename=void> struct T_assignable : T_false {};
template<typename A, typename B> struct T_assignable<A, B, T_void<decltype(declval<A>() = declval<B>())>> : T_true {};
template<typename A, typename B> static constexpr bool assignable_v = T_assignable<A, B>::value;

template<bool C, typename T, typename...P> struct T_is_function : yesno {
	template<typename R> static yes	test(R(*)(P...));
	template<typename R> static yes	test(R(T::*)(P...));
	template<typename R> static yes	test(R(T::*)(P...) const);
	static no						test(...);
	static const bool value = sizeof(test(declval<T>())) == sizeof(yes);
};
template<typename T, typename...P> struct T_is_function<false, T, P...> : yesno {
	template<typename R> static yes	test(R(*)(P...));
	static no						test(...);
	static const bool value = sizeof(test(declval<T>())) == sizeof(yes);
};
template<typename T, typename...P> static constexpr bool is_function = T_is_function<T_isclass<T>::value, T, P...>::value;

template<typename T> struct is_array_s 				: T_false {};
template<typename T> struct is_array_s<T[]> 		: T_true, T_type<T> {};
template<typename T, int N> struct is_array_s<T[N]>	: T_true, T_type<T> {};
template<typename T> static constexpr bool is_array = is_array_s<T>::value;
template<typename T>			using array_t		= typename is_array_s<T>::type;

template<typename T> struct can_index_s : yesno {
	template<typename U> static no		test(...);
	template<typename U> static yes		test(iso::noref_t<decltype(declval<U>()[0])>*);
	static const bool value = sizeof(test<T>(0)) == sizeof(yes);
};
template<typename T> static constexpr bool can_index = can_index_s<T>::value;

#define T_HASMEMFUNC(F) template<typename T, typename S> struct has_##F : yesno {\
	template<typename U>	static yes	&test(T_checktype<S U::*, &U::F>*);		\
	template<typename>		static no	&test(...);								\
	static bool const value = sizeof(test<T>(0)) == sizeof(yes);				\
}

template<typename T, bool = is_enum<T>>	struct _underlying_type : T_type<__underlying_type(T)> {};
template<typename T>					struct _underlying_type<T, false> {};
template<typename T> using underlying_type = typename _underlying_type<T>::type;

template<typename T, typename V = void> struct T_has_deref	: T_false {};
template<typename T, typename V = void> struct T_has_inc	: T_false {};
template<typename T, typename V = void> struct T_has_dec	: T_false {};
template<typename T, typename V = void> struct T_has_index	: T_false {};
template<typename T, typename V = void> struct T_has_arrow	: T_false {};
//template<typename T, typename I = int, typename V = void> struct T_has_add		: T_false {};
//template<typename T, typename I = int, typename V = void> struct T_has_addsub	: T_false {};

template<typename T> struct T_has_deref	<T, void_t<decltype(*declval<T>())>>			: T_true {};
template<typename T> struct T_has_inc	<T, void_t<decltype(++declval<T>())>>			: T_true {};
template<typename T> struct T_has_dec	<T, void_t<decltype(--declval<T>())>>			: T_true {};
template<typename T> struct T_has_index	<T, void_t<decltype(declval<T>()[1])>>			: T_true {};
template<typename T> struct T_has_arrow<T*>												: T_isclass<T>	{};
template<typename T> struct T_has_arrow<T&>												: T_has_arrow<T> {};
template<typename T> struct T_has_arrow<T, void_t<decltype(declval<T>().operator->())>>	: T_has_arrow<decltype(declval<T>().operator->())> {};
//template<typename T, typename I> struct T_has_add<T, I, void_t<decltype(declval<T>()+declval<I>())>> : T_true {};
//template<typename T, typename I> struct T_has_addsub<T, I, void_t<decltype(declval<T>()+declval<I>()-declval<I>())>> : T_true {};

template<typename A, typename B, typename R> using can_add_t	= exists_t<decltype(declval<A>() + declval<B>()), R>;
template<typename A, typename B, typename R> using can_addsub_t	= exists_t<decltype(declval<A>() + declval<B>() - declval<B>()), R>;
template<typename A, typename B, typename=void> constexpr bool can_add_v									= false;
template<typename A, typename B>				constexpr bool can_add_v<A, B, can_add_t<A, B, void>>		= true;
template<typename A, typename B, typename=void> constexpr bool can_addsub_v									= false;
template<typename A, typename B>				constexpr bool can_addsub_v<A, B, can_addsub_t<A, B, void>>	= true;

template<typename T> struct T_traits {
	typedef T					type;
	typedef const T				ctype;
	typedef	noref_t<T>			&ref;
	typedef noref_t<const T>	&cref;
};
template<typename T> struct T_traits<const T> {
	typedef T							type, ctype;
	typedef	typename T_traits<T>::cref	ref, cref;
};
template<> struct T_traits<void> {
	typedef void type, ctype, ref, base;
	typedef const int cref;
};


template<typename T> struct T_builtin_int	: T_false {};
#ifndef USE_SIGNEDCHAR
template<> struct T_builtin_int<char>		: T_true {};
#endif
template<> struct T_builtin_int<int8>		: T_true {};
template<> struct T_builtin_int<int16>		: T_true {};
template<> struct T_builtin_int<int32>		: T_true {};
template<> struct T_builtin_int<int64>		: T_true {};
template<> struct T_builtin_int<uint8>		: T_true {};
template<> struct T_builtin_int<uint16>		: T_true {};
template<> struct T_builtin_int<uint32>		: T_true {};
template<> struct T_builtin_int<uint64>		: T_true {};
#if USE_LONG
template<> struct T_builtin_int<long>		: T_true {};
template<> struct T_builtin_int<ulong>		: T_true {};
#endif
template<> struct T_builtin_int<wchar_t>	: T_true {};
template<> struct T_builtin_int<char16_t>	: T_true {};
template<> struct T_builtin_int<char32_t>	: T_true {};

template<typename T> struct T_builtin_num	: T_builtin_int<T> {};
template<> struct T_builtin_num<float>		: T_true {};
template<> struct T_builtin_num<double>		: T_true {};
template<typename T> constexpr bool is_builtin_num		= T_builtin_num<T>::value;

template<typename T> class T_swap_endian;
template<typename T> struct T_swap_endian_type						: T_type<T_swap_endian<T>> {};
template<typename T> struct T_swap_endian_type<T_swap_endian<T> >	: T_type<T> {};
template<typename T> using swap_endian_t		= typename T_swap_endian_type<T>::type;

template<typename T, typename V = void> struct num_traits {
	//enum {bits = 0};
	static const bool	is_signed	= false;
	static const bool	is_float	= false;
};

template<int M, int E, bool S> struct float_traits {
	static const int bits = E + M + int(S), exponent_bits = E, mantissa_bits = M;
	static const bool	is_signed	= S;
	static const bool	is_float	= true;
};

template<typename T> struct num_traits<T, enable_if_t<T_builtin_int<T>::value>> {
	enum {bits = sizeof(T) * 8};
	static const bool	is_signed	= T(-1) < T(0);
	static const bool	is_float	= false;
	static constexpr T	min()			{ return num_traits::is_signed ? T(-(T(1) << (sizeof(T) * 8 - 1))) : T(0); }
	static constexpr T	max()			{ return T(~min());	}
	static constexpr T	cast(float t)	{ return ISO_CHEAPVERIFY(t >= min() && t <= max()), T(t + 0.5f); }
	template<typename T2> static constexpr enable_if_t<num_traits<T2>::is_signed, T>	cast(const T2 &t) { return ISO_CHEAPVERIFY(t >= min() && t <= max()), T(t); }
	template<typename T2> static constexpr enable_if_t<!num_traits<T2>::is_signed, T>	cast(const T2 &t) { return ISO_CHEAPVERIFY(t <= max()), T(t); }
};

template<typename T> struct T_isint		: T_builtin_int<T> {};
template<typename T> struct T_isint<T&>	: T_isint<T> {};

template<typename T, typename=void> struct T_int_ops	: T_isint<T> {};	// operated on like an int (e.g. vector of int)
template<typename T, typename=void> struct T_int_rep	: T_isint<T> {};	// has an integer representation (eg. bool, enum)
template<> struct T_int_rep<bool>	: T_true {};
template<typename T> struct T_int_rep<T, enable_if_t<is_enum<T>>>	: T_true {};

template<typename T> constexpr bool is_int		= T_isint<T>::value;
template<typename T> constexpr bool has_int_ops	= T_int_ops<T>::value;
template<typename T> constexpr bool has_int_rep	= T_int_rep<T>::value;
template<typename T> constexpr bool is_signed	= num_traits<T>::is_signed;
template<typename T> constexpr bool is_float	= num_traits<T>::is_float;
template<typename T> constexpr bool is_num		= is_int<T> || is_float<T>;

template<class T0, class... T> struct T_test_overload : T_test_overload<T...> {
	using T_test_overload<T...>::f;
	static T0 f(T0);
};
template<class T0> struct T_test_overload<T0> {
	static void f(...);
	static T0 f(T0);
};

template<class X, class... T> using best_match_t = decltype(T_test_overload<T...>::f(declval<X>()));

#ifdef __OBJC__
template<class T> constexpr bool is_objc = T_is_base_of<id, T>::value;
#else
template<class T> constexpr bool is_objc = false;
#endif

//-----------------------------------------------------------------------------
//	mem
//-----------------------------------------------------------------------------

iso_export void		free(void* p);
iso_export void		free(void* p, size_t size);
iso_export void*	malloc(size_t size);
iso_export void*	realloc(void *p, size_t size);
iso_export void*	resize(void *p, size_t size);
//iso_export void*	calloc(size_t, size_t);

iso_export void		aligned_free(void *p);
iso_export void*	aligned_alloc(size_t size, size_t align);
iso_export void*	aligned_realloc(void *p, size_t size, size_t align);
iso_export void*	aligned_resize(void *p, size_t size, size_t align);
iso_export void*	aligned_alloc_unchecked(size_t size, size_t align);

inline void* memdup(const void *p, size_t s) { return memcpy(malloc(s), p, s); }

//-----------------------------------------------------------------------------
//	casts
//-----------------------------------------------------------------------------

template<typename T> constexpr	T*				keep_const(void *t)			{ return (T*)t; }
template<typename T> constexpr	const T*		keep_const(const void *t)	{ return (const T*)t; }
template<typename T> constexpr	T*				unconst(const T *t)			{ return const_cast<T*>(t);					}
template<typename T> constexpr	T&				unconst(const T &t)			{ return const_cast<T&>(t);					}
template<typename T> constexpr	const T*		make_const(T *t)			{ return const_cast<const T*>(t);			}
template<typename T> constexpr	const T&		make_const(T &t)			{ return const_cast<const T&>(t);			}
template<typename B, typename A> constexpr B	up_cast(A *a)				{ return (B)((char*)a - (int)(A*)(B)1 + 1); }
template<typename B, typename A> constexpr B	simple_cast(A &&a)			{ return a; }
template<typename B, typename A> constexpr B	force_cast(const A &a)		{ return reinterpret_cast<const B&>(a); }
template<typename B, typename A> constexpr B	force_cast(A &&a)			{ return reinterpret_cast<B&&>(a); }

template<typename B, typename A>				constexpr B* element_cast(A *p)				{ return reinterpret_cast<B*>(p); }
template<typename B, typename A, typename C>	constexpr B C::* element_cast(A C::*p)		{ return reinterpret_cast<B C::*>(p); }
template<typename B, typename A, typename T>	constexpr T B::* container_cast(T A::*p)	{ return static_cast<T B::*>(p); }

#ifdef USE_RVALUE_REFS
template<typename T> constexpr noref_t<T>&&		move(T &&a)					{ return static_cast<noref_t<T>&&>(a); }
template<typename T> constexpr T&&				move_nonref(T &a)			{ return static_cast<T&&>(a); }
template<typename T, int N>	constexpr T			(&move(T (&a)[N]))[N]		{ return a; }
template<typename T> nodebug_inline constexpr T&& forward(noref_t<T>& a) noexcept	{ return static_cast<T&&>(a); }
template<typename T> constexpr noref_t<T>		prvalue(T &&t)				{ return t; }
template<typename T> constexpr const noref_t<T>& rvalue(T &&t)				{ return t; }
template<typename T> constexpr noref_t<T>&		lvalue(T &&t)				{ return t; }
//template<typename T> constexpr noref_t<T>*		ptr(T &&t)					{ return &t; }
template<typename T> constexpr bool				is_constexpr(T &&t)			{ return noexcept(prvalue(t)); }
template<typename T> constexpr const T&			get(const T &t)				{ return t; }
template<typename T> constexpr	decltype(auto)	put(T &&t)					{ return forward<T>(t); }
template<typename T> constexpr	T				copy(const T &t)			{ return t; }
template<typename T> constexpr	T				copy(T &t)					{ return t; }
template<typename T> constexpr	T				copy(T &&t)					{ return move(t); }
#else
template<typename T> constexpr T&				move(const T &t)			{ return const_cast<T&>(t); }
#endif

template<template<class> class C, typename T>				constexpr C<T>*					T_get_class(T *t)				{ return (C<T>*)0; }
template<template<class> class C, typename S, typename T>	constexpr C<T S::*>*			T_get_class(T S::*t)			{ return (C<T S::*>*)0; }
//template<template<class> class C, typename S, typename T>	constexpr C<T>*					T_get_class(T S::*t)			{ return (C<T S::*>*)0; }
template<template<class> class C, typename S, typename T>	constexpr C<T (S::*)()>*		T_get_class(T (S::*t)())		{ return (C<T (S::*)()>*)0; }
template<template<class> class C, typename S, typename T>	constexpr C<T (S::*)() const>*	T_get_class(T (S::*t)() const)	{ return (C<T (S::*)() const>*)0; }
template<template<class> class C, typename S, typename T, typename P1>	inline C<T (S::*)(P1) const>*	T_get_class(T (S::*t)(P1) const)	{ return (C<T (S::*)(P1) const>*)0; }

template<typename B, typename T>	constexpr uintptr_t	T_get_base_offset()							{ return uintptr_t(static_cast<B*>((T*)1)) - 1; }
template<typename B, typename T>	constexpr uintptr_t	T_get_base_offset(T *t)						{ return T_get_base_offset<B, T>(); }
template<typename S, typename T>	constexpr uintptr_t	T_get_member_offset(T S::*t)				{ return uintptr_t(__builtin_addressof(((S*)1)->*t)) - 1; }
template<typename S, typename S2, typename T>	constexpr uintptr_t	T_get_member_offset2(T S2::*t)	{ return uintptr_t(__builtin_addressof(((S*)1)->*t)) - 1; }
template<typename S, typename T>	constexpr S*		T_get_enclosing(T *p, T S::*t)				{ return (S*)((char*)p - T_get_member_offset(t)); }
template<typename S, typename T>	constexpr const S*	T_get_enclosing(const T *p, T S::*t)		{ return (S*)((char*)p - T_get_member_offset(t)); }
#define ISO_GETMEMBERTYPE(p) deref_t<decltype(&p)>

template<typename S, typename T>	constexpr uintptr_t	BIT_OFFSET(T S::*t)							{ return T_get_member_offset(t) * 8; }


//-----------------------------------------------------------------------------
//	function helpers
//-----------------------------------------------------------------------------

template<typename R, typename...P> type_list<P...> func_helper(R (*)(P...));
template<typename F, typename R, typename...P> type_list<P...> func_helper(R (F::*)(P...));
template<typename F, typename R, typename...P> type_list<P...> func_helper(R (F::*)(P...) const);
template<typename F> auto func_helper(F&&)->decltype(func_helper(&noref_t<F>::operator()));

template<typename F>	using params_t = decltype(func_helper(declval<F>()));

template<typename F, typename... PP>			struct T_returns;
template<typename R, typename... PP>			struct T_returns<R(PP...)>				: T_type<R> {};
template<class T, typename R, typename... PP>	struct T_returns<R(T::*)(PP...)>		: T_type<R> {};
template<class T, typename R, typename... PP>	struct T_returns<R(T::*)(PP...) const>	: T_type<R> {};
template<typename F>							struct T_returns<F*>					: T_returns<F> {};
template<typename F>							struct T_returns<F&>					: T_returns<F> {};
template<typename F>							struct T_returns<const F&>				: T_returns<F> {};
template<typename F>							struct T_returns<F>						: T_returns<decltype(&noref_t<F>::operator())> {};
template<typename F, typename... PP>			struct T_returns						: T_type<decltype(declval<F>()(declval<PP>()...))> {};
template<class T>	using return_t = typename T_returns<T>::type;

#if 0
template<size_t N, typename T0, typename... TT>	struct PP_index_imp					{ static decltype(auto) f(T0&&, TT&&...p)		{ return PP_index_imp<N - 1, TT...>::f(forward<TT>(p)...); } };
template<typename T0, typename... TT>			struct PP_index_imp<0, T0, TT...>	{ static decltype(auto) f(T0&& p0, TT&&...p)	{ return forward<T0>(p0); } };
template<size_t N, typename... TT>	decltype(auto) PP_index(TT&&...p)	{ return PP_index_imp<N, TT...>::f(forward<TT>(p)...); }
#else

template<typename T>		struct make_temp_ptr_imp {
	static auto f(T& t) { return &t; }
};
template<typename T>		struct make_temp_ptr_imp<T&> {
	struct temp;
	static auto f(T& t) { return (temp*)&t; }
	friend constexpr T&		deref_temp(temp *t) 		{ return *(T*)t; }
};
template<typename T>		struct	make_temp_ptr_imp<T&&> {
	struct temp;
	static auto f(T& t) { return (temp*)&t; }
	friend constexpr T&&	deref_temp(temp *t) 		{ return move(*(T*)t); }
};
template<typename T, int N>	struct	make_temp_ptr_imp<T(&)[N]> {
	struct temp;
	static auto f(T (&t)[N]) { return (temp*)t; }
	friend constexpr T		(&deref_temp(temp *t))[N] 	{ return *(T(*)[N])t; }
};

template<typename T> constexpr T& deref_temp(T *t)		{ return *t; }

template<size_t N, typename ...T> auto PP_index(T&&...t) {
	return deref_temp(meta::param_index_impl<N>::f(make_temp_ptr_imp<T&&>::f(t)...));
}

#endif

template<typename T> struct return_holder {
	typedef	T&	type;
	T			*v;
	return_holder() : v(0) {}
	return_holder(T &_v) : v(&_v) {}
	operator T&()	const { return *v; }
};

template<typename T>		struct T_return_holder						: T_type<T> {};
template<typename T>		struct T_return_holder<T&>					: T_type<return_holder<T>> {};
template<typename T>		struct T_return_holder<return_holder<T>&>	: T_type<return_holder<T>> {};
template<typename T>		using return_holder_t = typename T_return_holder<T>::type;

template<typename T>		struct T_param_holder						: T_type<T> {};
template<typename T>		struct T_param_holder<T*&>					: T_type<T*> {};
template<typename T>		struct T_param_holder<T* const&>			: T_type<T*> {};
template<typename T>		struct T_param_holder<const T*&>			: T_type<const T*> {};
template<typename T>		struct T_param_holder<const T* const&>		: T_type<const T*> {};
template<typename T>		struct T_param_holder<T&&>					: T_type<T> {};
template<typename T, int N>	struct T_param_holder<T[N]>					: T_type<T*> {};
template<typename T>		using param_holder_t = typename T_param_holder<T>::type;

#define param(T)			iso::T_param<T>::type
#define paramT(T)			typename iso::T_param<T>::type

template<typename T, T N> struct held_constant {
	T t;
	constexpr held_constant() : t(N) {}
	constexpr operator T() const { return t; }
};

//-----------------------------------------------------------------------------
//	templated functions
//-----------------------------------------------------------------------------
template<typename T> struct T_underlying : T_type<T> {};
template<typename T> using underlying_t = typename T_underlying<T>::type;

template<typename T> inline			void*	allocate()								{ return aligned_alloc(sizeof(T), alignof(T));}
template<typename T, typename... A> T&		construct(T& t, A&&... a)				{ return *(new (__builtin_addressof(t)) T(forward<A>(a)...)); }
template<typename T, int N>			T&		construct(T (&t)[N])					{ return *(new (&t) T); }
template<typename T>				void	destruct(T& t)							{ t.~T(); }

template<typename T> inline			T*		allocate(size_t n)						{ return (T*)aligned_alloc(n * sizeof(T), alignof(T)); }
template<typename T> inline			void	deallocate(T *p, size_t n)				{ aligned_free((void*)p); }
template<typename T> inline			T*		reallocate(T *p, size_t n0, size_t n1)	{ return (T*)aligned_realloc((void*)p, n1 * sizeof(T), alignof(T)); }
template<typename T> inline			T*		resize(T *p, size_t n0, size_t n1)		{ return (T*)aligned_resize((void*)p, n1 * sizeof(T), alignof(T)); }

template<typename I> inline enable_if_t< is_trivially_destructible_v<deref_t<I>>>	destruct(I i, size_t n) {}
template<typename I> inline enable_if_t<!is_trivially_destructible_v<deref_t<I>>>	destruct(I i, size_t n) {
	while (n--)
		destruct(*i++);
}
//template<typename T, int N> inline void		destruct(T (*p)[N], size_t n)			{ destruct(&(*p)[0], n * N); }

template<typename S, typename D> inline void move(S&& s, D& d)						{ d = move(s); }
template<typename S, typename D> inline void move_new(S&& s, D& d)					{ construct(d, move(s)); destruct(s); }
template<typename S, typename D> inline void copy_new(S&& s, D& d)					{ construct(d, forward<S>(s)); }

struct discard { template<typename... T>	discard(T&&...t) {} };

template<typename... T>				inline		void		unused(T&&... t)				{}
template<typename A, typename B>	inline		void		assign(A &a, const B &b)		{ a = b; }
template<typename A, typename B>	inline		void		raw_copy(const A &a, B &b)		{ memcpy(&b, &a, sizeof(A) < sizeof(B) ? sizeof(A) : sizeof(B)); }
template<typename T>				inline		void		raw_swap(T &a, T &b)			{ uint8 t[sizeof(T)]; memcpy(t, &a, sizeof(T)); memcpy(&a, &b, sizeof(T)); memcpy(&b, t, sizeof(T)); }
template<typename A, typename B> 	constexpr 	int			simple_compare(const A &a, const B &b)	{ return a < b ? -1 : a == b ? 0 : 1; }
template<typename T>				inline		int			raw_compare(const T &a, const T &b)	{ return memcmp(&a, &b, sizeof(T)); }
template<typename A, typename B>	inline		void		swap(A &a, B &b)				{ A t(move(a)); a = move(b); b = move(t); }
template<typename A, typename B, typename C> inline void	permute3(A &a, B &b, C &c)		{ swap(a, b); swap(a, c); }
template<typename A, typename B>	inline		A			exchange(A &a, const B &b)		{ A t(move(a)); a = b; return t; }
template<typename A, typename B>	inline		A			exchange(A &a, B &&b)			{ A t(move(a)); a = forward<B>(b); return t;}
template<typename T>				inline		void 		clear(T &t)						{ memset(&t, 0, sizeof(T)); }
template<typename T>				inline		void 		clear(T *t, int n)				{ memset(t, 0, sizeof(T) * n); }
template<class I>					inline		void		clear(I first, I last)			{ for (; first != last; ++first) clear(*first);}

template<class T>	inline enable_if_t<is_trivially_copyable_v<T>> clear(T* first, T* last) {
	if (last > first)
		memset(first, 0, (char*)last - (char*)first);
}


template<typename T> constexpr T*		min(T *a,	void *b)	{ return a < b ? a : (T*)b; }
template<typename T> constexpr T*		max(T *a,	void *b)	{ return a > b ? a : (T*)b; }
template<typename T> constexpr const T* min(const T *a, const void *b)	{ return a < b ? a : (const T*)b; }
template<typename T> constexpr const T* max(const T *a, const void *b)	{ return a > b ? a : (const T*)b; }

template<typename T> constexpr int		min(int a,		T b)	{ return a < int	(b) ? a : int	(b); }
template<typename T> constexpr uint32	min(uint32 a,	T b)	{ return a < uint32	(b) ? a : uint32(b); }
template<typename T> constexpr int		max(int a,		T b)	{ return a > int	(b) ? a : int	(b); }
template<typename T> constexpr uint32	max(uint32 a,	T b)	{ return a > uint32	(b) ? a : uint32(b); }

template<typename T> constexpr float	min(float a,	T b)	{ return a > float	(b) || a != a ? float	(b) : a; }
template<typename T> constexpr double	min(double a,	T b)	{ return a > double	(b) || a != a ? double	(b) : a; }
template<typename T> constexpr float	max(float a,	T b)	{ return a < float	(b) || a != a ? float	(b) : a; }
template<typename T> constexpr double	max(double a,	T b)	{ return a < double	(b) || a != a ? double	(b) : a; }
#if USE_LONG
template<typename T> constexpr ulong	min(ulong a,	T b)	{ return a < ulong	(b) ? a : ulong	(b); }
template<typename T> constexpr long		min(long a,		T b)	{ return a < long	(b) ? a : long	(b); }
template<typename T> constexpr ulong	max(ulong a,	T b)	{ return a > ulong	(b) ? a : ulong	(b); }
template<typename T> constexpr long		max(long a,		T b)	{ return a > long	(b) ? a : long	(b); }
#endif
#if USE_64BITREGS
template<typename T> constexpr int64	min(int64 a,	T b)	{ return a < int64	(b) ? a : int64	(b); }
template<typename T> constexpr uint64	min(uint64 a,	T b)	{ return a < uint64	(b) ? a : uint64(b); }
template<typename T> constexpr int64	max(int64 a,	T b)	{ return a > int64	(b) ? a : int64	(b); }
template<typename T> constexpr uint64	max(uint64 a,	T b)	{ return a > uint64	(b) ? a : uint64(b); }
#endif

constexpr int		mod(int a,		int b)		{ return a % b; }
constexpr uint32	mod(uint32 a,	uint32 b)	{ return a % b; }

template<typename T> constexpr enable_if_t<is_builtin_num<T>, int> 	sign(T a)						{ return a < 0 ? -1 : a == 0 ? 0 : 1; }
template<typename T> constexpr enable_if_t<is_builtin_num<T>, int> 	sign1(T a)						{ return a < 0 ? -1 : 1; }

template<typename T, typename A>	constexpr enable_if_t<has_int_ops<T>, T> align(T x, A a)		{ return T((x + a - 1) / a * a); }
template<typename T, typename A>	constexpr enable_if_t<has_int_ops<T>, T> align_down(T x, A a)	{ return T(x / a * a); }
template<typename T, typename A>	constexpr enable_if_t<is_int<T>, bool>	is_aligned(T x, A a)	{ return x % a == 0; }
template<typename T, typename A>				constexpr T*	align(T *t, A a)					{ return (T*)align(uintptr_t(t), a); }
template<typename T, typename A>				constexpr T*	align_down(T *t, A a)				{ return (T*)align_down(uintptr_t(t), a); }
template<typename T, typename A>				constexpr bool	is_aligned(T *t, A a)				{ return is_aligned(uintptr_t(t), a); }

template<typename T>							constexpr T		round_pow2(T t, uint32 n)			{ return (t + (1 << (n - 1))) >> n; }
template<typename T>							constexpr T		ceil_pow2(T t, uint32 n)			{ return (t + (1 << n ) - 1) >> n; }
template<typename T>							constexpr T		floor_pow2(T t, uint32 n)			{ return t >> n; }

template<typename T>	constexpr enable_if_t<has_int_ops<T>, T> align_pow2(T t, uint32 n)			{ return (t + ((T(1) << n) - 1)) & -(T(1) << n); }
template<typename T>	constexpr enable_if_t<has_int_ops<T>, T> align_down_pow2(T t, uint32 n)		{ return t & -(T(1) << n); }
template<typename T>	constexpr enable_if_t<is_int<T>, bool>	is_aligned_pow2(T x, uint32 n)		{ return (x & ((T(1) << n) - 1)) == 0; }
template<typename T>							constexpr T*	align_pow2(T *t, uint32 n)			{ return (T*)align_pow2(uintptr_t(t), n); }
template<typename T>							constexpr T*	align_down_pow2(T *t, uint32 n)		{ return (T*)align_down_pow2(uintptr_t(t), n); }
template<typename T>							constexpr bool	is_aligned_pow2(T *t, uint32 n)		{ return is_aligned_pow2(uintptr(t), n); }

template<typename T, typename A, typename B>	constexpr bool	between(const T &v, const A &a, const B &b)	{ return v >= a && v <= b; }
template<typename A, typename B>				constexpr A		wrap_neg(const A &a, const B &b)			{ return a < 0 ? a + b : a; }
template<typename A, typename B>				constexpr A		wrap(const A &a, const B &b)				{ return wrap_neg(a % b, b); }
template<typename A, typename B, typename C>	constexpr A		wrap(const A &a, const B &b, const C &c)	{ return wrap(a - b, c - b) + b; }
template<typename A, typename B>				constexpr A		inc_mod(A a, B b)							{ return a + 1 == b ? 0 : a + 1; }
template<typename A, typename B>				constexpr A		dec_mod(A a, B b)							{ return a == 0 ? b - 1 : a - 1; }
template<typename A>							constexpr auto	square(const A &a)							{ return a * a;}
template<typename A>							constexpr A		cube(const A &a)							{ return a * a * a; }
template<typename A>							constexpr A		twice(const A &a)							{ return a + a; }

template<typename A>							constexpr auto	neg(A a)									{ return -a; }
template<typename A, typename B>				constexpr auto	add(A a, B b)								{ return a + b; }
template<typename A, typename B>				constexpr auto	sub(A a, B b)								{ return a - b; }
template<typename A, typename B>				constexpr auto	mul(A a, B b)								{ return a * b; }
template<typename A, typename B>				constexpr auto	div(A a, B b)								{ return a / b; }
template<typename A, typename B, typename C>	constexpr auto	madd(A a, B b, C c)							{ return add(mul(a, b), c); }
template<typename A, typename B, typename C>	constexpr auto	nmsub(A a, B b, C c)						{ return sub(c, mul(a, b)); }


constexpr bool all(bool b) { return b; }
constexpr bool any(bool b) { return b; }

template<typename X, typename Y> constexpr auto select(bool mask, X x, Y y) { return mask ? x : y; }


//-----------------------------------------------------------------------------
//	constructable
//-----------------------------------------------------------------------------

template<typename T> struct constructable : T {
	typedef typename T_underlying<T>::type T0;
	constructable() {}
	constructable(const T0 &x)										{ T::operator=(x); }
	template<typename T2> constructable&	operator=(const T2 &x)	{ T::operator=(x); return *this; }
	friend constexpr const T&	get(const constructable &t)			{ return t; }
	friend constexpr T&			put(constructable &t)				{ return t; }
};
template<typename T> struct T_underlying<constructable<T> > : T_underlying<T> {};

template<typename T> struct num_traits<constructable<T> > : num_traits<T> {
	template<typename T2> static constexpr constructable<T> cast(const T2 &t)	{ return constructable<T>(t); }
};
template<typename T> struct T_isint<constructable<T> >			: T_isint<T> {};

//-----------------------------------------------------------------------------
//	packed
//-----------------------------------------------------------------------------

#if (defined PLAT_WII && !defined __GNUC__) || defined PLAT_PS3

template<typename T> class _packed {
	uint8	t[sizeof(T)];
	T&		get(T &a)	const						{ memcpy(&a, t, sizeof(T)); return a; }
protected:
	T		get()		const						{ T a; get(a); return a; }
	void	set(const T &a)							{ memcpy(t, &a, sizeof(T)); }
public:
	operator T()		const						{ return get(); }
	void operator=(const T &_t)						{ set(_t); }
	const T operator->() const						{ return get(); }
	T operator++()									{ T a; set(++get(a)); return a; }
	T operator--()									{ T a; set(--get(a)); return a; }
	T operator++(int)								{ T a, b = get(a); set(++b); return a; }
	T operator--(int)								{ T a, b = get(a); set(--b); return a; }
	T operator+=(const T &_t)						{ T a; get(a); set(a += _t); return a; }
	T operator-=(const T &_t)						{ T a; get(a); set(a -= _t); return a; }
	T operator*=(const T &_t)						{ T a; get(a); set(a *= _t); return a; }
	T operator/=(const T &_t)						{ T a; get(a); set(a /= _t); return a; }
	friend T	get(const _packed &a)				{ return a; }
};

#elif defined __GNUC__
// THIS WASN'T WORKING FOR packed floats so we are using the WII version.

template<typename T> class _packed {
	T		t;
protected:
	T		get()	const							{ return t; }
	template<typename X> void set(const X &x)		{ t = x; }
public:
	operator T()	const							{ return get(); }
	template<typename X> void operator=(const X &x)	{ set(x); }
	T& operator->()									{ return t; }
	T operator++()									{ return ++t; }
	T operator++(int)								{ return t++; }
	T operator--()									{ return --t; }
	T operator--(int)								{ return t--; }
	T operator+=(const T &_t)						{ return t += _t; }
	T operator-=(const T &_t)						{ return t -= _t; }
	T operator*=(const T &_t)						{ return t *= _t; }
	T operator/=(const T &_t)						{ return t /= _t; }
	friend T	get(const _packed &a)				{ return a; }
} __attribute__((packed));

#else

template<typename T> class _packed {
	uint8		t[sizeof(T)];
protected:
	T		get()	const								{ return *(iso_unaligned(T*))t; }
	template<typename X> void set(const X &x)			{ *(iso_unaligned(T*))t = x; }
public:
	operator T()	const								{ return get(); }
	template<typename X> void operator=(const X &x)		{ set(x); }
	auto operator->()	const							{ return (iso_unaligned(const T*))t;	}
	auto operator->()									{ return (iso_unaligned(T*))t;			}
	T operator++()										{ return ++*(iso_unaligned(T*))t;		}
	T operator++(int)									{ return (*(iso_unaligned(T*))t)++;		}
	T operator--()										{ return --*(iso_unaligned(T*))t;		}
	T operator--(int)									{ return (*(iso_unaligned(T*))t)--;		}
	template<typename T2> T operator+=(const T2 &_t)	{ return *(iso_unaligned(T*))t += _t;	}
	template<typename T2> T operator-=(const T2 &_t)	{ return *(iso_unaligned(T*))t -= _t;	}
	template<typename T2> T operator*=(const T2 &_t)	{ return *(iso_unaligned(T*))t *= _t;	}
	template<typename T2> T operator/=(const T2 &_t)	{ return *(iso_unaligned(T*))t /= _t;	}
	friend T get(const _packed &a)	{ return a; }
};

#endif

template<typename T> struct T_underlying<_packed<T> > : T_underlying<T> {};

template<typename T> struct packed : _packed<T> {
	packed()						{}
	packed(const T &_t)				{ this->set(_t); }
	void operator=(const T &_t)		{ this->set(_t); }
	friend T get(const packed &a)	{ return a; }
	friend T put(packed &a)			{ return a; }
};

template<typename T> struct packed<constructable<T> > : _packed<T> {
	packed()					{}
	packed(const T &_t)			{ this->set(_t); }
	void operator=(const T &_t)	{ this->set(_t); }
};

template<typename T> struct T_isint<packed<T> >			: T_isint<T> {};
template<typename T> struct T_underlying<packed<T> >	: T_underlying<T> {};
template<typename T> struct num_traits<packed<T> >		: num_traits<T> {};

template<typename T> inline auto	load_packed(const void *p)			{ return get(*(const packed<T>*)p); }
template<typename T> inline auto	load_packed_inc(const uint8 *&p)	{ const uint8 *p0 = p; p = p0 + sizeof(T); return get(*(const packed<T>*)p0); }
template<typename T> inline void	store_packed(void *p, T t)			{ *(packed<T>*)p = t; }
template<typename T> inline void	store_packed_inc(uint8 *p, T t)		{ *(packed<T>*)p = t; p += sizeof(T); }
template<typename T> inline void	copy_packed(void *d, const void *s)	{ *(packed<T>*)d = *(const packed<T>*)s; }

//-----------------------------------------------------------------------------
//	comparisons
//-----------------------------------------------------------------------------
// with any type
template<typename T, bool ANY = true> struct comparisons {
	template<typename U> friend constexpr bool operator==(const T &a, const U &b) { return compare(a, b) == 0; }
	template<typename U> friend constexpr bool operator!=(const T &a, const U &b) { return compare(a, b) != 0; }
	template<typename U> friend constexpr bool operator< (const T &a, const U &b) { return compare(a, b) <  0; }
	template<typename U> friend constexpr bool operator<=(const T &a, const U &b) { return compare(a, b) <= 0; }
	template<typename U> friend constexpr bool operator> (const T &a, const U &b) { return compare(a, b) >  0; }
	template<typename U> friend constexpr bool operator>=(const T &a, const U &b) { return compare(a, b) >= 0; }
};

// with T only
template<typename T> struct comparisons<T, false> {
	friend constexpr bool operator==(const T &a, const T &b)		{ return compare(a, b) == 0; }
	friend constexpr bool operator!=(const T &a, const T &b)		{ return compare(a, b) != 0; }
	friend constexpr bool operator< (const T &a, const T &b)		{ return compare(a, b) <  0; }
	friend constexpr bool operator<=(const T &a, const T &b)		{ return compare(a, b) <= 0; }
	friend constexpr bool operator> (const T &a, const T &b)		{ return compare(a, b) >  0; }
	friend constexpr bool operator>=(const T &a, const T &b)		{ return compare(a, b) >= 0; }
};


//-----------------------------------------------------------------------------
//	constants
//-----------------------------------------------------------------------------

template<typename K, typename T, typename V = void> struct constant_cast;

template<typename K, typename T> struct constant_cast<K, T, enable_if_t<is_builtin_num<T> > > {
	static constexpr T f()	{ return K::template as<T>(); }
};

template<typename T, typename=void> constexpr bool use_constants = is_builtin_num<T>;

template<typename K> struct constant_type : T_type<float>	{};
template<typename K, typename B, typename = void>	struct co_type : constant_type<K> {};
template<typename K>								struct co_type<K, double> : T_type<double> {};

template<typename K> struct constant {
	template<typename B> using co_t = typename co_type<K, B>::type;

	constexpr constant() {}
	template<typename R> static constexpr R as()			{ return constant_cast<K, R>::f(); }
	template<typename B> constexpr auto		co()	const	{ return as<co_t<B>>(); }

	template<typename R, typename X = decltype(constant_cast<K, R>::f())> constexpr operator R() const { return constant_cast<K, R>::f(); }
};


}//namespace iso

#endif // DEFS_BASE_H
