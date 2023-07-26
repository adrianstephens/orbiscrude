#ifndef DEFERRED_H
#define DEFERRED_H

#include "defs.h"
#include "tuple.h"

namespace iso {

//-----------------------------------------------------------------------------
//	deferred
//-----------------------------------------------------------------------------

template<typename F, typename... P> struct deferred;

template<typename... P>				auto make_deferred(P&&... p)		{ return deferred<P...>(forward<P>(p)...); }
template<typename F, typename... P> auto make_deferred(P&&... p)		{ return deferred<F, P...>(F(), forward<P>(p)...); }

template<typename...A>				auto	operator- (const deferred<A...> &t)			{ return make_deferred([](auto a) { return -a; }, t); }
template<typename...A>				auto	operator~ (const deferred<A...> &t)			{ return make_deferred([](auto a) { return ~a; }, t); }
template<typename...A>				auto	operator! (const deferred<A...> &t)			{ return make_deferred([](auto a) { return !a; }, t); }
template<typename...A, typename B>	auto	operator+ (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a +  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator- (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a -  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator* (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a *  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator/ (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a /  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator% (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a %  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator& (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a &  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator| (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a |  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator^ (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a ^  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator<<(const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a << b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator>>(const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a >> b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator< (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a <  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator<=(const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a <= b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator> (const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a >  b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator>=(const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a >= b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator==(const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a == b; }, t, forward<B>(b)); }
template<typename...A, typename B>	auto	operator!=(const deferred<A...> &t, B &&b)	{ return make_deferred([](auto a, auto b) { return a != b; }, t, forward<B>(b)); }

template<typename B, typename...A>	auto	to(const deferred<A...> &t)	{ return make_deferred([](auto a) { return (B)a; }, t); }

template<typename A, typename...B>	auto&	operator+=(A& a, const deferred<B...> &b)	{ A t = make_deferred(op_add(), a, b); return a = move(t); }

struct op_abs			{ template<typename A> A operator()(const A &a) const { return abs(a); } };

template<typename...P> auto	abs(deferred<P...> &&t)												{ return make_deferred(op_abs(), move(t)); }
template<typename...A, typename B> auto	max(deferred<A...> &&a, B &&b)							{ return make_deferred(op_max(), move(a), forward<B>(b)); }
template<typename...A, typename B> auto	min(deferred<A...> &&a, B &&b)							{ return make_deferred(op_min(), move(a), forward<B>(b)); }
template<typename...A, typename B, typename C> auto select(deferred<A...> &&a,  B &&b, C &&c)	{ return make_deferred(op_select(), move(a), forward<B>(b), forward<C>(c)); }

template<typename T, typename...X>	T&			resolve_deferred(T &t, X&&...)						{ return t; }
template<typename...P, typename...X> auto		resolve_deferred(const deferred<P...> &t, X&&... x) { return t(forward<X>(x)...); }
template<int I, typename...X> decltype(auto)	resolve_deferred(const op_param<I>&, X&&... x)		{ return PP_index<I>(forward<X>(x)...); }
template<typename S, typename T>	T&			resolve_deferred(const op_field<S,T> &t, S &s)		{ return t(s); }

template<typename T> struct deferred<T> : inheritable<T> {
	//using T::T;
	deferred() {}
	deferred(const T &t)	: inheritable<T>(t) {}
	deferred(T &&t)			: inheritable<T>(move(t)) {}
};

template<typename T> struct deferred<T&> {
	T&	t;
	deferred(T &t)	: t(t) {}
	auto	begin() const	{ using iso::begin; return begin(t); }
	auto	end()	const	{ using iso::end; return end(t); }
};

template<typename F, typename... I> struct deferred_iterator {
	enum { N = sizeof...(I), FIRST_REAL = first_iterator<I...>};
	F			f;
	tuple<I...>	i;

	template<size_t... J>auto deref(index_list<J...>)			const	{ return f(*i.template get<J>()...); }
	template<size_t... J>auto index(int j, index_list<J...>)	const	{ return f(i.template get<J>()[j]...); }
	template<size_t... J>auto add_all(int j, index_list<J...>)	const	{ return deferred_iterator(f, i.template get<J>() + j...); }

public:
	deferred_iterator(F&& f, I... i) : f(forward<F>(f)), i(i...) {}
	auto	operator*()	 							const	{ return deref(meta::make_index_list<N>()); }
	auto	operator->() 							const	{ return make_ref_helper(operator*()); }
	auto	operator[](int j)						const	{ return index(j, meta::make_index_list<N>()); }
	auto&	operator++()									{ i.apply([](auto &i) { ++i; }); return *this; }
	auto&	operator--()									{ i.apply([](auto &i) { --i; }); return *this; }
	auto	operator+=(int j)								{ i.apply([j](auto &i) { i += j; }); return *this; }
	auto	operator+(int j)						const	{ return add_all(j, meta::make_index_list<N>()); }
	bool	operator==(const deferred_iterator &b)	const	{ return i.template get<FIRST_REAL>() == b.i.template get<FIRST_REAL>(); }
	bool	operator!=(const deferred_iterator &b)	const	{ return i.template get<FIRST_REAL>() != b.i.template get<FIRST_REAL>(); }
	auto	operator-(const deferred_iterator &b)	const	{ return i.template get<FIRST_REAL>() -  b.i.template get<FIRST_REAL>(); }
};

template<typename F, typename... I> deferred_iterator<F, I...> make_deferred_iterator(F&& f, I&&...i) {
	return {forward<F>(f), forward<I>(i)...};
}

template<typename F, typename... P> struct deferred : inheritable<F> {
	tuple<P...>	params;

	template<typename...X, size_t...J>	auto apply_all(const index_list<J...>&, X&&... x) const {
		return this->get_inherited()(params.template get<J>()(forward<X>(x)...)...);
	}
	template<size_t...J>				auto begin_all(const index_list<J...>&) const {
		using iso::begin;
		return make_deferred_iterator(this->get_inherited(), maybe_begin(params.template get<J>())...);
	}

	deferred(P&& ...p)			: params(forward<P>(p)...) {}
	deferred(F &&f, P&& ...p)	: inheritable<F>(forward<F>(f)), params(forward<P>(p)...) {}

	template<typename...X>	auto operator()(X&&... x) const {
		return apply_all(meta::make_index_list<sizeof...(P)>(), forward<X>(x)...);
	}
	auto	begin() const	{ return begin_all(meta::make_index_list<sizeof...(P)>()); }
	auto	end()	const	{ return begin() + num_elements(*this); }
	friend constexpr size_t num_elements(const deferred &t) { return num_elements(t.params.template get<0>()); }
};
/*
template<typename T, typename... P> struct deferred<T&, P...> {
	T	&f;
	tuple<P...>	params;

	template<typename...X, size_t...J>	auto apply_all(const index_list<J...>&, X&&... x) const {
		return f(params.template get<J>()(forward<X>(x)...)...);
	}
	template<size_t...J>				auto begin_all(const index_list<J...>&) const {
		using iso::begin;
		return make_deferred_iterator(f, maybe_begin(params.template get<J>())...);
	}

	deferred(T &f, P&& ...p)	: f(f), params(forward<P>(p)...) {}
	template<typename...X>	auto operator()(X&&... x) const {
		return apply_all(meta::make_index_list<sizeof...(P)>(), forward<X>(x)...);
	}
	auto	begin() const	{ return begin_all(meta::make_index_list<sizeof...(P)>()); }
	auto	end()	const	{ return begin() + num_elements(*this); }
	friend constexpr size_t num_elements(const deferred &t) { return num_elements(t.params.template get<0>()); }
};
*/
}//namespace iso

#endif // DEFERRED_H
