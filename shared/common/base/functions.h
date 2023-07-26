#ifndef FUNCTION_H
#define FUNCTION_H

#include "defs.h"
#include "tuple.h"

#ifndef USE_STDCALL
#define __stdcall
#endif

namespace iso {

//-----------------------------------------------------------------------------
//	function - type inspector for functions
//-----------------------------------------------------------------------------

template<typename F> struct function : function<decltype(&F::operator())> {
	typedef function<decltype(&F::operator())>	B;
};

template<typename F> struct function<F*> : function<F> {};

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

	static	r	lambda_to_static_end(pp... p, void *me)		{ return (*static_cast<T*>(me))(p...); }


	template<F f, class U>							static	r	to_static(U *me, pp... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(U *me, PP... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U>							static	r	to_static(void *me, pp... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(void *me, PP... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }

	template<F f, class U>							static	r	to_static_end(pp... p, U *me)		{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static_end(PP... p, U *me)		{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U>							static	r	to_static_end(pp... p, void *me)	{ return (static_cast<T*>((U*)me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static_end(PP... p, void *me)	{ return (static_cast<T*>((U*)me)->*f)(p...); }
};

template<class t, typename r, typename... pp> struct function<r(t::*)(pp...) const> {
	enum	{ N = sizeof...(pp) };
	typedef const t			T;
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (t::*F)(pp...) const;

	static	r	lambda_to_static_end(pp... p, void *me)		{ return (*static_cast<T*>(me))(p...); }


	template<F f, class U>							static	r	to_static(U *me, pp... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(U *me, PP... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U>							static	r	to_static(void *me, pp... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(void *me, PP... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }

	template<F f, class U>							static	r	to_static_end(pp... p, U *me)		{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static_end(PP... p, U *me)		{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U>							static	r	to_static_end(pp... p, void *me)	{ return (static_cast<T*>((U*)me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static_end(PP... p, void *me)	{ return (static_cast<T*>((U*)me)->*f)(p...); }
};

#ifdef USE_STDCALL

// types of member functions
template<class t, typename r, typename... pp> struct function<r(__stdcall t::*)(pp...)> {
	enum	{ N = sizeof...(pp) };
	typedef t				T;
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (t::*F)(pp...);

	template<F f, class U>							static	r	to_static(U *me, pp... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(U *me, PP... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U>							static	r	to_static(void *me, pp... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(void *me, PP... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }
};

template<class t, typename r, typename... pp> struct function<r(__stdcall t::*)(pp...) const> {
	enum	{ N = sizeof...(pp) };
	typedef const t			T;
	typedef r				R;
	typedef type_list<pp...> P;
	typedef R (t::*F)(pp...) const;

	template<F f, class U>							static	r	to_static(U *me, pp... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(U *me, PP... p)			{ return (static_cast<T*>(me)->*f)(p...); }
	template<F f, class U>							static	r	to_static(void *me, pp... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }
	template<F f, class U, class R, typename... PP>	static	R	to_static(void *me, PP... p)		{ return (static_cast<T*>((U*)me)->*f)(p...); }
};

#endif

#define make_staticfunc(f)			function<decltype(f)>::template to_static<f>
#define make_staticfunc2(f,T)		function<decltype(f)>::template to_static<f,T>
#define make_staticfunc1(T,f)		function<decltype(&T::f)>::template to_static<&T::f, T>
#define make_staticfunc_end(f)		function<decltype(f)>::template to_static_end<f>
#define make_staticlambda_end(f)	function<decltype(f)>::lambda_to_static_end


// types of functions defined by typelists
template<typename r, typename L> struct function_p;
template<typename r, typename... pp> struct function_p<r, type_list<pp...>> : function<r(pp...)>	{};

// get type of function with first parameter removed
template<typename F> struct strip_param1;
template<typename F> struct strip_param1<F*> : strip_param1<F> {};
template<typename r, typename p1, typename... pp> struct strip_param1<r(p1, pp...)> : function<r(pp...)> {};

// call a function using array
template<typename T, size_t N> class dynamic_array;

template<typename A, int I> struct _call_array_param {
	A	&a;
	_call_array_param(A &a) : a(a) {}
	template<typename T> operator T() const { return a[I]; }
	template<typename T> operator range<T>() const { return slice(a, I); }
	template<typename T> operator dynamic_array<T,0>() const { return slice(a, I); }
};
template<typename F, typename A, size_t... I, typename...P> force_inline auto _call_array(F f, A&& a, index_list<I...>&&, P&&...p) {
//	return f(forward<P>(p)..., a[I]...);
	return f(forward<P>(p)..., _call_array_param<A,I>(a)...);
}

// call a function using tuple
template<int O, typename F, typename P, size_t... I> force_inline auto call(F f, const TL_tuple<P> &p, index_list<I...>&&) {
	return f(p.template get<O + I>()...);
}
// call a member function using tuple
template<int O, class T, typename F, typename P, size_t... I> force_inline typename function<F>::R call(T &t, F f, const TL_tuple<P> &p, index_list<I...>&&) {
	return (t.*f)(p.template get<O + I>()...);
}

template<typename F, typename...P, typename A>	force_inline auto call_array(F f, A&& a, P&&...p)		{ return _call_array(f, forward<A>(a), meta::make_index_list<function<F>::N - sizeof...(P)>(), forward<P>(p)...); }

template<typename F,typename P>					force_inline auto call(F f, const TL_tuple<P> &p)		{ return call<0>(f, p, meta::make_index_list<function<F>::N>()); }
template<int O, typename F,typename P>			force_inline auto call(F f, const TL_tuple<P> &p)		{ return call<O>(f, p, meta::make_index_list<function<F>::N>()); }

template<class T, typename F,typename P>		force_inline auto call(T &t, F f, const TL_tuple<P> &p)	{ return call<0>(t, f, p, meta::make_index_list<function<F>::N>()); }
template<class T, int O, typename F,typename P>	force_inline auto call(T &t, F f, const TL_tuple<P> &p)	{ return call<O>(t, f, p, meta::make_index_list<function<F>::N>()); }

//-----------------------------------------------------------------------------
//	virtfunc - no object ptr
//	must inherit from this, because obj assumed relative to virtfunc itself
//-----------------------------------------------------------------------------

template<class T, T> struct virtfunc_maker;
#define make_virtfunc(f)	virtfunc_maker<decltype(f),f>

template<class F> class virtfunc;

template<class R, typename... PP> class virtfunc<R(PP...)> {
protected:
	typedef R	F(PP...);
	R		(*f)(virtfunc*, PP...);
	template<class T>						static	R	thunk(virtfunc *me, PP... pp)	{ return (*static_cast<T*>(me))(pp...);	}
	template<class T, typename F2, F2 f2>	static	R	thunk(virtfunc *me, PP... pp)	{ return ((static_cast<T*>(me))->*f2)(pp...); }
public:
	force_inline R	operator()(PP... pp)				{ return (*f)(this, pp...); }
	force_inline R	operator()(PP... pp)		const	{ return (*f)(unconst(this), pp...); }
	force_inline	explicit operator bool()	const	{ return !!f; }

	force_inline	void bind(R (*_f)(virtfunc*, PP...))	{ f = _f;	}
	template<class T>	force_inline void bind()			{ f = thunk<T>;	}
	template<class T>	force_inline void bind(T *t)		{ f = thunk<T>;	}
	template<class T, F T::*f2>	force_inline void bind()	{ f = &thunk<T, F T::*,f2>;	}
	template<class T, typename F2, F2 f2>	force_inline void bind()	{ f = &thunk<T,F2,f2>;	}

	constexpr virtfunc()							: f(nullptr)	{}
	constexpr virtfunc(R (*f)(virtfunc*, PP...))	: f(f)			{}
	template<class T> constexpr virtfunc(T*)		: f(thunk<T>)	{}
	template<class T, typename F2, F2 f2> virtfunc(const virtfunc_maker<F2 T::*, f2> &m) : f(thunk<T,F2,f2>) {}
};

template<class F, class L, class> struct lambda_virt;

template<class V, class L, typename X, typename R, typename... PP> struct lambda_virt<V, L, R (X::*)(PP...) const> : V {
	L	lambda;
	static	R	thunk(V *me, PP... pp)	{ return static_cast<lambda_virt*>(me)->lambda(pp...);	}
	lambda_virt(L &&lambda) : V(&thunk), lambda(move(lambda)) {}
};

template<class V, class L, typename X, typename R, typename... PP> struct lambda_virt<V, L, R (X::*)(PP...)> : V {
	L	lambda;
	static	R	thunk(V *me, PP... pp)	{ return static_cast<lambda_virt*>(me)->lambda(pp...);	}
	lambda_virt(L &&lambda) : V(&thunk), lambda(move(lambda)) {}
};

template<class V, class L> auto make_lambda(L &&lambda) {
	return lambda_virt<V, L, typename function<L>::F>(move(lambda));
}

template<class V, class L> V *new_lambda(L &&lambda) {
	return new lambda_virt<V, L, typename function<L>::F>(move(lambda));
}

//-----------------------------------------------------------------------------
//	virtfunc_ref - constructed from reference
//-----------------------------------------------------------------------------

template<class F> struct virtfunc_ref : virtfunc<F> {
	force_inline virtfunc_ref(const virtfunc_ref &)			= default;
	template<class T>	force_inline virtfunc_ref(T &me)	: virtfunc<F>(&me) {}
};

//-----------------------------------------------------------------------------
//	callback
//	function pointer + object pointer
//-----------------------------------------------------------------------------

template<class T, T> struct callback_maker;
template<class C, typename F, F C::*f> struct callback_maker<F C::*, f> {
	C	*c;
	callback_maker(C *c = 0) : c(c)	{}
//	template<typename...T> decltype(auto)	operator()(T&&...t) { return c->f(forward<T>(t)...); }
};
#define make_callback(c, f)		callback_maker<decltype(f),f>(c)

template<class F> class callback;

template<class R, typename... PP> class callback<R(PP...)> {
public:
	typedef	R		ftype(void*, PP...), ftype_end(PP..., void*), ftype0(PP...);
	template<class F>										static R adapt(void *f, PP... pp)	{ return ((F*)f)(pp...); }
	template<class T, class V = void>						static R thunk(V *me, PP... pp)		{ return (*static_cast<T*>(me))(pp...); }
	template<class T, class V = void>						static R thunk_end(PP... pp, V *me)	{ return (*static_cast<T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R thunk(V *me, PP... pp)		{ return (static_cast<T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R thunk_end(PP... pp, V *me)	{ return (static_cast<T*>(me)->*f)(pp...); }
	template<class T, class V = void>						static R __stdcall stdcall_thunk(V *me, PP... pp)		{ return (*static_cast<T*>(me))(pp...); }
	template<class T, class V = void>						static R __stdcall stdcall_thunk_end(PP... pp, V *me)	{ return (*static_cast<T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R __stdcall stdcall_thunk(V *me, PP... pp)		{ return (static_cast<T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R __stdcall stdcall_thunk_end(PP... pp, V *me)	{ return (static_cast<T*>(me)->*f)(pp...); }

	void	*me;
	ftype	*f;
public:
	force_inline R		operator()(PP... pp)				const 			{ return (*f)(me, pp...); }
	constexpr explicit	operator bool()						const			{ return !!f; }
	constexpr			bool operator==(const callback &b)	const			{ return me == b.me && f == b.f; }
	constexpr			bool operator!=(const callback &b)	const			{ return me != b.me || f != b.f; }
	force_inline		void	clear()										{ f = 0; }

	template<class T>	force_inline void bind(T *_me, R (*_f)(T*, PP...))	{ me = _me; f = (ftype*)_f; }
//	template<class T>	force_inline void bind(T *_me, R (T::*_f)(PP...))	{ me = _me; f = (ftype*)_f; }	//dodgy!
	template<class T>	force_inline void bind(T *_me)						{ me = _me; f = thunk<T>; }
	template<class T, typename F2, F2 f2>	force_inline void bind(T *_me)	{ me = _me;	f = &thunk<T,F2,f2>; }
	template<class T, R (T::*f2)(PP...)>	force_inline void bind(T *_me)	{ me = _me;	f = &thunk<T,R (T::*)(PP...),f2>; }
	void	*get_me() const { return me; }

	constexpr callback()					: me(NULL), f(NULL)	{}
	constexpr callback(const _none&)		: me(NULL), f(NULL)	{}
	constexpr callback(nullptr_t)			: me(NULL), f(NULL)	{}
	constexpr callback(void *me, ftype *f)	: me(me), f(f)		{}
	constexpr callback(ftype *f)			: me(0), f(f)		{}
	constexpr callback(ftype0 *f)			: me((void*)f), f(adapt<ftype0>)	{}
	template<class T>	constexpr callback(T *me, R (*f)(T*, PP...))	: me((void*)me), f((ftype*)f)	{}
	template<class T>	constexpr callback(T *me)						: me((void*)me), f(thunk<T>)	{}
	template<class F2, F2 f2> callback(const callback_maker<F2, f2> &m) : me(m.c), f(thunk<noref_t<decltype(*m.c)>,F2,f2>) {}
};

template<class R, typename... PP> class callback<R(PP...) const> {
public:
	typedef	R		ftype(const void*, PP...), ftype_end(PP..., const void*);
	template<class T, class V = void>						static R thunk(const V *me, PP... pp)		{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, class V = void>						static R thunk_end(PP... pp, const V *me)	{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R thunk(const V *me, PP... pp)		{ return (static_cast<const T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R thunk_end(PP... pp, const V *me)	{ return (static_cast<const T*>(me)->*f)(pp...); }
	template<class T, class V = void>						static R __stdcall stdcall_thunk(const V *me, PP... pp)		{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, class V = void>						static R __stdcall stdcall_thunk_end(PP... pp, const V *me)	{ return (*static_cast<const T*>(me))(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R __stdcall stdcall_thunk(const V *me, PP... pp)		{ return (static_cast<const T*>(me)->*f)(pp...); }
	template<class T, typename F2, F2 f, class V = void>	static R __stdcall stdcall_thunk_end(PP... pp, const V *me)	{ return (static_cast<const T*>(me)->*f)(pp...); }

	const void	*me;
	ftype		*f;
public:
	force_inline R		operator()(PP... pp)				const 						{ return (*f)(me, pp...); }
	constexpr explicit	operator bool()						const						{ return !!f; }
	constexpr			bool operator==(const callback &b)	const						{ return me == b.me && f == b.f; }
	constexpr			bool operator!=(const callback &b)	const						{ return me != b.me || f != b.f; }
	force_inline		void	clear()													{ f = 0; }

	template<class T>	force_inline void bind(const T *_me, R (*_f)(const T*, PP...))	{ me = _me; f = (ftype*)_f; }
//	template<class T>	force_inline void bind(const T *_me, R (T::*_f)(PP...) const)	{ me = _me; f = (ftype*)_f; }	//dodgy!
	template<class T>	force_inline void bind(const T *_me)							{ me = _me; f = thunk<T>; }
	template<class T, typename F2, F2 f2>		force_inline void bind(const T *_me)	{ me = _me;	f = &thunk<T,F2,f2>; }
	template<class T, R (T::*f2)(PP...) const>	force_inline void bind(const T *_me)	{ me = _me;	f = &thunk<T,R (T::*)(PP...)const,f2>; }
	void	*get_me() const { return me; }

	constexpr callback()							: me(NULL), f(NULL)	{}
	constexpr callback(const _none&)				: me(NULL), f(NULL)	{}
	constexpr callback(const void *me, ftype *f)	: me(me), f(f)		{}
	template<class T>	constexpr callback(const T *me, R (*f)(const T*, PP...))	: me(me), f((ftype*)f)	{}
	template<class T>	constexpr callback(const T *me)								: me(me), f(thunk<T>)	{}
	template<class F2, F2 f2> callback(callback_maker<F2, f2> &&m) : me(m.c), f(thunk<noref_t<decltype(*m.c)>,F2,f2>) {}
	template<class F2, F2 f2> callback(const callback_maker<F2, f2> &m) : me(m.c), f(thunk<noref_t<decltype(*m.c)>,F2,f2>) {}
};

template<class F, class T> typename callback<F>::ftype		*callback_function()				{ return callback<F>::template thunk<T>; }
template<class F, class T> typename callback<F>::ftype		*callback_function(T*)				{ return callback<F>::template thunk<T>; }
template<class F, class T> typename callback<F>::ftype_end	*callback_function_end()			{ return callback<F>::template thunk_end<T>; }
template<class F, class T> typename callback<F>::ftype_end	*callback_function_end(T*)			{ return callback<F>::template thunk_end<T>; }

template<class F, class T> typename callback<F>::ftype		*stdcall_callback_function()		{ return callback<F>::template stdcall_thunk<T>; }
template<class F, class T> typename callback<F>::ftype		*stdcall_callback_function(T*)		{ return callback<F>::template stdcall_thunk<T>; }
template<class F, class T> typename callback<F>::ftype_end	*stdcall_callback_function_end()	{ return callback<F>::template stdcall_thunk_end<T>; }
template<class F, class T> typename callback<F>::ftype_end	*stdcall_callback_function_end(T*)	{ return callback<F>::template stdcall_thunk_end<T>; }


//-----------------------------------------------------------------------------
//	async_callback
//	function pointer + (allocated) object
//-----------------------------------------------------------------------------

template<class F, int S = 64> class async_callback;

template<class F> class async_callback<F, 0> : public callback<F> {
protected:
	void	(*del)(void*);
	using callback<F>::me;
	async_callback(const async_callback &b) = delete;
public:
	async_callback()				{}
	async_callback(const _none&)	{}
	async_callback(async_callback &&b) : callback<F>(b) {
		del		= b.del;
		b.me	= 0;
	}
	template<class T>	async_callback(T &&me)	: callback<F>(dup(forward<T>(me))), del(deleter_fn<T>) {}
	template<class T>	async_callback(T *me, void(*del)(void*) = deleter_fn<T>) : callback<F>(me), del(del) {}
	template<class F2, F2 f2> async_callback(callback_maker<F2, f2> &&m, void(*del)(void*) = deleter_fn<void>) : callback<F>(m), del(del) {}

	async_callback& operator=(async_callback &&b) {
		raw_swap(*this, b);
		return *this;
	}
	template<class T>	void operator=(T &&me2)	{
		if (me)
			del(me);
		callback<F>::operator=(dup(forward<T>(me2)));
		del	= deleter_fn<noref_t<T>>;
	}
	~async_callback()	{ if (me) del(me); }
	void	clear()		{ if (me) del(me); me = 0; callback<F>::clear(); }
};

template<class F, int S> class async_callback : public async_callback<F, 0> {
	typedef async_callback<F, 0>	B;
	using B::me;
	uint8	space[S];
public:
	async_callback()				{}
	async_callback(const _none&)	{}
	async_callback(async_callback &&b) : B(move((B&)b)) {
		if (me == b.space) {
			memcpy(space, b.space, S);
			me = space;
		}
	}
	template<class T, enable_if_t<(sizeof(T) <=S),int> = 0> async_callback(T &&me)	: B(new(space) T(forward<T>(me)), destructor_fn<T>) {}
	template<class T, enable_if_t<(sizeof(T) > S),int> = 0> async_callback(T &&me)	: B(dup(forward<T>(me)), deleter_fn<T>) {}
	template<class F2, F2 f2> async_callback(callback_maker<F2, f2> &&m, void(*del)(void*) = destructor_fn<int>) : B(move(m), del) {}

	template<class T, enable_if_t<(sizeof(T) <=S),int> = 0>	void operator=(T &&me2)	{
		if (me)
			B::del(me);
		callback<F>::operator=(new(space) T(forward<T>(me2)));
		B::del	= destructor_fn<noref_t<T>>;
	}
	template<class T, enable_if_t<(sizeof(T) > S),int> = 0>	void operator=(T &&me2)	{ B::operator=(forward<T>(me2)); }
};

//-----------------------------------------------------------------------------
//	callback_ref - constructed from reference
//-----------------------------------------------------------------------------

template<class F> struct callback_ref : callback<F> {
	force_inline callback_ref()			{}
	force_inline callback_ref(callback_ref& b)				: callback<F>(b)	{}
	force_inline callback_ref(callback_ref&& b)				: callback<F>(b)	{}
	force_inline callback_ref(const callback_ref& b)		: callback<F>(b)	{}
	force_inline callback_ref(async_callback<F>& b)			: callback<F>(b)	{}
	force_inline callback_ref(const _none&)					: callback<F>(nullptr) {}

	template<class T>	force_inline callback_ref(T *me)	: callback<F>(me) {}
	template<class T>	force_inline callback_ref(T &&me)	: callback<F>(&me) {}
	template<class F2, F2 f2> callback_ref(callback_maker<F2, f2> &&m) : callback<F>(m.c, &callback<F>::template thunk<noref_t<decltype(*m.c)>,F2,f2>) {}

	force_inline callback_ref& operator=(const callback_ref &)= default;
};

//-----------------------------------------------------------------------------
//	callbacks
//	multiple callbacks so object pointer only stored once
//-----------------------------------------------------------------------------

template<typename... F> class callbacks {
	void		*me;
	tuple<typename callback<F>::ftype*...>	f;
public:
	callbacks()	: me(0), f((typename callback<F>::ftype*)nullptr...)	{}
	template<class T>	force_inline callbacks(T *me) : me(me), f(callback<F>::template thunk<T>...) {}
	template<size_t I>	callback<meta::VT_index_t<I, F...>>	get()	{ return {me, f.template get<I>()}; }
	force_inline	explicit operator bool()	const	{ return !!me; }
};

//-----------------------------------------------------------------------------
//	watched
//	add to container to watch modifications
//-----------------------------------------------------------------------------

template<typename T, typename K> struct watched_base : T {
	typedef	callback<void(T*, K k, int)>	cb_t;
	cb_t	cb;
	void						call(K k, int i)		{ cb(this, k, i); }
	template<typename R> auto	call(K k, int i, R&&r)	{ cb(this, k, i); return forward<R>(r); }
	watched_base() {}
	template<typename...P> watched_base(cb_t cb, P&&...p) : T(forward<P>(p)...), cb(cb) {}
};

}// namespace iso

#endif	// FUNCTION_H
