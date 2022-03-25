#include "plugin.h"
#include "iso/iso_files.h"
#include "windows/window.h"

namespace iso {
_none				none, terminate, empty;
}

using namespace iso;


class PluginFromModule : public Plugin {
	string	desc;
public:
	const char*		GetDescription() {
		return desc;
	}
	PluginFromModule(HMODULE h) {
		if (const win::VersionLink *p = win::Resource(h, VS_VERSION_INFO, RT_VERSION)) {
			p = p->child()->child()->child();
			while (p->name != cstr("FileDescription"))
				p = p->sibling();
			desc = (char16*)p->data();
		}
	}
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH: {
			static PluginFromModule plugin(hinstDLL);
			break;
		}
		case DLL_THREAD_ATTACH:		break;
		case DLL_THREAD_DETACH:		break;
		case DLL_PROCESS_DETACH:	break;
	}
	return TRUE;
}
