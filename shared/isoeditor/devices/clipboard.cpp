#include "main.h"
#include "device.h"
#include <objidl.h>
#include "com.h"
#include "windows\dib.h"
#include "iso\iso_binary.h"
#include "stream.h"

using namespace app;

int UsableClipboardContents() {
	return	Clipboard::Available(CF_ISOPOD)	? CF_ISOPOD
		:	Clipboard::Available(_CF_HTML)	? CF_HTML
		:	Clipboard::Available(CF_TEXT)	? CF_TEXT
		:	Clipboard::Available(CF_DIB)	? CF_DIB
		:	0;
}

ISO_ptr<void> GetClipboardContents(tag2 id, HWND hWnd) {
	if (int	format = UsableClipboardContents()) {
		Clipboard	clip(hWnd);
		Busy		bee;
		if (auto g = clip.Get(format)) {
			switch (format) {
				case CF_TEXT:
					if (auto s = g.data())
						return ISO_ptr<string>(id, s);
					break;

				case CF_DIB:
					if (auto dib = g.data())
						return ((DIB*)dib)->CreateBitmap(tag2(id));
					break;

				case CF_ISOPOD:
					((ISO::Browser*)g.data())->Duplicate();

				case CF_HTML: {
					void	*p = g.lock();
					g.unlock();
					break;
				}
			}
		}
	}
	return ISO_NULL;
}

struct ClipboardDevice : DeviceT<ClipboardDevice>, DeviceCreateT<ClipboardDevice> {
	void			operator()(const DeviceAdd &add) {
		add("Clipboard", this, LoadPNG("IDB_DEVICE_CLIPBOARD"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		return GetClipboardContents("clipboard", main);
	}
} clipboard_device;

