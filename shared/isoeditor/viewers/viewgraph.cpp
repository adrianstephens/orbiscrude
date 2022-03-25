#include "main.h"
#include "iso/iso.h"
#include "iso/iso_script.h"
#include "maths/geometry.h"
#include "utilities.h"

using namespace app;

static const int curvemin = 50;

struct UIEdit {
	ISO_ptr<void>	p;
	int	x, y;
};

ISO_DEFUSERCOMPV(UIEdit, p, x, y);

static int GetIndex(TreeControl tree, HTREEITEM hItem) {
	return tree.GetItemParam(hItem);
}

ISO::Browser	GetBrowser(TreeControl tree, HTREEITEM hItem, ISO::Browser root) {
	if (!hItem)
		return root;
	return GetBrowser(tree, tree.GetParentItem(hItem), root)[GetIndex(tree, hItem)];
}

ISO::Browser	GetBrowser(TreeControl tree, TVITEMA &item, ISO::Browser root) {
	return GetBrowser(tree, tree.GetParentItem(item.hItem), root)[int(item.lParam)];
}

class MiniWindow : public Window<MiniWindow> {
	friend class ViewGraph;

	struct Link {
		MiniWindow	*other;
		int			index;
		Link(MiniWindow *_other, int _index) : other(_other), index(_index)	{}
	};

	ISO_ptr<UIEdit>			uiedit;
	TreeColumnControl		treecolumn;
	dynamic_array<Link>		linksto;
	dynamic_array<Link>		linksfrom;

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {

			case WM_CREATE: {
				treecolumn.Create(GetChildWindowPos(), NULL, CHILD | VISIBLE | HSCROLL | TCS_GRIDLINES | TCS_HEADERAUTOSIZE, CLIENTEDGE | ACCEPTFILES);
				HeaderControl	header	= treecolumn.GetHeaderControl();
				header.SetValue(GWL_STYLE, CHILD | VISIBLE | HDS_FULLDRAG);
				HeaderControl::Item("Symbol").Format(HDF_LEFT).Width(100).Insert(header, 0);
				HeaderControl::Item("Value").Format(HDF_LEFT).Width(100).Insert(header, 1);

				ISOTree(treecolumn.GetTreeControl()).Setup(ISO::Browser(uiedit->p), TVI_ROOT, 0);
				break;
			}

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				dc.Fill(dc.rcPaint, (HBRUSH)(COLOR_3DFACE + 1));
				break;
			}

			case WM_MOVE: {
				treecolumn.Move(GetClientRect());
				Point		pos(lParam);
				Rect		rect(pos, pos);
				AdjustWindowRect(&rect, CHILD | OVERLAPPED | CAPTION, false);
				int			dx		= rect.left - uiedit->x;
				int			dy		= rect.top  - uiedit->y;
				uiedit->x = rect.left;
				uiedit->y = rect.top;

				Rect		rect1	= GetRelativeRect();

				if (dx > 0)
					rect1.left -= dx;
				else
					rect1.right -= dx;

				if (dy > 0)
					rect1.top -= dy;
				else
					rect1.bottom -= dy;

				for (int i = 0; i < linksto.size(); i++) {
					Rect	rect2	= linksto[i].other->GetRelativeRect();
					rect2.right		= max(rect1.right + curvemin, rect2.left);
					rect2.left		= min(rect1.right, rect2.left - curvemin);
					rect2.top		= min(rect1.top, rect2.top);
					rect2.bottom	= max(rect1.bottom, rect2.bottom);
					Parent().Invalidate(rect2);
				}
				for (int i = 0; i < linksfrom.size(); i++) {
					Rect	rect2	= linksfrom[i].other->GetRelativeRect();
					rect2.left		= min(rect1.left - curvemin, rect2.right);
					rect2.right		= max(rect1.left, rect2.right + curvemin);
					rect2.top		= min(rect1.top, rect2.top);
					rect2.bottom	= max(rect1.bottom, rect2.bottom);
					Parent().Invalidate(rect2);
				}
				break;
			}

			case WM_LBUTTONDOWN:
				SetFocus();
				break;


			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				TreeControl		tree	= treecolumn.GetTreeControl();
				HeaderControl	header	= treecolumn.GetHeaderControl();
				NMTREEVIEW		*nmtv	= (NMTREEVIEW*)nmh;
				switch (nmh->code) {
					case TCN_GETDISPINFO: {
						NMTCCDISPINFO *nmdi = (NMTCCDISPINFO*)nmh;
						switch (nmdi->iSubItem) {
							case 1: {
								try {
									ISO::Browser		b(GetBrowser(tree, nmdi->item, ISO::Browser(uiedit->p)));
									if (const char *ext = b.External()) {
										sprintf(nmdi->item.pszText, ext);
									} else {
										if (b.GetType() == ISO::REFERENCE && *(ISO_ptr<void>*)b) {
											if (((ISO_ptr<void>*)b)->Flags() & ISO::Value::CRCTYPE) {
												sprintf(nmdi->item.pszText, "unknown type");
												break;
											}
											if (b.GetName() && tag(b.GetName()) != tree.GetItemText(nmdi->item.hItem)) {
												strcpy(nmdi->item.pszText, tag(b.GetName()));
												break;
											}
											b = *b;
										}
										memory_writer	m(memory_block(nmdi->item.pszText, nmdi->item.cchTextMax - 1));
										ISO::ScriptWriter(m).DumpData(b);
										nmdi->item.pszText[m.length()] = 0;
									}
								} catch (const char *s) {
									sprintf(nmdi->item.pszText, (const char*)s);
								}
//								sprintf(nmdi->item.pszText, "some value");
								break;
							}
						}
						return nmdi->iSubItem;
					}
				}
				break;
			}

			case WM_NCDESTROY:
				delete this;
				break;

			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}

	MiniWindow(HWND hWndParent, ISO_ptr<UIEdit> &_uiedit) : uiedit(_uiedit) {
		Create(WindowPos(hWndParent, Rect(uiedit->x, uiedit->y, 100, 100)), uiedit.ID() ? tag(uiedit.ID()) : uiedit->p.ID() ? tag(uiedit->p.ID()) : "unnamed", CHILD | VISIBLE | CLIPSIBLINGS | OVERLAPPED | CAPTION, NOEX);
	}

	void	AddLinkTo(MiniWindow *dest, int index)		{ new (linksto)		Link(dest, index);	}
	void	AddLinkFrom(MiniWindow *srce, int index)	{ new (linksfrom)	Link(srce, index);	}

	void MoveBy(Point &move) {
		Move(Rect(uiedit->x, uiedit->y, 100, 100) + move);
	}
};

class ViewGraph : public aligned<Window<ViewGraph>, 16> {
	ISO_ptr<UIEdit>	root;
	dynamic_array<MiniWindow*>	windows;
	float	scale;
	Point	mouse;

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE: {
				break;
			}

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				Rect		client	= GetClientRect();
				dc.Fill(dc.rcPaint, (HBRUSH)(COLOR_3DFACE + 1));

				Pen			old_pen = dc.Select(Pen(colour::black, 4, PS_SOLID));

				for (int i = 0; i < windows.size(); i++) {
					MiniWindow	*mini	= windows[i];
					Rect		rfrom	= mini->GetRelativeRect();
					for (int j = 0; j < mini->linksto.size(); j++) {
						int		index	= mini->linksto[j].index;
						Rect	rto		= mini->linksto[j].other->GetRelativeRect();
						dc.MoveTo(rfrom.right, rfrom.top + (index + 1) * 10, NULL);
//						dc.LineTo(rto.left, rto.top + index * 10);
						int		t		= max((rto.left - rfrom.right) / 3, curvemin);
						POINT	bez[3] = {
							rfrom.right + t, rfrom.top + (index + 1) * 10,
							rto.left - t, rto.top + 10,
							rto.left, rto.top + 10,
						};
						dc.PolyBezierTo(bez, 3);
					}
				}

				dc.Select(old_pen).Destroy();
				break;
			}
			case WM_RBUTTONDOWN:
			case WM_LBUTTONDOWN:
				mouse		= Point(lParam);
				SetFocus();
				break;

			case WM_MOUSEMOVE: {
				Point	pt(lParam);
				if (wParam & MK_LBUTTON) {
					Point	move = pt - mouse;
					Scroll(move.x, move.y);
				} else if (wParam & MK_RBUTTON) {
				}
				mouse = pt;
				break;
			}

			case WM_MOUSEWHEEL: {
				Point	pt = ToClient(Point(lParam));
				float	mult = iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				scale *= mult;

				for (size_t i = 0, n = windows.size(); i < n; i++) {
					Rect	rect = windows[i]->GetRelativeRect();
					Point	topleft	= (rect.TopLeft() - pt) * mult + pt;
					windows[i]->Move(Rect(topleft, topleft + Point(100,100) * scale));
				}
				break;
			}

			case WM_NCDESTROY:
				delete this;
				break;

			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}

	void	FindWindows(MiniWindow *source, ISO::Browser b);

	ViewGraph(const WindowPos &wpos, const ISO_ptr<void> &_p) : root(_p), scale(1) {
		Create(wpos, NULL, CHILD | VISIBLE | CLIPCHILDREN, CLIENTEDGE);
		MiniWindow	*mini = new MiniWindow(*this, root);
		windows.push_back(mini);
		FindWindows(mini, ISO::Browser(root->p));
	}
};

void ViewGraph::FindWindows(MiniWindow *source, ISO::Browser b)
{
	for (int i = 0, n = b.Count(); i < n; i++) {
		ISO::Browser	b2 = b[i];
		if (b2.GetType() == ISO::REFERENCE && (*b2).Is<UIEdit>()) {
			ISO_ptr<UIEdit>	&uiedit = *(ISO_ptr<UIEdit>*)b2;
			MiniWindow		*mini = NULL;

			for (int j = 0; j < windows.size(); j++) {
				if (windows[j]->uiedit == uiedit) {
					mini = windows[j];
					break;
				}
			}

			if (!mini) {
				mini = new MiniWindow(*this, uiedit);
				windows.push_back(mini);
				FindWindows(mini, ISO::Browser(((UIEdit*)(*b2))->p));
			}

			source->AddLinkTo(mini, i);
			mini->AddLinkFrom(source, i);
		}
	}
}


class EditorGraph : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->SameAs<UIEdit>();
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &p) {
		return *new ViewGraph(wpos, p);
	}
public:
	EditorGraph() {
		ISO::getdef<UIEdit>();
	}
} editorntest;
