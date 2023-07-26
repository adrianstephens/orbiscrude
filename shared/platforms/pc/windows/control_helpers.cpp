#include "control_helpers.h"
#include "dialog.h"
#include "splitter.h"
#include "common.rc.h"
//#include "resource.h"

using namespace iso;
using namespace win;

//-----------------------------------------------------------------------------
//	_GetValueDialog
//-----------------------------------------------------------------------------

class _GetValueDialog : public Dialog<_GetValueDialog> {
	fixed_string<1024>	&value;
public:
	LRESULT	Proc(MSG_ID msg, WPARAM wParam, LPARAM lParam) {
		switch (msg) {
			case WM_INITDIALOG:
				Item(ID_EDIT).SetText(value);
				SetFocus(ID_EDIT);
				break;
			case WM_COMMAND: {
				switch (LOWORD(wParam)) {
					case IDOK: {
						value = Item(ID_EDIT).GetText();
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
	_GetValueDialog(fixed_string<1024> &_value) : value(_value) {}
	bool	operator()(HWND hWndParent) {
		DialogBoxCreator	dc(Rect(0, 0, 276,46), "IsoEditor", POPUP|CAPTION|SYSMENU|DS_SETFONT|DS_MODALFRAME|DS_FIXEDSYS);
		dc.SetFont(Font::Caption());
		dc.AddControl(Rect(6, 2, 264, 12),	DLG_STATIC,	"Value");
		dc.AddControl(Rect(6, 14, 264, 12), DLG_EDIT,	"", ID_EDIT, BORDER | EditControl::AUTOHSCROLL);
		dc.AddControl(Rect(6, 28, 50, 14),	DLG_BUTTON,	"OK", IDOK, Button::DEFPUSHBUTTON);
		dc.AddControl(Rect(60, 28, 50, 14), DLG_BUTTON,	"Cancel");

		return Modal(hWndParent, dc.GetTemplate()) != 0;
	}
};

bool win::GetValueDialog(HWND hWndParent, fixed_string<1024> &value) {
	return _GetValueDialog(value)(hWndParent);
}

const char *win::GetValueDialog(HWND hWndParent, text _value) {
	static fixed_string<1024>	value;
	value = _value;
	return _GetValueDialog(value)(hWndParent) ? (const char*)value : 0;
}

//-----------------------------------------------------------------------------
//	EditControl2
//-----------------------------------------------------------------------------

LRESULT EditControl2::Proc(MSG_ID msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE:
			owner = Parent();
			break;
		case WM_KEYDOWN:
			switch (wParam) {
				case VK_TAB:
					owner(WM_CHAR, (GetKeyState(VK_SHIFT) & 0x8000) | '\t');
					return 0;
				case VK_ESCAPE:
					owner(WM_CHAR, 27);
					return 0;
			}
			break;
		case WM_CHAR:
			if (wParam == '\r') {
				owner(msg, wParam, lParam);
				return 0;
			}
			break;

		case WM_MOUSEACTIVATE:
			SetFocus();
		case WM_SETFOCUS: {
			int	r = Super(msg, wParam, lParam);
			SetAccelerator(0, 0);
			return r;
		}
		case WM_KILLFOCUS:
			owner(WM_COMMAND, MAKEWPARAM(id.get(), EN_KILLFOCUS), (LPARAM)hWnd);
			return 0;

		case WM_NCDESTROY:
			hWnd = 0;
			break;
	}
	return Super(msg, wParam, lParam);
}

void EditLabelCommon(EditControl2 &edit, const WindowPos &wpos, string_param &&text, ID id) {
	edit.Create(wpos, text,
		Control::CHILD | Control::VISIBLE | Control::CLIPSIBLINGS | EditControl::AUTOHSCROLL | EditControl::WANTRETURN, Control::CLIENTEDGE,
		id
	);
	edit.SetFont(wpos.Parent().GetFont());
	edit.MoveAfter(HWND_TOP);
	edit.SetSelection(CharRange::all());
	edit.SetFocus();
}

void win::EditLabel(ListViewControl lv, EditControl2 &edit, int i, int col, ID id) {
	EditLabelCommon(edit, WindowPos(lv, lv.GetSubItemRect(i, col, ListViewControl::LABEL).Grow(0, 1, 0, 1)), str<256>(lv.GetItemText(i, col)), id);
}

void win::EditLabel(TreeControl tc, EditControl2 &edit, HTREEITEM h, int offset, ID id) {
	auto	text	= str<256>(tc.GetItemText(h));
	int		xoffset = offset ? DeviceContext(tc).SelectContinue(tc.GetFont()).GetTextExtent(text, offset).x : 0;
	EditLabelCommon(edit, WindowPos(tc, tc.GetItemRect(h, true).Grow(4 - xoffset, 1, 16, 1)), text.slice(offset), id);
}

void win::EditLabel(TreeColumnControl tc, EditControl2 &edit, HTREEITEM h, int col, ID id) {
	EditLabelCommon(edit, WindowPos(tc.GetTreeControl(), tc.GetItemRect(h, col, true, true).Grow(0, 1, 0, 1)), str<256>(tc.GetItemText(h, col)), id);
}

//-----------------------------------------------------------------------------
//	TextFinder
//-----------------------------------------------------------------------------

bool win::FindNextText(EditControl c, const char *search, int flags) {
	FINDTEXTEXA	find;
	find.lpstrText  = search;
	find.chrg		= c.GetSelection();

	if (flags & FR_DOWN) {
		find.chrg.cpMin	= min(find.chrg.cpMin, find.chrg.cpMax) + 1;
		find.chrg.cpMax	= -1;
	} else {
		find.chrg.cpMin	= max(find.chrg.cpMin, find.chrg.cpMax) - 1;
		find.chrg.cpMax	= 0;
	}

	int	found = c(EM_FINDTEXTEX, flags, (LPARAM)&find);
	if (found >= 0) {
		c.SetSelection(CharRange(found, found + string_len32(search)));
		c(EM_HIDESELECTION, 0);
		c(EM_SCROLLCARET);
		return true;
	}
	return false;
}

//UINT_PTR CALLBACK DummyDialogProc(HWND hWnd, MSG_ID msg, WPARAM wParam, LPARAM lParam) {
//	switch (msg) {
//		case WM_INITDIALOG:
//			return 1;
//		case WM_NCDESTROY:
//			ISO_TRACE("delete t\n");
//			break;
//	}
//	return 0;
//}

TextFinder::TextFinder(HWND hWnd) {
	clear(*this);
	lStructSize	= sizeof(FINDREPLACEA);
	hwndOwner	= hWnd;
	//hInstance	= hinstance;
	Flags			= FR_DOWN;
	lpstrFindWhat	= ss;
	//lpstrReplaceWith;
	wFindWhatLen		= 256;
	//wReplaceWithLen;
	//lCustData		= (LPARAM)this;
	//lpfnHook		= &DummyDialogProc;
	//lpTemplateName;
	Control	c = FindTextA(this);
	//c.user = this;
	SetModelessDialog(c);
}

TextFinder *TextFinder::CheckMessage(MSG_ID msg, WPARAM wParam, LPARAM lParam) {
	static UINT WM_FINDMSGSTRING = RegisterWindowMessageA(FINDMSGSTRINGA);
	if (msg == WM_FINDMSGSTRING) {
		TextFinder	*t = (TextFinder*)lParam;
		if (!(t->Flags & FR_DIALOGTERM))
			return t;
	} else if (msg == WM_PARENTNOTIFY) {
		if (LOWORD(wParam) == WM_DESTROY) {
			TextFinder	*t = Control((HWND)lParam).user;
			delete t;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	HeaderControl
//-----------------------------------------------------------------------------

void win::SetColumnWidths(HeaderControl c) {
	DeviceContext		dc(c);
	dc.Select(c.GetFont());

	for (int i = 0, n = c.Count(); i < n; i++) {
		uint32	w = c.GetItemRect(i).Width();
		uint32	e = dc.GetTextExtent(c.GetItemText<256>(i)).x;
		if (w < e)
			c.GetItem(i).Width(e).Set(c, i);
	}
}

//-----------------------------------------------------------------------------
//	ListView Helpers
//-----------------------------------------------------------------------------

bool win::CheckListView(ListViewControl list, int row, int column) {
	int	col;
	return list.HitTest(list.ToClient(GetMousePos()), col) == row && col == column;
}

//-----------------------------------------------------------------------------
//	Tree Helpers
//-----------------------------------------------------------------------------

void win::HighLightTree(TreeControl tree, HTREEITEM	prev, uint32 val) {
	while (prev) {
		TreeControl::Item(prev).SetState(INDEXTOSTATEIMAGEMASK(val)).Set(tree);
		prev = tree.GetParentItem(prev);
	}
}

void win::HighLightTree(TreeControl tree, uint32 addr, uint32 val) {
	HTREEITEM	prev	= 0;
	for (HTREEITEM h = tree.GetRootItem(); h; h = tree.GetNextItem(h)) {
		uint32	p = tree.GetItemParam(h);
		if (p > addr)
			h = tree.GetChildItem(prev);
		if (h)
			prev = h;
	}
	while (prev) {
		TreeControl::Item(prev).SetState(INDEXTOSTATEIMAGEMASK(val)).Set(tree);
		prev = tree.GetParentItem(prev);
	}
}

HTREEITEM win::FindOffset(TreeControl tree, HTREEITEM h, uint32 offset, bool stop_equal) {
	uint32	poff	= 0;
	for (HTREEITEM	p = h; p; ) {
		h		= p;
		p		= 0;
		uint32	parent_off = poff, off	= 0;
		for (HTREEITEM c = tree.GetChildItem(h); c; poff = off, p = c, c = tree.GetNextItem(c)) {
			off	= tree.GetItemParam(c);
			if (off == 0)	// ignore 0
				continue;
			if (stop_equal && off == offset)
				return c;
			if (off > offset) {
				if (auto c2 = tree.GetChildItem(c)) {
					if ((uint32)tree.GetItemParam(c2) < off) {
						p		= c;
						poff	= off;
					}
				}
				break;
			}
		}
	}
	return h;
}

dynamic_array<arbitrary> win::GetParamHierarchy(TreeControl tree, HTREEITEM hItem) {
	dynamic_array<arbitrary> a;
	for (auto h = hItem; h; h = tree.GetParentItem(h))
		a.push_back(tree.GetItemParam(h));
	return a;
}

//-----------------------------------------------------------------------------
//	Drag Helpers
//-----------------------------------------------------------------------------

CLIPFORMAT DropTargetHelpers::single_format(IDataObject* pDataObj) {
	com_ptr<IEnumFORMATETC> e;
	if (SUCCEEDED(pDataObj->EnumFormatEtc(DATADIR_GET, &e))) {
		FORMATETC	fmt[2];
		ulong		n;
		if (SUCCEEDED(e->Next(2, fmt, &n)) && n == 1 && fmt[0].tymed == TYMED_HGLOBAL)
			return fmt[0].cfFormat;
	}
	return 0;
}

HGLOBAL	DropTargetHelpers::has_format(IDataObject* pDataObj, CLIPFORMAT format) {
	FORMATETC	fmt = {format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	STGMEDIUM	med;
	return SUCCEEDED(pDataObj->GetData(&fmt, &med)) ? med.hGlobal : 0;
}

int DropTargetHelpers::has_filename(IDataObject* pDataObj, filename &fn) {
	FORMATETC	fmt = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	STGMEDIUM	med;

	if (SUCCEEDED(pDataObj->GetData(&fmt, &med))) {
		HDROP	hDrop	= (HDROP)med.hGlobal;
		int		nf		= DragQueryFile(hDrop, -1, 0, 0);
		DragQueryFileA(hDrop, 0, fn, sizeof(filename));
		return nf;
	}

	fmt.cfFormat	= RegisterClipboardFormat(CFSTR_FILECONTENTS);
	fmt.tymed		= TYMED_HGLOBAL | TYMED_ISTREAM | TYMED_ISTORAGE;
	fmt.lindex		= 0;
	if (SUCCEEDED(pDataObj->GetData(&fmt, &med))) {
		if (med.tymed & TYMED_ISTREAM) {
			STATSTG stg = {0};
			if (SUCCEEDED(med.pstm->Stat(&stg, STATFLAG_DEFAULT))) {
				fn = stg.pwcsName;
				CoTaskMemFree(stg.pwcsName);
				return 1;
			}
		}
	}
	return 0;
}

dynamic_array<filename>	DropTargetHelpers::has_filenames(IDataObject* pDataObj) {
	dynamic_array<filename>	fns;

	FORMATETC	fmt = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	STGMEDIUM	med;

	if (SUCCEEDED(pDataObj->GetData(&fmt, &med))) {
		HDROP	hDrop	= (HDROP)med.hGlobal;
		int		nf		= DragQueryFile(hDrop, -1, 0, 0);
		fns.resize(nf);
		for (int i = 0; i < nf; i++)
			DragQueryFileA(hDrop, i, fns[i], sizeof(filename));

	} else {
		fmt.cfFormat	= RegisterClipboardFormat(CFSTR_FILECONTENTS);
		fmt.tymed		= TYMED_HGLOBAL | TYMED_ISTREAM | TYMED_ISTORAGE;
		fmt.lindex		= 0;
		if (SUCCEEDED(pDataObj->GetData(&fmt, &med))) {
			if (med.tymed & TYMED_ISTREAM) {
				STATSTG stg = {0};
				if (SUCCEEDED(med.pstm->Stat(&stg, STATFLAG_DEFAULT))) {
					fns.push_back(stg.pwcsName);
					CoTaskMemFree(stg.pwcsName);
				}
			}
		}
	}
	return fns;
}

ImageList win::BeginDrag(Control parent, ImageList drag, const Point &pt) {
	drag.DragBegin(0, Point(0, 0));
	ImageList::DragEnter(Control::Desktop(), pt);
	SetCapture(parent);
	return drag;
}

// on LVN_BEGINDRAG:
ImageList win::BeginDrag(Control parent, ListViewControl lv, const Point &pt) {
	ImageList	drag;
	for (auto i : lv.Selected()) {
		Point		top_left;
		ImageList	il = lv.CreateDragImage(i, top_left);
		if (drag)
			drag = ImageList_Merge(drag, 0, il, 0, 0, drag.GetRect(0).Bottom());
		else
			drag = il;
	}
	return BeginDrag(parent, drag, pt);
}

// on TVN_BEGINDRAG:
ImageList win::BeginDrag(Control parent, TreeControl tc, HTREEITEM h, const Point &pt) {
	return BeginDrag(parent, tc.CreateDragImage(h), pt);
}

ImageList win::BeginDrag(Control parent, TreeControl tc, const Point &pt) {
	ImageList	drag;
	for (auto i : tc.Selected()) {
		ImageList	il = tc.CreateDragImage(i);
		if (drag)
			drag = ImageList_Merge(drag, 0, il, 0, 0, drag.GetRect(0).Bottom());
		else
			drag = il;
	}

	drag.DragBegin(0, Point(0, 0));
	ImageList::DragEnter(Control::Desktop(), pt);

	SetCapture(parent);
	return drag;
}

// on WM_LBUTTONUP:
void win::StopDrag(Control c, ImageList drag) {
	// End the drag-and-drop process
	ImageList::DragLeave(c);
	ImageList::DragEnd();
	drag.Destroy();

	ReleaseCapture();
}

