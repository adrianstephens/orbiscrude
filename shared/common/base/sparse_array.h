#ifndef SPARSE_ARRAY_H
#define SPARSE_ARRAY_H

#include "base/defs.h"
#include "base/bits.h"
#include "base/array.h"

namespace iso {

// function to make sparse index
template<typename T> T sparse(const T &t) { return t; }
template<typename T> using sparse_t = decltype(sparse(declval<T>()));

//-----------------------------------------------------------------------------
//	sparse_array	- O(1) ops, including iteration
// A = underlying array
// S = type of index
//-----------------------------------------------------------------------------

template<typename A, typename S = uint8> class sparse_array_base : public A {
protected:
	S*		to_dense	= 0;
	size_t	first_dense	= 0;
	size_t	num_dense	= 0;

	template<typename...U> auto&	new_entry(size_t i, U&&... u) {
		size_t	s	= A::size();
		ISO_CHEAPASSERT(S(s) == s);

		size_t	n	= num_dense;
		if (i < first_dense) {
			num_dense		= max(num_dense + first_dense - i, num_dense * 2);
			auto extra		= min(((num_dense - n) + (i - first_dense)) / 2, i);
			auto new_dense	= allocate<S>(num_dense);

			copy_n(to_dense, new_dense + (first_dense - i + extra), n);
			deallocate(to_dense, n);
			to_dense	= new_dense;
			first_dense = i - extra;

		} else if (i - first_dense >= n) {
			if (n == 0)
				first_dense = i;
			
			num_dense = max(i - first_dense + 1, num_dense * 2);
			auto new_dense = allocate<S>(num_dense);

			copy_n(to_dense, new_dense, n);
			deallocate(to_dense, n);
			to_dense = new_dense;
		}
		to_dense[i - first_dense] = S(s);
		return A::emplace_back(forward<U>(u)...);
	}

	iterator_t<const A>	get_possible_entry(size_t i) const {
		if (i >= first_dense && i - first_dense < num_dense) {
			auto	j = to_dense[i - first_dense];
			if (j < A::size())
				return A::begin() + j;
		}
		return nullptr;
	}

	//void	remove(iterator_t<A> e, size_t i) {
	//	A::erase_unordered(e);
	//	to_dense[i - first_dense] = A::index_of(e);
	//}

	void	fix(iterator_t<const A> e, size_t i) {
		to_dense[i - first_dense] = A::index_of(e);
	}

	S		dense_index(size_t i) const {
		return to_dense[i - first_dense];
	}

public:
	sparse_array_base()				{}
	sparse_array_base(size_t u)		{ to_dense = allocate<S>(num_dense = u); }
	sparse_array_base(sparse_array_base&& b) : A((A&&)b), to_dense(b.to_dense), first_dense(b.first_dense), num_dense(b.num_dense) { b.to_dense = nullptr; }
	~sparse_array_base()			{ deallocate(to_dense, num_dense); }

	sparse_array_base(const sparse_array_base& b) : A(b), first_dense(b.first_dense), num_dense(b.num_dense) {
		to_dense	= allocate<S>(num_dense);
		copy_n(b.to_dense, to_dense, num_dense);
	}

	sparse_array_base& operator=(sparse_array_base&& b) { swap(*this, b); return *this; }

	sparse_array_base& operator=(const sparse_array_base& b) {
		A::operator=(b);
		deallocate(to_dense, num_dense);
		first_dense	= b.num_dense;
		num_dense	= b.num_dense;
		to_dense	= allocate<S>(num_dense);
		copy_n(b.to_dense, to_dense, num_dense);
		return *this;
	}

	void	clear()					{ *this = {}; }
	void	resize(size_t u)		{ ISO_ASSERT(!to_dense); to_dense = allocate<S>(num_dense = u); }

	friend void	swap(sparse_array_base& a, sparse_array_base& b) {
		swap((A&)a, (A&)b);
		swap(a.to_dense, b.to_dense);
		swap(a.first_dense, b.first_dense);
		swap(a.num_dense, b.num_dense);
	}
};

template<typename T, typename I> struct sparse_element {
	I	i;
	T	t;
	constexpr operator T*()						{ return __builtin_addressof(t); }
	constexpr operator const T*()		const	{ return __builtin_addressof(t); }
	constexpr T*		operator->()			{ return __builtin_addressof(t); }
	constexpr const T*	operator->()	const	{ return __builtin_addressof(t); }
	constexpr auto	index()				const	{ return i; }
	template<typename...U> sparse_element(I i, U&&... u)	: i(i), t(forward<U>(u)...) {}
	sparse_element(const sparse_element &e)			= default;
	sparse_element(sparse_element &&e)				= default;
	sparse_element& operator=(sparse_element &&)	= default;
};

template<typename T, typename I = uint32, typename S = uint8> class sparse_array : public sparse_array_base<dynamic_array<sparse_element<T, I>>, S> {
	typedef sparse_element<T, I>					E;
	typedef sparse_array_base<dynamic_array<E>, S>	B;

	const E*	has_entry(I i) const {
		if (auto e = B::get_possible_entry((size_t)i)) {
			if (e->i == i)
				return e;
		}
		return nullptr;
	}
public:
	sparse_array() {}
	sparse_array(size_t u) : B(u) {}

	T*				check(const I &i) const {
		if (auto *e = has_entry(i))
			return &unconst(e)->t;
		return nullptr;
	}
	optional<const E&>	entry(const I &i) const {
		if (auto *e = has_entry(i))
			return *e;
		return none;
	}

	optional<const T&>	get(const I &i) const {
		if (auto *e = has_entry(i))
			return e->t;
		return none;
	}
	const T&		get_known(const I &i) const {
		return has_entry(i)->t;
	}
	optional<T&>	get(const I &i) {
		if (auto *e = has_entry(i))
			return unconst(e)->t;
		return none;
	}
	T&				put(const I &i) {
		if (auto e = has_entry(i))
			return unconst(e)->t;
		return B::new_entry((size_t)i, i).t;
	}
	template<typename U> T& put(const I &i, U &&u) {
		if (auto e = has_entry(i))
			return unconst(e)->t = forward<U>(u);
		return B::new_entry((size_t)i, i, forward<U>(u)).t;
	}
	template<typename... U> T& emplace(const I &i, U&&... u)	{
		if (auto e = has_entry(i))
			return unconst(e)->t = T(forward<U>(u)...);
		return B::new_entry((size_t)i, i, forward<U>(u)...).t;
	}
	bool			remove(const I &i) {
		if (auto e = has_entry(i)) {
			erase_unordered(unconst(e));
			B::fix(e, e->i);
			return true;
		}
		return false;
	}

	static auto	index_of(const E &e)	{ return e.i; }

	optional<const T&>	operator[](const I &i)		const			{ return get(i); }
	putter<sparse_array, I, optional<T&>> operator[](const I &i)	{ return {*this, i}; }
};

//-----------------------------------------------------------------------------
//	sparse_map
//-----------------------------------------------------------------------------

template<typename T, typename K = uint32, typename S = uint8> class sparse_map : public sparse_array<T, sparse_t<K>, S> {
	typedef sparse_array<T, sparse_t<K>, S> B;
public:
	using B::B;
	sparse_map(initializer_list<pair<K, T>> vals)	: B(vals.size() * 2)	{ for (auto &i : vals) B::put(sparse(i.a), i.b); }

	T*		check(const K &k)			const	{ return B::check(sparse(k)); }
	bool	remove(const K &k)					{ return B::remove(sparse(k)); }
	auto	operator[](const K &k)		const	{ return B::operator[](sparse(k)); }
	auto	operator[](const K &k)				{ return B::operator[](sparse(k)); }
};

//-----------------------------------------------------------------------------
//	sparse_set
//-----------------------------------------------------------------------------

template<typename K, typename S = uint8> class sparse_set : public sparse_array_base<dynamic_array<sparse_t<K>>, S> {
	typedef sparse_t<K>	I;
	typedef sparse_array_base<dynamic_array<I>, S> B;

	bool	has_entry(I i) const {
		if (auto e = B::get_possible_entry((size_t)i))
			return *e == i;
		return false;
	}
	void onion(const sparse_set &b) {
		for (auto i : b) {
			if (!has_entry(i))
				B::new_entry(i, i);
		}
	}
	void intersect(const sparse_set &b) {
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			if (!b.has_entry(*i))
				B::remove(i, *i);
		}
	}
	void difference(const sparse_set &b) {
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			if (b.has_entry(*i))
				B::remove(i, *i);
		}
	}
	void exclusive(const sparse_set &b) {
		for (auto i = b.begin(), e = b.end(); i != e; ++i) {
			auto	k = *i;
			if (has_entry(k))
				B::remove(i, k);
			else
				B::new_entry(k, k);
		}
	}
public:
	using B::B;
	sparse_set(initializer_list<K> keys)	: B(keys.size() * 2)	{ for (auto &i : keys) insert(i); }

	bool	count(const K &k) const {
		return has_entry(sparse(k));
	}
	int		get_index(const K &k) const {
		auto	i = sparse(k);
		return has_entry(i) ? B::dense_index(i) : -1;
	}
	void	insert(const K &k) {
		auto	i = sparse(k);
		if (!has_entry(i))
			B::new_entry(i, i);
	}
	void	insert(initializer_list<K> keys) {
		for (auto &i : keys)
			insert(i);
	}

	bool	check_insert(const K &k) {
		auto	i = sparse(k);
		return has_entry(i) || (new_entry(i, i), false);
	}
	bool	remove(const K &k) {
		auto	i = sparse(k);
		auto e = B::get_possible_entry(i);
		if (*e == i) {
			B::fix(e, i);
			return true;
		}
		return false;
	}

	_not<sparse_set>	operator~() const				{ return *this; }

	auto&		operator&=(const sparse_set &b)			{ intersect(b); return *this; }
	auto&		operator&=(const _not<sparse_set> &b)	{ difference(~b); return *this; }
	auto&		operator|=(const sparse_set &b)			{ onion(b); return *this; }
	auto&		operator^=(const sparse_set &b)			{ exclusive(b); return *this; }

	template<typename X> auto&		operator*=(const X &b)					{ return operator&=(b); }
	template<typename X> auto&		operator+=(const X &b)					{ return operator|=(b); }
	template<typename X> auto&		operator-=(const X &b)					{ return operator&=(~b); }

	template<typename X> friend auto operator&(sparse_set a, const X &b)	{ return a &= b; }
	template<typename X> friend auto operator|(sparse_set a, const X &b)	{ return a |= b; }
	template<typename X> friend auto operator^(sparse_set a, const X &b)	{ return a ^= b; }
	template<typename X> friend auto operator-(sparse_set a, const X &b)	{ return a -= b; }
};

template<typename K, typename S> struct _not<sparse_set<K, S>> {
	const sparse_set<K, S>	&t;
	_not(const sparse_set<K, S> &t) : t(t) {}
	bool	count(const K &k) const		{ return !t.count(k); }
};


} // namespace iso

#endif // SPARSE_ARRAY_H
