#include "docker.h"
#include "splitter.h"
#include "resource.h"
#include "common.rc.h"
#include "base\algorithm.h"

namespace iso { namespace win {

void FixTabPaint(TabControl tab, DeviceContext dc, const Rect &dirty) {
	Rect	client	= tab.GetClientRect();
	Rect	child	= tab.GetChildRect();

	if (tab.Count() == 0)
		dc.Fill(child, COLOR_WINDOW);

	if (dirty.left < child.left) {
		Rect	rect	= Rect(client.left, child.top - 3, child.left, client.bottom - child.top + 3);
		dc.Fill(rect, COLOR_WINDOW);
		dc.Fill(rect.Subbox(0, 0, 1, -1), COLOR_3DSHADOW);
	}

	if (dirty.right > child.right) {
		Rect	rect	= Rect(child.right, child.top - 3, client.right - child.right, client.bottom - child.top + 3);
		dc.Fill(rect.Subbox(0, 1, 1, -2), COLOR_WINDOW);
		dc.Fill(rect.Subbox(1, 0, 1, -1), COLOR_3DSHADOW);
		dc.Fill(rect.Subbox(2, 0, 0, 0), COLOR_3DFACE);
	}

	if (dirty.bottom > child.bottom && dirty.bottom > child.top) {
		Rect	rect	= Rect(client.left, child.bottom, client.Width(), client.bottom - child.bottom);
		dc.Fill(rect.Subbox(1, 0, -3, 2), COLOR_WINDOW);
		dc.Fill(rect.Subbox(0, 2, -3, 1), COLOR_3DSHADOW);
		dc.Fill(rect.Subbox(0, 3, 0, 0), COLOR_3DFACE);
	}

	if (dirty.top < child.top) {
		Rect	sel	= tab.GetItemRect(tab.GetSelectedIndex());
		Rect	rect	= Rect(client.left, child.top - 3, child.right, 3);
		dc.Fill(rect.Subbox(1, 0, -1, 0), COLOR_WINDOW);
		dc.Fill(rect.Subbox(0, 0, sel.left - 1, 1), COLOR_3DSHADOW);
		dc.Fill(rect.Subbox(sel.right + 1, 0, 0, 1), COLOR_3DSHADOW);
	}
}

void TabControl2::ShowSelectedControl() const {
	int	s = GetSelectedIndex();
	for (int i = 0, n = Count(); i < n; i++)
		GetItemControl(i).Show(i == s ? SW_SHOW : SW_HIDE);
}
void TabControl2::ResizeItems() const {
	Rect	rect = GetChildRect();
	for (int i = 0, n = Count(); i < n; i++)
		GetItemControl(i).Move(rect);
}
int TabControl2::FindControl(Control c) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (GetItemControl(i) == c)
			return i;
	}
	return -1;
}
int TabControl2::FindControlByID(ID id) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (GetItemControl(i).id == id)
			return i;
	}
	return -1;
}
bool TabControl2::SetSelectedIndex(int i) const {
	int old = TabControl::SetSelectedIndex(i);
	if (i != old) {
		GetItemControl(old).Show(SW_HIDE);
		GetItemControl(i).Show();
		return true;
	}
	return false;
}
bool TabControl2::SetSelectedControl(Control c) const {
	c.Show(SW_SHOW);
	return SetSelectedIndex(FindControl(c));
}
void TabControl2::AddItemControl(Control c, text title, int i) const {
	i = TabControl::Item().Text(title).Image(0).Param(c).Insert(*this, i);
	if (i != GetSelectedIndex())
		c.Hide();
	c.Move(GetChildWindowPos());
}
void TabControl2::AddItemControl(Control c, int i) const {
	AddItemControl(c, c.GetText(), i);
}
void TabControl2::SetItemControl(Control c, text title, int i) const {
	TabControl::Item	item	= GetItem(i);
	Control				old		= item.Param();
	item.Text(title).Param(c).Set(*this, i);
	c.Move(GetChildWindowPos());
	old.Destroy();
}
void TabControl2::SetItemControl(Control c, int i) const {
	SetItemControl(c, c.GetText(), i);
}


LRESULT TabControl3::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE:
			ResizeItems();
			break;

		case WM_PAINT: {
			Rect	rect;
			GetUpdateRect(*this, &rect, FALSE);
			Super(message, wParam, lParam);
			FixTabPaint(*this, DeviceContext(*this), rect);
			return 0;
		}

		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;

		case WM_NOTIFY:
		case WM_COMMAND:
			return Parent()(message, wParam, lParam);
	}
	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	TabWindow
//-----------------------------------------------------------------------------

ImageList	TabWindow::images;

TabWindow::TabWindow() : drag(-1), hot(-1), flags(REPLACE_SAMEID) {
	if (!images) {
		images	= ImageList(ID("IDB_IMAGELIST_TAB"), 12, LR_CREATEDIBSECTION);
		images = images.ScaledTo(DeviceContext::ScreenCaps().PixelScale(images.GetIconSize()));
	}
}

LRESULT TabWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			Class().style &= ~(CS_VREDRAW | CS_HREDRAW);
			SetImageList(images);
			break;

		case WM_SIZE:
			ResizeItems();
			return 0;

		case WM_ISO_CHILDRECT:
			*(Rect*)lParam = GetChildRect();
			return true;

		case WM_LBUTTONDOWN:
			if (hot >= 0)
				return Parent().Notify(*this, TCN_CLOSE, hot);
			drag	= HitTest(Point(lParam));
			if (drag >= 0) {
				SetFocus();
				SetCursor(Cursor::LoadSystem(IDC_HAND));
				SetCapture(*this);
			}
			break;

		case WM_LBUTTONUP:
			if (drag >= 0) {
				ReleaseCapture();
				drag	= -1;
			}
			return 0;

		case WM_MBUTTONDOWN: {
			Point	mouse(lParam);
			int	i	= HitTest(mouse);
			if (i >= 0)
				Parent().Notify(*this, TCN_MBUTTON, i);
			return 0;
		}

		case WM_MOUSEMOVE: {
			Point	mouse(lParam);
			int		x0	= DeviceContext::ScreenCaps().PixelScaleX(5);
			int		x1	= DeviceContext::ScreenCaps().PixelScaleX(15);

			if (drag >= 0) {
				if (!GetItemRect(drag).Contains(mouse)) {
					int	prev_drag = drag;
					drag = -1;
					if (Parent().Notify(*this, TCN_DRAG, prev_drag) == 0)
						drag = prev_drag;
				}
			} else if (hot >= 0) {
				Rect	r	= GetItemRect(hot);
				if (!r.Contains(mouse) || !between(mouse.x - r.left, x0, x1)) {
					GetItem(hot).Image(0).Set(*this, hot);
					hot = -1;
					ReleaseCapture();
				}
			} else {
				int		i	= HitTest(mouse);
				if (i >= 0 && between(mouse.x - GetItemRect(i).left, x0, x1)) {
					hot	= i;
					GetItem(i).Image(1).Set(*this, i);
					SetCapture(*this);
					SetCursor(Cursor::LoadSystem(IDC_HAND));
				}
			}
			return 0;
		}

		case WM_PAINT: {
			Rect	rect;
			GetUpdateRect(*this, &rect, FALSE);
			Super(message, wParam, lParam);
			FixTabPaint(*this, DeviceContext(*this), rect);
			return 0;
		}

		case TCM_DELETEITEM: {
			int	i = wParam;
			int	s = GetSelectedIndex();
			Super(TCM_DELETEITEM, i);
			if (Count() == 0) {
				Destroy();

			} else if (i == s) {
				SetSelectedIndex(i = max(i - 1, 0));
				if (Control c = GetItemControl(i)) {
					c.Show();
					c.Update();
				}
			}
			return TRUE;
		 }

		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_DESTROY: {
					Control	c	= lParam;
					int		i	= FindControl(c);
					if (i >= 0) {
						Parent()(message, wParam, lParam);
						RemoveItem(i);
						return 0;
					}
					break;
				}
				case WM_CREATE: {
					Control	c = lParam;
					if (c.Class().name() == "msctls_updown32")
						break;
					if (FindControl(lParam) < 0) {
						ID	id = c.id;
						int	i;
						if ((!!(flags & REPLACE_SAMEID) ^ !!(flags & REPLACE_SAMEID_TOGGLE)) && id && (i = FindControlByID(id)) >= 0) {
							TabControl::Item	item	= GetItem(i);
							Control				old		= item.Param();
							item.Text(c.GetText()).Param(c).Set(*this, i);
							old.Destroy();
						} else {
							i = TabControl::Item().Text(c.GetText()).Image(0).Param(c).Insert(*this);
						}
						flags &= ~REPLACE_SAMEID_TOGGLE;
						SetSelectedIndex(i);
						return 0;
					}
					break;
				}
			}
			break;

		default:
			if (message < WM_USER || message >= WM_USER + 0x100)
				break;
//			ISO_TRACEF("user ") << hex(message) << '\n';
		case WM_CHAR:
		case WM_NOTIFY:
		case WM_DROPFILES:
			return Parent()(message, wParam, lParam);
		case WM_COMMAND:
			if (HIWORD(wParam) != 0xffff)
				return Parent()(message, wParam, lParam);
			break;

		case WM_NCDESTROY:
			Super(message, wParam, lParam);
			delete this;
			return 0;
	}
	return Super(message, wParam, lParam);
}

void TabWindow::AddTab(Control c, int i, bool new_tab) {
	if (TabWindow *t = TabWindow::Cast(c)) {
		for (int j = 0, n = t->Count(); j < n; j++)
			AddItemControl(t->GetItemControl(j), i++);
		t->Destroy();
		return;
	}
	if (!new_tab && !!c.id) {
		int	i = FindControlByID(id);
		if (i >= 0) {
			SetItemControl(c, i);
			SetSelectedControl(c);
			return;
		}
	}
	AddItemControl(c);
	SetSelectedControl(c);
}

void DragTab(Control owner, TabControl2 tab, ID id, bool top) {
	Control			c		= tab.GetItemControl(id);

	if (StackWindow *bw = StackWindow::Cast(c)) {
		if (top && bw->Depth() == 0)
			top = false;

		if (top) {
			c		= bw->PopView();
			bw		= new StackWindow(tab.GetChildWindowPos(), c.GetText());
			bw->id	= c.id;
			c.SetParent(*bw);
			c		= *bw;
		}
		SeparateWindow	*sw = new SeparateWindow(c);
		sw->SetOwner(owner);
		if (!top)
			tab.RemoveItem(id);

		if (bw->NumViews() > 1)
			sw->CreateToolbar(IDR_TOOLBAR_NAV);
		sw->StartDrag();

	} else {
		SeparateWindow	*sw = new SeparateWindow(c);
		sw->SetOwner(owner);

		tab.RemoveItem(id);
		sw->StartDrag();

//		bw		= new StackWindow(tab.GetChildWindowPos(), str(c.GetText()));
//		bw->id	= c.id;
//		c.SetParent(*bw);
//		c		= *bw;
	}

}

TabControl2 DestroyedTabs(SplitterWindow *top, TabControl2 tab) {
	if (SplitterWindow *sw = SplitterWindow::Cast(tab.Parent())) {
		Control	sib = sw->GetPane(1 - sw->WhichPane(tab));
		while (sw = SplitterWindow::Cast(sib))
			sib = sw->GetPane(0);

		if (TabWindow::Cast(sib))
			return TabControl2(sib);
	}
	TabWindow	*t = new TabWindow;
	t->Create(top->GetPanePos(1), "tabs", Control::CHILD | Control::CLIPCHILDREN | Control::VISIBLE);
	t->SetFont(win::Font::DefaultGui());

	top->SetPane(1, *t);
	return *t;
}

//-----------------------------------------------------------------------------
//	TitledWindow
//-----------------------------------------------------------------------------

LRESULT TitledWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE: {
			Rect	rect = GetChildRect();
			for (auto i : Children())
				i.Move(rect);
			break;//return 0;
		}

		case WM_ISO_CHILDRECT:
			if (!Parent())
				return false;
			*(Rect*)lParam = GetClientRect().Grow(0, -16, 0, 0);
			return true;

		case WM_PAINT:
			if (Parent()) {
				DeviceContextPaint	dc(*this);
				SelectObject(dc, font);
				dc.Fill(GetClientRect().SetBottom(16), COLOR_ACTIVECAPTION);
				dc.SetOpaque(false);
				dc.TextOut(Point(4,1), GetText());
			}
			break;

		default:
			if (message < WM_USER)
				break;
		case WM_CHAR:
		case WM_NOTIFY:
		case WM_DROPFILES:
//		case WM_COMMAND:
			return Parent()(message, wParam, lParam);

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	StackWindow
//-----------------------------------------------------------------------------

StackWindow	*StackWindow::last;
/*
StackWindow *StackWindow::GetCurrent() {
	Control	c = GetFocus();
	if (TabWindow *t = TabWindow::Cast(c))
		c = t->GetSelectedControl();
	if (StackWindow *s = FindAncestor(c))
		last = s;
	return last;
}
*/

bool StackWindow::SetSelectedControl(Control c) {
	bool	found = false;
	for (auto i : Children()) {
		if (i == c)
			found = true;
		i.Show(i == c);
	}
	return found;
}


void StackWindow::PushView(Control c) {
	auto i = Children().begin();
	bool	single = i == c;

	while (i && !i->IsVisible())
		++i;

	Control	r = *i++;
	while (i) {
		if (i == c)
			++i;
		else
			i++->Destroy();
	}
	r.Hide();
	c.Show();
	if (!single) {
		for (Control p = Parent(); p; p = p.Parent()) {
			if (SeparateWindow *sw = (SeparateWindow*)SeparateWindow::Cast(p)) {
				sw->CreateToolbar(IDR_TOOLBAR_NAV);
				break;
			}
		}
	}
}

Control StackWindow::TopView() const {
	for (auto i : Children()) {
		if (i.IsVisible())
			return i;
	}
	return Control();
}
Control StackWindow::PopView() {
	auto i = Children().begin();
	while (i && !i->IsVisible())
		++i;

	if (i == Children().begin())
		return Control();

	Control	r = *i++;
	while (i)
		i++->Destroy();

	if (Control p = *--child_iterator(r))
		p.Show();

	return r;
}

uint32 StackWindow::Depth() const {
	uint32	n = 0;
	for (auto i : Children()) {
		if (!i.IsVisible())
			break;
		++n;
	}
	return n;
}

void StackWindow::NextView(bool prev) {
	auto i = Children().begin();
	while (i && !i->IsVisible())
		++i;

	Control	r = *i;
	if (Control	c = prev ? *--i : *++i) {
		r.Hide();
		c.Show();
		SetAccelerator(0, 0);
		c.SetFocus();
	}
}

void StackWindow::Dock(DockEdge edge, Control c) {
	if (c) {
		if (edge == DOCK_PUSH)
			PushView(c);
		else
			Docker(Parent()).Dock(edge, c);
	}
}

WindowPos StackWindow::Dock(DockEdge edge, uint32 size) {
	if (edge == DOCK_PUSH)
		return GetChildWindowPos();
	else
		return Docker(Parent()).Dock(edge, size);
}

LRESULT StackWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			last = this;
			break;

		case WM_SIZE: {
			Rect	rect = GetChildRect();
			for (auto i : Children())
				i.Move(rect);
			return 0;
		}

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_NEXT_PANE:
					if (IsVisible())
						NextView(false);
					return 1;

				case ID_PREV_PANE:
					if (IsVisible())
						NextView(true);
					return 1;

//				default:
//					return Parent()(message, wParam, lParam);
			}
			break;

		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_CREATE:
					PushView(lParam);
					break;
			}
			break;

		case WM_NCDESTROY:
			if (this == last)
				last = 0;
			delete this;
			return 0;

		case WM_NOTIFY:
			return Parent()(message, wParam, lParam);
	}
	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	Finder
//-----------------------------------------------------------------------------

class HighlightWindow : public Window<HighlightWindow> {
public:
	static HBRUSH get_class_background()	{ return GetSysColorBrush(COLOR_ACTIVECAPTION); }
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		if (message == WM_NCDESTROY) {
			hWnd = 0;
			return 0;
		}
		return Super(message, wParam, lParam);
	}
	bool	Move(const Rect &rect) {
		Point			size = rect.Size();
		BLENDFUNCTION	blend = {AC_SRC_OVER, 0, 128, 0};
		return !!UpdateLayeredWindow(*this,
			NULL,
			(POINT*)&rect.TopLeft(),
			(SIZE*)&size,
			NULL,
			NULL,
			0,
			&blend,
			ULW_ALPHA
		);
		//SetLayeredWindowAttributes(*this, RGB(0,0,0), 128, LWA_ALPHA);
	}
	bool	Create(Control parent, const Rect &rect) {
		Window<HighlightWindow>::Create(WindowPos(0, parent.ToScreen(rect)), 0, POPUP | DISABLED | VISIBLE, TOPMOST | LAYERED | NOACTIVATE);
/*		while (Control up = parent.Parent())
			parent = up;
		MoveAfter(parent);
*/		return !!SetLayeredWindowAttributes(*this, RGB(0,0,0), 128, LWA_ALPHA);
	}
};

class Finder {
	Control			control;
	DockEdge		edge;
	HighlightWindow	highlight;

	static int	split_size(int a, int b) {
		return a * b / (a + b);
	}

	static DockEdge CalcEdge(Control c, const Point &mouse, bool notab) {
//		Rect	r	= c.GetRect();
		Rect	r	= c.ToScreen(c.GetChildRect());

		int	left	= mouse.x - r.Left();
		int	right	= r.Right() - mouse.x;
		int	top		= mouse.y - r.Top();
		int	bottom	= r.Bottom() - mouse.y;
		int	xedge	= min(left, right), yedge = min(top, bottom);

		int	edge	= DOCK_LEFT;
		if (yedge < xedge) {
			left	= top;
			right	= bottom;
			edge	= DOCK_TOP;
		}
		edge		+= int(right < left);
		int	dist	= min(left, right);

		if (dist < 0) {
			if (auto dw = DockingWindow::Cast(c)) {
				if (dw->CanDock()) {
					if (!notab && edge == DOCK_TOP && dist > -8 && !TabWindow::Cast(dw->GetChild()))
						edge = DOCK_TAB;
					else
						edge += DOCK_EXTEND;
				}
			} else if (auto tabs = TabWindow::Cast(c)) {
				if (!notab && edge == DOCK_TOP)
					edge = DOCK_TAB | tabs->HitTest(tabs->ToClient(mouse)) * DOCK_INDEX;
			} else if (!notab && edge == DOCK_TOP) {
				edge = DOCK_TAB;
			}

		}

		return DockEdge(edge);
	}

	static Rect GetEdgeStrip(Control c, DockEdge edge, Point size) {
		Rect			r	= c.GetChildRect();

		switch (edge & (DOCK_MASK|DOCK_EXTEND)) {
			case DOCK_LEFT:		r = r.Subbox(0, 0, split_size(size.x, r.Width()), 0);	break;
			case DOCK_RIGHT:	r = r.Subbox(-split_size(size.x, r.Width()), 0, 0, 0);	break;
			case DOCK_TOP:		r = r.Subbox(0, 0, 0, split_size(size.y, r.Height()));	break;
			case DOCK_BOTTOM:	r = r.Subbox(0, -split_size(size.y, r.Height()), 0, 0);	break;

			case DOCK_LEFT_EDGE:	r = Rect(r.TopLeft() - Point(size.x, 0), r.BottomLeft());	break;
			case DOCK_RIGHT_EDGE:	r = Rect(r.TopRight(), r.BottomRight() + Point(size.x, 0));	break;
			case DOCK_TOP_EDGE:		r = Rect(r.TopLeft() - Point(0, size.y), r.TopRight());	break;
			case DOCK_BOTTOM_EDGE:	r = Rect(r.BottomLeft(), r.BottomRight() + Point(0, size.y));	break;

			default:
				if (TabWindow *tabs = TabWindow::Cast(c)) {
					int	i = uint32(edge) / DOCK_INDEX;
					int	n = tabs->Count();
					if (i < n) {
						Rect	dummy;
						tabs->GetItemRect(i).SplitX(0.5f, r, dummy);
					} else if (n > 0) {
						r = tabs->GetItemRect(n - 1);
						r += Point(r.Width(), 0);
					} else {
						r = tabs->GetTabStrip().Subbox(0, 0, 100, 0);
					}
				} else if (false) {// sw) {
					r = r.Subbox(0, 0, 0, 17);
				} else {
					//r = r.Subbox(0, 0, 0, TabControl(c).ClientDisplayRect().Top());
				}
				break;
		}
		return r;
	}

public:
	enum FLAGS {
		NO_ENTAB = 1,		// can't dock splitter into tabs
	};

	Control Find(const Point &mouse, Control exclude, Point size, uint32 flags) {
		Control	new_control;

		enum_thread_windows(GetCurrentThreadId(), [mouse, exclude, flags, &new_control](ChildEnumerator *e, Control c) {
			if (c != exclude) {
				Rect	r	= c.GetRect();
				Rect	cr	= c.ToScreen(c.GetChildRect());

				if (r.Contains(mouse)) {
					if (DockingWindow *dw = DockingWindow::Cast(c)) {
						if (!e->process(dw->GetChild()))
							return false;

						// check for extending window
						if (dw->CanDock() && !c.Parent() && !cr.Contains(mouse)) {
							new_control = c;
							return false;
						}

						if (!cr.Grow(-16,-16,-16,-16).Contains(mouse)) {
							new_control = c;
							return false;
						}
						return true;
					}

					if (TabWindow *tabs = TabWindow::Cast(c)) {
						if (!e->process(tabs->GetSelectedControl()))
							return false;

						//if ((!(flags & NO_ENTAB) || tabs->Count() > 0) && !r.Grow(-16, flags & NO_ENTAB ? -16 : -23,-16,-16).Contains(mouse)) {
						if (!cr.Grow(-16,-16,-16,-16).Contains(mouse)) {
							new_control = c;
							return false;
						}
						return true;
					}

					return e->enum_immediate_children(c);
				}
			}
			return true;
		});

		Control	prevc	= exchange(control, new_control);
		int		preve	= edge;

		if (control)
			edge = CalcEdge(control, mouse, !!(flags & NO_ENTAB));

		if (control != prevc || edge != preve) {
			ISO_TRACEF("Edge:") << edge << " HWND:0x" << hex(intptr_t((HWND)control)) << " (" << (DockingWindow::Cast(control) ? 'D' : TabWindow::Cast(control) ? 'T' : SplitterWindow::Cast(control) ? 'S' : '?') << ")\n";
			highlight.Destroy();
			if (control)
				highlight.Create(control, GetEdgeStrip(control, edge, size));
		}
		return control;
	}

	Control Finish(DockEdge &_edge) {
		highlight.Destroy();
		_edge = edge;
		return exchange(control, Control());
	}
};

Finder	finder;

//-----------------------------------------------------------------------------
//	DockingWindow
//-----------------------------------------------------------------------------

Rect DockingWindow::GetFrameAdjust() const {
	return _DWM::Composition()
		? RECT{0, -(TopExtra() + 3), 0, 0}
	: Rect(7,4 - TopExtra(), -14, TopExtra() - 7);
}

LRESULT DockingWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE:
			child.Move(GetChildRect());
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TCN_DRAG:
					DragTab(*this, nmh->hwndFrom, nmh->idFrom, !!(GetKeyState(VK_SHIFT) & 0x8000));
					return 1;

				//needed?
				case TCN_CLOSE:
					TabControl2(nmh->hwndFrom).GetItemControl(nmh->idFrom).Destroy();
					return 1;

				//needed?
				case TCN_SELCHANGE:
					TabControl2(nmh->hwndFrom).ShowSelectedControl();
					return 0;
			}
			return 0;
		}

		//needed?
		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_DESTROY: {
					if ((HWND)lParam == child) {
						if (SplitterWindow *s = SplitterWindow::Cast(child)) {
							SetChildImmediate(s->GetPane(!s->GetPane(0)));
						}
						return 1;
					}
				}
			}
			break;

//		case WM_COMMAND:
//			if (HIWORD(wParam) != 0xffff)
//				return Control(GetFocus())(message, wparam(wParam, 0xffff), lParam);
//			break;

		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;
	}
	return Super(message, wParam, lParam);
}

void DockingWindow::Dock(DockEdge edge, Control c) {
	Docker(this).Dock(edge, c);
}

WindowPos DockingWindow::Dock(DockEdge edge, uint32 size) {
	return Docker(this).Dock(edge, size);
}

TabWindow* DockingWindow::FindTab(ID id, int &index) {
	return Docker::FindTab(Child(), id, index);
}

//-----------------------------------------------------------------------------
//	SeparateWindow
//-----------------------------------------------------------------------------

void SeparateWindow::AdjustToolbars() {
	auto	frame	= GetFrameAdjust();
	int		x		= -frame.Left();
	for (auto *i = toolbars.begin(), *e = toolbars.end(); i != e; ++i) {
		auto	tb		= *i;
		Rect	last	= tb.GetItemRect(tb.Count() - 1);
		tb.Move(Rect(x, (-frame.Top() - last.Height()) / 2, last.Right(), last.Bottom()));
		x += last.Right();
	}
	Invalidate(Rect(x, 0, GetClientRect().right - x, TopExtra()));
}

void SeparateWindow::AddToolbar(ToolBarControl tb) {
	auto	frame	= GetFrameAdjust();
	int		xmin	= toolbars.empty() ? -frame.Left() : toolbars.back().GetRelativeRect().Right();
	Rect	last	= tb.GetItemRect(tb.Count() - 1);

	tb.Move(Rect(xmin, (-frame.Top() - last.Height()) / 2, last.Right(), last.Bottom()));
	tb.Show();

	toolbars.push_back(tb);

	last.left		= last.right;
	last.right		= GetClientRect().right;
	last.bottom		= -frame.Top();
	Invalidate(last);
}

ToolBarControl SeparateWindow::CreateToolbar(ID id, ID bmid) {
	for (auto &i : toolbars) {
		if (i.id == id)
			return i;
	}

	ToolBarControl	tb(*this, NULL, CHILD | CCS_NODIVIDER | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS, NOEX, id);
	tb.id = id;
	tb.Init(GetLocalInstance(), id, bmid);
	tb(TB_SETMAXTEXTROWS, 0);
	AddToolbar(tb);
	return tb;
}

void SeparateWindow::CheckButton(win::ID id, bool check) {
//	track_menu.CheckByID(id, check);
	for (auto &i : toolbars)
		i.CheckButton(id, check);
}

bool SeparateWindow::MakeDropDown(win::ID id, Menu menu) {
	ToolBarControl::Item	item(TBIF_STYLE | TBIF_SIZE);
	for (auto &i : toolbars) {
		if (item._Get(i, id) != -1) {
			int	ddwidth = GetSystemMetrics(SM_CXMENUCHECK);
			item.Style(BTNS_DROPDOWN).Width(i.ButtonSize().x + ddwidth).Param(menu).Set(i, id);
			i.SetExtendedStyle(ToolBarControl::DRAWDDARROWS);
			AdjustToolbars();
			return true;
		}
	}
	return false;
}

Menu SeparateWindow::GetDropDown(win::ID id) {
	ToolBarControl::Item	item(TBIF_STYLE | TBIF_LPARAM);
	for (auto &i : toolbars) {
		if (item._Get(i, id) != -1) {
			if (item.Style() & BTNS_DROPDOWN)
				return item.Param();
		}
	}
	return Menu();
}

Rect SeparateWindow::GetCaptionRect() const {
	auto	frame	= GetFrameAdjust();
	int		xmax	= toolbars.empty() ? -frame.Left() : toolbars.back().GetRelativeRect().Right();
	return GetClientRect().Grow(-xmax, 0, 0, 0).SetBottom(60);
}

/*
void CheckTab(SeparateWindow *sep, TabControl2 tab) {
	if (tab.Count() == 1 && sep->GetChild() == tab) {
		Control		c = tab.GetItemControl(0);
		sep->SetChild(c);
		sep->SetText(str(c.GetText()));
	}
}
*/
LRESULT SeparateWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	LRESULT	r;
	switch (message) {
	#ifndef DWM_H
		case WM_NCCREATE:
			EnableNonClientDpiScaling(hWnd);
			break;
	#endif
		case WM_CREATE: {
			child.SetParent(*this);
			owner = Parent();
		#ifdef DWM_H
			if (_DWM::Composition())
				ExtendIntoClient(GetFrameAdjust());
		#else
			CompositionWindow::OnCreate(*this);
		#endif
			SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_DRAWFRAME|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
			return 0;
		}
		case WM_DPICHANGED: {
			UINT uDpi = HIWORD(wParam);
			auto	rect	= (Rect*)lParam;
			SetWindowPos(hWnd, nullptr, rect->left, rect->top, rect->Width(), rect->Height(), SWP_NOZORDER | SWP_NOACTIVATE);
		#ifndef DWM_H
			OnDpiChanged(uDpi);
		#endif
			return 0;
		}
		case WM_SIZE:
		#ifndef DWM_H
			AddElement(100, 0,0, colour(1,0,0,1));
		#endif
			child.Move(GetChildRect());
			return 0;

		case WM_ISO_CHILDRECT:
			*(Rect*)lParam = GetChildRect();
			return true;

		case WM_MOVING:
			finder.Find(GetMousePos(), *this, GetClientRect().Size(), CastByProc<SplitterWindow>(child) ? Finder::NO_ENTAB : 0);
			SetForegroundWindow(*this);
			break;

		case WM_EXITSIZEMOVE: {
			DockEdge		edge;
			if (Control dest = finder.Finish(edge)) {
				Docker(dest).Dock(edge, child);
				Destroy();
			}
			break;
		}

		#ifdef DWM_H
		case WM_ACTIVATE:
			if (_DWM::Composition())
				ExtendIntoClient(GetFrameAdjust());
			break;

		case WM_NCCALCSIZE:
			if (_DWM::Composition()) {
				if (wParam == 0) {
					Rect	*r = (Rect*)lParam;
					//					*r	= r->Grow(-7,0,-7,-7);
					return 0;
				} else {
					NCCALCSIZE_PARAMS	*p = (NCCALCSIZE_PARAMS*)lParam;
					Rect	*r = (Rect*)p->rgrc;
					*r	= r->Grow(-7,0,-7,-7);
					//					*r	= r->Grow(-1,0,-1,-1);
					return 0;
				}
			}
			break;
		case WM_NCHITTEST:
			if (_DWM::Composition()) {
				if (DwmDefWindowProc(hWnd, message, wParam, lParam, &r))
					return r;
				Rect	client	= ToScreen(GetChildRect());
				Point	pt		= Point(lParam);
				int		edge	= pt.x < client.Left() ? 1 : pt.x >= client.Right() ? 2 : 0;
				if (pt.y < client.Top()) {
					if (client.Top() - pt.y < TopExtra()) {
						if (edge == 0)
							return HTCAPTION;
					} else {
						edge += 3;
					}
				} else if (pt.y >= client.Bottom()) {
					edge += 6;
				}

				static LRESULT hits[] = {HTCLIENT, HTLEFT, HTRIGHT, HTTOP, HTTOPLEFT, HTTOPRIGHT, HTBOTTOM, HTBOTTOMLEFT, HTBOTTOMRIGHT};
				return hits[edge];
			}
			break;

		case WM_PAINT:
			if (_DWM::Composition()) {
				DeviceContextPaint	dc(*this);
				PaintCustomCaption(dc, str16<256>(GetText()), GetCaptionRect(),
					win::Font("Segoe UI", 24, Font::ITALIC),
					win::Colour(0,0,128),//RGB(246,133,12),
					15,
					DT_LEFT | DT_SINGLELINE | DT_BOTTOM
				);
			}
			break;
		#endif

		case WM_SETTEXT:
			Invalidate(GetCaptionRect());
			break;

		case WM_CTLCOLORSTATIC:
			break;
//			InvalidateRect(HWND(lParam), NULL, FALSE);
			return (LRESULT)GetStockObject(WHITE_BRUSH);

		case WM_COMMAND:
			if (owner)
				return owner(message, wParam, lParam);
			if (HIWORD(wParam) != 0xffff && (lParam == 0 || find(toolbars, HWND(lParam)) != toolbars.end())) {
				//return Control(GetFocus())(message, wparam(wParam, 0xffff), lParam);
				for (Control c = GetFocus(); c; c = c.Parent()) {
					if (c(message, wparam(wParam, 0xffff), lParam))
						break;
				}
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TBN_DROPDOWN: {
					NMTOOLBAR		*nmtb	= (NMTOOLBAR*)nmh;
					ToolBarControl	tb(nmh->hwndFrom);
					Menu			m		= tb.GetItem(nmtb->iItem).Param();
					m.Track(*this, tb.ToScreen(Rect(nmtb->rcButton).BottomLeft()));
					break;
				}
				case TBN_HOTITEMCHANGE: {
					NMTBHOTITEM	*nmhi = (NMTBHOTITEM*)nmh;
					if (!(nmhi->dwFlags & HICF_LEAVING))
						EndMenu();
					break;
				}
				case TTN_GETDISPINFO: {
					TOOLTIPTEXT	*ttt	= (TOOLTIPTEXT*)nmh;
					ttt->hinst			= GetDefaultInstance();
					ttt->lpszText		= MAKEINTRESOURCE(ttt->hdr.idFrom);
					break;
				}

				case TCN_DRAG:
					DragTab(*this, nmh->hwndFrom, nmh->idFrom, false);
					return 1;

				case TCN_CLOSE:
					TabControl2(nmh->hwndFrom).GetItemControl(nmh->idFrom).Destroy();
					return 1;

				case TCN_SELCHANGE:
					TabControl2(nmh->hwndFrom).ShowSelectedControl();
					break;

				default:
					return owner(message, wParam, lParam);
			}
			return 0;
		}

		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_DESTROY: {
					Control	destroyed = (HWND)lParam;
					if (destroyed == child) {
						if (SplitterWindow *s = SplitterWindow::Cast(child)) {
							TabControl2	tabs = s->GetPane(!s->GetPane(0));
							if (tabs.Count() == 1) {
								Control	c = tabs.GetItemControl(0);
								Move(AdjustRect(tabs.GetRect()));
								SetChildImmediate(c);
								tabs.RemoveItem(0);
								SetText(c.GetText());
							} else {
								Move(AdjustRect(tabs.GetRect()));
								SetChildImmediate(tabs);
							}
						}
					} else {
						for (auto *i = toolbars.begin(), *e = toolbars.end(); i != e; ++i) {
							if (*i == destroyed) {
								Point	p	= i->GetRelativeRect().TopLeft();
								for (i = toolbars.erase(i), e = toolbars.end(); i != e; ++i) {
									i->Move(p);
									p.x += i->GetRect().Width();
								}
								break;
							}
						}
					}
					return 1;
				}
			}
			break;

		case WM_NCDESTROY:
			delete this;
			return 0;

	}
	if (DwmDefWindowProc(hWnd, message, wParam, lParam, &r))
		return r;

	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	DockableWindow
//-----------------------------------------------------------------------------

DockableWindow::DockableWindow(const Rect &r, const char *caption) {
	Create(WindowPos(0, r), caption, OVERLAPPEDWINDOW | CLIPCHILDREN | CLIPSIBLINGS, NOEX);
}

DockableWindow::DockableWindow(Control c) {
	Create(WindowPos(0, c.GetRect()), c.GetText(), OVERLAPPEDWINDOW | CLIPCHILDREN | CLIPSIBLINGS, NOEX);
	c.SetParent(*this);
}

LRESULT DockableWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE:
			Child().Move(GetChildRect());
			return 0;

		case WM_MOVING:
			finder.Find(GetMousePos(), *this, GetClientRect().Size(), 0);
			SetForegroundWindow(*this);
			break;

		case WM_EXITSIZEMOVE: {
			DockEdge		edge;
			if (Control dest = finder.Finish(edge)) {
				if (TabWindow *tabs = TabWindow::Cast(dest)) {
					tabs->AddTab(Child(), edge / DOCK_INDEX, edge == DOCK_ADDTAB);
					Destroy();
				} else if (DockingWindow *sep = DockingWindow::Cast(dest)) {
					new TabWindow(sep->GetChildWindowPos(), sep->GetChild());
					Destroy();
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			return 0;

	}
	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	Docker
//-----------------------------------------------------------------------------

void Docker::BringToFront(Control c) {
	Control	c0;

	while (c) {
		if (auto *s = StackWindow::Cast(c)) {
			s->SetSelectedControl(c0);

		} else if (auto *s = TabWindow::Cast(c)) {
			s->SetSelectedControl(c0);
		}

		c0	= c;
		c	= c.Parent();
	}
}

template<typename T> T *FindChild(Control c) {
	if (auto *sw = SplitterWindow::Cast(c)) {
		if (T *t = FindChild<T>(sw->GetPane(0)))
			return t;
		return FindChild<T>(sw->GetPane(1));
	}
	return T::Cast(c);
}

TabWindow *Docker::FindTab(Control c, ID id, int &index) {
	if (SplitterWindow *sw = SplitterWindow::Cast(c)) {
		TabWindow *tab;
		if ((tab = FindTab(sw->GetPane(0), id, index)) || (tab = FindTab(sw->GetPane(1), id, index)))
			return tab;

	} else if (TabWindow *tab = TabWindow::Cast(c)) {
		int i = tab->FindControlByID(id);
		if (i >= 0) {
			index = i;
			return tab;
		}
		for (int i = 0, n = tab->Count(); i < n; i++) {
			if (TabWindow *tab2 = FindTab(tab->GetItemControl(i), id, index))
				return tab2;
		}
	}
	return 0;
}

Docker::Docker(Control c) : stack(0), tab(0), pane(-1) {
	Control	c0;

	while (c) {
		if (DockingWindow::Cast(c)) {
			dock = c;
			break;
		}
		if (auto *s = SplitterWindow::Cast(c)) {
			if (s->flags & SplitterWindow::SWF_DOCK) {
				dock	= c;
				pane	= c0 ? s->WhichPane(c0) : 1;
				break;
			}
		}

		if (auto *s = StackWindow::Cast(c))
			stack = s;

		if (auto *s = TabWindow::Cast(c))
			tab = s;

		c0	= c;
		c	= c.Parent();
	}
}

Docker::Docker(DockingWindow *dock) : dock(*dock), stack(0), pane(-1), tab(0) {
	tab = FindChild<TabWindow>(dock->GetChild());
}

void Docker::Dock(DockEdge edge, Control c) {
	if (c) {
		if (CastByProc<SplitterWindow>(c) || TabWindow::Cast(c))
			edge = edge | DOCK_FIXED;

		auto	size	= c.GetClientRect().Size()[edge & 2 ? 1 : 0];
		auto	pos		= Dock(edge, size);
		c.Move(pos);
		pos.Parent()(WM_PARENTNOTIFY, WM_CREATE, (void*)c);
	}
}

WindowPos Docker::DockByID(ID id) {
	int		i;
	if (auto t = FindTab(dock.Child(), id, i))
		tab = t;
	if (!tab) {
		tab = new TabWindow(dock.GetChildWindowPos(), GetSibling());
		SetChild(*tab);
	}
	return tab->GetChildWindowPos();
}


WindowPos Docker::Dock(DockEdge edge, uint32 size) {
	if (edge == DOCK_PUSH && stack)
		return stack->GetChildWindowPos();

	DockingWindow	*dw = DockingWindow::Cast(dock);
	Control			sib	= dw ? dw->GetChild() : SplitterWindow::Cast(dock)->GetPane(pane);

	if ((edge & DOCK_MASK) < DOCK_TAB && (edge & DOCK_OR_TAB)) {
		if (tab) {
			edge = DOCK_TAB;
		} else if (edge & DOCK_FIXED) {
			if (auto *sw = SplitterWindow::Cast(sib)) {
				Control	old = sw->GetPane(edge & 1);
				sw->SetPane(edge & 1, Control());
				old.Destroy();
				return sw->_GetPanePos(edge & 1);
			}
		}
	}

	if ((edge & DOCK_MASK) < DOCK_TAB) {
		WindowPos	spos = pane < 0
			? dock.GetChildWindowPos()
			: SplitterWindow::Cast(dock)->GetPanePos(pane);

		uint32	size0	= spos.Rect().Size()[(edge & 2) >> 1];

		if (edge & DOCK_RELATIVE)
			size = size0 * size / MAXWORD;
		else if (size == 0)
			size = size0;

		if (!dock.Parent() && (edge & DOCK_EXTEND)) {
			Rect	rect	= dock.GetRect();

			switch (edge & DOCK_MASK) {
				case DOCK_LEFT:		rect.left	-= size; break;
				case DOCK_RIGHT:	rect.right	+= size; break;
				case DOCK_TOP:		rect.top	-= size; break;
				case DOCK_BOTTOM:	rect.bottom	+= size; break;
			}

			dock.Move(rect);
		}

		uint32			prop	= size * MAXWORD / (size0 + size);
		bool			flip	= !!(edge & DOCK_DOMINANT) ^ !!(edge & 1);
		if (flip)
			prop = MAXWORD - prop;

		SplitterWindow	*s		= new SplitterWindow(spos, 0,
			SplitterWindow::SWF_DELETE_ON_DESTROY | SplitterWindow::SWF_DOCK
			| (edge & 2					? SplitterWindow::SWF_HORZ		: SplitterWindow::SWF_VERT)
			| (edge & DOCK_RELATIVE		? SplitterWindow::SWF_PROP		: 0)
			| (flip						? SplitterWindow::SWF_FROM2ND	: 0)
		);

		if (!(edge & DOCK_FIXED_SIB) && !CastByProc<SplitterWindow>(sib) && !TabWindow::Cast(sib) && (!dw || !dw->FixedChild()))
			sib = *new TabWindow(s->_GetPanePos(~edge & 1), sib);

		if (edge & DOCK_FIXED) {
			if (edge & 1)
				s->SetPanesProportional(sib, Control(), prop);
			else
				s->SetPanesProportional(Control(), sib, prop);

			SetChild(*s);
			return s->_GetPanePos(edge & 1);
		}

		tab	= new TabWindow(s->_GetPanePos(edge & 1));

		if (edge & 1)
			s->SetPanesProportional(sib, *tab, prop);
		else
			s->SetPanesProportional(*tab, sib, prop);

		SetChild(*s);

	} else {
		if (edge == DOCK_TABID) {
			int			i;
			if (auto t = FindTab(dock.Child(), size, i))
				tab = t;
		}
		if (!tab) {
			if (auto *t = TabWindow::Cast(sib)) {
				tab = t;
			} else {
				tab = new TabWindow(dock.GetChildWindowPos(), sib);
				SetChild(*tab);
			}
		}
		tab->SetReplaceID(edge == DOCK_TABID);
	}
	return tab->GetChildWindowPos();
}

void Docker::SetChild(Control c) const {
	if (auto *dw = DockingWindow::Cast(dock)) {
		dw->SetChildImmediate(c);

	} else if (auto *sw = SplitterWindow::Cast(dock)) {
		sw->SetPane(pane, c);

	} else {
		ISO_ASSERT(0);
	}
}

Control Docker::GetSibling() const {
	if (auto *dw = DockingWindow::Cast(dock))
		return dw->GetChild();

	if (auto *sw = SplitterWindow::Cast(dock))
		return sw->GetPane(pane);

	ISO_ASSERT(0);
	unreachable();
}


} } //namesapce iso::win
