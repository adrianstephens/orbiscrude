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
	int	w0 = max(srce.size<1>(), 1u),	h0 = max(srce.size<2>(), 1u);
	int	w1 = max(w0 >> 1, 1),			h1 = max(h0 >> 1, 1);

	for (int y = 0; y < h1; y++) {
		uint8x4		*s0 = (uint8x4*)srce[y * 2].begin(),
					*s1 = (uint8x4*)srce[y * 2 + int(y * 2 < h0 - 1)].begin(),
					*d	= (uint8x4*)dest[y * 1].begin();

		for (int x = w1; x--; d++, s0 += 2, s1 += 2) {
			auto	t0	= to<uint16>(s0[0]),
					t1	= to<uint16>(s0[1]),
					t2	= to<uint16>(s1[0]),
					t3	= to<uint16>(s1[1]);
				
			if (alpha1bit) {
				int	a = t0.w + t1.w + t2.w + t3.w;
				*d = to<uint8>(select(a < 255 * 2, zero, concat((t0.xyz * t0.w + t1.xyz * t1.w + t2.xyz * t2.w + t3.xyz * t3.w) / a, 255)));
			} else {
				*d = to<uint8>((t0 + t1 + t2 + t3) >> 2);
			}
		}
	}
}

void BoxFilter(const block<ISO_rgba, 3> &srce, const block<ISO_rgba, 3> &dest, bool alpha1bit) {
	int	w0 = max(srce.size<1>(), 1u),	h0 = max(srce.size<2>(), 1u),	d0 = max(srce.size<3>(), 1u);
	int	w1 = max(w0 >> 1, 1),			h1 = max(h0 >> 1, 1),			d1 = max(d0 >> 1, 1);

	for (int d = 0; d < d1; d++) {
		block<ISO_rgba, 2>	srce2a	= srce[d * 2], srce2b = srce[d * 2 + int(d * 2 < d0 - 1)];
		block<ISO_rgba, 2>	dest2	= dest[d * 1];

		for (int y = 0; y < h1; y++) {
			uint8x4		*s0	= (uint8x4*)srce2a[y * 2].begin(),
						*s1 = (uint8x4*)srce2a[y * 2 + (y * 2 < h0 - 1)].begin(),
						*s2	= (uint8x4*)srce2b[y * 2].begin(),
						*s3 = (uint8x4*)srce2b[y * 2 + (y * 2 < h0 - 1)].begin(),
						*d	= (uint8x4*)dest2[y].begin();

			for (int x = w1; x--; d++, s0 += 2, s1 += 2, s2 += 2, s3 += 2) {
				if (alpha1bit) {
					int	a = s0[0].w + s0[1].w + s1[0].w + s1[1].w + s2[0].w + s2[1].w + s3[0].w + s3[1].w;
					*d	= select(a < 255 * 2, zero, concat(
						(s0[0].xyz * s0[0].w + s0[1].xyz * s0[1].w + s1[0].xyz * s1[0].w + s1[1].xyz * s1[1].w + s2[0].xyz * s2[0].w + s2[1].xyz * s2[1].w + s3[0].xyz * s3[0].w + s3[1].xyz * s3[1].w) / a,
						255)
					);
				} else {
					*d = (s0[0] + s0[1] + s1[0] + s1[1] + s2[0] + s2[1] + s3[0] + s3[1]) / 8;
				}
			}
		}
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
