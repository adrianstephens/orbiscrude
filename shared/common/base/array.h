#ifndef ARRAY_H
#define ARRAY_H

#include "defs.h"
#undef small

namespace iso {

template<class W, typename T>	inline bool writen(W &w, const T *t, size_t n);
template<class R, typename T>	inline bool readn(R &r, T *t, size_t n);
template<typename T, class R> const T*	readp(R &r);
template<typename T, class R> const T*	readp(R &r, size_t n);

//-----------------------------------------------------------------------------
//	array allocation
//-----------------------------------------------------------------------------

template<typename T> inline T *reallocate_move(T *p, size_t n0, size_t n1) {
	if (aligned_resize(p, n1 * sizeof(T), alignof(T)))
		return p;

	auto p1 = allocate<T>(n1);
	move_new_n(p, p1, n0);
	deallocate(p, n0);
	return p1;
}


template<typename T> T *new_array(size_t n) {
	T	*p = allocate<T>(n);
	fill_new_n(p, n);
	return p;
}
template<typename T> void delete_array(T *p, size_t n) {
	if (p) {
		destruct(p, n);
		deallocate(p, n);
	}
}

template<typename P> P insert_array(P p, size_t back) {
	typedef deref_t<P>	T;
	if (back) {
		P prev = p--;
		new(prev) T(move(*p));
		while (--back) {
			prev	= p--;
			*prev	= move(*p);
		}
		p->~T();
	}
	return p;
}

template<typename P, typename I> static P insert_array(P p, size_t back, I first, I last) {
	typedef deref_t<P>	T;
	size_t	n	= distance(first, last);
	P		s	= p + back;
	P		d	= s + n;

	size_t	nt	= min(n, back);
	back	-= nt;
	n		-= nt;
	while (nt--)
		new(--d) T(move(*--s));
	while (back--)
		*--d = *--s;
	while (n--)
		new(--d) T(*--last);
	while (last != first)
		*--d = *--last;
	return d;
}

template<typename P> void erase_array(P first, P last, P end) {
	auto	n = last - first;
	move_n(last, first, end - last);
	destruct(end - n, n);
}

//-----------------------------------------------------------------------------
//	array compare
//-----------------------------------------------------------------------------

template<class I1, class I2> bool equal_array_n(I1 i1, I2 i2, size_t n) {
	while (n--) {
		if (*i1 != *i2)
			return false;
	}
	return true;
}

template<class I1, class I2> int compare_array(I1 i1, I1 end1, I2 i2, I2 end2) {
	for (; i2 != end2; ++i1, ++i2) {
		if (i1 == end1)
			return -1;
		if (int r = simple_compare(*i1, *i2))
			return r;
	}
	return i1 == end1 ? 0 : 1;
}

template<class C1, class C2> int compare_array(const C1 &c1, const C2 &c2) {
	return compare_array(begin(c1), end(c1), begin(c2), end(c2));
}

template<typename T> struct array_comparisons {
	template<typename U> friend enable_if_t<has_end_v<U>, bool> operator==(const T &a, const U &b) {
		auto	n = a.size();
		return n == b.size() && equal_array_n(begin(a), begin(b), n);
	}
	template<typename U> friend enable_if_t<has_end_v<U>, bool> operator!=(const T &a, const U &b) { return !(a == b); }
	template<typename U> friend enable_if_t<has_end_v<U>, bool> operator< (const T &a, const U &b) { return compare_array(a, b) <  0; }
	template<typename U> friend enable_if_t<has_end_v<U>, bool> operator<=(const T &a, const U &b) { return compare_array(a, b) <= 0; }
	template<typename U> friend enable_if_t<has_end_v<U>, bool> operator> (const T &a, const U &b) { return compare_array(a, b) >  0; }
	template<typename U> friend enable_if_t<has_end_v<U>, bool> operator>=(const T &a, const U &b) { return compare_array(a, b) >= 0; }
};

//-----------------------------------------------------------------------------
//	class array_mixout
//	needs:	begin(), end(), [maybe size(), index_of(const T&)]
//-----------------------------------------------------------------------------

template<typename C, typename T> class array_mixout : public C {
public:
	using C::C;
	using C::begin; using C::end; using C::size; 
	typedef	decltype(declval<C>().begin())	I;

	template<typename R> bool	read(R &&r)				{ return readn(r, begin(), size()); }
	
	constexpr bool				empty()			const	{ return size() == 0; }
	constexpr uint32			size32()		const	{ return uint32(size()); }
	constexpr decltype(auto)	front()			const	{ return *begin(); }
	constexpr decltype(auto)	back()			const	{ return end()[-1]; }
	ISO_NOT_DEBUG(constexpr) decltype(auto)	at(size_t i)	const	{ ISO_ASSERT(i < size()); return begin()[i]; }
	ISO_NOT_DEBUG(constexpr) decltype(auto)	back(size_t i)	const	{ ISO_ASSERT(i < size()); return end()[~intptr_t(i)]; }

	decltype(auto)	front()					{ return *begin(); }
	decltype(auto)	back()					{ return end()[-1]; }
	decltype(auto)	at(size_t i)			{ ISO_ASSERT(i < size()); return begin()[i]; }
	decltype(auto)	back(size_t i)			{ ISO_ASSERT(i < size()); return end()[~intptr_t(i)]; }

	template<typename W> bool	write(W&& w)	const	{ return writen(w, begin(), size()); }

	template<typename A>				auto	slice(A&& a1)					{ return slice_a( begin(), end(), forward<A>(a1)); }
	template<typename B>				auto	slice_to(B&& b1)				{ return slice_b( begin(), end(), forward<B>(b1)); }
	template<typename A, typename B>	auto	slice(A&& a1, B&& b1)			{ return slice_ab(begin(), end(), forward<A>(a1), forward<B>(b1)); }

	template<typename A>				auto	slice(A&& a1)			const	{ return slice_a( begin(), end(), forward<A>(a1)); }
	template<typename B>				auto	slice_to(B&& b1)		const	{ return slice_b( begin(), end(), forward<B>(b1)); }
	template<typename A, typename B>	auto	slice(A&& a1, B&& b1)	const	{ return slice_ab(begin(), end(), forward<A>(a1), forward<B>(b1)); }
};

//-----------------------------------------------------------------------------
//	class dynamic_mixout
//	needs:	_expand(size_t), _set_size(size_t), begin(), end(), size(), index_of(const T&)
//-----------------------------------------------------------------------------

template<typename C, typename T> class dynamic_mixout : public array_mixout<C, T> {
public:
	typedef array_mixout<C, T>	B;
	using typename B::I;
	using B::B;
	using C::operator=;
	using B::begin; using B::end; using B::size; 
	using C::_expand; using C::_set_size;

	//resize
	auto&	resize(size_t i) {
		auto	n		= size();
		if (i < n)
			destruct(begin() + i, n - i);
		else if (i > n)
			fill_new_n(_expand(i - n), i - n);
		_set_size(i);
		return *this;
	}
	auto&	resize(size_t i, const T &t) {
		auto	n	= size();
		if (i < n)
			destruct(begin() + i, n - i);
		else if (i > n)
			fill_new_n(_expand(i - n), i - n, t);
		_set_size(i);
		return *this;
	}

	//expand
	I	expand() {
		return new(_expand(1)) T;
	}
	I	expand(size_t n) {
		auto	i = _expand(n);
		fill_new_n(i, n);
		return i;
	}

	auto& reserve(size_t n) {
		auto s = size();
		if (n > s) {
			_expand(n - s);
			_set_size(n);
		}
		return *this;
	}

	//insert
	I	insert(I iter) {
		ISO_ASSERT(between(iter, begin(), end()));
		size_t	back	= end() - iter;
		return new(insert_array(_expand(1), back)) T;
	}
	I	insert(I iter, const T &t) {
		ISO_ASSERT(between(iter, begin(), end()));
		int		i		= index_of(t);
		size_t	back	= end() - iter;
		return new(insert_array(_expand(1), back)) T(i >= 0 && i < size() - 1 ? begin()[i + int(&t >= iter)] : t);
	}
	template<typename U> I insert(I iter, U &&t)	{
		ISO_ASSERT(between(iter, begin(), end()));
		size_t	back	= end() - iter;
		return new(insert_array(_expand(1), back)) T(forward<U>(t));
	}
	template<typename J> I insert(I iter, J first, J last) {
		ISO_ASSERT(between(iter, begin(), end()));
		size_t	back	= end() - iter;
		return insert_array(_expand(distance(first, last)), back, first, last);
	}
	template<typename A> I insertc(I iter, A &c) {
		return insert(iter, begin(c), end(c));
	}
	template<typename... U> I emplace(I iter, U &&...t)	{
		ISO_ASSERT(between(iter, begin(), end()));
		size_t	back	= end() - iter;
		return new(insert_array(_expand(1), back)) T(forward<U>(t)...);
	}

	//append
	template<typename J>	enable_if_t<!is_int<J>, dynamic_mixout&>	append(J first, J last) {
		size_t n = distance(first, last); 
		copy_new_n(first, _expand(n), n);
		return *this;
	}
	template<typename A>	enable_if_t<has_end_v<A>, dynamic_mixout&>	append(A &&c)	{
		using iso::begin;
		size_t n = num_elements(c);
		copy_new_n(begin(c), _expand(n), n);
		return *this;
	}
	template<typename...P>	auto&	append(size_t n, P&&... p)		{ fill_new_n(_expand(n), n, T(forward<P>(p)...)); return *this; }

	//assign
	template<typename A>	auto&	assign(A &&c)					{ clear(); return append(forward<A>(c)); }
	template<typename J>	auto&	assign(J first, J last)			{ clear(); return append(first, last); }
	template<typename U>	auto&	assign(size_t n, const U &t)	{ clear(); return append(n, t); }
	template<typename A>	auto&	operator=(A &&c)				{ clear(); return append(forward<A>(c)); }

	//erase
	I		erase_unordered(I i) {
		ISO_ASSERT(i >= begin() && i < end());
		auto	end1 = end() - 1;
		move_n(end1, i, 1);
		destruct(end1, 1);
		_set_size(end1 - begin());
		return i;
	}
	I		erase_unordered(I first, I last) {
		ISO_ASSERT(last >= first && first >= begin() && last <= end());
		auto	n		= last - first;
		auto	end1	= end() - n;
		if (end1 > last)
			move_n(end1, first, n);
		else if (end1 > first)
			move_n(last, first, end1 - first);
		destruct(end1, n);
		_set_size(end1 - begin());
		return first;
	}
	I		erase(I i) {
		ISO_ASSERT(i >= begin() && i < end());
		auto	end1 = end() - 1;
		move_n(i + 1, i, end1 - i);
		destruct(end1, 1);
		_set_size(end1 - begin());
		return i;
	}
	I		erase(I first, I last) {
		ISO_ASSERT(last >= first && first >= begin() && last <= end());
		auto	n		= last - first;
		auto	end1	= end() - n;
		move_n(last, first, end1 - first);
		destruct(end1, n);
		_set_size(end1 - begin());
		return first;
	}

	// push_back
	T&							push_back()				{ return *new(unconst(_expand(1))) T; }
	template<typename U>	T&	push_back(U &&t)		{ return *new(unconst(_expand(1))) T(forward<U>(t)); }
	template<typename... U>	T&	emplace_back(U&&...t)	{ return *new(unconst(_expand(1))) T(forward<U>(t)...); }

	// pop_back
	void						pop_back() {
		auto end1 = end() - 1;
		destruct(end1, 1);
		_set_size(end1 - begin());
	}
	auto						pop_back_value() {
		auto end1	= end() - 1;
		auto t		= move(*end1);
		destruct(end1, 1);
		_set_size(end1 - begin());
		return t;
	}
	auto&						pop_back_retref() {
		auto end1	= end() - 1;
		_set_size(end1 - begin());
		return *end1;
	}

	void						clear()					{ destruct(begin(), size()); _set_size(0); }
	T&							set(int i)				{ if (i >= size()) resize(i + 1); return begin()[i]; }
	template<typename R> bool	read(R &&r)				{ return readn(r, begin(), size()); }
	template<typename R> bool	read(R &&r, size_t n)	{ resize(n); return readn(r, begin(), n); }

	dynamic_mixout()	{}	//msvc needs this
	dynamic_mixout(const dynamic_mixout&)	= default;
	dynamic_mixout(dynamic_mixout&&)		= default;
	~dynamic_mixout()	{ destruct(begin(), size()); }
};

//-----------------------------------------------------------------------------
//	class dynamic_de_mixout
//	also needs:	_expand_front(size_t), _move_front(size_t)
//-----------------------------------------------------------------------------

template<typename C, typename T> class dynamic_de_mixout : public dynamic_mixout<C, T> {
public:
	typedef dynamic_mixout<C, T>	B;
	using typename B::I;
	using B::B;
	using B::operator=;
	using B::begin; using B::end; using B::size; 
	using B::_expand; using B::_set_size;
	using C::_expand_front; using C::_move_front;

	//prepend
	template<typename J>	enable_if_t<!is_int<J>, dynamic_de_mixout&>		prepend(J first, J last) {
		size_t n = distance(first, last); 
		copy_new_n(first, _expand_front(n), n);
		return *this;
	}
	template<typename A>	enable_if_t<has_end_v<A>, dynamic_de_mixout&>	prepend(A &&c)	{
		using iso::begin;
		size_t n = num_elements(c);
		copy_new_n(begin(c), _expand_front(n), n);
		return *this;
	}
	template<typename...P>	auto&	prepend(size_t n, P&&... p)		{ fill_new_n(_expand_front(n), n, T(forward<P>(p)...)); return *this; }

	//erase
	I		erase(I i) {
		auto	a = begin(), b = end();
		ISO_ASSERT(i >= a && i < b);
		if (i - a <= b - i) {
			move_n(a, a + 1, i - a);
			destruct(a, 1);
			_move_front(1);
		} else {
			--b;
			move_n(i + 1, i, b - i);
			destruct(b, 1);
			_set_size(b - a);
		}
		return i;
	}
	I		erase(I first, I last) {
		auto	a	= begin(), b = end();
		auto	n	= last - first;
		ISO_ASSERT(last >= first && first >= a && last <= b);
		if (first - a < b - last) {
			move_n(a, a + n, first - a);
			destruct(a, n);
			_move_front(n);
		} else {
			b -= n;
			move_n(last, first, b - first);
			destruct(b, n);
			_set_size(b - a);
		}
		return first;
	}

	// push_front
	T&							push_front()				{ return *new(_expand_front(1)) T; }
	template<typename U>	T&	push_front(U &&t)			{ return *new(_expand_front(1)) T(forward<U>(t)); }
	template<typename... U>	T&	emplace_front(U&&...t)		{ return *new(_expand_front(1)) T(forward<U>(t)...); }

	// pop_front
	void						pop_front() {
		destruct(begin(), 1);
		_move_front(1);
	}
	auto						pop_front_value() {
		auto a		= begin();
		auto t		= move(*a);
		destruct(a, 1);
		_move_front(1);
		return t;
	}
	auto&						pop_front_retref() {
		auto a		= begin();
		_move_front(1);
		return *a;
	}
};

//-----------------------------------------------------------------------------
//	class array - fixed memory and size
//-----------------------------------------------------------------------------

template<typename T, typename I> struct auto_iterator {
	I	i;
	operator T()		{ return *i++; }
	auto_iterator(I i) : i(i) {}
};

template<typename T, typename I> auto_iterator<T, I> make_auto_iterator(I i)	{ return i; }
template<typename T, typename C> auto make_auto_iteratorc(C&& c)				{ return make_auto_iterator<T>(begin(c)); }

template<typename T, int N> class _fixed_array {
public:
	T t[N];
	constexpr _fixed_array() {}
	template<typename... U>				constexpr _fixed_array(const U&... u)			: t{T(u)...} {}
	template<typename U, size_t...I>	constexpr _fixed_array(U &&u, index_list<I...>)	: t{(I, T(forward<U>(u)))...} {}
	template<typename U>				constexpr _fixed_array(meta::num<0>, U &&u)		: _fixed_array(forward<U>(u), meta::make_index_list<N>()) {}
	template<typename U>				constexpr _fixed_array(meta::num<1>, U &&u)		: _fixed_array(make_auto_iterator<T>(forward<U>(u)), meta::make_index_list<N>()) {}
	template<typename U>				constexpr _fixed_array(meta::num<2>, U &&u)		: _fixed_array(make_auto_iteratorc<T>(forward<U>(u)), meta::make_index_list<N>()) {}

	constexpr size_t	size()					const	{ return N; }
	constexpr const T*	begin()					const	{ return t; }
	constexpr const T*	end()					const	{ return t + N; }
	constexpr T*		begin()							{ return t; }
	constexpr T*		end()							{ return t + N; }

	constexpr bool		contains(const T *e)	const	{ return e && e >= begin() && e < end(); }
	constexpr bool		contains(const T &e)	const	{ return &e >= begin() && &e < end(); }
	constexpr int		index_of(const T *e)	const	{ return e ? e - begin() : -1; }
	constexpr int		index_of(const T &e)	const	{ return &e - begin(); }
};

template<typename A, typename B, typename V = void> struct test_array_s2 {};
template<typename A, typename B> struct test_array_s : test_array_s2<A, B> {};
template<typename A, typename B> using test_array = typename test_array_s<A, B>::type;

template<typename T, int N> class array : public array_mixout<_fixed_array<T, N>, T> {
	typedef	array_mixout<_fixed_array<T, N>, T>	B;
public:
	using B::t;
	constexpr array()				{}
	constexpr array(initializer_list<T> c) 			: B(meta::num<0>(), make_auto_iteratorc<T>(c))			{}
	template<typename U, typename V = test_array<array, noref_t<U>>>	constexpr array(U &&u)	: B(V(), forward<U>(u)) {}
	template<typename U1, typename U2, typename... UU>	constexpr array(const U1& u1, const U2& u2, const UU&... uu) : B(u1, u2, uu...) {}
//	template<typename R, typename=is_reader_t<R>>	array(R &&r)	{ readn(r, this->p, n); }

	template<typename C> enable_if_t<has_end_v<C>, array&>		operator=(const C &c)	{ using iso::begin; copy_n(begin(c), t, num_elements(c)); return *this; }
	template<typename I> enable_if_t<is_iterator_v<I>, array&>	operator=(I i)			{ copy_n(i, t, N); return *this; }

	constexpr operator	T*()				{ return t; }
	constexpr operator	const T*()	const	{ return t; }

	constexpr memory_block			raw_data()			{ return {t, N * sizeof(T)}; }
	constexpr const_memory_block	raw_data()	const	{ return {t, N * sizeof(T)}; }
};

template<typename A, typename B>					struct test_array_s2<A, B, enable_if_t<has_end_v<B>>>		: T_type<meta::num<2>> {};
template<typename A, typename B>					struct test_array_s2<A, B, enable_if_t<is_iterator_v<B>>>	: T_type<meta::num<1>> {};
template<typename T, int N, typename B>				struct test_array_s2<array<T, N>, B, enable_if_t<!has_end_v<B> && constructable_v<T, B>>>	: T_type<meta::num<0>> {};

template<typename T1, int N1, typename T2, int N2>	struct test_array_s<array<T1, N1>, array<T2, N2>> {};
template<typename T1, typename T2, int N>			struct test_array_s<array<T1, N>, array<T2, N>> : T_type<meta::num<2>> {};

template<typename T, typename ...U> constexpr auto make_array(const T &t, U&&... u) {
	return array<T, sizeof...(U) + 1>(t, forward<U>(u)...);
}

template<typename T, int N> struct T_swap_endian_type<array<T, N>> : T_type<array<T_swap_endian<T>, N>> {};

//-----------------------------------------------------------------------------
//	class embedded_array
//-----------------------------------------------------------------------------

template<typename T, typename C> class _embedded_array {
protected:
	C			count;
public:
	size_t		size()	const	{ return count; }
	auto		begin()	const	{ return (const T*)get_after(&count); }
	auto		end()	const	{ return begin() + count; }
};

template<typename T, typename C> class embedded_array : public array_mixout<_embedded_array<T, C>, T> {
public:
	operator const T*()	const	{ return this->begin(); }

	template<typename R> bool read(R &r) {
		return r.read(this->count) && readn(r, this->begin(), this->count);
	}
	template<typename R> static const embedded_array *get_ptr(R &r) {
		const embedded_array	*p = r.get_ptr();
		readp<T>(r, p->size());
		return p;
	}
	friend void *get_after(const embedded_array *t)	{
		return (void*)t->end();
	}
};

template<typename T, typename C> class _embedded_next_array : public _embedded_array<T, C> {
public:
	typedef next_iterator<T>	iterator;
	iterator	begin()	const	{ return iterator((T*)_embedded_array<T, C>::begin(), 0); }
	iterator	end()	const	{ return iterator((T*)_embedded_array<T, C>::begin(), this->count); }
};
template<typename T, typename C> class embedded_next_array : public array_mixout<_embedded_next_array<T, C>, T> {
	friend void *get_after(const embedded_next_array *t)	{
		return (void*)t->end();
	}
};

//-----------------------------------------------------------------------------
//	class ref_array
//	reference counted array
//-----------------------------------------------------------------------------

template<typename T> class ref_array {
	struct P {
		uint32	nrefs;
		T		array[];
		P()					: nrefs(1) {}
		void	addref()	{ ++nrefs; }
		void	release()	{ if (--nrefs == 0) delete this; }
		P*		resize(size_t n)					{ return (P*)aligned_realloc(this, sizeof(P) + n * sizeof(T), alignof(T)); }
		void*	operator new(size_t s, size_t n)	{ return aligned_alloc(s + n * sizeof(T), alignof(T)); }
		void	operator delete(void *p)			{ return aligned_free(p); }
	};

	P		*p;
public:
	template<int N> struct C {
		uint32	nrefs;
		T		array[N];
	};

	ref_array()			: p(0)					{}
	ref_array(const ref_array &b)				{ if (p = b.p) p->addref(); }
	ref_array(ref_array &&b)	: p(exchange(b.p, nullptr))	{}
	ref_array(size_t n)	: p(new(n) P)			{}
	ref_array(size_t n, const T *t)	: p(new(n) P)	{ copy_new_n(t, p->array, n); }
	ref_array(size_t n, const T &t)	: p(new(n) P)	{ fill_new_n(p->array, n, t); }
	template<int N> ref_array(const C<N> &b)	{ p = (P*)&b; p->addref(); }
	template<typename C, typename = enable_if_t<!is_int<C>>> ref_array(const C &c) {
		using iso::begin;
		size_t	n	= num_elements(c);
		p			= new(n) P;
		copy_new_n(begin(c), p->array, n);
	}
	~ref_array()								{ if (p) p->release(); }
	void		operator=(const ref_array &b)	{ if (b.p) b.p->addref(); if (p) p->release(); p = b.p; }
	void		operator=(ref_array &&b)		{ swap(p, b.p); }

	T*			begin()							{ return p ? p->array : 0; }
	const T*	begin()			const			{ return p ? p->array : 0; }
	operator	T*()			const			{ return p ? p->array : 0; }
	bool		shared()		const			{ return p && p->nrefs > 1; }

	void		clear()							{ if (p) { p->release(); p = 0; } }
	void		create(size_t n)				{ clear(); p = new(n) P; }
	void		create(size_t n, const T *t)	{ create(n); copy_new_n(t, p->array, n); }
	void		create(size_t n, const T &t)	{ create(n); fill_new_n(p->array, n, t); }
	void		grow(size_t n) {
		if (p) {
			if (p->nrefs > 1) {
				P	*p2 = new(n) P;
				copy_new_n(p2->array, p->array, n);
				p->release();
				p = p2;
			} else {
				p = p->resize(n);
			}
		} else {
			p = new(n) P;
		}
	}

	ref_array	dup(int n)		const			{ return ref_array(n, begin()); }
	friend void	swap(ref_array &a, ref_array &b){ swap(a.p, b.p); }
	friend T*	get(const ref_array &a)			{ return a; }
	friend T*	put(ref_array &a)				{ return a; }
};

template<typename T, T... args> struct static_ref_array {
	enum { size = sizeof...(args) };
	static typename ref_array<T>::template C<sizeof...(args)>	x;
};
template<typename T, T... args> typename ref_array<T>::template C<sizeof...(args)>	static_ref_array<T, args...>::x = {1, {args... }};

template<typename T> class ref_array_size : public ref_array<T> {
	size_t	n;
public:
	ref_array_size(const _none&) {}
	template<typename C, typename=enable_if_t<has_begin_v<C>>> 	ref_array_size(const C &c)		: ref_array<T>(c), n(c.size()) {}
	ref_array_size(size_t n) : ref_array<T>(n), n(n) {}
	auto	end()				{ return this->begin() + n; }
	auto	end()		const	{ return this->begin() + n; }
	size_t	size()		const	{ return n; }
	uint32	size32()	const	{ return uint32(n); }
	bool	empty()		const	{ return n == 0; }
};

//-----------------------------------------------------------------------------
//	class compact_array - held as just a pointer
//-----------------------------------------------------------------------------

template<typename T> class _compact_array {
protected:
	struct P {
		size_t	max_size;
		size_t	curr_size;

		T	*array()	const { return align((T*)(this + 1), alignof(T)); }
		P(size_t max_size, size_t curr_size = 0)	: max_size(max_size), curr_size(curr_size) {}
		void*	operator new(size_t s, size_t n)	{ return aligned_alloc(s + n * sizeof(T), alignof(T)); }
		void	operator delete(void *p)			{ return aligned_free(p); }
	};
	typedef T*	iterator;

	P		*p;

	void	_reserve(size_t n)	{
		if (p) {
			if (aligned_resize(p, sizeof(P) + n * sizeof(T), alignof(T))) {
				p->max_size = n;
			} else {
				P	*p2 = new(n) P(n, p->curr_size);
				move_new_n(p->array(), p2->array(), p->curr_size);
				aligned_free(p);
				p = p2;
			}
		} else {
			p = new(n) P(n);
		}
	}

	T*		_expand(size_t n) {
		if (!p || p->curr_size + n > p->max_size)
			_reserve(max(p ? p->curr_size + n : n, p ? p->max_size * 2 : 16));
		return p->array() + exchange(p->curr_size, p->curr_size + n);
	}
	void	_set_size(size_t n) {
		p->curr_size = n;
	}

public:
	constexpr _compact_array()				: p(0)				{}
	constexpr _compact_array(size_t n)		: p(new(n) P(n, n))	{}
	constexpr _compact_array(_compact_array &&b) 		: p(exchange(b.p, nullptr)) {}
	_compact_array(const _compact_array &b) : _compact_array(b.size()) { copy_new_n(b.begin(), begin(), p->curr_size); }
	~_compact_array() { aligned_free(p); }

	constexpr operator	T*()					const	{ return p ? p->array() : nullptr; }
	constexpr T*		begin()					const	{ return p ? p->array() : 0; }
	constexpr T*		end()					const	{ return p ? p->array() + p->curr_size : 0; }
	constexpr size_t	size()					const	{ return p ? p->curr_size : 0; }
	constexpr size_t	capacity()				const	{ return p ? p->max_size : 0; }

	constexpr bool		contains(const T *e)	const	{ return p && e >= p && e < p->array() + p->curr_size; }
	constexpr bool		contains(const T &e)	const	{ return p && &e >= p && &e < p->array() + p->curr_size; }
	constexpr int		index_of(const T *e)	const	{ return p && e ? int(e - p->array()) : -1; }
	constexpr int		index_of(const T &e)	const	{ return p ? &e - p->array() : -1; }
};

template<typename T> class compact_array : array_comparisons<compact_array<T>>, public dynamic_mixout<_compact_array<T>, T> {
	typedef dynamic_mixout<_compact_array<T>, T>	B;
public:
	using B::begin; using B::p;
	compact_array(const compact_array&) = default;
	compact_array(compact_array&&)		= default;

	constexpr compact_array()				{}
	constexpr compact_array(const _none&)	{}
	compact_array(size_t n)										: B(n)						{}
	compact_array(initializer_list<T> init)						: B(init.size())			{ copy_new_n(init.begin(), begin(), p->curr_size); }
	template<typename U> compact_array(size_t n, const U &t)	: B(n)						{ fill_new_n(begin(), n, t); }
	template<typename I> compact_array(I first, I last)			: B(distance(first, last))	{ copy_new_n(first, begin(), this->size()); }
	template<typename C> compact_array(const C &c)				: B(num_elements32(c))		{ using iso::begin; copy_new_n(begin(c), begin(), p->curr_size); }
	template<typename C, typename=enable_if_t<!is_int<C>>> compact_array(const C &c) : B(num_elements(c)) { using iso::begin; copy_new_n(begin(c), begin(), p->curr_size); }

	compact_array&		operator=(const _none&)					{ this->clear(); return *this; }
	compact_array&		operator=(const compact_array &b)		{ return operator=(compact_array(b)); }
	compact_array&		operator=(compact_array &&b)			{ swap(B::p, b.p); return *this; }
	template<typename C> compact_array &operator=(C &&c)		{ return operator=(compact_array(forward<C>(c))); }
};

//-----------------------------------------------------------------------------
//	class static_array - fixed memory, variable size
//-----------------------------------------------------------------------------

template<typename T, int N> class _static_array : array_comparisons<_static_array<T, N>>  {
protected:
	space_for<T[N]>		t;
	uint32				curr_size;
	auto		_expand(size_t n)	{ ISO_ASSERT(curr_size + n <= N); T *r = t + curr_size; curr_size += n; return r; }
	void		_set_size(size_t n)	{ curr_size = uint32(n); }
public:
	_static_array()			: curr_size(0)	{}
	_static_array(int n)	: curr_size(n)	{ ISO_ASSERT(curr_size <= N); }

	constexpr operator		T*()						{ return t; }
	constexpr operator		const T*()			const	{ return t; }
	constexpr const T*		begin()				const	{ return t; }
	constexpr const T*		end()				const	{ return t + curr_size; }
	T*						begin()						{ return t; }
	T*						end()						{ return t + curr_size; }
	constexpr uint32		size()				const	{ return curr_size; }

	constexpr bool			contains(const T *e) const	{ return e && e >= t && e < end(); }
	constexpr bool			contains(const T &e) const	{ return &e >= t && &e < end(); }
	constexpr int			index_of(const T *e) const	{ return e ? e - t : -1; }
	constexpr int			index_of(const T &e) const	{ return &e - t; }

	constexpr size_t		capacity()			const	{ return N; }
	constexpr bool			full()				const	{ return curr_size == N; }
};

template<typename T, int N> class static_array : public dynamic_mixout<_static_array<T, N>, T> {
	typedef	dynamic_mixout<_static_array<T, N>, T>	B;
public:
	static_array()		{}
	template<typename U> static_array(int n, const U &t)	: B(n)						{ ISO_ASSERT(B::curr_size <= N); fill_new_n(B::begin(), n, t); }
	template<typename I> static_array(I first, I last)		: B(distance(first, last))	{ ISO_ASSERT(B::curr_size <= N); copy_new_n(first, B::begin(), B::curr_size); }
	template<typename C> static_array(const C &c)			: B(num_elements32(c))		{ ISO_ASSERT(B::curr_size <= N); using iso::begin; copy_new_n(begin(c), begin(), B::curr_size); }
	template<typename C> static_array&	operator=(C &&c)	{ using iso::begin; this->clear(); copy_new_n(begin(c), B::begin(), B::curr_size = num_elements(c)); return *this; }
	operator sized_placement()	{ return {B::_expand(1), sizeof(T)}; }

	friend constexpr size_t num_elements(const static_array &t)	{ return t.size(); }
};

//-----------------------------------------------------------------------------
//	trailing_array
//-----------------------------------------------------------------------------

template<typename B, typename M> struct trailing_array {
	struct B2 : B { M array[0]; };
	auto			me()					const	{ return static_cast<const B2*>(this); }
	auto			me()							{ return static_cast<B2*>(this); }

	static size_t	calc_size(uint32 n)				{ return (size_t)&(((B2*)1)->array[n]) - 1; }
	static size_t	calc_size(size_t s, uint32 n)	{ return calc_size(n) + s - sizeof(B); }

	const M*		begin()					const	{ return me()->array; }
	M*				begin()							{ return me()->array; }
	auto&			operator[](int i)		const	{ return begin()[i]; }
	auto&			operator[](int i)				{ return begin()[i]; }
	auto&			front()							{ return *begin(); }
	auto&			front()					const	{ return *begin()[-1]; }
	constexpr int	index_of(const M *e)	const	{ return e ? int(e - begin()) : -1; }
	constexpr int	index_of(const M &e)	const	{ return int(&e - begin()); }


	constexpr trailing_array()	{}
	trailing_array(uint32 n)	{ fill_new_n(begin(), n); }
	template<typename C, typename = enable_if_t<!is_int<C>>> trailing_array(C &&c) { using iso::begin; copy_new_n(begin(c), begin(), num_elements(c)); }

	void*						operator new(size_t s, uint32 n)			{ return B::operator new(calc_size(s, n)); }
	void						operator delete(void *p, size_t, uint32)	{ B::operator delete(p); }
	template<typename T> static void*	alloc(size_t s, T &t, uint32 n)		{ return t.alloc(calc_size(s, n), alignof(B)); }
	template<typename T> void*	operator new(size_t s, uint32 n, T &t)		{ return alloc(s, t, n); }
};

template<typename B, typename M> struct calc_size_s<trailing_array<B,M>> {
	template<typename...P> static constexpr size_t f(uint32 n, P&&...) { return trailing_array<B,M>::calc_size(n); }
};

//-----------------------------------------------------------------------------
//	trailing_array2 - requires a size() implementation
//-----------------------------------------------------------------------------

template<typename B, typename M> struct trailing_array2 : trailing_array<B, M> {
	using trailing_array<B, M>::me;
	using trailing_array<B, M>::begin;
	auto		size()			const	{ return me()->size(); }
	auto		end()					{ return begin() + size(); }
	auto		end()			const	{ return begin() + size(); }
	auto&		back()					{ return end()[-1]; }
	auto&		back()			const	{ return end()[-1]; }
	explicit operator bool()	const	{ return size() != 0; }
};

//-----------------------------------------------------------------------------
//	ptr_array - pointer + size
//-----------------------------------------------------------------------------

template<typename T> class _ptr_array : array_comparisons<_ptr_array<T>> {
protected:
	T		*p;
	size_t	curr_size;
	void	_set_size(size_t n) { curr_size = n; }
	T*		_expand(size_t n)	{ return nullptr; }

public:
	constexpr _ptr_array()					: p(nullptr), curr_size(0) {}
	constexpr _ptr_array(T *p, size_t n)	: p(p), curr_size(n) {}

	constexpr operator		T*()					const	{ return p; }
	constexpr const T*		begin()					const	{ return p; }
	constexpr const T*		end()					const	{ return p + curr_size; }
	T*						begin()							{ return p; }
	T*						end()							{ return p + curr_size; }
	constexpr size_t		size()					const	{ return curr_size; }

	constexpr bool			contains(const T *e)	const	{ return e >= p && e < p + curr_size; }
	constexpr bool			contains(const T &e)	const	{ return &e >= p && &e < p + curr_size; }
	constexpr int			index_of(const T *e)	const	{ return e ? int(e - p) : -1; }
	constexpr int			index_of(const T &e)	const	{ return int(&e - p); }

	memory_block			raw_data()						{ return {p, curr_size * sizeof(T)}; }
	const_memory_block		raw_data()				const	{ return {p, curr_size * sizeof(T)}; }
};

template<typename T> class _ptr_max_array : public _ptr_array<T> {
	typedef _ptr_array<T>	B;
protected:
	size_t	max_size;
	T*		_expand(size_t n) {
		return B::curr_size + n <= B::max_size ? B::p + exchange(B::curr_size, B::curr_size + n) : nullptr;
	}
public:
	_ptr_max_array() : max_size(0) {}
	_ptr_max_array(T *p, size_t curr_size, size_t max_size) : _ptr_array<T>(p, curr_size), max_size(max_size) {}
	size_t	capacity()	const	{ return max_size; }
};

template<typename T> using ptr_array = array_mixout<_ptr_array<T>, T>;

//-----------------------------------------------------------------------------
//	class auto_array:	for use with alloca
//-----------------------------------------------------------------------------

template<typename T> class _auto_array : public _ptr_array<T> {
	typedef _ptr_array<T>	B;
public:
	_auto_array(T *p, size_t n)				: B(p, n) {	fill_new_n(p, n); }
	_auto_array(T *p, size_t n, const T &t)	: B(p, n) {	fill_new_n(p, n, t); }
	template<typename I, typename=enable_if_t<is_iterator_v<I>>>	_auto_array(T *p, size_t n, I i)	: B(p, n)	{ copy_new_n(i, p, n); }
	template<typename R, typename=is_reader_t<R>>					_auto_array(T *p, size_t n, R &&r)	: B(p, n)	{ read_new_n(r, p, n); }
};
template<typename T> using auto_array = array_mixout<_auto_array<T>, T>;

#define new_auto(T,N)			auto_array<T>(alloc_auto(T, N), N)
#define new_auto_init(T,N,X)	auto_array<T>(alloc_auto(T, N), N, X)
#define new_auto_container(T,C)	new_auto_init(T, num_elements(C), iso::begin(C))

//-----------------------------------------------------------------------------
//	class dynamic_array
//-----------------------------------------------------------------------------

template<typename T, int N> class _dynamic_array : public _ptr_max_array<T> {
	typedef	_ptr_max_array<T>	B;
protected:
	using B::p; using B::curr_size; using B::max_size;
	space_for<T[N]>	space;

	bool	is_small() const	{ return p == (const T*)&space; }
	void	set_small()			{ p == (const T*)&space; }
	void	_set_size(size_t n) { curr_size = n; }

	void	_reserve(size_t n) {
		if (n > N) {
			if (is_small()) {
				p = allocate<T>(n);
				move_new_n((T*)&space, p, curr_size);
			} else {
				p = reallocate_move(p, curr_size, n);
			}
			max_size	= n;
		}
	}
	T*		_expand(size_t n) {
		if (curr_size + n > max_size)
			_reserve(max(curr_size + n, max_size ? max_size * 2 : 16));
		auto	r = this->end();
		curr_size += n;
		return r;
	}
public:
	_dynamic_array(size_t n = 0)		: B(n <= N ? (T*)&space : allocate<T>(n), n, max(n, N)) {}
	_dynamic_array(_dynamic_array &&b)	: B(b.is_small() ? (T*)space : exchange(b.p, nullptr), b.curr_size, b.max_size) {
		if (b.is_small())
			move_n(b.p, p, curr_size);
		b.curr_size = b.max_size = 0;
	}
	~_dynamic_array()	{
		if (!is_small())
			deallocate(p, max_size);
	}
	void reset() {
		destruct(p, curr_size);
		curr_size	= 0;
		if (!is_small()) {
			deallocate(p, max_size);
			p			= (T*)&space;
			max_size	= N;
		}
	}
	range<T*>	detach()	{
		auto	n = curr_size;
		if (is_small()) {
			T	*r = allocate<T>(n);
			move_new_n((T*)&space, r, n);
			curr_size	= 0;
			return {r, r + n};
		} else {
			T	*r = exchange(p, (T*)&space);
			curr_size	= 0;
			max_size	= N;
			return {r, r + n};
		}
	}
	friend void swap(_dynamic_array &a, _dynamic_array &b) {
		bool	asmall = a.is_small();
		bool	bsmall = b.is_small();

		if (asmall && bsmall) {
			swap(a.space, b.space);
			swap(a.curr_size, b.curr_size);

		} else {
			swap(a.p, b.p);
			swap(a.curr_size, b.curr_size);
			swap(a.max_size, b.max_size);

			if (asmall) {
				b.p		= (T*)&b.space;
				move_n(a.p, b.p, a.curr_size);
			} else if (bsmall) {
				a.p		= (T*)&a.space;
				move_n(b.p, a.p, b.curr_size);
			}
		}
	}
};

template<typename T> class _dynamic_array<T, 0> : public _ptr_max_array<T> {
	typedef _ptr_max_array<T> B;
protected:
	using B::p; using B::curr_size; using B::max_size;
	void	_set_size(size_t n)	{ curr_size = n; }
	void	_reserve(size_t n) {
		p = reallocate_move(unconst(p), curr_size, n);
		max_size = n;
	}
	T*		_expand(size_t n) {
		if (curr_size + n > max_size)
			_reserve(max(curr_size + n, max_size ? max_size * 2 : 16));
		auto	r = this->end();
		curr_size += n;
		return r;
	}
public:
	constexpr _dynamic_array()	{}
	_dynamic_array(size_t n) 			: B(n == 0 ? nullptr : allocate<T>(n), n, n)			{}
	_dynamic_array(_dynamic_array &&b) 	: B(exchange(b.p, nullptr), b.curr_size, b.max_size)	{ b.curr_size = b.max_size = 0; }
	~_dynamic_array()					{ deallocate(p, max_size); }

	void		reset() {
		if (p) {
			destruct(p, curr_size);
			deallocate(p, max_size);
			max_size	= curr_size = 0;
			p			= 0;
		}
	}
	range<T*>	detach() {
		auto	n	= curr_size;
		T		*r	= exchange(p, nullptr);
		max_size	= curr_size = 0;
		return {r, r + n};
	}
	friend void swap(_dynamic_array &a, _dynamic_array &b) {
		raw_swap(a, b);
	}
};

template<typename T, size_t N = 0> class dynamic_array : public dynamic_mixout<_dynamic_array<T, N>, T> {
protected:
	typedef	_dynamic_array<T, N>	B0;
	typedef	dynamic_mixout<B0, T>	B;
	using	B0::p;
	using	B0::curr_size;

public:
	constexpr dynamic_array()			{}
	constexpr dynamic_array(_none&)		{}
	dynamic_array(dynamic_array&&)		= default;
	dynamic_array(const dynamic_array &c)								: B(c.curr_size)	{ copy_new_n(c.begin(), p, curr_size); }
	dynamic_array(size_t n)												: B(n)				{ fill_new_n(p, curr_size); }
	dynamic_array(initializer_list<T> c) 								: B(c.size())		{ copy_new_n(c.begin(), p, curr_size); }
	template<typename U, size_t M>	dynamic_array(U (&&c)[M])			: B(M)				{ copy_new_n(&c[0], p, curr_size); }
	template<typename...U>		dynamic_array(size_t n, const U&...u)	: B(n)				{ fill_new_n(p, curr_size, u...); }
	template<typename C, typename=enable_if_t<has_begin_v<C>>> 		dynamic_array(C &&c)			: B(num_elements(c))		{ using iso::begin; copy_new_n(begin(c), p, curr_size); }
	template<typename I, typename=enable_if_t<is_iterator_v<I>>>	dynamic_array(I first, I last)	: B(distance(first, last))	{ copy_new_n(first, p, curr_size); }
	template<typename R, typename=is_reader_t<R>>					dynamic_array(R &&r, size_t n)	: B(n)						{ read_new_n(r, p, n); }

	dynamic_array&					operator=(const _none&)						{ this->clear(); return *this; }
	dynamic_array&					operator=(dynamic_array &&b)				{ swap(*this, b); return *this; }
	dynamic_array&					operator=(const dynamic_array &b)			{ B::assign(b); return *this; }
	template<typename C>			dynamic_array&	operator=(C &&c)			{ B::assign(c); return *this; }
	template<typename U, size_t M>	dynamic_array&	operator=(U (&&c)[M])		{ B::assign(make_move_iterator(&c[0]), make_move_iterator(&c[M])); return *this; }

	operator			sized_placement()	{ return {this->_expand(1), sizeof(T)}; }

	dynamic_array&		reserve(size_t n) {
		if (n > B::capacity())
			B::_reserve(n);
		return *this;
	}

	//const T&	operator[](intptr_t i) const	{ return B::at(i); }
	T&			operator[](intptr_t i)		{ return B::at(i); }


	friend void 		swap(dynamic_array &a, dynamic_array &b) { swap(static_cast<B0&>(a), static_cast<B0&>(b)); }
};

//-----------------------------------------------------------------------------
//	pair of dynamic_arrays (share memory from each end)
//-----------------------------------------------------------------------------

template<typename A, typename B> struct pair<dynamic_array<A>, dynamic_array<B>> {
	struct S {
		enum { ALIGNAB = max(alignof(A), alignof(B)) };
		A		*pa;
		B		*pb;
		size_t	sizea, sizeb;
		void	_reserve(size_t na, size_t nb) {
			auto	size = align(na * sizeof(A) + nb * sizeof(B), alignof(B));
			if (aligned_resize(pa, size, ALIGNAB)) {
				B	*pb1	= (B*)((char*)pa + size);
				move_new_n(pb - sizeb, pb1 - sizeb, sizeb);
				pb	= pb1;
			} else {
				A	*pa1	= (A*)aligned_alloc(size, ALIGNAB);
				B	*pb1	= (B*)((char*)pa1 + size);
				move_new_n(pa, pa1, sizea);
				move_new_n(pb - sizeb, pb1 - sizeb, sizeb);
				aligned_free(pa);
				pa	= pa1;
				pb	= pb1;
			}
		}
		S() : pa(nullptr), pb(nullptr), sizea(0), sizeb(0) {}
		~S() { aligned_free(pa); }
	};

	struct A1 : S {
		using S::sizea; using S::sizeb; using S::pa; using S::pb;
		void	_set_size(size_t n)			{ sizea = n; }
		A*		_expand(size_t n) {
			if ((char*)(pa + sizea + n) > (char*)(pb - sizeb))
				_reserve(max(sizea + n, sizea * 2, 16), sizeb * 2);
			return pa + exchange(sizea, sizea + n);
		}
		size_t			capacity()	const	{ return (A*)(pb - sizeb) - pa; }
		size_t			size()		const	{ return sizea; }
		const A*		begin()		const	{ return pa; }
		const A*		end()		const	{ return pa + sizea; }
		A*				begin()				{ return pa; }
		A*				end()				{ return pa + sizea; }
		constexpr operator	A*()	const	{ return pa; }
	};

	struct B1 : S {
		using S::sizea; using S::sizeb; using S::pa; using S::pb;
		typedef reverse_iterator<B*>		iterator;
		typedef reverse_iterator<const B*>	const_iterator;
		void		_set_size(size_t n)		{ sizeb = n; }
		iterator	_expand(size_t n) {
			if ((char*)(pa + sizea) > (char*)(pb - sizeb - n))
				_reserve(sizea * 2, max(sizeb + n, sizeb * 2, 16));
			return pb - exchange(sizeb, sizeb + n) - 1;
		}
		size_t			capacity()	const	{ return pb - (B*)(pa + sizea); }
		size_t			size()		const	{ return sizeb; }
		const_iterator	begin()		const	{ return pb - 1; }
		const_iterator	end()		const	{ return pb - sizeb - 1; }
		iterator		begin()				{ return pb - 1; }
		iterator		end()				{ return pb - sizeb - 1; }
		const B&	operator[](intptr_t i)	const	{ return pb[~i]; }
		B&			operator[](intptr_t i)			{ return pb[~i]; }
	};
	union {
		S						s;
		dynamic_mixout<A1,A>	a;
		dynamic_mixout<B1,B>	b;
	};

	pair()	: s() {}
	template<typename AP, typename BP> pair(AP &&_a, BP &&_b)		: s() { a = _a; b = _b; }
	pair(const dynamic_array<A> &_a, const dynamic_array<B> &_b)	: s() { a = _a; b = _b; }
	~pair()	{ a.clear(); b.clear(); s.~S(); }
};

//-----------------------------------------------------------------------------
//	class dynamic_array_de (double ended)
//-----------------------------------------------------------------------------

template<typename T> class _dynamic_array_de : public _ptr_max_array<T> {
	typedef _ptr_max_array<T>	B;
protected:
	using B::p; using B::curr_size; using B::max_size;
	size_t	offset;

	size_t	wrap(size_t i)		const { size_t j = i + offset; return j < max_size ? j : j - max_size; }
	size_t	unwrap(size_t i)	const { return i < offset ? i + max_size - offset : i - offset; }

	void	_reserve(size_t i) {
		T		*p0	= p;
		size_t	o	= offset;
		size_t	m	= curr_size;

		if (o == 0) {
			m		= 0;
		} else if (curr_size + o > max_size) {
			m		= max_size - o;
			offset	= i - m;
		}
		if (resize(p0, max_size, i)) {
			move_n(p0 + o, p0 + offset, m);

		} else {
			p = allocate<T>(i);
			move_new_n(p0, p, curr_size - m);
			move_new_n(p0 + o, p + offset, m);
			deallocate(p0, max_size);
		}
		max_size = i;
	}
	auto	_expand(size_t n) {
		if (curr_size + n > max_size)
			_reserve(max(curr_size + n, max_size ? max_size * 2 : 16));
		curr_size += n;
		return end() - n;
	}
	auto	_expand_front(size_t n) {
		if (curr_size + n > max_size)
			_reserve(max(curr_size + n, max_size ? max_size * 2 : 16));
		curr_size += n;
		offset = (offset < n ? max_size : offset) - n;
		return begin();
	}
	void	_set_size(size_t n) {
		curr_size = n;
	}
	void	_move_front(size_t n) {
		offset		+= n; 
		curr_size	-= n;
	}
public:
	typedef wrap_iterator<T*>			iterator;
	typedef wrap_iterator<const T*>		const_iterator;

	_dynamic_array_de() : offset(0)				{}
	_dynamic_array_de(_dynamic_array_de &&c)	: B(exchange(c.p, nullptr), c.curr_size, c.max_size), offset(c.offset) {}
	_dynamic_array_de& operator=(_dynamic_array_de &&c) { swap(*this, c); return *this; }
	~_dynamic_array_de()								{ deallocate(p, max_size); }
	
	void			reset() {
		destruct(begin(), curr_size);
		deallocate(p, max_size);
		p = nullptr;
		max_size = curr_size = offset = 0;
	}

	int				index_of(const T *e)	const	{ return e ? unwrap(e - p) : -1;}
	int				index_of(const T &e)	const	{ return unwrap(&e - p); }

	const_iterator	begin()					const	{ return {p + offset, p, p + max_size}; }
	const_iterator	end()					const	{ return {p + wrap(curr_size), p, p + max_size}; }
	iterator		begin()							{ return {p + offset, p, p + max_size}; }
	iterator		end()							{ return {p + wrap(curr_size), p, p + max_size}; }

	friend void 	swap(_dynamic_array_de<T> &a, _dynamic_array_de<T> &b) { raw_swap(a, b); }
};

template<typename T> using dynamic_array_de = dynamic_de_mixout<_dynamic_array_de<T>, T>;

//-----------------------------------------------------------------------------
//	class dynamic_array_de1 - no iterator wrapping
//-----------------------------------------------------------------------------

template<typename T> class _dynamic_array_de1 : public _ptr_max_array<T> {
	typedef _ptr_max_array<T>	B;
protected:
	using B::p; using B::curr_size; using B::max_size;
	size_t	offset;

	void	_reserve(size_t i) {
		if (!resize(p, max_size, i)) {
			T	*p0	= exchange(p, allocate<T>(i));
			move_new_n(p0 + offset, p + offset, curr_size);
			deallocate(p0, max_size);
		}
		max_size = i;
	}
	auto	_expand(size_t n) {
		if (offset + curr_size + n > max_size)
			_reserve(max(curr_size + n, max_size ? max_size * 2 : 16));
		curr_size += n;
		return end() - n;
	}
	auto	_expand_front(size_t n) {
		size_t	size0	= exchange(curr_size, curr_size + n);

		if (offset > n) {
			offset -= n;

		} else if (max_size > curr_size * 2) {
			size_t	offset0	= exchange(offset, (max_size - curr_size) / 2);
			move_new_check_n(p + offset0,	p + offset + n, size0);

		} else {
			size_t	max0	= exchange(max_size, max(curr_size * 2, max(max_size, 16)));
			size_t	offset0	= exchange(offset, (max_size - curr_size) / 2);
			T		*p0		= exchange(p, allocate<T>(max_size));
			move_new_n(p0 + offset0, p + offset + n, size0);
			deallocate(p0, max0);
		}
		return begin();
	}
	void	_set_size(size_t n) {
		curr_size = n;
	}
	void	_move_front(size_t n) {
		offset		+= n; 
		curr_size	-= n;
	}
public:
	_dynamic_array_de1()						: offset(0)	{}
	_dynamic_array_de1(_dynamic_array_de1 &&c)	: B(exchange(c.p, nullptr), c.curr_size, c.max_size), offset(c.offset) {}
	_dynamic_array_de1& operator=(_dynamic_array_de1 &&c)	{ swap(*this, c); return *this; }
	~_dynamic_array_de1()									{ deallocate(p, max_size); }

	void			reset() {
		destruct(begin(), curr_size);
		deallocate(exchange(p, nullptr), max_size);
		max_size = curr_size = offset = 0;
	}

	const T*		begin()					const	{ return p + offset; }
	const T*		end()					const	{ return p + offset + curr_size; }
	T*				begin()							{ return p + offset; }
	T*				end()							{ return p + offset + curr_size; }
	int				index_of(const T *e)	const	{ return e ? e - begin() : -1;}
	int				index_of(const T &e)	const	{ return &e - begin(); }

	friend void 	swap(_dynamic_array_de1<T> &a, _dynamic_array_de1<T> &b) { raw_swap(a, b); }
};

template<typename T> using dynamic_array_de1 = dynamic_de_mixout<_dynamic_array_de1<T>, T>;

//-----------------------------------------------------------------------------
//	class order_array - entries never get relocated
//-----------------------------------------------------------------------------

template<typename T, int MIN_ORDER, int MAX_ORDER> struct _order_array {
	typedef uint_bits_t<MAX_ORDER>	I;
	typedef	indexed_iterator<_order_array&, int_iterator<I>>		iterator;
	typedef	indexed_iterator<const _order_array&, int_iterator<I>>	const_iterator;

	I				curr_size;
	space_for<T>	order0[1 << MIN_ORDER];
	T*				orders[MAX_ORDER - MIN_ORDER];

	T*		_get(I i) const {
		int	o = highest_set_index(i);
		return o < MIN_ORDER
			? (T*)(order0 + i)
			: orders[o - MIN_ORDER] + i - bit<I>(o);
	}
	auto	_expand(size_t n) {
		I	i = curr_size;
		n += i;
		for (int o = max(highest_set_index(i), MIN_ORDER); bit<I>(o) < n; ++o) {
			if (!orders[o - MIN_ORDER])
				orders[o - MIN_ORDER] = allocate<T>(bit<I>(o));
		}
		curr_size = I(n);
		return iterator(*this, i);
	}
	void	_set_size(size_t n)	{
		curr_size = n;
	}
	T*		_expand() {
		I i = curr_size++;
		if (is_pow2(i)) {
			int	o = highest_set_index(i);
			if (o >= MIN_ORDER && !orders[o - MIN_ORDER])
				orders[o - MIN_ORDER] = allocate<T>(bit<I>(o));
		}
		return _get(i);
	}
	int		_index_of(const T *e) const	{
		if (e >= (T*)order0 && e < (T*)iso::end(order0))
			return e - (T*)order0;

		I	n = bit<I>(MIN_ORDER);
		for (auto p : orders) {
			if (!p)
				break;
			if (e >= p && e < p + n)
				return e - p + n;
			n *= 2;
		}
		return -1;
	}
public:
	_order_array() : curr_size(0)	{ iso::clear(orders); }
	~_order_array() {
		I	n = bit<I>(MIN_ORDER);
		for (auto p : orders) {
			deallocate(p, n);
			n *= 2;
		}
	}

	const_iterator	begin()			const	{ return {*this, 0}; }
	const_iterator	end()			const	{ return {*this, curr_size}; }
	iterator		begin()					{ return {*this, 0}; }
	iterator		end()					{ return {*this, curr_size}; }
	size_t			size()			const	{ return curr_size; }

	T		&operator[](I i)				{ return *_get(i); }
	const T	&operator[](I i)		const	{ return *_get(i); }
	int		index_of(const T *e)	const	{ return e ? _index_of(e) : -1; }
	int		index_of(const T &e)	const	{ return _index_of(&e); }
};

template<typename T, int MIN_ORDER = 2, int MAX_ORDER = 32> using order_array = dynamic_mixout<_order_array<T, MIN_ORDER, MAX_ORDER>, T>;

//-----------------------------------------------------------------------------
//	hierarchy_traverser
//-----------------------------------------------------------------------------

template<typename C> struct hierarchy_traverser {
	typedef iterator_t<C>	I;
	struct entry {
		C		c;
		I		i, e;
		bool	started;
		template<typename C2> entry(C2 &&_c) : c(forward<C2>(_c)), i(c.begin()), e(c.end()), started(false) {}
		bool	next() {
			if (!started) {
				started = true;
				return false;
			}
			return ++i == e;
		}
	};
	dynamic_array<entry>	stack;

	bool		next()	{
		while (stack.back().next()) {
			stack.pop_back();
			if (stack.empty())
				return true;
		}
		return false;
	}

	struct iterator {
		hierarchy_traverser	*h;
		iterator(hierarchy_traverser *_h) : h(_h) {}
		decltype(auto)	operator*()						const	{ return *h->stack.back().i; }
		iterator&		operator++()							{ if (h->next()) h = 0; return *this; }
		bool			operator==(const iterator &b)	const	{ return h == b.h; }
		bool			operator!=(const iterator &b)	const	{ return h != b.h; }
	};

	template<typename C2> hierarchy_traverser(C2 &&c) {
		stack.push_back(forward<C2>(c));
		stack.back().started = true;
	}
	iterator	begin() { return this; }
	iterator	end()	{ return 0; }

	template<typename C2> void push(C2 &&c) {
		stack.push_back(forward<C2>(c));
	}
	const C&	top() const {
		return stack.back().c;
	}
};

template<typename C> hierarchy_traverser<typename T_noref<C>::type> make_hierarchy_traverser(C &&c) { return forward<C>(c); }

}//namespace iso

#endif //ARRAY_H
