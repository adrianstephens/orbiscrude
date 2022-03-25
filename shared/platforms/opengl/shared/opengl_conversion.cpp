#include "scenegraph.h"
#include "iso/iso_files.h"
#include "base/bits.h"
#include "base/list.h"
#include "base/algorithm.h"
#include "maths/geometry.h"
#include "packed_types.h"
#include "codec/texels/pvrtc.h"
#include "3d/model_utils.h"
#include "systems/conversion/platformdata.h"
#include "systems/conversion/channeluse.h"
#include "systems/conversion/strip.h"
//#include "PowerVR/Tools/PVRTexTool/Library/Include/PVRTextureUtilities.h"

#include "graphics_defs.h"
#include "model_defs.h"
#include "filetypes/fx.h"

extern "C" {
	inline double round(double x) { return iso::round(x); }
}

using namespace iso;

//-----------------------------------------------------------------------------
//	OGLTexture
//-----------------------------------------------------------------------------

namespace iso {
void Init(OGLTexture *x, void *physram) {
	x->name += ISO::iso_bin_allocator().fix(physram);
}
void DeInit(OGLTexture *x) {}
} // namespace iso

ISO_DEFCALLBACKPOD(OGLTexture, rint32);

static int adjust_pow2(int x0) {
	int	x1 = next_pow2(x0);
	return min(x1 >> int(x1 / 2 > x0 * 3), 2048);
}

int PVRTCompress2(const pixel8 *src, PVRTCrec *dest, uint32 width, uint32 height, uint32 pitch, bool bpp2) {
#if 1
	return 0;
#else
	if (dest && ISO::root("variables")["usepvrtexlib"].GetInt()) {
		using pvrtexture::PixelType;
		using pvrtexture::CPVRTextureHeader;
		using pvrtexture::CPVRTexture;

		#ifdef PLAT_PC
		static HMODULE h = LoadLibraryA("PVRTexLib.dll");
		#endif
		CPVRTexture	pvrtex(CPVRTextureHeader(pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8).PixelTypeID, height, width));
		copy(
			make_strided_block((ISO_rgba*)src, width, pitch * sizeof32(ISO_rgba), height),
			make_block((ISO_rgba*)pvrtex.getDataPtr(0), width, height)
		);
		Transcode(pvrtex, bpp2 ? ePVRTPF_PVRTCI_2bpp_RGBA : ePVRTPF_PVRTCI_4bpp_RGBA, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB);
		uint32	size	= pvrtex.getDataSize();
		memcpy(dest, pvrtex.getDataPtr(), size);
		return size;
	} else {
		return PVRTCompress(src, dest, width, height, pitch, bpp2);
	}
#endif
}

void Make(OGLTexture *tex, void *texels, int srce_pitch, int width, int height, int depth, int levels, TextureType type, uint8 srce_swizzle, TexFormat fmt, uint32 flags) {
	tex->width		= width - 1;
	tex->height		= height - 1;
	tex->format		= fmt;
	tex->mips		= levels - 1;
	tex->cube		= type == TT_CUBE;
	tex->name		= vram_align(32);

	if (type == TT_ARRAY)
		tex->name |= depth - 1;

	bool	premip	= levels < 0;
	if (premip)
		levels = -levels;

	for (int face = 0, faces = type == TT_CUBE ? 6 : 1; face < faces; face++) {
		ISO_rgba	*srce		= (ISO_rgba*)((uint8*)texels + srce_pitch * height * face);
		int			mipwidth	= width;
		int			mipheight	= height;

		for (int level = 0; level < levels; level++) {

			if (level > 0) {
				if (premip) {
					srce += mipwidth;
				} else {
					BoxFilter(
						make_strided_block(srce, mipwidth, srce_pitch, mipheight),
						make_strided_block(srce, 0,	srce_pitch, 0),
						false
					);
				}
				mipwidth	= max(mipwidth  >> 1, 1);
				mipheight	= max(mipheight >> 1, 1);
			}

			switch (fmt) {
				case TEXF_PVRTC2: case TEXF_PVRTC4: {
					bool	bpp2	= fmt == TEXF_PVRTC2;
					int		size	= PVRTCompress(0, 0, mipwidth, mipheight, srce_pitch, bpp2);
					PVRTCompress2((pixel8*)srce, (PVRTCrec*)ISO::iso_bin_allocator().alloc(size), mipwidth, mipheight, srce_pitch / sizeof(ISO_rgba), bpp2);
					break;
				}
				case TEXF_R8:		MakeTextureData<Texel<TexelFormat< 8,0,8,0,0,0,0> >			>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_R8G8:		MakeTextureData<Texel<TexelFormat<16,0,8,8,8,0,0> >			>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_R8G8B8:	MakeTextureData<Texel<TexelFormat<24,0,8,8,8,16,8> >		>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_R8G8B8A8:	MakeTextureData<Texel<TexelFormat<32,0,8,8,8,16,8,24,8> >	>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_L8:		MakeTextureData<Texel<TexelFormat< 8,0,8,0,0,0,0> >			>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_A8:		MakeTextureData<Texel<TexelFormat< 8,0,0,0,0,0,0,0,8> >		>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_L8A8:		MakeTextureData<Texel<TexelFormat<16,0,8,0,0,0,0,8,8> >		>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_R5G6B5:	MakeTextureData<Texel<TexelFormat<16,0,5,5,6,11,5> >		>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_R4G4B4A4:	MakeTextureData<Texel<TexelFormat<16,0,4,4,4,8,4,12,4> >	>(srce, srce_pitch, mipwidth, mipheight); break;
				case TEXF_R5G5B5A1:	MakeTextureData<Texel<TexelFormat<16,0,5,5,5,10,5,15,1> >	>(srce, srce_pitch, mipwidth, mipheight); break;
			}
		}
	}
}

ISO_ptr<OGLTexture> Bitmap2OGLTexture(ISO_ptr<bitmap> bm) {
	ISO_ptr<OGLTexture>	tex;
	cache_filename		cache(bm);

	if (cache.newer()) {
		vram_align(32);
		tex = cache.load(bm.ID());

	} else {
		bm->Unpalette();

		int			width			= bm->BaseWidth();
		int			depth			= bm->Depth();
		int			height			= bm->IsCube() ? bm->BaseHeight() : bm->Height();
		float		aspect			= max(width, height) / float(min(width, height));
		TexFormat	dest_format		= TEXF_UNKNOWN;
		uint32		srce_swizzle	= 0;

		fixed_string<64> format_string = GetFormatString(bm);
		if (ChannelUse cuf = (const char*)format_string) {

			static struct {TexFormat format; uint32 sizes; } formats[] = {
				{TEXF_R4G4B4A4,		CHANS(4,4,4,4)				},
				{TEXF_R5G5B5A1,		CHANS(5,5,5,1)				},
				{TEXF_R8G8B8A8,		CHANS(8,8,8,8)				},
				{TEXF_R5G6B5,		CHANS(5,6,5,0)				},
				{TEXF_R8G8B8,		CHANS(8,8,8,0)				},
				{TEXF_PVRTC2,		CHANS(0x85,0x86,0x85,0x81)	},
				{TEXF_PVRTC4,		CHANS(0x85,0x86,0x85,0x81)	},
				{TEXF_A8,			CHANS(0,0,0,8)				},
			};
			int	i = cuf.FindBestMatch(&formats[0].sizes, num_elements(formats), int(&formats[1].sizes - &formats[0].sizes));
			if (i >= 0)
				dest_format = formats[i].format;
		}

		if (dest_format == TEXF_UNKNOWN) {
			ChannelUse	cu((bitmap*)bm);
			bool		nocompress	= (bm->Flags() & BMF_NOCOMPRESS) || aspect > 2;// || root("variables")["nocompress"].GetInt();

			for (int i = 0; i < 4; i++) {
				if (cu.ch[i] < i) {
					cu.ch[i] = i;
					cu.rc[i] = i;
					cu.nc++;
				}
			}

			bool	intensity = cu.IsIntensity();

			switch (cu.nc) {
				case 0:
				case 1:
					dest_format	= cu.rc == 0x00040404 ? TEXF_A8 : intensity && cu.rc.a == ChannelUse::ONE ? TEXF_L8 : TEXF_PVRTC4;
					break;
				case 2:
					if (intensity) {
						dest_format	= TEXF_L8A8;
					} else if (nocompress) {
						dest_format	= TEXF_R8G8B8;
					} else {
						dest_format	= TEXF_PVRTC4;
					}
					break;
				case 3:
					if (nocompress) {
						dest_format	= cu.rc.a == ChannelUse::ONE ? TEXF_R8G8B8 : TEXF_R8G8B8A8;
					} else {
						dest_format	= TEXF_PVRTC4;
					}
					break;
				case 4:
					if (nocompress) {
						dest_format	= TEXF_R8G8B8A8;
					} else {
						dest_format	= TEXF_PVRTC4;
					}
					break;
			}
			srce_swizzle = cu.ch;
		}

		float	tex_scale	= (bm->Flags() & BMF_UNNORMALISED) || (bm.Flags() & ISO::Value::SPECIFIC)
			?	1
			:	ISO::root("variables")["tex_scale"].GetFloat(1);

		uint32	&putsize	= bm.UserInt();
		bool	mip			= !(bm->Flags() & BMF_NOMIP);// && !root("variables")["nomip"].GetInt();
		bool	square		= dest_format == TEXF_PVRTC2 || dest_format == TEXF_PVRTC4;
		bool	pow2		= square || mip || bm->IsCube();

		if (tex_scale != 1 || width > 2048 || height > 2048 || (pow2 && (!is_pow2(width) || !is_pow2(height))) || (square && width != height)) {
			int	w2	= int(width * tex_scale), h2 = int(height * tex_scale);
			if (pow2) {
				w2	= adjust_pow2(w2);
				h2	= adjust_pow2(h2);
			}
			if (square)
				w2 = h2 = max(w2, h2);

			ISO_ptr<bitmap> bm2(NULL);
			if (int mips = bm->Mips()) {
				bm2->Create(w2 * 2, h2, bm->Flags());
				bm2->SetMips(mips);
				for (int i = 0; i < mips; i++)
					resample_via<HDRpixel>(bm2->Mip(i), bm->Mip(i));
			} else {
				bm2->Create(w2, h2, bm->Flags());
				resample_via<HDRpixel>(bm2->All(), bm->All());
			}
			bm2->SetDepth(depth);
			bm		= bm2;
			width	= w2;
			height	= h2;
		}

		TextureType	type	= bm->Type();
		int			nmips	= !mip ? 1 : bm->Mips() ? bm->Mips() : bm->MaxMips();
		uint32		mipdim	= dest_format == TEXF_PVRTC2 ? min(width / 16, height / 8)
							: dest_format == TEXF_PVRTC4 ? min(width / 8, height / 8)
							: max(width, height);
		nmips	= min(nmips, (int)iso::log2(mipdim) + 1);

		Make(tex.Create(),
			bm->ScanLine(0), bm->Width() * sizeof(ISO_rgba),
			width, height, depth, nmips * (bm->Mips() ? -1 : 1), type,
			srce_swizzle, dest_format,
			0
		);

		uint32		vram	= tex->name & ~0x1f;
		void		*end	= ISO::iso_bin_allocator().alloc(0, 1);
		uint32		size	= vram_offset(end) - vram;
		putsize				= size;

		if (cache) {
			uint32	offset = tex->offset0;
			tex->offset0 = offset - vram;
			cache.store(tex, (char*)end - size, size);
			tex->offset0 = offset;
		}
	}

//	if (str(root("variables")["uvmode"].GetString()) == "clamp")
	if (bm->Flags() & BMF_UVCLAMP)
		tex->name |= 1 << 31;

	return tex;
}

template<typename FORMAT> static uint32 Get(const block<ISO_rgba, 2> &dest, uint32 offset) {
	uint32	width	= dest.size<1>(), height = dest.size<2>();
	uint32	size	= width * height * sizeof(Texel<FORMAT>);
	copy(make_block((Texel<FORMAT>*)PlatformMemory(offset, size), width, height), dest);
	return offset + size;
}

static void CopyRed(const block<ISO_rgba, 2> &dest) {
	for (int y = 0; y < dest.size<2>(); y++) {
		for (int x = 0; x < dest.size<1>(); x++)
			dest[y][x].g = dest[y][x].b = dest[y][x].r;
	}
}

ISO_ptr<bitmap2> OGLTexture2Bitmap(OGLTexture &tex) {
	int		width	= tex.width + 1;
	int		height	= tex.height + 1;
	int		levels	= tex.mips;
	int		faces	= tex.cube ? 6 : 1;
	int		fmt		= tex.format;
	uint32	offset	= tex.name & 0x7fffffe0;

	ISO_ptr<bitmap> bm(0);
	bm->Create(levels ? width * 2 : width, height * faces, 0, tex.cube ? 6 : (tex.name & 0x1f) + 1);
	if (levels) {
		bm->SetMips(levels);
		fill(bm->Block(width, 0, width, height), 0);
	}

	for (int z = 0; z < faces; z++) {
		for (int level = 0; level <= levels; level++) {
			block<ISO_rgba, 2>	block = levels ? GetMip(bm->Slice(z), level) : bm->Slice(z);
			switch (fmt) {
				case TEXF_PVRTC2: case TEXF_PVRTC4: {
					uint32	size = PVRTCDecompress(0, 0, block.size<1>(), block.size<2>(), block.pitch<2>(), fmt == TEXF_PVRTC2);
					offset += PVRTCDecompress(
						(PVRTCrec*)PlatformMemory(offset, size),
						(pixel8*)(ISO_rgba*)block[0],
						block.size<1>(), block.size<2>(), block.pitch<2>(),
						fmt == TEXF_PVRTC2
					);
					break;
				}
				case TEXF_R8:		offset = Get<R8>		(block, offset); break;
				case TEXF_R8G8:		offset = Get<R8G8>		(block, offset); break;
				case TEXF_R8G8B8:	offset = Get<R8G8B8>	(block, offset); break;
				case TEXF_R8G8B8A8:	offset = Get<R8G8B8A8>	(block, offset); break;
				case TEXF_L8:		offset = Get<TexelFormat< 8,0,8,0,0,0,0> >		(block, offset); CopyRed(block); break;
				case TEXF_A8:		offset = Get<A8>		(block, offset); break;
				case TEXF_L8A8:		offset = Get<TexelFormat<16,0,8,0,0,0,0,8,8> >	(block, offset); CopyRed(block); break;

				case TEXF_R5G6B5:	offset = Get<R5G6B5>	(block, offset); break;
				case TEXF_R4G4B4A4:	offset = Get<R4G4B4A4>	(block, offset); break;
				case TEXF_R5G5B5A1:	offset = Get<R5G5B5A1>	(block, offset); break;
			}
		}
	}
	return ISO_ptr<bitmap2>(0, bm);
}

ISO_ptr<OGLTexture> HDRBitmap2OGLTexture(ISO_ptr<HDRbitmap> bm) {
	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	OGLSubMesh
//-----------------------------------------------------------------------------
uint8	OGLSubMesh_pos_fmt;

ISO_DEFUSERCOMPV(OGLVertexElement, stream, offset, type, attribute);
ISO_DEFUSERCOMPFBV(OGLSubMesh, ISO::TypeUser::WRITETOBIN, SubMeshBase, ve, stride, vb_offset, vb_size, ib_offset, ib_size);

enum {
	GL_S8, GL_U8, GL_S16, GL_U16, GL_S32, GL_U32, GL_F32, GL_F16,
	GL_NORM = 0x80,
};

template<typename T> struct _ComponentType {};
template<> struct _ComponentType<uint8>							{ enum {value = GL_U8				}; };
template<> struct _ComponentType<unorm8>						{ enum {value = GL_U8  | GL_NORM	}; };
template<> struct _ComponentType<int8>							{ enum {value = GL_S8				}; };
template<> struct _ComponentType<norm8>							{ enum {value = GL_S8  | GL_NORM	}; };
template<> struct _ComponentType<uint16>						{ enum {value = GL_U16				}; };
template<> struct _ComponentType<unorm16>						{ enum {value = GL_U16 | GL_NORM	}; };
template<> struct _ComponentType<int16>							{ enum {value = GL_S16				}; };
template<> struct _ComponentType<norm16>						{ enum {value = GL_S16 | GL_NORM	}; };
template<> struct _ComponentType<float16>						{ enum {value = GL_F16				}; };
template<> struct _ComponentType<float>							{ enum {value = GL_F32				}; };

template<typename T, int N> struct _ComponentType<T[N]>			{ enum {value = _ComponentType<T>::value + ((N - 1) << 4)};	};
template<typename T, int N> struct _ComponentType<array_vec<T, N> >	: _ComponentType<T[4]> {};
template<> struct _ComponentType<rgba8>							: _ComponentType<unorm8[4]> {};
template<> struct _ComponentType<float4>						: _ComponentType<float[4]> {};
template<> struct _ComponentType<colour>						: _ComponentType<float[4]> {};

#define ComponentType(T)		_ComponentType<T>::value

static void ConvertComponent(
	void *srce_data, const ISO::Type *srce_type, int srce_stride,
	void *dest_data, int dest_type, int dest_stride,
	size_t count) {

	switch (dest_type) {
		case ComponentType(float):
			copy_n(	strided((float*)				srce_data, srce_stride),
					strided((float*)				dest_data, dest_stride),
					count);
			break;
		case ComponentType(float[2]):
			copy_n(	strided((array_vec<float,2>*)	srce_data, srce_stride),
					strided((array_vec<float,2>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(float[3]):
			copy_n(	strided((array_vec<float,3>*)	srce_data, srce_stride),
					strided((array_vec<float,3>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(float[4]):
			copy_n(	strided((array_vec<float,4>*)	srce_data, srce_stride),
					strided((array_vec<float,4>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(norm16[2]):
			copy_n(	strided((array_vec<float,2>*)	srce_data, srce_stride),
					strided((array_vec<norm16,2>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(norm16[3]):
			copy_n(	strided((array_vec<float,3>*)	srce_data, srce_stride),
					strided((array_vec<norm16,3>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(norm16[4]):
			copy_n(	strided((array_vec<float,4>*)	srce_data, srce_stride),
					strided((array_vec<norm16,4>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(norm8[2]):
			copy_n(	strided((array_vec<float,2>*)	srce_data, srce_stride),
					strided((array_vec<norm8,2>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(norm8[3]):
			copy_n(	strided((array_vec<float,3>*)	srce_data, srce_stride),
					strided((array_vec<norm8,3>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(norm8[4]):
			if (srce_type->SameAs<float[3]>()) {
				copy_n(	strided((array_vec<float,3>*)	srce_data, srce_stride),
						strided((array_vec<norm8,3>*)	dest_data, dest_stride),
						count);
				copy_n(	scalar<norm8>(0),
						strided((norm8*)dest_data + 3, dest_stride),
						count);
			} else {
				copy_n(	strided((array_vec<float,4>*)	srce_data, srce_stride),
						strided((array_vec<norm8,4>*)	dest_data, dest_stride),
						count);
				}
			break;
		case ComponentType(unorm8[2]):
			copy_n(	strided((array_vec<float,2>*)	srce_data, srce_stride),
					strided((array_vec<unorm8,2>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(unorm8[3]):
			copy_n(	strided((array_vec<float,3>*)	srce_data, srce_stride),
					strided((array_vec<unorm8,3>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(unorm8[4]):
			copy_n(	strided((array_vec<float,4>*)	srce_data, srce_stride),
					strided((array_vec<unorm8,4>*)	dest_data, dest_stride),
					count);
			break;

		case ComponentType(uint8[4]):
			copy_n(	strided((array_vec<uint16,4>*)	srce_data, srce_stride),
					strided((array_vec<uint8,4>*)	dest_data, dest_stride),
					count);
			break;

		case ComponentType(uint16[2]):
			copy_n(	strided((array_vec<uint16,2>*)	srce_data, srce_stride),
					strided((array_vec<uint16,2>*)	dest_data, dest_stride),
					count);
			break;
		case ComponentType(uint16[4]):
			copy_n(	strided((array_vec<uint16,4>*)	srce_data, srce_stride),
					strided((array_vec<uint16,4>*)	dest_data, dest_stride),
					count);
			break;

		default: ISO_ASSERT(0);
	}
}

#define TEMP(x)	{ComponentType(x), sizeof(x)}
static const uint8 types[][2] = {
	TEMP(float[3]),		//USAGE_POSITION,
	TEMP(norm8[4]),		//USAGE_NORMAL,
	TEMP(unorm8[4]),	//USAGE_COLOR,
	TEMP(unorm8[4]),	//USAGE_COLOR1,
	TEMP(float[2]),		//USAGE_TEXCOORD,
	TEMP(float[2]),		//USAGE_TEXCOORD1,
	TEMP(float[2]),		//USAGE_TEXCOORD2,
	TEMP(float[2]),		//USAGE_TEXCOORD3,
	TEMP(float[2]),		//USAGE_TEXCOORD4,
	TEMP(float[2]),		//USAGE_TEXCOORD5,
	TEMP(float[2]),		//USAGE_TEXCOORD6,
	TEMP(float[2]),		//USAGE_TEXCOORD7,
	TEMP(norm8[3]),		//USAGE_TANGENT,
	TEMP(norm8[3]),		//USAGE_BINORMAL,	(smooth_normal)
	TEMP(unorm8[4]),	//USAGE_BLENDWEIGHT,
	TEMP(uint8[4]),		//USAGE_BLENDINDICES,
};
#undef TEMP

OGLSubMesh::OGLSubMesh(SubMesh *p) {
	minext		= p->minext;
	maxext		= p->maxext;
	flags		= p->flags;
	technique	= p->technique;
	parameters	= p->parameters;
	stride 		= 0;

	const int MAX_COMP = 20;
	struct VertData {
		const ISO::Type*	srce_type;
		int					srce_offset, mult, dest_type, dest_size;
	} vert_data[MAX_COMP];

	clear(vert_data);

	int				num_comp2	= 0;

	for (auto e : p->VertComponents()) {
		USAGE2	usage(e.id);
		if (!usage)
			continue;

		const ISO::Type		*srce_type	= e.type;
		int					mult 		= 1;
		if (srce_type->GetType() == ISO::ARRAY && ((ISO::TypeArray*)srce_type)->subtype->GetType() == ISO::ARRAY) {
			mult		= ((ISO::TypeArray*)srce_type)->Count();
			srce_type	= ((ISO::TypeArray*)srce_type)->subtype;
		}

		int	dest_type	= types[usage - 1][0];
		int	dest_size	= types[usage - 1][1];
		int	dims		= srce_type->GetType() == ISO::ARRAY ? ((ISO::TypeArray*)srce_type)->Count() : 1;

		switch (usage) {
			case USAGE_POSITION:
				break;
			case USAGE_NORMAL:
				break;
			case USAGE_COLOR:
				break;
			case USAGE_TEXCOORD:
				break;
			case USAGE_TANGENT:
				break;
			case USAGE_BLENDWEIGHT:
				if (dims == 1)
					dest_type = ComponentType(float);
				break;
			case USAGE_BLENDINDICES:
				switch (dims) {
					case 1: dest_type = ComponentType(uint16[2]); break;
					case 2: dest_type = ComponentType(uint16[2]); break;
				}
				break;
		}

		if (dest_size) {
			dest_size = align(dest_size, 4);
			uint32	srce_offset = e.offset;
			uint32	srce_size	= e.size / mult;
			for (int i = 0; i < mult; i++) {
				VertData	&vd = vert_data[usage];
				vd.srce_type	= srce_type;
				vd.dest_type	= dest_type;
				vd.dest_size	= dest_size;
				vd.srce_offset	= srce_offset;
				srce_offset		+= srce_size;
				usage = USAGE(usage + (1 << 8));
			}
			num_comp2		+= mult;
			stride			+= dest_size;
		}
	}

	ve.Create(num_comp2);

// vertex data
	uint32	num_verts	= p->NumVerts();
	uint32	srce_stride	= p->VertSize();
	char	*srce_data	= p->VertData();

	vb_size		= num_verts * stride;
	char	*dest_data	= (char*)ISO::iso_bin_allocator().alloc(vb_size, 32);
	int		dest_offset	= 0;
	vb_offset	= ISO::iso_bin_allocator().fix(dest_data);

	for (int i = 0, j = 0; i < MAX_COMP; i++) {
		VertData	&vd = vert_data[i];
		if (vd.dest_size == 0)
			continue;
		ve[j].stream	= 0;
		ve[j].offset	= dest_offset;
		ve[j].attribute = i;
		ve[j].type		= vd.dest_type;
		j++;

		ConvertComponent(
			srce_data + vd.srce_offset, vd.srce_type, srce_stride,
			dest_data + dest_offset, vd.dest_type, stride,
			num_verts
		);
		dest_offset	+= vd.dest_size;
	}

// index data

//	typedef	aligned<position3,16>	vertex;
//	vertex	*verts	= new vertex[num_verts];
//	copy_n(strided((float3p*)srce_data, srce_stride), (position3*)verts, num_verts);
//	StripList	strips(p->indices, make_range_n(verts, num_verts));

	StripList	strips(p->indices, p->VertComponentRange<float3p>(0));

	int	numo	= 0, numf = 0, numi = -2;
	for (Strip *strip : strips) {
		numi	+= int(strip->size() + 2);
		if (strip->size() & 1)
			numo++;
		else if (strip->dir)
			numf++;
	}

	if (!(numo & 1) && numf != 0)
		numi++;

	ib_size		= numi * sizeof(uint16);
	uint16	*p1	= (uint16*)ISO::iso_bin_allocator().alloc(ib_size, 32), *p0 = p1;
	ib_offset	= ISO::iso_bin_allocator().fix(p1) | 1;

	// all the unflipped even-length ones first
	for (Strip *strip : strips) {
		int	numv	= int(strip->size());
		if ((numv & 1) || strip->dir)
			continue;
		if (p1 > p0)
			p1 += 2;
		copy_n(strip->begin(), p1, numv);
		if (p1 > p0) {
			p1[-2] = p1[-3];
			p1[-1] = p1[0];
		}
		p1 += numv;
	}
	// all the odd-length ones second
	for (Strip *strip : strips) {
		size_t	numv	= strip->size();
		if (!(numv & 1))
			continue;

		if (p1 > p0)
			p1 += 2;

		if (((p1 - p0) & 1) ^ strip->dir)
			copy_n(reversed(*strip).begin(), p1, numv);
		else
			copy_n(strip->begin(), p1, numv);

		if (p1 > p0) {
			p1[-2] = p1[-3];
			p1[-1] = p1[0];
		}
		p1 += numv;
	}
	if (!(numo & 1) && numf != 0) {
		p1[0] = p1[-1];
		p1 += 1;
	}
	// all the flipped even-length ones third
	for (Strip *strip : strips) {
		size_t	numv	= strip->size();
		if ((numv & 1) || !strip->dir)
			continue;
		if (p1 > p0)
			p1 += 2;
		copy_n(strip->begin(), p1, numv);
		if (p1 > p0) {
			p1[-2] = p1[-3];
			p1[-1] = p1[0];
		}
		p1 += numv;
	}
	ISO_ASSERT(p1 - p0 == numi);
}

ISO_ptr<OGLSubMesh> SubMesh2OGLSubMesh(ISO_ptr<SubMesh> p) {
	return ISO_ptr<OGLSubMesh>(p.ID(), p);
}

ISO_ptr<Model3> OGLModelCheck(ISO_ptr<Model3> p) {
	if (ISO::root("variables")["data"]["exportfor"].GetString() != cstr("ios"))
		return ISO_NULL;
	if (p->submeshes[0].IsType<SubMesh>()) {
		cuboid	ext	= cuboid(position3(p->minext), position3(p->maxext));
		float	bs	= len(ext.extent());
		float	acc	= reduce_min(ext.extent());

		for (int i = 0, n = p->submeshes.Count(); i < n; i++) {
			SubMesh		*submesh = (SubMesh*)(SubMeshBase*)p->submeshes[i];
			ISO::Browser	b(submesh->verts);
			acc			= GetAccuracy<position3>(acc, strided((array_vec<float,3>*)*b, b[0].GetSize()), &submesh->indices[0], submesh->indices.Count());
		}

		int		a			= uint32(iso::log2(bs / acc));
		OGLSubMesh_pos_fmt	= a < 7 ? 1 : a < 15 ? 2 : 4;
	}
	return p;
}

//-----------------------------------------------------------------------------

bitmap PVRTC(ISO_ptr<bitmap> bm, bool8 bpp2) {
	int		width	= bm->Width(), height = bm->Height(), pitch = width * sizeof(pixel8);
	int		size	= PVRTCompress(0, 0, width, height, pitch, bpp2);
	malloc_block	buffer(size);
	PVRTCompress((pixel8*)bm->ScanLine(0), (PVRTCrec*)buffer, width, height, pitch, bpp2);

	bitmap	bm2(width, height);
	PVRTCDecompress((PVRTCrec*)buffer, (pixel8*)bm2.ScanLine(0), width, height, width, bpp2);
	return bm2;
}

ISO_ptr<void> ReadOGLFX(tag id, istream_ref file, const filename *fn, const char *platdef) {
	ISO_ptr<fx>			pfx(id);
	malloc_block		mb			= malloc_block::zero_terminated(file);
	ISO::Browser		vars		= ISO::root("variables");
	const char			*incdirs	= vars["incdirs"].GetString();
	string				fxargs		= vars["fxargs"].GetString();

	CgStructHolder		cg;
	BufferOutputData	output, errors;
	cgclib::set_output(cg, &output);
	cgclib::set_errors(cg, &errors);

	const char*	macros[] = {
		"PLAT_GLSL",
		platdef
	};
	CGCLIBIncludeHandler includer(fn, incdirs);

	cgclib::Options		options;
	options.opts		|= cgclib::FX;
	options.macro		= macros;
	options.num_macro	= (uint32)num_elements(macros);
	options.includer	= &includer;
	options.name		= fn ? *fn : "";

	cgclib::item	*items = cgclib::compile(cg, cgclib::Blob(mb, mb.size()), options);

	if (!items || !errors.empty()) {
		errors.put("\0", 1);
		throw_accum(str((char*)errors.buffer));
	}

	for (cgclib::item *i = items; i; i = i->next) {
		switch (i->type) {
			case cgclib::TECHNIQUE: {
				ISO_ptr<technique>	t		= pfx->Append().Create(i->name);
				cgclib::technique	*cgct	= (cgclib::technique*)i;
				for (cgclib::pass *cgcp = cgct->passes; cgcp; cgcp = (cgclib::pass*)cgcp->next) {
					if (cgclib::shader *cgcs = (cgclib::shader*)cgcp->shaders) {
						typedef uint32			OGLShader[2];
						static ISO::TypeUser	def_physical_ptr("physical_ptr", ISO::getdef<uint32>());
						static ISO::TypeArray	def_physical_ptr2(&def_physical_ptr, 2);
						static ISO::TypeUser	def_OGLShader("OGLShader", &def_physical_ptr2);
						ISO_ptr<OGLShader>		pass = MakePtr(&def_OGLShader, cgcp->name);
						const char terminator[1]	= {0};

						if (vram_offset() == 0)
							ISO::iso_bin_allocator().alloc(1);
						(*pass)[0] = vram_add(cgcs->code, cgcs->size);
						vram_add(terminator, 1);

						cgcs = (cgclib::shader*)cgcs->next;
						if (cgcs && cgcs->size && ((char*)cgcs->code)[0]) {
							(*pass)[1] = vram_add(cgcs->code, cgcs->size);
							vram_add(terminator, 1);
						} else {
							(*pass)[1] = 0;
						}
						t->Append(pass);
					}
				}
				break;
			}
		}
	}
	return pfx;
}

//-----------------------------------------------------------------------------

template ISO::Type* ISO::getdef<iso::OGLTexture>();
template ISO::Type* ISO::getdef<iso::OGLSubMesh>();

static initialise init(
	ISO_get_conversion(OGLModelCheck),
	ISO_get_cast(Bitmap2OGLTexture),
	ISO_get_cast(OGLTexture2Bitmap),
	ISO_get_cast(HDRBitmap2OGLTexture),
	ISO_get_cast(SubMesh2OGLSubMesh),
	ISO_get_operation(PVRTC)
);
