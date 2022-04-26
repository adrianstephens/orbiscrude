#include "treecolumns.h"

using namespace iso;
using namespace win;

//-----------------------------------------------------------------------------
//	TreeColumnControl
//-----------------------------------------------------------------------------
TreeColumnControl::TreeColumnControl() : siHoriz(SIF_RANGE | SIF_PAGE | SIF_POS), drop_target(0) {}

void TreeColumnControl::Draw(DeviceContext dc, HTREEITEM hItem, const Rect &rect, uint32 style, uint32 state, LPARAM lParam) const {
	int			num_cols		= header.Count();
	char		label[256]		= {0};

	NMTCCDISPINFO		dispInfo;
	InitNotification(dispInfo.hdr, *this, TCN_GETDISPINFO);
	dispInfo.item.mask			= TVIF_TEXT;
	dispInfo.item.hItem			= hItem;
	dispInfo.item.pszText		= label;
	dispInfo.item.cchTextMax	= int(num_elements(label) - 1);
	dispInfo.item.lParam		= lParam;

	// custom draw
	NMTCCCUSTOMDRAW		customDraw;
	InitNotification(customDraw.nmcd.hdr, *this, NM_CUSTOMDRAW);
	customDraw.nmcd.dwDrawStage = CDDS_ITEMPOSTPAINT;
	customDraw.nmcd.hdc			= dc;
	customDraw.nmcd.rc			= GetItemRect(hItem);
	customDraw.nmcd.dwItemSpec	= (DWORD_PTR)hItem;
	customDraw.nmcd.uItemState	= state;
	customDraw.nmcd.lItemlParam = lParam;

	customDraw.iLevel			= -2;

	if (state & TVIS_SELECTED) {
		customDraw.clrText		= Colour::SysColor(COLOR_HIGHLIGHTTEXT);
		customDraw.clrTextBk	= Colour::SysColor(COLOR_HIGHLIGHT);
	} else {
		customDraw.clrText		= Colour::SysColor(COLOR_BTNTEXT);
		customDraw.clrTextBk	= Colour::SysColor(COLOR_WINDOW);
	}

	// column rectangles
	for (int i = 0; i < num_cols; i++) {
		// rectangle
		Rect	columnRect	= header.GetItemRect(i);
		if (columnRect.right <= rect.left || columnRect.left >= rect.right)
			continue;

		columnRect.top		= rect.top;
		columnRect.bottom	= rect.bottom;
		customDraw.rcItem	= columnRect;
		AdjustRect(hItem, i, &customDraw.rcItem, style);

		// alignment
		switch (header.GetItem(i, HDI_FORMAT).Format() & HDF_JUSTIFYMASK) {
			case HDF_LEFT:		customDraw.uAlign	= DT_LEFT;		break;
			case HDF_RIGHT:		customDraw.uAlign	= DT_RIGHT;		break;
			case HDF_CENTER:	customDraw.uAlign	= DT_CENTER;	break;
		}

		dispInfo.iSubItem	= customDraw.iSubItem = i;

		// custom draw
		if (!SendMessage(WM_NOTIFY, customDraw.nmcd.hdr.idFrom, &customDraw)) {
			// display info
			if (!SendMessage(WM_NOTIFY, customDraw.nmcd.hdr.idFrom, &dispInfo))
				((TreeControl::Item&)dispInfo.item).Get(tree);

			// selection
			if (i == 0 || (style & TVS_FULLROWSELECT)) {
				RECT rcSelection = customDraw.rcItem;
				if (!(style & TVS_FULLROWSELECT))
					rcSelection.right	= min(rcSelection.left + dc.GetTextExtent(dispInfo.item.pszText).x + 2 * OFFSET_HORIZ, columnRect.right - 1);

				// draw
				if (rcSelection.right > rcSelection.left) {
					dc.Fill(rcSelection, win::Colour(customDraw.clrTextBk));
					if ((customDraw.nmcd.uItemState & CDIS_FOCUS) && GetFocus() == tree)
						dc.FocusRect(rcSelection);
				}
			}
			// rectangle
			if (i != 0)
				customDraw.rcItem.left += OFFSET_HORIZ_HEADER;

			InflateRect(&customDraw.rcItem, -OFFSET_HORIZ, -OFFSET_VERT);

			// label
			if (customDraw.rcItem.right > customDraw.rcItem.left) {
				dc.SetOpaque(false);
				dc.SetTextColour(win::Colour(customDraw.clrText));
				for (char *p = dispInfo.item.pszText; *p; p++) {
					if (*p < ' ')
						*p = ' ';
				}
				dc.DrawText(customDraw.rcItem, customDraw.uAlign | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, dispInfo.item.pszText);
			}
		}
		// grid
		if (style & GRIDLINES)
			dc.DrawEdge(columnRect, BDR_SUNKENINNER, BF_RIGHT | BF_BOTTOM);
	}
}

ImageList TreeColumnControl::CreateDragImage(HTREEITEM hItem) {
	DeviceContext	dc	= DeviceContext(tree).Compatible();
	Rect			r	= GetItemRect(hItem);
	int				w	= r.Width(), h = r.Height();
	Rect			r2(0, 0, w, h);
	Bitmap			bm(dc, w, h);

	auto	oldbm	= dc.Select(bm);
	auto	oldfnt	= dc.Select(tree.GetFont());

	dc.Fill(r2, colour::white);
	dc.SetTextColour(colour::black);
//	dc.TextOut(Point(0, 0), "hello");
	Draw(dc, hItem, r2, 0, 0, 0);

	dc.Select(oldfnt);
	dc.Select(oldbm);


	auto	il = ImageList::Create(w, h, ILC_COLOR | ILC_MASK, 0, 1);
	il.Add(bm, colour::green);
	return il;//tree.CreateDragImage(hItem);
}

static Rect GetDropRect(TreeControl tree, HTREEITEM hItem, DROP_TYPE type) {
	int		icons	= tree.GetImageList().GetIconSize().x;
	Rect	r		= tree.GetItemRect(hItem, true);
	switch (type) {
		default:
		case DROP_BELOW:	return r.Subbox(0, -4, 32, 0) + Point(-icons, +2);
		case DROP_ABOVE:	return r.Subbox(0,  0, 32, 4) + Point(-icons, -2);
		case DROP_ON:	return r + Point(-icons, 2);
	}
}

void TreeColumnControl::SetDropTarget(HTREEITEM hItem, DROP_TYPE type) {
	ImageList::DragShow(false);
	if (drop_target)
		tree.Invalidate(GetDropRect(tree, drop_target, drop_type));
	if (drop_target = hItem)
		tree.Invalidate(GetDropRect(tree, drop_target, drop_type = type));
	UpdateWindow(hWnd);
	ImageList::DragShow(true);
}

void TreeColumnControl::UpdateScrollInfo() {
	// adjust scrollbar
	int count		= header.Count();
	siHoriz.Set(0, count ? header.GetItemRect(count - 1).right - 1 : 0, GetClientRect().right);
	SetScroll(siHoriz, false);

	// reposition controls
	Rect		rect	= GetClientRect().SetLeft(-siHoriz.nPos).SetRight(siHoriz.nMax - siHoriz.nPos);
	header.Move(header.GetLayout(rect));
	rect.SetRight(siHoriz.nPage);
	tree.Move(rect);
}

void TreeColumnControl::AdjustColumns() {
	if (int count = header.Count()) {
		if (style & HEADERAUTOSIZE) {
			Rect	rect		= header.GetItemRect(count - 1);
			int		min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, count - 1).Param());
			int		width		= max(GetClientRect().Width() + siHoriz.nPos - rect.left, min_width);
			if (width != rect.Width())
				HeaderControl::Item().Width(width).Set(header, count - 1);
		}
		UpdateScrollInfo();
	}
}

Rect TreeColumnControl::GetItemRect(HTREEITEM hItem, int iSubItem, bool fromtree, bool labelsonly) const {
	Rect	rect = tree.GetItemRect(hItem, labelsonly);
	Rect	rect2;
	if (iSubItem < 0) {
		if (int count = header.Count())
			rect2		= header.GetItemRect(count - 1);
		else
			clear(rect2);
	} else {
		rect2	= header.GetItemRect(iSubItem);
		if (!labelsonly || iSubItem > 0)
			rect.left	= rect2.left;
	}
	rect.right	= rect2.right;
	if (!fromtree)
		rect += Point(-siHoriz.Pos(), rect2.bottom);
	return rect;
}

void TreeColumnControl::AdjustRect(HTREEITEM hItem, int iSubItem, RECT *rect, uint32 style) const {
	// grid
	if (style & GRIDLINES) {
		--rect->right;
		--rect->bottom;
	}

	// indent
	if (iSubItem == 0)
		rect->left = tree.GetItemRect(hItem, true).left;
}

HTREEITEM TreeColumnControl::HitTest(const POINT &pt, int &subitem, uint32 *flags) const {
	POINT posClient = pt;
	MapWindowPoints(hWnd, tree, &posClient, 1);

	HTREEITEM	h = tree.HitTest(posClient, flags);
	if (h) {
		subitem = header.Count();
		while (--subitem >= 0) {
			Rect rcColumn = header.GetItemRect(subitem);
			if (rcColumn.left <= posClient.x && rcColumn.right > posClient.x)
				break;
		}
	}
	return h;
}

HTREEITEM TreeColumnControl::HitTest(const POINT &pt) const {
	POINT posClient = pt;
	MapWindowPoints(hWnd, tree, &posClient, 1);
	return tree.HitTest(posClient);
}

void TreeColumnControl::SetMinWidth(int i, int width) const {
	HeaderControl::Item	item(HDI_WIDTH | HDI_LPARAM);
	item.Get(header, i);
	item.mask	= 0;
	if (int(item.Param()) != width) {
		item.Param(width);
		if (item.Width() < width)
			item.Width(width);
		item.Set(header, i);
	}
}

int save_dc;

LRESULT TreeColumnControl::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			Rect		rect	= GetClientRect();
			header.Create(*this, NULL, CHILD, NOEX, id);
			header.SetFont(Font::DefaultGui());
			header.Move(header.GetLayout(rect), SWP_SHOWWINDOW);

			tree.Create(*this, NULL, CHILD | CLIPSIBLINGS | VISIBLE, NOEX, rect, id);
			Point	size = DeviceContext::Screen().SelectContinue(tree.GetFont()).GetTextExtent("W...");
			ellipsis_width = size.x;
			break;
		}
		case WM_SIZE:
			if (int count = header.Count()) {
				if (style & HEADERAUTOSIZE) {
					Rect	rect		= header.GetItemRect(count - 1);
					int		min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, count - 1).Param());
					int		width		= max(Point(lParam).x + siHoriz.nPos - rect.left, min_width);
					if (width != rect.Width())
						HeaderControl::Item().Width(width).Set(header, count - 1);
				}
				UpdateScrollInfo();
			}
			break;

		case WM_HSCROLL:
			Scroll(siHoriz.ProcessScroll(wParam, 5), 0);
//			SetScroll(siHoriz, false);
			UpdateScrollInfo();
			break;

		case WM_KEYDOWN:
		case WM_CHAR:
		case WM_DROPFILES:
//		case WM_COMMAND:
//		case WM_PARENTNOTIFY:
			return Parent()(message, wParam, lParam);

		case WM_NOTIFY: {
			NMHDR *pnmh	= (NMHDR*)lParam;

			// tree
			if (pnmh->hwndFrom == tree) {
				switch (pnmh->code) {
					case NM_CUSTOMDRAW: {
						NMCUSTOMDRAW *pnmcd = (NMCUSTOMDRAW*)pnmh;
						if (pnmcd->rc.bottom == 0)
							return CDRF_DODEFAULT;

						switch (pnmcd->dwDrawStage) {
							case CDDS_PREPAINT: {
								int	ret = Parent()(message, wParam, lParam);
								return header.Count() ? CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT : CDRF_SKIPDEFAULT;
							}
							case CDDS_POSTPAINT:
								if (drop_target) {
									DeviceContext	dc(pnmcd->hdc);
									Rect	r = GetDropRect(tree, drop_target, drop_type);
									if (r.Overlaps(dc.GetClipBox())) {
										//ImageList::DragShow(false);
										dc.Fill(r, colour::black);
										//ImageList::DragShow(true);
									}
								}
								return Parent()(message, wParam, lParam);

							case CDDS_ITEMPREPAINT: {
								int		ret		= Parent()(message, wParam, lParam);
								//save_dc = ret == CDRF_NEWFONT ? SaveDC(pnmcd->hdc) : 0;
								save_dc = SaveDC(pnmcd->hdc);
								return CDRF_NOTIFYPOSTPAINT | ret;
							}

							case CDDS_ITEMPOSTPAINT: {
								if (save_dc)
									RestoreDC(pnmcd->hdc, save_dc);

								Rect	rect;
								GetClipBox(pnmcd->hdc, &rect);
								auto	hItem	= (HTREEITEM)pnmcd->dwItemSpec;
								Rect	rcLabel		= tree.GetItemRect(hItem);
								rect.top		= rcLabel.top;
								rect.bottom		= rcLabel.bottom;

								Draw(pnmcd->hdc, hItem, rect, style | tree.style, tree.GetItemState(hItem), pnmcd->lItemlParam);
								return CDRF_DODEFAULT;
							}
						}
						return CDRF_DODEFAULT;
					}

					case TVN_GETDISPINFO: {
						// display info
						NMTCCDISPINFO dispInfo;
						InitNotification(dispInfo.hdr, *this, TCN_GETDISPINFO);
						NMTVDISPINFO	*nmtvdi	= (NMTVDISPINFO*)pnmh;
						dispInfo.item			= nmtvdi->item;
						dispInfo.iSubItem		= 0;
						SendMessage(WM_NOTIFY, dispInfo.hdr.idFrom, (LPARAM)&dispInfo);
						nmtvdi->item			= dispInfo.item;
						break;
					}

					case NM_CLICK:
						if (Parent()(message, wParam, lParam) == 0) {
							uint32	flags;
							if (HTREEITEM hItem = tree.HitTest(tree.ToClient(GetMessageMousePos()), &flags)) {
								if (flags & TVHT_ONITEM)
									tree.SetSelectedItem(hItem);
							}
						}
						return 0;

					//case TVN_BEGINDRAG:
					//	NMTREEVIEW	nmtv;
				}
				return Parent()(message, wParam, lParam);

			// header
			} else if (pnmh->hwndFrom == header) {
				NMHEADER	*nmhd = (NMHEADER*)pnmh;
				switch (pnmh->code) {
//					case HDN_BEGINTRACK:
//						return (GetValue(GWL_STYLE) & HEADERAUTOSIZE) && nmhd->iItem == header.GetCount() - 1;

					case HDN_ITEMCHANGING:
						if (nmhd->pitem->mask & HDI_WIDTH) {
							int min_width = style & HEADERAUTOSIZE ? int(HeaderControl::Item(TCIF_PARAM).Get(header, nmhd->iItem).Param()) : 0;
							if (min_width)
								nmhd->pitem->cxy = max(nmhd->pitem->cxy, min_width);
							Rect	rect	= tree.GetClientRect();
							Rect	hr		= header.GetItemRect(nmhd->iItem);
							rect.left		= hr.right;
							rect.right		= 4096;
							ScrollWindowEx(tree, nmhd->pitem->cxy - hr.Width(), 0, &rect, 0, 0, 0, SW_ERASE | SW_INVALIDATE | SW_SCROLLCHILDREN);
							rect.left		= max(hr.left, hr.right - ellipsis_width);
							rect.right		= hr.right;
							tree.Invalidate(rect);
						}
						break;
					case HDN_ITEMCHANGED:
						if ((nmhd->pitem->mask & HDI_WIDTH) && (style & HEADERAUTOSIZE)) {
							int	last_index = header.Count() - 1;
							if (nmhd->iItem != last_index) {
								int	min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, last_index).Param());
								int width		= max(GetClientRect().Width() + siHoriz.nPos - header.GetItemRect(last_index).left, min_width);
								HeaderControl::Item().Width(width).Set(header, last_index);
							}
						}
						UpdateScrollInfo();
						break;
				}
				return Parent()(message, wParam, lParam);

			// this
			} else if (pnmh->hwndFrom == hWnd) {
				switch (pnmh->code) {
					case TCN_GETDISPINFO:
					case NM_CUSTOMDRAW:
						return Parent()(message, wParam, lParam);
				}
			}
			return Parent()(message, wParam, lParam);
		}
		case WM_ENABLE: {
			tree.EnableInput(!!wParam);
			header.EnableInput(!!wParam);
			break;
		}
	}

	return DefWindowProc(*this, message, wParam, lParam);
}

size_t TreeColumnControl::_string_getter::string_get(char *s, size_t len) const {
	if (i == 0)
		return c->GetTreeControl().GetItemText(h).get(s, len);

	NMTCCDISPINFO		dispInfo;
	InitNotification(dispInfo.hdr, *c, TCN_GETDISPINFO);
	dispInfo.item.mask			= TVIF_TEXT;
	dispInfo.item.hItem			= h;
	dispInfo.item.pszText		= s;
	dispInfo.item.cchTextMax	= uint32(len);
	dispInfo.item.lParam		= 0;
	dispInfo.iSubItem			= i;

	return c->SendMessage(WM_NOTIFY, (ID)c->id, (LPARAM)&dispInfo) ? strlen(s) : 0;

}

