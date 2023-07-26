#include "extra/identifier.h"
#include <d3d11_1.h>
#include "d3d11shader.h" 
#include "..\dx_shared\dx_fields.h" 
#include "dx11\dx11_record.h"

using namespace	iso;

template<> const char *field_names<D3D11_CONTEXT_TYPE>::s[] = {
	"D3D11_CONTEXT_TYPE_ALL",						//0
	"D3D11_CONTEXT_TYPE_3D",						//1
	"D3D11_CONTEXT_TYPE_COMPUTE",					//2
	"D3D11_CONTEXT_TYPE_COPY",						//3
	"D3D11_CONTEXT_TYPE_VIDEO",						//4
};

template<> const char *field_names<D3D11_TEXTURE_LAYOUT>::s[] = {
	"D3D11_TEXTURE_LAYOUT_UNDEFINED",				// 0
	"D3D11_TEXTURE_LAYOUT_ROW_MAJOR",				// 1
	"D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE",	// 2
};

template<> const char *field_names<D3D11_CONSERVATIVE_RASTERIZATION_MODE>::s[] = {
	"D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF",	// 0
	"D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON",		// 1
};

template<> const char *field_names<D3D11_INPUT_CLASSIFICATION>::s[] = {
	"D3D11_INPUT_PER_VERTEX_DATA",		//0,
	"D3D11_INPUT_PER_INSTANCE_DATA",	//1
};

template<> field fields<D3D11_INPUT_ELEMENT_DESC_rel>::f[] = {
#undef S
#define S D3D11_INPUT_ELEMENT_DESC_rel
	_MAKE_FIELD(S,SemanticName)
	_MAKE_FIELD(S,SemanticIndex)
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,InputSlot)
	_MAKE_FIELD(S,AlignedByteOffset)
	_MAKE_FIELD(S,InputSlotClass)
	_MAKE_FIELD(S,InstanceDataStepRate)
	TERMINATOR
};

template<> const char *field_names<D3D11_FILL_MODE>::s[] = {
	0, 0,
	"D3D11_FILL_WIREFRAME",	//2,
	"D3D11_FILL_SOLID",		//3
};

template<> const char *field_names<D3D11_CULL_MODE>::s[] = {
	0,
	"D3D11_CULL_NONE",	//1,
	"D3D11_CULL_FRONT",	//2,
	"D3D11_CULL_BACK",	//3
};

template<> field fields<D3D11_SO_DECLARATION_ENTRY>::f[] = {
#undef S
#define S D3D11_SO_DECLARATION_ENTRY
	_MAKE_FIELD(S,Stream)
	_MAKE_FIELD(S,SemanticName)
	_MAKE_FIELD(S,SemanticIndex)
	_MAKE_FIELD(S,StartComponent)
	_MAKE_FIELD(S,ComponentCount)
	_MAKE_FIELD(S,OutputSlot)
	TERMINATOR
};

template<> field fields<D3D11_VIEWPORT>::f[] = {
#undef S
#define S D3D11_VIEWPORT
	_MAKE_FIELD(S,TopLeftX)
	_MAKE_FIELD(S,TopLeftY)
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,Height)
	_MAKE_FIELD(S,MinDepth)
	_MAKE_FIELD(S,MaxDepth)
	TERMINATOR
};

template<> const char *field_names<D3D11_RESOURCE_DIMENSION>::s[] = {
	"D3D11_RESOURCE_DIMENSION_UNKNOWN",		//0,
	"D3D11_RESOURCE_DIMENSION_BUFFER",		//1,
	"D3D11_RESOURCE_DIMENSION_TEXTURE1D",	//2,
	"D3D11_RESOURCE_DIMENSION_TEXTURE2D",	//3,
	"D3D11_RESOURCE_DIMENSION_TEXTURE3D",	//4
};

template<> const char *field_names<D3D11_DSV_DIMENSION>::s[] = {
	"D3D11_DSV_DIMENSION_UNKNOWN",			//0,
	"D3D11_DSV_DIMENSION_TEXTURE1D",		//1,
	"D3D11_DSV_DIMENSION_TEXTURE1DARRAY",	//2,
	"D3D11_DSV_DIMENSION_TEXTURE2D",		//3,
	"D3D11_DSV_DIMENSION_TEXTURE2DARRAY",	//4,
	"D3D11_DSV_DIMENSION_TEXTURE2DMS",		//5,
	"D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY",	//6
};

template<> const char *field_names<D3D11_RTV_DIMENSION>::s[] = {
	"D3D11_RTV_DIMENSION_UNKNOWN",			//0,
	"D3D11_RTV_DIMENSION_BUFFER",			//1,
	"D3D11_RTV_DIMENSION_TEXTURE1D",		//2,
	"D3D11_RTV_DIMENSION_TEXTURE1DARRAY",	//3,
	"D3D11_RTV_DIMENSION_TEXTURE2D",		//4,
	"D3D11_RTV_DIMENSION_TEXTURE2DARRAY",	//5,
	"D3D11_RTV_DIMENSION_TEXTURE2DMS",		//6,
	"D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY",	//7,
	"D3D11_RTV_DIMENSION_TEXTURE3D",		//8
};

template<> const char *field_names<D3D11_UAV_DIMENSION>::s[] = {
	"D3D11_UAV_DIMENSION_UNKNOWN",			//0,
	"D3D11_UAV_DIMENSION_BUFFER",			//1,
	"D3D11_UAV_DIMENSION_TEXTURE1D",		//2,
	"D3D11_UAV_DIMENSION_TEXTURE1DARRAY",	//3,
	"D3D11_UAV_DIMENSION_TEXTURE2D",		//4,
	"D3D11_UAV_DIMENSION_TEXTURE2DARRAY",	//5,
	"D3D11_UAV_DIMENSION_TEXTURE3D",		//8
};

template<> const char *field_names<D3D11_USAGE>::s[] = {
	"D3D11_USAGE_DEFAULT",					//0,
	"D3D11_USAGE_IMMUTABLE",				//1,
	"D3D11_USAGE_DYNAMIC",					//2,
	"D3D11_USAGE_STAGING",					//3
};

template<> struct field_names<D3D11_BIND_FLAG>		{ static field_bit s[];	};
field_bit field_names<D3D11_BIND_FLAG>::s[] = {
	{"D3D11_BIND_VERTEX_BUFFER",			0x1L	},
	{"D3D11_BIND_INDEX_BUFFER",				0x2L	},
	{"D3D11_BIND_CONSTANT_BUFFER",			0x4L	},
	{"D3D11_BIND_SHADER_RESOURCE",			0x8L	},
	{"D3D11_BIND_STREAM_OUTPUT",			0x10L	},
	{"D3D11_BIND_RENDER_TARGET",			0x20L	},
	{"D3D11_BIND_DEPTH_STENCIL",			0x40L	},
	{"D3D11_BIND_UNORDERED_ACCESS",			0x80L	},
	{"D3D11_BIND_DECODER",					0x200L	},
	{"D3D11_BIND_VIDEO_ENCODER",			0x400L	},
	0,
};

template<> struct field_names<D3D11_CPU_ACCESS_FLAG>		{ static field_bit s[];	};
field_bit field_names<D3D11_CPU_ACCESS_FLAG>::s[] = {
	{"D3D11_CPU_ACCESS_WRITE",	0x10000L	},
	{"D3D11_CPU_ACCESS_READ",	0x20000L	},
	0,
};

template<> struct field_names<D3D11_RESOURCE_MISC_FLAG>		{ static field_bit s[];	};
field_bit field_names<D3D11_RESOURCE_MISC_FLAG>::s[] = {
	{"D3D11_RESOURCE_MISC_GENERATE_MIPS",					0x1L	},
	{"D3D11_RESOURCE_MISC_SHARED",							0x2L	},
	{"D3D11_RESOURCE_MISC_TEXTURECUBE",						0x4L	},
	{"D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS",				0x10L	},
	{"D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS",			0x20L	},
	{"D3D11_RESOURCE_MISC_BUFFER_STRUCTURED",				0x40L	},
	{"D3D11_RESOURCE_MISC_RESOURCE_CLAMP",					0x80L	},
	{"D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX",				0x100L	},
	{"D3D11_RESOURCE_MISC_GDI_COMPATIBLE",					0x200L	},
	{"D3D11_RESOURCE_MISC_SHARED_NTHANDLE",					0x800L	},
	{"D3D11_RESOURCE_MISC_RESTRICTED_CONTENT",				0x1000L	},
	{"D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE",		0x2000L	},
	{"D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE_DRIVER",	0x4000L	},
	{"D3D11_RESOURCE_MISC_GUARDED",							0x8000L	},
	{"D3D11_RESOURCE_MISC_TILE_POOL",						0x20000L},
	{"D3D11_RESOURCE_MISC_TILED",							0x40000L},
	0,
};

template<> const char *field_names<D3D11_MAP>::s[] = {
	0,
	"D3D11_MAP_READ",					//1,
	"D3D11_MAP_WRITE",					//2,
	"D3D11_MAP_READ_WRITE",				//3,
	"D3D11_MAP_WRITE_DISCARD",			//4,
	"D3D11_MAP_WRITE_NO_OVERWRITE",		//5
};

template<> struct field_names<D3D11_MAP_FLAG>		{ static field_bit s[];	};
field_bit field_names<D3D11_MAP_FLAG>::s[] = {
	{"D3D11_MAP_FLAG_DO_NOT_WAIT",	0x100000L},
	0,
};

template<> const char *field_names<D3D11_RAISE_FLAG>::s[] = {
	0,
	"D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR",	//0x1L
};

template<> const char *field_names<D3D11_CLEAR_FLAG>::s[] = {
	0,
	"D3D11_CLEAR_DEPTH",	//0x1L,
	"D3D11_CLEAR_STENCIL",	//0x2L
};

template<> field fields<D3D11_BOX>::f[] = {
#undef S
#define S D3D11_BOX
	_MAKE_FIELD(S,left)
	_MAKE_FIELD(S,top)
	_MAKE_FIELD(S,front)
	_MAKE_FIELD(S,right)
	_MAKE_FIELD(S,bottom)
	_MAKE_FIELD(S,back)
	TERMINATOR
};

template<> const char *field_names<D3D11_COMPARISON_FUNC>::s[] = {
	0,
	"D3D11_COMPARISON_NEVER",			//1,
	"D3D11_COMPARISON_LESS",			//2,
	"D3D11_COMPARISON_EQUAL",			//3,
	"D3D11_COMPARISON_LESS_EQUAL",		//4,
	"D3D11_COMPARISON_GREATER",			//5,
	"D3D11_COMPARISON_NOT_EQUAL",		//6,
	"D3D11_COMPARISON_GREATER_EQUAL",	//7,
	"D3D11_COMPARISON_ALWAYS",			//8
};

template<> const char *field_names<D3D11_DEPTH_WRITE_MASK>::s[] = {
	"D3D11_DEPTH_WRITE_MASK_ZERO",		//0,
	"D3D11_DEPTH_WRITE_MASK_ALL",		//1
};

template<> const char *field_names<D3D11_STENCIL_OP>::s[] = {
	0,
	"D3D11_STENCIL_OP_KEEP",			//1,
	"D3D11_STENCIL_OP_ZERO",			//2,
	"D3D11_STENCIL_OP_REPLACE",			//3,
	"D3D11_STENCIL_OP_INCR_SAT",		//4,
	"D3D11_STENCIL_OP_DECR_SAT",		//5,
	"D3D11_STENCIL_OP_INVERT",			//6,
	"D3D11_STENCIL_OP_INCR",			//7,
	"D3D11_STENCIL_OP_DECR",			//8
	0,0,0,0,0,0,0
};

template<> field fields<D3D11_DEPTH_STENCILOP_DESC>::f[] = {
#undef S
#define S D3D11_DEPTH_STENCILOP_DESC
	_MAKE_FIELD(S,StencilFailOp)
	_MAKE_FIELD(S,StencilDepthFailOp)
	_MAKE_FIELD(S,StencilPassOp)
	_MAKE_FIELD(S,StencilFunc)
	TERMINATOR
};

template<> field fields<D3D11_DEPTH_STENCIL_DESC>::f[] = {
#undef S
#define S D3D11_DEPTH_STENCIL_DESC
	_MAKE_FIELD(S,DepthEnable)
	_MAKE_FIELD(S,DepthWriteMask)
	_MAKE_FIELD(S,DepthFunc)
	_MAKE_FIELD(S,StencilEnable)
	_MAKE_FIELD(S,StencilReadMask)
	_MAKE_FIELD(S,StencilWriteMask)
	_MAKE_FIELD(S,FrontFace)
	_MAKE_FIELD(S,BackFace)
	TERMINATOR
};

template<> const char *field_names<D3D11_BLEND>::s[] = {
	0,
	"D3D11_BLEND_ZERO",					//1,
	"D3D11_BLEND_ONE",					//2,
	"D3D11_BLEND_SRC_COLOR",			//3,
	"D3D11_BLEND_INV_SRC_COLOR",		//4,
	"D3D11_BLEND_SRC_ALPHA",			//5,
	"D3D11_BLEND_INV_SRC_ALPHA",		//6,
	"D3D11_BLEND_DEST_ALPHA",			//7,
	"D3D11_BLEND_INV_DEST_ALPHA",		//8,
	"D3D11_BLEND_DEST_COLOR",			//9,
	"D3D11_BLEND_INV_DEST_COLOR",		//10,
	"D3D11_BLEND_SRC_ALPHA_SAT",		//11,
	"D3D11_BLEND_BLEND_FACTOR",			//14,
	"D3D11_BLEND_INV_BLEND_FACTOR",		//15,
	"D3D11_BLEND_SRC1_COLOR",			//16,
	"D3D11_BLEND_INV_SRC1_COLOR",		//17,
	"D3D11_BLEND_SRC1_ALPHA",			//18,
	"D3D11_BLEND_INV_SRC1_ALPHA",		//19
};

template<> const char *field_names<D3D11_BLEND_OP>::s[] = {
	0,
	"D3D11_BLEND_OP_ADD",				//1,
	"D3D11_BLEND_OP_SUBTRACT",			//2,
	"D3D11_BLEND_OP_REV_SUBTRACT",		//3,
	"D3D11_BLEND_OP_MIN",				//4,
	"D3D11_BLEND_OP_MAX",				//5
};

template<> struct field_names<D3D11_COLOR_WRITE_ENABLE>		{ static field_bit s[];	};
field_bit	field_names<D3D11_COLOR_WRITE_ENABLE>::s[] = {
	{"D3D11_COLOR_WRITE_ENABLE_RED",	1},
	{"D3D11_COLOR_WRITE_ENABLE_GREEN",	2},
	{"D3D11_COLOR_WRITE_ENABLE_BLUE",	4},
	{"D3D11_COLOR_WRITE_ENABLE_ALPHA",	8},
	{"D3D11_COLOR_WRITE_ENABLE_ALL",	15},
	0,
};

template<> const char *field_names<D3D11_LOGIC_OP>::s[] = {
	"D3D11_LOGIC_OP_CLEAR",
	"D3D11_LOGIC_OP_SET",
	"D3D11_LOGIC_OP_COPY",
	"D3D11_LOGIC_OP_COPY_INVERTED",
	"D3D11_LOGIC_OP_NOOP",
	"D3D11_LOGIC_OP_INVERT",
	"D3D11_LOGIC_OP_AND",
	"D3D11_LOGIC_OP_NAND",
	"D3D11_LOGIC_OP_OR",
	"D3D11_LOGIC_OP_NOR",
	"D3D11_LOGIC_OP_XOR",
	"D3D11_LOGIC_OP_EQUIV",
	"D3D11_LOGIC_OP_AND_REVERSE",
	"D3D11_LOGIC_OP_AND_INVERTED",
	"D3D11_LOGIC_OP_OR_REVERSE",
	"D3D11_LOGIC_OP_OR_INVERTED",
};

template<> field fields<D3D11_RENDER_TARGET_BLEND_DESC>::f[] = {
#undef S
#define S D3D11_RENDER_TARGET_BLEND_DESC
	_MAKE_FIELD(S,BlendEnable)
	_MAKE_FIELD(S,SrcBlend)
	_MAKE_FIELD(S,DestBlend)
	_MAKE_FIELD(S,BlendOp)
	_MAKE_FIELD(S,SrcBlendAlpha)
	_MAKE_FIELD(S,DestBlendAlpha)
	_MAKE_FIELD(S,BlendOpAlpha)
	_MAKE_FIELD(S,RenderTargetWriteMask)
	TERMINATOR
};

template<> field fields<D3D11_RENDER_TARGET_BLEND_DESC1>::f[] = {
#undef S
#define S D3D11_RENDER_TARGET_BLEND_DESC1
	_MAKE_FIELD(S,BlendEnable)
	_MAKE_FIELD(S,LogicOpEnable)
	_MAKE_FIELD(S,SrcBlend)
	_MAKE_FIELD(S,DestBlend)
	_MAKE_FIELD(S,BlendOp)
	_MAKE_FIELD(S,SrcBlendAlpha)
	_MAKE_FIELD(S,DestBlendAlpha)
	_MAKE_FIELD(S,BlendOpAlpha)
	_MAKE_FIELD(S,LogicOp)
	_MAKE_FIELD(S,RenderTargetWriteMask)
	TERMINATOR
};

template<> field fields<D3D11_BLEND_DESC>::f[] = {
#undef S
#define S D3D11_BLEND_DESC
	_MAKE_FIELD(S,AlphaToCoverageEnable)
	_MAKE_FIELD(S,IndependentBlendEnable)
	_MAKE_FIELD(S,RenderTarget)
	TERMINATOR
};

template<> field fields<D3D11_BLEND_DESC1>::f[] = {
#undef S
#define S D3D11_BLEND_DESC1
	_MAKE_FIELD(S,AlphaToCoverageEnable)
	_MAKE_FIELD(S,IndependentBlendEnable)
	_MAKE_FIELD(S,RenderTarget)
	TERMINATOR
};

template<> field fields<D3D11_RASTERIZER_DESC>::f[] = {
#undef S
#define S D3D11_RASTERIZER_DESC
	_MAKE_FIELD(S,FillMode)
	_MAKE_FIELD(S,CullMode)
	_MAKE_FIELD(S,FrontCounterClockwise)
	_MAKE_FIELD(S,DepthBias)
	_MAKE_FIELD(S,DepthBiasClamp)
	_MAKE_FIELD(S,SlopeScaledDepthBias)
	_MAKE_FIELD(S,DepthClipEnable)
	_MAKE_FIELD(S,ScissorEnable)
	_MAKE_FIELD(S,MultisampleEnable)
	_MAKE_FIELD(S,AntialiasedLineEnable)
	TERMINATOR
};

template<> field fields<D3D11_RASTERIZER_DESC1>::f[] = {
#undef S
#define S D3D11_RASTERIZER_DESC1
	field::make<D3D11_RASTERIZER_DESC>(0, 0),
	_MAKE_FIELD(S,ForcedSampleCount)
	TERMINATOR
};

template<> field fields<D3D11_SUBRESOURCE_DATA>::f[] = {
#undef S
#define S D3D11_SUBRESOURCE_DATA
	_MAKE_FIELD(S,pSysMem)
	_MAKE_FIELD(S,SysMemPitch)
	_MAKE_FIELD(S,SysMemSlicePitch)
	TERMINATOR
};

template<> field fields<D3D11_MAPPED_SUBRESOURCE>::f[] = {
#undef S
#define S D3D11_MAPPED_SUBRESOURCE
	_MAKE_FIELD(S,pData)
	_MAKE_FIELD(S,RowPitch)
	_MAKE_FIELD(S,DepthPitch)
	TERMINATOR
};

template<> field fields<D3D11_BUFFER_DESC>::f[] = {
#undef S
#define S D3D11_BUFFER_DESC
	_MAKE_FIELD(S,ByteWidth)
	_MAKE_FIELD(S,Usage)
	_MAKE_FIELDT(S,BindFlags, D3D11_BIND_FLAG),
	_MAKE_FIELDT(S,CPUAccessFlags, D3D11_CPU_ACCESS_FLAG),
	_MAKE_FIELDT(S,MiscFlags, D3D11_RESOURCE_MISC_FLAG ),
	_MAKE_FIELD(S,StructureByteStride)
	TERMINATOR
};

template<> field fields<D3D11_TEXTURE1D_DESC>::f[] = {
#undef S
#define S D3D11_TEXTURE1D_DESC
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,ArraySize)
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,Usage)
	_MAKE_FIELDT(S,BindFlags, D3D11_BIND_FLAG),
	_MAKE_FIELDT(S,CPUAccessFlags, D3D11_CPU_ACCESS_FLAG),
	_MAKE_FIELDT(S,MiscFlags, D3D11_RESOURCE_MISC_FLAG ),
	TERMINATOR
};

template<> field fields<D3D11_TEXTURE2D_DESC>::f[] = {
#undef S
#define S D3D11_TEXTURE2D_DESC
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,Height)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,ArraySize)
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,SampleDesc)
	_MAKE_FIELD(S,Usage)
	_MAKE_FIELDT(S,BindFlags, D3D11_BIND_FLAG),
	_MAKE_FIELDT(S,CPUAccessFlags, D3D11_CPU_ACCESS_FLAG),
	_MAKE_FIELDT(S,MiscFlags, D3D11_RESOURCE_MISC_FLAG ),
	TERMINATOR
};
template<> field fields<D3D11_TEXTURE3D_DESC>::f[] = {
#undef S
#define S D3D11_TEXTURE3D_DESC
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,Height)
	_MAKE_FIELD(S,Depth)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,Usage)
	_MAKE_FIELDT(S,BindFlags, D3D11_BIND_FLAG),
	_MAKE_FIELDT(S,CPUAccessFlags, D3D11_CPU_ACCESS_FLAG),
	_MAKE_FIELDT(S,MiscFlags, D3D11_RESOURCE_MISC_FLAG ),
	TERMINATOR
};

template<> const char *field_names<D3D11_TEXTURECUBE_FACE>::s[] = {
	"D3D11_TEXTURECUBE_FACE_POSITIVE_X",	//0,
	"D3D11_TEXTURECUBE_FACE_NEGATIVE_X",	//1,
	"D3D11_TEXTURECUBE_FACE_POSITIVE_Y",	//2,
	"D3D11_TEXTURECUBE_FACE_NEGATIVE_Y",	//3,
	"D3D11_TEXTURECUBE_FACE_POSITIVE_Z",	//4,
	"D3D11_TEXTURECUBE_FACE_NEGATIVE_Z",	//5
};

template<> field fields<D3D11_BUFFER_SRV>::f[] = {
#undef S
#define S D3D11_BUFFER_SRV
	_MAKE_FIELD(S,FirstElement)
	_MAKE_FIELD(S,NumElements)
	TERMINATOR
};

template<> const char *field_names<D3D11_BUFFEREX_SRV_FLAG>::s[] = {
	"D3D11_BUFFEREX_SRV_FLAG_RAW",	//0x1
};

template<> field fields<D3D11_BUFFEREX_SRV>::f[] = {
#undef S
#define S D3D11_BUFFEREX_SRV
	_MAKE_FIELD(S,FirstElement)
	_MAKE_FIELD(S,NumElements)
	_MAKE_FIELD(S,Flags)
	TERMINATOR
};

template<> field fields<D3D11_TEX1D_SRV>::f[] = {
#undef S
#define S D3D11_TEX1D_SRV
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	TERMINATOR
};

template<> field fields<D3D11_TEX1D_ARRAY_SRV>::f[] = {
#undef S
#define S D3D11_TEX1D_ARRAY_SRV
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
};

template<> field fields<D3D11_TEX2D_SRV>::f[] = {
#undef S
#define S D3D11_TEX2D_SRV
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_ARRAY_SRV>::f[] = {
#undef S
#define S D3D11_TEX2D_ARRAY_SRV
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX3D_SRV>::f[] = {
#undef S
#define S D3D11_TEX3D_SRV
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	TERMINATOR
};

template<> field fields<D3D11_TEXCUBE_SRV>::f[] = {
#undef S
#define S D3D11_TEXCUBE_SRV
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	TERMINATOR
};

template<> field fields<D3D11_TEXCUBE_ARRAY_SRV>::f[] = {
#undef S
#define S D3D11_TEXCUBE_ARRAY_SRV
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,First2DArrayFace)
	_MAKE_FIELD(S,NumCubes)
};

template<> field fields<D3D11_TEX2DMS_SRV>::f[] = {
	TERMINATOR
};

template<> field fields<D3D11_TEX2DMS_ARRAY_SRV>::f[] = {
#undef S
#define S D3D11_TEX2DMS_ARRAY_SRV
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

field* D3D11_SHADER_RESOURCE_VIEW_DESC_union[] = {
	0,
	fields<D3D11_BUFFER_SRV>::f,
	fields<D3D11_TEX1D_SRV>::f,
	fields<D3D11_TEX1D_ARRAY_SRV>::f,
	fields<D3D11_TEX2D_SRV>::f,
	fields<D3D11_TEX2D_ARRAY_SRV>::f,
	fields<D3D11_TEX2DMS_SRV>::f,
	fields<D3D11_TEX2DMS_ARRAY_SRV>::f,
	fields<D3D11_TEX3D_SRV>::f,
	fields<D3D11_TEXCUBE_SRV>::f,
	fields<D3D11_TEXCUBE_ARRAY_SRV>::f,
	fields<D3D11_BUFFEREX_SRV>::f,
};
	
template<> field fields<D3D11_SHADER_RESOURCE_VIEW_DESC>::f[] = {
#undef S
#define S D3D11_SHADER_RESOURCE_VIEW_DESC
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,ViewDimension)
	MAKE_UNION(Buffer, 1, D3D11_SHADER_RESOURCE_VIEW_DESC_union),
	TERMINATOR
};
template<> field fields<D3D11_BUFFER_RTV>::f[] = {
#undef S
#define S D3D11_BUFFER_RTV
	_MAKE_FIELD(S,FirstElement)
	_MAKE_FIELD(S,NumElements)
	TERMINATOR
};

template<> field fields<D3D11_TEX1D_RTV>::f[] = {
#undef S
#define S D3D11_TEX1D_RTV
	_MAKE_FIELD(S,MipSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX1D_ARRAY_RTV>::f[] = {
#undef S
#define S D3D11_TEX1D_ARRAY_RTV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_RTV>::f[] = {
#undef S
#define S D3D11_TEX2D_RTV
	_MAKE_FIELD(S,MipSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX2DMS_RTV>::f[] = {
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_ARRAY_RTV>::f[] = {
#undef S
#define S D3D11_TEX2D_ARRAY_RTV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX2DMS_ARRAY_RTV>::f[] = {
#undef S
#define S D3D11_TEX2DMS_ARRAY_RTV
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX3D_RTV>::f[] = {
#undef S
#define S D3D11_TEX3D_RTV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstWSlice)
	_MAKE_FIELD(S,WSize)
	TERMINATOR
};

field *D3D11_RENDER_TARGET_VIEW_DESC_union[] = {
	0,
	fields<D3D11_BUFFER_RTV>::f,
	fields<D3D11_TEX1D_RTV>::f,
	fields<D3D11_TEX1D_ARRAY_RTV>::f,
	fields<D3D11_TEX2D_RTV>::f,
	fields<D3D11_TEX2D_ARRAY_RTV>::f,
	fields<D3D11_TEX2DMS_RTV>::f,
	fields<D3D11_TEX2DMS_ARRAY_RTV>::f,
	fields<D3D11_TEX3D_RTV>::f,
};

template<> field fields<D3D11_RENDER_TARGET_VIEW_DESC>::f[] = {
#undef S
#define S D3D11_RENDER_TARGET_VIEW_DESC
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,ViewDimension)
	MAKE_UNION(Buffer,1,D3D11_RENDER_TARGET_VIEW_DESC_union),
	TERMINATOR
};
template<> field fields<D3D11_TEX1D_DSV>::f[] = {
#undef S
#define S D3D11_TEX1D_DSV
	_MAKE_FIELD(S,MipSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX1D_ARRAY_DSV>::f[] = {
#undef S
#define S D3D11_TEX1D_ARRAY_DSV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_DSV>::f[] = {
#undef S
#define S D3D11_TEX2D_DSV
	_MAKE_FIELD(S,MipSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_ARRAY_DSV>::f[] = {
#undef S
#define S D3D11_TEX2D_ARRAY_DSV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX2DMS_DSV>::f[] = {
	TERMINATOR
};

template<> field fields<D3D11_TEX2DMS_ARRAY_DSV>::f[] = {
#undef S
#define S D3D11_TEX2DMS_ARRAY_DSV
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> const char *field_names<D3D11_DSV_FLAG>::s[] = {
	"D3D11_DSV_READ_ONLY_DEPTH",	//0x1L,
	"D3D11_DSV_READ_ONLY_STENCIL",	//0x2L
};

field* D3D11_DEPTH_STENCIL_VIEW_DESC_union[] = {
	0,
	fields<D3D11_TEX1D_DSV>::f,
	fields<D3D11_TEX1D_ARRAY_DSV>::f,
	fields<D3D11_TEX2D_DSV>::f,
	fields<D3D11_TEX2D_ARRAY_DSV>::f,
	fields<D3D11_TEX2DMS_DSV>::f,
	fields<D3D11_TEX2DMS_ARRAY_DSV>::f,
};
template<> field fields<D3D11_DEPTH_STENCIL_VIEW_DESC>::f[] = {
#undef S
#define S D3D11_DEPTH_STENCIL_VIEW_DESC
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,ViewDimension)
	_MAKE_FIELD(S,Flags)
	MAKE_UNION(Texture1D, 1, D3D11_DEPTH_STENCIL_VIEW_DESC_union),
	TERMINATOR
};

template<> struct field_names<D3D11_BUFFER_UAV_FLAG>		{ static field_bit s[];	};
field_bit field_names<D3D11_BUFFER_UAV_FLAG>::s[] = {
	{"D3D11_BUFFER_UAV_FLAG_RAW",		0x1	},
	{"D3D11_BUFFER_UAV_FLAG_APPEND",	0x2	},
	{"D3D11_BUFFER_UAV_FLAG_COUNTER",	0x4	},
	0,
};

template<> field fields<D3D11_BUFFER_UAV>::f[] = {
#undef S
#define S D3D11_BUFFER_UAV
	_MAKE_FIELD(S,FirstElement)
	_MAKE_FIELD(S,NumElements)
	_MAKE_FIELD(S,Flags)
	TERMINATOR
};

template<> field fields<D3D11_TEX1D_UAV>::f[] = {
#undef S
#define S D3D11_TEX1D_UAV
	_MAKE_FIELD(S,MipSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX1D_ARRAY_UAV>::f[] = {
#undef S
#define S D3D11_TEX1D_ARRAY_UAV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_UAV>::f[] = {
#undef S
#define S D3D11_TEX2D_UAV
	_MAKE_FIELD(S,MipSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_ARRAY_UAV>::f[] = {
#undef S
#define S D3D11_TEX2D_ARRAY_UAV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	TERMINATOR
};

template<> field fields<D3D11_TEX3D_UAV>::f[] = {
#undef S
#define S D3D11_TEX3D_UAV
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstWSlice)
	_MAKE_FIELD(S,WSize)
	TERMINATOR
};

field *D3D11_UNORDERED_ACCESS_VIEW_DESC_union[] = {
	0,
	fields<D3D11_BUFFER_UAV>::f,
	fields<D3D11_TEX1D_UAV>::f,
	fields<D3D11_TEX1D_ARRAY_UAV>::f,
	fields<D3D11_TEX2D_UAV>::f,
	fields<D3D11_TEX2D_ARRAY_UAV>::f,
	0,
	0,
	fields<D3D11_TEX3D_UAV>::f,
};

template<> field fields<D3D11_UNORDERED_ACCESS_VIEW_DESC>::f[] = {
#undef S
#define S D3D11_UNORDERED_ACCESS_VIEW_DESC
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,ViewDimension)
	MAKE_UNION(Buffer, 1, D3D11_UNORDERED_ACCESS_VIEW_DESC_union),
	TERMINATOR
};

template<> struct field_names<D3D11_FILTER>				{ static field_value s[];	};
field_value	field_names<D3D11_FILTER>::s[] = {
	{"D3D11_FILTER_MIN_MAG_MIP_POINT",							0},
	{"D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR",					0x1},
	{"D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT",				0x4},
	{"D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR",					0x5},
	{"D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT",					0x10},
	{"D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR",			0x11},
	{"D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT",					0x14},
	{"D3D11_FILTER_MIN_MAG_MIP_LINEAR",							0x15},
	{"D3D11_FILTER_ANISOTROPIC",								0x55},
	{"D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT",				0x80},
	{"D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR",		0x81},
	{"D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT",	0x84},
	{"D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR",		0x85},
	{"D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT",		0x90},
	{"D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR",	0x91},
	{"D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT",		0x94},
	{"D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR",				0x95},
	{"D3D11_FILTER_COMPARISON_ANISOTROPIC",						0xd5},
	{"D3D11_FILTER_MINIMUM_MIN_MAG_MIP_POINT",					0x100},
	{"D3D11_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR",			0x101},
	{"D3D11_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",		0x104},
	{"D3D11_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR",			0x105},
	{"D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT",			0x110},
	{"D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",	0x111},
	{"D3D11_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT",			0x114},
	{"D3D11_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR",					0x115},
	{"D3D11_FILTER_MINIMUM_ANISOTROPIC",						0x155},
	{"D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_POINT",					0x180},
	{"D3D11_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR",			0x181},
	{"D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",		0x184},
	{"D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR",			0x185},
	{"D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT",			0x190},
	{"D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",	0x191},
	{"D3D11_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT",			0x194},
	{"D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR",					0x195},
	{"D3D11_FILTER_MAXIMUM_ANISOTROPIC",						0x1d5},
	0,
};

template<> const char *field_names<D3D11_FILTER_TYPE>::s[] = {
	"D3D11_FILTER_TYPE_POINT",	//0,
	"D3D11_FILTER_TYPE_LINEAR",	//1
};

template<> const char *field_names<D3D11_FILTER_REDUCTION_TYPE>::s[] = {
	"D3D11_FILTER_REDUCTION_TYPE_STANDARD",	//0,
	"D3D11_FILTER_REDUCTION_TYPE_COMPARISON",	//1,
	"D3D11_FILTER_REDUCTION_TYPE_MINIMUM",	//2,
	"D3D11_FILTER_REDUCTION_TYPE_MAXIMUM",	//3
};

template<> const char *field_names<D3D11_TEXTURE_ADDRESS_MODE>::s[] = {
	0,
	"D3D11_TEXTURE_ADDRESS_WRAP",	//1,
	"D3D11_TEXTURE_ADDRESS_MIRROR",	//2,
	"D3D11_TEXTURE_ADDRESS_CLAMP",	//3,
	"D3D11_TEXTURE_ADDRESS_BORDER",	//4,
	"D3D11_TEXTURE_ADDRESS_MIRROR_ONCE",	//5
};

template<> field fields<D3D11_SAMPLER_DESC>::f[] = {
#undef S
#define S D3D11_SAMPLER_DESC
	_MAKE_FIELD(S,Filter)
	_MAKE_FIELD(S,AddressU)
	_MAKE_FIELD(S,AddressV)
	_MAKE_FIELD(S,AddressW)
	_MAKE_FIELD(S,MipLODBias)
	_MAKE_FIELD(S,MaxAnisotropy)
	_MAKE_FIELD(S,ComparisonFunc)
	_MAKE_FIELD(S,BorderColor)
	_MAKE_FIELD(S,MinLOD)
	_MAKE_FIELD(S,MaxLOD)
	TERMINATOR
};

template<> struct field_names<D3D11_FORMAT_SUPPORT>				{ static field_bit s[];	};
field_bit	field_names<D3D11_FORMAT_SUPPORT>::s[] = {
	{"D3D11_FORMAT_SUPPORT_BUFFER",						0x1},
	{"D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER",			0x2},
	{"D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER",			0x4},
	{"D3D11_FORMAT_SUPPORT_SO_BUFFER",					0x8},
	{"D3D11_FORMAT_SUPPORT_TEXTURE1D",					0x10},
	{"D3D11_FORMAT_SUPPORT_TEXTURE2D",					0x20},
	{"D3D11_FORMAT_SUPPORT_TEXTURE3D",					0x40},
	{"D3D11_FORMAT_SUPPORT_TEXTURECUBE",				0x80},
	{"D3D11_FORMAT_SUPPORT_SHADER_LOAD",				0x100},
	{"D3D11_FORMAT_SUPPORT_SHADER_SAMPLE",				0x200},
	{"D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON",	0x400},
	{"D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_MONO_TEXT",	0x800},
	{"D3D11_FORMAT_SUPPORT_MIP",						0x1000},
	{"D3D11_FORMAT_SUPPORT_MIP_AUTOGEN",				0x2000},
	{"D3D11_FORMAT_SUPPORT_RENDER_TARGET",				0x4000},
	{"D3D11_FORMAT_SUPPORT_BLENDABLE",					0x8000},
	{"D3D11_FORMAT_SUPPORT_DEPTH_STENCIL",				0x10000},
	{"D3D11_FORMAT_SUPPORT_CPU_LOCKABLE",				0x20000},
	{"D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE",		0x40000},
	{"D3D11_FORMAT_SUPPORT_DISPLAY",					0x80000},
	{"D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT",		0x100000},
	{"D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET",	0x200000},
	{"D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD",			0x400000},
	{"D3D11_FORMAT_SUPPORT_SHADER_GATHER",				0x800000},
	{"D3D11_FORMAT_SUPPORT_BACK_BUFFER_CAST",			0x1000000},
	{"D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW",0x2000000},
	{"D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON",	0x4000000},
	{"D3D11_FORMAT_SUPPORT_DECODER_OUTPUT",				0x8000000},
	{"D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT",		0x10000000},
	{"D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT",		0x20000000},
	{"D3D11_FORMAT_SUPPORT_VIDEO_ENCODER",				0x40000000},
	0,
};

template<> struct field_names<D3D11_FORMAT_SUPPORT2>		{ static field_bit s[];	};
field_bit field_names<D3D11_FORMAT_SUPPORT2>::s[] = {
	{"D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD",								0x1		},
	{"D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS",						0x2		},
	{"D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE",	0x4		},
	{"D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE",							0x8		},
	{"D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX",					0x10	},
	{"D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX",				0x20	},
	{"D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD",								0x40	},
	{"D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE",								0x80	},
	{"D3D11_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP",						0x100	},
	{"D3D11_FORMAT_SUPPORT2_TILED",											0x200	},
	{"D3D11_FORMAT_SUPPORT2_SHAREABLE",										0x400	},
	0,
};

template<> const char *field_names<D3D11_ASYNC_GETDATA_FLAG>::s[] = {
	"D3D11_ASYNC_GETDATA_DONOTFLUSH",	//0x1
};

template<> const char *field_names<D3D11_QUERY>::s[] = {
	"D3D11_QUERY_EVENT",	//0,
	"D3D11_QUERY_OCCLUSION",
	"D3D11_QUERY_TIMESTAMP",
	"D3D11_QUERY_TIMESTAMP_DISJOINT",
	"D3D11_QUERY_PIPELINE_STATISTICS",
	"D3D11_QUERY_OCCLUSION_PREDICATE",
	"D3D11_QUERY_SO_STATISTICS",
	"D3D11_QUERY_SO_OVERFLOW_PREDICATE",
	"D3D11_QUERY_SO_STATISTICS_STREAM0",
	"D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0",
	"D3D11_QUERY_SO_STATISTICS_STREAM1",
	"D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1",
	"D3D11_QUERY_SO_STATISTICS_STREAM2",
	"D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2",
	"D3D11_QUERY_SO_STATISTICS_STREAM3",
	"D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3",
};

template<> const char *field_names<D3D11_QUERY_MISC_FLAG>::s[] = {
	"D3D11_QUERY_MISC_PREDICATEHINT",	//0x1
};

template<> field fields<D3D11_QUERY_DESC>::f[] = {
#undef S
#define S D3D11_QUERY_DESC
	_MAKE_FIELD(S,Query)
	_MAKE_FIELD(S,MiscFlags)
	TERMINATOR
};

template<> field fields<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT>::f[] = {
#undef S
#define S D3D11_QUERY_DATA_TIMESTAMP_DISJOINT
	_MAKE_FIELD(S,Frequency)
	_MAKE_FIELD(S,Disjoint)
	TERMINATOR
};

template<> field fields<D3D11_QUERY_DATA_PIPELINE_STATISTICS>::f[] = {
#undef S
#define S D3D11_QUERY_DATA_PIPELINE_STATISTICS
	_MAKE_FIELD(S,IAVertices)
	_MAKE_FIELD(S,IAPrimitives)
	_MAKE_FIELD(S,VSInvocations)
	_MAKE_FIELD(S,GSInvocations)
	_MAKE_FIELD(S,GSPrimitives)
	_MAKE_FIELD(S,CInvocations)
	_MAKE_FIELD(S,CPrimitives)
	_MAKE_FIELD(S,PSInvocations)
	_MAKE_FIELD(S,HSInvocations)
	_MAKE_FIELD(S,DSInvocations)
	_MAKE_FIELD(S,CSInvocations)
	TERMINATOR
};

template<> field fields<D3D11_QUERY_DATA_SO_STATISTICS>::f[] = {
#undef S
#define S D3D11_QUERY_DATA_SO_STATISTICS
	_MAKE_FIELD(S,NumPrimitivesWritten)
	_MAKE_FIELD(S,PrimitivesStorageNeeded)
	TERMINATOR
};

template<> const char *field_names<D3D11_COUNTER>::s[] = {
	"D3D11_COUNTER_DEVICE_DEPENDENT_0",	//0x40000000
};

template<> const char *field_names<D3D11_COUNTER_TYPE>::s[] = {
	"D3D11_COUNTER_TYPE_FLOAT32",
	"D3D11_COUNTER_TYPE_UINT16",
	"D3D11_COUNTER_TYPE_UINT32",
	"D3D11_COUNTER_TYPE_UINT64",
};

template<> field fields<D3D11_COUNTER_DESC>::f[] = {
#undef S
#define S D3D11_COUNTER_DESC
	_MAKE_FIELD(S,Counter)
	_MAKE_FIELD(S,MiscFlags)
};

template<> field fields<D3D11_COUNTER_INFO>::f[] = {
#undef S
#define S D3D11_COUNTER_INFO
	_MAKE_FIELD(S,LastDeviceDependentCounter)
	_MAKE_FIELD(S,NumSimultaneousCounters)
	_MAKE_FIELD(S,NumDetectableParallelUnits)
	TERMINATOR
};

template<> struct field_names<D3D11_STANDARD_MULTISAMPLE_QUALITY_LEVELS>		{ static field_value s[];	};
field_value field_names<D3D11_STANDARD_MULTISAMPLE_QUALITY_LEVELS>::s[] = {
	{"D3D11_STANDARD_MULTISAMPLE_PATTERN",	0xffffffff},
	{"D3D11_CENTER_MULTISAMPLE_PATTERN",	0xfffffffe},
	0,
};

template<> const char *field_names<D3D11_DEVICE_CONTEXT_TYPE>::s[] = {
	"D3D11_DEVICE_CONTEXT_IMMEDIATE",
	"D3D11_DEVICE_CONTEXT_DEFERRED",
};

template<> field fields<D3D11_CLASS_INSTANCE_DESC>::f[] = {
#undef S
#define S D3D11_CLASS_INSTANCE_DESC
	_MAKE_FIELD(S,InstanceId)
	_MAKE_FIELD(S,InstanceIndex)
	_MAKE_FIELD(S,TypeId)
	_MAKE_FIELD(S,ConstantBuffer)
	_MAKE_FIELD(S,BaseConstantBufferOffset)
	_MAKE_FIELD(S,BaseTexture)
	_MAKE_FIELD(S,BaseSampler)
	_MAKE_FIELD(S,Created)
	TERMINATOR
};

template<> const char *field_names<D3D11_FEATURE>::s[] = {
	"D3D11_FEATURE_THREADING",	//0,
	"D3D11_FEATURE_DOUBLES",
	"D3D11_FEATURE_FORMAT_SUPPORT",
	"D3D11_FEATURE_FORMAT_SUPPORT2",
	"D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS",
	"D3D11_FEATURE_D3D11_OPTIONS",
	"D3D11_FEATURE_ARCHITECTURE_INFO",
	"D3D11_FEATURE_D3D9_OPTIONS",
	"D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT",
	"D3D11_FEATURE_D3D9_SHADOW_SUPPORT",
	"D3D11_FEATURE_D3D11_OPTIONS1",
	"D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT",
	"D3D11_FEATURE_MARKER_SUPPORT",
	"D3D11_FEATURE_D3D9_OPTIONS1",
};

template<> field fields<D3D11_FEATURE_DATA_THREADING>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_THREADING
	_MAKE_FIELD(S,DriverConcurrentCreates)
	_MAKE_FIELD(S,DriverCommandLists)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_DOUBLES>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_DOUBLES
	_MAKE_FIELD(S,DoublePrecisionFloatShaderOps)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_FORMAT_SUPPORT>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_FORMAT_SUPPORT
	_MAKE_FIELD(S,InFormat)
	_MAKE_FIELD(S,OutFormatSupport)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_FORMAT_SUPPORT2>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_FORMAT_SUPPORT2
	_MAKE_FIELD(S,InFormat)
	_MAKE_FIELD(S,OutFormatSupport2)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS
	_MAKE_FIELD(S,ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_D3D11_OPTIONS>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_D3D11_OPTIONS
	_MAKE_FIELD(S,OutputMergerLogicOp)
	_MAKE_FIELD(S,UAVOnlyRenderingForcedSampleCount)
	_MAKE_FIELD(S,DiscardAPIsSeenByDriver)
	_MAKE_FIELD(S,FlagsForUpdateAndCopySeenByDriver)
	_MAKE_FIELD(S,ClearView)
	_MAKE_FIELD(S,CopyWithOverlap)
	_MAKE_FIELD(S,ConstantBufferPartialUpdate)
	_MAKE_FIELD(S,ConstantBufferOffsetting)
	_MAKE_FIELD(S,MapNoOverwriteOnDynamicConstantBuffer)
	_MAKE_FIELD(S,MapNoOverwriteOnDynamicBufferSRV)
	_MAKE_FIELD(S,MultisampleRTVWithForcedSampleCountOne)
	_MAKE_FIELD(S,SAD4ShaderInstructions)
	_MAKE_FIELD(S,ExtendedDoublesShaderInstructions)
	_MAKE_FIELD(S,ExtendedResourceSharing)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_ARCHITECTURE_INFO>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_ARCHITECTURE_INFO
	_MAKE_FIELD(S,TileBasedDeferredRenderer)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_D3D9_OPTIONS>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_D3D9_OPTIONS
	_MAKE_FIELD(S,FullNonPow2TextureSupport)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT
	_MAKE_FIELD(S,SupportsDepthAsTextureWithLessEqualComparisonFilter)
	TERMINATOR
};

template<> const char *field_names<D3D11_SHADER_MIN_PRECISION_SUPPORT>::s[] = {
	0,
	"D3D11_SHADER_MIN_PRECISION_10_BIT",	//0x1,
	"D3D11_SHADER_MIN_PRECISION_16_BIT",	//0x2
};

template<> field fields<D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT
	_MAKE_FIELD(S,PixelShaderMinPrecision)
	_MAKE_FIELD(S,AllOtherShaderStagesMinPrecision)
	TERMINATOR
};

template<> const char *field_names<D3D11_TILED_RESOURCES_TIER>::s[] = {
	"D3D11_TILED_RESOURCES_NOT_SUPPORTED",	//0,
	"D3D11_TILED_RESOURCES_TIER_1",			//1,
	"D3D11_TILED_RESOURCES_TIER_2",			//2
};

template<> field fields<D3D11_FEATURE_DATA_D3D11_OPTIONS1>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_D3D11_OPTIONS1
	_MAKE_FIELD(S,TiledResourcesTier)
	_MAKE_FIELD(S,MinMaxFiltering)
	_MAKE_FIELD(S,ClearViewAlsoSupportsDepthOnlyFormats)
	_MAKE_FIELD(S,MapOnDefaultBuffers)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT
	_MAKE_FIELD(S,SimpleInstancingSupported)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_MARKER_SUPPORT>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_MARKER_SUPPORT
	_MAKE_FIELD(S,Profile)
	TERMINATOR
};

template<> field fields<D3D11_FEATURE_DATA_D3D9_OPTIONS1>::f[] = {
#undef S
#define S D3D11_FEATURE_DATA_D3D9_OPTIONS1
	_MAKE_FIELD(S,FullNonPow2TextureSupported)
	_MAKE_FIELD(S,DepthAsTextureWithLessEqualComparisonFilterSupported)
	_MAKE_FIELD(S,SimpleInstancingSupported)
	_MAKE_FIELD(S,TextureCubeFaceRenderTargetWithNonCubeDepthStencilSupported)
	TERMINATOR
};

template<> const char *field_names<D3D11_SHADER_VERSION_TYPE>::s[]	= {
	"D3D11_SHVER_PIXEL_SHADER",		//0
	"D3D11_SHVER_VERTEX_SHADER",	//1
	"D3D11_SHVER_GEOMETRY_SHADER",	//2
	"D3D11_SHVER_HULL_SHADER",		//3
	"D3D11_SHVER_DOMAIN_SHADER",	//4
	"D3D11_SHVER_COMPUTE_SHADER",	//5
};

//D3D_RESOURCE_RETURN_TYPE D3D11_RESOURCE_RETURN_TYPE;
//D3D_CBUFFER_TYPE D3D11_CBUFFER_TYPE;

template<> field fields<D3D11_SIGNATURE_PARAMETER_DESC>::f[] = {
#undef S
#define S D3D11_SIGNATURE_PARAMETER_DESC
	_MAKE_FIELD(S,SemanticName)
	_MAKE_FIELD(S,SemanticIndex)
	_MAKE_FIELD(S,Register)
	_MAKE_FIELD(S,SystemValueType)
	_MAKE_FIELD(S,ComponentType)
	_MAKE_FIELD(S,Mask)
	_MAKE_FIELD(S,ReadWriteMask)
	_MAKE_FIELD(S,Stream)
	_MAKE_FIELD(S,MinPrecision)
	0
};

template<> field fields<D3D11_SHADER_BUFFER_DESC>::f[] = {
#undef S
#define S D3D11_SHADER_BUFFER_DESC
	_MAKE_FIELD(S,Name)
	_MAKE_FIELD(S,Type)
	_MAKE_FIELD(S,Variables)
	_MAKE_FIELD(S,Size)
	_MAKE_FIELD(S,uFlags)
	0
};

template<> field fields<D3D11_SHADER_VARIABLE_DESC>::f[] = {
#undef S
#define S D3D11_SHADER_VARIABLE_DESC
	_MAKE_FIELD(S,Name)
	_MAKE_FIELD(S,StartOffset)
	_MAKE_FIELD(S,Size)
	_MAKE_FIELD(S,uFlags)
	_MAKE_FIELD(S,DefaultValue)
	_MAKE_FIELD(S,StartTexture)
	_MAKE_FIELD(S,TextureSize)
	_MAKE_FIELD(S,StartSampler)
	_MAKE_FIELD(S,SamplerSize)
	0
};

template<> field fields<D3D11_SHADER_TYPE_DESC>::f[] = {
#undef S
#define S D3D11_SHADER_TYPE_DESC
	_MAKE_FIELD(S,Class)
	_MAKE_FIELD(S,Type)
	_MAKE_FIELD(S,Rows)
	_MAKE_FIELD(S,Columns)
	_MAKE_FIELD(S,Elements)
	_MAKE_FIELD(S,Members)
	_MAKE_FIELD(S,Offset)
	_MAKE_FIELD(S,Name)
	0
};

//D3D_TESSELLATOR_DOMAIN D3D11_TESSELLATOR_DOMAIN;
//D3D_TESSELLATOR_PARTITIONING D3D11_TESSELLATOR_PARTITIONING;
//D3D_TESSELLATOR_OUTPUT_PRIMITIVE D3D11_TESSELLATOR_OUTPUT_PRIMITIVE;

template<> field fields<D3D11_SHADER_DESC>::f[] = {
#undef S
#define S D3D11_SHADER_DESC
	_MAKE_FIELD(S,Version)
	_MAKE_FIELD(S,Creator)
	_MAKE_FIELD(S,Flags)
    
	_MAKE_FIELD(S,ConstantBuffers)
	_MAKE_FIELD(S,BoundResources)
	_MAKE_FIELD(S,InputParameters)
	_MAKE_FIELD(S,OutputParameters)

	_MAKE_FIELD(S,InstructionCount)
	_MAKE_FIELD(S,TempRegisterCount)
	_MAKE_FIELD(S,TempArrayCount)
	_MAKE_FIELD(S,DefCount)
	_MAKE_FIELD(S,DclCount)
	_MAKE_FIELD(S,TextureNormalInstructions)
	_MAKE_FIELD(S,TextureLoadInstructions)
	_MAKE_FIELD(S,TextureCompInstructions)
	_MAKE_FIELD(S,TextureBiasInstructions)
	_MAKE_FIELD(S,TextureGradientInstructions)
	_MAKE_FIELD(S,FloatInstructionCount)
	_MAKE_FIELD(S,IntInstructionCount)
	_MAKE_FIELD(S,UintInstructionCount)
	_MAKE_FIELD(S,StaticFlowControlCount)
	_MAKE_FIELD(S,DynamicFlowControlCount)
	_MAKE_FIELD(S,MacroInstructionCount)
	_MAKE_FIELD(S,ArrayInstructionCount)
	_MAKE_FIELD(S,CutInstructionCount)
	_MAKE_FIELD(S,EmitInstructionCount)
	_MAKE_FIELD(S,GSOutputTopology)
	_MAKE_FIELD(S,GSMaxOutputVertexCount)
	_MAKE_FIELD(S,InputPrimitive)
	_MAKE_FIELD(S,PatchConstantParameters)
	_MAKE_FIELD(S,cGSInstanceCount)
	_MAKE_FIELD(S,cControlPoints)
	_MAKE_FIELD(S,HSOutputPrimitive)
	_MAKE_FIELD(S,HSPartitioning)
	_MAKE_FIELD(S,TessellatorDomain)
    // instruction counts
	_MAKE_FIELD(S,cBarrierInstructions)
	_MAKE_FIELD(S,cInterlockedInstructions)
	_MAKE_FIELD(S,cTextureStoreInstructions)
	0
};

template<> field fields<D3D11_SHADER_INPUT_BIND_DESC>::f[] = {
#undef S
#define S D3D11_SHADER_INPUT_BIND_DESC
	_MAKE_FIELD(S,Name)
	_MAKE_FIELD(S,Type)
	_MAKE_FIELD(S,BindPoint)
	_MAKE_FIELD(S,BindCount)
    
	_MAKE_FIELD(S,uFlags)
	_MAKE_FIELD(S,ReturnType)
	_MAKE_FIELD(S,Dimension)
	_MAKE_FIELD(S,NumSamples)
	0
};

template<> field fields<D3D11_LIBRARY_DESC>::f[] = {
#undef S
#define S D3D11_LIBRARY_DESC
	_MAKE_FIELD(S,Creator)
	_MAKE_FIELD(S,Flags)
	_MAKE_FIELD(S,FunctionCount)
	0
};

template<> field fields<D3D11_FUNCTION_DESC>::f[] = {
#undef S
#define S D3D11_FUNCTION_DESC
	_MAKE_FIELD(S,Version)
	_MAKE_FIELD(S,Creator)
	_MAKE_FIELD(S,Flags)
    
	_MAKE_FIELD(S,ConstantBuffers)
	_MAKE_FIELD(S,BoundResources)

	_MAKE_FIELD(S,InstructionCount)
	_MAKE_FIELD(S,TempRegisterCount)
	_MAKE_FIELD(S,TempArrayCount)
	_MAKE_FIELD(S,DefCount)
	_MAKE_FIELD(S,DclCount)
	_MAKE_FIELD(S,TextureNormalInstructions)
	_MAKE_FIELD(S,TextureLoadInstructions)
	_MAKE_FIELD(S,TextureCompInstructions)
	_MAKE_FIELD(S,TextureBiasInstructions)
	_MAKE_FIELD(S,TextureGradientInstructions)
	_MAKE_FIELD(S,FloatInstructionCount)
	_MAKE_FIELD(S,IntInstructionCount)
	_MAKE_FIELD(S,UintInstructionCount)
	_MAKE_FIELD(S,StaticFlowControlCount)
	_MAKE_FIELD(S,DynamicFlowControlCount)
	_MAKE_FIELD(S,MacroInstructionCount)
	_MAKE_FIELD(S,ArrayInstructionCount)
	_MAKE_FIELD(S,MovInstructionCount)
	_MAKE_FIELD(S,MovcInstructionCount)
	_MAKE_FIELD(S,ConversionInstructionCount)
	_MAKE_FIELD(S,BitwiseInstructionCount)
	_MAKE_FIELD(S,MinFeatureLevel)
	_MAKE_FIELD(S,RequiredFeatureFlags)

	_MAKE_FIELD(S,Name)
	_MAKE_FIELD(S,FunctionParameterCount)
	_MAKE_FIELD(S,HasReturn)
	_MAKE_FIELD(S,Has10Level9VertexShader)
	_MAKE_FIELD(S,Has10Level9PixelShader)
	0
};

template<> field fields<D3D11_PARAMETER_DESC>::f[] = {
#undef S
#define S D3D11_PARAMETER_DESC
	_MAKE_FIELD(S,Name)
	_MAKE_FIELD(S,SemanticName)
	_MAKE_FIELD(S,Type)
	_MAKE_FIELD(S,Class)
	_MAKE_FIELD(S,Rows)
	_MAKE_FIELD(S,Columns)
	_MAKE_FIELD(S,InterpolationMode)
	_MAKE_FIELD(S,Flags)

	_MAKE_FIELD(S,FirstInRegister)
	_MAKE_FIELD(S,FirstInComponent)
	_MAKE_FIELD(S,FirstOutRegister)
	_MAKE_FIELD(S,FirstOutComponent)
	0
};

//Error	LNK2001	unresolved external symbol "public: static char const * * iso::field_names<enum D3D11_CONTEXT_TYPE>::s" (?s@?$field_names@W4D3D11_CONTEXT_TYPE@@@iso@@2PAPEBDA)	ORBIScrude	D:\dev\orbiscrude\dx11_view.lib(view_dx11gpu.obj)	1	
//Error	LNK2001	unresolved external symbol "public: static char const * * iso::field_names<enum D3D11_FENCE_FLAG>::s" (?s@?$field_names@W4D3D11_FENCE_FLAG@@@iso@@2PAPEBDA)	ORBIScrude	D:\dev\orbiscrude\dx11_view.lib(view_dx11gpu.obj)	1	
//Error	LNK2001	unresolved external symbol "public: static struct iso::field * iso::fields<unsigned long>::f" (?f@?$fields@K@iso@@2PAUfield@2@A)	ORBIScrude	D:\dev\orbiscrude\dx11_view.lib(view_dx11gpu.obj)	1	

template<> field fields<D3D11_QUERY_DESC1>::f[] = {
#undef S
#define S D3D11_QUERY_DESC1
	_MAKE_FIELD(S,Query)
	_MAKE_FIELD(S,MiscFlags)
	_MAKE_FIELD(S,ContextType)
	TERMINATOR
};
template<> field fields<D3D11_RASTERIZER_DESC2>::f[] = {
#undef S
#define S D3D11_RASTERIZER_DESC2
	_MAKE_FIELD(S,FillMode)
	_MAKE_FIELD(S,CullMode)
	_MAKE_FIELD(S,FrontCounterClockwise)
	_MAKE_FIELD(S,DepthBias)
	_MAKE_FIELD(S,DepthBiasClamp)
	_MAKE_FIELD(S,SlopeScaledDepthBias)
	_MAKE_FIELD(S,DepthClipEnable)
	_MAKE_FIELD(S,ScissorEnable)
	_MAKE_FIELD(S,MultisampleEnable)
	_MAKE_FIELD(S,AntialiasedLineEnable)
	_MAKE_FIELD(S,ForcedSampleCount)
	_MAKE_FIELD(S,ConservativeRaster)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_RTV1>::f[] = {
#undef S
#define S D3D11_TEX2D_RTV1
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,PlaneSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_ARRAY_RTV1>::f[] = {
#undef S
#define S D3D11_TEX2D_ARRAY_RTV1
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	_MAKE_FIELD(S,PlaneSlice)
	TERMINATOR
};
field *D3D11_RENDER_TARGET_VIEW_DESC1_union[] = {
	0,
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Buffer),
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Texture1D),
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Texture1DArray),
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Texture2D),
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Texture2DArray),
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Texture2DMS),
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Texture2DMSArray),
	get_fields(&D3D11_RENDER_TARGET_VIEW_DESC1::Texture3D),
};
template<> field fields<D3D11_RENDER_TARGET_VIEW_DESC1>::f[] = {
#undef S
#define S D3D11_RENDER_TARGET_VIEW_DESC1
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,ViewDimension)
	MAKE_UNION(Buffer,1,D3D11_RENDER_TARGET_VIEW_DESC1_union),
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_SRV1>::f[] = {
#undef S
#define S D3D11_TEX2D_SRV1
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,PlaneSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_ARRAY_SRV1>::f[] = {
#undef S
#define S D3D11_TEX2D_ARRAY_SRV1
	_MAKE_FIELD(S,MostDetailedMip)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	_MAKE_FIELD(S,PlaneSlice)
	TERMINATOR
};
field *D3D11_SHADER_RESOURCE_VIEW_DESC1_union[] = {
	0,
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Buffer),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Texture1D),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Texture1DArray),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Texture2D),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Texture2DArray),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Texture2DMS),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Texture2DMSArray),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::Texture3D),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::TextureCube),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::TextureCubeArray),
	get_fields(&D3D11_SHADER_RESOURCE_VIEW_DESC1::BufferEx),
};
template<> field fields<D3D11_SHADER_RESOURCE_VIEW_DESC1>::f[] = {
#undef S
#define S D3D11_SHADER_RESOURCE_VIEW_DESC1
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,ViewDimension)
	MAKE_UNION(Buffer,1,D3D11_SHADER_RESOURCE_VIEW_DESC1_union),
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_UAV1>::f[] = {
#undef S
#define S D3D11_TEX2D_UAV1
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,PlaneSlice)
	TERMINATOR
};

template<> field fields<D3D11_TEX2D_ARRAY_UAV1>::f[] = {
#undef S
#define S D3D11_TEX2D_ARRAY_UAV1
	_MAKE_FIELD(S,MipSlice)
	_MAKE_FIELD(S,FirstArraySlice)
	_MAKE_FIELD(S,ArraySize)
	_MAKE_FIELD(S,PlaneSlice)
	TERMINATOR
};
field *D3D11_UNORDERED_ACCESS_VIEW_DESC1_union[] = {
	0,
	get_fields(&D3D11_UNORDERED_ACCESS_VIEW_DESC1::Buffer),
	get_fields(&D3D11_UNORDERED_ACCESS_VIEW_DESC1::Texture1D),
	get_fields(&D3D11_UNORDERED_ACCESS_VIEW_DESC1::Texture1DArray),
	get_fields(&D3D11_UNORDERED_ACCESS_VIEW_DESC1::Texture2D),
	get_fields(&D3D11_UNORDERED_ACCESS_VIEW_DESC1::Texture2DArray),
	0,
	0,
	get_fields(&D3D11_UNORDERED_ACCESS_VIEW_DESC1::Texture3D),
};
template<> field fields<D3D11_UNORDERED_ACCESS_VIEW_DESC1>::f[] = {
#undef S
#define S D3D11_UNORDERED_ACCESS_VIEW_DESC1
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,ViewDimension)
	MAKE_UNION(Buffer,1,D3D11_UNORDERED_ACCESS_VIEW_DESC1_union),
	TERMINATOR
};

template<> field fields<D3D11_TEXTURE2D_DESC1>::f[] = {
#undef S
#define S D3D11_TEXTURE2D_DESC1
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,Height)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,ArraySize)
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,SampleDesc)
	_MAKE_FIELD(S,Usage)
	_MAKE_FIELD(S,BindFlags)
	_MAKE_FIELD(S,CPUAccessFlags)
	_MAKE_FIELD(S,MiscFlags)
	_MAKE_FIELD(S,TextureLayout)
	TERMINATOR
};

template<> field fields<D3D11_TEXTURE3D_DESC1>::f[] = {
#undef S
#define S D3D11_TEXTURE3D_DESC1
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,Height)
	_MAKE_FIELD(S,Depth)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,Usage)
	_MAKE_FIELD(S,BindFlags)
	_MAKE_FIELD(S,CPUAccessFlags)
	_MAKE_FIELD(S,MiscFlags)
	_MAKE_FIELD(S,TextureLayout)
	TERMINATOR
};

template<> field fields<D3D11_TILED_RESOURCE_COORDINATE>::f[] = {
#undef S
#define S D3D11_TILED_RESOURCE_COORDINATE
	_MAKE_FIELD(S,X)
	_MAKE_FIELD(S,Y)
	_MAKE_FIELD(S,Z)
	_MAKE_FIELD(S,Subresource)
	TERMINATOR
};

template<> field fields<D3D11_TILE_REGION_SIZE>::f[] = {
#undef S
#define S D3D11_TILE_REGION_SIZE
	_MAKE_FIELD(S,NumTiles)
	_MAKE_FIELD(S,bUseBox)
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,Height)
	_MAKE_FIELD(S,Depth)
	TERMINATOR
};

MAKE_FIELDS(D3D11_DRAW_INSTANCED_INDIRECT_ARGS, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
MAKE_FIELDS(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

template<> struct field_names<D3D11_FENCE_FLAG>	{ static field_bit s[];	};
field_bit	field_names<D3D11_FENCE_FLAG>::s[] = {
	{"D3D11_FENCE_FLAG_NONE",					1},
	{"D3D11_FENCE_FLAG_SHARED",					2},
	{"D3D11_FENCE_FLAG_SHARED_CROSS_ADAPTER",	4},
	0,
};

MAKE_FIELDS(dx11::DeviceContext_State::Stage, shader, cb, srv, smp);
MAKE_FIELDS(dx11::DeviceContext_State::VertexBuffer, buffer, stride, offset);
MAKE_FIELDS(dx11::DeviceContext_State::IndexBuffer, buffer, format, offset);

MAKE_FIELDS(dx11::DeviceContext_State,
	vs, ps, ds, hs, gs, cs,
	ps_uav, cs_uav,
	so_buffer, so_offset,
	rs, num_viewport, num_scissor, viewport, scissor,
	ia, ib, vb, prim,
	dsv, rtv, blend, blend_factor, sample_mask, depth_stencil,stencil_ref
);
