#include "thread.h"
#include "com.h"

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

}

using namespace iso;

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
