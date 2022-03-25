#define INITGUID

#include "viewbitmap.h"

namespace app {
using namespace iso;
using namespace win;

//-----------------------------------------------------------------------------
//	histogram
//-----------------------------------------------------------------------------

void histogram::init_medians() {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 256; j++)
			order[i][j] = j;

		uint32	(&h1)[256] = h[i];
		sort(order[i], order[i] + 256, [&h1](uint8 i0, uint8 i1) { return h1[i0] < h1[i1]; });
		int	a = 0, b = 256;
		while (a < b) {
			int	c = (a + b) / 2;
			if (h1[order[i][c]] == 0)
				a = c + 1;
			else
				b = c;
		}
		first_nonzero[i] = a;
	}
}

void histogram::add(const block<ISO_rgba, 1> &b) {
	for (auto i = b.begin(), e = b.end(); i != e; ++i) {
		++h[0][i->r];
		++h[1][i->g];
		++h[2][i->b];
		++h[3][i->a];
	}
}
void histogram::add(const block<ISO_rgba, 2> &b) {
	for (auto i = b.begin(), e = b.end(); i != e; ++i)
		add(i);
}
void histogram::init(bitmap64 *bm) {
	reset();
	if (int nmips = bm->Mips()) {
		for (int m = 0; m < nmips; m++)
			add(bm->Mip(m));
	} else {
		add(bm->All());
	}
	init_medians();
}

void histogram::add(const block<HDRpixel, 1> &b, const scale_offset &s) {
	for (auto i = b.begin(), e = b.end(); i != e; ++i) {
		float4	a = {i->r, i->g, i->b, i->a};
		a = select(a == a, (a + s.o) * s.s, zero);
		++h[0][clamp(int(a.x), 0, 255)];
		++h[1][clamp(int(a.y), 0, 255)];
		++h[2][clamp(int(a.z), 0, 255)];
		++h[3][clamp(int(a.w), 0, 255)];
	}
}
void histogram::add(const block<HDRpixel, 2> &b, const scale_offset &s) {
	for (auto i = b.begin(), e = b.end(); i != e; ++i)
		add(i, s);
}

void histogram::init(HDRbitmap64 *bm, HDRpixel minpix, HDRpixel maxpix) {
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

// draw histogram
void DrawHistogram(d2d::Target &target, win::Rect &rect, const histogram &hist, int c) {
	uint32	max_val = 0;
	if (c & CHAN_R)
		max_val = max(max_val, hist.max_value(0));
	if (c & CHAN_G)
		max_val = max(max_val, hist.max_value(1));
	if (c & CHAN_B)
		max_val = max(max_val, hist.max_value(2));
	if (c & CHAN_A)
		max_val = max(max_val, hist.max_value(3));

	target.SetTransform(translate(rect.Left(), rect.Bottom()) * scale(rect.Width() / 256.f, -float(rect.Height()) / max_val));
	target.Fill(d2d::rect(0, 0, 256, max_val), d2d::SolidBrush(target, d2d::colour(0,0,0,0.5f)));

	com_ptr<ID2D1SolidColorBrush>	cols[4];
	target.CreateBrush(&cols[0], d2d::colour(1,0,0));
	target.CreateBrush(&cols[1], d2d::colour(0,1,0));
	target.CreateBrush(&cols[2], d2d::colour(0,0,1));
	target.CreateBrush(&cols[3], d2d::colour(0,0,0));

	for (int i = 0; i < 256; i++) {
		uint32	h[4]		= {
			c & CHAN_R ? hist.h[0][i] : 0,
			c & CHAN_G ? hist.h[1][i] : 0,
			c & CHAN_B ? hist.h[2][i] : 0,
			c & CHAN_A ? hist.h[3][i] : 0
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
	num_slices			= flags.test(SEPARATE_SLICES) ? bitmap_depth : 1;
	int	width, height;
	if (auto hdr = test_cast<HDRbitmap64>(bm)) {
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
	if (p.IsType<bitmap64>()) {
		bm		= p;

	} else if (p.IsType<HDRbitmap64>()) {
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
		if (p2->IsType<bitmap64>())
			bm = *p2;
		else if (p2.IsType<HDRbitmap64>())
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

	if (auto hdr = test_cast<HDRbitmap64>(bm)) {
		bitmap_depth	= hdr->Depth();
		flags.set(CUBEMAP, hdr->IsCube());
		if (hdr->Mips())
			flags.set_all(HAS_MIPS | SHOW_MIPS);

#if 1
		stats<float4>	s;
		HDRpixel	*p	= hdr->ScanLine(0);
		for (int n = hdr->Width() * hdr->Height(); n--; p++) {
			s.add(*p);
		}
		float	q0 = invphi_approx(0.05f);
		float	q1 = invphi_approx(0.95f);
		HDRpixel	p0, p1;
		assign(p0, s.mean() + s.sigma() * q0);
		assign(p1, s.mean() + s.sigma() * q1);

#else
		HDRpixel	*p	= hdr->ScanLine(0);
		HDRpixel	p0	= *p, p1 = p0;
		for (int n = hdr->Width() * hdr->Height(); n--; p++) {
			p0 = min(p0, *p);
			p1 = max(p1, *p);
		}
		p0 = max(p0, -FLT_MAX);
		p1 = min(p1, FLT_MAX);
#endif
		chan_range		= interval<float>(min(min(min(p0.r, p0.g), p0.b), 0), max(max(max(p1.r, p1.g), p1.b), 1));
		alpha_range		= interval<float>(min(p0.a, 0), max(p1.a, 1));
		disp_range		= interval<float>(-chan_range.a / (chan_range.b - chan_range.a), 1);

		col_range		= interval<HDRpixel>(p0, p1);
		col_range.a.a	= col_range.a.a * chan_range.b / alpha_range.b;
		col_range.b.a	= col_range.b.a * chan_range.b / alpha_range.b;

	} else {
		bitmap64	*bm = this->bm;
		bitmap_depth	= bm->Depth();
		flags.set(CUBEMAP, bm->IsCube());
		if (bm->Mips())
			flags.set_all(HAS_MIPS | SHOW_MIPS);

		ISO_rgba	*p	= bm->ScanLine(0);
		ISO_rgba	p0	= *p, p1 = p0;
		for (int n = bm->Width() * bm->Height(); n--; p++) {
			p0 = min(p0, *p);
			p1 = max(p1, *p);
		}

		disp_range		= alpha_range = chan_range = interval<float>(0, 1);
		col_range		= interval<HDRpixel>(p0, p1);
	}

	num_slices	= bitmap_depth;
	if (bitmap_depth > 1)
		flags.set_all(SEPARATE_SLICES);

	UpdateBitmapRect();
	return true;
}

void ViewBitmap_base::UpdateBitmap(d2d::Target &target, ID2D1Bitmap *d2d_bitmap) {
	int			c		= channels;
	bool		alpha	= (c & CHAN_A) != 0;
	int			width	= bitmap_rect.Width();
	int			height	= bitmap_rect.Height() * num_slices;

	if (auto hdr = test_cast<HDRbitmap64>(bm)) {
		malloc_block	buffer(sizeof(d2d::texel) * width * height);
		d2d::texel*		d		= buffer;
		float			multc	= 255 / (disp_range.extent() * (chan_range.b - chan_range.a));
		float			offsetc = chan_range.a + disp_range.a * (chan_range.b - chan_range.a);
		float			multa	= 255;
		float			offseta = 0;
		if (c == CHAN_A) {
			multa	= 255 / (disp_range.extent() * (alpha_range.b - alpha_range.a));
			offseta	= alpha_range.a + disp_range.a * (alpha_range.b - alpha_range.a);
		}
		for (int y = 0; y < height; y++) {
			const HDRpixel	*s = hdr->ScanLine(y);
			for (int i = width; i--; s++, d++) {
				d->r = int(clamp((s->r - offsetc) * multc, 0.f, 255.f));
				d->g = int(clamp((s->g - offsetc) * multc, 0.f, 255.f));
				d->b = int(clamp((s->b - offsetc) * multc, 0.f, 255.f));
				d->a = int(clamp((s->a - offseta) * multa, 0.f, 255.f));
			}
		}
		d2d_bitmap->CopyFromMemory(0, buffer, width * sizeof(d2d::texel));

	} else if (bitmap64 *bm = this->bm) {
		if (bm->IsPaletted()) {
			if (!prev_bm.Width())
				prev_bm.Create(width, height);
			ISO_rgba	*clut	= bm->Clut();
			ISO_rgba	*s		= bm->ScanLine(0), *d = prev_bm.ScanLine(0);
			for (int n = width * height; --n; s++, d++) {
				if (clut[s->r].a != 0)
					*d = clut[s->r];
			}
			target.FillBitmap(d2d_bitmap, prev_bm.All());
		} else {
			target.FillBitmap(d2d_bitmap, bm->All());
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

	auto			hdr			= test_cast<HDRbitmap64>(bm);
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

	if (auto hdr = test_cast<HDRbitmap64>(bm))
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
