#ifndef ISO_H
#define ISO_H

#include "base/strings.h"
#include "base/list.h"
#include "base/tree.h"
#include "base/array.h"
#include "base/hash.h"
#include "base/bits.h"
#include "base/pointer.h"
#include "base/shared_ptr.h"
#include "allocators/allocator.h"
#include "iso_custominit.h"
#include "crc32.h"
#include "thread.h"

#define ISO_NULL	iso::ISO_ptr<void>()

namespace iso {
iso_export const char *store_string(const char *id);
template<> iso_export void *base_fixed<void>::get_base();
template<> iso_export void **base_select<void,2>::get_base();
template<> iso_export void base_select<void,2>::set(const void *p);
struct ISO_type;

//-----------------------------------------------------------------------------
//	pointers
//-----------------------------------------------------------------------------

static const int PTRBITS = sizeof(void*) * 8;

template<typename T, int N, int B = PTRBITS> struct iso_pointer_type;

template<typename T, int N> struct iso_pointer_type<T, N, N>{ typedef pointer<T>											type;};
template<typename T> struct iso_pointer_type<T, 32, 64>		{ typedef soft_pointer<T, base_fixed_shift<void,2> >			type;};
template<typename T> struct iso_pointer_type<T, 64, 32>		{ typedef soft_pointer<T, base_absolute<uint64> >				type;};
template<> struct iso_pointer_type<const ISO_type, 32, 64>	{ typedef soft_pointer<const ISO_type, base_select<void, 2> >	type;};

//template<> struct iso_pointer_type<char, 32, 64>	{ typedef soft_pointer<char, base_fixed<void> >	type;};

typedef iso_pointer_type<void, 32>::type					iso_void_ptr32;
typedef iso_pointer_type<void, 64>::type					iso_void_ptr64;
typedef iso_pointer_type<const ISO_type, 32>::type			ISO_type_ptr;

//template<typename T, int N, int B> inline size_t to_string(char *s, const typename iso_pointer_type<T, N, B>::type &t) {
//	return to_string(s, t.get());
//}

//-----------------------------------------------------------------------------
//	allocation
//-----------------------------------------------------------------------------

struct ISO_bin_allocator : vallocator {
	void	(*vtransfer)(void*, void*, const void*, size_t);
	uint32	(*vfix)		(void*, void*, size_t);
	void*	(*vunfix)	(void*, uint32);

	template<class T> struct thunk {
		static void	transfer(void *me, void *d, const void *s, size_t size)	{ ((T*)me)->transfer(d, s, size);	}
		static uint32	fix	(void *me, void *p, size_t size)				{ return ((T*)me)->fix(p, size);	}
		static void*	unfix(void *me, uint32 p)							{ return ((T*)me)->unfix(p);		}
	};
	template<typename T> void set(T *t) {
		vallocator::set(t);
		vtransfer	= thunk<T>::transfer;
		vfix		= thunk<T>::fix;
		vunfix		= thunk<T>::unfix;
	}

	void	transfer(void *d, const void *s, size_t size)		const	{ vtransfer(me, d, s, size);			}
	uint32	fix(void *p, size_t size = 0)						const	{ return vfix(me, p, size);				}
	void*	unfix(uint32 p)										const	{ return vunfix(me, p);					}
};
iso_export ISO_bin_allocator&	iso_bin_allocator();

struct ISO_allocate_base {
	enum FLAGS {
		TOOL_DELETE	= 1 << 0,
	};
	static iso::flags<FLAGS>				flags;
};

template<int B> struct ISO_allocate {
	static iso_export vallocator&	allocator();
	template<typename T> static void set(T *t)							{ allocator().set(t); }

	inline static void*	alloc(size_t size, size_t align = 16)			{ return allocator().alloc(size, align);		}
	inline static void*	realloc(void *p, size_t size, size_t align = 16){ return allocator().realloc(p, size, align);	}
	inline static bool	free(void *p)									{ return allocator().free(p);					}
	/*
	inline static char*	dup(const char *s) {
		char *m = 0;
		if (size_t n = string_len(s)) {
			memcpy(m = (char*)alloc(n + 1), s, n);
			m[n] = 0;
		}
		return m;
	}*/
};

//-----------------------------------------------------------------------------
//	iso_string8
//-----------------------------------------------------------------------------

struct iso_string8 : public string_base<iso_pointer_type<char, 32>::type> {
	typedef string_base<iso_pointer_type<char, 32>::type> B;
	inline static char*	dup(const char *s) {
		char *m = 0;
		if (size_t n = string_len(s)) {
			memcpy(m = (char*)ISO_allocate<32>::alloc(n + 1), s, n);
			m[n] = 0;
		}
		return m;
	}

	iso_string8()						: B(0)				{}
	iso_string8(const iso_string8 &b)	: B(b)				{ const_cast<iso_string8&>(b).p = 0; }
	iso_string8(const char *s)	: B(dup(s))					{}
	~iso_string8()											{ ISO_allocate<32>::free(p); }
	iso_string8&	operator=(const char *s)				{ ISO_allocate<32>::free(p); p = dup(s); return *this; }
};

//-----------------------------------------------------------------------------
//	tag
//-----------------------------------------------------------------------------

// tag - text based id
class tag : public string_base<const char*> {
	typedef string_base<const char*>	B;
public:
	constexpr tag()										: B(0)	{}
	constexpr tag(int)									: B(0)	{}
	template<int N> tag(const char (&p)[N])				: B(store_string(p))	{}
	template<int N> tag(char (&p)[N])					: B(store_string(p))	{}
	template<typename T> tag(const T *const &p, typename T_same<T,char>::type *x = 0) : B(store_string(str8(p)))	{}
	template<typename T> tag(const string_base<T> &p)	: B(store_string(str8(p)))	{}
};

// tag2 - holder of text and crc based versions of an id
class tag2 {
	iso::tag			t;
	mutable iso::crc32	c;
public:
	constexpr tag2()										{}
	constexpr tag2(iso::tag _t)				: t(_t)			{}
	constexpr tag2(int i)					: c(i)			{}	// to catch nulls
	explicit constexpr tag2(iso::crc32 _c)	: c(_c)			{}
	explicit constexpr tag2(uint32 _c)		: c(_c)			{}

	template<int N>			tag2(const char (&p)[N])		: t(p)	{}
	template<int N>			tag2(char (&p)[N])				: t(p)	{}
	template<typename T>	tag2(const T *const &p, typename T_same<T,char>::type *x = 0) : t(p)	{}
	template<typename T>	tag2(const string_base<T> &p)	: t(p)	{}

	operator bool()		const	{ return safe_bool::test(t || c);}
	operator iso::tag()				const	{ return t;			}
	operator		iso::crc32()	const	{ if (!c) return t; return c;	}
	operator		iso::tag&()				{ return t; }
	operator		iso::crc32&()			{ if (!c) c = t; return c;	}

	iso::tag		tag()			const	{ return t; }
	iso::crc32		crc32()			const	{ return operator iso::crc32();	}
	iso::tag&		tag()					{ return t; }
	iso::crc32&		crc32()					{ return operator iso::crc32&(); }

	friend bool operator==(const tag2 &a, const tag2 &b)		{ return a.t && b.t ? a.t == b.t : a.crc32() == b.crc32(); }
	friend bool operator!=(const tag2 &a, const tag2 &b)		{ return !(a == b); }
};

// storage of id - either a tag or a crc32
class tag1 {
	typedef	iso_pointer_type<const char, 32>::type S;
	uint32	u;
public:
	constexpr tag1()				: u(0) {}
	constexpr tag1(crc32 c)			: u(c) {}
	tag1(const tag &t)	: u(force_cast<uint32>(S(t))) {}
	constexpr bool	blank()		const	{ return u == 0;				}
	crc32	get_crc()			const	{ return *(const crc32*)this;	}
	tag		get_tag()			const	{ return force_cast<tag>(((S*)this)->get());		}
	crc32	get_crc(int iscrc)	const	{ return iscrc ? get_crc()		: crc32(get_tag());	}
	tag		get_tag(int iscrc)	const	{ return iscrc ? tag(0)			: get_tag();		}
	tag2	get_tag2(int iscrc)	const	{ return iscrc ? tag2(get_crc()): tag2(get_tag());	}
	operator uint32()			const	{ return u; }
};


constexpr tag2				__GetName(...) {
	return tag2();
}
template<typename T> tag2	__GetName(const T *t, T_checktype<tag2(T::*)() const, &T::ID> *dummy = 0) {
	return t->ID();
}
template<typename T> tag2	_GetName(const T *t)	{ return __GetName(t); }
template<typename T> tag2	_GetName(T *t)			{ return _GetName(const_cast<const T*>(t)); }
template<typename T> tag2	_GetName(const T &t)	{ return _GetName(&t); }

template<typename C> int	_GetIndex(const tag2 &id, const C &c, int from) {
	for (typename C::const_iterator i = begin(c) + from, e = end(c); i != e; ++i) {
		if (_GetName(*i) == id)
			return int(distance(begin(c), i));
	}
	return -1;
}

inline const char*		to_string(const tag &t)	{ return t; }
iso_export size_t		to_string(char *s, const tag2 &t);
iso_export const char*	to_string(const tag2 &t);

//-----------------------------------------------------------------------------
//	ISO_type plain structures
//-----------------------------------------------------------------------------

enum ISO_TYPE {
	ISO_UNKNOWN,
	ISO_INT,
	ISO_FLOAT,
	ISO_STRING,
	ISO_COMPOSITE,
	ISO_ARRAY,
	ISO_OPENARRAY,
	ISO_REFERENCE,
	ISO_VIRTUAL,
	ISO_USER,
	ISO_FUNCTION,
	ISO_TOTAL,

	TYPE_32BIT			= 0<<4,
	TYPE_64BIT			= 1<<4,
	TYPE_PACKED			= 1<<5,
	TYPE_DODGY			= 1<<6,
	TYPE_FIXED			= 1<<7,
	TYPE_MASK			= 15,
	TYPE_MASKEX			= TYPE_MASK | TYPE_64BIT,

	ISO_STRING64		= ISO_STRING	| TYPE_64BIT,
	ISO_OPENARRAY64		= ISO_OPENARRAY	| TYPE_64BIT,
	ISO_REFERENCE64		= ISO_REFERENCE	| TYPE_64BIT,
};

constexpr ISO_TYPE operator|(ISO_TYPE a, int b)	{ return ISO_TYPE(uint8(a) | b); }

struct ISO_type;
template<typename T> ISO_type *ISO_getdef();
template<typename T> inline ISO_type *ISO_getdef(const T &t)	{ return ISO_getdef<T>();	}

enum {
	ISOMATCH_NOUSERRECURSE		= 1<<0,	// user types are not recursed into for sameness check
	ISOMATCH_NOUSERRECURSE_RHS	= 1<<1,	// RHS user types are not recursed into for sameness check
	ISOMATCH_NOUSERRECURSE_BOTH	= 1<<2,	// if both types are user, do not recurse for sameness check
	ISOMATCH_MATCHNULLS			= 1<<3,	// null types match anything
	ISOMATCH_MATCHNULL_RHS		= 1<<4, // RHS null matches anything
	ISOMATCH_IGNORE_SIZE		= 1<<5, // ignore 64-bittedness
};

struct ISO_type {
	enum FLAGS {
		NONE = 0,
		FLAG0 = 1<<0, FLAG1 = 1<<1, FLAG2 = 1<<2, FLAG3 = 1<<3,
		FLAG4 = 1<<4, FLAG5 = 1<<5, FLAG6 = 1<<6, FLAG7 = 1<<7,
		//creation only flags
		UNSTORED0	= 1<<16,
	};
	struct FLAGS2 {
		uint16	f2;
		constexpr FLAGS2(FLAGS a, FLAGS b = NONE) : f2(uint8(a) | (uint8(b) << 8)) {}
		constexpr operator FLAGS()	const	{ return FLAGS(f2 & 0xff); }
		constexpr FLAGS	second()	const	{ return FLAGS(f2 >> 8); }
	};
	union {
		struct {
			uint8	type, flags;
			union {
				struct {uint8 param1, param2;};
				uint16	param16;
			};
		};
		uint32	u;
	};
	template<FLAGS A, FLAGS B> struct OR				{ static const FLAGS value = FLAGS(int(A) | int(B)); };
	friend constexpr FLAGS operator|(FLAGS a, FLAGS b)	{ return FLAGS(uint8(a) | uint8(b)); }
	friend ISO_TYPE		GetType(const ISO_type *type)	{ return type ? (ISO_TYPE)(type->type & TYPE_MASK) : ISO_UNKNOWN; }

	iso_export static const ISO_type*	SubType(const ISO_type *type);
	iso_export static const ISO_type*	SkipUser(const ISO_type *type);
	iso_export static uint32			GetSize(const ISO_type *type);
	iso_export static uint32			GetAlignment(const ISO_type *type);
	iso_export static bool				Is(const ISO_type *type, const tag2 &id);
	iso_export static bool				ContainsReferences(const ISO_type *type);
	iso_export static bool				IsPlainData(const ISO_type *type, bool flip = false);
	iso_export static bool				Same(const ISO_type *type1, const ISO_type *type2, int criteria);
public:
	constexpr ISO_type(ISO_TYPE t, FLAGS f = NONE) : type(t), flags(f), param1(0), param2(0) {}
	constexpr ISO_type(ISO_TYPE t, FLAGS f, uint8 p1, uint8 p2 = 0) : type(t), flags(f), param1(p1), param2(p2) {}
	constexpr ISO_TYPE			Type()							const { return (ISO_TYPE)(type & TYPE_MASK);	}
	constexpr ISO_TYPE			TypeEx()						const { return (ISO_TYPE)(type & TYPE_MASKEX);}
	const ISO_type*				SubType()						const { return SubType(this); }
	const ISO_type*				SkipUser()						const { return SkipUser(this); }
	uint32						GetSize()						const { return GetSize(this);	 }
	uint32						GetAlignment()					const { return GetAlignment(this); }
	bool						Is(tag2 id)						const { return Is(this, id); }
	bool						ContainsReferences()			const { return ContainsReferences(this); }
	bool						IsPlainData(bool flip = false)	const { return IsPlainData(this, flip); }
	bool						SameAs(const ISO_type *type2, int crit = 0) const { return Same(this, type2, crit); }
	template<typename T> bool	SameAs(int crit = 0)			const { return Same(this, ISO_getdef<T>(), crit); }
	template<typename T> bool	Is()							const { return this == ISO_getdef<T>();		}
	constexpr bool				Dodgy()							const { return !!(type & TYPE_DODGY);			}
	constexpr bool				Fixed()							const { return !!(type & TYPE_FIXED);			}
	constexpr bool				Is64Bit()						const { return !!(type & TYPE_64BIT);			}
	void*						ReadPtr(const void *data)		const { return Is64Bit() ? ((iso_void_ptr64*)data)->get() : ((iso_void_ptr32*)data)->get(); }
	void						WritePtr(void *data, void *p)	const { if (Is64Bit()) *(iso_void_ptr64*)data = p; else *(iso_void_ptr32*)data = p; }

	void*	operator new(size_t size, void *p)	{ return p; }
	void*	operator new(size_t size)			{ return ISO_allocate<32>::alloc(size); }
	void	operator delete(void *p)			{ ISO_allocate<32>::free(p); }
};

iso_export string_accum&	operator<<(string_accum &a, const ISO_type *const type);
inline size_t				to_string(char *s, const ISO_type *const type)	{ return (fixed_accum(s, 64).me() << type).getp() - s; }
inline fixed_string<64>		to_string(const ISO_type *const type)			{ fixed_string<64> s; s << type; return s; }

iso_export const ISO_type	*Duplicate(const ISO_type *type);
iso_export const ISO_type	*Canonical(const ISO_type *type);

//-----------------------------------------------------------------------------
//	ISO_type_int
//-----------------------------------------------------------------------------

struct ISO_type_int : ISO_type {
	static const FLAGS SIGN = FLAG0, HEX = FLAG1, ENUM = FLAG2, NOSWAP = FLAG3, FRAC_ADJUST = FLAG4, CHR = FLAG5;
	constexpr ISO_type_int(uint8 nbits, uint8 frac, FLAGS flags = NONE) : ISO_type(ISO_INT, flags, nbits, frac) {}
	constexpr uint32			GetSize()			const	{ return (param1 + 7) / 8;	}
	constexpr bool				is_signed()			const	{ return !!(flags & SIGN);	}
	constexpr uint8				num_bits()			const	{ return uint8(param1);		}
	constexpr uint8				frac_bits()			const	{ return uint8(param2);		}
	constexpr bool				frac_adjust()		const	{ return !!(flags & FRAC_ADJUST);				}
	constexpr int				frac_factor()		const	{ return bit(frac_bits()) - int(frac_adjust());	}
	constexpr int				get_max() 			const	{ return unsigned(-1) >> (is_signed() ? 33 - param1 : 32 - param1);	}
	constexpr int				get_min() 			const	{ return is_signed() ? -int(bit(param1 - 1)) : 0; }
	template<int nbits, int frac, int flags> static ISO_type_int *make() {
		static ISO_type_int t(nbits, frac, (FLAGS)flags);
		return &t;
	}
	template<typename T> static ISO_type_int *make() {
		return make<sizeof(T) * 8, 0, num_traits<T>::is_signed ? SIGN : NONE>();
	}
};

//-----------------------------------------------------------------------------
//	ISO_type_float
//-----------------------------------------------------------------------------

struct ISO_type_float : ISO_type {
	static const FLAGS SIGN = FLAG0;
	constexpr ISO_type_float(uint8 nbits, uint8 exp, FLAGS flags = NONE) : ISO_type(ISO_FLOAT, flags, nbits, exp) {}
	constexpr uint32			GetSize()			const	{ return (param1 + 7) / 8;	}
	constexpr bool				is_signed()			const	{ return !!(flags & SIGN);	}
	constexpr uint8				num_bits()			const	{ return uint8(param1);		}
	constexpr uint8				exponent_bits()		const	{ return uint8(param2);		}
	constexpr uint8				mantissa_bits()		const	{ return uint8(param1 - param2 - (flags & SIGN ? 1 : 0));	}
	constexpr float				get_max()			const	{ return iorf(bits(min(mantissa_bits(), 23)), bits(min(exponent_bits(), 8)) - 1, 0).f;	}
	constexpr float				get_min()			const	{ return is_signed() ? iorf(bits(min(mantissa_bits(), 23)), bits(min(exponent_bits(), 8)) - 1, 1).f : 0.f;	}
	constexpr double			get_max64()			const	{ return iord(bits(min(mantissa_bits(), 52)), bits(min(exponent_bits(), 11)) - 1, 0).f;	}
	constexpr double			get_min64()			const	{ return is_signed() ? iord(bits(min(mantissa_bits(), 52)), bits(min(exponent_bits(), 11)) - 1, 1).f : 0.f;	}
	template<uint8 nbits, uint8 exp, FLAGS flags> static ISO_type_float *make() {
		static ISO_type_float t(nbits, exp, flags);
		return &t;
	}
	template<typename T> static ISO_type_float *make() {
		return make<sizeof(T) * 8, num_traits<T>::exponent_bits, num_traits<T>::is_signed ? SIGN : NONE>();
	}
};

//-----------------------------------------------------------------------------
//	ISO_type_string
//-----------------------------------------------------------------------------

struct ISO_type_string : ISO_type {
	static const FLAGS UTF16 = FLAG0, UTF32 = FLAG1, ALLOC = FLAG2, UNESCAPED = FLAG3, _MALLOC = FLAG4, MALLOC = OR<ALLOC,_MALLOC>::value;
	ISO_type_string(bool is64bit, FLAGS flags)	: ISO_type(ISO_STRING | (is64bit ? TYPE_64BIT : 0), flags) {}
	constexpr uint32	GetSize()						const	{ return Is64Bit() ? 8 : 4; }
	constexpr uint32	GetAlignment()					const	{ return Is64Bit() ? 8 : 4; }
	constexpr uint32	log2_char_size()				const	{ return flags & (UTF16 | UTF32); }
	constexpr uint32	char_size()						const	{ return 1 << log2_char_size(); }
	const char*			get(const void *data)			const	{ return (const char*)ReadPtr(data); }
	uint32				len(const void *s)				const;
	void*				dup(const void *s)				const;

	void				free(void *data)				const	{
		if (flags & ALLOC) {
			if (flags & _MALLOC)
				iso::free((void*)get(data));
			else if (Is64Bit())
				ISO_allocate<64>::free((void*)get(data));
			else
				ISO_allocate<32>::free((void*)get(data));
		}
	}
	void				set(void *data, const char *s)	const	{
		if (flags & ALLOC) {
			if (flags & _MALLOC)
				s = strdup(s);
			else
				s = (char*)dup(s);
		}
		WritePtr(data, (void*)s);
	}
};

template<typename P, typename C, ISO_type::FLAGS F = ISO_type::NONE> struct TISO_type_string : ISO_type_string {
	constexpr TISO_type_string() : ISO_type_string(sizeof(P) > 4, FLAGS(F | LOG2(sizeof(C)))) {}
//	void				free(void *data)				const	{ A::free((void*)get(data));	}
//	void				set(void *data, const char *s)	const	{ WritePtr(data, (void*)A::dup(s));	}
};

//-----------------------------------------------------------------------------
//	ISO_type_composite
//-----------------------------------------------------------------------------

struct ISO_element {
	tag1				id;
	ISO_type_ptr		type;
	uint32				offset;
	uint32				size;
	uint32	set(tag1 _id, const ISO_type *_type, size_t _offset, size_t _size) {
		id		= _id;
		type	= _type;
		offset	= uint32(_offset);
		size	= uint32(_size);
		return offset + size;
	}
	uint32	set(tag1 _id, const ISO_type *_type, size_t _offset) {
		return set(_id, _type, _offset, _type->GetSize());
	}
	template<typename B, typename T> uint32 set(tag _id, T B::*p) {
		return set(_id, ISO_getdef<T>(), uint32(uintptr_t(&(((B*)0)->*p))), sizeof(T));
	}
	uint32	end() const {
		return offset + size;
	}
};

struct ISO_type_composite : ISO_type {
	static const FLAGS CRCIDS = FLAG0, DEFAULT = FLAG1, RELATIVEBASE = FLAG2, FORCEOFFSETS = FLAG3;
	uint32				count;
	ISO_type_composite(uint32 _count = 0, FLAGS flags = NONE) : ISO_type(ISO_COMPOSITE, flags), count(_count) {}
	void				SetLog2Align(uint32 log2align)		{ param1 = log2align; }
	constexpr uint32	Count()						const	{ return count; }
};

//-----------------------------------------------------------------------------
//	ISO_type_array
//-----------------------------------------------------------------------------

struct ISO_type_array : ISO_type {
	uint32				count;
	ISO_type_ptr		subtype;
	uint32				subsize;
	ISO_type_array(const ISO_type *_subtype, uint32 n, uint32 _subsize) : ISO_type(ISO_ARRAY), count(n), subtype(_subtype), subsize(_subsize) {}
	ISO_type_array(const ISO_type *_subtype, uint32 n) : ISO_type(ISO_ARRAY), count(n), subtype(_subtype), subsize(_subtype->GetSize()) {}
	constexpr uint32	Count()				const	{ return count;				}
	constexpr uint32	GetSize()			const	{ return subsize * count;	}
};

//-----------------------------------------------------------------------------
//	_ISO_type_openarray
//-----------------------------------------------------------------------------

struct _ISO_type_openarray : ISO_type {
	ISO_type_ptr		subtype;
	uint32				subsize;
	_ISO_type_openarray(ISO_TYPE t, const ISO_type *_subtype, uint32 _subsize, uint8 log2align) : ISO_type(t), subtype(_subtype), subsize(_subsize) { param1 = log2align; }
	_ISO_type_openarray(ISO_TYPE t, const ISO_type *_subtype) : ISO_type(t), subtype(_subtype), subsize(_subtype->GetSize()) {}
	constexpr uint32	SubAlignment()		const	{ return 1 << param1; }
};
struct ISO_type_openarray : _ISO_type_openarray {
	ISO_type_openarray(const ISO_type *_subtype, uint32 _subsize, uint8 log2align = 0) : _ISO_type_openarray(ISO_OPENARRAY, _subtype, _subsize, log2align) {}
	ISO_type_openarray(const ISO_type *_subtype) : _ISO_type_openarray(ISO_OPENARRAY, _subtype) {}
};
struct ISO_type_openarray64 : _ISO_type_openarray {
	ISO_type_openarray64(const ISO_type *_subtype, uint32 _subsize, uint8 log2align = 0) : _ISO_type_openarray(ISO_OPENARRAY64, _subtype, _subsize, log2align) {}
	ISO_type_openarray64(const ISO_type *_subtype) : _ISO_type_openarray(ISO_OPENARRAY64, _subtype) {}
};
struct ISO_type_openarrayB : _ISO_type_openarray {
	ISO_type_openarrayB(int b, const ISO_type *_subtype, uint32 _subsize, uint8 log2align = 0) : _ISO_type_openarray(ISO_OPENARRAY | (b == 64 ? TYPE_64BIT : TYPE_32BIT), _subtype, _subsize, log2align) {}
};

//-----------------------------------------------------------------------------
//	ISO_type_reference
//-----------------------------------------------------------------------------

struct _ISO_type_reference : ISO_type {
	static const FLAGS PTR64 = FLAG0;
	ISO_type_ptr		subtype;
	_ISO_type_reference(ISO_TYPE t, const ISO_type *_subtype) : ISO_type(t), subtype(_subtype) {}
};
struct ISO_type_reference : _ISO_type_reference {
	ISO_type_reference(const ISO_type *_subtype) : _ISO_type_reference(ISO_REFERENCE, _subtype) {}
};
struct ISO_type_reference64 : _ISO_type_reference {
	ISO_type_reference64(const ISO_type *_subtype) : _ISO_type_reference(ISO_REFERENCE64, _subtype) {}
};

//-----------------------------------------------------------------------------
//	ISO_type_user
//-----------------------------------------------------------------------------

struct ISO_type_user : ISO_type {
	static const FLAGS INITCALLBACK = FLAG0, FROMFILE = FLAG1, CRCID = FLAG2, CHANGE = FLAG3, LOCALSUBTYPE = FLAG4, WRITETOBIN = FLAG5;
	tag1				id;
	ISO_type_ptr		subtype;
	ISO_type_user(tag2 _id, const ISO_type *_subtype = 0, FLAGS flags = WRITETOBIN) : ISO_type(ISO_USER, flags), subtype(_subtype) { SetID(_id); }
	tag2				ID()				const	{ return id.get_tag2(flags & CRCID);	}
	constexpr bool		KeepExternals()		const	{ return (flags & (CHANGE | WRITETOBIN)) == (CHANGE | WRITETOBIN); }
	void				SetID(tag2 id2)				{ if (tag s = id2) { id = s; flags &= ~CRCID; } else { id = id2.crc32(); flags |= CRCID; }	}
};

//-----------------------------------------------------------------------------
//	ISO_type_function (TBD)
//-----------------------------------------------------------------------------

struct ISO_type_function : ISO_type {
	ISO_type_ptr		rettype;
	ISO_type_ptr		params;
	ISO_type_function(const ISO_type *r, const ISO_type *p) : ISO_type(ISO_FUNCTION), rettype(r), params(p) {}
};

//-----------------------------------------------------------------------------
//	UserTypes
//-----------------------------------------------------------------------------
struct UserTypeArray : dynamic_array<ISO_type_user*> {
	~UserTypeArray();

	ISO_type_user*	Find(const tag2 &id);
	void			Add(ISO_type_user *t);
};
extern iso_export singleton<UserTypeArray> _user_types;
#define user_types _user_types()

//-----------------------------------------------------------------------------
//	ISO_type helper classes
//-----------------------------------------------------------------------------

class CISO_type_int : public ISO_type_int {
public:
	constexpr CISO_type_int(uint8 nbits, uint8 frac = 0, FLAGS flags = NONE) : ISO_type_int(nbits, frac, flags) {}
	void				set(void *data, int64 i)	const	{ write_bytes(data, i, GetSize()); }
	int					get(void *data)				const	{ uint32 v = read_bytes<uint32>(data, GetSize()); return is_signed() ? sign_extend(v, num_bits()) : int(v); }
	int64				get64(void *data)			const	{ uint64 v = read_bytes<uint64>(data, GetSize()); return is_signed() ? sign_extend(v, num_bits()) : int64(v); }
	template<int BITS>	sint_type<BITS> get(void *data) const {
		uint_type<BITS> v = read_bytes<uint_type<BITS>>(data, GetSize()); return is_signed() ? sign_extend(v, num_bits()) : sint_type<BITS>(v);
	}
};

template<typename T> struct TISO_enum {
	tag1				id;
	T					value;
	tag		ID() const { return id.get_tag(); }
	void	set(tag _id, T _value) { id = _id; value = _value; }
};

template<typename T> class TISO_type_enum : public CISO_type_int, public trailing_array<TISO_type_enum<T>, TISO_enum<T> > {
	typedef trailing_array<TISO_type_enum<T>, TISO_enum<T> > B;
public:
	static const FLAGS DISCRETE = FRAC_ADJUST, CRCIDS = CHR;
	constexpr TISO_type_enum(uint8 nbits, FLAGS flags = NONE) : CISO_type_int(nbits, 0, flags | ENUM) {}
	iso_export uint32	num_values()				const;
	iso_export uint32	factors(T f[], uint32 num)	const;
	iso_export const TISO_enum<T>	*biggest_factor(T x)	const;
	void*	operator new(size_t s, int n)	{ return ISO_type::operator new(B::calc_size(s, n)); }
	void	operator delete(void *p, int n)	{ ISO_type::operator delete(p); }
};

typedef TISO_type_enum<uint32>	CISO_type_enum;
typedef TISO_enum<uint32>		ISO_enum;

class CISO_type_float : public ISO_type_float {
public:
	constexpr CISO_type_float(uint8 nbits, uint8 exp, FLAGS flags) : ISO_type_float(nbits, exp, flags) {}
	iso_export void		set(void *data, uint64 m, int e, bool s) const;
	iso_export void		set(void *data, float f)	const;
	iso_export void		set(void *data, double f)	const;
	iso_export float	get(void *data)				const;
	iso_export double	get64(void *data)			const;
};

class CISO_element : public ISO_element {
public:
	CISO_element(tag _id, const ISO_type *_type, size_t _offset, size_t _size) {
		set(_id, _type, _offset, _size);
	}
	template<typename B, typename T> CISO_element(tag _id, T B::*p) {
		set(_id, p);
	}
};

class CISO_type_composite : public ISO_type_composite, public trailing_array<CISO_type_composite, ISO_element> {
public:
	CISO_type_composite(uint32 count = 0, FLAGS flags = NONE) : ISO_type_composite(count, flags) {}
	void*				operator new(size_t size, void *p)				{ return p; }
	void*				operator new(size_t, uint32 n, uint32 defs = 0)	{ return ISO_type::operator new(calc_size(n) + defs); }
	void				operator delete(void *p, uint32, uint32)		{ ISO_type::operator delete(p); }

	constexpr uint32	GetSize()					const	{ return count ? array(count - 1).end() : 0; }
	uint32				GetAlignment()				const	{ return param1 ? (1 << param1) : CalcAlignment(); }
	uint32				CalcAlignment()				const;
	iso_export int		GetIndex(const tag2 &id, int from = 0)	const;
	const ISO_element*	Find(const tag2 &id)		const	{ int i = GetIndex(id); return i >= 0 ? &array(i) : 0; }
	tag2				GetID(const ISO_element *i) const	{ return i->id.get_tag2(flags & ISO_type_composite::CRCIDS); }
	tag2				GetID(int i)				const	{ return array(i).id.get_tag2(flags & ISO_type_composite::CRCIDS); }
	const void*			Default()					const	{ return flags & DEFAULT ? (void*)&(*this)[count] : NULL; }
	const ISO_element*	end()						const	{ return &(*this)[count]; }

	void				Add(const ISO_element &e)						{ array(count++) = e; }
	iso_export uint32				Add(const ISO_type* type, tag id = 0, bool packed = false);
	template<typename T> uint32		Add(tag id = 0, bool packed = false)	{ return Add(ISO_getdef<T>(), id, packed);	}
	iso_export CISO_type_composite	*Duplicate(void *defaults = 0)	const;
};

template<int N> struct TISO_type_fixedcomposite : ISO_type_composite {
	ISO_element			fields[N ? N : 1];
	TISO_type_fixedcomposite(FLAGS flags = NONE, uint32 log2align = 0) : ISO_type_composite(N, flags) {
		SetLog2Align(log2align);
	}
};
template<int N> struct TISO_type_composite : CISO_type_composite {
	ISO_element			fields[N ? N : 1];
	void				Reset()		{ count	= 0; flags = 0; }
};

class CISO_type_user : public ISO_type_user {
public:
	static const FLAGS	DONTKEEP = UNSTORED0;
	CISO_type_user(tag2 id, const ISO_type *subtype, FLAGS flags = NONE) : ISO_type_user(id, subtype, flags) {
		if (!(flags & DONTKEEP))
			user_types.Add(this);
	}
};
class CISO_type_user_comp : public CISO_type_user, trailing_array<CISO_type_user_comp, ISO_element> {
	CISO_type_composite	comp;
public:
	CISO_type_user_comp(tag2 id, uint32 count = 0, FLAGS2 flags = NONE) : CISO_type_user(id, &comp, flags), comp(count, flags.second())	{}
	void*				operator new(size_t size, void *p)				{ return p; }
	void*				operator new(size_t, uint32 n, uint32 defs = 0)	{ return ISO_type::operator new(calc_size(n) + defs); }
	void				operator delete(void *p, uint32, uint32)		{ ISO_type::operator delete(p); }
	void				Add(const ISO_element &e)						{ comp.Add(e); }
	iso_export uint32				Add(const ISO_type* type, tag id = 0, bool packed = false) { return comp.Add(type, id, packed); }
	template<typename T> uint32		Add(tag id = 0, bool packed = false)	{ return comp.Add(ISO_getdef<T>(), id, packed);	}
};

template<int N> struct TISO_type_user_comp : public CISO_type_user {
	ISO_type_composite	comp;
	ISO_element			fields[N ? N : 1];
	TISO_type_user_comp(tag2 id, FLAGS2 flags = NONE, uint32 log2align = 0) : CISO_type_user(id, &comp, flags), comp(N, flags.second()) {
		SetLog2Align(log2align);
	}
	void	SetLog2Align(uint32 log2align) { comp.SetLog2Align(log2align); }
};
template<int N, int B> struct TISO_type_user_enum : public CISO_type_user {
	typedef typename iso::T_uint_type<(B <= 32 ? 4 : 8)>::type	T;
	typedef TISO_enum<T>	ISO_enum;
	TISO_type_enum<T>	i;
	TISO_enum<T>		enums[N];
	uint32				term;
	TISO_type_user_enum(tag2 id, FLAGS2 flags = NONE) : CISO_type_user(id, &i, flags), i(B, flags.second()), term(0) {}
};

class CISO_type_callback : public CISO_type_user {
	template<typename T> struct callbacks {
	#ifdef PLAT_WII
		static void	init(void *p, void *physram)	{ extern ISO_INIT(T); Init((T*)p, physram); }
		static void	deinit(void *p)					{ extern ISO_DEINIT(T); DeInit((T*)p); }
	#else
		static void	init(void *p, void *physram)	{ Init<T>((T*)p, physram); }
		static void	deinit(void *p)					{ DeInit<T>((T*)p); }
	#endif
	};
public:
	void (*init)(void *p, void *physram);
	void (*deinit)(void *p);
	template<typename T> constexpr CISO_type_callback(T *, tag2 id, const ISO_type *subtype, FLAGS flags = NONE)
		: CISO_type_user(id, subtype, flags | INITCALLBACK), init(callbacks<T>::init), deinit(callbacks<T>::deinit)
	{}
};

//-----------------------------------------------------------------------------
//	ISO_ptr
//-----------------------------------------------------------------------------

struct ISO_value {
	enum {	// flag values
		CRCID		= ISO_BIT(16,  0),
		CRCTYPE		= ISO_BIT(16,  1),
		EXTERNAL	= ISO_BIT(16,  2),
		REDIRECT	= ISO_BIT(16,  3),
		HASEXTERNAL	= ISO_BIT(16,  4),
		SPECIFIC	= ISO_BIT(16,  5),
		ISBIGENDIAN	= ISO_BIT(16,  6),
		ROOT		= ISO_BIT(16,  7),
		ALWAYSMERGE	= ISO_BIT(16,  8),
		WEAKREFS	= ISO_BIT(16,  9),
		EXTREF		= ISO_BIT(16, 10),
		TEMP_USER	= ISO_BIT(16, 14),
		PROCESSED	= ISO_BIT(16, 15),
#ifdef ISO_BIGENDIAN
		NATIVE		= ISBIGENDIAN,
#else
		NATIVE		= 0,
#endif
		FROMBIN		= PROCESSED,
	};
	tag1				id;
	ISO_type_ptr		type;
	uint32				user;
	uint16				flags;
	atomic<uint16>		refs;

	friend bool			_IsType(const ISO_value *v, const tag2 &id2) {
		return v && !!v->type && (v->flags & CRCTYPE ? id2.crc32() == (crc32&)v->type : v->type->Type() == ISO_USER && ((ISO_type_user*)v->type.get())->ID() == id2);
	}
	friend bool			_IsType(const ISO_value *v, const ISO_type *t, int crit=0) {
		return ISO_type::Same(!v || v->flags & CRCTYPE ? 0 : (const ISO_type*)v->type, t, crit);
	}


	ISO_value(tag2 _id, const ISO_type *_type) : type(_type), user(0), flags(NATIVE), refs(0)	{ SetID(_id); }
	ISO_value(tag2 _id, crc32 _type) : user(0), flags(NATIVE | CRCTYPE), refs(0)				{ (crc32&)type = _type; SetID(_id); }

	void			SetID(tag2 id2)	{
		if (tag s = id2) {
			id = s;
			flags &= ~CRCID;
		} else {
			id = id2.crc32();
			flags |= CRCID;
		}
	}

	iso_export bool Delete();
	void			addref()							{ ++refs; }
	bool			release()							{ return refs-- == 0 && Delete(); }

	tag2			ID()						const	{ return id.get_tag2(flags & CRCID);}
	const ISO_type*	Type()						const	{ return type;						}
	uint32			Flags()						const	{ return flags;						}
	bool			IsExternal()				const	{ return !!(flags & EXTERNAL);		}
	bool			IsBin()						const	{ return (flags & FROMBIN) && !(flags & REDIRECT); }
	bool			HasCRCType()				const	{ return !!(flags & CRCTYPE);		}

	bool			IsID(const tag2 &id2)		const	{ return ID() == id2; }
	bool			IsType(const tag2 &id2)		const	{ return _IsType(this, id2); }
	bool IsType(const ISO_type *t, int crit=0)	const	{ return _IsType(this, t, crit); }
	template<typename T2>bool IsType(int crit=0)const	{ return _IsType(this, ISO_getdef<T2>(), crit); }
};

template<int B> void *_MakePtr(const ISO_type *type, tag2 id, uint32 size);
extern template void *_MakePtr<32>(const ISO_type *type, tag2 id, uint32 size);
extern template void *_MakePtr<64>(const ISO_type *type, tag2 id, uint32 size);

template<int B> void *_MakePtrExternal(const ISO_type *type, const char *filename, tag2 id);
extern template void *_MakePtrExternal<32>(const ISO_type *type, const char *filename, tag2 id);
extern template void *_MakePtrExternal<64>(const ISO_type *type, const char *filename, tag2 id);

template<int B> void *_MakePtr(const ISO_type *type, tag2 id) {
	return _MakePtr<B>(type, id, type->GetSize());
}

template<int B, typename T> T*	_MakePtr(tag2 id, const T &t) {
	T	*p	= (T*)_MakePtr<B>(ISO_getdef<T>(), id, sizeof(t));
	*p		= t;
	return p;
}

template<typename D, typename S> struct _iso_cast		{ static inline D f(S s)		{ return static_cast<D>(s); } };
template<typename D, typename S> inline D iso_cast(S s)	{ return _iso_cast<D, S>::f(s); }
template<typename D> inline D iso_cast(void *s)			{ return (D)s;					}

template<class T, int B=32> class ISO_ptr {
	template<class T2,int N2> friend class ISO_ptr;
protected:
	typedef typename iso_pointer_type<WRAP<T>, B>::type P;
	typedef pointer32<void>								U;

	struct C : ISO_value, WRAP<T> {
		C(tag2 _id, const ISO_type *_type) : ISO_value(_id, _type) {}
	#ifdef USE_RVALUE_REFS
//		template<typename T2>	C(tag2 _id, const ISO_type *_type, T2 &&_t)		: ISO_value(_id, _type), WRAP<T>(forward<T2>(_t)) {}
		template<typename...PP> C(tag2 _id, const ISO_type *_type, PP&&...pp)	: ISO_value(_id, _type), WRAP<T>(forward<PP>(pp)...) {}
	#else
		template<typename T2> C(tag2 _id, const ISO_type *_type, const T2 &_t)	: ISO_value(_id, _type), WRAP<T>(_t) {}
	#endif
	};

	P	p;

	void		addref()	const		{ if (!!p) static_cast<C*>(p.get())->addref(); }
	bool		release()				{ return !!p && static_cast<C*>(p.get())->release(); }
	void		set(WRAP<T> *_p)		{ release(); p = _p; }
	void		set_ref(WRAP<T> *_p)	{ if (_p) static_cast<C*>(_p)->addref(); set(_p); }
	template<typename T2>	operator T2**()			const;	//hide this
public:
	typedef T	subtype;

	static ISO_value*		Header(T *p)	{ return static_cast<C*>((WRAP<T>*)p); }
	static ISO_ptr<T,B>		Ptr(T *p)		{ P t = (WRAP<T>*)p; return (ISO_ptr<T,B>&)t; }

							ISO_ptr()						: p(0)								{}
	explicit				ISO_ptr(tag2 id)				: p(new(ISO_allocate<B>::allocator()) C(id, ISO_getdef<T>()))		{}
#ifdef USE_RVALUE_REFS
//	template<typename T2> 	ISO_ptr(tag2 id, T2 &&t)		: p(new(ISO_allocate<B>::allocator()) C(id, ISO_getdef<T>(), forward<T2>(t)))		{}
	template<typename...PP>	ISO_ptr(tag2 id, PP&&...pp)		: p(new(ISO_allocate<B>::allocator()) C(id, ISO_getdef<T>(), forward<PP>(pp)...))	{}
#else
	template<typename T2> 	ISO_ptr(tag2 id, const T2 &t)	: p(new(ISO_allocate<B>::allocator()) C(id, ISO_getdef<T>(), t))	{}
#endif
							ISO_ptr(const ISO_ptr &p2)		: p(p2.p)							{ addref(); }
	template<typename T2, int N2>	ISO_ptr(const ISO_ptr<T2,N2> &p2)	: p((WRAP<T>*)(T*)p2)	{ addref(); }
							~ISO_ptr()															{ release();}

	ISO_ptr&				operator=(const ISO_ptr &p2) {
		set_ref(p2.p);
		return *this;
	}
	template<typename T2> ISO_ptr<T2>& operator=(const ISO_ptr<T2> &p2) {
		set_ref((WRAP<T>*)iso_cast<T*>(p2.get()));
		return (ISO_ptr<T2>&)*this;
	}

	ISO_ptr&				Create(tag2 id = tag2()) {
		set(new(ISO_allocate<B>::allocator()) C(id, ISO_getdef<T>()));
		return *this;
	}
	template<typename T2> ISO_ptr&	Create(tag2 id, const T2 &t) {
		set(new(ISO_allocate<B>::allocator()) C(id, ISO_getdef<T>(), t));
		return *this;
	}
	ISO_ptr&				CreateExternal(const char *filename, tag2 id = tag2()) {
		set((WRAP<T>*)_MakePtrExternal<B>(ISO_getdef<T>(), filename, id));
		return *this;
	}

	bool					Clear()							{ bool ret = release(); p = 0; return ret;	}
	ISO_value*				Header()				const	{ return static_cast<C*>(p.get());			}
	const ISO_type*			Type()					const	{ return p && !Header()->HasCRCType() ? Header()->type : 0; }
	tag2					ID()					const	{ return p ? Header()->ID()		: tag2();	}
	const char*				External()				const	{ return p && Header()->IsExternal() ? (const char*)&*p : NULL; }
	uint32					UserInt()				const	{ return p ? Header()->user : 0;			}
	inline_only	void*		User()					const	{ return p ? ((U&)Header()->user).get() : 0;}
	uint32					Flags()					const	{ return p ? Header()->flags	: 0;		}
	bool					IsExternal()			const	{ return p && Header()->IsExternal();		}
	bool					IsBin()					const	{ return p && Header()->IsBin();			}

	bool					HasCRCType()			const	{ return p && Header()->HasCRCType();		}
	bool					HasCRCID()				const	{ return p && Header()->HasCRCID();			}
	bool					IsID(tag2 id)			const	{ return p && Header()->IsID(id);			}
	bool					IsType(tag2 id)			const	{ return p && Header()->IsType(id);			}
	bool					IsType(const ISO_type *t, int crit=0)	const	{ return _IsType(Header(), t, crit); }
	template<typename T2>bool IsType(int crit=0)	const	{ return IsType(ISO_getdef<T2>(), crit);	}

	uint32&					UserInt()						{ return Header()->user;			}
	U&						User()							{ return (U&)Header()->user;		}
	void					SetFlags(uint32 f)		const	{ if (p) Header()->flags |= f;		}
	void					ClearFlags(uint32 f)	const	{ if (p) Header()->flags &= ~f;		}
	bool					TestFlags(uint32 f)		const	{ return p && Header()->flags & f;	}
	void					SetID(tag2 id)			const	{ if (p) Header()->SetID(id);		}

	T*						get()					const	{ return &*p;						}
	operator				const T*()				const	{ return &*p;						}
	template<typename T2>	operator T2*()			const	{ return iso_cast<T2*>(&*p);		}
	typename T_type<T>::ref	operator*()				const	{ return *p;						}
	T*						operator->()			const	{ return &*p;						}
	bool					operator!()				const	{ return !p;						}

	template<typename T2> 	bool operator==(const ISO_ptr<T2> &p2)	const { return (void*)p == (void*)p2.p;	}
	template<typename T2> 	bool operator< (const ISO_ptr<T2> &p2)	const { return (void*)p <  (void*)p2.p;	}
};

template<class T> class ISO_ptr64 : public ISO_ptr<T, 64> {
	typedef ISO_ptr<T, 64>	base;
public:
				ISO_ptr64()						{}
	explicit	ISO_ptr64(tag2 id)				: base(id)		{}
				ISO_ptr64(const ISO_ptr64 &p2)	: base(p2.p)	{}
	template<typename T2, int N2> ISO_ptr64(const ISO_ptr<T2,N2> &p2)	: base(p2) {}
};

template<typename T> using ISO_ptr_machine = ISO_ptr<T, PTRBITS>;

inline void*								GetUser(const void *p)	{ return p ? ((pointer32<void>&)((ISO_value*)p - 1)->user).get() : 0;}
inline pointer32<void>&						GetUser(void *p)		{ return (pointer32<void>&)((ISO_value*)p - 1)->user; }

template<typename T> ISO_value*				GetHeader(T *p)			{ return ISO_ptr<T>::Header(p); }
template<int B, typename T> ISO_ptr<T,B>	GetPtr(T *p)			{ return ISO_ptr<T,B>::Ptr(p); }
template<int B, typename T> ISO_ptr<T,B>	GetPtr(const T *p)		{ return ISO_ptr<T,B>::Ptr((T*)p); }
template<typename T> ISO_ptr<T, PTRBITS>&	GetPtr(T *&&p)			{ return (ISO_ptr<T, PTRBITS>&)p; }
template<typename T> ISO_ptr<T, PTRBITS>&	GetPtr(T *&p)			{ return (ISO_ptr<T, PTRBITS>&)p; }
template<typename T> ISO_ptr<T, PTRBITS>&	GetPtr(const T *&p)		{ return (ISO_ptr<T, PTRBITS>&)p; }

template<int B, typename T> ISO_ptr<T,B>			MakePtr(tag2 id) {
	return ISO_ptr<T,B>(id);
}
template<int B, typename T, int N> ISO_ptr<T[N],B>	MakePtr(tag2 id, const T (&t)[N]) {
	return ISO_ptr<T[N],B>(id, (WRAP<T[N]>&)t);
}
template<int B, int N>	ISO_ptr<const char*, B>		MakePtr(tag2 id, const char (&t)[N]) {
	return ISO_ptr<const char*, B>(id, t);
}
template<int B> inline ISO_ptr<void,B>				MakePtr(const ISO_type *type, tag2 id = tag2()) {
	return GetPtr<B>(_MakePtr<B>(type, id, type->GetSize()));
}
template<int B> inline ISO_ptr<void,B>				MakePtr(const ISO_type *type, tag2 id, uint32 size) {
	return GetPtr<B>(_MakePtr<B>(type, id, size));
}
template<typename T, int B> inline ISO_ptr<T,B>		MakePtr(tag2 id = tag2(), uint32 size = (uint32)sizeof(T)) {
	return GetPtr<B>((T*)_MakePtr<B>(ISO_getdef<T>(), id, size));
}
template<int B> inline ISO_ptr<void,B>				MakePtrExternal(const ISO_type *type, const char *filename, tag2 id = tag2()) {
	return GetPtr<B>(_MakePtrExternal<B>(type, filename, id));
}
template<int B, typename T> inline ISO_ptr<T,B>		MakePtrExternal(const char *filename, tag2 id = tag2()) {
	return GetPtr<B>((T*)_MakePtrExternal<B>(ISO_getdef<T>(), filename, id));
}
template<int B> inline ISO_ptr<void,B>				MakePtrIndirect(ISO_ptr<void,B> p, const ISO_type *type) {
	ISO_ptr<ISO_ptr<void,B>,B>	p2(p.ID(), p);
	p2.Header()->type = type;
	p2.SetFlags(ISO_value::REDIRECT | (p.Flags() & ISO_value::HASEXTERNAL));
	return p2;
}
template<int B, typename T> inline ISO_ptr<T,B>		MakePtrIndirect(ISO_ptr<void,B> p) {
	return MakePtrIndirect(p, ISO_getdef<T>());
}

extern ISO_ptr<void>	iso_nil;

template<typename T> ISO_ptr<T>				MakePtr(tag2 id)										{ return MakePtr<32,T>(id);	}
template<typename T, int N> ISO_ptr<T[N]>	MakePtr(tag2 id, const T (&t)[N])						{ return ISO_ptr<T[N],32>(id, (WRAP<T[N]>&)t); }
template<int N>		ISO_ptr<const char*>	MakePtr(tag2 id, const char (&t)[N])					{ return ISO_ptr<const char*>(id, t); }
template<typename T> inline ISO_ptr<T>		MakePtrExternal(const char *filename, tag2 id = tag2())	{ return MakePtrExternal<32,T>(filename, id); }
template<typename T> inline ISO_ptr<T>		MakePtrIndirect(ISO_ptr<void> p)						{ return MakePtrIndirect<32,T>(p); }
template<typename T> ISO_ptr<T>				MakePtrCheck(tag2 id, const T &t)						{ if (!&t) return iso_nil; return MakePtr<32>(id, t); }

inline ISO_ptr<void> MakePtr(const ISO_type *type, tag2 id = tag2())								{ return MakePtr<32>(type, id); }
inline ISO_ptr<void> MakePtr(const ISO_type *type, tag2 id, uint32 size)							{ return MakePtr<32>(type, id, size); }
inline ISO_ptr<void> MakePtrExternal(const ISO_type *type, const char *filename, tag2 id = tag2())	{ return MakePtrExternal<32>(type, filename, id); }
inline ISO_ptr<void> MakePtrIndirect(ISO_ptr<void> p, const ISO_type *type)							{ return MakePtrIndirect<32>(p, type); }

#ifdef USE_RVALUE_REFS
template<int B, typename T> ISO_ptr<typename T_noconst<typename T_noref<T>::type>::type,B>	MakePtr(tag2 id, T &&t)	{
	return ISO_ptr<typename T_noconst<typename T_noref<T>::type>::type,B>(id, forward<T>(t));
}
template<typename T> ISO_ptr<typename T_noconst<typename T_noref<T>::type>::type>			MakePtr(tag2 id, T &&t)	{
	return ISO_ptr<typename T_noconst<typename T_noref<T>::type>::type,32>(id, forward<T>(t));
}
#else
template<int B, typename T> ISO_ptr<T,B>	MakePtr(tag2 id, const T &t)	{ return ISO_ptr<T,B>(id, t); }
template<typename T> ISO_ptr<T>				MakePtr(tag2 id, const T &t)	{ return MakePtr<32>(id, t); }
#endif

//-----------------------------------------------------------------------------
//	ISO_weak
//-----------------------------------------------------------------------------

struct ISO_weak : public e_treenode<ISO_weak> {
	struct A {
		ISO_value	*value()	{ return (ISO_value*)this - 1; }
	};

	typedef locked_s<e_tree<ISO_weak>, Mutex>	locked_tree;

	static locked_tree	tree() {
		static e_tree<ISO_weak> tree;
		static Mutex			m;
		return locked_tree(tree, m);
	}

	static ISO_weak	*get(void *_p)	{
		A *p = (A*)_p;
		if (p->value()->flags & ISO_value::WEAKREFS) {
			if (ISO_weak *w = find(tree().get(), p)) {
				w->addref();
				return w;
			}
		}
		return new ISO_weak(p);
	}
	A				*p;
	atomic<uint32>	weak_refs;

	operator void*() const	{ return p;		}
	void	addref()		{ weak_refs++;	}
	void	release()		{ if (weak_refs-- == 0) delete this; }

	void	set(A *_p)		{
		if (p) {
			p->value()->flags &= ~ISO_value::WEAKREFS;
			tree().get().remove(this);
		}
		if (p = _p) {
		p->value()->flags |= ISO_value::WEAKREFS;
		tree().get().insert(this);
	}
	}

	static void remove(const ISO_value *v) {
		if (ISO_weak *w = find(tree().get(), (A*)(v + 1))) {
			w->p = 0;
			tree().get().remove(w);
		}
	}

	ISO_weak() : p(0), weak_refs(0) {}

	ISO_weak(A *_p) : p(_p), weak_refs(0) {
		if (p) {
			p->value()->flags |= ISO_value::WEAKREFS;
			tree().get().insert(this);
		}
	}
	~ISO_weak() {
		if (p) {
			p->value()->flags &= ~ISO_value::WEAKREFS;
			tree().get().remove(this);
		}
	}
};

template<class T> class ISO_weakref : ISO_weak {
public:
	ISO_weakref() {}
	ISO_weakref(const ISO_ptr<T> &p) : ISO_weak((A*)(T*)p)	{}
	ISO_weakref	&operator=(const ISO_ptr<T> &p)	{ set((A*)(T*)p); return *this;	}
	operator ISO_ptr<T>()	const				{ return force_cast<ISO_ptr<T> >(iso_void_ptr32(p)); }
	operator T*()			const				{ return (T*)p;				}
};

template<class T> class ISO_weakptr {
	ISO_weak	*p;
public:
	ISO_weakptr() : p(0) {}
	ISO_weakptr(const ISO_ptr<T> &p2)			{ p = ISO_weak::get(p2);	}
	ISO_weakptr(const ISO_weakptr<T> &p2)		{ if (p = p2.p) p->addref(); }
	ISO_weakptr(ISO_weakptr<T> &&p2)			{ p = p2.p; p2.p = 0; }
	~ISO_weakptr()								{ if (p) p->release();		}
	void operator=(const ISO_ptr<T> &p2)		{ if (p) p->release(); p = ISO_weak::get(p2); }
	void operator=(const ISO_weakptr<T> &p2)	{ if (p) p->release(); if (p = p2.p) p->addref(); }

	operator T*()			const	{ return p ? (T*)p->p : 0;			}
	operator ISO_ptr<T>()	const	{ return ISO_ptr<T>::Ptr(*this);	}
};

//-----------------------------------------------------------------------------
//	ISO_array
//-----------------------------------------------------------------------------

template<class T, int N> class ISO_array {
	T	t[N];
public:
	typedef T			subtype;
	typedef T*			iterator;
	typedef const T*	const_iterator;

	operator	T*()								{ return t;		}
	operator	const T*()					const	{ return t;		}

	uint32				size()				const	{ return N;		}
	const_iterator		begin()				const	{ return &t[0];	}
	const_iterator		end()				const	{ return &t[N];	}
	iterator			begin()						{ return &t[0];	}
	iterator			end()						{ return &t[N];	}
};

template<class T, int N1, int N2> struct _iso_cast<ISO_array<T, N1>*, ISO_array<T, N2>*> {
	ISO_COMPILEASSERT(N1 <= N2);
	static ISO_array<T, N1>* f(ISO_array<T, N2>* s)	{ return 0; }
};

//-----------------------------------------------------------------------------
//	ISO_openarray
//-----------------------------------------------------------------------------

struct ISO_openarray_header {
	uint32	max;
	uint32	count:31, bin:1;

//	void operator delete(void*);

	friend uint32	_Count(const ISO_openarray_header *h)				{ return h ? h->count : 0; }
	friend uint8*	_Data(const ISO_openarray_header *h)				{ return h ? (uint8*)(h + 1) : 0; }

	ISO_openarray_header(uint32 _max, uint32 _count)	: max(_max), count(_count), bin(0) {}
	void	SetCount(uint32 _count)						{ count = _count; }
	void	SetBin(bool _bin)							{ bin	= _bin; }
	void*	GetElement(int i, uint32 subsize)	const	{ return (uint8*)(this + 1) + subsize * i; }
	iso_export bool	Remove(uint32 subsize, uint32 align, int i);

	static ISO_openarray_header *Header(void *p) {
		return p ? (ISO_openarray_header*)p - 1 : 0;
	}
	static const ISO_openarray_header *Header(const void *p) {
		return p ? (const ISO_openarray_header*)p - 1 : 0;
	}
};

template<int B> struct ISO_openarray_allocate {
	typedef ISO_allocate<B> A;
	static size_t	alloc_size(uint32 subsize, uint32 align, uint32 n)	{ return iso::align(sizeof(ISO_openarray_header) + subsize * n, align); }
	static void*	adjust(ISO_openarray_header *p, uint32 align)		{ return p && align > sizeof(ISO_openarray_header) ? (uint8*)(p + 1) - align : (uint8*)p; }
	static ISO_openarray_header*	adjust(void *p, uint32 align)		{ return p && align > sizeof(ISO_openarray_header) ? (ISO_openarray_header*)((uint8*)p + align) - 1 : (ISO_openarray_header*)p; }

	static void		Destroy(ISO_openarray_header *h, uint32 align)		{ if (h && !h->bin) A::free(adjust(h, align)); }

	static ISO_openarray_header	*Create(uint32 subsize, uint32 align, uint32 max, uint32 count) {
		return max
			? ISO_VERIFY(
				new(adjust(A::alloc(alloc_size(subsize, align, max), align), align))
				ISO_openarray_header(max, count)
			)
			: 0;
	}
	static ISO_openarray_header	*Resize(ISO_openarray_header *h, uint32 subsize, uint32 align, uint32 max, uint32 count) {
		return ISO_VERIFY(
			new(adjust(A::realloc(adjust(h, align), alloc_size(subsize, align, max), align), align))
				ISO_openarray_header(max, count)
		);
	}
	iso_export static ISO_openarray_header*	Create(ISO_openarray_header *h, uint32 subsize, uint32 align, uint32 count);
	iso_export static ISO_openarray_header*	Create(ISO_openarray_header *h, const ISO_type *type, uint32 count);
	iso_export static ISO_openarray_header*	Resize(ISO_openarray_header *h, uint32 subsize, uint32 align, uint32 count, bool clear = false);
	iso_export static ISO_openarray_header*	Append(ISO_openarray_header *h, uint32 subsize, uint32 align, bool clear);
	iso_export static ISO_openarray_header*	Insert(ISO_openarray_header *h, uint32 subsize, uint32 align, int i, bool clear = false);
};

template<typename T, int B=32> class ISO_openarray {
	typedef typename iso_pointer_type<T, B>::type	P;
	typedef ISO_openarray_allocate<B>				A;
	enum {align = T_alignment<T>::value};
	P		p;

public:
	typedef T			element, &reference, *iterator;
	typedef const T*	const_iterator;

	static ISO_openarray	Ptr(T *p)		{ P t = p; return (ISO_openarray&)t; }

	ISO_openarray()	{}
	ISO_openarray(uint32 count)	: p((T*)_Data(ISO_openarray_allocate<B>::Create((uint32)sizeof(T), align, count, count))) {
		construct_array(p.get(), count);
	}
	template<typename C> ISO_openarray(const C &c, typename T_enable_if<!T_isint<C>::value, void*>::type *x = 0) : p((T*)_Data(ISO_openarray_allocate<B>::Create(sizeof(T), align, num_elements32(c), num_elements32(c)))) {
//	template<typename C> ISO_openarray(const C &c, typename T_enable_if<has_begin<C>::value, void*>::type *x = 0) : p((T*)_Data(ISO_openarray_allocate<B>::Create(sizeof(T), align, num_elements(c), num_elements(c)))) {
		construct_array_it(p.get(), num_elements(c), iso::begin(c));
	}

	uint32			Count() const {
		return _Count(ISO_openarray_header::Header(p));
	}

	ISO_openarray&	Create(uint32 count, bool init = true) {
		T						*t = p;
		ISO_openarray_header	*h = ISO_openarray_header::Header(t);
		destruct_array(t, _Count(h));
		h = ISO_openarray_allocate<B>::Create(h, (uint32)sizeof(T), align, count);
		p = t = (T*)_Data(h);
		if (count && init)
			construct_array(t, count);
		return *this;
	}
	ISO_openarray&	Resize(uint32 count) {
		T						*t = p;
		ISO_openarray_header	*h = ISO_openarray_header::Header(t);
		size_t	count0 = _Count(h);
		if (count != count0) {
			if (count < count0) {
				destruct_array(t + count, count0 - count);
				h->SetCount(count);
			} else {
				if (h = ISO_openarray_allocate<B>::Resize(h, (uint32)sizeof(T), align, count)) {
					p = t = (T*)_Data(h);
					construct_array(t + count0, count - count0);
				}
			}
		}
		return *this;
	}
	void			Remove(int i) {
		if (T *t = p) {
			ISO_openarray_header	*h = ISO_openarray_header::Header(t);
			int n	= _Count(h) - 1;
			for (T *m = t + i, *e = t + n; m < e; m++)
				m[0] = m[1];
			destruct_array(t + n, 1);
			h->SetCount(n);
		}
	}
	void			Remove(T *i) {
		Remove(i - p);
	}

	void			Clear() {
		if (T *t = p) {
			ISO_openarray_header	*h = ISO_openarray_header::Header(t);
			destruct_array(t, _Count(h));
			A::Destroy(h, align);
			p = 0;
		}
	}
	T*				_Append() {
		T						*t = p;
		ISO_openarray_header	*h = ISO_openarray_header::Header(t);
		h = ISO_openarray_allocate<B>::Resize(h, (uint32)sizeof(T), align, _Count(h) + 1);
		p = t = (T*)_Data(h);
		return t + _Count(h) - 1;
	}

	template<typename T2> T& Append(const T2 &t)		{ return *new(_Append()) T(t);	}
	T&						Append()					{ return *new(_Append()) T;		}
	template<typename T2> T& Insert(int i, const T2 &t) {
		T	*p	= _Append(), *b = begin() + i;
		if (p == b) {
			return *new(p) T(t);
		} else {
			new(p) T(p[-1]);
			while (--p != b)
				p[0] = p[-1];
			return p[0] = t;
		}
	}

	inline_only	T			&Index(int i)				{ return p[i]; }
	inline_only	const T		&Index(int i)		const	{ return p[i]; }
	inline_only	T			&operator[](int i)			{ return p[i]; }
	inline_only	const	T	&operator[](int i)	const	{ return p[i]; }
	inline_only	operator	T*()				const	{ return p; }
	int						GetIndex(tag2 id, int from = 0)	const { return _GetIndex(id, *this, from); }
	T						&operator[](tag2 id)		{ int i = _GetIndex(id, *this, 0); return i < 0 ? (T&)iso_nil : (*this)[i]; }

	T&						FindByType(tag2 id);
	T&						FindByType(const ISO_type *t, int crit = 0);
	template<typename U>T&	FindByType(int crit=0)		{ return FindByType(ISO_getdef<U>(), crit); }

	size_t					size()				const	{ return Count();					}
	uint32					size32()			const	{ return Count();					}
	const_iterator			begin()				const	{ return (T*)p.get();				}
	const_iterator			end()				const	{ return (T*)p.get() + size();		}
	iterator				begin()						{ return (T*)p.get();				}
	iterator				end()						{ return (T*)p.get() + size();		}

	const T&				front()				const	{ return ((T*)p.get())[0];			}
	const T&				back()				const	{ return ((T*)p.get())[size() - 1];	}
	T&						front()						{ return ((T*)p.get())[0];			}
	T&						back()						{ return ((T*)p.get())[size() - 1];	}

	template<typename T2> T& push_back(const T2 &t)		{ return *new(_Append()) T(t);	}
};

template<int B> class ISO_openarray<void, B> {
	typedef typename iso_pointer_type<void, B>::type	P;
	typedef ISO_openarray_allocate<B>					A;
	P				p;
	ISO_openarray_header	*Header()		const	{ return ISO_openarray_header::Header(p); }
public:
	ISO_openarray(uint32 subsize, uint32 count)	: p(_Data(ISO_openarray_allocate<B>::Create(subsize, 4, count, count))) {}
	void			Clear(uint32 align)				{ A::Destroy(Header(), align); p = 0; }
	uint32			Count()					const	{ return _Count(Header());	}
	template<typename T> operator T*()		const	{ return (T*)p.get();	}
	void *GetElement(int i, uint32 subsize) const	{ return (uint8*)p.get() + i * subsize; }

	ISO_openarray&	Create(uint32 subsize, uint32 align, uint32 count)	{ p = _Data(ISO_openarray_allocate<B>::Create(Header(), subsize, align, count)); return *this; }
	ISO_openarray&	Resize(uint32 subsize, uint32 align, uint32 count)	{ p = _Data(ISO_openarray_allocate<B>::Resize(Header(), subsize, align, count)); return *this; }
	void*			Append(int subsize, uint32 align)					{ ISO_openarray_header *h = ISO_openarray_allocate<B>::Append(Header(), subsize, align, false); p = _Data(h); return h->GetElement(_Count(h) - 1, subsize); }
	void			Remove(uint32 subsize, uint32 align, int i)			{ return Header()->Remove(subsize, align, i, false); }

	ISO_openarray(const ISO_type *type, uint32 count)	: p(ISO_openarray_allocate<B>::Create(type->GetSize(), type->GetAlignment(), count, count)) {}
	ISO_openarray&	Create(const ISO_type *type, uint32 count)			{ return Create(type->GetSize(), type->GetAlignment(), count); }
	ISO_openarray&	Resize(const ISO_type *type, uint32 count)			{ return Resize(type->GetSize(), type->GetAlignment(), count); }
	void			Remove(const ISO_type *type, int i)					{ return Remove(type->GetSize(), type->GetAlignment(), i); }
	void*			Append(const ISO_type *type)						{ return Append(type->GetSize(), type->GetAlignment()); }
};

template<typename T> using ISO_openarray_machine = ISO_openarray<T, PTRBITS>;

template<typename T, int B> inline size_t num_elements(const ISO_openarray<T,B> &t) {
	return t.Count();
}

template<int B, typename C>	auto MakePtrArray(tag id, const C &c)	{ return ISO_ptr<ISO_openarray<typename container_traits<C>::element>, B>(id, c); }
template<typename C>		auto MakePtrArray(tag id, const C &c)	{ return MakePtrArray<32>(id, c); }

template<typename C, int B>		C	LoadPtrArray(const ISO_ptr<void, B> &p) {
	return *(const ISO_openarray<typename container_traits<C>::element>*)p;
}
template<typename C, int B>		void LoadPtrArray(const ISO_ptr<void, B> &p, C &dest) {
	dest = *(const ISO_openarray<typename container_traits<C>::element>*)p;
}

//-----------------------------------------------------------------------------
//	duplicate/copy/flip/compare blocks of data
//-----------------------------------------------------------------------------

enum {
	DUPF_DEEP			= 1 << 0,
	DUPF_CHECKEXTERNALS	= 1 << 1,
	DUPF_NOINITS		= 1 << 2,
	DUPF_EARLYOUT		= 1 << 3,	// stop checking when found an external
	DUPF_DUPSTRINGS		= 1 << 4,	// duplicate strings, even if not ALLOC
};

iso_export int		_Duplicate(const ISO_type *type, void *data, int flags = 0, void *physical_ram = 0);
iso_export void*	_Duplicate(tag2 id, const ISO_type *type, void *data, int flags = 0, uint16 pflags = 0);

inline ISO_ptr<void> Duplicate(tag2 id, const ISO_type *type, void *data, int flags = 0, uint16 pflags = 0) {
	return GetPtr(_Duplicate(id, type, data, flags, pflags));
}

template<class T> inline T DuplicateData(const T &t, bool deep = false) {
	T	t2 = t;
	_Duplicate(ISO_getdef<T>(), &t2, deep ? DUPF_DEEP : 0);
	return t2;
}

template<class T, int B> inline ISO_ptr<T, B> Duplicate(tag2 id, const ISO_ptr<T, B> &p, bool deep = false) {
	return GetPtr<B>((T*)_Duplicate(id, p.Type(), p,
		(deep ? DUPF_DEEP : 0) | ((p.Flags() & ISO_value::HASEXTERNAL) ? DUPF_CHECKEXTERNALS : 0),
		p.Flags() & (ISO_value::REDIRECT | ISO_value::SPECIFIC | ISO_value::ALWAYSMERGE)
	));
}
template<class T, int B> inline ISO_ptr<T, B> Duplicate(const ISO_ptr<T, B> &p, bool deep = false) {
	return Duplicate(p.ID(), p, deep);
}

iso_export bool		CompareData(const ISO_type *type, const void *data1, const void *data2, int flags = 0);
iso_export void		CopyData(const ISO_type *type, const void *srce, void *dest);
iso_export void		FlipData(const ISO_type *type, const void *srce, void *dest);
iso_export bool		CompareData(ISO_ptr<void> p1, ISO_ptr<void> p2, int flags = 0);
iso_export void		SetBigEndian(ISO_ptr<void> p, bool big);
iso_export bool		CheckHasExternals(const ISO_ptr<void> &p, int flags = DUPF_EARLYOUT, int depth = 64);

template<class T> inline void CopyData(const T *srce, T *dest) {
	CopyData(ISO_getdef<T>(), srce, dest);
}
template<class T> inline void FlipData(const T *srce, void *dest) {
	FlipData(ISO_getdef<T>(), srce, dest);
}
template<class T> inline bool CompareData(const T *data1, const T *data2, int flags = 0) {
	return CompareData(ISO_getdef<T>(), data1, data2, flags);
}

//-----------------------------------------------------------------------------
//	ISO_def type definition creators
//-----------------------------------------------------------------------------
// suffixes for composites:
//	P	param_element
//	T	template parameter
// general suffixes:
//	X	supply name (instead of using stringified typename)
//	F	override default flag field
//	T	supply type (instead of using actual type)

//simple types
#define ISO_TYPEDEF(a,b)				CISO_type_user def_##a(STRINGIFY(a), ISO_getdef<b>())
#define ISO_EXTTYPE(a)					template<> ISO_type *ISO_getdef<a>()
#define _ISO_DEFSAME(a,b)				struct ISO_def<a> { ISO_type *operator&() { return ISO_getdef<b>();} }	// make a's def be same as b's
#define ISO_DEFSAME(a,b)				template<> _ISO_DEFSAME(a,b)
#define ISO_DEFPOD(a,b)					ISO_DEFSAME(a,b[sizeof(a)/sizeof(b)])

//user types
#define _ISO_DEFUSER(a,b,n,F)			struct ISO_def<a> : CISO_type_user { ISO_def() : CISO_type_user(n, ISO_getdef<b>(), F) {} }
#define ISO_DEFUSERXT(a,T,b,n)			_ISO_DEFUSER(a<T>,b,n,NONE)
#define ISO_DEFUSERX(a,b,n)				template<> _ISO_DEFUSER(a,b,n,NONE)
#define ISO_DEFUSERXF(a,b,n,F)			template<> _ISO_DEFUSER(a,b,n,F)
#define ISO_DEFUSER(a,b)				ISO_DEFUSERX(a,b,STRINGIFY(a))
#define ISO_DEFUSERF(a,b,F)				ISO_DEFUSERXF(a,b,STRINGIFY(a),F)

//user types with callbacks
#define ISO_DEFCALLBACKXF(a,b,n,F)		template<> struct ISO_def<a> : CISO_type_callback { ISO_def() : CISO_type_callback((a*)0, n, ISO_getdef<b>(), F) {} }
#define ISO_DEFCALLBACKX(a,b,n)			ISO_DEFCALLBACKXF(a,b,n,NONE)
#define ISO_DEFCALLBACK(a,b)			ISO_DEFCALLBACKX(a,b,STRINGIFY(a))

//user types that are arrays of pod
#define ISO_DEFUSERPODXF(a,b,n,F)		ISO_DEFUSERXF(a,b[sizeof(a)/sizeof(b)],n,F)
#define ISO_DEFUSERPODX(a,b,n)			ISO_DEFUSERPODXF(a,b,n,NONE)
#define ISO_DEFUSERPODF(a,b,F)			ISO_DEFUSERPODXF(a,b,STRINGIFY(a),F)
#define ISO_DEFUSERPOD(a,b)				ISO_DEFUSERPODX(a,b,STRINGIFY(a))
#define ISO_DEFCALLBACKPODXF(a,b,n,F)	ISO_DEFCALLBACKXF(a,b[sizeof(a)/sizeof(b)],n,F)
#define ISO_DEFCALLBACKPODX(a,b,n)		ISO_DEFCALLBACKPODXF(a,b,n,NONE)
#define ISO_DEFCALLBACKPODF(a,b,F)		ISO_DEFCALLBACKPODXF(a,b,STRINGIFY(a),F)
#define ISO_DEFCALLBACKPOD(a,b)			ISO_DEFCALLBACKPODX(a,b,STRINGIFY(a))

//virtual and user-virtual types
#define _ISO_DEFVIRT(a)					struct ISO_def<a> : TISO_virtual<a> {}
#define _ISO_DEFVIRTF(a,F)				struct ISO_def<a> : TISO_virtual<a> { ISO_def() : TISO_virtual<a>(F) {} }
#define ISO_DEFVIRT(a)					template<> _ISO_DEFVIRT(a)
#define ISO_DEFVIRTF(a,F)				template<> _ISO_DEFVIRTF(a,F)
#define ISO_DEFVIRTT(S,T)				_ISO_DEFVIRT(S<T>)
#define ISO_DEFVIRTFT(S,T,F)			_ISO_DEFVIRTF(S<T>,F)
#define ISO_DEFUSERVIRTXT(a,b,n)		template<> struct ISO_def<a> : public CISO_type_user { TISO_virtual<b> v; ISO_def() : CISO_type_user(n, &v)	{} }
#define ISO_DEFUSERVIRTXFT(a,b,n,F)		template<> struct ISO_def<a> : public CISO_type_user { TISO_virtual<b> v; ISO_def() : CISO_type_user(n, &v), v(F)	{} }
#define ISO_DEFUSERVIRTX(a,b)			ISO_DEFUSERVIRTXT(a,a,b)
#define ISO_DEFUSERVIRTXF(a,b,F)		ISO_DEFUSERVIRTXFT(a,a,b,F)
#define ISO_DEFUSERVIRT(a)				ISO_DEFUSERVIRTXT(a,a,STRINGIFY(a))
#define ISO_DEFUSERVIRTF(a,F)			ISO_DEFUSERVIRTXFT(a,a,STRINGIFY(a),F)

//header for composite def
#define _ISO_DEFCOMP(S,N,F)				struct ISO_def<S> : public TISO_type_fixedcomposite<N> { typedef S _S, _T; ISO_def() : TISO_type_fixedcomposite<N>(F,T_log2alignment<S>::value)
#define ISO_DEFCOMPF(S,N,F)				template<> _ISO_DEFCOMP(S,N,F)
#define ISO_DEFCOMP(S,N)				ISO_DEFCOMPF(S,N,NONE)
//...of templated type
#define ISO_DEFCOMPTF(S,T,N,F)			_ISO_DEFCOMP(S<T>,N,F)
#define ISO_DEFCOMPT(S,T,N)				ISO_DEFCOMPTF(S,T,N,NONE)
//...with param element
#define _ISO_DEFCOMPPF(S,P,N,F)			struct ISO_def<param_element<S,P> > : public TISO_type_fixedcomposite<N> { typedef S _S; typedef param_element<S,P> _T; ISO_def() : TISO_type_fixedcomposite<N>(F,T_log2alignment<_S>::value)
#define ISO_DEFCOMPPF(S,P,N,F)			template<> _ISO_DEFCOMPPF(S,P,N,F)
#define ISO_DEFCOMPP(S,P,N)				ISO_DEFCOMPPF(S,P,N,NONE)
//...of templated type
#define ISO_DEFCOMPTPF(S,T,P,N,F)		_ISO_DEFCOMPPF(S<T>,P,N,F)
#define ISO_DEFCOMPTP(S,T,P,N)			ISO_DEFCOMPPTF(S,T,P,N,NONE)

//header for user-composite def
#define _ISO_DEFUSERCOMP(S,N,n,F)		struct ISO_def<S> : public TISO_type_user_comp<N> { typedef S _S, _T; ISO_def() : TISO_type_user_comp<N>(n,F,T_log2alignment<_S>::value)
#define ISO_DEFUSERCOMPXF(S,N,n,F)		template<> _ISO_DEFUSERCOMP(S,N,n,F)
#define ISO_DEFUSERCOMPX(S,N,n)			ISO_DEFUSERCOMPXF(S,N,n,NONE)
#define ISO_DEFUSERCOMPF(S,N,F)			ISO_DEFUSERCOMPXF(S,N,#S,F)
#define ISO_DEFUSERCOMP(S,N)			ISO_DEFUSERCOMPXF(S,N,#S,NONE)
//...of templated type
#define ISO_DEFUSERCOMPTXF(S,T,N,n,F)	_ISO_DEFUSERCOMP(S<T>,N,n,F)
#define ISO_DEFUSERCOMPTX(S,T,N,n)		ISO_DEFUSERCOMPTXF(S,T,N,n,NONE)
#define ISO_DEFUSERCOMPTF(S,T,N,F)		ISO_DEFUSERCOMPTXF(S,T,N,#S,F)
#define ISO_DEFUSERCOMPT(S,T,N)			ISO_DEFUSERCOMPTXF(S,T,N,#S,NONE)

//if you need parentheses around S
#define _ISO_DEFUSERCOMP2(S,N,n,F)		struct ISO_def<NO_PARENTHESES S> : public TISO_type_user_comp<N> { typedef NO_PARENTHESES S _S, _T; ISO_def() : TISO_type_user_comp<N>(n,F,T_log2alignment<_S>::value)

//param element
#define _ISO_DEFUSERCOMPP(S,P,N,n,F)	struct ISO_def<param_element<S&,P> > : public TISO_type_user_comp<N> { typedef S _S; typedef param_element<S&,P> _T; ISO_def() : TISO_type_user_comp<N>(n,F,T_log2alignment<_S>::value)
#define ISO_DEFUSERCOMPPXF(S,P,N,n,F)	template<> _ISO_DEFUSERCOMPP(S,P,N,n,F)
#define ISO_DEFUSERCOMPPX(S,P,N,n)		ISO_DEFUSERCOMPPXF(S,P,N,n,NONE)
#define ISO_DEFUSERCOMPPF(S,P,N,F)		ISO_DEFUSERCOMPPXF(S,P,N,#S,F)
#define ISO_DEFUSERCOMPP(S,P,N)			ISO_DEFUSERCOMPPXF(S,P,N,#S,NONE)
//...of templated type
#define ISO_DEFUSERCOMPTPXF(S,T,P,N,n,F) _ISO_DEFUSERCOMPP(S<T>,P,N,n,F)
#define ISO_DEFUSERCOMPTPX(S,T,P,N,n)	ISO_DEFUSERCOMPTPXF(S,T,P,N,n,NONE)
#define ISO_DEFUSERCOMPTPF(S,T,P,N,F)	ISO_DEFUSERCOMPTPXF(S,T,P,N,#S,F)
#define ISO_DEFUSERCOMPTP(S,T,P,N)		ISO_DEFUSERCOMPTPXF(S,T,P,N,#S,NONE)

//set up composite field
#define ISO_SETBASE(i,b)				fields[i].set(tag(), ISO_getdef<b>(), size_t(static_cast<b*>((_S*)1)) - 1, sizeof(b))

#define ISO_SETFIELDXT1(i,e,n,t)		fields[i].set(tag(n), ISO_getdef<t>(), iso_offset(_S,e), sizeof(((_S*)1)->e))
#define ISO_SETFIELDT1(i,e,t)			ISO_SETFIELDXT1(i,e,STRINGIFY2(e),t)
#define ISO_SETFIELDX1(i,e,n)			fields[i].set(tag(n), ISO_getdef(((_S*)1)->e), iso_offset(_S,e), sizeof(((_S*)1)->e))

#define ISO_SETFIELDXT(i,e,n,t)			fields[i].set(tag(n), ISO_getdef<t>(), iso_offset(_S,e), sizeof(t))
#define ISO_SETFIELDT(i,e,t)			ISO_SETFIELDXT(i,e,STRINGIFY2(e),t)
#if 1//def PLAT_PC
#define ISO_SETFIELDX(i,e,n)			T_get_class<ISO_element_setter>(&_S::e)->set<_T, &_S::e>(tag(n), fields[i])
#else
#define ISO_SETFIELDX(i,e,n)			ISO_SETFIELDX1(i, e, n)
#endif
#define ISO_SETFIELD(i,e)				ISO_SETFIELDX(i,e,STRINGIFY2(e))

#define ISO_SETACCESSORX(i,e,n)			fields[i].set(tag(n), ISO_accessor(&_S::e), 0, 0)
#define ISO_SETACCESSOR(i,e)			ISO_SETACCESSORX(i,e,STRINGIFY2(e))
#define ISO_SETACCESSORPTRX(i,e1,e2,n)	fields[i].set(tag(n), ISO_accessor(&_T::e2), iso_offset(_S,e1), sizeof(((_S*)1)->e1))
#define ISO_SETACCESSORPTR(i,e1,e2)		ISO_SETACCESSORPTRX(i,e1,e2,STRINGIFY2(e2))

//set up 2 to 8 composite fields
#define ISO_SETFIELDS2(i,E1,E2)						ISO_SETFIELD(i,E1),ISO_SETFIELD(i+1,E2)
#define ISO_SETFIELDS3(i,E1,E2,E3)					ISO_SETFIELDS2(i,E1,E2),ISO_SETFIELD(i+2,E3)
#define ISO_SETFIELDS4(i,E1,E2,E3,E4)				ISO_SETFIELDS2(i,E1,E2),ISO_SETFIELDS2(i+2,E3,E4)
#define ISO_SETFIELDS5(i,E1,E2,E3,E4,E5)			ISO_SETFIELDS4(i,E1,E2,E3,E4),ISO_SETFIELD(i+4,E5)
#define ISO_SETFIELDS6(i,E1,E2,E3,E4,E5,E6)			ISO_SETFIELDS4(i,E1,E2,E3,E4),ISO_SETFIELDS2(i+4,E5,E6)
#define ISO_SETFIELDS7(i,E1,E2,E3,E4,E5,E6,E7)		ISO_SETFIELDS4(i,E1,E2,E3,E4),ISO_SETFIELDS3(i+4,E5,E6,E7)
#define ISO_SETFIELDS8(i,E1,E2,E3,E4,E5,E6,E7,E8)	ISO_SETFIELDS4(i,E1,E2,E3,E4),ISO_SETFIELDS4(i+4,E5,E6,E7,E8)

//entire composite def for 0 to 8 composite fields
#define ISO_DEFCOMP0(S)								ISO_DEFCOMP(S,0) {} }
#define ISO_DEFCOMP1(S,E1)							ISO_DEFCOMP(S,1) {ISO_SETFIELD(0,E1);} }
#define ISO_DEFCOMP2(S,E1,E2)						ISO_DEFCOMP(S,2) {ISO_SETFIELDS2(0,E1,E2);} }
#define ISO_DEFCOMP3(S,E1,E2,E3)					ISO_DEFCOMP(S,3) {ISO_SETFIELDS3(0,E1,E2,E3);} }
#define ISO_DEFCOMP4(S,E1,E2,E3,E4)					ISO_DEFCOMP(S,4) {ISO_SETFIELDS4(0,E1,E2,E3,E4);} }
#define ISO_DEFCOMP5(S,E1,E2,E3,E4,E5)				ISO_DEFCOMP(S,5) {ISO_SETFIELDS5(0,E1,E2,E3,E4,E5);} }
#define ISO_DEFCOMP6(S,E1,E2,E3,E4,E5,E6)			ISO_DEFCOMP(S,6) {ISO_SETFIELDS6(0,E1,E2,E3,E4,E5,E6);} }
#define ISO_DEFCOMP7(S,E1,E2,E3,E4,E5,E6,E7)		ISO_DEFCOMP(S,7) {ISO_SETFIELDS7(0,E1,E2,E3,E4,E5,E6,E7);} }
#define ISO_DEFCOMP8(S,E1,E2,E3,E4,E5,E6,E7,E8)		ISO_DEFCOMP(S,8) {ISO_SETFIELDS8(0,E1,E2,E3,E4,E5,E6,E7,E8);} }

//entire user-composite def for up 2 to 8 composite fields
#define ISO_DEFUSERCOMP1(S,E1)						ISO_DEFUSERCOMP(S,1) {ISO_SETFIELD(0,E1);} }
#define ISO_DEFUSERCOMP2(S,E1,E2)					ISO_DEFUSERCOMP(S,2) {ISO_SETFIELDS2(0,E1,E2);} }
#define ISO_DEFUSERCOMP3(S,E1,E2,E3)				ISO_DEFUSERCOMP(S,3) {ISO_SETFIELDS3(0,E1,E2,E3);} }
#define ISO_DEFUSERCOMP4(S,E1,E2,E3,E4)				ISO_DEFUSERCOMP(S,4) {ISO_SETFIELDS4(0,E1,E2,E3,E4);} }
#define ISO_DEFUSERCOMP5(S,E1,E2,E3,E4,E5)			ISO_DEFUSERCOMP(S,5) {ISO_SETFIELDS5(0,E1,E2,E3,E4,E5);} }
#define ISO_DEFUSERCOMP6(S,E1,E2,E3,E4,E5,E6)		ISO_DEFUSERCOMP(S,6) {ISO_SETFIELDS6(0,E1,E2,E3,E4,E5,E6);} }
#define ISO_DEFUSERCOMP7(S,E1,E2,E3,E4,E5,E6,E7)	ISO_DEFUSERCOMP(S,7) {ISO_SETFIELDS7(0,E1,E2,E3,E4,E5,E6,E7);} }
#define ISO_DEFUSERCOMP8(S,E1,E2,E3,E4,E5,E6,E7,E8)	ISO_DEFUSERCOMP(S,8) {ISO_SETFIELDS8(0,E1,E2,E3,E4,E5,E6,E7,E8);} }

//entire user-composite def for up 2 to 8 composite fields
#define ISO_DEFUSERCOMPX1(S,n, E1)					ISO_DEFUSERCOMPX(S,1,n) {ISO_SETFIELD(0,E1);} }
#define ISO_DEFUSERCOMPX2(S,n, E1,E2)				ISO_DEFUSERCOMPX(S,2,n) {ISO_SETFIELDS2(0,E1,E2);} }
#define ISO_DEFUSERCOMPX3(S,n, E1,E2,E3)			ISO_DEFUSERCOMPX(S,3,n) {ISO_SETFIELDS3(0,E1,E2,E3);} }
#define ISO_DEFUSERCOMPX4(S,n, E1,E2,E3,E4)			ISO_DEFUSERCOMPX(S,4,n) {ISO_SETFIELDS4(0,E1,E2,E3,E4);} }
#define ISO_DEFUSERCOMPX5(S,n, E1,E2,E3,E4,E5)		ISO_DEFUSERCOMPX(S,5,n) {ISO_SETFIELDS5(0,E1,E2,E3,E4,E5);} }
#define ISO_DEFUSERCOMPX6(S,n, E1,E2,E3,E4,E5,E6)	ISO_DEFUSERCOMPX(S,6,n) {ISO_SETFIELDS6(0,E1,E2,E3,E4,E5,E6);} }
#define ISO_DEFUSERCOMPX7(S,n, E1,E2,E3,E4,E5,E6,E7)ISO_DEFUSERCOMPX(S,7,n) {ISO_SETFIELDS7(0,E1,E2,E3,E4,E5,E6,E7);} }
#define ISO_DEFUSERCOMPX8(S,n, E1,E2,E3,E4,E5,E6,E7,E8)	ISO_DEFUSERCOMPX(S,8,n) {ISO_SETFIELDS8(0,E1,E2,E3,E4,E5,E6,E7,E8);} }

//header for user-enum def
#define ISO_DEFUSERENUMXF(S,N,n,B,F)				template<> struct ISO_def<S> : public TISO_type_user_enum<N,B> { typedef S _S; ISO_def() : TISO_type_user_enum<N,B>(n,F)
#define ISO_DEFUSERENUMX(S,N,n)						ISO_DEFUSERENUMXF(S,N,n,32,NONE)
#define ISO_DEFUSERENUMF(S,N,B,F)					ISO_DEFUSERENUMXF(S,N,#S,B,F)
#define ISO_DEFUSERENUM(S,N)						ISO_DEFUSERENUMXF(S,N,#S,32,NONE)
#define ISO_SETENUMX(i,e,n)							enums[i].set(n, e)

//set up 1 to 8 enum values
#define ISO_SETENUM(i,e)							ISO_SETENUMX(i,e,STRINGIFY2(e))
#define ISO_SETENUMS2(i,E1,E2)						ISO_SETENUM(i,E1),ISO_SETENUM(i+1,E2)
#define ISO_SETENUMS3(i,E1,E2,E3)					ISO_SETENUMS2(i,E1,E2),ISO_SETENUM(i+2,E3)
#define ISO_SETENUMS4(i,E1,E2,E3,E4)				ISO_SETENUMS2(i,E1,E2),ISO_SETENUMS2(i+2,E3,E4)
#define ISO_SETENUMS5(i,E1,E2,E3,E4,E5)				ISO_SETENUMS4(i,E1,E2,E3,E4),ISO_SETENUM(i+4,E5)
#define ISO_SETENUMS6(i,E1,E2,E3,E4,E5,E6)			ISO_SETENUMS4(i,E1,E2,E3,E4),ISO_SETENUMS2(i+4,E5,E6)
#define ISO_SETENUMS7(i,E1,E2,E3,E4,E5,E6,E7)		ISO_SETENUMS4(i,E1,E2,E3,E4),ISO_SETENUMS3(i+4,E5,E6,E7)
#define ISO_SETENUMS8(i,E1,E2,E3,E4,E5,E6,E7,E8)	ISO_SETENUMS4(i,E1,E2,E3,E4),ISO_SETENUMS4(i+4,E5,E6,E7,E8)

//set up 1 to 8 enum values with _S:: qualifier
#define ISO_SETENUMQ(i,E)							enums[i].set(#E, _S::E)
#define ISO_SETENUMSQ2(i,E1,E2)						ISO_SETENUMQ(i,E1),ISO_SETENUMQ(i+1,E2)
#define ISO_SETENUMSQ3(i,E1,E2,E3)					ISO_SETENUMSQ2(i,E1, E2), ISO_SETENUMQ(i+2,E3)
#define ISO_SETENUMSQ4(i,E1,E2,E3,E4)				ISO_SETENUMSQ2(i,E1, E2), ISO_SETENUMSQ2(i+2,E3,E4)
#define ISO_SETENUMSQ5(i,E1,E2,E3,E4,E5)			ISO_SETENUMSQ4(i,E1, E2, E3, E4), ISO_SETENUMQ(i+4,E5)
#define ISO_SETENUMSQ6(i,E1,E2,E3,E4,E5,E6)			ISO_SETENUMSQ4(i,E1, E2, E3, E4), ISO_SETENUMSQ2(i+4,E5,E6)
#define ISO_SETENUMSQ7(i,E1,E2,E3,E4,E5,E6,E7)		ISO_SETENUMSQ4(i,E1, E2, E3, E4), ISO_SETENUMSQ3(i+4,E5,E6,E7)
#define ISO_SETENUMSQ8(i,E1,E2,E3,E4,E5,E6,E7,E8)	ISO_SETENUMSQ4(i,E1, E2, E3, E4), ISO_SETENUMSQ4(i+4,E5,E6,E7,E8)

//-----------------------------------------------------------------------------

template<typename T>		struct ISO_def;
template<typename T>		struct ISO_def<const T>						{ ISO_type *operator&() { return ISO_getdef<T>();} };
template<typename T>		struct ISO_def<const T*>					{ ISO_type *operator&() { return ISO_getdef<T*>();} };
template<typename T>		struct ISO_def<constructable<T> >			{ ISO_type *operator&() { return ISO_getdef<T>();} };
template<typename T>		struct ISO_def<packed<T> >	: ISO_def<T>	{ ISO_def() { this->type |= TYPE_PACKED;} };
template<typename T>		struct ISO_def<packed<constructable<T> > >	{ ISO_type *operator&() { return ISO_getdef<packed<T> >();} };
template<typename T>		struct ISO_def<packed<const T> >			{ ISO_type *operator&() { return ISO_getdef<packed<T> >();} };

template<typename T>		struct ISO_def<WRAP<T> > : ISO_type_composite {
	CISO_element	element;
	ISO_def() : ISO_type_composite(1), element(0, ISO_getdef<T>(), 0, sizeof(T)) {}
};

template<typename T, int B>	struct ISO_def<ISO_ptr<T,B> >		: _ISO_type_reference { ISO_def() : _ISO_type_reference(B == 64 ? ISO_REFERENCE64 : ISO_REFERENCE, 0) { subtype = ISO_getdef<T>(); } };
template<typename T, int N>	struct ISO_def<ISO_array<T, N> >	: ISO_type_array { ISO_def() : ISO_type_array(ISO_getdef<T>(), N, (uint32)sizeof(T)) {} };
template<typename T, int N>	struct ISO_def<T[N]>				: ISO_def<ISO_array<T,N> > {};
template<typename T, int N>	struct ISO_def<fixed_array<T, N> >	: ISO_def<ISO_array<T,N> > {};

template<typename T, int B>	struct ISO_def<ISO_openarray<T,B> > : ISO_type_openarrayB {
	ISO_def() : ISO_type_openarrayB(B, ISO_getdef<T>(), sizeof(T), T_log2alignment<T>::value)	{}
};

template<typename A, typename B> struct ISO_def<pair<A, B> > : ISO_type_composite {
	CISO_element	a, b;
	ISO_def() : ISO_type_composite(2),
		a("a", ISO_getdef<A>(), 0, sizeof(A)),
		b("b", ISO_getdef<B>(), size_t(&((pair<A,B>*)1)->b) - 1, sizeof(B))
	{}
};

template<typename T> struct ISO_def<interval<T> > : ISO_type_composite {
	CISO_element	a, b;
	ISO_def() : ISO_type_composite(2),
		a("min", &interval<T>::a),
		b("max", &interval<T>::b)
	{}
};

//-----------------------------------------------------------------------------

template<typename T> ISO_type	*ISO_getdef() {
	static manual_static<ISO_def<T> > def;
	return &def.get();
}
template<> inline ISO_type		*ISO_getdef<void>()	{
	return 0;
}
/*
template<> inline ISO_type		*ISO_getdef<int8>	()	{ return ISO_type_int::make<int8>	();	}
template<> inline ISO_type		*ISO_getdef<int16>	()	{ return ISO_type_int::make<int16>	();	}
template<> inline ISO_type		*ISO_getdef<int32>	()	{ return ISO_type_int::make<int32>	();	}
template<> inline ISO_type		*ISO_getdef<int64>	()	{ return ISO_type_int::make<int64>	();	}
template<> inline ISO_type		*ISO_getdef<uint8>	()	{ return ISO_type_int::make<uint8>	();	}
template<> inline ISO_type		*ISO_getdef<uint16>	()	{ return ISO_type_int::make<uint16>	();	}
template<> inline ISO_type		*ISO_getdef<uint32>	()	{ return ISO_type_int::make<uint32>	();	}
template<> inline ISO_type		*ISO_getdef<uint64>	()	{ return ISO_type_int::make<uint64>	();	}
template<> inline ISO_type		*ISO_getdef<uint128>()	{ return ISO_type_int::make<128,0, ISO_type_int::NONE>();	}
*/
template<> struct ISO_def<int8>		: ISO_type_int { ISO_def() : ISO_type_int(8,  0, ISO_type_int::SIGN) {} };
template<> struct ISO_def<int16>	: ISO_type_int { ISO_def() : ISO_type_int(16, 0, ISO_type_int::SIGN) {} };
template<> struct ISO_def<int32>	: ISO_type_int { ISO_def() : ISO_type_int(32, 0, ISO_type_int::SIGN) {} };
template<> struct ISO_def<int64>	: ISO_type_int { ISO_def() : ISO_type_int(64, 0, ISO_type_int::SIGN) {} };
template<> struct ISO_def<uint8>	: ISO_type_int { ISO_def() : ISO_type_int(8,  0) {} };
template<> struct ISO_def<uint16>	: ISO_type_int { ISO_def() : ISO_type_int(16, 0) {} };
template<> struct ISO_def<uint32>	: ISO_type_int { ISO_def() : ISO_type_int(32, 0) {} };
template<> struct ISO_def<uint64>	: ISO_type_int { ISO_def() : ISO_type_int(64, 0) {} };
template<> struct ISO_def<uint128>	: ISO_type_int { ISO_def() : ISO_type_int(128,0) {}	};
template<> struct ISO_def<char>		: ISO_type_int { ISO_def() : ISO_type_int( 8, 0, CHR) {} };
template<> struct ISO_def<char16>	: ISO_type_int { ISO_def() : ISO_type_int(16, 0, CHR) {} };
template<> struct ISO_def<char32>	: ISO_type_int { ISO_def() : ISO_type_int(32, 0, CHR) {} };

template<typename T> struct ISO_def<baseint<16,T> >		: CISO_type_int { ISO_def() : CISO_type_int((uint32)sizeof(T) * 8, 0, HEX) {} };
template<typename T> struct ISO_def<rawint<T> >			: CISO_type_int { ISO_def() : CISO_type_int((uint32)sizeof(T) * 8, 0, HEX | NOSWAP) {} };
template<typename E, typename S> struct ISO_def<flags<E, S> > { ISO_type *operator&() { return ISO_getdef<S>();} };

//template<> inline ISO_type		*ISO_getdef<float>	()	{ return ISO_type_float::make<float>();		}
//template<> inline ISO_type		*ISO_getdef<double>	()	{ return ISO_type_float::make<double>();	}
template<> struct ISO_def<float>	: ISO_type_float { ISO_def() : ISO_type_float(32, 8, ISO_type_float::SIGN) {} };
template<> struct ISO_def<double>	: ISO_type_float { ISO_def() : ISO_type_float(64, 11, ISO_type_float::SIGN) {} };

ISO_DEFUSERENUMXF(bool8,2,"bool",8,NONE) {
	ISO_SETENUMX(0, 0, "false");
	ISO_SETENUMX(1, 1, "true");
}};

template<> struct ISO_def<char*>		: TISO_type_string<char*,		char							>	{};
template<> struct ISO_def<string>		: TISO_type_string<string,		char,	ISO_type_string::MALLOC	>	{};
template<> struct ISO_def<char16*>		: TISO_type_string<char16*,		char16							>	{};
template<> struct ISO_def<string16>		: TISO_type_string<string16,	char16,	ISO_type_string::MALLOC	>	{};
template<> struct ISO_def<iso_string8>	: TISO_type_string<iso_string8,	char,	ISO_type_string::ALLOC	>	{};
template<typename B> struct ISO_def<string_base<B> > : TISO_type_string<string_base<B>, typename string_base<B>::element>{};

template<int N, typename C> struct ISO_def<fixed_string<N,C> > : ISO_type_array {
	ISO_def() : ISO_type_array(ISO_getdef<C>(), N, (uint32)sizeof(C)) {}
};

template<typename T> struct unescaped : T {};
template<typename T> struct ISO_def<unescaped<T> >	: ISO_def<T> { ISO_def() : ISO_def<T>() { this->flags |= this->UNESCAPED; } };

ISO_DEFSAME(tag, const char*);

template<> struct ISO_def<GUID> : public CISO_type_user_comp {
	CISO_element	Data1, Data2, Data3, Data4;
	ISO_def() : CISO_type_user_comp("GUID", 4),
		Data1(0, ISO_getdef<xint32>(), 0, 4),
		Data2(0, ISO_getdef<xint16>(), 4, 2),
		Data3(0, ISO_getdef<xint16>(), 6, 2),
		Data4(0, ISO_getdef<xint8[8]>(), 8, 8)
	{}
};

ISO_DEFCALLBACK(crc32, iso_string8);

typedef ISO_openarray<ISO_ptr<void> > anything;

//-----------------------------------------------------------------------------
//	type_list ISO_def type definition creators
//-----------------------------------------------------------------------------

template<typename T, int O> struct TL_ISO_elements {
	typedef typename TL_index<0,T>::type	A;
	typedef	typename TL_skip<1, T>::type	B;
	struct temp {
		char	pad[O];
		A		a;
	};
	CISO_element								a;
	TL_ISO_elements<B, (uint32)sizeof(temp)>	b;
	TL_ISO_elements() : a(0, ISO_getdef<A>(), size_t(&((temp*)1)->a) - 1, sizeof(A)){}
};
template<typename T> struct TL_ISO_elements<T, 0> {
	typedef typename TL_index<0,T>::type	A;
	typedef	typename TL_skip<1, T>::type	B;
	CISO_element							a;
	TL_ISO_elements<B, (uint32)sizeof(A)>	b;
	TL_ISO_elements() : a(0, ISO_getdef<A>(), 0, sizeof(A)){}
};
template<int O> struct TL_ISO_elements<type_list<>, O> {};

template<typename T> struct ISO_def<tuple<T> > : CISO_type_composite, TL_ISO_elements<T, 0> {
	ISO_def() : CISO_type_composite(T::count)	{}
};

template<typename T> ISO_type *TL_ISO_getdef() { return ISO_getdef<tuple<T> >(); }

//-----------------------------------------------------------------------------
//	class ISO_browser
//-----------------------------------------------------------------------------

class ISO_browser2;

class ISO_browser {
protected:
	const ISO_type		*type;
	void				*data;

	template<typename T> struct	s_setget;
	template<typename T> struct as;

	static const ISO_browser&	BadBrowser() { static ISO_browser b(0, 0); return b; }
	static iso_export void		ClearTempFlags(ISO_browser b);
	static iso_export void		Apply(ISO_browser2 b, const ISO_type *type, void f(void*));
	static iso_export void		Apply(ISO_browser2 b, const ISO_type *type, void f(void*, void*), void *param);

public:
	ISO_browser()															: type(0), data(0)						{}
	ISO_browser(const ISO_type *_type, void *_data)							: type(_data ? _type : 0), data(_data)	{}
	ISO_browser(const ISO_browser &b)										: type(b.type), data(b.data)			{}
	template<typename T, int B> explicit ISO_browser(const ISO_ptr<T, B> &t): type(t.Type()), data(t)				{}
	template<typename T> explicit ISO_browser(const ISO_openarray<T> &t)	: type(ISO_getdef(t)), data((void*)&t)	{}

	ISO_TYPE						Type()									const	{ return type ? type->Type() : ISO_UNKNOWN;	}
	bool							Is(const ISO_type *t, int crit=0)		const	{ return ISO_type::Same(type, t, crit);		}
	template<typename T>			bool Is(int crit=0)						const	{ return Is(ISO_getdef<T>(), crit);	}
	bool							Is(tag2 id)								const	{ return type && type->Is(id);		}
	const char*						External()								const	{ const ISO_type *t = type->SkipUser(); return t && t->Type() == ISO_REFERENCE ? GetPtr(t->ReadPtr(data)).External() : 0;	}
	bool							HasCRCType()							const	{ const ISO_type *t = type->SkipUser(); return t && t->Type() == ISO_REFERENCE && GetPtr(t->ReadPtr(data)).HasCRCType();	}

	inline	const ISO_browser2		operator[](tag2 id)						const;
	inline	const ISO_browser2		operator[](int i)						const;
	inline	const ISO_browser2		Index(int i)							const;
	inline bool						IsVirtPtr()								const;

	iso_export const ISO_browser2	operator*()								const;
	iso_export uint32				Count()									const;
	iso_export int					GetIndex(tag2 id, int from = 0)			const;
	iso_export tag2					GetName()								const;
	iso_export tag2					GetName(int i)							const;
	iso_export uint32				GetSize()								const;
	iso_export bool					Resize(uint32 n)						const;
	iso_export const ISO_browser	Append()								const;
	iso_export bool					Remove(int i)							const;
	iso_export const ISO_browser	Insert(int i)							const;
	iso_export const ISO_browser2	FindByType(tag2 id)						const;
	iso_export const ISO_browser2	FindByType(const ISO_type *t, int crit=0) const;
	template<typename T>const ISO_browser2 FindByType(int crit=0)			const;

	ISO_browser						SkipUser()								const	{ return ISO_browser(type->SkipUser(), data); }
	uint32							GetAlignment()							const	{ return type->GetAlignment(); }
	const ISO_type*					GetTypeDef()							const	{ return type; }

	template<typename T>			as<T> As()								const	{ return as<T>(*this); }
	bool							Check(tag2 id)							const	{ return GetIndex(id) >= 0; }

									operator void*()						const	{ return data; }
#if 0
	template<typename T>			operator T*()							const	{ ISO_ASSERT(type->SameAs<T>()); return (T*)data; }
	template<typename T>			operator ISO_ptr<T>&()					const	{ ISO_ASSERT(type->SameAs<T>()); return ISO_ptr<T>::Ptr((T*)data); }
#else										 
	template<typename T>			operator T*()							const	{
		return (T*)(type && type->Type() == ISO_REFERENCE ? type->ReadPtr(data) : data);
	}
	template<typename T>			operator T**()							const;//	{ ISO_COMPILEASSERT(0); return (T**)0; }
	template<typename T, int B>		operator ISO_ptr<T, B>()				const	{ return ISO_ptr<T, B>::Ptr((T*)data); }
#endif
	template<typename T> T			get()									const	{ return s_setget<T>::get(data, type, T()); }
	getter<const ISO_browser>		get()									const	{ return *this; }
	template<typename T> T			Get()									const	{ return s_setget<T>::get(data, type, T()); }
	getter<const ISO_browser>		Get()									const	{ return *this; }
	template<typename T> T			Get(const T &t)							const	{ return s_setget<T>::get(data, type, t); }
	int								GetInt(int i = 0)						const	{ return Get(i); }
	float							GetFloat(float f = 0)					const	{ return Get(f); }
	double							GetDouble(double f = 0)					const	{ return Get(f); }
	const char*						GetString(const char *s = 0)			const	{ return Get(s); }

	template<typename T, int B=32> ISO_ptr<T,B> ReadPtr()					const	{ return ISO_ptr<T,B>::Ptr((T*)type->SkipUser()->ReadPtr(data)); }
	inline const ISO_browser2		GetMember(crc32 id)						const;

	iso_export void					UnsafeSet(const void *srce)				const;
	template<typename T>	bool	Set(const T &t)							const	{ return s_setget<T>::set(data, type, t); }

	template<typename T>	bool	SetMember(tag2 id, const T &t)			const;
	template<int N>			bool	SetMember(tag2 id, const char (&t)[N])	const { return SetMember(id, (const char*)t); }
	template<typename T, int B>	bool SetMember(tag2 id, ISO_ptr<T,B> t)		const;

	iso_export int					ParseField(string_scan &s)						const;
	iso_export bool					Update(const char *s, bool from = false)		const;
	ISO_ptr_machine<void>			Duplicate(tag2 id = tag2(), int flags = DUPF_CHECKEXTERNALS)	const { return iso::Duplicate(id, type, data, flags);	}
	void			ClearTempFlags()												const { ClearTempFlags(*this); }
	inline const ISO_browser2		Parse(const char *s)							const;
	inline void		Apply(const ISO_type *type, void f(void*))						const;
	inline void		Apply(const ISO_type *type, void f(void*, void*), void *param)	const;
	template<typename T> void Apply(void f(T*))										const { Apply(ISO_getdef<T>(), (void(*)(void*))f);	}
	template<typename T, typename P> void Apply(void f(T*,P*), P *param)			const { Apply(ISO_getdef<T>(), (void(*)(void*, void*))f, (void*)param);	}

	class iterator;
	iso_export iterator	begin()		const;
	iso_export iterator	end()		const;
	operator const_memory_block()	const { return const_memory_block(data, GetSize()); }
};

class ISO_browser2 : public ISO_browser {
	ISO_ptr_machine<void>	p;

public:
	const ISO_ptr_machine<void>	&GetPtr()		const		{ return p; }
	bool						IsPtr()			const		{ return p == data && p.Type() == type; }
	ISO_ptr_machine<void>		TryPtr()		const		{ return IsPtr() ? p : ISO_NULL; }
	bool						IsBin()			const		{ return p.IsBin(); }
	const char*					External()		const		{ return p == data ? p.External() : ISO_browser::External(); }
	bool						HasCRCType()	const		{ return p == data ? p.HasCRCType() : ISO_browser::HasCRCType(); }
	ISO_browser2				SkipUser()		const		{ return ISO_browser2(type->SkipUser(), data, p); }
	void						AddRef()		const		{ if (p) p.Header()->addref(); }
	bool						Release()		const		{ return p && p.Header()->release(); }

	ISO_browser2()		{}
	ISO_browser2(const ISO_type *type, void *data, const ISO_ptr_machine<void> &_p) : ISO_browser(type, data), p(_p)	{}
	ISO_browser2(const ISO_browser2 &b)									: ISO_browser(b), p(b.p)	{}
	ISO_browser2(const ISO_browser &b)									: ISO_browser(b)			{}
	ISO_browser2(const ISO_browser &b, const ISO_ptr_machine<void> &_p)	: ISO_browser(b), p(_p)		{}
	template<typename T, int B> ISO_browser2(const ISO_ptr<T,B> &_p)	: ISO_browser(_p), p(_p)	{}

	template<typename T, int B>operator ISO_ptr<T, B>()	const	{
		if (IsPtr())
			return p;
		if (type && type->Type() == ISO_REFERENCE)
			return ISO_ptr<T, B>::Ptr((T*)type->ReadPtr(data));
		return Duplicate();
	}

	ISO_browser2		&operator=(const ISO_browser &b)	{ ISO_browser::operator=(b); p.Clear(); return *this;	}
	ISO_browser2		&operator=(const ISO_browser2 &b)	{ ISO_browser::operator=(b); p = b.p; return *this;		}

	iso_export const ISO_browser2	operator[](tag2 id)	const;
	iso_export const ISO_browser2	operator[](int i)	const;
	iso_export const ISO_browser2	operator*()			const;

	const ISO_browser2	Index(int i)					const	{ return operator[](i);	}
	const ISO_browser2	GetMember(crc32 id)				const	{ return operator[](tag2(id));	}

	tag2				GetName()				const		{
		return IsPtr() ? p.ID() : type && type->Type() == ISO_REFERENCE ? iso::GetPtr(type->ReadPtr(data)).ID() : tag2();
	}
	bool				SetName(tag2 id)		const		{
		if (IsPtr())
			p.SetID(id);
		else if (type && type->Type() == ISO_REFERENCE)
			iso::GetPtr(type->ReadPtr(data)).SetID(id);
		else
			return false;
		return true;
	}
	iso_export tag2		GetName(int i)			const;
	iso_export const ISO_browser2	Parse(const char *s, bool create = false) const;

	class iterator;
	typedef ISO_browser2	element, reference;
	typedef iterator		const_iterator;
	iso_export iterator	begin()				const;
	iso_export iterator	end()				const;

	friend	const ISO_browser2		operator/(const ISO_browser2 &b, tag2 id)		{ return b[id]; }
};


inline const ISO_browser2 ISO_browser::Index(int i)					const	{ return operator[](i);					}
inline const ISO_browser2 ISO_browser::GetMember(crc32 id)			const	{ return operator[](tag2(id));			}
inline const ISO_browser2 ISO_browser::operator[](tag2 id)			const	{ return ISO_browser2(*this)[id];		}
inline const ISO_browser2 ISO_browser::operator[](int i)			const	{ return ISO_browser2(*this)[i];		}
inline const ISO_browser2 ISO_browser::Parse(const char *s)			const	{ return ISO_browser2(*this).Parse(s);	}
inline const ISO_browser2	operator/(const ISO_browser &b, tag2 id)		{ return b[id]; }

inline void	ISO_browser::Apply(const ISO_type *type, void f(void*))						const { Apply(*this, type, f); ClearTempFlags(); }
inline void	ISO_browser::Apply(const ISO_type *type, void f(void*, void*), void *param)	const { Apply(*this, type, f, param); ClearTempFlags(); }

template<typename T> inline const ISO_browser2 ISO_browser::FindByType(int crit)	const	{ return FindByType(ISO_getdef<T>(), crit);	}

template<typename T> inline ISO_browser2 MakeBrowser(T &&t)	{
	return ISO_ptr<T>(0, forward<T>(t));
}
template<typename T> inline ISO_browser MakeBrowser(T &t)	{
	return ISO_browser(ISO_getdef<T>(), (void*)&t);
}
inline ISO_browser2 MakeBrowser(ISO_browser &&t)	{
	return t;
}
inline ISO_browser2 MakeBrowser(ISO_browser2 &&t)	{
	return t;
}


class PtrBrowser {
protected:
	ISO_ptr_machine<void>	p;
public:
	PtrBrowser()												{}
	PtrBrowser(const ISO_ptr_machine<void> &_p) : p(_p)			{}
	void	Clear()												{ p.Clear();	}
	PtrBrowser&	operator=(const ISO_ptr_machine<void> &_p)		{ p = _p; return *this; }
	operator ISO_ptr_machine<void>&()							{ return p;		}

	const ISO_browser2	operator[](tag2 id)				const	{ return ISO_browser(p)[id];	}
	const ISO_browser2	operator[](int i)				const	{ return ISO_browser(p)[i];		}
	const ISO_browser2	GetMember(crc32 id)				const	{ return ISO_browser(p).GetMember(id);	}
	operator const ISO_browser2()						const	{ return p;						}
	template<typename T, int B> operator ISO_ptr<T,B>()	const	{ return p; }
	template<typename T> operator T*()					const	{ return p; }
	operator void*()									const	{ return p; }
	template<typename T> T			get()				const	{ return ISO_browser(p).get<T>(); }
	getter<const PtrBrowser>		get()				const	{ return *this; }
};

class ISO_browser_root : public ISO_browser {
	singleton<anything>		names;
public:
	ISO_browser_root()					{ ISO_browser::operator=(ISO_browser(names())); }
	~ISO_browser_root()					{ names->Clear(); }
	void		Add(ISO_ptr<void> p)	{ names->Append(p); }
	anything*	operator&()				{ return &names(); }
};
extern iso_export ISO_browser_root root;

template<typename T> struct ISO_browser::as : ISO_browser {
	as(const ISO_browser &_b) : ISO_browser(_b)	{}
				operator T()			{ return Get<T>();						}
	T			operator+=(T t)			{ Set<T>(t += Get<T>(T())); return t;	}
	T			operator-=(T t)			{ Set<T>(t -= Get<T>(T())); return t;	}
	T			operator*=(T t)			{ Set<T>(t *= Get<T>(T())); return t;	}
	T			operator/=(T t)			{ Set<T>(t /= Get<T>(T())); return t;	}
	const T&	operator=(const T &t)	{ Set<T>(t); return t;	}
};

//-----------------------------------------------------------------------------
//	ISO_virtual
//-----------------------------------------------------------------------------

struct ISO_virtual : ISO_type {
	static const FLAGS	DEFER = FLAG0, VIRTSIZE = UNSTORED0;
	uint32				(*Count)(ISO_virtual *type, void *data);
	tag2				(*GetName)(ISO_virtual *type, void *data, int i);
	int					(*GetIndex)(ISO_virtual *type, void *data, const tag2 &id, int from);
	ISO_browser2		(*Index)(ISO_virtual *type, void *data, int i);
	ISO_browser2		(*Deref)(ISO_virtual *type, void *data);
	void				(*Delete)(ISO_virtual *type, void *data);
	bool				(*Update)(ISO_virtual *type, void *data, const char *spec, bool from);
	uint32				GetSize()				const	{ return param16; }
	bool				IsVirtPtr(void *data)			{ uint32 c = Count(this, data); return !c || !~c; }
	template<typename T> ISO_virtual(T*, FLAGS flags = NONE) : ISO_type(ISO_VIRTUAL, flags),
		Count(T::Count), GetName(T::GetName), GetIndex(T::GetIndex),
		Index(T::Index), Deref(T::Deref), Delete(T::Delete), Update(T::Update)
	{
		if (!(flags & VIRTSIZE))
			param16 = sizeof(T);
	}
};

inline bool ISO_browser::IsVirtPtr() const {
	return type && type->Type() == ISO_VIRTUAL && !type->Fixed() && ((ISO_virtual*)type)->IsVirtPtr(data);
}

template<typename T> bool ISO_browser::SetMember(tag2 id, const T &t) const {
	if (ISO_browser b = (*this)[id])
		return b.Set(t);
	void		*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE: {
				CISO_type_composite &comp = *(CISO_type_composite*)type;
				int	i = comp.GetIndex(id);
				return i < 0 ? false
					: ISO_browser(comp[i].type, (char*)data + comp[i].offset).Set(t);
			}
			case ISO_OPENARRAY:
				if (((ISO_type_openarray*)type)->subtype->SkipUser()->Type() == ISO_REFERENCE) {
					((anything*)data)->Append(ISO_ptr<T>(id, t));
					return true;
				}
				return false;

			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					return virt->Deref(virt, data).SetMember(id, t);
				}
			default:
				return false;
		}
	}
	return false;
}
template<typename T, int B> bool ISO_browser::SetMember(tag2 id, ISO_ptr<T, B> t) const {
	void		*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE: {
				CISO_type_composite &comp = *(CISO_type_composite*)type;
				int	i = comp.GetIndex(id);
				return i < 0 ? false
					: ISO_browser(comp[i].type, (char*)data + comp[i].offset).Set(t);
			}
			case ISO_OPENARRAY:
				if (((ISO_type_openarray*)type)->subtype->SkipUser()->Type() == ISO_REFERENCE) {
					if (!t.ID())
						t.SetID(id);
					else if (t.ID() != id)
						t = iso::Duplicate(id, t);

					anything	&a	= *(anything*)data;
					int			i	= a.GetIndex(id);
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

			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					return virt->Deref(virt, data).SetMember(id, t);
				}
			default:
				return false;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//	ISO_virtual
//-----------------------------------------------------------------------------

struct ISO_virtual_defaults {
	static uint32		Count()												{ return 0;				}
	static tag2			GetName(int i)										{ return tag2();		}
	static int			GetIndex(const tag2 &id, int from)					{ return -1;			}
	static ISO_browser2	Index(int i)										{ return ISO_browser2();}
	static ISO_browser2	Deref()												{ return ISO_browser2();}
	static bool			Update(const char *s, bool from)					{ return false;			}
};

// functions defined in T
template<class T> class TISO_virtual : public ISO_virtual {
public:
	static uint32		Count(ISO_virtual *type, void *data)				{ return uint32(((T*)data)->Count());	}
	static tag2			GetName(ISO_virtual *type, void *data, int i)		{ return tag2(((T*)data)->GetName(i));	}
	static int			GetIndex(ISO_virtual *type, void *data, const tag2 &id, int from)	{ return ((T*)data)->GetIndex(id, from); }
	static ISO_browser2	Index(ISO_virtual *type, void *data, int i)			{ return ((T*)data)->Index(i);			}
	static ISO_browser2	Deref(ISO_virtual *type, void *data)				{ return ((T*)data)->Deref();			}
	static void			Delete(ISO_virtual *type, void *data)				{ return ((T*)data)->~T();				}
	static bool			Update(ISO_virtual *type, void *data, const char *s, bool from)	{ return ((T*)data)->Update(s, from); }
	typedef T type;
	TISO_virtual(FLAGS flags = NONE) : ISO_virtual(this, flags) {}
};

// functions defined in V
template<class T, class V> class TISO_virtual1 : public ISO_virtual {
public:
//default implementations
	static uint32		Count(T &a)											{ return 0;				}
	static tag2			GetName(T &a, int i)								{ return tag2();		}
	static int			GetIndex(T &a, const tag2 &id, int from)			{ return -1;			}
	static ISO_browser2	Index(T &a, int i)									{ return ISO_browser();	}
	static ISO_browser2	Deref(T &a)											{ return ISO_browser();	}
	static bool			Update(T &a, const char *s, bool from)				{ return false;	}
	static void			Delete(T &a)										{}
public:
	static uint32		Count(ISO_virtual *type, void *data)				{ return static_cast<V*>(type)->Count(*(T*)data);				}
	static tag2			GetName(ISO_virtual *type, void *data, int i)		{ return tag2(static_cast<V*>(type)->GetName(*(T*)data, i));	}
	static int			GetIndex(ISO_virtual *type, void *data, const tag2 &id, int from)	{ return static_cast<V*>(type)->GetIndex(*(T*)data, id, from); }
	static ISO_browser2	Index(ISO_virtual *type, void *data, int i)			{ return static_cast<V*>(type)->Index(*(T*)data, i);			}
	static ISO_browser2	Deref(ISO_virtual *type, void *data)				{ return static_cast<V*>(type)->Deref(*(T*)data);				}
	static void			Delete(ISO_virtual *type, void *data)				{ static_cast<V*>(type)->Delete(*(T*)data);						}
	static bool			Update(ISO_virtual *type, void *data, const char *s, bool from)	{ return static_cast<V*>(type)->Update(*(T*)data, s, from); }
	template<typename X> TISO_virtual1(X *me, FLAGS flags = NONE)	: ISO_virtual(me, flags)	{}
	TISO_virtual1(FLAGS flags = NONE)								: ISO_virtual(this, flags)	{}
};

// functions defined in ISO_def<T>
template<class T> class TISO_virtual2 : public TISO_virtual1<T, ISO_def<T> > {
public:
	static void			Delete(ISO_virtual *type, void *data)				{ ((T*)data)->~T(); }
 	TISO_virtual2(ISO_type::FLAGS flags = ISO_type::NONE) : TISO_virtual1<T, ISO_def<T> >(this, flags) {}
};

// wrap an accessor
template<class S, class T, T> struct TISO_accessor;

// returns R
template<class S, class C, typename R, R (C::*f)() const> struct TISO_accessor<S, R (C::*)() const, f>
	: public TISO_virtual1<S, TISO_accessor<S, R (C::*)() const, f> > {
	static ISO_browser2	Deref(S &a)	{ return MakePtr(tag2(), (a.*f)()); }
};
// returns R*
template<class S, class C, typename R, R* (C::*f)() const> struct TISO_accessor<S, R* (C::*)() const, f>
	: public TISO_virtual1<S, TISO_accessor<S, R* (C::*)() const, f> > {
	static ISO_browser2	Deref(S &a)	{ return MakeBrowser(*(a.*f)()); }
};
// returns const char*
template<class S, class C, const char* (C::*f)() const> struct TISO_accessor<S, const char* (C::*)() const, f>
	: public TISO_virtual1<S, TISO_accessor<S, const char* (C::*)() const, f> > {
	static ISO_browser2	Deref(S &a)	{ return MakePtr(tag(), (a.*f)()); }
};
// returns const char16*
template<class S, class C, const char16* (C::*f)() const> struct TISO_accessor<S, const char16* (C::*)() const, f>
	: public TISO_virtual1<S, TISO_accessor<S, const char16* (C::*)() const, f> > {
	static ISO_browser2	Deref(S &a)	{ return MakePtr(tag(), (a.*f)()); }
};
// returns ISO_browser2
template<class S, class C, ISO_browser2 (C::*f)() const> struct TISO_accessor<S, ISO_browser2 (C::*)() const, f>
	: public TISO_virtual1<S, TISO_accessor<S, ISO_browser2 (C::*)() const, f> > {
	static ISO_browser2	Deref(S &a)	{ return (a.*f)(); }
};
// with parameter, returns R
template<class S, class C, typename P, typename R, R (C::*f)(P) const> struct TISO_accessor<S, R (C::*)(P) const, f>
	: public TISO_virtual1<S, TISO_accessor<S, R (C::*)(P) const, f> > {
	static ISO_browser2	Deref(S &a)	{ return !a.t ? iso_nil : MakePtr(tag2(), (a.t->*f)(a.p)); }
};
// with parameter, returns ISO_browser
template<class S, class C, typename P, ISO_browser (C::*f)(P) const> struct TISO_accessor<S, ISO_browser (C::*)(P) const, f>
	: public TISO_virtual1<S, TISO_accessor<S, ISO_browser (C::*)(P) const, f> > {
	static ISO_browser2	Deref(S &a)	{ return (a.t.*f)(a.p); }
};
// with parameter, returns ISO_browser2
template<class S, class C, typename P, ISO_browser2 (C::*f)(P) const> struct TISO_accessor<S, ISO_browser2 (C::*)(P) const, f>
	: public TISO_virtual1<S, TISO_accessor<S, ISO_browser2 (C::*)(P) const, f> > {
	static ISO_browser2	Deref(S &a)	{ return (a.t.*f)(a.p); }
};
// access field
template<class S, class C, typename T, T C::*f> struct TISO_accessor<S*, T C::*, f>
	: public TISO_virtual1<S*, TISO_accessor<S*, T C::*, f> > {
	static ISO_browser2	Deref(S *a)	{ return !a ? MakeBrowser(iso_nil) : MakeBrowser(a->*f); }
};
template<class S, class C, typename T, T C::*f> struct TISO_accessor<S, T C::*, f>
	: public TISO_virtual1<S, TISO_accessor<S, T C::*, f> > {
	static ISO_browser2	Deref(S &a)	{ return MakeBrowser(get(a.get(f))); }
};


template<typename F> struct ISO_get_accessor_s {
	template<class S, F f> static ISO_type *get() {
		static TISO_accessor<S, F, f>	def;
		return &def;
	}
};
#define ISO_accessor(f)	T_get_class<ISO_get_accessor_s>(f)->get<_S, f>()

template<typename F> struct ISO_element_setter;

template<typename C, typename T> struct ISO_element_setter<T C::*> {
	typedef T C::*F;
	template<typename S> struct set1 {
		template<F f> static uint32 set2(tag id, ISO_element &e) {
			auto&		p = ((S*)1)->*f;
			uintptr_t	o = intptr_t(&p) - 1;
			return e.set(id, ISO_getdef<T>(), uint32(o));
		}
	};
	template<typename S, typename P> struct set1<param_element<S,P> > {
		template<F f> static uint32 set2(tag id, ISO_element &e) {
			static TISO_accessor<param_element<S,P>, F, f>	def;
			return e.set(id, &def, 0, 0);
		}
	};
	template<typename S, F f> static uint32 set(tag id, ISO_element &e) {
		return set1<S>::template set2<f>(id, e);
	}
};

template<typename C, typename R> struct ISO_element_setter<R (C::*)() const> {
	typedef R (C::*F)() const;
	template<typename S, F f> static uint32 set(tag id, ISO_element &e) {
		static TISO_accessor<S, F, f>	def;
		return e.set(id, &def, 0, 0);
	}
};

template<typename C, typename R, typename P> struct ISO_element_setter<R (C::*)(P) const> {
	typedef R (C::*F)(P) const;
	template<typename S, F f> static uint32 set(tag id, ISO_element &e) {
		static TISO_accessor<S, F, f>	def;
		return e.set(id, &def, 0, 0);
	}
};

//-------------------------------------
// param

template<typename T, typename P> struct ISO_def<param_element<T, P> > : TISO_virtual2<param_element<T, P> > {
	typedef	param_element<T, P>	A;
	uint32			Count(A &a)								{ return MakeBrowser(get(a)).Count(); }
	tag2			GetName(A &a, int i)					{ return MakeBrowser(get(a)).GetName(i); }
	int				GetIndex(A &a, const tag2 &id, int from){ return MakeBrowser(get(a)).GetIndex(id, from); }
	ISO_browser2	Index(A &a, int i)						{ return MakeBrowser(get(a)).Index(i); }
	ISO_browser2	Deref(A &a)								{ ISO_browser2 b = MakeBrowser(get(a)); if (ISO_browser2 b2 = *b) return b2; return b; }
	bool			Update(A &a, const char *s, bool from)	{ return MakeBrowser(get(a)).Update(s, from); }
};

template<typename T, typename P> struct ISO_def<param_element<const T, P> > : ISO_def<param_element<T, P> >	{};
template<typename T, typename P> struct ISO_def<param_element<const T&, P> > : ISO_def<param_element<T&, P> >	{};

//-------------------------------------
// endian

template<typename T> tag2 GetName() {
	const ISO_type	*t = ISO_getdef<T>();
	if (t->Type() == ISO_USER)
		return ((ISO_type_user*)t)->ID();
	return (const char*)buffer_accum<256>(t);
}

template<typename T> struct ISO_def<T_swap_endian<T> > : CISO_type_user {
	struct V : TISO_virtual1<T_swap_endian<T>, V> {
		static ISO_browser2	Deref(const T_swap_endian<T> &a)	{ return MakePtr(tag(), get(a)); }
	} v;
	ISO_def() : CISO_type_user(GetName<T>(), &v)	{}
};

template<typename T, int BITS> struct ISO_def<compact<T, BITS> > : CISO_type_user {
	struct V : TISO_virtual1<compact<T, BITS>, V> {
		static ISO_browser2	Deref(const compact<T, BITS> &a)	{ return MakePtr(tag(), a.get()); }
	} v;
	ISO_def() : CISO_type_user(GetName<T>(), &v)	{}
};

//-------------------------------------
// pointers

template<typename T> struct ISO_def<T*> : TISO_virtual2<T*> {
	static ISO_browser	Deref(T *a)					{ return !a ? MakeBrowser(iso_nil) : MakeBrowser(*a); }
	static bool			Update(T *a, const char *s, bool from)	{ return Deref(a).Update(s, from);	}
};
template<> struct ISO_def<void*> : ISO_def<baseint<16, uintptr_t> > {};

template<typename T> struct ISO_def<pointer<T> > {
	ISO_type *operator&() { return ISO_getdef<T*>(); }
};

template<typename T, typename B> struct ISO_def<soft_pointer<T,B> > : TISO_virtual2<soft_pointer<T,B> > {
	static ISO_browser	Deref(soft_pointer<T,B> &a)	{ return !a ? MakeBrowser(iso_nil) : MakeBrowser(*a.get()); }
};

template<typename T> struct ISO_def<shared_ptr<T> > : TISO_virtual2<shared_ptr<T> > {
	static ISO_browser	Deref(shared_ptr<T> &a)	{ return !a ? MakeBrowser(iso_nil) : MakeBrowser(*a); }
};

template<typename T> struct ISO_def<pointer32<T> > : TISO_virtual2<pointer32<T> > {
	static ISO_browser	Deref(pointer32<T> &a)	{ return !a ? MakeBrowser(iso_nil) : MakeBrowser(*a.get()); }
};

//-------------------------------------
// containers

//template<typename T, typename P> tag2 __GetName(const T &t, const P &p, T_checktype<tag2(T::*)(P p) const, &T::ID> *dummy = 0) {
//	return t->ID(p);
//}
template<typename T, typename P> tag2 _GetName(const param_element<T,P> &t) {
//	return __GetName(t.t, t.p);
	return _GetName(get(t));
}

template<typename T> struct TISO_virtualarray : TISO_virtual2<T> {
	static uint32		Count(T &a)						{ return uint32(a.size());	}
	static ISO_browser2	Index(T &a, int i)				{ return MakeBrowser(a[i]);	}
	static tag2			GetName(T &a, int i)			{ return _GetName(a[i]);	}
	static int			GetIndex(T &a, const tag2 &id, int from)	{ return _GetIndex(id, a, from);	}
};

template<typename T> struct TISO_virtualtree : TISO_virtual2<T> {
	static uint32		Count(T &a)						{ return uint32(a.size());	}
	static ISO_browser2	Index(T &a, int i)				{ return MakeBrowser(*nth(a.begin(), i)); }
	static tag2			GetName(T &a, int i)			{ return _GetName(*nth(a.begin(), i)); }
	static int			GetIndex(T &a, const tag2 &id, int from)	{ return _GetIndex(id, a, from);	}
};

template<typename T> struct TISO_virtualarray_ptr : TISO_virtualarray<T> {
	static ISO_browser2	Index(T &a, int i)				{ return MakePtrCheck(tag2(), a[i]);	}
};

template<typename T> struct ISO_def<ptr_array<T> >				: TISO_virtualarray<ptr_array<T> >		{};
template<typename T> struct ISO_def<dynamic_array<T> >			: TISO_virtualarray<dynamic_array<T> >	{};
template<typename T> struct ISO_def<range<T> >					: TISO_virtualarray<range<T> >			{};
template<typename T> struct ISO_def<cached_range<T> >			: TISO_virtualarray<cached_range<T> >	{};
template<typename T, int N> struct ISO_def<split_range<T, N> >	: TISO_virtualarray<split_range<T, N> >	{};

template<typename K, typename T> struct ISO_def<hash_map<K,T> >	: TISO_virtual2<hash_map<K,T> > {
	typedef hash_map<K,T> type;
	static uint32		Count(type &a)						{ return uint32(a.size());	}
	static ISO_browser2	Index(type &a, int i)				{ return MakeBrowser(*a.by_index(i)); }
	static tag2			GetName(type &a, int i)				{ return _GetName(*a.by_index(i));	}
	static int			GetIndex(type &a, const tag2 &id, int from)	{ return a.index_of(a.check(id));	}
};

template<typename T> struct ISO_def<e_treenode0<T> >	: TISO_virtualtree<e_treenode0<T> >	{};
template<typename T> struct ISO_def<e_treenode<T> >		: TISO_virtualtree<e_treenode<T> >	{};
template<typename T> struct ISO_def<e_rbtree<T> >		: TISO_virtualtree<e_rbtree<T> >	{};
template<typename T> struct ISO_def<rbtree<T> >			: TISO_virtualtree<rbtree<T> >		{};
template<typename K> struct ISO_def<set<K> >			: TISO_virtualtree<set<K> >			{};
template<typename K> struct ISO_def<multiset<K> >		: TISO_virtualtree<multiset<K> >	{};

template<typename K, typename V> struct ISO_def<map<K,V> > : TISO_virtualtree<map<K,V> > {
	static tag2		GetName(map<K,V> &a, int i)						{ return to_string(nth(a.begin(), i).key()); }
	static int		GetIndex(map<K,V> &a, const tag &id, int from)	{ if (auto i = a.find(id)) return int(distance(a.begin(), i)); return -1; }
};
template<typename K, typename V> struct ISO_def<multimap<K, V> > : TISO_virtualtree<multimap<K, V> > {
	static tag2		GetName(multimap<K,V> &a, int i)						{ return to_string(nth(a.begin(), i).key()); }
	static int		GetIndex(multimap<K,V> &a, const tag2 &id, int from)	{ if (auto i = a.find(id)) return int(distance(a.begin(), i)); return -1; }
};

template<typename T> struct ISO_def<hierarchy<T> > : TISO_virtual2<hierarchy<T> >	{
	typedef hierarchy<T> type;
	static uint32		Count(type &a)						{ return a.children.size32();	}
	static ISO_browser	Index(type &a, int i)				{ return MakeBrowser(a.children[i]); }
	static tag2			GetName(type &a, int i)				{ return _GetName(a.children[i]); }
	static int			GetIndex(type &a, const tag &id, int from)	{ return _GetIndex(id, a.children, from);	}
};

//-------------------------------------
// memory

struct memory_block_deref : ISO_virtual_defaults {
	const void		*t;
	ISO_type_array	type;
	ISO_browser		Deref()		const	{ return ISO_browser(&type, unconst(t)); }
	memory_block_deref(const void *_t, uint32 len) : t(_t), type(ISO_getdef<xint8>(), len, 1) {}
	memory_block_deref(const memory_block &a)		: t(a), type(ISO_getdef<xint8>(), a.length32(), 1) {}
	memory_block_deref(const const_memory_block &a) : t(a), type(ISO_getdef<xint8>(), a.length32(), 1) {}
};
template<> struct ISO_def<memory_block_deref> : TISO_virtual<memory_block_deref> {};

template<> struct ISO_def<memory_block> : CISO_type_user {
	struct virt : TISO_virtual1<memory_block, virt> {
		static uint32		Count(memory_block &a)				{ return a.length32();						}
		static ISO_browser2	Index(memory_block &a, int i)		{ return MakeBrowser(((xint8*)a)[i]);		}
		static ISO_browser2	Deref(memory_block &a)				{ return ISO_ptr<memory_block_deref>(0, a);	}
		static void			Delete(memory_block &a)				{}
		static bool			Update(memory_block &a, const char *spec, bool from);
	} v;
	ISO_def() : CISO_type_user("Bin", &v) {}
};
template<> struct ISO_def<const_memory_block> : CISO_type_user {
	struct virt : TISO_virtual1<const_memory_block, virt> {
		static uint32		Count(const_memory_block &a)		{ return a.length32();						}
		static ISO_browser2	Index(const_memory_block &a, int i)	{ return MakeBrowser(((const xint8*)a)[i]);	}
		static ISO_browser2	Deref(const_memory_block &a)		{ return ISO_ptr<memory_block_deref>(0, a);	}
		static void			Delete(const_memory_block &a)		{}
		static bool			Update(const_memory_block &a, const char *spec, bool from);
	} v;
	ISO_def() : CISO_type_user("Bin", &v) {}
};
template<> struct ISO_def<malloc_block> : ISO_def<memory_block> {};

//-------------------------------------
// others

template<int N> struct ISO_def<string_base<_pascal_string<N> > > : TISO_virtual2<string_base<_pascal_string<N> > > {
	static ISO_browser2	Deref(string_base<_pascal_string<N> > &a) { return ISO_ptr<string>(0, a);	}
};

template<> struct ISO_def<string_base<embedded<char> > > : TISO_virtual2<string_base<embedded<char> > > {
	static ISO_browser2	Deref(string_base<embedded<char> > &a) { return ISO_ptr<string>(0, a);	}
};
template<> struct ISO_def<string_base<embedded<char16> > > : TISO_virtual2<string_base<embedded<char16> > > {
	static ISO_browser2	Deref(string_base<embedded<char16> > &a) { return ISO_ptr<string16>(0, a);	}
};

//-----------------------------------------------------------------------------
//	ISO_combine
//-----------------------------------------------------------------------------

template<typename A, typename B> struct ISO_combiner : pair<A,B>, ISO_virtual_defaults {
	using pair<A,B>::a;
	using pair<A,B>::b;
	int				na, nb;

	int				Count()			{ if (!nb) nb = this->b.Count(); return na + nb; }
	tag2			GetName(int i)	{ return i < na ? this->a.GetName(i) : this->b.GetName(i - na); }
	ISO_browser2	Index(int i)	{ return i < na ? this->a.Index(i) : this->b.Index(i - na); }
	int				GetIndex(const tag2 &id, int from) {
		int	n = from < na ? this->a.GetIndex(id, from) : -1;
		if (n == -1) {
			n = this->b.GetIndex(id, from < na ? 0 : from - na);
			if (n != -1)
				n += na;
		}
		return n;
	}
	ISO_combiner(const A &_a, const B &_b) : pair<A,B>(_a, _b), na(this->a.Count()), nb(0) {}
	template<typename A1, typename B1> ISO_combiner(const pair<A1,B1> &p) : pair<A,B>(p), na(this->a.Count()), nb(0) {}
};

template<typename A, typename B> ISO_ptr<ISO_combiner<A,B> > ISO_combine(tag2 id, const A &a, const B &b) {
	return ISO_ptr<ISO_combiner<A,B> >(id, make_pair(a, b));
}

template<typename A, typename B> struct ISO_def<ISO_combiner<A, B> > : public TISO_virtual<ISO_combiner<A, B> > {};

//-----------------------------------------------------------------------------
//	ISO_bitpacked
//-----------------------------------------------------------------------------

struct ISO_bitpacked : TISO_virtual1<uint8,ISO_bitpacked>, trailing_array<ISO_bitpacked, ISO_element> {
	static ISO_ptr<uint64>	GetTemp(void *tag);

	uint32			count;

	uint32			Count(uint8 &data)				{ return count;	}
	tag2			GetName(uint8 &data, int i)		{ return array(i).id.get_tag(); }
	int				GetIndex(uint8 &data, const tag2 &id, int from);
	ISO_browser2	Index(uint8 &data, int i);
	bool			Update(uint8 &data, const char *spec, bool from);

	void Add(const ISO_type* type, tag id, int start, int bits) { array(count++).set(id, type, start, bits); }
	template<typename T> void Add(tag id, int start, int bits)	{ Add(ISO_getdef<T>(), id, start, bits); }
	void Add(const ISO_type* type, tag id, int bits)			{ Add(type, id, count ? array(count - 1).end() : 0, bits); }
	template<typename T> void Add(tag id, int bits)				{ Add(ISO_getdef<T>(), id, bits); }

	ISO_bitpacked(int n = 0) : count(n) {}
	void*			operator new(size_t size, void *p)			{ return p; }
	void*			operator new(size_t, uint32 n)				{ return ISO_type::operator new(calc_size(n)); }
	void			operator delete(void *p, size_t, uint32)	{ ISO_type::operator delete(p); }
};

template<int N> struct TISO_bitpacked : ISO_bitpacked { ISO_element fields[N]; };
#define ISO_BITFIELD(P,S,F)	{ union { iso::T_uint_type<sizeof(S)>::type u; S s; } x; x.u = 0; x.s.F = ~0; P.Add(ISO_getdef(x.s.F), #F, lowest_set_index(x.u), highest_set_index(x.u) - lowest_set_index(x.u) + 1); }

//-----------------------------------------------------------------------------
//	ISO_browser:iterator
//-----------------------------------------------------------------------------

class ISO_browser::iterator {
protected:
	const ISO_type	*type;
	uint8			*start;
	size_t			stride;
	union {
		uintptr_t			current;
		void				*data;
		const ISO_element	*element;
	};
public:
	iterator() : type(0), start(0), stride(0), current(0) {}
	iterator(const ISO_type *_type, void *_data, size_t _stride) : type(_type), start(0), stride(_stride), data(_data) {}
	iterator(void *_data, const ISO_element *_element) : type(0), start((uint8*)_data), stride(sizeof(ISO_element)), element(_element) {}
	iterator(const ISO_virtual *_type, void *_data, uint32 _index) : type(_type), start((uint8*)_data), stride(1), current(_index) {}

	ISO_browser2 Ref() const {
		if (!start)
			return ISO_browser(type, data);
		if (!type)
			return ISO_browser(element->type, start + element->offset);
		return ((ISO_virtual*)type)->Index((ISO_virtual*)type, start, int(current));
	}
	ISO_browser2 operator*() const {
		if (!start)
			return type->Type() == ISO_REFERENCE ? *ISO_browser(type, data) : ISO_browser2(type, data, ISO_NULL);
		if (!type)
			return ISO_browser(element->type, start + element->offset);
		return ((ISO_virtual*)type)->Index((ISO_virtual*)type, start, int(current));
	}

	iterator&	operator++()		{ current += stride; return *this; }
	iterator&	operator--()		{ current -= stride; return *this;	}
	iterator	operator++(int)		{ iterator b(*this); operator++(); return b; }
	iterator	operator--(int)		{ iterator b(*this); operator--(); return b; }

	bool		operator==(const iterator &b)	const	{ return current == b.current; }
	bool		operator!=(const iterator &b)	const	{ return current != b.current; }
	ref_helper<ISO_browser2> operator->()		const	{ return operator*(); }

	tag2		GetName() const {
		return	!start			? (type->SkipUser()->Type() == ISO_REFERENCE ? ((ISO_ptr<void>*)data)->ID() : tag2())
			:	!type			? element->id.get_tag2(0)
			:	((ISO_virtual*)type)->GetName((ISO_virtual*)type, start, int(current));
	}
};

class ISO_browser2::iterator : public ISO_browser::iterator {
	ISO_ptr<void, PTRBITS>	p;
public:
	iterator() : ISO_browser::iterator() {}
	iterator(const ISO_browser::iterator &i, const ISO_ptr<void, PTRBITS> &_p) : ISO_browser::iterator(i), p(_p) {}

	ISO_browser2 operator*() const {
		if (!start)
			return type->Type() == ISO_REFERENCE ? *ISO_browser2(type, data, p) : ISO_browser2(type, data, p);
		if (!type)
			return ISO_browser2(element->type, start + element->offset, p);
		return ((ISO_virtual*)type)->Index((ISO_virtual*)type, start, int(current));
	}

	iterator&	operator++()		{ current += stride; return *this; }
	iterator&	operator--()		{ current -= stride; return *this;	}
	iterator	operator++(int)		{ iterator b(*this); operator++(); return b; }
	iterator	operator--(int)		{ iterator b(*this); operator--(); return b; }
};

//-----------------------------------------------------------------------------
//	ISO_browser set/get
//-----------------------------------------------------------------------------

template<typename T> struct ISO_browser::s_setget {
	static T	get(void *data, const ISO_type *type, const T &t);
	static bool	set(void *data, const ISO_type *type, const T &t);
};

template<> bool ISO_browser::s_setget<bool>::get(void *data, const ISO_type *type, const bool &t);

template<typename T> T ISO_browser::s_setget<T>::get(void *data, const ISO_type *type, const T &t) {
	while (type) {
		switch (type->Type()) {
			case ISO_INT:
				return T(T(((CISO_type_int*)type)->get<(uint32)sizeof(T)*8>(data)) / ((CISO_type_int*)type)->frac_factor());
			case ISO_FLOAT:
				return ((CISO_type_float*)type)->num_bits() > 32
					? T(((CISO_type_float*)type)->get64(data))
					: T(((CISO_type_float*)type)->get(data));
			case ISO_STRING:
				if (const char *p = ((ISO_type_string*)type)->get(data)) {
					T	t2;
					from_string(p, t2);
					return t2;
				}
				break;
			case ISO_REFERENCE: {
				void *p	= type->ReadPtr(data);
				type	= !p || GetHeader(p)->IsExternal() ? 0 : GetHeader(p)->Type();
				data	= p;
				break;
			}
			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;
			case ISO_VIRTUAL:
				if (!type->Fixed())
					return ((ISO_virtual*)type)->Deref((ISO_virtual*)type, data).Get(t);
			default:
				return t;
		}
	}
	return t;
}
template<typename T> bool ISO_browser::s_setget<T>::set(void *data, const ISO_type *type, const T &t) {
	while (type) {
		switch (type->Type()) {
			case ISO_INT:
				((CISO_type_int*)type)->set(data, int64(t * ((CISO_type_int*)type)->frac_factor()));
				return true;
			case ISO_FLOAT:
				((CISO_type_float*)type)->set(data, float(t));
				return true;
			case ISO_REFERENCE: {
				void *p	= type->ReadPtr(data);
				type	= !p || GetHeader(p)->IsExternal() ? 0 : GetHeader(p)->Type();
				data	= p;
				break;
			}
			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;
			case ISO_VIRTUAL:
				if (!type->Fixed())
					return ((ISO_virtual*)type)->Deref((ISO_virtual*)type, data).Set(t);
			default:
				return false;
		}
	}
	return false;
}

template<> struct ISO_browser::s_setget<const char*> {
	static iso_export const char*	get(void *data, const ISO_type *type, const char *t);
	static iso_export bool			set(void *data, const ISO_type *type, const char *t);
};
template<> struct ISO_browser::s_setget<string> : ISO_browser::s_setget<const char*> {
	static iso_export string		get(void *data, const ISO_type *type, const char *t);
};

template<typename T> struct ISO_browser::s_setget<const T*> {
	static const T*	get(void *data, const ISO_type *type, const T *t);
};
template<typename T> const T *ISO_browser::s_setget<const T*>::get(void *data, const ISO_type *type, const T *t) {
	while (type) {
		if (type->SameAs(ISO_getdef<T>()))
			return (T*)data;
		switch (type->Type()) {
			case ISO_REFERENCE: {
				void *p	= type->ReadPtr(data);
				type	= !p || GetHeader(p)->IsExternal() ? 0 : GetHeader(p)->Type();
				data	= p;
				break;
			}
			case ISO_OPENARRAY:
				data	= type->ReadPtr(data);
				type	= ((ISO_type_openarray*)type)->subtype;
				break;
			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;
			case ISO_VIRTUAL:
				if (!type->Fixed())
					return ((ISO_virtual*)type)->Deref((ISO_virtual*)type, data).Get(t);
			default:
				return t;
		}
	}
	return t;
}
template<typename T> struct ISO_browser::s_setget<T*> {
	static T*	get(void *data, const ISO_type *type, T *t) {
		return const_cast<T*>(ISO_browser::s_setget<const T*>::get(data, type, t));
	}
	static bool	set(void *data, const ISO_type *type, T *t) {
		return ISO_browser::s_setget<const T*>::set(data, type, t);
	}
};

template<int B> struct ISO_browser::s_setget<ISO_ptr<void, B> > {
	static ISO_ptr<void,B>	get(void *data, const ISO_type *type, const ISO_ptr<void, B> &t) {
		type = type->SkipUser();
		if (type->Type() == ISO_REFERENCE)
			return GetPtr<B>(type->ReadPtr(data));
		return t;
	}
	static bool				set(void *data, const ISO_type *type, const ISO_ptr<void, B> &t) {
		if (!data)
			return false;
		const ISO_type *type2 = type->SkipUser();
		if (type2 && type2->Type() == ISO_REFERENCE) {
			if (type2->SubType()->SameAs(t.Type(), ISOMATCH_MATCHNULLS)) {
				if (type2->Is64Bit())
					*(ISO_ptr<void,64>*)data = t;
				else
					*(ISO_ptr<void,32>*)data = t;
				return true;
			}
		} else if (type->SameAs(t.Type(), ISOMATCH_MATCHNULLS)) {
			CopyData(t.Type(), t, data);
			return true;
		} else {
			const ISO_type *type1 = t.Type()->SkipUser();
			if (type1 && type1->Type() == ISO_STRING)
				return s_setget<const char*>::set(data, type, ((ISO_type_string*)type1)->get(t));
		}
		return false;
	}
};

template<typename T, int B> struct ISO_browser::s_setget<ISO_ptr<T, B> > {
	static ISO_ptr<T,B>	get(void *data, const ISO_type *type, const ISO_ptr<T, B> &t) {
		type = type->SkipUser();
		return (type->Type() == ISO_REFERENCE) && (data = type->ReadPtr(data)) && GetHeader(data)->IsType<T>() ? GetPtr<B>(data) : t;
	}
	static bool			set(void *data, const ISO_type *type, const ISO_ptr<T, B> &t) {
		const ISO_type *type2 = type->SkipUser();
		const ISO_type *type0 = ISO_getdef<T>();
		if (type2->Type() == ISO_REFERENCE) {
			if (type2->Is64Bit()) {
				ISO_ptr<void,64>	*p = (ISO_ptr<void,64>*)data;
				if (p->IsType(type0, ISOMATCH_MATCHNULLS)) {
					*p = t;
					return true;
				}
			} else {
				ISO_ptr<void,32>	*p = (ISO_ptr<void,32>*)data;
				if (p->IsType(type0, ISOMATCH_MATCHNULLS)) {
					*p = t;
					return true;
				}
			}
		}
		if (type->SameAs(type0, ISOMATCH_MATCHNULLS)) {
			*(T*)data = *t;
			return true;
		}
		return false;
	}
};

template<typename T, int B> struct ISO_browser::s_setget<ISO_openarray<T, B> > {
	static ISO_openarray<T, B>	get(void *data, const ISO_type *type, const ISO_openarray<T, B> &t) {
		const ISO_type *type2 = type->SkipUser();
		if (type2->Type() == ISO_OPENARRAY && ((ISO_type_openarray*)type2)->subtype->SameAs<T>(ISOMATCH_MATCHNULLS))
			return ISO_openarray<T, B>::Ptr((T*)type2->ReadPtr(data));
		return t;
	}
	static bool			set(void *data, const ISO_type *type, const ISO_openarray<T, B> &t) {
		const ISO_type *type2 = type->SkipUser();
		if (type2->Type() == ISO_REFERENCE) {
			ISO_ptr<void>	p = GetPtr(type->ReadPtr(data));
			return set(p, p.Type(), t);
		}
		if (type2->Type() == ISO_OPENARRAY && ((ISO_type_openarray*)type2)->subtype->SameAs<T>(ISOMATCH_MATCHNULLS)) {
			type2->WritePtr(&t[0]);
			return true;
		}
		return false;
	}
};

//template<typename T, size_t N> struct ISO_browser::s_setget<T[N]> {
//	static bool	set(void *data, const ISO_type *type, T (&t)[N]);
//	static T	get(void *data, const ISO_type *type, T (&t)[N]);
//};
//-----------------------------------------------------------------------------
//	meta
//-----------------------------------------------------------------------------

ISO_DEFSAME(bool, bool8);

template<> struct ISO_def<tag2> : TISO_virtual2<tag2> {
	static ISO_browser2	Deref(tag2 &a) {
		if (const tag &t = a.tag())
			return MakeBrowser(t);
		return MakeBrowser(a.crc32());
	}
};

template<> struct ISO_def<ISO_type> : TISO_virtual2<ISO_type> {
	static ISO_browser2	Deref(const ISO_type &a) {
		switch (a.Type()) {
			case ISO_INT: {
				ISO_type_int	&i = (ISO_type_int&)a;
				if (!(a.flags & ISO_type_int::ENUM))
					return MakeBrowser(i);
				switch (i.GetSize()) {
					case 1:		return MakeBrowser((TISO_type_enum<uint8>&)a);
					case 2:		return MakeBrowser((TISO_type_enum<uint16>&)a);
					default:	return MakeBrowser((TISO_type_enum<uint32>&)a);
					case 8:		return MakeBrowser((TISO_type_enum<uint64>&)a);
				}
			}
			case ISO_FLOAT:		return MakeBrowser((ISO_type_float&)a);
			case ISO_STRING:	return MakeBrowser((ISO_type_string&)a);
			case ISO_COMPOSITE:	return MakeBrowser((ISO_type_composite&)a);
			case ISO_ARRAY:		return MakeBrowser((ISO_type_array&)a);
			case ISO_OPENARRAY:	return MakeBrowser((ISO_type_openarray&)a);
			case ISO_REFERENCE:	return MakeBrowser((ISO_type_reference&)a);
			case ISO_VIRTUAL:	return MakeBrowser((ISO_virtual&)a);
			case ISO_USER:		return MakeBrowser((ISO_type_user&)a);
			case ISO_FUNCTION:	return MakeBrowser((ISO_type_function&)a);
			default:			return ISO_browser();
		}
	}
};

ISO_DEFUSERCOMP2(ISO_type_array, count, subtype);
ISO_DEFUSERCOMP1(ISO_type_openarray, subtype);
ISO_DEFUSERCOMP1(ISO_type_reference, subtype);
ISO_DEFUSERCOMP2(ISO_type_function, rettype, params);
ISO_DEFUSERCOMP(ISO_type_user,2)	{ISO_SETACCESSOR(0,ID);ISO_SETFIELD(1,subtype);} };
ISO_DEFUSERCOMP(ISO_type_int,2)		{ISO_SETACCESSOR(0,is_signed);ISO_SETACCESSOR(1,num_bits);} };
ISO_DEFUSERCOMP(ISO_type_float,3)	{ISO_SETACCESSOR(0,is_signed);ISO_SETACCESSOR(1,num_bits);ISO_SETACCESSOR(1,exponent_bits);} };
ISO_DEFUSERCOMP(ISO_type_string,1)	{ISO_SETACCESSOR(0,char_size);} };

template<typename T> struct ISO_def<TISO_type_enum<T> > : TISO_virtual2<TISO_type_enum<T> > {
	static uint32		Count(TISO_type_enum<T> &a)				{ return a.num_values();			}
	static ISO_browser	Index(TISO_type_enum<T> &a, int i)		{ return MakeBrowser(a[i].value);	}
	static tag2			GetName(TISO_type_enum<T> &a, int i)	{ return a[i].id.get_tag2(0);		}
};

ISO_DEFUSERCOMP2(ISO_element, type, offset);
template<> struct ISO_def<ISO_type_composite> : TISO_virtual2<ISO_type_composite> {
	static uint32		Count(ISO_type_composite &a)				{ return a.Count();			}
	static ISO_browser	Index(ISO_type_composite &a, int i)			{ return MakeBrowser(static_cast<CISO_type_composite&>(a)[i]);	}
	static tag2			GetName(ISO_type_composite &a, int i)		{ return static_cast<CISO_type_composite&>(a).GetID(i);		}
	static int			GetIndex(ISO_type_composite &a, const tag2 &id, int from)	{ return static_cast<CISO_type_composite&>(a).GetIndex(id, from);	}
};

template<> struct ISO_def<ISO_bitpacked> : TISO_virtual2<ISO_bitpacked> {
	static uint32		Count(ISO_bitpacked &a)					{ return a.count;				}
	static ISO_browser	Index(ISO_bitpacked &a, int i)			{ return MakeBrowser(a[i]);		}
	static tag2			GetName(ISO_bitpacked &a, int i)		{ return a[i].id.get_tag2(0);	}
};

template<> struct ISO_def<ISO_virtual> : ISO_type_composite {};

}//namespace iso

template<typename T> inline void *operator new(size_t size, iso::ISO_openarray<T> &a)	{ return a._Append(); }
template<typename T> inline void operator delete(void *p, iso::ISO_openarray<T> &a)		{}

#endif// ISO_H
