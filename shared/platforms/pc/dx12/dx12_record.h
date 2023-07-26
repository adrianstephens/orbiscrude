#include <d3d12.h>
#include "dx12_helpers.h"
#include "base/sparse_array.h"

namespace iso {

struct CommandRange : holder<const ID3D12CommandList*>, interval<uint32> {
	CommandRange() {}
	CommandRange(ID3D12CommandList *t);
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


struct D3D12_GPU_VIRTUAL_ADDRESS2 {
	D3D12_GPU_VIRTUAL_ADDRESS p;
	constexpr operator D3D12_GPU_VIRTUAL_ADDRESS() const { return p; }
};

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
/*
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
*/

template<> struct TL_fields<D3D12_PIPELINE_STATE_STREAM_DESC>	: T_type<type_list<
	SIZE_T,
	next_array<0, dx12::PIPELINE::SUBOBJECT, sizeof(uint64)>*
>> {};

struct SHADER_ID {
	uint8	id[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
	SHADER_ID(const void *p) { memcpy(id, p, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES); }
	bool	operator==(const SHADER_ID &b) const { return memcmp(id, b.id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) == 0; }
};
typedef hash_map<LPCWSTR, SHADER_ID>	SHADER_IDS;

template<> struct TL_fields<D3D12_EXPORT_DESC>							: T_type<type_list<LPCWSTR, LPCWSTR, D3D12_EXPORT_FLAGS>> {};
template<> struct TL_fields<D3D12_HIT_GROUP_DESC>						: T_type<type_list<LPCWSTR, D3D12_HIT_GROUP_TYPE, LPCWSTR, LPCWSTR, LPCWSTR>> {};
template<> struct TL_fields<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION>		: T_type<type_list<dup_pointer<const uint8*>, UINT, counted<LPCWSTR, 1>>> {};
template<> struct TL_fields<D3D12_DXIL_LIBRARY_DESC>					: T_type<type_list<	D3D12_SHADER_BYTECODE, UINT, counted<D3D12_EXPORT_DESC, 1>>> {};
template<> struct TL_fields<D3D12_EXISTING_COLLECTION_DESC>				: T_type<type_list<ID3D12StateObject*, UINT, counted<D3D12_EXPORT_DESC, 1>>> {};
template<> struct TL_fields<D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION>: T_type<type_list<LPCWSTR, UINT, counted<LPCWSTR, 1>>> {};

template<> struct TL_fields<D3D12_STATE_SUBOBJECT>		: T_type<type_list<
	D3D12_STATE_SUBOBJECT_TYPE,
	selection<0, 
		D3D12_STATE_OBJECT_CONFIG*,						//0		D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG
		ID3D12RootSignature**,							//1		D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE
		ID3D12RootSignature**,							//2		D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE
		D3D12_NODE_MASK*,								//3		D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK
		uint8,											//4		unused
		D3D12_DXIL_LIBRARY_DESC*,						//5		D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY
		D3D12_EXISTING_COLLECTION_DESC*,				//6		D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*,		//7		D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION
		D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*,	//8		D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION
		D3D12_RAYTRACING_SHADER_CONFIG*,				//9		D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG
		D3D12_RAYTRACING_PIPELINE_CONFIG*,				//10	D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG
		D3D12_HIT_GROUP_DESC*,							//11	D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP
		D3D12_RAYTRACING_PIPELINE_CONFIG1*				//12	D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1
	>
> > {};
template<> struct TL_fields<D3D12_STATE_OBJECT_DESC>			: T_type<type_list<D3D12_STATE_OBJECT_TYPE, UINT, counted<save_location<const D3D12_STATE_SUBOBJECT>, 1>>> {};

template<> struct TL_fields<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>		: T_type<type_list<
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS,
	selection<0,
		tuple<
			UINT,
			D3D12_ELEMENTS_LAYOUT,
			D3D12_GPU_VIRTUAL_ADDRESS2
		>,
		tuple<
			UINT,
			D3D12_ELEMENTS_LAYOUT,
#if 1
			counted<D3D12_RAYTRACING_GEOMETRY_DESC, 0>
#else
			counted<selection<1,
				const D3D12_RAYTRACING_GEOMETRY_DESC,
				const D3D12_RAYTRACING_GEOMETRY_DESC*
			>, 0>
#endif
		>
	>
> > {};

template<> struct TL_fields<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>		: T_type<type_list<
	D3D12_GPU_VIRTUAL_ADDRESS2,
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS,
	D3D12_GPU_VIRTUAL_ADDRESS2,
	D3D12_GPU_VIRTUAL_ADDRESS2
> > {};

template<> struct TL_fields<D3D12_CONSTANT_BUFFER_VIEW_DESC>	: T_type<type_list<D3D12_GPU_VIRTUAL_ADDRESS2, UINT>> {};

template<template<class> class M> struct meta::map<M, D3D12_INPUT_ELEMENT_DESC>						: T_type<map_t<M, as_tuple<D3D12_INPUT_ELEMENT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_INPUT_LAYOUT_DESC>						: T_type<map_t<M, as_tuple<D3D12_INPUT_LAYOUT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_STREAM_OUTPUT_DESC>						: T_type<map_t<M, as_tuple<D3D12_STREAM_OUTPUT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_GRAPHICS_PIPELINE_STATE_DESC>			: T_type<map_t<M, as_tuple<D3D12_GRAPHICS_PIPELINE_STATE_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_COMPUTE_PIPELINE_STATE_DESC>			: T_type<map_t<M, as_tuple<D3D12_COMPUTE_PIPELINE_STATE_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_COMMAND_SIGNATURE_DESC>					: T_type<map_t<M, as_tuple<D3D12_COMMAND_SIGNATURE_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_RESOURCE_BARRIER>						: T_type<map_t<M, as_tuple<D3D12_RESOURCE_BARRIER>>> {};
template<template<class> class M> struct meta::map<M, D3D12_TEXTURE_COPY_LOCATION>					: T_type<map_t<M, as_tuple<D3D12_TEXTURE_COPY_LOCATION>>> {};
template<template<class> class M> struct meta::map<M, D3D12_PIPELINE_STATE_STREAM_DESC>				: T_type<map_t<M, as_tuple<D3D12_PIPELINE_STATE_STREAM_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_STATE_OBJECT_DESC>						: T_type<map_t<M, as_tuple<D3D12_STATE_OBJECT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_EXPORT_DESC>							: T_type<map_t<M, as_tuple<D3D12_EXPORT_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_HIT_GROUP_DESC>							: T_type<map_t<M, as_tuple<D3D12_HIT_GROUP_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION>		: T_type<map_t<M, as_tuple<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION>>> {};
template<template<class> class M> struct meta::map<M, D3D12_DXIL_LIBRARY_DESC>						: T_type<map_t<M, as_tuple<D3D12_DXIL_LIBRARY_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_EXISTING_COLLECTION_DESC>				: T_type<map_t<M, as_tuple<D3D12_EXISTING_COLLECTION_DESC>>> {};
template<template<class> class M> struct meta::map<M, D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION>	: T_type<map_t<M, as_tuple<D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION>>> {};
template<template<class> class M> struct meta::map<M, D3D12_STATE_SUBOBJECT>						: T_type<map_t<M, as_tuple<D3D12_STATE_SUBOBJECT>>> {};

template<template<class> class M> struct meta::map<M, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>	: T_type<map_t<M, as_tuple<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>>> {};
template<template<class> class M> struct meta::map<M, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>	: T_type<map_t<M, as_tuple<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>>> {};

template<> struct RTM<ID3D12CommandList*>	: T_type<CommandRange> {};

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

template<class A>	void transfer(A &a, const lookup<D3D12_STREAM_OUTPUT_BUFFER_VIEW> &t0, D3D12_STREAM_OUTPUT_BUFFER_VIEW &t1) {
	t1.BufferLocation			= a->lookup(t0.t.BufferLocation, t0.t.SizeInBytes);
	t1.SizeInBytes				= t0.t.SizeInBytes;
	t1.BufferFilledSizeLocation	= a->lookup(t0.t.BufferFilledSizeLocation, 8);
}

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
	INTF_Pause		= 1,
	INTF_Continue,
	INTF_CaptureFrames,
	INTF_GetObjects,
	INTF_ResourceData,
	INTF_HeapData,
	INTF_DebugOutput = 42,

	// back from target
	INTF_Status		= 0x80,
	INTF_Text		= 0x81,
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
		StateObject,
		LifetimeOwner,
		LifetimeTracker,
		MetaCommand,
		PipelineLibrary,
		ProtectedResourceSession,

		//fake - for viewer
		ResourceWrite,
		Shader,
		
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
	RecObject2(TYPE type, string16 &&name, void *obj) : RecObject(type, move(name)), obj(obj) {}

	template<typename R>	bool read(R &&r) {
		return r.read(type, name, obj) && info.read(r, r.template get<uint32>());
	}
	template<typename W>	bool write(W &&w) const	{
		return w.write(type, name, obj, info.size32(), info);
	}
};

struct Tiler {
	struct Mapping {
		ID3D12Heap	*heap;
		uint32		offset;
		Mapping(ID3D12Heap *heap = 0, uint32 offset = 0) : heap(heap), offset(offset) {}
	};

	struct Source {
		ID3D12Heap*						heap;
		const D3D12_TILE_RANGE_FLAGS*	flags;
		const UINT*						offsets;
		const UINT*						counts;
		uint32	flag, offset, count;

		Source(ID3D12Heap* heap, const D3D12_TILE_RANGE_FLAGS* flags, const UINT* offsets, const UINT*counts)
			: heap(heap), flags(flags), offsets(offsets), counts(counts)
			, flag	(flags		? *flags	: D3D12_TILE_RANGE_FLAG_NONE)
			, offset(offsets	? *offsets	: 0)
			, count	(counts		? *counts	: 0x7fffffff)
		{}

		bool	skip() const {
			return flag == D3D12_TILE_RANGE_FLAG_SKIP;
		}
		Mapping operator*()	const {
			return flag == D3D12_TILE_RANGE_FLAG_NULL ? Mapping() : Mapping(heap, offset);
		}
		Source& operator++() {
			if (--count == 0) {
				if (counts)
					count	= *++counts;
				if (offsets)
					offset	= *++offsets;
				if (flags)
					flag	= *++flags;
			} else {
				if (flag == D3D12_TILE_RANGE_FLAG_NONE)
					++offset;
			}
			return *this;
		}
	};

	struct SubTiler {
		Mapping *map;
		int		width, height, depth;

		SubTiler(Mapping *map, int width, int height, int depth) : map(map), width(width), height(height), depth(depth) {}

		uint32 CalcOffset(int x, int y, int z) const {
			ISO_ASSERT(x < width && y < height && z < depth);
			return x + width * (y + depth * z);
		}

		Mapping& Tile(int x, int y, int z) const {
			return map[CalcOffset(x, y, z)];
		}

		void FillBox(Source &src, int x, int y, int z, int _width, int _height, int _depth) {
			for (int z1 = z; z1 < z + _depth; z1++) {
				for (int y1 = y; y1 < y + _height; y1++) {
					for (int x1 = x; x1 < x + _width; x1++) {
						Tile(x1, y1, z1) = *src;
						++src;
					}
				}
			}
		}
		void FillStrip(Source &src, int x, int y, int z, uint32 n) {
			for (int i = CalcOffset(x, y, z); n--; ++i) {
				map[i] = *src;
				++src;
			}
		}
		void CopyBox(SubTiler &src, int xd, int yd, int zd, int xs, int ys, int zs, int _width, int _height, int _depth) {
			for (int z = 0; z < _depth; z++) {
				for (int y = 0; y < _height; y++) {
					for (int x = 0; x < _width; x++)
						Tile(xd + x, yd + y, zd + z) = src.Tile(xs + x, ys + y, zs + z);
				}
			}
		}
		void CopyStrip(SubTiler &src, int xd, int yd, int zd, int xs, int ys, int zs, uint32 n) {
			int di = CalcOffset(xd, yd, zd), si = src.CalcOffset(xs, ys, zs);
			while (n--)
				map[di++] = src.map[si++];
		}
	};

	RESOURCE_DESC	desc;
	Mapping*		map;
	int				log2tilew, log2tileh, log2tiled;

	Tiler(const RESOURCE_DESC &desc, Mapping *map) : desc(desc), map(map) {
		auto	layout		= DXGI_COMPONENTS(desc.Format).GetLayoutInfo();
		int		bpp			= layout.bits >> (layout.block ? 4 : 0);
		uint32	tilen		= D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES * 8 / bpp;
		int		log2tilen	= log2(tilen);

		log2tilew	= (log2tilen + 1) / 2;
		log2tileh	= log2tilen / 2;
		log2tiled	= 0;
	}

	SubTiler	SubResource(uint32 sub) {
		uint32	tile	= 0;
		int		w		= desc.Width, h = desc.Height, d = desc.DepthOrArraySize;

		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
			while (sub--) {
				tile += ceil_pow2(w, log2tilew) * ceil_pow2(h, log2tileh) * ceil_pow2(d, log2tiled);
				w = max(w / 2, 1);
				h = max(h / 2, 1);
				d = max(h / 2, 1);
			}
		} else {
			for (uint32	mip = desc.ExtractMip(sub); mip--;) {
				tile += ceil_pow2(w, log2tilew) * ceil_pow2(h, log2tileh) * d;
				w = max(w / 2, 1);
				h = max(h / 2, 1);
			}
			tile += ceil_pow2(w, log2tilew) * ceil_pow2(h, log2tileh) * desc.ExtractSlice(sub);
			d = 1;
		}

		return {map + tile, ceil_pow2(w, log2tilew), ceil_pow2(h, log2tileh), ceil_pow2(d, log2tiled)};
	}
};

static constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_UNKNOWN = (D3D12_RESOURCE_STATES)~0;

inline D3D12_RESOURCE_STATES max(D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
	if (a == D3D12_RESOURCE_STATE_UNKNOWN)
		return b;
	if (b == D3D12_RESOURCE_STATE_UNKNOWN)
		return a;
	auto	c = a | b;
	return  c & D3D12_RESOURCE_STATE_WRITE ? c & D3D12_RESOURCE_STATE_WRITE : c;
}

inline auto Transitioner(ID3D12GraphicsCommandList *list) {
	return [list](ID3D12Resource *res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 sub) {
		if (before != after)
			list->ResourceBarrier(1, RESOURCE_BARRIER::Transition(res, before, after, sub));
	};
}

template<int N> inline auto Transitioner(ID3D12GraphicsCommandList *cmd_list, Barriers<N> &b) {
	return [cmd_list, &b](ID3D12Resource *res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 sub) {
		b.Transition(cmd_list, res, before, after, sub);
	};
}

struct ResourceStates {
	sparse_array<D3D12_RESOURCE_STATES>	substates;
	D3D12_RESOURCE_STATES	state;

	ResourceStates(D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_UNKNOWN) : state(state) {}

	void operator=(D3D12_RESOURCE_STATES _state) {
		substates.clear();
		state = _state;
	}

	void set(uint32 sub, D3D12_RESOURCE_STATES _state) {
		if (sub == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
			substates.clear();
			state = _state;
		} else {
			substates[sub] = _state;
		}
	}
	void set(uint32 sub, uint32 num_sub, D3D12_RESOURCE_STATES _state) {
		if (sub != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && num_sub != 1) {
			if (_state == state) {
				substates.remove(sub);
				return;
			}
			
			substates[sub] = _state;
			if (substates.size() != num_sub)
				return;

			for (auto& j : substates)
				if (j.t != _state)
					return;
			
		}
		substates.clear();
		state = _state;
	}
	D3D12_RESOURCE_STATES get(uint32 sub) const {
		return sub == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ? state : substates[sub].or_default(state);
	}
	D3D12_RESOURCE_STATES get_all() const {
		D3D12_RESOURCE_STATES	all = state;
		for (auto &i : substates)
			all |= *i;
		return all;
	}

	void set_or(uint32 sub, D3D12_RESOURCE_STATES _state) {
		if (sub == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
			for (auto &i : substates)
				*i |= _state;
			state |= _state;
		} else {
			substates[sub] |= _state;
		}
	}
	void set_init(uint32 sub, D3D12_RESOURCE_STATES _state) {
		if (get(sub) == D3D12_RESOURCE_STATE_UNKNOWN) {
			if (sub == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
				state = _state;
			else
				set(sub, _state);
		}
	}
	void set_init(uint32 sub, uint32 num_sub, D3D12_RESOURCE_STATES _state) {
		set_init(sub == 0 && num_sub == 1 ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : sub, _state);
	}
	void combine(const ResourceStates &b) {
		if (b.substates) {
			for (auto &i : b.substates)
				substates[i.i] = i.t;
		} else {
			substates.clear();
			state = b.state;
		}
	}
	void combine_init(const ResourceStates &b) {
		if (b.substates) {
			for (auto &i : b.substates)
				set_init(i.i, i.t);
		} else {
			set_init(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, b.state);
		}
	}
	void combine_max(const ResourceStates &b) {
		if (b.substates) {
			for (auto &i : b.substates)
				substates[i.i] = max(substates[i.i].or_default(state), i.t);
		} else if (substates) {
			for (auto &i : substates)
				i.t = max(i.t, b.state);
		} else {
			state = max(state, b.state);
		}
	}

	void compact(uint32 num_sub, D3D12_RESOURCE_STATES default_state = D3D12_RESOURCE_STATE_COMMON) {
		if (state == D3D12_RESOURCE_STATE_UNKNOWN || substates.size() == num_sub)
			state = substates ? substates.begin()->t : default_state;

		bool	merge = true;
		for (auto &j : substates) {
			merge = j.t == state;
			if (!merge)
				break;
		}
		if (merge)
			substates.clear();
	}

	template<typename T> void transition_to(T &&transitioner, ID3D12Resource *res, D3D12_RESOURCE_STATES to) const {
		if (substates) {
			for (int i = 0, n = RESOURCE_DESC(res).NumSubresources(); i < n; i++)
				transitioner(res, get(i), to, i);
			//for (auto &i : substates)
			//	transitioner(res, i.t, to, i.i);
		} else {
			transitioner(res, state, to, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
	}
	template<typename T> void transition_from(T &&transitioner, ID3D12Resource *res, D3D12_RESOURCE_STATES from) const {
		if (substates) {
			for (int i = 0, n = RESOURCE_DESC(res).NumSubresources(); i < n; i++)
				transitioner(res, from, get(i), i);
			//for (auto &i : substates)
			//	transitioner(res, from, i.t, i.i);
		} else {
			transitioner(res, from, state, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
	}
	template<typename T> void transition_to(T &&transitioner, ID3D12Resource *res, const ResourceStates &to) const {
		if (substates || to.substates) {
			for (int i = 0, n = RESOURCE_DESC(res).NumSubresources(); i < n; i++)
				transitioner(res, get(i), to.get(i), i);
		} else {
			transitioner(res, state, to.state, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
	}
};

struct RecResource : RESOURCE_DESC {
	enum Allocation {
		Custom	= 8,	Cache_L0 = 0, Cache_L1 = 4,
		
		UnknownAlloc	= 0,
		Reserved,
		Placed,
		Committed,
		Committed_Upload,
		Committed_Readback,
		
		Custom_NoCPU	= Custom,
		Custom_WriteCombine,
		Custom_WriteBack,
	};
	Allocation	alloc;
	union {
		D3D12_GPU_VIRTUAL_ADDRESS	gpu;
		Tiler::Mapping				*mapping;
	};
	uint64				data_size;

	static HEAP_PROPERTIES	HeapProps(Allocation alloc) {
		if (alloc < Committed)
			return D3D12_HEAP_TYPE(0);
		if (alloc < Custom)
			return D3D12_HEAP_TYPE(alloc - Committed + 1);
		return HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY((alloc & 3) + 1), alloc & Cache_L1 ? D3D12_MEMORY_POOL_L0 : D3D12_MEMORY_POOL_L1);
	}
	static Allocation CalcAllocation(const D3D12_HEAP_PROPERTIES &props) {
		switch (props.Type) {
			default:
			case D3D12_HEAP_TYPE_DEFAULT:	return Committed;
			case D3D12_HEAP_TYPE_UPLOAD:	return Committed_Upload;
			case D3D12_HEAP_TYPE_READBACK:	return Committed_Readback;
			case D3D12_HEAP_TYPE_CUSTOM:	return Allocation(Custom + (props.CPUPageProperty - 1) + (props.MemoryPoolPreference == D3D12_MEMORY_POOL_L1 ? Cache_L1 : 0));
		}
	}

	RecResource() : alloc(UnknownAlloc), gpu(0), data_size(0) {
		Dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
	}
	~RecResource() {
//		if (alloc == Reserved && gpu < 0x4000000000000000ull)
//			delete[] mapping;
	}

	void	init(ID3D12Device *device, Allocation _alloc, const D3D12_RESOURCE_DESC &desc) {
		alloc	= _alloc;
		*static_cast<RESOURCE_DESC*>(this) = desc;
		data_size = TotalSize(device);
	}
	void	init(ID3D12Device *device, Allocation _alloc, const D3D12_RESOURCE_DESC1 &desc) {
		alloc	= _alloc;
		*static_cast<RESOURCE_DESC*>(this) = (D3D12_RESOURCE_DESC&)desc;
		data_size = TotalSize(device);
	}

	bool			HasData()	const	{ return gpu && data_size; }
	HEAP_PROPERTIES	HeapProps() const	{ return HeapProps(alloc); }
};

struct RecResourceClear : RecResource {
	D3D12_CLEAR_VALUE	clear;
	RecResourceClear() { iso::clear(clear); }
	template<typename D> void	init(ID3D12Device *device, Allocation _alloc, const D &desc, const D3D12_CLEAR_VALUE *_clear) {
		RecResource::init(device, _alloc, desc);
		if (_clear)
			clear = *_clear;
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
		
		tag_NUM,

	// breadcrumbs
		tag_InitializeExtensionCommand,
		tag_ExecuteExtensionCommand,
		tag_Present,
		tag_BeginSubmission,
		tag_EndSubmission,
		tag_EncodeFrame,
		tag_DecodeFrame,
		tag_DecodeFrame1,
		tag_DecodeFrame2,
		tag_ProcessFrames,
		tag_ProcessFrames1,
		tag_EstimateMotion,
		tag_ResolveMotionvectorHeap,
		tag_ResolveEncoderOutputMetadata,

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
/*0*/	tag_CreateCommandQueue,
		tag_CreateCommandAllocator,
		tag_CreateGraphicsPipelineState,
		tag_CreateComputePipelineState,
		tag_CreateCommandList,
		tag_CheckFeatureSupport,
		tag_CreateDescriptorHeap,
		tag_CreateRootSignature,
		tag_CreateConstantBufferView,
		tag_CreateShaderResourceView,
/*10*/	tag_CreateUnorderedAccessView,
		tag_CreateRenderTargetView,
		tag_CreateDepthStencilView,
		tag_CreateSampler,
		tag_CopyDescriptors,
		tag_CopyDescriptorsSimple,
		tag_CreateCommittedResource,
		tag_CreateHeap,
		tag_CreatePlacedResource,
		tag_CreateReservedResource,
/*20*/	tag_CreateSharedHandle,
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
/*30*/	tag_SetEventOnMultipleFenceCompletion,
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
/*40*/	tag_CreateReservedResource1,
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
/*50*/	tag_CreateSamplerFeedbackUnorderedAccessView,

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

struct D3D12_CONSTANT_BUFFER_VIEW_DESC2 : D3D12_CONSTANT_BUFFER_VIEW_DESC {

};
struct D3D12_SHADER_RESOURCE_VIEW_DESC2 : D3D12_SHADER_RESOURCE_VIEW_DESC {

};

struct D3D12_UNORDERED_ACCESS_VIEW_DESC2 : D3D12_UNORDERED_ACCESS_VIEW_DESC{

};

struct DESCRIPTOR {
	enum TYPE {
		NONE, CBV, SRV, UAV, RTV, DSV, SMP,
		SSMP,
		PCBV, PSRV, PUAV,
		IMM, VBV, IBV,
		DESCH,
		_NUM
	} type					= NONE;
	ID3D12Resource *res		= nullptr;
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
		arbitrary_const_ptr					imm;
	};
	
	template<TYPE T> struct type_s;
	template<TYPE T> using type_t = typename type_s<T>::type;
	template<typename T> static constexpr TYPE _get_type();

	template<typename T> void _set(TYPE _type, ID3D12Resource *_res, const T *desc) {
		type	= _type;
		res		= _res;
		if (desc)
			memcpy(&cbv, desc, sizeof(T));
		else
			memset(&cbv, 0, sizeof(T));
	}
	template<typename T> void _set(TYPE _type, ID3D12Resource *_res, const T &desc) {
		type	= _type;
		res		= _res;
		memcpy(&cbv, &desc, sizeof(T));
	}
	
	DESCRIPTOR() {}
	DESCRIPTOR(TYPE type);
	template<typename T> DESCRIPTOR(TYPE type, ID3D12Resource *res, const T &t)					{ _set(type, res, t); }
	template<typename T> DESCRIPTOR(const T &t)													{ _set(_get_type<T>(), nullptr, t); }
	template<TYPE T> static DESCRIPTOR make(const type_t<T> &t,ID3D12Resource *res = nullptr)	{ return DESCRIPTOR(T, res, t); }
//	template<TYPE T> static DESCRIPTOR make(const D3D12_GPU_VIRTUAL_ADDRESS2 &t)		{ return make<T>(t.p); }
	template<TYPE T> static DESCRIPTOR make(const RESOURCE_DESC &desc, ID3D12Resource *res)		{ return make<T>(type_t<T>(desc), res); }
	template<TYPE T> static DESCRIPTOR make(ID3D12Resource *res)								{ return make<T>(RESOURCE_DESC(res), res); }

	template<typename T> void set(const T *desc, ID3D12Resource *res = nullptr)	{ _set(_get_type<T>(), res, desc); }
	template<typename T> void set(const T &desc, ID3D12Resource *res = nullptr)	{ _set(_get_type<T>(), res, desc); }

	void	set(const D3D12_RENDER_TARGET_VIEW_DESC *desc, ID3D12Resource *pResource, const D3D12_RESOURCE_DESC &rdesc) {
		set(desc ? *desc : D3D12_RENDER_TARGET_VIEW_DESC(dx12::RESOURCE_DESC(rdesc)), pResource);
		if (rtv.Format == DXGI_FORMAT_UNKNOWN)
			rtv.Format = rdesc.Format;
	}
	void	set(const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, ID3D12Resource *pResource, const D3D12_RESOURCE_DESC &rdesc) {
		set(desc ? *desc : D3D12_DEPTH_STENCIL_VIEW_DESC(dx12::RESOURCE_DESC(rdesc)), pResource);
		if (dsv.Format == DXGI_FORMAT_UNKNOWN)
			dsv.Format = rdesc.Format;
	}

	bool	is_valid(D3D12_DESCRIPTOR_RANGE_TYPE kind) const {
		switch (kind) {
			case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:		return is_any(type, SRV, UAV, PSRV, PUAV);
			case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:		return is_any(type, UAV, PUAV);
			case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:		return is_any(type, CBV, PCBV);
			case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:	return is_any(type, SMP, SSMP);
			default:	return false;
		}
	}

	DXGI_FORMAT		get_format() const {
		switch (type) {
			case DESCRIPTOR::SRV:	return srv.Format;
			case DESCRIPTOR::UAV:	return uav.Format;
			case DESCRIPTOR::RTV:	return rtv.Format;
			case DESCRIPTOR::DSV:	return dsv.Format;
			default:				return DXGI_FORMAT_UNKNOWN;
		}
	}
	int		get_sub(const RESOURCE_DESC &rdesc) const {
		int		sub		= 0;
		switch (type) {
			case DESCRIPTOR::SRV: return rdesc.CalcSubresource(srv);
			case DESCRIPTOR::UAV: return rdesc.CalcSubresource(uav);
			case DESCRIPTOR::RTV: return rdesc.CalcSubresource(rtv);
			case DESCRIPTOR::DSV: return rdesc.CalcSubresource(dsv);
			default: return 0;
		}
	}
	operator VIEW_DESC() const {
		switch (type) {
			case DESCRIPTOR::SRV:	return srv;
			case DESCRIPTOR::UAV:	return uav;
			case DESCRIPTOR::RTV:	return rtv;
			case DESCRIPTOR::DSV:	return dsv;
			default:				return {};
		}
	}

	template<typename T>	friend T	as(TYPE t);
	friend constexpr bool writeable(TYPE t) { return is_any(t, UAV, RTV, DSV, PUAV); }
};

template<> struct DESCRIPTOR::type_s<DESCRIPTOR::CBV>	: T_type<D3D12_CONSTANT_BUFFER_VIEW_DESC>	{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::SRV>	: T_type<D3D12_SHADER_RESOURCE_VIEW_DESC>	{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::UAV>	: T_type<D3D12_UNORDERED_ACCESS_VIEW_DESC>	{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::RTV>	: T_type<D3D12_RENDER_TARGET_VIEW_DESC>		{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::DSV>	: T_type<D3D12_DEPTH_STENCIL_VIEW_DESC>		{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::SMP>	: T_type<D3D12_SAMPLER_DESC>				{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::SSMP>	: T_type<D3D12_SAMPLER_DESC>				{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::PCBV>	: T_type<D3D12_GPU_VIRTUAL_ADDRESS>			{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::PSRV>	: T_type<D3D12_GPU_VIRTUAL_ADDRESS>			{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::PUAV>	: T_type<D3D12_GPU_VIRTUAL_ADDRESS>			{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::IMM>	: T_type<uint32*>							{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::VBV>	: T_type<D3D12_VERTEX_BUFFER_VIEW>			{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::IBV>	: T_type<D3D12_INDEX_BUFFER_VIEW>			{};
template<> struct DESCRIPTOR::type_s<DESCRIPTOR::DESCH>	: T_type<D3D12_GPU_DESCRIPTOR_HANDLE>		{};

template<> constexpr DESCRIPTOR::TYPE DESCRIPTOR::_get_type<D3D12_CONSTANT_BUFFER_VIEW_DESC>()	{ return CBV; }
template<> constexpr DESCRIPTOR::TYPE DESCRIPTOR::_get_type<D3D12_SHADER_RESOURCE_VIEW_DESC>()	{ return SRV; }
template<> constexpr DESCRIPTOR::TYPE DESCRIPTOR::_get_type<D3D12_UNORDERED_ACCESS_VIEW_DESC>()	{ return UAV; }
template<> constexpr DESCRIPTOR::TYPE DESCRIPTOR::_get_type<D3D12_RENDER_TARGET_VIEW_DESC>()	{ return RTV; }
template<> constexpr DESCRIPTOR::TYPE DESCRIPTOR::_get_type<D3D12_DEPTH_STENCIL_VIEW_DESC>()	{ return DSV; }
template<> constexpr DESCRIPTOR::TYPE DESCRIPTOR::_get_type<D3D12_SAMPLER_DESC>()				{ return SMP; }
template<> constexpr DESCRIPTOR::TYPE DESCRIPTOR::_get_type<D3D12_STATIC_SAMPLER_DESC>()		{ return SSMP; }

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

	const DESCRIPTOR*	begin()	const	{ return descriptors; }
	const DESCRIPTOR*	end()	const	{ return descriptors + count; }

	int		index(const DESCRIPTOR *d)				const { return d - descriptors; }
	int		index(D3D12_CPU_DESCRIPTOR_HANDLE h)	const { return (h.ptr - cpu.ptr) / stride; }
	int		index(D3D12_GPU_DESCRIPTOR_HANDLE h)	const { return (h.ptr - gpu.ptr) / stride; }

	D3D12_CPU_DESCRIPTOR_HANDLE	get_cpu(int i)		const { return {cpu.ptr + i * stride}; }
	D3D12_GPU_DESCRIPTOR_HANDLE	get_gpu(int i)		const { return {gpu.ptr + i * stride}; }
	D3D12_GPU_DESCRIPTOR_HANDLE	get_gpu(D3D12_CPU_DESCRIPTOR_HANDLE h)	const { return {h.ptr + (gpu.ptr - cpu.ptr)}; }

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

