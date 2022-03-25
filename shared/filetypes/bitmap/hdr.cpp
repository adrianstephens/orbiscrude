#include "bitmap.h"
#include "iso/iso_convert.h"

namespace iso {

uint16 Scan(const block<HDRpixel, 1> &block, uint16 flags) {
	bool		alpha	= !!(flags & BMF_ALPHA);
	bool		grey	= !!(flags & BMF_GREY);
	for (const HDRpixel *i = block.begin(), *e = block.end(); i != e; ++i) {
		if (!alpha && i->HasAlpha()) {
			alpha = true;
			if (!grey)
				break;
		}
		if (grey && !i->IsGrey()) {
			grey = false;
			if (alpha)
				break;
		}
	}
	return	(alpha	? BMF_ALPHA : 0)
		|	(grey	? BMF_GREY	: 0);
}

uint16 Scan(const block<HDRpixel, 2> &block, uint16 flags) {
	for (auto i = block.begin(), e = block.end(); flags != BMF_ALPHA && i != e; ++i)
		flags = Scan(i, flags);
	return flags;
}

void BoxFilter(const block<HDRpixel, 2> &srce, const block<HDRpixel, 2> &dest, bool alpha1bit) {
	int	w0 = max(srce.size<1>(), 1u),	h0 = max(srce.size<2>(), 1u);
	int	w1 = max(w0 >> 1, 1),		h1 = max(h0 >> 1, 1);
	for (int y = 0; y < h1; y++) {
		HDRpixel	*s0 = srce[y * 2], *s1 = srce[y * 2 + int(y * 2 < h0 - 1)];
		HDRpixel	*d = dest[y * 1];
		for (int x = w1; x--; d++, s0 += 2, s1 += 2)
			*d = (s0[0] + s0[1] + s1[0] + s1[1]) / 4;
	}
}

void BoxFilter(const block<HDRpixel, 3> &srce, const block<HDRpixel, 3> &dest, bool alpha1bit) {
	int	w0 = max(srce.size<1>(), 1u), h0 = max(srce.size<2>(), 1u), d0 = max(srce.size<3>(), 1u);
	int	w1 = max(w0 >> 1, 1),		h1 = max(h0 >> 1, 1),		d1 = max(d0 >> 1, 1);

	for (int d = 0; d < d1; d++) {
		block<HDRpixel, 2>	srce2a	= srce[d * 2], srce2b = srce[d * 2 + int(d * 2 < d0 - 1)];
		block<HDRpixel, 2>	dest2	= dest[d * 1];
		for (int y = 0; y < h1; y++) {
			HDRpixel	*s0	= srce2a[y * 2], *s1 = srce2a[y * 2 + (y * 2 < h0 - 1)];
			HDRpixel	*s2	= srce2b[y * 2], *s3 = srce2b[y * 2 + (y * 2 < h0 - 1)];
			HDRpixel	*d	= dest2[y];
			for (int x = w1; x--; d++, s0 += 2, s1 += 2, s2 += 2, s3 += 2)
				*d = (s0[0] + s0[1] + s1[0] + s1[1] + s2[0] + s2[1] + s3[0] + s3[1]) / 8;
		}
	}
}

ISO_ptr<bitmap> HDR2bitmap(HDRbitmap &hdr) {
	ISO_ptr<bitmap> bm(NULL);
	int					w = hdr.Width(), h = hdr.Height();
	bm->Create(w, h, hdr.Flags(), hdr.Depth());
	bm->SetMips(hdr.Mips());
	for (int y = 0; y < h; y++) {
		ISO_rgba	*s = bm->ScanLine(y);
		HDRpixel	*d = hdr.ScanLine(y);
		for (int x = 0; x < w; x++) {
			s[x].r = uint8(clamp(d[x].r, 0.f, 1.f) * 255.f);
			s[x].g = uint8(clamp(d[x].g, 0.f, 1.f) * 255.f);
			s[x].b = uint8(clamp(d[x].b, 0.f, 1.f) * 255.f);
			s[x].a = uint8(clamp(d[x].a, 0.f, 1.f) * 255.f);
		}
	}
	return bm;
}

ISO_ptr<HDRbitmap> bitmap2HDR(bitmap &bm) {
	ISO_ptr<HDRbitmap> hdr(NULL);
	int					w = bm.Width(), h = bm.Height();
	hdr->Create(w, h, bm.Flags(), bm.Depth());
	hdr->SetMips(bm.Mips());
	for (int y = 0; y < h; y++) {
		ISO_rgba	*s = bm.ScanLine(y);
		HDRpixel	*d = hdr->ScanLine(y);
		for (int x = 0; x < w; x++) {
			d[x].r = s[x].r / 255.f;
			d[x].g = s[x].g / 255.f;
			d[x].b = s[x].b / 255.f;
			d[x].a = s[x].a / 255.f;
		}
	}
	return hdr;
}

ISO_ptr<bitmap> vbitmap2bitmap(vbitmap &vb) {
	ISO_ptr<bitmap> bm(NULL);
	int	w	= vb.Width(), h = vb.Height(), d = vb.Depth(), faces = 1;
	int	h2	= h * d;
	if (!vb.IsVolume()) {
		faces = d;
	}

	if (int	mips = vb.Mips()) {
		bm->Create(w, h2, vb.Flags() | BMF_MIPS | BMF_CLEAR, d);
		bm->SetMips(mips);
		for (int f = 0; f < faces; f++) {
			for (int m = 0; m <= mips; m++)
				vbitmap_loc(vb).set_z(f).set_mip(m).get(GetMip(bm->Slice(f), m));
		}
	} else {
		bm->Create(w, h2, vb.Flags(), d);
		for (int f = 0; f < faces; f++)
			vbitmap_loc(vb).set_z(f).get(bm->Slice(f));
	}
	return bm;
}

ISO_ptr<HDRbitmap> vbitmap2HDR(vbitmap &vb) {
	ISO_ptr<HDRbitmap> bm(NULL);
	int	w	= vb.Width(), h = vb.Height(), d = vb.Depth(), faces = 1;
	int	h2	= h * d;
	if (!vb.IsVolume()) {
		faces = d;
	}

	if (int	mips = vb.Mips()) {
		bm->Create(w * 2, h2, vb.Flags(), d);
		bm->SetMips(mips);
		for (int f = 0; f < faces; f++) {
			for (int m = 0; m <= mips; m++)
				vbitmap_loc(vb).set_z(f).set_mip(m).get(GetMip(bm->Slice(f), m));
		}
	} else {
		bm->Create(w, h2, vb.Flags(), d);
		for (int f = 0; f < faces; f++)
			vbitmap_loc(vb).set_z(f).get(bm->Slice(f));
	}

	return bm;
}

ISO_ptr<bitmap2> vbitmap2bitmap2(vbitmap &vb) {
	return vb.format & vbitmap_format::FLOAT
		? ISO_ptr<bitmap2>(0, vbitmap2HDR(vb))
		: ISO_ptr<bitmap2>(0, vbitmap2bitmap(vb));
}
} // namespace iso

using namespace iso;

initialise HDRinit(
	ISO_get_cast(HDR2bitmap),
	ISO_get_cast(bitmap2HDR),
	ISO_get_cast(vbitmap2bitmap),
	ISO_get_cast(vbitmap2HDR),
	ISO_get_cast(vbitmap2bitmap2)
);
