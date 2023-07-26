#ifndef CONTROL_HELPERS_H
#define CONTROL_HELPERS_H

#include "base/algorithm.h"
#include "filename.h"
#include "window.h"
#include "splitter.h"
#include "treecolumns.h"
#include "com.h"
#include <mmsystem.h>
#include <shlobj.h>
#include <shellapi.h>

namespace iso { namespace win {

const char*	GetValueDialog(HWND hWndParent, text _value = none);
bool		GetValueDialog(HWND hWndParent, fixed_string<1024> &value);

//-----------------------------------------------------------------------------
//	ScrollInfo
//-----------------------------------------------------------------------------

template<typename T> struct TScrollInfo {
	T		min, max, page, pos;

	TScrollInfo()		{ clear(*this); }
	T	Pos()	const	{ return pos;	}
	T	Page()	const	{ return page;	}
	T	Min()	const	{ return min;	}
	T	Max()	const	{ return max;	}
	T	Scale() const	{ return div_round_up(max - min, T(0x7fffffff)); }

	T	MoveTo(T newpos) {
		T	oldpos	= pos;
		pos			= clamp(newpos, min, iso::max(max - page, 0));
		return oldpos - pos;
	}
	T	MoveTo(int32 pos, int32 range)	{ return MoveTo(mul_div(T(pos), max - min, T(range)) + min); }
	T	MoveBy(T d)						{ return MoveTo(pos + d); }
	T	SetMin(T newmin)				{ min	= newmin; return MoveTo(pos); }
	T	SetMax(T newmax)				{ max	= newmax; return MoveTo(pos); }
	T	SetPage(T newpage)				{ page	= newpage; return MoveTo(pos); }
	T	SetRange(T newmin, T newmax)	{ min	= newmin; max = newmax; return MoveTo(pos); }

	void Set(T newmin, T newmax) {
		min	= newmin;
		max	= newmax;
	}
	T	Set(T newmin, T newmax, T newpage) {
		Set(newmin, newmax < newpage ? newpage : newmax);
		return SetPage(newpage);
	}

	T	ProcessScroll(WPARAM wParam, int line = 1) {
		T	p	= pos;
		switch (LOWORD(wParam)) {
			case SB_LINELEFT:		p -= line;		break;
			case SB_LINERIGHT:		p += line;		break;
			case SB_PAGELEFT:		p -= page;		break;
			case SB_PAGERIGHT: 		p += page;		break;
			case SB_LEFT:			p = 0;			break;
			case SB_RIGHT:			p = max - page; break;
			//case SB_THUMBTRACK:		p = short(HIWORD(wParam)) * Scale() + min; break;
		}
		return MoveTo(p);
	}

	T	ProcessKey(WPARAM wParam, int line = 1) {
		T	p	= pos;
		switch (wParam) {
			case VK_PRIOR:			p -= page;	break;
			case VK_NEXT:			p += page;	break;
			case VK_UP:				p -= line;	break;
			case VK_DOWN:			p += line;	break;
			default: return 0;
		}
		return MoveTo(p);
	}
	void Set(const SCROLLINFO &si) {
		T	scale	= 1;
		if (si.fMask & SIF_TRACKPOS) {
			pos			= mul_div(T(si.nTrackPos - si.nMin), max - min, T(si.nMax - si.nMin)) + min;

		} else if (si.fMask & SIF_RANGE) {
			min		= si.nMin;
			max		= si.nMax;
			if (si.fMask & SIF_POS)
				pos	= si.nPos;
		} else {
			scale	= Scale();
			if (si.fMask & SIF_POS)
				pos	= si.nPos * scale + min;
		}
		if (si.fMask & SIF_PAGE)
			page	= si.nPage * scale;
	}
	operator SCROLLINFO() {
		SCROLLINFO	si;
		T	scale	= Scale();
		si.cbSize	= sizeof(SCROLLINFO);
		si.fMask	= SIF_ALL;
		si.nMin		= 0;
		si.nMax		= (max - min) / scale;
		si.nPos		= (pos - min) / scale;
		si.nPage	= page / scale;
		return si;
	}
	TScrollInfo& operator=(const SCROLLINFO &si) {
		Set(si);
		return *this;
	}
};

//-----------------------------------------------------------------------------
//	SortColumn
//-----------------------------------------------------------------------------

struct SortColumn {
	int	sort_col;

	SortColumn() : sort_col(0) {}
	int SetColumn(HeaderControl h, int col) {
		int					prev	= abs(sort_col) - 1;
		HeaderControl::Item	item(HDI_FORMAT);
		if (col == prev) {
			sort_col = -sort_col;
			item.Get(h, col);
			item.Format(item.Format() ^ HDF_SORTUP|HDF_SORTDOWN).Set(h, col);
		} else {
			sort_col = col + 1;
			item.Get(h, prev);
			item.Format(item.Format() & ~(HDF_SORTUP|HDF_SORTDOWN)).Set(h, prev);
			item.Get(h, col);
			item.Format(item.Format() | HDF_SORTDOWN).Set(h, col);
		}
		return sort_col < 0 ? -1 : 1;
	}
	bool	sorted()	const	{ return sort_col != 0; }
	bool	up()		const	{ return sort_col < 0; }
	int		column()	const	{ return abs(sort_col) - 1; }
	template<typename T> bool SortCompare(const T &a, const T &b) const { return sort_col < 0 ? (b < a) : (a < b); }
};


template<typename C> struct SortedListViewControl : ListViewControl, SortColumn {
	C		sorted_order;
	typedef reference_t<C> E;
	typedef reference_t<const C> constE;

	template<typename C2> SortedListViewControl(C2 &&c) : sorted_order(forward<C2>(c)) {}

	template<typename C2, typename P> void MakeSorted(C2 &&c, P &&p) {
		sorted_order = c;
		if (sorted())
			sort_dir(up(), sorted_order, [&p, col = column()](E a, E b) { return p(col, a, b); });
		SetCount(sorted_order.size32());
	}

	int SetColumn(int col) {
		return SortColumn::SetColumn(GetHeader(), col);
	}

	optional<constE> GetItem(int i) const {
		if (i < sorted_order.size())
			return sorted_order[i];
		return none;
	}
	uint32	NumItems() const { return sorted_order.size32(); }
};

struct ColumnSorter {
	int				dir;
	template<typename T> inline int compare(T a, T b) const { return a < b ? -dir : a > b ? dir : 0; }
	ColumnSorter(int dir) : dir(dir) {}
};

template<typename L> struct _LambdaColumnSorter : ColumnSorter {
	L				&&lambda;
	template<typename T> inline int operator()(T a, T b) const { return lambda(a, b) * dir; }
	_LambdaColumnSorter(L &&lambda, int dir) : ColumnSorter(dir), lambda(forward<L>(lambda)) {}
};
template<typename L> _LambdaColumnSorter<L> LambdaColumnSorter(L &&lambda, int dir) {
	return {forward<L>(lambda), dir};
}

template<typename T> struct _StructFieldSorter;
template<typename C, typename T> struct _StructFieldSorter<T C::*> : ColumnSorter {
	T C::*p;
	int	operator()(uint32 *a, uint32 *b) const { return ColumnSorter::compare(((C*)a)->*p, ((C*)b)->*p); }
	_StructFieldSorter(T C::*p, int dir) : ColumnSorter(dir), p(p) {}
};

template<typename T> _StructFieldSorter<T> StructFieldSorter(T t, int dir) {
	return {t, dir};
}

struct ColumnTextSorter : ColumnSorter {
	ListViewControl	list;
	int				col;
	int	operator()(uint32 a, uint32 b) const {
		int	d = numstring_cmp(str<256>(list.GetItemText(a, col)), str<256>(list.GetItemText(b, col)));
		return d < 0 ? -dir : d > 0 ? dir : 0;
		//return compare(str<256>(list.GetItemText(a, col)), str<256>(list.GetItemText(b, col)));
	}
	ColumnTextSorter(ListViewControl list, int col, int dir) : ColumnSorter(dir), list(list), col(col) {}
};

//-----------------------------------------------------------------------------
//	EditControl2
//-----------------------------------------------------------------------------

class EditControl2 : public Subclass<EditControl2, EditControl> {
	Control		owner;
public:
	LRESULT		Proc(MSG_ID msg, WPARAM wParam, LPARAM lParam);
	void		SetOwner(Control c) { owner = c; }
	template<typename T> void operator=(const T &t) const { SetText(to_string(t)); }
};

void EditLabel(ListViewControl lv, EditControl2 &edit, int i, int col, ID id);
void EditLabel(TreeControl tc, EditControl2 &edit, HTREEITEM h, int offset, ID id);
void EditLabel(TreeColumnControl tc, EditControl2 &edit, HTREEITEM h, int col, ID id);

//-----------------------------------------------------------------------------
//	ListViewControl2
//-----------------------------------------------------------------------------

struct ListViewControl2 : public ListViewControl {
	void Create(const WindowPos &wpos, const char *title, Style style = CHILD | CLIPSIBLINGS) {
		ListViewControl::Create(wpos, title, style | REPORT | NOSORTHEADER | SHOWSELALWAYS);
		SetExtendedStyle(GRIDLINES | FULLROWSELECT | DOUBLEBUFFER | ONECLICKACTIVATE);
	};
	ListViewControl2() {}
	ListViewControl2(const WindowPos &wpos, const char *title = "", Style style = CHILD | CLIPSIBLINGS) { Create(wpos, title, style); }
};

bool CheckListView(ListViewControl list, int row, int column);

//-----------------------------------------------------------------------------
//	CustomListView - mixin for editable items
//-----------------------------------------------------------------------------

template<class X, class B> struct CustomListView : B, refs<X> {
	typedef		CustomListView	Base;
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				addref();
				break;

			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case NM_CLICK: {
						NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
						static_cast<X*>(this)->LeftClick(nmlv->hdr.hwndFrom, nmlv->iItem, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
						return 0;
					}
					case NM_RCLICK: {
						NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
						static_cast<X*>(this)->RightClick(nmlv->hdr.hwndFrom, nmlv->iItem, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
						return 0;
					}
					case LVN_GETDISPINFOW: {
						auto	&item	= ((NMLVDISPINFOW*)nmh)->item;
						if (item.mask & LVIF_TEXT) {
							string_builder	b;
							static_cast<X*>(this)->GetDispInfo(b, item.iItem, item.iSubItem);
							string_copy(string_buffer16(item.pszText, item.cchTextMax), b.term());
							//string_buffer16(item.pszText, item.cchTextMax) = b.term();
						}
						return 1;
					}
					case LVN_GETDISPINFOA: {
						auto	&item = ((NMLVDISPINFO*)nmh)->item;
						if (item.mask & LVIF_TEXT)
							static_cast<X*>(this)->GetDispInfo(lvalue(fixed_accum(item.pszText, item.cchTextMax)), item.iItem, item.iSubItem);
						return 1;
					}
					case NM_CUSTOMDRAW:
						if (int r = static_cast<X*>(this)->CustomDraw((NMLVCUSTOMDRAW*)lParam))
							return r;
						break;
				}
				break;
			}
			case WM_NCDESTROY:
				release();
			case WM_DESTROY:
				return 0;
		}
		return Super(message, wParam, lParam);
	}

	void	RightClick(Control from, int row, int col, const Point &pt, uint32 flags)	{}
	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags)	{}
	int		CustomDraw(NMLVCUSTOMDRAW *cd)						{ return 0; }

	template<typename...P> CustomListView(P&&... p) : B(forward<P>(p)...) {}
};

//-----------------------------------------------------------------------------
//	EditableListView - mixin for editable items
//-----------------------------------------------------------------------------

template<class X, class B> struct EditableListView : B, refs<X> {
	typedef			EditableListView	Base;
	EditControl2	edit_control;
	int				edit_index, edit_col;

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				addref();
				break;

			case WM_CHAR:
				if (wParam == 27)
					edit_control.SetText("");
				SetFocus();
				break;

			case WM_COMMAND:
				switch (int id = LOWORD(wParam)) {
					case ID_EDIT:
						switch (HIWORD(wParam)) {
							case EN_KILLFOCUS:
								if (edit_control.GetText().len()) {
									if (static_cast<X*>(this)->UpdateColumn(edit_index, edit_col, str<256>(edit_control.GetText())))
										static_cast<X*>(this)->WriteColumn(edit_index, edit_col);
								}
								edit_control.Destroy();
								break;
						}
						break;
				}
				break;

			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case NM_CLICK: {
						NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
						static_cast<X*>(this)->LeftClick(nmlv->hdr.hwndFrom, nmlv->iItem, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
						return 0;
					}
					case NM_RCLICK: {
						NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
						static_cast<X*>(this)->RightClick(nmlv->hdr.hwndFrom, nmlv->iItem, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
						return 0;
					}
					case LVN_GETDISPINFOW: {
						auto	&item	= ((NMLVDISPINFOW*)nmh)->item;
						if (item.mask & LVIF_TEXT) {
							string_builder	b;
							static_cast<X*>(this)->GetDispInfo(b, item.iItem, item.iSubItem);
							string_copy(string_buffer16(item.pszText, item.cchTextMax), b.term());
							//string_buffer16(item.pszText, item.cchTextMax) = b.term();
						}
						return 1;
					}
					case LVN_GETDISPINFOA: {
						auto	&item = ((NMLVDISPINFO*)nmh)->item;
						if (item.mask & LVIF_TEXT)
							static_cast<X*>(this)->GetDispInfo(lvalue(fixed_accum(item.pszText, item.cchTextMax)), item.iItem, item.iSubItem);
						return 1;
					}
					case NM_CUSTOMDRAW:
						if (int r = static_cast<X*>(this)->CustomDraw((NMLVCUSTOMDRAW*)lParam))
							return r;
						break;
				}
				break;
			}
			case WM_NCDESTROY:
				release();
			case WM_DESTROY:
				return 0;
		}
		return Super(message, wParam, lParam);
	}

	void	EditColumn(int i, int col) {
		win::EditLabel(*this, edit_control, i, col, ID_EDIT);
		edit_index	= i;
		edit_col	= col;
	}
//	void	EditName(int i) {
//		EditColumn(i, 0);
//	}
	void	RightClick(Control from, int row, int col, const Point &pt, uint32 flags)	{}
	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags)	{}
	void	GetDispInfo(string_accum &sa, int row, int col)		{}// sa << "unimplemented"; }
	int		CustomDraw(NMLVCUSTOMDRAW *cd)						{ return 0; }

	template<typename...P> EditableListView(P&&... p) : B(forward<P>(p)...) {}
};

//-----------------------------------------------------------------------------
//	Timer
//-----------------------------------------------------------------------------

class Timer {
	HANDLE	h;
public:
	enum FLAGS {
		DEFAULT					= WT_EXECUTEDEFAULT,
		UI_THREAD				= WT_EXECUTEINUITHREAD,
		WAIT_THREAD				= WT_EXECUTEINWAITTHREAD,
		ONLY_ONCE				= WT_EXECUTEONLYONCE,
		TIMER_THREAD			= WT_EXECUTEINTIMERTHREAD,
		LONG_FUNCTION			= WT_EXECUTELONGFUNCTION,
		PERSISTENT_IO_THREAD	= WT_EXECUTEINPERSISTENTIOTHREAD,
		PERSISTENT_THREAD		= WT_EXECUTEINPERSISTENTTHREAD,
		TRANSFER_IMPERSONATION	= WT_TRANSFER_IMPERSONATION,
	};
	Timer() : h(0)	{}
	~Timer()		{ DeleteTimerQueueTimer(0, h, INVALID_HANDLE_VALUE); }
	bool	Start(WAITORTIMERCALLBACK proc, float t, FLAGS flags = DEFAULT) {
		Stop();
		return CreateTimerQueueTimer(&h, 0, proc, this, 1000 * t, 1000 * t, flags);
	}
	bool	Next(WAITORTIMERCALLBACK proc, float t, FLAGS flags = DEFAULT) {
		Stop();
		return CreateTimerQueueTimer(&h, 0, proc, this, 1000 * t, 0, flags);
	}
	void	Stop() {
		if (HANDLE h0 = exchange(h, nullptr))
			DeleteTimerQueueTimer(0, h0, INVALID_HANDLE_VALUE);
	}
	bool	IsRunning() {
		return h != 0;
	}
};

class TimerV : public Timer, public virtfunc<void(TimerV*)> {
	static void CALLBACK proc(void *param, BOOLEAN unused) {
		(*(TimerV*)param)((TimerV*)param);
	}
public:
	using Timer = TimerV;
	void	Start(float t, FLAGS flags = DEFAULT)	{ win::Timer::Start(proc, t, flags); }
	void	Next(float t, FLAGS flags = DEFAULT)	{ win::Timer::Next(proc, t, flags); }
	template<typename T> TimerV(T *t)	: virtfunc<void(TimerV*)>(t) {}
};

template<typename T> class TimerT : public Timer {
	static void CALLBACK proc(void *param, BOOLEAN unused) {
		(*static_cast<T*>((Timer*)param))((Timer*)param);
	}
public:
	using Timer = TimerT;
	void	Start(float t, FLAGS flags = DEFAULT)	{ win::Timer::Start(proc, t, flags); }
	void	Next(float t, FLAGS flags = DEFAULT)	{ win::Timer::Next(proc, t, flags); }
};

template<typename T> class WindowTimer : public TimerT<WindowTimer<T>> {
protected:
	enum { WM_ISO_TIMER	= WM_USER+0 };
public:
	void	operator()(Timer*) { static_cast<T*>(this)->PostMessage(WM_ISO_TIMER); }
};

//-----------------------------------------------------------------------------
//	TextWindow
//-----------------------------------------------------------------------------

struct TextFinder : FINDREPLACEA {
	fixed_string<256>	ss;
	static TextFinder	*CheckMessage(MSG_ID msg, WPARAM wParam, LPARAM lParam);
	TextFinder(HWND hWnd);
	~TextFinder() { SetModelessDialog(0); }
};

bool FindNextText(EditControl c, const char *search, int flags);

//-----------------------------------------------------------------------------
//	Progress
//-----------------------------------------------------------------------------

typedef callback<void(uint32)>	progress;

struct ProgressTaskBar : ProgressBarControl {
	com_ptr<ITaskbarList3>	taskbar_list;
	Control					top;

	ProgressTaskBar() {}
	ProgressTaskBar(ProgressTaskBar &&b) : ProgressBarControl(move(b)), taskbar_list(move(b.taskbar_list)), top(b.top) { b.hWnd = 0; }
	ProgressTaskBar(const WindowPos &wpos, uint32 total) { Create(wpos, total); }
	~ProgressTaskBar() {
		if (taskbar_list && top)
			taskbar_list->SetProgressState(top, TBPF_NOPROGRESS);
		if (hWnd)
			PostMessage(WM_CLOSE);
	}

	void Create(const WindowPos &wpos, uint32 total) {
		ProgressBarControl::Create(wpos, 0, VISIBLE | OVERLAPPED | CAPTION | ((total == 0) * MARQUEE));
		if (total)
			SetRange(0, 100);
		else
			SetMarquee(true);

		top = wpos.parent;
		while (Control parent = top.Parent() ? top.Parent() : top.Owner())
			top = parent;
	}

	void	SetPos(uint32 pos) {
		ProgressBarControl::SetPos(pos);
		if (taskbar_list && top) {
			uint32	total = GetRange().y;
			taskbar_list->SetProgressState(top, pos == 0 ? TBPF_NOPROGRESS : total == 0 ? TBPF_INDETERMINATE : TBPF_NORMAL);
			if (pos && total)
				taskbar_list->SetProgressValue(top, pos, total);
		}
	}
};

struct Progress : ProgressTaskBar {
	const char	*caption	= nullptr;
	uint64		total;
	uint64		prev		= 0;

	Progress() {}
	Progress(const WindowPos &wpos, const char *caption, uint64 total) : ProgressTaskBar(wpos, total ? 100 : 0), caption(caption), total(total), prev(0) {}

	void Create(const WindowPos& wpos, const char* _caption, uint64 _total) {
		ProgressTaskBar::Create(wpos, _total ? 100 : 0);
		Reset(_caption, _total);
	}
	void Reset(const char *_caption, uint64 _total) {
		caption = _caption;
		prev	= 0;
		if (total	= _total) {
			SetMarquee(false);
			style = style - MARQUEE;
			SetRange(0, 100);
		} else {
			SetMarquee(true);
			style = style | MARQUEE;
		}
	}
	bool Changes(uint64 pos) {
		if (prev * 100 / total == pos * 100 / total)
			return false;
		prev = pos;
		return true;
	}
	void	Set(uint64 pos) {
		int		percent = int(pos * 100 / total);
		SetPos(percent);
		SetText(format_string("%i%% %s", percent, caption));
		Update();
	}
	void	operator()(uint64 pos) {
		if (Changes(pos))
			Set(pos);
	}

};

//-----------------------------------------------------------------------------
//	HeaderControl
//-----------------------------------------------------------------------------

void		SetColumnWidths(HeaderControl c);

//-----------------------------------------------------------------------------
//	Tree Helpers
//-----------------------------------------------------------------------------

void		HighLightTree(TreeControl tree, HTREEITEM prev, uint32 val);
void		HighLightTree(TreeControl tree, uint32 addr, uint32 val);
HTREEITEM	FindOffset(TreeControl tree, HTREEITEM h, uint32 offset, bool stop_equal = true);
dynamic_array<arbitrary> GetParamHierarchy(TreeControl tree, HTREEITEM hItem);

//-----------------------------------------------------------------------------
//	Drag Helpers
//-----------------------------------------------------------------------------

class DropSource : public com_list<IDataObject, IDropSource> {
	HGLOBAL		data;
	CLIPFORMAT	format;
public:
//IDataObject
	STDMETHODIMP	GetData(FORMATETC *fmt, STGMEDIUM *med) {
		if (fmt->cfFormat == format && fmt->tymed) {
			med->tymed			= TYMED_HGLOBAL;
			med->pUnkForRelease	= 0;
			med->hGlobal		= data;
			return S_OK;
		}
		return DV_E_FORMATETC;
	}
	STDMETHODIMP	QueryGetData(FORMATETC *fmt)										{ return fmt->cfFormat == format && fmt->tymed ? S_OK :  DV_E_FORMATETC; }
	STDMETHODIMP	GetDataHere(FORMATETC *fmt, STGMEDIUM *med)							{ return DATA_E_FORMATETC;}
	STDMETHODIMP	GetCanonicalFormatEtc(FORMATETC *in, FORMATETC *out)				{ out->ptd = NULL; return E_NOTIMPL; }
	STDMETHODIMP	SetData(FORMATETC *fmt, STGMEDIUM *med, BOOL release)				{ return E_NOTIMPL; }
	STDMETHODIMP	EnumFormatEtc(DWORD dir, IEnumFORMATETC **enum_fmt) {
		if (dir == DATADIR_GET) {
			FORMATETC	formatetc = {format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
			return SHCreateStdEnumFmtEtc(1, &formatetc, enum_fmt);
		}
		return E_NOTIMPL;
	}

	STDMETHODIMP	DAdvise(FORMATETC *fmt, DWORD adv, IAdviseSink *sink,  DWORD *conn)	{ return OLE_E_ADVISENOTSUPPORTED; }
	STDMETHODIMP	DUnadvise(DWORD conn)												{ return OLE_E_ADVISENOTSUPPORTED; }
	STDMETHODIMP	EnumDAdvise(IEnumSTATDATA **enum_advise)							{ return OLE_E_ADVISENOTSUPPORTED; }

//IDropSource
	STDMETHODIMP	QueryContinueDrag(BOOL escape, DWORD buttons)						{ return escape ? DRAGDROP_S_CANCEL : buttons & (MK_LBUTTON | MK_RBUTTON) ? S_OK : DRAGDROP_S_DROP; }
	STDMETHODIMP	GiveFeedback(DWORD effect)											{ return DRAGDROP_S_USEDEFAULTCURSORS; }
	DropSource(HGLOBAL data, CLIPFORMAT format) : data(data), format(format) {}
};


struct DropTargetHelpers {
	static DWORD	get_effect(DWORD buttons, DWORD effect) {
		if ((effect & DROPEFFECT_COPY) && ((buttons & MK_SHIFT) || !(effect & DROPEFFECT_MOVE)))
			return DROPEFFECT_COPY;
		return DROPEFFECT_MOVE;
	}

	static CLIPFORMAT	single_format(IDataObject* pDataObj);
	static HGLOBAL		has_format(IDataObject* pDataObj, CLIPFORMAT format);
	static int			has_filename(IDataObject* pDataObj, filename &fn);
	static dynamic_array<filename>	has_filenames(IDataObject* pDataObj);
};

template<typename X> struct DropTarget : com<IDropTarget>, DropTargetHelpers {
	virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD buttons, POINTL pt, DWORD* effect) {
		*effect = get_effect(buttons, *effect);
		static_cast<X*>(this)->DragEnter(pt);
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD buttons, POINTL pt, DWORD* effect) {
		*effect = get_effect(buttons, *effect);
		static_cast<X*>(this)->DragOver(pt);
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE DragLeave() {
		static_cast<X*>(this)->DragExit();
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD buttons, POINTL pt, DWORD* effect) {
		*effect = get_effect(buttons, *effect);
		ImageList::DragEnd();
		return static_cast<X*>(this)->Drop(pt, *effect, pDataObj) ? S_OK : S_FALSE;
	}
//	DropTarget() { RegisterDragDrop(*static_cast<X*>(this), this); }	//hWnd not set yet
};

ImageList	BeginDrag(Control parent, ImageList drag, const Point &pt);
ImageList	BeginDrag(Control parent, ListViewControl lv, const Point &pt);
ImageList	BeginDrag(Control parent, TreeControl tc, HTREEITEM h, const Point &pt);
ImageList	BeginDrag(Control parent, TreeControl tc, const Point &pt);
void		StopDrag(Control c, ImageList drag);

} } // namespace iso::win

#endif // CONTROL_HELPERS_H
