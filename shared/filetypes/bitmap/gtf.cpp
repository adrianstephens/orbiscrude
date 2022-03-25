#include "bitmapfile.h"
#include "gtf.h"
#include "hdr.h"
#include "codec/texels/dxt.h"
#include "base/bits.h"
#include "base/algorithm.h"

#include <windows.h>
#include "d3d9.h"
//#include "xgraphics.h"

//-----------------------------------------------------------------------------
//	Playstation 3 bitmaps
//-----------------------------------------------------------------------------

namespace iso {

BaseTextureFormat GTF::GetBaseFormat(uint8 f) {
	return BaseTextureFormat(f & 0x1f);
}

bool GTF::IsSwizzle(uint8 format) {
	return !(format & CELL_GCM_TEXTURE_LN);
}

uint32 GTF::GetScans(BaseTextureFormat format, uint32 height) {
	return reg_texture_format::isDXT(format) ? (height + 3) / 4 : height;
}

uint32 GTF::GetSize(BaseTextureFormat format, uint32 width, uint32 height) {
	return texture::getPitch(format, width) * GetScans(format, height);
}

bool GTF::IsHDR(BaseTextureFormat format) {
	switch (format) {
		case kBtfDepth24X8:
		case kBtfDepth24FX8:
		case kBtfDepth16:
		case kBtfDepth16F:
		case kBtfR16:
		case kBtfGr16:
		case kBtfAbgr16f:
		case kBtfAbgr32f:
		case kBtfR32f:
		case kBtfGr16f:
			return true;
	}
	return false;
}

bool GTF::IsSwizzlable(const CellGcmTexture_be &tex) {
	BaseTextureFormat raw_format = GetBaseFormat(tex.format);
	return	raw_format != kBtfB8R8G8R8
		&&	raw_format != kBtfR8B8R8G8
		&&	is_pow2(tex.width)
		&&	is_pow2(tex.height)
		&&	(tex.dimension != 3 || is_pow2(tex.depth));
}

#ifdef _XGRAPHICS_H_

D3DFORMAT GTFToX360(BaseTextureFormat format, remap_chans remap) {
	D3DFORMAT	xf;
	switch (format) {
		case kBtfB8:				xf = D3DFMT_A8;						break;
		case kBtfArgb1555:			xf = D3DFMT_A1R5G5B5;				break;
		case kBtfArgb4444:			xf = D3DFMT_A4R4G4B4;				break;
		case kBtfRgb565:			xf = D3DFMT_R5G6B5;					break;
		case kBtfArgb8888:			xf = D3DFMT_A8R8G8B8;				break;
		case kBtfDxt1:				xf = D3DFMT_DXT1;					break;
		case kBtfDxt3:				xf = D3DFMT_DXT2;					break;
		case kBtfDxt5:				xf = D3DFMT_DXT4;					break;
		case kBtfGb88:				xf = D3DFMT_G8R8;					break;
//		case kBtfB8R8G8R8:			xf = GPUTEXTUREFORMAT_5_6_5;		break;
//		case kBtfR8B8R8G8:			xf = GPUTEXTUREFORMAT_5_6_5;		break;
		case kBtfRgb655:			xf = D3DFMT_R6G5B5;					break;
//		case kBtfDepth24X8:			xf = GPUEDRAMDEPTHFORMAT_24_8;		break;
//		case kBtfDepth24FX8:		xf = GPUEDRAMDEPTHFORMAT_24_8_FLOAT;break;
//		case kBtfDepth16:			xf = GPUTEXTUREFORMAT_5_6_5;		break;
//		case kBtfDepth16F:			xf = GPUTEXTUREFORMAT_5_6_5;		break;
		case kBtfR16:				xf = D3DFMT_L16;					break;
		case kBtfGr16:				xf = D3DFMT_G16R16;					break;
//		case kBtfRgba5551:			xf = GPUTEXTUREFORMAT_5_6_5;		break;
		case kBtfHiLo8:				xf = D3DFMT_G8R8;					break;
		case kBtfHiLoS8:			xf = D3DFMT_G8R8;					break;
		case kBtfAbgr16f:			xf = D3DFMT_A16B16G16R16F;			break;
		case kBtfAbgr32f:			xf = D3DFMT_A32B32G32R32F;			break;
		case kBtfR32f:				xf = D3DFMT_R32F;					break;
//		case kBtfXrgb1555:			xf = GPUTEXTUREFORMAT_5_6_5;		break;
//		case kBtfXrgb8888:			xf = GPUTEXTUREFORMAT_5_6_5;		break;
		case kBtfGr16f:				xf = D3DFMT_G16R16F;				break;
		default:					xf = D3DFMT_UNKNOWN;				break;
	}

	uint32	xf2 = xf & ~D3DFORMAT_SWIZZLE_MASK;
	for (int i = 0; i < 4; i++) {
		static uint8	d3d_shift[] = {
			D3DFORMAT_SWIZZLEW_SHIFT,
			D3DFORMAT_SWIZZLEX_SHIFT,
			D3DFORMAT_SWIZZLEY_SHIFT,
			D3DFORMAT_SWIZZLEZ_SHIFT
		};
		static uint8	d3d_comp[] = {
//			1,2,3,0
			3,2,1,0
		};
		xf2 |= remap.get_chan(i) << d3d_shift[i];
	}

	return D3DFORMAT(xf2);
}
#endif

void GTF::Swizzle(uint32 width, uint32 height, uint32 depth, void *srce, void *dest, uint32 bpp) {
	if (bpp > 4) {
		if (bpp == 8)
			width *= 2;
		else
			width *= 4;
		bpp = 4;
	}

	uint32	log2width	= log2(width);
	uint32	log2height	= log2(height);
	uint32	log2depth	= log2(depth);

	uint32 maskx = 0, masky = 0, maskz = 0, t = 0;
	while (log2width | log2height | log2depth) {
		if (log2width){
			maskx |= 1 << t++;
			--log2width;
		}
		if (log2height){
			masky |= 1 << t++;
			--log2height;
		}
		if (log2depth){
			maskz |= 1 << t++;
			--log2depth;
		}
	}

	uint32	i = 0;
	for (uint32 z = 0, zc = depth; zc--; z = ((z | ~maskz) + 1) & maskz) {
		for (uint32 y = z, yc = height; yc--; y = (((y | ~masky) + 1) & masky) | z) {
			switch (bpp) {
				case 1:
					for (uint32 x = y, xc = width; xc--; x = (((x | ~maskx) + 1) & maskx) | y)
						((uint8*)dest)[x] = ((uint8*)srce)[i++];
					break;
				case 2:
					for (uint32 x = y, xc = width; xc--; x = (((x | ~maskx) + 1) & maskx) | y)
						((uint16*)dest)[x] = ((uint16*)srce)[i++];
					break;
				case 4:
					for (uint32 x = y, xc = width; xc--; x = (((x | ~maskx) + 1) & maskx) | y)
						((uint32*)dest)[x] = ((uint32*)srce)[i++];
					break;
			}
		}
	}
}

void GTF::DeSwizzle(uint32 width, uint32 height, uint32 depth, void *srce, void *dest, uint32 bpp) {
	if (bpp > 4) {
		if (bpp == 8)
			width *= 2;
		else
			width *= 4;
		bpp = 4;
	}

	uint32	log2width	= log2(width);
	uint32	log2height	= log2(height);
	uint32	log2depth	= log2(depth);

	uint32 maskx = 0, masky = 0, maskz = 0, t = 0;
	while (log2width | log2height | log2depth) {
		if (log2width){
			maskx |= 1 << t++;
			--log2width;
		}
		if (log2height){
			masky |= 1 << t++;
			--log2height;
		}
		if (log2depth){
			maskz |= 1 << t++;
			--log2depth;
		}
	}

	uint32	i = 0;
	for (uint32 z = 0, zc = depth; zc--; z = ((z | ~maskz) + 1) & maskz) {
		for (uint32 y = z, yc = height; yc--; y = (((y | ~masky) + 1) & masky) | z) {
			switch (bpp) {
				case 1:
					for (uint32 x = y, xc = width; xc--; x = (((x | ~maskx) + 1) & maskx) | y)
						((uint8*)dest)[i++] = ((uint8*)srce)[x];
					break;
				case 2:
					for (uint32 x = y, xc = width; xc--; x = (((x | ~maskx) + 1) & maskx) | y)
						((uint16*)dest)[i++] = ((uint16*)srce)[x];
					break;
				case 4:
					for (uint32 x = y, xc = width; xc--; x = (((x | ~maskx) + 1) & maskx) | y)
						((uint32*)dest)[i++] = ((uint32*)srce)[x];
					break;
			}
		}
	}
}

template<typename D> void Put(const block<ISO_rgba, 2> &srce, void *dest, uint32 dest_pitch) {
	copy(srce, make_strided_block((D*)dest, srce.template size<1>(), dest_pitch, srce.template size<2>()));
}

template<typename D> static void PutBC(const block<ISO_rgba, 2> &srce, void *dest, uint32 dest_pitch) {
	copy(srce, make_strided_block((D*)dest, max(srce.template size<1>() / 4, 1), dest_pitch, max(srce.template size<2>() / 4, 1)));
}

template<typename S, typename D> void Get(const block<D, 2> &dest, void *srce, uint32 srce_pitch) {
	copy(make_strided_block((S*)srce, dest.template size<1>(), srce_pitch, dest.template size<2>()), dest);
}

template<typename S, typename D> static void GetBC(const block<D, 2> &dest, void *srce, uint32 srce_pitch) {
	copy(make_strided_block((const S*)srce, max(dest.template size<1>() / 4, 1), srce_pitch, max(dest.template size<2>() / 4, 1)), dest);
}

void GTF::Convert(uint32 width, uint32 height, uint32 depth,
	void *srce, uint32 srce_pitch, uint8 srce_format, remap_chans srce_remap,
	void *dest, uint32 dest_pitch, uint8 dest_format, remap_chans dest_remap,
	bool nodither
) {
	uint32		height2		= height * depth;
	bool		dxt			= reg_texture_format::isDXT(GetBaseFormat(dest_format));
	bool		swiz		= !(dest_format & CELL_GCM_TEXTURE_LN) && !dxt;
	void		*temp		= dest;
	void		*temp2		= 0;

	if (reg_texture_format::isDXT(GetBaseFormat(srce_format))) {
		temp2	= malloc(srce_pitch * ((height2 + 3) / 4));
		copy_n((uint16be*)srce, (uint16*)temp2, srce_pitch / 2 * ((height2 + 3) / 4));
		srce	= temp2;
	} else if (!(srce_format & CELL_GCM_TEXTURE_LN)) {
		uint16	bpp	= reg_texture_format::getSize(GetBaseFormat(srce_format));
		temp2	= malloc(bpp * width * height * depth);
		DeSwizzle(width, height, depth, srce, temp2, bpp);
		srce	= temp2;
	}

	uint32		tilesize		= dxt ? 128 : 32;
	bool		oddsize			= width % tilesize || height2 % tilesize;

	if (oddsize)
		temp = calloc(max(texture::getPitch(GetBaseFormat(dest_format), align(width, tilesize)), dest_pitch) * GetScans(GetBaseFormat(dest_format), align(height2, tilesize)), 1);
	else if (swiz)
		temp = calloc(dest_pitch * height2, 1);
#if 0
	D3DFORMAT	srce_d3d	= GTFToX360(srce_format, srce_remap);
	D3DFORMAT	dest_d3d	= GTFToX360(dest_format, dest_remap);
	XGCopySurface(
		temp, dest_pitch, width, height2, dest_d3d, NULL,
		srce, srce_pitch, srce_d3d, NULL,
		nodither ? XGCOMPRESS_NO_DITHERING  : 0, 0.5f);
#else
	block<ISO_rgba, 2>	sb = make_strided_block((ISO_rgba*)srce, width, srce_pitch, height);

	switch (GetBaseFormat(dest_format)) {
		case kBtfArgb8888: {
			block<ISO_rgba, 2>	db = make_strided_block((ISO_rgba*)temp, width, dest_pitch, height);
			switch (GetBaseFormat(srce_format)) {
				case kBtfB8:			Get<Texel<TexelFormat< 8,0,8,0,0,0,0> >			>(db, srce, srce_pitch);	break;
				case kBtfArgb1555:		Get<Texel<TexelFormat<16,0,5,5,5,10,5,15,1> >	>(db, srce, srce_pitch);	break;
				case kBtfArgb4444:		Get<Texel<TexelFormat<16,0,4,4,4,8,4,12,4> >	>(db, srce, srce_pitch);	break;
				case kBtfRgb565:		Get<Texel<TexelFormat<16,0,5,5,6,11,5> >		>(db, srce, srce_pitch);	break;
				case kBtfArgb8888:		Get<Texel<R8G8B8A8>								>(db, srce, srce_pitch);	break;
				case kBtfDxt1:			GetBC<BE(DXT1rec)	>(db, srce, srce_pitch);								break;
				case kBtfDxt3:			GetBC<BE(DXT23rec)	>(db, srce, srce_pitch);								break;
				case kBtfDxt5:			GetBC<BE(DXT45rec)	>(db, srce, srce_pitch);								break;
				case kBtfGb88:			Get<Texel<TexelFormat<16,0,8,8,8,0,0> >			>(db, srce, srce_pitch);	break;
				case kBtfRgb655:		Get<Texel<TexelFormat<16,0,5,5,5,10,6> >		>(db, srce, srce_pitch);	break;
				default:
					break;
			}
			break;
		}

		case kBtfAbgr32f: {
			block<HDRpixel, 2>	db = make_strided_block((HDRpixel*)temp, width, dest_pitch, height);
			switch (GetBaseFormat(srce_format)) {
				case kBtfR16:			Get<uint16					>(db, srce, srce_pitch);	break;
				case kBtfGr16:			Get<array_vec<uint16,2>	>(db, srce, srce_pitch);	break;
				case kBtfAbgr16f:		Get<array_vec<float16,4>	>(db, srce, srce_pitch);	break;
				case kBtfAbgr32f:		Get<array_vec<float,4>		>(db, srce, srce_pitch);	break;
				case kBtfR32f:			Get<float					>(db, srce, srce_pitch);	break;
				case kBtfGr16f:			Get<array_vec<float16,2>	>(db, srce, srce_pitch);	break;
				default:
					break;
			}
			break;
		}

		case kBtfB8:			Put<Texel<TexelFormat< 8,0,8,0,0,0,0> >			>(sb, temp, dest_pitch);	break;
		case kBtfArgb1555:		Put<Texel<TexelFormat<16,0,5,5,5,10,5,15,1> >	>(sb, temp, dest_pitch);	break;
		case kBtfArgb4444:		Put<Texel<TexelFormat<16,0,4,4,4,8,4,12,4> >	>(sb, temp, dest_pitch);	break;
		case kBtfRgb565:		Put<Texel<TexelFormat<16,0,5,5,6,11,5> >		>(sb, temp, dest_pitch);	break;
		case kBtfDxt1:			PutBC<BE(DXT1rec)	>(sb, temp, dest_pitch);								break;
		case kBtfDxt3:			PutBC<BE(DXT23rec)	>(sb, temp, dest_pitch);								break;
		case kBtfDxt5:			PutBC<BE(DXT45rec)	>(sb, temp, dest_pitch);								break;
		case kBtfGb88:			Put<Texel<TexelFormat<16,0,8,8,8,0,0> >			>(sb, temp, dest_pitch);	break;
		case kBtfRgb655:		Put<Texel<TexelFormat<16,0,5,5,5,10,6> >		>(sb, temp, dest_pitch);	break;
		case kBtfR16:
		case kBtfGr16:
		case kBtfHiLo8:
		case kBtfHiLoS8:
		case kBtfAbgr16f:
		case kBtfR32f:
		case kBtfGr16f:
		default:
			break;
	}
#endif
	if (oddsize && !swiz) {
		memcpy(dest, temp, dest_pitch * (dxt ? (height2 + 3) / 4 : height));
		free(temp);
		temp = dest;
	}

	if (swiz) {
		uint16	bpp	= reg_texture_format::getSize(GetBaseFormat(dest_format));
		Swizzle(width, height, depth, temp, dest, bpp);
	} else if (dxt) {
		uint16	*p = (uint16*)dest;
		if (dest_format == kBtfDxt5)
			FixDXT5Alpha((uint8*)srce + srce_remap.swiz_w, srce_pitch, dest, 16, dest_pitch, width, height);
		for (uint32 n = dest_pitch / 2 * ((height + 3) / 4); n--; p++)
			*p = (uint16be&)*p;
	}
	if (temp != dest)
		free(temp);
	if (temp2)
		free(temp2);
}

} //namespace iso

using namespace iso;

class GTFFileHandler : public BitmapFileHandler, GTF {
	ISO_ptr<void>			Read(tag id, istream_ref file, CellGtfTextureAttribute_be &attr);
	uint32					Write(ISO_ptr<void> p, ostream_ref file, size_t offset);

	const char*		GetExt() override { return "gtf"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} gtf;

ISO_ptr<void> GTFFileHandler::Read(tag id, istream_ref file) {
	CellGtfFileHeader_be	header = file.get();

	if (header.numTexture == 1) {
		CellGtfTextureAttribute_be	attr = file.get();
		return Read(id, file, attr);
	}

	char				name[16];
	ISO_ptr<anything>	a(id);
	for (int i = 0, n = header.numTexture; i < n; i++) {
		file.seek(sizeof(CellGtfFileHeader_be) + i * sizeof(CellGtfTextureAttribute_be));
		CellGtfTextureAttribute_be	attr = file.get();
		sprintf(name, "%i", native_endian(attr.id));
		a->Append(Read(id, file, attr));
	}
	return a;
}

ISO_ptr<void> GTFFileHandler::Read(tag id, istream_ref file, CellGtfTextureAttribute_be &attr) {
	malloc_block	buffer(attr.textureSize);
	file.seek(attr.offsetToTex);
	file.readbuff(buffer, attr.textureSize);

	uint32				width	= attr.tex.width;
	uint32				height	= attr.tex.height;
	uint32				depth	= attr.tex.depth;
	uint32				pitch	= attr.tex.pitch;
	uint32				remap	= attr.tex.remap;
	BaseTextureFormat	fmt		= GetBaseFormat(attr.tex.format);
	uint16				bpp		= reg_texture_format::getSize(fmt);

	if (pitch == 0)
		pitch = texture::getPitch(fmt, width);

	ISO_ptr<bitmap>	bm(id);
	bm->Create(width, height * depth, depth > 1 ? BMF_VOLUME : 0, depth);

	if (!reg_texture_format::isDXT(fmt)) {
		switch (bpp) {
			case 1:	break;
			case 2:
				for (int i = 0, n = attr.textureSize / 2; i < n; i++)
					((uint16*)buffer)[i] = ((uint16be*)buffer)[i];
				break;
			default:
				for (int i = 0, n = attr.textureSize / 4; i < n; i++)
					((uint32*)buffer)[i] = ((uint32be*)buffer)[i];
				break;
		}
		if (!(attr.tex.format & CELL_GCM_TEXTURE_LN)) {
			malloc_block	buffer2(attr.textureSize);
			uint32	m		= square(min(width, height)) - 1;
			uint32	m2		= width * height - m - 1;
			uint32	sx		= log2(width);
			uint32	sy		= width > height ? log2(height) : 0;
			switch (bpp) {
				case 1:
					for (uint32 i = 0, n = width * height; i < n; i++) {
						uint32	j = ((part_by_1(uint16(i)) | (part_by_1(uint16(i >> sx)) << 1)) & m) | ((i << sy) & m2);
						((uint8*)buffer2)[i] = ((uint8*)buffer)[j];
					}
					break;
				case 2:
					for (uint32 i = 0, n = width * height; i < n; i++) {
						uint32	j = ((part_by_1(uint16(i)) | (part_by_1(uint16(i >> sx)) << 1)) & m) | ((i << sy) & m2);
						((uint16*)buffer2)[i] = ((uint16*)buffer)[j];
					}
					break;
				case 4:
					for (uint32 i = 0, n = width * height; i < n; i++) {
						uint32	j = ((part_by_1(uint16(i)) | (part_by_1(uint16(i >> sx)) << 1)) & m) | ((i << sy) & m2);
						((uint32*)buffer2)[i] = ((uint32*)buffer)[j];
					}
					break;
				case 8:
					for (uint32 i = 0, n = width * height; i < n; i++) {
						uint32	j = ((part_by_1(uint16(i)) | (part_by_1(uint16(i >> sx)) << 1)) & m) | ((i << sy) & m2);
						((uint64*)buffer2)[i] = ((uint64*)buffer)[j];
					}
					break;
				case 16:
					for (uint32 i = 0, n = width * height; i < n; i++) {
						uint32	j = ((part_by_1(uint16(i)) | (part_by_1(uint16(i >> sx)) << 1)) & m) | ((i << sy) & m2);
						((uint64*)buffer2)[i * 2 + 0] = ((uint64*)buffer)[j * 2 + 0];
						((uint64*)buffer2)[i * 2 + 1] = ((uint64*)buffer)[j * 2 + 1];
					}
					break;
			}

			swap(buffer, buffer2);
		}
	}

	switch ((int)fmt) {
		case CELL_GCM_TEXTURE_COMPRESSED_DXT1:
			for (int y = 0; y < height; y += 4) {
				DXT1rec	*dxt = (DXT1rec*)((uint8*)buffer + (y / 4) * pitch);
				for (int x = 0; x < width; x += 4)
					(dxt++)->Decode(bm->Block(x, y, 4, 4));
			}
			break;
		case CELL_GCM_TEXTURE_COMPRESSED_DXT23:
			for (int y = 0; y < height; y += 4) {
				DXT23rec	*dxt = (DXT23rec*)((uint8*)buffer + (y / 4) * pitch);
				for (int x = 0; x < width; x += 4)
					(dxt++)->Decode(bm->Block(x, y, 4, 4));
			}
			break;
		case CELL_GCM_TEXTURE_COMPRESSED_DXT45:
			for (int y = 0; y < height; y += 4) {
				DXT45rec	*dxt = (DXT45rec*)((uint8*)buffer + (y / 4) * pitch);
				for (int x = 0; x < width; x += 4)
					(dxt++)->Decode(bm->Block(x, y, 4, 4));
			}
			break;
		default:
			for (int y = 0; y < height; y++) {
				ISO_rgba	*dest	= bm->ScanLine(y);
				void		*srce	= (char*)buffer + pitch * y;
				switch (fmt) {
					case kBtfXrgb1555:
					case kBtfArgb1555:	copy_n((Texel<TexelFormat<16, 10,5, 5,5, 0,5, 15,1>	>*)srce, dest, width); break;
					case kBtfArgb4444:	copy_n((Texel<TexelFormat<16,  8,4, 4,4, 0,4, 12,4>	>*)srce, dest, width); break;
					case kBtfRgb565:	copy_n((Texel<TexelFormat<16, 11,5, 5,6, 0,5>		>*)srce, dest, width); break;
					case kBtfGb88:		copy_n((Texel<TexelFormat<16,  0,0, 8,8, 0,8>		>*)srce, dest, width); break;
					case kBtfRgb655:	copy_n((Texel<TexelFormat<16, 10,6, 5,5, 0,5>		>*)srce, dest, width); break;
					case kBtfRgba5551:	copy_n((Texel<TexelFormat<16, 10,5, 5,5, 0,5, 15,1>	>*)srce, dest, width); break;
//					case kBtfHiLo8:		copy_n((Texel<TexelFormat<16, 16,8, 8,8, 0,8, 24,8>	>*)srce, dest, width); break;
//					case kBtfHiLoS8:	copy_n((Texel<TexelFormat<16, 16,8, 8,8, 0,8, 24,8>	>*)srce, dest, width); break;
					case kBtfXrgb8888:
					case kBtfArgb8888:	copy_n((Texel<TexelFormat<32, 16,8, 8,8, 0,8, 24,8>	>*)srce, dest, width); break;
					case kBtfB8:
						for (int x = 0; x < width; x++)
							dest[x] = ((uint8*)srce)[x];
						break;
					case kBtfB8R8G8R8:
						for (int x = 0; x < width; x += 2) {
							dest[x + 0].b	= dest[x + 1].b = ((uint8*)srce)[x * 2 + 0];
							dest[x + 0].g	= dest[x + 1].g = ((uint8*)srce)[x * 2 + 2];
							dest[x + 0].r	= ((uint8*)srce)[x * 2 + 3];
							dest[x + 1].r	= ((uint8*)srce)[x * 2 + 1];
						}
						break;
					case kBtfR8B8R8G8:
						for (int x = 0; x < width; x += 2) {
							dest[x + 0].b	= dest[x + 1].b = ((uint8*)srce)[x * 2 + 1];
							dest[x + 0].g	= dest[x + 1].g = ((uint8*)srce)[x * 2 + 3];
							dest[x + 0].r	= ((uint8*)srce)[x * 2 + 2];
							dest[x + 1].r	= ((uint8*)srce)[x * 2 + 0];
						}
						break;
				}
			}
	}
#if 0
	uint8	map[4], cur[4], loc[4], swaps[4];
	for (int i = 0; i < 4; i++) {
		swaps[i] = cur[i] = loc[i] = i;
		switch ((remap >> (8 + i * 2)) & 3) {
			case CELL_GCM_TEXTURE_REMAP_ZERO:	map[i] = 4; break;
			case CELL_GCM_TEXTURE_REMAP_ONE:	map[i] = 5; break;
			case CELL_GCM_TEXTURE_REMAP_REMAP:	map[i] = (remap >> (i * 2)) & 3; break;
		}
	}
	for (int i = 0; i < 4; i++) {
		if (map[i] != cur[i]) {
			swaps[i] = loc[i];
		}
	}
#endif
	return bm;
}

bool GTFFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	CellGtfFileHeader_be	header;
	uint32	total		= 0;
	header.version		= 0x02000000;

	if (p.IsType<anything>()) {
		anything	*a		= p;
		int			n		= a->Count();
		header.numTexture	= n;
		size_t		offset	= align(sizeof(CellGtfFileHeader_be) + n * sizeof(CellGtfTextureAttribute_be), 128);
		for (int i = 0; i < n; i++) {
			file.seek(sizeof(CellGtfFileHeader_be) + i * sizeof(CellGtfTextureAttribute_be));
			total += Write((*a)[i], file, total + offset);
		}
	} else {
		header.numTexture	= 1;
		file.seek(sizeof(CellGtfFileHeader_be));
		total = Write(p, file, align(sizeof(CellGtfFileHeader_be) + sizeof(CellGtfTextureAttribute_be), 128));
	}

	header.size	= total;

	file.seek(0);
	file.write(header);
	return true;
}

uint32 GTFFileHandler::Write(ISO_ptr<void> p, ostream_ref file, size_t offset) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
	if (!bm)
		return 0;

	uint32		width	= bm->Width();
	uint32		height	= bm->Height();
	uint32		depth	= bm->Depth();
	uint32		pitch	= width * 4;
	uint32		size	= pitch * height;

	CellGtfTextureAttribute_be	attr;
	attr.id				= p.ID() ? from_string<uint32>(p.ID().get_tag()) : 0;
	attr.offsetToTex	= uint32(offset);
	attr.textureSize	= size;

	attr.tex.format		= CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN;
	attr.tex.mipmap		= 0;
	attr.tex.dimension	= 2;
	attr.tex.cubemap	= bm->IsCube();
	attr.tex.remap		= GTF_REMAP(0,1,2,3);
	attr.tex.width		= width;
	attr.tex.height		= height;
	attr.tex.depth		= depth;
	attr.tex.location	= 0;
	attr.tex.padding	= 0;
	attr.tex.pitch		= width * 4;
	attr.tex.offset		= 0;

	malloc_block	buffer(size);
	for (int y = 0; y < height; y++) {
		ISO_rgba	*srce	= bm->ScanLine(y);
		void		*dest	= (char*)buffer + pitch * y;
		copy_n(srce, (Texel<TexelFormat<32, 16,8, 8,8, 0,8, 24,8> >*)dest, width);
	}

	file.write(attr);
	file.seek(offset);
	file.writebuff(buffer, size);

	return size;
}
