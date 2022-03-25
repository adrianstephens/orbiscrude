#include "cvinfo.h"
#include "iso/iso.h"

namespace CV {

inline tag2 _GetName(const SYMTYPE &sym)	{ return get_name(&sym); }
inline tag2 _GetName(const SYMTYPE *sym)	{ return get_name(sym); }
inline tag2 _GetName(const TYPTYPE *type)	{ return get_name(type); }
/*
struct SubSection : SubSection, ISO::VirtualDefaults {
	ISO::Browser2	Deref() const {
		switch (type) {
			case DEBUG_S_SYMBOLS:				return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_SYMBOLS				>>());
			case DEBUG_S_LINES:					return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_LINES				>>());
			case DEBUG_S_STRINGTABLE:			return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_STRINGTABLE			>>());
			case DEBUG_S_FILECHKSMS:			return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_FILECHKSMS			>>());
			case DEBUG_S_FRAMEDATA:				return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_FRAMEDATA			>>());
			case DEBUG_S_INLINEELINES:			return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_INLINEELINES			>>());
			case DEBUG_S_CROSSSCOPEIMPORTS:		return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_CROSSSCOPEIMPORTS	>>());
			case DEBUG_S_CROSSSCOPEEXPORTS:		return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_CROSSSCOPEEXPORTS	>>());
			case DEBUG_S_IL_LINES:				return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_IL_LINES				>>());
			case DEBUG_S_FUNC_MDTOKEN_MAP:		return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_FUNC_MDTOKEN_MAP		>>());
			case DEBUG_S_TYPE_MDTOKEN_MAP:		return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_TYPE_MDTOKEN_MAP		>>());
			case DEBUG_S_MERGED_ASSEMBLYINPUT:	return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_MERGED_ASSEMBLYINPUT	>>());
			case DEBUG_S_COFF_SYMBOL_RVA:		return ISO::MakeBrowser(as<SubSectionT<DEBUG_S_COFF_SYMBOL_RVA		>>());
			default:							return ISO::MakeBrowser(as<SubSection>());
		}
	}
	const SubSection *next() const { return (const SubSection*)SubSection::next(); }
};
*/

#define ENUM(x) #x,  x
ISO_DEFUSERENUMXF(SYM_ENUM_e, 197, "SYM_ENUM_e", 16, NONE) {
Init(0,
	ENUM(S_COMPILE),			ENUM(S_REGISTER_16t),		ENUM(S_CONSTANT_16t),		ENUM(S_UDT_16t),			ENUM(S_SSEARCH),			ENUM(S_END),				ENUM(S_SKIP),				ENUM(S_CVRESERVE),
	ENUM(S_OBJNAME_ST),			ENUM(S_ENDARG),				ENUM(S_COBOLUDT_16t),		ENUM(S_MANYREG_16t),		ENUM(S_RETURN),				ENUM(S_ENTRYTHIS),			ENUM(S_BPREL16),			ENUM(S_LDATA16),
	ENUM(S_GDATA16),			ENUM(S_PUB16),				ENUM(S_LPROC16),			ENUM(S_GPROC16),			ENUM(S_THUNK16),			ENUM(S_BLOCK16),			ENUM(S_WITH16),				ENUM(S_LABEL16),
	ENUM(S_CEXMODEL16),			ENUM(S_VFTABLE16),			ENUM(S_REGREL16),			ENUM(S_BPREL32_16t),		ENUM(S_LDATA32_16t),		ENUM(S_GDATA32_16t),		ENUM(S_PUB32_16t),			ENUM(S_LPROC32_16t),
	ENUM(S_GPROC32_16t),		ENUM(S_THUNK32_ST),			ENUM(S_BLOCK32_ST),			ENUM(S_WITH32_ST),			ENUM(S_LABEL32_ST),			ENUM(S_CEXMODEL32),			ENUM(S_VFTABLE32_16t),		ENUM(S_REGREL32_16t),
	ENUM(S_LTHREAD32_16t),		ENUM(S_GTHREAD32_16t),		ENUM(S_SLINK32),			ENUM(S_LPROCMIPS_16t),		ENUM(S_GPROCMIPS_16t),		ENUM(S_PROCREF_ST),			ENUM(S_DATAREF_ST),			ENUM(S_ALIGN),
	ENUM(S_LPROCREF_ST),		ENUM(S_OEM),				ENUM(S_TI16_MAX),			ENUM(S_REGISTER_ST),		ENUM(S_CONSTANT_ST),		ENUM(S_UDT_ST),				ENUM(S_COBOLUDT_ST),		ENUM(S_MANYREG_ST),
	ENUM(S_BPREL32_ST),			ENUM(S_LDATA32_ST),			ENUM(S_GDATA32_ST),			ENUM(S_PUB32_ST),			ENUM(S_LPROC32_ST),			ENUM(S_GPROC32_ST),			ENUM(S_VFTABLE32),			ENUM(S_REGREL32_ST),
	ENUM(S_LTHREAD32_ST),		ENUM(S_GTHREAD32_ST),		ENUM(S_LPROCMIPS_ST),		ENUM(S_GPROCMIPS_ST),		ENUM(S_FRAMEPROC),			ENUM(S_COMPILE2_ST),		ENUM(S_MANYREG2_ST),		ENUM(S_LPROCIA64_ST),
	ENUM(S_GPROCIA64_ST),		ENUM(S_LOCALSLOT_ST),		ENUM(S_PARAMSLOT_ST),		ENUM(S_ANNOTATION),			ENUM(S_GMANPROC_ST),		ENUM(S_LMANPROC_ST),		ENUM(S_RESERVED1),			ENUM(S_RESERVED2),
	ENUM(S_RESERVED3),			ENUM(S_RESERVED4),			ENUM(S_LMANDATA_ST),		ENUM(S_GMANDATA_ST),		ENUM(S_MANFRAMEREL_ST),		ENUM(S_MANREGISTER_ST),		ENUM(S_MANSLOT_ST),			ENUM(S_MANMANYREG_ST),
	ENUM(S_MANREGREL_ST),		ENUM(S_MANMANYREG2_ST),		ENUM(S_MANTYPREF),			ENUM(S_UNAMESPACE_ST),		ENUM(S_ST_MAX),				ENUM(S_OBJNAME),			ENUM(S_THUNK32),			ENUM(S_BLOCK32),
	ENUM(S_WITH32),				ENUM(S_LABEL32),			ENUM(S_REGISTER),			ENUM(S_CONSTANT),			ENUM(S_UDT),				ENUM(S_COBOLUDT),			ENUM(S_MANYREG),			ENUM(S_BPREL32),
	ENUM(S_LDATA32),			ENUM(S_GDATA32),			ENUM(S_PUB32),				ENUM(S_LPROC32),			ENUM(S_GPROC32),			ENUM(S_REGREL32),			ENUM(S_LTHREAD32),			ENUM(S_GTHREAD32),
	ENUM(S_LPROCMIPS),			ENUM(S_GPROCMIPS),			ENUM(S_COMPILE2),			ENUM(S_MANYREG2),			ENUM(S_LPROCIA64),			ENUM(S_GPROCIA64),			ENUM(S_LOCALSLOT),			ENUM(S_SLOT),
	ENUM(S_PARAMSLOT),			ENUM(S_LMANDATA),			ENUM(S_GMANDATA),			ENUM(S_MANFRAMEREL),		ENUM(S_MANREGISTER),		ENUM(S_MANSLOT),			ENUM(S_MANMANYREG),			ENUM(S_MANREGREL),
	ENUM(S_MANMANYREG2),		ENUM(S_UNAMESPACE),			ENUM(S_PROCREF),			ENUM(S_DATAREF),			ENUM(S_LPROCREF),			ENUM(S_ANNOTATIONREF),		ENUM(S_TOKENREF),			ENUM(S_GMANPROC),
	ENUM(S_LMANPROC),			ENUM(S_TRAMPOLINE),			ENUM(S_MANCONSTANT),		ENUM(S_ATTR_FRAMEREL),		ENUM(S_ATTR_REGISTER),		ENUM(S_ATTR_REGREL),		ENUM(S_ATTR_MANYREG),		ENUM(S_SEPCODE),
	ENUM(S_LOCAL_2005),			ENUM(S_DEFRANGE_2005),		ENUM(S_DEFRANGE2_2005),		ENUM(S_SECTION),			ENUM(S_COFFGROUP),			ENUM(S_EXPORT),				ENUM(S_CALLSITEINFO),		ENUM(S_FRAMECOOKIE),
	ENUM(S_DISCARDED),			ENUM(S_COMPILE3),			ENUM(S_ENVBLOCK),			ENUM(S_LOCAL),				ENUM(S_DEFRANGE),			ENUM(S_DEFRANGE_SUBFIELD),	ENUM(S_DEFRANGE_REGISTER),	ENUM(S_DEFRANGE_FRAMEPOINTER_REL),
	ENUM(S_DEFRANGE_SUBFIELD_REGISTER),		ENUM(S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE),		ENUM(S_DEFRANGE_REGISTER_REL),
	ENUM(S_LPROC32_ID),			ENUM(S_GPROC32_ID),			ENUM(S_LPROCMIPS_ID),		ENUM(S_GPROCMIPS_ID),		ENUM(S_LPROCIA64_ID),		ENUM(S_GPROCIA64_ID),		ENUM(S_BUILDINFO),			ENUM(S_INLINESITE),
	ENUM(S_INLINESITE_END),		ENUM(S_PROC_ID_END),		ENUM(S_DEFRANGE_HLSL),		ENUM(S_GDATA_HLSL),			ENUM(S_LDATA_HLSL),			ENUM(S_FILESTATIC),
#if CC_DP_CXX
	ENUM(S_LOCAL_DPC_GROUPSHARED),	ENUM(S_LPROC32_DPC),		ENUM(S_LPROC32_DPC_ID),		ENUM(S_DEFRANGE_DPC_PTR_TAG),ENUM(S_DPC_SYM_TAG_MAP),
#endif
	ENUM(S_ARMSWITCHTABLE),		ENUM(S_CALLEES),			ENUM(S_CALLERS),			ENUM(S_POGODATA),			ENUM(S_INLINESITE2),		ENUM(S_HEAPALLOCSITE),		ENUM(S_MOD_TYPEREF),		ENUM(S_REF_MINIPDB),
	ENUM(S_PDBMAP),				ENUM(S_GDATA_HLSL32),		ENUM(S_LDATA_HLSL32),		ENUM(S_GDATA_HLSL32_EX),	ENUM(S_LDATA_HLSL32_EX),	ENUM(S_FASTLINK),			ENUM(S_INLINEES)
); }};

ISO_DEFUSERENUMXF(LEAF_ENUM_e, 131, "LEAF_ENUM_e", 16, NONE) {
Init(0,
	ENUM(LF_MODIFIER_16t),		ENUM(LF_POINTER_16t),		ENUM(LF_ARRAY_16t),			ENUM(LF_CLASS_16t),			ENUM(LF_STRUCTURE_16t),		ENUM(LF_UNION_16t),			ENUM(LF_ENUM_16t),			ENUM(LF_PROCEDURE_16t),
	ENUM(LF_MFUNCTION_16t),		ENUM(LF_VTSHAPE),			ENUM(LF_COBOL0_16t),		ENUM(LF_COBOL1),			ENUM(LF_BARRAY_16t),		ENUM(LF_LABEL),				ENUM(LF_NULL),				ENUM(LF_NOTTRAN),
	ENUM(LF_DIMARRAY_16t),		ENUM(LF_VFTPATH_16t),		ENUM(LF_PRECOMP_16t),		ENUM(LF_ENDPRECOMP),		ENUM(LF_OEM_16t),			ENUM(LF_TYPESERVER_ST),		ENUM(LF_SKIP_16t),			ENUM(LF_ARGLIST_16t),
	ENUM(LF_DEFARG_16t),		ENUM(LF_LIST),				ENUM(LF_FIELDLIST_16t),		ENUM(LF_DERIVED_16t),		ENUM(LF_BITFIELD_16t),		ENUM(LF_METHODLIST_16t),	ENUM(LF_DIMCONU_16t),		ENUM(LF_DIMCONLU_16t),
	ENUM(LF_DIMVARU_16t),		ENUM(LF_DIMVARLU_16t),		ENUM(LF_REFSYM),			ENUM(LF_BCLASS_16t),		ENUM(LF_VBCLASS_16t),		ENUM(LF_IVBCLASS_16t),		ENUM(LF_ENUMERATE_ST),		ENUM(LF_FRIENDFCN_16t),
	ENUM(LF_INDEX_16t),			ENUM(LF_MEMBER_16t),		ENUM(LF_STMEMBER_16t),		ENUM(LF_METHOD_16t),		ENUM(LF_NESTTYPE_16t),		ENUM(LF_VFUNCTAB_16t),		ENUM(LF_FRIENDCLS_16t),		ENUM(LF_ONEMETHOD_16t),
	ENUM(LF_VFUNCOFF_16t),		ENUM(LF_TI16_MAX),			ENUM(LF_MODIFIER),			ENUM(LF_POINTER),			ENUM(LF_ARRAY_ST),			ENUM(LF_CLASS_ST),			ENUM(LF_STRUCTURE_ST),		ENUM(LF_UNION_ST),
	ENUM(LF_ENUM_ST),			ENUM(LF_PROCEDURE),			ENUM(LF_MFUNCTION),			ENUM(LF_COBOL0),			ENUM(LF_BARRAY),			ENUM(LF_DIMARRAY_ST),		ENUM(LF_VFTPATH),			ENUM(LF_PRECOMP_ST),
	ENUM(LF_OEM),				ENUM(LF_ALIAS_ST),			ENUM(LF_OEM2),				ENUM(LF_SKIP),				ENUM(LF_ARGLIST),			ENUM(LF_DEFARG_ST),			ENUM(LF_FIELDLIST),			ENUM(LF_DERIVED),
	ENUM(LF_BITFIELD),			ENUM(LF_METHODLIST),		ENUM(LF_DIMCONU),			ENUM(LF_DIMCONLU),			ENUM(LF_DIMVARU),			ENUM(LF_DIMVARLU),			ENUM(LF_BCLASS),			ENUM(LF_VBCLASS),
	ENUM(LF_IVBCLASS),			ENUM(LF_FRIENDFCN_ST),		ENUM(LF_INDEX),				ENUM(LF_MEMBER_ST),			ENUM(LF_STMEMBER_ST),		ENUM(LF_METHOD_ST),			ENUM(LF_NESTTYPE_ST),		ENUM(LF_VFUNCTAB),
	ENUM(LF_FRIENDCLS),			ENUM(LF_ONEMETHOD_ST),		ENUM(LF_VFUNCOFF),			ENUM(LF_NESTTYPEEX_ST),		ENUM(LF_MEMBERMODIFY_ST),	ENUM(LF_MANAGED_ST),		ENUM(LF_ST_MAX),			ENUM(LF_TYPESERVER),
	ENUM(LF_ENUMERATE),			ENUM(LF_ARRAY),				ENUM(LF_CLASS),				ENUM(LF_STRUCTURE),			ENUM(LF_UNION),				ENUM(LF_ENUM),				ENUM(LF_DIMARRAY),			ENUM(LF_PRECOMP),
	ENUM(LF_ALIAS),				ENUM(LF_DEFARG),			ENUM(LF_FRIENDFCN),			ENUM(LF_MEMBER),			ENUM(LF_STMEMBER),			ENUM(LF_METHOD),			ENUM(LF_NESTTYPE),			ENUM(LF_ONEMETHOD),
	ENUM(LF_NESTTYPEEX),		ENUM(LF_MEMBERMODIFY),		ENUM(LF_MANAGED),			ENUM(LF_TYPESERVER2),		ENUM(LF_STRIDED_ARRAY),		ENUM(LF_HLSL),				ENUM(LF_MODIFIER_EX),		ENUM(LF_INTERFACE),
	ENUM(LF_BINTERFACE),		ENUM(LF_VECTOR),			ENUM(LF_MATRIX),			ENUM(LF_VFTABLE),			ENUM(LF_FUNC_ID),			ENUM(LF_MFUNC_ID),			ENUM(LF_BUILDINFO),			ENUM(LF_SUBSTR_LIST),
	ENUM(LF_STRING_ID),			ENUM(LF_UDT_SRC_LINE),		ENUM(LF_UDT_MOD_SRC_LINE )
); }};
#undef ENUM

ISO_DEFUSERCOMPXV(segmented32, "CV_segmented32", seg, off);
ISO_DEFUSERCOMPXV(SYMTYPE, "SYMTYPE", reclen, rectyp, data);
ISO_DEFUSERCOMPXV(TYPTYPE, "TYPTYPE", len, leaf, data);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_FUNC_MDTOKEN_MAP>::entry,	a, b);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_LINES>::line,				offset);	//	ISO_BITFIELD(linenumStart,deltaLineEnd,fStatement);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_LINES>::column,			offColumnStart, offColumnEnd);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_LINES>::entry,				offFile, cbBlock, lines);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_LINES>::entry_ex,			offFile, cbBlock, lines, columns);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_FRAMEDATA>::entry,			ulRvaStart,cbBlock,cbLocals,cbParams,cbStkMax,frameFunc,cbProlog,cbSavedRegs);//fHasSEH:1, fHasEH:1, fIsFunctionStart:1, reserved:29;
ISO_DEFCOMPV(SubSectionT<DEBUG_S_FILECHKSMS>::entry,		name, cbChecksum, ChecksumType);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_CROSSSCOPEIMPORTS>::entry,	externalScope, refs);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_CROSSSCOPEEXPORTS>::entry,	localId, globalId);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_INLINEELINES>::entry,		inlinee, fileId, sourceLineNum);
ISO_DEFCOMPV(SubSectionT<DEBUG_S_INLINEELINES>::entry_ex,	inlinee, fileId, sourceLineNum, files);


//	ISO_DEFUSERVIRT(SubSection);
//	ISO_DEFCOMPV(SubSection,				type, cbLen, data);
//	template<DEBUG_S_SUBSECTION_TYPE type> ISO_DEFUSERXT(SubSectionT, type, SubSection, "SubSection");

ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_IGNORE				>, "UNKNOWN",					type, cbLen, data);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_SYMBOLS				>, "S_SYMBOLS",					entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_LINES					>, "S_LINES",					addr, flags, cb, entries);//or entries_ex
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_STRINGTABLE			>, "S_STRINGTABLE",				entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_FILECHKSMS			>, "S_FILECHKSMS",				entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_FRAMEDATA				>, "S_FRAMEDATA",				entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_INLINEELINES			>, "S_INLINEELINES",			entries);//or entries_ex
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_CROSSSCOPEIMPORTS		>, "S_CROSSSCOPEIMPORTS",		entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_CROSSSCOPEEXPORTS		>, "S_CROSSSCOPEEXPORTS",		entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_IL_LINES				>, "S_IL_LINES",				entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_FUNC_MDTOKEN_MAP		>, "S_FUNC_MDTOKEN_MAP",		entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_TYPE_MDTOKEN_MAP		>, "S_TYPE_MDTOKEN_MAP",		entries);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_MERGED_ASSEMBLYINPUT	>, "S_MERGED_ASSEMBLYINPUT",	type, cbLen, data);
ISO_DEFUSERCOMPXV(SubSectionT<DEBUG_S_COFF_SYMBOL_RVA		>, "S_COFF_SYMBOL_RVA",			type, cbLen, data);

template<> struct ISO::def<SubSection> : ISO::VirtualT2<SubSection> {
	static ISO::Browser	Deref(SubSection &a) {
		switch (a.type) {
			case DEBUG_S_SYMBOLS:			return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_SYMBOLS			>>());
			case DEBUG_S_LINES:				return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_LINES				>>());
			case DEBUG_S_STRINGTABLE:		return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_STRINGTABLE		>>());
			case DEBUG_S_FILECHKSMS:		return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_FILECHKSMS		>>());
			case DEBUG_S_FRAMEDATA:			return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_FRAMEDATA			>>());
			case DEBUG_S_INLINEELINES:		return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_INLINEELINES		>>());
			case DEBUG_S_CROSSSCOPEIMPORTS:	return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_CROSSSCOPEIMPORTS	>>());
			case DEBUG_S_CROSSSCOPEEXPORTS:	return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_CROSSSCOPEEXPORTS	>>());
			case DEBUG_S_IL_LINES:			return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_IL_LINES			>>());
			case DEBUG_S_FUNC_MDTOKEN_MAP:	return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_FUNC_MDTOKEN_MAP	>>());
			case DEBUG_S_TYPE_MDTOKEN_MAP:	return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_TYPE_MDTOKEN_MAP	>>());
			case DEBUG_S_MERGED_ASSEMBLYINPUT:return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_MERGED_ASSEMBLYINPUT>>());
			case DEBUG_S_COFF_SYMBOL_RVA:	return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_COFF_SYMBOL_RVA	>>());
			default:						return ISO::MakeBrowser(*a.as<SubSectionT<DEBUG_S_IGNORE			>>());
		}
	}
};
}