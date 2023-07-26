#include "scenegraph.h"
#include "packed_types.h"
#include "systems/conversion/platformdata.h"
#include "systems/conversion/channeluse.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "base/algorithm.h"

#include "directx\Include\d3d9.h"
#include "directx\Include\d3dx9effect.h"
#include "directx\Include\d3dx9shader.h"
#include "directx\Include\D3Dcompiler.h"

#include "codec/texels/dxt.h"
#include "model_defs.h"
#include "com.h"

#undef min
#undef max

using namespace iso;

//-----------------------------------------------------------------------------
//	Shaders
//-----------------------------------------------------------------------------

struct MinimumD3D {
	IDirect3D9			*d3d;
	IDirect3DDevice9	*device;
	HWND				hWnd;
	MinimumD3D() {
		d3d		= Direct3DCreate9(D3D_SDK_VERSION);
		hWnd	= CreateWindowA("STATIC", "D3D Device Window", WS_POPUP, 0, 0, 1, 1, NULL, NULL, NULL, NULL);

		D3DPRESENT_PARAMETERS		pp;
		clear(pp);
		pp.BackBufferFormat			= D3DFMT_UNKNOWN;
		pp.SwapEffect				= D3DSWAPEFFECT_COPY;
		pp.Windowed					= TRUE;
		pp.EnableAutoDepthStencil	= FALSE;
		pp.AutoDepthStencilFormat	= D3DFMT_D24S8;
		d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
	}
	~MinimumD3D() {
//		device->Release();
		DestroyWindow(hWnd);
		d3d->Release();
	}
};

class DX9IncludeHandler : public ID3DXInclude, protected IncludeHandler {
	HRESULT	__stdcall Open(D3DXINCLUDE_TYPE include_type, const char *filename, const void *parent_data, const void **data, unsigned *bytes) {
		if (uint32 r = (uint32)open(filename, data)) {
			*bytes = r;
			return S_OK;
		}
		return E_FAIL;
	}
	HRESULT	__stdcall Close(const void *data) {
		close(data);
		return S_OK;
	}
public:
	DX9IncludeHandler(const filename *_fn, const char *_incs) : IncludeHandler(_fn, _incs)	{}
};

ISO_ptr<void> ReadDX9FX(tag id, istream_ref file, const filename *fn) {
	if (!LoadLibrary(D3DCOMPILER_DLL))
		throw_accum("Can't find " << D3DCOMPILER_DLL);

	static MinimumD3D	d3d;

	ISO_ptr<fx>			pfx(id);
	malloc_block		mb				= malloc_block::zero_terminated(file);
	ISO::Browser		vars			= ISO::root("variables");
	bool				optimise		= !!vars["optimise"].GetInt(1);
	const char			*incdirs		= vars["incdirs"].GetString();
	string				fxargs			= vars["fxargs"].GetString();


	const char*	args0[] = {
		"-name", *fn,
		"-incdirs", incdirs,
		"-implicitdown",
		"-fx",
		"-profile", "generic",
		"-parseonly",
		"-DPLAT_PC",
	};

	com_ptr<ID3DXEffect>	fx;
	com_ptr<ID3DXBuffer>	errors;
	D3DXMACRO				macros[2] = {
		{"PLAT_PC", ""},
		{NULL, NULL},
	};
	{
		DX9IncludeHandler	ih(fn, incdirs);
		D3DXCreateEffect(d3d.device, mb, UINT(mb.length()), macros, &ih, D3DXSHADER_DEBUG | (optimise ? 0 : D3DXSHADER_SKIPOPTIMIZATION) | D3DXSHADER_NO_PRESHADER, NULL, &fx, &errors);
	}
	if (errors) {
		char *e = (char*)errors->GetBufferPointer();
		if (!fx) {
			if (char *p = strchr((char*)errors->GetBufferPointer(), '(')) {
				int	line = from_string<int>(p + 1);
				throw_accum(p + 8 << " at line " << line);
			}
		}
		ISO_OUTPUT(e);
	}

	if (!fx)
		return ISO_NULL;

	D3DXEFFECT_DESC	fxdesc;
	fx->GetDesc(&fxdesc);

	pfx->Create(fxdesc.Techniques);
	for (unsigned i = 0; i < fxdesc.Techniques; i++) {
		D3DXHANDLE				th = fx->GetTechnique(i);
		D3DXTECHNIQUE_DESC		tdesc;
		fx->GetTechniqueDesc(th, &tdesc);
		fx->SetTechnique(th);

		technique			*t	= (*pfx)[i].Create(tdesc.Name);
		t->Create(tdesc.Passes);

		for (unsigned j = 0; j < tdesc.Passes; j++) {
			D3DXHANDLE				ph = fx->GetPass(th, j);
			D3DXPASS_DESC			pdesc;
			fx->GetPassDesc(ph, &pdesc);

			typedef ISO_ptr<void>	PCShaderCreate[2];
			static ISO::TypeUser	def_PCShader("PCShader", ISO::getdef<PCShaderCreate>());

			ISO_ptr<PCShaderCreate>	pass = MakePtr(&def_PCShader, pdesc.Name);
			if (int size = D3DXGetShaderSize(pdesc.pVertexShaderFunction)) {
				ISO::Type	*type = new ISO::TypeArray(ISO::getdef<rint32>(), size / 4);
				memcpy((*pass)[0] = MakePtr(type, "vertex"), pdesc.pVertexShaderFunction, size);
			}
			if (int size = D3DXGetShaderSize(pdesc.pPixelShaderFunction)) {
				ISO::Type	*type = new ISO::TypeArray(ISO::getdef<rint32>(), size / 4);
				memcpy((*pass)[1] = MakePtr(type, "pixel"), pdesc.pPixelShaderFunction,  size);
			}
			(*t)[j] = pass;
		}
	}
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
//	PCTexture
//-----------------------------------------------------------------------------

ISO_DEFUSERPOD(PCTexture, rint32);

template<typename R> void MakeDXT(ISO_rgba *srce, int srce_pitch, int width, int height) {
	R	*dest	= (R*)ISO::iso_bin_allocator().alloc(((width + 3) / 4) * ((height + 3) / 4) * sizeof(R));
	block<ISO_rgba, 2>	block(block<ISO_rgba, 1>(srce, width), srce_pitch, height);
	for (int y = 0; y < height; y += 4) {
		for (int x = 0; x < width; x += 4)
			dest++->Encode(block.sub<1>(x, 4).sub<2>(y, 4));
	}
}

void Make(PCTexture *tex, void *texels, uint32 srce_pitch, int width, int height, int depth, int levels, TextureType type, D3DFORMAT src_format, D3DFORMAT format, float threshold) {
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
				case D3DFMT_DXT1:			MakeDXT<DXT1rec>	(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_DXT2:			MakeDXT<DXT23rec>	(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_DXT4:			MakeDXT<DXT45rec>	(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A8:				MakeTextureData<Texel<TexelFormat< 8,0,0,0,0,0,0,0,8> > >		(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_R3G3B2:			MakeTextureData<Texel<TexelFormat< 8,5,3,2,3,0,2> > >			(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_L8:				MakeTextureData<Texel<TexelFormat< 8,0,8,0,0,0,0> > >			(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A4L4:			MakeTextureData<Texel<TexelFormat< 8,0,4,0,0,0,0,4,4> > >		(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_R5G6B5:			MakeTextureData<Texel<TexelFormat<16,11,5,5,6,0,5> > >			(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A1R5G5B5:		MakeTextureData<Texel<TexelFormat<16,10,5,5,5,0,5,15,1> > >		(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A4R4G4B4:		MakeTextureData<Texel<TexelFormat<16,8,4,4,4,0,4,12,4> > >		(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A8R3G3B2:		MakeTextureData<Texel<TexelFormat<16,5,3,2,3,0,2,8,8> > >		(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_R16F:			MakeTextureData<Texel<TexelFormat<16,0,4,4,4,8,4,12,4> > >		(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A8L8:			MakeTextureData<Texel<TexelFormat<16,0,8,0,0,0,0,8,8> > >		(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_L16:			MakeTextureData<Texel<TexelFormat<16,0,16,0,0,0,0> > >			(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_R8G8B8:			MakeTextureData<Texel<TexelFormat<24,16,8,8,8,0,8> > >			(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A8R8G8B8:		MakeTextureData<Texel<TexelFormat<32,16,8,8,8,0,8,24,8> > >		(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_A2R10G10B10:	MakeTextureData<Texel<TexelFormat<32,20,10,10,10,0,10,30,2> > >	(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_G16R16:			MakeTextureData<Texel<TexelFormat<32,0,8,0,0,0,0> > >			(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_G16R16F:		MakeTextureData<Texel<TexelFormat<32,0,0,0,0,0,0,0,8> > >		(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_R32F:			MakeTextureData<Texel<TexelFormat<32,0,8,0,0,0,0,8,8> > >		(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_A16B16G16R16:	MakeTextureData<Texel<TexelFormat<64,0,5,5,6,11,5> > >			(srce, srce_pitch, mipwidth, mipheight); break;
				case D3DFMT_A16B16G16R16F:	MakeTextureData<hfloat4p>										((HDRpixel*)srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_G32R32F:		MakeTextureData<Texel<TexelFormat<64,0,5,5,5,10,5,15,1> > >		(srce, srce_pitch, mipwidth, mipheight); break;
//				case D3DFMT_A32B32G32R32F:	MakeTextureData<Texel<TexelFormat<128,0,8,0,0,0,0> > >			(srce, srce_pitch, mipwidth, mipheight); break;
			}
		}
	}

}

ISO_ptr<PCTexture> Bitmap2PCTexture(ISO_ptr<bitmap> bm) {
	bm->Unpalette();

	D3DFORMAT	dest_format		= D3DFMT_UNKNOWN;
	float		threshold		= 0.f;

	if (ChannelUse cuf = GetFormatString(bm).begin()) {
		static struct {D3DFORMAT format; uint32 sizes; } formats[] = {
			//compressed
			{D3DFMT_DXT1,			CHANS(0x85,0x86,0x85,0x81)	},
			{D3DFMT_DXT2,			CHANS(0x85,0x86,0x85,0x04)	},
			{D3DFMT_DXT4,			CHANS(0x85,0x86,0x85,0x88)	},
			//8 bit
			{D3DFMT_A8,				CHANS(8,0,0,0)	},
			{D3DFMT_R3G3B2,			CHANS(3,3,2,0)	},
			{D3DFMT_L8,				CHANS(8,0,0,0) | channels::GREY	},
			{D3DFMT_A4L4,			CHANS(4,0,0,4) | channels::GREY	},
			//16 bit
			{D3DFMT_R5G6B5,			CHANS(5,6,5,0)	},
			{D3DFMT_A1R5G5B5,		CHANS(1,5,5,5)	},
			{D3DFMT_A4R4G4B4,		CHANS(4,4,4,4)	},
			{D3DFMT_A8R3G3B2,		CHANS(3,3,2,8)	},
			{D3DFMT_R16F,			CHANS(16,0,0,0) | channels::FLOAT | channels::SIGNED	},
			{D3DFMT_A8L8,			CHANS(8,0,0,8) | channels::GREY	},
			{D3DFMT_L16,			CHANS(16,0,0,0) | channels::GREY	},
			//24 bit
		    {D3DFMT_R8G8B8,			CHANS(8,8,8,0)	},
			//32 bit
			{D3DFMT_A8R8G8B8,		CHANS(8,8,8,8)	},
			{D3DFMT_A2R10G10B10,	CHANS(2,10,10,10)	},
			{D3DFMT_G16R16,			CHANS(16,16,0,0)	},
			{D3DFMT_G16R16F,		CHANS(16,16,0,0) | channels::FLOAT | channels::SIGNED	},
			{D3DFMT_R32F,			CHANS(32,0,0,0) | channels::FLOAT | channels::SIGNED	},
			//64 bit
			{D3DFMT_A16B16G16R16,	CHANS(16,16,16,16)	},
			{D3DFMT_A16B16G16R16F,	CHANS(16,16,16,16) | channels::FLOAT | channels::SIGNED	},
			{D3DFMT_G32R32F,		CHANS(32,32,0,0) | channels::FLOAT | channels::SIGNED	},
			//128 bit
			{D3DFMT_A32B32G32R32F,	CHANS(32,32,32,32) | channels::FLOAT | channels::SIGNED	},
		};
		int	i = cuf.FindBestMatch(&formats[0].sizes, num_elements(formats), &formats[1].sizes - &formats[0].sizes);
		if (i >= 0)
			dest_format = D3DFORMAT(formats[i].format);
	} else {
		ChannelUse	cu((bitmap*)bm);
		bool		nocompress	= !!(bm->Flags() & BMF_NOCOMPRESS);//|| root("variables")["nocompress"].GetInt();
		bool		intensity	= cu.IsIntensity();

		switch (cu.nc) {
			case 0:
			case 1:
				if (cu.rc == 0x00050505) {
					dest_format = D3DFMT_A8;
					break;
				}
				if (intensity && cu.rc.a == ChannelUse::ONE) {
					dest_format = D3DFMT_L8;
					break;
				}
				if (!nocompress) {
					dest_format = cu.analog.a ? D3DFMT_DXT4 : D3DFMT_DXT1;
					break;
				}
			case 2:
				if (intensity) {
					dest_format	= D3DFMT_A8L8;
					break;
				}
			case 3:
				if (nocompress && cu.rc.a == ChannelUse::ONE) {
					dest_format = D3DFMT_R8G8B8;
					break;
				}
			case 4:
				dest_format = nocompress ? D3DFMT_A8R8G8B8 : cu.analog.a ? D3DFMT_DXT4 : D3DFMT_DXT1;
				break;
		}
	}

	uint32		vram	= vram_offset();
	ISO_ptr<PCTexture>	tex(0);
	Make(tex,
		bm->ScanLine(0), bm->Width() * sizeof(ISO_rgba),
		bm->BaseWidth(), bm->BaseHeight(), bm->Depth(),
		bm->Flags() & BMF_NOMIP ? 1 : bm->Mips() ? -bm->Mips() : bm->MaxMips(),
		bm->Type(),
		D3DFMT_A8B8G8R8,
		dest_format,
		threshold
	);
	bm.UserInt() = vram_offset() - vram;
	return tex;
}

ISO_ptr<PCTexture> HDRBitmap2PCTexture(ISO_ptr<HDRbitmap> bm) {
	uint32		vram		= vram_offset();
	ISO_ptr<PCTexture>	tex(0);
	Make(tex,
		bm->ScanLine(0), bm->Width() * sizeof(HDRpixel), bm->BaseWidth(), bm->BaseHeight(), bm->Depth(), 1,
		bm->IsVolume() ? TT_VOLUME : bm->IsCube() ? TT_CUBE : bm->Depth() == 1 ? TT_NORMAL : TT_ARRAY,
		D3DFMT_A32B32G32R32F,
		D3DFMT_A16B16G16R16F, 0
	);
	bm.UserInt() = vram_offset() - vram;
	return tex;
}

ISO_ptr<PCTexture> VBitmap2PCTexture(ISO_ptr<vbitmap> bm) {
	return Bitmap2PCTexture(ISO_conversion::convert<bitmap>(bm));
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
			srce++->Decode(dest.sub<1>(x, 4).sub<2>(x, 4));
	}
	return offset + size;
}

ISO_ptr<bitmap2> PCTexture2Bitmap(PCTexture &tex) {
	bool	mipped	= tex.mips > 1;
	uint32	offset	= tex.offset;

	switch (tex.format) {
		case D3DFMT_R16F:
		case D3DFMT_L16:
		case D3DFMT_A2R10G10B10:
		case D3DFMT_G16R16:
		case D3DFMT_G16R16F:
		case D3DFMT_R32F:
		case D3DFMT_A16B16G16R16:
		case D3DFMT_G32R32F:
		case D3DFMT_A32B32G32R32F: {
			ISO_ptr<HDRbitmap>	bm(0);
			bm->Create(tex.width * (mipped ? 2 : 1), tex.height * tex.depth, tex.depth > 1 && tex.depth != 6 && !tex.array ? BMF_VOLUME : 0, tex.depth);
			for (int z = 0; z < tex.depth; z++) {
				for (int level = 0; level < tex.mips; level++) {
					block<HDRpixel, 2>	block = mipped ? GetMip(bm->Slice(z), level) : bm->Slice(z);
					switch (tex.format) {
		//				case D3DFMT_R16F:			offset = Get<Texel<TexelFormat<16,0,4,4,4,8,4,12,4> > >		(block, offset); break;
		//				case D3DFMT_L16:			offset = Get<Texel<TexelFormat<16,0,16,0,0,0,0> > >			(block, offset); break;
		//				case D3DFMT_A2R10G10B10:	offset = Get<Texel<TexelFormat<32,20,10,10,10,0,10,30,2> > >(block, offset); break;
		//				case D3DFMT_G16R16:			offset = Get<Texel<TexelFormat<32,0,8,0,0,0,0> > >			(block, offset); break;
		//				case D3DFMT_G16R16F:		offset = Get<Texel<TexelFormat<32,0,0,0,0,0,0,0,8> > >		(block, offset); break;
		//				case D3DFMT_R32F:			offset = Get<Texel<TexelFormat<32,0,8,0,0,0,0,8,8> > >		(block, offset); break;
		//				case D3DFMT_A16B16G16R16:	offset = Get<Texel<TexelFormat<64,0,5,5,6,11,5> > >			(block, offset); break;
						case D3DFMT_A16B16G16R16F:	offset = Get<hfloat4p>		(block, offset); break;
		//				case D3DFMT_G32R32F:		offset = Get<Texel<TexelFormat<64,0,5,5,5,10,5,15,1> > >	(block, offset); break;
		//				case D3DFMT_A32B32G32R32F:	offset = Get<Texel<TexelFormat<128,0,8,0,0,0,0> > >			(block, offset); break;
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
						case D3DFMT_DXT1:			offset = GetDXT<DXT1rec>									(block, offset); break;
						case D3DFMT_DXT2:			offset = GetDXT<DXT23rec>									(block, offset); break;
						case D3DFMT_DXT4:			offset = GetDXT<DXT45rec>									(block, offset); break;
						case D3DFMT_A8:				offset = Get<Texel<TexelFormat< 8,0,0,0,0,0,0,0,8> > >		(block, offset); break;
						case D3DFMT_R3G3B2:			offset = Get<Texel<TexelFormat< 8,5,3,2,3,0,2> > >			(block, offset); break;
						case D3DFMT_L8:				offset = Get<Texel<TexelFormat< 8,0,8,0,0,0,0> > >			(block, offset); break;
						case D3DFMT_A4L4:			offset = Get<Texel<TexelFormat< 8,0,4,0,0,0,0,4,4> > >		(block, offset); break;
						case D3DFMT_R5G6B5:			offset = Get<Texel<TexelFormat<16,11,5,5,6,0,5> > >			(block, offset); break;
						case D3DFMT_A1R5G5B5:		offset = Get<Texel<TexelFormat<16,10,5,5,5,0,5,15,1> > >	(block, offset); break;
						case D3DFMT_A4R4G4B4:		offset = Get<Texel<TexelFormat<16,8,4,4,4,0,4,12,4> > >		(block, offset); break;
						case D3DFMT_A8R3G3B2:		offset = Get<Texel<TexelFormat<16,5,3,2,3,0,2,8,8> > >		(block, offset); break;
						case D3DFMT_A8L8:			offset = Get<Texel<TexelFormat<16,0,8,0,0,0,0,8,8> > >		(block, offset); break;
						case D3DFMT_R8G8B8:			offset = Get<Texel<TexelFormat<24,16,8,8,8,0,8> > >			(block, offset); break;
						case D3DFMT_A8R8G8B8:		offset = Get<Texel<TexelFormat<32,16,8,8,8,0,8,24,8> > >	(block, offset); break;
					}
				}
			}
			return ISO_ptr<bitmap2>(0, bm);
		}
	}
}

//-----------------------------------------------------------------------------
//	PCSubMesh
//-----------------------------------------------------------------------------

typedef	tuple<
	uint16, uint16,
	uint8, uint8, uint8, uint8
>	PC_D3DVERTEXELEMENT9;

ISO_DEFSAME(PCVertexElement,	PC_D3DVERTEXELEMENT9);

ISO_DEFUSERCOMPFBV(PCSubMesh, ISO::TypeUser::WRITETOBIN, SubMeshBase, ve, stride, vb_offset, vb_size, ib_offset, ib_size);

static void ConvertComponent(
	void *srce_data, int srce_type, int srce_stride,
	void *dest_data, int dest_type, int dest_stride,
	size_t count) {

	switch (dest_type) {
		case D3DDECLTYPE_FLOAT1:
			copy_n(	strided((float*)					srce_data, srce_stride),
					strided((float*)					dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_FLOAT2:
			copy_n(	strided((array_vec<float,2>*)		srce_data, srce_stride),
					strided((array_vec<float,2>*)		dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_FLOAT3:
			copy_n(	strided((array_vec<float,3>*)		srce_data, srce_stride),
					strided((array_vec<float,3>*)		dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_DEC3N:
			copy_n(	strided((array_vec<float,3>*)		srce_data, srce_stride),
					strided((norm3_10_10_10*)			dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_UBYTE4N:
			copy_n(	strided((array_vec<float,4>*)		srce_data, srce_stride),
					strided((array_vec<unorm8,4>*)	dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_FLOAT16_2:
			copy_n(	strided((array_vec<float,2>*)		srce_data, srce_stride),
					strided((hfloat2p*)					dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_FLOAT16_4:
			copy_n(	strided((array_vec<float,4>*)		srce_data, srce_stride),
					strided((hfloat4p*)					dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_SHORT4:
			copy_n(	strided((array_vec<int16,4>*)		srce_data, srce_stride),
					strided((array_vec<int16,4>*)		dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_SHORT4N:
			if (srce_type == D3DDECLTYPE_FLOAT4) {
				copy_n(	strided((array_vec<float,4>*)		srce_data, srce_stride),
						strided((array_vec<norm16,4>*)	dest_data, dest_stride),
						count);
			} else {
				copy_n(	strided((array_vec<float,3>*)		srce_data, srce_stride),
						strided((array_vec<norm16,3>*)	dest_data, dest_stride),
						count);
				copy_n(	scalar<norm16>(0),
						strided((norm16*)dest_data + 3,		dest_stride),
						count);
			}
			break;
		case D3DDECLTYPE_UBYTE4:
			copy_n(	strided((array_vec<uint16,4>*)	srce_data, srce_stride),
					strided((array_vec<uint8,4>*)		dest_data, dest_stride),
					count);
			break;
		case D3DDECLTYPE_SHORT2:
			copy_n(	strided((array_vec<int16,2>*)		srce_data, srce_stride),
					strided((array_vec<int16,2>*)		dest_data, dest_stride),
					count);
			break;
		default:
			ISO_ASSERT(0);
	}
}

ISO_ptr<PCSubMesh> SubMesh2PCSubMesh(ISO_ptr<SubMesh> p) {
	ISO_ptr<PCSubMesh>	p2(0);

	p2->minext		= p->minext;
	p2->maxext		= p->maxext;
	p2->flags		= p->flags;
	p2->technique	= p->technique;
	p2->parameters	= p->parameters;

	ISO::Browser			b(p->verts);
	uint32	num_verts	= b.Count();

	struct {
		int srce_type,		dest_type;
		int	srce_offset,	dest_offset;
		int	srce_size,		dest_size;
		int	usage, usage_index, mult;
	} vert_data[16];

	const ISO::TypeComposite	*comp			= (const ISO::TypeComposite*)b[0].GetTypeDef();
	int							total			= 0;
	char						*srce_data		= *b;
	uint32						srce_stride		= b[0].GetSize();
	int							ncomp			= comp->Count();
	int							ncomp2			= 0;

	for (int i = 0; i < ncomp; i++) {
		const ISO::Element	&e			= (*comp)[i];
		const ISO::Type		*type		= e.type;
		int					dims		= type->GetType() == ISO::ARRAY ? ((ISO::TypeArray*)type)->Count() : 1;
		int					usage		= -1,				usage_index;
		int					srce_type	= -1,				dest_type;
		int					srce_size	= type->GetSize(),	dest_size;

		static struct { const ISO::Type *iso_type; int type; } known_types[] = {
			{ISO::getdef<float>(),			D3DDECLTYPE_FLOAT1		},
			{ISO::getdef<float[2]>(),		D3DDECLTYPE_FLOAT2		},
			{ISO::getdef<float[3]>(),		D3DDECLTYPE_FLOAT3		},
			{ISO::getdef<float[4]>(),		D3DDECLTYPE_FLOAT4		},
			{ISO::getdef<uint8[4]>(),		D3DDECLTYPE_UBYTE4		},
			{ISO::getdef<int16[2]>(),		D3DDECLTYPE_SHORT2		},
			{ISO::getdef<int16[4]>(),		D3DDECLTYPE_SHORT4		},
			{ISO::getdef<unorm8[4]>(),		D3DDECLTYPE_UBYTE4N		},
			{ISO::getdef<norm16[2]>(),		D3DDECLTYPE_SHORT2N		},
			{ISO::getdef<norm16[4]>(),		D3DDECLTYPE_SHORT4N		},
			{ISO::getdef<unorm16[2]>(),		D3DDECLTYPE_USHORT2N	},
			{ISO::getdef<unorm16[4]>(),		D3DDECLTYPE_USHORT4N	},
//			{ISO::getdef<uint3_10_10_10>(),	D3DDECLTYPE_UDEC3		},
//			{ISO::getdef<norm3_10_10_10>(),	D3DDECLTYPE_DEC3N		},
			{ISO::getdef<float16[2]>(),		D3DDECLTYPE_FLOAT16_2	},
			{ISO::getdef<float16[4]>(),		D3DDECLTYPE_FLOAT16_4	},
			//fake
			{ISO::getdef<uint16[2]>(),		D3DDECLTYPE_SHORT2		},
			{ISO::getdef<uint16[4]>(),		D3DDECLTYPE_SHORT4		},
			{ISO::getdef<uint32>(),			D3DDECLTYPE_SHORT2		},
		};
		static struct { crc32 id; int usage; int index; } known_uses[] = {
			{"position"_crc32,		D3DDECLUSAGE_POSITION, 		0},
			{"centre"_crc32,		D3DDECLUSAGE_POSITION, 		1},
			{"normal"_crc32,		D3DDECLUSAGE_NORMAL,		0},
			{"smooth_normal"_crc32,	D3DDECLUSAGE_NORMAL,		1},
			{"colour"_crc32,		D3DDECLUSAGE_COLOR,			0},
			{"texcoord0"_crc32,		D3DDECLUSAGE_TEXCOORD,		0},
			{"texcoord1"_crc32,		D3DDECLUSAGE_TEXCOORD,		1},
			{"texcoord2"_crc32,		D3DDECLUSAGE_TEXCOORD,		2},
			{"texcoord3"_crc32,		D3DDECLUSAGE_TEXCOORD,		3},
			{"texcoord4"_crc32,		D3DDECLUSAGE_TEXCOORD,		4},
			{"texcoord5"_crc32,		D3DDECLUSAGE_TEXCOORD,		5},
			{"texcoord6"_crc32,		D3DDECLUSAGE_TEXCOORD,		6},
			{"texcoord7"_crc32,		D3DDECLUSAGE_TEXCOORD,		7},
			{"tangent0"_crc32,		D3DDECLUSAGE_TANGENT,		0},
			{"tangent1"_crc32,		D3DDECLUSAGE_TANGENT,		1},
			{"tangent2"_crc32,		D3DDECLUSAGE_TANGENT,		2},
			{"tangent3"_crc32,		D3DDECLUSAGE_TANGENT,		3},
			{"tangent4"_crc32,		D3DDECLUSAGE_TANGENT,		4},
			{"tangent5"_crc32,		D3DDECLUSAGE_TANGENT,		5},
			{"tangent6"_crc32,		D3DDECLUSAGE_TANGENT,		6},
			{"tangent7"_crc32,		D3DDECLUSAGE_TANGENT,		7},
			{"weights"_crc32,		D3DDECLUSAGE_BLENDWEIGHT,	0},
			{"bones"_crc32,			D3DDECLUSAGE_BLENDINDICES,	0},
		};

		crc32	id = comp->GetID(i).get_crc32();
		for (int j = 0; j < num_elements(known_uses); j++) {
			if (id == known_uses[j].id) {
				usage		= known_uses[j].usage;
				usage_index	= known_uses[j].index;
				break;
			}
		}
		ISO_ASSERT(usage != -1);

		int	mult = 1;
		if (type->GetType() == ISO::ARRAY && ((ISO::TypeArray*)type)->subtype->GetType() == ISO::ARRAY) {
			mult = ((ISO::TypeArray*)type)->Count();
			type = ((ISO::TypeArray*)type)->subtype;
		}

		for (int j = 0; j < num_elements(known_types); j++) {
			if (type->SameAs(known_types[j].iso_type)) {
				srce_type = known_types[j].type;
				break;
			}
		}
		ISO_ASSERT(srce_type != -1);

		ChannelUse	cu;
		if (type->GetType() == ISO::ARRAY && ((ISO::TypeArray*)type)->subtype == ISO::getdef<float>())
			cu.Scan((const float*)(srce_data + e.offset), ((ISO::TypeArray*)type)->Count(), num_verts, srce_stride);

		switch (usage) {
			case D3DDECLUSAGE_POSITION:
				dest_type = D3DDECLTYPE_FLOAT3;
				dest_size = 12;
				break;
			case D3DDECLUSAGE_NORMAL:
				dest_type = D3DDECLTYPE_SHORT4N;
				dest_size = 8;
				break;
				switch (cu.ch[3]) {
					case ChannelUse::ONE:
					case ChannelUse::ALPHA:
						dest_type = D3DDECLTYPE_SHORT4N;
						dest_size = 8;
						break;
					default:
						dest_type = D3DDECLTYPE_FLOAT16_4;
						dest_size = 8;
//						dest_type = D3DDECLTYPE_DEC3N;
//						dest_size = 4;
						break;
				}
				break;
			case D3DDECLUSAGE_COLOR:
				dest_type = D3DDECLTYPE_UBYTE4N;
				dest_size = 4;
				break;
			case D3DDECLUSAGE_TEXCOORD:
				if (mult == 1) {
					dest_type	= D3DDECLTYPE_FLOAT16_2;
					dest_size	= 4;
				} else {
					dest_type = D3DDECLTYPE_FLOAT16_4;
					mult		/= 2;
					usage_index	*= mult;
					dest_size	= 8;
				}
				break;
			case D3DDECLUSAGE_TANGENT:
				dest_type = D3DDECLTYPE_FLOAT16_4;
				dest_size = 8;
//				dest_type = D3DDECLTYPE_DEC3N;
//				dest_size = 4;
				break;
			case D3DDECLUSAGE_BLENDWEIGHT:
				dest_type = dims == 1 ? D3DDECLTYPE_FLOAT1 : D3DDECLTYPE_UBYTE4N;
				dest_size = 4;
				break;
			case D3DDECLUSAGE_BLENDINDICES:
				dest_type = srce_type == D3DDECLTYPE_SHORT4 ? D3DDECLTYPE_UBYTE4 : srce_type;
				dest_size = 4;
				break;
		}

		vert_data[i].srce_type		= srce_type;
		vert_data[i].srce_offset	= e.offset;
		vert_data[i].srce_size		= srce_size;
		vert_data[i].dest_type		= dest_type;
		vert_data[i].dest_offset	= total;
		vert_data[i].dest_size		= dest_size;
		vert_data[i].usage			= usage;
		vert_data[i].usage_index	= usage_index;
		vert_data[i].mult			= mult;

		total		+= dest_size * mult;
		ncomp2		+= mult;
	}

	p2->stride		= total;
	p2->vb_offset	= vram_align(16);
	p2->vb_size		= total * num_verts;

	D3DVERTEXELEMENT9	*pve = p2->ve.Create(ncomp2 + 1);
	char	*dest_data	= (char*)ISO::iso_bin_allocator().alloc(p2->vb_size);

	for (int i = 0; i < ncomp; i++) {
		for (int j = 0; j < vert_data[i].mult; j++, pve++) {
			uint32	dest_offset = vert_data[i].dest_offset + vert_data[i].dest_size * j;
			uint32	srce_offset = vert_data[i].srce_offset + vert_data[i].srce_size * j;

			pve->Stream		= 0;
			pve->Method		= D3DDECLMETHOD_DEFAULT;
			pve->Usage		= vert_data[i].usage;
			pve->UsageIndex	= vert_data[i].usage_index + j;
			pve->Type		= vert_data[i].dest_type;
			pve->Offset		= dest_offset;

			if (pve->Usage == D3DDECLUSAGE_BLENDWEIGHT && vert_data[i].srce_type == D3DDECLTYPE_FLOAT4) {
				for (int i = 0; i < num_verts; i++) {
					float4p	*v4 = (float4p*)(srce_data + srce_offset + srce_stride * i);
					float		total = v4->x + v4->y + v4->z + v4->w;
					v4->x /= total;
					v4->y /= total;
					v4->z /= total;
					v4->w /= total;
				}
			}

			ConvertComponent(
				srce_data + srce_offset, vert_data[i].srce_type, srce_stride,
				dest_data + dest_offset, vert_data[i].dest_type, total,
				num_verts
			);
		}
	}
	pve->Stream		= 0xff;
	pve->Method		= D3DDECLMETHOD_DEFAULT;
	pve->Usage		= 0;
	pve->UsageIndex	= 0;
	pve->Type		= D3DDECLTYPE_UNUSED;
	pve->Offset		= 0;


	D3DFORMAT	ibfmt		= num_verts > 65535 ? D3DFMT_INDEX32 : D3DFMT_INDEX16;
	p2->ib_offset	= vram_align(16);
	p2->ib_size		= p->indices.Count() * 3;

	if (ibfmt == D3DFMT_INDEX16) {
		copy_n(&p->indices[0][0], (uint16*)ISO::iso_bin_allocator().alloc(p2->ib_size * sizeof(uint16)), p2->ib_size);
	} else {
		copy_n(&p->indices[0][0], (uint32*)ISO::iso_bin_allocator().alloc(p2->ib_size * sizeof(uint32)), p2->ib_size);
	}
	return p2;
}

//-----------------------------------------------------------------------------
//	PCSoundBuffer
//-----------------------------------------------------------------------------
#if 0
class PCSoundBuffer : bigendian_types {
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

ISO_DEFUSER(PCSoundBuffer, xint8[sizeof(PCSoundBuffer)]);

ISO_ptr<PCSoundBuffer> Sample2PCSoundBuffer(sample &s) {
	ISO_ptr<PCSoundBuffer>	p;
	if (s.flags & sample::NOCOMPRESS)
		p.Create()->Make(s);
	else {
		iso_ptr32<sample>	sp = &s;
		p.Create()->Make(*(ISO_xma*)ISO_conversion::convert<ISO_xma>(((ISO_ptr<void>&)sp)));
	}
	return p;
}
#endif

//-----------------------------------------------------------------------------
//	MakeParameterStruct
//-----------------------------------------------------------------------------

bool AddParameter(ISO::TypeComposite &comp, D3DXPARAMETER_TYPE type, int rows, int cols, const char *name) {
	if (!comp.Find(name)) switch (type) {
		case D3DXPT_INT:
			comp.Add<int>(name); return true;
			break;
		case D3DXPT_FLOAT:
			switch (rows * cols) {
				case 1:	comp.Add<float>(name);		return true;
				case 2:	comp.Add<float[2]>(name);	return true;
				case 3:	comp.Add<float[3]>(name);	return true;
				case 4:	comp.Add<float[4]>(name);	return true;
				case 9:	comp.Add<float3x3p>(name);	return true;
				case 12:comp.Add<float3x4p>(name);	return true;
				case 16:comp.Add<float4x4p>(name);	return true;
			}
			break;
		case D3DXPT_SAMPLER:
		case D3DXPT_SAMPLER1D:
		case D3DXPT_SAMPLER2D:
		case D3DXPT_SAMPLER3D:
		case D3DXPT_SAMPLERCUBE:
			comp.Add<Texture>(name);
			return true;
	}
	return false;
}

bool AddParameter(ISO::TypeComposite &comp, ID3DXEffect *fx, D3DXHANDLE ph) {
	if (!ph)
		return false;
	D3DXPARAMETER_DESC	pdesc;
	fx->GetParameterDesc(ph, &pdesc);
	return AddParameter(comp, pdesc.Type, pdesc.Rows, pdesc.Columns, pdesc.Name);
}

void CollectParameters(ISO::TypeComposite &comp, ID3DXConstantTable *ct) {
	D3DXCONSTANTTABLE_DESC		ctdesc;
	ct->GetDesc(&ctdesc);

	for (int i = 0; i < ctdesc.Constants; i++) {
		D3DXCONSTANT_DESC	cdesc;
		UINT				count	= 1;
		D3DXHANDLE			ch		= ct->GetConstant(NULL, i);
		ct->GetConstantDesc(ch, &cdesc, &count);
		if (cdesc.Name[0] == '_')
			continue;
		AddParameter(comp, cdesc.Type, cdesc.Rows, cdesc.Columns, cdesc.Name);
	}
}

void CollectParameters(ISO::TypeComposite &comp, ID3DXEffect *fx, const char *tname) {
	if (D3DXHANDLE ta = fx->GetAnnotationByName(fx->GetTechniqueByName(tname), "iso_output")) {
		const char *		iso_output;
		char				name[256];
		fx->GetString(ta, &iso_output);

		while (const char *semi = strchr(iso_output, ';')) {
			int	len = semi - iso_output;
			memcpy(name, iso_output, len);
			name[len] = 0;
			iso_output = semi + 1;
			AddParameter(comp, fx, fx->GetParameterByName(NULL, name));
		}
		AddParameter(comp, fx, fx->GetParameterByName(NULL, iso_output));
	}

	D3DXEFFECT_DESC	fdesc;
	fx->GetDesc(&fdesc);
	for (int i = 0; i < fdesc.Parameters; i++) {
		D3DXHANDLE			ph		= fx->GetParameter(NULL, i);
		if (fx->GetAnnotationByName(ph, "iso_output"))
			AddParameter(comp, fx, ph);
	}
}

ISO::TypeComposite *MakeParameterStruct(ID3DXEffect *fx, const char *technique) {
	ISO::TypeCompositeN<64>	builder(0);

	D3DXHANDLE		ht		= fx->GetTechniqueByName(technique);
	D3DXTECHNIQUE_DESC	tdesc;
	fx->GetTechniqueDesc(ht, &tdesc);

	D3DXPASS_DESC	pdesc;
	fx->GetPassDesc(fx->GetPass(ht, 0), &pdesc);

	int				num_passes		= tdesc.Passes;
	const DWORD		*vshader		= pdesc.pVertexShaderFunction;
	const DWORD		*pshader		= pdesc.pPixelShaderFunction;

	com_ptr<ID3DXConstantTable>	vct, pct;
	D3DXGetShaderConstantTable(vshader, &vct);
	D3DXGetShaderConstantTable(pshader, &pct);

	CollectParameters(builder, vct);
	CollectParameters(builder, pct);

	return builder.Duplicate();
}

//-----------------------------------------------------------------------------

class PlatformPCbase : Platform {
public:
	PlatformPCbase(const char *_platform) : Platform(_platform) {
		ISO::getdef<PCTexture>();
		ISO::getdef<PCSubMesh>();
//		ISO::getdef<PCSoundBuffer>();
		ISO_get_cast(Bitmap2PCTexture);
		ISO_get_cast(HDRBitmap2PCTexture);
		ISO_get_cast(PCTexture2Bitmap);
		ISO_get_cast(SubMesh2PCSubMesh);
	}
	type	Set() {
		Redirect<Texture,		PCTexture>();
		Redirect<SampleBuffer,	sample>();
		Redirect<SubMeshPtr,	PCSubMesh>();
		return PT_LITTLEENDIAN;
	}
	ISO_ptr<void>	ReadFX(tag id, istream_ref file, const filename *fn) {
		return ReadDX9FX(id, file, fn);
	}
};

class PlatformPC : PlatformPCbase {
public:
	PlatformPC() : PlatformPCbase("dx9") {}
} platform_pc;

//-----------------------------------------------------------------------------

struct DX9ShaderStage : ISO_openarray<rint32> {
	DX9ShaderStage(istream_ref file) : ISO_openarray<rint32>(file.length() / 4) { file.readbuff(*this, size() * 4); }
};
ISO_DEFUSER(DX9ShaderStage, ISO_openarray<rint32>);

class DX9ShaderFileHandler : public FileHandler {
public:
	virtual	const char*		GetDescription()	{ return "DX9 Shader Stage";	}

	virtual	int				Check(istream_ref file) {
		uint32	u;
		file.seek(0);
		return file.read(u) && (u == 0xffff0300 || u == 0xfffe0300) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	virtual ISO_ptr<void>	Read(tag id, istream_ref file)	{
		uint32	u;
		if (!file.read(u) || !(u == 0xffff0300 || u == 0xfffe0300))
			return ISO_NULL;

		file.seek_cur(-4);
		return ISO_ptr<DX9ShaderStage>(id, file);
	}

} dx9shaderstage;

