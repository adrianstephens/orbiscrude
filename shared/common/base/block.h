#ifndef BLOCK_H
#define BLOCK_H

#include "defs.h"
#include "deferred.h"
#include "interval.h"

namespace iso {

template<typename I> class iblock_buffer {
protected:
	I			buffer;
	void		move(intptr_t n)	{ buffer += n; }
public:
	constexpr int	pitch()		const	{ return 1; }
	constexpr I		begin()		const	{ return buffer; }
	constexpr operator	I()		const	{ return buffer; }
	constexpr intptr_t	offset(const I i) const	{ return i - buffer; }

	constexpr iblock_buffer(I buffer = I()) : buffer(buffer) {}
};

template<typename I, int N> class iblock_iterator;

template<typename I, int N> class iblock : public iblock<I, N - 1> {
protected:
	typedef iblock<I, N - 1>	B;
	struct temp;
	int32	stride;
	uint32	count;

public:
	typedef iblock_iterator<I, N>	iterator;

	iblock()	{}
	constexpr iblock(const B &b, int32 stride, uint32 count) : B(b), stride(stride), count(count)	{}

	constexpr uint32		size()					const	{ return count; }
	constexpr int32			pitch()					const	{ return stride; }
	constexpr iterator		begin()					const	{ return iterator(*this, stride); }
	constexpr iterator		end()					const	{ return begin() += (int)count; }
	constexpr uint32		max_size()				const	{ return max(count, B::max_size()); }
	constexpr B				operator[](int i)		const	{ return begin()[i]; }
	constexpr int			index_of(const I e)		const	{ return B::offset(e) / stride; }

	template<int N2> iblock<I, N2>&			get()			{ return *this; }
	template<int N2> const iblock<I, N2>&	get()	const	{ return *this; }

	void		cut(int i, int n) {
		B::move(i * intptr_t(stride));
		count	= count <= i ? 0 : n < 0 ? max(n + count - i, 0) : min(n, count - i);
	}
	void		cut(int i) {
		B::move(i * intptr_t(stride));
		count	= count <= i ? 0 : count - i;
	}
	void		skip(int i) {
		stride *= i;
		count /= i;
	}

	template<int N2> constexpr uint32	size()				const	{ return iblock<I, N2>::size(); }
	template<int N2> constexpr int32	pitch()				const	{ return iblock<I, N2>::pitch(); }
	template<int N2> constexpr temp		sub(int i, int n)	const	{ return temp(*this).template sub<N2>(i, n); }
	template<int N2> constexpr temp		slice(int i, int n)	const	{ return temp(*this).template slice<N2>(i, n); }
	template<int N2> constexpr temp		slice(int i)		const	{ return temp(*this).template slice<N2>(i); }
	template<int N2> constexpr auto		skip(int i)			const	{ return temp(*this).template skip<N2>(i); }

	template<typename S> void	assign(const S &s) {
		auto	j = begin();
		for (auto i = s.begin(), ie = i + min(num_elements(s), size()); i != ie; ++i, ++j)
			j.assign(*i);
	}
	template<typename W> bool	write(W &&w) const	{
		for (auto i : *this)
			if (!w.write(i))
				return false;
		return true;
	}
	template<typename R> bool	read(R &&r) const	{
		for (auto i : *this)
			if (!r.read(i))
				return false;
		return true;
	}
	friend	constexpr size_t	num_elements(const iblock &c) {
		return c.count;
	}
};

template<typename I, int N> class iblock_iterator : public iblock<I, N - 1> {
	typedef iblock<I, N - 1>	B;
	int32	stride;
public:
	iblock_iterator&	operator++()		{ B::move(stride);		return *this; }
	iblock_iterator&	operator--()		{ B::move(-stride);		return *this; }
	iblock_iterator		operator++(int)		{ auto i = *this; B::move( stride); return i; }
	iblock_iterator		operator--(int)		{ auto i = *this; B::move(-stride); return i; }
	iblock_iterator&	operator+=(int i)	{ B::move(intptr_t(stride) * i);	return *this; }
	iblock_iterator&	operator-=(int i)	{ B::move(-intptr_t(stride) * i);	return *this; }

	constexpr B			operator*()								const	{ return *this; }
	constexpr bool		operator==(const iblock_iterator &b)	const	{ return B::buffer == b.buffer; }
	constexpr bool		operator!=(const iblock_iterator &b)	const	{ return B::buffer != b.buffer; }
	constexpr B			operator[](int i)						const	{ return iblock_iterator(*this) += i; }
	constexpr iblock_iterator	operator+(int i)				const	{ return iblock_iterator(*this) += i; }
	constexpr iblock_iterator	operator-(int i)				const	{ return iblock_iterator(*this) -= i; }
	constexpr int		operator-(const iblock_iterator &b)		const	{ return offset(b.buffer) / stride; }
	constexpr iblock_iterator(const B &b, int32 stride) : B(b), stride(stride)	{}
};

template<int D, typename I, int N> constexpr auto skip(const iblock<I, N> &b, int s);

template<typename I, int N> struct iblock<I, N>::temp : iblock<I, N> {
	temp(const iblock<I, N> &b) : iblock<I, N>(b) {}
	template<int N2> temp		sub(int i, int n)		{ iblock<I, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i, int n)		{ iblock<I, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i)			{ iblock<I, N2>::cut(i); return *this; }
	template<int N2, typename T_enable_if<N2 != 1>::type* = nullptr> temp	skip(int i)		{ iblock<I, N2>::skip(i); return *this; }
	template<int N2, typename T_enable_if<N2 == 1>::type* = nullptr> auto	skip(int i) 	{ return iso::skip<1>(*this, i); }
	friend const iblock<I, N>&	get(const temp &b)		{ return b; }
	friend iblock<I, N>&		put(temp &b)			{ return b; }
};

template<typename I> class iblock<I, 1> : public iblock_buffer<I> {
protected:
	typedef iblock_buffer<I>	B;
	using	B::buffer;
	uint32	count;
	struct	temp;
public:
	iblock() 	{}
	constexpr	iblock(I buffer, uint32 count) : iblock_buffer<I>(buffer), count(count)	{}

	template<typename C> iblock &operator=(const C &c) { assign(c); return *this; }

	constexpr uint32		size()					const	{ return count; }
	constexpr auto			end()					const	{ return begin() + count; }
	constexpr uint32		max_size()				const	{ return count; }
	constexpr decltype(auto)operator[](int i)		const	{ return buffer[i]; }
	constexpr int			index_of(const I e)		const	{ return e < buffer ? -1 : e - buffer; }

	template<int N2> temp	slice(int i, int n)	const	{ return temp(*this).template slice<N2>(i, n); }
	template<int N2> temp	slice(int i)		const	{ return temp(*this).template slice<N2>(i); }

	void		cut(int i, int n) {
		buffer += i;
		count	= count <= i ? 0 : n < 0 ? max(n + count - i, 0) : min(n, count - i);
	}
	void		cut(int i) {
		buffer += i;
		count	= count <= i ? 0 : count - i;
	}

	template<typename W> bool	write(W &&w) const	{ return writen(w, buffer, count); }
	template<typename R> bool	read(R &&r) const	{ return readn(r, buffer, count); }

	template<typename S> void assign(const S &s) {
		using iso::begin;
		copy(begin(s), begin(s) + min(num_elements(s), size()), begin());
	}
	friend constexpr size_t num_elements(const iblock &c) {
		return c.count;
	}
};

template<> class iblock<void*, 1> : public iblock_buffer<void*> {
protected:
	uint32		count;
public:
	iblock()	{}
	constexpr	iblock(void *buffer, uint32 count) : iblock_buffer<void*>(buffer), count(count)	{}
	constexpr uint32	size()	const	{ return count; }
};

template<typename T> struct byte_moveable {
	T	*p;
	byte_moveable(T *p) : p(p) {}
	operator T*()	const	{ return p; }
};

template<typename T> class iblock_buffer<byte_moveable<T>> {
protected:
	T			*buffer;
	void		move(intptr_t n)		{ buffer = (T*)((uint8*)buffer + n); }
public:
	constexpr int	pitch()		const	{ return (int)sizeof(T); }
	constexpr T*	begin()		const	{ return buffer; }
	constexpr operator	T*()	const	{ return buffer; }
	constexpr intptr_t	offset(const T *i) const	{ return (uint8*)i - (uint8*)buffer; }
	constexpr iblock_buffer(T *buffer = nullptr) : buffer(buffer) {}
};


//-----------------------------------------------------------------------------
//	block - view onto supplied memory
//-----------------------------------------------------------------------------
#if 1
template<typename T, int N> using block = iblock<byte_moveable<T>, N>;
template<typename T, int N> using block_iterator = iblock_iterator<byte_moveable<T>, N>;

#else
template<typename T, int N> class block_iterator;

template<typename T, int N> class block : public block<T, N - 1> {
protected:
	typedef block<T, N - 1>	B;
	using B::buffer;
	struct temp;

	int32		stride;
	uint32		count;

public:
	typedef block_iterator<T, N>	iterator, const_iterator;
	typedef B			element, &reference;
	typedef	block		block_type;

	block()		{}
	constexpr block(const B &b, int32 stride, uint32 count) : B(b), stride(stride), count(count)	{}

	constexpr uint32		size()					const	{ return count; }
	constexpr int32			pitch()					const	{ return stride; }
	constexpr iterator		begin()					const	{ return iterator(*this, stride); }
	constexpr iterator		end()					const	{ return begin() += (int)count; }
	constexpr uint32		max_size()				const	{ return max(count, B::max_size()); }
	constexpr B				operator[](int i)		const	{ return begin()[i]; }
	constexpr int			index_of(const T *e)	const	{ return e < buffer ? -1 : ((char*)e - (char*)buffer) / stride; }
	constexpr memory_block	data()					const	{ return memory_block(buffer, stride * count); }

	template<int N2> block<T, N2>&			get()			{ return *this; }
	template<int N2> const block<T, N2>&	get()	const	{ return *this; }

	void		cut(int i, int n) {
		if (n < 0)
			n += count - i;
		buffer	= (T*)((char*)buffer + i * intptr_t(stride));
		count	= count > i ? min(uint32(max(n, 0)), count - i) : 0;
	}
	void		cut(int i) {
		buffer	= (T*)((char*)buffer + i * intptr_t(stride));
		count	= count > i ? count - i : 0;
	}
	void		skip(int i) {
		stride *= i;
		count /= i;
	}

	template<int N2> constexpr uint32	size()					const	{ return block<T, N2>::size(); }
	template<int N2> constexpr int32	pitch()					const	{ return block<T, N2>::pitch(); }
	template<int N2> constexpr temp		sub(int i, int n)		const	{ return temp(*this).template sub<N2>(i, n); }
	template<int N2> constexpr temp		slice(int i, int n)	const	{ return temp(*this).template slice<N2>(i, n); }
	template<int N2> constexpr temp		slice(int i)		const	{ return temp(*this).template slice<N2>(i); }
	template<int N2> constexpr auto		skip(int i)				const	{ return temp(*this).template skip<N2>(i); }

	template<typename S> void assign(const S &s) {
		auto	j = begin();
		for (auto i = s.begin(), ie = i + min(num_elements(s), size()); i != ie; ++i, ++j)
			j.assign(*i);
	}

	template<typename W> bool	write(W &&w) const	{
		for (auto i : *this)
			if (!w.write(i))
				return false;
		return true;
	}

	template<typename R> bool	read(R &&r) const	{
		for (auto i : *this)
			if (!r.read(i))
				return false;
		return true;
	}

	friend	constexpr size_t num_elements(const block &c) {
		return c.count;
	}
};

template<typename T, int N> class block_iterator : public block<T, N - 1> {
	typedef block<T, N - 1>	B;
	using	B::buffer;
	int32		stride;
public:
	typedef random_access_iterator_t	iterator_category;
	block_iterator&	operator++()		{ B::move(stride);		return *this; }
	block_iterator&	operator--()		{ B::move(-stride);		return *this; }
	block_iterator	operator++(int)		{ auto i = *this; B::move( stride); return i; }
	block_iterator	operator--(int)		{ auto i = *this; B::move(-stride); return i; }
	block_iterator&	operator+=(int i)	{ B::move(stride * i);	return *this; }
	block_iterator&	operator-=(int i)	{ B::move(-stride * i);	return *this; }

	constexpr B			operator*()							const	{ return *this; }
	constexpr bool		operator==(const block_iterator &b)	const	{ return buffer == b.buffer; }
	constexpr bool		operator!=(const block_iterator &b)	const	{ return buffer != b.buffer; }
	constexpr B			operator[](int i)					const	{ return block_iterator(*this) += i; }
	constexpr block_iterator	operator+(int i)			const	{ return block_iterator(*this) += i; }
	constexpr block_iterator	operator-(int i)			const	{ return block_iterator(*this) -= i; }
	constexpr int		operator-(const block_iterator &b)	const	{ return ((char*)buffer - (char*)b.buffer) / stride; }
	constexpr block_iterator(const B &b, int32 _stride) : B(b), stride(_stride)	{}
};

template<int D, typename T, int N> constexpr auto skip(const block<T, N> &b, int s);

template<typename T, int N> struct block<T, N>::temp : block<T, N> {
	temp(const block<T, N> &b) : block<T, N>(b) {}
	template<int N2> temp		sub(int i, int n)		{ block<T, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i, int n)	{ block<T, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i)		{ block<T, N2>::cut(i); return *this; }
	template<int N2, typename T_enable_if<N2 != 1>::type* = nullptr> temp	skip(int i)		{ block<T, N2>::skip(i); return *this; }
	template<int N2, typename T_enable_if<N2 == 1>::type* = nullptr> auto	skip(int i) 	{ return iso::skip<1>(*this, i); }
	friend const block<T, N>&	get(const temp &b)		{ return b; }
	friend block<T, N>&			put(temp &b)			{ return b; }
};


template<typename T> class block<T, 1> {
protected:
	T			*buffer;
	uint32		count;
	void		move(int n) { buffer = (T*)((char*)buffer + n); }
	struct temp;
public:
	typedef T		element, &reference, *iterator, *const_iterator;
	typedef	block	block_type;

	block() : buffer(0)	{}
	constexpr	block(T *buffer, uint32 count) : buffer(buffer), count(count)	{}
	template<size_t N> constexpr	block(T (&buffer)[N]) : buffer(buffer), count(N)	{}

	template<typename C> block &operator=(const C &c) { assign(c); return *this; }

	constexpr operator		T*()					const	{ return buffer; }
	constexpr uint32		size()					const	{ return count; }
	constexpr int32			pitch()					const	{ return (int32)sizeof(T); }
	constexpr iterator		begin()					const	{ return buffer; }
	constexpr iterator		end()					const	{ return begin() + count; }
	constexpr uint32		max_size()				const	{ return count; }
	constexpr T&			operator[](int i)		const	{ return begin()[i]; }
	constexpr int			index_of(const T *e)	const	{ return e < buffer ? -1 : e - buffer; }
	constexpr memory_block	data()					const	{ return memory_block(buffer, count * sizeof(T)); }

	template<int N2> temp	slice(int i, int n)	const	{ return temp(*this).template slice<N2>(i, n); }
	template<int N2> temp	slice(int i)		const	{ return temp(*this).template slice<N2>(i); }

	void		cut(int i, int n) {
		if (n < 0)
			n += count - i;
		buffer	+= i;
		count	= count > i ? min(uint32(max(n, 0)), count - i) : 0;
	}
	void		cut(int i) {
		buffer	+= i;
		count	= count > i ? count - i : 0;
	}

	template<typename W> bool	write(W &&w) const	{ return writen(w, buffer, count); }
	template<typename R> bool	read(R &&r) const	{ return readn(r, buffer, count); }

	template<typename S> void assign(const S &s) {
		using iso::begin;
		copy(begin(s), begin(s) + min(num_elements(s), size()), begin());
	}
	friend constexpr size_t num_elements(const block &c) {
		return c.count;
	}
};

template<> class block<void, 1> {
protected:
	void		*buffer;
	uint32		count;
	void		move(int n) { buffer = (char*)buffer + n; }
public:
	block() : buffer(0)	{}
	constexpr	block(void *buffer, uint32 count) : buffer(buffer), count(count)	{}

	constexpr void*			begin()					const	{ return buffer; }
	constexpr operator		void*()					const	{ return buffer; }
	constexpr uint32		size()					const	{ return count; }
};

template<typename T> struct block<T, 1>::temp : block<T, 1> {
	temp(const block<T, 1> &b) : block<T, 1>(b) {}
	template<int N2> temp		sub(int i, int n)		{ block<T, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i, int n)	{ block<T, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i)		{ block<T, N2>::cut(i); return *this; }
	friend const block<T, 1>&	get(const temp &b)		{ return b; }
	friend block<T, 1>&			put(temp &b)			{ return b; }
};
#endif

template<typename T>						constexpr block<T, 1>	make_block(T *p, uint32 w)											{ return block<T, 1>(p, w); }

template<typename T, int N>					constexpr block<T, N+1> make_block(const block<T,N> &p, uint32 w)							{ return block<T, N+1>(p, p.pitch() * p.size(), w); }
template<typename T, int N, typename... W>	constexpr auto			make_block(const block<T, N> &p, uint32 w0, W...w)					{ return make_block(make_block(p, w0), w...); }
template<typename T, typename... W>			constexpr auto			make_block(T *p, uint32 w0, W...w)									{ return make_block(make_block(p, w0), w...); }

template<typename T, int N>					constexpr block<T, N+1> make_strided_block(const block<T,N> &p, uint32 s, uint32 w)			{ return block<T, N+1>(p, s, w); }
template<typename T, int N, typename... W>	constexpr auto			make_strided_block(const block<T, N> &p, uint32 s0, uint32 w0, W...w){ return make_strided_block(make_strided_block(p, s0, w0), w...); }
template<typename T, typename... W>			constexpr auto			make_strided_block(T *p, uint32 w0, W...w)							{ return make_strided_block(make_block(p, w0), w...); }

template<int N>	struct block_maker {
	template<typename T, int M, typename W> static constexpr auto f(const block<T,M> &p, W *w)	{ return block_maker<N - 1>::f(make_block(p, *w), w + 1); }
	template<typename T, int M, typename W> static constexpr auto fs(const block<T,M> &p, W *w)	{ return block_maker<N - 1>::fs(make_strided_block(p, w[0], w[1]), w + 2); }
	template<typename T, int M, typename W> static constexpr auto r(const block<T,M> &p, W *w)	{ return block_maker<N - 1>::r(make_block(p, w[N - 1]), w); }
};
template<>		struct block_maker<0> {
	template<typename T, int M, typename W> static constexpr auto f(const block<T,M> &p, W *w)	{ return p; }
	template<typename T, int M, typename W> static constexpr auto fs(const block<T,M> &p, W *w) { return p; }
	template<typename T, int M, typename W> static constexpr auto r(const block<T,M> &p, W *w)	{ return p; }
};
template<int N, typename T, typename W>		constexpr block<T, N>	make_block(T *p, W *w)			{ return block_maker<N - 1>::f(block<T, 1>(p, *w), w + 1); }
template<int N, typename T, typename W>		constexpr block<T, N>	make_strided_block(T *p, W *w)	{ return block_maker<N - 1>::fs(block<T, 1>(p, *w), w + 1); }
template<int N, typename T, typename W>		constexpr block<T, N>	make_block_rev(T *p, W *w)		{ return block_maker<N - 1>::r(block<T, 1>(p, w[N - 1]), w); }

template<typename T> block<T, 2> flip_vertical(const block<T, 2> &b) {
	return make_strided_block(b[b.template size<2>() - 1].begin(), -b.template pitch<2>(), b.template size<1>(), b.template size<2>());
}

template<typename T, int N>				constexpr size_t		data_size(const block<T, N> &b)				{ return b.size() * b.pitch(); }
template<typename T, int N>				constexpr size_t		num_elements_total(const block<T, N> &b)	{ return b.size() * num_elements_total(b[0]); }
template<typename T>					constexpr size_t		num_elements_total(const block<T, 1> &b)	{ return b.size(); }
template<typename U, typename T>		constexpr block<U, 1>	element_cast(const block<T, 1> &b)			{ return block<U, 1>((U*)b.begin(), b.size()); }
template<typename U, typename T, int N> constexpr block<U, N>	element_cast(const block<T, N> &b)			{ return block<U, N>(element_cast<U>(b[0]), b.pitch(), b.size()); }
template<typename U, typename T, int N> constexpr block<U, N>	element_cast(block<T, N> &b)				{ return element_cast<U>(make_const(b)); }
template<typename U, typename T, int N> constexpr block<U, N>	element_cast(block<T, N> &&b)				{ return element_cast<U>(make_const(b)); }
template<int D, typename T, int N>		constexpr bool			can_flatten(const block<T, N> &b)			{ return b.template pitch<D>() == b.template pitch<D - 1>() * b.template size<D - 1>(); }

template<int D, int N>	struct flatten_s		{ template<typename T> static constexpr auto f(const block<T, N> &b) { return make_strided_block(flatten_s<D, N - 1>::f(b[0]), b.pitch(), b.size()); }};
template<int D>			struct flatten_s<D, D>	{ template<typename T> static constexpr auto f(const block<T, D> &b) { return make_block<T, D - 2>(b, b.template size<D - 1>() * b.size()); }};
template<>				struct flatten_s<1, 1>	{ template<typename T> static constexpr auto f(const block<T, 1> &b) { return b; }};
template<>				struct flatten_s<2, 2>	{ template<typename T> static constexpr auto f(const block<T, 2> &b) { return make_block(b.template get<1>().begin(), b.template size<1>() * b.template size()); }};
template<int D, typename T, int N> constexpr auto flatten(const block<T, N> &b)		{ return flatten_s<D, N>::f(b); }

template<int D, int N>	struct skip_s			{ template<typename T> static constexpr auto f(const block<T, N> &b, int s) { return make_strided_block(skip_s<D, N - 1>::f(b[0], s), b.pitch(), b.size()); }};
template<int D>			struct skip_s<D, D>		{ template<typename T> static constexpr auto f(const block<T, D> &b, int s) { return make_strided_block<T, D - 1>(b, b.pitch() * s, b.size() / s); }};
template<>				struct skip_s<1, 1>		{ template<typename T> static constexpr auto f(const block<T, 1> &b, int s) { return make_strided_block(b.begin(), b.pitch() * s, 1, b.size() / s); }};
template<int D, typename T, int N> constexpr auto skip(const block<T, N> &b, int s)	{ return skip_s<D, N>::f(b, s); }


//template<int X, int Y, typename T> auto block_mask(const block<T, 2> &block) {
//	return block_mask<X, Y>(block.template size<1>(), block.template size<2>());
//}

/*
template<typename F, typename AT, typename B, int N> struct deferred<F, const block<AT, N>&, B> : F {
	typedef const block<AT, N>	&A;
	typedef typename T_noref<B>::type	B2;
	A	a;
	B	b;

	typedef deferred<F, const block<AT, N - 1>&, typename B2::element>	element, reference;
	typedef random_access_iterator_t	iterator_category;
	typedef indexed_iterator<deferred, int_iterator<int> > iterator;

	deferred(F &&f, A &&_a, B &&_b)	: F(move(f)), a(forward<A>(_a)), b(forward<B>(_b)) {}
	element		operator[](int i)	const	{ return element(F(), a[i], b[i]); }
	iterator	begin()				const	{ return iterator(*this, 0); }
	iterator	end()				const	{ return iterator(*this, num_elements(*this)); }
	friend constexpr size_t num_elements(const deferred &c) {
		return min(c.a.size(), num_elements(c.b));
	}
};

template<typename F, typename AT, typename B> struct deferred<F, const block<AT, 1>&, B> : F {
	typedef const block<AT, 1>	&A;
	A	a;
	B	b;

	typedef AT							element, reference;
	typedef random_access_iterator_t	iterator_category;
	typedef indexed_iterator<deferred, int_iterator<int> > iterator;

	deferred(F &&f, A &&_a, B &&_b)	: F(move(f)), a(forward<A>(_a)), b(forward<B>(_b)) {}
	element		operator[](int i)	const	{ return (*this)(a[i], b[i]); }
	iterator	begin()				const	{ return iterator(*this, 0); }
	iterator	end()				const	{ return iterator(*this, num_elements(*this)); }
	friend constexpr size_t num_elements(const deferred &c) {
		return min(c.a.size(), num_elements(c.b));
	}
};
*/
//-----------------------------------------------------------------------------
//	auto_block - owns memory
//-----------------------------------------------------------------------------

template<typename T, int N> struct auto_block : block<T, N> {
	typedef block<T, N> B;

	auto_block()							{ clear(*this); }
#ifdef USE_RVALUE_REFS
	auto_block(auto_block &&b) : B(b)		{ b.buffer = 0; }
	auto_block& operator=(auto_block &&b)	{ swap(*this, b); return *this; }
#endif
	~auto_block()					{ delete[] B::buffer; }
	B&			get() 				{ return *this; }
	const B&	get() const			{ return *this; }
    B			detach() 			{ block<T, N> b = *this; B::buffer = 0; return b; }

	friend void swap(auto_block &a, auto_block &b) { swap((B&)a, (B&)b); }

	template<typename U> friend constexpr block<U, N>	element_cast(const auto_block &b)	{ return element_cast<U>(static_cast<const B&>(b)); }
	template<typename U> friend constexpr block<U, N>	element_cast(auto_block &b)			{ return element_cast<U>(make_const(b)); }
	template<typename U> friend constexpr block<U, N>	element_cast(auto_block &&b)		{ return element_cast<U>(make_const(b)); }
};

template<typename T, int N>				constexpr auto _make_auto_block(block<T, N> &&p)	{ return force_cast<auto_block<T,N>>(p); }
template<typename T, typename... W>		inline auto		make_auto_block(W... w)				{ return _make_auto_block(make_block(new T[prod(w...)], w...)); }
template<typename T, int N, typename W>	constexpr auto	make_auto_block(W *w)				{ return _make_auto_block(make_block(new T[prod<N>], w));}
template<typename T, int N, typename W>	constexpr auto	make_auto_block_rev(W *w)			{ return _make_auto_block(make_block_rev(new T[prod<N>], w)); }

template<typename T, typename T2, typename... W>		inline auto _make_auto_block_using(const T2 &p, W... w)				{ return make_block(new T[prod(w...)], w...); }
template<typename T, typename T2, int N, typename... W>	inline auto _make_auto_block_using(const block<T2,N> &p, W... w)	{ return _make_auto_block_using<T>(p[0], p.size(), w...); }
template<typename T, int N, typename... W>				inline auto	make_auto_block_using(const block<T,N> &p, W... w)		{ return _make_auto_block(_make_auto_block_using<T>(p, w...)); }
template<typename T, typename T2, int N, typename... W>	inline auto	make_auto_block_using(const block<T2,N> &p, W... w)		{ return _make_auto_block(_make_auto_block_using<T>(p, w...)); }
template<typename T, typename... W>						inline auto	make_auto_block_using(const T &t, W... w)				{ return make_auto_block<T>(w...); }


//-----------------------------------------------------------------------------
//	algorithms
//-----------------------------------------------------------------------------

template<class I, class F>						inline F for_each(I i, I end, F f);
template<class I1, class I2, class F>			inline F for_each2(I1 i1, I1 end, I2 i2, F f);
template<class I1, class I2, class I3, class F>	inline F for_each3(I1 i1, I1 end, I2 i2, I3 i3, F f);
//template<class I, class O> inline void copy(I first, I last, O dest);

template<typename T1, int N, class F> F for_each(block_iterator<T1, N> i1, block_iterator<T1, N> end, F f) {
	for (; i1 != end; ++i1)
		for_each(i1.begin(), i1.end(), f);
	return f;
}
template<typename T1, typename T2, int N, class F> F for_each2(block_iterator<T1, N> i1, block_iterator<T1, N> end, block_iterator<T2, N> i2, F f) {
	for (; i1 != end; ++i1, ++i2)
		for_each2(*i1, *i2, f);
	return f;
}
template<typename T1, typename T2, typename T3, int N, class F> F for_each3(block_iterator<T1, N> i1, block_iterator<T1, N> end, block_iterator<T2, N> i2, block_iterator<T3, N> i3, F f) {
	for (; i1 != end; ++i1, ++i2, ++i3)
		for_each3(*i1, *i2, *i3, f);
	return f;
}

template<typename T, int N> const block<T,N>& clamp(const block<T, N> &b, T t0, T t1) {
	for_each(b, [t0, t1](T &t) { t = clamp(t, t0, t1); });
	return b;
}

template<typename A, typename B, int N> const auto& operator+=(const block<A, N> &a, const block<B, N> &b)	{ for_each2(a, b, [](A &a, const B &b) { a += b; }); return a;}
template<typename A, typename B, int N> const auto& operator-=(const block<A, N> &a, const block<B, N> &b)	{ for_each2(a, b, [](A &a, const B &b) { a -= b; }); return a;}
template<typename A, typename B, int N> const auto& operator*=(const block<A, N> &a, const block<B, N> &b)	{ for_each2(a, b, [](A &a, const B &b) { a *= b; }); return a;}
template<typename A, typename B, int N> const auto& operator/=(const block<A, N> &a, const block<B, N> &b)	{ for_each2(a, b, [](A &a, const B &b) { a /= b; }); return a;}
template<typename A, typename B, int N> const auto& operator&=(const block<A, N> &a, const block<B, N> &b)	{ for_each2(a, b, [](A &a, const B &b) { a &= b; }); return a;}
template<typename A, typename B, int N> const auto& operator|=(const block<A, N> &a, const block<B, N> &b)	{ for_each2(a, b, [](A &a, const B &b) { a |= b; }); return a;}
template<typename A, typename B, int N> const auto& operator^=(const block<A, N> &a, const block<B, N> &b)	{ for_each2(a, b, [](A &a, const B &b) { a ^= b; }); return a;}

template<typename T, int N, typename U> const auto& operator+=(const block<T, N> &b, const U &u)			{ for_each(b, [u](T &t) { t += u; }); return b; }
template<typename T, int N, typename U> const auto& operator-=(const block<T, N> &b, const U &u)			{ for_each(b, [u](T &t) { t -= u; }); return b; }
template<typename T, int N, typename U> const auto& operator*=(const block<T, N> &b, const U &u)			{ for_each(b, [u](T &t) { t *= u; }); return b; }
template<typename T, int N, typename U> const auto& operator/=(const block<T, N> &b, const U &u)			{ for_each(b, [u](T &t) { t /= u; }); return b; }
template<typename T, int N, typename U> const auto& operator&=(const block<T, N> &b, const U &u)			{ for_each(b, [u](T &t) { t ^= u; }); return b; }
template<typename T, int N, typename U> const auto& operator|=(const block<T, N> &b, const U &u)			{ for_each(b, [u](T &t) { t |= u; }); return b; }
template<typename T, int N, typename U> const auto& operator^=(const block<T, N> &b, const U &u)			{ for_each(b, [u](T &t) { t ^= u; }); return b; }

template<typename A, typename B, int N> auto operator+(const block<A, N> &a, const block<B, N> &b)			{ return make_deferred<op_add>(a, b); }
template<typename A, typename B, int N> auto operator-(const block<A, N> &a, const block<B, N> &b)			{ return make_deferred<op_sub>(a, b); }
template<typename A, typename B, int N> auto operator*(const block<A, N> &a, const block<B, N> &b)			{ return make_deferred<op_mul>(a, b); }
template<typename A, typename B, int N> auto operator/(const block<A, N> &a, const block<B, N> &b)			{ return make_deferred<op_div>(a, b); }

template<typename T> T dot(const block<T, 1> &a, const block<T, 1> &b) {
	T	t	= 0;
	T	*pa	= a;
	T	*pb	= b;
	for (int i = 0, n = min(a.size(), b.size()); i < n; ++i)
		t += pa[i] * pb[i];
	return t;
}

template<int X, int Y, typename D, typename S> void decode_blocked(const S &s, D& d) {
	for (uint32 y = 0; y < s.size(); y++) {
		for (uint32 x = 0; x < s.template size<1>(); x++)
			s[y][x].Decode(d.template sub<1>(x * X, X).template sub<2>(y * Y, Y));
	}
}
template<int X, int Y, int Z, typename D, typename S> void decode_blocked(const S &s, D& d) {
	for (uint32 z = 0; z < s.size(); z++) {
		for (uint32 y = 0; y < s.template size<2>(); y++)
			for (uint32 x = 0; x < s.template size<1>(); x++)
				s[y][x].Decode(d.template sub<1>(x * X, X).template sub<2>(y * Y, Y).template sub<3>(z * Z, Z));
	}
}
template<int X, int Y, typename D,typename S> void encode_blocked(const S &s, D& d) {
	for (uint32 y = 0; y < d.size(); y++) {
		for (uint32 x = 0; x < d.template size<1>(); x++)
			d[y][x].Encode(s.template sub<1>(x * X, X).template sub<2>(y * Y, Y));
	}
}
template<int X, int Y, int Z, typename D,typename S> void encode_blocked(const S &s, D& d) {
	for (uint32 z = 0; z < s.size(); z++) {
		for (uint32 y = 0; y < s.template size<2>(); y++)
			for (uint32 x = 0; x < d.template size<1>(); x++)
				d[y][x].Encode(s.template sub<1>(x * X, X).template sub<2>(y * Y, Y).template sub<3>(z * Z, Z));
	}
}

template<typename T, typename V, int N> void fill(block_iterator<T, N> s, block_iterator<T, N> e, const V &v) {
	for (; s != e; ++s) fill(*s, v);
}

template<typename S, typename D, int N> void copy(block_iterator<S, N> s, block_iterator<S, N> e, block_iterator<D, N> d) {
	for (; s != e; ++s, ++d)
		copy(*s, *d);
}
template<typename S, typename D, int N> void copy(const block<S, N> &s, D &&d)	{ copy(unconst(s), d); }
template<typename S, typename D, int N> void copy(block<S, N> &&s, D &&d)		{ copy(s, d); }

template<typename T, int N, class P> inline T *find_if(const block<T, N> &b, P pred) {
	for (auto i : b) {
		if (T *p = find_if(i, pred))
			return p;
	}
	return 0;
}
template<typename T, class P> inline T *find_if(block<T, 1> &&b, P pred) {
	T *p = find_if(b.begin(), b.end(), pred);
	return p == b.end() ? 0 : p;
}
template<typename T, int N, class P> inline T *find_if(block<T, N> &&b, P pred) {
	for (auto i = b.begin(), e = b.end(); i != e; ++i) {
		if (T *p = find_if(*i, pred))
			return p;
	}
	return 0;
}
//template<typename T, class P> inline T *find_if(block<T, 1> &b, P pred) {
//	T *p = find_if(b.begin(), b.end(), pred);
//	return p == b.end() ? 0 : p;
//}

template<typename T> inline T dist2(block<T,1> a, block<T,1> b) {
	T s = 0;
	for (T *i0 = a.begin(), *i1 = b.begin(), *e0 = a.end(); i0 != e0; ++i0, ++i1)
		s += square(*i0 - *i1);
	return s;
}
template<typename T, int N> inline T dist2(block<T,N> a, block<T,N> b) {
	T		s = 0;
	auto	ib = b.begin();
	for (auto ia = a.begin(), ea = a.end(); ia != ea; ++ia)
		s += dist2(ia, ia);
}

//-----------------------------------------------------------------------------
// interval specialisation
//-----------------------------------------------------------------------------

template<typename T> struct interval<block<T, 1> > : auto_block<interval<T>, 1> {
	typedef block<T, 1>			V;
	typedef	interval<T>			I;
	typedef auto_block<I, 1>	B;

	interval() {}
	interval(const V &a, const V &b)	{
		if (!B::buffer)
			B::operator=(make_auto_block<I>(a.size()));
		for_each3(*(B*)this, a, b, [](I &me, const T &a, const T&b) { me = I(a, b); });
	}
	explicit interval(const V &a) {
		if (!B::buffer)
			B::operator=(make_auto_block<I>(a.size()));
		copy(a, *this);
		//for_each2(*(B*)this, a, [](I &me, const T &a) { me = I(a); });
	}
	void operator=(const V &a) {
		if (!B::buffer)
			B::operator=(make_auto_block<I>(a.size()));
		copy(a, *this);
		//for_each2(*(B*)this, a, [](I &me, const T &a) { me = I(a); });
	}

	interval& operator|=(const V &t)	{
		for_each2(*(B*)this, t, [](I &me, const T &t) { me |= t; });
		return *this;
	}
};

template<typename T> struct interval<auto_block<T, 1> > : interval<block<T, 1> > {
	void operator=(const block<T, 1> &a) {
		interval<block<T, 1> >::operator=(a);
	}
};

template<typename T> int max_component_index(const interval<block<T, 1> > &box) {
	int	max_axis	= 0;
	T	max_ext		= box[0].length();
	for (int j = 1, dims = box.size(); j < dims; j++) {
		if (box[j].length() > max_ext) {
			max_ext		= box[j].length();
			max_axis	= j;
		}
	}
	return max_axis;
}

//-----------------------------------------------------------------------------
//	is_scalar_multiple
//-----------------------------------------------------------------------------

template<typename T, int N> bool is_scalar_multiple(const block<T, N> &a, const block<T, N> &b, float scale) {
	auto	ia	= a.begin(), ib = b.begin();
	for (int n = a.size(); n--; ++ia, ++ib) {
		if (!is_scalar_multiple(*ia, *ib,  scale))
			return false;
	}
	return true;
}


template<typename T> bool find_scalar_multiple(const block<T, 1> &a, const block<T, 1> &b, float &scale) {
	int		imax	= 0;
	int		n		= a.size();
	for (int i = 1; i < n; i++) {
		if (bigger(a[i], a[imax]))
			imax = i;
	}

	if (!find_scalar_multiple(a[imax], b[imax], scale))
		return false;

	auto	ia	= a.begin(), ib = b.begin();

	for (int n = a.size(); n--; ++ia, ++ib) {
		if (!is_scalar_multiple(*ia, *ib, scale))
			return false;
	}
	return true;
}


template<typename T, int N> bool find_scalar_multiple(const block<T, N> &a, const block<T, N> &b, float &scale) {
	scale	= 0;

	T	*pmax = 0;
	for_each(a, [&pmax](T &i) { if (!pmax || i > *pmax) pmax = &i; });

	int		i0		= a.index_of(pmax);
	if (!find_scalar_multiple(a[i0], b[i0], scale))
		return false;

	for (int i = 0, n = a.template size<N>(); i < n; ++i) {
		if (i != i0 && !is_scalar_multiple(a[i], b[i], scale))
			return false;
	}
	return true;
}


}//namespace iso

#endif //BLOCK_H
