#include "iso/iso_convert.h"
#include "filename.h"
#include "stream.h"
#include "com.h"
#include "windows/registry.h"
#include <OleAuto.h>

using namespace iso;

string_accum& maybe_guid(string_accum &a, const GUID &g) {
	if (g != GUID_NULL)
		a << "__declspec(uuid(\"" << g  << "\")) ";
	return a;
};

uint32 type_desc(string_accum &a, ITypeInfo *tinfo, const TYPEDESC &tdesc, const string &vname, uint32 &alignment) {
	alignment = 0;

	switch (tdesc.vt) {
		case VT_VOID:		a << "void "			<< vname; return 0; 
		case VT_BOOL:		a << "VARIANT_BOOL "	<< vname; return 1; 
		case VT_I1:			a << "char "			<< vname; return 1; 
		case VT_UI1:		a << "unsigned char "	<< vname; return 1; 
		case VT_I2:			a << "short "			<< vname; return 2; 
		case VT_UI2:		a << "unsigned short "	<< vname; return 2; 
		case VT_INT:
		case VT_I4:			a << "int "				<< vname; return 4; 
		case VT_UINT:
		case VT_UI4:		a << "unsigned "		<< vname; return 4; 
		case VT_I8:			a << "__int64 "			<< vname; return 8; 
		case VT_UI8:		a << "unsigned __int64 "<< vname; return 8; 
		case VT_R4:			a << "float "			<< vname; return 4; 
		case VT_R8:			a << "double "			<< vname; return 8; 
		case VT_HRESULT:	a << "HRESULT "			<< vname; return 4; 
        case VT_BSTR:		a << "BSTR "			<< vname; return 8; 
		case VT_DISPATCH:	a << "IDispatch* "		<< vname; return 8; 
		case VT_SAFEARRAY:	a << "SAFEARRAY* "		<< vname; return 8;

		case VT_VARIANT:
			a << "VARIANT " << vname;
			alignment = alignof(VARIANT);
			return sizeof(VARIANT);

		case VT_CARRAY: {
			auto	&adesc	= *tdesc.lpadesc;
			uint32	size	= type_desc(a, tinfo, adesc.tdescElem, vname, alignment);
			ISO_ASSERT(size > 0);
			if (!alignment)
				alignment = size;

			for (int i = 0; i < adesc.cDims; i++) {
				uint32	num		= adesc.rgbounds[i].cElements;
				a  << '[' << num << ']';
				size *= num;
			}
			return size;
		}
		case VT_PTR: {
			type_desc(a, tinfo, *tdesc.lptdesc, "*" + vname, alignment);
			alignment = 0;
			return sizeof(void*);
		}
		case VT_USERDEFINED: {
			com_ptr<ITypeInfo>	tinfo2;
			com_string			tname2;
			TYPEATTR			*tattr2;
			HRESULT				hr;
			hr = tinfo->GetRefTypeInfo(tdesc.hreftype, &tinfo2);
			hr = tinfo2->GetDocumentation(MEMBERID_NIL, &tname2, NULL, NULL, NULL);
			hr = tinfo2->GetTypeAttr(&tattr2);
			a << tname2 << ' ' << vname;
			alignment	= tattr2->cbAlignment;
			uint32 size	= tattr2->cbSizeInstance;
			tinfo2->ReleaseTypeAttr(tattr2);
			return size;
		}
		default:
			ISO_ASSERT(0);
			return 0;
	}
}

void MakeTLH(const filename &in, string_accum &sa) {
	static const char *type_kinds[] = {
		"enum", 
		"struct", 
		"module", 
		"interface", 
		"dispatch", 
		"struct /*coclass*/", 
		"//alias", 
		"union", 
	};

	static const char *call_convs[] = {
		"__fastcall",
		"__cdecl",
		"__pascal",
		"__macpascal",
		"__stdcall",
		"__fpfastcall",
		"__syscall",
		"__mpwcdecl",
		"__mpwpascal",
	};
	HRESULT				hr;
	com_ptr<ITypeLib>	tlib;
	TLIBATTR			*lattr;
	com_string			lname;

	hr = LoadTypeLib(str16(in), &tlib);
	hr = tlib->GetLibAttr(&lattr);
	hr = tlib->GetDocumentation(-1, &lname, NULL, NULL, NULL);

	sa << "// Created by Isopod tools\n";
	sa << "// C++ source equivalent of type library " << in.name() << "\n";
	sa << "// compiler-generated file - DO NOT EDIT!\n\n";
	sa << "#pragma once\n\n";
	sa << "#include <comdef.h>\n\n";


	maybe_guid(sa << "struct ", lattr->guid) << "__" << lname << ";\n";

	sa << "\n// Forward references and typedefs\n\n";

	for (int i = 0, n = tlib->GetTypeInfoCount(); i < n; i++) {
		com_ptr<ITypeInfo>	tinfo;
		com_string			tname;
		TYPEATTR			*tattr;
		hr = tlib->GetTypeInfo(i, &tinfo);
		hr = tinfo->GetDocumentation(MEMBERID_NIL, &tname, NULL, NULL, NULL);
		hr = tinfo->GetTypeAttr(&tattr);

		if (tname == "_FILETIME")
			continue;

		switch (tattr->typekind) {
			//case TKIND_ENUM:
			//case TKIND_RECORD:
			//case TKIND_MODULE:
			//case TKIND_INTERFACE:
			case TKIND_DISPATCH:
			case TKIND_COCLASS:
				maybe_guid(sa << "struct ", tattr->guid) << tname << ";\n";
				break;
			//case TKIND_ALIAS:
			//case TKIND_UNION:
			default:
				sa << type_kinds[tattr->typekind] << ' ' << tname << ";\n";
				break;
		}
	}

	sa << "\n// Type library items\n\n";
	for (int i = 0, n = tlib->GetTypeInfoCount(); i < n; i++) {
		com_ptr<ITypeInfo>	tinfo;
		com_string			tname, tdoc;
		TYPEATTR			*tattr;
		hr = tlib->GetTypeInfo(i, &tinfo);
		hr = tinfo->GetDocumentation(MEMBERID_NIL, &tname, &tdoc, NULL, NULL);
		hr = tinfo->GetTypeAttr(&tattr);

		if (tname == "_FILETIME")
			continue;

//		if (tattr->guid == GUID_NULL)
//			continue;

		if (tdoc)
			sa << "// " << replace(tdoc, "\n", "\n// ") << '\n';

		switch (tattr->typekind) {
			case TKIND_ENUM:
				maybe_guid(sa << "enum ", tattr->guid) << tname << " {\n";
				for (int i = 0, n = tattr->cVars; i < n; i++) {
					VARDESC		*vdesc;
					com_string	vname;
					UINT		num;
					hr = tinfo->GetVarDesc(i, &vdesc);
					tinfo->GetNames(vdesc->memid, &vname, 1, &num);
					sa << '\t' << vname << " = " << com_variant(move(*vdesc->lpvarValue)) << ",\n";
					tinfo->ReleaseVarDesc(vdesc);
				}
				sa << "};\n";
				break;

			case TKIND_RECORD: {
				maybe_guid(sa << "struct ", tattr->guid) << tname << " {\n";
				uint32	offset = 0;
				for (int i = 0, n = tattr->cVars; i < n; i++) {
					VARDESC		*vdesc;
					com_string	vname;
					UINT		num;
					hr = tinfo->GetVarDesc(i, &vdesc);
					tinfo->GetNames(vdesc->memid, &vname, 1, &num);

					uint32	alignment;
					uint32	size = type_desc(sa << '\t', tinfo, vdesc->elemdescVar.tdesc, vname, alignment);
					ISO_ASSERT(size > 0);

					offset = align(offset, alignment ? alignment : size);
					ISO_ASSERT(vdesc->oInst == offset);
					offset += size;

					sa << ";\n";
					tinfo->ReleaseVarDesc(vdesc);
				}
				sa << "};\n";
				break;
			}
			case TKIND_UNION: {
				maybe_guid(sa << "union ", tattr->guid) << tname << " {\n";
				for (int i = 0, n = tattr->cVars; i < n; i++) {
					VARDESC		*vdesc;
					com_string	vname;
					UINT		num;
					hr = tinfo->GetVarDesc(i, &vdesc);
					tinfo->GetNames(vdesc->memid, &vname, 1, &num);

					uint32	alignment;
					uint32	size = type_desc(sa << '\t', tinfo, vdesc->elemdescVar.tdesc, vname, alignment);
					ISO_ASSERT(size > 0);

					sa << ";\n";
					tinfo->ReleaseVarDesc(vdesc);
				}
				sa << "};\n";
				break;
			}
			case TKIND_DISPATCH: {
				maybe_guid(sa << "struct ", tattr->guid) << tname << " : IDispatch {\n";
				for (int i = 7, n = tattr->cFuncs; i < n; i++) {	// first 7 are IDispatch
					FUNCDESC	*fdesc;
					com_string	fname;
					UINT		num;
					uint32		alignment;

					hr = tinfo->GetFuncDesc(i, &fdesc);
					tinfo->GetNames(fdesc->memid, &fname, 1, &num);

					sa << '\t';
					switch (fdesc->funckind) {
						case FUNC_VIRTUAL:
						case FUNC_PUREVIRTUAL:
							sa << "virtual ";
							break;
						case FUNC_DISPATCH:
							sa << "virtual HRESULT " << call_convs[fdesc->callconv] << ' ';
							break;
						case FUNC_NONVIRTUAL:
							break;
						case FUNC_STATIC:
							sa << "static ";
							break;
					}

					switch (fdesc->invkind) {
						case INVOKE_FUNC:
							sa << fname << '(';
							for (int i = 0; i < fdesc->cParams; i ++) {
								if (i > 0)
									sa << ", ";
								uint32	size = type_desc(sa, tinfo, fdesc->lprgelemdescParam[i].tdesc, 0, alignment);
								ISO_ASSERT(size > 0);
							}
							if (fdesc->elemdescFunc.tdesc.vt != VT_VOID) {
								if (fdesc->cParams > 0)
									sa << ", ";
								type_desc(sa, tinfo, fdesc->elemdescFunc.tdesc, "*pVal", alignment);
							}
							break;
						case INVOKE_PROPERTYGET:
							type_desc(sa << "get_" << fname << "(", tinfo, fdesc->elemdescFunc.tdesc, "*pVal", alignment);
							break;
						case INVOKE_PROPERTYPUT:
							type_desc(sa << "put_" << fname << "(", tinfo, fdesc->lprgelemdescParam[0].tdesc, "pVal", alignment);
							break;
						case INVOKE_PROPERTYPUTREF:
							break;
					}
						
					sa << ')';
					if (fdesc->funckind == FUNC_PUREVIRTUAL || fdesc->funckind == FUNC_DISPATCH)
						sa << " = 0";

					sa << ";\n";
					tinfo->ReleaseFuncDesc(fdesc);
				}
				sa << "};\n";
				break;
			}

			case TKIND_INTERFACE: {
				//TBD
				break;
			}
									  
			case TKIND_COCLASS:
				//nothing for 2nd pass
				break;

			case TKIND_MODULE:
			case TKIND_ALIAS:
				//TBD
				break;
		}

		tinfo->ReleaseTypeAttr(tattr);
	}
}

string GetLibID(const char *progid) {
	win::RegKey	r(HKEY_CLASSES_ROOT, progid, KEY_READ);
	win::RegKey	r2(HKEY_CLASSES_ROOT, "CLSID\\" + (string)r["CLSID"].value(), KEY_READ);
	return r2["TypeLib"].value();
}

filename GetTypeLib(const char *libid) {
	win::RegKey	r2(HKEY_CLASSES_ROOT, "TypeLib\\" + str(libid), KEY_READ);
	return r2[0][0]["win64"].value().get_text();
}


string TypeLib(const ISO::unescaped<string> &fn) {
	filename		fn2
		= fn.begins("progid:") ? GetTypeLib(GetLibID(fn.slice(7)))
		: fn.begins("libid:") ? GetTypeLib(fn.slice(6))
		: (filename)fn;
	string_builder	a;
	MakeTLH(fn2, a);
	return a;
}

static initialise init(
	ISO_get_operation(TypeLib)
);
