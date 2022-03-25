#include "window.h"
#include <uxtheme.h>
#include <vssym32.h>

#pragma comment(lib, "UxTheme.lib")

namespace iso { namespace win {

void PaintCustomCaption(DeviceContext &dc, const char16 *title, const Rect &rect) {
	if (HTHEME theme = OpenThemeData(NULL, L"CompositedWindow::Window")) {
		if (DeviceContext paint = dc.Compatible()) {
			if (Bitmap bm = Bitmap::CreateDIBSection(rect.Width(), rect.Height(), 32)) {
				Bitmap	old	= paint.Select(bm);

				// Setup the theme drawing options.
				DTTOPTS	opts	= {sizeof(DTTOPTS)};
				opts.dwFlags	= DTT_COMPOSITED | DTT_GLOWSIZE;
				opts.iGlowSize	= 15;

				// Select a font.
				win::Font::Params16	font;
				win::Font			oldfont = SUCCEEDED(GetThemeSysFont(theme, TMT_CAPTIONFONT, &font)) ? paint.Select(win::Font(font)) : Font(HFONT(0));

				// Draw the title.
				DrawThemeTextEx(theme, paint, 0, 0, title, -1, DT_LEFT | DT_WORD_ELLIPSIS, const_cast<Rect*>(&rect), &opts);

				// Blit text to the frame.
				dc.Blit(paint, Point(0,0), rect);

				DeleteObject(paint.Select(old));
				if (oldfont)
					paint.Select(oldfont);
			}
			paint.Destroy();
		}
		CloseThemeData(theme);
	}
}

void PaintCustomCaption(DeviceContext &dc, const char16 *title, const Rect &rect, HFONT font, win::Colour col, int glow, int textflags) {
	if (HTHEME theme = OpenThemeData(NULL, L"CompositedWindow::Window")) {
		if (DeviceContext paint = dc.Compatible()) {
			if (Bitmap bm = Bitmap::CreateDIBSection(rect.Width(), rect.Height(), 32)) {
				Bitmap old	= paint.Select(bm);
				//paint.Fill(Rect(0, 0, rect.Width(), rect.Height()), Brush::White());

				// Setup the theme drawing options.
				DTTOPTS	opts	= {sizeof(DTTOPTS)};
				opts.crText		= col;
				opts.dwFlags	= DTT_COMPOSITED | DTT_GLOWSIZE | DTT_TEXTCOLOR;
				opts.iGlowSize	= glow;

				// Select a font.
				HFONT	oldfont = paint.Select(font);

				// Draw the title.
				Rect	rect2(glow, glow, rect.Width(), rect.Height() - 2 * glow);
				DrawThemeTextEx(theme, paint, 0, 0, title, -1, textflags, &rect2, &opts);

				// Blit text to the frame.
				dc.Blit(paint, Point(0,0), rect);

				DeleteObject(paint.Select(old));
				paint.Select(oldfont);
			}
			paint.Destroy();
		}
		CloseThemeData(theme);
	}
}

} } // namespace iso::dwm
