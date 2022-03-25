#ifndef TEXT_H
#define TEXT_H

#include "base/strings.h"
#include "stream.h"

#undef NAN

namespace iso {

//-----------------------------------------------------------------------------
//	helpers
//-----------------------------------------------------------------------------

template<typename R> int skip_whitespace(R &r) {
	int	c;
	while ((c = r.getc()) != -1 && is_whitespace(c));
	return c;
}

template<typename R> int skip_chars(R &r, const char_set &set) {
	int	c;
	while ((c = r.getc()) != -1 && set.test(c));
	return c;
}

template<typename R> string	read_token(R &r, const char_set &set, int c = 0) {
	string_builder	b;
	if (c)
		b.putc(c);
	while (set.test(c = r.getc()))
		b.putc(c);
	r.put_back(c);
	return b;
}

template<typename R> char *read_token(R &r, char *dest, const char_set &set, int c = 0) {
	char	*d = dest;
	if (c)
		*d++ = c;
	while (set.test(c = r.getc()))
		*d++ = c;
	r.put_back(c);
	*d = 0;
	return dest;
}

template<typename R> bool read_to(R &r, string_builder &b, int end) {
	for (;;) {
		int c = r.getc();
		if (c < 0)
			return false;

		b.putc(c);
		if (c == end)
			return true;

		if (end == '"' || end == '\'') {
			if (c == '\\')
				r.getc();
		} else switch (c) {
			case '<':
				if (!read_to(r, b, '>')) return false;
				break;
			case '[':
				if (!read_to(r, b, ']')) return false;
				break;
			case '{':
				if (!read_to(r, b, '}')) return false;
				break;
			case '(':
				if (!read_to(r, b, ')')) return false;
				break;
			//case ']': case '}': case ')': throw(mismatch_exception(s.getp()));
			case '"': case '\'':
				if (!read_to(r, b, c)) return false;
				break;
		}
	}
}

template<typename R> string	read_to(R &r, int end) {
	string_builder	b;
	read_to(r, b, end);
	return b;
}

template<typename R> char32 get_escape(R &r) {
	switch (int c = r.getc()) {
		default: return char32(c);
		case 'a': return char32('\a');
		case 'b': return char32('\b');
		case 'f': return char32('\f');
		case 'n': return char32('\n');
		case 'r': return char32('\r');
		case 't': return char32('\t');
		case 'v': return char32('\v');

		case 'x':
			if (is_hex(c = r.getc())) {
				int	v = from_digit(c);
				if (is_hex(c = r.getc()))
					return char32(v * 16 + from_digit(c));
				r.put_back(c);
				return char32(v);
			}
			r.put_back(c);
			return char32('x');

		case 'u': {
			int	v	= 0, i;
			for (i = 0; i < 4 && is_hex(c = r.getc()); i++)
				v = v * 16 + from_digit(c);
			if (i < 4)
				r.put_back(c);
			return char32(i ? v : 'u');
		}
		case 'U': {
			int	v	= 0, i;
			for (i = 0; i < 8 && is_hex(c = r.getc()); i++)
				v = v * 16 + from_digit(c);
			if (i < 8)
				r.put_back(c);
			return char32(i ? v : 'U');
		}
		case '0': case '1': case '2': case '3': {
			int	v = c - '0';
			c = r.getc();
			if (c >= '0' && c <= '7') {
				v = v * 8 + c - '0';
				c = r.getc();
				if (c >= '0' && c <= '7')
					return char32(v * 8 + c - '0');
			}
			r.put_back(c);
			return char32(v);
		}
	}
}

template<typename R> string read_string(R &r, int c) {
	string_builder	b;

	if (c == 0)
		c = r.getc();

	if (c == '"' || c == '\'') {
		int	c2	= r.getc();
		while (c2 != EOF && c2 != c) {
			if (c2 == '\\') {
				if ((c2 = r.getc()) == EOF)
					break;
				if (c != c2)
					b.putc('\\');
			}
			b.putc((char)c2);
			c2		= r.getc();
		}
	} else {
		do {
			b.putc(c);
			c = r.getc();
		} while (is_alphanum(c) || c == '_');
		r.put_back(c);
	}

	return b;
}

//-----------------------------------------------------------------------------
//	number readers
//-----------------------------------------------------------------------------

template<typename R> uint64 read_base(int B, R &r, int c) {
	if (!c)
		c = r.getc();

	uint64	acc	= 0;
	for (uint32 d; is_alphanum(c) && (d = from_digit(c)) < B; c = r.getc())
		acc = acc * B + d;

	r.put_back(c);
	return acc;
}

template<typename R> uint64 read_unsigned(R &r, int c) {
	if (!c)
		c = r.getc();
	if (c == '0') {
		c = r.getc();
		return	c == 'x' || c == 'X' ? read_base(16, r, 0)
			:	c == 'b' || c == 'B' ? read_base(2, r, 0)
			:	read_base(8, r, c);
	}
	return read_base(10, r, c);
}

template<typename R> int64 read_integer(R &r, int c) {
	if (!c)
		c = r.getc();
	return c == '-'
		? -(int64)read_unsigned(r, 0)
		: (int64)read_unsigned(r, c == '+' ? 0 : c);
}

template<typename R> number read_number(R &r, int c) {
	bool			neg		= false;
	number::SIZE	size	= number::SIZE32;
	uint64			acc		= 0;
	int				dp		= 0;
	int				base	= 10;
	bool			whole	= true;

	if (!c)
		c = r.getc();

	if (c == '-' || c == '+') {
		neg	= c == '-';
		c	= r.getc();
	}

	if (c == '0') {
		c = r.getc();
		if (c == '.') {
			if ((c = r.getc()) == '#') {
				char	buff[8], *p = buff;
				while (p < buff + 7 && is_upper(c = r.getc()))
					*p++ = c;
				*p++ = 0;

				if (str(buff) == "INF")
					return number(neg ? -3 : 3, 0, number::NAN);
				if (str(buff) == "QNAN")
					return number(neg ? -1 : 1, 0, number::NAN);
				if (str(buff) == "IND")
					return number(neg ? -2 : 2, 0, number::NAN);
			} else {
				r.put_back(c);
				c = '.';
			}
		} else if (c == 'x' || c == 'X') {
			base = 16;
			c	= r.getc();
		} else if (c == 'b' || c == 'B') {
			base = 2;
			c	= r.getc();
		} else {
			base = 8;
		}

	} else if (c != '.' && !is_digit(c)) {
		return number();
	}

	int	d;
	while (is_alphanum(c) && (d = from_digit(c)) < base) {
		if (acc < 0xffffffffffffffffULL / base)
			acc = acc * base + d;
		else
			++dp;
		c		= r.getc();
	}

	if (c == '.') {
		while (is_alphanum(c = r.getc()) && (d = from_digit(c)) < base) {
			if (acc < 0xffffffffffffffffULL / base) {
				acc = acc * base + d;
				--dp;
			}
		}
		whole	= false;
	}

	if (base == 8)
		dp *= 3;
	else if (base == 16)
		dp *= 4;

	if (base == 10 ? (c == 'e' || c == 'E') : (c == 'p' || c == 'P')) {
		bool	neg		= false;

		c		= r.getc();
		if (c == '-' || c == '+') {
			neg	= c == '-';
			c	= r.getc();
		}
		if (!is_digit(c))
			return number();

		int	exp	= read_base(10, r, c);
		dp		+= neg ? -exp : exp;
		whole	= false;

		c		= r.getc();
	}

	number::TYPE	type	= whole ? number::INT : base == 10 ? number::DEC : number::FLT;
	if (whole) {
		if (c == 'u' || c == 'U') {
			type	= number::UINT;
			c		= r.getc();
		}
		if (c == 'l' || c == 'L') {
			c = r.getc();
			if (c == 'l' || c == 'L') {
				size	= number::SIZE64;
				c		= 0;
			}
		}
	} else {
		if (c == 'f' && c == 'F') {
			size	= number::SIZE32;
			c		= 0;
		} else if (c == 'l' && c == 'L') {
			size	= number::SIZE80;
			c		= 0;
		} else {
			size	= number::SIZE64;
		}
	}

	if (c > 0)
		r.put_back(c);

	return number(neg ? -acc : acc, dp, type, size);
}

//-----------------------------------------------------------------------------
//	text_reader
//-----------------------------------------------------------------------------

template<typename R, int B = 1> struct text_reader {
	R			r;
	int			line_number, col_number;
	int			backup[B];
	int			_getc();

	template<typename R1> text_reader(R1 &&r, int line_number = 1)	: r(forward<R1>(r)), line_number(line_number), col_number(1) { clear(backup); }

	int			num_back()	const	{ int n = 0; while (n < B && backup[n]) n++; return n; }
	streamptr	tell()				{ return r.tell() - num_back(); }
	int			getc();
	int			peekc();
	void		put_back(int c);
	int			get_back();

	bool read_to(const char_set &set, char *p, uint32 n) {
		int		c = r.getc();
		if (c == -1)
			return false;

		char	*e = p + n - 1;
		while (c && c != -1 && !set.test(c)) {
			if (p == e)
				return false;
			*p++ = c;
			c = r.getc();
		}

		*p = 0;
		return true;
	}

	bool read_line(char *p, uint32 n) {
		int	c;
		do c = r.getc(); while (c == '\r' || c == '\n');
		if (c == 0 || c == -1)
			return false;

		char *e = p + n - 1;
		while (c && c != -1 && c != '\n' && c != '\r') {
			if (p == e)
				return false;
			*p++ = c;
			c = r.getc();
		}

		*p = 0;
		return true;
	}

	template<int N>				bool read_to(const char_set &set, char (&p)[N])				{ return read_to(set, p, N); }
	template<int N>				bool read_line(char (&p)[N])								{ return read_line(p, N); }
	template<typename C, int N> bool read_to(const char_set &set, fixed_string<N, C> &s)	{ return read_to(set, s, N); }
	template<typename C, int N> bool read_line(fixed_string<N, C> &s)						{ return read_line(s, N); }

	template<typename C>		bool read_line(alloc_string<C> &s, bool clear = true) {
		C	c;
		do c = getc(); while (c == '\r' || c == '\n');
		if (c == -1)
			return false;

		auto b = build(s, clear);
		while (c && c != -1 && c != '\n' && c != '\r') {
			b.putc(c);
			c = getc();
		}
		return true;
	}
};

template<typename R, int B> text_reader<R, B>	make_text_reader(R&& r) { return forward<R>(r); }
template<typename R>		text_reader<R>		make_text_reader(R&& r) { return forward<R>(r); }


template<typename R, int B> void text_reader<R,B>::put_back(int c) {
	ISO_ASSERT(!backup[B-1]);
	for (int i = B; --i;)
		backup[i] = backup[i - 1];
	backup[0] = c;
	--col_number;
}
template<typename R, int B> int text_reader<R,B>::get_back() {
	int c = backup[0];
	if (c) {
		for (int i = 0; i < B - 1; i++)
			backup[i] = backup[i + 1];
		backup[B - 1] = 0;
	}
	return c;
}

template<typename R, int B> int text_reader<R,B>::_getc() {
	int	c = r.getc();
	if (c == '\r' && (c = r.getc()) != '\n') {
		put_back(c);
		c = '\n';
	}
	if (c == '\n') {
		line_number++;
		col_number = 0;
	}
	return c;
}

template<typename R, int B> int text_reader<R,B>::getc() {
	int c = get_back();
	if (!c)
		c = _getc();
	if (c != -1)
		col_number++;
	return c;
}

template<typename R, int B> int text_reader<R,B>::peekc() {
	if (int c = backup[0])
		return c;
	return backup[0] = _getc();
}

//-----------------------------------------------------------------------------
//	text_writer
//-----------------------------------------------------------------------------

template<typename W> struct text_writer {
	W			w;

	template<typename W1> text_writer(W1 &&w) : w(forward<W1>(w)) {}
	bool		putc(int c)							{ return w.putc(c) != -1; }
	bool		puts(const char *s, size_t len)		{ return writen(w, s, len); }
	bool		puts(const char *s)					{ return !s || check_writebuff(w, s, strlen(s)); }
	template<int N> bool puts(const char (&s)[N])	{ return writen(w, s, N - 1); }

	bool vformat(const char *fmt, va_list valist) {
		stream_accum<W&, 512>(w).vformat(fmt, valist);
		return true;
	}
	bool format(const char *fmt,...) {
		va_list valist;
		va_start(valist, fmt);
		return vformat(fmt, valist);
	}
	template<int N> stream_accum<W&, N>	begin()			{ return w; }
	stream_accum<W&, 512>				begin()			{ return w; }
	template<typename T> auto operator<<(const T &t)	{ return stream_accum<W&, 512>(w, t); }
};

template<typename W> struct line_counter {
	W		w;
	int		line_number;

	template<typename W1> line_counter(W1 &&w) : w(forward<W1>(w)), line_number(1) {}
	int		putc(int c)	{
		if (c == '\n')
			++line_number;
		return w.putc(c);
	}
	size_t	writebuff(const void *buffer, size_t size) {
		for (const char *p = (const char*)buffer, *e = p + size; p != e; ++p) {
			if (*p == '\n')
				++line_number;
		}
		return w.writebuff(buffer, size);
	}
};

//-----------------------------------------------------------------------------
//	text_mode
//-----------------------------------------------------------------------------

struct text_mode {
	enum MODE {ASCII, UTF16_LE, UTF16_BE, UTF8, UNRECOGNISED} mode;
	text_mode(MODE mode) : mode(mode) {}
	operator MODE() const { return mode; }

	template<typename R> static MODE detect(R &r, int *backup);
	template<typename R> int get_char(R &r)									const;
	template<typename W> void put_bom(W &w)									const;
	template<typename W> bool put_char(W &w, int c)							const;
	template<typename W> bool put_string(W &w, const char *s, size_t len)	const;
};

template<typename R> text_mode::MODE text_mode::detect(R &r, int *backup) {
	backup[0]	= 0;
	backup[1]	= 0;
	switch (int c = r.getc()) {
		case 0xef:
			if ((c = r.getc()) == 0xbb) {
				if ((c = r.getc()) == 0xbf)
					return UTF8;
				backup[1] = c;
			}
			backup[0] = 0xbb;
			break;
		case 0xff:
			if ((c = r.getc()) == 0xfe)
				return UTF16_LE;
			backup[0] = c;
			break;
		case 0xfe:
			if ((c = r.getc()) == 0xff)
				return UTF16_BE;
			backup[0] = c;
			break;
		default:
			backup[0] = c;
			if (c == '\n' || c == '\r' || (c >= ' ' && c < 0x80))
				return ASCII;
			break;
	}
	return UNRECOGNISED;
}

template<typename R> int text_mode::get_char(R &r) const {
	int	c = r.getc();
	if (c != -1) switch (mode) {
		case UTF16_LE: {
			int c2 = r.getc();
			if (c2 != -1)
				c |= c2 << 8;
			break;
		}
		case UTF16_BE: {
			int c2 = r.getc();
			if (c2 != -1)
				c = (c << 8) | c2;
			break;
		}
		case UTF8: {
			int	n = 0;
			for (int t = c; t & 0x80; t <<= 1, n++);
			if (n) {
				c &= (127 >> n);
				while (--n)
					c = (c << 6) | (r.getc() & 63);
			}
			break;
		}
		default:
			break;
	}
	return c;
}

template<typename W> void text_mode::put_bom(W &w) const {
	switch (mode) {
		case UTF16_LE:	w.putc(0xff); w.putc(0xfe); break;
		case UTF16_BE:	w.putc(0xfe); w.putc(0xff); break;
		case UTF8:		w.putc(0xef); w.putc(0xbb); w.putc(0xbf); break;
		default:		break;
	}
}

template<typename W> bool text_mode::put_char(W &w, int c) const {
	switch (mode) {
		case UTF16_LE:	return w.write(uint16le(c));
		case UTF16_BE:	return w.write(uint16be(c));
		default:		return w.putc(char(c)) != -1;
	}
}

template<typename W> bool text_mode::put_string(W &w, const char *s, size_t len) const {
	switch (mode) {
		case UTF16_LE: {
			char16le	s2[1024];
			return check_writebuff(w, s2, chars_copy(s2, s, len, num_elements(s2)));
		}
		case UTF16_BE:	{
			char16be	s2[1024];
			return check_writebuff(w, s2, chars_copy(s2, s, len, num_elements(s2)));
		}
		default:
			return check_writebuff(w, s, len);
	}
}

//-----------------------------------------------------------------------------
//	text_mode_reader
//-----------------------------------------------------------------------------

template<typename R> struct text_mode_reader : text_reader<R, 2> {
	typedef text_reader<R, 2>	B;
	text_mode	mode;

	template<typename R1> text_mode_reader(R1 &&r, int line_number = 1)	: text_reader<R, 2>(forward<R1>(r), line_number), mode(text_mode::detect(r, this->backup)) {}
	int			getc();
	int			peekc();
};

template<typename R> int text_mode_reader<R>::getc() {
	if (int c = B::get_back())
		return c;
	int	c = mode.get_char(B::r);
	if (c == '\n') {
		B::line_number++;
		B::col_number = 0;
	}
	B::col_number++;
	return c;
}

template<typename R> int text_mode_reader<R>::peekc() {
	if (int c = B::backup[0])
		return c;
	return B::backup[0] = getc();
}

//-----------------------------------------------------------------------------
//	text_mode_writer
//-----------------------------------------------------------------------------

template<typename W> struct text_mode_writer {
	W			w;
	text_mode	mode;
	bool		ok;

	template<int N>	class acc : public buffered_accum<acc<N>, char, N> {
		text_mode_writer	&w;
	public:
		void	flush(const char *s, size_t n)	{ w.puts(s, n); }
		acc(text_mode_writer &w)				: w(w)	{}
		template<typename T> acc(text_mode_writer &w, const T &t)	: w(w) { *this << t; }
		~acc()	{ this->flush_all(); }
	};

	template<typename W1> text_mode_writer(W1 &&w, text_mode tm, bool put_bom = true) : w(forward<W1>(w)), mode(tm), ok(true) { if (put_bom) mode.put_bom(w); }
	bool		putc(int c)							{ return ok = mode.put_char(w, c); }
	bool		puts(const char *s, size_t len)		{ return ok = mode.put_string(w, s, len); }
	bool		puts(const char *s)					{ return ok = mode.put_string(w, s, strlen(s)); }
	template<int N> bool puts(const char (&s)[N])	{ return ok = mode.put_string(w, s, N - 1); }

	template<int N> bool vformat(const char *fmt, va_list valist) {
		acc<256>(*this).vformat(fmt, valist);
		return ok;
	}
	template<int N> bool format(const char *fmt,...) {
		va_list valist;
		va_start(valist, fmt);
		return vformat<N>(fmt, valist);
	}
	bool format(const char *fmt,...) {
		va_list valist;
		va_start(valist, fmt);
		return vformat<256>(fmt, valist);
	}
	template<int N> acc<N>	begin()	{ return *this; }
	acc<512>				begin()	{ return *this; }
	template<typename T> acc<512> operator<<(const T &t) { return acc<512>(*this, t); }
};


}//namespace iso

#endif // TEXT_H
