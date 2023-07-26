#include "iso_convert.h"
#include "iso_files.h"
#include "maths/graph.h"

namespace ISO {

//-----------------------------------------------------------------------------
//	Conversion
//-----------------------------------------------------------------------------


struct ConversionNode {
};
struct ConversionEdge {
};


graph<ConversionNode>	ConversionGraph;


struct Converted {
	ptr_machine<void>					p;
	dynamic_array<ptr_machine<void>*>	deferred;
};

hash_map<ptr_machine<void>, Converted>	changes;

bool Conversion::checkinside(const Browser &b, FLAGS flags, int depth) {
	const Type *type = b.GetTypeDef()->SkipUser();
	bool	changed	= false;

	if (!IsPlainData(type)) switch (type->GetType()) {
		case REFERENCE: {
			auto	ref = (TypeReference*)type;
			ptr_machine<void>	p = ref->get(b);
			if (p) {
				if (p.Header()->flags & Value::TEMP_USER) {
					auto &a = changes[p].put();
					if (a.p)
						ref->set(b, a.p);
					else
						a.deferred.push_back(p);
					changed = true;

				} else {
					ptr_machine<void>	p2 = _convert(p, ((TypeReference*)type)->subtype, flags | (p.Flags() & Value::MEMORY32 ? MEMORY32 : NONE), depth - 1);//RECURSE | CHECK_INSIDE);
					if (changed = (void*)p2 != (void*)p) {
						auto &a = changes[p].put();
						a.p = p2;
						for (auto &i : a.deferred)
							*i = p2;
						a.deferred.clear();
						//changes.remove(a);
						//b.ClearTempFlags();
						ref->set(b, p2);
					}
				}
			}
			break;
		}
		case OPENARRAY:
			if (IsPlainData(((TypeOpenArray*)type)->subtype))
				break;
			//fallthrough
		case COMPOSITE:
		case ARRAY:
			for (auto i : b)
				changed = checkinside(i, flags, depth - 1) || changed;
			break;
		default:
			break;
	}
	return changed;
}

ptr_machine<void> Conversion::checkinside(ptr_machine<void> p, FLAGS flags, int depth) {
	if (p.Header()->flags & Value::TEMP_USER)
		return p;

	p.Header()->flags |= Value::TEMP_USER;

	const Type	*skipuser = p.GetType()->SkipUser();

	if (IsPlainData(skipuser))
		return p;

	if (skipuser->GetType() == REFERENCE) {
		auto ref = (TypeReference*)skipuser;
		ref->set(p, _convert(ref->get(p), ref->subtype, flags, depth - 1));// RECURSE | CHECK_INSIDE);
		return p;
	}

	if (flags & FULL_CHECK) {
		switch (skipuser->GetType()) {
			case COMPOSITE:
			case ARRAY:
			case OPENARRAY: {
				ptr_machine<void>	p2 = Duplicate(p);
				if (checkinside(Browser(p2), flags, depth - 1))
					p = p2;
			}
			//fallthrough
			default:
				break;
		}
//		return p;
	}

	if (flags & EXPAND_EXTERNALS)
		p = FileHandler::ExpandExternals(p);

	if (skipuser->ContainsReferences()) switch (skipuser->GetType()) {
		case COMPOSITE: {
			Browser	b(p);
			bool	need_conv = false;
			for (int i = 0, n = b.Count(); !need_conv && i < n; i++) {
				Browser	be = b[i];
				const Type	*eltype = be.GetTypeDef()->SkipUser();
				if (eltype && eltype->GetType() == REFERENCE && !be.External()) {
					auto	ref	= (TypeReference*)eltype;
					if (auto pe	= ref->get(be))
						need_conv = ref->subtype && ref->subtype != pe.GetType();
				}
			}
			if (need_conv) {
				p	= Duplicate(p);
				b	= Browser(p);
				for (int i = 0, n = b.Count(); i < n; i++) {
					Browser		be		= b[i];
					const Type	*eltype = be.GetTypeDef()->SkipUser();
					if (eltype && eltype->GetType() == REFERENCE && !be.External()) {
						auto	ref	= (TypeReference*)eltype;
						if (auto pe	= ref->get(be)) {
							if (ref->subtype && ref->subtype != pe.GetType())
								ref->set(be, _convert(pe, ref->subtype, flags, depth - 1));
						}
					}
				}
			}
			break;
		}
		case ARRAY:
		case OPENARRAY: {
			const Type	*eltype	= skipuser->SubType();
			if (!eltype || eltype->GetType() != REFERENCE || !(eltype = ((TypeReference*)eltype)->subtype))
				break;

			Browser	b(p);
			bool	need_conv	= false;
			for (int i = 0, n = b.Count(); !need_conv && i < n; i++) {
				ptr<void>	*pe	= b[i];
				need_conv	= *pe && eltype != pe->GetType();
			}
			if (need_conv) {
				p = Duplicate(p);
				for (int i = 0, n = b.Count(); i < n; i++) {
					ptr<void>	*pe	= b[i];
					*pe = _convert(*pe, eltype, flags, depth - 1);
				}
			}
			break;
		}
		default:
			break;
	}

	return p;
}

ptr_machine<void> Conversion::_convert(ptr_machine<void> p, const Type *type, FLAGS flags, int depth) {
	if (!p)
		return p;

	if (p.IsExternal()) {
		if ((flags & ALLOW_EXTERNALS) || (p.Flags() & Value::EXTREF))
			return p;

		if (!(flags & EXPAND_EXTERNALS))
			return ptr_machine<void>();//p

		tag2 id = p.ID();
		p		= FileHandler::ReadExternal(p);
		if (id)
			p.SetID(id);
	}

	if (p.Flags() & Value::REDIRECT) {
		tag2			id			= p.ID();
		const Type	*subtype	= p.GetType();
		if (subtype && subtype->GetType() == REFERENCE)
			subtype = subtype->SubType();

		p = *ptr<ptr<void> >(p);
		if (p.IsExternal()) {
			p	= FileHandler::ReadExternal(p);
			p.SetID(id);
		}

		p = _convert(p, subtype, flags, depth - 1);
		if (p.GetType() != subtype)
			return MakePtrIndirect(p, type ? type : subtype);

		if (type == subtype)
			return p;
	}

	bool	change = TypeType(p.GetType()) == USER && (p.GetType()->flags & TypeUser::CHANGE);
	if (change && p.IsType(type, MATCH_MATCHNULLS)) {
		ISO_TRACEF("expanding type:") << p.GetType() << '\n';
		saver<FLAGS>	save_flags(flags);
		if ((flags & EXPAND_EXTERNALS) && (p.GetType()->flags & TypeUser::WRITETOBIN))
			flags = (flags - EXPAND_EXTERNALS) | ALLOW_EXTERNALS;

		if ((flags & (FULL_CHECK | CHECK_INSIDE)) && depth > 0) {
			p = checkinside(p, flags, depth - 1);
			if (flags != save_flags)
				Browser(p).ClearTempFlags();

		} else if ((flags & EXPAND_EXTERNALS) && CheckHasExternals(p)) {
			p = FileHandler::ExpandExternals(p);
		}

		for (auto &i : all()) {
			if (ptr_machine<void> p2 = i(p, NULL, CHANGE)) {
				if (p == p2)
					break;//return p;

				if (!p2.ID())
					p2.SetID(p.ID());

				if (type == p.GetType())
					return p2;

				return _convert(p2, type, flags, depth - 1);
			}
		}

	} else if (change && (p.GetType()->flags & TypeUser::WRITETOBIN) && !(flags & ALLOW_EXTERNALS) && (p.Flags() & Value::HASEXTERNAL)) {
		return save(flags, (flags - EXPAND_EXTERNALS) | ALLOW_EXTERNALS), _convert(p, type, flags, depth);

	} else if (!(flags & (ALLOW_EXTERNALS | FULL_CHECK | CHECK_INSIDE)) && (flags & EXPAND_EXTERNALS) && (p.Flags() & Value::HASEXTERNAL)) {
		p = FileHandler::ExpandExternals(p);
	}

	if (!p || ((!type || type == p.GetType()) && !(flags & (FULL_CHECK | CHECK_INSIDE))))
		return p;

//	ASSERT(depth > 0);

	if ((p.Flags() & Value::CRCTYPE) || !p.GetType() || depth < 0)
		return ISO_NULL;

	if ((flags & (FULL_CHECK | CHECK_INSIDE)) && depth > 0)
		p = checkinside(p, flags, depth - 1);

	if (!type || type == p.GetType())
		return p;

	if (p.GetType()->SameAs(type)) {
		if (type->GetType() == USER && (type->flags & TypeUser::CHANGE)) {
			//p = Duplicate(p);
			p.Header()->type = type;
			//return _convert(p, type, flags, depth - 1);
		}
		return p;
	}

	const Type	*skipuser 	= p.GetType()->SkipUser();
	const Type	*skipuser2 	= type->SkipUser();

	if (!change) {
		if (skipuser && skipuser->GetType() == REFERENCE) {
	//		return _convert(*(ptr<void>*)p, type, flags | RECURSE, depth - 1);
			return _convert(((TypeReference*)skipuser)->get(p), type, flags | RECURSE, depth - 1);
		}

		if (skipuser && skipuser->GetType() == VIRTUAL && !skipuser->Fixed()) {
			Virtual	*virt = (Virtual*)skipuser;
			if (Browser2 b = virt->Deref(virt, p))
				return _convert(b, type, flags | RECURSE, depth - 1);
		}

	}
	if (skipuser2 && skipuser2->GetType() == REFERENCE && ((TypeReference*)skipuser2)->subtype) {
		ptr<void>	p2 = MakePtr(type, p.ID());
		*(ptr<void>*)p2 = _convert(p, ((TypeReference*)skipuser2)->subtype, flags | RECURSE, depth - 1);
		return p2;
	}

	if (type->GetType() != USER) {
		ptr<void>	p2 = MakePtr(type, p.ID());
		if (batch_convert(p, 0, p.GetType(), p2, 0, type, 1))
			return p2;
	} else {
		// only allow inherit from USER
		for (const Type *inherit = skipuser; inherit && inherit->GetType() == COMPOSITE;) {
			inherit = (*(TypeComposite*)inherit)[0].type;
			if (inherit == type)
				return p;
		}
	}

//	if (p.TestFlags(Value::HASEXTERNAL))
//		return p;

	for (auto &i : all()) {
		if (ptr_machine<void> p2 = i(p, type, flags - RECURSE)) {
			if (!p2.ID())
				p2.SetID(p.ID());
			return _convert(p2, type, flags, depth - 1);
		}
	}
	if (flags & RECURSE) {
		for (auto &i : all()) {
			if (ptr_machine<void> p2 = i(p, type, flags)) {
				if (!p2.ID())
					p2.SetID(p.ID());
				return _convert(p2, type, flags, depth - 1);
			}
		}
	}

	if (skipuser2 && skipuser2->GetType() == REFERENCE) {
		ptr<void>	p2 = MakePtr(type, p.ID());
		((TypeReference*)skipuser2)->set(p2, p);
//		*(ptr<void>*)p2 = p;
		return p2;
	}

	if (flags & RECURSE) {
		ptr<void> p2 = MakePtr(type, p.ID());
		if (batch_convert(p, 0, p.GetType(), p2, 0, type, 1))
			return p2;
	}
	return ISO_NULL;
}

ptr_machine<void> Conversion::convert(const ptr_machine<void> &p, const Type *type, FLAGS flags, int depth) {
	if ((flags & (FULL_CHECK | CHECK_INSIDE)) && !p.IsExternal()) {
		struct ClearTempFlags {
			ptr<void> p;
			ClearTempFlags(const ptr<void> &_p) : p(_p)	{}
			~ClearTempFlags() {
				if (p && (p.Flags() & Value::TEMP_USER)) {
					p.ClearFlags(Value::TEMP_USER);
					Browser(p).ClearTempFlags();
				}
				changes.clear();
			}
		} ctf(p);
		return _convert(p, type, flags, depth);
	}
	return _convert(p, type, flags, depth);
}
#if 0
ptr_machine<void> Conversion::convert(const Browser &b, const Type *type, FLAGS flags, int depth) {
	if (!b.GetTypeDef())
		return ISO_NULL;
	if (b.GetType() == REFERENCE)
		return convert(ptr<void>(*b), type, flags);
	return convert(b.Duplicate(), type, flags);
}
#else
ptr_machine<void> Conversion::convert(const Browser &b, const Type *type, FLAGS flags, int depth) {
	const Type *stype = b.GetTypeDef();
	if (!stype)
		return ISO_NULL;

	if (b.GetType() == REFERENCE)
		return convert((*b).operator ptr<void>(), type, flags);

	if (!type || type == stype)
		return b.Duplicate();

	if (type->SameAs(stype)) {
		ptr<void>	p = b.Duplicate();
		if (type->GetType() == USER && (type->flags & TypeUser::CHANGE)) {
			p.Header()->type = type;
			return _convert(p, type, flags, depth - 1);
		}
		return p;
	}

	const Type	*skipuser = stype->SkipUser();
	if (skipuser && skipuser->GetType() == REFERENCE)
		return convert(*b, type, flags | RECURSE);

	const Type	*skipuser2 = type->SkipUser();
	if (skipuser2 && skipuser2->GetType() == REFERENCE && ((TypeReference*)skipuser2)->subtype) {
		ptr<void>	p = MakePtr(type);
		*(ptr<void>*)p = convert(b, ((TypeReference*)skipuser2)->subtype, flags | RECURSE);
		return p;
	}

	if (type->GetType() != USER) {
		ptr<void>	p = MakePtr(type);
		if (batch_convert(b, 0, stype, p, 0, type, 1))
			return p;
	}

	for (const Type *inherit = skipuser; inherit && inherit->GetType() == COMPOSITE;) {
		inherit = (*(TypeComposite*)inherit)[0].type;
		if (inherit == type)
			return b.Duplicate();
	}

	ptr<void> p = MakePtr(type);
	if (batch_convert(b, 0, stype, p, 0, type, 1))
		return p;

	return ISO_NULL;
}
#endif

//-----------------------------------------------------------------------------
//	batch_convert
//-----------------------------------------------------------------------------

void strided_clear(void *dest, uint32 stride, uint32 size, uint32 num) {
	if (stride == size)
		memset(dest, 0, size * num);
	else for (int i = 0; i < num; i++)
		memset((char*)dest + stride * i, 0, size);
}

const Type *string_elem(const TypeString *type) {
	switch (type->log2_char_size()) {
		default:	return getdef<char>();
		case 1:		return getdef<char16>();
		case 2:		return getdef<char32>();
		case 3:		return getdef<uint64>();
	}
}

bool Conversion::batch_convert(
	const void	*srce, uint32 srce_stride, const Type *srce_type,
	void		*dest, uint32 dest_stride, const Type *dest_type,
	uint32		num, bool convert_ptrs, void *physical_ram)
{
	if (!dest || !dest_type)
		return false;

	//if (!srce || !srce_type)
	//	return false;

	if (dest_type->GetType() == USER && (dest_type->flags & TypeUser::INITCALLBACK)) {
		bool ret = batch_convert(
			srce, srce_stride, srce_type,
			dest, dest_stride, ((TypeUser*)dest_type)->subtype,
			num, convert_ptrs, physical_ram
		);
		if (dest_type->flags & TypeUser::LOCALSUBTYPE)
			dest_type = user_types.Find(((TypeUser*)dest_type)->ID());
		for (int i = 0; i < num; i++)
			((TypeUserCallback*)dest_type)->init((char*)dest + dest_stride * i, physical_ram);
		return ret;
	}

	auto	*srce_type2 = SkipUser(srce_type);
	auto	*dest_type2 = SkipUser(dest_type);

	auto	st = TypeType(srce_type2);
	auto	dt = TypeType(dest_type2);

	if (dt == UNKNOWN || Same(srce_type2, dest_type2)) {
		bool	plain	= IsPlainData(srce_type2);
		uint32	size	= GetSize(srce_type2);
		if (plain && (num == 1 || (srce_stride == size && dest_stride == size))) {
			memcpy(dest, srce, size * num);
		} else {
			for (int i = 0; i < num; i++) {
				void	*d = (char*)dest + i * dest_stride;
				memcpy(d, (char*)srce + i * srce_stride, size);
				if (!plain)
					_Duplicate(dest_type2, d, TRAV_DEFAULT, physical_ram);
			}
		}
		return true;
	}

	if (st == REFERENCE && dt != REFERENCE) {
		for (int i = 0; i < num; i++) {
			ptr<void>&	p	= *(ptr<void>*)((char*)srce + srce_stride * i);
			if (!batch_convert(
				p,								0,	p.GetType(),
				(char*)dest + dest_stride * i,	0,	dest_type,
				1, convert_ptrs, physical_ram
			))
				return false;
		}
		return true;
	}

	if (st == VIRTUAL && dt != VIRTUAL && !srce_type2->Fixed()) {// && ((Virtual*)srce_type2)->IsVirtPtr(const_cast<void*>(srce))) {
		Virtual	*virt = (Virtual*)srce_type2;
		if (virt->Deref(virt, (void*)srce)) {
			for (int i = 0; i < num; i++) {
				Browser		bs(srce_type2, (char*)srce + i * srce_stride);
				if (Browser2 b = *bs) {
					if (!batch_convert(
						b, 0, b.GetTypeDef(),
						(char*)dest + i * dest_stride, 0, dest_type2,
						1, convert_ptrs, physical_ram
					))
						return false;
				} else {
					return false;
				}
			}
			return true;
		}
	}

	switch (dt) {
		case REFERENCE: {
			auto	refd	= (TypeReference*)dest_type2;
			auto	type	= refd->subtype;
			if (st == REFERENCE) {
				auto	refs = (TypeReference*)srce_type;
				for (int i = 0; i < num; i++) {
					auto	p = refs->get((char*)srce + srce_stride * i);
					refd->set((char*)dest + dest_stride * i, convert_ptrs ? convert(p, type, ALLOW_EXTERNALS) : p);
				}
			} else {
				for (int i = 0; i < num; i++) {
					ptr_machine<void>	p = MakePtr(srce_type);
					memcpy(p, (char*)srce + srce_stride * i, srce_type->GetSize());
					_Duplicate(srce_type, p, TRAV_DEFAULT, physical_ram);
					refd->set((char*)dest + dest_stride * i, convert_ptrs ? convert(p, type, ALLOW_EXTERNALS) : p);
				}
			}
			return true;
		}

		case INT: {
			TypeInt	*cintd	= (TypeInt*)dest_type2;
			bool	sgn		= cintd->is_signed();
			int		mini	= cintd->get_min();
			int		maxi	= cintd->get_max();

			if (st == INT) {
				TypeInt		*cints	= (TypeInt*)srce_type2;
				int			shift	= cintd->frac_bits() - cints->frac_bits();
				for (int i = 0; i < num; i++) {
					int	t = cints->get((char*)srce + srce_stride * i);
					t = shift < 0 ? t >> -shift : t << shift;
					t = sgn ? clamp(t, mini, maxi) : min(uint32(t), uint32(maxi));
					cintd->set((char*)dest + dest_stride * i, t);
				}
				return true;

			} else if (st == FLOAT) {
				TypeFloat	*cflts	= (TypeFloat*)srce_type2;
				float		shift	= 1 << cintd->frac_bits();
				for (int i = 0; i < num; i++)
					cintd->set((char*)dest + dest_stride * i, clamp(int(cflts->get((char*)srce + srce_stride * i) * shift), mini, maxi));
				return true;
			}
			break;
		}

		case FLOAT: {
			TypeFloat	*cfltd	= (TypeFloat*)dest_type2;
			float		mini	= cfltd->get_min();
			float		maxi	= cfltd->get_max();

			if (st == INT) {
				TypeInt		*cints	= (TypeInt*)srce_type2;
				float		shift	= 1 / float(1 << cints->frac_bits());
				for (int i = 0; i < num; i++)
					cfltd->set((char*)dest + dest_stride * i, clamp(cints->get((char*)srce + srce_stride * i) * shift, mini, maxi));
				return true;

			} else if (st == FLOAT) {
				TypeFloat	*cflts	= (TypeFloat*)srce_type2;
				for (int i = 0; i < num; i++)
					cfltd->set((char*)dest + dest_stride * i, clamp(cflts->get((char*)srce + srce_stride * i), mini, maxi));
				return true;
			}
			break;
		}

		case STRING: {
			TypeString	*stringd	= (TypeString*)dest_type2;
			if (st == STRING) {
				for (int i = 0; i < num; i++)
					stringd->set((char*)dest + dest_stride * i, (const char*)srce_type2->ReadPtr((char*)srce + i * srce_stride));
				return true;

			} else if (st == OPENARRAY) {
				TypeOpenArray	*array	= (TypeOpenArray*)srce_type2;
				if (TypeType(array->subtype->SkipUser()) == INT) {
					for (int i = 0; i < num; i++) {
						OpenArrayHead	*hs		= OpenArrayHead::Get(srce_type2->ReadPtr((char*)srce + i * srce_stride));
						size_t			size	= GetCount(hs);
						char			*p		= NULL;
						switch (array->subsize) {
							case 1: {
								new(p) string(str((char*)GetData(hs), size));
								//p = (char*)malloc(size + 1);
								//memcpy(p, hs->Data(), size);
								//p[size] = 0;
								break;
							}
							case 2: {
								new(p) string(str((char16*)GetData(hs), size));
								//char16 *s = (char16*)hs->Data();
								//size_t	t = utf8_count(s, size);
								//p = (char*)malloc(t + 1);
								//to_utf8(s, p, t);
								//p[0]	= 0;
								break;
							}
						}
						stringd->set((char*)dest + dest_stride * i, p);
					}
				}
				return true;
			}
			return false;
		}
		case ARRAY: {
			TypeArray	*carrayd	= (TypeArray*)dest_type2;
			uint32		dest_stride2= carrayd->subsize;
			uint32		num2		= carrayd->Count();

			switch (st) {
				case STRING: {
					strided_clear(dest, dest_stride, dest_stride2 * num2, num);
					const TypeString	*srce_type3		= (TypeString*)srce_type2;
					const Type			*srce_elem		= string_elem(srce_type3);
					uint32				srce_stride2	= srce_type3->char_size();
					for (int i = 0; i < num; i++) {
						void	*sd		= srce_type3->ReadPtr((char*)srce + i * srce_stride);
						if (!batch_convert(
							sd, srce_stride2, srce_elem,
							(char*)dest + dest_stride * i, dest_stride2, carrayd->subtype,
							min(srce_type3->len(sd), num2), convert_ptrs, physical_ram))
							return false;
					}
					return true;
				}
				case ARRAY: {
					TypeArray	*carrays	= (TypeArray*)srce_type2;
					uint32		numt		= num2;
					while (carrays->Count() == num2 && TypeType(carrayd->subtype) == ARRAY && carrays->subtype->GetType() == ARRAY) {
						carrayd	= (TypeArray*)carrayd->subtype.get();
						carrays	= (TypeArray*)carrays->subtype.get();
						num2	= carrayd->Count();
						numt	*= num2;
					}
					uint32	srce_stride2 = carrays->subsize;
					if (carrays->Count() >= num2) {
						if (num > numt) {
							iso::swap(srce_stride, srce_stride2);
							iso::swap(dest_stride, dest_stride2);
							iso::swap(num, numt);
						}
						for (int i = 0; i < num; i++) {
							if (!batch_convert(
								(char*)srce + srce_stride * i, srce_stride2, carrays->subtype,
								(char*)dest + dest_stride * i, dest_stride2, carrayd->subtype,
								numt, convert_ptrs, physical_ram))
								return false;
						}
					} else {
						strided_clear(dest, dest_stride, dest_stride2 * num2, num);
						for (int i = 0; i < carrays->Count(); i++) {
							if (!batch_convert(
								(char*)srce + srce_stride2 * i, srce_stride, carrays->subtype,
								(char*)dest + dest_stride2 * i, dest_stride, carrayd->subtype,
								num, convert_ptrs, physical_ram))
								return false;
						}
					}
					return true;
				}
				case OPENARRAY: {
					const Type	*types	= ((TypeOpenArray*)srce_type2)->subtype;
					for (int i = 0; i < num; i++) {
						OpenArray<char>	&arrays	= *(OpenArray<char>*)((char*)srce + i * srce_stride);
						if (!batch_convert(
							arrays, types->GetSize(), types,
							(char*)dest + dest_stride * i, dest_stride2, carrayd->subtype,
							min(uint32(arrays.Count()), num2), convert_ptrs, physical_ram))
							return false;
					}
					return true;
				}

				default:
					strided_clear(dest, dest_stride, dest_stride2 * num2, num);
					return batch_convert(
						srce, srce_stride, srce_type,
						dest, dest_stride, carrayd->subtype,
						num, convert_ptrs, physical_ram
					);
			}
			break;
		}

		case OPENARRAY: {
			const Type	*typed	= ((TypeOpenArray*)dest_type2)->subtype;
			uint32		sized	= ((TypeOpenArray*)dest_type2)->subsize;
			uint32		alignd	= ((TypeOpenArray*)dest_type2)->SubAlignment();
			switch (st) {
				case STRING: {
					const TypeString	*srce_type3		= (TypeString*)srce_type2;
					const Type			*types	= string_elem(srce_type3);
					int					sizes	= srce_type3->char_size();
					for (int i = 0; i < num; i++) {
						void			*pd		= (char*)dest + i * dest_stride;
						void			*sd		= srce_type3->ReadPtr((char*)srce + i * srce_stride);
						uint32			num2	= srce_type3->len(sd);
						OpenArrayHead	*hd		= OpenArrayHead::Get(dest_type2->ReadPtr(pd));
						hd = dest_type2->Is64Bit()
							? OpenArrayAlloc<64>::Create(hd, typed, num2)
							: OpenArrayAlloc<32>::Create(hd, typed, num2);

						if (!batch_convert(
							sd, sizes, types,
							GetData(hd), sized, typed,
							num2, convert_ptrs, physical_ram)
						) {
							if (dest_type2->Is64Bit())
								OpenArrayAlloc<64>::Destroy(hd, alignd);
							else
								OpenArrayAlloc<32>::Destroy(hd, alignd);
							return false;
						}
						dest_type2->WritePtr(pd, GetData(hd));
					}
					return true;
				}
				case ARRAY: {
					const Type	*types	= ((TypeArray*)srce_type2)->subtype;
					uint32		sizes	= ((TypeArray*)srce_type2)->subsize;
					int			num2	= ((TypeArray*)srce_type2)->Count();

					for (int i = 0; i < num; i++) {
						void			*pd	= (char*)dest + i * dest_stride;
						OpenArrayHead	*hd	= OpenArrayHead::Get(dest_type2->ReadPtr(pd));
						hd = dest_type2->Is64Bit()
							? OpenArrayAlloc<64>::Create(hd, typed, num2)
							: OpenArrayAlloc<32>::Create(hd, typed, num2);

						if (!batch_convert(
							(char*)srce + i * srce_stride, sizes, types,
							GetData(hd), sized, typed,
							num2, convert_ptrs, physical_ram)
						) {
							if (dest_type2->Is64Bit())
								OpenArrayAlloc<64>::Destroy(hd, alignd);
							else
								OpenArrayAlloc<32>::Destroy(hd, alignd);
							return false;
						}
						dest_type2->WritePtr(pd, GetData(hd));
					}
					return true;
				}
				case OPENARRAY: {
					const Type	*types	= ((TypeOpenArray*)srce_type2)->subtype;
					uint32		sizes	= ((TypeOpenArray*)srce_type2)->subsize;

					for (int i = 0; i < num; i++) {
						OpenArrayHead	*hs		= OpenArrayHead::Get(srce_type2->ReadPtr((char*)srce + i * srce_stride));
						void			*pd		= (char*)dest + i * dest_stride;
						int				num2	= GetCount(hs);
						OpenArrayHead	*hd		= OpenArrayHead::Get(dest_type2->ReadPtr(pd));
						hd = dest_type2->Is64Bit()
							? OpenArrayAlloc<64>::Create(hd, typed, num2)
							: OpenArrayAlloc<32>::Create(hd, typed, num2);

						if (num2 && !batch_convert(
							GetData(hs), sizes, types,
							GetData(hd), sized, typed,
							num2, convert_ptrs, physical_ram)
						) {
							if (dest_type2->Is64Bit())
								OpenArrayAlloc<64>::Destroy(hd, alignd);
							else
								OpenArrayAlloc<32>::Destroy(hd, alignd);
							return false;
						}
						dest_type2->WritePtr(pd, GetData(hd));
					}
					return true;
				}
				case VIRTUAL: {
					for (int i = 0; i < num; i++) {
						Browser			bs(srce_type2, (char*)srce + i * srce_stride);
						int				num2	= bs.Count();
						void			*pd		= (char*)dest + i * dest_stride;
						OpenArrayHead	*hd		= OpenArrayHead::Get(dest_type2->ReadPtr(pd));
						hd = dest_type2->Is64Bit()
							? OpenArrayAlloc<64>::Create(hd, typed, num2)
							: OpenArrayAlloc<32>::Create(hd, typed, num2);

						bool	ret = true;
						for (int j = 0; ret && j < num2; j++) {
							Browser		bsj = bs[j];
							ret = convert(bsj, bsj.GetTypeDef(), hd->GetElement(j, sized), typed, convert_ptrs, physical_ram);
							if (typed->GetType() == REFERENCE)
								((ptr<void>*)hd->GetElement(j, sized))->SetID(bs.GetName(j));
						}

						if (!ret) {
							if (dest_type2->Is64Bit())
								OpenArrayAlloc<64>::Destroy(hd, alignd);
							else
								OpenArrayAlloc<32>::Destroy(hd, alignd);
							return false;
						}

						dest_type2->WritePtr(pd, GetData(hd));
					}
					return true;
				}
				case UNKNOWN:
					if (!srce)
						return true;

				default:
					break;
			}
			break;
		}

		case COMPOSITE: {
			TypeComposite	&compd	= *(TypeComposite*)dest_type2;
			switch (st) {
				case COMPOSITE: {
					TypeComposite	&comps	= *(TypeComposite*)srce_type2;
					if (const void *def = compd.Default()) {
						size_t	s	= compd.GetSize();
						char	*d	= (char*)dest;
						for (int i = 0; i < num; i++, d += dest_stride) {
							memcpy(d, def, s);
							_Duplicate(&compd, d, TRAV_DEFAULT, physical_ram);
						}
					}
					for (auto &i : compd) {
						if (const Element *e = comps.Find(compd.GetID(&i))) {
							if (!batch_convert(
								(char*)srce + e->offset,	srce_stride, e->type,
								(char*)dest + i.offset,		dest_stride, i.type,
								num, convert_ptrs, physical_ram
							))
								return false;
						} else if (TypeType(i.type->SkipUser()) == COMPOSITE) {
							batch_convert(
								srce,						srce_stride, srce_type2,
								(char*)dest + i.offset,		dest_stride, i.type,
								num, convert_ptrs, physical_ram
							);
						}
					}
					return true;
				}
				case OPENARRAY: {
					if (TypeType(((TypeOpenArray*)srce_type2)->subtype->SkipUser()) == REFERENCE) {
						for (int i = 0; i < num; i++) {
							Browser		bs(srce_type2, (char*)srce + i * srce_stride);
							int			got = 0;
							for (auto &i : compd) {
								if (Browser bse = bs[compd.GetID(&i)]) {
									if (!batch_convert(
										bse,					0, bse.GetTypeDef(),
										(char*)dest + i.offset,	0, i.type,
										1, convert_ptrs, physical_ram
									))
										return false;
									got++;
								}
							}
							if (got * 2 < compd.Count())
								return false;
						}
						return true;
					}
					break;
				}
				case ARRAY: {
					auto	arrays	= (TypeArray*)srce_type2;
					int		n		= min(arrays->Count(), compd.Count());
					for (int i = 0; i < n; i++) {
						if (!batch_convert(
							(char*)srce + arrays->subsize * i,	srce_stride, arrays->subtype,
							(char*)dest + compd[i].offset,		dest_stride, compd[i].type,
							num, convert_ptrs, physical_ram
						))
							return false;
					}
					return true;
				}
				default:
					break;
			}
			break;
		}
		case VIRTUAL: {
			Virtual	*v	= (Virtual*)dest_type2;
			for (int i = 0; i < num; i++) {
				if (!v->Convert(v, (char*)dest + i * dest_stride, srce_type2, (char*)srce + i * srce_stride))
					return false;
			}
			return true;
		}
		default:
			break;
	}
	return false;
}

bool soft_set(void* dst, const Type* dtype, const void* src, const Type* stype) {
	return Conversion::batch_convert(
		src, 0, stype,
		dst, 0, dtype,
		1, false, 0
	);
}


bool Assign(const Browser &b1, const Browser &b2, bool convert_ptrs) {
	return Conversion::batch_convert(b2, 0, b2.GetTypeDef(), b1, 0, b1.GetTypeDef(), 1, convert_ptrs);
}

bool Fix(const Browser &b) {
	void *data = b;
	for (const Type *type = b.GetTypeDef(); type;) {
		switch (type->GetType()) {
			case COMPOSITE:
			case ARRAY:
			case OPENARRAY:
				for (uint32 i = 0, count = b.Count(); i < count; ++i) {
					if (!Fix(b[i]))
						return false;
				}
				return true;

			case REFERENCE: {
				const Type	*subtype	= ((TypeReference*)type)->subtype;
				ptr<void>	&p			= *(ptr<void>*)data;
				p = Conversion::convert(p, subtype);
				return true;
			}

			case USER:
				type = ((TypeUser*)type)->subtype;
				break;

			default:
				return false;
		}
	}
	return false;
}

}//namespace iso
