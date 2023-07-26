#ifndef GRAPHICS_H
#define GRAPHICS_H

#define USE_DX9

#include "shared\graphics_defs.h"
#include "packed_types.h"
#include "base/atomic.h"
#include "base/block.h"
#include "allocators/lf_allocator.h"
#include "directx\Include\d3dx9effect.h"
#include <d3d9helper.h>
#include "windows/window.h"
#include "com.h"

#undef U_WRAP
#undef V_WRAP
#undef U_MIRROR
#undef V_MIRROR

#define ISO_HAS_ALPHATEST

namespace iso {

template<typename T> void DeInit(T *t);
template<typename T> void Init(T *t, void*);

typedef IDirect3DDevice9	D3DDevice;
typedef	IDirect3DSwapChain9 D3DSwapChain;

//class bitmap;
//class HDRbitmap;
//struct vbitmap;

//-----------------------------------------------------------------------------
//	functions
//-----------------------------------------------------------------------------

#ifdef _DEBUG
bool _CheckResult(HRESULT hr, const char *file, uint32 line);
#define CheckResult(hr)	_CheckResult(hr, __FILE__, __LINE__)
#else
inline bool CheckResult(HRESULT hr) { return SUCCEEDED(hr); }
#endif

float4x4 hardware_fix(param(float4x4) mat);
float4x4 map_fix(param(float4x4) mat);

//-----------------------------------------------------------------------------
//	enums
//-----------------------------------------------------------------------------

enum Memory {
	MEM_DEFAULT				= D3DUSAGE_WRITEONLY,
	MEM_DYNAMIC				= D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
	MEM_SYSTEM				= 0x00000004L,	//unused by dx9
	MEM_TARGET				= D3DUSAGE_RENDERTARGET,
	MEM_DEPTH				= D3DUSAGE_DEPTHSTENCIL,
	MEM_READABLE			= 0,

	MEM_CUBE				= 0,//0x00000400L,	//unused by dx9
	MEM_VOLUME				= 0,//

	MEM_OTHER				= MEM_DEFAULT,
};

enum TexFormat {
	TEXF_D24S8				= D3DFMT_D24S8,
	TEXF_D24FS8				= D3DFMT_D24FS8,
	TEXF_D15S1				= D3DFMT_D15S1,
	TEXF_D16				= D3DFMT_D16,
	TEXF_D16_LOCKABLE		= D3DFMT_D16_LOCKABLE,
	TEXF_D32				= D3DFMT_D32,
	TEXF_D32F_LOCKABLE		= D3DFMT_D32F_LOCKABLE,
	TEXF_D32_LOCKABLE		= D3DFMT_D32_LOCKABLE,
	TEXF_S8_LOCKABLE		= D3DFMT_S8_LOCKABLE,

	TEXF_A8R8G8B8			= D3DFMT_A8R8G8B8,
	TEXF_A16B16G16R16		= D3DFMT_A16B16G16R16,
	TEXF_R8G8B8				= D3DFMT_R8G8B8,
	TEXF_R5G6B5				= D3DFMT_R5G6B5,
	TEXF_A1R5G5B5			= D3DFMT_A1R5G5B5,
	TEXF_A4R4G4B4			= D3DFMT_A4R4G4B4,
	TEXF_R3G3B2				= D3DFMT_R3G3B2,
	TEXF_A8R3G3B2			= D3DFMT_A8R3G3B2,
	TEXF_A2B10G10R10		= D3DFMT_A2B10G10R10,
	TEXF_G16R16				= D3DFMT_G16R16,
	TEXF_A2R10G10B10		= D3DFMT_A2R10G10B10,

	TEXF_R16F				= D3DFMT_R16F,
	TEXF_G16R16F			= D3DFMT_G16R16F,
	TEXF_A16B16G16R16F		= D3DFMT_A16B16G16R16F,
	TEXF_R32F				= D3DFMT_R32F,
	TEXF_G32R32F			= D3DFMT_G32R32F,
	TEXF_A32B32G32R32F		= D3DFMT_A32B32G32R32F,

	TEXF_A8					= D3DFMT_A8,
	TEXF_L8					= D3DFMT_L8,
	TEXF_A8L8				= D3DFMT_A8L8,
	TEXF_A4L4				= D3DFMT_A4L4,
	TEXF_L16				= D3DFMT_L16,

	TEXF_DXT1				= D3DFMT_DXT1,
	TEXF_DXT2				= D3DFMT_DXT2,
	TEXF_DXT3				= D3DFMT_DXT3,
	TEXF_DXT4				= D3DFMT_DXT4,
	TEXF_DXT5				= D3DFMT_DXT5,

	TEXF_R8G8B8A8			= TEXF_A8R8G8B8,
};

template<typename T> struct		_TexFormat;
template<typename T> TexFormat	GetTexFormat()			{ return TexFormat(_TexFormat<T>::value); }
template<typename T> TexFormat	GetTexFormat(const T&)	{ return TexFormat(_TexFormat<T>::value); }

//typedef	scaled_vector3<uint16,5,31,6,63,5,31>			r5g6b5;
//typedef	scaled_vector4<uint16,1,1,5,31,5,31,5,31>		a1r5g5b5;
//typedef	scaled_vector4<uint16,4,15,4,15,4,15,4,15>		a4r4g4b4;
//typedef	scaled_vector3<uint8,3,7,3,7,2,3>				r3g3b2;
//typedef	scaled_vector4<uint16,8,255,3,7,3,7,2,3>		a8r3g3b2;

#define DEFTEXFORMAT(T, V)	template<> struct _TexFormat<T> { enum {value = V}; }
DEFTEXFORMAT(unorm16[2],		TEXF_G16R16			);
DEFTEXFORMAT(unorm16[4],		TEXF_A16B16G16R16	);
DEFTEXFORMAT(float,				TEXF_R32F			);
DEFTEXFORMAT(float[2],			TEXF_G32R32F		);
DEFTEXFORMAT(float[4],			TEXF_A32B32G32R32F	);
DEFTEXFORMAT(unorm8,			TEXF_A8				);
DEFTEXFORMAT(unorm8[3],			TEXF_R8G8B8			);
DEFTEXFORMAT(unorm8[4],			TEXF_R8G8B8A8		);
DEFTEXFORMAT(r5g6b5,			TEXF_R5G6B5			);
//DEFTEXFORMAT(a1r5g5b5,			TEXF_A1R5G5B5		);
//DEFTEXFORMAT(a4r4g4b4,			TEXF_A4R4G4B4		);
//DEFTEXFORMAT(r3g3b2,			TEXF_R3G3B2			);
//DEFTEXFORMAT(a8r3g3b2,			TEXF_A8R3G3B2		);
//DEFTEXFORMAT(unorm4_2_10_10_10,	TEXF_A2B10G10R10	);
#undef DEFTEXFORMAT

enum PrimType {
	PRIM_UNKNOWN			= 0,
	PRIM_POINTLIST			= D3DPT_POINTLIST,
	PRIM_LINELIST			= D3DPT_LINELIST,
	PRIM_LINESTRIP			= D3DPT_LINESTRIP,
	PRIM_TRILIST			= D3DPT_TRIANGLELIST,
	PRIM_TRIFAN				= D3DPT_TRIANGLEFAN,
	PRIM_TRISTRIP			= D3DPT_TRIANGLESTRIP,
	PRIM_QUADLIST			= 0x1000, // emulated
	PRIM_RECTLIST			= 0x1001, // emulated
};
inline PrimType AdjacencyPrim(PrimType p)	{ return p; }
inline PrimType PatchPrim(int n)			{ return PrimType(0); }

enum BackFaceCull {
	BFC_NONE				= D3DCULL_NONE,
	BFC_BACK				= D3DCULL_CW,
	BFC_FRONT				= D3DCULL_CCW,
};

enum DepthTest {
	DT_NEVER				= D3DCMP_NEVER,
	DT_LESS					= D3DCMP_LESS,
	DT_EQUAL				= D3DCMP_EQUAL,
	DT_LEQUAL				= D3DCMP_LESSEQUAL,
	DT_GREATER				= D3DCMP_GREATER,
	DT_NOTEQUAL				= D3DCMP_NOTEQUAL,
	DT_GEQUAL				= D3DCMP_GREATEREQUAL,
	DT_ALWAYS 				= D3DCMP_ALWAYS,

	DT_USUAL				= DT_GEQUAL,
	DT_CLOSER_SAME			= DT_GEQUAL,
	DT_CLOSER				= DT_GREATER,
};

enum AlphaTest {
	AT_NEVER				= D3DCMP_NEVER,
	AT_LESS					= D3DCMP_LESS,
	AT_EQUAL				= D3DCMP_EQUAL,
	AT_LEQUAL				= D3DCMP_LESSEQUAL,
	AT_GREATER				= D3DCMP_GREATER,
	AT_NOTEQUAL				= D3DCMP_NOTEQUAL,
	AT_GEQUAL				= D3DCMP_GREATEREQUAL,
	AT_ALWAYS 				= D3DCMP_ALWAYS,
};

enum UVMode {
	U_CLAMP					= D3DTADDRESS_CLAMP,
	V_CLAMP					= D3DTADDRESS_CLAMP	<<4,
	U_WRAP					= D3DTADDRESS_WRAP,			//0
	V_WRAP					= D3DTADDRESS_WRAP	<<4,	//0
	U_MIRROR				= D3DTADDRESS_MIRROR,
	V_MIRROR				= D3DTADDRESS_MIRROR<<4,

	ALL_CLAMP				= U_CLAMP	| V_CLAMP,
	ALL_WRAP				= U_WRAP	| V_WRAP,
	ALL_MIRROR				= U_MIRROR	| V_MIRROR,
};

#define MAKE_TEXFILTER(mag, min, mip)	((mag) << 0) | ((min) << 4) | ((mip) << 8)
enum TexFilter {// mag, min, mip
	TF_NEAREST_NEAREST_NONE		= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE),
	TF_NEAREST_LINEAR_NONE		= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_NONE),
	TF_NEAREST_NEAREST_NEAREST	= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT),
	TF_NEAREST_LINEAR_NEAREST	= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_POINT),
	TF_NEAREST_NEAREST_LINEAR	= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_LINEAR),
	TF_NEAREST_LINEAR_LINEAR	= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_LINEAR),

	TF_LINEAR_NEAREST_NONE		= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_POINT, D3DTEXF_NONE),
	TF_LINEAR_LINEAR_NONE		= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE),
	TF_LINEAR_NEAREST_NEAREST	= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_POINT, D3DTEXF_POINT),
	TF_LINEAR_LINEAR_NEAREST	= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_POINT),
	TF_LINEAR_NEAREST_LINEAR	= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_POINT, D3DTEXF_LINEAR),
	TF_LINEAR_LINEAR_LINEAR		= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR),
};
#undef MAKE_TEXFILTER

enum ChannelMask {
	CM_RED					= D3DCOLORWRITEENABLE_RED,
	CM_GREEN				= D3DCOLORWRITEENABLE_GREEN,
	CM_BLUE					= D3DCOLORWRITEENABLE_BLUE,
	CM_ALPHA				= D3DCOLORWRITEENABLE_ALPHA,
	CM_RGB					= CM_RED | CM_GREEN | CM_BLUE,
	CM_ALL					= CM_RED | CM_GREEN | CM_BLUE | CM_ALPHA,
};

enum MSAA {
	MSAA_NONE				= D3DMULTISAMPLE_NONE,
	MSAA_TWO				= D3DMULTISAMPLE_2_SAMPLES,
	MSAA_FOUR				= D3DMULTISAMPLE_4_SAMPLES,
};

enum CubemapFace {
	CF_POS_X				= D3DCUBEMAP_FACE_POSITIVE_X,
	CF_NEG_X				= D3DCUBEMAP_FACE_NEGATIVE_X,
	CF_POS_Y				= D3DCUBEMAP_FACE_POSITIVE_Y,
	CF_NEG_Y				= D3DCUBEMAP_FACE_NEGATIVE_Y,
	CF_POS_Z				= D3DCUBEMAP_FACE_POSITIVE_Z,
	CF_NEG_Z				= D3DCUBEMAP_FACE_NEGATIVE_Z,
};

enum TexState {
	TS_ADDRESS_U			= D3DSAMP_ADDRESSU,
	TS_ADDRESS_V			= D3DSAMP_ADDRESSV,
	TS_ADDRESS_W			= D3DSAMP_ADDRESSW,
//	D3DSAMP_BORDERCOLOR
	TS_FILTER_MAG			= D3DSAMP_MAGFILTER,
	TS_FILTER_MIN			= D3DSAMP_MINFILTER,
	TS_FILTER_MIP			= D3DSAMP_MIPFILTER,
	TS_MIP_BIAS				= D3DSAMP_MIPMAPLODBIAS,
	TS_MIP_MIN				= D3DSAMP_MAXMIPLEVEL,
//	TS_MIP_MAX				= D3DSAMP_MINMIPLEVEL,
	TS_ANISO_MAX			= D3DSAMP_MAXANISOTROPY,
};

#define TSV_MAXLOD	13

//X360-only, but included in each platform to avoid needing ifdef's
enum RenderTarget {
	RT_COLOUR0				= 0,
	RT_COLOUR1				= 1,
	RT_COLOUR2				= 2,
	RT_COLOUR3				= 3,
	RT_DEPTH				= 4
};

enum BlendOp {
	BLENDOP_ADD				= D3DBLENDOP_ADD,
	BLENDOP_MIN				= D3DBLENDOP_MIN,
	BLENDOP_MAX				= D3DBLENDOP_MAX,
	BLENDOP_SUBTRACT		= D3DBLENDOP_SUBTRACT,
	BLENDOP_REVSUBTRACT		= D3DBLENDOP_REVSUBTRACT,
};

enum BlendFunc {
	BLEND_ZERO				= D3DBLEND_ZERO,
	BLEND_ONE				= D3DBLEND_ONE,
	BLEND_SRC_COLOR			= D3DBLEND_SRCCOLOR,
	BLEND_INV_SRC_COLOR		= D3DBLEND_INVSRCCOLOR,
	BLEND_DST_COLOR			= D3DBLEND_DESTCOLOR,
	BLEND_INV_DST_COLOR		= D3DBLEND_INVDESTCOLOR,
	BLEND_SRC_ALPHA  		= D3DBLEND_SRCALPHA  ,
	BLEND_INV_SRC_ALPHA		= D3DBLEND_INVSRCALPHA,
	BLEND_DST_ALPHA			= D3DBLEND_DESTALPHA,
	BLEND_INV_DST_ALPHA		= D3DBLEND_INVDESTALPHA,
	BLEND_SRC_ALPHA_SATURATE= D3DBLEND_SRCALPHASAT,
	BLEND_CONSTANT_COLOR	= D3DBLEND_BLENDFACTOR,
	BLEND_INV_CONSTANT_COLOR= D3DBLEND_INVBLENDFACTOR,
//	BLEND_CONSTANT_ALPHA	= D3DBLEND_CONSTANTALPHA,
//	BLEND_INV_CONSTANT_ALPHA= D3DBLEND_INVCONSTANTALPHA,
};

enum StencilOp {
	STENCILOP_KEEP			= D3DSTENCILOP_KEEP,
	STENCILOP_ZERO			= D3DSTENCILOP_ZERO,
	STENCILOP_REPLACE		= D3DSTENCILOP_REPLACE,
	STENCILOP_INCR			= D3DSTENCILOP_INCRSAT,
	STENCILOP_INCR_WRAP		= D3DSTENCILOP_INCR,
	STENCILOP_DECR			= D3DSTENCILOP_DECRSAT,
	STENCILOP_DECR_WRAP		= D3DSTENCILOP_DECR,
	STENCILOP_INVERT		= D3DSTENCILOP_INVERT,
};

enum StencilFunc {
	STENCILFUNC_NEVER		= D3DCMP_NEVER,
	STENCILFUNC_LESS		= D3DCMP_LESS,
	STENCILFUNC_LEQUAL		= D3DCMP_LESSEQUAL,
	STENCILFUNC_GREATER		= D3DCMP_GREATER,
	STENCILFUNC_GEQUAL		= D3DCMP_GREATEREQUAL,
	STENCILFUNC_EQUAL		= D3DCMP_EQUAL,
	STENCILFUNC_NOTEQUAL	= D3DCMP_NOTEQUAL,
	STENCILFUNC_ALWAYS		= D3DCMP_ALWAYS,
};

enum FillMode {
	FILL_SOLID				= D3DFILL_SOLID,
	FILL_WIREFRAME			= D3DFILL_WIREFRAME,
};

//-----------------------------------------------------------------------------
//	Texture/Surface
//-----------------------------------------------------------------------------

typedef IDirect3DBaseTexture9	_Texture;

class Surface {
	template<typename T> struct Temp : public block<T,2> {
		IDirect3DSurface9 *surf;
		Temp(IDirect3DSurface9 *surf, const point &size) : surf(surf)	{
			D3DLOCKED_RECT	rect;
			CheckResult(surf->LockRect(&rect, NULL, 0));
			block<T,2>::operator=(make_block((T*)map.pData, size.x), map.RowPitch, size.y));
		}
		~Temp()		{ surf->UnlockRect(0); }
	};

	com_ptr2<IDirect3DSurface9>	surf;
	bool	Init(TexFormat fmt, int width, int height);
public:
	Surface()								{}
	Surface(IDirect3DSurface9 *t) : surf(t)	{ surf->Release(); }
	Surface(TexFormat fmt, int width, int height)	{ Init(fmt, width, height); }
	Surface(TexFormat fmt, const point &size)		{ Init(fmt, size.x, size.y); }
	IDirect3DSurface9*	operator->()		const	{ return surf; }
	operator IDirect3DSurface9*()			const	{ return surf; }
	IDirect3DSurface9**	operator&()					{ return &surf; }

	point		Size()						const	{ D3DSURFACE_DESC desc; surf->GetDesc(&desc); return {desc.Width, desc.Height}; }
	template<typename T> Temp<T> Data()		const	{ return {get(), CalcSubresource(), Size(), ctx}; }
};


class Texture : DXwrapper<_Texture, 32> {
	template<typename T> struct Temp : public block<T,2> {
		IDirect3DTexture9 *tex;
		Temp(IDirect3DTexture9 *tex, int level, const point &size) : tex(tex)	{
			D3DLOCKED_RECT	rect;
			CheckResult(tex->LockRect(level, &rect, NULL, 0));
			block<T,2>::operator=(make_block((T*)map.pData, size.x), map.RowPitch, size.y));
		}
		~Temp()		{ tex->UnlockRect(0); }
	};

	friend void Init<Texture>(Texture *x, void *physram);
	D3DSURFACE_DESC	GetDesc()				const	{ D3DSURFACE_DESC d; ((IDirect3DTexture9*)safe())->GetLevelDesc(0, &d); return d;  }
public:
	Texture()										{}
	bool		Init(TexFormat format, int width, int height, int depth = 1, int mips = 1, uint32 flags = 0);
	void		DeInit();
	point		Size()						const	{ D3DSURFACE_DESC desc = GetDesc(); return {desc.Width, desc.Height}; }
	int			Depth()						const;
	bool		IsDepth()					const	{ return false; }

	Texture(TexFormat format, int width, int height, int depth = 1, int mips = 1, uint8 flags = 0)		{ Init(format, width, height, depth, mips, flags); }
	template<typename T> Texture(int width, int height, int depth = 1, int mips = 1, uint8 flags = 0)	{ Init(GetTexFormat<T>(), width, height, depth, mips, flags); }

	Surface		GetSurface(int i = 0)			const	{ IDirect3DSurface9	*t; ((IDirect3DTexture9*)safe())->GetSurfaceLevel(i, &t); return t;	}
	Surface		GetSurface(CubemapFace f, int i)const	{ IDirect3DSurface9	*t; ((IDirect3DCubeTexture9*)safe())->GetCubeMapSurface((D3DCUBEMAP_FACES)f, i, &t); return t;	}

	_Texture*	operator->()				const	{ return safe(); }
	operator _Texture*()					const	{ return safe(); }
	operator Surface()						const	{ return GetSurface(0); }
	template<typename T> Temp<T> Data(int level = 0)		const	{ return {safe(), level, Size()}; }
	template<typename T> Temp<T> WriteData(int level = 0)	const	{ return {safe(), level, Size()}; }
};

/*
_Texture*	_MakeTexture(bitmap *bm);
Texture		MakeTexture(bitmap *bm);
Texture		MakeTexture(HDRbitmap *bm);
Texture		MakeTexture(vbitmap *bm);
*/
//-----------------------------------------------------------------------------
//	IndexBuffer/VertexBuffer
//-----------------------------------------------------------------------------

class _VertexBuffer {
	com_ptr<IDirect3DVertexBuffer9>		vb;
protected:
	struct _Temp {
		IDirect3DVertexBuffer9	*b;
		void					*p;
		_Temp(IDirect3DVertexBuffer9 *_b) : b(_b)	{ CheckResult(b->Lock(0, 0, &p, D3DLOCK_READONLY)); }
		~_Temp()									{ b->Unlock();	}
	};
public:
	bool		Init(void *data, uint32 size, Memory loc = MEM_DEFAULT);
	bool		Create(uint32 size, Memory loc = MEM_DEFAULT);
	void*		Begin()						const;//		{ void *lock; return SUCCEEDED(vb->lock(0, 0, &lock, 0)) ? lock : NULL; }
	void		End()						const;//		{ vb->Unlock();		}
	uint32		Size()						const;//		{ if (!vb) return 0; D3DVERTEXBUFFER_DESC desc; vb->GetDesc(&desc); return desc.Size; }
	operator	IDirect3DVertexBuffer9*()	const;
	IDirect3DVertexBuffer9	*operator->()	const;
};

inline void*	_VertexBuffer::Begin()						const	{ void *lock; return SUCCEEDED(vb->Lock(0, 0, &lock, 0)) ? lock : NULL; }
inline void		_VertexBuffer::End()						const	{ vb->Unlock();		}
inline uint32	_VertexBuffer::Size()						const	{ if (!vb) return 0; D3DVERTEXBUFFER_DESC desc; vb->GetDesc(&desc); return desc.Size; }
inline _VertexBuffer::operator IDirect3DVertexBuffer9*()	const	{ return vb;		}

template<typename T> class VertexBuffer : public _VertexBuffer {
	struct Temp : _Temp {
		Temp(IDirect3DVertexBuffer9 *_b) : _Temp(_b)	{}
		operator const T*()	 const { return (const T*)p; }
		const T*	get()	 const { return (const T*)p; }
	};
public:
	VertexBuffer()															{}
	VertexBuffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)				{ Init(t, n, loc);	}
	template<int N> VertexBuffer(const T (&t)[N], Memory loc = MEM_DEFAULT)		{ Init(t, loc);		}

	bool					Init(T *t, uint32 n, Memory loc = MEM_DEFAULT)		{ return _VertexBuffer::Init(t, n * sizeof(T), loc);}
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT)		{ return _VertexBuffer::Init(&t, sizeof(t), loc);	}
	bool	Create(uint32 n, Memory loc = MEM_DEFAULT)	{ return _VertexBuffer::Create(n * sizeof(T), loc);	}
	T*		Begin(uint32 n, Memory loc = MEM_DEFAULT)	{ return Create(n, loc) ? Begin() : NULL;	}
	T*		Begin()							const		{ return (T*)_VertexBuffer::Begin();		}
	Temp	Data()							const		{ return (IDirect3DVertexBuffer9*)*this;	}
	uint32	Size()							const		{ return _VertexBuffer::Size() / sizeof(T);	}
};

class _IndexBuffer {
	com_ptr<IDirect3DIndexBuffer9>		ib;
protected:
	struct _Temp {
		IDirect3DIndexBuffer9	*b;
		void					*p;
		_Temp(IDirect3DIndexBuffer9 *_b) : b(_b)	{ CheckResult(b->Lock(0, 0, &p, D3DLOCK_READONLY)); }
		~_Temp()									{ b->Unlock();	}
	};
public:
	bool		Transfer(const void *data, uint32 size) {
		memcpy(Begin(), data, size);
		End();
		return true;
	}
	template<int SIZE> bool	Create(uint32 size, Memory loc = MEM_DEFAULT);
	void*		Begin()						const		{ void *lock; return SUCCEEDED(ib->Lock(0, 0, &lock, 0)) ? lock : NULL; }
	void		End()						const		{ ib->Unlock();	}
	uint32		Size()						const		{ if (!ib) return 0; D3DINDEXBUFFER_DESC desc; ib->GetDesc(&desc); return desc.Size; }
	operator	IDirect3DIndexBuffer9*()	const		{ return ib;	}
	IDirect3DIndexBuffer9	*operator->()	const		{ return ib;	}
};

template<typename T> class IndexBuffer : public _IndexBuffer {
	struct Temp : _Temp {
		Temp(IDirect3DIndexBuffer9 *_b) : _Temp(_b)	{}
		operator const T*()	 const { return (const T*)p; }
		const T*	get()	 const { return (const T*)p; }
	};
public:
	IndexBuffer()										{}
	IndexBuffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)					{ Init(t, n, loc);		}
	template<int N> IndexBuffer(const T (&t)[N], Memory loc = MEM_DEFAULT)		{ Init(data, N, loc);	}

	bool					Init(T *t, uint32 n, Memory loc = MEM_DEFAULT)		{ return t && _IndexBuffer::Create<sizeof(T)>(n * sizeof(T), loc) && Transfer(t, n * sizeof(T));	}
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT)		{ return _IndexBuffer::Create<sizeof(T)>(sizeof(t), loc) && Transfer(t, sizeof(t));		}
	bool	Create(uint32 n, Memory loc = MEM_DEFAULT)	{ return _IndexBuffer::Create<sizeof(T)>(n * sizeof(T), loc);	}
	T*		Begin(uint32 n, Memory loc = MEM_DEFAULT)	{ return Create(n, loc) ? Begin() : NULL;	}
	T*		Begin()							const		{ return (T*)_IndexBuffer::Begin();			}
	Temp	Data()							const		{ return (IDirect3DIndexBuffer9*)*this;		}
	uint32	Size()							const		{ return _IndexBuffer::Size() / sizeof(T);	}
};

//-----------------------------------------------------------------------------
//	Vertex
//-----------------------------------------------------------------------------

enum ComponentUsage {
	USAGE_UNKNOWN			= -1,
	USAGE_POSITION			= D3DDECLUSAGE_POSITION,
	USAGE_BLENDWEIGHT		= D3DDECLUSAGE_BLENDWEIGHT,
	USAGE_BLENDINDICES		= D3DDECLUSAGE_BLENDINDICES,
	USAGE_NORMAL			= D3DDECLUSAGE_NORMAL,
	USAGE_PSIZE				= D3DDECLUSAGE_PSIZE,
	USAGE_TEXCOORD			= D3DDECLUSAGE_TEXCOORD,
	USAGE_TANGENT			= D3DDECLUSAGE_TANGENT,
	USAGE_BINORMAL			= D3DDECLUSAGE_BINORMAL,
	USAGE_TESSFACTOR		= D3DDECLUSAGE_TESSFACTOR,
	USAGE_COLOR				= D3DDECLUSAGE_COLOR,
	USAGE_FOG				= D3DDECLUSAGE_FOG,
	USAGE_DEPTH				= D3DDECLUSAGE_DEPTH,
	USAGE_SAMPLE			= D3DDECLUSAGE_SAMPLE,
};

typedef D3DDECLTYPE				ComponentType;
template<typename T>			struct _ComponentType;
template<typename T, int N>		struct _ComponentType<array_vec<T, N> >	: _ComponentType<T[N]> {};

#define DEFCOMPTYPE(T, V)	template<> struct _ComponentType<T> { enum {value = V}; }
DEFCOMPTYPE(void,				D3DDECLTYPE_UNUSED);
DEFCOMPTYPE(float,				D3DDECLTYPE_FLOAT1);
DEFCOMPTYPE(float[2],			D3DDECLTYPE_FLOAT2);
DEFCOMPTYPE(float[3],			D3DDECLTYPE_FLOAT3);
DEFCOMPTYPE(float[4],			D3DDECLTYPE_FLOAT4);
DEFCOMPTYPE(float2p,			D3DDECLTYPE_FLOAT2);
DEFCOMPTYPE(float3p,			D3DDECLTYPE_FLOAT3);
DEFCOMPTYPE(float4p,			D3DDECLTYPE_FLOAT4);
//DEFCOMPTYPE(float4,				D3DDECLTYPE_FLOAT4);
DEFCOMPTYPE(colour,				D3DDECLTYPE_FLOAT4);
//DEFCOMPTYPE(,	D3DDECLTYPE_D3DCOLOR);
DEFCOMPTYPE(uint8[4],			D3DDECLTYPE_UBYTE4);
DEFCOMPTYPE(int16[2],			D3DDECLTYPE_SHORT2);
DEFCOMPTYPE(int16[4],			D3DDECLTYPE_SHORT4);
DEFCOMPTYPE(norm8[4],			D3DDECLTYPE_UBYTE4N);
DEFCOMPTYPE(unorm8[4],			D3DDECLTYPE_UBYTE4N);
DEFCOMPTYPE(norm16[2],			D3DDECLTYPE_SHORT2N);
DEFCOMPTYPE(norm16[4],			D3DDECLTYPE_SHORT4N);
DEFCOMPTYPE(unorm16[2],			D3DDECLTYPE_USHORT2N);
DEFCOMPTYPE(unorm16[4],			D3DDECLTYPE_USHORT4N);
DEFCOMPTYPE(uint3_10_10_10,		D3DDECLTYPE_UDEC3);
DEFCOMPTYPE(norm3_10_10_10,		D3DDECLTYPE_DEC3N);
DEFCOMPTYPE(float16[2],			D3DDECLTYPE_FLOAT16_2);
DEFCOMPTYPE(float16[4],			D3DDECLTYPE_FLOAT16_4);
DEFCOMPTYPE(rgba8,				D3DDECLTYPE_UBYTE4N);
// faked
DEFCOMPTYPE(uint16[2],			D3DDECLTYPE_SHORT2);
DEFCOMPTYPE(uint16[4],			D3DDECLTYPE_SHORT4);
DEFCOMPTYPE(int32,				D3DDECLTYPE_SHORT2);
DEFCOMPTYPE(uint32,				D3DDECLTYPE_SHORT2);
#undef DEFCOMPTYPE

template<typename T> constexpr ComponentType	GetComponentType()			{ return ComponentType(_ComponentType<T>::value); }
template<typename T> constexpr ComponentType	GetComponentType(const T&)	{ return ComponentType(_ComponentType<T>::value); }

struct VertexElement : D3DVERTEXELEMENT9 {
	VertexElement()				{}
	VertexElement(const _none&)	{ Terminate(); }
	constexpr VertexElement(size_t offset, ComponentType type, ComponentUsage usage, uint8 usage_index = 0, int stream = 0) : D3DVERTEXELEMENT9{
		WORD(stream),
		WORD(offset),
		type,
		D3DDECLMETHOD_DEFAULT,
		usage,
		usage_index,
	} {}
	template<typename B, typename T> constexpr VertexElement(T B::* p, ComponentUsage usage, int usage_index = 0, int stream = 0) : VertexElement(
		uintptr_t(ptr(((B*)0)->*p)),
		GetComponentType<T>(),
		usage,
		usage_index,
		stream
	) {}
	void	Terminate() {
		Stream		= 0xff;
		Offset		= 0;
		Type		= D3DDECLTYPE_UNUSED;
		Method		= D3DDECLMETHOD_DEFAULT;
		Usage		= 0;
		UsageIndex	= 0;
	}
	void	SetUsage(ComponentUsage usage, int usage_index = 0) {
		Usage		= usage;
		UsageIndex	= usage_index;
	}
};

struct VertexDescription {
	com_ptr<IDirect3DVertexDeclaration9>	vd;
	inline VertexDescription()							{}
	inline VertexDescription(IDirect3DVertexDeclaration9 *_vd) : vd(_vd) {}
	inline VertexDescription(D3DVERTEXELEMENT9 *ve);
	inline void	 operator=(D3DVERTEXELEMENT9 *ve);
	operator IDirect3DVertexDeclaration9*()	const	{ return vd; }
};

//-----------------------------------------------------------------------------
//	Shader
//-----------------------------------------------------------------------------

struct ShaderReg {
	union {
		struct {uint16	reg:12, type:4, count:15, indirect:1;};
		uint32	u;
	};
	ShaderReg(uint32 _u)	{ u = _u;	}
	operator uint32() const	{ return u;	}
};

enum ShaderParameterType {
	_SPT_VS			= 0,
	_SPT_PS			= 4,
	_SPT_IND		= 8,

	SPT_VBOOL		= D3DXRS_BOOL	+ _SPT_VS,
	SPT_VINT		= D3DXRS_INT4	+ _SPT_VS,
	SPT_VFLOAT		= D3DXRS_FLOAT4	+ _SPT_VS,
	SPT_VSAMPLER	= D3DXRS_SAMPLER+ _SPT_VS,
	SPT_PBOOL		= D3DXRS_BOOL	+ _SPT_PS,
	SPT_PINT		= D3DXRS_INT4	+ _SPT_PS,
	SPT_PFLOAT		= D3DXRS_FLOAT4	+ _SPT_PS,
	SPT_PSAMPLER	= D3DXRS_SAMPLER+ _SPT_PS,

	SPT_VFLOATIND	= D3DXRS_FLOAT4	+ _SPT_IND,
};

class ShaderParameterIterator {
	com_ptr<ID3DXConstantTable>	ct1, ct2;
	D3DXCONSTANT_DESC	cdesc;
	ShaderReg			reg;
	int					total1, total;
	int					i;

	void Get(int i) {
		if (i < total) {
			ID3DXConstantTable	*ct = ct1;
			int	r	= 0;
			if (i >= total1) {
				i	-= total1;
				ct	= ct2;
				r	= 4;
			}
			UINT	count = 1;
			ct->GetConstantDesc(ct->GetConstant(NULL, i), &cdesc, &count);
			reg.reg		= cdesc.RegisterIndex;
			reg.type	= r + cdesc.RegisterSet;
			reg.count	= cdesc.RegisterCount;
		}
	}
	void Init(const void *vs, const void *ps) {
		D3DXCONSTANTTABLE_DESC	ctdesc;
		total = 0;
		if (vs) {
			D3DXGetShaderConstantTable((DWORD*)vs, &ct1);
			ct1->GetDesc(&ctdesc);
			total += (total1 = ctdesc.Constants);
		}
		if (ps) {
			D3DXGetShaderConstantTable((DWORD*)ps, &ct2);
			ct2->GetDesc(&ctdesc);
			total += ctdesc.Constants;
		}
		Get(i = 0);
	}

public:
	ShaderParameterIterator(const PCShader &s) : reg(0) { Init(s.vs.raw(), s.ps.raw()); }

	const char		*Name()		const		{ return cdesc.Name;			}
	ShaderReg		Reg()		const		{ return reg;					}
	const void		*Default()	const		{ return cdesc.DefaultValue;	}
	const void		*DefaultPerm()	const;

	int				Total()		const		{ return total;					}
	operator		bool()		const		{ return i < total;				}
	ShaderParameterIterator& operator++()	{ Get(++i); return *this;		}
	ShaderParameterIterator&	Reset()		{ Get(i = 0); return *this;		}
};

//-----------------------------------------------------------------------------
//	Graphics
//-----------------------------------------------------------------------------

template<typename T> struct SetShaderConstants_s;

class GraphicsContext;
class Application;

struct FrameAllocator : atomic<circular_allocator> {
	void	*ends[2];

	FrameAllocator() {}
	FrameAllocator(const memory_block &m) : atomic<circular_allocator>(m) {
		ends[0] = ends[1] = m;
	}
	void	BeginFrame(int index) {
		//index = (index + 1) % num_elements(ends);
		set_get(ends[index]);
	}
	void	EndFrame(int index) {
		ends[index] = getp();
	}
};

class Graphics {
friend GraphicsContext;
	IDirect3D9					*d3d;
	com_ptr<IDirect3DDevice9>	device;

public:
	class Display {
	protected:
		TexFormat						format;
		int								width, height;
		com_ptr<IDirect3DSwapChain9>	swapchain;
		com_ptr<IDirect3DSurface9>		disp;
	public:
		bool				SetSize(HWND hWnd, const point &size);
		bool				SetFormat(HWND hWnd, const point &size, TexFormat _format) {
			if (format != _format) {
				format = _format;
				swapchain.clear();
			}
			return SetSize(hWnd, size);
		}
		point				Size()				const	{ return {width, height}; }
		Surface				GetDispSurface()	const	{ return disp.get(); }
		IDirect3DSwapChain9	*GetSwapChain()		const	{ return swapchain; }
		bool				Present(const RECT *srce_rect = 0, const RECT *dest_rect = 0)	const;
	};
	Graphics();
	~Graphics();
	int					FindAdapter(const char *name);
	bool				Init(HWND hWnd, int adapter = D3DADAPTER_DEFAULT);
	bool				ReInit(HWND hWnd, int adapter);
	IDirect3DDevice9*	Device()			const	{ return device;	}

	void				BeginScene(GraphicsContext &ctx);
	void				EndScene(GraphicsContext &ctx);
};

extern Graphics graphics;

class GraphicsContext {
friend Application;
	IDirect3DDevice9	*device;

	inline int	Verts2Prim(PrimType prim, uint32 count) {
		static const	uint8 mult[]	= {0, 1, 2, 1, 3, 1, 1};
		static const	uint8 add[]		= {0, 0, 0, 1, 0, 2, 2};
		return (count - add[prim]) / mult[prim];
	}

public:
	void				Begin() {
		device		= graphics.Device();
	}

	IDirect3DDevice9*	Device()			const	{ return device; }
	force_inline	void	SetRenderState(D3DRENDERSTATETYPE state, uint32 value) {
		CheckResult(device->SetRenderState(state, value));
	}
	force_inline	uint32	GetRenderState(D3DRENDERSTATETYPE state) const {
		uint32 value;
		CheckResult(device->GetRenderState(state, (DWORD*)&value));
		return value;
	}
	void				PushMarker(const char *s)	{}
	void				PopMarker()					{}

	void				SetWindow(const rect &rect);
	rect				GetWindow();
	void				Clear(param(colour) col, bool zbuffer = true);
	void				ClearZ();
	void				SetMSAA(MSAA _msaa)	{}

	// render targets
	force_inline	void	SetRenderTarget(const Surface& s, RenderTarget i = RT_COLOUR0) {
		if (i == RT_DEPTH) {
			CheckResult(device->SetDepthStencilSurface(s));
		} else if (s) {
			CheckResult(device->SetRenderTarget(i, s));
		} else {
//			CheckResult(device->SetRenderTarget(i, s));
		}
	}
	force_inline	void	SetZBuffer(const Surface& s) {
		CheckResult(device->SetDepthStencilSurface(s));
	}
	force_inline	Surface	GetRenderTarget(int i = 0) {
		IDirect3DSurface9	*s;
		if (i == RT_DEPTH)
			device->GetDepthStencilSurface(&s);
		else
			device->GetRenderTarget(i, &s);
		return s;
	}
	force_inline	Surface	GetZBuffer() {
		IDirect3DSurface9	*s;
		device->GetDepthStencilSurface(&s);
		return s;
	}

	// draw
	void				SetVertexType(IDirect3DVertexDeclaration9 *vd)	{ device->SetVertexDeclaration(vd);	}
	template<typename T>void SetVertexType()							{ SetVertexType(GetVD<T>());		}

	void SetIndices(const _IndexBuffer &ib) {
		CheckResult(device->SetIndices(ib));
	}
	template<typename T>void SetVertices(const VertexBuffer<T> &vb, uint32 offset = 0) {
		device->SetVertexDeclaration(GetVD<T>());
		CheckResult(device->SetStreamSource(0, vb, offset * sizeof(T), sizeof(T)));
	}
	template<typename T>void SetVertices(uint32 stream, const VertexBuffer<T> &vb, uint32 offset = 0) {
		CheckResult(device->SetStreamSource(stream, vb, offset * sizeof(T), sizeof(T)));
	}
	void SetVertices(uint32 stream, const _VertexBuffer &vb, uint32 stride, uint32 offset = 0) {
		CheckResult(device->SetStreamSource(stream, vb, offset * stride, stride));
	}
	force_inline	void	DrawPrimitive(PrimType prim, uint32 start, uint32 count) {
		CheckResult(device->DrawPrimitive((D3DPRIMITIVETYPE)prim, start, count));
	}
	force_inline	void	DrawIndexedPrimitive(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start, uint32 count) {
		CheckResult(device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)prim, 0, min_index, num_verts, start, count));
	}
	force_inline void	DrawVertices(PrimType prim, uint32 start, uint32 count) {
		return DrawPrimitive(prim, start, Verts2Prim(prim, count));
	}
	force_inline void	DrawIndexedVertices(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start, uint32 count) {
		return DrawIndexedPrimitive(prim, min_index, num_verts, start, Verts2Prim(prim, count));
	}

	// shaders
	void	SetShader(const PCShader &s) {
		CheckResult(device->SetPixelShader(s.ps.get()));
		CheckResult(device->SetVertexShader(s.vs.get()));
	}
	void	SetShaderConstants(ShaderReg reg, const void *values);
	template<typename T> void	SetShaderConstants(const T *values, uint16 reg, uint32 count = 1) {
		SetShaderConstants_s<T>::f(device, reg, count, values);
	}
	// textures
	force_inline	void	SetTexture(Texture& tex, int i = 0) {
		CheckResult(device->SetTexture(i, tex));
	}
	force_inline	void	SetSamplerState(int sampler, TexState type, uint32 value) {
		CheckResult(device->SetSamplerState(sampler, D3DSAMPLERSTATETYPE(type), value));
	}
	force_inline	uint32	GetSamplerState(int sampler, TexState type) const {
		uint32 value; CheckResult(device->GetSamplerState(sampler, D3DSAMPLERSTATETYPE(type), (DWORD*)&value)); return value;
	}
	force_inline void	SetUVMode(int i, UVMode t)	{
		device->SetSamplerState(i, D3DSAMP_ADDRESSU, t & 15);
		device->SetSamplerState(i, D3DSAMP_ADDRESSV, t >> 4);
	}
	force_inline UVMode	GetUVMode(int i) {
		DWORD	u, v;
		device->GetSamplerState(i, D3DSAMP_ADDRESSU, &u);
		device->GetSamplerState(i, D3DSAMP_ADDRESSV, &v);
		return UVMode(u | (v << 4));
	}
	force_inline void	SetTexFilter(int i, TexFilter t) {
		device->SetSamplerState(i, D3DSAMP_MAGFILTER, (t >> 0) & 15);
		device->SetSamplerState(i, D3DSAMP_MINFILTER, (t >> 4) & 15);
		device->SetSamplerState(i, D3DSAMP_MIPFILTER, (t >> 8) & 15);
	}
	force_inline TexFilter	GetTexFilter(int i) {
		DWORD	mag, min, mip;
		device->GetSamplerState(i, D3DSAMP_MAGFILTER, &mag);
		device->GetSamplerState(i, D3DSAMP_MINFILTER, &min);
		device->GetSamplerState(i, D3DSAMP_MIPFILTER, &mip);
		return TexFilter((mag << 0) | (min << 4) | (mip << 8));
	}

	//raster
	force_inline void	SetFillMode(FillMode fill_mode)	{
		device->SetRenderState(D3DRS_FILLMODE, fill_mode);
	}
	force_inline void	SetBackFaceCull(BackFaceCull c)	{
		device->SetRenderState(D3DRS_CULLMODE, c);
	}

	force_inline void	SetDepthBias(float bias, float slope_bias) {
		device->SetRenderState(D3DRS_DEPTHBIAS,				iorf(bias / 256).i());
		device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,	iorf(slope_bias).i());
	}

	//blend
	force_inline void	SetBlendEnable(bool enable) {
		device->SetRenderState(D3DRS_ALPHABLENDENABLE,	enable);
	}
	force_inline void	SetBlend(BlendOp op, BlendFunc src, BlendFunc dest) {
		device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, false);
		device->SetRenderState(D3DRS_BLENDOP, op);
		device->SetRenderState(D3DRS_SRCBLEND, src);
		device->SetRenderState(D3DRS_DESTBLEND, dest);
	}
	force_inline void	SetBlendSeparate(BlendOp op, BlendFunc src, BlendFunc dest, BlendOp opAlpha, BlendFunc srcAlpha, BlendFunc destAlpha) {
		device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, true);
		device->SetRenderState(D3DRS_BLENDOP, op);
		device->SetRenderState(D3DRS_SRCBLEND, src);
		device->SetRenderState(D3DRS_DESTBLEND, dest);
		device->SetRenderState(D3DRS_BLENDOPALPHA, opAlpha);
		device->SetRenderState(D3DRS_SRCBLENDALPHA, srcAlpha);
		device->SetRenderState(D3DRS_DESTBLENDALPHA, destAlpha);
	}
	force_inline void	SetBlendConst(param(colour) col) {
		float4 t = col.rgba * 255;
		device->SetRenderState(D3DRS_BLENDFACTOR, D3DCOLOR_ARGB(int(t.w), int(t.x), int(t.y), int(t.z)));
	}
	force_inline void	SetMask(uint32 mask) {
		device->SetRenderState(D3DRS_COLORWRITEENABLE,	mask);
	}
	force_inline void	SetAlphaToCoverage(bool enable)		{}// device->SetRenderState(D3DRS_ALPHATOMASKENABLE,	enable);	}

	//depth & stencil
	force_inline void	SetDepthTest(DepthTest c) {
		device->SetRenderState(D3DRS_ZFUNC, c);
	}
	force_inline void	SetDepthTestEnable(bool enable) {
		device->SetRenderState(D3DRS_ZENABLE, enable);
	}
	force_inline void	SetDepthWriteEnable(bool enable) {
		device->SetRenderState(D3DRS_ZWRITEENABLE, enable);
	}

	force_inline void	SetStencilOp(StencilOp fail, StencilOp zfail, StencilOp zpass) {
		device->SetRenderState(D3DRS_STENCILFAIL,	D3DSTENCILOP_REPLACE);
		device->SetRenderState(D3DRS_STENCILZFAIL,	D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_STENCILPASS,	D3DSTENCILOP_REPLACE);
	}
	force_inline void	SetStencilFunc(StencilFunc func, unsigned char ref, unsigned char mask) {
		device->SetRenderState(D3DRS_STENCILFUNC,	func);
		device->SetRenderState(D3DRS_STENCILREF,	ref);
		device->SetRenderState(D3DRS_STENCILMASK,	mask);
	}
	force_inline void	SetStencilMask(unsigned char mask) {
		device->SetRenderState(D3DRS_STENCILWRITEMASK, 	mask);
	}

	force_inline void	SetAlphaTestEnable(bool enable)		{ device->SetRenderState(D3DRS_ALPHATESTENABLE,		enable);	}
	force_inline void	SetAlphaTest(AlphaTest func, uint32 ref) {
		device->SetRenderState(D3DRS_ALPHAFUNC, func);
		device->SetRenderState(D3DRS_ALPHAREF, ref);
	}

	force_inline void	Resolve(RenderTarget i = RT_COLOUR0) {}
};

template<> struct SetShaderConstants_s<bool> {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const bool *values) {
		d->SetVertexShaderConstantB(reg, (const BOOL*)values, count);
	}
};
template<> struct SetShaderConstants_s<int> {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const int *values) {
		d->SetVertexShaderConstantI(reg, values, count);
	}
};
template<> struct SetShaderConstants_s<float> {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const float *values) {
		d->SetVertexShaderConstantF(reg, values, count);
	}
};
template<> struct SetShaderConstants_s<float4> {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const void *values) {
		d->SetVertexShaderConstantF(reg, (const float*)values, count * 4);
	}
};

template<typename T> struct SetShaderConstants_s<array_vec<T,4> > {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const void *values) {
		SetShaderConstants_s<T>::f(d, reg, count * 4, (const T*)values);
	}
};
template<typename T> struct SetShaderConstants_s<array_vec<T,3> > {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const array_vec<T,3> *values) {
		while (count--)
			SetShaderConstants_s<T>::f(d, reg, 3, (const T*)values);
	}
};
template<typename T> struct SetShaderConstants_s<array_vec<T,2> > {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const array_vec<T,2> *values) {
		while (count--)
			SetShaderConstants_s<T>::f(d, reg, 2, (const T*)values);
	}
};
template<typename T, int N> struct SetShaderConstants_s<T[N]> {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const void *values) {
		SetShaderConstants_s<T>::f(d, reg, count * N, (const T*)values);
	}
};
template<typename A, typename B> struct SetShaderConstants_s<pair<A,B> > {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const pair<A,B> *values) {
		while (count--) {
			SetShaderConstants_s<A>::f(d, reg, 1, &values->a);
			SetShaderConstants_s<B>::f(d, reg, 1, &values->b);
		}
	}
};
template<typename A> struct SetShaderConstants_s<pair<A,A> > {
	static void f(IDirect3DDevice9 *d, uint16 reg, uint32 count, const void *values) {
		SetShaderConstants_s<A>::f(d, reg, count * 2, (const A*)values);
	}
};
template<> struct SetShaderConstants_s<float2> : SetShaderConstants_s<float4> {};
template<> struct SetShaderConstants_s<float3> : SetShaderConstants_s<float4> {};
template<> struct SetShaderConstants_s<float3x3> : SetShaderConstants_s<float3[3]> {};
template<> struct SetShaderConstants_s<float3x4> : SetShaderConstants_s<float3[4]> {};
template<> struct SetShaderConstants_s<float4x4> : SetShaderConstants_s<float4[4]> {};

//-----------------------------------------------------------------------------
//	Miscellaneous
//-----------------------------------------------------------------------------

inline VertexDescription::VertexDescription(D3DVERTEXELEMENT9 *ve)	{ graphics.Device()->CreateVertexDeclaration(ve, &vd); }
inline void VertexDescription::operator=(D3DVERTEXELEMENT9 *ve)		{ graphics.Device()->CreateVertexDeclaration(ve, &vd); }

template<typename T> IDirect3DVertexDeclaration9	*GetVD()			{ static VertexDescription vd(GetVE<T>()); return vd; }
template<typename T> IDirect3DVertexDeclaration9	*GetVD(const T &t)	{ return GetVD<T>(); }

//-----------------------------------------------------------------------------
//	ImmediateStream
//-----------------------------------------------------------------------------

class _ImmediateStream {
protected:
	static	com_ptr<IDirect3DVertexBuffer9>	vb;
	static	IndexBuffer<uint16>				ib;
	static	int 							vbi, ibi, vbsize, ibsize;

	GraphicsContext &ctx;
	PrimType	prim;
	int			count;
	void		*p;

	_ImmediateStream(GraphicsContext &_ctx, PrimType _prim, int _count, int tsize);
	void		Draw(IDirect3DVertexDeclaration9 *vd, uint32 tsize);

};

template<class T> class ImmediateStream : _ImmediateStream {
public:
	ImmediateStream(GraphicsContext &_ctx, PrimType _prim, int _count) : _ImmediateStream(_ctx, _prim, _count, sizeof(T))	{}
	~ImmediateStream()							{ Draw(GetVD<T>(), sizeof(T));	}
	T&					operator[](int i)		{ return ((T*)p)[i];	}
	T*					begin()					{ return (T*)p;			}
	T*					end()					{ return (T*)p + count;	}
	void				SetCount(int i)			{ count = i;			}
};

}
#endif	// GRAPHICS_DX9_H

