#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "extra/date.h"
#include "filetypes/container/archive_help.h"
#include "obj.h"
#include "pe.h"
#include "cvinfo_iso.h"
#include "disassembler.h"
#include "clr.h"
#include "cil.h"
#include "hashes/fnv.h"
#include "vm.h"

using namespace iso;

constexpr GUID coff::ANON_XBOXONE::guid;
constexpr GUID coff::ANON_BIGOBJ::guid;
//-----------------------------------------------------------------------------
//	COFF
//-----------------------------------------------------------------------------

class COFF {
protected:

	GUID		ClassID;			// only if Version >= 1
	uint32		SizeOfData;
	uint32		Flags;				// Version >= 2; 0x1 -> contains metadata
	uint32		MetaDataSize;		// Size of CLR metadata
	uint32		MetaDataOffset;		// Offset of CLR metadata

public:
	uint16		machine;
	bool		big				= false;
	uint32		symbol_offset	= 0;
	uint32		num_symbols		= 0;
	malloc_block	opt;

	dynamic_array<coff::SECTION_HEADER>	sections;

	COFF(coff::MACHINE machine) : machine(machine) {}
	COFF(istream_ref file) {
		coff::FILE_HEADER	h;
		file.read(h);

		if (h.IsAnon()) {
			file.seek(0);
			coff::ANON_HEADER0	anon;
			file.read(anon);
			machine = anon.Machine;

			if (anon.Version > 0)
				file.read(ClassID);

			file.read(SizeOfData);

			if (anon.Version > 1)
				read(file, Flags, MetaDataSize, MetaDataOffset);

			if (anon.Version == 0) {
				opt.read(file, SizeOfData);
				num_symbols		= 1;

			} else if (ClassID == coff::ANON_BIGOBJ::guid) {
				coff::ANON_BIGOBJ	bigobj;
				file.read(bigobj);
				big				= true;
				symbol_offset	= bigobj.PointerToSymbolTable;
				num_symbols		= bigobj.NumberOfSymbols;
				sections.read(file, bigobj.NumberOfSections);

			} else if (ClassID == coff::ANON_XBOXONE::guid) {
				coff::ANON_XBOXONE	xb1;
				file.read(xb1);
				sections.read(file, xb1.NumberOfSections);
			}

		} else {
			machine			= h.Machine;
			symbol_offset	= h.PointerToSymbolTable;
			num_symbols		= h.NumberOfSymbols;

			if (h.SizeOfOptionalHeader)
				opt.read(file, h.SizeOfOptionalHeader);

			sections.read(file, h.NumberOfSections);
		}
	}

	const coff::SECTION_HEADER *FindSectionRVA(uint32 rva) const {
		for (auto &i : sections) {
			if (rva >= i.VirtualAddress && rva < i.VirtualAddress + i.SizeOfRawData)
				return &i;
		}
		return nullptr;
	}

	const coff::SECTION_HEADER *FindSectionRaw(uint32 addr) const {
		for (auto &i : sections) {
			if (addr >= i.PointerToRawData && addr < i.PointerToRawData + i.SizeOfRawData)
				return &i;
		}
		return nullptr;
	}

	coff::SECTION_HEADER&	AddSection(const char *name) {
		auto&	sect = sections.push_back();
		clear(sect);
		strcpy(sect.Name, name);
		return sect;
	}

	void	SetSymbolTable(streamptr offset, size_t size) {
		symbol_offset	= uint32(offset);
		num_symbols		= uint32(size);
	}
	
	bool WriteHeader(ostream_ref file) {
		coff::FILE_HEADER	h;
		h.Machine				= machine;
		h.NumberOfSections		= sections.size32();
		h.NumberOfSymbols		= num_symbols;
		h.PointerToSymbolTable	= symbol_offset;
		h.SizeOfOptionalHeader	= opt.size32();
		file.seek(0);
		return file.write(h) && file.write(sections);
	}
};

//-----------------------------------------------------------------------------
//	PE
//-----------------------------------------------------------------------------

ISO_ptr<void> ReadBmp(tag id, const memory_block &mb, bool icon);

using StartBin		= ISO::VStartBin<ISO::OpenArray<xint8, sizeof(void*) * 8> >;

struct PE : public COFF {
	dynamic_array<ISO_ptr<StartBin>>	sect_data;

	arbitrary_ptr	GetDataRVA(uint32 rva)				const;
	arbitrary_ptr	GetDataRaw(uint32 addr)				const;
	memory_block	GetDataDir(pe::DATA_DIRECTORY &dir) const;

	ISO_ptr<void>	Read(tag id, istream_ref file);
	ISO_ptr<void>	ReadResources(tag id, const void *start, uint32 offset, uint32 virtoffset, pe::RESOURCE_TYPE type);
	ISO_ptr<void>	ReadDataDir(tag id, pe::DATA_DIRECTORY &dir, int i, bool _64bit);

	PE(istream_ref file) : COFF(file) {}
};

arbitrary_ptr PE::GetDataRVA(uint32 rva) const {
	if (auto sect = FindSectionRVA(rva))
		return sect_data[sect - sections]->b + (rva - sect->VirtualAddress);
	return nullptr;
}
arbitrary_ptr PE::GetDataRaw(uint32 addr) const {
	if (auto sect = FindSectionRaw(addr))
		return sect_data[sect - sections]->b + (addr - sect->PointerToRawData);
	return arbitrary_ptr();
}

memory_block PE::GetDataDir(pe::DATA_DIRECTORY &dir) const {
	if (dir.Size) {
		if (auto *sect = FindSectionRVA(dir.VirtualAddress)) {
			if (dir.VirtualAddress + dir.Size - sect->VirtualAddress < sect->SizeOfRawData)
				return memory_block(sect_data[sect - sections]->b + dir.VirtualAddress - sect->VirtualAddress, dir.Size);
		}
	}
	return empty;
}

//-----------------------------------------------------------------------------
//	CLR
//-----------------------------------------------------------------------------

namespace iso {
template<typename T> struct field_names		{ static const char *s[]; };
template<> const char *field_names<clr::TABLETYPE>::s[] = {
	"Module",
	"TypeRef",
	"TypeDef",
	"Unused03",
	"Field",
	"Unused05",
	"MethodDef",
	"Unused07",
	"Param",
	"InterfaceImpl",
	"MemberRef",
	"Constant",
	"CustomAttribute",
	"FieldMarshal",
	"DeclSecurity",
	"ClassLayout",
	"FieldLayout",
	"StandAloneSig",
	"EventMap",
	"Unused13",
	"Event",
	"PropertyMap",
	"Unused16",
	"Property",
	"MethodSemantics",
	"MethodImpl",
	"ModuleRef",
	"TypeSpec",
	"ImplMap",
	"FieldRVA",
	"Unused1e",
	"Unused1f",
	"Assembly",
	"AssemblyProcessor",
	"AssemblyOS",
	"AssemblyRef",
	"AssemblyRefProcessor",
	"AssemblyRefOS",
	"File",
	"ExportedType",
	"ManifestResource",
	"NestedClass",
	"GenericParam",
	"MethodSpec",
	"GenericParamConstraint",
};

const char *to_string(clr::TABLETYPE t) {
	return	t < clr::NumTables		? field_names<clr::TABLETYPE>::s[t]
		:	t == clr::StringHeap	? "Strings"
		:	"?";
}

}

namespace clr {

template<> uint8 CodedIndex<_TypeDefOrRef>::trans[]			= {TypeDef, TypeRef, TypeSpec};
template<> uint8 CodedIndex<_HasConstant>::trans[]			= {Field, Param, Property};
template<> uint8 CodedIndex<_HasCustomAttribute>::trans[]	= {
	MethodDef, Field, TypeRef, TypeDef, Param, InterfaceImpl,
	MemberRef, Module, DeclSecurity, Property, Event, StandAloneSig,
	ModuleRef, TypeSpec, Assembly, AssemblyRef, File, ExportedType,
	ManifestResource, GenericParam, GenericParamConstraint, MethodSpec
};
template<> uint8 CodedIndex<_HasFieldMarshall>::trans[]		= {Field, Param};
template<> uint8 CodedIndex<_HasDeclSecurity>::trans[]		= {TypeDef, MethodDef, Assembly};
template<> uint8 CodedIndex<_MemberRefParent>::trans[]		= {TypeDef, TypeRef, ModuleRef, MethodDef, TypeSpec};
template<> uint8 CodedIndex<_HasSemantics>::trans[]			= {Event, Property};
template<> uint8 CodedIndex<_MethodDefOrRef>::trans[]		= {MethodDef, MemberRef};
template<> uint8 CodedIndex<_MemberForwarded>::trans[]		= {Field, MethodDef};
template<> uint8 CodedIndex<_Implementation>::trans[]		= {File, AssemblyRef, ExportedType};
template<> uint8 CodedIndex<_CustomAttributeType>::trans[]	= {0, 0, MethodDef, MemberRef};
template<> uint8 CodedIndex<_ResolutionScope>::trans[]		= {Module, ModuleRef, AssemblyRef, TypeRef};
template<> uint8 CodedIndex<_TypeOrMethodDef>::trans[]		= {TypeDef, MethodDef};

struct ISO_METADATA : refs<ISO_METADATA>, METADATA {
	ISO_ptr<StartBin>	sect_data;	// keep reference

	hash_map<const ENTRY<TypeRef>*, const ENTRY<TypeDef>*>	ref2def;

	template<TABLETYPE I>	ISO::Browser2	GetTable(tag id, uint32 i, uint32 n) {
		return ISO::MakePtr(id, make_range_n(make_param_iterator((ENTRY<I>*)tables[I].p + i, this), n));
	}

	ISO::Browser2	GetTable(tag id, TABLETYPE t, int i, int n) {
		switch (t) {
			case Module:				return GetTable<Module					>(id, i, n);
			case TypeRef:				return GetTable<TypeRef					>(id, i, n);
			case TypeDef:				return GetTable<TypeDef					>(id, i, n);
			case Field:					return GetTable<Field					>(id, i, n);
			case MethodDef:				return GetTable<MethodDef				>(id, i, n);
			case Param:					return GetTable<Param					>(id, i, n);
			case InterfaceImpl:			return GetTable<InterfaceImpl			>(id, i, n);
			case MemberRef:				return GetTable<MemberRef				>(id, i, n);
			case Constant:				return GetTable<Constant				>(id, i, n);
			case CustomAttribute:		return GetTable<CustomAttribute			>(id, i, n);
			case FieldMarshal:			return GetTable<FieldMarshal			>(id, i, n);
			case DeclSecurity:			return GetTable<DeclSecurity			>(id, i, n);
			case ClassLayout:			return GetTable<ClassLayout				>(id, i, n);
			case FieldLayout:			return GetTable<FieldLayout				>(id, i, n);
			case StandAloneSig:			return GetTable<StandAloneSig			>(id, i, n);
			case EventMap:				return GetTable<EventMap				>(id, i, n);
			case Event:					return GetTable<Event					>(id, i, n);
			case PropertyMap:			return GetTable<PropertyMap				>(id, i, n);
			case Property:				return GetTable<Property				>(id, i, n);
			case MethodSemantics:		return GetTable<MethodSemantics			>(id, i, n);
			case MethodImpl:			return GetTable<MethodImpl				>(id, i, n);
			case ModuleRef:				return GetTable<ModuleRef				>(id, i, n);
			case TypeSpec:				return GetTable<TypeSpec				>(id, i, n);
			case ImplMap:				return GetTable<ImplMap					>(id, i, n);
			case FieldRVA:				return GetTable<FieldRVA				>(id, i, n);
			case Assembly:				return GetTable<Assembly				>(id, i, n);
			case AssemblyProcessor:		return GetTable<AssemblyProcessor		>(id, i, n);
			case AssemblyOS:			return GetTable<AssemblyOS				>(id, i, n);
			case AssemblyRef:			return GetTable<AssemblyRef				>(id, i, n);
			case AssemblyRefProcessor:	return GetTable<AssemblyRefProcessor	>(id, i, n);
			case AssemblyRefOS:			return GetTable<AssemblyRefOS			>(id, i, n);
			case File:					return GetTable<File					>(id, i, n);
			case ExportedType:			return GetTable<ExportedType			>(id, i, n);
			case ManifestResource:		return GetTable<ManifestResource		>(id, i, n);
			case NestedClass:			return GetTable<NestedClass				>(id, i, n);
			case GenericParam:			return GetTable<GenericParam			>(id, i, n);
			case MethodSpec:			return GetTable<MethodSpec				>(id, i, n);
			case GenericParamConstraint:return GetTable<GenericParamConstraint	>(id, i, n);
			default: return ISO::Browser2();
		}
	}

	ISO::Browser2	Lookup(TABLETYPE t, uint32 i) {
		return i ? GetTable(0, t, 0, tables[t].n)[i - 1] : ISO::Browser2();
	}
	ISO::Browser2	Lookup(const Token &i) {
		return Lookup(i.type(), i.index());
	}
	template<TABLETYPE I> ISO::Browser2 Lookup(const Indexed<I> &i) {
		return i ? ISO::Browser2(ISO::MakePtr(0, make_param_element(*GetEntry<I>(i.t), this))) : ISO::Browser2();
	}

	template<TABLETYPE I>	string_accum&	Dump(string_accum &a, const ENTRY<I> &i)		{ return a; }
	template<TABLETYPE I>	string_accum&	Dump(string_accum &a, int i)					{ return i ? Dump(a, *GetEntry<I>(i)) : a; }
	template<TABLETYPE I>	string_accum&	Dump(string_accum &a, const Indexed<I> &i)		{ return i ? Dump(a, *GetEntry<I>(i)) : a; }
	string_accum&							Dump(string_accum &a, TABLETYPE t, int i);
	template<typename T>	string_accum&	Dump(string_accum &a, const CodedIndex<T> &i)	{ return Dump(a, i.type(), i.index()); }

	ELEMENT_TYPE	DumpType(string_accum &a, byte_reader &r, const char *name = 0);
	NATIVE_TYPE		DumpNativeType(string_accum &a, byte_reader &r);
	string_accum&	DumpSignature(string_accum &a, byte_reader &r, const char *name = 0);
	string_accum&	DumpSignature(string_accum &a, byte_reader &r, const char *name, range<ENTRY<Param>*> params, uint32 flags);
	ISO_ptr<void>	GetValue(tag2 id, const ENTRY<TypeDef> *type, byte_reader &data);
	ISO_ptr<void>	GetValue(tag2 id, byte_reader &r, byte_reader &data);
	ISO_ptr<void>	GetValues(byte_reader &r, byte_reader &data, range<ENTRY<Param>*> params);

	const ENTRY<TypeDef> *GetTypeDef(const ENTRY<TypeRef> *ref) const {
		return ref2def[ref].or_default();
	}

	const ENTRY<TypeDef> *GetTypeDef(Token tok) const {
		switch (tok.type()) {
			case TypeDef:
				return GetEntry<TypeDef>(tok.index());

			case TypeRef:
				return ref2def[GetEntry<TypeRef>(tok.index())].or_default();

			default:
				return 0;
		}
	}
};

NATIVE_TYPE ISO_METADATA::DumpNativeType(string_accum &a, byte_reader &r) {
	NATIVE_TYPE	type = NATIVE_TYPE(r.getc());
	switch (type) {
		case NATIVE_TYPE_BOOLEAN:	a << "bool"; break;
		case NATIVE_TYPE_I1:		a << "int8"; break;
		case NATIVE_TYPE_U1:		a << "uint8"; break;
		case NATIVE_TYPE_I2:		a << "int16"; break;
		case NATIVE_TYPE_U2:		a << "uint16"; break;
		case NATIVE_TYPE_I4:		a << "int32"; break;
		case NATIVE_TYPE_U4:		a << "uint32"; break;
		case NATIVE_TYPE_I8:		a << "int64"; break;
		case NATIVE_TYPE_U8:		a << "uint64"; break;
		case NATIVE_TYPE_R4:		a << "float"; break;
		case NATIVE_TYPE_R8:		a << "double"; break;
		case NATIVE_TYPE_LPSTR:		a << "const char*"; break;
		case NATIVE_TYPE_LPWSTR:	a << "const wchar_t*"; break;
		case NATIVE_TYPE_INT:		a << "int"; break;
		case NATIVE_TYPE_UINT:		a << "unsigned int"; break;
		case NATIVE_TYPE_FUNC:		a << "func"; break;
		case NATIVE_TYPE_ARRAY: {
			DumpNativeType(a, r);
			CompressedUInt	param_num(r);
			CompressedUInt	num_elem(r);
			a << '[';
			if (param_num) {
				a << '@' << param_num.t;
				if (num_elem)
					a << '+';
			}
			a << num_elem << ']';
			break;
		}
		default:
			a << '?';
			break;
	}
	return type;
}

string_accum& ISO_METADATA::Dump(string_accum &a, TABLETYPE t, int i) {
	switch (t) {
		case Module:				return Dump<Module					>(a, i);
		case TypeRef:				return Dump<TypeRef					>(a, i);
		case TypeDef:				return Dump<TypeDef					>(a, i);
		case Field:					return Dump<Field					>(a, i);
		case MethodDef:				return Dump<MethodDef				>(a, i);
		case Param:					return Dump<Param					>(a, i);
		case InterfaceImpl:			return Dump<InterfaceImpl			>(a, i);
		case MemberRef:				return Dump<MemberRef				>(a, i);
		case Constant:				return Dump<Constant				>(a, i);
		case CustomAttribute:		return Dump<CustomAttribute			>(a, i);
		case FieldMarshal:			return Dump<FieldMarshal			>(a, i);
		case DeclSecurity:			return Dump<DeclSecurity			>(a, i);
		case ClassLayout:			return Dump<ClassLayout				>(a, i);
		case FieldLayout:			return Dump<FieldLayout				>(a, i);
		case StandAloneSig:			return Dump<StandAloneSig			>(a, i);
		case EventMap:				return Dump<EventMap				>(a, i);
		case Event:					return Dump<Event					>(a, i);
		case PropertyMap:			return Dump<PropertyMap				>(a, i);
		case Property:				return Dump<Property				>(a, i);
		case MethodSemantics:		return Dump<MethodSemantics			>(a, i);
		case MethodImpl:			return Dump<MethodImpl				>(a, i);
		case ModuleRef:				return Dump<ModuleRef				>(a, i);
		case TypeSpec:				return Dump<TypeSpec				>(a, i);
		case ImplMap:				return Dump<ImplMap					>(a, i);
		case FieldRVA:				return Dump<FieldRVA				>(a, i);
		case Assembly:				return Dump<Assembly				>(a, i);
		case AssemblyProcessor:		return Dump<AssemblyProcessor		>(a, i);
		case AssemblyOS:			return Dump<AssemblyOS				>(a, i);
		case AssemblyRef:			return Dump<AssemblyRef				>(a, i);
		case AssemblyRefProcessor:	return Dump<AssemblyRefProcessor	>(a, i);
		case AssemblyRefOS:			return Dump<AssemblyRefOS			>(a, i);
		case File:					return Dump<File					>(a, i);
		case ExportedType:			return Dump<ExportedType			>(a, i);
		case ManifestResource:		return Dump<ManifestResource		>(a, i);
		case NestedClass:			return Dump<NestedClass				>(a, i);
		case GenericParam:			return Dump<GenericParam			>(a, i);
		case MethodSpec:			return Dump<MethodSpec				>(a, i);
		case GenericParamConstraint:return Dump<GenericParamConstraint	>(a, i);
		default: return a;
	}
}

template<> string_accum& ISO_METADATA::Dump(string_accum &a, const ENTRY<TypeDef> &i) {
	if (i.namespce && i.namespce[0])
		a << i.namespce << '.';
	return a << i.name;
}
template<> string_accum& ISO_METADATA::Dump(string_accum &a, const ENTRY<TypeRef> &i) {
	if (i.namespce && i.namespce[0])
		a << i.namespce << '.';
	return a << i.name;
}
template<> string_accum& ISO_METADATA::Dump(string_accum &a, const ENTRY<TypeSpec> &i) {
//	return DumpSignature(a, byte_reader(i.signature));
	DumpType(a, unconst(byte_reader(i.signature)));
	return a;
}

void DumpSystemRuntimeCompilerServices(string_accum &a, const char *_name) {
	auto	name = str(_name);
	if (name == "AsyncStateMachineAttribute") {
	} else if (name == "CallConvCdecl") {
	 	a << "__cdecl ";
	} else if (name == "CallConvFastcall") {
	 	a << "__fastcall ";
	} else if (name == "CallConvStdcall") {
	 	a << "__stdcall ";
	} else if (name == "CallConvThiscall") {
	 	a << "__thiscall ";
	} else if (name == "IsBoxed") {
	} else if (name == "IsByValue") {
	} else if (name == "IsConst") {
	 	a << "const ";
	} else if (name == "IsCopyConstructed") {
	} else if (name == "IsExplicitlyDereferenced") {
	} else if (name == "IsImplicitlyDereferenced") {
	} else if (name == "IsJitIntrinsic") {
	} else if (name == "IsLong") {
	} else if (name == "IsPinned") {
	} else if (name == "IsSignUnspecifiedByte") {
	} else if (name == "IsUdtReturn") {
	} else if (name == "IsVolatile") {
		a << "volatile ";
	} else if (name == "ScopelessEnumAttribute") {
	}
}

ELEMENT_TYPE ISO_METADATA::DumpType(string_accum &a, byte_reader &r, const char *name) {
	for (;;) {
		ELEMENT_TYPE	t = ELEMENT_TYPE(r.getc());
		switch (t) {
			case ELEMENT_TYPE_CMOD_REQD:
				Dump(a, TypeDefOrRef(CompressedUInt(r))) << ' ';
				continue;

			case ELEMENT_TYPE_CMOD_OPT: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				if (x.type() == TypeRef && GetEntry<TypeRef>(x.index())->namespce == "System.Runtime.CompilerServices")
					DumpSystemRuntimeCompilerServices(a, GetEntry<TypeRef>(x.index())->name);
				else
					Dump(a << "opt(", x) << ") ";
				continue;
			}
			case ELEMENT_TYPE_SYSTEMTYPE:
				a << "system ";
				continue;

			case ELEMENT_TYPE_PINNED:
				a << "pinned ";
				continue;

			case ELEMENT_TYPE_BYREF:
				a << "byref ";
				continue;

			case ELEMENT_TYPE_CLASS:
			case ELEMENT_TYPE_VALUETYPE:
				Dump(a, TypeDefOrRef(CompressedUInt(r)));
				break;

			case ELEMENT_TYPE_TYPEDBYREF:	 a << "typedbyref"; break;
			case ELEMENT_TYPE_VOID:			 a << "void"; break;
			case ELEMENT_TYPE_OBJECT:		 a << "object"; break;
			case ELEMENT_TYPE_STRING:		 a << "string"; break;
			case ELEMENT_TYPE_INTERNAL:		 a << "internal"; break;

			case ELEMENT_TYPE_BOOLEAN:		 a << "bool"; break;
			case ELEMENT_TYPE_CHAR:			 a << "char"; break;
			case ELEMENT_TYPE_I1:			 a << "int8"; break;
			case ELEMENT_TYPE_U1:			 a << "uint8"; break;
			case ELEMENT_TYPE_I2:			 a << "int16"; break;
			case ELEMENT_TYPE_U2:			 a << "uint16"; break;
			case ELEMENT_TYPE_I4:			 a << "int32"; break;
			case ELEMENT_TYPE_U4:			 a << "uint32"; break;
			case ELEMENT_TYPE_I8:			 a << "int64"; break;
			case ELEMENT_TYPE_U8:			 a << "uint64"; break;
			case ELEMENT_TYPE_R4:			 a << "float"; break;
			case ELEMENT_TYPE_R8:			 a << "double"; break;
			case ELEMENT_TYPE_I:			 a << "int"; break;
			case ELEMENT_TYPE_U:			 a << "uint"; break;

			case ELEMENT_TYPE_ARRAY: {
				DumpType(a, r);
				CompressedUInt	rank(r);
				CompressedUInt	num_sizes(r);
				for (uint32 i = num_sizes; i--;)
					a << '[' << (int)CompressedUInt(r) << ']';
				for (uint32 i = rank - num_sizes; i--;)
					a << "[]";

				CompressedUInt	num_lobounds(r);
				for (uint32 i = num_lobounds; i--;)
					CompressedInt	lobound(r);
				return t;
			}
			case ELEMENT_TYPE_FNPTR:
				DumpSignature(a, r, name);
				return t;

			case ELEMENT_TYPE_GENERICINST: {
				ELEMENT_TYPE	t2 = ELEMENT_TYPE(r.getc());	// ELEMENT_TYPE_CLASS or ELEMENT_TYPE_VALUETYPE
				Dump(a, TypeDefOrRef(CompressedUInt(r)));
				a << '<';
				CompressedUInt	gen_arg_count(r);
				for (uint32 i = 0; i < gen_arg_count; ++i) {
					if (i)
						a << ", ";
					DumpType(a, r);
				}
				a << '>';
				break;
			}
			case ELEMENT_TYPE_MVAR:
			case ELEMENT_TYPE_VAR:
				a << 'T' << (int)CompressedUInt(r);
				break;

			case ELEMENT_TYPE_PTR:
				if (name)
					DumpType(a, r, str("*") + name);
				else
					DumpType(a, r, "*");
				return t;

			case ELEMENT_TYPE_SZARRAY:
				a << "array<";
				DumpType(a, r);
				a << '>';
				break;

			case ELEMENT_TYPE_END:
			case ELEMENT_TYPE_SENTINEL:
			case ELEMENT_TYPE_BOXED:
			case ELEMENT_TYPE_FIELD:
			case ELEMENT_TYPE_PROPERTY:
			case ELEMENT_TYPE_ENUM:
				break;
		}
		if (name)
			a << ' ' << name;

		return t;
	}
}

string_accum &ISO_METADATA::DumpSignature(string_accum &a, byte_reader &r, const char *name) {
	SIGNATURE	flags = SIGNATURE(r.getc());

	switch (flags & TYPE_MASK) {
		case GENERIC: {
			CompressedUInt	gen_param_count(r);
			a << '<' << uint32(gen_param_count) << "> ";
			// fall through
		}
		case DEFAULT:
		case C:
		case STDCALL:
		case THISCALL:
		case FASTCALL:
		case VARARG: {
			CompressedUInt	param_count(r);
			// return type
			DumpType(a, r, name);
			a << '(';
			// params
			for (int i = param_count; i--;) {
				ELEMENT_TYPE	t = DumpType(a, r);
				if (t == ELEMENT_TYPE_SENTINEL)
					t = DumpType(a, r);
				if (i)
					a << ", ";
			}
			a << ')';
			break;
		}
		case FIELD:
			DumpType(a, r, name);
			break;

		case LOCAL_SIG:
			for (int i = CompressedUInt(r); i--;) {
				ELEMENT_TYPE	t = DumpType(a, r);
				if (t == ELEMENT_TYPE_SENTINEL)
					t = DumpType(a, r);
				a << "; ";
			}
			break;

		case PROPERTY: {
			CompressedUInt	param_count(r);
			// type
			DumpType(a, r);
			// params
			if (int i = param_count) {
				a << '[';
				while (i--) {
					ELEMENT_TYPE	t = DumpType(a, r);
					if (t == ELEMENT_TYPE_SENTINEL)
						t = DumpType(a, r);
					if (i)
						a << ", ";
				}
				a << ']';
			}
			break;
		}
	}
	return a;
}

string_accum &ISO_METADATA::DumpSignature(string_accum &a, byte_reader &r, const char *name, range<ENTRY<Param>*> params, uint32 flags) {
	SIGNATURE	f		= SIGNATURE(r.getc());
	auto		*pname	= params.begin();

	switch (f & TYPE_MASK) {
		case GENERIC: {
			CompressedUInt	gen_param_count(r);
			a << '<' << uint32(gen_param_count) << "> ";
			// fall through
		}
		case DEFAULT:
		case C:
		case STDCALL:
		case THISCALL:
		case FASTCALL:
		case VARARG: {
			CompressedUInt	param_count(r);
			// return type
			if (flags & 1) {
				ISO_ASSERT(r.getc() == ELEMENT_TYPE_VOID);
				a << name;
			} else {
				DumpType(a, r, name);
			}
			a << '(';
			// params
			for (int i = param_count; i--; ++pname) {
				ELEMENT_TYPE	t = DumpType(a, r, pname->name);
				if (t == ELEMENT_TYPE_SENTINEL)
					t = DumpType(a, r, pname->name);
				if (i)
					a << ", ";
			}
			a << ')';
			break;
		}
		case FIELD:
			DumpType(a, r, name);
			break;

		case LOCAL_SIG:
			for (int i = CompressedUInt(r); i--; ++pname) {
				ELEMENT_TYPE	t = DumpType(a, r, pname->name);
				if (t == ELEMENT_TYPE_SENTINEL)
					t = DumpType(a, r);
				a << "; ";
			}
			break;

		case PROPERTY: {
			CompressedUInt	param_count(r);
			// type
			DumpType(a, r);
			// params
			if (int i = param_count) {
				a << '[';
				while (i--) {
					ELEMENT_TYPE	t = DumpType(a, r, pname++->name);
					if (t == ELEMENT_TYPE_SENTINEL)
						t = DumpType(a, r);
					if (i)
						a << ", ";
				}
				a << ']';
			}
			break;
		}
		default:
			ISO_ASSERT(0);
			break;
	}
	return a;
}

ISO_ptr<void> ISO_METADATA::GetValue(tag2 id, const ENTRY<TypeDef> *type, byte_reader &data) {
	if (type->extends.type() == TypeRef && GetEntry<TypeRef>(type->extends.index())->name == "Enum") {
		return ISO::MakePtr("enum", data.get<int>());
	}
	return ISO_ptr<int>(tag2(), 1);
}

ISO_ptr<void> GetSimpleValue(tag2 id, ELEMENT_TYPE t, byte_reader &data) {
	switch (t) {
		case ELEMENT_TYPE_SYSTEMTYPE:
		case ELEMENT_TYPE_ENUM:
		case ELEMENT_TYPE_STRING:		 return ISO::MakePtr<string>(id, SerString(data)); break;
		case ELEMENT_TYPE_BOOLEAN:		 return ISO::MakePtr(id, data.get<bool8>()); break;
		case ELEMENT_TYPE_CHAR:			 return ISO::MakePtr(id, data.get<wchar_t>()); break;
		case ELEMENT_TYPE_I1:			 return ISO::MakePtr(id, data.get<int8>()); break;
		case ELEMENT_TYPE_U1:			 return ISO::MakePtr(id, data.get<uint8>()); break;
		case ELEMENT_TYPE_I2:			 return ISO::MakePtr(id, data.get<int16>()); break;
		case ELEMENT_TYPE_U2:			 return ISO::MakePtr(id, data.get<uint16>()); break;
		case ELEMENT_TYPE_I4:			 return ISO::MakePtr(id, data.get<int32>()); break;
		case ELEMENT_TYPE_U4:			 return ISO::MakePtr(id, data.get<uint32>()); break;
		case ELEMENT_TYPE_I8:			 return ISO::MakePtr(id, data.get<int64>()); break;
		case ELEMENT_TYPE_U8:			 return ISO::MakePtr(id, data.get<uint64>()); break;
		case ELEMENT_TYPE_R4:			 return ISO::MakePtr(id, data.get<float>()); break;
		case ELEMENT_TYPE_R8:			 return ISO::MakePtr(id, data.get<double>()); break;
		case ELEMENT_TYPE_I:			 return ISO::MakePtr(id, data.get<int>()); break;
		case ELEMENT_TYPE_U:			 return ISO::MakePtr(id, data.get<unsigned>()); break;

		case ELEMENT_TYPE_SZARRAY: {
			ELEMENT_TYPE	t2 = ELEMENT_TYPE(data.getc());
			uint32			num	= data.get<uint32>();
			ISO_ptr<anything>	p(id);
			while (num--)
				p->Append(GetSimpleValue(0, t2, data));
			return p;
		}

	}
	return ISO_NULL;
}

ISO_ptr<void> ISO_METADATA::GetValue(tag2 id, byte_reader &r, byte_reader &data) {
	for (;;) {
		ELEMENT_TYPE	t = ELEMENT_TYPE(r.getc());
		switch (t) {
			case ELEMENT_TYPE_CMOD_REQD: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				continue;
			}
			case ELEMENT_TYPE_CMOD_OPT: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				continue;
			}

			case ELEMENT_TYPE_PINNED:
				continue;

			case ELEMENT_TYPE_BYREF:
				continue;

			case ELEMENT_TYPE_CLASS:
			case ELEMENT_TYPE_VALUETYPE: {
				TypeDefOrRef	x = TypeDefOrRef(CompressedUInt(r));
				const ENTRY<TypeDef>	*def = 0;
				switch (x.type()) {
					case TypeRef: {
						ENTRY<TypeRef>	*ref = GetEntry<TypeRef>(x.index());
						if (ref->namespce == "System") {
							if (ref->name == "Type")
								return GetSimpleValue(id, ELEMENT_TYPE_SYSTEMTYPE, data);
						}
						def = GetTypeDef(ref);
						break;
					}
					case TypeDef:
						def = GetEntry<TypeDef>(x.index());
						break;
				}
				if (def)
					return GetValue(id, def, data);
				break;
			}
			case ELEMENT_TYPE_OBJECT:
			case ELEMENT_TYPE_BOXED:
				return GetSimpleValue(id, ELEMENT_TYPE(r.getc()), data);

			case ELEMENT_TYPE_ARRAY: {
				//DumpType(a, r);
				CompressedUInt	rank(r);
				CompressedUInt	num_sizes(r);
				for (uint32 i = num_sizes; i--;) {
					CompressedUInt	size(r);
				}
				for (uint32 i = rank - num_sizes; i--;)
					;

				CompressedUInt	num_lobounds(r);
				for (uint32 i = num_lobounds; i--;)
					CompressedInt	lobound(r);
				break;
			}

			default:
				return GetSimpleValue(id, t, data);
		}
	}
}

ISO_ptr<void> ISO_METADATA::GetValues(byte_reader &r, byte_reader &data, range<ENTRY<Param>*> params) {
	SIGNATURE	f		= SIGNATURE(r.getc());
	uint16		prolog	= data.get();
	ISO_ASSERT(prolog == 1);

	ISO_ptr<anything>	a("value");
	auto		*param	= params.begin();

	switch (f & TYPE_MASK) {
		case GENERIC: {
			CompressedUInt	gen_param_count(r);
			// fall through
		}
		case DEFAULT:
		case C:
		case STDCALL:
		case THISCALL:
		case FASTCALL:
		case VARARG: {
			CompressedUInt	param_count(r);
			// return type
			ISO_VERIFY(r.getc() == ELEMENT_TYPE_VOID);
			// params
			for (int i = 0; i < param_count; ++i)
				a->Append(GetValue(param ? (const char*)param++->name : 0, r, data));
			break;
		}
		case FIELD:
			a->Append(GetValue(0, r, data));
			break;

		case LOCAL_SIG:
			for (int i = CompressedUInt(r); i--;)
				a->Append(GetValue(0, r, data));
			break;

		case PROPERTY: {
			CompressedUInt	param_count(r);
			// type
			a->Append(GetValue(0, r, data));
			// params
			for (int i = 0; i < param_count; ++i)
				a->Append(GetValue(param ? (const char*)param++->name : 0, r, data));
			break;
		}
		default:
			ISO_ASSERT(0);
			break;
	}

	int	num_named = data.get<uint16>();
	for (int i = num_named; i--;) {
		ELEMENT_TYPE	t = ELEMENT_TYPE(data.getc());
		switch (t) {
			case ELEMENT_TYPE_FIELD:
			case ELEMENT_TYPE_PROPERTY: {
				ELEMENT_TYPE	t2 = ELEMENT_TYPE(data.getc());
				a->Append(GetSimpleValue(SerString(data), t2, data));
			}
		}
	}

	return a;
}

tag2 _GetName(const ENTRY<Module			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<TypeRef			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<TypeDef			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<Field				>	&t) { return t.name; }
tag2 _GetName(const ENTRY<MethodDef			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<Param				>	&t) { return t.name; }
tag2 _GetName(const ENTRY<MemberRef			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<Event				>	&t) { return t.name; }
tag2 _GetName(const ENTRY<Property			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<ModuleRef			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<ImplMap			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<Assembly			>	&t) { return t.name; }
tag2 _GetName(const ENTRY<AssemblyRef		>	&t) { return t.name; }
tag2 _GetName(const ENTRY<File				>	&t) { return t.name; }
tag2 _GetName(const ENTRY<ExportedType		>	&t) { return t.name; }
tag2 _GetName(const ENTRY<ManifestResource	>	&t) { return t.name; }
tag2 _GetName(const ENTRY<GenericParam		>	&t) { return t.name; }

template<typename A, typename B> uint64 namespce_name(const A &namespce, const B &name) {
	hash_stream<FNV<64>>	fnv;
	fnv.write(namespce);
	fnv.putc('.');
	fnv.write(name);
	return fnv;
}
template<typename A> uint64 namespce_name(const A &qual) {
	hash_stream<FNV<64>>	fnv;
	fnv.write(qual);
	return fnv;
}

struct CLR {
	uint16			MajorRuntimeVersion;
	uint16			MinorRuntimeVersion;
	uint32			Flags;
	uint64			valid_tables;
	ref_ptr<ISO_METADATA>	MetaData;
	uint32			_EntryPoint;
	memory_block	Resources;
	memory_block	StrongNameSignature;
	memory_block	CodeManagerTable;
	memory_block	VTableFixups;
	memory_block	ExportAddressTableJumps;
	memory_block	ManagedNativeHeader;

	ISO::Browser2	EntryPoint() const {
		if (Flags & HEADER::FLAGS_NATIVE_ENTRYPOINT)
			return ISO::MakeBrowser((xint32&)_EntryPoint);
		else
			return MetaData->Lookup(Token(_EntryPoint));
	}

	void init(PE *pe, HEADER *header) {
		MajorRuntimeVersion		= header->MajorRuntimeVersion;
		MinorRuntimeVersion		= header->MinorRuntimeVersion;
		Flags					= header->Flags;
		_EntryPoint				= header->EntryPoint;

		MetaData	= new ISO_METADATA;
		MetaData->sect_data	= pe->sect_data[0];//hold a reference

		METADATA_ROOT	*meta_root	= (clr::METADATA_ROOT*)pe->GetDataDir(header->MetaData);
		TABLES			*tables		= 0;

		for (auto &h : meta_root->Headers()) {
			memory_block	mem((char*)meta_root + h.Offset, h.Size);
			if (h.Name == "#~")				tables								= mem;
			else if (h.Name == "#Strings")	MetaData->heaps[HEAP_String]		= mem;
			else if (h.Name == "#US")		MetaData->heaps[HEAP_UserString]	= mem;
			else if (h.Name == "#GUID")		MetaData->heaps[HEAP_GUID]			= mem;
			else if (h.Name == "#Blob")		MetaData->heaps[HEAP_Blob]			= mem;
		}

		valid_tables	= tables->Valid;
		void		*p	= MetaData->InitTables(tables);
		METADATA_READER	r(p, MetaData, tables->HeapSizes);
		auto		code_getter = [pe](uint32 rva, bool unmanaged) {
			if (rva) {
				const void	*p		= pe->GetDataRVA(rva);
				uint32		len		= 0x100;
				if (unmanaged) {
					if (Disassembler *dis = Disassembler::Find("Intel 64")) {
						uint8	*p2 = (uint8*)p;
						while (*p2 != 0xcc) {
							uint8	*p0 = p2;
							p2 += dis->GetInstructionInfo(memory_block(p2, 32)).offset;
							if (p0 == p2 || *p0 == 0xff)
								break;
						}
						len = uint32(p2 - (uint8*)p);
					}
				} else {
					len	= uint32((const uint8*)((const ILMETHOD*)p)->GetCode().end() - (const uint8*)p);
				}
				return iso::const_memory_block(p, len);
			}
			return iso::const_memory_block();
		};
		r.GetCode = &code_getter;
		for (uint64 b = tables->Valid; b; b = clear_lowest(b))
			r.ReadTable((TABLETYPE)lowest_set_index(b));

		hash_map<uint64, const ENTRY<TypeDef>*>	name2def;
		for (auto &i : MetaData->METADATA::GetTable<TypeDef>())
			name2def[namespce_name(i.namespce, i.name)] = &i;

		for (auto &i : MetaData->METADATA::GetTable<TypeRef>())
			MetaData->ref2def[&i] = name2def[namespce_name(i.namespce, i.name)];

		for (int i = 0; i < 6; i++)
			(&Resources)[i] = pe->GetDataDir((&header->Resources)[i]);

	}
	struct _Tables : ISO::VirtualDefaults {
		ISO_METADATA	*meta;
		uint64			valid_tables;
		_Tables(const CLR &clr) : meta(clr.MetaData), valid_tables(clr.valid_tables) {}

		uint32			Count()	const {
			return count_bits(valid_tables);
		}
		ISO::Browser2	Index(int i) {
			int	j = nth_set_index(valid_tables, i);
			return meta->GetTable(field_names<clr::TABLETYPE>::s[j], TABLETYPE(j), 0, meta->tables[j].n);
		}
		int				GetIndex(const tag2 &id, int from) {
			auto	f = find(field_names<clr::TABLETYPE>::s, id);
			if (f != end(field_names<clr::TABLETYPE>::s))
				return count_bits(bits64(uint32(f - field_names<clr::TABLETYPE>::s)) & valid_tables);
			return -1;
		}
	};
	struct _Heaps  {
		memory_block	String;
		memory_block	GUID;
		memory_block	Blob;
		memory_block	UserString;
	};

	_Tables	Tables()	const { return *this; }
	_Heaps	Heaps()		const { return (_Heaps&)MetaData->heaps; }
};

} //namespace clr

//-----------------------------------------------------------------------------
//	RESOURCES
//-----------------------------------------------------------------------------

ISO_ptr<void> PE::ReadResources(tag id, const void *start, uint32 offset, uint32 virtoffset, pe::RESOURCE_TYPE type) {
	static const char *RESOURCE_NAMES[] = {
		"ID_0",		"CURSOR",		"BITMAP",		"ICON",			"MENU",				"DIALOG",		"STRING",		"FONTDIR",
		"FONT",		"ACCELERATOR",	"RCDATA",		"MESSAGETABLE",	"GROUP_CURSOR",		"ID_13",		"GROUP_ICON",	"ID_15",
		"VERSION",	"DLGINCLUDE",	"ID_18",		"PLUGPLAY",		"VXD",				"ANICURSOR",	"ANIICON",		"HTML",
		"MANIFEST",
	};
	byte_reader							file((uint8*)start + offset);
	const pe::RESOURCE_DIRECTORY		*dir		= file.get_ptr();
	int									num			= dir->size();
	const pe::RESOURCE_DIRECTORY_ENTRY	*entries	= file.get_ptr(num);

	ISO_ptr<anything>	array(id, num);
	for (int i = 0; i < num; i++) {
		char	name8[256];
		if (entries[i].NameIsString) {
			char16	*name16 = (char16*)((uint8*)start + entries[i].NameOffset);
			size_t	len		= chars_copy(name8, name16 + 1, name16[0]);
			name8[len]		= 0;

		} else if (offset == 0 && entries[i].Id < num_elements(RESOURCE_NAMES)) {
			type = pe::RESOURCE_TYPE(entries[i].Id);
			strcpy(name8, RESOURCE_NAMES[entries[i].Id]);

		} else {
			sprintf(name8, "ID_%i", entries[i].Id);
		}

		if (entries[i].DataIsDirectory) {
			(*array)[i] = ReadResources(name8, start, entries[i].OffsetToDirectory, virtoffset, type);

		} else {
			pe::RESOURCE_DATA_ENTRY	*data	= (pe::RESOURCE_DATA_ENTRY*)((uint8*)start + entries[i].OffsetToData);
			memory_block		mb((uint8*)start + (data->OffsetToData - virtoffset), data->Size);

			tag2			id	= name8;
			ISO_ptr<void>	&p	= (*array)[i];
			switch (type) {
				case pe::IRT_CURSOR: {
					int16	hotx = file.get<int16>();
					int16	hoty = file.get<int16>();
				}
				case pe::IRT_ICON:
					p = ReadBmp(id, mb, true);
					break;

				case pe::IRT_BITMAP:
					p = ReadBmp(id, mb, false);
					break;

				case pe::IRT_STRING: {
					p = ISO_ptr<string16>(id, string16(str((char16*)mb, data->Size / 2)));
					break;
				}
				default:
					p = ReadRaw(id, memory_reader(mb).me(), data->Size);
					break;
			}
		}
	}

	return array;
}
//-----------------------------------------------------------------------------
//	EXPORTS
//-----------------------------------------------------------------------------

struct EXPORT_DIRECTORY {
	pe::TIMEDATE	TimeDateStamp;
	uint16			MajorVersion;
	uint16			MinorVersion;
	string			DLLName;
	anything		Entries;

	EXPORT_DIRECTORY(PE *pe, const memory_block &mem) {
		const pe::EXPORT_DIRECTORY *exp_dir = mem;
		TimeDateStamp	= exp_dir->TimeDateStamp;
		MajorVersion	= exp_dir->MajorVersion;
		MinorVersion	= exp_dir->MinorVersion;
		DLLName			= (char*)pe->GetDataRVA(exp_dir->DLLName);

		const uint32		*addresses	= pe->GetDataRVA(exp_dir->FunctionTable);
		const uint32		*names		= pe->GetDataRVA(exp_dir->NameTable);
		const uint16		*ordinals	= pe->GetDataRVA(exp_dir->OrdinalTable);
		bool				nodemangle	= !!ISO::root("variables")["nodemangle"].GetInt();
		for (int i = 0; i < exp_dir->NumberEntries; i++) {
			if (auto sect = pe->FindSectionRVA(addresses[i])) {
				uint32		ordinal	= (ordinals && i < exp_dir->NumberNames ? ordinals[i] : i) + exp_dir->OrdinalBase;
				const char	*name	= names && i < exp_dir->NumberNames ? (const char*)pe->GetDataRVA(names[i]) : "";
				auto		name2	= format_string("#%i: %s", ordinal, nodemangle ? name : (char*)demangle(name));
				void		*dest	= pe->GetDataRVA(addresses[i]);

				if (mem.contains(dest))
					Entries.Append(ISO_ptr<string>(name2, (const char*)dest));
				else
					Entries.Append(ISO_ptr<pair<string, xint32> >(name2, make_pair((char*)sect->Name, addresses[i])));
			}
		}
	}
};
ISO_DEFUSERCOMPV(EXPORT_DIRECTORY,TimeDateStamp,MajorVersion,MinorVersion,DLLName,Entries);

//-----------------------------------------------------------------------------
//	TLS
//-----------------------------------------------------------------------------

template<> struct ISO::def<coff::TIMEDATE> : ISO::VirtualT2<coff::TIMEDATE> {
	static ISO_ptr<void>	Deref(const pe::TIMEDATE &t) {
		return ISO_ptr<string>(0, to_string(DateTime::FromUnixTime(DateTime::Secs(t.secs_from1970))));
	}
};

ISO_DEFUSERCOMPXV(pe::TLS_DIRECTORY32, "TLS_DIRECTORY", StartAddressOfRawData, EndAddressOfRawData, AddressOfIndex, AddressOfCallBacks, SizeOfZeroFill, Characteristics);
ISO_DEFUSERCOMPXV(pe::TLS_DIRECTORY64, "TLS_DIRECTORY", StartAddressOfRawData, EndAddressOfRawData, AddressOfIndex, AddressOfCallBacks, SizeOfZeroFill, Characteristics);
ISO_DEFUSERCOMPXV(pe::CODE_INTEGRITY, "INTEGRITY", Flags, Catalog, CatalogOffset, Reserved);

ISO_DEFUSERCOMPXV(pe::LOAD_CONFIG_DIRECTORY32, "LOAD_CONFIG_DIRECTORY", Size,TimeDateStamp,MajorVersion,MinorVersion,GlobalFlagsClear,GlobalFlagsSet,CriticalSectionDefaultTimeout,DeCommitFreeBlockThreshold,
	DeCommitTotalFreeThreshold,LockPrefixTable,MaximumAllocationSize,VirtualMemoryThreshold,ProcessAffinityMask,ProcessHeapFlags,CSDVersion,Reserved1,
	EditList,SecurityCookie,SEHandlerTable,SEHandlerCount,GuardCFCheckFunctionPointer,GuardCFDispatchFunctionPointer,GuardCFFunctionTable,GuardCFFunctionCount,
	GuardFlags, CodeIntegrity);
ISO_DEFUSERCOMPXV(pe::LOAD_CONFIG_DIRECTORY64, "LOAD_CONFIG_DIRECTORY", Size,TimeDateStamp,MajorVersion,MinorVersion,GlobalFlagsClear,GlobalFlagsSet,CriticalSectionDefaultTimeout,DeCommitFreeBlockThreshold,
	DeCommitTotalFreeThreshold,LockPrefixTable,MaximumAllocationSize,VirtualMemoryThreshold,ProcessAffinityMask,ProcessHeapFlags,CSDVersion,Reserved1,
	EditList,SecurityCookie,SEHandlerTable,SEHandlerCount,GuardCFCheckFunctionPointer,GuardCFDispatchFunctionPointer,GuardCFFunctionTable,GuardCFFunctionCount,
	GuardFlags, CodeIntegrity);

ISO_DEFUSERCOMPXV(pe::EXCEPTION_DIRECTORY::ENTRY, "EXCEPTION_ENTRY", BeginAddress,EndAddress,UnwindInfoAddress);
ISO_DEFUSERCOMPXV(pe::DEBUG_DIRECTORY, "DEBUG_DIRECTORY",Characteristics,TimeDateStamp,MajorVersion,MinorVersion,Type,SizeOfData,AddressOfRawData,PointerToRawData);

//-----------------------------------------------------------------------------
//	PE
//-----------------------------------------------------------------------------

ISO_ptr<void> ReadThunk(PE *pe, pe::IMPORT_DIRECTORY::THUNK_DATA32 *ptr, xint32 val = 0) {
	if (ptr->is_ordinal()) {
		return ISO::MakePtr(format_string("ordinal_%i", ptr->Ordinal & ~ ptr->ORDINAL), val);
	} else {
		if (pe::IMPORT_DIRECTORY::BY_NAME *byname = pe->GetDataRVA(ptr->AddressOfData))
			return ISO::MakePtr((char*)byname->name, val);
	}
	return ISO_NULL;
}
ISO_ptr<void> ReadThunk(PE *pe, pe::IMPORT_DIRECTORY::THUNK_DATA64 *ptr, xint64 val = 0) {
	if (ptr->is_ordinal()) {
		return ISO::MakePtr(format_string("ordinal_%i", ptr->Ordinal & ~ ptr->ORDINAL), val);
	} else {
		if (pe::IMPORT_DIRECTORY::BY_NAME *byname = pe->GetDataRVA(uint32(ptr->AddressOfData)))
			return ISO::MakePtr((char*)byname->name, val);
	}
	return ISO_NULL;
}

ISO_ptr<void> PE::ReadDataDir(tag id, pe::DATA_DIRECTORY &dir, int i, bool _64bit) {
	if (memory_block mem = GetDataDir(dir)) {
		switch (i) {
			case pe::DATA_DIRECTORY::EXPORT:
				return ISO_ptr<EXPORT_DIRECTORY>(id, this, mem);

			case pe::DATA_DIRECTORY::IMPORT: {
				ISO_ptr<anything>	e(id);
				pe::IMPORT_DIRECTORY	*imp_dir	= mem;
				for (pe::IMPORT_DIRECTORY::DESCRIPTOR *d = imp_dir->desc; d->Characteristics; ++d) {
					ISO_ptr<anything>	p((const char*)GetDataRVA(d->DllName));
					e->Append(p);
					if (_64bit) {
						for (pe::IMPORT_DIRECTORY::THUNK_DATA64 *ptr = GetDataRVA(d->FirstThunk); ptr->exists(); ++ptr)
							p->Append(ReadThunk(this, ptr));
					} else {
						for (pe::IMPORT_DIRECTORY::THUNK_DATA32 *ptr = GetDataRVA(d->FirstThunk); ptr->exists(); ++ptr)
							p->Append(ReadThunk(this, ptr));
					}
				}
				return e;
			}

			case pe::DATA_DIRECTORY::RESOURCE:
				return ReadResources(id, mem, 0, dir.VirtualAddress, pe::IRT_NONE);
				/*
				if (sect.VirtualSize + 256 < sect.SizeOfRawData) {
					uint32	offset = align(sect.PointerToRawData + sect.VirtualSize, 256);
					file.seek(offset);
					t->Append(ReadRaw("extra", file, sect.PointerToRawData + sect.SizeOfRawData - offset));
				}
				*/

			case pe::DATA_DIRECTORY::EXCEPTION:
				return ISO::MakePtr(id, make_range<pe::EXCEPTION_DIRECTORY::ENTRY>(mem));
//			case pe::OPTIONAL_HEADER::SECURITY:
//			case pe::OPTIONAL_HEADER::BASERELOC:

			case pe::DATA_DIRECTORY::DEBUG_DIR: {
				pe::DEBUG_DIRECTORY	*d	= mem;
				int					n	= mem.size32() / sizeof(*d);
				ISO_ptr<anything>	a(id);
				while (n--) {
					a->Append(ISO::MakePtr(0, *d));
					if (void *r = GetDataRaw(d->PointerToRawData)) {
						//void *r = GetDataRVA(d->AddressOfRawData);
						ISO_ptr<ISO_openarray<uint8> >	b(0, d->SizeOfData);
						memcpy(*b, r, d->SizeOfData);
						a->Append(b);
					}
					++d;
				}
				return a;
			}

//			case pe::OPTIONAL_HEADER::COPYRIGHT://(or ARCHITECTURE)
//			case pe::OPTIONAL_HEADER::GLOBALPTR:

			case pe::DATA_DIRECTORY::TLS:
				if (_64bit)
					return ISO_ptr<pe::TLS_DIRECTORY64>(id, *mem);
				return ISO_ptr<pe::TLS_DIRECTORY32>(id, *mem);

			case pe::DATA_DIRECTORY::LOAD_CONFIG:
				if (_64bit)
					return ISO_ptr<pe::LOAD_CONFIG_DIRECTORY64>(id, *mem);
				return ISO_ptr<pe::LOAD_CONFIG_DIRECTORY32>(id, *mem);

			case pe::DATA_DIRECTORY::BOUND_IMPORT: {
				ISO_ptr<anything>	e(id);
				pe::BOUND_IMPORT_DIRECTORY	*imp = mem;
				for (auto &i : make_next_range<const pe::BOUND_IMPORT_DIRECTORY::DESCRIPTOR>(mem)) {
					if (!i.ModuleName.offset)
						break;
					ISO_ptr<anything>	p(i.ModuleName.get(imp));
					auto	*r = i.Refs;
					for (int n = i.NumberOfModuleForwarderRefs; n--; r++)
						p->Append(ISO_ptr<void>(r->ModuleName.get(imp)));
				}
				return e;
			}
/*
			case pe::OPTIONAL_HEADER::IAT: {
				ISO_ptr<anything>	e(id);
				for (auto &i : make_range<pe::IMPORT_DIRECTORY::THUNK_DATA64>(mem)) {
					if (i.exists())
						e->Append(ReadThunk(this, &i));
				}
				return e;
			}
*/
			case pe::DATA_DIRECTORY::DELAY_IMPORT: {
				ISO_ptr<anything>	e(id);
				for (auto &i : make_range<pe::DELAY_IMPORT_DIRECTORY::DESCRIPTOR>(mem)) {
					if (i.AllAttributes) {
						ISO_ptr<anything>	p((char*)GetDataRVA(i.DllName));
						e->Append(p);
						uint64			*iat	= GetDataRVA(i.ImportAddressTable);
						pe::IMPORT_DIRECTORY::THUNK_DATA64	*names	= GetDataRVA(i.ImportNameTable);
						uint64			*bound	= GetDataRVA(i.BoundImportAddressTable);
						while (*iat) {
							p->Append(ReadThunk(this, names, *iat));
//							pe::IMPORT_DIRECTORY::BY_NAME	*byname = GetDataRVA(*names);
//							p->Append(ISO_ptr<xint64>(byname ? (char*)byname->name : 0, *iat));
							++iat;
							++names;
							++bound;
						}
					}
				}
				return e;
			}

			case pe::DATA_DIRECTORY::CLR_DESCRIPTOR: {
				ISO_ptr<clr::CLR>	c(id);
				c->init(this, mem);
				return c;
			}

			default: {
				ISO_ptr<ISO_openarray<uint8> >	a(id, mem.size32());
				memcpy(*a, mem, mem.size32());
				return a;
			}
		}
	}
	return ISO_NULL;
}

ISO_ptr<void> PE::Read(tag id, istream_ref file) {
	ISO_ptr<anything>	t(id);

	sect_data.resize(sections.size());

	for (auto &sect : sections) {
		if (sect.SizeOfRawData) {
			ISO_ptr<StartBin>	p(sect.Name, xint64(sect.VirtualAddress), sect.SizeOfRawData);
			void				*data = p->b.begin();//Init(sect.VirtualAddress, sect.SizeOfRawData);
			file.seek(sect.PointerToRawData);
			file.readbuff(data, sect.SizeOfRawData);
			t->Append(p);
			sect_data[sections.index_of(sect)] = p;
		} else {
			t->Append(ISO_ptr<xint32>(sect.Name, sect.VirtualSize));
		}
	}

	if (num_symbols) {
		ISO_ptr<anything>	syms("symbols");
		t->Append(syms);
		file.seek(symbol_offset);
		for (int i = 0; i < num_symbols; i++) {
			pe::SYMBOL	symbol = file.get();
			ISO_ptr<xint32>	symbol2(symbol.Name.ShortName, symbol.Value);
			syms->Append(symbol2);
		}
	}

	if (coff::OPTIONAL_HEADER *opt = COFF::opt) {
		static const char *dir_names[] = {
			"EXPORT",
			"IMPORT",
			"RESOURCE",
			"EXCEPTION",
			"SECURITY",
			"BASERELOC",
			"DEBUG_DIR",
			"ARCHITECTURE",
			"GLOBALPTR",
			"TLS",
			"LOAD_CONFIG",
			"BOUND_IMPORT",
			"IAT",
			"DELAY_IMPORT",
			"CLR_DESCRIPTOR",
			"RESERVED",
		};

		if (opt->Magic == pe::OPTIONAL_HEADER::MAGIC_NT32) {
			pe::OPTIONAL_HEADER32	*opt32	= (pe::OPTIONAL_HEADER32*)opt;
			for (int i = 0; i < opt32->NumberOfRvaAndSizes; i++) {
				if (ISO_ptr<void> p = ReadDataDir(dir_names[i], opt32->DataDirectory[i], i, false))
					t->Append(p);
			}

		} else if (opt->Magic == pe::OPTIONAL_HEADER::MAGIC_NT64) {
			pe::OPTIONAL_HEADER64	*opt64	= (pe::OPTIONAL_HEADER64*)opt;
			for (int i = 0; i < opt64->NumberOfRvaAndSizes; i++) {
				if (ISO_ptr<void> p = ReadDataDir(dir_names[i], opt64->DataDirectory[i], i, true))
					t->Append(p);
			}
		}
	}

	return t;
}

//-----------------------------------------------------------------------------
//	ISODEFS
//-----------------------------------------------------------------------------

//template<clr::TABLETYPE I, typename P> auto get(param_element<clr::Indexed<I>&, P> &&a) {
//	return a.p->Lookup(a.t);
//}
template<clr::TABLETYPE I, typename P> auto get(param_element<clr::Indexed<I>&, P> &a) {
	return a.p->Lookup(a.t);
}
template<typename T, typename P, typename C, clr::TABLETYPE I> auto get_field(param_element<T, P> &a, clr::IndexedList<I> C::*f)	{
	return with_param(a.p->GetEntries(addr(get(a)), a.t.*f), a.p);
}
//template<typename T, typename P> auto get(param_element<clr::CodedIndex<T>&, P> &&a) {
//	return a.p->Lookup(a.t);
//}
template<typename T, typename P> auto get(param_element<clr::CodedIndex<T>&, P> &a) {
	return a.p->Lookup(a.t);
}
//template<typename P>	ISO::Browser2 get(param_element<clr::TypeSignature&, P> &&a) {
//	buffer_accum<1024>	ba;
//	if (auto &s = a.t)
//		a.p->DumpType(ba, unconst(byte_reader(s)));
//	return ISO_ptr<string>(0, ba.term());
//}
template<typename P>	ISO::Browser2 get(param_element<clr::TypeSignature&, P> &a) {
	buffer_accum<1024>	ba;
	if (auto &s = a.t)
		a.p->DumpType(ba, unconst(byte_reader(s)));
	return ISO_ptr<string>(0, ba.term());
}
//template<typename P> ISO::Browser2 get(param_element<clr::Signature&, P> &&a) {
//	buffer_accum<1024>	ba;
//	if (auto &s = a.t)
//		a.p->DumpSignature(ba, unconst(byte_reader(s)));
//	return ISO_ptr<string>(0, ba.term());
//}
template<typename P> ISO::Browser2 get(param_element<clr::Signature&, P> &a) {
	buffer_accum<1024>	ba;
	if (auto &s = a.t)
		a.p->DumpSignature(ba, unconst(byte_reader(s)));
	return ISO_ptr<string>(0, ba.term());
}
template<typename T, typename P, typename C> ISO::Browser2 get_field(param_element<T, P> &a, clr::CustomAttributeValue C::*f) {
	if (auto &data = get(a.t.*f)) {
		switch (a->type.type()) {
			case clr::MethodDef: {
				auto	*method = a.p->template GetEntry<clr::MethodDef>(a->type.index());
				return a.p->GetValues(unconst(byte_reader(method->signature)), unconst(byte_reader(data)), a.p->GetEntries(method, method->paramlist));
			}
			case clr::MemberRef: {
				auto	*member = a.p->template GetEntry<clr::MemberRef>(a->type.index());
				return a.p->GetValues(unconst(byte_reader(member->signature)), unconst(byte_reader(data)), empty);
			}
		}
	}
	return ISO::Browser2();
}

//template<typename T, int BITS> struct ISO::def<clr::compact<T, BITS> > : ISO::VirtualT2<clr::compact<T, BITS> > {
//	static ISO::Browser2	Deref(const clr::compact<T, BITS> &a)	{ return MakePtr(tag(), a.get()); }
//};

ISO_DEFVIRT(clr::CLR::_Tables);
ISO_DEFUSERCOMPXV(clr::CLR::_Heaps, "Heaps", String, GUID, Blob, UserString);
ISO_DEFUSERCOMPXV(clr::Code, "StartBin", start, data);

ISO_DEFUSERENUMX(clr::TABLETYPE, 38, "TABLETYPE") {
	using namespace clr;
	ISO_SETENUMS(0,
		Module, TypeRef, TypeDef, Field, MethodDef, Param, InterfaceImpl, MemberRef,
		Constant, CustomAttribute, FieldMarshal, DeclSecurity, ClassLayout, FieldLayout, StandAloneSig, EventMap,
		clr::Event, PropertyMap, Property, MethodSemantics, MethodImpl, ModuleRef, TypeSpec, ImplMap,
		FieldRVA, Assembly, AssemblyProcessor, AssemblyOS, AssemblyRef, AssemblyRefProcessor, AssemblyRefOS, File
	);
	ISO_SETENUMS(32, ExportedType, ManifestResource, NestedClass, GenericParam, MethodSpec, GenericParamConstraint);
}};

ISO_DEFUSERENUMX(clr::ELEMENT_TYPE, 41, "ELEMENTTYPE") {
	using namespace clr;
	ISO_SETENUMS(0,
		ELEMENT_TYPE_END,ELEMENT_TYPE_VOID,ELEMENT_TYPE_BOOLEAN,ELEMENT_TYPE_CHAR,ELEMENT_TYPE_I1,ELEMENT_TYPE_U1,ELEMENT_TYPE_I2,ELEMENT_TYPE_U2,
		ELEMENT_TYPE_I4,ELEMENT_TYPE_U4,ELEMENT_TYPE_I8,ELEMENT_TYPE_U8,ELEMENT_TYPE_R4,ELEMENT_TYPE_R8,ELEMENT_TYPE_STRING,ELEMENT_TYPE_PTR,
		ELEMENT_TYPE_BYREF,ELEMENT_TYPE_VALUETYPE,ELEMENT_TYPE_CLASS,ELEMENT_TYPE_VAR,ELEMENT_TYPE_ARRAY,ELEMENT_TYPE_GENERICINST,ELEMENT_TYPE_TYPEDBYREF,ELEMENT_TYPE_I,
		ELEMENT_TYPE_U,ELEMENT_TYPE_FNPTR,ELEMENT_TYPE_OBJECT,ELEMENT_TYPE_SZARRAY,ELEMENT_TYPE_MVAR,ELEMENT_TYPE_CMOD_REQD,ELEMENT_TYPE_CMOD_OPT,ELEMENT_TYPE_INTERNAL
	);
	ISO_SETENUMS(32,
		ELEMENT_TYPE_MODIFIER,ELEMENT_TYPE_SENTINEL,ELEMENT_TYPE_PINNED,ELEMENT_TYPE_SYSTEMTYPE,ELEMENT_TYPE_BOXED,ELEMENT_TYPE_,ELEMENT_TYPE_FIELD,ELEMENT_TYPE_PROPERTY,
		ELEMENT_TYPE_ENUM
	);
}};

ISO_DEFUSERX(clr::Blob, const_memory_block, "Blob");
ISO_DEFUSERX(clr::String, const char*, "String");

ISO_DEFUSERENUMXQV(clr::ENTRY<clr::TypeDef>::Flags, "Flags",
	NotPublic,Public,NestedPublic,NestedPrivate,NestedFamily,NestedAssem,NestedFamANDAssem,NestedFamORAssem,
	AutoLayout,SequentialLayout,ExplicitLayout,Interface,Abstract,Sealed,SpecialName,Import,
	Serializable,AnsiClass,UnicodeClass,AutoClass,CustomFormatClass,CustomStringFormat,BeforeFieldInit,RTSpecialName,
	HasSecurity, IsTypeForwarder, WinRT
);

ISO_DEFUSERENUMXQV(clr::ENTRY<clr::Field>::Flags, "Flags",
	FieldAccessMask,Static,InitOnly,Literal,NotSerialized,HasFieldRVA,SpecialName,RTSpecialName,
	HasFieldMarshal,PInvokeImpl,HasDefault
);

ISO_DEFUSERENUMXQV(clr::ENTRY<clr::MethodDef>::ImplFlags, "ImplFlags",
	IL,Native,OPTIL,Runtime,Unmanaged,NoInlining,ForwardRef,Synchronized,
	NoOptimization,PreserveSig,InternalCall
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::MethodDef>::Flags, "Flags",
	MemberAccessMask,UnmanagedExport,Static,Final,Virtual,HideBySig,NewSlot,Strict,
	Abstract,SpecialName,RTSpecialName,PInvokeImpl,HasSecurity,RequireSecObject,
	ACCESS_CompilerControlled,ACCESS_Private,ACCESS_FamANDAssem,ACCESS_Assem,ACCESS_Family,ACCESS_FamORAssem,ACCESS_Public
);

ISO_DEFUSERENUMXQV(clr::ENTRY<clr::Param>::Flags, "Flags",
	In,Out,Optional,HasDefault,HasFieldMarshal
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::Event>::Flags, "Flags",
	SpecialName,RTSpecialName
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::Property>::Flags, "Flags",
	SpecialName,RTSpecialName,HasDefault
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::MethodSemantics>::Flags, "Flags",
	Setter,Getter,Other,AddOn,RemoveOn,Fire
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::ImplMap>::Flags, "Flags",
	NoMangle,CharSetNotSpec,CharSetAnsi,CharSetUnicode,CharSetAuto,SupportsLastError,
	CallConvPlatformapi,CallConvCdecl,CallConvStdcall,CallConvThiscall,CallConvFastcall
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::Assembly>::HashAlgorithm, "HashAlgorithm",
	None, Reserved_MD5, SHA1
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::Assembly>::Flags, "Flags",
	PublicKey,Retargetable,WinRT,DisableJITcompileOptimizer,EnableJITcompileTracking
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::ManifestResource>::Flags, "Flags",
	VisibilityMask,Public,Private
);
ISO_DEFUSERENUMXQV(clr::ENTRY<clr::GenericParam>::Flags, "Flags",
	None,Covariant,Contravariant,ReferenceTypeConstraint,NotNullableValueTypeConstraint,DefaultConstructorConstraint
);

#define ISO_SETCOMPSIZE	fields[comp.Count() - 1].size = sizeof(_T)

ISO_DEFUSERCOMPPX(clr::ENTRY<clr::Module>,					clr::ISO_METADATA*,	5, "Module")					{ ISO_SETFIELDS(0, generation, name, mvid, encid, encbaseid);												ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::TypeRef>,					clr::ISO_METADATA*,	3, "TypeRef")					{ ISO_SETFIELDS(0, name, namespce, scope);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::TypeDef>,					clr::ISO_METADATA*,	6, "TypeDef")					{ ISO_SETFIELDS(0, flags, name, namespce, extends, fields, methods);										ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::Field>,					clr::ISO_METADATA*,	3, "Field")						{ ISO_SETFIELDS(0, name, flags, signature);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::MethodDef>,				clr::ISO_METADATA*,	6, "MethodDef")					{ ISO_SETFIELDS(0, name, code, implflags, flags, signature, paramlist);										ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::Param>,					clr::ISO_METADATA*,	3, "Param")						{ ISO_SETFIELDS(0, name, flags, sequence);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::InterfaceImpl>,			clr::ISO_METADATA*,	2, "InterfaceImpl")				{ ISO_SETFIELDS(0,clss, interfce);																			ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::MemberRef>,				clr::ISO_METADATA*,	3, "MemberRef")					{ ISO_SETFIELDS(0, name, clss, signature);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::Constant>,				clr::ISO_METADATA*,	3, "Constant")					{ ISO_SETFIELDS(0, type, parent, value);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::CustomAttribute>,			clr::ISO_METADATA*,	3, "CustomAttribute")			{ ISO_SETFIELDS(0, parent, type, value);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::FieldMarshal>,			clr::ISO_METADATA*,	2, "FieldMarshal")				{ ISO_SETFIELDS(0, parent, native_type);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::DeclSecurity>,			clr::ISO_METADATA*,	3, "DeclSecurity")				{ ISO_SETFIELDS(0, action, parent, permission_set);															ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::ClassLayout>,				clr::ISO_METADATA*,	3, "ClassLayout")				{ ISO_SETFIELDS(0, packing_size, class_size, parent);														ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::FieldLayout>,				clr::ISO_METADATA*,	2, "FieldLayout")				{ ISO_SETFIELDS(0, offset, field);																			ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::StandAloneSig>,			clr::ISO_METADATA*,	1, "StandAloneSig")				{ ISO_SETFIELDS(0, signature);																				ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::EventMap>,				clr::ISO_METADATA*,	2, "EventMap")					{ ISO_SETFIELDS(0, parent, event_list);																		ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::Event>,					clr::ISO_METADATA*,	3, "Event")						{ ISO_SETFIELDS(0, flags, name, event_type);																ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::PropertyMap>,				clr::ISO_METADATA*,	2, "PropertyMap")				{ ISO_SETFIELDS(0, parent, property_list);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::Property>,				clr::ISO_METADATA*,	3, "Property")					{ ISO_SETFIELDS(0, flags, name, type);																		ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::MethodSemantics>,			clr::ISO_METADATA*,	3, "MethodSemantics")			{ ISO_SETFIELDS(0, flags, method, association);																ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::MethodImpl>,				clr::ISO_METADATA*,	3, "MethodImpl")				{ ISO_SETFIELDS(0, clss, method_body, method_declaration);													ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::ModuleRef>,				clr::ISO_METADATA*,	1, "ModuleRef")					{ ISO_SETFIELDS(0, name);																					ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::TypeSpec>,				clr::ISO_METADATA*,	1, "TypeSpec")					{ ISO_SETFIELDS(0, signature);																				ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::ImplMap>,					clr::ISO_METADATA*,	4, "ImplMap")					{ ISO_SETFIELDS(0, flags, member_forwarded, name, scope);													ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::FieldRVA>,				clr::ISO_METADATA*,	2, "FieldRVA")					{ ISO_SETFIELDS(0, rva, field);																				ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::Assembly>,				clr::ISO_METADATA*,	9, "Assembly")					{ ISO_SETFIELDS(0, hashalg, major, minor, build, rev, flags, publickey, name); ISO_SETFIELD(8, culture);	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::AssemblyProcessor>,		clr::ISO_METADATA*,	1, "AssemblyProcessor")			{ ISO_SETFIELDS(0, processor);																				ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::AssemblyOS>,				clr::ISO_METADATA*,	3, "AssemblyOS")				{ ISO_SETFIELDS(0, platform, major, minor);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::AssemblyRef>,				clr::ISO_METADATA*,	9, "AssemblyRef")				{ ISO_SETFIELDS(0, major, minor, build, rev, flags, publickey, name, culture); ISO_SETFIELD(8, hashvalue);	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::AssemblyRefProcessor>,	clr::ISO_METADATA*,	2, "AssemblyRefProcessor")		{ ISO_SETFIELDS(0, processor, assembly);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::AssemblyRefOS>,			clr::ISO_METADATA*,	4, "AssemblyRefOS")				{ ISO_SETFIELDS(0, platform, major, minor, assembly);														ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::File>,					clr::ISO_METADATA*,	3, "File")						{ ISO_SETFIELDS(0, flags, name, hash);																		ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::ExportedType>,			clr::ISO_METADATA*,	5, "ExportedType")				{ ISO_SETFIELDS(0, flags, typedef_id, name, namespce, implementation);										ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::ManifestResource>,		clr::ISO_METADATA*,	4, "ManifestResource")			{ ISO_SETFIELDS(0, offset, flags, name, implementation);													ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::NestedClass>,				clr::ISO_METADATA*,	2, "NestedClass")				{ ISO_SETFIELDS(0, nested_class, enclosing_class);															ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::GenericParam>,			clr::ISO_METADATA*,	4, "GenericParam")				{ ISO_SETFIELDS(0, number, flags, owner, name);																ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::MethodSpec>,				clr::ISO_METADATA*,	2, "MethodSpec")				{ ISO_SETFIELDS(0, method, instantiation);																	ISO_SETCOMPSIZE; }};
ISO_DEFUSERCOMPPX(clr::ENTRY<clr::GenericParamConstraint>,	clr::ISO_METADATA*,	2, "GenericParamConstraint")	{ ISO_SETFIELDS(0, owner, constraint);																		ISO_SETCOMPSIZE; }};

ISO_DEFUSERCOMPXV(clr::CLR, "CLR", MajorRuntimeVersion,MinorRuntimeVersion,Flags,Heaps,Tables, EntryPoint,Resources,StrongNameSignature,CodeManagerTable,VTableFixups,ExportAddressTableJumps,ManagedNativeHeader);

//-----------------------------------------------------------------------------
//	FileHandlers
//-----------------------------------------------------------------------------

struct Debug_S : ISO::VirtualDefaults {
	malloc_block	data;
	ISO::Browser2	Deref() const { return ISO::MakeBrowser(make_next_range<const CV::SubSection>(data + 4)); }
	Debug_S(malloc_block &&data) : data(move(data))	{}
};

struct Debug_T : ISO::VirtualDefaults {
	malloc_block	data;
	ISO::Browser2	Deref() const { return ISO::MakeBrowser(make_next_range<const CV::TYPTYPE>(data + 4)); }
	Debug_T(malloc_block &&data) : data(move(data))	{}
};

ISO_DEFUSERVIRT(Debug_S);
ISO_DEFUSERVIRT(Debug_T);

class COFFFileHandler : FileHandler {
	const char*		GetExt() override { return "obj"; }
	const char*		GetDescription() override { return "COFF Object File"; }

	struct Symbols : dynamic_array<coff::SYMBOL> {
		STRINGTABLE	strings;
		void Add(const char *name, int sect, uint64 val, pe::SYM_TYPE type, pe::SYM_CLASS cls, int naux = 0) {
			auto	&s = push_back();

			iso::clear(s);
			if (strlen(name) < 8)
				strcpy(s.Name.ShortName, name);
			else
				s.Name.Long = strings.add(name) + 4;

			s.SectionNumber			= sect;
			s.Value					= uint32(val);
			s.StorageClass			= cls;
			s.Type					= type;
			s.NumberOfAuxSymbols	= naux;
		}
		template<typename A> A *Add(const char *name, int sect, uint64 val, pe::SYM_TYPE type, pe::SYM_CLASS cls) {
			int	naux = sizeof(A) / sizeof(pe::SYMBOL);
			Add(name, sect, val, type, cls, naux);
			return (A*)expand(naux);
		}
		Symbols() { reserve(256); }	// don't want reallocations!
	};

	static void	MakeSections(const dynamic_array<coff::SECTION_HEADER> &sections, istream_ref file, anything *t) {
		for (auto &sect : sections) {
			char	name[9];
			name[8] = 0;
			memcpy(name, sect.Name, sizeof(sect.Name));
			file.seek(sect.PointerToRawData);

			if (name == cstr(".debug$S")) {
				t->Append(ISO_ptr<Debug_S>(name, malloc_block(file, sect.SizeOfRawData)));

			} else if (name == cstr(".debug$T")) {
				t->Append(ISO_ptr<Debug_T>(name, malloc_block(file, sect.SizeOfRawData)));

			} else {
				ISO_ptr<ISO_openarray<xint8> > p(name);
				file.readbuff(p->Create(sect.SizeOfRawData), sect.SizeOfRawData);
				t->Append(p);
			}
		}
	}

	template<typename T> static ISO_ptr<anything> MakeSymbols(istream_ref file, uint32 offset, uint32 num, bool demangle) {
		if (num == 0)
			return ISO_NULL;

		ISO_ptr<anything> p("symbols");

		file.seek(offset + num * sizeof(T));
		uint32	size;
		if (file.read(size)) {
			malloc_block	strings(file, size);

			file.seek(offset);
			for (int i = 0, n = num; i < n; i++) {
				char		name[9], *name2 = 0;
				T			e = file.get();
				if (e.Name) {
					if (e.Name.is_short()) {
						name[8] = 0;
						name2	= name;
						memcpy(name, e.Name.ShortName, sizeof(e.Name.ShortName));
					} else {
						name2	= strings + (int(e.Name.Long) - 4);
					}
				}
				p->Append(ISO_ptr<int>(name2 && demangle ? (char*)::demangle(name2) : name2, e.SectionNumber));
				i += e.NumberOfAuxSymbols;
			}
		}
		return p;
	}

	static ISO_ptr<void> MakeSymbolsLIB(istream_ref file, const_memory_block data, bool demangle) {
		struct SYM {
			uint16	index;
			uint16	type;
			embedded_string	name;
		};
		const SYM *sym	= data;
		return ISO_ptr<int>(sym->name && demangle ? (char*)::demangle(sym->name) : sym->name, sym->index);
	}

	static bool Write(COFF &&coff, ISO::Browser b, ostream_ref file, FileHandler *fh, const char *name, const char *prefix) {
		const char	*section	= ISO::root("variables")["section"].GetString(".data");
		bool		putsize		= !!ISO::root("variables")["data_size"].GetInt(0);
		bool		putend		= !!ISO::root("variables")["data_end"].GetInt(1);
		size_t		bin_size	= fh && str(fh->GetExt()) != "ib" && str(fh->GetExt()) != "ibz" ? ISO::binary_data.Size() : 0;

		uint32		total		= 0;
		int			num_sects	= bin_size ? 2 : 1;
		uint32		offset		= sizeof(coff::FILE_HEADER) + sizeof(coff::SECTION_HEADER) * num_sects;

		Symbols		symbols;
		auto		&sect		= coff.AddSection(section);
		auto		aux			= symbols.Add<coff::AUXSYMBOL_SECT>(sect.Name, 1, 0, coff::SYM_TYPE_NULL, coff::SYM_CLASS_STATIC);

		file.seek(offset);

		for (int i = 0, count = b.Count(); i < count; i++) {
			ISO_ptr<void>	p		= *b[i];
			const char		*label	= p.ID().get_tag();
			uint32			start	= total;

			symbols.Add(format_string("%s%s", prefix, label), 1, start, coff::SYM_TYPE_NULL, coff::SYM_CLASS_EXTERNAL);
			fh->Write(p, ostream_offset(file).me());
			file.seek_end(0);
			total = file.tell32() - offset;

			if (putend)
				symbols.Add(format_string("%s%s_end", prefix, label), 1, total, coff::SYM_TYPE_NULL, coff::SYM_CLASS_EXTERNAL);
			if (putsize)
				symbols.Add(format_string("%s%s_size", prefix, label), 1, total - start, coff::SYM_TYPE_LONG, coff::SYM_CLASS_EXTERNAL);
		}

		sect.PointerToRawData	= offset;
		sect.SizeOfRawData		= total;

		clear(*aux);
		aux->Length = total;

		if (bin_size) {
			auto	&sect = coff.AddSection(".bin");
			sect.PointerToRawData	= file.tell32();
			sect.SizeOfRawData		= (uint32)bin_size;

			auto		aux = symbols.Add<coff::AUXSYMBOL_SECT>(format_string("%s_binary_bin", name), 2, 0, coff::SYM_TYPE_NULL, coff::SYM_CLASS_STATIC);
			clear(*aux);
			aux->Length = (uint32)bin_size;
			ISO::binary_data.Write(file);

			if (putend)
				symbols.Add(format_string("%s_binary_end", name), 2, bin_size, coff::SYM_TYPE_NULL, coff::SYM_CLASS_EXTERNAL);
			if (putsize)
				symbols.Add(format_string("%s_binary_size", name), 2, bin_size, coff::SYM_TYPE_LONG, coff::SYM_CLASS_EXTERNAL);
		}

		coff.SetSymbolTable(file.tell32(), symbols.size());

		for (auto &i : symbols)
			file.write(i);

		file.write(uint32(symbols.strings.size() + 4));
		file.write(symbols.strings);

		return coff.WriteHeader(file);
	}

	static bool	CheckMachine(uint16 m) {
		switch (m) {
			case coff::MACHINE_I386:
			case coff::MACHINE_X360:
			case coff::MACHINE_AMD64:
			case coff::MACHINE_ARM:
			case coff::MACHINE_THUMB:
			case coff::MACHINE_ARM64:
			case coff::MACHINE_ARMNT:
				return true;
		}
		return false;
	}

	int				Check(istream_ref file) override {
		file.seek(0);
		return CheckMachine(COFF(file).machine) ? FileHandler::CHECK_PROBABLE : FileHandler::CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		COFF	coff(file);
		if (!CheckMachine(coff.machine))
			return ISO_NULL;
		ISO_ptr<anything>	t(id);
		MakeSections(coff.sections, file, t);


		if (coff.num_symbols) {
			bool	demangle	= !ISO::root("variables")["nodemangle"].GetInt();
			if (coff.big)
				t->Append(MakeSymbols<coff::SYMBOL_BIGOBJ>(file, coff.symbol_offset, coff.num_symbols, demangle));
			else if (!coff.sections)
				t->Append(MakeSymbolsLIB(file, coff.opt, demangle));
			else
				t->Append(MakeSymbols<coff::SYMBOL>(file, coff.symbol_offset, coff.num_symbols, demangle));
		}

#if 0
		if (coff.IsAnon()) {
			coff::IMPORT_HEADER	Import;
			malloc_block	m(file, SizeOfData);

			ISO_ptr<anything>	p(id);
			p->Append(ISO_ptr<int>("ordinal", Import.Ordinal));
			p->Append(ISO_ptr<int>("flags", Import.flags));
			p->Append(ISO_ptr<string>("symbol", (char*)m));
			p->Append(ISO_ptr<string>("dll", (char*)m + strlen((char*)m) + 1));
			return p;
		}
#endif
		return t;
	}
	static coff::MACHINE RightPlatform() {
		const char *exportfor = ISO::root("variables")["exportfor"].GetString();
		return str(exportfor) == "pc" ? coff::MACHINE_I386
			: str(exportfor) == "x360" ? coff::MACHINE_X360
			: str(exportfor) == "pc64" ? coff::MACHINE_AMD64
			: str(exportfor) == "dx11" ? coff::MACHINE_AMD64
			: str(exportfor) == "dx12" ? coff::MACHINE_AMD64
			: coff::MACHINE_UNKNOWN;
	}
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		if (coff::MACHINE machine = RightPlatform()) {
			filename::ext_t	ext = filename(fn.name()).ext();
			FileHandler		*fh	= ext.blank() ? NULL : FileHandler::Get(ext);
			if (!fh)
				fh = FileHandler::Get("bin");

			return Write(COFF(machine), GetItems(p), FileOutput(fn).me(), fh, filename(fn).name(), machine == coff::MACHINE_AMD64 ? "" : "_");
		}
		return false;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (coff::MACHINE machine = RightPlatform())
			return Write(COFF(machine), GetItems(p), file, FileHandler::Get("bin"), 0, machine == coff::MACHINE_AMD64 ? "" : "_");
		return false;
	}

} coffobj;

//-----------------------------------------------------------------------------
//	EXE
//-----------------------------------------------------------------------------

class EXE {
	pe::EXE_HEADER	hdr;
	ISO_ptr<void>	ReadDOS(tag id, istream_ref file);
public:
	bool			Check()	const {
		return hdr.magic == pe::DOS_HEADER::MAGIC;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) {
		if (!Check())
			return ISO_NULL;
		if (hdr.lfanew < file.length()) {
			file.seek(hdr.lfanew);
			if (file.get<uint32be>() == ('PE' << 16)) {
				PE	pe(file);
				return pe.Read(id, file);
			}
		}
		return ReadDOS(id, file);
	}
	EXE(istream_ref file) {
		file.read(hdr);
	}
};

ISO_ptr<void> EXE::ReadDOS(tag id, istream_ref file) {
	ISO_ptr<anything>	p(id);

	ISO_ptr<xint16[sizeof(pe::DOS_HEADER)/sizeof(uint16)]>	h("header");
	*(pe::DOS_HEADER*)*h = hdr;
	p->Append(h);

	ISO_ptr<ISO_openarray<uint16[2]> >	relocs("relocs", hdr.crlc);
	file.seek(hdr.lfarlc);
	readn(file, relocs->begin(), hdr.crlc);
	p->Append(relocs);

	uint32	start	= hdr.cparhdr * hdr.PARAGRAPH;
	uint32	end		= (hdr.cp - int(hdr.cblp != 0)) * hdr.PAGE + hdr.cblp;
	ISO_ptr<ISO_openarray<uint8> >	code("code", end - start);
	file.seek(start);
	file.readbuff(*code, end - start);
	p->Append(code);

	return p;
}

class EXEFileHandler : FileHandler {
	const char*		GetExt() override { return "exe"; }
	const char*		GetDescription() override { return "Microsoft Executable";	}
	int				Check(istream_ref file) override { file.seek(0); return EXE(file).Check() ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return EXE(file).Read(id, file); }
} exe;

class DLLFileHandler : FileHandler {
	const char*		GetExt() override { return "dll"; }
	const char*		GetDescription() override { return "Microsoft Dynamic Library";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return EXE(file).Read(id, file); }
} dll;

class WINMDFileHandler : FileHandler {
	const char*		GetExt() override { return "winmd"; }
	const char*		GetDescription() override { return "Microsoft Metadata";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return EXE(file).Read(id, file); }
} winmd;

//-----------------------------------------------------------------------------
//	Compiled Resources
//-----------------------------------------------------------------------------

struct RES_HEADER {
	uint32	DataSize;
	uint32	HeaderSize;
};

struct RESOURCEHEADER {
	enum MEMORY_FLAGS {
		MOVEABLE	= 0x0010,	//	FIXED		= ~MOVEABLE
		PURE		= 0x0020,	//	IMPURE		= ~PURE
		PRELOAD		= 0x0040,	//	LOADONCALL	= ~PRELOAD
		DISCARDABLE = 0x1000,
	};
	uint32	DataSize;
	uint32	HeaderSize;
	uint32	TYPE;
	uint32	NAME;
	uint32	DataVersion;
	uint16	MemoryFlags;
	uint16	LanguageId;
	uint32	Version;
	uint32	Characteristics;
};

struct RES_ICONDIR {
	struct ENTRY : packed_types<littleendian_types> {
		union {
			struct {
				uint8 Width;
				uint8 Height;
				uint8 ColorCount;
				uint8 reserved;
			} Icon;
			struct {
				uint16 Width;
				uint16 Height;
			} Cursor;
		};
		uint16	Planes;
		uint16	BitCount;
		uint32	BytesInRes;
		uint16	IconCursorId;
	};

	uint16	Reserved;
	uint16	ResType;
	uint16	ResCount;
	range<const ENTRY*>	Dir() const { return make_range_n((const ENTRY*)(this + 1), ResCount); }
};

struct RES_TOOLBAR {
	uint16	version, width, height, count;
	range<const uint16*>	IDS() const { return make_range_n((const uint16*)(this + 1), count); }
};

struct RES_MENU {
	struct ENTRY {
		enum {POPUP = 0x10, END = 0x80};
		uint16	flags;
		uint16	id;
		const char16 *_text()	const	{ return (const char16*)this + (flags & POPUP ? 1 : 2); }
		const char16 *text()	const	{ return _text(); }
		const ENTRY *next()		const	{ return (const ENTRY*)(string_end(_text()) + 1); }
	};
	uint32	unknown;
	range<next_iterator<const ENTRY> >	entries() const { return make_next_range<const ENTRY>(const_memory_block(this + 1, unknown - 4)); }
};

#pragma push_macro("ISO_ENUM")
#undef ISO_ENUM
#define ISO_ENUM(x) , #x, pe::IRT_##x

ISO_DEFUSERENUMXV(pe::RESOURCE_TYPE, "RESOURCE_TYPE",
	NONE, CURSOR, BITMAP, ICON, MENU, DIALOG, STRING, FONTDIR,
	FONT, ACCELERATOR, RCDATA, MESSAGETABLE, GROUP_CURSOR, GROUP_ICON, VERSION, DLGINCLUDE,
	PLUGPLAY, VXD, ANICURSOR, ANIICON, HTML, MANIFEST, TOOLBAR
);
#pragma pop_macro("ISO_ENUM")

ISO_DEFUSERCOMPV(RES_ICONDIR::ENTRY, Planes, BitCount, BytesInRes, IconCursorId);
ISO_DEFUSERCOMPV(RES_ICONDIR, Reserved, ResType, Dir);
ISO_DEFUSERCOMPV(RES_TOOLBAR, version, width, height, IDS);
ISO_DEFUSERCOMPV(RES_MENU::ENTRY, text, id);
ISO_DEFUSERCOMPV(RES_MENU, entries);

class RESFileHandler : FileHandler {
	const char*		GetExt() override { return "res"; }
	const char*		GetDescription() override { return "Compiled Resources"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	res(id);
		RES_HEADER	rh;
		while (file.read(rh)) {
			malloc_block	header(file, rh.HeaderSize - sizeof(rh));
			auto	here = file.tell();

			uint16	*p = header;
			int		type = -1;
			string	custom_type;

			if (*p == 0xffff) {
				type = p[1];
				p += 2;
			} else {
				string16	custom((char16*)p);
				custom_type = custom;
				p += custom.length() + 1;
			}

			tag		name;
			if (*p == 0xffff) {
				uint16	n = p[1];
				if (type == pe::IRT_STRING)
					n *= 16;
				name = to_string(n);
				p += 2;
			} else {
				string16	custom((char16*)p);
				p += custom.length() + 1;
				name = custom;
			}

			ISO_ptr<void>	r;
			switch (type) {
				//				case pe::IRT_NONE:
				case pe::IRT_CURSOR:
				{
					int16	hotx = file.get<int16>();
					int16	hoty = file.get<int16>();
					r = ReadBmp(name, malloc_block(file, rh.DataSize), true);
					break;
				}
				case pe::IRT_BITMAP:
					r = ReadBmp(name, malloc_block(file, rh.DataSize), false);
					break;

				case pe::IRT_ICON:
					r = ReadBmp(name, malloc_block(file, rh.DataSize), true);
					break;

				case pe::IRT_MENU:
					r = ISO::MakePtrSize<32,RES_MENU>(name, rh.DataSize);
					file.readbuff(r, rh.DataSize);
					((RES_MENU*)r)->unknown = rh.DataSize;
					break;

//				case pe::IRT_DIALOG:
				case pe::IRT_STRING: {
					ISO_ptr<ISO_openarray<string16> >	s(name);
					malloc_block	data(file, rh.DataSize);
					uint16			*p = data;
					while (p < data.end()) {
						uint16		len	= *p++;
						string16	s2(len);
						memcpy(s2, p, len * 2);
						p += len;
						s->Append(s2);
					}
					r	= s;
					break;
				}
//				case pe::IRT_FONTDIR:
//				case pe::IRT_FONT:
//				case pe::IRT_ACCELERATOR:
//				case pe::IRT_RCDATA:
//				case pe::IRT_MESSAGETABLE:

				case pe::IRT_GROUP_CURSOR:
				case pe::IRT_GROUP_ICON:
					r = ISO::MakePtrSize<32,RES_ICONDIR>(name, rh.DataSize);
					file.readbuff(r, rh.DataSize);
					break;

//				case pe::IRT_VERSION:
//				case pe::IRT_DLGINCLUDE:
//				case pe::IRT_PLUGPLAY:
//				case pe::IRT_VXD:
//				case pe::IRT_ANICURSOR:
//				case pe::IRT_ANIICON:
//				case pe::IRT_HTML:
//				case pe::IRT_MANIFEST:
				case pe::IRT_TOOLBAR:
					r = ISO::MakePtrSize<32,RES_TOOLBAR>(name, rh.DataSize);
					file.readbuff(r, rh.DataSize);
					break;

				default: {
					ISO_ptr<pair<pe::RESOURCE_TYPE, ISO_openarray<uint8> > > data(name, make_pair(pe::RESOURCE_TYPE(type), rh.DataSize));
					file.readbuff(data->b, rh.DataSize);
					r	= data;
					break;
				}
				case -1: {
					ISO_ptr<pair<string, ISO_openarray<uint8> > > data(name, make_pair(custom_type, rh.DataSize));
					file.readbuff(data->b, rh.DataSize);
					r	= data;
					break;
				}
			}
			res->Append(r);

			file.seek(align(here + rh.DataSize, 4));
		}
		return res;
	}
} res;

//-----------------------------------------------------------------------------
//	Package Resource Index
//-----------------------------------------------------------------------------

struct PRI_header {
	char	magic[8];	// "mrm_pri2"
	uint32	version;	//?
	uint32	file_size;
	uint32	header_size;	//32
	uint32	data_start;
	uint16	root_entries;	//0x0023
	uint16	y;				//0xffff
	uint32	z;				//0
};

struct PRI_entry {
	char	name[16];
	uint32	a;	//0
	uint32	b;	//0
	uint32	offset;		// from data
	uint32	size;		// including this+tail
};

struct PRI_entry_tail {
	uint32	magic;	//DEF5FADE
	uint32	size;	//repeat of size
};

struct PRI_dataitem : PRI_entry {
	struct sub16 {
		uint16	offset;	// offset from data
		uint16	len;	// length of data (inc. terminating zero)
	};
	struct sub32 {
		uint32	offset;	// offset from data
		uint32	len;	// length of data (inc. terminating zero)
	};

	uint32	unknown;	//0
	uint16	count16;	//#16-bit subs
	uint16	count32;	//#32-bit subs
	uint32	data_size;	//size padded to multiple of 8

	sub16	sub16s[];	// x count16
//	sub32	sub32s[];	// x count32
//	char	data[];//len

	range<sub16*>	get_subs16()	{ return make_range_n(sub16s, count16); }
	range<sub32*>	get_subs32()	{ return make_range_n((sub32*)(sub16s + count16), count32); }
	memory_block	data()			{ return memory_block(get_subs32().end(), data_size); }
};

struct PRI_decn_info : PRI_entry {
	uint16	x[];
};
struct PRI_pridescex : PRI_entry {
	uint16	x[];
};


class PRIFileHandler : FileHandler {
	const char*		GetExt() override { return "pri"; }
	const char*		GetDescription() override { return "Package Resource Index"; }
	int				Check(istream_ref file) override {
		PRI_header	header;
		file.seek(0);
		return file.read(header) && header.magic == cstr("mrm_pri2") ? FileHandler::CHECK_PROBABLE : FileHandler::CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		PRI_header	header;
		file.read(header);
		if (header.magic != cstr("mrm_pri2"))
			return ISO_NULL;

		dynamic_array<PRI_entry>	entries;
		entries.read(file, header.root_entries);

		ISO_ptr<anything>	a(id);

		for (auto &i : entries) {
			file.seek(i.offset + header.data_start);
			malloc_block	mem(file, i.size);
			tag2			id	= (char*)mem;
			if (id == "[mrm_dataitem] ") {
				PRI_dataitem		*di		= mem;
				auto				data	= di->data();
				ISO_ptr<anything>	a2(id);
				for (auto &i : di->get_subs16())
					a2->Append(ISO_ptr<malloc_block>(0, data.slice(i.offset, i.len)));
				for (auto &i : di->get_subs32())
					a2->Append(ISO_ptr<malloc_block>(0, data.slice(i.offset, i.len)));
				a->Append(a2);
			} else {
				a->Append(ISO_ptr<malloc_block>(id, move(mem)));
			}
		}
		return a;
	}
} pri;

//-----------------------------------------------------------------------------
//	Decompilers, etc
//-----------------------------------------------------------------------------
#ifdef PLAT_PC

#if defined ISO_EDITOR
string Decompile(const param_element<clr::ENTRY<clr::TypeDef>&, clr::ISO_METADATA*> type) {
	string Decompile(const clr::ENTRY<clr::TypeDef> *type, clr::METADATA* md);
	return Decompile(&get(type), type.p);
}

string DecompileCS(const param_element<clr::ENTRY<clr::TypeDef>&, clr::ISO_METADATA*> type) {
	string DecompileCS(const clr::ENTRY<clr::TypeDef> *type, clr::METADATA* md);
	return DecompileCS(&get(type), type.p);
}

string DecompileMethod(const param_element<clr::ENTRY<clr::MethodDef>&, clr::ISO_METADATA*> method) {
	string Decompile(const clr::ENTRY<clr::MethodDef> *method, clr::METADATA* meta);
	return Decompile(&get(method), method.p);
}

string DecompileAll(clr::CLR &c) {
	string Decompile(clr::METADATA *meta);
	return Decompile(c.MetaData);
}

string DecompileAllCS(clr::CLR &c) {
	string DecompileCS(clr::METADATA *meta);
	return DecompileCS(c.MetaData);
}

#endif

anything MakeMDH(ISO_ptr<clr::CLR> c, const ISO_openarray<ISO::ptr_string<char,32>> &namespaces) {
	void MakeMDH(const clr::METADATA &md, string_accum &sa0, string_accum &sa, const dynamic_array<const char*> &namespaces);
	anything	a;

	if (namespaces) {
		string_builder	sa0, sa;
		MakeMDH(*c->MetaData, sa0, sa, namespaces);

		a.push_back(ISO_ptr<string>("0", move(sa0)));
		a.push_back(ISO_ptr<string>(tag2(), move(sa)));

	} else {
		hash_set_with_key<const char*>	all_ns;
		for (auto &i : c->MetaData->clr::METADATA::GetTable<clr::TypeDef>())
			all_ns.insert(i.namespce);

		for (auto i : all_ns) {
			if (i == cstr("Windows.UI.Xaml.Controls.Primitives"))
				continue;
			if (i && i[0]) {
				ISO_openarray<ISO::ptr_string<char,32>>	ns;
				ns.push_back(i);
				if (i == cstr("Windows.UI.Xaml.Controls"))
					ns.push_back("Windows.UI.Xaml.Controls.Primitives");

				ISO_OUTPUTF("Processing namespace ") << str(i) << '\n';

				string_builder	sa0, sa;
				MakeMDH(*c->MetaData, sa0, sa, ns);

				a.push_back(ISO_ptr<string>(str(i) + ".0", move(sa0)));
				a.push_back(ISO_ptr<string>(i, move(sa)));
			}
		}

	}

	return a;
}

static initialise init(
#if defined ISO_EDITOR
	ISO_get_operation(Decompile),
	ISO_get_operation(DecompileCS),
	ISO_get_operation(DecompileMethod),
	ISO_get_operation(DecompileAll),
	ISO_get_operation(DecompileAllCS),
#endif
	ISO_get_operation(MakeMDH)
);
#endif
