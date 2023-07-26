#ifndef TREECOLUMN_H
#define TREECOLUMN_H

#include "window.h"

//-----------------------------------------------------------------------------
//	TreeColumnControl
//-----------------------------------------------------------------------------
#define TCN_GETDISPINFO		(NM_LAST + 0)

// display info
struct NMTCCDISPINFO : NMTVDISPINFOA {
	int		iSubItem;
};

// custom draw
struct NMTCCCUSTOMDRAW : NMTVCUSTOMDRAW {
	int		iSubItem;
	RECT	rcItem;
	UINT	uAlign;
};

namespace iso { namespace win {

enum DROP_TYPE { DROP_BELOW, DROP_ABOVE, DROP_ON };

struct TreeControl2 : Subclass<TreeControl2, TreeControl> {
	Point		mouse_down;
	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CHAR:
				Super(message, wParam, lParam);
				return Parent().SendMessage(message, wParam, lParam);

			case WM_LBUTTONUP:
				clear(mouse_down);
				break;

			case WM_LBUTTONDOWN:
				mouse_down = Point(lParam);
				break;

			case WM_MOUSEMOVE:
				if ((wParam & MK_LBUTTON) && mouse_down != Point(0, 0)) {
					Point	move	= Point(lParam) - mouse_down;
					Point	drag	= SystemMetrics::DragSize();
					if (abs(move.x) > drag.x || abs(move.y) > drag.y) {
						if (HTREEITEM h = HitTest(mouse_down)) {
							NMTREEVIEW	nmtv;
							InitNotification(nmtv.hdr, *this, TVN_BEGINDRAG);
							nmtv.itemNew.hItem	= h;
							nmtv.ptDrag			= mouse_down;
							Parent()(WM_NOTIFY, id.get(), &nmtv);
						}
					}
				}
				break;

//			case WM_COMMAND:
//				return Parent()(message, wParam, lParam);
		}
		return Super(message, wParam, lParam);
	}
};


class TreeColumnControl : public Window<TreeColumnControl> {
protected:
	enum { OFFSET_HORIZ = 2, OFFSET_VERT = 1, OFFSET_HORIZ_HEADER = 7};

	struct _string_getter {
		const TreeColumnControl	*c;
		HTREEITEM		h;
		int				i;
		_string_getter(const TreeColumnControl *c, HTREEITEM h, int i) : c(c), h(h), i(i) {}
		size_t	string_len()						const	{ return 260; }
		size_t	string_get(char *s, size_t len)		const;
		size_t	string_get(char16 *s, size_t len)	const	{ return string_getter_transform<char>(*this, s, len); }
	};

	ScrollInfo			siHoriz;
	HeaderControl		header;
	TreeControl2		tree;
	int					ellipsis_width;
	HTREEITEM			drop_target;
	DROP_TYPE			drop_type;

	void	Draw(DeviceContext dc, HTREEITEM hItem, const Rect &rect, uint32 style, uint32 state, LPARAM lParam) const;
	void	AdjustRect(HTREEITEM hItem, int iSubItem, RECT *pRect, uint32 style)	const;

public:
	static const Style
		GRIDLINES		= Style(0x10000),
		HEADERAUTOSIZE	= Style(0x20000);

//	static HBRUSH		get_class_background()	{ return GetSysColorBrush(COLOR_WINDOW); }
						TreeColumnControl();
	LRESULT				Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	HeaderControl&		GetHeaderControl()			{ return header; }
	TreeControl&		GetTreeControl()			{ return tree;	}
	const HeaderControl& GetHeaderControl()	const	{ return header; }
	const TreeControl&	GetTreeControl()	const	{ return tree; }

	HTREEITEM			GetDropTarget(DROP_TYPE *type = 0)	const {
		if (type)
			*type = drop_type;
		return drop_target;
	}
	ImageList			CreateDragImage(HTREEITEM h);

	Rect				GetItemRect(HTREEITEM hItem, int iSubItem = -1, bool fromtree = false, bool labelsonly = false)	const;
	string_getter<_string_getter>	GetItemText(HTREEITEM hItem, int iSubItem)		const	{ return _string_getter(this, hItem, iSubItem); }
	void				AdjustRect(HTREEITEM hItem, int iSubItem, RECT *pRect)		const	{ AdjustRect(hItem, iSubItem, pRect, style); }
	HTREEITEM			HitTest(const POINT &pt, int &subitem, uint32 *flags = 0)	const;
	HTREEITEM			HitTest(const POINT &pt)									const;
	void				SetMinWidth(int i, int width)								const;

	void				UpdateScrollInfo();
	void				AdjustColumns();
	void				SetDropTarget(HTREEITEM hItem, DROP_TYPE type = DROP_BELOW);
	operator TreeControl() const { return tree; }

	HTREEITEM			GetSelectedItem()											const	{ return tree.GetSelectedItem(); }
	bool				SetSelectedItem(HTREEITEM hItem)							const	{ return tree.SetSelectedItem(hItem); }
	HTREEITEM			GetNextItem(HTREEITEM hItem)								const	{ return tree.GetNextItem(hItem); }
	HTREEITEM			GetPrevItem(HTREEITEM hItem)								const	{ return tree.GetPrevItem(hItem); }
	HTREEITEM			GetParentItem(HTREEITEM hItem)								const	{ return tree.GetParentItem(hItem); }
	bool				SetItemParam(HTREEITEM hItem, const arbitrary &p)			const	{ return tree.SetItemParam(hItem, p);  }
	arbitrary			GetItemParam(HTREEITEM hItem)								const	{ return tree.GetItemParam(hItem); }
	bool				DeleteItem(HTREEITEM hItem = TVI_ROOT)						const	{ return tree.DeleteItem(hItem); }
	TreeControl::Item	GetItem(HTREEITEM hItem, int mask = 0)						const	{ return tree.GetItem(hItem, mask); }
	void				DeleteChildren(HTREEITEM hItem)								const	{ tree.DeleteChildren(hItem); }
};

} } // namespace iso::win

#endif	//TREECOLUMN_H