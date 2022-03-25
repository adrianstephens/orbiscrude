#ifndef STREAM_H
#define STREAM_H

#include "base/defs.h"
#include "base/functions.h"

#ifdef PLAT_WINRT
#include "winrt/coroutine.h"
#include "winrt/Windows.Storage.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/Windows.Storage.AccessCache.h"
#include "base/hash.h"
#endif

#undef small

#ifndef PLAT_WII
#include <stdio.h>
#include <fcntl.h>
#endif

#ifdef PLAT_WIN32
#include "io.h"
#endif

#ifndef EOF
#define	EOF			(-1)
#endif

namespace iso {

typedef int64		streamptr;

template<typename T> struct T_terminated : T_inheritable<T>::type {
	template<typename T2> T_terminated(const T2 &t2) : T_inheritable<T>::type(t2)	{}
	friend const T& get(const T_terminated<T> &t) { return t; }
	template<typename W>	bool	write(W w)		{ w.template write<T>(*this); w.putc(0); }
};
template<typename T> inline T_terminated<T> terminated(const T &t) { return t; }

//-----------------------------------------------------------------------------
//	defaults
//-----------------------------------------------------------------------------

template<typename S> struct stream_defaults {
	bool		eof()								{ return static_cast<S*>(this)->tell() >= static_cast<S*>(this)->length(); }
	bool		exists()					const	{ return true;	}
	streamptr	tell()						const	{ return -1;	}
	streamptr	length()							{ return stream_length(static_cast<S*>(this)); }
	void		seek(streamptr offset)				{}
	void		seek_cur(streamptr offset)			{ static_cast<S*>(this)->seek(static_cast<S*>(this)->tell() + offset); }
	void		seek_end(streamptr offset)			{ static_cast<S*>(this)->seek(static_cast<S*>(this)->length() + offset); }
	S&			_clone()							{ return *static_cast<S*>(this); }
};

template<typename S> struct stream_defaults<const S> {
	bool		eof()						const	{ return static_cast<const S*>(this)->tell() >= static_cast<const S*>(this)->length(); }
	bool		exists()					const	{ return true;	}
	streamptr	tell()						const	{ return -1;	}
	streamptr	length()					const	{ return stream_length(static_cast<const S*>(this)); }
	void		seek(streamptr offset)		const	{}
	void		seek_cur(streamptr offset)	const	{ static_cast<const S*>(this)->seek(static_cast<const S*>(this)->tell() + offset); }
	void		seek_end(streamptr offset)	const	{ static_cast<const S*>(this)->seek(static_cast<const S*>(this)->length() + offset); }
	const S&	_clone()					const	{ return *static_cast<const S*>(this); }
};

//-----------------------------------------------------------------------------
//	reader_mixin
//-----------------------------------------------------------------------------

template<class R, typename T>	inline bool read(R &r, T &t);
template<class R, typename T, typename... TT> bool read(R &r, T &t, TT&... tt) {
	return read(r, t) && read(r, tt...);
}
template<class R> inline bool check_readbuff(R &r, void *buffer, size_t size) {
	return r.readbuff(buffer, size) == size;
}

template<class R> inline size_t readbuff_all(R &r, void *buffer, size_t size) {
	char *p = (char*)buffer;
	for (size_t result; size && (result = r.readbuff(p, size)); size -= result)
		p += result;
	return p - (char*)buffer;
}

template<class R> inline malloc_block read_all(R &r) {
	r.seek(0); return malloc_block(r, r.length());
}

template<class R, typename T, typename=void> static constexpr int	read_type_v = 1;//(int)is_reader_v<R>;
template<class R, typename T> static constexpr int read_type_v<R,T,void_t<decltype(declval<T>().read(declval<R>()))>>			= 2;
#ifndef PLAT_MSVC
template<class R, typename T> static constexpr int read_type_v<R,T,void_t<decltype(custom_read(declval<R>(), declval<T>()))>>	= 3;
#endif

template<class R, typename T> static constexpr bool has_read = read_type_v<R,T> == 2;

template<int N>	struct read_s;

template<>	struct read_s<1> {
	template<class R, typename T> static inline bool f(R &r, T &t)				{ return r.readbuff(&t, sizeof(T)) == sizeof(T);	}
	template<class R, typename T> static inline bool f(R &r, T *t, size_t n)	{ return r.readbuff(t, sizeof(T) * n) == sizeof(T) * n;	}
};
template<>	struct read_s<2> {
	template<class R, typename T> static inline bool f(R &r, T &t)				{ return t.read(r); }
	template<class R, typename T> static inline bool f(R &r, T *t, size_t n)	{ bool ret = true; for (int i = 0; i < n && (ret = t[i].read(r)); i++) {} return ret; }
};
template<>	struct read_s<3> {
	template<class R, typename T> static inline bool f(R &r, T &t)				{ return custom_read(r, t); }
	template<class R, typename T> static inline bool f(R &r, T *t, size_t n)	{ bool ret = true; for (int i = 0; i < n && (ret = custom_read(r, t[i])); i++) {} return ret; }
};

template<class R, typename T>			inline bool	read(R &r, T &t)				{ return read_s<read_type_v<R,T>>::f(r, t);		}
template<class R, typename T, int N>	inline bool	read(R &r, T (&t)[N])			{ return read_s<read_type_v<R,T>>::f(r, t, N);	}
template<class R, typename T>			inline bool	readn(R &r, T *t, size_t n)		{ return read_s<read_type_v<R,T>>::f(r, t, n);	}
template<class R, typename T>			inline bool	readnx1(R &r, T *t, size_t n)	{ bool ret = true; while (n-- && (ret = read(r, *t++))) {}; return ret; }

template<class R, typename T> inline enable_if_t< is_trivially_destructible_v<T>, bool>	read_new_n(R &r, T *t, size_t n)	{ return read_s<read_type_v<R,T>>::f(r, t, n);	}
template<class R, typename T> inline enable_if_t<!is_trivially_destructible_v<T>, bool>	read_new_n(R &r, T *t, size_t n)	{ fill_new_n(t, n); return read_s<read_type_v<R,T>>::f(r, t, n); }

template<typename R, typename T> struct reader_iterator {
	using iterator_category = input_iterator_t;
	R			r;
	streamptr	offset;
	reader_iterator(R &&r, streamptr offset) : r(forward<R>(r)), offset(offset) {}
	T					operator*()	const	{ return r.template get<T>(); }
	reader_iterator&	operator++()		{ offset = r.tell(); return *this; }
	bool				operator!=(const reader_iterator&b)	{ return offset != b.offset; }
};

template<typename T, typename U> size_t num_elements(const range<reader_iterator<T, U>> &r) {
	return (r.end().offset - r.begin().offset) / sizeof(U);
}

template<class S> struct reader {
	S&							me()			{ return *static_cast<S*>(this); }
	template<typename T>bool	read(T &t)		{ using iso::read; return read(me(), t); }
	template<typename T>T		get()			{ T t; ISO_VERIFY(read(t)); return t; }
	getter<reader>				get()			{ return *this;	}
	void						align(int a)	{ me().seek((me().tell() + a - 1) / a * a);	}
	uint32						tell32()		{ return uint32(me().tell()); }
	uint32						size32()		{ return uint32(me().length()); }
	size_t						remaining()		{ return me().length() - me().tell(); }
};

template<class S> struct reader<const S> {
	const S&					me()		const { return *static_cast<const S*>(this); }
	template<typename T>bool	read(T &t)	const { using iso::read; return read(me(), t); }
	template<typename T>T		get()		const { T t; ISO_VERIFY(read(t)); return t; }
	getter<const reader>		get()		const { return *this; }
	void						align(int a)const { me().seek((me().tell() + a - 1) / a * a); }
	uint32						tell32()	const { return uint32(me().tell()); }
	uint32						size32()	const { return uint32(me().length()); }
	size_t						remaining()	const { return me().length() - me().tell(); }
};

template<class S> struct reader_mixin : reader<S>, stream_defaults<S> {
	using reader<S>::me;
	int		getc()								{ uint8 c; return me().readbuff(&c, 1) ? c : EOF; }
	size_t	readbuff(void *buffer, size_t size)	{ char *p = (char*)buffer; int i, c; for (i = 0; i < (int)size && (c = me().getc()) != EOF; *p++ = c, i++) {} return i; }
	auto	get_block(size_t size)				{ return malloc_block(*static_cast<S*>(this), size); }

	template<typename U> auto	begin()									{ return reader_iterator<S&, U>(*this, this->tell()); }
	template<typename U> friend auto make_range(const S &r)				{ return range<reader_iterator<S&, U>>({r, r.tell()}, {r, r.length()}); }
	template<typename U> friend auto make_range_n(const S &r, size_t n) { return range<reader_iterator<S&, U>>({r, r.tell()}, {r, r.tell() + n * sizeof(U)}); }
};

template<typename S> struct reader_mixout : S, reader<reader_mixout<S>> {
	template<typename...P> reader_mixout(P&&...p) : S(forward<P>(p)...) {}
	auto			get_block(size_t size)	{ return malloc_block(*this, size); }
	reader_mixout	clone()	{ return S::_clone(); }
	//	using S::S;
};

//-----------------------------------------------------------------------------
//	writer_mixin
//-----------------------------------------------------------------------------

template<class W, typename T>	inline bool write(W &w, const T &t);
template<class W, typename T, typename... TT> bool write(W &w, const T &t, const TT&... tt) {
	return write(w, t) && write(w, tt...);
}
template<class W> inline bool check_writebuff(W &w, const void *buffer, size_t size) { return w.writebuff(buffer, size) == size; }

template<class W, typename T> static meta::array<uint8,1>	write_type_test(...);
template<class W, typename T> static meta::array<uint8,2>	write_type_test(decltype(declval<const T>().write(declval<W>()))*);
template<class W, typename T> static meta::array<uint8,3>	write_type_test(decltype(custom_write(declval<W>(), declval<const T&>()))*);
template<class R, typename T>	static constexpr int write_type_v = sizeof(write_type_test<R,T>(0));

template<int N>	struct write_s;
	
template<>	struct write_s<1> {
	template<class W, typename T> static inline bool f(W &w, const T &t)				{ return w.writebuff(&t, sizeof(T)) == sizeof(T); }
	template<class W, typename T> static inline bool f(W &w, const T *t, size_t n)		{ return n == 0 || w.writebuff(t, sizeof(T) * n) == sizeof(T) * n; }
};
template<>	struct write_s<2> {
	template<class W, typename T> static inline bool f(W &w, const T &t)				{ return t.write(w); }
	template<class W, typename I> static inline bool f(W &w, I i, size_t n)				{ bool ret = true; while (n-- && (ret = i->write(w))) ++i; return ret; }
	template<class W, typename T, int N> static inline bool f(W &w, const T (&t)[N])	{ return f(w, t, N); }
};
template<>	struct write_s<3> {
	template<class W, typename T> static inline bool f(W &w, const T &t)				{ return custom_write(w, t); }
	template<class W, typename I> static inline bool f(W &w, I i, size_t n)				{ bool ret = true; while (n-- && (ret = custom_write(w, *i))) ++i; return ret; }
	template<class W, typename T, int N> static inline bool f(W &w, const T (&t)[N])	{ return f(w, t, N); }
};

template<class W, typename T>			inline bool write(W &w, const T &t)					{ return write_s<write_type_v<W,T>>::f(w, t); }
template<class W, typename T>			inline bool writen(W &w, const T *t, size_t n)		{ return write_s<write_type_v<W,T>>::f(w, t, n); }
template<class W, typename I>			inline bool writen(W &w, I i, size_t n)				{ return write_s<write_type_v<W,it_element_t<I>>>::f(w, i, n); }
template<class W, typename T, int N>	inline bool write(W &w, const T (&t)[N])			{ return write_s<write_type_v<W,T>>::f(w, t); }

template<class W, typename T>			inline bool global_write(W &w, const T &t)			{ return write(w, t); }
template<class W, int N>				inline bool	global_write(W &w, const char (&s)[N])	{ return w.writebuff(s, N - 1) == N - 1; }

template<class S> struct writer {
	S&							me()							{ return *static_cast<S*>(this); }
	template<typename T>bool	write(const T &t)				{ return global_write(me(), t); }
	void						align(int a, char pad = 0x55)	{ while (me().tell() % a) me().putc(pad);	}
	uint32						tell32()						{ return uint32(me().tell()); }
	uint32						size32()						{ return uint32(me().length()); }
};

template<class S> struct writer<const S> {
	const S&					me()							const { return *static_cast<const S*>(this); }
	template<typename T>bool	write(const T &t)				const { return global_write(me(), t); }
	void						align(int a, char pad = 0x55)	const { while (me().tell() % a) me().putc(pad);	}
	uint32						tell32()						const { return uint32(me().tell()); }
	uint32						size32()						const { return uint32(me().length()); }
};

template<class S> struct writer_mixin : writer<S>, stream_defaults<S> {
	using writer<S>::me;
	int	putc(int c)										{ uint8 v = c; return me().writebuff(&v, 1) ? v : EOF; }
	size_t	writebuff(const void *buffer, size_t size)	{ char *p = (char*)buffer; int i; for (i = 0; i < (int)size && static_cast<S*>(this)->putc(*p++) != EOF; i++) {} return i; }
};

template<typename S> struct writer_mixout : S, writer<writer_mixout<S>> {
	template<typename...P> writer_mixout(P&&...p) : S(forward<P>(p)...) {}
};

//-----------------------------------------------------------------------------
//	readwriter_mixin
//-----------------------------------------------------------------------------

template<class S> struct readwriter : writer<S> {
	using writer<S>::me;
	template<typename T>bool	read(T &t)		{ using iso::read; return read(me(), t);	}
	template<typename T>T		get()			{ T t; read(t); return t;		}
	getter<readwriter>			get()			{ return *this;	}
	size_t						remaining()		{ return me().length() - me().tell(); }
};

template<class S> struct readwriter<const S> : writer<const S> {
	using writer<const S>::me;
	template<typename T>bool	read(T &t)		const { using iso::read; return read(me(), t);	}
	template<typename T>T		get()			const { T t; ISO_VERIFY(read(t)); return t;		}
	getter<readwriter>			get()			const { return *this;	}
	size_t						remaining()		const { return me().length() - me().tell(); }
};

template<class S> struct readwriter_mixin : readwriter<S>, stream_defaults<S> {
	using readwriter<S>::me;
	int		getc()										{ uint8 c; return me().readbuff(&c, 1) ? c : EOF; }
	size_t	readbuff(void *buffer, size_t size)			{ char *p = (char*)buffer; size_t i, c; for (i = 0; i < size && (c = me().getc()) != EOF; *p++ = c, i++) {} return i; }
	auto	get_block(size_t size)						{ return malloc_block(me(), size); }

	int		putc(int c)									{ uint8 v = c; return me().writebuff(&v, 1) ? v : EOF; }
	size_t	writebuff(const void *buffer, size_t size)	{ char *p = (char*)buffer; size_t i; for (i = 0; i < size && me().putc(*p++) != EOF; i++) {} return i; }
};

template<class S> struct readwriter_mixin<const S> : readwriter<const S>, stream_defaults<const S> {
	using readwriter<const S>::me;
	int		getc()										const { uint8 c; return me().readbuff(&c, 1) ? c : EOF; }
	size_t	readbuff(void *buffer, size_t size)			const { char *p = (char*)buffer; size_t i, c; for (i = 0; i < size && (c = me().getc()) != EOF; *p++ = c, i++) {} return i; }
	auto	get_block(size_t size)						const { return malloc_block(me(), size); }

	int		putc(int c)									const { uint8 v = c; return me().writebuff(&v, 1) ? v : EOF; }
	size_t	writebuff(const void *buffer, size_t size)	const { char *p = (char*)buffer; size_t i; for (i = 0; i < size && me().putc(*p++) != EOF; i++) {} return i; }
};

template<typename S> struct readwriter_mixout : S, readwriter<readwriter_mixout<S>> {
	template<typename...P> readwriter_mixout(P&&...p) : S(forward<P>(p)...) {}
};

//-----------------------------------------------------------------------------
//	readp
//-----------------------------------------------------------------------------

template<class R, typename T> static yesno::yes	has_get_ptr(decltype(T::get_ptr(*(R*)0))*);
template<class R, typename T> static yesno::no	has_get_ptr(...);

template<int N>	struct get_ptr_s {
	template<typename T, class R> static inline const T* f(R &r, size_t n = 1)	{ return r.template get_ptr<T>(n);	}
};
template<>		struct get_ptr_s<sizeof(yesno::yes)> {
	template<typename T, class R> static inline const T* f(R &r)			{ return T::get_ptr(r); }
	template<typename T, class R> static inline const T* f(R &r, size_t n)	{
		if (n) {
			const T *p = T::get_ptr(r);
			while (--n)
				T::get_ptr(r);
			return p;
		}
		return 0;
	}
};

template<typename T, class R> const T*	readp(R &r)				{ return get_ptr_s<sizeof(has_get_ptr<R,T>(0))>::template f<T>(r);	}
template<typename T, class R> const T*	readp(R &r, size_t n)	{ return get_ptr_s<sizeof(has_get_ptr<R,T>(0))>::template f<T>(r, n);	}

template<class R> struct _readp {
	R	&r;
	_readp(R &r) : r(r)	{}
	template<typename T> operator const T*() const	{ return readp<T>(r); }
};

template<class R> _readp<R>	readp(R &r)	{ return r; }

template<class R, typename T> bool read(R &r, const T *&t) { t = readp<T>(r); return true; }

//-----------------------------------------------------------------------------
//	helpers
//-----------------------------------------------------------------------------

template<typename T, typename I = uint32> struct with_size : T {
	with_size() {}
	with_size(T&& t) : T(move(t)) {}
	template<typename R>	bool read(R &&r)		{ return T::read(r, r.template get<I>()); }
	template<typename W>	bool write(W &&w) const	{ return w.write((I)T::size()) && T::write(w); }
};

template<int N, bool A = (N > 1024)> struct byte_buffer {
	uint8		buffer[N];
	operator uint8*()	{ return buffer; }
};

template<int N> struct byte_buffer<N, true> {
	uint8		*buffer;
	byte_buffer() : buffer((uint8*)malloc(N)) {}
	~byte_buffer()		{ free(buffer); }
	operator uint8*()	{ return buffer; }
};

template<class W, class R> streamptr stream_copy(W &&w, R &&r, void *buffer, size_t buffer_size, size_t len = 0) {
	streamptr	total = 0;
	while (size_t read = r.readbuff(buffer, len && buffer_size > len ? len : buffer_size)) {
		auto	written = w.writebuff(buffer, read);
		total	+= written;
		if (len && !(len -= read))
			break;
	}
	return total;
}

template<int N, class W, class R> streamptr stream_copy(W &&w, R &&r, size_t len = 0) {
	return stream_copy(w, r, byte_buffer<N>(), N, len);
}

template<class W, class R> streamptr stream_copy(W &&w, R &&r, size_t len = 0) {
	return stream_copy<0x10000>(w, r, len);
}

template<typename R> void stream_skip(R &r, intptr_t skip)	{
	ISO_ASSERT(skip >= 0);
	while (skip) {
		char	dummy[1024];
		skip -= r.readbuff(dummy, min(skip, 1024));
	}
}

template<typename S> inline streamptr stream_length(S *s) {
	streamptr pos = s->tell();
	s->seek_end(0);
	streamptr len = s->tell();
	s->seek(pos);
	return len;
}

template<typename S> struct check_pos {
	S			&s;
	streamptr	end;
	check_pos(S &s, streamptr end) : s(s), end(end) {}
	~check_pos() { ISO_ASSERT(s.tell() == end); }
};
template<typename S> check_pos<S> make_check_end(S &s, streamptr end)	{ return check_pos<S>(s, end); }
template<typename S> check_pos<S> make_check_size(S &s, streamptr size)	{ return check_pos<S>(s, size); }

template<typename S> struct save_pos {
	S			&s;
	streamptr	savepos;
	save_pos(S &s, streamptr savepos)					: s(s), savepos(savepos) {}
	save_pos(S &s, streamptr savepos, streamptr pos)	: s(s), savepos(savepos) { s.seek(pos); }
	~save_pos() { s.seek(savepos); }
};

template<typename S> save_pos<S> make_skip_end(S &s, streamptr end)		{ return save_pos<S>(s, end); }
template<typename S> save_pos<S> make_skip_size(S &s, streamptr size)	{ return save_pos<S>(s, s.tell() + size); }
template<typename S> save_pos<S> make_save_pos(S &s)					{ return save_pos<S>(s, s.tell()); }
template<typename S> save_pos<S> make_save_pos(S &s, streamptr pos)		{ return save_pos<S>(s, s.tell(), pos); }

//-----------------------------------------------------------------------------
//	buffered_reader / buffered_writer
//	clients only need seek and readbuff/writebuff
//-----------------------------------------------------------------------------

template<int N> class stream_buffer : protected byte_buffer<N> {
protected:
	streamptr	current, buffer_start;
	using byte_buffer<N>::buffer;
public:
	stream_buffer() : current(0), buffer_start(0)	{}
	memory_block		buffered()			{ return memory_block(buffer, current - buffer_start); }
	const_memory_block	buffered()	const	{ return const_memory_block(buffer, current - buffer_start); }
	streamptr			tell()		const	{ return current; }
};

template<int N> class buffered_reader0 : public stream_buffer<N> {
protected:
	streamptr	 buffer_end = 0;
	using stream_buffer<N>::current;
	using stream_buffer<N>::buffer_start;
	template<typename R> size_t	more(R &&reader) {
		size_t read = reader(this->buffer, N);
		buffer_start = buffer_end;
		buffer_end += read;
		return read;
	}
	template<typename R> size_t	buffered_read(R &&reader, void *buffer, size_t size);
	template<typename R> int	buffered_getc(R &&reader) {
		if (current == buffer_end && !more(reader))
			return EOF;
		return this->buffer[current++ - buffer_start];
	}
	bool	buffered_preseek(streamptr offset) {
		if (!between(offset, buffer_start, buffer_end)) {
			current = buffer_start = offset;
			return true;
		}
		current = offset;
		return false;
	}
};

template<int N> template<typename R> size_t buffered_reader0<N>::buffered_read(R &&reader, void *buffer, size_t size) {
	size_t	total = 0;
	while (size) {
		ISO_ASSERT(current >= buffer_start);
		while (current >= buffer_end) {
			if (size > N) {
				size_t	read = reader(buffer, size);
				buffer_start = buffer_end = current += read;
				return total + read;
			}
			if (!more(reader))
				return total;
		}
		size_t	size2 = buffer_end - current;
		if (size2 > size)
			size2 = size;
		memcpy(buffer, this->buffer + int(current - buffer_start), size2);

		total	+= size2;
		current	+= size2;
		buffer	= (uint8*)buffer + size2;
		size	-= size2;
	}
	return total;
}

template<class S, int N> class buffered_reader : public reader<buffered_reader<S, N>>, public buffered_reader0<N> {
	typedef buffered_reader0<N> B;
	S			file;
	auto		reader() { return [this](void *buffer, size_t size) { return file.readbuff(buffer, size); };  }
public:
	template<typename...P> buffered_reader(P&&...p) : file(forward<P>(p)...)	{}

	size_t		readbuff(void *buffer, size_t size)	{ return B::buffered_read(reader(), buffer, size); }
	int			getc()								{ return B::buffered_getc(reader()); }
	void		seek(streamptr offset)				{ if (B::buffered_preseek(offset)) file.seek(offset); }
	void		seek_cur(streamptr offset)			{ seek(B::current + offset); }
	void		seek_end(streamptr offset)			{ seek(file.length() + offset); }
	streamptr	length()							{ return file.length(); }
	bool		eof()				const			{ return false; }
	bool		exists()			const			{ return true; }
};

template<int N> class buffered_writer0 : public stream_buffer<N> {
protected:
	using stream_buffer<N>::current;
	using stream_buffer<N>::buffer_start;
	template<typename W>	size_t	flush(W &&writer) {
		size_t	write = writer(this->buffer, current - buffer_start);
		buffer_start = current;
		return write;
	}
	template<typename W>	size_t	buffered_write(W &&writer, const void *buffer, size_t size);
	template<typename W>	int		buffered_putc(W &&writer, int c) {
		if (current - buffer_start == N)
			flush(writer);
		return this->buffer[current++ - buffer_start] = c;
	}
	template<typename W>	bool	buffered_preseek(W &&writer, streamptr offset) {
		if (!between(offset, buffer_start, current)) {
			flush(writer);
			current = buffer_start = offset;
			return true;
		}
		current = offset;
		return false;
	}
};

template<int N> template<typename W> size_t buffered_writer0<N>::buffered_write(W &&writer, const void *buffer, size_t size) {
	for (size_t i = 0, n; i < size; i += n) {
		if ((n = buffer_start + N - current) == 0) {
			flush(writer);
			n = N;
		}
		if (n >= size - i)
			n = size - i;

		memcpy(this->buffer + int(current - buffer_start), (const char*)buffer + i, n);
		current	+= n;
	}
	return size;
}

template<class S, int N> class buffered_writer : public writer<buffered_writer<S, N>>, public buffered_writer0<N> {
	typedef buffered_writer0<N>	B;
	S			file;
	auto		writer() { return [this](const void *buffer, size_t size) { return file.writebuff(buffer, size); };  }
public:
	template<typename...P> buffered_writer(P&&...p) : file(forward<P>(p)...) {}
	~buffered_writer() { B::flush(writer()); }

	size_t		writebuff(const void *buffer, size_t size)	{ return B::buffered_write(writer(), buffer, size); }
	int			putc(int c)									{ return B::buffered_putc(writer(), c); }
	void		seek(streamptr offset)						{ if (B::buffered_preseek(writer(), offset)) file.seek(offset); }
	void		seek_cur(streamptr offset)					{ seek(B::current + offset); }
	void		seek_end(streamptr offset)					{ seek(file.length() + offset); }
	streamptr	length()									{ return file.length(); }
	bool		eof()				const					{ return false; }
	bool		exists()			const					{ return true; }
};

//-----------------------------------------------------------------------------
//	filereader/filewriter
//-----------------------------------------------------------------------------

#ifdef PLAT_WII

class filereader {
	DVDFileInfo	info;
	streamptr	current, buffer_start;
	char		buffer_space[2048] __attribute__ ((aligned(32)));;
public:
	filereader(const char *filename)	: current(0), buffer_start(-2048) { if (!DVDOpen(filename, &info)) info.length = 0; }
	filereader(const DVDFileInfo &info)	: info(info), current(0), buffer_start(-2048) {}
	~filereader()									{ if (info.length) DVDClose(&info);	}
	bool		exists()							{ return info.length;	}
	streamptr	length()							{ return info.length;	}
	streamptr	tell()								{ return current;		}
	void		seek(streamptr offset)				{ current = offset; }
	void		seek_cur(streamptr offset)			{ current += offset; }
	void		seek_end(streamptr offset)			{ current = info.length + offset; }
	const DVDFileInfo	&_clone() const				{ return info; }

	size_t		readbuff(void *buffer, size_t size) {
		size			= min(size, info.length - current);
		size_t	total	= size;
		while (size) {
			streamptr	offset = current - buffer_start;
			if (offset < 0 || offset >= 2048) {
				buffer_start	= current & ~2047;
				offset			= current - buffer_start;
				if (buffer_start < info.length) {
					size_t	size2	= min(2048, info.length - buffer_start);
					DVDRead(&info, buffer_space, size2 & ~31, buffer_start);
					if (size2 & 31) {
						DVDRead(&info, buffer_space + 2048 - 32, 32, (info.length - 32 + 3) & ~3);
						memcpy(buffer_space + (size2 & ~31), buffer_space + 2048 - ((info.length + 3) & 28), size2 & 31);
					}
				}
			}
			size_t	size2 = min(size_t(2048 - offset), size);
			if ((uint32(buffer) & 0xc0000000) == OS_BASE_UNCACHED) {
				ISO_ASSERT(((uint32(buffer) | uint32(offset) | size2) & 3) == 0);
				for (uint32 *d = (uint32*)buffer, *s = (uint32*)(buffer_space + int(offset)), *e = d + size2 / 4; d < e; *d++ = *s++);
			} else {
				memcpy(buffer, buffer_space + int(offset), size2);
			}

			current	+= size2;
			buffer	= (char*)buffer + size2;
			size	-= size2;
		}
		return total;
	}
};

#define HAS_FILE_READER

#elif defined PLAT_WINRT

} //namespace iso

namespace iso_winrt {
using namespace Windows::Storage;
using namespace Streams;
class file {
protected:
	struct file_map {
		hash_map<hstring, hstring> map;
		ptr<IStorageFile> get_file(hstring filename) const {
			hstring token = map[filename];
			return wait(AccessCache::StorageApplicationPermissions::FutureAccessList->GetItemAsync(token));
		}
		void add_file(ptr<IStorageFile> file) {
			auto token = AccessCache::StorageApplicationPermissions::FutureAccessList->Add(file);
			map[file->Path()] = token;
		}
	};
	static file_map& get_map() {
		static file_map map;
		return map;
	}

	ptr<IRandomAccessStream>	stream;
public:
	static void add_access(ptr<IStorageFile> file) {
		get_map().add_file(file);
	}

	file(ptr<IRandomAccessStream> stream) : stream(stream) {}
	bool		eof()						{ return stream->Position >= stream->Size; }
	bool		exists()					{ return !!stream; }
	streamptr	length()					{ return stream->Size; }
	void		seek(streamptr offset)		{ stream->Seek(offset); }
	auto		_clone() const				{ return stream; }
};

class filereader0 : public file {
public:
//	filereader0(const char *filename)				: file(wait(get_map().get_file(str(filename))->OpenAsync(FileAccessMode::Read))) {}
	filereader0(const char *filename)				: file(wait(wait(StorageFile::GetFileFromPathAsync(hstring(str(filename))))->OpenAsync(FileAccessMode::Read))) {}
	filereader0(ptr<IRandomAccessStream> stream)	: file(stream) {}

	size_t		readbuff(void *buffer, size_t size) {
		auto		temp2	= wait(stream->ReadAsync(ref_new<Buffer>((uint32)size), (uint32)size, InputStreamOptions::None));
		auto		temp3	= Buffer::CreateMemoryBufferOverIBuffer(temp2);
		BYTE		*bytes;
		unsigned	byte_count;
		((ptr<Platform::IMemoryBufferByteAccess>)temp3->CreateReference())->GetBuffer(&bytes, &byte_count);
		memcpy(buffer, bytes, byte_count);
		return byte_count;
	}
};

class filewriter0 : public file {
public:
	filewriter0(const char *filename)				: file(wait(get_map().get_file(str(filename))->OpenAsync(FileAccessMode::ReadWrite))) {}
	filewriter0(ptr<IRandomAccessStream> stream)	: file(stream) {}

	size_t		writebuff(void *buffer, size_t size) {
		auto		mem		= ref_new<Platform::IMemoryBuffer>();
		BYTE		*bytes;
		unsigned	byte_count;
		((ptr<Platform::IMemoryBufferByteAccess>)mem->CreateReference())->GetBuffer(&bytes, &byte_count);
		memcpy(bytes, buffer, byte_count);

		stream->WriteAsync(Buffer::CreateCopyFromMemoryBuffer(mem));
		return byte_count;
	}
};

#define HAS_FILE_READER
#define HAS_FILE_WRITER
} //namespace iso_winrt

namespace iso {

using filereader = buffered_reader<iso_winrt::filereader0, 1024>;
using filewriter = buffered_writer<iso_winrt::filewriter0, 1024>;

#else

#ifdef PLAT_WIN32
#define fdopen _fdopen
#endif

class file {
protected:
	mutable FILE *h;
public:
	file()							: h(0)				{}
	file(file &&b)					: h(exchange(b.h, nullptr))	{}
	file(FILE *h)					: h(h)				{}
	file(int fd, const char *mode)	: h(0)				{ open(fd, mode); }
	file(const char *filename,const char *mode) : h(0)	{ open(filename, mode); }
	~file()												{ close(); }

	bool		close() {
		FILE *t = h;
		h = 0;
		return t && fclose(t) == 0;
	}
	bool		open(int fd, const char *mode) {
		close();
		return !!(h = fdopen(fd, mode));
	}
	bool		open(const char *filename, const char *mode) {
		close();
		if (!filename)
			return false;
#ifdef PLAT_WIN32
		return fopen_s(&h, filename, mode) == 0;
#else
		return !!(h = fopen(filename, mode));
#endif
	}
	bool		open(FILE *_h)	{ close(); return !!(h = _h); }
	bool		exists()		{ return h != 0; }
	bool		eof()			{ return !!feof(h); }
//	bool		eof()			{ return !!_eof(fileno(h)); }

	void		_seek(streamptr offset, int origin) {
		if (!h)
			return;
#ifdef PLAT_WIN32
		_fseeki64(h, offset, origin);
#elif defined(PLAT_MAC) || defined(PLAT_IOS)
		fseeko(h, offset, origin);
#else
		fseek(h, offset, origin);
#endif
	}
	streamptr	tell() {
		if (!h)
			return 0;
#ifdef PLAT_WIN32
		return _ftelli64(h);
#elif defined(PLAT_MAC) || defined(PLAT_IOS)
		return ftello(h);
#else
		return ftell(h);
#endif
	}
	streamptr	length()					{ return stream_length(this); }

	void		seek(streamptr offset)		{ _seek(offset, 0); }
	void		seek_cur(streamptr offset)	{ _seek(offset, 1); }
	void		seek_end(streamptr offset)	{ _seek(offset, 2); }

#ifdef PLAT_WIN32
	bool		lock(bool lock = true, bool wait = false) const	{
		if (!h)
			return false;
		return true;
/*
		HANDLE h = (HANDLE)_get_osfhandle(fileno(this->h));
		OVERLAPPED offset =	{0, 0, 0, 0, NULL};
		return lock
			? !!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK + (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY), 0, 1, 0, &offset)
			: !!UnlockFileEx(h, 0, 1, 0, &offset);
*/
	}
	FILE*		clone1(const char *mode) const					{ return _fdopen(_dup(_fileno(h)), mode); }
#else
	bool		lock(bool lock = true, bool wait = false) const	{ return true; }
	FILE*		clone1(const char *mode) const					{ return fdopen(::dup(fileno(h)), mode); }
#endif
};

class filereader : public file {
public:
	filereader()												{}
	filereader(const char *filename)	: file(filename, "rb")	{}
	filereader(FILE *h)					: file(h)				{}
	bool		open(int fd)									{ return file::open(fd, "rb"); }
	bool		open(const char *filename)						{ return file::open(filename, "rb"); }
	bool		open(FILE *h)									{ return file::open(h); }
	size_t		readbuff(void *buffer, size_t size)				{ return exists() ? fread(buffer, 1, size, h) : 0; }
	int			getc()											{ return exists() ? fgetc(h) : EOF; }
	FILE		*_clone() const									{ return clone1("rb"); }
};

class filewriter : public file {
public:
	filewriter()												{}
	filewriter(const char *filename)	: file(filename, "wb")	{ if (!lock()) close(); }
	filewriter(FILE *h)					: file(h)				{}
	bool		open(int fd)									{ return file::open(fd, "wb"); }
	bool		open(const char *filename)						{ return file::open(filename, "wb"); }
	bool		open(FILE *h)									{ return file::open(h); }
	size_t		writebuff(const void *buffer, size_t size)		{ ISO_ASSERT(buffer); return buffer && exists() ? fwrite(buffer, 1, size, h): 0; }
	int			putc(int c)										{ return exists() ? fputc(c, h) : EOF; }
	FILE*		_clone() const									{ return clone1("wb"); }
};

class fileappender : public file {
public:
	fileappender()												{}
	fileappender(const char *filename)	: file(filename, "ab")	{ if (!lock()) close(); }
	fileappender(FILE *h)				: file(h)				{}
	bool		open(int fd)									{ return file::open(fd, "ab"); }
	bool		open(const char *filename)						{ return file::open(filename, "ab"); }
	bool		open(FILE *h)									{ return file::open(h); }
	size_t		writebuff(const void *buffer, size_t size)		{ ISO_ASSERT(buffer); return buffer && exists() ? fwrite(buffer, 1, size, h): 0; }
	int			putc(int c)										{ return exists() ? fputc(c, h) : EOF; }
	FILE*		_clone() const									{ return clone1("ab"); }
};

class fileinout : public file {
public:
	fileinout()													{}
	fileinout(const char *filename)		: file(filename, "r+")	{ if (!exists()) file::open(filename, "w+"); }
	fileinout(FILE *h)					: file(h)				{}
	bool		open(int fd)									{ return file::open(fd, "r+"); }
	bool		open(const char *filename)						{ return file::open(filename, "r+") || file::open(filename, "w+"); }
	bool		open(FILE *h)									{ return file::open(h); }
	size_t		readbuff(void *buffer, size_t size)				{ return exists() ? fread(buffer, 1, size, h) : 0; }
	int			getc()											{ return exists() ? fgetc(h) : EOF; }
	size_t		writebuff(const void *buffer, size_t size)		{ ISO_ASSERT(buffer); return buffer && exists() ? fwrite(buffer, 1, size, h): 0; }
	int			putc(int c)										{ return exists() ? fputc(c, h) : EOF; }
	FILE*		_clone() const									{ return clone1("r+"); }
};

#define HAS_FILE_READER
#define HAS_FILE_WRITER
#define HAS_FILE_APPENDER
#define HAS_FILE_INOUT

#endif

//-----------------------------------------------------------------------------
//	generic reader/writer adapters
//-----------------------------------------------------------------------------

template<typename T> struct common_ref {
	T	t;
	template<typename T1> common_ref(T1 &&t)	: t(forward<T1>(t))	{}
	bool		eof()						{ return t.eof(); }
	bool		exists()					{ return t.exists(); }
	void		seek(streamptr offset)		{ return t.seek(offset); }
	void		seek_cur(streamptr offset)	{ return t.seek_cur(offset); }
	void		seek_end(streamptr offset)	{ return t.seek_end(offset); }
	streamptr	tell()						{ return t.tell();	}
	streamptr	length()					{ return t.length(); }
	decltype(auto)	_clone()				{ return t._clone(); }
	operator T&()							{ return t; }
};

template<typename T> struct reader_ref : common_ref<T> {
	using common_ref<T>::t;
	template<typename T1> reader_ref(T1 &&t)	: common_ref<T>(forward<T1>(t))	{}
	int			getc()										{ return t.getc(); }
	size_t		readbuff(void *buffer, size_t size)			{ return t.readbuff(buffer, size); }
	auto		get_block(size_t size)						{ return malloc_block(*this, size); }
};
template<typename T> reader_ref<T> make_reader_ref(T &&t) { return forward<T>(t); }

template<typename T> struct writer_ref : common_ref<T> {
	using common_ref<T>::t;
	template<typename T1> writer_ref(T1 &&t) : common_ref<T>(forward<T1>(t))	{}
	int			putc(int c)									{ return t.putc(c); }
	size_t		writebuff(const void *buffer, size_t size)	{ return t.writebuff(buffer, size); }
};
template<typename T> writer_ref<T> make_writer_ref(T &&t) { return forward<T>(t); }

template<typename T> class reader_offset : public reader_ref<T> {
	using reader_ref<T>::t;
	streamptr	offset, end;
public:
	reader_offset(T t, streamptr offset, streamptr end)		: reader_ref<T>(forward<T>(t)), offset(offset), end(end ? end : t.length() - offset) { t.seek(offset); }
	reader_offset(T t, streamptr end)						: reader_ref<T>(forward<T>(t)), offset(t.tell()), end(end)					{}
	reader_offset(T t)										: reader_ref<T>(forward<T>(t)), offset(t.tell()), end(t.length() - offset)	{}

	size_t		readbuff(void *buffer, size_t size) {
		streamptr p = tell();
		if (p + size > end)
			size = size_t(end - p);
		return t.readbuff(buffer, size);
	}
	int				getc()						{ return tell() < end ? t.getc() : EOF; }
	void			seek(streamptr offset)		{ t.seek(offset + this->offset); }
	void			seek_end(streamptr offset)	{ seek(offset + end); }
	streamptr		tell()						{ return t.tell() - offset;	}
	streamptr		length()					{ return end;	}
	bool			eof()						{ return tell() >= end || t.eof(); }

	reader_offset	_clone()					{ return reader_offset(t.clone(), offset, end); }

	template<typename U> friend auto make_range(const reader_offset &r) {
		return range<reader_iterator<T&, U>>({r.t, r.t.tell()}, {r.t, r.offset + r.end});
	}
};

template<typename T> reader_offset<T> make_reader_offset(T &&t, streamptr offset, streamptr end) {
	return {forward<T>(t), offset, end};
}
template<typename T> reader_offset<T> make_reader_offset(T &&t, streamptr end) {
	return {forward<T>(t), end};
}
template<typename T> auto make_reader_offset(T &&t) {
	return forward<T>(t);
}

template<typename T> class writer_offset : public writer_ref<T> {
	using writer_ref<T>::t;
	streamptr	offset;
public:
	writer_offset(T t) : writer_ref<T>(forward<T>(t)), offset(t.tell()) {}
	void		seek(streamptr offset)			{ t.seek(offset + this->offset); }
	streamptr	tell()							{ return t.tell() - offset;	}
	streamptr	length()						{ return t.length() - offset;	}
};

template<typename T> writer_offset<T> make_writer_offset(T &&t) {
	return forward<T>(t);
}

template<typename T> class interleaved_reader : public reader<interleaved_reader<T> >, public reader_ref<T> {
	using reader_ref<T>::t;
	streamptr 	block, offset, stride;
	streamptr	filepos;
public:
	interleaved_reader(T t, streamptr block, streamptr offset, streamptr stride) : reader_ref<T>(forward<T>(t)), block(block), offset(offset), stride(stride), filepos(0) {}
	size_t		readbuff(void *buffer, size_t size) {
		size_t	total = 0;
		while (size) {
			streamptr	sector			= filepos / block;
			streamptr	sector_offset	= filepos - sector * block;

			t.seek(sector * stride + offset + sector_offset);
			size_t	read			= size < block - sector_offset ? size : size_t(block - sector_offset);
			size_t	read_actual		= t.readbuff(buffer, read);

			size	-= read_actual;
			buffer	=  (char*)buffer + read_actual;
			filepos	+= read_actual;
			total	+= read_actual;

			if (read != read_actual)
				break;
		}
		return total;
	}
	void		seek(streamptr offset)				{ filepos = offset; }
	void		seek_cur(streamptr offset)			{ filepos += offset; }
	void		seek_end(streamptr offset)			{ filepos = length() + offset; }
	streamptr	tell()								{ return filepos;	}
	streamptr	length()							{ streamptr len = t.length() - offset; return len / stride * block + min(len % stride, block); }
	interleaved_reader	_clone()					{ return interleaved_reader(t._clone(), block, offset, stride); }
};

template<typename T> interleaved_reader<T> make_interleaved_reader(T &&t, streamptr block, streamptr offset, streamptr stride) {
	return {forward<T>(t), block, offset, stride};
}

template<typename T0, typename T1> class combined_reader : public reader_mixin<combined_reader<T0, T1> > {
	T0			t0;
	T1			t1;
	streamptr	length0;
	streamptr	filepos;
public:
//	combined_reader(combined_reader &&b) : t0(move(b.t0)), t1(move(b.t1)), length0(b.length0), filepos(b.filepos) {}
	template<typename U0, typename U1> combined_reader(U0 &&t0, U1 &&t1) : t0(forward<U0>(t0)), t1(forward<U1>(t1)), length0(t0.length()), filepos(0) {}
	size_t		readbuff(void *buffer, size_t size) {
		size_t	result0 = 0;
		if (filepos < length0) {
			result0		= t0.readbuff(buffer, min(size, length0 - filepos));
			filepos		+= result0;

			if (filepos < length0)
				return result0;

			buffer = (char*)buffer + result0;
			size	-= result0;
			t1.seek(0);
		}

		size_t	result1 = t1.readbuff(buffer, size);
		filepos += result1;
		return result0 + result1;
	}
	void		seek(streamptr offset) {
		filepos = offset;
		if (filepos < length0)
			t0.seek(filepos);
		else
			t1.seek(filepos - length0);
	}
	void		seek_cur(streamptr offset)	{ seek(filepos + offset); }
	void		seek_end(streamptr offset)	{ seek(length() + offset); }
	streamptr	tell()						{ return filepos; }
	streamptr	length()					{ return length0 + t1.length(); }
	combined_reader	_clone()				{ return { t0.clone(), t1.clone() }; }

};

template<typename T0, typename T1> combined_reader<T0, T1> make_combined_reader(T0 &&t0, T1 &&t1) {
	return {forward<T0>(t0), forward<T1>(t1)};
}
//-----------------------------------------------------------------------------
//	nullwriter
//-----------------------------------------------------------------------------

class null_writer : writer_mixin<null_writer> {
	streamptr	p, end;
public:
	null_writer()											{ p = end = 0;}
	void		seek(streamptr offset)						{ p = offset; }
	void		seek_cur(streamptr offset)					{ p += offset; }
	void		seek_end(streamptr offset)					{ p = end + offset; }
	streamptr	tell()										{ return p;	}
	streamptr	length()									{ return end;	}
	int			putc(int c)									{ if (++p > end) end = p; return c & 0xff;}
	size_t		writebuff(const void *buffer, size_t size)	{ if ((p += size) > end) end = p; return size;}
};

//-----------------------------------------------------------------------------
//	byte_reader/byte_writer
//-----------------------------------------------------------------------------

struct byte_reader : reader_mixin<byte_reader> {
	const uint8	*p;
	byte_reader() : p(0)								{}
	byte_reader(const void *p) : p((uint8*)p)			{}
	void		operator=(const void *_p)				{ p = (uint8*)_p; }
	void		set_ptr(const void *_p)					{ p = (uint8*)_p; }
	const_memory_block peek_block(size_t size)	const	{ return const_memory_block(p, size); }
	int			peekc()							const	{ return *p; }
	size_t		readbuff(void *buffer, size_t size)		{ memcpy(buffer, p, size); p += size; return size; }
	int			getc()									{ return *p++; }
	void		skip(int64 size)						{ p += size; }
	void		align(int a)							{ p = iso::align(p, a);	}

	struct _get_ptr {
		byte_reader	&b;
		size_t		n;
		_get_ptr(byte_reader &_b, size_t _n) : b(_b), n(_n)	{}
		template<typename T> operator const T*() const		{ return b.get_ptr<T>(n); }
	};
	template<typename T> const T*	get_ptr(size_t n = 1)	{ const T *t = (const T*)p; p = (const uint8*)(t + n); return t; }
	_get_ptr						get_ptr(size_t n = 1)	{ return _get_ptr(*this, n); }
	const_memory_block				get_block(size_t n)		{ p += n; return const_memory_block(p - n, n); }
};

struct byte_writer : writer_mixin<byte_writer> {
	uint8	*p;
	byte_writer() : p(0)								{}
	byte_writer(void *p) : p((uint8*)p)					{}
	void	operator=(void *_p)							{ p = (uint8*)_p; }
	void	set_ptr(void *_p)							{ p = (uint8*)_p; }
	size_t	writebuff(const void *buffer, size_t size)	{ memcpy(p, buffer, size); p += size; return size; }
	int		putc(int c)									{ return *p++ = c; }

	struct _get_ptr {
		byte_writer	&b;
		size_t		n;
		_get_ptr(byte_writer &b, size_t n) : b(b), n(n)	{}
		template<typename T> operator T*() const			{ return b.get_ptr<T>(n); }
	};
	template<typename T> T*			get_ptr(size_t n = 1)	{ T *t = (T*)p; p = (uint8*)(t + n); return t; }
	_get_ptr						get_ptr(size_t n = 1)	{ return _get_ptr(*this, n); }
	memory_block					get_block(size_t n)		{ p += n; return memory_block(p - n, n); }
};

//-----------------------------------------------------------------------------
//	memory_reader/memory_writer
//-----------------------------------------------------------------------------

class memory_reader : public reader_mixin<memory_reader> {
protected:
	const_memory_block	b;
	const uint8			*p;
	struct _get_ptr {
		memory_reader	&b;
		size_t			n;
		_get_ptr(memory_reader &b, size_t n) : b(b), n(n)	{}
		template<typename T> operator const T*() const			{ return b.get_ptr<T>(n); }
	};
public:
	memory_reader(const const_memory_block &b)	: b(b), p(b) {}
	memory_reader(const void *p, const void *e)	: b(p, e), p((const uint8*)p) {}
	size_t				remaining()				const	{ return (const uint8*)b.end() - p; }
	const_memory_block	peek_block(size_t size)	const	{ return const_memory_block(p, min(size, remaining())); }
	int					peekc()					const	{ return *p; }
	const_memory_block	get_block(size_t size)			{ size = min(size, remaining()); p += size; return const_memory_block(p - size, size); }

	bool			eof()						const	{ return p >= b.end(); }
	streamptr		tell()						const	{ return p - (uint8*)b.p; }
	streamptr		length()					const	{ return b.size(); }
	void			seek(streamptr offset)				{ p = (const uint8*)b.p + min(offset, b.n); }
	void			seek_cur(streamptr offset)			{ p += offset; }
	void			seek_end(streamptr offset)			{ p = (const uint8*)b.end() + offset; }
	int				getc()								{ return eof() ? EOF : *p++; }
	size_t			readbuff(void *buffer, size_t size) { size = min(size, remaining()); memcpy(buffer, p, size); p += size; return size; }

	const void*		get_data(size_t at)			const	{ return b + at; }
	const void*		getp()						const	{ return p; }
	template<typename T> const T* get_ptr(size_t n = 1)	{ const T *t = (const T*)p; p = (const uint8*)(t + n); return t; }
	_get_ptr					get_ptr(size_t n = 1)	{ return {*this, n}; }

	struct _with_len {
		memory_reader		&r;
		const_memory_block	b;
		_with_len(memory_reader &r, uint32 len) : r(r), b(r.b) {
			r.b		= r.b.slice_to(r.tell() + len);
		}
		~_with_len() {
			r.b		= b;
		}
	};
	_with_len	with_len(uint32 len) { return {*this, len}; }
};

class memory_reader_owner : public memory_reader {
public:
	memory_reader_owner(malloc_block &&b) : memory_reader(b.detach()) {}
	~memory_reader_owner()							{ free(unconst((const void*)b)); }
	const const_memory_block&	_clone()	const	{ return b; }
};

class memory_writer : public writer_mixin<memory_writer> {
protected:
	memory_block	b;
	uint8			*p;
	struct _get_ptr {
		memory_writer	&b;
		size_t			n;
		_get_ptr(memory_writer &b, size_t n) : b(b), n(n) {}
		template<typename T> operator T*() const { return b.get_ptr<T>(n); }
	};
public:
	memory_writer(const memory_block &b)	: b(b), p(b) {}
	memory_writer(void *p, void *e)			: b(p, e), p((uint8*)p) {}
	size_t			remaining()		const				{ return (uint8*)b.end() - p; }
	memory_block	get_block(size_t size)				{ size = min(size, remaining()); p += size; return memory_block(p - size, size); }

	bool			eof()			const				{ return p == b.end(); }
	streamptr		tell()			const				{ return p - (uint8*)b.p; }
	streamptr		length()		const				{ return b.size(); }
	void			seek(streamptr offset)				{ p = (uint8*)b.p + min(offset, b.n); }
	void			seek_cur(streamptr offset)			{ p += offset; }
	void			seek_end(streamptr offset)			{ p = (uint8*)b.end() + offset; }
	int				putc(int c)							{ return eof() ?  EOF : (*p++ = c & 0xff); }
	size_t			writebuff(const void *buffer, size_t size) { size = min(size, remaining()); memcpy(p, buffer, size); p += size; return size; }

	void*					get_data(size_t at) const	{ return b + at; }
	void*					getp()				const	{ return p; }
	template<typename T> T*	get_ptr(size_t n = 1)		{ T *t = (T*)p; p = (uint8*)(t + n); return t; }
	_get_ptr				get_ptr(size_t n = 1)		{ return {*this, n}; }

	operator void*()							const	{ return b; }
	operator const_memory_block()				const	{ return const_memory_block(b, tell()); }
};

class dynamic_memory_writer : public writer_mixin<dynamic_memory_writer> {
protected:
	malloc_block	b;
	size_t			p, e;
	struct _get_ptr {
		dynamic_memory_writer	&b;
		size_t					n;
		_get_ptr(dynamic_memory_writer &b, size_t n) : b(b), n(n) {}
		template<typename T> operator T*() const { return b.get_ptr<T>(n); }
	};
public:
	size_t		alloc_offset(size_t n) {
		size_t	p0	= p;
		p			= p0 + n;
		if (p > b.size())
			b.resize(p * 2);
		if (p > e)
			e = p;
		return p0;
	}
	void*	alloc(size_t n) {
		return b + alloc_offset(n);
	}

	dynamic_memory_writer() : p(0), e(0)					{}
	streamptr	tell()								const	{ return p; }
	streamptr	length()							const	{ return e; }
	void		seek(streamptr offset)						{ p = offset; }
	void		seek_cur(streamptr offset)					{ p += offset; }
	void		seek_end(streamptr offset)					{ p = e + offset; }
	int			putc(int c)									{ return (*(uint8*)alloc(1) = c) & 0xff; }
	size_t		writebuff(const void *buffer, size_t size)	{ memcpy(alloc(size), buffer, size); return size; }

	dynamic_memory_writer	_clone()				const	{ return *this; }
	void*					get_data(size_t at)		const	{ return b + at; }
	void*					getp()					const	{ return get_data(p); }
	template<typename T> T*	get_ptr(size_t n = 1)			{ return (T*)alloc(sizeof(T) * n); }
	_get_ptr				get_ptr(size_t n = 1)			{ return {*this, n}; }
	memory_block			get_block(size_t size)			{ return memory_block(alloc(size), size); }

	const_memory_block		data()					const	{ return const_memory_block(b, e); }
	operator malloc_block&&()								{ b.n = min(b.n, e); return move(b); }
};

//-----------------------------------------------------------------------------
//	interfacing wrappers
//-----------------------------------------------------------------------------

class filename;
template<typename T> struct is_filename			: T_false {};
template<> struct is_filename<const char*>		: T_true {};
template<> struct is_filename<const filename&>	: T_true {};
template<> struct is_filename<filename>			: T_true {};

class common_intf {
protected:
	struct vtable {
		bool		(*eof)(void *p);
		bool		(*exists)(void *p);
		streamptr	(*tell)(void *p);
		streamptr	(*length)(void *p);
		void		(*seek)(void *p, streamptr offset);
		void		(*seek_cur)(void *p, streamptr offset);
		void		(*seek_end)(void *p, streamptr offset);
	};

	void			*p;
	const vtable	*vt;

	common_intf() : p(nullptr), vt(nullptr) {}
	common_intf(void *p, const vtable *vt) : p(p), vt(vt) {}

public:
	bool		eof()						const { return vt->eof(p); }
	bool		exists()					const { return p && vt->exists(p); }
	streamptr	tell()						const { return vt->tell(p); }
	streamptr	length()					const { return vt->length(p); }
	void		seek(streamptr offset)		const { return vt->seek(p, offset); }
	void		seek_cur(streamptr offset)	const { return vt->seek_cur(p, offset); }
	void		seek_end(streamptr offset)	const { return vt->seek_end(p, offset); }
};


template<typename I> class ptr_intf : public I {
	using 		I::p;
	void		(*del)(void*);
public:
	ptr_intf()				: del(nullptr)	{}
	ptr_intf(const _none&)	: del(nullptr)	{}
	ptr_intf(const I &b, void (*del)(void*) = nullptr)	: I(b), del(del)		{}
	ptr_intf(ptr_intf &&b)								: I((I&)b), del(b.del)	{ b.p = 0; }
	template<typename T> ptr_intf(T *t, void (*del)(void*) = deleter_fn<T>) : I(t), del(del) {}
	~ptr_intf()									{ if (del) del(p); }
	ptr_intf& operator=(ptr_intf &&b)			{ swap(*this, b); return *this; }
	void		clear()							{ if (del) del(p); p = nullptr; }
	friend void swap(ptr_intf &a, ptr_intf &b)	{ swap(a.p, b.p); swap(a.vt, b.vt); swap(a.del, b.del); }
};

class reader_intf;
class writer_intf;
class readwrite_intf;
typedef ptr_intf<reader_intf>		reader_ptr_intf;
typedef ptr_intf<writer_intf>		writer_ptr_intf;
typedef ptr_intf<readwrite_intf>	readwrite_ptr_intf;

class reader_intf : public common_intf, public reader<const reader_intf> {
	friend class readwrite_intf;
protected:
	struct vtable {
		common_intf::vtable	common;
		int						(*getc)(void *p);
		size_t					(*readbuff)(void *p, void *buffer, size_t size);
		const_memory_block_own	(*get_block)(void *p, size_t size);
		reader_ptr_intf			(*clone)(void *p);
	};
	template<typename T> struct vtable_t {
		static vtable table;
		static reader_ptr_intf	clone(void *p);
	};
	template<typename T> reader_intf(T *t, const vtable &vt = vtable_t<T>::table)	: common_intf(t, &vt.common) {}
public:
	reader_intf()				{}
	reader_intf(_none&)			{}
	reader_intf(reader_intf& b)				: common_intf(b)	{}
	reader_intf(reader_intf&& b)			: common_intf(b)	{}
	reader_intf(const reader_intf& b)		: common_intf(b)	{}

	reader_intf(reader_ptr_intf &t)			: reader_intf((reader_intf&)t)	{}
	reader_intf(const reader_ptr_intf &t)	: reader_intf((reader_intf&)t)	{}
	reader_intf(reader_ptr_intf &&t)		: reader_intf((reader_intf&)t)	{}

	reader_intf(const readwrite_intf &t);
	reader_intf(readwrite_intf &t)			: reader_intf(make_const(t)) {};

	template<typename T, typename=is_reader_t<T>> reader_intf(T &&t)	: reader_intf(__builtin_addressof(t)) {}
	reader_intf& operator=(reader_intf &&b)	{ p = exchange(b.p, nullptr); vt = b.vt; return *this; }

	int			getc()								const { return ((vtable*)vt)->getc(p); }
	size_t		readbuff(void *buffer, size_t size)	const { return ((vtable*)vt)->readbuff(p, buffer, size); }
	const_memory_block_own	get_block(size_t size)	const { return ((vtable*)vt)->get_block(p, size); }
	reader_ptr_intf			clone()					const { return ((vtable*)vt)->clone(p); }
	explicit operator bool()	const				{ return !!p; }
};

template<typename T> reader_intf::vtable reader_intf::vtable_t<T>::table = {
	make_staticfunc1(T, eof),
	make_staticfunc1(T, exists),
	make_staticfunc1(T, tell),
	make_staticfunc1(T, length),
	make_staticfunc1(T, seek),
	make_staticfunc1(T, seek_cur),
	make_staticfunc1(T, seek_end),
	make_staticfunc1(T, getc),
	make_staticfunc1(T, readbuff),
	make_staticfunc1(T, get_block),
	&clone,
};

template<typename T> reader_ptr_intf	reader_intf::vtable_t<T>::clone(void *p)	{ return new T(((T*)p)->_clone()); }
//inline reader_ptr_intf	reader_intf::clone()	const	{ return ((vtable*)vt)->clone(p); }

class writer_intf : public common_intf, public writer<const writer_intf> {
protected:
	struct vtable {
		common_intf::vtable	common;
		int			(*putc)(void *p, int c);
		size_t		(*writebuff)(void *p, const void *buffer, size_t size);
		writer_ptr_intf	(*clone)(void *p);
	};
	template<typename T> struct vtable_t {
		static vtable table;
		static writer_ptr_intf	clone(void *p);
	};
	template<typename T> writer_intf(T *t, const vtable &vt = vtable_t<T>::table)	: common_intf(t, &vt.common) {}
public:
	writer_intf()				{}
	writer_intf(_none&)			{}
	writer_intf(writer_intf& b)				: common_intf(b)	{}
	writer_intf(writer_intf&& b)			: common_intf(b)	{}
	writer_intf(const writer_intf& b)		: common_intf(b)	{}

	writer_intf(writer_ptr_intf &t)			: writer_intf((writer_intf&)t)	{}
	writer_intf(const writer_ptr_intf &t)	: writer_intf((writer_intf&)t)	{}
	writer_intf(writer_ptr_intf &&t)		: writer_intf((writer_intf&)t)	{}

	writer_intf(const readwrite_intf &t);
	writer_intf(readwrite_intf &t)			: writer_intf(make_const(t)) {};

	template<typename T, typename=is_writer_t<T>> writer_intf(T &&t)	: writer_intf(&t) {}
	writer_intf& operator=(writer_intf &&b)	{ p =  exchange(b.p, nullptr); vt = b.vt; return *this; }

	int			putc(int c)									const { return ((vtable*)vt)->putc(p, c); }
	size_t		writebuff(const void *buffer, size_t size)	const { return ((vtable*)vt)->writebuff(p, buffer, size); }
	writer_ptr_intf	clone()									const	{ return ((vtable*)vt)->clone(p); }
	explicit operator bool()								const { return !!p; }
};

template<typename T> writer_intf::vtable writer_intf::vtable_t<T>::table = {
	make_staticfunc1(T, eof),
	make_staticfunc1(T, exists),
	make_staticfunc1(T, tell),
	make_staticfunc1(T, length),
	make_staticfunc1(T, seek),
	make_staticfunc1(T, seek_cur),
	make_staticfunc1(T, seek_end),
	make_staticfunc1(T, putc),
	make_staticfunc1(T, writebuff),
	&clone,
};

template<typename T> writer_ptr_intf	writer_intf::vtable_t<T>::clone(void *p)	{ return new T(((T*)p)->_clone()); }
//inline writer_ptr_intf	writer_intf::clone()	const	{ return ((vtable*)vt)->clone(p); }

class readwrite_intf : public writer_intf, public reader<const readwrite_intf> {
	friend class reader_intf;
	friend class writer_intf;
protected:
	struct vtable {
		writer_intf::vtable	w;
		reader_intf::vtable	r;
	};

	template<typename T> struct vtable_t {
		static vtable	table;
		static writer_ptr_intf	writer_clone(void* p) { return new T(((T*)p)->_clone()); }
		static reader_ptr_intf	reader_clone(void* p) { return new T(((T*)p)->_clone()); }
	};
	template<typename T> readwrite_intf(T *t)	: writer_intf(t, vtable_t<T>::table.w) {}
	readwrite_intf()				{}
public:
	readwrite_intf(_none)			{}
	readwrite_intf(readwrite_intf& b)			: writer_intf((writer_intf&)b)	{}
	readwrite_intf(readwrite_intf&& b)			: writer_intf((writer_intf&)b)	{}
	readwrite_intf(const readwrite_intf& b)		: writer_intf((writer_intf&)b)	{}

	readwrite_intf(readwrite_ptr_intf &t)		: readwrite_intf((readwrite_intf&)t)	{}
	readwrite_intf(const readwrite_ptr_intf &t)	: readwrite_intf((readwrite_intf&)t)	{}
	readwrite_intf(readwrite_ptr_intf &&t)		: readwrite_intf((readwrite_intf&)t)	{}

	template<typename T> readwrite_intf(T &&t)	: readwrite_intf(&t) {}
	readwrite_intf& operator=(readwrite_intf &&b)		{ p =  exchange(b.p, nullptr); vt = b.vt; return *this; }

	int			getc()									{ return ((vtable*)vt)->r.getc(p);					}
	size_t		readbuff(void *buffer, size_t size)		{ return ((vtable*)vt)->r.readbuff(p, buffer, size);	}
	const_memory_block_own	get_block(size_t size)	const { return ((vtable*)vt)->r.get_block(p, size); }
	auto&		_clone()						const	{ return *this; }
	auto		clone()							const	{ return ((vtable*)vt)->w.clone(p); }
	explicit operator bool()					const	{ return !!p; }
};

inline reader_intf::reader_intf(const readwrite_intf &t) : reader_intf(t.p, ((readwrite_intf::vtable*)t.vt)->r)	{}
inline writer_intf::writer_intf(const readwrite_intf &t) : writer_intf(t.p, ((readwrite_intf::vtable*)t.vt)->w)	{}

template<typename T> readwrite_intf::vtable readwrite_intf::vtable_t<T>::table = {
	make_staticfunc1(T,eof),
	make_staticfunc1(T,exists),
	make_staticfunc1(T,tell),
	make_staticfunc1(T,length),
	make_staticfunc1(T,seek),
	make_staticfunc1(T,seek_cur),
	make_staticfunc1(T,seek_end),
	make_staticfunc1(T,putc),
	make_staticfunc1(T,writebuff),
	&writer_clone,
	make_staticfunc1(T,eof),
	make_staticfunc1(T,exists),
	make_staticfunc1(T,tell),
	make_staticfunc1(T,length),
	make_staticfunc1(T,seek),
	make_staticfunc1(T,seek_cur),
	make_staticfunc1(T,seek_end),
	make_staticfunc1(T,getc),
	make_staticfunc1(T,readbuff),
	make_staticfunc1(T,get_block),
	&reader_clone,
};

#if 1
typedef const reader_intf&		istream_ref;
typedef const writer_intf&		ostream_ref;
typedef const readwrite_intf&	iostream_ref;
#else
typedef reader_intf				istream_ref;
typedef writer_intf				ostream_ref;
typedef readwrite_intf			iostream_ref;
#endif

typedef reader_ptr_intf								istream_ptr;
typedef reader_mixout<reader_ref<reader_intf>>		istream_chain;
typedef reader_mixout<reader_offset<reader_intf>>	istream_offset;

typedef writer_ptr_intf								ostream_ptr;
typedef writer_mixout<writer_ref<writer_intf>>		ostream_chain;
typedef writer_mixout<writer_offset<writer_intf>>	ostream_offset;

typedef readwrite_ptr_intf							iostream_ptr;

#ifdef HAS_FILE_READER
typedef reader_mixout<filereader>			FileInput;
#endif
#ifdef HAS_FILE_WRITER
typedef writer_mixout<filewriter>			FileOutput;
#endif
#ifdef HAS_FILE_APPENDER
typedef writer_mixout<fileappender>			FileAppend;
#endif
#ifdef HAS_FILE_INOUT
typedef readwriter_mixout<fileinout>		FileInOut;
#endif

template<typename H> struct hash_stream : writer_mixin<hash_stream<H>> {
	H	h;
	hash_stream()	{}
	hash_stream(H h) : h(forward<H>(h))	{}
	size_t			writebuff(const void *data, size_t size) {
		h(data, size);
		return size;
	}
	auto		terminate()	const		{ return h.terminate(); }
	auto		digest()	const		{ return terminate(); }
	operator auto()			const		{ return terminate(); }
};

} //namespace iso

#endif	// STREAM_H
