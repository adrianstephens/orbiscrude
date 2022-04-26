#ifndef ITERATOR_H
#define ITERATOR_H

#include "defs_base.h"

namespace iso {

template<typename C> constexpr auto			begin(C &&c)		->decltype(c.begin())	{ return c.begin(); }
template<typename C> constexpr auto			end(C &&c)			->decltype(c.end())		{ return c.end(); }

template<typename T>		constexpr T*	begin(T *p)			{ return p; }
template<typename T, int N>	constexpr T*	end(T (&p)[N])		{ return p + N; }

template<typename C> constexpr auto			global_begin(C &&c)	->decltype(begin(c))	{ return begin(c); }
template<typename C> constexpr auto			global_end(C &&c)	->decltype(end(c))		{ return end(c); }

//-----------------------------------------------------------------------------
//	traits
//-----------------------------------------------------------------------------

struct not_iterator_t			{};
struct input_iterator_t			{};
struct output_iterator_t		{};
struct forward_iterator_t		: public input_iterator_t			{};//has ++
struct bidirectional_iterator_t : public forward_iterator_t			{};//has ++,--
struct random_access_iterator_t : public bidirectional_iterator_t	{};//has ++,--,+,-

template<typename T, typename V = void> static constexpr bool has_begin_v = false;
template<typename T> static constexpr bool has_begin_v<T, void_t<decltype(begin(declval<T>()))>>	= true;
template<typename T, typename V = void> static constexpr bool has_end_v = false;
template<typename T> static constexpr bool has_end_v<T, void_t<decltype(end(declval<T>()))>>		= true;

template<typename T, typename V = void> struct T_has_begin	: T_false {};
template<typename T> struct T_has_begin<T, void_t<decltype(begin(declval<T>()))>>	: T_true {};

template<typename T, typename V = void> struct T_has_end	: T_false {};
template<typename T> struct T_has_end<T, void_t<decltype(end(declval<T>()))>>		: T_true {};

template<typename T, typename V = void> struct T_iterator_category_deduce	: T_type<not_iterator_t> {};
template<typename T> struct T_iterator_category_deduce<T, enable_if_t<T_has_deref<T>::value && T_has_inc<T>::value>>
	: T_if<T_has_dec<T>::value, if_t<can_addsub_v<T, int>, random_access_iterator_t, bidirectional_iterator_t>, forward_iterator_t> {};

template<typename T, typename V = void> struct T_iterator_category : T_iterator_category_deduce<T> {};
template<typename T>					struct T_iterator_category<T, void_t<typename T::iterator_category>>	: T_type<typename T::iterator_category> {};
template<typename T> using iterator_category = typename T_iterator_category<T>::type;

template<typename I, typename = void>	struct iterator_traits_reference1										{ typedef void reference; };
template<typename I>					struct iterator_traits_reference1<I, void_t<decltype(*declval<I>())>>	{ typedef decltype(*declval<I>()) reference; };
template<typename I, typename = void>	struct iterator_traits_reference : iterator_traits_reference1<const I>	{};
template<typename I>					struct iterator_traits_reference<I, void_t<typename I::reference>>		{ typedef typename I::reference reference; };

template<typename I, typename = void>	struct iterator_traits_element									{ typedef noref_t<typename iterator_traits_reference<I>::reference> element; };
template<typename I>					struct iterator_traits_element<I, void_t<typename I::element>>	{ typedef typename I::element element; };

template<typename I> struct iterator_traits : iterator_traits_element<I>, iterator_traits_reference<I> {
	typedef typename T_iterator_category<I>::type iterator_category;
};

template<typename T> struct iterator_traits<T*> {
	typedef random_access_iterator_t iterator_category;
	typedef T			element;
	typedef T&			reference;
};

template<typename T> struct iterator_traits<const T> {
	typedef typename iterator_traits<T>::iterator_category iterator_category;
	typedef const typename iterator_traits<T>::element		element;
	typedef	const typename iterator_traits<T>::reference	reference;
};
template<typename T> struct iterator_traits<T&> : iterator_traits<T> {};

template<typename C, typename V=void> struct container_traits_iterator								: T_type<decltype(begin(declval<C>()))> {};
template<typename C> struct container_traits_iterator<C, void_t<typename C::iterator>>				: T_type<typename C::iterator>			{};
template<typename C, typename V=void> struct container_traits_const_iterator						: T_type<decltype(begin(declval<const C>()))> {};
template<typename C> struct container_traits_const_iterator<C, void_t<typename C::const_iterator>>	: T_type<typename C::const_iterator>	{};

template<typename C> struct container_traits {
	typedef typename container_traits_iterator<C>::type			iterator;	
	typedef typename container_traits_const_iterator<C>::type	const_iterator;
	typedef typename iterator_traits<iterator>::element			element;	
	typedef typename iterator_traits<iterator>::reference		reference;
};
template<typename C> struct container_traits<const C> {
	typedef	typename container_traits<C>::const_iterator		iterator, const_iterator;
	typedef typename iterator_traits<iterator>::element			element;
	typedef typename iterator_traits<iterator>::reference		reference;
};
template<typename T> struct container_traits<T&> : container_traits<T> {};

template<typename T> struct container_traits<T*> {
	typedef T			element;
	typedef	T&			reference;
	typedef	T*			iterator;
	typedef	const T*	const_iterator;
};

template<typename T> struct container_traits<T[]>					: container_traits<T*> {};
template<typename T, int N> struct container_traits<T[N]>			: container_traits<T*> {};
template<typename T, int N> struct container_traits<T(&)[N]>		: container_traits<T*> {};
template<typename T, int N> struct container_traits<const T[N]>		: container_traits<const T*> {};
template<typename T> struct container_traits<constructable<T>>		: container_traits<T> {};

template<typename T> using iterator_t		= typename container_traits<T>::iterator;
template<typename T> using element_t		= typename container_traits<T>::element;
template<typename T> using reference_t		= typename container_traits<T>::reference;
template<typename T> using it_element_t		= typename iterator_traits<T>::element;
template<typename T> using it_reference_t	= typename iterator_traits<T>::reference;
template<typename I> constexpr bool is_iterator_v		= !same_v<typename T_iterator_category<I>::type, not_iterator_t>;

template<class C> constexpr bool is_empty(const C &c)	{ return begin(c) == end(c); }

template<typename I> size_t _distance(I first, const I &last, input_iterator_t) {
	size_t n = 0;
	while (first != last) {
		++first;
		++n;
	}
	return n;
}
template<typename I> constexpr size_t _distance(const I &first, const I &last, random_access_iterator_t) {
	return last - first;
}
template<typename I> constexpr size_t distance(const I &first, const I &last) {
	return _distance(first, last, typename iterator_traits<I>::iterator_category());
}
template<typename I> constexpr uint32 distance32(const I &first, const I &last) {
	return uint32(distance(first, last));
}

template<typename I, typename J> I nth(I first, J i, input_iterator_t) {
	while (i-- > 0)
		++first;
	return first;
}
template<typename I, typename J> constexpr I nth(const I &first, J i, random_access_iterator_t) {
	return I(first + i);
}
template<typename I, typename J> constexpr I nth(const I &first, J i) {
	return nth(first, i, typename iterator_traits<I>::iterator_category());
}

template<typename I> I last(const I &begin, const I &end, input_iterator_t) {
	I last = begin;
	for (auto i = begin; i != end; ++i)
		last = i;
	return last;
}
template<typename I> constexpr I last(const I &begin, I end, bidirectional_iterator_t) {
	return *--end;
}
template<typename I> constexpr I last(const I &begin, const I &end) {
	return last(begin, end, typename iterator_traits<I>::iterator_category());
}

template<typename C> constexpr inline size_t num_elements(const C &c) {
	return distance(begin(c), end(c));
}

template<typename C> inline size_t index_of(const C &c, const iterator_t<const C> &i) {
	return distance(begin(c), i);
}

//-----------------------------------------------------------------------------
//	array movement
//-----------------------------------------------------------------------------

// copy
template<class S, class D> inline void copy_n(S s, D d, size_t n) {
	for (; n--; ++s, ++d)
		assign(*d, *s);
}
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> copy_n(T *s, T *d, size_t n) {
	memmove(d, s, n * sizeof(T));
}
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> copy_n(const T *s, T *d, size_t n) {
	memmove(d, s, n * sizeof(T));
}
template<class T> inline enable_if_t<!is_trivially_copyable_v<T>> copy_n(T *s, T *d, size_t n) {
	if (s < d && s + n > d) {
		for (s += n, d += n; n--;)
			assign(*--d, *--s);
	} else {
		for (; n--; ++s, ++d)
			assign(*d, *s);
	}
}
template<class S, class D> inline void copy(S s, S e, D d) {
	for (; s != e; ++s, ++d)
		assign(*d, *s);
}
template<class S, class D> inline void rcopy(D d, D e, S s) {
	for (; d != e; ++s, ++d)
		assign(*d, *s);
}
template<class S, class D> enable_if_t<has_end_v<S> && has_end_v<D>>	check_copy_size(S &&s, D &&d)	{ ISO_ASSERT(num_elements(s) <= num_elements(d)); }
template<class S, class D> enable_if_t<!has_end_v<S> || !has_end_v<D>>	check_copy_size(S &&s, D &&d)	{}

template<class T> inline void copy(T *s, T *e, T *d)		{ copy_n(s, d, e - s); }
template<class T> inline void copy(const T *s, T *e, T *d)	{ copy_n(s, d, e - s); }
template<class S, class D> inline void copy(S &&s, D &&d)	{ check_copy_size(s, d); copy(begin(s), end(s), begin(d)); }
template<class T> inline void rcopy(T *d, T *e, T *s)		{ copy_n(s, d, e - d); }
template<class T> inline void rcopy(T *d, T *e, const T *s)	{ copy_n(s, d, e - d); }
template<class S, class D> inline void rcopy(D &&d, S &&s)	{ check_copy_size(d, s); copy_n(s, begin(d), num_elements(d)); }

// move
template<class S, class D> inline void move_n(S s, D d, size_t n) {
	for (; n--; ++s, ++d)
		move(*s, *d);
}
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> move_n(T *s, T *d, size_t n) {
	memcpy(d, s, n * sizeof(T));
}
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> move_n(const T *s, T *d, size_t n) {
	memcpy(d, s, n * sizeof(T));
}
template<class S, class D> inline void move(S s, S e, D d) {
	for (; s != e; ++s, ++d)
		move(*s, *d);
}
template<class T> inline void move(T *s, T *e, T *d)		{ move_n(s, d, e - s); }
template<class T> inline void move(const T *s, T *e, T *d)	{ move_n(s, d, e - s); }

// copy new
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> copy_new_n(T *s, T *d, size_t n) {
	memcpy(d, s, n * sizeof(T));
}
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> copy_new_n(const T *s, T *d, size_t n) {
	memcpy(d, s, n * sizeof(T));
}
template<class S, class D> inline void copy_new_n(S s, D d, size_t n) {
	for (; n--; ++s, ++d)
		copy_new(*s, *d);
}

// move new
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> move_new_n(T *s, T *d, size_t n) {
	memcpy(d, s, n * sizeof(T));
}
template<class T> inline enable_if_t<is_trivially_copyable_v<T>> move_new_n(const T *s, T *d, size_t n) {
	memcpy(d, s, n * sizeof(T));
}
template<class S, class D> inline void move_new_n(S s, D d, size_t n) {
	for (; n--; ++s, ++d)
		move_new(*s, *d);
}
template<class I> inline void move_new_check_n(I s, I d, size_t n) {
	intptr_t	o = d - s;
	if (n < abs(o)) {
		move_new_n	(s,	d, n);
	} else if (o > 0) {
		move_n		(d,	d + o, n - o);
		move_new_n	(s,	d, o);
	} else {
		move_new_n	(s,	d, -o);
		move_n		(d,	d + o, n + o);
	}
}

// fill
template<class I, typename...V> inline void fill_new_n(I i, size_t n, const V&...v) {
	for (; n--; ++i)
		construct(*i, v...);
}
template<class I> inline void fill_new_n(I i, size_t n) {
	for (; n--; ++i)
		construct(*i);
}
template<class I, typename V> inline void fill_n(I i, size_t n, const V &v) {
	for (; n--; ++i)
		assign(*i, v);
}
template<class I, class V> inline void fill(I i, I e, const V &v) {
	for (; i != e; ++i)
		assign(*i, v);
}

template<class C, class V> inline void fill(C&& c, const V &v) {
	fill(c.begin(), c.end(), v);
}

//-----------------------------------------------------------------------------
// helper for operator->
//-----------------------------------------------------------------------------

template<typename T> struct ref_helper {
	T	t;
	template<typename... P> constexpr ref_helper(P&&... p) : t(forward<P>(p)...)	{}
	constexpr T*		operator->()	{ return &t;	}
};

template<typename T> struct ref_helper<T&> {
	T	&t;
	constexpr ref_helper(T &t) : t(t)	{}
	constexpr T*		operator->()	{ return &t; }
};

template<typename T> ref_helper<T> make_ref_helper(T&& t) {
	return forward<T>(t);
}

template<typename R, typename T = noref_t<decltype(get(declval<R>()))>> struct ref_helper2 {
	R	r;
	T	t;
	constexpr ref_helper2(R&& r) : r(forward<R>(r)), t(r)	{}
	~ref_helper2()					{ r = t; }
	constexpr T*	operator->()	{ return &t;	}
	
	operator sized_placement()		{ return {&t, sizeof(T)}; }
};
template<typename R, typename T> struct ref_helper2<R, const T> {
	const T	t;
	constexpr ref_helper2(R&& r) : t(r) {}
	constexpr const T* operator->() { return &t; }
};

template<typename P> ref_helper2<P> make_ref_helper2(P&& p) {
	return forward<P>(p);
}

//-----------------------------------------------------------------------------
//	iterator_wrapper
//-----------------------------------------------------------------------------

template<typename T, typename I, typename C = typename iterator_traits<I>::iterator_category> struct iterator_wrapper {
protected:
	I	i;
	constexpr T*		me()			{ return static_cast<T*>(this); }
	constexpr const T*	me()	const	{ return static_cast<const T*>(this); }
public:
	using iterator_category	= C;
//	using element			= typename iterator_traits<I>::element;
//	using reference			= typename iterator_traits<I>::reference;

	constexpr iterator_wrapper()			: i()			{}
	constexpr iterator_wrapper(I &&i)		: i(forward<I>(i))	{}
	constexpr iterator_wrapper(const I &i)	: i(i)			{}
	constexpr const I&	inner()			const	{ return i; }

	constexpr decltype(auto) operator*()	const	{ return *i; }
	constexpr I				operator->()	const	{ return i; }
	constexpr T&			operator++()			{ ++i; return *me(); }
	constexpr T				operator++(int)			{ auto t = *me(); ++*me(); return t; }
	constexpr T&			operator--()			{ --i; return *me(); }
	constexpr T				operator--(int)			{ auto t = *me(); --*me(); return t; }

	constexpr bool			operator==(const iterator_wrapper &b)	const	{ return i == b.i; }
	constexpr bool			operator!=(const iterator_wrapper &b)	const	{ return i != b.i; }
	constexpr bool			operator< (const iterator_wrapper &b)	const	{ return i <  b.i; }
	constexpr bool			operator<=(const iterator_wrapper &b)	const	{ return i <= b.i; }
	constexpr bool			operator> (const iterator_wrapper &b)	const	{ return i >  b.i; }
	constexpr bool			operator>=(const iterator_wrapper &b)	const	{ return i >= b.i; }

	// random-access iterators only
	constexpr T&			operator+=(intptr_t x)			{ i += x; return *me(); }
	constexpr T				operator+ (intptr_t x)	const	{ return T(*me()) += x; }
	constexpr T&			operator-=(intptr_t x)			{ i -= x; return *me(); }
	constexpr T				operator- (intptr_t x)	const	{ return T(*me()) -= x; }
	constexpr decltype(auto) operator[](intptr_t x)	const	{ return *(*me() + x); }

//	friend auto	operator-(const iterator_wrapper &a, const iterator_wrapper &b) { return a.i - b.i; }
	auto	operator-(const iterator_wrapper &b) const	{ return i - b.i; }
};

//-----------------------------------------------------------------------------
//	with_iterator - gives range-based-for access to iterator
//-----------------------------------------------------------------------------

template<typename T> struct with_iterator_s {
	T	t;
	typedef noref_cv_t<decltype(t.begin())> I;

	struct iterator {
		I	i;
		iterator(I i) : i(i) {}
		iterator&	operator++()					{ ++i; return *this; }
		const I&	operator*()				const	{ return i; }
		I&			operator*()						{ return i; }
		bool operator==(const iterator &b)	const	{ return i == b.i; }
		bool operator!=(const iterator &b)	const	{ return i != b.i; }
	};
	typedef iterator	const_iterator;

	with_iterator_s(T &&t) : t(forward<T>(t)) {}
	iterator begin()	const { return t.begin(); }
	iterator end()		const { return t.end(); }
};

template<typename T> with_iterator_s<T> with_iterator(T&& t) { return forward<T>(t); }

template<typename T> struct with_index_s {
	T	t;
	typedef noref_cv_t<decltype(t.begin())> I;

	struct element {
		T	&t;
		I	i;
		element(T &t, I i) : t(t), i(i) {}
		auto&	operator*()		const	{ return *i; }
		auto&	operator->()	const	{ return *i; }
		size_t	index()			const	{ return t.index_of(i); }
	};

	struct iterator : element {
		iterator(T &t, I i) : element(t, i) {}
		iterator&	operator++()					{ ++element::i; return *this; }
		const element&	operator*()			const	{ return *this; }
		bool operator!=(const iterator &b)	const	{ return element::i != b.i; }
	};

	with_index_s(T &&t) : t(forward<T>(t)) {}
	iterator begin()	{ return iterator(t, t.begin()); }
	iterator end()		{ return iterator(t, t.end()); }
};

template<typename T> with_index_s<T>	with_index(T&& t)	{ return forward<T>(t); }

//-----------------------------------------------------------------------------
//	range
//-----------------------------------------------------------------------------

template<typename I> class range;

template<typename I> constexpr I							slice_a0(I a, I b, I a1)			{ return a1; }
template<typename I> constexpr I							slice_a0(I a, I b, uint32 a1)		{ return nth(a, a1); }
template<typename I, typename T> can_add_t<I, T, I>			slice_a0(I a, I b, T a1)			{ return (a1 < 0 ? b : a) + a1; }
template<typename I, typename A> range<I>					slice_a(I a, I b, A&& a1)			{ return {slice_a0(a, b, forward<A>(a1)), b}; }

template<typename I> constexpr range<I>						slice_b(I a, I b, I b1)				{ return range<I>(a, b1); }
template<typename I> constexpr range<I>						slice_b(I a, I b, uint32 b1)		{ return range<I>(a, nth(a, b1)); }
template<typename I, typename T> can_add_t<I, T, range<I>>	slice_b(I a, I b, T b1)				{ return range<I>(a, (b1 <= 0 ? b : a) + b1); }

template<typename I, typename A, typename B> range<I>		slice_ab(I a, I b, A&& a1, B&& b1)	{ return slice_b(slice_a0(a, b, forward<A>(a1)), b, forward<B>(b1)); }

template<typename C,typename A>				constexpr auto	slice(C &c, A&& a)					{ return slice_a(begin(c), end(c), forward<A>(a)); }
template<typename C,typename A, typename B>	constexpr auto	slice(C &c, A&& a, B&& b)			{ return slice_ab(begin(c), end(c), forward<A>(a), forward<B>(b)); }
template<typename C,typename B>				constexpr auto	slice_to(C &c, B&& b)				{ return slice_b(begin(c), end(c), forward<B>(b)); }

template<typename I> class range {
protected:
	I	a, b;
public:
	typedef	typename iterator_traits<I>::element	element;
	typedef	typename iterator_traits<I>::reference	reference;
	typedef	I	iterator, const_iterator;

	struct range_assign {
		const range&	r;
		range_assign(const range &r) : r(r) {}
		template<typename C>range_assign &operator=(const C &c) {
			auto p = r.a;
			for (auto &&i : c) {
				*p = i;
				if (++p == r.b)
					break;
			}
			return *this;
		}
	};

	constexpr range()								: a(), b()			{}
	constexpr range(_none&)							: a(), b()			{}
	constexpr range(const I &a, const I &b)			: a(a), b(b)		{}
//	constexpr range(const I &a, size_t n)			: a(a), b(a + n)	{}
	template<int N, typename V = enable_if_t<!same_v<element, void>, element>> constexpr range(noref_t<V> (&a)[N])	: a(a), b(a + N)	{}
	template<typename C, typename = enable_if_t<has_end_v<C>>> range(C &&c) : a(global_begin(c)), b(global_end(c)) {}
	constexpr range(I &&a, I &&b)					: a(move(a)), b(move(b))	{}

	constexpr range_assign		operator*()			const	{ return *this;	}
	constexpr decltype(auto)	operator[](int i)	const	{ return *nth(a, i);	}
	bool				empty()						const	{ return a == b; }
	constexpr const I&	begin()						const	{ return a; }
	constexpr const I&	end()						const	{ return b; }
	constexpr size_t	size()						const	{ return distance(a, b); }
	constexpr uint32	size32()					const	{ return distance32(a, b); }

	reference			back()						const	{ return b[-1]; }
	void				pop_back()							{ --b; }
	reference			pop_back_value()					{ return *--b; }
	reference			front()						const	{ return *a; }
	void				pop_front()							{ ++a; }
	reference			pop_front_value()					{ return *a++; }

	template<typename T> void	push_back(T &&t)			{ *b++ = forward<T>(t); }

	constexpr bool		contains(const range &r)	const	{ return !(r.a < a) && !(r.b > b); }
	constexpr bool		contains(const I i)			const	{ return i && !(i < a) && i < b; }
	constexpr bool		contains(reference e)		const	{ return !(&e < a) && &e < b; }
	constexpr intptr_t	index_of(const I i)			const	{ return contains(i) ? distance(a, i) : -1; }
	constexpr intptr_t	index_of(reference e)		const	{ return &e - a; }

	I		erase_unordered(I i) {
		ISO_ASSERT(contains(i));
		if (i != --b)
			*i = *b;
		return i;
	}
	I		erase(I i) {
		ISO_ASSERT(contains(i));
		--b;
		copy_n(i + 1, i, b - i);
		return i;
	}
	I		erase(I first, I last) {
		ISO_ASSERT(last >= first && first >= a && last <= b);
		b	-= last - first;
		copy_n(last, first, b - first);
		return first;
	}

	constexpr I			inc_wrap(I i)	const	{ ++i; return i == b ? a : i; }
	constexpr I			dec_wrap(I i)	const	{ if (i == a) i = b; return --i; }

	template<typename A>	auto			slice(A&& a1)			const	{ return slice_a(a, b, forward<A>(a1)); }
	template<typename B>	auto			slice_to(B&& b1)		const	{ return slice_b(a, b, forward<B>(b1)); }
	template<typename A, typename B> auto	slice(A&& a1, B&& b1)	const	{ return slice_ab(a, b, forward<A>(a1), forward<B>(b1)); }
	range&									trim_front(uint32 n)			{ a = nth(a, n); return *this; }
	range&									trim_length(uint32 n)			{ b = nth(a, n); return *this; }
	template<typename T> can_add_t<I, T, range&>	trim_back(T t)			{ b -= t; return *this; }

	friend constexpr range	operator&(const range &a, const range &b)	{ return range(max(a.a, b.a), min(a.b, b.b)); }
	friend constexpr bool	operator<(const range &a, const range &b)	{ return a.a < b.a || (!(b.a < a.a) && a.b < b.b); }
	template<typename W> friend bool write(W &w, const range &a)		{ return writen(w, a.a, a.size()); }
	template<typename R> friend bool read(R &r, const range &a)			{ return readn(r, a.a, a.size()); }
};

template<typename I> constexpr bool overlaps(const range<I> &x, const range<I> &y)	{ return !(y.a > x.b) && !(y.b < x.a); }

template<typename I>		constexpr auto	make_range(const I &a, const I &b)	{ return range<I>(a, b); }
template<typename I>		constexpr range<I>	make_range_n(const I &a, size_t n)	{ return range<I>(a, nth(a, n)); }
template<typename T, int N> constexpr auto	make_range(T (&a)[N])				{ return range<T*>(&a[0], &a[N]);	}
template<typename T>		constexpr auto	make_range_n(T (&a)[], size_t n)	{ return range<T*>(&a[0], &a[n]);	}
template<typename T, int N>	constexpr auto	make_range_n(T (&a)[N], size_t n)	{ return range<T*>(&a[0], &a[n]);	}
template<typename I>		constexpr auto	make_range(I &&a, I &&b)			{ return range<noref_t<I>>(forward<I>(a), forward<I>(b)); }
template<typename I>		constexpr auto	make_range_n(I &&a, size_t n)		{ return make_range(forward<I>(a), nth(a, n)); }
template<typename C>		constexpr auto	make_rangec(C &c)					{ return make_range(begin(c), end(c)); }

template<typename T, typename C> auto element_cast(C &&c)	{ return make_range(element_cast<T>(begin(c)), element_cast<T>(end(c))); }

//-----------------------------------------------------------------------------
//	cached_range - cached range for quicker indexing
//-----------------------------------------------------------------------------

template<typename I> struct cached_range;

template<typename I> struct cached_iterator {
	const cached_range<I>	*r;
	typedef	typename iterator_traits<I>::reference	reference;
	size_t		i;

	cached_iterator(const cached_range<I> *r, size_t i) : r(r), i(i) {}
	bool		operator==(const cached_iterator &b)	const { return i == b.i; }
	bool		operator!=(const cached_iterator &b)	const { return i != b.i; }
	intptr_t	operator-(const cached_iterator &b)		const { return i - b.i; }

	reference			operator*()		const	{ return *r->get_nth(i); }
	auto				operator->()	const	{ return r->get_nth(i); }
	I					iterator()		const	{ return r->get_nth(i); }

	cached_iterator	operator+(int j)	const	{ return {r, i + j}; }
	cached_iterator	operator-(int j)	const	{ return {r, i - j}; }
	cached_iterator&	operator++()			{ ++i; return *this; }
	cached_iterator&	operator--()			{ --i; return *this; }
	friend I	get(const cached_iterator &i)	{ return i.iterator(); }
};

template<typename I> struct cached_range : range<I> {
	using typename range<I>::reference;

	struct saved {
		int		pos;
		I		i;
		saved(int pos, I i) : pos(pos), i(i) {}
	};
	mutable saved	prev;
	mutable saved	pow2;
	size_t			n;

	template<typename C> cached_range(C &&c) : range<I>(c.begin(), c.end()), prev(0, this->a), pow2(0, this->a), n(num_elements(c)) {}

	I	get_nth(size_t i) const {
		saved	from = i < prev.pos
			? (i < pow2.pos ? saved(0, this->a) : pow2)
			: (pow2.pos > prev.pos && i >= pow2.pos ? pow2 : prev);

		if (prev.pos != i && prev.pos && (prev.pos & (prev.pos - 1)) == 0)
			pow2 = prev;

		prev	= saved(i, nth(from.i, i - from.pos));
		return prev.i;
	}

	reference	operator[](int i)	const	{ return *get_nth(i); }
	cached_iterator<I>	begin()		const	{ return {this, 0}; }
	cached_iterator<I>	end()		const	{ return {this, n}; }
	friend constexpr size_t num_elements(const cached_range &c) { return c.n; }
};

template<typename C> cached_range<iterator_t<C>> make_cached_range(C &&c) { return c; }

//-----------------------------------------------------------------------------
//	split_range - split into chunks of N
//-----------------------------------------------------------------------------

template<typename I, int N> struct split_range : range<I> {
	typedef range<I>		element;
	typedef const element&	reference;
	using	range<I>::a;
	using	range<I>::b;

	struct iterator : range<I> {
		typedef random_access_iterator_t iterator_category;
		typedef range<I>		element;
		typedef const element&	reference;
		using		range<I>::a;
		using		range<I>::b;
		I			end;
		iterator(const I &a, const I &end) : range<I>(a, nth(a, N)), end(end) { if (b > end) b = end; }
		iterator(const I &end) : range<I>(end, end), end(end)		{}
		iterator&			operator++()							{ a = b; b = nth(b, N); if (b > end) b = end; return *this; }
		constexpr bool		operator!=(const iterator &b)	const	{ return a != b.a; }
		constexpr int		operator-(const iterator &b)	const	{ return (a - b.a + N - 1) / N; }
		iterator			operator+(int i)				const	{ return iterator(a + i * N, end); }
		constexpr reference	operator*()						const	{ return *this; }
	};
	typedef iterator const_iterator;

	iterator	begin() const { return iterator(a, b); }
	iterator	end()	const { return iterator(b); }
	size_t		size()	const { return (range<I>::size() + N - 1) / N; }
	element		operator[](int i)	const	{ return *(begin() + i);	}

	split_range(const range<I> &r) : range<I>(r) {}
};

template<int N, typename I> split_range<I, N> make_split_range(const range<I> &r) { return r; }
template<int N, typename C> auto make_split_range(const C &c) { return make_split_range<N>(make_rangec(c)); }

//-----------------------------------------------------------------------------
//	class circular_buffer
//-----------------------------------------------------------------------------

template<typename C, typename I> inline auto next_wrap(const C &c, const I &i) { return i + 1 == end(c) ? begin(c) : i + 1; }
template<typename C, typename I> inline auto prev_wrap(const C &c, const I &i) { return (i == begin(c) ? end(c) : i) - 1; }

template<typename I> struct wrap_iterator : range<I> {
	I	i;
	wrap_iterator(I i, I a, I b) : range<I>(a, b), i(i) {}
	wrap_iterator&	operator++()			{ if (++i == this->b) i = this->a; return *this; }
	wrap_iterator&	operator--()			{ if (i == this->a) i = this->b; --i; return *this; }
	decltype(auto)	operator*()		const	{ return *i; }
	I				operator->()	const	{ return i; }
	operator placement()			const	{ return i; }
	operator I()					const	{ return i; }
};

template<typename I> class circular_buffer : range<I> {
	I		g;
	int		num;
	using	range<I>::a;
	using	range<I>::b;
	using typename range<I>::reference;

	constexpr I		wrap(I g)		{ return (g - a) % (b - a) + a; }

public:
	typedef wrap_iterator<I>			iterator;
	typedef wrap_iterator<const I>		const_iterator;

	void set_get(I i) {
		g = i;
	}

	I	put(int n) {
		I	p = wrap(g + num, a, b);
		if (p + n > b) {
			if (a + n > g)
				return 0;
			num = b - g;
			p	= a;
		}
		num += n;
		return p;
	}

	circular_buffer(I a, I b)					: range<I>(a, b), g(a), num(0)		{}
	circular_buffer(I a, int n)					: range<I>(a, a + n), g(a), num(0)	{}
	template<typename C> circular_buffer(C &c)	: range<I>(c), g(a), num(0)			{}

	void			init(I _a, I _b)		{ g = a = _a; b = _b; num = 0; }
	void			init(I _a, int n)		{ Init(_a, _a + n); }
	void			reset()					{ num = 0; }

	I				get_buffer()	const	{ return a; }
	size_t			capacity()		const	{ return b - a;	}
	int				size()			const	{ return num; }
	size_t			space()			const	{ return capacity() - size(); }
	int				linear_space()	const	{ return (b - (g + num > b ? a : g)) - num; }
	bool			empty()			const	{ return num == 0; }
	bool			full()			const	{ return size() == capacity(); }

	auto&			operator[](int i)const	{ ISO_ASSERT(i < num); return *wrap(g + i); }
	auto&			front()			const	{ ISO_ASSERT(num); return *g; }
	auto&			back()			const	{ ISO_ASSERT(num); return *wrap(g + num - 1); }
	auto&			operator[](int i)		{ ISO_ASSERT(i < num); return *wrap(g + i); }
	auto&			front()					{ ISO_ASSERT(num); return *g; }
	auto&			back()					{ ISO_ASSERT(num); return *wrap(g + num - 1); }

	const_iterator	begin()			const	{ return const_iterator(g, a, b); }
	const_iterator	end()			const	{ return const_iterator(wrap(g + num), a, b); }
	iterator		begin()					{ return iterator(g, a, b); }
	iterator		end()					{ return iterator(wrap(g + num), a, b); }

	template<typename T> bool push_back(T &&t) {
		if (full())
			return false;
		*wrap(g + num++) = forward<T>(t);
		return true;
	}
	reference	push_back() {
		ISO_ASSERT(!full());
		return *wrap(g + num++);
	}
	void	pop_front() {
		ISO_ASSERT(num);
		--num;
		if (++g == b)
			g = a;
	}
};

//-----------------------------------------------------------------------------
//	move_iterator
//-----------------------------------------------------------------------------

template<class I> struct move_iterator : iterator_wrapper<move_iterator<I>, I> {
	using reference	= noref_t<typename iterator_traits<I>::reference>&&;
	using iterator_wrapper<move_iterator<I>, I>::iterator_wrapper;
	constexpr reference operator*()				const	{ return move(*this->i); }
	constexpr reference	operator[](size_t x)	const	{ return move(this->i[x]); }
};

template<typename T> move_iterator<T>	make_move_iterator(T i)		{ return i; }
template<typename T> auto				move_container(T&& c)		{ return make_range(make_move_iterator(begin(c)), make_move_iterator(end(c))); }

//-----------------------------------------------------------------------------
//	reverse_iterator
//-----------------------------------------------------------------------------

template<typename I> struct reverse_iterator {
	typedef typename iterator_traits<I>::iterator_category	iterator_category;
	I		i;
	reverse_iterator()				{}
	reverse_iterator(I i)	: i(i)	{}
	decltype(auto)		operator*()	 				const	{ return *i;			}
	decltype(auto)		operator->() 				const	{ return i;				}
	reverse_iterator&	operator++()						{ --i; return *this;	}
	reverse_iterator	operator++(int)						{ return i--;			}
	reverse_iterator&	operator--()						{ ++i; return *this;	}
	reverse_iterator	operator--(int)						{ return i++;			}
	reverse_iterator&	operator+=(intptr_t j)				{ i -= j; return *this; }
	reverse_iterator&	operator-=(intptr_t j)				{ i += j; return *this; }

	decltype(auto)		operator[](intptr_t j)				{ return *(i - j); }
	decltype(auto)		operator[](intptr_t j)		const	{ return *(i - j); }

	bool	operator==(const reverse_iterator &b)	const	{ return i == b.i;	}
	bool	operator!=(const reverse_iterator &b)	const	{ return i != b.i;	}
	bool	operator< (const reverse_iterator &b)	const	{ return i >  b.i; }
	bool	operator<=(const reverse_iterator &b)	const	{ return i >= b.i; }
	bool	operator> (const reverse_iterator &b)	const	{ return i <  b.i; }
	bool	operator>=(const reverse_iterator &b)	const	{ return i <= b.i; }
	
	operator placement()			const	{ return i; }

	friend intptr_t	operator-(const reverse_iterator &a, const reverse_iterator &b)		{ return b.i - a.i; }
	friend reverse_iterator	operator+(const reverse_iterator &a, intptr_t j)			{ return a.i - j;	}
	friend reverse_iterator	operator-(const reverse_iterator &a, intptr_t j)			{ return a.i + j;	}
	friend reverse_iterator	min(const reverse_iterator &a, const reverse_iterator &b)	{ return max(a.i, b.i);	}
	friend reverse_iterator	max(const reverse_iterator &a, const reverse_iterator &b)	{ return min(a.i, b.i);	}
};

template<class I> reverse_iterator<I> 	make_reverse_iterator(I i) 						{ return --i; }
template<class I> I 					make_reverse_iterator(reverse_iterator<I> i) 	{ return ++i.i; }

template<class C>	force_inline auto	reversed(C &&c)	{ return make_range(make_reverse_iterator(c.end()), make_reverse_iterator(c.begin()));	}

template<class T> inline enable_if_t<is_trivially_copyable_v<T>> copy(reverse_iterator<T*> s, reverse_iterator<T*> e, reverse_iterator<T*> d) {
	size_t	size = intptr_t(&*s) - intptr_t(&*e);
	memmove((char*)&d[-1] - size, (char*)&s[-1] - size, size);
}

//-----------------------------------------------------------------------------
//	dynamic - iterate over potentially changing container (using index)
//-----------------------------------------------------------------------------

template<typename C> struct dynamic {
	class iterator {
		C			&c;
		int			i;
		constexpr size_t	get_index()	const { return i < 0 ? num_elements(c) : i; }
	public:
		constexpr iterator(C &c, int i) : c(c), i(i) {}

		iterator&			operator++()				{ ++i; return *this; }
		iterator&			operator--()				{ --i; return *this; }
		iterator&			operator++(int)				{ iterator t = *this; ++i; return t; }
		iterator&			operator--(int)				{ iterator t = *this; --i; return t; }
		iterator&			operator+=(int n)			{ i += n; return *this; }
		iterator&			operator-=(int n)			{ i -= n; return *this; }

		constexpr decltype(auto)	operator*()				const	{ return c[i]; }
		constexpr decltype(auto)	operator->()			const	{ return c[i]; }
		constexpr decltype(auto)	operator[](int n)		const	{ return c[i + n]; }
		constexpr iterator			operator+(int n)		const	{ return iterator(c, i + n); }
		constexpr iterator			operator-(int n)		const	{ return iterator(c, i - n); }

		constexpr int		operator-(const iterator &b)	const	{ return get_index() = b.get_index(); }

		constexpr bool		operator==(const iterator &b)	const	{ return get_index() == b.get_index(); }
		constexpr bool		operator!=(const iterator &b)	const	{ return get_index() != b.get_index(); }
		constexpr bool		operator< (const iterator &b)	const	{ return get_index() <  b.get_index(); }
		constexpr bool		operator<=(const iterator &b)	const	{ return get_index() <= b.get_index(); }
		constexpr bool		operator> (const iterator &b)	const	{ return get_index() >  b.get_index(); }
		constexpr bool		operator>=(const iterator &b)	const	{ return get_index() >= b.get_index(); }
	};

	C	&c;

	dynamic(C &c) : c(c) {}
	iterator	begin()	const	{ return iterator(c, 0); }
	iterator	end()	const	{ return iterator(c, -1); }
};

template<class C>	force_inline dynamic<C>	make_dynamic(C &c)	{ return c;	}

//-----------------------------------------------------------------------------
//	int_iterator - just an int (or whatever)
//-----------------------------------------------------------------------------

template<typename I> class int_iterator {
protected:
	I		i;
public:
	constexpr int_iterator(I i)	: i(i)	{}
	int_iterator&			operator++()						{ i = I(i + 1); return *this; }
	int_iterator			operator++(int)						{ I j = i; i = I(i + 1); return j; }
	int_iterator&			operator--()						{ i = I(i - 1); return *this; }
	int_iterator			operator--(int)						{ I j = i; i = I(i - 1); return j; }
	int_iterator&			operator+=(intptr_t j)				{ i = I(i + j); return *this; }
	int_iterator&			operator-=(intptr_t j)				{ i = I(i - j); return *this; }
	constexpr I				operator*()					const	{ return i; 	}
	constexpr int_iterator	operator+(intptr_t j)		const	{ return I(i + j);	}
	constexpr int_iterator	operator-(intptr_t j)		const	{ return I(i - j);	}
	constexpr I				operator[](intptr_t j)		const	{ return I(i + j);	}

	constexpr auto	operator-(const int_iterator &b)	const	{ return i - b.i;	}
	constexpr bool	operator==(const int_iterator &b)	const	{ return i == b.i;	}
	constexpr bool	operator!=(const int_iterator &b)	const	{ return i != b.i;	}
	constexpr bool	operator< (const int_iterator &b)	const	{ return i <  b.i; }
	constexpr bool	operator<=(const int_iterator &b)	const	{ return i <= b.i; }
	constexpr bool	operator> (const int_iterator &b)	const	{ return i >  b.i; }
	constexpr bool	operator>=(const int_iterator &b)	const	{ return i >= b.i; }
};

template<typename I> force_inline int_iterator<I> make_int_iterator(I i) {
	return int_iterator<I>(i);
}
template<typename T> force_inline range<int_iterator<T> > int_range(T a, T b) {
	return range<int_iterator<T> >(a, b);
}
template<typename T> force_inline range<int_iterator<T> > int_range_inc(T a, T b) {
	return range<int_iterator<T> >(a, T((int)b + 1));
}
template<typename T> force_inline range<int_iterator<T> > int_range(T b) {
	return range<int_iterator<T> >(T(0), b);
}

//-----------------------------------------------------------------------------
//	indexed_iterator - uses index
//-----------------------------------------------------------------------------

template<typename C, typename X> class indexed_element {
	C		c;
	X		x;
public:
//	typedef typename iterator_traits<C>::reference	reference;
//	typedef typename iterator_traits<C>::element	element;
	typedef decltype(declval<C>()[0])	element;

	indexed_element(C &&c, X x) : c(forward<C>(c)), x(x)	{}
	decltype(auto)	get()							const	{ return c[x];	}
	operator		element()						const	{ return get();	}
	decltype(auto)	operator*()						const	{ return get();	}
	auto			operator->()					const	{ return &c[x];	}
	X				index()							const	{ return x; }

	template<typename B> auto& operator=(B &&b)	{ c[x] = forward<B>(b); return *this; }
	auto&	operator=(const indexed_element &b)	{ x = b.x; return *this; }

	//auto		operator-()							const	{ return -c[x]; }
	//auto		operator+()							const	{ return c[x]; }
	bool		operator==(const element &b)		const	{ return c[x] == b; }
	bool		operator!=(const element &b)		const	{ return c[x] != b; }
	bool		operator< (const element &b)		const	{ return c[x] <  b; }
	bool		operator<=(const element &b)		const	{ return c[x] <= b; }
	bool		operator>=(const element &b)		const	{ return c[x] >= b; }
	bool		operator> (const element &b)		const	{ return c[x] >  b; }

	friend void swap(indexed_element a, indexed_element b)	{ swap(a.x, b.x); }
	friend decltype(auto)	get(const indexed_element &a)	{ return a.get(); }
	friend decltype(auto)	put(indexed_element &a)			{ return a.get(); }
	friend decltype(auto)	get(indexed_element &&a)		{ return a.get(); }
};

template<typename C, typename X> struct indexed_helper {
	typedef	decltype(declval<C>()[0])				reference;
	static decltype(auto) deindex(C &c, X x)				{ return c[x]; }
};
template<typename C, typename X> struct indexed_helper<C, X&> {
	typedef	indexed_element<C, X&>					reference;
	static decltype(auto) deindex(C &c, X &x)			{ return reference(c, x); }
};
template<typename C, typename X> struct indexed_helper<C, const X&> {
	typedef	decltype(declval<C>()[0])				reference;
	static decltype(auto) deindex(C &c, const X &x)		{ return c[x]; }
};

template<typename C, typename I> class indexed_iterator : public iterator_wrapper<indexed_iterator<C, I>, I> {
	template<typename C2, typename I2> friend class indexed_iterator;
	typedef iterator_wrapper<indexed_iterator<C, I>, I>	B;
	typedef typename iterator_traits<I>::reference	index_t;
	typedef indexed_helper<C, index_t>				helper;
	typedef indexed_helper<const C, index_t>		const_helper;
//	typedef typename const_helper::reference		const_reference;
	using	B::i;
	C		c;
public:
	typedef typename iterator_traits<C>::element	element;
	typedef typename helper::reference				reference;

	indexed_iterator(C &&c, const I &i) : B(i), c(forward<C>(c))	{}
	template<typename C2, typename I2> indexed_iterator(const indexed_iterator<C2, I2> &b) : B(b.i), c(b.c) {}
	indexed_iterator&	operator=(const indexed_iterator &b)	{ B::operator=(b); return *this; }
	auto				operator->()							{ return &c[*i];	}
	decltype(auto)		operator*()								{ return helper::deindex(c, *i); }
	decltype(auto)		operator[](int j)						{ return helper::deindex(c, i[j]); }
	auto				operator->()				const		{ return &c[*i];	}
	decltype(auto)		operator*()					const		{ return const_helper::deindex(c, *i); }
	decltype(auto)		operator[](int j)			const		{ return const_helper::deindex(c, i[j]); }
	auto				index()						const		{ return *i; }

	operator placement()	{ return &c[*i]; }
};

template<typename C, typename I> force_inline indexed_iterator<C,I> make_indexed_iterator(C &&c, const I &i) {
	return indexed_iterator<C,I>(forward<C>(c), i);
}

template<typename C, typename CI> class indexed_container {
	template<typename C2, typename CI2> friend class indexed_container;
	typedef typename container_traits<CI>::iterator			I;
	typedef typename container_traits<CI>::const_iterator	constI;
	C		c;
	CI		i;
public:
	typedef	indexed_iterator<C&,I>				iterator;
	typedef	indexed_iterator<const C&,constI>	const_iterator;

	indexed_container(C &&c, CI &&i) : c(forward<C>(c)), i(forward<CI>(i))	{}
	template<typename C2, typename CI2> indexed_container(const indexed_container<C2, CI2> &b) : c(b.c), i(b.i)	{}
	template<typename C2, typename CI2> indexed_container& operator=(const indexed_container<C2, CI2> &b) { i = b.i; return *this; }

	decltype(auto)	operator[](int j)			{ return c[i[j]]; }
	decltype(auto)	operator[](int j)	const	{ return c[i[j]]; }
	auto			begin()						{ using iso::begin;	return iterator(c, begin(i)); }
	auto			end()						{ using iso::end;	return iterator(c, end(i)); }
	auto			begin()				const	{ using iso::begin;	return const_iterator(c, begin(i)); }
	auto			end()				const	{ using iso::end;	return const_iterator(c, end(i)); }
	auto			size()				const	{ return i.size(); }
	auto			size32()			const	{ return i.size32(); }
	decltype(auto)	back()				const	{ using iso::end;	return c[end(i)[-1]]; }
	decltype(auto)	front()				const	{ using iso::begin;	return c[*begin(i)]; }
	decltype(auto)	back()						{ using iso::end;	return c[end(i)[-1]]; }
	decltype(auto)	front()						{ using iso::begin;	return c[*begin(i)]; }
	bool			empty()				const	{ return i.empty(); }
	const CI&		indices()			const	{ return i; }
	const C&		direct()			const	{ return c; }

	iterator		erase(iterator first, iterator last) { return iterator(c, i.erase(first.inner(), last.inner())); }
};

template<typename C, typename CI> force_inline auto make_indexed_container(C &&c, CI &&i) {
	return indexed_container<C,CI>(forward<C>(c), forward<CI>(i));
}

template<typename C> force_inline auto make_indexed_container(C &&c) {
	return make_indexed_container(forward<C>(c), int_range(num_elements(c)));
}

template<typename C> force_inline auto make_indexed_container_n(C &&c, size_t n) {
	return make_indexed_container(forward<C>(c), int_range(n));
}

//-----------------------------------------------------------------------------
//	next_iterator - iterate over something with a next() function
//-----------------------------------------------------------------------------

template<typename T> force_inline T *next(T *p) { return p->next(); }

template<typename T> struct next_iterator : comparisons<next_iterator<T> > {
	typedef random_access_iterator_t iterator_category;
	typedef T	element, &reference;
	T	mutable	*p;
	int			i;
	mutable int	pi;

	T *getp() const {
		while (pi < i) {
			p = next(p);
			++pi;
		}
		return p;
	}

	constexpr next_iterator(T *p = 0) : p(p), i(0), pi(0)	{}
	constexpr next_iterator(T *p, int i) : p(p), i(i), pi(i) {}
	constexpr next_iterator(T *p, int i, int pi) : p(p), i(i), pi(pi) {}
	next_iterator&		operator++()					{ ++i; return *this; }
	next_iterator&		operator+=(size_t n)			{ getp(); i += int(n); return *this; }
	next_iterator		operator+(size_t n)		const	{ T *p = getp(); return next_iterator(p, int(i + n), pi); }
	constexpr T&		operator*()				const	{ return *getp(); }
	constexpr T*		operator->()			const	{ return getp(); }
	constexpr T&		operator[](uint32 i)	const	{ return *(*this + i); }

	T*					at_offset(int d)		const	{ return (T*)((uint8*)p + d); }

	friend constexpr int operator-(const next_iterator &a, const next_iterator &b) {
		if (b.i < 0)
			return a.i < 0 ? 0 : -(b - a);

		if (a.i < 0) {
			int	r	= 0;
			for (T *bp = b.getp(); bp < a.p; bp = next(bp))
				++r;
			return r;
		}
		return a.i - b.i;
	}
	friend constexpr int compare(const next_iterator &a, const next_iterator &b) {
		if (a.i < 0)
			return b.i < 0 ? 0 : -compare(b, a);

		if (b.i < 0) {
			T *bp = b.p;
			if (a.p) {
				while (a.pi < a.i && a.p <= bp) {
					a.p = next(a.p);
					++a.pi;
				}
			}
			return a.p < bp ? -1 : a.p > bp ? 1 : 0;
		}
		return a.i - b.i;
	}
};

template<typename T> force_inline auto make_next_iterator(T *p, int i = 0) {
	return next_iterator<T>(p, i);
}
template<typename T> force_inline auto make_next_range(T *begin, T *end) {
	return range<next_iterator<T>>(begin, next_iterator<T>(end, -1));
}
template<typename T> force_inline auto make_range_n(const next_iterator<T> &a, size_t n) {
	return range<next_iterator<T> >(a, next_iterator<T>(a.p, n, 0));
}

//-----------------------------------------------------------------------------
//	field_iterator - adapts iterator over structs to one over a field of the struct
//-----------------------------------------------------------------------------

template<typename I, typename F, F f> struct field_iterator;

template<typename I, typename C, typename T, T C::*f> struct field_iterator<I, T C::*, f> : iterator_wrapper<field_iterator<I, T C::*, f>, I> {
	typedef iterator_wrapper<field_iterator<I, T C::*, f>, I>	B;

	field_iterator(I &&i)		: B(move(i))	{}
	field_iterator(const I &i)	: B(i)			{}
	decltype(auto)	operator*()			const	{ return get(*B::i).*f;	}
	auto			operator->()		const	{ return &get(*B::i).*f; }
	decltype(auto)	operator[](int x)	const	{ return B::i[x].*f; }
};

template<typename F, F f, typename I> field_iterator<noref_cv_t<I>, F, f> _make_field_iterator(I &&i) { return forward<I>(i); }

#define make_field_iterator(I,F)	_make_field_iterator<decltype(&noref_t<decltype(get(*(I)))>::F), &noref_t<decltype(get(*(I)))>::F>(I)
#define make_field_container(C,F)	make_range(make_field_iterator(iso::global_begin(C), F), make_field_iterator(iso::global_end(C), F))

template<typename F> struct field_access {
	uint32	offset;
	constexpr field_access(uint32 offset) : offset(offset) {}
	constexpr F&		operator[](void* p)			const	{ return *(F*)((char*)p + offset); }
	constexpr const F&	operator[](const void* p)	const	{ return *(const F*)((const char*)p + offset); }
};

//-----------------------------------------------------------------------------
//	stride_iterator
//-----------------------------------------------------------------------------

template<typename T, int S> class fixed_stride_iterator : public iterator_wrapper<fixed_stride_iterator<T, S>, T*> {
	typedef iterator_wrapper<fixed_stride_iterator<T, S>, T*>	B;
	using	B::i;
public:
	fixed_stride_iterator(T *i) : B(i)		{}
	fixed_stride_iterator&	operator++()			{ i = (T*)((char*)i + S); return *this; }
	fixed_stride_iterator&	operator--()			{ i = (T*)((char*)i - S); return *this; }
	fixed_stride_iterator	operator++(int)			{ auto t = *this; operator++(); return t; }
	fixed_stride_iterator	operator--(int)			{ auto t = *this; operator--(); return t; }
	fixed_stride_iterator&	operator+=(intptr_t x)	{ i = (T*)((char*)i + x * S); return *this;	}
	fixed_stride_iterator&	operator-=(intptr_t x)	{ i = (T*)((char*)i - x * S); return *this;	}
	constexpr intptr_t	operator-(const fixed_stride_iterator &j)	const	{ return B::operator-(j) / S; }
	constexpr auto		operator-(intptr_t x)						const	{ return fixed_stride_iterator(*this) -= x; }
};

template<int S, typename T> fixed_stride_iterator<T,S> column(T(*t)[S]) { return t[0]; }
template<int S, typename T> fixed_stride_iterator<T,S> column(T *t)		{ return t; }

template<typename T> class stride_iterator;

template<> class stride_iterator<void> {
public:
	void	*p;
	int		s;
	constexpr stride_iterator(void *p = 0, int s = 0) : p(p), s(s) {}
	constexpr int		stride()		const	{ return s; }
	constexpr explicit operator void*()	const	{ return p; }
	constexpr explicit operator bool()	const	{ return !!p; }
	constexpr void*		operator*()		const	{ return p; }
	
	auto&	operator++()						{ p = ((char*)p + s); return *this;		}
	auto&	operator--()						{ p = ((char*)p - s); return *this;		}
	auto&	operator+=(intptr_t x)				{ p = ((char*)p + x * s); return *this;	}
	auto&	operator-=(intptr_t x)				{ p = ((char*)p - x * s); return *this;	}
	auto	operator+(intptr_t x)		const	{ return stride_iterator(*this) += x;	}
	auto	operator-(intptr_t x)		const	{ return stride_iterator(*this) -= x;	}

	bool				operator!=(const stride_iterator& b)	const	{ return p != b.p; }
	constexpr intptr_t	operator-(const stride_iterator &j)		const	{ return s ? intptr_t(((char*)p - (char*)j.p) / s) : 0; }
	template<typename F> constexpr auto		operator+(const field_access<F> &f)		const	{ return stride_iterator<F>(&f[p], s); }
	template<typename F> constexpr F&		operator->*(const field_access<F>& f)	const	{ return f[p]; }
	friend constexpr intptr_t operator-(const void* a, const stride_iterator &b)			{ return intptr_t(((char*)a - (char*)b.p) / b.s); }
};

template<typename T> class stride_iterator : public iterator_wrapper<stride_iterator<T>, T*> {
	typedef iterator_wrapper<stride_iterator<T>, T*>	B;
	template<typename T2> friend class stride_iterator;
	using	B::i;
	int		s;
public:
	constexpr stride_iterator()									: B(0), s(0)				{}
	constexpr stride_iterator(T *p, int s)						: B(p), s(s)				{}
	constexpr stride_iterator(const stride_iterator<void> &i)	: B((T*)i.p), s(i.s)		{}
	template<typename T2> constexpr stride_iterator(T2 *p)	: B((T*)p), s(sizeof(T2))	{}
	template<typename T2> explicit constexpr stride_iterator(const stride_iterator<T2> &i) : B((T*)i.i), s(i.s) {}

	template<typename C, T C::* f> stride_iterator(field_iterator<C*, T C::*, f> i) : B(&*i), s(sizeof(C)) {}
	template<int S> stride_iterator(fixed_stride_iterator<T, S> i) : B(&*i), s(S) {}

	constexpr T&		operator[](intptr_t x)		const	{ return *(T*)((char*)i + x * s); }
	stride_iterator&	operator++()						{ i = (T*)((char*)i + s); return *this; }
	stride_iterator&	operator--()						{ i = (T*)((char*)i - s); return *this;	}
	stride_iterator		operator++(int)						{ auto t = *this; operator++(); return t; }
	stride_iterator		operator--(int)						{ auto t = *this; operator--(); return t; }
	stride_iterator&	operator+=(intptr_t x)				{ i = (T*)((char*)i + x * s); return *this;	}
	stride_iterator&	operator-=(intptr_t x)				{ i = (T*)((char*)i - x * s); return *this;	}

	constexpr intptr_t	operator-(const stride_iterator &b)	const		{ return s ? ((char*)i - (char*)b.i) / s : 0; }
	constexpr auto		operator-(intptr_t x)				const		{ return stride_iterator(*this) -= x; }
	friend constexpr intptr_t operator-(T* a, const stride_iterator &b)	{ return intptr_t(((char*)a - (char*)b.i) / b.s); }

	constexpr int		stride()		const	{ return s; }
	constexpr explicit operator void*()	const	{ return i; }
	constexpr explicit operator bool()	const	{ return !!i; }
	template<typename U> friend constexpr stride_iterator<U>	element_cast(stride_iterator i)	{ return stride_iterator<U>((U*)i.i, i.s); }
};

template<typename T> constexpr stride_iterator<T> strided(T *p, int s)					{ return stride_iterator<T>(p, s);	}
template<typename T> constexpr stride_iterator<T> strided(const stride_iterator<T> &p, int s) { return stride_iterator<T>(p, p.stride() * s);	}
template<typename T> constexpr stride_iterator<T> make_stride_iterator(T *p, int s)		{ return stride_iterator<T>(p, s);	}
template<typename T> constexpr stride_iterator<T> begin(stride_iterator<T> &i)			{ return i; }
template<typename T> constexpr stride_iterator<T> begin(const stride_iterator<T> &i)	{ return i; }
template<typename T, typename A> auto element_cast(const stride_iterator<A> &c)			{ return stride_iterator<T>(c); }

//-----------------------------------------------------------------------------
//	after - something that exists after another
//-----------------------------------------------------------------------------

template<typename T> void *get_after(const T *t) {
	return (void*)(t + 1);
}

template<typename T, typename B> struct after {
	T&		get()		const	{ return *(T*)align(get_after((B*)this), alignof(T)); }
	operator T&()		const	{ return get(); }
	T*		operator&()	const	{ return &get(); }
	T*		operator->()const	{ return &get(); }
	template<typename T2> T&	operator=(const T2 &t2) { T &t = get(); t = t2; return t; }
	friend T&	get(const after &a)	{ return a; }
	friend T&	put(after &a)		{ return a; }
};

template<typename T, typename B> inline void *get_after(const after<T,B> *t) {
	return get_after(&t->get());
}

//-----------------------------------------------------------------------------
//	param_iterator
//-----------------------------------------------------------------------------

template<typename T, typename P> struct param_element {
//	typedef noref_t<T>	element;
	T			t;
	P			p;
	param_element() {}
	constexpr param_element(T &&t, const P &p) : t(forward<T>(t)), p(p) {}
	template<typename T2, typename P2> constexpr param_element(const param_element<T2, P2> &b) : t(b.t), p(b.p) {}
	operator			decltype(auto)()			{ return t; }
	auto				operator->()				{ return &t; }
	constexpr operator	decltype(auto)()	const	{ return t; }
	constexpr auto		operator->()		const	{ return &t; }
//	template<typename C, typename F> constexpr auto get(F C::*f)		{ return param_element<copy_const_t<T,F&>, P>(t.*f, p); }
//	template<typename C, typename F> constexpr auto get(F C::*f) const	{ return param_element<const F&, P>(t.*f, p); }
};

template<typename T, typename P, typename C, typename F> constexpr auto get_field(param_element<T,P> &a, F C::*f)		{ return param_element<copy_const_t<T,F&>, P>(a.t.*f, a.p); }
template<typename T, typename P, typename C, typename F> constexpr auto get_field(const param_element<T,P> &a, F C::*f) { return param_element<const F&, P>(a.t.*f, a.p); }

template<typename T, typename P> force_inline auto make_param_element(T &&t, P &&p) {
	return param_element<T,param_holder_t<P&&>>(forward<T>(t), forward<P>(p));
}

template<typename I, typename P> struct param_iterator : iterator_wrapper<param_iterator<I,P>, I> {
	typedef	iterator_wrapper<param_iterator<I,P>, I>	B;
	typedef param_element<typename iterator_traits<I>::reference, P> element, reference;
	P			p;
	constexpr param_iterator(I &&i, const P &p) : B(move(i)), p(p) {}
	constexpr param_iterator(const I &i, const P &p) : B(i), p(p) {}
	constexpr element	operator*()			const	{ return element(*B::i, p); }
	constexpr element	operator->()		const	{ return element(*B::i, p); }
	constexpr element	operator[](int j)	const	{ return element(B::i[j], p); }
};

template<typename I, typename P> force_inline auto make_param_iterator(I &&i, P &&p) {
	return param_iterator<noref_cv_t<I>, param_holder_t<P&&>>(forward<I>(i), forward<P>(p));
}

template<typename R, typename P> force_inline auto with_param(R &&r, P &&p) {
	return make_range(make_param_iterator(r.begin(), forward<P>(p)), make_param_iterator(r.end(), forward<P>(p)));
}

template<typename T, typename P> const T&	get(const param_element<T, P> &a)	{ return a.t; }
//template<typename T, typename P> auto		get(param_element<T, P> &&a)->decltype(get(make_const(a)))	{ return get(make_const(a)); }
//template<typename T, typename P> decltype(get(declval<const param_element<T, P>&>()))	get(param_element<T, P> &a)	{ return get(make_const(a)); }
//template<typename T, typename P> auto&&		get(param_element<T, P> &a)								{ return get(make_const(a)); }

template<typename T, typename P1, typename P2>	auto	get(param_element<param_element<T,P1>, P2> &a)			{ return make_param_element(get(a.t), a.p); }
template<typename T, typename P1, typename P2>	auto	get(const param_element<param_element<T,P1>, P2> &a)	{ return make_param_element(get(a.t), a.p); }
template<typename I, typename P>				auto	get(param_element<range<I>, P> &a)						{ return range<param_iterator<I, P> >(param_iterator<I,P>(a.t.begin(), a.p), param_iterator<I,P>(a.t.end(), a.p)); }
template<typename I, typename P>				auto	get(const param_element<range<I>, P> &a)				{ return range<param_iterator<I, P> >(param_iterator<I,P>(a.t.begin(), a.p), param_iterator<I,P>(a.t.end(), a.p)); }
template<typename T, typename B, typename P>	auto	get(const param_element<after<T,B>&, P> &a)				{ return param_element<T&, P>(a.t, a.p); }


template<typename I, typename P> struct param_iterator2 : iterator_wrapper<param_iterator2<I,P>, I> {
	typedef	iterator_wrapper<param_iterator2<I,P>, I>	B;
	typedef param_element<typename iterator_traits<I>::reference, P> element0;
	typedef decltype(get(move(declval<element0>())))	element, reference;
	P			p;
	constexpr param_iterator2(const I &i, const P &p) : B((I&&)i), p(p) {}

	constexpr element	operator*()			const	{ return get(element0(*B::i, p)); }
	constexpr element	operator->()		const	{ return get(element0(*B::i, p)); }
	constexpr element	operator[](int j)	const	{ return get(element0(B::i[j], p)); }
};

template<typename I, typename P> force_inline auto make_param_iterator2(const I &i, P &&p) {
	return param_iterator2<I, param_holder_t<P&&>>(i, forward<P>(p));
}

template<typename R, typename P> force_inline auto with_param2(R &&r, P &&p) {
	return make_range(make_param_iterator2(r.begin(), forward<P>(p)), make_param_iterator2(r.end(), forward<P>(p)));
}

//-----------------------------------------------------------------------------
//	filter_iterator (or use linq)
//-----------------------------------------------------------------------------

template<class C, class F> struct filtered_container {
	typedef typename container_traits<C>::const_iterator I;
	C	c;
	F	f;

	struct iterator : iterator_wrapper<iterator, I, forward_iterator_t> {
		typedef iterator_wrapper<iterator, I, forward_iterator_t>	B;
		const F	&f;
		const I	&e;
		void	next() {
			while (B::i != e && !f(*B::i))
				++B::i;
		}
		iterator(const F& f, I &&i, const I &e)			: B(move(i)),	f(f), e(e) { next(); }
		iterator(const F& f, const I &i, const I &e)	: B(i),			f(f), e(e) { next(); }
		iterator&	operator++()					{ ++B::i; next(); return *this; }
		iterator&	operator=(const iterator& b)	{ B::i = b.i; return *this; }
	};

	filtered_container(C&& c, const F& f)	: c(forward<C>(c)), f(f) {}
	filtered_container(C&& c, F&& f)		: c(forward<C>(c)), f(f) {}
	iterator begin()	const { return iterator(f, c.begin(), c.end()); }
	iterator end()		const { return iterator(f, c.end(), c.end()); }
};

template<class C, class F> auto filter(C &&c, F &&f) { return filtered_container<C,F>(forward<C>(c), forward<F>(f)); }

//-----------------------------------------------------------------------------
//	transformed_container (or use linq)
//-----------------------------------------------------------------------------

template<typename I, typename F, int N = params_t<F>::count> struct transform_iterator : iterator_wrapper<transform_iterator<I,F>, I> {
	typedef iterator_wrapper<transform_iterator<I,F>, I>	B;
	F	f;
	transform_iterator(const I &i, F &&f) : B(i), f(forward<F>(f)) {}

	template<size_t... J>auto helper(index_list<J...>) { return f(B::i[J]...); }
	auto				operator*()				{ return helper(meta::make_index_list<N>()); }
	transform_iterator&	operator++()			{ return B::operator+=(N);}
	transform_iterator&	operator--()			{ return B::operator-=(N);}
};

template<typename I, typename F> struct transform_iterator<I, F, 1> : iterator_wrapper<transform_iterator<I,F>, I> {
	typedef iterator_wrapper<transform_iterator<I,F>, I>	B;
	F	f;
	transform_iterator& operator=(const transform_iterator &b) { B::operator=(b); return *this; }
	transform_iterator(const I &i, F &&f) : B(i), f(forward<F>(f)) {}
	auto	operator*()			{ return f(B::operator*()); }
	auto	operator*()	const	{ return f(B::operator*()); }
	auto	operator->()		{ return make_ref_helper(f(B::operator*())); }
};

template<class C, class F> struct transformed_container {
	C	c;
	F	f;

	typedef typename container_traits<C>::const_iterator I;
	typedef transform_iterator<I, F&>		iterator;
	typedef transform_iterator<I, const F&>	const_iterator;

	template<typename C1, typename F1> transformed_container(C1&& c, F1&& f) : c(forward<C1>(c)), f(forward<F1>(f)) {}
	template<typename C1, typename F1> transformed_container(transformed_container<C1,F1> &&b) : c(move(b.c)), f(move(b.f)) {}
	iterator		begin()			{ return {c.begin(), f}; }
	iterator		end()			{ return {c.end(), f}; }
	const_iterator	begin()	const	{ return {c.begin(), f}; }
	const_iterator	end()	const	{ return {c.end(), f}; }
	size_t			size()	const	{ return c.size(); }
	auto			front()	const	{ return f(c.front()); }
	auto			back()	const	{ return f(c.back()); }

	friend constexpr size_t num_elements(const transformed_container &c) { return num_elements(c.c) / params_t<F>::count; }
};

template<class I, class F> auto transform(const I &i, F &&f)	{ return transform_iterator<I, F>(i, forward<F>(f)); }
template<class C, class F> auto transformc(C &&c, F &&f)		{ return transformed_container<C, F>(forward<C>(c), forward<F>(f)); }

//-----------------------------------------------------------------------------
//	back_insert_iterator - wrap pushes to back of container as output iterator
//-----------------------------------------------------------------------------

template<class C> class back_insert_iterator {
	C& c;

public:
	using iterator_category = output_iterator_t;

	back_insert_iterator(C& c) : c(c) {}

	template<typename V> back_insert_iterator& operator=(V &&v) {
		c.push_back(forward<V>(v));
		return *this;
	}
	back_insert_iterator& operator*()		{ return *this; }
	back_insert_iterator& operator++()		{ return *this; }
	back_insert_iterator  operator++(int)	{ return *this; }
};

template<class C> back_insert_iterator<C> back_inserter(C& c) { return c; }

//-----------------------------------------------------------------------------
//	virtual_iterator
//-----------------------------------------------------------------------------

template<typename T> struct virtual_iterator {
	struct vtable {
		void	(*vnext)(void *p);
		bool	(*vequal)(const void *a, const void *b);
		T		(*vget)(const void *p);
		void*	(*vdup)(const void *p);
		void	(*vdel)(void *p);
	};
	template<typename I> struct vtableT {
		static void		thunk_next(void *p)							{ ++(*(I*)p); }
		static bool		thunk_equal(const void *a, const void *b)	{ return *(I*)a == *(I*)b; }
		static T		thunk_get(const void *p)					{ return **(I*)p; }
		static void*	thunk_dup(const void *p)					{ return new I(*(I*)p); }
		static void		thunk_del(void *p)							{ delete (I*)p; }
		static vtable	table;
	};

	void	*p;
	vtable	*vt;

	virtual_iterator() : p(0) {}
	virtual_iterator(virtual_iterator &&i)		: p(i.p),				vt(i.vt) { i.p = 0; }
	virtual_iterator(const virtual_iterator &i)	: p(i.vt->vdup(i.p)),	vt(i.vt) {}

	template<typename I> virtual_iterator(const I &i) : p(new I(i)), vt(&vtableT<I>::table) {}
	~virtual_iterator()	{ if (p) vt->vdel(p); }

	virtual_iterator&	operator++()					{ vt->vnext(p); return *this; }
	T					operator*()				const	{ return vt->vget(p); }
	ref_helper<T>		operator->()			const	{ return vt->vget(p); }
	bool operator==(const virtual_iterator &b)	const	{ return vt->vequal(p, b.p); }
	bool operator!=(const virtual_iterator &b)	const	{ return !vt->vequal(p, b.p); }
};

template<typename T> template<typename I> typename virtual_iterator<T>::vtable virtual_iterator<T>::vtableT<I>::table = {
	thunk_next, thunk_equal, thunk_get, thunk_dup, thunk_del
};

struct temp_holder {
	void	*p;
	void	(*vdel)(void *p);
	temp_holder(temp_holder &&b)	: p(exchange(b.p, nullptr)), vdel(b.vdel) {}
	~temp_holder()					{ if (p && vdel) vdel(p); }
	template<typename T> temp_holder(T &t)	: p(&t), vdel(nullptr) {}
	template<typename T> temp_holder(T &&t)	: p(new T(forward<T>(t))), vdel([](void *p) {delete (T*)p; }) {}
	template<typename T> T*	as()		const { return (T*)p; }
};

template<typename T> struct virtual_container : temp_holder, range<virtual_iterator<T>> {
	template<typename C> virtual_container(C&& c) : temp_holder(forward<C>(c)), range<virtual_iterator<T>>(as<C>()->begin(), as<C>()->end()) {}
};

//-----------------------------------------------------------------------------
//	repeat
//-----------------------------------------------------------------------------


template<typename T> struct repeat_s {
	T		t;
	int		n;

	struct iterator : iterator_wrapper<iterator, int> {
		T		t;
		template<typename T1> iterator(T1 &&t, int i) : iterator_wrapper<iterator, int>(i), t(forward<T1>(t))	{}
		const T&		operator*()						const	{ return t; }
		ref_helper<T&>	operator->()					const	{ return t; }
		iterator		operator+ (const iterator &b)	const	{ return {t, int(n + b.n)}; }
	};

	repeat_s(T &&t, int n) : t(forward<T>(t)), n(n) {}
	size_t		size()	const	{ return n; }
	iterator	begin()	const	{ return {t, 0}; }
	iterator	end()	const	{ return {t, n}; }
	friend constexpr size_t num_elements(const repeat_s &r) { return r.n; }
};

template<typename T> inline repeat_s<T> repeat(T &&t, int n) {
	return {forward<T>(t), n};
}

template<typename T> struct scalar_s {
	T	t;
	typedef T		element;

	scalar_s(T &&t) : t(forward<T>(t)) {}
	scalar_s(const T &t) : t(t) {}
	operator T()	const	{ return t; }
	auto&			operator++()		const	{ return *this; }
	auto&			operator*()			const	{ return t; }
	ref_helper<T&>	operator->()		const	{ return t; }
	auto&			operator[](int i)	const	{ return t; }
	auto&			begin()				const	{ return *this; }
	auto&			end()				const	{ return *this; }
	template<typename...P> auto	operator()(P&&... p) const	{ return t; }
	friend constexpr size_t num_elements(const scalar_s&)	{ return ~size_t(0); }
};

template<typename T> scalar_s<T> scalar(T &&t) {
	return forward<T>(t);
}
template<typename T> scalar_s<T> scalar(const T &t) {
	return t;
}

}//namespace iso

#endif // ITERATOR_H
