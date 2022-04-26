#include <d3d12.h>
#include "dx12_helpers.h"
#include "dx\dxgi_helpers.h"

namespace iso {

struct CommandRange : holder<const ID3D12CommandList*>, interval<uint32> {
	CommandRange(ID3D12CommandList *_t);
};

typedef ID3D12Device8								ID3D12DeviceLatest;
typedef ID3D12GraphicsCommandList6					ID3D12GraphicsCommandListLatest;
typedef ID3D12PipelineLibrary1						ID3D12PipelineLibraryLatest;
typedef ID3D12Fence1								ID3D12FenceLatest;

template<> struct	Wrappable<ID3D12Device				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device1				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device2				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device3				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device4				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device5				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device6				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device7				> : T_type<ID3D12DeviceLatest> {};
template<> struct	Wrappable<ID3D12Device8				> : T_type<ID3D12DeviceLatest> {};

template<> struct	Wrappable<ID3D12GraphicsCommandList	> : T_type<ID3D12GraphicsCommandListLatest> {};
template<> struct	Wrappable<ID3D12GraphicsCommandList1> : T_type<ID3D12GraphicsCommandListLatest> {};
template<> struct	Wrappable<ID3D12GraphicsCommandList2> : T_type<ID3D12GraphicsCommandListLatest> {};
template<> struct	Wrappable<ID3D12GraphicsCommandList3> : T_type<ID3D12GraphicsCommandListLatest> {};
template<> struct	Wrappable<ID3D12GraphicsCommandList4> : T_type<ID3D12GraphicsCommandListLatest> {};
template<> struct	Wrappable<ID3D12GraphicsCommandList5> : T_type<ID3D12GraphicsCommandListLatest> {};
template<> struct	Wrappable<ID3D12GraphicsCommandList6> : T_type<ID3D12GraphicsCommandListLatest> {};

template<> struct	Wrappable<ID3D12PipelineLibrary		> : T_type<ID3D12PipelineLibraryLatest> {};
template<> struct	Wrappable<ID3D12PipelineLibrary1	> : T_type<ID3D12PipelineLibraryLatest> {};

template<> struct	Wrappable<ID3D12Fence				> : T_type<ID3D12FenceLatest> {};
template<> struct	Wrappable<ID3D12Fence1				> : T_type<ID3D12FenceLatest> {};

template<> struct TL_fields<D3D12_INPUT_ELEMENT_DESC>			: T_type<type_list<LPCSTR, UINT, DXGI_FORMAT, UINT, UINT, D3D12_INPUT_CLASSIFICATION, UINT>> {};
template<> struct TL_fields<D3D12_INPUT_LAYOUT_DESC>			: T_type<type_list<counted<const D3D12_INPUT_ELEMENT_DESC, 1>, UINT>> {};
template<> struct TL_fields<D3D12_SO_DECLARATION_ENTRY>			: T_type<type_list<UINT, LPCSTR, UINT, BYTE, BYTE, BYTE>> {};
template<> struct TL_fields<D3D12_STREAM_OUTPUT_DESC>			: T_type<type_list<counted<const D3D12_SO_DECLARATION_ENTRY, 1>, UINT, counted<const UINT, 3>, UINT, UINT>> {};
template<> struct TL_fields<D3D12_GRAPHICS_PIPELINE_STATE_DESC>	: T_type<type_list<
	ID3D12RootSignature*,
	D3D12_SHADER_BYTECODE, D3D12_SHADER_BYTECODE, D3D12_SHADER_BYTECODE, D3D12_SHADER_BYTECODE, D3D12_SHADER_BYTECODE,
	D3D12_STREAM_OUTPUT_DESC, D3D12_BLEND_DESC, UINT, D3D12_RASTERIZER_DESC, D3D12_DEPTH_STENCIL_DESC, D3D12_INPUT_LAYOUT_DESC,
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE, D3D12_PRIMITIVE_TOPOLOGY_TYPE, UINT, DXGI_FORMAT[8], DXGI_FORMAT, DXGI_SAMPLE_DESC,
	UINT, D3D12_CACHED_PIPELINE_STATE, D3D12_PIPELINE_STATE_FLAGS
>> {};
template<> struct TL_fields<D3D12_COMMAND_SIGNATURE_DESC>		: T_type<type_list<UINT, UINT, counted<const D3D12_INDIRECT_ARGUMENT_DESC, 1>, UINT>> {};
template<> struct TL_fields<D3D12_COMPUTE_PIPELINE_STATE_DESC>	: T_type<type_list<ID3D12RootSignature*, D3D12_SHADER_BYTECODE, UINT, D3D12_CACHED_PIPELINE_STATE, D3D12_PIPELINE_STATE_FLAGS>> {};
template<> struct TL_fields<D3D12_RESOURCE_BARRIER>				: T_type<type_list<
	D3D12_RESOURCE_BARRIER_TYPE,
	D3D12_RESOURCE_BARRIER_FLAGS,
	selection<0, 
		tuple<ID3D12Resource*, UINT, D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES>,	//D3D12_RESOURCE_TRANSITION_BARRIER,
		tuple<ID3D12Resource*, ID3D12Resource*>,									//D3D12_RESOURCE_ALIASING_BARRIER,
		tuple<ID3D12Resource*>														//D3D12_RESOURCE_UAV_BARRIER
	>
>> {};

template<> struct TL_fields<D3D12_TEXTURE_COPY_LOCATION>		: T_type<type_list<
	ID3D12Resource*,
	D3D12_TEXTURE_COPY_TYPE,
	selection<1,
		tuple<UINT>,								//SubresourceIndex
		tuple<UINT64, D3D12_SUBRESOURCE_FOOTPRINT>	//D3D12_PLACED_SUBRESOURCE_FOOTPRINT
	>
>> {};

using D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT = union_first<
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

template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE i> inline auto& get(const D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT& sub) {
	return sub.t.t.t.get<i>().template get<1>();
}

inline auto next(const D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT* p) {
	return (const D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT*)((const uint8*)p + align(p->t.t.t.size(p->t.u.t), 8));
}

template<> struct TL_fields<D3D12_PIPELINE_STATE_STREAM_DESC>	: T_type<type_list<
	SIZE_T,
	next_array<0, D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT, sizeof(uint64)>*
>> {};


template<> struct TL_fields<D3D12_CONSTANT_BUFFER_VIEW_DESC>	: T_type<type_list<D3D12_GPU_VIRTUAL_ADDRESS, UINT>> {};

template<template<class> class M> struct meta::map<M, D3D12_INPUT_ELEMENT_DESC>				: T_type<map_t<M, as_tuple<D3D12_INPUT_ELEMENT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_INPUT_LAYOUT_DESC>				: T_type<map_t<M, as_tuple<D3D12_INPUT_LAYOUT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_STREAM_OUTPUT_DESC>				: T_type<map_t<M, as_tuple<D3D12_STREAM_OUTPUT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_GRAPHICS_PIPELINE_STATE_DESC>	: T_type<map_t<M, as_tuple<D3D12_GRAPHICS_PIPELINE_STATE_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_COMPUTE_PIPELINE_STATE_DESC>	: T_type<map_t<M, as_tuple<D3D12_COMPUTE_PIPELINE_STATE_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_COMMAND_SIGNATURE_DESC>			: T_type<map_t<M, as_tuple<D3D12_COMMAND_SIGNATURE_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_RESOURCE_BARRIER>				: T_type<map_t<M, as_tuple<D3D12_RESOURCE_BARRIER>>> {};
template<template<class> class M> struct meta::map<M, D3D12_TEXTURE_COPY_LOCATION>			: T_type<map_t<M, as_tuple<D3D12_TEXTURE_COPY_LOCATION>>> {};
template<template<class> class M> struct meta::map<M, D3D12_PIPELINE_STATE_STREAM_DESC>		: T_type<map_t<M, as_tuple<D3D12_PIPELINE_STATE_STREAM_DESC>>> {};

template<> struct RTM<ID3D12CommandList*>						: T_type<CommandRange> {};



//struct D3D12_INDEX_BUFFER_VIEW {
//	D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
//	UINT SizeInBytes;
//	DXGI_FORMAT Format;
//};
//struct D3D12_VERTEX_BUFFER_VIEW {
//	D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
//	UINT SizeInBytes;
//	UINT StrideInBytes;
//};
//struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
//	D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
//	UINT SizeInBytes;
//};
//struct D3D12_STREAM_OUTPUT_BUFFER_VIEW {
//	D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
//	UINT64 SizeInBytes;
//	D3D12_GPU_VIRTUAL_ADDRESS BufferFilledSizeLocation;
//};
//struct D3D12_WRITEBUFFERIMMEDIATE_PARAMETER {
//	D3D12_GPU_VIRTUAL_ADDRESS Dest;
//	UINT32 Value;
//};

template<> struct PM<D3D12_SHADER_BYTECODE>									: T_type<lookup<D3D12_SHADER_BYTECODE>> {};

// modify D3D12_GPU_VIRTUAL_ADDRESS

template<> struct PM<D3D12_CPU_DESCRIPTOR_HANDLE>							: T_type<lookup<D3D12_CPU_DESCRIPTOR_HANDLE>> {};
template<> struct PM<D3D12_GPU_DESCRIPTOR_HANDLE>							: T_type<lookup<D3D12_GPU_DESCRIPTOR_HANDLE>> {};
template<> struct PM<D3D12_INDEX_BUFFER_VIEW>								: T_type<lookup<D3D12_INDEX_BUFFER_VIEW>> {};
template<> struct PM<D3D12_VERTEX_BUFFER_VIEW>								: T_type<lookup<D3D12_VERTEX_BUFFER_VIEW>> {};
template<> struct PM<D3D12_CONSTANT_BUFFER_VIEW_DESC>						: T_type<lookup<D3D12_CONSTANT_BUFFER_VIEW_DESC>> {};
template<> struct PM<D3D12_STREAM_OUTPUT_BUFFER_VIEW>						: T_type<lookup<D3D12_STREAM_OUTPUT_BUFFER_VIEW>> {};
template<> struct PM<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER>					: T_type<lookup<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER>> {};
template<> struct PM<D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE>					: T_type<lookup<D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE>> {};
template<> struct PM<D3D12_GPU_VIRTUAL_ADDRESS_RANGE>						: T_type<lookup<D3D12_GPU_VIRTUAL_ADDRESS_RANGE>> {};
template<> struct PM<D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE>			: T_type<lookup<D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE>> {};
template<> struct PM<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV>			: T_type<lookup<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV>> {};
template<> struct PM<D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC>				: T_type<lookup<D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC>> {};
template<> struct PM<D3D12_RAYTRACING_GEOMETRY_AABBS_DESC>					: T_type<lookup<D3D12_RAYTRACING_GEOMETRY_AABBS_DESC>> {};
template<> struct PM<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC>	: T_type<lookup<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC>> {};
template<> struct PM<D3D12_RAYTRACING_INSTANCE_DESC>						: T_type<lookup<D3D12_RAYTRACING_INSTANCE_DESC>> {};
template<> struct PM<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>	: T_type<lookup<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>> {};
template<> struct PM<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>	: T_type<lookup<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>> {};
template<> struct PM<D3D12_DISPATCH_RAYS_DESC>								: T_type<lookup<D3D12_DISPATCH_RAYS_DESC>> {};

template<class A>	void transfer(A &a, const lookup<D3D12_SHADER_BYTECODE> &t0, D3D12_SHADER_BYTECODE &t1) {
	t1.pShaderBytecode	= t0.t.pShaderBytecode ? a->lookup_shader(t0.t.pShaderBytecode) : t0.t.pShaderBytecode;
	t1.BytecodeLength	= t0.t.BytecodeLength;
}

template<class A>	void transfer(A &a, const lookup<D3D12_INDEX_BUFFER_VIEW> &t0, D3D12_INDEX_BUFFER_VIEW &t1) {
	t1.SizeInBytes		= t0.t.SizeInBytes;
	t1.Format			= t0.t.Format;
    t1.BufferLocation	= a->lookup(t0.t.BufferLocation, t1.SizeInBytes);
}

template<class A>	void transfer(A &a, const lookup<D3D12_VERTEX_BUFFER_VIEW> &t0, D3D12_VERTEX_BUFFER_VIEW &t1) {
	t1.SizeInBytes		= t0.t.SizeInBytes;
	t1.StrideInBytes	= t0.t.StrideInBytes;
    t1.BufferLocation	= a->lookup(t0.t.BufferLocation, t1.SizeInBytes);
}
template<class A>	void transfer(A &a, const lookup<D3D12_CONSTANT_BUFFER_VIEW_DESC> &t0, D3D12_CONSTANT_BUFFER_VIEW_DESC &t1) {
	t1.SizeInBytes		= t0.t.SizeInBytes;
    t1.BufferLocation	= a->lookup(t0.t.BufferLocation, t1.SizeInBytes);
}

template<class A>	void transfer(A &a, const lookup<D3D12_STREAM_OUTPUT_BUFFER_VIEW> &t0, D3D12_STREAM_OUTPUT_BUFFER_VIEW &t1) { t1 = t0.t; }

template<class A>	void transfer(A &a, const lookup<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER> &t0, D3D12_WRITEBUFFERIMMEDIATE_PARAMETER &t1) {
	t1.Dest				= a->lookup(t0.t.Dest, 4);
	t1.Value			= t0.t.Value;
}

template<class A>	void transfer(A &a, const lookup<D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE> &t0, D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_GPU_VIRTUAL_ADDRESS_RANGE> &t0, D3D12_GPU_VIRTUAL_ADDRESS_RANGE &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE> &t0, D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV> &t0, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC> &t0, D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_RAYTRACING_GEOMETRY_AABBS_DESC> &t0, D3D12_RAYTRACING_GEOMETRY_AABBS_DESC &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC> &t0, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_RAYTRACING_INSTANCE_DESC> &t0, D3D12_RAYTRACING_INSTANCE_DESC &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS> &t0, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> &t0, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC &t1) { t1 = t0.t; }
template<class A>	void transfer(A &a, const lookup<D3D12_DISPATCH_RAYS_DESC> &t0, D3D12_DISPATCH_RAYS_DESC &t1) { t1 = t0.t; }


}

namespace dx12 {
using namespace iso;

enum Interface : uint8 {
	INTF_GetMemory	= 0,
	INTF_Pause	= 1,
	INTF_Continue,
	INTF_CaptureFrames,
	INTF_GetObjects,
	INTF_ResourceData,
	INTF_HeapData,
	INTF_DebugOutput = 42,
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
		Handle,
		Shader,	//fake - for viewer
		
		NUM_TYPES,
		DEAD	= 1 << 7
	};
	TYPE			type;
	string16		name;

	RecObject()	: type(Unknown) {}
	RecObject(RecObject::TYPE type) : type(type) {}
	RecObject(RecObject::TYPE type, string16 &&name) : type(type), name(move(name)) {}
	constexpr bool is_dead() const			{ return !!(type & DEAD); }
	friend constexpr TYPE undead(TYPE id)	{ return TYPE(id & ~DEAD); }
};

struct RecObject2 : RecObject {
	void			*obj;
	malloc_block	info;
	RecObject2()	{}
	RecObject2(HANDLE h) : RecObject(Handle), obj(h) {}
	RecObject2(RecObject &rec, ID3D12Object *obj) : RecObject(rec), obj(obj) {}
	RecObject2(RecObject::TYPE type, string16 &&name, void *obj) : RecObject(type, move(name)), obj(obj) {}

	template<typename R>	bool read(R &&r) {
		iso::read(r, type, name, obj);
		auto	size = r.template get<uint32>();
		return info.read(r, size);
	}
	template<typename W>	bool write(W &&w) const	{
		return w.write(type, name, obj, info.size32(), info);
	}
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
		DXGI_COMPONENTS	comp = desc.Format;
		shift		= comp.IsBlock() ? 2 : 0;
		psize		= comp.Bytes();
		offset		= align((desc.Width >> shift) * psize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * (desc.Height >> shift) * slice;
		width		= max(desc.Width >> mip, 1);
		height		= max(desc.Height >> mip, 1);
		depth		= desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? max(desc.DepthOrArraySize >> mip, 1) : 1;
		row_pitch	= align((((width - 1) >> shift) + 1) * psize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	}
	D3D12BLOCK(const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &fp) {
		DXGI_COMPONENTS	comp = fp.Footprint.Format;
		offset		= fp.Offset;
		psize		= fp.Footprint.Format == DXGI_FORMAT_UNKNOWN ? 1 : comp.Bytes();
		shift		= comp.IsBlock() ? 2 : 0;
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

struct TileMapping {
	ID3D12Heap	*heap;
	uint32		offset;
	TileMapping(ID3D12Heap *_heap = 0, uint32 _offset = 0) : heap(_heap), offset(_offset) {}
};

struct TileSource {
	ID3D12Heap*						heap;
	const D3D12_TILE_RANGE_FLAGS*	flags;
	const UINT*						offsets;
	const UINT*						counts;

	uint32	flag, offset, count;

	TileSource(ID3D12Heap* _heap, const D3D12_TILE_RANGE_FLAGS* _flags, const UINT* _offsets, const UINT* _counts)
		: heap(_heap), flags(_flags), offsets(_offsets), counts(_counts)
	{
		flag	= flags		? *flags++		: D3D12_TILE_RANGE_FLAG_NONE;
		count	= counts	? *counts++		: 0x7fffffff;
		offset	= offsets	? *offsets++	: 0;
	}

	bool	skip() const {
		return flag == D3D12_TILE_RANGE_FLAG_SKIP;
	}
	TileMapping operator*()	const {
		return flag == D3D12_TILE_RANGE_FLAG_NULL
			? TileMapping()
			: TileMapping(heap, offset);
	}
	TileSource& operator++() {
		if (--count == 0) {
			if (counts)
				count	= *counts++;
			if (offsets)
				offset	= *offsets++;
			if (flags)
				flag	= *flags++;
		} else {
			if (flag == D3D12_TILE_RANGE_FLAG_NONE)
				++offset;
		}
		return *this;
	}

};

struct Tiler0 {
	TileMapping *map;
	int			width, height, depth;

	Tiler0(TileMapping *_map, int _width, int _height, int _depth) : map(_map), width(_width), height(_height), depth(_depth) {}

	uint32 CalcOffset(int x, int y, int z) const {
		ISO_ASSERT(x < width && y < height && z < depth);
		return x + width * (y + depth * z);
	}

	TileMapping& Tile(int x, int y, int z) const {
		return map[CalcOffset(x, y, z)];
	}

	void FillBox(TileSource &src, int x, int y, int z, int _width, int _height, int _depth) {
		for (int z1 = z; z1 < z + _depth; z1++) {
			for (int y1 = y; y1 < y + _height; y1++) {
				for (int x1 = x; x1 < x + _width; x1++) {
					Tile(x1, y1, z1) = *src;
					++src;
				}
			}
		}
	}

	void Fill(TileSource &src, int x, int y, int z, uint32 n) {
		for (int i = CalcOffset(x, y, z); n--; ++i) {
			map[i] = *src;
			++src;
		}
	}

	void CopyBox(Tiler0 &src, int xd, int yd, int zd, int xs, int ys, int zs, int _width, int _height, int _depth) {
		for (int z = 0; z < _depth; z++) {
			for (int y = 0; y < _height; y++) {
				for (int x = 0; x < _width; x++)
					Tile(xd + x, yd + y, zd + z) = src.Tile(xs + x, ys + y, zs + z);
			}
		}
	}

	void Copy(Tiler0 &src, int xd, int yd, int zd, int xs, int ys, int zs, uint32 n) {
		int di = CalcOffset(xd, yd, zd), si = src.CalcOffset(xs, ys, zs);
		while (n--)
			map[di++] = src.map[si++];
	}
};

struct Tiler {
	TileMapping *map;
	int			width, height, depth;
	int			mips;
	bool		volume;
	int			log2tilew, log2tileh, log2tiled;

	Tiler(const D3D12_RESOURCE_DESC &desc, TileMapping *_map) : map(_map), width(desc.Width), height(desc.Height), depth(desc.DepthOrArraySize), mips(desc.MipLevels), volume(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
		auto	layout		= DXGI_COMPONENTS(desc.Format).GetLayoutInfo();
		int		bpp			= layout.bits >> (layout.block ? 4 : 0);
		uint32	tilen		= D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES * 8 / bpp;
		int		log2tilen	= log2(tilen);

		log2tilew	= (log2tilen + 1) / 2;
		log2tileh	= log2tilen / 2;
		log2tiled	= 0;
	}

	Tiler0	SubResource(uint32 sub) {
		uint32	tile	= 0;
		int		w		= width, h = height, d = depth;

		if (volume) {
			while (sub--) {
				tile += ceil_pow2(w, log2tilew) * ceil_pow2(h, log2tileh) * ceil_pow2(d, log2tiled);
				w = max(w / 2, 1);
				h = max(h / 2, 1);
				d = max(h / 2, 1);
			}
		} else {
			for (uint32	mip = sub % mips; mip--;) {
				tile += ceil_pow2(w, log2tilew) * ceil_pow2(h, log2tileh) * d;
				w = max(w / 2, 1);
				h = max(h / 2, 1);
			}
			tile += ceil_pow2(w, log2tilew) * ceil_pow2(h, log2tileh) * (sub / mips);
			d = 1;
		}

		return Tiler0(map + tile, ceil_pow2(w, log2tilew), ceil_pow2(h, log2tileh), ceil_pow2(d, log2tiled));
	}
};

struct RecResource : RESOURCE_DESC {
	enum Allocation { UnknownAlloc, Reserved, Placed, Committed };
	Allocation					alloc;
	union {
		D3D12_GPU_VIRTUAL_ADDRESS	gpu;
		TileMapping					*mapping;
	};
	uint64						data_size;

	RecResource() : alloc(UnknownAlloc), gpu(0), data_size(0) {
		Dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
	}
	~RecResource() {
//		if (alloc == Reserved && gpu < 0x4000000000000000ull)
//			delete[] mapping;
	}

	void	init(ID3D12Device *device, Allocation _alloc, const D3D12_RESOURCE_DESC &desc) {
		alloc	= _alloc;
		*static_cast<D3D12_RESOURCE_DESC*>(this) = desc;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT	*layouts = alloc_auto(D3D12_PLACED_SUBRESOURCE_FOOTPRINT, NumSubresources(device));
		device->GetCopyableFootprints(this, 0, NumSubresources(device), 0, layouts, nullptr, nullptr, &data_size);
	}
	void	init(ID3D12Device *device, Allocation _alloc, const D3D12_RESOURCE_DESC1 &desc) {
		alloc	= _alloc;
		*static_cast<D3D12_RESOURCE_DESC*>(this) = (D3D12_RESOURCE_DESC&)desc;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT	*layouts = alloc_auto(D3D12_PLACED_SUBRESOURCE_FOOTPRINT, NumSubresources(device));
		device->GetCopyableFootprints(this, 0, NumSubresources(device), 0, layouts, nullptr, nullptr, &data_size);
	}

	bool	HasData() const { return gpu && data_size; }

	D3D12BLOCK	GetSubresource(ID3D12Device* device, uint32 i) {
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT	*layouts	= alloc_auto(D3D12_PLACED_SUBRESOURCE_FOOTPRINT, i + 1);
		device->GetCopyableFootprints(this, 0, i + 1, 0, layouts, nullptr, nullptr, nullptr);
		return D3D12BLOCK(layouts[i]);
	}
};

struct RecHeap : D3D12_HEAP_DESC {
	D3D12_GPU_VIRTUAL_ADDRESS	gpu;

	RecHeap() { clear(*this); }
	void	init(const D3D12_HEAP_DESC &desc) {
		*static_cast<D3D12_HEAP_DESC*>(this) = desc;
	}
	bool	contains(D3D12_GPU_VIRTUAL_ADDRESS a) const {
		return a >= gpu && a - gpu < SizeInBytes;
	}
};

struct RecCommandList {
	enum tag : uint8 {
	//ID3D12GraphicsCommandList
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
	//ID3D12GraphicsCommandList1
		tag_AtomicCopyBufferUINT,
		tag_AtomicCopyBufferUINT64,
		tag_OMSetDepthBounds,
		tag_SetSamplePositions,
		tag_ResolveSubresourceRegion,
		tag_SetViewInstanceMask,
	//ID3D12GraphicsCommandList2
		tag_WriteBufferImmediate,
	//ID3D12GraphicsCommandList3
		tag_SetProtectedResourceSession,
	//ID3D12GraphicsCommandList4
		tag_BeginRenderPass,
		tag_EndRenderPass,
		tag_InitializeMetaCommand,
		tag_ExecuteMetaCommand,
		tag_BuildRaytracingAccelerationStructure,
		tag_EmitRaytracingAccelerationStructurePostbuildInfo,
		tag_CopyRaytracingAccelerationStructure,
		tag_SetPipelineState1,
		tag_DispatchRays,
	//ID3D12GraphicsCommandList5
		tag_RSSetShadingRate,
		tag_RSSetShadingRateImage,
	//ID3D12GraphicsCommandList6
		tag_DispatchMesh,

		tag_NUM
	};
	UINT					node_mask;
	D3D12_COMMAND_LIST_FLAGS flags;
	D3D12_COMMAND_LIST_TYPE list_type;
	ID3D12CommandAllocator	*allocator;
	ID3D12PipelineState		*state;
	memory_block			buffer;

	void	init(UINT _node_mask, D3D12_COMMAND_LIST_TYPE _list_type, ID3D12CommandAllocator *_allocator, ID3D12PipelineState *_state) {
		node_mask		= _node_mask;
		flags			= D3D12_COMMAND_LIST_FLAG_NONE;
		list_type		= _list_type;
		allocator		= _allocator;
		state			= _state;
	}
	void init(UINT _node_mask, D3D12_COMMAND_LIST_TYPE _list_type, D3D12_COMMAND_LIST_FLAGS _flags) {
		node_mask		= _node_mask;
		flags			= _flags;
		list_type		= _list_type;
		allocator		= nullptr;
		state			= nullptr;
	}
};

struct RecDevice {
	enum tag : uint8 {
		//ID3D12Device
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
		//ID3D12Device1
		tag_CreatePipelineLibrary,
		tag_SetEventOnMultipleFenceCompletion,
		tag_SetResidencyPriority,
		//ID3D12Device2
		tag_CreatePipelineState,
		//ID3D12Device3
		tag_OpenExistingHeapFromAddress,
		tag_OpenExistingHeapFromFileMapping,
		tag_EnqueueMakeResident,
		//ID3D12Device4
		tag_CreateCommandList1,
		tag_CreateProtectedResourceSession,
		tag_CreateCommittedResource1,
		tag_CreateHeap1,
		tag_CreateReservedResource1,
		//ID3D12Device5
		tag_CreateLifetimeTracker,
		tag_CreateMetaCommand,
		tag_CreateStateObject,
		//ID3D12Device6
		tag_SetBackgroundProcessingMode,
		//ID3D12Device7
		tag_AddToStateObject,
		tag_CreateProtectedResourceSession1,
		//ID3D12Device8
		tag_CreateCommittedResource2,
		tag_CreatePlacedResource1,
		tag_CreateSamplerFeedbackUnorderedAccessView,

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
		//Event
		tag_WaitForSingleObjectEx,
		//ID3D12GraphicsCommandList
		tag_GraphicsCommandListClose,
		tag_GraphicsCommandListReset,

		tag_NUM,
	};
};

struct DESCRIPTOR {
	enum TYPE {
		NONE, CBV, SRV, UAV, RTV, DSV, SMP,
		SSMP,
		PCBV, PSRV, PUAV,
		IMM, VBV, IBV,
		DESCH,
		_NUM
	} type;
	ID3D12Resource *res;
	union {
		D3D12_CONSTANT_BUFFER_VIEW_DESC		cbv;
		D3D12_SHADER_RESOURCE_VIEW_DESC		srv;
		D3D12_UNORDERED_ACCESS_VIEW_DESC	uav;
		D3D12_RENDER_TARGET_VIEW_DESC		rtv;
		D3D12_DEPTH_STENCIL_VIEW_DESC		dsv;
		D3D12_SAMPLER_DESC					smp;
		D3D12_STATIC_SAMPLER_DESC			ssmp;
		D3D12_VERTEX_BUFFER_VIEW			vbv;
		D3D12_INDEX_BUFFER_VIEW				ibv;
		D3D12_GPU_VIRTUAL_ADDRESS			ptr;
		D3D12_GPU_DESCRIPTOR_HANDLE			h;
		const void							*imm;
	};
	DESCRIPTOR(TYPE _type = NONE) : type(_type), res(0) {}
	void	set(const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc) {
		type = CBV;
		if (desc)
			cbv = *desc;
		else clear(cbv);
	}
	void	set(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc) {
		res = pResource;
		type = SRV;
		if (desc) {
			srv = *desc;
		} else {
			clear(srv);
			srv.Shader4ComponentMapping	= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		}
	}
	void	set(ID3D12Resource *pResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc) {
		res = pResource;
		type = UAV;
		if (desc)
			uav = *desc;
		else
			clear(uav);
	}
	void	set(const D3D12_SAMPLER_DESC *desc) {
		type = SMP;
		smp = *desc;
	}
	void	set(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *desc, const D3D12_RESOURCE_DESC &rdesc) {
		res			= pResource;
		type		= RTV;
		if (desc) {
			rtv	= *desc;
			if (rtv.Format == DXGI_FORMAT_UNKNOWN)
				rtv.Format = rdesc.Format;
		} else {
			clear(rtv);
			rtv.Format = rdesc.Format;
			switch (rdesc.Dimension) {
				case D3D12_RESOURCE_DIMENSION_BUFFER:
					rtv.ViewDimension		= D3D12_RTV_DIMENSION_BUFFER;
					rtv.Buffer.NumElements	= rdesc.DepthOrArraySize;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
					rtv.ViewDimension		= D3D12_RTV_DIMENSION_TEXTURE1D;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
					rtv.ViewDimension		= D3D12_RTV_DIMENSION_TEXTURE2D;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
					rtv.ViewDimension		= D3D12_RTV_DIMENSION_TEXTURE3D;
					rtv.Texture3D.WSize		= rdesc.DepthOrArraySize;
					break;
			}
		}
	}
	void	set(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, const D3D12_RESOURCE_DESC &rdesc) {
		res			= pResource;
		type		= DSV;
		if (desc) {
			dsv	= *desc;
			if (dsv.Format == DXGI_FORMAT_UNKNOWN)
				dsv.Format = rdesc.Format;
		} else {
			clear(dsv);
			dsv.Format = rdesc.Format;
			switch (rdesc.Dimension) {
				case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
					dsv.ViewDimension	= D3D12_DSV_DIMENSION_TEXTURE1D;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
					dsv.ViewDimension	= D3D12_DSV_DIMENSION_TEXTURE2D;
					break;
			}
		}
	}

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
	D3D12_DESCRIPTOR_HEAP_TYPE	type;
	uint32						count, stride;
	D3D12_CPU_DESCRIPTOR_HANDLE	cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE	gpu;
	DESCRIPTOR					descriptors[1];

	int		index(const DESCRIPTOR *d)				const { return d - descriptors; }
	int		index(D3D12_CPU_DESCRIPTOR_HANDLE h)	const { return (h.ptr - cpu.ptr) / stride; }
	int		index(D3D12_GPU_DESCRIPTOR_HANDLE h)	const { return (h.ptr - gpu.ptr) / stride; }

	D3D12_CPU_DESCRIPTOR_HANDLE	get_cpu(int i)		const { return {cpu.ptr + i * stride}; }
	D3D12_GPU_DESCRIPTOR_HANDLE	get_gpu(int i)		const { return {gpu.ptr + i * stride}; }

	bool	holds(const DESCRIPTOR *d) const {
		return d >= descriptors && d < descriptors + count;
	}
	const DESCRIPTOR*	holds(D3D12_CPU_DESCRIPTOR_HANDLE h) const {
		return h.ptr >= cpu.ptr && h.ptr < cpu.ptr + stride * count && (h.ptr - cpu.ptr) % stride == 0 ? descriptors + index(h) : 0;
	}
	const DESCRIPTOR*	holds(D3D12_GPU_DESCRIPTOR_HANDLE h) const {
		return h.ptr >= gpu.ptr && h.ptr < gpu.ptr + stride * count && (h.ptr - gpu.ptr) % stride == 0 ? descriptors + index(h) : 0;
	}
};

struct Recording {
	ID3D12CommandList	*obj;
	memory_block		recording;
	Recording(ID3D12CommandList *_obj, void *start, size_t len) : obj(_obj), recording(start, len) {}
	Recording(ID3D12CommandList *_obj, malloc_block &&_recording) : obj(_obj), recording((const memory_block&)_recording) { _recording.p = 0; }
};

} // namespace dx12

