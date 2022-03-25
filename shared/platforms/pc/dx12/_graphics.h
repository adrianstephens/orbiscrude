#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "shared/graphics_defs.h"
#include "dx/dx_shaders.h"
#include "dx12_helpers.h"
#include "base/atomic.h"
#include "allocators/lf_allocator.h"
#include "allocators/lf_pool.h"
#include "base/vector.h"
//#include "base/soft_float.h"
#include "base/block.h"
#include "base/hash.h"
//#include "windows/window.h"
#include "com.h"
#include "crc32.h"
#include "dx/dxgi_helpers.h"
#include <pix.h>

#undef	CM_NONE

#define ISO_HAS_GEOMETRY
#define ISO_HAS_HULL
#define ISO_HAS_STREAMOUT
#define ISO_HAS_GRAHICSBUFFERS

namespace iso {

class GraphicsContext;
class ComputeContext;

template<typename T> void DeInit(T *t);
template<typename T> void Init(T *t, void*);

bool CheckResult(HRESULT h);

//-----------------------------------------------------------------------------
//	functions
//-----------------------------------------------------------------------------

float4x4 hardware_fix(param(float4x4) mat);
float4x4 map_fix(param(float4x4) mat);

//-----------------------------------------------------------------------------
//	enums
//-----------------------------------------------------------------------------

enum Memory {
	_MEM_RESOURCEFLAGS		= 0x3f,
	MEM_DEFAULT				= 0,
    MEM_TARGET				= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    MEM_DEPTH				= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
    MEM_WRITABLE			= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	MEM_SYSTEM				= 0,

    //D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER,
    //D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS,

	MEM_CPU_WRITE			= 1 << 6,
    MEM_CPU_READ			= 1 << 7,

	MEM_OTHER				= MEM_DEFAULT,

	MEM_CUBE				= 0x400,
	MEM_VOLUME				= 0x800,

	MEM_CASTABLE			= 0x1000,
};
inline Memory operator|(Memory a, Memory b)	{ return Memory(int(a) | b); }
inline Memory Bind(D3D12_RESOURCE_FLAGS b)	{ return Memory(b); }

#define MAKE_TEXFORMAT(DXGI,X,Y,Z,W)	DXGI | (D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(X,Y,Z,W)<<8)
enum TexFormat {
	TEXF_UNKNOWN			= MAKE_TEXFORMAT(DXGI_FORMAT_UNKNOWN,				0, 1, 2, 3),

	TEXF_D24S8				= MAKE_TEXFORMAT(DXGI_FORMAT_D24_UNORM_S8_UINT,		0, 1, 2, 3),
	TEXF_D24X8				= MAKE_TEXFORMAT(DXGI_FORMAT_R24_UNORM_X8_TYPELESS,	0, 1, 2, 3),
	TEXF_D16				= MAKE_TEXFORMAT(DXGI_FORMAT_D16_UNORM,				0, 1, 2, 3),
	TEXF_D16_LOCKABLE		= MAKE_TEXFORMAT(DXGI_FORMAT_D16_UNORM,				0, 1, 2, 3),
	TEXF_D32F				= MAKE_TEXFORMAT(DXGI_FORMAT_D32_FLOAT,				0, 1, 2, 3),
	TEXF_D32F_LOCKABLE		= MAKE_TEXFORMAT(DXGI_FORMAT_D32_FLOAT,				0, 1, 2, 3),

	TEXF_R8G8B8A8			= MAKE_TEXFORMAT(DXGI_FORMAT_R8G8B8A8_UNORM,		0, 1, 2, 3),
	TEXF_A8R8G8B8			= MAKE_TEXFORMAT(DXGI_FORMAT_B8G8R8A8_UNORM,		0, 1, 2, 3),
	TEXF_A8B8G8R8			= MAKE_TEXFORMAT(DXGI_FORMAT_R8G8B8A8_UNORM,		0, 1, 2, 3),
	TEXF_O8B8G8R8			= MAKE_TEXFORMAT(DXGI_FORMAT_B8G8R8X8_UNORM,		0, 1, 2, 3),
	TEXF_A16B16G16R16		= MAKE_TEXFORMAT(DXGI_FORMAT_R16G16B16A16_UNORM,	0, 1, 2, 3),
	TEXF_A16B16G16R16F		= MAKE_TEXFORMAT(DXGI_FORMAT_R16G16B16A16_FLOAT,	0, 1, 2, 3),
	TEXF_R32F				= MAKE_TEXFORMAT(DXGI_FORMAT_R32_FLOAT,				0, 1, 2, 3),
	TEXF_X8R8G8B8			= MAKE_TEXFORMAT(DXGI_FORMAT_B8G8R8X8_UNORM,		0, 1, 2, 3),
	TEXF_R5G6B5				= MAKE_TEXFORMAT(DXGI_FORMAT_B5G6R5_UNORM,			0, 1, 2, 3),
	TEXF_A1R5G5B5			= MAKE_TEXFORMAT(DXGI_FORMAT_B5G5R5A1_UNORM,		0, 1, 2, 3),
//	TEXF_A4R4G4B4			= MAKE_TEXFORMAT(DXGI_FORMAT_B4G4R4A4_UNORM,		0, 1, 2, 3),
	TEXF_A2B10G10R10		= MAKE_TEXFORMAT(DXGI_FORMAT_R10G10B10A2_UNORM,		0, 1, 2, 3),
	TEXF_G16R16				= MAKE_TEXFORMAT(DXGI_FORMAT_R16G16_UNORM,			0, 1, 2, 3),
	TEXF_A32B32G32R32F		= MAKE_TEXFORMAT(DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 1, 2, 3),

	TEXF_R8					= MAKE_TEXFORMAT(DXGI_FORMAT_R8_UNORM,				0, 1, 2, 3),
	TEXF_R8G8				= MAKE_TEXFORMAT(DXGI_FORMAT_R8G8_UNORM,			0, 1, 2, 3),
	TEXF_A8					= MAKE_TEXFORMAT(DXGI_FORMAT_A8_UNORM,				0, 1, 2, 3),
	TEXF_L8					= MAKE_TEXFORMAT(DXGI_FORMAT_R8_UNORM ,				0, 1, 2, 3),
	TEXF_A8L8				= MAKE_TEXFORMAT(DXGI_FORMAT_R8G8_UNORM,			0, 1, 2, 3),
	TEXF_L16				= MAKE_TEXFORMAT(DXGI_FORMAT_R16_UNORM,				0, 1, 2, 3),

	TEXF_DXT1				= MAKE_TEXFORMAT(DXGI_FORMAT_BC1_UNORM,				0, 1, 2, 3),
	TEXF_DXT3				= MAKE_TEXFORMAT(DXGI_FORMAT_BC2_UNORM,				0, 1, 2, 3),
	TEXF_DXT5				= MAKE_TEXFORMAT(DXGI_FORMAT_BC3_UNORM,				0, 1, 2, 3),
};

inline DXGI_FORMAT	GetDXGI(TexFormat f)	{ return DXGI_FORMAT(f & 255); }
inline uint32		GetSwizzle(TexFormat f) { return f >> 8;; }

enum PrimType {
	PRIM_UNKNOWN			= D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
	PRIM_POINTLIST			= D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
	PRIM_LINELIST			= D3D_PRIMITIVE_TOPOLOGY_LINELIST,
	PRIM_LINESTRIP			= D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
	PRIM_TRILIST			= D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	PRIM_TRISTRIP			= D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,

	//emulated
	PRIM_QUADLIST			= 0x1000,
	PRIM_RECTLIST,
	PRIM_TRIFAN,
};
inline PrimType StripPrim(PrimType p)		{ return PrimType(p | 1);}
inline PrimType AdjacencyPrim(PrimType p)	{ return PrimType(p | 8);}
inline PrimType PatchPrim(int n)			{ return PrimType(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + n - 1); }

enum BackFaceCull {
	BFC_NONE				= D3D12_CULL_MODE_NONE,
	BFC_FRONT				= D3D12_CULL_MODE_FRONT,
	BFC_BACK				= D3D12_CULL_MODE_BACK,
};

enum DepthTest {
	DT_NEVER				= D3D12_COMPARISON_FUNC_NEVER			- 1,
	DT_LESS					= D3D12_COMPARISON_FUNC_LESS			- 1,
	DT_EQUAL				= D3D12_COMPARISON_FUNC_EQUAL			- 1,
	DT_LEQUAL				= D3D12_COMPARISON_FUNC_LESS_EQUAL		- 1,
	DT_GREATER				= D3D12_COMPARISON_FUNC_GREATER			- 1,
	DT_NOTEQUAL				= D3D12_COMPARISON_FUNC_NOT_EQUAL		- 1,
	DT_GEQUAL				= D3D12_COMPARISON_FUNC_GREATER_EQUAL	- 1,
	DT_ALWAYS 				= D3D12_COMPARISON_FUNC_ALWAYS			- 1,

	DT_USUAL				= DT_GEQUAL,
	DT_CLOSER_SAME			= DT_GEQUAL,
	DT_CLOSER				= DT_GREATER,
};
inline DepthTest operator~(DepthTest b) { return DepthTest(9 - b); }

enum AlphaTest {
	AT_NEVER				= D3D12_COMPARISON_FUNC_NEVER			- 1,
	AT_LESS					= D3D12_COMPARISON_FUNC_LESS			- 1,
	AT_EQUAL				= D3D12_COMPARISON_FUNC_EQUAL			- 1,
	AT_LEQUAL				= D3D12_COMPARISON_FUNC_LESS_EQUAL		- 1,
	AT_GREATER				= D3D12_COMPARISON_FUNC_GREATER			- 1,
	AT_NOTEQUAL				= D3D12_COMPARISON_FUNC_NOT_EQUAL		- 1,
	AT_GEQUAL				= D3D12_COMPARISON_FUNC_GREATER_EQUAL	- 1,
	AT_ALWAYS 				= D3D12_COMPARISON_FUNC_ALWAYS			- 1,
};
inline AlphaTest operator~(AlphaTest b) { return AlphaTest(9 - b); }

enum UVMode {
	U_CLAMP					= D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	V_CLAMP					= D3D12_TEXTURE_ADDRESS_MODE_CLAMP	<<4,
	W_CLAMP					= D3D12_TEXTURE_ADDRESS_MODE_CLAMP	<<8,
	U_WRAP					= D3D12_TEXTURE_ADDRESS_MODE_WRAP,			//0
	V_WRAP					= D3D12_TEXTURE_ADDRESS_MODE_WRAP	<<4,	//0
	W_WRAP					= D3D12_TEXTURE_ADDRESS_MODE_WRAP	<<8,	//0
	U_MIRROR				= D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
	V_MIRROR				= D3D12_TEXTURE_ADDRESS_MODE_MIRROR	<<4,
	W_MIRROR				= D3D12_TEXTURE_ADDRESS_MODE_MIRROR	<<8,

	ALL_CLAMP				= U_CLAMP	| V_CLAMP	| W_CLAMP,
	ALL_WRAP				= U_WRAP	| V_WRAP	| W_WRAP,
	ALL_MIRROR				= U_MIRROR	| V_MIRROR	| W_MIRROR,
};

enum TexFilter {// mag, min, mip
//	TF_NEAREST_NEAREST_NONE		= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE),
//	TF_NEAREST_LINEAR_NONE		= MAKE_TEXFILTER(D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_NONE),
	TF_NEAREST_NEAREST_NEAREST	= D3D12_FILTER_MIN_MAG_MIP_POINT,
	TF_NEAREST_LINEAR_NEAREST	= D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,
	TF_NEAREST_NEAREST_LINEAR	= D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
	TF_NEAREST_LINEAR_LINEAR	= D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,

//	TF_LINEAR_NEAREST_NONE		= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_POINT, D3DTEXF_NONE),
//	TF_LINEAR_LINEAR_NONE		= MAKE_TEXFILTER(D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE),
	TF_LINEAR_NEAREST_NEAREST	= D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
	TF_LINEAR_LINEAR_NEAREST	= D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
	TF_LINEAR_NEAREST_LINEAR	= D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR,
	TF_LINEAR_LINEAR_LINEAR		= D3D12_FILTER_MIN_MAG_MIP_LINEAR,
};

enum ChannelMask {
	CM_RED					= D3D12_COLOR_WRITE_ENABLE_RED,
	CM_GREEN				= D3D12_COLOR_WRITE_ENABLE_GREEN,
	CM_BLUE					= D3D12_COLOR_WRITE_ENABLE_BLUE,
	CM_ALPHA				= D3D12_COLOR_WRITE_ENABLE_ALPHA,
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
	CF_POS_X,
	CF_NEG_X,
	CF_POS_Y,
	CF_NEG_Y,
	CF_POS_Z,
	CF_NEG_Z,
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
	BLENDOP_ADD				= D3D12_BLEND_OP_ADD,
	BLENDOP_MIN				= D3D12_BLEND_OP_MIN,
	BLENDOP_MAX				= D3D12_BLEND_OP_MAX,
	BLENDOP_SUBTRACT		= D3D12_BLEND_OP_SUBTRACT,
	BLENDOP_REVSUBTRACT		= D3D12_BLEND_OP_REV_SUBTRACT,
};

enum BlendFunc {
	BLEND_ZERO				= D3D12_BLEND_ZERO,
	BLEND_ONE				= D3D12_BLEND_ONE,
	BLEND_SRC_COLOR			= D3D12_BLEND_SRC_COLOR,
	BLEND_INV_SRC_COLOR		= D3D12_BLEND_INV_SRC_COLOR,
	BLEND_DST_COLOR			= D3D12_BLEND_DEST_COLOR,
	BLEND_INV_DST_COLOR		= D3D12_BLEND_INV_DEST_COLOR,
	BLEND_SRC_ALPHA  		= D3D12_BLEND_SRC_ALPHA  ,
	BLEND_INV_SRC_ALPHA		= D3D12_BLEND_INV_SRC_ALPHA,
	BLEND_DST_ALPHA			= D3D12_BLEND_DEST_ALPHA,
	BLEND_INV_DST_ALPHA		= D3D12_BLEND_INV_DEST_ALPHA,
	BLEND_SRC_ALPHA_SATURATE= D3D12_BLEND_SRC_ALPHA_SAT,
	BLEND_CONSTANT_COLOR	= D3D12_BLEND_BLEND_FACTOR,
	BLEND_INV_CONSTANT_COLOR= D3D12_BLEND_INV_BLEND_FACTOR,
//	BLEND_CONSTANT_ALPHA	= D3D12_BLEND_CONSTANTALPHA,
//	BLEND_INV_CONSTANT_ALPHA= D3D12_BLEND_INVCONSTANTALPHA,
};

enum StencilOp {
	STENCILOP_KEEP			= D3D12_STENCIL_OP_KEEP		- 1,
	STENCILOP_ZERO			= D3D12_STENCIL_OP_ZERO		- 1,
	STENCILOP_REPLACE		= D3D12_STENCIL_OP_REPLACE	- 1,
	STENCILOP_INCR			= D3D12_STENCIL_OP_INCR_SAT	- 1,
	STENCILOP_INCR_WRAP		= D3D12_STENCIL_OP_INCR		- 1,
	STENCILOP_DECR			= D3D12_STENCIL_OP_DECR_SAT	- 1,
	STENCILOP_DECR_WRAP		= D3D12_STENCIL_OP_DECR		- 1,
	STENCILOP_INVERT		= D3D12_STENCIL_OP_INVERT	- 1,
};

enum StencilFunc {
	STENCILFUNC_NEVER		= D3D12_COMPARISON_FUNC_NEVER			- 1,
	STENCILFUNC_LESS		= D3D12_COMPARISON_FUNC_LESS			- 1,
	STENCILFUNC_LEQUAL		= D3D12_COMPARISON_FUNC_LESS_EQUAL		- 1,
	STENCILFUNC_GREATER		= D3D12_COMPARISON_FUNC_GREATER			- 1,
	STENCILFUNC_GEQUAL		= D3D12_COMPARISON_FUNC_GREATER_EQUAL	- 1,
	STENCILFUNC_EQUAL		= D3D12_COMPARISON_FUNC_EQUAL			- 1,
	STENCILFUNC_NOTEQUAL	= D3D12_COMPARISON_FUNC_NOT_EQUAL		- 1,
	STENCILFUNC_ALWAYS		= D3D12_COMPARISON_FUNC_ALWAYS			- 1,
};
inline StencilFunc operator~(StencilFunc b) { return StencilFunc(9 - b); }

enum FillMode {
	FILL_SOLID				= D3D12_FILL_MODE_SOLID,
	FILL_WIREFRAME			= D3D12_FILL_MODE_WIREFRAME,
};

//-----------------------------------------------------------------------------
//	component types
//-----------------------------------------------------------------------------

typedef	DXGI_FORMAT	ComponentType;
template<typename T> ComponentType	GetComponentType(const T&)	{ return (ComponentType)_ComponentType<T>::value; }
template<typename T> ComponentType	GetComponentType()			{ return (ComponentType)_ComponentType<T>::value; }
template<typename T> TexFormat		GetTexFormat()				{ return TexFormat(_ComponentType<T>::value | (D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING << 8)); }
template<typename T> ComponentType	GetBufferFormat()			{ return ComponentType(_BufferFormat<_ComponentType<T>::value, sizeof(T)>::value); }

//-----------------------------------------------------------------------------
//	Resources
//-----------------------------------------------------------------------------

struct Resource : com_ptr<ID3D12Resource> {
	D3D12_RESOURCE_STATES		states;
	D3D12_CPU_DESCRIPTOR_HANDLE	cpu_handle;
	union {
		D3D12_CPU_DESCRIPTOR_HANDLE	uav_handle;
		D3D12_GPU_VIRTUAL_ADDRESS	gpu_address;
	};

	Resource()								{ cpu_handle.ptr = 0; uav_handle.ptr = 0; }
	Resource(ID3D12Resource *r) : com_ptr<ID3D12Resource>(r)	{ cpu_handle.ptr = 0; uav_handle.ptr = 0; }
	D3D12_RESOURCE_DESC	GetDesc()	const	{ return get()->GetDesc(); }
	void				Reset();
};

struct ResourceHandle : pool_index<atomic<fixed_pool<Resource,65536> >, int> {
	ResourceHandle()	{}
//	void		*raw()			const	{ return *(pointer32<void*>*)this; }
	void		*raw()			const	{ return *(void**)this; }
	void		release();
};

struct ResourceHandle2 : ResourceHandle {
	ResourceHandle2()		{}
	ResourceHandle2(ResourceHandle2 &&t) : ResourceHandle(t) {
		t.i = 0;
	}
	ResourceHandle2(const ResourceHandle &t) : ResourceHandle(t) {
		if (Resource *p = get())
			p->get()->AddRef();
	}
	ResourceHandle2(const ResourceHandle2 &t) : ResourceHandle(t) {
		if (Resource *p = get())
			p->get()->AddRef();
	}
	ResourceHandle2&	operator=(ResourceHandle2 &&t) {
		swap(i, t.i);
		return *this;
	}
	ResourceHandle2&	operator=(const ResourceHandle &t) {
		if (Resource *p = t.get())
			p->get()->AddRef();
		release();
		i = t.i;
		return *this;
	}
	ResourceHandle2&	operator=(const ResourceHandle2 &t) {
		if (Resource *p = t.get())
			p->get()->AddRef();
		release();
		i = t.i;
		return *this;
	}
	~ResourceHandle2()		{ release(); }
	void	clear()			{ release(); i = 0; }
};

inline point GetSize(const D3D12_RESOURCE_DESC &desc) {
	return point{desc.Width, desc.Height};
}

struct ResourceData {
	static D3D12_RANGE	empty_range;
	ID3D12Resource		*res;
	void				*p;

	ResourceData(ID3D12Resource *res, int sub = 0, bool read = false) : res(res) {
		CheckResult(res->Map(sub, read ? NULL : &empty_range, &p));
	}
	~ResourceData() {
		res->Unmap(0, NULL);
	}
};

template<typename T> struct TypedResourceData : ResourceData {
	TypedResourceData(ID3D12Resource *res, bool read = false) : ResourceData(res, read)	{}
	operator T*()	const { return (T*)p; }
	T*	get()		const { return (T*)p; }

};

//-----------------------------------------------------------------------------
//	Buffers
//-----------------------------------------------------------------------------

class _Buffer : public ResourceHandle2 {
protected:
	bool		Bind(DXGI_FORMAT format, uint32 stride, uint32 num);
public:
	bool		Init(uint32 size, Memory loc);
	bool		Init(const void *data, uint32 size, Memory loc);
	void*		Begin()				const;
	void		End()				const;
	uint32		Size()				const		{ return get()->GetDesc().Width; }
	Resource*	operator->()		const		{ return get();	}
	operator	Resource*()			const		{ return get();	}
	template<typename T> TypedResourceData<T>	Data()				{ return get()->get(); }
	template<typename T> TypedResourceData<T>	WriteData()	const	{ return get()->get(); }
};

template<typename T> class Buffer : public _Buffer {
public:
	Buffer()					{}
	Buffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)				{ Init(t, n, loc);	}
	template<int N> Buffer(const T (&t)[N], Memory loc = MEM_DEFAULT)	{ Init(t, loc);		}

	bool	Init(const T *t, uint32 n, Memory loc = MEM_DEFAULT) {
		return _Buffer::Init(t, n * sizeof(T), loc) && Bind(GetBufferFormat<T>(), sizeof(T), n);
	}
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT) {
		return Init(&t[0], N, loc);
	}
	bool	Init(uint32 n, Memory loc = MEM_DEFAULT) {
		return _Buffer::Init(n * sizeof(T), loc) && Bind(GetBufferFormat<T>(), sizeof(T), n);
	}
	TypedResourceData<const T>	Data(ID3D12GraphicsCommandList *ctx = 0)		const	{ return _Buffer::Data<T>(); }
	TypedResourceData<T>		WriteData(ID3D12GraphicsCommandList *ctx = 0)	const	{ return _Buffer::WriteData<T>(); }
	T*		Begin(ID3D12GraphicsCommandList *ctx = 0)	const { return (T*)_Buffer::Begin(); }
	void	End(ID3D12GraphicsCommandList *ctx = 0)		const {	_Buffer::End(); }
	uint32	Size()										const { return _Buffer::Size() / sizeof(T); }
};

class DataBuffer : _Buffer {
public:
	DataBuffer() {}
	template<typename T> DataBuffer(const Buffer<T> &b)	: _Buffer(b) {}
	template<typename T> operator Buffer<T>&()	const	{ return *(Buffer<T>*)this; }
};

template<D3D12_RESOURCE_STATES S> class _BufferGPU : public _Buffer {
public:
	bool		Init(uint32 size, Memory loc = MEM_DEFAULT) {
		_Buffer::Init(size, loc);
		Resource	*p	= get();
		p->gpu_address	= (*p)->GetGPUVirtualAddress();
		p->states		= S;
		return true;
	}
	bool		Init(const void *data, uint32 size, Memory loc = MEM_DEFAULT) {
		_Buffer::Init(data, size, loc);
		Resource	*p	= get();
		p->gpu_address	= (*p)->GetGPUVirtualAddress();
		p->states		= S;
		return true;
	}
};

typedef _BufferGPU<D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER>	_VertexBuffer;
typedef _BufferGPU<D3D12_RESOURCE_STATE_INDEX_BUFFER>				_IndexBuffer;

template<typename T, D3D12_RESOURCE_STATES S> class _TypedGPUBuffer : public _BufferGPU<S> {
public:
	_TypedGPUBuffer()															{}
	_TypedGPUBuffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)				{ this->Init(t, n, loc); }
	template<int N> _TypedGPUBuffer(const T (&t)[N], Memory loc = MEM_DEFAULT)	{ this->Init(t, loc); }

	bool					Init(const T *t, uint32 n, Memory loc = MEM_DEFAULT)	{ return _BufferGPU<S>::Init(t, n * sizeof(T), loc); }
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT)			{ return _BufferGPU<S>::Init(&t, sizeof(t), loc); }
	bool	Init(uint32 n, Memory loc = MEM_DEFAULT)	{ return _BufferGPU<S>::Init(n * sizeof(T), loc);	}
	T*		Begin(uint32 n, Memory loc = MEM_DEFAULT)	{ return Init(n, loc) ? Begin() : NULL;	}
	T*		Begin()							const		{ return (T*)_Buffer::Begin();	}
	uint32	Count()							const		{ return Size() / sizeof(T); }
	TypedResourceData<const T>	Data(ID3D12GraphicsCommandList *ctx = 0)		const	{ return _Buffer::Data<T>(); }
	TypedResourceData<T>		WriteData(ID3D12GraphicsCommandList *ctx = 0)	const	{ return _Buffer::WriteData<T>(); }
};

template<typename T> using VertexBuffer = _TypedGPUBuffer<T, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER>;
template<typename T> using IndexBuffer	= _TypedGPUBuffer<T, D3D12_RESOURCE_STATE_INDEX_BUFFER>;

//-----------------------------------------------------------------------------
//	Texture/Surface
//-----------------------------------------------------------------------------

class Surface : ResourceHandle2 {
public:
	friend class Texture;
	friend class Graphics;
	friend GraphicsContext;
	friend ComputeContext;
	
	uint8	fmt, mip, slice, num_slices;
	bool	Init(TexFormat _fmt, int width, int height, Memory loc);
	uint32	CalcSubresource() const	{ return slice == 0 ? mip : get()->GetDesc().MipLevels * slice + mip; }

	template<typename T> struct Temp : ResourceData, block<T,2> {
		Temp(ID3D12Resource *res, uint32 sub, const point &size, bool read) : ResourceData(res, sub, read), block<T,2>(make_block((T*)this->p, size.x), size.x, size.y) {}
	};

public:
	Surface() {}
	Surface(TexFormat fmt, int width, int height, Memory loc) {
		Init(fmt, width, height, loc);
	}
	Surface(TexFormat fmt, const point &size, Memory loc) {
		Init(fmt, size.x, size.y, loc);
	}
	Surface(const ResourceHandle &h, TexFormat fmt = TEXF_UNKNOWN, uint8 mip = 0, uint8 slice = 0, uint8 num_slices = 0)
		: ResourceHandle2(h), fmt(fmt), mip(mip), slice(slice), num_slices(num_slices) {
	}
	Surface(ID3D12Resource *r, TexFormat fmt = TEXF_UNKNOWN, uint8 mip = 0, uint8 slice = 0, uint8 num_slices = 0)
		: fmt(fmt), mip(mip), slice(slice), num_slices(num_slices) {
		r->AddRef();
		alloc(r); 
	}
	point	GetSize() const {
		return max(iso::GetSize(get()->GetDesc()) >> mip, one);
	}
	rect	GetRect() const {
		return rect(zero, GetSize());
	}

	template<typename T> TypedResourceData<const T>	Data(ID3D12GraphicsCommandList *ctx = 0)		const	{ return {get()->get(), sub, GetSize(), true}; }
	template<typename T> TypedResourceData<T>		WriteData(ID3D12GraphicsCommandList *ctx = 0)	const	{ return {get()->get(), sub, GetSize(), false}; }
	
	operator ID3D12Resource*() const { return get()->get(); }

	D3D12_DEPTH_STENCIL_VIEW_DESC DepthDesc() const {
		D3D12_DEPTH_STENCIL_VIEW_DESC	desc;
		desc.Format				= (DXGI_FORMAT)fmt;
		desc.ViewDimension		= D3D12_DSV_DIMENSION_TEXTURE2D;
		desc.Flags				= D3D12_DSV_FLAG_NONE;
		desc.Texture2D.MipSlice	= mip;
		switch (fmt) {
			case DXGI_FORMAT_R16_UINT:
				desc.Format = DXGI_FORMAT_D16_UNORM;
				break;

			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
				desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				break;

			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT:
				desc.Format = DXGI_FORMAT_D32_FLOAT;
				break;

			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_R32G32_UINT:
				desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
				break;
		}
		return desc;
	}

	D3D12_RENDER_TARGET_VIEW_DESC RenderDesc() const {
		D3D12_RENDER_TARGET_VIEW_DESC	desc;
		desc.Format					= (DXGI_FORMAT)fmt;
		desc.ViewDimension			= D3D12_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice		= mip;
		desc.Texture2D.PlaneSlice	= 0;
		return desc;
	}
};

typedef Resource _Texture;
class Texture : public ResourceHandle2 {
	friend void Init<Texture>(Texture *x, void *physram);

	template<typename T> struct Temp : ResourceData, block<T,2> {
		Temp(ID3D12Resource *res, const point &size) : ResourceData(res), block<T,2>(make_block((T*)this->p, size.x), size.x, size.y) {}
	};
public:
	Texture()										{}
	explicit Texture(const Surface &s);
	Texture(TexFormat format, int width, int height, int depth = 1, int mips = 1, Memory loc = MEM_DEFAULT) {
		Init(format, width, height, depth, mips, loc);
	}
	bool		Init(TexFormat format, int width, int height, int depth = 1, int mips = 1, Memory loc = MEM_DEFAULT);
	bool		Init(TexFormat format, int width, int height, int depth, int mips, Memory loc, void *data, int pitch);
	void		DeInit();
	point		Size()						const	{ return GetSize((*this)->GetDesc()); }
	int			Depth()						const;
	bool		IsDepth()					const	{ return false; }

	Surface		GetSurface(int i = 0)			const;
	Surface		GetSurface(CubemapFace f, int i)const;
	Texture		As(TexFormat format)		const;

	operator	Surface()			const		{ return GetSurface(0); }
	Resource*	operator->()		const		{ return get();	}
	operator	Resource*()			const		{ return get();	}
	template<typename T> Temp<const T>	Data(ID3D12GraphicsCommandList *ctx = 0)		const	{ return get()->get(); }
	template<typename T> Temp<T>		WriteData(ID3D12GraphicsCommandList *ctx = 0)	const	{ return get()->get(); }
};
/*
class bitmap;
class HDRbitmap;
struct vbitmap;

Texture	MakeTexture(bitmap *bm);
Texture	MakeTexture(HDRbitmap *bm);
Texture	MakeTexture(vbitmap *bm);
*/
//-----------------------------------------------------------------------------
//	Vertex
//-----------------------------------------------------------------------------

struct VertexElement {
	int				offset;
	ComponentType	type;
	uint32			usage;
	int				stream;

	VertexElement() {}
	constexpr VertexElement(int offset, ComponentType type, uint32 usage, int stream = 0) : offset(offset), type(type), usage(usage), stream(stream) {}
	template<typename T> constexpr VertexElement(int offset, uint32 usage, int stream = 0) : offset(offset), type(GetComponentType<T>()), usage(usage), stream(stream) {}
	template<typename B, typename T> constexpr VertexElement(T B::* p, uint32 usage, int stream = 0) : offset(intptr_t(get_ptr(((B*)0)->*p))), type(GetComponentType<T>()), usage(usage), stream(stream) {}
	void	SetUsage(uint32 _usage) { usage = _usage; }
};

struct VertexDescription : D3D12_INPUT_LAYOUT_DESC {
	bool	 Init(D3D11_INPUT_ELEMENT_DESC *ve, uint32 n, const void *vs) {
		pInputElementDescs = (D3D12_INPUT_ELEMENT_DESC*)ve;
		NumElements	= n;
		return true;
	}
	bool	 Init(const VertexElement *ve, uint32 n, const void *vs);
	bool	 Init(const VertexElement *ve, uint32 n, const DX11Shader *s) { return Init(ve, n, s->sub[SS_VERTEX].raw()); }

	VertexDescription() {}
	VertexDescription(VertexElements ve, const void *vs) { Init(ve.p, (uint32)ve.n, vs); }
};

struct StreamOutDescription : D3D12_STREAM_OUTPUT_DESC {
	void	Init(D3D12_SO_DECLARATION_ENTRY *e, uint32 n, const void *pass) {
		pSODeclaration	= e;
		NumEntries		= n;
	}
	StreamOutDescription()										{ pSODeclaration = 0; NumEntries = 0; }
};

template<typename T> const VertexDescription	&GetVD(const void *vs)	{ static VertexDescription vd(GetVE<T>(), vs); return vd; }

//-----------------------------------------------------------------------------
//	Shaders
//-----------------------------------------------------------------------------

enum ShaderParamType {
	SPT_VAL			= 0,
	SPT_TEXTURE		= 1,
	SPT_SAMPLER		= 2,
	SPT_BUFFER		= 3,
	SPT_COUNT,
};

struct ShaderReg {
	union {
		struct {uint64 offset:24, buffer:8, count:24, stage:3, type:2, unused:2, indirect:1;};
		uint64	u;
	};
	ShaderReg()				: u(0)	{}
	ShaderReg(uint64 u)	: u(u)	{}
	ShaderReg(uint32 offset, uint32 count, uint8 buffer, ShaderStage stage, ShaderParamType type)
		: offset(offset), buffer(buffer), count(count), stage(stage), type(type), indirect(0)
	{}
	operator uint64() const	{ return u;	}
};

class ShaderParameterIterator {
	const DX11Shader	&shader;
	int					stage;
	int					cbuff_index;
	dx::RD11			*rdef;
	range<stride_iterator<dx::RDEF::Variable> >	vars;
	range<stride_iterator<dx::RDEF::Binding> >	bindings;
	const char			*name;
	const void			*val;
	D3D11_SAMPLER_DESC	sampler;
	ShaderReg			reg;
	fixed_string<64>	temp_name;

	int				ArrayCount(const char *begin, const char *&end);
	void			Next();
public:
	ShaderParameterIterator(const DX11Shader &s) : shader(s), stage(-1), vars(empty), bindings(empty) { Next(); }
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

struct Fence : com_ptr<ID3D12Fence> {
	HANDLE	event;
	Fence() : event(0) {}
	Fence(ID3D12Device *device, uint64 value) : event(0) {
		init(device, value);
	}
	bool	init(ID3D12Device *device, uint64 value) {
		return CheckResult(device->CreateFence(value, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&*this));
	}
	bool	check(uint64 value) const {
		return get()->GetCompletedValue() >= value;
	}
	void	wait(uint64 value) {
		if (!check(value)) {
			if (!event)
				event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (SUCCEEDED(get()->SetEventOnCompletion(value, event)))
				WaitForSingleObjectEx(event, INFINITE, FALSE);
		}
	}
	void	signal(ID3D12CommandQueue *queue, uint64 value) const {
		queue->Signal(get(), value);
	}
	void	wait(ID3D12CommandQueue *queue, uint64 value) const {
		queue->Wait(get(), value);
	}
};

struct ConstBuffer {
	uint32			num;
	malloc_block	mem;
	void*			gpu;

	D3D12_CPU_DESCRIPTOR_HANDLE	handle;

	ConstBuffer() : gpu(0)		{ handle.ptr = 0; }
	ConstBuffer(uint32 size)	{ init(size); }
	ConstBuffer(ConstBuffer &&b) : num(b.num), mem(move(b.mem)), gpu(b.gpu), handle(b.handle) { b.gpu = 0; b.handle.ptr = 0; }
	~ConstBuffer();

	void	init(uint32 _size);

	bool	SetConstant(int offset, const void* data, size_t size) {
		ISO_ASSERT(offset >= 0 && size > 0 && offset + size <= mem.length());
		if (gpu) {
			if (memcmp((uint8*)mem + offset, data, size) == 0)
				return false;
			gpu = 0;
		}
		memcpy((uint8*)mem + offset, data, size);
		return true;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE	FixBuffer();
};

typedef atomic<circular_allocator>	FrameAllocator;

struct RenderWindow;

class Graphics {
	friend GraphicsContext;
	friend ComputeContext;
	friend ConstBuffer;
	friend _Buffer;

public:
	static const int	NumFrames = 4;

	class Display {
		com_ptr<IDXGISwapChain3>	swapchain;
		TexFormat					format;
		ResourceHandle				surface[NumFrames];
	public:
		Display() : format(TEXF_A8B8G8R8) {}
		~Display();
		bool			SetFormat(const RenderWindow *window, const point &size, TexFormat _format);
		bool			SetSize(const RenderWindow *window, const point &size)	{ return SetFormat(window, size, TEXF_A8B8G8R8); }
		point			Size()				const;
		Surface			GetDispSurface()	const	{ return Surface(surface[swapchain->GetCurrentBackBufferIndex()], format); }
		IDXGISwapChain*	GetSwapChain()		const	{ return swapchain; }
		void			MakePresentable(GraphicsContext &ctx) const;
		bool			Present()			const;
	};

	enum FEATURE {
		COMPUTE,
		DOUBLES,
	};

	template<int N> struct Barriers : static_array<D3D12_RESOURCE_BARRIER, N> {
		inline void			Transition(ID3D12Resource *res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
			D3D12_RESOURCE_BARRIER	&b	= push_back();
			b.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			b.Flags						= D3D12_RESOURCE_BARRIER_FLAG_NONE;
			b.Transition.pResource		= res;
			b.Transition.StateBefore	= before;
			b.Transition.StateAfter		= after;
			b.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		}
		inline void			UAVBarrier(ID3D12Resource *res) {
			D3D12_RESOURCE_BARRIER	&b	= push_back();
			b.Type						= D3D12_RESOURCE_BARRIER_TYPE_UAV;
			b.Flags						= D3D12_RESOURCE_BARRIER_FLAG_NONE;
			b.UAV.pResource				= res;
		}
		inline void			Transition(ID3D12GraphicsCommandList *cmd_list, Resource *r, D3D12_RESOURCE_STATES s) {
			static auto reads =
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
				|	D3D12_RESOURCE_STATE_INDEX_BUFFER
				|	D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
				|	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				|	D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
				|	D3D12_RESOURCE_STATE_COPY_SOURCE
				|	D3D12_RESOURCE_STATE_DEPTH_READ;
			static auto read_writes =
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				|	D3D12_RESOURCE_STATE_DEPTH_WRITE;
			static auto writes =
					D3D12_RESOURCE_STATE_COPY_DEST
				|	D3D12_RESOURCE_STATE_RENDER_TARGET
				|	D3D12_RESOURCE_STATE_STREAM_OUT;

			if (full())
				Flush(cmd_list);

			if (r->states != s) {
				if ((s & reads) && (r->states & s))
					return;
				Transition(*r, r->states, s);
				r->states = s;
			} else if (s == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
				UAVBarrier(*r);
			}
		}
		void Flush(ID3D12GraphicsCommandList *cmd_list) {
			if (uint32 n = size()) {
				for (auto &i : *this)
					cmd_list->ResourceBarrier(1, &i);
				//cmd_list->ResourceBarrier(n, *this);
				clear();
			}
		}
	};

	struct StageState {
		const void	*shader;
		ConstBuffer*				cb[16];
		D3D12_CPU_DESCRIPTOR_HANDLE	srv[16];
		D3D12_CPU_DESCRIPTOR_HANDLE	uav[16];
		uint16						dirty, cb_used, srv_used, uav_used;

		StageState()													{ Reset(); }
		void			SetCB(ConstBuffer *b, uint32 i)					{ cb[i] = b; cb_used |= 1 << i; dirty |= 1; }
		bool			SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE h, uint32 i)	{ if (srv[i].ptr == h.ptr) return false; srv[i] = h; srv_used |= 1 << i; dirty |= 2; return true;  }
		bool			SetUAV(D3D12_CPU_DESCRIPTOR_HANDLE h, uint32 i)	{ if (uav[i].ptr == h.ptr) return false; uav[i] = h; uav_used |= 1 << i; dirty |= 4; return true;  }
		void			ClearCB(uint32 i)								{ cb_used &= ~(1 << i);  }
		void			ClearSRV(uint32 i)								{ srv_used &= ~(1 << i); }
		void			ClearUAV(uint32 i)								{ uav_used &= ~(1 << i); }
		ConstBuffer*	GetCB(uint32 i)	const							{ return cb[i];	}

		void Flush(ID3D12Device *device, ID3D12GraphicsCommandList *cmd_list, int root_item, bool compute);
		bool Init(const void *_shader);
		void Reset() {
			clear(cb);
			clear(srv);
			clear(uav);
			dirty	= cb_used = srv_used = uav_used = 0;
			shader	= 0;
		}
	};

	struct CommandContext {
		com_ptr<ID3D12CommandAllocator>		allocator;
		com_ptr<ID3D12GraphicsCommandList>	list;

		bool	Init(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type) {
			return CheckResult(device->CreateCommandAllocator(type,  IID_PPV_ARGS(&allocator)))
				&& CheckResult(device->CreateCommandList(0, type, allocator, nullptr, IID_PPV_ARGS(&list)));
		}
		void	Reset() {
			allocator->Reset();
			list->Reset(allocator, nullptr);
		}
		bool	Close() {
			return CheckResult(list->Close());
		}
		void	Submit(ID3D12CommandQueue *queue) {
			ID3D12CommandList	*list0 = list;
			queue->ExecuteCommandLists(1, &list0);
		}
	};

	struct CommandContext2 : e_link<CommandContext2>, CommandContext {
		uint64							fence_value;
		dynamic_array<ResourceHandle2>	resources;

		CommandContext2(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type) : fence_value(0) {
			Init(device, type);
			Close();
		}
		void Reset() {
			resources.clear();
			fence_value = ~uint64(0);
			CommandContext::Reset();
		}
		void PendingRelease(ResourceHandle2 &r) {
			if (r) {
				auto	desc = r->GetDesc();
				swap(resources.push_back(), r);
			}
		}
	};

	class Queue : public com_ptr<ID3D12CommandQueue> {
		Fence					fence;
		uint64					fence_value;
		e_list<CommandContext2>	cmd_contexts;
	public:

		bool	Init(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type, int priority = 0) {
			return CheckResult(device->CreateCommandQueue(addr(dx12::COMMAND_QUEUE_DESC(type, priority)), __uuidof(ID3D12CommandQueue), (void**)get_addr()))
				&& fence.init(device, fence_value = 0);
		}
		CommandContext2 *alloc_cmd_list(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type) {
			for (auto &i : cmd_contexts) {
				if (fence.check(i.fence_value))
					return i.unlink();
			}
			return new CommandContext2(device, type);
		}
		uint64	InsertSignal() {
			uint64 value = ++fence_value;
			fence.signal(get(), value);
			return value;
		}
		void	WaitSignal(uint64 value) {
			fence.wait(value);
		}
		void	Submit(CommandContext2 *cmd) {
			cmd->Submit(get());
			cmd->fence_value = InsertSignal();
			cmd_contexts.push_back(cmd);
		}
		void	Flush() {
			auto	x = InsertSignal();
			WaitSignal(x);
			for (auto &i : cmd_contexts)
				i.Reset();
		}
	};

	com_ptr<ID3D12Device>			device;
	hash_map<uint32, ConstBuffer*>	cb;

	Queue							queue;
	uint32							frame;

#if 0
	com_ptr<ID3D12CommandQueue>	cmd_queue;
	e_list<CommandContext2>		cmd_contexts;
	Fence						fence;
	uint64						fence_value;

	CommandContext2*	alloc_cmd_list(D3D12_COMMAND_LIST_TYPE type) {
		for (auto &i : cmd_contexts) {
			if (i.type == type && fence.check(i.fence_value))
				return i.unlink();
		}
		return new CommandContext2(device, type);
	}
	void				Submit(CommandContext2 *cmd);
#endif
	FrameAllocator&		allocator();

	static void*		alloc(size_t size, size_t align)				{ return aligned_alloc(size, align); }
	static void*		realloc(void *p, size_t size, size_t align)		{ return aligned_realloc(p, size, align);	}
	static bool			free(void *p)									{ aligned_free(p); return true; }
	static bool			free(void *p, size_t size)						{ aligned_free(p); return true; }
	static void			transfer(void *d, const void *s, size_t size)	{ memcpy(d, s, size);	}
	static uint32		fix(void *p, size_t size)						{ return 0; }
	static void*		unfix(uint32 p)									{ ISO_ALWAYS_CHEAPASSERT(0); return 0; }

	bool				Init(IDXGIAdapter1 *adapter = 0);
	bool				Init(HWND hWnd, IDXGIAdapter1 *adapter = 0)		{ return Init(adapter); }
	ID3D12Device*		Device()										{ if (!device) Init(); return device;	}
	ConstBuffer*		FindConstBuffer(crc32 id, uint32 size, uint32 num);
	void				BeginScene(GraphicsContext &ctx);
	void				EndScene(GraphicsContext &ctx);

	ID3D12Resource*		MakeTextureResource(TexFormat format, int width, int height, int depth, int mips, Memory loc, void *data = 0, int pitch = 0);
	_Texture			MakeTexture(TexFormat format, int width, int height, int depth, int mips, Memory loc, void *data = 0, int pitch = 0);
};

extern Graphics graphics;

//-----------------------------------------------------------------------------
//	ComputeContext
//-----------------------------------------------------------------------------

class ComputeContext {
	friend Graphics;

	enum {MAX_CBV = 8, MAX_SRV = 16, MAX_UAV = 16, MAX_BARRIERS = 8};
	Graphics::Queue						&q;
	Graphics::CommandContext2*			cmd;
	com_ptr<ID3D12RootSignature>		root_signature;

	Graphics::StageState				stage;
	Graphics::Barriers<MAX_BARRIERS>	barriers;

	inline void			SetSRV(Resource *r, int i)	{
		if (r) {
			barriers.Transition(cmd->list, r, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			stage.SetSRV(r->cpu_handle, i);
		} else {
			stage.ClearSRV(i);
		}
	}
	inline void			SetSRV0(Resource *r, int i)	{
		if (r) {
			stage.SetSRV(r->cpu_handle, i);
		} else {
			stage.ClearSRV(i);
		}
	}
	inline void			SetUAV(Resource *r, int i)	{
		if (r) {
			barriers.Transition(cmd->list, r, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			stage.SetUAV(r->uav_handle, i);
		} else {
			stage.ClearUAV(i);
		}
	}

public:
	ComputeContext(Graphics::Queue &q = graphics.queue) : q(q), cmd(0) {}
	~ComputeContext() {
		if (cmd)
			DingDong();
	}

	operator ID3D12GraphicsCommandList*()	const	{ return cmd->list; }
	FrameAllocator		&allocator()				{ return graphics.allocator(); }

	void				Begin();
	void				End();
	void				DingDong();

	void				PushMarker(const char *s)	{ PIXBeginEvent(cmd->list, 0, s); }
	void				PopMarker()					{ PIXEndEvent(cmd->list); }

	void	Blit(const Surface &dest, const Surface &srce, const point &dest_pos, const rect &srce_rect) {
		D3D12_BOX	box = {
			(uint32)srce_rect.a.x,	(uint32)srce_rect.a.y,	0,
			(uint32)srce_rect.b.x,	(uint32)srce_rect.b.y,	1
		};

		dx12::TEXTURE_COPY_LOCATION	dest_loc(*dest.get(), dest.CalcSubresource());//0, dx12::SUBRESOURCE_FOOTPRINT(dest.get()->GetDesc(), 0));
		dx12::TEXTURE_COPY_LOCATION	srce_loc(*srce.get(), srce.CalcSubresource());//0, dx12::SUBRESOURCE_FOOTPRINT(srce.get()->GetDesc(), 0));

		cmd->list->CopyTextureRegion(
			&dest_loc, dest_pos.x, dest_pos.y, 0,
			&srce_loc, &box
		);
	}
	void	Blit(const Surface &dest, const Surface &srce, const point &dest_pos = zero)	{
		Blit(dest, srce, dest_pos, srce.GetRect());
	}

	void	SetBuffer(Resource *r, int i = 0)				{ SetSRV(r, i); }
	void	SetTexture(Resource *r, int i = 0)				{ SetSRV(r, i); }
	void	SetRWBuffer(Resource *r, int i = 0)				{ SetUAV(r, i); }
	void	SetRWTexture(Resource *r, int i = 0)			{ SetUAV(r, i); }
	void	SetConstBuffer(ConstBuffer &buffer, int i = 0)	{ stage.SetCB(&buffer, i); }

	void	SetShader(const DX12Shader &s);
	void	SetShaderConstants(ShaderReg reg, const void *values);

	void	Dispatch(uint32 dimx, uint32 dimy, uint32 dimz) {
		stage.Flush(graphics.device, cmd->list, 1, true);
		barriers.Flush(cmd->list);
		cmd->list->Dispatch(dimx, dimy, dimz);
	}
};

class ComputeQueue : public Graphics::Queue, public ComputeContext {
public:
	ComputeQueue(int priority = 0) : ComputeContext(*(Graphics::Queue*)this) {
		Init(graphics.Device(), D3D12_COMMAND_LIST_TYPE_COMPUTE, priority);
		Begin();
	}
};

//-----------------------------------------------------------------------------
//	GraphicsContext
//-----------------------------------------------------------------------------

struct PackedGraphicsState {
	struct TargetBlendState {
		static uint16	Blend(BlendFunc src, BlendFunc dst) { return src + dst * 20; }
		uint32	src_dst_blends:9, blend_op:3,
				src_dst_blends_alpha:9, blend_op_alpha:3,
				logic_op:4, write_mask:4;
		void GetDesc(D3D12_RENDER_TARGET_BLEND_DESC &desc) const {
			desc.BlendEnable			= src_dst_blends != 0;
			desc.LogicOpEnable			= src_dst_blends == 400;
			desc.SrcBlend				= D3D12_BLEND(src_dst_blends % 20);
			desc.DestBlend				= D3D12_BLEND(src_dst_blends / 20);
			desc.BlendOp				= D3D12_BLEND_OP(blend_op);
			desc.SrcBlendAlpha			= D3D12_BLEND(src_dst_blends_alpha % 20);
			desc.DestBlendAlpha			= D3D12_BLEND(src_dst_blends_alpha / 20);
			desc.BlendOpAlpha			= D3D12_BLEND_OP(blend_op_alpha);
			desc.LogicOp				= D3D12_LOGIC_OP(logic_op);
			desc.RenderTargetWriteMask	= write_mask;
		}
		void	reset() {
			src_dst_blends			= Blend(BLEND_ONE, BLEND_INV_SRC_COLOR);
			src_dst_blends_alpha	= Blend(BLEND_ONE, BLEND_ZERO);
			blend_op				= blend_op_alpha		= BLENDOP_ADD;
			logic_op				= D3D12_LOGIC_OP_NOOP;
			write_mask				= 15;
		}
	};
	struct RasterizerState {
		int		DepthBias;
		float	DepthBiasClamp;
		float	SlopeScaledDepthBias;
		uint8	FillMode:2, CullMode:2, FrontCounterClockwise:1, DepthClipEnable:1, MultisampleEnable:1, AntialiasedLineEnable:1;
		uint8	log2ForcedSampleCount:3, ConservativeRaster:1;
		void GetDesc(D3D12_RASTERIZER_DESC &desc) const {
			desc.FillMode				= D3D12_FILL_MODE(FillMode);
			desc.CullMode				= D3D12_CULL_MODE(CullMode);
			desc.FrontCounterClockwise	= FrontCounterClockwise;
			desc.DepthBias				= DepthBias;
			desc.DepthBiasClamp			= DepthBiasClamp;
			desc.SlopeScaledDepthBias	= SlopeScaledDepthBias;
			desc.DepthClipEnable		= DepthClipEnable;
			desc.MultisampleEnable		= MultisampleEnable;
			desc.AntialiasedLineEnable	= AntialiasedLineEnable;
			desc.ForcedSampleCount		= (1 << log2ForcedSampleCount) >> 1;
			desc.ConservativeRaster		= D3D12_CONSERVATIVE_RASTERIZATION_MODE(ConservativeRaster);
		}
		void	reset() {
			FillMode				= FILL_SOLID;
			CullMode				= BFC_NONE;
			FrontCounterClockwise	= true;
		}
	};
	struct DepthStencil {
		struct StencilOp {
			uint16	fail:3, depth_fail:3, pass:3, func:3;
			void GetDesc(D3D12_DEPTH_STENCILOP_DESC &desc) const {
				desc.StencilFailOp		= D3D12_STENCIL_OP(fail + 1);
				desc.StencilDepthFailOp	= D3D12_STENCIL_OP(depth_fail + 1);
				desc.StencilPassOp		= D3D12_STENCIL_OP(pass + 1);
				desc.StencilFunc		= D3D12_COMPARISON_FUNC(func + 1);
			}
		};
		uint8		DepthEnable:1, DepthWriteMask:1, DepthFunc:3, StencilEnable:1;
		uint8		StencilReadMask, StencilWriteMask;
		StencilOp	FrontFace, BackFace;
		void GetDesc(D3D12_DEPTH_STENCIL_DESC &desc) const {
			desc.DepthEnable		= DepthEnable;
			desc.DepthWriteMask		= D3D12_DEPTH_WRITE_MASK(DepthWriteMask);
			desc.DepthFunc			= D3D12_COMPARISON_FUNC(DepthFunc + 1);
			desc.StencilEnable		= StencilEnable;
			desc.StencilReadMask	= StencilReadMask;
			desc.StencilWriteMask	= StencilWriteMask;
			FrontFace.GetDesc(desc.FrontFace);
			BackFace.GetDesc(desc.BackFace);
		}
		void	reset() {
			DepthWriteMask		= true;
			DepthFunc			= DT_LESS;
			StencilReadMask		= StencilWriteMask	= 0xff;
			FrontFace.func		= BackFace.func = STENCILFUNC_ALWAYS;
		}
	};

	const DX12Shader			*shader;
	const VertexDescription		*input_layout;
	const StreamOutDescription	*stream_output;
	TargetBlendState			blend[8];
	RasterizerState				rasterizer;
	DepthStencil				depthstencil;
	uint32						SampleQuality;
	uint32						SampleMask;
	uint8						blend_enable, logic_enable, SampleCount;
	uint8						AlphaToCoverageEnable:1, IndependentBlendEnable:1, IBStripCutValue:2, PrimitiveTopologyType:3;
	uint8						NumRenderTargets;
	uint8						RTVFormats[8], DSVFormat;

	void	reset() {
		clear(*this);
		SampleCount	= 1;
		SampleMask	= ~0;
		for (auto &i : blend)
			i.reset();
		rasterizer.reset();
		depthstencil.reset();
	}

	void GetDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc) const {
		if (stream_output)
			desc.StreamOutput	= *stream_output;
		if (input_layout)
			desc.InputLayout	= *input_layout;

		if (sized_data ss = shader->sub[SS_VERTEX]) {
			desc.VS.pShaderBytecode	= ss;
			desc.VS.BytecodeLength	= ss.length();
		}
		if (sized_data ss = shader->sub[SS_PIXEL]) {
			desc.PS.pShaderBytecode	= ss;
			desc.PS.BytecodeLength	= ss.length();
		}
		if (sized_data ss = shader->sub[SS_LOCAL]) {
			desc.DS.pShaderBytecode	= ss;
			desc.DS.BytecodeLength	= ss.length();
		}
		if (sized_data ss = shader->sub[SS_HULL]) {
			desc.HS.pShaderBytecode	= ss;
			desc.HS.BytecodeLength	= ss.length();
		}
		if (sized_data ss = shader->sub[SS_GEOMETRY]) {
			desc.GS.pShaderBytecode	= ss;
			desc.GS.BytecodeLength	= ss.length();
		}

		desc.NumRenderTargets = NumRenderTargets;

		desc.BlendState.AlphaToCoverageEnable	= AlphaToCoverageEnable;
		desc.BlendState.IndependentBlendEnable	= IndependentBlendEnable;
		for (int i = 0; i < NumRenderTargets; i++) {
			blend[i].GetDesc(desc.BlendState.RenderTarget[i]);
			desc.RTVFormats[i] = DXGI_FORMAT(RTVFormats[i]);
		}

		rasterizer.GetDesc(desc.RasterizerState);
		depthstencil.GetDesc(desc.DepthStencilState);

		desc.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE(IBStripCutValue);
		desc.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE(PrimitiveTopologyType);
		desc.DSVFormat				= DXGI_FORMAT(DSVFormat);
		desc.SampleDesc.Count		= SampleCount;
		desc.SampleDesc.Quality		= SampleQuality;
		desc.SampleMask				= SampleMask;
	}
};

class GraphicsContext {
	friend Graphics;

	enum {MAX_CBV = 8, MAX_SRV = 16, MAX_UAV = 16, MAX_SMP = 8, MAX_BARRIERS = 8};

	Graphics::CommandContext2*			cmd;
	bool								submitted;
	com_ptr<ID3D12RootSignature>		root_signature;
	com_ptr<ID3D12RootSignature>		compute_root_signature;
	com_ptr<ID3D12PipelineState>		pipeline_state;

	ResourceHandle2						render_buffers[RT_NUM];
	ResourceHandle2						depth_buffer;
	D3D12_CPU_DESCRIPTOR_HANDLE			rtv[RT_NUM], dsv;

	PackedGraphicsState					state;
	PrimType							prim;
	D3D12_VIEWPORT						viewport;
	D3D12_SAMPLER_DESC					samplers[MAX_SMP];
	Graphics::StageState				stages[SS_COUNT + 1];
	Graphics::Barriers<MAX_BARRIERS>	barriers;

	enum UPDATE {
		_UPD_SAMPLER		= 1 << 0,
		UPD_BLEND			= 1 << 8,
		UPD_DEPTH			= 1 << 9,
		UPD_RASTER			= 1 << 10,
		UPD_MISC			= 1 << 11,

		UPD_SAMPLER_ALL		= _UPD_SAMPLER * 0xff,
		UPD_GRAPHICS		= UPD_BLEND | UPD_DEPTH | UPD_RASTER | UPD_MISC,

		UPD_TARGETS			= 1 << 13,
		UPD_COMPUTE			= 1 << 14,
		UPD_SHADER			= 1 << 16,

		UPD_SHADER_PIXEL	= UPD_SHADER << SS_PIXEL,
		UPD_SHADER_VERTEX	= UPD_SHADER << SS_VERTEX,
		UPD_SHADER_GEOMETRY	= UPD_SHADER << SS_GEOMETRY,
		UPD_SHADER_HULL		= UPD_SHADER << SS_HULL,
		UPD_SHADER_LOCAL	= UPD_SHADER << SS_LOCAL,
		UPD_SHADER_COMPUTE	= UPD_SHADER << SS_COMPUTE,

		UPD_SHADER_ANY		= UPD_SHADER_PIXEL | UPD_SHADER_VERTEX | UPD_SHADER_GEOMETRY | UPD_SHADER_HULL | UPD_SHADER_LOCAL,
	};
	flags<UPDATE>		update;

	void				FlushDeferred2();
	inline void			FlushDeferred()	{
		if (update)
			FlushDeferred2();
		barriers.Flush(cmd->list);
	}

	inline int	Prim2Verts(PrimType prim, uint32 count) {
		//									  PL LL LS TL TS	  Adj: PL LL LS TL TS
		static const	uint8 mult[]	= {0, 1, 2, 1, 3, 1, 0, 0,	0, 0, 4, 1, 6, 2, 0, 0};
		static const	uint8 add[]		= {0, 0, 0, 1, 0, 2, 0, 0,	0, 0, 0, 3, 0, 4, 0, 0};
		return count * mult[prim] + add[prim];
	}

	inline void	SetSRV0(ShaderStage stage, Resource *r, int i)	{
		if (r) {
			if (stages[stage].SetSRV(r->cpu_handle, i))
				update.set(UPDATE(UPD_SHADER << stage));
		} else {
			stages[stage].ClearSRV(i);
		}
	}
	inline void	SetSRV(ShaderStage stage, Resource *r, int i)	{
		if (r) {
			Transition(r, stage == SS_PIXEL ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			if (stages[stage].SetSRV(r->cpu_handle, i))
				update.set(UPDATE(UPD_SHADER << stage));
		} else {
			stages[stage].ClearSRV(i);
		}
	}
	inline void	SetUAV(ShaderStage stage, Resource *r, int i)	{
		if (r) {
			Transition(r, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			if (stages[stage].SetUAV(r->uav_handle, i))
				update.set(UPDATE(UPD_SHADER << stage));
		} else {
			stages[stage].ClearUAV(i);
		}
	}


	void _SetRenderTarget(int i, Resource *r, const D3D12_RENDER_TARGET_VIEW_DESC &desc);
	void _SetZBuffer(Resource *r, const D3D12_DEPTH_STENCIL_VIEW_DESC &desc);

	void _ClearRenderTarget(int i);
	void _ClearZBuffer();

	void _SetTopology(PrimType _prim) {
		if (prim != _prim) {
			static const uint8	types[]	= {
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
			};
			uint8	type = _prim & 32 ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH : types[_prim & 7];
			if (state.PrimitiveTopologyType != type) {
				state.PrimitiveTopologyType = type;
				update.set(UPD_MISC);
			}
			cmd->list->IASetPrimitiveTopology((D3D_PRIMITIVE_TOPOLOGY)(prim = _prim));
		}
	}

	force_inline void	_SetIndices(D3D12_GPU_VIRTUAL_ADDRESS va, uint32 size, DXGI_FORMAT format) {
		D3D12_INDEX_BUFFER_VIEW	view = {va, size, format};
		cmd->list->IASetIndexBuffer(&view);
	}
	force_inline void	_SetVertices(D3D12_GPU_VIRTUAL_ADDRESS va, uint32 size, uint32 stride) {
		D3D12_VERTEX_BUFFER_VIEW	view = {va, size, stride};
		cmd->list->IASetVertexBuffers(0, 1, &view);
	}

public:
	GraphicsContext() : cmd(0), prim(PRIM_UNKNOWN) {}

	operator ID3D12GraphicsCommandList*()	const	{ return cmd->list; }
	FrameAllocator		&allocator()				{ return graphics.allocator(); }
	void				Begin();
	void				End();

	void				PushMarker(const char *s)	{}
	void				PopMarker()					{}

	void				SetWindow(const rect &rect);
	rect				GetWindow();
	void				Clear(param(colour) col, bool zbuffer = true);
	void				ClearZ();
	void				SetMSAA(MSAA _msaa)	{}

	inline void	Transition(Resource *r, D3D12_RESOURCE_STATES s)	{
		barriers.Transition(cmd->list, r, s);
	}
	inline void	 FlushTransitions() {
		barriers.Flush(cmd->list);
	}

	void	Blit(const Surface &dest, const Surface &srce, const point &dest_pos, const rect &srce_rect) {
		D3D12_BOX	box = {
			(uint32)srce_rect.a.x,	(uint32)srce_rect.a.y,	0,
			(uint32)srce_rect.b.x,	(uint32)srce_rect.b.y,	1
		};

		dx12::TEXTURE_COPY_LOCATION	dest_loc(*dest.get(), dest.CalcSubresource());//0, dx12::SUBRESOURCE_FOOTPRINT(dest.get()->GetDesc(), 0));
		dx12::TEXTURE_COPY_LOCATION	srce_loc(*srce.get(), srce.CalcSubresource());//0, dx12::SUBRESOURCE_FOOTPRINT(srce.get()->GetDesc(), 0));

		Transition(srce.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		Transition(dest.get(), D3D12_RESOURCE_STATE_COPY_DEST);
		FlushTransitions();
		cmd->list->CopyTextureRegion(
           &dest_loc, dest_pos.x, dest_pos.y, 0,
           &srce_loc, &box
		);
	}
	void	Blit(const Surface &dest, const Surface &srce, const point &dest_pos = zero)	{
		Blit(dest, srce, dest_pos, srce.GetRect());
	}

	// render targets
	void	SetRenderTarget(const Surface& s, RenderTarget i = RT_COLOUR0);
	void	SetZBuffer(const Surface& s);
	void	SetRenderTarget(Surface&& s, RenderTarget i = RT_COLOUR0);
	void	SetZBuffer(Surface&& s);

	Surface	GetRenderTarget(int i = 0);
	Surface	GetZBuffer();

	void	SetBuffer(ShaderStage stage,	Resource *r, int i = 0)				{ SetSRV(stage, r, i); }
	void	SetTexture(ShaderStage stage,	Resource *r, int i = 0)				{ SetSRV(stage, r, i); }
	void	SetRWBuffer(ShaderStage stage,	Resource *r, int i = 0)				{ SetUAV(stage, r, i); }
	void	SetRWTexture(ShaderStage stage,	Resource *r, int i = 0)				{ SetUAV(stage, r, i); }
	void	SetTexture(Texture &tex, int i = 0)									{ SetTexture(SS_PIXEL, tex, i); }
	void	SetConstBuffer(ShaderStage stage, ConstBuffer &buffer, int i = 0)	{ stages[stage].SetCB(&buffer, i); }

	void	MapStreamOut(uint8 b0, uint8 b1, uint8 b2, uint8 b3);
	void	SetStreamOut(int i, void *start, uint32 size, uint32 stride);
	void	GetStreamOut(int i, uint64 *pos);
	void	FlushStreamOut();

	// draw
	void				SetVertexType(const VertexDescription &vd) {
		state.input_layout = &vd;
		update.set(UPD_MISC);
	}
	template<typename T>void SetVertexType() {
		SetVertexType(GetVD<T>(0));//stages[SS_VERTEX].shader));
	}

	void SetIndices(const IndexBuffer<uint16> &ib) {
		_SetIndices(ib->gpu_address, ib.Size(), DXGI_FORMAT_R16_UINT);
	}
	void SetIndices(const IndexBuffer<uint32> &ib) {
		_SetIndices(ib->gpu_address, ib.Size(), DXGI_FORMAT_R32_UINT);
	}
	template<typename T>void SetVertices(const VertexBuffer<T> &vb, uint32 offset = 0) {
		SetVertexType<T>();
		_SetVertices(vb->gpu_address + offset * sizeof(T), vb.Size() - offset * sizeof(T), sizeof(T));
	}
	template<typename T>void SetVertices(uint32 stream, const VertexBuffer<T> &vb, uint32 offset = 0) {
		_SetVertices(vb->gpu_address + offset * sizeof(T), vb.Size() - offset * sizeof(T), sizeof(T));
	}
	void SetVertices(uint32 stream, const _VertexBuffer &vb, uint32 stride, uint32 offset = 0) {
		_SetVertices(vb->gpu_address + offset, vb.Size() - offset, stride);
	}

	force_inline void	DrawVertices(PrimType prim, uint32 start, uint32 count) {
		_SetTopology(prim);
		FlushDeferred();
		cmd->list->DrawInstanced(count, 1, start, 0);
	}
	force_inline void	DrawIndexedVertices(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start, uint32 count) {
		_SetTopology(prim);
		FlushDeferred();
		cmd->list->DrawIndexedInstanced(count, 1, start, min_index, 0);
	}
	force_inline	void	DrawPrimitive(PrimType prim, uint32 start, uint32 count) {
		DrawVertices(prim, start, Prim2Verts(prim, count));
	}
	force_inline	void	DrawIndexedPrimitive(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start, uint32 count) {
		DrawIndexedVertices(prim, min_index, num_verts, start, Prim2Verts(prim, count));
	}

	void	Dispatch(uint32 dimx, uint32 dimy, uint32 dimz);

	// shaders
	void	SetShader(const DX12Shader &s);
	void	SetShaderConstants(ShaderReg reg, const void *values);
	template<typename T> void	SetShaderConstants(const T *values, uint16 reg, uint32 count = 1) {
		SetShaderConstants(ShaderReg(reg, count, 0, SS_PIXEL, SPT_VAL), values);
	}
	// samplers
	force_inline	void	SetSamplerState(int i, TexState type, uint32 value) {
		D3D12_SAMPLER_DESC	&desc = samplers[i];
		switch (type) {
			case TS_FILTER:		desc.Filter			= (D3D12_FILTER)value; break;
			case TS_ADDRESS_U:	desc.AddressU		= (D3D12_TEXTURE_ADDRESS_MODE)value; break;
			case TS_ADDRESS_V:	desc.AddressV		= (D3D12_TEXTURE_ADDRESS_MODE)value; break;
			case TS_ADDRESS_W:	desc.AddressW		= (D3D12_TEXTURE_ADDRESS_MODE)value; break;
			case TS_MIP_BIAS:	desc.MipLODBias		= value; break;
			case TS_ANISO_MAX:	desc.MaxAnisotropy	= value; break;
			case TS_COMPARISON:	desc.ComparisonFunc = (D3D12_COMPARISON_FUNC)value; break;
			case TS_MIP_MIN:	desc.MinLOD			= value; break;
			case TS_MIP_MAX:	desc.MaxLOD			= value; break;
		}
		update.set(UPDATE(1<<i));
	}
	force_inline void	SetUVMode(int i, UVMode t)	{
		D3D12_SAMPLER_DESC	&desc = samplers[i];
		desc.AddressU		= D3D12_TEXTURE_ADDRESS_MODE(t & 15);
		desc.AddressV		= D3D12_TEXTURE_ADDRESS_MODE(t >> 4);
		update.set(UPDATE(1<<i));
	}
	force_inline void	SetTexFilter(int i, TexFilter t) {
		samplers[i].Filter	= (D3D12_FILTER)t;
		update.set(UPDATE(1<<i));
	}
	force_inline UVMode	GetUVMode(int i)	{
		D3D12_SAMPLER_DESC	&desc = samplers[i];
		return UVMode(desc.AddressU | (desc.AddressV << 4));
	}
	//raster
	force_inline void	SetFillMode(FillMode fill_mode)	{
		state.rasterizer.FillMode = fill_mode;
		update.set(UPD_RASTER);
	}
	force_inline void	SetBackFaceCull(BackFaceCull c)		{
		state.rasterizer.CullMode = c;
		update.set(UPD_RASTER);
	}
	force_inline void	SetDepthBias(float bias, float slope_bias) {
		state.rasterizer.DepthBias				= bias;
		state.rasterizer.SlopeScaledDepthBias	= slope_bias;
		update.set(UPD_RASTER);
	}

	//blend
	force_inline void	SetBlendEnable(bool enable) {
		bit_set(state.blend_enable, uint8(1 << 0), enable);
		update.set(UPD_BLEND);
	}
	force_inline void	SetBlend(BlendOp op, BlendFunc src, BlendFunc dest) {
		state.blend[0].src_dst_blends	= state.blend[0].src_dst_blends_alpha	= PackedGraphicsState::TargetBlendState::Blend(src, dest);
		state.blend[0].blend_op			= state.blend[0].blend_op_alpha			= op;
		update.set(UPD_BLEND);
	}
	force_inline void	SetBlendSeparate(BlendOp op, BlendFunc src, BlendFunc dest, BlendOp opAlpha, BlendFunc srcAlpha, BlendFunc destAlpha) {
		state.blend[0].src_dst_blends		= PackedGraphicsState::TargetBlendState::Blend(src, dest);
		state.blend[0].blend_op				= op;
		state.blend[0].src_dst_blends_alpha	= PackedGraphicsState::TargetBlendState::Blend(srcAlpha, destAlpha);
		state.blend[0].blend_op_alpha		= opAlpha;
		update.set(UPD_BLEND);
	}
	force_inline void	SetMask(uint32 mask) {
		state.blend[0].write_mask	= mask;
		update.set(UPD_BLEND);
	}
	force_inline void	SetAlphaToCoverage(bool enable) {
		state.AlphaToCoverageEnable	= enable;
		update.set(UPD_MISC);
	}
	force_inline void	SetBlendConst(param(colour) col) {
		cmd->list->OMSetBlendFactor((float*)&col);
	}

	//depth & stencil
	force_inline void	SetDepthTest(DepthTest c) {
		state.depthstencil.DepthFunc	= c;
		update.set(UPD_DEPTH);
	}
	force_inline void	SetDepthTestEnable(bool enable) {
		state.depthstencil.DepthEnable	= enable;
		update.set(UPD_DEPTH);
	}
	force_inline void	SetDepthWriteEnable(bool enable) {
		state.depthstencil.DepthWriteMask	= (D3D12_DEPTH_WRITE_MASK)enable;
		update.set(UPD_DEPTH);
	}

	force_inline void	SetStencilOp(StencilOp fail, StencilOp zfail, StencilOp zpass) {
		state.depthstencil.FrontFace.fail		= fail;
		state.depthstencil.FrontFace.depth_fail	= zfail;
		state.depthstencil.FrontFace.pass		= zpass;
		update.set(UPD_DEPTH);
	}

	force_inline void	SetStencilFunc(StencilFunc func, uint8 ref, uint8 mask) {
		state.depthstencil.FrontFace.func		= func;
		state.depthstencil.StencilReadMask		= mask;
		cmd->list->OMSetStencilRef(ref);
		update.set(UPD_DEPTH);
	}
	force_inline void	SetStencilRef(unsigned char ref) {
		cmd->list->OMSetStencilRef(ref);
	}

	force_inline void	Resolve(RenderTarget i = RT_COLOUR0) {}
};

//-----------------------------------------------------------------------------
//	ImmediateStream
//-----------------------------------------------------------------------------

class _ImmediateStream {
protected:
#if 1
	static	_VertexBuffer	vb;
	static	int				vbi, vbsize;
#endif
	GraphicsContext &ctx;
	int				count;
	PrimType		prim;
	void			*p;

	_ImmediateStream(GraphicsContext &ctx, PrimType prim, int count, uint32 vert_size);
	void			Draw();

};

template<class T> class ImmediateStream : _ImmediateStream {
public:
	ImmediateStream(GraphicsContext &ctx, PrimType prim, int count) : _ImmediateStream(ctx, prim, count, sizeof(T)) {}
	~ImmediateStream()						{ ctx.SetVertexType<T>(); Draw(); }
	T&				operator[](int i)		{ return ((T*)p)[i]; }
	T*				begin()					{ return (T*)p; }
	T*				end()					{ return (T*)p + count; }
	void			SetCount(int i)			{ ISO_ASSERT(i <= count); count = i; }
};

}
#endif	// GRAPHICS_DX12_H

