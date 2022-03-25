#include "splitter.h"

using namespace iso::win;

//-----------------------------------------------------------------------------
//	Arranger
//-----------------------------------------------------------------------------

ControlArrangement::Token iso::win::ToolbarArrange[] = {
	ControlArrangement::VSplit(ToolBarControl::Height),
	ControlArrangement::ControlRect(0),
	ControlArrangement::ControlRect(1),
};

void iso::win::MoveWindows(Control *controls, const Rect *rects, int n) {
	while (n--)
		controls++->Move(*rects++);
}

void ControlArrangement::GetRects(const ControlArrangement::Token *p, Rect rect, Rect *rects) {
	for (;;) {
		uint16	value = p->value;
		if (value & SPLIT) {
			int		v	= int16(value << 3) >> 3;
			Rect	r1;
			switch ((value >> 13) & 3) {
				case 1: v = rect.Width() * v / VALUE_SCALE;
				case 0:	rect.SplitAtX(v, r1, rect); break;
				case 3: v = rect.Height() * v / VALUE_SCALE;
				case 2: rect.SplitAtY(v, r1, rect); break;
			}
			GetRects(p + 1, r1, rects);
			p += 2;
		} else if (value & SKIP) {
			p += 1 + (value & 0xff);
		} else {
			if (value != VALUE_MASK)
				rects[value & 0xff] = rect;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//	ArrangeWindow
//-----------------------------------------------------------------------------

LRESULT	ArrangeWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE:
			Arrange();
			return 0;

		case WM_CHAR:
//		case WM_COMMAND:
		case WM_NOTIFY:
		case WM_PARENTNOTIFY:
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
		case WM_DROPFILES:
			return Parent()(message, wParam, lParam);
	}
	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	SplitterWindow
//-----------------------------------------------------------------------------

void SplitAdjuster::Init(int _flags) {
	flags	= _flags;
	gripper	= flags & SWF_STATIC ? 0 : GetSystemMetrics(flags & SWF_VERT ? SM_CXSIZEFRAME : SM_CYSIZEFRAME) * 2;
}

void SplitterWindow::_SetPane(int i, Control c) {
	if ((pane[i] = c) && c.Parent() != *this)
		c.SetParent(*this);
	if (IsVisible())
		c.Show();
}

void SplitterWindow::Init(int _flags, int _pos) {
	SplitAdjuster::Init(_flags);
	pos		= _pos;
	offset	= 0;
	clear(rcGripper);
}

Rect SplitterWindow::_GetPaneRect(int i) const {
	Rect	r = GetClientRect();
	int		p = GetClientPos();
	if (flags & SWF_VERT) {
		if (i == 0)
			r.right = p;
		else
			r.left = p + gripper;
	} else {
		if (i == 0)
			r.bottom = p;
		else
			r.top = p + gripper;
	}
	return r;
}

Rect SplitterWindow::GetPaneRect(int i) const {
	if (!(flags & SWF_ALWAYSSPLIT) && !pane[i])
		return Rect(0, 0, 0, 0);

	Rect	r = GetClientRect();
	if (!(flags & SWF_ALWAYSSPLIT) && !pane[1 - i])
		return r;

	int		p = GetClientPos();
	if (flags & SWF_VERT) {
		if (i == 0)
			r.right = p;
		else
			r.left = p + gripper;
	} else {
		if (i == 0)
			r.bottom = p;
		else
			r.top = p + gripper;
	}
	return r;
}

void SplitterWindow::UpdateSplitter(bool always) {
	Rect	r = GetClientRect();

	if (always || (pane[0] && pane[1])) {
		Rect	r0, r1;
		int		p = GetClientPos();

	    if (flags & SWF_VERT) {
			r.SplitAtX(p, r0, r1);
			r1.SplitAtX(gripper, rcGripper, r1);
	    } else {
			r.SplitAtY(p, r0, r1);
			r1.SplitAtY(gripper, rcGripper, r1);
	    }

		pane[0].Move(r0);
		pane[1].Move(r1);
		Invalidate(rcGripper);
		pane[0].Update();
		pane[1].Update();

	} else if (pane[0]) {
		pane[0].Move(r);
		pane[0].Update();

	} else if (pane[1]) {
		pane[1].Move(r);
		pane[1].Update();

	} else if (pos == 0) {
		SetProportionalPos(MAXWORD / 2);
	}
}

void SplitterWindow::SetPaneAndSize(int i, Control c) {
	Control	prev	= pane[i];
	Rect	rx		= GetPaneRect(1 - i);	// other rect
	Point	adjust	= GetRect().Size() - GetClientRect().Size();

	if (pane[i] = c) {
		if (c.Parent() != *this)
			c.SetParent(*this);
		Rect	rn	= c.GetRelativeRect();		// new rect
		Rect	r	= rn | rx;
		SetClientPos(flags & SWF_VERT ? (i == 0 ? rn.right : rn.left - gripper) : (i == 0 ? rn.bottom : rn.top - gripper));
		Resize(r.Size() + adjust);
	} else {
		Resize(rx.Size() + adjust);
	}
}

LRESULT SplitterWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			pane[0] = pane[1] = Control();
			break;

		case WM_SIZE: {
			Point	size(lParam);
			range = (flags & SWF_VERT ? size.x : size.y) - gripper;
			UpdateSplitter(!!(flags & SWF_ALWAYSSPLIT));
			return 0;
		}

		case WM_LBUTTONDOWN: {
			Point	pt(lParam);
			if (rcGripper.Contains(pt)) {
				flags	|= SWF_DRAG;
				offset = flags & SWF_VERT ? rcGripper.left - pt.x : rcGripper.top - pt.y;
				SetCapture(hWnd);
			}
			SetCursor(LoadCursor(NULL, flags & SWF_VERT ? IDC_SIZEWE : IDC_SIZENS));
			return 0;
		}

		case WM_LBUTTONUP:
			ReleaseCapture();
			flags	&= ~SWF_DRAG;
			return 0;

		case WM_MOUSEMOVE: {
			Point	pt(lParam);
			if (flags & SWF_DRAG) {
				SetClientPos(clamp((flags & SWF_VERT ? pt.x : pt.y) + offset, 0, range));
				UpdateSplitter(!!(flags & SWF_ALWAYSSPLIT));
			} else if (!rcGripper.Contains(pt)) {
				return 0;
			}
			SetCursor(LoadCursor(NULL, flags & SWF_VERT ? IDC_SIZEWE : IDC_SIZENS));
			return 0;
		}

		case WM_PAINT:
			if ((flags & SWF_ALWAYSSPLIT) || (pane[0] && pane[1])) {
				DeviceContextPaint(*this).Fill(rcGripper, (HBRUSH)(COLOR_3DFACE + 1));
				return 0;
			}
			break;

		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_CREATE: {
					Control	c(lParam);
					int	w = !pane[0] && !pane[1]
							? c.GetRelativeRect().TopLeft() != Point(0, 0)
							: !pane[0] ? 0
							: !pane[1] ? 1
							: -1;
					if (w >= 0)
						SetPane(w, lParam);
					break;
				}

				case WM_DESTROY:
					Parent()(message, wParam, lParam);
					if (!(flags & SWF_ALWAYSSPLIT)) {
						Control	p	= Parent();
						int		w	= WhichPane(lParam);
						if (w >= 0) {
							pane[w] = Control();
							if (SplitterWindow *s = SplitterWindow::Cast(p))
								s->SetPane(s->WhichPane(*this), GetPane(1 - w));
							if (!(flags & SWF_NO_AUTO_DESTROY))
								Destroy();
						}
					}
					break;
			}
			break;

		case WM_NCDESTROY:
			if (flags & SWF_DELETE_ON_DESTROY)
				delete this;
			else
				hWnd = 0;
			return 0;

		default:
			if (message < WM_USER)
				break;

		case WM_CHAR:
		case WM_NOTIFY:
		case WM_DROPFILES:
			if (auto entry = recursion.check(MessageRecord(message, wParam, lParam)))
				return Parent()(message, wParam, lParam);
			break;

		case WM_COMMAND:
			if (HIWORD(wParam) != 0xffff)
				return Parent()(message, wParam, lParam);
			break;
	}

	return Super(message, wParam, lParam);
}

//-------------------------------------
// MultiSplitterWindow
//-------------------------------------

void MultiSplitterWindow::Init(int n, int flags) {
	SplitAdjuster::Init(flags);
	panes.resize(n);
	drag	= -1;
	for (int i = 0; i < n; i++)
		panes[i].edge = MAXWORD * (i + 1) / n;
}

void MultiSplitterWindow::UpdateSplitters() {
	if (panes) {
		Rect	r	= GetClientRect();
		Rect	r2	= r;

		panes.back().edge = flags & SWF_PROP ? MAXWORD : flags & SWF_VERT ? r.right : r.bottom;

		for (int i = 0; i < panes.size(); i++) {
			int	edge = ToAbsolute(panes[i].edge);
			(flags & SWF_VERT ? r2.right : r2.bottom) = edge;
			panes[i].c.Move(r2);
			(flags & SWF_VERT ? r2.left : r2.top) = edge + gripper;
		}
//		for (int i = 0; i < panes.size(); i++)
//			panes[i].c.Update();
	}
}

Rect MultiSplitterWindow::GetPaneRect(int i) const {
	Rect	r	= GetClientRect();
	if (i > 0)
		(flags & SWF_VERT ? r.left : r.top) = ToAbsolute(panes[i - 1].edge) + gripper;
	if (i < panes.size())
		(flags & SWF_VERT ? r.right : r.bottom) = ToAbsolute(panes[i].edge);
	return r;
}

void MultiSplitterWindow::SetPane(int i, Control c) {
	if ((panes[i].c = c) && c.Parent() != *this)
		c.SetParent(*this);
	c.Move(GetPaneRect(i));
}

int MultiSplitterWindow::WhichPane(Control c) const {
	for (auto &i : panes) {
		if (i.c == c)
			return panes.index_of(i);
	}
	return panes.size32();
}

WindowPos MultiSplitterWindow::InsertPane(int i, int w) {
	i	= clamp(i, 0, panes.size32());

	auto	pane = panes.insert(panes + i);
	pane->edge	= i ? pane[-1].edge : 0;

	while (pane < panes.end())
		pane++->edge += w;

	uint32	max = panes.back().edge / 2;
	uint32	val = flags & SWF_PROP ? MAXWORD : range;
	for (auto &i : panes)
		i.edge = i.edge / 2 * val / max;

	UpdateSplitters();
	return GetPanePos(i);
}

void MultiSplitterWindow::RemovePane(int i) {
	if (i < panes.size())
		panes.erase(panes + i);
}


LRESULT MultiSplitterWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE: {
			Point	size(lParam);
			range = flags & SWF_VERT ? size.x : size.y;
			UpdateSplitters();
			return 0;
		}

		case WM_LBUTTONDOWN:
			if (panes) {
				Point	pt(lParam);
				int		x = flags & SWF_VERT ? pt.x : pt.y;
				for (int i = 0; i < panes.size32() - 1; i++) {
					int	p = ToAbsolute(panes[i].edge);
					if (between(x, p, p + gripper)) {
						drag	= i;
						offset	= p - x;
						SetCapture(hWnd);
					}
				}
				SetCursor(LoadCursor(NULL, flags & SWF_VERT ? IDC_SIZEWE : IDC_SIZENS));
			}
			return 0;

		case WM_LBUTTONUP:
			ReleaseCapture();
			drag	= -1;
			return 0;

		case WM_MOUSEMOVE: {
			Point	pt(lParam);
			int		x = flags & SWF_VERT ? pt.x : pt.y;
			if (drag >= 0) {
				int	a = drag > 0 ? ToAbsolute(panes[drag - 1].edge) + gripper * 2 : 0;
				int	b = drag < panes.size32() - 2 ? ToAbsolute(panes[drag + 1].edge) : range;
				panes[drag].edge = FromAbsolute(clamp(x + offset, a, b));
				UpdateSplitters();
			} else if (panes) {
				for (int i = 0; i < panes.size32() - 1; i++) {
					int	p = ToAbsolute(panes[i].edge);
					if (between(x, p, p + gripper)) {
						SetCursor(LoadCursor(NULL, flags & SWF_VERT ? IDC_SIZEWE : IDC_SIZENS));
						break;
					}
				}
			}
			return 0;
		}

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			dc.Fill(dc.GetDirtyRect(), (HBRUSH)(COLOR_3DFACE + 1));
			return 0;
		}

		case WM_NCDESTROY:
			if (flags & SWF_DELETE_ON_DESTROY)
				delete this;
			else
				hWnd = 0;
			return 0;

		default:
			if (message < WM_USER)
				break;
		case WM_CHAR:
		case WM_NOTIFY:
//		case WM_COMMAND:
		case WM_DROPFILES:
			if (!(flags & SWF_PASSUP)) {
				flags |= SWF_PASSUP;
				int	r = Parent()(message, wParam, lParam);
				flags &= ~SWF_PASSUP;
				return r;
			}
			break;
	}

	return Super(message, wParam, lParam);
}

//-------------------------------------
// InfiniteSplitterWindow
//-------------------------------------

void InfiniteSplitterWindow::Split(int a) {
	int		b		= 1 - a;
	uint32	flags2	= (flags & ~SWF_DRAG) ^ ((flags & SWF_FLIP) ? SWF_VERT : 0);
	InfiniteSplitterWindow *split	= new InfiniteSplitterWindow(flags2);
	split->Create(GetChildWindowPos(), NULL, VISIBLE | CHILD | CLIPCHILDREN);//, CLIENTEDGE);
	split->Rebind(split);
	split->SetPane(1, SendMessage(WM_ISO_NEWPANE));

	SetPane(a, *split);

	if (!InfiniteSplitterWindow::Cast(GetPane(b))) {
		InfiniteSplitterWindow *split = new InfiniteSplitterWindow(flags2);
		split->Create(GetChildWindowPos(), NULL, VISIBLE | CHILD | CLIPCHILDREN);//, CLIENTEDGE);
		split->Rebind(split);
		split->SetPane(b, GetPane(b));
		if (b == 0)
			split->SetProportionalPos(MAXWORD);
		SetPane(b, *split);
	}
}

void InfiniteSplitterWindow::Unsplit(int a, bool root) {
	if (root) {
		GetPane(0).Destroy();
		SetPane(0, Control());
		if (a == 1) {
			SetPos(0);
			SplitterWindow::UpdateSplitter(true);
		}
		return;
	}

	int	b = 1 - a;
	GetPane(a).Destroy();
	SetPane(a, Control());

	if (InfiniteSplitterWindow *split = (InfiniteSplitterWindow*)InfiniteSplitterWindow::Cast(GetPane(b))) {
		bool	w;
		if ((w = !split->GetPane(0)) || !split->GetPane(1)) {
			SetPane(b, split->GetPane(int(w)));
			split->Destroy();
		}
	}
}

void InfiniteSplitterWindow::UpdateSplitter(bool root) {
	if (Dragging()) {
		if (GetPane(0) || GetPane(1)) {
			if (GetPane(0)) {
				if (GetPos() == 0)
					Unsplit(0, root);
			} else if (GetPos() > 0 && GetProportionalPos() < MAXWORD) {
				Split(0);
			}
			if (GetPane(1)) {
				if (GetProportionalPos() == MAXWORD)
					Unsplit(1, root);
			} else if (GetProportionalPos() < MAXWORD) {
				Split(1);
			}
		}
	} else {
		if (GetPane(0) || GetPane(1)) {
			if (!GetPane(0) && GetPos() == 0 && range && GetProportionalPos() < MAXWORD)
				SetCursor(LoadCursor(NULL, IDC_UPARROW));

			if (!GetPane(1) && range && GetProportionalPos() == MAXWORD)
				SetCursor(LoadCursor(NULL, IDC_UPARROW));
		}
	}
}

LRESULT InfiniteSplitterWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_ISO_NEWPANE)
		return Parent()(message, wParam, lParam);

	int r = SplitterWindow::Proc(message, wParam, lParam);
	if (message == WM_MOUSEMOVE)
		UpdateSplitter(false);

	return r;
}

LRESULT InfiniteSplitterWindow::Super(UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_ISO_NEWPANE)
		return Parent()(message, wParam, lParam);

	int r = SplitterWindow::Proc(message, wParam, lParam);
	if (message == WM_MOUSEMOVE)
		UpdateSplitter(true);

	return r;
}