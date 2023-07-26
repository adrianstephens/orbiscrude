#ifndef ASN1_H
#define ASN1_H

#include "base/defs.h"
#include "base/array.h"
#include "base/list.h"
#include "base/strings.h"
#include "stream.h"

namespace iso {

struct mpi;
class DateTime;

namespace ASN1 {

enum TYPE {
	TYPE_EOC				= 0,
	TYPE_BOOLEAN			= 1,
	TYPE_INTEGER			= 2,
	TYPE_BIT_STRING			= 3,
	TYPE_OCTET_STRING		= 4,
	TYPE_NULL				= 5,
	TYPE_OBJECT_ID			= 6,
	TYPE_OBJECT_DESCRIPTOR	= 7,
	TYPE_EXTERNAL			= 8,
	TYPE_REAL				= 9,
	TYPE_ENUMERATED			= 10,
	TYPE_EMBEDDED_PDV		= 11,
	TYPE_UTF8_STRING		= 12,	//any character from a recognized alphabet (including ASCII control characters)
	TYPE_REL_OBJECT_ID		= 13,
	TYPE_SEQUENCE			= 16,
	TYPE_SET				= 17,
	TYPE_NUMERIC_STRING		= 18,	//1, 2, 3, 4, 5, 6, 7, 8, 9, 0, and SPACE
	TYPE_PRINTABLE_STRING	= 19,	//a-z, A-Z, ' () +,-.?:/= and SPACE
	TYPE_T61_STRING			= 20,	//CCITT and T.101 character sets
	TYPE_VIDEOTEX_STRING	= 21,	//CCITT's T.100 and T.101 character sets
	TYPE_IA5_STRING			= 22,	//International ASCII characters (International Alphabet 5)
	TYPE_UTC_TIME			= 23,
	TYPE_GENERALIZED_TIME	= 24,
	TYPE_GRAPHIC_STRING		= 25,	//all registered G sets and SPACE
	TYPE_ISO64_STRING		= 26,	//International ASCII printing character sets
	TYPE_GENERAL_STRING		= 27,	//all registered graphic and character sets plus SPACE and DELETE
	TYPE_UNIVERSAL_STRING	= 28,	//ISO10646 character set
	TYPE_CHARACTER_STRING	= 29,
	TYPE_BMP_STRING			= 30,	//Basic Multilingual Plane of ISO/IEC/ITU 10646-1

	// aliases
	TYPE_TELETEX_STRING		= TYPE_T61_STRING,
	TYPE_VISIBLE_STRING		= TYPE_ISO64_STRING,

	TYPE_PRIMITIVE_TYPE		= 0x1f,
	TYPE_CONSTRUCTED		= 0x20,

	//fake
	TYPE_CHOICE				= 31,
	TYPE_SEQUENCE_OF		= 14,
	TYPE_SET_OF				= 15,
};

enum CLASS {
	// masks
	UNIVERSAL	= 0,
	APPLICATION	= 1,
	CONTEXT		= 2,
	PRIVATE		= 3,
};

struct FLOAT_FLAGS {
	enum {
		BINARY	= 0x80,
		SPECIAL	= 0x40,
		SIGN	= 0x40,
		BASE	= 0x30,
		SCALE	= 0x0c,
		EXPLEN	= 0x03,

		POS_INF	= 0x40, NEG_INF = 0x41,
		BASE2	= 0, BASE8 = 1, BASE16 = 2
	};
	union {
		struct { uint8	explen:2, scale:2, base:2, sign:1; };
		uint8	u;
	};

	FLOAT_FLAGS(uint8 _u) : u(_u)		{}
	FLOAT_FLAGS(iord f);
	bool	binary()		const	{ return u & BINARY; }
	bool	special()		const	{ return u & SPECIAL; }
	double	special_value() const	{ return iord::inf(u & 1).f(); }
	operator uint8()		const	{ return u; }
};

template<TYPE T> struct TYPE_COMPATIBILITY { static const uint32 value = 1 << T; };
struct TYPE_COMPATIBILITY_STRINGS	{ static const uint32 value = (1<<TYPE_OCTET_STRING)|(1<<TYPE_OBJECT_DESCRIPTOR)|(1<<TYPE_UTF8_STRING)|(1<<TYPE_NUMERIC_STRING)|(1<<TYPE_PRINTABLE_STRING)|(1<<TYPE_T61_STRING)|(1<<TYPE_VIDEOTEX_STRING)|(1<<TYPE_IA5_STRING)|(1<<TYPE_GRAPHIC_STRING)|(1<<TYPE_ISO64_STRING)|(1<<TYPE_GENERAL_STRING)|(1<<TYPE_UNIVERSAL_STRING)|(1<<TYPE_CHARACTER_STRING)|(1<<TYPE_BMP_STRING); };

template<> struct TYPE_COMPATIBILITY<TYPE_EOC				> { static const uint32 value = ~0; };
template<> struct TYPE_COMPATIBILITY<TYPE_BOOLEAN			> { static const uint32 value = (1<<TYPE_BOOLEAN)|(1<<TYPE_INTEGER); };
template<> struct TYPE_COMPATIBILITY<TYPE_INTEGER			> { static const uint32 value = (1<<TYPE_INTEGER)|(1<<TYPE_ENUMERATED); };
template<> struct TYPE_COMPATIBILITY<TYPE_BIT_STRING		> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_OCTET_STRING		> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_OBJECT_DESCRIPTOR	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_ENUMERATED		> { static const uint32 value = (1<<TYPE_ENUMERATED)|(1<<TYPE_INTEGER); };
template<> struct TYPE_COMPATIBILITY<TYPE_UTF8_STRING		> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_NUMERIC_STRING	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_PRINTABLE_STRING	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_T61_STRING		> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_VIDEOTEX_STRING	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_IA5_STRING		> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_UTC_TIME			> { static const uint32 value = (1<<TYPE_UTC_TIME)|(1<<TYPE_GENERALIZED_TIME); };
template<> struct TYPE_COMPATIBILITY<TYPE_GENERALIZED_TIME	> : TYPE_COMPATIBILITY<TYPE_UTC_TIME> {};
template<> struct TYPE_COMPATIBILITY<TYPE_GRAPHIC_STRING	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_ISO64_STRING		> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_GENERAL_STRING	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_UNIVERSAL_STRING	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_CHARACTER_STRING	> : TYPE_COMPATIBILITY_STRINGS {};
template<> struct TYPE_COMPATIBILITY<TYPE_BMP_STRING		> : TYPE_COMPATIBILITY_STRINGS {};

//-----------------------------------------------------------------------------
// OID
//-----------------------------------------------------------------------------

struct OID : hierarchy<OID> {
	int			id;
	const char	*name;

	OID*	get_child(int i);
	OID*	get_child_make(int i);
	void	attach_check(OID *t);

	OID(int _id) : id(_id), name(0)			{}
	OID(const char *_name = 0) : id(-1), name(_name)	{}

	bool operator==(int i) const { return id == i; }
};

extern OID	*OID_root();

template<typename... TT> inline const OID *find_objectid(OID *o, int id0, const TT&... tt) {
	return find_objectid(o->get_child(id0), tt...);
}
inline const OID *find_objectid(OID *o) {
	return o;
}
template<typename... TT> inline const OID *find_objectid(const TT&... tt) {
	return find_objectid(OID_root(), tt...);
}

//-----------------------------------------------------------------------------
// Values
//-----------------------------------------------------------------------------

struct Value {
	uint32	type:6, length:26;
	uint32	tag;
	union {
		uint64		u;
		double		f;
		uint8		buffer[8];
		void		*p;
	};

	static CLASS		get_class(uint32 tag)			{ return CLASS(tag >> 30); }

	static uint32		read_tag(istream_ref file, bool &constructed);
	static uint64		read_len(istream_ref file);
	static void			write_tag(ostream_ref file, uint32 tag, bool constructed = false);
	static void			write_len(ostream_ref file, uint64 len);

	static bool			write_bitstring(ostream_ref file, uint32 len, const void *buffer) {
		uint32	nbytes = (len + 7) >> 3;
		write_len(file, nbytes + 1);
		file.putc(-len & 7);
		return file.writebuff(buffer, nbytes) == nbytes;
	}

	Value() { clear(*this); }

	uint32				byte_length()			const	{ return type == TYPE_BIT_STRING ? (length + 7) / 8 : length; }
	bool				is_compatible(uint32 f) const	{ return !!(f & (1 << (type & 31))); }
	bool				is_constructed()		const	{ return !!(type & TYPE_CONSTRUCTED); }
	CLASS				get_class()				const	{ return get_class(tag); }
	arbitrary_const_ptr	get_ptr()				const	{ return byte_length() <= 8 ? buffer : p; }
	const_memory_block	get_buffer()			const	{
		uint32	len = byte_length();
		return const_memory_block(len <= 8 ? buffer : p, len);
	}
	memory_block		alloc_buffer() {
		uint32	len = byte_length();
		return memory_block(len > 8 ? p = malloc(len) : buffer, len);
	}
	memory_block		set_len(uint32 _length)			{ if (byte_length() > 8) free(p); length = _length; return alloc_buffer(); }
	memory_block		set(TYPE _type, uint32 _length)	{ type = tag = _type; return set_len(_length); }

	void*		extend_buffer(uint32 len);

	bool		read_bitstring(istream_ref file, uint64 len);
	bool		read_octetstring(istream_ref file, uint64 len);
	void		read_header(istream_ref file);
	bool		read_contents(istream_ref file);
	bool		read(istream_ref file);

	bool		write_contents(ostream_ref file) const;
	bool		write(ostream_ref file) const {
		write_tag(file, tag, is_constructed());
		return write_contents(file);
	}
	bool	write(ostream_ref file, uint32 tag, bool constructed = false) const {
		write_tag(file, tag ? tag : type & 0x1f, constructed);
		return write_contents(file);
	}

	double		get_float() const;
	void		set_float(double x);

	const OID*	get_objectid() 	const;
	void		set_objectid(const OID *x);
};

struct RawValue : Value {};

template<TYPE T> struct ValueT : Value {
	ValueT() { type = T; }
	void operator=(const Value &v)	{ Value::operator=(v); }
};

template<> struct ValueT<TYPE_REAL> : Value {
	ValueT() { type = TYPE_REAL; }
	void operator=(const Value &v)	{ Value::operator=(v); }
	void operator=(double x)		{ set_float(x); }
	operator double()	const		{ return get_float(); }
};

template<> struct ValueT<TYPE_OBJECT_ID> : Value {
	ValueT() { type = TYPE_OBJECT_ID; }
	void operator=(const Value &v)	{ Value::operator=(v); }
	void operator=(const OID *x)	{ set_objectid(x); }
	operator const OID*()	const	{ return get_objectid(); }
};

template<> struct ValueT<TYPE_OCTET_STRING> : Value {
	ValueT() { type = TYPE_OCTET_STRING; }
	void operator=(const Value &v)	{ Value::operator=(v); }
	void operator=(const malloc_block &x)		{ memcpy(set(TYPE_OCTET_STRING, x.size32()), x, x.size()); }
	operator const const_memory_block()	const	{ return get_buffer(); }
};
typedef	ValueT<TYPE_NULL>				Null;
typedef	ValueT<TYPE_BOOLEAN>			Boolean;
typedef	ValueT<TYPE_INTEGER>			Integer;
typedef	ValueT<TYPE_BIT_STRING>			BitString;
typedef	ValueT<TYPE_OCTET_STRING>		OctetString;
typedef	ValueT<TYPE_OBJECT_ID>			ObjectID;
typedef	ValueT<TYPE_REAL>				Real;
typedef	ValueT<TYPE_ENUMERATED>			Enumerated;
typedef	ValueT<TYPE_GENERALIZED_TIME>	GeneralizedTime;
typedef	ValueT<TYPE_GENERAL_STRING>		GeneralString;
typedef	ValueT<TYPE_IA5_STRING>			IA5String;
typedef	ValueT<TYPE_PRINTABLE_STRING>	Printablestring;
typedef	ValueT<TYPE_T61_STRING>			T61String;
typedef	ValueT<TYPE_UNIVERSAL_STRING>	UniversalString;
typedef	ValueT<TYPE_CHARACTER_STRING>	String;
typedef	ValueT<TYPE_UTC_TIME>			UTCTime;
typedef	ValueT<TYPE_UTF8_STRING>		UTF8String;
typedef	ValueT<TYPE_VISIBLE_STRING>		VisibleString;
typedef	ValueT<TYPE_REL_OBJECT_ID>		RelObjectID;
typedef	ValueT<TYPE_BMP_STRING>			BmpString;

template<typename T> struct SetOf : dynamic_array<T> {};
template<typename T> struct SeqOf : dynamic_array<T> {};

template<typename T, int TAG, CLASS C = CONTEXT> struct ImplicitTag : T_inheritable<T>::type {};
template<typename T, int TAG, CLASS C = CONTEXT> struct ExplicitTag : T_inheritable<T>::type {};

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct TypeBase {
	uint32		type:5, raw:1, size:26;
	bool		(*_read)(const TypeBase *type, istream_ref file, arbitrary_ptr p, Value &v);
	bool		(*_write)(const TypeBase *type, ostream_ref file, arbitrary_const_ptr p, uint32 tag);

	template<typename T> TypeBase(T*, TYPE _type, uint32 _size) : type(_type), raw(_size == 0), size(_size == 0 ? sizeof(Value) : _size)
		, _read(make_staticfunc(&T::read))
		, _write(make_staticfunc(&T::write))
	{}
};

struct Type : TypeBase {
	bool	read(istream_ref file, void *p, Value &v, bool exptag = false) const {
		if (v.get_class()) {
			if (exptag && v.is_constructed()) {
				streamptr end = file.tell() + v.length;
				Value	v2;
				v2.read_header(file);
				return read(file, p, v2) && file.tell() == end;
			}
			v.type = type | (v.type & TYPE_CONSTRUCTED);
		}
		return _read(this, file, p, v);
	}
	bool	write(ostream_ref file, const void *p, uint32 tag = 0) const {
		if (!(tag >> 30))
			tag = 0;
		return _write(this, file, p, tag);
	}
};

template<typename T> struct TypeDef;
template<typename T> const Type* get_type() { static TypeDef<T> t; return static_cast<const Type*>(static_cast<const TypeBase*>(&t)); }

template<> struct TypeDef<Value> : TypeBase	{
	TypeDef(): TypeBase(this, TYPE_EOC, 0) {}
	bool		read(istream_ref file, Value *p, Value &v) const {
		*p = v;
		return p->read_contents(file);
	}
	bool		write(ostream_ref file, const Value *p, uint32 tag) const {
		return !p->type || p->write(file, tag);
	}
};

template<> struct TypeDef<RawValue> : TypeBase	{
	TypeDef(): TypeBase(this, TYPE_EOC, 0) {}
	bool		read(istream_ref file, RawValue *p, Value &v) const {
		*(Value*)p = v;
		return check_readbuff(file, p->alloc_buffer(), p->length);
	}
	bool		write(ostream_ref file, const RawValue *p, uint32 tag) const {
		if (!p->type)
			return true;
		Value::write_tag(file, tag ? tag : p->type & 0x1f, p->is_constructed());
		Value::write_len(file, p->length);
		return file.write(p->get_buffer());
	}
};

template<TYPE T> struct TypeDef<ValueT<T> >	: TypeBase {
	TypeDef(): TypeBase(this, T, 0) {}
	bool		read(istream_ref file, Value *p, Value &v) const {
		if (!v.is_compatible(TYPE_COMPATIBILITY<T>::value))
			return false;
		*p = v;
		return p->read_contents(file);
	}
	bool		write(ostream_ref file, const Value *p, uint32 tag) const {
		if (!p->is_compatible(TYPE_COMPATIBILITY<T>::value))
			return false;
		return p->write(file, tag);
	}
};

struct TypeArray : TypeBase {
	const Type	*subtype;
	void		(*create)(void*);

	TypeArray(TYPE type, uint32 size, const Type *_subtype, void(*_create)(void*)): TypeBase(this, type, size), subtype(_subtype), create(_create) {}
	template<typename T> TypeArray(TYPE type, uint32 size, T*): TypeBase(this, type, size), subtype(get_type<T>()), create(&iso::construct<T>) {}
	bool	read(istream_ref file, void *p, Value &v)			const;
	bool	write(ostream_ref file, const void *p, uint32 tag)	const;

	size_t				count(const void *p) const;
	const_memory_block	get_element(const void *p, int i) const;
	memory_block		get_element(void *p, int i) const;
};

template<typename T, int N> struct TypeDef<T[N]> : TypeArray {
	TypeDef(): TypeArray(TYPE_SEQUENCE_OF, sizeof(T) * N, (T*)0) {}
};
template<typename T> struct TypeDef<dynamic_array<T> > : TypeArray {
	TypeDef(): TypeArray(TYPE_SEQUENCE_OF, sizeof(dynamic_array<T>), (T*)0) {}
};
template<typename T> struct TypeDef<SeqOf<T> > : TypeDef<dynamic_array<T> > {};

template<typename T> struct TypeDef<SetOf<T> > : TypeArray {
	TypeDef(): TypeArray(TYPE_SET_OF, sizeof(SetOf<T>), (T*)0) {}
};

template<typename T> struct TypeDef<optional<T> > : TypeDef<T> {
	TypeDef() {
		TypeBase::size		= sizeof(optional<T>);
		TypeBase::_read		= make_staticfunc(&TypeDef::read);
		TypeBase::_write	= make_staticfunc(&TypeDef::write);
	}
	bool	read(istream_ref file, optional<T> *p, Value &v) const {
		return TypeDef<T>::read(file, &put(*p), v);
	}
	bool	write(ostream_ref file, const optional<T> *p, uint32 tag) const {
		if (!p->exists())
			return true;
		auto	v = get(*p);
		return TypeDef<T>::write(file, &v, tag);
	}
};

template<> struct TypeDef<optional<Value> > : TypeDef<Value> {};
template<typename T, int TAG, CLASS C> struct TypeDef<ImplicitTag<T, TAG, C> > : TypeDef<T> {};
template<typename T, int TAG, CLASS C> struct TypeDef<ExplicitTag<T, TAG, C> > : TypeDef<T> {};

template<typename T> struct TypeInt : TypeBase {
	TypeInt(): TypeBase(this, TYPE_INTEGER, sizeof(T)) {}
	bool		read(istream_ref file, T *p, Value &v)	const {
		if (v.type != TYPE_BOOLEAN && v.type != TYPE_INTEGER && v.type != TYPE_ENUMERATED)
			return false;
		v.read_contents(file);
		*p = read_bytes<bigendian<T>>(v.get_buffer());
		return true;
	}
	bool		write(ostream_ref file, const T *p, uint32 tag) const {
		uint32	len	= (highest_set_index(*p) + 9) / 8;
		Value	v;
		write_bytes<bigendian<T>>(v.set(TYPE_INTEGER, len), *p);
		return v.write(file, tag);
	}
};
template<> struct TypeDef<uint8>	: TypeInt<uint8>	{};
template<> struct TypeDef<uint16>	: TypeInt<uint16>	{};
template<> struct TypeDef<uint32>	: TypeInt<uint32>	{};
template<> struct TypeDef<uint64>	: TypeInt<uint64>	{};
template<> struct TypeDef<int8>		: TypeInt<int8>		{};
template<> struct TypeDef<int16>	: TypeInt<int16>	{};
template<> struct TypeDef<int32>	: TypeInt<int32>	{};
template<> struct TypeDef<int64>	: TypeInt<int64>	{};

template<typename T> struct TypeFloat : TypeBase {
	TypeFloat(): TypeBase(this, TYPE_REAL, sizeof(T)) {}
	bool		read(istream_ref file, T *p, Value &v)	const {
		v.read_contents(file);
		if (v.type == TYPE_REAL)
			*p = v.get_float();
		else if (v.type == TYPE_INTEGER)
			*p = read_bytes<uint64be>(v.get_buffer());
		else
			return false;
		return true;
	}
	bool		write(ostream_ref file, const T *p, uint32 tag) const {
		Value	v;
		v.set(TYPE_REAL, sizeof(T));
		v.set_float(*p);
		return v.write(file, tag);
	}
};
template<> struct TypeDef<float>	: TypeFloat<float>	{};
template<> struct TypeDef<double>	: TypeFloat<double>	{};

template<> struct TypeDef<bool> : TypeBase {
	TypeDef() : TypeBase(this, TYPE_BOOLEAN, sizeof(bool)) {}
	bool		read(istream_ref file, bool *p, Value &v)	const {
		if (v.type != TYPE_BOOLEAN && v.type != TYPE_INTEGER && v.type != TYPE_ENUMERATED)
			return false;
		v.read_contents(file);
		*p = !!v.u;
		return true;
	}
	bool		write(ostream_ref file, const bool *p, uint32 tag) const {
		Value::write_tag(file, tag ? tag : TYPE_BOOLEAN);
		Value::write_len(file, 1);
		file.putc(*p ? 0xff : 0);
		return true;
	}
};

template<> struct TypeDef<string>	: TypeBase {
	TypeDef() : TypeBase(this, TYPE_IA5_STRING, sizeof(string)) {}
	bool		read(istream_ref file, string *p, Value &v)	const {
		if (!v.is_compatible(TYPE_COMPATIBILITY_STRINGS::value))
			return false;
		v.read_contents(file);
		*p = count_string(v.get_buffer(), v.length);
		return true;
	}
	bool		write(ostream_ref file, const string *p, uint32 tag) const {
		size_t	len = p->length();
		Value::write_tag(file, tag ? tag : TYPE_IA5_STRING);
		Value::write_len(file, len);
		return file.writebuff(*p, len) == len;
	}
};

template<typename T> struct TypeDef<dynamic_bitarray<T> > : TypeBase {
	TypeDef() : TypeBase(this, TYPE_BIT_STRING, sizeof(bool)) {}
	bool		read(istream_ref file, dynamic_bitarray<T> *p, Value &v)	const {
		v.read_contents(file);
		p->resize(v.length);
		v.get_buffer().copy_to(p->raw().begin());
		return true;
	}
	bool		write(ostream_ref file, const dynamic_bitarray<T> *p, uint32 tag) const {
		Value::write_tag(file, tag ? tag : TYPE_BIT_STRING);
		return Value::write_bitstring(file, p->size(), p->begin().ptr());
	}
};

template<> struct TypeDef<DateTime>	: TypeBase {
	TypeDef();
	bool		read(istream_ref file, DateTime *p, Value &v)	const;
	bool		write(ostream_ref file, const DateTime *p, uint32 tag) const;
};

template<> struct TypeDef<mpi>	: TypeBase {
	TypeDef();
	bool		read(istream_ref file, mpi *p, Value &v)	const;
	bool		write(ostream_ref file, const mpi *p, uint32 tag) const;
};

//-----------------------------------------------------------------------------
// Composite Type definitions
//-----------------------------------------------------------------------------

struct Field {
	enum {
		OPT					= 1 << 0,	// optional

		IMPTAG				= 1 << 1,	// IMPLICIT tagging
		EXPTAG				= 2 << 1,	// EXPLICIT tagging, inner tag from underlying type

		// ANY DEFINED BY - the 'item' field points to an ASN1_ADB structure which contains a table of values to decode the relevant type
		ADB_OID				= 1 << 3,
		ADB_INT				= 2 << 3,

		NDEF				= 1 << 5,	// use indefinite length constructed encoding
	};
	uint32		offset:26, flags:6;
	uint32		tag;
	const char	*name;
	const Type	*type;

	template<typename T> struct FlagsTagDef	{ static const int flags = 0, tag = 0; };

	template<typename S, typename T> Field(T S::*f, const char *n) : offset(uint32(intptr_t(&((S*)0->*f)))), flags(FlagsTagDef<T>::flags), tag(FlagsTagDef<T>::tag), name(n), type(get_type<T>()) {
		if (!(flags & (IMPTAG|EXPTAG)))
			tag = type->type;
	}
	Field(const TypeBase *_type, uint32 _flags, uint32 _tag) : offset(0), flags(_flags), tag(_tag), name(0), type(static_cast<const Type*>(_type)) {}

	const Field	*Find(const Value &v) const;
};

template<typename T> struct Field::FlagsTagDef<optional<T> >								: FlagsTagDef<T> { static const int flags = FlagsTagDef<T>::flags | OPT; };
template<typename T, int TAG, CLASS C> struct Field::FlagsTagDef<ImplicitTag<T, TAG, C> >	: FlagsTagDef<T> { static const int flags = FlagsTagDef<T>::flags | IMPTAG, tag = TAG | (C << 30); };
template<typename T, int TAG, CLASS C> struct Field::FlagsTagDef<ExplicitTag<T, TAG, C> >	: FlagsTagDef<T> { static const int flags = FlagsTagDef<T>::flags | EXPTAG, tag = TAG | (C << 30); };

template<int N> struct TypeComposite;

template<int N> struct Fields : Field {
	Fields<N - 1>	rest;
	template<typename F, typename... FF> Fields(F f, const char *n, FF... ff) : Field(f, n), rest(ff...) {}
	template<int M, typename... FF> Fields(const TypeComposite<M> &f, FF... ff) : Field(&f, 0, 0), rest(ff...) {}
};
template <> struct Fields<1> : Field {
	Field	terminator;
	template<typename F> Fields(F f, const char *n) : Field(f, n), terminator(0, 0, 0) {}
	template<int M> Fields(const TypeComposite<M> &f) : Field(f, 0), terminator(0, 0, 0) {}
};

#if 0
struct TypeSeq : TypeBase, trailing_array<TypeSeq, Field> {
	const char		*name;
	template<typename... FF> TypeSeq(uint32 size, const char *_name, FF... ff) : TypeBase(this, TYPE_SEQUENCE, size), name(_name), fields(ff...) {}
	bool		read(istream_ref file, void *p, Value &v)	const;
	bool		write(ostream_ref file, const void *p, uint32 tag) const;
};

struct TypeSet : TypeBase, trailing_array<TypeSet, Field> {
	const char		*name;
	template<typename... FF> TypeSet(uint32 size, const char *_name, FF... ff): TypeBase(this, TYPE_SET, size), name(_name), fields(ff...) {}
	bool		read(istream_ref file, void *p, Value &v)	const;
	bool		write(ostream_ref file, const void *p, uint32 tag) const;
};
#else
struct TypeCompositeBase : TypeBase, trailing_array<TypeCompositeBase, Field> {
	const char		*name;
	TypeCompositeBase(TYPE type, uint32 size, const char *_name) : TypeBase(this, type, size), name(_name) {}
	bool		read(istream_ref file, void *p, Value &v)	const;
	bool		write(ostream_ref file, const void *p, uint32 tag) const;
	const Field	*find(uint32 tag) const {
		for (const Field *f = begin(); f->name; ++f)
			if (f->tag == tag)
				return f;
		return 0;
	}
	int			count() const {
		const Field *f = begin();
		while (f->name)
			++f;
		return int(f - begin());
	}
};

template<int N> struct TypeComposite : TypeCompositeBase {
	Fields<N>		fields;
	template<typename... FF> TypeComposite(TYPE type, uint32 size, const char *name, FF... ff): TypeCompositeBase(type, size, name), fields(ff...) {}
};
#endif

#define ASN1_TYPEDEF(T,D)	template<> struct TypeDef<T> : TypeDef<D> {};
#define ASN1_COMP(T,S,N)	template<> struct TypeDef<S> : TypeComposite<N> { typedef S SS; TypeDef() : TypeComposite<N>(T, sizeof(S), #S
#define ASN1_STRUCT(S,N)	ASN1_COMP(TYPE_SEQUENCE,S,N)
#define ASN1_SET(S,N)		ASN1_COMP(TYPE_SET,S,N)
#define ASN1_CHOICE(S,N)	ASN1_COMP(TYPE_CHOICE,S,N)
#define ASN1_ENDSTRUCT		) {} };
#define ASN1_FIELD(F)		&SS::F, #F

#define ASN1_SUB(T,N)		TypeComposite<N>(T, 0, 0
#define ASN1_ENDSUB			)
#define ASN1_SUBSTRUCT(N)	ASN1_SUB(TYPE_SEQUENCE)
#define ASN1_SUBCHOICE(N)	ASN1_SUB(TYPE_CHOICE)


//-----------------------------------------------------------------------------
// OID
//-----------------------------------------------------------------------------

const OID *find_objectid(const const_memory_block &b);

struct ResolvedObjectID {
	const OID	*oid;
	ResolvedObjectID() : oid(0)	{}
	ResolvedObjectID(const OID *_oid) : oid(_oid) {}
	bool	set(const OID *_oid) {
		return !!(oid = _oid);
	}
	bool	operator==(const OID *_oid) const {
		return oid == _oid;
	}
	bool	operator==(const char *s) const {
		return oid && strcmp(oid->name, s) == 0;
	}
	operator const char*() const {
		return oid ? oid->name : 0;
	}
};

template<> struct TypeDef<ResolvedObjectID>	: TypeBase {
	TypeDef() : TypeBase(this, TYPE_OBJECT_ID, sizeof(ResolvedObjectID)) {}
	bool		read(istream_ref file, ResolvedObjectID *p, Value &v)	const {
		return v.read_contents(file) && p->set(v.get_objectid());
	}
	bool		write(ostream_ref file, const ResolvedObjectID *p, uint32 tag) const {
		Value	v;
		v.set_objectid(p->oid);
		return v.write(file, tag);
	}
};

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

bool Read(istream_ref file, void *p, const Type *type);
bool Write(ostream_ref file, const void *p, const Type *type);

template<typename R, typename T> bool Read(R &&file, T &t)			{ return Read(file, &t, get_type<T>()); }
template<typename W, typename T> bool Write(W &&file, const T &t)	{ return Write(file, &t, get_type<T>()); }
template<typename T> bool Read(RawValue &v, T &t)					{ return get_type<T>()->read(unconst(memory_reader(v.get_buffer())), &t, v); }

} // namespace ANS1;

// namespace iso;

size_t			to_string(char *s, const ASN1::Value &v);
string_accum&	operator<<(string_accum &a, const ASN1::Value &v);
template<> struct optional<ASN1::Value> : ASN1::Value {};

 } //namespace iso1

#endif
