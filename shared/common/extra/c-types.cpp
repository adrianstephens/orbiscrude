#include "c-types.h"
#include "base/bits.h"
#include "base/soft_float.h"

using namespace iso;

const C_type	C_types::dummy(C_type::UNKNOWN);

uint32 C_type_composite::alignment() const {
	uint32	a = 1;
	for (auto *i = elements.begin(), *e = elements.end(); i != e; ++i) {
		if (!i->is_static())
			a = align(a, i->type->alignment());
	}
	return a;
}

C_element *C_type_struct::add(string_ref id, const C_type* type, C_element::ACCESS access, bool is_static) {
	if (is_static)
		return add_static(move(id), type, access);

	size_t	offset	= 0;
	if (elements) {
		C_element	&last	= elements.back();
		if (type->packed() && last.type->packed() && last.type->type == C_type::INT) {
			offset			= (last.offset << 3) + last.shift + ((C_type_int*)last.type)->num_bits();
			_size			= (offset + ((C_type_int*)type)->num_bits() + 7) >> 3;
			auto	*e		= C_type_composite::add(move(id), type, offset >> 3, access);
			e->shift		= offset & 7;
			return e;
		}

		offset	= last.offset + last.type->size();
		if (!(flags & PACKED))
			offset = align(offset, type->alignment());
	}

	_size	= offset + type->size();
	return C_type_composite::add(move(id), type, offset, access);
}

C_element *C_type_struct::add_atoffset(string_ref id, const C_type* type, uint32 offset, C_element::ACCESS access) {
	_size	= type ? max(_size, offset + type->size()) : 0;
	return C_type_composite::add(move(id), type, offset, access);
}

C_element *C_type_struct::add_atbit(string_ref id, const C_type* type, uint32 bit, C_element::ACCESS access) {
	//flags |= PACKED;
	auto	*e		= C_type_composite::add(move(id), type, bit >> 3, access);
	e->shift = bit & 7;
	_size	= max(_size, (bit + type->size() + 7) >> 3);
	return e;
}

C_type_struct::C_type_struct(string_ref id, FLAGS f, initializer_list<C_element> elements) : C_type_composite(STRUCT, f, move(id)) {
	for (auto &i : elements)
		add(i.id, i.type, C_element::ACCESS(i.access), false);
}


const C_element *C_type_composite::get(const char *id, const C_type *type) const {
	bool	foundid = false;
	for (const C_element *i = elements.begin(), *e = elements.end(); i != e; ++i) {
		if (i->id == id) {
			if (!type || i->type == type)
				return i;
			foundid = true;
		}
	}

	if (!foundid) {
		for (const C_element *i = elements.begin(), *e = elements.end(); i != e; ++i) {
			if (!i->id && i->type->type == STRUCT) {
				if (const C_element *j = ((C_type_struct*)i->type)->get(id))
					return j;
			}
		}
	}
	return 0;
}

const C_element *C_type_composite::at(uint32 offset) const {
	for (const C_element *i = elements.begin(), *e = elements.end(); i != e; ++i) {
		if (i->offset <= offset && i->offset + i->size() > offset)
			return i;
	}
	return 0;
}

const C_type* C_type_composite::returns(const char *fn) const {
	if (const C_element	*e = get(fn)) {
		if (e->type->type == C_type::FUNCTION)
			return ((C_type_function*)e->type)->subtype;
		return e->type;
	}
	return 0;
}

struct stack_depth2 {
	int			&depth;
	stack_depth2(int &depth, int max) : depth(depth) {
		ISO_ALWAYS_ASSERT(++depth < max);
	}
	~stack_depth2()			{ --depth; }
};

C_type *C_types::instantiate(const C_type *type, const char *name, const C_type **args) {
//	static recursion_checker<const C_type *>	recursion;
//	auto	rec_check	= recursion.check(type);
//	ISO_ASSERT(rec_check);
//	static	int depth;
//	auto	sd	= stack_depth2(depth, 10);

	if (!type)
		return 0;

	if (!(type->flags & TEMPLATED))
		return unconst(type);


	switch (type->type) {
		case C_type::TEMPLATE_PARAM:
			return unconst(args[type->flags & C_type::USER_FLAGS]);

		case C_type::TEMPLATE: {
			C_type_template		*temp	= (C_type_template*)type;
			const C_type		**args2 = alloc_auto(const C_type*, temp->args.size());

			buffer_accum<2048>	a;
			a << name;
			for (int i = 0, n = temp->args.size32(); i < n; i++) {
				args2[i] = instantiate(temp->args[i].type, temp->args[i].id, args);
				DumpType(a << (i == 0 ? '<' : ','), args2[i], 0, 0);
			}
			a << '>';

			name = a.term();
			if (type = lookup(name))
				return unconst(type);

			return unconst(add(name, instantiate(temp->subtype, name, args2)));
		}
		case C_type::STRUCT:
		case C_type::UNION:
		case C_type::NAMESPACE: {
			C_type_composite	*type1 = (C_type_composite*)type;
			auto	save_flags	= save(type1->flags, type1->flags - TEMPLATED);
			C_type_composite	type2(type1->type, type1->flags, string(type1->id));
			for (const C_element *i = type1->elements.begin(), *e = type1->elements.end(); i != e; ++i)
				type2.add(i->id, instantiate(i->type, i->id, args), i->is_static());
			return add(type2);
		}
		case C_type::ARRAY: {
			C_type_array	*type1	= (C_type_array*)type;
			const C_type	*sub	= instantiate(type1->subtype, name, args);
			if (sub == type1->subtype)
				return type1;
			return add(C_type_array(sub, type1->count));
		}
		case C_type::POINTER: {
			C_type_pointer	*type1	= (C_type_pointer*)type;
			const C_type	*sub	= instantiate(type1->subtype, name, args);
			if (sub == type1->subtype)
				return type1;
			return add(C_type_pointer(sub, type1->flags));
		}
		case C_type::FUNCTION: {
			C_type_function	*type1	= (C_type_function*)type;
			C_type_function	type2(instantiate(type1->subtype, name, args));
			for (const C_arg *i = type1->args.begin(), *e = type1->args.end(); i != e; ++i)
				type2.args.push_back(C_arg(i->id, instantiate(i->type, i->id, args)));
			type2.flags = type1->flags;
			return add(type2);
		}

		default:
			return unconst(type);
	}
}

uint64 C_types::inferable_template_args(const C_type *type) {
	if (!type || !(type->flags & TEMPLATED))
		return 0;

	switch (type->type) {
		case C_type::ARRAY:
		case C_type::POINTER:
			return inferable_template_args(type->subtype());

		case C_type::TEMPLATE_PARAM:
			return bit<uint64>(type->flags & C_type::USER_FLAGS);

		case C_type::TEMPLATE:
		case C_type::FUNCTION: {
			uint64		used	= 0;
			for (auto &i : ((C_type_withargs*)type)->args)
				used |= inferable_template_args(i.type);
			return used;
		}

		default:
			return 0;
	}
}

C_types::~C_types() {
	for (auto i = types.begin(), e = types.end(); i != e; ++i) {
		if (!((*i)->flags & C_type::STATIC))
			delete *i;
	}
}

CRC32Chash::CRC32Chash(const C_type &t) {
	switch (t.type) {
		default:				u = CRC32C::calc(&t, sizeof(C_type));break;
		case C_type::INT:		u = CRC32C::calc(&t, sizeof(C_type_int));break;
		case C_type::FLOAT:		u = CRC32C::calc(&t, sizeof(C_type_float));break;
		case C_type::STRUCT:
		case C_type::UNION:
		case C_type::NAMESPACE: {
			const C_type_composite &s = static_cast<const C_type_composite&>(t);
			//if (s.id) {
			//	u = CRC32C::calc(s.id);
			//} else {
				for (auto &i : s.elements) {
					u = CRC32C::calc(i.id, u);
					u = CRC32C::calc(i.u, u);
				}
//				u = CRC32C::calc(s.elements.begin(), s.elements.size() * sizeof(C_element));
			//}
			break;
		}
		case C_type::ARRAY:		u = CRC32C::calc(&t, sizeof(C_type_array));break;
		case C_type::POINTER:	u = CRC32C::calc(&t, sizeof(C_type_pointer));break;

		case C_type::FUNCTION:
		case C_type::TEMPLATE: {
			const C_type_withargs &f = static_cast<const C_type_withargs&>(t);
			u = CRC32::calc(&t, sizeof(C_type_withsubtype));
			for (auto &a : f.args)
				u = CRC32C::calc(&a.type, sizeof(a.type), u);
			break;
		}
	}
}

string_accum &iso::DumpData(string_accum &sa, const void *data, const C_type *type, int shift, int depth, FORMAT::FLAGS flags) {
	if (type && data) switch (type->type) {
		case C_type::ARRAY: {
			C_type_array	*a	= (C_type_array*)type;
			size_t			size = a->subsize;
			sa << '{';
			for (int i = 0, n = a->count; i < n; i++, data = (uint8*)data + size) {
				if (sa.remaining() < 20 + depth * 4)
					return sa << "...}";
				DumpData(sa << onlyif(i, ", "), data, a->subtype, shift, depth + 1, flags);
			}
			sa << '}';
			break;
		}
		case C_type::UNION:
		case C_type::STRUCT: {
			C_type_composite	*s	= (C_type_composite*)type;
			sa << '{';
			int	n	= 0;
			for (auto *i = s->elements.begin(), *e = s->elements.end(); i != e; ++i) {
				if (sa.remaining() < 40 + depth * 4)
					return sa << "...}";
				if (!i->is_static())
					DumpData(sa << onlyif(n++, ", "), (uint8*)data + i->offset, i->type, i->shift + shift, depth + 1, flags);
			}
			sa << '}';
			break;
		}
		case C_type::INT: {
			C_type_int		*i	= (C_type_int*)type;
			if (i->num_bits() > 64) {
				uint128		v	= *(uint128*)data;
				sa << v;
				break;
			}

			uint64	v	= (*(uint64*)data >> shift) & bits64(i->num_bits());
			if (i->is_enum()) {
				C_type_enum *e = (C_type_enum*)i;
				for (auto e0 = e->begin(), e1 = e->end(); e0 != e1; ++e0) {
					if (e0->value == v)
						return sa << e0->id;
				}
			}
			uint64	scale = i->scale();
			if (i->sign()) {
				int64	s = sign_extend(v, i->num_bits()) + int(v == 0 && i->no_zero());
				sa << ifelse(scale == 1, formatted(s, flags), formatted(float(s) / scale, flags));
			} else {
				v += int(v == 0 && i->no_zero());
				sa << ifelse(scale == 1, formatted(v, flags), formatted(float(v) / scale, flags));
			}
			break;
		}
		case C_type::FLOAT: {
			C_type_float	*f	= (C_type_float*)type;
			switch (f->num_bits()) {
				case 16:	sa << formatted(float(*(float16*)data), flags); break;
				case 32:	sa << formatted(*(float32*)data, flags); break;
				case 64:	sa << formatted(*(float64*)data, flags); break;
				case 80:	sa << formatted(*(float80*)data, flags); break;
			}
			break;
		}
		case C_type::POINTER: {
			C_type_pointer	*p	= (C_type_pointer*)type;
			sa << "0x" << hex(read_bytes<uint64>(data, uint32(p->size())));
			break;
		}
		case C_type::CUSTOM: {
			C_type_custom	*c = (C_type_custom*)type;
			c->output(sa, data);
			break;
		}
	}
	return sa;
}

string_accum &iso::DumpField(string_accum &sa, const C_type *type, uint32 offset, bool dot) {
	while (type) {
		switch (type->type) {
			case C_type::STRUCT:
				if (const C_element *e = ((const C_type_struct*)type)->at(offset)) {
					if (e->id) {
						if (dot)
							sa << '.';
						sa	<< e->id;
					}
					offset	-= e->offset;
					type	= e->type;
				} else {
					type	= 0;
				}
				break;
			case C_type::ARRAY: {
				type	= ((C_type_array*)type)->subtype;
				uint32	size	= uint32(type->size());
				sa << '[' << offset / size << ']';
				offset %= size;
				break;
			}
			default:
				type = 0;
				break;
		}
		dot = true;
	}
	if (offset)
		sa << '+' << offset;
	return sa;
}

string_scan &iso::SetData(string_scan &ss, void *data, const C_type *type, int shift) {
	switch (type->type) {
		case C_type::INT: {
			C_type_int		*i	= (C_type_int*)type;
			uint64			v;
			ss >> v;
			write_bits(data, v, shift, i->num_bits());
			break;
		}
		case C_type::FLOAT: {
			C_type_float	*f	= (C_type_float*)type;
			switch (f->num_bits()) {
				case 16:	{float	v; ss >> v; *(float16*)data = (float16)v; break;}
				case 32:	ss >> *(float32*)data; break;
				case 64:	ss >> *(float64*)data; break;
			}
			break;
		}
	}

	return ss;
}

bool iso::SetFloat(void *data, const C_type *type, double d, int shift) {
	if (type) switch (type->type) {
		case C_type::INT: {
			C_type_int		*i	= (C_type_int*)type;
			int64			v	= (int64)d;
			write_bits(data, v, shift, i->num_bits());
			return true;
		}
		case C_type::FLOAT: {
			C_type_float	*f	= (C_type_float*)type;
			switch (f->num_bits()) {
				case 16:	*(float16*)data = (float16)d; return true;
				case 32:	*(float32*)data = d; return true;
				case 64:	*(float64*)data = d; return true;
			}
		}
	}
	return false;
}


float iso::GetFloat(const void *data, const C_type *type, int shift, float def) {
	if (type) switch (type->type) {
		case C_type::INT: {
			C_type_int		*i	= (C_type_int*)type;
			if (i->sign()) {
				int64	v = mask_sign_extend(*(uint64*)data >> shift, i->num_bits());
				v		+= int(v == 0 && i->no_zero());
				return float(v) / i->scale();

			} else {
				uint64	v	= ((shift + i->num_bits() > 32 ? *(uint64*)data : *(uint32*)data) >> shift) & bits64(i->num_bits());
				v		+= int(v == 0 && i->no_zero());
				return float(v) / i->scale();
			}
		}
		case C_type::FLOAT: {
			C_type_float	*f	= (C_type_float*)type;
			switch (f->num_bits()) {
				case 16:	return float(*(float16*)data);
				case 32:	return *(float32*)data;
				case 64:	return *(float64*)data;
			}
			break;
		}
	}
	return def;
}

double iso::GetDouble(const void *data, const C_type *type, int shift, double def) {
	if (type) switch (type->type) {
		case C_type::INT: {
			C_type_int		*i	= (C_type_int*)type;
			uint64			v	= (*(uint64*)data >> shift) & bits64(i->num_bits());
			if (i->sign())
				v = sign_extend(v, i->num_bits());
			v += int(v == 0 && i->no_zero());
			return double(v) / i->scale();
		}
		case C_type::FLOAT: {
			C_type_float	*f	= (C_type_float*)type;
			switch (f->num_bits()) {
				case 16:	return float(*(float16*)data);
				case 32:	return *(float32*)data;
				case 64:	return *(float64*)data;
			}
			break;
		}
	}
	return def;
}

int64 iso::GetInt(const void *data, const C_type *type, int shift) {
	if (type) switch (type->type) {
		case C_type::INT: {
			C_type_int		*i	= (C_type_int*)type;
			uint64			v	= (*(uint64*)data >> shift) & bits64(i->num_bits());
			if (i->sign())
				v = sign_extend(v, i->num_bits());
			v += int(v == 0 && i->no_zero());
			return v;
		}
		case C_type::FLOAT: {
			C_type_float	*f	= (C_type_float*)type;
			switch (f->num_bits()) {
				case 16:	return float(*(float16*)data);
				case 32:	return *(float32*)data;
				case 64:	return *(float64*)data;
			}
			break;
		}
	}
	return 0;
}

number iso::GetNumber(const void *data, const C_type *type, int shift) {
	if (type) switch (type->type) {
		case C_type::INT: {
			C_type_int		*i	= (C_type_int*)type;
			uint64			v	= (*(uint64*)data >> shift) & bits64(i->num_bits());
			if (i->sign())
				v = sign_extend(v, i->num_bits());
			v += int(v == 0 && i->no_zero());
			return (int64)v;
		}
		case C_type::FLOAT: {
			C_type_float	*f	= (C_type_float*)type;
			switch (f->num_bits()) {
				case 16:	return float(*(float16*)data);
				case 32:	return *(float32*)data;
				case 64:	return *(float64*)data;
			}
			break;
		}
	}
	return 0;
}

int iso::NumChildren(const C_type *type) {
	if (!type)
		return 0;

	switch (type->type) {
		case C_type::ARRAY:
			return ((const C_type_array*)type)->count;

		case C_type::STRUCT:
			return ((C_type_struct*)type)->elements.size32();

		default:
			return 0;
	}
}

int iso::NumElements(const C_type *type) {
	if (!type)
		return 0;

	switch (type->type) {
		case C_type::ARRAY: {
			const C_type_array	*a = (const C_type_array*)type;
			return a->count * NumElements(a->subtype);
		}
		case C_type::STRUCT: {
			const C_type_struct	*s = (const C_type_struct*)type;
			int		n = 0;
			for (auto *i = s->elements.begin(), *e = s->elements.end(); i != e; ++i)
				n += NumElements(i->type);
			return n;
		}
		default:
			return 1;
	}
}

const C_type *iso::GetNth(void *&data, const C_type *type, int i, int &shift) {
	shift = 0;
	while (type) {
		switch (type->type) {
			case C_type::ARRAY: {
				const C_type_array	*a = (const C_type_array*)type;
				type	= a->subtype;
				int		n = NumElements(type);
				int		x = i / n;
				if (x >= a->count)
					return 0;
				data	= (uint8*)data + x * a->subsize;
				i		%= n;
				break;
			}
			case C_type::STRUCT: {
				const C_type_struct	*s = (const C_type_struct*)type;
				for (auto *e = s->elements.begin(), *e1 = s->elements.end(); e != e1; ++e) {
					int	n = NumElements(e->type);
					if (n > i) {
						data	= (uint8*)data + e->offset;
						type	= e->type;
						shift	= e->shift;
						break;
					}
					i	-= n;
				}
				break;
			}
			default:
				return type;
		}
	}
	return 0;
}

string_accum& iso::GetNthName(string_accum &sa, const C_type *type, int i) {
	while (type) {
		switch (type->type) {
			case C_type::ARRAY: {
				const C_type_array	*a = (const C_type_array*)type;
				type	= a->subtype;
				int		n = NumElements(type);
				int		x = i / n;
				if (x >= a->count)
					return sa;
				sa << '[' << x << ']';
				i		%= n;
				break;
			}
			case C_type::STRUCT: {
				const C_type_struct	*s = (const C_type_struct*)type;
				for (auto *e = s->elements.begin(), *e1 = s->elements.end(); e != e1; ++e) {
					int	n = NumElements(e->type);
					if (n > i) {
						sa << '.' << e->id;
						type	= e->type;
						break;
					}
					i	-= n;
				}
				break;
			}
			default:
				return sa;
		}
	}
	return sa;
}

const C_type *iso::Index(void *&data, const C_type *type, int64 i, int &shift) {
	switch (type->type) {
		case C_type::ARRAY: {
			const C_type_array	*a = (const C_type_array*)type;
			type	= a->subtype;
			if (i < a->count) {
				data	= (uint8*)data + i * a->subsize;
				return type;
			}
			break;
		}
		case C_type::STRUCT:
		case C_type::UNION: {
			auto comp = (const C_type_composite*)type;
			auto &e = comp->elements[i];
			data	= (uint8*)data + e.offset;
			shift	= e.shift;
			return e.type;
		}
		case C_type::POINTER: {
			type	= type->subtype();
			data	= (uint8*)data + i * type->size();
			return type;
		}
	}
	return 0;
}


const C_type *iso::GetField(void *&data, const C_type *type, const char *id, int &shift) {
	if (auto comp = type->composite()) {
		if (auto *e = comp->get(id)) {
			data	= (uint8*)data + e->offset;
			shift	= e->shift;
			return e->type;
		}
	}
	return 0;
}

C_value	C_value::cast(const C_type *new_type)	const {
	uint64	v2	= 0;

	if (new_type->type == C_type::FLOAT)
		SetFloat(&v2, new_type, *this);
	else if (is_float())
		v2 = int64((double)*this);
	else
		memcpy(&v2, &v, new_type->size());

	return C_value(new_type, v2);
}


