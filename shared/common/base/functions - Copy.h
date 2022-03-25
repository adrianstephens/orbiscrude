#ifndef FUNCTION_H
#define FUNCTION_H

#include "defs.h"
#include "type_list.h"

#ifndef USE_STDCALL
#define __stdcall
#endif

namespace iso {

//-----------------------------------------------------------------------------
//	function - type inspector for functions
//-----------------------------------------------------------------------------

template<typename F> struct function : function<decltype(&F::operator())> {};
template<typename F> struct function<F*> : function<F> {};

#ifdef USE_VARIADIC_TEMPLATES

// types of functions
template<typename r, typename... pp> struct function<r(pp...)> { 
	enum	{ N = sizeof...(pp) };
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (F)(pp...);
#ifdef USE_STDCALL
	typedef R (__stdcall FS)(pp...);
#endif
};

// types of member functions
template<class t, typename r, typename... pp> struct function<r(t::*)(pp...)> {
	enum	{ N = sizeof...(pp) };
	typedef t				T; 
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (t::*F)(pp...);

	template<F f, class T>							static	r	to_static(T *me, pp... p)		{ return (static_cast<t*>(me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(T *me, PP... p)		{ return (static_cast<t*>(me)->*f)(p...); }
	template<F f, class T>							static	r	to_static(void *me, pp... p)	{ return (static_cast<t*>((T*)me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(void *me, PP... p)	{ return (static_cast<t*>((T*)me)->*f)(p...); }
};

template<class t, typename r, typename... pp> struct function<r(t::*)(pp...) const> {
	enum	{ N = sizeof...(pp) };
	typedef const t			T; 
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (t::*F)(pp...) const;
	
	template<F f, class T>							static	r	to_static(const T *me, pp... p)		{ return (static_cast<const t*>(me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(const T *me, PP... p)		{ return (static_cast<const t*>(me)->*f)(p...); }
	template<F f, class T>							static	r	to_static(const void *me, pp... p)	{ return (static_cast<const t*>((T*)me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(const void *me, PP... p)	{ return (static_cast<const t*>((T*)me)->*f)(p...); }
};

#ifdef USE_STDCALL

// types of member functions
template<class t, typename r, typename... pp> struct function<r(__stdcall t::*)(pp...)> {
	enum	{ N = sizeof...(pp) };
	typedef t				T; 
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (t::*F)(pp...);

	template<F f, class T>							static	r	to_static(T *me, pp... p)		{ return (static_cast<t*>(me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(T *me, PP... p)		{ return (static_cast<t*>(me)->*f)(p...); }
	template<F f, class T>							static	r	to_static(void *me, pp... p)	{ return (static_cast<t*>((T*)me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(void *me, PP... p)	{ return (static_cast<t*>((T*)me)->*f)(p...); }
};

template<class t, typename r, typename... pp> struct function<r(__stdcall t::*)(pp...) const> {
	enum	{ N = sizeof...(pp) };
	typedef const t			T; 
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (t::*F)(pp...) const;
	
	template<F f, class T>							static	r	to_static(const T *me, pp... p)		{ return (static_cast<const t*>(me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(const T *me, PP... p)		{ return (static_cast<const t*>(me)->*f)(p...); }
	template<F f, class T>							static	r	to_static(const void *me, pp... p)	{ return (static_cast<const t*>((T*)me)->*f)(p...); }
	template<F f, class T, class R, typename... PP>	static	R	to_static(const void *me, PP... p)	{ return (static_cast<const t*>((T*)me)->*f)(p...); }
};

#endif

// types of functions defined by typelists
template<typename r, typename L> struct function_p;
template<typename r, typename... pp> struct function_p<r, type_list<pp...> > : function<r(pp...)>	{};

// get type of function with first parameter removed
template<typename F> struct strip_param1;
template<typename F> struct strip_param1<F*> : strip_param1<F> {};
template<typename r, typename p1, typename... pp> struct strip_param1<r(p1, pp...)> : function<r(pp...)> {};

#else

// types of functions
//#define DEF(n,P)	enum {N = n}; typedef r R; typedef R (F)P; F *f
#ifdef USE_STDCALL
#define DEF(n,P)	enum {N = n}; typedef r R; typedef R (F)P; typedef R (__stdcall FS)P; F *f
#else
#define DEF(n,P)	enum {N = n}; typedef r R; typedef R (F)P; F *f
#endif

template<typename r> struct function<r(void)> { DEF(0, (void)); typedef type_list<> P; };
template<typename r, typename P1> struct function<r(P1)> { DEF(1, (P1)); typedef type_list<P1> P; };
template<typename r, typename P1, typename P2> struct function<r(P1,P2)> { DEF(2, (P1,P2)); typedef type_list<P1,P2> P; };
template<typename r, typename P1, typename P2, typename P3> struct function<r(P1,P2,P3)> { DEF(3, (P1,P2,P3)); typedef type_list<P1,P2,P3> P; };
template<typename r, typename P1, typename P2, typename P3, typename P4> struct function<r(P1,P2,P3,P4)> { DEF(4, (P1,P2,P3,P4)); typedef type_list<P1,P2,P3,P4> P; };
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5> struct function<r(P1,P2,P3,P4,P5)> { DEF(5, (P1,P2,P3,P4,P5)); typedef type_list<P1,P2,P3,P4,P5> P; };
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> struct function<r(P1,P2,P3,P4,P5,P6)> { DEF(6, (P1,P2,P3,P4,P5,P6)); typedef type_list<P1,P2,P3,P4,P5,P6> P; };
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> struct function<r(P1,P2,P3,P4,P5,P6,P7)> { DEF(7, (P1,P2,P3,P4,P5,P6,P7)); typedef type_list<P1,P2,P3,P4,P5,P6,P7> P; };
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> struct function<r(P1,P2,P3,P4,P5,P6,P7,P8)> { DEF(8, (P1,P2,P3,P4,P5,P6,P7,P8)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8> P; };
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> struct function<r(P1,P2,P3,P4,P5,P6,P7,P8,P9)> { DEF(9, (P1,P2,P3,P4,P5,P6,P7,P8,P9)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9> P; };
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> struct function<r(P1,P2,P3,P4,P5,P6,P7,P8,P9,P10)> { DEF(10, (P1,P2,P3,P4,P5,P6,P7,P8,P9,P10)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9,P10> P; };
#undef DEF

// types of member functions
#define DEF(n,P)	enum {N = n}; typedef t T; typedef r R; typedef R (t::*F)P; F f
template<class t, typename r> struct function<r(t::*)(void)> { DEF(0, (void)); typedef type_list<> P; };
template<class t, typename r, typename P1> struct function<r(t::*)(P1)> { DEF(1, (P1)); typedef type_list<P1> P; };
template<class t, typename r, typename P1, typename P2> struct function<r(t::*)(P1,P2)> { DEF(2, (P1,P2)); typedef type_list<P1,P2> P; };
template<class t, typename r, typename P1, typename P2, typename P3> struct function<r(t::*)(P1,P2,P3)> { DEF(3, (P1,P2, P3)); typedef type_list<P1,P2,P3> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4> struct function<r(t::*)(P1,P2,P3,P4)> { DEF(4, (P1,P2,P3,P4)); typedef type_list<P1,P2,P3,P4> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5> struct function<r(t::*)(P1,P2,P3,P4,P5)> { DEF(5, (P1,P2,P3,P4,P5)); typedef type_list<P1,P2,P3,P4,P5> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> struct function<r(t::*)(P1,P2,P3,P4,P5,P6)> { DEF(6, (P1,P2,P3,P4,P5,P6)); typedef type_list<P1,P2,P3,P4,P5,P6> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7)> { DEF(7, (P1,P2,P3,P4,P5,P6,P7)); typedef type_list<P1,P2,P3,P4,P5,P6,P7> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7,P8)> { DEF(8, (P1,P2,P3,P4,P5,P6,P7,P8)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7,P8,P9)> { DEF(9, (P1,P2,P3,P4,P5,P6,P7,P8,P9)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7,P8,P9,P10)> { DEF(10, (P1,P2,P3,P4,P5,P6,P7,P8,P9,P10)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9,P10> P; };

template<class t, typename r> struct function<r(t::*)(void) const> { DEF(0, (void)); typedef type_list<> P; };
template<class t, typename r, typename P1> struct function<r(t::*)(P1) const> { DEF(1, (P1)); typedef type_list<P1> P; };
template<class t, typename r, typename P1, typename P2> struct function<r(t::*)(P1,P2) const> { DEF(2, (P1,P2)); typedef type_list<P1,P2> P; };
template<class t, typename r, typename P1, typename P2, typename P3> struct function<r(t::*)(P1,P2,P3) const> { DEF(3, (P1,P2, P3)); typedef type_list<P1,P2,P3> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4> struct function<r(t::*)(P1,P2,P3,P4) const> { DEF(4, (P1,P2,P3,P4)); typedef type_list<P1,P2,P3,P4> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5> struct function<r(t::*)(P1,P2,P3,P4,P5) const> { DEF(5, (P1,P2,P3,P4,P5)); typedef type_list<P1,P2,P3,P4,P5> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> struct function<r(t::*)(P1,P2,P3,P4,P5,P6) const> { DEF(6, (P1,P2,P3,P4,P5,P6)); typedef type_list<P1,P2,P3,P4,P5,P6> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7) const> { DEF(7, (P1,P2,P3,P4,P5,P6,P7)); typedef type_list<P1,P2,P3,P4,P5,P6,P7> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7,P8) const> { DEF(8, (P1,P2,P3,P4,P5,P6,P7,P8)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7,P8,P9) const> { DEF(9, (P1,P2,P3,P4,P5,P6,P7,P8,P9)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9> P; };
template<class t, typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> struct function<r(t::*)(P1,P2,P3,P4,P5,P6,P7,P8,P9,P10) const> { DEF(10, (P1,P2,P3,P4,P5,P6,P7,P8,P9,P10)); typedef type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9,P10> P; };
#undef DEF

// types of functions defined by typelists
template<typename r, typename L> struct function_p;
template<typename r, typename P1> struct function_p<r, type_list<P1> > : function<r(P1)>	{};
template<typename r, typename P1, typename P2> struct function_p<r, type_list<P1,P2> > : function<r(P1,P2)> {};
template<typename r, typename P1, typename P2, typename P3> struct function_p<r, type_list<P1,P2,P3> > : function<r(P1,P2,P3)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4> struct function_p<r, type_list<P1,P2,P3,P4> > : function<r(P1,P2,P3,P4)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5> struct function_p<r, type_list<P1,P2,P3,P4,P5> > : function<r(P1,P2,P3,P4,P5)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> struct function_p<r, type_list<P1,P2,P3,P4,P5,P6> > : function<r(P1,P2,P3,P4,P5,P6)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> struct function_p<r, type_list<P1,P2,P3,P4,P5,P6,P7> > : function<r(P1,P2,P3,P4,P5,P6,P7)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> struct function_p<r, type_list<P1,P2,P3,P4,P5,P6,P7,P8> > : function<r(P1,P2,P3,P4,P5,P6,P7,P8)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> struct function_p<r, type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9> > : function<r(P1,P2,P3,P4,P5,P6,P7,P8,P9)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> struct function_p<r, type_list<P1,P2,P3,P4,P5,P6,P7,P8,P9,P10> > : function<r(P1,P2,P3,P4,P5,P6,P7,P8,P9,P10)> {};

// get type of function with first parameter removed
template<typename F> struct strip_param1;
template<typename F> struct strip_param1<F*> : strip_param1<F> {};
template<typename r, typename P1> struct strip_param1<r(P1)> : function<r(void)> {};
template<typename r, typename P1, typename P2> struct strip_param1<r(P1,P2)> : function<r(P2)> {};
template<typename r, typename P1, typename P2, typename P3> struct strip_param1<r(P1,P2,P3)> : function<r(P2,P3)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4> struct strip_param1<r(P1,P2,P3,P4)> : function<r(P2,P3,P4)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5> struct strip_param1<r(P1,P2,P3,P4,P5)> : function<r(P2,P3,P4,P5)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> struct strip_param1<r(P1,P2,P3,P4,P5,P6)> : function<r(P2,P3,P4,P5,P6)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> struct strip_param1<r(P1,P2,P3,P4,P5,P6,P7)> : function<r(P2,P3,P4,P5,P6,P7)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> struct strip_param1<r(P1,P2,P3,P4,P5,P6,P7,P8)> : function<r(P2,P3,P4,P5,P6,P7,P8)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> struct strip_param1<r(P1,P2,P3,P4,P5,P6,P7,P8,P9)> : function<r(P2,P3,P4,P5,P6,P7,P8,P9)> {};
template<typename r, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> struct strip_param1<r(P1,P2,P3,P4,P5,P6,P7,P8,P9,P10)> : function<r(P2,P3,P4,P5,P6,P7,P8,P9,P10)> {};

#endif

// call a function using tree of parameters
template<int N> struct call_s;
template<> struct call_s<0> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(); } };
template<> struct call_s<1> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a); } };
template<> struct call_s<2> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a); } };
template<> struct call_s<3> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a); } };
template<> struct call_s<4> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a, p.b.b.b.a); } };
template<> struct call_s<5> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a, p.b.b.b.a, p.b.b.b.b.a); } };
template<> struct call_s<6> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a, p.b.b.b.a, p.b.b.b.b.a, p.b.b.b.b.b.a); } };
template<> struct call_s<7> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a, p.b.b.b.a, p.b.b.b.b.a, p.b.b.b.b.b.a, p.b.b.b.b.b.b.a); } };
template<> struct call_s<8> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a, p.b.b.b.a, p.b.b.b.b.a, p.b.b.b.b.b.a, p.b.b.b.b.b.b.a, p.b.b.b.b.b.b.b.a); } };
template<> struct call_s<9> { template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a, p.b.b.b.a, p.b.b.b.b.a, p.b.b.b.b.b.a, p.b.b.b.b.b.b.a, p.b.b.b.b.b.b.b.a, p.b.b.b.b.b.b.b.b.a); } };
template<> struct call_s<10>{ template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.a, p.b.a, p.b.b.a, p.b.b.b.a, p.b.b.b.b.a, p.b.b.b.b.b.a, p.b.b.b.b.b.b.a, p.b.b.b.b.b.b.b.a, p.b.b.b.b.b.b.b.b.a, p.b.b.b.b.b.b.b.b.b.a); } };
template<typename F,typename P> inline_only typename function<F>::R call(F f, const P &p) { return call_s<function<F>::N>::go(f, p); }

// call a function using tuple
#ifdef USE_VARIADIC_TEMPLATES

// call a function using tuple
template<int O, typename F, typename P, size_t... II> inline_only typename function<F>::R call(F f, const TL_tuple<P> &p, index_list<II...>&&) {
	return f(p.template get<O+II>()...);
}
template<typename F,typename P>			inline_only typename function<F>::R call(F f, const TL_tuple<P> &p)	{ return call<0>(f, p, make_index_list<function<F>::N>()); }
template<int O, typename F,typename P>	inline_only typename function<F>::R call(F f, const TL_tuple<P> &p)	{ return call<O>(f, p, make_index_list<function<F>::N>()); }

// call a member function using tuple
template<int O, class T, typename F, typename P, size_t... II> inline_only typename function<F>::R call(T &t, F f, const TL_tuple<P> &p, index_list<II...>&&) {
	return (t.*f)(p.template get<O+II>()...);
}
template<class T, typename F,typename P>		inline_only typename function<F>::R call(T &t, F f, const TL_tuple<P> &p)	{ return call<0>(t, f, p, make_index_list<function<F>::N>()); }
template<class T, int O, typename F,typename P>	inline_only typename function<F>::R call(T &t, F f, const TL_tuple<P> &p)	{ return call<O>(t, f, p, make_index_list<function<F>::N>()); }

#else

template<int O, int N> struct call_tuple_s;
template<int O> struct call_tuple_s<O,0> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(); }
};
template<int O> struct call_tuple_s<O,1> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>()); }
};
template<int O> struct call_tuple_s<O,2> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>()); }
};
template<int O> struct call_tuple_s<O,3> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>()); }
};
template<int O> struct call_tuple_s<O,4> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>()); }
};
template<int O> struct call_tuple_s<O,5> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>()); }
};
template<int O> struct call_tuple_s<O,6> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>()); }
};
template<int O> struct call_tuple_s<O,7> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>()); }
};
template<int O> struct call_tuple_s<O,8> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>(), p.template get<O+7>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>(), p.template get<O+7>()); }
};
template<int O> struct call_tuple_s<O,9> {
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>(), p.template get<O+7>(), p.template get<O+8>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>(), p.template get<O+7>(), p.template get<O+8>()); }
};
template<int O> struct call_tuple_s<O,10>{
	template<typename F,typename P> static inline_only typename function<F>::R go(F f, const P &p) { return f(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>(), p.template get<O+7>(), p.template get<O+8>(), p.template get<O+9>()); }
	template<class T, typename F,typename P> static inline_only typename function<F>::R go(T &t, F f, const P &p) { return (t.*f)(p.template get<O>(), p.template get<O+1>(), p.template get<O+2>(), p.template get<O+3>(), p.template get<O+4>(), p.template get<O+5>(), p.template get<O+6>(), p.template get<O+7>(), p.template get<O+8>(), p.template get<O+9>()); }
};

// call a function using tuple
template<typename F,typename P> inline_only typename function<F>::R call(F f, const TL_tuple<P> &p)		{ return call_tuple_s<0, function<F>::N>::go(f, p); }
template<int O, typename F,typename P> inline_only typename function<F>::R call(F f, const TL_tuple<P> &p)	{ return call_tuple_s<O, function<F>::N>::go(f, p); }

// call a member function using tuple
template<class T, typename F,typename P> inline_only typename function<F>::R call(T &t, F f, const TL_tuple<P> &p)			{ return call_tuple_s<0, function<F>::N>::go(t, f, p); }
template<class T, int O, typename F,typename P> inline_only typename function<F>::R call(T &t, F f, const TL_tuple<P> &p)	{ return call_tuple_s<O, function<F>::N>::go(t, f, p); }

#endif

//-----------------------------------------------------------------------------
//	closure
//	function pointer and (some) parameters
//-----------------------------------------------------------------------------

#ifdef USE_VARIADIC_TEMPLATES
template<typename F> struct _closure;

template<typename R, typename P1, typename... PP> struct _closure<R(P1, PP...)> {
	typedef R				(*function_t)(P1, PP...);
	typedef	R				return_t;
	typedef	P1				param1_t;
	typedef tuple<PP...>	params_t;
	params_t				params;

	template<function_t f, size_t... II> return_t call(P1 p1, index_list<II...>) const {
		return f(p1, params.template get<II>()...);
	} 
	template<function_t f> return_t call(P1 p1) const { 
		return call<f>(p1, typename params_t::indices());
	} 
	_closure(PP&&... pp) : params(forward<PP>(pp)...) {}
};

template<typename R, typename C, typename P1, typename... PP> struct _closure<R (C::*)(P1, PP...)> {
	typedef R				(C::*function_t)(P1, PP...);
	typedef	R				return_t;
	typedef	P1				param1_t;
	typedef tuple<PP...>	params_t;
	C						&c;
	params_t				params;

	template<function_t f, size_t... II> return_t call(P1 p1, index_list<II...>) const {
		return (c.*f)(p1, params.template get<II>()...);
	} 
	template<function_t f> return_t call(P1 p1) const { 
		return call<f>(p1, typename params_t::indices());
	} 
	_closure(C &_c, PP&&... pp) : c(_c), params(forward<PP>(pp)...) {}
};

template<typename R, typename C, typename P1, typename... PP> struct _closure<R (C::*)(P1, PP...) const> {
	typedef R				(C::*function_t)(P1, PP...) const;
	typedef	R				return_t;
	typedef	P1				param1_t;
	typedef tuple<PP...>	params_t;
	const C					&c;
	params_t				params;

	template<function_t f, size_t... II> return_t call(P1 p1, index_list<II...>) const {
		return (c.*f)(p1, params.template get<II>()...);
	} 
	template<function_t f> return_t call(P1 p1) const {
		return call<f>(p1, make_index_list<P1::count>());
	}
	_closure(const C &_c, PP&&... pp) : c(_c), params(forward<PP>(pp)...) {}
};

template<typename F, F f> struct closure : _closure<F> {
	typedef _closure<F> B;
	template<typename...PP> closure(PP&&... pp) : B(pp...) {}
	typename B::return_t operator()(typename B::param1_t p1) const { return B::template call<f>(p1); }
};

template<class F> struct closure_maker {
	template<F f, typename...PP> static closure<F, f>	get(const PP&&... pp)	{ return closure<F,f>(pp...); }
};

#define make_closure(f)		T_get_class<closure_maker>(f)->template get<f>

//-----------------------------------------------------------------------------
//	lambda_container
//-----------------------------------------------------------------------------

template<typename C, typename F> struct lambda_container {
	C	c;
	F	f;

	class iterator {
		typedef typename container_traits<C>::iterator	I;
		I		i;
		const F	&f;
		typedef typename function<F>::R T;
	public:
		typedef T							element, reference;
		typedef random_access_iterator_t	iterator_category;
		iterator(const I &_i, const F &_f) : i(_i), f(_f)	{}
		element		operator*()					const	{ return f(*i); }
		iterator&	operator++()						{ ++i; return *this; }
		iterator	operator++(int)						{ iterator j(i, f); ++i; return j; }
		iterator&	operator--()						{ --i; return *this; }
		iterator	operator--(int)						{ iterator j(i, f); --i; return j; }
		iterator&	operator+=(int j)					{ i += j; return *this; }
		iterator&	operator-=(int j)					{ i -= j; return *this; }
		iterator	operator+(int j)			const	{ return iterator(i + j, f);	}
		iterator	operator-(int j)			const	{ return iterator(i - j, f);	}
		bool	operator==(const iterator &j)	const	{ return i == j.i;	}
		bool	operator!=(const iterator &j)	const	{ return i != j.i;	}
		int		operator-(const iterator &j)	const	{ return i - j.i;	}
	};
	lambda_container(const C &c, F &f) : c(c), f(f) {}
	iterator	begin()	const	{ return iterator(global_begin(c), f); }
	iterator	end()	const	{ return iterator(global_end(c), f); }
};

template<typename C, typename F> lambda_container<C, F> make_lambda_container(const C &c, F f) {
	return lambda_container<C,F>(c, f);
}

#endif

//-----------------------------------------------------------------------------
//	virtfunc - no object ptr
//	must inherit from this, because obj assumed relative to virtfunc itself
//-----------------------------------------------------------------------------

template<class F> class virtfunc;

#ifdef USE_VARIADIC_TEMPLATES

template<class R, typename... PP> class virtfunc<R(PP...)> {
protected:
	typedef R	F(PP...);
	R		(*f)(void*, PP...);
	template<class T>					static	R	thunk(void *me, PP... pp)	{ return (*static_cast<T*>((virtfunc*)me))(pp...);	}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, PP... pp)	{ return ((static_cast<T*>((virtfunc*)me))->*f)(pp...); }
public:
	inline_only R	operator()(PP... pp)		const	{ return (*f)((void*)this, pp...); }
	constexpr explicit	operator bool()			const	{ return !!f; }

	inline_only void bind(R (*_f)(void*, PP...))		{ f = _f;	}
	template<class T>	inline_only void bind()			{ f = thunk<T>;	}
	template<class T>	inline_only void bind(T *t)		{ f = thunk<T>;	}
	template<class T, F T::*f2>	inline_only void bind()	{ f = &thunk<T,F T::*,f2>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind()	{ f = &thunk<T,F2,f2>;	}

	constexpr virtfunc()						: f(NULL)		{}
	constexpr virtfunc(R (*_f)(void*, PP...))	: f(_f)			{}
	template<class T> constexpr virtfunc(T*)	: f(thunk<T>)	{}
};

template<class F, class L, class> struct lambda_virt;

template<class V, class L, typename X, typename R, typename... PP> struct lambda_virt<V, L, R (X::*)(PP...) const> : V {
	L	lambda;
	static	R	thunk(void *me, PP... pp)	{ return static_cast<lambda_virt*>((V*)me)->lambda(pp...);	}
	lambda_virt(L &&_lambda) : V(&thunk), lambda(move(_lambda)) {};
};

template<class V, class L, typename X, typename R, typename... PP> struct lambda_virt<V, L, R (X::*)(PP...)> : V {
	L	lambda;
	static	R	thunk(void *me, PP... pp)	{ return static_cast<lambda_virt*>((V*)me)->lambda(pp...);	}
	lambda_virt(L &&_lambda) : V(&thunk), lambda(move(_lambda)) {};
};

template<class V, class L> auto make_lambda(L &&lambda) {
	return lambda_virt<V, L, typename function<L>::F>(move(lambda));
}

template<class V, class L> V *new_lambda(L &&lambda) {
	return new lambda_virt<V, L, typename function<L>::F>(move(lambda));
}

#else

template<class R> class virtfunc<R()> {
	typedef R	F();
	R		(*f)(void*);
	template<class T>					static	R	thunk(void *me)			{ return (*static_cast<T*>((virtfunc*)me))(); }
	template<class T, typename F2, F2 f>static	R	thunk(void *me)			{ return ((static_cast<T*>((virtfunc*)me))->*f)(); }
public:
	inline_only R	operator()()				const { return (*f)((void*)this); }
	inline_only		operator safe_bool::type()	const { return safe_bool::test(f);}

	template<class T>	inline_only void bind()			{ f = thunk<T>;	}
	template<class T>	inline_only void bind(T *t)		{ f = thunk<T>;	}
	template<class T, F T::*f2>	inline_only void bind()	{ f = &thunk<T,F T::*,f2>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind()	{ f = &thunk<T,F2,f2>;	}
	inline_only void bind(R (*_f)(void*))				{ f = _f;	}

	virtfunc()	: f(NULL)		{}
	virtfunc(R (*_f)(void*)) : f(_f)	{}
	template<class T>	inline_only virtfunc(T *t)		{ f = thunk<T>;	}
};

template<class R, class P1> class virtfunc<R(P1)> {
	typedef R	F(P1);
	R		(*f)(void *me, P1 p1);
	template<class T>					static	R	thunk(void *me, P1 p1)	{ return (*static_cast<T*>((virtfunc*)me))(p1);	}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1)	{ return ((static_cast<T*>((virtfunc*)me))->*f)(p1); }
public:
	inline_only R	operator()(P1 p1)			const { return (*f)((void*)this, p1); }
	inline_only		operator safe_bool::type()	const { return safe_bool::test(f);}

	template<class T>	inline_only void bind()			{ f = thunk<T>;	}
	template<class T>	inline_only void bind(T *t)		{ f = thunk<T>;	}
	template<class T, F T::*f2>	inline_only void bind()	{ f = &thunk<T,F T::*,f2>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind()	{ f = &thunk<T,F2,f2>;	}
	inline_only void bind(R (*_f)(void*,P1))			{ f = _f;	}

	virtfunc()	: f(NULL)		{}
	virtfunc(R (*_f)(void*, P1)) : f(_f) {}
	template<class T>	inline_only virtfunc(T *t)		{ f = thunk<T>;	}
};
template<class R, class P1, class P2> class virtfunc<R(P1, P2)> {
	typedef R	F(P1,P2);
	R		(*f)(void*,P1,P2);
	template<class T>					static	R	thunk(void *me, P1 p1, P2 p2)	{ return (*static_cast<T*>((virtfunc*)me))(p1, p2);	}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1, P2 p2)	{ return ((static_cast<T*>((virtfunc*)me))->*f)(p1, p2); }
public:
	inline_only R	operator()(P1 p1, P2 p2)	const { return (*f)((void*)this, p1, p2); }
	inline_only		operator safe_bool::type()	const { return safe_bool::test(f);}

	template<class T>	inline_only void bind()			{ f = thunk<T>;	}
	template<class T>	inline_only void bind(T *t)		{ f = thunk<T>;	}
	template<class T, F T::*f2>	inline_only void bind()	{ f = &thunk<T,F T::*,f2>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind()	{ f = &thunk<T,F2,f2>;	}
	inline_only void bind(R (*_f)(void*,P1,P2))			{ f = _f;	}

	virtfunc()	: f(NULL)		{}
	virtfunc(R (*_f)(void*,P1,P2)) : f(_f) {}
	template<class T>	inline_only virtfunc(T *t)		{ f = thunk<T>;	}
};
template<class R, class P1, class P2, class P3> class virtfunc<R(P1, P2, P3)> {
	typedef R	F(P1,P2,P3);
	R		(*f)(void*,P1,P2,P3);
	template<class T>					static	R	thunk(void *me, P1 p1, P2 p2, P3 p3)	{ return (*static_cast<T*>((virtfunc*)me))(p1, p2, p3);	}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1, P2 p2, P3 p3)	{ return ((static_cast<T*>((virtfunc*)me))->*f)(p1, p2, p3); }
public:
	inline_only R	operator()(P1 p1, P2 p2, P3 p3)	const { return (*f)((void*)this, p1, p2, p3); }
	inline_only		operator safe_bool::type()		const { return safe_bool::test(f);}

	template<class T>	inline_only void bind()			{ f = thunk<T>;	}
	template<class T>	inline_only void bind(T *t)		{ f = thunk<T>;	}
	template<class T, F T::*f2>	inline_only void bind()	{ f = &thunk<T,F T::*,f2>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind()	{ f = &thunk<T,F2,f2>;	}
	inline_only void bind(R (*_f)(void*,P1,P2,P3))		{ f = _f;	}

	virtfunc()	: f(NULL)		{}
	virtfunc(R (*_f)(void*,P1,P2,P3)) : f(_f) {}
	template<class T>	inline_only virtfunc(T *t)		{ f = thunk<T>;	}
};
template<class R, class P1, class P2, class P3, class P4> class virtfunc<R(P1, P2, P3, P4)> {
	typedef R	F(P1,P2,P3,P4);
	R		(*f)(void*,P1,P2,P3,P4);
	template<class T>					static	R	thunk(void *me, P1 p1, P2 p2, P3 p3, P4 p4)	{ return (*static_cast<T*>((virtfunc*)me))(p1, p2, p3, p4);	}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1, P2 p2, P3 p3, P4 p4)	{ return ((static_cast<T*>((virtfunc*)me))->*f)(p1, p2, p3, p4); }
public:
	inline_only R	operator()(P1 p1, P2 p2, P3 p3, P4 p4)	const { return (*f)((void*)this, p1, p2, p3, p4); }
	inline_only		operator safe_bool::type()				const { return safe_bool::test(f);}

	template<class T>	inline_only void bind()			{ f = thunk<T>;	}
	template<class T>	inline_only void bind(T *t)		{ f = thunk<T>;	}
	template<class T, F T::*f2>	inline_only void bind()	{ f = &thunk<T,F T::*,f2>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind()	{ f = &thunk<T,F2,f2>;	}
	inline_only void bind(R (*_f)(void*,P1,P2,P3,P4))	{ f = _f;	}

	virtfunc()	: f(NULL)		{}
	virtfunc(R (*_f)(void*,P1,P2,P3,P4)) : f(_f) {}
	template<class T>	inline_only virtfunc(T *t)		{ f = thunk<T>;	}
};
#endif
//-----------------------------------------------------------------------------
//	callback
//	function pointer + object pointer
//-----------------------------------------------------------------------------

template<class F> class callback;

#ifdef USE_VARIADIC_TEMPLATES

template<class R, typename... PP> class callback<R(PP...)> {
public:
	typedef	R		ftype(void*, PP...), ftype_end(PP..., void*);
	template<class T, class V = void>						static	R	thunk(V *me, PP... pp)		{ return (*static_cast<T*>(me))(pp...); }
	template<class T, class V = void>						static	R	thunk_end(PP... pp, V *me)	{ return (*static_cast<T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	thunk(V *me, PP... pp)		{ return (static_cast<T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	thunk_end(PP... pp, V *me)	{ return (static_cast<T*>(me)->*f)(pp...); }
	template<class T, class V = void>						static	R	__stdcall stdcall_thunk(V *me, PP... pp)		{ return (*static_cast<T*>(me))(pp...); }
	template<class T, class V = void>						static	R	__stdcall stdcall_thunk_end(PP... pp, V *me)	{ return (*static_cast<T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	__stdcall stdcall_thunk(V *me, PP... pp)		{ return (static_cast<T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	__stdcall stdcall_thunk_end(PP... pp, V *me)	{ return (static_cast<T*>(me)->*f)(pp...); }

	void	*me;
	ftype	*f;
public:
	inline_only R		operator()(PP... pp)				const 			{ return (*f)(me, pp...); }
	constexpr explicit	operator bool()						const			{ return !!f; }
	constexpr			bool operator==(const callback &b)	const			{ return me == b.me && f == b.f; }
	constexpr			bool operator!=(const callback &b)	const			{ return me != b.me || f != b.f; }
	inline_only	void	clear()												{ f = 0; }

	template<class T>	inline_only void bind(R (*_f)(T*, PP...), T *_me)	{ me = _me; f = (ftype*)_f; }
	template<class T>	inline_only void bind(T *_me)						{ me = _me; f = thunk<T>; }
	template<class T, typename F2, F2 f2>	inline_only void bind(T *_me)	{ me = _me;	f = &thunk<T,F2,f2>; }
	template<class T, R (T::*f2)(PP...)>	inline_only void bind(T *_me)	{ me = _me;	f = &thunk<T,R (T::*)(PP...),f2>; }

	constexpr callback()					: me(NULL), f(NULL)	{}
	constexpr callback(const _none&)		: me(NULL), f(NULL)	{}
	constexpr callback(void *me, ftype *f)	: me(me), f(f)		{}
	constexpr callback(ftype *f)			: me(0), f(f)		{}
	template<class T>	constexpr callback(R (*f)(T*, PP...), T *me)	: me(me), f((ftype*)f)	{}
	template<class T>	constexpr callback(T *me)						: me(me), f(thunk<T>)	{}
};

template<class R, typename... PP> class callback<R(PP...) const> {
public:
	typedef	R		ftype(const void*, PP...), ftype_end(PP..., const void*);
	template<class T, class V = void>						static	R	thunk(const V *me, PP... pp)		{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, class V = void>						static	R	thunk_end(PP... pp, const V *me)	{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	thunk(const V *me, PP... pp)		{ return (static_cast<const T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	thunk_end(PP... pp, const V *me)	{ return (static_cast<const T*>(me)->*f)(pp...); }
	template<class T, class V = void>						static R	__stdcall stdcall_thunk(const V *me, PP... pp)		{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, class V = void>						static R	__stdcall stdcall_thunk_end(PP... pp, const V *me)	{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	__stdcall stdcall_thunk(const V *me, PP... pp)		{ return (static_cast<const T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static	R	__stdcall stdcall_thunk_end(PP... pp, const V *me)	{ return (static_cast<const T*>(me)->*f)(pp...); }

	const void	*me;
	ftype		*f;
public:
	inline_only R		operator()(PP... pp)				const 						{ return (*f)(me, pp...); }
	constexpr explicit	operator bool()						const						{ return !!f; }
	constexpr			bool operator==(const callback &b)	const						{ return me == b.me && f == b.f; }
	constexpr			bool operator!=(const callback &b)	const						{ return me != b.me || f != b.f; }
	inline_only	void	clear()															{ f = 0; }

	template<class T>	inline_only void bind(R (*_f)(const T*, PP...), const T *_me)	{ me = _me; f = (ftype*)_f; }
	template<class T>	inline_only void bind(const T *_me)								{ me = _me; f = thunk<T>; }
	template<class T, typename F2, F2 f2>		inline_only void bind(const T *_me)		{ me = _me;	f = &thunk<T,F2,f2>; }
	template<class T, R (T::*f2)(PP...)const>	inline_only void bind(const T *_me)		{ me = _me;	f = &thunk<T,R (T::*)(PP...)const,f2>; }

	constexpr callback()							: me(NULL), f(NULL)	{}
	constexpr callback(const _none&)				: me(NULL), f(NULL)	{}
	constexpr callback(const void *me, ftype *f)	: me(me), f(f)		{}
	template<class T>	constexpr callback(R (*f)(const T*, PP...), const T *me)	: me(me), f((ftype*)f)	{}
	template<class T>	constexpr callback(const T *me)								: me(me), f(thunk<T>)	{}
};

#else

template<class R> class callback<R()> {
public:
	typedef	R		ftype(void*), ftype_end(void*);
	template<class T>					static	R	thunk(void *me)			{ return (*(T*)me)(); }
	template<class T, typename F2, F2 f>static	R	thunk(void *me)			{ return (((T*)me)->*f)(); }
#ifdef USE_STDCALL
	template<class T> static	R	__stdcall stdcall_thunk(void *me)		{ return (*(T*)me)(); }
#endif
	void	*me;
	ftype	*f;
public:
	inline_only R		operator()()				const 					{ return (*f)(me);	}
	inline_only			operator safe_bool::type()	const					{ return safe_bool::test(f);}
	inline_only	void	clear()												{ f = 0;		}

	template<class T>			inline_only void bind(R (*_f)(T*), T *_me)	{ me = _me; f = (ftype*)_f; }
	template<class T>			inline_only void bind(T *_me)				{ me = _me; f = thunk<T>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind(T *_me)	{ me = _me;	f = &thunk<T,F2,f2>;	}

	callback()	: me(NULL), f(NULL)							{}
	callback(void *_me, ftype *_f)	: me(_me), f(_f)		{}
	template<class T>			inline_only callback(R (*_f)(T*), T *_me)	{ bind(_f, _me);	}
	template<class T>			inline_only callback(T *_me)				{ bind(_me);		}
};

template<class R, class P1> class callback<R(P1)> {
public:
	typedef	R		ftype(void*, P1), ftype_end(P1, void*);
	template<class T>					static	R	thunk(void *me, P1 p1)		{ return (*(T*)me)(p1);	}
	template<class T>					static	R	thunk_end(P1 p1, void *me)	{ return (*(T*)me)(p1);	}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1)		{ return (((T*)me)->*f)(p1); }
	template<class T, typename F2, F2 f>static	R	thunk_end(P1 p1,void *me)	{ return (((T*)me)->*f)(p1); }
#ifdef USE_STDCALL
	template<class T> static R	__stdcall stdcall_thunk(void *me, P1 p1)		{ return (*(T*)me)(p1);	}
	template<class T> static R	__stdcall stdcall_thunk_end(P1 p1, void *me)	{ return (*(T*)me)(p1);	}
#endif
	void	*me;
	ftype	*f;
public:
	inline_only R		operator()(P1 p1)			const 						{ return (*f)(me, p1);	}
	inline_only			operator safe_bool::type()	const						{ return safe_bool::test(f);}
	inline_only			bool operator==(const callback &b) const				{ return me == b.me && f == b.f; }
	inline_only			bool operator!=(const callback &b) const				{ return me != b.me || f != b.f; }
	inline_only	void	clear()													{ f = 0;		}

	template<class T>			inline_only void bind(R (*_f)(T*, P1), T *_me)	{ me = _me; f = (ftype*)_f; }
	template<class T>			inline_only void bind(T *_me)					{ me = _me; f = thunk<T>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind(T *_me)		{ me = _me;	f = &thunk<T,F2,f2>;	}

	callback()	: me(NULL), f(NULL)							{}
	callback(void *_me, ftype *_f)	: me(_me), f(_f)		{}
	template<class T>			inline_only callback(R (*_f)(T*, P1), T *_me)	{ bind(_f, _me);	}
	template<class T>			inline_only callback(T *_me)					{ bind(_me);		}
};

template<class R, class P1, class P2> class callback<R(P1, P2)> {
public:
	typedef	R		ftype(void*, P1,P2), ftype_end(P1,P2, void*);
	template<class T>					static	R	thunk(void *me, P1 p1, P2 p2)		{ return (*(T*)me)(p1, p2); }
	template<class T>					static	R	thunk_end(P1 p1, P2 p2, void *me)	{ return (*(T*)me)(p1, p2); }
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1, P2 p2)		{ return (((T*)me)->*f)(p1, p2); }
	template<class T, typename F2, F2 f>static	R	thunk_end(P1 p1, P2 p2, void *me)	{ return (((T*)me)->*f)(p1, p2); }
#ifdef USE_STDCALL
	template<class T> static R	__stdcall stdcall_thunk(void *me, P1 p1, P2 p2)			{ return (*(T*)me)(p1, p2);	}
	template<class T> static R	__stdcall stdcall_thunk_end(P1 p1, P2 p2, void *me)		{ return (*(T*)me)(p1, p2);	}
#endif
	void	*me;
	ftype	*f;
public:
	inline_only R		operator()(P1 p1, P2 p2)	const								{ return (*f)(me, p1, p2);	}
	inline_only			operator safe_bool::type()	const								{ return safe_bool::test(f);}
	inline_only	void	clear()															{ f = 0;		}

	template<class T>			inline_only void bind(R (*_f)(T*, P1, P2), T *_me)		{ me = _me; f = (ftype*)_f; }
	template<class T>			inline_only void bind(T *_me)							{ me = _me; f = thunk<T>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind(T *_me)				{ me = _me;	f = &thunk<T,F2,f2>;	}

	callback()	: me(NULL), f(NULL)							{}
	callback(void *_me, ftype *_f)	: me(_me), f(_f)		{}
	template<class T>			inline_only callback(R (*_f)(T*, P1, P2), T *_me)		{ bind(_f, _me);	}
	template<class T>			inline_only callback(T *_me)							{ bind(_me);		}
};

template<class R, class P1, class P2, class P3> class callback<R(P1, P2, P3)> {
public:
	typedef	R		ftype(void*, P1,P2,P3), ftype_end(P1,P2,P3, void*);
	template<class T>					static	R	thunk(void *me, P1 p1, P2 p2, P3 p3)	{ return (*(T*)me)(p1, p2, p3);		}
	template<class T>					static	R	thunk_end(P1 p1, P2 p2, P3 p3, void *me){ return (*(T*)me)(p1, p2, p3);		}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1, P2 p2, P3 p3)	{ return (((T*)me)->*f)(p1, p2, p3); }
	template<class T, typename F2, F2 f>static	R	thunk_end(P1 p1, P2 p2, P3 p3, void *me){ return (((T*)me)->*f)(p1, p2, p3); }
#ifdef USE_STDCALL
	template<class T> static R __stdcall stdcall_thunk(void *me, P1 p1, P2 p2, P3 p3)		{ return (*(T*)me)(p1, p2, p3);		}
	template<class T> static R __stdcall stdcall_thunk_end(P1 p1, P2 p2, P3 p3, void *me)	{ return (*(T*)me)(p1, p2, p3);		}
#endif
	void	*me;
	ftype	*f;
public:
	inline_only R		operator()(P1 p1, P2 p2, P3 p3)	const 								{ return (*f)(me, p1, p2, p3);	}
	inline_only			operator safe_bool::type()		const								{ return safe_bool::test(f);}
	inline_only	void	clear()																{ f = 0;		}

	template<class T>			inline_only void bind(R (*_f)(T*, P1, P2, P3), T *_me)		{ me = _me; f = (ftype*)_f; }
	template<class T>			inline_only void bind(T *_me)								{ me = _me; f = thunk<T>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind(T *_me)					{ me = _me;	f = &thunk<T,F2,f2>;	}

	callback()	: me(NULL), f(NULL)							{}
	callback(void *_me, ftype *_f)	: me(_me), f(_f)		{}
	template<class T>			inline_only callback(R (*_f)(T*, P1, P2, P3), T *_me)		{ bind(_f, _me);	}
	template<class T>			inline_only callback(T *_me)								{ bind(_me);		}
};

template<class R, class P1, class P2, class P3, class P4> class callback<R(P1, P2, P3, P4)> {
public:
	typedef	R		ftype(void*, P1,P2,P3,P4), ftype_end(P1,P2,P3,P4, void*);
	template<class T>			static	R			thunk(void *me, P1 p1, P2 p2, P3 p3, P4 p4)		{ return (*(T*)me)(p1, p2, p3, p4);	}
	template<class T>			static	R			thunk_end(P1 p1, P2 p2, P3 p3, P4 p4, void *me)	{ return (*(T*)me)(p1, p2, p3, p4);	}
	template<class T, typename F2, F2 f>static	R	thunk(void *me, P1 p1, P2 p2, P3 p3, P4 p4)		{ return (((T*)me)->*f)(p1, p2, p3, p4); }
	template<class T, typename F2, F2 f>static	R	thunk_end(P1 p1, P2 p2, P3 p3, P4 p4, void *me)	{ return (((T*)me)->*f)(p1, p2, p3, p4); }
#ifdef USE_STDCALL
	template<class T> static R __stdcall stdcall_thunk(void *me, P1 p1, P2 p2, P3 p3, P4 p4)		{ return (*(T*)me)(p1, p2, p3, p4);	}
	template<class T> static R __stdcall stdcall_thunk_end(P1 p1, P2 p2, P3 p3, P4 p4, void *me)	{ return (*(T*)me)(p1, p2, p3, p4);	}
#endif
	void	*me;
	ftype	*f;
public:
	inline_only R		operator()(P1 p1, P2 p2, P3 p3, P4 p4)	const 						{ return (*f)(me, p1, p2, p3, p4);	}
	inline_only			operator safe_bool::type()				const						{ return safe_bool::test(f);}
	inline_only	void	clear()																{ f = 0;		}

	template<class T>			inline_only void bind(R (*_f)(T*, P1, P2, P3, P4), T *_me)	{ me = _me; f = (ftype*)_f; }
	template<class T>			inline_only void bind(T *_me)								{ me = _me; f = thunk<T>;	}
	template<class T, typename F2, F2 f2>	inline_only void bind(T *_me)					{ me = _me;	f = &thunk<T,F2,f2>;	}

	callback()	: me(NULL), f(NULL)							{}
	callback(void *_me, ftype *_f)	: me(_me), f(_f)		{}
	template<class T> 			inline_only callback(R (*_f)(T*, P1, P2, P3, P4), T *_me)	{ bind(_f, _me);	}
	template<class T> 			inline_only callback(T *_me)								{ bind(_me);		}
};
#endif

template<class F> class async_callback : public callback<F> {
	void	(*del)(void*);
	using callback<F>::me;
	async_callback(const async_callback &b);
public:
	async_callback()		{}
	async_callback(_none)	{}
	async_callback(async_callback &&b) : callback<F>(b) {
		del		= b.del;
		b.me	= 0;
	}
	template<class T>	async_callback(T &&me)	: callback<F>(dup(forward<T>(me))), del(deleter<T>) {}
	template<class T>	async_callback(T *me, void(*del)(void*) = deleter<T>) : callback<F>(me), del(del) {}

	async_callback& operator=(async_callback &&b) {
		raw_swap(*this, b);
		return *this;
	}
	template<class T>	void operator=(T &&me2)	{
		if (me)
			del(me);
		callback<F>::operator=(dup(forward<T>(me2)));
		del	= deleter<noref_t<T>>;
	}
	~async_callback()	{ if (me) del(me); }
	void	clear()		{ if (me) del(me); me = 0; callback<F>::clear(); }
};


template<class F> struct virtfunc_ref : virtfunc<F> {
	inline_only virtfunc_ref(const virtfunc_ref &)			= default;
	template<class T>	inline_only virtfunc_ref(T &_me)	: virtfunc<F>(&_me) {}
};

template<class F> struct callback_ref : callback<F> {
	inline_only callback_ref()			{}
	inline_only callback_ref(const callback_ref &)			= default;
	template<class T>	inline_only callback_ref(T &_me)	: callback<F>(&_me) {}
};

template<class T, T> struct callback_maker;
template<class C, typename F, F C::*f> struct callback_maker<F C::*, f> {
	C			*c;
	callback_maker(C *_c = 0) : c(_c)	{}
	template<typename F2> operator callback<F2>() const {
		callback<F2> cb;
 		cb.template bind<C, F C::*, f>(c);
		return cb;
	}
};

template<typename F> struct function_tocallback;
template<typename C, typename R, typename...PP> struct function_tocallback<R(C*,PP...)> { typedef callback<R(PP...)> type; };

template<class T, T> struct funtion_maker;
template<class C, typename F, F C::*f> struct funtion_maker<F C::*, f> {
	template<typename F2> operator virtfunc<F2>() const {
		virtfunc<F2> vf;
 		vf.template bind<C, F C::*, f>();
		return vf;
	}
	operator typename callback<F>::ftype*		()	const {
		return callback<F>::template thunk<C, F C::*, f>;
	}
//	operator typename callback<F>::ftype_end*	()	const {
//		return callback<F>::template thunk_end<void, C, F C::*, f>;
//	}
	template<typename F2> operator F2*			()	const {
		typedef typename function_tocallback<F2>::type cb;
		return cb::template thunk<C, F C::*, f>;
	}
};

#if 1

#define make_callback(c, f)		callback_maker<decltype(f),f>(c)
#define make_virtfunc(f)		funtion_maker<decltype(f),f>
#define make_function(f)		function<decltype(f)>::template to_static<f>
#define make_function2(f,T)		function<decltype(f)>::template to_static<f,T>

#else
template<typename F> struct callback_maker2 {
	template<F f>				static funtion_maker<F, f>		get()		{ return funtion_maker<F, f>(); }
	template<F f, typename C>	static callback_maker<F, f>		get(C *c)	{ return callback_maker<F, f>(c); }
};
#define make_callback(c, f)		T_get_class<callback_maker2>(f)->template get<f>(c)
#define make_virtfunc(f)		T_get_class<callback_maker2>(f)->template get<f>()
#define make_function(f)		T_get_class<callback_maker2>(f)->template get<f>()
#endif

template<class F, class T> typename callback<F>::ftype		*callback_function()				{ return callback<F>::template thunk<T>; }
template<class F, class T> typename callback<F>::ftype		*callback_function(T*)				{ return callback<F>::template thunk<T>; }
template<class F, class T> typename callback<F>::ftype_end	*callback_function_end()			{ return callback<F>::template thunk_end<T>; }
template<class F, class T> typename callback<F>::ftype_end	*callback_function_end(T*)			{ return callback<F>::template thunk_end<T>; }

template<class F, class T> typename callback<F>::ftype		*stdcall_callback_function()		{ return callback<F>::template stdcall_thunk<T>; }
template<class F, class T> typename callback<F>::ftype		*stdcall_callback_function(T*)		{ return callback<F>::template stdcall_thunk<T>; }
template<class F, class T> typename callback<F>::ftype_end	*stdcall_callback_function_end()	{ return callback<F>::template stdcall_thunk_end<T>; }
template<class F, class T> typename callback<F>::ftype_end	*stdcall_callback_function_end(T*)	{ return callback<F>::template stdcall_thunk_end<T>; }

//-----------------------------------------------------------------------------
//	callbackN
//	multiple callbacks so object pointer only stored once
//-----------------------------------------------------------------------------

template<typename F1, typename F2> class callback2 {
	void							*me;
	typename callback<F1>::ftype	*f1;
	typename callback<F2>::ftype	*f2;
public:
	callback2()	: me(0), f1(0), f2(0)	{}
	template<class T>	inline_only callback2(T *_me) : me(_me)
		, f1(callback<F1>::template thunk<T>)
		, f2(callback<F2>::template thunk<T>)
	{}
	callback<F1>	cb1()		const	{ return callback<F1>(me, f1); }
	callback<F2>	cb2()		const	{ return callback<F2>(me, f2); }
	constexpr explicit	operator bool()	const	{ return !!me; }
};

template<typename F1, typename F2, typename F3> class callback3 {
	void							*me;
	typename callback<F1>::ftype	*f1;
	typename callback<F2>::ftype	*f2;
	typename callback<F3>::ftype	*f3;
public:
	callback3()	: me(0), f1(0), f2(0), f3(0)	{}
	template<class T>	inline_only callback3(T *_me) : me(_me)
		, f1(callback<F1>::template thunk<T>)
		, f2(callback<F2>::template thunk<T>)
		, f3(callback<F3>::template thunk<T>)
	{}
	callback<F1>	cb1()		const	{ return callback<F1>(me, f1); }
	callback<F2>	cb2()		const	{ return callback<F2>(me, f2); }
	callback<F3>	cb3()		const	{ return callback<F3>(me, f3); }
	constexpr explicit	operator bool()	const	{ return !!me; }
};

template<typename F1, typename F2, typename F3, typename F4> class callback4 {
	void							*me;
	typename callback<F1>::ftype	*f1;
	typename callback<F2>::ftype	*f2;
	typename callback<F3>::ftype	*f3;
	typename callback<F4>::ftype	*f4;
public:
	callback4()	: me(0), f1(0), f2(0), f3(0), f4(0)	{}
	template<class T>	inline_only callback4(T *_me) : me(_me)
		, f1(callback<F1>::template thunk<T>)
		, f2(callback<F2>::template thunk<T>)
		, f3(callback<F3>::template thunk<T>)
		, f4(callback<F4>::template thunk<T>)
	{}
	callback<F1>	cb1()		const	{ return callback<F1>(me, f1); }
	callback<F2>	cb2()		const	{ return callback<F2>(me, f2); }
	callback<F3>	cb3()		const	{ return callback<F3>(me, f3); }
	callback<F4>	cb4()		const	{ return callback<F4>(me, f4); }
	constexpr explicit	operator bool()	const	{ return !!me; }
};

//-----------------------------------------------------------------------------
//	function mods
//-----------------------------------------------------------------------------

template<class F> struct not_function : function<F> {
	F	f;
	not_function(const F &_f) : f(_f) {}
	bool operator()(const typename function<F>::P1& p1) const { return !f(p1); }
	bool operator()(const typename function<F>::P1& p1, const typename function<F>::P2& p2) const { return !f(p1, p2); }
};

//template<class F> not_function<F> not(const F &f) { return f; }

//-----------------------------------------------------------------------------
//	bind
//-----------------------------------------------------------------------------

template<typename P, typename T> struct _bind_first {
	const P	&p;
	const T	&a;
	bool	operator()(const T &b) const { return p(a, b); }
	_bind_first(const P &_p, const T &_a) : p(_p), a(_a)	{}
};

template<typename P, typename T> _bind_first<P, T> bind_first(const P &p, const T &a) {
	return _bind_first<P, T>(p, a);
}

template<typename P, typename T> struct _bind_second {
	const P	&p;
	const T	&b;
	bool	operator()(const T &a) const { return p(a, b); }
	template<class T2> bool operator()(const T2 &a) const { return p(a, b); }
	_bind_second(const P &_p, const T &_b) : p(_p), b(_b)	{}
};

template<typename P, typename T> _bind_second<P, T> bind_second(const P &p, const T &b) {
	return _bind_second<P, T>(p, b);
}

template<typename T1, typename T2, typename R> struct _bind_fun {
	R (*f)(T1, T2);
	R operator()(const T1 &a, const T2 &b) const { return f(a, b); }
	_bind_fun(R (*_f)(T1, T2)) : f(_f) {}
};

template<typename T1, typename T2, typename R> inline _bind_fun<T1, T2, R> bind_fun(R (*f)(T1, T2)) {
	return _bind_fun<T1, T2, R>(f);
}

}// namespace iso

#endif	// FUNCTION_H
