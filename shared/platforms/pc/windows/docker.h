#ifndef DOCKER_H
#define DOCKER_H

#include "window.h"
#include "dwm.h"
//#include "desktopcomposition.h"
#include "base\array.h"

namespace iso { namespace win {

enum DockEdge {
	DOCK_LEFT,
	DOCK_RIGHT,
	DOCK_TOP,
	DOCK_BOTTOM,

	DOCK_TAB,
	DOCK_PUSH,
	DOCK_TABID,	// replace tab with same ID as control

	DOCK_INDEX			= 8,
	DOCK_MASK			= DOCK_INDEX - 1,
	DOCK_EXTEND			= DOCK_INDEX << 0,
	DOCK_RELATIVE		= DOCK_INDEX << 1,
	DOCK_DOMINANT		= DOCK_INDEX << 2,	// split from this edge
	DOCK_FIXED			= DOCK_INDEX << 3,	// don't convert child to tab
	DOCK_FIXED_SIB		= DOCK_INDEX << 4,	// don't convert sibling to tab
	DOCK_OR_TAB			= DOCK_INDEX << 5,	// use tab if available

	DOCK_LEFT_EDGE		= DOCK_LEFT | DOCK_EXTEND,
	DOCK_RIGHT_EDGE		= DOCK_RIGHT | DOCK_EXTEND,
	DOCK_TOP_EDGE		= DOCK_TOP | DOCK_EXTEND,
	DOCK_BOTTOM_EDGE	= DOCK_BOTTOM | DOCK_EXTEND,

	DOCK_ADDTAB	= DOCK_TAB - DOCK_INDEX,
};

constexpr DockEdge operator|(DockEdge a, DockEdge b) { return DockEdge(int(a) | int(b)); }

//-----------------------------------------------------------------------------
//	TitledWindow
//-----------------------------------------------------------------------------

struct TitledWindow : Window<TitledWindow> {
	win::Font	font;

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	void Create(const WindowPos &wpos, const char *title) {
		font = win::Font::Caption();
		Window<TitledWindow>::Create(wpos, title, CHILD | CLIPSIBLINGS | VISIBLE);
	}
	void Create(const char *title, Control child) {
		Create(child.GetRelativeWindowPos(), title);
		child.SetParent(*this);
		child.Move(GetChildRect());
		child.Show();
	}
	TitledWindow(const WindowPos &wpos, const char *title)	{ Create(wpos, title); }
	TitledWindow(const char *title, Control child)			{ Create(title, child); }
	TitledWindow(Control child)								{ Create(string(child.GetText()), child); }
	Rect		GetChildRect()			const	{ return GetClientRect().Grow(0, Parent() ? -16 : 0, 0, 0); }
	WindowPos	GetChildWindowPos()		const	{ return WindowPos(*this, GetChildRect()); }
};


//-----------------------------------------------------------------------------
//	TabWindow
//-----------------------------------------------------------------------------

enum : uint32 {
	TCN_DRAG	= NM_LAST + 1,
	TCN_MBUTTON,
	TCN_CLOSE,
};

// TabControl2
// assumes items are controls

class TabControl2 : public TabControl {
public:
	using TabControl::TabControl;

	Control		GetItemControl(int i)			const { return TabControl::Item(TCIF_PARAM).Get(*this, i).Param(); }
	Control		GetSelectedControl()			const { return GetItemControl(GetSelectedIndex()); }
	void		ShowSelectedControl()			const;
	void		ResizeItems()					const;
	int			FindControl(Control c)			const;
	int			FindControlByID(ID id)			const;
	bool		SetSelectedIndex(int i)			const;
	bool		SetSelectedControl(Control c)	const;
	void		AddItemControl(Control c, text title, int i = 0x7fffffff)	const;
	void		AddItemControl(Control c, int i = 0x7fffffff)				const;
	void		SetItemControl(Control c, text title, int i)				const;
	void		SetItemControl(Control c, int i)							const;
};

// TabControl3
// passes up WM_NOTIFY & WM_COMMAND

class TabControl3 : public Subclass<TabControl3, TabControl2> {
public:
	using Subclass<TabControl3, TabControl2>::Subclass;
	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
};

class TabWindow : public Subclass<TabWindow, TabControl2> {
	enum { REPLACE_SAMEID = 1, REPLACE_SAMEID_TOGGLE = 2 };
	static ImageList	images;
	int16			drag, hot;
	uint32			flags;
public:
	TabWindow();
	TabWindow(const WindowPos &wpos, const char *caption = "tabs", Style style = CHILD | CLIPCHILDREN | VISIBLE, StyleEx stylex = NOEX) : TabWindow() {
		Create(wpos, caption, style, stylex);
		SetFont(Font::DefaultGui());
	}
	TabWindow(const WindowPos &wpos, Control c) : TabWindow(wpos, "tabs", CHILD | CLIPCHILDREN | VISIBLE) {
		if (c) {
			AddItemControl(c);
			SetSelectedControl(c);
		}
	}

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void		AddTab(Control c, int i, bool new_tab);
	void		SetReplaceID(bool set)	{ flags = (flags & ~REPLACE_SAMEID_TOGGLE) | ((flags & REPLACE_SAMEID) == set ? 0 : REPLACE_SAMEID_TOGGLE); }
};

//-----------------------------------------------------------------------------
//	DockableWindow
//-----------------------------------------------------------------------------

class DockableWindow : public Window<DockableWindow> {
protected:
public:
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	DockableWindow(const Rect &r, const char *caption);
	DockableWindow(Control c);
};

//-----------------------------------------------------------------------------
//	StackWindow
//-----------------------------------------------------------------------------

class StackWindow : public Window<StackWindow> {
	static StackWindow*	last;
public:
	StackWindow() {}
	StackWindow(const WindowPos &wpos, text title, ID id = ID()) {
		Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, NOEX, id);
	}

	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	uint32			NumViews()			const	{ return Children().size32(); }
	uint32			Depth()				const;
	bool			SetSelectedControl(Control c);
	void			PushView(Control c);
	void			NextView(bool prev);
	Control			TopView()			const;
	Control			PopView();
	void			Dock(DockEdge edge, Control c);
	WindowPos		Dock(DockEdge edge, uint32 size = 0);
};


//-----------------------------------------------------------------------------
//	DockingWindow
//	should only hold SplitterWindow or TabWindow
//-----------------------------------------------------------------------------

class DockingWindow : public Window<DockingWindow> {
protected:
	Control		child;
	uint32		flags;
public:
	enum {
		NO_DOCK		= 1 << 8,
		FIXED_CHILD	= 1 << 9,
		RECURSING	= 1 << 31,
	};

	static HBRUSH		get_class_background()	{
		return _DWM::Composition() ? Brush::Black() : Brush::SysColor(COLOR_MENUBAR);
	}

	void			SetChild(Control c) {
		Control old = exchange(child, c);
		c.Move(GetChildWindowPos());
		old.Destroy();
	}
	void			SetChildImmediate(Control c) {
		child = c;
		c.Move(GetChildWindowPos(), SWP_NOREDRAW);
		c.Update();
	}
	Control			GetChild()			const	{ return child; }
	bool			CanDock()			const	{ return !(flags & NO_DOCK); }
	bool			FixedChild()		const	{ return !!(flags & FIXED_CHILD); }
	int				TopExtra()			const	{ return flags & 0xff; }
	Rect			GetFrameAdjust()	const;
	Rect			GetChildRect()		const	{ return TopExtra() ? GetClientRect() - GetFrameAdjust() : GetClientRect(); }
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void			Dock(DockEdge edge, Control c);
	WindowPos		Dock(DockEdge edge, uint32 size = 0);
	TabWindow*		FindTab(ID id, int &index);

	DockingWindow(uint32 flags = 0)	: flags(flags) {}
	DockingWindow(const WindowPos &wpos, const char *caption, uint32 flags = 0) : flags(flags) {
		Create(wpos, caption, CHILD | VISIBLE | CLIPCHILDREN | CLIPSIBLINGS, NOEX);
	}
};

//-----------------------------------------------------------------------------
//	SeparateWindow
//-----------------------------------------------------------------------------

class SeparateWindow : public DockingWindow
#ifdef DWM_H
	, public DWM<SeparateWindow>
#else
	, public CompositionWindow
#endif
{
protected:
	dynamic_array<ToolBarControl>	toolbars;
	Control			owner;

	SeparateWindow(uint32 flags) : DockingWindow(flags) {}

public:
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void			SetOwner(Control c)					{ owner = c; }

	iso_export void	AddToolbar(ToolBarControl tb);
	ToolBarControl	CreateToolbar(ID id, ID bmid);
	ToolBarControl	CreateToolbar(ID id)				{ return CreateToolbar(id, id); }
	void			AdjustToolbars();

	bool			MakeDropDown(win::ID id, Menu menu);
	Menu			GetDropDown(win::ID id);
	iso_export void	CheckButton(ID id, bool check);

	Rect			GetCaptionRect()						const;
	Rect			GetChildRect()							const	{ return GetClientRect() - GetFrameAdjust(); }
	WindowPos		GetChildWindowPos()						const	{ return WindowPos(*this, GetChildRect()); }
	Rect			AdjustRect(const Rect &rect)			const	{ return rect + GetFrameAdjust(); }
	Rect			AdjustRectFromChild(const Rect &rect)	const	{ return rect + (GetRect() - ToScreen(GetChildRect())); }
	TabWindow*		GetTabs() {
		if (auto *tabs = TabWindow::Cast(child))
			return tabs;
		auto *tabs = new TabWindow(GetChildWindowPos(), GetChild());
		SetChildImmediate(*tabs);
		return tabs;
	}
	void StartDrag() {
		ReleaseCapture();
		Show();
		SendMessage(WM_SYSCOMMAND, SC_MOVE | 2);
	//	Update();
	}

	SeparateWindow(const Rect &r, const char *caption, uint32 flags = 48) : SeparateWindow(flags) {
		Create(WindowPos(0, r + GetFrameAdjust()), caption, OVERLAPPED | SYSMENU | THICKFRAME | MINIMIZEBOX | MAXIMIZEBOX | CLIPCHILDREN | CLIPSIBLINGS, NOEX);
		Rebind(this);
	}
	SeparateWindow(Control c, uint32 flags = 48) : SeparateWindow(c.GetRect(), str<256>(c.GetText()), flags) {
		SetChildImmediate(c);
	}
};

//-----------------------------------------------------------------------------
//	Docker
//-----------------------------------------------------------------------------

struct Docker {
	Control			dock;// DockingWindow or SplitterWindow
	StackWindow		*stack;
	TabWindow		*tab;
	int8			pane;

	static TabWindow*	FindTab(Control c, ID id, int &index);
	static void			BringToFront(Control c);

	Docker(Control c);
	Docker(TabWindow *tab)		: stack(0), tab(tab) {}
	Docker(DockingWindow *dock);
	void		Dock(DockEdge edge, Control c);

	WindowPos	DockByID(ID id);
	WindowPos	Dock(DockEdge edge, uint32 size = 0);
	void		SetChild(Control c)				const;
	Control		GetSibling()					const;
	WindowPos	GetChildWindowPos()				const	{ return dock.GetChildWindowPos(); }
};

struct DockerDock : Docker {
	DockEdge	edge;
	uint32		size;
	operator WindowPos() { return Dock(edge, size); }
	DockerDock(Control c, DockEdge edge, uint32 size = 0) : Docker(c), edge(edge), size(size) {}
};


void		DragTab(Control owner, TabControl2 tab, ID id, bool top);
TabControl2	DestroyedTabs(class SplitterWindow *top, TabControl2 tab);

} } // namespace iso::win

#endif //DOCKER_H
