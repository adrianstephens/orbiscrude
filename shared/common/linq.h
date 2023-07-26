#include "base/array.h"
#include "base/strings.h"
#include "base/algorithm.h"
#include "base/hash.h"

//-----------------------------------------------------------------------------
//	LINQ:
//	Language Integrated Query
//	eg.
//		auto	w2	= where([](int v) { return v % 3 == 0; })
//					| where([](int v) { return v < 50; });
//		auto	x2 = int_range<int>(100) | w2 | to_array();
//
//		for (auto i : from(int_range<int>(100)) | where([](int v) { return v % 3 == 0; }) | order_by([](int a, int b) { return a > b; })) {
//		}
//-----------------------------------------------------------------------------

namespace iso { namespace linq {

template<typename R>	using front_t = decltype(declval<R>().front());

//-----------------------------------------------------------------------------
//	op - operation tag
//-----------------------------------------------------------------------------

template<typename T> struct op;
template<typename A, typename B> struct op<type_list<A, B>> {
	op<A>	a;
	op<B>	b;
	op(op<A> &&_a, op<B> &&_b) : a(move(_a)), b(move(_b)) {}
};

template<typename A, typename B> auto operator|(op<A> &&a, op<B> &&b) {
	return op<type_list<A, B>>(move(a), move(b));
}

//-----------------------------------------------------------------------------
//	impl - container implementing operation
//-----------------------------------------------------------------------------

template<typename T, typename R> struct impl;

template<typename T, typename R> impl<T, R> implement(R &&range, op<T> &&b) {
	return impl<T, R>(forward<R>(range), move(b));
}


//-----------------------------------------------------------------------------
//	iterator - gives container iterator access
//-----------------------------------------------------------------------------

template<typename T, typename R> struct iterator {
	impl<T,R>	*r;
	iterator(impl<T,R> *r)	: r(r->empty() ? nullptr : r) {}
	iterator(nullptr_t)		: r(nullptr) {}
	auto		operator*()						const	{ return r->front(); }
	bool		operator!=(const iterator &b)	const	{ return r != b.r; }
	iterator&	operator++()							{ if (!r->next()) r = nullptr; return *this; }
};

template<typename T, typename R> struct iterator2 {
	space_for<impl<T, R>>	r;
	bool		done;
	iterator2()						: done(true) {}
	iterator2(impl<T, R>&& x)		: done(x.empty())	{ if (!done) new(r) impl<T, R>(move(x)); }
	iterator2(const impl<T, R>& x)	: done(x.empty())	{ if (!done) new(r) impl<T, R>(x); }
	auto		operator*()						const	{ return r->front(); }
	bool		operator!=(const iterator2& b)	const	{ return done != b.done; }
	iterator2&	operator++()							{ done = !r->next(); return *this; }
};

template<typename T, typename R> iterator<T, R>		begin(impl<T, R>& r)		{ return &r; }
template<typename T, typename R> iterator<T, R>		end(impl<T, R>&)			{ return nullptr; }
template<typename T, typename R> iterator2<T, R>	begin(const impl<T, R>& r)	{ return r; }
template<typename T, typename R> iterator2<T, R>	end(const impl<T, R>&)		{ return {}; }

//-----------------------------------------------------------------------------
//	from:	can also adapt normal container to linq
//	from(C)
//	from(begin, end)
//-----------------------------------------------------------------------------

template<typename I> struct from_s;
template<typename I> struct impl<from_s<I>, void> {
	I	c_begin, c_end;
	impl(const I &begin, const I &end) : c_begin(begin), c_end(end) {}
	bool			next()				{ return ++c_begin != c_end; }
	decltype(auto)	front()		const	{ return *c_begin; }
	bool			empty()		const	{ return c_begin == c_end; }
};

template<typename I> inline auto from(const I &begin, const I &end) {
	return impl<from_s<I>, void>(begin, end);
}

template<typename C> inline auto from(C &&c) {
	using iso::begin; using iso::end;
	return from(begin(c), end(c));
}

template<typename T, typename R> auto from(impl<T, R> &&range) {
	return move(range);
}

template<typename R, typename T> auto operator|(R &&range, op<T> &&b) {
	return implement(from(forward<R>(range)), move(b));
}
template<typename R, typename T> auto operator|(R &&range, const op<T> &b) {
	return implement(from(forward<R>(range)), copy(b));
}
template<typename R, typename A, typename B> auto operator|(R &&range, op<type_list<A, B>> &&b) {
	return implement(from(forward<R>(range)), move(b.a)) | move(b.b);
}
template<typename R, typename A, typename B> auto operator|(R &&range, const op<type_list<A, B>> &b) {
	return implement(from(forward<R>(range)), copy(b.a)) | copy(b.b);
}

//-----------------------------------------------------------------------------
//	Projection Operations:
//	C | transform(X)
//-----------------------------------------------------------------------------

template<typename X> struct transform_s;
template<typename X> struct op<transform_s<X>> {
	X	xform;
	op(X &&xform) : xform(forward<X>(xform)) {}
};

template<typename X, typename R> struct impl<transform_s<X>, R> : R {
	X	xform;
	impl(R &&r, op<transform_s<X>> &&t) : R(move(r)), xform(move(t.xform)) {}
	auto	front() const	{ return xform(R::front()); }
};

template<typename X> inline auto transform(X &&xform) {
	return op<transform_s<X>>(move(xform));
}

//-----------------------------------------------------------------------------
//	Filtering Operations:
//	C | where(lambda)
//	C | except(C2)
//	C | intersect(C2)
//	C | distinct()
//-----------------------------------------------------------------------------

template<typename P> struct where_s;
template<typename P> struct op<where_s<P>> {
	P	predicate;
	op(P &&predicate) : predicate(predicate) {}
};

template<typename P, typename R> struct impl<where_s<P>, R> : R {
	P	predicate;
	impl(R &&r, op<where_s<P>> &&t) : R(move(r)), predicate(move(t.predicate)) {
		if (!R::empty())
			while (!predicate(R::front()) && R::next())
				;
	}
	bool	next() {
		while (R::next()) {
			if (predicate(R::front()))
				return true;
		}
		return false;
	}
};

template<typename P> inline auto where(P &&predicate) {
	return op<where_s<P>>(forward<P>(predicate));
}

template<typename P, typename T> inline auto where(T&& t) {
	return where(op_bind_second<P, T>(forward<T>(t)));
}

template<typename C2> inline auto except(const C2 &c2) {
	typedef element_t<C2>	E;
	hash_set<E>				seen;
	seen.insert(begin(c2), end(c2));
	return where([=](const E &e) { return !seen.count(e); });
}

template<typename C2> inline auto intersect(const C2 &c2) {
	typedef element_t<C2>	E;
	hash_set<E>				seen;
	seen.insert(begin(c2), end(c2));
	return where([=](const E &e) { return seen.count(e); });
}

struct distinct {
	template<typename R> friend auto operator|(R &&range, distinct&&) {
		typedef noref_t<decltype(range.front())>	E;
		hash_set<E>	seen;
		return forward<R>(range) | where([seen](const E &e) mutable { return !seen.check_insert(e); });
	}
};

//-----------------------------------------------------------------------------
//	Partitioning Operations:
//	C | skip(int)
//	C | skip_while(lambda)
//	C | take(int)
//	C | take_while(lambda)
//-----------------------------------------------------------------------------

//	skip
struct skip_s;
template<> struct op<skip_s> {
	int		n;
	op(int n) : n(n) {}
};
template<typename R> struct impl<skip_s, R> : R {
	impl(R &&r, op<skip_s> &&t) : R(r) {
		for (int i = t.n; i--;)
			R::next();
	}
};
inline auto skip(int n) {
	return op<skip_s>(n);
}

//	skip_while
template<typename P> struct skip_while_s;
template<typename P> struct op<skip_while_s<P>> {
	P	predicate;
	op(P &&predicate) : predicate(move(predicate)) {}
};
template<typename P, typename R> struct impl<skip_while_s<P>, R> : R {
	impl(R &&r, op<skip_while_s<P>> &&t) : R(r) {
		while (t.predicate(R::front()) && R::next())
			;
	}
};
template<typename P> inline auto skip_while(P &&predicate) {
	return op<skip_while_s<P>>(move(predicate));
};

//	take
struct take_s;
template<> struct op<take_s> {
	int		n;
	op(int n) : n(n) {}
};
template<typename R> struct impl<take_s, R> : R {
	int		n;
	impl(R &&r, op<take_s> &&t) : R(r), n(t.n) {}
	bool	next() { return n-- > 0 && R::next(); }
};
inline auto take(int n) {
	return op<take_s>(n);
}

//	take_while
template<typename P> struct take_while_s;
template<typename P> struct op<take_while_s<P>> {
	P	predicate;
	op(P &&predicate) : predicate(move(predicate)) {}
};
template<typename P, typename R> struct impl<take_while_s<P>, R> : R {
	P	predicate;
	impl(R &&r, op<take_while_s<P>> &&t) : R(r), predicate(move(t.predicate)) {}
	bool	next() { return R::next() && predicate(R::front()); }
};

template<typename P> inline auto take_while(P &&predicate) {
	return op<take_while_s<P>>(move(predicate));
};

//-----------------------------------------------------------------------------
//	Element Operations:
//	first(C)
//	first_or_default(C, def)
//	last(C)
//	last_or_default(C, def)
//	at(C, int)
//	at_or_default(C, int, def)
//-----------------------------------------------------------------------------

//	first
struct first_s;
template<> struct op<first_s> {
	template<typename R> friend auto operator|(R &&range, op&&) {
		return range.front();
	}
};
inline auto first() {
	return op<first_s>();
}

//	first_or_default
struct first_or_default_s;
template<> struct op<first_or_default_s> {
	template<typename R, typename T = front_t<R>> friend auto operator|(R &&range, op&&) {
		return !range.empty() ? range.front() : T();
	}
};
inline auto first_or_default() {
	return op<first_or_default_s>();
}

//	last
template<typename R> auto last(R &&range) {
	auto	last = range.front();
	while (range.next())
		last = range.front();
	return last;
}

struct last_s;
template<> struct op<last_s> {
	template<typename R> friend auto operator|(R &&range, op&&) {
		return last(range);
	}
};
inline auto last() {
	return op<last_s>();
}

//	last_or_default
struct last_or_default_s;
template<> struct op<last_or_default_s> {
	template<typename R, typename T = front_t<R>> friend auto operator|(R &&range, op&&) {
		return !range.empty() ? last(range) : T();
	}
};
inline auto last_or_default() {
	return op<last_or_default_s>();
}

//	at
struct at_s;
template<> struct op<at_s> {
	int	i;
	op(int i) : i(i) {}
	template<typename R> friend auto operator|(R &&r, op &&o) {
		while (o.i > 0 && r.next())
			--o.i;
		return r.front();
	}
};
inline auto at(int i) {
	return op<at_s>(i);
}

//	at_or_default
struct at_or_default_s;
template<> struct op<at_or_default_s> {
	int	i;
	op(int i) : i(i) {}
	template<typename R, typename T = front_t<R>> friend auto operator|(R &&r, op &&o) {
		if (!r.empty()) {
			while (o.i > 0 && r.next())
				--o.i;
			if (o.i == 0)
				return r.front();
		}
		return T();
	}
};
inline auto at_or_default(int i) {
	return op<at_or_default_s>(i);
}


//	single
struct single_s;
template<> struct op<single_s> {
	template<typename R> friend auto operator|(R &&r, op&&) {
		ISO_ASSERT(!r.empty());
		auto	val = r.front();
		ISO_ASSERT(!r.next());
		return val;
	}
};
inline auto single() {
	return op<single_s>();
}

//	single_or_default
struct single_or_default_s;
template<> struct op<single_or_default_s> {
	template<typename R, typename T = front_t<R>> friend auto operator|(R &&r, op&&) {
		if (!r.empty()) {
			auto	val = r.front();
			if (!r.next())
				return val;
		}
		return T();
	}
};
inline auto single_or_default() {
	return op<single_or_default_s>();
}

//-----------------------------------------------------------------------------
//	Sorting:
//	C | order_by(P)
//	order_by(P) | then_by(P)
//-----------------------------------------------------------------------------

template<typename P> struct order_by_s;
template<typename P> struct op<order_by_s<P>> {
	P		predicate;
	op(P &&predicate) : predicate(forward<P>(predicate)) {}
};

template<typename P, typename R> struct impl<order_by_s<P>, R> {
	typedef return_holder_t<front_t<R>>	E;
	P					predicate;
	dynamic_array<E>	pq;

	impl(R &&r, op<order_by_s<P>> &&t) : predicate(move(t.predicate)) {
		if (!r.empty()) {
			do
				pq.push_back(r.front());
			while (r.next());
			heap_make(pq.begin(), pq.end(), predicate);
		}
	}
	bool	empty()	const	{
		return pq.empty();
	}
	bool	next() {
		heap_pop(pq.begin(), pq.end(), predicate);
		pq.pop_back();
		return !pq.empty();
	}
	auto	front() const {
		return pq.front();
	}
};

template<typename P> inline auto order_by(P &&predicate) {
	return op<order_by_s<P>>(forward<P>(predicate));
}

template<typename P> struct then_by_s {
	P	predicate;
	then_by_s(P &&predicate) : predicate(forward<P>(predicate)) {}

	template<typename P1, typename R> friend auto operator|(impl<order_by_s<P1>, R> &&range, then_by_s &&t) {
		typedef front_t<R> X;
		P1	predicate1 = move(range.predicate);
		P	predicate2 = move(t.predicate);

		return range.pq | order_by([predicate1, predicate2](X a, X b) {
			return predicate1(a, b) || (!predicate1(b, a) && predicate2(a, b));
		});
	}
};

template<typename P> inline then_by_s<P> then_by(P &&predicate) {
	return forward<P>(predicate);
}

//-----------------------------------------------------------------------------
//	join
//-----------------------------------------------------------------------------

template<typename C2, typename S1, typename S2, typename J> struct join_s;
template<typename C2, typename S1, typename S2, typename J> struct op<join_s<C2, S1, S2, J>> {
	typedef noref_t<return_t<S2>>				K2;
	typedef return_holder_t<reference_t<C2>>	V2;

	hash_map<K2, V2>	map;
	S1					select1;
	J					joint;

	op(C2 &c2, S1 &&select1, S2 &&select2, J &&joint) : select1(forward<S1>(select1)), joint(forward<J>(joint)) {
		for (auto &&i : c2)
			map[select2(i)] = i;
	}
};

template<typename C2, typename S1, typename S2, typename J, typename R> struct impl<join_s<C2, S1, S2, J>, R> : R {
	typedef op<join_s<C2, S1, S2, J> >	T;
	using K2 = typename T::K2;
	using V2 = typename T::V2;
	hash_map<K2, V2>	map;
	S1					select1;
	J					joint;
	V2					*v2;
	impl(R &&r, T &&t) : R(forward<R>(r)), map(move(t.map)), select1(move(t.select1)), joint(move(t.joint))  {
		if (!R::empty())
			while (!(v2 = map.check(select1(R::front()))) && R::next())
				;
	}
	bool	next() {
		while (R::next()) {
			if (v2 = map.check(select1(R::front())))
				return true;
		}
		return false;
	}
	auto	front() const	{ return joint(R::front(), *v2); }
};

template<typename C2, typename S1, typename S2, typename J> inline auto join(C2 &c2, S1 &&select1, S2 &&select2, J &&joint) {
	return op<join_s<C2, S1, S2, J>>(c2, forward<S1>(select1), forward<S2>(select2), forward<J>(joint));
}

//-----------------------------------------------------------------------------
//	concat
//-----------------------------------------------------------------------------

template<typename R> struct concat_s;
template<typename R> struct op<concat_s<R>> {
	R	range;
	op(R &&range) : range(forward<R>(range)) {}
};
template<typename R1, typename R2> struct impl<concat_s<R2>, R1> {
	R1		range1;
	R2		range2;
	bool	use_r2;

	impl(R1 &&r, op<concat_s<R2>> &&t) : range1(forward<R1>(r)), range2(move(t.range)) {
		use_r2 = range1.empty();
	}
	bool	empty()	const	{
		return use_r2 ? range2.empty() : range1.empty();
	}
	bool	next() {
		if (!use_r2) {
			if (range1.next())
				return true;
			use_r2 = true;
			return !range2.empty();
		}
		return range2.next();
	}
	auto	front() const	{
		return use_r2 ? range2.front() : range1.front();
	}
};

template<typename R> inline auto concat(R &&range2) {
	return op<concat_s<R>>(forward<R>(range2));
}

//-----------------------------------------------------------------------------
//	functions:
//	C | to_array()	or to_array(C)
//	C | count()		or count(C)
//	C | sum()		or sum(C)
//	C | average()	or average(C)
//	C | min()		or min(C)
//	C | max()		or max(C)
//	C | contains(T)	or contains(C,T)
//	C | all(P)		or all(C,P)
//	C | any(P)		or any(C,P)
//-----------------------------------------------------------------------------

//	to_array(C)
template<typename R> inline auto to_array(R &&range) {
	typedef noref_t<front_t<R>>	E;
	dynamic_array<E>	a;
	if (!range.empty()) {
		do
			a.push_back(range.front());
		while (range.next());
	}
	return a;
}

struct to_array_s;
template<> struct op<to_array_s> {
	template<typename R> friend auto operator|(R &&range, op&&) {
		return to_array(range);
	}
};
inline auto to_array() {
	return op<to_array_s>();
}

//	count
template<typename R> inline size_t count(R &&range) {
	if (range.empty())
		return 0;
	size_t	n		= 1;
	while (range.next())
		++n;
	return n;
}
struct count_s;
template<> struct op<count_s> { template<typename R> friend auto operator|(R &&range, op&&) { return count(range); } };
inline auto count() { return op<count_s>(); }

//	sum
template<typename R> inline auto sum(R &&range) {
	if (range.empty())
		return 0;
	auto	result = range.front();
	while (range.next())
		result += range.front();
	return result;
}

struct sum_s;
template<> struct op<sum_s> { template<typename R> friend auto operator|(R &&range, op&&) { return sum(range); } };
inline auto sum() { return op<sum_s>(); }

//	average
template<typename R> inline auto average(R &&range) {
	ISO_ASSERT(!range.empty());
	size_t	n		= 1;
	auto	result	= range.front();
	while (range.next()) {
		result += range.front();
		++n;
	}
	return result / n;
}

struct average_s;
template<> struct op<average_s> { template<typename R> friend auto operator|(R &&range, op&&) { return average(range); } };
inline auto average() { return op<average_s>(); }


//	max
template<typename R> inline auto max(R &&range) {
	ISO_ASSERT(!range.empty());
	auto	result	= range.front();
	while (range.next())
		result = max(result, range.front());
	return result;
}

struct max_s;
template<> struct op<max_s> { template<typename R> friend auto operator|(R &&range, op&&) { return max(range); } };
inline auto max() { return op<max_s>(); }


//	min
template<typename R> inline auto min(R &&range) {
	ISO_ASSERT(!range.empty());
	auto	result	= range.front();
	while (range.next())
		result = min(result, range.front());
	return result;
}

struct min_s;
template<> struct op<min_s> { template<typename R> friend auto operator|(R &&range, op&&) { return min(range); } };
inline auto min() { return op<min_s>(); }


//	contains
template<typename R, typename T> inline bool contains(R &&range, const T &t) {
	if (!range.empty()) {
		do {
			if (range.front() == t)
				return true;
		} while (range.next());
	}
	return false;
}

template<typename T> struct contains_s;
template<typename T> struct op<contains_s<T>> {
	T	t;
	op(T &&t) : t(move(t)) {}
	template<typename R> friend auto operator|(R &&range, op &&o) { return contains(range, o.t); }
};
template<typename T> inline auto contains(T &&t) { return op<contains_s<T>>(move(t)); }

//	all
template<typename R, typename P> inline bool all(R &&range, P &&predicate) {
	if (!range.empty()) {
		do {
			if (!predicate(range.front()))
				return false;
		} while (range.next());
	}
	return true;
}

template<typename P> struct all_s;
template<typename P> struct op<all_s<P>> {
	P	predicate;
	op(P &&predicate) : predicate(move(predicate)) {}
	template<typename R> friend auto operator|(R &&r, op &&o) { return all(r, move(o.predicate)); }
};
template<typename P> inline auto all(P &&predicate) { return op<all_s<P>>(move(predicate)); }


//	any
template<typename R, typename P> inline bool any(R &&range, P &&predicate) {
	if (!range.empty()) {
		do {
			if (predicate(range.front()))
				return true;
		} while (range.next());
	}
	return false;
}

template<typename P> struct any_s;
template<typename P> struct op<any_s<P>> {
	P	predicate;
	op(P &&predicate) : predicate(move(predicate)) {}
	template<typename R> friend auto operator|(R &&r, op &&o) { return any(r, move(o.predicate)); }
};
template<typename P> inline auto any(P &&predicate) { return op<any_s<P>>(move(predicate)); }

}

// namespace iso



} // namespace iso::linq

