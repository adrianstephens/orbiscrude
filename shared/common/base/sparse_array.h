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
	typedef S			dense_index;
	dynamic_array<S>	to_dense;

	auto	new_entry(size_t i) {
		size_t	s		= A::size();
		//ISO_CHEAPASSERT(i < to_dense.size() && S(s) == s);
		if (i >= to_dense.size()) {
			ISO_CHEAPASSERT(S(s) == s);
			to_dense.resize(i + 1);
		}
		auto	*e		= A::expand();
		to_dense[i]		= S(s);
		return e;
	}
public:
	sparse_array_base() {}
	sparse_array_base(size_t u) : to_dense(u) {}
	void	resize(size_t u)		{ to_dense.resize(u); }
	size_t	universe()		const	{ return to_dense.size(); }
};

template<typename T, typename I> struct sparse_element {
	I	i;
	T	t;
	constexpr operator T&()						{ return t; }
	constexpr operator const T&()		const	{ return t; }
	constexpr T*		operator->()			{ return &t; }
	constexpr const T*	operator->()	const	{ return &t; }
	constexpr auto	index()				const	{ return i; }
	sparse_element()					= default;
	sparse_element(sparse_element &&e)	= default;
	//	sparse_element(const sparse_element &e) {}//= default;//: i(e.i), t(e.t)	{}
};

template<typename T, typename I = uint32, typename S = uint8> class sparse_array : public sparse_array_base<dynamic_array<sparse_element<T, I>>, S> {
	typedef sparse_element<T, I>					E;
	typedef sparse_array_base<dynamic_array<E>, S>	B;

	const E*	has_entry(I i) const {
		//		return i < B::universe() && B::to_dense[i] < B::size() && this->at(B::to_dense[i]).i == i;
		if (i < B::universe()) {
			auto	j = B::to_dense[i];
			if (j < B::size()) {
				const E	*e = &this->at(j);
				if (e->i == i)
					return e;
			}
		}
		return nullptr;
	}
public:
	sparse_array() {}
	sparse_array(size_t u) : B(u) {}

	T*				check(const I &i)			const	{ if (auto *e = has_entry(i)) return &unconst(e)->t; return nullptr; }
	optional<T&>	get(const I &i)				const	{ if (auto *e = has_entry(i)) return unconst(e)->t; return none; }
	T&				put(const I &i)						{
		if (auto e = has_entry(i))
			return unconst(e)->t;
		E *e	= B::new_entry(i);
		e->i	= i;
		return e->t;
	}
	template<typename U> T& put(const I &i, U &&u)		{ return put(i) = forward<U>(u); }

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
template<typename T> struct sparse_type {
	typedef decltype(sparse(*(const T*)0))	type;
};

template<typename K, typename S = uint8> class sparse_set : public sparse_array_base<dynamic_array<typename sparse_type<K>::type>, S> {
	typedef typename sparse_type<K>::type	I;
	typedef sparse_array_base<dynamic_array<I>, S> B;

	bool	has_entry(I i) const {
		return i < B::to_dense.size() && B::to_dense[i] < B::size() && this->at(B::to_dense[i]) == i;
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

template<typename T, typename K = uint32, typename S = uint8> class sparse_map : public sparse_array<T, typename sparse_type<K>::type, S> {
	typedef typename sparse_type<K>::type	I;
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
