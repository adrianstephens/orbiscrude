#ifndef DXGI_HELPERS_H
#define DXGI_HELPERS_H

#ifdef PLAT_XONE
#include <d3d11_x.h>
#else
#include <DXGI1_4.h>
#endif

#include "base/defs.h"
#include "base/vector.h"
#include "extra/colour.h"
#include "packed_types.h"
#include "com.h"

namespace iso {

struct DXGI_COMPONENTS {
	enum LAYOUT {
		UNKNOWN,
		R32G32B32A32, R32G32B32, R16G16B16A16, R32G32, R32G8X24, R10G10B10A2, R11G11B10, R8G8B8A8,
		R16G16,R32, R24G8, R8G8, R16, R8, R1, B5G6R5, B5G5R5A1, R4G4B4A4, R9G9B9E5, R8G8_B8G8, G8R8_G8B8,
		BC1, BC2, BC3,  BC4, BC5, BC6, BC7,
		R16G8, R4G4,
		YUV420_8, YUV420_16, YUV411_8, YUV411_16
	};
	enum TYPE {
		TYPELESS, FLOAT, UFLOAT, UINT, SINT, UNORM, SNORM, SRGB,
		UFLOAT2,
	};
	enum CHANNEL {
		X, Y, Z, W, _0, _1
	};
	enum SWIZZLE {
		RGBA	= X  | ( Y << 3) | ( Z << 6) | ( W << 9),
		BGRA	= Z  | ( Y << 3) | ( X << 6) | ( W << 9),
		RGB_	= X  | ( Y << 3) | ( Z << 6) | (_1 << 9),
		RG__	= X  | ( Y << 3) | (_0 << 6) | (_1 << 9),
		R___	= X  | (_0 << 3) | (_0 << 6) | (_1 << 9),
		___A	= _0 | (_0 << 3) | (_0 << 6) | ( X << 9),
		_G__	= _0 | ( Y << 3) | (_0 << 6) | (_1 << 9),
		ALL0	= _0 | ( _0 << 3) | (_0 << 6) | (_0 << 9),
	};

	friend SWIZZLE	Swizzle(CHANNEL x, CHANNEL y, CHANNEL z, CHANNEL w) {
		return SWIZZLE(x | (y << 3) | (z << 6) | (w << 9));
	}
	friend CHANNEL	GetChan1(SWIZZLE s, int i)		{ return CHANNEL((s >> (i * 3)) & 7); }
	friend CHANNEL	GetChan2(SWIZZLE s, CHANNEL c)	{ return c < 4 ? GetChan1(s, c) : c; }

	friend SWIZZLE	Rearrange(SWIZZLE a, SWIZZLE b) {
		return Swizzle(
			GetChan2(a, GetChan1(b, 0)),
			GetChan2(a, GetChan1(b, 1)),
			GetChan2(a, GetChan1(b, 2)),
			GetChan2(a, GetChan1(b, 3))
		);
	}

	struct LayoutInfo {
		uint8	bits;
		uint8	comps:3, block:1, hdr:1, planar:2;
		uint8	chanbits[4];
	};
	struct DXGIInfo {
		uint32	layout:8, type:4, chans:12, dxgi:8;
	};

	uint32	layout:8, type:4, chans:12, dxgi:8;

	static constexpr const LayoutInfo GetInfo(LAYOUT layout) {
		using arr = const LayoutInfo[35];
		return arr{
		//	bits	N, B, H, P	chans
			{  0,	0, 0, 0, 0,	{0,  0,  0,  0	}	},	//UNKNOWN,
			{128,	4, 0, 1, 0,	{32, 32, 32, 32	}	},	//R32G32B32A32,
			{ 96,	3, 0, 1, 0,	{32, 32, 32, 0	}	},	//R32G32B32,
			{ 64,	4, 0, 1, 0,	{16, 16, 16, 16	}	},	//R16G16B16A16,
			{ 64,	2, 0, 1, 0,	{32, 32, 0,  0	}	},	//R32G32,
			{ 64,	2, 0, 1, 0,	{32, 8,  0,  0	}	},	//R32G8X24,
			{ 32,	4, 0, 1, 0,	{10, 10, 10, 2	}	},	//R10G10B10A2,
			{ 32,	3, 0, 1, 0,	{11, 11, 10, 0	}	},	//R11G11B10,
			{ 32,	4, 0, 0, 0,	{8,	 8,	 8,  8	}	},	//R8G8B8A8,
			{ 32,	2, 0, 1, 0,	{16, 16, 0,  0	}	},	//R16G16,
			{ 32,	1, 0, 1, 0,	{32, 0,  0,  0	}	},	//R32,
			{ 32,	2, 0, 1, 0,	{24, 8,  0,  0	}	},	//R24G8,
			{ 16,	2, 0, 0, 0,	{8,	 8,  0,  0	}	},	//R8G8,
			{ 16,	1, 0, 1, 0,	{16, 0,  0,  0	}	},	//R16,
			{  8,	1, 0, 0, 0,	{8,	 0,  0,  0	}	},	//R8,
			{  1,	1, 0, 0, 0,	{1,	 0,  0,  0	}	},	//R1,
			{ 16,	3, 0, 0, 0,	{5,	 6,  5,  0	}	},	//B5G6R5,
			{ 16,	4, 0, 0, 0,	{5,	 5,  5,  1	}	},	//B5G5R5A1,
			{ 16,	4, 0, 0, 0,	{4,	 4,  4,  4	}	},	//R4G4B4A4,
			{ 32,	3, 0, 0, 0,	{9,	 9,  9,  0	}	},	//R9G9B9E5,
			{ 16,	4, 0, 0, 0,	{8,	 8,  8,  8	}	},	//R8G8_B8G8,
			{ 16,	4, 0, 0, 0,	{8,	 8,  8,  8	}	},	//G8R8_G8B8,
			{ 64,	3, 1, 0, 0,	{0,	 0,  0,  0	}	},	//BC1,
			{128,	4, 1, 0, 0,	{0,	 0,  0,  0	}	},	//BC2,
			{128,	4, 1, 0, 0,	{0,	 0,  0,  0	}	},	//BC3,
			{128,	4, 1, 0, 0,	{0,	 0,  0,  0	}	},	//BC4,
			{128,	4, 1, 0, 0,	{0,	 0,  0,  0	}	},	//BC5,
			{128,	3, 1, 0, 0,	{0,	 0,  0,  0	}	},	//BC6,
			{128,	3, 1, 1, 0,	{0,	 0,  0,  0	}	},	//BC7,
			{ 16,	2, 0, 0, 1,	{16, 8,  0,  0	}	},	//R16G8,
			{  8,	2, 0, 0, 0,	{4,  4,  0,  0	}	},	//R4G4,
			{  8,	3, 0, 0, 2,	{4,  4,  0,  0	}	},	//YUV420_8,
			{ 16,	3, 0, 0, 2,	{4,  4,  0,  0	}	},	//YUV420_16,
			{  8,	3, 0, 0, 1,	{4,  4,  0,  0	}	},	//YUV411_8,
			{ 16,	3, 0, 0, 1,	{4,  4,  0,  0	}	}	//YUV411_16
		}[layout];
	}

	range<const DXGIInfo*>	static GetDXGITable() {
		static const DXGIInfo table[] = {
		//	 layout			type		chans	dxgi									
			{UNKNOWN,		TYPELESS,	RGBA,	DXGI_FORMAT_UNKNOWN						},
			{R32G32B32A32,	TYPELESS,	RGBA,	DXGI_FORMAT_R32G32B32A32_TYPELESS		},
			{R32G32B32A32,	FLOAT,	    RGBA,	DXGI_FORMAT_R32G32B32A32_FLOAT			},
			{R32G32B32A32,	UINT,	    RGBA,	DXGI_FORMAT_R32G32B32A32_UINT			},
			{R32G32B32A32,	SINT,	    RGBA,	DXGI_FORMAT_R32G32B32A32_SINT			},
			{R32G32B32,		TYPELESS,	RGBA,	DXGI_FORMAT_R32G32B32_TYPELESS			},
			{R32G32B32,		FLOAT,	    RGB_,	DXGI_FORMAT_R32G32B32_FLOAT				},
			{R32G32B32,		UINT,	    RGB_,	DXGI_FORMAT_R32G32B32_UINT				},
			{R32G32B32,		SINT,	    RGB_,	DXGI_FORMAT_R32G32B32_SINT				},
			{R16G16B16A16,	TYPELESS,	RGBA,	DXGI_FORMAT_R16G16B16A16_TYPELESS		},
			{R16G16B16A16,	FLOAT,	    RGBA,	DXGI_FORMAT_R16G16B16A16_FLOAT			},
			{R16G16B16A16,	UNORM,	    RGBA,	DXGI_FORMAT_R16G16B16A16_UNORM			},
			{R16G16B16A16,	UINT,	    RGBA,	DXGI_FORMAT_R16G16B16A16_UINT			},
			{R16G16B16A16,	SNORM,	    RGBA,	DXGI_FORMAT_R16G16B16A16_SNORM			},
			{R16G16B16A16,	SINT,	    RGBA,	DXGI_FORMAT_R16G16B16A16_SINT			},
			{R32G32,		TYPELESS,	RG__,	DXGI_FORMAT_R32G32_TYPELESS				},
			{R32G32,		FLOAT,	    RG__,	DXGI_FORMAT_R32G32_FLOAT				},
			{R32G32,		UINT,	    RG__,	DXGI_FORMAT_R32G32_UINT					},
			{R32G32,		SINT,	    RG__,	DXGI_FORMAT_R32G32_SINT					},
			{R32G8X24,		TYPELESS,	RG__,	DXGI_FORMAT_R32G8X24_TYPELESS			},
			{R32G8X24,		UINT,	    RG__,	DXGI_FORMAT_D32_FLOAT_S8X24_UINT		},
			{R32G8X24,		TYPELESS,	R___,	DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS	},
			{R32G8X24,		UINT,	    RGBA,	DXGI_FORMAT_X32_TYPELESS_G8X24_UINT		},
			{R10G10B10A2,	TYPELESS,	RGBA,	DXGI_FORMAT_R10G10B10A2_TYPELESS		},
			{R10G10B10A2,	UNORM,	    RGBA,	DXGI_FORMAT_R10G10B10A2_UNORM			},
			{R10G10B10A2,	UINT,	    RGBA,	DXGI_FORMAT_R10G10B10A2_UINT			},
			{R11G11B10,		UFLOAT,	    RGB_,	DXGI_FORMAT_R11G11B10_FLOAT				},
			{R8G8B8A8,		TYPELESS,	RGBA,	DXGI_FORMAT_R8G8B8A8_TYPELESS			},
			{R8G8B8A8,		UNORM,	    RGBA,	DXGI_FORMAT_R8G8B8A8_UNORM				},
			{R8G8B8A8,		SRGB,	    RGBA,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB			},
			{R8G8B8A8,		UINT,	    RGBA,	DXGI_FORMAT_R8G8B8A8_UINT				},
			{R8G8B8A8,		SNORM,	    RGBA,	DXGI_FORMAT_R8G8B8A8_SNORM				},
			{R8G8B8A8,		SINT,	    RGBA,	DXGI_FORMAT_R8G8B8A8_SINT				},
			{R16G16,		TYPELESS,	RG__,	DXGI_FORMAT_R16G16_TYPELESS				},
			{R16G16,		FLOAT,	    RG__,	DXGI_FORMAT_R16G16_FLOAT				},
			{R16G16,		UNORM,	    RG__,	DXGI_FORMAT_R16G16_UNORM				},
			{R16G16,		UINT,	    RG__,	DXGI_FORMAT_R16G16_UINT					},
			{R16G16,		SNORM,	    RG__,	DXGI_FORMAT_R16G16_SNORM				},
			{R16G16,		SINT,	    RG__,	DXGI_FORMAT_R16G16_SINT					},
			{R32,			TYPELESS,	R___,	DXGI_FORMAT_R32_TYPELESS				},
			{R32,			FLOAT,	    R___,	DXGI_FORMAT_D32_FLOAT					},
			{R32,			FLOAT,	    R___,	DXGI_FORMAT_R32_FLOAT					},
			{R32,			UINT,	    R___,	DXGI_FORMAT_R32_UINT					},
			{R32,			SINT,	    R___,	DXGI_FORMAT_R32_SINT					},
			{R24G8,			TYPELESS,	RG__,	DXGI_FORMAT_R24G8_TYPELESS				},
			{R24G8,			UINT,	    RG__,	DXGI_FORMAT_D24_UNORM_S8_UINT			},
			{R24G8,			TYPELESS,	R___,	DXGI_FORMAT_R24_UNORM_X8_TYPELESS		},
			{R24G8,			UINT,	    _G__,	DXGI_FORMAT_X24_TYPELESS_G8_UINT		},
			{R8G8,			TYPELESS,	RG__,	DXGI_FORMAT_R8G8_TYPELESS				},
			{R8G8,			UNORM,	    RG__,	DXGI_FORMAT_R8G8_UNORM					},
			{R8G8,			UINT,	    RG__,	DXGI_FORMAT_R8G8_UINT					},
			{R8G8,			SNORM,	    RG__,	DXGI_FORMAT_R8G8_SNORM					},
			{R8G8,			SINT,	    RG__,	DXGI_FORMAT_R8G8_SINT					},
			{R16,			TYPELESS,	R___,	DXGI_FORMAT_R16_TYPELESS				},
			{R16,			FLOAT,	    R___,	DXGI_FORMAT_R16_FLOAT					},
			{R16,			UNORM,	    R___,	DXGI_FORMAT_D16_UNORM					},
			{R16,			UNORM,	    R___,	DXGI_FORMAT_R16_UNORM					},
			{R16,			UINT,	    R___,	DXGI_FORMAT_R16_UINT					},
			{R16,			SNORM,	    R___,	DXGI_FORMAT_R16_SNORM					},
			{R16,			SINT,	    R___,	DXGI_FORMAT_R16_SINT					},
			{R8,			TYPELESS,	R___,	DXGI_FORMAT_R8_TYPELESS					},
			{R8,			UNORM,	    R___,	DXGI_FORMAT_R8_UNORM					},
			{R8,			UINT,	    R___,	DXGI_FORMAT_R8_UINT						},
			{R8,			SNORM,	    R___,	DXGI_FORMAT_R8_SNORM					},
			{R8,			SINT,	    R___,	DXGI_FORMAT_R8_SINT						},
			{R8,			UNORM,	    ___A,	DXGI_FORMAT_A8_UNORM					},
			{R1,			UNORM,	    R___,	DXGI_FORMAT_R1_UNORM					},
			{R9G9B9E5,		FLOAT,	    RGBA,	DXGI_FORMAT_R9G9B9E5_SHAREDEXP			},
			{R8G8_B8G8,		UNORM,	    RGBA,	DXGI_FORMAT_R8G8_B8G8_UNORM				},
			{G8R8_G8B8,		UNORM,	    RGBA,	DXGI_FORMAT_G8R8_G8B8_UNORM				},
			{BC1,			TYPELESS,	RGBA,	DXGI_FORMAT_BC1_TYPELESS				},
			{BC1,			UNORM,	    RGBA,	DXGI_FORMAT_BC1_UNORM					},
			{BC1,			SRGB,	    RGBA,	DXGI_FORMAT_BC1_UNORM_SRGB				},
			{BC2,			TYPELESS,	RGBA,	DXGI_FORMAT_BC2_TYPELESS				},
			{BC2,			UNORM,	    RGBA,	DXGI_FORMAT_BC2_UNORM					},
			{BC2,			SRGB,	    RGBA,	DXGI_FORMAT_BC2_UNORM_SRGB				},
			{BC3,			TYPELESS,	RGBA,	DXGI_FORMAT_BC3_TYPELESS				},
			{BC3,			UNORM,	    RGBA,	DXGI_FORMAT_BC3_UNORM					},
			{BC3,			SRGB,	    RGBA,	DXGI_FORMAT_BC3_UNORM_SRGB				},
			{BC4,			TYPELESS,	RGBA,	DXGI_FORMAT_BC4_TYPELESS				},
			{BC4,			UNORM,	    RGBA,	DXGI_FORMAT_BC4_UNORM					},
			{BC4,			SNORM,	    RGBA,	DXGI_FORMAT_BC4_SNORM					},
			{BC5,			TYPELESS,	RGBA,	DXGI_FORMAT_BC5_TYPELESS				},
			{BC5,			UNORM,	    RGBA,	DXGI_FORMAT_BC5_UNORM					},
			{BC5,			SNORM,	    RGBA,	DXGI_FORMAT_BC5_SNORM					},
			{B5G6R5,		UNORM,	    BGRA,	DXGI_FORMAT_B5G6R5_UNORM				},
			{B5G5R5A1,		UNORM,	    BGRA,	DXGI_FORMAT_B5G5R5A1_UNORM				},
			{R8G8B8A8,		UNORM,	    BGRA,	DXGI_FORMAT_B8G8R8A8_UNORM				},
			{R8G8B8A8,		UNORM,	    BGRA,	DXGI_FORMAT_B8G8R8X8_UNORM				},
			{R10G10B10A2,	UNORM,	    RGBA,	DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM	},
			{R8G8B8A8,		TYPELESS,	BGRA,	DXGI_FORMAT_B8G8R8A8_TYPELESS			},
			{R8G8B8A8,		SRGB,	    BGRA,	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB			},
			{R8G8B8A8,		TYPELESS,	BGRA,	DXGI_FORMAT_B8G8R8X8_TYPELESS			},
			{R8G8B8A8,		SRGB,	    BGRA,	DXGI_FORMAT_B8G8R8X8_UNORM_SRGB			},
			{BC6,			TYPELESS,	RGBA,	DXGI_FORMAT_BC6H_TYPELESS				},
			{BC6,			UFLOAT,	    RGBA,	DXGI_FORMAT_BC6H_UF16					},
			{BC6,			FLOAT,	    RGBA,	DXGI_FORMAT_BC6H_SF16					},
			{BC7,			TYPELESS,	RGBA,	DXGI_FORMAT_BC7_TYPELESS				},
			{BC7,			UNORM,	    RGBA,	DXGI_FORMAT_BC7_UNORM					},
			{BC7,			SRGB,	    RGBA,	DXGI_FORMAT_BC7_UNORM_SRGB				},
			{R8G8B8A8,		UNORM,		RGBA,	DXGI_FORMAT_AYUV						},
			{R10G10B10A2,	UNORM,		RGBA,	DXGI_FORMAT_Y410						},
			{R16G16B16A16,	UNORM,		RGBA,	DXGI_FORMAT_Y416						},
			{YUV420_8,		UNORM,		RGBA,	DXGI_FORMAT_NV12						},
			{YUV420_16,		UNORM,		RGBA,	DXGI_FORMAT_P010						},
			{YUV420_16,		UNORM,		RGBA,	DXGI_FORMAT_P016						},
			{YUV420_8,		UNORM,		RGBA,	DXGI_FORMAT_420_OPAQUE					},
			{R8G8B8A8,		UNORM,		RGBA,	DXGI_FORMAT_YUY2						},
			{R16G16B16A16,	UNORM,		RGBA,	DXGI_FORMAT_Y210						},
			{R16G16B16A16,	UNORM,		RGBA,	DXGI_FORMAT_Y216						},
			{YUV411_8,		TYPELESS,	RGBA,	DXGI_FORMAT_NV11						},
			{R4G4,			TYPELESS,	RGBA,	DXGI_FORMAT_AI44						},
			{R4G4,			TYPELESS,	RGBA,	DXGI_FORMAT_IA44						},
			{R8,			TYPELESS,	RGBA,	DXGI_FORMAT_P8							},
			{R8G8,			TYPELESS,	RGBA,	DXGI_FORMAT_A8P8						},
			{R4G4B4A4,		TYPELESS,	BGRA,	DXGI_FORMAT_B4G4R4A4_UNORM				},
		#ifdef D_PLATFORM_XBOXONE
			{R10G10B10A2,	UFLOAT,		RGBA,	DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT		},
			{R10G10B10A2,	UFLOAT2,	RGBA,	DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT		},
			{R16G8,			UNORM,		RGBA,	DXGI_FORMAT_D16_UNORM_S8_UINT			},
			{R16G8,			UNORM,		RGBA,	DXGI_FORMAT_R16_UNORM_X8_TYPELESS		},
			{R16G8,			UINT,		RGBA,	DXGI_FORMAT_X16_TYPELESS_G8_UINT		},
		#endif
			{UNKNOWN,		TYPELESS,	RGBA,	DXGI_FORMAT_P208						},
			{UNKNOWN,		TYPELESS,	RGBA,	DXGI_FORMAT_V208						},
			{UNKNOWN,		TYPELESS,	RGBA,	DXGI_FORMAT_V408						},
		#ifdef D_PLATFORM_XBOXONE
			{UNKNOWN,		UNORM,		RGBA,	DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM	},
			{R4G4,			UNORM,		RG__,	DXGI_FORMAT_R4G4_UNORM					},
			{R24G8,			UNORM,		RG__,	DXGI_FORMAT_R24G8_UNORM					},
		#endif
		};
		return table;
	}

	DXGI_FORMAT	GetFormat() const {
		for (auto &i : GetDXGITable()) {
			if (layout == i.layout && type == i.type && chans == i.chans)
				return (DXGI_FORMAT)i.dxgi;
		}
		return DXGI_FORMAT_UNKNOWN;
	}

	constexpr DXGI_COMPONENTS(LAYOUT layout, TYPE type, SWIZZLE chans = RGBA, DXGI_FORMAT dxgi = DXGI_FORMAT_UNKNOWN) : layout(layout), type(type), chans(chans), dxgi(dxgi) {}
	constexpr DXGI_COMPONENTS() : layout(0), type(0), chans(0), dxgi(0) {}

	DXGI_COMPONENTS(DXGI_FORMAT f) {
		range<const DXGIInfo*>	table = GetDXGITable();
		auto	*i	= table.begin();
		for (auto n = table.size(); n; n >>= 1) {
			auto	middle = i + (n >> 1);
			if (middle->dxgi < f) {
				i = ++middle;
				--n;
			}
		}
		layout	= i->layout;
		type	= i->type;
		chans	= i->chans;
	}

	constexpr LAYOUT		Layout()		const	{ return LAYOUT(layout); }
	constexpr TYPE			Type()			const	{ return TYPE(type); }
	constexpr LayoutInfo	GetLayoutInfo()	const	{ return GetInfo(Layout()); }
	constexpr CHANNEL		GetChan(int i)	const	{ return CHANNEL((chans >> (i * 3)) & 7); }

	DXGI_COMPONENTS&		Layout(LAYOUT x)		{ layout = x; return *this; }
	DXGI_COMPONENTS&		Type(TYPE x)			{ type = x; return *this; }

	constexpr int			Bits()			const	{ return GetLayoutInfo().bits; }
	constexpr int			Bytes()			const	{ return Bits() / 8; }
	constexpr int			NumComps()		const	{ return GetLayoutInfo().comps; }
	constexpr int			CompSize(int i)	const	{ return GetLayoutInfo().chanbits[i]; }
	constexpr bool			IsHDR()			const	{ return GetLayoutInfo().hdr; }
	constexpr bool			IsBlock()		const	{ return GetLayoutInfo().block; }
	constexpr uint8			NumPlanes()		const	{ return layout == R32G8X24 ? 2 : 1; }	//not true on AMD

	constexpr friend int	Bits(LAYOUT layout)				{ return GetInfo(layout).bits; }
	constexpr friend int	Bytes(LAYOUT layout)			{ return GetInfo(layout).bits / 8; }
	constexpr friend int	NumComps(LAYOUT layout)			{ return GetInfo(layout).comps; }
	constexpr friend int	CompSize(LAYOUT layout, int i)	{ return GetInfo(layout).chanbits[i]; }
	constexpr friend bool	IsHDR(LAYOUT layout)			{ return GetInfo(layout).hdr; }
	constexpr friend bool	IsBlock(LAYOUT layout)			{ return GetInfo(layout).block; }
	constexpr friend uint8	NumPlanes(LAYOUT layout)		{ return layout == R32G8X24 ? 2 : 1; }	//not true on AMD

	void	SetChan(int i, CHANNEL c)	{ chans = (chans & ~(7 << (i * 3))) | (c << (i * 3)); }
};

constexpr uint32 dxgi_align(uint32 x) {
	return align_pow2(x, 8);
}
constexpr uint32 dxgi_padding(uint32 x) {
	return -int(x) & 255;
}
constexpr uint32 dxgi_align(uint32 x, bool align) {
	return align ? dxgi_align(x) : x;
}
constexpr uint32 adjust_size(DXGI_COMPONENTS fmt, uint32 x) {
	return fmt.IsBlock() ? (x + 3) >> 2 : x;
}
constexpr uint32 mip_size(uint32 x, int mip) {
	return max(x >> mip, 1);
}
constexpr uint32 mip_size(DXGI_COMPONENTS fmt, uint32 x, int mip) {
	return adjust_size(fmt, mip_size(x, mip));
}
constexpr uint32 stride(DXGI_COMPONENTS fmt, uint32 x) {
	return fmt.Bytes() * adjust_size(fmt, x);
}
constexpr uint32 mip_stride(DXGI_COMPONENTS fmt, uint32 x, int mip) {
	return fmt.Bytes() * mip_size(fmt, x, mip);
}

inline size_t size1D(DXGI_COMPONENTS fmt, int width, int mips, bool align = true) {
	size_t	total	= 0;
	for (int i = 0; i < mips; i++)
		total += dxgi_align(mip_stride(fmt, width, i), align);
	return total;
}
inline size_t size2D(DXGI_COMPONENTS fmt, int width, int height, int mips, bool align = true) {
	size_t	total	= 0;
	for (int i = 0; i < mips; i++)
		total += dxgi_align(mip_stride(fmt, width, i), align) *  mip_size(fmt, height, i);
	return total;
}
inline size_t size3D(DXGI_COMPONENTS fmt, int width, int height, int depth, int mips, bool align = true) {
	size_t	total	= 0;
	for (int i = 0; i < mips; i++)
		total += dxgi_align(mip_stride(fmt, width, i), align) *  mip_size(fmt, height, i) *  mip_size(depth, i);
	return total;
}

inline uint32	ByteSize(DXGI_FORMAT format) { return DXGI_COMPONENTS(format).Bytes(); }
inline uint8	NumPlanes(DXGI_FORMAT format) { return DXGI_COMPONENTS(format).NumPlanes(); } //not true on AMD

//-----------------------------------------------------------------------------
//	component types
//-----------------------------------------------------------------------------

typedef DXGI_FORMAT	ComponentType;
template<typename T, typename V=void> struct	_ComponentType { enum {value = DXGI_FORMAT_UNKNOWN}; };

template<int V, int S> struct	_BufferFormat							{ enum {value = V}; };
template<> struct				_BufferFormat<DXGI_FORMAT_UNKNOWN, 1>	{ enum {value = DXGI_FORMAT_R8_UINT}; };
template<> struct				_BufferFormat<DXGI_FORMAT_UNKNOWN, 2>	{ enum {value = DXGI_FORMAT_R16_UINT}; };
template<> struct				_BufferFormat<DXGI_FORMAT_UNKNOWN, 4>	{ enum {value = DXGI_FORMAT_R32_UINT}; };

#define DEFCOMPTYPE(T, V)	template<> struct _ComponentType<T> { enum {value = V}; }

DEFCOMPTYPE(void,				DXGI_FORMAT_UNKNOWN);
DEFCOMPTYPE(int8,				DXGI_FORMAT_R8_SINT);
DEFCOMPTYPE(int8[2],			DXGI_FORMAT_R8G8_SINT);
DEFCOMPTYPE(int8[4],			DXGI_FORMAT_R8G8B8A8_SINT);
DEFCOMPTYPE(uint8,				DXGI_FORMAT_R8_UINT);
DEFCOMPTYPE(uint8[2],			DXGI_FORMAT_R8G8_UINT);
DEFCOMPTYPE(uint8[4],			DXGI_FORMAT_R8G8B8A8_UINT);
DEFCOMPTYPE(norm8,				DXGI_FORMAT_R8_SNORM);
DEFCOMPTYPE(norm8[2],			DXGI_FORMAT_R8G8_SNORM);
DEFCOMPTYPE(norm8[4],			DXGI_FORMAT_R8G8B8A8_SNORM);
DEFCOMPTYPE(unorm8,				DXGI_FORMAT_R8_UNORM);
DEFCOMPTYPE(unorm8[2],			DXGI_FORMAT_R8G8_UNORM);
DEFCOMPTYPE(unorm8[4],			DXGI_FORMAT_R8G8B8A8_UNORM);

DEFCOMPTYPE(int16,				DXGI_FORMAT_R16_SINT);
DEFCOMPTYPE(int16[2],			DXGI_FORMAT_R16G16_SINT);
DEFCOMPTYPE(int16[4],			DXGI_FORMAT_R16G16B16A16_SINT);
DEFCOMPTYPE(uint16,				DXGI_FORMAT_R16_UINT);
DEFCOMPTYPE(uint16[2],			DXGI_FORMAT_R16G16_UINT);
DEFCOMPTYPE(uint16[4],			DXGI_FORMAT_R16G16B16A16_UINT);
DEFCOMPTYPE(norm16,				DXGI_FORMAT_R16_SNORM);
DEFCOMPTYPE(norm16[2],			DXGI_FORMAT_R16G16_SNORM);
DEFCOMPTYPE(norm16[4],			DXGI_FORMAT_R16G16B16A16_SNORM);
DEFCOMPTYPE(unorm16,			DXGI_FORMAT_R16_UNORM);
DEFCOMPTYPE(unorm16[2],			DXGI_FORMAT_R16G16_UNORM);
DEFCOMPTYPE(unorm16[4],			DXGI_FORMAT_R16G16B16A16_UNORM);
DEFCOMPTYPE(float16,			DXGI_FORMAT_R16_FLOAT);
DEFCOMPTYPE(float16[2],			DXGI_FORMAT_R16G16_FLOAT);
DEFCOMPTYPE(float16[4],			DXGI_FORMAT_R16G16B16A16_FLOAT);

DEFCOMPTYPE(int32,				DXGI_FORMAT_R32_SINT);
DEFCOMPTYPE(int32[2],			DXGI_FORMAT_R32G32_SINT);
DEFCOMPTYPE(int32[3],			DXGI_FORMAT_R32G32B32_SINT);
DEFCOMPTYPE(int32[4],			DXGI_FORMAT_R32G32B32A32_SINT);
DEFCOMPTYPE(uint32,				DXGI_FORMAT_R32_UINT);
DEFCOMPTYPE(uint32[2],			DXGI_FORMAT_R32G32_UINT);
DEFCOMPTYPE(uint32[3],			DXGI_FORMAT_R32G32B32_UINT);
DEFCOMPTYPE(uint32[4],			DXGI_FORMAT_R32G32B32A32_UINT);
DEFCOMPTYPE(float,				DXGI_FORMAT_R32_FLOAT);
DEFCOMPTYPE(float[2],			DXGI_FORMAT_R32G32_FLOAT);
DEFCOMPTYPE(float[3],			DXGI_FORMAT_R32G32B32_FLOAT);
DEFCOMPTYPE(float[4],			DXGI_FORMAT_R32G32B32A32_FLOAT);

DEFCOMPTYPE(uint4_10_10_10_2,	DXGI_FORMAT_R10G10B10A2_UINT);
DEFCOMPTYPE(float3_11_11_10,	DXGI_FORMAT_R11G11B10_FLOAT);
DEFCOMPTYPE(r5g6b5,				DXGI_FORMAT_B5G6R5_UNORM);
DEFCOMPTYPE(r5g5b5a1,			DXGI_FORMAT_B5G5R5A1_UNORM);
DEFCOMPTYPE(r4g4b4a4,			DXGI_FORMAT_B4G4R4A4_UNORM);

DEFCOMPTYPE(norm24_uint8,		DXGI_FORMAT_D24_UNORM_S8_UINT);

#undef DEFCOMPTYPE

template<typename T>		struct _ComponentType<rawint<T> >			: _ComponentType<T>			{};
template<typename T, int N> struct _ComponentType<rawint<T>[N]>			: _ComponentType<T[N]>		{};
template<typename V>		struct _ComponentType<V, enable_if_t<is_vec<V>>>	: _ComponentType<element_type<V>[num_elements_v<V>]>		{};
template<>					struct _ComponentType<rgba8>				: _ComponentType<unorm8[4]> {};
//template<>					struct _ComponentType<float4>				: _ComponentType<float[4]>	{};
template<>					struct _ComponentType<colour>				: _ComponentType<float[4]>	{};

com_ptr<IDXGIAdapter1>	GetAdapter(int index);
com_ptr<IDXGIOutput>	FindAdapterOutput(char* name);
com_ptr<IDXGIOutput>	GetAdapterOutput(int adapter_index, int output_index);

#ifndef PLAT_XONE
com_ptr<IDXGIAdapter3>	GetAdapter(LUID id);
com_ptr<IDXGIFactory3>	GetDXGIFactory(IUnknown *device);
#endif

} // namespace iso

#endif	// DXGI_HELPERS_H

