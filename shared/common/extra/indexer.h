#ifndef INDEXER_H
#define INDEXER_H

#include "base/algorithm.h"
#include "base/hash.h"

namespace iso {

//-----------------------------------------------------------------------------
//	index_iterator/ed	- only indices are swapped
//-----------------------------------------------------------------------------

template<typename C, typename CI> class index_container {
	typedef iterator_t<CI>	I;
	const C		&c;
	CI			&i;
public:
	class element {
		typedef typename iterator_traits<C>::element	T;
		typedef typename iterator_traits<I>::reference	X;
		const C	&c;
		X		i;
	public:
		element(const C &_c, const X _i) : c(_c), i(_i)	{}
		operator	T&()							const	{ return c[i];	}
		T&			operator*()						const	{ return c[i];	}
		T*			operator->()					const	{ return &c[i];	}
		X			index()							const	{ return i; }

		auto		operator==(const element &b)	const	{ return c[i] == *b; }
		auto		operator!=(const element &b)	const	{ return c[i] != *b; }
		auto		operator< (const element &b)	const	{ return c[i] <  *b; }
		auto		operator<=(const element &b)	const	{ return c[i] <= *b; }
		auto		operator>=(const element &b)	const	{ return c[i] >= *b; }
		auto		operator> (const element &b)	const	{ return c[i] >  *b; }

		element&	operator=(const element &b)				{ i = b.index(); return *this; }
		friend void swap(element a, element b)				{ swap(a.i, b.i); }
	};

	class iterator {
		const C	&c;
		I		i;
	public:
		typedef typename index_container::element					element, reference;
		typedef typename iterator_traits<I>::iterator_category		iterator_category;

		iterator(const C &_c, const I &_i) : c(_c), i(_i)	{}
		iterator&	operator=(const iterator &b)			{ i = b.i; return *this; }

		element		operator*()						const	{ return element(c, *i);	}
		element		operator->()					const	{ return element(c, *i);	}
		element		operator[](int j)				const	{ return element(c, i[j]);	}

		bool		operator==(const iterator &b)	const	{ return i == b.i; }
		bool		operator!=(const iterator &b)	const	{ return i != b.i; }
		bool		operator< (const iterator &b)	const	{ return i <  b.i; }
		bool		operator<=(const iterator &b)	const	{ return i <= b.i; }
		bool		operator>=(const iterator &b)	const	{ return i >= b.i; }
		bool		operator> (const iterator &b)	const	{ return i >  b.i; }

		iterator&	operator+=(intptr_t j)			{ i += j; return *this; }
		iterator&	operator-=(intptr_t j)			{ i -= j; return *this; }
		iterator&	operator++()					{ ++i; return *this;	}
		iterator&	operator--()					{ --i; return *this;	}
		iterator	operator++(int)					{ iterator t(*this); ++i; return t; }
		iterator	operator--(int)					{ iterator t(*this); --i; return t; }
		iterator	operator+(intptr_t j)	const	{ return iterator(c, i + j); }
		iterator	operator-(intptr_t j)	const	{ return iterator(c, i - j); }

		typename iterator_traits<I>::reference index() const	{ return *i; }

		friend auto	operator-(const iterator &a, const iterator &b) {
			return a.i - b.i;
		}
	};

	typedef	element		reference;
	typedef	iterator	const_iterator;

	index_container(const C &_c, CI &_i) : c(_c), i(_i)	{}

	element				operator[](int j)	const	{ return element(c, i[j]); }
	int					size()				const	{ return i.size(); }
	iterator			begin()				const	{ return iterator(c, i.begin()); }
	iterator			end()				const	{ return iterator(c, i.end()); }

	void				pop_back()					{ i.pop_back(); }
	element				pop_back_value()			{ return i.pop_back_value(); }
};

template<typename C, typename CI> force_inline index_container<C,CI>	make_index_container(const C &c, CI &ci) {
	return index_container<C,CI>(c, ci);
}
template<typename C, typename I> force_inline typename index_container<C,range<I> >::iterator make_index_iterator(const C &c, I i) {
	return typename index_container<C,range<I> >::iterator(c, i);
}

//-----------------------------------------------------------------------------
//	double_index_iterator/ed - maintains reverse mapping
//-----------------------------------------------------------------------------

template<typename R, typename CI> class double_index_container {
	R		&r;
	CI		&c;
	typedef iterator_t<CI>	I;
public:
	class element {
		typedef typename iterator_traits<I>::element	X;
		R		&r;
		CI		&c;
		X		&i;
	public:
		element(R &_r, CI &_c, X &_i) : r(_r), c(_c), i(_i) 	{}
		operator X()	const	{ return i;	}

		element&	operator=(const element &b) {
			r[b.i]	= index_of(c, &i);
			i		= b.i;
			return *this;
		}
		friend void swap(element &a, element &b) {
			swap(a.i, b.i);
			swap(a.r[a.i], b.r[b.i]);
		}
	};

	class iterator {
		R	&r;
		CI	&c;
		I	i;
	public:
		typedef typename double_index_container::element		element, reference;
		typedef typename iterator_traits<I>::iterator_category	iterator_category;

		iterator(R &_r, CI &_c, I _i) : r(_r), c(_c), i(_i)	{}
		iterator& operator=(const iterator &b)		{ i = b.i; return *this; }

		element		operator*()						const	{ return element(r, c, *i);	}
		element		operator->()					const	{ return element(r, c, *i);	}
		element		operator[](int j)				const	{ return element(r, c, i[j]);	}
		bool		operator==(const iterator &b)	const	{ return i == b.i;	}
		bool		operator!=(const iterator &b)	const	{ return i != b.i;	}
		bool		operator< (const iterator &b)	const	{ return i <  b.i;	}
		bool		operator<=(const iterator &b)	const	{ return i <= b.i;	}
		bool		operator>=(const iterator &b)	const	{ return i >= b.i;	}
		bool		operator> (const iterator &b)	const	{ return i >  b.i;	}

		iterator&	operator+=(intptr_t j)			{ i += j; return *this; }
		iterator&	operator-=(intptr_t j)			{ i -= j; return *this; }
		iterator&	operator++()					{ ++i; return *this;	}
		iterator&	operator--()					{ --i; return *this;	}
		iterator	operator++(int)					{ iterator t(*this); ++i; return t; }
		iterator	operator--(int)					{ iterator t(*this); --i; return t; }
		iterator	operator+(intptr_t j)	const	{ return iterator(r, c, i + j); }
		iterator	operator-(intptr_t j)	const	{ return iterator(r, c, i - j); }

		friend auto	operator-(const iterator &a, const iterator &b) {
			return a.i - b.i;
		}
	};

	typedef	iterator	const_iterator;
	typedef element		reference;

	double_index_container(R &_r, CI &_c) : r(_r), c(_c)	{}
	iterator	begin()				const	{ return iterator(r, c, iso::begin(c)); }
	iterator	end()				const	{ return iterator(r, c, iso::end(c)); }
	void		pop_back()					{ c.pop_back(); }
	element		pop_back_value()			{ return c.pop_back_value(); }
};

template<typename R, typename CI> force_inline double_index_container<R,CI> make_double_index_container(R &r, CI &c) {
	return double_index_container<R,CI>(r, c);
}
template<typename R, typename I> force_inline typename double_index_container<R,range<I> >::iterator make_double_index_iterator(R &r, I i) {
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
	typedef typename iterator_traits<C>::element	E;
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
	typedef typename iterator_traits<C>::element	E;
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
