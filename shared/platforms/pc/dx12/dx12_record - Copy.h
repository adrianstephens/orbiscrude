#include <d3d12.h>
#include "shared\dxgi_helpers.h"

namespace iso {

enum SHADERSTAGE {
	VS, PS, DS, HS, GS,
	NUM_SHADERS,
	CS	= NUM_SHADERS,
	NUM_SHADERS_CS,
};

struct D3D12_INPUT_ELEMENT_DESC_rel {
	rel_ptr<const char>					SemanticName;
	UINT								SemanticIndex;
	DXGI_FORMAT							Format;
	UINT								InputSlot;
	UINT								AlignedByteOffset;
	D3D12_INPUT_CLASSIFICATION			InputSlotClass;
	UINT								InstanceDataStepRate;
/*	operator D3D12_INPUT_ELEMENT_DESC() const {
		D3D12_INPUT_ELEMENT_DESC desc;
		desc.SemanticName			= SemanticName;
		desc.SemanticIndex			= SemanticIndex;
		desc.Format					= Format;
		desc.InputSlot				= InputSlot;
		desc.AlignedByteOffset		= AlignedByteOffset;
		desc.InputSlotClass			= InputSlotClass;
		desc.InstanceDataStepRate	= InstanceDataStepRate;
		return desc;
	}*/
};

struct D3D12_INPUT_LAYOUT_DESC_rel {
	rel_counted<const D3D12_INPUT_ELEMENT_DESC_rel,1> pInputElementDescs;
    UINT NumElements;
};

struct D3D12_SO_DECLARATION_ENTRY_rel {
	UINT								Stream;
	rel_ptr<const char>					SemanticName;
	UINT								SemanticIndex;
	BYTE								StartComponent;
	BYTE								ComponentCount;
	BYTE								OutputSlot;
};


struct D3D12_STREAM_OUTPUT_DESC_rel  {
	rel_counted<const D3D12_SO_DECLARATION_ENTRY_rel,1> pSODeclaration;
	UINT								NumEntries;
	rel_counted<const UINT, 3>			pBufferStrides;
	UINT								NumStrides;
	UINT								RasterizedStream;
};

struct D3D12_GRAPHICS_PIPELINE_STATE_DESC_rel {
	ID3D12RootSignature					*pRootSignature;
	D3D12_SHADER_BYTECODE				VS;
	D3D12_SHADER_BYTECODE				PS;
	D3D12_SHADER_BYTECODE				DS;
	D3D12_SHADER_BYTECODE				HS;
	D3D12_SHADER_BYTECODE				GS;
	D3D12_STREAM_OUTPUT_DESC_rel		StreamOutput;
	D3D12_BLEND_DESC					BlendState;
	UINT								SampleMask;
	D3D12_RASTERIZER_DESC				RasterizerState;
	D3D12_DEPTH_STENCIL_DESC			DepthStencilState;
	D3D12_INPUT_LAYOUT_DESC_rel			InputLayout;
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE	IBStripCutValue;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE		PrimitiveTopologyType;
	UINT								NumRenderTargets;
	DXGI_FORMAT							RTVFormats[8];
	DXGI_FORMAT							DSVFormat;
	DXGI_SAMPLE_DESC					SampleDesc;
	UINT								NodeMask;
	D3D12_CACHED_PIPELINE_STATE			CachedPSO;
	D3D12_PIPELINE_STATE_FLAGS			Flags;

	const D3D12_SHADER_BYTECODE&	GetShader(SHADERSTAGE stage) const {
		return (&VS)[stage];
	}
};

//template<> struct RTM<D3D12_INPUT_ELEMENT_DESC>				{ typedef D3D12_INPUT_ELEMENT_DESC_rel type; };
//template<> struct RTM<D3D12_INPUT_LAYOUT_DESC>				{ typedef D3D12_INPUT_LAYOUT_DESC_rel type; };
//template<> struct RTM<D3D12_STREAM_OUTPUT_DESC>				{ typedef D3D12_STREAM_OUTPUT_DESC_rel type; };
template<> struct RTM<D3D12_GRAPHICS_PIPELINE_STATE_DESC>	{ typedef D3D12_GRAPHICS_PIPELINE_STATE_DESC_rel type; };

template<class A>	void allocate(A &a, const D3D12_INPUT_ELEMENT_DESC &t, D3D12_INPUT_ELEMENT_DESC_rel*) {
	a.alloc(strlen(t.SemanticName) + 1);
}
template<class A>	void transfer(A &a, const D3D12_INPUT_ELEMENT_DESC &t0, D3D12_INPUT_ELEMENT_DESC_rel &t1) {
	char *p = (char*)a.alloc(strlen(t0.SemanticName) + 1);
	strcpy(p, t0.SemanticName);
	t1.SemanticName				= p;
	t1.SemanticIndex			= t0.SemanticIndex;
	t1.Format					= t0.Format;
	t1.InputSlot				= t0.InputSlot;
	t1.AlignedByteOffset		= t0.AlignedByteOffset;
	t1.InputSlotClass			= t0.InputSlotClass;
	t1.InstanceDataStepRate		= t0.InstanceDataStepRate;
}

template<class A>	void allocate(A &a, const D3D12_SO_DECLARATION_ENTRY &t, D3D12_SO_DECLARATION_ENTRY_rel*) {
	a.alloc(strlen(t.SemanticName) + 1);
}
template<class A>	void transfer(A &a, const D3D12_SO_DECLARATION_ENTRY &t0, D3D12_SO_DECLARATION_ENTRY_rel &t1) {
	char *p = (char*)a.alloc(strlen(t0.SemanticName) + 1);
	strcpy(p, t0.SemanticName);
	t1.SemanticName				= p;
	t1.Stream					= t0.Stream;
	t1.SemanticName				= t0.SemanticName;
	t1.SemanticIndex			= t0.SemanticIndex;
	t1.StartComponent			= t0.StartComponent;
	t1.ComponentCount			= t0.ComponentCount;
	t1.OutputSlot				= t0.OutputSlot;
}

template<class A>	void transfer(A &a, const D3D12_INPUT_LAYOUT_DESC &t0, D3D12_INPUT_LAYOUT_DESC_rel &t1) {
	transfer(a, t0.pInputElementDescs, t0.NumElements, t1.pInputElementDescs);
    t1.NumElements			= t0.NumElements;
}
template<class A>	void transfer(A &a, const D3D12_STREAM_OUTPUT_DESC &t0, D3D12_STREAM_OUTPUT_DESC_rel &t1) {
	transfer(a, t0.pSODeclaration, t0.NumEntries, t1.pSODeclaration);
	transfer(a, t0.pBufferStrides, t0.NumStrides, t1.pBufferStrides);

	t1.NumEntries				= t0.NumEntries;
	t1.NumStrides				= t0.NumStrides;
	t1.RasterizedStream			= t0.RasterizedStream;
}

template<class A>	void allocate(A &a, const D3D12_GRAPHICS_PIPELINE_STATE_DESC &t, D3D12_GRAPHICS_PIPELINE_STATE_DESC_rel*) {
	allocate<D3D12_INPUT_ELEMENT_DESC_rel>(a, t.InputLayout.pInputElementDescs, t.InputLayout.NumElements);
	allocate<D3D12_SO_DECLARATION_ENTRY_rel>(a, t.StreamOutput.pSODeclaration, t.StreamOutput.NumEntries);
	allocate<UINT>(a, t.StreamOutput.pBufferStrides, t.StreamOutput.NumStrides);
}
template<class A>	void transfer(A &a, const D3D12_GRAPHICS_PIPELINE_STATE_DESC &t0, D3D12_GRAPHICS_PIPELINE_STATE_DESC_rel &t1) {
	transfer(a, t0.InputLayout, t1.InputLayout);
	transfer(a, t0.StreamOutput, t1.StreamOutput);

	t1.pRootSignature			= t0.pRootSignature;
	t1.VS						= t0.VS;
	t1.PS						= t0.PS;
	t1.DS						= t0.DS;
	t1.HS						= t0.HS;
	t1.GS						= t0.GS;
	t1.BlendState				= t0.BlendState;
	t1.SampleMask				= t0.SampleMask;
	t1.RasterizerState			= t0.RasterizerState;
	t1.DepthStencilState		= t0.DepthStencilState;
	t1.IBStripCutValue			= t0.IBStripCutValue;
	t1.PrimitiveTopologyType	= t0.PrimitiveTopologyType;
	t1.NumRenderTargets			= t0.NumRenderTargets;
	for (int i = 0; i < 8; i++)
		t1.RTVFormats[i] = t0.RTVFormats[i];
	t1.DSVFormat				= t0.DSVFormat;
	t1.SampleDesc				= t0.SampleDesc;
	t1.NodeMask					= t0.NodeMask;
	t1.CachedPSO				= t0.CachedPSO;
	t1.Flags					= t0.Flags;
}

}

namespace dx12 {
using namespace iso;

struct CaptureStats {
	uint32			num_cmdlists;
	uint32			num_descriptor_heaps;
	uint32			num_objects;
	memory_block	names;
};

struct RecObject {
public:
	enum TYPE {
		Unknown,
		RootSignature,
		Heap,
		Resource,
		CommandAllocator,
		Fence,
		PipelineState,
		DescriptorHeap,
		QueryHeap,
		CommandSignature,
		GraphicsCommandList,
		CommandQueue,
		Device,
		NUM_TYPES,
	};
	TYPE			type;
	string16		name;

	RecObject()	: type(Unknown) {}
	RecObject(RecObject::TYPE _type, const char *_name)	: type(_type), name(_name) {}
};

struct RecObject2 : RecObject {
	ID3D12Object	*obj;
	ID3D12Object	*orig;
	memory_block	info;
	RecObject2(RecObject &rec, ID3D12Object *_obj, ID3D12Object *_orig, const memory_block &_info) : RecObject(rec), obj(_obj), orig(_orig), info(_info) { name.clear(); }
	RecObject2(RecObject::TYPE _type, const char *_name, const memory_block &_info, ID3D12Object *_obj, ID3D12Object *_orig) : RecObject(_type, _name), obj(_obj), orig(_orig), info(_info) {}
};

struct D3D12BLOCK {
	uint64	offset;
	uint32	psize;
	uint32	shift;
	uint32	width;
	uint32	height;
	uint32	depth;
	uint32	row_pitch;

	D3D12BLOCK(const D3D12_RESOURCE_DESC &desc, uint32 mip, uint32 slice, uint32 plane) {
		shift		= DXGI::isblock(desc.Format) ? 2 : 0;
		psize		= DXGI::size(desc.Format);
		offset		= (desc.Height >> shift) * (desc.Width >> shift) * psize * slice;
		width		= max(desc.Width >> mip, 1);
		height		= max(desc.Height >> mip, 1);
		depth		= desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? max(desc.DepthOrArraySize >> mip, 1) : 1;
		row_pitch	= (((width - 1) >> shift) + 1) * psize;
	}
	D3D12BLOCK(const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &fp) {
		offset		= fp.Offset;
		psize		= fp.Footprint.Format == DXGI_FORMAT_UNKNOWN ? 1 : DXGI::size(fp.Footprint.Format);
		shift		= DXGI::isblock(fp.Footprint.Format) ? 2 : 0;
		width		= fp.Footprint.Width;
		height		= fp.Footprint.Height;
		depth		= fp.Footprint.Depth;
		row_pitch	= fp.Footprint.RowPitch;
	}
	bool	Valid(int x, int y, int z) const {
		return x <= width && y <= height && z <= depth;
	}
	uint64	CalcOffset(int x, int y, int z) const {
		return (z * (height >> shift) + (y >> shift)) * row_pitch + (x >> shift) * psize + offset;
	}
	uint64	Size() const {
		return ((height >> shift) - 1) * row_pitch + (width >> shift) * psize;
	}
	D3D12_BOX	Box() const {
		return D3D12_BOX {0, 0, 0, width, height, depth};
	}
};

struct RESOURCE_DESC : D3D12_RESOURCE_DESC {
	RESOURCE_DESC(const D3D12_RESOURCE_DESC *_desc) : D3D12_RESOURCE_DESC(*_desc) {}

	static inline uint8 GetFormatPlaneCount(ID3D12Device* device, DXGI_FORMAT format) {
		D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {format};
		return SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo))) ? formatInfo.PlaneCount : 0;
	}

	inline uint16	Depth()									const { return (Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1); }
	inline uint16	ArraySize()								const { return (Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1); }
	inline uint8	PlaneCount(ID3D12Device* device)		const { return GetFormatPlaneCount(device, Format); }
	inline uint32	NumSubresources(ID3D12Device* device)	const { return MipLevels * ArraySize() * PlaneCount(device); }
	inline uint32	ExtractMip(uint32 sub)					const { return sub % MipLevels; }
	inline uint32	ExtractSlice(uint32 sub)				const { return (sub / MipLevels) % ArraySize(); }
	inline uint32	ExtractPlane(uint32 sub)				const { return sub / (MipLevels * ArraySize()); }
	inline uint32	CalcSubresource(uint32 mip, uint32 slice, uint32 plane) const { return mip + MipLevels * (slice + ArraySize() * plane);}

	inline uint64	CalcTotalSize(ID3D12Device* device) {
		uint64	size = 0;
		device->GetCopyableFootprints(this, 0, NumSubresources(device), 0, nullptr, nullptr, nullptr, &size);
		return size;
	}
	D3D12BLOCK		GetSubresource(ID3D12Device* device, uint32 i) {
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT	*layouts	= alloc_auto(D3D12_PLACED_SUBRESOURCE_FOOTPRINT, i + 1);
		device->GetCopyableFootprints(this, 0, i + 1, 0, layouts, nullptr, nullptr, nullptr);
		return D3D12BLOCK(layouts[i]);
	}

};

struct RecResource : RESOURCE_DESC {
	D3D12_GPU_VIRTUAL_ADDRESS	gpu;

	RecResource(const D3D12_RESOURCE_DESC *_desc, D3D12_GPU_VIRTUAL_ADDRESS _gpu) : RESOURCE_DESC(_desc), gpu(_gpu) {}

	uint8			*data()			{ return (uint8*)(this + 1); }
	const uint8		*data()	const	{ return (const uint8*)(this + 1); }

	void			CopyBufferRegion(uint64 dst_offset, const void *src, uint64 num_bytes, size_t data_size) {
		ISO_ASSERT(dst_offset + num_bytes <= data_size);
		//return;
		memcpy(data() + dst_offset, src, num_bytes);
	}
	void			CopyBufferRegion(uint64 dst_offset, const RecResource *rsrc, uint64 src_offset, uint64 num_bytes, size_t data_size) {
		CopyBufferRegion(dst_offset, rsrc->data() + src_offset, num_bytes, data_size);
	}

	void			CopyTextureRegion(const D3D12BLOCK &dblock, const D3D12_BOX &dbox, const void *src, uint32 pitch, uint32 depth_pitch, size_t data_size) {
		ISO_ASSERT(dblock.Valid(dbox.right, dbox.bottom, dbox.back));
		//return;

		uint8	*d			= data() + dblock.CalcOffset(dbox.left, dbox.top, dbox.front);
		uint32	rowsize		= ((dbox.right - dbox.left) >> dblock.shift) * dblock.psize;
		uint32	numrows		= (dbox.bottom - dbox.top) >> dblock.shift;
		void	*data_end	= data() + data_size;

		uint8	*s0			= (uint8*)src;
		for (uint32 z = dbox.front; z < dbox.back; z++) {
			uint8	*s		= s0;
			for (uint32 y = 0; y < numrows; y++) {
				ISO_ASSERT(d + rowsize <= data_end);
				memcpy(d, s, rowsize);
				d += dblock.row_pitch;
				s += pitch;
			}
			d	+= dblock.row_pitch * ((dblock.height >> dblock.shift) - numrows);
			s0	+= depth_pitch;
		}
	}
	void			CopyTextureRegion(const D3D12BLOCK &dblock, uint32 x, uint32 y, uint32 z, const RecResource *rsrc, const D3D12BLOCK &sblock, const D3D12_BOX *box, size_t data_size) {
		ISO_ASSERT(sblock.psize == dblock.psize);

		D3D12_BOX	dbox;
		dbox.left	= x;
		dbox.top	= y;
		dbox.front	= z;

		const uint8	*src;
		if (box) {
			ISO_ASSERT(box->right > box->left && box->bottom > box->top && box->back > box->front);
			ISO_ASSERT(sblock.Valid(box->right, box->bottom, box->back));
			src			= rsrc->data() + sblock.CalcOffset(box->left, box->top, box->front);
			dbox.right	= x + box->right	- box->left;
			dbox.bottom	= y + box->bottom	- box->top;
			dbox.back	= z + box->back		- box->front;
		} else {
			ISO_ASSERT(sblock.Valid(x + 1, y + 1, z + 1));
			src			= rsrc->data() + sblock.offset;
			dbox.right	= sblock.width;
			dbox.bottom	= sblock.height;
			dbox.back	= sblock.depth;
		}
		CopyTextureRegion(dblock, dbox, src, sblock.row_pitch, sblock.row_pitch * (sblock.height >> sblock.shift), data_size);
	}
};

struct RecCommandList {
	enum tag {
/*0*/	tag_Close,
		tag_Reset,
		tag_ClearState,
		tag_DrawInstanced,
		tag_DrawIndexedInstanced,
		tag_Dispatch,
		tag_CopyBufferRegion,
		tag_CopyTextureRegion,
		tag_CopyResource,
		tag_CopyTiles,
/*10*/	tag_ResolveSubresource,
		tag_IASetPrimitiveTopology,
		tag_RSSetViewports,
		tag_RSSetScissorRects,
		tag_OMSetBlendFactor,
		tag_OMSetStencilRef,
		tag_SetPipelineState,
		tag_ResourceBarrier,
		tag_ExecuteBundle,
		tag_SetDescriptorHeaps,
/*20*/	tag_SetComputeRootSignature,
		tag_SetGraphicsRootSignature,
		tag_SetComputeRootDescriptorTable,
		tag_SetGraphicsRootDescriptorTable,
		tag_SetComputeRoot32BitConstant,
		tag_SetGraphicsRoot32BitConstant,
		tag_SetComputeRoot32BitConstants,
		tag_SetGraphicsRoot32BitConstants,
		tag_SetComputeRootConstantBufferView,
		tag_SetGraphicsRootConstantBufferView,
/*30*/	tag_SetComputeRootShaderResourceView,
		tag_SetGraphicsRootShaderResourceView,
		tag_SetComputeRootUnorderedAccessView,
		tag_SetGraphicsRootUnorderedAccessView,
		tag_IASetIndexBuffer,
		tag_IASetVertexBuffers,
		tag_SOSetTargets,
		tag_OMSetRenderTargets,
		tag_ClearDepthStencilView,
		tag_ClearRenderTargetView,
/*40*/	tag_ClearUnorderedAccessViewUint,
		tag_ClearUnorderedAccessViewFloat,
		tag_DiscardResource,
		tag_BeginQuery,
		tag_EndQuery,
		tag_ResolveQueryData,
		tag_SetPredication,
		tag_SetMarker,
		tag_BeginEvent,
		tag_EndEvent,
/*50*/	tag_ExecuteIndirect,
	};
};

struct RecDevice {
	enum tag {
		tag_CreateCommandQueue,
		tag_CreateCommandAllocator,
		tag_CreateGraphicsPipelineState,
		tag_CreateComputePipelineState,
		tag_CreateCommandList,
		tag_CheckFeatureSupport,
		tag_CreateDescriptorHeap,
		tag_CreateRootSignature,
		tag_CreateConstantBufferView,
		tag_CreateShaderResourceView,
		tag_CreateUnorderedAccessView,
		tag_CreateRenderTargetView,
		tag_CreateDepthStencilView,
		tag_CreateSampler,
		tag_CopyDescriptors,
		tag_CopyDescriptorsSimple,
		tag_CreateCommittedResource,
		tag_CreateHeap,
		tag_CreatePlacedResource,
		tag_CreateReservedResource,
		tag_CreateSharedHandle,
		tag_OpenSharedHandle,
		tag_OpenSharedHandleByName,
		tag_MakeResident,
		tag_Evict,
		tag_CreateFence,
		tag_CreateQueryHeap,
		tag_SetStablePowerState,
		tag_CreateCommandSignature,
		//ID3D12CommandQueue
		tag_CommandQueueUpdateTileMappings,
		tag_CommandQueueCopyTileMappings,
		tag_CommandQueueExecuteCommandLists,
		tag_CommandQueueSetMarker,
		tag_CommandQueueBeginEvent,
		tag_CommandQueueEndEvent,
		tag_CommandQueueSignal,
		tag_CommandQueueWait,
		//ID3D12CommandAllocator
		tag_CommandAllocatorReset,
		//ID3D12Fence
		tag_FenceSetEventOnCompletion,
		tag_FenceSignal,
	};
};

struct DESCRIPTOR {
	enum TYPE {
		NONE, CBV, SRV, UAV, RTV, DSV, SMP, _NUM
	} type;
	ID3D12Resource *res;
	union {
		D3D12_CONSTANT_BUFFER_VIEW_DESC		cbv;
		D3D12_SHADER_RESOURCE_VIEW_DESC		srv;
		D3D12_UNORDERED_ACCESS_VIEW_DESC	uav;
		D3D12_RENDER_TARGET_VIEW_DESC		rtv;
		D3D12_DEPTH_STENCIL_VIEW_DESC		dsv;
		D3D12_SAMPLER_DESC					smp;
	};
	DESCRIPTOR() : type(NONE), res(0) {}

	template<typename T>	friend T	as(TYPE t);
};

template<>	inline D3D12_DESCRIPTOR_HEAP_TYPE	as<D3D12_DESCRIPTOR_HEAP_TYPE>(DESCRIPTOR::TYPE t)		{
	static const int8 table[] = {
		-1,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
	};
	return (D3D12_DESCRIPTOR_HEAP_TYPE)table[t];
}
template<>	inline D3D12_DESCRIPTOR_RANGE_TYPE	as<D3D12_DESCRIPTOR_RANGE_TYPE>(DESCRIPTOR::TYPE t)		{
	static const int8 table[] = {
		-1,
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
		-1,
		-1,
		D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
	};
	return (D3D12_DESCRIPTOR_RANGE_TYPE)table[t];
}
template<>	inline D3D12_ROOT_PARAMETER_TYPE	as<D3D12_ROOT_PARAMETER_TYPE>(DESCRIPTOR::TYPE t)		{
	static const int8 table[] = {
		-1,
		D3D12_ROOT_PARAMETER_TYPE_CBV,
		D3D12_ROOT_PARAMETER_TYPE_SRV,
		D3D12_ROOT_PARAMETER_TYPE_UAV,
		-1,
		-1,
		-1,
	};
	return (D3D12_ROOT_PARAMETER_TYPE)table[t];
}

struct RecDescriptorHeap {
	D3D12_CPU_DESCRIPTOR_HANDLE	start;
	uint32						count, stride;
	DESCRIPTOR					*backing()			{ return (DESCRIPTOR*)(this + 1); }
	const DESCRIPTOR			*backing()	const	{ return (const DESCRIPTOR*)(this + 1); }
//	DESCRIPTOR					*backing;

	RecDescriptorHeap(uint32 _count, uint32 _stride, D3D12_CPU_DESCRIPTOR_HANDLE _start) : count(_count), stride(_stride), start(_start) {
		construct(backing(), _count);
	}

//	RecDescriptorHeap() : backing(0) {}
//	RecDescriptorHeap(uint64 _start, uint32 _stride, const memory_block &mem) : stride(_stride), count(mem.length() / sizeof(DESCRIPTOR)), backing(mem) {
//		start.ptr = _start;
//	}

	DESCRIPTOR*	holds(D3D12_CPU_DESCRIPTOR_HANDLE h) {
		return h.ptr >= start.ptr && h.ptr < start.ptr + stride * count ? backing() + (h.ptr - start.ptr) / stride : 0;
	}
};

struct Recording {
	enum TYPE { DEVICE, CMDLIST}	type;
	memory_block					recording;
	Recording(TYPE _type, void *start, size_t len) : type(_type), recording(start, len) {}
	Recording(TYPE _type, malloc_block &&_recording) : type(_type), recording((const memory_block&)_recording) { _recording.start = 0; }
};


} // namespace dx12

