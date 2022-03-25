#include "main.h"
#include "base/vector.h"
#include "filetypes/video/movie.h"
#include "windows/d2d.h"
#include "windows/dib.h"
#include "iso/iso.h"
#include "iso/iso_convert.h"

using namespace app;

class ViewMovie : public Window<ViewMovie>, public WindowTimer<ViewMovie> {
	TrackBarControl		tb;
#ifdef D2D_H
	d2d::WND			target;
	com_ptr<ID2D1Bitmap>	bitmap[3];
#else
	DIB					*dib;
#endif

	ISO_ptr_machine<movie>	mv;
	ISO_ptr_machine<void>	fr;
	ISO::Browser			frames;
	int					width, height;
	int					frame, shown;
	float				scale;
	Point				pos;
	Point				prevmouse;
	uint32				prevbutt;

	void	UpdateBitmap() {
	#ifdef D2D_H
		if (bitmap[0]) {
			if (fr.IsType("vbitmap")) {
				for (int i = 0; i < 3; ++i) {
					vbitmap_format fmt;
					uint32	w, h, s;
					if (void *data = ((vbitmap*)fr)->GetRaw(i, &fmt, &s, &w, &h))
						bitmap[i]->CopyFromMemory(0, data, s);
				}
			}
		}
	#else
		if (ISO_ptr<void> bm = ISO_conversion::convert<bitmap>(fr))
			dib->Update(*bm);
	#endif
	}
	void	SetBitmap(const ISO_ptr<void> &p) {
		fr = p;
		UpdateBitmap();
		Invalidate();
	}

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				tb.Create(*this, NULL, VISIBLE | CHILD, NOEX, GetClientRect().Subbox(0, -32, 0, 0));
				tb(TBM_SETRANGEMIN, FALSE, 0);
				tb(TBM_SETRANGEMAX, FALSE, frames.Count());
				SetBitmap(frames[0]);
				break;

			case WM_SIZE:
				tb.Move(GetClientRect().Subbox(0, -32, 0, 0));
				if (target.Resize(Point(lParam))) {
					target.DeInit();
					bitmap[0].clear();
					bitmap[1].clear();
					bitmap[2].clear();
				}
				break;

			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				Rect		client	= GetClientRect().Subbox(0, 0, 0, -32);
			#ifdef D2D_H
				if (target.Init(hWnd, client.Size()) && !target.Occluded()) {
					if (!bitmap[0]) {
						target.CreateBitmap(&bitmap[0], width, height, DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT);
						target.CreateBitmap(&bitmap[1], width / 2, height / 2, DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT);
						target.CreateBitmap(&bitmap[2], width / 2, height / 2, DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT);
						UpdateBitmap();
					}
					target.BeginDraw();

					target.SetTransform(identity);
					target.Fill(client, d2d::SolidBrush(target, colour::black));
					target.SetTransform(translate(position2(pos.x, pos.y)) * iso::scale(scale));

				#if 0
					target.Draw(
						d2d::rect(pos.x, pos.y, width * scale + pos.x, height * scale + pos.y),
						bitmap[0]
					);
				#elif 0
					d2d::matrix5x4	yuv_conversion = float4x4(
						float3x3(
							float3(1.164f,  1.164f, 1.164f),
							float3(  0.0f, -0.392f, 2.017f),
							float3(1.596f, -0.813f,   0.0f)
						)
						* translate(-16.0f/255, -0.5f, -0.5f)
					);
					swap(yuv_conversion[2], yuv_conversion[3]);

					target.DrawImage(
						d2d::Effect(target, CLSID_D2D1ColorMatrix)
							.SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, yuv_conversion)
							.SetValue(D2D1_COLORMATRIX_PROP_ALPHA_MODE, D2D1_COLORMATRIX_ALPHA_MODE_STRAIGHT)
							.SetInput(bitmap[0])
					);
				#else
					d2d::matrix5x4	maty	= d2d::matrix5x4(float4(zero), float4(zero), float4(zero), float4{1,0,0,0}, float4{0,0,0,1});
					d2d::matrix5x4	matu	= d2d::matrix5x4(float4(zero), float4(zero), float4(zero), float4{1,0,0,0}, float4{0,0,0,1});
					d2d::matrix5x4	matv	= d2d::matrix5x4(float4(zero), float4(zero), float4(zero), float4{0,1,0,0}, float4{0,0,0,1});

					target.DrawImage(
						d2d::Effect(target, CLSID_D2D1YCbCr)
							.SetValue(D2D1_YCBCR_PROP_CHROMA_SUBSAMPLING, D2D1_YCBCR_CHROMA_SUBSAMPLING_420)
							.SetInput(0, d2d::Effect(target, CLSID_D2D1ColorMatrix).SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, maty).SetInput(bitmap[0]))
							.SetInput(1,
								d2d::Effect(target, CLSID_D2D1Composite)
									.SetValue(D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_PLUS)
									.SetInput(0, d2d::Effect(target, CLSID_D2D1ColorMatrix).SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matu).SetInput(bitmap[1]))
									.SetInput(1, d2d::Effect(target, CLSID_D2D1ColorMatrix).SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matv).SetInput(bitmap[2]))
							)
					);
				#endif
					if (target.EndDraw()) {
						target.DeInit();
						bitmap[0].clear();
						bitmap[1].clear();
						bitmap[2].clear();
					}
				}
			#else
				if (fr) {
					Rect	rect(pos.x, pos.y, width * scale, height * scale));
					HBRUSH	hbr		= (HBRUSH)GetStockObject(BLACK_BRUSH);

					if (rect.bottom < client.bottom)
						dc.Fill(Rect(client).SetTop(rect.bottom), hbr);
					if (rect.top > 0)
						dc.Fill(Rect(client).SetBottom(rect.top), hbr);
					if (rect.right < client.right)
						dc.Fill(Rect(client).SetLeft(rect.right), hbr);
					if (rect.left > 0)
						dc.Fill(Rect(client).SetRight(rect.left), hbr);

					SetStretchBltMode(dc, COLORONCOLOR);
					dib->Stretch(dc, rect);
				}
			#endif
				shown	= frame;
				break;
			}

			case WM_ISO_TIMER: {
				bool	ready = shown == frame;
				tb.SetPos(frame = (frame + 1) % frames.Count());
				if (ready)
					SetBitmap(frames[frame]);
				break;
			}

			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
				SetFocus();
				break;

			case WM_MOUSEMOVE:
				if (prevbutt != wParam) {
					prevmouse	= Point(lParam);
					prevbutt	= wParam;
				}
				if (wParam & MK_LBUTTON) {
					Point mouse(lParam);
					pos			+= mouse - prevmouse;
					prevmouse	= mouse;
					Invalidate();
				}
				break;

			case WM_MOUSEWHEEL: {
				Point	pt		= ToClient(Point(lParam));
				float	mult	=iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				scale	*= mult;
				pos		= pt + (pos - pt) * mult;
				Invalidate();
				break;
			}

			case WM_KEYDOWN:
				if (wParam == VK_SPACE) {
					if (IsRunning())
						Timer::Stop();
					else
						Timer::Start(1.f / mv->fps);
				}
				break;

			case WM_HSCROLL:
				switch (LOWORD(wParam)) {
					case SB_THUMBTRACK: {
						frame	= tb.GetPos();
						SetBitmap(frames[frame]);
						break;
					}
				}
				break;

			case WM_NCDESTROY:
				delete this;
				break;
			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}

	ViewMovie(const WindowPos &wpos, ISO_ptr_machine<movie> _mv) : mv(_mv), frame(0), scale(1), pos(0,0) {
		frames	= ISO::Browser(mv->frames);
		width	= mv->width;
		height	= mv->height;
		Create(wpos, NULL, CHILD | VISIBLE, CLIENTEDGE);
	}
	~ViewMovie() {
	#ifdef D2D_H
	#else
		dib->Destroy();
	#endif
	}
};

class EditorMovie : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->SameAs<movie>();
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		return *new ViewMovie(wpos, p);
	}
} editormovie;
