#include "viewbin.h"
#include "main.h"
#include "viewbin.rc.h"
#include "iso/iso_files.h"

#define	IDR_MENU_BINARY			"IDR_MENU_BINARY"
#define IDR_MENU_BINARYCONTEXT	"IDR_MENU_BINARYCONTEXT"
#define	IDR_ACCELERATOR_BINARY	"IDR_ACCELERATOR_BINARY"

#define WM_ISO_FOUND			WM_USER+1

namespace app {

COLORREF	custom_colours[16];

//-----------------------------------------------------------------------------
//	ViewBin
//-----------------------------------------------------------------------------

void ViewBin::Check(uint32 id, bool check) {
	for (int i = 0, n = toolbar.Count(); i < n; ++i) {
		Menu	menu = toolbar.GetItem(i, TBIF_LPARAM).Param();
		menu.CheckByID(id, check);
	}
#ifdef DISASSEMBLER_H
	SetDisassembler(0);
#else
	Changed();
#endif
}
void ViewBin::Radio(uint32 id) {
	for (int i = 0, n = toolbar.Count(); i < n; ++i) {
		Menu	menu = toolbar.GetItem(i, TBIF_LPARAM).Param();
		menu.Radio(id);
	}
#ifdef DISASSEMBLER_H
	SetDisassembler(0);
#else
	Changed();
#endif
}

void ViewBin::Find(const interval<uint64> &range, bool fwd) {
	if (!prog) {
		prog.Create(toolbar, "progress", VISIBLE | CHILD | ProgressBarControl::SMOOTH, NOEX, toolbar.GetUnusedRect());
		prog_start	= range.a;
		prog_shift	= max(highest_set_index(uint64(range.extent())), 30) - 30;
		prog.SetRange(0, range.extent() >> prog_shift);
		SetTimer(2, 0);
	}

	finder = new Finder(find_pattern, this, getter, range - start_addr, fwd);
}

bool ViewBin::CheckFinding() {
	if (!finder)
		return false;
	if (MessageBoxA(*this, "Abort current find?", "IsoEditor", MB_ICONQUESTION | MB_YESNO) == IDNO)
		return true;
	if (finder)
		finder->state = 1;
	return false;
}

void ViewBin::AbortFind() {
	if (finder)
		finder->state = 1;
}
bool	ViewBin::operator()(int state, interval<uint64> &found) {
	SendMessage(WM_ISO_FOUND, state, (LPARAM)&found);
	return true;
}

void ViewBin::VScroll(float y) {
#ifdef D2D_H
	Invalidate();
#else
	Scroll(0, y * line_height, SW_INVALIDATE | SW_SCROLLCHILDREN, &rects[1], &rects[1]);
#endif
	if (tip_state) {
		uint64				addr	= AddressFromWindow(ToClient(GetMousePos()));
		buffer_accum<256>	ba;
		if (addr < start_addr + getter.length) {
			tooltip.Update(*this, GetTipText(ba, addr).term());
			tip_state = 2;
		}
		//tooltip.Track();
	}
}

int	ViewBin::GetMouseWheel(WPARAM wParam) {
	int	z = GET_WHEEL_DELTA_WPARAM(wParam);
	if (prevz && ((z < 0) ^ (prevz < 0)))
		z = 0;
	prevz = z;
	return z;
}

void ViewBin::ChangedSize() {
	if (!is_dis() && flags.test(AUTOSIZE)) {
		uint64 address	= AddressAtLine(0);
		if (SetBytesPerLine(CalcBytesPerLine2(rects[1].Width() / zoom)))
			ChangedBytesPerLine(address);
	}
	float	move = si.SetPage(rects[1].Height() / (line_height * zoom));
	SetScroll(si);
	VScroll(move);

	int		chars_per_line		= CalcCharsPerLine(bytes_per_line, bytes_per_element, CalcCharsPerElement2(), address_digits, flags.test(ASCII));
	sih.Set(0, chars_per_line, rects[1].Width() / zoom / char_width);
	SetScroll(sih, false);

	if ((HWND)miniedit)
		miniedit.Move(CalcRect(edit_addr, edit_ascii).Grow(4,2,4,2));
}


void ViewBin::Goto(uint64 address) {
	address = max(address, start_addr + offset);
	if (is_dis()) {
#ifdef DISASSEMBLER_H
		si.MoveTo(lower_bound(line_iterator(dis_state, 0), line_iterator(dis_state, dis_state->Count()), address).i);
#endif
	} else {
		offset = (address - start_addr) % bytes_per_line;
		si.MoveTo((address - start_addr) / bytes_per_line);
	}
	SetScroll(si);
	Invalidate();
}

void ViewBin::Select(const interval<uint64> &i) {
	selection	= i;
	Rect	r	= CalcRect(selection.a, selection.b);
	if (!rects[1].Contains(r)) {
		si.MoveTo(is_dis() ? selection.a - start_addr : (selection.a - start_addr - offset) / bytes_per_line);
		SetScroll(si);
	}
	Invalidate();
}

LRESULT ViewBin::Proc(MSG_ID msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			toolbar.Create(*this, NULL, CHILD | VISIBLE);

			Menu	menu	= Menu(IDR_MENU_BINARY);
#ifdef DISASSEMBLER_H
			Menu	dis_menu = menu.GetSubMenuByName("Disassembly");
			dis_menu.SetStyle(MNS_NOTIFYBYPOS);
			int		pos		= 0;
			for (Disassembler::iterator i = Disassembler::begin(); i != Disassembler::end(); ++i)
				Menu::Item(i->GetDescription(), ID_BINARY_DISASSEMBLY).Param(i).InsertByPos(dis_menu, pos++);
#else
			menu.RemoveByPos(menu.FindPosition("Disassembly"));
#endif
			switch (base) {
				case  2:	menu.Radio(ID_BINARY_RADIX_BINARY);			break;
				case  8:	menu.Radio(ID_BINARY_RADIX_OCTAL);			break;
				case 10:	menu.Radio(ID_BINARY_RADIX_DECIMAL);		break;
				case 16:	menu.Radio(ID_BINARY_RADIX_HEX);			break;
			}
			switch (bytes_per_element) {
				case 1:		menu.Radio(ID_BINARY_SIZE_8BIT);			break;
				case 2:		menu.Radio(ID_BINARY_SIZE_16BIT);			break;
				case 4:		menu.Radio(ID_BINARY_SIZE_32BIT);			break;
				case 8:		menu.Radio(ID_BINARY_SIZE_64BIT);			break;
			}
			if (flags.test(AUTOSIZE))  {
				menu.Radio(ID_BINARY_BYTESPERLINE_AUTO);
			} else switch (bytes_per_line) {
				case 4:		menu.Radio(ID_BINARY_BYTESPERLINE_4);		break;
				case 8:		menu.Radio(ID_BINARY_BYTESPERLINE_8);		break;
				case 16:	menu.Radio(ID_BINARY_BYTESPERLINE_16);		break;
				case 32:	menu.Radio(ID_BINARY_BYTESPERLINE_32);		break;
				case 64:	menu.Radio(ID_BINARY_BYTESPERLINE_64);		break;
				default:	menu.Radio(ID_BINARY_BYTESPERLINE_SPECIFY);	break;
			}

			menu.CheckByID(ID_BINARY_ASCII,			flags.test(ASCII));
			menu.CheckByID(ID_BINARY_BIGENDIAN,		flags.test(BIGENDIAN));
			menu.CheckByID(ID_BINARY_SIGNED,		flags.test(SIGNED));
			menu.CheckByID(ID_BINARY_LEADINGZEROS,	flags.test(ZEROS));

			toolbar.Init(menu);

			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);
			tip_state	= 0;

			hFont		= Font("Courier New", 15);
			break;
		}

		case WM_SIZE:
			ControlArrangement::GetRects(ToolbarArrange, Rect(Point(0, 0), Point(lParam)), rects);
			toolbar.Move(rects[0]);
			if (prog)
				prog.Move(toolbar.GetUnusedRect());
			ChangedSize();
			break;

		case WM_VSCROLL: {
			int	move;
			if (LOWORD(wParam) == SB_THUMBTRACK) {
				ScrollInfo	tsi(SIF_RANGE | SIF_TRACKPOS);
				GetScroll(tsi);
				move = si.MoveTo(tsi.nTrackPos, tsi.nMax);
			} else {
				move = si.ProcessScroll(wParam);
				SetScroll(si);
			}
			VScroll(move);
			break;
		}

		case WM_HSCROLL:
			sih.ProcessScroll(wParam);
			SetScroll(sih, false);
			Invalidate();
			break;

		case WM_MOUSEACTIVATE:
			SetAccelerator(*this, IDR_ACCELERATOR_BINARY);
			break;

		case WM_LBUTTONDOWN:
			if (SetFocus() == *this) {
				uint64	addr	= AddressFromWindow(Point(lParam));
				if (addr < start_addr + getter.length) {
					if (selection.b == selection.a) {
						selection.b = (selection.a = addr) + bytes_per_element;
						Invalidate(CalcRect(selection.a, selection.b));
					} else {
						Invalidate(CalcRect(selection.a, selection.b));
						if (wParam & MK_SHIFT) {
							selection.b = addr;
							Invalidate(CalcRect(selection.a, selection.b));
						} else {
							selection.b = selection.a = 0;
						}
					}
					Update();
					SetCapture(*this);
				}
			}
			break;

		case WM_LBUTTONUP:
			ReleaseCapture();
			break;

		case WM_LBUTTONDBLCLK:
			if (!read_only) {
				edit_addr	= AddressFromWindow(Point(lParam), &edit_ascii);
				if (edit_addr < start_addr + getter.length) {
					if (edit_ascii)
						edit_addr = align_down(edit_addr - offset, bytes_per_element) + offset;
					Rect rect	= CalcRect(edit_addr, edit_ascii);
					char	buffer[64];
					if (edit_ascii) {
						memcpy(buffer, GetMemory(edit_addr), bytes_per_element);
						buffer[bytes_per_element] = 0;
					} else {
						int		chars_per_element	= CalcCharsPerElement2();
						PutNumber(buffer, GetMemory(edit_addr), chars_per_element);
						buffer[chars_per_element] = 0;
					}
					miniedit.Create(*this, none, CHILD | VISIBLE | CLIPSIBLINGS | EditControl::AUTOHSCROLL | EditControl::WANTRETURN, CLIENTEDGE,
						rect.Grow(4,2,4,2),
						ID_EDIT
					);
					miniedit.SetFont(hFont);
					miniedit.MoveAfter(HWND_TOP);
					miniedit.SetText(buffer);
					miniedit.SetFocus();
				}
			}
			break;

		case WM_MBUTTONDOWN:
			mouse_down	= Point(lParam);
			mouse_pos	= si.Pos() + mouse_down.y / line_height;
			SetAccelerator(*this, IDR_ACCELERATOR_BINARY);
			SetFocus();
		case WM_MBUTTONUP:
			if (timer) {
				KillTimer(timer);
				timer = 0;
			}
			break;

		case WM_MOUSEMOVE: {
			Point	mouse(lParam);
			if (tip_state == 2) {
				tooltip.Update(*this);
				tip_state = true;
			}
			if (!tip_state) {
				TrackMouse(TME_LEAVE);
				tooltip.Activate(*this, tip_state = true);
			}
			if (wParam & MK_LBUTTON) {
				if (int move = mouse.y > rects[1].Bottom() ? 1 : mouse.y < rects[1].Top() ? -1 : 0) {
					VScroll(si.MoveBy(move));
					SetScroll(si);
				}
				bool	is_ascii;
				uint64	addr	= AddressFromWindow(mouse, &is_ascii);
				if (addr < start_addr + getter.length) {
					uint32	bpe		= is_ascii ? 1 : bytes_per_element;
					if (selection.b == selection.a) {
						selection.a = addr;
						selection.b	= addr + bpe;
						Invalidate(CalcRect(selection.a, selection.b));
					} else {
						Rect	rect = CalcRect(selection.a, selection.b);
						if (addr >= selection.a)
							addr += bpe;
						selection.b	= min(addr, start_addr + getter.length);
						rect	|= CalcRect(selection.a, selection.b);
						Invalidate(rect);
					}
				}
				Update();

			} else if (wParam & MK_MBUTTON) {
				if (!timer)
					timer = SetTimer(1, 0);
			}
			tooltip.Track();
			break;
		}

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, tip_state = false);
			break;

		case WM_TIMER:
			switch (wParam) {
				case 1:	// scrolling
					if (GetKeyState(VK_MBUTTON) & 0x80) {
						Point	mouse	= ToClient(GetMousePos());
						int		move	= si.MoveBy(mouse_down.y - mouse.y);
						SetScroll(si);
						VScroll(move);
					} else {
						mouse_inertia	*=  0.95f;
						if (mouse_inertia < .1f)
							KillTimer(1);

						mouse_frac		+= mouse_inertia;
						int		move	= si.MoveBy(trunc(mouse_frac));
						mouse_frac		= frac(mouse_frac);
						SetScroll(si);
						VScroll(move);
					}
					break;

				case 2:	//progress
					if (finder)
						prog.SetPos((finder->current - prog_start) >> prog_shift);
					else
						KillTimer(2);
					break;
			}
			break;

		case WM_MOUSEWHEEL: {
			int	s = GetMouseWheel(wParam);
			if (wParam & MK_CONTROL) {
				float	mult	= iso::pow(1.05f, s / 64.f);
				zoom	= max(zoom / mult, 1/30.f);
				ChangedSize();
			} else {
				VScroll(si.MoveBy(-s * 5 / WHEEL_DELTA));
				SetScroll(si);
			}
			break;
		}

		case WM_CONTEXTMENU:
			edit_addr = AddressFromWindow(ToClient(Point(lParam)));
			if (edit_addr < start_addr + getter.length)
				Menu(IDR_MENU_BINARYCONTEXT).GetSubMenuByPos(0).Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
			break;

		case WM_PAINT: {
			ViewBinMode::Current() = *this;
			DeviceContextPaint	xdc(*this);
			if (target.Init(hWnd, GetClientRect().Size()) && !target.Occluded()) {
				target.BeginDraw();
				target.SetTransform(translate(0, rects[1].top) * float2x3(scale(zoom, zoom)));

				Paint(target, rects[1], sih.Pos(), si.Pos(), write, font);

				if (target.EndDraw())
					target.DeInit();
			}
			break;
		}

		case WM_ERASEBKGND:
			return TRUE;

		case WM_KEYDOWN:
			if (selection.b != selection.a && GetAsyncKeyState(VK_SHIFT) & 0x8000) {
				switch (wParam) {
					case VK_LEFT:	selection.b -= bytes_per_element; break;
					case VK_RIGHT:	selection.b += bytes_per_element; break;
					case VK_PRIOR:	selection.b -= int(rects[1].Height() / line_height) * bytes_per_line; break;
					case VK_NEXT:	selection.b += int(rects[1].Height() / line_height) * bytes_per_line; break;
					case VK_UP:		selection.b -= bytes_per_line; break;
					case VK_DOWN:	selection.b += bytes_per_line; break;
					default: return 0;
				}
				Invalidate();

			} else  if (int move = si.ProcessKey(wParam)) {
				SetScroll(si);
				VScroll(move);

			} else if (wParam == VK_LEFT || wParam == VK_RIGHT) {
				offset = (offset + (wParam == VK_LEFT ? bytes_per_line - 1 : 1)) % bytes_per_line;
				Invalidate();
			}
			break;

#ifdef DISASSEMBLER_H
		case WM_MENUCOMMAND : {
			Menu::Item	i(MIIM_DATA | MIIM_ID);
			i.GetByPos(Menu((HMENU)lParam), wParam);
			switch (i.ID()) {
				case ID_BINARY_DISASSEMBLY:
					SetDisassembler((Disassembler*)i.Param());
					break;

				case ID_BINARY_DISASSEMBLY_SETSYMBOLS:	{
					ISO::Browser	b = ((IsoEditor*)MainWindow::Get())->GetSelection();
					symbols.clear();
					for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
						tag	name = i.GetName();
						if (!name)
							name = i->FindByType<string>().GetString();

						if (name) {
							ISO::Browser	v	= i->SkipUser();
							while (v.GetType() == ISO::REFERENCE)
								v = (*v).SkipUser();
							if (v.GetType() == ISO::INT || (v = i->FindByType<uint64>()) || (v = i->FindByType<uint32>()))
								symbols.emplace_back(v.Get<uint64>(), name);
						}
					}
					if (Disassembler *d = disassembler) {
						disassembler = 0;
						SetDisassembler(d);
					}
					break;
				}
				case ID_BINARY_DISASSEMBLY_SETSTRINGS: {
					strings = ((IsoEditor*)MainWindow::Get())->GetSelection();
					if (strings.GetType() == ISO::REFERENCE)
						strings = *strings;
					if (Disassembler *d = disassembler) {
						disassembler = 0;
						SetDisassembler(d);
					}
					break;
				}
			}
			break;
		}
#endif

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_BINARY_RADIX_BINARY:		base =  2; Radio(id); break;
				case ID_BINARY_RADIX_OCTAL:			base =  8; Radio(id); break;
				case ID_BINARY_RADIX_DECIMAL:		base = 10; Radio(id); break;
				case ID_BINARY_RADIX_HEX:			base = 16; Radio(id); break;
				case ID_BINARY_FLOATINGPOINT: {
					bool	f = flags.flip(FLOAT).test(FLOAT);
					flags.set(SIGNED, f);
					Radio(id);
					Check(ID_BINARY_SIGNED, f);
					break;
				}

				case ID_BINARY_SIZE_8BIT:			SetBytesPerElement(1); Radio(id); break;
				case ID_BINARY_SIZE_16BIT:			SetBytesPerElement(2); Radio(id); break;
				case ID_BINARY_SIZE_32BIT:			SetBytesPerElement(4); Radio(id); break;
				case ID_BINARY_SIZE_64BIT:			SetBytesPerElement(8); Radio(id); break;

				case ID_BINARY_BYTESPERLINE_AUTO:	flags.set(AUTOSIZE); Radio(id);			break;
				case ID_BINARY_BYTESPERLINE_4:		SetFixedBytesPerLine(4); Radio(id);		break;
				case ID_BINARY_BYTESPERLINE_8:		SetFixedBytesPerLine(8); Radio(id);		break;
				case ID_BINARY_BYTESPERLINE_16:		SetFixedBytesPerLine(16); Radio(id);	break;
				case ID_BINARY_BYTESPERLINE_32:		SetFixedBytesPerLine(32); Radio(id);	break;
				case ID_BINARY_BYTESPERLINE_64:		SetFixedBytesPerLine(64); Radio(id);	break;
				case ID_BINARY_BYTESPERLINE_SPECIFY:
					if (const char *val = GetValueDialog(*this, iso::to_string(bytes_per_line))) {
						int	n;
						if (from_string(val, n)) {
							SetFixedBytesPerLine(n);
							Radio(id);
						}
					}
					break;

				case ID_BINARY_ASCII:			Check(id, flags.test_flip(ASCII));		break;
				case ID_BINARY_BIGENDIAN:		Check(id, flags.test_flip(BIGENDIAN));	break;
				case ID_BINARY_SIGNED:			Check(id, flags.test_flip(SIGNED));		break;
				case ID_BINARY_LEADINGZEROS:	Check(id, flags.test_flip(ZEROS));		break;
				case ID_BINARY_SELSTART:		Goto(selection.a); break;
				case ID_BINARY_SELEND:			Goto(selection.b); break;

				case ID_BINARY_GOTO:
				#ifdef DISASSEMBLER_H
					if (const char *val = GetValueDialog(*this, buffer_accum<256>("0x") << hex(IndexToAddress(AddressAtLine(0))))) {
				#else
					if (const char *val = GetValueDialog(*this, buffer_accum<256>("0x") << hex(AddressAtLine(0)))) {
				#endif
						uint64	address;
						if (from_string(val, address))
							Goto(address);
					}
					break;

#ifdef ISO_EDITOR
				case ID_BINARY_OPEN: {
					ISO_ptr<ISO_openarray<uint8> > p(format_string("binary_0x%llx", selection.a));
					uint64	a		= min(selection.a, selection.b);
					uint64	e		= max(selection.a, selection.b);
					uint8	*d		= p->Create(e - a, false);

					while (a < e) {
						memory_block	m	= GetMemory(a);
						if (m.length() == 0)
							break;
						uint32			s	= min(m.length(), e - a);
						memcpy(d, m, s);
						d += s;
						a += s;
					}
					((IsoEditor*)MainWindow::Get())->AddEntry2(p, false);
					break;
				}
#endif

				case ID_BINARY_FOLLOW: {
					uint64	address = LoadNumber(GetMemory(edit_addr));
					si.MoveTo((address - start_addr - offset) / bytes_per_line);
					SetScroll(si);
					Invalidate();
					break;
				}

				case ID_BINARY_COLOUR: {
					CHOOSECOLOR	cc;
					clear(cc);
					cc.lStructSize	= sizeof(cc);
					cc.hwndOwner	= *this;
					cc.lpCustColors	= custom_colours;
					cc.Flags		= CC_RGBINIT;
					if (ChooseColor(&cc)) {
						colours[make_interval(selection.a, selection.b)] = win::Colour(cc.rgbResult);
						Invalidate();
					}
					break;
				}

				case ID_EDIT_FIND:
					if (!CheckFinding() && GetValueDialog(*this, find_text)) {
						find_pattern.init(find_text);
						uint64	start = selection.a == selection.b ? AddressAtLine(0) : selection.a + 1;
						Find(interval<uint64>(start, start_addr + getter.length), true);
					}
					break;

				case ID_EDIT_FINDNEXT:
					if (!CheckFinding())
						Find(interval<uint64>(selection.a + 1, start_addr + getter.length), true);
					break;

				case ID_EDIT_FINDPREV:
					if (!CheckFinding())
						Find(interval<uint64>(selection.a - 1, start_addr + getter.length), false);
					break;

				case ID_EDIT_FINDABORT:
					AbortFind();
					break;

				case ID_EDIT_COPY:
					if (HIWORD(wParam) == 2) {
						// save copy of data for diffing
						prev_bin = memory_block(getter.bin, getter.length);
					} else {
						dynamic_memory_writer	m;
						if (is_dis()) {
#ifdef DISASSEMBLER_H
							uint32	begin = 0, end = dis_state->Count();
							if (selection.a != selection.b) {
								begin	= (uint32)min(selection.a, selection.b);
								end		= (uint32)max(selection.a, selection.b);
							}
							for (uint32 i = begin; i != end; ++i) {
								buffer_accum<1024>	ba;
								dis_state->GetLine(ba, i);
								m.write((ba << '\n').term());
							}
#endif
						} else {
							uint64	begin = 0, end = getter.length;
							if (selection.a != selection.b) {
								begin	= min(selection.a, selection.b);
								end		= max(selection.a, selection.b);
							}
							int		chars_per_element	= CalcCharsPerElement2();
							int		chars_per_line		= CalcCharsPerLine(bytes_per_line, bytes_per_element, chars_per_element, address_digits, flags.test(ASCII)) + 1;
							char	*buffer				= alloc_auto(char, chars_per_line + 2);
							for (uint64 a = begin; a < end; a += bytes_per_line) {
								fixed_accum	acc(buffer, chars_per_line + 2);
								PutLine(acc, a - begin, GetMemory(a), min(end - a, uint64(bytes_per_line)), 0, 0, chars_per_element);
								acc << '\n';
								m.write(acc.term());
							}
						}
						m.putc(0);
						Clipboard	clip(*this);
						if (clip.Empty())
							clip.Set(CF_TEXT, m.data());
					}
					break;

				case ID_EDIT_DELETE:
					if (selection.a != selection.b && getter.bin) {
						uint64	begin	= min(selection.a, selection.b) - start_addr;
						uint64	end		= min(max(selection.a, selection.b)- start_addr, getter.length);
//						ISO_ptr<ISO_openarray<uint8> >	save("save", end - begin);
//						memcpy(save, (uint8*)bin + begin, end - begin);
						memcpy((uint8*)getter.bin + begin, (uint8*)getter.bin + end, getter.length - end);
						getter.b.Resize(getter.length - (end - begin));
						getter.bin				= *getter.b;
						getter.length			= getter.b.Count() * getter.b[0].GetSize();
						selection.b	= selection.a = 0;
						si.SetMax(getter.length / bytes_per_line + 1);
						SetScroll(si);
						Invalidate();
					}
					break;
#if 0
				case ID_EDIT:
#ifdef ISO_EDITOR
					if (HIWORD(wParam) == EN_KILLFOCUS) {
						if (size_t len = miniedit.GetText().len()) {
							uint8	*p = GetMemory(edit_addr);
							((IsoEditor*)MainWindow::Get())->Do(MakeModifyDataOp(ISO::MakeBrowser(*p)));
							if (edit_ascii)
								miniedit.GetText().get((char*)p, bytes_per_element);
							else
								GetNumber(str<64>(miniedit.GetText()), p);
						}
						Invalidate();
						miniedit.Destroy();
						break;
					}
#endif
					if (HIWORD(wParam) & 1)
						FlushMemory();

					if (HIWORD(wParam) & 2) {
						auto	r	= (interval<uint64>*)lParam;
						Select(*r);

					} else if (~lParam) {
						Goto(lParam);
					}
					Invalidate();
					break;
#endif
				default:
					return 0;
			}
			return 1;

		case WM_SETFOCUS:
			SetAccelerator(*this, IDR_ACCELERATOR_BINARY);
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TBN_DROPDOWN: {
					NMTOOLBAR	*nmtb	= (NMTOOLBAR*)nmh;
					ToolBarControl(nmh->hwndFrom).GetItem(nmtb->iItem).Param<Menu>().Track(*this, ToScreen(Rect(nmtb->rcButton).BottomLeft()));
					break;
				}
				case TTN_GETDISPINFOA: {
					NMTTDISPINFOA	*nmtdi	= (NMTTDISPINFOA*)nmh;
					uint64			addr	= AddressFromWindow(ToClient(GetMousePos()));
					if (addr < start_addr + getter.length)
						GetTipText(lvalue(fixed_accum(nmtdi->szText)), addr);
					else
						tooltip.Activate(*this, tip_state = false);
					break;
				}
			}
			break;
		}

		case WM_ISO_FOUND: {
			uint32	state	= wParam;
			auto	i		= *(interval<uint64>*)lParam + start_addr;
			prog.Destroy();
			prog.hWnd = 0;
			finder	= 0;

			switch (state) {
				case 0:	Select(i); break;
				case 1: MessageBoxA(*this, format_string("Find aborted at 0x%llx", i.a), "IsoEditor", MB_ICONINFORMATION); break;
				case 2: MessageBoxA(*this, "Pattern was not found", "IsoEditor", MB_ICONERROR); break;
			}

			return 0;
		}

		case WM_NCDESTROY:
			tooltip.Destroy();
			delete this;
			return 0;
		default:
			return Super(msg, wParam, lParam);
	}
	return 0;
}

Point ViewBin::CalcPoint(uint64 a, bool br, bool ascii) {
	uint64	start = AddressAtLine(0);
	if (a < start)
		return Point(0,0);
	if (is_dis())
		return Point(br ? rects[1].right : rects[1].left, (a - start + int(br)) * line_height + rects[1].top);
	int		xascii;
	int		y = (a - start) / bytes_per_line;
	int		x = CharFromOffset(a - start - y * bytes_per_line + (br ? bytes_per_element : 0), &xascii) - int(br);
	return Point(int((ascii ? xascii : x) * char_width * zoom), int((y + int(br)) * line_height * zoom) + rects[1].top);
}

Rect ViewBin::CalcRect(uint64 a, bool ascii) {
	return Rect(CalcPoint(a, false, ascii), CalcPoint(a, true, ascii));
}

Rect ViewBin::CalcRect(uint64 a, uint64 b) {
	if (a > b)
		swap(a, b);

	uint64	start	= AddressAtLine(0);
	a = max(a, start);
	if (b <= a)
		return Rect(0,0,0,0);

	if (is_dis())
		return Rect(rects[1].left, (a - start) * line_height + rects[1].top, rects[1].Width(), (b - a) * line_height);

	uint32	linea	= (a - start) / bytes_per_line;
	uint32	lineb	= (b - start) / bytes_per_line;
	if (linea != lineb) {
		a = start + linea * bytes_per_line;
		b = start + lineb * bytes_per_line + bytes_per_line - 1;
	}

	Point	pta = CalcPoint(a, false, false);
	Point	ptb = CalcPoint(b, true, true);
	return Rect(Point(min(pta.x, ptb.x), min(pta.y, ptb.y)), Point(max(pta.x, ptb.x), max(pta.y, ptb.y)));
}

int ViewBin::CalcRegion(uint64 a, uint64 b, Rect *rects) {
	Rect	*pr		= rects;
	uint64	start	= AddressAtLine(0);

	if (a > b)
		swap(a, b);
	a = max(a, start);
	b = max(a, start);

	uint32	linea	= (a - start) / bytes_per_line;
	uint32	lineb	= (b - start) / bytes_per_line;

	if (linea == lineb) {
		*pr++ = Rect(CalcPoint(a, false, false), CalcPoint(b, true, false));
		*pr++ = Rect(CalcPoint(a, false, true), CalcPoint(b, true, true));
	} else {
		uint64	a1 = start + linea * bytes_per_line;
		if (a != a1) {
			a1 += bytes_per_line;
			*pr++ = Rect(CalcPoint(a, false, false), CalcPoint(a1 - 1, true, false));
			*pr++ = Rect(CalcPoint(a, false, true), CalcPoint(a1 - 1, true, true));
		}
		uint64	b1 = start + lineb * bytes_per_line;
		if (b != b1) {
			*pr++ = Rect(CalcPoint(b1, false, false), CalcPoint(b, true, false));
			*pr++ = Rect(CalcPoint(b1, false, true), CalcPoint(b, true, true));
		} else {
			b1 -= bytes_per_line;
		}
		if (a1 != b1) {
			*pr++ = Rect(CalcPoint(a1, false, false), CalcPoint(b1, true, false));
			*pr++ = Rect(CalcPoint(a1, false, true), CalcPoint(b1, true, true));
		}
	}
	return pr - rects;
}

void ViewBin::ChangedBytesPerLine(uint64 address) {
	si.SetMax(getter.length / bytes_per_line + 1);
	si.MoveTo((address - start_addr - offset) / bytes_per_line);
	SetScroll(si);

	int		chars_per_line		= CalcCharsPerLine(bytes_per_line, bytes_per_element, CalcCharsPerElement2(), address_digits, flags.test(ASCII));
	sih.Set(0, chars_per_line, rects[1].Width() / zoom / char_width);
	SetScroll(sih, false);
	offset	= 0;

	Invalidate();
}

void ViewBin::Changed() {
	if (is_dis() || flags.test(AUTOSIZE)) {
		Rect	client	= GetClientRect();
		int64	nMax;

#ifdef DISASSEMBLER_H
		if (is_dis()) {
			nMax = dis_state->Count();
		} else
#endif
		{
			bytes_per_line = CalcBytesPerLine2(client.Width());
			nMax = getter.length / bytes_per_line + 1;
		}
		auto	newpos = mul_div(si.Pos(), nMax, si.Max());
		si.Set(0, nMax, client.Height() / line_height);
		si.MoveTo(newpos);
		SetScroll(si);
		offset	= 0;
	}
	Invalidate();
}

#ifdef DISASSEMBLER_H

void ViewBin::SetDisassembler(Disassembler *d) {
	if (disassembler != d) {
		Busy	bee;
		dis_state = 0;
		if (disassembler = d) {
			//			d->Disassemble(GetMemory(addr), length - addr, lines, symbols, (const char*)strings.Get<uint8*>(0), strings.Count());
			dis_state = d->Disassemble(GetMemory(start_addr), start_addr, make_callback(this, &ViewBin::SymbolFinder));
		}
	}
	Changed();
}
#endif

ViewBin::ViewBin(const WindowPos &wpos, const char *title, const ISO::Browser2 &b, ID id) : font(write, L"Courier New", 15) {
	font->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	SetFontMetrics(write, font);

	SetBinary(b);
	si.SetMax(getter.length / bytes_per_line + 1);

	if (!title)
		title = "binary";

	Create(wpos, title, CHILD | VISIBLE | CLIPCHILDREN, NOEX, id);
}

Control BinaryWindow(const WindowPos &pos, const ISO::Browser2 &b, ID id) {
	return *new ViewBin(pos, b.GetName().get_tag(), b, id);
}

//-----------------------------------------------------------------------------
//	Editor
//-----------------------------------------------------------------------------

class EditorBin : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return	ViewBin_base::Matches(type);
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_VirtualTarget &b) {
		return *new ViewBin(pos, b.GetName().get_tag(), b.GetData());
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_ptr<void> &p) {
		return *new ViewBin(pos, p.ID().get_tag(), ISO::Browser2(p));
	}
} editorbin;

//-----------------------------------------------------------------------------
//	Hex reader
//-----------------------------------------------------------------------------

class HEXFileHandler : public FileHandler {
	virtual	const char*		GetDescription()		{ return "Hex"; }
	virtual	int				Check(istream_ref file)	{
		file.seek(0);
		text_mode_reader<istream_ref>	tmr(file);
		if (tmr.mode == text_mode::UNRECOGNISED)
			return CHECK_DEFINITE_NO;
		int			c = tmr.getc();
		return c == '#' || is_hex(c) ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}
	virtual ISO_ptr<void>	Read(tag id, istream_ref file) {
		ISO_ptr<ISO_openarray<uint8> > p(id);
		string	line;
		auto	text = make_text_reader(file);
		while (text.read_line(line)) {
			if (line && line[0] != '#') {
				string_scan	ss(line);
				ReadHexLine(*p, ss);
			}
		}
		return p;
	}
} hexfile;

} // namespace app

