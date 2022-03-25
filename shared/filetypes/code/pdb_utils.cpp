#include "pdb.h"

using namespace iso;

string_accum& operator<<(string_accum &sa, const CV::SYMTYPE &sym) {
	return sa <<  "type=0x" << hex(sym.rectyp) << ", name=" << get_name(&sym);
}
string_accum& operator<<(string_accum &sa, const CV::Leaf &type) {
	return sa << "type=0x" << hex(type.leaf) << ", name=" << get_name(&type);
}

const char *iso::namespace_sep(const char *name) {
	if (!name)
		return 0;
	int	nested = 0;
	for (const char *p = name; ; ++p) {
		switch (*p) {
			case 0: return 0;
			case '<': ++nested; break;
			case '>': --nested; break;
			case ':': if (nested == 0) return p;
		}
	}
}
const char *iso::rnamespace_sep(const char *name) {
	if (!name)
		return 0;
	int	nested = 0;
	for (const char *p = string_end(name); p >= name; --p) {
		switch (*p) {
			case '>': ++nested; break;
			case '<': --nested; break;
			case ':': if (nested == 0) return p + 1;
		}
	}
	return name;
}

bool iso::anonymous_namespace(const count_string &s) {
	return s == "`anonymous namespace'" || s == "`anonymous-namespace'";
}

/*
inline const MethodList::entry *FindMethod(const MethodList *list, TI tiFunc) {
	for (auto &i : list->list()) {
		if (i.index == tiFunc)
			return &i;
	}
	return nullptr;
}

const Leaf *FindMember(const FieldList *list, const char *member, uint16 *legal_leaves, int leaf_cnt) {
	for (auto &i : list->list()) {
		if (get_name(&i) == member) {
			if (leaf_cnt == 0)
				return &i;

			for (auto j : make_range_n(legal_leaves, leaf_cnt)) {
				if (j == i.leaf)
					return &i;
			}
		}
	}
	return nullptr;
}

const Leaf *MemberOfClass(const PDB &pdb, const char *sz, TI *ptiClass) {
	if (const char *pc = str(sz).rfind(':')) {
		if (*ptiClass = pdb.LookupUDT(sz, true)) {
			if (TYPTYPE *type = pdb.GetType(*ptiClass)) {
				if (type = pdb.GetType(UDT(type).field))
					return FindMember(type->as<FieldList>(), pc + 1, NULL, 0);
			}
		}
	}
	return false;
}
*/

//-----------------------------------------------------------------------------
//	size_getter
//-----------------------------------------------------------------------------

size_t iso::simple_size(TI ti) {
	static const uint8 special_types[]	= {0, 0, 0, 0, 4, 4, 8, 0, 4};
	static const uint8 float_types[]	= {4, 8, 10, 16, 6, 4, 2};
	static const uint8 integral_types[]	= {1, 2, 4, 8, 16};
	static const uint8 int_types[]		= {1, 1, 2, 2, 4, 4, 8, 8, 16, 16, 2, 4};
	static const uint8 pointer_modes[]	= {0, 2, 4, 4, 4, 8, 8, 16};

	int	ptr	= CV_MODE(ti);
	if (ptr)
		return pointer_modes[ptr];

	int	sub	= CV_SUBT(ti);
	switch (CV_TYPE(ti)) {
		case CV::SPECIAL:	return special_types[sub];
		case CV::SIGNED:	return integral_types[sub];
		case CV::UNSIGNED:	return integral_types[sub];
		case CV::BOOLEAN:	return 1;
		case CV::REAL:		return float_types[sub];
		case CV::COMPLEX:	return float_types[sub] * 2;
		case CV::INT:		return int_types[sub];
		default:			return 0;
	}
}

struct size_getter {
	const PDB_types	&pdb;

	size_t procTI(TI ti) {
		if (ti >= pdb.MinTI())
			return CV::process<size_t>(pdb.GetType(ti), *this);
		return simple_size(ti);
	}

	template<typename T> size_t	operator()(const T &t, bool)	{ return operator()(t); }
	template<typename T> size_t	operator()(const T &t)			{ return 0; }

	size_t	operator()(const CV::Alias &t)		{ return procTI(t.utype); }
	size_t	operator()(const CV::Enum &t)			{ return t.utype ? procTI(t.utype) : 4; }
	size_t	operator()(const CV::Pointer &t)		{ return t.attr.size; }
	size_t	operator()(const CV::Array &t)		{ return (int64)t.size; }
	size_t	operator()(const CV::StridedArray &t)	{ return (int64)t.size; }
	size_t	operator()(const CV::Vector &t)		{ return (int64)t.size; }
	size_t	operator()(const CV::Matrix &t)		{ return (int64)t.size; }
	size_t	operator()(const CV::HLSL &t) {
		switch (t.kind) {
			case CV_BI_HLSL_INTERFACE_POINTER:	break;
			case CV_BI_HLSL_TEXTURE1D: 			break;
			case CV_BI_HLSL_TEXTURE1D_ARRAY:	break;
			case CV_BI_HLSL_TEXTURE2D: 			break;
			case CV_BI_HLSL_TEXTURE2D_ARRAY:	break;
			case CV_BI_HLSL_TEXTURE3D: 			break;
			case CV_BI_HLSL_TEXTURECUBE: 		break;
			case CV_BI_HLSL_TEXTURECUBE_ARRAY:	break;
			case CV_BI_HLSL_TEXTURE2DMS: 		break;
			case CV_BI_HLSL_TEXTURE2DMS_ARRAY:	break;
			case CV_BI_HLSL_SAMPLER: 			break;
			case CV_BI_HLSL_SAMPLERCOMPARISON:	break;
			case CV_BI_HLSL_BUFFER: 			break;
			case CV_BI_HLSL_POINTSTREAM: 		break;
			case CV_BI_HLSL_LINESTREAM: 		break;
			case CV_BI_HLSL_TRIANGLESTREAM:		return procTI(t.subtype);
			case CV_BI_HLSL_INPUTPATCH: 		return procTI(t.subtype);
			case CV_BI_HLSL_OUTPUTPATCH: 		return procTI(t.subtype);
			case CV_BI_HLSL_RWTEXTURE1D:		break;
			case CV_BI_HLSL_RWTEXTURE1D_ARRAY:	break;
			case CV_BI_HLSL_RWTEXTURE2D:		break;
			case CV_BI_HLSL_RWTEXTURE2D_ARRAY:	break;
			case CV_BI_HLSL_RWTEXTURE3D:		break;
			case CV_BI_HLSL_RWBUFFER:			break;
			case CV_BI_HLSL_BYTEADDRESS_BUFFER: break;
			case CV_BI_HLSL_RWBYTEADDRESS_BUFFER: break;
			case CV_BI_HLSL_STRUCTURED_BUFFER:	break;
			case CV_BI_HLSL_RWSTRUCTURED_BUFFER: break;
			case CV_BI_HLSL_APPEND_STRUCTURED_BUFFER: break;
			case CV_BI_HLSL_CONSUME_STRUCTURED_BUFFER: break;
			case CV_BI_HLSL_MIN8FLOAT:			break;
			case CV_BI_HLSL_MIN10FLOAT:			break;
			case CV_BI_HLSL_MIN16FLOAT:			break;
			case CV_BI_HLSL_MIN12INT:			break;
			case CV_BI_HLSL_MIN16INT:			break;
			case CV_BI_HLSL_MIN16UINT:			break;
		}
		return 0;
	}

	size_t	operator()(const CV::Union &t) {
		if (!t.size) {
			if (TI ti2 = pdb.LookupUDT(t.name()))
				return procTI(ti2);
		}
		return (int64)t.size;
	}
	size_t	operator()(const CV::Class &t) {
		if (!t.size) {
			if (TI ti2 = pdb.LookupUDT(t.name()))
				return procTI(ti2);
		}
		return (int64)t.size;
	}
	size_t	operator()(const CV::Modifier &t)		{ return procTI(t.type); }
	size_t	operator()(const CV::ModifierEx &t)	{ return procTI(t.type); }
	size_t	operator()(const CV::Bitfield &t)		{ return procTI(t.type); }
	size_t	operator()(const CV::Member &t)		{ return procTI(t.index); }
	size_getter(const PDB_types &pdb) : pdb(pdb) {}
};

size_t PDB_types::GetTypeSize(const CV::Leaf &type)	const	{ return CV::process<size_t>(&type, size_getter(*this)); }
size_t PDB_types::GetTypeSize(TI ti)					const	{ return size_getter(*this).procTI(ti); }

//-----------------------------------------------------------------------------
//	alignment_getter
//-----------------------------------------------------------------------------

struct alignment_getter {
	const PDB_types		&pdb;

	uint32 procTI(TI ti) {
		if (ti >= pdb.MinTI())
			return CV::process<uint32>(pdb.GetType(ti), *this);
		return (uint32)simple_size(ti);
	}

	uint32	fieldlist(TI ti) {
		uint32	alignment = 1;
		if (ti) {
			auto	*list = pdb.GetType(ti)->as<CV::FieldList>();
			ISO_ASSERT(list->leaf == CV::LF_FIELDLIST || list->leaf == CV::LF_FIELDLIST_16t);

			for (auto &i : list->list())
				alignment = max(alignment, CV::process<uint32>(&i, *this));
		}
		return alignment;
	}

	template<typename T> uint32	operator()(const T &t, bool)	{ return operator()(t); }
	template<typename T> uint32	operator()(const T &t)			{ return 0; }

	uint32	operator()(const CV::Enum &t)			{ return t.utype ? procTI(t.utype) : 4; }
	uint32	operator()(const CV::Pointer &t)		{ return t.attr.size; }
	uint32	operator()(const CV::Array &t)		{ return procTI(t.elemtype); }
	uint32	operator()(const CV::StridedArray &t)	{ return (int64)t.stride; }
	uint32	operator()(const CV::Vector &t)		{ return procTI(t.elemtype); }
	uint32	operator()(const CV::Matrix &t)		{ return procTI(t.elemtype); }
	uint32	operator()(const CV::Union &t) {
		if (!t.field) {
			if (TI ti2 = pdb.LookupUDT(t.name()))
				return procTI(ti2);
		}
		if (t.property.packed)
			return 1;
		return fieldlist(t.field);
	}
	uint32	operator()(const CV::Class &t) {
		if (!t.field) {
			if (TI ti2 = pdb.LookupUDT(t.name()))
				return procTI(ti2);
		}
		if (t.property.packed)
			return 1;
		return fieldlist(t.field);
	}
	uint32	operator()(const CV::Modifier &t)		{ return t.attr.MOD_unaligned ? 1 : procTI(t.type); }
	uint32	operator()(const CV::ModifierEx &t)	{
		for (auto &p : t.list()) {
			if (p == CV_MOD_UNALIGNED)
				return 1;
		}
		return procTI(t.type);
	}
	uint32	operator()(const CV::Bitfield &t)		{ return procTI(t.type); }
	uint32	operator()(const CV::Member &t)		{ return procTI(t.index); }

	alignment_getter(const PDB_types &pdb) : pdb(pdb) {}
};

uint32 PDB_types::GetTypeAlignment(const CV::Leaf &type)	const	{ return CV::process<uint32>(&type, alignment_getter(*this)); }
uint32 PDB_types::GetTypeAlignment(TI ti)					const	{ return alignment_getter(*this).procTI(ti); }

//-----------------------------------------------------------------------------
// type_kinder: get kind of type
//-----------------------------------------------------------------------------

struct simple_type_getter {
	const PDB_types		&pdb;
	simple_type procTI(TI ti) {
		return ti < pdb.MinTI() ? ti : CV::process<simple_type>(pdb.GetType(ti), *this);
	}

	template<typename T> simple_type	operator()(const T &t, bool)	{ return 0; }
	template<typename T> simple_type	operator()(const T &t)			{ return 0; }

	simple_type	operator()(const CV::Enum &t)		{ return procTI(t.utype); }
	simple_type	operator()(const CV::Pointer &t)	{
		static const uint8	trans[] = {
			CV::TM_NPTR,	//	CV_PTR_NEAR				= 0x00, // 16 bit pointer
			CV::TM_FPTR,	//	CV_PTR_FAR				= 0x01, // 16:16 far pointer
			CV::TM_HPTR,	//	CV_PTR_HUGE				= 0x02, // 16:16 huge pointer
			CV::TM_NPTR,	//	CV_PTR_BASE_SEG			= 0x03, // based on segment
			CV::TM_NPTR,	//	CV_PTR_BASE_VAL			= 0x04, // based on value of base
			CV::TM_NPTR,	//	CV_PTR_BASE_SEGVAL		= 0x05, // based on segment value of base
			CV::TM_NPTR,	//	CV_PTR_BASE_ADDR		= 0x06, // based on address of base
			CV::TM_NPTR,	//	CV_PTR_BASE_SEGADDR		= 0x07, // based on segment address of base
			CV::TM_NPTR,	//	CV_PTR_BASE_TYPE		= 0x08, // based on type
			CV::TM_NPTR,	//	CV_PTR_BASE_SELF		= 0x09, // based on self
			CV::TM_NPTR32,	//	CV_PTR_NEAR32			= 0x0a, // 32 bit pointer
			CV::TM_FPTR32,	//	CV_PTR_FAR32			= 0x0b, // 16:32 pointer
			CV::TM_NPTR64,	//	CV_PTR_64				= 0x0c, // 64 bit pointer
			CV::TM_NPTR,	//	CV_PTR_UNUSEDPTR		= 0x0d	// first unused pointer type
		};
		return procTI(t.utype).set_mode((CV::prmode_e)trans[t.attr.ptrtype]);
	}

	simple_type	operator()(const CV::Modifier &t)	{ return procTI(t.type); }
	simple_type	operator()(const CV::Bitfield &t)	{ return procTI(t.type); }

	simple_type_getter(const PDB_types &pdb) : pdb(pdb) {}
};

simple_type iso::get_simple_type(const PDB_types &pdb, const CV::Leaf &type)	{ return CV::process<simple_type>(&type, simple_type_getter(pdb)); }
simple_type iso::get_simple_type(const PDB_types &pdb, TI ti)					{ return simple_type_getter(pdb).procTI(ti); }

struct typeloc_getter {
	const PDB_types		&pdb;

	TypeLoc	leaf(TypeLoc::Type type, int64 offset, TI ti) {
		return TypeLoc(type, offset, pdb.GetTypeSize(ti), pdb.GetTypeAlignment(ti));
	}
	TypeLoc procTI(TI ti) {
		return ti < pdb.MinTI() ? leaf(TypeLoc::ThisRel, 0, ti) : CV::process<TypeLoc>(pdb.GetType(ti), *this);
	}

	TypeLoc	operator()(const CV::Bitfield_16t &t)	{ return TypeLoc(TypeLoc::Bitfield, t.position, t.length, pdb.GetTypeAlignment(t.type)); }
	TypeLoc	operator()(const CV::Bitfield &t)		{ return TypeLoc(TypeLoc::Bitfield, t.position, t.length, pdb.GetTypeAlignment(t.type)); }
	TypeLoc	operator()(const CV::BClass_16t &t)	{ return leaf(TypeLoc::ThisRel, t.offset, t.index); }
	TypeLoc	operator()(const CV::BClass &t)		{ return leaf(TypeLoc::ThisRel, t.offset, t.index); }
	TypeLoc	operator()(const CV::Member_16t &t)	{ return procTI(t.index) + TypeLoc(TypeLoc::ThisRel, t.offset); }
	TypeLoc	operator()(const CV::Member &t)		{ return procTI(t.index) + TypeLoc(TypeLoc::ThisRel, t.offset); }

	TypeLoc	operator()(const CV::NestType_16t &t)	{ return leaf(TypeLoc::TypeDef, 0, t.index); }
	TypeLoc	operator()(const CV::NestType &t)		{ return leaf(TypeLoc::TypeDef, 0, t.index); }
	TypeLoc	operator()(const CV::NestTypeEx &t)	{ return leaf(TypeLoc::TypeDef, 0, t.index); }

	TypeLoc	operator()(const CV::STMember_16t &t)	{ return leaf(TypeLoc::Static, 0, t.index); }
	TypeLoc	operator()(const CV::STMember &t)		{ return leaf(TypeLoc::Static, 0, t.index); }
	TypeLoc	operator()(const CV::Method_16t &t)	{ return TypeLoc(TypeLoc::Method, 0, 0, 0); }
	TypeLoc	operator()(const CV::Method &t)		{ return TypeLoc(TypeLoc::Method, 0, 0, 0); }

	TypeLoc	operator()(const CV::OneMethod_16t)	{ return TypeLoc(TypeLoc::Method, 0, 0, 0); }
	TypeLoc	operator()(const CV::OneMethod &t)	{ return TypeLoc(TypeLoc::Method, 0, 0, 0); }

	template<typename T> TypeLoc	operator()(const T &t, bool=false)	{
		return TypeLoc(TypeLoc::None, 0, pdb.GetTypeSize(t), pdb.GetTypeAlignment(t));
		//return TypeLoc();
	}

	typeloc_getter(const PDB_types &pdb) : pdb(pdb) {}
};

TypeLoc iso::get_typeloc(const PDB_types &pdb, const CV::Leaf &type) {
	return CV::process<TypeLoc>(&type, typeloc_getter(pdb));
}

//-----------------------------------------------------------------------------
//	symbol_ti
//-----------------------------------------------------------------------------

struct symbol_ti {
	const PDB		&pdb;

	TI	operator()(const CV::UDTSYM &t)			{ return t.typind; }
	TI	operator()(const CV::REFSYM2 &t)		{ return CV::process<TI>(pdb.GetModule(t.imod).GetSymbol(t.ibSym), *this); }
	TI	operator()(const CV::PROCSYM32 &t)		{ return t.typind; }
	TI	operator()(const CV::CONSTSYM &t)		{ return t.typind; }
	TI	operator()(const CV::DATASYM32 &t)		{ return t.typind; }

	//catchall
	template<typename T> TI	operator()(const T &t)			{ return 0; }
	template<typename T> TI	operator()(const T &t, bool)	{ return operator()(t); }

	symbol_ti(const PDB &pdb) : pdb(pdb) {}
};

TI PDB::GetSymbolTI(const CV::SYMTYPE *sym) const { return CV::process<TI>(sym, symbol_ti(*this)); }

//-----------------------------------------------------------------------------
// symbol_dumper
//-----------------------------------------------------------------------------

const char *iso::dump_simple(string_accum &sa, TI ti) {
	static auto float_types	= make_array<const char*>(
	//static const char *const float_types[] = {
		"float",
		"double",
		"float80",
		"float128",
		"float48",
		"float32PP",
		"float16"
//	}
	);
#if 0
	static const char *const integral_types[] = {
		"char",
		"short",
		"int",
		"__int64",
		"__int128",
	};
	static const char *const int_types[] = {
		"char",		//"signed char",
		"wchar_t",	//"wchar?",
		"short",
		"unsigned short",
		"int",
		"unsigned",
		"__int64",
		"unsigned __int64",
		"__int128",
		"unsigned __int128",
		"char16_t",
		"char32_t",
	};
#else
	static const char *const signed_types[] = {
		"int8",
		"int16",
		"int",
		"int64",
		"int128",
	};
	static const char *const unsigned_types[] = {
		"uint8",
		"uint16",
		"uint32",
		"uint64",
		"uint128",
	};
	static const char *const int_types[] = {
		"char",		//"signed char",
		"wchar_t",	//"wchar?",
		"int16",
		"uint16",
		"int",
		"uint32",
		"int64",
		"uint64",
		"int128",
		"uint128",
		"char16",
		"char32",
	};
#endif
	static const char *const special_types[] = {
		"notype",
		"abs",
		"segment",
		"void",
		"currency",
		"nbasicstr",
		"fbasicstr",
		"nottrans",
		"HRESULT",
	};
	static const char *const special_types2[] = {
		"bit",
		"paschar",
		"bool32ff",
	};
	static const char *const pointer_modes[] = {
		"",
		"near *",
		"far *",
		"huge *",
		"*",		//near32
		"far32 *",
		"*",		//ptr64
		"ptr128 *",
	};

	int	sub	= CV_SUBT(ti);
	switch (CV_TYPE(ti)) {
		case CV::SPECIAL:	sa << special_types[sub]; break;
		case CV::SIGNED:	sa << signed_types[sub]; break;//"signed " << integral_types[sub]; break;
		case CV::UNSIGNED:	sa << unsigned_types[sub]; break;//"unsigned " << integral_types[sub]; break;
		case CV::BOOLEAN:	sa << "bool"; break;
		case CV::REAL:		sa << float_types[sub]; break;
		case CV::COMPLEX:	sa << "complex<" << float_types[sub] << '>'; break;
		case CV::SPECIAL2:	sa << special_types2[sub];	break;
		case CV::INT:		sa << int_types[sub];	break;
		case CV::CVRESERVED:sa << "<reserved>";	break;
	}
	return pointer_modes[CV_MODE(ti)];
}

string_accum &iso::dump_constant(string_accum &sa, simple_type type, const CV::Value &value) {
	switch (value.kind()) {
		case CV::Value::Int: {
			int64	v = value;
			switch (type.type) {
				case CV::REAL:		sa << type.as_float(v); break;
				case CV::UNSIGNED:	sa << "0x" << hex((uint64)v); break;
				case CV::BOOLEAN:	sa << !!v; break;
				default:			sa << v; break;
			}
			break;
		}
		case CV::Value::Float:	sa << (double)value; break;
		//case Value::Complex:	o << (complex<double>)value; break;
		default:
			switch (value.leaf) {
				//case LF_QUADWORD:	sa << value.as<ValueT<uint128>>()->val; break;
				//case LF_UQUADWORD:sa << value.as<ValueT<uint128>>()->val; break;
				case CV::LF_VARSTRING:	sa << value.as<CV::VarString>()->value(); break;
				case CV::LF_UTF8STRING:	sa << value.as<CV::VarString>()->value(); break;
			}
			break;
	}
	return sa;
}
