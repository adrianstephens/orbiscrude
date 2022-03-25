#ifndef GTF_H
#define GTF_H

#include "stream.h"
#include "bitmap.h"

enum GXCITexFmt {
    GX_TF_C4    = 0x8,
    GX_TF_C8    = 0x9,
    GX_TF_C14X2 = 0xa
};

enum GXTexFmt {
#define _GX_TF_CTF     0x20 /* copy-texture-format only */
#define _GX_TF_ZTF     0x10 /* Z-texture-format */

    GX_TF_I4     = 0x0,
    GX_TF_I8     = 0x1,
    GX_TF_IA4    = 0x2,
    GX_TF_IA8    = 0x3,
    GX_TF_RGB565 = 0x4,
    GX_TF_RGB5A3 = 0x5,
    GX_TF_RGBA8  = 0x6,
    GX_TF_CMPR   = 0xE,

    GX_CTF_R4    = 0x0 | _GX_TF_CTF,
    GX_CTF_RA4   = 0x2 | _GX_TF_CTF,
    GX_CTF_RA8   = 0x3 | _GX_TF_CTF,
    GX_CTF_YUVA8 = 0x6 | _GX_TF_CTF,
    GX_CTF_A8    = 0x7 | _GX_TF_CTF,
    GX_CTF_R8    = 0x8 | _GX_TF_CTF,
    GX_CTF_G8    = 0x9 | _GX_TF_CTF,
    GX_CTF_B8    = 0xA | _GX_TF_CTF,
    GX_CTF_RG8   = 0xB | _GX_TF_CTF,
    GX_CTF_GB8   = 0xC | _GX_TF_CTF,

    GX_TF_Z8     = 0x1 | _GX_TF_ZTF,
    GX_TF_Z16    = 0x3 | _GX_TF_ZTF,
    GX_TF_Z24X8  = 0x6 | _GX_TF_ZTF,

    GX_CTF_Z4    = 0x0 | _GX_TF_ZTF | _GX_TF_CTF,
    GX_CTF_Z8M   = 0x9 | _GX_TF_ZTF | _GX_TF_CTF,
    GX_CTF_Z8L   = 0xA | _GX_TF_ZTF | _GX_TF_CTF,
    GX_CTF_Z16L  = 0xC | _GX_TF_ZTF | _GX_TF_CTF,

    GX_TF_A8     = GX_CTF_A8, // to keep compatibility
};

enum GXTlutFmt {
    GX_TL_IA8    = 0x0,
    GX_TL_RGB565 = 0x1,
    GX_TL_RGB5A3 = 0x2,
    GX_MAX_TLUTFMT
};

enum GXTlutSize {
    GX_TLUT_16	= 1, // number of 16 entry blocks.
    GX_TLUT_32	= 2,
    GX_TLUT_64	= 4,
    GX_TLUT_128	= 8,
    GX_TLUT_256	= 16,
    GX_TLUT_512	= 32,
    GX_TLUT_1K	= 64,
    GX_TLUT_2K	= 128,
    GX_TLUT_4K	= 256,
    GX_TLUT_8K	= 512,
    GX_TLUT_16K	= 1024
};

#define	GX_TF_XFB	 0x2F

namespace iso {

class TPL {
public:
	static ISO_rgba		I(uint8 i) {
		return ISO_rgba(i,i,i,i);
	}
	static ISO_rgba		IA(uint8 i, uint8 a) {
		return ISO_rgba(i,i,i,a);
	}
	static ISO_rgba		RGB5A3(uint16 x) {
		return x & 0x8000
			? ISO_rgba(extend_bits<5,8>(x >> 10), extend_bits<5,8>(x >> 5), extend_bits<5,8>(x), 255)
			: ISO_rgba(extend_bits<4,8>(x >>  8), extend_bits<4,8>(x >> 4), extend_bits<4,8>(x), extend_bits<3,8>(x >> 12));
	}

	static ISO_rgba		RGB565(uint16 x) {
		return ISO_rgba(extend_bits<5,8>(x >> 11), extend_bits<6,8>(x >> 5), extend_bits<5,8>(x), 255);
	}

	static uint16		RGB5A3(ISO_rgba c) {
		return c.a > 224
			? 0x8000			| ((c.r>>3) << 10) | ((c.g>>3) << 5) | (c.b>>3)
			: ((c.a>>5) << 12)	| ((c.r>>4) <<  8) | ((c.g>>4) << 4) | (c.b>>4);
	}

	static uint16		RGB565(ISO_rgba c) {
		return ((c.r>>3) << 11) | ((c.g>>2) << 5) | (c.b>>3);
	}
	static int	GetSize(	GXTexFmt format, uint32 width, uint32 height);
	static int	GetFormat(	GXTexFmt format, uint32 &tilew, uint32 &tileh, uint32 &bpp);
	static void	ReadImage(	bitmap *pbm, istream_ref file, GXTexFmt format);
	static void	WriteImage(	bitmap *pbm, ostream_ref file, GXTexFmt format);
	static void	ReadClut(	ISO_rgba *clut, int n, istream_ref file, GXTlutFmt format);
	static void	WriteClut(	ISO_rgba *clut, int n, ostream_ref file, GXTlutFmt format);

	static void	Read(const block<ISO_rgba, 2> &block, void *srce, uint32 srce_swizzle, GXTexFmt format);
	static void	Write(const block<ISO_rgba, 2> &block, void *dest, uint32 srce_swizzle, GXTexFmt format);
};

} //namespace iso

#endif
