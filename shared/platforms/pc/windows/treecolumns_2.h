#ifndef TREECOLUMN_H
#define TREECOLUMN_H

#include "window.h"

//-----------------------------------------------------------------------------
//	TreeColumnControl
//-----------------------------------------------------------------------------
#define TCS_GRIDLINES		(0x0001)
#define TCS_HEADERAUTOSIZE	(0x0002)
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

namespace win {

class TreeColumnControl : public Window<TreeColumnControl> {
protected:
	enum { OFFSET_HORIZ = 2, OFFSET_VERT = 1, OFFSET_HORIZ_HEADER = 7};

	struct TreeControl2 : Subclass<TreeControl2, TreeControl> {
		LRESULT		Proc(UINT message, WPARAM wParam, LPARAM lParam) {
			switch (message) {
				case WM_ERASEBKGND:
					return 1;
			}
			return Super(message, wParam, lParam);
		}
	};

	HeaderControl		header;
	TreeControl			tree;
	int					tree_col;

	void				UpdateScrollInfo(ScrollInfo	&siHoriz, ScrollInfo &siVert);
	void				UpdateScrollInfo() {
		ScrollInfo	siHorz	= GetScroll(SIF_RANGE | SIF_PAGE | SIF_POS, false);
		ScrollInfo	siVert	= GetScroll(SIF_RANGE | SIF_PAGE | SIF_POS, true);
		UpdateScrollInfo(siHorz, siVert);
	}
public:
	static HBRUSH		get_class_background()	{ return GetSysColorBrush(COLOR_WINDOW); }
	LRESULT				Proc(UINT message, WPARAM wParam, LPARAM lParam);

	HeaderControl&		GetHeaderControl()		{ return header; }
	TreeControl&		GetTreeControl()		{ return tree;	}

	Rect				GetItemRect(HTREEITEM hItem, int iSubItem, bool fromtree = false)	const;
	void				AdjustRect(HTREEITEM hItem, int iSubItem, RECT *pRect)				const;
	HTREEITEM			HitTest(const POINT &pt, int &subitem, uint32 *flags = 0)			const;
	HTREEITEM			HitTest(const POINT &pt)											const;
	void				SetMinWidth(int i, int width)										const;

	void				AdjustColumns();
	void				SetTreeColumn(int i);
	operator TreeControl() const { return tree; }
	TreeColumnControl() : tree_col(0) {}
};

}	// namespace win

#endif	TREECOLUMN_H