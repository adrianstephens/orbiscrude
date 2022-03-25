#include "graphics.h"
#include "windows/control_helpers.h"
#include "windows/dib.h"
#include "iso/iso.h"
#include "shader.h"
#include "gpu.h"
#include "extra/indexer.h"
#include "systems/mesh/shapes.h"
#include "filetypes/3d/model_utils.h"
#include "viewmesh.rc.h"

#define IDR_MENU_MESH			"IDR_MENU_MESH"
#define IDR_TOOLBAR_MESH		"IDR_TOOLBAR_MESH"
#define IDR_ACCELERATOR_MESH	"IDR_ACCELERATOR_MESH"

using namespace iso;
using namespace win;
using namespace app;

#if 1
Cursor	app::CURSOR_LINKBATCH = TextCursor(Cursor::LoadSystem(IDC_HAND), win::Font::DefaultGui(), "batch"),
		app::CURSOR_LINKDEBUG = TextCursor(Cursor::LoadSystem(IDC_HAND), win::Font::DefaultGui(), "debug"),
		app::CURSOR_ADDSPLIT = CompositeCursor(Cursor::LoadSystem(IDC_SIZENS), Cursor::Load("IDR_OVERLAY_ADD", 0));
#else
Cursor	app::CURSOR_LINKBATCH = Cursor::LoadSystem(IDC_HAND),
		app::CURSOR_LINKDEBUG = Cursor::LoadSystem(IDC_HAND),
		app::CURSOR_ADDSPLIT = Cursor::LoadSystem(IDC_SIZENS);
#endif

Control app::ErrorControl(const WindowPos &wpos, const char *error) {
	StaticControl	err(wpos, error, WS_CHILD | WS_VISIBLE | SS_CENTER, 0);
	err.Class().style |= CS_HREDRAW;
	err.SetFont(win::Font("Segoe UI", 32));
	return err;
}

template<> struct C_types::type_getter<float16> {
	static const C_type *f(C_types &types)	{ return C_type_float::get<float16>(); }
};
template<typename I, I S> struct C_types::type_getter<scaled<I,S> > {
	static const C_type *f(C_types &types)	{ return C_type_int::get<I>(); }
};

//-----------------------------------------------------------------------------
//	RegisterTree
//-----------------------------------------------------------------------------

HTREEITEM RegisterTree::Add(HTREEITEM h, const char *text, int image, const arbitrary &param) {
	return TreeControl::Item(text).Image(image).Param(param).Insert(tree, h);
}

HTREEITEM RegisterTree::AddText(HTREEITEM h, const char *text, uint32 addr) {
	return TreeControl::Item(text).Param(addr).Insert(tree, h);
}

HTREEITEM RegisterTree::AddHex(HTREEITEM h, uint32 addr, uint32 val) {
	return AddText(h,format_string("0x%08x", val), addr);
}

HTREEITEM RegisterTree::AddHex(HTREEITEM h, uint32 addr, const uint32 *vals, uint32 n) {
	while (n--) {
		AddHex(h, addr, *vals++);
		addr += 4;
	}
	return h;
}

HTREEITEM RegisterTree::AddFields(HTREEITEM h, const field *pf, uint32 val) {
	if (pf == float_field) {
		AddText(h, format_string("%f", (float&)val));
	} else if (pf) for (; pf->name; ++pf) {
		if (pf->num == 0) {
			AddFields(h, (const field*)pf->values, val >> pf->start);
		} else {
			AddText(h, PutField(buffer_accum<256>(), format, pf, pf->get_value(val)));
		}
	}
	return h;
}

void RegisterTree::AddField(HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
	if (pf->num == 0) {
		switch (pf->offset) {
			case field::MODE_CALL:
				if (pf->name)
					h = AddText(h, Identifier(prefix, pf->name, format).get(), addr);
				_AddFields(h, pf->get_call(p, offset), p, offset + pf->start, addr);
				break;

			case field::MODE_POINTER: {
				buffer_accum<64>	ba(Identifier(prefix, pf->name, format));
				if (uint32le *p2 = ((uint32le**)p)[(offset + pf->start) / 64]) {
					if (pf->shift)
						ba << '[' << pf->get_companion_value(p, offset) << ']';
					else
						ba << "->";
					h = AddText(h, ba, addr);
					if (format & IDFMT_FOLLOWPTR) {
						if (pf->shift)
							AddArray(h, (const field*)pf->values, p2, 0, pf->get_companion_value(p, offset));
						else
							_AddFields(h, (const field*)pf->values, p2, 0, 0);
					}
				} else {
					ba << " = 0";
					AddText(h, ba, addr);
				}
				break;
			}

			case field::MODE_RELPTR: {
				buffer_accum<64>	ba(Identifier(prefix, pf->name, format));
				uint32le	*p0 = (uint32le*)((char*)p + (offset + pf->start) / 8);
				if (*p0) {
					if (pf->shift)
						ba << '[' << pf->get_companion_value(p, offset) << ']';
					else
						ba << "->";
					h = AddText(h, ba, addr);
					if (format & IDFMT_FOLLOWPTR) {
						if (pf->shift)
							AddArray(h, (const field*)pf->values, (uint32le*)((char*)p0 + *p0), 0, pf->get_companion_value(p, offset));
						else
							_AddFields(h, (const field*)pf->values, (uint32le*)((char*)p0 + *p0), 0, 0);
					}
				} else {
					ba << " = 0";
					AddText(h, ba, addr);
				}
				break;
			}
		}

	} else if (pf->offset == field::MODE_CUSTOM) {
		if (cb)
			cb(*this, h, pf, p, offset, addr);
		else
			AddText(h, PutField(buffer_accum<256>("<custom>"), format, pf, p, offset, prefix), addr + offset);

	} else {
		AddText(h, PutField(buffer_accum<256>(), format, pf, p, offset, prefix), addr + offset);
	}
}

void RegisterTree::_AddFields(HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
	if (pf == float_field) {
		AddText(h, buffer_accum<256>() << (float&)p[offset / 32], addr);

	} else if (pf) {
		while (pf->num || pf->values) {
			AddField(h, pf, p, offset, addr);
			pf++;
		}
	}
}

HTREEITEM RegisterTree::AddFields(HTREEITEM h, const field *pf, const uint32le *p, uint32 addr) {
	if (p) {
		if (!pf)
			AddText(h, buffer_accum<256>() << "value = 0x" << hex(*p), addr);
		else
			_AddFields(h, pf, p, 0, addr);
	}
	return h;
}

void RegisterTree::AddArray(HTREEITEM h, const field *pf, const uint32le *p, uint32 stride, uint32 n, uint32 addr) {
	if (stride == 0) {
		uint32	s = Terminator(pf)->start;
		ISO_ASSERT(s);
		if (s % 32) {
			AddHex(h, addr, p, n * s / 4);
			return;
		}
		stride = s / 32;
	}

	for (uint32 i = 0; i < n; i++)
		AddFields(AddText(h, format_string("[%i]", i), addr + stride * i), pf, p + stride * i, addr + stride * i);
}


HTREEITEM app::FindOffset(TreeControl tree, HTREEITEM h, uint32 offset) {
	for (HTREEITEM	p = h; p; ) {
		h = p;
		p = 0;
		for (HTREEITEM c = tree.GetChildItem(h); c; p = c, c = tree.GetNextItem(c)) {
			TreeControl::Item	i	= tree.GetItem(c, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM);
			uint32	off	= i.Param();
			if (off == offset)
				return c;
			if (off > offset)
				break;
		}
	}
	return h;
}

void app::HighLightTree(TreeControl tree, HTREEITEM	prev, uint32 val) {
	while (prev) {
		TreeControl::Item(prev).SetState(INDEXTOSTATEIMAGEMASK(val)).Set(tree);
		prev = tree.GetParentItem(prev);
	}
}

void app::HighLightTree(TreeControl tree, uint32 addr, uint32 val) {
	HTREEITEM	prev	= 0;
	for (HTREEITEM h = tree.GetRootItem(); h; h = tree.GetNextItem(h)) {
		uint32	p = tree.GetItemParam(h);
		if (p > addr)
			h = tree.GetChildItem(prev);
		if (h)
			prev = h;
	}
	while (prev) {
		TreeControl::Item(prev).SetState(INDEXTOSTATEIMAGEMASK(val)).Set(tree);
		prev = tree.GetParentItem(prev);
	}
}

//-----------------------------------------------------------------------------
//	BatchList
//-----------------------------------------------------------------------------

string_accum &app::WriteBatchList(string_accum &sa, BatchList &bl) {
	for (auto i = bl.begin(), e = bl.end(); i != e;) {
		int		n = 16;
		sa.getp(n);
		sa.move(-n);
		if (n < 16)
			return sa << "...";

		uint32	start = *i, end = start;
		do {
			end = *i++;
		} while (i != e && *i == end + 1);

		sa << start;
		if (end > start)
			sa << "-" << end;

		if (i != e)
			sa << ',';
	}
	return sa;
}

int app::SelectBatch(HWND hWnd, const Point &mouse, BatchList &b, bool always_list) {
	if (b.size() == 0)
		return -1;

	uint32	batch	= b[0];

	if (always_list || b.size() > 1) {
		Menu	menu	= Menu::Popup();
		int		dy		= win::GetMenuSize().y;

		dy = GetSystemMetrics(SM_CYMENUSIZE);

		int		maxy	= Control::Desktop().GetClientRect().Height();
		int		y		= dy;

		menu.Append("Go to_batch:", 0, MF_DISABLED);
		for (uint32 *i = b.begin(), *e = b.end(); i != e;) {
			int	type = 0;
			if (y > maxy) {
				type	= MFT_MENUBARBREAK;
				y		= 0;
			}
			y += dy;

			uint32	start = *i, end = start;
			do {
				end = *i++;
			} while (i != e && *i == end + 1);

			if (end > start) {
				Menu	submenu	= Menu::Create();
				Menu::Item().Text(format_string("%i-%i", start, end)).Type(type).SubMenu(submenu).AppendTo(menu);
				int		y1		= dy * 2;
				for (int b = start; b <= end; b++) {
					Menu::Item().Text(to_string(b)).ID(b + 1).Type(y1 > maxy ? MFT_MENUBARBREAK : 0).AppendTo(submenu);
					if (y1 > maxy)
						y1		= 0;
					y1 += dy;
				}
			} else {
				Menu::Item().Text(to_string(start)).ID(start + 1).Type(type).AppendTo(menu);
			}

		}
		batch = menu.Track(hWnd, mouse, TPM_NONOTIFY | TPM_RETURNCMD);
		if (batch == 0)
			return -1;
		--batch;
	}
	return batch;
}

//-----------------------------------------------------------------------------
//	ListViews
//-----------------------------------------------------------------------------

int RegisterList::MakeColumns(const field *pf, int nc, const char *prefix) {
	if (pf) {
		while (pf->values || pf->num) {
			if (pf->num == 0) {
				switch (pf->offset) {
					case field::MODE_NONE:
						nc = MakeColumns(pf->get_call(), nc, pf->name);
						break;

					case field::MODE_POINTER:
					case field::MODE_RELPTR:
						if (format & IDFMT_FOLLOWPTR)
							nc = MakeColumns(pf->get_call(), nc, pf->name);
						break;
				}
			} else {
				buffer_accum<256>	b(Identifier(prefix, pf->name, format));
				ListViewControl::Column(b).Width(100).Insert(lv, nc++);
			}
			++pf;
		}
	}
	return nc;
}

void RegisterList::Autosize() const {
	for (int i = 0, n = lv.NumColumns(); i < n; i++)
		lv.SetColumnWidth(i, LVSCW_AUTOSIZE);
}

void RegisterList::FillRow(ListViewControl::Item &item, const field *pf, const uint32 *p, uint32 offset) {
	if (pf) {
		while (pf->values || pf->num) {
			if (pf->num == 0) {
				switch (pf->offset) {
					case field::MODE_NONE:
						FillRow(item, pf->get_call(p, offset), p, offset + pf->start);
						break;

					case field::MODE_POINTER: {
						const uint32	*p2 = ((uint32le**)p)[(offset + pf->start) / 64];
						if (p2 && (format & IDFMT_FOLLOWPTR)) {
							if (pf->shift)
								FillRowArray(item, (const field*)pf->values, p2, 0, pf->get_companion_value(p, offset));
							else
								FillRow(item, (const field*)pf->values, p2, 0);
						}
						break;
					}

					case field::MODE_RELPTR: {
						const uint32le	*p0 = (uint32le*)((char*)p + (offset + pf->start) / 8);
						if (*p0 && (format & IDFMT_FOLLOWPTR)) {
							if (pf->shift)
								FillRowArray(item, (const field*)pf->values, (uint32le*)((char*)p0 + *p0), 0, pf->get_companion_value(p, offset));
							else
								FillRow(item, (const field*)pf->values, (uint32le*)((char*)p0 + *p0), 0);
						}
						break;
					}
				}

			} else {
				buffer_accum<256>	b;

				if (pf->offset == field::MODE_CUSTOM && cb) {
					cb(b, pf->get_raw_value(p, offset));
				} else {
					if (format & IDFMT_FIELDNAME)
						PutFieldName(b, format, pf);

					PutConst(b, format, pf, p, offset);
				}

				item.Text(b);
				if (item.iSubItem == 0)
					item.Param(p).Insert(lv);
				else
					item.Set(lv);

				item.NextColumn();
			}
			++pf;
		}
	}
}

void RegisterList::FillRowArray(ListViewControl::Item &item, const field *pf, const uint32 *p, uint32 stride, uint32 n) {
	if (stride == 0) {
		uint32	s = Terminator(pf)->start;
		ISO_ASSERT(s);
		stride = s / 32;
	}

	for (uint32 i = 0; i < n; i++)
		FillRow(item, pf, p + stride * i);
}

int app::NumElements(const C_type *type) {
	switch (type->type) {
		case C_type::ARRAY: {
			const C_type_array	*a = (const C_type_array*)type;
			return a->count * NumElements(a->subtype);
		}
		case C_type::STRUCT: {
			const C_type_struct	*s = (const C_type_struct*)type;
			int		n = 0;
			for (auto *i = s->elements.begin(), *e = s->elements.end(); i != e; ++i)
				n += NumElements(i->type);
			return n;
		}
		default:
			return 1;
	}
}
const C_type *app::GetNth(void *&data, const C_type *type, int i, int &shift) {
	shift = 0;
	while (type) {
		switch (type->type) {
			case C_type::ARRAY: {
				type	= type->subtype();
				int		n = NumElements(type);
				int		x = i / n;
				data	= (uint8*)data + x * type->size();
				i		%= n;
				break;
			}
			case C_type::STRUCT: {
				const C_type_struct	*s = (const C_type_struct*)type;
				for (auto *e = s->elements.begin(), *e1 = s->elements.end(); e != e1; ++e) {
					int	n = NumElements(e->type);
					if (n > i) {
						data	= (uint8*)data + e->offset;
						type	= e->type;
						shift	= e->shift;
						break;
					}
					i	-= n;
				}
				break;
			}
			default:
				return type;
		}
	}
	return 0;
}
int app::MakeHeaders(win::ListViewControl lv, int nc, const C_type *type, string_accum &prefix) {
	switch (type->type) {
		case C_type::ARRAY: {
			const C_type_array	*a = (const C_type_array*)type;
			char	*startp = prefix.getp();
			for (int i = 0, n = a->count; i < n; i++) {
				nc = MakeHeaders(lv, nc, a->subtype, prefix << '[' << i << ']');
				prefix.move(startp - prefix.getp());
			}
			break;
		}
		case C_type::STRUCT: {
			const C_type_struct	*s = (const C_type_struct*)type;
			char	*startp = prefix.getp();
			for (auto *i = s->elements.begin(), *e = s->elements.end(); i != e; ++i) {
				nc = MakeHeaders(lv, nc, i->type, prefix << '.' << i->id);
				prefix.move(startp - prefix.getp());
			}
			break;
		}
		default:
			win::ListViewControl::Column(prefix).Width(75).Insert(lv, nc++);
			break;
	}
	return nc;
}

//-----------------------------------------------------------------------------
//	ColumnColours
//-----------------------------------------------------------------------------

win::Colour app::FadeColour(win::Colour c) {
	return win::Colour((c.r + 255) / 2, (c.g + 255) / 2, (c.b + 255) / 2);
}

win::Colour app::MakeColour(int c) {
	return win::Colour(c & 1 ? 255 : 240, c & 2 ? 255 : 240, c & 4 ? 255 : 240);
}

LRESULT	ColumnColours::CustomDraw(NMCUSTOMDRAW	*nmcd, Control parent) {
	DeviceContext	dc(nmcd->hdc);

	switch (nmcd->dwDrawStage) {
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT: {
			NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmcd;
			int	result = parent(WM_NOTIFY, 0, (LPARAM)nmcd);
			rowcol = result ? nmlvcd->clrTextBk : RGB(255,255,255);
			return result | CDRF_NOTIFYSUBITEMDRAW;
		}
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
			NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmcd;
			DWORD_PTR		i		= nmcd->dwItemSpec;
			int				sub		= nmlvcd->iSubItem;

			if (vw.GetItemState(i, LVIS_SELECTED)) {
				DeviceContext	dc(nmcd->hdc);
				Rect			rect = vw.GetSubItemRect(i, sub);
				dc.Fill(rect, Brush(win::Colour::red));
				dc.DrawText(rect.Grow(-6, -2, 0, 0), DT_LEFT | DT_NOCLIP, str<64>(vw.GetItemText(i, sub)));
				return CDRF_SKIPDEFAULT;

			} else {
				nmlvcd->clrTextBk = Colour(colours[sub % colours.size()]) * Colour(rowcol);
				return CDRF_DODEFAULT;
			}
		}
		default:
			return parent(WM_NOTIFY, 0, (LPARAM)nmcd);
	}
}


LRESULT VertexWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			vw.Create(GetChildWindowPos(), "verts", WS_CHILD | WS_CLIPSIBLINGS);
			return 0;

		case WM_SIZE:
			vw.Resize(Point(lParam));
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CUSTOMDRAW:
					return CustomDraw((NMCUSTOMDRAW*)nmh, Parent());

				case MeshNotification::SET:
					return Notification((MeshNotification*)nmh);
			}
			return Parent()(message, wParam, lParam);
		}

		case WM_NCDESTROY:
			delete this;
			return 0;

		default:
			if (message >= LVM_FIRST && message < LVM_FIRST + 0x100)
				return vw.SendMessage(message, wParam, lParam);
			break;
	}
	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	ColourTree
//-----------------------------------------------------------------------------

LRESULT ColourTree::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			ToolTipControl	oldtt = GetToolTipControl();
			TTTOOLINFOA		ti;
			clear(ti);
			ti.cbSize		= sizeof(TOOLINFO);
			ti.hwnd			= *this;
			ti.uId			= (UINT_PTR)ti.hwnd;
			oldtt(TTM_GETTOOLINFO, 0, (LPARAM)&ti);

			ToolTipControl	tt(*this, NULL, WS_POPUP |TTS_ALWAYSTIP, 0);
			tt(TTM_ADDTOOLA, 0, (LPARAM)&ti);
			//tt.Attach(*this);
			SetToolTipControl(tt);

			TTTOOLINFOA		ti2;
			clear(ti2);
			ti2.cbSize		= sizeof(TOOLINFO);
			ti2.hwnd			= *this;
			ti2.uId			= (UINT_PTR)ti.hwnd;
			tt(TTM_GETTOOLINFO, 0, (LPARAM)&ti2);

			font = GetFont().GetParams().Underline(true);
			return 0;
		}
		case WM_MOUSEMOVE: {
			uint32		flags;
			HTREEITEM	hit = HitTest(Point(lParam), &flags);
			if (!(flags & TVHT_ONITEM) || (hit != hot && TreeControl::Item(hit).Image(0).Get(*this).Image() == 0))
				hit = 0;

			if (hit != hot) {
				if (hot)
					Invalidate(GetItemRect(hot));
				if (hot = hit)
					Invalidate(GetItemRect(hot));
			}

			if (hot)
				SetCursor(Item(hit).Image(0).Get(*this).Image(), wParam & MK_SHIFT ? 1 : wParam & MK_CONTROL ? 2 : 0);
			break;
		}
		case WM_COMMAND:
		case WM_CHAR:
			Parent().SendMessage(message, wParam, lParam);
			break;
	}
	return Super(message, wParam, lParam);
}

LRESULT	ColourTree::CustomDraw(NMCUSTOMDRAW	*nmcd, Control parent) const {
	switch (nmcd->dwDrawStage) {
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT: {
			NMTVCUSTOMDRAW *nmtvcd = (NMTVCUSTOMDRAW*)nmcd;
			if ((HTREEITEM)nmcd->dwItemSpec == hot) {
				SelectObject(nmcd->hdc, font);
				nmtvcd->clrText = colours[TreeControl::Item(hot).Image(0).Get(*this).Image()];
				return CDRF_NEWFONT;
			}
			break;
		}
	}
	return CDRF_DODEFAULT;
}

void ColourTree::SetCursor(int type, int mod) const {
	int	x	= cursor_indices[type][mod];
	if (x > 0 && cursors[x - 1])
		::SetCursor(cursors[x - 1]);
}

//-----------------------------------------------------------------------------
//	transpose_bits
//-----------------------------------------------------------------------------

BatchSelectWindow::BatchSelectWindow(const WindowPos &wpos, int _nbatch) : nbatch(_nbatch), offset(0), zoom(wpos.Width() / float(nbatch)), last_batch(0) {
	Create(wpos, "Performance", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, WS_EX_CLIENTEDGE);
}

void BatchSelectWindow::GetTipText(string_accum &acc, int i) const {
	acc << "batch=" << i;
}

void BatchSelectWindow::SetBatch(int i) {
	if (i != last_batch) {
		int	x0	= BatchToClient(last_batch);
		int	x1	= BatchToClient(i);
		if (x1 < x0)
			swap(x0, x1);

		Invalidate(DragStrip().SetLeft(x0 - 10).SetRight(x1 + 10), false);
		last_batch = i;
		Update();
	}
}

LRESULT BatchSelectWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			tooltip.Create(*this, NULL, WS_POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Attach(*this);
			tip_on	= false;
			owner	= Parent();
			break;

		case WM_SIZE:
			if (d2d.Resize(Point(lParam)))
				d2d.DeInit();
			Invalidate();
			break;

		case WM_PAINT:
			Paint(hWnd);
			break;

		case WM_ISO_BATCH:
			SetBatch(wParam + 1);
			break;

		case WM_RBUTTONDOWN:
			SetCapture(*this);
			SetFocus();
			break;

		case WM_LBUTTONDOWN: {
			SetCapture(*this);
			SetFocus();
			Point	mouse(lParam);
			int	batch = ClientToBatch(mouse.x);
			if (batch && batch < nbatch && DragStrip().Contains(mouse)) {
				owner(WM_ISO_BATCH, batch - 1);
				SetBatch(batch - 1);
			}
			break;
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			ReleaseCapture();
			return 0;

		case WM_MOUSEMOVE: {
			Point	mouse	= Point(lParam);
			if (wParam & MK_LBUTTON) {
				float	prev_offset	= offset;
				offset -= (mouse.x - prevmouse.x) / zoom;
				Invalidate();
			} else if (wParam & MK_RBUTTON) {
				int	i = min(ClientToBatch(mouse.x), nbatch - 1);
				if (i != last_batch && i)
					owner(WM_ISO_JOG, i - 1);
				SetBatch(i);
			}
			prevmouse	= mouse;
			if (!tip_on) {
				TrackMouse(TME_LEAVE);
				tooltip.Activate(*this, tip_on = true);
			}
			tooltip.Track(ToScreen(mouse) + Point(15, 15));
			if (DragStrip().Contains(mouse))
				CURSOR_LINKBATCH.Set();
			else
				Cursor::LoadSystem(IDC_SIZEWE).Set();
			break;
		}
  		case WM_MOUSEWHEEL:
			if (GetRect().Contains(Point(lParam))) {
				Point	pt		= ToClient(Point(lParam));
				float	t		= offset + int(pt.x / zoom);
				zoom	*= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				offset	= t - pt.x / zoom;
				Invalidate(0, false);
			}
			break;

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, tip_on = false);
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA: {
					NMTTDISPINFOA	*nmtdi	= (NMTTDISPINFOA*)nmh;
					GetTipText(fixed_accum(nmtdi->szText), ClientToBatch(ToClient(GetMousePos()).x));
					break;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;
	}
	return Super(message, wParam, lParam);
}

void BatchSelectWindow::Paint(const DeviceContextPaint &dc) {
	Rect	client	= GetClientRect();
	Rect	dirty	= d2d.device ? dc.GetDirtyRect() : client;

	d2d.Init(*this, client.Size());
	if (!d2d.Occluded()) {
		d2d::Write	write;
		float		w		= client.Width(), h = client.Height();
		float2x3	trans	= translate(client.BottomLeft()) * scale(zoom, -h * .9f) * translate(float2(-offset, 0));
		float2x3	itrans	= inverse(trans);
		d2d::point	a		= itrans * d2d::point(dirty.BottomLeft());
		d2d::point	b		= itrans * d2d::point(dirty.TopRight());
		float		lscale	= 2 - log10(zoom);
		float		tscale	= pow(10.f, int(floor(lscale)));
		int			batch0	= max(int(a.x) - 1, 0);
		int			batch1	= min(int(b.x), nbatch - 1);

		d2d.BeginDraw();
		d2d.SetTransform(identity);
		d2d.device->PushAxisAlignedClip(d2d::rect(dirty), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

		d2d.Clear(colour(zero));

		d2d.SetTransform(trans);
		{// batch bands
			d2d::SolidBrush	grey(d2d, colour(0.2f,0.2f,0.2f));
			uint64			toff	= 0;
			for (int i = batch0 & ~1; i < batch1; i += 2) {
				d2d.Fill(rectangle(i, a.y, i + 1, b.y), grey);
			}
		}

		d2d.SetTransform(identity);
		{// batch labels
			com_ptr<IDWriteTextFormat>		font;
			write.CreateFont(&font, L"arial", 20);
			font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
			d2d::SolidBrush	textbrush(d2d, colour(one));

			float	t0	= -100;
			float	s	= trans.xx;
			float	o	= trans.z.x;
			for (int i = batch0; i < batch1; i++) {
				float	t1	= i * s + o;
				if (t1 > t0) {
					d2d.DrawText(d2d::rect(t1 - 100, h - 50, t1 + 100, h), str16(buffer_accum<256>() << i), font, textbrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
					t0 = t1 + 60;
				}
			}
		}

		//scrub bar
		d2d.Fill(d2d::rect(0, 0, w, 12), d2d::SolidBrush(d2d, colour(0.5f)));

		//marker
		if (!marker_geom) {
			com_ptr<ID2D1GeometrySink>	sink;
			d2d.CreatePath(&marker_geom, &sink);
			sink->BeginFigure(d2d::point(-10, 0), D2D1_FIGURE_BEGIN_FILLED);
			sink->AddLine(d2d::point(+10, 0));
			sink->AddLine(d2d::point(0, 10));
			sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			sink->Close();
		}
		d2d.SetTransform(translate(BatchToClient(last_batch), 0));
		d2d.Fill(marker_geom, d2d::SolidBrush(d2d, colour(1,0,0)));

		d2d.device->PopAxisAlignedClip();

		if (d2d.EndDraw()) {
			d2d.DeInit();
			marker_geom.clear();
		}
	}
}
//-----------------------------------------------------------------------------
//	transpose_bits
//-----------------------------------------------------------------------------


struct SpacePartition {
	cuboid		ext;
	uint64		*vecs;
	uint32		*indices;
	size_t		num;

	static uint64		mask1(const iorf &i)		{ return part_bits<1, 2, 21>(i.m >> 2); }
	static uint64		clamp_mask1(const iorf &i)	{ return part_bits<1, 2, 21>(i.e < 0x80 ? 0 : i.e == 0x80 ? i.m >> 2 : 0x1fffff); }
	static iorf			unmask1(uint64 m)			{return iorf(0x40000000 | unpart_bits<1, 2, 21>(m) << 2); }

	static uint64		clamp_mask(param(position3) p);
	static uint64		mask(param(position3) p);
	static position3	unmask(uint64 m);

	float3x4			matrix()	const	{ return translate(3) * scale(0.999f) * ext.inv_matrix(); }
	int					closest(param(position3) p);
	bool				closest(param(position3) p0, param(position3) p1);

	void				init(const stride_iterator<const float3p> &begin, const stride_iterator<const float3p> &end);
	void				init(const stride_iterator<const float4p> &begin, const stride_iterator<const float4p> &end);
	void				init(const stride_iterator<const float3p> &begin, size_t n)	{ init(begin, begin + n); }
	void				init(const stride_iterator<const float4p> &begin, size_t n)	{ init(begin, begin + n); }

	SpacePartition() : ext(empty), vecs(0), indices(0), num(0) {}
	~SpacePartition() { delete[] vecs; delete[] indices; }
	SpacePartition(const stride_iterator<const float3p> &begin, const stride_iterator<const float3p> &end) { init(begin, end); }
	SpacePartition(const stride_iterator<const float4p> &begin, const stride_iterator<const float4p> &end) { init(begin, end); }
};

//-----------------------------------------------------------------------------
//	transpose_bits
//-----------------------------------------------------------------------------

//transpose MxN matrix of B bits
/*
template<typename T, int M, int N, int B, int I> struct s_transpose_bits {
	enum {
		S = B << (I - 1),
		M1	= (M >> (I - 1)) + N
	};
	static inline T f(T x) {
		T	mm = s_bitblocks<T, I * 4, 1>::value & ((T(1) << M) - 1);
		T	mn = s_bitblocks<T, M, 1>::value * mm;

		x = ((x & m) << M1) | ((x >> M) & m);


		x = (x & ~(m | (m << M))) | ((x & m) << M) | ((x >> M) & m);
		x = s_transpose_bits<T, M, N, B, I - 1>::f(x);
		return x;
	}
};

template<typename T, int M, int N, int B> T transpose_bits(T x) {
	return s_transpose_bits<T, M, N, B, 0>::f(x);
}

struct s_test_transpose {
	s_test_transpose() {
		uint64	t = 0x123456789ABCDEF0ull;
		t = transpose_bits<uint64,21,3,1>(t);
	}
} test_transpose;
*/
//-----------------------------------------------------------------------------
//	MeshWindow
//-----------------------------------------------------------------------------

/*
template<typename T, int N, int I> struct s_interleave_n<T, N, I> {
	static inline T f(T x) {
		T	m = s_bitblocks<T, (N + 1) << (I + 1), 1 << I>::value;
		return s_interleave_n<T, N, I - 1, true>::f((x & m) ^ ((x & ~m) << (N << I)));
	}
	static inline T g(T x) {
		x = s_interleave_n<T, N, I - 1>::g(x);
		T	m = s_bitblocks<T, (N + 1) << (I + 1), 1 << I>::value;
		return (x & m) ^ ((x & ~m) >> (N << I));
	}
};
template<typename T, int N> struct s_interleave_n<T, N, 0> {
	static inline T f(T x) {
		T	m = s_bitblocks<T, (N + 1) * 2, 1>::value;
		return (x & m) ^ ((x & ~m) << N);
	}
	static inline T g(T x) {
		T	m = s_bitblocks<T, (N + 1) * 2, 1>::value;
		return (x & m) ^ ((x & ~m) >> N);
	}
};
template<typename T, int N> inline T interleave(T x)	{ return s_interleave_n<UNSIGNED(T), sizeof(T) * 2>::f(x); }
template<typename T, int N> inline T uninterleave(T x)	{ return s_interleave_n<UNSIGNED(T), sizeof(T) * 2>::g(x); }

void SpacePartition::insert(param(position3) p) {
	uint64	x	= mask(p);
	node	*n	= &root;

	for (int b = 21; b--;) {
		uint8	i = uint8(x >> (b * 3)) & 7;
		uint8	c = n->count(i);
		if (c == 0) {
			if (node *n2 = n->child[i].n) {
				n = n2;
				continue;
			}
			n->child[i].v = new uint64(x);
			n->inc(i);
			return;

		} else if (c == 15) {
			uint64	*v0	= n->child[i].v, *v1 = v0 + 15, *v = v0;
			n = n->makenode(i);

			for (int j = 0; j < 8; j++) {
				while (v < v1 && (uint8(*v >> (b * 3)) & 7) <= j)
					++v;
				if (int c = v - v0) {
					n->flags	|= c << (j * 4);
					n->child[j].v = v;
					v0 = v;
				}
			}
		} else {
			uint64	*v	= n->child[i].v, *v1 = v + c;
			while (v < v1 && x > *v)
				v++;
			while (v1 > v) {
				v1[0] = v1[-1];
				--v1;
			}
			*v	= x;
			n->inc(i);
			return;
		}
	}
}
*/
uint64 SpacePartition::clamp_mask(param(position3) p) {
	iorf	*i = (iorf*)&p;
	return clamp_mask1(i[0]) | (clamp_mask1(i[1]) << 1) | (clamp_mask1(i[2]) << 2);
}

uint64 SpacePartition::mask(param(position3) p) {
	iorf	*i = (iorf*)&p;
	//ISO_ASSERT(i[0].e == 0x80 && i[1].e == 0x80 && i[2].e == 0x80);
	return (mask1(i[0]) << 0) | (mask1(i[1]) << 1) | (mask1(i[2]) << 2);
}

position3 SpacePartition::unmask(uint64 m) {
	return position3(unmask1(m >> 0).f, unmask1(m >> 1).f, unmask1(m >> 2).f);
}

/*
void SpacePartition::init(const stride_iterator<const float3p> &begin, const stride_iterator<const float3p> &end) {
	num		= distance(begin, end);
	vecs	= new uint64[num];
	indices	= new uint32[num];

	ext = empty;
	for (stride_iterator<const float3p> i = begin; i != end; ++i)
		ext |= position3(*i);
	ext.p1 += (ext.extent() == zero).select(one, zero);

	float3x4	mat	= matrix();
	uint64		*p	= vecs;
	for (stride_iterator<const float3p> i = begin; i != end; ++i)
		*p++ = mask(mat * position3(*i));

	for (int i = 0; i < num; ++i)
		indices[i] = i;

	buddy_iterator<uint64*, uint32*>	buddy(vecs, indices);
	sort(buddy, buddy + num);
}

void SpacePartition::init(const stride_iterator<const float4p> &begin, const stride_iterator<const float4p> &end) {
	num		= distance(begin, end);
	vecs	= new uint64[num];
	indices	= new uint32[num];

	ext = empty;
	for (stride_iterator<const float4p> i = begin; i != end; ++i)
		ext |= project(float4(*i));
	ext.p1 += (ext.extent() == zero).select(one, zero);

	float3x4	mat	= matrix();
	uint64		*p	= vecs;
	for (stride_iterator<const float4p> i = begin; i != end; ++i)
		*p++ = mask(mat * project(float4(*i)));

	for (int i = 0; i < num; ++i)
		indices[i] = i;

	buddy_iterator<uint64*, uint32*>	buddy(vecs, indices);
	sort(buddy, buddy + num);
}

int SpacePartition::closest(param(position3) p) {
	float3x4	mat	= matrix();
	uint64		m	= clamp_mask(mat * p);
	uint64		*a	= lower_bound(vecs, vecs + num, m);
	if (a == vecs + num)
		return -1;

	if (a == vecs)
		return indices[0];

	uint64	c	= 0x1249249249249249ull;

	uint64	d	= m ^ *a;
//	uint64	d	= a[0] ^ a[-1];
	int		s	= highest_set_index(d);
	switch (s % 3) {
		case 0: {
			m &= ~((c * 6) >> (63 - s));
			a = lower_bound(vecs, vecs + num, m);
			break;
		}
		case 1:
			m -= 2ull << s;
			a = lower_bound(vecs, vecs + num, m);
			break;
		case 2:
			break;
	}

	if (a != vecs && m - a[-1] < a[0] - m)
		--a;
	return indices[a - vecs];
}

bool SpacePartition::closest(param(position3) _p0, param(position3) _p1) {
	position3	p0	= _p0;
	position3	p1	= _p1;

	if (ext.clip(p0, p1))
		return false;

	float3x4	mat	= matrix();
	p0				= mat * p0;
	p1				= mat * p1;
	vector3		d	= p1 - p0;
	float		dd	= max_component(d);
	uint32		di = iorf(dd).m >> 2;

	uint64		m0	= mask(p0);
	uint64		m1	= mask(p1);
	uint64		*v0	= lower_bound(vecs, vecs + num, m0);
	uint64		*v1	= lower_bound(vecs, vecs + num, m1);

	uint64		dm	= m0 ^ m1;
	uint64		h	= highest_set((dm | (dm >> 1) | (dm >> 2)) & s_bitblocks<uint64,3,1>::value);

	for (;;) {
		bool	xcross	= dm & (h << 0);
		bool	ycross	= dm & (h << 1);
		bool	zcross	= dm & (h << 2);
	}

	return true;
}
*/

//-----------------------------------------------------------------------------
//	Topology
//-----------------------------------------------------------------------------

Topology::Topology(Type _type, uint8 _chunks) : type(_type), chunks(_chunks) {
	static const Prim2Vert p2v_table[] = {
		{0,		0,	0,	0},	//UNKNOWN,		
		{1,		0,	0,	0},	//POINTLIST,		
		{2,		0,	0,	0},	//LINELIST,		
		{1,		1,	0,	0},	//LINESTRIP,		
		{1,		0,	0,	0},	//LINELOOP,		
		{3,		0,	0,	0},	//TRILIST,		
		{1,		2,	1,	0},	//TRISTRIP,		
		{1,		2,	0,	0},	//TRIFAN,			
		{3,		0,	0,	0},	//RECTLIST,		
		{4,		0,	0,	0},	//QUADLIST,		
		{2,		2,	0,	0},	//QUADSTRIP,		
		{0xff,	0,	0,	0},	//POLYGON,		
		{4,		0,	0,	0},	//LINELIST_ADJ,	
		{1,		2,	0,	1},	//LINESTRIP_ADJ,	
		{6,		0,	0,	0},	//TRILIST_ADJ,	
		{2,		2,	0,	0},	//TRISTRIP_ADJ,	
		{0xff,	0,	0,	0},	//PATCH,			
	};
	static const struct {uint8 type:4, num:4; } hw_type_table[] = {
		{UNKNOWN,		1},	//UNKNOWN,
		{POINTLIST,		1},	//POINTLIST,		
		{LINELIST,		1},	//LINELIST,		
		{LINESTRIP,		1},	//LINESTRIP,		
		{LINELOOP,		1},	//LINELOOP,		
		{TRILIST,		1},	//TRILIST,		
		{TRISTRIP,		1},	//TRISTRIP,		
		{TRIFAN,		1},	//TRIFAN,			
		{TRILIST,		2},	//RECTLIST,		
		{TRILIST,		2},	//QUADLIST,		
		{TRISTRIP,		2},	//QUADSTRIP,		
		{TRIFAN,		0},	//POLYGON,		
		{LINELIST,		1},	//LINELIST_ADJ,	
		{LINESTRIP,		1},	//LINESTRIP_ADJ,	
		{TRILIST,		1},	//TRILIST_ADJ,	
		{TRISTRIP,		1},	//TRISTRIP_ADJ,	
		{POINTLIST,		0},	//PATCH,			
	};

	hw_type	= (Type)hw_type_table[type].type;
	hw_mul	= hw_type_table[type].num;
	p2v		= p2v_table[type];
	hw_p2v	= p2v_table[hw_type];
}

PrimType Topology::GetHWType(int &num_prims) const {
	static const PrimType conv[] = {
		PRIM_UNKNOWN,	//UNKNOWN
		PRIM_POINTLIST,	//POINTLIST
		PRIM_LINELIST,	//LINELIST
		PRIM_LINESTRIP,	//LINESTRIP
		PRIM_LINELIST,	//LINELOOP
		PRIM_TRILIST,	//TRILIST
		PRIM_TRISTRIP,	//TRISTRIP
		PRIM_TRIFAN,	//TRIFAN
		PRIM_RECTLIST,	//RECTLIST
		PRIM_TRILIST,	//QUADLIST,
		PRIM_TRISTRIP,	//QUADSTRIP,
		PRIM_TRIFAN,	//POLYGON,
		PRIM_LINELIST,	//LINELIST_ADJ
		PRIM_LINESTRIP,	//LINESTRIP_ADJ
		PRIM_TRILIST,	//TRILIST_ADJ
		PRIM_TRISTRIP,	//TRISTRIP_ADJ
		PRIM_POINTLIST,	//PATCH,
	};
	num_prims *= hw_mul;
	return conv[hw_type];
}

//-----------------------------------------------------------------------------
//	Tesselation
//-----------------------------------------------------------------------------

static uint16	*put_tri(uint16 *p, uint32 a, uint32 b, uint32 c) {
	p[0] = a;
	p[1] = b;
	p[2] = c;
	return p + 3;
}
static uint16	*put_quad(uint16 *p, uint32 a, uint32 b, uint32 c, uint32 d) {
	p[0] = a;
	p[1] = p[4] = b;
	p[2] = p[3] = d;
	p[5] = c;
	return p + 6;
}
static uint16	*put_fan(uint16 *p, uint32 a0, uint32 b0, uint32 bn) {
	while (bn--) {
		p[0] = b0;
		p[1] = a0;
		p[2] = ++b0;
		p += 3;
	}
	return p;
}
static uint16	*put_strip(uint16 *p, uint32 a0, uint32 an, uint32 b0, uint32 bn) {
	uint32 n = min(an, bn);
//	if (n == 0)
//		return p;

	for (; n--; p += 6) {
		p[0] = a0;
		p[1] = p[4] = b0;
		p[2] = p[3] = ++a0;
		p[5] = ++b0;
	}
	return	an < bn	? put_fan(p, a0, b0, bn - an)
		:	bn < an	? put_fan(p, b0, a0, an - bn)
		:	p;
}

//quad
Tesselation::Tesselation(const float4p &edges, const float2p &inside, Spacing spacing) {
	if (edges.x <= 0 || edges.y <= 0 || edges.z <= 0|| edges.w <= 0)
		return;

	uint32	ex	= effective(spacing, inside.x);
	uint32	ey	= effective(spacing, inside.y);
	uint32	e0	= effective(spacing, edges.x);
	uint32	e1	= effective(spacing, edges.y);
	uint32	e2	= effective(spacing, edges.z);
	uint32	e3	= effective(spacing, edges.w);
	if (e0 == 1 && e1 == 1 && e2 == 1 && e3 == 1) {
		if (ex == 1 || ex == 1) {
			auto	*p	= uvs.resize(4).begin();
			p[0].set(0, 0);
			p[1].set(1, 0);
			p[2].set(0, 1);
			p[2].set(1, 1);

			put_quad(indices.resize(2 * 3).begin(), 3, 2, 1, 0);
			return;
		}
	}
	if (ex == 1)
		ex = effective_min(spacing);
	if (ey == 1)
		ey = effective_min(spacing);


	uint32	nuv_outer	= e0 + e1 + e2 + e3;
	uint32	nuv			= nuv_outer + (ex - 1) * (ey - 1);
	uint32	ntri		= e0 + e1 + e2 + e3 + (ex + ey - 4) * 2 + (ex - 2) * (ey - 2) * 2;

	auto	*p		= uvs.resize(nuv).begin();
	auto	*ix		= indices.resize(ntri * 3).begin();
	uint32	a		= 0, b = nuv_outer;
	float	fi;

	--ex;
	--ey;

	//outer ring
	p++->set(0, 0);
	fi = 1 - (e0 - edges.x) / 2;
	for (int i = 0; i < e0 - 1; i++)
		p++->set((i + fi) / edges.x, 0);

	ix	= put_strip(ix, a, e0, b, ex - 1);
	a	+= e0;
	b	+= ex - 1;

	p++->set(1, 0);
	fi = 1 - (e1 - edges.y) / 2;
	for (int i = 0; i < e1 - 1; i++)
		p++->set(1, (i + fi) / edges.y);

	ix = put_strip(ix, a, e1, b, ey - 1);
	a	+= e1;
	b	+= ey - 1;

	p++->set(1, 1);
	fi = 1 - (e2 - edges.z) / 2;
	for (int i = 0; i < e2 - 1; i++)
		p++->set(1 - (i + fi) / edges.z, 1);

	ix	= put_strip(ix, a, e2, b, ex - 1);
	a	+= e2;
	b	+= ex - 1;

	p++->set(0, 1);
	fi = 1 - (e3 - edges.w) / 2;
	for (int i = 0; i < e3 - 1; i++)
		p++->set(0, 1 - (i + fi) / edges.w);

	ix	= put_strip(ix, a, e3 - 1, b, ey - 2);
	if (ey > 1)
		ix	= put_quad(ix, 0, a + e3 - 1, b + ey - 2, nuv_outer);
	else
		ix	= put_quad(ix, 0, b, a + e3 - 2, a + e3 - 1);

	a	+= e3;
	b	+= ey - 1;

	float	s = (1 - (ex + 1 - inside.x) / 2) / inside.x;
	float	t = (1 - (ey + 1 - inside.y) / 2) / inside.y;

	//inner rings
	while (ex > 1 && ey > 1) {
		for (int i = 0; i < ex - 1; i++)
			p++->set(s + i / inside.x, t);

		for (int i = 0; i < ey - 1; i++)
			p++->set(1 - s, t + i / inside.y);


		for (int i = 0; i < ex - 1; i++)
			p++->set(1 - s - i / inside.x, 1 - t);

		for (int i = 0; i < ey - 1; i++)
			p++->set(s, 1 - t - i / inside.y);

		if (ex == 2) {
			for (int i = 0; i < ey - 1; i++)
				ix	= put_quad(ix, (i == 0 ? a : b - i), b - i - 1, a + i + 2, a + i + 1);

		} else if (ey == 2) {
			for (int i = 0; i < ex - 1; i++)
				ix	= put_quad(ix, (i == 0 ? a : b - i), b - i - 1, a + i + 2, a + i + 1);

		} else {
			uint32	a0 = a, b0 = b;
			ix	= put_strip(ix, a, ex - 1, b, ex - 3);
			a	+= ex - 1;
			b	+= ex - 3;

			ix	= put_strip(ix, a, ey - 1, b, ey - 3);
			a	+= ey - 1;
			b	+= ey - 3;

			ix	= put_strip(ix, a, ex - 1, b, ex - 3);
			a	+= ex - 1;
			b	+= ex - 3;

			if (ey > 3) {
				ix	= put_strip(ix, a, ey - 2, b, ey - 4);
				ix	= put_quad(ix, a0, a + ey - 2, b + ey - 4, b0);
			} else {
				ix	= put_quad(ix, a0, a + ey - 2, a + ey - 3, b);
			}
			a	+= ey - 1;
			b	+= ey - 3;
		}

		s += 1 / inside.x;
		t += 1 / inside.y;

		ex -= 2;
		ey -= 2;
	}

	if (ex == 1) {
		for (int i = 0; i < ey; i++)
			p++->set(s, t + i / inside.y);

	} else if (ey == 1) {
		for (int i = 0; i < ex; i++)
			p++->set(s + i / inside.x, t);
	}
}

//tri
Tesselation::Tesselation(const float3p &edges, float inside, Spacing spacing) {
	if (edges.x <= 0 || edges.y <= 0 || edges.z <= 0)
		return;

	uint32	ei	= effective(spacing, inside);
	uint32	e0	= effective(spacing, edges.x);
	uint32	e1	= effective(spacing, edges.y);
	uint32	e2	= effective(spacing, edges.z);
	if (e0 == 1 && e1 == 1 && e2 == 1) {
		if (ei == 1) {
			auto	*p	= uvs.resize(3).begin();
			p[0].set(0, 0);
			p[1].set(1, 0);
			p[2].set(0, 1);
			return;
		}
		ei = effective_min(spacing);
	}

	uint32	outer_ring	= e0 + e1 + e2;
	uint32	inner		= ei & 1 ? square(ei + 1) * 3 / 4 : (square(ei / 2) + ei / 2) * 3 + 1;
	inner -= ei * 3;

	auto	*p		= uvs.resize(outer_ring + inner).begin();
	float	fi;

	//outer ring
	fi = 1 - (e0 - edges.x) / 2;

	// 1,0,0 ... 1-t,t,0 ... 0,1,0
	p++->set(1, 0);
	for (int i = 0; i < e0 - 1; i++) {
		float	t = (i + fi) / edges.x;
		p++->set(1 - t, t);
	}

	// 0,1,0 ... 0,1-t,t ... 0,0,1
	p++->set(0, 1);
	fi = 1 - (e1 - edges.y) / 2;
	for (int i = 0; i < e1 - 1; i++) {
		float	t = (i + fi) / edges.y;
		p++->set(0, 1 - t);
	}

	// 0,0,1 ... t,0,1-t ... 1,0,0
	p++->set(0, 0);
	fi = 1 - (e2 - edges.z) / 2;
	for (int i = 0; i < e2 - 1; i++) {
		float	t = (i + fi) / edges.z;
		p++->set(t, 0);
	}

	float	t = (1 - (ei - inside) / 2) / inside;

	//inner rings
	--ei;
	while (ei > 1) {
		
		for (int i = 0; i < ei - 1; i++) {
			float	u = t + i / inside;
			p++->set(1 - u, u);
		}

		for (int i = 0; i < ei - 1; i++) {
			float	u = t + i / inside;
			p++->set(t, 1 - u);
		}

		for (int i = 0; i < ei - 1; i++) {
			float	u = t + i / inside;
			p++->set(u, t);
		}

		t += 1 / inside;
		ei -= 2;
	}

	if (ei == 1)
		p++->set(t, t);

}

//isoline
Tesselation::Tesselation(const float2p &edges, Spacing spacing) {
	if (edges.x <= 0 || edges.y <= 0)
		return;

	uint32	e0 = effective(EQUAL, edges.x);
	uint32	e1 = effective(spacing, edges.y);
	auto	*p	= uvs.resize(e0 * (e1 + 1)).begin();

	for (int x = 0; x < e0; x++) {
		float	u = float(x) / e0;

		p++->set(u, 0);
		float	fy = 1 - (e1 - edges.y) / 2;
		for (int y = 0; y < e1 - 1; y++) {
			float	v = (y + fy) / edges.y;
			p++->set(u, v);
		}
		p++->set(u, 1);
	}
}

//-----------------------------------------------------------------------------
//	Finders
//-----------------------------------------------------------------------------

template<typename I> dynamic_array<cuboid> get_extents(I prims, int num_prims) {
	dynamic_array<cuboid>	exts(num_prims);
	for (int i = 0; i < num_prims; ++i)
		exts[i] = get_box(prims[i]);
	return exts;
}


int FindVertex(param(float4x4) mat, const kd_tree<dynamic_array<constructable<float3p> > > &partition) {
	typedef kd_tree<dynamic_array<constructable<float3p> > >::kd_node	kd_node;
	typedef kd_tree<dynamic_array<constructable<float3p> > >::kd_leaf	kd_leaf;

	struct stack_element {
		const kd_node	*node;
		float			t0, t1;
	};

	stack_element	stack[32], *sp = stack;
	const kd_node	*node = partition.nodes.begin();

	float4x4 cof		= cofactors(mat);
	float	min_dist	= maximum;
	int		min_index	= -1;
	float	t0			= 0, t1 = maximum;

	for (;;) {
		while (node->axis >= 0) {
			float4	p		= cof[node->axis] - cof.w * node->split;
			int		side	= p.w > zero;
			float	t		= -p.w / p.z;

			if (t > 0) {
				if (t < t1) {
					sp->node	= node->child[1 - side];
					sp->t0		= t;
					sp->t1		= t1;
					++sp;
					t1		= t;
				}
				if (t < t0)
					break;
			}
			node	= node->child[side];
		}

		if (node->axis < 0) {
			kd_leaf	*leaf = (kd_leaf*)node;
			for (auto &i : leaf->indices) {
				position3	p		= project(mat * float4(position3(partition.data[i])));
				float		dist	= len2(p.xy);
				if (dist < min_dist) {
					min_dist	= dist;
					min_index	= i;
				}
			}
		}

		if (sp == stack)
			return min_index;

		--sp;
		node	= sp->node;
		t0		= sp->t0;
		t1		= sp->t1;
	}

	return -1;
}

int FindFromIndex(const uint32 *p, int v) {
	for (const uint32 *p2 = p; ; ++p2) {
		if (*p2 == v)
			return p2 - p;
	}
}

template<typename I> int FindVertex(param(float4x4) mat, I verts, int num_verts, float tol) {
	float	closestd	= tol;
	int		closestv	= -1;
	for (int i = 0; i < num_verts; ++i) {
		position3	p = project(mat * float4(verts[i]));
		float		d	= len2(p.xy);
		if (d < closestd) {
			closestd = d;
			closestv = i;
		}
	}
	return closestv;
}

template<typename I> int FindPrim(param(float4x4) mat, I prims, int num_prims) {
	float4		pn			= float4(zero, zero, -one, one) / mat;
	float4		pf			= float4(zero, zero, one, one) / mat;
	position3	from		= project(pn);
	float3		dir			= select(pf.w == zero, pf.xyz, project(pf) - from);

	float		closestt	= 1;
	int			closestf	= -1;

	position3		pos[64];
	for (int i = 0; i < num_prims; ++i) {
		auto	prim		= prims[i];
		uint32	num_verts	= prim.size();
		if (num_verts < 3)
			continue;
		
		copy(prim, pos);

		if (num_verts > 3) {
			if (!get_box(pos, pos + num_verts).contains(zero))
				continue;
		}

		triangle3	tri(pos[0], pos[1], pos[2]);
		float		t;
		if (tri.check_ray(from, dir, t)) {
			if (closestf < 0 || t <= closestt) {
				closestt = t;
				closestf = i;
			}
		}
	}
	return closestf;
}

template<typename I> int FindVertPrim(param(float4x4) mat, I prims, int num_prims, int num_verts, float tol, int &face) {
	face	= prims->size() < 3 ? -1 : FindPrim(mat, prims, num_prims);
	if (face >= 0) {
		float4	pos[64];
		copy(prims[face], pos);
		return FindVertex(mat, pos, 3, 1e38f);
	}
	return FindVertex(mat, prims.i, num_verts, tol);
}

//-----------------------------------------------------------------------------
//	Axes
//-----------------------------------------------------------------------------

void Axes::draw(GraphicsContext &ctx, param(float3x3) rot)	const {
	ctx.SetDepthTestEnable(true);
	ctx.SetDepthTest(DT_USUAL);
	DrawAxes(ctx, .05f, .2f, .2f, 0.25f, tech, rot);
}
int Axes::click(const Rect &client, const Point &mouse, param(float3x3) rot) const {
	return GetAxesClick(proj() * rot, (float2(mouse) - float2(size / 2, client.Height() - size / 2)) / float2(size / 2, size / 2));
}

//-----------------------------------------------------------------------------
//	MeshWindow
//-----------------------------------------------------------------------------

struct float3_in4 {
	float4p	v;
	operator position3() const { return project(float4(v)); }
};
template<> struct vec_traits<float3_in4> : vec_traits<position3> {};
template<> struct vget<float3_in4> : vget<position3> {};

struct vertex_tex {
	float3p	pos;
	float2p	uv;
};

namespace iso {
template<> VertexElements *GetVE<vertex_idx>() {
	static VertexElements ve[] = {
		VertexElements(0, GetComponentType<float4p>(), USAGE_POSITION, 0, 0),
		VertexElements(0, GetComponentType<uint8[4]>(), USAGE_BLENDINDICES, 0, 1),
		terminate,
	};
	return ve;
};
template<> VertexElements *GetVE<vertex_tex>() {
	static VertexElements ve[] = {
		VertexElements(&vertex_tex::pos, USAGE_POSITION),
		VertexElements(&vertex_tex::uv, USAGE_TEXCOORD),
		terminate,
	};
	return ve;
};
template<> VertexElements *GetVE<vertex_norm>() {
	static VertexElements ve[] = {
		VertexElements(0, GetComponentType<float4p>(), USAGE_POSITION, 0, 0),
		VertexElements(0, GetComponentType<float3p>(), USAGE_NORMAL, 0, 1),
		terminate,
	};
	return ve;
};
}
void MeshWindow::SetScissor(param(rectangle) _scissor) {
	scissor = cuboid(position3(_scissor.pt0(), zero), position3(_scissor.pt1(), one));
	flags.set(SCISSOR);
}

void MeshWindow::SetScissor(param(cuboid) _scissor) {
	scissor = _scissor;
	flags.set(SCISSOR);
}

void MeshWindow::SetDepthBounds(float zmin, float zmax) {
	if (!flags.test(SCISSOR))
		SetScissor(cuboid(position3(viewport.y), viewport.x));
	scissor.p0.z = zmin;
	scissor.p1.z = zmax;
}

void MeshWindow::SetScreenSize(const Point &s) {
	screen_size = s;
	flags.set(SCREEN_RECT);
}

void MeshWindow::SetColourTexture(const Texture &_ct) {
	ct = _ct;
	Invalidate();
}

Texture MipDepth(const Texture &_dt) {
	Point	size	= _dt.Size();
	int		width	= size.x, height = size.y;
	int		mips	= log2(max(width, height));
	Texture	dt2(TEXF_R32F, width, height, 1, mips, MEM_TARGET);

	GraphicsContext		ctx;
	graphics.BeginScene(ctx);

	Texture	dt3 = _dt;
	AddShaderParameter("_zbuffer", dt3);
	static pass	*depth_min		= *root["data"]["default"]["depth_min"][0];

	for (int i = 0; i < mips; i++) {
		AddShaderParameter("mip", 0);//max(i - 1, 0));
		ctx.SetRenderTarget(dt2.GetSurface(i));
		Set(ctx, depth_min);
		ImmediateStream<vertex_tex>	ims(ctx, PRIM_QUADLIST, 4);
		vertex_tex	*p	= ims.begin();
		p[0].pos.set(+1, -1, 1); p[0].uv.set(1, 0);
		p[1].pos.set(-1, -1, 1); p[1].uv.set(0, 0);
		p[2].pos.set(-1, +1, 1); p[2].uv.set(0, 1);
		p[3].pos.set(+1, +1, 1); p[3].uv.set(1, 1);
		dt3 = Texture(dt2.GetSurface(i));
	}
	graphics.EndScene(ctx);
	return dt2;
}

void MeshWindow::SetDepthTexture(const Texture &_dt) {
	dt = _dt;
//	dt = MipDepth(_dt);
	Invalidate();
}

void MeshWindow::GetMatrices(float4x4 *world, float4x4 *proj) const {
	float2		size(Size());
	float4x4	vpmat	= float4x4(scale(viewport.x * float3(1,1,zscale)));
	float2		zoom2	= min_component(abs(size / viewport.x.xy)) * viewport.x.x * zoom / size;

	switch (mode) {
		case SCREEN_PERSP: {
			float4x4 unpersp = float4x4(
				float4(1,0,0,0),
				float4(0,1,0,0),
				float4(0,0,0,-zoom),	//-0.9f
				float4(0,0,1 + viewport.y.z / viewport.x.z,1)
			);

			*proj	= perspective_projection(zoom2.x, zoom2.y, 1);
			*world	= vpmat * unpersp;
			break;
		}
		case SCREEN:
			vpmat.w.z = (viewport.y.z - half) * zscale;
			*proj	= float4x4(float3x3(scale(float3(zoom2, zero))));
			*world	= vpmat;
			break;

		case FIXEDPROJ:
			*proj	= scale(float3(zoom2, rlen(extent.extent()))) * fixed_proj;
			*world	= float4x4(translate(-extent.centre()));
			break;

		default:
			*proj	= perspective_projection(zoom2.x, zoom2.y, move_scale * 10 / 256);
			*world	= float4x4(translate(-extent.centre()));
			break;
	}
}

float4x4 MeshWindow::GetMatrix() const {
	float4x4	world, proj;
	GetMatrices(&world, &proj);
	return proj * view_loc * world;
}

void MeshWindow::SelectVertex(const Point &mouse) {
	Point		size		= Size();
	float		sx			= float(size.x) / 2, sy = float(size.y) / 2, sz = max(sx, sy);
	float4x4	mat			= translate(sx - mouse.x, sy - mouse.y, 0) * (scale(sx, sy, one) * GetMatrix());
//	float4x4	mat			= translate(sx - mouse.x, sy - mouse.y, 0) * scale(sx, sy, 1) * GetMatrix();
	int			face		= -1, vert = -1;
#if 1
	if (!oct.nodes) {
		//partition.init(make_range_n((float3_in4*)vb.Data().get(), vb.Size()), 4);
		int						hw_prims	= num_prims * topology.hw_mul;
		dynamic_array<cuboid>	exts(hw_prims);
		cuboid					*ext		= exts;
		if (ib) {
			for (auto &i : make_range_n(make_prim_iterator(topology.hw_p2v, make_indexed_iterator(vb.Data().get(), ib.Data().get())), hw_prims))
				*ext++ = get_box(i);
		} else {
			for (auto &i : make_range_n(make_prim_iterator(topology.hw_p2v, vb.Data().get()), hw_prims))
				*ext++ = get_box(i);
		}
		oct.init(exts, hw_prims);
	}

	if (ib) {
		auto	prims = make_prim_iterator(topology.hw_p2v, make_indexed_iterator(vb.Data().get(), ib.Data().get()));
		face	= oct.shoot_ray(mat, 0.25f, &[prims](int i, param(position3) p, param(vector3) d, float &t) {
			return prim_check_ray(prims[i], p, d, t);
		});
		if (face >= 0) {
			float4	pos[64];
			copy(prims[face], pos);
			vert = FindVertex(mat, pos, 3, 1e38f);
		}
	} else {
		auto	prims = make_prim_iterator(topology.hw_p2v, vb.Data().get());
		face	= oct.shoot_ray(mat, 0.25f, &[prims](int i, param(position3) p, param(vector3) d, float &t) {
			return prim_check_ray(prims[i], p, d, t);
		});
		if (face >= 0) {
			float4	pos[64];
			copy(prims[face], pos);
			vert = FindVertex(mat, pos, 3, 1e38f);
		}
	}
	/*
	if (face < 0) {
		vert = FindVertex(mat, partition);
		if (vert >= 0 && ib) {
			vert = FindFromIndex(ib.Data(), vert);
		}
	}
	*/
#else
	int			hw_prims	= num_prims * topology.hw_mul;
	vert	= ib
		? FindVertPrim(mat, make_prim_iterator(topology.hw_p2v, make_indexed_iterator(vb.Data().get(), ib.Data().get())), hw_prims, num_verts, 10, face)
		: FindVertPrim(mat, make_prim_iterator(topology.hw_p2v, vb.Data().get()), hw_prims, num_verts, 10, face);

#endif
	if (vert >= 0) {
		vert += topology.hw_p2v.first_vert(face % topology.hw_mul);
		face /= topology.hw_mul;
		vert = topology.p2v.first_vert(face) + topology.FromHWOffset(vert);
		MeshNotification::Set(*this, vert).Send(Parent());
	}
}

LRESULT MeshWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			addref();
			toolbar.Create(*this, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 0);
			toolbar.Init(IDR_TOOLBAR_MESH);
			SetSize(*this, GetClientRect().Size());
			break;

		case WM_SIZE:
			SetSize(*this, Point(lParam));
			toolbar.Resize(toolbar.GetItemRect(toolbar.Count() - 1).BottomRight() + Point(3, 3));
			break;

		case WM_PAINT:
			Paint();
			break;

		case WM_LBUTTONDOWN: {
			Point		mouse(lParam);
			if (int axis = axes.click(GetClientRect(), mouse, view_loc.rot)) {
				quaternion	q = GetAxesRot(axis);
				if (q == target_loc.rot)
					q = rotate_in_x(pi) * q;
				target_loc.rot = q;
				InitTimer(0.01f);
				break;
			}
			if (vb) {
				SelectVertex(mouse);
			}
		}
		// fall through
		case WM_RBUTTONDOWN:
			Stop();
			prevmouse	= Point(lParam);
			prevbutt	= wParam;
			SetFocus();
			break;

		case WM_LBUTTONDBLCLK:
			ResetView();
			break;

		case WM_MOUSEACTIVATE:
			SetAccelerator(*this, IDR_ACCELERATOR_MESH);
			break;

		case WM_MOUSEMOVE:
			MouseMove(Point(lParam), wParam);
			break;

		case WM_MOUSEWHEEL:
			if (GetRect().Contains(Point(lParam)))
				MouseWheel(ToClient(Point(lParam)), LOWORD(wParam), (short)HIWORD(wParam));
			break;

		case WM_COMMAND:
			switch (uint16 id = LOWORD(wParam)) {
				case ID_MESH_BACKFACE:
					cull = cull == BFC_BACK ? BFC_FRONT : BFC_BACK;
					toolbar.CheckButton(id, cull == BFC_FRONT);
					Invalidate();
					break;
				case ID_MESH_RESET:
					ResetView();
					break;
				case ID_MESH_BOUNDS:
					toolbar.CheckButton(id, flags.flip_test(BOUNDING_EDGES));
					Invalidate();
					break;
				case ID_MESH_FILL:
					toolbar.CheckButton(id, flags.flip_test(FILL));
					Invalidate();
					break;
				case ID_MESH_PROJECTION:
					if (mode == FIXEDPROJ) {
						mode = prev_mode;
					} else {
						prev_mode = mode;
						mode	= FIXEDPROJ;
					}
					Invalidate();
					break;
			}
			break;

		case WM_ISO_TIMER: {
			float		rate	= 0.1f;

			if (mode == SCREEN)
				zoom	= lerp(zoom, 0.8f, rate);

			view_loc.trans	= lerp(view_loc.trans, target_loc.trans, rate);
			view_loc.rot	= normalise(slerp(view_loc.rot, target_loc.rot, rate));
			if (len2(view_loc.trans - target_loc.trans) < move_scale / 256 && abs(cosang(view_loc.rot, target_loc.rot)) > 0.999f) {
				view_loc.trans	= target_loc.trans;
				view_loc.rot	= target_loc.rot;
				Stop();
			}
			Invalidate();
			break;
		}

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA: {
					TOOLTIPTEXT	*ttt	= (TOOLTIPTEXT*)nmh;
					ttt->hinst			= GetLocalInstance(); 
					ttt->lpszText		= MAKEINTRESOURCE(ttt->hdr.idFrom); 
					break;
				}
			}
			break;
		}
		case WM_NCDESTROY:
			release();//delete this;
			break;
	}
	return Super(message, wParam, lParam);
}

void MeshWindow::MouseMove(Point mouse, int buttons) {
	if (prevbutt) {
		if (buttons & MK_RBUTTON) {
			float3	move(int(mouse.x - prevmouse.x), int(mouse.y - prevmouse.y), zero);
			view_loc.translate(move * move_scale / 256);
			Invalidate();

		} else if (buttons & MK_LBUTTON) {
			quaternion	rot	= quaternion(int(prevmouse.y - mouse.y), int(mouse.x - prevmouse.x), 0, 256);
			if (buttons & MK_CONTROL) {
				light_rot =  normalise(rot * light_rot);

			} else if (mode == SCREEN_PERSP) {
				float4x4	world, proj;
				GetMatrices(&world, &proj);
				position3	centre = project(world * extent.centre());
				view_loc.rotate_about_local(rot, centre);

			} else {
				view_loc.rotate(rot);

			}
			Invalidate();
		}
	}
	prevmouse	= mouse;
}

void MeshWindow::MouseWheel(Point mouse, int buttons, int x) {
	Stop();
	float	mag	= iso::pow(1.05f, x / 64.f);
	switch (mode) {
		case SCREEN:
			zoom	*= mag;
			break;

		case SCREEN_PERSP:
			if (buttons & MK_CONTROL) {
				zscale	*= mag;
				//zoom	= pow(zoom, m);
			} else {
				Point	size = GetClientRect().Size();
				view_loc.translate(float3(float(mouse.x) / size.x * 2 - 1, float(mouse.y) / size.y * 2 - 1, 1) * float(x * move_scale / 1024));
			}
			break;
		default:
			if (buttons & MK_CONTROL)
				zoom	*= mag;
			else
				view_loc.translate(float3(zero, zero, x * move_scale / 1024));
			break;
	}
	Invalidate();
}

void MeshWindow::ResetView() {
	target_loc.reset();
	if (mode == PERSPECTIVE)
		target_loc.trans.z = move_scale;
	InitTimer(0.01f);
}

void MeshWindow::SetSelection(int i, bool zoom) {
	select = i;
	if (zoom) {
		position3	pos[64];
		if (ib) {
			copy(make_prim_iterator(topology.hw_p2v, make_indexed_iterator(vb.Data().get(), ib.Data().get()))[i], pos);
		} else {
			copy(make_prim_iterator(topology.hw_p2v, vb.Data().get())[i], pos);
		}

		float3		normal	= GetNormal(pos);
		position3	centre	= centroid(pos);
		float4x4	world, proj;
		GetMatrices(&world, &proj);

		if (reverse(cull, proj.det() < 0) == BFC_FRONT)
			normal = -normal;

		centre	= project(world * centre);
		float	dist		= sqrt(len(normal));

		float4	min_pos		= float4(0,0,-1,1) / proj;
		float	min_dist	= len(project(min_pos));

		dist	= max(dist, min_dist);


		target_loc.rot		= quaternion::between(normalise(normal), float3(0,0,-1));
//		target_loc.trans	= position3(0, 0, len(centre) + sqrt(len(normal) * half)) - target_loc.rot * float3(centre);
		target_loc.trans	= position3(0, 0, dist) - target_loc.rot * float3(centre);

		InitTimer(0.01f);

	}
	Invalidate();
}

bool MeshWindow::IsChunkStart(int i) const {
	if (topology.chunks)
		return i % topology.chunks == 0;
	if (patch_starts.size()) {
		auto j = lower_boundc(patch_starts, i);
		return j != patch_starts.end() && *j == i;
	}
	return false;
}

void MeshWindow::DrawMesh(GraphicsContext &ctx, int v, int n) {
	PrimType	prim = topology.GetHWType(n);

	if (int cs = topology.chunks) {
		v	= topology.VertexFromPrim(v, false);
		n	= topology.p2v.prims_to_verts(n);
		while (n) {
			int	n1 = min(n, cs - v % cs);
			if (ib)
				ctx.DrawIndexedVertices(prim, 0, num_verts, v, n1);
			else
				ctx.DrawVertices(prim, v, n1);
			n -= n1;
			v += n1;
		}

	} else {
		if (ib)
			ctx.DrawIndexedPrimitive(prim, 0, num_verts, v, n);
		else
			ctx.DrawPrimitive(prim, v, n);
	}
}

void DrawFrustum(GraphicsContext &ctx) {
	ImmediateStream<float3p>	ims(ctx, PRIM_LINELIST, 24);
	float3p	*p	= ims.begin();
	for (int i = 0; i < 12; i++, p += 2) {
		int	ix = i / 4, iy = (ix + 1) % 3, iz = (ix + 2) % 3;
		p[0][ix] = p[1][ix] = (i & 1) * 2 - 1;
		p[0][iy] = p[1][iy] = (i & 2) - 1;
		p[0][iz] = -1;
		p[1][iz] = +1;
	}
}

void DrawRect(GraphicsContext &ctx) {
	ImmediateStream<float3p>	ims(ctx, PRIM_LINESTRIP, 5);
	float3p	*p	= ims.begin();
	p[0] = p[4] = float3(-1, -1, 0);
	p[1] = float3(+1, -1, 0);
	p[2] = float3(+1, +1, 0);
	p[3] = float3(-1, +1, 0);
}

void SetThickPoint(GraphicsContext &ctx, float size) {
#ifdef USE_DX11
	static pass *thickpoint	= *root["data"]["default"]["thickpoint4"][0];
	float2		point_size	= float2(size / ctx.GetWindow().Size());
	AddShaderParameter("point_size",		point_size);
	Set(ctx, thickpoint);
#else
	ctx.Device()->SetRenderState(D3DRS_POINTSIZE, iorf(8.f).i);
#endif
}

void MeshWindow::Paint() {
	static pass	*blend_idx		= *root["data"]["default"]["blend_idx"][0];
	static pass *blend4			= *root["data"]["default"]["blend4"][0];
	static pass *tex_blend		= *root["data"]["default"]["tex"][0];
	static pass *specular		= *root["data"]["default"]["specular"][0];
	static pass *specular4		= *root["data"]["default"]["specular4"][0];

	axes.tech = specular;

#if 0
	Texture	dt2;
	if (dt)
		dt2 = MipDepth(dt);
#endif

	DeviceContextPaint	dc(*this);
	GraphicsContext		ctx;
	graphics.BeginScene(ctx);

	ctx.SetRenderTarget(GetDispSurface());
	ctx.Clear(colour(0,0,0));
	ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);

	float4x4	world, proj0;
	GetMatrices(&world, &proj0);

	float4x4	proj			= hardware_fix(proj0);
	float3x4	view			= view_loc;
	float4x4	viewProj		= proj * view;
	float4x4	worldviewproj	= viewProj * world;

	float3x4	iview			= inverse(view);
	bool		flip			= proj.det() < 0;
	BackFaceCull cull2			= cull == BFC_NONE ? BFC_NONE : reverse(cull, flip);
	float		flip_normals	= cull2 == BFC_FRONT ? -1 : 1;
	float3		shadowlight_dir	= light_rot * float3(0, 0, -1);
	colour		shadowlight_col(0.75f);
	colour		ambient(float3(0.25f));
	colour		tint(one);
	colour		col;

	AddShaderParameter("projection",		proj);
	AddShaderParameter("worldViewProj",		worldviewproj);
	AddShaderParameter("matrices",			matrices);
	AddShaderParameter("diffuse_colour",	col);

	AddShaderParameter("iview",				iview);
	AddShaderParameter("world",				world);
	AddShaderParameter("view",				view);
	AddShaderParameter("viewProj",			viewProj);
	AddShaderParameter("flip_normals",		flip_normals);
	AddShaderParameter("tint",				tint);
	AddShaderParameter("shadowlight_dir",	shadowlight_dir);
	AddShaderParameter("shadowlight_col",	shadowlight_col);
	AddShaderParameter("light_ambient",		ambient);

	worldviewproj	= viewProj * world;

	plane		near_plane(float3(0,0,1),-1);
	plane		far_plane(float3(0,0,1), 1);
	plane		near_plane2		= worldviewproj * near_plane;

	float4	pos(0, 0, 0, 1);

	float4x4	wvp			= transpose(worldviewproj);
	wvp[2]					= near_plane.as_float4();
	float4		near_pt		= cofactors(wvp) * pos;
	wvp[2]					= far_plane.as_float4();
	float4		far_pt		= cofactors(wvp) * pos;

	bool		test		= near_plane2.test(near_pt);
	bool		test2		= near_plane2.clip(near_pt, far_pt);

#if 0
	if (dt) {
		col				= one;
		AddShaderParameter("diffuse_samp", dt);
		Set(ctx, tex_blend);
		ImmediateStream<vertex_tex>	ims(ctx, PRIM_QUADLIST, 4);
		vertex_tex	*p	= ims.begin();
		p[0].pos.set(+1, -1, 1); p[0].uv.set(1, 0);
		p[1].pos.set(-1, -1, 1); p[1].uv.set(0, 0);
		p[2].pos.set(-1, +1, 1); p[2].uv.set(0, 1);
		p[3].pos.set(+1, +1, 1); p[3].uv.set(1, 1);
	} else if (ct) {
		col				= one;
		AddShaderParameter("diffuse_samp", ct);
		Set(ctx, tex_blend);
		ImmediateStream<vertex_tex>	ims(ctx, PRIM_QUADLIST, 4);
		vertex_tex	*p	= ims.begin();
		p[0].pos.set(+1, -1, 1); p[0].uv.set(1, 0);
		p[1].pos.set(-1, -1, 1); p[1].uv.set(0, 0);
		p[2].pos.set(-1, +1, 1); p[2].uv.set(0, 1);
		p[3].pos.set(+1, +1, 1); p[3].uv.set(1, 1);
	}
#elif 1
	if (dt) {
		AddShaderParameter("linear_samp", dt);
		AddShaderParameter("diffuse_samp", ct ? ct : dt);
		static pass	*depth_ray	= *root["data"]["default"]["depth_ray"][0];
		Set(ctx, depth_ray);
		ImmediateStream<float3p>	ims(ctx, PRIM_QUADLIST, 4);
		float3p	*p	= ims.begin();
		p[0].set(+1, -1, 1);
		p[1].set(-1, -1, 1);
		p[2].set(-1, +1, 1);
		p[3].set(+1, +1, 1);
	}
#endif

	if (!ct && flags.test(FRUSTUM_PLANES)) {
		col	= colour(0.25f, 0.25f, 0.25f);
		Set(ctx, blend4);
		ImmediateStream<float3p>	ims(ctx, PRIM_QUADLIST, 4);
		float3p	*p	= ims.begin();
		p[0].set(+1, -1, 1);
		p[1].set(-1, -1, 1);
		p[2].set(-1, +1, 1);
		p[3].set(+1, +1, 1);
	}

	if (num_prims) {
		worldviewproj	= viewProj * world;
		col	= colour(1,0.95f,0.8f);

		if (topology.type == Topology::POINTLIST || topology.type == Topology::PATCH) {
			ctx.SetBlendEnable(true);
			SetThickPoint(ctx, 4);
			ctx.SetVertices(vb);

		} else if (vb_matrix) {
			Set(ctx, blend_idx);
			ctx.SetVertexType<vertex_idx>();
			ctx.SetVertices(0, vb);
			ctx.SetVertices(1, vb_matrix);

		} else {
			Set(ctx, blend4);
			ctx.SetVertices(vb);
		}

		if (ib)
			ctx.SetIndices(ib);

		if (flags.test(FILL)) {
			ctx.SetBackFaceCull(cull2);
			ctx.SetFillMode(FILL_SOLID);
			ctx.SetZBuffer(Surface(TEXF_D24S8, Size(), MEM_DEPTH));
//			ctx.SetZBuffer(Surface(TEXF_D32F, Size()));
			
			ctx.SetDepthTest(DT_USUAL);
			ctx.SetDepthTestEnable(true);
			ctx.ClearZ();

			if (vb_norm && topology.type != Topology::POINTLIST && topology.type != Topology::PATCH) {
				Set(ctx, specular4);
				ctx.SetVertexType<vertex_norm>();
				ctx.SetVertices(0, vb);
				ctx.SetVertices(1, vb_norm);
			}
			DrawMesh(ctx);
			if (cull != BFC_NONE) {
				ctx.SetBackFaceCull(reverse(cull, !flip));
				ctx.SetFillMode(FILL_WIREFRAME);
				DrawMesh(ctx);
			}

		} else {
			//ctx.SetBackFaceCull(BFC_NONE);
			ctx.SetBackFaceCull(cull2);
			ctx.SetFillMode(FILL_WIREFRAME);
			DrawMesh(ctx);
		}

		if (select >= 0) {

			if (patch_starts.size()) {
				col = colour(0, 1, 0);
				/*
				static pass *thickline	= *root["data"]["default"]["thickline4"][0];
				float2		point_size	= float2(4 / ctx.GetWindow().Size());
				AddShaderParameter("point_size",		point_size);
				Set(ctx, thickline);
				*/
				Set(ctx, vb_matrix ? blend_idx : blend4);
				ctx.SetBackFaceCull(BFC_NONE);
				ctx.SetDepthTestEnable(false);
				ctx.SetFillMode(FILL_WIREFRAME);

				auto		j		= lower_boundc(patch_starts, select);
				int			end		= j[0];
				int			start	= j == patch_starts.begin() ? 0 : j[-1];
				int			n		= 1;
				PrimType	prim	= topology.GetHWType(n);
				if (ib)
					ctx.DrawIndexedVertices(prim, 0, num_verts, start, end - start);
				else
					ctx.DrawVertices(prim, start, end - start);
			}

			int	prim	= PrimFromVertex(select, false);
			int	off		= topology.ToHWOffset(max(select - VertexFromPrim(prim, false), 0));
			int	vert	= VertexFromPrim(prim * topology.hw_mul, true);

			col = colour(1,0.5f,0.5f);

			switch (topology.type) {
				case Topology::POINTLIST:
					break;

				case Topology::PATCH: {
				#if 0
					int			cp		= topology.hw_mul;
					PrimType	prim	= PatchPrim(cp);
					static technique *convex_hull	= root["data"]["default"]["convex_hull"];
					Set(ctx, (*convex_hull)[cp - 3]);
					try {
					if (flags.test(FILL)) {
						ctx.SetFillMode(FILL_SOLID);
						ctx.SetBackFaceCull(cull2);
						if (ib)
							ctx.DrawIndexedVertices(prim, 0, num_verts, vert, cp);
						else
							ctx.DrawVertices(prim, vert, cp);
					}
					} catch (...) {}

					try {
					ctx.SetBackFaceCull(BFC_NONE);
					ctx.SetDepthTestEnable(false);
					ctx.SetFillMode(FILL_WIREFRAME);
					if (ib)
						ctx.DrawIndexedVertices(prim, 0, num_verts, vert, cp);
					else
						ctx.DrawVertices(prim, vert, cp);
					} catch (...) {}

					col = colour(1,0.5f,0.5f);
				#endif
				}
				// fall through
				default: {
					if (topology.type == Topology::POINTLIST || topology.type == Topology::PATCH)
						SetThickPoint(ctx, 8);
					else
						Set(ctx, vb_matrix ? blend_idx : blend4);

					if (flags.test(FILL)) {
						ctx.SetFillMode(FILL_SOLID);
						ctx.SetBackFaceCull(cull2);
						DrawMesh(ctx, vert, 1);
					}

					ctx.SetBackFaceCull(BFC_NONE);
					ctx.SetDepthTestEnable(false);
					ctx.SetFillMode(FILL_WIREFRAME);
					DrawMesh(ctx, vert, 1);
					break;
				}
			}

			col = colour(1,0,0);
			if (topology.type == Topology::POINTLIST || topology.type == Topology::PATCH)
				SetThickPoint(ctx, 8);
			else
				Set(ctx, vb_matrix ? blend_idx : blend4);

			SetThickPoint(ctx, 8);
			ctx.SetBlendEnable(true);
			ctx.SetFillMode(FILL_SOLID);
			ctx.DrawPrimitive(PRIM_POINTLIST, ib ? ib.Data()[vert + off] : vert + off, 1);
		}

	}

	ctx.SetBlendEnable(false);
	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetFillMode(FILL_SOLID);
	ctx.SetDepthTestEnable(false);

	if (flags.test(FRUSTUM_EDGES)) {
		worldviewproj = viewProj * world * translate(zero, zero, (one + viewport.y.z) * half) * scale(one, one, viewport.x.z * half);
		col = colour(0.5f, 0.5f, 0.5f);
		Set(ctx, blend4);
		DrawFrustum(ctx);
	}

	if (flags.test(SCISSOR)) {
		worldviewproj	= viewProj * world * ((scissor - viewport.y) / viewport.x).matrix();
		col = colour(1,1,0);
		Set(ctx, blend4);
		DrawFrustum(ctx);
	}

	if (flags.test(SCREEN_RECT)) {
		rectangle	r(zero, zero, screen_size.x, screen_size.y);
		r = (r - viewport.y.xy) / viewport.x.xy;
		worldviewproj	= viewProj * world * translate(0,0,1) * float3x4(r.matrix());
		col = colour(1,1,1);
		Set(ctx, blend4);
		DrawRect(ctx);
	}

	if (flags.test(BOUNDING_EDGES)) {
		worldviewproj	= viewProj * world * extent.matrix();
		col				= colour(0,1,0);

		Set(ctx, blend4);
		ImmediateStream<float3p>	ims(ctx, PRIM_LINELIST, 24);
		float3p	*p	= ims.begin();
		for (int i = 0; i < 12; i++, p += 2) {
			int	ix = i / 4, iy = (ix + 1) % 3, iz = (ix + 2) % 3;
			p[0][ix] = p[1][ix] = (i & 1) * 2 - 1;
			p[0][iy] = p[1][iy] = (i & 2) - 1;
			p[0][iz] = -1;
			p[1][iz] = +1;
		}
	}

	ctx.SetWindow(axes.window(GetClientRect()));
	proj			= hardware_fix(axes.proj());
	viewProj		= proj * view_loc.rot;
	iview			= float3x3((quaternion)inverse(view_loc.rot));
	flip_normals	= 1;
	shadowlight_dir	= inverse(view_loc.rot) * float3(0,0,-1);

	axes.draw(ctx, mode == PERSPECTIVE ? float3x3(world) : identity);

	graphics.EndScene(ctx);
	Present();
}

void MeshWindow::Init(const Topology _topology, int _num_verts, param(float3x2) _viewport, param(cuboid) &_extent, MODE _mode) {
	mode		= _mode;
	topology	= _topology;
	num_verts	= _num_verts;
	num_prims	= topology.p2v.verts_to_prims(num_verts);
	viewport	= _viewport;
	extent		= _extent;

	if (mode == PERSPECTIVE) {
		float size	= len(extent.extent());
		move_scale	= size;
		view_loc.trans = float3(zero, zero, size);
	} else {
		move_scale	= len(viewport.x);
	}
	toolbar.CheckButton(ID_MESH_BACKFACE, cull == BFC_FRONT);
	toolbar.CheckButton(ID_MESH_BOUNDS, flags.test(BOUNDING_EDGES));
	toolbar.CheckButton(ID_MESH_FILL, flags.test(FILL));
}

MeshWindow::MeshWindow(const WindowPos &wpos, ID id)
	: axes(100, 0)
	, prevbutt(0), mode(SCREEN), move_scale(1), zoom(0.8f), zscale(1024), light_rot(identity)
	, select(-1), flags(0), cull(BFC_BACK)
	, scissor(unit_cube)
	, fixed_proj(identity)
{
	Create(wpos, "Mesh", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, id);
}

//-----------------------------------------------------------------------------
//	MeshVertexWindow
//-----------------------------------------------------------------------------

LRESULT MeshVertexWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			GetPane(0).SetFocus();
			break;

		case WM_SETFOCUS:
			GetPane(0).SetFocus();
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case LVN_ITEMCHANGED: {
					NMLISTVIEW	*nmlv = (NMLISTVIEW*)nmh;
					if (nmlv->uNewState & LVIS_SELECTED)
						MeshWindow::Cast(GetPane(1))->SetSelection(nmlv->iItem, false);
					return 0;
				}
				case NM_CUSTOMDRAW: {
					NMCUSTOMDRAW *nmcd = (NMCUSTOMDRAW*)nmh;
					switch (nmcd->dwDrawStage) {
						case CDDS_PREPAINT:
							return CDRF_NOTIFYITEMDRAW;

						case CDDS_ITEMPREPAINT: {
							static const COLORREF cols[] = { RGB(255,255,255), RGB(224,224,224) };
							NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmh;
							MeshWindow		*mw		= MeshWindow::Cast(GetPane(1));
							int				i		= nmcd->dwItemSpec;
							nmlvcd->clrTextBk		= cols[mw->PrimFromVertex(i, false) & 1];
							return mw->IsChunkStart(i + 1) ? CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT : CDRF_NEWFONT;
						}

						case CDDS_ITEMPOSTPAINT: {
							ListViewControl	lv(nmcd->hdr.hwndFrom);
							DeviceContext	dc(nmcd->hdc);
							Rect			rect	= lv.GetItemRect(nmcd->dwItemSpec);//(nmcd->rc);
							dc.Fill(rect.Subbox(0, -2, 0, 0), win::Colour(0,0,0));
							return CDRF_DODEFAULT;
						}
					}
					break;
				}

				case MeshNotification::SET:
					return GetPane(0).SendMessage(message, wParam, lParam);
			}
			break;
		}
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

MeshVertexWindow::MeshVertexWindow(const WindowPos &wpos, const char *title, ID id) : SplitterWindow(SWF_VERT | SWF_PROP) {
	Create(wpos, title, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0, id);
	Rebind(this);
}
