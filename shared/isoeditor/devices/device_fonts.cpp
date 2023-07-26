#include "main.h"
#include "windows/d2d.h"
#include "devices/device.h"

using namespace app;

//-----------------------------------------------------------------------------
//	FontsDevice
//-----------------------------------------------------------------------------

template<typename T> bool EnumerateFonts(T *t, HDC hdc, LOGFONT &lf) {
	return !!EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)callback_function_end<int(const LOGFONT*, const TEXTMETRIC*, DWORD),T>(), (LPARAM)t, 0);
}

ISO_DEFUSERCOMPV(LOGFONT,lfHeight,lfWidth,lfEscapement,lfOrientation,lfWeight,lfItalic,lfUnderline,lfStrikeOut,lfCharSet,lfOutPrecision,lfClipPrecision,lfQuality,lfPitchAndFamily);

struct FontsDevice : DeviceT<FontsDevice>, DeviceCreateT<FontsDevice> {
	filename	fn;
	void			operator()(const DeviceAdd &add) {
		add("Fonts", this, LoadPNG("IDB_DEVICE_FONTS"));
	}

	ISO_ptr<void>	operator()(const Control &main) {
		struct Enumerator {
			ISO_ptr<ISO_openarray<ISO_ptr<LOGFONT> > >	p;
			int		operator()(const LOGFONT *lf, const TEXTMETRIC *tm, DWORD type) {
				if (lf->lfFaceName[0] != '@')
					p->Append(ISO_ptr<LOGFONT>(lf->lfFaceName, *lf));
				return 1;
			}
			Enumerator() : p("Fonts") {
				LOGFONT	lf;
				clear(lf);
				//lf.lfCharSet = DEFAULT_CHARSET;
				EnumerateFonts(this, DeviceContext::Screen(), lf);
			}
		} e;
		return e.p;
	}
} fonts_device;

//-----------------------------------------------------------------------------
//	ViewFont
//-----------------------------------------------------------------------------

struct ViewFont : public Window<ViewFont> {
	d2d::WND	target;
	d2d::Write	write;
	d2d::Font	label;
	LOGFONT		*lf;
	TextMetric	tm;
	TScrollInfo<uint32>	si;
	uint32		num_chars, num_across;
	float		scale;

	void		SetSize(const Point &size, float scale) {
		num_across	= max(int(size.x / scale), 1);
		si.Set(0, div_round_up(num_chars, num_across) * scale, size.y);
	}

	uint32		Pos2Char(const Point &p) const {
		int	x = p.x / scale;
		int	y = (p.y + si.Pos()) / scale;
		return x + y * num_across;
	}

	Point		Char2Pos(uint32 c) const {
		int	x = c % num_across;
		int	y = c / num_across;
		return Point(x * scale, y * scale - si.Pos());
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE: {
				Point	size(lParam);
				if (!target.Resize(size))
					target.DeInit();

				SetSize(size, scale);
				SetScroll(si);
				break;
			}

			case WM_VSCROLL:
				si.ProcessScroll(wParam);
				SetScroll(si);
				Invalidate();
				break;

			case WM_MOUSEWHEEL:
				if (GetKeyState(VK_CONTROL) & 0x8000) {
					Point	pt		= ToClient(Point(lParam));
					uint32	c		= Pos2Char(pt);

					scale	= max(scale * iso::pow(1.05f, (short)HIWORD(wParam) / 64.f), 16);
					SetSize(GetClientRect().Size(), scale);

					Point	pt2		= Char2Pos(c);
					si.MoveBy(pt2.y - pt.y);
				} else {
					si.MoveBy(-short(HIWORD(wParam)) / WHEEL_DELTA);
				}
				SetScroll(si);
				Invalidate();
				break;

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				Rect	rect = GetClientRect();
				if (target.Init(hWnd, rect.Size()) && !target.Occluded()) {
					target.BeginDraw();
					target.Clear(d2d::colour(1,1,1));

					d2d::SolidBrush	black(target, colour(0,0,0));
					d2d::Font		font(write, str16(lf->lfFaceName), scale * .5f);
					target.SetTransform(identity);

					for (int x = 0; x <= num_across; ++x)
						target.DrawLine(d2d::point(x * scale, 0), d2d::point(x * scale, rect.Height()), black);

					char16		s[2] = {0, 0};
					int		y0	= div_round_down(si.Pos(), scale), y1 = div_round_up(si.Pos() + si.Page(), scale);
					for (int y = y0; y < y1; ++y) {
						float	yf = y * scale - si.Pos();
						target.DrawLine(d2d::point(0, yf), d2d::point(num_across * scale, yf), black);
						for (int x = 0; x < num_across; ++x) {
							d2d::rect	r(x * scale, yf, (x + 1) * scale, yf + scale);
							s[0] = char16(y * num_across + x);
							target.DrawText(r.Subbox(scale / 4, scale / 4, 0, 0), s, font, black);
							if (scale > 32) {
								target.DrawText(r.Subbox(4, 4, 0, 0), str16(format_string("%04x", s[0])), label, black);
							}
						}
					}

					if (target.EndDraw())
						target.DeInit();
				}
				return 0;
			}

			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}
	ViewFont(const WindowPos &wpos, LOGFONT *_lf) : label(write, L"Verdana", 8), lf (_lf), scale(64) {
		DeviceContext	dc = DeviceContext::Screen();
		dc.Select(Font(*lf));
		tm = dc.GetTextMetrics();

		num_chars	= 65536;//tm.tmLastChar;

		Create(wpos, lf->lfFaceName, CHILD | VISIBLE);
	}
};

class EditorFont : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is<LOGFONT>();
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void,64> &p) {
		return *new ViewFont(wpos, p);
	}
} editorfont;

//-----------------------------------------------------------------------------
//	ViewFonts
//-----------------------------------------------------------------------------

struct ViewFonts : public Window<ViewFonts> {
	d2d::WND	target;
	d2d::Write	write;
	d2d::Font	label;
	ScrollInfo	si;
	string16	text;
	float		scale;

	ISO_ptr<ISO_openarray<ISO_ptr<LOGFONT> > > fonts;

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE:
				if (!target.Resize(win::Point(lParam)))
					target.DeInit();
				si.SetPage(win::Point(lParam).y);
				SetScroll(si);
				break;

			case WM_VSCROLL:
				si.ProcessScroll(wParam);
				SetScroll(si);
				Invalidate();
				break;

			case WM_MOUSEWHEEL:
				if (GetKeyState(VK_CONTROL) & 0x8000) {
					Point	pt		= ToClient(Point(lParam));
					float	mult	= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
					scale	*= mult;
					si.Range(0, fonts->Count() * scale);
					si.MoveTo((si.Pos() + pt.y) * mult - pt.y);
				} else {
					si.MoveBy(-short(HIWORD(wParam)) / WHEEL_DELTA);
				}
				SetScroll(si);
				Invalidate();
				break;

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				Rect				client = GetClientRect();
				if (target.Init(hWnd, client.Size()) && !target.Occluded()) {
					target.BeginDraw();
					target.Clear(d2d::colour(1,1,1));

					d2d::SolidBrush	black(target, colour(0,0,0));
					target.SetTransform(identity);

					int		i0	= div_round_down(si.Pos(), scale), i1 = min(div_round_up(si.Pos() + si.Page(), scale), fonts->Count());
					for (int i = i0; i < i1; ++i) {
						LOGFONT *lf	= (*fonts)[i];
						Rect	rect(0, i * scale - si.Pos(), 100000, scale);
						target.DrawText(rect, &lf->lfFaceName[0], label, black);
						target.DrawText(rect.Subbox(100,0,0,0), text, d2d::Font(write, str16(lf->lfFaceName), scale * .8f), black);
						//target.DrawText(rect.Subbox(100,0,0,0), text, d2d_fonts[i], black);
					}

					if (target.EndDraw())
						target.DeInit();
				}
				return 0;
			}

			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}
	ViewFonts(const WindowPos &wpos, const ISO_ptr<void> &p) : label(write, L"Verdana", 8), text(L"The quick brown fox jumps over the lazy dog. 0123456789"), scale(20), fonts(p) {
		si.Range(0, fonts->Count() * scale);
		Create(wpos, p.ID().get_tag(), CHILD | VISIBLE);
	}
};

class EditorFonts : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is<ISO_openarray<ISO_ptr<LOGFONT> > >();
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void,64> &p) {
		return *new ViewFonts(wpos, p);
	}

	virtual ID GetIcon(const ISO_ptr<void> &p) {
		return "IDB_DEVICE_FONTS";
	}
} editorfonts;