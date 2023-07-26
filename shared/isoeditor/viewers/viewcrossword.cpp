#include "main.h"
#include "iso/iso_files.h"
#include "windows/d2d.h"

using namespace app;

class ParagraphIndenter : public com<IDWriteInlineObject> {
	float	indent;

	STDMETHOD(Draw)(void *context, IDWriteTextRenderer *renderer, float x, float y, BOOL sideways, BOOL right_to_left, IUnknown *effect) {
		return S_OK;
	}
	STDMETHOD(GetMetrics)(DWRITE_INLINE_OBJECT_METRICS *metrics) {
		clear(*metrics);
		metrics->width = indent;
		return S_OK;
	}
    STDMETHOD(GetOverhangMetrics)(DWRITE_OVERHANG_METRICS *overhangs) {
		clear(*overhangs);
		return S_OK;
	}
	STDMETHOD(GetBreakConditions)(DWRITE_BREAK_CONDITION *before, DWRITE_BREAK_CONDITION *after) {
		*before = DWRITE_BREAK_CONDITION_NEUTRAL;
		*after	= DWRITE_BREAK_CONDITION_NEUTRAL;
		return S_OK;
	}
public:
	ParagraphIndenter(float _indent) : indent(_indent) {}
};

class ViewCrossword : public Window<ViewCrossword>, public WindowTimer<ViewCrossword> {
	MainWindow		&main;
	d2d::WND		target;
	d2d::Write		write;
	float			start_y, speed_y, damp_y;
	float			bottom_y;

//	com_ptr<IDWriteTextFormat>	font;

	ISO_ptr<void>	p;

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				break;

			case WM_SIZE: {
				Point	size(lParam);
				if (!target.Resize(size))
					target.DeInit();
				Invalidate();
				break;
			}

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				Rect	client	= GetClientRect();
				if (target.Init(hWnd, client.Size()) && !target.Occluded()) {
					target.BeginDraw();
					target.SetTransform(identity);

					d2d::SolidBrush	white(target, colour(1,1,1)), black(target, colour(0,0,0)), blue(target, colour(.8f,.8f,1));
					target.Fill(dc.GetDirtyRect(), white);

					target.SetTransform(translate(0, start_y));
					ISO::Browser	b(p);
					ISO::Browser	c = b["grid"];
					int	h		= c.Count();
					int	w		= c[0].Count();
					int	g		= 50;

					for (int i = 0; i <= h; i++)
						target.DrawLine(d2d::point(0, i * g), d2d::point(w * g, i * g), black);
					for (int i = 0; i <= w; i++)
						target.DrawLine(d2d::point(i * g, 0), d2d::point(i * g, h * g), black);

					{
						d2d::Font	font(write, L"arial", 16);
						int	y = 0;
						for (ISO::Browser::iterator i = c.begin(), ie = c.end(); i != ie; ++i, ++y) {
							int	x = 0;
							for (ISO::Browser::iterator j = i->begin(), je = i->end(); j != je; ++j, ++x) {
								ISO::Browser	bj	= *j;
								if (bj[0].GetInt() == 0) {
									target.Fill(d2d::rect(x * g, y * g, (x + 1) * g, (y + 1) * g), black);
								} else if (int n = bj[1].GetInt()) {
									target.DrawText(d2d::rect(x * g, y * g, (x + 1) * g, (y + 1) * g), str16(to_string(n)), font, black);
								}
							}
						}
					}

					{
						d2d::Font	font(write, L"arial", 32);
						font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
						font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
						int	y = 0;
						for (ISO::Browser::iterator i = c.begin(), ie = c.end(); i != ie; ++i, ++y) {
							int	x = 0;
							for (ISO::Browser::iterator j = i->begin(), je = i->end(); j != je; ++j, ++x) {
								if (char16 c = (*j)[0].GetInt())
									target.DrawText(d2d::rect(x * g, y * g, (x + 1) * g, (y + 1) * g), &c, 1, font, black);
							}
						}
					}

					{
						static const float	col_width	= 500;
						static const float	col_gap		= 50;
						static const float	clue_gap	= 5;
						static const float	indent		= 50;

						d2d::Font					heading(write, L"arial", 32);
						d2d::Font					font(write, L"arial", 20);
						DWRITE_TEXT_METRICS			metrics;
						DWRITE_TEXT_RANGE			range	= {0, 1};

						font->SetIncrementalTabStop(indent);

						com_ptr<ParagraphIndenter>	para = new ParagraphIndenter(-indent);
						ISO::Browser	c = b["clues"];
						float		x = 0, max_y = 0;

						for (ISO::Browser::iterator i = c.begin(), ie = c.end(); i != ie; ++i, x += col_width) {
							ISO::Browser	bi	= *i;
							float		y	= g * h;
							target.DrawText(d2d::rect(x, y, x + col_width, y + 20), str16(bi[0].GetString()), heading, black);
							y += 50;

							for (ISO::Browser::iterator j = bi[1].begin(), je = bi[1].end(); j != je; ++j) {
								ISO::Browser		bj		= *j;
								d2d::TextLayout	layout(write, str16(format_string(" %i.\t%s", bj[0].GetInt(), bj[1].GetString())), font, col_width - col_gap - indent, 1000);

								layout->SetInlineObject(para, range);
								layout->GetMetrics(&metrics);
								target.DrawText(d2d::point(x + indent, y), layout, black);
								y += metrics.height + clue_gap;
							}
							max_y = max(max_y, y);
						}
						bottom_y = max_y;
					}
					if (target.EndDraw())
						target.DeInit();
				}
				break;
			}

			case WM_MOUSEWHEEL: {
				if (!IsRunning())
					Timer::Start(0.01f);
				speed_y += (short)HIWORD(wParam) / 120.f;
				break;
			}

			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
				SetFocus();
				break;

			case WM_MOUSEMOVE: {
				Point mouse(lParam);
				break;
			}

			case WM_ISO_TIMER:
				start_y += speed_y;
				if (start_y > 0) {
					speed_y -= start_y / 64;
					damp_y = 0.8f;
				} else if (start_y + bottom_y < GetClientRect().Height()) {
					speed_y -= (start_y + bottom_y - GetClientRect().Height()) / 64;
					damp_y = 0.8f;
				}
				speed_y *= damp_y;
				if (abs(speed_y) < 0.01f) {
					Stop();
					speed_y = 0;
					damp_y	= 0.95f;
				}
				Invalidate();
				break;

			case WM_COMMAND:
//				switch (LOWORD(wParam)) {
//					break;
//				}
				break;

			case WM_NCDESTROY:
				delete this;
				break;

			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}

	ViewCrossword(MainWindow &main, const WindowPos &wpos, ISO_ptr<void> p) : main(main), start_y(0), speed_y(0), damp_y(0.95f), p(p) {
		Create(wpos, tag(p.ID()), CHILD, CLIENTEDGE);
	}
};

class EditorCrosswprd : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is("crossword");
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &p) {
		return *new ViewCrossword(main, wpos, p);
	}
} editorcrossword;
