#include "iso_binary.h"
#include "iso_files.h"
#include "iso_convert.h"

extern "C" {void glFlush();}

namespace ISO {

BinaryData2	binary_data;

//-----------------------------------------------------------------------------
//	BinaryData_writer
//-----------------------------------------------------------------------------

uint64 hash(const Type *type) {
	uint64	u = (uintptr_t)type / alignof(Type);
	if (auto *sub = type->SubType())
		u ^= (sub - type) * 9;

	switch (type->GetType()) {
		case ARRAY:
			u ^= reverse_bits<uint64>(((TypeArray*)type)->count);
			break;
		case COMPOSITE:
			u ^= reverse_bits<uint64>(((TypeComposite*)type)->count);
			break;
		default:
			break;
	}
	return u;
}

class BinaryData_writer {
	filename				fn;
	ostream_ref				file;
	uint64					offset;
	iso::flags<BIN_FLAGS>	flags;

	hash_map<const Type*, uint32>					types;
	hash_map<void*, pair<WeakPtr<void>, uint32>>	pointers;

	bool		ExpandUser(const TypeUser *user) const {
		return flags.test(BIN_WRITEALLTYPES)
			|| user->Fixed()
			|| (flags.test(BIN_WRITEREADTYPES) && (user->flags & TypeUser::WRITETOBIN));
	}

	template<typename T> void DumpEnums(const TypeEnumT<T> *type);
	void		DumpType(FileValue &Value, const Type *type);
	uint32		DumpType(const Type *type);
	void		DumpTypes(const Browser &b);
	void		DumpData(const Browser &b, bool big, bool flip);
	Browser2	GetVirtual(const Browser &b) const;
public:
	BinaryData_writer(const char *fn, ostream_ref file, uint32 flags = 0) : fn(fn), file(file), flags(flags) {
		this->fn.cleanup();
	}
	void		Dump(ptr_machine<void> p);
};

Browser2 BinaryData_writer::GetVirtual(const Browser &b) const {
	if (Browser2 b2 = *b) {
		if (b2.SkipUser().GetType() == VIRTUAL)
			return b2;

		ptr_machine<void>	p = b2.TryPtr();
		if (!p)
			p = b2.Duplicate();
		return MakeBrowser(move(p));

	} else if (int c = b.Count()) {
		ptr<anything>	a(0, c);
		for (int i = 0; i < c; i++)
			(*a)[i] = b[i];
		return MakeBrowser(move(a));
	}

	return Browser2();
}

void BinaryData_writer::Dump(ptr_machine<void> p) {
	FILE_HEADER	header;
	streamptr	here	= file.tell();

	if (p.Flags() & Value::EXTERNAL)
		p = FileHandler::ReadExternal(p);

	Browser2	b(p);
	if (b.GetType() == VIRTUAL)
		b = GetVirtual(b);


	offset	= here + sizeof(header) + GetSize(b.GetTypeDef());

	DumpType(header, b.GetTypeDef());
	//dump all referenced types (so they're near start of file)
	DumpTypes(b);

	file.seek(sizeof(header));

	bool	big = flags.test(BIN_BIGENDIAN);
	if (big)
		header.flags |= Value::ISBIGENDIAN;
	header.flags |= p.Flags() & Value::HASEXTERNAL;
	DumpData(b, big, !(p.Flags() & Value::ISBIGENDIAN) == big);

	(uint32le&)header.user = align(offset, 32);
	file.seek(here);
	file.write(header);
	file.seek(offset);
	file.align(32, 0);
}

template<typename T> void BinaryData_writer::DumpEnums(const TypeEnumT<T> *type) {
	bool	crc_ids = !!(type->flags & type->CRCIDS);
	int		n		= type->size();

	offset		+= type->calc_size(n) + 4;

	for (auto &x : *type) {
		if (!crc_ids && flags.test(BIN_STRINGIDS)) {
			streamptr	here	= file.tell();
			uint32		len		= x.id.get_tag().size32() + 1;
			file.seek(offset);
			file.writebuff(x.id.get_tag(), len);
			file.seek(here);
			(uint32le&)x.id		= offset;
			offset	+= len;
		} else {
			(uint32le&)x.id	= x.id.get_crc(crc_ids);
		}
		file.write(x);
	}
	file.write((uint32)0);
}

uint32 BinaryData_writer::DumpType(const Type *type) {
	if (!type)
		return 0;

	if (type->GetType() == VIRTUAL)
		type = getdef<ptr<void> >();

	if (uint32 loc = types[type])
		return loc;

	offset	= (offset + 3) & ~3ull;
	uint32		ret		= offset;
	streamptr	here	= file.tell();
	file.seek(offset);

	types[type] = offset;

	Type type2	= *type;
	type2.flags		&= Type::TYPE_MASKEX | 0xff00;

	switch (type->GetType()) {
		case INT:
			if ((type->flags & TypeInt::ENUM) && flags.test(BIN_ENUMS)) {
				if (!flags.test(BIN_STRINGIDS))
					type2.flags |= TypeEnumT<uint32>::CRCIDS;
				file.write(type2);
				if (((TypeInt*)type)->num_bits() > 32)
					DumpEnums((TypeEnumT<uint64>*)type);
				else
					DumpEnums((TypeEnumT<uint32>*)type);
				break;
			}
			type2.flags &= ~TypeInt::ENUM;
			//fallthrough
		case FLOAT:
		case STRING:
		case VIRTUAL:
			file.write(type2);
			offset = file.tell();
			break;

		case COMPOSITE: {
			TypeComposite	&comp	= *(TypeComposite*)type;
			bool			crc_ids	= !!(type->flags & TypeComposite::CRCIDS);
			offset	+= comp.calc_size(comp.Count());
			if (!flags.test(BIN_STRINGIDS))
				type2.flags |= TypeComposite::CRCIDS;
			file.write(type2);
			file.write(uint32le(comp.Count()));
			for (auto e : comp) {
				if (!e.id.blank() && !crc_ids && flags.test(BIN_STRINGIDS)) {
					streamptr	here	= file.tell();
					uint32		len		= e.id.get_tag().size32() + 1;
					file.seek(offset);
					file.writebuff(e.id.get_tag(), len);
					file.seek(here);
					(uint32le&)e.id		= offset;
					offset				+= len;
				} else {
					(uint32le&)e.id		= e.id.get_crc(crc_ids);//(tag&)crc32(e.id);
				}
				(uint32le&)e.offset		= e.offset;
				(uint32le&)e.size		= e.size;
				(uint32le&)e.type		= DumpType(e.type);
				file.write(e);
			}
			break;
		}
		case ARRAY: {
			offset += sizeof(TypeArray);
			TypeArray	*a = (TypeArray*)type;
			file.write(type2);
			file.write(uint32le(a->count));
			file.write(DumpType(a->subtype));
			file.write(uint32le(a->subsize));
			break;
		}
		case OPENARRAY: {
			offset += sizeof(TypeOpenArray);
			TypeOpenArray	*a = (TypeOpenArray*)type;
			file.write(type2);
			file.write(DumpType(a->subtype));
			file.write(uint32le(a->subsize));
			break;
		}
		case REFERENCE: {
			offset += sizeof(TypeReference);
			TypeReference	*a = (TypeReference*)type;
			file.write(type2);
			file.write(DumpType(a->subtype));
			break;
		}
//		case FUNCTION:
		case USER: {
			offset += sizeof(TypeUser);
			TypeUser	*a = (TypeUser*)type;
			type2.flags &= ~TypeUser::INITCALLBACK;
			if (!flags.test(BIN_STRINGIDS) || (a->flags & TypeUser::CRCID))
				type2.flags |= TypeUser::CRCID;
			file.write(type2);
			if (type2.flags & TypeUser::CRCID) {
//				uint32 	c = a->id.get_crc(a->flags & TypeUser::CRCID);
				file.write(a->id.get_crc(a->flags & TypeUser::CRCID));
			} else {
				file.write(uint32le(offset));
				streamptr	here	= file.tell();
				uint32		len		= a->id.get_tag().size32() + 1;
				file.seek(offset);
				file.writebuff(a->id.get_tag(), len);
				file.seek(here);
				offset		+= len;
			}
			if (ExpandUser(a))
				file.write(DumpType(a->subtype));
			else
				file.write(0);
			break;
		}
		default:
			break;
	}

	file.seek(here);
	return ret;
}

void BinaryData_writer::DumpType(FileValue &Value, const Type *type) {
	if (type && type->GetType() == USER && !ExpandUser((const TypeUser*)type)) {
		Value.type	= ((TypeUser*)type)->ID().get_crc32();
		Value.flags	|= Value::CRCTYPE;
	} else {
		Value.type	= DumpType(type);
	}
}

void BinaryData_writer::DumpTypes(const Browser &_b) {
	if (Browser b = _b.SkipUser()) {
		const Type	*type	= b.GetTypeDef();
		if (type->GetType() == REFERENCE) {
			Browser				b2	= *b;
			ptr_machine<void>	p	= b2;
			if (p) {
				if (!((TypeReference*)type)->subtype && !p.HasCRCType()) {
					auto subtype = p.GetType();
					if (subtype && (subtype->GetType() != USER || ExpandUser((const TypeUser*)subtype)))
						DumpType(subtype);
				}
			}
		}
	}
}


void BinaryData_writer::DumpData(const Browser &_b, bool big, bool flip) {
	Browser		b		= _b.SkipUser();
	if (!b)
		return;
	
	const Type	*type	= b.GetTypeDef();
	void		*data	= b;

	if (type->IsPlainData(flip)) {
		file.writebuff(data, GetSize(b.GetTypeDef()));
		return;
	}

	switch (type->GetType()) {
		case INT:
		case FLOAT:
			switch (uint32 size = type->GetSize()) {
				case 2:	file.write(swap_endian(*(uint16*)data)); break;
				case 4:	file.write(swap_endian(*(uint32*)data)); break;
				case 8:	file.write(swap_endian(*(uint64*)data)); break;
				default:	// any other size we just write out (for now)
					file.writebuff(data, size);
					break;
			}
			break;

		case STRING: {
			if (const void *s = type->ReadPtr(data)) {
				if (type->Is64Bit()) {
					file.write(uint64le(offset));
				} else {
					offset	= align(offset, 4);	// so offset can be represent with base_fixed_shift<void,2>
					file.write(uint32le(offset));
				}
				TypeString	*stype	= (TypeString*)type;
				streamptr	here	= file.tell();
				file.seek(offset);
				file.writebuff(s, (stype->len(s) + 1) << stype->log2_char_size());
				offset = file.tell();
				file.seek(here);

			} else {
				if (type->Is64Bit())
					file.write(uint64(0));
				else
					file.write(uint32(0));
			}
			break;
		}
		case COMPOSITE: {
			TypeComposite	&comp	= *(TypeComposite*)type;
			streamptr		here	= file.tell();
			for (auto &i : comp) {
				file.seek(here + i.offset);
				if (i.type)
					DumpData(Browser(i.type, (char*)data + i.offset), big, flip);
				else
					file.writebuff((char*)data + i.offset, i.size);
			}
			break;
		}
		case VIRTUAL:
			DumpData(GetVirtual(b), big, flip);
			break;

		case ARRAY:
			for (int i = 0, c = b.Count(); i < c; i++)
				DumpData(b[i], big, flip);
			break;

		case OPENARRAY: {
			TypeOpenArray	*array = (TypeOpenArray*)type;
			if (int count = b.Count()) {
				offset = align(offset + 4, max(array->SubAlignment(), 4u));
				if (array->Is64Bit())
					file.write(uint64le(offset));
				else
					file.write(uint32le(offset));

				streamptr	start	= offset;
				streamptr	here	= file.tell();
				file.seek(offset - 4);
				file.write(uint32le(count));
				offset += array->subsize * count;
				if (array->subtype->IsPlainData(flip)) {
					file.writebuff(b[0], count * array->subsize);
				} else {
					for (int i = 0; i < count; i++) {
						file.seek(start + i * array->subsize);
						DumpData(b[i], big, flip);
					}
				}
				file.seek(here);
			} else {
				if (array->Is64Bit())
					file.write(uint64(0));
				else
					file.write(uint32(0));
			}
			break;
		}
		case REFERENCE: {
			Browser				b2	= *b;
			ptr_machine<void>	p	= b2;
			uint64				location = 0;
			if (p) {
				const Type	*subtype0	= ((TypeReference*)type)->subtype;
				const Type	*subtype	= subtype0;
				bool		duped		= p.Header()->refs > 2;

				if (!subtype)
					subtype = p.GetType();

				if (duped)
					location = pointers[p]->b;

				if (!location && !p.HasCRCType()) {
					WeakPtr<void>	wp;
					if (duped)
						wp = p;

					if (!flags.test(BIN_DONTCONVERT))
						p = Conversion::convert(p, subtype0, (flags.test(BIN_EXPANDEXTERNALS) ? Conversion::EXPAND_EXTERNALS : Conversion::ALLOW_EXTERNALS) | Conversion::RECURSE);

					else if (flags.test(BIN_EXPANDEXTERNALS))
						p = FileHandler::ExpandExternals(p);

					if (p && !(p.Flags() & Value::CRCTYPE)) {
						streamptr	here	= file.tell();
						FileValue	Value;
						Value.flags	= p.Flags() & (Value::EXTERNAL | Value::HASEXTERNAL | Value::ISBIGENDIAN | Value::ALWAYSMERGE | Value::REDIRECT);

						flip = !(Value.flags & Value::ISBIGENDIAN) == big;
						if (flip)
							Value.flags ^= Value::ISBIGENDIAN;

						if (tag2 id2 = p.ID()) {
							tag	id	= id2;
							if (flags.test(BIN_STRINGIDS) && id) {
								uint32	len	= uint32(strlen(id) + 1);
								file.seek(offset);
								file.writebuff(id, len);
								Value.id	= offset;
								offset		+= len;
							} else {
								crc32		c = id2.get_crc32();
								Value.flags	|= Value::CRCID;
								Value.id	= c;
							}
						}

						DumpType(Value, p.GetType());
						location = align(offset, max(p.GetType()->GetAlignment(), 4u));

						if (duped)
							pointers[wp] = make_pair(wp, location);

						file.seek(location);

						if (Value.flags & Value::EXTERNAL) {
							filename	f((char*)p);
							if (flags.test(BIN_RELATIVEPATHS))
								f = f.relative_to(fn);

							uint32	size	= f.size32() + 1;
							offset	= location + sizeof(Value) + size;
							file.write(Value);
							file.writebuff(f, size);
						} else {
							offset	= location + sizeof(Value) + GetSize(p.GetType());
							file.write(Value);
							DumpData(Browser(p), big, flip);
						}
						file.seek(here);
					}
				}
			}
			if (location)
				location += sizeof(Value);
			file.write(uint32le(location));
			break;
		}
		default:
			break;
	}
}

size_t BinaryData::Write(ostream_ref file) const {
	return ram_total ? file.writebuff(ram_buffer, ram_total) : 0;
}

bool BinaryData::Write(const ptr_machine<void> &p, ostream_ref file, const char *fn, uint32 flags) {
	if (file.exists()) {
		BinaryData_writer(fn, file, flags).Dump(p);
		return Write(file) == ram_total;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	BinaryData_reader
//-----------------------------------------------------------------------------

struct BinaryData_fixer {
	filename				fn;
	uint8*					start;
	void					*phys_ram;
	iso::flags<BIN_FLAGS>	flags;

	ptr_machine<void>	FixPtr(Value *value);

	ptr_machine<void>	Fix(Value *value, uint32 phys_size, void *_phys_ram) {
		start		= (uint8*)value;
		phys_ram	= _phys_ram;

		ptr_machine<void>	p = FixPtr(value);
		p.UserInt() = iso_bin_allocator().fix(phys_ram, phys_size);
		return p;
	}

	BinaryData_fixer(const char *_fn, uint32 flags)	: fn(_fn), flags(flags)	{ fn.cleanup(); }
};

template<typename T> class Fixer : T {
	BinaryData_fixer *fixer;
	bool	has_external;
	bool	needs_dupe;
public:
	Fixer(BinaryData_fixer *fixer) : fixer(fixer), has_external(false), needs_dupe(false)	{}
	bool	FixData(const Type *type, void *data);
	bool	HasExternal()	const	{ return has_external; }
	bool	NeedsDupe()		const	{ return needs_dupe; }
};

bool FixType(type_ptr &_type, uint8 *start);

const Type *MakeType(const Type *type) {
	switch (type->GetType()) {
		case COMPOSITE: {
			TypeComposite	*comp	= (TypeComposite*)type;
			int				count	= comp->Count();
			bool			diff	= false;
			TypeComposite	*comp2	= new(count) TypeComposite(0, Type::FLAGS(comp->flags & ~TypeComposite::DEFAULT));
			for (auto &e : *comp) {
				auto	etype	= MakeType(e.type);
				diff	= diff || etype != e.type;
				comp2->Add(etype, e.id);
			}
			if (diff)
				return comp2;
			iso::free(comp2);
			break;
		}

		case ARRAY: {
			TypeArray		*array		= (TypeArray*)type;
			const Type		*subtype	= MakeType(array->subtype);
			if (subtype != array->subtype)
				return new TypeArray(subtype, array->count);
			break;
		}

		case OPENARRAY: {
			TypeOpenArray	*array		= (TypeOpenArray*)type;
			const Type		*subtype	= MakeType(array->subtype);
			if (subtype != array->subtype)
				return new TypeOpenArray(subtype);
			break;
		}

		case USER:
			if (type->Dodgy()) {
				TypeUser	*user	= (TypeUser*)type;
				TypeUser	*user2	= user_types.Find(user->ID());
				return user2 ? user2 : new TypeUser(user->ID(), MakeType(user->subtype));
			}
			break;

		default:
			break;
	}
	return type;
}

template<typename T> void FixEnums(TypeEnumT<T> *type, uint8 *start) {
	bool	crc_ids = !!(type->flags & type->CRCIDS);
	for (auto &i : *type) {
		uint32 t	= (uint32le&)i.id;
		i.id		= crc_ids ? (tag1&)t : tag((char*)start + t);
	}
}

bool FixType2(Type *type, uint8 *start) {
	switch (type->GetType()) {
		case INT:
			if (type->flags & TypeInt::ENUM) {
				if (((TypeInt*)type)->num_bits() > 32)
					FixEnums((TypeEnumT<uint64>*)type, start);
				else
					FixEnums((TypeEnumT<uint32>*)type, start);
			}
			break;

		case COMPOSITE: {
			TypeComposite	&comp	= *(TypeComposite*)type;
			bool			crc_ids	= !!(type->flags & TypeComposite::CRCIDS);
			bool			ok		= true;

			comp.count	= (uint32le&)comp.count;
			for (auto &e : comp) {
				uint32 t	= (uint32le&)e.id;
				e.id		= crc_ids || t == 0 ? (tag1&)t : tag((char*)start + t);
				e.offset	= (uint32le&)e.offset;
				e.size		= (uint32le&)e.size;
				ok			= ok & FixType(e.type, start);
			}
			return ok;
		}
		case ARRAY: {
			TypeArray	*array = (TypeArray*)type;
			array->count	= (uint32le&)array->count;
			array->subsize	= (uint32le&)array->subsize;
			return FixType(array->subtype, start);
		}
		case OPENARRAY: {
			TypeOpenArray	*array	= (TypeOpenArray*)type;
			array->subsize	= (uint32le&)array->subsize;
			return FixType(array->subtype, start);
		}
		case REFERENCE:
			return FixType(((TypeReference*)type)->subtype, start);

		case USER: {
			TypeUser	*user	= (TypeUser*)type;
			uint32 t	= (uint32le&)user->id;
			user->id	= (user->flags & TypeUser::CRCID) || t == 0 ? (tag1&)t : tag((char*)(start + t));
			user->flags = (user->flags & ~TypeUser::INITCALLBACK) | TypeUser::FROMFILE | TypeUser::WRITETOBIN;
			return FixType(user->subtype, start);
		}

		default:
			break;
	}
	return true;
}

bool FixType(type_ptr &_type, uint8 *start) {
	if (!_type)
		return true;

	Type	*type	= (Type*)(start + (uint32le&)_type);
	_type			= type;

	if (type->GetType() == FLOAT && (type->param1 == 0 || (type->param1 == 32 && type->param2 == 8))) {
		_type = getdef<float>();
		return true;
	} else if (type->GetType() == STRING) {
		if ((sizeof(void*) == 8) == type->Is64Bit() && ((TypeString*)type)->log2_char_size() == 0)
			_type = getdef<string>();
		return true;
	}

	if (!type->Fixed()) {
		type->flags |= Type::TYPE_FIXED;
		if (!FixType2(type, start))
			type->flags |= Type::TYPE_DODGY;
	}

	if (type->Dodgy())
		return false;

	if (type->GetType() == USER) {
		TypeUser	*user	= (TypeUser*)type;
		if (TypeUser *user2 = user_types.Find(user->ID())) {
			if (!user2->subtype) {
				user2->subtype = Duplicate(user->subtype);
				_type = user2;
//				user->SetID(user2->ID());
//				user->flags	|= TypeUser::LOCALSUBTYPE | (user2->flags & TypeUser::INITCALLBACK);
				return true;

			} else if (user->subtype->SameAs(user2->subtype, MATCH_MATCHNULLS)) {
				_type = user2;
				return true;

			} else if (!(user2->Dodgy())) {
				type->flags |= Type::TYPE_DODGY;
				return false;
			}
		} else {
//			ISO_TRACEF("can't find matching user type with id=") << user->ID() << '\n';
		}
	}
	return true;
}

ptr_machine<void> BinaryData_fixer::FixPtr(Value *value) {
	const Type	*newtype	= NULL;

	if (!(value->flags & Value::PROCESSED)) {
		value->flags |= Value::PROCESSED;

		uint32 t	= (uint32le&)value->id;
		value->id	= (value->flags & Value::CRCID) || t == 0 ? (tag1&)t : tag((char*)(start + t));

		if (value->flags & Value::CRCTYPE) {
			crc32 crc = crc32((uint32le&)value->type);
			if (TypeUser *user = user_types.Find(tag2(crc))) {
				value->type = user;
				value->flags -= Value::CRCTYPE;
			} else {
				(crc32&)value->type = crc;
				ISO_TRACEF("can't find type with crc=0x%08X\n", (uint32)crc);
			}

		} else if (!FixType(value->type, start)) {
			newtype = MakeType(value->type);
		}
		
//		if (!newtype && value->type && value->type->GetType() == USER)
//			ISO_TRACEF("fixing:") << value->type << '\n';

		if (value->flags & Value::EXTERNAL) {
			filename	ext((const char*)(value + 1));
			if (flags.test(BIN_EXPANDEXTERNALS)) {
				ptr<void>	p = FileHandler::ReadExternal(fn.dir().relative(ext));
				if (p && !value->id.blank())
					p.SetID(value->ID());
				p.SetFlags(Value::EXTREF);
				*(void_ptr32*)(value + 1)	= p;
				value->flags |= Value::REDIRECT;
				return p;

			} else if (ext.is_relative()) {
				ptr<void>	p = MakePtrExternal(value->type, fn.dir().relative(ext), value->ID());
				*(void_ptr32*)(value + 1)	= p;
				value->flags |= Value::REDIRECT;
				return p;
			}

		} else if (!(value->flags & Value::CRCTYPE)) {
			saver<iso::flags<BIN_FLAGS> >	save_flags(flags);
			const Type	*type = value->type;
#ifdef CROSS_PLATFORM
			if (type && type->GetType() == USER && ((TypeUser*)type)->KeepExternals())
				flags.clear(BIN_EXPANDEXTERNALS).set(BIN_DONTCONVERT);
#endif

			if (value->flags & Value::ISBIGENDIAN) {
#ifndef ISO_BIGENDIAN
				value->flags -= Value::ISBIGENDIAN;
#endif
				Fixer<bigendian_types>	f(this);
				f.FixData(type, value + 1);
				if (f.HasExternal())
					value->flags |= Value::HASEXTERNAL;

			} else {
#ifdef ISO_BIGENDIAN
				value->flags |= Value::ISBIGENDIAN;
#endif
				Fixer<littleendian_types>	f(this);
				f.FixData(type, value + 1);
				if (f.HasExternal())
					value->flags |= Value::HASEXTERNAL;
				/*
				if (SkipUserType(type) == VIRTUAL) {
					Virtual		*virt = (Virtual*)SkipUser(type);
					if (auto size = virt->GetSize()) {
						auto*	pp	= (ptr<void>*)(value + 1);
						auto	p	= MakePtrSize(virt, value->ID(), size);
						if (virt->Convert(virt, p, pp->GetType(), *pp))
							return p;
					}
				}
				*/
			}

			if (newtype && newtype != value->type) {
				ISO_TRACEF("Mismatched type: ") << value->type << " -> " << newtype << "\n";
				ptr<void>	p = MakePtr(newtype, value->ID());
				p.Header()->flags |= value->flags & (Value::CRCID | Value::EXTERNAL | Value::HASEXTERNAL);
				Conversion::batch_convert(
					value + 1, 0, value->type,
					p, 0, newtype,
					1, !flags.test(BIN_DONTCONVERT), phys_ram
				);
				if (!GetPtr<void>(value + 1).Clear()) {	// if it gets freed, we can't (+don't need to) modify its header
					value->flags |= Value::REDIRECT;
					value->type	= NULL;
				}
				return p;
			}
		}
	}
	if (value->flags & Value::REDIRECT)
		return *(ptr<void>*)(value + 1);

	return GetPtr<void>(value + 1);
}

template<typename T> bool Fixer<T>::FixData(const Type *type, void *data) {
	if (!type)
		return false;

	if (type->IsPlainData(!is_native_endian<uint32>()))
		return false;

//	type	= type->SkipUser();
	switch (type->GetType()) {
		case INT:
		case FLOAT:
			if (!is_native_endian<typename T::uint32>()) {
				switch (type->GetSize()) {
					case 2:	*(iso::uint16*)data = *(typename T::uint16*)data; return true;
					case 4:	*(iso::uint32*)data = *(typename T::uint32*)data; return true;
				}
			}
			return false;

		case STRING:
			if (type->Is64Bit()) {
				if (uint64 offset = *(uint64le*)data)
					*(void_ptr64*)data = fixer->start + offset;
			} else {
				if (uint32 offset = *(uint32le*)data) {
					void	*s = (char*)fixer->start + offset;
					type->WritePtr(data, s);
					if (offset & 3) {
						needs_dupe = true;
					//	s = ((TypeString*)type)->dup(s);
					}
				}
			}
			return true;

		case COMPOSITE: {
			TypeComposite	&comp	= *(TypeComposite*)type;
			bool			need	= false;
			for (auto &e : comp) {
				try {
					if (FixData(e.type, (char*)data + e.offset))
						need = true;
				} catch_all() { memset((char*)data + e.offset, 0, comp.GetSize() - e.offset); rethrow; }
			}
			return need;
		}
		case ARRAY: {
			TypeArray	*a = (TypeArray*)type;
			for (int i = 0, c = a->Count(); i < c; i++) {
				try {
				if (!FixData(a->subtype, (char*)data + a->subsize * i))
					return false;
				} catch_all() { memset((char*)data + a->subsize * i, 0, a->subsize * (c - i)); rethrow; }
			}
			return true;
		}
		case OPENARRAY:
			if (uint64 o = type->Is64Bit() ? *(uint64le*)data : *(uint32le*)data) {
				uint8			*d		= fixer->start + o;
				TypeOpenArray	*a		= (TypeOpenArray*)type;
				OpenArrayHead	*h		= OpenArrayHead::Get(d);
				iso::uint32		c		= endian(GetCount(h), false);
				iso::uint32		align	= a->subtype->GetAlignment();
				a->param1		= log2_ceil(align);
				h->SetCount(c);
				h->SetBin(true);
				type->WritePtr(data, d);
				for (int i = 0; i < c; i++) {
					try {
					if (!FixData(a->subtype, h->GetElement(i, a->subsize)))
						break;
					} catch_all() { h->SetCount(i); rethrow; }
				}
			}
			return true;

		case REFERENCE:
			if (*(uint32*)data) {
				ptr_machine<void>	p	= fixer->FixPtr((Value*)(fixer->start + *(uint32le*)data) - 1);
				if (!type->Is64Bit())
					p.SetFlags(Value::MEMORY32);

				if (p.Flags() & (Value::EXTERNAL | Value::HASEXTERNAL))
					has_external = true;
#ifdef ISO_EDITOR
				if (!(fixer->flags.test(BIN_DONTCONVERT)) && !p.IsType(((TypeReference*)type)->subtype, MATCH_MATCHNULLS))
					p = Conversion::convert(p, ((TypeReference*)type)->subtype);
#endif
				if (type->Is64Bit())
					new(data) ptr<void,64>(p);
				else
					new(data) ptr<void,32>(p);
			}
			return true;

		case USER: {
			TypeUser	*user	= (TypeUser*)type;
			bool		need	= FixData(user->subtype, data);
			if (type->flags & TypeUser::INITCALLBACK) {
				if (type->flags & TypeUser::LOCALSUBTYPE)
					user = user_types.Find(user->ID());
				((TypeUserCallback*)user)->init(data, fixer->phys_ram);
				need = true;
			}
			return need;
		}

		case VIRTUAL:
			if (*(uint32*)data) {
				ptr_machine<void>	p		= fixer->FixPtr((Value*)(fixer->start + *(uint32le*)data) - 1);
				Virtual				*virt	= (Virtual*)type;
				virt->Convert(virt, data, p.GetType(), p);
				/*
				if (type->Is64Bit())
					new(data) ptr<void,64>(p);
				else
					new(data) ptr<void,32>(p);

				Virtual		*virt	= (Virtual*)type;
				virt->Update(virt, data, 0, false);
				*/
			}
			return true;

		default:
			break;
	}
	return false;
}

Value *BinaryData2::_ReadRaw(istream_ref file, vallocator &allocator, uint32 &phys_size, void *&phys_ram, bool directread) {
	uint64		total	= file.length();
	FILE_HEADER	header	= file.get();
	uint64		length	= (total >> 32) || !header.user ? total : (uint32)(uint32le&)header.user;
	uint8		*buffer	= (uint8*)allocator.alloc(length);

	if (file.readbuff(buffer + sizeof(FILE_HEADER), length - sizeof(FILE_HEADER)) != length - sizeof(FILE_HEADER)) {
		allocator.free(buffer);
		return 0;
	}

	header.id		= 0;
	header.refs		= 0;
	header.flags	|= Value::ROOT;
	header.user		= 0;

	phys_ram	= 0;
	if (phys_size = total -= length) {
		if (!(phys_ram = iso_bin_allocator().alloc(total, 256))) {
			allocator.free(buffer);
			return 0;
		}
		char	*phys	= (char*)phys_ram;
		if (directread) {
			if (file.readbuff(phys, total) != total) {
				iso_bin_allocator().free(phys_ram);
				allocator.free(buffer);
				return 0;
			}
		} else {
			uint32	chunk;
			void	*temp;
			for (chunk = total; !(temp = aligned_alloc_unchecked(chunk, 128)); chunk = (chunk / 2 + 15) & ~15);

			while (total) {
				uint32	chunk2 = min(total, chunk);
				if (file.readbuff(temp, chunk2) != chunk2) {
					aligned_free(temp);
					iso_bin_allocator().free(phys_ram);
					allocator.free(buffer);
					return ISO_NULL;
				}
				iso_bin_allocator().transfer(phys, temp, chunk2);
				total	-= chunk2;
				phys	+= chunk2;
			}
			aligned_free(temp);
		}
	}

	*(FileValue*)buffer	= header;
	return (Value*)buffer;
}

ptr_machine<void> BinaryData2::_Fixup(Value *header, uint32 flags, const char *fn, void *phys_ram, uint32 phys_size) {
	return BinaryData_fixer(fn, flags).Fix(header, phys_size, phys_ram);
}

}//namespace iso
