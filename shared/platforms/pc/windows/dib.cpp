#include "base/maths.h"
#include "extra/filters.h"
#include "dib.h"
#include <VersionHelpers.h>

namespace iso { namespace win {

ISO_ptr<bitmap> DIBHEADER::CreateBitmap(tag2 id, void *data) const {
	int		bitcount	= biPlanes * biBitCount, width = biWidth, height = abs(biHeight);
	uint32	scan		= ScanWidth();

	ISO_ptr<bitmap>	p(id);
	p->Create(width, height);
	for (int y = 0; y < height; y++) {
		uint8		*srce = (uint8*)data + scan * (biHeight < 0 ? y : height - 1 - y);
		ISO_rgba	*dest = p->ScanLine(y);
		switch (bitcount) {
			case 1: {
				uint8	*alpha	= srce;
				for (int x = 0; x < width; x += 8) {
					uint8	t	= *alpha++;
					int		n	= width - x;
					if (n > 8)
						n = 8;
					while (n--) {
						*dest++ = (t&0x80) ? 1 : 0;
						t <<= 1;
					}
				}
				break;
			}
			case 2:
				for (int x = 0; x < width / 4; x++) {
					dest[4 * x + 0] = (srce[x] >> 6) & 3;
					dest[4 * x + 1] = (srce[x] >> 4) & 3;
					dest[4 * x + 2] = (srce[x] >> 2) & 3;
					dest[4 * x + 3] = (srce[x] >> 0) & 3;
				}
				break;
			case 4:
				for (int x = 0; x < width / 2; x++) {
					dest[2 * x + 0] = srce[x] >> 4;
					dest[2 * x + 1] = srce[x] & 0xf;
				}
				break;
			case 8:
				for (int x = 0; x < width; x++)
					dest[x] = srce[x];
				break;
			case 16: copy_n((const Texel<R5G5B5>*)srce, dest, width);	break;
			case 24: copy_n((const Texel<B8G8R8>*)srce, dest, width);	break;
			case 32: copy_n((const Texel<B8G8R8A8>*)srce, dest, width);	break;
		}
	}
	return p;
}

void FixAlpha(const block<DIBHEADER::RGBQUAD, 2> &b) {
	for_each(b, [](DIBHEADER::RGBQUAD &p) {
		ISO_rgba	c = p;
		c	*= c.a;
		p	= c;
	});
}
struct DIBALPHA {
	uint8	b,g,r,a;
	operator float() const		{
		return a / 255.f;
	}
	void	operator=(float x)	{
		a	= max(a, x * 255);
		r	= r * a / 255;
		g	= g * a / 255;
		b	= b * a / 255;
	}
};

void MakeShadow(const block<DIBALPHA, 2> &b, float scale = 0.65f, int dx = 10, int dy = 9) {
	float	coeffs[16];
	gaussian(coeffs, 16, 1.5f, scale);

	auto	dest1 = make_auto_block<float>(b.size<1>(), b.size<2>());
	fill(dest1, 0);
	hfilter(dest1, b, coeffs, 16, dx);
	auto	dest2 = make_auto_block<float>(b.size<1>(), b.size<2>());
	fill(dest2, 0);
	vfilter(dest2, dest1, coeffs, 16, dy);

	copy(dest2.get(), b);
/*	for_each2(b, dest2.get(), [](DIBHEADER::RGBQUAD &d, HDRpixel &s) {
		ISO_rgba	c = d;
		uint8		a = max(c.a, s.a * 255);
		c	*= a;
		c.a	= a;
		d	= c;
	});*/
}

Cursor MakeMaskCursor(HBITMAP hb, const POINT &hotspot) {
	return Icon::Params().Mask(hb).Hotspot(hotspot);
}

Cursor MakeCursor(HBITMAP bm, void *bits, const POINT &hotspot, bool shadow) {
	Point	size	= Bitmap(bm).GetSize();

	if (shadow)
		MakeShadow(MakeDIBBlock((DIBALPHA*)bits, size), 0.8f, 4, 2);
	else
		FixAlpha(MakeDIBBlock((DIBHEADER::RGBQUAD*)bits, size));

	return Icon::Params().Color(bm).Mask(Bitmap(size.x, size.y, 1)).Hotspot(hotspot);
}

Cursor MakeCursor(HBITMAP hb, const POINT &hotspot, bool shadow) {
	void			*bits;
	auto			params	= Bitmap(hb).GetParams();
	Point			size	= params.Size() + Point(4, 4);
	Bitmap			bm		= Bitmap::CreateDIBSection(size, 32, 1, &bits);
	copy(MakeDIBBlock((DIBHEADER::RGBQUAD*)Bitmap(hb).GetBits(DeviceContext().Compatible(), 0, size.y), size), MakeDIBBlock((DIBHEADER::RGBQUAD*)bits, size));
	return MakeCursor(bm, bits, hotspot, shadow);
}

Cursor MakeCursor(HCURSOR hc, bool shadow) {
	Icon::Params	info(hc);
	return MakeCursor(info.Color(), info.Hotspot());
}

void GetCursorColor(DeviceContext &dc, const Icon::Params &info, const block<DIBHEADER::RGBQUAD, 2> &dest) {
	if (info.Color()) {
		Point			size1	= info.Color().GetSize();
		copy(
			MakeDIBBlock((DIBHEADER::RGBQUAD*)info.Color().GetBits(dc, 0, size1.y), size1),
			dest
		);

	} else {
		Point			size1	= info.Mask().GetSize();
		malloc_block	mask	= info.Mask().GetBits(dc, 0, size1.y);
		uint32			scan	= ((size1.x + 31) >> 3) & ~3;

		for (int y = 0, y1 = min(size1.y / 2, dest.size<2>()); y < y1; y++) {
			uint8	*and_mask	= mask + scan * y;
			uint8	*xor_mask	= mask + scan * (y + size1.y / 2);
			DIBHEADER::RGBQUAD	*d	= dest[y];
			for (int x = 0; x < scan; x++, and_mask++, xor_mask++) {
				uint8	and8	= *and_mask, xor8 = *xor_mask;
				for (int i = 0; i < 8; i++, and8 <<= 1, xor8 <<= 1, ++d)
					*d = ISO_rgba(xor8 & 0x80 ? 0xff : 0, and8 & 0x80 ? 0 : 0xff);
			}
		}
	}
}

Cursor CompositeCursor(HCURSOR hc, HBITMAP hb, const POINT offset, bool shadow) {
	void			*bits;
	Icon::Params	info(hc);
	Point			size1	= info.Size();
	Point			size2	= Bitmap(hb).GetSize();
	DeviceContext	dc		= DeviceContext().Compatible();

	Point			size	= Point(max(size1.x, size2.x + offset.x) + max(-offset.x, 0), max(size1.y, size2.y + offset.y) + max(-offset.y, 0)) + Point(4, 4);
	Bitmap			colord	= Bitmap::CreateDIBSection(size, 32, 1, &bits);
	GetCursorColor(dc, info, MakeDIBBlock((DIBHEADER::RGBQUAD*)bits, size));

	for_each2(
		MakeDIBBlock((DIBHEADER::RGBQUAD*)Bitmap(hb).GetBits(dc, 0, size2.y), size2),
		(const block<DIBHEADER::RGBQUAD, 2>&)MakeDIBBlock((DIBHEADER::RGBQUAD*)bits, colord.GetSize()).sub<1>(offset.x, size2.x).sub<2>(offset.y, size2.y),
		[](const DIBHEADER::RGBQUAD &a, DIBHEADER::RGBQUAD &b) {
			ISO_rgba	ca = a;
			ISO_rgba	cb = b;
			b = PreMultipliedBlend(cb, ca);
		}
	);
	return MakeCursor(colord, bits, info.Hotspot() + Point(max(-offset.x, 0), max(-offset.y, 0)), shadow);
}

Cursor CompositeCursor(HCURSOR hc1, HCURSOR hc2, bool shadow) {
	if (!IsWindows8Point1OrGreater())
		return hc1;

	Icon::Params	info1(hc1);
	if (hc2) {
		Icon::Params	info2(hc2);
		return CompositeCursor(hc1, info2.Color(), info1.Hotspot() - info2.Hotspot());
	}

	void			*bits;
	Point			size	= info1.Size() + Point(4, 4);
	DeviceContext	dc		= DeviceContext().Compatible();
	Bitmap			bm		= Bitmap::CreateDIBSection(size, 32, 1, &bits);
	GetCursorColor(dc, info1, MakeDIBBlock((DIBHEADER::RGBQUAD*)bits, size));
	return MakeCursor(bm, bits, info1.Hotspot(), shadow);
}

Bitmap MakeTextBitmap(HFONT hf, const char *text) {
	DeviceContext	dc		= DeviceContext().Compatible();
	dc.Select(hf);

	void	*bits;
	Point	size	= dc.GetTextExtent(text) + Point(4, 4);
	Bitmap	bm		= Bitmap::CreateDIBSection(size, 32, 1, &bits);
	Bitmap	oldd	= dc.Select(bm);

	auto	b		 = MakeDIBBlock((DIBHEADER::RGBQUAD*)bits, size);
//	fill(b, ISO_rgba(0xff, 0xff, 0xff));
	dc.Fill(Rect(Point(0, 0), size), colour::white);
	dc.FrameRect(Rect(Point(0, 0), size), Brush::Black());
	dc.SetTextColour(colour::black);
	dc.SetBackground(colour::white);
	dc.SetOpaque(false);
	dc.TextOut(Point(2, 2), text);
	dc.Select(oldd);
#if 1
	for_each(b, [](DIBHEADER::RGBQUAD &p) {
		ISO_rgba	c = p;
		c.a	= 0xff;//c.r;
		p	= c;
	});
#endif
	return bm;
}

Cursor TextCursor(HCURSOR hc, HFONT hf, const char *text, bool shadow) {
	if (!IsWindows8Point1OrGreater())
		return hc;
	return CompositeCursor(hc, MakeTextBitmap(hf, text), Point(8, 16), shadow);
}

} } // namespace iso::win
