#include "iso.h"
#include "base/soft_float.h"
#include "base/lf_hash.h"
#include "thread.h"
#include "extra/random.h"

#include "allocators/tlsf.h"
#include "vm.h"

namespace iso {

ISO_browser_root	root;
ISO_ptr<void>		iso_nil;

const char *store_string(const char *id) {
	static atomic<hash_map<uint32, const char*> > h;

	if (!id)
		return id;

	uint32		c	= CRC32C(id);
	auto		s	= h[c];
	if (!s) {
		size_t	len	= strlen(id) + 1;
		void	*p	= ISO_allocate<32>::alloc(len);
		memcpy(p, id, len);
		s = (const char*)p;
	}
	return s;
}

#ifdef ISO_TEST
struct test_store_string {
	atomic<hash_map<uint32, const char*> > h;

	test_store_string() {
//		for (int i = 0; i < 8; i++) {
//			RunThread([this]() {
				rng<simple_random>	rand(Thread::current_id());
				char	test[16];
				for (int j = 0; j < 100; j++) {
					for (int i = 0; i < 15; i++)
						test[i] = rand.from('a', char('z' + 1));
					test[15] = 0;
					h[CRC32C(test)] = strdup(test);
				}
				
				rng<simple_random>	rand2(Thread::current_id());
				for (int j = 0; j < 100; j++) {
					for (int i = 0; i < 15; i++)
						test[i] = rand2.from('a', char('z' + 1));
					test[15] = 0;
					ISO_ASSERT(strcmp(h[CRC32C(test)], test) == 0);
				}
//			});
//		}
	}
} _test_store_string;
#endif

#ifdef PLAT_WII
void Init(crc32 *x, void *physram) {
	*(uint32*)x = *(char**)x ? CRC32(*(char**)x) : 0;
}
void DeInit(crc32 *x) {}
#else
template<> void Init<crc32>(crc32 *x, void *physram) {
#ifndef ISO_EDITOR
	iso_string8	&s = *(iso_string8*)x;
	*(uint32*)x = !s ? 0 : CRC32(s.begin());
#endif
}
template<> void DeInit<crc32>(crc32 *x) {}
#endif

static initialise init(ISO_getdef<crc32>());

//-----------------------------------------------------------------------------
//	types
//-----------------------------------------------------------------------------

singleton<UserTypeArray> _user_types;

UserTypeArray::~UserTypeArray() {
	for (iterator i = begin(), e = end(); i != e; ++i)
		if ((*i)->flags & ISO_type_user::FROMFILE)
			delete *i;
}

ISO_type_user *UserTypeArray::Find(const tag2 &id) {
	for (iterator i = begin(), e = end(); i != e; ++i)
		if ((*i)->ID() == id)
			return *i;
	return 0;
}
void UserTypeArray::Add(ISO_type_user *t) {
	if (Find(t->ID()))
		ISO_TRACEF("Type ") << t->ID() << " already exists\n";
	push_back(t);
}

ISO_TYPEDEF(uint8,		uint8);
ISO_TYPEDEF(int8,		int8);
ISO_TYPEDEF(xint8,		xint8);
ISO_TYPEDEF(uint16,		uint16);
ISO_TYPEDEF(int16,		int16);
ISO_TYPEDEF(uint32,		uint32);
ISO_TYPEDEF(int,		int);
ISO_TYPEDEF(xint32,		xint32);
ISO_TYPEDEF(uint64,		uint64);
ISO_TYPEDEF(int64,		int64);
ISO_TYPEDEF(xint64,		xint64);
ISO_TYPEDEF(anything,	anything);
ISO_TYPEDEF(unescaped,	unescaped<string>);

const ISO_type	*def_bool	= ISO_getdef<bool8>();
const ISO_type	*def_float	= ISO_getdef<float>();
const ISO_type	*def_double	= ISO_getdef<double>();

//-----------------------------------------------------------------------------
//	allocation
//-----------------------------------------------------------------------------

template<typename T> void *base_fixed<T>::base;
template<> void *base_fixed<void>::get_base() {
	static bool	init = !!(base = ISO_allocate<32>::alloc(1));
	return base;
}

template<typename T, int B> void *base_select<T,B>::base[(1<<B) + 1] = {base, 0};
template<> void **base_select<void,2>::get_base() {
	static bool	init = !!(base[1] = base_fixed<void>::get_base());
	return base;
}
template<> void base_select<void,2>::set(const void *p) {
	if (!p) {
		offset = 0;
		return;
	}
	void **base = get_base();
	int	i = 0;
	for (i = 0; base[i]; ++i) {
		intptr_t v = (char*)p - (char*)base[i];
		if (int32(v) == v) {
			offset = int32(v) | i;
			return;
		}
	}
	if (i < 4) {
		base[i] = (void*)p;
		offset = i;
		return;
	}
	ISO_ASSERT(0);
}

struct malloc_iso_allocator {
	void	*alloc(size_t size, size_t align)				{ return aligned_alloc(iso::align(size, (uint32)align), align); }
	void	*realloc(void *p, size_t size, size_t align)	{ return aligned_realloc(p, size, align); }
	bool	free(void *p)									{ aligned_free(p); return true; }
	bool	free(void *p, size_t size)						{ return free(p); }
};

struct tracking_iso_allocator {
	size_t	total;
	void	*first;

	struct header {
		size_t	size;
		void	*malloc;
		operator void*()	{ return this + 1; }
	};

	header&	get_header(void *p)								{ return ((header*)p)[-1]; }
	header&	get_header(void *p, size_t align)				{ return get_header(iso::align((char*)p + sizeof(header), (uint32)align)); }
	size_t	adjusted_size(size_t size, size_t align)		{ return sizeof(header) + align + size; }

	void*	alloc(size_t size, size_t align) {
		void	*m	= malloc(adjusted_size(size, align));
		if (!first)
			first = m;
		header	&h	= get_header(m, align);
		h.size		= size;
		h.malloc	= m;
		total		+= size;
		return h;
	}
	void*	realloc(void *p, size_t size, size_t align) {
		if (!p)
			return alloc(size, align);

		header		&h0		= get_header(p);
		size_t		size0	= h0.size;
		intptr_t	off0	= (char*)p - (char*)h0.malloc;

		void		*m		= iso::realloc(h0.malloc, adjusted_size(size, align));
		header		&h		= get_header(m, align);
		p					= h;

		if (intptr_t move = (char*)p - (char*)m - off0)
			memmove(p, (char*)p + move, size0);

		h.size		= size;
		h.malloc	= m;
		total		+= size - size0;
		return p;
	}
	bool	free(void *p) {
		if (p) {
			header	&h	= get_header(p);
			total		-= h.size;
			iso::free(h.malloc);
		}
		return true;
	}
	bool	free(void *p, size_t size) {
		return free(p);
	}
};

struct tlsf_iso_allocator {
	tlsf::heap	*pp;
	void*	alloc(size_t size, size_t align)			{ return tlsf::alloc(pp, size, align); }
	void*	realloc(void *p, size_t size, size_t align)	{ return tlsf::realloc(pp, p, size); }
	bool	free(void *p)								{ tlsf::free(pp, p); return true; }
	bool	free(void *p, size_t size)					{ return free(p); }
	size_t	get_size(void *p)							{ return tlsf::block_size(p); }

	tlsf_iso_allocator() {
		size_t	size	= 1024*1024*1024;
		void	*mem	= malloc(size);
		pp	= tlsf::create(mem, size);
	}
};

struct tlsfvm_iso_allocator {
	tlsf::heap	*pp;
	Mutex		mutex;

	void more(size_t size) {
		size_t	curr_size	= vmem(pp).size();
		size_t	extra_size	= clamp(size * 2, 1024 * 1024, (uint64(1) << 32) - curr_size);
		ISO_ASSERT(extra_size);
		void	*mem		= vmem::commit(pp, curr_size + extra_size);
		tlsf::add_region(pp, (uint8*)mem + curr_size, extra_size);
	}
	void*	alloc(size_t size, size_t align) {
		auto	w	= with(mutex);
		void	*p	= tlsf::alloc(pp, size, align);
		if (!p) {
			more(size);
			p = tlsf::alloc(pp, size, align);
		}
		return p;
	}
	void*	realloc(void *p, size_t size, size_t align)	{
		auto	w	= with(mutex);
		void	*p2	= tlsf::realloc(pp, p, size);
		if (!p2) {
			more(size);
			p2 = tlsf::realloc(pp, p, size);
		}
		return p2;
	}
	bool	free(void *p) {
		if (p < pp || p > (uint8*)pp + (1ull << 32))
			return false;
		with(mutex), tlsf::free(pp, p);
		return true;
	}
	bool	free(void *p, size_t size)	{
		return free(p);
	}
	size_t	get_size(void *p) {
		return tlsf::block_size(p);
	}
	tlsfvm_iso_allocator() {
#ifdef ISO_PTR64
		void	*base	= vmem::reserve(bit64(32));
#else
		void	*base	= vmem::reserve(bit(30));
#endif
		size_t	size	= bit64(20);
		void	*mem	= vmem::commit(base, size);
		pp	= tlsf::create(mem, size);
	}
};

template<typename A> struct tracking_allocator {
	A		a;
	size_t	total;

	void*	alloc(size_t size, size_t align)			{ total += size; return a.alloc(size, align); }
	void*	realloc(void *p, size_t size, size_t align)	{ total += size - a.get_size(p); return a.realloc(p, size, align); }
	bool	free(void *p)								{ total -= a.get_size(p); return a.free(p); }
	bool	free(void *p, size_t size)					{ total -= size; return a.free(p, size); }
	tracking_allocator() : total(0) {}
};

typedef tlsfvm_iso_allocator	default_iso_allocator32;
//typedef tracking_allocator<tlsfvm_iso_allocator>	default_iso_allocator32;
//typedef malloc_iso_allocator	default_iso_allocator32;
typedef malloc_iso_allocator	default_iso_allocator64;

template<> vallocator&	ISO_allocate<32>::allocator() {
	static default_iso_allocator32	_default_iso_allocator;
	static vallocator	t(&_default_iso_allocator);
	return t;
}

template<> vallocator&	ISO_allocate<64>::allocator() {
	static default_iso_allocator64	_default_iso_allocator;
	static vallocator	t(&_default_iso_allocator);
	return t;
}

ISO_bin_allocator&	iso_bin_allocator() {
	static ISO_bin_allocator t;
	return t;
};

flags<ISO_allocate_base::FLAGS>	ISO_allocate_base::flags;

vallocator&	allocator32() { return ISO_allocate<32>::allocator(); }

//-----------------------------------------------------------------------------
//	tag2
//-----------------------------------------------------------------------------

size_t to_string(char *s, const tag2 &t) {
	if (!t)
		return to_string(s, "<no id>");
	if (tag p = t)
		return string_copy(s, (const char*)p);
	return _format(s, "crc_%08x", (uint32)t.crc32());
}
const char* to_string(const tag2 &t) {
	if (tag p = t)
		return (const char*)p;
	return t ? "<unknown>" : "<no id>";
}

//-----------------------------------------------------------------------------
//	ISO_type
//-----------------------------------------------------------------------------

string_accum& operator<<(string_accum &a, const ISO_type *const type) {
	if (type) switch (type->Type()) {
		case ISO_INT:
			a << "int";
			if (type != ISO_getdef<int>()) {
				ISO_type_int	*i = (ISO_type_int*)type;
				a << '{' << onlyif((i->flags & ISO_type_int::HEX) || !(i->flags & ISO_type_int::SIGN), i->flags & ISO_type_int::HEX ? 'x' : 'u') << i->num_bits();
				if (i->frac_bits())
					a << '.' << i->frac_bits();
				a << '}';
			}
			break;
		case ISO_FLOAT:
			a << "float";
			if (type != ISO_getdef<float>())
				a << '{' << onlyif(!(type->flags & ISO_type_float::SIGN), 'u') << type->param1 << '.' << type->param2 << '}';
			break;
		case ISO_STRING:
			a << "string";
			if (int	c = ((ISO_type_string*)type)->log2_char_size())
				a << (8 << c);
			return a;

		case ISO_VIRTUAL:	return a << "virtual";
		case ISO_COMPOSITE:	{
			CISO_type_composite	&comp	= *(CISO_type_composite*)type;
			a << '{';
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i)
				a << ' ' << get(i->type) << '.' << ifelse(i->id.blank(), '.', comp.GetID(i));
			return a << " }";
		}
		case ISO_ARRAY:		return a << get(((ISO_type_array*)type)->subtype) << '[' << ((ISO_type_array*)type)->count << ']';
		case ISO_OPENARRAY:	return a << get(((ISO_type_openarray*)type)->subtype) << "[]";
		case ISO_REFERENCE:	return a << get(((ISO_type_reference*)type)->subtype) << "*";
		case ISO_USER:		return a << get(((ISO_type_user*)type)->ID());
		default:			break;
	}
	return a;
}

const ISO_type *ISO_type::SubType(const ISO_type *type) {
	if (type) switch (type->Type()) {
		case ISO_STRING:	return type->flags & ISO_type_string::UTF16 ? ISO_getdef<char16>() : type->flags & ISO_type_string::UTF32 ? ISO_getdef<char32>() : ISO_getdef<char>();
		case ISO_ARRAY:		return ((ISO_type_array*)type)->subtype;
		case ISO_OPENARRAY:	return ((ISO_type_openarray*)type)->subtype;
		case ISO_REFERENCE:	return ((ISO_type_reference*)type)->subtype;
		case ISO_USER:		return ((ISO_type_user*)type)->subtype;
		default:			break;
	}
	return 0;
}

const ISO_type *ISO_type::SkipUser(const ISO_type *type) {
	while (type && type->Type() == ISO_USER)
		type = ((ISO_type_user*)type)->subtype;
	return type;
}

uint32 ISO_type::GetSize(const ISO_type *type) {
	if (type) switch (type->Type()) {
		case ISO_INT:		return ((ISO_type_int*)type)->GetSize();
		case ISO_FLOAT:		return ((ISO_type_float*)type)->GetSize();
		case ISO_STRING:	return ((ISO_type_string*)type)->GetSize();
		case ISO_COMPOSITE:	return ((CISO_type_composite*)type)->GetSize();
		case ISO_ARRAY:		return ((ISO_type_array*)type)->GetSize();
		case ISO_VIRTUAL:	return ((ISO_virtual*)type)->GetSize();
		case ISO_USER:		return GetSize(((ISO_type_user*)type)->subtype);
		case ISO_OPENARRAY:
		case ISO_REFERENCE:	return type->Is64Bit() ? 8 : 4;
		default:			break;
	}
	return 0;
}

uint32 ISO_type::GetAlignment(const ISO_type *type) {
	if (!type || (type->type & TYPE_PACKED))
		return 1;
	switch (type->Type()) {
		case ISO_INT:		return ((ISO_type_int*)type)->GetSize();
		case ISO_FLOAT:		return ((ISO_type_float*)type)->GetSize();
		case ISO_STRING:	return ((ISO_type_string*)type)->GetAlignment();
		case ISO_COMPOSITE:	return ((CISO_type_composite*)type)->GetAlignment();
		case ISO_ARRAY:		return GetAlignment(((ISO_type_array*)type)->subtype);
		case ISO_OPENARRAY:
		case ISO_REFERENCE:	return type->Is64Bit() ? 8 : 4;
		case ISO_USER:		return GetAlignment(((ISO_type_user*)type)->subtype);
		default:			return 1;
	}
}

bool ISO_type::Is(const ISO_type *type, const tag2 &id) {
	return type && (type->type & TYPE_MASK) == ISO_USER && ((ISO_type_user*)type)->ID() == id;
}

bool ISO_type::ContainsReferences(const ISO_type *type) {
	if (type) switch (type->Type()) {
		case ISO_COMPOSITE: {
			CISO_type_composite	&comp	= *(CISO_type_composite*)type;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i) {
				if (ContainsReferences(i->type))
					return true;
			}
			return false;
		}
		case ISO_ARRAY:		return ContainsReferences(((ISO_type_array*)type)->subtype);
		case ISO_OPENARRAY64:
		case ISO_OPENARRAY:	return ContainsReferences(((ISO_type_openarray*)type)->subtype);
		case ISO_USER:		return ContainsReferences(((ISO_type_user*)type)->subtype);
		case ISO_REFERENCE64:
		case ISO_REFERENCE:	return true;
		case ISO_VIRTUAL:	return true;
		default:			break;
	}
	return false;
}

bool ISO_type::IsPlainData(const ISO_type *type, bool flip_endian) {
	if (type) switch (type->Type()) {
		case ISO_INT:		return !flip_endian || GetSize(type) == 1 || (type->flags & ISO_type_int::NOSWAP);
		case ISO_FLOAT:		return !flip_endian || GetSize(type) == 1;
		case ISO_COMPOSITE:	{
			CISO_type_composite	&comp = *(CISO_type_composite*)type;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i) {
				if (!IsPlainData(i->type, flip_endian))
					return false;
			}
			return true;
		}
		case ISO_ARRAY:		return IsPlainData(((ISO_type_array*)type)->subtype, flip_endian);
		case ISO_USER:		return !(type->flags & (ISO_type_user::CHANGE | ISO_type_user::INITCALLBACK)) && IsPlainData(((ISO_type_user*)type)->subtype, flip_endian);
		default:			break;
	}
	return false;
}

bool IsSubclass(const ISO_type *type1, const ISO_type *type2, int criteria) {
	if (GetType(type1) != ISO_COMPOSITE)
		return false;
	const CISO_type_composite *comp = (const CISO_type_composite*)type1;
	return comp->Count() > 0 && (*comp)[0].id == 0 && ISO_type::Same((*comp)[0].type, type2, criteria);
}

bool ISO_type::Same(const ISO_type *type1, const ISO_type *type2, int criteria) {
	if (type1 == type2)
		return true;
	if (!type1 || !type2) {
		return (criteria & ISOMATCH_MATCHNULLS)
			|| ((criteria & ISOMATCH_MATCHNULL_RHS) && !type2);
	}

	if ((type1->type ^ type2->type) & (criteria & ISOMATCH_IGNORE_SIZE ? TYPE_MASK : TYPE_MASKEX)) {
/*		if (type2->Type() == ISO_COMPOSITE) {
			CISO_type_composite	*comp2 = (CISO_type_composite*)type2;
			if (comp2->Count() && !(*comp2)[0].id && (*comp2)[0].offset == 0 && Same(type1, (*comp2)[0].type, criteria))
				return true;
		}*/
		return	!(criteria & ISOMATCH_NOUSERRECURSE) && (
				type1->Type()	== ISO_USER	? Same(((ISO_type_user*)type1)->subtype, type2, criteria)
			:	type2->Type()	== ISO_USER	&& !(criteria & ISOMATCH_NOUSERRECURSE_RHS) ? Same(type1, ((ISO_type_user*)type2)->subtype, criteria)
			:	false
		);
	}
	switch (type1->Type()) {
		case ISO_INT:
			return type1->param1 == type2->param1
				&& type1->param2 == type2->param2
				&& !((type1->flags ^ type2->flags) & (ISO_type_int::SIGN | ISO_type_int::NOSWAP | ISO_type_int::FRAC_ADJUST));

		case ISO_FLOAT:
			return type1->param1 == type2->param1
				&& type1->param2 == type2->param2
				&& !((type1->flags ^ type2->flags) & ISO_type_float::SIGN);

		case ISO_STRING:
			return !((type1->flags ^ type2->flags) & (ISO_type_string::UTF16 | ISO_type_string::UTF32));

		case ISO_COMPOSITE: {
			CISO_type_composite	&comp1 = *(CISO_type_composite*)type1;
			CISO_type_composite	&comp2 = *(CISO_type_composite*)type2;
			if (comp1.Count() != comp2.Count() || comp1.GetSize() != comp2.GetSize())
				return false;
			for (int i = 0; i < comp1.Count(); i++) {
				if (!Same(comp1[i].type, comp2[i].type, criteria))
					return false;
			}
			return true;
		}
		case ISO_ARRAY:
			return (((ISO_type_array*)type2)->count == 0 || ((ISO_type_array*)type1)->count == ((ISO_type_array*)type2)->count)
				&& Same(((ISO_type_array*)type1)->subtype, ((ISO_type_array*)type2)->subtype, criteria);

		case ISO_OPENARRAY:
			return Same(((ISO_type_openarray*)type1)->subtype, ((ISO_type_openarray*)type2)->subtype, criteria);

		case ISO_REFERENCE:
			return Same(((ISO_type_reference*)type1)->subtype, ((ISO_type_reference*)type2)->subtype, criteria);

		case ISO_USER: {
			if (IsSubclass(SkipUser(type1), type2, criteria & ~ISOMATCH_MATCHNULLS))
				return true;
			return !(criteria & (ISOMATCH_NOUSERRECURSE | ISOMATCH_NOUSERRECURSE_BOTH | ISOMATCH_NOUSERRECURSE_RHS))
				&& Same(type1, ((ISO_type_user*)type2)->subtype, criteria);
		}
		case ISO_FUNCTION:
		case ISO_VIRTUAL:
			return false;

		default:
			return true;
	}
}


const ISO_type *Canonical(const ISO_type_int *type) {
	if (type->frac_bits() || (type->flags & (ISO_type_int::NOSWAP | ISO_type_int::FRAC_ADJUST)))
		return 0;
	bool	sign = type->is_signed();
	switch (type->num_bits()) {
		case 8:		return sign ? ISO_getdef<int8>()	: ISO_getdef<uint8>();
		case 16:	return sign ? ISO_getdef<int16>()	: ISO_getdef<uint16>();
		case 32:	return sign ? ISO_getdef<int32>()	: ISO_getdef<uint32>();
		case 64:	return sign ? ISO_getdef<int64>()	: ISO_getdef<uint64>();
		default:	return 0;
	}
}

const ISO_type *Canonical(const ISO_type_float *type) {
	if (type->is_signed()) {
		if (type->num_bits() == 32) {
			if (type->exponent_bits() == 8)
				return ISO_getdef<float>();
		} else if (type->num_bits() == 64) {
			if (type->exponent_bits() == 11)
				return ISO_getdef<double>();
		}
	}
	return 0;
}

const ISO_type *Canonical(const ISO_type *type) {
	if (type) switch (type->Type()) {
		case ISO_INT:	return Canonical((const ISO_type_int*)type);
		case ISO_FLOAT: return Canonical((const ISO_type_float*)type);
		default: break;
	}
	return 0;
};

const ISO_type *Duplicate(const ISO_type *type) {
	if (type) switch (ISO_TYPE t = type->TypeEx()) {
		case ISO_INT: {
			const ISO_type *canon = Canonical((const ISO_type_int*)type);
			return canon ? canon : new ISO_type_int(*(ISO_type_int*)type);
		}
		case ISO_FLOAT: {
			const ISO_type *canon = Canonical((const ISO_type_float*)type);
			return canon ? canon : new ISO_type_float(*(ISO_type_float*)type);
		}
		case ISO_COMPOSITE: {
			CISO_type_composite*comp	= (CISO_type_composite*)type;
			uint32				count	= comp->Count();
			CISO_type_composite*comp2	= new(count) CISO_type_composite(count);
			comp2->flags				= comp->flags;
			for (int i = 0; i < count; i++) {
				ISO_element		&e	= (*comp2)[i] = (*comp)[i];
				e.type	= Duplicate(e.type);
			}
			return comp2;
		}
		case ISO_ARRAY: {
			ISO_type_array	*array	= (ISO_type_array*)type;
			return new ISO_type_array(Duplicate(array->subtype), array->count);
		}

		case ISO_OPENARRAY:
		case ISO_OPENARRAY64:
			return new _ISO_type_openarray(t, Duplicate(((ISO_type_openarray*)type)->subtype));

		case ISO_REFERENCE:
		case ISO_REFERENCE64:
			return new _ISO_type_reference(t, Duplicate(((ISO_type_reference*)type)->subtype));

		case ISO_USER:
			if (type->flags & ISO_type_user::CRCID)
				return Duplicate(((ISO_type_user*)type)->subtype);
	}
	return type;
}

//-----------------------------------------------------------------------------
//	class CISO_type_composite
//-----------------------------------------------------------------------------

uint32 CISO_type_composite::CalcAlignment() const {
	uint32	a = 1;
	for (uint32 i = 0; i < count; i++)
		a = align(a, array(i).type->GetAlignment());
	return a;
}

int CISO_type_composite::GetIndex(const tag2 &id, int from) const {
	if (flags & CRCIDS) {
		crc32	c = id;
		for (uint32 i = from; i < count; i++)
			if (c == (crc32&)array(i).id)
				return i;
	} else if (tag s = id) {
		for (uint32 i = from; i < count; i++)
			if (s == array(i).id.get_tag())
				return i;
	} else {
		crc32	c = id;
		for (uint32 i = from; i < count; i++)
			if (c == array(i).id.get_crc(false))
				return i;
	}
	return -1;
}

uint32 CISO_type_composite::Add(const ISO_type* type, tag id, bool packed) {
	if (!type || (type->type & TYPE_DODGY))
		this->type |= TYPE_DODGY;

	ISO_element	&e	= array(count);
	size_t		off	= 0;
	if (count) {
		off = (&e)[-1].end();
		if (!packed)
			off = align(off, type->GetAlignment());
	}

	e.set(id, type, off);
	count++;
	return uint32(off);
}

CISO_type_composite *CISO_type_composite::Duplicate(void *defaults) const {
	int		n		= Count();
	uint32	size	= defaults ? GetSize() : 0;
	CISO_type_composite	*c = new(n, size) CISO_type_composite(n);
	memcpy(&c->array(0), &array(0), n * sizeof(ISO_element));

	if (defaults) {
		c->flags |= DEFAULT;
		memcpy(&c->array(n), defaults, size);
	}
	return c;
}

//-----------------------------------------------------------------------------
//	class TISO_type_enum<T>
//-----------------------------------------------------------------------------

template<typename T> uint32 TISO_type_enum<T>::num_values() const {
	uint32		n	= 0;
	for (uint32 i = 0; !(*this)[i].id.blank(); i++)
		n++;
	return n;
}

template<typename T> uint32 TISO_type_enum<T>::factors(T f[], uint32 num) const {
	uint32		nc	= 0;
	uint32		nf	= 0;
	T			pv	= 0;

	for (uint32 i = 0; !(*this)[i].id.blank(); i++) {
		if (T v = (*this)[i].value) {
			f[nc++] = v;
			if (nc == num)
				return ~0u;
		}
	}

	for (bool swapped = true; swapped;) {
		swapped = false;
		for (uint32 i = 1; i < nc; i++) {
			if (f[i - 1] > f[i]) {
				swap(f[i - 1], f[i]);
				swapped = true;
			}
		}
	}

	for (uint32 i = 0; i < nc; i++) {
		T	v = f[i], v0 = v;

//		if (i == nc - 1 && v < pv * 2)
		if (i == nc - 1 && v == pv + 1)
			break;

		for (uint32 j = i + 1; j < nc; j++)
			v = gcd(v, f[j]);

		if (nf == 0 || (v > pv && v > f[nf - 1]))
			f[nf++] = v;
		pv	= v0;
	}
	f[nf] = pv * 2;
	return nf;
}

template<typename T> const TISO_enum<T> *TISO_type_enum<T>::biggest_factor(T x) const {
	if (flags & DISCRETE) {
		for (auto *i = this->begin(); !i->id.blank(); i++) {
			if (i->value == x)
				return i;
		}
		return 0;
	}
	const TISO_enum<T> *best	= 0;
	for (auto *i = this->begin(); !i->id.blank(); i++) {
		if (i->value <= x && (!best || i->value >= best->value))
			best	= i;
	}
	if (!best || (best->value == 0 && x != 0))
		return 0;
	return best;
}
template uint32 TISO_type_enum<uint32>::num_values() const;
template uint32 TISO_type_enum<uint32>::factors(uint32 f[], uint32 num) const;
template const TISO_enum<uint32> *TISO_type_enum<uint32>::biggest_factor(uint32 x) const;

template uint32 TISO_type_enum<uint64>::num_values() const;
template uint32 TISO_type_enum<uint64>::factors(uint64 f[], uint32 num) const;
template const TISO_enum<uint64> *TISO_type_enum<uint64>::biggest_factor(uint64 x) const;

template uint32 TISO_type_enum<uint8>::num_values() const;
template uint32 TISO_type_enum<uint16>::num_values() const;

//-----------------------------------------------------------------------------
//	class CISO_type_float
//-----------------------------------------------------------------------------

float CISO_type_float::get(void *data) const {
	return	this == ISO_getdef<float>()		?	*(float*)data
	:		this == ISO_getdef<double>()	?	*(double*)data
	:		read_float<float>(data, param1, param2, is_signed());
}

double CISO_type_float::get64(void *data) const {
	return	this == ISO_getdef<float>()		?	*(float*)data
	:		this == ISO_getdef<double>()	?	*(double*)data
	:		read_float<double>(data, param1, param2, is_signed());
}

void CISO_type_float::set(void *data, uint64 m, int e, bool s) const {
	bool	sign	= is_signed();
	int		nbits	= num_bits(), exp = exponent_bits(), mant = nbits - exp - int(sign);
	e += bit(exp - 1);
	if (e >= bit(exp)) {
		e = bits(exp);
		m = bits(mant);
	}
	uint64	i = e < 0 || m == 0 ? 0
		: ((((m << 1) >> (63 - mant)) + 1) >> 1)
		| (uint64(e | ((s & sign) << exp)) << mant);
	write_bytes(data, i, (nbits + 7) / 8);
}

void CISO_type_float::set(void *data, float f) const {
	if (this == ISO_getdef<float>())
		*(float*)data = f;
	else if (this == ISO_getdef<double>())
		*(double*)data = f;
	else {
		float_components<float> c;
		set(data, c.m, c.e, !!c.s);
	}
}

void CISO_type_float::set(void *data, double f) const {
	if (this == ISO_getdef<float>())
		*(float*)data = f;
	else if (this == ISO_getdef<double>())
		*(double*)data = f;
	else {
		float_components<double> c;
		set(data, c.m, c.e, !!c.s);
	}
}

//-----------------------------------------------------------------------------
//	ISO_type_string
//-----------------------------------------------------------------------------

uint32 ISO_type_string::len(const void *s) const {
	switch (log2_char_size()) {
		default:	return (uint32)string_len((const char*)s);
		case 1:		return (uint32)string_len((const char16*)s);
		case 2:		return (uint32)string_len((const char32*)s);
		case 3:		return (uint32)string_len((uint64*)s);
	}
}

void *ISO_type_string::dup(const void *s) const {
	uint32	size = (len(s) + 1) << log2_char_size();
	void	*t	= Is64Bit() ? ISO_allocate<64>::alloc(size) : ISO_allocate<32>::alloc(size);
	memcpy(t, s, size);
	return t;
}

//-----------------------------------------------------------------------------
//	openarrays
//-----------------------------------------------------------------------------

template<int B> ISO_openarray_header *ISO_openarray_allocate<B>::Create(ISO_openarray_header *h, uint32 subsize, uint32 align, uint32 count) {
	Destroy(h, align);
	return count ? Create(subsize, align, count, count) : 0;
}
template<int B> ISO_openarray_header *ISO_openarray_allocate<B>::Create(ISO_openarray_header *h, const ISO_type *type, uint32 count) {
	uint32	align	= type->GetAlignment();
	h				= Create(h, type->GetSize(), align, count);
	if (type->SkipUser()->Type() == ISO_REFERENCE)
		memset(_Data(h), 0, count * 4);
	return h;
}
template<int B> ISO_openarray_header *ISO_openarray_allocate<B>::Resize(ISO_openarray_header *h, uint32 subsize, uint32 align, uint32 count, bool clear) {
	if (count == 0) {
		Destroy(h, align);
		return 0;
	}
	uint32	count0	= h ? h->count : 0;

	if (h && h->bin) {
		if (count > count0) {	//error!
			ISO_openarray_header	*h0 = h;
			h = Create(h, subsize, align, count);
			memcpy(_Data(h), _Data(h0), count0 * subsize);
		} else {
			h->count = count;
		}
	} else {
		uint32	max		= h ? h->max : 0;
		if (count <= max) {
			h->count = count;
		} else {
			max	= iso::max(max < 8 ? 16 : max * 2, count);
			h = Resize(h, subsize, align, max, count);
		}
	}

	if (clear && count > count0)
		memset(h->GetElement(count0, subsize), 0, (count - count0) * subsize);
	return h;
}

template<int B> ISO_openarray_header *ISO_openarray_allocate<B>::Append(ISO_openarray_header *h, uint32 subsize, uint32 align, bool clear) {
	return Resize(h, subsize, align, _Count(h) + 1, clear);
}
template<int B> ISO_openarray_header *ISO_openarray_allocate<B>::Insert(ISO_openarray_header *h, uint32 subsize, uint32 align, int i, bool clear) {
	uint32	n	= _Count(h);
	h = Resize(h, subsize, align, n + 1, false);
	void	*d	= h->GetElement(i, subsize);
	memmove((uint8*)d + subsize, d, (n - i) * subsize);
	if (clear)
		memset(d, 0, subsize);
	return h;
}

template struct ISO_openarray_allocate<32>;
template struct ISO_openarray_allocate<64>;

bool ISO_openarray_header::Remove(uint32 subsize, uint32 align, int i) {
	if (i < count) {
		--count;
		void	*d	= GetElement(i, subsize);
		memcpy(d, (uint8*)d + subsize, (count - i) * subsize);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	FindByType
//-----------------------------------------------------------------------------

ISO_ptr<void> &FindByType(ISO_ptr<void> *i, ISO_ptr<void> *e, const ISO_type *type, int crit) {
	for (; i != e; ++i) {
		if (i->IsType(type, crit))
			return *i;
	}
	return iso_nil;
}
ISO_ptr<void> &FindByType(ISO_ptr<void> *i, ISO_ptr<void> *e, tag2 id) {
	for (; i != e; ++i) {
		if (i->IsType(id))
			return *i;
	}
	return iso_nil;
}

template<> ISO_ptr<void> &anything::FindByType(const ISO_type *type, int crit) {
	return iso::FindByType(begin(), end(), type, crit);
}

template<> ISO_ptr<void> &anything::FindByType(tag2 id) {
	return iso::FindByType(begin(), end(), id);
}

//-----------------------------------------------------------------------------
//	_MakePtr / _MakePtrExternal
//-----------------------------------------------------------------------------

template<int B> void *_MakePtr(const ISO_type *type, tag2 id, uint32 size) {
	ISO_value	*v		= new(ISO_allocate<B>::alloc(sizeof(ISO_value) + size)) ISO_value(id, type);
	memset(v + 1, 0, size);
	return v + 1;
}

template void *_MakePtr<32>(const ISO_type *type, tag2 id, uint32 size);
template void *_MakePtr<64>(const ISO_type *type, tag2 id, uint32 size);

template<int B> void *_MakePtrExternal(const ISO_type *type, const char *filename, tag2 id) {
	size_t		size	= strlen(filename) + 1;
	ISO_value	*v		= new(type && type->Is64Bit() ? ISO_allocate<B>::alloc(sizeof(ISO_value) + size) :  ISO_allocate<32>::alloc(sizeof(ISO_value) + size)) ISO_value(id, type);
	char		*p		= (char*)(v + 1);
	v->flags |= ISO_value::EXTERNAL;
	memcpy(p, filename, size);
	return p;
}

template void *_MakePtrExternal<32>(const ISO_type *type, const char *filename, tag2 id);
template void *_MakePtrExternal<64>(const ISO_type *type, const char *filename, tag2 id);

//-----------------------------------------------------------------------------
//	Delete
//-----------------------------------------------------------------------------

static int Delete(const ISO_type *type, void *data, uint32 flags) {
	if (type) switch (type->Type()) {
		case ISO_STRING:
			if (!(flags & ISO_value::FROMBIN)) {
				((ISO_type_string*)type)->free(data);
				return 1;
			}
			break;

		case ISO_COMPOSITE: {
			CISO_type_composite	&comp	= *(CISO_type_composite*)type;
			int					ret		= 0;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i)
				ret |= Delete(i->type, (char*)data + i->offset, flags);
			return ret;
		}
		case ISO_ARRAY: {
			ISO_type_array	*array	= (ISO_type_array*)type;
			size_t			stride	= array->subsize;
			int				ret		= 0;
			type	= array->subtype;
			for (int n = array->Count(); n--; data = (uint8*)data + stride) {
				ret |= Delete(type, data, flags);
				if (!(ret & 1))
					return 0;
			}
			return ret;
		}
		case ISO_OPENARRAY: {
			if (data = type->ReadPtr(data)) {
				ISO_openarray_header	*h			= ISO_openarray_header::Header(data);
				ISO_type_openarray		*array		= (ISO_type_openarray*)type;
				uint32					subsize		= array->subsize;
				const ISO_type			*subtype	= array->subtype;
				int						ret			= 0;
				for (int n = _Count(h); n--; data = (uint8*)data + subsize) {
					ret |= Delete(subtype, data, flags);
					if (!(ret & 1))
						break;
				}
				if (!(flags & ISO_value::FROMBIN)) {
					if (type->Is64Bit())
						ISO_openarray_allocate<64>::Destroy(h, array->SubAlignment());
					else
						ISO_openarray_allocate<32>::Destroy(h, array->SubAlignment());
				}
				return ret;
			}
			return 1;
		}
		case ISO_REFERENCE:
			if (data = type->ReadPtr(data)) {
				ISO_value	*v = GetHeader(data);
				if (v->refs != 0xffff) {
					uint32	pflags = v->Flags();
					if (!(pflags & ISO_value::FROMBIN)
					|| (!ISO_allocate_base::flags.test(ISO_allocate_base::TOOL_DELETE)
						&& ((pflags ^ flags) & (ISO_value::FROMBIN | ISO_value::EXTREF))
					)) {
						v->release();
						return 1;
					}
					if (v->refs == 0) {
						v->refs--;
						if (v->Delete())
							return 1;
					}
					return 3;
				}
			}
			return 1;

		case ISO_USER: {
			if (type->flags & ISO_type_user::INITCALLBACK) {
				const ISO_type	*cb = type->flags & ISO_type_user::LOCALSUBTYPE ? user_types.Find(((ISO_type_user*)type)->ID()) : type;
				((CISO_type_callback*)cb)->deinit(data);
			}
			return Delete(((ISO_type_user*)type)->subtype, data, flags);
		}
		case ISO_VIRTUAL:
			if (!type->Fixed())
				((ISO_virtual*)type)->Delete((ISO_virtual*)type, data);
			return 1;

		default:
			break;
	};
	return 0;
}

bool ISO_value::Delete() {
//	ISO_TRACE("deleting 0x%08x: ", this) << leftjustify(type, 16) << ID() << '\n';
	bool	weak	= !!(flags & WEAKREFS);
	int		ret		= 0;

	if ((flags & REDIRECT) && !(flags & FROMBIN)) {
		((ISO_ptr<void>*)(this + 1))->Clear();
	} else if (!(flags & (EXTERNAL | CRCTYPE))) {
		ret = iso::Delete(type, this + 1, flags & REDIRECT ? (flags & ~FROMBIN) : flags);
	}

	if (flags & FROMBIN) {
		if (ret & 2) {
			refs = 0;
			return false;
		}
		if (flags & ROOT) {
			if (user)
				iso_bin_allocator().free(iso_bin_allocator().unfix(user));
			if (!ISO_allocate<32>::free(this))
				ISO_allocate<64>::free(this);
		}
	} else {
		if (!ISO_allocate<32>::free(this))
			ISO_allocate<64>::free(this);
	}

	if (weak)
		ISO_weak::remove(this);
	return true;
}

//-----------------------------------------------------------------------------
//	Compare
//-----------------------------------------------------------------------------

static bool NeedsFullCompare(const ISO_type *type, int flags) {
	if (!type)
		return false;

	switch (type->Type()) {
		case ISO_STRING:
			return true;
		case ISO_COMPOSITE:	{
			CISO_type_composite	&comp = *(CISO_type_composite*)type;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i) {
				if (NeedsFullCompare(i->type, flags))
					return true;
			}
			return false;
		}
		case ISO_OPENARRAY:	return true;
		case ISO_ARRAY:		return NeedsFullCompare(((ISO_type_array*)type)->subtype, flags);
		case ISO_USER:		return NeedsFullCompare(((ISO_type_user*)type)->subtype, flags);
		case ISO_REFERENCE:	return !!(flags & DUPF_DEEP);
		default:			return false;
	}
}

static bool CompareDataFull(const ISO_type *type, const void *data1, const void *data2, int flags) {
	switch (type->Type()) {
		case ISO_STRING:
			return str(((ISO_type_string*)type)->get(data1)) == ((ISO_type_string*)type)->get(data2);

		case ISO_COMPOSITE:	{
			CISO_type_composite	&comp = *(CISO_type_composite*)type;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i) {
				if (!CompareData(i->type, (char*)data1 + i->offset, (char*)data2 + i->offset, flags))
					return false;
			}
			return true;
		}
		case ISO_OPENARRAY:
			data1 = type->ReadPtr(data1);
			data2 = type->ReadPtr(data2);
			if (!data1 && !data2)
				return true;
			if (!data1 || !data2)
				return false;
			{
				const ISO_openarray_header *h1	= ISO_openarray_header::Header(data1);
				const ISO_openarray_header *h2	= ISO_openarray_header::Header(data2);
				int	c = _Count(h1);
				if (_Count(h2) != c)
					return false;

				ISO_type_openarray	*array	= (ISO_type_openarray*)type;
				size_t				stride	= array->subsize;
				type	= array->subtype;
				if (!NeedsFullCompare(type, flags))
					return memcmp(data1, data2, stride * c) == 0;

				while (c--) {
					if (!CompareDataFull(type, data1, data2, flags))
						return false;
					data1 = (uint8*)data1 + stride;
					data2 = (uint8*)data2 + stride;
				}
				return true;
			}

		case ISO_ARRAY: {
			ISO_type_array	*array	= (ISO_type_array*)type;
			size_t			stride	= array->subsize;
			type	= array->subtype;
			for (int n = array->Count(); n--; data1 = (uint8*)data1 + stride, data2 = (uint8*)data2 + stride) {
				if (!CompareDataFull(type, data1, data2, flags))
					return false;
			}
			return true;
		}
		case ISO_USER:
			return CompareDataFull(((ISO_type_user*)type)->subtype, data1, data2, flags);

		case ISO_REFERENCE:
			return (type->Is64Bit() ? (*(uint64*)data1 == *(uint64*)data2) : (*(uint32*)data1 == *(uint32*)data2))
				|| ((flags & DUPF_DEEP) && CompareData(GetPtr(type->ReadPtr(data1)), GetPtr(type->ReadPtr(data2)), flags));

		default:
			return false;
	}
}

bool CompareData(const ISO_type *type, const void *data1, const void *data2, int flags) {
	return data1 == data2 || (NeedsFullCompare(type, flags)
		? CompareDataFull(type, data1, data2, flags)
		: memcmp(data1, data2, type->GetSize()) == 0
	);
}

bool CompareData(ISO_ptr<void> p1, ISO_ptr<void> p2, int flags) {
	if (!p1)
		return !p2;

	if (!p2 || !p1.Type()->SameAs(p2.Type()))
		return false;

	if (p1.IsExternal() || p2.IsExternal())
		return p1.IsExternal() && p2.IsExternal() && istr((const char*)p1) == (const char*)p2;

	return CompareData(p1.Type(), p1, p2, flags);
}

//-----------------------------------------------------------------------------
//	Duplicate
//-----------------------------------------------------------------------------

int _Duplicate(const ISO_type *type, void *data, int flags, void *physical_ram) {

	if (!(flags & DUPF_NOINITS) && type->Type() == ISO_USER && (type->flags & ISO_type_user::INITCALLBACK)) {
		int	ret = _Duplicate(((ISO_type_user*)type)->subtype, data, flags, physical_ram);

		if (type->flags & ISO_type_user::LOCALSUBTYPE)
			type = user_types.Find(((ISO_type_user*)type)->ID());
		((CISO_type_callback*)type)->init((char*)data, physical_ram);
		return ret | 1;
	}

	type	= type->SkipUser();

	switch (type->Type()) {
		case ISO_STRING: {
			if (!(flags & DUPF_DUPSTRINGS) && !(type->flags & ISO_type_string::ALLOC))
				return 0;
			if (const char *p = ((ISO_type_string*)type)->get(data))
				type->WritePtr(data, ((ISO_type_string*)type)->dup(p));
			return 1;
		}

		case ISO_COMPOSITE: {
			CISO_type_composite	&comp	= *(CISO_type_composite*)type;
			int					ret		= 0;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i)
				ret |= _Duplicate(i->type, (uint8*)data + i->offset, flags, physical_ram);
			return ret;
		}
		case ISO_ARRAY: {
			ISO_type_array	*array	= (ISO_type_array*)type;
			size_t			stride	= array->subsize;
			int				ret		= 0;
			type	= array->subtype;
			for (int n = array->Count(); n--; data = (uint8*)data + stride) {
				if (!(ret |= _Duplicate(type, data, flags, physical_ram)))
					return 0;
			}
			return ret;
		}
		case ISO_OPENARRAY:
			if (void *p1 = type->ReadPtr(data)) {
				ISO_type_openarray	*array	= (ISO_type_openarray*)type;
				uint32				subsize	= array->subsize;

				ISO_openarray_header	*h1	= ISO_openarray_header::Header(p1);
				int						n	= _Count(h1);
				ISO_openarray_header	*h2	= type->Is64Bit()
					? ISO_openarray_allocate<64>::Create(subsize, array->SubAlignment(), n, n)
					: ISO_openarray_allocate<32>::Create(subsize, array->SubAlignment(), n, n);
				uint8					*p2	= _Data(h2);

				memcpy(p2, p1, subsize * n);
				type->WritePtr(data, p2);

				int	ret	= 0;
				type	= array->subtype;
				while (n--) {
					if (!(ret |= _Duplicate(type, p2, flags, physical_ram)))
						break;
					p2 += subsize;
				}
				return ret | 1;
			}
			return 1;

		case ISO_REFERENCE:
			if (data = type->ReadPtr(data)) {
				ISO_value	*v = GetHeader(data);
				uint32	pflags = v->Flags();
				v->addref();
				if (!(pflags & (ISO_value::EXTERNAL | ISO_value::CRCTYPE)) && (flags & DUPF_DEEP))
					Duplicate(v->ID(), v->Type(), data, (pflags & ISO_value::HASEXTERNAL) ? flags : (flags & ~DUPF_CHECKEXTERNALS));
				if ((flags & DUPF_CHECKEXTERNALS) && (pflags & (ISO_value::EXTERNAL | ISO_value::HASEXTERNAL)))
					return 3;
			}
			return 1;

		default:
			return 0;
	};
}

void *_Duplicate(tag2 id, const ISO_type *type, void *data, int flags, uint16 pflags) {
	if (!data)
		return ISO_NULL;

	void	*p = _MakePtr<32>(type, id);
	GetHeader(p)->flags |= pflags;

	memcpy(p, data, type->GetSize());
	int	ret = _Duplicate(type, p, flags);
	if (ret & 2)
		GetHeader(p)->flags |= ISO_value::HASEXTERNAL;
	return p;
}

void CopyData(const ISO_type *type, const void *srce, void *dest) {
	Delete(type, dest, 0);
	if (srce) {
		memcpy(dest, srce, type->GetSize());
		_Duplicate(type, dest);
	} else {
		memset(dest, 0, type->GetSize());
	}
}

//-----------------------------------------------------------------------------
//	CheckHasExternals
//-----------------------------------------------------------------------------
bool _CheckHasExternals(const ISO_ptr<void> &p, int flags, int depth);

bool _CheckHasExternals(const ISO_type *type, void *data, int flags, int depth) {
	type = type->SkipUser();

	bool	ret		= false;
	switch (type->Type()) {
		case ISO_COMPOSITE: {
			CISO_type_composite	&comp	= *(CISO_type_composite*)type;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i) {
				if (_CheckHasExternals(i->type, (uint8*)data + i->offset, flags, depth)) {
					ret = true;
					if (flags & DUPF_EARLYOUT)
						break;
				}
			}
			break;
		}
		case ISO_ARRAY: {
			ISO_type_array	&array	= *(ISO_type_array*)type;
			if (array.subtype->ContainsReferences()) {
				for (int i = 0, c = array.Count(); i < c; i++) {
					if (_CheckHasExternals(array.subtype, (char*)data + array.subsize * i, flags, depth)) {
						ret = true;
						if (flags & DUPF_EARLYOUT)
							break;
					}
				}
			}
			break;
		}
		case ISO_OPENARRAY: {
			data = type->ReadPtr(data);
			ISO_type_openarray	*array	= (ISO_type_openarray*)type;
			if (array->subtype->ContainsReferences()) {
				for (int n = _Count(ISO_openarray_header::Header(data)); n--; data = (uint8*)data + array->subsize) {
					if (_CheckHasExternals(array->subtype, data, flags, depth)) {
						ret = true;
						if (flags & DUPF_EARLYOUT)
							break;
					}
				}
			}
			break;
		}

		case ISO_REFERENCE:
			return _CheckHasExternals(GetPtr(type->ReadPtr(data)), flags, depth);

		default:
			break;
	};
	return ret;
}

bool _CheckHasExternals(const ISO_ptr<void> &p, int flags, int depth) {
	if (p) {
		if (p.IsExternal())
			return true;

		if (depth > 0) {
			if (flags & DUPF_DEEP) {
				if (!p.TestFlags(ISO_value::TEMP_USER)) {
					p.SetFlags(ISO_value::TEMP_USER);
					bool	has = _CheckHasExternals(p.Type(), p, flags, depth - 1);
					if (has)
						p.SetFlags(ISO_value::HASEXTERNAL);
					else
						p.ClearFlags(ISO_value::HASEXTERNAL);
					return has;
				}

			} else if (p.TestFlags(ISO_value::HASEXTERNAL)) {
				if (_CheckHasExternals(p.Type(), p, flags, depth))
					return true;
				p.ClearFlags(ISO_value::HASEXTERNAL);
			}
		}
	}
	return false;
}

bool CheckHasExternals(const ISO_ptr<void> &p, int flags, int depth) {
	if (flags & DUPF_DEEP) {
		bool ret = _CheckHasExternals(p, flags, depth);
		p.ClearFlags(ISO_value::TEMP_USER);
		ISO_browser(p).ClearTempFlags();
		return ret;
	}
	return _CheckHasExternals(p, flags, depth);
}

//-----------------------------------------------------------------------------
//	Endianness
//-----------------------------------------------------------------------------

//inplace
void FlipData(const ISO_type *type, void *data, bool big, bool flip) {
	type	= type->SkipUser();
	if (ISO_type::IsPlainData(type, flip))
		return;

	switch (type->Type()) {
		case ISO_INT:
		case ISO_FLOAT:
			switch (type->GetSize()) {
				case 2:	swap_endian_inplace(*(uint16*)data); break;
				case 4:	swap_endian_inplace(*(uint32*)data); break;
				case 8:	swap_endian_inplace(*(uint64*)data); break;
			}
			break;

		case ISO_STRING: {
			const ISO_type_string	*str = (ISO_type_string*)type;
			switch (str->char_size()) {
				case 2:
					for (uint16 *p = (uint16*)str->get(data); *p; ++p)
						swap_endian_inplace(*p);
					break;
				case 4:
					for (uint32 *p = (uint32*)str->get(data); *p; ++p)
						swap_endian_inplace(*p);
					break;
			}
			break;
		}

		case ISO_COMPOSITE: {
			const CISO_type_composite	&comp	= *(const CISO_type_composite*)type;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i)
				FlipData(i->type, (uint8*)data + i->offset, big, flip);
			break;
		}

		case ISO_ARRAY: {
			ISO_type_array	*array = (ISO_type_array*)type;
			for (int i = 0, c = array->Count(); i < c; i++)
				FlipData(array->subtype, (char*)data + array->subsize * i, big, flip);
			break;
		}

		case ISO_OPENARRAY: {
			data = type->ReadPtr(data);
			ISO_type_openarray	*array	= (ISO_type_openarray*)type;
			if (!ISO_type::IsPlainData(array->subtype, flip)) {
				for (int n = _Count(ISO_openarray_header::Header(data)); n--; data = (uint8*)data + array->subsize)
					FlipData(array->subtype, data, big, flip);
			}
			break;
		}

		case ISO_REFERENCE:
			if (data = type->ReadPtr(data)) {
				ISO_value *v = GetHeader(data);
				if (v->flags & (ISO_value::EXTERNAL | ISO_value::CRCTYPE))
					return;
				const ISO_type *type0 = ((ISO_type_reference*)type)->subtype;
				if (type0 && type0->SkipUser() != v->type->SkipUser())
					return;
				bool	flip = !!(v->flags & ISO_value::ISBIGENDIAN) != big;
				if (flip)
					v->flags	= v->flags ^ ISO_value::ISBIGENDIAN;
				FlipData(v->type, data, big, flip);
			}
			break;

		default:
			break;
	};
}

void SetBigEndian(ISO_ptr<void> p, bool big) {
	if (ISO_value *value = p.Header()) {
		if (value->flags & (ISO_value::EXTERNAL | ISO_value::CRCTYPE))
			return;
		bool	flip = !!(value->flags & ISO_value::ISBIGENDIAN) != big;
		if (flip)
			value->flags	= value->flags ^ ISO_value::ISBIGENDIAN;
		FlipData(value->type, p, big, flip);
	}
}

//srce -> dest
void FlipData(const ISO_type *type, const void *srce, void *dest) {
	type	= type->SkipUser();

	switch (type->Type()) {
		case ISO_INT:
		case ISO_FLOAT:
		case ISO_VIRTUAL:
			switch (uint32 size = type->GetSize()) {
				case 0:	break;
				case 1:	*(uint8*)dest	= *(uint8*)srce; break;
				case 2:	*(uint16*)dest	= swap_endian(*(uint16*)srce); break;
				case 4:	*(uint32*)dest	= swap_endian(*(uint32*)srce); break;
				case 8:	*(uint64*)dest	= swap_endian(*(uint64*)srce); break;
				default: memcpy(dest, srce, size); break;
			}
			break;

		case ISO_COMPOSITE: {
			const CISO_type_composite	&comp	= *(const CISO_type_composite*)type;
			for (CISO_type_composite::const_iterator i = comp.begin(), e = comp.end(); i != e; ++i)
				FlipData(i->type, (char*)srce + i->offset, (char*)dest + i->offset);
			break;
		}

		case ISO_ARRAY: {
			ISO_type_array		*array	= (ISO_type_array*)type;
			uint32				subsize	= array->subsize;
			if (subsize == 1) {
				memcpy(dest, srce, array->Count());
			} else {
				for (int n = array->Count(); n--; dest = (char*)dest + subsize, srce = (char*)srce + subsize)
					FlipData(array->subtype, srce, dest);
			}
			break;
		}

		default:
			break;
	};
}

//-----------------------------------------------------------------------------
//	class ISO_browser
//-----------------------------------------------------------------------------

const ISO_ptr<void> *_Find(tag2 id, const ISO_ptr<void> *p, uint32 n) {
	for (const ISO_ptr<void> *i = p, *e = p + n; i != e; ++i) {
		if (i->IsID(id))
			return i;
	}
	return 0;
}

int _GetIndex(tag2 id, const ISO_ptr<void> *p, uint32 n) {
	if (const ISO_ptr<void> *i = _Find(id, p, n))
		return int(i - p);
	return -1;
}

uint32	ISO_browser::GetSize() const {
	if (!type)
		return 0;
	uint32 size = type->GetSize();
	if (size == 0 && type->Type() == ISO_VIRTUAL && !type->Fixed()) {
		ISO_virtual	*virt = (ISO_virtual*)type;
		uint32		count = virt->Count(virt, data);
		if (count == 0 || ~count == 0)
			size = virt->Deref(virt, data).GetSize();
		else
			size = virt->Index(virt, data, 0).GetSize() * count;
	}
	return size;
}

void ISO_browser::UnsafeSet(const void *srce) const {
	memcpy(data, srce, type->GetSize());
}

uint32 ISO_browser::Count() const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_STRING: {
				const char	*p = ((ISO_type_string*)type)->get(data);
				return	uint32(
					type->flags & ISO_type_string::UTF16 ? string_len((const char16*)p)
				:	type->flags & ISO_type_string::UTF32 ? string_len((const char32*)p)
				:	string_len(p)
				);
			}
			case ISO_COMPOSITE:
				return ((ISO_type_composite*)type)->count;

			case ISO_ARRAY:
				return ((ISO_type_array*)type)->count;

			case ISO_OPENARRAY:
				return _Count(ISO_openarray_header::Header(type->ReadPtr(data)));

			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					uint32		count = virt->Count(virt, data);
					return count ? (~count == 0 ? 0 : count) : virt->Deref(virt, data).Count();
				}
			default:
				return 0;
		}
	}
	return 0;
}

int ISO_browser::GetIndex(tag2 id, int from) const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE:
				return ((CISO_type_composite*)type)->GetIndex(id, from);

			case ISO_ARRAY:
				if (((ISO_type_array*)type)->subtype->SkipUser()->Type() == ISO_REFERENCE) {
					int i = _GetIndex(id, (const ISO_ptr<void>*)data + from, ((ISO_type_array*)type)->Count() - from);
					if (i >= 0)
						return i + from;
				}
				return -1;

			case ISO_OPENARRAY:
				if (((ISO_type_openarray*)type)->subtype->SkipUser()->Type() == ISO_REFERENCE) {
					data = type->ReadPtr(data);
					int	i = _GetIndex(id, (const ISO_ptr<void>*)data + from, _Count(ISO_openarray_header::Header(data)) - from);
					if (i >= 0)
						return i + from;
				}
				return -1;

			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt	= (ISO_virtual*)type;
					int			i		= virt->GetIndex(virt, data, id, from);
					return i >= 0 ? i : virt->Deref(virt, data).GetIndex(id);
				}
			default:
				type = 0;
				break;
		}
	}
	return -1;
}

tag2 ISO_browser::GetName() const {
	const ISO_type *type = this->type->SkipUser();
	switch (type->Type()) {
		case ISO_REFERENCE:
			if (void *p = type->ReadPtr(data))
				return GetHeader(p)->ID();
			return tag2();

		case ISO_VIRTUAL:
			if (!type->Fixed()) {
				ISO_virtual	*virt	= (ISO_virtual*)type;
				return virt->Deref(virt, data).GetName();
			}
		default:
			return tag2();
	}
}

tag2 ISO_browser::GetName(int i) const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE:
				return ((CISO_type_composite*)type)->GetID(i);

			case ISO_ARRAY:
				if (GetType(((ISO_type_array*)type)->subtype->SkipUser()) == ISO_REFERENCE) {
					if (i < ((ISO_type_array*)type)->Count())
						return ((ISO_ptr<void>*)data)[i].ID();
				}
				return tag2();

			case ISO_OPENARRAY:
				if (GetType(((ISO_type_openarray*)type)->subtype->SkipUser()) == ISO_REFERENCE) {
					void *p = type->ReadPtr(data);
					if (i < _Count(ISO_openarray_header::Header(p)))
						return ((ISO_ptr<void>*)p)[i].ID();
				}
				return tag2();

			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt	= (ISO_virtual*)type;
					tag2		id		= virt->GetName(virt, data, i);
					return id ? id : virt->Deref(virt, data).GetName(i);
				}
			default:
				return tag2();
		}
	}
	return tag2();
}

const ISO_browser2 ISO_browser::operator*() const {
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_OPENARRAY:
				return ISO_browser(((ISO_type_openarray*)type)->subtype, type->ReadPtr(data));

			case ISO_REFERENCE:
				return GetPtr(type->ReadPtr(data));

			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed())
					return ((ISO_virtual*)type)->Deref((ISO_virtual*)type, data);

			default:
				type = 0;
				break;
		}
	}
	return BadBrowser();
}

const ISO_browser2 ISO_browser::FindByType(const ISO_type *t, int crit) const {
	if (GetType(type->SkipUser()) == ISO_REFERENCE)
		return (**this).FindByType(t, crit);

	for (uint32 i = 0, n = Count(); i < n; i++) {
		for (ISO_browser2 b = (*this)[i]; b; b = *b) {
			if (b.Is(t, crit))
				return b;
		}
	}
	return BadBrowser();
}

const ISO_browser2 ISO_browser::FindByType(tag2 id) const {
	if (GetType(type->SkipUser()) == ISO_REFERENCE)
		return (**this).FindByType(id);

	for (uint32 i = 0, n = Count(); i < n; i++) {
		ISO_browser2	b	= (*this)[i];
		if (b.Is(id))
			return b;
		while (b.Type() == ISO_REFERENCE) {
			b	= *b;
			if (b.Is(id))
				return b;
		}
	}
	return BadBrowser();
}

ISO_browser::iterator ISO_browser::begin() const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE: {
				CISO_type_composite	&comp	= *(CISO_type_composite*)type;
				return iterator(data, comp.begin());
			}
			case ISO_ARRAY: {
				ISO_type_array		&array	= *(ISO_type_array*)type;
				return iterator(array.subtype, data, array.subsize);
			}
			case ISO_OPENARRAY: {
				ISO_type_openarray	&array	= *(ISO_type_openarray*)type;
				return iterator(array.subtype, type->ReadPtr(data), array.subsize);
			}
			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt	= (ISO_virtual*)type;
					uint32		count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).begin() : ~count == 0 ? iterator() : iterator(virt, data, 0);
				}

			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

ISO_browser::iterator ISO_browser::end() const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE: {
				CISO_type_composite	&comp	= *(CISO_type_composite*)type;
				return iterator((uint8*)data + comp.GetSize(), comp.end());
			}
			case ISO_ARRAY: {
				ISO_type_array		&array	= *(ISO_type_array*)type;
				return iterator(array.subtype, (uint8*)data + array.subsize * array.Count(), array.subsize);
			}
			case ISO_OPENARRAY: {
				ISO_type_openarray	&array	= *(ISO_type_openarray*)type;
				data	= type->ReadPtr(data);
				return iterator(array.subtype, (uint8*)data + array.subsize * _Count(ISO_openarray_header::Header(data)), array.subsize);
			}
			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt	= (ISO_virtual*)type;
					uint32		count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).end() : ~count == 0 ? iterator() : iterator(virt, data, count);
				}

			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

bool ISO_browser::Resize(uint32 n) const {
	void	*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_OPENARRAY: {
				ISO_type_openarray	*array	= (ISO_type_openarray*)type;
				void				*p		= type->ReadPtr(data);
				ISO_openarray_header *h		= ISO_openarray_header::Header(p);
				uint32				i		= _Count(h);
				uint32				align	= 4;
				if (n < i && !ISO_type::IsPlainData(array->subtype)) {
					while (i-- > n)
						Delete(array->subtype, (uint8*)p + i * array->subsize, 0);
				}
				h = type->Is64Bit()
					? ISO_openarray_allocate<64>::Resize(h, array->subsize, align, n, true)
					: ISO_openarray_allocate<32>::Resize(h, array->subsize, align, n, true);

				type->WritePtr(data, _Data(h));
				return true;
			}
			case ISO_REFERENCE: {
				data	= type->ReadPtr(data);
				ISO_value	*value = GetHeader(data);
				type	= value->IsExternal() ? 0 : value->Type();
				break;
			}
			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					return virt->Count(virt, data) == 0 && virt->Deref(virt, data).Resize(n);
				}
			default:
				type = 0;
				break;
		}
	}
	return false;
}

bool ISO_browser::Remove(int i) const {
	void	*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_OPENARRAY:
				if (void *p = type->ReadPtr(data)) {
					ISO_type_openarray	*array	= (ISO_type_openarray*)type;
					ISO_openarray_header *h		= ISO_openarray_header::Header(p);
					uint32				align	= 4;
					if (!ISO_type::IsPlainData(array->subtype))
						Delete(array->subtype, (uint8*)p + i * array->subsize, 0);
					return h->Remove(array->subsize, align, i);
				}
				return false;

			case ISO_REFERENCE: {
				data	= type->ReadPtr(data);
				ISO_value	*value = GetHeader(data);
				type	= value->IsExternal() ? 0 : value->Type();
				break;
			}
			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					return virt->Count(virt, data) == 0 && virt->Deref(virt, data).Remove(i);
				}
			default:
				type = 0;
				break;
		}
	}
	return false;
}

const ISO_browser ISO_browser::Insert(int i) const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_OPENARRAY: {
				ISO_type_openarray	*array	= (ISO_type_openarray*)type;
				void				*p		= type->ReadPtr(data);
				ISO_openarray_header *h		= ISO_openarray_header::Header(p);
				uint32				align	= 4;

				h = type->Is64Bit()
					? ISO_openarray_allocate<64>::Insert(h, array->subsize, align, i, true)
					: ISO_openarray_allocate<32>::Insert(h, array->subsize, align, i, true);

				if (h) {
					p	= _Data(h);
					type->WritePtr(data, p);
					return ISO_browser(array->subtype, (uint8*)p + i * array->subsize);
				}
				type = 0;
				break;
			}
			case ISO_REFERENCE: {
				data	= type->ReadPtr(data);
				ISO_value	*value = GetHeader(data);
				type	= value->IsExternal() ? 0 : value->Type();
				break;
			}
			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					if (virt->Count(virt, data) == 0)
						return virt->Deref(virt, data).Insert(i);
				}
			default:
				type = 0;
				break;
		}
	}
	return BadBrowser();
}

const ISO_browser ISO_browser::Append() const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_OPENARRAY: {
				ISO_type_openarray	*array	= (ISO_type_openarray*)type;
				void				*p		= type->ReadPtr(data);
				ISO_openarray_header *h		= ISO_openarray_header::Header(p);
				uint32				align	= 4;

				h = type->Is64Bit()
					? ISO_openarray_allocate<64>::Append(h, array->subsize, align, true)
					: ISO_openarray_allocate<32>::Append(h, array->subsize, align, true);

				if (h) {
					p	= _Data(h);
					type->WritePtr(data, p);
					return ISO_browser(array->subtype, (uint8*)p + (_Count(h) - 1) * array->subsize);
				}
				type = 0;
				break;
			}
			case ISO_REFERENCE:
				if (void *data2 = type->ReadPtr(data)) {
					ISO_value	*v = GetHeader(data2);
					if (const ISO_type *type2 = v->IsExternal() ? 0 : v->Type()) {
						data	= data2;
						type	= type2;
						break;
					}
					ISO_ptr<anything>	p(v->ID(), 1);
					v->release();
					type->WritePtr(data, p);
					p.Header()->addref();
					return MakeBrowser((*p)[0]);
				}
				type	= 0;
				break;

			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					if (ISO_browser2 b = virt->Deref(virt, data))
						return b.Append();
				}
			default:
				type = 0;
				break;
		}
	}
	return BadBrowser();
}

int ISO_browser::ParseField(string_scan &s) const {
	s.get_token(char_set(";. \t/\\"));
	switch (s.peekc()) {
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			return s.get<uint32>();

		case '[': {
			int	i = s.move(1).get<uint32>();
			return s.getc() == ']' ? i : -1;
		}
		case '\'': {
			int i = GetIndex(s.move(1).get_token(~char_set('\'')));
			s.move(1);
			return i;
		}

		default:
			return GetIndex(s.get_token(~char_set("=.[/\\")));
	}
}

ISO_browser2 GetOrAddMember(const ISO_browser2 &b, tag2 id, bool create) {
	ISO_browser2 b2 = b[id];
	if (b2 || !create)
		return b2;

	if (b.IsPtr() || b.Type() == ISO_REFERENCE) {
		const ISO_type *type = (*b).GetTypeDef();
		if (type == 0) {
			ISO_ptr<anything>	p(b.GetName());
			p->Append(ISO_ptr<void>(id));

			b.Set(p);
			return b[id];
		}
	}

	b.SetMember(id, ISO_ptr<void>(id));
	return b[id];
}

const ISO_browser2 ISO_browser2::Parse(const char *spec, bool create) const {
	if (!spec)
		return *this;

	string_scan		s(spec);
	ISO_browser2	b = *this;

	while (b && s.peekc()) {
		s.get_token(char_set(";. \t/\\"));
		switch (s.peekc()) {
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				b = b[s.get<uint32>()];
				break;

			case '[': {
				int	i = s.move(1).get<uint32>();
				if (s.getc() != ']')
					return ISO_browser2();
				b = b[i];
				break;
			}
			case '\'':
				b = GetOrAddMember(b, s.move(1).get_token(~char_set('\'')), create);
				s.move(1);
				break;

			default:
				b = GetOrAddMember(b, s.get_token(~char_set("=.[/\\")), create);
				break;
		}
	}
	return b;
}

bool ISO_browser::Update(const char *s, bool from) const {
	void *data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_VIRTUAL:
				return !type->Fixed() && ((ISO_virtual*)type)->Update((ISO_virtual*)type, data, s, from);

			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			default:
				type = 0;
				break;
//				string_scan	ss(s);
//				int			i = ParseField(ss);
//				return i >= 0 && (*this)[i].Update(ss.getp(), from);
		}
	}
	return true;
}

#if 0
void ISO_browser::ClearTempFlags(ISO_browser b) {
	for (;;) {
		if (const ISO_type *type = b.GetTypeDef()->SkipUser()) {
			if (type->Type() == ISO_REFERENCE) {
				b = *b;
				ISO_ptr<void>	p = b;
				if (p && (p.Flags() & ISO_value::TEMP_USER)) {
					p.ClearFlags(ISO_value::TEMP_USER);
					continue;
				}
			} else if (type->ContainsReferences()) {
				for (ISO_browser::iterator i = b.begin(), e = b.end(); i != e; ++i)
					ClearTempFlags(i.Ref());
			}
		}
		return;
	}
}
#else

void ClearTempFlags(ISO_browser2 b, dynamic_array<ISO_ptr<void> > &stack) {
	if (const ISO_type *type = b.GetTypeDef()->SkipUser()) {
		if (type->Type() == ISO_REFERENCE) {
			b = *b;
			ISO_ptr<void>	p = b;
			if (p && (p.Flags() & ISO_value::TEMP_USER)) {
				p.ClearFlags(ISO_value::TEMP_USER);
				stack.push_back(p);
			}
		} else if (type->ContainsReferences()) {
			for (ISO_browser::iterator i = b.begin(), e = b.end(); i != e; ++i)
				ClearTempFlags(i.Ref(), stack);
		}
	}
}


void ISO_browser::ClearTempFlags(ISO_browser b) {
	dynamic_array<ISO_ptr<void> >	stack;
	for (;;) {
		iso::ClearTempFlags(b, stack);
		if (stack.empty())
			break;
		b = ISO_browser(stack.pop_back_value());
	}
}
#endif

void ISO_browser::Apply(ISO_browser2 b, const ISO_type *type, void f(void*)) {
	for (;;) {
		if (b.Type() == ISO_REFERENCE) {
			b = *b;
			ISO_ptr<void>	p = b;
			if (p && !(p.Flags() & ISO_value::TEMP_USER)) {
				p.SetFlags(ISO_value::TEMP_USER);
				if (!p.IsType(type))
					continue;
				f(p);
			}
		} else if (b.GetTypeDef()->ContainsReferences()) {
			for (ISO_browser2::iterator i = b.begin(), e = b.end(); i != e; ++i)
				Apply(i.Ref(), type, f);
		}
		return;
	}
}

void ISO_browser::Apply(ISO_browser2 b, const ISO_type *type, void f(void*, void*), void *param) {
	if (b.Is(type))
		f(b, param);

	if (b.SkipUser().Type() == ISO_REFERENCE) {
		if (!b.HasCRCType() && !b.External()) {
			ISO_browser2	b2	= *b;
			ISO_ptr<void>	p	= b2;
			if (p && !(p.Flags() & ISO_value::TEMP_USER)) {
				p.SetFlags(ISO_value::TEMP_USER);
				Apply(b2, type, f, param);
			}
		}
	} else if (b.GetTypeDef()->ContainsReferences()) {
		for (ISO_browser2::iterator i = b.begin(), e = b.end(); i != e; ++i)
			Apply(i.Ref(), type, f, param);
	}
}

//-----------------------------------------------------------------------------
//	class ISO_browser2
//-----------------------------------------------------------------------------

const ISO_browser2 WithPtr(const ISO_browser2 &b, const ISO_ptr<void> &p) {
	if (b && !b.GetPtr())
		return ISO_browser2(b, p);
	return b;
}

tag2 ISO_browser2::GetName(int i) const {
	void		*data = this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE:
				return ((CISO_type_composite*)type)->GetID(i);

			case ISO_ARRAY:
				if (GetType(((ISO_type_array*)type)->subtype->SkipUser()) == ISO_REFERENCE) {
					if (i < ((ISO_type_array*)type)->Count())
						return ((ISO_ptr<void>*)data)[i].ID();
				}
				return tag2();

			case ISO_OPENARRAY:
				if (GetType(((ISO_type_openarray*)type)->subtype->SkipUser()) == ISO_REFERENCE) {
					data = type->ReadPtr(data);
					if (i < _Count(ISO_openarray_header::Header(data)))
						return ((ISO_ptr<void>*)data)[i].ID();
				}
				return tag2();

			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_USER:
				type = ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt	= (ISO_virtual*)type;
					tag2		id		= virt->GetName(virt, data, i);
					return id ? id : virt->Deref(virt, data).GetName(i);
				}

			default:
				return tag2();
		}
	}
	return tag2();
}

const ISO_browser2 ISO_browser2::operator[](tag2 id) const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE: {
				CISO_type_composite &comp = *(CISO_type_composite*)type;
				int i = comp.GetIndex(id);
				if (i >= 0) {
					ISO_element	&e = comp[i];
					if (e.type && e.type->Type() == ISO_REFERENCE)
						return iso::GetPtr(e.type->ReadPtr((char*)data + comp[i].offset));
					return ISO_browser2(e.type, (char*)data + e.offset, p);
					//return ISO_browser2(comp[i].type, (char*)data + comp[i].offset, p);
				}
				return BadBrowser();
			}
			case ISO_ARRAY:
				if (GetType(((ISO_type_array*)type)->subtype->SkipUser()) == ISO_REFERENCE) {
					if (const ISO_ptr<void> *i = _Find(id, (const ISO_ptr<void>*)data, ((ISO_type_array*)type)->Count()))
						return *i;
				}
				type = 0;
				break;

			case ISO_OPENARRAY:
				if (GetType(((ISO_type_openarray*)type)->subtype->SkipUser()) == ISO_REFERENCE) {
					data = type->ReadPtr(data);
					if (const ISO_ptr<void> *i = _Find(id, (const ISO_ptr<void>*)data, _Count(ISO_openarray_header::Header(data)))) {
						if (i->Type())
							return *i;
						return MakeBrowser(*i);
					}
				}
				type = 0;
				break;

			case ISO_REFERENCE:
				p		= iso::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					int i = virt->GetIndex(virt, data, id, 0);
					if (i >= 0)
						return virt->Index(virt, data, i);
					return virt->Deref(virt, data)[id];
				}
			default:
				type = 0;
				break;
		}
	}
	return BadBrowser();
}

const ISO_browser2 ISO_browser2::operator[](int i) const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_STRING: {
				const char	*p = ((ISO_type_string*)type)->get(data);
				if (type->flags & ISO_type_string::UTF16) {
					const char16	*s = (const char16*)p;
					if (i < string_len(s))
						return MakeBrowser(s[i]);
				} else if (type->flags & ISO_type_string::UTF32) {
					const char32	*s = (const char32*)p;
					if (i < string_len(s))
						return MakeBrowser(s[i]);
				} else {
					if (i < string_len(p))
						return MakeBrowser(p[i]);
				}
				type = 0;
				break;
			}
			case ISO_COMPOSITE: {
				CISO_type_composite	&comp	= *(CISO_type_composite*)type;
				if (i < comp.Count())
					return ISO_browser2(comp[i].type, (char*)data + comp[i].offset, p);
				type = 0;
				break;
			}
			case ISO_ARRAY: {
				ISO_type_array		&array	= *(ISO_type_array*)type;
				if (i < array.Count())
					return ISO_browser2(array.subtype, (char*)data + array.subsize * i, p);
				type = 0;
				break;
			}
			case ISO_OPENARRAY:
				data = type->ReadPtr(data);
				if (i < _Count(ISO_openarray_header::Header(data))) {
					ISO_type_openarray	&array	= *(ISO_type_openarray*)type;
					return ISO_browser2(array.subtype, (uint8*)data + i * array.subsize, p);
				}
				type = 0;
				break;

			case ISO_REFERENCE:
				p		= iso::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt = (ISO_virtual*)type;
					if (ISO_browser2 b = virt->Index(virt, data, i))
						return b;
					return WithPtr(virt->Deref(virt, data)[i], p);
				}
			default:
				type = 0;
				break;
		}
	}
	return BadBrowser();
}

const ISO_browser2 ISO_browser2::operator*() const {
	return WithPtr(ISO_browser::operator*(), p);
}

ISO_browser2::iterator ISO_browser2::begin() const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE: {
				CISO_type_composite	&comp	= *(CISO_type_composite*)type;
				return iterator(ISO_browser::iterator(data, comp.begin()),p);
			}
			case ISO_ARRAY: {
				ISO_type_array		&array	= *(ISO_type_array*)type;
				return iterator(ISO_browser::iterator(array.subtype, data, array.subsize),p);
			}
			case ISO_OPENARRAY: {
				ISO_type_openarray	&array	= *(ISO_type_openarray*)type;
				return iterator(ISO_browser::iterator(array.subtype, type->ReadPtr(data), array.subsize),p);
			}
			case ISO_REFERENCE:
				p		= iso::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt	= (ISO_virtual*)type;
					uint32		count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).begin() : ~count == 0 ? iterator() : iterator(ISO_browser::iterator(virt, data, 0), p);
				}

			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

ISO_browser2::iterator ISO_browser2::end() const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const ISO_type *type = this->type; type;) {
		switch (type->Type()) {
			case ISO_COMPOSITE: {
				CISO_type_composite	&comp	= *(CISO_type_composite*)type;
				return iterator(ISO_browser::iterator((uint8*)data + comp.GetSize(), comp.end()), p);
			}
			case ISO_ARRAY: {
				ISO_type_array		&array	= *(ISO_type_array*)type;
				return iterator(ISO_browser::iterator(array.subtype, (uint8*)data + array.subsize * array.Count(), array.subsize), p);
			}
			case ISO_OPENARRAY: {
				ISO_type_openarray	&array	= *(ISO_type_openarray*)type;
				data	= type->ReadPtr(data);
				return iterator(ISO_browser::iterator(array.subtype, (uint8*)data + array.subsize * _Count(ISO_openarray_header::Header(data)), array.subsize), p);
			}
			case ISO_REFERENCE:
				p		= iso::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.Type();
				break;

			case ISO_USER:
				type	= ((ISO_type_user*)type)->subtype;
				break;

			case ISO_VIRTUAL:
				if (!type->Fixed()) {
					ISO_virtual	*virt	= (ISO_virtual*)type;
					uint32		count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).end() : ~count == 0 ? iterator() : iterator(ISO_browser::iterator(virt, data, count), p);
				}

			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

//-----------------------------------------------------------------------------
//	ISO_browser::s_setget
//-----------------------------------------------------------------------------

template<> bool ISO_browser::s_setget<bool>::get(void *data, const ISO_type *type, const bool &t) {
	while (type) {
		switch (type->Type()) {
		case ISO_INT:
		case ISO_FLOAT:
			return read_bytes<uint64>(data, type->GetSize()) != 0;
		case ISO_STRING:
			if (const char *p = ((ISO_type_string*)type)->get(data)) {
				bool	t2;
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

const char *ISO_browser::s_setget<const char*>::get(void *data, const ISO_type *type, const char *t) {
	while (type) {
		switch (type->Type()) {
			case ISO_STRING:
				return ((ISO_type_string*)type)->get(data);
			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
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

bool ISO_browser::s_setget<const char*>::set(void *data, const ISO_type *type, const char *t) {
	while (type) {
		switch (type->Type()) {
			case ISO_INT:
				if (is_signed_int(t)) {
					((CISO_type_int*)type)->set(data, from_string<int64>(t));
					return true;
				}
				return false;

			case ISO_FLOAT: {
				float	f;
				if (from_string(t, f)) {
					((CISO_type_float*)type)->set(data, f);
					return true;
				}
				return false;
			}

			case ISO_STRING:
				((ISO_type_string*)type)->set(data, t);
				return true;

			case ISO_REFERENCE:
				if (void *data2 = type->ReadPtr(data)) {
					data	= data2;
					type	= GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				} else {
					iso_string8	*p = _MakePtr<32>(tag2(), iso_string8(t));
					GetHeader(p)->addref();
					type->WritePtr(data, p);
					return true;
				}
				break;

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

string ISO_browser::s_setget<string>::get(void *data, const ISO_type *type, const char *t) {
	while (type) {
		switch (type->Type()) {
			case ISO_INT: {
				string			s;
				string_builder	b(s);
				CISO_type_int	*t	= (CISO_type_int*)type;
				int64			i = t->get64(data);
				if (t->frac_bits())
					b << i / float(t->frac_factor());
				else
					b << i;
				return b;
			}
			case ISO_FLOAT: {
				string			s;
				string_builder	b(s);
				CISO_type_float	*t	= (CISO_type_float*)type;
				if (t->num_bits() > 32)
					b << t->get64(data);
				else
					b << t->get(data);
				return b;
			}
			case ISO_STRING:
				return ((ISO_type_string*)type)->get(data);

			case ISO_REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->Type();
				break;

			case ISO_ARRAY: {
				const ISO_type *subtype = type->SubType()->SkipUser();
				if (subtype && subtype->Type() == ISO_INT && (subtype->flags & ISO_type_int::CHR)) {
					uint32	len = ((ISO_type_array*)type)->Count();
					switch (subtype->GetSize()) {
						case 1:	return count_string((const char*)data, len);
						case 2:	return count_string16((const char16*)data, len);
					}
				}
				return t;
			}
			case ISO_OPENARRAY: {
				const ISO_type *subtype = type->SubType()->SkipUser();
				if (subtype && subtype->Type() == ISO_INT && (subtype->flags & ISO_type_int::CHR)) {
					ISO_type_openarray	&array	= *(ISO_type_openarray*)type;
					void	*data2	= type->ReadPtr(data);
					uint32	len		= _Count(ISO_openarray_header::Header(data2));
					switch (subtype->GetSize()) {
						case 1:	return count_string((const char*)data2, len);
						case 2:	return count_string16((const char16*)data2, len);
					}
				}
				return t;
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

//-------------------------------------
// memory
//-------------------------------------

bool ISO_def<memory_block>::virt::Update(memory_block &a, const char *spec, bool from) {
	if (!spec && !from) {
		// loading - should be ptr to char[n]
		auto	p = *(ISO_ptr<void>*)&a;
		a = memory_block(p, p.Type()->GetSize());
		return true;
	}
	return false;
}

bool ISO_def<const_memory_block>::virt::Update(const_memory_block &a, const char *spec, bool from) {
	if (!spec && !from) {
		// loading - should be ptr to char[n]
		auto	p = *(ISO_ptr<void>*)&a;
		a = const_memory_block(p, p.Type()->GetSize());
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	ISO_bitpacked
//-----------------------------------------------------------------------------

ISO_ptr<uint64> ISO_bitpacked::GetTemp(void *tag) {
	static struct TEMP {
		ISO_ptr<uint64>	p;
		void*	tag()	const	{ return p.User(); }
		void	tag(void *x)	{ p.User() = x; }
	} temps[16];

	TEMP	*avail = 0;
	for (TEMP *t = temps; t < temps + 16; ++t) {
		if (t->p) {
			if (!avail && t->p.Header()->refs == 0)
				avail = t;
			if (t->tag() == tag)
				return t->p;
		} else {
			if (!avail)
				(avail = t)->p.Create(0);
			avail->tag(tag);
			return avail->p;
		}
	}
	return ISO_ptr<uint64>();
}

int ISO_bitpacked::GetIndex(uint8 &data, const tag2 &id, int from) {
	for (uint32 i = from; i < count; i++)
		if (id == array(i).id.get_tag())
			return i;
	return -1;
}
ISO_browser2 ISO_bitpacked::Index(uint8 &data, int i) {
	ISO_element		&e	= array(i);
	ISO_ptr<uint64> t	= GetTemp(&data + i);

	uint64	*p	= (uint64*)(&data + e.offset / 8);
	*t			= extract_bits(*p, e.offset & 7, e.size);
	return ISO_browser2(e.type, t, t);
}
bool ISO_bitpacked::Update(uint8 &data, const char *spec, bool from) {
	if (from)
		return false;
	int				i	= from_string<int>(spec + 1);
	ISO_element		&e	= array(i);
	ISO_ptr<uint64> t	= GetTemp(&data + i);
	write_bits(&data + e.offset / 8, *t, e.offset & 7, e.size);
	return true;
}

}//namespace iso
