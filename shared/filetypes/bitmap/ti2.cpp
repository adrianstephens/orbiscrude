#include "bitmapfile.h"

//-----------------------------------------------------------------------------
//	Playstation 2 bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

#include "bitmap.h"

#define SCE_GS_PSMCT32			0		//0
#define SCE_GS_PSMCT24			1		//1
#define SCE_GS_PSMCT16			2		//2
#define SCE_GS_PSMCT16S			10		// 8 | 2
#define SCE_GS_PSMT8			19		//16 | 3
#define SCE_GS_PSMT4			20		//16 | 4
#define SCE_GS_PSMT8H			27		//24 | 3
#define SCE_GS_PSMT4HL			36		//32 | 4
#define SCE_GS_PSMT4HH			44		//32 | 8 | 4
#define SCE_GS_PSMZ32			48		//48 | 0
#define SCE_GS_PSMZ24			49		//48 | 1
#define SCE_GS_PSMZ16			50		//48 | 2
#define SCE_GS_PSMZ16S			58		//48 | 8 | 2

#define GS_TEX0_TBP0_M	0x3fff
#define GS_TEX0_TBW_M	0x003f
#define GS_TEX0_PSM_M	0x003f
#define GS_TEX0_TW_M	0x000f
#define GS_TEX0_TH_M	0x000f
#define GS_TEX0_TCC_M	0x0001
#define GS_TEX0_TFX_M	0x0003
#define GS_TEX0_CBP_M	0x3fff
#define GS_TEX0_CPSM_M	0x000f
#define GS_TEX0_CSM_M	0x0001
#define GS_TEX0_CSA_M	0x001f
#define GS_TEX0_CLD_M	0x0007

#define GS_TEX0_TBP0_O	0
#define GS_TEX0_TBW_O	14
#define GS_TEX0_PSM_O	20
#define GS_TEX0_TW_O	26
#define GS_TEX0_TH_O	30
#define GS_TEX0_TCC_O	34
#define GS_TEX0_TFX_O	35
#define GS_TEX0_CBP_O	37
#define GS_TEX0_CPSM_O	51
#define GS_TEX0_CSM_O	55
#define GS_TEX0_CSA_O	56
#define GS_TEX0_CLD_O	61

#define SCE_GS_SET_TEX0(tbp, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa, cld) \
	(uint64(tbp)			| (uint64(tbw)	<< 14) | \
	(uint64(psm)	<< 20)	| (uint64(tw)	<< 26) | \
	(uint64(th)		<< 30)	| (uint64(tcc)	<< 34) | \
	(uint64(tfx)	<< 35)	| (uint64(cbp)	<< 37) | \
	(uint64(cpsm)	<< 51)	| (uint64(csm)	<< 55) | \
	(uint64(csa)	<< 56)	| (uint64(cld)	<< 61))

#define SCE_GS_SET_TEX1(lcm, mxl, mmag, mmin, mtba, l, k) \
	(uint64(lcm)			| (uint64(mxl)	<< 2)	| \
	(uint64(mmag)	<< 5)	| (uint64(mmin)	<< 6)	| \
	(uint64(mtba)	<< 9)	| (uint64(l)	<< 19)	| \
	(uint64(k)		<< 32))

#define SCE_GS_SET_MIPTBP(tbp1, tbw1, tbp2, tbw2, tbp3, tbw3) \
	(uint64(tbp1)			| (uint64(tbw1) << 14)	| \
	(uint64(tbp2)	<< 20)	| (uint64(tbw2) << 34)	| \
	(uint64(tbp3)	<< 40)	| (uint64(tbw3) << 54))

#define SCE_GS_SET_TEXAFBAPABE(ta0, aem, ta1, pabe, fba) \
	(uint32(ta0)			| ((uint32)(aem)	<< 15)	| \
	(uint32(ta1)	<< 16)	| ((uint32)(pabe)	<< 30)	| \
	(uint32(fba)	<< 31))

#define SCE_GS_SET_TEXCLUT(cbw, cou, cov) \
	((uint32(cbw) | (uint32(cou) << 6) | (uint32(cov) << 12))

#define SCE_GS_SET_TRXDIR(xdr) (uint64(xdr))

#define SCE_GS_SET_TRXPOS(ssax, ssay, dsax, dsay, dir) \
	(uint64(ssax)        | (uint64(ssay) << 16) | \
	(uint64(dsax) << 32) | (uint64(dsay) << 48) | \
	(uint64(dir) << 59))

#define SCE_GS_SET_TRXREG(rrw, rrh) \
	(uint64(rrw) | (uint64(rrh) << 32))

//typedef void	uint128;

enum {
	PS2TEXF_TYPE			= 0x003f,
	PS2TEXF_SWIZZLED		= 0x0040,
	PS2TEXF_COMPRESSED		= 0x0080,
	PS2TEXF_MIPS			= 0x0700,
	PS2TEXF_POINTSAMPLE		= 0x0800,
	PS2TEXF_PAGEALIGNCLUT	= 0x1000,
	PS2TEXF_DESTINATION		= 0x8000,
};

class PS2_Texture {
public:
	uint16		format;
	uint16		width, height;
	uint16		clutsize;
	uint16		nblocks;
	uint16		refcount;
	uint32		data;
	uint64		tex0, miptbp1, miptbp2;

	void		Init();
	PS2_Texture() : format(0), refcount(0), data(0)	{}
	void		Set(uint16 f, uint16 w, uint16 h, uint16 cs);

	uint16		Format()		const	{ return format;					}
	int			Type()			const	{ return format & PS2TEXF_TYPE;		}
	int			MipLevels()		const	{ return (format & PS2TEXF_MIPS) >> 8;	}
	uint16		Width()			const	{ return width;						}
	uint16		Height()		const	{ return height;					}
	uint16		ClutSize()		const	{ return clutsize;					}

	uint64		Tex0()			const	{ return tex0;						}
	int			TBP()			const	{ return tex0 & 0x3fff;				}
	int			FBP()			const	{ return TBP() >> 5;				}
	int			BW()			const	{ return (tex0 >> 14) & 63;			}

	uint32		NumBlocks()		const	{ return nblocks;					}
	uint32		NumPages()		const	{ return nblocks >> 5;				}
	uint32		ClutBlocks()	const	{ return clutsize <= 16 ? 1 : (clutsize + 127) / 128 * 2; }
	uint32		VRAMSize()		const	{ return nblocks << 8;				}
};

class PS2_Texture2 {
public:
	uint16		format;
	uint16		width, height;
	uint16		vram_blocks;
	uint32		data_size;
	uint32		data;
	uint64		tex0, tex1, miptbp1, miptbp2;

	uint16		Format()		const	{ return format;					}
	int			Type()			const	{ return format & PS2TEXF_TYPE;		}
	int			MipLevels()		const	{ return (format & PS2TEXF_MIPS) >> 8; }
	uint16		Width()			const	{ return width;						}
	uint16		Height()		const	{ return height;					}

	uint64		Tex0()			const	{ return tex0;						}
	uint64		Tex1()			const	{ return tex1;						}
	int			TBP()			const	{ return tex0 & 0x3fff;				}
	int			CBP()			const	{ return (tex0 >> 37) & 0x3fff;		}
	int			FBP()			const	{ return TBP() >> 5;				}
	int			BW()			const	{ return (tex0 >> 14) & 63;			}

	uint32		NumBlocks()		const	{ return vram_blocks;				}
};

class TI2 {
	enum		BPP { BPP_32, BPP_24, BPP_16, BPP_8, BPP_4 };
	static int	Twiddle(int i)		{ return i & ~(8 | 16) | ((i & 8) << 1) | ((i & 16) >> 1); }
public:

	struct PS2_TexBuffer {
		uint32	bp;
		uint32	bw;
	};

	static void	GetBlockBufferRect( istream_ref file, unsigned char *buffer, unsigned dbw, unsigned x0, unsigned y0, unsigned w, unsigned h);
	static void	GetBlockBuffer(		istream_ref file, unsigned char *buffer, int nblocks);

	static int	CalculateVRAM(int format, int w, int h, int mips, int bp, PS2_TexBuffer *result);
	static int	CalculateVRAMrev(int format, int w, int h, int mips, int bp, PS2_TexBuffer *result);

	static void	ReadClut( ISO_rgba *clut, int n, bool fourbit, istream_ref file);
	static void	ReadImage(const block<ISO_rgba, 2> &bm, istream_ref file, int format, int nblocks);
};



#define SCE_GIF_PACKED			0
#define SCE_GIF_REGLIST			1
#define SCE_GIF_IMAGE			2
#define SCE_GIF_PACKED_AD 		0x0e

#define SCE_GS_TRXPOS			0x51
#define SCE_GS_TRXREG			0x52
#define SCE_GS_TRXDIR			0x53

#define SCE_GIF_SET_TAG(nloop, eop, pre, prim, flg, nreg) \
	((uint64)(nloop) | ((uint64)(eop)<<15) | ((uint64)(pre) << 46) | \
	((uint64)(prim)<<47) | ((uint64)(flg)<<58) | ((uint64)(nreg)<<60))


#define TI2F_MIPMAPPED			(1<<0)
#define TI2F_SWIZZLED			(1<<1)
#define TI2F_MIPMAPTABLE		(1<<2)

struct sceColour32 {
	unsigned char r, g, b, a;
	sceColour32()				{}
	sceColour32(unsigned char _r, unsigned char  _g, unsigned char  _b, unsigned char  _a) : r(_r), g(_g), b(_b), a(_a) {}
//	sceColour32(const ISO_rgba &c)	{ *this = *(sceColour32*)&c; }
//	sceColour32(const ISO_rgba &c)	: r(c.r), g(c.g), b(c.b), a(c.a == 255 ? 0 : ((c.a/2)|128)) {}
	sceColour32(const ISO_rgba &c)	: r(c.r), g(c.g), b(c.b), a((c.a + 1) / 2) {}
	operator ISO_rgba() const		{ return ISO_rgba(r, g, b, a < 128 ? a * 2 : 255); }
};

struct sceColour24 {
	unsigned char r, g, b;
	sceColour24()				{}
	sceColour24(const ISO_rgba &c)	: r(c.r), g(c.g), b(c.b) {}
	operator ISO_rgba() const		{ return ISO_rgba(r,g,b,255); }
};

struct sceColour16 {
	WORD	r:5, g:5, b:5, a:1;
	sceColour16()				{}
	sceColour16(const ISO_rgba &c)	: r(c.r>>3), g(c.g>>3), b(c.b>>3), a(c.a>128) {}
	sceColour16(const ISO_rgba &c, int _a)	: r(c.r>>3), g(c.g>>3), b(c.b>>3), a(_a) {}
	operator ISO_rgba() const		{ return ISO_rgba(r<<3, g<<3, b<<3, a ? 255 : 0); }
};

struct sceColour8 {
	unsigned char i;
	sceColour8()				{}
	sceColour8(const ISO_rgba &c) : i(c.r) {}
	operator ISO_rgba() const		{ return ISO_rgba(i); }
};

__int64		g_miptbp1, g_miptbp2;

int TI2::CalculateVRAM(int format, int w, int h, int mips, int bp, PS2_TexBuffer *result)
{
	int		type	= format & 7;
	int		bws		= type < 2 ? 3 : type < 4 ? 4 : 5,
			bhs		= type < 3 ? 3 : 4;

	int		bx, by, sx, sy, end;
	int		nblocks	= 0;

	// block x & y
	bx				= (w - 1) >> bws;
	by				= (h - 1) >> bhs;

	// switch for 4 and 16 bit
	if (type == 2 || type == 4) {
		int	t = bx; bx = by; by = t;
	}

	// 'swizzled' block x & y
	sx				= (bx & 1) | ((bx & 2) << 1) | ((bx & 4) << 2);
	sy				= ((by & 1) << 1) | ((by & 2) << 2);

	// last block accessed + 1
	end				= bp + ((bx / 8 + 1) * (by / 4 + 1) - 1) * 32	// whole pages (last one might be whole as well...)
					+ (sx | sy) + 1;								// offset into last page

	nblocks			= end;
	bx++;
	by++;
	sx				= (bx & 1) | ((bx & 2) << 1) | ((bx & 4) << 2);
	sy				= ((by & 1) << 1) | ((by & 2) << 2);
	bp				+= (bx / 8) * (by / 4) * 32 + ((sx && sx < sy) || sy == 0 ? sx : sy);

	for (int i = 0; i < mips; i++) {
		w		>>= 1;
		h		>>= 1;

		if (result) {
			result[i].bp	= bp;
			result[i].bw	= bws == 3 ? (w + 63) / 64 : (w + 127) / 128 * 2;
		}

		bx		= (w - 1) >> bws;
		by		= (h - 1) >> bhs;
		if (type == 2 || type == 4) {
			int	t = bx; bx = by; by = t;
		}
		sx		= (bx & 1) | ((bx & 2) << 1) | ((bx & 4) << 2);
		sy		= ((by & 1) << 1) | ((by & 2) << 2);
		end		= bp + ((bx / 8 + 1) * (by / 4 + 1) - 1) * 32 + (sx | sy) + 1;
		if (end > nblocks)
			nblocks	= end;
		bx++;
		by++;
		sx		= (bx & 1) | ((bx & 2) << 1) | ((bx & 4) << 2);
		sy		= ((by & 1) << 1) | ((by & 2) << 2);
		bp		+= (bx / 8) * (by / 4) * 32 + ((sx && sx < sy) || sy == 0 ? sx : sy);

	}
	return (nblocks + 31) & ~31;
//	return nblocks;
}


int CalcBufSize(int psm, int w, int h)
{
	static unsigned char
	CT32SizeTbl[4][8] = {
		{  1,  2,  5,  6, 17, 18, 21, 22 },
		{  3,  4,  7,  8, 19, 20, 23, 24 },
		{  9, 10, 13, 14, 25, 26, 29, 30 },
		{ 11, 12, 15, 16, 27, 28, 31, 32 }
	},
	CT16SizeTbl[8][4] = {
		{  1,  3,  9, 11 },
		{  2,  4, 10, 12 },
		{  5,  7, 13, 15 },
		{  6,  8, 14, 16 },
		{ 17, 19, 25, 27 },
		{ 18, 20, 26, 28 },
		{ 21, 23, 29, 31 },
		{ 22, 24, 30, 32 }
	};

	int size = 0;
	int row;

	switch(psm) {
		case SCE_GS_PSMCT32:
		case SCE_GS_PSMCT24:
		case SCE_GS_PSMT8H:
		case SCE_GS_PSMT4HL:
		case SCE_GS_PSMT4HH:
			row = ((w+63)>>6)<<5;
			size = CT32SizeTbl[((h-1)>>3)&3][((w-1)>>3)&7] + (row-32) + ((h-1)>>5)*row;
			break;

		case SCE_GS_PSMCT16:
			row = ((w+63)>>6)<<5;
			size = CT16SizeTbl[((h-1)>>3)&7][((w-1)>>4)&3] + (row-32) + ((h-1)>>6)*row;
			break;

		case SCE_GS_PSMT8:
			row = ((w+63)>>6)<<4;
			size = CT32SizeTbl[((h-1)>>4)&3][((w-1)>>4)&3] + (row-16) + ((h-1)>>6)*row;
			break;

		case SCE_GS_PSMT4:
			row = ((w+127)>>7)<<5;
			size = CT16SizeTbl[((h-1)>>4)&7][((w-1)>>5)&3] + (row-32) + ((h-1)>>7)*row;
			break;
	}

	return size;
}

int TI2::CalculateVRAMrev(int format, int w, int h, int mips, int bp, PS2_TexBuffer *result)
{
	int		type	= format & 7;

	for (int i = mips - 1; i >= 0; i--)	{
		int mw = w >> i;
		int mh = h >> i;

		// Page align top level within upload in case used as render target.
		// Note that the entire upload will also be page aligned in that case.
		// If other levels need to be used as render targets, we have a problem.
		if (i == 0 && (format & PS2TEXF_DESTINATION))
			bp = (bp + 31) & ~31;

		result[i].bp	= bp;
		result[i].bw	= type < 3 ? (mw + 63) / 64 : (mw + 127) / 128 * 2;

		bp += CalcBufSize(format & PS2TEXF_TYPE, mw, mh);
	}

	// Set the total size of the texture.
	return bp;
}

void TI2::ReadClut(ISO_rgba *clut, int n, bool fourbit, istream_ref file)
{
	for (int i = 0; i < n; i++) {
		sceColour32	col;
		file.read(col);
		if (fourbit) {
			if (i < 16)
				clut[i] = col;
		} else {
			clut[Twiddle(i)] = col;
		}
	}
}

void TI2::ReadImage(const block<ISO_rgba, 2> &bm, istream_ref file, int format, int nblocks)
{
	BPP			bpp			= BPP(format & 7);
	int			bpp2		= bpp == BPP_4 ? 1 : (4 - bpp) * 2;
	int			mult		= bpp >= BPP_8 || bpp == BPP_24 ? 8 : bpp == BPP_16 ? 4 : 2;
	int			w0			= bm.size<1>(),
				w			= (w0 + mult - 1) & -mult,
				w2			= w * bpp2 / 2;

	if (format & PS2TEXF_COMPRESSED) {
		int				pagew		= 128;
		int				pageh		= 64;
		int				npages		= (nblocks + 31) / 32;
		malloc_block	buffer(npages * 8192);

		GetBlockBuffer(file, buffer, npages * 12);

		unsigned char *srce = buffer;
		for (int y0 = 0; y0 < bm.size<2>(); y0 += pageh) {
			for (int x0 = 0; x0 < bm.size<1>(); x0 += pagew) {
				unsigned char	vq_pal[1024], indices[64*32];

				for (int i = 0; i < 256; ++i) {
					int j = (i&0x8f)|((i&0x10)<<2)|((i&0x60)>>1);
					memcpy(vq_pal + i * 4, srce + (j << 2), 4);
				}
				srce += 4 * 256;

				// Get vq indices
				for (int i = 0; i < 64 * 32; ++i) {
					indices[i] = srce[
						((i&0x20)<<5)|((i&0x440)>>1)|((i&0x10)<<4)|((i&0x308)>>2)|
						(((i&0x100)>>4)^((i&0x80)>>3)^((i&0x4)<<2))|
						((i&0x3)<<2)|((i&0x80)>>7)
					];
				}
				srce += 8 * 256;

				int	xend = min(bm.size<1>()  - x0, 128);
				int	yend = min(bm.size<2>() - y0, 64);

				for (int y1 = 0; y1 < yend; ++y1) {
					ISO_rgba* dst = bm[y0 + y1].begin() + x0;
					for (int x1 = 0; x1 < xend; ++x1) {
						unsigned index = indices[
							(((y1&~0x3)>>1)|(y1&1)) * 64 +
							(((x1&~0xf)>>1)|((y1&0x4)^((y1&0x2)<<1)^(x1&0x4)) | (x1&0x3))
						];
						*dst++ = vq_pal[(index<<2)+(((x1&0x8)>>2)|((y1&0x2)>>1))];
					}
				}
			}
		}

	} else if (format & PS2TEXF_SWIZZLED) {
		int			pagew		= bpp2 >= 4 ? 64 : 128,
					pageh		= bpp2 >= 6 ? 32 : bpp2 >= 2 ? 64 : 128;
		int			count		= bm.size<1>() == 64 && pagew == 128 ? 4096 : 8192;
		malloc_block srce(count);
		for (int py = 0; py < bm.size<2>(); py += pageh) {
			for (int px = 0; px < bm.size<1>(); px += pagew) {
				int	i;
				file.readbuff(srce, count);
				switch (bpp) {
					case BPP_4:
						for (i = 0; i < count; i++ ) {
							int	x = (((i >> 2) & 7) | ((i << 3) & 0x18) | ((i >> 6) & 0x60)) ^ ((i >> 7) & 4),
								y = ((i >> 8) & 1) | ((i >> 7) & 0xc) | ((i >> 1) & 0x70);
							bm[py + y][px + x]				= ((unsigned char*)srce)[i] & 15;
							bm[py + y + 2][px + (x ^ 4)]	= ((unsigned char*)srce)[i] >> 4;
						}
						break;
					case BPP_8:
						for (i = 0; i < count; i++ ) {
							int	x = (((i >> 2) & 7) | ((i << 2) & 8) | ((i >> 1) & 0x70)) ^ (((i << 2) ^ (i >> 7)) & 4),
								y = ((i >> 8) & 1) | ((i << 1) & 2) | ((i >> 7) & 0x3c);
							bm[py + y][px + x] = ((sceColour8*)srce)[i];
						}
						break;
					case BPP_16:
						for (i = 0; i < 4096; i++ ) {
							int	x = ((i >> 1) & 7) | ((i << 3) & 8) | ((i >> 6) & 0x30),
								y = ((i >> 7) & 7) | ((i >> 1) & 0x38);
							bm[py + y][px + x] = ((sceColour16*)srce)[i];
						}
						break;
					case BPP_32:
						for (i = 0; i < 2048; i++ ) {
							int	x = i & 0x3f,
								y = i >> 6;
							bm[py + y][px + x] = ((sceColour32*)srce)[i];
						}
						break;
				}
			}
		}
	} else {
		malloc_block srce(w2);
		for (int y = 0; y < bm.size<2>(); y++) {
			ISO_rgba	*dest = bm[y].begin();
			int		x;
			file.readbuff(srce, w2);
			switch (bpp) {
				case BPP_4: {
					unsigned char	*p = (unsigned char*)srce;
					for (x = 0; x < bm.size<1>(); x += 2, p++) {
						dest[x+0] = *p & 15;
						dest[x+1] = *p >> 4;
					}
					break;
				}
				case BPP_8:
					for (x = 0; x < w0; x++ )
						dest[x] = ((sceColour8*)srce)[x];
					break;
				case BPP_16:
					for (x = 0; x < w0; x++ )
						dest[x] = ((sceColour16*)srce)[x];
					break;
				case BPP_24:
					for (x = 0; x < w0; x++ )
						dest[x] = ((sceColour24*)srce)[x];
					break;
				case BPP_32:
					for (x = 0; x < w0; x++ )
						dest[x] = ((sceColour32*)srce)[x];
					break;
			}
		}
	}
}

void TI2::GetBlockBufferRect(istream_ref file, unsigned char *buffer, unsigned dbw, unsigned x0, unsigned y0, unsigned w, unsigned h)
{
	for (unsigned y = y0; y < y0 + h; ++y) {
		for (unsigned x = x0; x < x0+w; x += 8)	{
			// Copy 8 32-bit pixels (each group of 8 is contiguous, and x and w are always multiples of 8)
			unsigned	offset = (
							(/*(x&0x7)|*/((y & 0x7) << 3) | ((x & 0x8) << 3) | ((y & 0x8) << 4) | ((x & 0x10) << 4) | ((y & 0x10) << 5) | ((x & ~0x1F) << 5))
							+ (((y >> 5) * dbw) << 11)
						) << 2;
			file.readbuff(buffer + offset, 8 * 4);
//			memset(buffer + offset, 0x55, 8 * 4);
		}
	}
}

void TI2::GetBlockBuffer(istream_ref file, unsigned char *buffer, int nblocks)
{
	int	y		= (nblocks >> 5) * 32;

	if (y) {
		GetBlockBufferRect(file, buffer, 1, 0, 0, 64, y);
		buffer	+= y * 256;
	}

	if (nblocks & 16) {
		GetBlockBufferRect(file, buffer, 1, 0, 0, 32, 32);
		buffer	+= 4096;
	}

	if (nblocks & 8) {
		GetBlockBufferRect(file, buffer, 1, 0, 0, 32, 16);
		buffer	+= 2048;
	}

	if (nblocks & 4) {
		GetBlockBufferRect(file, buffer, 1, 0, 0, 16, 16);
		buffer	+= 1024;
	}

	if (nblocks & 2) {
		GetBlockBufferRect(file, buffer, 1, 0, 0, 16, 8);
		buffer	+= 512;
	}

	if (nblocks & 1) {
		GetBlockBufferRect(file, buffer, 1, 0, 0, 8, 8);
	}
}

//-----------------------------------------------------------------------------
//
//	TI2
//
//-----------------------------------------------------------------------------

struct TI2_HEADER {
	BYTE	type;
	BYTE	flags;
	WORD	clut;
	WORD	width, height;
	LONG	size;
	WORD	pad[2];
};

class TI2FileHandler : BitmapFileHandler {
	const char	*GetExt()			{ return "ti2";			}
	const char	*GetDescription()	{ return "PS2 Bitmap";	}

	ISO_ptr<void> Read(tag id, istream_ref file)
	{
		TI2_HEADER	header;
		bool	valid = file.read(header) && (
			((header.type == SCE_GS_PSMCT32 || header.type == SCE_GS_PSMCT24 || header.type == SCE_GS_PSMCT16) && header.clut == 0)
		||	(header.type == SCE_GS_PSMT8 && header.clut <= 256)
		||	(header.type == SCE_GS_PSMT4 && header.clut <=  32)
			);

		if (!valid || header.width > 2048 || header.width == 0 || header.height == 0)
			return ISO_NULL;

		ISO_ptr<bitmap>	bm(id);
		bm->Create(header.width, header.height);

		if (header.flags & TI2F_MIPMAPTABLE)
			file.seek(16);

		if (header.clut) {
			bool	onebit = true;
			bm->CreateClut(header.clut);
			TI2::ReadClut(bm->Clut(), header.clut, header.type == SCE_GS_PSMT4, file);
		}

		TI2::ReadImage(*bm, file, header.type | (header.flags & TI2F_SWIZZLED ? PS2TEXF_SWIZZLED : 0), 0);
		return bm;
	}
} ti2;

//-----------------------------------------------------------------------------
//
//	TI3
//
//-----------------------------------------------------------------------------

void PS2_Texture::Init() {
	TI2::PS2_TexBuffer		mipbuf[6];

	int		type	= format & 7;
	uint64	bw		= type < 3 ? (width + 63) / 64 : (width + 127) / 128 * 2;
	uint64	log2w	= log2(width), log2h	= log2(height);
	int		nmips	= MipLevels();

	for (int i = nmips; i < 6; i++)
		mipbuf[i].bw = mipbuf[i].bp = 0;

	nblocks			= TI2::CalculateVRAM(format, width,	height, nmips, 0, mipbuf);
//	tex0			= SCE_GS_SET_TEX0(0, bw, Type(), log2w, log2h, 1, 0, nblocks, 0, 0, 0, 1);
	tex0			= SCE_GS_SET_TEX0(ClutBlocks(), bw, Type(), log2w, log2h, type != 1, 0, 0, 0, 0, 0, 1);
	miptbp1			= SCE_GS_SET_MIPTBP(mipbuf[0].bp, mipbuf[0].bw, mipbuf[1].bp, mipbuf[1].bw, mipbuf[2].bp, mipbuf[2].bw);
	miptbp2			= SCE_GS_SET_MIPTBP(mipbuf[3].bp, mipbuf[3].bw, mipbuf[4].bp, mipbuf[4].bw, mipbuf[5].bp, mipbuf[5].bw);
}

void PS2_Texture::Set(uint16 f, uint16 w, uint16 h, uint16 cs) {
	format		= f;
	width		= w;
	height		= h;
	clutsize	= cs <= 16 ? (cs + 7) & ~7 : (cs + 31) & ~31;

	Init();
}

class TI3FileHandler : BitmapFileHandler {
	const char	*GetExt()			{ return "ti3";				}
	const char	*GetDescription()	{ return "PS2 Bitmap (2)";	}

	ISO_ptr<void> Read(tag id, istream_ref file) {
		PS2_Texture	tex;
		file.read(tex);

		int		type	= tex.Type();
		bool	valid	=
						((type == SCE_GS_PSMCT32 || type == SCE_GS_PSMCT24 || type == SCE_GS_PSMCT16) && tex.clutsize == 0)
					||	(type == SCE_GS_PSMT8 && tex.clutsize <= 256)
					||	(type == SCE_GS_PSMT4 && tex.clutsize <=  32);

		if (!valid || tex.refcount != 0 || tex.width > 2048 || tex.width == 0 || tex.height == 0)
			return ISO_NULL;

		ISO_ptr<bitmap>	bm(id);
		bm->Create(tex.width, tex.height);

		file.seek(tex.data);
		if (int clutsize = tex.clutsize) {
			if (tex.Type() == SCE_GS_PSMT4 && clutsize > 16)
				clutsize = 16;

			bool	onebit = true;
			bm->CreateClut(clutsize);
			TI2::ReadClut(bm->Clut(), tex.clutsize, tex.Type() == SCE_GS_PSMT4, file);
		}

		TI2::ReadImage(*bm, file, tex.format, tex.nblocks);
		return bm;
	}
} ti3;
