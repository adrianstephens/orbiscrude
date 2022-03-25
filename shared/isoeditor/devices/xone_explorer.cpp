#include "comms/http.h"
#include "com.h"
#include <XtfConsoleManager.h>
#include "stream.h"
#include "filename.h"
#include "iso/iso_binary.h"
#include "communication/isolink.h"
#include "bitmap/bitmap.h"
#include "container/archive_help.h"

using namespace iso;

class XONEExplorerConsole : public ISO_virtual_defaults {
	string							name, host;
	ISO_ptr<bitmap>					bm;
public:
	const char*		Name()				{ return name;						}
};

class XONEExplorer : public ISO_virtual_defaults {
	dynamic_array<XONEExplorerConsole>	consoles;
public:
	bool			Init();
public:
	uint32			Count()				{ return uint32(consoles.size());	}
	tag				GetName(int i)		{ return consoles[i].Name();		}
	ISO_browser2	Index(int i)		{ return MakePtr(consoles[i].Name(), &consoles[i]);	}
};


struct ConsoleCallback : com<IXtfConsoleManagerCallback> {
	void STDMETHODCALLTYPE OnAddConsole(const XTFCONSOLEDATA *pConsoleData) {}
	void STDMETHODCALLTYPE OnRemoveConsole(const XTFCONSOLEDATA *pConsoleData) {}
	void STDMETHODCALLTYPE OnChangedDefaultConsole(const XTFCONSOLEDATA *pConsoleData) {}
};

struct EnumCallback : com<IXtfEnumerateConsolesCallback> {
	HRESULT STDMETHODCALLTYPE OnConsoleFound(const XTFCONSOLEDATA *pConsoleData) {
		return S_OK;
	}
};

bool XONEExplorer::Init() {
	init_com();

	ConsoleCallback	ccb;
	EnumCallback	ecb;

	com_ptr<IXtfConsoleManager>	xbm;

	HRESULT hr;
	hr	= XtfCreateConsoleManager(&ccb, __uuidof(xbm), (void**)&xbm);
	hr	= xbm->EnumerateConsoles(&ecb);


	return true;
}

ISO_DEFVIRT(XONEExplorerConsole);
ISO_DEFVIRT(XONEExplorer);

#include "device.h"

struct XONEDevice : app::DeviceT<XONEDevice>, app::DeviceCreateT<XONEDevice> {
	void			operator()(const app::DeviceAdd &add)	{
		add("Xbox ONE Explorer", this, app::LoadPNG("IDB_DEVICE_XBOX"));
	}
	ISO_ptr<void>	operator()(win::Control &main)		{
		win::Busy();
		ISO_ptr<XONEExplorer> p("Xbox ONE Explorer");
		if (p->Init())
			return p;
		return ISO_NULL;
	}
} xone_device;
