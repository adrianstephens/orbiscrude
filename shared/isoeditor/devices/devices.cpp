#include "base/algorithm.h"
#include "device.h"
#include "main.h"
#include "thread.h"
#include "iso/iso_files.h"
#include "iso/iso_script.h"
#include "comms/upnp.h"
#include "systems/communication/isolink.h"

#if defined PLAT_PC
#ifdef PLAT_WINRT
#include "winrt/filedialogs.h"
#else
#include "windows/filedialogs.h"
#include "windows/control_helpers.h"
#include "windows/dib.h"
#include <winioctl.h>
#endif
#endif

namespace iso {

template<typename T> struct string_traits<string_getter<T>> {
	typedef	string_getter<T>	S;
	typedef _none				start_type;
	typedef const char			element, *iterator;

	static constexpr _none		start(const S &s)		{ return none; }
	static constexpr _none		terminator(const S &s)	{ return none; }
	static constexpr iterator	begin(const S &s)		{ return 0; }
	static constexpr iterator	end(const S &s)			{ return 0; }
	static constexpr size_t		len(const S &s)			{ return s.len(); }
};

template<typename T> struct string_traits<const string_getter<T>> : string_traits<string_getter<T>> {};

template<typename T> string_base<T> make_str(T &&t) {
	return forward<T>(t);
}

}

using namespace app;

#ifdef PLAT_WINRT


Bitmap app::LoadPNG(ID id) {
	ptr<Controls::BitmapIcon>	icon = ref_new<Controls::BitmapIcon>();
	icon->UriSource = ref_new<Platform::Uri>(L"ms-appx:///Assets/" + id + L".png");
	return icon;
}

#elif defined PLAT_PC

win::Bitmap PreMultiplyAlpha(const win::Bitmap &bm) {
	Point			size = bm.GetSize();
	void			*data2;
	win::Bitmap		bm2 = win::Bitmap::CreateDIBSection(bm.GetSize(), 32, 1, &data2);

	transform(MakeDIBBlock((DIBHEADER::RGBQUAD*)bm.GetBits(DeviceContext::Screen(), 0, size.y), size), MakeDIBBlock((DIBHEADER::RGBQUAD*)data2, size), [](const DIBHEADER::RGBQUAD &p) {
		ISO_rgba	c = p;
		c *= c.a;
		return c;
	});
	return bm2;
}

Bitmap app::LoadPNG(win::ID id) {
	Resource		r(id, "PNG");
	ISO_ptr<bitmap>	p	= FileHandler::Get("PNG")->Read(0, lvalue(memory_reader(r)));
	Point			size(p->Width(), p->Height());

	void	*data;
	Bitmap	bm = Bitmap::CreateDIBSection(size, 32, 1, &data);
	copy(p->All(), MakeDIBBlock((DIBHEADER::RGBQUAD*)data, size));
	return bm;
//	unique_ptr<DIB>	dib = DIB::Create(*p);
//	return dib->CreateDIBSection(DeviceContext());
}

GetURLPWDialog::GetURLPWDialog(HWND hWndParent, const char *_url, const char *_user, const char *_pw) : url(_url), user(_user), pw(_pw) {
	ret = Modal(hWndParent, IDD_OPENURL_PW);
}

LRESULT	GetURLPWDialog::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_INITDIALOG: {
			Item(IDC_URL).SetText(url);
			Item(IDC_USER).SetText(user);
			Item(IDC_PW).SetText(pw);
			break;
		}
		case WM_COMMAND: {
			switch (LOWORD(wParam)) {
				case IDOK: {
					url		= Item(IDC_URL).GetText();
					user	= Item(IDC_USER).GetText();
					pw		= Item(IDC_PW).GetText();
					return EndDialog(1);
				}
				case IDCANCEL:
					return EndDialog(0);
			}
			break;
		}
	}
	return FALSE;
}
#else

Bitmap app::LoadPNG(win::ID id) {
	if (id.s.begins("IDB_DEVICE_"))
		return Bitmap(id.s + 11);
	return Bitmap(id);
}

#endif

//-----------------------------------------------------------------------------
//	RootDevice
//-----------------------------------------------------------------------------

struct RootDevice : DeviceT<RootDevice>, DeviceCreateT<RootDevice> {
	void			operator()(const DeviceAdd &add) {
		add("Root", this, LoadPNG("IDB_DEVICE_ROOT"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		return ISO::MakePtr("root", &ISO::root());
	}
} root_device;

//-----------------------------------------------------------------------------
//	FileDevice
//-----------------------------------------------------------------------------


struct FileDevice : DeviceT<FileDevice>, DeviceCreateT<FileDevice> {
	filename	fn;
	void			operator()(const DeviceAdd &add) {
		add("File...", this, LoadPNG("IDB_DEVICE_FILE"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		string	s = GetFileFilter();

#ifdef PLAT_WINRT
		if (fn = to_string(GetOpen("Open File", s.begin()))) {
#else
		if (GetOpen(main, fn, "Open File", s)) {
#endif
		#ifdef PLAT_PC
			//IsoEditor::Cast(main)->AddRecent(fn);
		#endif
			Busy bee;
			tag	id = fn.name();
			if (ISO_ptr<void> p = FileHandler::Read(id, fn))
				return p;
			if (ISO_ptr<void> p = FileHandler::Get("bin")->ReadWithFilename(id, fn))
				return p;
			throw_accum("Can't read " << fn);
		}
		return ISO_NULL;
	}
} file_device;


//-----------------------------------------------------------------------------
//	DirectoryDevice
//-----------------------------------------------------------------------------


struct DirectoryDevice : DeviceT<DirectoryDevice>, DeviceCreateT<DirectoryDevice> {
	filename	fn;
	void			operator()(const DeviceAdd &add) {
		add("Directory...", this, LoadPNG("IDB_DEVICE_FOLDER"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
#ifndef PLAT_WINRT
		if (GetDirectory(main, fn, "Select Destination"))
			return ISO::GetDirectory(fn, fn);
#endif
		return ISO_NULL;
	}
} directory_device;


//-----------------------------------------------------------------------------
//	SnippetsDevice
//-----------------------------------------------------------------------------
#ifdef PLAT_WIN32

struct SnippetsDevice : DeviceT<SnippetsDevice>, MenuCallbackT<SnippetsDevice> {
	int		id;
	RegKey	reg;
	Event	event;

	struct Snippet : DeviceCreateT<Snippet>, Handles2<Snippet,AppEvent> {
		string text;
		ISO_ptr<void>	operator()(const Control &main) {
			return ISO::ScriptRead("snippet", filename(), lvalue(memory_reader(const_memory_block(text))), ISO::SCRIPT_KEEPEXTERNALS | ISO::SCRIPT_DONTCONVERT);
		}
		void	operator()(AppEvent *ev) {
			if (ev->state == AppEvent::END)
				delete this;
		}
		Snippet(const char *_text) : text(_text)	{}
	};

	SnippetsDevice() {
		reg = Settings(true).subkey("Snippets", KEY_ALL_ACCESS);
		event.signal();
	}

	void			ClearMenu(Menu m) {
		Menu::Item item(MIIM_DATA | MIIM_SUBMENU);
		while (item._GetByPos(m, 0)) {
			if (Menu m2 = item.SubMenu())
				ClearMenu(m2);
			else if (!is_any(m.GetItemTextByPos(0), cstr("Add..."), cstr("Edit...")))
				delete (Snippet*)item.Param();
			m.RemoveByPos(0);
		}
	}

	void			operator()(const DeviceAdd &add) {
		this->id	= add.id;
		add("Snippets", this, app::LoadPNG("IDB_DEVICE_CODE"));
	}
	void			operator()(Control c, Menu m) {
		if (event.wait(0)) {
			RegNotifyChangeKeyValue(reg, true, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, event, true);

			ClearMenu(m);

			for (auto v : reg.values()) {
				Menu			sub = m;
				if (v.name[0]) {
					sub	= Menu::Create();
					Menu::Item().Text(v.name).SubMenu(sub).AppendTo(m);
				}
				multi_string_alloc<char>	s = v.get();
				DeviceAdd	add(sub, id);
				for (auto t = s.begin(); *t; ++t)
					add(*t, new Snippet(*t));
			}
			DeviceAdd	add(m, id);

			add("Edit...", new_lambda<DeviceCreate>([this](const Control &c) {
				return (ISO_ptr<void>)GetRegistry("Snippets", reg);
			}));

			add("Add...", new_lambda<DeviceCreate>([this](const Control &c) {
				fixed_string<1024>	value;
				if (GetValueDialog(c, value)) {
					auto	v	= reg.value();
					v = v.get<multi_string_alloc<char>>() + value;
				}
				return ISO_NULL;
			}));
		}
	}
} snippets_device;
#endif

//-----------------------------------------------------------------------------
//	KnownFoldersDevice
//-----------------------------------------------------------------------------
#ifdef PLAT_WIN32

template<typename T> struct com_data {
	T	*p;
	com_data()	: p(0)		{}
	~com_data()				{ CoTaskMemFree(p); }
	T**	operator&()			{ ISO_ASSERT(!p); return &p; }
	T*	get()		const	{ return p; }
	operator T*()	const	{ return p; }
};

struct KnownFoldersDevice : DeviceT<KnownFoldersDevice>, DeviceCreateT<KnownFoldersDevice> {
	filename	fn;
	void			operator()(const DeviceAdd &add) {
		add("Known Folders", this, LoadPNG("IDB_DEVICE_FOLDER"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		init_com();
		com_ptr<IKnownFolderManager>	kfm;
		if (kfm.create(CLSID_KnownFolderManager)) {
			com_data<KNOWNFOLDERID>	ids;
			UINT			num		= 0;
			HRESULT			hr		= kfm->GetFolderIds(&ids, &num);
			if (SUCCEEDED(hr)) {
				ISO_ptr<anything>	p("KnownFolders");

				for (int i = 0; i < num; ++i) {
					com_ptr<IKnownFolder>	kf;
					if (SUCCEEDED(hr = kfm->GetFolder(ids[i], &kf))) {
						KNOWNFOLDER_DEFINITION kfd;
						if (SUCCEEDED(hr = kf->GetFolderDefinition(&kfd))) {
							com_data<wchar_t>	path;
							if (SUCCEEDED(hr = kf->GetPath(0, &path))) {
								p->Append(ISO_ptr<string>(str8(kfd.pszName), path.get()));
							} else {
								p->Append(ISO_ptr<string>(str8(kfd.pszName), kfd.pszParsingName));
							}
							FreeKnownFolderDefinitionFields(&kfd);
						}
					}
				}
				return p;
			}
		}
		return ISO_NULL;
	}
} knownfolders_device;

#endif
