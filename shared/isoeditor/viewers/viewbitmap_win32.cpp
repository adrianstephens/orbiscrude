#define INITGUID

#include "viewbitmap.h"
#include "iso/iso_files.h"
#include "windows/dib.h"
#include "windows/filedialogs.h"
#include "windows/control_helpers.h"
#include "viewer.h"
#include "viewbitmap.rc.h"
#include "windows/common.rc.h"

#define IDR_MENU_BITMAP			"IDR_MENU_BITMAP"
#define IDR_TOOLBAR_BITMAP		"IDR_TOOLBAR_BITMAP"
#define IDR_ACCELERATOR_BITMAP	"IDR_ACCELERATOR_BITMAP"
#define IDB_CHECKER				"IDB_CHECKER"

namespace app {
using namespace iso;
using namespace win;

struct TrackBarControlF : TrackBarControl {
	float	GetPos()			const { return 1 - TrackBarControl::GetPos() / 1000.f; }
	void	SetPos(float f)		const { TrackBarControl::SetPos((1 - f) * 1000); }
	bool	SetTick(float f)	const { return TrackBarControl::SetTick((1 - f) * 1000); }

	TrackBarControlF()	{}
	TrackBarControlF(Control c) : TrackBarControl(c) {}

	TrackBarControlF(HWND hWndParent, const char *caption, Style style, StyleEx styleEx, const RECT &rect, ID id = ID())
		: TrackBarControl(hWndParent, caption, style, styleEx, rect, id) {
		SetRange(0, 1000);
	}
};

TrackBarControlF DropDownTrackBar(ToolBarControl toolbar, uint16 id, uint32 height) {
	toolbar.CheckButton(id,	true);
	Rect	r	= toolbar.GetItemRectByID(id);
	return TrackBarControlF(toolbar.Parent(), 0, Control::CHILD | Control::CLIPSIBLINGS | Control::VISIBLE | TrackBarControl::BOTTOM, Control::NOEX, Rect(r.Left(), r.Bottom(), r.Width(), height), id);
}

class ViewBitmap : ViewBitmap_base, public Window<ViewBitmap>, public WindowTimer<ViewBitmap> {
	enum {
		CONTROL_IDS	= 10000,
		TRACK_MIN,
		TRACK_MAX,
	};

	static const float	min_gamma, max_gamma;

	ToolBarControl			toolbar;
	ToolTipControl			tooltip;
	Menu					contextmenu;
	Rect					rects[2];
	char					tooptip_text[256];

	d2d::WND				target;
	d2d::BitmapBrush		checker;
	D2D_Marker				marker;
	com_ptr<ID2D1Bitmap>	d2d_bitmap;

	histogram4		hist;

	static filename	fn;

	Point			prevmouse;
	uint32			prevbutt	= 0;
	Rect			hist_rect	= {0, ToolBarControl::Height, 512, 200};
	Rect			selection	= {0,0,0,0};
	bool			update_bitmap, update_hist;

	bool	OverHistogram(const Point &pt) {
		return flags.test(DISP_HIST) && hist_rect.Contains(pt);
	}
	d2d::point	TexelToClient(const d2d::point &pt) {
		return d2d::point(pt * zoom) + rects[1].TopLeft() + pos;
	}
	Point	ClientToTexel(const Point &pt) {
		d2d::point	retf = ClientToTexel0(pt - rects[1].TopLeft());
		Point		ret(retf.x, retf.y);
		if (num_slices > 1) {
			int	h0 = bitmap_rect.Height();
			int	h1 = h0 + 8;
			ret.y = ret.y / h1 * h0 + ret.y % h1;
		}
		return ret;
	}
	int		ClientToSlice(const Point &pt) {
		return CalcSlice(lvalue(ClientToTexel0(pt - rects[1].TopLeft())));
	}

	void	SetChannels(int c) {
		if (c == 0)
			c = CHAN_RGB;
		channels = c;
		toolbar.CheckButton(ID_BITMAP_CHANNEL_RED,	!!(c & CHAN_R));
		toolbar.CheckButton(ID_BITMAP_CHANNEL_GREEN,!!(c & CHAN_G));
		toolbar.CheckButton(ID_BITMAP_CHANNEL_BLUE,	!!(c & CHAN_B));
		toolbar.CheckButton(ID_BITMAP_CHANNEL_ALPHA,!!(c & CHAN_A));
		if (c & CHAN_DEPTH) {
			bitmap64	*bm = this->bm;
			depth_range = get_extent(make_range_n((uint32*)bm->ScanLine(0), bm->Width() * bm->Height()));
		}
#ifndef _D2D1_1_H_
		update_bitmap = true;
#endif
		Invalidate();
	}

	bool CreateDeviceResources() {
		target.Init(hWnd, GetClientRect().Size());
		if (!d2d_bitmap)
			update_bitmap = target.CreateBitmap(&d2d_bitmap, bitmap_rect.Width(), bitmap_rect.Height() * num_slices);
		return checker.CreateDeviceResources(target);
	}
	void DiscardDeviceResources() {
		checker.DiscardDeviceResources();
		d2d_bitmap.clear();
		target.DeInit();
	}

	void UpdateHist() {
		hist.reset();
		if (auto hdr = bm.test_cast<HDRbitmap64>()) {
			if (selection.Width() && selection.Height()) {
				Rect	r	= selection & bitmap_rect;
				hist.add(hdr->Block(r.Left(), r.Top(), r.Width(), r.Height()), histogram4::scale_offset(HDRpixel(rgb_range.a, alpha_range.a), HDRpixel(rgb_range.b, alpha_range.b)));
				hist.init_medians();
			} else {
				hist.init(hdr, HDRpixel(rgb_range.a, alpha_range.a), HDRpixel(rgb_range.b, alpha_range.b));
			}
		} else {
			bitmap64	*bm = this->bm;
			if (selection.Width() && selection.Height()) {
				Rect	r	= selection & bitmap_rect;
				hist.add(bm->Block(r.Left(), r.Top(), r.Width(), r.Height()), [](const ISO_rgba &x) { return reinterpret_cast<const uint8x4&>(x); });
				hist.init_medians();
			} else {
				hist.init(bm);
			}
		}
	}
	void Paint();

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	ViewBitmap(const WindowPos &wpos, const ISO_ptr_machine<void> &p, const char *text, bool auto_scale)
		: checker(Bitmap::Load(IDB_CHECKER, LR_CREATEDIBSECTION), float2x3((float2x2)scale(10)))
	{
		MyCustomEffect::Register(target.factory);

		if (auto_scale)
			flags.set(AUTO_SCALE);

		SetBitmap(FileHandler::ExpandExternals(p));

		Create(wpos, text, (wpos.parent ? CHILD : OVERLAPPEDWINDOW | VISIBLE) | CLIPCHILDREN | CLIPSIBLINGS, wpos.parent ? CLIENTEDGE : NOEX);

		if (anim)
			Timer::Next((*anim)[0].b);
		update_bitmap	= true;
		update_hist		= true;
	}

};

LRESULT ViewBitmap::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			toolbar.Create(*this, NULL, CHILD | CLIPSIBLINGS | VISIBLE | ToolBarControl::FLAT | ToolBarControl::TOOLTIPS);
			toolbar.SetExtendedStyle(ToolBarControl::DRAWDDARROWS);
			toolbar.Init(IDR_TOOLBAR_BITMAP);

			//toolbar.Indent(100);
			//StaticControl	stat(toolbar, text, CHILD | VISIBLE | SS_LEFT, 0, Rect(2, 0, 96, 28));
			//stat.SetFont(win::Font::Caption());

			if (false) {
				static int ids[] = {
					ID_BITMAP_GAMMA, ID_BITMAP_MIN, ID_BITMAP_MAX,
					//ID_BITMAP_CHANNEL_RED, ID_BITMAP_CHANNEL_GREEN, ID_BITMAP_CHANNEL_BLUE, ID_BITMAP_CHANNEL_ALPHA
				};
				for (int i = 0; i < num_elements(ids); i++) {
					ToolBarControl::Item	item;
					item.Get(toolbar, ids[i]);
					item.Style(BTNS_DROPDOWN);
					item.Width(toolbar.GetItemRect(i).Width() + 8).Set(toolbar, ids[i]);
				}
			}

			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);

			if (flags.test(HAS_MIPS))
				toolbar.CheckButton(ID_BITMAP_MIPS, true);
			else
				toolbar.EnableButton(ID_BITMAP_MIPS, false);

			toolbar.EnableButton(ID_BITMAP_SLICES, bitmap_depth > 1);

			return 0;

		case WM_SIZE: {
			Point	size(lParam);
			ControlArrangement::GetRects(ToolbarArrange, Rect(Point(0, 0), size), rects);
			toolbar.Move(rects[0]);
			if (flags.test(AUTO_SCALE))
				Autoscale(rects[1].Width(), rects[1].Height());
			if (!target.Resize(size))
				DiscardDeviceResources();
			Invalidate();
			return 0;
		}

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			if (CreateDeviceResources() && !target.Occluded()) {
				if (update_bitmap) {
					UpdateBitmap(target, d2d_bitmap);
					update_bitmap = false;
				}
				if (update_hist && flags.test(DISP_HIST)) {
					UpdateHist();
					update_hist = false;
				}
				target.BeginDraw();
				Paint();
				if (target.EndDraw())
					DiscardDeviceResources();
			}
			return 0;
		}

		case WM_ISO_TIMER:
			frame			= (frame + 1) % anim->Count();
			Timer::Next((*anim)[frame].b);
			bm				= ISO_ptr_machine<bitmap64>(0, *(*anim)[frame].a.get());
			update_bitmap	= true;
			update_hist		= true;
			Invalidate();
			return 0;

		case WM_ISO_CONTEXTMENU:
			contextmenu = HMENU(wParam);
			break;

		case WM_CONTEXTMENU: {
			Point	pt(lParam);
			if (contextmenu) {
				if (int id = contextmenu.Track(*this, pt, TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_RETURNCMD)) {
					auto	param = make_pair(contextmenu.GetItemParamByID(id), ClientToTexel(ToClient(pt)));
					Parent()(WM_COMMAND, id, (LPARAM)&param);
				}
			} else {
				Menu(IDR_MENU_BITMAP).GetSubMenuByPos(0).Track(*this, pt, TPM_NONOTIFY | TPM_RIGHTBUTTON);
			}
			return 0;
		}

		case WM_LBUTTONDOWN:
			if (wParam & MK_CONTROL) {
				Point	p = ClientToTexel(Point(lParam));
				selection = Rect(p, p);
			}
		case WM_RBUTTONDOWN:
			SetFocus();
		case WM_SETFOCUS:
			SetAccelerator(*this, IDR_ACCELERATOR_BITMAP);
			return 0;

		case WM_LBUTTONUP:
			drag = DRAG_OFF;
			break;

		case WM_LBUTTONDBLCLK: {
			int	slice = ClientToSlice(Point(lParam));
			if ((sel_slice >= 0 && slice >= 0 && slice != sel_slice) || flags.flip_test(AUTO_SCALE)) {
				sel_slice = slice;
				Autoscale(rects[1].Width(), rects[1].Height());
			} else {
				zoom		= 1;
				pos			= Point(0, 0);
				sel_slice	= -1;
			}
			Invalidate();
			return 0;
		}

		case WM_MOUSEMOVE: {
			Point	mouse(lParam);

			if (prevbutt != wParam) {
				prevmouse	= mouse;
				prevbutt	= wParam;
			}

			int		slice	= -1;
			if (OverHistogram(mouse) || (slice = ClientToSlice(mouse)) >= 0) {
				TrackMouse(TME_LEAVE);
				tooltip.Activate(*this);
			}

			DRAG	would_drag = drag;
			if (wParam & MK_CONTROL)
				drag = DRAG_SELECTION;
			else if (drag == DRAG_SELECTION)
				drag = DRAG_OFF;

			if (!drag) {
				if (slice >= 0) {
					would_drag = DRAG_BITMAP;
					if (flags.test(DISP_GRID)) {
						d2d::point	p = TexelToClient(grid);
						if (abs(mouse.x - p.x) < 3)
							would_drag = abs(mouse.y - p.y) < 3 ? DRAG_GRID_SIZE : DRAG_GRID_WIDTH;
						else if (abs(mouse.y - p.y) < 3)
							would_drag = DRAG_GRID_HEIGHT;
					}
				} else {
					//histogram4
					if (mouse.y - hist_rect.Top() < 8) {
						if (abs(mouse.x - hist_rect.Left() - disp_range.a * hist_rect.Width()) < 8)
							would_drag = DRAG_MIN;
						else if (abs(mouse.x - hist_rect.Left() - disp_range.b * hist_rect.Width()) < 8)
							would_drag = DRAG_MAX;
					} else if (mouse.x - hist_rect.Right() > -8) {
						would_drag = mouse.y - hist_rect.Bottom() > -8 ? DRAG_HIST_SIZE : DRAG_HIST_WIDTH;
					} else if (mouse.y - hist_rect.Bottom() > -8) {
						would_drag = DRAG_HIST_HEIGHT;
					} else {
						would_drag = DRAG_HIST;
					}
				}
			}

			if (would_drag) {
				ID	cursor;
				switch (would_drag) {
					case DRAG_HIST:
					case DRAG_BITMAP:		cursor = IDC_HAND;		break;
					case DRAG_MIN:
					case DRAG_MAX:
					case DRAG_GRID_WIDTH:
					case DRAG_HIST_WIDTH:	cursor = IDC_SIZEWE;	break;
					case DRAG_GRID_HEIGHT:
					case DRAG_HIST_HEIGHT:	cursor = IDC_SIZENS;	break;
					case DRAG_SELECTION:
					case DRAG_HIST_SIZE:
					case DRAG_GRID_SIZE:	cursor = IDC_SIZEALL;	break;
				}
				SetCursor(LoadCursor(NULL, cursor));
			}

			if (wParam & MK_LBUTTON) {
				switch (drag = would_drag) {
					case DRAG_BITMAP:		pos					+= mouse - prevmouse; flags.clear(AUTO_SCALE); break;
					case DRAG_MIN:			disp_range.a		= clamp((mouse.x - hist_rect.Left()) / float(hist_rect.Width()), 0, 1); update_bitmap = bm.IsType<HDRbitmap64>(); break;
					case DRAG_MAX:			disp_range.b		= clamp((mouse.x - hist_rect.Left()) / float(hist_rect.Width()), 0, 1); update_bitmap = bm.IsType<HDRbitmap64>(); break;
					case DRAG_HIST:			hist_rect			+= mouse - prevmouse;	break;
					case DRAG_HIST_SIZE:	hist_rect.bottom	+= mouse.y - prevmouse.y;
					case DRAG_HIST_WIDTH:	hist_rect.right		+= mouse.x - prevmouse.x;	break;
					case DRAG_HIST_HEIGHT:	hist_rect.bottom	+= mouse.y - prevmouse.y;	break;
					case DRAG_GRID_SIZE:	grid.y				= max(ClientToTexel(mouse).y, 4);
					case DRAG_GRID_WIDTH:	grid.x				= max(ClientToTexel(mouse).x, 4); break;
					case DRAG_GRID_HEIGHT:	grid.y				= max(ClientToTexel(mouse).y, 4); break;
					case DRAG_SELECTION:	selection.BottomRight() = ClientToTexel(mouse); update_hist = true; break;
				}
				prevmouse	= mouse;
				Invalidate();
			}
			tooltip.Track();
			return 0;
		}

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, *this, false);
			return 0;

		case WM_MOUSEWHEEL:
			if (GetRect().Contains(Point(lParam))) {
				d2d::point	pt		= ToClient(Point(lParam));
				float	mult	= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				zoom	*= mult;
				pos		= pt + (pos - pt) * mult;
				flags.clear(AUTO_SCALE);
				Invalidate();
				return 0;
			}
			break;

		case WM_VSCROLL:
			if (Control c = lParam) {
				uint16	id = c.id.get();
				switch (id) {
					case ID_BITMAP_GAMMA:
						gamma = TrackBarControlF(c).GetPos() * (max_gamma - min_gamma) + min_gamma;
						break;
					case ID_BITMAP_MIN:
						disp_range.a = TrackBarControlF(c).GetPos();
						break;
					case ID_BITMAP_MAX:
						disp_range.b = TrackBarControlF(c).GetPos();
						break;
				}
				tooltip.Activate(*this);
				tooltip.Invalidate();
				tooltip.Track();
				update_bitmap	= bm.IsType<HDRbitmap64>();
				Invalidate();
			}
			return 0;

		case WM_COMMAND: {
			bool	shift	= !!(GetKeyState(VK_SHIFT) & 0x8000);
			switch (uint16 id = LOWORD(wParam)) {
#ifndef ISO_EDITOR
				case ID_EDIT: {	// from main
					ISO::Browser2&			b	= *(ISO::Browser2*)lParam;
					ISO_ptr_machine<void>	p	= b;
					if (SetBitmap(p)) {
						d2d_bitmap.clear();
						update_bitmap = true;
						Invalidate();
						Update();
						return true;
					}
					break;
				}
#endif
				case ID_EDIT_SELECT: {
					ISO::Browser b	= *(ISO::Browser*)lParam;
					if (b.Is("SpriteData")) {
						struct SpriteRect { int x, y, w, h; };
						SpriteRect	*r = b;
						selection = Rect(r->x, r->y, r->w, r->h);
						Invalidate();
						return true;
					}
					break;
				}

				case ID_BITMAP_CHANNEL_RED:			SetChannels(shift ? channels ^ CHAN_R : channels == CHAN_R ? CHAN_RGB : CHAN_R); break;
				case ID_BITMAP_CHANNEL_GREEN:		SetChannels(shift ? channels ^ CHAN_G : channels == CHAN_G ? CHAN_RGB : CHAN_G); break;
				case ID_BITMAP_CHANNEL_BLUE:		SetChannels(shift ? channels ^ CHAN_B : channels == CHAN_B ? CHAN_RGB : CHAN_B); break;
				case ID_BITMAP_CHANNEL_ALPHA:		SetChannels(shift ? channels ^ CHAN_A : channels == CHAN_RGB ? CHAN_RGBA : channels == CHAN_A ? CHAN_RGB : CHAN_A); break;
				case ID_BITMAP_CHANNEL_INDEX:		SetChannels(CHAN_INDEX);			break;
				case ID_BITMAP_CHANNEL_ALPHAMASK:	SetChannels(CHAN_A + CHAN_MASK);	break;
				case ID_BITMAP_CHANNEL_DEPTH:		SetChannels(CHAN_DEPTH);			break;
				case ID_BITMAP_CHANNEL_ALL:			SetChannels(CHAN_RGB); 				break;

				case ID_BITMAP_BILINEAR:
					toolbar.CheckButton(id, flags.flip_test(BILINEAR));
					Invalidate();
					break;

				case ID_BITMAP_AUTOCONTRAST: {
					float		c0 = 1e38f, c1 = -c0;
					for (int i = 0; i < 4; i++) {
						if (channels & bit(i) && chan_range.a[i] != chan_range.b[i]) {
							c0 = min(c0, chan_range.a[i]);
							c1 = max(c1, chan_range.b[i]);
						}
					}
					disp_range.a	= (c0 - rgb_range.a) / (rgb_range.b - rgb_range.a);
					disp_range.b	= (c1 - rgb_range.a) / (rgb_range.b - rgb_range.a);
					update_bitmap	= bm.IsType<HDRbitmap64>();

					if (TrackBarControlF t = ChildByID(ID_BITMAP_MIN))
						t.SetPos(disp_range.a);
					if (TrackBarControlF t = ChildByID(ID_BITMAP_MAX))
						t.SetPos(disp_range.b);

					Invalidate();
					break;
				}

				case ID_BITMAP_MIPS:
					toolbar.CheckButton(id, flags.flip_test(SHOW_MIPS));
					UpdateBitmapRect();
					Invalidate();
					break;

				case ID_BITMAP_GAMMA:
					if (Control c = ChildByID(id)) {
						c.Destroy();
						toolbar.CheckButton(id,	false);
					} else {
						TrackBarControlF	t = DropDownTrackBar(toolbar, id, 200);
						t.SetPos((gamma - min_gamma) / (max_gamma - min_gamma));
						t.SetTick((1.0f - min_gamma) / (max_gamma - min_gamma));
						t.SetTick((2.2f - min_gamma) / (max_gamma - min_gamma));
					}
					break;

				case ID_BITMAP_MIN:
					if (Control c = ChildByID(id)) {
						c.Destroy();
						toolbar.CheckButton(id,	false);
					} else {
						DropDownTrackBar(toolbar, id, 200).SetPos(disp_range.a);
					}
					break;

				case ID_BITMAP_MAX:
					if (Control c = ChildByID(id)) {
						c.Destroy();
						toolbar.CheckButton(id,	false);
					} else {
						DropDownTrackBar(toolbar, id, 200).SetPos(disp_range.b);
					}
					break;

				case ID_BITMAP_SAVEAS: {
					buffer_accum<1024>	ba;
					for (FileHandler::iterator i = FileHandler::begin(); i != FileHandler::end(); ++i) {
						if (i->GetCategory() == cstr("bitmap")) {
							if (const char *des = i->GetDescription()) {
								ba.format("%s (*.%s)", des, i->GetExt()) << "\0";
								ba.format("*.%s", i->GetExt()) << "\0";
							}
						}
					}
//					if (GetSave(hWnd, fn, "Save As", "Image Files\0*.bmp;*.jpg;*.tga\0All Files (*.*)\0*.*\0"))
					if (GetSave(hWnd, fn, "Save As", ba.term()))
						Busy(), FileHandler::Write(bm, fn);
					break;
				}

				case ID_BITMAP_UPDOWN:
					toolbar.CheckButton(id, flags.flip_test(FLIP_Y));
					Invalidate();
					break;

				case ID_BITMAP_LEFTRIGHT:
					toolbar.CheckButton(id, flags.flip_test(FLIP_X));
					Invalidate();
					break;

				case ID_BITMAP_HISTOGRAM:
					toolbar.CheckButton(id, flags.flip_test(DISP_HIST));
					Invalidate();
					break;

				case ID_BITMAP_GRID:
					toolbar.CheckButton(id, flags.flip_test(DISP_GRID));
					Invalidate();
					break;

				case ID_BITMAP_SLICES:
					toolbar.CheckButton(id, flags.flip_test(SEPARATE_SLICES));
					UpdateBitmapRect();
					Invalidate();
					break;

				case ID_EDIT_COPY:
					if (bm) {
						Clipboard	c(*this);
						if (c.Empty()) {
							bitmap64	*bm = this->bm;
							
							block<ISO_rgba, 2> texels = selection.Width() && selection.Height()
								? bm->Block(selection.Left(), selection.Top(), selection.Width(), selection.Height())
								: bm->All();

							global_base	glob(DIB::CalcSize(texels.size<1>(), texels.size<2>(), 32, 1, bm->ClutSize()));
							DIB::Init(glob.data(), texels, bm->ClutBlock());
							c.Set(CF_DIB, glob);
						}
					}
					break;

				default:
					return 0;//Parent()(message, wParam, lParam);
			}
			return 0;
		}

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA:
					if (nmh->hwndFrom == tooltip) {
						NMTTDISPINFOA	*nmtdi = (NMTTDISPINFOA*)nmh;
						fixed_accum		sa(tooptip_text);//nmtdi->szText);
						nmtdi->lpszText	= tooptip_text;
						Control			c = ChildAt(GetMousePos());
						switch (c.id.get()) {
							case ID_BITMAP_GAMMA:
								sa << gamma;
								break;
							case ID_BITMAP_MIN:
								sa << disp_range.a;
								break;
							case ID_BITMAP_MAX:
								sa << disp_range.b;
								break;
							default: {
								Point	pt		= ToClient(GetMousePos());
								if (OverHistogram(pt)) {
									if (bm.IsType<HDRbitmap64>()) {
										sa.format("Histogram[%f]", (pt.x - hist_rect.Left()) * (rgb_range.b - rgb_range.a) / hist_rect.Width() + rgb_range.a);
									} else {
										int	i = (pt.x - hist_rect.Left()) * 256 / hist_rect.Width();
										sa.format("Histogram[%i]", i);
									}
									break;
								} else if (drag == DRAG_SELECTION) {
									const Point	&p0 = selection.TopLeft();
									const Point	&p1 = selection.BottomRight();
									sa.format("Select (%i,%i) - (%i,%i) w:%i, h:%i", p0.x, p0.y, p1.x, p1.y, abs(p1.x - p0.x), abs(p1.y - p0.y));
									break;
								}

								d2d::point	p	= ClientToTexel0(pt - rects[1].TopLeft());
								int		slice	= CalcSlice(p);
								if (slice < 0) {
									tooltip.Activate(*this, false);
									break;
								}

								DumpTexelInfo(sa, p.x, p.y, slice, flags.test(SHOW_MIPS) ? CalcMip(p) : 0);
								break;
							}
						}

					} else {
						TOOLTIPTEXT	*ttt	= (TOOLTIPTEXT*)nmh;
						ttt->hinst			= GetDefaultInstance();
						ttt->lpszText		= MAKEINTRESOURCE(ttt->hdr.idFrom);
					}
					break;
			}
			return 0;
		}

		case WM_NCDESTROY:
			tooltip.Destroy();
			delete this;
			return 0;
	}
	return Super(message, wParam, lParam);
}

void ViewBitmap::Paint() {
	target.SetTransform(identity);
	target.Fill(rects[1], checker);

	DrawBitmapPaint(target, rects[1], d2d_bitmap);

	// selection
	if (selection.Width() && selection.Height())
		DrawSelection(target, selection);

	// draw histogram4
	if (flags.test(DISP_HIST)) {
		DrawHistogram(target, hist_rect, hist, channels);

		// gamma curve
		DrawGammaCurve(target, hist_rect);

		// markers
		marker.Draw(target, d2d::point(hist_rect.Left() + disp_range.a * hist_rect.Width(), hist_rect.Top()), colour(0,0,0));
		marker.Draw(target, d2d::point(hist_rect.Left() + disp_range.b * hist_rect.Width(), hist_rect.Top()), colour(1,1,1));
	}
}

filename	ViewBitmap::fn;
const float	ViewBitmap::min_gamma = 0.5f, ViewBitmap::max_gamma = 3.0f;

class ViewThumbnail : public Window<ViewThumbnail> {
	ISO_ptr_machine<void>	p;
	int						width, height;
	d2d::WND				target;
	d2d::Write				write;
	d2d::BitmapBrush		checker;
	com_ptr<ID2D1Bitmap>	d2d_bitmap;
	string16				text;

	bool CreateDeviceResources() {
		target.Init(hWnd, GetClientRect().Size());
		if (!d2d_bitmap) {
			try {
			if (p.IsType<bitmap>()) {
				bitmap	*bm = p;
				target.CreateBitmap(&d2d_bitmap, bm->Base());
			} else {
				HDRbitmap *bm = p;
				target.CreateBitmap(&d2d_bitmap, bm->Base());
			}
			} catch (HRESULT hr) {
				MessageBoxA(*this, (string)com_error(hr).Description(), "Error", MB_OK);
			}
		}
		return checker.CreateDeviceResources(target);
	}
	void DiscardDeviceResources() {
		d2d_bitmap.clear();
		checker.DiscardDeviceResources();
		target.DeInit();
	}

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE:
				if (!target.Resize(Point(lParam)))
					DiscardDeviceResources();
				Invalidate();
				return 0;

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				if (CreateDeviceResources() && !target.Occluded()) {
					Rect	rect	= GetClientRect();

					target.BeginDraw();
					target.SetTransform(identity);
					target.Fill(rect, checker);

					if (width * rect.Height() > height * rect.Width()) {
						float zoom = float(rect.Width()) / width;
						target.SetTransform(scale(zoom) * translate(0, (rect.Height() / zoom - height) / 2));
					} else {
						float zoom = float(rect.Height()) / height;
						target.SetTransform(scale(zoom) * translate((rect.Width() / zoom - width) / 2, 0));
					}
					target.Draw(Rect(0, 0, width, height), d2d_bitmap);

					if (text) {
						d2d::Font		font(write, L"arial", 10);
						d2d::SolidBrush	black(target, colour(0,0,0)), white(target, colour(1,1,1));
						target.SetTransform(identity);
						target.DrawText(rect, text, font, black);
						target.DrawText(rect + Point(1, 1), text, font, white);
					}

					if (target.EndDraw())
						DiscardDeviceResources();
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

	ViewThumbnail(const WindowPos &wpos, const ISO_ptr_machine<void> &p, const char *text) : p(p), checker(Bitmap::Load(IDB_CHECKER, LR_CREATEDIBSECTION), float2x3(scale(10))), text(text) {
		if (p.IsType<bitmap>()) {
			bitmap	*bm = p;
			width		= bm->BaseWidth();
			height		= bm->Height();
		} else {
			HDRbitmap *bm = p;
			width		= bm->BaseWidth();
			height		= bm->Height();
		}
		Create(wpos, tag(p.ID()), CHILD | CLIPCHILDREN, CLIENTEDGE);
	}
};

Control ThumbnailWindow(const WindowPos &wpos, const ISO_ptr_machine<void> &p, const char *text) {
	return *new ViewThumbnail(wpos, p, text);
}
Control BitmapWindow(const WindowPos &wpos, const ISO_ptr_machine<void> &p, const char *text, bool auto_scale) {
	return *new ViewBitmap(wpos, p, text, auto_scale);
}

class EditorBitmap : public Editor {
	virtual bool Matches(const ISO::Browser &b) {
		return ViewBitmap_base::Matches(b.GetTypeDef());
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		return *new ViewBitmap(wpos, p, 0, false);
	}
} editorbitmap;

void MakeThumbnail(void *dest, const block<ISO_rgba, 2> &block, int w, int h) {
	int			w0 = block.size<1>(), h0 = block.size<2>();
	int			w1, h1;
	if (w0 * h > h0 * w) {
		w1	= w;
		h1	= w * h0 / w0;
	} else {
		w1	= h * w0 / h0;
		h1	= h;
	}
	int			x0	= (w - w1) / 2;
	for (int y = 0; y < h; y++) {
		Texel<B8G8R8A8>		*d	= (Texel<B8G8R8A8>*)dest + (h - 1 - y) * w + x0;
		int			yo	= y - (h - h1) / 2;
		if (yo >= 0 && yo < h1) {
			auto	*s	= block[yo * h0 / h1].begin();
			for (int x = 0; x < w1; x++)
				*d++ = ISO_rgba(s[x * w0 / w1], 255);
		}
	}
}

void MakeThumbnail(void *dest, const block<HDRpixel, 2> &block, int w, int h) {
	int			w0 = block.size<1>(), h0 = block.size<2>();
	int			w1, h1;
	if (w0 * h > h0 * w) {
		w1	= w;
		h1	= w * h0 / w0;
	} else {
		w1	= h * w0 / h0;
		h1	= h;
	}
	int			x0	= (w - w1) / 2;
	for (int y = 0; y < h; y++) {
		Texel<B8G8R8A8>		*d	= (Texel<B8G8R8A8>*)dest + (h - 1 - y) * w + x0;
		int			yo	= y - (h - h1) / 2;
		if (yo >= 0 && yo < h1) {
			auto	*s	= block[yo * h0 / h1].begin();
			for (int x = 0; x < w1; x++)
				*d++ = s[x * w0 / w1].WithAlpha(1);
		}
	}
}

bool MakeThumbnail(void *dest, const ISO_ptr_machine<void> &p, int w, int h) {
	if (p.IsType<bitmap>()) {
		bitmap		*bm	 = p;
		MakeThumbnail(dest, bm->Slice(0), w, h);
		return true;
	}
	if (p.IsType<bitmap64>()) {
		bitmap64	*bm	 = p;
		MakeThumbnail(dest, bm->Slice(0), w, h);
		return true;
	}
	if (p.IsType<HDRbitmap>()) {
		HDRbitmap	*bm	 = p;
		MakeThumbnail(dest, bm->Slice(0), w, h);
		return true;
	}
	if (p.IsType<HDRbitmap64>()) {
		HDRbitmap64	*bm	 = p;
		MakeThumbnail(dest, bm->Slice(0), w, h);
		return true;
	}
	return false;
}

bool MakeThumbnail(void *dest, const ISO_ptr_machine<void> &p, const POINT &size) {
	return MakeThumbnail(dest, p, size.x, size.y);
}

} //namespace app
