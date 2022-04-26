#ifndef	VLC_H
#define	VLC_H

#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
// bit_stack
//-----------------------------------------------------------------------------

template<typename T, bool be> class bit_stack;

template<typename T> class bit_stack<T, true> {
protected:
	T	bit_buffer;
public:
	typedef T		buffer_t;
	inline T		bits_past(int n)const		{ return bit_buffer << n; }
	bit_stack<T, false>	flip(int n)	const		{ return bit_buffer >> (sizeof(T) * 8 - n); }
	int				last_bit()		const		{ return sizeof(T) * 8 - 1 - lowest_set_index(bit_buffer); }

	inline const T&	peek()			const		{ return bit_buffer; }
	inline T		peek(int n)		const		{ return n ? (bit_buffer >> (sizeof(T) * 8 - n)) : 0; }

	inline void		discard(int n)				{ bit_buffer <<= n; }
	inline void		push(T code, int n)			{ bit_buffer = (bit_buffer >> n) | (code << (sizeof(T) * 8 - n)); }

	bit_stack(T t = 0) : bit_buffer(t)	{}
};

template<typename T> class bit_stack<T, false> {
protected:
	T	bit_buffer;
public:
	typedef T		buffer_t;
	inline T		bits_past(int n)const		{ return bit_buffer >> n; }
	bit_stack<T, true> flip(int n)	const		{ return bit_buffer << (sizeof(T) * 8 - n);}
	int				last_bit()		const		{ return highest_set_index(bit_buffer); }

	inline const T&	peek()			const		{ return bit_buffer; }
	inline T		peek(int n)		const		{ return bit_buffer & ((1 << n) - 1); }

	inline void		discard(int n)				{ bit_buffer >>= n; }
	inline void		push(T code, int n)			{ bit_buffer = (bit_buffer << n) | (code & ((1 << n) - 1)); }

	bit_stack(T t = 0) : bit_buffer(t)	{}
};

//-----------------------------------------------------------------------------
// with count
//-----------------------------------------------------------------------------

template<typename T, bool be> class bit_stack_count : public bit_stack<T,be> {
protected:
	enum {MAX_BITS = BIT_COUNT<T> - 7};
	typedef bit_stack<T,be>	B;
	uint8			bits_left = 0;

public:
	void			reset()							{ B::bit_buffer = 0; bits_left = 0; }
	inline void		discard(int n)					{ ISO_ASSERT(bits_left >= n); bits_left -= n; B::discard(n); }
	inline void		extend(int n)					{ bits_left += n; }
	unsigned		bits_held()			const		{ return bits_left; }
	inline bool		can_extend(int n)	const		{ return bits_left + n <= sizeof(T) * 8; }
	inline bool		has_bits(int n)		const		{ return bits_left >= n; }

	template<typename S> void fill(S &file, int n = sizeof(T) * 8 - 7);
	template<typename S> void dump(S &file);
};

template<typename T, bool be> template<typename S> void bit_stack_count<T, be>::fill(S &file, int n) {
	auto	temp = B::flip(bits_left);
	int		c;
	while (bits_left < n && (c = file.getc()) != EOF) {
		temp.push(c, 8);
		bits_left += 8;
	}
	B::operator=(temp.flip(bits_left));
}
template<typename T, bool be> template<typename S> void bit_stack_count<T, be>::dump(S &file) {
	auto	temp = B::flip(bits_left);
	while (bits_left >= 8) {
		bits_left -= 8;
		file.putc(temp.peek(8));
		temp.discard(8);
	}
}

//-----------------------------------------------------------------------------
// with sentinel bit
//-----------------------------------------------------------------------------

template<typename T, bool be> class bit_stack_sentinel : public bit_stack<T,be> {
protected:
	enum {MAX_BITS = BIT_COUNT<T> - 8};
	typedef bit_stack<T,be>	B;

public:
	bit_stack_sentinel()							{ B::push(1, 1); }
	void			reset()							{ B::bit_buffer = 0; B::push(1, 1); }
	inline void		extend(int n)		const		{}
	unsigned		bits_held()			const		{ return B::last_bit(); }
	inline bool		can_extend(int n)	const		{ return B::bits_past(sizeof(T) * 8 - n) == 0; }
	inline bool		has_bits(int n)		const		{ return B::bits_past(n) != 0; }
	
	template<typename S> void fill(S &file, int n = sizeof(T) * 8 - 8);
	template<typename S> void dump(S &file);
};

template<typename T, bool be> template<typename S> void bit_stack_sentinel<T, be>::fill(S &file, int n) {
	int		bits_left	= bits_held();
	auto	temp		= B::flip(bits_left);
	int		c;
	while (bits_left < n && (c = file.getc()) != EOF) {
		temp.push(c, 8);
		bits_left += 8;
	}
	temp.push(1, 1);
	B::operator=(temp.flip(bits_left + 1));
}
template<typename T, bool be> template<typename S> void bit_stack_sentinel<T, be>::dump(S &file) {
	int		bits_left	= bits_held();
	auto	temp		= B::flip(bits_left);
	while (bits_left >= 8) {
		bits_left -= 8;
		file.putc(temp.peek(8));
		temp.discard(8);
	}
	temp.push(1, 1);
	B::operator=(temp.flip(bits_left + 1));
}

//-----------------------------------------------------------------------------
// vlc_in
//-----------------------------------------------------------------------------

template<typename T, typename S> class vlc_in0 : public T {
protected:
	using typename T::buffer_t;
	struct stack_t : T {
		inline auto			get(int n)		{ auto r = T::peek(n); T::discard(n); return r; }
		inline bool			get_bit()		{ return !!get(1); }
		template<typename U> inline U get()	{ uintn<sizeof(U)> t = get(BIT_COUNT<U>); return (U&)t; }
		stack_t(T t) : T(t) {}
	};
	template<int N, typename = void> struct helper_s;

	S					file;

	inline void			check(int n)				{ if (!T::has_bits(n)) T::fill(file, n); }
public:
	typedef T		state_t;
	template<typename T2> vlc_in0(T2 &&t) : file(forward<T2>(t))	{}

	inline auto			peek(int n)					{ check(n); return T::peek(n); }
	inline void			discard(int n)				{ check(n); T::discard(n); }
	inline auto			get(int n)					{ auto r = peek(n); T::discard(n); return r; }

	template<int N> auto get()						{ return helper_s<N>(this).get(); }

	inline bool			get_bit()					{ return !!get(1); }
	template<typename U> inline U get()				{ auto t = get<BIT_COUNT<U>>(); return (U&)t; }
	template<typename U> inline U peek()			{ auto t = peek(BIT_COUNT<U>); return (U&)t; }
	inline void			align(int n)				{ T::discard(T::bits_held() % n); }

	state_t				get_state()		const		{ return *this; }
	void				set_state(const state_t &s)	{ T::operator=(s); }
	stack_t				get_stack(int n)			{ check(n); return *this; }
	inline S&			get_stream()				{ return file; }
//	void				restore_unused()			{ file.seek_cur(-int(T::bits_held() + 7) / 8); T::reset(); }
	void				restore_unused()			{ file.seek_cur(-int(T::bits_held() / 8)); T::reset(); }
	void				seek_bit(streamptr p)		{ file.seek(p / 8); T::reset(); discard(p & 7); }
	void				seek_cur_bit(streamptr p)	{ seek_bit(tell_bit() + p); }
	streamptr			tell_bit()					{ return file.tell() * 8 - T::bits_held(); }
};

template<typename T, typename S> template<int N> struct vlc_in0<T,S>::helper_s<N, enable_if_t<(N <= T::MAX_BITS)>> {
	vlc_in0<T,S>	*v;
	helper_s(vlc_in0<T,S> *v) : v(v) {}
	auto get() { return v->get(N); }
};
template<typename T, typename S> template<int N> struct vlc_in0<T,S>::helper_s<N, enable_if_t<(N > T::MAX_BITS)>> {
	vlc_in0<T,S>	*v;
	helper_s(vlc_in0<T,S> *v) : v(v) {}
	auto get() {
		typedef uint_bits_t<N>	U;

		auto	n0	= v->bits_held(), n1 = (N - n0) & 7;
		U		v0	= v->get(n0);
		U		u	= 0;
		v->get_stream().readbuff(&u, (N - n0) / 8);
		//return (v0 << (N - n0)) | (u << n1) | v->get(n1);
		return v0 | (u << n0) | (U(v->get(n1)) << (N - n1));
	}
};


template<typename T, bool be, typename S=istream_ref> using vlc_in				= vlc_in0<bit_stack_count<T, be>, S>;
template<typename T, bool be, typename S=istream_ref> using vlc_in_sentinel		= vlc_in0<bit_stack_sentinel<T, be>, S>;

template<typename T, bool be, typename S> auto	make_vlc_in(S &&file)	{
	return vlc_in<T,be,S>(forward<S>(file));
}
template<typename B, typename S> auto	make_vlc_in(S &&file, const B &state)	{
	vlc_in0<B, S>	v(forward<S>(file));
	v.set_state(state);
	return v;
}

//-----------------------------------------------------------------------------
// vlc_out
//-----------------------------------------------------------------------------

template<typename T, typename S> class vlc_out0 : public T {
protected:
	using typename T::buffer_t;
	S				file;

	inline void			extend(int n)					{ if (!T::can_extend(n)) T::dump(file); T::extend(n); }
public:
	typedef T		state_t;
	template<typename T2> vlc_out0(T2 &&t) : file(forward<T2>(t))	{}
	inline void			put(buffer_t code, int n)		{ extend(n); T::push(code, n); }
	inline void			put_bit(bool b)					{ put(b, 1); }
	template<typename U> inline void put(const U &u)	{ put((uintn<sizeof(U)>&)u, BIT_COUNT<U>); }

	void				flush(uint8 fill = 0)			{ put(fill, 7); T::dump(file); T::reset(); }
	inline void			align(int n, int off = 0, buffer_t fill = 0)	{ put(fill, (T::bits_held() + sizeof(buffer_t) * 8 - off) % n); }

	state_t				get_state()		const			{ return *this; }
	void				set_state(const state_t &s)		{ T::operator=(s); }
	inline S&			get_stream()					{ return file; }
	streamptr			tell_bit()						{ return file.tell() * 8 + T::bits_held(); }
};

template<typename T, bool be, typename S=ostream_ref> using vlc_out				= vlc_out0<bit_stack_count<T, !be>, S>;
template<typename T, bool be, typename S=ostream_ref> using vlc_out_sentinel	= vlc_out0<bit_stack_sentinel<T, !be>, S>;

template<typename T, bool be, typename S> auto	make_vlc_out(S &&file)	{
	return vlc_out<T,be,S>(forward<S>(file));
}
template<typename B, typename S> auto	make_vlc_out(S &&file, const B &state)	{
	vlc_out0<B, S>	v(forward<S>(file));
	v.set_state(state);
	return v;
}

}	// namespace iso

#endif	//	VLC_H
