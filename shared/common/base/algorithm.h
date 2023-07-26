#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "deferred.h"
#include "defs.h"
#include "functions.h"

namespace iso {

//-----------------------------------------------------------------------------
//	reduce
//-----------------------------------------------------------------------------

template<class I, class P, class T = it_element_t<I>>	T reduce(I i, I end, T t = {}, P bin = {}) {
	for (; i != end; ++i)
		t = bin(t, *i);
	return t;
}
template<class P, class I, class T = it_element_t<I>>	T reduce(I i, I end, T t = {}) {
	return reduce(i, end, t, P());
}
template<class C, class P, class T = element_t<C>>		T reduce(C&& c, T t = {}, P bin = {}) {
	return reduce(begin(c), end(c), t, bin);
}
template<class P, class C, class T = element_t<C>> 		T reduce(C&& c, T t = {}) {
	return reduce(begin(c), end(c), t, P{});
}

//-----------------------------------------------------------------------------
//	for_each
//-----------------------------------------------------------------------------

template<class I, class F> inline F for_each(I i, I end, F f) {
	for (; i != end; ++i)
		f(*i);
	return f;
}
template<class I, class F> inline F for_eachn(I i, size_t n, F f) {
	while (n--) {
		f(*i);
		++i;
	}
	return f;
}
template<class C, class F> inline F for_each(C&& c, F f) { return for_each(begin(c), end(c), f); }

// for_each2
template<class I1, class I2, class F> inline F for_each2(I1 i1, I1 end, I2 i2, F f) {
	for (; i1 != end; ++i1, ++i2)
		f(*i1, *i2);
	return f;
}
template<class I1, class I2, class F> inline F for_each2n(I1 i1, I2 i2, size_t n, F f) {
	while (n--) {
		f(*i1, *i2);
		++i1;
		++i2;
	}
	return f;
}
template<class C1, class C2, class F> inline F for_each2(C1&& c1, C2&& c2, F f) { return for_each2(begin(c1), end(c1), begin(c2), f); }

// for_each3
template<class I1, class I2, class I3, class F> inline F for_each3(I1 i1, I1 end, I2 i2, I3 i3, F f) {
	for (; i1 != end; ++i1, ++i2, ++i3)
		f(*i1, *i2, *i3);
	return f;
}
template<class I1, class I2, class I3, class F> inline F for_each3n(I1 i1, I2 i2, I3 i3, size_t n, F f) {
	while (n--) {
		f(*i1, *i2, *i3);
		++i1;
		++i2;
		++i3;
	}
	return f;
}
template<class C1, class C2, class C3, class F> inline F for_each3(C1&& c1, C2&& c2, C3&& c3, F f) { return for_each3(begin(c1), end(c1), begin(c2), begin(c3), f); }

//-----------------------------------------------------------------------------
//	copies
//-----------------------------------------------------------------------------

template<class A, class B> inline void swap_ranges(A first, A last, B dst)	{ for_each2(first, last, dst, op_swap()); }
template<class A, class B> inline void swap_ranges(A&& a, B&& b)			{ swap_ranges(a.begin(), a.end(), b.begin()); }


//-----------------------------------------------------------------------------
//	reverse
//-----------------------------------------------------------------------------

template<class I> inline void reverse(I a, I b) {
	while (a != b && a != --b)
		swap(*a++, *b);
}
template<class C> inline void reverse(C&& c) {
	reverse(begin(c), end(c));
}

// a..b with decreasing c
template<class I> inline I _reverse0(I a, I b, I c) {
	while (a < b)
		swap(*a++, *--c);
	return c;
}

// b..c with increasing a
template<class I> inline I _reverse1(I a, I b, I c) {
	while (b < c)
		swap(*a++, *--c);
	return a;
}

// exchange a..b with b..c
template<class I> inline void block_exchange(I a, I b, I c) {
	reverse(a, b);
	reverse(b, c);
	reverse(a, c);
}

// same - less moves?
template<typename I> inline void block_exchange2(I a, I b, I c) {
	while (a != b && b != c) {
		if (b - a <= c - b) {
			swap_ranges(a, b, b);
			a = exchange(b, b + (b - a));
		} else {
			I	t = b - (c - b);
			swap_ranges(t, b, b);
			c = exchange(b, t);
		}
	}
}

// exchange a..b with c..d
template<class I> inline void block_exchange(I a, I b, I c, I d) {
	reverse(a, b);
	reverse(c, d);
	if (b - a <= d - c)
		reverse(c, _reverse0(a, b, d));
	else
		reverse(_reverse1(a, c, d), b);
}

template<class I> void _rotate(I first, I middle, I last, bidirectional_iterator_t) {
	block_exchange(first, middle, last);
}
template<class I> void _rotate(I first, I middle, I last, forward_iterator_t) {
	for (I next = middle; first != next; ) {
		swap(*first++, *next++);
		if (next == last)
			next = middle;
		else if (first == middle)
			middle = next;
	}
}

template<class I> void rotate(I first, I middle, I last) {
	return _rotate(first, middle, last, iterator_category<I>());
}

template<class I> void _rotate1(I first, I middle, bidirectional_iterator_t) {
	auto	temp = *middle;
	for (I p = middle; p > first; --p)
		*p = *(p - 1);
	*first = temp;
}
template<class I> void _rotate1(I first, I middle, forward_iterator_t) {
	while (first != middle)
		swap(*first++, *middle);
}

// equivalent to rotate(first, middle, middle + 1) {
template<class I> void rotate1(I first, I middle) {
	return _rotate1(first, middle, iterator_category<I>());
}

//-----------------------------------------------------------------------------
//	transforms
//-----------------------------------------------------------------------------

template<class F> struct transformer {
	F f;
	transformer(F f) : f(f) {}
	template<typename P1, typename R> 				void operator()(const P1& p1, R& r) const { r = f(p1); }
	template<typename P1, typename P2, typename R>	void operator()(const P1& p1, const P2& p2, R& r) const { r = f(p1, p2); }
};

template<class I, class O, class F> inline void				transform(I first, I last, O dst, F f)					{ for_each2(first, last, dst, transformer<F>(f)); }
template<class C, class O, class F> inline void				transform(C&& c, O&& o, F f)							{ for_each2(forward<C>(c), forward<O>(o), transformer<F>(f)); }
template<class I1, class I2, class O, class F> inline void	transform(I1 first1, I1 last1, I2 first2, O dst, F f)	{ for_each3(first1, last1, first2, dst, transformer<F>(f)); }
template<class I, class O, class F> inline void				transformn(I first, O dst, F f, size_t n)				{ for_each2n(first, dst, n, transformer<F>(f)); }
template<class I1, class I2, class O, class F> inline O		transformn(I1 first1, I2 first2, O dst, F f, size_t n)	{ for_each3n(first1, first2, dst, n, transformer<F>(f)); }

template<class C, class I> inline void append(C &&c, I first, I last) {
	for (; first != last; ++first)
		c.push_back(*first);
}
template<class C1, class C2> inline void append(C1 &&c1, C2 &&c2)	{ append(c1, begin(c2), end(c2)); }

template<class C, class I> inline void prepend(C &&c, I first, I last) {
	for (; first != last; ++first)
		c.push_front(*first);
}
template<class C1, class C2> inline void prepend(C1 &&c1, C2 &&c2)	{ prepend(c1, begin(c2), end(c2)); }

template<class F> struct counter {
	F   f;
	int n;
	counter(F f) : n(0), f(f) {}
	template<typename P1> 				void operator()(const P1& p1) { n += int(f(p1)); }
	template<typename P1, typename P2> 	void operator()(const P1& p1, const P2& p2) { n += int(f(p1, p2)); }
	operator int() const { return n; }
};

template<class I, class F> inline int count(I first, I last, F f)	{ return for_each(first, last, counter<F>(f)); }
template<class C, class F> inline int count(C &&c, F f)				{ return for_each(c, counter<F>(f)); }


//-----------------------------------------------------------------------------
//	finds
//-----------------------------------------------------------------------------

template<class P, class I> inline I find_best(I i, I end, P pred = P()) {
	I best = i;
	while (++i != end) {
		if (pred(*i, *best))
			best = i;
	}
	return best;
}

template<class P, class C> inline auto	find_best(C &&c, P pred = P())		{ return find_best(begin(c), end(c), pred); }
template<class C> inline auto argmax(C&& c) { return find_best<greater>(c); }
template<class C> inline auto argmin(C&& c) { return find_best<less>(c); }

template<class P, class I, class V> inline I find_best_value(I i, I end, V& result, P pred = P()) {
	typedef decltype(*i)	T;
	I	bestu	= i;
	T	bestv	= *i;
	while (++i != end) {
		T	iv = *i;
		if (pred(iv, bestv)) {
			besti	= i;
			bestv	= move(iv);
		}
	}
	result = move(bestv);
	return besti;
}

template<class P, class I> inline I find_if(I i, I end, P pred = P()) {
	while (i != end) {
		if (pred(*i))
			break;
		++i;
	}
	return i;
}
template<class P, class I> inline I find_ifn(I i, size_t n, P pred = P()) {
	while (n--) {
		if (pred(*i))
			break;
		++i;
	}
	return i;
}


template<class P, class C> inline auto	find_if(C &&c, P pred = P())		{ return find_if(begin(c), end(c), pred); }

template<class I, class T> inline auto	find(I i, I end, T&& t)				{ return find_if(i, end, op_bind_second<equal_to, T>(forward<T>(t))); }
template<class I, class T> inline auto	findn(I i, T&& t, size_t n)			{ return find_ifn(i, op_bind_second<equal_to, T>(forward<T>(t)), n); }
template<class C, class T> inline auto	find(C &&c, T&& t)					{ return find(begin(c), end(c), forward<T>(t)); }

template<class P, class I> inline bool	all_of( I i, I end, P p = P())		{ return find_if(i, end, make_not(p)) == end; }
template<class P, class I> inline bool	any_of( I i, I end, P p = P())		{ return find_if(i, end, p) != end; }
template<class P, class I> inline bool	none_of(I i, I end, P p = P())		{ return find_if(i, end, p) == end; }
template<class P, class C> inline bool	all_of( C &&c, P p = P())			{ return all_of(begin(c), end(c), p); }
template<class P, class C> inline bool	any_of( C &&c, P p = P())			{ return any_of(begin(c), end(c), p); }
template<class P, class C> inline bool	none_of(C &&c, P p = P())			{ return none_of(begin(c), end(c), p); }


template<class I1, class I2, class P> inline auto	find_if2(I1 i1, I1 e1, I2 i2, P pred) {
	while (i1 != e1) {
		if (pred(*i1, *i2))
			break;
		++i1;
		++i2;
	}
	return i1;
}

template<class P, class C1, class I2> inline auto	find_if2(C1 &&c1, I2 i2, P p = P())			{ return find_if2(begin(c1), end(c1), i2, p); }
template<class P, class I1, class I2> inline bool	all_of2( I1 i1, I1 e1, I2 i2, P p = P())	{ return find_if2(i1, e1, i2, make_not(p)) == e1; }
template<class P, class I1, class I2> inline bool	any_of2( I1 i1, I1 e1, I2 i2, P p = P())	{ return find_if2(i1, e1, i2, p) != e1; }
template<class P, class I1, class I2> inline bool	none_of2(I1 i1, I1 e1, I2 i2, P p = P())	{ return find_if2(i1, e1, i2, p) == e1; }
template<class P, class C1, class I2> inline bool	all_of2( C1 &&c1, I2 i2, P p = P())			{ return all_of2(begin(c1), end(c1), i2, p); }
template<class P, class C1, class I2> inline bool	any_of2( C1 &&c1, I2 i2, P p = P())			{ return any_of2(begin(c1), end(c1), i2, p); }
template<class P, class C1, class I2> inline bool	none_of2(C1 &&c1, I2 i2, P p = P())			{ return none_of2(begin(c1), end(c1), i2, p); }

// return fail instead of end

template<class I, class T> inline I find_check(I begin, I end, const T& t, I fail = I()) {
	I i = find(begin, end, t);
	return i == end ? fail : i;
}
template<class C, class T, class I = iterator_t<C>> inline I	find_check(C &&c, const T& t, I fail = I()) { return find_check(begin(c), end(c), t, fail); }

template<class I, class P> inline I find_if_check(I begin, I end, P pred, I fail = I()) {
	I i = find_if(begin, end, pred);
	return i == end ? fail : i;
}
template<class C, class P, class I = iterator_t<C>> inline I	find_if_check(C &&c, P pred, I fail = I()) { return find_if_check(begin(c), end(c), pred, fail); }

//-----------------------------------------------------------------------------
// remove/unique
//-----------------------------------------------------------------------------

template<typename I, class P = equal_to> I unique(I i, I end, P pred = P()) {
	if (i == end)
		return end;

	I prev = i;
	while (++i != end) {
		if (!pred(*prev, *i))
			*++prev = move(*i);
	}
	return ++prev;
}

template<class C, class P = equal_to> inline auto unique(C&& c, P pred = P()) {
	return unique(begin(c), end(c), pred);
}

template<typename I, class P> I remove(I i, I end, P pred) {
	while (i != end && !pred(*i))
		++i;

	I last = i;
	while (i != end) {
		if (!pred(*i))
			*last++ = move(*i);
		++i;
	}
	return last;
}

template<class C, class P> inline auto remove(C&& c, P pred) {
	return remove(begin(c), end(c), pred);
}

template<class I, class P> inline I remove_unordered(I i, I end, P pred) {
	while (i != end) {
		if (pred(*i))
			swap(*i, *--end);
		else
			++i;
	}
	return i;
}

template<class C, class P> inline auto remove_unordered(C&& c, P pred) {
	return remove_unordered(begin(c), end(c), pred);
}

//-----------------------------------------------------------------------------
// action: perform action if item found
//-----------------------------------------------------------------------------

template<class C, class I, class A> void _action(C&& c, I i, A act) {
	if (i != c.end())
		act(c, i);
}

template<class C, class P, class A> inline void action_if(C&& c, P pred, A act) { return _action(c, find_if(begin(c), end(c), pred), act); }
//template<class C, class T, class A> inline void action(C& c, const T& t, A act) { return _action(c, find(begin(c), end(c), t), act); }

//-----------------------------------------------------------------------------
// upper/lower bound
//-----------------------------------------------------------------------------

// with value + predicate

template<class I0, class I1, class T, class P> inline I0 _lower_bound(I0 first, I1 last, T&& t, P pred, input_iterator_t) {
	while (first != last && pred(*first, t))
		++first;
	return first;
}
template<class I, class T, class P> inline I _lower_bound(I first, I last, T&& t, P pred, random_access_iterator_t) {
	for (auto n = distance(first, last); n; n >>= 1) {
		I middle = first + (n >> 1);
		if (pred(*middle, t)) {
			first = ++middle;
			--n;
		}
	}
	return first;
}

template<class I0, class I1, class T, class P=less>	inline auto	lower_bound(I0 first, I1 last, T&& t, P pred = P())	{
	return _lower_bound(first, last, t, pred, iterator_category<I0>());
}
template<class C, class T, class P=less>			inline auto	lower_boundc(C&& c, T&& t, P pred = P()) {
	return lower_bound(begin(c), end(c), t, pred);
}
template<class I0, class I1, class T, class P=less>	inline auto	upper_bound(I0 first, I1 last, T&& t, P pred = P())	{
	return lower_bound(first, last, t, !flip(pred));
}
template<class C, class T, class P=less>			inline auto	upper_boundc(C&& c, T&& t, P pred = P()) {
	return lower_boundc(c, t, !flip(pred));
}

//...with unary predicate

template<class I, class P> inline I _first_not(I first, I last, P pred, input_iterator_t) {
	while (first != last && pred(*first))
		++first;
	return first;
}
template<class I, class P> inline I _first_not(I first, I last, P pred, random_access_iterator_t) {
	for (auto n = distance(first, last); n; n >>= 1) {
		I middle = first + (n >> 1);
		if (pred(*middle)) {
			first = ++middle;
			--n;
		}
	}
	return first;
}

template<class I, class P> inline I		first_not(I first, I last, P pred)	{ return _first_not(first, last, pred, iterator_category<I>()); }
template<class C, class P> inline auto	first_not(C&& c, P pred)			{ return first_not(begin(c), end(c), pred); }


template<class I0, class I1, class T, class P = less> auto binary_find(I0 first, I1 last, const T& t, I0 fail = I0(), P pred = P()) {
	first = lower_bound(first, last, t, pred);
	return first != last && !(t < *first) ? first : fail;
}
template<class C, class T, class P = less> inline auto binary_findc(C &&c, const T& t, iterator_t<C> fail = iterator_t<C>(), P pred = P()) {
	return binary_find(begin(c), end(c), t, pred, fail);
}

// gallop_find, aka exponential_search
template<typename T, class I, class P> I gallop_find(const T &key, I begin, I end, I start, P pred) {
	I	lo = start, hi = start;
	
	if (pred(*start, key)) {
		// gallop right, until *lo < key <= *hi
		for (size_t ofs = 1; (hi += ofs) < end && pred(*hi, key); ofs <<= 1)
			lo = hi;
		
		hi 	= min(hi, end);
		
	} else {
		// gallop left, until *lo < key <= *hi
		for (size_t ofs = 1; (lo -= ofs) >= begin && !pred(*lo, key); ofs <<= 1)
			hi = lo;

		lo	= max(lo, begin - 1);
	}

	// Now *lo < key <= *hi, so key belongs somewhere to the right of lo but no farther right than hi
	return lower_bound(lo + 1, hi, key, pred);
}

//-----------------------------------------------------------------------------
//	sorting
//-----------------------------------------------------------------------------

// get longest run in the slice [lo, hi)
template<class I, class P = less> I find_run(I lo, I hi, P pred = P()) {
    I	i = lo;
	while (++i < hi && !pred(*i, *lo))
		lo = i;
	return i;
}

template<class I, class P = less> 	bool check_sorted(I lo, I hi, P pred = P()) { return find_run(lo, hi, pred) == hi; }
template<class P, class I> 			bool check_sorted(I lo, I hi) 				{ return check_sorted(lo, hi, P()); }
template<class C, class P = less> 	bool check_sorted(C&& c, P comp = P())		{ return check_sorted(begin(c), end(c), comp); }
template<class P, class C> 			bool check_sorted(C&& c)					{ return check_sorted(c, P()); }

//----------------------------------------
// 	insertion_sort (stable)
// 	must have lo <= start <= hi, and that [lo, start) is already sorted
//----------------------------------------

template<class I, class P = less> void insertion_sort(I lo, I hi, I start, P comp = P()) {
    ISO_ASSERT(lo < start && start <= hi);
    for (; start < hi; ++start) {
		I	i = start, p;
		while ((p = i) > lo && comp(*start, *--i));
		rotate1(p, start);
	}
}
template<class P, class I> 			void insertion_sort(I lo, I hi, I start)	{ insertion_sort(lo, hi, start, P()); }
template<class I, class P = less> 	void insertion_sort(I lo, I hi, P pred = P()) { insertion_sort(lo, hi, lo + 1, pred); }
template<class P, class I> 			void insertion_sort(I lo, I hi)				{ insertion_sort(lo, hi, P()); }
template<class C, class P = less> 	void insertion_sort(C&& c, P comp = P())	{ insertion_sort(begin(c), end(c), comp); }
template<class P, class C> 			void insertion_sort(C&& c)					{ insertion_sort(c, P()); }

//----------------------------------------
//	binary_sort (insertion sort with bisection) (stable)
//  must have lo <= start <= hi, and that [lo, start) is already sorted
//----------------------------------------

template<class I, class P = less> void binary_sort(I lo, I hi, I start, P pred = P()) {
    ISO_ASSERT(lo < start && start <= hi);
    for (; start < hi; ++start) {
		I	i	= upper_bound(lo, start, *start, pred);
		rotate1(i, start);
    }
}

template<class P, class I> 			void binary_sort(I lo, I hi, I start) 		{ binary_sort(lo, hi, start, P()); }
template<class I, class P = less> 	void binary_sort(I lo, I hi, P pred = P()) 	{ binary_sort(lo, hi, lo + 1, pred); }
template<class P, class I> 			void binary_sort(I lo, I hi) 				{ binary_sort(lo, hi, lo + 1, P()); }
template<class C, class P = less> 	void binary_sort(C&& c, P comp = P())		{ binary_sort(begin(c), end(c), comp); }
template<class P, class C> 			void binary_sort(C&& c)						{ binary_sort(c, P()); }

//----------------------------------------
//	selection_sort
//----------------------------------------

template<class I, class P = less> void selection_sort(I lo, I hi, P comp = P()) {
	while (hi != lo) {
		auto	p = find_best(lo, hi, comp);
		swap(*p, *lo);
		++lo;
	}
}

//----------------------------------------
//	sort (quick sort)
//----------------------------------------

template<class I, class P = less> enable_if_t<!is_lvalue_v<I>> sort(I lo, I hi, P comp = P()) {
	if (hi - lo < 2)
		return;

	struct stack_entry {
		I lo, hi;
		stack_entry(I lo, I hi) : lo(lo), hi(hi) {}
	};
	typedef void* dummy_entry[sizeof(stack_entry) / sizeof(void*)];
	dummy_entry   stack[30], *sp = stack;

	--hi;
	for (;;) {
		uintptr_t size = (hi - lo) + 1;

		if (size <= 8) {
			// below a certain size, it is faster to use a O(n^2) sorting method
			++hi;
			selection_sort(lo, hi, comp);

		} else {
			// Sort the first, middle, last elements into order
			I mid = lo + size / 2;

			if (comp(*mid, *lo))
				swap(*lo, *mid);

			if (comp(*hi, *lo))
				swap(*lo, *hi);

			if (comp(*hi, *mid))
				swap(*mid, *hi);

			I lo2 = lo, hi2 = hi;
			for (;;) {
				if (lo2 < mid)
					while (++lo2 < mid && !comp(*mid, *lo2))
						;
				if (lo2 >= mid)
					while (++lo2 <= hi && !comp(*mid, *lo2))
						;

				while (--hi2 > mid && comp(*mid, *hi2))
					;

				if (hi2 < lo2)
					break;

				swap(*lo2, *hi2);

				// If the partition element was moved, follow it
				// Only need to check for mid == hi2, since before the swap, *lo2 > *mid => lo2 != mid.
				if (mid == hi2)
					mid = lo2;
			}

			++hi2;
			if (hi2 > mid)
				while (--hi2 > mid && !comp(*hi2, *mid))
					;
			if (hi2 <= mid)
				while (--hi2 > lo && !comp(*hi2, *mid))
					;

			if (hi2 - lo >= hi - lo2) {
				if (lo < hi2)
					new(sp++) stack_entry(lo, hi2);
				if (lo2 < hi) {
					lo = lo2;
					continue;
				}
			} else {
				if (lo2 < hi)
					new (sp++) stack_entry(lo2, hi);
				if (lo < hi2) {
					hi = hi2;
					continue;
				}
			}
		}

		if (sp == stack)
			break;

		stack_entry* e = (stack_entry*)--sp;
		lo			   = e->lo;
		hi			   = e->hi;
		e->~stack_entry();
	}
}

template<class P, class I> 			void sort(I lo, I hi)	{ sort(lo, hi, P()); }
template<class C, class P = less, typename=enable_if_t<has_begin_v<C>>>	void sort(C&& c, P comp = P())	{ sort(begin(c), end(c), comp); }
template<class P, class C> 			void sort(C&& c)		{ sort(c, P()); }

//----------------------------------------
//	reverse sort
//----------------------------------------

template<class C, class P> void reverse_sort(C&& c, P comp)	{
	sort(begin(c), end(c), flip(comp));
}

template<class C, class P = less> void sort_dir(bool up, C&& c, P comp = P()) {
	if (up)
		sort(begin(c), end(c), flip(comp));
	else
		sort(begin(c), end(c), comp);
}
template<class P, class C> void sort_dir(bool up, C&& c) { return sort_dir(up, c, P()); }

//----------------------------------------
//	firstn
//----------------------------------------

template<class I, class P = less> I firstn(I lo, I hi, int n, P comp = P()) {
	--hi;

	I stop = lo + n;
	if (stop > hi)
		stop = hi;

	for (;;) {
		I mid = lo + ((hi - lo + 1) >> 1);

		if (comp(*mid, *lo))
			swap(*lo, *mid);

		if (comp(*hi, *lo))
			swap(*lo, *hi);

		if (comp(*hi, *mid))
			swap(*mid, *hi);

		I lo2 = lo, hi2 = hi;
		for (;;) {
			if (lo2 < mid)
				while (++lo2 < mid && !comp(*mid, *lo2))
					;
			if (lo2 >= mid)
				while (++lo2 <= hi && !comp(*mid, *lo2))
					;

			while (--hi2 > mid && comp(*mid, *hi2))
				;

			if (hi2 < lo2)
				break;

			swap(*lo2, *hi2);

			if (mid == hi2)
				mid = lo2;
		}

		++hi2;
		if (hi2 > mid)
			while (--hi2 > mid && !comp(*hi2, *mid))
				;
		if (hi2 <= mid)
			while (--hi2 > lo && !comp(*hi2, *mid))
				;

		if (hi2 < stop) {
			if (lo2 > stop)
				return stop;
			lo = lo2;
		} else {
			hi = hi2;
		}
	}
}

template<class P, class I>			auto	firstn(I lo, I hi)					{ return firstn(lo, hi, P()); }
template<class C, class P = less>	auto	firstn(C&& c, P comp = P())			{ return firstn(begin(c), end(c), comp); }
template<class P, class C>			auto	firstn(C&& c)						{ return firstn(c, P()); }

template<class I, class P = less>	auto	median(I lo, I hi, P comp = P()) 	{ return firstn(lo, hi, int((hi - lo) >> 1), comp); }
template<class P, class I>			auto	median(I lo, I hi)					{ return median(lo, hi, P()); }
template<class C, class P = less>	auto	median(C&& c, P comp = P())			{ return median(begin(c), end(c), comp); }
template<class P, class C>			auto	median(C&& c)						{ return median(c, P()); }

//----------------------------------------
// bitonic_sort
//----------------------------------------

template<class I, class P> void bitonic_merge(I lo, I hi, P comp, bool dir) {
	auto n = hi - lo;
	if (n > 1) {
		int m = highest_set(n - 1);
		for (I i = lo; i < lo + n - m; i++) {
			I j = i + m;
			if (comp(*i, *j) == dir)
				swap(*i, *j);
		}
		bitonic_merge(lo, lo + m, comp, dir);
		bitonic_merge(lo + m, hi, comp, dir);
	}
}

template<class I, class P = less> void bitonic_sort(I lo, I hi, P comp = P(), bool dir = false) {
	auto n = (hi - lo) / 2;
	if (n) {
		bitonic_sort(lo, lo + n, comp, !dir);
		bitonic_sort(lo + n, hi, comp, dir);
		bitonic_merge(lo, hi, comp, dir);
	}
}

template<class C, class P = less> 	void bitonic_sort(C&& c, P comp = P())		{ bitonic_sort(begin(c), end(c), comp); }
template<class P, class C> 			void bitonic_sort(C&& c)					{ bitonic_sort(c, P()); }

// constexpr bitonic_sort
#if 0
template<typename T, size_t N> struct bitonic_sorter {
	static const size_t M = N / 2;
	static const size_t P = T_highest_set<N - 1>::value;

	template<size_t...I> static constexpr meta::array<T, N> compare(bool dir, const meta::array<T,N> &a, index_list<I...>) {
		return {{
			(
				I < N-P	?	((a[I] > a[I + P]) == dir ? a[I] : a[I + P])
			:	I < P	?	a[I]
			:				((a[I - P] > a[I]) == dir ? a[I] : a[I - P])
			)...
		}};
	}
	static constexpr meta::array<T, N>	merge2(bool dir, const meta::array<T,N> &a) {
		return	bitonic_sorter<T, P>::merge(dir, slice<P>(a, 0))
			+	bitonic_sorter<T, N - P>::merge(dir, slice<N - P>(a, P));
	}
	static constexpr meta::array<T, N>	merge(bool dir, const meta::array<T,N> &a) {
		return merge2(dir, compare(dir, a, meta::make_index_list<N>()));
	}
	static constexpr meta::array<T, N>	sort(bool dir, const meta::array<T,N> &a) {
		return merge(dir,
			bitonic_sorter<T, M>::sort(!dir, slice<M>(a, 0))
		+	bitonic_sorter<T, N - M>::sort(dir, slice<N - M>(a, M))
		);
	}
};

template<typename T> struct bitonic_sorter<T, 1> {
	static constexpr meta::array<T, 1>	merge(bool reverse, const meta::array<T,1> &a)	{ return a; }
	static constexpr meta::array<T, 1>	sort(bool reverse, const meta::array<T,1> &a)	{ return a; }
};

template<typename T, size_t N> constexpr meta::array<T, N>	sort(const meta::array<T,N> &a) {
	return bitonic_sorter<T,N>::sort(false, a);
}
#endif

//-----------------------------------------------------------------------------
//	merges
//-----------------------------------------------------------------------------

// Given ranges [begin, mid) and [mid, end), does a standard merge to buffer
// The input ranges are assumed to be sorted using comp and will ultimately be sorted by comp
template<typename I, typename C> void merge(I begin, I mid, I end, I buffer, C comp) {
	for (I i1 = begin, i2 = mid, out = buffer; ; ++out) {
		if (comp(*i1, *i2)) {
			*out = *i1;
			if (++i1 == mid) {
				copy(i2, end, out);
				break;
			}
		} else {
			*out = *i2;
			if (++i2 == end) {
				copy(i1, mid, out);
				break;
			}
		}
	}
}

// inplace merge
template<typename I, typename C = less> void block_merge(I lo, I mid, I hi, C comp = C()) {
	while (lo < mid) {
		if (comp(*mid, *lo)) {
			auto	p = lower_bound(mid, hi, *lo, comp);
			rotate(lo, mid, p);
			lo 	+= p - mid;
			mid = p;
		}
		++lo;
	}
}

//-----------------------------------------------------------------------------
//	heap (and priority queue)
//-----------------------------------------------------------------------------

template<typename I, typename P = less> void heap_siftdown(I begin, I end, I root, P comp = P()) {
	for (;;) {
		I child = root + (root - begin + 1);
		if (child >= end)
			break;
		if (child + 1 < end && comp(child[1], child[0]))
			++child;
		if (!comp(*child, *root))
			break;
		swap(*root, *child);
		root = child;
	}
}
template<typename I, typename P = less> void heap_siftup(I begin, I end, I child, P comp = P()) {
	while (child > begin) {
		I parent = begin + (child - begin - 1) / 2;
		if (comp(*parent, *child))
			break;
		swap(*parent, *child);
		child = parent;
	}
}

template<typename I, typename P = less> void heap_make(I begin, I end, P comp = P()) {
	for (I i = begin + (end - begin - 2) / 2; i >= begin; --i)
		heap_siftdown(begin, end, i, comp);
}

template<typename I, typename P = less> void heap_push(I begin, I end, P comp = P()) {
	heap_siftup(begin, end, end - 1, comp);
}

template<typename I, typename P = less> void heap_pop(I begin, I end, P comp = P()) {
	swap(*--end, *begin);
	heap_siftdown(begin, end, begin, comp);
}

template<typename I, typename P = less> auto heap_pop_value(I begin, I end, P comp = P()) {
	auto	r		= exchange(*begin, *--end);
	heap_siftdown(begin, end, begin, comp);
	return r;
}

template<typename I, typename P = less> void heap_remove(I begin, I end, I item, P comp = P()) {
	swap(*--end, *item);
	if (comp(*item, *end))
		heap_siftup(begin, end, item, comp);
	else
		heap_siftdown(begin, end, item, comp);
}
template<typename I, typename P = less> void heap_update(I begin, I end, I item, P comp = P()) {
	I parent = begin + (item - begin - 1) / 2;
	if (comp(*item, *parent))
		heap_siftup(begin, end, item, comp);
	else
		heap_siftdown(begin, end, item, comp);
}
template<typename I, typename P = less> void heap_sort(I begin, I end, P comp = P()) {
	for (I i = end; --i > begin;) {
		swap(*i, *begin);
		heap_siftdown(begin, i, begin, comp);
	}
	reverse(begin, end);
}

// only efficient if item is near front
// requires a==b is equivalent to !comp(a,b) && !comp(b,a)
template<typename I, typename P, typename T> I heap_find(I begin, I end, const T& item, P comp) {
	for (I i = begin;;) {
		if (comp(*i, item)) {
			I next = i + (i - begin + 1);
			if (next < end) {
				i = next;
				continue;
			}

		} else if (!comp(item, *i)) {
			return i;
		}

		while (!((i - begin) & 1)) {
			i = begin + (i - begin - 1) / 2;
			if (i == begin)
				return end;
		}
		++i;
	}
}
// only efficient if item is near front
// requires a==b is equivalent to !comp(a,b) && !comp(b,a)
template<typename I, typename P, typename E, typename T> I heap_find(I begin, I end, const T& item, P comp, E equal) {
	for (I i = begin;;) {
		if (equal(item, *i))
			return i;

		if (comp(*i, item)) {
			I next = i + (i - begin + 1);
			if (next < end) {
				i = next;
				continue;
			}
		}

		while (!((i - begin) & 1)) {
			i = begin + (i - begin - 1) / 2;
			if (i == begin)
				return end;
		}
		++i;
	}
}

template<class C, typename P = less> void heap_make(C &&c, P comp = P()) { heap_make(begin(c), end(c), comp); }
template<class C, typename P = less> void heap_push(C &&c, P comp = P()) { heap_push(begin(c), end(c), comp); }
template<class C, typename P = less> void heap_pop(	C &&c, P comp = P()) { heap_pop(begin(c), end(c), comp); }
template<class C, typename P = less> void heap_sort(C &&c, P comp = P()) { heap_sort(begin(c), end(c), comp); }

template<class C, typename P = less> struct priority_queue : P {
	C	c;

	priority_queue() {}
	template<typename P2> priority_queue(P2&& p2) : P(forward<P2>(p2)) {}
	template<typename C2, typename P2> priority_queue(C2&& c2, P2&& p2) : P(forward<P2>(p2)), c(forward<C2>(c2)) {
		heap_make(c.begin(), c.end(), (P&)*this);
	}

	struct iterator {
		priority_queue*		p;
		iterator(priority_queue* p) : p(p) {}
		iterator& operator++() {
			p->pop();
			return *this;
		}
		bool	  operator!=(const iterator& b)	const { return !p->empty(); }
		decltype(auto) operator*()				const { return p->front(); }
	};

	bool			empty()				{ return c.empty(); }
	void			clear()				{ c.clear(); }
	void			make_heap()			{ heap_make(c.begin(), c.end(), (P&)*this); }
	const C&		container()	const	{ return *this; }
	C&				container()			{ return c; }
	iterator		begin()				{ return this; }
	iterator		end()				{ return nullptr; }

	decltype(auto)	top()		const	{ return c.front(); }
	decltype(auto)	top()				{ return c.front(); }

	void			pop() {
		heap_pop(c.begin(), c.end(), (P&)*this);
		c.pop_back();
	}
	auto			pop_value() {
		auto	r		= heap_pop_value(c.begin(), c.end(), (P&)*this);
		c.pop_back();
		return r;
	}
	template<typename T2> void push(const T2& t) {
		c.push_back(t);
		heap_push(c.begin(), c.end(), (P&)*this);
	}

	void			remove(iterator_t<C> i) {
		heap_remove(c.begin(), c.end(), i, (P&)*this);
		c.pop_back();
	}
	void 			update(iterator_t<C> i) {
		heap_update(c.begin(), c.end(), i, (P&)*this);
	}

	auto						find(const element_t<C>& t)			{ return heap_find(c.begin(), c.end(), t, (P&)*this); }
	template<typename E> auto	find(const element_t<C>& t, E e)	{ return heap_find(c.begin(), c.end(), t, (P&)*this, e); }
};

template<typename P, class C> priority_queue<C, P>	make_priority_queue(P&& p)				{ return priority_queue<C, P>(forward<P>(p)); }
template<typename P, class C> priority_queue<C, P>	make_priority_queue(C&& c, P&& p = P())	{ return priority_queue<C, P>(forward<C>(c), forward<P>(p)); }

//-----------------------------------------------------------------------------
//	partition
//-----------------------------------------------------------------------------

template<class I, class P = less> I partition(I lo, I hi, P test = P()) {
	I no = lo, yes = hi;
	while (no != yes) {
		if (test(*no)) {
			while (no != yes && test(*--yes))
				;
			if (no == yes)
				break;
			swap(*no, *yes);
		}
		++no;
	}
	return yes;
}

template<class P, class I> 			auto partition(I lo, I hi) 			{ return partition(lo, hi, P()); }
template<class C, class P = less> 	auto partition(C&& c, P comp = P()) { return partition(begin(c), end(c), comp); }
template<class P, class C> 			auto partition(C&& c) 				{ return partition(c, P()); }

//-----------------------------------------------------------------------------
//	nearest_neighbour
//-----------------------------------------------------------------------------


template<typename C, typename X> auto nearest_neighbour(C& c, const X& x, float& min_dist) {
	auto	besti = c.begin();
	float	bestd = num_traits<float>::max();

	for (auto i = besti, e = c.end(); i != e; ++i) {
		const float d = dist2(*i, x);
		if (d < bestd) {
			bestd = d;
			besti = i;
		}
	}

	min_dist = bestd;
	return besti;
}

}  // namespace iso

#endif  // ALGORITHM_H
