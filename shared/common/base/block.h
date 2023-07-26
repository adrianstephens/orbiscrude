#ifndef BLOCK_H
#define BLOCK_H

#include "defs.h"
#include "deferred.h"
#include "interval.h"

namespace iso {

template<typename I> constexpr int		intra_pitch(const I&)				{ return 1; }
template<typename I> void				intra_move(I &i, intptr_t n)		{ i += n; }
template<typename I> constexpr intptr_t	intra_diff(const I &a, const I &b)	{ return a - b; }

template<typename I, int N> class iblock;
template<typename I, int N> class iblock_iterator;
template<typename I, int N> class iblockptr;

//-----------------------------------------------------------------------------
//	iblock
//-----------------------------------------------------------------------------

template<typename I> class iblock<I, 1> {
protected:
	friend iblockptr<I, 1>;
	I		buffer;
	uint32	count;
//	struct	temp;
public:
	iblock() 	{}
	iblock(const iblock&)				= default;
	constexpr	iblock(I buffer, uint32 count) : buffer(buffer), count(count)	{}
//	iblock&					operator=(const iblock &c) 	{ assign(c); return *this; }
//	template<typename C> iblock &operator=(const C &c)	{ assign(c); return *this; }
	constexpr operator		auto()					const	{ return begin(); }

	constexpr uint32		size()					const	{ return count; }
	constexpr int32			pitch()					const	{ return intra_pitch(buffer); }
	constexpr auto			begin()					const	{ using iso::begin; return begin(buffer); }
	constexpr auto			end()					const	{ using iso::begin; return begin(buffer) + count; }
	constexpr uint32		max_size()				const	{ return count; }
	constexpr decltype(auto)operator[](int i)		const	{ return buffer[i]; }
	constexpr int			index_of(const I e)		const	{ return e < buffer ? -1 : e - buffer; }

//	template<int N2> temp	slice(int i, int n)	const	{ return temp(*this).template slice<N2>(i, n); }
//	template<int N2> temp	slice(int i)		const	{ return temp(*this).template slice<N2>(i); }

	iblock<I, 1>	slice(int i, int n)	const	{ auto t = *this; t.cut(i, n); return t; }
	iblock<I, 1>	slice(int i)		const	{ auto t = *this; t.cut(i); return t; }

	void		cut(int i) {
		buffer	+= i;
		count	= count <= i ? 0 : count - i;
	}
	void		cut(int i, int n) {
		cut(i);
		count	= n < 0 ? max(n + count, 0) : min(n, count);
	}

	template<typename W> bool	write(W &&w) const	{ using iso::begin; return writen(w, begin(buffer), count); }
	template<typename R> bool	read(R &&r) const	{ using iso::begin; return readn(r, begin(buffer), count); }

	template<typename S> void assign(const S &s) {
		using iso::begin;
		copy(begin(s), begin(s) + min(num_elements(s), size()), begin());
	}
	friend constexpr size_t num_elements(const iblock &c) {
		return c.count;
	}
};

template<typename I, int N> class iblock : public iblock<I, N - 1> {
protected:
	typedef iblock<I, N - 1>	B;
	struct	temp;
	int32	stride;
	uint32	count;

public:
	typedef iblock_iterator<I, N>	iterator;

	iblock()	{}
	iblock(const iblock&)				= default;
	constexpr iblock(const B &b, int32 stride, uint32 count) : B(b), stride(stride), count(count)	{}
//	iblock&	operator=(const iblock &c) 	{ assign(c); return *this; }
	
	constexpr uint32		size()					const	{ return count; }
	constexpr int32			pitch()					const	{ return stride; }
	constexpr iterator		begin()					const	{ return iterator(*this, stride); }
	constexpr iterator		end()					const	{ return begin() += (int)count; }
	constexpr uint32		max_size()				const	{ return max(count, B::max_size()); }
	constexpr B				operator[](int i)		const	{ return begin()[i]; }
	constexpr int			index_of(const I e)		const	{ return intra_diff(e, B::buffer) / stride; }

	template<int N2> iblock<I, N2>&			get()			{ return *this; }
	template<int N2> const iblock<I, N2>&	get()	const	{ return *this; }

	void		cut(int i) {
		intra_move(B::buffer, i * intptr_t(stride));
		count	= count <= i ? 0 : count - i;
	}
	void		cut(int i, int n) {
		cut(i);
		count	= n < 0 ? max(n + count, 0) : min(n, count);
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
	iblock_iterator&	operator++()		{ intra_move(B::buffer, stride);		return *this; }
	iblock_iterator&	operator--()		{ intra_move(B::buffer, -stride);		return *this; }
	iblock_iterator		operator++(int)		{ auto i = *this; intra_move(B::buffer,  stride); return i; }
	iblock_iterator		operator--(int)		{ auto i = *this; intra_move(B::buffer, -stride); return i; }
	iblock_iterator&	operator+=(int i)	{ intra_move(B::buffer,  intptr_t(stride) * i);	return *this; }
	iblock_iterator&	operator-=(int i)	{ intra_move(B::buffer, -intptr_t(stride) * i);	return *this; }

	constexpr B			operator*()								const	{ return *this; }
	constexpr bool		operator==(const iblock_iterator &b)	const	{ return B::buffer == b.buffer; }
	constexpr bool		operator!=(const iblock_iterator &b)	const	{ return B::buffer != b.buffer; }
	constexpr B			operator[](int i)						const	{ return iblock_iterator(*this) += i; }
	constexpr iblock_iterator	operator+(int i)				const	{ return iblock_iterator(*this) += i; }
	constexpr iblock_iterator	operator-(int i)				const	{ return iblock_iterator(*this) -= i; }
	constexpr int		operator-(const iblock_iterator &b)		const	{ return intra_diff(B::buffer, b.buffer) / stride; }
	constexpr iblock_iterator(const B &b, int32 stride) : B(b), stride(stride)	{}
};

template<int D, typename I, int N> constexpr auto skip(const iblock<I, N> &b, int s);

template<typename I, int N> struct iblock<I, N>::temp : iblock<I, N> {
	temp(const iblock<I, N> &b) : iblock<I, N>(b) {}
	template<int N2> temp		sub(int i, int n)		{ iblock<I, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i, int n)		{ iblock<I, N2>::cut(i, n); return *this; }
	template<int N2> temp		slice(int i)			{ iblock<I, N2>::cut(i); return *this; }
	template<int N2, enable_if_t<N2 != 1>* = nullptr> temp	skip(int i)		{ iblock<I, N2>::skip(i); return *this; }
	template<int N2, enable_if_t<N2 == 1>* = nullptr> auto	skip(int i) 	{ return iso::skip<1>(*this, i); }
	friend const iblock<I, N>&	get(const temp &b)		{ return b; }
	friend iblock<I, N>			get(temp &&b)			{ return b; }
	friend iblock<I, N>&		put(temp &b)			{ return b; }
};


template<> class iblock<void*, 1> {
protected:
	void		*buffer;
	uint32		count;
public:
	iblock()	{}
	constexpr	iblock(void *buffer, uint32 count) : buffer(buffer), count(count)	{}
	constexpr uint32	size()	const	{ return count; }
};

//-----------------------------------------------------------------------------
//	iblockptr
//-----------------------------------------------------------------------------

template<typename I> class iblockptr<I, 1> {
protected:
	I		buffer;
	constexpr auto			_begin()				const	{ using iso::begin; return begin(buffer); }
public:
	iblockptr()	{}
	constexpr	iblockptr(I buffer) : buffer(buffer)	{}
	constexpr	iblockptr(const iblock<I, 1> &b) : buffer(b.buffer)	{}
	constexpr int32			pitch()					const	{ return intra_pitch(buffer); }
	constexpr int			index_of(const I e)		const	{ return e < buffer ? -1 : e - buffer; }
	constexpr decltype(auto) operator[](int i)		const	{ return buffer[i]; }
	auto&					operator+=(int i)				{ buffer += i; return *this; }

};

template<typename I, int N> class iblockptr : public iblockptr<I, N - 1> {
protected:
	typedef iblockptr<I, N - 1>	B;
	struct	temp;
	int32	stride;
	constexpr auto			_begin()				const	{ return *this; }

public:
	iblockptr()	{}
	constexpr iblockptr(const B &b, int32 stride) : B(b), stride(stride) {}
	constexpr iblockptr(const iblock<I, N> &b) : B(b), stride(b.pitch()) {}

	constexpr auto			begin()					const	{ return B::_begin(); }
	constexpr int32			pitch()					const	{ return stride; }
	constexpr int			index_of(const I e)		const	{ return intra_diff(e, B::buffer) / stride; }
	template<int N2> constexpr temp	slice(int i)	const	{ return temp(*this).template slice<N2>(i); }
	template<typename...I> auto		slice(I...i)	const	{ return temp(*this).template slice<1>(i...); }

	auto&			operator++()		{ intra_move(B::buffer, stride);					return *this; }
	auto&			operator--()		{ intra_move(B::buffer, -stride);					return *this; }
	auto			operator++(int)		{ auto i = *this; intra_move(B::buffer,  stride);	return i; }
	auto			operator--(int)		{ auto i = *this; intra_move(B::buffer, -stride);	return i; }
	auto&			operator+=(int i)	{ intra_move(B::buffer,  intptr_t(stride) * i);		return *this; }
	auto&			operator-=(int i)	{ intra_move(B::buffer, -intptr_t(stride) * i);		return *this; }

	constexpr B		operator*()						const	{ return *this; }
	constexpr bool	operator==(const iblockptr &b)	const	{ return B::buffer == b.buffer; }
	constexpr bool	operator!=(const iblockptr &b)	const	{ return B::buffer != b.buffer; }
	constexpr auto	operator[](int i)				const	{ return (*this + i).begin(); }
	constexpr auto	operator+(int i)				const	{ return copy(*this) += i; }
	constexpr auto	operator-(int i)				const	{ return copy(*this) -= i; }
	constexpr int	operator-(const iblockptr &b)	const	{ return intra_diff(B::buffer, b.buffer) / stride; }
};

template<typename I, int N> struct iblockptr<I, N>::temp : iblockptr<I, N> {
	temp(const iblockptr<I, N> &b) : iblockptr<I, N>(b) {}
	template<int N2> temp	slice(int i)	{ iblockptr<I, N2>::operator+=(i); return *this; }
	template<int N2, typename...I> auto		slice(int i0, I...i)	{ return slice<N2>(i0).template slice<N2 + 1>(i...); }
};

//-----------------------------------------------------------------------------
//	makers
//-----------------------------------------------------------------------------

template<typename I>						constexpr auto	make_blockptr(I p)														{ return iblockptr<I, 1>(p); }
template<typename I, int N>					constexpr auto	make_blockptr(const iblockptr<I, N> &p, uint32 s)						{ return iblockptr<I, N + 1>(p, s); }
template<typename I, int N, typename... W>	constexpr auto	make_blockptr(const iblockptr<I, N> &p, uint32 s0, W...s)				{ return make_block(make_blockptr(p, s0), s...); }
//template<typename I, typename... W>			constexpr auto	make_blockptr(I p, W...s)												{ return make_blockptr(make_blockptr(p), s...); }
template<typename T>						constexpr auto	make_iblockptr(T *p)													{ return iblockptr<T*, 1>(p); }
template<typename T, typename... W>			constexpr auto	make_iblockptr(T *p, W...w)												{ return make_blockptr(make_iblockptr(p), w...); }
template<typename T, int N>					constexpr auto	make_iblockptr(T (*p)[N])												{ return make_iblockptr((T*)p, N * sizeof(T)); }

template<typename I>						constexpr auto	make_block(I p, uint32 w)												{ return iblock<I, 1>(p, w); }
template<typename I, int N>					constexpr auto	make_block(const iblock<I, N> &p, uint32 w)								{ return iblock<I, N + 1>(p, p.pitch() * p.size(), w); }
template<typename I, int N, typename... W>	constexpr auto	make_block(const iblock<I, N> &p, uint32 w0, W...w)						{ return make_block(make_block(p, w0), w...); }
template<typename I, typename... W>			constexpr auto	make_block(I p, uint32 w0, W...w)										{ return make_block(make_block(p, w0), w...); }

template<typename I, int N>					constexpr auto	make_strided_block(const iblock<I, N> &p, uint32 s, uint32 w)			{ return iblock<I, N + 1>(p, s, w); }
template<typename I, int N, typename... W>	constexpr auto	make_strided_block(const iblock<I, N> &p, uint32 s0, uint32 w0, W...w)	{ return make_strided_block(make_strided_block(p, s0, w0), w...); }
template<typename I, typename... W>			constexpr auto	make_strided_iblock(I p, uint32 w0, W...w)								{ return make_strided_block(make_block(p, w0), w...); }

template<typename I> iblock<I, 2> flip_vertical(const iblock<I, 2> &b) {
	return make_strided_block(b[b.template size<2>() - 1].begin(), b.template size<1>(), -b.template pitch<2>(), b.template size<2>());
}

template<typename I, int N>			constexpr size_t	data_size(const iblock<I, N> &b)			{ return b.size() * b.pitch(); }
template<typename I, int N>			constexpr size_t	num_elements_total(const iblock<I, N> &b)	{ return b.size() * num_elements_total(b[0]); }
template<typename I>				constexpr size_t	num_elements_total(const iblock<I, 1> &b)	{ return b.size(); }
template<int D, typename I, int N>	constexpr bool		can_flatten(const iblock<I, N> &b)			{ return b.template pitch<D>() == b.template pitch<D - 1>() * b.template size<D - 1>(); }

template<int D, int N>	struct flatten_s		{ template<typename I> static constexpr auto f(const iblock<I, N> &b) { return make_strided_block(flatten_s<D, N - 1>::f(b[0]), b.pitch(), b.size()); }};
template<int D>			struct flatten_s<D, D>	{ template<typename I> static constexpr auto f(const iblock<I, D> &b) { return make_block<I, D - 2>(b, b.template size<D - 1>() * b.size()); }};
template<>				struct flatten_s<1, 1>	{ template<typename I> static constexpr auto f(const iblock<I, 1> &b) { return b; }};
template<>				struct flatten_s<2, 2>	{ template<typename I> static constexpr auto f(const iblock<I, 2> &b) { return make_block(b.template get<1>().begin(), b.template size<1>() * b.template size()); }};
template<int D, typename I, int N> constexpr auto flatten(const iblock<I, N> &b)		{ return flatten_s<D, N>::f(b); }

template<int D, int N>	struct skip_s			{ template<typename I> static constexpr auto f(const iblock<I, N> &b, int s) { return make_strided_block(skip_s<D, N - 1>::f(b[0], s), b.pitch(), b.size()); }};
template<int D>			struct skip_s<D, D>		{ template<typename I> static constexpr auto f(const iblock<I, D> &b, int s) { return make_strided_block(b, b.pitch() * s, b.size() / s); }};
template<>				struct skip_s<1, 1>		{ template<typename I> static constexpr auto f(const iblock<I, 1> &b, int s) { return make_strided_iblock(b.begin(), b.pitch() * s, 1, b.size() / s); }};
template<int D, typename I, int N> constexpr auto skip(const iblock<I, N> &b, int s)	{ return skip_s<D, N>::f(b, s); }

template<typename T, typename V, int N> void fill(iblock_iterator<T, N> s, iblock_iterator<T, N> e, const V &v) {
	for (; s != e; ++s)
		fill(*s, v);
}

template<typename S, typename D, int N> void copy(iblock_iterator<S, N> s, iblock_iterator<S, N> e, iblock_iterator<D, N> d) {
	for (; s != e; ++s, ++d)
		copy(*s, *d);
}
template<typename S, typename D, int N> void copy(const iblock<S, N> &s, D &&d)	{ copy(unconst(s), d); }
template<typename S, typename D, int N> void copy(iblock<S, N> &&s, D &&d)		{ copy(s, d); }

template<typename I, typename F> void over(iblock<I, 1>& b, F &&f) {
	for (auto &i : b)
		f(i);
}
template<typename I, typename F, int N> void over(iblock<I, N> &&b, F &&f)	{
	for (auto i : b)
		over(i, f);
}

template<typename I, typename F> auto transform(const iblock<I, 1>& b, F &&f) {
	return make_block(transform(b.begin(), forward<F>(f)), b.size());
}

template<typename I, int N, typename F> auto transform(const iblock<I, N>& b, F &&f) {
	return make_block(transform(*b.begin(), forward<F>(f)), b.size());
}

//-----------------------------------------------------------------------------
//	block - view onto supplied memory
//-----------------------------------------------------------------------------

template<typename T> struct byte_moveable {
	T	*p;
	byte_moveable(T *p = nullptr) : p(p) {}
	constexpr operator T*()	const	{ return p; }
	auto& operator+=(size_t i)		{ p += i; return *this; }

	friend constexpr T*			begin(const byte_moveable& i)				{ return i.p; }
	friend constexpr T*			begin(byte_moveable&& i)					{ return i.p; }
	friend constexpr int		intra_pitch(const byte_moveable&)			{ return (int)sizeof(T); }
	friend void					intra_move(byte_moveable &i, intptr_t n)	{ i.p = (T*)((uint8*)i.p + n); }
	friend constexpr intptr_t	intra_diff(const byte_moveable &a, const byte_moveable &b)	{ return (uint8*)a.p - (uint8*)b.p; }
};

template<typename T, int N> using block		= iblock<byte_moveable<T>, N>;
template<typename T, int N> using blockptr	= iblockptr<byte_moveable<T>, N>;

template<typename T>				constexpr auto			make_blockptr(T *p)						{ return blockptr<T, 1>(p); }
template<typename T, typename... W>	constexpr auto			make_blockptr(T *p, W...w)				{ return make_blockptr(make_blockptr(p), w...); }
template<typename T, int N>			constexpr auto			make_blockptr(T (*p)[N])				{ return make_blockptr((T*)p, N * sizeof(T)); }

template<typename T>				constexpr block<T, 1>	make_block(T *p, uint32 w)					{ return {p, w}; }
template<typename T, typename... W>	constexpr auto			make_block(T *p, uint32 w0, W...w)			{ return make_block(make_block(p, w0), w...); }
template<typename T, typename... W>	constexpr auto			make_strided_block(T *p, uint32 w0, W...w)	{ return make_strided_block(make_block(p, w0), w...); }
template<typename T, int N, typename H>	constexpr auto		make_block(T (*p)[N], uint32 w, H h)		{ return make_strided_block(make_block((T*)p, w), N * sizeof(T), h); }

template<int N>	struct block_maker {
	template<typename I, int M, typename W> static constexpr auto f (const iblock<I,M> &p, W *w)	{ return block_maker<N - 1>::f(make_block(p, *w), w + 1); }
	template<typename I, int M, typename W> static constexpr auto fs(const iblock<I,M> &p, W *w)	{ return block_maker<N - 1>::fs(make_strided_block(p, w[0], w[1]), w + 2); }
	template<typename I, int M, typename W> static constexpr auto r (const iblock<I,M> &p, W *w)	{ return block_maker<N - 1>::r(make_block(p, w[N - 1]), w); }
};
template<>		struct block_maker<0> {
	template<typename I, int M, typename W> static constexpr auto f (const iblock<I,M> &p, W *w)	{ return p; }
	template<typename I, int M, typename W> static constexpr auto fs(const iblock<I,M> &p, W *w)	{ return p; }
	template<typename I, int M, typename W> static constexpr auto r (const iblock<I,M> &p, W *w)	{ return p; }
};
template<int N, typename I, typename W>		constexpr iblock<I, N>	make_block(I p, W *w)			{ return block_maker<N - 1>::f(make_block(p, *w), w + 1); }
template<int N, typename I, typename W>		constexpr iblock<I, N>	make_strided_block(I p, W *w)	{ return block_maker<N - 1>::fs(make_block(p, *w), w + 1); }
template<int N, typename I, typename W>		constexpr iblock<I, N>	make_block_rev(I p, W *w)		{ return block_maker<N - 1>::r(make_block(p, w[N - 1]), w); }

template<typename U, typename T>		constexpr block<U, 1>	element_cast(const block<T, 1> &b)			{ return block<U, 1>((U*)b.begin(), b.size()); }
template<typename U, typename T, int N> constexpr block<U, N>	element_cast(const block<T, N> &b)			{ return block<U, N>(element_cast<U>(b[0]), b.pitch(), b.size()); }
template<typename U, typename T, int N> constexpr block<U, N>	element_cast(block<T, N> &b)				{ return element_cast<U>(make_const(b)); }
template<typename U, typename T, int N> constexpr block<U, N>	element_cast(block<T, N> &&b)				{ return element_cast<U>(make_const(b)); }

//-----------------------------------------------------------------------------
//	auto_block - owns memory
//-----------------------------------------------------------------------------

template<typename T, int N> struct auto_block : block<T, N> {
	typedef block<T, N> B;

	auto_block()							{ B::buffer = nullptr; }
	auto_block(auto_block &&b) : B(b)		{ b.buffer = 0; }
	auto_block& operator=(auto_block &&b)	{ swap(*this, b); return *this; }
	~auto_block()							{ delete[] B::buffer; }
	B&			get() 						{ return *this; }
	const B&	get() const					{ return *this; }
    B			detach() 					{ block<T, N> b = *this; B::buffer = 0; return b; }
	void		clear()						{ delete[] B::buffer; B:buffer = 0; }
	friend void swap(auto_block &a, auto_block &b) { raw_swap((B&)a, (B&)b); }

	template<typename U> friend constexpr block<U, N>	element_cast(const auto_block &b)	{ return element_cast<U>(static_cast<const B&>(b)); }
	template<typename U> friend constexpr block<U, N>	element_cast(auto_block &b)			{ return element_cast<U>(make_const(b)); }
	template<typename U> friend constexpr block<U, N>	element_cast(auto_block &&b)		{ return element_cast<U>(make_const(b)); }
};

template<typename T, int N>				constexpr auto	_make_auto_block(block<T, N> &&p)	{ return force_cast<auto_block<T,N>>(p); }
template<typename T, typename... W>		inline auto		make_auto_block(W... w)				{ return _make_auto_block(make_block(new T[prod(w...)], w...)); }
template<typename T, int N, typename W>	constexpr auto	make_auto_block(W *w)				{ return _make_auto_block(make_block(new T[prod<N>], w));}
template<typename T, int N, typename W>	constexpr auto	make_auto_block_rev(W *w)			{ return _make_auto_block(make_block_rev(new T[prod<N>], w)); }

template<typename T, typename S, typename... W>			inline auto _make_auto_block_using(const S &p, W... w)				{ return make_block(new T[prod(w...)], w...); }
template<typename T, typename S, int N, typename... W>	inline auto _make_auto_block_using(const iblock<S,N> &p, W... w)	{ return _make_auto_block_using<T>(p[0], p.size(), w...); }
template<typename T, int N, typename... W>				inline auto	make_auto_block_using(const block<T,N> &p, W... w)		{ return _make_auto_block(_make_auto_block_using<T>(p, w...)); }
template<typename T, typename S, int N, typename... W>	inline auto	make_auto_block_using(const iblock<S,N> &p, W... w)		{ return _make_auto_block(_make_auto_block_using<T>(p, w...)); }
template<typename T, typename... W>						inline auto	make_auto_block_using(const T &t, W... w)				{ return make_auto_block<T>(w...); }

template<typename D, typename S, int N>	auto_block<D,N>	to(const iblock<S, N> &b) {
	auto	b2 = make_auto_block_using<D>(b);
	copy(b, b2.get());
	return b2;
}

template<typename D, typename S, int N>	enable_if_t<!same_v<S,D>, auto_block<D,N>>			to(const block<S, N> &b) {
	auto	b2 = make_auto_block_using<D>(b);
	copy(b, b2.get());
	return b2;
}
template<typename D, typename S, int N>	enable_if_t<same_v<S,D>, const block<D,N>&>			to(const block<S, N> &b) {
	return b;
}

template<typename D, typename S, int N> struct block_via_s : auto_block<D, N> {
	const iblock<S, N>	&s;
	block_via_s(block_via_s&&) = default;
	block_via_s(const iblock<S, N> &s) : s(s) { auto_block<D, N>::operator=(make_auto_block_using<D, S>(s)); }
	~block_via_s() { copy(this->get(), s); }
};

template<typename D, typename S, int N> block_via_s<D, S, N>	via(const iblock<S, N> &s) { return s; }
template<typename D, typename S, int N> enable_if_t<!same_v<D, S>, block_via_s<D, byte_moveable<S>, N>>	via(const block<S, N> &s) { return s; }
template<typename D, typename S, int N> enable_if_t<same_v<D, S>, const block<S, N>&>		via(const block<S, N> &s) { return s; }

template<typename A, typename B, int N>	void	swap(iblock<A, N> &&a, iblock<B, N> &&b) {
	auto	ai = a.begin();
	auto	bi = b.begin();
	for (int i = a.size(); i--; ++ai, ++bi)
		swap(*ai, *bi);
}

//-----------------------------------------------------------------------------
//	algorithms
//-----------------------------------------------------------------------------

template<class I, class F>						inline F for_each(I i, I end, F f);
template<class I1, class I2, class F>			inline F for_each2(I1 i1, I1 end, I2 i2, F f);
template<class I1, class I2, class I3, class F>	inline F for_each3(I1 i1, I1 end, I2 i2, I3 i3, F f);

template<typename T1, int N, class F> F for_each(iblock_iterator<T1, N> i1, iblock_iterator<T1, N> end, F f) {
	for (; i1 != end; ++i1)
		for_each(i1.begin(), i1.end(), f);
	return f;
}
template<typename T1, typename T2, int N, class F> F for_each2(iblock_iterator<T1, N> i1, iblock_iterator<T1, N> end, iblock_iterator<T2, N> i2, F f) {
	for (; i1 != end; ++i1, ++i2)
		for_each2(*i1, *i2, f);
	return f;
}
template<typename T1, typename T2, typename T3, int N, class F> F for_each3(iblock_iterator<T1, N> i1, iblock_iterator<T1, N> end, iblock_iterator<T2, N> i2, iblock_iterator<T3, N> i3, F f) {
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

template<int X, typename D, typename S> void decode_blocked(const S &s, D& d) {
	for (uint32 x = 0; x < s.size(); x++)
		s[x].Decode(d.slice(x * X, X));
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
template<int X, typename D,typename S> void encode_blocked(const S &s, D& d) {
	for (uint32 x = 0; x < d.size(); x++)
		d[x].Encode(s.slice(x * X, X));
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
