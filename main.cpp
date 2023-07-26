#define ISO_TEST

#include "main.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "iso/iso_script.h"
#include "windows/filedialogs.h"
#include "windows/registry.h"
#include "windows/dialog.h"
#include "extra/date.h"
#include "extra/async_stream.h"
#include "extra/regex.h"
#include "comms/http.h"
#include "comms/ftp.h"
#include "codec/aes.h"
#include "codec/cbc.h"
#include "hashes/sha.h"
#include "extra/c-types.h"
#include "comms/zip.h"
#include "directory.h"
#include "thread.h"
#include "sockets.h"
#include "plugin.h"
#include "hook.h"
#include "../../platforms/ps4/shared/sdb.h"
#include "windows/text_control.h"
#include "base/sparse_array.h"
#include "packed_types.h"

#include "resource.h"

#ifdef USE_VLD
#include "vld.h"
#endif

#include <shellapi.h>
#include <ShellScalingApi.h>
#include <dbghelp.h>

#ifndef __clang__
#pragma comment(linker, "\"\
/manifestdependency:\
type='win32' \
name='Microsoft.Windows.Common-Controls' \
version='6.0.0.0' \
processorArchitecture='*' \
publicKeyToken='6595b64144ccf1df' \
language='*' \
\"")
#endif

#include "base/vector.h"
#include "maths/geometry.h"
#include "base/soft_float.h"

namespace iso {
	struct Model3;
	ISO_INIT(Model3)	{}
	ISO_DEINIT(Model3)	{}
	extern C_types	user_ctypes;
}

using namespace app;

MainWindow		*app::MainWindow::me;

Control Settings(MainWindow &_main);

string_accum &VersionString(string_accum &sa, uint64 ver) {
	return sa << uint16(ver >> 48) << '.' << uint16(ver >> 32) << '.' << uint16(ver >> 16) << '.' << uint16(ver >> 0);
}

//-----------------------------------------------------------------------------
//	MainWindow
//-----------------------------------------------------------------------------

MainWindow::MainWindow() : SeparateWindow(48)	{ me = this; flags |= NO_DOCK; }
MainWindow::~MainWindow()						{ me = 0; }

void MainWindow::SetTitle(const char *title) {
	SetText(VersionString(buffer_accum<256>() << title << " - ORBIScrude ver ", GetVersion(0)));

}
void MainWindow::AddView(Control control) {
	//BlurBehind(false);
	control.Show();
	SetChild(control);
}

void MainWindow::AddLoadFilters(cmulti_string filters) {
	load_filters += filters;
}

struct RecentFiles : MenuItemCallbackT<RecentFiles>, Menu {
	int		next_id;

	void	operator()(Control c, Menu::Item i) {
		int		id	= i.ID();
		if (GetKeyState(VK_RBUTTON) & 0x8000) {
			Menu	popup = Menu::Popup();
			popup.Append("Remove", 2);
			int	r = popup.Track(c, GetMousePos(), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD);
			if (r == 2) {
				//settings.Remove(id - 1);
				RemoveByID(id);
			}
			return;
		}
		c(WM_COMMAND, wparam(ID_FILE_OPEN, 2), i.Text());
	}

	RecentFiles() : Menu(Menu::Popup()), next_id(1) {
		SetStyle(MNS_NOTIFYBYPOS);
		RemoveByPos(0);
	}
	void	add_nocheck(const char *fn, int at = -1) {
		if (fn)
			InsertByPos(at, Menu::Item(fn, next_id++).Param(static_cast<MenuItemCallback*>(this)));
	}
	int		find(const char *fn) {
		for (auto &i : Items()) {
			if (str(i.Text()) == fn)
				return i.ID();
		}
		return -1;
	}
	void	add(const char *fn) {
		for (auto &i : Items()) {
			if (str(i.Text()) == fn) {
				RemoveByID(i.ID());
				InsertByPos(0, i);
				return;
			}
		}
		add_nocheck(fn, 0);
	}
};
struct vertical_split : Rect {
	typedef pair<int, Rect> element, reference;
	typedef indexed_iterator<vertical_split&, int_iterator<int> > iterator;
	typedef random_access_iterator_t iterator_category;

	int		num;

	vertical_split(const Rect &r, int _num) : Rect(r), num(_num) {}
	element		operator[](int i) const { return element(i, Subbox(0, Height() * i / num, 0, Height() * (i + 1) / num)); }
	iterator	begin()					{ return iterator(*this, 0); }
	iterator	end()					{ return iterator(*this, num); }
};

struct horizontal_split : Rect {
	typedef pair<int, Rect> element, reference;
	typedef indexed_iterator<horizontal_split&, int_iterator<int> > iterator;
	typedef random_access_iterator_t iterator_category;

	int		num;

	horizontal_split(const Rect &r, int _num) : Rect(r), num(_num) {}
	element		operator[](int i) const { return element(i, Subbox(Width() * i / num, 0, Width() * (i + 1) / num, 0)); }
	iterator	begin()					{ return iterator(*this, 0); }
	iterator	end()					{ return iterator(*this, num); }
};

void AddLabelControl(win::DialogBoxCreator &dc, const Rect &rect, int labelwidth, const char *label, const win::DialogBoxCreator::Control &c) {
	Rect	rlabel, rcontrol;
	rect.SplitAtX(labelwidth, rlabel, rcontrol);
	dc.AddControl(rlabel, DLG_STATIC, label);
	dc.AddControl(rcontrol, c);
}


struct HomeView : win::Dialog<HomeView> {
	LRESULT	Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_INITDIALOG:
				SetFocus(ID_EDIT);
				break;
			case WM_COMMAND: {
				switch (LOWORD(wParam)) {
					case IDOK:
						return EndDialog(1);
					case IDCANCEL:
						return EndDialog(0);
					default:
						if (HIWORD(wParam) == EN_CHANGE)
							Parent().SendMessage(message, wParam, lParam);
						break;
				}
				//if (HIWORD(wParam) != 0xffff)
				//	Parent().SendMessage(message, wParam, lParam);
				break;

			case WM_NCDESTROY:
				delete this;
				return 0;
			}
		}
		return FALSE;
	}

	HomeView(const win::WindowPos &pos) {

		win::DialogBoxCreator	dc(pos.Rect(), "Home", CHILD|VISIBLE);
		dc.SetFont(Font::Caption());
		dc.rect = dc.ScreenToDialog(dc.rect);

		for (auto i : vertical_split(dc.rect.Subbox(50, 0, 400, 400), 4)) {
			Rect	r = i.b.Subbox(0, 0, 0, 14);
			switch (i.a) {
				case 0: AddLabelControl(dc, r, 100, "Executable",		win::DialogBoxCreator::Control(DLG_EDIT,	"", ID_EDIT + 0, BORDER | EditControl::AUTOHSCROLL)); break;
				case 1: AddLabelControl(dc, r, 100, "Arguments",		win::DialogBoxCreator::Control(DLG_EDIT,	"", ID_EDIT + 1, BORDER | EditControl::AUTOHSCROLL)); break;
				case 2: AddLabelControl(dc, r, 100, "Working Directory", win::DialogBoxCreator::Control(DLG_EDIT,	"", ID_EDIT + 2, BORDER | EditControl::AUTOHSCROLL)); break;
				case 3: AddLabelControl(dc, r, 100, "Options",			win::DialogBoxCreator::Control(DLG_EDIT,	"", ID_EDIT + 3, BORDER | EditControl::AUTOHSCROLL)); break;
			}
		}
		Modeless(pos.parent, dc.GetTemplate());
		id = "Home";
	}
};

class OrbisCrude : public MainWindow {
public:
	static RecentFiles	recent;
	static int			num;

	filename	fn;
	string		exec_cmd;
	string		exec_dir;
	Menu		menu_select;

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	bool		OpenView(const ISO::Browser2 &b);
	void		SetFilename(const char *_fn);
	HomeView	*MakeHomeView() {
		auto	hv = new ::HomeView(GetChildWindowPos());
		hv->Item(ID_EDIT + 1).SetText(exec_cmd);
		hv->Item(ID_EDIT + 2).SetText(exec_dir);
		return hv;
	};
	OrbisCrude(const Rect &rect, bool licensed);
	~OrbisCrude();
};

int							OrbisCrude::num;
RecentFiles					OrbisCrude::recent;
multi_string_alloc<char>	MainWindow::load_filters = cmulti_string("ORBIScrude\0*.ib;*.ibz;*.ix\0");

OrbisCrude::OrbisCrude(const Rect &rect, bool licensed) {
	Create(WindowPos(0, rect), VersionString(buffer_accum<256>() << "ORBIScrude ver ", GetVersion(0)) << (licensed ? " (licensed)" : " (unlicensed)"),
		OVERLAPPED | SYSMENU | THICKFRAME | MINIMIZEBOX | MAXIMIZEBOX | CLIPCHILDREN,
		ACCEPTFILES
	);

	++num;

	exec_cmd	= GetSettings("ExecCommand").GetString();
	exec_dir	= GetSettings("ExecDir").GetString();

	CreateToolbar(IDR_TOOLBAR_START);
	MakeDropDown(ID_FILE_OPEN, recent);

	Menu	grab	= Menu(IDR_MENU_SUBMENUS).GetSubMenuByName("Grab");
	grab.SetStyle(MNS_NOTIFYBYPOS);
	MakeDropDown(ID_ORBISCRUDE_GRAB, grab);

	Rebind(this);
	SendMessage(WM_SETICON, 0, Icon::Load(IDI_ISOPOD));
#if 0
	LoadLibraryA("RICHED20.DLL");
	Resource		r(0, "ORBISCRUDE.RTF", "BIN");
	RichEditControl	re(*this, 0, CHILD | VISIBLE | EditControl::MULTILINE, 0, GetChildRect());
	re.SendMessage(EM_EXLIMITTEXT, 0, ~0);
	re.SetText2(r);
	re.SetMargins(100, 100);
	SetChild(re);
#else
	SetChild(*MakeHomeView());
#endif
	Show(SW_SHOW);
}

OrbisCrude::~OrbisCrude() {
	GetSettings("ExecCommand").Set(exec_cmd);
	GetSettings("ExecDir").Set(exec_dir);
}

void OrbisCrude::SetFilename(const char *_fn) {
	fn = _fn;
	recent.add(_fn);
	SetTitle(fn.name());
}


#if 0
struct MenuIntercept : Subclass<MenuIntercept, Control> {
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		ISO_TRACEF("message=") << hex(message) << '\n';
		switch (message) {
			case WM_RBUTTONDOWN:
				ISO_TRACEF("DOWN!\n");
				break;
			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}
	MenuIntercept(HWND hWnd) : Subclass<MenuIntercept, Control>(hWnd) {}
};
#endif

LRESULT OrbisCrude::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	static UINT WM_TASKBARBUTTONCREATED = RegisterWindowMessage("TaskbarButtonCreated");

	switch (message) {
		case WM_CREATE:
			//BlurBehind(true);
			break;//return 0;

		case WM_DROPFILES: {
			Busy		bee;
			filename	fn;
			HDROP		hDrop	= (HDROP)wParam;
			DragQueryFile(hDrop, 0, fn, sizeof(fn));
			if (FileHandler *fh = FileHandler::Get(fn.ext())) {
				try {
					if (ISO_ptr<void> p = FileHandler::CachedRead(fn)) {
						if (OpenView(ISO::Browser2(p))) {
							SetTitle(fn.name());
						} else {
							MessageBox(*this, format_string("Cannot display %s", (const char*)fn), "Load error", MB_ICONERROR|MB_OK);
						}
					} else {
						MessageBox(*this, format_string("Failed to load %s", (const char*)fn), "Load error", MB_ICONERROR|MB_OK);
					}
				} catch (string &s) {
					MessageBox(*this, s, "Load error", MB_ICONERROR|MB_OK);
				}
			} else {
				MessageBox(*this, format_string("No handler for %s files", (const char*)fn.ext()), "Load error", MB_ICONERROR|MB_OK);
			}
			break;
		}

		case WM_INITMENUPOPUP: {
			Menu	menu((HMENU)wParam);
			int		i = LOWORD(lParam);
			ISO_OUTPUTF("popup") << i << ' ' << menu << '\n';
			if (menu_select && menu_select.GetSubMenuByPos(LOWORD(lParam)) == menu) {
				ISO_OUTPUTF("popup got\n");
				fixed_string<256>	name;
				Menu::Item			item(MIIM_DATA);
				if (item.Text(name, 256)._GetByPos(menu_select, i) && (void*)item.Param())
					(*(MenuCallback*)item.Param())(*this, menu);
			}
			break;
		}

		case WM_MENUSELECT:
			if (lParam && ((HIWORD(wParam) & MF_POPUP))) {
				menu_select = (HMENU)lParam;
				//Menu				menu((HMENU)lParam);
				//fixed_string<256>	name;
				//Menu::Item			item(MIIM_DATA);
				//if (item.Text(name, 256)._GetByPos(menu, LOWORD(wParam)) && (void*)item.Param())
				//	(*(MenuCallback*)item.Param())(*this, menu.GetSubMenuByPos(LOWORD(wParam)));
			}
			break;

		case WM_MENUCOMMAND : {
			Menu	menu((HMENU)lParam);
			fixed_string<256>	name;
			Menu::Item	item(MIIM_ID | MIIM_DATA);
			if (item.Text(name, 256)._GetByPos(menu, wParam) && (void*)item.Param()) {
				(*(MenuItemCallback*)item.Param())(*this, item);
				break;
			}
			wParam = item.ID();
		}
		case WM_COMMAND:
			switch (uint16 id = LOWORD(wParam)) {
				case ID_ORBISCRUDE_HOME:
					GetTabs()->AddItemControl(*MakeHomeView());
					return 1;

				case ID_FILE_OPEN:
					if (HIWORD(wParam) == 2) {
						fn = (const char*)lParam;
					} else if (!GetOpen(*this, fn, "Open File", *load_filters.begin())) {
						return 1;
					}
					{
						Busy	bee;
						if (auto p = FileHandler::Read64(tag(), fn)) {
							recent.add(fn);
							OpenView(ISO::Browser2(p));
							SetTitle(fn.name());
							return 1;
						}
					}
					MessageBox(*this, format_string("Cannot load %s", (const char*)fn), "ORBIScrude", MB_ICONERROR|MB_OK);
					return 1;

				case ID_APP_EXIT:
					Destroy();
					return 1;

				case ID_ORBISCRUDE_SETTINGS:
					Settings(*this);
					return 1;

				case ID_ORBISCRUDE_LICENSE:
					ShellExecute(NULL, "open", "http://www.orbiscrude.com/license.htm", NULL, NULL, SW_SHOW);
					return 1;

				case ID_ORBISCRUDE_HELP:
					ShellExecute(NULL, "open", "http://www.orbiscrude.com/help2.htm", NULL, NULL, SW_SHOW);
					return 1;

				case ID_EDIT + 1:
					exec_cmd = Control(lParam).GetText();
					break;

				case ID_EDIT + 2:
					exec_dir = Control(lParam).GetText();
					break;

				default:
					if (id && Editor::CommandAll(*this, id))
						return 1;
					break;
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
			#if 0
				case NM_LDOWN: {
					NMMOUSE			*nmm	= (NMMOUSE*)nmh;
					ToolBarControl	tb(nmh->hwndFrom);
					if (nmm->pt.x < tb.GetItemRectByID(nmm->dwItemSpec).Left() + tb.ButtonSize().x) {
						if (Editor::CommandAll(*this, nmm->dwItemSpec))
							return 1;
					}
					break;
				}
			#endif
				case TCN_DRAG:
					if (nmh->hwndFrom == child) {
						TabControl2		tab(nmh->hwndFrom);
						int				id	= nmh->idFrom;
						Control			c	= tab.GetItemControl(id);

						auto	*win = new OrbisCrude(c.GetRect(), true);
						win->SetChild(c);

						tab.RemoveItem(id);
						if (tab.Count() == 1 && child == tab)
							SetChild(tab.GetItemControl(0));
						win->StartDrag();
						return 1;
					}
					break;
			}
			break;
		}

		case WM_MOVING:
			return 0;

		case WM_EXITSIZEMOVE: {
			Rect	rect = GetRect();
			ISO::root("settings").Parse("General/window").Set((char*)format_string<256>("%i,%i,%i,%i", rect.Left(), rect.Top(), rect.Width(), rect.Height()));
			ISO::root("settings").Update("General/window");
			return 0;
		}

		case WM_ISO_JOB:
			job((void*)wParam, (void(*)(void*))lParam)();
			return 0;

		case WM_NCDESTROY:
			if (--num == 0)
				PostQuitMessage(0);
			delete this;
			return 0;

		default:
			break;
	}
	return Super::Proc(message, wParam, lParam);
}

bool OrbisCrude::OpenView(const ISO::Browser2 &b) {
	ISO::Browser2	b2 = b;
	if (Editor *ed = Editor::Find(b2)) {
		ed->Create(*this, Dock(DOCK_TAB), b2.GetPtr());
		return true;
		//if (Control c = ed->Create(*this, GetChildWindowPos(), b2.GetPtr())) {
		//if (Control c = ed->Create(*this, Dock(DOCK_TAB), b2.GetPtr())) {
		//	AddView(c);
		//	return true;
		//}
	}
	return false;
}

//-----------------------------------------------------------------------------
//	Editor
//-----------------------------------------------------------------------------

Editor *Editor::Find(ISO::Browser2 &b) {
	for (;;) {
		if (b.HasCRCType())
			return 0;

		for (iterator i = begin(); i != end(); ++i) {
			if (i->Matches(b))
				return i;
		}

		if (b.SkipUser().GetType() == ISO::VIRTUAL && b.Count() == 1)
			b = b[0];

		else if (b.SkipUser().GetType() == ISO::REFERENCE)
			b = *b;

		else
			return 0;
	}
}

bool Editor::CommandAll(MainWindow &main, ID id) {
	for (iterator i = begin(); i != end(); ++i) {
		if (i->Command(main, id))
			return true;
	}
	return false;
}

void MainWindow::SetFilename(const char *fn) {
	static_cast<OrbisCrude*>(this)->SetFilename(fn);
}

//-----------------------------------------------------------------------------
//	Update
//-----------------------------------------------------------------------------

static const char aes_key[] = "use - ORBIScrude";
/*
class UpdateThread : public Thread, HTTP {
	uint64		current_ver;
	filename	new_exe;
public:
	int	operator()() {
		Socket			sock = Connect();
		HTTPinput		inp(Request(sock.s, "GET", "downloads/version.txt"));
		if (inp.exists()) {
			streamptr		len	= inp.length();
			malloc_block	mb(inp, len);

			uint16			v[4];
			(((string_scan(mb) >> v[0]).move(1) >> v[1]).move(1) >> v[2]).move(1) >> v[3];
			uint64			ver	= (uint64((v[0] << 16) | v[1]) << 32) | (v[2] << 16) | v[3];

			if (ver > current_ver) {
				CBC_decrypt<AES_decrypt, 16, HTTPinput>	aes(const_memory_block(aes_key), Request(sock.s, "GET", "downloads/ORBIScrude.bin"));
				stream_copy<1024>(FileOutput(new_exe), aes);
			}
		}
		delete this;
		return 0;
	}
	UpdateThread(const char *url, uint64 ver, const filename &path) : Thread(this), HTTP("Orbiscrude", url), current_ver(ver), new_exe(path) {
		Start();
	}
};
*/
bool CheckForUpdates(char *cmdline) {
	filename	cur_exe	= get_exec_path();
	filename	new_exe	= filename(cur_exe).relative("new.exe");
	uint64		ver		= GetVersion(0);

	if (exists(new_exe)) {
		uint64	new_ver		= GetVersion(new_exe);
		if (new_ver > ver) {
			buffer_accum<256>	ren_exe, ren_elf;
			VersionString(ren_exe << "ORBIScrude.", ver) << ".exe";

			buffer_accum<256>	message;
			VersionString(message << "Version ", new_ver) << " has been downloaded.\n\n"
				<< "Would you like to switch to this version?\n"
				<< "(the current executable will be renamed to " << ren_exe << ")";

			if (MessageBox(NULL, message.term(), "ORBIScrude", MB_ICONQUESTION | MB_YESNO) == IDYES) {
				rename(cur_exe, filename(cur_exe).relative(ren_exe));
				rename(new_exe, cur_exe);

				PROCESS_INFORMATION	pi;
				STARTUPINFO			sui = {sizeof(STARTUPINFO), 0};

				return !!CreateProcess(
					NULL,
					string(cur_exe) + " " + cmdline,
					NULL,	//LPSECURITY_ATTRIBUTES lpProcessAttributes,
					NULL,	//LPSECURITY_ATTRIBUTES lpThreadAttributes,
					FALSE,	//BOOL bInheritHandles,
					0,		//DWORD dwCreationFlags,
					NULL,	//LPVOID lpEnvironment,
					NULL,	//LPCTSTR lpCurrentDirectory,
					&sui,	//LPSTARTUPINFO lpStartupInfo,
					&pi		//LPPROCESS_INFORMATION lpProcessInformation
				);
			}
		}
	}

	RunThread([ver, new_exe]() {
		HTTP			http("Orbiscrude", "http://www.orbiscrude.com");
		SOCKET			sock = http.Connect();
		HTTPinput		inp(http.Request(sock, "GET", "downloads/version.txt"));
		if (inp.exists()) {
			if (streamptr len = inp.length()) {
				malloc_block	mb(inp, len);

				uint16			v[4];
				(((string_scan(mb, mb.end()) >> v[0]).move(1) >> v[1]).move(1) >> v[2]).move(1) >> v[3];
				uint64			new_ver	= (uint64((v[0] << 16) | v[1]) << 32) | (v[2] << 16) | v[3];

				if (new_ver > ver) {
					CBC_decrypt<AES_decrypt, 16, HTTPinput>	aes(const_memory_block(aes_key), http.Request(sock, "GET", "downloads/ORBIScrude.bin"));
					stream_copy<1024>(FileOutput(new_exe), aes);
				}
			}
		}
	});

//	new UpdateThread("http://www.orbiscrude.com", ver, new_exe);
	return false;
}
//-----------------------------------------------------------------------------
//	Resources
//-----------------------------------------------------------------------------

class Resources : public ISO::VirtualDefaults {
	string		type;
	anything	found;
public:
	Resources(const char *_type) : type(_type)	{}
	~Resources()								{ found.Clear();		}
	int				Count()						{ return found.Count();	}
	tag				GetName(int i = 0)			{ return found[i].ID();	}
	ISO::Browser2	Index(int i)				{ return found[i];		}
	void			Delete()					{ this->~Resources();	}

	int			GetIndex(tag id, int from) {
		for (int i = from, n = found.Count(); i < n; i++) {
			if (found[i].ID() == (const char*)id)
				return i;
		}
		fixed_string<256>	name = id;
		char	*ext = name + strlen(name);
		*ext++ = '.';
		//iterate through known extensions
		for (FileHandler::iterator i = FileHandler::begin(); i != FileHandler::end(); ++i) {
			if (const char *ext2 = i->GetExt()) {
				strcpy(ext, ext2);
				if (Resource r = Resource(NULL, name, type)) {
					found.Append(i->Read(id, memory_reader(r)));
					return found.Count() - 1;
				}
			}
		}
		return -1;
	}
};

template<> struct ISO::def<Resources> : public ISO::VirtualT<Resources> {};

//-----------------------------------------------------------------------------
//	WinMain
//-----------------------------------------------------------------------------

ISO_ptr<void> MakeRegistryBacked(const ISO::Browser2 &b, HKEY h);

LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS *ep) {
#if 0
	DateTime	now			= DateTime::Now();
	Date		day			= now.Days();
	TimeOfDay	time		= now.TimeOfDay();
	filename	fn			= get_exec_path();
	fn.format(".%02d%02d%04d_%02d%02d%02d.dmp",
		day.day, day.month, day.year,
		time.Hour(), time.Min(), int(time.Sec())
	);
#else
	filename	fn			= str(get_exec_path() << '.' << ISO_8601(DateTime::Now(), ISO_8601::MINIMAL) << ".dmp");
#endif

	buffer_accum<256>	ba;
	ba << get_exec_path().name() << " has crashed at " << hex(uint64(ep->ExceptionRecord->ExceptionAddress)) << " with exception 0x" << hex(ep->ExceptionRecord->ExceptionCode) << ".\n\n";
	if (MiniDump(ep, fn)) {
		ba << "A minidump was saved at " << fn << ".\nPlease email it to bugs@orbiscrude.com.\n\n";
	} else {
		ba << "Failed to create a minidump at " << fn << " with error code " << GetLastError() << "\n\n";
	}

	if (MessageBox(NULL, (ba << "Would you like to debug?").term(), "Crash", MB_ICONERROR | MB_YESNO) == IDYES) {
		SetUnhandledExceptionFilter(0);

	//	switch (ep->ExceptionRecord->ExceptionCode) {
	//		case EXCEPTION_SINGLE_STEP:
	//			ep->ContextRecord->Eip++;
	//			break;
	//	}
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

ISO::Browser2 app::GetSettings(const char *path) {
	static ISO::PtrBrowser	settings(ISO::root("settings"));
	return settings.Parse(path);
}

void iso::JobQueueMain::put(const job &j) {
	if (auto *main = MainWindow::Get())
		main->PostMessage(WM_ISO_JOB, j.me, j.f);
	else
		j();
}

#if 1
#define FORCE_UNDEFINED_SYMBOL(x) extern void x(); void* __ ## x ## _fp =(void*)&x;
//FORCE_UNDEFINED_SYMBOL(ps3_dummy);
//FORCE_UNDEFINED_SYMBOL(ps4_dummy);
FORCE_UNDEFINED_SYMBOL(dx11_dummy);
FORCE_UNDEFINED_SYMBOL(dx12_dummy);
//FORCE_UNDEFINED_SYMBOL(wii_dummy);
//FORCE_UNDEFINED_SYMBOL(xone_dummy);
FORCE_UNDEFINED_SYMBOL(x64_dummy);
FORCE_UNDEFINED_SYMBOL(dxbc_dis_dummy);
//FORCE_UNDEFINED_SYMBOL(isa_dis_dummy);
FORCE_UNDEFINED_SYMBOL(intel_dis_dummy);
#endif

D2DTextWindow	*console;
#if 0
void CreateReadPipe(HANDLE pipe) {
	HANDLE				read_pipe;
	SECURITY_ATTRIBUTES	sec;
	sec.nLength					= sizeof(sec);
	sec.lpSecurityDescriptor	= 0;
	sec.bInheritHandle			= TRUE;

	if (CreatePipe(&read_pipe, &pipe, &sec, 1024)) {
		SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

		RunThread([read_pipe]() {
			char	buffer[1024];
			DWORD	read;
			while (ReadFile(read_pipe, buffer, 1023, &read, NULL)) {
				buffer_accum<1024>	ba;
				ba << ansi_colour("91") << str(buffer, read) << ansi_colour("0");
				//buffer[read] = 0;
				OutputDebugStringA(ba.term());
			}
			CloseHandle(read_pipe);
		});
	}
}
#endif

void WINAPI Hooked_OutputDebugStringA(LPCSTR lpOutputString) {
	if (console) {
//		JobQueue::Main().add([s = string(lpOutputString)]() {
			AddTextANSI(*console, string(lpOutputString));
			console->EnsureVisible();
//		});
	}
	if (IsDebuggerPresent())
		get_orig(OutputDebugStringA)(lpOutputString);
}

void WINAPI Hooked_OutputDebugStringW(LPCWSTR lpOutputString) {
	if (console) {
		AddTextANSI(*console, lpOutputString);
		console->EnsureVisible();
	}
	if (IsDebuggerPresent())
		get_orig(OutputDebugStringW)(lpOutputString);
}


template<typename T> struct maybe_owned : pointer_pair<T, bool, 2> {
	maybe_owned(T *p, bool own) : pointer_pair<T, bool, 2>(p, own) {}
	~maybe_owned() {
		if (b)
			free((void*)a.get());
	}
	//T*		detach()	{ return b ? exchange(B::p, nullptr).a.get() : strdup(B::p); }
};

void warshall_shortest_path(block<float, 2> w) {
	int		V	= w.size();
	for (int k = 0; k < V; ++k) {
		auto	wk = w[k];
		for (int i = 0; i < V; ++i) {
			if (i != k) {
				auto	wi	= w[i];
				auto	wik	= wi[k];
				for (int j = 0; j < V; ++j)
					wi[j] = min(wi[j], wik + wk[j]);
			}
		}
	}
}

auto_block<int,2> warshall_shortest_path_path(block<float, 2> w) {
	int		V		= w.size();
	auto	next	= make_auto_block<int>(V, V);
	
	for (int i = 0; i < V; ++i)
		for (int j = 0; j < V; ++j)
			next[i][j] = w[i][j] ? j : -1;

	for (int k = 0; k < V; ++k) {
		auto	wk = w[k];
		for (int i = 0; i < V; ++i) {
			if (i != k) {
				auto	wi = w[i];
				auto	wik	= wi[k];
				auto	ni	= next[i];
				for (int j = 0; j < V; ++j) {
					if (wi[j] > wik + wk[j]) {
						wi[j] = wik + wk[j];
						ni[j] = ni[k];
					}
				}
			}
		}
	}
	return next;
}

dynamic_array<int> get_path(const block<int,2> &next, int u, int v) {
	dynamic_array<int>	path;
	if (next[u][v] != -1) {
		path.push_back(u);
		while (u != v) {
			u = next[u][v];
			path.push_back(u);
		}
	}
	return path;
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *cmdline, int show) {
	OleInitialize(NULL);
	socket_init();
	AppEvent(AppEvent::PRE_GRAPHICS).send();

	RunThread([]() {
		char	buffer[1024];
		for (;;) {
			IP4::socket_addr	addr(PORT(4568));
			Socket				sock = IP4::UDP();
			sock.options().reuse_addr();
			addr.bind(sock);

			int	n = addr.recv(sock, buffer, 1024);
			if (n < 0)
				break;
			ISO_TRACEF(str(buffer, n)) << "@" << addr.ip << ':' << addr.port << '\n';
		}
	});

	{
		//broadcast on port 4567
		IP4::socket_addr	addr(IP4::broadcast, 4567);
		Socket				sock = IP4::UDP();
		sock.options().broadcast(1);
		addr.send(sock, "host");
	}

//	RunThread([]() {
		auto *con = new DockableWindow(Rect(0,0,200,200), "OC console");
		console = new D2DTextWindow(con->GetChildWindowPos()
			, "OC console"
			, Control::CHILD | Control::VISIBLE
		);
		con->Show();
//	});

//	hook(OutputDebugStringA, "kernel32.dll");
//	hook(OutputDebugStringW, "kernel32.dll");

	ApplyHooks();
	SetUnhandledExceptionFilter(ExceptionFilter);

#if	_WIN32_WINNT >= 0x0A00
	SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
#else
	SetProcessDPIAware();
#endif

	uint32	check = 1;
	if (!RegKey(HKEY_CURRENT_USER, "Software\\Isopod\\OrbisCrude\\General").values()["check for updates"].get(check) || check) {
		if (CheckForUpdates(cmdline))
			return 0;
	}

//	OleInitialize(0);
	InitCommonControls();
	init_com();

//	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_DELAY_FREE_MEM_DF);

	ISO::root().Add(ISO_ptr<anything>("externals"));
	ISO_ptr<anything>	variables("variables");
	ISO::root().Add(variables);
	ISO::root().Add(ISO_ptr<Resources>("data", "ISOPOD"));

	ISO::root().Add(MakeRegistryBacked(ISO::root("data")["settings"], RegKey(HKEY_CURRENT_USER, "Software\\Isopod\\OrbisCrude", KEY_ALL_ACCESS).detach()));

	if (const char *f = ISO::root("settings").Parse("Paths/custom_types").GetString()) {
		try {
			if (!exists(f))
				throw_accum(str("Custom type file \"") << f << "\" not found");
			ReadCTypes(FileInput(f), user_ctypes);
		} catch (const char *error) {
			MessageBox(NULL, error, "Custom Type Error", MB_ICONERROR | MB_OK);
		}
	}

#if 0
	for (auto i = directories(*GetSettings("Paths/plugin_path"), "*.ipi"); i; ++i) {
		filename	fn = i;
		ISO_TRACEF("Loading plugin ") << fn << '\n';
		Win32ErrorCheck(LoadLibrary(fn), (const char*)fn);
	}
#else
	if (ISO_openarray<ISO::ptr_string<char,32>> *plugins = GetSettings("Paths/plugin_path")) {
		for (auto i = directories(*plugins, "*.ipi"); i; ++i) {
			filename	fn = i;
			ISO_TRACEF("Loading plugin ") << fn << '\n';
			Win32ErrorCheck(LoadLibrary(fn), (const char*)fn);
		}
	}
#endif
	for (auto &i : Plugin::all())
		ISO_TRACEF(i.GetDescription()) << '\n';

	filename	license = get_exec_dir().add_dir("license.bin");
	bool		licensed = false;
	if (exists(license)) try {
		FileHandler		*ix	= FileHandler::Get("ix");
		ISO::Browser2	b	= ix->Read(tag(), FileInput(license));
		licensed			= iso::crc32(buffer_accum<256>() << "ORBIScrude License;" << b["name"].GetString() << ";" << b["email"].GetString() << ";" << b["id"].GetString()) == b["hash"].GetInt();
//		licensed			= crc32(buffer_accum<256>() << b["type"].GetString() << ";" << b["name"].GetString() << ";" << b["email"].GetString() << ";" << b["id"].GetString()) == b["hash"].GetInt();
	} catch(...) {}


	Rect	rect(CW_USEDEFAULT, CW_USEDEFAULT, 1024, 640);
	if (const char *f = ISO::root("settings").Parse("General/window").GetString()) {
		int	x, y, w, h;
		if (sscanf(f, "%i,%i,%i,%i", &x, &y, &w, &h) == 4)
			rect = Rect(x, y, w, h);
	}

	for (auto i : GetSettings("Recent"))
		OrbisCrude::recent.add_nocheck(i.GetString());

	OrbisCrude	*main = new OrbisCrude(rect, licensed);

	main->Dock(DOCK_BOTTOM, *console);
	con->Destroy();
	AppEvent(AppEvent::BEGIN).send();

	while (ProcessMessage(true))
		;

	{
		auto	b = GetSettings("Recent");
		b.Resize(0);
		for (auto i : OrbisCrude::recent.Items())
			b.Append().Set(i.Text());
	}
	
	AppEvent(AppEvent::END).send();
	return 0;
}
