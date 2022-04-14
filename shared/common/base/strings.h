#ifndef STRINGS_H
#define STRINGS_H

#include "functions.h"
#include "bits.h"

#ifdef ISO_RELEASE
#define ISO_TRACEF(...)		iso::dummy_accum()
#define ISO_TRACEFI(...)	iso::dummy_accum()
#define ISO_PRINTVAR(x)
#else
#define ISO_TRACEF(...)		iso::trace_accum(__VA_ARGS__)
#define ISO_TRACEFI(...)	iso::trace_accum().formati(__VA_ARGS__)
#define ISO_PRINTVAR(x)		ISO_TRACEF(#x "=") << x << '\n'
#endif

#define ISO_ALWAYS_ASSERTF(exp, ...)	DO({ if (!(exp)) { iso::_iso_assert_msg(__FILE__, __LINE__, format_string(__VA_ARGS__)); _iso_break(); } })
#define ISO_ALWAYS_ASSUMEF(exp, ...)	DO({ if (!(exp)) iso::trace_accum(__VA_ARGS__); })

#define ISO_ASSERTF(exp, ...)	ISO_NOT_RELEASE_STMT(ISO_ALWAYS_ASSERTF(exp, __VA_ARGS__))
#define ISO_ASSUMEF(exp, ...)	ISO_NOT_RELEASE_STMT(ISO_ALWAYS_ASSUMEF(exp, __VA_ARGS__))
#define ISO_OUTPUTF(...)		iso::trace_accum(__VA_ARGS__)

#define throw_accum(x) iso_throw(((iso::toggle_accum&)(iso::toggle_accum() << x)).detach()->begin())

namespace iso {
//using ::strlen;
//using ::strcpy;
char *strdup(const char *s);

template<typename T> struct char_type {
	T	t;
	char_type()					{}
	explicit char_type(T _t) : t (_t) {}
	template<typename X> char_type& operator=(const X &x)	{ t = x; return *this; }
	operator uint32()	const	{ return t; }
};

template<typename T> struct T_swap_endian_type<char_type<T> >			{ typedef char_type<T_swap_endian<T> > type; };

typedef char_type<uint8>	char8;
typedef T_if<sizeof(wchar_t)==2, wchar_t, char16_t>::type	char16;
typedef T_if<sizeof(wchar_t)==4, wchar_t, char32_t>::type	char32;

typedef BE(char16)	char16be;
typedef LE(char16)	char16le;
typedef BE(char32)	char32be;
typedef LE(char32)	char32le;

template<typename T>	struct T_ischar							: T_false {};
template<typename T>	struct T_ischar<T&>						: T_ischar<T> {};
template<typename T>	struct T_ischar<constructable<T> >		: T_ischar<T> {};
template<typename T>	struct T_ischar<T_swap_endian_type<T> >	: T_ischar<T> {};
template<typename T>	struct T_ischar<char_type<T> >			: T_true {};
template<>				struct T_ischar<char>					: T_true {};
//template<>				struct T_ischar<char16>				: T_true {};
//template<>				struct T_ischar<char32>				: T_true {};
template<>				struct T_ischar<wchar_t>				: T_true {};
template<>				struct T_ischar<char16_t>				: T_true {};
template<>				struct T_ischar<char32_t>				: T_true {};

template<typename T> constexpr bool is_char = T_ischar<T>::value;

iso_export uint32	legalise_utf8(const char *srce, char *dest, size_t maxlen);
iso_export size_t	_format(char *dest, const char *format, va_list valist);
iso_export size_t	_format(char *dest, const char *format, ...);
iso_export size_t	_format(char *dest, size_t maxlen, const char *format, va_list valist);
iso_export size_t	_format(char *dest, size_t maxlen, const char *format, ...);

template<typename C> class accum;
template<typename C> class builder;

//-----------------------------------------------------------------------------
//	utf conversions
//-----------------------------------------------------------------------------

inline int max_utf8(int len) {
	int b = len == 1 ? 7 : 8 - (len + 1) + 6 * (len - 1);
	return (1 << b) - 1;
}

inline uint32 to_utf16_surrogates(uint32 c) {
	uint16 hi = 0xd800 + ((c - 0x10000) >> 10);
	uint16 lo = 0xdc00 + (c & 0x3FF);
	return (hi << 16) | lo;
}

inline char32 from_utf16_surrogates(uint16 a, uint16 b) {
	return between(min(a, b), 0xd800, 0xdbff) && between(max(a, b), 0xdc00, 0xddff)
		? ((max(a, b) - 0xd800) << 10) + (min(a, b) - 0xdc00) + 0x10000
		: 0;
}

//char_count<T>(F c): number of characters in T needed to represent c

template<typename C> uint32	char_count(char32 c, bool strict = true) {
	return 1;
}

template<> inline uint32	char_count<char16>(char32 c, bool strict) {
	return c < 0x10000 ? 1 : 2;
}

template<> inline uint32	char_count<char>(char32 c, bool strict) {
	if (c < (strict ? 0x80 : 0xc0))
		return 1;
	uint32	n = 2;
	while (n < 6 && c > (2 << (n * 5)))
		n++;
	return n;
}

//char_length(F c): number of characters that c should be the leading character of

iso_export uint32	char_length(char c);

inline uint32	char_length(char16 c) {
	return between(c, 0xd800, 0xdfff) ? 2 : 1;
}

//put_char(char32 c, T *p): write one char32 to p and return number of T chars written

template<typename C> uint32	put_char(C c, C *dest, bool strict = true) {
	*dest = c;
	return 1;
}
iso_export uint32	put_char(char32 c, char16 *dest, bool strict = true);
iso_export uint32	put_char(char32 c, char16be *dest, bool strict = true);
iso_export uint32	put_char(char32 c, char *dest, bool strict = true);

//get_char(char32 &c, const T *p): read chars from p for one char32 and return number of T chars read

template<typename C> uint32	get_char(char32 &c, const C *srce) {
	c = *srce;
	return 1;
}
iso_export uint32	get_char(char32 &c, const char16 *srce);
iso_export uint32	get_char(char32 &c, const char *srce);

//chars_count<T,F>(const F *srce): return number of T chars needed to represent string at srce

template<typename T, typename F> size_t chars_count(const F *srce, bool strict = true) {
	size_t	n = 0;
	if (srce) {
		while (auto c = *srce++)
			n += char_count<T>(c, strict);
	}
	return n;
}
template<typename T> size_t chars_count(T *srce, T *end, bool strict = true) {
	return end - srce;
}
template<typename T> size_t chars_count(const T *srce, const T *end, bool strict = true) {
	return end - srce;
}

template<typename T, typename I> size_t chars_count(I srce, I end, bool strict = true) {
	size_t	n = 0;
	while (srce != end)
		n += char_count<T>(*srce++, strict);
	return n;
}
template<typename T> size_t chars_count(const char *srce, bool strict = true) {
	size_t	n = 0;
	if (srce) {
		while (auto c = *srce) {
			srce += char_length(c);
			++n;
		}
	}
	return n;
}
template<typename T> size_t chars_count(const char *srce, const char *end, bool strict = true) {
	size_t	n = 0;
	while (srce != end) {
		srce += char_length(*srce);
		++n;
	}
	return n;
}
template<> inline size_t chars_count<char>(const char *srce, bool strict) {
	if (auto p = srce) {
		while (*p)
			++p;
		return p - srce;
	}
	return 0;
}
template<> inline size_t chars_count<char>(const char *srce, const char *end, bool strict) {
	return end - srce;
}

template<typename T, typename I> size_t chars_count(I s, size_t n, bool strict = true)	{
	return chars_count<T>(s, s + n, strict);
}

//chars_copy<T,F>(T *dest, const F *srce): copy chars from srce to dest and return number of T chars written

template<typename T, typename F> size_t chars_copy(T *dest, const F *srce, size_t maxlen = 0, bool strict = true) {
	T	*d	= dest;
	if (srce) {
		for (T *de = maxlen ? d + maxlen : (T*)~uintptr_t(0); d < de && *srce;)
			d += put_char(*srce++, d, strict);
	}
	return d - dest;
}
template<typename T, typename I> size_t chars_copy(T *dest, I srce, I end, size_t maxlen = 0, bool strict = true) {
	T	*d	= dest, *de = maxlen ? d + maxlen : (T*)~uintptr_t(0);
	while (d < de && srce != end)
		d += put_char(*srce++, d, strict);
	return d - dest;
}

template<typename C> enable_if_t<(sizeof(C) > 1), size_t> chars_copy(C *dest, const char *srce, size_t maxlen) {
	C *d = dest;
	if (srce) {
		char32	c;
		for (C *de = maxlen ? d + maxlen : (C*)~uintptr_t(0); d < de && *srce; *d++ = c)
			srce += get_char(c, srce);
	}
	return d - dest;
}
template<typename C> enable_if_t<(sizeof(C) > 1), size_t> chars_copy(C *dest, const char *srce, const char *end, size_t maxlen) {
	C			*d	= dest, *de = maxlen ? d + maxlen : (C*)~uintptr_t(0);
	while (d < de && srce < end) {
		char32	c;
		srce += get_char(c, srce);
		*d++ = c;
	}
	return d - dest;
}

template<typename T, typename I> size_t chars_copy(T *dest, I srce, size_t len, size_t maxlen, bool strict = true) {
	return chars_copy(dest, srce, srce + len, maxlen, strict);
}
/*
template<typename T, int N, typename I> size_t chars_copy(T (&dest)[N], I srce, I end, bool strict = true) {
	return chars_copy(dest, srce, end, N, strict);
}
template<typename T, int N, typename I> size_t chars_copy(T (&dest)[N], I srce, size_t len, bool strict = true) {
	return chars_copy(dest, srce, srce + len, N, strict);
}
*/

//-----------------------------------------------------------------------------
//	character tests
//-----------------------------------------------------------------------------

constexpr bool	is_cntrl(char c)					{ return c < ' ' || c == 127; }
constexpr bool	is_print(char c)					{ return between(c, ' ', 126); }
constexpr bool	is_graph(char c)					{ return between(c, 33, 126); }
constexpr bool	is_whitespace(char c)				{ return c > 0 && c <= ' '; }
constexpr bool	is_digit(char c)					{ return between(c, '0', '9'); }
constexpr bool	is_upper(char c)					{ return between(c, 'A', 'Z'); }
constexpr bool	is_lower(char c)					{ return between(c, 'a', 'z'); }
constexpr bool	is_alpha(char c)					{ return between(c, 'A', 'z') && (c <= 'Z' || c >= 'a'); }
constexpr bool	is_alphanum(char c)					{ return between(c, '0', 'z') && (c <= '9' || c >= 'a' || between(c, 'A', 'Z')); }
constexpr bool	is_wordchar(char c)					{ return is_alphanum(c) || c == '_'; }
constexpr bool	is_punct(char c)					{ return is_graph(c) && !is_alphanum(c); }
constexpr bool	is_hex(char c)						{ return between(c, '0', 'f') && (c <= '9' || c >= 'a' || between(c, 'A', 'F')); }

constexpr char	to_upper(char c)					{ return is_lower(c) ? c - ('a' - 'A') : c; }
constexpr char	to_lower(char c)					{ return is_upper(c) ? c + ('a' - 'A') : c; }
constexpr char	to_case(char c, bool upper)			{ return upper ? to_upper(c) : to_lower(c); }
constexpr char	to_digit(uint8 n, char ten = 'A')	{ return n < 10 ? n + '0' : n - 10 + ten; }
constexpr uint8 from_digit(char c)					{ return c <= '9' ? c - '0' : (c & 31) + 9; }

inline char end_char(char c) {
	switch (c) {
		case '[': return ']';
		case '{': return '}';
		case '(': return ')';
		case '"': case '\'': return c;
		default: return 0;
	}
}

template<typename C> void to_lower(C *dest, const C *srce) {
	if (srce) {
		while (C x = *srce++)
			*dest++ = to_lower(x);
	}
	if (dest)
		*dest = 0;
}

template<typename C> void to_upper(C *dest, const C *srce) {
	if (srce) {
		while (C x = *srce++)
			*dest++ = to_upper(x);
	}
	if (dest)
		*dest = 0;
}

class char_set : bitarray<256> {
	typedef	bitarray<256> B;
public:
	static char_set whitespace, digit, upper, lower, alpha, alphanum, identifier, ascii, control;
	using B::count_set;
	using B::next;

	char_set()									{}
	char_set(const B &b)		: B(b)			{}
	char_set(char c)							{ set((uint8)c); }
	char_set(char s, char e)					{ slice((uint8)s, (uint8)e - (uint8)s + 1).set_all(); }
	explicit char_set(const char *s)			{ while (*s) set((uint8)*s++);	}
	char_set&	operator+=(const char_set &b)	{ B::operator+=(b); return *this; }
	char_set&	operator-=(const char_set &b)	{ B::operator-=(b); return *this; }
	char_set&	operator*=(const char_set &b)	{ B::operator*=(b); return *this; }
	char_set	operator~()		const			{ return B(B::operator~());	}

	bool		test(char c)	const			{ return B::test((uint8)c); }

	friend char_set	operator|(const char_set &a, const char_set &b)		{ return char_set(a) += b; }
	friend char_set	operator+(const char_set &a, const char_set &b)		{ return char_set(a) += b; }
	friend char_set	operator-(const char_set &a, const char_set &b)		{ return char_set(a) -= b; }
	friend char_set	operator*(const char_set &a, const char_set &b)		{ return char_set(a) *= b; }
};

//-----------------------------------------------------------------------------
//	case_insensitive
//-----------------------------------------------------------------------------

template<typename T> struct case_insensitive {
	T	t;
	case_insensitive(T t) : t(t)	{}
	operator T()	const			{ return to_lower(t); }
};

template<typename T> inline auto make_case_insensitive(T t)					{ return case_insensitive<T>(t); }
template<typename T> inline auto make_case_insensitive(T *t)				{ return (case_insensitive<T>*)t; }
template<typename T> inline auto make_case_insensitive(const T *t)			{ return (const case_insensitive<T>*)t; }
template<typename T> inline auto make_case_insensitive(const range<T> &t)	{ return make_range(make_case_insensitive(t.begin()), make_case_insensitive(t.end())); }
template<typename T> inline auto make_case_insensitive(range<T> &t)			{ return make_range(make_case_insensitive(t.begin()), make_case_insensitive(t.end())); }
//template<typename T, int N> inline auto make_case_insensitive(T (&t)[N])	{ return (case_insensitive<T>(&)[N])t; };

template<typename T> struct char_traits {
	typedef	T							case_sensitive;
	typedef	iso::case_insensitive<T>	case_insensitive, case_other;
};
template<typename T> struct char_traits<case_insensitive<T> > {
	typedef	T							case_sensitive, case_other;
	typedef	iso::case_insensitive<T>	case_insensitive;
};

template<typename T> struct char_traits<const T> : char_traits<T> {};

typedef case_insensitive<char>		ichar;
typedef case_insensitive<char16>	ichar16;


template<typename C> uint32	put_char(char32 c, case_insensitive<C>* dest, bool strict = true) {
	return put_char(c, &dest->t);
}

//-----------------------------------------------------------------------------
//	compare char
//-----------------------------------------------------------------------------

template<typename A, typename B> constexpr int simple_compare(case_insensitive<A> a, B b)					{ return to_lower(a.t) - to_lower(b); }
template<typename A, typename B> constexpr int simple_compare(A a, case_insensitive<B> b)					{ return to_lower(a)   - to_lower(b.t); }
template<typename A, typename B> constexpr int simple_compare(case_insensitive<A> a, case_insensitive<B> b)	{ return to_lower(a.t) - to_lower(b.t); }

template<typename A, typename B> inline bool char_match(const A &a, const B &b) {
	return simple_compare(a, b) == 0;
}
template<typename A> inline bool char_match(const A &a, const char_set &set) {
	return set.test(a);
}

//-----------------------------------------------------------------------------
//	constexpr
//-----------------------------------------------------------------------------

constexpr meta::array<char, 2>	to_hex_string(uint8 u)	{ return {{to_digit(u >> 4, 'a'), to_digit(u & 15, 'a')}}; }
constexpr meta::array<char, 4>	to_hex_string(uint16 u)	{ return to_hex_string(uint8(u >> 8)) + to_hex_string(uint8(u)); }
constexpr meta::array<char, 8>	to_hex_string(uint32 u)	{ return to_hex_string(uint16(u >> 16)) + to_hex_string(uint16(u)); }
constexpr meta::array<char, 16>	to_hex_string(uint64 u)	{ return to_hex_string(uint32(u >> 32)) + to_hex_string(uint32(u)); }
#if USE_LONG
constexpr meta::array<char, 8>	to_hex_string(ulong u)	{ return to_hex_string(uint16(u >> 16)) + to_hex_string(uint16(u)); }
#endif

template<typename T, int N> struct to_hex_string_s	{ static constexpr auto	f(const T *p)	{ return to_hex_string(p[0]) + to_hex_string_s<T, N - 1>::f(p + 1); } };
template<typename T> struct to_hex_string_s<T, 0>	{ static constexpr auto	f(const T *)	{ return meta::array<char, 0>(); } };
template<int N, typename T> constexpr auto	to_hex_string(const T *p)		{ return to_hex_string_s<T, N>::f(p); }

template<int B> struct from_hex_string_s;
template<> struct from_hex_string_s<1> { template<size_t N> static constexpr uint8	f(const meta::array<char, N> &a, int o)	{ return (from_digit(a[o]) << 4) | from_digit(a[o + 1]); } };
template<> struct from_hex_string_s<2> { template<size_t N> static constexpr uint16	f(const meta::array<char, N> &a, int o)	{ return ((uint16)from_hex_string_s<1>::f(a, o) << 8)  | from_hex_string_s<1>::f(a, o + 2); } };
template<> struct from_hex_string_s<4> { template<size_t N> static constexpr uint32	f(const meta::array<char, N> &a, int o)	{ return ((uint32)from_hex_string_s<2>::f(a, o) << 16) | from_hex_string_s<2>::f(a, o + 4); } };
template<> struct from_hex_string_s<8> { template<size_t N> static constexpr uint64	f(const meta::array<char, N> &a, int o)	{ return ((uint64)from_hex_string_s<4>::f(a, o) << 32) | from_hex_string_s<4>::f(a, o + 8); } };
template<typename T, size_t N> constexpr T	from_hex_string(const meta::array<char, N> &a, int o = 0)						{ return (T)from_hex_string_s<sizeof(T)>::f(a, o); }


template<int B>				constexpr size_t	from_base_string(const char *a, size_t n, size_t i = 0, size_t x = 0)	{ return i == n ? x : from_base_string<B>(a, n, i + 1, x * B + from_digit(a[i])); }
template<int B, size_t N>	constexpr size_t	from_base_string(const char (&a)[N])		{ return from_base_string<B>(a, N); }

template<typename T, size_t N> struct constexpr_string : meta::array<T, N> {
	constexpr constexpr_string(const meta::array<T, N> &a)							: meta::array<T, N>(a) {}
	template<typename T1> constexpr constexpr_string(const T1 (&a)[N + 1])			: constexpr_string(meta::make_array<T, N>(a)) {}
	template<typename T1> constexpr constexpr_string(const meta::array<T1, N> &a)	: constexpr_string(meta::make_array<T>(a)) {}
};

template<typename T, size_t N> constexpr constexpr_string<T, N - 1>	to_constexpr_string(const T (&v)[N])				{ return v; }
template<typename T, size_t N> constexpr constexpr_string<T, N - 1>	to_constexpr_string(T (&v)[N])						{ return v; }
template<typename T, size_t N> constexpr constexpr_string<T, N>		to_constexpr_string(const constexpr_string<T,N> &v)	{ return v; }

template<typename T, size_t N1, size_t N2>	constexpr constexpr_string<T, N1 + N2>	operator+(const constexpr_string<T, N1> &a1, const constexpr_string<T, N2> &a2)	{
	return meta::concat<T>(a1.begin(), meta::make_index_list<N1>(), a2.begin(), meta::make_index_list<N2>());
}
template<typename T, size_t N1, size_t N2>	constexpr constexpr_string<T, N1 + N2>	operator+(const constexpr_string<T, N1> &a1, const meta::array<T, N2> &a2)	{
	return meta::concat<T>(a1.begin(), meta::make_index_list<N1>(), a2.begin(), meta::make_index_list<N2>());
}
template<typename T, size_t N1, size_t N2>	constexpr auto	operator+(const constexpr_string<T, N1> &a1, const T (&a2)[N2])	{ return a1 + to_constexpr_string(a2); }
template<typename T, size_t N1, size_t N2>	constexpr auto	operator+(const T (&a1)[N1], const constexpr_string<T, N2> &a2)	{ return to_constexpr_string(a1) + a2; }
template<typename T, size_t N1, size_t N2>	constexpr auto	operator+(const T (&a1)[N1], const meta::array<T, N2> &a2)		{ return to_constexpr_string(a1) + a2; }

constexpr auto to_constexpr_string(const meta::tuple<> &t)	{ return to_constexpr_string(""); }
template<typename T0, typename... T> constexpr auto to_constexpr_string(const meta::tuple<T0, T...> &t) {
	return to_constexpr_string(t.head) + to_constexpr_string(t.tail);
}

//-----------------------------------------------------------------------------
//	string_base helper functions
//-----------------------------------------------------------------------------

template<typename B> class string_base;

template<typename C> inline C *skip_whitespace(C *srce) {
	while (is_whitespace(*srce))
		srce++;
	return srce;
}
template<typename C> inline C *skip_whitespace(C *srce, C *end) {
	while ((!end || srce < end) && is_whitespace(*srce))
		srce++;
	return srce;
}

template<typename T> T*				string_end(T *s)				{ if (s) while (*s) s++; return s; }
template<typename T> inline size_t	string_len(T *s)				{ return string_end(s) - s; }
template<typename T> T*				string_end(T *s, T *e)			{ while (s < e && *s) s++; return s; }
template<typename T> inline size_t	string_len(T *s, T *e)			{ return string_end(s, e) - s; }
template<typename T> inline uint32	string_len32(T *s, T *e)		{ return uint32(string_len(s, e)); }
template<typename T> inline size_t	string_len(const range<T*> &r)	{ return string_len(r.begin(), r.end()); }
template<typename S> inline uint32	string_len32(const S &s)		{ return uint32(string_len(s)); }

template<typename T> inline size_t	string_term(T *d, size_t n)		{ if (d) d[n] = 0; return n; }

template<typename A, typename B> inline int string_cmp(const A *a, const B *b) {
	if (!a || !b)
		return a ? 1 : b ? -1 : 0;
	for (;;) {
		A	ca = *a++;
		B	cb = *b++;
		if (ca == 0 || cb == 0)
			return ca - cb;
		if (int d = simple_compare(ca, cb))
			return d;
	}
}
// no terminators in a or b
template<typename A, typename B> inline int string_cmp0(const A *a, const B *b, size_t n) {
	while (n--) {
		if (int d = simple_compare(*a++, *b++))
			return d;
	}
	return 0;
}
// terminator in a only
template<typename A, typename B> inline int string_cmp1(const A *a, const B *b, size_t n) {
	if (!a)
		return n ? -1 : 0;
	while (n--) {
		A	ca = *a++;
		if (int d = simple_compare(ca, *b++))
			return d;
		if (ca == 0)
			return n == 0 ? 0 : -1;
	}
	return *a != 0;
}
// terminator in a or b
template<typename A, typename B> inline int string_cmp(const A *a, const B *b, size_t n) {
	if (!a)
		return n && b[0] ? -1 : 0;
	while (n--) {
		A	ca = *a++;
		B	cb = *b++;
		if (ca == 0 || cb == 0)
			return ca - cb;
		if (int d = simple_compare(ca, cb))
			return d;
	}
	return 0;
}

template<typename A, typename B> inline int string_cmp(const A *a, const B *b, const _none&) {
	return string_cmp(a, b);
}
template<typename A, typename B> inline int string_cmp(const A *a, const B *b, const A *ae) {
	return -string_cmp1(b, a, ae - a);
}
template<typename A, typename B> inline int string_cmp(const A *a, const B *b, const _none&, const B *be) {
	return string_cmp1(a, b, be - b);
}
template<typename A, typename B> inline int string_cmp(const A *a, const B *b, const A *ae, const B *be) {
	if (int r = string_cmp0(a, b, min(ae - a, be - b)))
		return r;
	auto	d = (ae - a) - (be - b);
	return d && (d < 0 ? be[d] == 0 : ae[-d] == 0) ? 0 : int(d);
}

// find char or char_set (forward)

template<typename T, typename S> T *string_find(T *p, S c, T *e) {
	while (p < e) {
		if (char_match(*p, c))
			return p;
		++p;
	}
	return 0;
}
template<typename T, typename S> T *string_find(T *p, S c) {
	if (p) {
		while (T c2 = *p) {
			if (char_match(c2, c))
				return p;
			++p;
		}
	}
	return 0;
}
template<typename T, typename S> inline T *string_find(T *p, S c, const _none&) {
	return string_find(p, c);
}

// find string (forward)

template<typename T, typename S> T *string_find(T *p, S *s, T *e) {
	if (p) {
		size_t	len = string_len(s);
		for (e -= len; p <= e; p++) {
			if (string_cmp(p, s, len) == 0)
				return p;
		}
	}
	return 0;
}
template<typename T, typename S> inline	T *string_find(T *p, S *s, T *pe, S *se) {
	if (p) {
		size_t	len = se - s;
		for (pe -= len; p <= pe; p++) {
			if (string_cmp(p, s, len) == 0)
				return p;
		}
	}
	return 0;
}

template<typename T, typename S> inline	T *string_find(T *p, S *s) {
	return string_find(p, s, string_end(p));
}
template<typename T, typename S> inline T *string_find(T *p, S *s, const _none&) {
	return string_find(p, s, string_end(p));
}
template<typename T, typename S> inline T *string_find(T *p, S *s, const _none&, const _none&) {
	return string_find(p, s, string_end(p));
}
template<typename T, typename S> inline T *string_find(T *p, S *s, const _none&, S *se) {
	return string_find(p, s, string_end(p), se);
}

// find char or char_set (reverse)

template<typename T, typename S> T *string_rfind(T *p, S c, T *e) {
	while (p < e) {
		if (char_match(*--e, c))
			return e;
	}
	return 0;
}

// find string (reverse)

template<typename T, typename S> T *string_rfind(T *p, S *s, T *e) {
	size_t	len = string_len(s);
	for (e -= len; e >= p; e--) {
		if (string_cmp(e, s, len) == 0)
			return e;
	}
	return 0;
}

// find number of matches of char or char_set

template<typename T, typename S> int string_count(const T *s, S c, const T *e) {
	int	n = 0;
	while (const T *p = string_find(s, c, e)) {
		s = p + 1;
		++n;
	}
	return n;
}
template<typename T, typename S> int string_count(const T *s, S c) {
	int	n = 0;
	while (const T *p = string_find(s, c)) {
		s = p + 1;
		++n;
	}
	return n;
}

// string_copy

template<typename S, typename D> size_t	string_copy(D *d, const S *s, size_t maxlen = 0)	{ return string_term(d, chars_copy(d, s, maxlen)); }
template<typename I, typename D> size_t	string_copy(D *d, I s, I e, size_t maxlen = 0)		{ return string_term(d, chars_copy(d, s, e, maxlen)); }
template<typename D, typename I> size_t	string_copy(D *d, I s, size_t n, size_t maxlen = 0)	{ return string_copy(d, s, s + n, maxlen); }

template<typename C> void replace(C *dest, const C *srce, const C *from, const C *to) {
	size_t		flen	= string_len(from);
	//size_t		tlen	= string_len(to);

	while (const C *found = string_find(srce, from)) {
		dest += chars_copy(dest, srce, found);
		//memcpy(dest, srce, (found - srce) * sizeof(C));
		//dest += found - srce;
		dest += chars_copy(dest, to);
		//memcpy(dest, to, tlen * sizeof(C));
		//dest += tlen;
		srce = found + flen;
	}
	string_copy(dest, srce);
}

template<typename C> void replace(C *s, const C from, const C to) {
	while (*s) {
		if (*s == from)
			*s = to;
		++s;
	}
}

template<typename I> constexpr string_base<range<I>> string_slice(int s, int n, I b, I e)	{
	return range<I>(b ? (s < 0 ? e : b) + s : b, b ? (n < 0 ? e : b) + n : b);
}
template<typename I> constexpr string_base<range<I>> string_slice(int s, int n, I b, const _none&)	{
	return string_slice(s, n, b, b && (s < 0 || n < 0) ? string_end(b) : b);
}
template<typename I> constexpr string_base<range<I>> string_slice(int s, I b, I e) {
	return range<I>(e ? (s < 0 ? e : b) + s : e, e);
}
template<typename I> constexpr string_base<I> string_slice(int s, I b, const _none&) {
	return (s < 0 ? string_end(b) : b) + s;
}

// check whole string matches set

inline bool string_check(const char *s, const char_set &set) {
	while (char c = *s++) {
		if (!set.test(c))
			return false;
	}
	return true;
}
inline bool string_check(const char *s, size_t n, const char_set &set) {
	while (n--) {
		if (!set.test(*s++))
			return false;
	}
	return true;
}
inline int string_check(const char *s, const char_set &set, const _none&) {
	return string_check(s, set);
}
inline int string_check(const char *s, const char_set &set, const char *se) {
	return string_check(s, se - s, set);
}

// in defs.cpp (using FNV1)
iso_export uint32 string_hash(const char *s);
iso_export uint32 string_hash(const char16 *s);
iso_export uint32 string_hash(const char *s, size_t n);
iso_export uint32 string_hash(const char16 *s, size_t n);

//-----------------------------------------------------------------------------
//	read_to/read_line
//-----------------------------------------------------------------------------
/*
template<typename R> bool read_to(R &r, const char_set &set, char *p, uint32 n) {
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

template<typename R> bool read_line(R &r, char *p, uint32 n) {
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

template<typename R, int N> bool read_to(R &r, const char_set &set, char (&p)[N]) {
	return read_to(r, set, p, N);
}

template<typename R, int N> bool read_line(R &r, char (&p)[N]) {
	return read_line(r, p, N);
}
*/
//-----------------------------------------------------------------------------
//	string_getter - for deferred copying of strings from functions
//-----------------------------------------------------------------------------

template<typename T> struct string_getter {
	T	t;

	template<typename T1> string_getter(T1 &&t) : t(forward<T1>(t)) {}
	size_t	len()							const	{ return t.string_len(); }			// default impl
	size_t	get(char *s, size_t len)		const	{ return t.string_get(s, len); }	// default impl
	size_t	get(char16 *s, size_t len)		const	{ return t.string_get(s, len); }	// default impl

	template<typename S>	bool operator==(const S &s)	{ return s == *this; }
	template<typename S>	bool operator!=(const S &s)	{ return s != *this; }
	template<typename S>	bool operator< (const S &s)	{ return s >  *this; }
	template<typename S>	bool operator<=(const S &s)	{ return s >= *this; }
	template<typename S>	bool operator> (const S &s)	{ return s <  *this; }
	template<typename S>	bool operator>=(const S &s)	{ return s <= *this; }
};

// string_getter helper for char/char16 transforms
template<typename A, typename B, typename T> static size_t string_getter_transform(T &t, B *b, size_t len) {
	A	*a = alloc_auto(A, len + 1);
	return string_copy(b, a, t.string_get(a, len));
}

template<typename C, typename T> inline size_t to_string(C *s, const string_getter<T> &g) {
	return g.get(s, g.len());
}

struct from_string_getter {
	const char *s;
	from_string_getter(const char *s) : s(s) {}
	explicit operator bool() const { return !!s; }
	template<typename T> constexpr operator T()			const;// { return s ? from_string<T>(s) : T(); }
	template<typename T> constexpr bool read(T &t)		const;// { return s && from_string(s, t); }
	template<typename T> bool operator==(const T &t)	const { operator T() == t; }
	bool	operator==(const char *b)					const { return string_cmp(s, b) == 0; }
	template<typename T> T or_default(const T &def)		const;// { return s ? from_string<T>(s) : def; }
};

inline from_string_getter from_string(const char *s) { return s; }

//-----------------------------------------------------------------------------
//	string traits
//-----------------------------------------------------------------------------

template<typename S> struct string_traits : container_traits<S> {
	typedef iterator_t<S>	iterator, start_type;
	static constexpr start_type		start(S &s)				{ return begin(s);	}
	static constexpr iterator		begin(S &s)				{ using iso::begin; return begin(s);	}
	static constexpr iterator		end(S &s)				{ return string_end(begin(s));	}
	static constexpr _none			terminator(const S &s)	{ return none; }
	static constexpr size_t			len(const S &s)			{ return string_len(begin(s));	}
};

template<typename T, int N> struct string_traits<T(&)[N]> {
	typedef T element, *iterator, *start_type;
	static constexpr T*				start(T *s)				{ return s;	}
	static constexpr T*				begin(T *s)				{ return s;	}
	static constexpr T*				end(T *s)				{ return s + N;	}
	static constexpr T*				terminator(T *s)		{ return s + N; }
	static constexpr size_t			len(T *s)				{ return N;	}
};

template<typename T> struct string_traits<range<T*> > : container_traits<range<T*> > {
	typedef range<T*>	S;
	typedef void		start_type;
	typedef T*			iterator;
	static constexpr _none			start(const S &s)		{ return none;		}
	static constexpr iterator		begin(S &s)				{ return s.begin();	}
	static constexpr T*				end(const S &s)			{ return s.end();	}
	static constexpr T*				terminator(const S &s)	{ return s.end();	}
	static constexpr size_t			len(const S &s)			{ return s.size();	}
};
template<typename T> struct string_traits<const range<T*> > : container_traits<const range<T*> > {
	typedef const range<T*>	S;
	typedef void			start_type;
	typedef T*				iterator;
	static constexpr _none			start(S &s)				{ return none;		}
	static constexpr iterator		begin(S &s)				{ return s.begin();	}
	static constexpr T*				end(S &s)				{ return s.end();	}
	static constexpr T*				terminator(S &s)		{ return s.end();	}
	static constexpr size_t			len(S &s)				{ return s.size();	}
};

//-----------------------------------------------------------------------------
//	string_base
//-----------------------------------------------------------------------------

template<class W, int N>				inline bool	write(W &w, const char (&s)[N])			{ return w.writebuff(s, N - 1) == N - 1; }
template<class W>						inline bool	write(W &w, const char *s)				{ return !s || check_writebuff(w, s, string_len(s)); }
template<class W>						inline bool	write(W &w, char *s)					{ return !s || check_writebuff(w, s, string_len(s)); }

template<class W, int N>				inline bool	write(W &w, const char16 (&s)[N])		{ return w.writebuff(s, N - 1) == N - 1; }
template<class W>						inline bool	write(W &w, const char16 *s)			{ return !s || check_writebuff(w, s, string_len(s)); }
template<class W>						inline bool	write(W &w, char16 *s)					{ return !s || check_writebuff(w, s, string_len(s)); }

template<typename B> class string_base {
protected:
	B			p;
	typedef	string_traits<B>		traits;
	typedef	string_traits<const B>	const_traits;

public:
	typedef typename traits::element			element;
	typedef	typename traits::iterator			iterator;
	typedef	typename const_traits::iterator		const_iterator;

	constexpr string_base()											{}
	constexpr string_base(const B &p) : p(p)						{}
	B&							representation()					{ return p; }
	constexpr const B&			representation()			const	{ return p; }

	constexpr size_t			length()					const	{ return const_traits::len(p);	}
	constexpr uint32			size32()					const	{ return uint32(const_traits::len(p));	}
	constexpr auto				begin()						const	{ return const_traits::begin(p);}
	constexpr auto				end()						const	{ return const_traits::end(p);	}
	constexpr auto				begin()								{ return traits::begin(p);		}
	constexpr auto				end()								{ return traits::end(p);		}
	auto						front()						const	{ return *begin(); }
	auto						back()						const	{ return end()[-1]; }
	constexpr auto				operator[](intptr_t i)		const	{ return begin()[i]; }
	constexpr auto&				operator[](intptr_t i)				{ return begin()[i]; }

	constexpr bool				blank()						const	{ return length() == 0; }
	constexpr bool				empty()						const	{ return length() == 0; }
	constexpr bool				operator!()					const	{ return length() == 0; }

	constexpr operator typename const_traits::start_type()	const	{ return const_traits::start(p); }
	operator typename traits::start_type()							{ return traits::start(p); }

	template<typename S> auto	find(const S &s)					{ return string_find(begin(), s, traits::terminator(p)); }
	template<typename S> auto	rfind(const S &s)					{ return string_rfind(begin(), s, end()); }
	template<typename S> auto	find(const S &s)			const	{ return string_find(begin(), s, const_traits::terminator(p)); }
	template<typename S> auto	rfind(const S &s)			const	{ return string_rfind(begin(), s, end()); }

	template<typename S> bool	begins(const S &s)			const	{ return string_cmp(begin(), iso::begin(s), string_len(s)) == 0; }
	template<typename S> bool	ends(const S &s)			const	{ size_t len = string_len(s); return len <= length() && string_cmp(end() - len, iso::begin(s), len) == 0; }

	auto						slice(int s)						{ return string_slice(s, begin(), traits::terminator(p)); }
	auto						slice(int s)				const	{ return string_slice(s, begin(), traits::terminator(p)); }
	auto						slice(int s, int n)					{ return string_slice(s, n, begin(), traits::terminator(p)); }
	constexpr auto				slice(int s, int n)			const	{ return string_slice(s, n, begin(), traits::terminator(p)); }
	constexpr auto				slice(const_iterator s)		const	{ return str(range<const_iterator>(s, end())); }
	constexpr auto				slice_to(const_iterator e)	const	{ return str(range<const_iterator>(begin(), e ? e : end())); }
	template<typename S> auto	slice_to_find(const S &s)	const	{ return slice_to(find(s)); }

	bool						check(const char_set &set)				const	{ return string_check(begin(), set, const_traits::terminator(p)); }

	template<typename B2> int	compare_to(const string_base<B2> &s2)	const	{ return -s2.compare_to(begin(), const_traits::terminator(p)); }
	template<typename C> int	compare_to(const C *s2)					const	{ return string_cmp(begin(), s2, const_traits::terminator(p)); }
	template<typename C> int	compare_to(const C *s2, const _none&)	const	{ return string_cmp(begin(), s2, const_traits::terminator(p)); }
	template<typename C> int	compare_to(const C *s2, const C *e2)	const	{ return string_cmp(begin(), s2, const_traits::terminator(p), e2); }
	int							compare_to(const_iterator s2)			const	{ return string_cmp(begin(), s2, const_traits::terminator(p)); }

	template<typename T> int	compare_to(const string_getter<T> &s2)	const	{
		size_t	len	= s2.len();
		len	= len ? min(length() + 1, len) : length() + 1;
		element	*p	= alloc_auto(element, len + 1);
		len = s2.get(unconst(p), len);
		return compare_to(p, p + len);
	}

	const_memory_block			data()					const	{ return const_memory_block(begin(), length() * sizeof(element)); }
//	explicit constexpr operator const_memory_block()	const	{ return const_memory_block(begin(), length() * sizeof(element)); }
	template<typename R> bool	read(R &r)			{ return r.read(p); }
	template<typename W> bool	write(W &w) const	{ return w.write(p); }

	// friends

	friend						void	swap(string_base &a, string_base &b)					{ swap(a.p, b.p); }
	friend						uint32	hash(const string_base &s)								{ return string_hash(s.begin(), s.length()); }
	friend						size_t	string_len(const string_base &s)						{ return s.length(); }
	template<typename D> friend	size_t	string_copy(D *d, const string_base &s)					{ return string_copy(d, s.begin(), s.end()); }
	template<typename D> friend	size_t	chars_count(const string_base &s)						{ return chars_count<D>(s.begin(), s.end()); }
	template<typename T> friend	T*		string_find(T *p, const string_base &s, const _none&)	{ return string_find(p, s.begin(), none, s.end());	}
	template<typename T> friend	T*		string_find(T *p, const string_base &s, T *e)			{ return string_find(p, s.begin(), e, s.end());	}
};

template<typename B, typename T>			inline int	compare(const string_base<B> &s1, const T &s2)		{ return s1.compare_to(s2); }
template<typename B, typename C, int N>		inline int	compare(const string_base<B> &s1, const C (&s2)[N])	{ return s1.compare_to(&s2[0], &s2[N - 1]); }
template<typename B, typename C, int N>		inline int	compare(const string_base<B> &s1, C (&s2)[N])		{ return s1.compare_to(&s2[0]); }
template<typename B, typename C, size_t N>	inline int	compare(const string_base<B> &s1, const meta::array<C,N> &s2)	{ return s1.compare_to(s2.begin()); }
template<typename B, typename T>			inline bool operator==(const string_base<B> &s1, T &&s2)		{ return compare(s1, s2) == 0;	}
template<typename B, typename T>			inline bool operator!=(const string_base<B> &s1, T &&s2)		{ return compare(s1, s2) != 0;	}
template<typename B, typename T>			inline bool operator< (const string_base<B> &s1, T &&s2)		{ return compare(s1, s2) <  0;	}
template<typename B, typename T>			inline bool operator<=(const string_base<B> &s1, T &&s2)		{ return compare(s1, s2) <= 0;	}
template<typename B, typename T>			inline bool operator> (const string_base<B> &s1, T &&s2)		{ return compare(s1, s2) >  0;	}
template<typename B, typename T>			inline bool operator>=(const string_base<B> &s1, T &&s2)		{ return compare(s1, s2) >= 0;	}
template<typename B, typename T>			inline bool operator==(const T *s1, const string_base<B> &s2)	{ return compare(s2, s1) == 0; }
template<typename B, typename T>			inline bool operator!=(const T *s1, const string_base<B> &s2)	{ return compare(s2, s1) != 0; }
template<typename B, typename T>			inline bool operator< (const T *s1, const string_base<B> &s2)	{ return compare(s2, s1) >  0; }
template<typename B, typename T>			inline bool operator<=(const T *s1, const string_base<B> &s2)	{ return compare(s2, s1) >= 0; }
template<typename B, typename T>			inline bool operator> (const T *s1, const string_base<B> &s2)	{ return compare(s2, s1) <  0; }
template<typename B, typename T>			inline bool operator>=(const T *s1, const string_base<B> &s2)	{ return compare(s2, s1) <= 0; }

template<typename T> struct is_string : yesno {
	template<typename B> static yes test(const string_base<B>*);
	static no test(...);
	static bool const value = sizeof(test((T*)0)) == sizeof(yes);
};

typedef string_base<const char*>	cstring;
typedef string_base<const char16*>	cstring16;

//-----------------------------------------------------------------------------
//	string_param - for passing strings to functions
//-----------------------------------------------------------------------------

template<typename C> class alloc_string;

template<typename C> using string_param_storage = pointer_pair<const C, bool, 2>;

template<typename C> class string_paramT : public string_base<string_param_storage<C>> {
	typedef string_base<string_param_storage<C>>	B;
	C		*alloc(size_t n) {
		C *c = (C*)malloc((n + 1) * sizeof(C));
		B::p = {c, true}; return c;
	}
	template<typename T> void init(const string_base<T> &s) { 
		auto	n = chars_count<C>(s);
		auto	n2 = string_copy(alloc(n), s);
		ISO_ASSERT(n == n2);
	}

public:
	constexpr					string_paramT()									: B(0) {}
	constexpr					string_paramT(const _none&)						: B(0) {}
	constexpr					string_paramT(string_param_storage<C> p)		: B(p) {}
	constexpr					string_paramT(alloc_string<C> &&s)				: B({s.detach(), true}) {}
	constexpr					string_paramT(builder<C> &&s)					: B({s.detach(), true}) {}
	constexpr					string_paramT(string_paramT &&s)				: B(s)			{ s.p.b = false; }
	constexpr					string_paramT(const C *s)						: B(s)			{}
	constexpr					string_paramT(const string_base<C*> &s)			: B(s.begin())	{}
	constexpr					string_paramT(const string_base<const C*> &s)	: B(s.begin())	{}
	template<int N> constexpr	string_paramT(string_base<C[N]> &s)				: B(s.begin())	{}
	template<int N> constexpr	string_paramT(const string_base<const C[N]> &s)	: B(s.begin())	{}

	template<typename T>		string_paramT(const T *s)						{ string_copy(alloc(chars_count<C>(s)), s); }
	template<typename T>		string_paramT(const string_base<T> &s)			{ init(s); }
	template<typename T>		string_paramT(const string_getter<T> &s)		{ init(string(s)); }

	template<typename F, typename = exists_t<decltype(declval<F>()(declval<accum<C>&>())), accum<C>&>> string_paramT(F &&f) : string_paramT(builder<C>(f)) {}

//	string_paramT(accum<C> &s);
//	string_paramT(accum<C> &&s);

	string_paramT&	operator=(string_paramT &&b) { raw_swap(*this, b); return *this; }

	//template<typename T>		string_paramT(const string_base<T> &s, enable_if_t<T_same<typename string_base<T>::element, C>::value,int> dummy = 0) : B(s.begin()), mem(0) {}
	~string_paramT()	{ if (B::p.b) free((void*)B::p.a.get()); }
	C*		detach()	{ return B::p.b ? (C*)exchange(B::p, nullptr).a.get() : strdup(B::p); }

	friend uint32	hash(const string_paramT &s)	{ return string_hash(s.p.a); }
	template<typename W> bool	write(W &w) const	{ return w.write(B::begin()); }
};

typedef string_paramT<char>		string_param;
typedef string_paramT<char16>	string_param16;
typedef const string_param&		string_ref;
typedef const string_param16&	string_ref16;

template<typename T> struct named : string_paramT<char>, T {
	named()	{}
	template<typename N, typename... P> named(N &&name, P&&... p) : string_param(forward<N>(name)), T(forward<P>(p)...) {}
	const string_paramT<char>&	name()	const { return *this; }
//	bool	operator==(const char *s) const { return str(_name::name) == s; }
};


//-----------------------------------------------------------------------------
//	char_string	- single character
//-----------------------------------------------------------------------------

template<typename C> struct char_string_traits {
	typedef C				element, *start_type, *iterator;
	static C				*begin(C &s)			{ return &s; }
	static C				*start(C &s)			{ return &s; }
	static C				*end(C &s)				{ return &s + 1; }
	static inline _none		terminator(const C &s)	{ return none; }
	static inline size_t	len(const C &s)			{ return 1; }
};

template<> struct string_traits<const char>		: char_string_traits<const char>	{};
template<> struct string_traits<const char16>	: char_string_traits<const char16>	{};
template<> struct string_traits<const ichar>	: char_string_traits<const char>	{};
template<> struct string_traits<const ichar16>	: char_string_traits<const ichar16> {};

#if 1
typedef string_base<char>		char_string;
typedef string_base<char16>		char_string16;
typedef string_base<ichar>		ichar_string;
typedef string_base<ichar16>	ichar_string16;
#else
template<typename T> class char_stringT : public string_base<T> {
public:
	constexpr char_stringT()		{}
	constexpr char_stringT(T t)		: string_base<T>(t)	{}
};

typedef char_stringT<char>		char_string;
typedef char_stringT<char16>	char_string16;
typedef char_stringT<ichar>		ichar_string;
typedef char_stringT<ichar16>	ichar_string16;
#endif

//-----------------------------------------------------------------------------
//	count_string (no terminator)
//-----------------------------------------------------------------------------

template<typename T> class count_stringT : public string_base<range<T*> > {
	typedef string_base<range<T*>>	B;
public:
	constexpr					count_stringT()				{}
	constexpr					count_stringT(const _none&) {}
	constexpr					count_stringT(const B &b)		: B(b)		{}
	template<int N> constexpr	count_stringT(const T (&p)[N])	: B(range<T*>(p, p + N - 1))		{}
	explicit constexpr			count_stringT(T *p)				: B(range<T*>(p, string_end(p)))	{}
	constexpr					count_stringT(T *p, size_t n)	: B(range<T*>(p, p + n))			{}
	constexpr					count_stringT(T *p, T *e)		: B(range<T*>(p, e))				{}
//	constexpr					count_stringT(const count_stringT<const typename char_traits<T>::case_other> &b)
//		: B(range<T*>((T*)b.begin(), (T*)b.end()))	{}
	template<typename S> explicit count_stringT(const string_base<S> &b) : B(range<T*>((T*)b.begin(), (T*)b.end()))	{}
//	template<int N>				count_stringT(const string_base<noconst_t<T>[N]> &b) : B(range<T*>((T*)b.begin(), (T*)b.end()))	{}


	constexpr bool				operator!()				const	{ return this->p.size() == 0;	}
	constexpr explicit			operator bool()			const	{ return !!*this; }

	friend	uint32	hash(const count_stringT &s)	{ return string_hash(s.begin(), s.length()); }

};

typedef count_stringT<const char>		count_string;
typedef count_stringT<const char16>		count_string16;
typedef count_stringT<const ichar>		icount_string;
typedef count_stringT<const ichar16>	icount_string16;

template<typename C, typename C2>	size_t	string_copy(string_base<range<C*>> &s, const C2 *p)					{ return chars_copy(s.begin(), p, s.length()); }
template<typename C, typename C2>	size_t	string_copy(string_base<range<C*>> &s, const C2 *p, const C2 *e)	{ return chars_copy(s.begin(), p, e, s.length()); }
template<typename C, typename B>	size_t	string_copy(string_base<range<C*>> &s, const string_base<B> &b)		{ return string_copy(s, b.begin(), b.end()); }
template<typename C, typename C2>	size_t	string_copy(string_base<range<C*>> &&s, const C2 *p)				{ return chars_copy(s.begin(), p, s.length()); }
template<typename C, typename C2>	size_t	string_copy(string_base<range<C*>> &&s, const C2 *p, const C2 *e)	{ return chars_copy(s.begin(), p, e, s.length()); }
template<typename C, typename B>	size_t	string_copy(string_base<range<C*>> &&s, const string_base<B> &b)	{ return string_copy(s, b.begin(), b.end()); }

template<typename C> auto trim(const C *s, const C *e) {
	s = skip_whitespace(s);
	while (is_whitespace(e[-1]))
		--e;
	return count_stringT<const C>(s, e);
}
template<typename C> inline auto	trim(const C *s)				{ return trim(s, string_end(s)); }
template<typename B> inline auto	trim(const string_base<B> &s)	{ return trim(s.begin(), s.end()); }

//-----------------------------------------------------------------------------
//	string_buffer (terminator)
//-----------------------------------------------------------------------------

template<typename T> class string_bufferT : public string_base<range<T*> > {
public:
	template<int N> constexpr	string_bufferT(T (&p)[N])		: string_base<range<T*> >(range<T*>(p, p + N - 1))		{}
	constexpr					string_bufferT(T *p, size_t n)	: string_base<range<T*> >(range<T*>(p, p + n))		{}
	constexpr					string_bufferT(T *p, T *e)		: string_base<range<T*> >(range<T*>(p, e))			{}

	template<typename C2>	friend size_t	string_copy(const string_bufferT &s, const C2 *p)				{ return string_copy(s.begin(), p, s.length()); }
	template<typename C2>	friend size_t	string_copy(const string_bufferT &s, const C2 *p, const C2 *e)	{ return string_copy(s.begin(), p, e, s.length()); }
	template<typename B>	friend size_t	string_copy(const string_bufferT &s, const string_base<B> &b)	{ return string_copy(s, b.begin(), b.end()); }
};

typedef string_bufferT<char>	string_buffer;
typedef string_bufferT<char16>	string_buffer16;

//-----------------------------------------------------------------------------
//	embedded_string
//-----------------------------------------------------------------------------

template<typename T> struct embedded { T t; };

template<typename C> struct string_traits<embedded<C> > {
	typedef C				element, *start_type, *iterator;
	static C				*begin(embedded<C> &s)				{ return &s.t; }
	static C				*start(embedded<C> &s)				{ return &s.t; }
	static C				*end(embedded<C> &s)				{ return string_end(&s.t); }
	static inline _none		terminator(const embedded<C> &s)	{ return none; }
	static inline size_t	len(const embedded<C> &s)			{ return string_len(&s.t); }
};
template<typename C> struct string_traits<const embedded<C> > : string_traits<embedded<C> > {
	typedef const C			*start_type, *iterator;
	static const C			*begin(const embedded<C> &s)		{ return &s.t; }
	static const C			*start(const embedded<C> &s)		{ return &s.t; }
	static const C			*end(const embedded<C> &s)			{ return string_end(&s.t); }
};

typedef string_base<embedded<char> >	embedded_string;
typedef string_base<embedded<char16> >	embedded_string16;

template<typename C> const embedded_string*	next(const string_base<embedded<C>> *t)			{ return (const string_base<embedded<C>>*)(t->end() + 1);}
template<typename C> void*					get_after(const string_base<embedded<C>> *t)	{ return (void*)(t->end() + 1); }

//-----------------------------------------------------------------------------
//	pascal_string (length, then chars)
//-----------------------------------------------------------------------------

template<typename U=uint8, typename C=char, int N = 0> struct _pascal_string {
	U		len;
	C		buffer[N ? N : 1];
	_pascal_string()	{}
	_pascal_string(const C *s) : len(U(string_len(s))) { memcpy(buffer, s, len * sizeof(C)); }

//	friend auto	str(const _pascal_string &p) { return str(p.buffer, p.len); }

	template<typename R> friend bool read(R &r, _pascal_string &s) {
		s.len = r.template get<U>();
		return check_readbuff(r, s.buffer, s.len * sizeof(C));
	}
	template<typename W> friend bool write(W &w, const _pascal_string &s) {
		w.write(s.len);
		return check_writebuff(w, s.buffer, s.len * sizeof(C));
	}
};

template<typename U, typename C> void *get_after(const _pascal_string<U, C, 0> *t) {
	return (void*)&t->buffer[t->len];
}

template<typename U, typename C, int N> struct string_traits<_pascal_string<U,C,N> > {
	typedef _pascal_string<U,C,N>	S;
	typedef C				element, *start_type, *iterator;
	static C				*begin(S &s)			{ return s.buffer; }
	static C				*start(S &s)			{ return s.buffer; }
	static C				*end(S &s)				{ return s.buffer + s.len; }
	static const C			*terminator(const S &s)	{ return s.buffer + s.len; }
	static inline size_t	len(const S &s)			{ return s.len; }
};
template<typename U, typename C, int N> struct string_traits<const _pascal_string<U,C,N> > : string_traits<_pascal_string<U,C,N> > {
	typedef const _pascal_string<U,C,N>	S;
	typedef const C			element, *start_type, *iterator;
	static const C			*begin(S &s)			{ return s.buffer; }
	static const C			*start(S &s)			{ return s.buffer; }
	static const C			*end(S &s)				{ return s.buffer + s.len; }
};

typedef string_base<_pascal_string<uint8,char,255> >	pascal_string;

//-----------------------------------------------------------------------------
//	alloc_string - aka string & string16
//-----------------------------------------------------------------------------

template<typename C> class alloc_string : public string_base<C*> {
protected:
	typedef string_base<C*> B;
	using B::p;
	C	*_alloc(size_t n)		{ return p = n ? (C*)malloc((n + 1) * sizeof(C)) : 0; }
	C	*_realloc(size_t n)		{ return p = (C*)realloc(p, n ? (n + 1) * sizeof(C) : 0); }
	template<typename S> void init(const S *s) {
		auto	n = chars_count<C>(s);
		auto	n2 = string_copy(_alloc(n), s);
		ISO_ASSERT(n == n2);
	}
	template<typename I> void init(I s, I e) {
		auto	n = chars_count<C>(s, e);
		auto	n2 = string_copy(_alloc(n), s, e);
		ISO_ASSERT(n == n2);
	}

public:
	explicit alloc_string(size_t n)								{ _alloc(n); if (n) p[n] = 0; }
	constexpr alloc_string()				: B(0)				{}
	constexpr alloc_string(const _none&)	: B(0)				{}
	constexpr alloc_string(alloc_string &&s): B(s.detach())		{}
	alloc_string(string_paramT<C> &&s)		: B(s.detach())		{}
	alloc_string(builder<C> &&s)			: B(s.detach())		{}
	alloc_string(const alloc_string &s)							{ init(s.begin(), s.end()); }
	alloc_string(const C *s)									{ init(s); }
	alloc_string(const from_string_getter &s)					{ init(s.s); }
	template<typename S> alloc_string(const S *s)				{ init(s); }
	template<typename S> alloc_string(const S *s, const S *e)	{ init(s, e); }
	template<typename S> alloc_string(const S *s, size_t n)		{ init(s, s + n); }
	template<typename T> alloc_string(const string_getter<T> &s) {
		size_t n = s.len();
		s.get(_alloc(n), n);
	}
	template<typename T, typename=enable_if_t<has_begin_v<T> && has_end_v<T>>> alloc_string(const T &c) {
		using iso::begin; using iso::end;
		init(begin(c), end(c));
	}
//	template<typename R> static alloc_string get(R &r, size_t n)			{ alloc_string s(n); r.readbuff(s.p, n * sizeof(C)); return s; }
	template<typename R, typename=is_reader_t<R>>				alloc_string(R &&r, size_t n)	: alloc_string(n)	{  r.readbuff(p, n * sizeof(C)); }

	~alloc_string()												{ free(p); }

	alloc_string&	operator=(const alloc_string &s)			{ if (s.p != p) { free(p); init(s.begin(), s.end()); } return *this; }
	alloc_string&	operator=(const C *s)						{ auto p0 = p; init(s); free(p0); return *this; }
	alloc_string&	operator=(const from_string_getter &s)		{ auto p0 = p; init(s.s); free(p0); return *this; }
	alloc_string&	operator=(alloc_string &&s)					{ swap(p, s.p); return *this; }
	alloc_string&	operator=(string_paramT<C> &&s)				{ free(p); p = s.detach(); return *this; }
	alloc_string&	operator=(accum<C> &s);
	template<typename S> alloc_string& operator=(const S *s)	{ free(p); init(s); return *this; }
	template<typename T> alloc_string& operator=(const string_getter<T> &s) {
		free(p);
		size_t n = s.len();
		s.get(_alloc(n), n);
		return *this; 
	}
	template<typename T, typename=enable_if_t<has_begin_v<T> && has_end_v<T>>> 	alloc_string& operator=(const T &c)	{
		using iso::begin; using iso::end;
		auto p0 = p;
		init(begin(c), end(c));
		free(p0);
		return *this;
	}

	C*				extend(size_t n)					{ auto n0 = B::length(); if (_realloc(n0 + n)) p[n0 + n] = 0; return p + n0; }
	alloc_string&	append(const C *s, size_t n)		{ memcpy(extend(n), s, n * sizeof(C)); return *this; }
	alloc_string&	append(const C *s)					{ return append(s, string_len(s)); }
	alloc_string&	push_back(C c)						{ return append(&c, 1); }
	alloc_string&	operator+=(const C *s)				{ return append(s); }
	alloc_string&	operator+=(C c)						{ return append(&c, 1); }
	alloc_string&	alloc(size_t n)						{ free(p); _alloc(n); return *this; }
	alloc_string&	resize(size_t n)					{ if (_realloc(n)) p[n] = 0; return *this; }
	alloc_string&	clear()								{ free(p); p = 0; return *this; }

	C*				detach()							{ return exchange(p, nullptr); }

	operator accum<C>();

	template<typename T> alloc_string& append(const string_base<T> &s)		{ return append(s.begin(), s.length()); }
	template<typename T> alloc_string& operator+=(const string_base<T> &s)	{ return append(s.begin(), s.length()); }

	template<typename R> bool	read(R &r);
	template<typename W> bool	write(W &w) const {
		return p ? check_writebuff(w, p, (B::length() + 1) * sizeof(C)) : w.write(C(0));
	}

	template<typename R> static alloc_string get(R &r, size_t n)			{ alloc_string s(n); r.readbuff(s.p, n * sizeof(C)); return s; }
	template<typename R> bool read(R &r, size_t n, bool clear = true)		{ size_t o = clear ? 0 : B::length(); resize(o + n); return check_readbuff(r, p + o, n * sizeof(C)); }
//	template<typename R> bool read_line(R &r, bool clear = true);
//	template<typename R> bool read_to(R &r, const char_set &set, bool clear = true);

	friend uint32	hash(const alloc_string &s)					{ return string_hash(s.begin(), s.length()); }
	friend void		swap(alloc_string &a, alloc_string &b)		{ swap(a.p, b.p); }
};

typedef alloc_string<char>		string;
typedef alloc_string<char16>	string16;

template<typename B1, typename B2> inline auto operator+(const string_base<B1> &s1, const string_base<B2> &s2) {
	size_t n1 = s1.length(), n2 = s2.length();
	alloc_string<noconst_t<typename string_base<B1>::element>> s(n1 + n2);
	string_copy(s.begin(), s1.begin(), n1);
	string_copy(s.begin() + n1, s2.begin(), n2);
	return s;
}
template<typename B1, typename B2> inline auto operator+(const string_base<B1> &s1, const B2 *s2) {
	size_t n1 = s1.length(), n2 = string_len(s2);
	alloc_string<noconst_t<typename string_base<B1>::element>> s(n1 + n2);
	string_copy(s.begin(), s1.begin(), n1, 0);
	string_copy(s.begin() + n1, s2, n2, 0);
	return s;
}
template<typename B1, typename B2> inline auto operator+(const B1 *s1, const string_base<B2> &s2) {
	size_t n1 = string_len(s1), n2 = s2.length();
	alloc_string<B1> s(n1 + n2);
	string_copy(s.begin(), s1, n1);
	string_copy(s.begin() + n1, s2.begin(), n2);
	return s;
}

template<typename C> alloc_string<C> to_lower(const C *s) {
	alloc_string<C>	d(string_len(s));
	to_lower(d.begin(), s);
	return d;
}

template<typename C> alloc_string<C> to_upper(const C *s) {
	alloc_string<C>	d(string_len(s));
	to_upper(d.begin(), s);
	return d;
}

template<typename S> auto to_lower(const string_base<S> &s) {
	alloc_string<noconst_t<typename string_base<S>::element>>	d(string_len(s));
	auto	*p = d.begin();
	for (auto c : s)
		*p++ = to_lower(c);
	return d;
}

template<typename S> auto to_upper(const string_base<S> &s) {
	alloc_string<noconst_t<typename string_base<S>::element>>	d(string_len(s));
	auto	*p = d.begin();
	for (auto c : s)
		*p++ = to_upper(c);
	return d;
}

template<typename R> string read_string(R &r, size_t n) {
	string	s;
	s.read(r, n);
	return s;
}

//-----------------------------------------------------------------------------
//	fixed_string<int>	- fixed storage, null terminated
//-----------------------------------------------------------------------------

template<int N, typename C = char> class fixed_string : public string_base<C[N]> {
	typedef	string_base<C[N]>	B;
public:
	fixed_string()												{ B::p[0] = 0; }
	fixed_string(const C *s)									{ string_term(B::p, chars_copy(B::p, s, N - 1)); }
	fixed_string(string_paramT<C> fmt, va_list valist)			{ _format(B::p, N, fmt, valist); }
//	fixed_string(const meta::array<C, N> &s)					{ memcpy(B::p, &s, N); }
	template<typename T> explicit fixed_string(const T *s)		{ string_term(B::p, chars_copy(B::p, s, N - 1)); }
//	template<typename T> fixed_string(const string_base<T> &s)	{ string_copy(B::p, s.begin(), s.end()); }
	template<typename T> fixed_string(const string_getter<T> &s) { B::p[s.get(B::p, N)] = 0; }

	template<typename T, typename=enable_if_t<has_begin_v<T> && has_end_v<T>>> fixed_string(const T &c) {
		using iso::begin; using iso::end;
		string_term(B::p, chars_copy(B::p, begin(c), end(c), N - 1));
	}

	template<typename T> fixed_string&	operator=(const T *s)						{ string_term(B::p, chars_copy(B::p, s, N - 1)); return *this; }
//	template<typename T> fixed_string&	operator=(const string_base<T> &s)			{ string_copy(B::p, s.begin(), min(s.length(), N - 1)); return *this; }
//	template<typename T> fixed_string&	operator=(const meta::array<C, N - 1> &s)	{ memcpy(B::p, &s, N - 1); B::p[N - 1] = 0;	}

	template<typename T, typename=enable_if_t<has_begin_v<T> && has_end_v<T>>> fixed_string&	operator=(const T &c) {
		using iso::begin; using iso::end;
		string_term(B::p, chars_copy(B::p, begin(c), end(c), N - 1));
		return *this;
	}

	fixed_string&						operator+=(const C c)				{ append(c); return *this; }
	template<typename T> fixed_string&	operator+=(const T *s)				{ append(s); return *this; }
	template<typename T> fixed_string&	operator+=(const string_base<T> &s)	{ append(s.begin(), s.length()); return *this; }

	fixed_string&	clear()							{ B::p[0] = 0; return *this; }
	size_t			max_length()		const		{ return N; }
	size_t			remaining()			const		{ return N - this->length(); }
	bool			blank()				const		{ return !B::p[0]; }
	bool			operator!()			const		{ return !B::p[0]; }

	template<typename T> void append(const T *s, size_t n)	{ if (s) { C *e = B::end(); string_term(e, chars_copy(e, s, s + n, B::p + N - 1 - e)); } }
	template<typename T> void append(const T *s)			{ if (s) { C *e = B::end(); string_term(e, chars_copy(e, s, B::p + N - 1 - e)); } }
	void			append(const C c)						{ C *e = B::end(); e[0] = c; e[1] = 0; }

	accum<C>		format(const C *fmt, ...);
	template<typename... PP> accum<C>	formati(string_paramT<C> fmt, PP... pp);
	//template<class R> bool read_to(R &r, const char_set &set)	{ return iso::read_to(r, set, B::p, N); }
	//template<class R> bool read_line(R &r)						{ return iso::read_line(r, B::p, N); }

	friend inline fixed_string replace(const fixed_string &srce, const C *from, const C *to) {
		fixed_string	dest;
		replace(dest, srce, from, to);
		return dest;
	}
	friend inline fixed_string to_lower(const fixed_string &srce) {
		fixed_string	dest;
		to_lower(dest.begin(), srce.begin());
		return dest;
	}
	friend inline fixed_string to_upper(const fixed_string &srce) {
		fixed_string	dest;
		to_upper(dest.begin(), srce.begin());
		return dest;
	}
};

template<int N1, int N2, typename C> inline fixed_string<N1 + N2,C> operator+(const fixed_string<N1, C> &s1, const fixed_string<N2, C> &s2) {
	return fixed_string<N1 + N2,C>(s1) += s2;
}
template<int N1, int N2, typename C> inline fixed_string<N1 + N2,C> operator+(const fixed_string<N1, C> &s1, const C (&s2)[N2]) {
	return fixed_string<N1 + N2,C>(s1) += s2;
}
template<int N1, int N2, typename C> inline fixed_string<N1 + N2,C> operator+(const C (&s1)[N1], const fixed_string<N2, C> &s2) {
	return fixed_string<N1 + N2,C>(s1) += s2;
}

//-----------------------------------------------------------------------------
//	makers
//-----------------------------------------------------------------------------

template<typename T> constexpr const char (&as_chars(const T &u))[sizeof(T)] { return (const char(&)[sizeof(T)])u; }

constexpr string_base<const char>						str(char c)							{ return c; }
constexpr string_base<const char16>						str(char16 c)						{ return c; }
template<typename I> constexpr string_base<range<I>>	str(const range<I> &r)				{ return r; }
template<typename T> constexpr string_base<T*>			str(T *const &p)					{ return p; }
template<typename T> constexpr string_base<range<T*> >	str(T *p, size_t n)					{ return range<T*>(p, p + n); }
template<typename T> constexpr string_base<range<T*> >	str(T *p, T *e)						{ return range<T*>(p, e); }
template<typename T, size_t N>	constexpr	auto		str(const meta::array<T, N> &s)		{ return count_stringT<const T>(s.t, N); }
template<typename B>			constexpr	auto&		str(const string_base<B> &s)		{ return s; }
template<typename U, typename C, int N>		auto		str(const _pascal_string<U,C,N> &p) { return str(p.buffer, p.len); }

constexpr cstring										str8(const char *s)					{ return s; }
constexpr string_base<range<const char*>>				str8(const char *s, size_t N)		{ return make_range_n(s, N); }
template<int N> inline string_base<const char(&)[N]>	str8(const char (&s)[N])			{ return s; }
template<typename T> inline string						str8(const T *s)					{ return s; }
template<typename T> inline string						str8(const T *s, size_t N)			{ return {s, N}; }
template<typename T, int N> inline string				str8(T (&s)[N])						{ return {s, s + N}; }
template<typename B> constexpr auto						str8(const string_base<B> &b)		{ return str8(b.begin(), b.length()); }
template<typename I> constexpr auto						str8(const range<I> &b)				{ return str8(b.begin(), b.size()); }
constexpr string_base<range<const char*>>				str8(const const_memory_block &b)	{ return make_range<const char>(b); }
constexpr string_base<range<char*>>						str8(const memory_block &b)			{ return make_range<char>(b); }

constexpr cstring16										str16(const char16 *s)				{ return s; }
constexpr string_base<range<const char16*>>				str16(const char16 *s, size_t N)	{ return make_range_n(s, N); }
template<int N> inline string_base<const char16(&)[N]>	str16(const char16 (&s)[N])			{ return s; }
template<typename T> inline string16 					str16(const T *s)					{ return s; }
template<typename T> inline string16 					str16(const T *s, size_t N)			{ return {s, s + N}; }
template<typename T, int N> inline string16				str16(T (&s)[N])					{ return {s, s + N}; }
template<typename B> constexpr auto						str16(const string_base<B> &b)		{ return str16(b.begin(), b.length()); }
template<typename I> constexpr auto						str16(const range<I> &b)			{ return str16(b.begin(), b.size()); }
constexpr string_base<range<const char16*>>				str16(const const_memory_block &b)	{ return make_range<const char16>(b); }
constexpr string_base<range<char16*>>					str16(const memory_block &b)		{ return make_range<char16>(b); }


template<typename T> constexpr auto	istr(T *p)								{ return str(make_case_insensitive(p)); }
template<typename T> constexpr auto	istr(T *p, size_t n)					{ return str(make_range_n(make_case_insensitive(p), n)); }
template<typename T> constexpr auto	istr(T *p, T *e)						{ return str(make_range(make_case_insensitive(p), make_case_insensitive(e))); }
template<typename B> constexpr auto	istr(string_base<B> &s)					{ return str(make_case_insensitive(s.representation())); }
template<typename B> constexpr auto	istr(const string_base<B> &s) 			{ return str(make_case_insensitive(s.representation())); }
template<typename C> constexpr auto	istr(const string_base<range<C>> &s) 	{ return str(make_case_insensitive(s.representation())); }

template<typename T, size_t N>	constexpr const string_base<const T(&)[N]>						str(const T (&p)[N])	{ return p; }
template<typename T, size_t N>	constexpr const string_base<const T(&)[N - 1]>					cstr(const T (&p)[N])	{ return (const T(&)[N - 1])p; }
template<typename T, size_t N>	constexpr const string_base<const case_insensitive<T>(&)[N]>	icstr(const T (&p)[N])	{ return (case_insensitive<T>(&)[N])p; }
template<typename T, size_t N>	constexpr auto&													fstr(T (&p)[N])			{ return (fixed_string<N, T>&)p; }

template<int N> constexpr fixed_string<N, char>			str8(const fixed_string<N, char16> &s)	{ return s; }
template<int N> constexpr const char*					str8(const fixed_string<N, char> &s)	{ return s; }
template<int N> constexpr fixed_string<N, char16>		str16(const fixed_string<N, char> &s)	{ return s; }
template<int N> constexpr const char16*					str16(const fixed_string<N, char16> &s)	{ return s; }

template<int N, typename T> fixed_string<N, char>		str(const string_getter<T> &s)			{ return s; }
//template<typename T>		fixed_string<256, char>		str(const string_getter<T> &s)			{ return s; }
template<int N, typename T> fixed_string<N, char16>		str16(const string_getter<T> &s)		{ return s; }
//template<typename T>		fixed_string<256, char16>	str16(const string_getter<T> &s)		{ return s; }

constexpr auto operator"" _cstr(const char *s, size_t len) { return str(s, len); }
constexpr auto operator"" _istr(const char *s, size_t len) { return istr(s, len); }

//-----------------------------------------------------------------------------
//	write numbers
//-----------------------------------------------------------------------------

template<int B, typename T, typename C> void put_num_base(C *s, int len, T i, char ten = 'A') {
	for (auto t = uint_for_t<T>(i); len--;  t /= B)
		s[len] = to_digit(uint32(t % B), ten);
}
template<int B, typename T, typename C> no_inline size_t put_unsigned_num_base(C *s, T i, char ten = 'A') {
	int n = log_base<B>(i) + 1;
	put_num_base<B>(s, n, i, ten);
	return n;
}
template<int B, typename T, typename C> size_t put_signed_num_base(C *s, T i, char ten = 'A') {
	if (get_sign(i)) {
		*s = '-';
		return put_unsigned_num_base<B>(s + 1, -i, ten) + 1;
	}
	return put_unsigned_num_base<B>(s, i, ten);
}

template<typename T, typename C> inline void put_num_base(int B, C *s, int len, T i, char ten = 'A') {
	while (len--) {
		s[len] = to_digit(uint32(i % B), ten);
		i /= B;
	}
}
template<typename T, typename C> size_t put_unsigned_num_base(int B, C *s, T i, char ten = 'A') {
	int n = log_base(B, i) + 1;
	put_num_base(B, s, n, i, ten);
	return n;
}
template<typename T, typename C> size_t put_signed_num_base(int B, C *s, T i, char ten = 'A') {
	if (i < 0) {
		*s = '-';
		return put_unsigned_num_base(B, s + 1, uint_for_t<T>(-i), ten) + 1;
	}
	return put_unsigned_num_base(B, s, uint_for_t<T>(i), ten);
}

template<int B, int M, typename T> uint32 get_floatlen(T m, T delta) {
	uint32	r = 0;
	while (m > delta) {
		m		= (m & bits<T>(M)) * B;
		delta	*= B;
		++r;
	}
	return r;
}

template<int B, int M, typename C, typename T> C *put_float_digits(C *s, T m, T delta, char ten = 'a') {
	while (m > delta) {
		*s++	= to_digit(int(m >> M), ten);
		m		= (m & bits<T>(M)) * B;
		delta	*= B;
	}
	return s;
}

template<int B, int M, typename C, typename T> C *put_float_digits_n(C *s, T &m, int n, char ten = 'a') {
	while (n--) {
		*s++	= to_digit(int(m >> M), ten);
		m		= (m & bits<T>(M)) * B;
	}
	return s;
}

template<typename T> size_t put_float(char *s, const float_info<T> &f, uint32 digits, uint32 exp_digits);

//-----------------------------------------------------------------------------
//	read integer
//-----------------------------------------------------------------------------

template<typename C> inline bool is_prefixed_int(const C *p) {
	return p[0] == '0' ? (
			(p[1] == 'x' && string_check(p + 2, char_set("0123456789abcdefABCDEF")))
		||	(p[1] == 'b' && string_check(p + 2, char_set("01")))
		||	string_check(p, char_set("01234567"))
	) : string_check(p, char_set::digit);
}

template<typename C> inline bool is_signed_int(const C *p) {
	return is_prefixed_int(p + int(p[0] == '+' || p[0] == '-'));
}

template<int B, typename T, typename C> inline const C *get_num_base(const C *p, T &i) {
	T		r = 0;
	uint32	d;
	while (is_alphanum(*p) && (d = from_digit(*p)) < B && r <= num_traits<T>::max() / B) {
		r = r * B + d;
		++p;
	}
	i = r;
	return p;
}
template<typename T, typename C> inline const C *get_prefixed_num(const C *p, T &i) {
	return *p == '0' ? (p[1] == 'x' ? get_num_base<16>(p + 2, i) : p[1] == 'b' ? get_num_base<2>(p + 2, i) : get_num_base<8>(p + 1, i)) : get_num_base<10>(p, i);
}
template<typename T, typename C> const C* get_unsigned_num(const C *s, T &i) {
	if (const C *p = s)
		return get_prefixed_num(skip_whitespace(p), i);
	return s;
}
template<typename T, typename C> const C* get_signed_num(const C *s, T &i) {
	if (const C *p = s) {
		p = skip_whitespace(p);
		if (*p == '-') {
			p = get_prefixed_num(p + 1, i);
			i = -i;
			return p;
		}
		return get_prefixed_num(p + int(*p == '+'), i);
	}
	return s;
}

template<int B, typename T, typename C> inline const C *get_num_base(const C *p, const C *e, T &i) {
	T		r;
	uint32	d;
	for (r = 0; p < e && is_alphanum(*p) && (d = from_digit(*p)) < B && r <= num_traits<T>::max() / B; ++p)
		r = r * B + d;
	i = r;
	return p;
}
template<typename T, typename C> inline const C *get_prefixed_num(const C *p, const C *e, T &i) {
	return *p == '0' ? (p[1] == 'x' ? get_num_base<16>(p + 2, e, i) : p[1] == 'b' ? get_num_base<2>(p + 2, e, i) : get_num_base<8>(p + 1, e, i)) : get_num_base<10>(p, e, i);
}
template<typename T, typename C> const C* get_unsigned_num(const C *s, const C *e, T &i) {
	if (const C *p = s)
		return get_prefixed_num(skip_whitespace(p), e, i);
	return s;
}
template<typename T, typename C> const C* get_signed_num(const C *s, const C *e, T &i) {
	if (const C *p = s) {
		p = skip_whitespace(p, e);
		if (*p == '-') {
			p = get_prefixed_num(p + 1, e, i);
			i = -i;
			return p;
		}
		return get_prefixed_num(p + int(*p == '+'), e, i);
	}
	return s;
}

//-----------------------------------------------------------------------------
//	formatting
//-----------------------------------------------------------------------------

struct FORMAT {
	enum FLAGS {
		NONE		= 0,
		LEFTALIGN	= 1 << 0,	//Left align the result within the given field width.
		PLUS		= 1 << 1,	//Prefix the output value with a sign (+ or -) if the output value is of a signed type.
		ZEROES		= 1 << 2,	//zeros are added until the minimum width is reached.
		BLANK		= 1 << 3,	//Prefix the output value with a blank if the output value is signed and positive.
		CFORMAT		= 1 << 4,	//Prefix any nonzero output value with 0, 0x, or 0X; force decimal point in float formats
		PRECISION	= 1 << 5,	//Precision specified
		SHORT		= 1 << 6,	//Short prefix
		BITS32		= 1 << 7,
		BITS64		= 1 << 8,
		UPPER		= 1 << 9,	//Upper case hex

		LONG		= sizeof(long)== 8 ? BITS64 : BITS32,
		LONGLONG	= BITS64,

		SKIP		= LEFTALIGN,
		SCIENTIFIC	= 1 << 10,	// %e
		SHORTEST	= 1 << 11,	// %g

		BASE		= 1 << 12, BASE_MASK = BASE * 3,
		DEC			= BASE * 0,
		HEX			= BASE * 1,
		OCT			= BASE * 2,
		BIN			= BASE * 3,
	};

	friend constexpr FLAGS	operator| (FLAGS a, FLAGS b)	{ return FLAGS(int(a) | int(b)); }
	friend constexpr FLAGS	operator- (FLAGS a, FLAGS b)	{ return FLAGS(int(a) & ~int(b)); }
	friend constexpr FLAGS	operator* (FLAGS a, bool b)		{ return b ? a : NONE; }
	friend FLAGS&			operator|=(FLAGS& a, FLAGS b)	{ return a = a | b; }

	friend int	base(FLAGS a)									{ return "\x0a\x10\x08\x02"[(a & BASE_MASK) / BASE]; }
	friend bool	use_sci(FLAGS flags, int precision, int exp)	{ return (flags & SCIENTIFIC) || ((flags & SHORTEST) && (exp < -4 || exp >= precision)); }

	FLAGS	flags;
	int		width, precision;

};

template<int N, typename C> inline	auto vformat_string(const C *fmt, va_list valist)	{ return fixed_string<N, C>(fmt, valist);	}
template<int N, typename C>			auto format_string(const C *fmt, ...)				{ va_list valist; va_start(valist, fmt); return fixed_string<N, C>(fmt, valist); }
template<typename C>		inline	auto vformat_string(const C *fmt, va_list valist)	{ return fixed_string<256, C>(fmt, valist); }
template<typename C>		inline	auto format_string(const C *fmt, ...)				{ va_list valist; va_start(valist, fmt); return vformat_string(fmt, valist); }

//to_string can take any of these forms:
//size_t			to_string(char *s, T t)				--
//fixed_string<N>	to_string(T t)						-- we know the (maximum) output size
//string_base<B>	to_string(T t)						--
//size_t			to_string(fixed_string<N> &d, T t)	-- we know the (maximum) output size
//const char*		to_string(T t)						-- returns (portion of) input as string

template<typename C, int B, typename T> inline enable_if_t<num_traits<T>::is_signed, size_t>	to_string(C *s, baseint<B,T> v)	{ return put_signed_num_base<B>(s, v.i, 'A'); }
template<typename C, int B, typename T> inline enable_if_t<!num_traits<T>::is_signed, size_t>	to_string(C *s, baseint<B,T> v)	{ return put_unsigned_num_base<B>(s, v.i, 'A'); }

template<typename C> inline size_t	to_string(C *s, int i)			{ return put_signed_num_base<10>(s, i); }
template<typename C> inline size_t	to_string(C *s, uint32 i)		{ return put_unsigned_num_base<10>(s, i); }
#if USE_LONG
template<typename C> inline size_t	to_string(C *s, long i)			{ return put_signed_num_base<10>(s, i); }
template<typename C> inline size_t	to_string(C *s, ulong i)		{ return put_unsigned_num_base<10>(s, i); }
#endif
#if USE_64BITREGS
template<typename C> inline size_t	to_string(C *s, int64 i)		{ return put_signed_num_base<10>(s, i); }
template<typename C> inline size_t	to_string(C *s, uint64 i)		{ return put_unsigned_num_base<10>(s, i); }
#endif
template<typename C> inline size_t	to_string(C *s, int128 i)		{ return put_signed_num_base<10>(s, i); }
template<typename C> inline size_t	to_string(C *s, uint128 i)		{ return put_unsigned_num_base<10>(s, i); }

inline size_t				to_string(char *s, char c)				{ *s = c; return 1; }
inline size_t				to_string(char *s, char16 c)			{ return put_char(c, s); }
inline size_t				to_string(char *s, const char16 *v)		{ return chars_copy(s, v); }
inline size_t				to_string(char *s, char32 c)			{ return put_char(c, s); }
inline size_t				to_string(char *s, const char32 *v)		{ return chars_copy(s, v); }

inline size_t				to_string(char *s, float f)				{ return put_float(s, get_print_info<10>(f), 6, 2); }
inline size_t				to_string(char *s, double f)			{ return put_float(s, get_print_info<10>(f), 15, 3); }

template<typename C> inline size_t to_string(C *s, const char *v)	{ return string_copy(s, v); }
template<typename C, typename B> inline size_t to_string(C *s, const string_base<B> &v)	{ return string_copy(s, v.begin(), v.end()); }


template<typename B> inline enable_if_t<!same_v<typename string_traits<B>::start_type,_none>, typename string_traits<const B>::start_type>
	to_string(const string_base<B> &v)	{ return v; }

template<int N, typename T> inline fixed_string<N>	_to_string(const T &t)	{ fixed_string<N> s; s[to_string(s.begin(), t)] = 0; return s; }
inline const char*			to_string(const char *v)				{ return v ? v : ""; }
inline auto					to_string(char v)						{ return _to_string<2>(v); }
inline auto					to_string(char16 v)						{ return _to_string<8>(v); }
inline auto					to_string(char32 v)						{ return _to_string<8>(v); }
inline auto					to_string(int v)						{ return _to_string<16>(v); }
inline auto					to_string(uint32 v)						{ return _to_string<16>(v); }
inline auto					to_string(float v)						{ return _to_string<16>(v); }
inline auto					to_string(double v)						{ return _to_string<32>(v); }
//inline fixed_string<32>	to_string(const void *v)				{ return _to_string<32>(v); }
#if USE_LONG
inline auto					to_string(long v)						{ return _to_string<16>(v); }
inline auto					to_string(ulong v)						{ return _to_string<16>(v); }
#endif
#if USE_64BITREGS
inline auto					to_string(int64 v)						{ return _to_string<32>(v); }
inline auto					to_string(uint64 v)						{ return _to_string<32>(v); }
#endif

inline auto					to_string(const range<const char*> &v)	{ return str(v); }


// ensure only actual bools match these
template<typename Bool, typename T = enable_if_t<same_v<Bool, bool>>> const char*	to_string(Bool v)			{ return v ? "true" : "false"; }
template<typename Bool, typename T = enable_if_t<same_v<Bool, bool>>> size_t		to_string(char *s, Bool v)	{ string_copy(s, to_string(v)); return string_len(s); }

template<int B, typename T> inline fixed_string<baseint<B,T>::digits + 1>	to_string(baseint<B,T> v)	{ return _to_string<baseint<B,T>::digits + 1>(v); }

template<typename C, class T> struct has_to_string {
	template<typename U>	static decltype(to_string(*(U*)0))			test0(int);
	template<typename>		static void									test0(...);
	template<typename U>	static decltype(to_string((C*)0, *(U*)0))	test1(int);
	template<typename>		static void									test1(...);

	static const bool is_string		= iso::is_string<T>::value;					// is it (or does it inherit from) a string_base<B>?
	static const bool has_immediate = same_v<decltype(test0<T>(0)), const C*>;	// is there a const C *to_string(T)?
	static const bool has_castable	= !same_v<decltype(test0<T>(0)), void>;		// is there a X to_string(T)?
	static const bool has_copy		= !same_v<decltype(test1<T>(0)), void>;		// is there a to_string(C*, T)?

	static int const get	= is_char<T>	? 0
							: is_string		? 3
							: has_immediate	? 1
							: has_copy		? 2
							: has_castable	? 1
							: 0;
	static int const copy	= is_char<T>	? 0
							: is_string		? 3
							: has_copy		? 2
							: has_immediate	|| has_castable	? 1
							: 0;
};
template<typename C, class T, int N> struct has_to_string<C, T[N]> {
	static const int get = 0, copy = 0;
};

template<typename T> inline decltype(to_string(*(T*)0))	to_string(const constructable<T> &t)	{
	return to_string(get(t));
}

//template<typename C, typename T> inline enable_if_t<has_to_string0<T>::value, size_t>	to_string(C *s, const T &t)	{
//	return string_copy(s, to_string(get(t)));
//}

//-----------------------------------------------------------------------------
//	string_accum
//-----------------------------------------------------------------------------

template<typename C> class accum : public virtfunc<C*(int&)> {
	struct _item {
		const void	*p;
		void		(*f)(accum &a, const void *p);
		template<typename T> static void thunk(accum &a, const void *p) { a << *(const T*)p; }
		template<typename T> _item(const T *t) : p(t), f(&thunk<T>) {}
		void		operator()(accum &a) const { f(a, p); }
	};
protected:
	C*			startp;
	accum() : startp(0) {}

public:
	C*			getp(int &n)		{ return (*this)(n); }
	C*			getp()				{ int n = 0; return (*this)(n); }
	accum&		move(int n)			{ if (int n0 = n) { getp(n); ISO_ASSERT(n == n0); } return *this; }
	size_t		length()			{ return getp() - startp; }
	size_t		remaining()			{ int n = 0; getp(n); return n;	}

	C*			begin()	const		{ return startp; }
	const C*	term()				{ *getp() = 0; return startp; }
	operator const C*()				{ return term(); }
	C			back()				{ auto p = getp(); return p == startp ? 0 : p[-1]; }

	accum&		putc(C c)			{ int n = 1; *getp(n) = c; return *this;	}
	accum&		putc(C c, int n)	{ if (n > 0) { C *d = getp(n); memset(d, c, n * sizeof(C)); } return *this;	}

	accum&		vformat(const C *fmt, va_list valist);
	accum&		format(const C *fmt, ...) {
		va_list valist;
		va_start(valist, fmt);
		return vformat(fmt, valist);
	}

	accum&		vformat(const C *fmt, const _item *items);
	template<typename... PP> accum& formati(string_paramT<C> fmt, PP... pp) {
		_item	items[] = {&pp...};
		return vformat(fmt, items);
	}
	accum&		merge(const C *s, const C *e) {
		return merge(s, e - s);
	}
	template<typename S> accum&	merge(const S *s, size_t n)	{
		while (int n2 = int(chars_count<C>(s, n))) {
			C *d	= getp(n2);
			if (!n2)
				break;
			chars_copy(d, s, s + n2);
			int	n1	= int(chars_count<S>(d, n2));
			n	-= n1;
			s	+= n1;
		}
		return *this;
	}

	const_memory_block	data()				{ auto endp = getp(); return {startp, endp}; }
	count_stringT<const C>	to_string()		{ auto endp = getp(); return {startp, endp}; }
	operator	count_stringT<const C>()	{ return to_string(); }
	operator	string_paramT<C>()		&	{ return term(); }
	operator	string_paramT<C>()		&&	{ return to_string(); }
};

typedef accum<char>		string_accum;
typedef accum<char16>	string_accum16;

template<typename C> alloc_string<C>&	alloc_string<C>::operator=(accum<C> &s)	{ return operator=(s.to_string()); }
//template<typename C> string_paramT<C>::string_paramT(accum<C> &s)				: B(s.term()) {}
//template<typename C> string_paramT<C>::string_paramT(accum<C> &&s)				{ init(s.to_string()); }

extern template accum<char>&	accum<char>::vformat(const char *format, va_list argptr);
extern template accum<char16>&	accum<char16>::vformat(const char16 *format, va_list argptr);

template<typename C> accum<C>& accum<C>::vformat(const C *fmt, const _item *items) {
	if (fmt) while (C c = *fmt++) {
		if (c == '%') {
			if (*fmt != '%') {
				int		x;
				fmt = get_num_base<10>(fmt, x);
				items[x](*this);
				continue;
			}
			fmt++;
		}
		putc(c);
	}
	return *this;
}

inline cstring		str(accum<char> &s)		{ return s.term(); }
inline cstring		str8(accum<char> &s)	{ return s.term(); }
inline string16		str16(accum<char> &s)	{ return s.term(); }
inline cstring16	str(accum<char16> &s)	{ return s.term(); }
inline cstring16	str16(accum<char16> &s)	{ return s.term(); }
inline string		str8(accum<char16> &s)	{ return s.term(); }

template<typename C, class T>			inline accum<C>& operator<<(accum<C> &&a, const T &t) { return a << t; }

template<typename C, class T>			inline enable_if_t<has_to_string<C,T>::copy == 1, accum<C>&> operator<<(accum<C> &a, const T &t) {
	return a << to_string(get(t));
}
template<typename C, class T>			inline enable_if_t<has_to_string<C,T>::copy == 2, accum<C>&> operator<<(accum<C> &a, const T &t) {
	int n = 0; return a.move(int(to_string(a.getp(n), t)));
}

template<typename C, typename T> inline accum<C> & operator<<(accum<C> &a, const string_getter<T> &g) {
	int		n = int(g.len());
	auto	p = a.getp(n);
	return a.move(int(g.get(p, n)) - n);
}

template<typename C, typename T> inline accum<C> & operator<<(accum<C> &a, const optional<T> &t) {
	return t.exists() ? a << get(t) : a;
}

template<typename C, class T, int N>		enable_if_t<!is_char<T>, accum<C>&> operator<<(accum<C> &a, const T (&t)[N])				{ for (int i = 0; i < N; i++) a << t[i]; return a; }
template<typename C>						inline accum<C>&	operator<<(accum<C> &a, int f(C*))										{ int n = 0; return a.move(f(a.getp(n))); }
template<typename C1, typename C2>			inline accum<C1>&	operator<<(accum<C1> &a, const accum<C2> &b)							{ return a.merge(b.begin(), b.length()); }

template<typename C1, typename C2>			inline enable_if_t<is_char<C2>, accum<C1>&>	operator<<(accum<C1> &a, const C2 *const &s)	{ return a.merge(s, string_len(s)); }
template<typename C1, typename C2>			inline enable_if_t<is_char<C2>, accum<C1>&>	operator<<(accum<C1> &a, C2 c)					{ int n = char_count<C1>(c); put_char(c, a.getp(n)); return a; }
template<typename C1, typename C2, size_t N>inline enable_if_t<is_char<C2>, accum<C1>&>	operator<<(accum<C1> &a, const C2 (&s)[N])		{ return a.merge(s, N - 1); }
template<typename C1, typename B>			inline accum<C1>&	operator<<(accum<C1> &a, const string_base<B> &s)						{ return a.merge(s.begin(), s.length()); }
//template<typename C, size_t N>			inline accum<C>&	operator<<(accum<C> &a, const meta::array<char, N> &s)					{ return a.merge(&s[0], N); }
//template<typename C, size_t N>			inline accum<C>&	operator<<(accum<C> &a, const meta::array<char16, N> &s)				{ return a.merge(&s[0], N); }
template<typename C, typename T, size_t N>	inline accum<C>&	operator<<(accum<C> &a, const meta::array<T, N> &s)						{ return a << s.t; }

template<typename C, typename F> exists_t<decltype(declval<F>()(declval<accum<C>&>())), accum<C>&> operator<<(accum<C> &sa, F &&f) { f(sa); return sa; }

template<typename T, typename C> struct accumT : accum<C> {
	accumT()	{ accum<C>::template bind<T, &T::getp>(); }
};

struct CODE_GUID : GUID {
	CODE_GUID(const GUID &g) : GUID(g) {}
};

template<typename C> accum<C>&	operator<<(accum<C> &a, const GUID &g);
template<typename C> accum<C>&	operator<<(accum<C> &a, const CODE_GUID &g);

template<typename C> inline accum<C>&	operator<<(accum<C> &a, const repeat_s<C> &r) { return a.putc(r.t, r.n); }

template<typename C, typename T> accum<C>& operator<<(accum<C> &a, const repeat_s<T> &r) {
	for (auto &i : r)
		a << i;
	return a;
}

//-----------------------------------------------------------------------------
//	formatting
//-----------------------------------------------------------------------------

template<int B, typename C, typename T> void put_float(accum<C> &acc, const float_info<T> &f, uint32 digits, uint32 exp_digits, FORMAT::FLAGS flags, int width);
template<typename C, typename T> inline enable_if_t<is_float<T>> put_format(accum<C> &acc, T f, FORMAT::FLAGS flags, int width = 0, uint32 precision = 0) {
	put_float<10>(acc, get_print_info<10>(f), flags & FORMAT::PRECISION ? precision : num_traits<T>::mantissa_bits * 3 / 10, num_traits<T>::exponent_bits * 3 / 10, flags, width);
}

template<typename T, typename C> void put_int(accum<C> &acc, T x, FORMAT::FLAGS flags, int width);
template<typename C, typename T> inline enable_if_t<is_int<T>> put_format(accum<C> &acc, T f, FORMAT::FLAGS flags, int width = 0) {
	put_int(acc, f, flags, width);
}

template<typename C> void put_str(accum<C> &acc, const C *s, FORMAT::FLAGS flags, int width, uint32 precision);
template<typename C> inline void put_format(accum<C> &acc, const C *s, FORMAT::FLAGS flags, int width = 0, uint32 precision = 0) {
	put_str(acc, s, flags, width, precision);
}

template<typename T> struct formatted_s {
	T&				t;
	FORMAT::FLAGS	flags;
	int				width;
	formatted_s(T &t, FORMAT::FLAGS flags, int width = 0) : t(t), flags(flags), width(width) {}
	friend string_accum& operator<<(string_accum &sa, const formatted_s &f) { put_format(sa, f.t, f.flags, f.width); return sa; }
};

template<typename T> struct formatted2_s : formatted_s<T> {
	uint32			precision;
	formatted2_s(T &t, FORMAT::FLAGS flags, int width, int precision) : formatted_s<T>(t, flags, width), precision(precision) {}
	friend string_accum& operator<<(string_accum &sa, const formatted2_s &f) { put_format(sa, f.t, f.flags, f.width, f.precision); return sa; }
};

template<typename T> inline auto formatted(T &&t, FORMAT::FLAGS flags, int width = 0)				{ return formatted_s<T>(t, flags, width); }
template<typename T> inline auto formatted(T &&t, FORMAT::FLAGS flags, int width, uint32 precision)	{ return formatted2_s<T>(t, flags, width, precision); }

template<typename... PP> inline auto format(const char *fmt, PP... pp) {
	return [fmt, pp...](string_accum &a) { return a.format(fmt, pp...); };
}

template<typename... PP> inline auto formati(const char *fmt, PP... pp) {
	return [fmt, pp...](string_accum &a) { return a.formati(fmt, pp...); };
}

//-----------------------------------------------------------------------------
//	onlyif, ifelse
//-----------------------------------------------------------------------------

//template<typename T> inline auto onlyif(bool b, const T &t)	{
//	return [b, &t](string_accum &sa) { if (b) sa << t; };
//}
template<typename T1, typename T2> inline auto ifelse(bool b, const T1 &t1, const T2 &t2) {
	return [b, &t1, &t2](string_accum &sa) { if (b) sa << t1; else sa << t2; };
}

//-----------------------------------------------------------------------------
//	leftjustify, rightjustify, tabs
//-----------------------------------------------------------------------------

class _justify {
protected:
	mutable string_accum	*a;
	mutable size_t			start;
	uint16					w;
	char					c;
	void	init(string_accum &_a) const	{ a = &_a; start = a->length(); }
public:
	explicit _justify(uint16 w, char c = ' ') : a(0), w(w), c(c)	{}
	_justify(string_accum &a, uint16 w, char c = ' ') : a(&a), start(a.length()), w(w), c(c) {}
	operator string_accum&()										const		{ return *a; }
};

class leftjustify : public _justify {
	void	done() const {
		intptr_t len = a->length() - start;
		if (len < w)
			a->putc(c, int(w - len));
	}
public:
	explicit leftjustify(uint16 w, char c = ' ') : _justify(w, c)				{}
	leftjustify(string_accum &a, uint16 w, char c = ' ') : _justify(a, w, c)	{}
	~leftjustify()																{ if (a) done(); }
	template<typename T> const leftjustify& operator<<(const T &t) const		{ *a << t; return *this; }
	string_accum& operator<<(const _none&)							const		{ done(); return *exchange(a, nullptr); }
	friend const leftjustify &operator<<(string_accum &a, const leftjustify &t)	{ t.init(a); return t; }
};

class rightjustify : public _justify {
	void	done() const {
		intptr_t len = a->length() - start;
		if (len < w) {
			int		n = int(w - len);
			auto	p = a->getp(n) - len;
			memmove(p + n, p, len);
			memset(p, c, n);
		}
	}
public:
	explicit rightjustify(uint16 w, char c = ' ') : _justify(w, c)				{}
	rightjustify(string_accum &a, uint16 w, char c = ' ') : _justify(a, w, c)	{}
	~rightjustify()																{ if (a) done(); }
	template<typename T> const rightjustify& operator<<(const T &t) const		{ *a << t; return *this; }
	string_accum& operator<<(const _none&)							const		{ done(); return *exchange(a, nullptr); }
	friend const rightjustify &operator<<(string_accum &a, const rightjustify &t) { t.init(a); return t; }
};

//-----------------------------------------------------------------------------
//	bracketed, indenter
//-----------------------------------------------------------------------------

template<char OPEN, char CLOSE> class bracketed {
	string_accum	*a;
	bool			need;
	void	init(string_accum &_a) { a = &_a; if (need) _a << OPEN; }
public:
	bracketed(string_accum &a, bool need = true)	: need(need)		{ init(a); }
	bracketed(bool need = true)						: a(0), need(need)	{}
	~bracketed()									{ if (need) *a << CLOSE; }
	friend string_accum &operator<<(string_accum &a, bracketed &&p)	{ p.init(a); return a; }
};

using parentheses		= bracketed<'(', ')'>;
using square_brackets	= bracketed<'[', ']'>;
using curly_braces		= bracketed<'{', '}'>;
using angle_brackets	= bracketed<'<', '>'>;

struct indenter {
	enum {
		SPACES			= 1 << 0, SPACE_MASK = SPACES * 0xff,
		NEWLINE			= 1 << 8,
		INDENT_BRACES	= 1 << 9,
		HADLINE			= 1 << 31,
	};
	int				depth;
	uint32			flags;

	indenter(int depth = 0, uint32 flags = 0) : depth(depth), flags(flags)	{}

	string_accum&	newline(string_accum &sa) {
		flags |= HADLINE;
		return sa << '\n' << repeat(flags & SPACE_MASK ? ' ' : '\t', max(flags & SPACE_MASK, 1) * depth);
	}
	string_accum&	open(string_accum &sa, const char *with = "{") {
		if (flags & INDENT_BRACES)
			++depth;
		if (flags & NEWLINE)
			newline(sa);
		if (!(flags & INDENT_BRACES))
			++depth;
		flags &= ~HADLINE;
		return sa << with;
	}
	string_accum&	close(string_accum &sa, const char *with = "}") {
		ISO_ASSERT(depth);
		if (!(flags & INDENT_BRACES))
			--depth;
		if (flags & (NEWLINE | HADLINE))
			newline(sa);
		if (flags & INDENT_BRACES)
			--depth;
		return sa << with;
	}
};

//-----------------------------------------------------------------------------
//	separated_list, comma_list, separated_number
//-----------------------------------------------------------------------------

template<typename C> auto separated_list(const C &c, const char *sep = ", ") {
	return [&c, sep](string_accum &sa) {
		int	j = 0;
		for (auto &&i : c)
			sa << onlyif(j++, sep) << i;
	};
}

//flags & 1 - start with sep (if any)
//flags & 2 - end with sep (if any)
template<typename C> auto separated_list(const C &c, int flags, const char *sep = ", ") {
	return [&c, sep, flags](string_accum &sa) {
		int	j = flags & 1;
		for (auto &&i : c)
			sa << onlyif(j++, sep) << i;
		if ((flags & 2) && j > (flags & 1))
			sa << sep;
	};
}

template<typename C, typename L, typename R> struct comma_list_s {
	const	C	&c;
	L			lambda;
	int			from;
	void	operator()(string_accum &a) const {
		int	j = from;
		for (auto &i : c)
			lambda(a << onlyif(j++, ", "), i);
	}
	comma_list_s(const C &c, L&& lambda, int from = 0) : c(c), lambda(lambda), from(from) {}
};
template<typename C, typename L> struct comma_list_s<C, L, bool> {
	const	C	&c;
	L			lambda;
	int			from;
	void	operator()(string_accum &sa) const {
		int	j = from;
		for (auto &i : c) {
			if (lambda(sa << onlyif(j, ", "), i))
				++j;
		}
	}
	comma_list_s(const C &c, L&& lambda, int from = 0) : c(c), lambda(lambda), from(from) {}
};

template<typename C, typename L> auto comma_list(const C &c, L lambda, int from = 0) {
	return comma_list_s<C, L, decltype(lambda(declval<string_accum&>(), c.front()))>(c, move(lambda), from);
}

template<typename T> auto	separated_number(T t, int d = 3, char c = ',') {
	return [t, d, c](string_accum &sa) {
		unsigned_t<T>	i;
		if (get_sign(t)) {
			sa << '-';
			i = -t;
		} else {
			i = t;
		}
		int		n	= log_base<10>(i);
		int		n1	= n + n / d + 1;
		auto	s	= sa.getp(n1) + n1;
		for (int j = 0; j++ < n;) {
			*--s = to_digit(uint32(i % 10));
			i /= 10;
			if (j % d == 0)
				*--s = c;
		}
		*--s = to_digit(uint32(i));
	};
}

//-----------------------------------------------------------------------------
//	scanning
//-----------------------------------------------------------------------------
int				vscan_string(const char *buffer, const char *format, va_list valist);
int				scan_string(const char *buffer, const char *format, ...);

//from_string can take any of these forms:
//size_t			from_string(const char *s, T &t)				--
//size_t			from_string(const char *s, const char *e, T &t)	--
//string_base<B>	from_string(const count_string &s, T &t)		--

inline size_t		from_string(const char *s, string &x)								{ x = s; return string_len(s); }
inline size_t		from_string(const char *s, const char *&i)							{ i = s; return string_len(s); }
template<typename C> inline size_t	from_string(const C *s, int8 &i)					{ return get_signed_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, uint8 &i)					{ return get_unsigned_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, int16 &i)					{ return get_signed_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, uint16 &i)					{ return get_unsigned_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, int &i)						{ return get_signed_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, uint32 &i)					{ return get_unsigned_num(s, i) - s; }

template<typename C> inline size_t	from_string(const C *s, const C *e, string &x)		{ x = count_string(s, e); return e - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, const char *&i)	{ i = s; return e - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, int8 &i)		{ return get_signed_num(s, e, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, uint8 &i)		{ return get_unsigned_num(s, e, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, int16 &i)		{ return get_signed_num(s, e, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, uint16 &i)		{ return get_unsigned_num(s, e, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, int &i)			{ return get_signed_num(s, e, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, uint32 &i)		{ return get_unsigned_num(s, e, i) - s; }
#if USE_LONG
template<typename C> inline size_t	from_string(const C *s, long &i)					{ return get_signed_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, ulong &i)					{ return get_unsigned_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, long &i)		{ return get_signed_num(s, e, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, ulong &i)		{ return get_unsigned_num(s, e, i) - s; }
#endif
#if USE_64BITREGS
template<typename C> inline size_t	from_string(const C *s, int64 &i)					{ return get_signed_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, uint64 &i)					{ return get_unsigned_num(s, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, int64 &i)		{ return get_signed_num(s, e, i) - s; }
template<typename C> inline size_t	from_string(const C *s, const C *e, uint64 &i)		{ return get_unsigned_num(s, e, i) - s; }
#endif

iso_export size_t	from_string(const char *s, bool &i);
iso_export size_t	from_string(const char *s, float &f);
iso_export size_t	from_string(const char *s, double &f);
iso_export size_t	from_string(const char *s, long double &f);
//iso_export size_t	from_string(const char *s, GUID &g);

iso_export size_t	from_string(const char *s, const char *e, bool &i);
iso_export size_t	from_string(const char *s, const char *e, float &f);
iso_export size_t	from_string(const char *s, const char *e, double &f);
iso_export size_t	from_string(const char *s, const char *e, long double &f);
iso_export size_t	from_string(const char *s, const char *e, GUID &g);

template<typename C, class T> struct has_from_string {
	template<typename U>	static decltype(from_string((const C*)0, *(U*)0))				test0(int);
	template<typename>		static void														test0(...);
	template<typename U>	static decltype(from_string((const C*)0, (const C*)0, *(U*)0))	test1(int);
	template<typename>		static void														test1(...);

	static const bool is_string		= iso::is_string<T>::value;					// is it (or does it inherit from) a string_base<B>?
	static const bool has0			= !same_v<decltype(test0<T>(0)), void>;		// is there a from_string(const C*, T)?
	static const bool has1			= !same_v<decltype(test1<T>(0)), void>;		// is there a from_string(const C*, const C*, T)?
};

template<typename C, typename T> inline size_t	from_string(const C *s, const C *e, T &t)			{ return from_string(s, t); }
template<typename T, typename B> inline size_t	from_string(const string_base<B> &s, T &t)			{ return from_string(s.begin(), s.end(), t); }
template<typename T, typename C> T				from_string(const C *s)								{ T t = T(); from_string(s, string_end(s), t); return t; }
template<typename T, typename C> T				from_string(const C *s, const C *e)					{ T t = T(); from_string(s, e, t); return t; }
template<typename T, typename B> T				from_string(const string_base<B> &s)				{ T t = T(); from_string(s.begin(), s.end(), t); return t; }

template<typename C, typename T> inline size_t	from_string(const C *s, constructable<T> &i)				{ return from_string(s, (T&)i); }
template<typename C, typename T> inline size_t	from_string(const C *s, const C *e, constructable<T> &i)	{ return from_string(s, e, (T&)i); }

template<typename C, int B, typename T> inline size_t	from_string(const C *s, baseint<B,T> &i)	{
	return s ? get_num_base<B, T>(skip_whitespace(s), i.i) - s : 0;
}
template<typename C, int B, typename T> inline size_t	from_string(const C *s, const C *e, baseint<B,T> &i)	{
	return s ? get_num_base<B, T>(skip_whitespace(s, e), e, i.i) - s : 0;
}

template<typename R, typename T>	_read_as<R, T>	read_as(T p)	{ return p; }
//template<typename R, typename B, typename T>	struct _read_as<R,T B::*>	{ T B::* p; _read_as(T B::* p) : p(p) {} };
//template<typename R, typename B, typename T>	_read_as<R,T B::*>	read_as(T B::* p)	{ return p; }

template<typename R, typename T, typename C> inline size_t	from_string(const C *s, _read_as<R,T> &x) {
	R	r;
	size_t	len = from_string(s, r);
	if (len)
		x.t = T(r);
	return len;
}

template<typename C, typename T, T N> inline size_t	from_string(const C *s, meta::constant<T,N> &x)	{ return 1; }

template<typename C, typename T> size_t from_string(const C *s, formatted_s<T> &t) {
	if (t.flags & FORMAT::HEX)
		return get_num_base<16>(s, t.t) - s;
	return 0;
}

template<typename T> constexpr from_string_getter::operator T()			const { return s ? from_string<T>(s) : T(); }
template<typename T> constexpr bool from_string_getter::read(T &t)		const { return s && from_string(s, t); }
template<typename T> T from_string_getter::or_default(const T &def)		const { return s ? from_string<T>(s) : def; }

//-----------------------------------------------------------------------------
//	string_scan
//-----------------------------------------------------------------------------

template<typename C> class string_scanT {
	const C	*p, *endp;
	typedef case_insensitive<C>		CI;
public:
	typedef count_stringT<const C>	count_string;

	string_scanT(const C *p)							: p(p), endp(p + string_len(p))			{}
	string_scanT(const C *p, const char *end)			: p(p), endp(end)						{}
	string_scanT(const from_string_getter &s)			: p(s.s), endp(p + string_len(p))		{}
	template<typename T> string_scanT(const string_base<T> &s) : p(s.begin()), endp(s.end())	{}

	template<typename T>string_scanT& operator>>(T &t) const		{ return (*const_cast<string_scanT*>(this)) >> t; }
	const C*			getp()			const			{ return p;					}
	const C*			end()			const			{ return endp;				}
	size_t				remaining()		const			{ return max(endp - p, 0);	}
	bool				empty()			const			{ return p >= endp;			}
	count_string		remainder()		const			{ return str(p, endp);		}
	C					peekc()			const			{ return p < endp ? *p : 0;	}
	C					peekc(int n)	const			{ return p + n < endp ? p[n] : 0; }
	string_scanT&		move(int n)						{ p += n; return *this;		}
	void				backup()						{ move(-1); }	// for expression, e.g.

	string_scanT&		skip_whitespace()				{ while (p < endp && is_whitespace(*p)) ++p; return *this; }

	const C*			scan(C c)						{ if (const C *r = str(p, endp - p).find(c)) return p = r; return 0; }
	const C*			scan(const C *s)				{ if (const C *r = str(p, endp - p).find(s)) return p = r; return 0; }
	const C*			scan(CI c)						{ if (const C *r = str(p, endp - p).find(c)) return p = r; return 0; }
	const C*			scan(const CI *s)				{ if (const C *r = str(p, endp - p).find(s)) return p = r; return 0; }
	C					scan(const char_set &set)		{ if (const C *r = str(p, endp - p).find(set)) return *(p = r); return 0; }

	const C*			scan_skip(C c)					{ if (const C *r = str(p, endp - p).find(c)) return p = r + 1; return 0; }
	const C*			scan_skip(const CI *s)			{ if (const C *r = str(p, endp - p).find(s)) return p = r + string_len(s); return 0; }
	const C*			scan_skip(CI c)					{ if (const C *r = str(p, endp - p).find(c)) return p = r + 1; return 0; }
	const C*			scan_skip(const C *s)			{ if (const C *r = str(p, endp - p).find(s)) return p = r + string_len(s); return 0; }
	C					scan_skip(const char_set &set)	{ if (const C *r = str(p, endp - p).find(set)) { p = r + 1; return *r; } return 0; }
	template<typename T> count_string get_to_scan(T t)	{ auto s = p; if (scan_skip(t)) return str(s, p); p = s; return none; }


	bool				begins(const C *s)				{ return str(p, endp - p).begins(s); }
	bool				begins(const CI *s)				{ return str(p, endp - p).begins(s); }
	bool				begins(C c)						{ return remaining() && *p == c; }
	bool				begins(CI c)					{ return remaining() && *p == c; }
	C					begins(const char_set &set)		{ return remaining() && set.test(*p) ? *p : 0; }

	bool				check(const C *s)				{ if (begins(s))	{ p += string_len(s); return true; } return false; }
	bool				check(const CI *s)				{ if (begins(s))	{ p += string_len(s); return true; } return false; }
	bool				check(C c)						{ if (begins(c))	{ ++p; return true; } return false; }
	bool				check(CI c)						{ if (begins(c))	{ ++p; return true; } return false; }
	C					check(const char_set &set)		{ if (begins(set))	{ ++p; return true; } return false; }

	C					getc()							{ return p < endp ? *p++ : 0; }
	count_string		get_raw(const char_set &set)	{ const C *s = p; while (p < endp && set.test(*p)) ++p; return str(s, p); }
	count_string		get_token()						{ const C *s = skip_whitespace().getp(); while (p < endp && !is_whitespace(*p)) ++p; return str(s, p); }
	count_string		get_token(const char_set &set)	{ const C *s = skip_whitespace().getp(); while (p < endp && set.test(*p)) ++p; return str(s, p); }
	count_string		get_token(const char_set &first, const char_set &set) { const C *s = skip_whitespace().getp(); if (first.test(*s)) while (++p < endp && set.test(*p)); return str(s, p); }
	count_string		get_n(int n)					{ const C *s = p; move(n); return str(s, p); }
	bool				skip_to(int end);
	count_string		get_to(int end)					{ auto s = p; if (skip_to(end)) return str(s, p); p = s; return none; }

	template<typename C2, typename T> bool match_wild(const C2 *wild, T &matches);

	bool				get_utf8(char32 &c) {
		int n = char_length(*p);
		if (n > remaining())
			return false;
		p += get_char(c, p);
		return true;
	}
	uint32				get_utf8()						{ uint32 c; return get_utf8(c) ? c : 0; }

	template<typename T> inline T		get()			{ T t; move(int(from_string(p, endp, t))); return t; }
	template<typename T> inline bool	get(T &t)		{ size_t m = from_string(p, endp, t); move(int(m)); return m != 0; }
	getter<string_scanT>				get()			{ return *this; }
};

template<typename C> template<typename C2, typename T> bool string_scanT<C>::match_wild(const C2 *wild, T &matches) {
	while (auto c = *wild++) {
		if (c == '*') {
			c = *wild++;
			if (!c) {
				matches.push_back(remainder());
				return true;
			}
			auto	start = p;
			if (auto end = scan(c))
				matches.push_back(str(start, end));
		}
		if (!check(c))
			return false;
	}
	return !remaining();
}

template<typename C> bool string_scanT<C>::skip_to(int end) {
	for (;;) {
		int c = getc();
		if (c == 0)
			return false;

		if (c == end)
			return true;

		if (end == '"' || end == '\'') {
			if (c == '\\')
				move(1);
		} else switch (c) {
			case '<': 
				if (end == '>' && !skip_to('>'))
					return false;
				break;
			case '[':
				if (!skip_to(']'))
					return false;
				break;
			case '{':
				if (!skip_to('}'))
					return false;
				break;
			case '(':
				if (!skip_to(')'))
					return false;
				break;
			case ']': case '}': case ')':
				return false;
			case '"': case '\'':
				if (!skip_to(c))
					return false;
				break;
			default:
				if (end == 0)
					move(-1);
				break;
		}
		if (end == 0)
			return true;
	}
}

template<typename C> inline string_scanT<C>& operator>>(string_scanT<C> &a, const char c)		{ ISO_ASSERT(*a.getp() == c); return a.move(1); }
template<typename C> inline string_scanT<C>& operator>>(string_scanT<C> &a, const char *s)		{ size_t len = string_len(s); ISO_ASSERT(string_cmp(a.getp(), s, len) == 0); return a.move(int(len)); }
template<typename C, typename T> inline string_scanT<C>& operator>>(string_scanT<C> &a, T &t)	{ return a.move(int(from_string(a.getp(), a.end(), t))); }

template<typename C, typename T> inline string_scanT<C>& operator>>(string_scanT<C> &a, formatted_s<T> &&t)	{ return a.move(int(from_string(a.getp(), a.end(), t))); }

typedef string_scanT<char>		string_scan;
typedef string_scanT<char16>	string_scan16;

template<char SEP> struct parts {
	const char *s, *e;

	struct iterator {
		typedef	count_string	element, reference;
		typedef bidirectional_iterator_t iterator_category;
		const char *s, *e, *p, *n;

		iterator(const char *s, const char *e, const char *p)	: s(s), e(e), p(p), n(string_find(s, SEP, e))	{ if (!n) n = e; }
		iterator(const char *s) : iterator(s, string_end(s), s) {}

		count_string operator*() {
			return str(p, n);
		}
		count_string full() {
			return str(s, n);
		}
		iterator&	operator++() {
			if (n != e) {
				p = n + 1;
				n = string_find(p, SEP, e);
				if (!n)
					n = e;
			} else {
				p = n;
			}
			return *this;
		}
		iterator	operator++(int) {
			iterator	t = *this;
			operator++();
			return t;
		}

		iterator&	operator--() {
			if (p != s) {
				n = p == e ? p : p - 1;
				p = string_rfind(s, SEP, n);
				p = p ? p + 1 : s;
			} else {
				n = p;
			}
			return *this;
		}
		iterator	operator--(int) {
			iterator	t = *this;
			operator--();
			return t;
		}

		bool	operator==(const iterator &b) const { return p == b.p; }
		bool	operator!=(const iterator &b) const { return p != b.p; }
	};

	parts(const char *s)				: s(s), e(string_end(s))	{}
	parts(const count_string &s)		: s(s.begin()), e(s.end())	{}
	parts(const char *s, const char *e)	: s(s), e(e)				{}
	iterator	begin()		const { return iterator(s, e, s); }
	iterator	end()		const { return iterator(s, e, e); }
};

struct ansi_colour {
	const char *x;
	ansi_colour(const char *x) : x(x) {}
	template<typename C> friend accum<C> &operator<<(accum<C> &a, const ansi_colour &col) {
		return a << "\x1b[" << col.x << 'm';
	}
};

//-----------------------------------------------------------------------------
//	fixed_accum
//-----------------------------------------------------------------------------

template<typename C> class fixed_accumT : public accumT<fixed_accumT<C>, C> {
	typedef accumT<fixed_accumT<C>, C>	B;
	using B::startp;
	C	*p, *endp;
protected:
	fixed_accumT()		{}
	void				init(C *_p, C *_endp)				{ startp = p = _p; endp = _endp - 1; }
	void				init(C *_p, size_t n)				{ init(_p, _p + n); }
public:
	fixed_accumT(C *p, size_t n)							{ init(p, n);		}
	fixed_accumT(C *p, C *endp)								{ init(p, endp);	}
	fixed_accumT(const count_stringT<C> &p)					{ init(p.begin(), p.end()); }

	template<int N>		fixed_accumT(C(&p)[N])				{ init(p, N);		}
	template<int N>		fixed_accumT(string_base<C[N]> &s)	{ init(s, N);		}
	template<int N>		fixed_accumT(fixed_string<N,C> &s)	{ init(s.end(), s.remaining()); }
	~fixed_accumT() 										{ *p = 0; }

	C*					getp()			const				{ return p; }
	C*					getp(int &n) {
		C	*t	= p;
		int	r	= int(endp - p);
		if (n)
			p += (r = min(n, r));
		n = r;
		return t;
	}
	fixed_accumT&		reset()								{ p = startp; return *this;	}
	size_t				length()		const				{ return p - startp; }
	uint32				size32()		const				{ return (uint32)length(); }
	size_t				remaining()		const				{ return endp - p; }
	memory_block		remainder()		const				{ return {p, endp}; }
	C*					dup()								{ *p = 0; return strdup(startp); }
	friend string_base<C*>	str(fixed_accumT &s)			{ *s.p = 0; return s.startp; }
};

typedef fixed_accumT<char>		fixed_accum;
typedef fixed_accumT<char16>	fixed_accum16;
template<typename C, int N>	auto make_string_accum(C(&p)[N])		{ return fixed_accumT<C>(p, N); }
template<typename C>		auto make_string_accum(C *p, C *e)		{ return fixed_accumT<C>(p, e); }
template<typename C>		auto make_string_accum(C *p, size_t n)	{ return fixed_accumT<C>(p, n); }

inline const char *to_string(fixed_accum &v) {
	return str(v);
}

template<typename C, int N, typename T> inline fixed_accumT<C> operator<<(fixed_string<N,C> &s, const T &t) {
	fixed_accumT<C>	a(s, s + N);
	a.move(int(s.length())) << t;
	return a;
}
template<typename C, int N, typename T> inline fixed_accumT<C> operator<<(fixed_string<N,C> &&s, const T &t) {
	return s << t;
}
template<int N, typename C>	accum<C> fixed_string<N,C>::format(const C *fmt, ...) {
	va_list valist;
	va_start(valist, fmt);
	return fixed_accumT<C>(*this).vformat(fmt, valist);
}

template<int N, typename C> template<typename... PP> accum<C> fixed_string<N,C>::formati(string_paramT<C> fmt, PP... pp) {
	fixed_accumT<C>	a(*this);
	return a.move(int(this->length())).formati(move(fmt), pp...);
}

template<int N, typename C, typename... PP> inline	fixed_string<N,C> formati_string(string_paramT<C> fmt, PP... pp) {
	fixed_string<N, C>	f;
	fixed_accumT<C>(f).formati(move(fmt), pp...);
	return f;
}

template<typename... PP> inline	fixed_string<256> formati_string(string_param fmt, PP... pp) {
	return formati_string<256>(move(fmt), pp...);
}

//-----------------------------------------------------------------------------
//	buffer_accum<int>
//-----------------------------------------------------------------------------

template<typename C, int N>	class buffer_accumT : public fixed_accumT<C> {
protected:
	string_base<C[N]>	s;
public:
	buffer_accumT() : fixed_accumT<C>(s)	{}
	friend string_base<C*>			str(buffer_accumT &s)	{ *s.getp() = 0; return s.s.begin(); }
};

template<int N>	class buffer_accum : public buffer_accumT<char, N> {
public:
	buffer_accum()											{}
	buffer_accum(const char *fmt, ...)						{ va_list valist; va_start(valist, fmt); this->vformat(fmt, valist); }
	buffer_accum(const char *fmt, va_list valist)			{ this->vformat(fmt, valist); }
	template<typename T> buffer_accum(const T &t)			{ *this << t; }
	friend string_base<char*>		str8(buffer_accum &s)	{ return str(s); }
	friend fixed_string<N,char16>	str16(buffer_accum &s)	{ *s.getp() = 0; return fixed_string<N, char16>(s.s); }
};

template<int N>	class buffer_accum16 : public buffer_accumT<char16, N> {
public:
	buffer_accum16()										{}
	buffer_accum16(const char16 *fmt, ...)					{ va_list valist; va_start(valist, fmt); this->vformat(fmt, valist); }
	buffer_accum16(const char16 *fmt, va_list valist)		{ this->vformat(fmt, valist); }
	template<typename T> buffer_accum16(const T &t)			{ *this << t; }
	friend string_base<char16*>		str16(buffer_accum16 &s){ return str(s); }
	friend fixed_string<N,char>		str8(buffer_accum16 &s)	{ *s.getp() = 0; return fixed_string<N, char>(s.s); }
};

template<int N>	inline string_accum& operator<<(string_accum &a, const buffer_accum<N> &b)		{ return a.merge(b.begin(), b.length()); }
template<int N>	inline string_accum& operator<<(string_accum16 &a, const buffer_accum16<N> &b)	{ return a.merge(b.begin(), b.length()); }

template<typename T, typename C, int N> class buffered_accum : public accumT<buffered_accum<T,C,N>, C> {
protected:
	C		temp[N], *p;
	void	flush_all() {
		static_cast<T*>(this)->flush(temp, p - temp);
	}
	void	check_flush(C *e) {
		if (e > end(temp) - 64) {
			flush_all();
			if (e > p)
				memcpy(temp, p, (e - p) * sizeof(C));
			p = temp;
		}
	}
public:
	C*		getp(int &n) {
		if (n == 0) {
			check_flush(p);
			n = int(end(temp) - p);
			return p;
		}
		if (n < 0) {
			ISO_ASSERT(p + n >= temp);
			return p += n;
		}
		n = min(n, N);
		check_flush(p + n);
		C *t = p;
		p += n;
		return t;
	}
	buffered_accum&		putc(C c) {
		*p++ = c;
		check_flush(p);
		return *this;
	}

	buffered_accum()						: p(temp)	{}
	buffered_accum(buffered_accum &&b)		: p(temp)	{ b.flush(); }
	buffered_accum(const buffered_accum &b)	: p(temp)	{ unconst(b).flush_all(); }
	template<int N2> buffered_accum(const buffered_accum<T,C,N2> &b) : p(temp)	{ unconst(b).flush(); }
	// T has been destructed first, so can't do this:
	//~buffered_accum()			{ static_cast<T*>(this)->flush(temp, p - temp); }
};

//-----------------------------------------------------------------------------
//	stream_accum, trace_accum, dummy_accum, temp_accum
//-----------------------------------------------------------------------------

template<typename W, int N> struct stream_accum : buffered_accum<stream_accum<W, N>, char, N> {
	W			w;
	void		flush(const char *s, size_t n) { w.writebuff(s, n); }
	template<typename...PP> stream_accum(PP&&...pp) : w(forward<PP>(pp)...)	{}
	~stream_accum()	{ this->flush_all(); }
};
template<typename W, int N> struct stream_accum<W&, N> : buffered_accum<stream_accum<W&, N>, char, N> {
	W&			w;
	void		flush(const char *s, size_t n) { w.writebuff(s, n); }
	stream_accum(W &w)							: w(w) {}
	stream_accum(W &w, const char *fmt, ...)	: w(w) { va_list valist; va_start(valist, fmt); this->vformat(fmt, valist); }
	template<typename T> stream_accum(W &w, const T &t) : w(w) { *this << t; }
	~stream_accum()	{ this->flush_all(); }
};

template<int N, typename W> inline stream_accum<W, N> make_stream_accum(W &&w) {
	return forward<W>(w);
}
template<typename W> inline stream_accum<W, 512> make_stream_accum(W &&w) {
	return forward<W>(w);
}

struct trace_accum : buffered_accum<trace_accum, char, 512> {
	void		flush(const char *s, size_t n) {  save(unconst(s)[n], 0), _iso_debug_print(s); }
	trace_accum() {}
	trace_accum(const char *fmt, ...)	{ va_list valist; va_start(valist, fmt); this->vformat(fmt, valist); }
	template<typename T> trace_accum(const T &t) { *this << t; }
	~trace_accum()	{ this->flush_all(); }
};

struct dummy_accum {
	dummy_accum() {}
	dummy_accum(const char *fmt, ...)	{}
	template<typename T> dummy_accum(const T &t) {}
	template<typename T> dummy_accum& operator<<(const T &t) { return *this; }
};

class temp_accum {
	malloc_block	mb;
	fixed_accum		sa;
public:
	temp_accum(size_t size) : mb(size), sa(mb, mb.size()) {}
	void		reset()			{ sa.reset(); }
	template<typename T> string_accum&	operator<<(const T &t)	{ return sa << t; }
	operator	string_accum&()	{ return sa; }
	operator	string_param()	{ return str(sa); }
//	operator	const char*()	{ return str(sa); }
};

//-----------------------------------------------------------------------------
//	builder
//-----------------------------------------------------------------------------

template<typename C> class builder : public accumT<builder<C>, C> {
	friend alloc_string<C>;
	friend string_paramT<C>;
	using accumT<builder<C>, C>::startp;
	static const int N = 256;

	C		buffer[N];
	size_t	curr_size, max_size;

	C*		resize(size_t n) {
		if (n < N) {
			if (startp != buffer) {
				memcpy(buffer, startp, n);
				free(startp);
				max_size = N;
			}
			return startp = buffer;
		}
		if (startp != buffer)
			return startp = (C*)realloc(startp, (max_size = n) * sizeof(C));

		startp = (C*)malloc((max_size = n) * sizeof(C));
		memcpy(startp, buffer, curr_size);
		return startp;
	}

	C*	detach() {
		resize(curr_size + (curr_size != 0));
		string_term(startp, curr_size);
		curr_size = 0;
		max_size = N;

		if (startp == buffer)
			return strdup(startp);
		return exchange(startp, buffer);
	}

	string_param_storage<C>	detach1() {
		resize(curr_size + (curr_size != 0));
		string_term(startp, curr_size);
		curr_size = 0;
		max_size = N;

		if (startp == buffer)
			return {startp, false};
		return {exchange(startp, buffer), true};
	}
public:
	C*		getp(int &n) {
		size_t	offset	= curr_size;

		if (n == 0) {
			n = int(max_size - curr_size);
			if (n < 64)
				n = 256;
		} else {
			curr_size += n;
		}

		if (offset + n > max_size)
			resize(max(max_size * 2, offset + n));

		return startp + offset;
	}

	builder() : curr_size(0), max_size(N) { startp = buffer; }
	builder(const char *fmt, ...)						: builder()	{ va_list valist; va_start(valist, fmt); vformat(fmt, valist); }
	template<typename T> explicit builder(const T &t)	: builder()	{ *this << t; }

	~builder() 							{ if (startp != buffer) free(startp); }
	malloc_block	detach_block()		{
		if (startp == buffer)
			return const_memory_block(startp, curr_size);
		resize(curr_size);
		return malloc_block::own(exchange(startp, buffer), curr_size);
	}
	operator count_stringT<C>()			{ return {startp, curr_size}; }
	explicit operator bool()	const	{ return !!startp; }
};

template<typename C> class temp_builder : public accumT<temp_builder<C>, C> {
	typedef	alloc_string<C>	S;
	S			*s;
	size_t		curr_size, max_size;
public:
	C*		getp(int &n) {
		size_t	offset	= curr_size;

		if (n == 0) {
			n = int(max_size - curr_size);
			if (n < 64)
				n = 256;
		} else {
			curr_size += n;
		}

		if (offset + n > max_size)
			this->startp = s->resize(max_size = max(max_size * 2, offset + n)).begin();

		return s->begin() + offset;
	}

	void	flush()				{ this->startp = s->resize(max_size = curr_size).begin(); }

	temp_builder(S &s, bool clear = true)	: s(&s)								{ this->startp = s.begin(); max_size = s.length(); curr_size = clear ? 0 : max_size; }
	temp_builder(const temp_builder &b)		: s(b.s)							{ this->startp = b.startp; curr_size = b.curr_size; max_size = b.max_size; }
	template<typename T> temp_builder(S &s, const T &t, bool clear) : temp_builder(s, clear)		{ *this << t; }
	~temp_builder() 															{ if (s) s->resize(curr_size); }
	S*		detach()			{ s->resize(curr_size); this->startp = 0; return exchange(s, nullptr); }
	operator S&()				{ return *detach(); }
	operator string_paramT<C>()	{ return *detach(); }
};

template<typename C>				inline auto	build(alloc_string<C> &a, bool clear = false)	{ return temp_builder<C>(a, clear); }
template<typename C, typename T>	inline auto	operator<<(alloc_string<C> &a, const T &t)		{ return temp_builder<C>(a, t, false); }
template<typename C, typename T>	inline auto	operator<<(alloc_string<C> &&a, const T &t)		{ return temp_builder<C>(a, t, false); }
template<typename C>				alloc_string<C>::operator accum<C>()						{ return temp_builder<C>(*this, false); }

typedef builder<char>	string_builder;
typedef builder<char16>	string16_builder;

struct toggle_accum : temp_builder<char> {
	string&	get_buffer() {
		static string	s[2];
		static int		i = 0;
		return s[i++ & 1];
	}
	toggle_accum()									: temp_builder<char>(get_buffer())	{}
	toggle_accum(const char *fmt, ...)				: temp_builder<char>(get_buffer())	{ va_list valist; va_start(valist, fmt); vformat(fmt, valist); }
	template<typename T> toggle_accum(const T &t)	: temp_builder<char>(get_buffer())	{ *this << t; }
};

//-----------------------------------------------------------------------------
//	things that needed builder
//-----------------------------------------------------------------------------

template<class W, typename F> exists_t<decltype(declval<F>()(declval<string_accum&>())), bool> custom_write(W &w, const F &t) {
	string_builder a;
	t(a);
	return w.write(a.to_string());
}

template<typename C, typename T> inline auto get_string(const T &t, enable_if_t<has_to_string<C,T>::get == 1> *dummy = 0) {
	return to_string(get(t));
}
template<typename C, typename T> inline auto get_string(const T &t, ...) {
	builder<C>	b;
	b << t;
	return str(b);
}

/*	NOT USED?
template<typename B> struct split_s {
	buffer_accum<256>	a;
	B					b;
	int					col, last;
	split_s(B &&b, int col) : b(forward<B>(b)), col(col), last(0) {}
	~split_s()			{ b << str(a); }
	template<typename T> auto& operator<<(const T &t) {
		a << t;
		int	len = a.size32();
		if (len > col) {
			auto	s = str(a);
			b << s.slice(0, last) << '\n';
			memcpy(s, s + last, len - last);
			a.move(-last);
		}
		return *this;
	}
	auto& operator<<(const _none&) { last = a.size32(); return *this; }
};

template<typename B> inline auto split_at(B &&b, int col) {
	return split_s<B>(forward<B>(b), col);
}
*/

/*
class tabs : _justify {
	iso_export void	tab(string_param &p) const;
public:
	explicit tabs(uint16 w, char c = ' ') : _justify(w, c)						{}
	tabs(string_accum &a, uint16 w, char c = ' ') : _justify(a, w, c)			{}
	template<typename T> const tabs& operator<<(const T &t)			const		{ tab(get_string(t)); return *this; }
	string_accum& operator<<(const _none&)							const		{ return *a; }
	friend const tabs &operator<<(string_accum &a, const tabs &t)				{ t.init(a); return t; }
};
*/
template<typename C> alloc_string<C> replace(const C *srce, const C *from, const C *to) {
	builder<C>			b;
	size_t				flen	= string_len(from);
	size_t				tlen	= string_len(to);

	while (const C *found = str(srce).find(from)) {
		b.merge(srce, found - srce);
		b.merge(to, tlen);
		srce = found + flen;
	}
	b << srce;
	return b;
}

template<typename B, typename C> alloc_string<C> replace(const string_base<B> &srce, const C *from, const C *to) {
	typedef noconst_t<typename string_base<B>::element> E;
	builder<C>			b;
	size_t				flen	= string_len(from);
	size_t				tlen	= string_len(to);

	const E *begin = srce.begin(), *end = srce.end();
	while (const E *found = str(begin, end).find(from)) {
		b.merge(begin, found - begin);
		b.merge(to, tlen);
		begin = found + flen;
	}
	b.merge(begin, end - begin);
	return move(b);
}

template<typename C> template<typename R> bool alloc_string<C>::read(R &r) {
	if (r.eof())
		return false;

	auto b = build(*this, true);
	for (C c; ISO_VERIFY(r.read(c)) && c;)
		b.putc(c);
	return true;
}
/*
template<typename C> template<typename R> bool alloc_string<C>::read_to(R &r, const char_set &set, bool clear) {
	if (r.eof())
		return false;

	auto b = build(*this, clear);
	for (C c; r.read(c) && c && !set.test(c);)
		b.putc(c);
	return true;
}
*/

//-----------------------------------------------------------------------------
//	multi_string
//-----------------------------------------------------------------------------

template<typename C> struct multi_string_iterator {
	typedef C	*element, *reference;
	typedef forward_iterator_t	iterator_category;
	C	*p;
	multi_string_iterator(C *p = nullptr) : p(p)	{}
	multi_string_iterator&	operator++()							{ if (p) p += string_len(p) + 1; return *this; }
	multi_string_iterator	operator++(int)							{ multi_string_iterator t(p); p += string_len(p) + 1; return t; }
	C*			operator*() const									{ return p && *p ? p : 0; }
	bool		operator==(const multi_string_iterator &b)	const	{ return **this == *b; }
	bool		operator!=(const multi_string_iterator &b)	const	{ return **this != *b; }
	bool		operator>(const multi_string_iterator &b)	const	{ return b.p && **this > *b; }
};

template<typename C> struct embedded_multi_string {
	C	p[1];

	typedef	multi_string_iterator<C>		iterator;
	typedef	multi_string_iterator<const C>	const_iterator;

	operator		C*()				{ return p; }
	iterator		begin()				{ return p; }
	iterator		end()				{ return nullptr; }

	operator		const C*()	const	{ return p; }
	const_iterator	begin()		const	{ return p; }
	const_iterator	end()		const	{ return nullptr; }

	size_t		size()	const {
		size_t	n = 1;
		for (const C *x = p; *x; ++n)
			while (*x++);
		return n;
	}
	size_t		length() const {
		const C *x = p;
		while (*x)
			while (*x++);
		return x - p;
	}

	auto	dup() const { return (embedded_multi_string<noconst_t<C>>*)memdup(p, (length() + 1) * sizeof(C)); }
};

template<typename C> struct multi_string_base {
	typedef embedded_multi_string<C>		P;
	typedef multi_string_iterator<C>		iterator;
	typedef multi_string_iterator<const C>	const_iterator;

	P	*p;

	template<typename S> static P *create(const S *pp, size_t n) {
		size_t	t = 1;
		for (const S *i = pp, *e = i + n; i != e; ++i)
			t += *i ? string_len(*i) + 1 : 1;
		P	*p = (P*)malloc(t);
		C	*d = *p;
		for (const S *i = pp, *e = i + n; i != e; ++i) {
			if (const C *s = *i)
				while (*d++ = *s++);
		}
		*d++ = 0;
		return p;
	}

	multi_string_base(P *p = 0)	: p(p) {}
	multi_string_base(C *p)		: p((P*)p) {}

	P*		operator->()	const	{ return p; }
	iterator	begin()		const	{ return iterator((char*)p); }
	iterator	end()		const	{ return iterator(0); }

	size_t		size()		const	{ return p ? p->size() : 0; }
	size_t		length()	const	{ return p ? p->length() : 0; }
};

template<typename C> struct multi_string_alloc : multi_string_base<C> {
	typedef embedded_multi_string<C>	P;
	typedef multi_string_base<C>		B;
	typedef multi_string_base<const C>	CB;
	using B::p;

	C	*_append(size_t n)	{
		size_t n0 = p->length();
		p = (P*)realloc(p, (n0 + n + 1) * sizeof(C));
		C *d = *p + n0;
		d[n] = 0;
		return d;
	}
	template<typename I> void append(I s, I e)		{ string_copy(_append(chars_count<C>(s, e) + 1), s, e);	}
	template<typename S> void append(const S *s)	{ string_copy(_append(chars_count<C>(s) + 1), s);	}

	multi_string_alloc()					{}
	multi_string_alloc(size_t size)												: B((P*)malloc(size * sizeof(C))) {}
	multi_string_alloc(const C **pp, size_t n)									: B(B::create(pp, n))	{}
	multi_string_alloc(const B &b)												: B(b->dup())			{}
	multi_string_alloc(const CB &b)												: B(b->dup())			{}
	multi_string_alloc(multi_string_alloc &&b)									: B(b.p)				{ b.p = 0; }
	template<typename S> multi_string_alloc(const string_base<S> *pp, size_t n)	: B(B::create(pp, n))	{}
	~multi_string_alloc()					{ free(p); }

	multi_string_alloc&	operator=(const B &b)				{ free(p); p = b->dup(); return *this; }
	multi_string_alloc&	operator=(const CB &b)				{ free(p); p = b->dup(); return *this; }
	multi_string_alloc&	operator=(multi_string_alloc &&b)	{ free(p); p = b.p; b.p = 0; return *this; }

	multi_string_alloc& operator+=(const B &b)										{ size_t len2 = b->length(); memcpy(_append(len2), b.p, len2 * sizeof(C)); return *this; }
	multi_string_alloc& operator+=(const CB &b)										{ size_t len2 = b->length(); memcpy(_append(len2), b.p, len2 * sizeof(C)); return *this; }
	template<typename S> multi_string_alloc& operator+=(const string_base<S> &b)	{ append(b.begin(), b.end()); return *this;	}
	template<typename S> multi_string_alloc& operator+=(const S *s)					{ append(s); return *this; }
	template<typename S> multi_string_alloc	operator+(const string_base<S> &b) &&	{ append(b.begin(), b.end()); return move(*this); }
};

typedef multi_string_base<char>			multi_string;
typedef multi_string_base<char16>		multi_string16;
typedef multi_string_base<const char>	cmulti_string;
typedef multi_string_base<const char16>	cmulti_string16;

//-----------------------------------------------------------------------------
//	escape sequences
//-----------------------------------------------------------------------------

template<typename C> void escape(accum<C> &a, uint32 c) {
	switch (c) {
		case '\a':	a.putc(C('\\')); c = 'a';	break;
		case '\b':	a.putc(C('\\')); c = 'b';	break;
		case '\f':	a.putc(C('\\')); c = 'f';	break;
		case '\n':	a.putc(C('\\')); c = 'n';	break;
		case '\r':	a.putc(C('\\')); c = 'r';	break;
		case '\t':	a.putc(C('\\')); c = 't';	break;
		case '\v':	a.putc(C('\\')); c = 'v';	break;
		case '\\':
		case '\'':
		case '"':	a.putc(C('\\')); break;
	}
	if (c < ' ' || c >= 0x7f) {
		if (c < 0x100)
			a.format("\\x%02x", (uint8)c);
		else if (c < 0x10000)
			a.format("\\u%04x", c);
		else
			a.format("\\U%08x", c);
	} else {
		a.putc(c);
	}
}

inline auto escaped(uint32 c) {
	return [c](string_accum &a) { escape(a, c); };
}

template<typename B> auto escaped(const string_base<B> &s) {
	return [s](string_accum &a) {
		for (auto c : s)
			escape(a, c);
	};
}

template<typename C> alloc_string<C> escape(const C *s, const C *e) {
	builder<C>	b;
	while (s < e)
		escape(b, *s++);
	return b;
}

template<typename B> auto escape(const string_base<B> &s) {
	return escape(s.begin(), s.end());
}

template<typename C> uint32 unescape1(string_scanT<C> &s) {
	switch (uint32 c = s.getc()) {
		case 'a':	return '\a';
		case 'b':	return '\b';
		case 'f':	return '\f';
		case 'n':	return '\n';
		case 'r':	return '\r';
		case 't':	return '\t';
		case 'v':	return '\v';
		case '\\':
		case '\'':
		case '"':	return c;

		case 'x':
			c = 0;
			while (is_hex(s.peekc()))
				c = c * 16 + from_digit(s.getc());
			return c;

		case 'u':
			c = 0;
			for (int i = 4; i-- && is_hex(s.peekc());)
				c = c * 16 + from_digit(s.getc());
			return c;

		case 'U':
			c = 0;
			for (int i = 8; i-- && is_hex(s.peekc());)
				c = c * 16 + from_digit(s.getc());
			return c;

		case '0': case '1': case '2': case '3': {
			c -= '0';
			if (between(s.peekc(), '0', '7')) {
				c = c * 8 + s.getc() - '0';
				if (between(s.peekc(), '0', '7'))
					c = c * 8 + s.getc() - '0';
			}
			return c;
		}
		default:
			return c;
	}
}

template<typename C> auto unescaped(const string_scanT<C> &s) {
	return [s](string_accum &a) {
		while (s.remaining()) {
			uint32	c = s.getc();
			a.putc(c == '\\' ? unescape1(s) : c);
		}
	};
}

template<typename C> alloc_string<C> unescape(string_scanT<C> &&s) {
	builder<C>	b;
	while (s.remaining()) {
		uint32	c = s.getc();
		b.putc(c == '\\' ? unescape1(s) : c);
	}
	return b;
}

template<typename B> auto unescape(const string_base<B> &s) {
	return unescape(string_scanT<typename string_traits<B>::element>(s));
}

}//namespace iso

#endif // STRINGS_H
