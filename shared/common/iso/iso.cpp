#include "iso.h"
#include "base/soft_float.h"
#include "thread.h"
#include "extra/random.h"

#include "allocators/tlsf.h"
#include "vm.h"

namespace ISO {

//template<int B> ptr<void, B>	iso_nil;

browser_root& root() {
	static browser_root u;
	return u;
}

//-----------------------------------------------------------------------------
//	types
//-----------------------------------------------------------------------------

//UserTypeArray user_types;

UserTypeArray&	get_user_types() {
	static UserTypeArray u;
	return u;
}

UserTypeArray::~UserTypeArray() {
	for (auto &i : *this)
		if (i->flags & TypeUser::FROMFILE)
			delete i;
}

TypeUser *UserTypeArray::Find(const tag2 &id) {
	for (auto &i : *this)
		if (i->ID() == id)
			return i;
	return nullptr;
}
void UserTypeArray::Add(TypeUser *t) {
	if (auto p = Find(t->ID()))
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

initialise init(
	getdef<bool8>(),
	getdef<float>(),
	getdef<double>(),
	getdef<StartBin>()
);

//-----------------------------------------------------------------------------
//	allocation
//-----------------------------------------------------------------------------

struct malloc_iso_allocator {
	void	*alloc(size_t size, size_t align)				{ return iso::aligned_alloc(iso::align(size, (uint32)align), align); }
	void	*realloc(void *p, size_t size, size_t align)	{ return iso::aligned_realloc(p, size, align); }
	bool	free(void *p)									{ iso::aligned_free(p); return true; }
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
		void	*m	= iso::malloc(adjusted_size(size, align));
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
		void	*mem	= iso::malloc(size);
		pp	= tlsf::create(mem, size);
	}
};

struct tlsfvm_iso_allocator {
	tlsf::heap	*pp;
	Benaphore	mutex;

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
		memset(p, 0xbd, tlsf::block_size(p));
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
		size_t	size	= bit64(sizeof(void*) == 8 ? 32 : 30);
		void	*base;
		while (!(base = vmem::reserve(size))) {
			size >>= 1;
		}
		size	= bit64(20);
		pp		= tlsf::create(vmem::commit(base, size), size);
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

template<> vallocator&	allocate<32>::allocator() {
	static default_iso_allocator32	_default_iso_allocator;
	static vallocator	t(&_default_iso_allocator);
	return t;
}

template<> vallocator&	allocate<64>::allocator() {
	static default_iso_allocator64	_default_iso_allocator;
	static vallocator	t(&_default_iso_allocator);
	return t;
}

bin_allocator&	iso_bin_allocator() {
	static bin_allocator t;
	return t;
}

flags<allocate_base::FLAGS>	allocate_base::flags;

//-----------------------------------------------------------------------------
//	tag2
//-----------------------------------------------------------------------------

size_t to_string(char *s, const tag2 &t) {
	if (!t)
		return to_string(s, "<no id>");
	if (tag p = t)
		return string_copy(s, (const char*)p);
	return _format(s, "crc_%08x", (uint32)t.get_crc32());
}

const char* to_string(const tag2 &t) {
	if (tag p = t)
		return (const char*)p;
	return t ? "<unknown>" : "<no id>";
}

//-----------------------------------------------------------------------------
//	Type
//-----------------------------------------------------------------------------

string_accum& operator<<(string_accum &a, const Type *const type) {
	if (type) switch (type->GetType()) {
		case INT:
			a << "int";
			if (type != getdef<int>()) {
				TypeInt	*i = (TypeInt*)type;
				a << '{' << onlyif((i->flags & TypeInt::HEX) || !(i->flags & TypeInt::SIGN), i->flags & TypeInt::HEX ? 'x' : 'u') << i->num_bits();
				if (i->frac_bits())
					a << '.' << i->frac_bits();
				a << '}';
			}
			break;
		case FLOAT:
			a << "float";
			if (type != getdef<float>())
				a << '{' << onlyif(!(type->flags & TypeFloat::SIGN), 'u') << type->param1 << '.' << type->param2 << '}';
			break;
		case STRING:
			a << "string";
			if (int	c = ((TypeString*)type)->log2_char_size())
				a << (8 << c);
			return a;

		case VIRTUAL:	return a << "virtual";
		case COMPOSITE:	{
			TypeComposite	&comp	= *(TypeComposite*)type;
			a << '{';
			for (auto &i : comp)
				a << ' ' << get(i.type) << '.' << ifelse(i.id.blank(), '.', comp.GetID(&i));
			return a << " }";
		}
		case ARRAY:		return a << get(((TypeArray*)type)->subtype) << '[' << ((TypeArray*)type)->count << ']';
		case OPENARRAY:	return a << get(((TypeOpenArray*)type)->subtype) << "[]";
		case REFERENCE:	return a << get(((TypeReference*)type)->subtype) << "*";
		case USER:		return a << get(((TypeUser*)type)->ID());
		default:		break;
	}
	return a;
}

const Type *Type::_SubType(const Type *type) {
	if (type) switch (type->GetType()) {
		case STRING:	return type->flags & TypeString::UTF16 ? getdef<char16>() : type->flags & TypeString::UTF32 ? getdef<char32>() : getdef<char>();
		case ARRAY:		return ((TypeArray*)type)->subtype;
		case OPENARRAY:	return ((TypeOpenArray*)type)->subtype;
		case REFERENCE:	return ((TypeReference*)type)->subtype;
		case USER:		return ((TypeUser*)type)->subtype;
		default:		break;
	}
	return 0;
}

const Type *Type::_SkipUser(const Type *type) {
	while (type && type->GetType() == USER)
		type = ((TypeUser*)type)->subtype;
	return type;
}

uint32 Type::_GetSize(const Type *type) {
	if (type) switch (type->GetType()) {
		case INT:		return ((TypeInt*)type)->GetSize();
		case FLOAT:		return ((TypeFloat*)type)->GetSize();
		case STRING:	return ((TypeString*)type)->GetSize();
		case COMPOSITE:	return ((TypeComposite*)type)->GetSize();
		case ARRAY:		return ((TypeArray*)type)->GetSize();
		case VIRTUAL:	return ((Virtual*)type)->GetSize();
		case USER:		return _GetSize(((TypeUser*)type)->subtype);
		case OPENARRAY:
		case REFERENCE:	return type->Is64Bit() ? 8 : 4;
		default:		break;
	}
	return 0;
}

uint32 Type::_GetAlignment(const Type *type) {
	if (!type || type->IsPacked())
		return 1;
	switch (type->GetType()) {
		case INT:		return ((TypeInt*)type)->GetSize();
		case FLOAT:		return ((TypeFloat*)type)->GetSize();
		case STRING:	return ((TypeString*)type)->GetAlignment();
		case COMPOSITE:	return ((TypeComposite*)type)->GetAlignment();
		case ARRAY:		return _GetAlignment(((TypeArray*)type)->subtype);
		case OPENARRAY:
		case REFERENCE:	return type->Is64Bit() ? 8 : 4;
		case USER:		return _GetAlignment(((TypeUser*)type)->subtype);
		default:		return 1;
	}
}

bool Type::_Is(const Type *type, const tag2 &id) {
	return TypeType(type) == USER && ((TypeUser*)type)->ID() == id;
}

bool Type::_ContainsReferences(const Type *type) {
	if (type) switch (type->GetType()) {
		case COMPOSITE:
			for (auto &i : *(const TypeComposite*)type) {
				if (_ContainsReferences(i.type))
					return true;
			}
			return false;
		case ARRAY:		return _ContainsReferences(((TypeArray*)type)->subtype);
		case OPENARRAY:	return _ContainsReferences(((TypeOpenArray*)type)->subtype);
		case USER:		return _ContainsReferences(((TypeUser*)type)->subtype);
		case REFERENCE:	return true;
		case VIRTUAL:	return false;
		default:		break;
	}
	return false;
}

bool Type::_IsPlainData(const Type *type, bool flip_endian) {
	if (type) switch (type->GetType()) {
		case INT:		return !flip_endian || _GetSize(type) == 1 || (type->flags & TypeInt::NOSWAP);
		case FLOAT:		return !flip_endian || _GetSize(type) == 1;
		case COMPOSITE:
			for (auto &i : *(const TypeComposite*)type) {
				if (!_IsPlainData(i.type, flip_endian))
					return false;
			}
			return true;
		case ARRAY:		return _IsPlainData(((TypeArray*)type)->subtype, flip_endian);
		case USER:		return !(type->flags & (TypeUser::CHANGE | TypeUser::INITCALLBACK)) && _IsPlainData(((TypeUser*)type)->subtype, flip_endian);
		default:		break;
	}
	return false;
}

bool IsSubclass(const Type *type1, const Type *type2, int criteria) {
	if (TypeType(type1) != COMPOSITE)
		return false;
	const TypeComposite *comp = (const TypeComposite*)type1;
	return comp->Count() > 0 && (*comp)[0].id == 0 && Same((*comp)[0].type, type2, criteria);
}

bool Type::_Same(const Type *type1, const Type *type2, int criteria) {
	if (type1 == type2)
		return true;
	if (!type1 || !type2) {
		return (criteria & MATCH_MATCHNULLS)
			|| ((criteria & MATCH_MATCHNULL_RHS) && !type2);
	}

	if ((type1->flags ^ type2->flags) & (criteria & MATCH_IGNORE_SIZE ? TYPE_MASK : TYPE_MASKEX)) {
/*		if (type2->TypeType() == COMPOSITE) {
			TypeComposite	*comp2 = (TypeComposite*)type2;
			if (comp2->Count() && !(*comp2)[0].id && (*comp2)[0].offset == 0 && Same(type1, (*comp2)[0].type, criteria))
				return true;
		}*/
		return	!(criteria & MATCH_NOUSERRECURSE) && (
				type1->GetType()	== USER	? Same(((TypeUser*)type1)->subtype, type2, criteria)
			:	type2->GetType()	== USER	&& !(criteria & MATCH_NOUSERRECURSE_RHS) ? Same(type1, ((TypeUser*)type2)->subtype, criteria)
			:	false
		);
	}
	switch (type1->GetType()) {
		case INT:
			return type1->param1 == type2->param1
				&& type1->param2 == type2->param2
				&& ((criteria & MATCH_IGNORE_INTERPRETATION) || !((type1->flags ^ type2->flags) & (TypeInt::SIGN | TypeInt::NOSWAP | TypeInt::FRAC_ADJUST)));

		case FLOAT:
			return type1->param1 == type2->param1
				&& type1->param2 == type2->param2
				&& ((criteria & MATCH_IGNORE_INTERPRETATION) || !((type1->flags ^ type2->flags) & TypeFloat::SIGN));

		case STRING:
			return !((type1->flags ^ type2->flags) & (TypeString::UTF16 | TypeString::UTF32));

		case COMPOSITE: {
			TypeComposite	&comp1 = *(TypeComposite*)type1;
			TypeComposite	&comp2 = *(TypeComposite*)type2;
			if (comp1.Count() != comp2.Count() || comp1.GetSize() != comp2.GetSize())
				return false;
			for (int i = 0; i < comp1.Count(); i++) {
				if (!Same(comp1[i].type, comp2[i].type, criteria))
					return false;
			}
			return true;
		}
		case ARRAY:
			return (((TypeArray*)type2)->count == 0 || ((TypeArray*)type1)->count == ((TypeArray*)type2)->count)
				&& Same(((TypeArray*)type1)->subtype, ((TypeArray*)type2)->subtype, criteria);

		case OPENARRAY:
			return Same(((TypeOpenArray*)type1)->subtype, ((TypeOpenArray*)type2)->subtype, criteria);

		case REFERENCE:
			return Same(((TypeReference*)type1)->subtype, ((TypeReference*)type2)->subtype, criteria);

		case USER: {
			if (IsSubclass(type1->SkipUser(), type2, criteria & ~MATCH_MATCHNULLS))
				return true;
			return !(criteria & (MATCH_NOUSERRECURSE | MATCH_NOUSERRECURSE_BOTH | MATCH_NOUSERRECURSE_RHS))
				&& Same(type1, ((TypeUser*)type2)->subtype, criteria);
		}
		case FUNCTION:
		case VIRTUAL:
			return false;

		default:
			return true;
	}
}

const Type *Canonical(const TypeInt *type) {
	if (type->frac_bits() || (type->flags & (TypeInt::NOSWAP | TypeInt::FRAC_ADJUST)))
		return 0;
	bool	sign = type->is_signed();
	switch (type->num_bits()) {
		case 8:		return sign ? getdef<int8>()	: getdef<uint8>();
		case 16:	return sign ? getdef<int16>()	: getdef<uint16>();
		case 32:	return sign ? getdef<int32>()	: getdef<uint32>();
		case 64:	return sign ? getdef<int64>()	: getdef<uint64>();
		default:	return 0;
	}
}

const Type *Canonical(const TypeFloat *type) {
	if (type->is_signed()) {
		if (type->num_bits() == 32) {
			if (type->exponent_bits() == 8)
				return getdef<float>();
		} else if (type->num_bits() == 64) {
			if (type->exponent_bits() == 11)
				return getdef<double>();
		}
	}
	return 0;
}

const Type *Canonical(const Type *type) {
	if (type) switch (type->GetType()) {
		case INT:	return Canonical((const TypeInt*)type);
		case FLOAT: return Canonical((const TypeFloat*)type);
		default:	break;
	}
	return 0;
}

const Type *Duplicate(const Type *type) {
	if (type) switch (TYPE t = type->GetTypeEx()) {
		case INT:
			if (const Type *canon = Canonical((const TypeInt*)type))
				return canon;
			return new TypeInt(*(TypeInt*)type);

		case FLOAT:
			if (const Type *canon = Canonical((const TypeFloat*)type))
				return canon;
			return new TypeFloat(*(TypeFloat*)type);

		case COMPOSITE: {
			TypeComposite*	comp	= (TypeComposite*)type;
			uint32			count	= comp->Count();
			TypeComposite*	comp2	= new(count) TypeComposite(count, (Type::FLAGS)comp->flags);
			for (int i = 0; i < count; i++) {
				Element	&e	= (*comp2)[i] = (*comp)[i];
				e.type	= Duplicate(e.type);
			}
			return comp2;
		}
		case ARRAY: {
			TypeArray	*array	= (TypeArray*)type;
			return new TypeArray(Duplicate(array->subtype), array->count);
		}

		case OPENARRAY:
		case OPENARRAY|Type::TYPE_64BIT:
			return new TypeOpenArray(Duplicate(((TypeOpenArray*)type)->subtype), Type::FLAGS(t & Type::TYPE_64BIT));

		case REFERENCE:
		case REFERENCE|Type::TYPE_64BIT:
			return new TypeReference(Duplicate(((TypeReference*)type)->subtype), Type::FLAGS(t & Type::TYPE_64BIT));

		case USER:
			if (type->flags & TypeUser::CRCID)
				return Duplicate(((TypeUser*)type)->subtype);
			fallthrough
		default:
			break;
	}
	return type;
}

//-----------------------------------------------------------------------------
//	class TypeComposite
//-----------------------------------------------------------------------------

uint32 TypeComposite::CalcAlignment() const {
	uint32	a = 1;
	for (auto &i : *this)
		a = align(a, i.type->GetAlignment());
	return a;
}

int TypeComposite::GetIndex(const tag2 &id, int from) const {
	if (flags & CRCIDS) {
		crc32	c = id;
		for (uint32 i = from; i < count; i++)
			if (c == (crc32&)(*this)[i].id)
				return i;
	} else if (tag s = id) {
		for (uint32 i = from; i < count; i++)
			if (s == (*this)[i].id.get_tag())
				return i;
	} else {
		crc32	c = id;
		for (uint32 i = from; i < count; i++)
			if (c == (*this)[i].id.get_crc(false))
				return i;
	}
	return -1;
}

uint32 TypeComposite::Add(const Type* type, tag1 id, bool packed) {
	if (!type || type->Dodgy())
		this->flags |= TYPE_DODGY;

	Element	*e			= end();
	size_t	offset		= 0;
	auto	alignment	= type->GetAlignment();

	if (alignment > (1 << param1))
		param1 = log2(alignment);

	if (count) {
		offset = e[-1].end();
		if (!packed)
			offset = align(offset, alignment);
	}

	e->set(id, type, offset);
	count++;
	return uint32(offset);
}

TypeComposite *TypeComposite::Duplicate(void *defaults) const {
	int		n		= Count();
	uint32	size	= defaults ? GetSize() : 0;
	TypeComposite	*c = new(n, size) TypeComposite(n);
	c->param1		= param1;
	memcpy(c->begin(), begin(), n * sizeof(Element));

	if (defaults) {
		c->flags |= DEFAULT;
		memcpy(c->end(), defaults, size);
	}
	return c;
}

//-----------------------------------------------------------------------------
//	class TypeEnumT<T>
//-----------------------------------------------------------------------------

template<typename T> uint32 TypeEnumT<T>::size() const {
	uint32	n	= 0;
	for (auto *i = this->begin(); !i->id.blank(); i++)
		n++;
	return n;
}

template<typename T> uint32 TypeEnumT<T>::factors(T f[], uint32 num) const {
	uint32		nc	= 0;
	uint32		nf	= 0;
	T			pv	= 0;

	for (auto *i = this->begin(); !i->id.blank(); i++) {
		if (T v = i->value) {
			f[nc++] = v;
			if (nc == num)
				return ~0u;
		}
	}

	for (bool swapped = true; swapped;) {
		swapped = false;
		for (uint32 i = 1; i < nc; i++) {
			if (f[i - 1] > f[i]) {
				iso::swap(f[i - 1], f[i]);
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

template<typename T> const EnumT<T> *TypeEnumT<T>::biggest_factor(T x) const {
	if (flags & DISCRETE) {
		for (auto *i = this->begin(); !i->id.blank(); i++) {
			if (i->value == x)
				return i;
		}
		return 0;
	}
	const EnumT<T> *best	= 0;
	for (auto *i = this->begin(); !i->id.blank(); i++) {
		if (i->value <= x && (!best || i->value >= best->value))
			best	= i;
	}
	if (!best || (best->value == 0 && x != 0))
		return 0;
	return best;
}
template uint32 TypeEnumT<uint32>::size() const;
template uint32 TypeEnumT<uint32>::factors(uint32 f[], uint32 num) const;
template const EnumT<uint32> *TypeEnumT<uint32>::biggest_factor(uint32 x) const;

template uint32 TypeEnumT<uint64>::size() const;
template uint32 TypeEnumT<uint64>::factors(uint64 f[], uint32 num) const;
template const EnumT<uint64> *TypeEnumT<uint64>::biggest_factor(uint64 x) const;

template uint32 TypeEnumT<uint8>::size() const;
template uint32 TypeEnumT<uint16>::size() const;

//-----------------------------------------------------------------------------
//	class TypeFloat
//-----------------------------------------------------------------------------

float TypeFloat::get(void *data) const {
	return	this == getdef<float>()		?	*(float*)data
		:	this == getdef<double>()	?	*(double*)data
		:	read_float<float>(data, param1, param2, is_signed());
}

double TypeFloat::get64(void *data) const {
	return	this == getdef<float>()		?	*(float*)data
		:	this == getdef<double>()	?	*(double*)data
		:	read_float<double>(data, param1, param2, is_signed());
}

void TypeFloat::set(void *data, uint64 m, int e, bool s) const {
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

void TypeFloat::set(void *data, float f) const {
	if (this == getdef<float>())
		*(float*)data = f;
	else if (this == getdef<double>())
		*(double*)data = f;
	else {
		float_components<float> c;
		set(data, c.m, c.e, !!c.s);
	}
}

void TypeFloat::set(void *data, double f) const {
	if (this == getdef<float>())
		*(float*)data = f;
	else if (this == getdef<double>())
		*(double*)data = f;
	else {
		float_components<double> c;
		set(data, c.m, c.e, !!c.s);
	}
}

//-----------------------------------------------------------------------------
//	TypeString
//-----------------------------------------------------------------------------

memory_block TypeString::get_memory(const void* data) const {
	void	*s = ReadPtr(data);
	switch (log2_char_size()) {
		default:	return memory_block(s, string_end((char*)s));
		case 1:		return memory_block(s, string_end((char16*)s));
		case 2:		return memory_block(s, string_end((char32*)s));
		case 3:		return memory_block(s, string_end((uint64*)s));
	}
}

uint32 TypeString::len(const void *s) const {
	switch (log2_char_size()) {
		default:	return (uint32)string_len((const char*)s);
		case 1:		return (uint32)string_len((const char16*)s);
		case 2:		return (uint32)string_len((const char32*)s);
		case 3:		return (uint32)string_len((uint64*)s);
	}
}

void *TypeString::dup(const void *s) const {
	if (!s)
		return nullptr;

	if (flags & _MALLOC)
		return iso::strdup((const char*)s);

	uint32	size = (len(s) + 1) << log2_char_size();
	void	*t	= Is64Bit() ? allocate<64>::alloc(size) : allocate<32>::alloc(size);
	memcpy(t, s, size);
	return t;
}

//-----------------------------------------------------------------------------
//	openarrays
//-----------------------------------------------------------------------------

template<int B> OpenArrayHead *OpenArrayAlloc<B>::Create(OpenArrayHead *h, uint32 subsize, uint32 align, uint32 count) {
	Destroy(h, align);
	return count ? Create(subsize, align, count, count) : 0;
}

template<int B> OpenArrayHead *OpenArrayAlloc<B>::Create(OpenArrayHead *h, const Type *type, uint32 count) {
	uint32	align	= type->GetAlignment();
	h				= Create(h, type->GetSize(), align, count);
	if (type->SkipUser()->GetType() == REFERENCE)
		memset(GetData(h), 0, count * 4);
	return h;
}

template<int B> OpenArrayHead *OpenArrayAlloc<B>::Resize(OpenArrayHead *h, uint32 subsize, uint32 align, uint32 count, bool clear) {
	if (count == 0) {
		Destroy(h, align);
		return 0;
	}
	uint32	count0	= h ? h->count : 0;

	if (h && h->bin) {
		if (count > count0) {	//error!
			OpenArrayHead	*h0 = h;
			h = Create(h, subsize, align, count);
			memcpy(GetData(h), GetData(h0), count0 * subsize);
		} else {
			h->count = count;
		}
	} else {
		uint32	max	= h ? h->max : 0;
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

template<int B> OpenArrayHead *OpenArrayAlloc<B>::Append(OpenArrayHead *h, uint32 subsize, uint32 align, bool clear) {
	return Resize(h, subsize, align, GetCount(h) + 1, clear);
}

template<int B> OpenArrayHead *OpenArrayAlloc<B>::Insert(OpenArrayHead *h, uint32 subsize, uint32 align, int i, bool clear) {
	uint32	n	= GetCount(h);
	h = Resize(h, subsize, align, n + 1, false);
	void	*d	= h->GetElement(i, subsize);
	memmove((uint8*)d + subsize, d, (n - i) * subsize);
	if (clear)
		memset(d, 0, subsize);
	return h;
}

template struct OpenArrayAlloc<32>;
template struct OpenArrayAlloc<64>;

bool OpenArrayHead::Remove(uint32 subsize, uint32 align, int i) {
	if (i < count) {
		--count;
		void	*d	= GetElement(i, subsize);
		memcpy(d, (uint8*)d + subsize, (count - i) * subsize);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	TypeReference
//-----------------------------------------------------------------------------

void TypeReference::set(void* data, const ptr<void,64> &p) const {
	if (Is64Bit())
		*(ptr<void,64>*)data = p;
	else
		*(ptr<void,32>*)data = p;
}

ptr<void,64> TypeReference::get(void* data) const {
	return GetPtr<64>(ReadPtr(data));
}

//-----------------------------------------------------------------------------
//	FindByType
//-----------------------------------------------------------------------------
/*
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

template<> ptr<void>	&anything::FindByType(const Type *type, int crit)	{ return ISO::FindByType(begin(), end(), type, crit); }
template<> ptr<void>	&anything::FindByType(tag2 id)						{ return ISO::FindByType(begin(), end(), id); }
template<> ptr64<void>	&anything64::FindByType(const Type *type, int crit)	{ return ISO::FindByType(begin(), end(), type, crit); }
template<> ptr64<void>	&anything64::FindByType(tag2 id)					{ return ISO::FindByType(begin(), end(), id); }
*/
//-----------------------------------------------------------------------------
//	MakeRawPtr / MakeRawPtrExternal
//-----------------------------------------------------------------------------

template<int B> void *MakeRawPtrSize(const Type *type, tag2 id, uint32 size) {
	Value	*v		= new(allocate<B>::alloc(sizeof(Value) + size)) Value(id, Value::NATIVE | (B == 32 ? Value::MEMORY32 : 0), type);
	memset(v + 1, 0, size);
	return v + 1;
}

template void *MakeRawPtrSize<32>(const Type *type, tag2 id, uint32 size);
template void *MakeRawPtrSize<64>(const Type *type, tag2 id, uint32 size);

template<int B> void *MakeRawPtrExternal(const Type *type, const char *filename, tag2 id) {
	size_t	size	= strlen(filename) + 1;
	Value	*v		= new(type && type->Is64Bit() ? allocate<B>::alloc(sizeof(Value) + size) : allocate<32>::alloc(sizeof(Value) + size)) Value(id, Value::EXTERNAL | Value::NATIVE | (B == 32 ? Value::MEMORY32 : 0), type);
	char	*p		= (char*)(v + 1);
	memcpy(p, filename, size);
	return p;
}

template void *MakeRawPtrExternal<32>(const Type *type, const char *filename, tag2 id);
template void *MakeRawPtrExternal<64>(const Type *type, const char *filename, tag2 id);

//-----------------------------------------------------------------------------
//	Delete
//-----------------------------------------------------------------------------

static int Delete(const Type *type, void *data, uint32 flags) {
	if (type) switch (type->GetType()) {
		case STRING:
			if (!(flags & Value::FROMBIN)) {
				((TypeString*)type)->free(data);
				return 1;
			}
			break;

		case COMPOSITE: {
			int			ret		= 0;
			for (auto &i : *(TypeComposite*)type)
				ret |= Delete(i.type, (char*)data + i.offset, flags);
			return ret;
		}
		case ARRAY: {
			TypeArray	*array	= (TypeArray*)type;
			size_t		stride	= array->subsize;
			int			ret		= 0;
			type	= array->subtype;
			for (int n = array->Count(); n--; data = (uint8*)data + stride) {
				ret |= Delete(type, data, flags);
				if (!(ret & 1))
					return 0;
			}
			return ret;
		}
		case OPENARRAY: {
			if (data = type->ReadPtr(data)) {
				OpenArrayHead	*h			= OpenArrayHead::Get(data);
				TypeOpenArray	*array		= (TypeOpenArray*)type;
				uint32			subsize		= array->subsize;
				const Type		*subtype	= array->subtype;
				int				ret			= 0;
				for (int n = GetCount(h); n--; data = (uint8*)data + subsize) {
					ret |= Delete(subtype, data, flags);
					if (!(ret & 1))
						break;
				}
				if (!(flags & Value::FROMBIN)) {
					if (type->Is64Bit())
						OpenArrayAlloc<64>::Destroy(h, array->SubAlignment());
					else
						OpenArrayAlloc<32>::Destroy(h, array->SubAlignment());
				}
				return ret;
			}
			return 1;
		}
		case REFERENCE:
			if (data = type->ReadPtr(data)) {
				Value	*v = GetHeader(data);
				if (v->refs != 0xffff) {
					uint32	pflags = v->Flags();
					if (!(pflags & Value::FROMBIN)
					|| (!allocate_base::flags.test(allocate_base::TOOL_DELETE)
						&& ((pflags ^ flags) & (Value::FROMBIN | Value::EXTREF))
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

		case USER: {
			if (type->flags & TypeUser::INITCALLBACK) {
				const Type	*cb = type->flags & TypeUser::LOCALSUBTYPE ? user_types.Find(((TypeUser*)type)->ID()) : type;
				((TypeUserCallback*)cb)->deinit(data);
			}
			return Delete(((TypeUser*)type)->subtype, data, flags);
		}
		case VIRTUAL:
			if (!type->Fixed())
				((Virtual*)type)->Delete((Virtual*)type, data);
			return 1;

		default:
			break;
	};
	return 0;
}

bool Value::Delete() {
//	TRACE("deleting 0x%08x: ", this) << leftjustify(type, 16) << ID() << '\n';
	bool	weak	= !!(flags & WEAKREFS);
	int		ret		= 0;

	if ((flags & REDIRECT) && !(flags & FROMBIN)) {
		((ptr<void>*)(this + 1))->Clear();
	} else if (!(flags & (EXTERNAL | CRCTYPE))) {
		ret = ISO::Delete(type, this + 1, flags & REDIRECT ? (flags & ~FROMBIN) : flags);
	}

	if (flags & FROMBIN) {
		if (ret & 2) {
			refs = 0;
			return false;
		}
		if (flags & ROOT) {
			if (user)
				iso_bin_allocator().free(iso_bin_allocator().unfix(user));
			if (!allocate<32>::free(this))
				allocate<64>::free(this);
		}
	} else {
		if (!allocate<32>::free(this))
			allocate<64>::free(this);
	}

	if (weak)
		weak::remove(this);
	return true;
}

//-----------------------------------------------------------------------------
//	Compare
//-----------------------------------------------------------------------------

static bool NeedsFullCompare(const Type *type, int flags) {
	if (!type)
		return false;

	switch (type->GetType()) {
		case STRING:
			return true;
		case COMPOSITE:
			for (auto &i : *(const TypeComposite*)type) {
				if (NeedsFullCompare(i.type, flags))
					return true;
			}
			return false;
		case OPENARRAY:	return true;
		case ARRAY:		return NeedsFullCompare(((TypeArray*)type)->subtype, flags);
		case USER:		return NeedsFullCompare(((TypeUser*)type)->subtype, flags);
		case REFERENCE:	return !!(flags & DUPF_DEEP);
		default:		return false;
	}
}

static bool CompareDataFull(const Type *type, const void *data1, const void *data2, int flags) {
	switch (type->GetType()) {
		case STRING:
			return *((TypeString*)type)->get_memory(data1) == *((TypeString*)type)->get_memory(data2);

		case COMPOSITE:
			for (auto &i : *(const TypeComposite*)type) {
				if (!CompareData(i.type, (char*)data1 + i.offset, (char*)data2 + i.offset, flags))
					return false;
			}
			return true;

		case OPENARRAY:
			data1 = type->ReadPtr(data1);
			data2 = type->ReadPtr(data2);
			if (!data1 && !data2)
				return true;
			if (!data1 || !data2)
				return false;
			{
				const OpenArrayHead *h1	= OpenArrayHead::Get(data1);
				const OpenArrayHead *h2	= OpenArrayHead::Get(data2);
				int	c = GetCount(h1);
				if (GetCount(h2) != c)
					return false;

				TypeOpenArray	*array	= (TypeOpenArray*)type;
				size_t			stride	= array->subsize;
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

		case ARRAY: {
			TypeArray	*array	= (TypeArray*)type;
			size_t		stride	= array->subsize;
			type	= array->subtype;
			for (int n = array->Count(); n--; data1 = (uint8*)data1 + stride, data2 = (uint8*)data2 + stride) {
				if (!CompareDataFull(type, data1, data2, flags))
					return false;
			}
			return true;
		}
		case USER:
			//ISO_TRACEF("CompareDataFull:") << type << '\n';
			return CompareDataFull(((TypeUser*)type)->subtype, data1, data2, flags);

		case REFERENCE:
			return (type->Is64Bit() ? (*(uint64*)data1 == *(uint64*)data2) : (*(uint32*)data1 == *(uint32*)data2))
				|| ((flags & DUPF_DEEP) && CompareData(GetPtr(type->ReadPtr(data1)), GetPtr(type->ReadPtr(data2)), flags));

		default:
			return false;
	}
}

bool CompareData(const Type *type, const void *data1, const void *data2, int flags) {
	return data1 == data2 || (NeedsFullCompare(type, flags)
		? CompareDataFull(type, data1, data2, flags)
		: memcmp(data1, data2, type->GetSize()) == 0
	);
}

bool CompareData(ptr<void> p1, ptr<void> p2, int flags) {
	if (!p1)
		return !p2;

	if (!p2 || !p1.GetType()->SameAs(p2.GetType()))
		return false;

	if (p1.IsExternal() || p2.IsExternal())
		return p1.IsExternal() && p2.IsExternal() && istr((const char*)p1) == (const char*)p2;

	return CompareData(p1.GetType(), p1, p2, flags);
}

//-----------------------------------------------------------------------------
//	Duplicate
//-----------------------------------------------------------------------------

int _Duplicate(const Type *type, void *data, int flags, void *physical_ram) {

	if (!(flags & DUPF_NOINITS) && type->GetType() == USER && (type->flags & TypeUser::INITCALLBACK)) {
		int	ret = _Duplicate(((TypeUser*)type)->subtype, data, flags, physical_ram);

		if (type->flags & TypeUser::LOCALSUBTYPE)
			type = user_types.Find(((TypeUser*)type)->ID());
		((TypeUserCallback*)type)->init((char*)data, physical_ram);
		return ret | 1;
	}

	type	= type->SkipUser();

	switch (type->GetType()) {
		case STRING: {
			if (!(flags & DUPF_DUPSTRINGS) && !(type->flags & TypeString::ALLOC))
				return 0;
			if (const void *p = type->ReadPtr(data))
				type->WritePtr(data, ((TypeString*)type)->dup(p));
			return 1;
		}

		case COMPOSITE: {
			int	ret		= 0;
			for (auto &i : *(const TypeComposite*)type)
				ret |= _Duplicate(i.type, (uint8*)data + i.offset, flags, physical_ram);
			return ret;
		}
		case ARRAY: {
			TypeArray	*array	= (TypeArray*)type;
			size_t		stride	= array->subsize;
			int			ret		= 0;
			type	= array->subtype;
			for (int n = array->Count(); n--; data = (uint8*)data + stride) {
				if (!(ret |= _Duplicate(type, data, flags, physical_ram)))
					return 0;
			}
			return ret;
		}
		case OPENARRAY:
			if (void *p1 = type->ReadPtr(data)) {
				TypeOpenArray	*array	= (TypeOpenArray*)type;
				uint32			subsize	= array->subsize;

				OpenArrayHead	*h1	= OpenArrayHead::Get(p1);
				int				n	= GetCount(h1);
				OpenArrayHead	*h2	= type->Is64Bit()
					? OpenArrayAlloc<64>::Create(subsize, array->SubAlignment(), n, n)
					: OpenArrayAlloc<32>::Create(subsize, array->SubAlignment(), n, n);
				uint8			*p2	= GetData(h2);

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

		case REFERENCE:
			if (data = type->ReadPtr(data)) {
				Value	*v		= GetHeader(data);
				uint32	pflags	= v->Flags();
				v->addref();
				if (!(pflags & (Value::EXTERNAL | Value::CRCTYPE)) && (flags & DUPF_DEEP))
					Duplicate(v->ID(), v->GetType(), data, (pflags & Value::HASEXTERNAL) ? flags : (flags & ~DUPF_CHECKEXTERNALS));
				if ((flags & DUPF_CHECKEXTERNALS) && (pflags & (Value::EXTERNAL | Value::HASEXTERNAL)))
					return 3;
			}
			return 1;

		default:
			return 0;
	};
}

void CopyData(const Type *type, const void *srce, void *dest) {
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
bool _CheckHasExternals(const ptr<void> &p, int flags, int depth);

bool _CheckHasExternals(const Type *type, void *data, int flags, int depth) {
	type = type->SkipUser();

	bool	ret		= false;
	switch (type->GetType()) {
		case COMPOSITE:
			for (auto &i : *(const TypeComposite*)type) {
				if (_CheckHasExternals(i.type, (uint8*)data + i.offset, flags, depth)) {
					ret = true;
					if (flags & DUPF_EARLYOUT)
						break;
				}
			}
			break;

		case ARRAY: {
			TypeArray	&array	= *(TypeArray*)type;
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
		case OPENARRAY: {
			data = type->ReadPtr(data);
			TypeOpenArray	*array	= (TypeOpenArray*)type;
			if (array->subtype->ContainsReferences()) {
				for (int n = GetCount(OpenArrayHead::Get(data)); n--; data = (uint8*)data + array->subsize) {
					if (_CheckHasExternals(array->subtype, data, flags, depth)) {
						ret = true;
						if (flags & DUPF_EARLYOUT)
							break;
					}
				}
			}
			break;
		}

		case REFERENCE:
			return _CheckHasExternals(GetPtr(type->ReadPtr(data)), flags, depth);

		default:
			break;
	};
	return ret;
}

bool _CheckHasExternals(const ptr<void> &p, int flags, int depth) {
	if (p) {
		if (p.IsExternal())
			return true;

		if (depth > 0) {
			if (flags & DUPF_DEEP) {
				if (!p.TestFlags(Value::TEMP_USER)) {
					p.SetFlags(Value::TEMP_USER);
					bool	has = _CheckHasExternals(p.GetType(), p, flags, depth - 1);
					if (has)
						p.SetFlags(Value::HASEXTERNAL);
					else
						p.ClearFlags(Value::HASEXTERNAL);
					return has;
				}

			} else if (p.TestFlags(Value::HASEXTERNAL)) {
				if (_CheckHasExternals(p.GetType(), p, flags, depth))
					return true;
				p.ClearFlags(Value::HASEXTERNAL);
			}
		}
	}
	return false;
}

bool CheckHasExternals(const ptr<void> &p, int flags, int depth) {
	if (flags & DUPF_DEEP) {
		bool ret = _CheckHasExternals(p, flags, depth);
		p.ClearFlags(Value::TEMP_USER);
		Browser(p).ClearTempFlags();
		return ret;
	}
	return _CheckHasExternals(p, flags, depth);
}

//-----------------------------------------------------------------------------
//	Endianness
//-----------------------------------------------------------------------------

//inplace
void FlipData(const Type *type, void *data, bool big, bool flip) {
	type	= type->SkipUser();
	if (IsPlainData(type, flip))
		return;

	switch (type->GetType()) {
		case INT:
		case FLOAT:
			switch (type->GetSize()) {
				case 2:	swap_endian_inplace(*(uint16*)data); break;
				case 4:	swap_endian_inplace(*(uint32*)data); break;
				case 8:	swap_endian_inplace(*(uint64*)data); break;
			}
			break;

		case STRING: {
			switch (((TypeString*)type)->log2_char_size()) {
				case 1:
					for (uint16 *p = (uint16*)type->ReadPtr(data); *p; ++p)
						swap_endian_inplace(*p);
					break;
				case 2:
					for (uint32 *p = (uint32*)type->ReadPtr(data); *p; ++p)
						swap_endian_inplace(*p);
					break;
			}
			break;
		}

		case COMPOSITE:
			for (auto &i : *(const TypeComposite*)type)
				FlipData(i.type, (uint8*)data + i.offset, big, flip);
			break;

		case ARRAY: {
			TypeArray	*array = (TypeArray*)type;
			for (int i = 0, c = array->Count(); i < c; i++)
				FlipData(array->subtype, (char*)data + array->subsize * i, big, flip);
			break;
		}

		case OPENARRAY: {
			data = type->ReadPtr(data);
			TypeOpenArray	*array	= (TypeOpenArray*)type;
			if (!IsPlainData(array->subtype, flip)) {
				for (int n = GetCount(OpenArrayHead::Get(data)); n--; data = (uint8*)data + array->subsize)
					FlipData(array->subtype, data, big, flip);
			}
			break;
		}

		case REFERENCE:
			if (data = type->ReadPtr(data)) {
				Value *v = GetHeader(data);
				if (v->flags & (Value::EXTERNAL | Value::CRCTYPE))
					return;
				const Type *type0 = ((TypeReference*)type)->subtype;
				if (type0 && type0->SkipUser() != v->type->SkipUser())
					return;
				bool	flip = !!(v->flags & Value::ISBIGENDIAN) != big;
				if (flip)
					v->flags	= v->flags ^ Value::ISBIGENDIAN;
				FlipData(v->type, data, big, flip);
			}
			break;

		default:
			break;
	};
}

void SetBigEndian(ptr<void> p, bool big) {
	if (Value *v = p.Header()) {
		if (v->flags & (Value::EXTERNAL | Value::CRCTYPE))
			return;
		bool	flip = !!(v->flags & Value::ISBIGENDIAN) != big;
		if (flip)
			v->flags	= v->flags ^ Value::ISBIGENDIAN;
		FlipData(v->type, p, big, flip);
	}
}

//srce -> dest
void FlipData(const Type *type, const void *srce, void *dest) {
	type	= type->SkipUser();

	switch (type->GetType()) {
		case INT:
		case FLOAT:
		case VIRTUAL:
			switch (uint32 size = type->GetSize()) {
				case 0:	break;
				case 1:	*(uint8*)dest	= *(uint8*)srce; break;
				case 2:	*(uint16*)dest	= swap_endian(*(uint16*)srce); break;
				case 4:	*(uint32*)dest	= swap_endian(*(uint32*)srce); break;
				case 8:	*(uint64*)dest	= swap_endian(*(uint64*)srce); break;
				default: memcpy(dest, srce, size); break;
			}
			break;

		case COMPOSITE:
			for (auto &i : *(const TypeComposite*)type)
				FlipData(i.type, (char*)srce + i.offset, (char*)dest + i.offset);
			break;

		case ARRAY: {
			TypeArray		*array	= (TypeArray*)type;
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
//	class Browser
//-----------------------------------------------------------------------------

template<int B> const ptr<void, B> *_Find(tag2 id, const ptr<void, B> *p, uint32 n) {
	for (const ptr<void, B> *i = p, *e = p + n; i != e; ++i) {
		if (i->IsID(id))
			return i;
	}
	return 0;
}

template<int B> int _GetIndex(tag2 id, const ptr<void, B> *p, uint32 n) {
	if (auto i = _Find(id, p, n))
		return int(i - p);
	return -1;
}

uint32 Browser::GetSize() const {
	if (!type)
		return 0;
	uint32 size = type->GetSize();
	if (size == 0 && type->GetType() == VIRTUAL && !type->Fixed()) {
		Virtual	*virt = (Virtual*)type;
		uint32	count = virt->Count(virt, data);
		if (count == 0 || ~count == 0)
			size = virt->Deref(virt, data).GetSize();
		else
			size = virt->Index(virt, data, 0).GetSize() * count;
	}
	return size;
}

void Browser::UnsafeSet(const void *srce) const {
	memcpy(data, srce, type->GetSize());
}

uint32 Browser::Count() const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case STRING:
				return	((TypeString*)type)->len(type->ReadPtr(data));

			case COMPOSITE:
				return ((TypeComposite*)type)->count;

			case ARRAY:
				return ((TypeArray*)type)->count;

			case OPENARRAY:
				return GetCount(OpenArrayHead::Get(type->ReadPtr(data)));

			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt = (Virtual*)type;
					uint32	count = virt->Count(virt, data);
					return count ? (~count == 0 ? 0 : count) : virt->Deref(virt, data).Count();
				}
				//fall through
			default:
				return 0;
		}
	}
	return 0;
}

int Browser::GetIndex(tag2 id, int from) const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE:
				return ((TypeComposite*)type)->GetIndex(id, from);

			case ARRAY: {
				auto	type2 = ((TypeArray*)type)->subtype->SkipUser();
				if (TypeType(type2) == REFERENCE) {
					uint32	n = ((TypeArray*)type)->Count() - from;
					int		i = type->Is64Bit()
						? _GetIndex(id, (const ptr64<void>*)data + from, n)
						: _GetIndex(id, (const ptr<void>*)data + from, n);
					if (i >= 0)
						return i + from;
				}
				return -1;
			}

			case OPENARRAY: {
				auto	type2 = ((TypeOpenArray*)type)->subtype->SkipUser();
				if (TypeType(type2) == REFERENCE) {
					data = type->ReadPtr(data);
					uint32	n = GetCount(OpenArrayHead::Get(data)) - from;
					int	i = type2->Is64Bit()
						? _GetIndex(id, (const ptr64<void>*)data + from, n)
						: _GetIndex(id, (const ptr<void>*)data + from, n);
					if (i >= 0)
						return i + from;
				}
				return -1;
			}

			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					int		i		= virt->GetIndex(virt, data, id, from);
					return i >= 0 ? i : virt->Deref(virt, data).GetIndex(id);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return -1;
}

tag2 Browser::GetName() const {
	const Type *type = this->type->SkipUser();
	switch (type->GetType()) {
		case REFERENCE:
			if (void *p = type->ReadPtr(data))
				return GetHeader(p)->ID();
			return tag2();

		case VIRTUAL:
			if (!type->Fixed()) {
				Virtual	*virt	= (Virtual*)type;
				return virt->Deref(virt, data).GetName();
			}
			//fall through
		default:
			return tag2();
	}
}

tag2 Browser::GetName(int i) const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE:
				return ((TypeComposite*)type)->GetID(i);

			case ARRAY: {
				auto	type2 = ((TypeArray*)type)->subtype->SkipUser();
				if (TypeType(type2) == REFERENCE) {
					if (i < ((TypeArray*)type)->Count()) {
						return type->Is64Bit()
							? ((ptr64<void>*)data)[i].ID()
							: ((ptr<void>*)data)[i].ID();
					}
				}
				return tag2();
			}
			case OPENARRAY: {
				auto	type2 = ((TypeOpenArray*)type)->subtype->SkipUser();
				if (TypeType(type2) == REFERENCE) {
					data = type->ReadPtr(data);
					if (i < GetCount(OpenArrayHead::Get(data))) {
						return type2->Is64Bit()
							? ((ptr64<void>*)data)[i].ID()
							: ((ptr<void>*)data)[i].ID();
					}
				}
				return tag2();
			}
			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					if (tag2 id = virt->GetName(virt, data, i))
						return id;
					if (tag2 id = virt->Index(virt, data, i).GetName())
						return id;
					return virt->Deref(virt, data).GetName(i);
				}
				//fall through
			default:
				return tag2();
		}
	}
	return tag2();
}

Browser::iterator Browser::begin() const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite	&comp	= *(TypeComposite*)type;
				return iterator(data, comp.begin());
			}
			case ARRAY: {
				TypeArray		&array	= *(TypeArray*)type;
				return iterator(array.subtype, data, array.subsize);
			}
			case OPENARRAY: {
				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				return iterator(array.subtype, type->ReadPtr(data), array.subsize);
			}
			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					uint32	count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).begin() : ~count == 0 ? iterator() : iterator(virt, data, 0);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

Browser::iterator Browser::end() const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite	&comp	= *(TypeComposite*)type;
				return iterator((uint8*)data + comp.GetSize(), comp.end());
			}
			case ARRAY: {
				TypeArray		&array	= *(TypeArray*)type;
				return iterator(array.subtype, array.get_memory(data).end(), array.subsize);
			}
			case OPENARRAY: {
				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				return iterator(array.subtype, array.get_memory(data).end(), array.subsize);
			}
			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					uint32	count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).end() : ~count == 0 ? iterator() : iterator(virt, data, count);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

bool Browser::Resize(uint32 n) const {
	void	*data	= this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case OPENARRAY: {
				TypeOpenArray	*array	= (TypeOpenArray*)type;
				void			*p		= type->ReadPtr(data);
				OpenArrayHead	*h		= OpenArrayHead::Get(p);
				uint32			i		= GetCount(h);
				uint32			align	= array->subtype->GetAlignment();
				if (n < i && !IsPlainData(array->subtype)) {
					while (i-- > n)
						Delete(array->subtype, (uint8*)p + i * array->subsize, 0);
				}
				h = type->Is64Bit()
					? OpenArrayAlloc<64>::Resize(h, array->subsize, align, n, true)
					: OpenArrayAlloc<32>::Resize(h, array->subsize, align, n, true);

				type->WritePtr(data, GetData(h));
				return true;
			}
			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt = (Virtual*)type;
					if (virt->Resize(virt, data, n))
						return true;
					return virt->Count(virt, data) == 0 && virt->Deref(virt, data).Resize(n);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return false;
}

bool Browser::Remove(int i) const {
	void	*data	= this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case OPENARRAY:
				if (void *p = type->ReadPtr(data)) {
					TypeOpenArray	*array	= (TypeOpenArray*)type;
					OpenArrayHead	*h		= OpenArrayHead::Get(p);
					uint32			align	= 4;
					if (!IsPlainData(array->subtype))
						Delete(array->subtype, (uint8*)p + i * array->subsize, 0);
					return h->Remove(array->subsize, align, i);
				}
				return false;

			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt = (Virtual*)type;
					return virt->Count(virt, data) == 0 && virt->Deref(virt, data).Remove(i);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return false;
}

const Browser Browser::Insert(int i) const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case OPENARRAY: {
				TypeOpenArray	*array	= (TypeOpenArray*)type;
				void			*p		= type->ReadPtr(data);
				OpenArrayHead	*h		= OpenArrayHead::Get(p);
				uint32			align	= 4;

				h = type->Is64Bit()
					? OpenArrayAlloc<64>::Insert(h, array->subsize, align, i, true)
					: OpenArrayAlloc<32>::Insert(h, array->subsize, align, i, true);

				if (h) {
					p	= GetData(h);
					type->WritePtr(data, p);
					return Browser(array->subtype, (uint8*)p + i * array->subsize);
				}
				type = 0;
				break;
			}
			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt = (Virtual*)type;
					if (virt->Count(virt, data) == 0)
						return virt->Deref(virt, data).Insert(i);
				}
				fallthrough
			default:
				type = 0;
				break;
		}
	}
	return Browser();
}

const Browser Browser::Append() const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case OPENARRAY: {
				TypeOpenArray	*array	= (TypeOpenArray*)type;
				void			*p		= type->ReadPtr(data);
				OpenArrayHead	*h		= OpenArrayHead::Get(p);
				uint32			align	= 4;

				h = type->Is64Bit()
					? OpenArrayAlloc<64>::Append(h, array->subsize, align, true)
					: OpenArrayAlloc<32>::Append(h, array->subsize, align, true);

				if (h) {
					p	= GetData(h);
					type->WritePtr(data, p);
					return Browser(array->subtype, (uint8*)p + (GetCount(h) - 1) * array->subsize);
				}
				type = 0;
				break;
			}
			case REFERENCE:
				if (void *data2 = type->ReadPtr(data)) {
					Value	*v = GetHeader(data2);
					if (const Type *type2 = v->IsExternal() ? 0 : v->GetType()) {
						data	= data2;
						type	= type2;
						break;
					}
					ptr<anything>	p(v->ID(), 1);
					v->release();
					type->WritePtr(data, p);
					p.Header()->addref();
					return MakeBrowser((*p)[0]);
				}
				type	= 0;
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt = (Virtual*)type;
					if (Browser2 b = virt->Deref(virt, data))
						return b.Append();
				}
				fallthrough
			default:
				type = 0;
				break;
		}
	}
	return Browser();
}

int Browser::ParseField(string_scan &s) const {
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

bool Browser::Update(const char *s, bool from) const {
	void *data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case VIRTUAL:
				return !type->Fixed() && ((Virtual*)type)->Update((Virtual*)type, data, s, from);

			case USER:
				type = ((TypeUser*)type)->subtype;
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
void Browser::ClearTempFlags(Browser b) {
	for (;;) {
		if (const Type *type = b.GetTypeDef()->SkipUser()) {
			if (type->GetType() == REFERENCE) {
				b = *b;
				ptr<void>	p = b;
				if (p && (p.Flags() & Value::TEMP_USER)) {
					p.ClearFlags(Value::TEMP_USER);
					continue;
				}
			} else if (type->ContainsReferences()) {
				for (Browser::iterator i = b.begin(), e = b.end(); i != e; ++i)
					ClearTempFlags(i.Ref());
			}
		}
		return;
	}
}
#else

void ClearTempFlags(Browser2 b, dynamic_array<ptr_machine<void> > &stack) {
	if (const Type *type = b.GetTypeDef()->SkipUser()) {
		if (type->GetType() == REFERENCE) {
			b = *b;
			ptr_machine<void>	p = b;
			if (p && (p.Flags() & Value::TEMP_USER)) {
				p.ClearFlags(Value::TEMP_USER);
				stack.push_back(p);
			}
		} else if (type->ContainsReferences()) {
			for (auto i : b)
				ClearTempFlags(i, stack);
		}
	}
}

void Browser::ClearTempFlags(Browser b) {
	dynamic_array<ptr_machine<void> >	stack;
	for (;;) {
		ISO::ClearTempFlags(b, stack);
		if (stack.empty())
			break;
		b = Browser(stack.pop_back_value());
	}
}
#endif

void Browser::Apply(Browser2 b, const Type *type, void f(void*)) {
	for (;;) {
		if (b.GetType() == REFERENCE) {
			b = *b;
			ptr<void>	p = b;
			if (p && !(p.Flags() & Value::TEMP_USER)) {
				p.SetFlags(Value::TEMP_USER);
				if (!p.IsType(type))
					continue;
				f(p);
			}
		} else if (b.GetTypeDef()->ContainsReferences()) {
			for (auto i : b)
				Apply(i, type, f);
		}
		return;
	}
}

void Browser::Apply(Browser2 b, const Type *type, void f(void*, void*), void *param) {
	if (b.Is(type))
		f(b, param);

	if (b.SkipUser().GetType() == REFERENCE) {
		if (!b.HasCRCType() && !b.External()) {
			Browser2	b2	= *b;
			ptr<void>	p	= b2;
			if (p && !(p.Flags() & Value::TEMP_USER)) {
				p.SetFlags(Value::TEMP_USER);
				Apply(b2, type, f, param);
			}
		}
	} else if (b.GetTypeDef()->ContainsReferences()) {
		for (auto i : b)
			Apply(i, type, f, param);
	}
}

const Element *GetElement(const Type *type, tag2 id) {
	if (type = SkipUser(type)) {
		if (type->GetType() == COMPOSITE)
			return ((const TypeComposite*)type)->Find(id);
	}
	return nullptr;
}

range<stride_iterator<void>> Browser::GetArray(tag2 id)	const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case ARRAY: {
				auto	array	= (TypeArray*)type;
				uint32	offset	= 0;
				if (id) {
					auto element = GetElement(array->subtype, id);
					if (!element)
						return {};
					offset = element->offset;
				}
				return make_range_n(stride_iterator<void>((uint8*)data + offset, array->subsize), array->count);
			}
			case OPENARRAY: {
				if (void *p = type->ReadPtr(data)) {
					auto	array	= (TypeOpenArray*)type;
					auto	h		= OpenArrayHead::Get(p);
					uint32	offset	= 0;
					if (id) {
						auto element = GetElement(array->subtype, id);
						if (!element)
							return {};
						offset = element->offset;
					}
					return make_range_n(stride_iterator<void>(GetData(h) + offset, array->subsize), h->count);
				}
				type = 0;
				break;
			}
			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt = (Virtual*)type;
					if (Browser2 b = virt->Deref(virt, data))
						return b.GetArray(id);
				}
				fallthrough
			default:
				type = 0;
				break;
		}
	}
	return {};
}

//-----------------------------------------------------------------------------
//	class Browser2
//-----------------------------------------------------------------------------

ptr_machine<void> Browser2::_GetPtr() const {
	if (IsPtr())
		return p;

	auto	type2 = type->SkipUser();

	switch (TypeType(type2)) {
		case REFERENCE:
			return ptr_machine<void>::Ptr(type2->ReadPtr(data));

		case VIRTUAL:
			if (!type2->Fixed()) {
				Virtual	*virt = (Virtual*)type2;
				if (Browser2 b = virt->Deref(virt, data)) {
					return b._GetPtr();
				}
			}
			fallthrough
		default:
			return Duplicate();
	}
}

const Browser2 WithPtr(const Browser2 &b, const ptr_machine<void> &p) {
	if (b && !b.GetPtr())
		return Browser2(b, p);
	return b;
}


const Browser2 Browser::operator*() const {
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case OPENARRAY:
				return Browser(((TypeOpenArray*)type)->subtype, type->ReadPtr(data));

			case REFERENCE:
				return GetPtr(type->ReadPtr(data));

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed())
					return ((Virtual*)type)->Deref((Virtual*)type, data);
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return Browser2();
}

const Browser2 Browser::FindByType(const Type *t, int crit) const {
	if (TypeType(type->SkipUser()) == REFERENCE)
		return (**this).FindByType(t, crit);

	for (uint32 i = 0, n = Count(); i < n; i++) {
		for (Browser2 b = (*this)[i]; b; b = *b) {
			if (b.Is(t, crit))
				return b;
		}
	}
	return Browser2();
}

const Browser2 Browser::FindByType(tag2 id) const {
	if (TypeType(type->SkipUser()) == REFERENCE)
		return (**this).FindByType(id);

	for (uint32 i = 0, n = Count(); i < n; i++) {
		Browser2	b	= (*this)[i];
		if (b.Is(id))
			return b;
		while (b.GetType() == REFERENCE) {
			b	= *b;
			if (b.Is(id))
				return b;
		}
	}
	return Browser2();
}

Browser2 GetOrAddMember(const Browser2 &b, tag2 id, const Type *create_type) {
	Browser2 b2 = b[id];
	if (b2 || !create_type)
		return b2;

	if ((b.IsPtr() && !b.GetTypeDef()) || b.Is<ptr<void>>()) {
		ptr<anything>	p(b.GetName());
		p->Append(MakePtr(create_type, id));

		b.Set(move(p));
		return b[id];
	}

//	b.SetMember(id, ptr<void>(id));
	b.SetMember(id, MakePtr(create_type, id));
	return b[id];
}

const Browser2 Browser2::Parse(string_scan spec, const char_set &set, const Type *create_type) const {
//	if (!spec)
//		return *this;

	spec.check(';');//if (*spec == ';')		++spec;

//	string_scan		s(spec);
	Browser2		b = *this;

	while (b && spec.peekc()) {
		switch (spec.peekc()) {
//			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
//				b = b[spec.get<uint32>()];
//				break;

			case '[': {
				int	i = spec.move(1).get<uint32>();
				if (spec.getc() != ']')
					return Browser2();
				b = b[i];
				break;
			}
			case '\'':
				b = GetOrAddMember(b, spec.move(1).get_token(~char_set('\'')), create_type);
				spec.move(1);
				break;

			default:
				if (!spec.get_token(~set)) {
					tag2	id	= spec.get_token(set);
					int		i	= b.GetIndex(id);
					while (spec.check('+')) {
						if (i >= 0)
							i = b.GetIndex(id, i + 1);
					}
					if (i >= 0) {
						b = b[i];
						break;
					}
					if (create_type) {
						auto	p = MakePtr(create_type, id);
						if ((b.IsPtr() && !b.GetTypeDef()) || (b.SkipUser().GetType() == REFERENCE && !*b)) {//.Is<ptr<void>>()) {
							ptr<anything>	a(b.GetName());
							a->Append(p);
							b.Set(move(a));
						} else {
							b.SetMember(id, p);
						}
						b = p;
					}
					//b = GetOrAddMember(b, id, create_type);
				}
				break;
		}
	}
	return b;
}

/*
tag2 Browser2::GetName(int i) const {
	void		*data = this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE:
				return ((TypeComposite*)type)->GetID(i);

			case ARRAY:
				if (TypeType(((TypeArray*)type)->subtype->SkipUser()) == REFERENCE) {
					if (i < ((TypeArray*)type)->Count())
						return ((ptr<void>*)data)[i].ID();
				}
				return tag2();

			case OPENARRAY:
				if (TypeType(((TypeOpenArray*)type)->subtype->SkipUser()) == REFERENCE) {
					data = type->ReadPtr(data);
					if (i < GetCount(OpenArrayHead::Get(data)))
						return ((ptr<void>*)data)[i].ID();
				}
				return tag2();

			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					tag2	id		= virt->GetName(virt, data, i);
					return id ? id : virt->Deref(virt, data).GetName(i);
				}
				//fall through
			default:
				return tag2();
		}
	}
	return tag2();
}
*/
const Browser2 Browser2::Index(tag2 id) const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite &comp = *(TypeComposite*)type;
				int i = comp.GetIndex(id);
				if (i >= 0) {
					Element	&e = comp[i];
					if (e.type && e.type->GetType() == REFERENCE)
						return ISO::GetPtr(e.type->ReadPtr((char*)data + comp[i].offset));
					return Browser2(e.type, (char*)data + e.offset, p);
				}
				//inherited?
				if (!comp[0].id) {
					type = comp[0].type;
					break;
				}
				return Browser2();
			}
			case ARRAY:
				if (TypeType(((TypeArray*)type)->subtype->SkipUser()) == REFERENCE) {
					if (const ptr<void> *i = _Find(id, (const ptr<void>*)data, ((TypeArray*)type)->Count()))
						return *i;
				}
				type = 0;
				break;

			case OPENARRAY: {
				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				if (TypeType(array.subtype->SkipUser()) == REFERENCE) {
					data = type->ReadPtr(data);
					if (const ptr<void> *i = _Find(id, (const ptr<void>*)data, GetCount(OpenArrayHead::Get(data)))) {
						if (i->GetType())
							return *i;
						return MakeBrowser(*i);
					}
				}
				type = 0;
				break;
			}

			case REFERENCE:
				p		= ISO::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					int		i		= virt->GetIndex(virt, data, id, 0);
					if (i >= 0)
						return virt->Index(virt, data, i);
					return virt->Deref(virt, data)[id];
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return Browser2();
}

const Browser2 Browser2::ref(tag2 id) const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite &comp = *(TypeComposite*)type;
				int i = comp.GetIndex(id);
				if (i >= 0) {
					Element	&e = comp[i];
					return Browser2(e.type, (char*)data + e.offset, p);
				}
				//inherited?
				if (!comp[0].id) {
					type = comp[0].type;
					break;
				}
				return Browser2();
			}
			case ARRAY:
				if (TypeType(((TypeArray*)type)->subtype->SkipUser()) == REFERENCE) {
					if (const ptr<void> *i = _Find(id, (const ptr<void>*)data, ((TypeArray*)type)->Count()))
						return *i;
				}
				type = 0;
				break;

			case OPENARRAY: {
				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				if (TypeType(array.subtype->SkipUser()) == REFERENCE) {
					data = type->ReadPtr(data);
					if (const ptr<void> *i = _Find(id, (const ptr<void>*)data, GetCount(OpenArrayHead::Get(data)))) {
						if (i->GetType())
							return *i;
						return MakeBrowser(*i);
					}
				}
				type = 0;
				break;
			}

			case REFERENCE:
				p		= ISO::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					int		i		= virt->GetIndex(virt, data, id, 0);
					if (i >= 0)
						return virt->Index(virt, data, i);
					return virt->Deref(virt, data)[id];
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return Browser2();
}

const Browser2 Browser2::Index(int i) const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case STRING: {
				const void	*p = type->ReadPtr(data);
				if (type->flags & TypeString::UTF16) {
					const char16	*s = (const char16*)p;
					if (i < string_len(s))
						return MakeBrowser(s[i]);
				} else if (type->flags & TypeString::UTF32) {
					const char32	*s = (const char32*)p;
					if (i < string_len(s))
						return MakeBrowser(s[i]);
				} else {
					const char	*s = (const char*)p;
					if (i < string_len(s))
						return MakeBrowser(s[i]);
				}
				type = 0;
				break;
			}
			case COMPOSITE: {
				TypeComposite	&comp	= *(TypeComposite*)type;
				if (i < comp.Count())
					return Browser2(comp[i].type, (char*)data + comp[i].offset, p);
				type = 0;
				break;
			}
			case ARRAY: {
				TypeArray		&array	= *(TypeArray*)type;
				if (i < array.Count())
					return Browser2(array.subtype, (char*)data + array.subsize * i, p);
				type = 0;
				break;
			}
			case OPENARRAY: {
				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				if (i < array.get_count(data))
					return Browser2(array.subtype, (uint8*)type->ReadPtr(data) + i * array.subsize, p);
				type = 0;
				break;
			}

			case REFERENCE:
				p		= ISO::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt = (Virtual*)type;
					if (Browser2 b = virt->Index(virt, data, i))
						return WithPtr(b, p);
					return WithPtr(virt->Deref(virt, data)[i], p);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return Browser2();
}

const Browser2 Browser2::operator*() const {
	return p.IsExternal() ? Browser2() : WithPtr(Browser::operator*(), p);
}

Browser2::iterator Browser2::begin() const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite	&comp	= *(TypeComposite*)type;
				return iterator(Browser::iterator(data, comp.begin()),p);
			}
			case ARRAY: {
				TypeArray		&array	= *(TypeArray*)type;
				return iterator(Browser::iterator(array.subtype, data, array.subsize),p);
			}
			case OPENARRAY: {
				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				return iterator(Browser::iterator(array.subtype, type->ReadPtr(data), array.subsize),p);
			}
			case REFERENCE:
				p		= ISO::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					uint32	count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).begin() : ~count == 0 ? iterator() : iterator(Browser::iterator(virt, data, 0), p);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

Browser2::iterator Browser2::end() const {
	auto	p		= this->p;
	void	*data	= this->data;
	for (const Type *type = this->type; type;) {
		switch (type->GetType()) {
			case COMPOSITE: {
				TypeComposite	&comp	= *(TypeComposite*)type;
				return iterator(Browser::iterator((uint8*)data + comp.GetSize(), comp.end()), p);
			}
			case ARRAY: {
				TypeArray		&array	= *(TypeArray*)type;
				return iterator(Browser::iterator(array.subtype, array.get_memory(data).end(), array.subsize), p);
			}
			case OPENARRAY: {
				TypeOpenArray	&array	= *(TypeOpenArray*)type;
				return iterator(Browser::iterator(array.subtype, array.get_memory(data).end(), array.subsize), p);
			}
			case REFERENCE:
				p		= ISO::GetPtr(type->ReadPtr(data));
				data	= p;
				type	= p.IsExternal() ? 0 : p.GetType();
				break;

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;

			case VIRTUAL:
				if (!type->Fixed()) {
					Virtual	*virt	= (Virtual*)type;
					uint32	count	= virt->Count(virt, data);
					return count == 0 ? virt->Deref(virt, data).end() : ~count == 0 ? iterator() : iterator(Browser::iterator(virt, data, count), p);
				}
				//fall through
			default:
				type = 0;
				break;
		}
	}
	return iterator();
}

//-----------------------------------------------------------------------------
//	Browser::s_setget
//-----------------------------------------------------------------------------

template<> bool Browser::s_setget<bool>::get(void *data, const Type *type, bool &t) {
	while (type) {
		switch (type->GetType()) {
			case INT:
			case FLOAT:
				t = read_bytes<uint64>(data, type->GetSize());
				return true;

			case STRING:
				if (const void *p = type->ReadPtr(data)) {
					from_string((const char*)p, t);
					return true;
				}
				break;
			case REFERENCE: {
				void *p	= type->ReadPtr(data);
				type	= !p || GetHeader(p)->IsExternal() ? 0 : GetHeader(p)->GetType();
				data	= p;
				break;
			}
			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;
			case VIRTUAL:
				if (!type->Fixed())
					return ((Virtual*)type)->Deref((Virtual*)type, data).Read(t);
				//fall through
			default:
				return false;
		}
	}
	return false;
}

bool Browser::s_setget<const char*>::get(void *data, const Type *type, const char *&t) {
	while (type) {
		switch (type->GetType()) {
			case STRING:
				t = (const char*)type->ReadPtr(data);
				return true;
			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;
			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;
			case VIRTUAL:
				if (!type->Fixed())
					return ((Virtual*)type)->Deref((Virtual*)type, data).Read(t);
				//fall through
			default:
				return false;
		}
	}
	return false;
}

bool Browser::s_setget<const char*>::set(void *data, const Type *type, const char *t) {
	while (type) {
		switch (type->GetType()) {
			case INT:
				if (is_signed_int(t)) {
					((TypeInt*)type)->set(data, from_string<int64>(t));
					return true;
				}
				return false;

			case FLOAT: {
				float	f;
				if (from_string(t, f)) {
					((TypeFloat*)type)->set(data, f);
					return true;
				}
				return false;
			}

			case STRING:
				((TypeString*)type)->set(data, t);
				return true;

			case REFERENCE: {
				tag2 id;
				if (void *data2 = type->ReadPtr(data)) {
					if (auto type2 = GetHeader(data2)->IsExternal() ? 0 : GetHeader(data2)->GetType()) {
						data = data2;
						type = type2;
						break;
					}
					auto v = (Value*)data2 - 1;
					id = v->ID();
					if (!allocate<32>::free(v))
						allocate<64>::free(v);

				}
				ptr_string<char,32>	*p = MakeRawPtr<32>(id, ptr_string<char,32>(t));
				GetHeader(p)->addref();
				type->WritePtr(data, p);
				return true;
			}

			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;
			case VIRTUAL:
				if (!type->Fixed())
					return ((Virtual*)type)->Deref((Virtual*)type, data).Set(t);
				//fall through
			default:
				return false;
		}
	}
	return false;
}

bool Browser::s_setget<string>::get(void *data, const Type *type, string &t) {
	while (type) {
		switch (type->GetType()) {
			case INT: {
				auto	b	= build(t);
				TypeInt	*ti	= (TypeInt*)type;
				int64	i	= ti->get64(data);
				if (ti->frac_bits())
					b << i / float(ti->frac_factor());
				else
					b << i;
				return true;
			}
			case FLOAT: {
				auto		b	= build(t);
				TypeFloat	*tf	= (TypeFloat*)type;
				if (tf->num_bits() > 32)
					b << tf->get64(data);
				else
					b << tf->get(data);
				return true;
			}
			case STRING: {
				TypeString	*ts = (TypeString*)type;
				const void	*p = ts->ReadPtr(data);
				switch (ts->log2_char_size()) {
					default:	t = (const char*)p; break;
					case 1:		t = (const char16*)p; break;
					case 2:		t = (const char32*)p; break;
					case 3:		t = (const uint64*)p; break;
				}
				return true;
			}

			case REFERENCE:
				data	= type->ReadPtr(data);
				type	= !data || GetHeader(data)->IsExternal() ? 0 : GetHeader(data)->GetType();
				break;

			case ARRAY: {
				const Type *subtype = type->SubType()->SkipUser();
				if (subtype && subtype->GetType() == INT && (subtype->flags & TypeInt::CHR)) {
					uint32	len = ((TypeArray*)type)->Count();
					switch (subtype->GetSize()) {
						case 1:	t = count_string((const char*)data, len); return true;
						case 2:	t = count_string16((const char16*)data, len); return true;
					}
				}
				return false;
			}
			case OPENARRAY: {
				const Type *subtype = type->SubType()->SkipUser();
				if (TypeType(subtype) == INT && (subtype->flags & TypeInt::CHR)) {
					void			*data2	= type->ReadPtr(data);
					uint32			len		= GetCount(OpenArrayHead::Get(data2));
					switch (subtype->GetSize()) {
						case 1:	t = count_string((const char*)data2, len); return true;
						case 2:	t = count_string16((const char16*)data2, len); return true;
					}
				}
				return false;
			}
			case USER:
				type	= ((TypeUser*)type)->subtype;
				break;
			case VIRTUAL:
				if (!type->Fixed())
					return ((Virtual*)type)->Deref((Virtual*)type, data).Read(t);
				//fall through
			default:
				return false;
		}
	}
	return t;
}

//-----------------------------------------------------------------------------
//	BitPacked
//-----------------------------------------------------------------------------

ptr<uint64> BitPacked::GetTemp(void *tag) {
	static struct TEMP {
		ptr<uint64>	p;
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
	return ptr<uint64>();
}

int BitPacked::GetIndex(uint8 &data, const tag2 &id, int from) {
	for (uint32 i = from; i < count; i++)
		if (id == (*this)[i].id.get_tag())
			return i;
	return -1;
}
Browser2 BitPacked::Index(uint8 &data, int i) {
	Element		&e	= (*this)[i];
	ptr<uint64> t	= GetTemp(&data + i);

	uint64	*p	= (uint64*)(&data + e.offset / 8);
	*t			= extract_bits(*p, e.offset & 7, e.size);
	return Browser2(e.type, t, t);
}
bool BitPacked::Update(uint8 &data, const char *spec, bool from) {
	if (from)
		return false;
	int				i	= from_string<int>(spec + 1);
	Element		&e	= (*this)[i];
	ptr<uint64> t	= GetTemp(&data + i);
	write_bits(&data + e.offset / 8, *t, e.offset & 7, e.size);
	return true;
}

template<typename T> void* base_fixed<T>::base;
template<> void*	base_fixed<void>::get_base() {
	static bool init = !!(base = allocate<32>::alloc(1));
	return (void)init, base;
}

template<typename T, int B> void* base_select<T, B>::base[(1 << B) + 1] = {base, 0};
template<> void**	base_select<void, 2>::get_base() {
	static bool init = !!(base[1] = base_fixed<void>::get_base());
	return (void)init, base;
}
template<> void base_select<void, 2>::set(const void* p) {
	if (!p) {
		offset = 0;
		return;
	}
	void** base = get_base();
	int	i	= 0;
	for (i = 0; base[i]; ++i) {
		intptr_t v = (char*)p - (char*)base[i];
		if (int32(v) == v) {
			offset = int32(v) | i;
			return;
		}
	}
	if (i < 4) {
		base[i] = (void*)p;
		offset  = i;
		return;
	}
	ISO_ASSERT(0);
}

const char *store_string(const char *id) {
	static hash_map<uint32, const char*, true>	h;

	if (!id)
		return id;

	uint32		c	= CRC32C::calc(id);
	auto		s	= h[c];
	if (!s) {
		size_t	len	= strlen(id) + 1;
		void	*p	= allocate<32>::alloc(len, 1);
		memcpy(p, id, len);
		s = (const char*)p;
	}
	return s;
}

}//namespace ISO

namespace iso {

vallocator&	allocator32() { return ISO::allocate<32>::allocator(); }

iso_export void*	get32(int32 v)			{ return (char*)ISO::base_fixed<void>::get_base() + (intptr_t(v) << 2); }
iso_export int32	put32(const void *p)	{ return ISO::base_fixed_shift<void, 2>::calc_check(p); }

void Init(crc32 *x, void *physram) {
	auto	*s = (ISO::ptr_string<char,32>*)x;
	*(uint32*)x = *s ? CRC32::calc(*s) : 0;
}
void DeInit(crc32 *x) {}

static initialise init(ISO::getdef<crc32>());

}  // namespace iso

