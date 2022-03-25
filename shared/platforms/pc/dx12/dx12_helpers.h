#ifndef DX12_HELPERS_H
#define DX12_HELPERS_H

#include "base/defs.h"
#include "com.h"
#include <d3d12.h>

namespace dx12 {
using namespace iso;

struct DEFAULT {};
extern const DECLSPEC_SELECTANY DEFAULT D3D12_DEFAULT;

template<int HT, int RT, int T> struct D3D12_ENUM {
	operator D3D12_DESCRIPTOR_HEAP_TYPE()	{ return D3D12_DESCRIPTOR_HEAP_TYPE(HT); }
	operator D3D12_DESCRIPTOR_RANGE_TYPE()	{ return D3D12_DESCRIPTOR_RANGE_TYPE(RT); }
	operator D3D12_ROOT_PARAMETER_TYPE()	{ return D3D12_ROOT_PARAMETER_TYPE(T); }
};

typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	D3D12_DESCRIPTOR_RANGE_TYPE_SRV,	D3D12_ROOT_PARAMETER_TYPE_SRV>	_SRV;
typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	D3D12_DESCRIPTOR_RANGE_TYPE_UAV,	D3D12_ROOT_PARAMETER_TYPE_UAV>	_UAV;
typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	D3D12_DESCRIPTOR_RANGE_TYPE_CBV,	D3D12_ROOT_PARAMETER_TYPE_CBV>	_CBV;
typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,		D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,-1>								_SMP;

extern _SRV	SRV;
extern _UAV	UAV;
extern _CBV	CBV;
extern _SMP	SMP;

//------------------------------------------------------------------------------------------------
/*
struct RECT : D3D12_RECT {
	RECT() {}
	RECT(int Left, int Top, int Right, int Bottom) {
		left	= Left;
		top		= Top;
		right	= Right;
		bottom	= Bottom;
	}
};
*/
//------------------------------------------------------------------------------------------------

struct BOX : D3D12_BOX {
	BOX() {}
	BOX(int Left, int Right) {
		left	= Left;
		top		= 0;
		front	= 0;
		right	= Right;
		bottom	= 1;
		back	= 1;
	}
	BOX(int Left, int Top, int Right, int Bottom) {
		left	= Left;
		top		= Top;
		front	= 0;
		right	= Right;
		bottom	= Bottom;
		back	= 1;
	}
	BOX(int Left, int Top, int Front, int Right, int Bottom, int Back) {
		left	= Left;
		top		= Top;
		front	= Front;
		right	= Right;
		bottom	= Bottom;
		back	= Back;
	}
};

//------------------------------------------------------------------------------------------------

struct RESOURCE_ALLOCATION_INFO : D3D12_RESOURCE_ALLOCATION_INFO {
	RESOURCE_ALLOCATION_INFO() {}
	RESOURCE_ALLOCATION_INFO(uint64 size, uint64 alignment) {
		SizeInBytes = size;
		Alignment	= alignment;
	}
};

//------------------------------------------------------------------------------------------------

struct COMMAND_QUEUE_DESC : D3D12_COMMAND_QUEUE_DESC {
	COMMAND_QUEUE_DESC(D3D12_COMMAND_LIST_TYPE type, int priority = 0, D3D12_COMMAND_QUEUE_FLAGS flags = D3D12_COMMAND_QUEUE_FLAG_NONE, uint32 nodeMask = 1) {
		Type		= type;
		Priority	= priority;
		Flags		= flags;
		NodeMask	= nodeMask;
	}
};

//------------------------------------------------------------------------------------------------

struct HEAP_PROPERTIES : D3D12_HEAP_PROPERTIES {
	HEAP_PROPERTIES() {}
	HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY page, D3D12_MEMORY_POOL pool, uint32 creationNodeMask = 1, uint32 nodeMask = 1) {
		Type					= D3D12_HEAP_TYPE_CUSTOM;
		CPUPageProperty			= page;
		MemoryPoolPreference	= pool;
		CreationNodeMask		= creationNodeMask;
		VisibleNodeMask			= nodeMask;
	}
	HEAP_PROPERTIES(D3D12_HEAP_TYPE type, uint32 creationNodeMask = 1, uint32 nodeMask = 1) {
		Type					= type;
		CPUPageProperty			= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
		CreationNodeMask		= creationNodeMask;
		VisibleNodeMask			= nodeMask;
	}
	bool IsCPUAccessible() const {
		return Type == D3D12_HEAP_TYPE_UPLOAD || Type == D3D12_HEAP_TYPE_READBACK
			|| (Type == D3D12_HEAP_TYPE_CUSTOM && (CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK));
	}
	constexpr operator const D3D12_HEAP_PROPERTIES*() const { return this; }
};

//------------------------------------------------------------------------------------------------

struct HEAP_DESC : D3D12_HEAP_DESC {
	HEAP_DESC() {}
	HEAP_DESC(uint64 size, D3D12_HEAP_PROPERTIES properties, uint64 alignment = 0, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
		SizeInBytes = size;
		Properties	= properties;
		Alignment	= alignment;
		Flags		= flags;
	}
	HEAP_DESC(uint64 size, D3D12_HEAP_TYPE type, uint64 alignment = 0, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
		SizeInBytes	= size;
		Properties	= HEAP_PROPERTIES(type);
		Alignment	= alignment;
		Flags		= flags;
	}
	HEAP_DESC(uint64 size, D3D12_CPU_PAGE_PROPERTY page, D3D12_MEMORY_POOL pool, uint64 alignment = 0, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
		SizeInBytes = size;
		Properties	= HEAP_PROPERTIES(page, pool);
		Alignment	= alignment;
		Flags		= flags;
	}
	HEAP_DESC(const D3D12_RESOURCE_ALLOCATION_INFO& info, D3D12_HEAP_PROPERTIES properties, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
		SizeInBytes = info.SizeInBytes;
		Properties	= properties;
		Alignment	= info.Alignment;
		Flags		= flags;
	}
	HEAP_DESC(const D3D12_RESOURCE_ALLOCATION_INFO& info, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
		SizeInBytes	= info.SizeInBytes;
		Properties	= HEAP_PROPERTIES(type);
		Alignment	= info.Alignment;
		Flags		= flags;
	}
	HEAP_DESC(const D3D12_RESOURCE_ALLOCATION_INFO& info, D3D12_CPU_PAGE_PROPERTY page, D3D12_MEMORY_POOL pool, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
		SizeInBytes	= info.SizeInBytes;
		Properties	= HEAP_PROPERTIES(page, pool);
		Alignment	= info.Alignment;
		Flags		= flags;
	}
	bool IsCPUAccessible() const {
		return ((const HEAP_PROPERTIES*)&Properties)->IsCPUAccessible();
	}
};

//------------------------------------------------------------------------------------------------

struct QUERY_HEAP_DESC : D3D12_QUERY_HEAP_DESC {
	QUERY_HEAP_DESC()	{}
	QUERY_HEAP_DESC(D3D12_QUERY_HEAP_TYPE type, uint32 count, uint32 nodeMask = 1) {
		Type		= type;
		Count		= count;
		NodeMask	= nodeMask;
	}
	constexpr operator const D3D12_QUERY_HEAP_DESC*() const { return this; }
};

//------------------------------------------------------------------------------------------------

struct CLEAR_VALUE : D3D12_CLEAR_VALUE {
	CLEAR_VALUE() {}
	CLEAR_VALUE(DXGI_FORMAT format, const float color[4]) {
		Format	= format;
		memcpy(Color, color, sizeof(Color));
	}
	CLEAR_VALUE(DXGI_FORMAT format, float depth, uint8 stencil) {
		Format	= format;
		/* Use memcpy to preserve NAN values */
		memcpy(&DepthStencil.Depth, &depth, sizeof(depth));
		DepthStencil.Stencil = stencil;
	}
};

//------------------------------------------------------------------------------------------------

struct RANGE : D3D12_RANGE {
	RANGE() {}
	RANGE(const _none&)				{ Begin	= End = 0; }
	RANGE(size_t begin, size_t end) { Begin = begin; End = end; }
};

//------------------------------------------------------------------------------------------------

struct SHADER_BYTECODE : D3D12_SHADER_BYTECODE {
	SHADER_BYTECODE() {}
	SHADER_BYTECODE(ID3DBlob* pShaderBlob) {
		pShaderBytecode = pShaderBlob->GetBufferPointer();
		BytecodeLength	= pShaderBlob->GetBufferSize();
	}
	SHADER_BYTECODE(void* code, size_t len) {
		pShaderBytecode = code;
		BytecodeLength	= len;
	}
};

//------------------------------------------------------------------------------------------------

struct TILED_RESOURCE_COORDINATE : D3D12_TILED_RESOURCE_COORDINATE {
	TILED_RESOURCE_COORDINATE() {}
	TILED_RESOURCE_COORDINATE(uint32 x, uint32 y, uint32 z, uint32 subresource) {
		X			= x;
		Y			= y;
		Z			= z;
		Subresource	= subresource;
	}
};

//------------------------------------------------------------------------------------------------

struct TILE_REGION_SIZE : D3D12_TILE_REGION_SIZE {
	TILE_REGION_SIZE() {}
	TILE_REGION_SIZE(uint32 numTiles, bool useBox, uint32 width, uint16 height, uint16 depth) {
		NumTiles	= numTiles;
		UseBox		= useBox;
		Width		= width;
		Height		= height;
		Depth		= depth;
	}
};

//------------------------------------------------------------------------------------------------

struct SUBRESOURCE_TILING : D3D12_SUBRESOURCE_TILING {
	SUBRESOURCE_TILING() {}
	SUBRESOURCE_TILING(uint32 widthInTiles, uint16 heightInTiles, uint16 depthInTiles, uint32 startTileIndexInOverallResource) {
		WidthInTiles	= widthInTiles;
		HeightInTiles	= heightInTiles;
		DepthInTiles	= depthInTiles;
		StartTileIndexInOverallResource = startTileIndexInOverallResource;
	}
};

//------------------------------------------------------------------------------------------------

struct TILE_SHAPE : D3D12_TILE_SHAPE {
	TILE_SHAPE() {}
	TILE_SHAPE(uint32 widthInTexels, uint32 heightInTexels, uint32 depthInTexels) {
		WidthInTexels	= widthInTexels;
		HeightInTexels	= heightInTexels;
		DepthInTexels	= depthInTexels;
	}
};

//------------------------------------------------------------------------------------------------

struct RESOURCE_BARRIER : D3D12_RESOURCE_BARRIER {
	RESOURCE_BARRIER() {}
	static inline D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) {
		D3D12_RESOURCE_BARRIER result;
		clear(result);
		result.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		result.Flags					= flags;
		result.Transition.pResource		= res;
		result.Transition.StateBefore	= before;
		result.Transition.StateAfter	= after;
		result.Transition.Subresource	= subresource;
		return result;
	}
	static inline D3D12_RESOURCE_BARRIER Aliasing(ID3D12Resource* before, ID3D12Resource* after) {
		D3D12_RESOURCE_BARRIER result;
		clear(result);
		result.Type						= D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
		result.Aliasing.pResourceBefore = before;
		result.Aliasing.pResourceAfter	= after;
		return result;
	}
	static inline D3D12_RESOURCE_BARRIER UAV(ID3D12Resource* res) {
		D3D12_RESOURCE_BARRIER result;
		clear(result);
		result.Type				= D3D12_RESOURCE_BARRIER_TYPE_UAV;
		result.UAV.pResource	= res;
		return result;
	}
};

//------------------------------------------------------------------------------------------------

struct PACKED_MIP_INFO : D3D12_PACKED_MIP_INFO {
	PACKED_MIP_INFO() {}
	PACKED_MIP_INFO(uint8 numStandardMips, uint8 numPackedMips, uint32 numTilesForPackedMips, uint32 startTileIndexInOverallResource) {
		NumStandardMips			= numStandardMips;
		NumPackedMips			= numPackedMips;
		NumTilesForPackedMips	= numTilesForPackedMips;
		StartTileIndexInOverallResource = startTileIndexInOverallResource;
	}
};

//------------------------------------------------------------------------------------------------

struct SUBRESOURCE_FOOTPRINT : D3D12_SUBRESOURCE_FOOTPRINT {
	SUBRESOURCE_FOOTPRINT() {}
	SUBRESOURCE_FOOTPRINT(DXGI_FORMAT format, uint32 width, uint32 height, uint32 depth, uint32 rowPitch) {
		Format		= format;
		Width		= width;
		Height		= height;
		Depth		= depth;
		RowPitch	= rowPitch;
	}
	SUBRESOURCE_FOOTPRINT(const D3D12_RESOURCE_DESC& resDesc, uint32 rowPitch) {
		Format		= resDesc.Format;
		Width		= uint32(resDesc.Width);
		Height		= resDesc.Height;
		Depth		= resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? resDesc.DepthOrArraySize : 1;
		RowPitch	= rowPitch;
	}
};

//------------------------------------------------------------------------------------------------

struct TEXTURE_COPY_LOCATION : D3D12_TEXTURE_COPY_LOCATION {
	TEXTURE_COPY_LOCATION() {}
	TEXTURE_COPY_LOCATION(ID3D12Resource* res) { pResource = res; }
	TEXTURE_COPY_LOCATION(ID3D12Resource* res, uint64 offset, const D3D12_SUBRESOURCE_FOOTPRINT &footprint) {
		pResource		= res;
		Type			= D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		PlacedFootprint.Offset		= offset;
		PlacedFootprint.Footprint	= footprint;
	}
	TEXTURE_COPY_LOCATION(ID3D12Resource* res, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &footprint) {
		pResource		= res;
		Type			= D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		PlacedFootprint	= footprint;
	}
	TEXTURE_COPY_LOCATION(ID3D12Resource* res, uint32 sub) {
		pResource		= res;
		Type			= D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		SubresourceIndex = sub;
	}
};

//------------------------------------------------------------------------------------------------

inline uint8 GetFormatPlaneCount(ID3D12Device* device, DXGI_FORMAT format) {
	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {format};
	return SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo))) ? formatInfo.PlaneCount : 0;
}

//------------------------------------------------------------------------------------------------

struct RESOURCE_DESC : public D3D12_RESOURCE_DESC {
	RESOURCE_DESC() 		{}
	RESOURCE_DESC(const D3D12_RESOURCE_DESC &b) : D3D12_RESOURCE_DESC(b) {}
	RESOURCE_DESC(D3D12_RESOURCE_DIMENSION dimension, uint64 alignment, uint64 width, uint32 height, uint16 depth, uint16 mips, DXGI_FORMAT format, uint32 sampleCount, uint32 sampleQuality, D3D12_TEXTURE_LAYOUT layout, D3D12_RESOURCE_FLAGS flags) {
		Dimension			= dimension;
		Alignment			= alignment;
		Width				= width;
		Height				= height;
		DepthOrArraySize	= depth;
		MipLevels			= mips;
		Format				= format;
		SampleDesc.Count	= sampleCount;
		SampleDesc.Quality	= sampleQuality;
		Layout				= layout;
		Flags				= flags;
	}
	static inline RESOURCE_DESC Buffer(const D3D12_RESOURCE_ALLOCATION_INFO& alloc, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alloc.Alignment, alloc.SizeInBytes, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
	}
	static inline RESOURCE_DESC Buffer(uint64 width, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alignment, width, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
	}
	static inline RESOURCE_DESC Tex1D(DXGI_FORMAT format, uint64 width, uint16 arraySize = 1, uint16 mips = 0, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE1D, alignment, width, 1, arraySize, mips, format, 1, 0, layout, flags);
	}
	static inline RESOURCE_DESC Tex2D(DXGI_FORMAT format, uint64 width, uint32 height, uint16 arraySize = 1, uint16 mips = 0, uint32 sampleCount = 1, uint32 sampleQuality = 0, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D, alignment, width, height, arraySize, mips, format, sampleCount, sampleQuality, layout, flags);
	}
	static inline RESOURCE_DESC Tex3D(DXGI_FORMAT format, uint64 width, uint32 height, uint16 depth, uint16 mips = 0, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE3D, alignment, width, height, depth, mips, format, 1, 0, layout, flags);
	}
	constexpr	uint16	Depth()									const	{ return (Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1); }
	constexpr	uint16	ArraySize()								const	{ return (Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1); }
	inline		uint8	PlaneCount(ID3D12Device* pDevice)		const	{ return GetFormatPlaneCount(pDevice, Format); }
	inline		uint32	NumSubresources(ID3D12Device* pDevice)	const	{ return MipLevels * ArraySize() * PlaneCount(pDevice); }

	constexpr	uint32	CalcSubresource(uint32 MipSlice, uint32 ArraySlice, uint32 PlaneSlice) const { return MipSlice + MipLevels * (ArraySlice + ArraySize() * PlaneSlice);}
	constexpr	uint32	ExtractMip(uint32 sub)					const	{ return sub % MipLevels; }
	constexpr	uint32	ExtractSlice(uint32 sub)				const	{ return (sub / MipLevels) % ArraySize(); }
	constexpr	uint32	ExtractPlane(uint32 sub)				const	{ return sub / (MipLevels * ArraySize()); }
	
	constexpr operator const RESOURCE_DESC*() const { return this; }
};

struct RESOURCE_DESC1 : public RESOURCE_DESC {
	D3D12_MIP_REGION SamplerFeedbackMipRegion;
};

//------------------------------------------------------------------------------------------------

inline bool IsLayoutOpaque(D3D12_TEXTURE_LAYOUT Layout) {
	return Layout == D3D12_TEXTURE_LAYOUT_UNKNOWN || Layout == D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
}

//------------------------------------------------------------------------------------------------

struct DESCRIPTOR_RANGE : D3D12_DESCRIPTOR_RANGE {
	DESCRIPTOR_RANGE() {}
	DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32 num, uint32 baseShaderRegister, uint32 space = 0, uint32 offset = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) {
		Init(type, num, baseShaderRegister, space, offset);
	}
	inline void Init(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32 num, uint32 baseShaderRegister, uint32 space = 0, uint32 offset = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) {
		RangeType							= type;
		NumDescriptors						= num;
		BaseShaderRegister					= baseShaderRegister;
		RegisterSpace						= space;
		OffsetInDescriptorsFromTableStart	= offset;
	}
};

//------------------------------------------------------------------------------------------------

struct _ROOT_SIGNATURE_DESC : D3D12_ROOT_SIGNATURE_DESC {
	struct ROOT_PARAMETER : D3D12_ROOT_PARAMETER {
		inline void InitAsTable(uint32 num, const D3D12_DESCRIPTOR_RANGE* ranges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			ParameterType						= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			ShaderVisibility					= visibility;
			DescriptorTable.NumDescriptorRanges	= num;
			DescriptorTable.pDescriptorRanges	= ranges;
		}
		template<int N> inline void InitAsTable(const DESCRIPTOR_RANGE (&ranges)[N], D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			InitAsTable(N, ranges, visibility);
		}
		inline void InitAsConstants(uint32 num, uint32 reg, uint32 space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			ParameterType				= D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			ShaderVisibility			= visibility;
			Constants.Num32BitValues	= num;
			Constants.ShaderRegister	= reg;
			Constants.RegisterSpace		= space;
		}
		inline void InitAsView(D3D12_ROOT_PARAMETER_TYPE type, uint32 reg, uint32 space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			ParameterType				= type;
			ShaderVisibility			= visibility;
			Descriptor.ShaderRegister	= reg;
			Descriptor.RegisterSpace	= space;
		}
	};
	struct STATIC_SAMPLER_DESC : D3D12_STATIC_SAMPLER_DESC {
		inline void Init(
			uint32						reg,
			D3D12_FILTER				filter		= D3D12_FILTER_ANISOTROPIC,
			D3D12_TEXTURE_ADDRESS_MODE	addressU	= D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE	addressV	= D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE	addressW	= D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			float						mipLODBias	= 0,
			uint32						max_aniso	= 16,
			D3D12_COMPARISON_FUNC		comp		= D3D12_COMPARISON_FUNC_LESS_EQUAL,//D3D12_COMPARISON_FUNC_NEVER
			D3D12_STATIC_BORDER_COLOR	border		= D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,//D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
			float						minLOD		= 0.f,
			float						maxLOD		= D3D12_FLOAT32_MAX,
			D3D12_SHADER_VISIBILITY		visibility	= D3D12_SHADER_VISIBILITY_ALL,
			uint32						space		= 0
		) {
			ShaderRegister		= reg;
			Filter				= filter;
			AddressU			= addressU;
			AddressV			= addressV;
			AddressW			= addressW;
			MipLODBias			= mipLODBias;
			MaxAnisotropy		= max_aniso;
			ComparisonFunc		= comp;
			BorderColor			= border;
			MinLOD				= minLOD;
			MaxLOD				= maxLOD;
			ShaderVisibility	= visibility;
			RegisterSpace		= space;
		}
	};

	_ROOT_SIGNATURE_DESC(D3D12_ROOT_PARAMETER *params, int num_params, D3D12_STATIC_SAMPLER_DESC *samplers, int num_samplers, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) {
		NumParameters		= num_params;
		pParameters			= params;
		NumStaticSamplers	= num_samplers;
		pStaticSamplers		= samplers;
		Flags				= flags;
	}

	bool Create(ID3D12Device *device, ID3D12RootSignature **root_sig) {
		com_ptr<ID3DBlob> sig, err;
		if (FAILED(D3D12SerializeRootSignature(this, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
			const char *e = (const char*)err->GetBufferPointer();
			ISO_OUTPUT(e);
			return false;
		}
		return device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)root_sig) == S_OK;
	}
};

//------------------------------------------------------------------------------------------------

template<int NP, int NS> struct ROOT_SIGNATURE_DESC : _ROOT_SIGNATURE_DESC {
	ROOT_PARAMETER		params[NP];
	STATIC_SAMPLER_DESC	samplers[NS];
	ROOT_SIGNATURE_DESC(D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) : _ROOT_SIGNATURE_DESC(params, NP, samplers, NS, flags) {}
};

//------------------------------------------------------------------------------------------------

struct DescriptorHeap : com_ptr<ID3D12DescriptorHeap> {
	uint32							descriptor_size;
	D3D12_CPU_DESCRIPTOR_HANDLE		cpu_start;

	D3D12_CPU_DESCRIPTOR_HANDLE	item(uint32 i)	const	{
		return force_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(cpu_start.ptr + i * descriptor_size);
	}
	uint32	Index(D3D12_CPU_DESCRIPTOR_HANDLE h) {
		return (h.ptr - cpu_start.ptr) / descriptor_size;
	}
	bool	Contains(D3D12_CPU_DESCRIPTOR_HANDLE h, uint32 num) {
		return h.ptr >= cpu_start.ptr && h.ptr < cpu_start.ptr + num * descriptor_size;
	}
	bool	Init(ID3D12Device *device, uint32 num, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.Type			= type;
		desc.NumDescriptors	= num;
		desc.Flags			= flags;
		desc.NodeMask		= 0;
		if (device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void**)&*this) == S_OK) {
			descriptor_size	= device->GetDescriptorHandleIncrementSize(type);
			cpu_start		= get()->GetCPUDescriptorHandleForHeapStart();
			return true;
		}
		return false;
	}
	template<typename I> void	Set(ID3D12Device *device, uint32 dest, I values, int num, D3D12_DESCRIPTOR_HEAP_TYPE type) const {
		D3D12_CPU_DESCRIPTOR_HANDLE	dest_start		= item(dest);
		UINT						dest_size		= num;
		D3D12_CPU_DESCRIPTOR_HANDLE	*srce_starts	= alloc_auto(D3D12_CPU_DESCRIPTOR_HANDLE, num);
		UINT						*srce_sizes		= alloc_auto(UINT, num);
		for (int i = 0; i < num; ++i, ++values) {
			srce_starts[i]	= *values;
			srce_sizes[i]	= 1;
		}
		device->CopyDescriptors(1, &dest_start, &dest_size, num, srce_starts, srce_sizes, type);
	}
	void	Set(ID3D12Device *device, uint32 dest, D3D12_CPU_DESCRIPTOR_HANDLE value, D3D12_DESCRIPTOR_HEAP_TYPE type) const {
		device->CopyDescriptorsSimple(1, item(dest), value, type);
	}
};

} // namespace dx12

#endif //DX12_HELPERS_H
