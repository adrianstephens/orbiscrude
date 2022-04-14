#ifndef ISO_H
#define ISO_H

#include "allocators/allocator.h"
#include "base/array.h"
#include "base/bits.h"
#include "base/block.h"
#include "base/hash.h"
#include "base/sparse_array.h"
#include "base/list.h"
#include "base/pointer.h"
#include "base/shared_ptr.h"
#include "base/strings.h"
#include "base/tree.h"
#include "crc32.h"
#include "iso_custominit.h"
#include "thread.h"

namespace ISO {
using namespace iso;
using iso::crc32;

#define ISO_NULL	ISO::ptr<void>()
#define ISO_NULL64	ISO::ptr64<void>()

struct Type;

//-----------------------------------------------------------------------------
//	pointers
//-----------------------------------------------------------------------------

template<typename T> struct base_fixed {
	static void		*base;
	static void		*get_base();
	int32			offset;

	static constexpr bool	_check(intptr_t t)			{ return int32(t) == t; }
	static constexpr int32	assert_check(intptr_t t)	{ return ISO_ALWAYS_CHEAPVERIFY(_check(t)), int32(t); }
	static intptr_t			calc(const void *p)			{ return p ? (char*)p - (char*)get_base() : 0; }
	static bool				check(const void *p)		{ return _check(calc(p)); }
	static int32			calc_check(const void *p)	{ return assert_check(calc(p)); }

	constexpr base_fixed()		: offset(0)				{}
	base_fixed(const void *p)	: offset(calc_check(p))	{}
	void	set(const void *p)			{ offset = calc_check(p); }
	void*	get()				const	{ return offset ? (char*)get_base() + offset : 0; }
	bool	operator!()			const	{ return !offset; }
};

template<typename T, int S> struct base_fixed_shift {
	int32			offset;

	static constexpr bool	_check(const void *p, intptr_t v)		{ return (intptr_t(p) & ((1<<S)-1)) == 0 && int32(v) == v; }
	static constexpr int32	assert_check(const void *p, intptr_t v)	{ return ISO_ALWAYS_CHEAPVERIFY(_check(p, v)), int32(v); }
	static intptr_t			calc(const void *p)						{ return p ? ((char*)p - (char*)base_fixed<T>::get_base()) >> S : 0; }
	static bool				check(const void *p)					{ return _check(p, calc(p)); }
	static int32			calc_check(const void *p)				{ return assert_check(p, calc(p)); }

	constexpr base_fixed_shift()	: offset(0)				{}
	base_fixed_shift(const void *p)	: offset(calc_check(p))	{}
	void	set(const void *p)			{ offset = calc_check(p); }
	void*	get()				const	{ return offset ? (char*)base_fixed<T>::get_base() + (intptr_t(offset) << S) : 0; }
	bool	operator!()			const	{ return !offset; }
};

template<typename T, int B> struct base_select {
	int32			offset;

	static void		*base[(1<<B) + 1];
	static void		**get_base();
	base_select()	: offset(0)			{}
	base_select(const void *p)			{ set(p); }
	void	set(const void *p);
	void*	get()				const	{ return offset ? (char*)get_base()[offset & ((1 << B) - 1)] + (offset & -(1 << B)) : 0; }
	bool	operator!()			const	{ return !offset; }
};
//template<typename T, int B> void **base_select<T,B>::get_base() {
//	static void *base[1 << B];
//	return base;
//}
template<typename T, int B> void base_select<T,B>::set(const void *p) {
	if (!p) {
		offset = 0;
		return;
	}
	void	**base = get_base();
	for (int i = 0; i < 1<<B; i++) {
		intptr_t v = (char*)p - (char*)base[i];
		if (int32(v) == v) {
			offset = int32(v) | i;
			return;
		}
	}
	ISO_CHEAPASSERT(0);
}
//-----------------------------------------------------------------------------
//	allocation
//-----------------------------------------------------------------------------

struct bin_allocator : vallocator {
	void	(*vtransfer)(void*, void*, const void*, size_t);
	uint32	(*vfix)		(void*, void*, size_t);
	void*	(*vunfix)	(void*, uint32);

	template<class T> struct thunk {
		static void		transfer(void* me, void* d, const void* s, size_t size) { ((T*)me)->transfer(d, s, size); }
		static uint32	fix(void* me, void* p, size_t size) { return ((T*)me)->fix(p, size); }
		static void*	unfix(void* me, uint32 p) { return ((T*)me)->unfix(p); }
	};
	template<typename T> void set(T* t) {
		vallocator::set(t);
		vtransfer	= thunk<T>::transfer;
		vfix		= thunk<T>::fix;
		vunfix		= thunk<T>::unfix;
	}

	void	transfer(void* d, const void* s, size_t size)	const { vtransfer(me, d, s, size); }
	uint32	fix(void* p, size_t size = 0)					const { return vfix(me, p, size); }
	void*	unfix(uint32 p)									const { return vunfix(me, p); }

	uint32	offset(const void *p)	{ return uint32(uintptr_t(fix((void*)p))); }
	uint32	align(uint32 a)			{ return offset(alloc(0, a)); }
	uint32	offset()				{ return align(1); }
	uint32	add(const void *p, size_t n, size_t a = 1) {
		void	*d = alloc(n, a);
		memcpy(d, p, n);
		return offset(d);
	}
	void	not0() {
		if (offset() == 0) {
			char	zero = 0;
			add(&zero, 1);
		}
	}

};
iso_export bin_allocator&	iso_bin_allocator();

struct allocate_base {
	enum FLAGS {
		TOOL_DELETE = 1 << 0,
	};
	static iso::flags<FLAGS> flags;
};

template<int B> struct allocate {
	static iso_export vallocator&	allocator();
	template<typename T> static void set(T* t) { allocator().set(t); }

	inline static void* alloc(size_t size, size_t align = 16)				{ return allocator().alloc(size, align); }
	inline static void* realloc(void* p, size_t size, size_t align = 16)	{ return allocator().realloc(p, size, align); }
	inline static bool	free(void* p)										{ return allocator().free(p); }
};

//-----------------------------------------------------------------------------
//	strings
//-----------------------------------------------------------------------------

iso_export const char*	store_string(const char* id);
template<> iso_export void*	base_fixed<void>::get_base();
template<> iso_export void**base_select<void, 2>::get_base();
template<> iso_export void	base_select<void, 2>::set(const void* p);

static const int PTRBITS = sizeof(void*) * 8;

template<typename T, int N, int B = PTRBITS> struct ptr_type;
template<typename T, int N>	struct ptr_type<T, N, N>			{ typedef pointer<T> type; };
template<typename T>		struct ptr_type<T, 32, 64>			{ typedef soft_pointer<T, base_fixed_shift<void, 2>> type; };
template<typename T>		struct ptr_type<T, 64, 32>			{ typedef soft_pointer<T, base_absolute<uint64>> type; };
template<>					struct ptr_type<const Type, 32, 64> { typedef soft_pointer<const Type, base_select<void, 2>> type; };

typedef ptr_type<void, 32>::type	void_ptr32;
typedef ptr_type<void, 64>::type	void_ptr64;
typedef ptr_type<const Type, 32>::type type_ptr;

template<typename C, int N> struct ptr_string : public string_base<typename ptr_type<C, N>::type> {
	typedef string_base<typename ptr_type<C, N>::type> B;
	inline static C *dup(const C *s) {
		C *m = 0;
		if (size_t n = string_len(s)) {
			memcpy(m = (C*)allocate<N>::alloc(sizeof(C) * (n + 1)), s, n);
			m[n] = 0;
		}
		return m;
	}

	ptr_string() : B(0) {}
	ptr_string(ptr_string &&b)	: B(b.detach()) {}
	ptr_string(const C* s)		: B(dup(s)) {}
	~ptr_string()								{ allocate<N>::free(B::p); }
	ptr_string& operator=(ptr_string &&b)		{ swap(B::p, b.p); return *this; }
	ptr_string& operator=(const C* s)			{ allocate<N>::free(exchange(B::p, dup(s))); return *this; }
	ptr_string& operator=(const ptr_string &b)	{ return operator=(b.begin()); }
	C	*detach() { return exchange(B::p, nullptr); }
};

//-----------------------------------------------------------------------------
//	tag
//-----------------------------------------------------------------------------

// tag - text based id
class tag : public cstring {
public:
	constexpr				tag()					: cstring(0) {}
	constexpr				tag(int)				: cstring(0) {}
	template<int N>			tag(const char (&p)[N]) : cstring(store_string(p)) {}
	template<int N>			tag(char (&p)[N])		: cstring(store_string(p)) {}
	template<typename T>	tag(const T* const& p, typename T_same<T, char>::type* x = 0) : cstring(store_string(str8(p))) {}
	template<typename T>	tag(const string_base<T>& p) : cstring(store_string(string(p))) {}
	tag& operator=(const tag&) = default;
};

// tag2 - holder of text and crc based versions of an id
class tag2 {
	tag				t;
	mutable crc32	c;
public:
	constexpr				tag2() {}
	constexpr				tag2(tag t)		: t(t) {}
	constexpr				tag2(int i)		: c((uint32)i)	{}	// to catch nulls
	explicit constexpr		tag2(crc32 c)	: c(c) {}
	explicit constexpr		tag2(uint32 c)	: c(c) {}

	template<int N>			tag2(const char (&p)[N]) : t(p) {}
	template<int N>			tag2(char (&p)[N]) : t(p)		{}
	template<typename T>	tag2(const T* const& p, typename T_same<T, char>::type* x = 0) : t(p) {}
	template<typename T>	tag2(const string_base<T>& p) : t(p) {}

	explicit operator bool()	const	{ return t || c; }
	operator tag()		const	{ return t; }
	operator crc32()	const	{ if (!c) c.set(t); return c; }
	operator tag&()				{ return t; }
	operator crc32&()			{ if (!c) c.set(t); return c; }

	tag		get_tag()	const	{ return t; }
	crc32	get_crc32()	const	{ return operator crc32(); }
	tag&	get_tag()			{ return t; }
	crc32&	get_crc32()			{ return operator crc32&(); }

	friend bool operator==(const tag2& a, const tag2& b) { return a.t && b.t ? a.t == b.t : a.get_crc32() == b.get_crc32(); }
	friend bool operator!=(const tag2& a, const tag2& b) { return !(a == b); }
};

// storage of id - either a tag or a crc32
class tag1 {
	typedef ptr_type<const char, 32>::type S;
	uint32	u;
public:
	constexpr tag1()		: u(0) {}
	constexpr tag1(crc32 c)	: u(c) {}
	tag1(const tag& t)		: u(force_cast<uint32>(S(t))) {}
	constexpr bool blank()		const { return u == 0; }
	crc32	get_crc()			const { return *(const crc32*)this; }
	tag		get_tag()			const { return force_cast<tag>(((S*)this)->get()); }
	crc32	get_crc(int iscrc)	const { return iscrc ? get_crc() : crc32(get_tag()); }
	tag		get_tag(int iscrc)	const { return iscrc ? tag(0) : get_tag(); }
	tag2	get_tag2(int iscrc) const { return iscrc ? tag2(get_crc()) : tag2(get_tag()); }
	operator uint32()			const { return u; }
};

template<typename T>	tag2	__GetName(const T& t, ...)				{ return tag2(); }
template<typename T>	tag2	__GetName(const T& t, void_t<decltype(declval<T>().ID())> *dummy = 0) { return t.ID(); }
template<typename T>	tag2	__GetName(const T& t, void_t<decltype(declval<T>()->ID())> *dummy = 0) { return t->ID(); }

template<typename T>	tag2	__GetKeyName(const T& t, void_t<decltype(tag2(declval<T>()))> *dummy = 0) { return tag2(t); }
template<typename T>	tag2	__GetKeyName(const T& t,...)					{ return tag2(); }

template<typename T>	tag2	_GetName(const T& t)							{ return __GetName(t, 0); }
template<typename T>	tag2	_GetIteratorName(T&& t, void_t<decltype(declval<T>().key())> *dummy = 0) { return __GetKeyName(t.key(), 0); }
template<typename T>	tag2	_GetIteratorName(T&& t, ...)					{ return _GetName(*t); }
template<typename T>	tag2	_GetIteratorName(cached_iterator<T>&& t)		{ return _GetIteratorName(t.iterator(), 0); }
//template<typename T>	tag2	_GetName(const cached_iterator<T>& t)			{ return _GetIteratorName(t.iterator(), 0); }

//template<typename T> enable_if_t< T_has_add<decltype(begin(declval<T>())), int>::value, tag2>	_GetIndexName(const T& t, int i)	{ return _GetIteratorName(begin(t) + i); }
//template<typename T> enable_if_t<!T_has_add<decltype(begin(declval<T>())), int>::value, tag2>	_GetIndexName(const T& t, int i)	{ return _GetName(t[i]); }
template<typename T> enable_if_t< can_add_v<decltype(begin(declval<T>())), int>, tag2>	_GetIndexName(const T& t, int i)	{ return _GetIteratorName(begin(t) + i); }
template<typename T> enable_if_t<!can_add_v<decltype(begin(declval<T>())), int>, tag2>	_GetIndexName(const T& t, int i)	{ return _GetName(t[i]); }

template<typename C> int _GetIndex(const tag2& id, const C& c, int from) {
	auto	e = end(c);
	for (auto i = nth(begin(c), from); i != e; ++i) {
		if (_GetIteratorName(i, 0) == id)
			return int(distance(begin(c), i));
	}
	return -1;
}

inline const char*		to_string(const tag& t) { return t; }
iso_export size_t		to_string(char* s, const tag2& t);
iso_export const char*	to_string(const tag2& t);

//-----------------------------------------------------------------------------
//	Types
//-----------------------------------------------------------------------------

enum TYPE {
	UNKNOWN,
	INT,
	FLOAT,
	STRING,
	COMPOSITE,
	ARRAY,
	OPENARRAY,
	REFERENCE,
	VIRTUAL,
	USER,
	FUNCTION,
	TOTAL,
};

//constexpr TYPE operator|(TYPE a, int b) { return TYPE(uint8(a) | b); }

template<typename T> Type*			getdef();
template<typename T> inline Type*	getdef(const T& t) { return getdef<T>(); }

enum MATCH {
	MATCH_NOUSERRECURSE			= 1 << 0,	// user types are not recursed into for sameness check
	MATCH_NOUSERRECURSE_RHS		= 1 << 1,	// RHS user types are not recursed into for sameness check
	MATCH_NOUSERRECURSE_BOTH	= 1 << 2,	// if both types are user, do not recurse for sameness check
	MATCH_MATCHNULLS			= 1 << 3,	// null types match anything
	MATCH_MATCHNULL_RHS			= 1 << 4,	// RHS null matches anything
	MATCH_IGNORE_SIZE			= 1 << 5,	// ignore 64-bittedness
	MATCH_IGNORE_INTERPRETATION	= 1 << 6,	// ignore sign flags, etc, that only affect interpretation
};

struct Type {
	enum FLAGS {
		//common flags
		TYPE_32BIT	= 0 << 4,
		TYPE_64BIT	= 1 << 4,
		TYPE_PACKED	= 1 << 5,
		TYPE_DODGY	= 1 << 6,
		TYPE_FIXED	= 1 << 7,
		TYPE_MASK	= 15,
		TYPE_MASKEX	= TYPE_MASK | TYPE_64BIT,

		// type specific flags
		NONE		= 0,
		FLAG0		= 0x100 << 0,
		FLAG1		= 0x100 << 1,
		FLAG2		= 0x100 << 2,
		FLAG3		= 0x100 << 3,
		FLAG4		= 0x100 << 4,
		FLAG5		= 0x100 << 5,
		FLAG6		= 0x100 << 6,
		FLAG7		= 0x100 << 7,

		// creation only flags
		UNSTORED0	= 1 << 16,
	};

	struct FLAGS2 {
		uint16 f2;
		constexpr FLAGS2(FLAGS a, FLAGS b = NONE) : f2(uint16(a) | (uint16(b) >> 8)) {}
		constexpr operator FLAGS()	const { return FLAGS(f2 & 0xff00); }
		constexpr FLAGS second()	const { return FLAGS((f2 & 0xff) << 8); }
	};
	union {
		struct {
			uint16 flags;	// type in bottom bits
			union {
				struct {
					uint8 param1, param2;
				};
				uint16 param16;
			};
		};
		uint32 u;
	};
	friend constexpr FLAGS operator|(FLAGS a, FLAGS b)	{ return FLAGS(uint16(a) | uint16(b)); }

	iso_export static const Type*	_SubType(const Type* type);
	iso_export static const Type*	_SkipUser(const Type* type);
	iso_export static uint32		_GetSize(const Type* type);
	iso_export static uint32		_GetAlignment(const Type* type);
	iso_export static bool			_Is(const Type* type, const tag2& id);
	iso_export static bool			_ContainsReferences(const Type* type);
	iso_export static bool			_IsPlainData(const Type* type, bool flip = false);
	iso_export static bool			_Same(const Type* type1, const Type* type2, int criteria);

	friend const Type*			SubType(const Type* type)									{ return _SubType(type); }
	friend const Type*			SkipUser(const Type* type)									{ return _SkipUser(type); }
	friend uint32				GetSize(const Type* type)									{ return _GetSize(type); }
	friend uint32				GetAlignment(const Type* type)								{ return _GetAlignment(type); }
	friend bool					Is(const Type* type, const tag2& id)						{ return _Is(type, id); }
	friend bool					ContainsReferences(const Type* type)						{ return _ContainsReferences(type); }
	friend bool					IsPlainData(const Type* type, bool flip = false)			{ return _IsPlainData(type, flip); }
	friend bool					Same(const Type* type1, const Type* type2, int criteria)	{ return _Same(type1, type2, criteria); }
	friend TYPE					TypeType(const Type* type)									{ return type ? (TYPE)(type->flags & TYPE_MASK) : UNKNOWN; }
	friend FLAGS				Flags(const Type* type)										{ return type ? (FLAGS)type->flags : NONE; }
	friend TYPE					SkipUserType(const Type* type)								{ return TypeType(_SkipUser(type)); }
	friend FLAGS				SkipUserFlags(const Type* type)								{ return Flags(_SkipUser(type)); }

public:
	Type(TYPE t, FLAGS f = NONE) : flags(t | f), param1(0), param2(0) {}
	Type(TYPE t, FLAGS f, uint8 p1, uint8 p2 = 0) : flags(t | f), param1(p1), param2(p2) {}

	constexpr TYPE				GetType()								const { return (TYPE)(flags & TYPE_MASK); }
	constexpr TYPE				GetTypeEx()								const { return (TYPE)(flags & TYPE_MASKEX); }
	const Type*					SubType()								const { return _SubType(this); }
	const Type*					SkipUser()								const { return _SkipUser(this); }
	uint32						GetSize()								const { return _GetSize(this); }
	uint32						GetAlignment()							const { return _GetAlignment(this); }
	bool						Is(tag2 id)								const { return _Is(this, id); }
	bool						ContainsReferences()					const { return _ContainsReferences(this); }
	bool						IsPlainData(bool flip = false)			const { return _IsPlainData(this, flip); }
	bool						SameAs(const Type* type2, int crit = 0) const { return _Same(this, type2, crit); }
	template<typename T> bool	SameAs(int crit = 0)					const { return _Same(this, getdef<T>(), crit); }
	template<typename T> bool	Is()									const { return this == getdef<T>(); }
	constexpr bool				Dodgy()									const { return !!(flags & TYPE_DODGY); }
	constexpr bool				Fixed()									const { return !!(flags & TYPE_FIXED); }
	constexpr bool				Is64Bit()								const { return !!(flags & TYPE_64BIT); }
	constexpr bool				IsPacked()								const { return !!(flags & TYPE_PACKED); }
	void*						ReadPtr(const void* data)				const { return Is64Bit() ? ((void_ptr64*)data)->get() : ((void_ptr32*)data)->get(); }
	void						WritePtr(void* data, void* p)			const { if (Is64Bit()) *(void_ptr64*)data = p; else *(void_ptr32*)data = p; }

	void*	operator new(size_t size, void* p) { return p; }
	void*	operator new(size_t size) { return allocate<32>::alloc(size); }
	void	operator delete(void* p) { allocate<32>::free(p); }
};

iso_export string_accum& operator<<(string_accum& a, const Type* const Type);
inline size_t			to_string(char* s, const Type* const type)	{ return (lvalue(fixed_accum(s, 64)) << type).getp() - s; }
inline fixed_string<64>	to_string(const Type* const type)			{ fixed_string<64> s; s << type; return s; }

iso_export const Type* Duplicate(const Type* type);
iso_export const Type* Canonical(const Type* type);

//-----------------------------------------------------------------------------
//	TypeInt
//-----------------------------------------------------------------------------

struct TypeInt : Type {
	static const FLAGS SIGN = FLAG0, HEX = FLAG1, ENUM = FLAG2, NOSWAP = FLAG3, FRAC_ADJUST = FLAG4, CHR = FLAG5;
	TypeInt(uint8 nbits, uint8 frac, FLAGS flags = NONE) : Type(INT, flags, nbits, frac) {}
	constexpr uint32	GetSize()		const { return (param1 + 7) / 8; }
	constexpr bool		is_signed()		const { return !!(flags & SIGN); }
	constexpr uint8		num_bits()		const { return uint8(param1); }
	constexpr uint8		frac_bits()		const { return uint8(param2); }
	constexpr bool		frac_adjust()	const { return !!(flags & FRAC_ADJUST); }
	constexpr int		frac_factor()	const { return bit(frac_bits()) - int(frac_adjust()); }
	constexpr int		get_max()		const { return unsigned(-1) >> (is_signed() ? 33 - param1 : 32 - param1); }
	constexpr int		get_min()		const { return is_signed() ? -int(bit(param1 - 1)) : 0; }

	template<int nbits, int frac, int flags> static TypeInt* make() {
		static TypeInt t(nbits, frac, (FLAGS)flags);
		return &t;
	}
	template<typename T> static TypeInt* make() {
		return make<sizeof(T) * 8, 0, num_traits<T>::is_signed ? SIGN : NONE>();
	}

	void	set(void* data, int64 i)	const { write_bytes(data, i, GetSize()); }
	void	set(void* data, number n)	const { write_bytes(data, n.fixed(frac_bits(), frac_adjust()), GetSize()); }
	int		get(void* data)				const { uint32 v = read_bytes<uint32>(data, GetSize()); return is_signed() ? sign_extend(v, num_bits()) : int(v); }
	int64	get64(void* data)			const { uint64 v = read_bytes<uint64>(data, GetSize()); return is_signed() ? sign_extend(v, num_bits()) : int64(v); }
	template<int BITS> sint_bits_t<BITS> get(void* data) const {
		uint_bits_t<BITS> v = read_bytes<uint_bits_t<BITS>>(data, GetSize());
		return is_signed() ? sign_extend(v, num_bits()) : sint_bits_t<BITS>(v);
	}
};

template<typename T> struct EnumT {
	tag1	id;
	T		value;
	tag		ID() const { return id.get_tag(); }
	void	set(tag _id, T _value) {
		id		= _id;
		value	= _value;
	}
};

template<typename T, int N> struct EnumTN {
	typedef EnumT<T> Enum;
	Enum			enums[N];
	uint32			term;
	template<typename...P> void Init(int i, tag id, T p0, P...p) { enums[i].set(id, p0); Init(i + 1, p...); }
	void				Init(int i) {}
	template<typename...P> EnumTN(P...p) : term(0) { Init(0, p...); }
};

//-----------------------------------------------------------------------------
//	TypeFloat
//-----------------------------------------------------------------------------

struct TypeFloat : Type {
	static const FLAGS SIGN = FLAG0;
	TypeFloat(uint8 nbits, uint8 exp, FLAGS flags = NONE) : Type(FLOAT, flags, nbits, exp) {}
	constexpr uint32	GetSize()		const { return (param1 + 7) / 8; }
	constexpr bool		is_signed()		const { return !!(flags & SIGN); }
	constexpr uint8		num_bits()		const { return uint8(param1); }
	constexpr uint8		exponent_bits()	const { return uint8(param2); }
	constexpr uint8		mantissa_bits()	const { return uint8(param1 - param2 - (flags & SIGN ? 1 : 0)); }
	constexpr float		get_max()		const { return iorf(bits(min(mantissa_bits(), 23)), bits(min(exponent_bits(), 8)) - 1, 0).f(); }
	constexpr float		get_min()		const { return is_signed() ? iorf(bits(min(mantissa_bits(), 23)), bits(min(exponent_bits(), 8)) - 1, 1).f() : 0.f; }
	constexpr double	get_max64()		const { return iord(bits(min(mantissa_bits(), 52)), bits(min(exponent_bits(), 11)) - 1, 0).f(); }
	constexpr double	get_min64()		const { return is_signed() ? iord(bits(min(mantissa_bits(), 52)), bits(min(exponent_bits(), 11)) - 1, 1).f() : 0.f; }

	template<uint8 nbits, uint8 exp, FLAGS flags> static TypeFloat* make() {
		static TypeFloat t(nbits, exp, flags);
		return &t;
	}
	template<typename T> static TypeFloat* make() {
		return make<sizeof(T) * 8, num_traits<T>::exponent_bits, num_traits<T>::is_signed ? SIGN : NONE>();
	}

	iso_export void		set(void* data, uint64 m, int e, bool s) const;
	iso_export void		set(void* data, float f) const;
	iso_export void		set(void* data, double f) const;
	iso_export void		set(void* data, number n) const {
		n	= n.to_binary();
		uint64	m	= iso::abs(n.m);
		int		i	= leading_zeros(m);
		set(data, m << i, n.e - i + 62, n.m < 0);
	}
	iso_export float	get(void* data) const;
	iso_export double	get64(void* data) const;
};

//-----------------------------------------------------------------------------
//	TypeString
//-----------------------------------------------------------------------------

struct TypeString : Type {
	static const FLAGS UTF16 = FLAG0, UTF32 = FLAG1, ALLOC = FLAG2, UNESCAPED = FLAG3, _MALLOC = FLAG4, MALLOC = ALLOC | _MALLOC;
	TypeString(bool is64bit, FLAGS flags) : Type(STRING, flags | (is64bit ? TYPE_64BIT : NONE)) {}
	constexpr uint32	GetSize()			const { return Is64Bit() ? 8 : 4; }
	constexpr uint32	GetAlignment()		const { return Is64Bit() ? 8 : 4; }
	constexpr uint32	log2_char_size()	const { return (flags / UTF16) & 3; }
	constexpr uint32	char_size()			const { return 1 << log2_char_size(); }

	//	const char*			get(const void* data) const { return (const char*)ReadPtr(data); }
	memory_block		get_memory(const void* data) const;
	uint32				len(const void *s)	const;
	void*				dup(const void *s)	const;

	void free(void* data) const {
		if (void *p = ReadPtr(data)) {
			if (flags & ALLOC) {
				if (flags & _MALLOC)
					iso::free(p);
				else if (Is64Bit())
					allocate<64>::free(p);
				else
					allocate<32>::free(p);
			}
		}
	}
	void set(void* data, const char *s) const {
		if (flags & ALLOC)
			s = (char*)dup(s);
		WritePtr(data, (void*)s);
	}
};

//-----------------------------------------------------------------------------
//	TypeComposite
//-----------------------------------------------------------------------------

struct Element {
	tag1		id;
	type_ptr	type;
	uint32		offset;
	uint32		size;

	Element()	{}
	Element(tag id, const Type* type, size_t offset, size_t size)	{ set(id, type, offset, size); }
	template<typename B, typename T> Element(tag id, T B::*p)		{ set(id, p); }

	constexpr uint32	end()					const	{ return offset + size; }
	memory_block		get(void *base)			const	{ return memory_block((uint8*)base + offset, size); }
	const_memory_block	get(const void *base)	const	{ return const_memory_block((const uint8*)base + offset, size); }

	template<typename S, typename C, typename T> struct set_helper {
		template<typename U, T C::*F> static uint32 f(tag id, Element& e) {
			return e.set(id, getdef<U>(), (uint32)T_get_member_offset2<S,C,T>(F), sizeof(T));
		}
	};

	template<typename S, typename C, typename T, int O> struct set_helper<S, C, offset_type<T, O>> {
		template<typename U, offset_type<T, O> C::*F> static uint32 f(tag id, Element& e) {
			return e.set(id, getdef<T>(), uint32(T_get_member_offset2<S,C,offset_type<T, O>>(F) + T_get_member_offset(&offset_type<T, O>::t)), sizeof(T));
		}
	};

	uint32	set(tag1 _id, const Type* _type, size_t _offset, size_t _size) {
		id		= _id;
		type	= _type;
		offset	= uint32(_offset);
		size	= uint32(_size);
		return offset + size;
	}
	uint32	set(tag1 id, const Type* type, size_t offset)			{ return set(id, type, offset, type->GetSize()); }
	template<typename C, typename T> uint32 set(tag id, T C::*p)	{ return set(id, getdef<T>(), (uint32)T_get_member_offset(p), sizeof(T)); }

	template<typename S, typename U, typename C, typename T, T C::*p>				uint32 set2(const meta::field<T C::*, p> &f) 	{ return set_helper<S, C, T>::template f<U, p>(tag(f.name), *this); }
	template<typename S, typename C, typename T, T C::*p>							uint32 set(const meta::field<T C::*, p> &f)		{ return set_helper<S, C, T>::template f<T, p>(tag(f.name), *this); }
	template<typename S, typename C, typename R, R (C::*p)()>						uint32 set(const meta::field<R (C::*)(), p> &f);
	template<typename S, typename C, typename R, R (C::*p)() const>					uint32 set(const meta::field<R (C::*)() const, p> &f);
	template<typename S, typename P, typename C, typename R, R (C::*p)(P)>			uint32 set(const meta::field<R (C::*)(P), p> &f);
	template<typename S, typename P, typename C, typename R, R (C::*p)(P) const>	uint32 set(const meta::field<R (C::*)(P) const, p> &f);
};

template<int N> struct ElementN {
	Element fields[N ? N : 1];

	template<typename S> void Init(int i) {}
	template<typename S, typename P0, typename...P> void Init(int i, tag id, P0 p0, P...p) {
		fields[i].set(id, p0);
		Init<S>(i + 1, p...);
	}
	template<typename S, typename T, T t, typename...P> void Init(int i, const meta::field<T, t> &p0, P...p) {
		fields[i].set<S>(p0);
		Init<S>(i + 1, p...);
	}
	ElementN() {}
	template<typename S, typename...P> ElementN(S *dummy, P...p)	{ Init<S>(0, p...); }
};

struct Element2 {
	tag2		id;
	type_ptr	type;
	uint32		offset;
	uint32		size;
	Element2(tag2 id, const Element &e) : id(id), type(e.type), offset(e.offset), size(e.size) {}
};

struct TypeComposite : public Type, public trailing_array2<TypeComposite, Element> {
	static const FLAGS CRCIDS = FLAG0, DEFAULT = FLAG1, RELATIVEBASE = FLAG2, FORCEOFFSETS = FLAG3;
	uint32			count;

	void*	operator new(size_t size, void* p)				{ return p; }
	void*	operator new(size_t, uint32 n, uint32 defs = 0)	{ return Type::operator new(calc_size(n) + defs); }
	void	operator delete(void* p, uint32, uint32)		{ Type::operator delete(p); }

	TypeComposite(uint32 count = 0, FLAGS flags = NONE) : Type(COMPOSITE, flags), count(count)			{}
	TypeComposite(uint32 count, FLAGS flags, uint32 log2align) : Type(COMPOSITE, flags), count(count)	{ SetLog2Align(log2align); }

	void				SetLog2Align(uint32 log2align)	{ param1 = log2align; }
	constexpr uint32	size()					const	{ return count; }
	constexpr uint32	Count()					const	{ return count; }
	constexpr uint32	GetSize()				const	{ return !count ? 0 : !param1 ? back().end() : align_pow2(back().end(), param1); }
	uint32				GetAlignment()			const	{ return param1 ? (1 << param1) : CalcAlignment(); }
	uint32				CalcAlignment()			const;
	iso_export int		GetIndex(const tag2& id, int from = 0) const;
	const Element*		Find(const tag2& id)	const	{ int i = GetIndex(id); return i >= 0 ? begin() + i : 0; }
	tag2				GetID(const Element* i)	const	{ return i->id.get_tag2(flags & TypeComposite::CRCIDS); }
	tag2				GetID(int i)			const	{ return (*this)[i].id.get_tag2(flags & TypeComposite::CRCIDS); }
	const void*			Default()				const	{ return flags & DEFAULT ? (void*)&(*this)[count] : NULL; }

	void						Reset()					{ count = 0; flags &= TYPE_MASK; }
	void						Add(const Element& e)	{ begin()[count++] = e; }
	iso_export uint32			Add(const Type* type, tag1 id, bool packed = false);
	uint32						Add(const Type* type, tag id = 0, bool packed = false) 	{ return Add(type, (tag1)id, packed); }
	template<typename T> uint32 Add(tag id = 0, bool packed = false) 					{ return Add(getdef<T>(), id, packed); }
	iso_export TypeComposite*	Duplicate(void* defaults = 0) const;

	auto				Components() 			const 	{ return transformc(*this, [this](const ISO::Element &e) { return ISO::Element2(GetID(&e), e); }); }

	memory_block		get(void *base, int i)			const { return begin()[i].get(base); }
	const_memory_block	get(const void *base, int i)	const { return begin()[i].get(base); }
};

template<int N> struct TypeCompositeN : TypeComposite, ElementN<N> {
	TypeCompositeN(uint32 count = N, FLAGS flags = NONE) : TypeComposite(count, flags) {}
	template<typename...P> TypeCompositeN(FLAGS flags, uint32 log2align, P...p)		: TypeComposite(N, flags, log2align), ElementN<N>(p...) {}
	template<typename T, typename...P> TypeCompositeN(T *dummy, FLAGS flags, P...p)	: TypeComposite(N, flags, log2alignment<T>), ElementN<N>(dummy, p...) {}
	template<typename T, typename...P> TypeCompositeN(T *dummy, P...p)				: TypeComposite(N, NONE, log2alignment<T>), ElementN<N>(dummy, p...) {}
};

//-----------------------------------------------------------------------------
//	TypeArray
//-----------------------------------------------------------------------------

struct TypeArray : Type {
	uint32		count;
	type_ptr	subtype;
	uint32		subsize;
	TypeArray(const Type* subtype, uint32 n, uint32 subsize)	: Type(ARRAY), count(n), subtype(subtype), subsize(subsize) {}
	TypeArray(const Type* subtype, uint32 n)					: Type(ARRAY), count(n), subtype(subtype), subsize(subtype->GetSize()) {}
	constexpr uint32 Count()	const { return count; }
	constexpr uint32 GetSize()	const { return subsize * count; }
	memory_block		get_memory(void* data)			const	{ return {data, GetSize()}; }
	const_memory_block	get_memory(const void* data)	const	{ return {data, GetSize()}; }
};

template<typename T> struct TypeArrayT : TypeArray {
	constexpr TypeArrayT(uint32 n) : TypeArray(getdef<T>(), n, sizeof(T)) {}
};

//-----------------------------------------------------------------------------
//	TypeOpenArray
//-----------------------------------------------------------------------------

struct OpenArrayHead;

struct TypeOpenArray : Type {
	type_ptr	subtype;
	uint32		subsize;
	TypeOpenArray(const Type* subtype, uint32 subsize, uint8 log2align = 0, FLAGS flags = NONE) : Type(OPENARRAY, flags), subtype(subtype), subsize(subsize) { param1 = log2align; }
	TypeOpenArray(const Type* subtype, FLAGS flags = NONE) : Type(OPENARRAY, flags), subtype(subtype), subsize(subtype->GetSize()) {}
	constexpr uint32 SubAlignment() const { return 1 << param1; }
	OpenArrayHead*	get_header(const void* data)	const;
	uint32			get_count(const void* data)		const;
	memory_block	get_memory(const void* data)	const;
};

//-----------------------------------------------------------------------------
//	TypeReference
//-----------------------------------------------------------------------------

template<typename T, int B = 32> class ptr;

struct TypeReference : Type {
	type_ptr	subtype;
	TypeReference(const Type* subtype, FLAGS flags = NONE) : Type(REFERENCE, flags), subtype(subtype) {}
	iso_export void			set(void* data, const ptr<void,64> &p) const;
	iso_export ptr<void,64>	get(void* data) const;
};

//-----------------------------------------------------------------------------
//	TypeUser
//-----------------------------------------------------------------------------

struct TypeUser : Type {
	static const FLAGS INITCALLBACK = FLAG0, FROMFILE = FLAG1, CRCID = FLAG2, CHANGE = FLAG3, LOCALSUBTYPE = FLAG4, WRITETOBIN = FLAG5;
	tag1		id;
	type_ptr	subtype;
	TypeUser(tag2 id, const Type* subtype = 0, FLAGS flags = WRITETOBIN) : Type(USER, flags), subtype(subtype) { SetID(id); }
	tag2			ID()			const { return id.get_tag2(flags & CRCID); }
	constexpr bool	KeepExternals() const { return (flags & (CHANGE | WRITETOBIN)) == (CHANGE | WRITETOBIN); }
	void			SetID(tag2 id2) {
		if (tag s = id2) {
			id = s;
			flags &= ~CRCID;
		} else {
			id = id2.get_crc32();
			flags |= CRCID;
		}
	}
};

//-----------------------------------------------------------------------------
//	TypeFunction (TBD)
//-----------------------------------------------------------------------------

struct TypeFunction : Type {
	type_ptr rettype;
	type_ptr params;
	TypeFunction(const Type* r, const Type* p) : Type(FUNCTION), rettype(r), params(p) {}
};

//-----------------------------------------------------------------------------
//	UserTypes
//-----------------------------------------------------------------------------
struct UserTypeArray : dynamic_array<TypeUser*> {
	iso_export ~UserTypeArray();

	iso_export TypeUser*	Find(const tag2& id);
	iso_export void			Add(TypeUser* t);
};

iso_export	UserTypeArray&	get_user_types();
#define user_types get_user_types()

//extern iso_export	UserTypeArray	user_types;
//extern iso_export singleton<UserTypeArray> _user_types;
//#define user_types _user_types()

//-----------------------------------------------------------------------------
//	enum helpers
//-----------------------------------------------------------------------------
template<typename T> class TypeEnumT : public TypeInt, public trailing_array2<TypeEnumT<T>, EnumT<T>> {
	typedef trailing_array<TypeEnumT<T>, EnumT<T>> B;
public:
	static const FLAGS DISCRETE = FRAC_ADJUST, CRCIDS = CHR;
	constexpr TypeEnumT(uint8 nbits, FLAGS flags = NONE) : TypeInt(nbits, 0, flags | ENUM) {}
	iso_export uint32			size()						const;
	iso_export uint32			factors(T f[], uint32 num)	const;
	iso_export const EnumT<T>*	biggest_factor(T x)			const;
	void*		operator new(size_t s, int n)	{ return Type::operator new(B::calc_size(s, n)); }
	void		operator delete(void* p, int n) { Type::operator delete(p); }
};

template<typename T, int N> struct TypeEnumTN : TypeEnumT<T>, EnumTN<T, N> {
	using typename Type::FLAGS;
	constexpr TypeEnumTN(uint8 nbits, FLAGS flags = Type::NONE) : TypeEnumT<T>(nbits, flags) {}
};

typedef EnumT<uint32>		Enum;
typedef TypeEnumT<uint32>	TypeEnum;

//-----------------------------------------------------------------------------
//	user type helpers
//-----------------------------------------------------------------------------

class TypeUserSave : public TypeUser {
public:
	static const FLAGS DONTKEEP = UNSTORED0;
	TypeUserSave(tag2 id, const Type* subtype, FLAGS flags = NONE) : TypeUser(id, subtype, flags) {
		if (!(flags & DONTKEEP))
			user_types.Add(this);
	}
};

struct TypeUserComp : public TypeUserSave, trailing_array<TypeUserComp, Element> {
	TypeComposite comp;
	TypeUserComp(tag2 id, uint32 count = 0, FLAGS2 flags = NONE, uint32 log2align = 0) : TypeUserSave(id, &comp, flags), comp(count, flags.second(), log2align) {}
	void*	operator new(size_t size, void* p)				{ return p; }
	void*	operator new(size_t, uint32 n, uint32 defs = 0) { return Type::operator new(calc_size(n) + defs); }
	void	operator delete(void* p, uint32, uint32)		{ Type::operator delete(p); }

	void						Add(const Element& e) { comp.Add(e); }
	iso_export uint32			Add(const Type* type, tag id = 0, bool packed = false) { return comp.Add(type, id, packed); }
	template<typename T> uint32 Add(tag id = 0, bool packed = false) { return comp.Add(getdef<T>(), id, packed); }
};

template<int N> struct TypeUserCompN : public TypeUserComp, ElementN<N> {
	TypeUserCompN(tag2 id, FLAGS2 flags = NONE, uint32 log2align = 0) : TypeUserComp(id, N, flags) {}
	template<typename T, typename...P> TypeUserCompN(tag2 id, FLAGS2 flags, T *dummy, P...p) : TypeUserComp(id, N, flags, log2alignment<T>), ElementN<N>(dummy, p...) {}
};

class TypeUserCallback : public TypeUserSave {
	template<typename T> struct callbacks {
		static void init(void* p, void* physram) { Init((T*)p, physram); }
		static void deinit(void* p) { DeInit((T*)p); }
	};
public:
	void (*init)(void* p, void* physram);
	void (*deinit)(void* p);
	template<typename T> constexpr TypeUserCallback(T*, tag2 id, const Type* subtype, FLAGS flags = NONE)
		: TypeUserSave(id, subtype, flags | INITCALLBACK), init(callbacks<T>::init), deinit(callbacks<T>::deinit) {}
};

template<int B> struct TypeUserEnum : TypeUserSave, trailing_array<TypeUserEnum<B>, EnumT<uint_bits_t<B>>> {
	TypeEnumT<uint_bits_t<B>>	i;
	TypeUserEnum(tag2 id, FLAGS2 flags = NONE) : TypeUserSave(id, &i, flags), i(B, flags.second()) {}
	void*		operator new(size_t s, void* p)	{ return p; }
	void*		operator new(size_t s, int n)	{ return Type::operator new(TypeUserEnum::calc_size(s, n)); }
	void		operator delete(void* p, int n) { Type::operator delete(p); }
};

template<int N, int B> struct TypeUserEnumN : TypeUserEnum<B>, EnumTN<uint_bits_t<B>, N> {
	using typename Type::FLAGS2;
	TypeUserEnumN(tag2 id, FLAGS2 flags = Type::NONE) : TypeUserEnum<B>(id, flags) {}
	template<typename...P> TypeUserEnumN(tag2 id, FLAGS2 flags, P...p) : TypeUserEnum<B>(id, flags), EnumTN<uint_bits_t<B>, N>(p...) {}
};

//-----------------------------------------------------------------------------
//	ptr
//-----------------------------------------------------------------------------

struct Value {
	enum { // flag values
		CRCID		= ISO_BIT(16, 0),	// the id is a crc
		CRCTYPE		= ISO_BIT(16, 1),	// the type is a crc
		EXTERNAL	= ISO_BIT(16, 2),	// this is an external value
		REDIRECT	= ISO_BIT(16, 3),	// this is a ptr to the true value
		HASEXTERNAL = ISO_BIT(16, 4),	// there is an external child
		SPECIFIC	= ISO_BIT(16, 5),
		ISBIGENDIAN = ISO_BIT(16, 6),	// value is stored in bigendian
		ROOT		= ISO_BIT(16, 7),	// this is the root of a loaded ib
		ALWAYSMERGE = ISO_BIT(16, 8),
		WEAKREFS	= ISO_BIT(16, 9),	// there are weakrefs to this value
		EXTREF		= ISO_BIT(16, 10),
		MEMORY32	= ISO_BIT(16, 11),	// keep this in 32 bit memory
		TEMP_USER	= ISO_BIT(16, 14),	// temp flag
		PROCESSED	= ISO_BIT(16, 15),	// has been fixed up from ib
#ifdef ISO_BIGENDIAN
		NATIVE = ISBIGENDIAN,
#else
		NATIVE = 0,
#endif
		FROMBIN = PROCESSED,
	};
	tag1			id;
	type_ptr		type;
	uint32			user;
	uint16			flags;
	atomic<uint16>	refs;

	friend bool _IsType(const Value* v, const tag2& id2) { return v && !!v->type && (v->flags & CRCTYPE ? id2.get_crc32() == (crc32&)v->type : v->type->GetType() == USER && ((TypeUser*)v->type.get())->ID() == id2); }
	friend bool _IsType(const Value* v, const Type* t, int crit = 0) { return Same(!v || v->flags & CRCTYPE ? 0 : (const Type*)v->type, t, crit); }

	Value(tag2 _id, uint16 flags, const Type* type) : type(type), user(0), flags(flags), refs(0) { SetID(_id); }
	Value(tag2 _id, crc32 _type) : user(0), flags(NATIVE | CRCTYPE), refs(0) {
		(crc32&)type = _type;
		SetID(_id);
	}

	void SetID(tag2 id2) {
		if (tag s = id2) {
			id = s;
			flags &= ~CRCID;
		} else {
			id = id2.get_crc32();
			flags |= CRCID;
		}
	}

	iso_export bool Delete();
	void			addref()	{ ++refs; }
	bool			release()	{ return refs-- == 0 && Delete(); }

	tag2			ID()			const { return id.get_tag2(flags & CRCID); }
	const Type*		GetType()		const { return type; }
	uint32			Flags()			const { return flags; }
	bool			IsExternal()	const { return !!(flags & EXTERNAL); }
	bool			IsBin()			const { return (flags & FROMBIN) && !(flags & REDIRECT); }
	bool			HasCRCType()	const { return !!(flags & CRCTYPE); }

	bool			IsID(const tag2& id2)				const { return ID() == id2; }
	bool			IsType(const tag2& id2)				const { return _IsType(this, id2); }
	bool			IsType(const Type* t, int crit = 0)	const { return _IsType(this, t, crit); }
	template<typename T2> bool IsType(int crit = 0)		const { return _IsType(this, getdef<T2>(), crit); }
};

template<int B> void* MakeRawPtrExternal(const Type* type, const char* filename, tag2 id);
extern template void* MakeRawPtrExternal<32>(const Type* type, const char* filename, tag2 id);
extern template void* MakeRawPtrExternal<64>(const Type* type, const char* filename, tag2 id);

template<typename D, typename S> struct		_iso_cast			{ static inline D f(S s) { return static_cast<D>(s); } };
template<typename D, typename S> inline D	iso_cast(S s)		{ return _iso_cast<D, S>::f(s); }
template<typename D> inline D				iso_cast(void* s)	{ return (D)s; }

typedef ptr_type<void, 32>::type	user_t;

template<typename T, int B> class ptr {
	template<class T2, int N2> friend class ptr;

protected:
	typedef typename ptr_type<holder<T>, B>::type P;

	struct C : Value, holder<T> {
		static const uint16 vflags = Value::NATIVE | (B == 32 ? Value::MEMORY32 : 0);
//		C(tag2 id, const Type* type) : Value(id, vflags, type) {}
		template<typename... PP> C(tag2 id, const Type* type, PP&&... pp) : Value(id, vflags, type), holder<T>(forward<PP>(pp)...) {}
	};

	P p;

	void addref() const {
		if (!!p)
			static_cast<C*>(p.get())->addref();
	}
	bool release() { return !!p && static_cast<C*>(p.get())->release(); }
	void set(holder<T>* _p) {
		release();
		p = _p;
	}
	void set_ref(holder<T>* _p) {
		if (_p)
			static_cast<C*>(_p)->addref();
		set(_p);
	}
	holder<T>*	detach()				{ holder<T> *r = p; p = 0; return r; }
	template<typename T2> operator T2**() const; // hide this
public:
	typedef T subtype;

	static Value*	Header(T* p) { return static_cast<C*>((holder<T>*)p); }
	static ptr<T, B> Ptr(T* p) {
		P t = (holder<T>*)p;
		return (ptr<T, B>&)t;
	}

	ptr() 													: p(0) 					{}
	explicit ptr(tag2 id)									: p(allocate<B>::allocator().template make<C>(id, getdef<T>()))	{}
	ptr(const ptr &p2)										: p(p2.p)				{ addref(); }
	template<typename T2, int B2> ptr(const ptr<T2,B2> &p2)	: p((holder<T>*)(T*)p2)	{ addref(); }
	template<typename...PP>	ptr(tag2 id, PP&&...pp)			: p(allocate<B>::allocator().template make<C>(id, getdef<T>(), forward<PP>(pp)...)) {}
	ptr(ptr &&p2)											: p(p2.detach()) 		{}

	~ptr()														{ release();}

	ptr&										operator=(const ptr &p2)		{ set_ref(p2.p); return *this; }
	template<typename T2, int B2> ptr<T2, B>&	operator=(const ptr<T2, B2> &p2){ set_ref((holder<T>*)iso_cast<T*>(p2.get())); return (ptr<T2,B>&)*this; }
	ptr&										operator=(ptr &&p2)				{ set(p2.detach()); return *this; }
	template<typename T2, int B2> ptr<T2, B>&	operator=(ptr<T2, B2> &&p2)		{ set((holder<T>*)iso_cast<T*>(&*p2.detach())); return (ptr<T2,B>&)*this; }

	ptr&					Create(tag2 id = tag2())			{ set(allocate<B>::allocator().template make<C>(id, getdef<T>())); return *this; }
	template<typename T2> ptr& Create(tag2 id, const T2& t)		{ set(allocate<B>::allocator().template make<C>(id, getdef<T>(), t)); return *this; }
	ptr& CreateExternal(const char* filename, tag2 id = tag2())	{ set((holder<T>*)MakeRawPtrExternal<B>(getdef<T>(), filename, id)); return *this; }
	bool					Clear()								{ bool ret = release(); p = 0; return ret; }

	Value*					Header()				const	{ return static_cast<C*>(p.get()); }
	const Type*				GetType()				const	{ return p && !Header()->HasCRCType() ? Header()->type : 0; }
	tag2					ID()					const	{ return p ? Header()->ID() : tag2(); }
	const char*				External()				const	{ return p && Header()->IsExternal() ? (const char*)&*p : NULL; }
	uint32					UserInt()				const	{ return p ? Header()->user : 0; }
	force_inline void*		User()					const	{ return p ? ((user_t&)Header()->user).get() : 0; }
	uint32					Flags()					const	{ return p ? Header()->flags : 0; }
	bool					IsExternal()			const	{ return p && Header()->IsExternal(); }
	bool					IsBin()					const	{ return p && Header()->IsBin(); }

	bool					HasCRCType()			const	{ return p && Header()->HasCRCType(); }
	bool					HasCRCID()				const	{ return p && Header()->HasCRCID(); }
	bool					IsID(tag2 id)			const	{ return p && Header()->IsID(id); }
	bool					IsType(tag2 id)			const	{ return p && Header()->IsType(id); }
	bool					IsType(const Type* t, int crit = 0)	const { return _IsType(Header(), t, crit); }
	template<typename T2> bool	IsType(int crit = 0) const	{ return IsType(getdef<T2>(), crit); }

	uint32&					UserInt()						{ return Header()->user; }
	user_t&					User()							{ return (user_t&)Header()->user; }
	void					SetFlags(uint32 f)		const	{ if (p) Header()->flags |= f;	}
	void					ClearFlags(uint32 f)	const	{ if (p) Header()->flags &= ~f; }
	bool					TestFlags(uint32 f)		const	{ return p && Header()->flags & f; }
	void					SetID(tag2 id)			const	{ if (p) Header()->SetID(id); }

	T*						get()					const	{ return &*p; }
							operator const T*()		const	{ return &*p; }
	template<typename T2>	operator T2*()			const	{ return iso_cast<T2*>(&*p); }
	typename T_traits<T>::ref	operator*()			const	{ return *p; }
	T*						operator->()			const	{ return &*p; }
	bool					operator!()				const	{ return !p; }
	explicit operator		bool()					const	{ return !!p; }

	template<typename T2> bool operator==(const ptr<T2>& p2)	const { return (void*)p == (void*)p2.p; }
	template<typename T2> bool operator<(const ptr<T2>& p2)		const { return (void*)p < (void*)p2.p; }
};

template<typename T>	using ptr64			= ptr<T, 64>;
template<typename T>	using ptr_machine	= ptr<T, PTRBITS>;

inline void*					GetUser(const void* p)	{ return p ? ((user_t&)((Value*)p - 1)->user).get() : 0; }
inline user_t&					GetUser(void* p)		{ return (user_t&)((Value*)p - 1)->user; }
template<typename T> Value*		GetHeader(T* p)			{ return ptr<T>::Header(p); }

template<int B, typename T>			ptr<T, B>			GetPtr(T* p)		{ return ptr<T, B>::Ptr(p); }
template<int B, typename T>			ptr<T, B>			GetPtr(const T* p)	{ return ptr<T, B>::Ptr((T*)p); }
template<typename T>				ptr_machine<T>&		GetPtr(T*&& p)		{ return (ptr_machine<T>&)p; }
template<typename T>				ptr_machine<T>&		GetPtr(T*& p)		{ return (ptr_machine<T>&)p; }
template<typename T>				ptr_machine<T>&		GetPtr(const T*& p)	{ return (ptr_machine<T>&)p; }

template<int B>						void*				MakeRawPtrSize(const Type* type, tag2 id, uint32 size);
extern template						void*				MakeRawPtrSize<32>(const Type* type, tag2 id, uint32 size);
extern template						void*				MakeRawPtrSize<64>(const Type* type, tag2 id, uint32 size);
template<int B>						void*				MakeRawPtr(const Type* type, tag2 id = tag2())						{ return MakeRawPtrSize<B>(type, id, type->GetSize()); }

template<int B, typename T>			ptr<T, B>			MakePtr(tag2 id)													{ return ptr<T, B>(id); }
template<int B, typename T, int N>	ptr<T[N], B>		MakePtr(tag2 id, const T (&t)[N])									{ return ptr<T[N], B>(id, (holder<T[N]>&)t); }
template<int B, int N>				ptr<const char*, B>	MakePtr(tag2 id, const char (&t)[N])								{ return ptr<const char*, B>(id, t); }
template<int B> inline				ptr<void, B>		MakePtr(const Type* type, tag2 id = tag2())							{ return GetPtr<B>(MakeRawPtrSize<B>(type, id, type->GetSize())); }
template<int B> inline				ptr<void, B>		MakePtrSize(const Type* type, tag2 id, uint32 size)					{ return GetPtr<B>(MakeRawPtrSize<B>(type, id, size)); }
template<int B, typename T> inline	ptr<T, B>			MakePtrSize(tag2 id, uint32 size)									{ return GetPtr<B>((T*)MakeRawPtrSize<B>(getdef<T>(), id, size)); }
template<int B> inline				ptr<void, B>		MakePtrExternal(const Type* type, const char* fn, tag2 id = tag2()) { return GetPtr<B>(MakeRawPtrExternal<B>(type, fn, id)); }
template<int B, typename T> inline	ptr<T, B>			MakePtrExternal(const char* filename, tag2 id = tag2())				{ return GetPtr<B>((T*)MakeRawPtrExternal<B>(getdef<T>(), filename, id)); }
template<int B> inline				ptr<void, B>		MakePtrIndirect(ptr<void, B> p, const Type* type) {
	ptr<ptr<void, B>, B> p2(p.ID(), p);
	p2.Header()->type = type;
	p2.SetFlags(Value::REDIRECT | (p.Flags() & Value::HASEXTERNAL));
	return p2;
}
template<int B, typename T> inline ptr<T, B> MakePtrIndirect(ptr<void, B> p) { return MakePtrIndirect(p, getdef<T>()); }

template<int B> ptr<void, B>	iso_nil = ptr<void, B>();

template<typename T>				ptr<T>				MakePtr(tag2 id)													{ return MakePtr<32, T>(id); }
template<typename T, int N>			ptr<T[N]>			MakePtr(tag2 id, const T (&t)[N])									{ return ptr<T[N], 32>(id, (holder<T[N]>&)t); }
template<int N>						ptr<const char*>	MakePtr(tag2 id, const char (&t)[N])								{ return ptr<const char*>(id, t); }
template<typename T> inline			ptr<T>				MakePtrExternal(const char* fn, tag2 id = tag2())					{ return MakePtrExternal<32, T>(fn, id); }
template<typename T> inline			ptr<T>				MakePtrIndirect(ptr<void> p)										{ return MakePtrIndirect<32, T>(p); }
template<typename T>				ptr<T>				MakePtrCheck(tag2 id, const T& t)									{ if (!&t) return iso_nil<32>; return MakePtr<32>(id, t); }

inline								ptr<void>			MakePtr(const Type* type, tag2 id = tag2())							{ return MakePtr<32>(type, id); }
inline								ptr<void>			MakePtrSize(const Type* type, tag2 id, uint32 size)					{ return MakePtrSize<32>(type, id, size); }
inline								ptr<void>			MakePtrExternal(const Type* type, const char* fn, tag2 id = tag2())	{ return MakePtrExternal<32>(type, fn, id); }
inline								ptr<void>			MakePtrIndirect(ptr<void> p, const Type* type)						{ return MakePtrIndirect<32>(p, type); }

#ifdef USE_RVALUE_REFS
template<int B, typename T, typename U> auto	MakeRawPtr(const Type* type, tag2 id, U&& u)	{ return new(MakeRawPtrSize<B>(type, id, type->GetSize())) T(forward<U>(u)); }
template<int B, typename T, typename U> auto	MakeRawPtr(tag2 id, U&& u)					{ return new(MakeRawPtrSize<B>(getdef<T>(), id, sizeof(T))) T(forward<U>(u)); }

template<int B, typename T>			auto		MakeRawPtr(const Type* type, tag2 id, T&& t)	{ return MakeRawPtr<B,T,T>(type, id, forward<T>(t)); }
template<int B, typename T>			auto		MakeRawPtr(tag2 id, T&& t)					{ return MakeRawPtr<B,T,T>(id, forward<T>(t)); }

template<int B, typename T>			auto		MakePtr(tag2 id, T&& t)						{ return ptr<noconst_t<noref_t<T>>, B>(id, forward<T>(t)); }
template<typename T>				auto		MakePtr(tag2 id, T&& t)						{ return ptr<noconst_t<noref_t<T>>>(id, forward<T>(t)); }
template<int B, typename T>			auto		MakePtr(const Type* type, tag2 id, T &&t)	{ return GetPtr<B>(MakeRawPtr<B>(type, id, forward<T>(t))); }
template<typename T>				auto		MakePtr(const Type* type, tag2 id, T &&t)	{ return GetPtr<32>(MakeRawPtr<32>(type, id, forward<T>(t))); }
#else
template<int B, typename T>			ptr<T, B>	MakePtr(tag2 id, const T& t) { return ptr<T, B>(id, t); }
template<typename T>				ptr<T>		MakePtr(tag2 id, const T& t) { return MakePtr<32>(id, t); }
#endif

//-----------------------------------------------------------------------------
//	weak
//-----------------------------------------------------------------------------

struct weak : public e_treenode<weak, true> {
	typedef e_tree<weak, less>	tree_t;
	struct A {
		Value* get() { return (Value*)this - 1; }
	};

	typedef locked_s<tree_t, Mutex> locked_tree;

	static locked_tree tree() {
		static tree_t		tree;
		static Mutex		m;
		return locked_tree(tree, m);
	}

	static weak* get(void* _p) {
		A* p = (A*)_p;
		if (p->get()->flags & Value::WEAKREFS) {
			if (weak* w = find(tree().get(), p)) {
				w->addref();
				return w;
			}
		}
		return new weak(p);
	}
	A*				p;
	atomic<uint32>	weak_refs;

	operator void*() const { return p; }
	void addref() { weak_refs++; }
	void release() {
		if (weak_refs-- == 0)
			delete this;
	}

	void set(A* _p) {
		if (p) {
			p->get()->flags &= ~Value::WEAKREFS;
			tree().get().remove(this);
		}
		if (p = _p) {
			p->get()->flags |= Value::WEAKREFS;
			tree().get().insert(this);
		}
	}

	static void remove(const Value* v) {
		if (weak* w = find(tree().get(), (A*)(v + 1))) {
			w->p = 0;
			tree().get().remove(w);
		}
	}

	weak() : p(0), weak_refs(0) {}

	weak(A* _p) : p(_p), weak_refs(0) {
		if (p) {
			p->get()->flags |= Value::WEAKREFS;
			tree().get().insert(this);
		}
	}
	~weak() {
		if (p) {
			p->get()->flags &= ~Value::WEAKREFS;
			tree().get().remove(this);
		}
	}
};

template<class T> class WeakRef : weak {
public:
	WeakRef() {}
	WeakRef(const ptr_machine<T>& p) : weak((A*)(T*)p) {}
	WeakRef& operator=(const ptr_machine<T>& p) {
		set((A*)(T*)p);
		return *this;
	}
	operator T*()				const { return (T*)p; }
	operator ptr<T>()			const { return ptr<T>::Ptr(*this); }
	operator ptr_machine<T>()	const { return ptr_machine<T>::Ptr(*this); }
};

template<class T> class WeakPtr {
	weak* p;

public:
	WeakPtr() : p(0) {}
	WeakPtr(const ptr_machine<T>& p2) { p = weak::get(p2); }
	WeakPtr(const WeakPtr<T>& p2) {
		if (p = p2.p)
			p->addref();
	}
	WeakPtr(WeakPtr<T>&& p2) {
		p	= p2.p;
		p2.p = 0;
	}
	~WeakPtr() {
		if (p)
			p->release();
	}
	void operator=(const ptr_machine<T>& p2) {
		if (p)
			p->release();
		p = weak::get(p2);
	}
	void operator=(const WeakPtr<T>& p2) {
		if (p)
			p->release();
		if (p = p2.p)
			p->addref();
	}

	operator T*()				const { return p ? (T*)p->p : 0; }
	operator ptr_machine<T>()	const { return ptr_machine<T>::Ptr(*this); }
};

//-----------------------------------------------------------------------------
//	Array
//-----------------------------------------------------------------------------

template<class T, int N> class Array {
	T t[N];

public:
	typedef T			subtype;
	typedef T*			iterator;
	typedef const T*	const_iterator;

	operator T*() { return t; }
	operator const T*() const { return t; }

	uint32			size() const { return N; }
	const_iterator	begin() const { return &t[0]; }
	const_iterator	end() const { return &t[N]; }
	iterator		begin() { return &t[0]; }
	iterator		end() { return &t[N]; }
};

template<class T, int N1, int N2> struct _iso_cast<Array<T, N1>*, Array<T, N2>*> {
	ISO_COMPILEASSERT(N1 <= N2);
	static Array<T, N1>* f(Array<T, N2>* s) { return 0; }
};

//-----------------------------------------------------------------------------
//	OpenArray
//-----------------------------------------------------------------------------

struct OpenArrayHead {
	uint32 max;
	uint32 count : 31, bin : 1;

	//	void operator delete(void*);

	friend uint32 GetCount(const OpenArrayHead* h)	{ return h ? h->count : 0; }
	friend uint8* GetData(const OpenArrayHead* h)	{ return h ? (uint8*)(h + 1) : 0; }
	friend memory_block GetMemory(const OpenArrayHead* h, uint32 subsize)	{ return {GetData(h), subsize * GetCount(h)}; }

	OpenArrayHead(uint32 _max, uint32 _count) : max(_max), count(_count), bin(0) {}
	void			SetCount(uint32 _count)						{ count = _count; }
	void			SetBin(bool _bin)							{ bin = _bin; }
	void*			GetElement(int i, uint32 subsize)	const	{ return (uint8*)(this + 1) + subsize * i; }
	iso_export bool Remove(uint32 subsize, uint32 align, int i);

	template<typename T> auto	Data() const				{ return make_range_n((T*)(this + 1), count); }
	template<typename T> auto	Data(uint32 subsize) const	{ return make_range_n(stride_iterator<T>((T*)(this + 1), subsize), count); }

	static OpenArrayHead*		Get(void* p)				{ return p ? (OpenArrayHead*)p - 1 : 0; }
	static const OpenArrayHead*	Get(const void* p)			{ return p ? (const OpenArrayHead*)p - 1 : 0; }
};

inline OpenArrayHead *TypeOpenArray::get_header(const void* data) const {
	return OpenArrayHead::Get(ReadPtr(data));
}
inline uint32 TypeOpenArray::get_count(const void* data) const {
	return GetCount(get_header(data));
}
inline memory_block TypeOpenArray::get_memory(const void* data) const {
	return GetMemory(get_header(data), subsize);
}


template<int B> struct OpenArrayAlloc {
	typedef allocate<B>		A;
	static size_t			alloc_size(uint32 subsize, uint32 align, uint32 n)	{ return iso::align(sizeof(OpenArrayHead) + subsize * n, align); }
	static void*			adjust(OpenArrayHead* p, uint32 align)				{ return p && align > sizeof(OpenArrayHead) ? (uint8*)(p + 1) - align : (uint8*)p; }
	static OpenArrayHead*	adjust(void* p, uint32 align)						{ return p && align > sizeof(OpenArrayHead) ? (OpenArrayHead*)((uint8*)p + align) - 1 : (OpenArrayHead*)p; }

	static void Destroy(OpenArrayHead* h, uint32 align) {
		if (h && !h->bin)
			A::free(adjust(h, align));
	}

	static OpenArrayHead* Create(uint32 subsize, uint32 align, uint32 max, uint32 count) {
		ISO_ASSERT(count < 0x80000000u);
		return max ? ISO_VERIFY(new (adjust(A::alloc(alloc_size(subsize, align, max), align), align)) OpenArrayHead(max, count)) : 0;
	}
	static OpenArrayHead* Resize(OpenArrayHead* h, uint32 subsize, uint32 align, uint32 max, uint32 count) {
		ISO_ASSERT(count < 0x80000000u);
		return ISO_VERIFY(new (adjust(A::realloc(adjust(h, align), alloc_size(subsize, align, max), align), align)) OpenArrayHead(max, count));
	}
	iso_export static OpenArrayHead* Create(OpenArrayHead* h, uint32 subsize, uint32 align, uint32 count);
	iso_export static OpenArrayHead* Create(OpenArrayHead* h, const Type* type, uint32 count);
	iso_export static OpenArrayHead* Resize(OpenArrayHead* h, uint32 subsize, uint32 align, uint32 count, bool clear = false);
	iso_export static OpenArrayHead* Append(OpenArrayHead* h, uint32 subsize, uint32 align, bool clear);
	iso_export static OpenArrayHead* Insert(OpenArrayHead* h, uint32 subsize, uint32 align, int i, bool clear = false);
};

template<int B> ptr<void, B> &FindByType(ptr<void, B> *i, ptr<void, B> *e, const Type *type, int crit) {
	for (; i != e; ++i) {
		if (i->IsType(type, crit))
			return *i;
	}
	return iso_nil<B>;
}

template<int B> ptr<void, B> &FindByType(ptr<void, B> *i, ptr<void, B> *e, tag2 id) {
	for (; i != e; ++i) {
		if (i->IsType(id))
			return *i;
	}
	return iso_nil<B>;
}

template<typename T, int B = 32> class OpenArrayView {
protected:
	typename ptr_type<T, B>::type	p;
	OpenArrayView(T *p) : p(p) {}
public:
	typedef T				*iterator;
	typedef OpenArrayView	view_t;

	static OpenArrayView Ptr(T* p)	{ return p; }

	OpenArrayView() {}
	uint32			Count()				const	{ return GetCount(OpenArrayHead::Get(p)); }
	operator T*()						const	{ return p; }
	int	GetIndex(tag2 id, int from = 0)	const	{ return _GetIndex(id, *this, from); }
	T&				Index(int i)				{ return p[i]; }
	const T&		Index(int i)		const	{ return p[i]; }
	T&				operator[](int i)			{ return p[i]; }
	const T&		operator[](int i)	const	{ return p[i]; }
	T&				operator[](tag2 id)			{ int i = _GetIndex(id, *this, 0); return i < 0 ? (T&)iso_nil<PTRBITS> : (*this)[i]; }
	const T&		operator[](tag2 id)	const	{ int i = _GetIndex(id, *this, 0); return i < 0 ? (T&)iso_nil<PTRBITS> : (*this)[i]; }

	T&						FindByType(tag2 id)						{ return ISO::FindByType(begin(), end(), id); }
	T&						FindByType(const Type* t, int crit = 0)	{ return ISO::FindByType(begin(), end(), t, crit); }
	template<typename U> ptr<U, B>	FindByType(int crit = 0)		{ return FindByType(getdef<U>(), crit); }

	size_t			size()		const	{ return Count(); }
	uint32			size32()	const	{ return Count(); }
	auto			begin()		const	{ return p.get(); }
	auto			end()		const	{ return p.get() + size(); }
	auto			begin()				{ return p.get(); }
	auto			end()				{ return p.get() + size(); }

	const T&		front()		const	{ return p.get()[0]; }
	const T&		back()		const	{ return p.get()[size() - 1]; }
	T&				front()				{ return p.get()[0]; }
	T&				back()				{ return p.get()[size() - 1]; }

	constexpr int	index_of(const T *e)	const	{ return e ? int(e - begin()) : -1; }
	constexpr int	index_of(const T &e)	const	{ return int(&e - begin()); }
	constexpr bool	contains(const T *e)	const	{ return e && e >= begin() && e < end(); }
	constexpr bool	contains(const T &e)	const	{ return &e >= begin() && &e < end(); }

	operator	memory_block()	const	{ return memory_block(p.get(), Count() * sizeof(T)); }
};

template<int B> class OpenArrayView<void, B> {
protected:
	typename ptr_type<void, B>::type	p;
	OpenArrayView(void *p) : p(p) {}
	OpenArrayHead* Header() const { return OpenArrayHead::Get(p); }

public:
	uint32		Count() const { return GetCount(OpenArrayHead::Get(p)); }
	void*		GetElement(int i, uint32 subsize) const { return (uint8*)p.get() + i * subsize; }
	template<typename T> operator T*() const { return (T*)p.get(); }
};

template<typename T, int B = 32> class OpenArray : public OpenArrayView<T, B> {
	typedef OpenArrayView<T, B>	BASE;
	using BASE::p;
	enum { align = alignof(T) };

	OpenArray(BASE b)	: BASE(b) {}
	OpenArray(const OpenArray &b)	: BASE((T*)GetData(OpenArrayAlloc<B>::Create((uint32)sizeof(T), align, b.Count(), b.Count()))) {
		copy_new_n(b.begin(), p.get(), b.Count());
	}
public:
	static OpenArray Ptr(T* p)	{ return BASE::Ptr(p); }
	BASE	View() const		{ return *this; }

	OpenArray() {}
	OpenArray(OpenArray &&b)		: BASE(exchange(b.p, nullptr)) {}
	OpenArray(uint32 count)			: BASE((T*)GetData(OpenArrayAlloc<B>::Create((uint32)sizeof(T), align, count, count))) {
		fill_new_n(p.get(), count);
	}
	template<typename C, typename = enable_if_t<has_begin_v<C>>> OpenArray(const C& c) {
		using iso::begin;
		uint32	count = num_elements32(c);
		p	= (T*)GetData(OpenArrayAlloc<B>::Create(sizeof(T), align, count, count));
		copy_new_n(begin(c), p.get(), count);
	}
	template<typename R, typename=is_reader_t<R>>	OpenArray(R &&r, size_t n) : OpenArray(n) { readn(r, BASE::begin(), n); }

	OpenArray(const const_memory_block &mem) : OpenArray(make_range<const T>(mem)) {}
	OpenArray(const memory_block &mem) : OpenArray(make_range<const T>(mem)) {}
	OpenArray(const malloc_block &mem) : OpenArray(make_range<const T>(mem)) {}

	~OpenArray() { Clear(); }

	OpenArray&	operator=(OpenArray &&b)		{ swap(*this, b); return *this; }
	//OpenArray&	operator=(const OpenArray &b)	{
	//	Create(b.Count(), false);
	//	construct_array_it(p.get(), b.Count(), b.begin());
	//}

	OpenArray	Dup() const {
		return *this;
	}

	OpenArray& Create(uint32 count, bool init = true) {
		T*				t = p;
		OpenArrayHead*	h = OpenArrayHead::Get(t);
		destruct(t, GetCount(h));
		h = OpenArrayAlloc<B>::Create(h, (uint32)sizeof(T), align, count);
		p = t = (T*)GetData(h);
		if (count && init)
			fill_new_n(t, count);
		return *this;
	}
	OpenArray& Resize(uint32 count) {
		T*				t		= p;
		OpenArrayHead*	h		= OpenArrayHead::Get(t);
		size_t			count0	= GetCount(h);
		if (count != count0) {
			if (count < count0) {
				destruct(t + count, count0 - count);
				h->SetCount(count);
			} else {
				if (h = OpenArrayAlloc<B>::Resize(h, (uint32)sizeof(T), align, count)) {
					p = t = (T*)GetData(h);
					fill_new_n(t + count0, count - count0);
				}
			}
		}
		return *this;
	}
	void Remove(int i) {
		if (T* t = p) {
			OpenArrayHead*	h = OpenArrayHead::Get(t);
			int				n = GetCount(h) - 1;
			for (T *m = t + i, *e = t + n; m < e; m++)
				m[0] = m[1];
			destruct(t + n, 1);
			h->SetCount(n);
		}
	}
	void Remove(T* i) { Remove(i - p); }

	void Clear() {
		if (T* t = p) {
			OpenArrayHead* h = OpenArrayHead::Get(t);
			destruct(t, GetCount(h));
			OpenArrayAlloc<B>::Destroy(h, align);
			p = 0;
		}
	}
	T* _Append() {
		T*				t = p;
		OpenArrayHead*	h = OpenArrayHead::Get(t);
		h = OpenArrayAlloc<B>::Resize(h, (uint32)sizeof(T), align, GetCount(h) + 1);
		p = t = (T*)GetData(h);
		return t + GetCount(h) - 1;
	}
	template<typename T2> T& Append(T2 &&t) {
		return *new (_Append()) T(forward<T2>(t));
	}
	T& Append() {
		return *new (_Append()) T;
	}
	template<typename T2> T& Insert(int i, const T2& t) {
		T *p = _Append(), *b = BASE::begin() + i;
		if (p == b) {
			return *new (p) T(t);
		} else {
			new (p) T(p[-1]);
			while (--p != b)
				p[0] = p[-1];
			return p[0] = t;
		}
	}

	T*	detach() { return exchange(p, nullptr); }

	template<typename T2> T& push_back(const T2& t) { return *new(_Append()) T(t); }
	template<typename R> bool read(R &r, size_t n)		{ Resize((uint32)n); return readn(r, BASE::begin(), n); }
	friend void swap(OpenArray &a, OpenArray &b)		{ swap(a.p, b.p); }
	friend OpenArray Duplicate(const OpenArray& a)		{ return a; }
};

template<int B> class OpenArray<void, B> : public OpenArrayView<void, B> {
	typedef OpenArrayView<void, B>	BASE;
	using BASE::p;

	OpenArray(const OpenArray &b)	: BASE(GetData(OpenArrayAlloc<B>::Create(1, 1, b.Count(), b.Count()))) {}
public:
	OpenArray(uint32 subsize, uint32 count, uint32 align = 4) : BASE(GetData(OpenArrayAlloc<B>::Create(subsize, align, count, count))) {}
	void Clear(uint32 align) {
		OpenArrayAlloc<B>::Destroy(OpenArrayHead::Get(p), align);
		p = 0;
	}
	OpenArray& Create(uint32 subsize, uint32 align, uint32 count) {
		p = GetData(OpenArrayAlloc<B>::Create(OpenArrayHead::Get(p), subsize, align, count));
		return *this;
	}
	OpenArray& Resize(uint32 subsize, uint32 align, uint32 count) {
		p = GetData(OpenArrayAlloc<B>::Resize(OpenArrayHead::Get(p), subsize, align, count));
		return *this;
	}
	void* Append(int subsize, uint32 align) {
		OpenArrayHead* h = OpenArrayAlloc<B>::Append(OpenArrayHead::Get(p), subsize, align, false);
		p = GetData(h);
		return h->GetElement(GetCount(h) - 1, subsize);
	}
	void Remove(uint32 subsize, uint32 align, int i)	{ return OpenArrayHead::Get(p)->Remove(subsize, align, i, false); }

	OpenArray(const Type* type, uint32 count)			: OpenArray(type->GetSize(), count, type->GetAlignment()) {}
	OpenArray&	Create(const Type* type, uint32 count)	{ return Create(type->GetSize(), type->GetAlignment(), count); }
	OpenArray&	Resize(const Type* type, uint32 count)	{ return Resize(type->GetSize(), type->GetAlignment(), count); }
	void		Remove(const Type* type, int i)			{ return Remove(type->GetSize(), type->GetAlignment(), i); }
	void*		Append(const Type* type)				{ return Append(type->GetSize(), type->GetAlignment()); }
};

template<typename T, int B> inline size_t num_elements(const OpenArrayView<T, B>& t) { return t.Count(); }

template<int B, typename C> auto	MakePtrArray(tag id, const C& c)				{ return ptr<OpenArray<typename container_traits<C>::element>, B>(id, c); }
template<typename C> auto			MakePtrArray(tag id, const C& c)				{ return MakePtrArray<32>(id, c); }
template<typename C, int B> C		LoadPtrArray(const ptr<void, B>& p)				{ return *(const OpenArray<typename container_traits<C>::element>*)p; }
template<typename C, int B> void	LoadPtrArray(const ptr<void, B>& p, C& dest)	{ dest = *(const OpenArray<typename container_traits<C>::element>*)p; }

//-----------------------------------------------------------------------------
//	duplicate/copy/flip/compare blocks of data
//-----------------------------------------------------------------------------

enum DUPF {
	DUPF_DEEP			= 1 << 0,
	DUPF_CHECKEXTERNALS = 1 << 1,
	DUPF_NOINITS		= 1 << 2,
	DUPF_EARLYOUT		= 1 << 3,	// stop checking when found an external
	DUPF_DUPSTRINGS		= 1 << 4,	// duplicate strings, even if not ALLOC
	DUPF_MEMORY32		= 1 << 5,
};

iso_export int		_Duplicate(const Type* type, void* data, int flags = 0, void* physical_ram = 0);

template<int B> void* _DuplicateT(tag2 id, const Type* type, void* data, int flags = 0, uint16 pflags = 0) {
	if (!data)
		return NULL;

	void	*p = MakeRawPtr<B>(type, id);
	GetHeader(p)->flags |= pflags;

	memcpy(p, data, type->GetSize());
	int	ret = _Duplicate(type, p, flags);
	if (ret & 2)
		GetHeader(p)->flags |= Value::HASEXTERNAL;
	return p;
}

inline ptr_machine<void>	Duplicate(tag2 id, const Type* type, void* data, int flags = 0, uint16 pflags = 0) {
	return GetPtr(flags & DUPF_MEMORY32
		? _DuplicateT<32>(id, type, data, flags, pflags)
		: _DuplicateT<64>(id, type, data, flags, pflags)
	);
}

template<class T> inline T DuplicateData(const T& t, bool deep = false) {
	T t2 = t;
	_Duplicate(getdef<T>(), &t2, deep ? DUPF_DEEP : 0);
	return t2;
}

template<class T, int B> inline ptr<T, B> Duplicate(tag2 id, const ptr<T, B>& p, bool deep = false) {
	auto	ptrflags	= p.Flags();
	auto	pflags		= ptrflags & (Value::REDIRECT | Value::SPECIFIC | Value::ALWAYSMERGE);
	auto	dupflags 	= (deep ? DUPF_DEEP : 0) | ((ptrflags & Value::HASEXTERNAL) ? DUPF_CHECKEXTERNALS : 0);
	return GetPtr<B>((T*)(
		p.Flags() & Value::MEMORY32
			? _DuplicateT<32>(id, p.GetType(), p, dupflags, pflags)
			: _DuplicateT<B>(id, p.GetType(), p, dupflags, pflags)
	));
}
template<class T, int B> inline ptr<T, B> Duplicate(const ptr<T, B>& p, bool deep = false) {
	return Duplicate(p.ID(), p, deep);
}

template<int B, class T, int B2> inline ptr<T, B> Duplicate(tag2 id, const ptr<T, B2>& p, bool deep = false) {
	return GetPtr<B>((T*)_DuplicateT<B>(id, p.GetType(), p, (deep ? DUPF_DEEP : 0) | ((p.Flags() & Value::HASEXTERNAL) ? DUPF_CHECKEXTERNALS : 0), p.Flags() & (Value::REDIRECT | Value::SPECIFIC | Value::ALWAYSMERGE)));
}
template<int B, class T, int B2> inline ptr<T, B> Duplicate(const ptr<T, B2>& p, bool deep = false) {
	return Duplicate<B>(p.ID(), p, deep);
}


iso_export bool CompareData(const Type* type, const void* data1, const void* data2, int flags = 0);
iso_export void CopyData(const Type* type, const void* srce, void* dest);
iso_export void FlipData(const Type* type, const void* srce, void* dest);
iso_export bool CompareData(ptr<void> p1, ptr<void> p2, int flags = 0);
iso_export void SetBigEndian(ptr<void> p, bool big);
iso_export bool CheckHasExternals(const ptr<void>& p, int flags = DUPF_EARLYOUT, int depth = 64);

template<class T> inline void CopyData(const T* srce, T* dest) { CopyData(getdef<T>(), srce, dest); }
template<class T> inline void FlipData(const T* srce, void* dest) { FlipData(getdef<T>(), srce, dest); }
template<class T> inline bool CompareData(const T* data1, const T* data2, int flags = 0) { return CompareData(getdef<T>(), data1, data2, flags); }

//-----------------------------------------------------------------------------
//	def type definition creators
//-----------------------------------------------------------------------------
// suffixes for composites:
//	P	param_element
//	T	template parameter
// general suffixes:
//	X	supply name (instead of using stringified typename)
//	F	override default flag field
//	T	supply type (instead of using actual type)

// simple types
#define ISO_TYPEDEF(a, b)					ISO::TypeUserSave def_##a(STRINGIFY(a), ISO::getdef<b>())
#define ISO_EXTTYPE(a)						template<> ISO::Type* ISO::getdef<a>()
#define _ISO_DEFSAME(a, b)					struct ISO::def<a> { ISO::Type* operator&() { return getdef<b>(); } }	// make a's def be same as b's
#define ISO_DEFSAME(a, b)					template<> _ISO_DEFSAME(a, b)
#define ISO_DEFPOD(a, b)					ISO_DEFSAME(a, b[sizeof(a) / sizeof(b)])

// user types
#define _ISO_DEFUSER(a, b, n, F)			struct ISO::def<a> : ISO::TypeUserSave { def() : TypeUserSave(n, getdef<b>(), F) {} }
#define ISO_DEFUSERXT(a, T, b, n)			_ISO_DEFUSER(a<T>, b, n, NONE)
#define ISO_DEFUSERX(a, b, n)				template<> _ISO_DEFUSER(a, b, n, NONE)
#define ISO_DEFUSERXF(a, b, n, F)			template<> _ISO_DEFUSER(a, b, n, F)
#define ISO_DEFUSER(a, b)					ISO_DEFUSERX(a, b, STRINGIFY(a))
#define ISO_DEFUSERF(a, b, F)				ISO_DEFUSERXF(a, b, STRINGIFY(a), F)
// if you need parentheses around a
#define _ISO_DEFUSER2(a, b, n, F)			struct ISO::def<NO_PARENTHESES a> : ISO::TypeUserSave { def() : TypeUserSave(n, getdef<b>(), F) {} }

// user types with callbacks
#define ISO_DEFCALLBACKXF(a, b, n, F)		template<> struct ISO::def<a> : ISO::TypeUserCallback { def() : TypeUserCallback((a*)0, n, getdef<b>(), F) {} }
#define ISO_DEFCALLBACKX(a, b, n)			ISO_DEFCALLBACKXF(a, b, n, NONE)
#define ISO_DEFCALLBACK(a, b)				ISO_DEFCALLBACKX(a, b, STRINGIFY(a))

// user types that are arrays of pod
#define ISO_DEFUSERPODXF(a, b, n, F)		ISO_DEFUSERXF(a, b[sizeof(a) / sizeof(b)], n, F)
#define ISO_DEFUSERPODX(a, b, n)			ISO_DEFUSERPODXF(a, b, n, NONE)
#define ISO_DEFUSERPODF(a, b, F)			ISO_DEFUSERPODXF(a, b, STRINGIFY(a), F)
#define ISO_DEFUSERPOD(a, b)				ISO_DEFUSERPODX(a, b, STRINGIFY(a))
#define ISO_DEFCALLBACKPODXF(a, b, n, F)	ISO_DEFCALLBACKXF(a, b[sizeof(a) / sizeof(b)], n, F)
#define ISO_DEFCALLBACKPODX(a, b, n)		ISO_DEFCALLBACKPODXF(a, b, n, NONE)
#define ISO_DEFCALLBACKPODF(a, b, F)		ISO_DEFCALLBACKPODXF(a, b, STRINGIFY(a), F)
#define ISO_DEFCALLBACKPOD(a, b)			ISO_DEFCALLBACKPODX(a, b, STRINGIFY(a))

// virtual and user-virtual types
#define _ISO_DEFVIRT(a)						struct ISO::def<a> : ISO::VirtualT<a> {}
#define _ISO_DEFVIRTF(a, F)					struct ISO::def<a> : ISO::VirtualT<a> { def() : VirtualT<a>(F) {} }
#define ISO_DEFVIRT(a)						template<> _ISO_DEFVIRT(a)
#define ISO_DEFVIRTF(a, F)					template<> _ISO_DEFVIRTF(a, F)
#define ISO_DEFVIRTT(S, T)					_ISO_DEFVIRT(S<T>)
#define ISO_DEFVIRTFT(S, T, F)				_ISO_DEFVIRTF(S<T>, F)
#define ISO_DEFUSERVIRTXT(a, b, n)			template<> struct ISO::def<a> : public ISO::TypeUserSave { VirtualT<b> v; def() : TypeUserSave(n, &v) {} }
#define ISO_DEFUSERVIRTXFT(a, b, n, F)		template<> struct ISO::def<a> : public ISO::TypeUserSave { VirtualT<b> v; def() : TypeUserSave(n, &v), v(F) {} }
#define ISO_DEFUSERVIRTX(a, b)				ISO_DEFUSERVIRTXT(a, a, b)
#define ISO_DEFUSERVIRTXF(a, b, F)			ISO_DEFUSERVIRTXFT(a, a, b, F)
#define ISO_DEFUSERVIRT(a)					ISO_DEFUSERVIRTXT(a, a, STRINGIFY(a))
#define ISO_DEFUSERVIRTF(a, F)				ISO_DEFUSERVIRTXFT(a, a, STRINGIFY(a), F)

// header for composite def
#define _ISO_DEFCOMP(S, N, F)				struct ISO::def<S> : public ISO::TypeCompositeN<N> { typedef S _S, _T; def() : TypeCompositeN<N>(F, log2alignment<S>)
#define ISO_DEFCOMPF(S, N, F)				template<> _ISO_DEFCOMP(S, N, F)
#define ISO_DEFCOMP(S, N)					ISO_DEFCOMPF(S, N, NONE)
//...of templated type
#define ISO_DEFCOMPTF(S, T, N, F)			_ISO_DEFCOMP(S<T>, N, F)
#define ISO_DEFCOMPT(S, T, N)				ISO_DEFCOMPTF(S, T, N, NONE)
//...with param element
#define _ISO_DEFCOMPPF(S, P, N, F)			struct ISO::def<param_element<S, P>> : public ISO::TypeCompositeN<N> { typedef noref_t<S> _S; typedef param_element<S, P> _T; def() : TypeCompositeN<N>(F, log2alignment<_S>)
#define ISO_DEFCOMPPF(S, P, N, F)			template<> _ISO_DEFCOMPPF(S, P, N, F)
#define ISO_DEFCOMPP(S, P, N)				ISO_DEFCOMPPF(S, P, N, NONE)
//...of templated type
#define ISO_DEFCOMPTPF(S, T, P, N, F)		_ISO_DEFCOMPPF(S<T>, P, N, F)
#define ISO_DEFCOMPTP(S, T, P, N)			ISO_DEFCOMPPTF(S, T, P, N, NONE)

// header for user-composite def
#define _ISO_DEFUSERCOMP(S, N, n, F)		struct ISO::def<DEPAREN(S)> : public ISO::TypeUserCompN<N> { typedef DEPAREN(S) _S, _T; def() : TypeUserCompN<N>(n, F, log2alignment<_S>)
#define ISO_DEFUSERCOMPXF(S, N, n, F)		template<> _ISO_DEFUSERCOMP(S, N, n, F)
#define ISO_DEFUSERCOMPX(S, N, n)			ISO_DEFUSERCOMPXF(S, N, n, NONE)
#define ISO_DEFUSERCOMPF(S, N, F)			ISO_DEFUSERCOMPXF(S, N, #S, F)
#define ISO_DEFUSERCOMP(S, N)				ISO_DEFUSERCOMPXF(S, N, #S, NONE)
//...of templated type
#define ISO_DEFUSERCOMPTXF(S, T, N, n, F)	_ISO_DEFUSERCOMP(S<T>, N, n, F)
#define ISO_DEFUSERCOMPTX(S, T, N, n)		ISO_DEFUSERCOMPTXF(S, T, N, n, NONE)
#define ISO_DEFUSERCOMPTF(S, T, N, F)		ISO_DEFUSERCOMPTXF(S, T, N, #S, F)
#define ISO_DEFUSERCOMPT(S, T, N)			ISO_DEFUSERCOMPTXF(S, T, N, #S, NONE)
//...of templated type with S<T>
#define ISO_DEFUSERCOMPT2XF(S, N, n, F)		_ISO_DEFUSERCOMP(S, N, n, F)
#define ISO_DEFUSERCOMPT2X(S, N, n)			ISO_DEFUSERCOMPT2XF(S, N, n, NONE)
#define ISO_DEFUSERCOMPT2F(S, N, F)			ISO_DEFUSERCOMPT2XF(S, N, #S, F)
#define ISO_DEFUSERCOMPT2(S, N)				ISO_DEFUSERCOMPT2XF(S, N, #S, NONE)

// if you need parentheses around S
#define _ISO_DEFUSERCOMPV(S, N, n, F)		struct ISO::def<NO_PARENTHESES S> : public ISO::TypeUserCompN<N> { typedef NO_PARENTHESES S _S, _T; def() : TypeUserCompN<N>(n, F, log2alignment<_S>)

// param element
#define _ISO_DEFUSERCOMPP(S, P, N, n, F)	struct ISO::def<param_element<S, P>> : public ISO::TypeUserCompN<N> { typedef noref_t<S> _S; typedef param_element<S, P> _T; def() : TypeUserCompN<N>(n, F, log2alignment<_S>)
#define ISO_DEFUSERCOMPPXF(S, P, N, n, F)	template<> _ISO_DEFUSERCOMPP(S, P, N, n, F)

#define ISO_DEFUSERCOMPPX(S, P, N, n)		ISO_DEFUSERCOMPPXF(S&, P, N, n, NONE)
#define ISO_DEFUSERCOMPPF(S, P, N, F)		ISO_DEFUSERCOMPPXF(S&, P, N, #S, F)
#define ISO_DEFUSERCOMPP(S, P, N)			ISO_DEFUSERCOMPPXF(S&, P, N, #S, NONE)
//...no ref
#define ISO_DEFUSERCOMPP0X(S, P, N, n)		ISO_DEFUSERCOMPPXF(S, P, N, n, NONE)
#define ISO_DEFUSERCOMPP0F(S, P, N, F)		ISO_DEFUSERCOMPPXF(S, P, N, #S, F)
#define ISO_DEFUSERCOMPP0(S, P, N)			ISO_DEFUSERCOMPPXF(S, P, N, #S, NONE)
//...of templated type
#define ISO_DEFUSERCOMPTPXF(S, T, P, N, n, F) _ISO_DEFUSERCOMPP(S<T>&, P, N, n, F)
#define ISO_DEFUSERCOMPTPX(S, T, P, N, n)	ISO_DEFUSERCOMPTPXF(S, T, P, N, n, NONE)
#define ISO_DEFUSERCOMPTPF(S, T, P, N, F)	ISO_DEFUSERCOMPTPXF(S, T, P, N, #S, F)
#define ISO_DEFUSERCOMPTP(S, T, P, N)		ISO_DEFUSERCOMPTPXF(S, T, P, N, #S, NONE)

// set up composite field
#define ISO_SETBASE(i, b)					fields[i].set(tag(), getdef<b>(), size_t(static_cast<b*>((_S*)1)) - 1, sizeof(b))

#define ISO_SETFIELDXT1(i, e, n, t)			fields[i].set(tag(n), getdef<t>(), iso_offset(_S, e), sizeof(((_S*)1)->e))
#define ISO_SETFIELDT1(i, e, t)				ISO_SETFIELDXT1(i, e, STRINGIFY2(e), t)
#define ISO_SETFIELDX1(i, e, n)				fields[i].set(tag(n), getdef(((_S*)1)->e), iso_offset(_S, e), sizeof(((_S*)1)->e))

#define ISO_SETFIELDXT(i, e, n, t)			fields[i].template set2<_T, t>(MAKE_FIELD(&_S::e, n))
#define ISO_SETFIELDT(i, e, t)				ISO_SETFIELDXT(i, e, STRINGIFY2(e), t)
#define ISO_SETFIELDX(i, e, n)				fields[i].template set<_T>(MAKE_FIELD(&_S::e, n))
#define ISO_SETFIELD(i, e)					ISO_SETFIELDX(i, e, STRINGIFY2(e))

// set up 2 to 8 composite fields
#define ISO_SETFIELDS2(i, E1, E2) ISO_SETFIELD(i, E1), ISO_SETFIELD(i + 1, E2)
#define ISO_SETFIELDS3(i, E1, E2, E3) ISO_SETFIELDS2(i, E1, E2), ISO_SETFIELD(i + 2, E3)
#define ISO_SETFIELDS4(i, E1, E2, E3, E4) ISO_SETFIELDS2(i, E1, E2), ISO_SETFIELDS2(i + 2, E3, E4)
#define ISO_SETFIELDS5(i, E1, E2, E3, E4, E5) ISO_SETFIELDS4(i, E1, E2, E3, E4), ISO_SETFIELD(i + 4, E5)
#define ISO_SETFIELDS6(i, E1, E2, E3, E4, E5, E6) ISO_SETFIELDS4(i, E1, E2, E3, E4), ISO_SETFIELDS2(i + 4, E5, E6)
#define ISO_SETFIELDS7(i, E1, E2, E3, E4, E5, E6, E7) ISO_SETFIELDS4(i, E1, E2, E3, E4), ISO_SETFIELDS3(i + 4, E5, E6, E7)
#define ISO_SETFIELDS8(i, E1, E2, E3, E4, E5, E6, E7, E8) ISO_SETFIELDS4(i, E1, E2, E3, E4), ISO_SETFIELDS4(i + 4, E5, E6, E7, E8)

//using variadic macros
#define _FIELD(x)	, MAKE_FIELD(&_S::x, #x)
#define ISO_SETFIELDS(i, ...)			Init<_T>(i VA_APPLY(_FIELD, __VA_ARGS__))
#define ISO_SETFIELDS_EXP0(X)
#define ISO_SETFIELDS_EXP1(X)			ISO_SETFIELDS(X)

#define ISO_DEFCOMPV(S, ...)			ISO_DEFCOMP(S, VA_NUM(__VA_ARGS__))				{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFCOMPPV(S, P, ...)		ISO_DEFCOMPP(S, P, VA_NUM(__VA_ARGS__))			{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFCOMPVT(S, T, ...)		ISO_DEFCOMPT(S, T, VA_NUM(__VA_ARGS__))			{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFCOMPBV(S, B, ...)		ISO_DEFCOMP(S, VA_NUM(__VA_ARGS__)+1)			{ ISO_SETBASE(0, B); ISO_SETFIELDS(1, __VA_ARGS__); } }
#define ISO_DEFCOMPBVT(S, T, B, ...)	ISO_DEFCOMPT(S, T, VA_NUM(__VA_ARGS__)+1)		{ ISO_SETBASE(0, B); ISO_SETFIELDS(1, __VA_ARGS__); } }

#define ISO_DEFUSERCOMPVT(S, T, ...)	ISO_DEFUSERCOMPT(S, T, VA_NUM(__VA_ARGS__))		{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPXVT(S, n, ...)	_ISO_DEFUSERCOMP(S, VA_NUM(__VA_ARGS__), n, NONE)		{ ISO_SETFIELDS(0, __VA_ARGS__); } }

// param element variadic
#define ISO_DEFUSERCOMPPV(S, P, ...)	ISO_DEFUSERCOMPP(S, P, VA_NUM(__VA_ARGS__))		{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPPXV(S, P, n,...)	ISO_DEFUSERCOMPPX(S, P, VA_NUM(__VA_ARGS__), n)	{ ISO_SETFIELDS(0, __VA_ARGS__); } }

#define ISO_DEFUSERCOMPV(S, ...)		ISO_DEFUSERCOMP(S, VA_NUM(__VA_ARGS__))			{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPXV(S, n,...)		ISO_DEFUSERCOMPX(S, VA_NUM(__VA_ARGS__), n)		{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPFV(S, F, ...)	ISO_DEFUSERCOMPF(S, VA_NUM(__VA_ARGS__), F)		{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPXFV(S, n, F,...)	ISO_DEFUSERCOMPXF(S, VA_NUM(__VA_ARGS__), n, F)	{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPVT2(S, ...)		ISO_DEFUSERCOMPT2(S, VA_NUM(__VA_ARGS__))		{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPVT2X(S, n, ...)	ISO_DEFUSERCOMPT2X(S, VA_NUM(__VA_ARGS__), n)	{ ISO_SETFIELDS(0, __VA_ARGS__); } }

#define ISO_DEFUSERCOMPPV(S, P, ...)	ISO_DEFUSERCOMPP(S, P, VA_NUM(__VA_ARGS__))			{ ISO_SETFIELDS(0, __VA_ARGS__); } }

#define ISO_DEFUSERCOMPBVT(S, T, B, ...)	ISO_DEFUSERCOMPT(S, T, VA_NUM(__VA_ARGS__)+1)	{ ISO_SETBASE(0, B); ISO_SETFIELDS(1, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPBV(S, B, ...)		ISO_DEFUSERCOMP(S, VA_NUM(__VA_ARGS__)+1)		{ ISO_SETBASE(0, B); ISO_SETFIELDS(1, __VA_ARGS__); } }
#define ISO_DEFUSERCOMPFBV(S, F, B, ...)	ISO_DEFUSERCOMPF(S, VA_NUM(__VA_ARGS__)+1, F)	{ ISO_SETBASE(0, B); ISO_SETFIELDS(1, __VA_ARGS__); } }

#define ISO_DEFCOMP0(S) ISO_DEFCOMP(S, 0) {} }

// header for user-enum def
#define ISO_DEFUSERENUMXF(S, N, n, B, F) template<> struct ISO::def<S> : public ISO::TypeUserEnumN<N, B> { typedef S _S; def() : TypeUserEnumN<N, B>(n, F)
#define ISO_DEFUSERENUMX(S, N, n) ISO_DEFUSERENUMXF(S, N, n, 32, NONE)
#define ISO_DEFUSERENUMF(S, N, B, F) ISO_DEFUSERENUMXF(S, N, #S, B, F)
#define ISO_DEFUSERENUM(S, N) ISO_DEFUSERENUMXF(S, N, #S, BIT_COUNT<S>, NONE)
#define ISO_SETENUMX(i, e, n) enums[i].set(n, e)

#define ISO_ENUM(x)	, #x, x
#define ISO_ENUMQ(x)	, #x, _S::x

#define ISO_SETENUMS(i, ...)		Init(i VA_APPLY(ISO_ENUM, __VA_ARGS__))
#define ISO_SETENUMSQ(i, ...)		Init(i VA_APPLY(ISO_ENUMQ, __VA_ARGS__))
#define ISO_DEFUSERENUMXFV(S, n, B, F, ...)		ISO_DEFUSERENUMXF(S, VA_NUM(__VA_ARGS__), n, B, F) { ISO_SETENUMS(0, __VA_ARGS__); } }
#define ISO_DEFUSERENUMXFQV(S, n, B, F, ...)	ISO_DEFUSERENUMXF(S, VA_NUM(__VA_ARGS__), n, B, F) { ISO_SETENUMSQ(0, __VA_ARGS__); } }
#define ISO_DEFUSERENUMXV(S, n, ...)			ISO_DEFUSERENUMXF(S, VA_NUM(__VA_ARGS__), n, 32, NONE) { ISO_SETENUMS(0, __VA_ARGS__); } }
#define ISO_DEFUSERENUMXQV(S, n, ...)			ISO_DEFUSERENUMXF(S, VA_NUM(__VA_ARGS__), n, 32, NONE) { ISO_SETENUMSQ(0, __VA_ARGS__); } }
#define ISO_DEFUSERENUMV(S, ...)				ISO_DEFUSERENUMXFV(S, #S, 32, NONE, __VA_ARGS__)
#define ISO_DEFUSERENUMQV(S, ...)				ISO_DEFUSERENUMXFQV(S, #S, 32, NONE, __VA_ARGS__)

//// set up 1 to 8 enum values
//#define ISO_SETENUM(i, e) ISO_SETENUMX(i, e, STRINGIFY2(e))
//#define ISO_SETENUMS2(i, E1, E2) ISO_SETENUM(i, E1), ISO_SETENUM(i + 1, E2)
//#define ISO_SETENUMS3(i, E1, E2, E3) ISO_SETENUMS2(i, E1, E2), ISO_SETENUM(i + 2, E3)
//#define ISO_SETENUMS4(i, E1, E2, E3, E4) ISO_SETENUMS2(i, E1, E2), ISO_SETENUMS2(i + 2, E3, E4)
//#define ISO_SETENUMS5(i, E1, E2, E3, E4, E5) ISO_SETENUMS4(i, E1, E2, E3, E4), ISO_SETENUM(i + 4, E5)
//#define ISO_SETENUMS6(i, E1, E2, E3, E4, E5, E6) ISO_SETENUMS4(i, E1, E2, E3, E4), ISO_SETENUMS2(i + 4, E5, E6)
//#define ISO_SETENUMS7(i, E1, E2, E3, E4, E5, E6, E7) ISO_SETENUMS4(i, E1, E2, E3, E4), ISO_SETENUMS3(i + 4, E5, E6, E7)
//#define ISO_SETENUMS8(i, E1, E2, E3, E4, E5, E6, E7, E8) ISO_SETENUMS4(i, E1, E2, E3, E4), ISO_SETENUMS4(i + 4, E5, E6, E7, E8)
//
//// set up 1 to 8 enum values with _S:: qualifier
//#define ISO_SETENUMQ(i, E) enums[i].set(#E, _S::E)
//#define ISO_SETENUMSQ2(i, E1, E2) ISO_SETENUMQ(i, E1), ISO_SETENUMQ(i + 1, E2)
//#define ISO_SETENUMSQ3(i, E1, E2, E3) ISO_SETENUMSQ2(i, E1, E2), ISO_SETENUMQ(i + 2, E3)
//#define ISO_SETENUMSQ4(i, E1, E2, E3, E4) ISO_SETENUMSQ2(i, E1, E2), ISO_SETENUMSQ2(i + 2, E3, E4)
//#define ISO_SETENUMSQ5(i, E1, E2, E3, E4, E5) ISO_SETENUMSQ4(i, E1, E2, E3, E4), ISO_SETENUMQ(i + 4, E5)
//#define ISO_SETENUMSQ6(i, E1, E2, E3, E4, E5, E6) ISO_SETENUMSQ4(i, E1, E2, E3, E4), ISO_SETENUMSQ2(i + 4, E5, E6)
//#define ISO_SETENUMSQ7(i, E1, E2, E3, E4, E5, E6, E7) ISO_SETENUMSQ4(i, E1, E2, E3, E4), ISO_SETENUMSQ3(i + 4, E5, E6, E7)
//#define ISO_SETENUMSQ8(i, E1, E2, E3, E4, E5, E6, E7, E8) ISO_SETENUMSQ4(i, E1, E2, E3, E4), ISO_SETENUMSQ4(i + 4, E5, E6, E7, E8)

//-----------------------------------------------------------------------------

template<typename T, typename V=void> struct def;

template<typename T> Type* getdef() {
	static manual_static<def<T>> def;
	return &def.get();
}
template<> inline Type* getdef<void>() { return 0; }

template<typename T>	struct def_same { Type* operator&() { return getdef<T>(); } };

template<typename T> struct def<const T>					{ Type* operator&() { return getdef<T>(); }};
template<typename T> struct def<const T*>					{ Type* operator&() { return getdef<T*>(); }};
template<typename T> struct def<constructable<T>>			{ Type* operator&() { return getdef<T>(); }};
template<typename T> struct def<_packed<T>> : def<T>		{ def() { (&*this)->flags |= Type::TYPE_PACKED; }};
template<typename T> struct def<packed<T>> : def<T>			{ def() { (&*this)->flags |= Type::TYPE_PACKED; }};
template<typename T> struct def<packed<constructable<T>>>	{ Type* operator&() { return getdef<packed<T>>(); }};
template<typename T> struct def<packed<const T>>			{ Type* operator&() { return getdef<packed<T>>(); }};

template<typename T> struct def<holder<T>> : TypeComposite {
	Element element;
	def() : TypeComposite(1), element(0, getdef<T>(), 0, sizeof(T)) {}
};

template<typename T, int B> struct def<ptr<T, B>> : TypeReference {
	def() : TypeReference(0, B == 64 ? TYPE_64BIT : TYPE_32BIT) { subtype = getdef<T>(); }
};
template<typename T, int N> struct def<Array<T, N>> : TypeArray {
	def() : TypeArray(getdef<T>(), N, (uint32)sizeof(T)) {}
};
template<typename T, int N> struct def<T[N]> : def<Array<T, N>> {};
template<typename T, int N> struct def<const T[N]> : def<Array<T, N>> {};
template<typename T, int N> struct def<array<T, N>> : def<Array<T, N>> {};

template<typename T, int B> struct def<OpenArray<T, B>> : TypeOpenArray {
	def() : TypeOpenArray(getdef<T>(), sizeof(T), log2alignment<T>, B == 64 ? TYPE_64BIT : TYPE_32BIT) {}
};

template<typename A, typename B> struct def<pair<A, B>> : TypeCompositeN<2> {
	typedef pair<A, B>	S;
	def() : TypeCompositeN<2>((S*)0, "a", &S::a, "b", &S::b) {}
};

template<typename T> struct def<interval<T>> : TypeCompositeN<2> {
	typedef interval<T>	S;
	def() : TypeCompositeN<2>((S*)0, "min", &S::a, "max", &S::b) {}
};

//-----------------------------------------------------------------------------

template<int N, Type::FLAGS F> struct TypeIntT : TypeInt { constexpr TypeIntT() : TypeInt(N, 0, F) {} };
template<typename T> struct TypeIntT2 : TypeIntT<uint32(sizeof(T) * 8), (is_signed<T> ? TypeInt::SIGN : Type::NONE) | (is_char<T> ? TypeInt::CHR : Type::NONE)> {};

template<> struct def<int8>		: TypeIntT2<int8>	{};
template<> struct def<int16>	: TypeIntT2<int16>	{};
template<> struct def<int32>	: TypeIntT2<int32>	{};
template<> struct def<int64>	: TypeIntT2<int64>	{};
template<> struct def<int128>	: TypeIntT2<int128>	{};
template<> struct def<uint8>	: TypeIntT2<uint8>	{};
template<> struct def<uint16>	: TypeIntT2<uint16>	{};
template<> struct def<uint32>	: TypeIntT2<uint32>	{};
template<> struct def<uint64>	: TypeIntT2<uint64>	{};
template<> struct def<uint128>	: TypeIntT2<uint128>{};
#if USE_LONG
template<> struct def<long>		: TypeIntT2<long>	{};
template<> struct def<ulong>	: TypeIntT2<ulong>	{};
#endif
#ifndef USE_SIGNEDCHAR
template<> struct def<char>		: TypeIntT2<char>	{};
#endif
template<> struct def<char16>	: TypeIntT2<char16>	{};
template<> struct def<char32>	: TypeIntT2<char32>	{};

template<typename C> struct def<char_type<C>>	: TypeIntT<sizeof(C) * 8, TypeInt::CHR>	{};

template<typename T> struct	def<baseint<16, T>> : TypeIntT<(uint32)sizeof(T) * 8, TypeInt::HEX> {};
template<typename T> struct	def<rawint<T>>		: TypeIntT<(uint32)sizeof(T) * 8, TypeInt::HEX | TypeInt::NOSWAP> {};
template<typename E, typename S> struct	def<flags<E, S>>	{ Type* operator&() { return getdef<S>(); }};

template<> struct def<float>	: TypeFloat	{ def() : TypeFloat(32, 8, TypeFloat::SIGN) {}};
template<> struct def<double>	: TypeFloat	{ def() : TypeFloat(64, 11, TypeFloat::SIGN) {}};

template<> struct def<bool8> : public TypeUserEnumN<2, 8> { def() : TypeUserEnumN<2, 8>("bool", NONE, "false", 0, "true", 1) {} };

template<typename P, typename C, Type::FLAGS F = Type::NONE> struct TypeStringT : TypeString {
	constexpr TypeStringT() : TypeString(sizeof(P) > 4, F | FLAGS(klog2<sizeof(C)> * UTF16)) {}
};

template<> struct def<char*>		: TypeStringT<char*, char> {};
template<> struct def<string>		: TypeStringT<string, char, TypeString::MALLOC> {};
template<> struct def<char16*>		: TypeStringT<char16*, char16> {};
template<> struct def<string16>		: TypeStringT<string16, char16, TypeString::MALLOC> {};
template<typename C, int N> struct def<ptr_string<C, N>>	: TypeStringT<ptr_string<C, N>, C, TypeString::ALLOC> {};
template<typename B> struct def<string_base<B>>				: TypeStringT<string_base<B>, typename string_base<B>::element> {};

template<int N, typename C> struct def<fixed_string<N, C>> : TypeArray {
	def() : TypeArray(getdef<char_type<C>>(), N, (uint32)sizeof(C)) {}
};

template<typename T> struct unescaped : T {};
template<typename T> struct def<unescaped<T>> : def<T> {
	def() : def<T>() { this->flags |= this->UNESCAPED; }
};

ISO_DEFSAME(tag, const char*);

template<> struct def<GUID> : public TypeUserCompN<4> {
	def() : TypeUserCompN<4>("GUID", NONE, (GUID*)0,
		0, &GUID::Data1,
		0, &GUID::Data2,
		0, &GUID::Data3,
		0, &GUID::Data4
	) {}
};

typedef ptr_string<char, 32> ptr_string32;
ISO_DEFCALLBACK(crc32, ptr_string32);

typedef OpenArray<ptr<void>>					anything;
typedef OpenArray<ptr<void, 64>, 64>			anything64;
typedef OpenArray<ptr<void, PTRBITS>, PTRBITS>	anything_machine;

//-----------------------------------------------------------------------------
//	type_list def type definition creators
//-----------------------------------------------------------------------------

template<typename T, int O> struct TL_elements {
	typedef meta::TL_head_t<T>	A;
	typedef meta::TL_tail_t<T>	B;
	struct temp {
		char pad[O];
		A	a;
	};
	Element		a;
	TL_elements<B, (uint32)sizeof(temp)> b;
	TL_elements() : a(0, getdef<A>(), size_t(&((temp*)1)->a) - 1, sizeof(A)) {}
};
template<typename T> struct TL_elements<T, 0> {
	typedef meta::TL_head_t<T>	A;
	typedef meta::TL_tail_t<T>	B;
	Element		a;
	TL_elements<B, (uint32)sizeof(A)> b;
	TL_elements() : a(0, getdef<A>(), 0, sizeof(A)) {}
};
template<int O> struct TL_elements<type_list<>, O> {};

template<typename... T> struct def<tuple<T...>> : TypeComposite, TL_elements<type_list<T...>, 0> {
	def() : TypeComposite(sizeof...(T)) {}
};

//-----------------------------------------------------------------------------
//	class Browser & class Browser2
//-----------------------------------------------------------------------------

class Browser2;

class Browser {
protected:
	const Type* type;
	void*		data;

	template<typename T, typename V=void> struct s_setget;
	template<typename T> struct as;

	static iso_export void ClearTempFlags(Browser b);
	static iso_export void Apply(Browser2 b, const Type* type, void f(void*));
	static iso_export void Apply(Browser2 b, const Type* type, void f(void*, void*), void* param);

public:
	Browser() : type(0), data(0) {}
	Browser(const Type* type, void* data) : type(data ? type : 0), data(data) {}
	Browser(const Browser& b) : type(b.type), data(b.data) {}
	template<typename T, int B> explicit Browser(const ptr<T, B>& t) : type(t.GetType()), data(t) {}
	template<typename T> explicit Browser(const OpenArray<T>& t) : type(getdef(t)), data((void*)&t) {}

	TYPE								GetType()			const { return type ? type->GetType() : UNKNOWN; }
	bool								Is(const Type* t, int crit = 0) const { return Same(type, t, crit); }
	template<typename T> bool			Is(int crit = 0)	const { return Is(getdef<T>(), crit); }
	bool								Is(tag2 id)			const { return type && type->Is(id); }
	const char*							External()			const { const Type* t = type->SkipUser(); return t && t->GetType() == REFERENCE ? GetPtr(t->ReadPtr(data)).External() : 0; }
	bool								HasCRCType()		const { const Type* t = type->SkipUser(); return t && t->GetType() == REFERENCE && GetPtr(t->ReadPtr(data)).HasCRCType(); }

	inline const Browser2				ref(tag2 id)		const;
	inline const Browser2				Index(tag2 id)		const;
	inline const Browser2				Index(int i)		const;
	inline bool							IsVirtPtr()			const;
	inline const Browser2				operator[](tag2 id) const;
	inline const Browser2				operator[](int i)	const;

	iso_export const Browser2			operator*()			const;
	iso_export uint32					Count()				const;
	iso_export int						GetIndex(tag2 id, int from = 0) const;
	iso_export tag2						GetName()			const;
	iso_export tag2						GetName(int i)		const;
	iso_export uint32					GetSize()			const;
	iso_export bool						Resize(uint32 n)	const;
	iso_export const Browser			Append()			const;
	iso_export bool						Remove(int i)		const;
	iso_export const Browser			Insert(int i)		const;
	iso_export const Browser2			FindByType(tag2 id) const;
	iso_export const Browser2			FindByType(const Type* t, int crit = 0) const;
	template<typename T> const Browser2 FindByType(int crit = 0) const;

	Browser								SkipUser()			const { return Browser(type->SkipUser(), data); }
	uint32								GetAlignment()		const { return type->GetAlignment(); }
	const Type*							GetTypeDef()		const { return type; }

	template<typename T> as<T>			As()				const { return as<T>(*this); }
	bool								Check(tag2 id)		const { return GetIndex(id) >= 0; }

	explicit operator bool()	const	{ return !!data; }
	operator void*()			const	{ return data; }
	operator const void*()		const	{ return data; }

#if 0
	template<typename T>				operator T*()			const	{ ASSERT(type->SameAs<T>()); return (T*)data; }
	template<typename T>				operator ptr<T>&()		const	{ ASSERT(type->SameAs<T>()); return ptr<T>::Ptr((T*)data); }
#else
	template<typename T>				operator T*()			const;
	template<typename T>				operator OpenArray<T>*()const;
	template<typename T, int B>			operator ptr<T, B>*()	const	{ return (ptr<T, B>*)((SkipUserFlags(type) & Type::TYPE_MASKEX) == (REFERENCE | (B == 64 ? Type::TYPE_64BIT : Type::TYPE_32BIT)) ? data : 0); }
	template<typename T>				operator T**()			const;//	{ static_assert(false, "nope"); };	//	{ COMPILEASSERT(0); return (T**)0; }
	template<typename T, int B>			operator ptr<T, B>()	const	{ return ptr<T, B>::Ptr((T*)data); }
	template<typename T> T*				check()					const;

#endif
	operator							const_memory_block()	const	{ return const_memory_block(data, GetSize()); }

	getter<const Browser>				get()				const { return *this; }
	getter<const Browser>				Get()				const { return *this; }
	template<typename T> noref_cv_t<T>	Get(T&& def)		const { noref_cv_t<T> t; if (s_setget<noref_cv_t<T>>::get(data, type, t)) return t; return forward<T>(def); }
	template<typename T> T				Get()				const { return Get(T()); }
	template<typename T> T				get()				const { return Get(T()); }
	int									GetInt(const int i = 0)				const { return Get(i); }
	float								GetFloat(const float f = 0)			const { return Get(f); }
	double								GetDouble(const double f = 0)		const { return Get(f); }
	const char*							GetString(const char* const s = 0)	const { return Get(s); }
	template<typename T> bool			Read(T &t)							const { return s_setget<noref_cv_t<T>>::get(data, type, t); }

	range<stride_iterator<void>>		GetArray(tag2 id = {})	const;

//	template<typename T, int B = 32> ptr<T, B>	ReadPtr()			const { return ptr<T, B>::Ptr((T*)type->SkipUser()->ReadPtr(data)); }
	inline const Browser2				GetMember(crc32 id) const;

	iso_export void						UnsafeSet(const void* srce) const;
	template<typename T> bool			Set(T&& t)			const { return s_setget<noref_cv_t<T>>::set(data, type, forward<T>(t)); }

	template<typename T> bool			SetMember(tag2 id, const T& t) const;
	template<int N> bool				SetMember(tag2 id, const char (&t)[N]) const { return SetMember(id, (const char*)t); }
	template<typename T, int B> bool	SetMember(tag2 id, ptr<T, B> t) const;

	iso_export int						ParseField(string_scan& s)	const;
	iso_export bool						Update(const char* s, bool from = false) const;
	ptr_machine<void>					Duplicate(tag2 id = tag2(), int flags = DUPF_CHECKEXTERNALS) const { return ISO::Duplicate(id, type, data, flags); }
	void								ClearTempFlags()		const { ClearTempFlags(*this); }
	inline const Browser2				Parse(string_scan s, const char_set &set = ~char_set(" \t.[/\\"))	const;
	inline void							Apply(const Type* type, void f(void*)) const;
	inline void							Apply(const Type* type, void f(void*, void*), void* param) const;
	template<typename T> void			Apply(void f(T*))		const { Apply(getdef<T>(), (void (*)(void*))f); }
	template<typename T, typename P> void Apply(void f(T*, P*), P* param) const { Apply(getdef<T>(), (void (*)(void*, void*))f, (void*)param); }

	class iterator;
	iso_export iterator					begin()	const;
	iso_export iterator					end()	const;
};

class Browser2 : public Browser {
	ptr_machine<void> p;
	ptr_machine<void> _GetPtr() const;

public:
	using Browser::GetName;

	const ptr_machine<void>&GetPtr()		const { return p; }
	bool					IsPtr()			const { return p == data && p.GetType() == type; }
	ptr_machine<void>		TryPtr()		const { return IsPtr() ? p : ptr_machine<void>(); }
	bool					IsBin()			const { return p.IsBin(); }
	const char*				External()		const { return p == data ? p.External() : Browser::External(); }
	bool					HasCRCType()	const { return p == data ? p.HasCRCType() : Browser::HasCRCType(); }
	Browser2				SkipUser()		const { return Browser2(type->SkipUser(), data, p); }
	void					AddRef()		const { if (p) p.Header()->addref(); }
	bool					Release()		const { return p && p.Header()->release(); }

	Browser2() {}
	Browser2(const Type* type, void* data, const ptr_machine<void>& p) : Browser(type, data), p(p) {}
	Browser2(const Browser2& b)									= default;//: Browser(b), p(b.p) {}
#ifdef USE_RVALUE_REFS
	Browser2(Browser2 &&b)										= default;//: p(p2.detach()) {}
#endif
	Browser2(const Browser& b)									: Browser(b) {}
	Browser2(const Browser& b, const ptr_machine<void>& p)		: Browser(b), p(p) {}
	template<typename T, int B> Browser2(const ptr<T, B>& p)	: Browser(p), p(p) {}

	template<typename T, int B> operator ptr<T, B>() const { return _GetPtr(); }

	Browser2& operator=(const Browser& b)	{ Browser::operator=(b); p.Clear(); return *this; }
	Browser2& operator=(const Browser2& b)	{ Browser::operator=(b); p = b.p; return *this; }

	iso_export const Browser2 Index(tag2 id)	const;
	iso_export const Browser2 Index(int i)		const;
	iso_export const Browser2 ref(tag2 id)		const;
	iso_export const Browser2 operator*()		const;

	const Browser2 operator[](tag2 id)	const { return Index(id); }
	const Browser2 operator[](int i)	const { return Index(i); }
	uint32			Count()				const { return External() ? 0 : Browser::Count(); }
	const Browser2	GetMember(crc32 id)	const { return operator[](tag2(id)); }

	tag2 GetName() const { return IsPtr() ? p.ID() : type && type->GetType() == REFERENCE ? ISO::GetPtr(type->ReadPtr(data)).ID() : tag2(); }
	bool SetName(tag2 id) const {
		if (IsPtr())
			p.SetID(id);
		else if (type && type->GetType() == REFERENCE)
			ISO::GetPtr(type->ReadPtr(data)).SetID(id);
		else
			return false;
		return true;
	}
//	iso_export tag2				GetName(int i) const;
	iso_export const Browser2	Parse(string_scan spec, const char_set &set = ~char_set(".[/\\+"), const Type *create_type = 0) const;

	class iterator;
	typedef Browser2 element, reference;
	typedef iterator const_iterator;
	iso_export iterator			begin() const;
	iso_export iterator			end() const;

	friend const Browser2 operator/(const Browser2& b, tag2 id) { return b[id]; }
};

inline const Browser2	operator/(const Browser& b, tag2 id)		{ return b.Index(id); }

inline const Browser2	Browser::Index(int i)			const	{ return Browser2(*this).Index(i); }
inline const Browser2	Browser::Index(tag2 id)			const	{ return Browser2(*this).Index(id); }
inline const Browser2	Browser::GetMember(crc32 id)	const	{ return Browser2(*this).Index(id); }
inline const Browser2	Browser::ref(tag2 id)			const	{ return Browser2(*this).ref(id); }
inline const Browser2	Browser::operator[](tag2 id)	const	{ return Index(id); }
inline const Browser2	Browser::operator[](int i)		const	{ return Index(i); }

inline const Browser2	Browser::Parse(string_scan spec, const char_set &set)	const	{ return Browser2(*this).Parse(spec, set); }

template<typename T>	Browser::operator T*() const	{
	switch (TypeType(type)) {
		case REFERENCE:
		case OPENARRAY: return (T*)(**this).data;
		default: return (T*)data;
	}
}
//	if (!is_any(TypeType(type), REFERENCE, OPENARRAY)) return (T*)data; return **this; }
template<typename T>	Browser::operator OpenArray<T>*() const	{
	if (is_any(SkipUserType(type), REFERENCE, VIRTUAL)) return **this; return (OpenArray<T>*)data;
}
template<typename T>	T *Browser::check() const {
	switch (TypeType(type)) {
		case REFERENCE:
		case OPENARRAY: return (**this).check<T>();
		default: 
			return Is<T>() ? (T*)data : nullptr;
	}
}

inline void Browser::Apply(const Type* type, void f(void*)) const {
	Apply(*this, type, f);
	ClearTempFlags();
}
inline void Browser::Apply(const Type* type, void f(void*, void*), void* param) const {
	Apply(*this, type, f, param);
	ClearTempFlags();
}

template<typename T> inline const Browser2 Browser::FindByType(int crit) const { return FindByType(getdef<T>(), crit); }

template<typename T> inline Browser		MakeBrowser(T* t)				{ return Browser(getdef<T>(), (void*)t); }
template<typename T> inline Browser		MakeBrowser(T& t)				{ return Browser(getdef<T>(), (void*)&t); }
template<typename T> inline Browser2	MakeBrowser(T&& t)				{ return ptr<T>(0, forward<T>(t)); }
inline Browser							MakeBrowser(Browser& t)			{ return t; }
inline Browser							MakeBrowser(Browser&& t)		{ return t; }
inline Browser							MakeBrowser(const Browser& t)	{ return t; }
inline Browser2							MakeBrowser(Browser2& t)		{ return t; }
inline Browser2							MakeBrowser(Browser2&& t)		{ return move(t); }
inline Browser2							MakeBrowser(const Browser2& t)	{ return t; }

class PtrBrowser {
protected:
	ptr_machine<void> p;
public:
	PtrBrowser() {}
	PtrBrowser(const ptr_machine<void>& p) : p(p) {}
	void		Clear()									{ p.Clear(); }
	PtrBrowser& operator=(const ptr_machine<void>& _p)	{ p = _p; return *this; }
	operator ptr_machine<void>&()						{ return p; }

	const Browser2				operator[](tag2 id)		const { return Browser(p)[id]; }
	const Browser2				operator[](int i)		const { return Browser(p)[i]; }
	const Browser2				GetMember(crc32 id)		const { return Browser(p).GetMember(id); }
	operator const Browser2()							const { return p; }
	template<typename T, int B> operator ptr<T, B>()	const { return p; }
	template<typename T>		operator T*()			const { return p; }
	operator void*()									const { return p; }
	template<typename T> T		get()					const { return Browser(p).get<T>(); }
	getter<const PtrBrowser>	get()					const { return *this; }
	const Browser2				Parse(const char* s, const char_set &set = ~char_set(" \t.[/\\"), const Type *create_type = 0)	const { return Browser2(p).Parse(s, set, create_type); }
};

class browser_root : public Browser {
	anything	names;

public:
	browser_root()					{ Browser::operator=(Browser(names)); }
	~browser_root()					{ names.Clear(); }
	void		Add(ptr<void> p)	{ names.Append(p); }
	anything*	operator&()			{ return &names; }
};
iso_export browser_root& root();
inline auto root(tag2 id)			{ return root()[id]; }

template<typename T> struct Browser::as : Browser {
	as(const Browser& b) : Browser(b) {}
	operator T()					{ return Get<T>(); }
	T operator+=(T t)				{ Set<T>(t += Get<T>(T())); return t; }
	T operator-=(T t)				{ Set<T>(t -= Get<T>(T())); return t; }
	T operator*=(T t)				{ Set<T>(t *= Get<T>(T())); return t; }
	T operator/=(T t)				{ Set<T>(t /= Get<T>(T())); return t; }
	const T& operator=(const T& t)	{ Set<T>(t); return t;	}
};

//-----------------------------------------------------------------------------
//	Virtual
//-----------------------------------------------------------------------------

struct Virtual : Type {
	static const FLAGS DEFER = FLAG0, VIRTSIZE = UNSTORED0;
	uint32		(*Count)	(Virtual *type, void* data);
	tag2		(*GetName)	(Virtual *type, void* data, int i);
	int			(*GetIndex)	(Virtual *type, void* data, const tag2& id, int from);
	Browser2	(*Index)	(Virtual *type, void* data, int i);
	Browser2	(*Deref)	(Virtual *type, void* data);
	bool		(*Resize)	(Virtual *type, void* data, int size);
	void		(*Delete)	(Virtual *type, void* data);
	bool		(*Update)	(Virtual *type, void* data, const char* spec, bool from);
	bool		(*Convert)	(Virtual *type, void* data, const Type *srce_type, const void *srce_data);

	uint32		GetSize() const { return param16; }
	bool		IsVirtPtr(void* data) {
		uint32 c = Count(this, data);
		return !c || !~c;
	}
	template<typename T> Virtual(T*, size_t size, FLAGS flags = NONE) : Type(VIRTUAL, flags), Count(T::Count), GetName(T::GetName), GetIndex(T::GetIndex), Index(T::Index), Deref(T::Deref), Resize(T::Resize), Delete(T::Delete), Update(T::Update), Convert(T::Convert) {
		if (!(flags & VIRTSIZE))
			param16 = (uint16)size;
	}
};

struct VirtualDefaults {
	static uint32	Count()										{ return 0; }
	static tag2		GetName(int i)								{ return tag2(); }
	static int		GetIndex(const tag2& id, int from)			{ return -1; }
	static Browser2 Index(int i)								{ return Browser2(); }
	static Browser2 Deref()										{ return Browser2(); }
	static bool		Resize(int size)							{ return false; }
	static bool		Update(const char* s, bool from)			{ return false; }
	static bool		Convert(const Type *type, const void *data)	{ return false; }
};

// functions defined in T
template<class T> class VirtualT : public Virtual {
public:
	static uint32	Count(Virtual* type, void* data)								{ return uint32(((T*)data)->Count()); }
	static tag2		GetName(Virtual* type, void* data, int i)						{ return tag2(((T*)data)->GetName(i)); }
	static int		GetIndex(Virtual* type, void* data, const tag2& id, int from)	{ return ((T*)data)->GetIndex(id, from); }
	static Browser2 Index(Virtual* type, void* data, int i)							{ return ((T*)data)->Index(i); }
	static Browser2 Deref(Virtual* type, void* data)								{ return ((T*)data)->Deref(); }
	static bool		Resize(Virtual* type, void* data, int size)						{ return ((T*)data)->Resize(size); }
	static void		Delete(Virtual* type, void* data)								{ return ((T*)data)->~T(); }
	static bool		Update(Virtual* type, void* data, const char* s, bool from)		{ return ((T*)data)->Update(s, from); }
	static bool		Convert(Virtual *type, void *data, const Type *srce_type, const void *srce_data) {
		return ((T*)data)->Convert(srce_type, srce_data);
	}
	typedef T		type;
	VirtualT(FLAGS flags = NONE) : Virtual(this, sizeof(T), flags)				{}
};

// functions defined in V
template<class T, class V> class VirtualT1 : public Virtual {
public:
	// default implementations
	static uint32	Count(T& a)											{ return 0; }
	static tag2		GetName(T& a, int i)								{ return tag2(); }
	static int		GetIndex(T& a, const tag2& id, int from)			{ return -1; }
	static Browser2 Index(T& a, int i)									{ return Browser(); }
	static Browser2 Deref(T& a)											{ return Browser(); }
	static bool		Resize(T& a, int size)								{ return false; }
	static void		Delete(T& a)										{}
	static bool		Update(T& a, const char* s, bool from)				{ return false; }
	static bool		Convert(T &a, const Type *type, const void *data)	{ return false; }

public:
	static uint32	Count(Virtual* type, void* data)								{ return static_cast<V*>(type)->Count(*(T*)data); }
	static tag2		GetName(Virtual* type, void* data, int i)						{ return tag2(static_cast<V*>(type)->GetName(*(T*)data, i)); }
	static int		GetIndex(Virtual* type, void* data, const tag2& id, int from)	{ return static_cast<V*>(type)->GetIndex(*(T*)data, id, from); }
	static Browser2 Index(Virtual* type, void* data, int i)							{ return static_cast<V*>(type)->Index(*(T*)data, i); }
	static Browser2 Deref(Virtual* type, void* data)								{ return static_cast<V*>(type)->Deref(*(T*)data); }
	static bool		Resize(Virtual* type, void* data, int size)						{ return static_cast<V*>(type)->Resize(*(T*)data, size); }
	static void		Delete(Virtual* type, void* data)								{ static_cast<V*>(type)->Delete(*(T*)data); }
	static bool		Update(Virtual* type, void* data, const char* s, bool from)		{ return static_cast<V*>(type)->Update(*(T*)data, s, from); }
	static bool		Convert(Virtual *type, void *data, const Type *srce_type, const void *srce_data) {
		return static_cast<V*>(type)->Convert(*(T*)data, srce_type, srce_data);
	}
	template<typename X> VirtualT1(X* me, size_t size, FLAGS flags = NONE) : Virtual(me, size, flags)	{}
	VirtualT1(FLAGS flags = NONE) : Virtual(this, sizeof(T), flags)										{}
};

// functions defined in def<T>
template<class T> class VirtualT2 : public VirtualT1<T, def<T>> {
public:
	static void Delete(Virtual* type, void* data) { ((T*)data)->~T(); }
	VirtualT2(Type::FLAGS flags = Type::NONE) : VirtualT1<T, def<T>>(this, sizeof(T), flags) {}
};

template<class T> struct TypeUserVirt : public TypeUserSave {
	VirtualT<T>	virt;
	TypeUserVirt(tag2 id, FLAGS2 flags = NONE) : TypeUserSave(id, &virt, flags), virt(flags.second()) {}
};

template<typename S, typename P, typename C, typename T> struct Element::set_helper<param_element<S, P>, C, T> {
	template<typename U, T C::*f> static uint32 f(tag id, Element& e) {
		static struct Accessor : public VirtualT1<param_element<S, P>, Accessor> {
			static Browser2 Deref(param_element<S, P>& a) { return MakeBrowser(get_field(a, element_cast<U>(f))); }
		} def;
		return e.set(id, &def, 0, 0);
	}
};
template<typename S, typename C, typename R, R (C::*p)()>						uint32 Element::set(const meta::field<R (C::*)(), p> &f) {
	static struct Accessor : public VirtualT1<S, Accessor> {
		static Browser2 Deref(S& a) { return MakeBrowser((a.*p)()); }
	} def;
	return set(tag(f.name), &def, 0, 0);
}
template<typename S, typename C, typename R, R (C::*p)() const>					uint32 Element::set(const meta::field<R (C::*)() const, p> &f) {
	static struct Accessor : public VirtualT1<S, Accessor> {
		static Browser2 Deref(S& a) { return MakeBrowser((a.*p)()); }
	} def;
	return set(tag(f.name), &def, 0, 0);
}
template<typename S, typename P, typename C, typename R, R (C::*p)(P)>			uint32 Element::set(const meta::field<R (C::*)(P), p> &f) {
	static struct Accessor : public VirtualT1<S, Accessor> {
		static Browser2 Deref(S& a) { return MakeBrowser((a.t.*p)(a.p)); }
	} def;
	return set(tag(f.name), &def, 0, 0);
}
template<typename S, typename P, typename C, typename R, R (C::*p)(P) const>	uint32 Element::set(const meta::field<R (C::*)(P) const, p> &f) {
	static struct Accessor : public VirtualT1<S, Accessor> {
		static Browser2 Deref(S& a) { return MakeBrowser((a.t.*p)(a.p)); }
	} def;
	return set(tag(f.name), &def, 0, 0);
}

//-------------------------------------
// param

template<typename T, typename P> struct def<param_element<T, P>> : VirtualT2<param_element<T, P>> {
	typedef param_element<T, P> A;
	uint32		Count(A& a)									{ return MakeBrowser(get(a)).Count(); }
	tag2		GetName(A& a, int i)						{ return MakeBrowser(get(a)).GetName(i); }
	int			GetIndex(A& a, const tag2& id, int from)	{ return MakeBrowser(get(a)).GetIndex(id, from); }
	Browser2	Index(A& a, int i)							{ return MakeBrowser(get(a)).Index(i); }
	Browser2	Deref(A& a) {
		Browser2 b = MakeBrowser(get(a));
		if (Browser2 b2 = *b)
			return b2;
		return b;
	}
	bool		Update(A& a, const char* s, bool from)		{ return MakeBrowser(get(a)).Update(s, from); }
};

template<typename T, typename P> struct def<param_element<const T, P>> : def<param_element<T, P>> {};
template<typename T, typename P> struct def<param_element<const T&, P>> : def<param_element<T&, P>> {};
template<typename T, typename P> tag2	_GetName(const param_element<T, P> &t)	{ return _GetName(get(t)); }

//-------------------------------------
// endian

template<typename T> tag2 GetName() {
	const Type* t = getdef<T>();
	if (t->GetType() == USER)
		return ((TypeUser*)t)->ID();
	return buffer_accum<256>(t).term();
}

template<typename T> struct def<T_swap_endian<T>> : TypeUserSave {
	struct V : VirtualT1<T_swap_endian<T>, V> {
		static Browser2 Deref(const T_swap_endian<T>& a) { return MakePtr(tag(), get(a)); }
	} v;
	def() : TypeUserSave(GetName<T>(), &v) {}
};

template<typename T, int BITS> struct def<compact<T, BITS>> : TypeUserSave {
	struct V : VirtualT1<compact<T, BITS>, V> {
		static Browser2 Deref(const compact<T, BITS>& a) { return MakePtr(tag(), a.get()); }
	} v;
	def() : TypeUserSave(GetName<T>(), &v) {}
};

//-------------------------------------
// pointers

template<> struct def<void*> : def<baseint<16, uintptr_t>> {};

template<typename P> struct TISO_virtualpointer : VirtualT2<P> {
	typedef noref_t<decltype(*declval<P>())>	T;
	static Browser	Deref(P &a)								{ return !a ? Browser() : MakeBrowser(*get(a)); }
	static bool		Update(P &a, const char* s, bool from)	{ return Deref(a).Update(s, from); }
	static bool		Convert(P &a, const Type *type, const void *data)	{
		if (type->Is<T>()) {
			a = (T*)data;
			return true;
		}
		return false;
	}
};

template<typename T> struct def<T*>				: TISO_virtualpointer<T*> {};
template<typename T> struct def<pointer<T>>		: TISO_virtualpointer<T*> {};
template<typename T> struct def<unique_ptr<T>>	: TISO_virtualpointer<unique_ptr<T>> {};
template<typename T> struct def<ref_ptr<T>>		: TISO_virtualpointer<ref_ptr<T>>	{};
template<typename T> struct def<shared_ptr<T>>	: TISO_virtualpointer<shared_ptr<T>> {};

template<typename T, typename B> struct def<soft_pointer<T, B>> : TISO_virtualpointer<soft_pointer<T, B>> {};

//-------------------------------------
// containers

template<typename I, typename V=void> struct has_key : T_false {};
template<typename I> struct has_key<I, void_t<decltype(tag2(declval<I>().key()))>> : T_true {};

template<typename K, typename T> enable_if_t<T_conversion<const tag2, K>::exists, int> _GetIndex(const tag2& id, const hash_map<K, T> &c, int from) {
	return c.index_of(c.check(id));
}
template<typename K, typename V> enable_if_t<T_conversion<const tag2, K>::exists, int> _GetIndex(const tag2& id, const map<K, V> &c, int from) {
	if (auto i = c.find(id))
		return int(distance(c.begin(), i));
	return -1;
}
template<typename K, typename V> enable_if_t<T_conversion<const tag2, K>::exists, int> _GetIndex(const tag2& id, const multimap<K, V> &c, int from) {
	if (auto i = c.find(id))
		return int(distance(c.begin(), i));
	return -1;
}

template<typename T> bool _Resize(T& a, int size)										{ return false; }
template<typename T> bool _Update(T& a, const char* s, bool from)						{ return false; }
template<typename T> bool _Convert(T& a, const Type *srce_type, const void *srce_data)	{ return false; }

// uses operator[]
template<typename T> struct TISO_virtualarray : VirtualT2<T> {
	static uint32	Count(T& a)									{ return num_elements32(a); }
	static Browser2 Index(T& a, int i)							{ return MakeBrowser(a[i]); }
	static tag2		GetName(T& a, int i)						{ return _GetIndexName(a, i); }
	static int		GetIndex(T& a, const tag2& id, int from)	{ return _GetIndex(id, a, from); }
	static bool		Resize(T& a, int size)						{ return _Resize(a, size); }
	static bool		Update(T &a, const char* s, bool from)		{ return _Update(a, s, from); }
	static bool		Convert(T& a, const Type *srce_type, const void *srce_data) { return _Convert(a, srce_type, srce_data); }
};

template<typename T> struct TISO_virtualarray_ptr : TISO_virtualarray<T> {
	static Browser2 Index(T& a, int i) { return MakePtrCheck(tag2(), a[i]); }
};

template<typename T> struct def<ptr_array<T>>				: TISO_virtualarray<ptr_array<T>>		{};
template<typename T> struct def<dynamic_array<T>>			: TISO_virtualarray<dynamic_array<T>>	{};
template<typename T> struct def<range<T>>					: TISO_virtualarray<range<T>>			{};
template<typename T> struct def<cached_range<T>>			: TISO_virtualarray<cached_range<T>>	{};
template<typename T, int N> struct def<split_range<T, N>>	: TISO_virtualarray<split_range<T, N>>	{};

template<typename T, typename I>	tag2	_GetName(const sparse_element<T, I>& t)	{ return _GetName(*t); }
template<typename T, typename I, typename S> struct def<sparse_array<T, I, S>>	: TISO_virtualarray<dynamic_array<sparse_element<T, I>>>	{};

// uses cached_range to provide operator[]
template<typename T> struct TISO_virtualcachedarray : VirtualT2<T> {
	static Browser2 Deref(const T& a)							{ return MakeBrowser(make_cached_range(a)); }
	static int		GetIndex(T& a, const tag2& id, int from)	{ return _GetIndex(id, a, from); }
	static bool		Resize(T& a, int size)						{ return _Resize(a, size); }
	static bool		Update(T &a, const char* s, bool from)		{ return _Update(a, s, from); }
	static bool		Convert(T& a, const Type *srce_type, const void *srce_data) { return _Convert(a, srce_type, srce_data); }
};

template<typename K, typename T> struct def<hash_map<K, T>> : TISO_virtualcachedarray<hash_map<K, T>>	{};
template<typename K, typename T> struct def<hash_map_with_key<K, T>> : TISO_virtualcachedarray<hash_map_with_key<K, T>> {};
//	static Browser2 Index(hash_map_with_key<K, T>& a, int i)	{ return MakeBrowser(nth(begin(a), i).key_val()); }
//};

template<typename T> struct def<e_list<T>>					: TISO_virtualcachedarray<e_list<T>>		{};
template<typename T> struct def<list<T>>					: TISO_virtualcachedarray<list<T>>			{};
template<typename T> struct def<e_slist<T>>					: TISO_virtualcachedarray<e_slist<T>>		{};
template<typename T> struct def<slist<T>>					: TISO_virtualcachedarray<slist<T>>			{};
template<typename T> struct def<e_slist_tail<T>>			: TISO_virtualcachedarray<e_slist_tail<T>>	{};
template<typename T> struct def<slist_tail<T>>				: TISO_virtualcachedarray<slist_tail<T>>	{};
template<typename T> struct def<circular_list<T>>			: TISO_virtualcachedarray<circular_list<T>>	{};

template<typename T, bool PARENT> struct def<e_treenode<T, PARENT>>	: TISO_virtualcachedarray<e_treenode<T, PARENT>>	{};
template<typename T, typename C> struct def<e_rbtree<T, C>>	: TISO_virtualcachedarray<e_rbtree<T, C>>		{};
//template<typename T, typename C, bool PARENT> struct def<rbtree<T, C, PARENT>>		: TISO_virtualcachedarray<rbtree<T, C, PARENT>>		{};
template<typename K> struct def<set<K>>						: TISO_virtualcachedarray<set<K>>			{};
template<typename K> struct def<multiset<K>>				: TISO_virtualcachedarray<multiset<K>>		{};

template<typename K, typename V> struct def<map<K, V>>		: TISO_virtualcachedarray<map<K, V>>		{};
template<typename K, typename V> struct def<multimap<K, V>>	: TISO_virtualcachedarray<multimap<K, V>>	{};

template<typename K, typename V> bool _Resize(map<K, V> &a, int size) {
	return false;
}

/*
template<typename K, typename V> struct def<map<K, V>> : TISO_virtualarray<map<K, V>> {
	static tag2	GetName(map<K, V>& a, int i)							{ return to_string(nth(a.begin(), i).key()); }
	static int	GetIndex(map<K, V>& a, const tag& id, int from)			{ return _GetIndex(id, a, from); }
};
template<typename K, typename V> struct def<multimap<K, V>> : TISO_virtualarray<multimap<K, V>> {
	static tag2	GetName(multimap<K, V>& a, int i)						{ return to_string(nth(a.begin(), i).key()); }
	static int	GetIndex(multimap<K, V>& a, const tag2& id, int from)	{ return _GetIndex(id, a, from); }
};
*/
template<typename T, typename I> struct def<sparse_element<T, I>> : TypeCompositeN<2> {
	typedef sparse_element<T, I>	S;
	def() : TypeCompositeN<2>((S*)0, "i", &S::i, "t", &S::t) {}
};

//-------------------------------------
// memory

struct memory_block_deref : VirtualDefaults {
	const void*	t;
	TypeArray	type;
	Browser		Deref() const { return Browser(&type, unconst(t)); }
	template<typename T> memory_block_deref(T *t, uint32 len) : t(t), type(getdef<T>(), len, sizeof(T)) {}
	memory_block_deref(const void *t, uint32 len) : t(t), type(getdef<xint8>(), len, 1) {}
};
template<> struct def<memory_block_deref> : VirtualT<memory_block_deref> {};

template<> struct def<memory_block> : TypeUserSave {
	struct virt : VirtualT1<memory_block, virt> {
		static uint32	Count(memory_block& a)				{ return a.size32(); }
		static Browser2 Index(memory_block& a, int i)		{ return MakeBrowser(((xint8*)a)[i]); }
		static Browser2	Deref(memory_block &a)				{ return ptr<memory_block_deref>(0, a, a.size32());	}
		static void		Delete(memory_block& a)				{}
		static bool		Convert(memory_block& a, const Type *type, const void *data) {
			if (TypeType(type) == OPENARRAY)
				a = ((TypeOpenArray*)type)->get_memory(data);
			else
				a = memory_block(unconst(data), type->GetSize());
			return true;
		}
	} v;
	def() : TypeUserSave("Bin", &v) {}
};
template<> struct def<const_memory_block> : TypeUser {
	struct virt : VirtualT1<const_memory_block, virt> {
		static uint32	Count(const_memory_block& a)		{ return a.size32(); }
		static Browser2 Index(const_memory_block& a, int i) { return MakeBrowser(((const xint8*)a)[i]); }
		static Browser2 Deref(const_memory_block& a)		{ return ptr<memory_block_deref>(0, a, a.size32()); }
		static void		Delete(const_memory_block& a)		{}
		static bool		Convert(const_memory_block& a, const Type *type, const void *data) {
			if (TypeType(type) == OPENARRAY)
				a = ((TypeOpenArray*)type)->get_memory(data);
			else
				a = const_memory_block(data, type->GetSize());
			return true;
		}
	} v;
	def() : TypeUser("Bin", &v) {}
};
template<> struct def<malloc_block> : TypeUser {
	struct virt : VirtualT1<malloc_block, virt> {
		static uint32	Count(malloc_block& a)				{ return a.size32(); }
		static Browser2 Index(malloc_block& a, int i)		{ return MakeBrowser(((const xint8*)a)[i]); }
		static Browser2 Deref(malloc_block& a)				{ return ptr<memory_block_deref>(0, a, a.size32()); }
		static void		Delete(malloc_block& a)				{ a.~malloc_block(); }
		static bool		Convert(malloc_block& a, const Type *type, const void *data) {
			if (TypeType(type) == OPENARRAY)
				a = ((TypeOpenArray*)type)->get_memory(data);
			else
				a = const_memory_block(data, type->GetSize());
			return true;
		}
	} v;
	def() : TypeUser("Bin", &v) {}
};

template<typename T, int N> struct def<block<T,N>> : VirtualT2<block<T,N>> {
	static uint32		Count(block<T,N> &a)				{ return a.size(); }
	static Browser2		Index(block<T,N> &a, int i)			{ return MakeBrowser(a[i]);	}
	static void			Delete(block<T,N> &a)				{}
	static bool			Convert(block<T,N> &a, const Type *type, const void *data) {
		if (type->GetType() == ARRAY) {
			auto	*t = (TypeArray*)type;
			if (def<block<T, N-1>>::Convert(a, t->subtype, data)) {
				a = block<T, N>(a, t->subsize * t->count, t->count);
				return true;
			}
		} else if (type->Is<auto_block<T,N>>()) {
			a = *(auto_block<T,N>*)data;
			return true;
		}
		return false;
	}
};

template<typename T> struct def<block<T,1>> : VirtualT2<block<T,1>> {
	static uint32		Count(block<T,1> &a)				{ return a.size(); }
	static Browser2		Index(block<T,1> &a, int i)			{ return MakeBrowser(a[i]); }
	static Browser2		Deref(block<T,1> &a)				{ return ptr<memory_block_deref>(0, a.begin(), a.size()); }
	static void			Delete(block<T,1> &a)				{}
	static bool			Convert(block<T,1> &a, const Type *type, const void *data)	{
		if (type->GetType() == ARRAY) {
			auto	*t = (TypeArray*)type;
			if (t->subtype->Is<T>()) {
				a = block<T, 1>((T*)data, t->count);
				return true;
			}
		}
		return false;
	}
};

template<typename T, int N> struct def<auto_block<T,N>> : VirtualT2<block<T,N>> {
	static void			Delete(auto_block<T,N> &a)		{ a.~auto_block<T,N>(); }
};

template<typename T, typename A = xint64> struct VStartBin : pair<A, T> {
	typedef pair<A, T>	B;
	VStartBin() : B(0) {}
	template<typename U> VStartBin(A a, U&& b) : B(a, forward<U>(b)) {}
};

template<typename T, typename A> struct def<VStartBin<T,A>> : TypeUser { def() : TypeUser("StartBin", getdef<typename VStartBin<T>::B>()) {} };

struct StartBin : VStartBin<OpenArray<xint8> > {
	StartBin()											{}
	StartBin(uint64 addr, uint32 size, const void *p)	{ a = addr; memcpy(b.Create(size, false), p, size); }
};

struct StartBinMachine : VStartBin<OpenArray<xint8, PTRBITS> > {
	StartBinMachine()											{}
	StartBinMachine(uint64 addr, uint32 size, const void *p)	{ a = addr; memcpy(b.Create(size, false), p, size); }
};

ISO_DEFUSER(StartBin, StartBin::B);

//-------------------------------------
// others

template<typename U, typename C, int N> struct def<_pascal_string<U, C, N> > : VirtualT2<_pascal_string<U, C, N> > {
	static Browser2	Deref(_pascal_string<U, C, N> &a) { return ptr<string>(0, str(a.buffer, a.len));	}
};

template<typename U, typename C, int N> struct def<string_base<_pascal_string<U, C, N>>> : VirtualT2<string_base<_pascal_string<U, C, N> > > {
	static Browser2	Deref(string_base<_pascal_string<U, C, N> > &a) { return ptr<string>(0, a);	}
};
template<> struct def<string_base<embedded<char>>> : VirtualT2<string_base<embedded<char>>> {
	static Browser2 Deref(string_base<embedded<char>>& a) { return ptr<string>(0, a); }
};
template<> struct def<string_base<embedded<char16>>> : VirtualT2<string_base<embedded<char16>>> {
	static Browser2 Deref(string_base<embedded<char16>>& a) { return ptr<string16>(0, a); }
};

template<typename T> struct def<optional<T>> : VirtualT2<optional<T>> {
	static Browser2	Deref(optional<T> &a) { return a.exists() ? MakeBrowser(get(a)) : Browser2();	}
};

template<typename T> struct VirtualGet : VirtualT2<T> {
	static Browser2	Deref(T &a) { return MakeBrowser(get(a));	}
};

template<typename T, typename B>	struct def<after<T, B>>			: VirtualGet<after<T, B>>		{};
template<typename T, int S, int N>	struct def<bitfield<T, S, N>>	: VirtualGet<bitfield<T, S, N>> {};
template<int N, bool BE>			struct def<uintn<N, BE>>		: VirtualGet<uintn<N, BE>>		{};

//-----------------------------------------------------------------------------
//	combine
//-----------------------------------------------------------------------------

template<typename A, typename B> struct combiner : pair<A, B>, VirtualDefaults {
	using pair<A, B>::a;
	using pair<A, B>::b;
	int na, nb;

	int		Count() {
		if (!nb)
			nb = this->b.Count();
		return na + nb;
	}
	tag2		GetName(int i)	{ return i < na ? this->a.GetName(i) : this->b.GetName(i - na); }
	Browser2	Index(int i)	{ return i < na ? this->a.Index(i) : this->b.Index(i - na); }
	int			GetIndex(const tag2& id, int from) {
		int n = from < na ? this->a.GetIndex(id, from) : -1;
		if (n == -1) {
			n = this->b.GetIndex(id, from < na ? 0 : from - na);
			if (n != -1)
				n += na;
		}
		return n;
	}
	combiner(const A& a, const B& b) : pair<A, B>(a, b), na(a.Count()), nb(0) {}
};

template<typename A, typename B> ptr<combiner<A, B>> combine(tag2 id, const A& a, const B& b)	{ return ptr<combiner<A, B>>(id, a, b); }
template<typename A, typename B> combiner<A, B> combine(const A& a, const B& b)					{ return combiner<A, B>(a, b); }

template<typename A, typename B> struct def<combiner<A, B>> : public VirtualT<combiner<A, B>> {};

//-----------------------------------------------------------------------------
//	BitPacked
//-----------------------------------------------------------------------------

struct BitPacked : VirtualT1<uint8, BitPacked>, trailing_array2<BitPacked, Element> {
	static ptr<uint64> GetTemp(void* tag);

	uint32 count;

	uint32		size()				const	{ return count; }
	uint32		Count(uint8& data)			{ return count; }
	tag2		GetName(uint8& data, int i) { return begin()[i].id.get_tag(); }
	int			GetIndex(uint8& data, const tag2& id, int from);
	Browser2	Index(uint8& data, int i);
	bool		Update(uint8& data, const char* spec, bool from);

	void						Add(const Type* type, tag id, int start, int bits) { begin()[count++].set(id, type, start, bits); }
	template<typename T> void	Add(tag id, int start, int bits) { Add(getdef<T>(), id, start, bits); }
	void						Add(const Type* type, tag id, int bits) { Add(type, id, count ? back().end() : 0, bits); }
	template<typename T> void	Add(tag id, int bits) { Add(getdef<T>(), id, bits); }

	BitPacked(int n = 0) : count(n) {}
	void*	operator new(size_t size, void* p) { return p; }
	void*	operator new(size_t, uint32 n) { return Type::operator new(calc_size(n)); }
	void	operator delete(void* p, size_t, uint32) { Type::operator delete(p); }
};

template<int N> struct TISO_bitpacked : BitPacked { Element fields[N]; };
#define BITFIELD(P, S, F) { union { iso::uint_t<sizeof(S)> u; S s; } x; x.u = 0; x.s.F = ~0; P.Add(getdef(x.s.F), #F, lowest_set_index(x.u), highest_set_index(x.u) - lowest_set_index(x.u) + 1); }

//-----------------------------------------------------------------------------
//	Browser:iterator
//-----------------------------------------------------------------------------

class Browser::iterator {
protected:
	const Type* type;
	uint8*		start;
	size_t		stride;
	union {
		uintptr_t		current;
		void*			data;
		const Element*	e;
	};

public:
	iterator() : type(0), start(0), stride(0), current(0) {}
	iterator(const Type* type, void* data, size_t stride) : type(type), start(0), stride(stride), data(data) {}
	iterator(void* data, const Element* e) : type(0), start((uint8*)data), stride(sizeof(Element)), e(e) {}
	iterator(const Virtual* type, void* data, uint32 index) : type(type), start((uint8*)data), stride(1), current(index) {}

	Browser2 	operator*() const {
		if (!start)
			return Browser(type, data);
		if (!type)
			return Browser(e->type, start + e->offset);
		return ((Virtual*)type)->Index((Virtual*)type, start, int(current));
	}
	Browser2 	Deref() const {
		if (!start)
			return type->GetType() == REFERENCE ? *Browser(type, data) : Browser2(type, data, ISO_NULL);
		if (!type)
			return Browser(e->type, start + e->offset);
		return ((Virtual*)type)->Index((Virtual*)type, start, int(current));
	}

	iterator&	operator++()	{ current += stride; return *this; }
	iterator&	operator--()	{ current -= stride; return *this; }
	iterator	operator++(int)	{ iterator b(*this); operator++(); return b; }
	iterator	operator--(int)	{ iterator b(*this); operator--(); return b; }

	bool		operator==(const iterator& b)	const { return current == b.current; }
	bool		operator!=(const iterator& b)	const { return current != b.current; }
	ref_helper<Browser2> operator->()			const { return Deref(); }

	tag2		GetName()	const {
		return	!start	? (SkipUserType(type) == REFERENCE ? ((ptr<void>*)data)->ID() : tag2())
			:	!type	? e->id.get_tag2(0)
			:	((Virtual*)type)->GetName((Virtual*)type, start, int(current));
	}
	size_t		GetStride()	const {
		return !start && type ? stride : 0;
	}
	const Type*	GetType()	const {
		return !start ? type : 0;
	}

	intptr_t	operator-(const iterator &b) const { return (current - b.current) / stride; }
};

class Browser2::iterator : public Browser::iterator {
	ptr_machine<void> p;

public:
	iterator() : Browser::iterator() {}
	iterator(const Browser::iterator& i, const ptr_machine<void>& p) : Browser::iterator(i), p(p) {}

	Browser2 	Deref() const {
		if (!start)
			return type->GetType() == REFERENCE ? *Browser2(type, data, p) : Browser2(type, data, p);
		if (!type)
			return Browser2(e->type, start + e->offset, p);
		return ((Virtual*)type)->Index((Virtual*)type, start, int(current));
	}

	iterator&	operator++()	{ current += stride; return *this; }
	iterator&	operator--()	{ current -= stride; return *this; }
	iterator	operator++(int) { iterator b(*this); operator++(); return b; }
	iterator	operator--(int) { iterator b(*this); operator--(); return b; }
};

//-----------------------------------------------------------------------------
//	Browser set/get
//-----------------------------------------------------------------------------


inline bool Browser::IsVirtPtr() const { return type && type->GetType() == VIRTUAL && !type->Fixed() && ((Virtual*)type)->IsVirtPtr(data); }

template<typename T> bool Browser::SetMember(tag2 id, const T& t) const {
	if (Browser b = (*this)[id])
		return b.Set(t);
	void* data = this->data;
	for (const Type* type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite&	comp	= *(TypeComposite*)type;
				int				i		= comp.GetIndex(id);
				return i < 0 ? false : Browser(comp[i].type, (char*)data + comp[i].offset).Set(t);
			}
			case OPENARRAY:
				if (((TypeOpenArray*)type)->subtype->SkipUser()->GetType() == REFERENCE) {
					((anything*)data)->Append(ptr<T>(id, t));
					return true;
				}
				return false;

			case REFERENCE:
				data = type->ReadPtr(data);
				type = !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual* virt = (Virtual*)type;
					return virt->Deref(virt, data).SetMember(id, t);
				}
				fallthrough

			default:
				return false;
		}
	}
	return false;
}
template<typename T, int B> bool Browser::SetMember(tag2 id, ptr<T, B> t) const {
	void* data = this->data;
	for (const Type* type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite&	comp	= *(TypeComposite*)type;
				int				i		= comp.GetIndex(id);
				return i >= 0 && Browser(comp[i].type, (char*)data + comp[i].offset).Set(t);
			}
			case OPENARRAY:
				if (((TypeOpenArray*)type)->subtype->SkipUser()->GetType() == REFERENCE) {
					if (!t.ID())
						t.SetID(id);
					else if (t.ID() != id)
						t = ISO::Duplicate(id, t);

					anything&	a = *(anything*)data;
					int			i = a.GetIndex(id);
					if (i < 0) {
						if (t)
							((anything*)data)->Append(t);
					} else if (t) {
						a[i] = t;
					} else {
						a.Remove(i);
					}
					return true;
				}
				return false;

			case REFERENCE:
				data = type->ReadPtr(data);
				type = !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual* virt = (Virtual*)type;
					return virt->Deref(virt, data).SetMember(id, t);
				}
				fallthrough
			default:
				return false;
		}
	}
	return false;
}

bool soft_set(void *dst, const Type *dtype, const void *src, const Type *stype);

template<typename T, typename V> struct Browser::s_setget {
	static bool	get(void* data, const Type* type, T& t) {
		return soft_set(&t, getdef<T>(), data, type);
	}
	static bool set(void* data, const Type* type, const T& t) {
		return soft_set(data, type, &t, getdef<T>());
	}
};

template<> bool Browser::s_setget<bool>::get(void* data, const Type* type, bool &t);

template<typename T> struct Browser::s_setget<T, enable_if_t<is_builtin_num<T>>> {
	static bool get(void* data, const Type* type, T& t) {
		while (type) {
			switch (type->GetType()) {
				case INT:
					t = T(((TypeInt*)type)->get<(uint32)sizeof(T) * 8>(data)) / ((TypeInt*)type)->frac_factor();
					return true;
				case FLOAT:
					t = ((TypeFloat*)type)->num_bits() > 32 ? T(((TypeFloat*)type)->get64(data)) : T(((TypeFloat*)type)->get(data));
					return true;
				case STRING:
					if (const void* p = type->ReadPtr(data)) {
						switch (((TypeString*)type)->log2_char_size()) {
							case 0:	return !!from_string((const char*)p, t);
							//case 1:	from_string((const char16*)p, t);
						}
					}
					return false;
				case REFERENCE: {
					void* p = type->ReadPtr(data);
					type	= !p || GetHeader(p)->IsExternal() ? 0 : GetHeader(p)->GetType();
					data	= p;
					break;
				}
				case USER: type = ((TypeUser*)type)->subtype; break;
				case VIRTUAL:
					if (!type->Fixed())
						return ((Virtual*)type)->Deref((Virtual*)type, data).Read(t);
					fallthrough
				default: return false;
			}
		}
		return false;
	}
	static bool set(void* data, const Type* type, const T& t) {
		while (type) {
			switch (type->GetType()) {
				case INT:	((TypeInt*)type)->set(data, int64(t * ((TypeInt*)type)->frac_factor())); return true;
				case FLOAT:	((TypeFloat*)type)->set(data, float(t)); return true;
				case REFERENCE: {
					void* p = type->ReadPtr(data);
					type	= !p || GetHeader(p)->IsExternal() ? 0 : GetHeader(p)->GetType();
					data	= p;
					break;
				}
				case USER: type = ((TypeUser*)type)->subtype; break;
				case VIRTUAL:
					if (!type->Fixed())
						return ((Virtual*)type)->Deref((Virtual*)type, data).Set(t);
					fallthrough
				default: return false;
			}
		}
		return false;
	}
};

template<> struct Browser::s_setget<const char*> {
	static iso_export bool	get(void* data, const Type* type, const char*& t);
	static iso_export bool	set(void* data, const Type* type, const char* t);
};
template<> struct Browser::s_setget<string> : Browser::s_setget<const char*> {
	static iso_export bool	get(void* data, const Type* type, string& t);
};

template<typename T> struct Browser::s_setget<const T*> {
	static bool get(void* data, const Type* type, T*& t);
};

template<typename T> bool Browser::s_setget<const T*>::get(void* data, const Type* type, T*& t) {
	while (type) {
		if (type->SameAs(getdef<T>())) {
			t = (T*)data;
			return true;
		}
		switch (type->GetType()) {
			case REFERENCE: {
				void* p = type->ReadPtr(data);
				type	= !p || GetHeader(p)->IsExternal() ? 0 : GetHeader(p)->GetType();
				data	= p;
				break;
			}
			case OPENARRAY:
				data = type->ReadPtr(data);
				type = ((TypeOpenArray*)type)->subtype;
				break;
			case USER:
				type = ((TypeUser*)type)->subtype;
				break;
			case VIRTUAL:
				if (!type->Fixed())
					return ((Virtual*)type)->Deref((Virtual*)type, data).Get(t);
			default:
				return false;
		}
	}
	return false;
}
template<typename T> struct Browser::s_setget<T*> {
	static bool		get(void* data, const Type* type, T*& t)	{ return Browser::s_setget<const T*>::get(data, type, t); }
	static bool		set(void* data, const Type* type, T* t)		{ return Browser::s_setget<const T*>::set(data, type, t); }
};

template<int B> struct Browser::s_setget<ptr<void, B>> {
	static bool get(void* data, const Type* type, ptr<void, B>& t) {
		type = type->SkipUser();
		if (type->GetType() == REFERENCE) {
			t = GetPtr<B>(type->ReadPtr(data));
			return true;
		}
		return false;
	}
	static bool set(void* data, const Type* type, const ptr<void, B>& t) {
		if (!data)
			return false;
		const Type* type2 = type->SkipUser();
		if (type2 && type2->GetType() == REFERENCE) {
			if (type2->SubType()->SameAs(t.GetType(), MATCH_MATCHNULLS)) {
				if (type2->Is64Bit())
					*(ptr<void, 64>*)data = t;
				else
					*(ptr<void, 32>*)data = t;
				return true;
			}
		} else if (type->SameAs(t.GetType(), MATCH_MATCHNULLS)) {
			CopyData(t.GetType(), t, data);
			return true;
		} else {
			const Type* type1 = t.GetType()->SkipUser();
			if (type1 && type1->GetType() == STRING)
				return s_setget<const char*>::set(data, type, (const char*)type1->ReadPtr(t));
		}
		return false;
	}
};

template<bool copyable> struct maybe_copy {
	template<typename T> static bool f(T* dest, const T &src) {
		*dest = src;
		return true;
	}
};
template<> struct maybe_copy<false> {
	template<typename T> static bool f(T* dest, const T &src) {
		return false;
	}
};

template<typename T, int B> struct Browser::s_setget<ptr<T, B>> {
	static bool get(void* data, const Type* type, ptr<T, B>& t) {
		type = type->SkipUser();
		if (type->GetType() == REFERENCE && (data = type->ReadPtr(data)) && GetHeader(data)->IsType<T>()) {
			t = GetPtr<B>(data);
			return true;
		}
		return false;
	}
	static bool set(void* data, const Type* type, const ptr<T, B>& t) {
		const Type* type2 = type->SkipUser();
		const Type* type0 = getdef<T>();
		if (type2->GetType() == REFERENCE) {
			if (type2->Is64Bit()) {
				ptr<void, 64>* p = (ptr<void, 64>*)data;
				if (p->IsType(type0, MATCH_MATCHNULLS)) {
					*p = t;
					return true;
				}
			} else {
				ptr<void, 32>* p = (ptr<void, 32>*)data;
				if (p->IsType(type0, MATCH_MATCHNULLS)) {
					*p = t;
					return true;
				}
			}
		}
		if (type->SameAs(type0, MATCH_MATCHNULLS))
			return maybe_copy<T_assignable<T,T>::value>::f((T*)data, *t);

		return false;
	}
};

template<typename T, int B> struct Browser::s_setget<OpenArray<T, B>> {
	static bool get(void* data, const Type* type, OpenArray<T, B>& t) {
		const Type* type2 = type->SkipUser();
//		if (type2->GetType() == VIRTUAL)
//			return ((Virtual*)type2)->Deref((Virtual*)type2, data).Get(t);
		if (TypeType(type2) == OPENARRAY && ((TypeOpenArray*)type2)->subtype->SameAs<T>(MATCH_MATCHNULLS)) {
			t = OpenArray<T, B>::Ptr((T*)type2->ReadPtr(data));
			return true;
		}
		return false;
	}
	static bool set(void* data, const Type* type, const OpenArray<T, B>& t) {
		const Type* type2 = type->SkipUser();
		if (type2->GetType() == REFERENCE) {
			ptr<void> p = GetPtr(type->ReadPtr(data));
			return set(p, p.GetType(), t);
		}
		if (type2->GetType() == OPENARRAY && ((TypeOpenArray*)type2)->subtype->SameAs<T>(MATCH_MATCHNULLS)) {
			type2->WritePtr(&t[0]);
			return true;
		}
		return false;
	}
};

// template<typename T, size_t N> struct Browser::s_setget<T[N]> {
//	static bool	set(void *data, const Type *type, T (&t)[N]);
//	static T	get(void *data, const Type *type, T (&t)[N]);
//};
//-----------------------------------------------------------------------------
//	meta
//-----------------------------------------------------------------------------

ISO_DEFSAME(bool, bool8);

template<> struct def<tag2> : VirtualT2<tag2> {
	static Browser2 Deref(tag2& a) {
		if (const tag& t = a.get_tag())
			return MakeBrowser(t);
		return MakeBrowser(a.get_crc32());
	}
};

template<> struct def<Type> : VirtualT2<Type> {
	static Browser2 Deref(const Type& a) {
		switch (a.GetType()) {
			case INT: {
				TypeInt& i = (TypeInt&)a;
				if (!(a.flags & TypeInt::ENUM))
					return MakeBrowser(i);
				switch (i.GetSize()) {
					case 1:	return MakeBrowser((TypeEnumT<uint8>&)a);
					case 2:	return MakeBrowser((TypeEnumT<uint16>&)a);
					default:return MakeBrowser((TypeEnumT<uint32>&)a);
					case 8:	return MakeBrowser((TypeEnumT<uint64>&)a);
				}
			}
			case FLOAT:		return MakeBrowser((TypeFloat&)a);
			case STRING:	return MakeBrowser((TypeString&)a);
			case COMPOSITE:	return MakeBrowser((TypeComposite&)a);
			case ARRAY:		return MakeBrowser((TypeArray&)a);
			case OPENARRAY:	return MakeBrowser((TypeOpenArray&)a);
			case REFERENCE:	return MakeBrowser((TypeReference&)a);
			case VIRTUAL:	return MakeBrowser((Virtual&)a);
			case USER:		return MakeBrowser((TypeUser&)a);
			case FUNCTION:	return MakeBrowser((TypeFunction&)a);
			default:		return Browser();
		}
	}
};

ISO_DEFUSERCOMPV(TypeArray, count, subtype);
ISO_DEFUSERCOMPV(TypeOpenArray, subtype);
ISO_DEFUSERCOMPV(TypeReference, subtype);
ISO_DEFUSERCOMPV(TypeFunction, rettype, params);
ISO_DEFUSERCOMPV(TypeUser, ID, subtype);
ISO_DEFUSERCOMPV(TypeInt, is_signed, num_bits);
ISO_DEFUSERCOMPV(TypeFloat, is_signed, num_bits, exponent_bits);
ISO_DEFUSERCOMPV(TypeString, char_size);

template<typename T> struct def<TypeEnumT<T>> : VirtualT2<TypeEnumT<T>> {
	static uint32	Count(TypeEnumT<T>& a)				{ return a.size(); }
	static Browser	Index(TypeEnumT<T>& a, int i)		{ return MakeBrowser(a[i].value); }
	static tag2		GetName(TypeEnumT<T>& a, int i)		{ return a[i].id.get_tag2(0); }
};

ISO_DEFUSERCOMPV(Element, type, offset);
template<> struct def<TypeComposite> : VirtualT2<TypeComposite> {
	static uint32	Count(TypeComposite& a)				{ return a.Count(); }
	static Browser	Index(TypeComposite& a, int i)		{ return MakeBrowser(static_cast<TypeComposite&>(a)[i]); }
	static tag2		GetName(TypeComposite& a, int i)	{ return static_cast<TypeComposite&>(a).GetID(i); }
	static int		GetIndex(TypeComposite& a, const tag2& id, int from) { return static_cast<TypeComposite&>(a).GetIndex(id, from); }
};

template<> struct def<BitPacked> : VirtualT2<BitPacked> {
	static uint32	Count(BitPacked& a)					{ return a.count; }
	static Browser	Index(BitPacked& a, int i)			{ return MakeBrowser(a[i]); }
	static tag2		GetName(BitPacked& a, int i)		{ return a[i].id.get_tag2(0); }
};

template<> struct def<Virtual> : TypeComposite {};

} // namespace ISO

template<typename T> inline void*	operator new(size_t size, ISO::OpenArray<T>& a) { return a._Append(); }
template<typename T> inline void	operator delete(void* p, ISO::OpenArray<T>& a) {}

//#include "iso_compat.h"
namespace iso {
void Init(crc32*, void*);
void DeInit(crc32*);

using tag		           = ISO::tag;
using tag1		           = ISO::tag1;
using tag2		           = ISO::tag2;

template<typename T, int B = 32>	using ISO_ptr			= ISO::ptr<T, B>;
template<typename T>				using ISO_ptr64			= ISO::ptr<T, 64>;
template<class T>					using ISO_ptr_machine	= ISO::ptr_machine<T>;
template<typename T, int B = 32>	using ISO_openarray		= ISO::OpenArray<T,B>;
template<typename T>				using ISO_openarray_machine		= ISO::OpenArray<T,ISO::PTRBITS>;
template<typename T> using iso_ptr32 = typename ISO::ptr_type<T, 32>::type;

using ISO::anything;
using ISO::anything_machine;

inline uint32	vram_align(uint32 a)				{ return ISO::iso_bin_allocator().align(a); }
inline uint32	vram_offset()						{ return ISO::iso_bin_allocator().offset(); }
inline uint32	vram_offset(const void *p)			{ return ISO::iso_bin_allocator().offset(p); }
inline uint32	vram_add(const void *p, size_t n)	{ return ISO::iso_bin_allocator().add(p, n); }
inline uint32	vram_add(const memory_block &b)		{ return ISO::iso_bin_allocator().add(b, b.n); }
inline void		vram_not0()							{ ISO::iso_bin_allocator().not0(); }

}

#endif  // ISO_H
