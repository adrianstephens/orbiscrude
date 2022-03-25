#include "TreeColumns.h"

using namespace win;

//-----------------------------------------------------------------------------
//	TreeColumnControl
//-----------------------------------------------------------------------------
TreeColumnControl::TreeColumnControl() : siHoriz(SIF_RANGE | SIF_PAGE | SIF_POS), tree_col(0) {
}

void TreeColumnControl::UpdateScrollInfo() {
	// adjust scrollbar
	int count		= header.Count();
	siHoriz.Set(0, count ? header.GetItemRect(count - 1).right - 1 : 0, GetClientRect().right);
	SetScroll(siHoriz, false);

	// reposition controls
	Rect		rect		= GetClientRect().SetLeft(-siHoriz.nPos).SetRight(siHoriz.nMax - siHoriz.nPos);
	WINDOWPOS	wp;
	header.GetLayout(rect, &wp);
	header.Move(&wp);

	Rect		rect_col	= header.GetItemRect(tree_col);
	rect.SetLeft(rect_col.left);//.SetRight(rect_col.right);
	tree.Move(rect);
}

void TreeColumnControl::AdjustColumns() {
	if (int count = header.Count()) {
		if (style & TCS_HEADERAUTOSIZE) {
			Rect	rect		= header.GetItemRect(count - 1);
			int		min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, count - 1).Param());
			int		width		= max(GetClientRect().Width() + siHoriz.nPos - rect.left, min_width);
			if (width != rect.Width())
				HeaderControl::Item().Width(width).Set(header, count - 1);
		}
		UpdateScrollInfo();
	}
}

bool TreeColumnControl::GetItemRect(HTREEITEM hItem, int iSubItem, RECT *rect, bool fromtree) const {
	if (!tree.GetItemRect(hItem, rect))
		return false;

	if (iSubItem < 0) {
		if (int count = header.Count()) {
			Rect	rect2	= header.GetItemRect(count - 1);
			rect->right		= rect2.right;
			if (!fromtree) {
				rect->left		-= siHoriz.Pos();
				rect->right		-= siHoriz.Pos();
				rect->top		+= rect2.bottom;
				rect->bottom	+= rect2.bottom;
			}
		}
	} else {
		Rect	rect2	= header.GetItemRect(iSubItem);
		rect->left		= rect2.left;
		rect->right		= rect2.right;
		if (!fromtree) {
			rect->left		-= siHoriz.Pos();
			rect->right		-= siHoriz.Pos();
			rect->top		+= rect2.bottom;
			rect->bottom	+= rect2.bottom;
		}
	}
	return true;
}

Rect TreeColumnControl::GetItemRect(HTREEITEM hItem, int iSubItem, bool fromtree) const {
	Rect	rect = tree.GetItemRect(hItem);
	if (iSubItem < 0) {
		if (int count = header.Count()) {
			Rect	rect2	= header.GetItemRect(count - 1);
			rect.right		= rect2.right;
			if (!fromtree)
				rect += Point(-siHoriz.Pos(), rect2.bottom);
		}
	} else {
		Rect	rect2	= header.GetItemRect(iSubItem);
		rect.left		= rect2.left;
		rect.right		= rect2.right;
		if (!fromtree)
			rect += Point(-siHoriz.Pos(), rect2.bottom);
	}
	return rect;
}

void TreeColumnControl::AdjustRect(HTREEITEM hItem, int iSubItem, RECT *rect, bool label) const {
	// grid
	if (style & TCS_GRIDLINES) {
		rect->right		-= 1;
		rect->bottom	-= 1;
	}

	// indent
	if (label && iSubItem == tree_col)
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

void TreeColumnControl::SetTreeColumn(int i) {
	tree_col = i;
	UpdateScrollInfo();
}


void TreeColumnControl::DrawSubItem(DeviceContext &dc, int i, const Rect &rect, NMCUSTOMDRAW *pnmcd) {
	HTREEITEM		hItem		= (HTREEITEM)pnmcd->dwItemSpec;
	ID				id			= this->id;
	char			label[256]	= {0};

	NMTCCDISPINFO	dispInfo;
	dispInfo.hdr.hwndFrom		= hWnd;
	dispInfo.hdr.idFrom			= id;
	dispInfo.hdr.code			= TCN_GETDISPINFO;
	dispInfo.iSubItem			= i;
	dispInfo.item.hItem			= hItem;
	dispInfo.item.pszText		= label;
	dispInfo.item.cchTextMax	= int(num_elements(label) - 1);
	dispInfo.item.lParam		= pnmcd->lItemlParam;

	// custom draw
	NMTCCCUSTOMDRAW	customDraw;
	(NMTVCUSTOMDRAW&)customDraw	= *(NMTVCUSTOMDRAW*)pnmcd;
	customDraw.nmcd.hdr.hwndFrom= hWnd;
	customDraw.nmcd.hdr.idFrom	= id;
	customDraw.iSubItem			= i;

	// alignment
	switch (header.GetItem(i, HDI_FORMAT).Format() & HDF_JUSTIFYMASK) {
		case HDF_LEFT:		customDraw.uAlign	= DT_LEFT;		break;
		case HDF_RIGHT:		customDraw.uAlign	= DT_RIGHT;		break;
		case HDF_CENTER:	customDraw.uAlign	= DT_CENTER;	break;
	}

	customDraw.rcItem = rect;
	if (i == tree_col)
		customDraw.rcItem.left = tree.GetItemRect(hItem, true).left;

	// custom draw
	if (!SendMessage(WM_NOTIFY, id, (LPARAM)&customDraw)) {
		// display info
		if (!SendMessage(WM_NOTIFY, id, (LPARAM)&dispInfo)) {
			dispInfo.item.mask = TVIF_TEXT;
			((TreeControl::Item&)dispInfo.item).Get(tree);
		}

		// selection
		int			tree_style		= tree.style;
		if (i == tree_col || (tree_style & TVS_FULLROWSELECT)) {
			Rect	rcSelection		= rect;
			if (!(tree_style & TVS_FULLROWSELECT))
				rcSelection.right	= min(rect.left + dc.GetTextExtent(dispInfo.item.pszText).x + 2 * OFFSET_HORIZ, rect.right - 1);

			// draw
			if (rcSelection.right > rcSelection.left) {
				dc.Fill(rcSelection, Brush(customDraw.clrTextBk));

				if (customDraw.nmcd.uItemState & CDIS_FOCUS && GetFocus() == tree)
					dc.FocusRect(rcSelection);
			}
		}
		// rectangle
		if (i != tree_col)
			customDraw.rcItem.left += OFFSET_HORIZ_HEADER;

		InflateRect(&customDraw.rcItem, -OFFSET_HORIZ, -OFFSET_VERT);

		// label
		if (customDraw.rcItem.right > customDraw.rcItem.left) {
			dc.SetOpaque(false);
			dc.SetTextColour(customDraw.clrText);
			for (char *p = dispInfo.item.pszText; *p; p++) {
				if (*p < ' ')
					*p = ' ';
			}
			dc.DrawText(customDraw.rcItem, customDraw.uAlign | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, dispInfo.item.pszText);
		}
	}
}

int save_dc;

LRESULT TreeColumnControl::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			Rect		rect	= GetClientRect();
			WINDOWPOS	pos;

			header.Create(*this, NULL, WS_CHILD, 0, id);
			header.SetFont(Font::DefaultGui());
			header.Move(header.GetLayout(rect, &pos), SWP_SHOWWINDOW);

			tree.Create(*this, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0, rect, id);
			tree.Class().background = 0;
			break;
		}
		case WM_SIZE: {
			if (int count = header.Count()) {
				if (style & TCS_HEADERAUTOSIZE) {
					Rect	rect		= header.GetItemRect(count - 1);
					int		min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, count - 1).Param());
					int		width		= max(Point(lParam).x + siHoriz.nPos - rect.left, min_width);
					if (width != rect.Width())
						HeaderControl::Item().Width(width).Set(header, count - 1);
				}
				UpdateScrollInfo();
			}
			break;
		}

		case WM_HSCROLL:
			Scroll(siHoriz.ProcessScroll(wParam, 5), 0);
//			SetScroll(siHoriz, false);
			UpdateScrollInfo();
			break;

		case WM_CHAR:
			return Parent()(message, wParam, lParam);
		case WM_COMMAND:
		case WM_DROPFILES:
			return Parent()(message, wParam, lParam);

		case WM_NOTIFY: {
			NMHDR *pnmh	= (NMHDR*)lParam;
			//ISO_TRACEF("Notification: 0x%08x\n", pnmh->code);

			// tree
			if (pnmh->hwndFrom == tree) {
				if (pnmh->code == NM_CUSTOMDRAW) {

					NMCUSTOMDRAW *pnmcd = (NMCUSTOMDRAW*)pnmh;
					switch (pnmcd->dwDrawStage) {
						case CDDS_PREPAINT: {
							int	ret = Parent()(message, wParam, lParam);
							DeviceContext	dc(pnmcd->hdc);
							save_dc	= dc.Save();
							return header.Count() ? CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYPOSTERASE : CDRF_SKIPDEFAULT;
						}
						case CDDS_POSTPAINT:
							return Parent()(message, wParam, lParam);

						case CDDS_ITEMPREPAINT: {
							int		ret		= Parent()(message, wParam, lParam);
							DeviceContext	dc(pnmcd->hdc);
							//save_dc = ret == CDRF_NEWFONT ? SaveDC(pnmcd->hdc) : 0;
							if (save_dc)
								RestoreDC(dc, save_dc);
							save_dc	= dc.Save();
							dc		&= Rect(0, 0, header.GetItemRect(tree_col).Width(), 4096);
//							return CDRF_NOTIFYPOSTERASE | ret;
							return CDRF_NOTIFYPOSTPAINT | ret;
						}

						case CDDS_ITEMPOSTPAINT: {
							DeviceContext	dc0(*this);
							DeviceContext	dc(pnmcd->hdc);

							if (save_dc)
								RestoreDC(dc, save_dc);

							dc0.Select(Font::DefaultGui());

							HTREEITEM	hItem			= (HTREEITEM)pnmcd->dwItemSpec;
							int			num_cols		= header.Count();
							char		label[256]		= {0};

							// column rectangles
							Rect	rcItem0		= dc0.GetClipBox();
							Rect	rcItem		= dc.GetClipBox();
							Rect	rcTreeCol	= header.GetItemRect(tree_col);
							Rect	rcLabel;

							// draw columns
							if (tree.GetItemRect(hItem, &rcLabel, true)) {
								if (style & TCS_GRIDLINES)
									rcLabel.bottom	-= 1;
								for (int i = 0; i < num_cols; i++) {
									Rect	rect		= header.GetItemRect(i);
									rect.top	= rcLabel.top;
									rect.bottom	= rcLabel.bottom;
									if (rect.right > rcItem0.left && rect.left < rcItem0.right) {
										if (style & TCS_GRIDLINES)
											rect.right	-= 1;

										if (i < tree_col) {
											rect.top	+= rcTreeCol.bottom;
											rect.bottom	+= rcTreeCol.bottom;
											DrawSubItem(dc0, i, rect, pnmcd);
										} else {
											rect.left	-= rcTreeCol.left;
											rect.right	-= rcTreeCol.left;
											if (i != tree_col)
												DrawSubItem(dc, i, rect, pnmcd);
										}

										// grid
										if (style & TCS_GRIDLINES)
											dc.DrawEdge(rect, BDR_SUNKENINNER, BF_RIGHT | BF_BOTTOM);

									}
								}
							}
							return CDRF_DODEFAULT;
						}
					}
					return CDRF_DODEFAULT;

				} else if (pnmh->code == TVN_GETDISPINFO) {
					// display info
					NMTCCDISPINFO dispInfo;
					(NMTVDISPINFO&)dispInfo	= *(NMTVDISPINFO*)pnmh;
					dispInfo.hdr.hwndFrom	= hWnd;
					dispInfo.hdr.idFrom		= GetID();
					dispInfo.hdr.code		= TCN_GETDISPINFO;
					dispInfo.iSubItem		= 0;

					SendMessage(WM_NOTIFY, dispInfo.hdr.idFrom, (LPARAM)&dispInfo);

				} else if (pnmh->code == NM_CLICK) {
					if (Parent()(message, wParam, lParam) == 0) {
						uint32	flags;
						if (HTREEITEM hItem = tree.HitTest(tree.ToClient(GetMousePos()), &flags)) {
							if (flags & TVHT_ONITEM)
								tree.SetSelectedItem(hItem);
						}
					}
					break;
				}
				return Parent()(message, wParam, lParam);

			// header
			} else if (pnmh->hwndFrom == header) {
				NMHEADER	*nmhd = (NMHEADER*)pnmh;
				switch (pnmh->code) {
//					case HDN_BEGINTRACK:
//						return (GetValue(GWL_STYLE) & TCS_HEADERAUTOSIZE) && nmhd->iItem == header.GetCount() - 1;

					case HDN_ITEMCHANGING:
						if (nmhd->pitem->mask & HDI_WIDTH) {
							int min_width = style & TCS_HEADERAUTOSIZE ? int(HeaderControl::Item(TCIF_PARAM).Get(header, nmhd->iItem).Param()) : 0;
							if (min_width)
								nmhd->pitem->cxy = max(nmhd->pitem->cxy, min_width);
							Rect	hr		= header.GetItemRect(nmhd->iItem);
							int		scroll	= nmhd->pitem->cxy - hr.Width();
							int		left	= header.GetItemRect(tree_col).Left();
							if (style & TCS_GRIDLINES)
								hr.right	-= 1;
							if (nmhd->iItem >= tree_col) {
								Rect	rect	= tree.GetClientRect();
								rect.left		= hr.right - left;
								rect.right		= 4096;
								tree.Scroll(scroll, 0, SW_ERASE | SW_INVALIDATE | SW_SCROLLCHILDREN, &rect);
								rect.left		= max(hr.left, hr.right - 20) - left;
								rect.right		= hr.right - left;
								//tree.Invalidate(rect);
							} else {
								Rect	rect	= GetClientRect();
								rect.top		= hr.bottom;
								rect.left		= hr.right;
								Scroll(scroll, 0, SW_ERASE | SW_INVALIDATE | SW_SCROLLCHILDREN, &rect);
								rect.left		= hr.left - left;
								rect.right		= hr.left + 1;
								tree.Invalidate(rect);
							}
						}
						break;
					case HDN_ITEMCHANGED:
						if ((nmhd->pitem->mask & HDI_WIDTH) && (style & TCS_HEADERAUTOSIZE)) {
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

	return  DefWindowProc(*this, message, wParam, lParam);
}
