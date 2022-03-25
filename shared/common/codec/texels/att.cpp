#include "att.h"

namespace iso {

// word 1: XRRRRRGG GGGBBBBB (1-bit method, 5-bit R, G, B)
// word 2: RRRRRGGG GGGBBBBB (5-bit R, 6-bit G, 5-bit B)

void ATTrgb::Decode(ISO_rgba *color) const {
	ISO_rgba	c0 = Texel<R5G5B5>(v0);
	ISO_rgba	c1 = Texel<R5G6B5>(v1);

	if (v0 & 0x8000) {
		color[2] = c0;
		color[3] = c1;
		color[0] = ISO_rgba(0,0,0);
		color[1] = ISO_rgba(c0.r - c1.r / 4, c0.g - c1.g / 4, c0.b - c1.b / 4);
	} else {
		color[0] = c0;
		color[3] = c1;
		color[1] = InterpCol(c0, c1, 2, 1);
		color[2] = InterpCol(c0, c1, 1, 2);
	}
}

void ATTrec::Decode(const block<ISO_rgba, 2> &block) const {
	ISO_rgba	color[4];
	ATTrgb::Decode(color);

	uint32	d	= bits;
	int		w	= block.size<1>(), h = block.size<2>();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < 4; x++) {
			if (x < w)
				block[y][x] = color[d & 3];
			d >>= 2;
		}
	}
}

}//namespace iso
