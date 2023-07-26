#include "main.h"
#include "base/vector.h"
#include "filetypes/video/movie.h"
#include "windows/d2d.h"
#include "windows/dib.h"
#include "iso/iso.h"
#include "iso/iso_convert.h"

using namespace app;

//#define USE_IMAGE_SOURCE

class ViewMovie : public Window<ViewMovie>, public WindowTimer<ViewMovie> {
	TrackBarControl			tb;
	ToolTipControl			tooltip;

#ifdef D2D_H
	d2d::WND				target;
	com_ptr<ID2D1Bitmap>	bitmap[3];
	com_ptr<IDXGISurface>	surface[3];
	d2d::Effect				effect;
	com_ptr<ID2D1ImageSource>		image_source;

#else
	DIB						*dib;
#endif

	ISO_ptr_machine<movie>	mv;
	ISO_ptr_machine<void>	fr;
	ISO::Browser			frames;
	int					width, height, planes, bitdepth;
	int					frame, shown;
	float				scale;
	Point				pos;
	Point				prevmouse;
	uint32				prevbutt;

	float2	ClientToLocal(Point pt) const {
		return float2{pt.x - pos.x, pt.y - pos.y} / scale;
	}
	bool	InFrame(float2 p) const {
		return all(p >= 0) && p.x < width && p.y < height;
	}

	template<typename T> static point	BitmapSize(T *bm) { return {bm->Width(), bm->Height()}; }

	point	BitmapSize() {
		if (fr.IsType("vbitmap"))
			return BitmapSize((vbitmap*)fr);
		if (fr.IsType<iso::bitmap>())
			return BitmapSize((iso::bitmap*)fr);
		if (fr.IsType<HDRbitmap>())
			return BitmapSize((HDRbitmap*)fr);
		return zero;
	}
	void	ClearResources();
	void	CreateBitmap();
	void	UpdateBitmap();

	void	SetBitmap(const ISO_ptr<void> &p) {
		fr		= p;
		planes	= 1;

		if (fr.IsType("vbitmap")) {
			auto	bm = (vbitmap*)fr;
			bitdepth = bm->format & 0x3f;
			planes	= 3;

		}

		UpdateBitmap();
		Invalidate();
	}

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	ViewMovie(const WindowPos &wpos, ISO_ptr_machine<movie> _mv) : mv(_mv), frame(0), scale(1), pos(0,0) {
		frames		= ISO::Browser(mv->frames);
		width		= mv->width;
		height		= mv->height;
		bitdepth	= 8;
		Create(wpos, NULL, CHILD | VISIBLE | CLIPCHILDREN, CLIENTEDGE);
	}
	~ViewMovie() {
	#ifdef D2D_H
	#else
		dib->Destroy();
	#endif
	}
};

void ViewMovie::ClearResources() {
	surface[0].clear();
	surface[1].clear();
	surface[2].clear();
	image_source.clear();

	bitmap[0].clear();
	bitmap[1].clear();
	bitmap[2].clear();

	effect.clear();
}

void ViewMovie::CreateBitmap() {
	if (fr) {
		ClearResources();
		auto	size = BitmapSize();
		width	= size.x;
		height	= size.y;
	}

	if (planes == 1) {
		target.CreateBitmap(&bitmap[0], width, height, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);//, D2D1_ALPHA_MODE_STRAIGHT);
		effect.Create(target, CLSID_D2D1ColorMatrix).SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, d2d::matrix5x4(identity, float4{0,0,0,1})).SetInput(bitmap[0]);

	} else {
#ifdef USE_IMAGE_SOURCE
		com_ptr<ID3D11Device>			d3d;
		com_ptr<ID3D11DeviceContext>	d3dcontext;
		D3D11CreateDevice(
			nullptr, 
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			&d3d,
			nullptr,
			&d3dcontext
		);

		D3D11_TEXTURE2D_DESC		desc;
		clear(desc);
		desc.MipLevels			= 1;
		desc.ArraySize			= 1;
		desc.SampleDesc.Count	= 1;
		desc.Usage				= D3D11_USAGE_DYNAMIC;
		desc.BindFlags			= D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;

		for (int i = 0; i < 3; ++i) {
			vbitmap_format fmt;
			uint32	w, h, s;
			if (auto src = ((vbitmap*)fr)->GetRaw(i, &fmt, &s, &w, &h)) {
				desc.Width				= w;
				desc.Height				= h;
				desc.Format				= DXGI_FORMAT_A8_UNORM;

				D3D11_SUBRESOURCE_DATA	data;
				data.pSysMem			= src;
				data.SysMemPitch		= s;
				data.SysMemSlicePitch	= 0;

				com_ptr<ID3D11Texture2D>	tex;
				d3d->CreateTexture2D(&desc, &data, &tex);
				surface[i] = tex.query();
			}
		}

		com_ptr<ID2D1DeviceContext2>	dc2 = target.device.query();
		dc2->CreateImageSourceFromDxgi(
			unconst(&make_const(surface)[0]), 3,
			DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,//DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601,
			D2D1_IMAGE_SOURCE_FROM_DXGI_OPTIONS_NONE,
			&image_source
		);
	#else
		target.CreateBitmap(&bitmap[0], width, height, DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT);
		target.CreateBitmap(&bitmap[1], width / 2, height / 2, DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT);
		target.CreateBitmap(&bitmap[2], width / 2, height / 2, DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT);
		int	o = 1;// << ((bitdepth <= 8 ? 8 : 16) - bitdepth);

		d2d::matrix5x4	maty	= d2d::matrix5x4(float4(zero), float4(zero), float4(zero), float4{o,0,0,0}, float4{0,0,0,1});
		d2d::matrix5x4	matu	= d2d::matrix5x4(float4(zero), float4(zero), float4(zero), float4{o,0,0,0}, float4{0,0,0,1});
		d2d::matrix5x4	matv	= d2d::matrix5x4(float4(zero), float4(zero), float4(zero), float4{0,o,0,0}, float4{0,0,0,1});

		effect.Create(target, CLSID_D2D1YCbCr)
			.SetValue(D2D1_YCBCR_PROP_CHROMA_SUBSAMPLING, D2D1_YCBCR_CHROMA_SUBSAMPLING_420)
			.SetInput(0, d2d::Effect(target, CLSID_D2D1ColorMatrix).SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, maty).SetInput(bitmap[0]))
			.SetInput(1,
				d2d::Effect(target, CLSID_D2D1Composite)
				.SetValue(D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_PLUS)
				.SetInput(0, d2d::Effect(target, CLSID_D2D1ColorMatrix).SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matu).SetInput(bitmap[1]))
				.SetInput(1, d2d::Effect(target, CLSID_D2D1ColorMatrix).SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matv).SetInput(bitmap[2]))
		);
	#endif
	}
}


void ViewMovie::UpdateBitmap() {
#ifdef D2D_H
	if (bitmap[0]) {
		if (fr.IsType("vbitmap")) {
		#ifdef USE_IMAGE_SOURCE
			for (int i = 0; i < 3; ++i) {
				vbitmap_format fmt;
				uint32	w, h, s;
				if (void *data = ((vbitmap*)fr)->GetRaw(i, &fmt, &s, &w, &h)) {
					DXGI_MAPPED_RECT	rect;
					surface[i]->Map(&rect, DXGI_MAP_WRITE | DXGI_MAP_DISCARD);
					for (int y = 0; y < h; y++)
						memcpy(rect.pBits + y * rect.Pitch, (uint8*)data + y * s, s);
					surface[i]->Unmap();
				}
			}
		#else
			for (int i = 0; i < 3; ++i) {
				vbitmap_format fmt;
				uint32	w, h, s;
				if (void *data = ((vbitmap*)fr)->GetRaw(i, &fmt, &s, &w, &h)) {
					if (fmt <= 8) {
						bitmap[i]->CopyFromMemory(0, data, s);
					} else {
						uint32			n	= w * h;
						malloc_block	temp(n);
						uint16			*src	= (uint16*)data;
						uint8			*dst	= temp;
						uint32			shift	= fmt - 8;
						while (n--) {
							*dst++ = *src++ >> shift;
						}
						bitmap[i]->CopyFromMemory(0, temp, w);
					}
				}
			}
		#endif

		} else if (fr.IsType<iso::bitmap>()) {
			d2d::FillBitmap(bitmap[0], transform(((iso::bitmap*)fr)->All(), [](ISO_rgba i)->d2d::texel { return {i.r, i.g, i.b}; }));

		} else if (fr.IsType<HDRbitmap>()) {
			d2d::FillBitmap(bitmap[0], transform(((iso::HDRbitmap*)fr)->All(), [](float4 i) { return concat(to_sat<uint8>(i.xyz / 8), 255); }));

		}
	}
#else
	if (ISO_ptr<void> bm = ISO_conversion::convert<bitmap>(fr))
		dib->Update(*bm);
#endif
}

LRESULT ViewMovie::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			tb.Create(*this, NULL, CLIPSIBLINGS | VISIBLE | CHILD, NOEX, GetClientRect().Subbox(0, -32, 0, 0));
			tb(TBM_SETRANGEMIN, FALSE, 0);
			tb(TBM_SETRANGEMAX, FALSE, frames.Count());

			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);

			SetBitmap(frames[0]);
			break;

		case WM_SIZE:
			tb.Move(GetClientRect().Subbox(0, -32, 0, 0));
			if (!target.Resize(Point(lParam))) {
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
				if (!bitmap[0] || any(BitmapSize() != point{width, height})) {
					CreateBitmap();
					UpdateBitmap();
				}
				target.BeginDraw();

				target.SetTransform(identity);
				target.Fill(client, d2d::SolidBrush(target, colour::black));
				target.SetTransform(translate(position2(pos.x, pos.y)) * iso::scale(scale));

			#ifdef USE_IMAGE_SOURCE
				target.DrawImage(image_source);
			#else
				target.DrawImage(effect);
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

		case WM_SETFOCUS:
			SetAccelerator(*this, Accelerator());
			break;

		case WM_MOUSEACTIVATE:
			SetFocus();
			return MA_NOACTIVATE;
			//case WM_LBUTTONDOWN:
			//case WM_RBUTTONDOWN:
			//	SetFocus();
			//	break;

		case WM_MOUSEMOVE: {
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
			if (InFrame(ClientToLocal(Point(lParam)))) {
				tooltip.Activate(*this);
				tooltip.Track();
			}
			break;
		}

		case WM_MOUSEWHEEL: {
			Point	pt		= ToClient(Point(lParam));
			float	mult	=iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
			scale	*= mult;
			pos		= pt + (pos - pt) * mult;
			Invalidate();
			break;
		}

		case WM_KEYDOWN:
			switch (wParam) {
				case VK_SPACE:
					if (IsRunning())
						Timer::Stop();
					else
						Timer::Start(1.f / mv->fps);
					break;
				case VK_LEFT:
					tb.SetPos(frame = (frame + frames.Count() - 1) % frames.Count());
					SetBitmap(frames[frame]);
					break;
				case VK_RIGHT:
					tb.SetPos(frame = (frame + 1) % frames.Count());
					SetBitmap(frames[frame]);
					break;
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

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, *this, false);
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA:
					if (nmh->hwndFrom == tooltip) {
						auto	pt	= ClientToLocal(ToClient(GetMousePos()));
						NMTTDISPINFOA	*nmtdi = (NMTTDISPINFOA*)nmh;
						fixed_accum		sa(nmtdi->szText);

						if (InFrame(pt)) {
							int		chans[4];
							auto	p	= to<int>(pt);
							sa << '(' << p.x << ',' << p.y << ')' << ':';
							if (fr.IsType("vbitmap")) {
								for (int i = 0; i < 3; ++i) {
									vbitmap_format fmt;
									uint32	w, h, s;
									if (void *data = ((vbitmap*)fr)->GetRaw(i, &fmt, &s, &w, &h))
										chans[i] = 0;
								}
							} else if (fr.IsType<iso::bitmap>()) {
								//d2d::FillBitmap(bitmap[0], ((iso::bitmap*)fr)->All());
								auto	px = ((iso::bitmap*)fr)->All()[p.y][p.x];
								sa << '(' << (int)px.r << ',' << (int)px.g << ',' << (int)px.b << ')';

							} else if (fr.IsType<HDRbitmap>()) {
								auto	px = ((iso::HDRbitmap*)fr)->All()[p.y][p.x];
								sa << '(' << px.r << ',' << px.g << ',' << px.b << ')';
							}
							sa << '\n';

						} else {
							tooltip.Activate(*this, false);
						}

					} else {
						TOOLTIPTEXT	*ttt	= (TOOLTIPTEXT*)nmh;
						ttt->hinst			= GetDefaultInstance();
						ttt->lpszText		= MAKEINTRESOURCE(ttt->hdr.idFrom);
					}
					break;
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

class EditorMovie : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->SameAs<movie>();
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		return *new ViewMovie(wpos, p);
	}
} editormovie;
