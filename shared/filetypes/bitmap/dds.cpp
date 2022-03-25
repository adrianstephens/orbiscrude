#include "codec/texels/dxt_compute.h"
#include "base/vector.h"
#include "bitmapfile.h"
#include "base/bits.h"
#include "systems/conversion/channeluse.h"

//-----------------------------------------------------------------------------
//	DDS D3D Texture format
//-----------------------------------------------------------------------------

//#undef HAS_COMPUTE
using namespace iso;

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((uint32(uint8(ch0))) | ((uint32(uint8(ch1))) << 8) | ((uint32(uint8(ch2))) << 16) | ((uint32(uint8(ch3))) << 24))
#endif

namespace DDS {

	enum class DXGI_FORMAT : uint32 {
		//components listed low to high
		UNKNOWN 					= 0,
		R32G32B32A32_TYPELESS 		= 1,
		R32G32B32A32_FLOAT 			= 2,
		R32G32B32A32_UINT 			= 3,
		R32G32B32A32_SINT 			= 4,
		R32G32B32_TYPELESS 			= 5,
		R32G32B32_FLOAT 			= 6,
		R32G32B32_UINT 				= 7,
		R32G32B32_SINT 				= 8,
		R16G16B16A16_TYPELESS 		= 9,
		R16G16B16A16_FLOAT 			= 10,
		R16G16B16A16_UNORM 			= 11,
		R16G16B16A16_UINT 			= 12,
		R16G16B16A16_SNORM 			= 13,
		R16G16B16A16_SINT 			= 14,
		R32G32_TYPELESS 			= 15,
		R32G32_FLOAT 				= 16,
		R32G32_UINT 				= 17,
		R32G32_SINT 				= 18,
		R32G8X24_TYPELESS 			= 19,
		D32_FLOAT_S8X24_UINT 		= 20,
		R32_FLOAT_X8X24_TYPELESS	= 21,
		X32_TYPELESS_G8X24_UINT		= 22,
		R10G10B10A2_TYPELESS 		= 23,
		R10G10B10A2_UNORM 			= 24,
		R10G10B10A2_UINT 			= 25,
		R11G11B10_FLOAT 			= 26,
		R8G8B8A8_TYPELESS 			= 27,
		R8G8B8A8_UNORM 				= 28,
		R8G8B8A8_UNORM_SRGB 		= 29,
		R8G8B8A8_UINT 				= 30,
		R8G8B8A8_SNORM 				= 31,
		R8G8B8A8_SINT 				= 32,
		R16G16_TYPELESS 			= 33,
		R16G16_FLOAT 				= 34,
		R16G16_UNORM 				= 35,
		R16G16_UINT 				= 36,
		R16G16_SNORM 				= 37,
		R16G16_SINT 				= 38,
		R32_TYPELESS 				= 39,
		D32_FLOAT 					= 40,
		R32_FLOAT 					= 41,
		R32_UINT 					= 42,
		R32_SINT 					= 43,
		R24G8_TYPELESS 				= 44,
		D24_UNORM_S8_UINT 			= 45,
		R24_UNORM_X8_TYPELESS 		= 46,
		X24_TYPELESS_G8_UINT 		= 47,
		R8G8_TYPELESS 				= 48,
		R8G8_UNORM 					= 49,
		R8G8_UINT 					= 50,
		R8G8_SNORM 					= 51,
		R8G8_SINT 					= 52,
		R16_TYPELESS 				= 53,
		R16_FLOAT 					= 54,
		D16_UNORM 					= 55,
		R16_UNORM 					= 56,
		R16_UINT 					= 57,
		R16_SNORM 					= 58,
		R16_SINT 					= 59,
		R8_TYPELESS 				= 60,
		R8_UNORM 					= 61,
		R8_UINT 					= 62,
		R8_SNORM 					= 63,
		R8_SINT 					= 64,
		A8_UNORM 					= 65,
		R1_UNORM 					= 66,
		R9G9B9E5_SHAREDEXP 			= 67,
		R8G8_B8G8_UNORM 			= 68,
		G8R8_G8B8_UNORM 			= 69,
		BC1_TYPELESS 				= 70,
		BC1_UNORM 					= 71,
		BC1_UNORM_SRGB 				= 72,
		BC2_TYPELESS 				= 73,
		BC2_UNORM 					= 74,
		BC2_UNORM_SRGB 				= 75,
		BC3_TYPELESS 				= 76,
		BC3_UNORM 					= 77,
		BC3_UNORM_SRGB 				= 78,
		BC4_TYPELESS 				= 79,
		BC4_UNORM 					= 80,
		BC4_SNORM 					= 81,
		BC5_TYPELESS 				= 82,
		BC5_UNORM 					= 83,
		BC5_SNORM 					= 84,
		B5G6R5_UNORM 				= 85,
		B5G5R5A1_UNORM 				= 86,
		B8G8R8A8_UNORM 				= 87,
		B8G8R8X8_UNORM 				= 88,
		R10G10B10_XR_BIAS_A2_UNORM	= 89,
		B8G8R8A8_TYPELESS 			= 90,
		B8G8R8A8_UNORM_SRGB 		= 91,
		B8G8R8X8_TYPELESS 			= 92,
		B8G8R8X8_UNORM_SRGB 		= 93,
		BC6H_TYPELESS 				= 94,
		BC6H_UF16 					= 95,
		BC6H_SF16 					= 96,
		BC7_TYPELESS 				= 97,
		BC7_UNORM 					= 98,
		BC7_UNORM_SRGB 				= 99,
	};

	enum class D3DFMT : uint32 {
		//components listed high to low
		UNKNOWN				= 0,

		R8G8B8				= 20,
		A8R8G8B8			= 21,
		X8R8G8B8			= 22,
		R5G6B5				= 23,
		X1R5G5B5			= 24,
		A1R5G5B5			= 25,
		A4R4G4B4			= 26,
		R3G3B2				= 27,
		A8					= 28,
		A8R3G3B2			= 29,
		X4R4G4B4			= 30,
		A2B10G10R10			= 31,
		A8B8G8R8			= 32,
		X8B8G8R8			= 33,
		G16R16				= 34,
		A2R10G10B10			= 35,
		A16B16G16R16		= 36,

		A8P8				= 40,
		P8					= 41,

		L8					= 50,
		A8L8				= 51,
		A4L4				= 52,

		V8U8				= 60,
		L6V5U5				= 61,
		X8L8V8U8			= 62,
		Q8W8V8U8			= 63,
		V16U16				= 64,
		A2W10V10U10			= 67,

		UYVY				= MAKEFOURCC('U', 'Y', 'V',	'Y'),
		R8G8_B8G8			= MAKEFOURCC('R', 'G', 'B',	'G'),
		YUY2				= MAKEFOURCC('Y', 'U', 'Y',	'2'),
		G8R8_G8B8			= MAKEFOURCC('G', 'R', 'G',	'B'),
		DXT1				= MAKEFOURCC('D', 'X', 'T',	'1'),
		DXT2				= MAKEFOURCC('D', 'X', 'T',	'2'),
		DXT3				= MAKEFOURCC('D', 'X', 'T',	'3'),
		DXT4				= MAKEFOURCC('D', 'X', 'T',	'4'),
		DXT5				= MAKEFOURCC('D', 'X', 'T',	'5'),

		BC4					= MAKEFOURCC('A', 'T', 'I',	'1'),
		BC5					= MAKEFOURCC('A', 'T', 'I',	'2'),

		DXT1b				= 0xC,
		DXT2b				= 0xE,
		DXT4b				= 0xF,

		D16_LOCKABLE		= 70,
		D32					= 71,
		D15S1				= 73,
		D24S8				= 75,
		D24X8				= 77,
		D24X4S4				= 79,
		D16					= 80,

		D32F_LOCKABLE		= 82,
		D24FS8				= 83,


		L16					= 81,

		VERTEXDATA			= 100,
		INDEX16				= 101,
		INDEX32				= 102,

		Q16W16V16U16		= 110,

		MULTI2_ARGB8		= MAKEFOURCC('M','E','T','1'),

		// Floating	point surface formats

		// s10e5 formats (16-bits per channel)
		R16F				= 111,
		G16R16F				= 112,
		A16B16G16R16F		= 113,

		// IEEE	s23e8 formats (32-bits per channel)
		R32F				= 114,
		G32R32F				= 115,
		A32B32G32R32F		= 116,

		CxV8U8				= 117,

		// my formats
		B8G8R8				= 20 | 0x80000000,

		DS10				= MAKEFOURCC('D','S','1','0'),
		BC6H_UF16			= 0x80000000|(uint32)DXGI_FORMAT::BC6H_UF16,
		BC6H_SF16			= 0x80000000|(uint32)DXGI_FORMAT::BC6H_SF16,
		BC7_UNORM 			= 0x80000000|(uint32)DXGI_FORMAT::BC7_UNORM,
		BC7_UNORM_SRGB 		= 0x80000000|(uint32)DXGI_FORMAT::BC7_UNORM_SRGB,
	};

	constexpr bool			is_dxgi(D3DFMT f)			{ return (uint32)f & 0x80000000; }
	constexpr bool			is_fourcc(D3DFMT f)			{ return (uint32)f >= 0x100; }
	constexpr DXGI_FORMAT	to_dxgi(D3DFMT f)			{ return DXGI_FORMAT((uint32)f & 0x7fffffff); }
	constexpr D3DFMT		from_dxgi(DXGI_FORMAT f)	{ return D3DFMT((uint32)f | 0x80000000); }

	enum {
		DDS_ALPHA		= 0x1,
		DDS_ALPHAONLY	= 0x2,
		DDS_FOURCC		= 0x4,
		DDS_RGB			= 0x40,
		DDS_RGBA		= DDS_RGB | DDS_ALPHA,
		DDS_YUV			= 0x200,
		DDS_LUMINANCE	= 0x20000,
	};

	struct PIXELFORMAT {
		uint32le	size;
		uint32le	flags;
		uint32le	fourCC;

		union {
			struct {
				uint32le	bits, m0, m1, m2, m3;// RGBA,  YUV, DuDvL, ---Z
			};
			struct {
				uint32le	_1, mask[4];
			};
			struct {
				uint32le	z_bits, s_bits, z_mask, s_mask;
			};
			struct {
				uint32le	_2;
				uint32le	operations;
				struct {
					uint16le flip_types, blit_types;
				} ms;
			};
		};

		PIXELFORMAT() {}
		constexpr PIXELFORMAT(D3DFMT f) : size(sizeof(*this)), flags(DDS_FOURCC), fourCC((uint32)f), bits(0), m0(0), m1(0), m2(0), m3(0) {}
		constexpr PIXELFORMAT(D3DFMT f, uint32 bits, uint32 lum) : size(sizeof(*this)), flags(DDS_RGB|DDS_LUMINANCE), fourCC((uint32)f), bits(bits), m0(lum), m1(0), m2(0), m3(0) {}
		constexpr PIXELFORMAT(D3DFMT f, uint32 bits, uint32 lum, uint32 a) : size(sizeof(*this)), flags(DDS_RGBA|DDS_LUMINANCE), fourCC((uint32)f), bits(bits), m0(lum), m1(0), m2(0), m3(a) {}
		constexpr PIXELFORMAT(D3DFMT f, uint32 bits, uint32 r, uint32 g, uint32 b) : size(sizeof(*this)), flags(DDS_RGB), fourCC((uint32)f), bits(bits), m0(r), m1(g), m2(b), m3(0) {}
		constexpr PIXELFORMAT(D3DFMT f, uint32 bits, uint32 r, uint32 g, uint32 b, uint32 a) : size(sizeof(*this)), flags(DDS_RGBA), fourCC((uint32)f), bits(bits), m0(r), m1(g), m2(b), m3(a) {}
	};


	enum DDS_FLAGS {
		DDSD_CAPS					= 0x00000001,
		DDSD_HEIGHT					= 0x00000002,
		DDSD_WIDTH					= 0x00000004,
		DDSD_PITCH					= 0x00000008,
		DDSD_PIXELFORMAT			= 0x00001000,
		DDSD_MIPMAPCOUNT			= 0x00020000,
		DDSD_LINEARSIZE				= 0x00080000,
		DDSD_DEPTH					= 0x00800000,
		DDSD_TEXTURE				= (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT),
		//nvidia/god-of-war extensions
		DDSD_NOT4VRAM0				= 0x00000010,
		DDSD_NOT4VRAM1				= 0x00000020,
		DDSD_SRGB					= 0x10000000,
		DDSD_CUTOUT_ALPHA			= 0x20000000,
		DDSD_ARBITRARY_ALPHA		= 0x40000000,
		DDSD_SWIZZLED				= 0x80000000,
	};

	enum DDS_CAPS {
		DDSCAPS_RESERVED1			= 0x00000001,
		DDSCAPS_ALPHA				= 0x00000002,
		DDSCAPS_BACKBUFFER			= 0x00000004,
		DDSCAPS_COMPLEX				= 0x00000008,
		DDSCAPS_FLIP				= 0x00000010,
		DDSCAPS_FRONTBUFFER			= 0x00000020,
		DDSCAPS_OFFSCREENPLAIN		= 0x00000040,
		DDSCAPS_OVERLAY				= 0x00000080,
		DDSCAPS_PALETTE				= 0x00000100,
		DDSCAPS_PRIMARYSURFACE		= 0x00000200,
		DDSCAPS_RESERVED3			= 0x00000400,
		DDSCAPS_PRIMARYSURFACELEFT	= 0x00000000,
		DDSCAPS_SYSTEMMEMORY		= 0x00000800,
		DDSCAPS_TEXTURE				= 0x00001000,
		DDSCAPS_3DDEVICE			= 0x00002000,
		DDSCAPS_VIDEOMEMORY			= 0x00004000,
		DDSCAPS_VISIBLE				= 0x00008000,
		DDSCAPS_WRITEONLY			= 0x00010000,
		DDSCAPS_ZBUFFER				= 0x00020000,
		DDSCAPS_OWNDC				= 0x00040000,
		DDSCAPS_LIVEVIDEO			= 0x00080000,
		DDSCAPS_HWCODEC				= 0x00100000,
		DDSCAPS_MODEX				= 0x00200000,
		DDSCAPS_MIPMAP				= 0x00400000,
		DDSCAPS_RESERVED2			= 0x00800000,
		DDSCAPS_ALLOCONLOAD			= 0x04000000,
		DDSCAPS_VIDEOPORT			= 0x08000000,
		DDSCAPS_LOCALVIDMEM			= 0x10000000,
		DDSCAPS_NONLOCALVIDMEM		= 0x20000000,
		DDSCAPS_STANDARDVGAMODE		= 0x40000000,
		DDSCAPS_OPTIMIZED			= 0x80000000,

		DDSCAPS2_RESERVED4			= 0x00000002,
		DDSCAPS2_HARDWAREDEINTERLACE= 0x00000000,
		DDSCAPS2_HINTDYNAMIC		= 0x00000004,
		DDSCAPS2_HINTSTATIC			= 0x00000008,
		DDSCAPS2_TEXTUREMANAGE		= 0x00000010,
		DDSCAPS2_RESERVED1			= 0x00000020,
		DDSCAPS2_RESERVED2			= 0x00000040,
		DDSCAPS2_OPAQUE				= 0x00000080,
		DDSCAPS2_HINTANTIALIASING	= 0x00000100,
		DDSCAPS2_CUBEMAP			= 0x00000200,
		DDSCAPS2_CUBEMAP_POSITIVEX	= 0x00000400,
		DDSCAPS2_CUBEMAP_NEGATIVEX	= 0x00000800,
		DDSCAPS2_CUBEMAP_POSITIVEY	= 0x00001000,
		DDSCAPS2_CUBEMAP_NEGATIVEY	= 0x00002000,
		DDSCAPS2_CUBEMAP_POSITIVEZ	= 0x00004000,
		DDSCAPS2_CUBEMAP_NEGATIVEZ	= 0x00008000,
		DDSCAPS2_CUBEMAP_ALL_FACES	= 0x0000FC00,
		DDSCAPS2_MIPMAPSUBLEVEL		= 0x00010000,
		DDSCAPS2_D3DTEXTUREMANAGE	= 0x00020000,
		DDSCAPS2_DONOTPERSIST		= 0x00040000,
		DDSCAPS2_STEREOSURFACELEFT	= 0x00080000,
		DDSCAPS2_VOLUME				= 0x00200000,
		DDSCAPS2_NOTUSERLOCKABLE	= 0x00400000,
		DDSCAPS2_POINTS				= 0x00800000,
		DDSCAPS2_RTPATCHES			= 0x01000000,
		DDSCAPS2_NPATCHES			= 0x02000000,
		DDSCAPS2_RESERVED3			= 0x04000000,
		DDSCAPS2_DISCARDBACKBUFFER	= 0x10000000,
		DDSCAPS2_ENABLEALPHACHANNEL	= 0x20000000,
		DDSCAPS2_EXTENDEDFORMATPRIMARY=0x40000000,
		DDSCAPS2_ADDITIONALPRIMARY	= 0x80000000,

		DDSCAPS3_MULTISAMPLE_MASK	= 0x0000001F,
		DDSCAPS3_MULTISAMPLE_QUALITY_MASK	= 0x000000E0,
		DDSCAPS3_MULTISAMPLE_QUALITY_SHIFT	= 5,
		DDSCAPS3_RESERVED1			= 0x00000100,
		DDSCAPS3_RESERVED2			= 0x00000200,
		DDSCAPS3_LIGHTWEIGHTMIPMAP	= 0x00000400,
		DDSCAPS3_AUTOGENMIPMAP		= 0x00000800,
		DDSCAPS3_DMAP				= 0x00001000,
		DDSCAPS3_CREATESHAREDRESOURCE=0x00002000,
		DDSCAPS3_READONLYRESOURCE	= 0x00004000,
		DDSCAPS3_OPENSHAREDRESOURCE	= 0x00008000,
	};

	struct HEADER: littleendian_types {
		uint32			size;
		uint32			flags;
		uint32			height;
		uint32			width;
		uint32			dwPitchOrLinearSize;
		uint32			dwDepth; // only if DDS::HEADER_FLAGS_VOLUME is set in flags
		uint32			dwMipMapCount;
		uint32			dwReserved1[11];
		PIXELFORMAT		ddspf;
		uint32			dwCaps;
		uint32			dwCaps2;
		uint32			dwCaps3;
		union {
			uint32		dwCaps4;
			uint32		dwVolumeDepth;
		};
		uint32			dwReserved2;
	};

#ifndef __d3d10_h__
	enum D3D10_RESOURCE_DIMENSION {
		D3D10_RESOURCE_DIMENSION_UNKNOWN 		= 0,
		D3D10_RESOURCE_DIMENSION_BUFFER 		= 1,
		D3D10_RESOURCE_DIMENSION_TEXTURE1D 		= 2,
		D3D10_RESOURCE_DIMENSION_TEXTURE2D 		= 3,
		D3D10_RESOURCE_DIMENSION_TEXTURE3D 		= 4,
	};
	enum D3D10_RESOURCE_MISC_FLAG {
		D3D10_RESOURCE_MISC_GENERATE_MIPS 		= 0x1L,
		D3D10_RESOURCE_MISC_SHARED 				= 0x2L,
		D3D10_RESOURCE_MISC_TEXTURECUBE 		= 0x4L,
		D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX	= 0x10L,
		D3D10_RESOURCE_MISC_GDI_COMPATIBLE 		= 0x20L,
	};
#endif
	struct HEADER_DS10: littleendian_types {
		DXGI_FORMAT		dxgiFormat;
		D3D10_RESOURCE_DIMENSION resourceDimension;
		uint32			miscFlag;
		uint32			arraySize;
		uint32			reserved;
	};

	typedef TexelFormat<16, 10,5, 5,5, 0,5, 15,1>	Col16_A1R5G5B5;
	typedef TexelFormat<16, 11,5, 5,6, 0,5>			Col16_R5G6B5;
	typedef TexelFormat<16,  8,4, 4,4, 0,4, 12,4>	Col16_A4R4G4B4;
	typedef TexelFormat<16,  0,8, 0,0, 0,0, 8,8>	Col16_A8P8;
	typedef TexelFormat<24, 16,8, 8,8, 0,8>			Col24_R8G8B8;
	typedef TexelFormat<24,  0,8, 8,8, 16,8>		Col24_B8G8R8;
	typedef TexelFormat<32, 16,8, 8,8, 0,8, 24,8>	Col32_A8R8G8B8;
	typedef TexelFormat<32,  0,8, 8,8, 16,8, 24,8>	Col32_A8B8G8R8;

	const PIXELFORMAT DDSPF_DXT1(D3DFMT::DXT1);
	const PIXELFORMAT DDSPF_DXT2(D3DFMT::DXT2);
	const PIXELFORMAT DDSPF_DXT3(D3DFMT::DXT3);
	const PIXELFORMAT DDSPF_DXT4(D3DFMT::DXT4);
	const PIXELFORMAT DDSPF_DXT5(D3DFMT::DXT5);
	const PIXELFORMAT DDSPF_R8G8_B8G8(D3DFMT::R8G8_B8G8);
	const PIXELFORMAT DDSPF_G8R8_G8B8(D3DFMT::G8R8_G8B8);
	const PIXELFORMAT DDSPF_DS10(D3DFMT::DS10);
	const PIXELFORMAT DDSPF_A8R8G8B8	(D3DFMT::A8R8G8B8,	32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	const PIXELFORMAT DDSPF_A8B8G8R8	(D3DFMT::A8B8G8R8,	32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	const PIXELFORMAT DDSPF_A1R5G5B5	(D3DFMT::A1R5G5B5,	16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000);
	const PIXELFORMAT DDSPF_A4R4G4B4	(D3DFMT::A4R4G4B4,	16, 0x0000f000, 0x000000f0, 0x0000000f, 0x0000f000);
	const PIXELFORMAT DDSPF_R8G8B8		(D3DFMT::R8G8B8,	24, 0x00ff0000, 0x0000ff00, 0x000000ff);
	const PIXELFORMAT DDSPF_R5G6B5		(D3DFMT::R5G6B5,	16, 0x0000f800, 0x000007e0, 0x0000001f);
	const PIXELFORMAT DDSPF_A8L8		(D3DFMT::A8L8,		16, 0x000000ff, 0x0000ff00);
	const PIXELFORMAT DDSPF_L8			(D3DFMT::L8,		8,	0x000000ff);

	DXGI_FORMAT	GetDXGIformat(D3DFMT fmt) {
		switch (fmt) {
			case D3DFMT::R8G8B8:			return DXGI_FORMAT::B8G8R8A8_UNORM;
			case D3DFMT::A8R8G8B8:			return DXGI_FORMAT::B8G8R8A8_UNORM;
			case D3DFMT::X8R8G8B8:			return DXGI_FORMAT::B8G8R8A8_UNORM;
			case D3DFMT::R5G6B5:			return DXGI_FORMAT::B5G6R5_UNORM;
			case D3DFMT::X1R5G5B5:			return DXGI_FORMAT::B5G5R5A1_UNORM;
			case D3DFMT::A1R5G5B5:			return DXGI_FORMAT::B5G5R5A1_UNORM;//??
//			case D3DFMT::A4R4G4B4:			return
//			case D3DFMT::R3G3B2:			return
			case D3DFMT::A8:				return DXGI_FORMAT::A8_UNORM;
//			case D3DFMT::A8R3G3B2:			return
//			case D3DFMT::X4R4G4B4:			return
			case D3DFMT::A2B10G10R10:		return DXGI_FORMAT::R10G10B10A2_UNORM;
			case D3DFMT::A8B8G8R8:			return DXGI_FORMAT::R8G8B8A8_UNORM;
			case D3DFMT::X8B8G8R8:			return DXGI_FORMAT::R8G8B8A8_UNORM;
			case D3DFMT::G16R16:			return DXGI_FORMAT::R16G16_UNORM;
			case D3DFMT::A2R10G10B10:		return DXGI_FORMAT::R10G10B10A2_UNORM;
			case D3DFMT::A16B16G16R16:		return DXGI_FORMAT::R16G16B16A16_UNORM;
			case D3DFMT::A8P8:				return DXGI_FORMAT::R8G8_UNORM;
			case D3DFMT::P8:				return DXGI_FORMAT::A8_UNORM;
			case D3DFMT::L8:				return DXGI_FORMAT::A8_UNORM;
			case D3DFMT::A8L8:				return DXGI_FORMAT::R8G8_UNORM;
//			case D3DFMT::A4L4:				return
//			case D3DFMT::V8U8:				return
//			case D3DFMT::L6V5U5:			return
//			case D3DFMT::X8L8V8U8:			return
//			case D3DFMT::Q8W8V8U8:			return
//			case D3DFMT::V16U16:			return
//			case D3DFMT::A2W10V10U10:		return
			case D3DFMT::R16F:				return DXGI_FORMAT::R16_FLOAT;
			case D3DFMT::G16R16F:			return DXGI_FORMAT::R16G16_FLOAT;
			case D3DFMT::R32F:				return DXGI_FORMAT::R32_FLOAT;
			case D3DFMT::G32R32F:			return DXGI_FORMAT::R32G32_FLOAT;
			case D3DFMT::DXT1:				return DXGI_FORMAT::BC1_UNORM;
			case D3DFMT::DXT2:
			case D3DFMT::DXT3:				return DXGI_FORMAT::BC2_UNORM;
			case D3DFMT::DXT4:
			case D3DFMT::DXT5:				return DXGI_FORMAT::BC2_UNORM;
			case D3DFMT::BC4:				return DXGI_FORMAT::BC4_UNORM;
			case D3DFMT::BC5:				return DXGI_FORMAT::BC5_UNORM;
			case D3DFMT::R8G8_B8G8:			return DXGI_FORMAT::G8R8_G8B8_UNORM;
			case D3DFMT::G8R8_G8B8:			return DXGI_FORMAT::R8G8_B8G8_UNORM;
			case D3DFMT::A16B16G16R16F:		return DXGI_FORMAT::R16G16B16A16_FLOAT;
			case D3DFMT::A32B32G32R32F:		return DXGI_FORMAT::R32G32B32A32_FLOAT;
			default:						return DXGI_FORMAT::UNKNOWN;
		}
	}

	vbitmap_format Getformat(PIXELFORMAT &ddspf) {
		switch ((D3DFMT)(uint32)ddspf.fourCC) {
			case D3DFMT::DXT1:
			case D3DFMT::DXT1b:			return vbitmap_format(5,6,5,1,		vbitmap_format::ALL_COMP);
			case D3DFMT::DXT2b:
			case D3DFMT::DXT2:
			case D3DFMT::DXT3:			return vbitmap_format(5,6,5,4,		vbitmap_format::RGB_COMP);
			case D3DFMT::DXT4:
			case D3DFMT::DXT4b:
			case D3DFMT::DXT5:			return vbitmap_format(5,6,5,8,		vbitmap_format::ALL_COMP);
			case D3DFMT::BC4:			return vbitmap_format(8,0,0,0,		vbitmap_format::ALL_COMP);
			case D3DFMT::BC5:			return vbitmap_format(8,8,0,0,		vbitmap_format::ALL_COMP);
			case D3DFMT::BC7_UNORM:		return vbitmap_format(5,6,5,5,		vbitmap_format::ALL_COMP);
			case D3DFMT::A8:			return vbitmap_format(0,0,0,8);
//			case D3DFMT::A1R5G5B5:		return vbitmap_format(5,5,5,1);
			case D3DFMT::A4R4G4B4:		return vbitmap_format(4,4,4,4);
			case D3DFMT::R5G6B5:		return vbitmap_format(5,6,5,0);
			case D3DFMT::A8B8G8R8:		return vbitmap_format(8,8,8,8);
			case D3DFMT::G16R16F:		return vbitmap_format(16,16,0,0,	vbitmap_format::FLOAT);
			case D3DFMT::A16B16G16R16F:	return vbitmap_format(16,16,16,16,	vbitmap_format::FLOAT);
			case D3DFMT::A32B32G32R32F:	return vbitmap_format(32,32,32,32,	vbitmap_format::FLOAT);
			case D3DFMT::R32F:			return vbitmap_format(32,0,0,0,		vbitmap_format::FLOAT);

			case D3DFMT::R8G8_B8G8:
			case D3DFMT::G8R8_G8B8:
				return 0;
			case D3DFMT::UYVY:
				return 0;
			default: {
				vbitmap_format	f;
				for (int i = 0; i < 4; i++)
					f.array[i] = count_bits(ddspf.mask[i]);
				//if (ddspf.flags & DDS_LUMINANCE)
				f.i |= vbitmap_format::GREY;	// also signifies non-standard
				return f;
			}
		}
	}
	vbitmap_format Getformat(DXGI_FORMAT dxgi) {
		switch (dxgi) {
			case DXGI_FORMAT::R32G32B32A32_TYPELESS:
			case DXGI_FORMAT::R32G32B32A32_FLOAT:
			case DXGI_FORMAT::R32G32B32A32_UINT:
			case DXGI_FORMAT::R32G32B32A32_SINT:	return vbitmap_format(32,32,32,32);
			case DXGI_FORMAT::R32G32B32_TYPELESS:
			case DXGI_FORMAT::R32G32B32_FLOAT:
			case DXGI_FORMAT::R32G32B32_UINT:
			case DXGI_FORMAT::R32G32B32_SINT:		return vbitmap_format(32,32,32,0);

			case DXGI_FORMAT::R16G16B16A16_TYPELESS:
			case DXGI_FORMAT::R16G16B16A16_FLOAT:
			case DXGI_FORMAT::R16G16B16A16_UNORM:
			case DXGI_FORMAT::R16G16B16A16_UINT:
			case DXGI_FORMAT::R16G16B16A16_SNORM:
			case DXGI_FORMAT::R16G16B16A16_SINT:	return vbitmap_format(16,16,16,16);

			case DXGI_FORMAT::R32G32_TYPELESS:
			case DXGI_FORMAT::R32G32_FLOAT:
			case DXGI_FORMAT::R32G32_UINT:
			case DXGI_FORMAT::R32G32_SINT:			return vbitmap_format(32,32,0,0);

			case DXGI_FORMAT::R32G8X24_TYPELESS:
			case DXGI_FORMAT::D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT::R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT::X32_TYPELESS_G8X24_UINT: return vbitmap_format(32,8,24,0);

			case DXGI_FORMAT::R10G10B10A2_TYPELESS:
			case DXGI_FORMAT::R10G10B10A2_UNORM:
			case DXGI_FORMAT::R10G10B10A2_UINT:		return vbitmap_format(10,10,10,2);
			case DXGI_FORMAT::R11G11B10_FLOAT:		return vbitmap_format(11,11,10,0);

			case DXGI_FORMAT::R8G8B8A8_TYPELESS:
			case DXGI_FORMAT::R8G8B8A8_UNORM:
			case DXGI_FORMAT::R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT::R8G8B8A8_UINT:
			case DXGI_FORMAT::R8G8B8A8_SNORM:
			case DXGI_FORMAT::R8G8B8A8_SINT:		return vbitmap_format(8,8,8,8);

			case DXGI_FORMAT::R16G16_TYPELESS:
			case DXGI_FORMAT::R16G16_FLOAT:
			case DXGI_FORMAT::R16G16_UNORM:
			case DXGI_FORMAT::R16G16_UINT:
			case DXGI_FORMAT::R16G16_SNORM:
			case DXGI_FORMAT::R16G16_SINT:			return vbitmap_format(16,16,0,0);

			case DXGI_FORMAT::R32_TYPELESS:
			case DXGI_FORMAT::D32_FLOAT:
			case DXGI_FORMAT::R32_FLOAT:
			case DXGI_FORMAT::R32_UINT:
			case DXGI_FORMAT::R32_SINT:				return vbitmap_format(32,0,0,0);

			case DXGI_FORMAT::R24G8_TYPELESS:
			case DXGI_FORMAT::D24_UNORM_S8_UINT:
			case DXGI_FORMAT::R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT::X24_TYPELESS_G8_UINT:	return vbitmap_format(24,8,0,0);

			case DXGI_FORMAT::R8G8_TYPELESS:
			case DXGI_FORMAT::R8G8_UNORM:
			case DXGI_FORMAT::R8G8_UINT:
			case DXGI_FORMAT::R8G8_SNORM:
			case DXGI_FORMAT::R8G8_SINT:			return vbitmap_format(8,8,0,0);

			case DXGI_FORMAT::R16_TYPELESS:
			case DXGI_FORMAT::R16_FLOAT:
			case DXGI_FORMAT::D16_UNORM:
			case DXGI_FORMAT::R16_UNORM:
			case DXGI_FORMAT::R16_UINT:
			case DXGI_FORMAT::R16_SNORM:
			case DXGI_FORMAT::R16_SINT:				return vbitmap_format(16,0,0,0);

			case DXGI_FORMAT::R8_TYPELESS:
			case DXGI_FORMAT::R8_UNORM:
			case DXGI_FORMAT::R8_UINT:
			case DXGI_FORMAT::R8_SNORM:
			case DXGI_FORMAT::R8_SINT:				return vbitmap_format(8,0,0,0);

			case DXGI_FORMAT::A8_UNORM:				return vbitmap_format(0,0,0,8);
			case DXGI_FORMAT::R1_UNORM:				return vbitmap_format(1,0,0,0);

			case DXGI_FORMAT::R9G9B9E5_SHAREDEXP:	return vbitmap_format(9,9,9,0,		vbitmap_format::FLOAT);
			case DXGI_FORMAT::R8G8_B8G8_UNORM:
			case DXGI_FORMAT::G8R8_G8B8_UNORM:

			case DXGI_FORMAT::BC1_TYPELESS:
			case DXGI_FORMAT::BC1_UNORM:
			case DXGI_FORMAT::BC1_UNORM_SRGB:		return vbitmap_format(5,6,5,1,		vbitmap_format::ALL_COMP);
			case DXGI_FORMAT::BC2_TYPELESS:
			case DXGI_FORMAT::BC2_UNORM:
			case DXGI_FORMAT::BC2_UNORM_SRGB:		return vbitmap_format(5,6,5,4,		vbitmap_format::ALL_COMP);
			case DXGI_FORMAT::BC3_TYPELESS:
			case DXGI_FORMAT::BC3_UNORM:
			case DXGI_FORMAT::BC3_UNORM_SRGB:		return vbitmap_format(5,6,5,4,		vbitmap_format::RGB_COMP);
			case DXGI_FORMAT::BC4_TYPELESS:
			case DXGI_FORMAT::BC4_UNORM:
			case DXGI_FORMAT::BC4_SNORM:			return vbitmap_format(8,0,0,0,		vbitmap_format::R_COMP);
			case DXGI_FORMAT::BC5_TYPELESS:
			case DXGI_FORMAT::BC5_UNORM:
			case DXGI_FORMAT::BC5_SNORM:			return vbitmap_format(8,8,0,0,		vbitmap_format::RG_COMP);

			case DXGI_FORMAT::B5G6R5_UNORM:			return vbitmap_format(5,6,5,0);
			case DXGI_FORMAT::B5G5R5A1_UNORM:		return vbitmap_format(5,5,5,1);
			case DXGI_FORMAT::B8G8R8A8_UNORM:		return vbitmap_format(8,8,8,8);
			case DXGI_FORMAT::B8G8R8X8_UNORM:		return vbitmap_format(8,8,8,8);
			case DXGI_FORMAT::R10G10B10_XR_BIAS_A2_UNORM:
			case DXGI_FORMAT::B8G8R8A8_TYPELESS:
			case DXGI_FORMAT::B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT::B8G8R8X8_TYPELESS:
			case DXGI_FORMAT::B8G8R8X8_UNORM_SRGB:

			case DXGI_FORMAT::BC6H_TYPELESS:
			case DXGI_FORMAT::BC6H_UF16:			return vbitmap_format(16,16,16,0, vbitmap_format::RGB_COMP | vbitmap_format::FLOAT);
			case DXGI_FORMAT::BC6H_SF16:			return vbitmap_format(16,16,16,0, vbitmap_format::RGB_COMP | vbitmap_format::FLOAT | vbitmap_format::SIGNED);
			case DXGI_FORMAT::BC7_TYPELESS:
			case DXGI_FORMAT::BC7_UNORM:
			case DXGI_FORMAT::BC7_UNORM_SRGB:		return vbitmap_format(8,8,8,8, vbitmap_format::ALL_COMP);
			default:								return vbitmap_format();
		}
	}

	uint32 CalcSurfaceStride(uint32 width, const PIXELFORMAT &ddspf) {
		DDS::D3DFMT	fmt = (DDS::D3DFMT)ddspf.fourCC;
		switch (fmt) {
			case D3DFMT::DXT1:
			case D3DFMT::BC4:
				return ((width + 3) / 4) * 8;
			case D3DFMT::DXT2:
			case D3DFMT::DXT3:
			case D3DFMT::DXT4:
			case D3DFMT::DXT5:
			case D3DFMT::BC5:
				return ((width + 3) / 4) * 16;
			case D3DFMT::R8G8_B8G8:
			case D3DFMT::G8R8_G8B8:
			case D3DFMT::UYVY:
				return (width + 1) / 2 * 4;
			case D3DFMT::A8R8G8B8:
			case D3DFMT::A8B8G8R8:
				return 4 * width;
			case D3DFMT::B8G8R8:
			case D3DFMT::R8G8B8:
				return 3 * width;
			case D3DFMT::A8P8:
			case D3DFMT::A8L8:
			case D3DFMT::A1R5G5B5:
			case D3DFMT::A4R4G4B4:
			case D3DFMT::R5G6B5:
				return 2 * width;
			case D3DFMT::L8:
			case D3DFMT::P8:
				return width;

			case D3DFMT::BC6H_UF16:
			case D3DFMT::BC6H_SF16:
			case D3DFMT::BC7_UNORM:
			case D3DFMT::BC7_UNORM_SRGB:
				width = (width + 3) / 4;
				if (fmt <= from_dxgi(DDS::DXGI_FORMAT::BC1_UNORM_SRGB))
					return width * 8;
				return width * 16;
			default: {
				if (is_dxgi(fmt))
					return (Getformat(to_dxgi(fmt)).bits() + 7) / 8 * width;
				return (ddspf.bits + 7) / 8 * width;
			}
		}
	}

	uint32 BlockWidth(const D3DFMT fmt) {
		switch (fmt) {
			case D3DFMT::DXT1:
			case D3DFMT::DXT2:
			case D3DFMT::DXT3:
			case D3DFMT::DXT4:
			case D3DFMT::DXT5:
			case D3DFMT::BC4:
			case D3DFMT::BC5:
			case D3DFMT::BC6H_UF16:
			case D3DFMT::BC6H_SF16:
			case D3DFMT::BC7_UNORM:
			case D3DFMT::BC7_UNORM_SRGB:
				return 4;
			default:
				return 1;
		}
	}

	uint32 CalcSurfaceSize(uint32 width, uint32 height, const PIXELFORMAT &ddspf) {
		uint32 block = BlockWidth((DDS::D3DFMT)(uint32)ddspf.fourCC);
		return CalcSurfaceStride(width, ddspf) * ((height + block - 1) / block);
	}

	uint32 CalcMipsSize(uint32 width, uint32 height, uint32 depth, const PIXELFORMAT &ddspf, uint32 num_mips, bool volume) {
		uint32	total = 0;
		for (uint32 i = 0; i < num_mips; i++)
			total += CalcSurfaceSize(max(width >> i, 1), max(height >> i, 1), ddspf) * (volume ? max(depth >> i, 1) : depth);
		return total;
	}

} //namespace DDS

//using namespace DDS;

struct DDSFileHandler: public BitmapFileHandler {
	const char*		GetExt() override { return "dds";					}
	const char*		GetDescription() override { return "DirectDraw Surface";	}
	int				Check(istream_ref file) override { file.seek(0); return file.get<uint32>() == "DDS "_u32 ? CHECK_PROBABLE: CHECK_DEFINITE_NO; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override;

#ifdef CROSS_PLATFORM
	static bool				Write(bitmap &bm, ostream_ref file, DDS::D3DFMT fmt);
	static bool				Write(HDRbitmap &bm, ostream_ref file, DDS::D3DFMT fmt);
	static bool				Write(vbitmap &bm, ostream_ref file, DDS::D3DFMT fmt, uint32 flags);

	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
#endif
} dds;

#ifdef CROSS_PLATFORM
bool WriteDDS(vbitmap &bm, ostream_ref file, uint32 flags) {
	return DDSFileHandler::Write(bm, file, DDS::D3DFMT::UNKNOWN, flags);
}
#endif

vbitmap_format GetDDSFormat(void *dds_header) {
	return DDS::Getformat(((DDS::HEADER*)((uint32*)dds_header + 1))->ddspf);
}

void PS3_DeSwizzle(uint32 width, uint32 height, uint32 depth, void *srce, void *dest, uint32 bpp) {
	if (bpp > 4) {
		if (bpp == 8)
			width *= 2;
		else
			width *= 4;
		bpp = 4;
	}

	uint32	log2width	= iso::log2(width);
	uint32	log2height	= iso::log2(height);
	uint32	log2depth	= iso::log2(depth);

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

template<typename S, typename D> bool CopyPixels(const block<D, 2> &rect, S *srce, uint32 stride) {
	int		width	= rect.template size<1>(), height = rect.template size<2>();
	for (int y = 0; y < height; y++, srce = (S*)((uint8*)srce + stride))
		copy_n((S*)srce, rect[y].begin(), width);
	return true;
}

void SwitchRG(const block<ISO_rgba, 2> &block) {
	for (auto i = block.begin(), i1 = block.end(); i != i1; ++i) {
		for (auto j = i.begin(), j1 = i.end(); j != j1; ++j)
			swap(j->r, j->b);
	}
}

template<typename S, typename D> bool CopyBlocks(const block<D, 2> &rect, S *srce, uint32 stride) {
	int		width	= rect.template size<1>(), height = rect.template size<2>();
	for (int y = 0; y < height; y += 4, srce = (S*)((uint8*)srce + stride)) {
		S	*p = srce;
		for (int x = 0; x < width; x += 4)
			p++->Decode(rect.template sub<1>(x, 4).template sub<2>(y, 4));
	}
	//SwitchRG(rect);
	return true;
}

bool LoadSurface(const block<ISO_rgba, 2> &rect, void *srce, uint32 stride, const DDS::PIXELFORMAT &ddspf) {
	using namespace DDS;

	int		width	= rect.size<1>(), height = rect.size<2>();

	switch ((D3DFMT)ddspf.fourCC) {
		case D3DFMT::DXT1:
			return CopyBlocks(rect, (DXT1rec*)srce, stride);

		case D3DFMT::DXT2:
		case D3DFMT::DXT3:
			return CopyBlocks(rect, (DXT23rec*)srce, stride);

		case D3DFMT::DXT4:
		case D3DFMT::DXT5:
			return CopyBlocks(rect, (DXT45rec*)srce, stride);

		case D3DFMT::BC4:
			return CopyBlocks(rect, (BC<4>*)srce, stride);

		case D3DFMT::BC5:
			return CopyBlocks(rect, (BC<5>*)srce, stride);

		case D3DFMT::BC7_UNORM:
			return CopyBlocks(rect, (BC<7>*)srce, stride);

		case D3DFMT::R8G8_B8G8:
			for (int y = 0; y < height; y++, srce = (uint8*)srce + stride) {
				ISO_rgba	*dest = rect[y];
				for (int x = 0; x < width; x += 2) {
					dest[x + 0].r	= dest[x + 1].r = ((uint8*)srce)[x * 2 + 0];
					dest[x + 0].b	= dest[x + 1].b = ((uint8*)srce)[x * 2 + 2];
					dest[x + 0].g	= ((uint8*)srce)[x * 2 + 1];
					dest[x + 1].g	= ((uint8*)srce)[x * 2 + 3];
				}
			}
			return true;

		case D3DFMT::G8R8_G8B8:
			for (int y = 0; y < height; y++, srce = (uint8*)srce + stride) {
				ISO_rgba	*dest = rect[y];
				for (int x = 0; x < width; x += 2) {
					dest[x + 0].r	= dest[x + 1].r = ((uint8*)srce)[x * 2 + 1];
					dest[x + 0].b	= dest[x + 1].b = ((uint8*)srce)[x * 2 + 3];
					dest[x + 0].g	= ((uint8*)srce)[x * 2 + 0];
					dest[x + 1].g	= ((uint8*)srce)[x * 2 + 2];
				}
			}
			return true;

		case D3DFMT::UYVY:
			for (int y = 0; y < height; y++, srce = (uint8*)srce + stride) {
				ISO_rgba	*dest = rect[y];
				struct uyvy { uint8 y0, v, y1, u; } *p = (uyvy*)srce;
				for (int x = 0; x < width; x += 2, p++) {
					dest[x + 0]	= ISO_rgba::YUV(p->y0, p->u, p->v);
					dest[x + 1]	= ISO_rgba::YUV(p->y1, p->u, p->v);
				}
			}
			return true;

		case D3DFMT::A8R8G8B8:
			return CopyPixels(rect, (Texel<Col32_A8R8G8B8>*)srce, stride);

		case D3DFMT::B8G8R8:
			return CopyPixels(rect, (Texel<Col32_A8R8G8B8>*)srce, stride);
//			for (int y = 0; y < height; y++, srce = (uint8*)srce + stride)
//				copy_n((Texel<Col24_B8G8R8>*)srce, rect[y].begin(), width);
//			return true;

		case D3DFMT::R8G8B8:
			return CopyPixels(rect, (Texel<Col24_R8G8B8>*)srce, stride);

		case D3DFMT::A8P8:
		case D3DFMT::A8L8:
			return CopyPixels(rect, (Texel<Col16_A8P8>*)srce, stride);

		case D3DFMT::A1R5G5B5:
			return CopyPixels(rect, (Texel<Col16_A1R5G5B5>*)srce, stride);

		case D3DFMT::A4R4G4B4:
			return CopyPixels(rect, (Texel<Col16_A4R4G4B4>*)srce, stride);

		case D3DFMT::R5G6B5:
			return CopyPixels(rect, (Texel<Col16_R5G6B5>*)srce, stride);

		case D3DFMT::L8:
		case D3DFMT::P8:
			return CopyPixels(rect, (uint8*)srce, stride);

		case D3DFMT::A32B32G32R32F:
			return CopyPixels(rect, (HDRpixel*)srce, stride);

		default: {
			uint32	rmask	= ddspf.mask[0];
			uint32	gmask	= ddspf.mask[1];
			uint32	bmask	= ddspf.mask[2];
			uint32	amask	= ddspf.mask[3];

			uint32	rshift	= highest_set_index(rmask)	- 7;
			uint32	gshift	= highest_set_index(gmask)	- 7;
			uint32	bshift	= highest_set_index(bmask)	- 7;
			uint32	ashift	= highest_set_index(amask)	- 7;

			int		bpp		= (ddspf.bits + 7) / 8;
			for (int y = 0; y < height; y++, srce = (uint8*)srce + stride) {
				ISO_rgba	*dest = rect[y];
				for (int x = 0; x < width; x++) {
					uint32	p	= *(uint32*)((char*)srce + x * bpp);
					dest[x].r = (p & rmask) >> rshift;
					dest[x].g = (p & gmask) >> gshift;
					dest[x].b = (p & bmask) >> bshift;
					dest[x].a = (p & amask) >> ashift;
				}
			}
			return true;
		}
	}
	return true;
}

inline int	sign_extend_mask(uint32 x, uint32 m, uint32 s)	{ return (x & m) - ((x & s) << 1); }

bool LoadSurface(const block<HDRpixel, 2> &rect, void *srce, uint32 stride, const DDS::PIXELFORMAT &ddspf) {
	using namespace DDS;

	int				width	= rect.size<1>(), height = rect.size<2>();

	switch ((D3DFMT)ddspf.fourCC) {
		case D3DFMT::A32B32G32R32F:
			return CopyPixels(rect, (float4p*)srce, stride);

		case D3DFMT::A16B16G16R16:
			return CopyPixels(rect, (field_vec<unorm16,unorm16,unorm16,unorm16>*)srce, stride);

		case D3DFMT::BC6H_UF16:
		case D3DFMT::BC6H_SF16:
			return CopyBlocks(rect, (BC<6>*)srce, stride);

		default: {
			uint32	rmask	= ddspf.mask[0];
			uint32	gmask	= ddspf.mask[1];
			uint32	bmask	= ddspf.mask[2];
			uint32	amask	= ddspf.mask[3];

			int		bpp		= (ddspf.bits + 7) / 8;

			if (ddspf.flags & 0x80000) {
				float	rscale	= rmask ? 0.5f / rmask: 0.f;
				float	gscale	= gmask ? 0.5f / gmask: 0.f;
				float	bscale	= bmask ? 0.5f / bmask: 0.f;
				float	ascale	= amask ? 0.5f / amask: 0.f;

				uint32	rsign	= highest_set(rmask);
				uint32	gsign	= highest_set(gmask);
				uint32	bsign	= highest_set(bmask);
				uint32	asign	= highest_set(amask);

				for (int y = 0; y < height; y++, srce = (uint8*)srce + stride) {
					HDRpixel	*dest = rect[y];
					for (int x = 0; x < width; x++) {
						uint32	p	= *(uint32*)((char*)srce + x * bpp);
						dest[x].r = sign_extend_mask(p, rmask, rsign) * rscale;
						dest[x].g = sign_extend_mask(p, gmask, gsign) * gscale;
						dest[x].b = sign_extend_mask(p, bmask, bsign) * bscale;
						dest[x].a = sign_extend_mask(p, amask, asign) * ascale;
					}
				}
			} else {
				float	rscale	= rmask ? 1.0f / rmask: 0.f;
				float	gscale	= gmask ? 1.0f / gmask: 0.f;
				float	bscale	= bmask ? 1.0f / bmask: 0.f;
				float	ascale	= amask ? 1.0f / amask: 0.f;

				for (int y = 0; y < height; y++, srce = (uint8*)srce + stride) {
					HDRpixel	*dest = rect[y];
					for (int x = 0; x < width; x++) {
						uint32	p	= *(uint32*)((char*)srce + x * bpp);
						dest[x].r = (p & rmask) * rscale;
						dest[x].g = (p & gmask) * gscale;
						dest[x].b = (p & bmask) * bscale;
						dest[x].a = (p & amask) * ascale;
					}
				}
			}
			return true;
		}
	}
}

struct vbitmap_DDS: vbitmap {

	DDS::PIXELFORMAT	ddspf;
	DDS::HEADER_DS10	ddsh10;
	malloc_block		data;

	bool	get(const vbitmap_loc &in, vbitmap_format fmt, void *dest, uint32 stride, uint32 width, uint32 height) {
		bool	volume		= !!(flags & BMF_VOLUME);
		uint32	mywidth		= vbitmap::width;
		uint32	myheight	= vbitmap::height;
		uint32	offset		= 0;

		if (in.z && volume)
			offset = CalcMipsSize(mywidth, myheight, depth, ddspf, mips + 1, volume) * in.z;

		if (in.m) {
			offset	+= CalcMipsSize(mywidth, myheight, depth, ddspf, in.m, volume);
			mywidth		= max(mywidth >> in.m, 1);
			myheight	= max(myheight >> in.m, 1);
		}

		if (in.z && !volume)
			offset += CalcSurfaceSize(mywidth, myheight, ddspf) * in.z;

		uint32	mystride	= CalcSurfaceStride(mywidth, ddspf);
		uint32	blockwidth	= BlockWidth((DDS::D3DFMT)(uint32)ddspf.fourCC);
		offset	+= in.x + in.y / blockwidth * mystride;

		if (!fmt || fmt == vbitmap::format) {
			uint32	dest_width = fmt ? (width * fmt.bits() + 7) / 8: width;
			for (int y = 0; y < height; y++, offset += mystride, dest = (uint8*)dest + stride)
				memcpy(dest, (uint8*)data + offset, dest_width);
			return true;

		} else if (fmt.is<ISO_rgba>()) {
			return LoadSurface(make_strided_block((ISO_rgba*)dest, width, stride, height), (uint8*)data + offset, mystride, ddspf);

		} else if (fmt.is<HDRpixel>()) {
			return LoadSurface(make_strided_block((HDRpixel*)dest, width, stride, height), (uint8*)data + offset, mystride, ddspf);

		} else {
			return false;
		}
	}

	vbitmap_DDS(istream_ref file): vbitmap(this) {
		using namespace DDS;

		uint32			ddsh_size	= file.get<uint32le>();
		malloc_block	header(ddsh_size);
		DDS::HEADER		*ddsh		= header;
		ddsh->size					= ddsh_size;
		file.readbuff((char*)ddsh + sizeof(uint32), ddsh_size - sizeof(uint32));

		ddspf	= ddsh->ddspf;
		width	= ddsh->width;
		height	= ddsh->height;
		depth	= ddsh->flags & DDSD_DEPTH ? ddsh->dwDepth: 1;
		format	= Getformat(ddsh->ddspf);
		mips	= ddsh->dwMipMapCount ? ddsh->dwMipMapCount - 1: 0;

		if ((D3DFMT)(uint32)ddspf.fourCC == D3DFMT::DS10) {
			file.read(ddsh10);
			if (ddsh10.dxgiFormat != DDS::DXGI_FORMAT::UNKNOWN) {
				ddspf.fourCC = (uint32)ddsh10.dxgiFormat | 0x80000000;
				format = Getformat(ddsh10.dxgiFormat);
				int		total = 0;
				for (int i = 0; i < 4; i++) {
					int		n		= format.array[i] & vbitmap_format::SIZE_MASK;
					ddspf.mask[i]	= bits(n, total);
					total	+= n;
				}
				ddspf.bits = total;

			}
			switch (ddsh10.resourceDimension) {
				case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
					break;
				case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
					depth = ddsh10.arraySize;
					break;
				default:
					break;
			}
		} else {
			clear(ddsh10);
		}

		if (ddsh->dwCaps2 & DDSCAPS2_CUBEMAP_ALL_FACES) {
			int		cubeflags	= ddsh->dwCaps2;
			depth = !!(cubeflags & DDSCAPS2_CUBEMAP_POSITIVEX)
				+	!!(cubeflags & DDSCAPS2_CUBEMAP_NEGATIVEX)
				+	!!(cubeflags & DDSCAPS2_CUBEMAP_POSITIVEY)
				+	!!(cubeflags & DDSCAPS2_CUBEMAP_NEGATIVEY)
				+	!!(cubeflags & DDSCAPS2_CUBEMAP_POSITIVEZ)
				+	!!(cubeflags & DDSCAPS2_CUBEMAP_NEGATIVEZ);
			flags	|= BMF_CUBE;
		}

		bool	volume = !!(ddsh->dwCaps2 & DDSCAPS2_VOLUME);
		if (volume)
			flags |= BMF_VOLUME;
		uint32 total = CalcMipsSize(width, height, depth, ddspf, mips + 1, volume);

		data.create(total);
		file.readbuff(data, total);

		if (ddsh->flags & DDSD_SWIZZLED) {
			uint32 w = width, h = height, b;
			switch ((D3DFMT)(uint32)ddspf.fourCC) {
				case D3DFMT::DXT1:
					b	= 8;
					w	= (w + 3) / 4;
					h	= (h + 3) / 4;
					return;
				case D3DFMT::DXT2:
				case D3DFMT::DXT3:
				case D3DFMT::DXT4:
				case D3DFMT::DXT5:
					b	= 16;
					w	= (w + 3) / 4;
					h	= (h + 3) / 4;
					return;
				case D3DFMT::R8G8_B8G8:
				case D3DFMT::G8R8_G8B8:
				case D3DFMT::UYVY:
					b	= 4;
					w	= (w + 1) / 2;
					break;
				case D3DFMT::A8R8G8B8:
					b	= 4;
					break;
				case D3DFMT::B8G8R8:
				case D3DFMT::R8G8B8:
					b	= 3;
					break;
				case D3DFMT::A8P8:
				case D3DFMT::A8L8:
				case D3DFMT::A1R5G5B5:
				case D3DFMT::A4R4G4B4:
				case D3DFMT::R5G6B5:
					b	= 2;
					break;
				case D3DFMT::L8:
				case D3DFMT::P8:
					b	= 1;
					break;
				default:
					b = (ddspf.bits + 7) / 8;
					break;
			}

			malloc_block	deswiz(total);
			uint8	*srce = data, *dest = deswiz;
			for (uint32 i = 0; i <= mips; i++) {
				PS3_DeSwizzle(w, h, depth, srce, dest, b);
				uint32	size = w * h * b;
				srce += size;
				dest += size;
				w = max(w >> 1, 1);
				h = max(h >> 1, 1);
			}
			data	= move(deswiz);
		}
	}
	void*	get_raw(uint32 plane, vbitmap_format *fmt, uint32 *stride, uint32 *width, uint32 *height) { return 0; }
};

ISO_DEFSAME(vbitmap_DDS, vbitmap);

ISO_ptr<void> DDSFileHandler::Read(tag id, istream_ref file) {
	if (file.get<uint32>() != "DDS "_u32)
		return ISO_NULL;

	return ISO_ptr<vbitmap_DDS>(id, file);
}

#ifdef CROSS_PLATFORM

bool SaveSurface(const block<ISO_rgba, 2> &rect, ostream_ref file, DDS::D3DFMT fmt) {
	using namespace DDS;

	int		scan	= 0;
	int		width	= rect.size<1>(), height = rect.size<2>();

	switch (fmt) {
		case D3DFMT::DXT1: {
			auto	dest = make_auto_block<DXT1rec>(div_round_up(width, 4), div_round_up(height, 4));
			copy(rect, dest.get());
			return file.write(dest);
		}
		case D3DFMT::DXT2:
		case D3DFMT::DXT3: {
			auto	dest = make_auto_block<DXT23rec>(div_round_up(width, 4), div_round_up(height, 4));
			copy(rect, dest.get());
			return file.write(dest);
		}
		case D3DFMT::DXT4:
		case D3DFMT::DXT5: {
			auto	dest = make_auto_block<DXT45rec>(div_round_up(width, 4), div_round_up(height, 4));
			copy(rect, dest.get());
			return file.write(dest);
		}
		case D3DFMT::BC4: {
			BC<4>	rec;
			for (int y = 0; y < height; y += 4) {
				for (int x = 0; x < width; x += 4) {
					rec.Encode(rect.sub<1>(x, 4).sub<2>(y, 4));
					file.write(rec);
				}
			}
			return true;
		}
		case D3DFMT::BC5: {
			BC<5>	rec;
			for (int y = 0; y < height; y += 4) {
				for (int x = 0; x < width; x += 4) {
					rec.Encode(rect.sub<1>(x, 4).sub<2>(y, 4));
					file.write(rec);
				}
			}
			return true;
		}
		case D3DFMT::BC7_UNORM: {
			BC<7>	rec;
			for (int y = 0; y < height; y += 4) {
				for (int x = 0; x < width; x += 4) {
					rec.Encode(rect.sub<1>(x, 4).sub<2>(y, 4));
					file.write(rec);
				}
			}
			return true;
		}
		case D3DFMT::BC6H_UF16:
		case D3DFMT::BC6H_SF16: {
			auto	srce = make_auto_block<HDRpixel>(width, height);
			copy(rect, srce.get());
			auto	dest = make_auto_block<BC<6>>(div_round_up(width, 4), div_round_up(height, 4));
			copy(make_const(srce.get()), dest.get());
			return file.write(dest);
		}

		case D3DFMT::R8G8_B8G8:
		case D3DFMT::G8R8_G8B8:	scan = (width + 1) / 2 * 4; break;
		case D3DFMT::A8B8G8R8:
		case D3DFMT::A8R8G8B8:	scan = width * 4;			break;
		case D3DFMT::R8G8B8:	scan = width * 3;			break;
		case D3DFMT::A8P8:
		case D3DFMT::A8L8:
		case D3DFMT::A1R5G5B5:
		case D3DFMT::A4R4G4B4:
		case D3DFMT::R5G6B5:	scan = width * 2;			break;
		case D3DFMT::L8:
		case D3DFMT::P8:		scan = width;				break;
		default:
			return false;
	};

	if (scan) {
		malloc_block	dest(scan + 4);
		for (int y = 0; y < height; y++) {
			const ISO_rgba	*srce = rect[y];
			switch (fmt) {
				case D3DFMT::A8B8G8R8:	copy_n(srce, (Texel<Col32_A8B8G8R8>*)dest, width); break;
				case D3DFMT::A8R8G8B8:	copy_n(srce, (Texel<Col32_A8R8G8B8>*)dest, width); break;
				case D3DFMT::R8G8B8:	copy_n(srce, (Texel<Col24_R8G8B8>*)	dest, width); break;
				case D3DFMT::A8P8:
				case D3DFMT::A8L8:		copy_n(srce, (Texel<Col16_A8P8>*)	dest, width); break;
				case D3DFMT::A1R5G5B5:	copy_n(srce, (Texel<Col16_A1R5G5B5>*)dest, width); break;
				case D3DFMT::A4R4G4B4:	copy_n(srce, (Texel<Col16_A4R4G4B4>*)dest, width); break;
				case D3DFMT::R5G6B5:	copy_n(srce, (Texel<Col16_R5G6B5>*)	dest, width); break;
				case D3DFMT::L8:
				case D3DFMT::P8:
					for (int x = 0; x < width; x++)
						((uint8*)dest)[x] = srce[x].r;
					break;
				case D3DFMT::R8G8_B8G8:
					for (int x = 0; x < width; x += 2) {
						((uint8*)dest)[x * 2 + 0] = (srce[x + 0].r + srce[x + 1].r) / 2;
						((uint8*)dest)[x * 2 + 1] = srce[x + 0].g;
						((uint8*)dest)[x * 2 + 2] = (srce[x + 0].b + srce[x + 1].b) / 2;
						((uint8*)dest)[x * 2 + 3] = srce[x + 1].g;
					}
					break;
				case D3DFMT::G8R8_G8B8:
					for (int x = 0; x < width; x += 2) {
						((uint8*)dest)[x * 2 + 0] = srce[x + 0].g;
						((uint8*)dest)[x * 2 + 1] = (srce[x + 0].r + srce[x + 1].r) / 2;
						((uint8*)dest)[x * 2 + 2] = srce[x + 1].g;
						((uint8*)dest)[x * 2 + 3] = (srce[x + 0].b + srce[x + 1].b) / 2;
					}
					break;
				default:
					break;
			};
			file.writebuff(dest, scan);
		}
	}

	return true;
}

bool SaveSurface(const block<HDRpixel, 2> &rect, ostream_ref file, DDS::D3DFMT fmt) {
	using namespace DDS;
	int		width	= rect.size<1>(), height = rect.size<2>();

	switch (fmt) {
		case D3DFMT::BC6H_UF16:
		case D3DFMT::BC6H_SF16: {
			auto	dest = make_auto_block<BC<6>>(div_round_up(width, 4), div_round_up(height, 4));
			copy(rect, dest.get());
			return file.write(dest);
		}
		case D3DFMT::A32B32G32R32F: {
			for (int y = 0; y < height; y++)
				file.writebuff(rect[y], width * sizeof(float[4]));
			return true;
		}

		default:
			return false;
	}

}

template<typename T> bool SaveAllMipSurfaces(const block<T, 2> &rect, ostream_ref file, DDS::D3DFMT fmt, int num_mips) {
	if (num_mips == 1)
		return SaveSurface(rect, file, fmt);

	for (int i = 0; i < num_mips; i++) {
		if (!SaveSurface(GetMip(rect, i), file, fmt))
			return false;
	}
	return true;
}

bool DDSFileHandler::Write(bitmap &bm, ostream_ref file, DDS::D3DFMT fmt) {
	using namespace DDS;

	HEADER		ddsh;
	HEADER_DS10	ddsh10;

	bool		intensity	= bm.IsIntensity();
	bool		paletted	= bm.IsPaletted();
	bool		alpha		= bm.HasAlpha();
//	bool		sixteen		= BMWF_GETBITS(flags) == 16;

	file.write("DDS "_u32);

	int			width		= bm.BaseWidth();
	int			height		= bm.BaseHeight();
	int			depth		= bm.Depth();
	int			num_mips	= bm.Mips();

	clear(ddsh);
	clear(ddsh10);
	ddsh.size		= sizeof(ddsh);
	ddsh.flags		= DDSD_TEXTURE;
	ddsh.width		= width;
	ddsh.height		= height;
	ddsh.dwCaps		= DDSCAPS_TEXTURE;

	if (num_mips) {
		int	block		= BlockWidth(fmt);
		int	max_mips	= MaxMips(width / block, height / block);
		num_mips		= min(num_mips, max_mips - 1);

		ddsh.flags			|= DDSD_MIPMAPCOUNT;
		ddsh.dwCaps			|= DDSCAPS_MIPMAP;
		ddsh.dwMipMapCount	= num_mips + 1;
	}

	if (fmt == D3DFMT::UNKNOWN) {
	/*	if (flags & BMWF_COMPRESS) {
			fmt	= !alpha || (bm.Flags() & BMF_ONEBITALPHA) || BMWF_GETALPHABITS(flags) == 1 ? D3DFMT::DXT1: BMWF_GETALPHABITS(flags) == 4 ? D3DFMT::DXT2: D3DFMT::DXT4;
			ddsh.flags		|= DDS::HEADER_FLAGS_LINEARSIZE;
			ddsh.dwPitchOrLinearSize = (width / 4) * (height / 4) * (fmt == D3DFMT::DXT1 ? 8: 16);
		} else */{
			if (paletted) {
				fmt = alpha ? D3DFMT::A8P8: D3DFMT::P8;
			} else if (intensity) {
				fmt = alpha ? D3DFMT::A8L8: D3DFMT::L8;
			} /*else if (sixteen) {
				fmt = alpha ? (BMWF_GETALPHABITS(flags) == 1 || (bm.Flags() & BMF_ONEBITALPHA) ? D3DFMT::A1R5G5B5: D3DFMT::A4R4G4B4): D3DFMT::R5G6B5;
			} */else {
				fmt = alpha ? D3DFMT::A8B8G8R8 : D3DFMT::R8G8B8;
			}
		}
	}
	ddsh.flags		|= DDSD_LINEARSIZE;

	const DDS::PIXELFORMAT	*ddspf;
	switch (fmt) {
		case D3DFMT::DXT1:		ddspf = &DDSPF_DXT1;		break;
		case D3DFMT::DXT2:		ddspf = &DDSPF_DXT2;		break;
		case D3DFMT::DXT3:		ddspf = &DDSPF_DXT3;		break;
		case D3DFMT::DXT4:		ddspf = &DDSPF_DXT4;		break;
		case D3DFMT::DXT5:		ddspf = &DDSPF_DXT5;		break;
//		case D3DFMT::A8R8G8B8:	ddspf = &DDSPF_A8R8G8B8;	break;
		case D3DFMT::A8B8G8R8:	ddspf = &DDSPF_A8B8G8R8;	break;
		case D3DFMT::A1R5G5B5:	ddspf = &DDSPF_A1R5G5B5;	break;
		case D3DFMT::A4R4G4B4:	ddspf = &DDSPF_A4R4G4B4;	break;
		case D3DFMT::R8G8B8:	ddspf = &DDSPF_R8G8B8;		break;
		case D3DFMT::R5G6B5:	ddspf = &DDSPF_R5G6B5;		break;
		case D3DFMT::R8G8_B8G8:	ddspf = &DDSPF_R8G8_B8G8;	break;
		case D3DFMT::G8R8_G8B8:	ddspf = &DDSPF_G8R8_G8B8;	break;
		case D3DFMT::A8L8:		ddspf = &DDSPF_A8L8;		break;
		case D3DFMT::L8:		ddspf = &DDSPF_L8;			break;
		default:
			if (!is_dxgi(fmt))
				return false;
			ddspf						= &DDSPF_DS10;
			ddsh10.dxgiFormat			= (DDS::DXGI_FORMAT)((uint32)fmt & ~0x80000000);
			ddsh10.resourceDimension	= D3D10_RESOURCE_DIMENSION_TEXTURE2D;
			ddsh10.arraySize			= depth;
			break;
	}

	if (bm.IsCube()) {
		ddsh.dwCaps		|= DDSCAPS_COMPLEX;
		ddsh.dwCaps2	= DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALL_FACES;
	} else if (bm.IsVolume()) {
		ddsh.dwDepth	= depth;
		ddsh.flags		|= DDSD_DEPTH;
		ddsh.dwCaps2	= DDSCAPS2_VOLUME;
	} else if (depth != 1 && !is_dxgi(fmt)) {
		ddspf			= &DDSPF_DS10;
		ddsh10.dxgiFormat			= GetDXGIformat(fmt);
		ddsh10.resourceDimension	= D3D10_RESOURCE_DIMENSION_TEXTURE2D;
		ddsh10.arraySize			= depth;
	}

	ddsh.ddspf = *ddspf;
	file.write(ddsh);

	if (ddspf == &DDSPF_DS10)
		file.write(ddsh10);

	if (bm.IsCube()) {
		for (int i = 0; i < 6; i++) {
			if (!SaveAllMipSurfaces(bm.Block(0, height * i, bm.Width(), height), file, fmt, num_mips + 1))
				return false;
		}
	} else if (bm.IsVolume()) {
		for (int i = 0; i < depth; i++) {
			if (!SaveSurface(bm.Block(0, height * i, bm.Width(), height), file, fmt))
				return false;
		}
	} else {
		for (int i = 0; i < depth; i++) {
			if (!SaveAllMipSurfaces(bm.Block(0, height * i, bm.Width(), height), file, fmt, num_mips + 1))
				return false;
		}
	}
	return true;
}

bool DDSFileHandler::Write(HDRbitmap &bm, ostream_ref file, DDS::D3DFMT fmt) {
	using namespace DDS;

	if (fmt == D3DFMT::UNKNOWN)
		fmt = D3DFMT::A32B32G32R32F;

	int			width		= bm.BaseWidth();
	int			height		= bm.BaseHeight();
	int			depth		= bm.Depth();
	int			num_mips	= bm.Mips();

	HEADER	ddsh;
	clear(ddsh);
	ddsh.size		= sizeof(ddsh);
	ddsh.flags		= DDSD_TEXTURE | DDSD_LINEARSIZE;
	ddsh.width		= width;
	ddsh.height		= height;
	ddsh.dwDepth	= depth == 6 ? 1: depth;
	ddsh.dwCaps		= DDSCAPS_TEXTURE;

	if (num_mips > 1) {
		ddsh.flags			|= DDSD_MIPMAPCOUNT;
		ddsh.dwCaps			|= DDSCAPS_MIPMAP;
		ddsh.dwMipMapCount	= num_mips + 1;
	}

	if (bm.IsCube()) {
		ddsh.dwCaps		|= DDSCAPS_COMPLEX;
		ddsh.dwCaps2	= DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALL_FACES;
	} else if (bm.IsVolume()) {
		ddsh.dwDepth	= depth;
		ddsh.flags		|= DDSD_DEPTH;
		ddsh.dwCaps2	= DDSCAPS2_VOLUME;
	}

	PIXELFORMAT DDSPF_A32B32G32R32F(D3DFMT::A32B32G32R32F, 128, 32,32,32,32);
	ddsh.ddspf = DDSPF_A32B32G32R32F;

	file.write("DDS "_u32);
	file.write(ddsh);
#if 0
	return SaveSurface(bm.Block(0, 0, width, height), file, fmt);
#else
	if (bm.IsCube()) {
		for (int i = 0; i < 6; i++) {
			if (!SaveAllMipSurfaces(bm.Block(0, height * i, bm.Width(), height), file, fmt, num_mips + 1))
				return false;
		}
	} else if (bm.IsVolume()) {
		for (int i = 0; i < depth; i++) {
			if (!SaveSurface(bm.Block(0, height * i, bm.Width(), height), file, fmt))
				return false;
		}
	} else {
		for (int i = 0; i < depth; i++) {
			if (!SaveAllMipSurfaces(bm.Block(0, height * i, bm.Width(), height), file, fmt, num_mips + 1))
				return false;
		}
	}
	return true;
#endif
}

bool SaveSurface(const vbitmap_loc &loc, ostream_ref file, int w, int h, int bpp) {
	auto	temp = make_auto_block<uint8>(w * bpp / 8, h);
	loc.get(temp);
	file.writebuff(temp[0], w * bpp / 8 * h);
	return true;
}

bool SaveAllMipSurfaces(const vbitmap_loc &loc, ostream_ref file, int w, int h, int bpp, int num_mips) {
	for (int i = 0; i < num_mips; ++i) {
		if (!SaveSurface(vbitmap_loc(loc).set_mip(i), file, max(w >> i, 1), max(h >> i, 1), bpp))
			return false;
	}
	return true;
}

bool DDSFileHandler::Write(vbitmap &bm, ostream_ref file, DDS::D3DFMT fmt, uint32 flags) {
	using namespace DDS;

	HEADER			ddsh;
	PIXELFORMAT		&ddspf	= ddsh.ddspf;

	clear(ddsh);

	int			width		= bm.Width();
	int			height		= bm.Height();
	int			depth		= bm.Depth();
	int			num_mips	= bm.Mips();

	ddsh.size		= sizeof(ddsh);
	ddspf.size		= sizeof(ddspf);
	ddsh.flags		= flags | DDSD_TEXTURE | DDSD_LINEARSIZE;
	ddsh.width		= width;
	ddsh.height		= height;
	ddsh.dwCaps		= DDSCAPS_TEXTURE;

	if (num_mips > 1) {
		ddsh.flags			|= DDSD_MIPMAPCOUNT;
		ddsh.dwCaps			|= DDSCAPS_MIPMAP;
		ddsh.dwMipMapCount	= num_mips + 1;
	}
	if (bm.IsCube()) {
		ddsh.dwCaps		|= DDSCAPS_COMPLEX;
		ddsh.dwCaps2	= DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALL_FACES;
	} else if (bm.IsVolume()) {
		ddsh.dwDepth	= depth;
		ddsh.flags		|= DDSD_DEPTH;
		ddsh.dwCaps2	= DDSCAPS2_VOLUME;
	}

	if (fmt != D3DFMT::UNKNOWN) {
		static struct {D3DFMT d3dfmt; vbitmap_format vfmt; } formats[] = {
			{D3DFMT::DXT1,			vbitmap_format(5,6,5,1,		vbitmap_format::ALL_COMP)	},
			{D3DFMT::DXT3,			vbitmap_format(5,6,5,4,		vbitmap_format::RGB_COMP)	},
			{D3DFMT::DXT5,			vbitmap_format(5,6,5,8,		vbitmap_format::ALL_COMP)	},
			{D3DFMT::A8,			vbitmap_format(0,0,0,8)									},
		//	{D3DFMT::A1R5G5B5,		vbitmap_format(5,5,5,1)									},
			{D3DFMT::A4R4G4B4,		vbitmap_format(4,4,4,4)									},
			{D3DFMT::R5G6B5,		vbitmap_format(5,6,5,0)									},
			{D3DFMT::A8B8G8R8,		vbitmap_format(8,8,8,8)									},
			{D3DFMT::G16R16F,		vbitmap_format(16,16,0,0,	vbitmap_format::FLOAT)		},
			{D3DFMT::A16B16G16R16F,	vbitmap_format(16,16,16,16,	vbitmap_format::FLOAT)		},
			{D3DFMT::A32B32G32R32F,	vbitmap_format(32,32,32,32,	vbitmap_format::FLOAT)		},
			{D3DFMT::R32F,			vbitmap_format(32,0,0,0,	vbitmap_format::FLOAT)		},
		};
		for (int i = 0; i < num_elements(formats); i++) {
			if (formats[i].vfmt == bm.format) {
				fmt = formats[i].d3dfmt;
				break;
			}
		}
	}

	uint32	bpp = 0;
	ddspf.fourCC	= (uint32)fmt;

	switch (fmt) {
		case D3DFMT::DXT1:
		case D3DFMT::BC4:
			bpp			= 64;
			width		/= 4;
			height		/= 4;
			ddspf.flags	= DDS_FOURCC;
			break;
		case D3DFMT::DXT3:
		case D3DFMT::DXT5:
		case D3DFMT::BC5:
			bpp			= 128;
			width		/= 4;
			height		/= 4;
			ddspf.flags	= DDS_FOURCC;
			break;
		default:
			break;
	}

	if ((uint32)fmt < 0x100) {
		if (bm.format & 0x003f3f3f)
			ddspf.flags	|= DDS_RGB;
		if (bm.format & 0x3f000000)
			ddspf.flags	|= DDS_ALPHA;

		bpp				= bm.format.bits();
		ddspf.bits	= bpp;
		uint32	mask0	= 1;
		uint32	mask1	= mask0 << ((bm.format >>  0) & 0x3f);
		uint32	mask2	= mask1 << ((bm.format >>  8) & 0x3f);
		uint32	mask3	= mask2 << ((bm.format >> 16) & 0x3f);
		uint32	mask4	= mask3 << ((bm.format >> 24) & 0x3f);
		ddspf.mask[0]	= mask1 - mask0;
		ddspf.mask[1]	= mask2 - mask1;
		ddspf.mask[2]	= mask3 - mask2;
		ddspf.mask[3]	= mask4 - mask3;
	}

	file.write("DDS "_u32);
	file.write(ddsh);

	if (bm.Flags() & BMF_VOLUME) {
		for (int i = 0; i < num_mips; ++i) {
			for (int z = 0, nz = max(depth >> i, 1); z < nz; z++) {
				if (!SaveSurface(vbitmap_loc(bm).set_mip(i).set_z(z), file, max(width >> i, 1), max(height >> i, 1), bpp))
					return false;
			}
		}
	} else {
		for (int i = 0; i < depth; i++) {
			if (!SaveAllMipSurfaces(vbitmap_loc(bm).set_z(i), file, width, height, bpp, num_mips))
				return false;
		}
	}
	return true;
}

bool DDSFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	using namespace DDS;

	D3DFMT	fmt = D3DFMT::UNKNOWN;
	ChannelUse	cu(GetFormatString(p));

	static struct {D3DFMT format; uint32 sizes; } formats[] = {
		{D3DFMT::R8G8B8,		CHANS(8,8,8,0)	},
		{D3DFMT::A8R8G8B8,		CHANS(8,8,8,8)	},
//		{D3DFMT::X8R8G8B8,		CHANS()	},
		{D3DFMT::R5G6B5,		CHANS(5,6,5,0)	},
		{D3DFMT::X1R5G5B5,		CHANS(5,5,5,0)	},
//		{D3DFMT::A1R5G5B5,		CHANS()	},
		{D3DFMT::A4R4G4B4,		CHANS(4,4,4,4)	},
		{D3DFMT::R3G3B2,		CHANS(3,3,2,0)	},
		{D3DFMT::A8,			CHANS(0,0,0,8)	},
		{D3DFMT::A8R3G3B2,		CHANS(2,3,2,8)	},
//		{D3DFMT::X4R4G4B4,		CHANS()	},
		{D3DFMT::A2B10G10R10,	CHANS(10,10,10,2)	},
//		{D3DFMT::A8B8G8R8,		CHANS()	},
//		{D3DFMT::X8B8G8R8,		CHANS()	},
//		{D3DFMT::G16R16,		CHANS()	},
//		{D3DFMT::A2R10G10B10,	CHANS()	},
//		{D3DFMT::A16B16G16R16,	CHANS()	},
//		{D3DFMT::A8P8,			CHANS()	},
//		{D3DFMT::P8,			CHANS()	},
		{D3DFMT::L8,			CHANS(0x48,0,0,0)	},
		{D3DFMT::A8L8,			CHANS(0x48,0,0,8)	},
		{D3DFMT::A4L4,			CHANS(0x44,0,0,4)	},
//		{D3DFMT::V8U8,			CHANS()	},
//		{D3DFMT::L6V5U5,		CHANS()	},
//		{D3DFMT::X8L8V8U8,		CHANS()	},
//		{D3DFMT::Q8W8V8U8,		CHANS()	},
//		{D3DFMT::V16U16,		CHANS()	},
//		{D3DFMT::A2W10V10U10,	CHANS()	},
//		{D3DFMT::UYVY,			CHANS()	},
//		{D3DFMT::R8G8_B8G8,		CHANS()	},
//		{D3DFMT::YUY2,			CHANS()	},
//		{D3DFMT::G8R8_G8B8,		CHANS()	},
		{D3DFMT::DXT1,			CHANS(0x85,0x86,0x85,0x81)	},
		{D3DFMT::DXT2,			CHANS(0x85,0x86,0x85,0x04)	},
		{D3DFMT::DXT3,			CHANS(0x85,0x86,0x85,0x04)	},
		{D3DFMT::DXT4,			CHANS(0x85,0x86,0x85,0x88)	},
		{D3DFMT::DXT5,			CHANS(0x85,0x86,0x85,0x88)	},
		{D3DFMT::BC4,			CHANS(0x88,0x80,0x80,0x00)	},
		{D3DFMT::BC5,			CHANS(0x88,0x88,0x00,0x00)	},
		{D3DFMT::BC6H_UF16,		CHANS(0x90,0x90,0x90,0x40)	},
		{D3DFMT::BC6H_SF16,		CHANS(0x90,0xd0,0x90,0x40)	},
		{D3DFMT::BC7_UNORM,		CHANS(0x85,0x86,0x85,0x85)	},
	};

	if (cu) {
		int	i = cu.FindBestMatch(&formats[0].sizes, num_elements(formats), &formats[1].sizes - &formats[0].sizes);
		if (i >= 0)
			fmt = formats[i].format;
	}

	if (p.IsType<HDRbitmap>())
		return Write(*(HDRbitmap*)p, file, fmt);

	if (p.IsType<vbitmap>())
		return Write(*(vbitmap*)p, file, fmt, 0);

	if (ISO_ptr<bitmap2> p2 = ISO_conversion::convert<bitmap2>(p)) {
		p = *p2;

		if (p.IsType<HDRbitmap>())
			return Write(*(HDRbitmap*)p, file, fmt);

		if (p.IsType<vbitmap>())
			return Write(*(vbitmap*)p, file, fmt, 0);

		return Write(*(bitmap*)p, file, fmt);
	}

	return false;
}

#endif
