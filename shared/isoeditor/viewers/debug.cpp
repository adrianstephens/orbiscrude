#include "debug.h"
#include "viewer_identifier.h"
#include "windows/common.rc.h"
#include "filename.h"
#include "resource.h"
#include "main.h"

using namespace app;

//-----------------------------------------------------------------------------
//	Source/Disassembly
//-----------------------------------------------------------------------------

void app::SetSourceWindow(win::RichEditControl &text, const SyntaxColourer &colourer, const memory_block &s, int *active, size_t num_active) {
	if (s) {
		text.SetFont(colourer.font);
		text.SendMessage(EM_EXLIMITTEXT, 0, ~0);
		text.SetFont(colourer.font);
		memory_reader	r(s);
		text.StreamIn(SF_TEXT, [&r](void *p, uint32 n) { return r.readbuff(p, n); });
		colourer.Process(text, active, num_active);
	}
}
EditControl app::MakeSourceWindow(const WindowPos &wpos, const char *title, const SyntaxColourer &colourer, const memory_block &s, int *active, size_t num_active, Control::Style style, Control::StyleEx styleEx) {
	if (s) {
#if 0
		TextWindow *tw = new TextWindow(wpos, title, style, styleEx);
		tw->SendMessage(EM_EXLIMITTEXT, 0, ~0);
		tw->SetFont(colourer.font);
		tw->SetText2(colourer.RTF(s, active, num_active));
		return *tw;
#else
		D2DTextWindow *tw = new D2DTextWindow(wpos, title, style | Control::CHILD, styleEx);
		SetSourceWindow(*tw, colourer, s, active, num_active);
		return *tw;
#endif
	}
	return EditControl();
}

void app::ShowSourceLine(EditControl edit, uint32 line) {
	edit.SetSelection(edit.GetLineRange(line - 1));
	edit.EnsureVisible();
}

void app::ShowSourceTabLine(TabControl2 tabs, uint32 file, uint32 line) {
	int	i = tabs.FindControlByID(file);
	if (i >= 0) {
		tabs.SetSelectedIndex(i);
		EditControl edit	= tabs.GetItemControl(i);
		ShowSourceLine(edit, line);
	}
}
void app::ShowSourceTabLine(TabControl2 tabs, const Disassembler::Location *loc) {
	if (loc)
		ShowSourceTabLine(tabs, loc->file, loc->line);
}

void app::DumpDisassemble(win::RichEditControl &text, Disassembler::State *state, int flags, Disassembler::SymbolFinder sym_finder) {
	string_builder	a;
	for (int i = 0, n = state->Count(); i < n; i++) {
		state->GetLine(a, i, flags, sym_finder);
		a << "\r\n";
	}
	text.SetText(a);
}

template<typename L> void DumpDisassemble(win::RichEditControl &text, Disassembler::State *state, int flags, Disassembler::SymbolFinder sym_finder, uint64 base, L locations, const Disassembler::Files &files, const SyntaxColourer &colourer) {
	text.SetFont(colourer.font);
	text.SetText(0);

	auto loc = locations.begin(), loc_end = locations.end();

	for (int i = 0, n = state->Count(); i < n; i++) {
		uint32	offset	= uint32(state->GetAddress(i) - base);
		while (loc != loc_end && loc->offset <= offset) {
			if (flags & Disassembler::SHOW_LINENOS) {
				uint32	offset = text.GetTextLength();
				text.SetSelection(CharRange(offset, offset));
				text.ReplaceSelection(format_string("%8i:", loc->line));
				text.SetSelection(CharRange(offset, offset+8));
				text.SetFormat(win::CharFormat().Colour(0, 0, 0).Size(10));
			}

			if (const char *start = files.get_line(*loc)) {
				const char	*end = strchr(start, '\n');
				if (!end)
					end = string_end(start);

				uint32		len		= text.GetTextLength();
				const char	*offset	= start - len;

				text.SetSelection(CharRange::end());
				text.ReplaceSelection(str(start, end) + "\n");
				text.SetSelection(CharRange(len).to_end());
				text.SetFormat(win::CharFormat().Colour(0, 0, 0));

				string_scan s(start, end);
				while (s.remaining()) {
					auto type = colourer.GetNext(s, &start);

					text.SetSelection(CharRange(start - offset, s.getp() - offset));
					text.SetFormat(win::CharFormat().Colour(colourer.Colour(type)));
				}
			} else {
				text.SetSelection(CharRange::end());
				text.ReplaceSelection("missing source\n");
				//text.SetSelection(CharRange(len).to_end());
				text.SetFormat(win::CharFormat().Colour(0, 0, 0));
			}
			++loc;
		}
		string_builder	a;
		state->GetLine(a, i, flags, sym_finder);
		a << '\n';
		text.SetSelection(CharRange::end());
//		text.SetFormat(win::CharFormat().Colour(colourer.Colour(SyntaxColourer::USER, true)));
		text.SetFormat(win::CharFormat().Colour(128,128,128));
		text.ReplaceSelection(a);
	}
}

//-----------------------------------------------------------------------------
//	CodeHelper
//-----------------------------------------------------------------------------

void CodeHelper::FixLocations(uint64 _base) {
	base		= _base;
	loc_indices = int_range(locations.size32());
	auto	indexed = make_indexed_container(locations, loc_indices);
	sort(indexed);
	indexed.erase(unique(indexed.begin(), indexed.end()), indexed.end());
}

void CodeHelper::SetDisassembly(RichEditControl c, Disassembler::State *_state) {
	state		= _state;
	DumpDisassemble(c, state, mode, sym_finder);
}

void CodeHelper::SetDisassembly(RichEditControl c, Disassembler::State *_state, const Disassembler::Files &files) {
	state		= _state;

	if (state && state->Count()) {
		if (!(mode & Disassembler::SHOW_SOURCE)) {
			c.SetFont(colourer.font);
			return DumpDisassemble(c, state, mode, sym_finder);
		}

		auto indexed = make_indexed_container(locations, make_const(loc_indices));
		loc_indices_combine.clear();

		auto	i	= lower_boundc(indexed, state->GetAddress(0) - base);
		uint64	end	= state->GetAddress(state->Count() - 1) - base;

		while (i != indexed.end() && i->offset < end) {
			if (i == indexed.begin() || !state->IgnoreLoc(i, base, !(mode & Disassembler::SHOW_ALLSOURCE)))
				loc_indices_combine.push_back(i.index());
			++i;
		}
		::DumpDisassemble(c, state, mode, sym_finder, base, make_indexed_container(locations, make_const(loc_indices_combine)), files, colourer);
	}
}

void CodeHelper::Select(EditControl click, EditControl dis, TabControl2 *tabs) {
	int			line	= click.GetLine(click.GetSelection().cpMin);
	ID			id		= click.id;

	switch (id) {
		case 0:
			if (mode & Disassembler::SHOW_SOURCE) {
				if (auto loc = LineToSource(line))
					ShowSourceTabLine(*tabs, loc->file, loc->line);
			}
			break;

		default: {
			ShowCode(dis, id, line);
			break;
		}
	}
}

void CodeHelper::SourceTabs(TabControl2 &tabs, const Disassembler::Files &files) {
	for (auto i : with_iterator(files)) {
		EditControl c = MakeSourceWindow(tabs.GetChildWindowPos(), filename(i->name).name_ext(), colourer, i->source, 0, 0, EditControl::READONLY);
		c.id = i.hash();
		tabs.AddItemControl(c);
	}
}

//-----------------------------------------------------------------------------
//	DebugWindow
//-----------------------------------------------------------------------------

Accelerator DebugWindow::GetAccelerator() {
	static const ACCEL a[] = {
		{FVIRTKEY | FNOINVERT,				VK_F9,		ID_DEBUG_BREAKPOINT,	},
		{FVIRTKEY | FNOINVERT,				VK_F10,		ID_DEBUG_STEPOVER,		},
		{FVIRTKEY | FSHIFT | FNOINVERT,		VK_F10,		ID_DEBUG_STEPBACK,		},
		{FVIRTKEY | FNOINVERT,				VK_F5,		ID_DEBUG_RUN,			},
		{FVIRTKEY | FCONTROL | FNOINVERT,	'C',		ID_EDIT_COPY,			},
		{FVIRTKEY | FCONTROL | FNOINVERT,	'V',		ID_EDIT_PASTE,			},
		{FVIRTKEY | FCONTROL | FNOINVERT,	'F',		ID_EDIT_FIND,			},
		{FVIRTKEY | FNOINVERT,				VK_F3,		ID_EDIT_FINDNEXT,		},
		{FVIRTKEY | FSHIFT | FNOINVERT,		VK_F3,		ID_EDIT_FINDPREV,		},
		{FVIRTKEY | FCONTROL | FNOINVERT,	'A',		ID_EDIT_SELECT_ALL,		},
	};
	return Accelerator(a);
}

void DebugWindow::SetMode(int new_mode) {
	dynamic_array<uint64>	bp_addr = transformc(bp, [this](uint32 line) { return LineToAddress(line); });
	uint64					pc_addr = LineToAddress(pc_line);

	mode = new_mode;
	CodeHelper::SetDisassembly(*this, state, files);

	transform(bp_addr, bp, [this](uint64 addr) { return AddressToLine(addr); });
	pc_line = AddressToLine(pc_addr);
}

DebugWindow::DebugWindow(const SyntaxColourerRE &colourer, Disassembler::AsyncSymbolFinder &&sym_finder, int mode)
	: CodeHelper(colourer, move(sym_finder), mode)
	, pc_line(0)
	, orig_mode(mode)
	, images(ID("IDB_IMAGELIST_DEBUG"), 16, LR_CREATEDIBSECTION)
{}

LRESULT DebugWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			return 0;

		case WM_SIZE: {
			Point	pt(lParam);
			SetEditRect(Rect(16, 0, pt.x - 16, pt.y));
			break;
		}

		case WM_LBUTTONDOWN: {
			Point	pt(lParam);
			if (Margin().Contains(pt)) {
				SetSelection(GetChar(pt));
				Parent()(WM_COMMAND, ID_DEBUG_BREAKPOINT);
			} else {
				Super(message, wParam, lParam);
			}
			Parent()(WM_COMMAND, MAKEWPARAM(id.get(), EN_SETFOCUS), (LPARAM)hWnd);
			return 0;
		}

		case WM_CONTEXTMENU: {
			Menu	menu = Menu(IDR_MENU_SUBMENUS).GetSubMenuByName("Text");
			Menu::Item("Show Source", ID_DEBUG_SHOWSOURCE).State(mode & Disassembler::SHOW_SOURCE ? MFS_CHECKED : 0).InsertByPos(menu, 0);
			if (mode & Disassembler::SHOW_SOURCE)
				Menu::Item("Combine Source", ID_DEBUG_COMBINESOURCE).State(mode & Disassembler::SHOW_ALLSOURCE ? 0 : MFS_CHECKED).InsertByPos(menu, 1);
			Menu::Item::Separator().InsertByPos(menu, 2);
			menu.Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
			return 0;
		}

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_DEBUG_SHOWSOURCE:
					SetMode(mode ^ Disassembler::SHOW_SOURCE);
					Invalidate();
					return 1;

				case ID_DEBUG_COMBINESOURCE:
					SetMode(mode ^ Disassembler::SHOW_ALLSOURCE);
					Invalidate();
					return 1;
			}
			break;

		case WM_PAINT: {
			Rect	rect;
			GetUpdateRect(*this, &rect, FALSE);
			D2DTextWindow::Proc(message, wParam, lParam);
			DeviceContext	dc(*this);

			rect	= Margin();
			dc.Fill(rect, COLOR_WINDOW);
			int		first_line	= GetLine(GetChar(rect.TopLeft()));
			int		last_line	= GetLine(GetChar(rect.BottomRight()));
			auto	bpi			= lower_boundc(bp, first_line), bpe = bp.end();

			for (int i = first_line; i <= last_line; i++) {
				Point	pos = GetCharPos(GetLineStart(i));
				if (bpi != bpe && *bpi == i) {
					dc.DrawImage(images, 1, 0, pos.y);
					++bpi;
				}
				if (i == pc_line) {
					dc.SetBackground(Colour::none);
					dc.DrawImage(images, 2, 0, pos.y);
				}

			}
			return 0;
		}
		case WM_NCDESTROY:
			return Super(message, wParam, lParam);

	}
	return D2DTextWindow::Proc(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	RegisterWindow
//-----------------------------------------------------------------------------

string_accum& RegisterWindow::Entry::GetValue(string_accum &a, int col, uint32 *val) const {
	switch (col) {
		case 0:
			return a << name;

		case 1:
			if (flags & UNSET)
				return a <<  "-";

			switch (flags & SIZE_MASK) {
				case SIZE64:			return a.format("0x%016I64x", *(uint64*)val);
				case SIZE128:			return a.format("0x%08x-0x%08x-0x%08x-0x%08x", val[0], val[1], val[2], val[3]);
				default:				return a.format("0x%08x", *val);
			}

		case 2:
			if (fields)
				return PutFields(a, IDFMT_CAMEL | IDFMT_NOSPACES, fields, val, 0, ", ");
			return a << "-";

		case 3:
			return a << type;

		default:
			return a;
	}
}

LRESULT RegisterWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			return 0;

		case WM_ISO_NEWPANE: {
			ListViewControl	lv;
			lv.Create(GetChildWindowPos(), 0, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA);
			lv.SetExtendedStyle(ListViewControl::DOUBLEBUFFER | ListViewControl::FULLROWSELECT | ListViewControl::GRIDLINES | ListViewControl::ONECLICKACTIVATE | ListViewControl::UNDERLINEHOT | ListViewControl::AUTOSIZECOLUMNS);
			lv.id = 'RG';
			Init(lv);
			return (LRESULT)lv.hWnd;
		}

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case LVN_HOTTRACK: {
					NMLISTVIEW		*nmlv = (NMLISTVIEW*)lParam;
					if (nmlv->iItem	!= -1 && !(entries[nmlv->iItem].flags & Entry::LINK_MASK))
						nmlv->iItem	= -1;
					break;
				}

				case LVN_GETDISPINFO: {
					Item			&i		= (Item&)((NMLVDISPINFO*)nmh)->item;
					if (i.mask & LVIF_TEXT) {
						Entry	&e		= entries[i.Index()];
						uint32	*val	= prev_regs + e.offset;
						e.GetValue(lvalue(fixed_accum(i.TextBuffer())), i.iSubItem, val);
						return 1;
					}
					break;
				}

				case NM_CUSTOMDRAW:
					if (nmh->idFrom == 'RG') {
						NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmh;

						switch (nmlvcd->nmcd.dwDrawStage) {
							case CDDS_PREPAINT:
								return CDRF_NOTIFYITEMDRAW;

							case CDDS_ITEMPREPAINT: {
								uint32	flags = entries[nmlvcd->nmcd.dwItemSpec].flags;
								if (flags & Entry::DISABLED)
									nmlvcd->clrText	= win::Colour(128,128,128);
								else if (flags & Entry::CHANGED)
									nmlvcd->clrText	= win::Colour(192,0,0);
								else switch (flags & Entry::LINK_MASK) {
									case Entry::BUFFER:		nmlvcd->clrText	= win::Colour(0,192,0); break;
									case Entry::TEXTURE:	nmlvcd->clrText	= win::Colour(0,0,192); break;
									case Entry::PTR:		nmlvcd->clrText	= win::Colour(0,192,192); break;
									default: break;
								}
								return CDRF_DODEFAULT;
							}

						}
					}
					break;

//				case LVN_ITEMACTIVATE: {
//					NMITEMACTIVATE	*nma	= (NMITEMACTIVATE*)lParam;
//					Entry	&e		= entries[nma->iItem];
//					nma->lParam		= (LPARAM)&e;
//					if (e.flags & Entry::LINK_MASK) {
//						nma->lParam		= (LPARAM)(void*)(prev_regs + e.offset);
//						nma->iSubItem	= (e.flags & Entry::LINK_MASK) / Entry::LINK;
//					}
//				}
				default:
					return Parent()(message, wParam, lParam);
			}
			break;
		}
		case WM_NCDESTROY:
			delete this;
			return 0;
	}

	return Super(message, wParam, lParam);
}

void RegisterWindow::Init(ListViewControl lv) {
	uint32	width		= GetClientRect().Width();
	uint32	value_width = max(width / 4, width - 500);
	uint32	other_width	= (width - value_width) / 5;
	lv.AddColumns(
		"name",				other_width,
		"hex",				other_width * 3,
		"float/deduced",	value_width,
		"type",				other_width
	);
	lv.SetCount(entries.size32());
}

//-----------------------------------------------------------------------------
//	LocalsWindow
//-----------------------------------------------------------------------------

void FrameData::_add_block(uint32 offset, uint64 addr, uint32 size, bool remote) {
	uint32	end		= offset + size;
	uint64	adelta	= addr - offset;
	auto	b		= upper_boundc(blocks, offset);

	if (b != blocks.begin()) {
		--b;
		offset	= max(offset, b->offset + b->size);
	}
	while (b != blocks.end()) {
		if (offset < b->offset)
			b = blocks.emplace(b, adelta + offset, offset, min(end, b->offset) - offset, remote);

		uint32	bend	= b->end();

		if (b->addr - b->offset == adelta && b->remote == remote) {
			if (offset < b->offset) {
				b->offset	= offset;
				b->addr		= adelta + offset;
				b->size		= bend - offset;
			}
			if (end > bend) {
				bend		= b + 1 == blocks.end() ? end : min(end, b[1].offset);
				b->size		= bend - b->offset;
			}
		}

		offset		= bend;
		if (offset >= end)
			return;

		++b;
//		if (b != blocks.end())
//			b = blocks.emplace(b, adelta + offset, offset, min(end, b->offset) - offset, remote);
	}
	b = blocks.emplace(b, adelta + offset, offset, end - offset, remote);
}

bool FrameData::read(void *buffer, size_t size, uint32 offset, memory_interface *mem) const {
	uint32	end		= offset + uint32(size);
	auto	b		= upper_boundc(blocks, offset);
	if (b == blocks.begin())
		return false;

	for (--b; b != blocks.end() && offset < end; ++b) {
		if (!between(offset, b->offset, b->end()))
			return false;

		uint64	addr2	= offset - b->offset + b->addr;
		uint32	size2	= min(end, b->end()) - offset;
		if (!b->remote) {
			memcpy(buffer, (void*)addr2, size2);
		} else {
			if (!mem->get(buffer, size2, addr2))
				return false;
		}
		buffer = (uint8*)buffer + size2;
		offset += size2;
	}
	return offset == end;
}
/*
template<typename C> uint64 GetStringLength(memory_interface *mem, uint64 addr) {
	uint64	len;
	uint64	addr1	= addr;

	do
		len = string_len(make_range<C>(mem->get(addr1, 0x100 * sizeof(C))));
	while (len == 0x100);

	return len + (addr1 - addr);
}


uint64 GetDataSize(const C_type *type, uint64 addr, memory_interface *mem) {
	if (!type)
		return 0;

	if (type->type == C_type::INT && type->flags & C_type_int::CHAR) {
		switch (type->size()) {
			case 1:	return (GetStringLength<char>(mem, addr) + 1);
			case 2:	return (GetStringLength<char16>(mem, addr) + 1) * 2;
			case 4:	return (GetStringLength<char32>(mem, addr) + 1) * 4;
		}
	}

	return type->size();
}
*/
uint64 GetAddress(ast::node *node0, ast::get_variable_t get_var, memory_interface *mem, uint64 *size) {
	auto	node	= const_fold(node0, get_var, mem);
	if (node->kind == ast::literal && node->flags & ast::ADDRESS) {
		uint64	addr = node->cast<ast::lit_node>()->v;
		if (size)
			*size = node->type ? node->type->size() : 0;//::GetDataSize(node->type, addr, mem);
		return addr;
	}
	return 0;
}

string app::Description(ast::node *node) {
	string_builder	b;
	Dumper(0).DumpCPP(b, node);
	return b;
}

//-----------------------------------------------------------------------------
//	LocalsWindow
//-----------------------------------------------------------------------------

uint64 LocalsWindow::GetAddress(ast::node *node, uint64 *size)	const {
	return ::GetAddress(node, get_var, mem, size);
}

void Expand(TreeControl tc, HTREEITEM h, NATVIS *natvis, ast::get_memory_t get_mem, ast::get_variable_t get_var) {
	ast::node	*node	= tc.GetItemParam(h);
	auto		exp		= Expand(node, natvis, get_mem, get_var);

	for (auto &i : exp)
		TreeControl::Item(i.id).BothImages(I_IMAGECALLBACK).Children(HasChildren(i.ast, natvis, none, get_mem)).Param(i.ast->addref()).Insert(tc, h);
}

void UpdateTree(TreeControl tc, HTREEITEM h, NATVIS *natvis, ast::get_memory_t get_mem, ast::get_variable_t get_var) {
	auto		item		= tc.GetItem(h, TVIF_PARAM|TVIF_STATE|TVIF_HANDLE);
	ast::node	*node		= item.Param();
	auto		exp			= Expand(node, natvis, get_mem, get_var);
	bool		children	= !exp.empty();

	item.Children(children);

	if (!children || !(item.state & TVIS_EXPANDED)) {
		item.ClearState(TVIS_EXPANDED|TVIS_EXPANDEDONCE).Set(tc);
		tc.DeleteChildren(h);
		return;
	}

	auto	items	= tc.ChildItems(h);
	auto	hi		= items.begin();
	for (auto &i : exp) {
		HTREEITEM		h2	= *hi++;
		if (!h2)
			h2 = TreeControl::Item(i.id).BothImages(I_IMAGECALLBACK).Param(i.ast->addref()).Insert(tc, h);
		else
			tc.SetItemParam(h2, i.ast->addref());
		UpdateTree(tc, h2, natvis, get_mem, get_var);
	}

	while (HTREEITEM h2 = *hi++)
		tc.DeleteItem(h2);
}

bool CheckBP(ast::node *node, const hash_set<uint64> &bps) {
	return node->kind == ast::literal && node->flags & ast::ADDRESS && bps.count(node->cast<ast::lit_node>()->v);
}

void DumpType(string_accum &sa, ast::node *node, const C_types &types) {
	if (node->type) {
		if (const char *type_name = types.get_name(node->type))
			sa << type_name;
		else
			DumpType(sa, node->type, 0, 0);
	}
}

DWORD BeginDrag(TreeColumnControl tc, HTREEITEM h, const Point &pt) {
	DWORD			effect = 0;
	if (ast::node *ast = tc.GetItemParam(h)) {
		ImageList		il		= tc.CreateDragImage(h);
		Rect			rc		= tc.GetItemRect(h, -1, true);

		il.DragBegin(0, pt - rc.TopLeft());

		DropSource		*drop	= new DropSource(MakeGlobal(substitute_params(ast)), CF_AST);
		HRESULT			hr		= DoDragDrop(drop, drop, DROPEFFECT_LINK | DROPEFFECT_COPY, &effect);
		drop->Release();

		il.Destroy();
	}
	return effect;
}

LRESULT LocalsWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			tc.Create(GetChildWindowPos(), 0, CHILD | VISIBLE | CLIPSIBLINGS | TCS_GRIDLINES | TCS_HEADERAUTOSIZE);
			tc.GetTreeControl().style = CHILD | VISIBLE | CLIPSIBLINGS | TVS_NOHSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT;
			tc.GetTreeControl().SetImageList(ImageList(ID("IDB_IMAGELIST_DEBUG"), 16, LR_CREATEDIBSECTION));

			HeaderControl	header	= tc.GetHeaderControl();
			header.style = CHILD | VISIBLE | HDS_FULLDRAG;
			HeaderControl::Item("Name").	Format(HDF_LEFT).Width(250).Insert(header);
			HeaderControl::Item("Value").	Format(HDF_LEFT).Width(100).Insert(header);
			HeaderControl::Item("Type").	Format(HDF_LEFT|HDF_FIXEDWIDTH).Width(header.Remaining()).Insert(header);
			return 0;
		}

		case WM_SIZE:
			tc.Resize((Point)lParam);
			return  0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TVN_DELETEITEM: {
					NMTREEVIEW	*nmtv = (NMTREEVIEW*)nmh;
					((ast::node*)nmtv->itemOld.lParam)->release();
					return 0;
				}
				case TVN_ITEMEXPANDING: {
					NMTREEVIEW	*nmtv = (NMTREEVIEW*)nmh;
					if (nmtv->action == TVE_EXPAND && !(nmtv->itemNew.state & TVIS_EXPANDEDONCE))
						Expand(tc, nmtv->itemNew.hItem, natvis, mem, get_var);
					return 0;
				}
				case TCN_GETDISPINFO: {
					NMTCCDISPINFO	*nmdi	= (NMTCCDISPINFO*)nmh;
					ast::noderef	node0	= tc.GetItemParam(nmdi->item.hItem);
					auto			node	= const_fold(node0, none, mem);
					node0->type = node->type;

					if (nmdi->item.mask & TVIF_IMAGE) {
						nmdi->item.iImage = nmdi->item.iSelectedImage = CheckBP(node, bps) ? 1 : 0;
						return 0;
					}

					fixed_accum		sa(nmdi->item.pszText, nmdi->item.cchTextMax);
					switch (nmdi->iSubItem) {
						case 1:
							return DumpValue(sa, node, FORMAT::SHORTEST, natvis, mem, get_var);
						case 2:
							DumpType(sa, node, types);
							return 1;
					}
					return 0;
				}
				case TVN_BEGINDRAG: {
					NMTREEVIEW		*nmtv	= (NMTREEVIEW*)nmh;
					BeginDrag(tc, nmtv->itemNew.hItem, nmtv->ptDrag);
					return 0;
				}

				default:
					return Parent()(message, wParam, lParam);
			}
			break;
		}

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_EDIT_COPY:
					ast::node	*ast	= tc.GetItemParam(tc.GetSelectedItem());
					Clipboard(*this).Set(CF_AST, substitute_params(ast));
					return 1;
			}
			break;

		case WM_NCDESTROY:
			delete this;
			return 0;
	}

	return Super(message, wParam, lParam);
}

void LocalsWindow::AppendEntry(string_param &&id, const C_type *type, uint64 addr, bool local) {
	TreeControl::Item(id).BothImages(I_IMAGECALLBACK).Children(HasChildren(type, natvis)).Param((new ast::lit_node(id, type, addr, ast::ADDRESS | (local ? ast::LOCALMEM : 0)))->addref()).Insert(tc, TVI_ROOT);
}

//-----------------------------------------------------------------------------
//	WatchWindow
//-----------------------------------------------------------------------------

uint64 WatchWindow::GetAddress(ast::node *node, uint64 *size) const {
	return ::GetAddress(node, get_var, mem, size);
}

void WatchWindow::Redraw() {
	auto	tree	= tc.GetTreeControl();
	auto	items	= tree.ChildItems();

	for (auto h : items) {
		if (ast::node *n0 = tree.GetItemParam(h)) {
			auto	type0	= n0->type;
			auto	n1		= const_fold(n0, get_var, mem);
			auto	type1	= n1->type;

			if (type0 != type1) {
				n0->type = type1;
				tree.DeleteChildren(h);
				tree.GetItem(h).Children(HasChildren(type1, natvis)).Set(tree);

			} else {
				UpdateTree(tree, h, natvis, mem, get_var);
			}
		}
	}
	tc.Invalidate();
}

bool WatchWindow::Drop(const Point &pt, uint32 effect, IDataObject* data) {
	if (auto ast = (ast::noderef*)has_format(data, CF_AST)) {
		AppendEntry(Description(*ast), *ast);
		return true;
	}
	if (auto text = (const char*)has_format(data, CF_TEXT)) {
		AppendEntry(text, ReadCExpression(lvalue(memory_reader(text)), types, nullptr, none));
		return true;
	}
	return false;
}

void WatchWindow::AppendEntry(string_param &&s, ast::node *node) {
	TreeControl::Item(s).BothImages(I_IMAGECALLBACK).Children(HasChildren(node, natvis, get_var, mem)).Param(node->addref()).InsertBefore(tc, TVI_ROOT, tc.GetTreeControl().ChildItems().back());
}

void WatchWindow::RemoveEntry(HTREEITEM h, bool backwards) {
	auto		tree	= tc.GetTreeControl();
	int			i		= tree.GetItemParam(h);
	HTREEITEM	hn		= tree.GetNextItem(h);

	if (backwards) {
		if (HTREEITEM h2 = tree.GetPrevItem(h))
			tree.SetSelectedItem(h2);
	}
	tree.DeleteItem(h);
}

LRESULT WatchWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			tc.Create(GetChildWindowPos(), 0, CHILD | VISIBLE | CLIPSIBLINGS | TCS_GRIDLINES | TCS_HEADERAUTOSIZE);
			auto	tree	= tc.GetTreeControl();
			tree.style = CHILD | VISIBLE | CLIPSIBLINGS | TVS_NOHSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT;
			tree.SetImageList(ImageList(ID("IDB_IMAGELIST_DEBUG"), 16, LR_CREATEDIBSECTION));

			HeaderControl	header	= tc.GetHeaderControl();
			header.style = CHILD | VISIBLE | HDS_FULLDRAG;
			HeaderControl::Item("Name").	Format(HDF_LEFT).Width(250).Param(0).Insert(header, 0);
			HeaderControl::Item("Value").	Format(HDF_LEFT).Width(100).Param(0).Insert(header, 1);
			HeaderControl::Item("Type").	Format(HDF_LEFT|HDF_FIXEDWIDTH).Width(header.Remaining()).Param(0).Insert(header, 2);

			TreeControl::Item("").Param(0).Insert(tree);
			return 0;
		}

		case WM_SIZE:
			tc.Resize((Point)lParam);
			return  0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TVN_DELETEITEM: {
					NMTREEVIEW	*nmtv = (NMTREEVIEW*)nmh;
					if (auto n = (ast::node*)nmtv->itemOld.lParam)
						n->release();
					return 0;
				}
				case TVN_ITEMEXPANDING: {
					NMTREEVIEW	*nmtv = (NMTREEVIEW*)nmh;
					if (nmtv->action == TVE_EXPAND && !(nmtv->itemNew.state & TVIS_EXPANDEDONCE))
						Expand(tc, nmtv->itemNew.hItem, natvis, mem, get_var);
					return 0;
				}
				case TCN_GETDISPINFO: {
					NMTCCDISPINFO	*nmdi	= (NMTCCDISPINFO*)nmh;
					if (auto node = const_fold(tc.GetItemParam(nmdi->item.hItem), get_var, mem)) {
						if (nmdi->item.mask & TVIF_IMAGE) {
							nmdi->item.iImage = nmdi->item.iSelectedImage = CheckBP(node, bps) ? 1 : 0;
							return 0;
						}

						fixed_accum		sa(nmdi->item.pszText, nmdi->item.cchTextMax);
						switch (nmdi->iSubItem) {
							case 1:
								return DumpValue(sa, node, FORMAT::SHORTEST, natvis, mem, get_var);
							case 2:
								DumpType(sa, node, types);
								return 1;
						}
					}
					return 0;
				}
				case TVN_KEYDOWN: {
					auto	nmtvk = (NMTVKEYDOWN*)nmh;
					edit_control.Destroy();
					switch (nmtvk->wVKey) {
						case VK_BACK:
						case VK_DELETE: {
							HTREEITEM	h		= tc.GetSelectedItem();
							bool		back	= nmtvk->wVKey == VK_BACK;
							if (!tc.GetParentItem(h)) {
								if (tc.GetItemParam(h)) {
									RemoveEntry(h, back);
								} else if (back) {
									if (h = tc.GetPrevItem(h))
										RemoveEntry(h, true);
								}
							}
							break;
						}
					}
					return 0;
				}

				case TVN_BEGINDRAG:
					if (!edit_control) {
						NMTREEVIEW		*nmtv	= (NMTREEVIEW*)nmh;
						if (BeginDrag(tc, nmtv->itemNew.hItem, nmtv->ptDrag) == DROPEFFECT_MOVE)
							RemoveEntry(nmtv->itemNew.hItem, false);
					}
					return 0;


				default:
					return Parent()(message, wParam, lParam);
			}
			break;
		}

		case WM_CHAR:
			if (wParam == 27)
				edit_control.SetText("");
			SetFocus();
			Parent()(WM_MOUSEACTIVATE);
			break;

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_EDIT:
					switch (HIWORD(wParam)) {
						case EN_KILLFOCUS:
							if (edit_control.GetText().len()) {
								ast::node	*old	= tc.GetItemParam(edit_hitem);
								string		id		= edit_control.GetText();
								auto		ast		= ReadFormattedExpression(id, types, nullptr, none);
								if (!old) {
									AppendEntry(move(id), ast);
								} else {
									tc.GetItem(edit_hitem).Text(id).Param(ast->addref()).Children(HasChildren(ast, natvis, get_var, mem)).ClearState(TVIS_EXPANDED|TVIS_EXPANDEDONCE).Set(tc);
									tc.DeleteChildren(edit_hitem);
									old->release();
								}
							}
							edit_control.Destroy();
							break;
					}
					break;

				case ID_EDIT_COPY:
					if (ast::node *node	= tc.GetItemParam(tc.GetSelectedItem()))
						Clipboard(*this).Set(CF_AST, node->addref());
					return 1;

				case ID_EDIT_PASTE:
					switch (Clipboard::AvailableAny(CF_AST, CF_TEXT)) {
						case CF_AST: {
							Clipboard	clip(hWnd);
							if (auto ast = clip.Get<ast::noderef>(CF_AST))
								AppendEntry(Description(*ast), *ast);
							break;
						}
						case CF_TEXT: {
							Clipboard	clip(hWnd);
							if (auto text = clip.Get<char>(CF_TEXT))
								AppendEntry(&*text, ReadCExpression(lvalue(memory_reader(&*text)), types, nullptr, none));
							break;
						}
					}
					return 1;
			}
			break;

		case WM_MOUSEACTIVATE:
			if (HIWORD(lParam) == WM_LBUTTONDOWN) {
				Point	pt	= GetMousePos();
				if (edit_control && edit_control.GetRect().Contains(pt))
					return MA_ACTIVATE;

				int		subitem;
				uint32	flags;
				HTREEITEM h = tc.HitTest(ToClient(pt), subitem, &flags);
				if (h && subitem == 0) {
					if ((flags & (TVHT_ONITEMLABEL|TVHT_ONITEMRIGHT)) && !tc.GetParentItem(h)) {
						edit_hitem = h;
						win::EditLabel(tc, edit_control, h, 0, ID_EDIT);
						edit_control.SetOwner(*this);
						return MA_NOACTIVATEANDEAT;
					}
				}
				tc.SetSelectedItem(h);
				SetFocus();
			}
			break;

		case WM_NCDESTROY:
			delete this;
			return 0;
	}

	return Super(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	TraceWindow
//-----------------------------------------------------------------------------

LRESULT TraceWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			return 0;

		case WM_SIZE:
			c.Resize(Point(lParam));
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CLICK: {
					c.SetFocus();
					NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
					int	col, item = c.HitTest(nmlv->ptAction, col);
					if (item >= 0 && col >= 2) {
						selected = rows[item].fields[(col - 2) / cols_per_entry].reg;
						c.Invalidate();
					}
					break;
				}

				case NM_CUSTOMDRAW: {
					NMCUSTOMDRAW	*nmcd	= (NMCUSTOMDRAW*)nmh;

					switch (nmcd->dwDrawStage) {
						case CDDS_PREPAINT:
							return CDRF_NOTIFYITEMDRAW;

						case CDDS_ITEMPREPAINT: {
							NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmh;
							if (rows[nmcd->dwItemSpec].fields[0].mask == 0)
								nmlvcd->clrText = RGB(128,128,128);
							return CDRF_NOTIFYSUBITEMDRAW;
						}

						case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
							NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmh;
							int				i		= nmlvcd->iSubItem;
							if (i >= 2) {
								auto	&f = rows[nmcd->dwItemSpec].fields[(i - 2) / cols_per_entry];
								if (f.mask & bit((i - 2) % cols_per_entry)) {
									int		sel			= f.reg == selected ? 128 : 240;
									nmlvcd->clrTextBk	= f.write ? RGB(255,sel,sel) : RGB(sel,255,sel);
								} else {
									nmlvcd->clrTextBk	= RGB(255,255, 255);
								}
							} else {
								nmlvcd->clrTextBk	= i == 1 ? RGB(240,240,255) : RGB(255,255,255);
							}
							break;
						}
					}
					break;
				}
			}
			return Parent()(message, wParam, lParam);
		}
		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;
	}
	return Super(message, wParam, lParam);
}

TraceWindow::TraceWindow(const WindowPos &wpos, int cols_per_entry) : cols_per_entry(cols_per_entry) {
	Create(wpos, 0, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, CLIENTEDGE);
	c.Create(GetChildWindowPos(), 0, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS);
	c.SetExtendedStyle(ListViewControl::GRIDLINES | ListViewControl::DOUBLEBUFFER);// | LVS_EX_FULLROWSELECT);
	c.SetFont(win::Font("Courier New", 11));
	c.AddColumns("address", 100, "instruction", 400);
}
