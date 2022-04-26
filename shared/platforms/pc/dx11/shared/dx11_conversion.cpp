#include "scenegraph.h"
#include "systems/conversion/platformdata.h"
#include "systems/conversion/channeluse.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "base/algorithm.h"
#include "vector_iso.h"

#include "codec/texels/dxt.h"
#include "dx/dx11_effect.h"
#include "dx/dx_shaders.h"
#include "model_defs.h"
#include "com.h"
#include "extra/identifier.h"
#include "filetypes/fx.h"
#include "hashes/md5.h"

#include <d3d11shader.h>
#include <d3dcompiler.h>

#undef min
#undef max

using namespace iso;

template<DXGI_FORMAT F> struct dxgi_type { typedef void type; };
#define DXGI_TYPE(F,T)	template<> struct dxgi_type<F>	{ typedef NO_PARENTHESES T type; }

DXGI_TYPE(DXGI_FORMAT_R32G32B32A32_FLOAT,			(array_vec<float,4>));
DXGI_TYPE(DXGI_FORMAT_R32G32B32A32_UINT,			(array_vec<uint32,4>));
DXGI_TYPE(DXGI_FORMAT_R32G32B32A32_SINT,			(array_vec<int32,4>));
DXGI_TYPE(DXGI_FORMAT_R32G32B32_FLOAT,				(array_vec<float,3>));
DXGI_TYPE(DXGI_FORMAT_R32G32B32_UINT,				(array_vec<uint32,3>));
DXGI_TYPE(DXGI_FORMAT_R32G32B32_SINT,				(array_vec<int32,3>));
DXGI_TYPE(DXGI_FORMAT_R16G16B16A16_FLOAT,			(array_vec<float16,4>));
DXGI_TYPE(DXGI_FORMAT_R16G16B16A16_UNORM,			(array_vec<unorm16,4>));
DXGI_TYPE(DXGI_FORMAT_R16G16B16A16_UINT,			(array_vec<uint16,4>));
DXGI_TYPE(DXGI_FORMAT_R16G16B16A16_SNORM,			(array_vec<norm16,4>));
DXGI_TYPE(DXGI_FORMAT_R16G16B16A16_SINT,			(array_vec<int16,4>));
DXGI_TYPE(DXGI_FORMAT_R32G32_FLOAT,					(array_vec<float,2>));
DXGI_TYPE(DXGI_FORMAT_R32G32_UINT,					(array_vec<uint32,2>));
DXGI_TYPE(DXGI_FORMAT_R32G32_SINT,					(array_vec<int32,2>));
DXGI_TYPE(DXGI_FORMAT_R10G10B10A2_UNORM,			(unorm4_10_10_10_2));
DXGI_TYPE(DXGI_FORMAT_R10G10B10A2_UINT,				(uint4_10_10_10_2));
DXGI_TYPE(DXGI_FORMAT_R11G11B10_FLOAT,				(float3_11_11_10));
DXGI_TYPE(DXGI_FORMAT_R8G8B8A8_UNORM,				(array_vec<unorm8,4>));
//DXGI_TYPE(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,		(array_vec<unorm8,4>));
DXGI_TYPE(DXGI_FORMAT_R8G8B8A8_UINT,				(array_vec<uint8,4>));
DXGI_TYPE(DXGI_FORMAT_R8G8B8A8_SNORM,				(array_vec<norm8,4>));
DXGI_TYPE(DXGI_FORMAT_R8G8B8A8_SINT,				(array_vec<int8,4>));
DXGI_TYPE(DXGI_FORMAT_R16G16_FLOAT,					(array_vec<float16,2>));
DXGI_TYPE(DXGI_FORMAT_R16G16_UNORM,					(array_vec<unorm16,2>));
DXGI_TYPE(DXGI_FORMAT_R16G16_UINT,					(array_vec<uint16,2>));
DXGI_TYPE(DXGI_FORMAT_R16G16_SNORM,					(array_vec<norm16,2>));
DXGI_TYPE(DXGI_FORMAT_R16G16_SINT,					(array_vec<int16,2>));
//DXGI_TYPE(DXGI_FORMAT_D32_FLOAT,					(float));
DXGI_TYPE(DXGI_FORMAT_R32_FLOAT,					(float));
DXGI_TYPE(DXGI_FORMAT_R32_UINT,						(uint32));
DXGI_TYPE(DXGI_FORMAT_R32_SINT,						(int32));
DXGI_TYPE(DXGI_FORMAT_R8G8_UNORM,					(array_vec<unorm8,2>));
DXGI_TYPE(DXGI_FORMAT_R8G8_UINT,					(array_vec<uint8,2>));
DXGI_TYPE(DXGI_FORMAT_R8G8_SNORM,					(array_vec<norm8,2>));
DXGI_TYPE(DXGI_FORMAT_R8G8_SINT,					(array_vec<int8,2>));
DXGI_TYPE(DXGI_FORMAT_R16_FLOAT,					(float16));
//DXGI_TYPE(DXGI_FORMAT_D16_UNORM,					(unorm16));
DXGI_TYPE(DXGI_FORMAT_R16_UNORM,					(unorm16));
DXGI_TYPE(DXGI_FORMAT_R16_UINT,						(uint16));
DXGI_TYPE(DXGI_FORMAT_R16_SNORM,					(norm16));
DXGI_TYPE(DXGI_FORMAT_R16_SINT,						(int16));
DXGI_TYPE(DXGI_FORMAT_R8_UNORM,						(unorm8));
DXGI_TYPE(DXGI_FORMAT_R8_UINT,						(uint8));
DXGI_TYPE(DXGI_FORMAT_R8_SNORM,						(norm8));
DXGI_TYPE(DXGI_FORMAT_R8_SINT,						(int8));
#if 0
//DXGI_TYPE(DXGI_FORMAT_A8_UNORM,					unorm8);
//DXGI_TYPE(DXGI_FORMAT_R1_UNORM,					);
//DXGI_TYPE(DXGI_FORMAT_R9G9B9E5_SHAREDEXP,			);
//DXGI_TYPE(DXGI_FORMAT_R8G8_B8G8_UNORM,			);
//DXGI_TYPE(DXGI_FORMAT_G8R8_G8B8_UNORM,			);
DXGI_TYPE(DXGI_FORMAT_BC1_UNORM,					);
DXGI_TYPE(DXGI_FORMAT_BC1_UNORM_SRGB,				);
DXGI_TYPE(DXGI_FORMAT_BC2_UNORM,					);
DXGI_TYPE(DXGI_FORMAT_BC2_UNORM_SRGB,				);
DXGI_TYPE(DXGI_FORMAT_BC3_UNORM,					);
DXGI_TYPE(DXGI_FORMAT_BC3_UNORM_SRGB,				);
DXGI_TYPE(DXGI_FORMAT_BC4_UNORM,					);
DXGI_TYPE(DXGI_FORMAT_BC4_SNORM,					);
DXGI_TYPE(DXGI_FORMAT_BC5_UNORM,					);
DXGI_TYPE(DXGI_FORMAT_BC5_SNORM,					);
DXGI_TYPE(DXGI_FORMAT_B5G6R5_UNORM,					);
DXGI_TYPE(DXGI_FORMAT_B5G5R5A1_UNORM,				);
DXGI_TYPE(DXGI_FORMAT_B8G8R8A8_UNORM,				);
DXGI_TYPE(DXGI_FORMAT_B8G8R8X8_UNORM,				);
DXGI_TYPE(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,	);
DXGI_TYPE(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,			);
DXGI_TYPE(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,			);
DXGI_TYPE(DXGI_FORMAT_BC6H_UF16,					);
DXGI_TYPE(DXGI_FORMAT_BC6H_SF16,					);
DXGI_TYPE(DXGI_FORMAT_BC7_UNORM,					);
DXGI_TYPE(DXGI_FORMAT_BC7_UNORM_SRGB,				);
#endif

namespace iso {
DXGI_FORMAT GetDXGI(const ISO::Type *type) {
#define TYPE(F) { ISO::getdef<typename dxgi_type<F>::type>(), F }
	static struct { const ISO::Type *iso_type; DXGI_FORMAT type; } types[] = {
		TYPE(DXGI_FORMAT_R32G32B32A32_FLOAT),
		TYPE(DXGI_FORMAT_R32G32B32A32_UINT),
		TYPE(DXGI_FORMAT_R32G32B32A32_SINT),
		TYPE(DXGI_FORMAT_R32G32B32_FLOAT),
		TYPE(DXGI_FORMAT_R32G32B32_UINT),
		TYPE(DXGI_FORMAT_R32G32B32_SINT),
		TYPE(DXGI_FORMAT_R16G16B16A16_FLOAT),
		TYPE(DXGI_FORMAT_R16G16B16A16_UNORM),
		TYPE(DXGI_FORMAT_R16G16B16A16_UINT),
		TYPE(DXGI_FORMAT_R16G16B16A16_SNORM),
		TYPE(DXGI_FORMAT_R16G16B16A16_SINT),
		TYPE(DXGI_FORMAT_R32G32_FLOAT),
		TYPE(DXGI_FORMAT_R32G32_UINT),
		TYPE(DXGI_FORMAT_R32G32_SINT),
		TYPE(DXGI_FORMAT_R10G10B10A2_UNORM),
		TYPE(DXGI_FORMAT_R10G10B10A2_UINT),
		TYPE(DXGI_FORMAT_R11G11B10_FLOAT),
		TYPE(DXGI_FORMAT_R8G8B8A8_UNORM),
		TYPE(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB),
		TYPE(DXGI_FORMAT_R8G8B8A8_UINT),
		TYPE(DXGI_FORMAT_R8G8B8A8_SNORM),
		TYPE(DXGI_FORMAT_R8G8B8A8_SINT),
		TYPE(DXGI_FORMAT_R16G16_FLOAT),
		TYPE(DXGI_FORMAT_R16G16_UNORM),
		TYPE(DXGI_FORMAT_R16G16_UINT),
		TYPE(DXGI_FORMAT_R16G16_SNORM),
		TYPE(DXGI_FORMAT_R16G16_SINT),
		TYPE(DXGI_FORMAT_D32_FLOAT),
		TYPE(DXGI_FORMAT_R32_FLOAT),
		TYPE(DXGI_FORMAT_R32_UINT),
		TYPE(DXGI_FORMAT_R32_SINT),
		TYPE(DXGI_FORMAT_R8G8_UNORM),
		TYPE(DXGI_FORMAT_R8G8_UINT),
		TYPE(DXGI_FORMAT_R8G8_SNORM),
		TYPE(DXGI_FORMAT_R8G8_SINT),
		TYPE(DXGI_FORMAT_R16_FLOAT),
		TYPE(DXGI_FORMAT_D16_UNORM),
		TYPE(DXGI_FORMAT_R16_UNORM),
		TYPE(DXGI_FORMAT_R16_UINT),
		TYPE(DXGI_FORMAT_R16_SNORM),
		TYPE(DXGI_FORMAT_R16_SINT),
		TYPE(DXGI_FORMAT_R8_UNORM),
		TYPE(DXGI_FORMAT_R8_UINT),
		TYPE(DXGI_FORMAT_R8_SNORM),
		TYPE(DXGI_FORMAT_R8_SINT),
		TYPE(DXGI_FORMAT_A8_UNORM),
		TYPE(DXGI_FORMAT_R1_UNORM),
		TYPE(DXGI_FORMAT_R9G9B9E5_SHAREDEXP),
		TYPE(DXGI_FORMAT_R8G8_B8G8_UNORM),
		TYPE(DXGI_FORMAT_G8R8_G8B8_UNORM),
		TYPE(DXGI_FORMAT_BC1_UNORM),
		TYPE(DXGI_FORMAT_BC1_UNORM_SRGB),
		TYPE(DXGI_FORMAT_BC2_UNORM),
		TYPE(DXGI_FORMAT_BC2_UNORM_SRGB),
		TYPE(DXGI_FORMAT_BC3_UNORM),
		TYPE(DXGI_FORMAT_BC3_UNORM_SRGB),
		TYPE(DXGI_FORMAT_BC4_UNORM),
		TYPE(DXGI_FORMAT_BC4_SNORM),
		TYPE(DXGI_FORMAT_BC5_UNORM),
		TYPE(DXGI_FORMAT_BC5_SNORM),
		TYPE(DXGI_FORMAT_B5G6R5_UNORM),
		TYPE(DXGI_FORMAT_B5G5R5A1_UNORM),
		TYPE(DXGI_FORMAT_B8G8R8A8_UNORM),
		TYPE(DXGI_FORMAT_B8G8R8X8_UNORM),
		TYPE(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM),
		TYPE(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB),
		TYPE(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB),
		TYPE(DXGI_FORMAT_BC6H_UF16),
		TYPE(DXGI_FORMAT_BC6H_SF16),
		TYPE(DXGI_FORMAT_BC7_UNORM),
		TYPE(DXGI_FORMAT_BC7_UNORM_SRGB),
	};
#undef TYPE
	for (int i = 0; i < num_elements(types); i++) {
		if (type->SameAs(types[i].iso_type))
			return types[i].type;
	}
	return DXGI_FORMAT_UNKNOWN;
}
} // namespace iso
//-----------------------------------------------------------------------------
//	DX11Texture
//-----------------------------------------------------------------------------

ISO_DEFCALLBACKPODF(DX11Texture, rint32, ISO::TypeUser::WRITETOBIN);

template<typename R> void MakeDXT(ISO_rgba *srce, int srce_pitch, int width, int height) {
	R	*dest	= (R*)ISO::iso_bin_allocator().alloc(((width + 3) / 4) * ((height + 3) / 4) * sizeof(R));
	block<ISO_rgba, 2>	block	= make_strided_block(srce, width, srce_pitch, height);
	for (int y = 0; y < height; y += 4) {
		for (int x = 0; x < width; x += 4)
			dest++->Encode(block.sub<1>(x, 4).sub<2>(y, 4));
	}
}

void Make(DX11Texture *tex, void *texels, int srce_pitch, int width, int height, int depth, int levels, TextureType type, DXGI_FORMAT format, float threshold, bool hdr) {
	bool	premip	= levels < 0;
	if (premip)
		levels = -levels;

	tex->format		= format;
	tex->width		= width;
	tex->height		= height;
	tex->depth		= depth;
	tex->mips		= levels;
	tex->cube		= type == TT_CUBE;
	tex->array		= type == TT_ARRAY;
	tex->offset		= vram_align(16);

	if (hdr) {
		for (int z = 0; z < depth; z++) {
			HDRpixel	*srce		= (HDRpixel*)((uint8*)texels + srce_pitch * height * z);
			int			mipwidth	= width;
			int			mipheight	= height;

			for (int level = 0; level < levels; level++) {
				if (level > 0) {
					if (premip) {
						srce += mipwidth;
					} else {
						BoxFilter(
							make_strided_block(srce, mipwidth, srce_pitch, mipheight),
							make_strided_block(srce, 0, srce_pitch, 0)
						);
					}
					mipwidth	= max(mipwidth  >> 1, 1);
					mipheight	= max(mipheight >> 1, 1);
				}

				switch (format) {
					case DXGI_FORMAT_R16G16B16A16_FLOAT:	MakeTextureData<hfloat4p>			(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_R32G32_FLOAT:			MakeTextureData<hfloat2p>			(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_R32G32B32A32_FLOAT:	MakeTextureData<float4p>			(srce, srce_pitch, mipwidth, mipheight); break;
				}
			}
		}
	} else {
		for (int z = 0; z < depth; z++) {
			ISO_rgba	*srce		= (ISO_rgba*)((uint8*)texels + srce_pitch * height * z);
			int			mipwidth	= width;
			int			mipheight	= height;

			for (int level = 0; level < levels; level++) {
				if (level > 0) {
					if (premip) {
						srce += mipwidth;
					} else {
						BoxFilter(
							make_strided_block(srce, mipwidth, srce_pitch, mipheight),
							make_strided_block(srce, 0, srce_pitch, 0),
							false
						);
					}
					mipwidth	= max(mipwidth  >> 1, 1);
					mipheight	= max(mipheight >> 1, 1);
				}

				switch (format) {
					case DXGI_FORMAT_BC1_UNORM:				MakeDXT<BC<1> >						(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_BC2_UNORM:				MakeDXT<BC<2> >						(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_BC3_UNORM:				MakeDXT<BC<3> >						(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_A8_UNORM:				MakeTextureData<Texel<A8> >			(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_B5G6R5_UNORM:			MakeTextureData<Texel<B5G6R5> >		(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_B5G5R5A1_UNORM:		MakeTextureData<Texel<B5G5R5A1> >	(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_R8G8_UNORM:			MakeTextureData<Texel<R8G8> >		(srce, srce_pitch, mipwidth, mipheight); break;
					case DXGI_FORMAT_R8G8B8A8_UNORM:		MakeTextureData<Texel<R8G8B8A8> >	(srce, srce_pitch, mipwidth, mipheight); break;
				}
			}
		}
	}
}

ISO_ptr<DX11Texture> Bitmap2DX11Texture(ISO_ptr<bitmap> bm) {
	bm->Unpalette();

	DXGI_FORMAT	dest_format		= DXGI_FORMAT_UNKNOWN;
	float		threshold		= 0.f;

	if (ChannelUse cuf = GetFormatString(bm).begin()) {
		static struct {DXGI_FORMAT format; uint32 sizes; } formats[] = {
			//compressed
			{DXGI_FORMAT_BC1_UNORM,			CHANS(0x85,0x86,0x85,0x81)	},
			{DXGI_FORMAT_BC2_UNORM,			CHANS(0x85,0x86,0x85,0x04)	},
			{DXGI_FORMAT_BC3_UNORM,			CHANS(0x85,0x86,0x85,0x88)	},
			//8 bit
			{DXGI_FORMAT_A8_UNORM,			CHANS(8,0,0,0)	},
			//16 bit
			{DXGI_FORMAT_B5G6R5_UNORM,		CHANS(5,6,5,0)	},
			{DXGI_FORMAT_B5G5R5A1_UNORM,	CHANS(5,5,5,1)	},
			//32 bit
			{DXGI_FORMAT_R8G8B8A8_UNORM,	CHANS(8,8,8,8)	},
			//64 bit
			{DXGI_FORMAT_R16G16B16A16_FLOAT,CHANS(16,16,16,16) | channels::FLOAT | channels::SIGNED	},
			{DXGI_FORMAT_R32G32_FLOAT,		CHANS(32,32,0,0) | channels::FLOAT | channels::SIGNED	},
			//128 bit
			{DXGI_FORMAT_R32G32B32A32_FLOAT,CHANS(32,32,32,32) | channels::FLOAT | channels::SIGNED	},
		};
		int	i = cuf.FindBestMatch(&formats[0].sizes, num_elements(formats), &formats[1].sizes - &formats[0].sizes);
		if (i >= 0)
			dest_format = formats[i].format;
	} else {
		ChannelUse	cu((bitmap*)bm);
		bool		nocompress	= !!(bm->Flags() & BMF_NOCOMPRESS);//|| root("variables")["nocompress"].GetInt();
		bool		intensity	= cu.IsIntensity();

		switch (cu.nc) {
			case 0:
			case 1:
				if (cu.rc == 0x00050505) {
					dest_format = DXGI_FORMAT_A8_UNORM;
					break;
				}
				if (!nocompress) {
					dest_format = cu.analog.a ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_BC1_UNORM;
					break;
				}
			case 2:
				dest_format = DXGI_FORMAT_R8G8_UNORM;
				break;
			case 3:
			case 4:
				dest_format = nocompress ? DXGI_FORMAT_R8G8B8A8_UNORM : cu.analog.a ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_BC1_UNORM;
				break;
		}
	}

	uint32		blockx = DXGI_COMPONENTS(dest_format).IsBlock() ? 4 : 1, blocky = blockx;
	int			maxmips	= 1;

	if (bm->BaseWidth() % blockx || bm->BaseHeight() % blocky) {
		int		w = align(bm->BaseWidth(), blockx), h = align(bm->BaseHeight(), blocky);
		ISO_ptr<bitmap>	bm2(0);
		bm2->Create(w, h);
		resample_via<HDRpixel>(bm2->All(), bm->All());
		bm = bm2;
	}

	if (!(bm->Flags() & BMF_NOMIP) && is_pow2(bm->BaseWidth()) && is_pow2(bm->BaseHeight())) {
		maxmips = bm->MaxMips() - log2_floor(max(blockx, blocky));
	}

	uint32		vram	= vram_offset();
	ISO_ptr<DX11Texture>	tex(0);
	Make(tex,
		bm->ScanLine(0), bm->Width() * sizeof(ISO_rgba),
		bm->BaseWidth(), bm->BaseHeight(), bm->Depth(),
		bm->Mips() ? -min(bm->Mips(), maxmips) : maxmips,
		bm->Type(),
		dest_format,
		threshold, false
	);
	bm.UserInt() = vram_offset() - vram;
	return tex;
}

ISO_ptr<DX11Texture> HDRBitmap2DX11Texture(ISO_ptr<HDRbitmap> bm) {
	uint32		vram		= vram_offset();
	ISO_ptr<DX11Texture>	tex(0);
	Make(tex,
		bm->ScanLine(0), bm->Width() * sizeof(HDRpixel),
		bm->BaseWidth(), bm->BaseHeight(), bm->Depth(),
		bm->Flags() & BMF_NOMIP ? 1 : bm->Mips() ? -bm->Mips() : bm->MaxMips(),
		bm->Type(),
		DXGI_FORMAT_R16G16B16A16_FLOAT, 0, true
	);
	bm.UserInt() = vram_offset() - vram;
	return tex;
}

template<typename S, typename D> static uint32 Get(const block<D, 2> &dest, uint32 offset) {
	uint32	width	= dest.template size<1>(), height = dest.template size<2>();
	uint32	size	= width * height * sizeof(S);
	copy(make_block((S*)PlatformMemory(offset, size), width, height), dest);
	return offset + size;
}
template<typename R> uint32 GetDXT(const block<ISO_rgba, 2> &dest, uint32 offset) {
	uint32	width	= dest.template size<1>(), height = dest.template size<2>();
	uint32	size	= (width / 4) * (height / 4) * sizeof(R);
	R		*srce	= (R*)PlatformMemory(offset, size);
	for (int y = 0; y < height; y += 4) {
		for (int x = 0; x < width; x += 4)
			srce++->Decode(dest.sub<1>(x, 4).sub<2>(y, 4));
	}
	return offset + size;
}

ISO_ptr<bitmap2> DX11Texture2Bitmap(DX11Texture &tex) {
	bool	mipped	= tex.mips > 1;
	uint32	offset	= tex.offset;

	switch (tex.format) {
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_FLOAT: {
			ISO_ptr<HDRbitmap>	bm(0);
			bm->Create(tex.width * (mipped ? 2 : 1), tex.height * tex.depth, tex.depth > 1 && tex.depth != 6 && !tex.array ? BMF_VOLUME : 0, tex.depth);
			for (int z = 0; z < tex.depth; z++) {
				for (int level = 0; level < tex.mips; level++) {
					block<HDRpixel, 2>	block = mipped ? GetMip(bm->Slice(z), level) : bm->Slice(z);
					switch (tex.format) {
						case DXGI_FORMAT_R16G16B16A16_FLOAT:	offset = Get<hfloat4p>	(block, offset); break;
						case DXGI_FORMAT_R32G32B32A32_FLOAT:	offset = Get<float4p>	(block, offset); break;
					}
				}
			}
			return ISO_ptr<bitmap2>(0, bm);
		}
		default: {
			ISO_ptr<bitmap>	bm(0);
			bm->Create(tex.width * (mipped ? 2 : 1), tex.height * tex.depth, tex.depth > 1 && tex.depth != 6 && !tex.array ? BMF_VOLUME : 0, tex.depth);
			for (int z = 0; z < tex.depth; z++) {
				for (int level = 0; level < tex.mips; level++) {
					block<ISO_rgba, 2>	block = mipped ? GetMip(bm->Slice(z), level) : bm->Slice(z);
					switch (tex.format) {
						case DXGI_FORMAT_BC1_UNORM:			offset = GetDXT<DXT1rec>		(block, offset); break;
						case DXGI_FORMAT_BC2_UNORM:			offset = GetDXT<DXT23rec>		(block, offset); break;
						case DXGI_FORMAT_BC3_UNORM:			offset = GetDXT<DXT45rec>		(block, offset); break;
						case DXGI_FORMAT_A8_UNORM:			offset = Get<Texel<A8> >		(block, offset); break;
						case DXGI_FORMAT_B5G6R5_UNORM:		offset = Get<Texel<B5G6R5> >	(block, offset); break;
						case DXGI_FORMAT_B5G5R5A1_UNORM:	offset = Get<Texel<B5G5R5A1> >	(block, offset); break;
						case DXGI_FORMAT_R8G8_UNORM:		offset = Get<Texel<R8G8> >		(block, offset); break;
						case DXGI_FORMAT_R8G8B8A8_UNORM:	offset = Get<Texel<R8G8B8A8> >	(block, offset); break;
					}
				}
			}
			return ISO_ptr<bitmap2>(0, bm);
		}
	}
}

//-----------------------------------------------------------------------------
//	DX11Buffer
//-----------------------------------------------------------------------------
ISO_DEFUSERPODF(DX11Buffer, rint32, ISO::TypeUser::WRITETOBIN);

ISO_ptr<DX11Buffer> MakeDX11Buffer(ISO_ptr<void> p) {

	if (TypeType(p.GetType()) == ISO::OPENARRAY) {
		ISO_ptr<DX11Buffer>	p2(p.ID());

		ISO_openarray<void>	*a = p;
		p2->size	= a->Count();
		p2->offset	= vram_offset();

		const ISO::Type *type	= p.GetType()->SubType();
		uint32			size	= type->GetSize();
		if (DXGI_FORMAT dxgi = GetDXGI(type)) {
			p2->format	= dxgi;
		} else {
			p2->format	= size | 0x80000000;
		}

		size_t	total	= size * p2->size;
		memcpy(ISO::iso_bin_allocator().alloc(total), *a, total);
		return p2;
	}
	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	DX11SubMesh
//-----------------------------------------------------------------------------

ISO_DEFSAME(DXGI_FORMAT,	uint32);
ISO_DEFSAME(D3D11_INPUT_CLASSIFICATION,	uint32);
ISO_DEFCOMPV(D3D11_INPUT_ELEMENT_DESC, SemanticName, SemanticIndex, Format, InputSlot, AlignedByteOffset, InputSlotClass, InstanceDataStepRate);

ISO_DEFUSERCOMPFBV(DX11SubMesh, ISO::TypeUser::WRITETOBIN, SubMeshBase, ve, stride, vb_offset, vb_size, ib_offset, ib_size);

void ConvertComponent(
	void *srce_data, DXGI_FORMAT srce_type, int srce_stride,
	void *dest_data, DXGI_FORMAT dest_type, int dest_stride,
	size_t count
) {
	if (srce_type == dest_type) {
		struct uint96 {uint32 t[3]; };
		struct uint128 {uint32 t[4]; };
		switch (DXGI_COMPONENTS(srce_type).Bytes()) {
			case 1:		copy_n(strided((uint8*)		srce_data, srce_stride),	strided((uint8*)	dest_data, dest_stride), count); break;
			case 2:		copy_n(strided((uint16*)	srce_data, srce_stride),	strided((uint16*)	dest_data, dest_stride), count); break;
			case 4:		copy_n(strided((uint32*)	srce_data, srce_stride),	strided((uint32*)	dest_data, dest_stride), count); break;
			case 8:		copy_n(strided((uint64*)	srce_data, srce_stride),	strided((uint64*)	dest_data, dest_stride), count); break;
			case 12:	copy_n(strided((uint96*)	srce_data, srce_stride),	strided((uint96*)	dest_data, dest_stride), count); break;
			case 16:	copy_n(strided((uint128*)	srce_data, srce_stride),	strided((uint128*)	dest_data, dest_stride), count); break;
		}
		return;
	}

	switch (dest_type) {
		case DXGI_FORMAT_R32_FLOAT:
			copy_n(	strided((float*)					srce_data, srce_stride),
					strided((float*)					dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
			copy_n(	strided((array_vec<float,2>*)		srce_data, srce_stride),
					strided((array_vec<float,2>*)		dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			copy_n(	strided((array_vec<float,3>*)		srce_data, srce_stride),
					strided((array_vec<float,3>*)		dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R10G10B10A2_UNORM:
			copy_n(	strided((array_vec<float,3>*)		srce_data, srce_stride),
					strided((norm3_10_10_10*)			dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			copy_n(	strided((array_vec<float,4>*)		srce_data, srce_stride),
					strided((array_vec<unorm8,4>*)	dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R16G16_FLOAT:
			copy_n(	strided((array_vec<float,2>*)		srce_data, srce_stride),
					strided((hfloat2p*)					dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			copy_n(	strided((array_vec<float,4>*)		srce_data, srce_stride),
					strided((hfloat4p*)					dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R16G16B16A16_SINT:
			copy_n(	strided((array_vec<int16,4>*)		srce_data, srce_stride),
					strided((array_vec<int16,4>*)		dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R16G16B16A16_SNORM:
			if (srce_type == DXGI_FORMAT_R32_FLOAT) {
				copy_n(	strided((array_vec<float,4>*)		srce_data, srce_stride),
						strided((array_vec<norm16,4>*)	dest_data, dest_stride),
						count);
			} else {
				copy_n(	strided((array_vec<float,3>*)		srce_data, srce_stride),
						strided((array_vec<norm16,3>*)	dest_data, dest_stride),
						count);
				copy_n(	scalar<norm16>(0),
						strided((norm16*)dest_data + 3, dest_stride),
						count);
			}
			break;
		case DXGI_FORMAT_R16G16B16A16_UNORM:
			copy_n(	strided((array_vec<float,4>*)		srce_data, srce_stride),
					strided((array_vec<unorm16,4>*)	dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R8G8B8A8_UINT:
			copy_n(	strided((array_vec<uint16,4>*)	srce_data, srce_stride),
					strided((array_vec<uint8,4>*)		dest_data, dest_stride),
					count);
			break;
		case DXGI_FORMAT_R16G16_SINT:
			copy_n(	strided((array_vec<int16,2>*)		srce_data, srce_stride),
					strided((array_vec<int16,2>*)		dest_data, dest_stride),
					count);
			break;
		default:
			ISO_ASSERT(0);
	}
}

ISO_ptr<DX11SubMesh> SubMesh2DX11SubMesh(ISO_ptr<SubMesh> p) {
	ISO_ptr<DX11SubMesh>	p2(0);

	p2->minext		= p->minext;
	p2->maxext		= p->maxext;
	p2->flags		= p->flags;
	p2->technique	= p->technique;
	p2->parameters	= p->parameters;

	ISO::Browser			b(p->verts);
	uint32	num_verts	= b.Count();

	struct {
		const char	*usage;
		int			usage_index,	mult;
		DXGI_FORMAT	srce_type,		dest_type;
		int			srce_offset,	dest_offset;
		int			srce_size,		dest_size;
	} vert_data[16];

	const ISO::TypeComposite	*comp			= (const ISO::TypeComposite*)b[0].GetTypeDef();
	int							total			= 0;
	char						*srce_data		= *b;
	uint32						srce_stride		= b[0].GetSize();
	int							ncomp			= comp->Count();
	int							ncomp2			= 0;
	int							tex_index		= 0;

	for (int i = 0; i < ncomp; i++) {
		const ISO::Element	&e			= (*comp)[i];
		const ISO::Type		*type		= e.type;
		int					dims		= type->GetType() == ISO::ARRAY ? ((ISO::TypeArray*)type)->Count() : 1;
		tag2				id0			= comp->GetID(i);
		crc32				id			= comp->GetID(i);
		tag					name		= id0.get_tag();

		int	mult = 1;
		if (type->GetType() == ISO::ARRAY && ((ISO::TypeArray*)type)->subtype->GetType() == ISO::ARRAY) {
			mult = ((ISO::TypeArray*)type)->Count();
			type = ((ISO::TypeArray*)type)->subtype;
		}

		DXGI_FORMAT	srce_type	= GetDXGI(type);
		DXGI_FORMAT	dest_type	= srce_type;
		int			srce_size	= type->GetSize();
		int			usage_index	= 0;

		if (name && is_digit(name.end()[-1])) {
			const char *e = name.end();
			while (is_digit(e[-1]))
				--e;
			from_string(e, usage_index);
			name	= str(name.begin(), e);
			id		= name;
		}

		ChannelUse	cu;
		if (type->GetType() == ISO::ARRAY && ((ISO::TypeArray*)type)->subtype == ISO::getdef<float>())
			cu.Scan((const float*)(srce_data + e.offset), ((ISO::TypeArray*)type)->Count(), num_verts, srce_stride);

		if (id == "position") {
			dest_type = DXGI_FORMAT_R32G32B32_FLOAT;
		} else if (id == "normal") {
			dest_type = DXGI_FORMAT_R16G16B16A16_SNORM;
		} else if (id == "smooth_normal") {
			name		= "normal";
			usage_index	= 1;
			dest_type	= DXGI_FORMAT_R16G16B16A16_SNORM;
		} else if (id == "colour") {
			dest_type = DXGI_FORMAT_R8G8B8A8_UNORM;
			name		= "color";
		} else if (id == "centre") {
			name		= "position";
			usage_index = 1;
			dest_type	= DXGI_FORMAT_R32G32B32_FLOAT;
		} else if (id == "texcoord") {
			if (mult == 1) {
				dest_type	= DXGI_FORMAT_R16G16_FLOAT;
			} else {
				dest_type = DXGI_FORMAT_R16G16B16A16_FLOAT;
				mult		/= 2;
				usage_index	*= mult;
			}
			tex_index	= usage_index + mult;
		} else if (id == "tangent") {
			dest_type	= DXGI_FORMAT_R8G8B8A8_UNORM;
		} else if (id == "weights") {
			dest_type	= DXGI_FORMAT_R16G16B16A16_UNORM;
		} else if (id == "bones") {
			dest_type	= DXGI_FORMAT_R8G8B8A8_UINT;
		} else {
			name		= "texcoord";
			usage_index	= tex_index++;
		}


		vert_data[i].srce_type		= srce_type;
		vert_data[i].srce_offset	= e.offset;
		vert_data[i].srce_size		= srce_size;
		vert_data[i].dest_type		= dest_type;
		vert_data[i].dest_offset	= total;
		vert_data[i].dest_size		= DXGI_COMPONENTS(dest_type).Bytes();;
		vert_data[i].usage			= name;
		vert_data[i].usage_index	= usage_index;
		vert_data[i].mult			= mult;

		total		+= vert_data[i].dest_size * mult;
		ncomp2		+= mult;
	}

	p2->stride		= total;
	p2->vb_offset	= vram_align(16);
	p2->vb_size		= total * num_verts;

	D3D11_INPUT_ELEMENT_DESC	*pve = p2->ve.Create(ncomp2);
	char	*dest_data	= (char*)ISO::iso_bin_allocator().alloc(p2->vb_size);

	for (int i = 0; i < ncomp; i++) {
		for (int j = 0; j < vert_data[i].mult; j++, pve++) {
			uint32	dest_offset = vert_data[i].dest_offset + vert_data[i].dest_size * j;
			uint32	srce_offset = vert_data[i].srce_offset + vert_data[i].srce_size * j;

			clear(*pve);
			pve->SemanticName		= vert_data[i].usage;
			pve->SemanticIndex		= vert_data[i].usage_index;
			pve->Format				= vert_data[i].dest_type;
			pve->InputSlot			= 0;
			pve->AlignedByteOffset	= dest_offset;

			ConvertComponent(
				srce_data + srce_offset, vert_data[i].srce_type, srce_stride,
				dest_data + dest_offset, vert_data[i].dest_type, total,
				num_verts
			);
		}
	}

	p2->ib_offset	= vram_align(16);
	p2->ib_size		= p->indices.Count() * p->GetVertsPerPrim();

	copy_n(&p->indices[0][0], (uint16*)ISO::iso_bin_allocator().alloc(p2->ib_size * sizeof(uint16)), p2->ib_size);
	return p2;
}
#if 0
//-----------------------------------------------------------------------------
//	DX11SoundBuffer
//-----------------------------------------------------------------------------

class DX11SoundBuffer {
	uint32		buffer;
	uint32		size;
	uint32		length;
	float32		frequency;
	uint8		channels;
	uint8		type;
	uint16		flags;
public:
	void	Make(sample &s) {
		buffer		= vram_align(2048);
		length		= s.Length();
		frequency	= s.Frequency();
		channels	= s.Channels();
		size		= channels * length;
		type		= 0;
		flags		= s.Flags();
		iso_bin_allocator().alloc(s.Samples(), size);
	}
};

ISO_DEFUSER(DX11SoundBuffer, xint8[sizeof(DX11SoundBuffer)]);

ISO_ptr<DX11SoundBuffer> Sample2DX11SoundBuffer(sample &s) {
	ISO_ptr<DX11SoundBuffer>	p;
	p.Create()->Make(s);
	return p;
}
#endif

//-----------------------------------------------------------------------------
//	Shaders
//-----------------------------------------------------------------------------

typedef ISO_ptr<ISO_openarray<xint8> >	DX11ShaderCreate[SS_COUNT];
static ISO::TypeUser	def_DX11Shader("DX11Shader", ISO::getdef<DX11ShaderCreate>(), ISO::TypeUser::WRITETOBIN);

struct D3D_SHADER_MACRO2 : D3D_SHADER_MACRO {
	D3D_SHADER_MACRO2()				{ Name = Definition = 0; }
	~D3D_SHADER_MACRO2()			{ iso::free((char*)Name); iso::free((char*)Definition); }


	void Set(const string &name, const string &def) {
		(string&)Name		= name;
		(string&)Definition	= def;
	}
};

//#define D3DCOMPILE_FORCE_VS_SOFTWARE_NO_OPT	(1 << 6)
//#define D3DCOMPILE_FORCE_PS_SOFTWARE_NO_OPT	(1 << 7)

namespace iso {
inline size_t from_string(const char *s, D3D_SHADER_MACRO2 &x) {
	if (const char *e = strchr(s, '=')) {
		x.Set(str(s, e), e + 1);
	} else {
		x.Set(s, "");
	}
	return strlen(s);
}
}

class DX11IncludeHandler : public ID3DInclude, protected IncludeHandler {
	HRESULT	__stdcall Open(D3D_INCLUDE_TYPE include_type, const char *filename, const void *parent_data, const void **data, unsigned *bytes) noexcept {
		if (uint32 r = (uint32)open(filename, data)) {
			*bytes = r;
			return S_OK;
		}
		return E_FAIL;
	}
	HRESULT	__stdcall Close(const void *data) noexcept {
		close(data);
		return S_OK;
	}
public:
	DX11IncludeHandler(const filename *fn, const char *incs) : IncludeHandler(fn, incs)	{}
};

struct _DX11ShaderCompiler {
	dll_function<decltype(D3DCompile)>			Compile;
	dll_function<decltype(D3DCompile2)>			Compile2;
	dll_function<decltype(D3DPreprocess)>		Preprocess;
	dll_function<decltype(D3DGetBlobPart)>		GetBlobPart;
	dll_function<decltype(D3DSetBlobPart)>		SetBlobPart;
	dll_function<decltype(D3DStripShader)>		StripShader;
	dll_function<decltype(D3DCreateBlob)>		CreateBlob;
	dll_function<decltype(D3DCompressShaders)>	CompressShaders;

	bool init() {
		if (!Compile) {
			auto	mod = LoadLibraryA(D3DCOMPILER_DLL);
			if (!mod)
				return false;

			Compile.bind(			mod, "D3DCompile");
			Compile2.bind(			mod, "D3DCompile2");
			Preprocess.bind(		mod, "D3DPreprocess");
			GetBlobPart.bind(		mod, "D3DGetBlobPart");
			SetBlobPart.bind(		mod, "D3DSetBlobPart");
			StripShader.bind(		mod, "D3DStripShader");
			CreateBlob.bind(		mod, "D3DCreateBlob");
			CompressShaders.bind(	mod, "D3DCompressShaders");
		}
		return true;
	}
} DX11ShaderCompiler;

struct DX11Options {
	static Option option_list[];

	dynamic_array<D3D_SHADER_MACRO2>defines;
	const char						*incdirs;
	const char*						privatefn;
	uint8							optimisation;
	uint32							flags1;
	uint32							flags2;
	bool							compress;

	DX11Options(const char *_incdirs, const char *args);

	HRESULT	Compile(const memory_block &srce, const filename *fn, const char *profile, const char *entry, ID3DBlob **shader, ID3DBlob **errors) noexcept {
		uint32	flags = flags1;
		switch (optimisation) {
			case 0:		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0; break;
			case 1:		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL1; break;
			case 2:		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL2; break;
			default:	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3; break;
		}
		if (defines.back().Name)
			defines.push_back();

		DX11IncludeHandler	ih(fn, incdirs);
		HRESULT hr = DX11ShaderCompiler.Compile(srce, srce.length(), *fn,
			defines, &ih,
			entry,
			profile, flags, flags2,
			shader, errors
		);

		if (SUCCEEDED(hr)) {
			if (compress) {
				D3D_SHADER_DATA		data2 = {(*shader)->GetBufferPointer(), (*shader)->GetBufferSize()};
				ID3DBlob			*comp;
				if (SUCCEEDED(DX11ShaderCompiler.CompressShaders(1, &data2, D3D_COMPRESS_SHADER_KEEP_ALL_PARTS, &comp))) {
					(*shader)->Release();
					*shader = comp;
				}
			}
		}
		return hr;
	}

	HRESULT	Preprocess(const memory_block &srce, const filename *fn, ID3DBlob **text, ID3DBlob **errors) {
		if (defines.back().Name)
			defines.push_back();

		DX11IncludeHandler	ih(fn, incdirs);
		return DX11ShaderCompiler.Preprocess(srce, srce.length(), *fn,
			defines, &ih,
			text, errors
		);
	}
};

Option DX11Options::option_list[] = {
//	{"I",				read_as<array_entry>(&DX11Options::incdirs)											},	//additional include path
	{"D",				read_as<array_entry>(&DX11Options::defines)											},	//define macro

	{"Od",				flag_option<D3DCOMPILE_SKIP_OPTIMIZATION>(				&DX11Options::flags1)		},	//disable optimizations
	{"Op",				flag_option<D3DCOMPILE_NO_PRESHADER>(					&DX11Options::flags1)		},	//disable preshaders
	{"O",																		&DX11Options::optimisation	},	//optimization level 0..3.	1 is default
	{"WX",				flag_option<D3DCOMPILE_WARNINGS_ARE_ERRORS>(			&DX11Options::flags1)		},	//treat warnings as errors
	{"Vd",				flag_option<D3DCOMPILE_SKIP_VALIDATION>(				&DX11Options::flags1)		},	//disable validation
	{"Zi",				flag_option<D3DCOMPILE_DEBUG>(							&DX11Options::flags1)		},	//enable debugging information
	{"Zpr",				flag_option<D3DCOMPILE_PACK_MATRIX_ROW_MAJOR>(			&DX11Options::flags1)		},	//pack matrices in row-major order
	{"Zpc",				flag_option<D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR>(		&DX11Options::flags1)		},	//pack matrices in column-major order

	{"Gpp",				flag_option<D3DCOMPILE_PARTIAL_PRECISION>(				&DX11Options::flags1)		},	//force partial precision
	{"Gfa",				flag_option<D3DCOMPILE_AVOID_FLOW_CONTROL>(				&DX11Options::flags1)		},	//avoid flow control constructs
	{"Gfp",				flag_option<D3DCOMPILE_PREFER_FLOW_CONTROL>(			&DX11Options::flags1)		},	//prefer flow control constructs
	{"Gdp",				flag_option<D3DCOMPILE_EFFECT_ALLOW_SLOW_OPS>(			&DX11Options::flags2)		},	//disable effect performance mode
	{"Ges",				flag_option<D3DCOMPILE_ENABLE_STRICTNESS>(				&DX11Options::flags1)		},	//enable strict mode
	{"Gec",				flag_option<D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY>(	&DX11Options::flags1)		},	//enable backwards compatibility mode
	{"Gis",				flag_option<D3DCOMPILE_IEEE_STRICTNESS>(				&DX11Options::flags1)		},	//force IEEE strictness
	{"Gch",				flag_option<D3DCOMPILE_EFFECT_CHILD_EFFECT>(			&DX11Options::flags2)		},	//compile as a child effect for FX 4.x targets

#if 0
	{"Fo",				&DX11Options::flags1},	//output object file
	{"Fl",				&DX11Options::flags1},	//output a library
	{"Fc",				&DX11Options::flags1},	//output assembly code listing file
	{"Fx",				&DX11Options::flags1},	//output assembly code and hex listing file
	{"Fh",				&DX11Options::flags1},	//output header file containing object code
	{"Fe",				&DX11Options::flags1},	//output warnings and errors to a specific file
	{"Fd",				&DX11Options::flags1},	//extract shader PDB and write to given file
	{"Vn",				&DX11Options::flags1},	//use <name> as variable name in header file
	{"Cc",				&DX11Options::flags1},	//output color coded assembly listings
	{"Ni",				&DX11Options::flags1},	//output instruction numbers in assembly listings
	{"No",				&DX11Options::flags1},	//output instruction byte offset in assembly listings
	{"Lx",				&DX11Options::flags1},	//output hexadecimal literals
	{"shtemplate",		&DX11Options::flags1},	//template shader file for merging/matching resources
	{"mergeUAVs",		&DX11Options::flags1},	//merge UAV slots of template shader and current shader
	{"matchUAVs",		&DX11Options::flags1},	//match template shader UAV slots in current shader
	{"res_may_alias",	&DX11Options::flags1},	//assume that UAVs/SRVs may alias for cs_5_0+
	{"getprivate",		&DX11Options::flags1},	//save private data from shader blob
#endif

	{"compress",		read_as<yes>(&DX11Options::compress)	},
	{"setprivate",		&DX11Options::privatefn	},	//private data to add to compiled shader blob
};

DX11Options::DX11Options(const char *_incdirs, const char *args)
	: incdirs(_incdirs)
	, optimisation(1)
	, flags1(D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG)
	, flags2(0)
	, compress(false)
{
	if (!DX11ShaderCompiler.init())
		throw_accum("Can't find " << D3DCOMPILER_DLL);

	defines.push_back().Set("PLAT_PC", "");
	defines.push_back().Set("USE_DX11", "");

	for (const char *p = args, *e = string_end(p); p < e; ) {
		p = skip_whitespace(p);
		if (*p == '-' || *p == '/') {
			Option *o = find(option_list, ++p);
			if (o != end(option_list)) {
				size_t	len = o->set(this, p + strlen(o->name));
				if (len) {
					p += strlen(o->name) + len;
					continue;
				}
			}
		}
		throw_accum("Bad option: " << p);
	}
}

template<> struct field_names<D3D11_FILTER>				{ static field_value s[]; };
field_value field_names<D3D11_FILTER>::s[] = {
	{"MIN_MAG_MIP_POINT",							D3D11_FILTER_MIN_MAG_MIP_POINT},
	{"MIN_MAG_POINT_MIP_LINEAR",					D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR},
	{"MIN_POINT_MAG_LINEAR_MIP_POINT",				D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT},
	{"MIN_POINT_MAG_MIP_LINEAR",					D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR},
	{"MIN_LINEAR_MAG_MIP_POINT",					D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT},
	{"MIN_LINEAR_MAG_POINT_MIP_LINEAR",				D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR},
	{"MIN_MAG_LINEAR_MIP_POINT",					D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT},
	{"MIN_MAG_MIP_LINEAR",							D3D11_FILTER_MIN_MAG_MIP_LINEAR},
	{"ANISOTROPIC",									D3D11_FILTER_ANISOTROPIC},
	{"COMPARISON_MIN_MAG_MIP_POINT",				D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT},
	{"COMPARISON_MIN_MAG_POINT_MIP_LINEAR",			D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR},
	{"COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT",	D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT},
	{"COMPARISON_MIN_POINT_MAG_MIP_LINEAR",			D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR},
	{"COMPARISON_MIN_LINEAR_MAG_MIP_POINT",			D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT},
	{"COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR",	D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR},
	{"COMPARISON_MIN_MAG_LINEAR_MIP_POINT",			D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT},
	{"COMPARISON_MIN_MAG_MIP_LINEAR",				D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR},
	{"COMPARISON_ANISOTROPIC",						D3D11_FILTER_COMPARISON_ANISOTROPIC},
	{"MINIMUM_MIN_MAG_MIP_POINT",					D3D11_FILTER_MINIMUM_MIN_MAG_MIP_POINT},
	{"MINIMUM_MIN_MAG_POINT_MIP_LINEAR",			D3D11_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR},
	{"MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",		D3D11_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT},
	{"MINIMUM_MIN_POINT_MAG_MIP_LINEAR",			D3D11_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR},
	{"MINIMUM_MIN_LINEAR_MAG_MIP_POINT",			D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT},
	{"MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",		D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR},
	{"MINIMUM_MIN_MAG_LINEAR_MIP_POINT",			D3D11_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT},
	{"MINIMUM_MIN_MAG_MIP_LINEAR",					D3D11_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR},
	{"MINIMUM_ANISOTROPIC",							D3D11_FILTER_MINIMUM_ANISOTROPIC},
	{"MAXIMUM_MIN_MAG_MIP_POINT",					D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_POINT},
	{"MAXIMUM_MIN_MAG_POINT_MIP_LINEAR",			D3D11_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR},
	{"MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",		D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT},
	{"MAXIMUM_MIN_POINT_MAG_MIP_LINEAR",			D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR},
	{"MAXIMUM_MIN_LINEAR_MAG_MIP_POINT",			D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT},
	{"MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",		D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR},
	{"MAXIMUM_MIN_MAG_LINEAR_MIP_POINT",			D3D11_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT},
	{"MAXIMUM_MIN_MAG_MIP_LINEAR",					D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR},
	{"MAXIMUM_ANISOTROPIC",							D3D11_FILTER_MAXIMUM_ANISOTROPIC},
};


struct DX11ErrorCollector : ErrorCollector {
	void Errors(char *e);
};
void DX11ErrorCollector::Errors(char *e) {
	string_accum &b = error_builder;

	for (string_scan ss(e); ss.skip_whitespace().remaining(); ) {
		const char *s	= ss.getp();
		filename	fn;
		uint32		line = 0, col = 0, code = 0;

		if (ss.scan('(') && is_digit(ss.peekc(1))) {
			fn		= str(s, ss.getp());
			s = ss.getp();

			ss.move(1) >> line;
			if (ss.getc() == ',')
				ss >> col;
			ss.scan(')');
			if (ss.move(1).peekc() == ':')
				ss.move(1);
		} else {
			ss.move(s - ss.getp());
		}

		auto	tok			= ss.get_token();
		int		severity	= tok == "warning" ? 1 : tok == "error" ? 2 : 0;
		if (severity) {
			(severity == 1 ? have_warnings : have_errors) = true;
			ss.move(2) >> code;
		}

		if (ss.peekc() == ':')
			ss.move(1).skip_whitespace();
		s = ss.getp();
		if (!ss.scan('\n'))
			ss.move(int(ss.remaining()));

		count_string	msg	= str(s, ss.getp());

		if (had.insert(entry(code, fn, line, col)))
			Error(b, code, msg, fn, line);
	}
}

template<> const char* field_names<D3D11_TEXTURE_ADDRESS_MODE>::s[]	= {0, "Wrap","Mirror","Clamp","Border", "MirrorOnce", 0,0};
template<> const char* field_names<D3D11_COMPARISON_FUNC>::s[]		= {0, "Never","Less","Equal","Less_Equal","Greater","Not_Equal","Greater_Equal","Always", 0,0,0,0,0,0,0};

template<> field fields<D3D11_SAMPLER_DESC>::f[] = {
	{field::make("Filter",			&D3D11_SAMPLER_DESC::Filter)		},
	{field::make("AddressU",		&D3D11_SAMPLER_DESC::AddressU)		},
	{field::make("AddressV",		&D3D11_SAMPLER_DESC::AddressV)		},
	{field::make("AddressW",		&D3D11_SAMPLER_DESC::AddressW)		},
	{field::make("MipLODBias",		&D3D11_SAMPLER_DESC::MipLODBias)	},
	{field::make("MaxAnisotropy",	&D3D11_SAMPLER_DESC::MaxAnisotropy)	},
	{field::make("ComparisonFunc",	&D3D11_SAMPLER_DESC::ComparisonFunc)},
	{field::make("BorderColor",		&D3D11_SAMPLER_DESC::BorderColor)	},
	{field::make("MinLOD",			&D3D11_SAMPLER_DESC::MinLOD)		},
	{field::make("MaxLOD",			&D3D11_SAMPLER_DESC::MaxLOD)		},
	{},
};

D3D11_SAMPLER_DESC	MakeSamplerRecord(cgclib::sampler *s) {
	D3D11_SAMPLER_DESC	samp = {
		D3D11_FILTER_MIN_MAG_MIP_POINT,	//D3D11_FILTER Filter;
		D3D11_TEXTURE_ADDRESS_WRAP,		//D3D11_TEXTURE_ADDRESS_MODE AddressU;
		D3D11_TEXTURE_ADDRESS_WRAP,		//D3D11_TEXTURE_ADDRESS_MODE AddressV;
		D3D11_TEXTURE_ADDRESS_WRAP,		//D3D11_TEXTURE_ADDRESS_MODE AddressW;
		0,								//FLOAT MipLODBias;
		1,								//UINT	MaxAnisotropy;
		D3D11_COMPARISON_NEVER,			//D3D11_COMPARISON_FUNC ComparisonFunc;
		{1, 1, 1, 1},					//FLOAT BorderColor[ 4 ];
		0,								//FLOAT MinLOD;
		1e38f,							//FLOAT MaxLOD;
	};

	for (cgclib::item *j = s->states; j; j = j->next) {
		uint32			offset;
		if (const field *pf = FindField(fields<D3D11_SAMPLER_DESC>::f, istr(j->name), offset)) {
			switch (j->type) {
				case cgclib::STATE_SYM: {
					string_scan	ss(((cgclib::state_symb*)j)->value);
					GetField(ss, pf, (uint32*)&samp);
					break;
				}
			}
		}
	}

	return samp;
}

void CalcDXBCHash(dx::DXBC *dxbc) {
	MD5		md5;
	uint32	size	= dxbc->size - 20;
	md5.writebuff(&dxbc->version, size);

	uint32	last[16];
	clear(last);

	const uint32 remaining = size & 0x3f;
	if (remaining >= 56) {
		memcpy(&last[0], md5.block, remaining);
		last[remaining / 4] = 0x80;
		md5.process(last);
		clear(last);
	} else {
		memcpy(&last[1], md5.block, remaining);
		last[1 + remaining / 4] = 0x80;
	}

	last[ 0] = size * 8;
	last[15] = size * 2 + 1;
	md5.process(last);

	memcpy(dxbc->md5digest, &md5.state.a, 16);
}

#if 0
void PatchSamplers(void *data, const map<string, D3D11_SAMPLER_DESC> &samplers) {
	dx::DXBC	*dxbc	= (dx::DXBC*)data;
	dx::RDEF	*rdef	= ((dx::DXBC*)data)->GetBlob<dx::RDEF>();
	for (auto &i : rdef->Bindings()) {
		if (i.type == dx::RDEF::Binding::SAMPLER) {
			if (D3D11_SAMPLER_DESC *d = samplers.find(i.name.get(rdef)))
				i.samples = DX11CompactSampler(*d).u;
		}
	}
	CalcDXBCHash(dxbc);
}
#else
void PatchSamplers(com_ptr<ID3DBlob> &shader, const map<string, D3D11_SAMPLER_DESC> &samplers) {
	uint32		size	= (uint32)shader->GetBufferSize();
	void		*data	= shader->GetBufferPointer();
	dx::DXBC	*dxbc	= (dx::DXBC*)data;
	dx::RDEF	*rdef	= ((dx::DXBC*)data)->GetBlob<dx::RDEF>();
	uint32		add		= 0;

	for (auto &i : rdef->Bindings()) {
		if (i.type == dx::RDEF::Binding::SAMPLER) {
			if (samplers.find(i.name.get(rdef)))
				++add;
		}
	}
	if (add) {
		uint32	size2	= align(size + add * sizeof(D3D11_SAMPLER_DESC), 16);
		com_ptr<ID3DBlob>	shader2;
		DX11ShaderCompiler.CreateBlob(size2, &shader2);

		void	*data2	= shader2->GetBufferPointer();
		memcpy(data2, data, size);

		dxbc	= (dx::DXBC*)data2;
		rdef	= ((dx::DXBC*)data2)->GetBlob<dx::RDEF>();

		D3D11_SAMPLER_DESC	*desc = (D3D11_SAMPLER_DESC*)((uint8*)data2 + size);
		for (auto &i : rdef->Bindings()) {
			if (i.type == dx::RDEF::Binding::SAMPLER) {
				if (auto d = samplers.find(i.name.get(rdef))) {
					i.samples = (uint8*)desc - (uint8*)&i.samples;
					*desc++ = *d;
				}
			}
		}

		dxbc->size	= size2;

		CalcDXBCHash(dxbc);
		swap(shader, shader2);
	}
}
#endif

ISO_ptr<void> ReadDX11FX(tag id, istream_ref file, const filename *fn) {
	ISO::Browser		vars	= ISO::root("variables");
	DX11Options				options(vars["incdirs"].GetString(), vars["fxargs"].GetString());
	DX11ErrorCollector	errors;

	size_t			filelen	= size_t(file.length());
	malloc_block	srce(filelen);
	file.readbuff(srce, filelen);

	com_ptr<ID3DBlob>	shader, err;
	HRESULT hr = options.Preprocess(srce, fn, &shader, &err);
	if (err)
		errors.Errors((char*)err->GetBufferPointer());

	if (!shader || errors.have_errors) {
		throw(errors.error_builder.detach()->begin());
		return ISO_NULL;
	}

	ISO_ptr<fx>			pfx(id);

	cgclib::item	*items = ParseFX((char*)shader->GetBufferPointer());

	static struct StateName {
		const char		*name;
		ShaderStage		state;
		bool			set;
		bool	operator==(const char *n) const { return str(name) == n; }
	} state_names[] = {
		{"SetPixelShader",		SS_PIXEL,	true,},
		{"SetVertexShader",		SS_VERTEX,	true,},
		{"SetGeometryShader",	SS_GEOMETRY,true,},
		{"SetHullShader",		SS_HULL,	true,},
		{"SetDomainShader",		SS_LOCAL,	true,},
		{"SetComputeShader",	SS_PIXEL,	true,},

		{"PixelShader",			SS_PIXEL,	false,},
		{"VertexShader",		SS_VERTEX,	false,},
	};

	map<string, D3D11_SAMPLER_DESC>	samplers;

	for (cgclib::item *i = items; i; i = i->next) {
		switch (i->type) {
			// technique
			case cgclib::TECHNIQUE: {
				cgclib::technique	*cgct	= (cgclib::technique*)i;
				if (!cgct->passes)
					errors.Error(0, buffer_accum<256>("No passes in technique ") << cgct->name);

				ISO_ptr<technique>	tech	= pfx->Append().Create(cgct->name);
				for (cgclib::item *passes = cgct->passes; passes; passes = passes->next) {
					cgclib::pass 	*cgcp	= (cgclib::pass*)passes;
					bool			got_shader = false;

					ISO_ptr<DX11ShaderCreate>	pass = MakePtr(&def_DX11Shader, cgcp->name);
					tech->Append(pass);

					for (cgclib::item	*states = cgcp->shaders; states; states = states->next) {
						const StateName	*i = iso::find(state_names, states->name);
						if (i != iso::end(state_names)) {
							string_scan		s(((cgclib::state_symb*)states)->value);
							char_set		tokspec	= char_set::alphanum + '_';
							string			profile;

							if (i->set) {
								if (s.get_token(tokspec) != "CompileShader" || s.skip_whitespace().getc() != '(')
									continue;
								profile = s.get_token(tokspec);
								if (s.skip_whitespace().getc() != ',')
									continue;
							} else {
								profile = i->state == SS_PIXEL ? "ps_5_0" : "vs_5_0";
								options.flags1 &= ~D3DCOMPILE_ENABLE_STRICTNESS;
								options.flags1 |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
							}

							count_string	entry	= s.get_token(tokspec);

							com_ptr<ID3DBlob>	shader, err;
							HRESULT hr = options.Compile(srce, fn, profile, string(entry), &shader, &err);
							if (err)
								errors.Errors((char*)err->GetBufferPointer());

							if (shader) {
							#if 1
								PatchSamplers(shader, samplers);
								uint32		size	= (uint32)shader->GetBufferSize();
								void		*data	= shader->GetBufferPointer();
							#else
								uint32		size	= (uint32)shader->GetBufferSize();
								void		*data	= shader->GetBufferPointer();
								PatchSamplers(data, samplers);
							#endif
								memcpy(*(*pass)[i->state].Create(0, size), data, size);
								got_shader		= true;
							}

						}
					}
					if (!got_shader)
						errors.Error(0, buffer_accum<256>("No shaders in technique ") << cgct->name << ", pass " << cgcp->name);

				}
				break;
			}
			// sampler
			case cgclib::SAMPLER:
				samplers[i->name]		= MakeSamplerRecord((cgclib::sampler*)i);
				break;
		}
	}

	if (errors.have_errors)
		errors.Throw();

#ifdef ISO_EDITOR
	for (int j = 0; j < pfx->Count(); j++) {
		technique	&t = *(*pfx)[j];
		for (int k = 0; k < t.Count(); k++)
			Init((pass*)t[k], 0);
	}
#endif

	return pfx;
}

//-----------------------------------------------------------------------------
//	MakeDX11Shader
//-----------------------------------------------------------------------------

ISO_ptr<void> MakeDX11Shader(const ISO_ptr<ISO_openarray<ISO_ptr<string> > > &shader) {
	ISO::Browser		vars	= ISO::root("variables");
	DX11Options				options(vars["incdirs"].GetString(), vars["fxargs"].GetString());
	DX11ErrorCollector	errors;

	static struct StateName {
		const char		*name;
		const char		*profile;
		ShaderStage		state;
		bool	operator==(const char *n) const { return str(name) == n; }
	} state_names[] = {
		{"PS",	"ps_5_0",	SS_PIXEL	},
		{"VS",	"vs_5_0",	SS_VERTEX	},
		{"GS",	"gs_5_0",	SS_GEOMETRY	},
		{"HS",	"hs_5_0",	SS_HULL		},
		{"DS",	"ds_5_0",	SS_LOCAL	},
		{"CS",	"cs_5_0",	SS_PIXEL	},
	};

	ISO_ptr<DX11ShaderCreate>	p = MakePtr(&def_DX11Shader, shader.ID());
	for (auto &s : *shader) {
		const StateName	*i = iso::find(state_names, s.ID().get_tag());
		if (i != iso::end(state_names)) {
			com_ptr<ID3DBlob>				output, err;
			map<string, D3D11_SAMPLER_DESC>	samplers;
			HRESULT							hr;

			hr = options.Preprocess(memory_block(s->begin(), s->length()), 0, &output, &err);

			if (output) {
				cgclib::item *items = ParseFX((char*)output->GetBufferPointer());
				for (cgclib::item *i = items; i; i = i->next) {
					if (i->type == cgclib::SAMPLER)
						samplers[i->name] = MakeSamplerRecord((cgclib::sampler*)i);
				}
				output.clear();
			}

			err.clear();
			hr = options.Compile(memory_block(s->begin(), s->length()), 0, i->profile, "main", &output, &err);
			if (err)
				errors.Errors((char*)err->GetBufferPointer());

			if (output) {
			#if 1
				PatchSamplers(output, samplers);
				uint32		size	= (uint32)output->GetBufferSize();
				void		*data	= output->GetBufferPointer();
			#else
				uint32		size	= (uint32)output->GetBufferSize();
				void		*data	= output->GetBufferPointer();
				PatchSamplers(data, samplers);
			#endif
				memcpy(*(*p)[i->state].Create(0, size), data, size);
			}
		}
	}
	if (errors.have_errors)
		errors.Throw();

#ifdef ISO_EDITOR
	Init((pass*)(void*)p, 0);
#endif

	return p;

}

//-----------------------------------------------------------------------------

class PlatformDX11 : Platform {
public:
	PlatformDX11() : Platform("dx11") {
		ISO::getdef<DX11Buffer>();
		ISO::getdef<DX11Texture>();
		ISO::getdef<DX11SubMesh>();
//		ISO::getdef<PCSoundBuffer>();
		ISO_get_cast(MakeDX11Buffer);
		ISO_get_cast(Bitmap2DX11Texture);
		ISO_get_cast(HDRBitmap2DX11Texture);
		ISO_get_cast(DX11Texture2Bitmap);
		ISO_get_cast(SubMesh2DX11SubMesh);
	}
	type	Set() {
		Redirect<DataBuffer,	DX11Buffer>();
		Redirect<Texture,		DX11Texture>();
		Redirect<SampleBuffer,	sample>();
		Redirect<SubMeshPtr,	DX11SubMesh>();
		return PT_LITTLEENDIAN;
	}
	ISO_ptr<void>	ReadFX(tag id, istream_ref file, const filename *fn) {
		return ReadDX11FX(id, file, fn);
	}
	ISO_ptr<void>	MakeShader(const ISO_ptr<ISO_openarray<ISO_ptr<string> > > &shader) {
		return MakeDX11Shader(shader);
	}

} platform_dx11;

class PlatformDX12 : Platform {
public:
	PlatformDX12() : Platform("dx12") {
	}
	type	Set() {
		Redirect<DataBuffer,	DX11Buffer>();
		Redirect<Texture,		DX11Texture>();
		Redirect<SampleBuffer,	sample>();
		Redirect<SubMeshPtr,	DX11SubMesh>();
		return PT_LITTLEENDIAN;
	}
	ISO_ptr<void>	ReadFX(tag id, istream_ref file, const filename *fn) {
		return ReadDX11FX(id, file, fn);
	}
	ISO_ptr<void>	MakeShader(const ISO_ptr<ISO_openarray<ISO_ptr<string> > > &shader) {
		return MakeDX11Shader(shader);
	}

} platform_dx12;


//-----------------------------------------------------------------------------

struct DX11ShaderStage : ISO_openarray<xint8> {
	DX11ShaderStage(istream_ref file) : ISO_openarray<xint8>(file.length()) { file.readbuff(*this, size()); }
};
ISO_DEFUSER(DX11ShaderStage, ISO_openarray<xint8>);

class DX11ShaderFileHandler : public FileHandler {
public:
	virtual	const char*		GetDescription()	{ return "DX11 Shader Stage";	}

	virtual	int				Check(istream_ref file) {
		uint32be	u;
		file.seek(0);
		return file.read(u) && u == 'DXBC' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	virtual ISO_ptr<void>	Read(tag id, istream_ref file)	{
		uint32be	u;
		if (!file.read(u) || u != 'DXBC')
			return ISO_NULL;

		file.seek_cur(-4);
		return ISO_ptr<DX11ShaderStage>(id, file);
	}

} dx11shaderstage;


