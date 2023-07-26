#include "pdb.h"
#include "extra/c-types.h"

namespace iso {
extern		C_types ctypes, user_ctypes;
C_types&	builtin_ctypes();
}

using namespace iso;

#define CT(T)	ctypes.get_type<T>()

template<uint32 M, uint32 E, bool S> struct C_types::type_getter<soft_float_imp<M, E, S> > {
	static const C_type *f(C_types &types)	{
		return C_type_float::get<M + E + int(S), E, S>();
	}
};

struct type_converter {
	const PDB_types		&pdb;
	TI					minTI;
	const char			*name;
	C_type_composite	*forward_struct;
	const CV::Bitfield	*bitfield;

	static const C_type *const float_types[];
	static const C_type *const signed_integral_types[];
	static const C_type *const unsigned_integral_types[];
	static const C_type *const int_types[];
	static const C_type *const special_types[];
	static const C_type *const special_types2[];

	static const C_type *simple(TI ti) {
	
		int	sub	= CV_SUBT(ti);
		const C_type *type = 0;
		switch (CV_TYPE(ti)) {
			case CV::SPECIAL:	type = special_types[sub]; break;
			case CV::SIGNED:	type = signed_integral_types[sub]; break;
			case CV::UNSIGNED:	type = unsigned_integral_types[sub]; break;
			case CV::BOOLEAN:	type = CT(bool); break;
			case CV::REAL:		type = float_types[sub]; break;
		//	case CV::COMPLEX:	type = "complex<" << float_types[sub] << '>'; break;
			case CV::SPECIAL2:	type = special_types2[sub];	break;
			case CV::INT:		type = int_types[sub];	break;
		//	case CV::CVRESERVED:type = "<reserved>";	break;
			default: ISO_ASSERT(0); return 0;
		}
		switch (CV_MODE(ti)) {
			case CV::TM_DIRECT:	return type;
			case CV::TM_NPTR:	return ctypes.add(C_type_pointer(type, 16, false));
			case CV::TM_NPTR32:	return ctypes.add(C_type_pointer(type, 32, false));
			case CV::TM_NPTR64:	return ctypes.add(C_type_pointer(type, 64, false));
			default: ISO_ASSERT(0); return 0;
		}
	}
	
	static bool is_conversion_operator(const char *name) {
		return str(name).begins("operator ");
	}

	const C_type *procTI(TI ti) {
		if (ti < 0x1000)
			return simple(ti);
		return CV::process<const C_type*>(pdb.GetType(ti), *this);
	}

	CV::FieldList	*get_fieldlist(TI ti) {
		if (ti) {
			CV::FieldList	*list = pdb.GetType(ti)->as<CV::FieldList>();
			ISO_ASSERT(list->leaf == CV::LF_FIELDLIST || list->leaf == CV::LF_FIELDLIST_16t);
			return list;
		}
		return 0;
	}

//	const C_type *operator()(nullptr_t) {}
	template<typename T> const C_type *operator()(const T&, bool = false) { return 0; }

	const C_type *operator()(const CV::HLSL &t) {
		static const char *to_pssl[] = {
			0,							//CV_BI_HLSL_INTERFACE_POINTER			= 0x0200,
			"Texture1D",				//CV_BI_HLSL_TEXTURE1D					= 0x0201,
			"Texture1D_Array",			//CV_BI_HLSL_TEXTURE1D_ARRAY			= 0x0202,
			"Texture2D",				//CV_BI_HLSL_TEXTURE2D					= 0x0203,
			"Texture2D_Array",			//CV_BI_HLSL_TEXTURE2D_ARRAY			= 0x0204,
			"Texture3D",				//CV_BI_HLSL_TEXTURE3D					= 0x0205,
			"TextureCube",				//CV_BI_HLSL_TEXTURECUBE				= 0x0206,
			"TextureCube_Array",		//CV_BI_HLSL_TEXTURECUBE_ARRAY			= 0x0207,
			"Texture2D",				//CV_BI_HLSL_TEXTURE2DMS				= 0x0208,
			"Texture2D_Array",			//CV_BI_HLSL_TEXTURE2DMS_ARRAY			= 0x0209,
			"Sampler",					//CV_BI_HLSL_SAMPLER					= 0x020a,
			"Sampler",					//CV_BI_HLSL_SAMPLERCOMPARISON			= 0x020b,
			"DataBuffer",				//CV_BI_HLSL_BUFFER						= 0x020c,
			"PointBuffer",				//CV_BI_HLSL_POINTSTREAM				= 0x020d,
			"LineBuffer",				//CV_BI_HLSL_LINESTREAM					= 0x020e,
			"TriangleBuffer",			//CV_BI_HLSL_TRIANGLESTREAM				= 0x020f,
			"Inputpatch",				//CV_BI_HLSL_INPUTPATCH					= 0x0210,
			"Outputpatch",				//CV_BI_HLSL_OUTPUTPATCH				= 0x0211,
			"RW_Texture1d",				//CV_BI_HLSL_RWTEXTURE1D				= 0x0212,
			"RW_Texture1d_Array",		//CV_BI_HLSL_RWTEXTURE1D_ARRAY			= 0x0213,
			"RW_Texture2d",				//CV_BI_HLSL_RWTEXTURE2D				= 0x0214,
			"RW_Texture2d_Array",		//CV_BI_HLSL_RWTEXTURE2D_ARRAY			= 0x0215,
			"RW_Texture3d",				//CV_BI_HLSL_RWTEXTURE3D				= 0x0216,
			"RW_DataBuffer",			//CV_BI_HLSL_RWBUFFER					= 0x0217,
			"ByteBuffer",				//CV_BI_HLSL_BYTEADDRESS_BUFFER			= 0x0218,
			"RW_ByteBuffer",			//CV_BI_HLSL_RWBYTEADDRESS_BUFFER		= 0x0219,
			"RegularBuffer",			//CV_BI_HLSL_STRUCTURED_BUFFER			= 0x021a,
			"RW_RegularBuffer",			//CV_BI_HLSL_RWSTRUCTURED_BUFFER		= 0x021b,
			"AppendRegularBuffer",		//CV_BI_HLSL_APPEND_STRUCTURED_BUFFER	= 0x021c,
			"ConsumeRegularBuffer",		//CV_BI_HLSL_CONSUME_STRUCTURED_BUFFER	= 0x021d,
			0,							//CV_BI_HLSL_MIN8FLOAT					= 0x021e,
			0,							//CV_BI_HLSL_MIN10FLOAT					= 0x021f,
			0,							//CV_BI_HLSL_MIN16FLOAT					= 0x0220,
			0,							//CV_BI_HLSL_MIN12INT					= 0x0221,
			0,							//CV_BI_HLSL_MIN16INT					= 0x0222,
			0,							//CV_BI_HLSL_MIN16UINT					= 0x0223,
		};
		auto	*type = procTI(t.subtype);
		if (const char *pssl = to_pssl[t.kind - 0x200]) {
			if (auto *temp = builtin_ctypes().lookup(to_pssl[t.kind - 0x200]))
				type = ctypes.instantiate(temp, pssl, type);
		}
		return type;
	}
	const C_type *operator()(const CV::Alias &t) {
		return ctypes.add(t.name, procTI(t.utype));
	}
	const C_type *operator()(const CV::Pointer &t) {
		return ctypes.add(C_type_pointer(procTI(t.utype), 64, is_any(t.attr.ptrmode, CV::PTR_MODE_REF, CV::PTR_MODE_LVREF, CV::PTR_MODE_RVREF)));
	}
	const C_type *operator()(const CV::Array &t) {
		return ctypes.add(C_type_array(procTI(t.elemtype), uint32((int64)t.size / pdb.GetTypeSize(t.elemtype))));
	}
	const C_type *operator()(const CV::StridedArray &t) {
		return ctypes.add(C_type_array(procTI(t.elemtype), uint32((int64)t.size / t.stride), t.stride));
	}
	const C_type *operator()(const CV::Vector &t) {
		return ctypes.add(C_type_array(procTI(t.elemtype), t.count));
	}
	const C_type *operator()(const CV::Matrix &t) {
		if (t.matattr.row_major)
			return ctypes.add(C_type_array(ctypes.add(C_type_array(procTI(t.elemtype), t.cols)), t.rows));
		else
			return ctypes.add(C_type_array(ctypes.add(C_type_array(procTI(t.elemtype), t.rows)), t.cols));
	}
	const C_type *operator()(const CV::Proc &t) {
		return 0;
	}
	const C_type *operator()(const CV::BClass &t) {
		return 0;
	}
	const C_type *operator()(const CV::Enumerate &t) {
		return 0;
	}
//	const C_type *operator()(const CV::Member &t) {
//		auto	type = procTI(t.index);
//		name	= t.name();
//		offset	= t.offset;
//		return type;
//	}
//	const C_type *operator()(const CV::STMember &t) {
//		return 0;
//	}
//	const C_type *operator()(const CV::MFunc &t) {
//		return 0;
//	}
//	const C_type *operator()(const CV::Method &t) {
//		return 0;
//	}
//	const C_type *operator()(const CV::OneMethod &t) {
//		return 0;
//	}
	const C_type *operator()(const CV::NestType &t) {
		return 0;
	}
	const C_type *operator()(const CV::Class &t) {
		string	id		= t.name();
		auto	ptype	= (C_type_struct*)ctypes.lookup(id);

		if (!ptype || ptype != forward_struct) {
			if (ptype)
				return ptype;

			ptype = new C_type_struct(id);
			ctypes.add(id, ptype);
		
			if (t.property.fwdref) {
				auto	ti = pdb.LookupUDT(t.name());
				return save(forward_struct, ptype), procTI(ti);
			}
		}
		
		ptype->_size = (int64)t.size;

		if (auto *list = get_fieldlist(t.derived)) {
			for (auto &i : list->list())
				ptype->add(0, CV::process<const C_type*>(&i, *this));
		}

		if (auto *list = get_fieldlist(t.field)) {
			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_BCLASS:
					case CV::LF_BINTERFACE: {
						auto	*b = i.as<CV::BClass>();
						ptype->add_atoffset(nullptr, procTI(b->index), (uint32)(int64)b->offset);
						break;
					}
				}
			}

			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_MEMBER: {
						auto	*m	= i.as<CV::Member>();
						auto	bf	= save(bitfield, nullptr);
						if (auto *t	= procTI(m->index)) {;
							int64	offset	= m->offset;
							if (bitfield) {
								ISO_ASSERT(t->type == C_type::INT);
								auto		*t1 = (const C_type_int*)t;
								ptype->add_atbit(m->name(), ctypes.add(C_type_int(bitfield->length, t1->flags & ~C_type_int::ENUM)), (uint32)offset * 8 + bitfield->position);
							} else {
								ptype->add_atoffset(m->name(), t, (uint32)offset);
							}
						}
						break;
					}
					case CV::LF_STMEMBER: {
						auto	*m	= i.as<CV::STMember>();
						auto	*t	= procTI(m->index);
						ptype->add_static(m->name, t);
						break;
					}
				}
			}
		}

		return ptype;
	}
	const C_type *operator()(const CV::Union &t) {
		string	id		= t.name();
		auto	ptype	= (C_type_union*)ctypes.lookup(id);

		if (!ptype || ptype != forward_struct) {
			if (ptype)
				return ptype;

			ptype = new C_type_union(id);
			ctypes.add(id, ptype);

			if (t.property.fwdref) {
				auto	ti = pdb.LookupUDT(t.name());
				return save(forward_struct, ptype), procTI(ti);
			}
		}

		ptype->_size = (int64)t.size;

		if (auto *list = get_fieldlist(t.field)) {
			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_MEMBER: {
						auto	*m	= i.as<CV::Member>();
						auto	*t	= procTI(m->index);
						ptype->add(m->name(), t);
						break;
					}
					case CV::LF_STMEMBER: {
						auto	*m	= i.as<CV::STMember>();
						auto	*t	= procTI(m->index);
						ptype->add_static(m->name, t);
						break;
					}
				}
			}
		}

		return ptype;
	}
	const C_type *operator()(const CV::Enum &t) {
		auto	underlying = procTI(t.utype);
		if (underlying->type != C_type::INT)
			return 0;

		C_type_enum		e(*(const C_type_int*)underlying, t.count);
		if (auto *list = get_fieldlist(t.field)) {
			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_ENUMERATE: {
						auto	en = i.as<CV::Enumerate>();
						e.emplace_back(en->name(), en->value);
						break;
					}
				}
			}
		}
		return ctypes.add(t.name, ctypes.add(e));

	}
	const C_type *operator()(const CV::ArgList &t) {
		return 0;
	}
	const C_type *operator()(const CV::Modifier &t) {
		return procTI(t.type);
	}
	const C_type *operator()(const CV::ModifierEx &t) {
		return procTI(t.type);
	}
	const C_type *operator()(const CV::Bitfield &t) {
		const C_type *sub = procTI(t.type);
		bitfield = &t;
		return sub;
	}

	type_converter(const PDB_types &pdb) : pdb(pdb), minTI(pdb.MinTI()) {}
};

const C_type *const type_converter::float_types[] = {
	CT(float),
	CT(double),
	CT(float80),
	CT(float128),
	CT(float48),
	0,//CT(float32PP),
	CT(float16),
};
const C_type *const type_converter::signed_integral_types[] = {
	CT(char),
	CT(short),
	CT(int),
	CT(int64),
	0,//CT(int128),
};
const C_type *const type_converter::unsigned_integral_types[] = {
	CT(uint8),
	CT(uint16),
	CT(uint32),
	CT(uint64),
	CT(uint128),
};
const C_type *const type_converter::int_types[] = {
	CT(char),
	CT(unsigned char),	//>()wchar?>(),
	CT(short),
	CT(unsigned short),
	CT(int),
	CT(unsigned),
	CT(int64),
	CT(uint64),
	0,//CT(int128),
	CT(uint128),
	CT(char16_t),
	CT(char32_t),
};
const C_type *const type_converter::special_types[] = {
	0,	//"notype"
	0,	//"abs",
	0,	//"segment",
	0,	//"void",
	0,	//"currency",
	0,	//"nbasicstr",
	0,	//"fbasicstr",
	0,	//"nottrans",
	CT(HRESULT),	//"HRESULT",
};
const C_type *const type_converter::special_types2[] = {
	0,	//"bit",
	0,	//"paschar",
	0,	//"bool32ff",
};

namespace app {
const C_type *to_c_type(const PDB_types &pdb, TI ti) {
	if (ti < 0x1000)
		return type_converter::simple(ti);

	type_converter	conv(pdb);
	return CV::process<const C_type*>(pdb.GetType(ti), conv);
}
}
