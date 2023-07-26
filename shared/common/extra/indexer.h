#ifndef INDEXER_H
#define INDEXER_H

#include "base/algorithm.h"
#include "base/hash.h"

namespace iso {

//-----------------------------------------------------------------------------
//	double_index_iterator/ed - maintains reverse mapping
//-----------------------------------------------------------------------------

template<typename R, typename C> class double_index_container {
	R		r;
	C		c;
	typedef iterator_t<C>	I;
public:
	class element : comparisons<element> {
		typedef it_element_t<I>	X;
		R		&r;
		C		&c;
		X		&x;
	public:
		element(R &r, C &c, X &x) : r(r), c(c), x(x) 	{}
		operator auto()	const	{ return get(x);	}

		element&	operator=(const element &b) {
			r[b.x]	= index_of(c, x);
			x		= b.x;
			return *this;
		}
		friend void swap(element &a, element &b) {
			swap(a.x, b.x);
			swap(a.r[a.x], b.r[b.x]);
		}
		friend int	compare(const element &a, const element &b) {
			return simple_compare(get(*a.i), get(*b.i));
		}
		template<typename C2> friend indexed_element<C2&, element> deindex(C2 &c, element &&x)	{ return {c, x}; }
	};

	class iterator : public iterator_wrapper<iterator, I> {
		typedef iterator_wrapper<iterator, I> B;
		using B::i;
		R	&r;
		C	&c;
	public:
		iterator(R &r, C &c, I i) : B(i), r(r), c(c)	{}
		iterator&	operator=(const iterator &b)		{ i = b.i; return *this; }
		element		operator*()				const	{ return {r, c, *i};	}
		element		operator->()			const	{ return {r, c, *i};	}
		element		operator[](intptr_t j)	const	{ return {r, c, i[j]}; }
		iterator	operator+(intptr_t j)	const	{ return {r, c, i + j}; }
		iterator	operator-(intptr_t j)	const	{ return {r, c, i - j}; }

		friend auto	operator-(const iterator &a, const iterator &b) { return a.i - b.i; }
	};

	double_index_container(R &&r, C &&c) : r(forward<R>(r)), c(forward<C>(c))	{}
	iterator	begin()				const	{ using iso::begin; return {r, c, begin(c)}; }
	iterator	end()				const	{ using iso::end; return {r, c, end(c)}; }
	auto		size()				const	{ return c.size(); }
	void		pop_back()					{ c.pop_back(); }
	element		pop_back_value()			{ return c.pop_back_value(); }
	element		operator[](int j)	const	{ return {r, c, c[j]}; }
};

template<typename R, typename C> force_inline double_index_container<R,C> make_double_index_container(R &&r, C &&c) {
	return double_index_container<R,C>(forward<R>(r), forward<C>(c));
}
template<typename R, typename I> force_inline typename double_index_container<R, range<I>>::iterator make_double_index_iterator(R &&r, I i) {
	return typename double_index_container<R,range<I> >::iterator(r, i);
}

//-----------------------------------------------------------------------------
//	Indexer - generate indices
//-----------------------------------------------------------------------------

template<typename I> class Indexer {
	I		*links;
	I		*indices;
	I		*rev_indices;
	int		num_indices;
	int		num_unique;

	template<typename C, typename P> void	FindLink(const C &values, int i, I ix, P pred);
public:
	Indexer(int _num_indices) : num_indices(_num_indices), num_unique(0)	{
		links		= new I[num_indices];
		indices		= new I[num_indices];
		rev_indices	= new I[num_indices];
		memset(indices, 0, sizeof(I) * num_indices);
		memset(rev_indices, 0, sizeof(I) * num_indices);
	}
	~Indexer() {
		delete[] links;
		delete[] indices;
		delete[] rev_indices;
	}

	range<const I*>	Indices()		const	{ return make_range_n((const I*)indices, num_indices);		}
	range<const I*>	RevIndices()	const	{ return make_range_n((const I*)rev_indices, num_unique);	}
	const I&	Index(int i)		const	{ return indices[i];	}
	const I&	RevIndex(int i)		const	{ return rev_indices[i];}
	int			NumUnique()			const	{ return num_unique;	}
	int			NumIndices()		const	{ return num_indices;	}

	void		SetIndex(I i, I j)			{ rev_indices[indices[i] = j] = i; }
	void		SetNext(I i)				{ num_unique = i; }

	template<typename I2> void SetIndices(I2 indices2, int _num_unique) {
		num_unique	= _num_unique;
		for (int i = num_indices; i--;)
			rev_indices[indices[i] = indices2[i]] = i;
	}

	template<typename C, typename P>	int	Process(const C &values, P pred = P());
	template<typename C, typename P>	int ProcessFirst(const C &values, P pred = P());
};

template<typename I> template<typename C, typename P>
void Indexer<I>::FindLink(const C &values, int i, I ix, P pred) {
	typedef it_element_t<C>	E;
	E	v		= values[i];
	I	pix;

	do {
		if (pred(values[rev_indices[ix]], v)) {
			indices[i] = ix;
			return;
		}
		pix = ix;
	} while (ix	= links[ix]);

	links[pix]		= ix = num_unique++;
	links[ix]		= 0;
	indices[i]		= ix;
	rev_indices[ix]	= i;
}

template<typename I> template<typename C, typename P>
int Indexer<I>::Process(const C &values, P pred) {
	if (num_unique == 0)
		return ProcessFirst(values, pred);

	for (int i = 0; i < num_unique; i++)
		links[i] = I(~0);

	for (int i = 0; i < num_indices; i++) {
		int		ix	= indices[i];
		if (links[ix] == I(~0))
			links[ix] = 0;
		else
			FindLink(values, i, ix, pred);
	}
	return num_unique;
}

template<typename I> template<typename C, typename P>
int Indexer<I>::ProcessFirst(const C &values, P pred) {
	typedef it_element_t<C>	E;
	hash_map<E, I>	m;
	for (int i = 0; i < num_indices; i++)
		links[i] = 0;
	for (int i = 0; i < num_indices; i++) {
		if (const I *p = m.check(values[i])) {
			FindLink(values, i, *p, pred);
		} else {
			int			ix	= num_unique++;
			m[values[i]]	= indices[i] = ix;
			rev_indices[ix]	= i;
		}
	}
	return num_unique;
}

} // namespace iso

#endif // INDEXER_H
