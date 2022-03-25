#ifndef TYPELIST_H
#define TYPELIST_H

#include "defs.h"

namespace iso {

#if 1

template<class TL, size_t I> struct tuple_field {
	typedef meta::TL_index_t<I, TL>	T;
	T	t;
	tuple_field() {}
	template<typename U> tuple_field(U&& u) : t(forward<U>(u)) {}
	T&			get()		{ return t; }
	const T&	get() const	{ return t; }
};

template<class TL, class IL = meta::make_index_list<TL::count>> struct TL_tuple {};

template<class TL, size_t...I> struct TL_tuple<TL, index_list<I...>> : tuple_field<TL, I>... {
	TL_tuple()	{}
	template<typename... U>	constexpr TL_tuple(U&&...u)	: tuple_field<TL, I>(forward<U>(u))... {}

	template<size_t N>	decltype(auto)				get()			{ return ((tuple_field<TL, N>*)this)->get(); }
	template<size_t N>	constexpr decltype(auto)	get()	const	{ return ((const tuple_field<TL, N>*)this)->get(); }
	template<size_t N>	static auto					field()			{ return &tuple_field<TL, N>::t; }

	template<typename R> bool read(R&& r)			{ return iso::read(r, get<I>()...); }
	template<typename W> bool write(W&& w) const	{ return iso::write(w, get<I>()...); }
};

template<> struct TL_tuple<type_list<>,  meta::make_index_list<0>> {
	template<typename R> bool read(R&& r)			{ return true; }
	template<typename W> bool write(W&& w) const	{ return true; }
};

template<class... T> using tuple = TL_tuple<type_list<T...>>;


//template<class... T> using tuple = tuple1<type_list<T...>, meta::make_index_list<sizeof...(T)>>;
//template<class TL>	using TL_tuple = tuple1<TL, meta::make_index_list<TL::count>>;


#else

//-----------------------------------------------------------------------------
//	tuple helpers
//-----------------------------------------------------------------------------

template<int O, typename... T>	struct offset_type_list : type_list<T...> {};
template<int O, typename...T>	struct meta::TL_make_index_list<offset_type_list<O, T...>> : make_index_list<sizeof...(T)> {};

//-----------------------------------------------------------------------------
//	tuple
//-----------------------------------------------------------------------------

template<typename TL> struct TL_tuple;

template<int O, typename T0, typename... T> struct TL_tuple<offset_type_list<O, T0, T...> > : elided<offset_type<T0, O>> {
	typedef	offset_type<T0, O>		head_offset_t;
	typedef TL_tuple<offset_type_list<sizeof(head_offset_t), T...> >	tail_t;
	typedef elided<head_offset_t>	B;

	tail_t			tail;
	template<int N>	static auto		field()			{ return &tuple_offset_type<N, O, T0, T...>::type::t; }
	template<int N>	auto&			get()			{ return ((typename tuple_offset_type<N, O, T0, T...>::type*)this)->t; }
	template<int N>	constexpr auto&	get()	const	{ return ((typename tuple_offset_type<N, O, T0, T...>::type*)this)->t; }

	TL_tuple()	{}
	template<typename P0, typename... PP>	constexpr TL_tuple(P0 &&p0, PP&&... pp)	: B(forward<P0>(p0)), tail(forward<PP>(pp)...) {}
	template<typename T2>					constexpr TL_tuple(T2 &&t2)				: B(t2.template get<0>()), tail(t2.tail) {}
	template<typename T2> TL_tuple&			operator=(T2 &&t2)						{ B::operator=(t2.template get<0>()); tail = t2.tail; return *this; }
	template<typename T1, typename T2> TL_tuple& operator=(const pair<T1,T2> &p)	{ B::operator=(p.a); tail = p.b; return *this; }
	template<typename T1, typename T2> TL_tuple& operator=(pair<T1,T2> &p)			{ B::operator=(p.a); tail = p.b; return *this; }
	template<typename T1, typename T2> TL_tuple& operator=(pair<T1,T2> &&p)			{ B::operator=(p.a); tail = p.b; return *this; }

	template<typename R> bool read(R&& r)			{ return r.read<B>(this) && tail.read(r); }
	template<typename W> bool write(W&& w) const	{ return w.write<B>(this) && tail.write(w); }
};

template<>		struct TL_tuple<type_list<> > {
	template<typename R> bool read(R&& r)			{ return true; }
	template<typename W> bool write(W&& w) const	{ return true; }
};
template<int O> struct TL_tuple<offset_type_list<O> > : TL_tuple<type_list<> > {};

template<int O, typename T0> struct TL_tuple<offset_type_list<O, T0> > {
	typedef offset_type<T0, O>		head_offset_t;
	typedef TL_tuple<type_list<> >	tail_t;

	head_offset_t	head;

	template<int N>	static auto	field()		{ return &head_offset_t::t; }
	template<int N>	auto&		get()		{ return head.t; }
	template<int N>	auto&		get() const	{ return head.t; }

	TL_tuple()	{}
	template<typename P0>			constexpr TL_tuple(P0 &&p0)					: head(forward<P0>(p0)) {}
	template<typename TL2>			constexpr TL_tuple(TL_tuple<TL2> &t2)		: head(t2.template get<0>()) {}
	template<typename TL2>			constexpr TL_tuple(const TL_tuple<TL2> &t2)	: head(t2.template get<0>()) {}
	template<typename T2> TL_tuple&	operator=(TL_tuple<T2> &t2)			{ head = t2.head; return *this; }
	template<typename T2> TL_tuple&	operator=(const TL_tuple<T2> &t2)	{ head = t2.head; return *this; }
	template<typename T2> TL_tuple&	operator=(const T2 &t2)				{ head = t2; return *this; }

	template<typename R> bool read(R&& r)			{ return r.read(unconst(head.t)); }
	template<typename W> bool write(W&& w) const	{ return w.write(head.t); }
};

template<typename... T> struct TL_tuple<type_list<T...>> : TL_tuple<offset_type_list<0, T...> > {
	typedef TL_tuple<offset_type_list<0, T...> > B;
	TL_tuple()	{}
	template<typename... PP>		TL_tuple(PP&&... pp)				: B(forward<PP>(pp)...) {}
	template<typename TL2>			TL_tuple(const TL_tuple<TL2> &t2)	: B(t2) {}
	template<typename T2> TL_tuple&	operator=(const T2 &&t2)			{ B::operator=(t2); return *this; }
};

template<typename... T> using tuple = TL_tuple<offset_type_list<0, T...> >;

template<template<class> class M, int O, typename... T> struct meta::map<M, TL_tuple<offset_type_list<O, T...> > > {
	typedef TL_tuple<offset_type_list<O, typename meta::map<M, T>::type...> > type;
};

template<int O, typename...T> static constexpr size_t tuple_size<TL_tuple<offset_type_list<O, T...>>> = sizeof...(T);


#endif

template<template<class> class M, typename... T> struct meta::map<M, TL_tuple<type_list<T...> > > {
	typedef TL_tuple<type_list<typename meta::map<M, T>::type...> > type;
};

#if 0
template<class F, class T, size_t ...I> inline constexpr auto apply_imp(F &&f, T &&t, meta::index_list<I...>) {
	return f(t.template get<I>()...);
}
template<class F, class T> inline constexpr auto apply(F &&f, T &&t) {
	return apply_imp(forward<F>(f), forward<T>(t), meta::make_index_list<tuple_size<noref_cv_t<T>>>());
}
template<class F, class T, size_t ...I> inline constexpr  auto apply_each_array_imp(F &&f, const meta::array<T, sizeof...(I)> &t, meta::index_list<I...>) {
	return meta::array<decltype(f(declval<T>())), sizeof...(I)>{{f(t[I])...}};
}
template<class F, class T, size_t N> inline constexpr auto apply_each(F &&f, meta::array<T, N> t) {
	return apply_each_array_imp(forward<F>(f), t, meta::make_index_list<N>());
}
#endif

template<typename... T> constexpr auto make_tuple(T&&... t) {
	return tuple<T...>(forward<T>(t)...);
}

//-----------------------------------------------------------------------------
//	as_tuple
//-----------------------------------------------------------------------------

template<typename T>		struct TL_fields;
template<typename T>		struct TL_fields<const T> : TL_fields<T> {};

template<typename T, typename TL = typename TL_fields<T>::type> struct as_tuple : TL_tuple<TL> {};

template<typename T> struct as_tuple<T, typename TL_fields<T>::type> : TL_tuple<typename TL_fields<T>::type> {
	operator T&()				{ return *(T*)this; }
	operator const T&() const	{ return *(T*)this; }
};

template<template<class> class M, typename T, typename TL> struct meta::map<M, as_tuple<T, TL> > {
	typedef as_tuple<T, typename meta::map<M, TL>::type> type;
};

//-----------------------------------------------------------------------------
//	tuple_iterator
//-----------------------------------------------------------------------------

template<typename... I> struct tuple_iterator : tuple<I...> {
	typedef	tuple<I...> B;
//	typedef tuple<typename iterator_traits<I>::reference...>	element, reference;

	template<size_t... II> auto		deref(index_list<II...>&&)	const	{ return make_tuple(*B::template get<II>()...); }
	template<size_t... II> void		inc(index_list<II...>&&)			{ unused(B::template get<II>()++...); }

	tuple_iterator(const I&... i) : B(i...) {}

	constexpr decltype(auto)	operator*()		const	{ return deref(meta::make_index_list<sizeof...(I)>()); }
	constexpr tuple_iterator&	operator++()			{ inc(meta::make_index_list<sizeof...(I)>()); return *this; }
	bool	operator!=(const tuple_iterator &b) const	{ return B::template get<0>() != b.template get<0>(); }
};

template<typename... I> tuple_iterator<I...> make_tuple_iterator(const I&... i) {
	return tuple_iterator<I...>(i...);
}

template<typename... C> auto make_tuple_range(C&&... c) {
	return make_range(make_tuple_iterator(c.begin()...), make_tuple_iterator(c.end()...));
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

template<typename V, typename...A> auto	variant_visit(V&& v, size_t i, A&&...a) {
	return i ? variant_visit(forward<V>(v), i - 1, variant_tail(forward<A>(a))...) : v(variant_head(forward<A>(a))...);
}
template<typename V, typename T, typename...A> auto	variant_visit(V&& v, size_t i, union_of<T> &a0, A&&...a) {
	return v(a0.t, variant_head(forward<A>(a))...);
}
template<typename V, typename T, typename...A> auto	variant_visit(V&& v, size_t i, const union_of<T> &a0, A&&...a) {
	return v(a0.t, variant_head(forward<A>(a))...);
}

template<typename... T> class variant {
	template<typename U> static constexpr size_t best_index = meta::VT_find<best_match_t<U, T..., _none>, T..., _none>;
	union_of<T..., _none>		u;
public:
	uint_bits_t<klog2<sizeof...(T) + 1>> index;
	template<size_t I>	using alternative_t = meta::VT_index_t<I, T...>;

	template<typename V, typename...A> auto	visit(V&& v, A&&...a)		{ return variant_visit(forward<V>(v), index, u, variant_head(forward<A>(a))...); }
	template<typename V, typename...A> auto	visit(V&& v, A&&...a) const	{ return variant_visit(forward<V>(v), index, u, variant_head(forward<A>(a))...); }

	variant()					: index(0) {}
	variant(const variant &b)	: index(b.index) { visit(op_construct(), b); }
	variant(variant &&b)		: index(b.index) { visit(op_construct(), move(b)); }
	variant(variant &b)			: variant((const variant&)b)	{}
	template<typename A, size_t I = best_index<A>> variant(A &&a) : index(I) { construct(u.template get<I>(), forward<A>(a));}
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

	template<size_t I>		optional<alternative_t<I>>	get() const { if (I != index) return {}; return u.template get<I>(); }
	template<typename U>	auto						get() const { return get<best_index<U>>(); }
	template<size_t I>		optional<alternative_t<I>&>	get()		{ if (I != index) return {}; return u.template get<I>(); }
	template<typename U>	auto						get()		{ return get<best_index<U>>(); }

	constexpr bool operator==(const variant &b)	const	{ return index == b.index && visit(equal_to(), b); }
	constexpr bool operator< (const variant &b)	const	{ return index < b.index || (index == b.index && visit(less(), b)); }
	constexpr bool operator<=(const variant &b)	const	{ return index < b.index || (index == b.index && visit(less_equal(), b)); }
	constexpr bool operator!=(const variant &b)	const	{ return !(*this == b); }
	constexpr bool operator> (const variant &b)	const	{ return b < *this; }
	constexpr bool operator>=(const variant &b)	const	{ return b <= *this; }

	friend auto&	variant_head(variant &v)		{ return v.u; }
	friend auto&	variant_head(const variant &v)	{ return v.u; }
	friend auto&&	variant_head(variant &&v)		{ return move(v.u); }
};

}//namespace iso

#endif // TYPELIST_H
