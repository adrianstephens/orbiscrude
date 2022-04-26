#ifndef IDENTIFIER_H
#define IDENTIFIER_H

#include "base/defs.h"

namespace iso {
template<typename T> inline enable_if_t<is_enum<T>, size_t>			from_string(const char *s, T &t);
template<typename T> inline enable_if_t<is_enum<T>, const char*>	to_string(const T &t);
}

#include "base/strings.h"
#include "base/pointer.h"
#include "base/bits.h"
#include "base/functions.h"

#define	_MAKE_FIELD(S,X)				field::make<S>(#X, &S::X),
#define	_MAKE_FIELDS(S, ...)			VA_APPLYP(_MAKE_FIELD, S, __VA_ARGS__)
#define	MAKE_FIELDS(S, ...)				template<> field fields<S>::f[] = { _MAKE_FIELDS(S, __VA_ARGS__) field::terminator<S>() }

#define	DECLARE_VALUE_FIELD(S)			template<> struct fields<S> : value_field<S>	{};

#define	_MAKE_ENUM(S,X)					#X,
#define	_MAKE_ENUMS(S, ...)				VA_APPLYP(_MAKE_ENUM, S, __VA_ARGS__)
#define	_MAKE_VALUE(S,X)				{#X, X},
#define	_MAKE_VALUES(S, ...)			VA_APPLYP(_MAKE_VALUE, S, __VA_ARGS__)
#define	_MAKE_PREFIXED_VALUE(P,X)		{#X, P##X},
#define	_MAKE_PREFIXED_VALUES(P, ...)	VA_APPLYP(_MAKE_PREFIXED_VALUE, P, __VA_ARGS__)

#define	MAKE_ENUMS(S, ...)				template<> const char *field_names<S>::s[]	= { _MAKE_ENUMS(S, __VA_ARGS__) }

#define	DECLARE_VALUE_ENUMS(S)			template<> struct field_names<S> { static field_value s[]; };
#define	MAKE_VALUE_ENUMS(S, ...)		field_value field_names<S>::s[] = { _MAKE_VALUES(S, __VA_ARGS__) 0 }

#define	DECLARE_BIT_ENUMS(S)			template<> struct field_names<S> { static field_bit s[]; };
#define	MAKE_BIT_ENUMS(S, ...)			field_bit field_names<S>::s[] =	{ _MAKE_VALUES(S, __VA_ARGS__) 0 }

#define	DECLARE_PREFIXED_ENUMS(S)		template<> struct field_names<S> { static field_prefix<const char*> s; };
#define	MAKE_PREFIXED_ENUMS(S, P, ...)	field_prefix<const char*>	field_names<S>::s	= { #P, (const char*[]) {_MAKE_ENUMS(S, __VA_ARGS__) }}

#define	DECLARE_PREFIXED_VALUE_ENUMS(S)		template<> struct field_names<S> { static field_prefix<field_value> s; };
#define	MAKE_PREFIXED_VALUE_ENUMS(S, P, ...) field_prefix<field_value>	field_names<S>::s	= { #P, (field_value[]) {_MAKE_PREFIXED_VALUES(P, __VA_ARGS__) 0}}

#define	DECLARE_PREFIXED_BIT_ENUMS(S)		template<> struct field_names<S> { static field_prefix<field_bit> s; };
#define	MAKE_PREFIXED_BIT_ENUMS(S, P, ...)	field_prefix<field_bit>	field_names<S>::s	= { #P, (field_bit[]) {_MAKE_PREFIXED_VALUES(P, __VA_ARGS__) 0}}

namespace iso {

//-----------------------------------------------------------------------------
//	identifier
//-----------------------------------------------------------------------------

enum IDFMT {
	IDFMT_LEAVE, IDFMT_LOWER, IDFMT_UPPER, IDFMT_LOWER_NOUL, IDFMT_UPPER_NOUL, IDFMT_CAMEL,
	IDFMT_MASK				= 7,
	IDFMT_TYPE_PREFIX		= 1 << 3,
	IDFMT_STORAGE_PREFIX	= 1 << 4,
	IDFMT_SEMANTIC_PREFIX	= 1 << 5,

	// for fields
	IDFMT_FIELDNAME			= 1 << 7,
	IDFMT_CONSTNUMS			= 1 << 8,
	IDFMT_NONAMES			= 1 << 9,
	IDFMT_NOSPACES			= 1 << 10,
	IDFMT_NOPREFIX			= 1 << 11,
	IDFMT_FIELDNAME_AFTER_UNION = 1 << 12,

	// for gpu trees
	IDFMT_FOLLOWPTR			= 1 << 13,
	IDFMT_OFFSETS			= 1 << 14,
	IDFMT_MARKERS			= 1 << 15,
};

inline IDFMT operator*(IDFMT a, bool b)		{ return b ? a : IDFMT(0); }
inline IDFMT operator*(bool b, IDFMT a)		{ return b ? a : IDFMT(0); }
inline IDFMT operator|(IDFMT a, IDFMT b)	{ return IDFMT(uint32(a) | uint32(b)); }
inline IDFMT operator^(IDFMT a, IDFMT b)	{ return IDFMT(uint32(a) ^ uint32(b)); }
inline IDFMT operator-(IDFMT a, IDFMT b)	{ return IDFMT(uint32(a) & ~uint32(b)); }

inline IDFMT operator|=(IDFMT &a, IDFMT b)	{ return a = a | b; }
inline IDFMT operator^=(IDFMT &a, IDFMT b)	{ return a = a ^ b; }
inline IDFMT operator-=(IDFMT &a, IDFMT b)	{ return a = a - b; }

enum IDTYPE {
	IDTYPE_TYPES			= 1 << 0,
	IDTYPE_TYPES_MASK		= 1 << 15,

	IDTYPE_BOOL				= 1,	// f	Flag (Boolean)
	IDTYPE_WORD				= 2,	// w	Word with arbitrary contents
	IDTYPE_CHAR				= 3,	// ch	Character, usually in ASCII text
	IDTYPE_BYTE				= 4,	// b	Byte, not necessarily holding a coded character
	IDTYPE_STRING			= 5,	// sz	Pointer to first character of a zero terminated string
	IDTYPE_PASCALSTRING		= 6,	// st	Pointer to a string. First byte is the count of characters cch
	IDTYPE_HANDLE			= 7,	// h	pp (in heap).

	IDTYPE_SEMANTIC			= 1 << 4,
	IDTYPE_SEMANTIC_MASK	= IDTYPE_SEMANTIC * 15,
	IDTYPE_DIFFERENCE		= IDTYPE_SEMANTIC * 1,	// d
	IDTYPE_COUNT			= IDTYPE_SEMANTIC * 2,	// c
	IDTYPE_RANGE			= IDTYPE_SEMANTIC * 3,	// rg
	IDTYPE_INDEX			= IDTYPE_SEMANTIC * 4,	// i
	IDTYPE_BYTECOUNT		= IDTYPE_SEMANTIC * 5,	// cb
	IDTYPE_WORDCOUNT		= IDTYPE_SEMANTIC * 6,	// cw
	IDTYPE_DWORDCOUNT		= IDTYPE_SEMANTIC * 7,	// cdw

	IDTYPE_POINTER			= 1 << 8,	// p
	IDTYPE_POINTER_MASK		= IDTYPE_POINTER * 15,

	IDTYPE_STORAGE			= 1 << 12,
	IDTYPE_STORAGE_MASK		= IDTYPE_STORAGE * 3,
	IDTYPE_LOCAL			= IDTYPE_STORAGE * 0,
	IDTYPE_MEMBER			= IDTYPE_STORAGE * 1,
	IDTYPE_STATIC			= IDTYPE_STORAGE * 2,
	IDTYPE_GLOBAL			= IDTYPE_STORAGE * 3,
};

string_accum&	FormatIdentifier(string_accum &sa, const char *p, IDFMT format, IDTYPE type = IDTYPE_MEMBER);
//string			FormatIdentifier(const char *p, IDFMT format, IDTYPE type = IDTYPE_MEMBER);
inline auto FormatIdentifier(const char *p, IDFMT format, IDTYPE type) {
	return [=](string_accum &sa) {
		return FormatIdentifier(sa, p, format, type);
	};
}

template<typename T> class IdentifierT {
	const T		str;
	IDFMT		format;
	IDTYPE		type;
public:
	IdentifierT(T &&str, IDFMT format, IDTYPE type) : str(move(str)), format(format), type(type) {}
	operator string() const { return FormatIdentifier(begin(str), format, type); }
	string		get() const { return FormatIdentifier(begin(str), format, type); }
	friend string_accum &operator<<(string_accum &sa, const IdentifierT &i) { return FormatIdentifier(sa, begin(i.str), i.format, i.type); }
};

template<typename T> IdentifierT<T>	Identifier(T &&str, IDFMT format, IDTYPE type = IDTYPE_MEMBER) {
	return IdentifierT<T>(forward<T>(str), format, type);
}
IdentifierT<string>	Identifier(const char *prefix, const char *name, IDFMT format, IDTYPE type = IDTYPE_MEMBER);

//-----------------------------------------------------------------------------
//	Option
//-----------------------------------------------------------------------------

struct array_entry;
template<typename T> inline size_t from_string(const char *s, _read_as<array_entry, T> &x) {
	return from_string(s, x.t.push_back());
}

template<uint32 F> struct _flag_option;
template<uint32 F, typename I> inline size_t from_string(const char *s, _read_as<_flag_option<F>, I> &x) {
	x.t |= F;
	return 1;
}
template<uint32 F, typename B, typename T> _read_as<_flag_option<F>,T B::*> flag_option(T B::* p) {
	return p;
}

struct structure_field {
	typedef	size_t read_type(const char *v, void *p);
	read_type	*_read;
	uint32		offset;
	template<typename T, typename B>				constexpr structure_field(T B::* p)				: _read((read_type*)(size_t(*)(const char*,T&))&iso::from_string), offset(uint32(T_get_member_offset(p))) {}
	template<typename R, typename B, typename T>	constexpr structure_field(_read_as<R,T B::*> p)	: _read((read_type*)(size_t(*)(const char*,_read_as<R,T>&))&iso::from_string), offset(uint32(T_get_member_offset(p.t)))	{}
};

typedef meta::constant<bool, true>	yes;
typedef meta::constant<bool, false>	no;

template<typename T> inline size_t from_string(const char *s, _not<T> &x) {
	return from_string(s, x.t);
}

struct Option {
	const char		*name;
	structure_field	off;
	bool			operator==(const char *n) const		{ return str(n).begins(name) && !is_alpha(n[strlen(name)]); }
	size_t			set(void *p, const char *v)	const	{ return off._read(v, (char*)p + off.offset); }
};

//-----------------------------------------------------------------------------
//	field values
//-----------------------------------------------------------------------------

struct field;

template<typename T> struct typed_nullptr { constexpr operator T*() const { return nullptr; }; };

struct field_value {
	const char *name;
	int32		value;
};
struct field_bit {
	const char *name;
	uint32		value;
};

struct field_custom {};

typedef string_accum&	(*field_callback_func)(string_accum&, const field*, const uint32le*, uint32, uint32);

struct field_thing {
	uint32		*p;
	const field	*pf;
	field_thing() {}
	field_thing(uint32 *p, const field *pf) : p(p), pf(pf) {}
	virtual ~field_thing()	{}
};
typedef field_thing* (*field_follow_func)(const field*, const uint32le*, uint32);

template<typename T> struct field_callback;

template<typename T> struct field_dot : T {};

template<typename T> struct field_prefix {
	const char	*prefix;
	const T		*names;
};

template<typename T> struct field_names		{ static const char *s[]; };
template<typename T> struct field_values	{ static field_value s[]; };
template<typename T> struct field_customs	{ static field_custom s[]; };
template<typename T> field_custom field_customs<T>::s[1];
template<typename T> struct field_names<const T>			: field_names<T>	{};
template<typename T> struct field_names<constructable<T>>	: field_names<T>	{};
template<typename T> struct field_values<const T>			: field_values<T>	{};

struct field_names_none						{ static const char **s; };
struct field_names_hex						{ static const char *s[]; };
struct field_names_signed					{ static const char *s[]; };

template<typename T> struct field_names<T*> : field_names_hex		{};
template<typename T> struct field_names<baseint<16,T>> : field_names_hex {};
template<> struct field_names<uint8>		: field_names_none		{};
template<> struct field_names<uint16>		: field_names_none		{};
template<> struct field_names<uint32>		: field_names_none		{};
template<> struct field_names<uint64>		: field_names_none		{};
template<> struct field_names<int8>			: field_names_signed	{};
template<> struct field_names<int16>		: field_names_signed	{};
template<> struct field_names<int32>		: field_names_signed	{};
template<> struct field_names<int64>		: field_names_signed	{};
#ifndef USE_SIGNEDCHAR
template<> struct field_names<char>			: field_names_none		{};
#endif
#if USE_LONG
template<> struct field_names<ulong>		: field_names_none		{};
template<> struct field_names<long>			: field_names_signed	{};
#endif
#ifdef PLAT_PC
template<> struct field_names<wchar_t>		: field_names_none		{};
#endif

extern const char	*sString[], *sRelString[], *sDec[], *sEnable[], *sOn[], *sPowersOfTwo[], *sCustom[];
#define sFloat	field_names<float>::s
#define sBool	field_names<bool>::s
#define sSigned	field_names_signed::s
#define sHex	field_names_hex::s
#define sNot	field_names<_not<bool>>::s

template<typename T> constexpr field_prefix<T> make_field_prefix(const char *prefix, const T *names) {
	return {prefix, names};
}

struct field_info {
	const char		*name;
	struct field	*fields;
};

//generate array indices
template<int N>	struct _number_string : _number_string<N / 10> { char c; _number_string() : c(char('0' + N % 10)) {} };
template<>		struct _number_string<0> {};
template<int N>	struct number_string : _number_string<N> {};
template<>		struct number_string<0> { char c; number_string() : c('0') {} };

template<int N> struct array_names : array_names<N - 1> {
	char					open;
	number_string<N - 1>	digits;
	char					close, term;
	array_names() : open('['), close(']'), term(0) {}
	static array_names<N> &get()	{ static array_names<N> a; return a; }
};
template<> struct array_names<0> {
	operator const char*() { return (const char*)this; }
};

//-----------------------------------------------------------------------------
//	field
//-----------------------------------------------------------------------------

template<typename T>			static constexpr bool field_is_struct = is_class<T>;
template<typename T>			static constexpr bool field_is_struct<constructable<T>> = field_is_struct<T>;
template<int B, typename T>		static constexpr bool field_is_struct<baseint<B,T>> = false;
template<typename T, size_t N>	static constexpr bool field_is_struct<T[N]> = true;
template<>						static constexpr bool field_is_struct<string> = false;

template<typename T, bool S = field_is_struct<T>> struct field_maker;

template<typename T>    struct fields;
template<typename...TT> struct union_fields		{ static const field *p[]; };

struct field {
	enum MODE {	//stored in offset
	// num != 0
		MODE_NONE		= 0,
		MODE_VALUES		= 1,
		MODE_BITS		= 2,
		MODE_CUSTOM		= 3,
		MODE_PREFIX		= 4,
		MODE_DOT		= 8,
		MODE_CALLBACK	= MODE_CUSTOM | 16,
	// num == 0
		MODE_CALL		= 0,
		MODE_POINTER	= 1,
		MODE_RELPTR		= 2,
		MODE_CUSTOM_PTR	= 3,
	};
	const char	*name;
	uint32		start:15, num:8, offset:4, shift:5;
	const char	*const *values;

	template<typename T> struct mode;

	static constexpr MODE get_mode(const char**)							{ return MODE_NONE; }
	static constexpr MODE get_mode(field*)									{ return MODE_NONE; }
	static constexpr MODE get_mode(field_value*)							{ return MODE_VALUES; }
	static constexpr MODE get_mode(field_bit*)								{ return MODE_BITS; }
	static constexpr MODE get_mode(field_custom*)							{ return MODE_CUSTOM; }
	static constexpr MODE get_mode(field_callback_func)						{ return MODE_CALLBACK; }
	template<typename T> static constexpr MODE get_mode(const field_prefix<T>&)	{ return MODE(get_mode((T*)0) | MODE_PREFIX); }
//	template<typename T> static constexpr MODE get_mode(field_prefix<T>*)	{ return get_mode((T*)0) | MODE_PREFIX; }
	template<typename T> static constexpr MODE get_mode(field_dot<T>*)		{ return get_mode((T*)0) | MODE_DOT; }

	static constexpr field dummy(uint32 num) {
		return field {0, 0, num, 0, 0, sHex};
	}
	static constexpr field make(const char *name, uint32 start, uint32 num, uint32 offset = 0, int shift = 0, const char *const *values = 0) {
		return field {name, start, num, offset, uint32(shift), (const char**)values};
	}
	static constexpr field call(const field *fields, uint32 start, const char *prefix = 0) {
		return field {prefix, start, 0, MODE_CALL, 0, (const char**)fields};
	}
	template<typename V> static constexpr field make(const char *name, uint32 start, uint32 num, V *values) {
		return {name, start, num, uint32(get_mode(values) & 15), uint32(get_mode(values) >> 4), (const char**)values};
	}
	template<typename V> static constexpr field make(const char *name, uint32 start, uint32 num, const V &values) {
		return {name, start, num, uint32(get_mode(values) & 15), uint32(get_mode(values) >> 4), (const char**)&values};
	}
	template<typename B, typename T, typename V> static constexpr field make(const char *name, T B::*p, V *values) {
		return make(name, uint32(BIT_OFFSET(p)), uint32(sizeof(T)), values);
	}
	template<typename B, typename T> static constexpr field make(const char *name, T B::*p, field *values) {
		return make(name, uint32(BIT_OFFSET(p)), 0, values);
	}
	template<typename T> static constexpr field make(const char *name, uint32 start, uint32 num) {
		return make(name, start, num, field_names<T>::s);
	}
	template<typename T> static constexpr field make(const char *name, uint32 start) {
		return field_maker<T>::f(name, start);
	}
	template<typename B, typename T> static constexpr field make(const char *name, T B::*p) {
		return field_maker<T>::f(name, BIT_OFFSET(p));
	}
	template<typename B, typename T, typename T0> static constexpr field make(const char *name, T0 B::*p) {
		return field_maker<T>::f(name, BIT_OFFSET(p));
	}
	template<typename T, T t> static constexpr field make(meta::field<T, t> &&f) {
		return field_maker<T>::f(f.name, BIT_OFFSET(t));
	}
	template<typename T, MODE M = MODE_CALL> static constexpr field call(const char *name, uint32 start, int rel_field = 0) {
		return field {name, start, 0, M, uint32(rel_field), (const char**)(const field*)fields<T>::f};
	}
	template<typename B, typename T> static constexpr field call(const char *name, T B::*p) {
		return call<T>(name, BIT_OFFSET(p));
	}
	template<typename...TT> static constexpr field call_union(const char *name, uint32 start, int select_rel_field) {
		return field {name, start, 0, MODE_CALL, uint32(select_rel_field), (const char**)union_fields<TT...>::p};
	}
	template<typename T> static constexpr field terminator() {
		return field {0, BIT_COUNT<T>};
	}

	constexpr bool		is_terminator()			const	{ return !num && !values; }
	constexpr bool		has_values()			const	{ return values && values[0]; }
	constexpr bool		is_signed()				const	{ return values == sSigned; }
	constexpr int		get_offset()			const	{ return has_values() ? 0 : sign_extend<4>(offset);	}
	constexpr uint64	do_shift(uint64 u)		const	{ return shift & 16 ? u >> (32 - shift) : u << shift; }
	constexpr int64		undo_shift(int64 u)		const	{ return shift & 16 ? u << (32 - shift) : u >> shift; }
	constexpr float		do_scale(uint64 u)		const	{ return shift & 16 ? float(u) / (1 << (32 - shift)) : float(u << shift); }
	constexpr int64		undo_scale(float f)		const	{ return uint64(shift & 16 ? f * (1 << (32 - shift)) : f / (1 << shift)); }
	constexpr int64		undo_scale(uint64 u)	const	{ return shift & 16 ? u << (32 - shift) : (int64)u >> shift; }

	uint64		get_raw_value(uint32 x) const {
		uint64	val = (x >> start) & bits(num);
		return is_signed() ? sign_extend(val, num) : val;
	}
	uint32		set_raw_value(uint32 x, uint64 val) const {
		return (x & ~bits(num, start)) | uint32((min(val, bits(num)) << start));
	}
	const void*	get_raw_ptr(const void *p, uint32 start_offset = 0) const {
		return (const uint8*)p + (start_offset + start) / 8;
	}
	uint64		get_raw_value(const uint32 *p, uint32 start_offset = 0) const {
		uint32	start	= start_offset + this->start;
		uint32	i		= start / 32;
		start	&= 31;
		uint64	val		= p[i] >> start;
		if (start + num > 32)
			val |= uint64(p[i + 1]) << (start + 32);
		val &= bits64(num);
		return is_signed() ? sign_extend(val, num) : val;
	}
	void		set_raw_value(uint32 *p, uint64 val, uint32 start_offset = 0) const {
		uint32	start	= start_offset + this->start;
		uint32	i		= start / 32;
		val		= min(val, bits(num));
		start	&= 31;
		p[i]	= (p[i] & ~bits(num, start)) | (uint32(val) << start);
		if (start + num > 32)
			p[i + 1] = (p[i + 1] & ~bits((start + num - 32))) | uint32(val >> (32 - start));
	}

	uint64		get_value(uint32 x)												const { return do_shift(get_raw_value(x)) + get_offset(); }
	uint64		get_value(const uint32 *p, uint32 start_offset = 0)				const { return do_shift(get_raw_value(p, start_offset)) + get_offset();	}
	float		get_float_value(const uint32 *p, uint32 start_offset = 0)		const { return do_scale(get_raw_value(p, start_offset)) + get_offset();	}
	void		set_value(uint32 x, uint64 val)									const { set_raw_value(x, undo_scale(val - get_offset())); }
	void		set_value(uint32 *p, uint64 val, uint32 start_offset = 0)		const { set_raw_value(p, undo_scale(val - get_offset()), start_offset);	}
	void		set_float_value(uint32 *p, float val, uint32 start_offset = 0)	const { set_raw_value(p, undo_scale(val - get_offset()), start_offset);	}

	const field *get_call()														const { return shift ? *(const field**)values : (const field*)values; }
	const field	*get_companion()												const { return this - sign_extend<5>(shift); }
	uint64		get_companion_value(const uint32 *p, uint32 start_offset = 0)	const { return get_companion()->get_raw_value(p, start_offset); }

	const field *get_call(const uint32 *p, uint32 offset) const {
		return shift
			? ((const field**)values)[get_companion_value(p, offset)]
			: (const field*)values;
	}
	const uint32 *get_ptr(const uint32 *p, uint32 off) const {
		return offset == MODE_POINTER ? (const uint32*)get_raw_value(p, off) : p;
	}
	constexpr uint32 get_offset(uint32 off) const {
		return offset == MODE_POINTER ? 0 : off + start;
	}
	template<typename T> constexpr bool is_type() const {
		return values == (const char**)field_names<T>::s;
	}
};

extern field	float_field[], empty_field[], custom_ptr_field[];

template<typename T> struct field_callback {
    static string_accum& s(string_accum &sa, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
        uint64    v = pf->get_raw_value(p, offset);
        return sa << pf->name << " = " << reinterpret_cast<T&>(v);
    }
};


template<typename T, typename U> inline constexpr int get_field_size(const U&) {
	return BIT_COUNT<T>;
}
template<typename T, int N> inline constexpr int get_field_size(const char *const (&)[N]) {
	return LOG2_CEIL(N);
}
template<typename T> constexpr int get_field_size() {
	return get_field_size<T>(field_names<T>::s);
}

// field
template<typename T> struct field_maker<T, false> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, get_field_size<T>(), field_names<T>::s); }
};
// struct
template<typename T> struct field_maker<T, true> {
	static constexpr field f(const char *name, uint32 start) { return field::call<T>(name, start); }
};

// strings
template<> struct field_maker<const char*, false> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, BIT_COUNT<void*>, 0, 0, sString); }
};
template<> struct field_maker<char*, false> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, BIT_COUNT<void*>, 0, 0, sString); }
};
template<> struct field_maker<string, false> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, BIT_COUNT<void*>, 0, 0, sString); }
};

// fields - array of field
template<typename T>	struct fields			{ static field f[]; };
template<typename T>	struct fields<const T>	: fields<T> {};
template<>				struct fields<_none>	{ static typed_nullptr<field> f; };
template<>				struct fields<void>		: fields<_none> {};

template<typename T, typename B>	struct fields<soft_pointer<T, B> > { static field f[]; };
template<typename T, typename B>	field fields<soft_pointer<T, B> >::f[] = { field::call<T, field::MODE_RELPTR>(0, 0), field::terminator<T>() };

// fields with indices
template<int N, typename B, typename T> constexpr field make_field_idx(const char *name, T B::*p) {
	return field::make<B>(name, p);
}

// fields of tuple
template<typename TL, typename IL> struct fieldsI;
template<typename TL, int... II> struct fieldsI<TL, index_list<II...> >  { static field f[]; };
template<typename TL, int... II> field fieldsI<TL, index_list<II...> >::f[]	= {
	make_field_idx<II>("p", TL_tuple<TL>::template field<II>())...,
	field::terminator<TL_tuple<TL> >(),
};
template<typename TL> struct fields<TL_tuple<TL> > : fieldsI<TL, meta::make_index_list<TL::count>> {};

// fields of as_tuple
template<int N, typename B, typename T> constexpr field make_field_idx_test(const char *name, T B::*p) {
	return field::make<B>(name, p);
}
template<typename T, typename TL, typename IL> struct fieldsIN;
template<typename T, typename TL, int... II> struct fieldsIN<T, TL, index_list<II...> >  { static field f[]; };
template<typename T, typename TL, int... II> field fieldsIN<T, TL, index_list<II...> >::f[]	= {
	make_field_idx_test<II>(fields<T>::f[II].name, TL_tuple<TL>::template field<II>())...,
	field::terminator<TL_tuple<TL> >(),
};
template<typename T, typename TL> struct fields<as_tuple<T,TL> > : fieldsIN<T, TL, meta::make_index_list<TL::count>> {};

// fields of union
template<typename...TT> const field *union_fields<TT...>::p[] = { fields<TT>::f..., };

//value_field - single entry fields
template<typename T> struct value_field				{ static field f[];	};
template<typename T> field value_field<T>::f[]	=	{ field::make(0, 0, BIT_COUNT<T>, field_names<T>::s), field::terminator<T>() };

template<typename T> struct value_field<T*>			{ static field f[];	};
template<typename T> field value_field<T*>::f[] =	{ field::call<T,field::MODE_POINTER>(0, 0), 0 };

template<> struct fields<uint8>		: value_field<uint8>	{};
template<> struct fields<uint16>	: value_field<uint16>	{};
template<> struct fields<uint32>	: value_field<uint32>	{};
template<> struct fields<uint64>	: value_field<uint64>	{};
template<> struct fields<int8>		: value_field<int8>		{};
template<> struct fields<int16>		: value_field<int16>	{};
template<> struct fields<int32>		: value_field<int32>	{};
template<> struct fields<int64>		: value_field<int64>	{};
template<> struct fields<float>		: value_field<float>	{};
template<> struct fields<double>	: value_field<double>	{};
#if USE_LONG
template<> struct fields<ulong>		: value_field<ulong>	{};
template<> struct fields<long>		: value_field<long>		{};
#endif
template<typename T> struct fields<T*> : value_field<T*>	{};

// arrays
template<typename T, int N> struct fields_array {
	field f[N + 1];
	struct B { T t; };
	fields_array() {
		const char *names = array_names<N>::get();
		for (int i = 0; i < N; i++) {
			uintptr_t p	= sizeof(T) * i;
			f[i]		= field::make(names, *(T B::**)(&p));
			names += strlen(names) + 1;
		}
		f[N] = field::terminator<T[N]>();
	}
	operator const field*() const { return f; }
};

template<typename T, int N>	struct fields<T[N]> { static fields_array<T, N> f; };
template<typename T, int N>	struct fields<const T[N]> : fields<T[N]> {};
template<typename T, int N>	fields_array<T, N> fields<T[N]>::f;

// get_field_name
inline constexpr const char *get_field_name(const char *const *p, int i) {
	return p[i];
}
inline constexpr const char *get_field_name(const field_value *p, int i) {
	while (p->name) {
		if (i == p->value)
			return p->name;
		++p;
	}
	return 0;
}
template<typename T> constexpr const char *get_field_name(const field_prefix<T> &p, int i) {
	return get_field_name(p.names, i);
}
template<typename T> constexpr const char *get_field_name(const T &t) {
	return get_field_name(field_names<T>::s, (int)t);
}

template<typename T> inline enable_if_t<is_enum<T>, const char*> to_string(const T& t) {
	return get_field_name(t);
}

// get_field_value
template<typename S, typename T> size_t get_field_valuez(const S &s, T &t, const char *const *names) {
	for (int i = 0; names[i]; i++) {
		if (str(names[i]) == s) {
			t = T(i);
			return strlen(names[i]);
		}
	}
	return 0;
}
template<typename T, int N> size_t get_field_value(const char *s, T &t, const char *(&names)[N]) {
	for (int i = 0; i < N; i++) {
		if (str(names[i]) == s) {
			t = T(i);
			return strlen(names[i]);
		}
	}
	return 0;
}
template<typename T, int N> size_t get_field_value(const char *s, T &t, const field_value (&values)[N]) {
	for (int i = 0; i < N; i++) {
		if (str(values[i].name) == s) {
			t = T(values[i].value);
			return strlen(values[i].name);
		}
	}
	return 0;
}
template<typename T> size_t	get_field_value(const char *s, T &t) {
	return get_field_value(s, t, field_names<T>::s);
}

template<typename T> inline enable_if_t<is_enum<T>, size_t> from_string(const char *s, T &t) {
	return get_field_value(s, t);
}


string_accum&	PutConst(string_accum &a, IDFMT fmt, const char *const *names, uint32 val, uint8 mode = 0);
string_accum&	PutConst(string_accum &a, IDFMT fmt, const field *pf, uint64 val);
string_accum&	PutConst(string_accum &a, IDFMT fmt, const field *pf, const uint32 *p, uint32 offset);

string_accum&	PutFieldName(string_accum &sa, IDFMT fmt, const field *pf, const char *prefix = 0);
string_accum&	PutField(string_accum &sa, IDFMT fmt, const field *pf, uint64 val, const char *prefix = 0);
string_accum&	PutFields(string_accum &sa, IDFMT fmt, const field *pf, uint64 val, const char* separator = "\r\n", uint64 mask = -1, const char *prefix = 0);

string_accum&	PutField(string_accum &sa, IDFMT fmt, const field *pf, const uint32 *p, uint32 offset = 0, const char *prefix = 0);
string_accum&	PutFields(string_accum &sa, IDFMT fmt, const field *pf, const uint32 *p, uint32 offset = 0, const char* separator = "\r\n", const char *prefix = 0);

uint64			GetConst(string_scan &ss, const char *const *names, int bits, uint8 mode = 0);
uint64			GetField(string_scan &ss, const field *pf);
void			GetField(string_scan &ss, const field *pf, uint32 *p, uint32 offset = 0);

field			FieldIndex(const field *pf, int i);
const field*	FieldIndex(const field *pf, int i, const uint32 *&p, uint32 &offset, bool follow_ptr = true);

field			FindField(const field *pf, uint32 bit);
uint32			TotalBits(const field *pf);
uint32			BiggestValue(const field_value *vals);
const field*	Terminator(const field *pf);

template<typename S> const field *_FindField(const field *pf, const string_base<S> &name, uint32 &offset) {
	while (pf->values || pf->num) {
		if (pf->num == 0) {
			if (const field *ret = _FindField((const field*)pf->values, name, offset)) {
				offset += pf->start;
				return ret;
			}
		} else {
			if (name == pf->name)
				return pf;
		}
		pf++;
	}
	return 0;
}

template<typename S> const field *FindField(const field *pf, const string_base<S> &name, uint32 &offset) {
	offset = 0;
	return pf ? _FindField(pf, name, offset) : 0;
}

class FieldPutter {
	IDFMT		format;
	void		*me;

	void	(*vOpen)(void *me, const char *name, uint32 addr);
	void	(*vClose)(void *me);
	void	(*vLine)(void *me, const char *name, const char *value, uint32 addr);
	void	(*vCallback)(void *me, const field *pf, const uint32le *p, uint32 offset, uint32 addr);

	void	Open(string_param name, uint32 addr)					{ vOpen(me, name, addr); }
	void	Close()													{ vClose(me); }
	void	Line(string_param name, const char *value, uint32 addr)	{ vLine(me, name, value, addr); }
	void	Callback(const field *pf, const uint32le *p, uint32 offset, uint32 addr) { vCallback(me, pf, p, offset, addr); }

	auto	FieldName(const field* pf) {
		return FormatIdentifier(pf->name, format, IDTYPE_MEMBER);
	}
public:
	template<typename T> FieldPutter(T *t, IDFMT format) : format(format), me(t),
		vOpen(make_staticfunc2(&T::Open,T)),
		vClose(make_staticfunc2(&T::Close,T)),
		vLine(make_staticfunc2(&T::Line,T)),
		vCallback(make_staticfunc2(&T::Callback,T))
	{}
	template<typename T> FieldPutter(T&& t, IDFMT format) : FieldPutter(&t, format) {}

	void	AddHex(const uint32 *vals, uint32 n, uint32 addr);
	void	AddArray(const field *pf, const uint32le *p, uint32 stride, uint32 n, uint32 addr = 0);
	void	AddField(const field *pf, const uint32 *p, uint32 addr = 0, uint32 offset = 0);
	void	AddFields(const field* pf, const uint32* p, uint32 addr = 0, uint32 offset = 0);

	template<typename T> void	AddFields(const T *p, uint32 addr = 0) {
		return AddFields(fields<T>::f, (uint32le*)p, addr);
	}
	template<typename T> void	AddArray(const T &p) {
		int	x = 0;
		for (auto& i : p) {
			Open(format_string("[%i]", x++), 0);
			AddFields(&i);
			Close();
		}
	}
};

} // namespace iso

//template<typename T> inline iso::enable_if_t<iso::is_enum<T>, size_t> from_string(const char *s, T &t) {
//    return iso::get_field_value(s, t);
//}


#endif // IDENTIFIER_H
