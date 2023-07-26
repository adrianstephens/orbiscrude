#ifndef TUPLE_H
#define TUPLE_H

#include "defs.h"

namespace iso {

template<class TL, class IL = meta::make_index_list<TL::count>> struct tuple1;

template<class TL, size_t I> struct tuple_field {
	typedef meta::TL_index_t<I, TL>	T;

	//natvis helpers
	typedef tuple_field<TL, I + 1>	next_t;
	typedef tuple1<TL>				tuple_t;
	enum {last = I == TL::count - 1 };
	//natvis helpers end

	T	t;
	tuple_field() {}
	template<typename U> constexpr tuple_field(U&& u) : t(forward<U>(u)) {}
	constexpr T&		get()		{ return t; }
	constexpr const T&	get() const	{ return t; }
};

template<class TL, size_t...I> struct tuple1<TL, index_list<I...>> : tuple_field<TL, I>... {
	template<size_t N>	constexpr decltype(auto)	get()			{ return static_cast<tuple_field<TL, N>*>(this)->get(); }
	template<size_t N>	constexpr decltype(auto)	get()	const	{ return static_cast<const tuple_field<TL, N>*>(this)->get(); }
	template<size_t N>	static auto					field()			{ return static_cast<meta::TL_index_t<N, TL> tuple1::*>(&tuple_field<TL, N>::t); }

	tuple1()	{}
	template<typename... U>	constexpr tuple1(U&&...u)	: tuple_field<TL, I>(forward<U>(u))... {}

	template<typename L> void apply(L&& lambda) {
		bool	unused[] = {(lambda(get<I>()), true)...};
	}
	template<typename L> void apply(L&& lambda) const {
		bool	unused[] = {(lambda(get<I>()), true)...};
	}
	template<typename L> auto apply_to(int i, L&& lambda) const {
		optional<decltype(lambda(get<0>()))>	result;
		bool	unused[] = {i == I && ((result = lambda(get<I>())), true)...};
		return result;
	}
	template<typename R> bool read(R&& r)			{ return r.read(get<I>()...); }
	template<typename W> bool write(W&& w) const	{ return w.write(get<I>()...); }
};

template<> struct tuple1<type_list<>> {
	template<typename R> bool read(R&& r)			{ return true; }
	template<typename W> bool write(W&& w) const	{ return true; }
};

template<class TL>	struct TL_tuple : tuple1<TL> {
	using tuple1<TL>::tuple1;
};

template<class... T> using tuple = TL_tuple<type_list<T...>>;

template<template<class> class M, typename TL> struct meta::map<M, TL_tuple<TL>> : T_type<TL_tuple<map_t<M, TL>>> {};

template<typename... T> constexpr auto make_tuple(T&&... t) {
	return tuple<T...>(forward<T>(t)...);
}

template<typename...T> auto onlyif(bool b, T&&...t) {
	return b ? optional<tuple<T...>>(make_tuple(forward<T>(t)...)) : none;
}

//-----------------------------------------------------------------------------
//	as_tuple
//-----------------------------------------------------------------------------

template<typename T>		struct TL_fields;
template<typename T>		struct TL_fields<const T> : TL_fields<T> {};
template<typename TL>		struct TL_fields<TL_tuple<TL>> : T_type<TL> {};
template<typename T>		using TL_fields_t = typename TL_fields<T>::type;

template<typename T, typename TL = TL_fields_t<T>> struct as_tuple : TL_tuple<TL> {
	constexpr operator T&()				{ return *(T*)this; }
	constexpr operator const T&() const	{ return *(T*)this; }
};

template<template<class> class M, typename T, typename TL> struct meta::map<M, as_tuple<T, TL> > : T_type<as_tuple<T, map_t<M, TL>>> {};

//-----------------------------------------------------------------------------
//	tuple_iterator
//-----------------------------------------------------------------------------

template<bool has_begin> struct s_maybe_iterator {
	template<typename T> static auto b(T &&t)	{ return begin(forward<T>(t)); }
	template<typename T> static auto e(T &&t)	{ return end(forward<T>(t)); }
};
template<> struct s_maybe_iterator<false> {
	template<typename T> static scalar_s<T> b(T &&t) { return forward<T>(t); }
	template<typename T> static scalar_s<T> e(T &&t) { return forward<T>(t); }
};
template<typename T> auto maybe_begin(T &&t) {
	return s_maybe_iterator<has_begin_v<T>>::b(forward<T>(t));
}
template<typename T> auto maybe_end(T &&t) {
	return s_maybe_iterator<has_begin_v<T>>::e(forward<T>(t));
}

template<typename... I> struct not_fake_s;
template<typename I0, typename... I> struct not_fake_s<I0, I...> { static const int index = sizeof...(I); };
template<typename I0, typename... I> struct not_fake_s<scalar_s<I0>, I...> : not_fake_s<I...> {};
template<> struct not_fake_s<> { static const int index = 0; };
template<typename... I> static constexpr int first_iterator = sizeof...(I) - not_fake_s<I...>::index - 1;

template<typename... I> struct tuple_iterator : tuple<I...> {
	typedef	tuple<I...> B;
	enum { N = sizeof...(I), R = first_iterator<I...>};

	template<size_t... II> auto		deref(index_list<II...>&&)	const	{ return make_tuple(*B::template get<II>()...); }
	template<size_t... II> void		inc(index_list<II...>&&)			{ unused(B::template get<II>()++...); }

	tuple_iterator(const I&... i) : B(i...) {}

	constexpr decltype(auto)	operator*()		const	{ return deref(meta::make_index_list<N>()); }
	constexpr tuple_iterator&	operator++()			{ inc(meta::make_index_list<N>()); return *this; }
	bool	operator!=(const tuple_iterator &b) const	{ return B::template get<R>() != b.template get<R>(); }
};

template<typename... I> auto make_tuple_iterator(const I&... i) {
	return tuple_iterator<I...>(i...);
}

template<typename... C> auto make_tuple_range(C&&... c) {
	return make_range(make_tuple_iterator(maybe_begin(forward<C>(c))...), make_tuple_iterator(maybe_end(forward<C>(c))...));
}

//-----------------------------------------------------------------------------
//	variant
//-----------------------------------------------------------------------------

template<typename T>	T&&		variant_head(T &&t)						{ return static_cast<T&&>(t); }
template<typename T>	T&&		variant_tail(T &&t)						{ return static_cast<T&&>(t); }

template<typename...T>	auto&	variant_head(union_of<T...> &v)			{ return v.t; }
template<typename...T>	auto&	variant_head(const union_of<T...> &v)	{ return v.t; }
template<typename...T>	auto&&	variant_head(union_of<T...> &&v)		{ return move(v.t); }

template<typename...T>	auto&	variant_tail(union_of<T...> &v)			{ return v.u; }
template<typename...T>	auto&	variant_tail(const union_of<T...> &v)	{ return v.u; }
template<typename...T>	auto&&	variant_tail(union_of<T...> &&v)		{ return move(v.u); }

template<typename F, typename...A> auto	variant_visit(F&& f, size_t i, A&&...a) {
	return i ? variant_visit(forward<F>(f), i - 1, variant_tail(forward<A>(a))...) : f(variant_head(forward<A>(a))...);
}
template<typename F, typename T, typename...A> auto	variant_visit(F&& f, size_t i, union_of<T> &a0, A&&...a) {
	return f(a0.t, variant_head(forward<A>(a))...);
}
template<typename F, typename T, typename...A> auto	variant_visit(F&& f, size_t i, const union_of<T> &a0, A&&...a) {
	return f(a0.t, variant_head(forward<A>(a))...);
}

template<typename C> class accum;

template<typename... T> class variant {
	union_of<T..., _none>		u;
public:
	typedef variant variant_t;
	template<size_t I>	using alternative_t = meta::VT_index_t<I, T...>;
	template<typename U> static constexpr size_t best_index = meta::VT_find<best_match_t<U, T..., _none>, T..., _none>;
	uint_bits_t<klog2<sizeof...(T) + 1>> index;

	template<typename F, typename...A> auto	visit(F&& f, A&&...a)		{ return variant_visit(forward<F>(f), index, u, variant_head(forward<A>(a))...); }
	template<typename F, typename...A> auto	visit(F&& f, A&&...a) const	{ return variant_visit(forward<F>(f), index, u, variant_head(forward<A>(a))...); }

	variant()					: index(0) {}
	variant(const variant &b)	: index(b.index) { visit(op_construct(), b); }
	variant(variant &&b)		: index(b.index) { visit(op_construct(), move(b)); }
	variant(variant &b)			: variant((const variant&)b)	{}
	template<typename A, size_t I = best_index<A>> variant(A &&a) : index(I) { construct(u.template get<I>(), forward<A>(a));}
	template<typename A, size_t I = best_index<A>> variant(const A &a) : index(I) { construct(u.template get<I>(), a);}
	~variant()	{ visit(op_destruct()); }

	variant&	operator=(const variant &b)	{
		if (index != b.index) {
			visit(op_destruct());
			index = b.index;
			visit(op_construct(), b);
		} else {
			visit(op_move(), b);
		}
		return *this;
	}
	variant&	operator=(variant &&b) {
		if (index != b.index) {
			visit(op_destruct());
			index = b.index;
			visit(op_construct(), move(b));
		} else {
			visit(op_move(), move(b));
		}
		return *this;
	}
	variant&	operator=(variant &b) {
		return operator=((const variant&)b);
	}
	template<typename A> variant& operator=(A &&a) {
		static const size_t I = best_index<A>;
		if (index != I) {
			visit(op_destruct());
			index = I;
			construct(u.template get<I>(), forward<A>(a));
		} else {
			u.template get<I>() = forward<A>(a);
		}
		return *this;
	}

	template<size_t I, typename... A>	auto&	emplace(A&&... a)	{
		visit(op_destruct());
		index = I;
		return construct(u.template get<I>(), forward<A>(a)...);
	}
	template<typename U, typename... A>	auto&	emplace(A&&... a)	{
		return emplace<best_index<U>>(forward<A>(a)...);
	}

	template<size_t I>		bool						is()		const	{ return I == index; }
	template<typename U>	bool						is()		const	{ return is<best_index<U>>(); }

	template<size_t I>		optional<const alternative_t<I>&>	get()		const	{ if (I != index) return {}; return u.template get<I>(); }
	template<typename U>	decltype(auto)				get()		const	{ return get<best_index<U>>(); }
	template<size_t I>		optional<alternative_t<I>&>	get()				{ if (I != index) return {}; return u.template get<I>(); }
	template<typename U>	decltype(auto)				get()				{ return get<best_index<U>>(); }

	template<size_t I>		const alternative_t<I>&		get_known() const	{ ISO_ASSERT(I == index); return u.template get<I>(); }
	template<typename U>	decltype(auto)				get_known() const	{ return get_known<best_index<U>>(); }
	template<size_t I>		alternative_t<I>&			get_known()			{ ISO_ASSERT(I == index); return u.template get<I>(); }
	template<typename U>	decltype(auto)				get_known()			{ return get_known<best_index<U>>(); }

	constexpr bool operator==(const variant &b)	const	{ return index == b.index && visit(equal_to(), b); }
	constexpr bool operator< (const variant &b)	const	{ return index < b.index || (index == b.index && visit(less(), b)); }
	constexpr bool operator<=(const variant &b)	const	{ return index < b.index || (index == b.index && visit(less_equal(), b)); }
	constexpr bool operator!=(const variant &b)	const	{ return !(*this == b); }
	constexpr bool operator> (const variant &b)	const	{ return b < *this; }
	constexpr bool operator>=(const variant &b)	const	{ return b <= *this; }

	friend auto&	variant_head(variant &v)		{ return v.u; }
	friend auto&	variant_head(const variant &v)	{ return v.u; }
	friend auto&&	variant_head(variant &&v)		{ return move(v.u); }
	template<typename C> friend accum<C>& operator<<(accum<C> &a, const variant &v) {
		v.visit([a](auto &x) { a << x; });
	}
};



}//namespace iso

#endif // TUPLE_H
