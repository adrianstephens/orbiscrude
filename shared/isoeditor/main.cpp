#include "base/array.h"
#include "maths/geometry.h"
#include "main.h"
#include "windows/control_helpers.h"
#include "iso/iso_files.h"
#include "iso/iso_script.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"

#include "directory.h"
#include "graphics.h"
#include "shader.h"
#include "thread.h"
#include "plugin.h"
#include "vm.h"

#include "filetypes/bitmap/bitmap.h"
#include "filetypes/container/archive_help.h"
#include "systems/conversion/platformdata.h"
#include "systems/communication/connection.h"
#include "devices/device.h"
#include "windows/dib.h"
#include "windows/filedialogs.h"
#include "extra/date.h"
#include "hook.h"
#include "bin.h"
//#include "Composition.h"

#include "maths/polynomial.h"
#include "speech/tts.h"

#include <ShellScalingApi.h>
#include <shellapi.h>
#include <sapi.h>
#include <Ole2.h>
#include <d3dcompiler.h>

#include "comms/WebSocket.h"

#ifdef USE_VLD
#include "vld.h"
#endif

#ifndef __clang__
#pragma comment(linker,\
"\"/manifestdependency:\
type='win32' \
name='Microsoft.Windows.Common-Controls' \
version='6.0.0.0' \
processorArchitecture='*' \
publicKeyToken='6595b64144ccf1df' \
language='*'\"")
#endif

using namespace app;

extern Control			MakeTTYViewer(const WindowPos &wpos);
extern Control			MakeIXEditor(MainWindow &main, const WindowPos &wpos, ISO_VirtualTarget &v);
extern bool				MakeTextEditor(MainWindow &main, const WindowPos &wpos, Control *c);
extern Control			MakeFinderWindow(MainWindow &main, const ISO::Browser2 &b, const char *route);
extern Control			MakeCompareWindow(MainWindow &main, const ISO::Browser2 &b1, const ISO::Browser2 &b2);

extern int				UsableClipboardContents();
extern ISO_ptr<void>	GetClipboardContents(tag2 id, HWND hWnd);

struct browser_pointer : ISO::VirtualDefaults {
	const ISO::Browser2	b;
	browser_pointer(const ISO::Browser2 &_b) : b(_b) {}
	ISO::Browser2	Deref()	{ return b; }
};

ISO_DEFVIRT(browser_pointer);

void iso::JobQueueMain::put(const job &j) {
	if (auto *main = MainWindow::Get())
		main->PostMessage(WM_ISO_JOB, j.me, j.f);
	else
		j();
}

//-----------------------------------------------------------------------------
//	Misc
//-----------------------------------------------------------------------------

namespace app {
UINT			_CF_HTML;
}

ISO::Browser	clipdata;

namespace iso {

_Texture*	_MakeTexture(bitmap *bm);
DXGI_FORMAT GetDXGI(const ISO::Type *type);

ID3D11ShaderResourceView *_MakeDataBuffer(const ISO::Type *type, void *data, uint32 count) {
	return graphics.MakeDataBuffer((TexFormat)GetDXGI(type), type->GetSize(), data, count);
}

#ifdef ISO_PTR64

ID3D11ShaderResourceView *get_safe(const DXwrapper<ID3D11ShaderResourceView,64> *x) {
	if (ISO::binary_data.GetMode())
		return 0;
	if (auto& p1 = (ISO_ptr<void,64>&)x->p) {
		_Texture	*&t = *x->write();
		if (!t) {
			if (p1.IsType("DataBuffer")) {
				ISO::Browser	b(p1);
				if (t = _MakeDataBuffer(b[0].GetTypeDef(), b[0], b.Count()))
					p1.Header()->addref();

			} else if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(FileHandler::ExpandExternals(p1), ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS)) {
				t = _MakeTexture(bm.get());
				p1.Header()->addref();
			}
		}
		return t;
	}
	return 0;
}

#else

template<> _Texture *DXwrapper<_Texture>::safe() const {
	if (ISO::binary_data.GetMode())
		return 0;
	if (ISO_ptr<void> &p1 = (ISO_ptr<void>&)p) {
		_Texture	*&t = *write();
		if (!t && (t = MakeTexture(ISO_conversion::convert<bitmap>(FileHandler::ExpandExternals(p1), ISO_conversion::FULL_CHECK))))
			p1.Header()->addref();
		return t;
	}
	return 0;
}

#endif
} // namespace iso

namespace app {

ImageList			tree_imagelist;
map<uint32, int>	tree_imagemap;

int AddTreeIcon(ID id) {
	auto &i = tree_imagemap[id.IsOrdinal() ? id.i : CRC32::calc(id.s)];
	if (!i)
		i = tree_imagelist.ScaleAdd(LoadPNG(id));
	return i;
}

int GetTreeIcon(const ISO::Browser2 &b0) {
	if (b0.External())
		return AddTreeIcon("IDB_EXTERNAL");

	ISO::Browser2 b = b0;
	if (Editor *e = Editor::Find(b)) {
		if (ID id = e->GetIcon(b))
			return AddTreeIcon(id);
	}
	return 0;
}

RegKey	Settings(bool write) {
	return RegKey(HKEY_CURRENT_USER, "Software\\Isopod\\IsoEditor", write ? KEY_ALL_ACCESS : KEY_READ);
}

Menu GetTypesMenu(int id) {
	Menu	menu	= Menu::Create();
	int		y		= 0;
	int		dy		= win::GetMenuSize().y;
	int		maxy	= Control::Desktop().GetClientRect().Height();
	for (FileHandler::iterator i = FileHandler::begin(); i; ++i) {
		if (const char *desc = i->GetDescription()) {
			y += dy;
			Menu::Item().Text(desc).ID(id).Param(i).Type(y > maxy ? MFT_MENUBARBREAK : 0).AppendTo(menu);
			if (y > maxy)
				y = 0;
		}
	}
	return menu;
}

Menu GetTypesMenu(Menu menu, dynamic_array<pair<int,int>> &&items, int id, int did) {
	sort(items, [](const pair<int,int> &a, const pair<int,int> &b) {
		if (a.a != b.a)
			return a.a > b.a;

		FileHandler	*fha	= FileHandler::index(a.b);
		FileHandler	*fhb	= FileHandler::index(b.b);
		return str(fha->GetDescription()) < fhb->GetDescription();
	});

	int	y		= 0;
	int	dy		= win::GetMenuSize().y;
	int	maxy	= Control::Desktop().GetClientRect().Height();
	int	prev	= -1000;

	int	mincheck	= FileHandler::CHECK_UNLIKELY;
	static const char *confidence[] = {
		"UNLIKELY",
		"NO OPINION",
		"POSSIBLE",
		"PROBABLE",
	};

	for (auto &i : items) {
		if (i.a < mincheck)
			break;
		if (i.a != prev) {
			if (&i != items.begin())
				menu.Separator();
			Menu::Item().Text(confidence[i.a + 1]).AppendTo(menu);
		}
		prev = i.a;
		y	+= dy;
		FileHandler	*fh	= FileHandler::index(i.b);
		Menu::Item().Text(fh->GetDescription()).ID(id + i.b * did).Param(fh).Type(y > maxy ? MFT_MENUBARBREAK : 0).AppendTo(menu);
		if (y > maxy)
			y = 0;
	}
	return menu;
}

dynamic_array<pair<int,int>> GetTypes(Menu menu, istream_ref file, const char *ext) {
	dynamic_array<pair<int,int>>	items;

	if (file.exists()) {
		size_t		n		= distance(FileHandler::begin(), FileHandler::end());
		int			index	= 0;
		items.reserve(n);

		if (ext)
			ext += ext[0] == '.';

		for (FileHandler::iterator i = FileHandler::begin(); i; ++i, ++index) {
			if (const char *desc = i->GetDescription()) {
				int	c = i->Check(file);
				if (c > FileHandler::CHECK_DEFINITE_NO) {
					if (c == FileHandler::CHECK_NO_OPINION && ext && str(ext) == i->GetExt())
						c = FileHandler::CHECK_PROBABLE;
					items.emplace_back(c, index);
				}
			}
		}
	}

	return items;
}

dynamic_array<pair<int,int>> GetTypes(Menu menu, const filename &fn) {
	dynamic_array<pair<int,int>>	items;

	size_t		n		= distance(FileHandler::begin(), FileHandler::end());
	int			index	= 0;
	items.reserve(n);

	for (FileHandler::iterator i = FileHandler::begin(); i; ++i, ++index) {
		if (const char *desc = i->GetDescription()) {
			int	c = i->Check(fn);
			if (c > FileHandler::CHECK_DEFINITE_NO)
				items.emplace_back(c, index);
		}
	}
	return items;
}


Menu GetTypesMenu(Menu menu, istream_ref file, const char *ext, int id, int did) {
	return GetTypesMenu(menu, GetTypes(menu, file, ext), id, did);
}

Menu GetTypesMenu(Menu menu, const filename &fn, int id, int did) {
	return GetTypesMenu(menu, GetTypes(menu, fn), id, did);
}

FileHandler *GetFileHandlerFromMenu(filename &fn, HWND hWnd, const Point &pt) {
	if (int id = GetTypesMenu(Menu::Popup(), FileInput(fn), fn.ext(), 1, 0).Track(hWnd, pt, TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON))
		return FileHandler::index(id - 1);
	return 0;
}

struct SendToMenu : MenuCallbackT<SendToMenu> {
	bool	ready;
	void	operator()(Control c, Menu m) {
		if (ready) {
			ready = false;
			while (m.RemoveByPos(0));

			RunThread([this, m]() {
				init_com();

				string	targets = RegKey(HKEY_CURRENT_USER, "Software\\Isopod\\IsoLink").values()["targets"];
				for (char *p = targets.begin(), *n; p; p = n) {
					if (n = strchr(p, ';'))
						*n++ = 0;
					m.Append(p, ID_ENTITY_UPLOAD);
				}

				auto	lambda = [m](const char *target, isolink_platform_t platform) { 
					Menu::Item(target, ID_ENTITY_UPLOAD).Param(platform).AppendTo(m);
					return false;
				};
				isolink_enum_hosts(callback_function_end<bool(const char*, const char*)>(&lambda), ISOLINK_ENUM_PLATFORMS, &lambda);
				isolink_enum_hosts(callback_function_end<bool(const char*, const char*)>(&lambda), ISOLINK_ENUM_DEVICES, &lambda);
				//Menu::Item("Enumerate All").Param((MenuCallback*)this).SubMenu(Menu::Create()).Append(menu);
				ready = true;
			});
		}
	}
	SendToMenu() : ready(true)	{}
} sendto_menu;

}

//-----------------------------------------------------------------------------
//	Resources
//-----------------------------------------------------------------------------

class Resources : public ISO::VirtualDefaults {
	const Module::ResourceDir	*type;
	anything					found;
public:
	Resources(const char *_type) {
		Module		mod = Module::current();
		auto		res = mod.Resources();
		for (auto &i : res->entries()) {
			if (i.Name(res) == _type) {
				type = i.SubDir(res);
				break;
			}
		}
	}

	~Resources()						{ found.Clear();		}
	int				Count()				{ return found.Count();	}
	tag				GetName(int i = 0)	{ return found[i].ID();	}
	ISO::Browser2	Index(int i)		{ return found[i];		}

	int				GetIndex(tag id, int from) {
		int	i = found.GetIndex(id, from);
		if (i >= 0)
			return i;
		Module		mod = Module::current();
		auto		res = mod.Resources();
		for (auto &j : type->entries()) {
			auto	name = j.Name(res);
			if (name.begins(id) && name[int(strlen(id))] == '.') {
				if (FileHandler::iterator f = FileHandler::Get(string(name.slice(int(strlen(id)))))) {
					auto	p = &j;
					while (auto dir = p->SubDir(res))
						p = &dir->entries()[0];
					found.Append(f->Read(id, lvalue(memory_reader(p->Data(mod, res)))));
					return found.Count() - 1;
				}
			}
		}

		return -1;
	}
};

ISO_DEFVIRT(Resources);

//-----------------------------------------------------------------------------
//	TabSplitter
//-----------------------------------------------------------------------------

class TabSplitter : public SplitterWindow {
public:
	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_PARENTNOTIFY: {
				if (wParam == WM_DESTROY) {
					Control	p	= Parent();
					int		w	= WhichPane(lParam);
					p(message, wParam, lParam);
					if (w >= 0) {
						if (SplitterWindow *s = SplitterWindow::Cast(p))
							s->SetPane(s->WhichPane(*this), GetPane(1 - w));
						Destroy();
					}
				}
				break;
			}
			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return SplitterWindow::Proc(message, wParam, lParam);
	}

	TabSplitter(const WindowPos &pos, int flags, Style style = CHILD | VISIBLE, StyleEx exstyle = NOEX) : SplitterWindow(flags) {
		Create(pos, NULL, style, exstyle);
		Rebind(this);
	}
};

//-----------------------------------------------------------------------------
//	TreeColumnDisplay
//-----------------------------------------------------------------------------

struct temp_output : memory_writer {
	fixed_accum &a;
	temp_output(fixed_accum &a) : memory_writer(a.remainder()), a(a) {}
	~temp_output() {
		a.move(tell32());
	}
	fixed_accum &_clone()	{ return a; }
};

bool TreeColumnDisplay::Display(const ISO::Browser2 &b0, ISOTree tree, NMTCCDISPINFO *nmdi) {
	if (!(nmdi->item.mask & TVIF_TEXT))
		return false;

	HTREEITEM		hItem	= nmdi->item.hItem;
//	memory_writer	m(memory_block(nmdi->item.pszText, nmdi->item.cchTextMax - 1));
	fixed_accum		a(nmdi->item.pszText, nmdi->item.cchTextMax - 1);

	try {
		ISO::Browser2	b(tree.GetBrowser(b0, nmdi->item));

		switch (nmdi->iSubItem) {
			case 0:	//symbol
				col0rect |= tree.GetItemRect(hItem, true);
				if (names) {
					ISO::Browser2	bp	= tree.GetBrowser(b0, tree.GetParentItem(hItem));
					if (tag2 id = bp.GetName(int(nmdi->item.lParam))) {
						a << get_id(id);
						break;
					}
					if (tag2 id = b.GetName()) {
						a << get_id(id);
						break;
					}
				}
				a << '[' << int(nmdi->item.lParam) << ']';
				break;

			case 1:	//type
				if (tree.GetItem(hItem, TVIF_STATE).State() & TVIS_EXPANDEDONCE) {
					ISO::ScriptWriter(lvalue(temp_output(a))).DumpType(SkipPointer(b));
				} else if (b.IsPtr()) {
					ISO_ptr_machine<void> p = b;
					return ISO::ScriptWriter(lvalue(temp_output(a))).DumpType(p);
				} else {
					return ISO::ScriptWriter(lvalue(temp_output(a))).DumpType(b);
				}

				break;

			case 2:	//value
				if (tree.GetItem(hItem, TVIF_STATE).State() & TVIS_EXPANDEDONCE)
					b = SkipPointer(b);

				if (b.IsPtr()) {
					if (b.HasCRCType()) {
						a << "unknown type";
						break;
					}
					tag	id = b.GetName();
					if (id && id != tree.GetItemText(hItem)) {
						a << (const char*)id;
						break;
					}
				}

				const ISO::Type *t = b.GetTypeDef()->SkipUser();
#if 1
				if (TypeType(t) == ISO::REFERENCE) {
					tag	id = b.GetName();
					if (id && id != tree.GetItemText(hItem)) {
						a << (const char*)id;
						break;
					}
					b = *b;
					t = b.GetTypeDef()->SkipUser();
				}
#else
				if (t && t->GetType() == ISO::REFERENCE && *(ISO_ptr<void>*)b) {
					if (b.HasCRCType()) {
						m.write("unknown type");
						break;
					}
					tag	id = b.GetName();
					if (id && id != str(tree.GetItemText(hItem))) {
						m.write((const char*)id);
						break;
					}
					b = *b;
					t = b.GetTypeDef()->SkipUser();
				}
#endif
				if (const char *ext = b.External()) {
					a << ext;
				} else if (t && t->GetType() == ISO::STRING) {
					const void	*s = t->ReadPtr(b);
					if (t->flags & ISO::TypeString::UTF16)
						a << (const char16*)s;
					else
						a << (const char*)s;
				} else {
					ISO::ScriptWriter(lvalue(temp_output(a))).SetFlags(ISO::SCRIPT_ONLYNAMES).DumpData(b);
				}
				break;
		}
		//m.putc(0);

	} catch (const char *s) {
		strcpy(nmdi->item.pszText, s);
	}
	return true;
}

void TreeColumnDisplay::PostDisplay(TreeColumnControl &treecolumn) {
	col0width = col0rect.right;
	treecolumn.SetMinWidth(0, col0width + 1);
	col0rect = Rect(0,0,0,0);
}

void TreeColumnDisplay::Expanding(HWND hWnd, const ISO::Browser2 &b0, ISOTree tree, NMTREEVIEW *nmtv, int max_expand) {
	if (nmtv->hdr.hwndFrom == tree && nmtv->action == TVE_EXPAND && !(nmtv->itemNew.state & TVIS_EXPANDEDONCE)) {
		ISO::Browser2	b = tree.GetBrowser(b0, nmtv->itemNew.hItem).SkipUser();
		if (b.GetType() == ISO::REFERENCE) {
			ISO_ptr_machine<void>	p = b;
			const ISO::Type	*type	= p.GetType();
			if (!p.HasCRCType()) {
				const ISO::Type	*type2	= p.GetType()->SkipUser();
				if (type2 && type2->GetType() == ISO::VIRTUAL && type2->Fixed()) {
					ISO_VirtualTarget	v = tree.GetVirtualTarget(b0, nmtv->itemNew.hItem);
					p	= RemoteFix(p.ID(), type, v.v, v.spec);
					b	= ISO::Browser(p);
				}
			}
		}
		if (int n = tree.Setup(b, nmtv->itemNew.hItem, abs(max_expand))) {
			if (max_expand > 0 && MessageBoxA(hWnd, format_string("There are %i items in this list.  Do you still want to expand this node?", n), "Expanding error", MB_ICONQUESTION|MB_YESNO) == IDYES)
				tree.Setup(b, nmtv->itemNew.hItem, n);
				//new ExpandItemThread(tree, b, nmtv->itemNew.hItem, n);
		}
	}
}

//-----------------------------------------------------------------------------
//	Differences
//-----------------------------------------------------------------------------

ISO::Browser2 Differences(const ISO::Browser2 &b1, const ISO::Browser2 &b2, hash_set<void*> &done) {

	const ISO::Type *type	= b1.GetTypeDef();
	if (!type->SameAs(b2.GetTypeDef()))
		return b2;

#if 0
	if (b1.IsPtr()) {
		if (b1.GetName() != b2.GetName())
			return b1;
	}
#endif
	if (type && type->GetType() == ISO::REFERENCE || b1.IsVirtPtr())
		return Differences(*b1, *b2, done);

	if (!type || done.count(b1) || (type->GetType() != ISO::VIRTUAL && CompareData(type, b1, b2)))
		return ISO::Browser2();

	done.insert(b1);

	if (SkipUserType(type) == ISO::STRING) {
		return b1;
	}
	int	n1	= b1.Count();
	int	n2	= b2.Count();
	int	n	= max(n1, n2);

	ISO_ptr<anything>	p(0);
	for (int i = 0; i < n; i++) {
		ISO::Browser2 d;
		if (i < n1 && i < n2) {
			d = Differences(b1[i], b2[i], done);
			if (!d)
				continue;
		} else if (i < n1) {
			d = b1[i];
		} else {
			d = b2[i];
		}
		p->Append(d);
	}
	if (p->Count()) {
		p.SetID(b1.GetName());
		return p;
	}

	return ISO::Browser2();
}

//-----------------------------------------------------------------------------
//	Drag & Drop
//-----------------------------------------------------------------------------

filename GetTempPath(const ISO::Browser2 &b) {
	filename	fn;
	::GetTempPath(sizeof(fn), fn);

	tag	id = b.GetName();
	fn.add_dir(id ? id : "temp");

	if (auto ext = WriteFilesExt(SkipPointer(b), id))
		return fn.set_ext(ext);
	return fn;
}

class ISODropSource : public com_list<IDataObject, IDropSource> {
	ISO::Browser2	b;
	bool			dropping;

public:
//IDataObject
	STDMETHODIMP	GetData(FORMATETC *fmt, STGMEDIUM *med) {
		ISO_TRACEF("GetData %08x\n", fmt->cfFormat);
		if (fmt->cfFormat == CF_ISOPOD) {
			med->tymed			= TYMED_HGLOBAL;
			med->pUnkForRelease	= 0;
			med->hGlobal		= MakeGlobal(ISO::Browser2(b));
			return S_OK;

		} else if (fmt->cfFormat == CF_HDROP && fmt->tymed) {
			filename		fn	= GetTempPath(b);
			if (dropping) {
				Busy	bee;
				HierarchyProgress	progress(WindowPos(0, Rect(0, 0, 1000, 100)), "progress");
				WriteFiles(fn, SkipPointer(b), progress);
				dropping = false;
			}

			global_base	hgDrop(sizeof(DROPFILES) + fn.length() + 2, GMEM_SHARE);
			DROPFILES	*pDrop	= (DROPFILES*)hgDrop.lock();
			pDrop->pFiles		= sizeof(DROPFILES);
			pDrop->fWide		= FALSE;
			strcpy((char*)(pDrop + 1), fn);
			((char*)(pDrop + 1))[fn.length() + 1] = 0;

			med->tymed			= TYMED_HGLOBAL;
			med->pUnkForRelease	= 0;
			med->hGlobal		= pDrop;
			return S_OK;

		} else {
			return DV_E_FORMATETC;

		}
	}
	STDMETHODIMP	QueryGetData(FORMATETC *fmt) {
		ISO_TRACEF("QueryGetData %016p:%08x\n", fmt, fmt->cfFormat);
		if (fmt->cfFormat == CF_ISOPOD)
			return S_OK;
		if (fmt->cfFormat == CF_HDROP && fmt->tymed)
			return S_OK;
		return DV_E_FORMATETC;
	}
	STDMETHODIMP	GetDataHere(FORMATETC *fmt, STGMEDIUM *med)							{ return DATA_E_FORMATETC;}
	STDMETHODIMP	GetCanonicalFormatEtc(FORMATETC *in, FORMATETC *out)				{ out->ptd = NULL; return E_NOTIMPL; }
	STDMETHODIMP	SetData(FORMATETC *fmt, STGMEDIUM *med, BOOL release)				{
		ISO_TRACEF("SetData %08x\n", fmt->cfFormat);
		return E_NOTIMPL;
	}
	STDMETHODIMP	EnumFormatEtc(DWORD dir, IEnumFORMATETC **enum_fmt) {
		if (dir == DATADIR_GET) {
			FORMATETC	format = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
			return SHCreateStdEnumFmtEtc(1, &format, enum_fmt);
		}
		return E_NOTIMPL;
	}

	STDMETHODIMP	DAdvise(FORMATETC *fmt, DWORD adv, IAdviseSink *sink,  DWORD *conn)	{ return OLE_E_ADVISENOTSUPPORTED; }
	STDMETHODIMP	DUnadvise(DWORD conn)												{ return OLE_E_ADVISENOTSUPPORTED; }
	STDMETHODIMP	EnumDAdvise(IEnumSTATDATA **enum_advise)							{ return OLE_E_ADVISENOTSUPPORTED; }

//IDropSource
	STDMETHODIMP	QueryContinueDrag(BOOL escape, DWORD key_state) {
		if (!(key_state & (MK_LBUTTON | MK_RBUTTON))) {
			dropping	= true;
			ISO_TRACEF("DropQuery\n");
			return DRAGDROP_S_DROP;
		}
		return escape ? DRAGDROP_S_CANCEL : S_OK;
	}
	STDMETHODIMP	GiveFeedback(DWORD effect) {
		DWORD	process;
		uint32	thread = GetWindowThreadProcessId(GetControlAt(GetMousePos()), &process);
		if (process == GetCurrentProcessId())
			return S_OK;
		return DRAGDROP_S_USEDEFAULTCURSORS;
	}

public:
	ISODropSource(const ISO::Browser2 &_b) : b(_b), dropping(false) {
	}
	~ISODropSource() {
	}
};

struct IStreamReader : reader_mixin<IStreamReader> {
	com_ptr2<IStream> p;
	IStreamReader(IStream *_p) : p(_p) {}
	size_t	readbuff(void *buffer, size_t size)	{
		ULONG read = 0;
		p->Read(buffer, ULONG(size), &read);
		return read;
	}
	IStream*	_clone()		const	{ return p; }
	streamptr	length()		const	{ STATSTG stg = {0}; return SUCCEEDED(p->Stat(&stg, STATFLAG_NONAME)) ? stg.cbSize.QuadPart : 0; }
	streamptr	tell()			const	{
		LARGE_INTEGER	zero;
		clear(zero);
		ULARGE_INTEGER	pos;
		p->Seek(zero, STREAM_SEEK_CUR, &pos);
		return pos.QuadPart;
	}
	void		_seek(streamptr offset, int origin) {
		LARGE_INTEGER	to;
		to.QuadPart = offset;
		p->Seek(to, STREAM_SEEK_CUR, nullptr);
	}
	void		seek(streamptr offset)		{ _seek(offset, STREAM_SEEK_SET); }
	void		seek_cur(streamptr offset)	{ _seek(offset, STREAM_SEEK_CUR); }
	void		seek_end(streamptr offset)	{ _seek(offset, STREAM_SEEK_END); }
};

//-----------------------------------------------------------------------------
//	IsoEditor2
//-----------------------------------------------------------------------------

enum DROPMODE {
	MODE_CANCEL			= 0,
	MODE_INSERT			= 1,
	MODE_APPEND			= 2,
	MODE_DEFS			= 3,
	MODE_KEEPEXTERNALS	= 1 << 2,
	MODE_RAW			= 2 << 2,
	MODE_ASEXTERNAL		= 3 << 2,
	MODE_LOADAS			= 16,
};

enum DROPFLAGS {
	DROPFLAG_TYPEMENU	= MK_CONTROL,	//8
	DROPFLAG_OPTIONS	= MK_RBUTTON,	//2
	DROPFLAG_ATEND		= 1,			//hAfter == TVI_LAST
	DROPFLAG_DIR		= 4,			//nf == 1 && is_dir(fn)
	DROPFLAG_IH			= 16,			//fn.ext() == ".ih" && nf == 1
};

DROPMODE GetDropMode(Control c, Point pt, uint32 flags, const MenuCallback &types_menu) {
	if (flags & DROPFLAG_TYPEMENU) {
		Menu	menu = Menu::Popup();
		types_menu(c, menu);
		return (DROPMODE)menu.Track(c, Point(pt), TPM_RETURNCMD);

	}

	if (flags & DROPFLAG_OPTIONS) {
		Menu	menu = Menu::Popup();

		if (flags & DROPFLAG_IH) {
			menu.Append("Load definitions", MODE_DEFS);
		} else {
			if (!(flags & DROPFLAG_ATEND)) {
				menu.Append("Insert Here", 1);
				menu.Append("Insert Here (keep externals)", MODE_INSERT|MODE_KEEPEXTERNALS);
				menu.Append("Insert Here (as external)", MODE_INSERT|MODE_ASEXTERNAL);
			}
			menu.Append("Append to Root", 2);
			menu.Append("Append to Root (keep externals)", MODE_APPEND|MODE_KEEPEXTERNALS);
			menu.Append("Append to Root (as external)", MODE_APPEND|MODE_ASEXTERNAL);

			if (flags & DROPFLAG_DIR)
				menu.Append("Append Raw directory", MODE_APPEND|MODE_RAW);
			Menu::Item("Append as", Menu::Create()).Param(&types_menu).AppendTo(menu);

			menu.Separator();
			menu.Append("Cancel", -1);
			return (DROPMODE)menu.Track(c, pt, TPM_RETURNCMD | TPM_RIGHTBUTTON);
		}
	}

	return MODE_INSERT;
}

ControlArrangement::Token arrange[] = {
	ControlArrangement::VSplit(29),
	ControlArrangement::ControlRect(0),
	ControlArrangement::ControlRect(1),
};

//class IsoEditor2 : public Inherit<IsoEditor2, IsoEditor>, com<IDropTarget> {
class IsoEditor2 : public IsoEditor, com<IDropTarget> {//}; , Composition11{
	SplitterWindow		splitter;
	ArrangeWindow		arrangement;
	Control				dummy;
	TreeColumnDisplay	treecolumn_display;

	Menu				context_menu;
	Control				context_control;
	int					context_index;
	HTREEITEM			context_item;

	Control				texteditor;
	LatchMouseButtons	latched_buttons;

	struct MiniEdit : EditControl2 {
		HTREEITEM		item;
		int				subitem;

		void	Create(IsoEditor2 *main, HTREEITEM _item, int _subitem) {
			item			= _item;
			subitem			= _subitem;
			EditControl2::Create(main->treecolumn, NULL, CHILD | VISIBLE | CLIPSIBLINGS | ES_AUTOHSCROLL | ES_WANTRETURN, CLIENTEDGE, main->treecolumn.GetItemRect(item, subitem, false, true), ID_EDIT);
			SetFont(main->treecolumn.GetTreeControl().GetFont());
			MoveAfter(HWND_TOP);
			SetOwner(*main);
			SetFocus();
		}
	} miniedit;

	struct RecentFilesDevice : DeviceT<RecentFilesDevice> {
		struct RecentFile : DeviceCreateT<RecentFile> {
			filename fn;
			ISO_ptr<void>	operator()(const Control &main) {
				tag	id = fn.name();
				if (LatchMouseButtons::get_clear_buttons() & MK_RBUTTON) {
					Event	event;
					int		result;
					JobQueue::Main().add([&]() {
						Menu	menu = Menu::Popup();
						GetTypesMenu(menu, FileInput(fn), fn.ext(), 1,1);
						result = menu.Track(main, GetMessageMousePos(), TPM_RETURNCMD);
						event.signal();
					});
					event.wait();
					if (result) {
						Busy bee;
						if (ISO_ptr<void> p = FileHandler::index(result - 1)->ReadWithFilename(id, fn))
							return p;
						throw_accum("Can't read " << fn);
						return ISO_NULL;
					}
				}
				Busy bee;
				if (ISO_ptr<void> p = FileHandler::Read(id, fn))
					return p;
				if (ISO_ptr<void> p = FileHandler::Get("bin")->ReadWithFilename(id, fn))
					return p;
				throw_accum("Can't read " << fn);
				return ISO_NULL;
			}
			//void	operator()(AppEvent *ev) {
			//	if (ev->state == AppEvent::END)
			//		delete this;
			//}
			RecentFile(const char *_fn) : fn(_fn)	{}
			bool operator==(const char *f) const { return fn == f; }
		};

		order_array<RecentFile>		recent;
		DeviceAdd					add2;

		void	operator()(const DeviceAdd &add) {
			add2	= add("Recent Files");

			RegKey	reg = Settings()["RecentFiles"];
			for (auto v : reg.values()) {
				filename	fn	= v.get_text();
				recent.push_back(fn);
				add2.to_top(fn, &recent.back(), win::Bitmap());
			}
		}

		void	add(const char *fn) {
			auto f = iso::find(recent, fn);
			if (f != recent.end())
				recent.erase(f);
			recent.push_back(fn);
			add2.to_top(fn, &recent.back(), win::Bitmap());

			RegKey	reg = Settings(true).subkey("RecentFiles", KEY_WRITE);
			reg.values()[to_string(recent.size32())] = fn;
		}
	} recent_files;

	struct MenuItem : Menu::Item {
		fixed_string<256>	text;
		int	Get(WPARAM wParam, HMENU hMenu)	{
			fMask = MIIM_DATA | MIIM_ID;
			Text(text, 256);
			GetByPos(hMenu, wParam);
			return wID;
		}
	} menu_item;

	uint32		drag_buttons;
	bool		selchanged;

	//IDropTarget
	virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD buttons, POINTL pt, DWORD *pdwEffect) {
		ImageList::DragEnter(treecolumn, Point(pt) - treecolumn.GetRect().TopLeft());
		*pdwEffect		= DROPEFFECT_COPY;
		drag_buttons	= buttons;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD buttons, POINTL pt, DWORD *pdwEffect) {
		TreeControl	tree	= treecolumn.GetTreeControl();
		Point		ptc		= tree.ToClient(Point(pt));
		HTREEITEM	hit		= tree.HitTest(ptc);
		bool		below	= true;
		if (tree.GetItemState(hit) & TVIS_EXPANDED) {
			hit = tree.GetChildItem(hit);
			below = false;
		}

		treecolumn.SetDropTarget(hit, below ? DROP_BELOW : DROP_ABOVE);
		ImageList::DragMove(Point(pt) - treecolumn.GetRect().TopLeft());

		*pdwEffect			= DROPEFFECT_COPY;
		HTREEITEM	hItem	= 0;
		Rect		rect	= tree.GetClientRect();
		if (ptc.y < 16) {
			hItem = tree.GetNextItem(NULL, TVGN_FIRSTVISIBLE);
			if (hItem)
				hItem = tree.GetNextItem(hItem, TVGN_PREVIOUSVISIBLE);

		} else if (ptc.y > rect.Bottom() - 16) {
			hItem = tree.GetNextItem(NULL, TVGN_FIRSTVISIBLE);
			while (hItem = tree.GetNextItem(hItem, TVGN_NEXTVISIBLE)) {
				if (tree.GetItemRect(hItem).Bottom() > rect.Bottom())
					break;
			}
		}

		if (hItem) {
			*pdwEffect |= DROPEFFECT_SCROLL;
			ImageList::DragShow(false);
			tree.EnsureVisible(hItem);
			tree.Update();
			ImageList::DragShow(true);
		}

		drag_buttons	= buttons;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE DragLeave() {
		ImageList::DragLeave(treecolumn);
		treecolumn.SetDropTarget(0);
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD buttons, POINTL pt, DWORD *pdwEffect) {
		ISOTree		tree	= treecolumn.GetTreeControl();
		FORMATETC	fmte	= {CF_ISOPOD, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM	medium;
		HRESULT		hr;

		DROP_TYPE	drop_type;
		DROPMODE	drop_mode	= MODE_CANCEL;
		HTREEITEM	hAfter		= treecolumn.GetDropTarget(&drop_type);
		HTREEITEM	hParent		= hAfter ? tree.GetParentItem(hAfter) : TVI_ROOT;
		if (!hAfter)
			hAfter = TVI_LAST;
		if (drop_type == DROP_ABOVE)
			hAfter = TVI_FIRST;

		if (SUCCEEDED(hr = pDataObj->GetData(&fmte, &medium))) {
			ISO::Browser	b		= *(ISO::Browser*)medium.hGlobal;
			ISO_ptr<void>	p		= b.SkipUser().GetType() == ISO::REFERENCE ? *(ISO_ptr_machine<void>*)b : b.Duplicate();
			int				mode	= 1;
			if (drag_buttons & MK_RBUTTON) {
				Menu	menu = Menu::Popup();
				if (hAfter != TVI_LAST)
					menu.Append("Insert Here", 1);
				menu.Append("Append to Root", 2);
				menu.Separator();
				menu.Append("Cancel", -1);
				mode = menu.Track(hWnd, Point(pt), TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON);
			}
			switch (mode) {
				case 2:
					hParent	= TVI_ROOT;
					hAfter	= TVI_LAST;
				case 1:
					//recent_files.add()
					Busy(), Do(MakeInsertEntryOp(tree, hParent, hAfter, GetBrowser(hParent), p));
					break;
				default:
					*pdwEffect = DROPEFFECT_NONE;
					break;
			}
			DragLeave();
			return S_OK;
		}

		filename	fn;
		int			nf	= 1;
		fmte.cfFormat	= CF_HDROP;

		if (SUCCEEDED(hr = pDataObj->GetData(&fmte, &medium))) {
			HDROP	hDrop	= (HDROP)medium.hGlobal;
			nf				= DragQueryFile(hDrop, -1, 0, 0);
			DragQueryFileA(hDrop, 0, fn, sizeof(fn));

		} else {
			fmte.cfFormat	= RegisterClipboardFormat(CFSTR_FILECONTENTS);
			fmte.tymed		= TYMED_HGLOBAL | TYMED_ISTREAM | TYMED_ISTORAGE;
			fmte.lindex		= 0;
			if (SUCCEEDED(hr = pDataObj->GetData(&fmte, &medium))) {

				if (medium.tymed & TYMED_ISTREAM) {
					STATSTG stg = {0};
					if (SUCCEEDED(medium.pstm->Stat(&stg, STATFLAG_DEFAULT))) {
						fn = stg.pwcsName;
						CoTaskMemFree(stg.pwcsName);
					}
				}

			}
		}

		if (fn) {
			drop_mode	= GetDropMode(*this, pt,
				(drag_buttons & (DROPFLAG_TYPEMENU|DROPFLAG_OPTIONS)) | (nf == 1 ? (is_dir(fn) ? DROPFLAG_DIR : fn.ext() == ".ih" ? DROPFLAG_IH : 0) : 0) | (hAfter == TVI_LAST ? DROPFLAG_ATEND : 0),
				make_lambda<MenuCallback>([&fn](Control c, Menu m) {
					if (is_dir(fn))
						GetTypesMenu(m, fn, MODE_LOADAS|MODE_APPEND, MODE_LOADAS);
					else
						GetTypesMenu(m, FileInput(fn), fn.ext(), MODE_LOADAS|MODE_APPEND, MODE_LOADAS);
				})
			);

			//drop_mode	= MODE_ASEXTERNAL;

			if (nf == 1 && drop_mode >= 0)
				recent_files.add(fn);

			if (medium.tymed & TYMED_ISTREAM) {
				HTREEITEM	hAfter2		= drop_mode & MODE_APPEND ? TVI_LAST : hAfter;
				HTREEITEM	hParent2	= drop_mode & MODE_APPEND ? TVI_ROOT : hParent;
				uint32		mode2		= drop_mode & ~3;

				reader_mixout<IStreamReader>	file(medium.pstm);
				tag				id	= fn.name();
				ISO_ptr<void>	p;

				if (drop_mode >= MODE_LOADAS) {
					p = FileHandler::index(drop_mode / MODE_LOADAS - 1)->Read(id, file);
				} else if (!(p = FileHandler::Read(id, file, fn.ext()))) {
					p = ReadRaw(id, file, file.length());
				}
				if (p)
					Do(MakeInsertEntryOp(tree, hParent2, hAfter2, GetBrowser(hParent2), p));
				DragLeave();
				return S_OK;
			}

//			ConcurrentJobs::Get().add([=] {
				CoInitialize(0);
				try {
					Busy	bee;
					if (drop_mode < 0) {
						*pdwEffect = DROPEFFECT_NONE;

					} else if (drop_mode == MODE_DEFS) {
						ISO::UserTypeArray	types;
						if (!ISO::ScriptReadDefines(FileInput(fn).me(), types))
							throw_accum("Can't open " << fn);

					} else {
						HTREEITEM	hAfter2		= drop_mode & MODE_APPEND ? TVI_LAST : hAfter;
						HTREEITEM	hParent2	= drop_mode & MODE_APPEND ? TVI_ROOT : hParent;
						uint32		mode2		= drop_mode & ~3;

						int	old_keepexternals;
						if (drop_mode & MODE_KEEPEXTERNALS) {
							old_keepexternals = ISO::root("variables")["keepexternals"].GetInt();
							ISO::root("variables").SetMember("keepexternals", 1);
						}

						if (nf > 1) {
							ISO::Browser2	bp	= GetBrowser(hParent2);
							int				x	= hAfter2 == TVI_LAST ? bp.Count() : hAfter2 == TVI_FIRST ? 0 : tree.FindChildIndex(hParent2, hAfter2) + 1;
							CompositeOp		*op	= new CompositeOp;
							for (int i = 0; i < nf; i++) {
								filename		fn;

								if (medium.tymed & TYMED_HGLOBAL)
									DragQueryFileA((HDROP)medium.hGlobal, i, fn, sizeof(fn));

								tag				id	= fn.name();
								ISO_ptr<void>	p	= mode2 == MODE_ASEXTERNAL ? ISO::MakePtrExternal<void>(fn, id) : FileHandler::Read(id, fn);
								op->Add(MakeInsertEntryOp(tree, hParent2, hAfter2, bp, p));
							}
							Do(op);

						} else {
							tag				id	= fn.name();
							ISO_ptr<void>	p;

							if (medium.tymed & TYMED_HGLOBAL) {
								if (drop_mode >= MODE_LOADAS) {
									p = FileHandler::index(drop_mode / MODE_LOADAS - 1)->ReadWithFilename(id, fn);
//								} else if (is_dir(fn)) {
//									p = ISO::GetDirectory(fn.name().blank() ? (char*)fn.drive() : fn.name(), fn, !!(drop_mode & MODE_RAW));
								} else if (mode2 == MODE_ASEXTERNAL) {
									p = ISO::MakePtrExternal<void>(fn, id);
								} else if (!(p = FileHandler::Read(id, fn))) {
									p = ReadRaw(id, lvalue(FileInput(fn)), filelength(fn));
								}
							} else if (medium.tymed & TYMED_ISTREAM) {
								reader_mixout<IStreamReader>	file(medium.pstm);
								if (drop_mode >= MODE_LOADAS) {
									p = FileHandler::index(drop_mode / MODE_LOADAS - 1)->Read(id, file);
								} else if (!(p = FileHandler::Read(id, file, fn.ext()))) {
									p = ReadRaw(id, file, file.length());
								}
							}

							if (p) {
								if (drop_mode & (MODE_APPEND | MODE_INSERT)) {
									Do(MakeInsertEntryOp(tree, hParent2, hAfter2, GetBrowser(hParent2), p));
								} else {
									int		i = tree.FindChildIndex(hParent, hAfter);
									auto	b = GetBrowser(hParent)[i];
									Do(MakeModifyValueOp(tree, hAfter, b));
									b.Set(p);
								}
							}
						}
						if (drop_mode & MODE_KEEPEXTERNALS)
							ISO::root("variables").SetMember("keepexternals", old_keepexternals);

					}
				} catch (const char *s) {
					ModalError(s);
				}

//			});
			DragLeave();
			hr = S_OK;
		}
		return hr;
	}

	ISO::Browser2	SetExternal(HTREEITEM hItem) {
		if (!hItem)
			return root_ptr;
		ISOTree			tree	= treecolumn.GetTreeControl();
		ISO::Browser2	b		= SetExternal(tree.GetParentItem(hItem));
		while (b.SkipUser().GetType() == ISO::REFERENCE) {
			((ISO_ptr<void>*)b)->SetFlags(ISO::Value::HASEXTERNAL);
			b = *b;
		}
		return tree.GetChild(b, hItem);
	}
	ISO::Browser2	TryGetBrowser(HTREEITEM hItem) {
		try {
			return GetBrowser(hItem);
		} catch (const char *s) {
			ModalError(s);
			return ISO::Browser();
		}
	}

	void			LeftClick(HTREEITEM h);
	void			RightClick(HTREEITEM h);
	void			BeginDrag(HTREEITEM hItem, const Point &pt);

public:
	void			DropFiles(HDROP hDrop, HTREEITEM hParent, HTREEITEM hAfter, uint32 buttons);
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	IsoEditor2(const Rect &rect) : splitter(SplitterWindow::SWF_VERT), arrangement(arrange, &dummy, 2), selchanged(true) {
		Create(WindowPos(NULL, rect), "IsoEditor", OVERLAPPEDWINDOW | CLIPCHILDREN, NOEX);
		Rebind(this);

		SendMessage(WM_SETICON, 0, Icon::Load(IDI_ISOPOD));

		ISOTree(treecolumn.GetTreeControl()).Setup(root_ptr, TVI_ROOT, max_expand);
		treecolumn.UpdateScrollInfo();
		Show(SW_SHOW);

		AppEvent(AppEvent::BEGIN).send();
		RegisterDragDrop(treecolumn, this);
	}

	~IsoEditor2() {
		RevokeDragDrop(treecolumn);
		undo.deleteall();
		AppEvent(AppEvent::END).send();
	}

	void AddRecent(const char *fn) {
		recent_files.add(fn);
	}
};

IsoEditor*	IsoEditor::Cast(Control c) {
	return CastByProc<IsoEditor2>(c);
}

void IsoEditor::AddRecent(const char *fn) {
	static_cast<IsoEditor2*>(this)->AddRecent(fn);
}

void IsoEditor2::BeginDrag(HTREEITEM hItem, const Point &pt) {
	TreeControl		tree	= treecolumn.GetTreeControl();
	ISO::Browser2	b		= GetBrowser(hItem);
	tree.SetSelectedItem(hItem);

	ImageList	il = tree.CreateDragImage(hItem);
	il.DragBegin(0, pt - tree.GetItemRect(hItem, true).TopLeft() + Point(tree.GetImageList().GetIconSize().x, 0));

	ISODropSource	*drop	= new ISODropSource(b);
	try {
		DWORD	effect;
		HRESULT hr = DoDragDrop(drop, drop, DROPEFFECT_LINK | DROPEFFECT_COPY, &effect);
	} catch_all() {
	}
	drop->Release();

	ImageList::DragEnd();
}

void IsoEditor2::DropFiles(HDROP hDrop, HTREEITEM hParent, HTREEITEM hAfter, uint32 buttons) {
	IsoEditorCache	cache;
	int				nf		= DragQueryFile(hDrop, -1, 0, 0);
	bool			ctrl	= buttons & MK_CONTROL;
	filename		fn;
	POINT			pt;

	DragQueryFileA(hDrop, 0, fn, sizeof(fn));
	DragQueryPoint(hDrop, &pt);

	try {
		ISOTree			tree	= treecolumn.GetTreeControl();
		ISO::Browser2	bp		= GetBrowser(hParent);

		FileHandler *fh		= 0;
		if (nf > 1) {
			if (!ctrl)
				fh = FileHandler::Get(fn.ext());
			if (!fh)
				fh = GetFileHandlerFromMenu(fn, *this, ToScreen(pt));
			if (fh) {
				Busy	bee;
				int		x = hAfter == TVI_LAST ? bp.Count() : hAfter == TVI_FIRST ? 0 : tree.FindChildIndex(hParent, hAfter) + 1;
				for (int i = 0; i < nf; i++) {
					DragQueryFileA(hDrop, i, fn, sizeof(fn));
					ISO_ptr<void>	p = fh->ReadWithFilename(fn.name(), fn);

					BrowserAssign(bp.Insert(x), ISO::Browser(p));
					tree.InsertItem(hParent, hAfter, x, p);
				}
			}
		} else if (fn.ext() == ".ih") {
			Busy				bee;
			ISO::UserTypeArray	types;
			if (!ISO::ScriptReadDefines(FileInput(fn).me(), types))
				throw_accum("Can't open " << fn);
			MessageBoxA(*this, format_string("Definitions loaded from %s.", (const char*)fn), "Loaded", MB_OK);

		} else if (is_dir(fn)) {
			if (fn.name().blank())
				fn.end()[-1] = 0;
			Busy	bee;
			if (ISO_ptr<void> p = ISO::GetDirectory(fn.name().blank() ? (char*)fn.drive() : (char*)fn.name(), fn, ctrl))
				Do(MakeInsertEntryOp(tree, hParent, hAfter, bp, p));

		} else {
			tag				id	= fn.name();
			ISO_ptr<void>	p;
			if (!ctrl)
				p = (Busy(), FileHandler::Read(id, fn));
			if (!p && (fh = GetFileHandlerFromMenu(fn, *this, ToScreen(pt))))
				p = (Busy(), fh->ReadWithFilename(id, fn));
			if (p)
				Busy(), Do(MakeInsertEntryOp(tree, hParent, hAfter, bp, p));
		}

	} catch (const char *s) {
		ModalError(s);
	}
}

void IsoEditor2::LeftClick(HTREEITEM h) {
	if (miniedit.hWnd)
		miniedit.Destroy();

	ISO_VirtualTarget	v		= GetVirtualTarget(h);
	ISO::Browser		b		= v;
	const ISO::Type*	type	= b.GetTypeDef();
	Rect				r		= treecolumn.GetItemRect(h, 2);
	bool				imm		= true;

	buffer_accum<256>	a;

	if (TypeType(type->SkipUser()) == ISO::REFERENCE) {
		ISO_ptr_machine<void>	p = *b;
		if (p.HasCRCType()) {
			a << "unknown type";
		} else {
			imm		= ((ISO::TypeReference*)type->SkipUser())->subtype == p.GetType();
			tag	id	= p.ID();
			if (false && id && id != treecolumn.GetTreeControl().GetItemText(h)) {
				if (ISO::ScriptWriter::LegalLabel(id)) {
					a << id;
				} else {
					a.format("'%s'", (const char*)id);
				}
			} else if (!b.External()) {
				type	= p.GetType();
				b		= ISO::Browser(p);
			}
		}
	}

	if (a.length() == 0) {
		// colours
		if (type->Is("rgba8") || type->Is("rgba8u") ) {
			if (GetColour(*this, *(win::Colour*)b)) {
				v.Update();
				treecolumn.Invalidate(r);
			}
			return;

		} else if (type->Is("colour")) {
			float3p		col{b[0].GetFloat(), b[1].GetFloat(), b[2].GetFloat()};
			win::Colour	colw(col.x * 255, col.y * 255, col.z * 255);
			if (GetColour(*this, colw)) {
				b[0].Set(colw.r / 255.f);
				b[1].Set(colw.g / 255.f);
				b[2].Set(colw.b / 255.f);
				v.Update();
				treecolumn.Invalidate(r);
			}
			return;
		}
		type	=  type->SkipUser();

		// enums
		if (type && type->GetType() == ISO::INT && (type->flags & ISO::TypeInt::ENUM)) {
			context_menu			= Menu::Popup();
			ISO::TypeEnum	&e		= *(ISO::TypeEnum*)type;
			uint32	value			= e.get(b);
			uint32	factors[64], nf = e.factors(factors, 64);

			context_menu.Append("edit", -1);
			for (int i = 0, j = 0; !e[i].id.blank(); i++) {
				if (nf > 0 && (e[i].value == 0 || e[i].value >= factors[j])) {
					context_menu.Separator();
					j++;
				}
				context_menu.Append(e[i].id.get_tag(), e[i].value + 1);
			}

			const ISO::EnumT<uint32> *i;
			for (uint32 x = value; x && (i = e.biggest_factor(x)); x -= i->value)
				context_menu.CheckByID(i->value + 1);

			int	ret = context_menu.Track(*this, treecolumn.ToScreen(r.BottomLeft()), TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON);
			if (ret == 0)
				return;

			if (ret > 0) {
				ret--;
				if (nf > 0) {
					for (int i = 1; ; i++) {
						if (factors[i] > ret) {
							uint32	prev = value % factors[i] - value % factors[i - 1];
							if (prev == ret)
								ret = 0;
							value = value % factors[i - 1] + value / factors[i] * factors[i] + ret;
							break;
						}
					}
				} else {
					value = ret;
				}
				e.set(b, value);
				v.Update();
				treecolumn.Invalidate(r);
				return;
			}
		}

		// anything else

		if (imm && type && type->GetType() == ISO::STRING) {
			const void	*s = type->ReadPtr(b);
			if (type->flags & ISO::TypeString::UTF16)
				a << (const char16*)s;
			else
				a << (const char*)s;
		} else {
			temp_output			t(a);
			ISO::ScriptWriter	s(t);
			s.SetFlags(ISO::SCRIPT_IGNORE_DEFER);

			if (!imm) {
				s.DumpType(b);
				t.putc(' ');
			}
			s.DumpData(b);
		}

		const char	*p = a.term();
		char		*d = unconst(p);
		while (*p) {
			if (*p < ' ') {
				while (*++p && *p < ' ');
				*d++ = ' ';
			} else {
				*d++ = *p++;
			}
		}
	}
	miniedit.Create(this, h, 2);
	miniedit.SetText(a);
	selchanged		= true;
}

void IsoEditor2::RightClick(HTREEITEM h) {
	TreeControl		tree	= treecolumn.GetTreeControl();
	context_item	= h;
	context_menu	= Menu::Popup();
	context_menu.SetStyle(MNS_NOTIFYBYPOS);

	if (h) {
		context_menu.Append("Edit", ID_ENTITY_EDIT);
		ISO::Browser2	b = TryGetBrowser(tree.GetParentItem(h)).SkipUser();
		if (b.GetType() == ISO::REFERENCE)
			b = (*b).SkipUser();
		if (b.GetType() == ISO::OPENARRAY || b.GetType() == ISO::VIRTUAL) {
			context_menu.Append("Cut", ID_EDIT_CUT);
			context_menu.Append("Delete", ID_ENTITY_DELETE);
			if (UsableClipboardContents())
				context_menu.Append("Paste", ID_EDIT_PASTE);
		}
		context_menu.Append("Copy", ID_EDIT_COPY);

		if (h != tree.GetSelectedItem())
			context_menu.Append("Compare", ID_ENTITY_COMPARE);

		if ((b.GetType() == ISO::OPENARRAY || b.GetType() == ISO::ARRAY) && b.GetTypeDef()->SubType()->GetType() == ISO::REFERENCE)
			context_menu.Append("Rename", ID_ENTITY_RENAME);

		context_menu.Append("Save As...", ID_ENTITY_SAVE);

		context_menu.Separator();

	} else if (UsableClipboardContents()) {
		context_menu.Append("Paste", ID_EDIT_PASTE);
		context_menu.Separator();
	}

	ISO::Browser2	b0		= TryGetBrowser(h);
	tag2			id		= b0.GetName();

	ISO::Browser2	b		= VirtualDeref(b0).SkipUser();
	if (!id)
		id = b.GetName();

	if (b.GetType() == ISO::REFERENCE) {
		ISO_ptr<void>	p		= b;
		const char		*ext	= b.External();
		b0	= p;

		if (!id)
			id = p.ID();

		if (!ext) {
			b	= VirtualDeref(b0).SkipUser();
			switch (b.GetType()) {
				case ISO::REFERENCE:
					b0 = p = b;
					break;
				case ISO::VIRTUAL:
					if (b = *b)
						b0 = b;
					break;
				default:
					break;
			}
		}

		if ((p.Flags() & (ISO::Value::ROOT | ISO::Value::FROMBIN)) == (ISO::Value::ROOT | ISO::Value::FROMBIN))
			context_menu.Append("View Physical Memory", ID_ENTITY_PHYSICAL);

		context_menu.Append("Browse for file", ID_ENTITY_BROWSE);

		if (ext && !str((const char*)b).find(';')) {
			if (FileHandler *fh = FileHandler::Get(filename(ext).ext()))
				Menu::Item().Text(format_string("Load as %s", fh->GetDescription())).ID(ID_ENTITY_EXPANDAS).Param(fh).AppendTo(context_menu);
			context_menu.Append("Load as", GetTypesMenu(ID_ENTITY_EXPANDAS));
		}

		context_menu.Append("Evaluate", ID_ENTITY_EVALUATE);
		Menu::Item("Send to", Menu::Create()).Param(&sendto_menu).AppendTo(context_menu);

		if (ISO::Tokeniser::GetLineNumber(p))
			context_menu.Append("Edit source", ID_ENTITY_EDITSOURCE);

	}

	if (int bin	= FindRawData(b0)) {
		if (FileHandler *fh = FileHandler::Get(filename(tag(id)).ext()))
			Menu::Item().Text(format_string("Expand as %s", fh->GetDescription())).ID(ID_ENTITY_EXPANDAS).Param(fh).AppendTo(context_menu);
		/*
		Menu menu =  Menu::Create();
		if (bin == 3)
			GetTypesMenu(menu, BigBinStream(b0), 0, ID_ENTITY_EXPANDAS);
		else
			GetTypesMenu(menu, BinStream(b0), 0, ID_ENTITY_EXPANDAS);
		context_menu.Append("Expand as", menu);
		*/
		context_menu.Append("Expand as", GetTypesMenu(Menu::Create(), bin == 3 ? istream_ref(BigBinStream(b0)) : istream_ref(BinStream(b)), 0, ID_ENTITY_EXPANDAS));
	}

	if (b.GetTypeDef()->SameAs<anything>() || b.GetType() == ISO::VIRTUAL) {
		Menu	child	= Menu::Popup();
		Menu::Item("using extension", ID_ENTITY_EXPANDDIR).Param(0).AppendTo(child);
		Menu::Item("using detection", ID_ENTITY_EXPANDDIR).Param(1).AppendTo(child);
		Menu::Item("recursively, using extension", ID_ENTITY_EXPANDDIR).Param(2).AppendTo(child);
		Menu::Item("recursively, using detection",ID_ENTITY_EXPANDDIR).Param(3).AppendTo(child);
		context_menu.Append("Expand children", child);
	}

	if (IsRawData(b))
		context_menu.Append("View as", GetTypesMenu(Menu::Create(), memory_reader(GetRawData(b)), 0, ID_ENTITY_VIEWAS));

	if (b.GetType() == ISO::OPENARRAY || b.GetType() == ISO::VIRTUAL)
		context_menu.Append("Append entry", ID_ENTITY_APPEND);

	context_menu.Separator();
	context_menu.Append("Expand All", ID_ENTITY_EXPANDALL);

	if (!(tree.GetItem(h, TVIF_STATE).state & TVIS_EXPANDEDONCE))
		context_menu.Append("Force Expand", ID_ENTITY_FORCEEXPAND);

	context_menu.Append("Find...", ID_ENTITY_FIND);
	context_menu.Append("Sort", ID_ENTITY_SORT);

	context_menu.Track(*this, GetMousePos());
}

LRESULT IsoEditor2::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			//Composition11::Init(*this);

			try {
				graphics.Init(*this);
			} catch (const char *s) {
				ModalError(s);
			}
			_CF_HTML		= Clipboard::Register("HTML Format");

			crc32::set_callback(&get_crc_string);

			DeviceAdd	add(Menu::Popup(), ID_ADD_DUMMY);
			add.menu.SetStyle(MNS_NOTIFYBYPOS);
			for (Device::iterator i = Device::begin(); i != Device::end(); ++i)
				(*i)(add);
			//break;
#if 0
			arrangement.Create(splitter, NULL, CHILD | CLIPCHILDREN | VISIBLE, 0, GetClientRect());
			arrangement.Class().background = Brush::White();
			splitter.SetPane(0, arrangement);

			ToolBarControl	tb(arrangement, NULL, CHILD | VISIBLE | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS, 0);
			tb.Init(IDR_TOOLBAR_MAIN);
			ToolBarControl::Item().Style(BTNS_DROPDOWN).Param((void*)add_menu).Set(tb, ID_ADD_DUMMY);
			AddToolbar(tb);
			treecolumn.Create(arrangement, NULL, CHILD | VISIBLE | HSCROLL | TCS_GRIDLINES | TCS_HEADERAUTOSIZE, CLIENTEDGE | ACCEPTFILES);
#else
			ToolBarControl	tb = CreateToolbar(IDR_TOOLBAR_MAIN);
			ToolBarControl::Item().Style(BTNS_DROPDOWN).Param((void*)add.menu).Set(tb, ID_ADD_DUMMY);
			treecolumn.Create(GetChildWindowPos(), NULL, CHILD | VISIBLE | HSCROLL | TCS_GRIDLINES | TCS_HEADERAUTOSIZE, ACCEPTFILES);
			SetChildImmediate(treecolumn);
#endif

			HeaderControl	header	= treecolumn.GetHeaderControl();
			header.SetValue(GWL_STYLE, CHILD | VISIBLE | HDS_FULLDRAG);
			HeaderControl::Item("Symbol").	Format(HDF_LEFT).Width(100).Insert(header, 0);
			HeaderControl::Item("Type").	Format(HDF_LEFT).Width(100).Insert(header, 1);
			HeaderControl::Item("Value").	Format(HDF_LEFT).Width(100).Insert(header, 2);
			treecolumn.SetMinWidth(2, 100);
			treecolumn.GetTreeControl().style = CHILD | VISIBLE | CLIPSIBLINGS | TVS_NOHSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT;

			tree_imagelist = ImageList::CreateSmallIcons(ILC_COLOR32, 1, 1);
			tree_imagelist.ScaleAdd(LoadPNG("IDB_DOT"));

			treecolumn.GetTreeControl().SetImageList(tree_imagelist);
			break;
		}
#if 0
		case WM_MOUSEMOVE: {
			Cursor	c = Cursor::Current();
			Cursor::Params	info(c);
			DeviceContext	dc		= DeviceContext().Compatible();
			DIB		*cdib	= DIB::Create(dc, info.Color());
			DIB		*mdib	= DIB::Create(dc, info.Mask());
			break;
		}
#endif
		//case WM_SIZE:
		//	 Composition11::Size(wParam);
		//	 break;

		case WM_EXITSIZEMOVE: {
			WINDOWPLACEMENT	wp = {sizeof(wp)};
			if (GetWindowPlacement(*this, &wp)) {
				wp.rcNormalPosition = AdjustRectFromChild(treecolumn.GetRect());
				Settings(true).values()["window"] = format_string("%i,%i,%i,%i,%i,%i,%i",
					wp.showCmd, wp.ptMaxPosition.x, wp.ptMaxPosition.y,
					wp.rcNormalPosition.left, wp.rcNormalPosition.right, wp.rcNormalPosition.top, wp.rcNormalPosition.bottom
				);
			}
			break;
		}

		case WM_COPYDATA: {
			Busy			bee;
			COPYDATASTRUCT	*cds	= (COPYDATASTRUCT*)lParam;
			if (cds->lpData) {
				filename		&fn		= *(filename*)cds->lpData;
				if (fn == "-tty") {
					MakeTTYViewer(Docker(this).Dock(DOCK_RIGHT | DOCK_EXTEND | DOCK_FIXED_SIB | DOCK_OR_TAB, 480));
				} else try {
					if (fn[0] == '+') {
						ISO::UserTypeArray	types;
						if (!ISO::ScriptReadDefines(FileInput(fn.begin() + 1).me(), types))
							throw_accum("Can't open " << fn + 1);
					} else {
						AddEntry(FileHandler::Read(fn.name(), fn));
					}
				} catch (const char *s) {
					ModalError(s);
				}
			} else {/*
				ISO_ptr<void>	p = GetPtr(cds.dwData);
				a->Append(p);
				if (tree.GetItem(h, TVIF_STATE).state & TVIS_EXPANDEDONCE) {
					int i = a->Count() - 1;
					AddTreeItem(tree, h, tree.ItemNameI(ISO::Browser(a), i), i, GetFlags(p));
				}*/
			}
//			Move(HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE);
//			SetActiveWindow(*this);
			SetForegroundWindow(*this);
			return 1;
		}

		case WM_RENDERFORMAT:
			ISO_TRACE("WM_RENDERFORMAT\n");
			switch (wParam) {
				case CF_TEXT: {
					dynamic_memory_writer	file;
					ISO::ScriptWriter(file).SetFlags(ISO::SCRIPT_IGNORE_DEFER).DumpData(clipdata.Duplicate());
					file.putc(0);
					Clipboard::Set(CF_ISOPOD, file.data());
					return 0;
				}
			}
			return 0;

		case WM_CHAR: {
			bool	editing = !!miniedit;
			if (editing) {
				if (wParam == 27)
					miniedit.SetText("");
				miniedit.Destroy();
			}

			if ((wParam & 0x7fff) == '\t') {
				TreeControl	tree = treecolumn.GetTreeControl();
				int			n	= wParam & 0x8000 ? TVGN_PREVIOUS : TVGN_NEXT;
				HTREEITEM	h	= miniedit.item, h2 = tree.GetSelectedItem();
				if (editing) {
					while (h && !(h2 = tree.GetNextItem(h, n)))
						h = tree.GetParentItem(h);
				}
				if (h = h2) {
					while (h2 = tree.GetChildItem(h))
						h = h2;
					tree.EnsureVisible(h);
					LeftClick(h);
				}
			}
			break;
		}

		case WM_ISO_SELECT:
			SelectRoute((const char*)lParam);
			break;

		case WM_MENUSELECT:
			if (HIWORD(wParam) & MF_POPUP) {
				Menu		menu((HMENU)lParam);
				fixed_string<256>	name;
				Menu::Item	item(MIIM_DATA);
				if (item.Text(name, 256)._GetByPos(menu, LOWORD(wParam)) && (void*)item.Param())
					(*(MenuCallback*)item.Param())(*this, menu.GetSubMenuByPos(LOWORD(wParam)));
			}
			break;

		case WM_MENUCOMMAND:
			wParam = menu_item.Get(wParam, (HMENU)lParam);
			context_menu.Destroy();
		case WM_COMMAND: {
			int	id = LOWORD(wParam);
			switch (id) {
				case ID_FILE_TABS:
					if (auto *sw = SplitterWindow::Cast(child)) {
						if (!TabWindow::Cast(sw->GetPane(1)))
							sw->SetPane(1, *new TabWindow(sw->GetPanePos(1), sw->GetPane(1)));
					}
					return 0;

				case ID_FILE_TTY:
					MakeTTYViewer(Docker(this).Dock(DOCK_RIGHT | DOCK_EXTEND | DOCK_FIXED_SIB | DOCK_OR_TAB, 480));
					return 0;

				case ID_OPT_NAMES:
					treecolumn_display.names = !treecolumn_display.names;
					treecolumn.Invalidate();
					return 0;

				case ID_ADD_DUMMY: {
					DeviceCreate	*dev = (DeviceCreate*)menu_item.Param();
					ConcurrentJobs::Get().add([this, dev]() {
						try {
							if (ISO_ptr<void> p = (*dev)(*this))
								AddEntry(p, false);
						} catch (const char *s) {
							ModalError(s);
						}
					});
					return 0;
				}

				case ID_EDIT_UNDO:
					if (!undo.empty()) {
						Operation	*o = undo.pop_front();
						if (!o->Undo())
							ModalError("Failed to undo");

						redo.push_front(o);
					}
					return 0;

				case ID_EDIT_REDO:
					if (!redo.empty()) {
						Operation	*o = redo.pop_front();
						if (!o->Do())
							ModalError("Failed to redo");
						undo.push_front(o);
					}
					return 0;

				case ID_ENTITY_DELETE: {
					ISOTree	tree	= treecolumn.GetTreeControl();
					if (HTREEITEM h = context_item) {
						HTREEITEM		hp	= tree.GetParentItem(h);
						ISO::Browser2	bp	= GetBrowser(hp);
						ISO_ptr<void>	p	= bp.GetPtr();
						if (p.Flags() & ISO::Value::PROCESSED) {
							p.ClearFlags(ISO::Value::PROCESSED);
							_Duplicate(p.GetType(), p);
						}

						ISO_VirtualTarget	v = GetVirtualTarget(hp);
						ISO::Browser		b = v.SkipUser();
						if (b.GetType() == ISO::REFERENCE)
							b = (*b).SkipUser();
						if (!Do(MakeDeleteOp(tree, hp, tree.GetIndex(h), v)))
							ModalError("Failed to delete");
					}
					return 0;
				}

				case ID_EDIT_CUT: {
					Clipboard	clip(*this);
					if (clip.Empty()) {
						ISOTree		tree	= treecolumn.GetTreeControl();
						if (HTREEITEM h = context_item) {
							HTREEITEM	hp		= tree.GetParentItem(h);
							int			index	= tree.GetIndex(h);
							ISO_VirtualTarget	v = GetVirtualTarget(hp);
							if (v.SkipUser().GetType() == ISO::REFERENCE)
								v = *v;
						#if 1
							clip.Set(CF_ISOPOD, v[index]);
							clip.Set(CF_TEXT, 0);
						#else
//							dynamic_memory_writer	file;
//							ISO::binary_data.Write(v[index].Duplicate(), file, 0, ISO::BIN_STRINGIDS | ISO::BIN_DONTCONVERT);
//							clip.Set(CF_ISOPOD, Global(file, file.length()));
						#endif
							if (!Do(MakeDeleteOp(tree, hp, index, v)))
								ModalError("Failed to delete");
						}
					}
					return 0;
				}

				case ID_EDIT_COPY: {
					Clipboard	clip(*this);
					if (clip.Empty()) {
						TreeControl		tree	= treecolumn.GetTreeControl();
						ISO::Browser2	b		= TryGetBrowser(context_item);

					#if 1
						clipdata			= SkipPointer(b);
						clip.Set(CF_ISOPOD, clipdata);
						clip.Set(CF_TEXT, 0);
					#else
						dynamic_memory_writer	file;
						ISO::binary_data.Write(SkipPointer(b), file, 0, ISO::BIN_STRINGIDS | ISO::BIN_DONTCONVERT);
						Global	data(file, file.length());
						clipdata	= data;
						clip.Set(CF_ISOPOD, data);
						clip.Set(CF_TEXT, 0);
					#endif
					}
					return 0;
				}

				case ID_EDIT_PASTE: {
					ISOTree			tree	= treecolumn.GetTreeControl();
					HTREEITEM		h		= context_item;
					ISO::Browser2	b		= SkipPointer(TryGetBrowser(h)).SkipUser();
					if (b.GetType() != ISO::OPENARRAY) {
						h = 0;
						b = ISO::Browser(*root_ptr);
					}
					try {
						if (ISO_ptr<void> p = GetClipboardContents(0, *this)) {
							int		i = b.Count();
							b.Append().Set(p);
							if (!h || (tree.GetItem(h, TVIF_STATE).state & TVIS_EXPANDEDONCE))
								tree.AddItem(h, TVI_LAST, tree.ItemNameI(b, i), i, tree.GetFlags(b[i]));
						}
					} catch (const char *s) {
						ModalError(s);
					}
					return 0;
				}

				case ID_APP_EXIT:
					Destroy();
					return 0;

				case ID_EDIT: {
					switch (HIWORD(wParam)) {
						case EN_KILLFOCUS:
							if (selchanged) {
								selchanged = false;
								miniedit.SetFocus();
//								miniedit.SetSelection(miniedit.GetTextLength());
								miniedit.SetSelection(CharRange::all());
								break;
							}

							ISOTree	tree	= treecolumn.GetTreeControl();

							if (string buffer = miniedit.GetText()) {
								ISO_VirtualTarget	v = GetVirtualTarget(miniedit.item);

								if (miniedit.subitem == 0) {
									v.SetName(buffer);
									Invalidate(ToClient(miniedit.GetRect()));

								} else try {
									bool	imm	= v.GetType() != ISO::REFERENCE || ((ISO::TypeReference*)v.GetTypeDef())->subtype == (*v).GetTypeDef();
									if (v.v && v.GetType() == ISO::REFERENCE && !v.Is<ISO_ptr<void> >())
										v = *v;

									Do(MakeModifyValueOp(tree, miniedit.item, v));
									if (imm && v.GetType() == ISO::STRING)
										((ISO::TypeString*)v.GetTypeDef())->set(v, buffer);
									else
										ISO::ScriptRead(v, lvalue(memory_reader(buffer.data())), ISO::SCRIPT_KEEPEXTERNALS | ISO::SCRIPT_DONTCONVERT);

									if (v.Update()) {
										if (tree.GetItem(miniedit.item, TVIF_STATE).state & TVIS_EXPANDEDONCE) {
											tree.DeleteChildren(miniedit.item);
											tree.Setup(v, miniedit.item, 0);
										} else {
											fixed_string<256>	name;
											TreeControl::Item	i(miniedit.item, TVIF_CHILDREN);
											i.Text(name).Get(tree);
											if (name[0] == '[') {
												if (tag id = v.GetName())
													name = id;
											}
											i.Children(ISOTree::GetFlags(v) & ISOTree::HAS_CHILDREN);
											i.Set(tree);
										}
										tree.Invalidate();
										tree.SetSelectedItem(miniedit.item);
									} else {
										ModalError("Failed to update");
									}
								} catch (const char *s) {
									ModalError(s);
								}
							} else {
								Invalidate(ToClient(miniedit.GetRect()));
							}
							miniedit.Destroy();
							tree.SetFocus();
							SetAccelerator(*this, IDR_ACCELERATOR_MAIN);
							break;
					}
					return 0;
				}

				case ID_ENTITY_EDIT: {
					Busy			bee;
					IsoEditorCache	cache;
					TreeControl		tree	= treecolumn.GetTreeControl();
					HTREEITEM		h		= context_item;
					try {
						ISO_VirtualTarget	v = GetVirtualTarget(h);
						if (editwindow && editwindow(WM_COMMAND, MAKEWPARAM(ID_EDIT, 0), &v))
							return 0;

						Docker	docker(this);
						DockEdge edge	= DOCK_RIGHT | DOCK_EXTEND | DOCK_DOMINANT | DOCK_FIXED_SIB | DOCK_FIXED | DOCK_OR_TAB;

						ISO_VirtualTarget	v2 = v;
						if (Editor *ed = Editor::Find(v2)) {
							editwindow = ed->Create(*this, docker.Dock(edge, 480), v2);
							editwindow.SetFocus();
							return 0;
						}

						if (v.External()) {
							ISO::Browser2	b = v.GetPtr();
							if (Editor *ed = Editor::Find(b)) {
								editwindow = ed->Create(*this, docker.Dock(edge, 480), v);
								editwindow.SetFocus();
								return 0;
							}
						}
						MakeIXEditor(*this, docker.Dock(edge, 480), v);

					} catch (const char *s) {
						ModalError(s);
					}
					return 0;
				}

				case ID_ENTITY_RENAME: {
					TreeControl		tree	= treecolumn.GetTreeControl();
					HTREEITEM		h		= context_item;
					Rect			r		= treecolumn.GetItemRect(h, 0, false, true);
					ISO::Browser2	b		= GetBrowser(h);
					tag2			id		= b.GetName();

					miniedit.Create(this, h, 0);
					miniedit.SetText(id.get_tag());
					selchanged		= true;
					return 0;
				}

				case ID_ENTITY_APPEND: {
					ISO::Browser2	b	= SkipPointer(GetBrowser(context_item)).SkipUser();
					if (b.GetType() == ISO::OPENARRAY || b.GetType() == ISO::VIRTUAL)
						Do(MakeAddEntryOp(treecolumn.GetTreeControl(), context_item, b));
					return 0;
				}

				case ID_ENTITY_BROWSE: {
					TreeControl		tree	= treecolumn.GetTreeControl();
					HTREEITEM		h		= context_item;
					ISO::Browser2	b		= GetBrowser(h).SkipUser();
					filename		fn;

					if (const char *ext = b.External())
						fn = ext;

					if (GetOpen(hWnd, fn, "Find external", GetFileFilter())) {
						b.Set(MakePtrExternal(((ISO::TypeReference*)b.GetTypeDef())->subtype, fn));
						SetExternal(h);
					}
					return 0;
				}

				case ID_ENTITY_EVALUATE: {
					TreeControl		tree	= treecolumn.GetTreeControl();
					HTREEITEM		h		= context_item;
					ISO::Browser2	b		= GetBrowser(h).SkipUser();
					try {
						if (ISO_ptr<void> *pp = b) {
							auto			p	= *pp;
							Busy			bee;
							IsoEditorCache	cache;
							const ISO::Type	*type = p.GetType();
							if (type && type->GetType() == ISO::REFERENCE && (((ISO_ptr<void>*)p)->Flags() & ISO::Value::REDIRECT))
								type = type->SubType();
							timer	start;
	#if 0
							if (p.IsExternal())
								p	= FileHandler::ExpandExternals(p);
							p	= ISO_conversion::convert(p, type, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::ALLOW_EXTERNALS);
	#else
							p	= ISO_conversion::convert(p, type, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
	#endif
							ISO_OUTPUTF("Evaluation took %fs\n", (float)start);
							Do(MakeModifyPtrOp(tree, h, *pp, p));

						} else if (ISO_ptr_machine<void> *pp = b) {
							auto			p	= *pp;
							Busy			bee;
							IsoEditorCache	cache;
							const ISO::Type	*type = p.GetType();
							if (type && type->GetType() == ISO::REFERENCE && (((ISO_ptr<void>*)p)->Flags() & ISO::Value::REDIRECT))
								type = type->SubType();
							timer	start;
							p	= ISO_conversion::convert(p, type, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
							ISO_OUTPUTF("Evaluation took %fs\n", (float)start);
							Do(MakeModifyPtrOp(tree, h, *pp, p));
						}
					} catch (const char *s) {
						ModalError(s);
					}
					tree.Invalidate();
					return 0;
				}

				case ID_ENTITY_SAVE: {
					TreeControl		tree	= treecolumn.GetTreeControl();
					ISO::Browser2	b		= GetBrowser(context_item);
					filename		fn		= tag(b.GetName());

					//if (b.GetType() == ISO::VIRTUAL && b.Count() == 1)
					//	b = b[0];
					if (b.SkipUser().GetType() == ISO::REFERENCE)
						b = *b;
					if (GetSave(hWnd, fn, "Save As", GetFileFilter())) try {
						Busy			bee;
						IsoEditorCache	cache;
						if (fn.ext() == ".ix") {
							FileOutput			file(fn);
							ISO::ScriptWriter	writer(file);
							writer.SetFlags(ISO::SCRIPT_IGNORE_DEFER);
							writer.DumpDefs(b);
							writer.DumpType(b.GetTypeDef());
							writer.putc(' ');
							writer.DumpData(b);
							//ISO::ScriptWriter(FileOutput(fn)).SetFlags(ISO::SCRIPT_VIRTUALS).DumpData(b);
						} else {
							if (!FileHandler::Write(b, fn)) {
								if (b.GetTypeDef()->SameAs<anything>()) {
									HierarchyProgress	progress(WindowPos(*this, Rect(0, 0, 1000, 100)), "progress");
									WriteDirectory(fn, b, progress);
								} else
									throw_accum("Failed to write " << fn);
							}
							//FileHandler::Write(b, fn);
						}
					} catch (const char *s) {
						ModalError(s);
					}
					return 0;
				}

				case ID_ENTITY_EXPANDAS:
					if (FileHandler *fh = (FileHandler*)menu_item.Param()) try {
						Busy			bee;
						IsoEditorCache	cache;
						TreeControl		tree	= treecolumn.GetTreeControl();
						HTREEITEM		h		= context_item;
						ISO::Browser2	b		= GetBrowser(h);

						b	= VirtualDeref(b);

						if (b.GetType() == ISO::REFERENCE) {
							ISO_ptr<void>	*pp	= b;

						#if 1
							b	= *b;
							if (auto b1 = *b)
								b = b1;
							ISO_ptr<void>	p	= b;

							if (const char *fn = b.External()) {
								p = fh->ReadWithFilename(p.ID(), fn);
							} else {
								p = fh->Read(pp->ID(), b.Is("BigBin") ? istream_ref(BigBinStream(b)) : istream_ref(BinStream(b)));
							}
						#else
							ISO_ptr<void>	*pp	= b;
							ISO_ptr<void>	p	= *pp;

							if (p.IsExternal())
								p = fh->ReadWithFilename(p.ID(), (const char*)p);
							else
								p = fh->Read(p.ID(), memory_reader(GetRawData(p)));
						#endif
							if (p)
								Do(MakeModifyPtrOp(tree, h, *pp, p));
						} else {
							if (ISO_ptr<void> p = fh->Read(b.GetName(), b.Is("BigBin") ? istream_ref(BigBinStream(b)) : istream_ref(BinStream(b))))
								AddEntry(p, false);
						}
					} catch (const char *s) {
						ModalError(s);
					}
					return 0;

				case ID_ENTITY_VIEWAS:
					if (FileHandler *fh = (FileHandler*)menu_item.Param()) try {
						Busy			bee;
						IsoEditorCache	cache;
						TreeControl		tree	= treecolumn.GetTreeControl();
						HTREEITEM		h		= context_item;
						ISO::Browser2	b		= GetBrowser(h);
						tag2			id		= b.GetName();

						b		= SkipPointer(b);
						if (ISO_ptr<void> p	= fh->Read(id, b.Is("BigBin") ? istream_ref(BigBinStream(b)) : istream_ref(BinStream(b)))) {
							ISO::Browser2	b(p);
							if (Editor *ed = Editor::Find(b))
								ed->Create(*this, Docker(this).Dock(DOCK_RIGHT | DOCK_EXTEND | DOCK_DOMINANT | DOCK_FIXED_SIB | DOCK_FIXED | DOCK_OR_TAB, 480), ISO_VirtualTarget(b));
							else
								AddEntry(p, false);
						}
					} catch (const char *s) {
						ModalError(s);
					}
					return 0;

				case ID_ENTITY_EXPANDDIR: {
					int				mode	= menu_item.Param();
					TreeControl		tree	= treecolumn.GetTreeControl();
					HTREEITEM		h		= context_item;
					ISO::Browser2	b		= GetBrowser(h);
					ISO_ptr<void>	*pp		= b;
					ISO_ptr<void>	p		= *pp;

					Busy			bee;
					IsoEditorCache	cache;
					string			errors;

					if (SkipPointer(b).SkipUser().GetType() == ISO::VIRTUAL) {
						p = UnVirtualise(b);
						ExpandChildren(ISO::Browser(p), errors, mode);
						Do(MakeModifyPtrOp(tree, h, *pp, p));
					} else {
						ExpandChildren(b, errors, mode);
					}
					if (errors)
						ModalError(errors);
					tree.Invalidate();
					return 0;
				}

				case ID_ENTITY_EXPANDALL: {
					struct expander {
						ISO_ptr<void>	proot;
						HTREEITEM		hroot;
						int				depth;

						bool operator()(TreeControl	tree, HTREEITEM hItem) {
							int	i = depth;
							for (HTREEITEM h = hItem; h != hroot; h = tree.GetParentItem(h)) {
								if (--i == 0)
									return false;
							}
							ISO::Browser2	b = ISOTree(tree).GetBrowser(proot, hItem).SkipUser();
							if (b.GetType() == ISO::REFERENCE) {
								ISO_ptr<void>	p = b;
								ISO_TRACEF("expand %p\n", (void*)p);
								if (p && (p.Flags() & ISO::Value::TEMP_USER))
									return false;
								p.SetFlags(ISO::Value::TEMP_USER);
							}
							tree.ExpandItem(hItem);
							return true;
						}

						expander(HTREEITEM _hroot, ISO_ptr<void> _proot, int _depth) : proot(_proot), depth(_depth) {}
					};
					Busy		bee;
					TreeControl	tree	= treecolumn.GetTreeControl();
					HTREEITEM	item	= context_item;
					tree.ExpandItem(item);
					ISO::Browser2(root_ptr).ClearTempFlags();
					save(max_expand, -100), tree.Enum(item, expander(item, root_ptr, 3));
					return 0;
				}

				case ID_ENTITY_UPLOAD: {
					TreeControl	tree	= treecolumn.GetTreeControl();
					ISO::Browser	b		= GetBrowser(tree.GetSelectedItem()).SkipUser();
					if (b.GetType() == ISO::VIRTUAL && b.Count() == 1)
						b = b[0];

					if (b.GetType() == ISO::REFERENCE) try {
						Busy			bee;
						IsoEditorCache	cache;
						const char *exportfor0	= ISO::root("variables")["exportfor"].GetString();
						const char *platform	= GetPlatform(menu_item.text);//(const char*)menu_item.Param();
						if (!platform)
							throw_accum(menu_item.text << " is not responding");

						const char *exportfor1	= str(platform) == "pc" ? "dx11" : platform;
						auto	t			= iso::Platform::Set(exportfor1);
						bool	bigendian	= !!(t & iso::Platform::_PT_BE);
						int		save_mode	= ISO::binary_data.SetMode(1);
						ISO::root("variables").SetMember("exportfor", exportfor1);
						isolink_handle_t	h	= SendCommand(menu_item.text, (*b).Duplicate("Load"), bigendian);
						ISO::root("variables").SetMember("exportfor", exportfor0);
						ISO::binary_data.SetMode(save_mode);
						iso::Platform::Set(exportfor0);

						if (h == isolink_invalid_handle)
							throw isolink_get_error();
						isolink_close(h);
					} catch (const char *s) {
						ModalError(s);
					}
					return 0;
				}

				case ID_ENTITY_EDITSOURCE: {
					TreeControl		tree	= treecolumn.GetTreeControl();
					ISO_ptr<void>	p		= *GetBrowser(context_item).SkipUser();
					const char		*fn;
					int				line	= ISO::Tokeniser::GetLineNumber(p, &fn);
					if (!texteditor)
						MakeTextEditor(*this, Docker(this).Dock(DOCK_RIGHT | DOCK_EXTEND | DOCK_FIXED_SIB | DOCK_OR_TAB, 480), &texteditor);
					texteditor(WM_ISO_SET, line, LPARAM(fn));
					return 0;
				}

				case ID_ENTITY_FIND: {
					ISOTree				tree	= treecolumn.GetTreeControl();
					HTREEITEM			hItem	= context_item;
					route				r;
					tree.GetRoute(r, hItem);
					MakeFinderWindow(*this, GetBrowser(hItem), r);
					return 0;
				}

				case ID_ENTITY_COMPARE: {
					Busy			bee;
					ISO::Browser2	b2	= GetBrowser(treecolumn.GetTreeControl().GetSelectedItem());
					ISO::Browser2	b1	= GetBrowser(context_item);
#if 0
					MakeCompareWindow(*this, b1, b2);
#else
					hash_set<void*> done;
					AddEntry2(Differences(b1, b2, done), false);
#endif
					return 0;
				}

				case ID_ENTITY_SORT: {
					struct entry {
						string	name;
						int		index;
						int		children;
						entry(TreeControl tree, HTREEITEM h) : name(tree.GetItemText(h)), index(tree.GetItemParam(h)), children(tree.GetItem(h, TVIF_CHILDREN).Children()) {}
						bool	operator<(const entry &b) const {
							return	name[0] == '[' && b.name[0] == '[' ? index < b.index
								:	name[0] == '[' || (b.name[0] != '[' && name < b.name);
						}
					};

					ISOTree					tree	= treecolumn.GetTreeControl();
					HTREEITEM				hItem	= context_item;
					dynamic_array<entry>	entries;
					for (HTREEITEM hChild = tree.GetChildItem(hItem); hChild; hChild = tree.GetNextItem(hChild))
						entries.emplace_back(tree, hChild);

					sort(entries);
					tree.DeleteChildren(hItem);

					for (auto &i : entries) {
						tree.AddItem(hItem, TVI_LAST, i.name, i.index, i.children ? ISOTree::HAS_CHILDREN : ISOTree::NONE);
					}
					return 0;
				}

				case ID_ENTITY_PHYSICAL: {
					TreeControl		tree	= treecolumn.GetTreeControl();
					HTREEITEM		h		= context_item;
					ISO::Browser2	b		= GetBrowser(h);
					ISO_ptr<void>	p		= *b;

					if (void *phys = ISO::iso_bin_allocator().unfix(p.Header()->user)) {
						ISO_ptr<PhysicalMemory>	p2(p.ID(), memory_block(phys, ((uint32*)phys)[-1]));
						ISO::Browser2	b2(p2);
						if (Editor *ed = Editor::Find(b2))
							ed->Create(*this, Docker(this).Dock(DOCK_RIGHT | DOCK_EXTEND | DOCK_FIXED_SIB | DOCK_OR_TAB, 480), ISO_VirtualTarget(b2));
					}
					return 0;
				}

				case ID_ENTITY_REFRESH:
					Refresh(treecolumn.GetTreeControl(), root_ptr, TVI_ROOT);
					treecolumn.Invalidate();
					break;

				case ID_TAB_BREAKOFF: {
					TabControl2	tab(context_control);
					Control		c	= tab.GetItemControl(context_index);
					new SeparateWindow(c);
					tab.RemoveItem(context_index);
					return 0;
				}

				case ID_TAB_SPLIT: {
					TabControl2	tab1(context_control);
					TabWindow			*tab2	= new TabWindow;
					SplitterWindow		*split0	= SplitterWindow::Cast(tab1.Parent());
					TabSplitter			*split1	= new TabSplitter(split0->_GetPanePos(0), SplitterWindow::SWF_VERT | SplitterWindow::SWF_PROP);

					tab2->Create(*split1, NULL, CHILD | CLIPCHILDREN | VISIBLE, CLIENTEDGE);
					tab2->SetFont((HFONT)GetStockObject(DEFAULT_GUI_FONT));

					split0->SetPane(split0->WhichPane(tab1), *split1);
					split1->SetPanes(tab1, *tab2, 50);

					Control	c	= tab1.GetItemControl(context_index);
					tab1.RemoveItem(context_index);
					tab2->AddItemControl(c, 0);
					c.Show();
					return 0;
				}

				case ID_TAB_DELETE: {
					TabControl2	tab(context_control);
					tab.GetItemControl(context_index).Destroy();
					tab.RemoveItem(context_index);
					return 0;
				}

				default:
					if (editwindow && (lParam == 0 || find(toolbars, HWND(lParam)) != toolbars.end()))
						return editwindow(WM_COMMAND, wParam, lParam);
			}
			break;
		}

		case WM_MOUSEACTIVATE:
			SetAccelerator(*this, IDR_ACCELERATOR_MAIN);
			break;

		case WM_PARENTNOTIFY:
			if (wParam == WM_DESTROY) {
				HWND	child = (HWND)lParam;
				if (child == splitter.GetPane(1)) {
					Move(AdjustRectFromChild(treecolumn.GetRect()));
					if (child == tabcontrol)
						tabcontrol = TabControl2();

				} else {
					for (ToolBarControl *i = toolbars.begin(), *e = toolbars.end(); i != e; ++i) {
						if (*i == child) {
							Point	p	= i->GetRelativeRect().TopLeft();
							for (i = toolbars.erase(i), e = toolbars.end(); i != e; ++i) {
								i->Move(p);
								p.x += i->GetRect().Width();
							}
							break;
						}
					}
					break;
				}
			}
			break;

		case WM_NOTIFY: {
			NMHDR			*nmh	= (NMHDR*)lParam;
			TreeControl		tree	= treecolumn.GetTreeControl();
			HeaderControl	header	= treecolumn.GetHeaderControl();
			switch (nmh->code) {
				case TCN_GETDISPINFO: {
					NMTCCDISPINFO *nmdi = (NMTCCDISPINFO*)nmh;
					return treecolumn_display.Display(root_ptr, tree, nmdi);
				}
				case TVN_ITEMEXPANDING:
					try {
						treecolumn_display.Expanding(*this, root_ptr, tree, (NMTREEVIEW*)nmh, max_expand);
					} catch (const char *s) {
						ModalError(s);
					}
					return 0;

				case TVN_BEGINDRAG:
					BeginDrag(((NMTREEVIEW*)nmh)->itemNew.hItem, ((NMTREEVIEW*)nmh)->ptDrag);
					return 0;

				case TVN_SELCHANGED: {
					if (miniedit.hWnd) {
						::SetFocus(NULL);
						miniedit.Destroy();
					}

					NMTREEVIEW		*nmtv = (NMTREEVIEW*)nmh;
					context_item	= nmtv->itemNew.hItem;

					if (editwindow) {
						if (ISO_VirtualTarget v = GetVirtualTarget(context_item))
							editwindow(WM_COMMAND, MAKEWPARAM(ID_EDIT_SELECT, 0), &v);
					}
					selchanged = true;
					return 0;
				}

				case TCN_SELCHANGE: {
					TabControl2	tab(nmh->hwndFrom);
					tab.ShowSelectedControl();
					Control	c = tab.GetSelectedControl();
					c.Update();
					SetEditWindow(c);
					Invalidate();
					return 0;
				}

				case TBN_DROPDOWN: {
					NMTOOLBAR	*nmtb	= (NMTOOLBAR*)nmh;
					ToolBarControl	tb(nmh->hwndFrom);
					tb.GetItem(nmtb->iItem).Param<Menu>().Track(*this, tb.ToScreen(Rect(nmtb->rcButton).BottomLeft()));
					return 0;
				}

				case TBN_HOTITEMCHANGE: {
					NMTBHOTITEM	*nmhi = (NMTBHOTITEM*)nmh;
					if (!(nmhi->dwFlags & HICF_LEAVING))
						EndMenu();
					return 0;
				}

				case NM_CUSTOMDRAW: {
					NMCUSTOMDRAW *nmcd = (NMCUSTOMDRAW*)nmh;
					if (nmcd->dwDrawStage == CDDS_POSTPAINT && nmh->hwndFrom == tree) {
						treecolumn_display.PostDisplay(treecolumn);
						return 0;
					}
					break;
				}

				case NM_CLICK:
					if (nmh->hwndFrom == tree) {
						int			subitem = -1;
						HTREEITEM	h		= treecolumn.HitTest(treecolumn.ToClient(GetMessageMousePos()), subitem);
						if (subitem == 2)
							LeftClick(h);
						else
							tree.SetSelectedItem(h);
						return 1;
					}
					break;

				case NM_RCLICK: {
					Point		mouse = GetMousePos();
					if (nmh->hwndFrom == tree) {
						RightClick(tree.HitTest(tree.ToClient(mouse)));
					} else if (nmh->hwndFrom == header) {
						HeaderControl::Item().Width(treecolumn_display.col0width + 1).Set(header, 0);
					} else if (Control(nmh->hwndFrom).Class().name() == TabControl::ClassName()) {
						TabControl	tab(nmh->hwndFrom);
						context_control	= tab;
						context_index	= tab.HitTest(tab.ToClient(mouse));
						Menu(IDR_MENU_SUBMENUS).GetSubMenuByName("Tabs").Track(*this, GetMousePos());
					}
					return 0;
				}

				//tabs
				case TCN_MBUTTON: {
					TabControl2	tab(nmh->hwndFrom);
					int					i = nmh->idFrom;
					Control				c = tab.GetItemControl(i);
					c.Destroy();
					tab.RemoveItem(i);
					if (tab)
						SetEditWindow(tab.GetSelectedControl());
					return 0;
				}
				//tooltip
				case TTN_GETDISPINFO: {
					TOOLTIPTEXT	*ttt	= (TOOLTIPTEXT*)nmh;
					ttt->hinst			= GetDefaultInstance();
					ttt->lpszText		= MAKEINTRESOURCE(ttt->hdr.idFrom);
					return 0;
				}
			}
			break;
		}

		case WM_ISO_JOB:
			job((void*)wParam, (void(*)(void*))lParam)();
			return 0;

		case WM_NCDESTROY:
			delete this;
			PostQuitMessage(0);
			return 0;
	}
	return Super::Proc(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	MainWindow
//-----------------------------------------------------------------------------

MainWindow		*MainWindow::me;

MainWindow::MainWindow() : SeparateWindow(48 | DockingWindow::FIXED_CHILD)	{ me = this; }
MainWindow::~MainWindow()						{ me = 0; }

void MainWindow::AddView(Control c)						{}
void MainWindow::SetTitle(const char *title)			{}
void MainWindow::AddLoadFilters(cmulti_string filters)	{}

//-----------------------------------------------------------------------------
//	IsoEditor
//-----------------------------------------------------------------------------

void IsoEditor::ModalError(const char *s) {
	MessageBoxA(*this, s, "IsoEditor", MB_ICONERROR | MB_OK);
}

IsoEditor::IsoEditor() : max_expand(1024) {
	AppEvent(AppEvent::PRE_GRAPHICS).send();

	InitCommonControls();
	isolink_init();

	ISO::root().Add(ISO_ptr<Resources>("data", "ISOPOD"));
	root_ptr.Create();

	Settings().values()["max_expand"].get(max_expand);
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

char *ParseCommandLine(char *f, filename &fn) {
	for (;;) {
		char	c;
		do c = *f++; while (c && c == ' ');
		if (!c)
			return 0;

		char	*d		= fn;
		int		depth	= 0;
		int		dquote	= 1, quote	= 1;
		do {
			*d++ = c;
			c = *f++;
			switch (c) {
				case '"':	depth += dquote; dquote = -dquote; break;
				case '\'':	depth += quote; quote = -quote; break;
				case '(':
				case '{':
				case '[':	depth++; break;
				case ')':
				case '}':
				case ']':	depth--; break;
			}
		} while (c && (depth || c != ' '));

		f -= int(c == 0);
		*d = 0;

		if (char *equals = strchr(fn, '=')) {
			*equals++ = 0;
			if (ISO::Browser b = ISO::root(fn)) {
				b.Append().Set(FileHandler::Read(filename(equals).name(), filename(equals)));
			} else if (*equals == '{') {
				ISO::root("variables").Append().Set(ISO::ScriptRead(fn, 0, memory_reader(equals), 0));
			} else {
				ISO::root("variables").SetMember(fn, string(equals));
			}
		} else if (fn[0]) {
			return f;
		}
	}
}

void stream_lister(const char *dir) {
	for (directory_iterator d(filename(dir).add_dir("*.*")); d; ++d) {
		filename fn = filename(dir).add_dir(d);
		WIN32_FIND_STREAM_DATA	find;
		HANDLE	h = FindFirstStreamW(str16(fn), FindStreamInfoStandard, &find, 0);
		if (h != INVALID_HANDLE_VALUE) {
			while (FindNextStreamW(h, &find)) {
				trace_accum("Stream for %s - %S (size = %I64u)\n", (char*)fn, find.cStreamName, find.StreamSize);
			}
			FindClose(h);
		}

		if (d.is_dir() && d[0] != '.')
			stream_lister(fn);
	}
}

void StartWebServer(const char *dir, PORT port, bool secure);
void DiskCopy(int f, int t);
void GetFTP(const char *host, const char *user, const char *password, const char *infile, const char *outfile);
#undef GetEnvironmentStrings

ISO::Browser2 app::GetSettings(const char *path) {
	static ISO::PtrBrowser	settings = GetRegistry("settings", HKEY_CURRENT_USER, "Software\\Isopod\\OrbisCrude");
	return settings.Parse(path, false);
}

void *NativeWrapMethod();
void WindowsComposition(HWND h);


int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdline, int nCmdShow) {
	ApplyHooks();

#ifdef _MSC_VER
	ISO_TRACEF("_MSC_VER = ") << _MSC_VER << '\n';
#endif

#if 0
	RoInitialize(RO_INIT_MULTITHREADED);
	composition::init();
	auto	xml = LR"--(
<Button
	xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
	xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
	xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
	xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
	mc:Ignorable="d"
	Content="Kbt1">
	<Button.KeyboardAccelerators>
		<KeyboardAccelerator Key="K" Modifiers="Control" />
	</Button.KeyboardAccelerators>
</Button>
	)--";

	auto	button = composition::LoadXamlControl(xml);
#else
	OleInitialize(NULL);
#endif


//	init_com();

	auto	hr = CoInitializeSecurity(
		NULL,
		-1,								// COM negotiates service
		NULL,							// Authentication services
		NULL,							// Reserved
		RPC_C_AUTHN_LEVEL_CONNECT,		// Default authentication
		RPC_C_IMP_LEVEL_IMPERSONATE,	// Default Impersonation
		NULL,							// Authentication info
		EOAC_NONE,						// Additional capabilities
		NULL							// Reserved
	);



#if	_WIN32_WINNT >= 0x0A00
//	SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#else
	SetProcessDPIAware();
#endif

//	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_CHECK_ALWAYS_DF);
	StartWebServer("D:\\dev\\orbiscrude\\website", 1080, false);
	StartWebServer("D:\\dev\\orbiscrude\\website", 1443, true);

	Sleep(1000);

//	auto	h = HTTPopenURL("isoeditor", "https://localhost:1443");
	{
		WebSocketClient	wsc("isoeditor", "ws://127.0.0.1:1080");
		wsc.SendPing("ping");
		wsc.SendText("Hola!");
		wsc.Process();
	}

	ISO::allocate_base::flags.set(ISO::allocate_base::TOOL_DELETE);
	ISO::root().Add(ISO_ptr<anything>("externals"));

	dynamic_array<char*>	envp;
	for (char *p = GetEnvironmentStrings(); *p; p += strlen(p) + 1) {
		if (*p != '=')
			envp.push_back(p);
	}
	envp.push_back((char*)0);
	ISO::root().Add(ISO::GetEnvironment("environment", envp));

	ISO_ptr<anything>	variables("variables");
	ISO::root().Add(variables);
	variables->Append(ISO_ptr<int>("isoeditor",		1));
	variables->Append(ISO_ptr<int>("raw",			1));
	variables->Append(ISO_ptr<string>("exportfor",	"dx11"));

	{
		RegKey	reg0	= Settings();
		if (!reg0.HasSubKey("defaults")) {
			RegKey	reg(reg0, "defaults");
			reg.values()["keepexternals"] = uint32(1);
		}
		RegKey	reg(reg0, "defaults");
		for (auto v : reg.values()) {
			switch (v.type) {
				case RegKey::sz:		variables->Append(ISO_ptr<string>(v.name, v)); break;
				case RegKey::uint32:	variables->Append(ISO_ptr<uint32>(v.name, v.get<uint32>())); break;
			}
		}
	}

	IsoEditor	*editor = 0;
	WINDOWPLACEMENT	wp = {0};
	Control		control;
	filename	fn;
	Rect		rect(CW_USEDEFAULT, CW_USEDEFAULT, 320, 640);

	for (char *p = cmdline; p = ParseCommandLine(p, fn);) {
		if (fn == "-u") {
			if (!control)
				control = FindWindowA(0, "IsoEditor");
		} else if (fn == "-winpos") {
			if (scan_string(str<256>(Settings().values()["window"].get_text()),
				"%i,%i,%i,%i,%i,%i,%i",
				&wp.showCmd, &wp.ptMaxPosition.x, &wp.ptMaxPosition.y,
				&wp.rcNormalPosition.left, &wp.rcNormalPosition.right, &wp.rcNormalPosition.top, &wp.rcNormalPosition.bottom
			) == 7) {
				rect		= wp.rcNormalPosition;
				wp.length	= sizeof(wp);
				wp.flags	= 0;
			}
		} else {
			if (!control) {
				editor	= new IsoEditor2(rect);
				control	= *editor;
			}
			if (fn[0] != '-') {
				if (fn[0] == '+')
					fn = filename("+" + get_cwd().relative(fn.begin() + 1));
				else
					fn = get_cwd().relative(fn);
			}
			COPYDATASTRUCT	cds = {0, sizeof(fn), &fn};
			control(WM_COPYDATA, 0, (LPARAM)&cds);
		}
	}
	if (!editor) {
		if (control)
			return 0;
		editor = new IsoEditor2(rect);
	}

#if 0
	composition::CreateDesktopWindowsXamlSource(*editor, composition::LoadXamlControl(xml));
#endif

//	for (directory_iterator plugins(get_exec_path().relative("*.ipi")); plugins; ++plugins)
//		LoadLibrary((filename)plugins);

	for (Plugin::iterator i = Plugin::begin(), e = Plugin::end(); i != e; ++i)
		ISO_TRACEF(i->GetDescription()) << '\n';

	// http://answers.microsoft.com/en-us/windows/forum/windows_7-files/file-drag-n-drop-via-wmdropfiles-doesnt-work-in/d172ed8c-1a5b-e011-8dfc-68b599b31bf5
	#define WM_COPYGLOBALDATA  0x49
	ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
	ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
	ChangeWindowMessageFilter(WM_COPYGLOBALDATA, MSGFLT_ADD);


#if 1
	while (ProcessMessage(true));
#else
	HANDLE	h[] = { JobQueue::Main() };
	while (uint32 r = ProcessMessage(h)) {
		if (r == 2)
			JobQueue::Main().dispatch();
	}
#endif

	(&ISO::root())->Clear();
	return 0;
}

