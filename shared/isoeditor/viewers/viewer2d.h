#include "base/vector.h"
#include "windows/d2d.h"

using namespace app;


class Viewer2D : public Inherit<Viewer2D, d2d::Window> {
public:
	float				zoom;
	d2d::point			pos;
	Point				mouse;

	auto	transformation()	const { return translate(pos) * scale(zoom); }

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_PAINT: {
				//DeviceContextPaint	dc(*this);
				if (BeginDraw()) {
					Clear(colour(0.5f,0.5f,0.5f));

					d2d::PAINT_INFO(*this, this).Send(*this);

					if (EndDraw())
						DeInit();
				}
				break;
			}

			case WM_ERASEBKGND:
				return TRUE;

			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
				mouse	= Point(lParam);
				SetFocus();
				break;

			case WM_MOUSEMOVE:
				if (wParam & MK_LBUTTON) {
					Point mouse2(lParam);
					pos		+= mouse2 - mouse;
					mouse	= mouse2;
					Invalidate();
				}
				break;

			case WM_MOUSEWHEEL: {
				Point		pt		= ToClient(Point(lParam));
				position2	pt2(pt.x, pt.y);
				float		mult	= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				zoom		*= mult;
				pos			= pt2 - ((position2)(float2)pos - pt2) * mult;
				Invalidate();
				break;
			}

//			case WM_NCDESTROY:
//				delete this;
//				break;
		}
		return Super(message, wParam, lParam);
	}


	Viewer2D(const WindowPos &wpos, const char *title) : zoom(1), pos(0,0) {
		Create(wpos, title, CHILD | VISIBLE, CLIENTEDGE);
	}
	Viewer2D() : zoom(1), pos(0,0) {}
};
