#include "viewer_identifier.h"

using namespace iso;
using namespace win;
using namespace app;

template<> struct C_types::type_getter<float16> {
	static const C_type *f(C_types &types)	{ return C_type_float::get<float16>(); }
};
template<typename I, I S> struct C_types::type_getter<scaled<I,S> > {
	static const C_type *f(C_types &types)	{ return C_type_int::get<I>(); }
};

C_types	iso::ctypes, iso::user_ctypes;

C_types&	iso::builtin_ctypes() {
	static struct Read_C_types : C_types {
		Read_C_types() {
			win::Resource	r(0, "CTYPES.H", "BIN");
			ReadCTypes(lvalue(memory_reader(r)), *this);

			add("int", C_type_int::get<int>());
			add("float", C_type_float::get<float>());
			add("double", C_type_float::get<double>());
		}
	} types;
	return types;
};

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

string_accum &RegisterList::FillSubItem(string_accum &b, const field *pf, const uint32 *p, uint32 offset) {
	if (format & IDFMT_FIELDNAME)
		PutFieldName(b, format, pf);

	if (pf->offset == field::MODE_CUSTOM) {
		if (pf->shift)
			((field_callback_func)pf->values)(b, pf, p, offset, 0);
		else
			Callback(b, pf, p, offset);

	} else {
		PutConst(b, format, pf, p, offset);
	}
	return b;
}

void RegisterList::FillRow(ListViewControl::Item &item, const field *pf, const uint32 *p, uint32 offset) {
	if (pf) {
		while (!pf->is_terminator()) {
			if (pf->num == 0) {
				switch (pf->offset) {
					case field::MODE_NONE:
						save(format, format | IDFMT_FIELDNAME * !!(format & IDFMT_FIELDNAME_AFTER_UNION)),
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
				FillSubItem(b, pf, p, offset);

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

int app::MakeHeaders(win::ListViewControl lv, int nc, const C_type *type, string_accum &prefix) {
	if (type) switch (type->type) {
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
			for (auto &i : s->elements) {
				nc = MakeHeaders(lv, nc, i.type, prefix << '.' << i.id);
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

int app::MakeHeaders(win::ListViewControl lv, int nc, const SoftType &type, string_accum &prefix) {
	for (int i = 0, n = type.Count(); i < n; i++) {
		char	*startp = prefix.getp();
		win::ListViewControl::Column(type.Name(prefix, i)).Width(75).Insert(lv, nc++);
		prefix.move(startp - prefix.getp());
	}
	return nc;
}

//-----------------------------------------------------------------------------
//	RegisterTree
//-----------------------------------------------------------------------------

void app::StructureHierarchy(RegisterTree &c, HTREEITEM h, const C_types &types, const char *name, const C_type *type, uint32 offset, const void *data, const uint64 *valid) {
	buffer_accum<256>	acc;

	if (const char *type_name = types.get_name(type)) {
		acc << type_name << ' ' << name;
	} else {
		DumpType(acc, type, name, 0);
	}

	if (type && data && (!valid || bits_all(bit_pointer<const uint64>(valid) + offset, bit_pointer<const uint64>(valid) + offset + type->size32(), true)))
		DumpData(acc << " = ", (uint8*)data + offset, type);

	h = c.AddText(h, acc.term());

	if (!type)
		return;

	switch (type->type) {
		case C_type::ARRAY: {
			C_type_array	*a		= (C_type_array*)type;
			uint32			stride	= a->subsize;
			if (const C_type *subtype = a->subtype) {
				for (int i = 0, n = a->count; i < n; i++) {
					StructureHierarchy(c, h, types, format_string("[%i]", i), subtype, offset, data, valid);
					offset += stride;
				}
			}
			return;
		}
		case C_type::TEMPLATE:
		case C_type::STRUCT: {
			C_type_struct	*s = (C_type_struct*)type, *b;
			while ((b = s->get_base()) && b->size() == s->size())
				s = b;

			for (auto *i = s->elements.begin(), *e = s->elements.end(); i != e; ++i)
				StructureHierarchy(c, h, types, i->id, i->type, offset + i->offset, data, valid);
			return;
		}

		default:
			break;
	}
}

//-----------------------------------------------------------------------------
//	ColourList
//-----------------------------------------------------------------------------

win::Colour app::FadeColour(win::Colour c) {
	return win::Colour((c.r + 255) / 2, (c.g + 255) / 2, (c.b + 255) / 2);
}

win::Colour app::MakeColour(int c) {
	return win::Colour(c & 1 ? 255 : 240, c & 2 ? 255 : 240, c & 4 ? 255 : 240);
}

LRESULT	ColourList::CustomDraw(NMCUSTOMDRAW	*nmcd, Control parent) {
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
				dc.Fill(rect, Brush(colour::red));
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

int ColourList::AddColumns(int nc, const C_type *type, string_accum &prefix, win::Colour col0, win::Colour col1) {
	switch (type->type) {
		case C_type::ARRAY: {
			const C_type_array	*a = (const C_type_array*)type;
			char	*startp = prefix.getp();
			for (int i = 0, n = a->count; i < n; i++) {
				nc = AddColumns(nc, a->subtype, prefix << '[' << i << ']', col0, col1);
				prefix.move(startp - prefix.getp());
			}
			break;
		}
		case C_type::STRUCT: {
			const C_type_struct	*s = (const C_type_struct*)type;
			char	*startp = prefix.getp();
			for (auto *i = s->elements.begin(), *e = s->elements.end(); i != e; ++i) {
				nc = AddColumns(nc, i->type, prefix << '.' << i->id, col0, col1);
				prefix.move(startp - prefix.getp());
			}
			break;
		}
		default:
			AddColumn(nc, prefix, 50, nc & 1 ? col1 : col0);
			++nc;
			break;
	}
	return nc;
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

			ToolTipControl	tt(*this, NULL, POPUP |TTS_ALWAYSTIP);
			tt(TTM_ADDTOOLA, 0, (LPARAM)&ti);
			//tt.Attach(*this);
			SetToolTipControl(tt);

			TTTOOLINFOA		ti2;
			clear(ti2);
			ti2.cbSize		= sizeof(TOOLINFO);
			ti2.hwnd			= *this;
			ti2.uId			= (UINT_PTR)ti.hwnd;
			tt(TTM_GETTOOLINFO, 0, (LPARAM)&ti2);
			return 0;
		}
		case WM_MOUSEMOVE: {
			uint32		flags;
			HTREEITEM	hit = HitTest(Point(lParam), &flags);
			uint32		mod	= wParam & MK_SHIFT ? 1 : wParam & MK_CONTROL ? 2 : 0;

			if (!(flags & TVHT_ONITEM) || (hit != hot && cursor_indices[GetItem(hit, TVIF_IMAGE).Image()][mod] == 0))
				hit = 0;

			if (hit != hot) {
				if (hot)
					Invalidate(GetItemRect(hot));
				if (hot = hit)
					Invalidate(GetItemRect(hot));
			}

			if (hot)
				SetCursor(Item(hit).Image(0).Get(*this).Image(), mod);
			break;
		}
//		case WM_COMMAND:
		case WM_CHAR:
			Parent().SendMessage(message, wParam, lParam);
			break;

		case WM_DESTROY:
			ISO_TRACE("Destroy ColourTree\n");
			break;

	}
	return Super(message, wParam, lParam);
}

LRESULT	ColourTree::CustomDraw(NMCUSTOMDRAW	*nmcd) const {
	switch (nmcd->dwDrawStage) {
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT: {
			NMTVCUSTOMDRAW *nmtvcd	= (NMTVCUSTOMDRAW*)nmcd;
			auto	hItem			= (HTREEITEM)nmcd->dwItemSpec;
			auto	item			= GetItem(hItem, TVIF_IMAGE|TVIF_STATE);
			auto&	colour			= colours[item.Image()];

			nmtvcd->clrText = colour.rgb();

			if (hItem == hot || colour.x) {
				DeviceContext	dc(nmcd->hdc);
				auto			font = dc.Current<win::Font>().GetParams();
				dc.Select(win::Font(font.Underline(hItem == hot).Bold(colour.x & 2).Italic(colour.x & 1)));
				return CDRF_NEWFONT;

			} else {
				return CDRF_DODEFAULT;
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
