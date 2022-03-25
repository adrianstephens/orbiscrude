#include "main.h"
#include "iso/iso.h"
#include "geometry.h"
#include "SceneGraph.h"

using namespace app;

namespace iso {
void rbasis(int c, float t, int n, const float *k, float *r);
}

struct Knot : e_link<Knot> {
	float	x, y, s;
	Knot(float _x, float _y, float _s) : x(_x), y(_y), s(_s)	{}
};

void FindKnots(float *p, int count, int stride, e_list<Knot> &knots)
{
	if (count == 0)
		return;

	float	py = *p;
	float	pdy = 0, pddy = 0;
	float	pm = py;
	bool	pd = false;

	for (int i = 0; i < count; i++, p += stride) {
		float	y	= *p;
		float	dy	= y - py;
		float	ddy	= dy - pdy;
		bool	d	= y > py;

		if (i == 1) {
			knots.push_back(Knot(i - 1, py, y - py));
		} else if (i == count - 1) {
			knots.push_back(Knot(i, y, y - py));
		} else if (d != pd && abs(pm - y) > 0.01f) {
			if (abs(ddy - pddy) > 0.005f) {
				knots.push_back(Knot(i - 1, py, pdy));
				knots.push_back(Knot(i - 1, py, dy));
			} else {
				knots.push_back(Knot(i - 1, py, 0));
			}
			pm	= y;
		}
		py		= y;
		pdy		= dy;
		pddy	= ddy;
		pd		= d;
	}
}

float4 CalcBezier(Knot *k0)
{
	Knot	*k1 = k0->next;
	float	len = k1->x - k0->x;
	return float4(k0->y, k0->y + k0->s * len / 3, k1->y - k1->s * len / 3, k1->y);
}

bool RefineBezier(float *p, int stride, Knot *knot)
{
	float	x0		= knot->x, x1 = knot->next->x;
	float	maxd	= 0, maxx;

	float4	cub	= bezier_spline::blend * CalcBezier(knot);
	for (int i = x0; i < x1; i++) {
		float	t = (i - x0) / (x1 - x0);
		float	y = ((cub.x * t + cub.y) * t + cub.z) * t + cub.w;
		float	d = abs(y - p[i * stride]);
		if (d > maxd) {
			maxd = d;
			maxx = i;
		}
	}

	if (maxd > 0.005f) {
		float	y		= p[int(maxx) * stride];
		float	ny		= p[(int(maxx)+1) * stride];
		float	py		= p[(int(maxx)-1) * stride];
		float	ppy		= p[(int(maxx)-2) * stride];
		float	dy		= ny - y;
		float	pdy		= y - py;
		float	ppdy	= py - ppy;
		float	ddy		= dy - pdy;
		float	pddy	= pdy - ppdy;
		if (abs(ddy - pddy) > 0.005f)
			knot->insert_after(new Knot(maxx, y, dy));
		knot->insert_after(new Knot(maxx, y, pdy));
		return true;
	}
	return false;
}

void BezierToBSpline(e_list<Knot> &bez, e_list<Knot> &bspline)
{
	e_list<Knot>::iterator i0 = bez.begin(), i1 = i0, i2 = i1;

	while (!bspline.empty())
		delete bspline.pop_back();

	float	t0, t1, t2;
	float	len0, len1;
	float	y0, y1, y2;
	float	y001, y011, y012;

	bspline.push_back(new Knot(i0->x, i0->y, 0));
	bspline.push_back(new Knot(i0->x, i0->y, 0));

	while (i0 != bez.end()) {
		i0		= i1;
		i1		= i2;
		if (i2 != bez.end())
			++i2;

		t0		= i0->x;
		t1		= i1->x;
		t2		= i2->x;

//		float	t00		= i0 == bez.begin() ? t0 : i0->prev->x;

		len0	= t1 - t0;
		len1	= t2 - t1;

		y0		= i0->y;
		y1		= i1->y;
		y2		= i2->y;

		y001	= y0 + i0->s * len0 / 3;
		y011	= y1 - i1->s * len0 / 3;
		y012	= t1 == t0 ? y001 : ((t1 - t2) * y001 + (t2 - t0) * y011) / (t1 - t0);

		bspline.push_back(new Knot(t0, y012, 0));

	}
}
#if 1
void BSplineToBezier(e_list<Knot> &bspline, e_list<Knot> &bez)
{
	e_list<Knot>::iterator i0 = bspline.begin(), i1 = i0->next, i2 = i1->next, i3 = i2->next, i4 = i3->next, i5 = i4->next;

	while (!bez.empty())
		delete bez.pop_back();

	while (i0 != bspline.end()) {
		i0		= i1;
		i1		= i2;
		i2		= i3;
		i3		= i4;
		i4		= i5;
		if (i5 != bspline.end())
			++i5;

		float	t0		= i0->x;
		float	t1		= i1->x;
		float	t2		= i2->x;
		float	t3		= i3->x;
		float	t4		= i4->x;
		float	t5		= i5->x;

		float	y012	= i0->y;
		float	y123	= i1->y;
		float	y234	= i2->y;
		float	y345	= i3->y;

		float	y223	= ((t4 - t2) * y123 + (t2 - t1) * y234) / (t4 - t1);
		float	y233	= ((t4 - t3) * y123 + (t3 - t1) * y234) / (t4 - t1);

		float	y212	= ((t3 - t2) * y012 + (t2 - t0) * y123) / (t3 - t0);
		float	y334	= ((t5 - t4) * y234 + (t4 - t2) * y345) / (t5 - t2);

		float	y222	= ((t3 - t2) * y212 + (t2 - t1) * y223) / (t3 - t1);

		bez.push_back(new Knot(t2, y222, (y223 - y222) / (t3 - t2) * 3));
	}
}
#endif
float EvaluateBSpline(e_list<Knot> &bspline, float t)
{
	e_list<Knot>::iterator begin = bspline.begin(), end = bspline.end(), i = begin;
	while (i != bspline.end() && t > i->x) {
		++i;
	}

	Knot	*i4 = i;
	Knot	*i3 = i4 == begin ? i4 : i4->prev;
	Knot	*i2 = i3 == begin ? i3 : i3->prev;
	Knot	*i1 = i2 == begin ? i2 : i2->prev;
	Knot	*i0 = i1 == begin ? i1 : i1->prev;
	Knot	*i5 = i4 == end ? i4 : i4->next;
	Knot	*i6 = i5 == end ? i5 : i5->next;

#if 1
	float	t0 = i0->x;
	float	t1 = i1->x;
	float	t2 = i2->x;
	float	t3 = i3->x;
	float	t4 = i4->x;
	float	t5 = i5->x;
	float	t6 = i6->x;
#else
	Knot	*im1 = i0 == begin ? i0 : i0->prev;
	float	t0 = im1->x;
	float	t1 = i0->x;
	float	t2 = i1->x;
	float	t3 = i2->x;
	float	t4 = i3->x;
	float	t5 = i4->x;
	float	t6 = i5->x;
#endif

	float	d00 = i0->y;
	float	d10 = i1->y;
	float	d20 = i2->y;
	float	d30 = i3->y;

	float	a;

	a = (t - t1) / (t4 - t1);
	float	d11 = (1-a) * d00 + a * d10;
	a = (t - t2) / (t5 - t2);
	float	d21 = (1-a) * d10 + a * d20;
	a = (t - t3) / (t6 - t3);
	float	d31 = (1-a) * d20 + a * d30;

	a = (t - t2) / (t4 - t2);
	float	d22 = (1-a) * d11 + a * d21;
	a = (t - t3) / (t5 - t3);
	float	d32 = (1-a) * d21 + a * d31;

	a = (t - t3) / (t4 - t3);
	float	d33 = (1-a) * d22 + a * d32;
	return d33;
}


class ViewAnimComp : public aligned<Window<ViewAnimComp>, 16> {
	Animation				*p;
	e_list<Knot>			knots[3];
	e_list<Knot>			bspline[3];
	Knot					*knot;
	int						knot_bank;
	int						bi;

	float		scale, scaley;
	int			selected;
	Point		pos;
	Point		mouse;


	void	NextBezier(int d);

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE: {
				Rect		client	= GetClientRect();
				ISO_browser	b(*p);
//				ISO_openarray<float4p>	*p2		= (*p)["rot"];
//				float4p	*p2		= p->rot;
				float	minx = 0, maxx = b[0].Count();
				float	miny = -1, maxy = 1;

				int		w = client.Width(), h = client.Height();
				scale	= w / (maxx - minx) * 0.75f;
				scaley	= h / (maxy - miny) * 0.75f / scale;
				pos.x	= w / 2 - (minx + maxx) / 2 * scale;
				pos.y	= h / 2 - (miny + maxy) / 2 * scaley;
				break;
			}

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				Rect		client	= GetClientRect();
				dc.Fill(dc.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

				Pen			old_pen = dc.Select(Pen(RGB(255, 255, 255)));

				ISO_openarray<float4p>	*p2		= (*p)["rot"];
				int		n	= p2->Count();

				static COLORREF	cols[] = {
					RGB(255,0,0),
					RGB(0,255,0),
					RGB(0,0,255),
					RGB(255,0,255),
					RGB(255,255,0),
					RGB(0,255,255),
				};
				for (int j = 0; j < 3; j++) {
					DeleteObject(dc.Select(CreatePen(PS_SOLID, 0, cols[j])));
					for (int i = 0; i < n; i++) {
						int	x = int(i * scale) + pos.x;
						int	y = int((*p2)[i][j] * scale * scaley) + pos.y;
						if (i == 0)
							dc.MoveTo(x, y);
						else
							dc.LineTo(x, y);
					}
					for (e_list<Knot>::iterator i = knots[j].begin(); i != knots[j].end(); ++i) {
						int	x = int(i->x * scale) + pos.x;
						int	y = int(i->y * scale * scaley) + pos.y;
						dc.MoveTo(x-4, y-4, NULL);
						dc.LineTo(x+4, y-4);
						dc.LineTo(x+4, y+4);
						dc.LineTo(x-4, y+4);
						dc.LineTo(x-4, y-4);
					}
					DeleteObject(dc.Select(CreatePen(PS_SOLID, 0, cols[j+3])));
					for (e_list<Knot>::iterator i = bspline[j].begin(); i != bspline[j].end(); ++i) {
						int	x = int(i->x * scale) + pos.x;
						int	y = int(i->y * scale * scaley) + pos.y;
						dc.MoveTo(x-4, y-4, NULL);
						dc.LineTo(x+4, y-4);
						dc.LineTo(x+4, y+4);
						dc.LineTo(x-4, y+4);
						dc.LineTo(x-4, y-4);
					}
				}

				if (!bspline[0].empty()) {
					int		x0	= max(pos.x, 0), x1 = min(int(p2->Count() * scale) + pos.x, client.Width());

					float	k[100], r[100];
					int		n = 0;
					for (e_list<Knot>::iterator i = bspline[0].begin(); i != bspline[0].end(); ++i)
						k[n++] = i->x;
					DeleteObject(dc.Select(CreatePen(PS_SOLID, 0, RGB(128,128,128))));
					for (int x = x0; x < x1; x++) {
						float	t = (x - pos.x) / scale;
//						rbasis(3, t, n, k, r);
						float	v = 0, *pr = r;
						for (e_list<Knot>::iterator i = bspline[0].begin(); i != bspline[0].end(); ++i)
							v += *pr++ * i->y;
						int		y = int(v * scale * scaley) + pos.y;
						if (x == x0)
							dc.MoveTo(x, y, NULL);
						else
							dc.LineTo(x, y);
					}
					DeleteObject(dc.Select(CreatePen(PS_SOLID, 0, RGB(128,0,0))));
					for (int x = x0; x < x1; x++) {
						float	t = (x - pos.x) / scale;
						float	v = EvaluateBSpline(bspline[0], t);
						int		y = int(v * scale * scaley) + pos.y;
						if (x == x0)
							dc.MoveTo(x, y, NULL);
						else
							dc.LineTo(x, y);
					}
				}

				if (knot) {
					DeleteObject(dc.Select(CreatePen(PS_SOLID, 0, RGB(0,0,0))));
					float4	cub	= bezier_spline::blend * CalcBezier(knot);
					float	x0	= knot->x, x1 = knot->next->x;
					for (int i = 0; i <= 100; i++) {
						float	t = i / 100.f;
						float	x = x0 * (1 - t) + x1 * t;
						float	y = ((cub.x * t + cub.y) * t + cub.z) * t + cub.w;
						int		ix = int(x * scale) + pos.x;
						int		iy = int(y * scale * scaley) + pos.y;
						if (i == 0)
							dc.MoveTo(ix, iy);
						else
							dc.LineTo(ix, iy);
					}
				}

				dc.Select(old_pen).Destroy();
				break;
			}

			case WM_KEYDOWN: {
				ISO_openarray<float4p>	*p2		= (*p)["rot"];
				switch (wParam) {
					case 'N': NextBezier(1);		Invalidate(); break;
					case 'P': NextBezier(-1);		Invalidate(); break;
					case 'R': RefineBezier(&(*p2)[0][knot_bank], 4, knot);	Invalidate(); break;
					case 'A':
						for (int j = 0; j < 3; j++) {
							for (e_list<Knot>::iterator i = knots[j].begin(); i != knots[j].end(); ++i) {
								while (RefineBezier(&(*p2)[0][j], 4, i));
							}
						}
						Invalidate();
						break;
					case 'B':
						for (int j = 0; j < 3; j++)
							BezierToBSpline(knots[j], bspline[j]);
						Invalidate();
						break;
					case 'X':
						for (int j = 0; j < 3; j++)
							BSplineToBezier(bspline[j], knots[j]);
						Invalidate();
						break;
				}
				break;
			}

			case WM_LBUTTONDOWN: {
				mouse		= Point(lParam);
				SetFocus();
				break;
			}

			case WM_RBUTTONDOWN: {
				Invalidate();
				break;
			}

			case WM_MOUSEMOVE: {
				Point	pt(lParam);
				if (wParam & MK_LBUTTON) {
					pos	+= pt - mouse;
					Invalidate();
				}
				mouse = pt;
				break;
			}

			case WM_MOUSEWHEEL: {
				if (wParam & MK_CONTROL) {
					scaley *= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				} else {
					Point	pt = ToClient(Point(lParam));
					float	mult = iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
					scale	*= mult;
					pos.x	= pt.x + mult * (pos.x - pt.x);
					pos.y	= pt.y + mult * (pos.y - pt.y);
				}
				Invalidate();
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

	ViewAnimComp(const WindowPos &pos, const ISO_ptr<void> &_p) : p(_p), knot(NULL) {
		Create(pos, NULL, WS_CHILD | WS_VISIBLE, WS_EX_CLIENTEDGE);

		bi	= 0;
		ISO_openarray<float4p>	*p2		= (*p)["rot"];
		if (int n = p2->Count()) {
			FindKnots(&(*p2)[0][0], n, 4, knots[0]);
			FindKnots(&(*p2)[0][1], n, 4, knots[1]);
			FindKnots(&(*p2)[0][2], n, 4, knots[2]);
//		} else if (int n = p->pos.Count()) {
//			FindKnots(&p->pos[0][0], n, 4, knots[0]);
//			FindKnots(&p->pos[0][1], n, 4, knots[1]);
//			FindKnots(&p->pos[0][2], n, 4, knots[2]);
		} else
			return;
		NextBezier(0);
	}
};

void ViewAnimComp::NextBezier(int d)
{
	bi = max(bi + d, 0);

	bool	found = false;
	for (int j = 0, bi2 = bi; !found && j < 4; j++) {
		if (j == 3) {
			bi	= bi2;
			j	= 0;
			if (d == 0)
				break;
		}
		for (e_list<Knot>::iterator i = knots[j].begin(); ; bi2--) {
			knot = i;
			++i;
			if (i == knots[j].end())
				break;
			if (bi2 == 0) {
				found = true;
				knot_bank = j;
				break;
			}
		}
	}

}

class EditorAnimComp : public Editor {
	virtual bool Matches(const ISO_type *type) {
		if (type->SameAs<Animation>(ISOMATCH_NOUSERRECURSE))
			return true;
		return false;
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_ptr<void> &p) {
		return *new ViewAnimComp(pos, p);
	}
} editoranimcomp;
