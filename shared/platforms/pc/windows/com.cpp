#include "thread.h"
#include "com.h"
#include "extra/date.h"

namespace iso {

HRESULT CoCreateInstanceFromDLL(void **unknown, const char *fn, const CLSID &clsid, const IID &iid) {
	typedef	BOOL (WINAPI T_DllGetClassObject)(REFCLSID,REFIID,LPVOID);
	if (HMODULE hmodule = LoadLibrary(fn)) {
		if (T_DllGetClassObject	*f = (T_DllGetClassObject*)GetProcAddress(hmodule, "DllGetClassObject")) {
			com_ptr<IClassFactory>	cf;
			HRESULT hr = f(clsid, IID_IClassFactory, &cf);
			return FAILED(hr) ? hr : cf->CreateInstance(NULL, iid, unknown);
		}
	}
	return HRESULT_FROM_WIN32(GetLastError());
}

CriticalSection	TypeInfo_cs;

bool _IDispatch::Init(LCID loc) {
	auto	w	= with(TypeInfo_cs);
	if (!info) {
		com_ptr<ITypeLib>	lib;
		if (FAILED(LoadRegTypeLib(*libid, major, minor, loc, &lib))
		||	FAILED(lib->GetTypeInfoOfGuid(iid, &info))
		)
			return false;
	}

	if (info && !map) {
		TYPEATTR	*attr;
		HRESULT	hr	= info->GetTypeAttr(&attr);
		count		= attr->cFuncs;
		map			= count ? new entry[count] : 0;
		for (int i = 0; i < count; i++) {
			FUNCDESC	*f;
			if (SUCCEEDED(info->GetFuncDesc(i, &f))) {
				if (SUCCEEDED(info->GetDocumentation(f->memid, &map[i].name, NULL, NULL, NULL))) {
					map[i].len	= SysStringLen(map[i].name);
					map[i].id	= f->memid;
				}
				info->ReleaseFuncDesc(f);
			}
		}
		info->ReleaseTypeAttr(attr);
	}
	return true;
}

HRESULT	_IDispatch::GetIDsOfNames(REFIID riid, BSTR *names, UINT num, LCID loc, DISPID *dispid) {
	if (Check(loc)) {
		if (num == 1) {
			int	len = SysStringLen(names[0]);
			for (int j = count; j--;) {
				if (len == map[j].len && memcmp(map[j].name, names[0], len * 2) == 0) {
					dispid[0] = map[j].id;
					return S_OK;
				}
			}
		}
		return info->GetIDsOfNames(names, num, dispid);
	}
	return E_FAIL;
}


template<typename C> accum<C> print_variant(accum<C> &a, VARTYPE vt, const void *p) {
	switch (vt) {
		case VT_EMPTY:	return a << "<empty>";
		case VT_NULL:	return a << "<null>";
		case VT_I2:		return a << *(SHORT*)p;
		case VT_I4:		return a << *(LONG*)p;
		case VT_R4:		return a << *(float*)p;
		case VT_R8:		return a << *(double*)p;
		case VT_BSTR:	return a << *(com_string*)p;
		case VT_BOOL:	return a << V_BOOL(&v);
		case VT_I1:		return a << V_I1(&v);
		case VT_UI1:	return a << V_UI1(&v);
		case VT_UI2:	return a << V_UI2(&v);
		case VT_UI4:	return a << V_UI4(&v);
		case VT_I8:		return a << v.hVal.QuadPart;
		case VT_UI8:	return a << v.uhVal.QuadPart;
		case VT_INT:	return a << V_INT(&v);
		case VT_UINT:	return a << V_UINT(&v);
		case VT_VOID:	return a << "<void>";
		case VT_HRESULT:return a << V_UI4(&v);
		case VT_LPSTR:	return a << v.pszVal;
		case VT_LPWSTR:	return a << v.pwszVal;
		case VT_FILETIME: return a << DateTime(v.filetime);
		case VT_DATE:	return a << (DateTime(1900, 1, 1) + Duration::Days(v.date - 2));
		case VT_CLSID:	return a << *v.puuid;

		default:
			if (v.vt & VT_VECTOR) {
				v.caub.cElems;
				v.caub.pElems;

			} else if (v.vt & VT_ARRAY) {
			} else if (v.vt & VT_BYREF) {
			} else {
				return a << "<unsupported variant " << v.vt << '>';
			}
			//		case VT_CY:
			//		case VT_VARIANT:
			//		case VT_UNKNOWN:
			//		case VT_DECIMAL:
			//		case VT_DISPATCH:
			//		case VT_ERROR:
			//		case VT_PTR:
			//		case VT_SAFEARRAY:
			//		case VT_CARRAY:
			//		case VT_USERDEFINED:
			//		case VT_RECORD:
			//		case VT_INT_PTR:
			//		case VT_UINT_PTR:
			//		case VT_BLOB:
			//		case VT_STREAM:
			//		case VT_STORAGE:
			//		case VT_STREAMED_OBJECT:
			//		case VT_STORED_OBJECT:
			//		case VT_BLOB_OBJECT:
			//		case VT_CF:
			//		case VT_VERSIONED_STREAM:
	}
}

template<typename C, typename T> accum<C>& variant_vector(accum<C>& a, const T *p, uint32 n) {
	a << '[';
	int	j = 0;
	for (auto &i : make_range_n(p, n))
		a << onlyif(j++, ", ") << i;
	return a << ']';
}

template<typename C, typename T> accum<C>& variant_vector(accum<C>& a, const T& array) {
	return variant_vector(a, array.pElems, array.cElems);
}

template<typename C> accum<C> &operator<<(accum<C> &a, const com_variant &v) {
	switch (v.vt) {
		case VT_EMPTY:	return a << "<empty>";
		case VT_NULL:	return a << "<null>";
		case VT_I2:		return a << V_I2(&v);
		case VT_I4:		return a << V_I4(&v);
		case VT_R4:		return a << V_R4(&v);
		case VT_R8:		return a << V_R8(&v);
		case VT_BSTR:	return a << (com_string&)v.bstrVal;
		case VT_BOOL:	return a << V_BOOL(&v);
		case VT_I1:		return a << V_I1(&v);
		case VT_UI1:	return a << V_UI1(&v);
		case VT_UI2:	return a << V_UI2(&v);
		case VT_UI4:	return a << V_UI4(&v);
		case VT_I8:		return a << v.hVal.QuadPart;
		case VT_UI8:	return a << v.uhVal.QuadPart;
		case VT_INT:	return a << V_INT(&v);
		case VT_UINT:	return a << V_UINT(&v);
		case VT_VOID:	return a << "<void>";
		case VT_HRESULT:return a << V_UI4(&v);
		case VT_LPSTR:	return a << v.pszVal;
		case VT_LPWSTR:	return a << v.pwszVal;
		case VT_FILETIME: return a << DateTime(v.filetime);
		case VT_DATE:	return a << (DateTime(1900, 1, 1) + Duration::Days(v.date - 2));
		case VT_CLSID:	return a << *v.puuid;

		default:
			if (v.vt & VT_VECTOR) {
				switch (v.vt & 0xfff) {
					case VT_I1:			return variant_vector(a, v.cac);
					case VT_UI1:		return variant_vector(a, v.caub);
					case VT_I2:			return variant_vector(a, v.cai);
					case VT_UI2:		return variant_vector(a, v.caui);
					case VT_I4:			return variant_vector(a, v.cal);
					case VT_UI4:		return variant_vector(a, v.caul);
					case VT_I8:			return variant_vector(a, (int64*)v.cah.pElems, v.cah.cElems);
					case VT_UI8:		return variant_vector(a, (uint64*)v.cauh.pElems, v.cauh.cElems);
					case VT_R4:			return variant_vector(a, v.caflt);
					case VT_R8:			return variant_vector(a, v.cadbl);
					case VT_BOOL:		return variant_vector(a, v.cabool);
					//case VT_SCODE:		return variant_vector(a, v.cascode);
					case VT_CY:			return variant_vector(a, v.cacy);
					case VT_DATE:		return variant_vector(a, v.cadate);
					case VT_FILETIME:	return variant_vector(a, v.cafiletime);
					case VT_CLSID:		return variant_vector(a, v.cauuid);
					//case VT_CLIPDATA:	return variant_vector(a, v.caclipdata);
					case VT_BSTR:		return variant_vector(a, v.cabstr);
					//case VT_BSTRBLOB:	return variant_vector(a, v.cabstrblob);
					case VT_LPSTR:		return variant_vector(a, v.calpstr);
					case VT_LPWSTR:		return variant_vector(a, v.calpwstr);
					case VT_VARIANT:	return variant_vector(a, (com_variant*)v.capropvar.pElems, v.capropvar.cElems);
				}

			} else if (v.vt & VT_ARRAY) {
			} else if (v.vt & VT_BYREF) {
			}
			return a << "<unsupported variant " << v.vt << '>';

//		case VT_CY:
//		case VT_VARIANT:
//		case VT_UNKNOWN:
//		case VT_DECIMAL:
//		case VT_DISPATCH:
//		case VT_ERROR:
//		case VT_PTR:
//		case VT_SAFEARRAY:
//		case VT_CARRAY:
//		case VT_USERDEFINED:
//		case VT_RECORD:
//		case VT_INT_PTR:
//		case VT_UINT_PTR:
//		case VT_BLOB:
//		case VT_STREAM:
//		case VT_STORAGE:
//		case VT_STREAMED_OBJECT:
//		case VT_STORED_OBJECT:
//		case VT_BLOB_OBJECT:
//		case VT_CF:
//		case VT_VERSIONED_STREAM:
	}
}

template accum<char>	&operator<<(accum<char> &a, const com_variant &v);
template accum<char16>	&operator<<(accum<char16> &a, const com_variant &v);

}//namespace iso