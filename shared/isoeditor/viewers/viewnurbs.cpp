#include "main.h"
#include "windows/window.h"
#include "iso/iso.h"

using namespace app;

namespace iso {
void rbasis(int c, float t, int n, const float *k, float *r);
}

float EvaluateNURBS(float t, ISO_openarray<float[2]> &p) {
	int	n = p.Count();
	int	i = 0;
	while (i < n - 2 && t > p[i+1][0])
		i++;

	float	t0 = p[i][0];
	float	t1 = p[i+1][0];
	float	t2 = p[i+2][0];
	float	v0 = p[i][1];
	float	v1 = p[i+1][1];
	float	v2 = p[i+2][1];
	float	va =  v0 + (v1 - v0) * (t - t0) / (t1 - t0);
	float	vb =  v1 + (v2 - v1) * (t - t1) / (t2 - t1);
	return va + (vb - va) * (t - t0) / (t2 - t0);
}

class ViewNURBS : public Window<ViewNURBS> {
	ISO_openarray<float[2]>	*p;
	float	mink, maxk;
	float	scale;
	float	offset;
	int		selected;
	Point	mouse;

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				break;

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				Rect		client	= GetClientRect();
				dc.Fill(dc.rcPaint, (HBRUSH)(COLOR_3DFACE + 1));

				offset	= mink;
				scale	= client.Width() / (maxk - mink);

				int		n	= p->Count();
				float	a	= (*p)[0][0], b = (*p)[n - 1][0];
				float	*k	= new float[n + 4];
				float	*r	= new float[n + 4];
				for (int i = 0; i < n + 4; i++)
//					k[i] = (*p)[clamp(i - 2, 0, n - 1)][0];
					k[i] = ((*p)[clamp(i - 2, 0, n - 1)][0] + (*p)[clamp(i - 1, 0, n - 1)][0]) / 2;

				for (int i = 0; i <= 100; i++) {
					float t = a + (b - a) * i / 100;
#if 0
					float	v = EvaluateNURBS(t, *p);
#elif 1
//					rbasis(3, t, n + 3, k, r);
					float	v = 0;
					for (int j = 0; j < n; j++) {
						v += r[j] * (*p)[j][1];
					}
#else
					float	(*k)[2] = *p;
					for (int j = n - 1; j-- && t > k[0][0]; k++);

					float	r0, r1, r2;

					// calculate the second order nonrational basis functions
					r1	= (k[1][0] - t) / (k[1][0] - k[0][0]);
					r0	= (t - k[0][0]) / (k[1][0] - k[0][0]);	// = 1 - r1

					// calculate the third order nonrational basis functions
					r2	= (k[1][0] - t) * r1 / (k[1][0] - k[-1][0]);
					r1	= (t - k[-1][0]) * r1 / (k[1][0] - k[-1][0]) + (k[2][0] - t) * r0 / (k[2][0] - k[0][0]);
					r0	= (t - k[0][0]) * r0 / (k[2][0] - k[0][0]);

					float	v = r0 * k[0][1] + r1 * k[-1][1] + r2 * k[-2][1];
#endif
					if (i == 0)
						dc.MoveTo(int((t - offset) * scale), int(v * scale), NULL);
					else
						dc.LineTo(int((t - offset) * scale), int(v * scale));
				}
				delete[] k;
				delete[] r;

				for (int i = 0; i < n; i++) {
					int	x = int(((*p)[i][0] - offset) * scale);
					int	y = int((*p)[i][1] * scale);
					dc.MoveTo(x - 4, y - 4, NULL);
					dc.LineTo(x + 4, y - 4);
					dc.LineTo(x + 4, y + 4);
					dc.LineTo(x - 4, y + 4);
					dc.LineTo(x - 4, y - 4);
				}
				break;
			}
			case WM_LBUTTONDOWN: {
				Point	pt(lParam);
				int		n	= p->Count();
				selected	= -1;
				for (int i = 0; i < n; i++) {
					int	x = int(((*p)[i][0] - offset) * scale);
					int	y = int((*p)[i][1] * scale);
					if (abs(x - pt.x) <= 4 && abs(y - pt.y) <= 4) {
						mouse	= pt;
						selected = i;
						break;
					}
				}
				break;
			}
			case WM_MOUSEMOVE: {
				if (wParam & MK_LBUTTON && selected >= 0) {
					Point	pt(lParam);
					(*p)[selected][0] = pt.x / scale + offset;
					(*p)[selected][1] = pt.y / scale;
					Invalidate();
					Parent().Invalidate();
				}
				break;
			}
			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}
	ViewNURBS(const WindowPos &wpos, ISO_ptr<void> &_p) : p(_p) {
		int		n	= p->Count();
		mink = (*p)[0][0];
		maxk = (*p)[n - 1][0];
		Create(wpos, NULL, WS_CHILD | WS_VISIBLE, WS_EX_CLIENTEDGE);
	}
};

class EditorNURBS : public Editor {
	virtual bool Matches(const ISO_type *type) {
		if (type->SameAs<ISO_openarray<float[2]> >())
			return true;
		return false;
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, ISO_ptr<void> &p) {
		return *new ViewNURBS(wpos, p);
	}
} editornurbs;
