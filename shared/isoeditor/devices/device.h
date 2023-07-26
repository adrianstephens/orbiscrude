#ifndef DEVICE_H
#define DEVICE_H

#include "iso/iso.h"

#ifdef PLAT_WINRT
#include "winrt/window.h"
#else
#include "windows/window.h"
#endif

namespace app {
using namespace iso;
using namespace win;

typedef virtfunc<ISO_ptr_machine<void>(const Control&)>	DeviceCreate;
template<typename T> struct DeviceCreateT : DeviceCreate { DeviceCreateT() : DeviceCreate(static_cast<T*>(this)) {} };
win::Bitmap LoadPNG(win::ID id);

#ifdef PLAT_WINRT

struct DeviceAdd {
	Control			sidebar;
	HTREEITEM		parent;
	DeviceAdd(const Control &_sidebar, HTREEITEM _parent = TVI_ROOT) : sidebar(_sidebar), parent(_parent) {}
	void	operator()(const char *text, DeviceCreate *dev, win::Bitmap bm = win::Bitmap()) const {
		auto	item = ref_new<Controls::NavigationViewItem>();
		item->Content = hstring(str(text));
		item->DataContext = iso_winrt::Platform::make_value(dev);
		item->Icon = bm;
		((ptr<Controls::NavigationView>)sidebar)->MenuItems->Append(item);
	}
	DeviceAdd	operator()(const char *text, win::Bitmap bm = win::Bitmap()) const {
		auto	item = ref_new<Controls::NavigationViewItem>();
		item->Content = hstring(str(text));
//		item->DataContext = group;
		item->Icon = bm;
		((ptr<Controls::NavigationView>)sidebar)->MenuItems->Append(item);

		return DeviceAdd(sidebar);
	}
};

#elif defined PLAT_PC

struct DeviceAdd {
	Menu menu;
	int id;
	DeviceAdd() {}
	DeviceAdd(Menu _menu, int _id) : menu(_menu), id(_id) {}

	void	_add(Menu::Item &item, win::Bitmap bm, int pos = -1) const {
		if (bm)
			item.Bitmap(bm.ScaledTo(SystemMetrics::MenuCheckSize()));
		item.InsertByPos(menu, pos);
	}

	void	operator()(string_param text, DeviceCreate *dev, win::Bitmap bm = win::Bitmap()) const {
		_add(Menu::Item(text, id).Param(dev), bm);
	}
	void	to_top(string_param text, DeviceCreate *dev, win::Bitmap bm = win::Bitmap()) const {
		_add(Menu::Item(text, id).Param(dev), bm, 0);
	}
	DeviceAdd	operator()(string_param text, MenuCallback *cb, win::Bitmap bm = win::Bitmap()) const {
		Menu	sub = Menu::Create();
		_add(Menu::Item(text, id).Param(cb).SubMenu(sub), bm);
		return DeviceAdd(sub, id);
	}
	DeviceAdd	operator()(string_param text, win::Bitmap bm = win::Bitmap()) const {
		Menu	sub = Menu::Create();
		_add(Menu::Item(text, id).SubMenu(sub), bm);
		return DeviceAdd(sub, id);
	}
};

#else

struct DeviceAdd {
	OutlineControl	sidebar;
	HTREEITEM		parent;
	DeviceAdd(const OutlineControl &_sidebar, HTREEITEM _parent = TVI_ROOT) : sidebar(_sidebar), parent(_parent) {}
	void	operator()(const char *text, DeviceCreate *dev, win::Bitmap bm = win::Bitmap()) const {
		OutlineControl::Item(text).Param(dev).Image(bm).Insert(sidebar, parent);
	}
	DeviceAdd	operator()(const char *text, win::Bitmap bm = win::Bitmap()) const {
		return DeviceAdd(sidebar, OutlineControl::Item(text).Image(bm).Insert(sidebar, parent));
	}
};

#endif

typedef virtfunc<void(const app::DeviceAdd&)>	_DeviceAdd;

class Device : public static_list<Device>, public _DeviceAdd {
public:
	void	Add(const app::DeviceAdd &add)	{ (*this)(add); }
	template<class T> Device(T *me) : _DeviceAdd(me)	{}
};

template<typename T> struct DeviceT : Device {
	DeviceT() : Device(static_cast<T*>(this)) {}
};


#ifdef PLAT_WIN32

ISO_ptr_machine<void> GetRegistry(tag id, HKEY h);
ISO_ptr_machine<void> GetRegistry(tag id, HKEY h, const char *subkey);

class GetURLPWDialog : public Dialog<GetURLPWDialog> {
public:
	fixed_string<1024>	url;
	fixed_string<1024>	user;
	fixed_string<1024>	pw;
	int					ret;

	LRESULT	Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	operator bool()				{ return ret != 0;	}
	operator const char *()		{ return url.blank() ? NULL : (const char*)url; }

	GetURLPWDialog(HWND hWndParent, const char *_url, const char *_user, const char *_pw);
};
#endif


} // namespace app

#ifdef PLAT_WINRT
namespace iso_winrt {
	template<> struct from_abi_s<app::DeviceCreate*>		{ typedef app::DeviceCreate *type; };
}
#endif


#endif //DEVICE_H
