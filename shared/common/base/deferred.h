#ifndef DEFERRED_H
#define DEFERRED_H

#include "defs.h"
#include "tuple.h"

namespace iso {

//-----------------------------------------------------------------------------
//	deferred
//-----------------------------------------------------------------------------

template<typename F, typename... P> struct deferred;

template<typename F, typename... P> auto make_deferred(F &&f, P&&... p)	{ return deferred<F, P...>(forward<F>(f), forward<P>(p)...); }
template<typename F, typename... P> auto make_deferred(P&&... p)		{ return deferred<F, P...>(F(), forward<P>(p)...); }

template<typename T> struct deferred_mixin {
	friend auto							operator-(T &&t)				{ return deferred<op_neg,		T>(move(t)); }
	friend auto							operator~(T &&t)				{ return deferred<op_not,		T>(move(t)); }
	template<typename B> friend auto	operator+ (T &&t, B &&b)		{ return deferred<op_add,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator- (T &&t, B &&b)		{ return deferred<op_sub,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator* (T &&t, B &&b)		{ return deferred<op_mul,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator/ (T &&t, B &&b)		{ return deferred<op_div,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator% (T &&t, B &&b)		{ return deferred<op_mod,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator& (T &&t, B &&b)		{ return deferred<op_and,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator| (T &&t, B &&b)		{ return deferred<op_or,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator^ (T &&t, B &&b)		{ return deferred<op_xor,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator<<(T &&t, B &&b)		{ return deferred<op_shl,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator>>(T &&t, B &&b)		{ return deferred<op_shr,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator< (T &&t, B &&b)		{ return deferred<less,			T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator<=(T &&t, B &&b)		{ return deferred<less_equal,	T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator> (T &&t, B &&b)		{ return deferred<greater,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator>=(T &&t, B &&b)		{ return deferred<greater_equal,T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator==(T &&t, B &&b)		{ return deferred<equal_to,		T, B>(move(t), forward<B>(b)); }
	template<typename B> friend auto	operator!=(T &&t, B &&b)		{ return deferred<not_equal_to,	T, B>(move(t), forward<B>(b)); }

	friend auto							operator-(const T &t)			{ return deferred<op_neg,		const T&>(t); }
	friend auto							operator~(const T &t)			{ return deferred<op_not,		const T&>(t); }
	template<typename B> friend auto	operator+ (const T &t, B &&b)	{ return deferred<op_add,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator- (const T &t, B &&b)	{ return deferred<op_sub,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator* (const T &t, B &&b)	{ return deferred<op_mul,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator/ (const T &t, B &&b)	{ return deferred<op_div,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator% (const T &t, B &&b)	{ return deferred<op_mod,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator& (const T &t, B &&b)	{ return deferred<op_and,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator| (const T &t, B &&b)	{ return deferred<op_or,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator^ (const T &t, B &&b)	{ return deferred<op_xor,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator<<(const T &t, B &&b)	{ return deferred<op_shl,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator>>(const T &t, B &&b)	{ return deferred<op_shr,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator< (const T &t, B &&b)	{ return deferred<less,			const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator<=(const T &t, B &&b)	{ return deferred<less_equal,	const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator> (const T &t, B &&b)	{ return deferred<greater,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator>=(const T &t, B &&b)	{ return deferred<greater_equal,const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator==(const T &t, B &&b)	{ return deferred<equal_to,		const T&, B>(t, forward<B>(b)); }
	template<typename B> friend auto	operator!=(const T &t, B &&b)	{ return deferred<not_equal_to,	const T&, B>(t, forward<B>(b)); }

	template<typename B> friend auto	operator+=(T &t, B &&b)			{ return make_deferred(op_add_eq(),	t, forward<B>(b)); }
	template<typename B> friend auto	operator-=(T &t, B &&b)			{ return make_deferred(op_sub_eq(),	t, forward<B>(b)); }
	template<typename B> friend auto	operator*=(T &t, B &&b)			{ return make_deferred(op_mul_eq(),	t, forward<B>(b)); }
	template<typename B> friend auto	operator/=(T &t, B &&b)			{ return make_deferred(op_div_eq(),	t, forward<B>(b)); }
	template<typename B> friend auto	operator%=(T &t, B &&b)			{ return make_deferred(op_mod_eq(),	t, forward<B>(b)); }
	template<typename B> friend auto	operator&=(T &t, B &&b)			{ return make_deferred(op_and_eq(),	t, forward<B>(b)); }
	template<typename B> friend auto	operator|=(T &t, B &&b)			{ return make_deferred(op_or_eq(),	t, forward<B>(b)); }
	template<typename B> friend auto	operator^=(T &t, B &&b)			{ return make_deferred(op_xor_eq(),	t, forward<B>(b)); }
};

template<typename T, int container> struct deferred_container {};

//binary
template<typename F, typename A, typename B> struct deferred_container<deferred<F, A, B>, 3> {
	typedef	deferred<F, A, B>	T;

	struct iterator {
		typedef iterator_t<const A>		AI;
		typedef iterator_t<const B>		BI;
		const T	*t;
		AI		a;
		BI		b;
		iterator(const T *t, AI a, BI b) : t(t), a(a), b(b) {}
		auto		operator*()	 					const	{ return (*(F*)t)(*a, *b); }
		auto		operator->() 					const	{ return make_ref_helper((*(F*)t)(*a, *b)); }
		auto		operator[](int j)				const	{ return (*t)(a[j], b[j]); }
		iterator&	operator++()							{ ++a; ++b; return *this; }
		iterator	operator++(int)							{ return iterator(t, a++, b++); }
		iterator&	operator--()							{ --a; --b; return *this; }
		iterator	operator--(int)							{ return iterator(t, a--, b--); }
		iterator&	operator+=(int j)						{ a += j; b += j; return *this; }
		iterator&	operator-=(int j)						{ a -= j; b -= j; return *this; }
		bool		operator==(const iterator &i)	const	{ return a == i.a; }
		bool		operator!=(const iterator &i)	const	{ return a != i.a; }
		auto		operator-(const iterator &i)	const	{ return a - i.a; }
		iterator	operator+(int j)				const	{ return iterator(t, a + j, b + j); }
	};

	auto		operator[](int i)	const	{ auto t = static_cast<const T*>(this); return (*(F*)t)(t->a[i], t->b[i]); }
	iterator	begin()				const	{ using iso::begin;	auto t = static_cast<const T*>(this); return iterator(t, begin(t->a), begin(t->b)); }
	iterator	end()				const	{ using iso::end;	auto t = static_cast<const T*>(this); return iterator(t, end(t->a), end(t->b)); }
	auto		front()				const	{ return *begin(); }
	auto		back()				const	{ return *end(); }

	friend constexpr size_t num_elements(const T &t) { return min(num_elements(t.a), num_elements(t.b)); }
};

template<typename F, typename A, typename B> struct deferred_container<deferred<F, A, B>, 1> {
	typedef	deferred<F, A, B>	T;

	struct iterator {
		typedef iterator_t<const A>		AI;
		const T	*t;
		AI		a;
		iterator(const T *t, AI a) : t(t), a(a) {}
		auto		operator*()	 					const	{ return (*(F*)t)(*a, t->b); }
		auto		operator->() 					const	{ return make_ref_helper((*(F*)t)(*a, t->b)); }
		auto		operator[](int j)				const	{ return (*(F*)t)(a[j], t->b); }
		iterator&	operator++()							{ ++a; return *this; }
		iterator	operator++(int)							{ return iterator(t, a++); }
		iterator&	operator--()							{ --a; return *this; }
		iterator	operator--(int)							{ return iterator(t, a--); }
		iterator&	operator+=(int j)						{ a += j; return *this; }
		iterator&	operator-=(int j)						{ a -= j; return *this; }
		bool		operator==(const iterator &i)	const	{ return a == i.a; }
		bool		operator!=(const iterator &i)	const	{ return a != i.a; }
		auto		operator-(const iterator &i)	const	{ return a - i.a; }
		iterator	operator+(int j)				const	{ return iterator(t, a + j); }
	};

	auto		operator[](int i)	const	{ auto t = static_cast<const T*>(this); return (*(F*)t)(t->a[i], t->b); }
	iterator	begin()				const	{ using iso::begin;	auto t = static_cast<const T*>(this); return iterator(t, begin(t->a)); }
	iterator	end()				const	{ using iso::end;	auto t = static_cast<const T*>(this); return iterator(t, end(t->a)); }
	auto		front()				const	{ return *begin(); }
	auto		back()				const	{ return *end(); }

	friend constexpr size_t num_elements(const T &t) { return num_elements(t.a); }
};

template<typename F, typename A, typename B> struct deferred_container<deferred<F, A, B>, 2> {
	typedef	deferred<F, A, B>	T;

	struct iterator {
		typedef iterator_t<const B>		BI;
		const T	*t;
		BI		b;
		iterator(const T *t, BI b) : t(t), b(b) {}
		auto		operator*()	 					const	{ return (*(F*)t)(t->a, *b); }
		auto		operator->() 					const	{ return make_ref_helper((*(F*)t)(t->a, *b)); }
		auto		operator[](int j)				const	{ return (*(F*)t)(t->a, b[j]); }
		iterator&	operator++()							{ ++b; return *this; }
		iterator	operator++(int)							{ return iterator(t, t->a, b++); }
		iterator&	operator--()							{ --b; return *this; }
		iterator	operator--(int)							{ return iterator(t, b--); }
		iterator&	operator+=(int j)						{ b += j; return *this; }
		iterator&	operator-=(int j)						{ b -= j; return *this; }
		bool		operator==(const iterator &i)	const	{ return b == i.b; }
		bool		operator!=(const iterator &i)	const	{ return b != i.b; }
		auto		operator-(const iterator &i)	const	{ return b - i.b; }
		iterator	operator+(int j)				const	{ return iterator(t, b + j); }
	};

	auto		operator[](int i)	const	{ auto t = static_cast<const T*>(this); return (*(F*)t)(t->a, t->b[i]); }
	iterator	begin()				const	{ using iso::begin;	auto t = static_cast<const T*>(this); return iterator(t, begin(t->b)); }
	iterator	end()				const	{ using iso::end;	auto t = static_cast<const T*>(this); return iterator(t, end(t->b)); }
	auto		front()				const	{ return *begin(); }
	auto		back()				const	{ return *end(); }

	friend constexpr size_t num_elements(const T &t) { return num_elements(t.b); }
};


//unary
template<typename F, typename A> struct deferred_container<deferred<F, A>, 1> {
	typedef	deferred<F, A>	T;

	struct iterator {
		typedef iterator_t<const A>		AI;
		const T	*t;
		AI		a;
		iterator(const T *t, AI a) : t(t), a(a) {}
		auto		operator*()	 					const	{ return (*(F*)t)(*a); }
		auto		operator->() 					const	{ return make_ref_helper((*(F*)t)(*a)); }
		auto		operator[](int j)				const	{ return (*(F*)t)(a[j]); }
		iterator&	operator++()							{ ++a; return *this; }
		iterator	operator++(int)							{ return iterator(t, a++); }
		iterator&	operator--()							{ --a; return *this; }
		iterator	operator--(int)							{ return iterator(t, a--); }
		iterator&	operator+=(int j)						{ a += j; return *this; }
		iterator&	operator-=(int j)						{ a -= j; return *this; }
		bool		operator==(const iterator &i)	const	{ return a == i.a; }
		bool		operator!=(const iterator &i)	const	{ return a != i.a; }
		auto		operator-(const iterator &i)	const	{ return a - i.a; }
		iterator	operator+(int j)				const	{ return iterator(t, a + j); }
	};

	auto		operator[](int i)	const	{ auto t = static_cast<const T*>(this); return (*(F*)t)(t->a[i]); }
	iterator	begin()				const	{ using iso::begin;	auto t = static_cast<const T*>(this); return iterator(t, begin(t->a)); }
	iterator	end()				const	{ using iso::end;	auto t = static_cast<const T*>(this); return iterator(t, end(t->a)); }
	auto		front()				const	{ return *begin(); }
	auto		back()				const	{ return *end(); }

	friend constexpr size_t num_elements(const T &t) { return num_elements(t.a); }
};

template<typename T> struct deferred<T> : T, deferred_mixin<T> {
	using T::T;
	deferred() {}
	deferred(const T &t)	: T(t) {}
	deferred(T &&t)			: T(move(t)) {}
};

template<typename T, typename...X>	T&			resolve_deferred(T &t, X&&...)						{ return t; }
template<typename...P, typename...X> auto		resolve_deferred(const deferred<P...> &t, X&&... x) { return t(forward<X>(x)...); }
template<int I, typename...X> decltype(auto)	resolve_deferred(const op_param<I>&, X&&... x)		{ return PP_index<I>(forward<X>(x)...); }
template<typename S, typename T>	T&			resolve_deferred(const op_field<S,T> &t, S &s)		{ return t(s); }

template<typename F, typename A, typename B> struct deferred<F, A, B> : F, deferred_mixin<deferred<F, A, B> >, deferred_container<deferred<F, A, B>, int(has_begin_v<A>) | (has_begin_v<B> * 2)> {
	A	a;
	B	b;
	deferred(F &&f, A &&a, B &&b)	: F(forward<F>(f)), a(forward<A>(a)), b(forward<B>(b)) {}
	deferred(A &&a, B &&b)			: a(forward<A>(a)), b(forward<B>(b)) {}
	template<typename...X>	auto operator()(X&&... x) const { return F::operator()(resolve_deferred(a, forward<X>(x)...), resolve_deferred(b, forward<X>(x)...)); }
};

template<typename F, typename A> struct deferred<F, A> : F, deferred_mixin<deferred<F, A> >, deferred_container<deferred<F, A>, has_begin_v<A>> {
	A	a;
	deferred(F &&f, A &&a)	: F(forward<F>(f)), a(forward<A>(a)) {}
	deferred(A &&a)			: a(forward<A>(a)) {}
	template<typename...X>	auto operator()(X&&... x) const { return F::operator()(resolve_deferred(a, forward<X>(x)...)); }
};

struct op_abs			{ template<typename A> A operator()(const A &a) const { return abs(a); } };
template<typename...P> auto	abs(deferred<P...> &&t)				{ return make_deferred(op_abs(),		move(t)); }

template<typename F, typename... P> struct deferred : F, deferred_mixin<deferred<F,P...>> {
	tuple<P...>	params;
	deferred(F &&f, P&& ...p) : F(forward<F>(f)), params(forward<P>(p)...) {}
	template<typename...X, size_t...J>	auto apply(const index_list<J...>&, X&&... x)	const { return ((F*)this)(params.template get<J>()(forward<X>(x)...)...); }
	template<typename...X>				auto operator()(X&&... x)						const { return apply(meta::make_index_list<sizeof...(P)>(), forward<X>(x)...); }
};

}//namespace iso

#endif // DEFERRED_H
