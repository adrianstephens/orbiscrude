#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "shared/graphics_defs.h"
#include "dx/dx_shaders.h"
#include "base/atomic.h"
#include "allocators/lf_allocator.h"
#include "base/vector.h"
#include "base/array.h"
#include "base/hash.h"
#include "hashes/fnv.h"
#include "base/block.h"
#include "com.h"
#include "crc32.h"

#undef	CM_NONE

namespace iso {

typedef ID3D11ShaderResourceView _Texture;
#ifdef ISO_EDITOR
extern _Texture *get_safe(const DXwrapper<_Texture, 64> *x);
template<> inline _Texture *DXwrapper<_Texture, 64>::safe() const {
	return get_safe(this);
}
#endif

#define ISO_HAS_GEOMETRY
#define ISO_HAS_HULL
#define ISO_HAS_STREAMOUT
#define ISO_HAS_GRAHICSBUFFERS

class GraphicsContext;
class ComputeContext;
class Application;

//-----------------------------------------------------------------------------
//	functions
//-----------------------------------------------------------------------------

#if 1//def _DEBUG
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
	_MEM_BIND_MASK			= 0xff,
	_MEM_ACCESS_MASK		= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE,

	MEM_DEFAULT				= 0,
	MEM_TARGET				= D3D11_BIND_RENDER_TARGET,
	MEM_DEPTH				= D3D11_BIND_DEPTH_STENCIL,
	MEM_WRITABLE			= D3D11_BIND_UNORDERED_ACCESS,
	MEM_VERTEXBUFFER		= D3D11_BIND_VERTEX_BUFFER,
	MEM_INDEXBUFFER			= D3D11_BIND_INDEX_BUFFER,

	MEM_CPU_WRITE			= D3D11_CPU_ACCESS_WRITE,
    MEM_CPU_READ			= D3D11_CPU_ACCESS_READ,

	MEM_MUTABLE				= 0x100,
	MEM_STAGING				= 0x200,

	MEM_SYSTEM				= MEM_DEFAULT,
	MEM_OTHER				= MEM_DEFAULT,

	MEM_CUBE				= 0x400,
	MEM_VOLUME				= 0x800,
	MEM_FORCE2D				= 0xc00,

	MEM_CASTABLE			= 0x1000,
	MEM_INDIRECTARG			= 0x2000,
};
constexpr Memory operator|(Memory a, Memory b)		{
	return Memory(int(a) | b);
}
constexpr Memory		Bind(D3D11_BIND_FLAG b)		{
	return Memory(b ^ D3D11_BIND_SHADER_RESOURCE);
}
constexpr uint32		GetBind(Memory loc)			{
	return (loc & _MEM_BIND_MASK) ^ D3D11_BIND_SHADER_RESOURCE;
}
constexpr uint32		GetCPUAccess(Memory loc)	{
	return loc & _MEM_ACCESS_MASK;
}
constexpr D3D11_USAGE	GetUsage(Memory loc, bool nodata = false) {
	return	loc & MEM_STAGING													? D3D11_USAGE_STAGING
		:	(loc & MEM_CPU_WRITE) && !(loc & MEM_CPU_READ)						? D3D11_USAGE_DYNAMIC
		:	nodata || (loc & (MEM_MUTABLE|MEM_TARGET|MEM_DEPTH|MEM_WRITABLE))	? D3D11_USAGE_DEFAULT
		:	D3D11_USAGE_IMMUTABLE;
}

enum TexFormat {
	TEXF_UNKNOWN			= DXGI_FORMAT_UNKNOWN,

	TEXF_D24S8				= DXGI_FORMAT_D24_UNORM_S8_UINT,
	TEXF_D24X8				= DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
	TEXF_D16				= DXGI_FORMAT_D16_UNORM,
	TEXF_D16_LOCKABLE		= DXGI_FORMAT_D16_UNORM,
	TEXF_D32F				= DXGI_FORMAT_D32_FLOAT,
	TEXF_D32F_LOCKABLE		= DXGI_FORMAT_D32_FLOAT,

	TEXF_R24X8				= DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
	TEXF_X24G8				= DXGI_FORMAT_X24_TYPELESS_G8_UINT,

	TEXF_R8G8B8A8			= DXGI_FORMAT_R8G8B8A8_UNORM,
	TEXF_A8R8G8B8			= DXGI_FORMAT_B8G8R8A8_UNORM,
	TEXF_A8B8G8R8			= DXGI_FORMAT_R8G8B8A8_UNORM,//D3DFMT_A8B8G8R8,
	TEXF_O8B8G8R8			= DXGI_FORMAT_B8G8R8X8_UNORM,
	TEXF_A16B16G16R16		= DXGI_FORMAT_R16G16B16A16_UNORM,
	TEXF_A16B16G16R16F		= DXGI_FORMAT_R16G16B16A16_FLOAT,
	TEXF_R32F				= DXGI_FORMAT_R32_FLOAT,
	TEXF_X8R8G8B8			= DXGI_FORMAT_B8G8R8X8_UNORM,
	TEXF_R5G6B5				= DXGI_FORMAT_B5G6R5_UNORM,
	TEXF_A1R5G5B5			= DXGI_FORMAT_B5G5R5A1_UNORM,
//	TEXF_A4R4G4B4			= DXGI_FORMAT_B4G4R4A4_UNORM,
	TEXF_A2B10G10R10		= DXGI_FORMAT_R10G10B10A2_UNORM,
	TEXF_G16R16				= DXGI_FORMAT_R16G16_UNORM,
	TEXF_A32B32G32R32F		= DXGI_FORMAT_R32G32B32A32_FLOAT,

	TEXF_R8G8B8A8_SRGB		= DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,

	TEXF_R8					= DXGI_FORMAT_R8_UNORM,
	TEXF_R8G8				= DXGI_FORMAT_R8G8_UNORM,
	TEXF_A8					= DXGI_FORMAT_A8_UNORM,
	TEXF_L8					= DXGI_FORMAT_R8_UNORM ,
	TEXF_A8L8				= DXGI_FORMAT_R8G8_UNORM,
	TEXF_L16				= DXGI_FORMAT_R16_UNORM,

	TEXF_DXT1				= DXGI_FORMAT_BC1_UNORM,
	TEXF_DXT3				= DXGI_FORMAT_BC2_UNORM,
	TEXF_DXT5				= DXGI_FORMAT_BC3_UNORM,
};

enum PrimType {
	PRIM_UNKNOWN			= D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
	PRIM_POINTLIST			= D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
	PRIM_LINELIST			= D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
	PRIM_LINESTRIP			= D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
	PRIM_TRILIST			= D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	PRIM_TRISTRIP			= D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,

	PRIM_QUADSTRIP			= PRIM_TRISTRIP,
	//emulated
	PRIM_EMULATED			= 0x1000,
	PRIM_QUADLIST			= PRIM_EMULATED,
	PRIM_RECTLIST,
	PRIM_TRIFAN,
};
inline PrimType StripPrim(PrimType p)		{ return PrimType(p | 1);}
inline PrimType AdjacencyPrim(PrimType p)	{ return PrimType(p | 8);}
inline PrimType PatchPrim(int n)			{ return PrimType(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + n - 1); }

enum BackFaceCull {
	BFC_NONE				= D3D11_CULL_NONE,
	BFC_FRONT				= D3D11_CULL_FRONT,
	BFC_BACK				= D3D11_CULL_BACK,
};

enum DepthTest {
	DT_NEVER				= D3D11_COMPARISON_NEVER,
	DT_LESS					= D3D11_COMPARISON_LESS,
	DT_EQUAL				= D3D11_COMPARISON_EQUAL,
	DT_LEQUAL				= D3D11_COMPARISON_LESS_EQUAL,
	DT_GREATER				= D3D11_COMPARISON_GREATER,
	DT_NOTEQUAL				= D3D11_COMPARISON_NOT_EQUAL,
	DT_GEQUAL				= D3D11_COMPARISON_GREATER_EQUAL,
	DT_ALWAYS 				= D3D11_COMPARISON_ALWAYS,

	DT_USUAL				= DT_GEQUAL,
	DT_CLOSER_SAME			= DT_GEQUAL,
	DT_CLOSER				= DT_GREATER,
};
inline DepthTest operator~(DepthTest b) { return DepthTest(9 - b); }

enum AlphaTest {
	AT_NEVER				= D3D11_COMPARISON_NEVER,
	AT_LESS					= D3D11_COMPARISON_LESS,
	AT_EQUAL				= D3D11_COMPARISON_EQUAL,
	AT_LEQUAL				= D3D11_COMPARISON_LESS_EQUAL,
	AT_GREATER				= D3D11_COMPARISON_GREATER,
	AT_NOTEQUAL				= D3D11_COMPARISON_NOT_EQUAL,
	AT_GEQUAL				= D3D11_COMPARISON_GREATER_EQUAL,
	AT_ALWAYS 				= D3D11_COMPARISON_ALWAYS,
};
inline AlphaTest operator~(AlphaTest b) { return AlphaTest(9 - b); }

enum UVMode {
	U_CLAMP					= D3D11_TEXTURE_ADDRESS_CLAMP,
	V_CLAMP					= D3D11_TEXTURE_ADDRESS_CLAMP	<<4,
	U_WRAP					= D3D11_TEXTURE_ADDRESS_WRAP,			//0
	V_WRAP					= D3D11_TEXTURE_ADDRESS_WRAP	<<4,	//0
	U_MIRROR				= D3D11_TEXTURE_ADDRESS_MIRROR,
	V_MIRROR				= D3D11_TEXTURE_ADDRESS_MIRROR	<<4,

	ALL_CLAMP				= U_CLAMP	| V_CLAMP,
	ALL_WRAP				= U_WRAP	| V_WRAP,
	ALL_MIRROR				= U_MIRROR	| V_MIRROR,
};

enum TexFilter {// mag, min, mip
//	TF_NEAREST_NEAREST_NONE		= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE),
//	TF_NEAREST_LINEAR_NONE		= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_NONE),
	TF_NEAREST_NEAREST_NEAREST	= D3D11_FILTER_MIN_MAG_MIP_POINT,
	TF_NEAREST_LINEAR_NEAREST	= D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT,
	TF_NEAREST_NEAREST_LINEAR	= D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR,
	TF_NEAREST_LINEAR_LINEAR	= D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,

//	TF_LINEAR_NEAREST_NONE		= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_POINT, D3DTEXF_NONE),
//	TF_LINEAR_LINEAR_NONE		= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE),
	TF_LINEAR_NEAREST_NEAREST	= D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
	TF_LINEAR_LINEAR_NEAREST	= D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
	TF_LINEAR_NEAREST_LINEAR	= D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR,
	TF_LINEAR_LINEAR_LINEAR		= D3D11_FILTER_MIN_MAG_MIP_LINEAR,
};

enum ChannelMask {
	CM_RED					= D3D11_COLOR_WRITE_ENABLE_RED,
	CM_GREEN				= D3D11_COLOR_WRITE_ENABLE_GREEN,
	CM_BLUE					= D3D11_COLOR_WRITE_ENABLE_BLUE,
	CM_ALPHA				= D3D11_COLOR_WRITE_ENABLE_ALPHA,
	CM_RGB					= CM_RED | CM_GREEN | CM_BLUE,
	CM_ALL					= CM_RED | CM_GREEN | CM_BLUE | CM_ALPHA,
	CM_NONE					= 0,
};

enum MSAA {
	MSAA_NONE				= 1,
	MSAA_TWO				= 2,
	MSAA_FOUR				= 4,
};

enum CubemapFace {
	CF_POS_X				= D3D11_TEXTURECUBE_FACE_POSITIVE_X,
	CF_NEG_X				= D3D11_TEXTURECUBE_FACE_NEGATIVE_X,
	CF_POS_Y				= D3D11_TEXTURECUBE_FACE_POSITIVE_Y,
	CF_NEG_Y				= D3D11_TEXTURECUBE_FACE_NEGATIVE_Y,
	CF_POS_Z				= D3D11_TEXTURECUBE_FACE_POSITIVE_Z,
	CF_NEG_Z				= D3D11_TEXTURECUBE_FACE_NEGATIVE_Z,
};

enum TexState {
	TS_FILTER,
	TS_ADDRESS_U,
	TS_ADDRESS_V,
	TS_ADDRESS_W,
	TS_MIP_BIAS,
	TS_ANISO_MAX,
	TS_COMPARISON,
	TS_MIP_MIN,
	TS_MIP_MAX,
};

#define TSV_MAXLOD	13

enum RenderTarget {
	RT_COLOUR0,
	RT_COLOUR1,
	RT_COLOUR2,
	RT_COLOUR3,
	RT_NUM,
	RT_DEPTH				= RT_NUM
};

enum BlendOp {
	BLENDOP_ADD				= D3D11_BLEND_OP_ADD,
	BLENDOP_MIN				= D3D11_BLEND_OP_MIN,
	BLENDOP_MAX				= D3D11_BLEND_OP_MAX,
	BLENDOP_SUBTRACT		= D3D11_BLEND_OP_SUBTRACT,
	BLENDOP_REVSUBTRACT		= D3D11_BLEND_OP_REV_SUBTRACT,
};

enum BlendFunc {
	BLEND_ZERO				= D3D11_BLEND_ZERO,
	BLEND_ONE				= D3D11_BLEND_ONE,
	BLEND_SRC_COLOR			= D3D11_BLEND_SRC_COLOR,
	BLEND_INV_SRC_COLOR		= D3D11_BLEND_INV_SRC_COLOR,
	BLEND_DST_COLOR			= D3D11_BLEND_DEST_COLOR,
	BLEND_INV_DST_COLOR		= D3D11_BLEND_INV_DEST_COLOR,
	BLEND_SRC_ALPHA  		= D3D11_BLEND_SRC_ALPHA  ,
	BLEND_INV_SRC_ALPHA		= D3D11_BLEND_INV_SRC_ALPHA,
	BLEND_DST_ALPHA			= D3D11_BLEND_DEST_ALPHA,
	BLEND_INV_DST_ALPHA		= D3D11_BLEND_INV_DEST_ALPHA,
	BLEND_SRC_ALPHA_SATURATE= D3D11_BLEND_SRC_ALPHA_SAT,
	BLEND_CONSTANT_COLOR	= D3D11_BLEND_BLEND_FACTOR,
	BLEND_INV_CONSTANT_COLOR= D3D11_BLEND_INV_BLEND_FACTOR,
//	BLEND_CONSTANT_ALPHA	= D3D11_BLEND_CONSTANTALPHA,
//	BLEND_INV_CONSTANT_ALPHA= D3D11_BLEND_INVCONSTANTALPHA,
};

enum StencilOp {
	STENCILOP_KEEP			= D3D11_STENCIL_OP_KEEP,
	STENCILOP_ZERO			= D3D11_STENCIL_OP_ZERO,
	STENCILOP_REPLACE		= D3D11_STENCIL_OP_REPLACE,
	STENCILOP_INCR			= D3D11_STENCIL_OP_INCR_SAT,
	STENCILOP_INCR_WRAP		= D3D11_STENCIL_OP_INCR,
	STENCILOP_DECR			= D3D11_STENCIL_OP_DECR_SAT,
	STENCILOP_DECR_WRAP		= D3D11_STENCIL_OP_DECR,
	STENCILOP_INVERT		= D3D11_STENCIL_OP_INVERT,
};

enum StencilFunc {
	STENCILFUNC_OFF			= -1,
	STENCILFUNC_NEVER		= D3D11_COMPARISON_NEVER,
	STENCILFUNC_LESS		= D3D11_COMPARISON_LESS,
	STENCILFUNC_LEQUAL		= D3D11_COMPARISON_LESS_EQUAL,
	STENCILFUNC_GREATER		= D3D11_COMPARISON_GREATER,
	STENCILFUNC_GEQUAL		= D3D11_COMPARISON_GREATER_EQUAL,
	STENCILFUNC_EQUAL		= D3D11_COMPARISON_EQUAL,
	STENCILFUNC_NOTEQUAL	= D3D11_COMPARISON_NOT_EQUAL,
	STENCILFUNC_ALWAYS		= D3D11_COMPARISON_ALWAYS,
};
inline StencilFunc operator~(StencilFunc b) { return StencilFunc(9 - b); }

enum FillMode {
	FILL_SOLID				= D3D11_FILL_SOLID,
	FILL_WIREFRAME			= D3D11_FILL_WIREFRAME,
};

struct DrawVerticesArgs : D3D11_DRAW_INSTANCED_INDIRECT_ARGS {
	DrawVerticesArgs()	{}
	DrawVerticesArgs(uint32 start_vert, uint32 num_verts, uint32 start_instance, uint32 num_instances) {
		VertexCountPerInstance	= num_verts;
		InstanceCount			= num_instances;
		StartVertexLocation		= start_vert;
		StartInstanceLocation	= start_instance;
	}
};

struct DrawIndexedVerticesArgs : D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS {
	DrawIndexedVerticesArgs()	{}
	DrawIndexedVerticesArgs(uint32 min_index, uint32 num_verts, uint32 start_index, uint32 num_indices, uint32 start_instance, uint32 num_instances) {
		IndexCountPerInstance	= 0;
		InstanceCount			= num_instances;
		StartIndexLocation		= start_index;
		BaseVertexLocation		= min_index;
		StartInstanceLocation	= start_instance;
	}
};

//-----------------------------------------------------------------------------
//	component types
//-----------------------------------------------------------------------------

typedef	DXGI_FORMAT	ComponentType;
template<typename T> constexpr ComponentType	GetComponentType(const T&)	{ return (ComponentType)_ComponentType<T>::value; }
template<typename T> constexpr ComponentType	GetComponentType()			{ return (ComponentType)_ComponentType<T>::value; }
template<typename T> constexpr TexFormat		GetTexFormat()				{ return TexFormat(_ComponentType<T>::value); }
template<typename T> constexpr ComponentType	GetBufferFormat()			{ return ComponentType(_BufferFormat<_ComponentType<T>::value, sizeof(T)>::value); }
template<typename T> constexpr bool				ValidTexFormat()			{ return _ComponentType<T>::value != DXGI_FORMAT_UNKNOWN; }

//-----------------------------------------------------------------------------
//	Texture/Surface
//-----------------------------------------------------------------------------

class Texture;

ID3D11DeviceContext*	ImmediateContext();
point					GetSize(ID3D11Resource *res);
point3					GetSize3D(ID3D11Resource *res);
uint32					GetMipLevels(ID3D11Resource *res);

struct ResourceData {
	ID3D11DeviceContext			*ctx;
	com_ptr<ID3D11Resource>		res;
	D3D11_MAPPED_SUBRESOURCE	map;

	ResourceData(ResourceData &&b) : ctx(b.ctx), res(move(b.res)), map(b.map) {}
	ResourceData(ID3D11Resource *_res, int sub, D3D11_MAP type, ID3D11DeviceContext *_ctx = ImmediateContext()) : ctx(_ctx), res(_res) {
		res->AddRef();
		if (!CheckResult(ctx->Map(res, sub, type, 0, &map)))
			map.pData = 0;
	}
	ResourceData(ID3D11View *view, int sub, D3D11_MAP type, ID3D11DeviceContext *_ctx = ImmediateContext()) : ctx(_ctx) {
		view->GetResource(&res);
		if (!CheckResult(ctx->Map(res, sub, type, 0, &map)))
			map.pData = 0;
	}
	~ResourceData() {
		ctx->Unmap(res, 0);
	}
	template<typename T> operator T*()	const { return (T*)map.pData; }
};

class Surface : public com_ptr2<ID3D11Resource>	{
	friend Texture;
	friend GraphicsContext;
	friend ComputeContext;
	template<typename T> friend struct SurfaceT;

	uint8	fmt, mip, slice, num_slices;
	void	Init(TexFormat _fmt, int width, int height, Memory loc);
	uint32	CalcSubresource() const	{ return slice == 0 ? mip : GetMipLevels(get()) * slice + mip; }

	template<typename T> struct TypedData : ResourceData, block<T,2> {
		TypedData(ID3D11Resource *res, uint32 sub, const point &size, ID3D11DeviceContext *ctx) : ResourceData(res, sub, D3D11_MAP_READ, ctx), block<T,2>(make_block((T*)map.pData, size.x), map.RowPitch, size.y) {}
	};
public:
	Surface() {}
	Surface(TexFormat fmt, int width, int height, Memory loc = MEM_DEFAULT) {
		Init(fmt, width, height, loc);
	}
	Surface(TexFormat fmt, const point &size, Memory loc = MEM_DEFAULT) {
		Init(fmt, size.x, size.y, loc);
	}
	Surface(IDXGISurface *t) {
		if (t)
			t->QueryInterface(__uuidof(ID3D11Resource), (void**)&*this);
		fmt = mip = slice = num_slices = 0;
	}
	Surface(ID3D11Resource *t, TexFormat fmt = TEXF_UNKNOWN, uint8 mip = 0, uint8 slice = 0, uint8 num_slices = 0)
		: com_ptr2<ID3D11Resource>(t), fmt(fmt), mip(mip), slice(slice), num_slices(num_slices) {
	}
	Surface(ID3D11RenderTargetView *view) {
		D3D11_RENDER_TARGET_VIEW_DESC	desc;
		view->GetResource(&*this);
		view->GetDesc(&desc);
		fmt		= desc.Format;
		mip		= desc.Texture2D.MipSlice;
		slice	= num_slices = 0;
	}
	Surface(ID3D11DepthStencilView *view) {
		D3D11_DEPTH_STENCIL_VIEW_DESC	desc;
		view->GetResource(&*this);
		view->GetDesc(&desc);
		fmt		= desc.Format;
		mip		= desc.Texture2D.MipSlice;
		slice	= num_slices = 0;
	}

	TexFormat	Format()	const { return (TexFormat)fmt; }
	point		Size()		const { return max(GetSize(get()) >> mip, one); }
	point3		Size3D()	const { return max(GetSize3D(get()) >> mip, one); }
	rect		GetRect()	const { return rect(zero, Size()); }
	vol			GetVol()	const { return vol(point3(zero), Size3D()); }

	template<typename T> TypedData<T> Data(ID3D11DeviceContext *ctx = ImmediateContext()) const {
		return {get(), CalcSubresource(), Size(), ctx};
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC DepthDesc() const {
		D3D11_DEPTH_STENCIL_VIEW_DESC	desc;
		desc.Format				= (DXGI_FORMAT)fmt;
		desc.ViewDimension		= D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Flags				= 0;
		desc.Texture2D.MipSlice	= mip;
		switch (fmt) {
			case DXGI_FORMAT_R32G32_UINT:	desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; break;
			case DXGI_FORMAT_R32_FLOAT:		desc.Format = DXGI_FORMAT_D32_FLOAT; break;
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
			case DXGI_FORMAT_R16_UINT:		desc.Format = DXGI_FORMAT_D16_UNORM; break;
		}
		return desc;
	}

	D3D11_RENDER_TARGET_VIEW_DESC RenderDesc() const {
		D3D11_RENDER_TARGET_VIEW_DESC	desc;
		desc.Format				= (DXGI_FORMAT)fmt;
		desc.ViewDimension		= D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice	= mip;
		return desc;
	}

	Surface		As(TexFormat format)		const {
		return Surface(*this, format, mip, slice, num_slices);
	}
//	operator _Texture* ()					const;
};

class Texture : DXwrapper<_Texture, 64> {
	friend void Init(Texture *x, void *physram);

	template<typename T> struct TypedData : ResourceData, block<T,2> {
		TypedData(ID3D11View *view, D3D11_MAP type, const point &size, ID3D11DeviceContext *ctx) : ResourceData(view, 0, type, ctx), block<T,2>(make_block((T*)map.pData, size.x), map.RowPitch, size.y) {}
	};
	struct Info {
		TexFormat format;
		int		width, height, depth, mips;
		point	Size()	const	{ return {width, height}; }
	};
public:
	Texture()	{}
	explicit Texture(const Surface &s);
	explicit Texture(ID3D11ShaderResourceView *srv) { *write() = srv; }
	Texture(TexFormat format, int width, int height, int depth = 1, int mips = 1, Memory loc = MEM_DEFAULT) {
		Init(format, width, height, depth, mips, loc);
	}
	Texture(const Info &info, Memory loc = MEM_DEFAULT) {
		Init(info.format, info.width, info.height, info.depth, info.mips, loc);
	}
	Texture(Texture &&t)					= default;
	Texture(const Texture &t)				= default;
	Texture& operator=(Texture &&t)			= default;
	Texture& operator=(const Texture &t)	= default;

	bool		Init(TexFormat format, int width, int height, int depth = 1, int mips = 1, Memory loc = MEM_DEFAULT);
	bool		Init(TexFormat format, int width, int height, int depth, int mips, Memory loc, void *data, int pitch);
	void		DeInit();
	Info		GetInfo()					const;
	point		Size()						const	{ return GetInfo().Size(); }
	int			Depth()						const;
	bool		IsDepth()					const	{ return false; }

	Surface		GetSurface(int i = 0)			const;
	Surface		GetSurface(CubemapFace f, int i)const;
	Texture		As(TexFormat format)		const;

	_Texture*	operator->()				const	{ return safe(); }
	operator	_Texture*()					const	{ return safe(); }
	operator	Surface()					const	{ return GetSurface(0);	 }
	template<typename T> TypedData<const T>	Data(ID3D11DeviceContext *ctx = ImmediateContext())			const	{ return {safe(), D3D11_MAP_READ, Size(), ctx}; }
	template<typename T> TypedData<T>		WriteData(ID3D11DeviceContext *ctx = ImmediateContext())	const	{ return {safe(), D3D11_MAP_WRITE_DISCARD, Size(), ctx}; }
};

//-----------------------------------------------------------------------------
//	VertexBuffer/IndexBuffer
//-----------------------------------------------------------------------------

class _Buffer {
public:
//protected:
	template<typename T> struct TypedData : ResourceData, block<T,1> {
		TypedData(ID3D11Buffer *b, D3D11_MAP type, uint32 size, ID3D11DeviceContext *ctx)	: ResourceData(b, 0, type, ctx), block<T,1>((T*)map.pData, size) {}
		TypedData(ID3D11View *v, D3D11_MAP type, uint32 size, ID3D11DeviceContext *ctx)		: ResourceData(v, 0, type, ctx), block<T,1>((T*)map.pData, size) {}
	};
	com_ptr<ID3D11Buffer>	b;

	D3D11_BUFFER_DESC GetDesc() const {
		D3D11_BUFFER_DESC	desc;
		b->GetDesc(&desc);
		return desc;
	}
	_Buffer		MakeStaging(ID3D11DeviceContext *ctx) const;

public:
	bool		_Bind(DXGI_FORMAT format, uint32 n, ID3D11ShaderResourceView **srv);

	bool		Init(uint32 size, Memory loc);
	bool		Init(const void *data, uint32 size, Memory loc);
	bool		Init(uint32 n, uint32 stride, Memory loc);
	bool		Init(const void *data, uint32 n, uint32 stride, Memory loc);

//	void*		Begin()						const;
//	void		End()						const;
	uint32		Size()						const		{ return b ? GetDesc().ByteWidth : 0; }

	operator	ID3D11Buffer*()				const		{ return b;	}
	bool		Transfer(const void *data, uint32 size) {
		ResourceData	rd(b, 0, D3D11_MAP_WRITE_NO_OVERWRITE, ImmediateContext());
		memcpy(rd.map.pData, data, size);
		return true;
	}
};

template<D3D11_BIND_FLAG BIND> class _BufferBind : public _Buffer {
public:
	bool	Init(const void *data, uint32 size, Memory loc = MEM_DEFAULT)	{ return data && _Buffer::Init(data, size, Bind(BIND) | loc);	}
	bool	Init(uint32 size, Memory loc = MEM_DEFAULT)						{ return _Buffer::Init(size, Bind(BIND) | loc);	}
	ResourceData	Data(ID3D11DeviceContext *ctx = ImmediateContext())			const { return {MakeStaging(ctx), 0, D3D11_MAP_READ, ctx}; }
	ResourceData	WriteData(ID3D11DeviceContext *ctx = ImmediateContext())	const {	return {b, 0, D3D11_MAP_WRITE_NO_OVERWRITE, ctx}; }
};

typedef _BufferBind<D3D11_BIND_VERTEX_BUFFER>	_VertexBuffer;
typedef _BufferBind<D3D11_BIND_INDEX_BUFFER>	_IndexBuffer;

template<D3D11_BIND_FLAG BIND, typename T> class _BufferTyped : public _BufferBind<BIND> {
	typedef _BufferBind<BIND>	B;
public:
	bool					Init(const T *t, uint32 n, Memory loc = MEM_DEFAULT)	{ return B::Init(t, n * sizeof(T), loc); }
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT)			{ return B::Init(&t, sizeof(t), loc); }
	bool	Init(uint32 n, Memory loc = MEM_DEFAULT)	{ return B::Init(n * sizeof(T), loc);	}
	auto	Begin(uint32 n, Memory loc = MEM_DEFAULT)	{ Init(n, loc | MEM_CPU_WRITE); return WriteData(); }
//	T*		Begin()							const		{ return (T*)B::Begin();	}
	uint32	Size()							const		{ return B::Size() / sizeof(T); }
	_Buffer::TypedData<const T>	Data(ID3D11DeviceContext *ctx = ImmediateContext())			const { return {B::MakeStaging(ctx), D3D11_MAP_READ, Size(), ctx}; }
	_Buffer::TypedData<T>		WriteData(ID3D11DeviceContext *ctx = ImmediateContext())	const {	return {b, D3D11_MAP_WRITE_NO_OVERWRITE, Size(), ctx}; }
};

template<typename T> class VertexBuffer : public _BufferTyped<D3D11_BIND_VERTEX_BUFFER, T> {
public:
	VertexBuffer()															{}
	VertexBuffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)			{ this->Init(t, n, loc);	}
	template<int N> VertexBuffer(const T (&t)[N], Memory loc = MEM_DEFAULT)	{ this->Init(t, loc);		}
};

template<typename T> class IndexBuffer : public _BufferTyped<D3D11_BIND_INDEX_BUFFER, T> {
public:
	IndexBuffer()															{}
	IndexBuffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)				{ this->Init(t, n, loc);	}
	template<int N> IndexBuffer(const T (&t)[N], Memory loc = MEM_DEFAULT)	{ this->Init(t, loc);	}
};

//-----------------------------------------------------------------------------
//	DataBuffer
//-----------------------------------------------------------------------------
#if 0
template<typename T> class Buffer {
	com_ptr<ID3D11ShaderResourceView>	p;
public:
	Buffer()					{}
	Buffer(uint32 n, Memory loc = MEM_DEFAULT)							{ Init(n, loc);	}
	Buffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)				{ Init(t, n, loc);	}
	template<int N> Buffer(const T (&t)[N], Memory loc = MEM_DEFAULT)	{ Init(t, loc);		}

	bool	Init(const T *t, uint32 n, Memory loc = MEM_DEFAULT) {
		_Buffer	b;
		return (GetBufferFormat<T>()
			? b.Init(t, n * sizeof(T), Bind(D3D11_BIND_SHADER_RESOURCE) | loc)
			: b.InitStructured(t, n, sizeof(T), Bind(D3D11_BIND_SHADER_RESOURCE) | loc)
		) && b._Bind(GetBufferFormat<T>(), n, &p);
	}
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT) {
		return Init(&t[0], N, loc);
	}
	bool	Init(uint32 n, Memory loc = MEM_DEFAULT) {
		_Buffer	b;
		return (GetBufferFormat<T>()	
			? b.Init(n * sizeof(T), Bind(D3D11_BIND_SHADER_RESOURCE) | loc)
			: b.InitStructured(n, sizeof(T), Bind(D3D11_BIND_SHADER_RESOURCE) | loc)
		) && b._Bind(GetBufferFormat<T>(), n, &p);
	}
	_Buffer::TypedData<T>	Data(ID3D11DeviceContext *ctx = ImmediateContext())			const { return {p.get(), D3D11_MAP_READ, Size(), ctx}; }
	_Buffer::TypedData<T>	WriteData(ID3D11DeviceContext *ctx = ImmediateContext())	const { return {p.get(), D3D11_MAP_WRITE_DISCARD, Size(), ctx}; }
	operator ID3D11ShaderResourceView*() const { return p; }
#if 1
	T*		Begin(ID3D11DeviceContext *ctx) const {
		com_ptr<ID3D11Resource>		res;
		p->GetResource(&res);
		D3D11_MAPPED_SUBRESOURCE	map;
		return CheckResult(ctx->Map(res, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)) ? (T*)map.pData : 0;
	}
	void	End(ID3D11DeviceContext *ctx) const {
		com_ptr<ID3D11Resource>		res;
		p->GetResource(&res);
		ctx->Unmap(res, 0);
	}
#endif
	uint32	Size() const {
		com_ptr<ID3D11Resource>		res;
		D3D11_BUFFER_DESC			desc;
		p->GetResource(&res);
		((ID3D11Buffer*)res.get())->GetDesc(&desc);
		return desc.ByteWidth / sizeof(T);
	}
};
#endif
class DataBuffer : DXwrapper<ID3D11ShaderResourceView, 64> {
public:
	DataBuffer()									{}
	DataBuffer(ID3D11ShaderResourceView *p)			{ if (*write() = p) p->AddRef(); }
	~DataBuffer();
	operator	ID3D11ShaderResourceView*()	const	{ return safe();	}
	auto		operator->()				const	{ return safe(); }

	bool	Init(TexFormat format, int width, Memory flags = MEM_DEFAULT) {
		_Buffer	b;
		return b.Init(width * DXGI_COMPONENTS((DXGI_FORMAT)format).Bytes(), Bind(D3D11_BIND_SHADER_RESOURCE) | flags)
			&& b._Bind((DXGI_FORMAT)format, width, write());
	}
	bool	Init(const void *data, TexFormat format, int width, Memory flags) {
		_Buffer	b;
		return b.Init(data, width * DXGI_COMPONENTS((DXGI_FORMAT)format).Bytes(), Bind(D3D11_BIND_SHADER_RESOURCE) | flags)
			&& b._Bind((DXGI_FORMAT)format, width, write());
	}
	bool	Init(stride_t stride, uint32 width, Memory flags = MEM_DEFAULT) {
		_Buffer	b;
		if (flags & MEM_INDIRECTARG) {
			return b.Init(width * stride, flags)
				&& b._Bind(DXGI_FORMAT_R32_UINT, width, write());
		} else {
			return b.Init(width, stride, Bind(D3D11_BIND_SHADER_RESOURCE) | flags)
				&& b._Bind(DXGI_FORMAT_UNKNOWN, width, write());
		}
	}
	bool	Init(const void *data, stride_t stride, uint32 width, Memory flags = MEM_DEFAULT) {
		_Buffer	b;
		if (flags & MEM_INDIRECTARG) {
			return b.Init(data, width * stride, flags)
				&& b._Bind(DXGI_FORMAT_R32_UINT, width, write());
		} else {
			return b.Init(data, width, stride, Bind(D3D11_BIND_SHADER_RESOURCE) | flags)
				&& b._Bind(DXGI_FORMAT_UNKNOWN, width, write());
		}
	}

	uint32	Size()	const	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		safe()->GetDesc(&desc);
		return desc.Buffer.NumElements;
	}
	auto	Resource() const {
		com_ptr<ID3D11Resource>		res;
		safe()->GetResource(&res);
		return res;
	}

	template<typename T> _Buffer::TypedData<T>	Data(ID3D11DeviceContext *ctx = ImmediateContext())			const	{ return {safe(), D3D11_MAP_READ, Size(), ctx}; }
	template<typename T> _Buffer::TypedData<T>	WriteData(ID3D11DeviceContext *ctx = ImmediateContext())	const	{ return {safe(), D3D11_MAP_WRITE_DISCARD, Size(), ctx}; }
};

struct ConstBuffer : _Buffer {
	uint32		size;
	void*		data;

	ConstBuffer() : size(0), data(0) {}
	ConstBuffer(uint32 size) : size(size), data(0) {
		Init(align(size, 16), Bind(D3D11_BIND_CONSTANT_BUFFER) | MEM_CPU_WRITE);
	}
	bool	SetConstant(int offset, const void* _data, size_t _size) {
		ISO_ASSERT(offset >= 0 && _size > 0 && offset + _size <= size);
		if (!data) {
			D3D11_MAPPED_SUBRESOURCE map;
			data = CheckResult(ImmediateContext()->Map(b, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)) ? map.pData : 0;
		}
		//data	= Begin();
		memcpy((uint8*)data + offset, _data, _size);
		return true;
	}
	bool	FixBuffer();
	void*	Data();
};

//-----------------------------------------------------------------------------
//	Vertex
//-----------------------------------------------------------------------------

struct VertexElement {
	int				offset;
	ComponentType	type;
	uint32			usage;
	int				stream;

	VertexElement()		{}
	constexpr VertexElement(int offset, ComponentType type, uint32 usage, int stream = 0) : offset(offset), type(type), usage(usage), stream(stream) {}
	template<typename T> constexpr VertexElement(int offset, uint32 usage, int stream = 0) : offset(offset), type(GetComponentType<T>()), usage(usage), stream(stream) {}
	template<typename B, typename T> constexpr VertexElement(T B::* p, uint32 usage, int stream = 0) : offset(intptr_t(get_ptr(((B*)0)->*p))), type(GetComponentType<T>()), usage(usage), stream(stream) {}
	void	SetUsage(uint32 _usage) { usage = _usage; }
};

struct VertexDescription {
	com_ptr<ID3D11InputLayout>	input;
	bool	 Init(D3D11_INPUT_ELEMENT_DESC *ve, uint32 n, const void *vs);
	bool	 Init(const VertexElement *ve, uint32 n, const void *vs);
	bool	 Init(const VertexElement *ve, uint32 n, const DX11Shader *s)	{ return Init(ve, n, s->sub[SS_VERTEX].raw()); }

	VertexDescription()											{}
	VertexDescription(ID3D11InputLayout *input) : input(input)	{}
	VertexDescription(VertexElements ve, const void *vs)		{ Init(ve.p, (uint32)ve.n, vs); }
	operator ID3D11InputLayout*()	const						{ return input; }
};

template<typename T> ID3D11InputLayout	*GetVD(const void *vs)	{ static VertexDescription vd(GetVE<T>(), vs); return vd; }

//-----------------------------------------------------------------------------
//	PixelShader/VertexShader
//-----------------------------------------------------------------------------

enum ShaderParamType {
	SPT_VAL				= 0,
	SPT_SAMPLER			= 1,
	SPT_RESOURCE		= 2,
	SPT_WRITE_RESOURCE	= 3,
	SPT_COUNT			= 4,
};

struct ShaderReg {
	union {
		struct {uint64 offset:24, buffer:8, count:24, stage:3, type:2, unused:2, indirect:1;};
		uint64	u;
	};
	ShaderReg()				: u(0)	{}
	ShaderReg(uint64 _u)	: u(_u)	{}
	ShaderReg(uint32 _offset, uint32 _count, uint8 _buffer, ShaderStage _stage, ShaderParamType _type)
		: offset(_offset), buffer(_buffer), count(_count), stage(_stage), type(_type), indirect(0)
	{}
	operator uint64() const	{ return u;	}
};

class ShaderParameterIterator {
	typedef dx::DXBC::BlobT<dx::DXBC::ResourceDef>	RDEF;
	const DX11Shader	&shader;
	int					stage;
	int					cbuff_index;
	const dx::RD11		*rdef;
	range<stride_iterator<RDEF::Variable> >	vars;
	range<stride_iterator<RDEF::Binding> >	bindings;
	const char			*name;
	const void			*val;
	D3D11_SAMPLER_DESC	sampler;
	ShaderReg			reg;
	fixed_string<64>	temp_name;

	int				ArrayCount(const char *begin, const char *&end);
	void			Next();
public:
	ShaderParameterIterator(const DX11Shader &s) : shader(s), stage(-1), vars(empty), bindings(empty) { Next(); }
	ShaderParameterIterator(const DX11Shader &s, ID3D11DeviceContext*) : ShaderParameterIterator(s) {}
	ShaderParameterIterator& Reset();
	ShaderParameterIterator& operator++()	{ Next(); return *this;		}

	const char		*Name()			const	{ return name;	}
	const void		*Default()		const	{ return val;	}
	const void		*DefaultPerm()	const	{ return val;	}
	ShaderReg		Reg()			const	{ return reg; }
	operator		bool()			const	{ return stage < SS_COUNT;	}
	int				Total()			const;
};
//-----------------------------------------------------------------------------
//	Graphics
//-----------------------------------------------------------------------------

struct FrameAllocator : atomic<circular_allocator> {
	void		*ends[2];
	FrameAllocator() {}
	FrameAllocator(const memory_block &m) {
		init(m);
	}
	void	init(const memory_block &m) {
		atomic<circular_allocator>::init(m);
		for (int i = 0; i < num_elements(ends); i++)
			ends[i] = m;
	}

	void	BeginFrame(int index)	{ set_get(ends[index]); }
	void	EndFrame(int index)		{ ends[index] = getp(); }
};

struct packed_D3D11_BLEND_DESC {
	struct packed_D3D11_RENDER_TARGET_BLEND_DESC {
		uint32 BlendEnable:1, SrcBlend:5, DestBlend:5, BlendOp:3, SrcBlendAlpha:5, DestBlendAlpha:5, BlendOpAlpha:3, RenderTargetWriteMask:4, spare:1;
		void operator=(const D3D11_RENDER_TARGET_BLEND_DESC &desc) {
			BlendEnable				= desc.BlendEnable;
			SrcBlend				= desc.SrcBlend;
			DestBlend				= desc.DestBlend;
			BlendOp					= desc.BlendOp;
			SrcBlendAlpha			= desc.SrcBlendAlpha;
			DestBlendAlpha			= desc.DestBlendAlpha;
			BlendOpAlpha			= desc.BlendOpAlpha;
			RenderTargetWriteMask	= desc.BlendOpAlpha;
			spare					= 0;
		}
	} RenderTarget[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];

	packed_D3D11_BLEND_DESC(const D3D11_BLEND_DESC &desc) {
		for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
			RenderTarget[i] = desc.RenderTarget[i];
		RenderTarget[0].spare = desc.AlphaToCoverageEnable;
		RenderTarget[1].spare = desc.IndependentBlendEnable;
	}
};

struct packed_D3D11_DEPTH_STENCIL_DESC {
	struct packed_D3D11_DEPTH_STENCILOP_DESC {
		uint16	StencilFailOp:4, StencilDepthFailOp:4, StencilPassOp:4, StencilFunc:4;
		packed_D3D11_DEPTH_STENCILOP_DESC(const D3D11_DEPTH_STENCILOP_DESC &desc) :
			StencilFailOp(desc.StencilFailOp),
			StencilDepthFailOp(desc.StencilDepthFailOp),
			StencilPassOp(desc.StencilPassOp),
			StencilFunc(desc.StencilFunc)
		{}
	};

	uint8	StencilReadMask, StencilWriteMask;
	packed_D3D11_DEPTH_STENCILOP_DESC FrontFace, BackFace;
	uint16	DepthEnable:1, DepthWriteMask:1, DepthFunc:3, StencilEnable:1, spare:10;

	packed_D3D11_DEPTH_STENCIL_DESC(const D3D11_DEPTH_STENCIL_DESC &desc) :
		StencilReadMask(desc.StencilReadMask),
		StencilWriteMask(desc.StencilWriteMask),
		FrontFace(desc.FrontFace),
		BackFace(desc.BackFace),
		DepthEnable(desc.DepthEnable),
		DepthWriteMask(desc.DepthWriteMask),
		DepthFunc(desc.DepthFunc),
		StencilEnable(desc.StencilEnable),
		spare(0)
	{}
};

struct packed_D3D11_RASTERIZER_DESC {
	uint32 FillMode:1, CullMode:2, FrontCounterClockwise:1, DepthClipEnable:1, ScissorEnable:1, MultisampleEnable:1, AntialiasedLineEnable:1, DepthBias:24;
	float DepthBiasClamp, SlopeScaledDepthBias;
	packed_D3D11_RASTERIZER_DESC(const D3D11_RASTERIZER_DESC &desc) :
		FillMode(desc.FillMode),
		CullMode(desc.CullMode),
		FrontCounterClockwise(desc.FrontCounterClockwise),
		DepthClipEnable(desc.DepthClipEnable),
		ScissorEnable(desc.ScissorEnable),
		MultisampleEnable(desc.MultisampleEnable),
		AntialiasedLineEnable(desc.AntialiasedLineEnable),
		DepthBias(desc.DepthBias),
		DepthBiasClamp(desc.DepthBiasClamp),
		SlopeScaledDepthBias(desc.SlopeScaledDepthBias)
	{}
};

inline uint64 hash(const D3D11_BLEND_DESC &desc)			{ return FNV1<uint64>(packed_D3D11_BLEND_DESC(desc)); }
inline uint64 hash(const D3D11_DEPTH_STENCIL_DESC &desc)	{ return FNV1<uint64>(packed_D3D11_DEPTH_STENCIL_DESC(desc)); }
inline uint64 hash(const D3D11_RASTERIZER_DESC &desc)		{ return FNV1<uint64>(packed_D3D11_RASTERIZER_DESC(desc)); }

struct RenderWindow;

class Graphics {
	friend GraphicsContext;
	friend ComputeContext;
	friend class Fence;

	struct CBKey {
		union {
			uint64	v;
			struct {
				crc32	id;
				uint16	size, num;
			};
		};
		CBKey(crc32 id, uint32 size, uint32 num) : id(id), size(size), num(num) {}
	};

	com_ptr<ID3D11Device>			device;
	com_ptr<ID3D11DeviceContext>	context;
	hash_map<CBKey, ConstBuffer*>	cb;
	hash_map<ID3D11ShaderResourceView*, com_ptr<ID3D11UnorderedAccessView>, false, 4>	uav_map;
	FrameAllocator					fa;
	int								frame;
public:
	enum FEATURE {
		COMPUTE,
		DOUBLES,
		VPRT,		// ability to set the render target array index from the vertex shader stage
	};

	class Display {
	protected:
		TexFormat					format;
		int							width, height;
		com_ptr<ID3D11Texture2D>	disp;
		com_ptr<IDXGISwapChain1>	swapchain;
		void SetFormat(TexFormat _format) {
			if (format != _format) {
				format = _format;
				swapchain.clear();
			}
		}
	public:
		Display() : format(TEXF_A8B8G8R8), width(0), height(0) {}
		~Display() {}
		bool				SetFormat(const RenderWindow *window, const point &size, TexFormat _format);
		bool				SetSize(const RenderWindow *window, const point &size) { return SetFormat(window, size, format); }
		point				Size()				const	{ return {width, height}; }
		Surface				GetDispSurface()	const	{ return disp.get(); }
		IDXGISwapChain*		GetSwapChain()		const	{ return swapchain; }
		ID3D11Texture2D*	GetTex()			const	{ return disp; }
		void				MakePresentable(GraphicsContext &ctx) const {}
		bool				Present()			const;
		bool				Present(const RECT &rect) const;
	};

	struct StageState {
		const void		*raw;
		uint16			dirty;
		ConstBuffer*	buffers[16];

		StageState()									{ Clear(); }
		void			Set(ConstBuffer *b, uint32 i)	{ buffers[i] = b; }
		ConstBuffer*	Get(uint32 i)	const			{ return buffers[i];	}

		int Flush(ID3D11DeviceContext *context, ID3D11Buffer **buffs) {
			int	i = 0;
			for (int m = dirty; m; m >>= 1, i++)
				buffs[i] = (m & 1) && buffers[i]->FixBuffer() ? (ID3D11Buffer*)*buffers[i] : 0;
			dirty = 0;
			return i;
		}
		bool Init(const void *_raw);
		void Clear()									{ clear(buffers); dirty = 0; raw = 0; }
	};

	struct UAVS : dynamic_array<com_ptr2<ID3D11UnorderedAccessView>> {
		inline	ID3D11UnorderedAccessView**	Get(int i) {
			if (size() <= i)
				resize(i + 1);
			else
				(*this)[i].clear();
			return &(*this)[i];
		}

		inline	void	Set(ID3D11UnorderedAccessView *uav, int i) {
			*Get(i) = uav;
		}
		inline	bool	Set(ID3D11Resource *res, TexFormat format, int i);
		inline	bool	Set(ID3D11ShaderResourceView *srv, int i);

		void	FlushCS(ID3D11DeviceContext *ctx) {
			if (uint32 n = size32()) {
				auto	counts = alloc_auto(uint32, n);
				memset(counts, 0, sizeof(uint32*) * n);
				ctx->CSSetUnorderedAccessViews(0, n, (ID3D11UnorderedAccessView**)begin(), counts);
				while (n-- && !back())
					pop_back();
			}
		}
		void	ClearCS(ID3D11DeviceContext *ctx) {
			for (auto &i : *this)
				i.clear();
		}

	};

	flags<FEATURE>		features;

	bool				Remove(ID3D11ShaderResourceView *srv) {
		if (auto *p = uav_map.remove(srv)) {
			p->clear();
			return true;
		}
		return false;
	}
	ID3D11UnorderedAccessView*	GetUAV(ID3D11ShaderResourceView *srv)	{ return get(uav_map[srv].get()); }
	bool				MakeUAV(ID3D11Buffer *b, TexFormat format, ID3D11UnorderedAccessView **uav);
	bool				MakeUAV(ID3D11Resource *res, TexFormat format, ID3D11UnorderedAccessView **uav);
	bool				MakeUAV(ID3D11ShaderResourceView *srv, ID3D11UnorderedAccessView **uav);

	static void*		alloc(size_t size, size_t align)				{ return aligned_alloc(size, align); }
	static void*		realloc(void *p, size_t size, size_t align)		{ return aligned_realloc(p, size, align);	}
	static bool			free(void *p)									{ aligned_free(p); return true; }
	static bool			free(void *p, size_t size)						{ aligned_free(p); return true; }
	static void			transfer(void *d, const void *s, size_t size)	{ memcpy(d, s, size);	}
	static uint32		fix(void *p, size_t size)						{ return 0; }
	static void*		unfix(uint32 p)									{ ISO_ALWAYS_CHEAPASSERT(0); return 0; }

	hash_map<D3D11_BLEND_DESC,			ID3D11BlendState*>			blend_cache;
	hash_map<D3D11_DEPTH_STENCIL_DESC,	ID3D11DepthStencilState*>	depthstencil_cache;
	hash_map<D3D11_RASTERIZER_DESC,		ID3D11RasterizerState*>		raster_cache;

	ID3D11BlendState		*GetBlendObject(const D3D11_BLEND_DESC &desc) {
		auto &obj = blend_cache[desc].put();
		if (!obj)
			device->CreateBlendState(&desc, &obj);
		return obj;
	}

	ID3D11DepthStencilState	*GetDepthStencilObject(const D3D11_DEPTH_STENCIL_DESC &desc) {
		auto &obj = depthstencil_cache[desc].put();
		if (!obj)
			device->CreateDepthStencilState(&desc, &obj);
		return obj;
	}

	ID3D11RasterizerState	*GetRasterObject(const D3D11_RASTERIZER_DESC &desc) {
		auto &obj = raster_cache[desc].put();
		if (!obj)
			device->CreateRasterizerState(&desc, &obj);
		return obj;
	}

	Graphics();
	~Graphics();

	bool					Init(IDXGIAdapter *adapter = 0);
	bool					Init(HWND hWnd, IDXGIAdapter *adapter = 0) { return Init(adapter); }
	ID3D11Device*			GetDevice()					{ if (frame == -1) Init(); return device;	}
	ID3D11Device*			Device()			const	{ return device;	}
	ID3D11DeviceContext*	Context()			const	{ return context;	}
	ConstBuffer*			FindConstBuffer(crc32 id, uint32 size, uint32 num);
	void					BeginScene(GraphicsContext &ctx);
	void					EndScene(GraphicsContext &ctx);
	void					Trim();

	ID3D11Resource*				MakeTextureResource(TexFormat format, int width, int height, int depth, int mips, Memory loc, const D3D11_SUBRESOURCE_DATA *init_data = 0);
	ID3D11ShaderResourceView*	MakeTextureView(ID3D11Resource *tex, TexFormat format, int width, int height, int depth, int mips, Memory loc);
	ID3D11ShaderResourceView*	MakeTexture(TexFormat format, int width, int height, int depth, int mips, Memory loc, const D3D11_SUBRESOURCE_DATA *init_data = 0);
	ID3D11ShaderResourceView*	MakeDataBuffer(TexFormat format, uint32 count, stride_t stride, void *data);
	ID3D11ShaderResourceView*	MakeTextureView(ID3D11Resource *tex, TexFormat format);
};

extern Graphics graphics;

inline bool Graphics::UAVS::Set(ID3D11Resource *res, TexFormat format, int i) {
	return res && graphics.MakeUAV(res, format, Get(i));
}
inline bool Graphics::UAVS::Set(ID3D11ShaderResourceView *srv, int i) {
	return srv && graphics.MakeUAV(srv, Get(i));
}

class Fence : com_ptr<ID3D11Query> {
public:
	Fence() {}
	Fence(ID3D11Query *p)  : com_ptr<ID3D11Query>(p) {}

	void	Wait() const {
		if (get()) {
			BOOL	data;
		#if 0
			int		wait	= 0;
			timer	t;
			while (graphics.context->GetData(f, &data, sizeof(BOOL), 0) != S_OK)
				++wait;
			ISO_TRACEF("waited ") << t << " s\n";
		#else
			while (graphics.context->GetData(get(), &data, sizeof(BOOL), 0) != S_OK) {}
		#endif
		}
	}
};

//-----------------------------------------------------------------------------
//	ComputeContext
//-----------------------------------------------------------------------------

class ComputeContext {
	friend Graphics;

	ID3D11DeviceContext		*context;
	Graphics::StageState	cs_state;
	Graphics::UAVS			uavs;

public:
	ComputeContext() : context(0) {}
	~ComputeContext()	{
		if (context && context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
			DingDong();
			context->Release();
		}
	}

	operator ID3D11DeviceContext*() const			{ return context; }
	ID3D11DeviceContext *Context()	const			{ return context;	}
	FrameAllocator&		allocator()					{ return graphics.fa; }

	bool				Begin();
	void				End();
	void				DingDong();
//	void				Reset();

	void				PushMarker(const char *s)	{}
	void				PopMarker()					{}

	Fence	PutFence()	{
		ID3D11Query			*query;
		D3D11_QUERY_DESC	desc = {D3D11_QUERY_EVENT, 0};
		CheckResult(graphics.Device()->CreateQuery(&desc, &query));
		graphics.context->End(query);
		return query;
	}

	void	Blit(const Surface &dest, const Surface &srce, const point &dest_pos, const rect &srce_rect) {
		D3D11_BOX	box = {
			(uint32)srce_rect.a.x,	(uint32)srce_rect.a.y,	0,
			(uint32)srce_rect.b.x,	(uint32)srce_rect.b.y,	1
		};
		context->CopySubresourceRegion(dest, dest.mip, dest_pos.x, dest_pos.y, 0, srce, srce.mip, &box);
	}
	void	Blit(const Surface& dest, const Surface& srce, const rect& dest_rect) {
		Blit(dest, srce, dest_rect.a, rect(zero, dest_rect.extent()));
	}
	void	Blit(const Surface& dest, const Surface& srce, const point& dest_pos) {
		Blit(dest, srce, dest_pos, srce.GetRect());
	}
	void	Blit(const Surface &dest, const Surface &srce)	{
		context->CopyResource(dest, srce);
	}
	void	PutCount(ID3D11UnorderedAccessView *src, ID3D11Buffer *dst, uint32 offset) {
		context->CopyStructureCount(dst, offset, src);
	}

	force_inline	void	SetBuffer	(ID3D11ShaderResourceView *srv, int i = 0)	{ context->CSSetShaderResources(i, 1, &srv); }
	force_inline	void	SetTexture	(ID3D11ShaderResourceView *srv, int i = 0)	{ context->CSSetShaderResources(i, 1, &srv); }
	force_inline	void	SetRWBuffer	(ID3D11Buffer *res, TexFormat format, int i = 0)	{ uavs.Set(res, format, i); }
	force_inline	void	SetRWBuffer	(ID3D11ShaderResourceView *srv, int i = 0)	{ uavs.Set(srv, i); }
	force_inline	void	SetRWTexture(ID3D11ShaderResourceView *srv, int i = 0)	{ uavs.Set(srv, i); }

	force_inline	void	SetConstBuffer(ConstBuffer &buffer, int i = 0) {
		buffer.FixBuffer();
		ID3D11Buffer *b = buffer;
		context->CSSetConstantBuffers(0, 1, &b);
	}

	void				SetShader(const DX11Shader &s);
	void				SetShaderConstants(ShaderReg reg, const void *values);

	force_inline void	Dispatch(uint32 dimx, uint32 dimy = 1, uint32 dimz = 1) {
		if (cs_state.dirty) {
			ID3D11Buffer *buffs[16];
			context->CSSetConstantBuffers(0, cs_state.Flush(context, buffs), buffs);
		}
		uavs.FlushCS(context);
		context->Dispatch(dimx, dimy, dimz);
		uavs.ClearCS(context);
		uavs.FlushCS(context);
	}
};

class ComputeQueue : public ComputeContext {
public:
	ComputeQueue() { Begin(); }
};

//-----------------------------------------------------------------------------
//	GraphicsContext
//-----------------------------------------------------------------------------

class GraphicsContext {
	friend Graphics;
	friend Application;
	friend class _ImmediateStream;

	enum {MAX_SAMPLERS = 8};
	ID3D11DeviceContext					*context;
	D3D11_VIEWPORT						viewport;

	com_ptr<ID3D11RenderTargetView>		render_buffers[4];
	com_ptr<ID3D11DepthStencilView>		depth_buffer;
	int									num_render_buffers;

	D3D11_RASTERIZER_DESC				raster;
	com_ptr2<ID3D11RasterizerState>		rasterstate;

	D3D11_BLEND_DESC					blend;
	com_ptr2<ID3D11BlendState>			blendstate;
	colour								blendfactor;

	D3D11_DEPTH_STENCIL_DESC			depth;
	com_ptr2<ID3D11DepthStencilState>	depthstate;
	uint8								stencil_ref;

	D3D11_SAMPLER_DESC					samplers[MAX_SAMPLERS];
	com_ptr2<ID3D11SamplerState>		samplerstates[MAX_SAMPLERS];
	Graphics::StageState				stage_states[SS_COUNT];
	Graphics::UAVS						uavs;

	PrimType	prev_prim	= PRIM_UNKNOWN;
	bool		is_compute	= false;

	template<typename T> struct AllocBuffer {
		T		buffer;
		uint32	index, size;
		AllocBuffer() : index(0), size(0) {}

		uint32	alloc(uint32 count, uint32 align = 4) {
			if (count * 2 > size) {
				size = max(count * 3, 1024);
				buffer.Init(size, MEM_CPU_WRITE);
				index = 0;
			} else if (iso::align(index, align) + count > size) {
				index = 0;
			}
			uint32	r = iso::align(index, align);
			index = r + count;
			return r;
		}
	};

	AllocBuffer<_VertexBuffer>			immediate_vb;
	AllocBuffer<IndexBuffer<uint16>>	immediate_ib;

	enum UPDATE {
		_UPD_SAMPLER		= 1 << 0,
		UPD_BLEND			= 1 << 8,
		UPD_BLENDFACTOR		= 1 << 9,
		UPD_DEPTH			= 1 << 10,
		UPD_STENCIL_REF		= 1 << 11,
		UPD_RASTER			= 1 << 12,
		UPD_TARGETS			= 1 << 13,
		UPD_CBS				= 1 << 16,

		UPD_CBS_PIXEL		= UPD_CBS << SS_PIXEL,
		UPD_CBS_VERTEX		= UPD_CBS << SS_VERTEX,
		UPD_CBS_GEOMETRY	= UPD_CBS << SS_GEOMETRY,
		UPD_CBS_HULL		= UPD_CBS << SS_HULL,
		UPD_CBS_LOCAL		= UPD_CBS << SS_LOCAL,
		//UPD_CBS_COMPUTE		= UPD_CBS << SS_COMPUTE,
	};
	flags<UPDATE>		update;

	void				FlushDeferred2();
	inline void			FlushDeferred()	{
		if (update)
			FlushDeferred2();
	}
	inline void			ClearTargets() {
		for (int i = 0; i < num_render_buffers; i++)
			render_buffers[i].clear();
	}
	inline void			FlushTargets()	const {
		context->OMSetRenderTargetsAndUnorderedAccessViews(
			num_render_buffers, (ID3D11RenderTargetView**)render_buffers, depth_buffer,
			num_render_buffers, uavs.size32(), (ID3D11UnorderedAccessView**)uavs.begin(), 0
		);
	}

	inline int	Prim2Verts(PrimType prim, uint32 count) {
		//									  PL LL LS TL TS	  Adj: PL LL LS TL TS
		static const	uint8 mult[]	= {0, 1, 2, 1, 3, 1, 0, 0,	0, 0, 4, 1, 6, 2, 0, 0};
		static const	uint8 add[]		= {0, 0, 0, 1, 0, 2, 0, 0,	0, 0, 0, 3, 0, 4, 0, 0};
		return count * mult[prim] + add[prim];
	}

	inline void	SetSRV(ShaderStage stage, ID3D11ShaderResourceView *srv, int i) {
		switch (stage) {
			case SS_PIXEL:		context->PSSetShaderResources(i, 1, &srv); break;
			case SS_VERTEX:		context->VSSetShaderResources(i, 1, &srv); break;
			case SS_GEOMETRY:	context->GSSetShaderResources(i, 1, &srv); break;
			case SS_HULL:		context->HSSetShaderResources(i, 1, &srv); break;
			case SS_LOCAL:		context->DSSetShaderResources(i, 1, &srv); break;
			case SS_COMPUTE:	context->CSSetShaderResources(i, 1, &srv); break;
		}
	}

	inline void	SetUAV(ShaderStage stage, ID3D11UnorderedAccessView *uav, int i) {
		uavs.Set(uav, i);
		if (!is_compute)
			update.set(UPD_TARGETS);
	}
	inline void	SetUAV(ShaderStage stage, ID3D11Resource *res, TexFormat format, int i) {
		uavs.Set(res, format, i);
		if (!is_compute)
			update.set(UPD_TARGETS);
	}
	inline void	SetUAV(ShaderStage stage, ID3D11ShaderResourceView *srv, int i) {
		uavs.Set(srv, i);
		if (!is_compute)
			update.set(UPD_TARGETS);
	}
	
	inline void	SetPrim(PrimType prim) {
		if (prim != prev_prim) {
			context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)prim);
			prev_prim = prim;
		}
	}

	void				_Begin();
public:
	GraphicsContext() : context(0)					{}
	operator ID3D11DeviceContext*()		const		{ return context; }
	ID3D11DeviceContext* operator->()	const		{ return context; }
	ID3D11DeviceContext* Context()		const		{ return context; }
	FrameAllocator&		allocator()					{ return graphics.fa; }

	void				Begin();
	void				End();

	void				PushMarker(const char *s)	{}
	void				PopMarker()					{}

	void				SetWindow(const rect &rect);
	rect				GetWindow();
	void				Clear(param(colour) col, bool zbuffer = true);
	void				ClearZ();
	void				SetMSAA(MSAA _msaa)	{}

	Fence	PutFence()	{
		ID3D11Query			*query;
		D3D11_QUERY_DESC	desc = {D3D11_QUERY_EVENT, 0};
		CheckResult(graphics.Device()->CreateQuery(&desc, &query));
		graphics.context->End(query);
		return query;
	}

	void	Blit(const Surface &dest, const Surface &srce, const point &dest_pos, const rect &srce_rect) {
		D3D11_BOX	box = {
			(uint32)srce_rect.a.x,	(uint32)srce_rect.a.y,	0,
			(uint32)srce_rect.b.x,	(uint32)srce_rect.b.y,	1
		};
		context->CopySubresourceRegion(dest, dest.CalcSubresource(), dest_pos.x, dest_pos.y, 0, srce, srce.CalcSubresource(), &box);
	}
	void	Blit(const Surface &dest, const Surface &srce, const point3 &dest_pos, const vol &srce_vol) {
		D3D11_BOX	box = {
			(uint32)srce_vol.a.x,	(uint32)srce_vol.a.y,	(uint32)srce_vol.a.z,
			(uint32)srce_vol.b.x,	(uint32)srce_vol.b.y,	(uint32)srce_vol.b.z
		};
		context->CopySubresourceRegion(dest, dest.CalcSubresource(), dest_pos.x, dest_pos.y, dest_pos.z, srce, srce.CalcSubresource(), &box);
	}
	void	Blit(const Surface &dest, const Surface &srce, const point &dest_pos)	{
		Blit(dest, srce, dest_pos, srce.GetRect());
	}
	void	Blit(const Surface &dest, const Surface &srce)	{
		point	size = srce.Size();
		if (any(size != dest.Size()))
			Blit(dest, srce, zero);
		else
			context->CopySubresourceRegion(dest, dest.CalcSubresource(), 0, 0, 0, srce, srce.CalcSubresource(), NULL);
	}
	void	PutCount(const DataBuffer &src, const DataBuffer &dst, uint32 offset) {
		com_ptr<ID3D11Resource>				dst_res;
		dst->GetResource(&dst_res);
		context->CopyStructureCount(static_cast<ID3D11Buffer*>(dst_res.get()), offset, graphics.GetUAV(src));
	}

	bool				Enabled(ShaderStage stage) const {
		return !!stage_states[stage].raw;
	}

	// render targets
	void				SetRenderTarget(const Surface& s, RenderTarget i = RT_COLOUR0);
	void				SetZBuffer(const Surface& s);

	force_inline	Surface	GetRenderTarget(int i = 0) {
		if (i == RT_DEPTH)
			return depth_buffer.get();
		else
			return render_buffers[i].get();
	}
	force_inline	Surface	GetZBuffer() {
		return depth_buffer.get();
	}

	force_inline	void	SetBuffer(ShaderStage stage, ID3D11ShaderResourceView *srv, int i = 0)		{ SetSRV(stage, srv, i); }
	force_inline	void	SetRWBuffer(ShaderStage stage, ID3D11Buffer *res, TexFormat format, int i = 0)	{ SetUAV(stage, res, format, i); }
	force_inline	void	SetRWBuffer(ShaderStage stage, ID3D11ShaderResourceView *srv, int i = 0)	{ SetUAV(stage, srv, i); }
	force_inline	void	SetTexture(ShaderStage stage, ID3D11ShaderResourceView *srv, int i = 0)		{ SetSRV(stage, srv, i); }
	force_inline	void	SetRWTexture(ShaderStage stage, ID3D11ShaderResourceView *srv, int i = 0)	{ SetUAV(stage, srv, i); }

	void SetBuffer(ShaderStage stage, const _Buffer &b, int i) {
		auto	desc	= b.GetDesc();
		ISO_ASSERT(desc.StructureByteStride);
		D3D11_SHADER_RESOURCE_VIEW_DESC view;
		view.Format					= DXGI_FORMAT_UNKNOWN;
		view.ViewDimension			= D3D11_SRV_DIMENSION_BUFFER;
		view.Buffer.FirstElement	= 0;
		view.Buffer.NumElements		= desc.ByteWidth / desc.StructureByteStride;
		com_ptr<ID3D11ShaderResourceView>	srv;
		graphics.Device()->CreateShaderResourceView(b, &view, &srv);
		SetBuffer(stage, srv, i);
	}
	
	void SetBuffer(ShaderStage stage, const _Buffer &b, TexFormat format, int i) {
		auto	desc	= b.GetDesc();
		ISO_ASSERT(desc.StructureByteStride == 0);
		D3D11_SHADER_RESOURCE_VIEW_DESC view;
		view.Format					= (DXGI_FORMAT)format;
		view.ViewDimension			= D3D11_SRV_DIMENSION_BUFFER;
		view.Buffer.FirstElement	= 0;
		view.Buffer.NumElements		= desc.ByteWidth / DXGI_COMPONENTS((DXGI_FORMAT)format).Bytes();
		com_ptr<ID3D11ShaderResourceView>	srv;
		graphics.Device()->CreateShaderResourceView(b, &view, &srv);
		SetBuffer(stage, srv, i);
	}

	force_inline	void	SetBuffer(ShaderStage stage, const DataBuffer &buf, int i = 0)				{ SetSRV(stage, buf, i); }
	force_inline	void	SetRWBuffer(ShaderStage stage, const DataBuffer &buf, int i = 0)			{ SetUAV(stage, buf, i); }
	force_inline	void	SetTexture(ShaderStage stage, const Texture &tex, int i = 0)				{ SetSRV(stage, tex, i); }
	force_inline	void	SetRWTexture(ShaderStage stage, const Texture &tex, int i = 0)				{ SetUAV(stage, tex, i); }
	force_inline	void	SetTexture(ShaderStage stage, const Surface &surf, int i = 0)				{ SetSRV(stage, graphics.MakeTextureView(surf, surf.Format()), i); }
	force_inline	void	SetRWTexture(ShaderStage stage, const Surface &surf, int i = 0)				{ SetUAV(stage, graphics.MakeTextureView(surf, surf.Format()), i); }

	force_inline	void	SetConstBuffer(ShaderStage stage, ConstBuffer &buffer, int i = 0) {
		buffer.FixBuffer();
		ID3D11Buffer *b = buffer;
		switch (stage) {
			case SS_PIXEL:		context->PSSetConstantBuffers(i, 1, &b); break;
			case SS_VERTEX:		context->VSSetConstantBuffers(i, 1, &b); break;
			case SS_GEOMETRY:	context->GSSetConstantBuffers(i, 1, &b); break;
			case SS_HULL:		context->HSSetConstantBuffers(i, 1, &b); break;
			case SS_LOCAL:		context->DSSetConstantBuffers(i, 1, &b); break;
			case SS_COMPUTE:	context->CSSetConstantBuffers(i, 1, &b); break;
		}
	}
	force_inline	void	SetTexture(Texture &tex, int i = 0) {
		ID3D11ShaderResourceView *srv = tex;
		context->PSSetShaderResources(i, 1, &srv);
	}

	void				MapStreamOut(uint8 b0, uint8 b1, uint8 b2, uint8 b3);
	void				SetStreamOut(int i, void *start, uint32 size, uint32 stride);
	void				GetStreamOut(int i, uint64 *pos);
	void				FlushStreamOut();

	// draw
	void				SetVertexType(ID3D11InputLayout *vd)	{ context->IASetInputLayout(vd);	}
	template<typename T>void SetVertexType()					{ SetVertexType(GetVD<T>(stage_states[SS_VERTEX].raw));		}

	void SetIndices(const IndexBuffer<uint16> &ib) {
		context->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
	}
	void SetIndices(const IndexBuffer<uint32> &ib) {
		context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
	}
	void SetIndices(const DataBuffer &ib) {
		context->IASetIndexBuffer(static_cast<ID3D11Buffer*>(ib.Resource().get()), DXGI_FORMAT_R16_UINT, 0);
	}
	template<typename T>void SetVertices(const VertexBuffer<T> &vb, uint32 offset = 0) {
		offset	*= sizeof(T);
		uint32	stride = sizeof(T);
		SetVertexType<T>();
		context->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&vb, &stride, &offset);
	}
	template<typename T>void SetVertices(uint32 stream, const VertexBuffer<T> &vb, uint32 offset = 0) {
		offset	*= sizeof(T);
		uint32	stride = sizeof(T);
		//SetVertexType<T>();
		context->IASetVertexBuffers(stream, 1, (ID3D11Buffer**)&vb, &stride, &offset);
	}
	void SetVertices(uint32 stream, const _Buffer &vb, uint32 stride, uint32 offset = 0) {
		context->IASetVertexBuffers(stream, 1, (ID3D11Buffer**)&vb, &stride, &offset);
	}
	force_inline void	DrawPrimitive(PrimType prim, uint32 start_vert, uint32 num_prims) {
		DrawVertices(prim, start_vert, Prim2Verts(prim, num_prims));
	}
	force_inline void	DrawIndexedPrimitive(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start_index, uint32 num_indices) {
		DrawIndexedVertices(prim, min_index, num_verts, start_index, Prim2Verts(prim, num_indices));
	}
	force_inline void	DrawVertices(PrimType prim, uint32 start_vert, uint32 num_verts) {
		FlushDeferred();
		SetPrim(prim);
		context->Draw(num_verts, start_vert);
	}
	force_inline void	DrawIndexedVertices(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start_index, uint32 num_indices) {
		FlushDeferred();
		SetPrim(prim);
		context->DrawIndexed(num_indices, start_index, min_index);
	}
	force_inline void	DrawVertices(PrimType prim, uint32 start, uint32 num_verts, uint32 start_instance, uint32 num_instances) {
		FlushDeferred();
		SetPrim(prim);
		context->DrawInstanced(num_verts, num_instances, start, start_instance);
	}
	force_inline void	DrawIndexedVertices(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start_index, uint32 num_indices, uint32 start_instance, uint32 num_instances) {
		FlushDeferred();
		SetPrim(prim);
		context->DrawIndexedInstanced(num_indices, num_instances, start_index, min_index, start_instance);
	}
	force_inline void	DrawVertices(PrimType prim, const DataBuffer &args, uint32 offset = 0) {
		FlushDeferred();
		SetPrim(prim);
		context->DrawInstancedIndirect(static_cast<ID3D11Buffer*>(args.Resource().get()), offset);
	}
	force_inline void	DrawIndexedVertices(PrimType prim, const DataBuffer &args, uint32 offset = 0) {
		FlushDeferred();
		SetPrim(prim);
		context->DrawIndexedInstancedIndirect(static_cast<ID3D11Buffer*>(args.Resource().get()), offset);
	}

	force_inline void	Dispatch(uint32 dimx, uint32 dimy = 1, uint32 dimz = 1) {
		ISO_ASSERT(is_compute);
		if (update.test(UPD_CBS_PIXEL)) {
			ID3D11Buffer *buffs[16];
			context->CSSetConstantBuffers(0, stage_states[0].Flush(context, buffs), buffs);
		}
		uavs.FlushCS(context);
		context->Dispatch(dimx, dimy, dimz);
		uavs.ClearCS(context);
		uavs.FlushCS(context);
	}

	void	Dispatch(const DataBuffer &args, uint32 offset = 0) {
		ISO_ASSERT(is_compute);
		if (update.test(UPD_CBS_PIXEL)) {
			ID3D11Buffer *buffs[16];
			context->CSSetConstantBuffers(0, stage_states[0].Flush(context, buffs), buffs);
		}
		uavs.FlushCS(context);
		context->DispatchIndirect(static_cast<ID3D11Buffer*>(args.Resource().get()), offset);
		uavs.ClearCS(context);
		uavs.FlushCS(context);
	}

	// shaders
	void	SetShader(const DX11Shader &s);
	void	SetShaderConstants(ShaderReg reg, const void *values);
	template<typename T> void	SetShaderConstants(const T *values, uint16 reg, uint32 count = 1) {
		//SetShaderConstants_s<T>::f(device, reg, count, values);
	}
	// samplers
	force_inline	void	SetSamplerState(int i, TexState type, uint32 value) {
		D3D11_SAMPLER_DESC	&desc = samplers[i];
		switch (type) {
			case TS_FILTER:		desc.Filter			= (D3D11_FILTER)value; break;
			case TS_ADDRESS_U:	desc.AddressU		= (D3D11_TEXTURE_ADDRESS_MODE)value; break;
			case TS_ADDRESS_V:	desc.AddressV		= (D3D11_TEXTURE_ADDRESS_MODE)value; break;
			case TS_ADDRESS_W:	desc.AddressW		= (D3D11_TEXTURE_ADDRESS_MODE)value; break;
			case TS_MIP_BIAS:	desc.MipLODBias		= value; break;
			case TS_ANISO_MAX:	desc.MaxAnisotropy	= value; break;
			case TS_COMPARISON:	desc.ComparisonFunc = (D3D11_COMPARISON_FUNC)value; break;
			case TS_MIP_MIN:	desc.MinLOD			= value; break;
			case TS_MIP_MAX:	desc.MaxLOD			= value; break;
		}
		update.set(UPDATE(1<<i));
	}
	force_inline void	SetUVMode(int i, UVMode t)	{
		D3D11_SAMPLER_DESC	&desc = samplers[i];
		desc.AddressU		= D3D11_TEXTURE_ADDRESS_MODE(t & 15);
		desc.AddressV		= D3D11_TEXTURE_ADDRESS_MODE(t >> 4);
		update.set(UPDATE(1<<i));
	}
	force_inline void	SetTexFilter(int i, TexFilter t) {
		samplers[i].Filter	= (D3D11_FILTER)t;
		update.set(UPDATE(1<<i));
	}
	force_inline UVMode	GetUVMode(int i)	{
		D3D11_SAMPLER_DESC	&desc = samplers[i];
		return UVMode(desc.AddressU | (desc.AddressV << 4));
	}
	//raster
	force_inline void	SetFillMode(FillMode fill_mode)	{
		raster.FillMode = (D3D11_FILL_MODE)fill_mode;
		update.set(UPD_RASTER);
	}
	force_inline void	SetBackFaceCull(BackFaceCull c)		{
		raster.CullMode = (D3D11_CULL_MODE)c;
		update.set(UPD_RASTER);
	}
	force_inline void	SetDepthBias(float bias, float slope_bias) {
		raster.DepthBias			= bias;
		raster.SlopeScaledDepthBias	= slope_bias;
		update.set(UPD_RASTER);
	}

	//blend
	force_inline void	SetBlendEnable(bool enable) {
		blend.RenderTarget[0].BlendEnable	= enable;
		update.set(UPD_BLEND);
	}
	force_inline void	SetBlend(BlendOp op, BlendFunc src, BlendFunc dest) {
		blend.RenderTarget[0].SrcBlend		= (D3D11_BLEND)src;
		blend.RenderTarget[0].DestBlend		= (D3D11_BLEND)dest;
		blend.RenderTarget[0].BlendOp		= (D3D11_BLEND_OP)op;
		update.set(UPD_BLEND);
	}
	force_inline void	SetBlendSeparate(BlendOp op, BlendFunc src, BlendFunc dest, BlendOp opAlpha, BlendFunc srcAlpha, BlendFunc destAlpha) {
		blend.RenderTarget[0].SrcBlend		= (D3D11_BLEND)src;
		blend.RenderTarget[0].DestBlend		= (D3D11_BLEND)dest;
		blend.RenderTarget[0].BlendOp		= (D3D11_BLEND_OP)op;
		blend.RenderTarget[0].SrcBlendAlpha	= (D3D11_BLEND)srcAlpha;
		blend.RenderTarget[0].DestBlendAlpha= (D3D11_BLEND)destAlpha;
		blend.RenderTarget[0].BlendOpAlpha	= (D3D11_BLEND_OP)opAlpha;
		update.set(UPD_BLEND);
	}
	force_inline void	SetMask(uint32 mask) {
		blend.RenderTarget[0].RenderTargetWriteMask	= mask;
		update.set(UPD_BLEND);
	}
	force_inline void	SetAlphaToCoverage(bool enable) {
		blend.AlphaToCoverageEnable	= enable;
		update.set(UPD_BLEND);
	}
	force_inline void	SetBlendConst(param(colour) col) {
		blendfactor = col;
		update.set(UPD_BLENDFACTOR);
	}

	//depth & stencil
	force_inline void	SetDepthTest(DepthTest c) {
		depth.DepthFunc	= (D3D11_COMPARISON_FUNC)c;
		update.set(UPD_DEPTH);
	}
	force_inline void	SetDepthTestEnable(bool enable) {
		depth.DepthEnable	= enable;
		update.set(UPD_DEPTH);
	}
	force_inline void	SetDepthWriteEnable(bool enable) {
		depth.DepthWriteMask	= (D3D11_DEPTH_WRITE_MASK)enable;
		update.set(UPD_DEPTH);
	}

	force_inline void	SetStencilOp(StencilOp fail, StencilOp zfail, StencilOp zpass) {
		depth.FrontFace.StencilFailOp		= (D3D11_STENCIL_OP)fail;
		depth.FrontFace.StencilDepthFailOp	= (D3D11_STENCIL_OP)zfail;
		depth.FrontFace.StencilPassOp		= (D3D11_STENCIL_OP)zpass;
		update.set(UPD_DEPTH);
	}
	force_inline void	SetStencilOpBack(StencilOp fail, StencilOp zfail, StencilOp zpass) {
		depth.BackFace.StencilFailOp		= (D3D11_STENCIL_OP)fail;
		depth.BackFace.StencilDepthFailOp	= (D3D11_STENCIL_OP)zfail;
		depth.BackFace.StencilPassOp		= (D3D11_STENCIL_OP)zpass;
		update.set(UPD_DEPTH);
	}

	force_inline void	SetStencilFunc(StencilFunc func, uint8 ref, uint8 mask = 0xff) {
		depth.StencilEnable				= func != STENCILFUNC_OFF;
		depth.FrontFace.StencilFunc		= (D3D11_COMPARISON_FUNC)(func == STENCILFUNC_OFF ? STENCILFUNC_ALWAYS : func);
		depth.StencilReadMask			= mask;
		stencil_ref						= ref;
		update.set(UPD_DEPTH);
	}
	force_inline void	SetStencilFunc(StencilFunc front, StencilFunc back, uint8 ref, uint8 mask = 0xff) {
		depth.StencilEnable				= front != STENCILFUNC_OFF || back != STENCILFUNC_OFF;
		depth.FrontFace.StencilFunc		= (D3D11_COMPARISON_FUNC)(front == STENCILFUNC_OFF ? STENCILFUNC_ALWAYS : front);
		depth.BackFace.StencilFunc		= (D3D11_COMPARISON_FUNC)(back  == STENCILFUNC_OFF ? STENCILFUNC_ALWAYS : back);
		depth.StencilReadMask			= mask;
		stencil_ref						= ref;
		if (front == STENCILFUNC_OFF && back != STENCILFUNC_OFF)
			depth.FrontFace.StencilFailOp = depth.FrontFace.StencilDepthFailOp = depth.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		else if (front != STENCILFUNC_OFF && back == STENCILFUNC_OFF)
			depth.BackFace.StencilFailOp = depth.BackFace.StencilDepthFailOp = depth.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		update.set(UPD_DEPTH);
	}
	force_inline void	SetStencilRef(uint8 ref) 	{
		stencil_ref						= ref;
		update.set(UPD_STENCIL_REF);
	}
#if 0
	force_inline void	SetAlphaTestEnable(bool enable)		{ device->SetRenderState(D3DRS_ALPHATESTENABLE,		enable);	}
	force_inline void	SetAlphaTest(AlphaTest func, uint32 ref) {
		graphics.SetRenderState(D3DRS_ALPHAFUNC, func);
		graphics.SetRenderState(D3DRS_ALPHAREF, ref);
	}
#endif

	force_inline void	Resolve(RenderTarget i = RT_COLOUR0) {}
};

//-----------------------------------------------------------------------------
//	ImmediateStream
//-----------------------------------------------------------------------------

class _ImmediateStream {
protected:
	GraphicsContext &ctx;
	PrimType		prim;

	_ImmediateStream(GraphicsContext &ctx, PrimType prim) : ctx(ctx), prim(prim) {}
	void			*alloc(int count, uint32 vert_size, uint32 align);
	void			Draw(int count);
};

template<class T> class ImmediateStream : _ImmediateStream, public ptr_array<T> {
public:
	typedef T	*iterator;
	ImmediateStream(GraphicsContext &ctx, PrimType prim, int count) : _ImmediateStream(ctx, prim), ptr_array<T>((T*)alloc(count, sizeof(T), alignof(T)), count) {}
	~ImmediateStream()						{ ctx.SetVertexType<T>(); Draw(curr_size); }
	void			SetCount(int i)			{ ISO_ASSERT(i <= curr_size); curr_size = i; }
};

}
#endif	// GRAPHICS_DX11_H

