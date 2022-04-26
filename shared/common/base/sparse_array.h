#ifndef SPARSE_ARRAY_H
#define SPARSE_ARRAY_H

#include "base/defs.h"
#include "base/bits.h"
#include "base/array.h"

namespace iso {

//-----------------------------------------------------------------------------
//	sparse_array	- O(1) ops, including iteration
// A = underlying array
// S = type of index
//-----------------------------------------------------------------------------

template<typename A, typename S = uint8> class sparse_array_base : public A {
protected:
	typedef S	dense_index;
	S*		to_dense	= 0;
	size_t	first_dense	= 0;
	size_t	num_dense	= 0;

	auto	new_entry(size_t i) {
		size_t	s		= A::size();
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
		auto	*e		= A::expand();
		to_dense[i - first_dense] = S(s);
		return e;
	}

	iterator_t<const A>	get_possible_entry(size_t i) const {
		if (i >= first_dense && i - first_dense < num_dense) {
			auto	j = to_dense[i - first_dense];
			if (j < A::size())
				return A::begin() + j;
		}
		return nullptr;
	}
public:
	sparse_array_base()				{}
	sparse_array_base(size_t u)		{ to_dense = allocate<S>(num_dense = u); }
	sparse_array_base(sparse_array_base&& b) : A((A&&)b), to_dense(b.to_dense), first_dense(b.first_dense), num_dense(b.num_dense) { b.to_dense = nullptr; }
	~sparse_array_base()			{ deallocate(to_dense, num_dense); }
	
	sparse_array_base& operator=(sparse_array_base&& b) = default;

	void	resize(size_t u)		{ ISO_ASSERT(!to_dense); to_dense = allocate<S>(num_dense = u); }
};

template<typename T, typename I> struct sparse_element {
	I	i;
	T	t;
	constexpr operator T*()						{ return __builtin_addressof(t); }
	constexpr operator const T*()		const	{ return __builtin_addressof(t); }
	constexpr T*		operator->()			{ return __builtin_addressof(t); }
	constexpr const T*	operator->()	const	{ return __builtin_addressof(t); }
	constexpr auto	index()				const	{ return i; }
	sparse_element()					= default;
	sparse_element(sparse_element &&e)	= default;
};

template<typename T, typename I = uint32, typename S = uint8> class sparse_array : public sparse_array_base<dynamic_array<sparse_element<T, I>>, S> {
	typedef sparse_element<T, I>					E;
	typedef sparse_array_base<dynamic_array<E>, S>	B;

	const E*	has_entry(I i) const {
		if (auto e = B::get_possible_entry(i)) {
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
	optional<T&>	get(const I &i) const {
		if (auto *e = has_entry(i))
			return unconst(e)->t;
		return none;
	}
	T&				put(const I &i) {
		if (auto e = has_entry(i))
			return unconst(e)->t;
		E *e	= B::new_entry(i);
		e->i	= i;
		return e->t;
	}
	template<typename U> T& put(const I &i, U &&u) {
		return put(i) = forward<U>(u);
	}

	bool			remove(const I &i) {
		if (!has_entry(i))
			return false;
		auto	s = B::to_dense[i];
		B::to_dense[erase_unordered(B::begin() + s)->i] = s;
		return true;
	}

	static auto	index_of(const E &e)	{ return e.i; }

	optional<T&>	operator[](const I &i)		const				{ return get(i); }
	putter<sparse_array, I, optional<T&>> operator[](const I &i)	{ return {*this, i}; }
};

//-----------------------------------------------------------------------------
//	sparse_set	- O(1) ops, including iteration
//-----------------------------------------------------------------------------

template<typename T> T sparse(const T &t) {
	return t;
}

template<typename T> using sparse_t = decltype(sparse(declval<T>()));

template<typename K, typename S = uint8> class sparse_set : public sparse_array_base<dynamic_array<sparse_t<K>>, S> {
	typedef sparse_t<K>	I;
	typedef sparse_array_base<dynamic_array<I>, S> B;

	bool	has_entry(I i) const {
		if (auto e = B::get_possible_entry(i))
			return *e == i;
		return false;
	}
public:
	sparse_set(size_t u) : B(u) {}

	bool	check(const K &k) const {
		return has_entry(sparse(k));
	}
	int		get_index(const K &k) const {
		I	i = sparse(k);
		return has_entry(i) ? B::to_dense[i] : -1;
	}
	void	insert(const K &k) {
		I	i = sparse(k);
		if (!has_entry(i))
			*this->new_entry(i) = i;
	}
	bool	check_insert(const K &k) {
		I	i = sparse(k);
		return has_entry(i) || ((*this->new_entry(i) = i), false);
	}
	bool	remove(const K &k) {
		I	i = sparse(k);
		if (!has_entry(i))
			return false;
		auto	s = B::to_dense[i];
		B::to_dense[*erase_unordered(B::begin() + s)] = s;
		return true;
	}
};

template<typename T, typename K = uint32, typename S = uint8> class sparse_map : public sparse_array<T, sparse_t<K>, S> {
	typedef sparse_t<K>	I;
	typedef sparse_array<T, I, S> B;
public:
	sparse_map(size_t u) : B(u) {}

	T*				check(const K &k)			const	{ return B::check(sparse(k)); }
	bool			remove(const K &k)					{ return B::remove(sparse(k)); }
	optional<T&>	operator[](const K &k)		const	{ return B::operator[](sparse(k)); }
	auto			operator[](const K &k)				{ return B::operator[](sparse(k)); }
};

} // namespace iso

#endif // SPARSE_ARRAY_H
