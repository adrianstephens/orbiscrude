#ifndef DX12_HELPERS_H
#define DX12_HELPERS_H

#include "base/defs.h"
#include "base/array.h"
#include "base/list.h"
#include "base/tuple.h"
#include "com.h"
#include "dx/dxgi_helpers.h"
#include <d3d12.h>

namespace iso {
template<> inline size_t string_getter<ID3D12Object*>::len() const	{
	UINT size = 0;
	t->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, nullptr);
	return size / 2 - 1;
}

template<> template<> inline size_t string_getter<ID3D12Object*>::get<char16>(char16 *s, size_t len) const {
	UINT size = (len + 1) * 2;
	t->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, s);
	return size / 2 - 1;
}

template<> template<> inline size_t string_getter<ID3D12Object*>::get<char>(char *s, size_t len) const {
	char16	*a = alloc_auto(char16, len + 1);
	return string_copy(s, a, get(a, len));
}
}

inline iso::string_getter<ID3D12Object*> to_string(const ID3D12Object *obj) { return iso::unconst(obj); }

namespace dx12 {
using namespace iso;

const char *GetD3D12Name(ID3D12Object *obj);

template<D3D12_DESCRIPTOR_HEAP_TYPE HT, D3D12_DESCRIPTOR_RANGE_TYPE RT, D3D12_ROOT_PARAMETER_TYPE T> struct D3D12_ENUM {
	constexpr operator D3D12_DESCRIPTOR_HEAP_TYPE()		{ return D3D12_DESCRIPTOR_HEAP_TYPE(HT); }
	constexpr operator D3D12_DESCRIPTOR_RANGE_TYPE()	{ return D3D12_DESCRIPTOR_RANGE_TYPE(RT); }
	constexpr operator D3D12_ROOT_PARAMETER_TYPE()		{ return D3D12_ROOT_PARAMETER_TYPE(T); }
};

typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	D3D12_DESCRIPTOR_RANGE_TYPE_SRV,	D3D12_ROOT_PARAMETER_TYPE_SRV>	_SRV;
typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	D3D12_DESCRIPTOR_RANGE_TYPE_UAV,	D3D12_ROOT_PARAMETER_TYPE_UAV>	_UAV;
typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	D3D12_DESCRIPTOR_RANGE_TYPE_CBV,	D3D12_ROOT_PARAMETER_TYPE_CBV>	_CBV;
typedef D3D12_ENUM<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,		D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,D3D12_ROOT_PARAMETER_TYPE(-1)>	_SMP;

extern _SRV	SRV;
extern _UAV	UAV;
extern _CBV	CBV;
extern _SMP	SMP;

//-------------------------------------

struct RECT : D3D12_RECT {
	RECT() {}
	RECT(int Left, int Top, int Right, int Bottom) {
		left	= Left;
		top		= Top;
		right	= Right;
		bottom	= Bottom;
	}
	constexpr operator const D3D12_RECT*() const { return this; }
};

//-------------------------------------

struct BOX : D3D12_BOX {
	BOX() {}
	BOX(int Left, int Top, int Front, int Right, int Bottom, int Back) {
		left	= Left;
		top		= Top;
		front	= Front;
		right	= Right;
		bottom	= Bottom;
		back	= Back;
	}
	BOX(int Left, int Right)						: BOX(Left, 0, 0, Right, 1, 1) {}
	BOX(int Left, int Top, int Right, int Bottom)	: BOX(Left, Top, 0, Right, Bottom, 1) {}
	constexpr operator const D3D12_BOX*() const { return this; }
};

//-------------------------------------

struct RESOURCE_ALLOCATION_INFO : D3D12_RESOURCE_ALLOCATION_INFO {
	RESOURCE_ALLOCATION_INFO() {}
	RESOURCE_ALLOCATION_INFO(uint64 size, uint64 alignment) {
		SizeInBytes = size;
		Alignment	= alignment;
	}
};

//-------------------------------------

struct COMMAND_QUEUE_DESC : D3D12_COMMAND_QUEUE_DESC {
	COMMAND_QUEUE_DESC(D3D12_COMMAND_LIST_TYPE type, int priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_FLAGS flags = D3D12_COMMAND_QUEUE_FLAG_NONE, uint32 nodeMask = 1) {
		Type		= type;
		Priority	= priority;
		Flags		= flags;
		NodeMask	= nodeMask;
	}
	constexpr operator const D3D12_COMMAND_QUEUE_DESC*() const { return this; }
};

//-------------------------------------

struct HEAP_PROPERTIES : D3D12_HEAP_PROPERTIES {
	HEAP_PROPERTIES() {}
	HEAP_PROPERTIES(const D3D12_HEAP_PROPERTIES &props) : D3D12_HEAP_PROPERTIES(props) {}
	HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY cpu, D3D12_MEMORY_POOL pool, uint32 creationNodeMask = 1, uint32 nodeMask = 1) {
		Type					= D3D12_HEAP_TYPE_CUSTOM;
		CPUPageProperty			= cpu;
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

//-------------------------------------

struct HEAP_DESC : D3D12_HEAP_DESC {
	HEAP_DESC() {}
	HEAP_DESC(uint64 size, const D3D12_HEAP_PROPERTIES &properties, uint64 alignment = 0, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
		SizeInBytes = size;
		Properties	= properties;
		Alignment	= alignment;
		Flags		= flags;
	}
	HEAP_DESC(uint64 size, D3D12_HEAP_TYPE type, uint64 alignment = 0, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE)
		: HEAP_DESC(size, HEAP_PROPERTIES(type), alignment, flags) {}
	HEAP_DESC(uint64 size, D3D12_CPU_PAGE_PROPERTY page, D3D12_MEMORY_POOL pool, uint64 alignment = 0, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE)
		: HEAP_DESC(size, HEAP_PROPERTIES(page, pool), alignment, flags) {}
	HEAP_DESC(const D3D12_RESOURCE_ALLOCATION_INFO& info, D3D12_HEAP_PROPERTIES properties, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE)
		: HEAP_DESC(info.SizeInBytes, properties, info.Alignment, flags) {}
	HEAP_DESC(const D3D12_RESOURCE_ALLOCATION_INFO& info, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE)
		: HEAP_DESC(info.SizeInBytes,  HEAP_PROPERTIES(type), info.Alignment, flags) {}
	HEAP_DESC(const D3D12_RESOURCE_ALLOCATION_INFO& info, D3D12_CPU_PAGE_PROPERTY page, D3D12_MEMORY_POOL pool, D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE)
		: HEAP_DESC(info.SizeInBytes,  HEAP_PROPERTIES(page, pool), info.Alignment, flags) {}
	bool IsCPUAccessible() const {
		return ((const HEAP_PROPERTIES*)&Properties)->IsCPUAccessible();
	}
	constexpr operator const D3D12_HEAP_DESC*() const { return this; }
};

//-------------------------------------

struct QUERY_HEAP_DESC : D3D12_QUERY_HEAP_DESC {
	QUERY_HEAP_DESC()	{}
	QUERY_HEAP_DESC(D3D12_QUERY_HEAP_TYPE type, uint32 count, uint32 nodeMask = 1) {
		Type		= type;
		Count		= count;
		NodeMask	= nodeMask;
	}
	constexpr operator const D3D12_QUERY_HEAP_DESC*() const { return this; }
};

//-------------------------------------

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
	constexpr operator const D3D12_CLEAR_VALUE*() const { return this; }
};

//-------------------------------------

struct RANGE : D3D12_RANGE {
	RANGE() {}
	RANGE(const _none&)				{ Begin	= End = 0; }
	RANGE(size_t begin, size_t end) { Begin = begin; End = end; }
	constexpr operator const RANGE*() const { return this; }
};

//-------------------------------------

struct SHADER_BYTECODE : D3D12_SHADER_BYTECODE {
	SHADER_BYTECODE() {}
	SHADER_BYTECODE(const_memory_block code) {
		pShaderBytecode = code;
		BytecodeLength	= code.size();
	}
	SHADER_BYTECODE(ID3DBlob* pShaderBlob) {
		pShaderBytecode = pShaderBlob->GetBufferPointer();
		BytecodeLength	= pShaderBlob->GetBufferSize();
	}
};

//------------------------------------------------------------------------------------------------
// sub resources
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

struct SUBRESOURCE_FOOTPRINT : D3D12_SUBRESOURCE_FOOTPRINT {
	SUBRESOURCE_FOOTPRINT() {}
	SUBRESOURCE_FOOTPRINT(DXGI_FORMAT format, uint32 width, uint32 height, uint32 depth, uint32 rowPitch) {
		Format		= format;
		Width		= width;
		Height		= height;
		Depth		= depth;
		RowPitch	= rowPitch;
	}
	SUBRESOURCE_FOOTPRINT(const D3D12_RESOURCE_DESC& resDesc, uint32 rowPitch)
		: SUBRESOURCE_FOOTPRINT(resDesc.Format, uint32(resDesc.Width), resDesc.Height, resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? resDesc.DepthOrArraySize : 1, rowPitch) {}
};


struct PLACED_SUBRESOURCE_FOOTPRINT : D3D12_PLACED_SUBRESOURCE_FOOTPRINT {
	uint32	rows;
	uint64	row_size;
	constexpr uint64	RowPitch()		const { return Footprint.RowPitch; }
	constexpr uint64	SlicePitch()	const { return Footprint.RowPitch * rows; }
	constexpr uint64	TotalSize()		const { return SlicePitch() * Footprint.Depth; }
};


struct TEXTURE_COPY_LOCATION : D3D12_TEXTURE_COPY_LOCATION {
	TEXTURE_COPY_LOCATION() {}
//	TEXTURE_COPY_LOCATION(ID3D12Resource* res) { pResource = res; }
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
	operator const TEXTURE_COPY_LOCATION*() const { return this; }
};

//------------------------------------------------------------------------------------------------
// generic VIEW_DESC
//------------------------------------------------------------------------------------------------

struct VIEW_DESC {
	enum DIMENSION {
		UNKNOWN				= 0,
		BUFFER				= 1,
		TEXTURE1D			= 2,
		TEXTURE1DARRAY		= 3,
		TEXTURE2D			= 4,
		TEXTURE2DARRAY		= 5,
		TEXTURE2DMS			= 6,
		TEXTURE2DMSARRAY	= 7,
		TEXTURE3D			= 8,
		TEXTURECUBE			= 9,
		TEXTURECUBEARRAY	= 10,
		RAYTRACING			= 11
	};

	static DIMENSION get_dim(D3D12_SRV_DIMENSION d)	{ return DIMENSION(d); }
	static DIMENSION get_dim(D3D12_UAV_DIMENSION d)	{ return DIMENSION(d); }
	static DIMENSION get_dim(D3D12_RTV_DIMENSION d)	{ return DIMENSION(d); }
	static DIMENSION get_dim(D3D12_DSV_DIMENSION d)	{ return DIMENSION(d + 1); }

	friend D3D12_RESOURCE_DIMENSION get_dim(DIMENSION d) {
		switch (d) {
			default:
				return D3D12_RESOURCE_DIMENSION_UNKNOWN;
			case BUFFER:
			case RAYTRACING:
				return D3D12_RESOURCE_DIMENSION_BUFFER;
			case TEXTURE1D:
			case TEXTURE1DARRAY:
				return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			case TEXTURE2D:
			case TEXTURE2DARRAY:
			case TEXTURE2DMS:
			case TEXTURE2DMSARRAY:
			case TEXTURECUBE:
			case TEXTURECUBEARRAY:
				return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			case TEXTURE3D:
				return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		}
	}

	DXGI_FORMAT		format;
	uint32			component_mapping	= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	DIMENSION		dim;
	uint32			first_mip;		// or buffer stride
	uint32			num_mips;
	uint64			first_slice;	// or plane slice
	uint32			num_slices;
	float			min_lod				= 0;


	void	set(uint32 _first_mip, uint32 _num_mips, uint32 _first_slice = 0, uint32 _num_slices = 1) {
		first_mip	= _first_mip;
		num_mips	= _num_mips;
		first_slice	= _first_slice;
		num_slices	= _num_slices;
	}

	VIEW_DESC() : format(DXGI_FORMAT_UNKNOWN), dim(UNKNOWN), first_mip(0), num_mips(0), first_slice(0), num_slices(0) {}

	VIEW_DESC(const D3D12_RESOURCE_DESC &desc) {
		set(0, desc.MipLevels, 0, desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? desc.Width : desc.DepthOrArraySize);

		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
			dim		= VIEW_DESC::BUFFER;
			format	= DXGI_FORMAT_R32G32B32A32_FLOAT;

		} else {
			format = desc.Format;
			DXGI_COMPONENTS	comp(format);
			if (comp.Type() == DXGI_COMPONENTS::TYPELESS) {
				if (auto format2 = comp.Type(DXGI_COMPONENTS::UINT).GetFormat())
					format = format2;

				if (auto format2 = comp.Type(DXGI_COMPONENTS::UNORM).GetFormat())
					format = format2;
			}

			switch (desc.Dimension) {
				case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
					dim = desc.DepthOrArraySize == 1 ? VIEW_DESC::TEXTURE1D : VIEW_DESC::TEXTURE1DARRAY;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
					dim = desc.DepthOrArraySize == 1
						? (desc.SampleDesc.Count > 1 ? VIEW_DESC::TEXTURE2DMS : VIEW_DESC::TEXTURE2D)
						: (desc.SampleDesc.Count > 1 ? VIEW_DESC::TEXTURE2DMSARRAY : VIEW_DESC::TEXTURE2DARRAY);
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
					dim = VIEW_DESC::TEXTURE3D;
					break;
			}
		}
	}

	DXGI_COMPONENTS	GetComponents() const {
		DXGI_COMPONENTS	format2 = format;
		format2.chans = Rearrange((DXGI_COMPONENTS::SWIZZLE)format2.chans, (DXGI_COMPONENTS::SWIZZLE)component_mapping);
		return format2;
	}

	VIEW_DESC(const D3D12_SHADER_RESOURCE_VIEW_DESC& v) : format(v.Format), component_mapping(v.Shader4ComponentMapping), dim(get_dim(v.ViewDimension)) {
		switch (v.ViewDimension) {
			case D3D12_SRV_DIMENSION_BUFFER:			set(v.Buffer.StructureByteStride, 0, v.Buffer.FirstElement, v.Buffer.NumElements); break;
			case D3D12_SRV_DIMENSION_TEXTURE1D:			set(v.Texture1D.MostDetailedMip, v.Texture1D.MipLevels); break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:	set(v.Texture1D.MostDetailedMip, v.Texture1D.MipLevels, v.Texture1DArray.FirstArraySlice, v.Texture1DArray.ArraySize); break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:			set(v.Texture2D.MostDetailedMip, v.Texture2D.MipLevels); break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:	set(v.Texture2DArray.MostDetailedMip, v.Texture2DArray.MipLevels, v.Texture2DArray.FirstArraySlice, v.Texture2DArray.ArraySize); break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:	set(0, 1); break;
			case D3D12_SRV_DIMENSION_TEXTURE3D:			set(v.Texture3D.MostDetailedMip, v.Texture3D.MipLevels); break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:		set(v.TextureCube.MostDetailedMip, v.TextureCube.MipLevels); break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:	set(v.TextureCubeArray.MostDetailedMip, v.TextureCubeArray.MipLevels, v.TextureCubeArray.First2DArrayFace, v.TextureCubeArray.NumCubes * 6); break;
		}
	}
	VIEW_DESC(const D3D12_UNORDERED_ACCESS_VIEW_DESC& v) : format(v.Format), dim(get_dim(v.ViewDimension)) {
		switch (v.ViewDimension) {
			case D3D12_UAV_DIMENSION_BUFFER:			set(v.Buffer.StructureByteStride, 0, v.Buffer.FirstElement, v.Buffer.NumElements); break;
			case D3D12_UAV_DIMENSION_TEXTURE1D:			set(v.Texture1D.MipSlice, 1); break;
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:	set(v.Texture1DArray.MipSlice, 1, v.Texture1DArray.FirstArraySlice, v.Texture1DArray.ArraySize); break;
			case D3D12_UAV_DIMENSION_TEXTURE2D:			set(v.Texture2D.MipSlice, 1); break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:	set(v.Texture2DArray.MipSlice, 1, v.Texture2DArray.FirstArraySlice, v.Texture2DArray.ArraySize); break;
			case D3D12_UAV_DIMENSION_TEXTURE3D:			set(v.Texture3D.MipSlice, 1, v.Texture3D.FirstWSlice); break;

		}
	}
	VIEW_DESC(const D3D12_RENDER_TARGET_VIEW_DESC& v) : format(v.Format), dim(get_dim(v.ViewDimension)) {
		switch (v.ViewDimension) {
			case D3D12_RTV_DIMENSION_BUFFER:			set(0, 0, v.Buffer.FirstElement, v.Buffer.NumElements); break;
			case D3D12_RTV_DIMENSION_TEXTURE1D:			set(v.Texture1D.MipSlice, 1); break;
			case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:	set(v.Texture1DArray.MipSlice, 1, v.Texture1DArray.FirstArraySlice, v.Texture1DArray.ArraySize); break;
			case D3D12_RTV_DIMENSION_TEXTURE2D:			set(v.Texture2D.MipSlice, 1); break;
			case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:	set(v.Texture2DArray.MipSlice, 1, v.Texture2DArray.FirstArraySlice, v.Texture2DArray.ArraySize); break;
			case D3D12_RTV_DIMENSION_TEXTURE2DMS:
			case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:	set(0, 1); break;
			case D3D12_RTV_DIMENSION_TEXTURE3D:			set(v.Texture3D.MipSlice, 1, v.Texture3D.FirstWSlice); break;
		}
	}
	VIEW_DESC(const D3D12_DEPTH_STENCIL_VIEW_DESC& v) : format(v.Format), dim(get_dim(v.ViewDimension)) {
		switch (v.ViewDimension) {
			case D3D12_DSV_DIMENSION_TEXTURE1D:			set(v.Texture1D.MipSlice, 1); break;
			case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:	set(v.Texture1DArray.MipSlice, 1, v.Texture1DArray.FirstArraySlice, v.Texture1DArray.ArraySize); break;
			case D3D12_DSV_DIMENSION_TEXTURE2D:			set(v.Texture2D.MipSlice, 1); break;
			case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:	set(v.Texture2DArray.MipSlice, 1, v.Texture2DArray.FirstArraySlice, v.Texture2DArray.ArraySize); break;
			case D3D12_DSV_DIMENSION_TEXTURE2DMS:
			case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:	set(0, 1); break;
		}
	}

	operator D3D12_SHADER_RESOURCE_VIEW_DESC() const {
		D3D12_SHADER_RESOURCE_VIEW_DESC	view;
		clear(view);
		view.Shader4ComponentMapping	= component_mapping;
		view.Format						= format;
		view.ViewDimension				= D3D12_SRV_DIMENSION(dim);
		switch (dim) {
			case BUFFER:			view.Buffer			= {first_slice, num_slices, first_mip, D3D12_BUFFER_SRV_FLAG_NONE};		break;
			case TEXTURE1D:			view.Texture1D		= {first_mip, num_mips, min_lod};										break;
			case TEXTURE1DARRAY:	view.Texture1DArray	= {first_mip, num_mips, (uint32)first_slice, num_slices, min_lod};		break;
			case TEXTURE2D:			view.Texture2D		= {first_mip, num_mips, (uint32)first_slice, min_lod};					break;
			case TEXTURE2DARRAY:	view.Texture2DArray	= {first_mip, num_mips, (uint32)first_slice, num_slices, 0, min_lod};	break;
			case TEXTURE3D:			view.Texture3D		= {first_mip, num_mips, min_lod};										break;
			case TEXTURECUBE:		view.TextureCube	= {first_mip, num_mips, min_lod};										break;
			case TEXTURECUBEARRAY:	view.TextureCubeArray = {first_mip, num_mips, (uint32)first_slice, num_slices / 6, min_lod};break;
		}
		return view;
	}
	operator D3D12_UNORDERED_ACCESS_VIEW_DESC() const {
		D3D12_UNORDERED_ACCESS_VIEW_DESC	view;
		clear(view);
		view.Format			= format;
		view.ViewDimension	= D3D12_UAV_DIMENSION(dim);
		switch (dim) {
			case BUFFER:			view.Buffer			= {first_slice, num_slices, first_mip, 0, D3D12_BUFFER_UAV_FLAG_NONE};	break;
			case TEXTURE1D:			view.Texture1D		= {first_mip};															break;
			case TEXTURE1DARRAY:	view.Texture1DArray = {first_mip, (uint32)first_slice, num_slices};							break;
			case TEXTURE2D:			view.Texture2D		= {first_mip, (uint32)first_slice};										break;
			case TEXTURE2DARRAY:	view.Texture2DArray	= {first_mip, (uint32)first_slice, num_slices, 0};						break;
			case TEXTURE3D:			view.Texture3D		= {first_mip, (uint32)first_slice, num_slices};							break;
		}
		return view;
	}
	operator D3D12_RENDER_TARGET_VIEW_DESC() const {
		D3D12_RENDER_TARGET_VIEW_DESC	view;
		clear(view);
		view.Format			= format;
		view.ViewDimension	= D3D12_RTV_DIMENSION(dim);
		switch (dim) {
			case BUFFER:			view.Buffer			= {first_slice, num_slices};						break;
			case TEXTURE1D:			view.Texture1D		= {first_mip};										break;
			case TEXTURE1DARRAY:	view.Texture1DArray	= {first_mip, (uint32)first_slice, num_slices};		break;
			case TEXTURE2D:			view.Texture2D		= {first_mip, (uint32)first_slice};					break;
			case TEXTURE2DARRAY:	view.Texture2DArray	= {first_mip, (uint32)first_slice, num_slices, 0};	break;
			case TEXTURE2DMSARRAY:	view.Texture2DMSArray= {(uint32)first_slice, num_slices};				break;
			case TEXTURE3D:			view.Texture3D		= {first_mip, (uint32)first_slice, num_slices};		break;
		}
		return view;
	}
	operator D3D12_DEPTH_STENCIL_VIEW_DESC() const {
		D3D12_DEPTH_STENCIL_VIEW_DESC	view;
		clear(view);
		view.Format			= format;
		view.ViewDimension	= D3D12_DSV_DIMENSION(dim - 1);
		switch (dim) {
			case TEXTURE1D:			view.Texture1D		= {first_mip};										break;
			case TEXTURE1DARRAY:	view.Texture1DArray	= {first_mip, (uint32)first_slice, num_slices};		break;
			case TEXTURE2D:			view.Texture2D		= {first_mip};										break;
			case TEXTURE2DARRAY:	view.Texture2DArray	= {first_mip, (uint32)first_slice, num_slices};		break;
			case TEXTURE2DMSARRAY:	view.Texture2DMSArray= {(uint32)first_slice, num_slices};				break;
		}
		return view;
	}
	
	void	CreateShaderResourceView(ID3D12Device *device, ID3D12Resource *res, D3D12_CPU_DESCRIPTOR_HANDLE h) const {
		device->CreateShaderResourceView(res, addr(D3D12_SHADER_RESOURCE_VIEW_DESC(*this)), h);
	}
	void	CreateUnorderedAccessView (ID3D12Device *device, ID3D12Resource *res, D3D12_CPU_DESCRIPTOR_HANDLE h) const {
		device->CreateUnorderedAccessView (res, nullptr, addr(D3D12_UNORDERED_ACCESS_VIEW_DESC(*this)), h);
	}
	void	CreateRenderTargetView(ID3D12Device *device, ID3D12Resource *res, D3D12_CPU_DESCRIPTOR_HANDLE h) const {
		device->CreateRenderTargetView(res, addr(D3D12_RENDER_TARGET_VIEW_DESC(*this)), h);
	}
	void	CreateDepthStencilView(ID3D12Device *device, ID3D12Resource *res, D3D12_CPU_DESCRIPTOR_HANDLE h) const {
		device->CreateDepthStencilView(res, addr(D3D12_DEPTH_STENCIL_VIEW_DESC(*this)), h);
	}
	void	CreateUnorderedAccessView (ID3D12Device *device, ID3D12Resource *res, ID3D12Resource *counter, uint32 counter_offset, D3D12_CPU_DESCRIPTOR_HANDLE h) const {
		D3D12_UNORDERED_ACCESS_VIEW_DESC	uav = *this;
		uav.Buffer.CounterOffsetInBytes	= counter_offset;
		device->CreateUnorderedAccessView (res, counter, &uav, h);
	}
};

//------------------------------------------------------------------------------------------------

inline uint8 GetFormatPlaneCount(ID3D12Device* device, DXGI_FORMAT format) {
	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {format};
	return SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo))) ? formatInfo.PlaneCount : 1;
}

inline bool IsLayoutOpaque(D3D12_TEXTURE_LAYOUT Layout) {
	return Layout == D3D12_TEXTURE_LAYOUT_UNKNOWN || Layout == D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
}

struct RESOURCE_DESC : public D3D12_RESOURCE_DESC {
	RESOURCE_DESC() 		{}
	RESOURCE_DESC(ID3D12Resource *res) 			: D3D12_RESOURCE_DESC(res->GetDesc()) {}
	RESOURCE_DESC(const D3D12_RESOURCE_DESC &b) : D3D12_RESOURCE_DESC(b) {
		if (MipLevels == 0)
			MipLevels = 1;
	}
	RESOURCE_DESC(D3D12_RESOURCE_DIMENSION dimension, uint64 alignment, uint64 width, uint32 height, uint16 depth, uint16 mips, DXGI_FORMAT format, uint32 sampleCount, uint32 sampleQuality, D3D12_TEXTURE_LAYOUT layout, D3D12_RESOURCE_FLAGS flags) {
		Dimension			= dimension;
		Alignment			= alignment;
		Width				= width;
		Height				= height;
		DepthOrArraySize	= depth;
		MipLevels			= mips ? mips : 1;
		Format				= format;
		SampleDesc.Count	= sampleCount;
		SampleDesc.Quality	= sampleQuality;
		Layout				= layout;
		Flags				= flags;
	}
	
	constexpr operator const RESOURCE_DESC*() const { return this; }

	static inline RESOURCE_DESC Buffer(const D3D12_RESOURCE_ALLOCATION_INFO& alloc, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alloc.Alignment, alloc.SizeInBytes, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
	}
	static inline RESOURCE_DESC Buffer(uint64 width, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alignment, width, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
	}
	static inline RESOURCE_DESC Tex1D(DXGI_FORMAT format, uint64 width, uint16 arraySize = 1, uint16 mips = 0, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE1D, alignment, width, 1, arraySize, mips, format, 1, 0, layout, flags);
	}
	static inline RESOURCE_DESC Tex2D(DXGI_FORMAT format, uint64 width, uint32 height, uint16 arraySize = 1, uint16 mips = 0, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D, alignment, width, height, arraySize, mips, format, 1, 0, layout, flags);
	}
	static inline RESOURCE_DESC Tex3D(DXGI_FORMAT format, uint64 width, uint32 height, uint16 depth, uint16 mips = 0, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, uint64 alignment = 0) {
		return RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE3D, alignment, width, height, depth, mips, format, 1, 0, layout, flags);
	}
	auto&		SetFlags(D3D12_RESOURCE_FLAGS flags)			{ Flags |= flags; return *this; }
	auto&		SetSample(uint32 count = 1, uint32 quality = 0)	{ SampleDesc = {count, quality}; return *this; }

	constexpr	uint16	Depth()									const	{ return (Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1); }
	constexpr	uint16	ArraySize()								const	{ return (Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1); }
	inline		uint8	PlaneCount(ID3D12Device* device)		const	{ return GetFormatPlaneCount(device, Format); }
	inline		uint32	NumSubresources(ID3D12Device* device)	const	{ return MipLevels * ArraySize() * PlaneCount(device); }
	inline		uint32	NumSubresources()						const	{ return MipLevels * ArraySize() * NumPlanes(Format); }
	inline		uint32	NumSubresources(nullptr_t)				const	{ return MipLevels * ArraySize() * NumPlanes(Format); }

	constexpr	uint32	CalcSubresource(uint32 mip, uint32 array, uint32 plane) const { return mip + MipLevels * (array + ArraySize() * plane);}
	constexpr	uint32	CalcSubresource(const VIEW_DESC &view)	const	{ return CalcSubresource(view.first_mip, view.first_slice, 0); }
	constexpr	uint32	ExtractMip(uint32 sub)					const	{ return sub % MipLevels; }
	constexpr	uint32	ExtractSlice(uint32 sub)				const	{ return (sub / MipLevels) % ArraySize(); }
	constexpr	uint32	ExtractPlane(uint32 sub)				const	{ return sub / (MipLevels * ArraySize()); }

	PLACED_SUBRESOURCE_FOOTPRINT	SubFootprint(ID3D12Device* device, uint32 sub) const {
		PLACED_SUBRESOURCE_FOOTPRINT	footprint;
		device->GetCopyableFootprints(this, sub, 1, 0, &footprint, &footprint.rows, &footprint.row_size, 0);
		return footprint;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT	SubFootprint(ID3D12Device* device, uint32 sub, uint32 &rows, uint64 &row_size) const {
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT	footprint;
		device->GetCopyableFootprints(this, sub, 1, 0, &footprint, &rows, &row_size, 0);
		return footprint;
	}
	uint64	TotalSize(ID3D12Device* device) const {
		uint64	size;
		device->GetCopyableFootprints(this, 0, NumSubresources(device), 0, nullptr, nullptr, nullptr, &size);
		return size;
	}

	D3D12_MIP_REGION	GetMipRegion(uint32 mip) const {
		return {
			max((UINT)Width >> mip, 1),
			max(Height >> mip, 1),
			Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? max((UINT)DepthOrArraySize >> mip, 1) : 1
		};
	}

	DXGI_FORMAT			ViewFormat() const {
		if (Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			return DXGI_FORMAT_R32G32B32A32_FLOAT;

		DXGI_COMPONENTS	comp(Format);
		if (comp.Type() == DXGI_COMPONENTS::TYPELESS) {
			if (auto format2 = comp.Type(DXGI_COMPONENTS::UINT).GetFormat())
				return format2;

			if (auto format2 = comp.Type(DXGI_COMPONENTS::UNORM).GetFormat())
				return format2;
		}
		return Format;
	}

	bool validate_state(uint32 sub, D3D12_RESOURCE_STATES state) const {
		if (sub != 0 && sub != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && sub >= NumSubresources())
			return false;

		if (state & (D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER)) {
			if (Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
				return false;
		}

		uint32	needed =
			(state & D3D12_RESOURCE_STATE_RENDER_TARGET																? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET		: 0)
		|	(state & (D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ)							? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL		: 0)
		|	(state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS															? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS	: 0)
		|	(state & (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)	? D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE		: 0)
		;
		return ((Flags ^ D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) & needed) == needed;
	}

	bool validate_view(DXGI_FORMAT v) const {
		return v == Format || DXGI_COMPONENTS(Format).layout == DXGI_COMPONENTS(v).layout;
	}
	bool validate_view(const VIEW_DESC& v) const {
		return validate_view(v.format)
			&& get_dim(v.dim) == Dimension
			&& v.first_mip + v.num_mips <= MipLevels
			&& v.first_slice + v.num_slices <= DepthOrArraySize;
	}

	bool validate(const D3D12_SHADER_RESOURCE_VIEW_DESC& v) const {
		return validate_view(v) && !(Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
	}
	bool validate(const D3D12_UNORDERED_ACCESS_VIEW_DESC& v) const {
		return validate_view(v) && (Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}
	bool validate(const D3D12_RENDER_TARGET_VIEW_DESC& v) const {
		return validate_view(v) && (Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	}
	bool validate(const D3D12_DEPTH_STENCIL_VIEW_DESC& v) const {
		return validate_view(v) && (Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	}

#if 1
	explicit operator D3D12_SHADER_RESOURCE_VIEW_DESC() const {
		ISO_ASSERT(!(Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));
		return VIEW_DESC(*this);
	}
	explicit operator D3D12_UNORDERED_ACCESS_VIEW_DESC() const {
		ISO_ASSERT(Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		return VIEW_DESC(*this);
	}
	explicit operator D3D12_RENDER_TARGET_VIEW_DESC() const {
		ISO_ASSERT(Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		return VIEW_DESC(*this);
	}
	explicit operator D3D12_DEPTH_STENCIL_VIEW_DESC() const {
		ISO_ASSERT(Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		return VIEW_DESC(*this);
	}
#else
	explicit operator D3D12_SHADER_RESOURCE_VIEW_DESC() const {
		ISO_ASSERT(!(Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));
		D3D12_SHADER_RESOURCE_VIEW_DESC	view;
		clear(view);
		view.Shader4ComponentMapping	= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		view.Format						= ViewFormat();

		switch (Dimension) {
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				view.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
				view.Buffer.NumElements				= Width / 16;
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE1D;
					view.Texture1D.MipLevels		= MipLevels;
				} else {
					view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
					view.Texture1DArray.MipLevels	= MipLevels;
					view.Texture1DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2D;
					view.Texture2D.MipLevels		= MipLevels;
				} else {
					view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					view.Texture2DArray.MipLevels	= MipLevels;
					view.Texture2DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				view.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE3D;
				view.Texture3D.MipLevels			= MipLevels;
				break;
		}
		return view;
	}
	explicit operator D3D12_UNORDERED_ACCESS_VIEW_DESC() const {
		ISO_ASSERT(Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		D3D12_UNORDERED_ACCESS_VIEW_DESC	view;
		clear(view);
		view.Format	= ViewFormat();

		switch (Dimension) {
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				view.ViewDimension					= D3D12_UAV_DIMENSION_BUFFER;
				view.Buffer.NumElements				= Width / 16;
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= D3D12_UAV_DIMENSION_TEXTURE1D;
				} else {
					view.ViewDimension				= D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
					view.Texture1DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= D3D12_UAV_DIMENSION_TEXTURE2D;
				} else {
					view.ViewDimension				= D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					view.Texture2DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				view.ViewDimension					= D3D12_UAV_DIMENSION_TEXTURE3D;
				view.Texture3D.WSize				= DepthOrArraySize;
				break;
		}
		return view;
	}
	explicit operator D3D12_RENDER_TARGET_VIEW_DESC() const {
		ISO_ASSERT(Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		D3D12_RENDER_TARGET_VIEW_DESC	view;
		clear(view);
		view.Format	= ViewFormat();

		switch (Dimension) {
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				view.ViewDimension					= D3D12_RTV_DIMENSION_BUFFER;
				view.Buffer.NumElements				= Width / 16;
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= D3D12_RTV_DIMENSION_TEXTURE1D;
				} else {
					view.ViewDimension				= D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
					view.Texture1DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
				} else if (SampleDesc.Count > 1) {
					view.ViewDimension				= D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
					view.Texture2DMSArray.ArraySize	= DepthOrArraySize;
				} else {
					view.ViewDimension				= D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					view.Texture2DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				view.ViewDimension					= D3D12_RTV_DIMENSION_TEXTURE3D;
				view.Texture3D.WSize				= DepthOrArraySize;
				break;
		}
		return view;
	}
	explicit operator D3D12_DEPTH_STENCIL_VIEW_DESC() const {
		ISO_ASSERT(Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_DEPTH_STENCIL_VIEW_DESC	view;
		clear(view);
		view.Format	= ViewFormat();

		switch (Dimension) {
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= D3D12_DSV_DIMENSION_TEXTURE1D;
				} else {
					view.ViewDimension				= D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
					view.Texture1DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				if (DepthOrArraySize == 1) {
					view.ViewDimension				= SampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
				} else if (SampleDesc.Count > 1) {
					view.ViewDimension				= D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
					view.Texture2DMSArray.ArraySize	= DepthOrArraySize;
				} else {
					view.ViewDimension				= D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
					view.Texture2DArray.ArraySize	= DepthOrArraySize;
				}
				break;
			default:
				ISO_ASSERT(0);
		}
		return view;
	}
#endif
};

struct RESOURCE_DESC1 : public RESOURCE_DESC {
	D3D12_MIP_REGION SamplerFeedbackMipRegion;
	RESOURCE_DESC1(const RESOURCE_DESC &desc) : RESOURCE_DESC(desc), SamplerFeedbackMipRegion(GetMipRegion(0)) {}
	constexpr operator D3D12_RESOURCE_DESC1*() const { return (D3D12_RESOURCE_DESC1*)this; }
};

//------------------------------------------------------------------------------------------------
// tiling
//------------------------------------------------------------------------------------------------

struct TILED_RESOURCE_COORDINATE : D3D12_TILED_RESOURCE_COORDINATE {
	TILED_RESOURCE_COORDINATE() {}
	TILED_RESOURCE_COORDINATE(uint32 x, uint32 y, uint32 z, uint32 sub) {
		X			= x;
		Y			= y;
		Z			= z;
		Subresource	= sub;
	}
};

struct TILE_REGION_SIZE : D3D12_TILE_REGION_SIZE {
	TILE_REGION_SIZE() {}
	TILE_REGION_SIZE(uint32 numTiles) {
		NumTiles	= numTiles;
		UseBox		= false;
	}
	TILE_REGION_SIZE(uint32 width, uint16 height, uint16 depth) {
		NumTiles	= width * height * depth;
		UseBox		= true;
		Width		= width;
		Height		= height;
		Depth		= depth;
	}
};

struct TILING_INFO {
	UINT						num_tilings;
	UINT						total_tiles;
	D3D12_PACKED_MIP_INFO		mip_packing;
	D3D12_TILE_SHAPE			tile_shape;
	D3D12_SUBRESOURCE_TILING	packed_tiles;

	TILING_INFO(ID3D12Device *device, ID3D12Resource *res) {
		RESOURCE_DESC	desc(res);
		num_tilings	= desc.NumSubresources();
		device->GetResourceTiling(res, &total_tiles, &mip_packing, &tile_shape, &num_tilings, 0, &packed_tiles);
	}
};

//------------------------------------------------------------------------------------------------
// root signature
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

struct _ROOT_SIGNATURE_DESC : D3D12_ROOT_SIGNATURE_DESC {
	struct ROOT_PARAMETER : D3D12_ROOT_PARAMETER {
		inline void Table(uint32 num, const D3D12_DESCRIPTOR_RANGE* ranges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			ParameterType						= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			ShaderVisibility					= visibility;
			DescriptorTable.NumDescriptorRanges	= num;
			DescriptorTable.pDescriptorRanges	= ranges;
		}
		template<int N> inline void Table(const DESCRIPTOR_RANGE (&ranges)[N], D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			Table(N, ranges, visibility);
		}
		inline void Constants(uint32 num, uint32 reg, uint32 space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			ParameterType				= D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			ShaderVisibility			= visibility;
			D3D12_ROOT_PARAMETER::Constants.Num32BitValues	= num;
			D3D12_ROOT_PARAMETER::Constants.ShaderRegister	= reg;
			D3D12_ROOT_PARAMETER::Constants.RegisterSpace	= space;
		}
		inline void View(D3D12_ROOT_PARAMETER_TYPE type, uint32 reg, uint32 space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
			ParameterType				= type;
			ShaderVisibility			= visibility;
			Descriptor.ShaderRegister	= reg;
			Descriptor.RegisterSpace	= space;
		}
	};
	struct STATIC_SAMPLER_DESC : D3D12_STATIC_SAMPLER_DESC {
		void Init(
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
		return device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), COM_CREATE(root_sig)) == S_OK;
	}
	ID3D12RootSignature *Create(ID3D12Device *device) {
		com_ptr<ID3DBlob> sig, err;
		if (FAILED(D3D12SerializeRootSignature(this, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
			const char *e = (const char*)err->GetBufferPointer();
			ISO_OUTPUT(e);
			return nullptr;
		}
		ID3D12RootSignature	*root_sig;
		return SUCCEEDED(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), COM_CREATE(&root_sig))) ? root_sig : nullptr;
	}
};

template<int NP, int NS> struct ROOT_SIGNATURE_DESC : _ROOT_SIGNATURE_DESC {
	ROOT_PARAMETER		params[NP];
	STATIC_SAMPLER_DESC	samplers[NS];
	ROOT_SIGNATURE_DESC(D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) : _ROOT_SIGNATURE_DESC(params, NP, samplers, NS, flags) {}
};

//------------------------------------------------------------------------------------------------
// descriptors
//------------------------------------------------------------------------------------------------

struct DescriptorHeap : com_ptr<ID3D12DescriptorHeap> {
	uint32							stride;
	D3D12_CPU_DESCRIPTOR_HANDLE		cpu_start;

	DescriptorHeap() {}
	DescriptorHeap(ID3D12Device *device, uint32 num, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {
		ISO_VERIFY(init(device, num, type, flags));
	}
	bool	init(ID3D12Device *device, uint32 num, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {type, num, flags, 0};
		if (device->CreateDescriptorHeap(&desc, COM_CREATE(get_addr())) == S_OK) {
			stride		= device->GetDescriptorHandleIncrementSize(type);
			cpu_start	= get()->GetCPUDescriptorHandleForHeapStart();
			return true;
		}
		return false;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE	cpu(uint32 i)						const	{ return {cpu_start.ptr + i * stride}; }
	uint32	index_of(D3D12_CPU_DESCRIPTOR_HANDLE h)					const	{ return (h.ptr - cpu_start.ptr) / stride; }
	bool	contains(D3D12_CPU_DESCRIPTOR_HANDLE h, uint32 num)		const	{ return h.ptr >= cpu_start.ptr && h.ptr < cpu_start.ptr + num * stride && (h.ptr - cpu_start.ptr) % stride == 0; }

	template<typename I> void	Set(ID3D12Device *device, uint32 dest, I values, int num, D3D12_DESCRIPTOR_HEAP_TYPE type) const {
		D3D12_CPU_DESCRIPTOR_HANDLE	dest_start		= item(dest);
		UINT						dest_size		= num;
		D3D12_CPU_DESCRIPTOR_HANDLE	*srce_starts	= alloc_auto(D3D12_CPU_DESCRIPTOR_HANDLE, num);
		for (int i = 0; i < num; ++i, ++values)
			srce_starts[i]	= *values;
		device->CopyDescriptors(1, &dest_start, &dest_size, num, srce_starts, nullptr, type);
	}
	void	Set(ID3D12Device *device, uint32 dest, D3D12_CPU_DESCRIPTOR_HANDLE value, D3D12_DESCRIPTOR_HEAP_TYPE type) const {
		device->CopyDescriptorsSimple(1, cpu(dest), value, type);
	}

};

struct DescriptorHeapGPU : DescriptorHeap {
	D3D12_GPU_DESCRIPTOR_HANDLE		gpu_start;

	DescriptorHeapGPU() {}
	DescriptorHeapGPU(ID3D12Device *device, uint32 num, D3D12_DESCRIPTOR_HEAP_TYPE type, bool vis = true) {
		ISO_VERIFY(init(device, num, type, vis));
	}
	bool	init(ID3D12Device *device, uint32 num, D3D12_DESCRIPTOR_HEAP_TYPE type, bool vis = true) {
		if (DescriptorHeap::init(device, num, type, vis ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE)) {
			if (vis)
				gpu_start	= get()->GetGPUDescriptorHandleForHeapStart();
			return true;
		}
		return false;
	}

	using DescriptorHeap::index_of;
	using DescriptorHeap::contains;
	D3D12_GPU_DESCRIPTOR_HANDLE	gpu(uint32 i)							const	{ return {gpu_start.ptr + i * stride}; }
	uint32	index_of(D3D12_GPU_DESCRIPTOR_HANDLE h)						const	{ return (h.ptr - gpu_start.ptr) / stride; }
	bool	contains(D3D12_GPU_DESCRIPTOR_HANDLE h, uint32 num)			const	{ return h.ptr >= gpu_start.ptr && h.ptr < gpu_start.ptr + num * stride && (h.ptr - gpu_start.ptr) % stride == 0; }

	D3D12_CPU_DESCRIPTOR_HANDLE	to_cpu(D3D12_GPU_DESCRIPTOR_HANDLE h)	const	{ return {h.ptr + (cpu_start.ptr - gpu_start.ptr)}; }
	D3D12_GPU_DESCRIPTOR_HANDLE	to_gpu(D3D12_CPU_DESCRIPTOR_HANDLE h)	const	{ return {h.ptr + (gpu_start.ptr - cpu_start.ptr)}; }
};

//------------------------------------------------------------------------------------------------
// Barriers
//------------------------------------------------------------------------------------------------

static constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_DEPTH
	= D3D12_RESOURCE_STATE_DEPTH_READ
	| D3D12_RESOURCE_STATE_DEPTH_WRITE;
static constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_WRITE
	= D3D12_RESOURCE_STATE_RENDER_TARGET
	| D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	| D3D12_RESOURCE_STATE_DEPTH_WRITE
	| D3D12_RESOURCE_STATE_STREAM_OUT
	| D3D12_RESOURCE_STATE_COPY_DEST
	| D3D12_RESOURCE_STATE_RESOLVE_DEST
	| D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE
	| D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE
	| D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE;

struct RESOURCE_BARRIER : D3D12_RESOURCE_BARRIER {
	RESOURCE_BARRIER() {}
	static inline RESOURCE_BARRIER Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 sub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) {
		RESOURCE_BARRIER result;
		result.Type		= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		result.Flags	= flags;
		result.D3D12_RESOURCE_BARRIER::Transition.pResource		= res;
		result.D3D12_RESOURCE_BARRIER::Transition.StateBefore	= before;
		result.D3D12_RESOURCE_BARRIER::Transition.StateAfter	= after;
		result.D3D12_RESOURCE_BARRIER::Transition.Subresource	= sub;
		return result;
	}
	static inline RESOURCE_BARRIER Aliasing(ID3D12Resource* before, ID3D12Resource* after) {
		RESOURCE_BARRIER result;
		result.Type		= D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
		result.D3D12_RESOURCE_BARRIER::Aliasing.pResourceBefore = before;
		result.D3D12_RESOURCE_BARRIER::Aliasing.pResourceAfter	= after;
		return result;
	}
	static inline RESOURCE_BARRIER UAV(ID3D12Resource* res) {
		RESOURCE_BARRIER result;
		result.Type		= D3D12_RESOURCE_BARRIER_TYPE_UAV;
		result.D3D12_RESOURCE_BARRIER::UAV.pResource	= res;
		return result;
	}
	operator const D3D12_RESOURCE_BARRIER*() const { return this; }
};

//inline void Transition(ID3D12GraphicsCommandList *list, ID3D12Resource *res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 sub) {
//	if (from != to)
//		list->ResourceBarrier(1, RESOURCE_BARRIER::Transition(res, from, to, sub));
//}

template<int N> struct Barriers : static_array<D3D12_RESOURCE_BARRIER, N> {
	bool Transition(ID3D12Resource *res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 sub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
		if (before == after)
			return false;
		emplace_back(RESOURCE_BARRIER::Transition(res, before, after, sub));
		return full();
	}
	bool Aliasing(ID3D12Resource* before, ID3D12Resource* after) {
		emplace_back(RESOURCE_BARRIER::Aliasing(before, after));
		return full();
	}
	bool UAV(ID3D12Resource *res) {
		emplace_back(RESOURCE_BARRIER::UAV(res));
		return full();
	}
#if 0
	void Transition(ID3D12GraphicsCommandList *list, ID3D12Resource *res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 sub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
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
			Flush(list);
		
		if (before != after) {
			if (!(before & reads) || !(after & before))
				Transition(res, before, after, sub);
		} else if (before == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
			UAVBarrier(res);
		}
	}
#else
	void Transition(ID3D12GraphicsCommandList *list, ID3D12Resource *res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 sub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
		if (Transition(res, before, after, sub))
			Flush(list);
	}
#endif
	void Flush(ID3D12GraphicsCommandList *list) {
		if (uint32 n = size()) {
			list->ResourceBarrier(n, *this);
			clear();
		}
	}
};

//------------------------------------------------------------------------------------------------
// Pipeline
//------------------------------------------------------------------------------------------------

using STREAM_OUTPUT_DESC = D3D12_STREAM_OUTPUT_DESC;

struct RENDER_TARGET_BLEND_DESC : D3D12_RENDER_TARGET_BLEND_DESC {
	RENDER_TARGET_BLEND_DESC(D3D12_RENDER_TARGET_BLEND_DESC &d) : D3D12_RENDER_TARGET_BLEND_DESC(d) {}
	RENDER_TARGET_BLEND_DESC(D3D12_LOGIC_OP op, uint8 mask = 0xff) {
		clear(*this);
		LogicOpEnable	= true;
		LogicOp			= op;
		RenderTargetWriteMask	= mask;
	}
	RENDER_TARGET_BLEND_DESC(D3D12_BLEND_OP op, D3D12_BLEND s, D3D12_BLEND d, uint8 mask = 0xff) {
		BlendEnable		= true;
		LogicOpEnable	= false;
		SrcBlend		= s;
		DestBlend		= d;
		BlendOp			= op;
		SrcBlendAlpha	= s;
		DestBlendAlpha	= d;
		BlendOpAlpha	= op;
		RenderTargetWriteMask	= mask;
	}
	RENDER_TARGET_BLEND_DESC(D3D12_BLEND_OP op, D3D12_BLEND s, D3D12_BLEND d, D3D12_BLEND_OP opa, D3D12_BLEND sa, D3D12_BLEND da, uint8 mask = 0xff) {
		BlendEnable		= true;
		LogicOpEnable	= false;
		SrcBlend		= s;
		DestBlend		= d;
		BlendOp			= op;
		SrcBlendAlpha	= sa;
		DestBlendAlpha	= da;
		BlendOpAlpha	= opa;
		RenderTargetWriteMask	= mask;
	}
};


struct BLEND_DESC : D3D12_BLEND_DESC {
	BLEND_DESC(const D3D12_BLEND_DESC &d) : D3D12_BLEND_DESC(d) {}
	BLEND_DESC(bool alpha_coverage, RENDER_TARGET_BLEND_DESC desc) {
		clear(RenderTarget);
		AlphaToCoverageEnable	= alpha_coverage;
		IndependentBlendEnable	= false;
		RenderTarget[0]			= desc;
	}
};

struct DEPTH_STENCILOP_DESC : D3D12_DEPTH_STENCILOP_DESC {
	DEPTH_STENCILOP_DESC(D3D12_COMPARISON_FUNC fn = D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STENCIL_OP fail = D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP depth_fail = D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP pass = D3D12_STENCIL_OP_KEEP) {
		StencilFailOp		= fail;
		StencilDepthFailOp	= depth_fail;
		StencilPassOp		= pass;
		StencilFunc			= fn;
	}
};

struct DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC {
	DEPTH_STENCIL_DESC(const D3D12_DEPTH_STENCIL_DESC &d) : D3D12_DEPTH_STENCIL_DESC(d) {}
	DEPTH_STENCIL_DESC(D3D12_COMPARISON_FUNC fn, bool write = true) {
		DepthEnable			= true;
		DepthWriteMask		= (D3D12_DEPTH_WRITE_MASK)write;
		DepthFunc			= fn;
		StencilEnable		= FALSE;
		StencilReadMask		= 0xff;
		StencilWriteMask	= 0xff;
		FrontFace			= BackFace = DEPTH_STENCILOP_DESC();
	}
	DEPTH_STENCIL_DESC(D3D12_COMPARISON_FUNC fn, bool write, DEPTH_STENCILOP_DESC front, DEPTH_STENCILOP_DESC back = {}, uint8 stencil_read = 0xff, uint8 stencil_write = 0xff) {
		DepthEnable			= true;
		DepthWriteMask		= (D3D12_DEPTH_WRITE_MASK)write;
		DepthFunc			= fn;
		StencilEnable		= true;
		StencilReadMask		= stencil_read;
		StencilWriteMask	= stencil_write;
		FrontFace			= front;
		BackFace			= back;
	}
	DEPTH_STENCIL_DESC(DEPTH_STENCILOP_DESC front, DEPTH_STENCILOP_DESC back = {}, uint8 stencil_read = 0xff, uint8 stencil_write = 0xff) {
		DepthEnable			= false;
		DepthWriteMask		= D3D12_DEPTH_WRITE_MASK_ALL;
		DepthFunc			= D3D12_COMPARISON_FUNC_NEVER;
		StencilEnable		= true;
		StencilReadMask		= stencil_read;
		StencilWriteMask	= stencil_write;
		FrontFace			= front;
		BackFace			= back;
	}
};

struct DEPTH_STENCIL_DESC1 : D3D12_DEPTH_STENCIL_DESC1 {
	DEPTH_STENCIL_DESC1(D3D12_COMPARISON_FUNC fn, bool write = true, bool bounds = false) {
		DepthEnable			= true;
		DepthWriteMask		= (D3D12_DEPTH_WRITE_MASK)write;
		DepthFunc			= fn;
		StencilEnable		= FALSE;
		StencilReadMask		= 0xff;
		StencilWriteMask	= 0xff;
		FrontFace			= BackFace = DEPTH_STENCILOP_DESC();
		DepthBoundsTestEnable	= bounds;
	}
	DEPTH_STENCIL_DESC1(D3D12_COMPARISON_FUNC fn, bool write, bool bounds, DEPTH_STENCILOP_DESC front, DEPTH_STENCILOP_DESC back = {}, uint8 stencil_read = 0xff, uint8 stencil_write = 0xff) {
		DepthEnable			= true;
		DepthWriteMask		= (D3D12_DEPTH_WRITE_MASK)write;
		DepthFunc			= fn;
		StencilEnable		= true;
		StencilReadMask		= stencil_read;
		StencilWriteMask	= stencil_write;
		FrontFace			= front;
		BackFace			= back;
		DepthBoundsTestEnable	= bounds;
	}
	DEPTH_STENCIL_DESC1(DEPTH_STENCILOP_DESC front, DEPTH_STENCILOP_DESC back = {}, uint8 stencil_read = 0xff, uint8 stencil_write = 0xff) {
		DepthEnable			= false;
		DepthWriteMask		= D3D12_DEPTH_WRITE_MASK_ALL;
		DepthFunc			= D3D12_COMPARISON_FUNC_NEVER;
		StencilEnable		= true;
		StencilReadMask		= stencil_read;
		StencilWriteMask	= stencil_write;
		FrontFace			= front;
		BackFace			= back;
		DepthBoundsTestEnable	= false;
	}
};


struct RASTERIZER_DESC : D3D12_RASTERIZER_DESC {
	RASTERIZER_DESC(const D3D12_RASTERIZER_DESC &d) : D3D12_RASTERIZER_DESC(d) {}
	RASTERIZER_DESC(
		D3D12_FILL_MODE fill			= D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE cull			= D3D12_CULL_MODE_BACK,
		bool	frontCounterClockwise	= false,
		int		depthBias				= D3D12_DEFAULT_DEPTH_BIAS,
		float	depthBiasClamp			= D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		float	slopeScaledDepthBias	= D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		bool	depthClipEnable			= true,
		bool	multisampleEnable		= false,
		bool	antialiasedLineEnable	= false,
		uint32	forcedSampleCount		= 0,
		D3D12_CONSERVATIVE_RASTERIZATION_MODE conservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	) {
		FillMode				= fill;
		CullMode				= cull;
		FrontCounterClockwise	= frontCounterClockwise;
		DepthBias				= depthBias;
		DepthBiasClamp			= depthBiasClamp;
		SlopeScaledDepthBias	= slopeScaledDepthBias;
		DepthClipEnable			= depthClipEnable;
		MultisampleEnable		= multisampleEnable;
		AntialiasedLineEnable	= antialiasedLineEnable;
		ForcedSampleCount		= forcedSampleCount;
		ConservativeRaster		= conservativeRaster;
	}
};

struct INPUT_LAYOUT_DESC : D3D12_INPUT_LAYOUT_DESC {
	INPUT_LAYOUT_DESC(const D3D12_INPUT_LAYOUT_DESC &d) : D3D12_INPUT_LAYOUT_DESC(d) {}
	INPUT_LAYOUT_DESC(const D3D12_INPUT_ELEMENT_DESC* p, uint32 n) {
		pInputElementDescs	= p;
		NumElements			= n;
	}
};

struct RT_FORMATS : D3D12_RT_FORMAT_ARRAY {
	RT_FORMATS(const D3D12_RT_FORMAT_ARRAY &a) : D3D12_RT_FORMAT_ARRAY(a) {}
	RT_FORMATS(initializer_list<DXGI_FORMAT> formats) {
		clear(*this);
		NumRenderTargets = formats.size();
		memcpy(RTFormats, formats.begin(), sizeof(DXGI_FORMAT) * NumRenderTargets);
	}
};

struct VIEW_INSTANCING_DESC : D3D12_VIEW_INSTANCING_DESC {
	VIEW_INSTANCING_DESC(initializer_list<D3D12_VIEW_INSTANCE_LOCATION> locs, D3D12_VIEW_INSTANCING_FLAGS flags = D3D12_VIEW_INSTANCING_FLAG_NONE) {
		ViewInstanceCount = locs.size();
		pViewInstanceLocations = locs.begin();
		Flags = flags;
	}
};

struct PIPELINE {
	static constexpr auto ROOT_SIGNATURE		= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
	static constexpr auto VS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
	static constexpr auto PS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
	static constexpr auto DS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS;
	static constexpr auto HS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS;
	static constexpr auto GS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS;
	static constexpr auto CS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
	static constexpr auto STREAM_OUTPUT			= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT;
	static constexpr auto BLEND					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND;
	static constexpr auto SAMPLE_MASK			= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK;
	static constexpr auto RASTERIZER			= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER;
	static constexpr auto DEPTH_STENCIL			= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
	static constexpr auto INPUT_LAYOUT			= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT;
	static constexpr auto IB_STRIP_CUT_VALUE	= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE;
	static constexpr auto PRIMITIVE_TOPOLOGY	= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY;
	static constexpr auto RENDER_TARGET_FORMATS	= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
	static constexpr auto DEPTH_STENCIL_FORMAT	= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
	static constexpr auto SAMPLE_DESC			= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC;
	static constexpr auto NODE_MASK				= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK;
	static constexpr auto CACHED_PSO			= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO;
	static constexpr auto FLAGS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS;
	static constexpr auto DEPTH_STENCIL1		= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1;
	static constexpr auto VIEW_INSTANCING		= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING;
	static constexpr auto AS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
	static constexpr auto MS					= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;

	using types = type_list<
		ID3D12RootSignature*,				//0		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE
		SHADER_BYTECODE,					//1		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS
		SHADER_BYTECODE,					//2		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS
		SHADER_BYTECODE,					//3		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS
		SHADER_BYTECODE,					//4		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS
		SHADER_BYTECODE,					//5		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS
		SHADER_BYTECODE,					//6		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS
		STREAM_OUTPUT_DESC,					//7		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT
		BLEND_DESC,							//8		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND
		UINT,								//9		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK
		RASTERIZER_DESC,					//10	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER
		DEPTH_STENCIL_DESC,					//11	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL
		INPUT_LAYOUT_DESC,					//12	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE,	//13	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE
		D3D12_PRIMITIVE_TOPOLOGY_TYPE,		//14	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY
		RT_FORMATS,							//15	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS
		DXGI_FORMAT,						//16	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT
		DXGI_SAMPLE_DESC,					//17	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC
		UINT,								//18	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK
		D3D12_CACHED_PIPELINE_STATE,		//19	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO
		D3D12_PIPELINE_STATE_FLAGS,			//20	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS
		DEPTH_STENCIL_DESC1,				//21	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1
		VIEW_INSTANCING_DESC,				//22	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING
		void,								//23	unused
		SHADER_BYTECODE,					//24	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS
		SHADER_BYTECODE						//25	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS
	>;

	template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE I> struct alignas(void*) SUB {
		typedef meta::TL_index_t<I, types> T;
		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE	i = I;
		T		t;
		template<typename...P> SUB(const P&...p)	: t(p...) {}
	};

	using SUBOBJECT = union_first<
		selection<1, 
		tuple<int, ID3D12RootSignature*>,				//0		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE
		tuple<int, D3D12_SHADER_BYTECODE>,				//1		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS
		tuple<int, D3D12_SHADER_BYTECODE>,				//2		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS
		tuple<int, D3D12_SHADER_BYTECODE>,				//3		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS
		tuple<int, D3D12_SHADER_BYTECODE>,				//4		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS
		tuple<int, D3D12_SHADER_BYTECODE>,				//5		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS
		tuple<int, D3D12_SHADER_BYTECODE>,				//6		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS
		tuple<int, D3D12_STREAM_OUTPUT_DESC>,			//7		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT
		tuple<int, D3D12_BLEND_DESC>,					//8		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND
		tuple<int, UINT>,								//9		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK
		tuple<int, D3D12_RASTERIZER_DESC>,				//10	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER
		tuple<int, D3D12_DEPTH_STENCIL_DESC>,			//11	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL
		tuple<int, D3D12_INPUT_LAYOUT_DESC>,			//12	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT
		tuple<int, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>,	//13	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE
		tuple<int, D3D12_PRIMITIVE_TOPOLOGY_TYPE>,		//14	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY
		tuple<int, D3D12_RT_FORMAT_ARRAY>,				//15	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS
		tuple<int, DXGI_FORMAT>,						//16	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT
		tuple<int, DXGI_SAMPLE_DESC>,					//17	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC
		tuple<int, UINT>,								//18	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK
		tuple<int, D3D12_CACHED_PIPELINE_STATE>,		//19	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO
		tuple<int, D3D12_PIPELINE_STATE_FLAGS>,			//20	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS
		tuple<int, D3D12_DEPTH_STENCIL_DESC1>,			//21	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1
		tuple<int, D3D12_VIEW_INSTANCING_DESC>,			//22	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING
		int, 										 	//23	unused
		tuple<int, D3D12_SHADER_BYTECODE>,				//24	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS
		tuple<int, D3D12_SHADER_BYTECODE>				//25	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS
		>,
		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE
	>;
};


struct PIPELINE_STATE_STREAM_DESC : D3D12_PIPELINE_STATE_STREAM_DESC {
	template<typename T> PIPELINE_STATE_STREAM_DESC(const T &t)	{ SizeInBytes = sizeof(t); pPipelineStateSubobjectStream = unconst(&t); }
	constexpr operator const D3D12_PIPELINE_STATE_STREAM_DESC*() const { return this; }

	ID3D12PipelineState *Create(ID3D12Device2 *device) {
		ID3D12PipelineState	*state;
		return SUCCEEDED(device->CreatePipelineState(*this, COM_CREATE(&state))) ? state : nullptr;
	}
};

template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE...I> auto MakePipelineDesc(PIPELINE::SUB<I>&&... i) { 
	typedef tuple<PIPELINE::SUB<I>...> B;
	struct DESC : B {
		using B::B;
		ID3D12PipelineState *Create(ID3D12Device2 *device) { return PIPELINE_STATE_STREAM_DESC(*this).Create(device); }
	};
	return DESC(move(i)...);
}

//------------------------------------------------------------------------------------------------
// StateObjects
//------------------------------------------------------------------------------------------------

struct STATE_OBJECT_DESC0 : D3D12_STATE_OBJECT_DESC {
	template<typename T> struct ClearMixin		: T { ClearMixin() : T() {} };
	template<typename T> struct ExportsMixin	: ClearMixin<T> { auto Exports() const { return make_range_n(T::pExports, T::NumExports); } };

	template<typename T> static void SetCOM(T*& dst, T* src)  {
		if (src)
			src->AddRef();
		if (dst)
			dst->Release();
		dst = src;
	}
	
	template<D3D12_STATE_SUBOBJECT_TYPE I> struct SUB;

	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG>					: ClearMixin<D3D12_STATE_OBJECT_CONFIG> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK>								: ClearMixin<D3D12_NODE_MASK> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG>				: ClearMixin<D3D12_RAYTRACING_SHADER_CONFIG> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG>			: ClearMixin<D3D12_RAYTRACING_PIPELINE_CONFIG> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP>								: ClearMixin<D3D12_HIT_GROUP_DESC> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1>			: ClearMixin<D3D12_RAYTRACING_PIPELINE_CONFIG1> {};

	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION>		: ExportsMixin<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION> : ExportsMixin<D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY>							: ExportsMixin<D3D12_DXIL_LIBRARY_DESC> {};

	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE>					: ClearMixin<D3D12_GLOBAL_ROOT_SIGNATURE> {
		~SUB()								{ if (pGlobalRootSignature) pGlobalRootSignature->Release(); }
		void Set(ID3D12RootSignature* p)	{ SetCOM(pGlobalRootSignature, p); }
	};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE>					: ClearMixin<D3D12_LOCAL_ROOT_SIGNATURE> {
		~SUB()								{ if (pLocalRootSignature) pLocalRootSignature->Release(); }
		void Set(ID3D12RootSignature* p)	{ SetCOM(pLocalRootSignature, p); }
	};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION>					: ExportsMixin<D3D12_EXISTING_COLLECTION_DESC> {
		void Set(ID3D12StateObject* p)		{ SetCOM(pExistingCollection, p); }
		~SUB()								{ if (pExistingCollection) pExistingCollection->Release(); }
	};

	auto	SubObjects() const { return make_range_n(pSubobjects, NumSubobjects); }
};

template<D3D12_STATE_SUBOBJECT_TYPE I> inline auto get(const D3D12_STATE_SUBOBJECT& sub) {
	ISO_ASSERT(sub.Type == I);
	return (const STATE_OBJECT_DESC0::SUB<I>*)sub.pDesc;
}

class STATE_OBJECT_DESC {
	STATE_OBJECT_DESC0	desc;
	
	struct SUB_BASE {};
	template<D3D12_STATE_SUBOBJECT_TYPE I> struct SUB : SUB_BASE, STATE_OBJECT_DESC0::SUB<I> {};

	struct StringHolders {
		list<string16> strings;
		LPCWSTR LocalCopy(LPCWSTR string) {
			if (string) {
				strings.push_back(string);
				return strings.back();
			}
			return nullptr;
		}
	};
	struct StringHolder {
		string16	s;
		LPCWSTR LocalCopy(LPCWSTR string) { return s = string; }
	};

	template<typename T> struct Exports {
		StringHolders						strings;
		dynamic_array<D3D12_EXPORT_DESC>	exports;
		Exports() {
			static_cast<T*>(this)->pExports		= nullptr;
			static_cast<T*>(this)->NumExports	= 0;
		}
		void DefineExport(LPCWSTR Name, LPCWSTR ExportToRename = nullptr, D3D12_EXPORT_FLAGS Flags = D3D12_EXPORT_FLAG_NONE) {
			D3D12_EXPORT_DESC& e	= exports.push_back();
			e.Name					= strings.LocalCopy(Name);
			e.ExportToRename		= strings.LocalCopy(ExportToRename);
			e.Flags					= Flags;
			static_cast<T*>(this)->pExports		= exports.begin();
			static_cast<T*>(this)->NumExports	= exports.size32();
		}
		template<size_t N> void DefineExports(LPCWSTR(&Exports)[N]) {
			for (auto i : Exports)
				DefineExport(Exports[i]);
		}
		void DefineExports(const LPCWSTR* Exports, uint32 N) {
			for (uint32 i = 0; i < N; i++)
				DefineExport(Exports[i]);
		}
	};
	template<typename T> struct StringExports {
		StringHolders			strings;
		dynamic_array<LPCWSTR>	exports;
		StringExports() {
			static_cast<T*>(this)->pExports		= nullptr;
			static_cast<T*>(this)->NumExports	= 0;
		}
		void AddExport(LPCWSTR Export) {
			exports.push_back(strings.LocalCopy(Export));
			static_cast<T*>(this)->pExports		= exports.begin();
			static_cast<T*>(this)->NumExports	= exports.size32();
		}
		template<size_t N> void AddExports(LPCWSTR (&Exports)[N]) {
			for (auto i : Exports)
				AddExport(i);
		}
		void AddExports(const LPCWSTR* Exports, uint32 N) {
			for (uint32 i = 0; i < N; i++)
				AddExport(Exports[i]);
		}
	};

	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION> : SUB_BASE, STATE_OBJECT_DESC0::SUB<D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION>, StringExports<SUB<D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION>> {
		const SUB_BASE	*sub = nullptr;
	public:
		void	Set(const SUB_BASE *sub) noexcept { this->sub = sub; }
		bool	Reassociate(range<D3D12_STATE_SUBOBJECT*> subs) {
			for (auto& j : subs) {
				if ((SUB_BASE*)j.pDesc == sub) {
					pSubobjectToAssociate = &j;
					return true;
				}
			}
			return false;
		}
	};

	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION> : SUB_BASE, STATE_OBJECT_DESC0::SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION>, StringExports<SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION>> {
		StringHolder	name;
	public:
		void	Set(LPCWSTR sub) { SubobjectToAssociate = name.LocalCopy(sub); }
	};

	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY>			: SUB_BASE, STATE_OBJECT_DESC0::SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY>, Exports<SUB<D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY>> {};
	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION>	: SUB_BASE, STATE_OBJECT_DESC0::SUB<D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION>, Exports<SUB<D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION>> {};

	template<> struct SUB<D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP> : SUB_BASE, STATE_OBJECT_DESC0::SUB<D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP> {
		StringHolder	strings[4];
	public:
		void	SetHitGroupType(D3D12_HIT_GROUP_TYPE Type)	{ this->Type = Type; }
		void	SetHitGroupExport(LPCWSTR name)				{ HitGroupExport			= strings[0].LocalCopy(name); }
		void	SetAnyHitShaderImport(LPCWSTR name)			{ AnyHitShaderImport		= strings[1].LocalCopy(name); }
		void	SetClosestHitShaderImport(LPCWSTR name)		{ ClosestHitShaderImport	= strings[2].LocalCopy(name); }
		void	SetIntersectionShaderImport(LPCWSTR name)	{ IntersectionShaderImport	= strings[3].LocalCopy(name); }
	};

	dynamic_array<D3D12_STATE_SUBOBJECT>				array;
	dynamic_array<unique_ptr<SUB_BASE, void(*)(void*)>>	owned;

public:
	STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE type = D3D12_STATE_OBJECT_TYPE_COLLECTION) {
		desc.Type			= type;
		desc.pSubobjects	= nullptr;
		desc.NumSubobjects	= 0;
	}
	operator const D3D12_STATE_OBJECT_DESC&() {
		desc.pSubobjects	= array.begin();
		desc.NumSubobjects	= array.size32();
		for (auto& i : array) {
			if (i.Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
				ISO_VERIFY(((SUB<D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION>*)i.pDesc)->Reassociate(array));
		}
		return desc;
	}
	operator const D3D12_STATE_OBJECT_DESC*() {
		return &static_cast<const D3D12_STATE_OBJECT_DESC&>(*this);
	}

	void AddSubobject(D3D12_STATE_SUBOBJECT_TYPE type, const void *desc)		{ array.push_back({type, desc}); }
	void AddSubobject(const D3D12_STATE_OBJECT_CONFIG					*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG, desc); }
	void AddSubobject(const D3D12_GLOBAL_ROOT_SIGNATURE					*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, desc); }
	void AddSubobject(const D3D12_LOCAL_ROOT_SIGNATURE					*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, desc); }
	void AddSubobject(const D3D12_NODE_MASK								*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK, desc); }
	void AddSubobject(const D3D12_DXIL_LIBRARY_DESC						*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, desc); }
	void AddSubobject(const D3D12_EXISTING_COLLECTION_DESC				*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION, desc); }
	void AddSubobject(const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION		*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, desc); }
	void AddSubobject(const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION	*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION, desc); }
	void AddSubobject(const D3D12_RAYTRACING_SHADER_CONFIG				*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, desc); }
	void AddSubobject(const D3D12_RAYTRACING_PIPELINE_CONFIG			*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, desc); }
	void AddSubobject(const D3D12_HIT_GROUP_DESC						*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, desc); }
	void AddSubobject(const D3D12_RAYTRACING_PIPELINE_CONFIG1			*desc)	{ AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1, desc); }

	template<D3D12_STATE_SUBOBJECT_TYPE I> auto CreateSubobject() {
		auto sub = new SUB<I>;
		AddSubobject(I, sub);
		owned.emplace_back(sub, deleter_fn<SUB<I>>);
		return sub;
	}
};

//------------------------------------------------------------------------------------------------

inline void Execute(ID3D12CommandQueue *queue, ID3D12GraphicsCommandList *list) {
	ID3D12CommandList	*list0 = list;
	queue->ExecuteCommandLists(1, &list0);
}

inline void ExecuteReset(ID3D12CommandQueue *queue, ID3D12GraphicsCommandList *list, ID3D12CommandAllocator *alloc, ID3D12PipelineState *init = nullptr) {
	list->Close();
	Execute(queue, list);
	list->Reset(alloc, init);
}

struct CommandListWithAlloc {
	com_ptr<ID3D12CommandAllocator>		alloc;
	com_ptr<ID3D12GraphicsCommandList>	list;

	CommandListWithAlloc() {}
	CommandListWithAlloc(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) {
		ISO_VERIFY(init(device, type));
	}
	bool init(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) {
		return	SUCCEEDED(device->CreateCommandAllocator(type, COM_CREATE(&alloc)))
			&&	SUCCEEDED(device->CreateCommandList(1, type, alloc, 0, COM_CREATE(&list)));
	}
	void ExecuteReset(ID3D12CommandQueue *queue, ID3D12PipelineState *init = nullptr) {
		dx12::ExecuteReset(queue, list, alloc, init);
	}
	void Execute(ID3D12CommandQueue *queue) {
		dx12::Execute(queue, list);
	}
	void	Reset() {
		auto hr = list->Close();
		ISO_ASSERT(hr != S_OK);
		alloc->Reset();
		list->Reset(alloc, nullptr);
	}
	operator ID3D12GraphicsCommandList*() const { return list; }
	ID3D12GraphicsCommandList* operator->() const { return list; }
};

struct CommandListWithAllocAndQueue : CommandListWithAlloc {
	com_ptr<ID3D12CommandQueue>		queue;

	CommandListWithAllocAndQueue() {}
	CommandListWithAllocAndQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) {
		ISO_VERIFY(init(device, type));
	}
	bool init(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) {
		return	CommandListWithAlloc::init(device, type)
			&&	SUCCEEDED(device->CreateCommandQueue(COMMAND_QUEUE_DESC(type), COM_CREATE(&queue)));
	}
	void ExecuteReset(ID3D12PipelineState *init = nullptr) {
		CommandListWithAlloc::ExecuteReset(queue, init);
	}
};

struct Fence : com_ptr<ID3D12Fence> {
	HANDLE	event	= 0;
	Fence() {}
	Fence(ID3D12Device *device, uint64 value = 0) { ISO_VERIFY(init(device, value)); }
	~Fence()	{ if (event) CloseHandle(event); }

	bool	init(ID3D12Device *device, uint64 value) {
		return SUCCEEDED(device->CreateFence(value, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&*this));
	}
	bool	check(uint64 value) const {
		return get()->GetCompletedValue() >= value;
	}
	bool	wait_always(uint64 value) {
		if (!event)
			event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		return SUCCEEDED(get()->SetEventOnCompletion(value, event))
			&& WaitForSingleObjectEx(event, INFINITE, FALSE) == WAIT_OBJECT_0;
	}
	bool	wait(uint64 value) {
		return check(value) || wait_always(value);
	}
	void	signal(ID3D12CommandQueue *queue, uint64 value) const {
		queue->Signal(get(), value);
	}
	void	wait(ID3D12CommandQueue *queue, uint64 value) const {
		queue->Wait(get(), value);
	}
};

struct Waiter {
	Fence	fence;
	uint32	value = 0;
	Waiter() {}
	Waiter(ID3D12Device *device) : fence(device, 0) {}
	bool	init(ID3D12Device *device) { return fence.init(device, value); }
	bool	Wait(ID3D12CommandQueue *q) {
		fence.signal(q, ++value);
		return fence.wait_always(value);
	}
};

//-----------------------------------------------------------------------------
//	Uploading
//-----------------------------------------------------------------------------

struct ResourceData {
	ID3D12Resource		*res;
	void				*p;
	ResourceData(ID3D12Resource *res, int sub = 0, D3D12_RANGE *read = nullptr) : res(res) { ISO_VERIFY(SUCCEEDED(res->Map(sub, read, &p)));  }
	~ResourceData()		{ res->Unmap(0, nullptr); }
	template<typename T> operator T*() const	{ return (T*)p; }
};

bool			DownloadFromBuffer(ID3D12Device *device, ID3D12GraphicsCommandList *list, ID3D12Resource *res, ID3D12Resource *buffer, uint32 sub);
bool			DownloadFromBuffer(ID3D12Device *device, ID3D12GraphicsCommandList *list, ID3D12Resource *res, ID3D12Resource *buffer);
com_ptr<ID3D12Resource> DownloadToReadback(ID3D12Device *device, ID3D12GraphicsCommandList *list, ID3D12Resource *res, uint64 *total_size);
com_ptr<ID3D12Resource> DownloadToReadback(ID3D12Device *device, ID3D12GraphicsCommandList *list, ID3D12Resource *res, uint64 *total_size, uint32 sub);

malloc_block	DownloadToMemory(ID3D12Device *device, ID3D12Resource *res, uint64 *total_size);
bool			DownloadResource(ID3D12Resource *res, memory_block out);
malloc_block	DownloadResource(ID3D12Resource *res, size_t size);

struct Downloader : CommandListWithAllocAndQueue {
	ID3D12Device*					device;

	Downloader(ID3D12Device *device) : CommandListWithAllocAndQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT), device(device) {}

	void	Transition(ID3D12Resource *res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 sub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
		if (from != to)
			list->ResourceBarrier(1, RESOURCE_BARRIER::Transition(res, from, to, sub));
		//dx12::Transition(list, res, from, to, sub);
	}
	ID3D12Resource* Download(ID3D12Resource* res, uint64* total_size) {
		return DownloadToReadback(device, list, res, total_size);
	}
	ID3D12Resource* Download(ID3D12Resource* res, uint64* total_size, uint32 sub) {
		return DownloadToReadback(device, list, res, total_size, sub);
	}
};

struct Uploader : CommandListWithAllocAndQueue {
	struct Block {
		const void	*data;
		size_t		row_pitch;
		size_t		slice_pitch;
		Block() {}
		Block(const void *data, size_t row_pitch, size_t slice_pitch = 0) : data(data), row_pitch(row_pitch), slice_pitch(slice_pitch) {}
		Block(const void *data, const PLACED_SUBRESOURCE_FOOTPRINT &layout) : Block(data, layout.Footprint.RowPitch, layout.SlicePitch()) {}
	};

	uint64						buffer_size = 0;
	com_ptr<ID3D12Resource>		buffer, buffer2;
	Waiter						waiter;
	void						*buffer_start, *buffer_start2;
	uint64						offset;

	Uploader()	{}
	Uploader(ID3D12Device *device, uint64 size = 16 * 1024 * 1024) {
		ISO_VERIFY(Begin(device, size));
	}
	bool	Begin(ID3D12Device *device, uint64 size = 16 * 1024 * 1024);

	bool	Wait() {
		return waiter.Wait(queue);
	}
	void	End() {
		ExecuteReset();
		waiter.Wait(queue);
	}
	void	Flush() {
		ExecuteReset();

		waiter.fence.signal(queue, ++waiter.value);
		waiter.fence.wait_always(waiter.value - 1);

		swap(buffer, buffer2);
		swap(buffer_start, buffer_start2);
		offset = 0;
	}

	uint64	AddOffset(uint64 size) {
		return exchange(offset, (offset + size + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) & -D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	}

	static const uint8 *Copy(const Block &dest, const Block &srce, size_t rowsize, uint32 rows) {
		uint8		*d = (uint8*)dest.data;
		const uint8	*s = (const uint8*)srce.data;
		for (uint32 y = rows; y--; d += dest.row_pitch, s += srce.row_pitch)
			memcpy(d, s, rowsize);
		return s;
	}

	static const uint8 *Copy(const Block &dest, const Block &srce, size_t rowsize, uint32 rows, uint32 slices) {
		uint8		*d = (uint8*)dest.data;
		const uint8	*s = (const uint8*)srce.data;
		for (uint32 z = slices; z--; d += dest.slice_pitch, s += srce.slice_pitch) {
			uint8		*d1 = d;
			const uint8	*s1 = s;
			for (uint32 y = rows; y--; d1 += dest.row_pitch, s1 += srce.row_pitch)
				memcpy(d1, s1, rowsize);
		}
		return s;
	}

	void	UploadBuffer(ID3D12Resource *resource, const_memory_block data);
	size_t	Upload(ID3D12Resource *resource, uint32 sub, PLACED_SUBRESOURCE_FOOTPRINT layout, const Block &srce);
	bool	Upload(ID3D12Device *device, ID3D12Resource *resource, const_memory_block data);

	size_t	Upload(ID3D12Device *device, ID3D12Resource *resource, uint32 sub, const Block &block) {
		RESOURCE_DESC	desc(resource);
		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
			UploadBuffer(resource, {block.data, desc.Width});
			return desc.Width;
		}
		return Upload(resource, sub, desc.SubFootprint(device, sub), block);
	}

	size_t	Upload(ID3D12Device *device, ID3D12Resource *resource, uint32 sub, const_memory_block data) {
		RESOURCE_DESC	desc(resource);
		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
			UploadBuffer(resource, data);
			return desc.Width;
		}
		auto	layout	= desc.SubFootprint(device, sub);
		return Upload(resource, sub, layout, Block(data, layout));
	}

	void	UploadFromBuffer(ID3D12Resource *resource, uint32 sub,
		uint32 x, uint32 y, uint32 z,
		uint32 w, uint32 h, uint32 d,
		ID3D12Resource	*buffer
	) {
		RESOURCE_DESC desc(resource);
		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
			sub += desc.MipLevels * z;
			z = 0;
		}
		list->CopyTextureRegion(TEXTURE_COPY_LOCATION(resource, sub), x, y, z, TEXTURE_COPY_LOCATION(buffer, 0), BOX(0,0,0, w,h,d));
	}

	static void	UploadToUpload(ID3D12Resource *resource, const_memory_block data) {
		void	*p;
		if (SUCCEEDED(resource->Map(0, 0, &p))) {
			data.copy_to(p);
			resource->Unmap(0, 0);
		}
	}
};

} // namespace dx12

template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE I> inline auto& get(const dx12::PIPELINE::SUBOBJECT& sub) {
	return sub.t.t.t.get<I>().template get<1>();
}
inline auto next(const dx12::PIPELINE::SUBOBJECT* p) {
	return (const dx12::PIPELINE::SUBOBJECT*)((const char*)p + iso::align(p->t.t.t.size(p->t.u.t), 8));
}


#endif //DX12_HELPERS_H
