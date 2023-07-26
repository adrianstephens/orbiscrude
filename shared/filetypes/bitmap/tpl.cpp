#include "tpl.h"
#include "base/bits.h"
#include "codec/texels/dxt.h"
#include "iso/iso_files.h"

//-----------------------------------------------------------------------------
//	Nintendo WII bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

static const float rgb_mat[3][4] = {
	{1.1641443538998835f, -0.001788897713620724f, 1.5957862054353396f, -222.65796505077816f},
	{1.1641443538998837f, -0.39144276434223674f, -0.813482068507793f, 135.60406894240566f},
	{1.1641443538998837f, 2.017825509600896f, -0.0012458394791287075f, -276.74850743798436f}
};

struct WiiYUV2  {
	uint8	y0;
	uint8	u;
	uint8	y1;
	uint8	v;

	void	ToRGB(ISO_rgba *dest) {
		float	r = rgb_mat[0][1] * u + rgb_mat[0][2] * v + rgb_mat[0][3];
		float	g = rgb_mat[1][1] * u + rgb_mat[1][2] * v + rgb_mat[1][3];
		float	b = rgb_mat[2][1] * u + rgb_mat[2][2] * v + rgb_mat[2][3];
		float	i0 = y0 * rgb_mat[0][0];
		float	i1 = y1 * rgb_mat[0][0];
		dest[0] = ISO_rgba(clamp(int(r + i0),0,255), clamp(int(g + i0),0,255), clamp(int(b + i0),0,255));
		dest[1] = ISO_rgba(clamp(int(r + i1),0,255), clamp(int(g + i1),0,255), clamp(int(b + i1),0,255));
	}
};

int TPL::GetFormat(GXTexFmt format, uint32 &tilew, uint32 &tileh, uint32 &bpp) {
	bool	intensity	= false;
	bool	alpha		= false;

	switch ((int)format) {
		case GX_TF_I4:
			intensity	= true;
		case GX_TF_C4:
		case GX_CTF_R4:
		case GX_CTF_Z4:
		case GX_TF_CMPR:
			tilew		= tileh = 8;
			bpp			= 4;
			break;

		case GX_TF_IA4:
			alpha		= true;
		case GX_TF_I8:
			intensity	= true;
		case GX_TF_C8:
		case GX_CTF_A8:
		case GX_CTF_R8:
		case GX_CTF_G8:
		case GX_CTF_B8:
		case GX_TF_Z8:
		case GX_CTF_Z8M:
		case GX_CTF_Z8L:
		case GX_CTF_RA4:
			tilew		= 8;
			tileh		= 4;
			bpp			= 8;
			break;

		case GX_TF_IA8:
			intensity	= true;
		case GX_CTF_RA8:
		case GX_TF_RGB5A3:
			alpha		= true;
		case GX_CTF_RG8:
		case GX_CTF_GB8:
		case GX_TF_Z16:
		case GX_CTF_Z16L:
		case GX_TF_RGB565:
		case GX_TF_C14X2:
			tilew		= tileh = 4;
			bpp			= 16;
			break;
		case GX_TF_Z24X8:
		case GX_TF_RGBA8:
			alpha		= true;
			tilew		= tileh = 4;
			bpp			= 32;
			break;
	}
	return (alpha ? 1 : 0) | (intensity ? 2 : 0);
}

int TPL::GetSize(GXTexFmt format, uint32 width, uint32 height) {
	uint32 tilew = 8, tileh = 8, tiles = 32;
	switch ((int)format) {
		case GX_TF_XFB:
			return align(width, 16) * 2 * height;
		case GX_TF_Z24X8:
		case GX_TF_RGBA8:
			tiles		= 64;
		case GX_TF_IA8:
		case GX_TF_RGB5A3:
		case GX_TF_RGB565:
		case GX_TF_C14X2:
		case GX_TF_Z16:
		case GX_CTF_RA8:
		case GX_CTF_RG8:
		case GX_CTF_GB8:
		case GX_CTF_Z16L:
			tilew		= 4;
		case GX_TF_IA4:
		case GX_TF_I8:
		case GX_TF_C8:
		case GX_CTF_A8:
		case GX_CTF_R8:
		case GX_CTF_G8:
		case GX_CTF_B8:
		case GX_TF_Z8:
		case GX_CTF_Z8M:
		case GX_CTF_Z8L:
		case GX_CTF_RA4:
			tileh		= 4;
	}
	return ((width + tilew - 1) / tilew) * ((height + tileh - 1) / tileh) * tiles;
}

void TPL::Read(const block<ISO_rgba, 2> &b, void *srce, uint32 srce_swizzle, GXTexFmt format) {
	uint32	width	= b.size<1>(), height = b.size<2>();

	if (format == GX_TF_XFB) {
		int	scan = align(width, 16) / 2;
		for (uint32 y = 0; y < height; y++) {
			ISO_rgba	*dest	= b[y].begin();
			WiiYUV2		*yuv	= (WiiYUV2*)srce + scan * y;
			for (uint32 x = 0; x < width; x += 2, dest += 2, yuv++)
				yuv->ToRGB(dest);
		}
		return;
	}

	uint32	tilew, tileh, bpp;
	GetFormat(format, tilew, tileh, bpp);

	for (uint32 y = 0; y < height; y += tileh) {
		for (uint32 x = 0; x < width; x += tilew) {
			block<ISO_rgba, 2>	b2 = b.sub<1>(x, tilew).sub<2>(y, tileh);
			uint32	w0 = b2.size<1>(), h0 = b2.size<2>();
			uint8	(&tile)[32] = *(uint8(*)[32])srce;
			srce	= (uint8*)srce + 32;

			switch ((int)format) {
				case GX_TF_I4:
				case GX_TF_C4: {
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0 += 2) {
							uint8	byte	= tile[y0 * 4 + x0 / 2];
							b[y + y0][x + x0 + 0] = extend_bits<4,8>(byte>>4);
							b[y + y0][x + x0 + 1] = extend_bits<4,8>(byte);
						}
					}
					break;
				}

				case GX_TF_I8:
				case GX_TF_C8: {
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							int	a = tile[y0 * 8 + x0];
							b[y + y0][x + x0] = ISO_rgba(a, a);
						}
					}
					break;
				}

				case GX_TF_IA4: {
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							uint8	byte	= tile[y0 * 8 + x0];
							b[y + y0][x + x0] = ISO_rgba(extend_bits<4,8>(byte), extend_bits<4,8>(byte >> 4));
						}
					}
					break;
				}

				case GX_TF_IA8: {
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							uint8	*byte	= tile + y0 * 8 + x0 * 2;
							b[y + y0][x + x0] = ISO_rgba(byte[1], byte[0]);
						}
					}
					break;
				}

				case GX_TF_RGB565: {
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							uint8	*p		= tile + y0 * 8 + x0 * 2;
							uint16	word	= (p[0] << 8) | p[1];
							b[y + y0][x + x0] = RGB565(word);
						}
					}
					break;
				}

				case GX_TF_RGB5A3: {
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							uint8	*p		= tile + y0 * 8 + x0 * 2;
							uint16	word	= (p[0] << 8) | p[1];
							b[y + y0][x + x0] = RGB5A3(word);
						}
					}
					break;
				}

				case GX_TF_RGBA8: {
					uint8	(&tile2)[32] = *(uint8(*)[32])srce;
					srce	= (uint8*)srce + 32;
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							uint32	i		= y0 * 8 + x0 * 2;
							b[y + y0][x + x0] = ISO_rgba(tile[i + 1], tile2[i + 0], tile2[i + 1], tile[i + 0]);
						}
					}
					break;
				}

				case GX_TF_C14X2: {	//14 bit palette entry (not supported)
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							uint8	*p		= tile + y0 * 8 + x0 * 2;
							uint16	word	= (p[0] << 8) | p[1];
							b[y + y0][x + x0] = word;
						}
					}
					break;
				}

				case GX_TF_CMPR: {	// S3 compression...
					DXT1rec	dxt[4];
					for (int i = 0; i < 4; i++) {
						uint8		*b		= tile + i * 8;
						uint32		t		= *(uint32*)(b + 4);
						dxt[i].v0	= (b[0] << 8) | b[1];
						dxt[i].v1	= (b[2] << 8) | b[3];
						dxt[i].bits	= ((t & 0x03030303) << 6)
									| ((t & 0x0c0c0c0c) << 2)
									| ((t & 0x30303030) >> 2)
									| ((t & 0xc0c0c0c0) >> 6);
					}
					dxt[0].Decode(b.sub<1>(x + 0, 4).sub<2>(y + 0, 4), true);
					dxt[1].Decode(b.sub<1>(x + 4, 4).sub<2>(y + 0, 4), true);
					dxt[2].Decode(b.sub<1>(x + 0, 4).sub<2>(y + 4, 4), true);
					dxt[3].Decode(b.sub<1>(x + 4, 4).sub<2>(y + 4, 4), true);
					break;
				}
			}
		}
	}
}

void TPL::Write(const block<ISO_rgba, 2> &b, void *dest, uint32 srce_swizzle, GXTexFmt format) {
	uint32	width	= b.size<1>(), height = b.size<2>();
	uint32	tilew, tileh, bpp;
	GetFormat(format, tilew, tileh, bpp);

	int		r = 0;//, g = 1, b = 2, a = 3;
	if (srce_swizzle) {
		r = srce_swizzle & 3;
	}

	for (uint32 y = 0; y < height; y += tileh) {
		for (uint32 x = 0; x < width; x += tilew) {
			block<ISO_rgba, 2>	b2 = b.sub<1>(x, tilew).sub<2>(y, tileh);
			uint32	w0 = b2.size<1>(), h0 = b2.size<2>();
			uint8	(&tile)[32] = *(uint8(*)[32])dest;
			clear(tile);
			dest	= (uint8*)dest + 32;

			switch ((int)format) {
				case GX_TF_I4:
				case GX_TF_C4:
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0 += 2)
							tile[y0 * 4 + x0 / 2] = (b2[y0][x0][r] << 4) | (b2[y0][x0 + 1][r] & 0x0F);
					}
					break;

				case GX_TF_I8:
				case GX_TF_C8:
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++)
							tile[y0 * 8 + x0] = b2[y0][x0][r];
					}
					break;

				case GX_TF_IA4:
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							ISO_rgba	c = b2[y0][x0];
							tile[y0 * 8 + x0] = (c.a & 0xf0) | (c.r >> 4);
						}
					}
					break;

				case GX_TF_IA8:
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							ISO_rgba	c	= b2[y0][x0];
							((uint16*)tile)[y0 * 4 + x0] = c.a + (c.r << 8);
						}
					}
					break;

				case GX_TF_RGB565:
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++)
							((uint16be*)tile)[y0 * 4 + x0] = RGB565(b2[y0][x0]);
					}
					break;

				case GX_TF_RGB5A3:
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++)
							((uint16be*)tile)[y0 * 4 + x0] = RGB5A3(b2[y0][x0]);
					}
					break;

				case GX_TF_RGBA8: {
					uint8	(&tile2)[32] = *(uint8(*)[32])dest;
					clear(tile2);
					dest	= (uint8*)dest + 32;
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++) {
							uint32		i	= y0 * 8 + x0 * 2;
							ISO_rgba	c	= b2[y0][x0];
							tile[i + 0]	= c.a;
							tile[i + 1]	= c.r;
							tile2[i + 0]= c.g;
							tile2[i + 1]= c.b;
						}
					}
					break;
				}

				case GX_TF_C14X2:	//14 bit palette entry (not supported)
					for (uint32 y0 = 0; y0 < h0; y0++) {
						for (uint32 x0 = 0; x0 < w0; x0++)
							((uint16be*)tile)[y0 * 4 + x0] = uint16(b2[y0][x0].r);
					}
					break;

				case GX_TF_CMPR: {
					struct DXTrgb_be {
						uint16be	v0, v1;
						uint32		bits;
					} *dst		= (DXTrgb_be*)tile;

					int	blank	= 0;
					for (int i = 0; i < 4; i++) {
						DXT1rec	dxt1;
						block<ISO_rgba, 2>	b3 = b2.sub<1>((i & 1) * 4, 4).sub<2>((i & 2) * 2, 4);
						uint32	mask		= block_mask<4, 4>(b3.size<1>(), b3.size<2>());
						uint32	trans_mask	= 0;
						for (uint32 i = 0, m = block_mask<4, 4>(b3.size<1>(), b3.size<2>()); m; i++, m >>= 1)
							trans_mask |= ((m & 1) && b3[i >> 2][i & 3].a < 128) << i;

						blank			|= int(trans_mask == mask) << i;
						dxt1.Encode(b3, DXTENC_WIIFACTORS);
						uint32	t	= dxt1.bits;
						dst[i].v0	= dxt1.v0;
						dst[i].v1	= dxt1.v1;
						dst[i].bits	= ((t & 0x03030303) << 6) | ((t & 0x0c0c0c0c) << 2) | ((t & 0x30303030) >> 2) | ((t & 0xc0c0c0c0) >> 6);
					}
					if (blank) {
						int	blankh = blank & ~(((blank & 0x5) << 1) | ((blank & 0xa) >> 1));
						int	blankv = blank & ~(((blank & 0x3) << 2) | ((blank & 0xc) >> 2));
						for (int i = 0; i < 4; i++) {
							if ((blankh | blankv) & (1 << i)) {
								const DXTrgb_be	&b = dst[i ^ (blankh & (1 << i) ? 1 : 2)];
								if (b.v0 <= b.v1) {
									dst[i].v0 = b.v0;
									dst[i].v1 = b.v1;
								} else {
									dst[i].v0 = b.v1;
									dst[i].v1 = b.v0;
								}
							}
						}
					}
					break;
				}
			}
		}
	}
}

void TPL::WriteClut(ISO_rgba *clut, int n, ostream_ref file, GXTlutFmt format) {
	while (n--) {
		file.write(uint16be(format == GX_TL_IA8 ? ((clut->a << 8) | clut->r) : format == GX_TL_RGB565 ? RGB565(*clut) : RGB5A3(*clut)));
		clut++;
	}
}

void TPL::ReadClut(ISO_rgba *clut, int n, istream_ref file, GXTlutFmt format) {
	while (n--) {
		uint16	w	= file.get<uint16be>();
		*clut++	= format == GX_TL_IA8 ? ISO_rgba(w & 255, w >> 8) : format == GX_TL_RGB565 ? RGB565(w) : RGB5A3(w);
	}
}

void TPL::ReadImage(bitmap *pbm, istream_ref file, GXTexFmt format) {
	int		size	= GetSize(format, pbm->Width(), pbm->Height());
	void	*buffer	= malloc(size);
	file.readbuff(buffer, size);
	Read(*pbm, buffer, 0, format);
	free(buffer);
}

void TPL::WriteImage(bitmap *pbm, ostream_ref file, GXTexFmt format) {
	int		size	= GetSize(format, pbm->Width(), pbm->Height());
	void	*buffer	= malloc(size);
	Write(*pbm, buffer, 0, format);
	file.writebuff(buffer, size);
	free(buffer);
}

const uint32 TplVersion			= 2142000;				  // tpl version number:

class TPLFileHandler : FileHandler {
	const char*		GetExt() override { return "tpl";			}
	const char*		GetDescription() override { return "Wii Texture";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;

	struct Header : bigendian_types {
		uint32	ver;
		uint32	ntex;
		uint32	offset;
	};

	struct	TextureDescriptor : bigendian_types {
		uint32 im, pal;
	};

	struct	ImageDescriptor : bigendian_types {
		uint16	height;
		uint16	width;
		uint32	format;
		uint32	offset;

		uint32	wraps;
		uint32	wrapt;

		uint32	minfilter;
		uint32	magfilter;
		uint32	lodbias;
		uint8	lodedge;
		uint8	minlod;
		uint8	maxlod;
		uint8	pad;
	};

	struct	PaletteDescriptor : bigendian_types {
		uint16	numentry;
		uint16	pad;
		uint32	format;
		uint32	offset;
	};

	ISO_ptr<bitmap>		Read1(tag id, istream_ref file, const TextureDescriptor &td);

} tpl;

ISO_ptr<void> TPLFileHandler::Read(tag id, istream_ref file) {
	Header	header(file.get());
	if (header.ver != TplVersion)
		return ISO_NULL;

	int	ntex = header.ntex;
	if (ntex == 1)
		return Read1(id, file, file.get());

	TextureDescriptor *td = new TextureDescriptor[ntex];
	for (int i = 0; i < ntex; i++)
		td[i] = file.get();

	ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > > p(id, ntex);
	for (int i = 0; i < ntex; i++)
		(*p)[i] = Read1(none, file, td[i]);

	delete[] td;
	return p;
}

ISO_ptr<bitmap> TPLFileHandler::Read1(tag id, istream_ref file, const TextureDescriptor &td) {
	file.seek(td.im);
	ImageDescriptor	idesc(file.get());
	file.seek(idesc.offset);

	GXTexFmt	fmt		= GXTexFmt(uint32(idesc.format));
	uint32		w		= idesc.width;
	uint32		h		= idesc.height;

	ISO_ptr<bitmap>	bm(id);

	{
		uint32			size	= TPL::GetSize(fmt, w, h);
		malloc_block	buffer(size);
		if (int nmips = idesc.maxlod - idesc.minlod) {
			bm->Create(w * 2, h);
			bm->SetMips(nmips);
			for (int i = 0; i <= nmips; i++) {
				block<ISO_rgba, 2>	mip		= bm->Mip(i);
				file.readbuff(buffer, TPL::GetSize(fmt, mip.size<1>(), mip.size<2>()));
				TPL::Read(mip, buffer, 0, fmt);
			}
		} else {
			bm->Create(w, h);
			file.readbuff(buffer, size);
			TPL::Read(*bm, buffer, 0, fmt);
		}
	}

	if (td.pal) {
		file.seek(td.pal);
		PaletteDescriptor	pd(file.get());

		bm->CreateClut(pd.numentry);

		file.seek(pd.offset);
		TPL::ReadClut(bm->Clut(), pd.numentry, file, GXTlutFmt(uint32(pd.format)));
	}
	return bm;
}

bool TPLFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
#if 1
	return false;
#else
	uint32		ntex	= bm.NumFrames(), npal, i;
	uint32		start	= file.Tell();

	// header

	file.PutBEL(TplVersion);
	file.PutBEL(ntex);
	file.PutBEL(12);

	TextureDescriptor	*tds = new TextureDescriptor[ntex];
	ImageDescriptor		*ids = new ImageDescriptor[ntex];
	PaletteDescriptor	*pds = new PaletteDescriptor[ntex];

	LuxBitmapFrame *pbm;
	uint32				offset	= start + sizeof(Header) + ntex * sizeof(TextureDescriptor);

	// palette descriptor offsets

	for (i = 0, pbm	= &bm; i < ntex; i++, pbm = pbm->Next()) {
		if (bm.Paletted()) {
			tds[i].pal	= offset - start;
			offset		+= sizeof(PaletteDescriptor);
		} else
			tds[i].pal	= 0;
	}

	// set palette descriptors

	for (npal = 0, pbm	= &bm; pbm; pbm = pbm->Next()) {
		if (pbm->Paletted()) {
			if (pbm->HasAlpha() && !(pbm->Flags() & BMF_CLUTALPHA))
				MedianSplit(pbm, 256, false);
			PaletteDescriptor	&pd = pds[npal++];
			pd.numentry	= pbm->ClutSize();
			pd.pad		= 0;
			pd.format	= pbm->Flags() & BMF_CLUTALPHA ? GX_TL_RGB5A3 : GX_TL_RGB565;
			pd.offset	= offset - start;

			offset		= (offset + 31) & ~31;
			offset		+= (pd.numentry + 15) / 16 * 32;
		}
	}

	// write texture descriptors

	for (i = 0; i < ntex; i++) {
		tds[i].im	= offset - start;
		offset		+= sizeof(ImageDescriptor);
		tds[i].Write(file);
	}

	// write palette descriptors

	for (i = 0; i < npal; i++)
		pds[i].Write(file);

	// write palette data

	for (pbm = &bm; pbm; pbm = pbm->Next()) {
		if (bm.Paletted()) {
			file.Align(32, 0);
			TPL::WriteClut(pbm->Clut(), pbm->ClutSize(), file, pbm->Flags() & BMF_CLUTALPHA ? GX_TL_RGB5A3 : GX_TL_RGB565);
		}
	}

	// create and write image descriptors

	offset = (offset + 31) & ~31;
	for (i = 0, pbm	= &bm; i < ntex; i++, pbm = pbm->Next()) {
		ImageDescriptor	&id = ids[i];

		bool		compress	= !!(flags & BMWF_COMPRESS);
		bool		paletted	= bm.Paletted();
		bool		mipmap		= (flags & (BMWF_MIPMAP | BMWF_FADEMIP)) && !paletted && IsPowerOf2(bm.Width()) && IsPowerOf2(bm.Height());
		bool		alpha		= bm.HasAlpha();
		bool		alpha4		= alpha && BMWF_GETALPHABITS(flags) == 4;
		bool		intensity	= bm.IsIntensity();
		bool		intensity4	= intensity && BMWF_GETINTENSITYBITS(flags) == 4;
		bool		sixteen		= BMWF_GETBITS(flags) == 16;
		GXTexFmt	format		=
			intensity ? (
				  alpha4		? GX_TF_IA4
				: alpha			? GX_TF_IA8
				: intensity4	? GX_TF_I4
				:				  GX_TF_I8
			) : paletted ? GXTexFmt(
				pbm->ClutSize() > 16 ?	GX_TF_C8
				:						GX_TF_C4
			) : compress ? (
				GX_TF_CMPR
			) : sixteen ? (
				alpha ?	GX_TF_RGB5A3
				:		GX_TF_RGB565
			) :  GX_TF_RGBA8;

		id.width	= pbm->Width();
		id.height	= pbm->Height();
		id.format	= format;
		id.offset	= offset - start;

		id.wraps	= 0;
		id.wrapt	= 0;

		if (mipmap) {
			int	levels		= Log2(min(pbm->Width(), pbm->Height()));
			bm.GenerateMipmaps(levels);
			if (flags & BMWF_FADEMIP) {
				for (int i = BMWF_GETMIPMAPLEVELS(flags); i <= levels; i++) {
					LuxBitmapImage	*image	= bm.GetMip(i);
					ISO_rgba			*p		= image->ScanLine(0);
					if (image->Paletted()) {
						for (int n = image->Width() * image->Height(); n--; p++) {
							*p		= image->Lookup(*p);
							p->a	= 0;
						}
						image->SetClut(true, true);
					} else {
						for (int n = image->Width() * image->Height(); n--; p++)
							p->a = 0;
					}
				}
			}
		}

		int	mips = pbm->NumMipLevels();
		switch (format) {
			default:
				if (mips)
					id.minfilter = GX_LIN_MIP_LIN;
					id.magfilter = GX_LINEAR;
					break;
			case GX_TF_C8:
			case GX_TF_C4:
			case GX_TF_C14X2:
				id.minfilter =
				id.magfilter = GX_LINEAR;
				break;
		}

		id.lodbias	= 0;
		id.lodedge	= 0;
		id.minlod	= 0;
		id.maxlod	= mips;
		id.pad		= 0;

		id.Write(file);

		int		tilew, tileh;
		TPL::GetFormat(format, tilew, tileh);

		int	tiles = 0;
		for (int i = 0; i <= mips; i++)
			tiles += (((id.width >> i) + tilew - 1) / tilew) * (((id.height >> i) + tileh - 1) / tileh);
//			int	tiles = ((id.width + tilew - 1) / tilew) * ((id.height + tileh - 1) / tileh);
		if (id.format == GX_TF_RGBA8)
			tiles = tiles * 2;

		offset += tiles * 32;
	}

	// write image data

	file.Align(32, 0);
	for (i = 0, pbm	= &bm; i < ntex; i++, pbm = pbm->Next())
		TPL::WriteImage(pbm, file, GXTexFmt(ids[i].format));

	delete[] tds;
	delete[] ids;
	delete[] pds;

	return true;
#endif
}
