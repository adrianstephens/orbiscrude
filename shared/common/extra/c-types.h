#ifndef C_TYPES_H
#define C_TYPES_H

#include "base/defs.h"
#include "base/strings.h"
#include "base/array.h"
#include "base/hash.h"
#include "crc32.h"
#include "thread.h"

namespace iso {

class C_type_composite;

class C_type {
public:
	enum TYPE : uint8 {
		UNKNOWN,
		INT,
		FLOAT,
		STRUCT,
		UNION,
		ARRAY,
		POINTER,
		FUNCTION,

		NAMESPACE,
		TEMPLATE,
		TEMPLATE_PARAM,

		CUSTOM,
	};
	enum FLAGS : uint8 {
		NONE		= 0,
		FLAG0		= 1<<0, FLAG1 = 1<<1, FLAG2 = 1<<2, FLAG3 = 1<<3,
		FLAG4		= 1<<4,
		USER_FLAGS	= FLAG0 | FLAG1 | FLAG2 | FLAG3 | FLAG4,
		TEMPLATED	= 1<<5,
		STATIC		= 1<<6,
		PACKED		= 1<<7,
	};
	friend constexpr FLAGS	operator-(FLAGS a, FLAGS b)		{ return FLAGS(uint8(a) & ~uint8(b)); }
	friend constexpr FLAGS	operator+(FLAGS a, FLAGS b)		{ return FLAGS(uint8(a) | uint8(b)); }
	friend constexpr FLAGS	operator|(FLAGS a, FLAGS b)		{ return FLAGS(uint8(a) | uint8(b)); }
	friend constexpr FLAGS	operator&(FLAGS a, FLAGS b)		{ return FLAGS(uint8(a) & uint8(b)); }
	friend constexpr FLAGS	operator*(FLAGS a, bool b)		{ return b ? a : NONE; }
	friend constexpr FLAGS&	operator|=(FLAGS &a, FLAGS b)	{ return a = a | b; }

	TYPE	type;
	FLAGS	flags;
	constexpr C_type(TYPE t, FLAGS f = NONE) : type(t), flags(f) {}
	constexpr bool			packed()	const	{ return !!(flags & PACKED); }
	constexpr bool			templated()	const	{ return !!(flags & TEMPLATED); }
	constexpr size_t		size()		const;
	constexpr uint32		size32()	const	{ return uint32(size()); }
	constexpr uint32		alignment()	const;
	constexpr const C_type *subtype()	const;
	constexpr const char*	tag()		const;

	constexpr const C_type_composite*composite()	const	{ return type == STRUCT || type == UNION || type == NAMESPACE ? (const C_type_composite*)this : nullptr; }
	constexpr const C_type	*skip_template()		const	{ return type == TEMPLATE ? subtype() : this; }
};

struct C_arg {
	string			id;
	const C_type*	type;
	C_arg()			{}
	C_arg(string_ref id, const C_type *type) : id(move(id)), type(type) {}
	void	init(string_ref _id, const C_type *_type) {
		id		= move(_id);
		type	= _type;
	}
};

struct C_element : C_arg {
	enum ACCESS {PUBLIC = 0, PROTECTED = 1, PRIVATE = 2};
	union {
		uint32	u;
		struct {
			uint32	offset:27, access:2, shift:3;
		};
	};
	C_element()		{}
	C_element(string_ref id, const C_type *type, size_t offset = ~size_t(0), ACCESS access = PUBLIC) : C_arg(move(id), type), offset(uint32(offset)), access(access), shift(0) {}

	void	init(string_ref _id, const C_type *_type, size_t _offset = ~size_t(0), ACCESS _access = PUBLIC) {
		C_arg::init(move(_id), _type);
		offset		= uint32(_offset);
		access		= _access;
		shift		= 0;
	}
	constexpr size_t	size()		const	{ return type->size(); }
	constexpr bool		is_static()	const	{ return offset == ((1 << 27) - 1); }
};

class C_type_withsubtype : public C_type {
public:
	uint8			_unused[2] = {0, 0};
	uint32			subsize;
	const C_type*	subtype;
	constexpr C_type_withsubtype(TYPE t, const C_type *subtype, FLAGS f = NONE) : C_type(t, f | (subtype ? (subtype->flags & TEMPLATED) : NONE)), subsize(subtype ? (uint32)subtype->size() : 0), subtype(subtype) {}
};

class C_type_withargs : public C_type_withsubtype {
public:
	dynamic_array<C_arg>	args;
	constexpr C_type_withargs(TYPE t, const C_type *subtype, FLAGS f = NONE) : C_type_withsubtype(t, subtype, f) {}
	size_t			num_args()	const	{ return args.size(); }
	C_arg*			add(string_ref id, const C_type* type) {
		if (type)
			flags |= type->flags & TEMPLATED;
		return &args.emplace_back(move(id), type);
	}
};

class C_type_int : public C_type {
//protected:
public:
	static const FLAGS SIGN = FLAG0, ENUM = FLAG1, CHAR = FLAG2, NORM = FLAG3, NOZ = FLAG4;
	uint8		nbits;
	constexpr C_type_int(uint8 nbits, FLAGS f) : C_type(INT, f), nbits(nbits) {}
public:
	constexpr C_type_int(uint8 nbits, bool sign) : C_type(INT, sign ? SIGN : NONE), nbits(nbits) {}
	constexpr uint32	num_bits()	const	{ return nbits;	}
	constexpr size_t	size()		const	{ return (nbits + 7) / 8;	}
	constexpr uint32	alignment()	const	{ return nbits <= 8 ? 1 : nbits <= 16 ? 2 : nbits <= 32 ? 4 : 8; }
	constexpr bool		sign()		const	{ return !!(flags & SIGN);	}
	constexpr bool		is_enum()	const	{ return !!(flags & ENUM);	}
	constexpr bool		no_zero()	const	{ return !!(flags & NOZ);	}
	constexpr uint64	scale()		const	{ return flags & NORM ? bit64(nbits - int(!!(flags & SIGN))) - int(!(flags & NOZ)) : 1; }

	void	set_size(uint8 _nbits, bool sign)	{ nbits = _nbits; flags = (flags - SIGN) | (sign ? SIGN : NONE); }

	template<int B, FLAGS F> static C_type_int *get() {
		static C_type_int type(B, F | C_type::STATIC);
		return &type;
	}
	template<typename T> static C_type_int *get() {
		typedef num_traits<T> N;
		return get<N::bits, SIGN * N::is_signed + CHAR * T_ischar<T>::value>();
	}
	static C_type_int *get(int B, FLAGS F) {
		switch (F) {
			case SIGN:
				switch (B) {
					case 1: return get<int8>();
					case 2: return get<int16>();
					case 4: return get<int32>();
					case 8: return get<int64>();
				}
				break;
			case NONE:
				switch (B) {
					case 1: return get<uint8>();
					case 2: return get<uint16>();
					case 4: return get<uint32>();
					case 8: return get<uint64>();
				}
				break;
		}
		return new C_type_int(B, F);
	}
};

struct C_enum {
	string		id;
	int64		value;
	C_enum() {}
	C_enum(string_ref id, int64 value) : id(move(id)), value(value) {}
};

class C_type_enum : public C_type_int, public dynamic_array<C_enum> {
public:
	C_type_enum(uint8 nbits, uint16 count = 0, FLAGS f = NONE) : C_type_int(nbits, f | ENUM), dynamic_array<C_enum>(count) {}
	C_type_enum(const C_type_int &i, uint16 count = 0) : C_type_int(i.nbits, i.flags | ENUM), dynamic_array<C_enum>(count) {}
};

class C_type_float : public C_type {
	static const FLAGS SIGN = FLAG0;
	uint8		nbits, exp;
	constexpr C_type_float(uint8 nbits, uint8 exp, FLAGS f) : C_type(FLOAT, f), nbits(nbits), exp(exp) {}
public:
	static uint32		usual_exp_bits(uint32 nbits) { return nbits <= 64 ? log2(nbits) * 3 - 7 : log2(nbits) * 4 - 13; }

	constexpr C_type_float(uint8 nbits, uint8 exp, bool sign = true) : C_type(FLOAT, sign ? SIGN : NONE), nbits(nbits), exp(exp) {}
	constexpr C_type_float(uint8 nbits) : C_type(FLOAT, SIGN), nbits(nbits), exp(usual_exp_bits(nbits)) {}
	constexpr uint32	num_bits()		const	{ return nbits;	}
	constexpr size_t	exp_bits()		const	{ return exp;	}
	constexpr size_t	size()			const	{ return (nbits + 7) / 8;	}
	constexpr uint32	alignment()		const	{ return nbits <= 8 ? 1 : nbits <= 16 ? 2 : nbits <= 32 ? 4 : 8; }
	constexpr bool		sign()			const	{ return !!(flags & SIGN);	}
	template<int B, int E, bool S> static C_type_float *get() {
		static C_type_float	type(B, E, SIGN * S + STATIC);
		return &type;
	}
	template<typename T> static C_type_float *get() {
		typedef num_traits<T> N;
		return get<N::bits, N::exponent_bits, N::is_signed>();
	}
};

class C_type_composite : public C_type {
public:
	static const FLAGS FORWARD = FLAG0;

	string							id;
	dynamic_array<C_element>		elements;
	const C_type					*parent;
	hash_map<string, const C_type*>	children;
	size_t							_size;

	C_type_composite(TYPE t, FLAGS f = NONE) : C_type(t, f), parent(0), _size(0) {}
	C_type_composite(TYPE t, FLAGS f, string_ref id = nullptr) : C_type(t, f), id(move(id)), parent(0), _size(0) {}
	C_type_composite(C_type_composite&&)		= default;
	C_type_composite(const C_type_composite&)	= default;

	constexpr size_t	size()		const	{ return _size; }
	uint32				alignment()	const;
	C_element*			add(string_ref id, const C_type* type, size_t offset, C_element::ACCESS access = C_element::PUBLIC) {
		C_element	*e	= new(elements) C_element;
		e->init(move(id), type, offset, access);
		if (type)
			flags |= type->flags & TEMPLATED;
		return e;
	}
	C_element*			add_static(string_ref id, const C_type* type, C_element::ACCESS access = C_element::PUBLIC) {
		return add(move(id), type, ~(size_t)0, access);
	}
	const C_element*	get(const char *id, const C_type *type = 0) const;
	const C_element*	at(uint32 offset) const;
	const C_type*		returns(const char *fn) const;

	const C_type*		child(const char *name) {
		if (const C_type *const *type = children.check(name))
			return *type;
		return 0;
	}
	const C_type*		add_child(const char *name, const C_type *t) {
		if (t) {
			if (auto c = t->composite())
				unconst(c)->parent = this;
		}
		return children[name] = t;
	}
};

class C_type_struct : public C_type_composite {
public:
	C_type_struct()		: C_type_composite(STRUCT) {}
	C_type_struct(string_ref id, FLAGS f = NONE) : C_type_composite(STRUCT, f, move(id)) {}
	C_type_struct(string_ref id, FLAGS f, initializer_list<C_element> elements);
	C_type_struct(C_type_struct&&)		= default;
	C_type_struct(const C_type_struct&)	= default;

	void				set_packed(bool packed)	{ flags = (flags - PACKED) | (PACKED * packed); }
	C_element*			add(string_ref id, const C_type* type, C_element::ACCESS access = C_element::PUBLIC, bool is_static = false);
	C_element*			add_atoffset(string_ref id, const C_type* type, uint32 offset, C_element::ACCESS access = C_element::PUBLIC);
	C_element*			add_atbit(string_ref id, const C_type* type, uint32 bit, C_element::ACCESS access = C_element::PUBLIC);
	C_type_struct*		get_base() const {
		if (!_size)
			return 0;
		C_element	&e = elements[0];
		return e.offset == 0 && !e.id && e.type->type == C_type::STRUCT ? (C_type_struct*)e.type : 0;
	}

	template<typename C, typename E>		C_element*			add(string_ref id, E C::*field, C_element::ACCESS access = C_element::PUBLIC);
	template<typename C, typename E, int O> C_element*			add(string_ref id, offset_type<E,O> C::*field, C_element::ACCESS access = C_element::PUBLIC);
	template<typename C, typename E, int S, int N> C_element*	add(string_ref id, bitfield<E,S,N> C::*field, C_element::ACCESS access = C_element::PUBLIC);

	template<typename C, typename E>		C_element*			add(struct C_types &ctypes, string_ref id, E C::*field, C_element::ACCESS access = C_element::PUBLIC);
	template<typename C, typename E, int O> C_element*			add(struct C_types &ctypes, string_ref id, offset_type<E,O> C::*field, C_element::ACCESS access = C_element::PUBLIC);
	template<typename C, typename E, int S, int N> C_element*	add(struct C_types &ctypes, string_ref id, bitfield<E,S,N> C::*field, C_element::ACCESS access = C_element::PUBLIC);
};

class C_type_union : public C_type_composite {
public:
	C_type_union()		: C_type_composite(UNION) {}
	C_type_union(string_ref id)	: C_type_composite(UNION, NONE, move(id)) {}
	C_element*			add(string_ref id, const C_type* type, C_element::ACCESS access = C_element::PUBLIC, bool is_static = false) {
		return C_type_composite::add(move(id), type, is_static ? ~(size_t)0 : 0, access);
	}
};

class C_type_namespace : public C_type_composite {
public:
	C_type_namespace()		: C_type_composite(NAMESPACE) {}
	C_type_namespace(string_ref id)	: C_type_composite(NAMESPACE, NONE, move(id)) {}
};

class C_type_array : public C_type_withsubtype {
public:
	static const FLAGS VECTOR = FLAG0;
	uint32			count;
	constexpr C_type_array(const C_type *subtype, uint32 count)					: C_type_withsubtype(ARRAY, subtype), count(count)	{}
	constexpr C_type_array(const C_type *subtype, uint32 count, uint32 stride)	: C_type_withsubtype(ARRAY, subtype), count(count)	{ subsize = stride; }
	constexpr size_t		size()		const	{ return subsize * count; }
	constexpr uint32		alignment()	const	{ return subtype->alignment(); }
};

class C_type_pointer : public C_type_withsubtype {
public:
	static const FLAGS LONG = FLAG0, LONGLONG = FLAG1, MANAGED = FLAG2, ALLOC = FLAG3, REFERENCE = FLAG4;
	constexpr C_type_pointer(const C_type *subtype, FLAGS flags = NONE) : C_type_withsubtype(POINTER, subtype, flags) {}
	constexpr C_type_pointer(const C_type *subtype, uint32 nbits, bool reference, bool managed = false, bool alloc = false)
		: C_type_withsubtype(POINTER, subtype, FLAGS(nbits <= 16 ? 0 : nbits <= 32 ? 1 : nbits <= 64 ? 2 : 3) | (MANAGED * managed) | (ALLOC * alloc) | (REFERENCE * reference)) {}
	constexpr size_t		size()		const	{ return 2 << (flags & 3);	}
	constexpr uint32		nbits()		const	{ return 16 << (flags & 3);	}
	constexpr bool			managed()	const	{ return !!(flags & MANAGED); }
	constexpr bool			alloc()		const	{ return !!(flags & ALLOC); }
	constexpr bool			reference()	const	{ return !!(flags & REFERENCE); }
};

class C_type_function : public C_type_withargs {
public:
	static const FLAGS HASTHIS = FLAG0, NORETURN = FLAG1, VIRTUAL = FLAG2, ABSTRACT = FLAG3;
	constexpr C_type_function(const C_type *ret, const C_type *_this = nullptr) : C_type_withargs(FUNCTION, ret, _this ? HASTHIS : NONE) {
		if (_this)
			add("this", _this);
	}
	constexpr bool	has_this()	const	{ return !!(flags & HASTHIS); }
};

class C_type_templateparam : public C_type {
public:
	constexpr C_type_templateparam(int n) : C_type(TEMPLATE_PARAM, FLAGS(n) | TEMPLATED) {}
};

class C_type_template : public C_type_withargs {
public:
	C_type_template(const C_type *subtype, int nargs) : C_type_withargs(TEMPLATE, subtype, NONE) {
		for (int i = 0; i < nargs; i++)
			add(format_string("T%i", i), new C_type_templateparam(i));
	}
};

class C_type_custom : public C_type {
public:
	string							id;
	async_callback<string_accum&(string_accum&, const void*)>	output;
	template<typename L> C_type_custom(string_ref id, L &&output) : C_type(CUSTOM, NONE), id(move(id)), output(forward<L>(output)) {}
};

constexpr size_t C_type::size() const {
	switch (type) {
		case INT:		return ((C_type_int*)this)->size();
		case FLOAT:		return ((C_type_float*)this)->size();
		case STRUCT:
		case UNION:
		case NAMESPACE:	return ((C_type_composite*)this)->size();
		case ARRAY:		return ((C_type_array*)this)->size();
		case POINTER:	return ((C_type_pointer*)this)->size();
		default:		return 0;
	}
}

constexpr uint32 C_type::alignment() const {
	if (packed())
		return 1;
	switch (type) {
		case INT:		return ((C_type_int*)this)->alignment();
		case FLOAT:		return ((C_type_float*)this)->alignment();
		case STRUCT:
		case UNION:
		case NAMESPACE:	return ((C_type_composite*)this)->alignment();
		case ARRAY:		return ((C_type_array*)this)->alignment();
		case POINTER:	return ((C_type_pointer*)this)->size32();
		default:		return 1;
	}
}

constexpr const C_type *C_type::subtype() const {
	switch (type) {
		case ARRAY:
		case POINTER:
		case FUNCTION:
		case TEMPLATE:
			return ((C_type_withsubtype*)this)->subtype;
		default:
			return 0;
	}
}

constexpr const char *C_type::tag() const {
	if (auto c = composite())
		return c->id;
	return 0;
}

//-----------------------------------------------------------------------------
//	C_types
//-----------------------------------------------------------------------------

struct CRC32Chash {
	uint32	u;
	CRC32Chash(const char *s)	: u(CRC32C::calc(s))	{}
	CRC32Chash(const C_type &t);
	template<typename T> CRC32Chash(const string_base<T> &s) : u(CRC32C::calc(s.begin(), s.length(), 0)) {}
	friend uint32 hash(const CRC32Chash &h) { return h.u; }
};

struct C_types : C_type_namespace {
	Benaphore							benaphore;
	hash_map<CRC32Chash, C_type*>		types;
	hash_map<const C_type*, string>		type_to_name;

	static const C_type dummy;

	C_types()			{}
	C_types(C_types&&)	= default;
	~C_types();

	const char *get_name(const C_type *t) const {
		if (t) {
			if (const string *p = type_to_name.check(t))
				return *p;
			if (auto c = t->composite())
				return c->id;
		}
		return 0;
	}
	const char *get_name(C_type *t) const {
		return get_name(const_cast<const C_type*>(t));
	}
	template<typename T> const char *get_name(const T &t) const {
		if (C_type **p = types.check(t))
			return get_name(*p);
		return 0;
	}
	const C_type *add(const char *name, const C_type *t) {
		with(benaphore), add_child(name, t);
		if (t)
			with(benaphore), type_to_name[t] = name;
		return t;
	}
	template<typename T> C_type *add(const T &t) {
		auto	w	= with(benaphore);
		auto	p	= types[t];
		if (!p.exists())
			p = new T(t);
		return p;
	}
	template<typename T> C_type *add(T &&t) {
		auto	w	= with(benaphore);
		auto	p	= types[t];
		if (!p.exists())
			p = new noref_t<T>(forward<T>(t));
		return p;
	}
	const C_type *lookup(string_ref s) {
		return child(s);
	}
	C_type *instantiate(const C_type *type, const char *name, const C_type **args);
	C_type *instantiate(const C_type *type, const char *name, const C_type *arg)	{ return instantiate(type, name, &arg); }
	uint64	inferable_template_args(const C_type *type);

	template<typename T, typename V = void> struct type_getter;// {};
	template<typename T> constexpr const C_type*	get_type()				{ return type_getter<T>::f(*this); }
	template<typename T> constexpr const C_type*	get_type(T)				{ return type_getter<T>::f(*this); }
	template<typename T> constexpr const C_type*	get_type(string_ref s)	{
		if (auto t = child(s))
			return t;
		return add(s, get_type<T>());
	}

	template<typename T> constexpr auto				get_type_maybe()->decltype(type_getter<T>::f(this))		{ return type_getter<T>::f(*this); }
	template<typename T> constexpr const C_type*	get_type_maybe()		{ return &dummy; }

	template<typename T> static constexpr const C_type*	get_static_type()	{ return type_getter<T>::f(); }
	template<typename T> static constexpr const C_type*	get_static_type(T)	{ return type_getter<T>::f(); }

	template<typename T> constexpr const C_type*	get_array_type(int n)	{ return add(C_type_array(get_type<T>(), n)); }

	static constexpr const C_type*	or_dummy(const C_type *type)	{ return type ? type : &dummy; }

	template<typename P1, typename...P2> void add_args(C_type_function *f) {
		f->add(0, get_type<P1>());
		add_args<P2...>(f);
	}
	template<typename ...P2> void add_args(C_type_function *f) {
	}
};

template<typename C, typename E> C_element* C_type_struct::add(string_ref id, E C::*field, C_element::ACCESS access) {
	return add_atoffset(move(id), C_types::get_static_type<E>(), T_get_member_offset(field), access);
}
template<typename C, typename E> C_element* C_type_struct::add(C_types &ctypes, string_ref id, E C::*field, C_element::ACCESS access) {
	return add_atoffset(move(id), ctypes.get_type<E>(), T_get_member_offset(field), access);
}
template<typename C, typename E, int S, int N> C_element* C_type_struct::add(string_ref id, bitfield<E, S, N> C::* field, C_element::ACCESS access) {
	return add_atbit(move(id), C_types::get_static_type<E>(), S);
}

template<typename C, typename E, int O> C_element* C_type_struct::add(string_ref id, offset_type<E,O> C::*field, C_element::ACCESS access) {
	return add_atoffset(move(id), C_types::get_static_type<E>(), O, access);
}
template<typename C, typename E, int O> C_element* C_type_struct::add(C_types &ctypes, string_ref id, offset_type<E,O> C::*field, C_element::ACCESS access) {
	return add_atoffset(move(id), ctypes.get_type<E>(), O, access);
}
template<typename C, typename E, int S, int N> C_element* C_type_struct::add(struct C_types& ctypes, string_ref id, bitfield<E, S, N> C::* field, C_element::ACCESS access) {
	return add_atbit(move(id), ctypes.get_type<E>(), S);
}

//-----------------------------------------------------------------------------
//	type_getters
//-----------------------------------------------------------------------------

template<typename T> struct C_types::type_getter<T, enable_if_t<T_builtin_int<T>::value || T_ischar<T>::value>> {
	static constexpr const C_type *f()					{ return C_type_int::get<T>(); }
	static constexpr const C_type *f(C_types &types)	{ return f(); }
};

template<> struct C_types::type_getter<float> {
	static const C_type *f()				{ return C_type_float::get<float>(); }
	static const C_type *f(C_types &types)	{ return f(); }
};
template<> struct C_types::type_getter<double> {
	static const C_type *f()				{ return C_type_float::get<double>(); }
	static const C_type *f(C_types &types)	{ return f(); }
};

template<typename T> struct C_types::type_getter<constructable<T> > : type_getter<T> {};

template<typename T, int N> struct C_types::type_getter<T[N]> {
	static const C_type *f(C_types &types)	{ return types.add(C_type_array(types.get_type<T>(), N)); }
};

template<typename T> struct C_types::type_getter<T*> {
	static const C_type *f()				{ static C_type_pointer type(get_static_type<T>(), sizeof(T*), false); return &type; }
	static const C_type *f(C_types &types)	{ return types.add(C_type_pointer(types.get_type<T>(), sizeof(T*) * 8, false)); }
};

template<typename R, typename... PP> struct C_types::type_getter<R(PP...)> {
	static const C_type *f(C_types &types)	{
		C_type_function	f(types.get_type<R>());
		types.add_args<PP...>(&f);
		return types.add(f);
	}
};

template<typename C, typename R, typename... PP> struct C_types::type_getter<R (C::*)(PP...)> {
	static const C_type *f(C_types &types)	{
		C_type_function	f(types.get_type<R>(), types.get_type_maybe<C>());
		types.add_args<PP...>(&f);
		return types.add(f);
	}
};
template<typename C, typename R, typename... PP> struct C_types::type_getter<R (C::*)(PP...) const> {
	static const C_type *f(C_types &types)	{
		C_type_function	f(types.get_type<R>(), types.get_type_maybe<C>());
		types.add_args<PP...>(&f);
		return types.add(f);
	}
};
template<> struct C_types::type_getter<void> {
	static const C_type *f(C_types &types)	{ return 0; }
};

class C_type_bool : public C_type_enum {
public:
	C_type_bool(int bits, FLAGS f = NONE) : C_type_enum(bits, 2, f) {
		at(0) = {"false", 0};
		at(1) = {"true", 1};
	}
	template<int B> static C_type_bool *get() {
		static C_type_bool type(B, C_type::STATIC);
		return &type;
	}
};

template<> struct C_types::type_getter<bool> {
	static const C_type *f()				{ return C_type_bool::get<sizeof(bool)>(); }
	static const C_type *f(C_types &types)	{ return f(); }
};

//-----------------------------------------------------------------------------
//	functions
//-----------------------------------------------------------------------------

int				NumChildren(const C_type *type);
int				NumElements(const C_type *type);
const C_type*	GetNth(void *&data, const C_type *type, int i, int &shift);
const C_type*	Index(void *&data, const C_type *type, int64 i, int &shift);
const C_type*	GetField(void *&data, const C_type *type, const char *id, int &shift);

string_accum&	DumpData(string_accum &sa, const void *data, const C_type *type, int shift = 0, int depth = 0, FORMAT::FLAGS flags = FORMAT::NONE);
string_accum&	DumpField(string_accum &sa, const C_type *type, uint32 offset, bool dot = false);
string_accum&	GetNthName(string_accum &sa, const C_type *type, int i);

string_scan&	SetData(string_scan &ss, void *data, const C_type *type, int shift);
bool			SetFloat(void *data, const C_type *type, double d, int shift = 0);

float			GetFloat(const void *data, const C_type *type, int shift = 0, float def = 0);
double			GetDouble(const void *data, const C_type *type, int shift = 0, double def = 0);
int64			GetInt(const void *data, const C_type *type, int shift = 0);
number			GetNumber(const void *data, const C_type *type, int shift = 0);

inline const C_type*	GetNth(const void *&data, const C_type *type, int i, int &shift) {
	return GetNth((void*&)data, type, i, shift);
}
inline const C_type*	Index(const void *&data, const C_type *type, int64 i, int &shift) {
	return Index((void*&)data, type, i, shift);
}

inline string_accum& DumpData(string_accum &&sa, const void *data, const C_type *type, int shift = 0, int depth = 0, FORMAT::FLAGS flags = FORMAT::NONE) {
	return DumpData(sa, data, type, shift, depth, flags);
}
inline string_accum& DumpField(string_accum &&sa, const C_type *type, uint32 offset, bool dot = false) {
	return DumpField(sa, type, offset, dot);
}

inline const C_type *IsReference(const C_type *type) {
	return type && type->type == C_type::POINTER && (type->flags & C_type_pointer::REFERENCE) ? type->subtype() : nullptr;
}
inline const C_type *NotReference(const C_type *type) {
	return type && type->type == C_type::POINTER && (type->flags & C_type_pointer::REFERENCE) ? type->subtype() : type;
}
inline const C_type *NotPointer(const C_type *type) {
	return type && type->type == C_type::POINTER ? type->subtype() : type;
}
inline int IsChar(const C_type *type) {
	return type && type->type == C_type::INT && (type->flags & C_type_int::CHAR) ? type->size32() : 0;
}
inline int IsString(const C_type *type) {
	return IsChar(type->subtype());
}
inline const C_type_composite *GetScope(const C_type *type) {
	return type ? type->composite() : 0;
}

inline string_accum& operator<<(string_accum &sa, const param_element<const uint8&, const C_type*> &x) {
	return DumpData(sa, &x.t, x.p);
}

template<typename T> void assign(T &f, const param_element<const uint8&, const C_type*> &a)	{
	const C_type	*subtype;
	void			*data;
	int				shift;

	auto	type = a.p;
	if (type->type == C_type::STRUCT)
		type = (((C_type_struct*)type)->elements)[0].type;

	data	= (void*)&a.t;
	subtype = GetNth(data, type, 0, shift);
	float	x	= GetFloat(data, subtype, shift);

	data	= (void*)&a.t;
	subtype = GetNth(data, type, 1, shift);
	float	y	= GetFloat(data, subtype, shift);

	data	= (void*)&a.t;
	subtype = GetNth(data, type, 2, shift);
	float	z	= GetFloat(data, subtype, shift);

	data	= (void*)&a.t;
	subtype = GetNth(data, type, 3, shift);
	float	w	= GetFloat(data, subtype, shift, 1);

	f.set(x, y, z, w);
}

//-----------------------------------------------------------------------------
//	C_value
//-----------------------------------------------------------------------------

struct C_value {
	const C_type	*type;
	uint64			v;

	static constexpr int num_id(number::TYPE t, number::SIZE s) { return t + s * number::TYPE_COUNT; }

	template<typename T> void set(T t) { type = C_types::get_static_type<T>(); v = 0; raw_copy(t, v); }

	constexpr C_value()								: type(nullptr), v(0) {}
	constexpr C_value(const C_type *type, uint64 v)	: type(type), v(v) {}
	template<typename T> C_value(T t)		{ set(t); }
	C_value(const number &n) : v(n.m) {
		switch (num_id(n.type, n.size)) {
			case num_id(number::UINT, number::SIZE8 ):	type = C_types::get_static_type<uint8>(); break;
			case num_id(number::UINT, number::SIZE16):	type = C_types::get_static_type<uint16>(); break;
			case num_id(number::UINT, number::SIZE32):	type = C_types::get_static_type<uint32>(); break;
			case num_id(number::UINT, number::SIZE64):	type = C_types::get_static_type<uint64>(); break;

			case num_id(number::INT,  number::SIZE8):	type = C_types::get_static_type<int8>(); break;
			case num_id(number::INT,  number::SIZE16):	type = C_types::get_static_type<int16>(); break;
			case num_id(number::INT,  number::SIZE32):	type = C_types::get_static_type<int32>(); break;
			case num_id(number::INT,  number::SIZE64):	type = C_types::get_static_type<int64>(); break;

//			case num_id(number::FLT,  number::SIZE8 ):	set((float8)n); break;
//			case num_id(number::FLT,  number::SIZE16):	set((float16)n); break;
			case num_id(number::FLT,  number::SIZE32):	set((float32)n); break;
			case num_id(number::FLT,  number::SIZE64):	set((float64)n); break;

//			case num_id(number::DEC,  number::SIZE8 ):	set((float8)n.to_binary()); break;
//			case num_id(number::DEC,  number::SIZE16):	set((float16)n.to_binary()); break;
			case num_id(number::DEC,  number::SIZE32):	set((float32)n.to_binary()); break;
			case num_id(number::DEC,  number::SIZE64):	set((float64)n.to_binary()); break;
			default:			type = nullptr; break;
		}
	}

	operator	int64()		const	{ return GetInt(&v, type); }
	operator	float()		const	{ return GetFloat(&v, type); }
	operator	double()	const	{ return GetDouble(&v, type); }
	bool		is_float()	const	{ return type->type == C_type::FLOAT; }
	size_t		is_ptr()	const	{ return type->type == C_type::POINTER ? type->subtype()->size() : 0; }
	bool		either_float(const C_value &b)	const	{ return is_float() || b.is_float(); }
	const C_type* result_int(const C_value &b)	const	{ return C_type_int::get(max(type->size(), b.type->size()), (type->flags | b.type->flags) - C_type_int::ENUM); }

	explicit operator bool() const { return !!v; }

	friend constexpr int compare(const C_value &a, const C_value &b) {
		return	a.is_float() || b.is_float() ? simple_compare((double)a, (double)b) : simple_compare((int64)a, (int64)b);
	}

	C_value	operator-()	const	{ if (type->type == C_type::FLOAT) return -operator double(); return -operator int64(); }
	C_value	operator~()	const	{ return ~operator int64(); }
	bool	operator!()	const	{ return !v; }

	C_value	operator+(const C_value &b)	const	{
		if (either_float(b))
			return operator double() + (double)b;
		if (auto s = is_ptr())
			return C_value(type, v + (int64)b * s);
		if (auto s = b.is_ptr())
			return C_value(b.type, b.v + operator int64() * s);
		return C_value(result_int(b), operator int64() + (int64)b);
	}
	C_value	operator-(const C_value &b)	const	{
		if (either_float(b))
			return operator double() - (double)b;
		if (auto s = is_ptr()) {
			if (b.is_ptr())
				return int64(v - b.v) / s;
			return C_value(type, v - (int64)b * s);
		}
		return C_value(result_int(b), operator int64() - (int64)b);
	}
	C_value	operator*(const C_value &b)	const	{ if (either_float(b)) return operator double() * (double)b; return C_value(result_int(b), operator int64() * (int64)b); }
	C_value	operator/(const C_value &b)	const	{ if (either_float(b)) return operator double() / (double)b; return C_value(result_int(b), operator int64() / (int64)b); }
	int64	operator%(const C_value &b)	const	{ return operator int64() % (int64)b; }
	int64	operator&(const C_value &b)	const	{ return operator int64() & (int64)b; }
	int64	operator|(const C_value &b)	const	{ return operator int64() | (int64)b; }
	int64	operator^(const C_value &b)	const	{ return operator int64() ^ (int64)b; }

	C_value	cast(const C_type *type)	const;
};

} // namespace iso

#include "stream.h"
#include "text_stream.h"
namespace iso {

//from c-header.cpp
bool			ReadCType(text_reader<reader_intf> &reader, C_types &types, const C_type *&type);
bool			ReadCType(istream_ref file, C_types &types, const C_type *&type);
const C_type*	ReadCType(istream_ref file, C_types &types, C_type *type, char *name);
const C_type*	ReadCType(istream_ref file, C_types &types, char *name);
void			ReadCTypes(istream_ref file, C_types &types);
string_accum&	DumpType(string_accum &sa, const C_type *type, string_ref name, int indent, bool _typedef = false);
inline string_accum& DumpType(string_accum &&sa, const C_type *type, string_ref name, int indent, bool _typedef = false) {
	return DumpType(sa, type, name, indent, _typedef);
}
inline string_accum& operator<<(string_accum &sa, const C_type* x) {
	return DumpType(sa, x, 0, 0);
}

//from csharp.cpp
bool			ReadCSType(text_reader<reader_intf> &reader, C_types &types, const C_type *&type);
bool			ReadCSType(istream_ref file, C_types &types, const C_type *&type);
const C_type*	ReadCSType(istream_ref file, C_types &types, C_type *type, char *name);
const C_type*	ReadCSType(istream_ref file, C_types &types, char *name);
void			ReadCSTypes(istream_ref file, C_types &types);

string_accum&	DumpTypeCS(string_accum &sa, const C_type *type);

} // namespace iso


#endif // C_TYPES_H
