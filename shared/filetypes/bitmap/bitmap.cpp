#include "bitmap.h"

namespace iso {

uint16 Scan(const block<ISO_rgba, 1> &block, uint16 flags) {
	bool		alpha	= !!(flags & BMF_ALPHA);
	bool		grey	= !!(flags & BMF_GREY);
	for (const ISO_rgba *i = block.begin(), *e = block.end(); i != e; ++i) {
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

uint16 Scan(const block<ISO_rgba, 2> &block, uint16 flags) {
	for (auto i = block.begin(), e = block.end(); flags != BMF_ALPHA && i != e; ++i)
		flags = Scan(i, flags);
	return flags;
}

void Unpalette(ISO_rgba	*p, size_t n, const ISO_rgba *clut, uint32 flags) {
	if (clut) {
		if (flags & BMF_SEPALPHA) {
			while (n--) {
				ISO_rgba	c = clut[p->r];
				p->r = c.r;
				p->g = c.g;
				p->b = c.b;
				++p;
			}
		} else {
			while (n--) {
				*p = clut[p->r];
				++p;
			}
		}
	}
}

void BoxFilter(const block<ISO_rgba, 2> &srce, const block<ISO_rgba, 2> &dest, bool alpha1bit) {
	int	w0 = max(srce.size<1>(), 1u), h0 = max(srce.size<2>(), 1u);
	int	w1 = max(w0 >> 1, 1),		h1 = max(h0 >> 1, 1);
	for (int y = 0; y < h1; y++) {
		ISO_rgba	*s0 = srce[y * 2], *s1 = srce[y * 2 + int(y * 2 < h0 - 1)];
		ISO_rgba	*d	= dest[y * 1];

		for (int x = w1; x--; d++, s0 += 2, s1 += 2) {
			if (alpha1bit) {
				int	a = s0[0].a + s0[1].a + s1[0].a + s1[1].a;
				if (a < 255 * 2) {
					*d = ISO_rgba(0,0,0,0);
				} else {
					*d = ISO_rgba(
						(s0[0].r * s0[0].a + s0[1].r * s0[1].a + s1[0].r * s1[0].a + s1[1].r * s1[1].a) / a,
						(s0[0].g * s0[0].a + s0[1].g * s0[1].a + s1[0].g * s1[0].a + s1[1].g * s1[1].a) / a,
						(s0[0].b * s0[0].a + s0[1].b * s0[1].a + s1[0].b * s1[0].a + s1[1].b * s1[1].a) / a,
						255
					);
				}
			} else {
				*d = ISO_rgba(
					(s0[0].r + s0[1].r + s1[0].r + s1[1].r) / 4,
					(s0[0].g + s0[1].g + s1[0].g + s1[1].g) / 4,
					(s0[0].b + s0[1].b + s1[0].b + s1[1].b) / 4,
					(s0[0].a + s0[1].a + s1[0].a + s1[1].a) / 4
				);
			}
		}
	}
}

void BoxFilter(const block<ISO_rgba, 3> &srce, const block<ISO_rgba, 3> &dest, bool alpha1bit) {
	int	w0 = max(srce.size<1>(), 1u), h0 = max(srce.size<2>(), 1u), d0 = max(srce.size<3>(), 1u);
	int	w1 = max(w0 >> 1, 1),		h1 = max(h0 >> 1, 1),		d1 = max(d0 >> 1, 1);

	for (int d = 0; d < d1; d++) {
		block<ISO_rgba, 2>	srce2a	= srce[d * 2], srce2b = srce[d * 2 + int(d * 2 < d0 - 1)];
		block<ISO_rgba, 2>	dest2	= dest[d * 1];

		for (int y = 0; y < h1; y++) {
			ISO_rgba	*s0	= srce2a[y * 2], *s1 = srce2a[y * 2 + (y * 2 < h0 - 1)];
			ISO_rgba	*s2	= srce2b[y * 2], *s3 = srce2b[y * 2 + (y * 2 < h0 - 1)];
			ISO_rgba	*d	= dest2[y];

			for (int x = w1; x--; d++, s0 += 2, s1 += 2, s2 += 2, s3 += 2) {
				if (alpha1bit) {
					int	a = s0[0].a + s0[1].a + s1[0].a + s1[1].a + s2[0].a + s2[1].a + s3[0].a + s3[1].a;
					if (a < 255 * 2) {
						*d = ISO_rgba(0,0,0,0);
					} else {
						*d = ISO_rgba(
							( s0[0].r * s0[0].a + s0[1].r * s0[1].a + s1[0].r * s1[0].a + s1[1].r * s1[1].a
							+ s2[0].r * s2[0].a + s2[1].r * s2[1].a + s3[0].r * s3[0].a + s3[1].r * s3[1].a) / a,
							( s0[0].g * s0[0].a + s0[1].g * s0[1].a + s1[0].g * s1[0].a + s1[1].g * s1[1].a
							+ s2[0].g * s2[0].a + s2[1].g * s2[1].a + s3[0].g * s3[0].a + s3[1].g * s3[1].a) / a,
							( s0[0].b * s0[0].a + s0[1].b * s0[1].a + s1[0].b * s1[0].a + s1[1].b * s1[1].a
							+ s2[0].b * s2[0].a + s2[1].b * s2[1].a + s3[0].b * s3[0].a + s3[1].b * s3[1].a) / a,
							255
						);
					}
				} else {
					*d = ISO_rgba(
						(s0[0].r + s0[1].r + s1[0].r + s1[1].r + s2[0].r + s2[1].r + s3[0].r + s3[1].r) / 8,
						(s0[0].g + s0[1].g + s1[0].g + s1[1].g + s2[0].g + s2[1].g + s3[0].g + s3[1].g) / 8,
						(s0[0].b + s0[1].b + s1[0].b + s1[1].b + s2[0].b + s2[1].b + s3[0].b + s3[1].b) / 8,
						(s0[0].a + s0[1].a + s1[0].a + s1[1].a + s2[0].a + s2[1].a + s3[0].a + s3[1].a) / 8
					);
				}
			}
		}
//		BoxFilter(pixels + (d * 2 + 0) * pitch2, pitch, w0, h0, alpha1bit);
//		BoxFilter(pixels + (d * 2 + 1) * pitch2, pitch, w0, h0, alpha1bit);
	}
}

uint32 GetBitmapFlags() {
	uint32			flags	= 0;
	ISO::Browser	vars	= ISO::root("variables");
	if (str(vars["uvmode"].GetString()) == "clamp")
		flags |= BMF_UVCLAMP;
	if (vars["nocompress"].GetInt())
		flags |= BMF_NOCOMPRESS;
	if (vars["nomip"].GetInt())
		flags |= BMF_NOMIP;
	return flags;
}

}//namespace iso
