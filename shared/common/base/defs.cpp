#include "strings.h"
#include "bits.h"
#include "maths.h"
#include "soft_float.h"
#include "hashes/fnv.h"
#include "allocators/allocator.h"

DECL_ALLOCATOR void*  operator new(size_t size)		{ return iso::malloc(size); }
DECL_ALLOCATOR void*  operator new[](size_t size)	{ return iso::malloc(size); }
void	operator delete(void *p)					noexcept	{ iso::free(p); }
void	operator delete[](void *p)					noexcept	{ iso::free(p); }
void	operator delete(void *p, size_t size)		noexcept	{ iso::free(p); }
void	operator delete[](void *p, size_t size)		noexcept	{ iso::free(p); }

#ifndef PLAT_MSVC
no_inline size_t strlen(const char *s) {
	using namespace iso;
	auto	p = s;
	uint64	v64;

	if (!contains_zero(v64 = load_packed<uint64>(p))) {
		auto	p8 = (const uint64*)(intptr_t(p + 8) & -intptr_t(8));
		while (!contains_zero(v64 = *p8))
			++p8;
		p = (const char*)p8;
	}

	size_t	r	= p - s;
	uint32	v32	= lo(v64);
	if (!contains_zero(v32)) {
		r	+= 4;
		v32	= hi(v64);
	}
	return	((v32 >>  0) & 0xff) == 0 ? r + 0
		:	((v32 >>  8) & 0xff) == 0 ? r + 1
		:	((v32 >> 16) & 0xff) == 0 ? r + 2
		:	r + 3;
}
#endif

#if 0
no_inline size_t wcslen(const wchar_t *s) {
	using namespace iso;
	auto	p = s;
	uint64	v64;

	if (!contains_zero(v64 = load_packed<uint64>(p))) {
		auto	p8 = (const uint64*)(intptr_t(p + 16) & -intptr_t(16));
		while (!contains_zero<uint64, 16>(v64 = *p8))
			++p8;
		p = (const wchar_t*)p8;
	}

	size_t	r	= p - s;
	uint32	v32	= lo(v64);
	if (!contains_zero<uint32, 16>(v32)) {
		r	+= 2;
		v32	= hi(v64);
	}
	return	r + ((wchar_t)v32 != 0);
}
#endif

const void *memmem(const void *l, size_t l_len, const void *s, size_t s_len) {
	if (l_len == 0 || s_len == 0 || l_len < s_len)
		return nullptr;

	char	first	= *(const char*)s;
	if (s_len == 1)
		return memchr(l, first, l_len);

	for (auto i = (const char*)l, end = i + (l_len - s_len); i <= end; ++i)
		if (*i == first && memcmp(i, s, s_len) == 0)
			return i;

	return nullptr;
}

namespace iso {

int64	hash_put_misses	= 0, hash_put_count	= 0;
int64	hash_get_misses	= 0, hash_get_count	= 0;


thread_local arena_allocator<4096> thread_temp_allocator;
thread_local int stack_depth::depth = 0;

initialise::initialise(...)	{}
_none		none, terminate, empty;
_i			i;
_zero		zero;
_one		one, identity;

constant<__int<2> >		two;
constant<__int<4> >		four;
constant<__pi>			pi;
constant<__sign>		sign_mask;
constant<_maximum>		maximum;
constant<_minimum>		minimum;
constant<_epsilon>		epsilon;
constant<_nan>			nan;
constant<_infinity>		infinity;

decltype(one/two)		half;
decltype(one/three)		third;
decltype(one/four)		quarter;
decltype(sqrt(two))		sqrt2;
decltype(sqrt(three))	sqrt3;
decltype(sqrt(half))	rsqrt2;

template<int A>	struct axis_s {};

axis_s<0> x_axis;
axis_s<1> y_axis;
axis_s<2> z_axis, xy_plane;
axis_s<3> w_axis;

extern void __iso_debug_print(void*, const char*);
iso_export _iso_debug_print_t _iso_debug_print = {&__iso_debug_print, 0};
iso_export _iso_debug_print_t _iso_set_debug_print(const _iso_debug_print_t &f) {
	return exchange(_iso_debug_print, f);
}

char	thousands_separator	= ',';
char	decimal_point		= '.';

void*	calloc(size_t num, size_t size) {
	void	*p = iso::malloc(num * size);
	if (p)
		memset(p, 0, num * size);
	return p;
}

char*	strdup(const char *s) {
	if (!s || !*s)
		return nullptr;

	auto	len = strlen(s);
	char	*r	= (char*)iso::malloc(len + 1);
	memcpy(r, s, len + 1);
	return r;
}

//-----------------------------------------------------------------------------
//	scan
//-----------------------------------------------------------------------------

size_t from_string(const char *s, bool &i) {
	const char *p = skip_whitespace(s);
	i = *p && (*p == '1' || to_upper(*p) == 'T');
	while (*p && !is_whitespace(*p))
		p++;
	return p - s;
}

size_t from_string(const char *s, const char *e, bool &i) {
	const char *p = skip_whitespace(s, e);
	i = *p && (*p == '1' || to_upper(*p) == 'T');
	while (p < e && *p && !is_whitespace(*p))
		p++;
	return p - s;
}

template<typename T> inline const char *get_float(const char *p, T &f) {
	uint_t<sizeof(T)>	r;
	bool	sign = *p == '-';

	p = get_num_base<10>(p + int(sign || *p == '+'), r);

	T	f0 = T(r);
	while (is_digit(*p)) {
		f0 *= 10;
		p++;
	}
	if (*p == '.') {
		const char *p0 = p + 1;
		uint32 nf = uint32((p = get_num_base<10>(p0, r)) - p0);
		f0 += r / pow(T(10), nf);
		while (is_digit(*p))
			p++;
	}
	if (*p == 'e' || *p == 'E') {
		bool	signe = p[1] == '-';
		uint32	r;
		p	= get_num_base<10>(p + 1 + int(signe || p[1] == '+'), r);
		f0	= signe ? f0 / pow(T(10), r) : f0 * pow(T(10), r);
	}
	f = sign ? -f0 : f0;
	return p;
}

size_t from_string(const char *s, float &f) {
	return s ? get_float(skip_whitespace(s), f) - s : 0;
}
size_t from_string(const char *s, double &f) {
	return s ? get_float(skip_whitespace(s), f) - s : 0;
}

template<typename T> inline const char *get_float(const char *p, const char *e, T &f) {
	uint_t<sizeof(T)>	r;
	bool	sign = *p == '-';

	p = get_num_base<10>(p + int(sign || *p == '+'), e, r);

	T	f0 = T(r);
	while (p < e && is_digit(*p)) {
		f0 *= 10;
		p++;
	}
	if (*p == '.') {
		const char *p0 = p + 1;
		uint32 nf = uint32((p = get_num_base<10>(p0, e, r)) - p0);
		f0 += r / pow(T(10), nf);
		while (p < e && is_digit(*p))
			p++;
	}
	if (*p == 'e' || *p == 'E') {
		bool	signe = p[1] == '-';
		uint32	r;
		p	= get_num_base<10>(p + 1 + int(signe || p[1] == '+'), e, r);
		f0	= signe ? f0 / pow(T(10), r) : f0 * pow(T(10), r);
	}
	f = sign ? -f0 : f0;
	return p;
}

size_t from_string(const char *s, const char *e, float &f) {
	return s ? get_float(skip_whitespace(s, e), e, f) - s : 0;
}
size_t from_string(const char *s, const char *e, double &f) {
	return s ? get_float(skip_whitespace(s, e), e, f) - s : 0;
}

size_t from_string(const char *s, const char *e, GUID &g) {
	if (!s)
		return 0;
	const char *p = s;
	if (*p == '{')
		p++;
	p = get_num_base<16>(p, g.Data1);
	if (*p != '-')
		return 0;
	p = get_num_base<16>(p + 1, g.Data2);
	if (*p != '-')
		return 0;
	p = get_num_base<16>(p + 1, g.Data3);
	if (*p != '-')
		return 0;
	uint16	t16;
	uint64	t64;
	p = get_num_base<16>(p + 1, t16);
	if (*p != '-')
		return 0;
	p = get_num_base<16>(p + 1, t64);
	if (*s == '{' && *p++ != '}')
		return 0;
	*(uint64be*)g.Data4 = t64 + (uint64(t16) << 48);
	return p - s;
}

int	vscan_string(const char *buffer, const char *format, va_list argptr) {
	int			total	= 0;
	const char *p		= buffer;
	char		c;
	for (const char *in = format; c = *in; in++) {
		if (c == '%') {
			in++;
			if (*in == '%') {
				if (c == *p) {
					++p;
					continue;
				}
				break;
			}

			FORMAT::FLAGS	flags		= FORMAT::NONE;
			int				width		= 0;
			if (*in == '*') {
				in++;
				flags |= FORMAT::SKIP;
			}
			in = get_num_base<10>(in, width);

			switch (*in) {
				case 'h':
					in++;
					flags |= FORMAT::SHORT;
					break;
				case 'l':
					in++;
					if (*in == 'l') {
						in++;
						flags |= FORMAT::LONGLONG;
					} else {
						flags |= FORMAT::LONG;
					}
					break;
				case 'I':
					if (in[1] == '3' && in[2] == '2') {
						flags |= FORMAT::BITS32;
						in += 3;
					} else if (in[1] == '6' && in[2] == '4') {
						flags |= FORMAT::BITS64;
						in += 3;
					}
					break;
			}
			switch (char fmt = *in) {
				case 'c':
					*va_arg(argptr, char*) = *p++;
					++total;
					break;
				case 'd':
				case 'i':
					if (flags & FORMAT::BITS64)
						p += from_string(p, *va_arg(argptr, int64*));
					else if (flags & FORMAT::SHORT)
						p += from_string(p, *va_arg(argptr, int16*));
					else
						p += from_string(p, *va_arg(argptr, int32*));
					++total;
					break;
				case 'o':
					if (flags & FORMAT::BITS64)
						p = get_num_base<8>(p, *va_arg(argptr, uint64*));
					else if (flags & FORMAT::SHORT)
						p = get_num_base<8>(p, *va_arg(argptr, uint16*));
					else
						p = get_num_base<8>(p, *va_arg(argptr, uint32*));
					++total;
					break;
				case 'u':
					if (flags & FORMAT::BITS64)
						p = get_num_base<10>(p, *va_arg(argptr, uint64*));
					else if (flags & FORMAT::SHORT)
						p = get_num_base<10>(p, *va_arg(argptr, uint16*));
					else
						p = get_num_base<10>(p, *va_arg(argptr, uint32*));
					++total;
					break;
				case 'x':
					if (flags & FORMAT::BITS64)
						p = get_num_base<16>(p, *va_arg(argptr, uint64*));
					else if (flags & FORMAT::SHORT)
						p = get_num_base<16>(p, *va_arg(argptr, uint16*));
					else
						p = get_num_base<16>(p, *va_arg(argptr, uint32*));
					++total;
					break;
				case 'e':case 'E':case 'f':case 'g':case 'G':
					if (flags & (FORMAT::BITS64 | FORMAT::BITS32))
						p = get_float(p, *va_arg(argptr, double*));
					else
						p = get_float(p, *va_arg(argptr, float*));
					++total;
					break;
				case 'n':
					*va_arg(argptr, int*) = int(p - buffer);
					++total;
					break;
				case 's': {
					char	*s = va_arg(argptr, char*);
					while (is_whitespace(*p))
						++p;
					while (!is_whitespace(*p))
						*s++ = *p++;
					*s = 0;
					++total;
					break;
				}
			}

		} else if (is_whitespace(c)) {
			while (is_whitespace(*p))
				++p;

		} else if (c == *p) {
			++p;

		} else {
			break;
		}
	}
	return total;
}

int	scan_string(const char *buffer, const char *format, ...) {
	va_list valist;
	va_start(valist, format);
	return vscan_string(buffer, format, valist);
}

//-----------------------------------------------------------------------------
//	formatting helpers
//-----------------------------------------------------------------------------

static const char *special_num(float_category cat) {
	static const char *s[] = {
		0, "#QNAN", "#SNAN", "#INF", "-#INF"
	};
	return s[cat];
}

template<typename C> inline C *fill_char(C *p, int n, int c = ' ') {
	while (n-- > 0)
		*p++ = c;
	return p;
}

template<typename C> inline C *put_sign(C *p, bool sign, FORMAT::FLAGS flags) {
	if (sign)
		*p++ = '-';
	else if (flags & FORMAT::PLUS)
		*p++ = '+';
	else if (flags & FORMAT::BLANK)
		*p++ = ' ';
	return p;
}

template<typename C> C* put_leading_zeros(C *p, int n) {
	if (n >= 0) {
		*p++ = '0';
		if (n > 0) {
			*p++ = '.';
			while (n--)
				*p++ = '0';
		}
	}
	return p;
}

//-----------------------------------------------------------------------------
//	format
//-----------------------------------------------------------------------------

template<typename C> void put_str(accum<C> &acc, const C *s, FORMAT::FLAGS flags, int width, uint32 precision) {
	uint32	len	= flags & FORMAT::PRECISION
		? string_len32(s, s + precision)
		: string_len32(s);

	width	= max(width, len);
	C	*p	= acc.getp(width);

	len		= min(width, len);
	if (!(flags & FORMAT::LEFTALIGN))
		p = fill_char(p, width - len);
	for (int i = len; i--;)
		*p++ = *s++;
	if (flags & FORMAT::LEFTALIGN)
		p = fill_char(p, width - len);
}

template<typename C1, typename C2> void put_str(accum<C1> &acc, const C2 *s, FORMAT::FLAGS flags, int width, uint32 precision) {
	put_str(acc, alloc_string<C1>(s).begin(), flags, width, precision);
}

template<typename T, typename C> void put_int(accum<C> &acc, T x, FORMAT::FLAGS flags, int width) {
	absint<T>	a(x);
	int			B		= base(flags);
	bool		sign	= a.neg || (flags & (FORMAT::PLUS | FORMAT::BLANK));
	int			fmt		= (!(flags & FORMAT::CFORMAT) || B == 10) ? 0 : B == 8 ? 1 : 2;
	int			len		= log_base(B, a.u) + 1;
	int			total	= len + fmt + int(sign);
	int			extra	= width - total;

	if (width > total) {
		if (flags & FORMAT::ZEROES)
			len += exchange(extra, 0);
		total	= width;
	}

	C	*p	= acc.getp(total);

	if (!(flags & FORMAT::LEFTALIGN))
		p = fill_char(p, extra);

	p = put_sign(p, a.neg, flags);
	if (fmt) {
		*p++ = '0';
		if (B == 16)
			*p++ = flags & FORMAT::UPPER ? 'X' : 'x';
		else if (B == 2)
			*p++ = flags & FORMAT::UPPER ? 'B' : 'b';
	}

	put_num_base(B, p, len, a.u, flags & FORMAT::UPPER ? 'A' : 'a');
	if (flags & FORMAT::LEFTALIGN)
		fill_char(p + len, extra);
}

template<int B, typename C, typename T> void put_float(accum<C> &acc, T m, int frac, int exp, bool sign, uint32 digits, uint32 exp_digits, FORMAT::FLAGS flags, int width) {
	enum {M = sizeof(T) * 8 - 5};

	ISO_ASSERT(digits > 0);
	T	delta	= bit<T>(M) / pow(T(B), uint32(digits - 1));
	m	= shift_bits(m, M - frac) + (delta >> 1);

	if ((m >> M) >= B) {
		m = m / B + (delta >> 1);
		++exp;
	}

	bool	sci	= use_sci(flags, digits, exp);
	int		len	= int(sign || (flags & (FORMAT::PLUS | FORMAT::BLANK)));

	// actual number of digits
	digits	= get_floatlen<B, M>(m, delta);
	if (digits == 0)
		digits = 1;

	if (sci)
		len += digits + int(digits > 1) + exp_digits + 2;
	else if (exp < 0)
		len += digits + 1 - exp;
	else
		len += digits > exp + 1 ? digits + 1 : exp + 1;

	int		n0	= max(len, width), n = n0;
	C		*p	= acc.getp(n);
	if (n != n0)
		return;

	if (!(flags & FORMAT::LEFTALIGN))
		p = fill_char(p, width - len);

	p = put_sign(p, sign, flags);

	char	ten	= flags & FORMAT::UPPER ? 'A' : 'a';

	if (sci) {
		*p++	= to_digit(int(m >> M), ten);
		m		= (m & bits<T>(M)) * B;
		if (--digits) {
			*p++	= decimal_point;
			p		= put_float_digits_n<B, M>(p, m, digits, ten);
		}
		*p++ = flags & FORMAT::UPPER ? 'E' : 'e';
		*p++ = exp < 0 ? '-' : '+';
		put_num_base<10>(p, exp_digits, abs(exp));
		p += exp_digits;

	} else {
		if (exp < 0) {
			*p++ = '0';
			*p++ = decimal_point;
			while (exp < -1) {
				exp++;
				*p++ = '0';
			}
		} else {
			p = put_float_digits_n<10, M>(p, m, exp + 1);
			if (digits > exp + 1) {
				*p++ = decimal_point;
				digits -= exp + 1;
			} else {
				digits = 0;
			}
		}
		p = put_float_digits_n<B, M>(p, m, digits, ten);
	}

	if (flags & FORMAT::LEFTALIGN)
		p = fill_char(p, width - len);
}


template<int B, typename C, typename T> void put_float(accum<C> &acc, const float_info<T> &f, uint32 digits, uint32 exp_digits, FORMAT::FLAGS flags, int width) {
	if (f.cat)
		put_str(acc, special_num(f.cat), flags - FORMAT::PRECISION, width, 0);
	else
		put_float<B>(acc, f.mant, f.frac, f.exp, f.sign, digits, exp_digits, flags, width);
}

template void put_float<10, char, uint32>(accum<char> &acc, const float_info<uint32> &f, uint32 digits, uint32 exp_digits, FORMAT::FLAGS flags, int width);
template void put_float<10, char, uint64>(accum<char> &acc, const float_info<uint64> &f, uint32 digits, uint32 exp_digits, FORMAT::FLAGS flags, int width);

template<int B, typename C, typename T> void put_float(accum<C> &acc, T f, FORMAT::FLAGS flags, int width, uint32 precision) {
	put_float<B>(acc, get_print_info<B>(f),
		flags & FORMAT::PRECISION ? precision : (FLOG_BASE(B, 2, 8) * num_traits<T>::mantissa_bits) >> 8,
		num_traits<T>::exponent_bits * 3 / 10,
		flags,
		width
	);
}

template<typename C> accum<C>& accum<C>::vformat(const C *format, va_list argptr) {
	C		c;
	if (format) for (const C *in = format; c = *in; in++) {
		if (c == '%') {
			if (in[1] == '%') {
				putc(c);
				in++;
				continue;
			}
			FORMAT::FLAGS	flags		= FORMAT::NONE;
			uint32			precision	= 6;
			int				width		= 0;
			for (bool stop = false; !stop;) switch (*++in) {
				case '-': flags |= FORMAT::LEFTALIGN;	break;
				case '+': flags |= FORMAT::PLUS;			break;
				case '0': flags |= FORMAT::ZEROES;		break;
				case ' ': flags |= FORMAT::BLANK;		break;
				case '#': flags |= FORMAT::CFORMAT;		break;
				default: stop = true; break;
			}
			if (*in == '*') {
				in++;
				width = va_arg(argptr, int);
			} else {
				in = get_num_base<10>(in, width);
			}
//			if (width && out + width > guard)
//				break;

			if (*in == '.') {
				flags |= FORMAT::PRECISION;
				if (in[1] == '*') {
					in += 2;
					precision = va_arg(argptr, int);
				} else {
					in = get_num_base<10>(in + 1, precision);
				}
			}
			switch (*in) {
				case 'h':
					flags |= FORMAT::SHORT;
					in++;
					break;
				case 'l':
					in++;
					flags |= FORMAT::LONG;
					if (*in == 'l') {
						flags |= FORMAT::LONGLONG;
						in++;
					}
					break;
				case 'I':
					if (in[1] == '3' && in[2] == '2') {
						flags |= FORMAT::BITS32;
						in += 3;
					} else if (in[1] == '6' && in[2] == '4') {
						flags |= FORMAT::BITS64;
						in += 3;
					}
					break;
			}
			switch (C fmt = *in) {
				case 'c':
					if (!(flags & FORMAT::LEFTALIGN))
						putc(' ', width - 1);
					putc(va_arg(argptr, int));
					if (flags & FORMAT::LEFTALIGN)
						putc(' ', width - 1);
					break;

				case 'd': case 'i':
					if (flags & FORMAT::BITS64) {
						put_int(*this, va_arg(argptr, int64), flags, width);
					} else {
						int	 val = va_arg(argptr, int);
						put_int(*this, flags & FORMAT::SHORT ? (short)val : val, flags, width);
					}
					break;

				case 'b':
					if (flags & FORMAT::BITS64) {
						put_int(*this, va_arg(argptr, uint64), flags | FORMAT::BIN, width);
					} else {
						uint32	val = va_arg(argptr, uint32);
						put_int(*this, flags & FORMAT::SHORT ? (uint16)val : val, flags | FORMAT::BIN, width);
					}
					break;

				case 'B': {
					uint32	val = va_arg(argptr, uint32);
					put_str(*this, val ? "true" : "false", flags - FORMAT::PRECISION, width, 0);
					break;
				}

				case 'o':
					if (flags & FORMAT::BITS64) {
						put_int(*this, va_arg(argptr, uint64), flags | FORMAT::OCT, width);
					} else {
						uint32	val = va_arg(argptr, uint32);
						put_int(*this, flags & FORMAT::SHORT ? (uint16)val : val, flags | FORMAT::OCT, width);
					}
					break;

				case 'u':
					if (flags & FORMAT::BITS64) {
						put_int(*this, va_arg(argptr, uint64), flags, width);
					} else {
						uint32	val = va_arg(argptr, uint32);
						put_int(*this, flags & FORMAT::SHORT ? (uint16)val : val, flags, width);
					}
					break;

				case 'x': case 'X': {
					auto	flags2 = flags | FORMAT::HEX | FORMAT::UPPER * (fmt == 'X');
					if (flags & FORMAT::BITS64) {
						put_int(*this, va_arg(argptr, uint64), flags2, width);
					} else {
						uint32	val = va_arg(argptr, uint32);
						put_int(*this, flags & FORMAT::SHORT ? (uint16)val : val, flags2, width);
					}
					break;
				}
				case 'p': case 'P': {
					auto	flags2 = flags | FORMAT::HEX | FORMAT::UPPER * (fmt == 'P');
					put_int(*this, va_arg(argptr, uintptr_t), flags2, width);
					break;
				}
				case 'e': case 'E': {
					double	val = va_arg(argptr, double);
					put_float<10>(*this, val, flags | FORMAT::SCIENTIFIC | FORMAT::UPPER * (fmt == 'E'), width, precision);
					break;
				}
				case 'f': {
					double	val = va_arg(argptr, double);
					put_float<10>(*this, val, flags, width, precision);
					break;
				}
				case 'g': case 'G': {
					double	val = va_arg(argptr, double);
					put_float<10>(*this, val, flags | FORMAT::SHORTEST | FORMAT::UPPER * (fmt == 'G'), width, precision);
					break;
				}
				case 'a': case 'A': {
					double	val = va_arg(argptr, double);
					put_float<16>(*this, val, flags, width, precision);
					break;
				}
				case 'n': {
					int	*ptr = va_arg(argptr, int*);
					*ptr = int(getp() - startp);
					break;
				}
				case 's': {
					char *s = va_arg(argptr, char*);
					put_str(*this, s ? s : "(null)", flags, width, precision);
					break;
				}
				case 'S': {
					char16 *s = va_arg(argptr, char16*);
					put_str(*this, s ? s : (char16*)u"(null)", flags, width, precision);
					break;
				}
			}
		} else {
			putc(c);
		}
	}
	return *this;
}

template accum<char>& accum<char>::vformat(const char *format, va_list argptr);
template accum<char16>& accum<char16>::vformat(const char16 *format, va_list argptr);

#if 0
template<> string_accum16& string_accum16::vformat(const char16 *format, va_list argptr) {
	struct temp_accum : buffered_accum<temp_accum, char, 256> {
		string_accum16		&a;
		void		flush(const char *s, size_t n)	{ a.merge(s, n); }
		temp_accum(string_accum16 &_a) : a(_a) {}
	};
	temp_accum(*this).vformat(str8(format), argptr);
	return *this;
}
#endif

size_t _format(char *dest, size_t maxlen, const char *format, va_list valist) {
	return fixed_accum(dest, maxlen).vformat(format, valist).length();
}
size_t _format(char *dest, size_t maxlen, const char *format, ...) {
	va_list valist;
	va_start(valist, format);
	return _format(dest, maxlen, format, valist);
}
size_t _format(char *dest, const char *format, va_list valist) {
	return _format(dest, 256, format, valist);
}
size_t _format(char *dest, const char *format, ...) {
	va_list valist;
	va_start(valist, format);
	return _format(dest, 256, format, valist);
}

size_t sprintf(char *buffer, const char *format, ...) {
	va_list valist;
	va_start(valist, format);
	return _format(buffer, 256, format, valist);
}
template<typename C> accum<C> &operator<<(accum<C> &a, const CODE_GUID &g) {
	//{0x9de1c535, 0x6ae1, 0x11e0, {0x84, 0xe1, 0x18, 0xa9, 0x05, 0xbc, 0xc5, 0x3f}}
	a << "{0x" << hex(g.Data1) << "u, 0x" << hex(g.Data2) << ", 0x" << hex(g.Data3) << ", {";
	for (int i = 0; i < 8; i++)
		a << onlyif(i, ", ") << "0x" << hex(g.Data4[i]);
	return a << "}}";
}
template accum<char> &operator<<(accum<char> &a, const CODE_GUID &g);

template<typename C> accum<C> &operator<<(accum<C> &a, const GUID &g) {
	//{9de1c535-6ae1-11e0-84e118a905bcc53f}
//	return a << '{' << hex(g.Data1) << '-' << hex(g.Data2) << '-' << hex(g.Data3) << '-' << hex((uint64&)g.Data4) << '}';
	int		n = 36;
	char	*s = a.getp(n);
	put_num_base<16>(s, 8, g.Data1);
	s[8] = '-';
	put_num_base<16>(s + 9, 4, g.Data2);
	s[13] = '-';
	put_num_base<16>(s + 14, 4, g.Data3);
	s[18] = '-';
	put_num_base<16>(s + 19, 4, (uint16)*(uint16be*)g.Data4);
	s[23] = '-';
	put_num_base<16>(s + 24, 12, (uint64)(uint64be&)g.Data4);
	return a;
}
template accum<char> &operator<<(accum<char> &a, const GUID &g);

//-----------------------------------------------------------------------------
//	put_float
//-----------------------------------------------------------------------------

template<typename T> size_t put_float(char *s, T m, int frac, int exp, bool sign, uint32 digits, uint32 exp_digits) {
	enum {M = sizeof(T) * 8 - 5};

	char	*out		= s;
	if (sign)
		*out++ = '-';

	T	delta	= bit<T>(M) / pow(T(10), uint32(digits - 1));
	m	= shift_bits(m, M - frac) + (delta >> 1);

	if ((m >> M) >= 10) {
		m = m / 10 + (delta >> 1);
		++exp;
	}

	if (exp < -4 || exp >= int(digits)) {
		*out++ = to_digit(int(m >> M));
		m		= (m & bits<T>(M)) * 10;
		delta	*= 10;

		if (m > delta) {
			*out++ = decimal_point;
			out = put_float_digits<10, M>(out, m, delta);
		}
		*out++ = 'e';
		*out++ = exp < 0 ? '-' : '+';
		put_num_base<10>(out, exp_digits, abs(exp));
		out += exp_digits;

	} else {
		if (exp < 0) {
			*out++ = '0';
			*out++ = decimal_point;
			while (exp < -1) {
				exp++;
				*out++ = '0';
			}
		} else {
			while (exp-- >= 0) {
				*out++ = to_digit(int(m >> M));
				m		= (m & bits<T>(M)) * 10;
				delta	*= 10;
			}
			if (m > delta)
				*out++ = decimal_point;
		}
		out = put_float_digits<10, M>(out, m, delta);
	}
	return out - s;
}

template<> size_t put_float<uint16>(char *s, uint16 m, int frac, int exp, bool sign, uint32 digits, uint32 exp_digits) {
	return put_float<uint32>(s, m, frac, exp, sign, digits, exp_digits);
}
template size_t put_float<uint64>(char *s, uint64 m, int frac, int exp, bool sign, uint32 digits, uint32 exp_digits);
template size_t put_float<uint128>(char *s, uint128 m, int frac, int exp, bool sign, uint32 digits, uint32 exp_digits);

template<typename T> size_t put_float(char *s, const float_info<T> &f, uint32 digits, uint32 exp_digits) {
	if (f.cat) {
		strcpy(s, special_num(f.cat));
		return strlen(s);
	}
	return put_float(s, f.mant, f.frac, f.exp, f.sign, digits, exp_digits);
}

template size_t put_float(char *s, const float_info<uint16> &f, uint32 digits, uint32 exp_digits);
template size_t put_float(char *s, const float_info<uint32> &f, uint32 digits, uint32 exp_digits);
template size_t put_float(char *s, const float_info<uint64> &f, uint32 digits, uint32 exp_digits);
template size_t put_float(char *s, const float_info<uint128> &f, uint32 digits, uint32 exp_digits);

//-----------------------------------------------------------------------------
//	utf8
//-----------------------------------------------------------------------------

uint32 char_length(char c) {
	uint32	n = 1;
	if (uint8(c) >= 0xc0) {
		do {
			n++;
			c <<= 1;
		} while (c & 0x40);
	}
	return n;
}

uint32 put_char(char32 c, char *dest, bool strict) {
	if (c < (strict ? 0x80 : 0xc0)) {
		dest[0]	= (char)c;
		return 1;
	}

	uint32	n = 2;
	while (c >= (2 << (n * 5)))
		n++;

	dest[0] = char((c >> (n * 6 - 6)) - (1 << (8 - n)));
	for (int i = 1; i < n; i++)
		dest[i] = char(((c >> ((n - i - 1) * 6)) & 0x3f) | 0x80);
	return n;
}

uint32	put_char(char32 c, char16 *dest, bool strict) {
	if (c < 0x10000) {
		*dest = char16(c);
		return 1;
	} else {
		dest[0] = 0xd800 + ((c - 0x10000) >> 10);
		dest[1] = 0xdc00 + (c & 0x3FF);
		return 2;
	}
}
uint32	put_char(char32 c, char16be *dest, bool strict) {
	if (c < 0x10000) {
		*dest = char16(c);
		return 1;
	} else {
		dest[0] = 0xd800 + ((c - 0x10000) >> 10);
		dest[1] = 0xdc00 + (c & 0x3FF);
		return 2;
	}
}

uint32 get_char(char32 &c, const char *srce) {
	const char	*p	= srce;
	uint32	r		= uint8(*p++);
	uint32	m		= 0xc0;
	if (r >= m) {
		do {
			r = ((r & ~m) << 6) | (*p++ & 0x3f);
			m <<= 5;
		} while (r & m);
	}
	c = r;
	return uint32(p - srce);
}

uint32 get_char(char32 &c, const char16 *srce) {
	if (between(*srce, 0xd800, 0xddff) && (c = from_utf16_surrogates(srce[0], srce[1])))
		return 2;
	c = *srce;
	return 1;
}
uint32 get_char(char32 &c, const char16be *srce) {
	if (between(*srce, 0xd800, 0xddff) && (c = from_utf16_surrogates(srce[0], srce[1])))
		return 2;
	c = *srce;
	return 1;
}

uint32 legalise_utf8(const char *srce, char *dest, size_t maxlen) {
	char *p = dest, *e = p + maxlen - 1, c;
	if (srce) {
		while (p < e && (c = *srce)) {
			if (c > 0x80u && c < 0xc0u)
				*p++ = 0xc2u;
			*p++ = c;
		}
	}
	if (p)
		*p = 0;
	return uint32(p - dest);
}

//-----------------------------------------------------------------------------
//	string
//-----------------------------------------------------------------------------

char_set char_set::whitespace("\t\r\n ");
char_set char_set::digit('0', '9');
char_set char_set::upper('A', 'Z');
char_set char_set::lower('a', 'z');
char_set char_set::alpha		= char_set::upper + char_set::lower;
char_set char_set::alphanum		= char_set::alpha + char_set::digit;
char_set char_set::identifier	= char_set::alphanum + '_';
char_set char_set::ascii(0, 0x7f);
char_set char_set::control		= char_set(0, 0x1f) + 0x7f;

uint32 string_hash(const char *s)				{ return FNV1<uint32>(s); }
uint32 string_hash(const char16 *s)				{ return FNV1<uint32>(s); }
uint32 string_hash(const char *s, size_t n)		{ return FNV1_str<uint32>(s, n); }
uint32 string_hash(const char16 *s, size_t n)	{ return FNV1_str<uint32>(s, n); }

//-----------------------------------------------------------------------------
//	number
//-----------------------------------------------------------------------------

number number::to_binary() const {
	if (type != DEC)
		return *this;

	int		e2	= 0;
	int		e10	= e;
	uint64	m2	= iso::abs(m);

	while (e10 > 0) {
#if 1
		if (m2 > 0x7fffffffffffffffULL / 10 * 8) {
			m2 >>= 1;
			++e2;
		}
		m2 += m2 >> 2;	// * 10 / 8
		e2 += 3;
#else
		while (m2 > 0x7fffffffffffffffULL / 10) {
			m2 >>= 1;
			e2++;
		}
		m2 *= 10;
#endif
		--e10;
	}

	if (e10 && m2) {
		uint64	pow10 = 1;
		while (e10++) {
			while (pow10 > 0x7fffffffffffffffULL / 10) {
				pow10 >>= 1;
				--e2;
			}
			pow10 *= 10;
		}
		while (m2 < pow10) {
			m2 <<= 1;
			--e2;
		}
		uint64	num = m2 % pow10;
		m2 /= pow10;
		while (num && !(m2 & (1ULL << 62))) {
			m2  <<= 1;
			num <<= 1;
			--e2;
			if (num > pow10) {
				num -= pow10;
				++m2;
			}
		}
	}
	return number(m < 0 ? -m2 : m2, e2, FLT, size);
}

int64 number::fixed(uint32 frac, bool adj) const {
	if (type == DEC)
		return to_binary().fixed(frac, adj);

	int64	m1	= shift_bits_round(m, frac + e);
	return adj ? m1 - (m1 >> frac) : m1;
}

//-----------------------------------------------------------------------------
//	soft_float
//-----------------------------------------------------------------------------

void soft_float_make_table(float *table, uint32 mb, uint32 eb, bool sb) {
	int		eo	= 128 - (1 << (eb - 1));
	iorf	*p	= (iorf*)table;
	for (int s = 0; s < 1 << int(sb); s++) {
		//zero
		*p++ = iorf(0, 0, s).f();
		//denormals
		for (int m = 1; m < 1 << mb; m++) {
			int	h = highest_set_index(m);
			*p++ = iorf(m << (23 - h), eo - mb + h + 1, s).f();
		}
		//normals
		for (int e = 1; e < 1 << eb; e++)
			for (int m = 0; m < 1 << mb; m++)
				*p++ = iorf(m << (23 - mb), e + eo, s).f();
	}
}

//-----------------------------------------------------------------------------
//	DPD encoded value Decimal digits
//-----------------------------------------------------------------------------

/*
987	654	3210 	d2		d1		d0 		Values encoded 		Description						top
-----------------------------------------------------------------------------------------------
abc	def	0ghi	0abc	0def	0ghi	(0-7) (0-7) (0-7)	Three small digits				000

abc	def	100i	0abc	0def	100i	(0-7) (0-7) (8-9)	Two small digits,one large		001
abc	ghf	101i	0abc	100f	0ghi	(0-7) (8-9) (0-7) 									010
ghc	def	110i	100c	0def	0ghi	(8-9) (0-7) (0-7)									100

ghc	00f	111i	100c	100f	0ghi	(8-9) (8-9) (0-7)	One small digit,two large		110
dec	01f	111i	100c	0def	100i	(8-9) (0-7) (8-9) 									101
abc	10f	111i	0abc	100f	100i	(0-7) (8-9) (8-9) 									011

xxc	11f	111i	100c	100f	100i	(8-9) (8-9) (8-9)	Three large digits				111
*/

uint64	bcd_to_dpd(uint64 x) {
	static const uint64	mask = bit_every<uint64, 12>;

	uint64	t0	= mask & (x >> 11);
	uint64	t1	= mask & (x >> 7);
	uint64	t2	= mask & (x >> 3);

	uint64	b5	= t0 & t2;
	uint64	b6	= t1 & t2;
	uint64	b1	= b5 | t1;
	uint64	b2	= b6 | t0;
	uint64	b3	= t0 | t1 | t2;

	uint64	Aab	= mask ^ t0;
	uint64	Ade	= b5 & ~t1;
	uint64	Agh	= t0 & ~t2;
	uint64	Bde	= mask ^ (b5 | t1);
	uint64	Bgh	= ~t0 & t1 & ~t2;
	uint64	Cgh	= mask ^ b3;

	uint64	y =	(x & (mask * 0x111))
		|	 (x & (Aab * 0x600 + Bde * 0x060 + Cgh * 0x006))
		|	((x & (Ade * 0x060 + Bgh * 0x006)) << 4)
		|	((x & (Agh * 0x006)) << 8)
		|	(b1 << 1)
		|	(b2 << 2)
		|	(b3 << 3)
		|	(b5 << 5)
		|	(b6 << 6)
		;

	uint64	t	= y & (mask * 0x700);
	y	= (y ^ t) | (t >> 1);	// shift d2 into place

	return unpart_bits<10, 2, 64>(y);
}

uint64	dpd_to_bcd(uint64 y) {
	static const uint64	mask = bit_every<uint64, 12>;

	uint64	x	= part_bits<10, 2, 54>(y);
	uint64	t	= x & (mask * 0x380);
	x	= (x ^ t) | (t << 1);	// shift d2 into place

	uint64	b1	= x >> 1;
	uint64	b2	= x >> 2;
	uint64	b3	= (x >> 3) & mask;
	uint64	b5	= x >> 5;
	uint64	b6	= x >> 6;

	uint64	t0	= b3 & b2 & (~b1 | b5 | ~b6);
	uint64	t1	= b3 & b1 & (~b2 | ~b5 | b6);
	uint64	t2	= b3 & ((b1 & b2 & (b5 | b6)) | ~(b1 | b2));

	uint64	Aab	= mask ^ t0;
	uint64	Ade	= t0 & t2 & ~t1;
	uint64	Agh	= t0 & ~t2;
	uint64	Bde	= mask ^ (b1 & b3);
	uint64	Bgh	= ~t0 & t1 & ~t2;
	uint64	Cgh	= mask ^ b3;

	return (x & (mask * 0x111))
		|  (x & (Aab * 0x600 + Bde * 0x060 + Cgh * 0x006))
		| ((x & (Ade * 0x600 + Bgh * 0x060)) >> 4)
		| ((x & (Agh * 0x600)) >> 8)
		| (t0 << 11)
		| (t1 << 7)
		| (t2 << 3);
}

uint64 to_bcd(uint64 x, int n) {
	static const uint64	mask = bit_every<uint64, 4>;
	uint64 lo = 0;
	while (n--) {
		lo += ((lo + mask * 3) & (mask * 8)) / 8 * 3;	// add 3 everywhere that nibble >= 5
		lo = (lo << 1) | (x >> 63);
		x <<= 1;
	}
	return lo;
}

uint64 bin_to_dpd(uint64 x) {
	if (x < bit64(50))
		return bcd_to_dpd(to_bcd(x));

	uint64	hi = divmod(x, POW(10, 15));
	return (bcd_to_dpd(to_bcd(x))) | (bcd_to_dpd(to_bcd(hi)) << 50);
}

uint64 dpd_to_bin(uint64 x) {
	if (x < bit64(50))
		return from_bcd(dpd_to_bcd(x));

	return from_bcd(dpd_to_bcd(x) & bits64(60)) + from_bcd(dpd_to_bcd(x >> 50)) * POW(10,15);
}

uint128 bin_to_dpd(uint128 x) {
	uint64	lo1, hi1	= fulldivmodc(lo(x), hi(x), POW(10, 15), lo1);
	lo1	= bcd_to_dpd(to_bcd(lo1));

	if (hi1 < bit64(50)) {
		hi1	= bcd_to_dpd(to_bcd(hi1));
		return lo_hi(lo1 | (hi1 << 50), hi1 >> (64 - 50));
	}

	uint64	hi2	= divmod(hi1, POW(10,15));
	hi1	= bcd_to_dpd(to_bcd(hi1));
	hi2	= bcd_to_dpd(to_bcd(hi2));

	return lo_hi(lo1 | (hi1 << 50), (hi1 >> (64 - 50)) | (hi2 << (100 - 64)));
}

uint128	dpd_to_bin(uint128 x) {
	uint64	lo1	= from_bcd(dpd_to_bcd(lo(x) & bits64(50)));
	uint64	hi1	= from_bcd(dpd_to_bcd(lo(x >> 50) & bits64(50)));
	uint64	t	= maddc(hi1, POW(10, 15), lo1, lo1);

	if (hi(x) < bit64(100 - 64))
		return lo_hi(lo1, t);

	return lo_hi(lo1, t) + fullmul(from_bcd(dpd_to_bcd(lo(x >> 100))), POW(10, 15)) * POW(10, 15);
}

char *put_bcd_digits(char *s, uint64 m, int x, int dp) {
	//char	*s0 = s;
	m <<= x * 4;
	while (x < 16) {
		if (x == dp)
			*s++ = '.';
		*s++ = (m >> 60) + '0';
		m <<= 4;
		++x;
	}
	return s;
}


template<typename T> size_t put_decimal(char *s, T m, int dp, bool sign);

template<> size_t put_decimal<uint32>(char *s, uint32 m, int dp, bool sign) {
	char	*s0 = s;
	if (sign)
		*s++ = '-';

	uint64	b0	= to_bcd(m);
	int		x	= leading_zeros(b0) / 4;
	return fill_char(put_bcd_digits(put_leading_zeros(s, x + dp - 16), b0, x, 16 - dp), -dp, '0') - s0;
}

template<> size_t put_decimal<uint64>(char *s, uint64 m, int dp, bool sign) {
	char	*s0 = s;
	if (sign)
		*s++ = '-';

	if (m < POW(10, 15)) {
		uint64	b0	= to_bcd(m);
		int	x = leading_zeros(b0) / 4;
		return fill_char(put_bcd_digits(put_leading_zeros(s, x + dp - 16), b0, x, 16 - dp), -dp, '0') - s0;
	}

	uint64	mod, b0, b1;
	m	= divmod(m, POW(10, 15), mod);
	b0	= to_bcd(mod);
	b1	= to_bcd(m);

	int	x = leading_zeros(b1) / 4;

	s = put_leading_zeros(s, x + dp - 31);
	s = put_bcd_digits(s, b1, x, 31 - dp);
	s = put_bcd_digits(s, b0, 1, 16 - dp);
	s = fill_char(s, -dp, '0');

	return s - s0;
}

template<> size_t put_decimal<uint128>(char *s, uint128 m, int dp, bool sign) {
	char	*s0 = s;
	if (sign)
		*s++ = '-';

	uint64	mod, b0, b1 = 0, b2 = 0;
	m	= divmod(m, POW(10, 15), mod);
	b0	= to_bcd(mod);
	if (!!m) {
		m	= divmod(m, POW(10, 15), mod);
		b1	= to_bcd(mod);
		if (uint64 m0 = lo(m))
			b2	= to_bcd(m0);
	}

	int	x = (b2 ? leading_zeros(b2) : b1 ? leading_zeros(b1) + 60 : leading_zeros(b0) + 120) / 4;

	s = put_leading_zeros(s, x + dp - 46);
	s = put_bcd_digits(s, b2, x, 46 - dp);
	s = put_bcd_digits(s, b1, max(x, 16) - 15, 31 - dp);
	s = put_bcd_digits(s, b0, max(x, 31) - 30, 16 - dp);
	s = fill_char(s, -dp, '0');

	return s - s0;
}

//-----------------------------------------------------------------------------
//	is_square helpers
//-----------------------------------------------------------------------------

bool check_square_mod128(uint32 a) {
#if 0
	static const char rem_128[128] = {
		0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
		0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
		1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
		1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,

		0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
		1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
		1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
		1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
	};
	return !rem_128[a];
#else
	static const uint64 rem_128b[2] = {
		0b11111101'11111101'11111101'11101101'11111101'11111100'11111101'11101100,
		0b11111101'11111101'11111101'11101101'11111101'11111101'11111101'11101100,
	};
	return !(rem_128b[a / 64] & bit(a & 63));
#endif
}

bool check_square_mod3_5_7(uint32 a) {
	static const char rem_105[105] = {
		0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
		0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1,
		0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1,
		1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
		0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1,
		1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1,
		1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1,
	};

	return !rem_105[a];
}

//-----------------------------------------------------------------------------
//	gamma/lgamma
//-----------------------------------------------------------------------------

// Directly approximates gamma over (1,2) and uses reduction identities to reduce other arguments to this interval (only for x < 12)
double gamma0(double x) {
//	static const double p[] = {
//		-1.71618513886549492533811E+0,
//		 2.47656508055759199108314E+1,
//		-3.79804256470945635097577E+2,
//		 6.29331155312818442661052E+2,
//		 8.66966202790413211295064E+2,
//		-3.14512729688483675254357E+4,
//		-3.61444134186911729807069E+4,
//		 6.64561438202405440627855E+4
//	};
//
//	static const double q[] = {
//		-3.08402300119738975254353E+1,
//		 3.15350626979604161529144E+2,
//		-1.01515636749021914166146E+3,
//		-3.10777167157231109440444E+3,
//		 2.25381184209801510330112E+4,
//		 4.75584627752788110767815E+3,
//		-1.34659959864969306392456E+5,
//		-1.15132259675553483497211E+5
//	};

	double	z = frac(x);

	double	res = 1 + horner(z,
		-1.71618513886549492533811E+0,
		2.47656508055759199108314E+1,
		-3.79804256470945635097577E+2,
		6.29331155312818442661052E+2,
		8.66966202790413211295064E+2,
		-3.14512729688483675254357E+4,
		-3.61444134186911729807069E+4,
		6.64561438202405440627855E+4
	) / horner(z,
		-3.08402300119738975254353E+1,
		3.15350626979604161529144E+2,
		-1.01515636749021914166146E+3,
		-3.10777167157231109440444E+3,
		2.25381184209801510330112E+4,
		4.75584627752788110767815E+3,
		-1.34659959864969306392456E+5,
		-1.15132259675553483497211E+5,
		1.0
	);

//	double	num = 0, den = 1;
//	for (int i = 0; i < 8; i++) {
//		num = (num + p[i]) * z;
//		den = den * z + q[i];
//	}
//	double	res = num / den + 1;

	// Apply correction if argument was not initially in (1,2)
	if (x < 1) {
		// Use identity gamma(z) = gamma(z+1)/z
		res /= z;
	} else {
		// Use the identity gamma(z+n) = z*(z+1)* ... *(z+n-1)*gamma(z)
		for (int n = (int)floor(x); --n;)
			res *= ++z;
	}
	return res;
}

//only for x >= 12
double lgamma0(double x) {
//	static const double c[8] = {
//		+1		/ 12.0,
//		-1		/ 360.0,
//		+1		/ 1260.0,
//		-1		/ 1680.0,
//		+1		/ 1188.0,
//		-691	/ 360360.0,
//		+1		/ 156.0,
//		-3617	/ 122400.0
//	};
	static const double halfLogTwoPi = 0.91893853320467274178032973640562;

	double z	= 1 / (x * x);
	double	res = horner(z,
		+1		/ 12.0,
		-1		/ 360.0,
		+1		/ 1260.0,
		-1		/ 1680.0,
		+1		/ 1188.0,
		-691	/ 360360.0,
		+1		/ 156.0,
		-3617	/ 122400.0
	);
//	double res	= c[7];
//	for (int i = 6; i >= 0; i--)
//		res = res * z + c[i];

	return (x - 0.5) * ln(x) - x + res / x + halfLogTwoPi;
}

double lgamma(double x) { // x must be positive
	return	x <= 0		? nan
		:	x < 12		? ln(abs(gamma0(x)))
		:	lgamma0(x);
}

double gamma(double x) {
	const double gamma = 0.577215664901532860606512090;		// Euler's gamma constant
	return	x <= 0		? nan
		:	x < 0.001	? 1 / (x * (1 + gamma * x))			// For small x, 1/Gamma(x) has power series x + gamma x^2 - ...
		:	x < 12		? gamma0(x)
		:	x > 171.624	? infinity
		:	exp(lgamma0(x));
}

double digamma(double x) {
	if (x < 0) {
		double	s, c;
		sincos(x * pi, &s, &c);
		return pi * c / s * digamma(1 - x);
	}
	double r = 0;
	while (x < 7)
		r -= 1 / x++;

	x -= half;
	return r + horner(1 / square(x), log(x), 1. / 24, -7.0 / 960, 31.0 / 8064, -127.0 / 30720);
}


//-----------------------------------------------------------------------------
//	erf/erfc
//-----------------------------------------------------------------------------
//                          x
//                   2      |\
//    erf(x)  =  ---------  | exp(-t*t)dt
//                sqrt(pi) \|
//                          0


inline double erf0(double x) {
	double	z = x * x;
	return	x
		*	horner(z, 1.1283791670955126, 0.17644011324127379, 0.051728385731987528, 0.0034814902101731196, 0.00032806639713878541, 8.8784405615357056e-06, 1.2040999373658753e-07)
		/	horner(z, 1.0000000000000000, 0.48969931241768294, 0.10907619238514397, 0.014283713727543943, 0.0011742377551152094, 5.8411032889811751e-05, 1.4060567875468972e-06);
}

inline double erfc0(double x) {
	return	exp(-x * x)
		*	horner(x, 0.83571382001120953, 1.2525362561495670, 0.91231706220035758, 0.38676415143622900, 0.095637893500953100, 0.011684287172270874, 6.1792832735846071e-10)
		/	horner(x, 0.83571864525686790, 2.1955066259704652, 2.5541015508553357, 1.7015840993444071, 0.69589694980162042, 0.16951239053544545, 0.020709916302643547);
}

inline double erf_abs(double x) {
	return	x < 1.523	? erf0(x)
		:	x < 5.84	? 1 - erfc0(x)
		:	1;
}
double erf(double x) {
	return x < 0 ? -erf_abs(-x) : erf_abs(x);
}

inline double erfc_abs(double x) {
	return	x < 1.523 ? 1 - erf0(x) : erfc0(x);
}
double erfc(double x) {
	return x < 0 ? 2 - erfc_abs(-x) : erfc_abs(x);
}


inline float erf0(float x) {
	float	z = x * x;
	return	x
		*	horner(z, 1.12837923f, 0.131320685f, 0.0360523686f, 0.000261409412f)
		/	horner(z, 1.00000000f, 0.449713647f, 0.0818521455f, 0.00636281027f);
}

inline float erfc0(float x) {
	return	exp(-x * x)
		*	horner(x, 0.827261329f, 0.390499413f, 0.00336514297f, -0.000190432969f)
		/	horner(x, 0.841951013f, 1.27641082f, 0.737274826f);
}

inline float erf_abs(float x) {
	return	x < 1.523f	? erf0(x)
		:	x < 3.82f	? 1 - erfc0(x)
		:	1;
}
float erf(float x) {
	return x < 0 ? -erf_abs(-x) : erf_abs(x);
}

inline float erfc_abs(float x) {
	return	x < 1.523f ? 1 - erf0(x) : erfc0(x);
}
float erfc(float x) {
	return x < 0 ? 2 - erfc_abs(-x) : erfc_abs(x);
}

float cdf(float x)							{ return (1 + erf(x * rsqrt2)) * half; }
float cdf(float x, float mu, float sigma)	{ return cdf((x - mu) / sigma); }


//-----------------------------------------------------------------------------
//	PRIME
//-----------------------------------------------------------------------------

const uint16 primes[2048] = {
        2,     3,     5,     7,    11,    13,    17,    19,       23,    29,    31,    37,    41,    43,    47,    53,
       59,    61,    67,    71,    73,    79,    83,    89,       97,   101,   103,   107,   109,   113,   127,   131,
      137,   139,   149,   151,   157,   163,   167,   173,      179,   181,   191,   193,   197,   199,   211,   223,
      227,   229,   233,   239,   241,   251,   257,   263,      269,   271,   277,   281,   283,   293,   307,   311,
      313,   317,   331,   337,   347,   349,   353,   359,      367,   373,   379,   383,   389,   397,   401,   409,
      419,   421,   431,   433,   439,   443,   449,   457,      461,   463,   467,   479,   487,   491,   499,   503,
      509,   521,   523,   541,   547,   557,   563,   569,      571,   577,   587,   593,   599,   601,   607,   613,
      617,   619,   631,   641,   643,   647,   653,   659,      661,   673,   677,   683,   691,   701,   709,   719,
      727,   733,   739,   743,   751,   757,   761,   769,      773,   787,   797,   809,   811,   821,   823,   827,
      829,   839,   853,   857,   859,   863,   877,   881,      883,   887,   907,   911,   919,   929,   937,   941,
      947,   953,   967,   971,   977,   983,   991,   997,     1009,  1013,  1019,  1021,  1031,  1033,  1039,  1049,
     1051,  1061,  1063,  1069,  1087,  1091,  1093,  1097,     1103,  1109,  1117,  1123,  1129,  1151,  1153,  1163,
     1171,  1181,  1187,  1193,  1201,  1213,  1217,  1223,     1229,  1231,  1237,  1249,  1259,  1277,  1279,  1283,
     1289,  1291,  1297,  1301,  1303,  1307,  1319,  1321,     1327,  1361,  1367,  1373,  1381,  1399,  1409,  1423,
     1427,  1429,  1433,  1439,  1447,  1451,  1453,  1459,     1471,  1481,  1483,  1487,  1489,  1493,  1499,  1511,
     1523,  1531,  1543,  1549,  1553,  1559,  1567,  1571,     1579,  1583,  1597,  1601,  1607,  1609,  1613,  1619,
     1621,  1627,  1637,  1657,  1663,  1667,  1669,  1693,     1697,  1699,  1709,  1721,  1723,  1733,  1741,  1747,
     1753,  1759,  1777,  1783,  1787,  1789,  1801,  1811,     1823,  1831,  1847,  1861,  1867,  1871,  1873,  1877,
     1879,  1889,  1901,  1907,  1913,  1931,  1933,  1949,     1951,  1973,  1979,  1987,  1993,  1997,  1999,  2003,
     2011,  2017,  2027,  2029,  2039,  2053,  2063,  2069,     2081,  2083,  2087,  2089,  2099,  2111,  2113,  2129,
     2131,  2137,  2141,  2143,  2153,  2161,  2179,  2203,     2207,  2213,  2221,  2237,  2239,  2243,  2251,  2267,
     2269,  2273,  2281,  2287,  2293,  2297,  2309,  2311,     2333,  2339,  2341,  2347,  2351,  2357,  2371,  2377,
     2381,  2383,  2389,  2393,  2399,  2411,  2417,  2423,     2437,  2441,  2447,  2459,  2467,  2473,  2477,  2503,
     2521,  2531,  2539,  2543,  2549,  2551,  2557,  2579,     2591,  2593,  2609,  2617,  2621,  2633,  2647,  2657,
     2659,  2663,  2671,  2677,  2683,  2687,  2689,  2693,     2699,  2707,  2711,  2713,  2719,  2729,  2731,  2741,
     2749,  2753,  2767,  2777,  2789,  2791,  2797,  2801,     2803,  2819,  2833,  2837,  2843,  2851,  2857,  2861,
     2879,  2887,  2897,  2903,  2909,  2917,  2927,  2939,     2953,  2957,  2963,  2969,  2971,  2999,  3001,  3011,
     3019,  3023,  3037,  3041,  3049,  3061,  3067,  3079,     3083,  3089,  3109,  3119,  3121,  3137,  3163,  3167,
     3169,  3181,  3187,  3191,  3203,  3209,  3217,  3221,     3229,  3251,  3253,  3257,  3259,  3271,  3299,  3301,
     3307,  3313,  3319,  3323,  3329,  3331,  3343,  3347,     3359,  3361,  3371,  3373,  3389,  3391,  3407,  3413,
     3433,  3449,  3457,  3461,  3463,  3467,  3469,  3491,     3499,  3511,  3517,  3527,  3529,  3533,  3539,  3541,
     3547,  3557,  3559,  3571,  3581,  3583,  3593,  3607,     3613,  3617,  3623,  3631,  3637,  3643,  3659,  3671,
     3673,  3677,  3691,  3697,  3701,  3709,  3719,  3727,     3733,  3739,  3761,  3767,  3769,  3779,  3793,  3797,
     3803,  3821,  3823,  3833,  3847,  3851,  3853,  3863,     3877,  3881,  3889,  3907,  3911,  3917,  3919,  3923,
     3929,  3931,  3943,  3947,  3967,  3989,  4001,  4003,     4007,  4013,  4019,  4021,  4027,  4049,  4051,  4057,
     4073,  4079,  4091,  4093,  4099,  4111,  4127,  4129,     4133,  4139,  4153,  4157,  4159,  4177,  4201,  4211,
     4217,  4219,  4229,  4231,  4241,  4243,  4253,  4259,     4261,  4271,  4273,  4283,  4289,  4297,  4327,  4337,
     4339,  4349,  4357,  4363,  4373,  4391,  4397,  4409,     4421,  4423,  4441,  4447,  4451,  4457,  4463,  4481,
     4483,  4493,  4507,  4513,  4517,  4519,  4523,  4547,     4549,  4561,  4567,  4583,  4591,  4597,  4603,  4621,
     4637,  4639,  4643,  4649,  4651,  4657,  4663,  4673,     4679,  4691,  4703,  4721,  4723,  4729,  4733,  4751,
     4759,  4783,  4787,  4789,  4793,  4799,  4801,  4813,     4817,  4831,  4861,  4871,  4877,  4889,  4903,  4909,
     4919,  4931,  4933,  4937,  4943,  4951,  4957,  4967,     4969,  4973,  4987,  4993,  4999,  5003,  5009,  5011,
     5021,  5023,  5039,  5051,  5059,  5077,  5081,  5087,     5099,  5101,  5107,  5113,  5119,  5147,  5153,  5167,
     5171,  5179,  5189,  5197,  5209,  5227,  5231,  5233,     5237,  5261,  5273,  5279,  5281,  5297,  5303,  5309,
     5323,  5333,  5347,  5351,  5381,  5387,  5393,  5399,     5407,  5413,  5417,  5419,  5431,  5437,  5441,  5443,
     5449,  5471,  5477,  5479,  5483,  5501,  5503,  5507,     5519,  5521,  5527,  5531,  5557,  5563,  5569,  5573,
     5581,  5591,  5623,  5639,  5641,  5647,  5651,  5653,     5657,  5659,  5669,  5683,  5689,  5693,  5701,  5711,
     5717,  5737,  5741,  5743,  5749,  5779,  5783,  5791,     5801,  5807,  5813,  5821,  5827,  5839,  5843,  5849,
     5851,  5857,  5861,  5867,  5869,  5879,  5881,  5897,     5903,  5923,  5927,  5939,  5953,  5981,  5987,  6007,
     6011,  6029,  6037,  6043,  6047,  6053,  6067,  6073,     6079,  6089,  6091,  6101,  6113,  6121,  6131,  6133,
     6143,  6151,  6163,  6173,  6197,  6199,  6203,  6211,     6217,  6221,  6229,  6247,  6257,  6263,  6269,  6271,
     6277,  6287,  6299,  6301,  6311,  6317,  6323,  6329,     6337,  6343,  6353,  6359,  6361,  6367,  6373,  6379,
     6389,  6397,  6421,  6427,  6449,  6451,  6469,  6473,     6481,  6491,  6521,  6529,  6547,  6551,  6553,  6563,
     6569,  6571,  6577,  6581,  6599,  6607,  6619,  6637,     6653,  6659,  6661,  6673,  6679,  6689,  6691,  6701,
     6703,  6709,  6719,  6733,  6737,  6761,  6763,  6779,     6781,  6791,  6793,  6803,  6823,  6827,  6829,  6833,
     6841,  6857,  6863,  6869,  6871,  6883,  6899,  6907,     6911,  6917,  6947,  6949,  6959,  6961,  6967,  6971,
     6977,  6983,  6991,  6997,  7001,  7013,  7019,  7027,     7039,  7043,  7057,  7069,  7079,  7103,  7109,  7121,
     7127,  7129,  7151,  7159,  7177,  7187,  7193,  7207,     7211,  7213,  7219,  7229,  7237,  7243,  7247,  7253,
     7283,  7297,  7307,  7309,  7321,  7331,  7333,  7349,     7351,  7369,  7393,  7411,  7417,  7433,  7451,  7457,
     7459,  7477,  7481,  7487,  7489,  7499,  7507,  7517,     7523,  7529,  7537,  7541,  7547,  7549,  7559,  7561,
     7573,  7577,  7583,  7589,  7591,  7603,  7607,  7621,     7639,  7643,  7649,  7669,  7673,  7681,  7687,  7691,
     7699,  7703,  7717,  7723,  7727,  7741,  7753,  7757,     7759,  7789,  7793,  7817,  7823,  7829,  7841,  7853,
     7867,  7873,  7877,  7879,  7883,  7901,  7907,  7919,     7927,  7933,  7937,  7949,  7951,  7963,  7993,  8009,
     8011,  8017,  8039,  8053,  8059,  8069,  8081,  8087,     8089,  8093,  8101,  8111,  8117,  8123,  8147,  8161,
     8167,  8171,  8179,  8191,  8209,  8219,  8221,  8231,     8233,  8237,  8243,  8263,  8269,  8273,  8287,  8291,
     8293,  8297,  8311,  8317,  8329,  8353,  8363,  8369,     8377,  8387,  8389,  8419,  8423,  8429,  8431,  8443,
     8447,  8461,  8467,  8501,  8513,  8521,  8527,  8537,     8539,  8543,  8563,  8573,  8581,  8597,  8599,  8609,
     8623,  8627,  8629,  8641,  8647,  8663,  8669,  8677,     8681,  8689,  8693,  8699,  8707,  8713,  8719,  8731,
     8737,  8741,  8747,  8753,  8761,  8779,  8783,  8803,     8807,  8819,  8821,  8831,  8837,  8839,  8849,  8861,
     8863,  8867,  8887,  8893,  8923,  8929,  8933,  8941,     8951,  8963,  8969,  8971,  8999,  9001,  9007,  9011,
     9013,  9029,  9041,  9043,  9049,  9059,  9067,  9091,     9103,  9109,  9127,  9133,  9137,  9151,  9157,  9161,
     9173,  9181,  9187,  9199,  9203,  9209,  9221,  9227,     9239,  9241,  9257,  9277,  9281,  9283,  9293,  9311,
     9319,  9323,  9337,  9341,  9343,  9349,  9371,  9377,     9391,  9397,  9403,  9413,  9419,  9421,  9431,  9433,
     9437,  9439,  9461,  9463,  9467,  9473,  9479,  9491,     9497,  9511,  9521,  9533,  9539,  9547,  9551,  9587,
     9601,  9613,  9619,  9623,  9629,  9631,  9643,  9649,     9661,  9677,  9679,  9689,  9697,  9719,  9721,  9733,
     9739,  9743,  9749,  9767,  9769,  9781,  9787,  9791,     9803,  9811,  9817,  9829,  9833,  9839,  9851,  9857,
     9859,  9871,  9883,  9887,  9901,  9907,  9923,  9929,     9931,  9941,  9949,  9967,  9973, 10007, 10009, 10037,
    10039, 10061, 10067, 10069, 10079, 10091, 10093, 10099,    10103, 10111, 10133, 10139, 10141, 10151, 10159, 10163,
    10169, 10177, 10181, 10193, 10211, 10223, 10243, 10247,    10253, 10259, 10267, 10271, 10273, 10289, 10301, 10303,
    10313, 10321, 10331, 10333, 10337, 10343, 10357, 10369,    10391, 10399, 10427, 10429, 10433, 10453, 10457, 10459,
    10463, 10477, 10487, 10499, 10501, 10513, 10529, 10531,    10559, 10567, 10589, 10597, 10601, 10607, 10613, 10627,
    10631, 10639, 10651, 10657, 10663, 10667, 10687, 10691,    10709, 10711, 10723, 10729, 10733, 10739, 10753, 10771,
    10781, 10789, 10799, 10831, 10837, 10847, 10853, 10859,    10861, 10867, 10883, 10889, 10891, 10903, 10909, 10937,
    10939, 10949, 10957, 10973, 10979, 10987, 10993, 11003,    11027, 11047, 11057, 11059, 11069, 11071, 11083, 11087,
    11093, 11113, 11117, 11119, 11131, 11149, 11159, 11161,    11171, 11173, 11177, 11197, 11213, 11239, 11243, 11251,
    11257, 11261, 11273, 11279, 11287, 11299, 11311, 11317,    11321, 11329, 11351, 11353, 11369, 11383, 11393, 11399,
    11411, 11423, 11437, 11443, 11447, 11467, 11471, 11483,    11489, 11491, 11497, 11503, 11519, 11527, 11549, 11551,
    11579, 11587, 11593, 11597, 11617, 11621, 11633, 11657,    11677, 11681, 11689, 11699, 11701, 11717, 11719, 11731,
    11743, 11777, 11779, 11783, 11789, 11801, 11807, 11813,    11821, 11827, 11831, 11833, 11839, 11863, 11867, 11887,
    11897, 11903, 11909, 11923, 11927, 11933, 11939, 11941,    11953, 11959, 11969, 11971, 11981, 11987, 12007, 12011,
    12037, 12041, 12043, 12049, 12071, 12073, 12097, 12101,    12107, 12109, 12113, 12119, 12143, 12149, 12157, 12161,
    12163, 12197, 12203, 12211, 12227, 12239, 12241, 12251,    12253, 12263, 12269, 12277, 12281, 12289, 12301, 12323,
    12329, 12343, 12347, 12373, 12377, 12379, 12391, 12401,    12409, 12413, 12421, 12433, 12437, 12451, 12457, 12473,
    12479, 12487, 12491, 12497, 12503, 12511, 12517, 12527,    12539, 12541, 12547, 12553, 12569, 12577, 12583, 12589,
    12601, 12611, 12613, 12619, 12637, 12641, 12647, 12653,    12659, 12671, 12689, 12697, 12703, 12713, 12721, 12739,
    12743, 12757, 12763, 12781, 12791, 12799, 12809, 12821,    12823, 12829, 12841, 12853, 12889, 12893, 12899, 12907,
    12911, 12917, 12919, 12923, 12941, 12953, 12959, 12967,    12973, 12979, 12983, 13001, 13003, 13007, 13009, 13033,
    13037, 13043, 13049, 13063, 13093, 13099, 13103, 13109,    13121, 13127, 13147, 13151, 13159, 13163, 13171, 13177,
    13183, 13187, 13217, 13219, 13229, 13241, 13249, 13259,    13267, 13291, 13297, 13309, 13313, 13327, 13331, 13337,
    13339, 13367, 13381, 13397, 13399, 13411, 13417, 13421,    13441, 13451, 13457, 13463, 13469, 13477, 13487, 13499,
    13513, 13523, 13537, 13553, 13567, 13577, 13591, 13597,    13613, 13619, 13627, 13633, 13649, 13669, 13679, 13681,
    13687, 13691, 13693, 13697, 13709, 13711, 13721, 13723,    13729, 13751, 13757, 13759, 13763, 13781, 13789, 13799,
    13807, 13829, 13831, 13841, 13859, 13873, 13877, 13879,    13883, 13901, 13903, 13907, 13913, 13921, 13931, 13933,
    13963, 13967, 13997, 13999, 14009, 14011, 14029, 14033,    14051, 14057, 14071, 14081, 14083, 14087, 14107, 14143,
    14149, 14153, 14159, 14173, 14177, 14197, 14207, 14221,    14243, 14249, 14251, 14281, 14293, 14303, 14321, 14323,
    14327, 14341, 14347, 14369, 14387, 14389, 14401, 14407,    14411, 14419, 14423, 14431, 14437, 14447, 14449, 14461,
    14479, 14489, 14503, 14519, 14533, 14537, 14543, 14549,    14551, 14557, 14561, 14563, 14591, 14593, 14621, 14627,
    14629, 14633, 14639, 14653, 14657, 14669, 14683, 14699,    14713, 14717, 14723, 14731, 14737, 14741, 14747, 14753,
    14759, 14767, 14771, 14779, 14783, 14797, 14813, 14821,    14827, 14831, 14843, 14851, 14867, 14869, 14879, 14887,
    14891, 14897, 14923, 14929, 14939, 14947, 14951, 14957,    14969, 14983, 15013, 15017, 15031, 15053, 15061, 15073,
    15077, 15083, 15091, 15101, 15107, 15121, 15131, 15137,    15139, 15149, 15161, 15173, 15187, 15193, 15199, 15217,
    15227, 15233, 15241, 15259, 15263, 15269, 15271, 15277,    15287, 15289, 15299, 15307, 15313, 15319, 15329, 15331,
    15349, 15359, 15361, 15373, 15377, 15383, 15391, 15401,    15413, 15427, 15439, 15443, 15451, 15461, 15467, 15473,
    15493, 15497, 15511, 15527, 15541, 15551, 15559, 15569,    15581, 15583, 15601, 15607, 15619, 15629, 15641, 15643,
    15647, 15649, 15661, 15667, 15671, 15679, 15683, 15727,    15731, 15733, 15737, 15739, 15749, 15761, 15767, 15773,
    15787, 15791, 15797, 15803, 15809, 15817, 15823, 15859,    15877, 15881, 15887, 15889, 15901, 15907, 15913, 15919,
    15923, 15937, 15959, 15971, 15973, 15991, 16001, 16007,    16033, 16057, 16061, 16063, 16067, 16069, 16073, 16087,
    16091, 16097, 16103, 16111, 16127, 16139, 16141, 16183,    16187, 16189, 16193, 16217, 16223, 16229, 16231, 16249,
    16253, 16267, 16273, 16301, 16319, 16333, 16339, 16349,    16361, 16363, 16369, 16381, 16411, 16417, 16421, 16427,
    16433, 16447, 16451, 16453, 16477, 16481, 16487, 16493,    16519, 16529, 16547, 16553, 16561, 16567, 16573, 16603,
    16607, 16619, 16631, 16633, 16649, 16651, 16657, 16661,    16673, 16691, 16693, 16699, 16703, 16729, 16741, 16747,
    16759, 16763, 16787, 16811, 16823, 16829, 16831, 16843,    16871, 16879, 16883, 16889, 16901, 16903, 16921, 16927,
    16931, 16937, 16943, 16963, 16979, 16981, 16987, 16993,    17011, 17021, 17027, 17029, 17033, 17041, 17047, 17053,
    17077, 17093, 17099, 17107, 17117, 17123, 17137, 17159,    17167, 17183, 17189, 17191, 17203, 17207, 17209, 17231,
    17239, 17257, 17291, 17293, 17299, 17317, 17321, 17327,    17333, 17341, 17351, 17359, 17377, 17383, 17387, 17389,
    17393, 17401, 17417, 17419, 17431, 17443, 17449, 17467,    17471, 17477, 17483, 17489, 17491, 17497, 17509, 17519,
    17539, 17551, 17569, 17573, 17579, 17581, 17597, 17599,    17609, 17623, 17627, 17657, 17659, 17669, 17681, 17683,
    17707, 17713, 17729, 17737, 17747, 17749, 17761, 17783,    17789, 17791, 17807, 17827, 17837, 17839, 17851, 17863,
};

//-----------------------------------------------------------------------------
//	debug
//-----------------------------------------------------------------------------

void _iso_assert_msg(const char *filename, int line, const char *expr) {
	_iso_debug_print(format_string("%s(%d): %s\n", filename, line, expr));
}

//-----------------------------------------------------------------------------
//	compatibility
//-----------------------------------------------------------------------------

#if defined __clang__ && _MSC_VER >= 1924
extern "C" {
	uint128 __umodti3(uint128 a, uint128 b) {
		uint128	mod;
		fulldivmodc(a, uint128(0), b, mod);
		return mod;
	}
	uint128 __udivti3(uint128 a, uint128 b) {
		return fulldivc(a, uint128(0), b);
	}
	
	float __gnu_h2f_ieee(short param) {
		return reinterpret_cast<float16&>(param);
	}

	short __gnu_f2h_ieee(float param) {
		return force_cast<short>(float16(param));
	}

}
#endif

} // iso


