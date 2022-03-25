#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

#include <windows.h>
#include "d3d9.h"

#if 1
#define __XBOXMATH2_H__
typedef __m128 XMVECTOR;
#endif
#include "xgraphics.h"

using namespace iso;

class MCTFileHandler : public FileHandler {
	const char*		GetExt() override { return "mct";	}
	const char*		GetDescription() override { return "XBOX 360 MCT texture compression"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	MCTFileHandler()		{ ISO::getdef<bitmap>(); }
} mct;

#define D3DFMT_R8G8B8A8 (D3DFORMAT)MAKED3DFMT(GPUTEXTUREFORMAT_8_8_8_8, GPUENDIAN_8IN32, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_RGBA)
#define D3DFMT_LIN_R8G8B8A8 (D3DFORMAT)MAKELINFMT(D3DFMT_R8G8B8A8)

ISO_ptr<void> MCTFileHandler::Read(tag id, istream_ref file) {
	unsigned		width = 256, height = 256;
	D3DFORMAT		format = D3DFMT_DXT1;

	ISO_ptr<bitmap>	bm(id);
	bm->Create(width, height);

	return bm;
}

bool MCTFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (bitmap *bm = ISO_conversion::convert<bitmap>(p)) {
		HRESULT			hr;
		int				width			= bm->Width();
		int				height			= bm->Height();
		int				depth			= bm->Depth();
		int				levels			= 1;
		D3DRESOURCETYPE	type			= depth == 1 ? D3DRTYPE_TEXTURE : bm->IsCube() ? D3DRTYPE_CUBETEXTURE : D3DRTYPE_ARRAYTEXTURE;
		D3DFORMAT		format			= D3DFMT_DXT1;

		UINT			base_size = 0, mip_size = 0;
		D3DBaseTexture	d3dtex;

		switch (type) {
			case D3DRTYPE_TEXTURE:
				XGSetTextureHeaderEx(width, height, levels, 0, format, 0, 0, 0, 0, 0, (D3DTexture*)&d3dtex, &base_size, &mip_size);
				break;
			case D3DRTYPE_CUBETEXTURE:
			    XGSetCubeTextureHeaderEx( width, levels, 0, format, 0, 0, 0, 0, (D3DCubeTexture*)&d3dtex, &base_size, &mip_size);
				break;
			case D3DRTYPE_ARRAYTEXTURE:
			    XGSetArrayTextureHeaderEx(width, height, depth, levels, 0, format, 0, 0, 0, 0, 0, (D3DArrayTexture*)&d3dtex, &base_size, &mip_size);
				break;
			case D3DRTYPE_VOLUMETEXTURE:
				XGSetVolumeTextureHeaderEx(width, height, depth, levels, 0, format, 0, 0, 0, 0, (D3DVolumeTexture*)&d3dtex, &base_size, &mip_size);
				break;
		}

		XGTEXTURE_DESC	base_desc;
		XGGetTextureDesc(&d3dtex, 0, &base_desc);

		DWORD	copyflags		= MAKELINFMT(base_desc.Format)==D3DFMT_LIN_UYVY || MAKELINFMT(base_desc.Format)==D3DFMT_LIN_YUY2 ? XGCOMPRESS_YUV_DESTINATION : 0;
		bool	tile			= !!XGIsTiledFormat(format);
		malloc_block	temp_buffer(tile ? base_size : 0);
		malloc_block	buffer(base_size + mip_size);
		memset(buffer, 0, base_size + mip_size);

		for (int mip = 0; mip < levels; mip++) {
			XGTEXTURE_DESC	mip_desc;
			XGGetTextureDesc(&d3dtex, mip, &mip_desc);

			for (int d = 0; d < depth; d++) {
				DWORD	offset = XGGetMipLevelOffset(&d3dtex, d, mip);
				if (mip > 0 && mip_size > 0)
					offset += base_size;

//				pSrcImages[dwArrayIndex]->Resize(mip_desc.Width, mip_desc.Height, IMG_FILTER_BOX, 0);
				ISO_rgba	*srce = bm->ScanLine(0);
				if (tile) {
					XGCopySurface(temp_buffer,
						mip_desc.RowPitch, mip_desc.Width, mip_desc.Height, format, NULL,
						srce, width * 4, D3DFMT_LIN_R8G8B8A8, NULL,
						copyflags, 0.5f);
					XGTileTextureLevel(width, height, mip,
						XGGetGpuFormat(format), 0,
						buffer + offset, NULL, temp_buffer, mip_desc.RowPitch, NULL);
				} else {
					XGCopySurface(buffer + offset,
						mip_desc.RowPitch, mip_desc.Width, mip_desc.Height, format, NULL,
						srce, width * 4, D3DFMT_LIN_R8G8B8A8, NULL,
						copyflags, 0.5f);
				}
			}
		}

//		UINT			context_size	= XGMCTGetCompressionContextSize(type, width, height, depth, 1, format, NULL, 0, NULL);
//		void			*context_buffer	= malloc(context_size);
//		XGMCTCOMPRESSION_CONTEXT context = XGMCTInitializeCompressionContext(context_buffer, context_size);

		base_size = mip_size = 0;
		hr = XGMCTCompressTexture(
			NULL,//context,
			NULL, &base_size,
			NULL, &mip_size,
			D3DFMT_UNKNOWN,
			&d3dtex,
			NULL,
			XGCOMPRESS_MCT_CONTIGUOUS_MIP_LEVELS,
			NULL
		);

		malloc_block	base_buffer(base_size);
		malloc_block	mip_buffer(mip_size);
		hr = XGMCTCompressTexture(
			NULL,//context,
			base_buffer, &base_size,
			mip_buffer,  &mip_size,
			D3DFMT_UNKNOWN,
			&d3dtex,
			NULL,
			XGCOMPRESS_MCT_CONTIGUOUS_MIP_LEVELS,
			NULL
		);

		file.putc('M');
		file.putc('C');
		file.putc('T');
		file.putc(type);
		file.write(width);
		file.write(height);
		file.write(depth);
		file.writebuff(base_buffer, base_size);

//		XGMCTDestroyCompressionContext(context);
//		free(context_buffer);
		return true;
	}
	return false;
}
