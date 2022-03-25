#include "TreeColumns.h"

using namespace win;

//-----------------------------------------------------------------------------
//	TreeColumnControl
//-----------------------------------------------------------------------------

void TreeColumnControl::UpdateScrollInfo(ScrollInfo	&siHoriz, ScrollInfo &siVert) {
	// adjust scrollbar
	Rect		client	= GetClientRect();
	int			count	= header.Count();
	int			maxx	= count ? header.GetItemRect(count - 1).right - 1 : 0;
	int			maxy	= tree.GetItemRect(tree.GetNextItem(0, TVGN_LASTVISIBLE)).bottom;
	int			x		= siHoriz.nPos;
	int			y		= siVert.nPos;

	siHoriz.Set(0, maxx, client.right);
	SetScroll(siHoriz, false);

	siVert.Set(0, maxy, client.bottom);
	SetScroll(siVert, true);

	// reposition controls
	Rect	rect	= GetClientRect().SetLeft(-x);//.SetRight(maxx - siHoriz.nPos);
	header.Move(header.GetLayout(rect));

	Rect	col		= header.GetItemRect(tree_col);
	tree.Move(rect.SetLeft(col.Left()).SetRight(col.Right()));
	tree.SetScroll(siVert, true);
	SetScrollPos(tree, SB_CTL, y, TRUE);
//	col.SetBottom(maxy);
//	tree.Move(col - Point(x, y - rect.top));
}

void TreeColumnControl::AdjustColumns() {
	if (int count = header.Count()) {
		if (style & TCS_HEADERAUTOSIZE) {
			ScrollInfo	siHoriz = GetScroll(SIF_RANGE | SIF_PAGE | SIF_POS, false);

			Rect	rect		= header.GetItemRect(count - 1);
			int		min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, count - 1).Param());
			int		width		= max(GetClientRect().Width() + siHoriz.nPos - rect.left, min_width);
			if (width != rect.Width())
				HeaderControl::Item().Width(width).Set(header, count - 1);
		}
		UpdateScrollInfo();
	}
}

Rect TreeColumnControl::GetItemRect(HTREEITEM hItem, int iSubItem, bool fromtree) const {
	Rect	rect = tree.GetItemRect(hItem);
	if (iSubItem < 0) {
		if (int count = header.Count()) {
			Rect	rect2	= header.GetItemRect(count - 1);
			rect.right		= rect2.right;
			if (!fromtree)
				rect += Point(-GetScroll(SIF_POS, false).Pos(), rect2.bottom);
		}
	} else {
		Rect	rect2	= header.GetItemRect(iSubItem);
		rect.left		= rect2.left;
		rect.right		= rect2.right;
		if (!fromtree)
			rect += Point(-GetScroll(SIF_POS, false).Pos(), rect2.bottom);
	}
	return rect;
}

void TreeColumnControl::AdjustRect(HTREEITEM hItem, int iSubItem, RECT *rect) const {
	// grid
	if (style & TCS_GRIDLINES) {
		rect->right		-= 1;
		rect->bottom	-= 1;
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

void TreeColumnControl::SetTreeColumn(int i) {
	tree_col = i;
	UpdateScrollInfo();
}
int save_dc;

LRESULT TreeColumnControl::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			Rect		rect	= GetClientRect();
			header.Create(*this, NULL, WS_CHILD, 0, id);
			header.SetFont(Font::DefaultGui());
			header.Move(header.GetLayout(rect), SWP_SHOWWINDOW);
			tree.Create(*this, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0, rect, id);
			break;
		}
		case WM_SIZE: {
			if (int count = header.Count()) {
				if (style & TCS_HEADERAUTOSIZE) {
					Rect	rect		= header.GetItemRect(count - 1);
					int		min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, count - 1).Param());
					int		width		= max(Point(lParam).x + GetScroll(SIF_POS, false).Pos() - rect.left, min_width);
					if (width != rect.Width())
						HeaderControl::Item().Width(width).Set(header, count - 1);
				}
				UpdateScrollInfo();
			}
			break;
		}

		case WM_HSCROLL: {
			ScrollInfo	siHorz	= GetScroll(SIF_RANGE | SIF_PAGE | SIF_POS, false);
			ScrollInfo	siVert	= GetScroll(SIF_RANGE | SIF_PAGE | SIF_POS, true);
			Scroll(siHorz.ProcessScroll(wParam, 5), 0);
			UpdateScrollInfo(siHorz, siVert);
			break;
		}

		case WM_VSCROLL: {
			ScrollInfo	siHorz	= GetScroll(SIF_RANGE | SIF_PAGE | SIF_POS, false);
			ScrollInfo	siVert	= GetScroll(SIF_RANGE | SIF_PAGE | SIF_POS, true);
			siVert.ProcessScroll(wParam, 5);
			//Scroll(0, siVert.ProcessScroll(wParam, 5));
			UpdateScrollInfo(siHorz, siVert);
			break;
		}

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
							return header.Count() ? CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT : CDRF_SKIPDEFAULT;
						}
						case CDDS_POSTPAINT:
							return Parent()(message, wParam, lParam);

						case CDDS_ITEMPREPAINT: {
							NMTCCCUSTOMDRAW		customDraw;

							// custom draw
							(NMTVCUSTOMDRAW&)customDraw		= *(NMTVCUSTOMDRAW*)pnmcd;
							customDraw.nmcd.hdr.hwndFrom	= hWnd;
							customDraw.nmcd.hdr.idFrom		= ID(this->id);
							customDraw.iSubItem				= tree_col;

							// alignment
							switch (header.GetItem(tree_col, HDI_FORMAT).Format() & HDF_JUSTIFYMASK) {
								case HDF_LEFT:		customDraw.uAlign	= DT_LEFT;		break;
								case HDF_RIGHT:		customDraw.uAlign	= DT_RIGHT;		break;
								case HDF_CENTER:	customDraw.uAlign	= DT_CENTER;	break;
							}

							int			ret			= Parent()(message, wParam, lParam);
							return CDRF_NOTIFYPOSTPAINT | ret;
						}
#if 1
						case CDDS_ITEMPOSTPAINT: {
							if (tree.style & TCS_GRIDLINES)
								DeviceContext(pnmcd->hdc).DrawEdge(header.GetItemRect(tree_col), BDR_SUNKENINNER, BF_RIGHT | BF_BOTTOM);

							return CDRF_DODEFAULT;
						}
#else
						case CDDS_ITEMPOSTPAINT: {
							if (save_dc)
								RestoreDC(pnmcd->hdc, save_dc);

							DeviceContext	dc(pnmcd->hdc);
							HTREEITEM	hItem			= (HTREEITEM)pnmcd->dwItemSpec;
							int			tree_style		= tree.style;
							int			num_cols		= header.Count();
							char		label[256]		= {0};
							ID			id				= this->id;

							NMTCCDISPINFO		dispInfo;
							dispInfo.hdr.hwndFrom		= hWnd;
							dispInfo.hdr.idFrom			= id;
							dispInfo.hdr.code			= TCN_GETDISPINFO;
							dispInfo.item.hItem			= hItem;
							dispInfo.item.pszText		= label;
							dispInfo.item.cchTextMax	= int(num_elements(label) - 1);
							dispInfo.item.lParam		= pnmcd->lItemlParam;

							// custom draw
							NMTCCCUSTOMDRAW		customDraw;
							(NMTVCUSTOMDRAW&)customDraw		= *(NMTVCUSTOMDRAW*)pnmcd;
							customDraw.nmcd.hdr.hwndFrom	= hWnd;
							customDraw.nmcd.hdr.idFrom		= id;

							// column rectangles
							Rect	rcItem		= dc.GetClipBox();
							Rect	rcLabel;
							// draw columns
							if (tree.GetItemRect(hItem, &rcLabel, true)) for (int i = 0; i < num_cols; i++) {
								// rectangle
								Rect	columnRect	= header.GetItemRect(i);
								if (columnRect.right <= rcItem.left || columnRect.left >= rcItem.right)
									continue;

								columnRect.top		= rcLabel.top;
								columnRect.bottom	= rcLabel.bottom;
								customDraw.rcItem	= columnRect;
								AdjustRect(hItem, i, &customDraw.rcItem);

								// alignment
								switch (header.GetItem(i, HDI_FORMAT).Format() & HDF_JUSTIFYMASK) {
									case HDF_LEFT:		customDraw.uAlign	= DT_LEFT;		break;
									case HDF_RIGHT:		customDraw.uAlign	= DT_RIGHT;		break;
									case HDF_CENTER:	customDraw.uAlign	= DT_CENTER;	break;
								}

								dispInfo.iSubItem	=
								customDraw.iSubItem	= i;

								// custom draw
								if (!SendMessage(WM_NOTIFY, id, (LPARAM)&customDraw)) {
									// display info
									if (!SendMessage(WM_NOTIFY, id, (LPARAM)&dispInfo)) {
										dispInfo.item.mask = TVIF_TEXT;
										((TreeControl::Item&)dispInfo.item).Get(tree);
									}

									// selection
									if (i == 0 || (tree_style & TVS_FULLROWSELECT)) {
										RECT rcSelection = customDraw.rcItem;
										if (!(tree_style & TVS_FULLROWSELECT))
											rcSelection.right	= min(rcSelection.left + dc.GetTextExtent(dispInfo.item.pszText).x + 2 * OFFSET_HORIZ, columnRect.right - 1);

										// draw
										if (rcSelection.right > rcSelection.left) {
											dc.Fill(rcSelection, Brush(customDraw.clrTextBk));

											if (customDraw.nmcd.uItemState & CDIS_FOCUS && GetFocus() == tree)
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
										dc.SetTextColour(customDraw.clrText);
										for (char *p = dispInfo.item.pszText; *p; p++) {
											if (*p < ' ')
												*p = ' ';
										}
										dc.DrawText(customDraw.rcItem, customDraw.uAlign | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, dispInfo.item.pszText);
									}
								}
								// grid
								if (style & TCS_GRIDLINES)
									dc.DrawEdge(columnRect, BDR_SUNKENINNER, BF_RIGHT | BF_BOTTOM);
							}
							return CDRF_DODEFAULT;
						}
#endif
					}
					return CDRF_DODEFAULT;

				} else if (pnmh->code == TVN_GETDISPINFO) {
					// display info
					NMTCCDISPINFO dispInfo;
					(NMTVDISPINFO&)dispInfo	= *(NMTVDISPINFO*)pnmh;
					dispInfo.hdr.hwndFrom	= hWnd;
					dispInfo.hdr.idFrom		= GetID();
					dispInfo.hdr.code		= TCN_GETDISPINFO;
					dispInfo.iSubItem		= tree_col;

					SendMessage(WM_NOTIFY, dispInfo.hdr.idFrom, (LPARAM)&dispInfo);

				} else if (pnmh->code == NM_CLICK) {
					if (Parent()(message, wParam, lParam) == 0) {
						uint32	flags;
						if (HTREEITEM hItem = tree.HitTest(tree.ToClient(GetMousePos()), &flags)) {
							if (flags & TVHT_ONITEM)
								tree.SetSelectedItem(hItem);
						}
					}
					return 0;
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
							Rect	rect	= tree.GetClientRect();
							Rect	hr		= header.GetItemRect(nmhd->iItem);
							rect.left		= hr.right;
							rect.right		= 4096;
							ScrollWindowEx(tree, nmhd->pitem->cxy - hr.Width(), 0, &rect, 0, 0, 0, SW_ERASE | SW_INVALIDATE | SW_SCROLLCHILDREN);
							rect.left		= max(hr.left, hr.right - 20);
							rect.right		= hr.right;
							tree.Invalidate(rect);
						}
						break;

					case HDN_ITEMCHANGED:
						if ((nmhd->pitem->mask & HDI_WIDTH) && (style & TCS_HEADERAUTOSIZE)) {
							int	last_index = header.Count() - 1;
							if (nmhd->iItem != last_index) {
								int	min_width	= int(HeaderControl::Item(TCIF_PARAM).Get(header, last_index).Param());
								int width		= max(GetClientRect().Width() + GetScroll(SIF_POS, false).Pos() - header.GetItemRect(last_index).left, min_width);
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
