#define INITGUID

#include "viewbitmap.h"

namespace app {
using namespace iso;
using namespace win;

//-----------------------------------------------------------------------------
//	histogram4
//-----------------------------------------------------------------------------

void histogram4::init(bitmap64 *bm) {
	reset();
	auto	tovec = [](const ISO_rgba &x) { return reinterpret_cast<const uint8x4&>(x); };

	if (int nmips = bm->Mips()) {
		for (int m = 0; m < nmips; m++)
			add(bm->Mip(m), tovec);
	} else {
		add(bm->All(), tovec);
	}
	init_medians();
}

void histogram4::init(HDRbitmap64 *bm, HDRpixel minpix, HDRpixel maxpix) {
	reset();
	scale_offset	s(minpix, maxpix);

	if (int nmips = bm->Mips()) {
		for (int m = 0; m < nmips; m++)
			add(bm->Mip(m), s);
	} else {
		add(bm->All(), s);
	}
	init_medians();
}

void ViewBitmap_base::DrawHistogram(d2d::Target &target, win::Rect &rect, const histogram4 &hist, int c) {
	uint32	max_val = 0;
	if (c & CHAN_R) max_val = max(max_val, hist[0].max_value());
	if (c & CHAN_G) max_val = max(max_val, hist[1].max_value());
	if (c & CHAN_B) max_val = max(max_val, hist[2].max_value());
	if (c & CHAN_A) max_val = max(max_val, hist[3].max_value());

	target.SetTransform(translate(rect.Left(), rect.Bottom()) * scale(rect.Width() / 256.f, -float(rect.Height()) / max_val));
	target.Fill(d2d::rect(0, 0, 256, max_val), d2d::SolidBrush(target, d2d::colour(0,0,0,0.5f)));

	com_ptr<ID2D1SolidColorBrush>	cols[4];
	target.CreateBrush(&cols[0], d2d::colour(1,0,0));
	target.CreateBrush(&cols[1], d2d::colour(0,1,0));
	target.CreateBrush(&cols[2], d2d::colour(0,0,1));
	target.CreateBrush(&cols[3], d2d::colour(0,0,0));

	for (int i = 0; i < 256; i++) {
		uint32	h[4]		= {
			c & CHAN_R ? hist[0].h[i] : 0,
			c & CHAN_G ? hist[1].h[i] : 0,
			c & CHAN_B ? hist[2].h[i] : 0,
			c & CHAN_A ? hist[3].h[i] : 0
		};
		uint8	order[4]	= {0,1,2,3};
		bool	swapped = true;
		while (swapped) {
			swapped = false;
			for (int j = 0; j < 3; j++) {
				if (h[order[j]] > h[order[j + 1]]) {
					swap(order[j], order[j+1]);
					swapped = true;
				}
			}
		}
		int	h0 = 0;
		for (int j = 0; j < 4; j++) {
			int	h1 = h[order[j]];
			target.Fill(d2d::rect(i, h0, i + 1, h1), cols[order[j]]);
			h0	= h1;
		}
	}
}

//-----------------------------------------------------------------------------
//	ViewBitmap_base
//-----------------------------------------------------------------------------

void ViewBitmap_base::UpdateBitmapRect() {
	num_slices	= flags.test(SEPARATE_SLICES) ? bitmap_depth : 1;
	int	width, height;
	if (auto hdr = bm.test_cast<HDRbitmap64>()) {
		width	= hdr->Width();
		height	= hdr->Height();
	} else {
		bitmap64	*bm = this->bm;
		width	= bm->Width();
		height	= bm->Height();
	}
	bitmap_rect.right	= width >> int(flags.test(HAS_MIPS) && !flags.test(SHOW_MIPS));
	bitmap_rect.bottom	= height / num_slices;
}

int ViewBitmap_base::CalcSlice(d2d::point &ret) {
	if (between(int(floor(ret.x)), bitmap_rect.Left(), bitmap_rect.Right() - 1) && ret.y > 0) {
		int		h	= bitmap_rect.Height();
		if (flags.test(SEPARATE_SLICES)) {
			int		h1	= h + 8;
			float	z	= ret.y / h1;
			if (frac(z) * h1 < h) {
				ret.y	= floor(z) * h + frac(z) * h1;
				return int(z);
			}
		} else {
			h		/= bitmap_depth;
			int		z = int(ret.y / h);
			ret.y	-= z * h;
			return z;
		}
	}
	return -1;
}

int ViewBitmap_base::CalcMip(d2d::point &ret) {
	int		m = 0;
	int		w = bitmap_rect.Width();
	w >>= 1;
	while (w && ret.x >= w) {
		ret.x -= w;
		w >>= 1;
		m++;
	}
	return m;
}

void ViewBitmap_base::Autoscale(int sw, int sh) {
	int	bw = bitmap_rect.Width(), bh = bitmap_rect.Height();
	int	ss = sel_slice;
	if (ss < 0) {
		bh *= num_slices;
		ss = 0;
	}
	if (bw * sh > bh * sw) {
		zoom	= float(sw) / bw;
		pos		= Point(0, (sh - bh * zoom) / 2 - (bh + 8) * ss * zoom);
	} else {
		zoom	= float(sh) / bh;
		pos		= Point((sw - bw * zoom) / 2, -(bh + 8) * ss * zoom);
	}
}

bool ViewBitmap_base::SetBitmap(const ISO_ptr_machine<void> &p) {
	if (p.IsType<bitmap64>() || p.IsType<HDRbitmap64>()) {
		bm		= p;

	} else if (p.IsType<bitmap>()) {
		bm		= ISO_ptr_machine<bitmap64>(p.ID(), *(bitmap*)p);

	} else if (p.IsType<HDRbitmap>()) {
		bm		= ISO_ptr_machine<HDRbitmap64>(p.ID(), *(HDRbitmap*)p);

	} else if (p.IsType<bitmap_anim>()) {
		anim	= p;
		frame	= 0;
		bm		= ISO_ptr_machine<bitmap64>(0, *(*anim)[0].a.get());

	} else if (p.IsType("Texture")) {
		ISO_ptr<bitmap2> p2 = ISO::Conversion::convert<bitmap2>(*(ISO_ptr_machine<ISO_ptr_machine<void> >&)p, ISO::Conversion::RECURSE | ISO::Conversion::EXPAND_EXTERNALS);
		if (p2->IsType<bitmap>())
			bm = ISO_ptr_machine<bitmap64>(p.ID(), *(bitmap*)*p2);
		else
			bm = ISO_ptr_machine<HDRbitmap64>(p.ID(), *(HDRbitmap*)*p2);

	} else if (ISO_ptr<bitmap2> p2 = ISO::Conversion::convert<bitmap2>(p)) {
		if (p2->IsType<bitmap64>() || p2.IsType<HDRbitmap64>())
			bm = *p2;
		else if (p2->IsType<bitmap>())
			bm	= ISO_ptr_machine<bitmap64>(p.ID(), *(bitmap*)*p2);
		else if (p2->IsType<HDRbitmap>())
			bm	= ISO_ptr_machine<HDRbitmap64>(p.ID(), *(HDRbitmap*)*p2);
		else
			return false;

	} else {
		return false;
	}
	
	flags.clear(HAVE_STATS);
	chan_range	= {zero, one};
	disp_range	= alpha_range = rgb_range = {zero, one};

	if (auto hdr = bm.test_cast<HDRbitmap64>()) {
		bitmap_depth	= hdr->Depth();
		flags.set(CUBEMAP, hdr->IsCube());
		if (hdr->Mips())
			flags.set_all(HAS_MIPS | SHOW_MIPS);
	} else {
		bitmap64	*bm = this->bm;
		bitmap_depth	= bm->Depth();
		flags.set(CUBEMAP, bm->IsCube());
		if (bm->Mips())
			flags.set_all(HAS_MIPS | SHOW_MIPS);
	}

	if (bitmap_depth > 1)
		flags.set_all(SEPARATE_SLICES);

	UpdateBitmapRect();
	return true;
}

void ViewBitmap_base::GetStats() {
	if (!flags.test_set(HAVE_STATS)) {
		if (auto hdr = bm.test_cast<HDRbitmap64>()) {
			stats<float4>	s;
			for (auto i : hdr->All()) {
				for (auto &j : i)
					s.add(j);
			}

			float4		p0	= s.mean() + s.sigma() * invphi_approx(0.05f);
			float4		p1	= s.mean() + s.sigma() * invphi_approx(0.95f);

			rgb_range		= {reduce_min(p0.xyz), reduce_max(p1.xyz)};
			alpha_range		= {min(p0.w, 0), max(p1.w, 1)};
			disp_range		= {-rgb_range.a / (rgb_range.b - rgb_range.a), 1};

			chan_range		= {p0, p1};
			chan_range.a.w	= chan_range.a.w * rgb_range.b / alpha_range.b;
			chan_range.b.w	= chan_range.b.w * rgb_range.b / alpha_range.b;

		} else {
			bitmap64	*bm = this->bm;
			ISO_rgba	*p	= bm->ScanLine(0);
			ISO_rgba	p0	= *p, p1 = p0;
			for (int n = bm->Width() * bm->Height(); n--; p++) {
				p0 = min(p0, *p);
				p1 = max(p1, *p);
			}
			chan_range		= {HDRpixel(p0), HDRpixel(p1)};
		}
	}
}


void ViewBitmap_base::UpdateBitmap(d2d::Target &target, ID2D1Bitmap *d2d_bitmap) {
	int			c		= channels;
	int			width	= bitmap_rect.Width();
	int			height	= bitmap_rect.Height() * num_slices;

	if (auto hdr = bm.test_cast<HDRbitmap64>()) {
		GetStats();
		float4			mult	= 255 / (disp_range.extent() * (rgb_range.b - rgb_range.a));
		float4			offset	= rgb_range.a + disp_range.a * (rgb_range.b - rgb_range.a);
		if (c == CHAN_A) {
			mult.w		= 255 / (disp_range.extent() * (alpha_range.b - alpha_range.a));
			offset.w	= alpha_range.a + disp_range.a * (alpha_range.b - alpha_range.a);
		} else {
			mult.w		= 255;
			offset.w	= 0;
		}

		d2d::FillBitmap(d2d_bitmap, transform(hdr->Block(0, 0, width, height), [offset, mult](float4 i) { return to_sat<uint8>((i - offset) * mult); }));

	} else if (bitmap64 *bm = this->bm) {
		if (bm->IsPaletted()) {
			d2d::FillBitmap(d2d_bitmap, transform(bm->Block(0, 0, width, height), [clut = bm->Clut()](ISO_rgba i) { return clut[i.r]; }));
		} else {
			d2d::FillBitmap(d2d_bitmap, bm->All());
		}
	}
}

void ViewBitmap_base::DrawBitmapPaint(d2d::Target &target, win::Rect &rect, ID2D1Bitmap *d2d_bitmap) {
	float2x3	transform = translate(rect.TopLeft() + pos)
		* scale(flags.test(FLIP_X) ? -zoom : zoom, flags.test(FLIP_Y) ? -zoom : zoom)
		* translate(flags.test(FLIP_X) ? -bitmap_rect.Width() : 0, flags.test(FLIP_Y) ? -bitmap_rect.Height() : 0);

#ifndef _D2D1_1_H_
	target.SetTransform(transform);
	target.Draw(bitmap_rect, bitmap_rect, d2d_bitmap, 1, flags.test(BILINEAR) ? D2D1_BITMAP_INTERPOLATION_MODE_LINEAR : D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);

#elif 0

	target.DrawImage(
		transform.translation(),
		d2d::Effect(target, CLSID_MyCustomEffect)
		.SetValue(MyCustomEffect::PARAM_transform,	d2d::matrix(transform))
		.SetInput(d2d_bitmap),
		d2d::rect((transform * d2d::rect(bitmap_rect)).get_rect()),
		flags.test(BILINEAR) ? D2D1_INTERPOLATION_MODE_LINEAR : D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
	);
	target.SetTransform(transform);

#else

	target.SetTransform(transform);

	auto			hdr			= bm.test_cast<HDRbitmap64>();
	int				c			= channels;
	float			multc		= hdr ? 1 : 1 / disp_range.extent();
	float			offsetc		= hdr ? 0 : -disp_range.a / disp_range.extent();
	float			invgamma	= hdr ? 1 : 1 / gamma;
	bool			blend		= (c & CHAN_A) && c != CHAN_A;

	d2d::matrix5x4	cmat(
		float4{int(!!(c & CHAN_R)) * multc, 0, 0, 0},
		float4{0, int(!!(c & CHAN_G)) * multc, 0, 0},
		float4{0, 0, int(!!(c & CHAN_B)) * multc, 0},
		concat(float3(int(c == CHAN_A)), blend ? 1.f : iorf(1u).f()),
		concat(float3(offsetc), int(!blend))
	);

	d2d::Effect		effect(target, CLSID_D2D1GammaTransfer);

	effect.SetValue(D2D1_GAMMATRANSFER_PROP_RED_EXPONENT, invgamma)
		.SetValue(D2D1_GAMMATRANSFER_PROP_GREEN_EXPONENT, invgamma)
		.SetValue(D2D1_GAMMATRANSFER_PROP_BLUE_EXPONENT, invgamma)
		.SetValue(D2D1_GAMMATRANSFER_PROP_ALPHA_DISABLE, TRUE)
		.SetInput(d2d::Effect(target, CLSID_D2D1ColorMatrix)
			.SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, cmat)
			.SetValue(D2D1_COLORMATRIX_PROP_ALPHA_MODE, D2D1_COLORMATRIX_ALPHA_MODE_STRAIGHT)
			.SetInput(d2d_bitmap)
		);

	d2d::rect	visible_rect(
		ClientToTexel0(d2d::point(0, 0)),
		ClientToTexel0(rect.Size()) + d2d::point(1, 1)
	);

	for (int i = 0; i < num_slices; i++) {
		win::Point	slice_offset(0, i * (bitmap_rect.Height() + 8));
		target.DrawImage(
			slice_offset,
			effect,
			bitmap_rect + win::Point(0, bitmap_rect.Height() * i),
			flags.test(BILINEAR) ? D2D1_INTERPOLATION_MODE_LINEAR : D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
		);

		d2d::rect	r0 = visible_rect & (bitmap_rect + slice_offset);
#if 1
		// pixel grid
		if (zoom > 8) {
			d2d::SolidBrush	black(target, colour(0, 0, 0, min((zoom - 8) / 8, 1.f)));

			for (float y = floor(r0.top); y <= r0.bottom; y++)
				target.DrawLine(d2d::point(r0.left, y), d2d::point(r0.right, y), black, 1 / zoom);

			for (float x = floor(r0.left); x <= r0.right; x++)
				target.DrawLine(d2d::point(x, r0.top), d2d::point(x, r0.bottom), black, 1 / zoom);
		}
#endif
		// grid
		if (flags.test(DISP_GRID)) {
			d2d::SolidBrush	brush(target, colour(1, 0, 0));
			d2d::point	xy0 = align(r0.TopLeft() - slice_offset, grid) + slice_offset;

			for (float y = xy0.y; y <= r0.bottom; y += grid.y)
				target.DrawLine(d2d::point(r0.left, y), d2d::point(r0.right, y), brush, 1.5f / zoom);

			for (float x = xy0.x; x <= r0.right; x += grid.x)
				target.DrawLine(d2d::point(x, r0.top), d2d::point(x, r0.bottom), brush, 1.5f / zoom);
		}
	}
#endif
}

void	ViewBitmap_base::DrawSelection(d2d::Target &target, win::Rect &rect) {
	win::Rect			r0	= rect & bitmap_rect;
	if (num_slices > 1) {
		int		slice = rect.Top() / bitmap_rect.Height();
		r0 = rect + win::Point(0, slice * 8);
	}

	d2d::SolidBrush	brush(target, colour(1, 1, 0));
	target.DrawLine(r0.TopLeft(), r0.TopRight(), brush, 1.5f / zoom);
	target.DrawLine(r0.TopRight(), r0.BottomRight(), brush, 1.5f / zoom);
	target.DrawLine(r0.BottomRight(), r0.BottomLeft(), brush, 1.5f / zoom);
	target.DrawLine(r0.BottomLeft(), r0.TopLeft(), brush, 1.5f / zoom);
}

void	ViewBitmap_base::DrawGammaCurve(d2d::Target &target, win::Rect &rect) {
	d2d::SolidBrush	white(target, d2d::colour(1,1,1));
	target.SetTransform(translate(rect.Left(), rect.Bottom()));

	float	multc			= 1 / disp_range.extent();
	float	offsetc			= -disp_range.a / disp_range.extent();
	float	invgamma		= 1 / gamma;
	float	prev	= 0;
	for (int i = 1; i < 256; i++) {
		float	next = pow(clamp(i * multc / 256 + offsetc, 0, 1), invgamma);
		target.DrawLine(
			d2d::point(float(i - 1) * rect.Width() / 256, -prev * rect.Height()),
			d2d::point(i * rect.Width() / 256, -next * rect.Height()),
			white, 2
		);
		prev	= next;
	}
}

string_accum&	ViewBitmap_base::DumpTexelInfo(string_accum &sa, float x, float y, int slice, int mip) {
	int		ix	= int(x), iy = int(y);
	int		w	= bitmap_rect.Width(), h = bitmap_rect.Height();

	if (bitmap_depth > 1) {
		if (flags.test(CUBEMAP)) {
			static const char *faces[] = {
				"Left", "Right", "Up", "Down", "Front", "Back",
			};
			sa << faces[slice];
			if (mip)
				sa << ", mip " << mip;
			sa.format(" (%i, %i)/(%.3g, %.3g) = ", ix, iy, x / w, y / h);
		} else {
			if (mip)
				sa << "mip " << mip << ' ';
			sa.format("(%i, %i, %i)/(%.3g, %.3g, %.3g) = ", ix, iy, slice, x / w, y / h, float(slice) / bitmap_depth);
		}
	} else {
		if (mip)
			sa << "mip " << mip << ' ';
		sa.format("(%i, %i)/(%.3g, %.3g) = ", ix, iy, x / w, y / h);
	}

	if (flags.test(FLIP_X))
		ix = w - 1 - ix;

	if (flags.test(FLIP_Y))
		iy = h - 1 - iy;

	if (auto hdr = bm.test_cast<HDRbitmap64>())
		return DumpTexel(sa, hdr, ix, iy);

	return DumpTexel(sa, (bitmap64*)bm, ix, iy);
}

bool ViewBitmap_base::Matches(const ISO::Type *type) {
	return type->SameAs<bitmap>()
		|| type->SameAs<bitmap64>()
		|| type->SameAs<vbitmap>()
		|| type->SameAs<HDRbitmap>()
		|| type->SameAs<HDRbitmap64>()
		|| type->SameAs<bitmap_anim>()
		|| type->Is("Texture")
		|| type->Is("IOSTexture")
		|| type->Is("X360Texture");
}

} //namespace app
