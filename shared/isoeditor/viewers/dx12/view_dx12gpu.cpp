#include "main.h"
#include "graphics.h"
#include "thread.h"
#include "jobs.h"
#include "fibers.h"
#include "vm.h"
#include "disassembler.h"
#include "base/functions.h"

#include "iso/iso.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"

#include "common/shader.h"

#include "windows/filedialogs.h"
#include "windows/text_control.h"
#include "windows/dib.h"
#include "extra/indexer.h"
#include "extra/colourise.h"
#include "hashes/fnv.h"

#include "filetypes/3d/model_utils.h"
#include "view_dx12gpu.rc.h"
#include "resource.h"
#include "hook.h"
#include "hook_com.h"
#include "stack_dump.h"
#include "extra/xml.h"
#include "dx12/dx12_record.h"
#include "dx12/dx12_helpers.h"
#include "dx/dxgi_read.h"
#include "dx/sim_dxbc.h"
#include "dx/sim_dxil.h"
#include "..\dx_shared\view_dxbc.h"
#include "view_dxil.h"
#include "dx/spdb.h"
#include "dx12_fields.h" 

#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "dxcapi.h"
#include "dxgi1_4.h"

static const uint64 GPU	= bit64(63);

#define	IDR_MENU_DX12GPU	"IDR_MENU_DX12GPU"
using namespace app;
using namespace dx12;

template<> static constexpr bool field_is_struct<D3D12_GPU_VIRTUAL_ADDRESS2> = false;
template<> struct field_names<D3D12_GPU_VIRTUAL_ADDRESS2>	: field_customs<D3D12_GPU_VIRTUAL_ADDRESS2>		{};

struct D3D12_WRITEBUFFERIMMEDIATE_PARAMETER2 {
	D3D12_GPU_VIRTUAL_ADDRESS2 Dest;
	UINT32 Value;
};
MAKE_FIELDS(D3D12_WRITEBUFFERIMMEDIATE_PARAMETER2, Dest, Value);

struct D3D12_VERTEX_BUFFER_VIEW2 {
	D3D12_GPU_VIRTUAL_ADDRESS2 BufferLocation;
	UINT SizeInBytes;
	UINT StrideInBytes;
};
MAKE_FIELDS(D3D12_VERTEX_BUFFER_VIEW2, BufferLocation, SizeInBytes, StrideInBytes);

struct D3D12_INDEX_BUFFER_VIEW2 {
	D3D12_GPU_VIRTUAL_ADDRESS2 BufferLocation;
	UINT SizeInBytes;
	DXGI_FORMAT Format;
};
MAKE_FIELDS(D3D12_INDEX_BUFFER_VIEW2, BufferLocation, SizeInBytes, Format);


DECLARE_VALUE_FIELD(DXGI_FORMAT);
DECLARE_VALUE_FIELD(D3D12_CPU_DESCRIPTOR_HANDLE);
DECLARE_VALUE_FIELD(D3D12_GPU_VIRTUAL_ADDRESS2);
DECLARE_VALUE_FIELD(D3D12_WRITEBUFFERIMMEDIATE_MODE);
DECLARE_VALUE_FIELD(D3D12_TILE_RANGE_FLAGS);
DECLARE_VALUE_FIELD(D3D12_RESIDENCY_PRIORITY);
DECLARE_VALUE_FIELD(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);
DECLARE_VALUE_FIELD(D3D12_PRIMITIVE_TOPOLOGY_TYPE);
DECLARE_VALUE_FIELD(D3D12_PIPELINE_STATE_FLAGS);
DECLARE_VALUE_FIELD(D3D12_SHADING_RATE_COMBINER);

field_info GraphicsCommandList_commands[] = {
	//ID3D12GraphicsCommandList
	{"Close",									ff()},
	{"Reset",									ff(fp<ID3D12CommandAllocator*>("pAllocator"),fp<ID3D12PipelineState*>("pInitialState"))},
	{"ClearState",								ff(fp<ID3D12PipelineState*>("pPipelineState"))},
	{"DrawInstanced",							ff(fp<UINT>("draw.vertex_count"),fp<UINT>("draw.instance_count"),fp<UINT>("draw.vertex_offset"),fp<UINT>("draw.instance_offset"))},
	{"DrawIndexedInstanced",					ff(fp<UINT>("IndexCountPerInstance"),fp<UINT>("draw.instance_count"),fp<UINT>("draw.index_offset"),fp<INT>("BaseVertexLocation"),fp<UINT>("draw.instance_offset"))},
	{"Dispatch",								ff(fp<UINT>("compute.dim_x"),fp<UINT>("compute.dim_y"),fp<UINT>("compute.dim_z"))},
	{"CopyBufferRegion %0 + %1, %2 + %3, %4",	ff(fp<ID3D12Resource*>("pDstBuffer"),fp<UINT64>("DstOffset"),fp<ID3D12Resource*>("pSrcBuffer"),fp<UINT64>("SrcOffset"),fp<UINT64>("NumBytes"))},
	{"CopyTextureRegion %0 + (%1,%2,%3), %4",	ff(fp<const D3D12_TEXTURE_COPY_LOCATION*>("pDst"),fp<UINT>("DstX"),fp<UINT>("DstY"),fp<UINT>("DstZ"),fp<const D3D12_TEXTURE_COPY_LOCATION*>("pSrc"),fp<const D3D12_BOX*>("pSrcBox"))},
	{"CopyResource",							ff(fp<ID3D12Resource*>("pDstResource"),fp<ID3D12Resource*>("pSrcResource"))},
	{"CopyTiles",								ff(fp<ID3D12Resource*>("pTiledResource"),fp<const D3D12_TILED_RESOURCE_COORDINATE*>("pTileRegionStartCoordinate"),fp<const D3D12_TILE_REGION_SIZE*>("pTileRegionSize"),fp<ID3D12Resource*>("pBuffer"),fp<UINT64>("BufferStartOffsetInBytes"),fp<D3D12_TILE_COPY_FLAGS>("Flags"))},
	{"ResolveSubresource",						ff(fp<ID3D12Resource*>("pDstResource"),fp<UINT>("DstSubresource"),fp<ID3D12Resource*>("pSrcResource"),fp<UINT>("SrcSubresource"),fp<DXGI_FORMAT>("Format"))},
	{"IASetPrimitiveTopology",					ff(fp<D3D12_PRIMITIVE_TOPOLOGY>("PrimitiveTopology"))},
	{"RSSetViewports",							ff(fp<UINT>("NumViewports"),fp<counted<const D3D12_VIEWPORT,0>>("pViewports"))},
	{"RSSetScissorRects",						ff(fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 0>>("pRects"))},
	{"OMSetBlendFactor",						ff(fp<const FLOAT[4]>("BlendFactor[4]"))},
	{"OMSetStencilRef %0",						ff(fp<UINT>("StencilRef"))},
	{"SetPipelineState %0",						ff(fp<ID3D12PipelineState*>("pPipelineState"))},
	{"ResourceBarrier",							ff(fp<UINT>("NumBarriers"),fp<counted<const D3D12_RESOURCE_BARRIER, 0>>("pBarriers"))},
	{"ExecuteBundle %0",						ff(fp<ID3D12GraphicsCommandList*>("pCommandList"))},
	{"SetDescriptorHeaps",						ff(fp<UINT>("NumDescriptorHeaps"),fp<counted<ID3D12DescriptorHeap* const, 0>>("pp"))},
	{"SetComputeRootSignature %0",				ff(fp<ID3D12RootSignature*>("pRootSignature"))},
	{"SetGraphicsRootSignature %0",				ff(fp<ID3D12RootSignature*>("pRootSignature"))},
	{"SetComputeRootDescriptorTable %0, %1",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_DESCRIPTOR_HANDLE>("BaseDescriptor"))},
	{"SetGraphicsRootDescriptorTable %0, %1",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_DESCRIPTOR_HANDLE>("BaseDescriptor"))},
	{"SetComputeRoot32BitConstant %0",			ff(fp<UINT>("RootParameterIndex"),fp<UINT>("SrcData"),fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetGraphicsRoot32BitConstant %0",			ff(fp<UINT>("RootParameterIndex"),fp<UINT>("SrcData"),fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetComputeRoot32BitConstants %0, %1",		ff(fp<UINT>("RootParameterIndex"),fp<UINT>("Num32BitValuesToSet"),fp<counted<const uint32, 1>>("pSrcData"), fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetGraphicsRoot32BitConstants %0, %1",	ff(fp<UINT>("RootParameterIndex"),fp<UINT>("Num32BitValuesToSet"),fp<counted<const uint32, 1>>("pSrcData"), fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetComputeRootConstantBufferView %0, %1",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS2>("BufferLocation"))},
	{"SetGraphicsRootConstantBufferView %0, %1",ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS2>("BufferLocation"))},
	{"SetComputeRootShaderResourceView %0, %1",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS2>("BufferLocation"))},
	{"SetGraphicsRootShaderResourceView %0, %1",ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS2>("BufferLocation"))},
	{"SetComputeRootUnorderedAccessView %0, %1",ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS2>("BufferLocation"))},
	{"SetGraphicsRootUnorderedAccessView %0, %1",ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS2>("BufferLocation"))},
	{"IASetIndexBuffer",						ff(fp<const D3D12_INDEX_BUFFER_VIEW2*>("pView"))},
	{"IASetVertexBuffers %0 x %1",				ff(fp<UINT>("StartSlot"),fp<UINT>("NumViews"),fp<counted<const D3D12_VERTEX_BUFFER_VIEW2,1>>("pViews"))},
	{"SOSetTargets %0 x %1",					ff(fp<UINT>("StartSlot"),fp<UINT>("NumViews"),fp<counted<const D3D12_STREAM_OUTPUT_BUFFER_VIEW, 1>>("pViews"))},
	{"OMSetRenderTargets",						ff(fp<UINT>("NumRenderTargetDescriptors"),fp<counted<const D3D12_CPU_DESCRIPTOR_HANDLE, 0>>("pRenderTargetDescriptors"), fp<BOOL>("RTsSingleHandleToDescriptorRange"), fp<const D3D12_CPU_DESCRIPTOR_HANDLE*>("pDepthStencilDescriptor"))},
	{"ClearDepthStencilView %0",				ff(fp<D3D12_CPU_DESCRIPTOR_HANDLE>("DepthStencilView"),fp<D3D12_CLEAR_FLAGS>("ClearFlags"),fp<FLOAT>("Depth"),fp<UINT8>("Stencil"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 4>>("pRects"))},
	{"ClearRenderTargetView %0",				ff(fp<D3D12_CPU_DESCRIPTOR_HANDLE>("RenderTargetView"),fp<const FLOAT[4]>("ColorRGBA[4]"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 2>>("pRects"))},
	{"ClearUnorderedAccessViewUint %2",			ff(fp<D3D12_GPU_DESCRIPTOR_HANDLE>("ViewGPUHandleInCurrentHeap"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("ViewCPUHandle"),fp<ID3D12Resource*>("pResource"),fp<const UINT[4]>("Values[4]"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 4>>("pRects"))},
	{"ClearUnorderedAccessViewFloat %2",		ff(fp<D3D12_GPU_DESCRIPTOR_HANDLE>("ViewGPUHandleInCurrentHeap"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("ViewCPUHandle"),fp<ID3D12Resource*>("pResource"),fp<const FLOAT[4]>("Values[4]"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 4>>("pRects"))},
	{"DiscardResource %0",						ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_DISCARD_REGION*>("pRegion"))},
	{"BeginQuery %0[%2]",						ff(fp<ID3D12QueryHeap*>("pQueryHeap"),fp<D3D12_QUERY_TYPE>("Type"),fp<UINT>("Index"))},
	{"EndQuery %0[%2]",							ff(fp<ID3D12QueryHeap*>("pQueryHeap"),fp<D3D12_QUERY_TYPE>("Type"),fp<UINT>("Index"))},
	{"ResolveQueryData %0[%2] x %3",			ff(fp<ID3D12QueryHeap*>("pQueryHeap"),fp<D3D12_QUERY_TYPE>("Type"),fp<UINT>("StartIndex"),fp<UINT>("NumQueries"),fp<ID3D12Resource*>("pDestinationBuffer"),fp<UINT64>("AlignedDestinationBufferOffset"))},
	{"SetPredication",							ff(fp<ID3D12Resource*>("pBuffer"),fp<UINT64>("AlignedBufferOffset"),fp<D3D12_PREDICATION_OP>("Operation"))},
	{"SetMarker",								ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"BeginEvent",								ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"EndEvent",								ff()},
	{"ExecuteIndirect",							ff(fp<ID3D12CommandSignature*>("pCommandSignature"),fp<UINT>("MaxCommandCount"),fp<ID3D12Resource*>("pArgumentBuffer"),fp<UINT64>("ArgumentBufferOffset"),fp<ID3D12Resource*>("pCountBuffer"),fp<UINT64>("CountBufferOffset"))},
	//ID3D12GraphicsCommandList1
	{"AtomicCopyBufferUINT",					ff(fp<ID3D12Resource*>("pDstBuffer"),fp<UINT64>("DstOffset"),fp<ID3D12Resource*>("pSrcBuffer"),fp<UINT64>("SrcOffset"),fp<UINT>("Dependencies"),fp<counted<ID3D12Resource *const, 4>>("ppDependentResources"),fp<const D3D12_SUBRESOURCE_RANGE_UINT64*>("pDependentSubresourceRanges"))},
	{"AtomicCopyBufferUINT64",					ff(fp<ID3D12Resource*>("pDstBuffer"),fp<UINT64>("DstOffset"),fp<ID3D12Resource*>("pSrcBuffer"),fp<UINT64>("SrcOffset"),fp<UINT>("Dependencies"),fp<counted<ID3D12Resource *const, 4>>("ppDependentResources"),fp<const D3D12_SUBRESOURCE_RANGE_UINT64*>("pDependentSubresourceRanges"))},
	{"OMSetDepthBounds",						ff(fp<FLOAT>("Min"),fp<FLOAT>("Max"))},
	{"SetSamplePositions",						ff(fp<UINT>("NumSamplesPerPixel"),fp<UINT>("NumPixels"),fp<D3D12_SAMPLE_POSITION*>("pSamplePositions"))},
	{"ResolveSubresourceRegion",				ff(fp<ID3D12Resource*>("pDstResource"),fp<UINT>("DstSubresource"),fp<UINT>("DstX"),fp<UINT>("DstY"),fp<ID3D12Resource*>("pSrcResource"),fp<UINT>("SrcSubresource"),fp<D3D12_RECT*>("pSrcRect"),fp<DXGI_FORMAT>("Format"),fp<D3D12_RESOLVE_MODE>("ResolveMode"))},
	{"SetViewInstanceMask",						ff(fp<UINT>("Mask"))},
	//ID3D12GraphicsCommandList2
	{"WriteBufferImmediate",					ff(fp<UINT>("Count"),fp<counted<const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER2, 0>>("pParams"),fp<counted<const D3D12_WRITEBUFFERIMMEDIATE_MODE, 0>>("pModes"))},
	//ID3D12GraphicsCommandList3
	{"SetProtectedResourceSession",				ff(fp<ID3D12ProtectedResourceSession*>("pProtectedResourceSession"))},
	//ID3D12GraphicsCommandList4
	{"BeginRenderPass",							ff(fp<UINT>("NumRenderTargets"),fp<counted<const D3D12_RENDER_PASS_RENDER_TARGET_DESC, 0>>("pRenderTargets"),fp<const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC*>("pDepthStencil"),fp<D3D12_RENDER_PASS_FLAGS>("Flags"))},
	{"EndRenderPass",							ff()},
	{"InitializeMetaCommand",					ff(fp<ID3D12MetaCommand*>("pMetaCommand"),fp<const void*>("pInitializationParametersData"),fp<SIZE_T>("InitializationParametersDataSizeInBytes"))},
	{"ExecuteMetaCommand",						ff(fp<ID3D12MetaCommand*>("pMetaCommand"),fp<const void*>("pExecutionParametersData"),fp<SIZE_T>("ExecutionParametersDataSizeInBytes"))},
	{"BuildRaytracingAccelerationStructure",	ff(fp<const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC*>("pDesc"),fp<UINT>("NumPostbuildInfoDescs"),fp<counted<const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC, 1>>("pPostbuildInfoDescs"))},
	{"EmitRaytracingAccelerationStructurePostbuildInfo",ff(fp<const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC*>("pDesc"),fp<UINT>("NumSourceAccelerationStructures"),fp<counted<const D3D12_GPU_VIRTUAL_ADDRESS2, 1>>("pSourceAccelerationStructureData"))},
	{"CopyRaytracingAccelerationStructure",		ff(fp<D3D12_GPU_VIRTUAL_ADDRESS2>("DestAccelerationStructureData"),fp<D3D12_GPU_VIRTUAL_ADDRESS2>("SourceAccelerationStructureData"),fp<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE>("Mode"))},
	{"SetPipelineState1",						ff(fp<ID3D12StateObject*>("pStateObject"))},
	{"DispatchRays",							ff(fp<const D3D12_DISPATCH_RAYS_DESC*>("pDesc"))},
	//ID3D12GraphicsCommandList5
	{"RSSetShadingRate",						ff(fp<D3D12_SHADING_RATE>("baseShadingRate"), fp<const D3D12_SHADING_RATE_COMBINER[2]>("combiners"))},
	{"RSSetShadingRateImage",					ff(fp<ID3D12Resource*>("shadingRateImage"))},
	//ID3D12GraphicsCommandList6
	{"DispatchMesh",							ff(fp<UINT>("ThreadGroupCountX"), fp<UINT>(" ThreadGroupCountY"), fp<UINT>(" ThreadGroupCountZ"))},
};
static_assert(num_elements(GraphicsCommandList_commands) == RecCommandList::tag_NUM, "Command table bad size");

field_info Device_commands[] = {
	//ID3D12Device
	{"CreateCommandQueue %2",					ff(fp<const D3D12_COMMAND_QUEUE_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateCommandAllocator %2",				ff(fp<D3D12_COMMAND_LIST_TYPE>("type"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateGraphicsPipelineState %2",			ff(fp<const D3D12_GRAPHICS_PIPELINE_STATE_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateComputePipelineState %2",			ff(fp<const D3D12_COMPUTE_PIPELINE_STATE_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateCommandList %2",					ff(fp<UINT>("nodeMask"),fp<D3D12_COMMAND_LIST_TYPE>("type"),fp<ID3D12CommandAllocator*>("pCommandAllocator"),fp<ID3D12PipelineState*>("pInitialState"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CheckFeatureSupport %0",					ff(fp<D3D12_FEATURE>("Feature"),fp<void*>("pFeatureSupportData"),fp<UINT>("FeatureSupportDataSize"))},
	{"CreateDescriptorHeap %2",					ff(fp<const D3D12_DESCRIPTOR_HEAP_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateRootSignature %4",					ff(fp<UINT>("nodeMask"),fp<counted<const uint8,2>>("pBlobWithRootSignature"),fp<SIZE_T>("blobLengthInBytes"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateConstantBufferView %0",				ff(fp<const D3D12_CONSTANT_BUFFER_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateShaderResourceView %0",				ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_SHADER_RESOURCE_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateUnorderedAccessView %0",			ff(fp<ID3D12Resource*>("pResource"),fp<ID3D12Resource*>("pCounterResource"),fp<const D3D12_UNORDERED_ACCESS_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateRenderTargetView %0",				ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_RENDER_TARGET_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateDepthStencilView %0",				ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_DEPTH_STENCIL_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateSampler",							ff(fp<const D3D12_SAMPLER_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CopyDescriptors",							ff(fp<UINT>("dest_num"),fp<counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0>>("dest_starts"),fp<counted<const UINT,0>>("dest_sizes"),fp<UINT>("srce_num"),fp<counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3>>("srce_starts"),fp<counted<const UINT,3>>("srce_sizes"),fp<D3D12_DESCRIPTOR_HEAP_TYPE>("type"))},
	{"CopyDescriptorsSimple %1 <-%2 x %0",		ff(fp<UINT>("num"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest_start"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("srce_start"),fp<D3D12_DESCRIPTOR_HEAP_TYPE>("type"))},
	{"CreateCommittedResource %6",				ff(fp<const D3D12_HEAP_PROPERTIES*>("pHeapProperties"),fp<D3D12_HEAP_FLAGS>("HeapFlags"),fp<const D3D12_RESOURCE_DESC*>("desc"),fp<D3D12_RESOURCE_STATES>("InitialResourceState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riidResource"),fp<void**>("pp"))},
	{"CreateHeap %2",							ff(fp<const D3D12_HEAP_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreatePlacedResource %6",					ff(fp<ID3D12Heap*>("pHeap"),fp<UINT64>("HeapOffset"),fp<const D3D12_RESOURCE_DESC*>("desc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateReservedResource %4",				ff(fp<const D3D12_RESOURCE_DESC*>("desc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateSharedHandle %3",					ff(fp<ID3D12DeviceChild*>("pObject"),fp<const SECURITY_ATTRIBUTES*>("pAttributes"),fp<DWORD>("Access"),fp<LPCWSTR>("Name"),fp<HANDLE*>("pHandle"))},
	{"OpenSharedHandle %2",						ff(fp<HANDLE>("NTHandle"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"OpenSharedHandleByName %0",				ff(fp<LPCWSTR>("Name"),fp<DWORD>("Access"),fp<HANDLE*>("pNTHandle"))},
	{"MakeResident",							ff(fp<UINT>("NumObjects"),fp<counted<ID3D12Pageable* const, 0>>("pp"))},
	{"Evict",									ff(fp<UINT>("NumObjects"),fp<counted<ID3D12Pageable* const, 0>>("pp"))},
	{"CreateFence %3",							ff(fp<UINT64>("InitialValue"),fp<D3D12_FENCE_FLAGS>("Flags"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateQueryHeap %2",						ff(fp<const D3D12_QUERY_HEAP_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"SetStablePowerState %0",					ff(fp<BOOL>("Enable"))},
	{"CreateCommandSignature %3",				ff(fp<const D3D12_COMMAND_SIGNATURE_DESC*>("desc"),fp<ID3D12RootSignature*>("pRootSignature"),fp<REFIID>("riid"),fp<void**>("pp"))},
	//ID3D12Device1
	{"CreatePipelineLibrary %3",				ff(fp<const void*>("pLibraryBlob"),fp<SIZE_T>("BlobLength"),fp<REFIID>("riid"),fp<void **>("ppPipelineLibrary"))},
	{"SetEventOnMultipleFenceCompletion",		ff(fp<ID3D12Fence* const*>("ppFences"),fp<const UINT64*>("pFenceValues"),fp<UINT>("NumFences"),fp<D3D12_MULTIPLE_FENCE_WAIT_FLAGS>("Flags"), fp<HANDLE>("hEvent"))},
	{"SetResidencyPriority",					ff(fp<UINT>("NumObjects"), fp<counted<const ID3D12Pageable*,0>>("ppObjects"),fp<counted<const D3D12_RESIDENCY_PRIORITY, 0>>("pPriorities"))},
	//ID3D12Device2
	{"CreatePipelineState %2",					ff(fp<const D3D12_PIPELINE_STATE_STREAM_DESC*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppPipelineState"))},
	//ID3D12Device3
	{"OpenExistingHeapFromAddress",				ff(fp<const void*>("pAddress"),fp<REFIID>("riid"),fp<void **>("ppvHeap"))},
	{"OpenExistingHeapFromFileMapping",			ff(fp<HANDLE>("hFileMapping"),fp<REFIID>("riid"),fp<void **>("ppvHeap"))},
	{"EnqueueMakeResident",						ff(fp<D3D12_RESIDENCY_FLAGS>("Flags"),fp<UINT>("NumObjects"), fp<counted<ID3D12Pageable* const, 1>>("ppObjects"),fp<ID3D12Fence*>("pFenceToSignal"), fp<UINT64>("FenceValueToSignal"))},
	//ID3D12Device4
	{"CreateCommandList1 %3",					ff(fp<UINT>("nodeMask"),fp<D3D12_COMMAND_LIST_TYPE>("type"),fp<D3D12_COMMAND_LIST_FLAGS>("flags"),fp<REFIID>("riid"),fp<void **>("ppCommandList"))},
	{"CreateProtectedResourceSession",			ff(fp<const D3D12_PROTECTED_RESOURCE_SESSION_DESC*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppSession"))},
	{"CreateCommittedResource1",				ff(fp<const D3D12_HEAP_PROPERTIES*>("pHeapProperties"),fp<D3D12_HEAP_FLAGS>("HeapFlags"),fp<const D3D12_RESOURCE_DESC*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialResourceState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riidResource"),fp<void **>("ppvResource"))},
	{"CreateHeap1 %3",							ff(fp<const D3D12_HEAP_DESC*>("pDesc"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riid"),fp<void **>("ppvHeap"))},
	{"CreateReservedResource1 %4",				ff(fp<const D3D12_RESOURCE_DESC*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riid"),fp<void **>("ppvResource"))},
	//ID3D12Device5
	{"CreateLifetimeTracker %2",				ff(fp<ID3D12LifetimeOwner*>("pOwner"),fp<REFIID>("riid"),fp<void **>("ppvTracker"))},
	{"CreateMetaCommand %5",					ff(fp<REFGUID>("CommandId"),fp<UINT>("NodeMask"),fp<const void*>("pCreationParametersData"),fp<SIZE_T>("CreationParametersDataSizeInBytes"),fp<REFIID>("riid"),fp<void **>("ppMetaCommand"))},
	{"CreateStateObject %2",					ff(fp<const D3D12_STATE_OBJECT_DESC*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppStateObject"))},
	//ID3D12Device6
	{"SetBackgroundProcessingMode",				ff(fp<D3D12_BACKGROUND_PROCESSING_MODE>("Mode"),fp<D3D12_MEASUREMENTS_ACTION>("MeasurementsAction"),fp<HANDLE>("hEventToSignalUponCompletion"),fp<BOOL*>("pbFurtherMeasurementsDesired"))},
	//ID3D12Device7
	{"AddToStateObject",						ff(fp<const D3D12_STATE_OBJECT_DESC*>("pAddition"),fp<ID3D12StateObject*>("pStateObjectToGrowFrom"),fp<REFIID>("riid"),fp<void **>("ppNewStateObject"))},
	{"CreateProtectedResourceSession1 %2",		ff(fp<const D3D12_PROTECTED_RESOURCE_SESSION_DESC1*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppSession"))},
	//ID3D12Device8
	{"CreateCommittedResource2 %7",				ff(fp<const D3D12_HEAP_PROPERTIES*>("pHeapProperties"),fp<D3D12_HEAP_FLAGS>("HeapFlags"),fp<const D3D12_RESOURCE_DESC1*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialResourceState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riidResource"),fp<void **>("ppvResource"))},
	{"CreatePlacedResource1 %6",				ff(fp<ID3D12Heap*>("pHeap"),fp<UINT64>("HeapOffset"),fp<const D3D12_RESOURCE_DESC1*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riid"),fp<void **>("ppvResource"))},
	{"CreateSamplerFeedbackUnorderedAccessView",ff(fp<ID3D12Resource*>("pTargetedResource"),fp<ID3D12Resource*>("pFeedbackResource"), fp<D3D12_CPU_DESCRIPTOR_HANDLE>("DestDescriptor"))},

	//ID3D12CommandQueue
	{"UpdateTileMappings",						ff(fp<ID3D12Resource*>("pResource"),fp<UINT>("NumResourceRegions"),fp<counted<const D3D12_TILED_RESOURCE_COORDINATE, 1>>("pResourceRegionStartCoordinates"), fp<const D3D12_TILE_REGION_SIZE*>("pResourceRegionSizes"), fp<ID3D12Heap*>("pHeap"), fp<UINT>("NumRanges"), fp<counted<const D3D12_TILE_RANGE_FLAGS, 5>>("pRangeFlags"),fp<counted<const UINT, 5>>("pHeapRangeStartOffsets"), fp<counted<const UINT,5>>("pRangeTileCounts"),fp<D3D12_TILE_MAPPING_FLAGS>("Flags"))},
	{"CopyTileMappings",						ff(fp<ID3D12Resource*>("pDstResource"),fp<const D3D12_TILED_RESOURCE_COORDINATE*>("pDstRegionStartCoordinate"),fp<ID3D12Resource*>("pSrcResource"),fp<const D3D12_TILED_RESOURCE_COORDINATE*>("pSrcRegionStartCoordinate"),fp<const D3D12_TILE_REGION_SIZE*>("pRegionSize"),fp<D3D12_TILE_MAPPING_FLAGS>("Flags"))},
	{"ExecuteCommandLists",						ff(fp<UINT>("NumCommandLists"),fp<counted<ID3D12CommandList* const, 0>>("pp"))},
	{"SetMarker",								ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"BeginEvent",								ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"EndEvent",								ff()},
	{"Signal %0",								ff(fp<ID3D12Fence*>("pFence"),fp<UINT64>("Value"))},
	{"Wait %0",									ff(fp<ID3D12Fence*>("pFence"),fp<UINT64>("Value"))},

	//ID3D12CommandAllocator
	{"Reset",									ff()},

	//ID3D12Fence
	{"SetEventOnCompletion",					ff(fp<UINT64>("Value"),fp<HANDLE>("hEvent"))},
	{"Signal %0",								ff(fp<UINT64>("Value"))},

	//Event
	{"WaitForSingleObjectEx",					ff(fp<DWORD>("dwMilliseconds"),fp<BOOL>("bAlertable"))},

	//ID3D12GraphicsCommandList
	{"GraphicsCommandListClose",				ff()},
	{"GraphicsCommandListReset",				ff(fp<ID3D12CommandAllocator*>("pAllocator"),fp<ID3D12PipelineState*>("pInitialState"))},
};
static_assert(num_elements(Device_commands) == RecDevice::tag_NUM, "Device command table bad size");

namespace dx12 {

template<typename W> bool write(W &w, const VIEW_DESC &view) {
	return w.write(view.format, view.component_mapping, view.dim, view.first_mip, view.num_mips, view.first_slice, view.num_slices);
}

uint64 hash(const DESCRIPTOR &d) {
	hash_stream<FNV<64>>	fnv;
	fnv.write(d.res, VIEW_DESC(d));
	return fnv;
}

}

const Cursor DX12cursors[] {
	Cursor::LoadSystem(IDC_HAND),
	CompositeCursor(Cursor::LoadSystem(IDC_HAND), Cursor::Load(IDR_OVERLAY_ADD, 0)),
};

//-----------------------------------------------------------------------------
//	DX12Shader
//-----------------------------------------------------------------------------

struct DX12Shader : dx::Shader {
	using dx::Shader::Shader;

	dx::DeclResources	DeclResources(const char *entry = nullptr)	const;
	bool				IsDXIL()		const	{
		if (auto dxbc = DXBC())
			return !!dxbc->GetBlob<dx::DXBC::DXIL>();
		return false;
	}

	uint3p				GetThreadGroup() const {
		if (auto dxbc = DXBC()) {
			dx::DXBC::UcodeHeader	header;
			auto	ucode = dxbc->GetUCode(header);
			if (header.MajorVersion < 6)
				return dx::GetThreadGroupDXBC(ucode);

			if (auto psv = dxbc->GetBlob<dxil::PSV>()) {
				if (auto info2 = psv->GetRuntimeInfo<2>())
					return {info2->NumThreadsX, info2->NumThreadsY, info2->NumThreadsZ};
			}
		}
		return zero;
	}

	const char*			GetMangledName(const char* name) const {
		if (name) {
			if (auto dxbc = DXBC()) {
				if (auto rdat = dxbc->GetBlob<dx::DXBC::RuntimeData>()) {
					typedef noref_t<decltype(*rdat)>	RDAT;
					const_memory_block	strings;
					for (auto &i : rdat->tables()) {
						switch (i.type) {
							case rdat->StringBuffer:
								strings = i.raw();
								break;
							case rdat->FunctionTable:
								for (auto &j : i.table<dxil::RDAT_Function>()) {
									if (j.unmangled_name.get(strings) == str(name))
										return j.name.get(strings);
								}
								break;
						}

					}
				}
			}
		}
		return nullptr;
	}
};

extern class DisassemblerDXIL dxil_dis;

Disassembler::State *Disassemble(const_memory_block ucode, uint64 addr, bool dxil) {
	unused(&dxil_dis);
	if (ucode) {
		if (dxil) {
			static Disassembler	*dis = Disassembler::Find("DXIL");
			if (dis)
				return dis->Disassemble(ucode, addr);
		} else {
			static Disassembler	*dis = Disassembler::Find("DXBC");
			if (dis)
				return dis->Disassemble(ucode, addr);
		}
	}
	return 0;
}

dx::DeclResources DX12Shader::DeclResources(const char *name) const {
	dx::DeclResources	decl;

	if (auto dxbc = DXBC()) {
		dx::DXBC::UcodeHeader	header;
		auto	ucode = dxbc->GetUCode(header);
		if (header.MajorVersion < 6) {
			Read(decl, ucode);

		} else if (auto psv = dxbc->GetBlob<dxil::PSV>()) {
			Read(decl, psv, header.ProgramType);

		} else if (auto dxil = dxbc->GetBlob<dx::DXBC::DXIL>()) {
			bitcode::Module mod;
			ISO_VERIFY(ReadDXILModule(mod, dxil));

			for (dxil::meta_entryPoint i : mod.GetMeta("dx.entryPoints")) {
				if (auto func = (const bitcode::Function*)i.function) {
					if (!name || func->name == name) {
						Read(decl, i, header.ProgramType);
						break;
					}
				} else {
					Read(decl, i, header.ProgramType);
				}
			}
		}
	}
	return decl;
}

//-----------------------------------------------------------------------------
//	DX12Assets
//-----------------------------------------------------------------------------

struct DX12Assets {
	enum {
		WT_NONE,
		WT_OFFSET,
		WT_BATCH,
		WT_MARKER,
		WT_CALLSTACK,
		WT_REPEAT,
		WT_OBJECT,
		WT_SHADER,
		WT_DESCRIPTOR,
	};


	struct MarkerRecord : interval<uint32> {
		string		str;
		uint32		colour:24, flags:8;
		MarkerRecord(string &&str, uint32 start, uint32 end, uint32 colour) : interval<uint32>(start, end), str(move(str)), colour(colour), flags(0) {}
	};

	struct CallRecord {
		uint32		srce, dest, index;
		CallRecord(uint32 srce, uint32 dest, uint32 index) : srce(srce), dest(dest), index(index) {}
		bool operator<(uint32 offset) const { return dest < offset; }
	};

	struct ShaderRecord : DX12Shader {
		string				name;
		ShaderRecord(dx::SHADERSTAGE stage, malloc_block&& data, uint64 addr) : DX12Shader(stage, data.detach(), addr) {}
		ShaderRecord(ShaderRecord &&b) : DX12Shader(b), name(move(b.name)) { b.data = none; }
		string				GetName()		const	{ return name; }
		uint64				GetBase()		const	{ return addr; }
		uint64				GetSize()		const	{ return data.length(); }
	};

	struct ObjectRecord : RecObject2 {
		enum USED : uint8 {
			USED_UNUSED		= 0,
			USED_READ		= 1,
			USED_WRITE		= 2,
			USED_WRITEFIRST	= 4,
			USED_CLEAN		= 0x80,
		};
		void				*aux	= nullptr;
		mutable USED		used	= USED_UNUSED;

		ObjectRecord(RecObject::TYPE type, string16 &&name, void *obj) : RecObject2(type, move(name), obj) {}
		string				GetName()		const	{ return str8(name); }
		uint64				GetBase()		const	{ return uint64(obj); }
		uint64				GetSize()		const	{ return info.length(); }

		void		Used(bool write) const {
			if (!used && write)
				used = USED_WRITEFIRST;
			used = USED(used | (write ? USED_WRITE : USED_READ));
		}
		bool		Used()			const	{ return !!used; }
		bool		NeedsRestore()	const	{ return !(used & USED_CLEAN) && (used & USED_READ) && !(used & USED_WRITEFIRST); }
		void		MarkClean()				{ used = USED(used | USED_CLEAN); }
		void		MarkDirty()				{ used = USED(used & ~USED_CLEAN); }

		friend string_accum& operator<<(string_accum &a, const ObjectRecord *obj) { return a << obj->name; }
	};

	struct ResourceStates2 {
		ResourceStates curr;
		ResourceStates init;
	};

	struct ResourceRecord {
		struct Event {
			enum TYPE { READ, WRITE, BARRIER };
			uint32	type:2, addr:30;
		};
		ObjectRecord*			obj;
		ResourceStates2			states;
		dynamic_array<Event>	events;

		const RecResource*	res()				const { return obj->info; }
		const D3D12_CLEAR_VALUE* clear_value()	const { return obj->info.size() >= sizeof(RecResourceClear) ? &((const RecResourceClear*)obj->info)->clear : nullptr; }
		string				GetName()			const { return str8(obj->name); }
		uint64				GetBase()			const { return res()->gpu; }
		uint64				GetSize()			const { return res()->data_size; }
		DESCRIPTOR			DefaultDescriptor() const { return DESCRIPTOR::make<DESCRIPTOR::SRV>(RESOURCE_DESC(*res()), (ID3D12Resource*)obj->obj); }
		ResourceRecord(ObjectRecord *obj = 0) : obj(obj) {}
	};

	struct DescriptorHeapRecord {
		ObjectRecord		*obj;
		DescriptorHeapRecord(ObjectRecord *obj) : obj(obj) {}
		const RecDescriptorHeap	*operator->()	const { return obj->info; }
		operator const RecDescriptorHeap*()		const { return obj->info; }
	};

	struct BatchInfo {
		RecCommandList::tag	op;
		union {
			struct {
				uint32			prim:8, instance_count:18;
				uint32			vertex_count;
				uint32			vertex_offset, instance_offset, index_offset;
			} draw;
			struct {
				uint32			dim_x, dim_y, dim_z;
			} compute;
			struct {
				uint32				command_count;
				const ObjectRecord	*signature;
				const ObjectRecord	*arguments;
				uint64				arguments_offset;
				const ObjectRecord	*counts;
				uint64				counts_offset;

				BatchInfo	GetCommand(const dx::cache_type &cache) const {
					auto	desc	= rmap_unique<RTM, D3D12_COMMAND_SIGNATURE_DESC>(signature->info);
					const RecResource *rr		= arguments->info;
					return {desc, cache(rr->gpu + arguments_offset)};
				}

			} indirect;
			struct {
				uint32			dim_x, dim_y, dim_z;
				D3D12_GPU_VIRTUAL_ADDRESS_RANGE				RayGenerationShaderRecord;
				D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	MissShaderTable;
				D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	HitGroupTable;
				D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	CallableShaderTable;
			} rays;
		};
		BatchInfo()	{}
		BatchInfo(const D3D12_COMMAND_SIGNATURE_DESC *desc, const void *args);
	};

	struct BatchRecord : BatchInfo {
		uint32		begin, end;
		uint32		use_offset;
		uint64		timestamp	= 0;
		D3D12_QUERY_DATA_PIPELINE_STATISTICS	stats;
		BatchRecord(const BatchInfo &info, uint32 begin, uint32 end, uint32 use_offset) : BatchInfo(info), begin(begin), end(end), use_offset(use_offset) {}	
		D3D_PRIMITIVE_TOPOLOGY	Prim()		const { return (D3D_PRIMITIVE_TOPOLOGY)draw.prim; }
		uint64					Duration()	const { return this[1].timestamp - timestamp; }
	};

	struct BoundDescriptorHeaps {
		const ObjectRecord	*cbv_srv_uav	= 0;
		const ObjectRecord	*sampler		= 0;

		const RecDescriptorHeap	*get(bool samp) const {
			if (auto obj = samp ? sampler : cbv_srv_uav)
				return obj->info;
			return nullptr;
		}
		void	set(bool samp, const ObjectRecord *obj) {
			(samp ? sampler : cbv_srv_uav) = obj;
		}
		void	clear() {
			cbv_srv_uav = sampler = 0;
		}
	};

	struct use {
		uint32 index:28, type:4; 
		RecObject::TYPE	Type() const { return RecObject::TYPE(type); }
		use(uint32 index, RecObject::TYPE type)		: index(index), type(type) { ISO_ASSERT(index < 0x80000000u); }
	};

	dynamic_array<MarkerRecord>				markers;
	dynamic_array<use>						uses;
	dynamic_array<BatchRecord>				batches;
	dynamic_array<CallRecord>				calls;

	dynamic_array<ObjectRecord>				objects;
	dynamic_array<ResourceRecord>			resources;
	dynamic_array<ShaderRecord>				shaders;
	dynamic_array<DescriptorHeapRecord>		descriptor_heaps;
	ObjectRecord*							device_object	= nullptr;

	hash_map<uint64, ObjectRecord*>			object_map;
	hash_set<D3D12_CPU_DESCRIPTOR_HANDLE>	descriptor_used;
	interval_tree<uint64, ObjectRecord*>	vram_tree;
	hash_map<uint32, BoundDescriptorHeaps>	bound_heaps;

	// Adds

	static void		Used(const ObjectRecord *obj, bool write = false) {
		if (obj)
			obj->Used(write);
	}

	void			AddUse(const ObjectRecord *e, bool write = false) {
		if (e) {
			auto	i = objects.index_of(e);
			if (write) {
				ISO_ASSERT(e->type == RecObject::Resource);
				uses.emplace_back(i, RecObject::ResourceWrite);
			} else {
				uses.emplace_back(i, e->type);
			}
			e->Used(write);
		}
	}
	void			AddUse(const ID3D12DeviceChild *e, bool write = false) {
		AddUse(FindObject((uint64)e), write);
	}
	void			AddUse(D3D12_GPU_VIRTUAL_ADDRESS addr, uint32 size, dx::cache_type &cache, memory_interface *mem) {
		if (auto obj = FindByGPUAddress(addr)) {
			AddUse(obj);
		} else if (mem) {
			cache(addr | GPU, size, mem);
		}
	}

	auto			AddUseGet(D3D12_GPU_VIRTUAL_ADDRESS addr, uint32 size, dx::cache_type &cache, memory_interface *mem) {
		if (auto obj = FindByGPUAddress(addr)) {
			AddUse(obj);
			if (size == 0) {
				const RecResource	*rec = obj->info;
				size = rec->gpu + rec->data_size - addr;
			}
		}
		return cache(addr | GPU, size, mem);
	}

	void			AddCall(uint32 addr, uint32 dest, uint32 index) {
		calls.emplace_back(addr, dest, index);
	}

	BatchRecord*	AddBatch(const BatchInfo &info, uint32 begin, uint32 last, uint32 end) {
		for (auto &u : uses.slice(batches.empty() ? 0 : batches.back().use_offset)) {
			if (u.type == RecObject::Resource || u.type == RecObject::ResourceWrite) {
				auto	rr	= (ResourceRecord*)objects[u.index].aux;
				rr->events.push_back({u.type == RecObject::Resource ? ResourceRecord::Event::READ : ResourceRecord::Event::WRITE, last});
			}
		}

		return &batches.emplace_back(info, begin, end, uses.size32());
	}

	int				AddMarker(string16 &&s, uint32 start, uint32 end, uint32 col) {
		markers.emplace_back(move(s), start, end, col);
		return markers.size32() - 1;
	}

	ShaderRecord*	AddShader(dx::SHADERSTAGE stage, malloc_block &&data, uint64 addr, string_ref name);
	ShaderRecord*	AddShader(dx::SHADERSTAGE stage, const D3D12_SHADER_BYTECODE &b, memory_interface *mem);
	ObjectRecord*	AddObject(RecObject::TYPE type, string16 &&name, malloc_block &&info, void *obj, memory_interface *mem);

	void	FinishLoading() {
		for (auto &r : resources)
			r.obj->aux = &r;
	}

	//	Gets

	const BatchRecord*	GetBatchByTime(uint64 timestamp) const {
		return first_not(batches, [timestamp](const BatchRecord &r) { return r.timestamp < timestamp; });
	}
	const BatchRecord*	GetBatch(uint32 addr) const {
		return first_not(batches, [addr](const BatchRecord &r) { return r.end < addr; });
	}
	const MarkerRecord*	GetMarker(uint32 addr) const {
		return first_not(markers, [addr](const MarkerRecord &r) { return r.a < addr; });
	}

	range<use*>			GetUsage(uint32 from_batch, uint32 to_batch) const {
		if (from_batch > to_batch)
			swap(from_batch, to_batch);
		return make_range(uses + batches[max(from_batch, 1) - 1].use_offset, uses + batches[to_batch].use_offset);
	}
	range<use*>			GetUsage(uint32 batch) const {
		return GetUsage(batch, batch);
	}
	range<use*>			GetUsage(const BatchRecord *batch) const {
		return make_range(uses + (batch == batches.begin() ? 0 : batch[-1].use_offset), uses + batch->use_offset);
	}

	void				GetUsedAt(BatchListArray &usedat, RecObject::TYPE type) const {
		for (uint32 b = 0, nb = batches.size32() - 1; b < nb; b++) {
			auto	usage = GetUsage(b);
			for (const DX12Assets::use *u = usage.begin(), *ue = usage.end(); u != ue; ++u) {
				if (u->type == type)
					usedat[u->index].push_back(b);
			}
		}
	}

	const CallRecord*	FindCallTo(uint32 dest) const {
		auto i = lower_boundc(calls, dest, [](const CallRecord &c, uint32 dest) { return c.dest < dest; });
		return i == calls.begin() ? nullptr : i - 1;
	}

	const ObjectRecord*	FindObject(uint64 addr) const {
		if (auto *p = object_map.check(addr))
			return *p;
		return 0;
	}
	ShaderRecord*		FindShader(uint64 addr) {
		for (auto &i : shaders) {
			if (i.GetBase() == addr)
				return &i;
		}
		return 0;
	}
	ObjectRecord*		FindByGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS a) const {
		auto	i = lower_boundc(vram_tree, a);
		return i != vram_tree.end() && a >= i.key().a ? *i : 0;
	}
	const ObjectRecord*	FindHeapByGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS a) const {
		for (auto &i : objects) {
			if (i.type == RecObject::Heap) {
				const RecHeap	*r = i.info;
				if (r->contains(a))
					return &i;
			}
		}
		return nullptr;
	}

	template<typename T> const DESCRIPTOR*	FindDescriptor(T h, ObjectRecord **obj = nullptr) const {
		for (auto &i : descriptor_heaps) {
			if (const DESCRIPTOR *d = i->holds(h)) {
				if (obj)
					*obj = i.obj;
				return d;
			}
		}
		return nullptr;
	}
	template<typename T> const ObjectRecord* FindDescriptorHeapObject(T h) const {
		for (auto &i : descriptor_heaps) {
			if (i->holds(h))
				return i.obj;
		}
		return nullptr;
	}
	template<typename T> const RecDescriptorHeap* FindDescriptorHeap(T h) const {
		if (auto obj = FindDescriptorHeapObject(h))
			return obj->info;
		return nullptr;
	}

	// commands

	const_memory_block	GetCommands(const RecCommandList *rec, const interval<uint32> &r) const {
		return rec->buffer.slice(r.begin(), r.extent());
	}
	const_memory_block	GetCommands(const CommandRange &r) const {
		if (!r.empty()) {
			if (auto *obj = FindObject(uint64(&*r)))
				return GetCommands(obj->info, r);
		}
		return empty;
	}
	const_memory_block	GetCommands(const CallRecord &c) const {
		if (!device_object)
			return none;
		const COMRecording::header *p	= device_object->info + c.srce;
		const CommandRange	&pr			= COMParse(p->data(), [&](UINT NumCommandLists, counted<CommandRange,0> pp) {
			return pp[c.index];
		});
		return GetCommands(pr);
	}
	const_memory_block	GetCommands(uint32 addr) const {
		if (auto call = FindCallTo(addr + 1))
			return GetCommands(*call).slice(addr - call->dest);
		return device_object ? device_object->info + addr : none;
	}
	const_memory_block	GetCommands(const BatchRecord &b) const {
		return GetCommands(b.begin).slice_to(b.end - b.begin);
	}

	static const COMRecording::header *FindOp(const_memory_block com, RecCommandList::tag op);

	uint32				LastOp(const BatchRecord& b) const {
		auto	com	= GetCommands(b);
		return ((const char*)FindOp(com, b.op) - com) + b.begin;
	}

	// descriptions

	void operator()(string_accum &sa, const field *pf, const uint32le *p, uint32 offset);
	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr);

	string_accum& WriteDesc(string_accum &a, const DESCRIPTOR *d, const DescriptorHeapRecord &heap) const {
		a << heap.obj->name << '[' << heap->index(d) << "]";
		if (auto obj = FindObject((uint64)d->res))
			a << "=" << obj->name;
		return a;
	}

	auto Description(const DESCRIPTOR *d) const {
		return [=](string_accum &a) {
			if (d) {
				for (auto &i : descriptor_heaps) {
					if (i->holds(d)) {
						WriteDesc(a, d, i);
						break;
					}
				}
			}
		};
	}

	auto ObjectName(const DESCRIPTOR *d) const {
		return [=](string_accum &a) {
			if (d) {
				if (auto obj = FindObject((uint64)d->res))
					a << " (" << obj->name << ')';
			}
		};
	}

	auto ObjectName(D3D12_GPU_DESCRIPTOR_HANDLE h) const {
		return [=](string_accum &a) {
			for (auto &i : descriptor_heaps) {
				if (const DESCRIPTOR *d = i->holds(h)) {
					WriteDesc(a << " (", d, i) << ')';
					break;
				}
			}
		};
	}

	auto ObjectName(D3D12_CPU_DESCRIPTOR_HANDLE h) const {
		return [=](string_accum &a) {
			for (auto &i : descriptor_heaps) {
				if (const DESCRIPTOR *d = i->holds(h)) {
					WriteDesc(a << " (", d, i) << ')';
					break;
				}
			}
		};

	}

	auto ObjectName(D3D12_GPU_VIRTUAL_ADDRESS gpu) const {
		return [=](string_accum &a) {
			if (auto obj = FindByGPUAddress(gpu)) {
				RecResource	*r	= obj->info;
				a << " (" << obj->name;
				if (auto offset = gpu - r->gpu)
					a << " + 0x" << hex(offset);
				a << ')';
			}
		};
	}
};

DX12Assets::ShaderRecord* DX12Assets::AddShader(dx::SHADERSTAGE stage, malloc_block &&data, uint64 addr, string_ref name) {
	if (data) {
		const dx::DXBC*	dxbc = data;
		ISO_ASSERT(dxbc->valid(data.size()));

		auto&	r	= shaders.emplace_back(stage, move(data), addr);
		if (name) {
			r.name		= name;

		} else if (auto spdb = dxbc->GetBlob(dx::DXBC::ShaderPDB)) {
			r.name = GetPDBFirstFilename(memory_reader(spdb));

		} else if (auto dxil = dxbc->GetBlob<dx::DXBC::DXIL>()) {
			r.name = GetDXILShaderName(dxil, addr);

		} else {
			r.name = (format_string("shader_") << hex(addr)).term();

		}
		return &r;
	}
	return nullptr;
}

DX12Assets::ShaderRecord* DX12Assets::AddShader(dx::SHADERSTAGE stage, const D3D12_SHADER_BYTECODE &b, memory_interface *mem) {
	if (!b.pShaderBytecode || !b.BytecodeLength)
		return nullptr;

	ShaderRecord *r = FindShader((uint64)b.pShaderBytecode);
	if (!r)
		r = AddShader(stage, mem ? mem->get((uint64)b.pShaderBytecode, b.BytecodeLength) : none, (uint64)b.pShaderBytecode, none);
	return r;
}

DX12Assets::ObjectRecord* DX12Assets::AddObject(RecObject::TYPE type, string16 &&name, malloc_block &&info, void *obj, memory_interface *mem) {
	ObjectRecord	*p	= &objects.emplace_back(type, move(name), obj);
	p->info	= move(info);
	object_map[uint64(obj)] = p;

	switch (undead(p->type)) {
		case RecObject::Device:
			device_object	= p;
			break;

		case RecObject::GraphicsCommandList:
			if (mem) {
				RecCommandList	*r	= p->info;
				auto	ext = p->info.extend(r->buffer.length());
				r	= p->info;
				mem->get(ext, uint64(r->buffer.p));
				r->buffer.p = ext;
			} else {
				RecCommandList	*r	= p->info;
				r->buffer.p = r + 1;
			}
			break;

		case RecObject::Resource: {
			RecResource	*r	= p->info;
			switch (r->alloc) {
				case RecResource::UnknownAlloc:
					break;

				case RecResource::Reserved: {
					uint32	ntiles = 1;
					uint64	addr	= (uint64)r->mapping;
					r->mapping		= new Tiler::Mapping[ntiles];
					if (mem)
						mem->get(r->mapping, ntiles * sizeof(Tiler::Mapping), addr);
					break;
				}

				default:
					vram_tree.insert({r->gpu, r->gpu + r->data_size}, p);
					break;
			}

			resources.emplace_back(p);
			break;
		}

		case RecObject::DescriptorHeap: {
			descriptor_heaps.push_back(p);
			break;
		}

		default:
			break;
	}
	return p;
}

void DX12Assets::operator()(string_accum &sa, const field *pf, const uint32le *p, uint32 offset) {
	uint64		v = pf->get_raw_value(p, offset);
	if (pf->is_type<D3D12_GPU_VIRTUAL_ADDRESS2>()) {
		sa << "0x" << hex(v) << ObjectName(v);

	} else if (pf->is_type<D3D12_GPU_DESCRIPTOR_HANDLE>()) {
		sa << "0x" << hex(v) << ObjectName(D3D12_GPU_DESCRIPTOR_HANDLE{v});

	} else if (pf->is_type<D3D12_CPU_DESCRIPTOR_HANDLE>()) {
		D3D12_CPU_DESCRIPTOR_HANDLE h;
		h.ptr = v;
		sa << "0x" << hex(v) << ObjectName(h);

	} else if (v == 0) {
		sa << "nil";
		
	} else if (auto* rec = FindObject(v)) {
		sa << rec->name;

	} else {
		sa << "(unfound)0x" << hex(v);
	}
}

void DX12Assets::operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
	buffer_accum<256>	ba;
	PutFieldName(ba, tree.format, pf);

	uint64		v = pf->get_raw_value(p, offset);

	if (pf->is_type<D3D12_GPU_VIRTUAL_ADDRESS2>()) {
		ba << "0x" << hex(v) << ObjectName(v);

	} else if (pf->is_type<D3D12_GPU_DESCRIPTOR_HANDLE>()) {
		D3D12_GPU_DESCRIPTOR_HANDLE	gpu{v};
		ba << "0x" << hex(v) << ObjectName(gpu);
		
		if (auto heap = FindDescriptorHeap(gpu)) {
			auto	cpu = heap->get_cpu(heap->index(gpu));
			tree.Add(h, ba, WT_DESCRIPTOR, cpu);
			return;
		}

	} else if (pf->is_type<D3D12_CPU_DESCRIPTOR_HANDLE>()) {
		D3D12_CPU_DESCRIPTOR_HANDLE	cpu{v};
		ba << "0x" << hex(v) << ObjectName(cpu);
		tree.Add(h, ba, WT_DESCRIPTOR, cpu);
		return;

	} else if (v == 0) {
		ba << "nil";

	} else if (auto *rec = FindObject(v)) {
		tree.Add(h, ba << rec->name, WT_OBJECT, rec);
		return;

	} else if (auto *rec = FindShader(v)) {
		tree.Add(h, ba << rec->name, WT_SHADER, rec);
		return;

	} else {
		ba << "(unfound)0x" << hex(v);
	}
	tree.AddText(h, ba, addr);
}

const COMRecording::header* DX12Assets::FindOp(const_memory_block com, RecCommandList::tag op) {
	const COMRecording::header* p = com, *stack = nullptr;
	while (p < com.end()) {
		if (p->id == 0xfffe) {
			stack = p;
		} else {
			if (p->id == op)
				break;
			stack = nullptr;
		}
		p = p->next();
	}
	if (stack)
		return stack;
	return p < com.end() ? p : nullptr;
}

template<typename T> uint32 GetSize(const T &t)		{ return t.GetSize(); }

template<> const char *field_names<RecResource::Allocation>::s[]	= {
	"Unknown", "Reserved", "Placed", "Committed(default)", "Committed(upload)", "Committed(readback)",
	"Custom", "Custom, NoCPU", "Custom, WriteCombine", "Custom, WriteBack",
};

template<> field	fields<RecResource>::f[] = {
	field::make<RecResource>("gpu",		&RecResource::gpu),
	field::make<RecResource>("size",	&RecResource::data_size),
	field::make<RecResource>("alloc",	&RecResource::alloc),
	//field::call<D3D12_RESOURCE_DESC>(0, 0),
	field::make<D3D12_RESOURCE_DESC>(0, 0),
	0,
};

template<> field	fields<RecHeap>::f[] = {
	field::make<RecHeap>("gpu",	&RecHeap::gpu),
	field::make<D3D12_HEAP_DESC>(0, 0),
	0,
};

struct RecPipelineState;
template<> field	fields<RecPipelineState>::f[] = {
	field::make<int>("type",	0),
	field::call_union<
		D3D12_GRAPHICS_PIPELINE_STATE_DESC,
		D3D12_COMPUTE_PIPELINE_STATE_DESC,
		map_t<KM, D3D12_PIPELINE_STATE_STREAM_DESC>
	>(0, 64, 1),
	0,
};

struct RecRootSignature {
	struct field_thing2 : field_thing {
		com_ptr<ID3D12RootSignatureDeserializer>	deserializer;
		field_thing2(const dx::DXBC* dxbc) {
			D3D12CreateRootSignatureDeserializer(dxbc, dxbc->size, deserializer.uuid(), (void**)&deserializer);
			const D3D12_ROOT_SIGNATURE_DESC	*desc = deserializer->GetRootSignatureDesc();
			p	= (uint32*)desc;
			pf	= fields<D3D12_ROOT_SIGNATURE_DESC>::f;
		}
	};
	static field_thing* f(const field *pf, const uint32le* p, uint32 offset) {
		return new field_thing2((dx::DXBC*)p);
	}
};

template<> field	fields<RecRootSignature>::f[] = {
	{"deserializer", 0, 0, field::MODE_CUSTOM_PTR, 0, (char**)&RecRootSignature::f},
	0,
};

MAKE_FIELDS(RecDescriptorHeap, type, count, stride, cpu, gpu);
MAKE_FIELDS(RecCommandList, node_mask, flags, list_type, allocator, state);

DECLARE_VALUE_FIELD(D3D12_COMMAND_LIST_TYPE);

using RecStateObject = tuple<D3D12_STATE_OBJECT_DESC, SHADER_IDS>;

template<> const char *field_names<RecObject::TYPE>::s[]	= {
	"Unknown",
	"RootSignature",
	"Heap",
	"Resource",
	"CommandAllocator",
	"Fence",
	"PipelineState",
	"DescriptorHeap",
	"QueryHeap",
	"CommandSignature",
	"GraphicsCommandList",
	"CommandQueue",
	"Device",
	"HANDLE",
	"StateObject",
	"LifetimeOwner",
	"LifetimeTracker",
	"MetaCommand",
	"PipelineLibrary",
	"ProtectedResourceSession",
};

template<> field	fields<DX12Assets::ObjectRecord>::f[] = {
	field::make("type",	container_cast<DX12Assets::ObjectRecord>(&DX12Assets::ObjectRecord::type)),
	//field::make2<DX12Assets::ObjectRecord>("obj",	&DX12Assets::ObjectRecord::obj),
	field::make("obj",	element_cast<uint64>(container_cast<DX12Assets::ObjectRecord>(&DX12Assets::ObjectRecord::obj))),
	field::call_union<
		void,										// Unknown,
		RecRootSignature*,							// RootSignature,
		RecHeap*,									// Heap,
		RecResource*,								// Resource,
		D3D12_COMMAND_LIST_TYPE*,					// CommandAllocator,
		void,										// Fence,
		RecPipelineState*,							// PipelineState,
		RecDescriptorHeap*,							// DescriptorHeap,
		D3D12_QUERY_HEAP_DESC*,						// QueryHeap,
		map_t<KM, tuple<D3D12_COMMAND_SIGNATURE_DESC, ID3D12RootSignature*>>*,	// CommandSignature,
		RecCommandList*,							// GraphicsCommandList,
		D3D12_COMMAND_QUEUE_DESC*,					// CommandQueue,
		void,										// Device,
		void,										// HANDLE,
		map_t<KM, D3D12_STATE_OBJECT_DESC>*,		// StateObject,
		void,										// LifetimeOwner,
		void,										// LifetimeTracker,
		void,										// MetaCommand,
		void,										// PipelineLibrary,
		void										// ProtectedResourceSession,
	>(0, T_get_member_offset(&DX12Assets::ObjectRecord::info) * 8, 2),
	0,
};

template<> field	fields<dx12::ResourceStates>::f[] = {
	field::make<dx12::ResourceStates>("state",	&dx12::ResourceStates::state),
	0
};

template<> field	fields<DX12Assets::ResourceRecord>::f[] = {
	field::make<DX12Assets::ResourceRecord>(0,	member_chain(&DX12Assets::ResourceRecord::states, &DX12Assets::ResourceStates2::init)),
	field::make<DX12Assets::ResourceRecord>(0,	&DX12Assets::ResourceRecord::obj),
	0,
};

MAKE_FIELDS(DX12Assets::ShaderRecord, addr, stage);

auto IndirectFields = union_fields<
	D3D12_DRAW_ARGUMENTS,
	D3D12_DRAW_INDEXED_ARGUMENTS,
	D3D12_DISPATCH_ARGUMENTS,
	D3D12_VERTEX_BUFFER_VIEW,
	D3D12_INDEX_BUFFER_VIEW,
	uint64,
	D3D12_CONSTANT_BUFFER_VIEW_DESC,
	D3D12_SHADER_RESOURCE_VIEW_DESC,
	D3D12_UNORDERED_ACCESS_VIEW_DESC,
	D3D12_DISPATCH_RAYS_DESC,
	D3D12_DISPATCH_MESH_ARGUMENTS
>::p;

uint8 IndirectSizes[] = {
	(uint8)sizeof(D3D12_DRAW_ARGUMENTS),
	(uint8)sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
	(uint8)sizeof(D3D12_DISPATCH_ARGUMENTS),
	(uint8)sizeof(D3D12_VERTEX_BUFFER_VIEW),
	(uint8)sizeof(D3D12_INDEX_BUFFER_VIEW),
	(uint8)sizeof(uint64),
	(uint8)sizeof(D3D12_CONSTANT_BUFFER_VIEW_DESC),
	(uint8)sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC),
	(uint8)sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC),
	(uint8)sizeof(D3D12_DISPATCH_RAYS_DESC),
	(uint8)sizeof(D3D12_DISPATCH_MESH_ARGUMENTS)
};

bool IsGraphics(const D3D12_COMMAND_SIGNATURE_DESC *desc) {
	for (auto &i : make_range_n(desc->pArgumentDescs, desc->NumArgumentDescs)) {
		switch (i.Type) {
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
				return true;
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
				return false;
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
				return true;
		}
	}
	return false;
}

DX12Assets::BatchInfo::BatchInfo(const D3D12_COMMAND_SIGNATURE_DESC *desc, const void *args) {
	clear(*this);
	uint32	offset = 0;
	for (auto &i : make_range_n(desc->pArgumentDescs, desc->NumArgumentDescs)) {
		switch (i.Type) {
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW: {
				auto p	= (const D3D12_DRAW_ARGUMENTS*)((const char*)args + offset);
				op		= RecCommandList::tag_DrawInstanced;
				draw.instance_count		= p->InstanceCount;
				draw.vertex_count		= p->VertexCountPerInstance;
				draw.vertex_offset		= p->StartInstanceLocation;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED: {
				auto p	= (const D3D12_DRAW_INDEXED_ARGUMENTS*)((const char*)args + offset);
				op		= RecCommandList::tag_DrawIndexedInstanced;
				draw.instance_count		= p->InstanceCount;
				draw.vertex_count		= p->IndexCountPerInstance;
				draw.vertex_offset		= p->StartInstanceLocation;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH: {
				auto p	= (const D3D12_DISPATCH_ARGUMENTS*)((const char*)args + offset);
				op		= RecCommandList::tag_Dispatch;
				compute.dim_x			= p->ThreadGroupCountX;
				compute.dim_y			= p->ThreadGroupCountY;
				compute.dim_z			= p->ThreadGroupCountZ;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:{
				auto p = (const D3D12_DISPATCH_RAYS_DESC*)((const char*)args + offset);
				op	= RecCommandList::tag_DispatchRays;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH: {
				auto p	= (const D3D12_DISPATCH_MESH_ARGUMENTS*)((const char*)args + offset);
				op		= RecCommandList::tag_DispatchMesh;
				compute.dim_x			= p->ThreadGroupCountX;
				compute.dim_y			= p->ThreadGroupCountY;
				compute.dim_z			= p->ThreadGroupCountZ;
				break;
			}
		}
		offset += IndirectSizes[i.Type];
	}
}

//-----------------------------------------------------------------------------
//	DX12State
//-----------------------------------------------------------------------------

struct DX12RootState {
	struct Slot {
		struct Slot2 {
			arbitrary_ptr	data;
			Slot2(void *p) : data(p) {}
			auto	operator&()								const	{ return data; }
			operator uint32*()										{ return data; }
			operator const uint32*()						const	{ return data; }
			operator const D3D12_GPU_DESCRIPTOR_HANDLE()	const	{ return *data; }
			operator const D3D12_GPU_VIRTUAL_ADDRESS()		const	{ return *data; }
			operator const D3D12_GPU_VIRTUAL_ADDRESS2()		const	{ return *data; }
			void operator=(D3D12_GPU_DESCRIPTOR_HANDLE h)			{ *data = h; }
			void operator=(D3D12_GPU_VIRTUAL_ADDRESS h)				{ *data = h; }
		};

		enum TYPE {
			TABLE_CBV_SRV_UAV,
			TABLE_SMP,
			CONSTS,
			DIRECT_CBV,
			DIRECT_SRV,
			DIRECT_UAV,
		};
		TYPE		type;
		uint32		offset;
		Slot2		get(const void *p) const { return (uint8*)p + offset; }
	};

	com_ptr<ID3D12RootSignatureDeserializer>	deserializer;
	dynamic_array<Slot>			slot;

	uint32				SetRootSignature(const memory_block data);
	const D3D12_ROOT_SIGNATURE_DESC	*GetRootSignatureDesc() const {
		return deserializer ? deserializer->GetRootSignatureDesc() : nullptr;
	}
	DESCRIPTOR			GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const RecDescriptorHeap *dh, const void *root_data) const;
};

uint32 DX12RootState::SetRootSignature(const memory_block data) {
	deserializer.clear();
	D3D12CreateRootSignatureDeserializer(data, data.size(), deserializer.uuid(), (void**)&deserializer);

	const D3D12_ROOT_SIGNATURE_DESC	*root_desc = GetRootSignatureDesc();
	slot.resize(root_desc->NumParameters);

	uint32	out	= 0;
	for (int i = 0; i < root_desc->NumParameters; i++) {
		const D3D12_ROOT_PARAMETER	&p = root_desc->pParameters[i];
		auto	&s	= slot[i];
		s.offset	= out;

		switch (p.ParameterType) {
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				s.type = p.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? Slot::TABLE_SMP : Slot::TABLE_CBV_SRV_UAV;
				break;

			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				s.type = Slot::CONSTS;
				out += p.Constants.Num32BitValues * sizeof(uint32);
				continue;

			case D3D12_ROOT_PARAMETER_TYPE_CBV:
				s.type = Slot::DIRECT_CBV; 
				break;

			case D3D12_ROOT_PARAMETER_TYPE_SRV:
				s.type = Slot::DIRECT_SRV;
				break;

			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				s.type = Slot::DIRECT_UAV;
				break;
		}
		out += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);
	}
	return out;
}

DESCRIPTOR DX12RootState::GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const RecDescriptorHeap *dh, const void *root_data) const {
	static const uint8	visibility[] = {
		0xff,
		1 << dx::VS,
		1 << dx::HS,
		1 << dx::DS,
		1 << dx::GS,
		1 << dx::PS,
		1 << dx::AS,
		1 << dx::MS,
	};

	uint8	stage_mask	= stage == dx::SHADER_LIB ? 0xff : (1 << stage);

	if (const D3D12_ROOT_SIGNATURE_DESC	*root_desc = GetRootSignatureDesc()) {

		if (type == DESCRIPTOR::SMP) {
			for (int i = 0; i < root_desc->NumStaticSamplers; i++) {
				const D3D12_STATIC_SAMPLER_DESC	&p = root_desc->pStaticSamplers[i];
				if (visibility[p.ShaderVisibility] & stage_mask && p.ShaderRegister == bind && p.RegisterSpace == space)
					return p;
			}
		}

		for (int i = 0; i < root_desc->NumParameters; i++) {
			const D3D12_ROOT_PARAMETER	&p = root_desc->pParameters[i];
			if (visibility[p.ShaderVisibility] & stage_mask) {

				switch (p.ParameterType) {
					case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
						if (dh) {
							D3D12_GPU_DESCRIPTOR_HANDLE	s		= slot[i].get(root_data);
							uint32						start	= 0;

							for (int j = 0; j < p.DescriptorTable.NumDescriptorRanges; j++) {
								const D3D12_DESCRIPTOR_RANGE	&range = p.DescriptorTable.pDescriptorRanges[j];

								if (range.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
									start = range.OffsetInDescriptorsFromTableStart;

								if (range.RangeType == as<D3D12_DESCRIPTOR_RANGE_TYPE>(type) && bind >= range.BaseShaderRegister && bind < range.BaseShaderRegister + range.NumDescriptors) {
									if (const DESCRIPTOR *d = dh->holds(s)) {
										if (type == DESCRIPTOR::SMP)
											return *d;

										auto	index	= dh->index(d) + bind - range.BaseShaderRegister + start;
										return DESCRIPTOR::make<DESCRIPTOR::DESCH>(dh->get_gpu(index));
									}
								}
								start += range.NumDescriptors;
							}
						}
						break;

					case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
						if (type == DESCRIPTOR::CBV && p.Descriptor.ShaderRegister == bind)
							return DESCRIPTOR::make<DESCRIPTOR::IMM>(slot[i].get(root_data));
						break;

					case D3D12_ROOT_PARAMETER_TYPE_CBV:
						if (type == DESCRIPTOR::CBV && p.Descriptor.ShaderRegister == bind)
							return DESCRIPTOR::make<DESCRIPTOR::PCBV>(slot[i].get(root_data));
						break;

					case D3D12_ROOT_PARAMETER_TYPE_SRV:
						if (type == DESCRIPTOR::SRV && p.Descriptor.ShaderRegister == bind)
							return DESCRIPTOR::make<DESCRIPTOR::PSRV>(slot[i].get(root_data));
						break;

					case D3D12_ROOT_PARAMETER_TYPE_UAV:
						if (type == DESCRIPTOR::UAV && p.Descriptor.ShaderRegister == bind)
							return DESCRIPTOR::make<DESCRIPTOR::PUAV>(slot[i].get(root_data));
						break;
				}
			}
		}
	}
	return DESCRIPTOR();
}

struct DX12PipelineBase : DX12RootState {
	const DX12Assets::ObjectRecord	*obj		= 0;
	UINT							NodeMask	= 0;
	D3D12_CACHED_PIPELINE_STATE		CachedPSO	= {0, 0};
	D3D12_PIPELINE_STATE_FLAGS		Flags		= D3D12_PIPELINE_STATE_FLAG_NONE;
	ID3D12RootSignature				*root_signature	= 0;
	malloc_block					root_data;

	void	Clear() {
		NodeMask	= 0;
		CachedPSO.CachedBlobSizeInBytes	= 0;
		CachedPSO.pCachedBlob			= 0;
		Flags		= D3D12_PIPELINE_STATE_FLAG_NONE;
	}
	
	void	SetRootSignature(DX12Assets *assets, ID3D12RootSignature *p) {
		if (p == root_signature)
			return;

		root_signature	= p;

		if (auto *obj2 = assets->FindObject(uint64(p)))
			root_data.resize(DX12RootState::SetRootSignature(obj2->info));
	}

	auto	GetSlot(int i)	const { return slot[i].get(root_data); }

	DESCRIPTOR GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const RecDescriptorHeap *dh) const {
		return DX12RootState::GetBound(stage, type, bind, space, dh, root_data);
	}
	DESCRIPTOR GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const RecDescriptorHeap *dh, const DX12RootState *local, const void *local_data) const {
		DESCRIPTOR	desc	= GetBound(stage, type, bind, space, dh);
		if (desc.type == DESCRIPTOR::NONE && local)
			desc = local->GetBound(stage, type, bind, space, dh, local_data);
		return desc;
	}

	constexpr operator const void*() const { return obj->info; }//CachedPSO.pCachedBlob; }
};

MAKE_FIELDS(DX12PipelineBase, NodeMask, Flags);

struct DX12ComputePipeline : DX12PipelineBase {
	D3D12_SHADER_BYTECODE		CS = {0, 0};
	unique_ptr<RecStateObject>	state_object;

	constexpr auto	StateSubObjects() const { return ((STATE_OBJECT_DESC0&)state_object->get<0>()).SubObjects(); }
	DX12Assets::ShaderRecord*	FindShader(DX12Assets *assets, const string_param &name) const;
	DX12Assets::ShaderRecord*	FindShader(DX12Assets *assets, const SHADER_ID& id, string_param &name) const;
	const D3D12_HIT_GROUP_DESC*	FindHitGroup(const SHADER_ID& id) const;
	ID3D12RootSignature*		LocalPipeline(const string_param16 &name) const;

	void	Set(const DX12Assets::ObjectRecord *obj, DX12Assets *assets, const D3D12_COMPUTE_PIPELINE_STATE_DESC *d) {
		this->obj	= obj;
		if (!root_signature) 
			SetRootSignature(assets, d->pRootSignature);
		CS			= d->CS;
		NodeMask	= d->NodeMask;
		CachedPSO	= d->CachedPSO;
		Flags		= d->Flags;
	}

	void Set(const DX12Assets::ObjectRecord *obj, DX12Assets *assets, const D3D12_PIPELINE_STATE_STREAM_DESC *d) {
		DX12PipelineBase::Clear();
		clear(CS);
		this->obj	= obj;
		for (auto &sub : make_next_range<dx12::PIPELINE::SUBOBJECT>(const_memory_block(d->pPipelineStateSubobjectStream, d->SizeInBytes))) {
			switch (sub.t.u.t) {
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:	if (!root_signature) SetRootSignature(assets, get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>(sub)); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:				CS			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:			NodeMask	= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:		CachedPSO	= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:				Flags		= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS>(sub); break;
				default: ISO_ASSERT(0);
			}
		}
	}

	void Set(const DX12Assets::ObjectRecord *obj, DX12Assets *assets, unique_ptr<RecStateObject> &&p) {
		state_object	= move(p);
		for (auto &sub : StateSubObjects()) {
			switch (sub.Type) {
				case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:	SetRootSignature(assets, get<D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE>(sub)->pGlobalRootSignature); break;
				case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:				NodeMask	= get<D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK>(sub)->NodeMask; break;
			}
		}
	}
};

DX12Assets::ShaderRecord* DX12ComputePipeline::FindShader(DX12Assets *assets, const string_param &name) const {
	for (auto &sub : StateSubObjects()) {
		if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY) {
			auto	lib		= get<D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY>(sub);
			if (lib->NumExports) {
				for (auto &i : lib->Exports()) {
					if (i.Name == name)
						return assets->FindShader((uint64)lib->DXILLibrary.pShaderBytecode);
				}

			} else {
				auto	rec	= assets->FindShader((uint64)lib->DXILLibrary.pShaderBytecode);
				if (auto dxbc = rec->DXBC()) {
					if (auto rdat = dxbc->GetBlob<dx::DXBC::RuntimeData>()) {
						const_memory_block	strings;
						for (auto &i : rdat->tables()) {
							switch (i.type) {
								case rdat->StringBuffer:
									strings = i.raw();
									break;
								case rdat->FunctionTable:
									for (auto &j : i.table<dxil::RDAT_Function>()) {
										if (j.unmangled_name.get(strings) == name)
											return rec;
									}
									break;
							}

						}
					}
				}
			}
		}
	}
	return nullptr;
}

DX12Assets::ShaderRecord* DX12ComputePipeline::FindShader(DX12Assets *assets, const SHADER_ID& id, string_param &name) const {
	const auto&	hash	= state_object->get<1>();
	for (auto &sub : StateSubObjects()) {
		if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY) {
			auto	lib		= get<D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY>(sub);
			if (lib->NumExports) {
				for (auto &i : lib->Exports()) {
					auto	e	= hash[i.Name];
					if (auto p = e.exists_ptr()) {
						if (*p == id) {
							name	= i.Name;
							return assets->FindShader((uint64)lib->DXILLibrary.pShaderBytecode);
						}
					}
				}

			} else {
				auto	rec	= assets->FindShader((uint64)lib->DXILLibrary.pShaderBytecode);
				if (auto dxbc = rec->DXBC()) {
					if (auto rdat = dxbc->GetBlob<dx::DXBC::RuntimeData>()) {
						typedef noref_t<decltype(*rdat)>	RDAT;
						const_memory_block	strings;
						for (auto &i : rdat->tables()) {
							switch (i.type) {
								case rdat->StringBuffer:
									strings = i.raw();
									break;
								case rdat->FunctionTable:
									for (auto &j : i.table<dxil::RDAT_Function>()) {
										auto	unmangled = j.unmangled_name.get(strings);
										auto	e	= hash[str16(unmangled)];
										if (auto p = e.exists_ptr()) {
											if (*p == id) {
												name = unmangled;
												return rec;
											}
										}
									}
									break;
							}

						}
					}
				}
			}
		}
	}
	return nullptr;
}

const D3D12_HIT_GROUP_DESC* DX12ComputePipeline::FindHitGroup(const SHADER_ID& id) const {
	const auto&	hash	= state_object->get<1>();
	for (auto &sub : StateSubObjects()) {
		if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP) {
			auto hitgroup = get<D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP>(sub);
			if (auto p = hash[hitgroup->HitGroupExport].exists_ptr()) {
				if (*p == id)
					return hitgroup;
			}
		}
	}
	return nullptr;
}

ID3D12RootSignature* DX12ComputePipeline::LocalPipeline(const string_param16 &name) const {
	for (auto &sub : StateSubObjects()) {
		if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION) {
			auto assoc = get<D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION>(sub);
			if (assoc->pSubobjectToAssociate->Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE) {
				for (auto& i : make_range_n(assoc->pExports, assoc->NumExports)) {
					if (i == name)
						return get<D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE>(*assoc->pSubobjectToAssociate)->pLocalRootSignature;
				}
			}
		}
	}
	return nullptr;
}

struct _DX12GraphicsPipeline {
	struct STREAM_OUTPUT_DESC {
		dynamic_array<D3D12_SO_DECLARATION_ENTRY> declarations;
		dynamic_array<UINT> strides;
		UINT				RasterizedStream;
		void operator=(const D3D12_STREAM_OUTPUT_DESC &d) {
			declarations	= make_range_n(d.pSODeclaration, d.NumEntries);
			strides			= make_range_n(d.pBufferStrides, d.NumStrides);
		}
	};
	struct INPUT_LAYOUT_DESC {
		struct ELEMENT : D3D12_INPUT_ELEMENT_DESC {
			ELEMENT(const D3D12_INPUT_ELEMENT_DESC &d) : D3D12_INPUT_ELEMENT_DESC(d) { SemanticName = iso::strdup(SemanticName); }
			~ELEMENT() { iso::free(unconst(SemanticName)); }
		};
		dynamic_array<ELEMENT> elements;

		void	operator=(const D3D12_INPUT_LAYOUT_DESC &d) {
			elements	= make_range_n(d.pInputElementDescs, d.NumElements);
		}
		const C_type *GetVertexType(int slot) const {
			C_type_struct	comp;
			uint32			offset = 0;
			for (auto &i : elements) {
				if (i.InputSlot == slot) {
					if (i.AlignedByteOffset != D3D12_APPEND_ALIGNED_ELEMENT)
						offset = i.AlignedByteOffset;
					auto	type = dx::to_c_type(i.Format);
					comp.add_atoffset(string_builder(i.SemanticName) << onlyif(i.SemanticIndex, i.SemanticIndex), type, offset);
					offset += uint32(type->size());
				}
			}
			return ctypes.add(move(comp));
		}
	};
	struct VIEW_INSTANCING_DESC {
		dynamic_array<D3D12_VIEW_INSTANCE_LOCATION> locations;
		D3D12_VIEW_INSTANCING_FLAGS Flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
		void	operator=(const D3D12_VIEW_INSTANCING_DESC &d) {
			locations	= make_range_n(d.pViewInstanceLocations, d.ViewInstanceCount);
			Flags		= d.Flags;
		}
	};

	struct DEPTH_STENCIL_DESC : D3D12_DEPTH_STENCIL_DESC1 {
		void operator=(const D3D12_DEPTH_STENCIL_DESC &d) {
			D3D12_DEPTH_STENCIL_DESC1::operator=({
				d.DepthEnable,
				d.DepthWriteMask,
				d.DepthFunc,
				d.StencilEnable,
				d.StencilReadMask,
				d.StencilWriteMask,
				d.FrontFace,
				d.BackFace,
				true
				});
		}
		void operator=(const D3D12_DEPTH_STENCIL_DESC1 &d) {
			D3D12_DEPTH_STENCIL_DESC1::operator=(d);
		}
	};

	D3D12_SHADER_BYTECODE		shaders[7] = {{0,0}};

	STREAM_OUTPUT_DESC					StreamOutput;
	D3D12_BLEND_DESC					BlendState;
	UINT								SampleMask;
	D3D12_RASTERIZER_DESC				RasterizerState;
	DEPTH_STENCIL_DESC					DepthStencilState;
	INPUT_LAYOUT_DESC					InputLayout;
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE	IBStripCutValue;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE		PrimitiveTopologyType;
	D3D12_RT_FORMAT_ARRAY				RTVFormats;
	DXGI_FORMAT							DSVFormat;
	DXGI_SAMPLE_DESC					SampleDesc;
	VIEW_INSTANCING_DESC				ViewInstancing;

//	auto	&shader(int i) const { return (&VS)[i]; }
};

struct DX12GraphicsPipeline : DX12PipelineBase, _DX12GraphicsPipeline {

	void Set(const DX12Assets::ObjectRecord *obj, DX12Assets *assets, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *d) {
		this->obj	= obj;
		if (!root_signature)
			SetRootSignature(assets, d->pRootSignature);
		shaders[dx::VS]			= d->VS;
		shaders[dx::PS]			= d->PS;
		shaders[dx::DS]			= d->DS;
		shaders[dx::HS]			= d->HS;
		shaders[dx::GS]			= d->GS;
		StreamOutput			= d->StreamOutput;
		BlendState				= d->BlendState;
		SampleMask				= d->SampleMask;
		RasterizerState			= d->RasterizerState;
		DepthStencilState		= d->DepthStencilState;
		InputLayout				= d->InputLayout;
		IBStripCutValue			= d->IBStripCutValue;
		PrimitiveTopologyType	= d->PrimitiveTopologyType;
		raw_copy(d->RTVFormats, RTVFormats.RTFormats);
		RTVFormats.NumRenderTargets =d->NumRenderTargets;
		DSVFormat				= d->DSVFormat;
		SampleDesc				= d->SampleDesc;
		NodeMask				= d->NodeMask;
		CachedPSO				= d->CachedPSO;
		Flags					= d->Flags;
	}

	void Set(const DX12Assets::ObjectRecord *obj, DX12Assets *assets, const D3D12_PIPELINE_STATE_STREAM_DESC *d) {
		*(_DX12GraphicsPipeline*)this = _DX12GraphicsPipeline();
		this->obj	= obj;
		for (auto &sub : make_next_range<dx12::PIPELINE::SUBOBJECT>(const_memory_block(d->pPipelineStateSubobjectStream, d->SizeInBytes))) {
			switch (sub.t.u.t) {
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:		if (!root_signature) SetRootSignature(assets, get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>(sub)); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:					shaders[dx::VS]			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:					shaders[dx::PS]			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:					shaders[dx::DS]			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:					shaders[dx::HS]			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:					shaders[dx::GS]			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS>(sub); break;
				//case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:					CS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:			StreamOutput			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:					BlendState				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:			SampleMask				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:			RasterizerState			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:			DepthStencilState		= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:			InputLayout				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:	IBStripCutValue			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:	PrimitiveTopologyType	= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:	RTVFormats				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:	DSVFormat				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:			SampleDesc				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:				NodeMask				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:			CachedPSO				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:					Flags					= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:		DepthStencilState		= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:		ViewInstancing			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:					shaders[dx::AS]			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:					shaders[dx::MS]			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>(sub); break;
				default: ISO_ASSERT(0);
			}
		}
	}
};

struct DX12State : DX12Assets::BatchInfo {
	DX12GraphicsPipeline						graphics_pipeline;
	DX12ComputePipeline							compute_pipeline;
	DX12Assets::BoundDescriptorHeaps			bound_heaps;
	D3D12_INDEX_BUFFER_VIEW						ibv				= {0, 0, DXGI_FORMAT_UNKNOWN};
	dynamic_array<D3D12_VERTEX_BUFFER_VIEW>		vbv;
	D3D12_CPU_DESCRIPTOR_HANDLE					targets[8]		= {{0}};
	D3D12_CPU_DESCRIPTOR_HANDLE					depth			= {0};
	float4p										BlendFactor		= {0,0,0,0};
	uint32										StencilRef		= 0;
	D3D12_PRIMITIVE_TOPOLOGY					topology		= D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	hash_map<D3D12_CPU_DESCRIPTOR_HANDLE, DESCRIPTOR>	mod_descriptors;

	static_array<D3D12_VIEWPORT, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>	viewport;
	static_array<D3D12_RECT, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>		scissor;

	bool					IsGraphics()		const;

	const DESCRIPTOR*		GetDescriptor(DX12Assets* assets, D3D12_CPU_DESCRIPTOR_HANDLE h, DX12Assets::ObjectRecord **heap_obj = nullptr) const {
		if (heap_obj)
			*heap_obj = nullptr;
		if (mod_descriptors[h].exists())
			return &mod_descriptors[h];
		return assets->FindDescriptor(h, heap_obj);
	}

	const D3D12_VIEWPORT&	GetViewport(int i)	const { return viewport[i]; };
	const D3D12_RECT&		GetScissor(int i)	const { return scissor[i]; };
	Topology				GetTopology()		const { return dx::GetTopology(topology); }

	BackFaceCull GetCull(bool flipped) const {
		return graphics_pipeline.RasterizerState.CullMode == D3D12_CULL_MODE_NONE ? BFC_NONE
			: (graphics_pipeline.RasterizerState.CullMode == D3D12_CULL_MODE_FRONT) ^ flipped ^ graphics_pipeline.RasterizerState.FrontCounterClockwise
			? BFC_BACK : BFC_FRONT;
	}
	/*
	indices		GetIndexing(dx::cache_type &cache, const DX12Assets::BatchInfo &batch) const {
		if (batch.op == RecCommandList::tag_DrawIndexedInstanced) {
			int		size	= ByteSize(ibv.Format);
			return indices(cache(ibv.BufferLocation + batch.draw.index_offset * size, batch.draw.vertex_count * size), size, batch.draw.vertex_offset, batch.draw.vertex_count);
		}
		return indices(0, 0, batch.draw.vertex_offset, batch.draw.vertex_count);
	}

	indices		GetIndexing(dx::cache_type &cache)	const {
		return GetIndexing(cache, *this);
	}
	*/
	const DX12PipelineBase& GetPipeline(bool compute) const {
		return compute ? (const DX12PipelineBase&)compute_pipeline : (const DX12PipelineBase&)graphics_pipeline;
	}

	DESCRIPTOR	GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const DX12RootState *local = nullptr, const void *local_data = nullptr) const;

	void		SetPipelineState(DX12Assets *assets, ID3D12PipelineState *p);
	void		ProcessDevice(DX12Assets *assets, uint16 id, const uint16 *p);
	void		ProcessCommand(DX12Assets *assets, uint16 id, const uint16 *p);
};

bool DX12State::IsGraphics() const {
	return	op == RecCommandList::tag_ExecuteIndirect ? ::IsGraphics(rmap_unique<RTM, D3D12_COMMAND_SIGNATURE_DESC>(indirect.signature->info))
		:	op == RecCommandList::tag_DrawInstanced || op == RecCommandList::tag_DrawIndexedInstanced;
}

void DX12State::SetPipelineState(DX12Assets *assets, ID3D12PipelineState *p) {
	if (auto *obj = assets->FindObject(uint64(p))) {
		switch (*(int*)obj->info) {
			case 0:
				graphics_pipeline.Set(obj, assets, rmap_unique<KM, D3D12_GRAPHICS_PIPELINE_STATE_DESC>(obj->info + 8));
				break;

			case 1:
				compute_pipeline.Set(obj, assets, rmap_unique<KM, D3D12_COMPUTE_PIPELINE_STATE_DESC>(obj->info + 8));
				break;

			case 2: {
				auto	p		= rmap_unique<KM, D3D12_PIPELINE_STATE_STREAM_DESC>(obj->info + 8);
				bool	compute	= false;
				for (auto &sub : make_next_range<dx12::PIPELINE::SUBOBJECT>(const_memory_block(p->pPipelineStateSubobjectStream, p->SizeInBytes))) {
					if (compute = (sub.t.u.t == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS))
						break;
				}
				if (compute) {
					compute_pipeline.Set(obj, assets, p);
				} else {
					graphics_pipeline.Set(obj, assets, p);
				}
				break;
			}
		}
	}
}

DESCRIPTOR DX12State::GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const DX12RootState *local, const void *local_data) const {
	auto		dh		= bound_heaps.get(type == DESCRIPTOR::SMP);
	DESCRIPTOR	desc	= GetPipeline(stage == dx::CS || stage == dx::SHADER_LIB).GetBound(stage, type, bind, space, dh, local, local_data);
	if (desc.type == DESCRIPTOR::DESCH) {
		auto	mod		= mod_descriptors[dh->get_cpu(dh->index(desc.h))];
		if (mod.exists())
			return mod;

		return *dh->holds(desc.h);
	}
	return desc;
}


void DX12State::ProcessDevice(DX12Assets *assets, uint16 id, const uint16 *p) {
	switch (id) {
		case RecDevice::tag_CreateConstantBufferView: COMParse(p, [this](const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
			mod_descriptors[dest]->set(desc);
		}); break;

		case RecDevice::tag_CreateShaderResourceView: COMParse(p, [this](ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
			mod_descriptors[dest]->set(desc, pResource);
		}); break;

		case RecDevice::tag_CreateUnorderedAccessView: COMParse(p, [this](ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
			mod_descriptors[dest]->set(desc, pResource);
		}); break;

		case RecDevice::tag_CreateRenderTargetView: COMParse(p, [this, assets](ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
			if (auto *obj = assets->FindObject((uint64)pResource)) {
				const RecResource*	rr = obj->info;
				mod_descriptors[dest]->set(desc, pResource, *rr);
			}
		}); break;

		case RecDevice::tag_CreateDepthStencilView: COMParse(p, [this, assets](ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
			if (auto *obj = assets->FindObject((uint64)pResource)) {
				const RecResource*	rr = obj->info;
				mod_descriptors[dest]->set(desc, pResource, *rr);
			}
		}); break;

		case RecDevice::tag_CreateSampler: COMParse(p, [this](const D3D12_SAMPLER_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
			mod_descriptors[dest]->set(desc);
		}); break;

		case RecDevice::tag_CopyDescriptors: COMParse(p, [this, assets](UINT dest_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0> dest_starts, counted<const UINT,0> dest_sizes, UINT srce_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3> srce_starts, counted<const UINT,3> srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type)  {
			UINT				ssize	= 0;
			D3D12_CPU_DESCRIPTOR_HANDLE	sdesc;
			for (UINT s = 0, d = 0; d < dest_num; d++) {
				UINT		dsize	= dest_sizes ? dest_sizes[d] : 1;
				D3D12_CPU_DESCRIPTOR_HANDLE	ddesc = dest_starts[d];
				UINT		stride	= assets->FindDescriptorHeap(ddesc)->stride;

				while (dsize) {
					if (ssize == 0) {
						ssize	= srce_sizes ? srce_sizes[s] : 1;
						sdesc	= srce_starts[s];
						s++;
					}
					while (ssize && dsize) {
						mod_descriptors[ddesc] = copy(*GetDescriptor(assets, sdesc));
						ddesc.ptr += stride;
						sdesc.ptr += stride;
						--ssize;
						--dsize;
					}
				}
			}
		}); break;

		case RecDevice::tag_CopyDescriptorsSimple: COMParse(p, [this, assets](UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
			UINT	stride	= assets->FindDescriptorHeap(dest_start)->stride;

			while (num--) {
				mod_descriptors[dest_start] = copy(*GetDescriptor(assets, srce_start));
				dest_start.ptr += stride;
				srce_start.ptr += stride;
			}
		}); break;
	}
}

void DX12State::ProcessCommand(DX12Assets *assets, uint16 id, const uint16 *p) {
	op	= (RecCommandList::tag)id;

	switch (id) {
		//ID3D12GraphicsCommandList
		case RecCommandList::tag_Close: COMParse(p, []() {
			});
			break;
		case RecCommandList::tag_Reset: COMParse(p, [this, assets](ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
				SetPipelineState(assets, pInitialState);
			});
			break;
		case RecCommandList::tag_ClearState: COMParse(p, [this, assets](ID3D12PipelineState *pPipelineState) {
				SetPipelineState(assets, pPipelineState);
			});
			break;
		case RecCommandList::tag_DrawInstanced: COMParse(p, [this](UINT vertex_count, UINT instance_count, UINT vertex_offset, UINT instance_offset) {
				draw.vertex_count		= vertex_count;
				draw.instance_count		= instance_count;
				draw.vertex_offset		= vertex_offset;
				draw.instance_offset	= instance_offset;
				draw.prim				= topology;
			});
			break;
		case RecCommandList::tag_DrawIndexedInstanced: COMParse(p, [this](UINT IndexCountPerInstance, UINT instance_count, UINT index_offset, INT vertex_offset, UINT instance_offset) {
				draw.vertex_count		= IndexCountPerInstance;
				draw.instance_count		= instance_count;
				draw.index_offset		= index_offset;
				draw.vertex_offset		= vertex_offset;
				draw.instance_offset	= instance_offset;
				draw.prim				= topology;
			});
			break;
		case RecCommandList::tag_Dispatch: COMParse(p, [this](UINT dim_x, UINT dim_y, UINT dim_z) {
				compute.dim_x			= dim_x;
				compute.dim_y			= dim_y;
				compute.dim_z			= dim_z;
			});
			break;
		case RecCommandList::tag_CopyBufferRegion: COMParse(p, [](ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes) {
			});
			break;
		case RecCommandList::tag_CopyTextureRegion: COMParse2(p, [](const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox) {
			});
			break;
		case RecCommandList::tag_CopyResource: COMParse(p, [](ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource) {
			});
			break;
		case RecCommandList::tag_CopyTiles: COMParse(p, [](ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
			});
			break;
		case RecCommandList::tag_ResolveSubresource: COMParse(p, [](ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) {
			});
			break;
		case RecCommandList::tag_IASetPrimitiveTopology: COMParse(p, [this](D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) {
				topology	= PrimitiveTopology;
			});
			break;
		case RecCommandList::tag_RSSetViewports: COMParse(p, [this](UINT n, const D3D12_VIEWPORT *pViewports) {
				viewport = make_range_n(pViewports, n);
			});
			break;
		case RecCommandList::tag_RSSetScissorRects: COMParse(p, [this](UINT n, const D3D12_RECT *pRects) {
				scissor = make_range_n(pRects, n);
			});
			break;
		case RecCommandList::tag_OMSetBlendFactor: COMParse(p, [this](const FLOAT BlendFactor[4]) {
				memcpy(&this->BlendFactor, BlendFactor, sizeof(FLOAT) * 4);
			});
			break;
		case RecCommandList::tag_OMSetStencilRef: COMParse(p, [this](UINT StencilRef) {
				this->StencilRef = StencilRef;
			});
			break;
		case RecCommandList::tag_SetPipelineState: COMParse(p, [this, assets](ID3D12PipelineState *pPipelineState) {
				SetPipelineState(assets, pPipelineState);
			});
			break;
		case RecCommandList::tag_ResourceBarrier: COMParse2(p, [](UINT NumBarriers, counted<const D3D12_RESOURCE_BARRIER,0> pBarriers) {
			});
			break;
		case RecCommandList::tag_ExecuteBundle: COMParse(p, [](ID3D12GraphicsCommandList *pCommandList) {
			});
			break;
		case RecCommandList::tag_SetDescriptorHeaps: COMParse(p, [this, assets](UINT n, ID3D12DescriptorHeap *const *pp) {
			bound_heaps.clear();
			for (int i = 0; i < n; i++) {
				if (auto *obj = assets->FindObject((uint64)pp[i])) {
					ISO_ASSERT(obj->type == RecObject::DescriptorHeap);
					const RecDescriptorHeap	*h = obj->info;
					bound_heaps.set(h->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, obj);
				}
			}
			});
			break;
		case RecCommandList::tag_SetComputeRootSignature: COMParse(p, [this, assets](ID3D12RootSignature *pRootSignature) {
				compute_pipeline.SetRootSignature(assets, pRootSignature);
			});
			break;
		case RecCommandList::tag_SetGraphicsRootSignature: COMParse(p, [this, assets](ID3D12RootSignature *pRootSignature) {
				graphics_pipeline.SetRootSignature(assets, pRootSignature);
			});
			break;
		case RecCommandList::tag_SetComputeRootDescriptorTable: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
				compute_pipeline.GetSlot(RootParameterIndex) = BaseDescriptor;
			});
			break;
		case RecCommandList::tag_SetGraphicsRootDescriptorTable: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
				graphics_pipeline.GetSlot(RootParameterIndex) = BaseDescriptor;
			});
			break;
		case RecCommandList::tag_SetComputeRoot32BitConstant: COMParse(p, [this](UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
				compute_pipeline.GetSlot(RootParameterIndex)[DestOffsetIn32BitValues] = SrcData;
			});
			break;
		case RecCommandList::tag_SetGraphicsRoot32BitConstant: COMParse(p, [this](UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
				graphics_pipeline.GetSlot(RootParameterIndex)[DestOffsetIn32BitValues] = SrcData;
			});
			break;
		case RecCommandList::tag_SetComputeRoot32BitConstants: COMParse(p, [this](UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
				memcpy((uint32*)compute_pipeline.GetSlot(RootParameterIndex) + DestOffsetIn32BitValues, pSrcData, Num32BitValuesToSet * 4);
			});
			break;
		case RecCommandList::tag_SetGraphicsRoot32BitConstants: COMParse(p, [this](UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
				memcpy((uint32*)graphics_pipeline.GetSlot(RootParameterIndex) + DestOffsetIn32BitValues, pSrcData, Num32BitValuesToSet * 4);
			});
			break;
		case RecCommandList::tag_SetComputeRootConstantBufferView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
				compute_pipeline.GetSlot(RootParameterIndex) = BufferLocation;
			});
			break;
		case RecCommandList::tag_SetGraphicsRootConstantBufferView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
				graphics_pipeline.GetSlot(RootParameterIndex) = BufferLocation;
			});
			break;
		case RecCommandList::tag_SetComputeRootShaderResourceView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
				compute_pipeline.GetSlot(RootParameterIndex) = BufferLocation;
			});
			break;
		case RecCommandList::tag_SetGraphicsRootShaderResourceView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
				graphics_pipeline.GetSlot(RootParameterIndex) = BufferLocation;
			});
			break;
		case RecCommandList::tag_SetComputeRootUnorderedAccessView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
				compute_pipeline.GetSlot(RootParameterIndex) = BufferLocation;
			});
			break;
		case RecCommandList::tag_SetGraphicsRootUnorderedAccessView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
				graphics_pipeline.GetSlot(RootParameterIndex) = BufferLocation;
			});
			break;
		case RecCommandList::tag_IASetIndexBuffer: COMParse(p, [this](const D3D12_INDEX_BUFFER_VIEW *pView) {
				ibv = *pView;
			});
			break;
		case RecCommandList::tag_IASetVertexBuffers: COMParse(p, [this](UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews) {
				vbv.assign(pViews, pViews + NumViews);
			});
			break;
		case RecCommandList::tag_SOSetTargets: COMParse(p, [](UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews) {
			});
			break;
		case RecCommandList::tag_OMSetRenderTargets: COMParse(p, [this](UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) {
				copy_n(pRenderTargetDescriptors, targets, NumRenderTargetDescriptors);
				if (pDepthStencilDescriptor)
					depth = *pDepthStencilDescriptor;
				else
					depth.ptr = 0;
			});
			break;
		case RecCommandList::tag_ClearDepthStencilView: COMParse(p, [](D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects) {
			});
			break;
		case RecCommandList::tag_ClearRenderTargetView: COMParse(p, [](D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects) {
			});
			break;
		case RecCommandList::tag_ClearUnorderedAccessViewUint: COMParse(p, [](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, counted<const D3D12_RECT,4> pRects) {
			//ISO_TRACEF("ClearUnorderedAccessViewUint:") << hex(ViewGPUHandleInCurrentHeap.ptr) << ", " << hex(ViewCPUHandle.ptr) << '\n';
			});
			break;
		case RecCommandList::tag_ClearUnorderedAccessViewFloat: COMParse(p, [](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, counted<const D3D12_RECT,4> pRects) {
			});
			break;
		case RecCommandList::tag_DiscardResource:
			/*
			COMParse(p, [assets](ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion) {
				if (auto *obj = assets->FindObject((uint64)pResource)) {
					auto	rr = (DX12Assets::ResourceRecord*)obj->aux;
					ISO_ASSERT(rr->states.state == D3D12_RESOURCE_STATE_RENDER_TARGET);
				}
			});
			*/
			break;
		case RecCommandList::tag_BeginQuery: COMParse(p, [](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
			});
			break;
		case RecCommandList::tag_EndQuery: COMParse(p, [](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
			});
			break;
		case RecCommandList::tag_ResolveQueryData: COMParse(p, [](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset) {
			});
			break;
		case RecCommandList::tag_SetPredication: COMParse(p, [](ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation) {
			});
			break;
		case RecCommandList::tag_SetMarker: COMParse(p, [](UINT Metadata, const void *pData, UINT Size) {
			});
			break;
		case RecCommandList::tag_BeginEvent: COMParse(p, [](UINT Metadata, const void *pData, UINT Size) {
			});
			break;
		case RecCommandList::tag_EndEvent: COMParse(p, []() {
			});
			break;
		case RecCommandList::tag_ExecuteIndirect: COMParse(p, [this, assets](ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset) {
				indirect.command_count		= MaxCommandCount;
				indirect.signature			= assets->FindObject(uint64(pCommandSignature));
				indirect.arguments			= assets->FindObject(uint64(pArgumentBuffer));
				indirect.counts				= assets->FindObject(uint64(pCountBuffer));
				indirect.arguments_offset	= ArgumentBufferOffset;
				indirect.counts_offset		= CountBufferOffset;
			});
			break;

		//ID3D12GraphicsCommandList1
		case RecCommandList::tag_AtomicCopyBufferUINT: COMParse(p, [](ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges) {}); break;
		case RecCommandList::tag_AtomicCopyBufferUINT64: COMParse(p, [](ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges) {}); break;
		case RecCommandList::tag_OMSetDepthBounds: COMParse(p, [](FLOAT Min, FLOAT Max) {}); break;
		case RecCommandList::tag_SetSamplePositions: COMParse(p, [](UINT NumSamplesPerPixel, UINT NumPixels, D3D12_SAMPLE_POSITION *pSamplePositions) {}); break;
		case RecCommandList::tag_ResolveSubresourceRegion: COMParse(p, [](ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, ID3D12Resource *pSrcResource, UINT SrcSubresource, D3D12_RECT *pSrcRect, DXGI_FORMAT Format, D3D12_RESOLVE_MODE ResolveMode) {}); break;
		case RecCommandList::tag_SetViewInstanceMask: COMParse(p, [](UINT Mask) {}); break;
		//ID3D12GraphicsCommandList2
		case RecCommandList::tag_WriteBufferImmediate: COMParse(p, [](UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams, const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes) {}); break;
		//ID3D12GraphicsCommandList3
		case RecCommandList::tag_SetProtectedResourceSession: COMParse(p, [](ID3D12ProtectedResourceSession *pProtectedResourceSession) {}); break;
		//ID3D12GraphicsCommandList4
		case RecCommandList::tag_BeginRenderPass: COMParse(p, [this](UINT NumRenderTargets, counted<const D3D12_RENDER_PASS_RENDER_TARGET_DESC, 0> pRenderTargets, const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags) {
			for (int i = 0; i < NumRenderTargets; i++)
				targets[i] = pRenderTargets[i].cpuDescriptor;
			if (pDepthStencil)
				depth = pDepthStencil->cpuDescriptor;
			else
				depth.ptr = 0;
			});
			break;
		case RecCommandList::tag_EndRenderPass: COMParse(p, []() {}); break;
		case RecCommandList::tag_InitializeMetaCommand: COMParse(p, [](ID3D12MetaCommand *pMetaCommand, const void *pInitializationParametersData, SIZE_T InitializationParametersDataSizeInBytes) {}); break;
		case RecCommandList::tag_ExecuteMetaCommand: COMParse(p, [](ID3D12MetaCommand *pMetaCommand, const void *pExecutionParametersData, SIZE_T ExecutionParametersDataSizeInBytes) {}); break;
		case RecCommandList::tag_BuildRaytracingAccelerationStructure: {
			COMParse2(p, [](const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc, UINT NumPostbuildInfoDescs, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs) {
				ISO_TRACEF("hello");
			});
			break;
		}
		case RecCommandList::tag_EmitRaytracingAccelerationStructurePostbuildInfo: COMParse(p, [](const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc, UINT NumSourceAccelerationStructures, const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData) {}); break;
		case RecCommandList::tag_CopyRaytracingAccelerationStructure: COMParse(p, [](D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData, D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) {}); break;
		case RecCommandList::tag_SetPipelineState1: COMParse(p, [this, assets](ID3D12StateObject *pStateObject) {
			if (auto *obj = assets->FindObject(uint64(pStateObject)))
				compute_pipeline.Set(obj, assets, rmap_unique<KM, RecStateObject>(obj->info));
			});
			break;
		case RecCommandList::tag_DispatchRays: COMParse(p, [this](const D3D12_DISPATCH_RAYS_DESC *pDesc) {
				rays.dim_x	= pDesc->Width;
				rays.dim_y	= pDesc->Height;
				rays.dim_z	= pDesc->Depth;
				rays.RayGenerationShaderRecord	= pDesc->RayGenerationShaderRecord;
				rays.MissShaderTable			= pDesc->MissShaderTable;
				rays.HitGroupTable				= pDesc->HitGroupTable;
				rays.CallableShaderTable		= pDesc->CallableShaderTable;
			});
			break;
		//ID3D12GraphicsCommandList5
		case RecCommandList::tag_RSSetShadingRate: COMParse(p, [](D3D12_SHADING_RATE baseShadingRate, const array<D3D12_SHADING_RATE_COMBINER, 2> &combiners) {}); break;
		case RecCommandList::tag_RSSetShadingRateImage: COMParse(p, [](ID3D12Resource* shadingRateImage) {}); break;
		//ID3D12GraphicsCommandList6
		case RecCommandList::tag_DispatchMesh: COMParse(p, [this](UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) {
				compute.dim_x	= ThreadGroupCountX;
				compute.dim_y	= ThreadGroupCountY;
				compute.dim_z	= ThreadGroupCountZ;
			});
			break;
	}
}


auto PutMask(uint32 mask) {
	return [=](string_accum& sa) {
		if (mask != 15)
			sa << '.' << onlyif(mask & 1, 'r') << onlyif(mask & 2, 'g') << onlyif(mask & 4, 'b') << onlyif(mask & 8, 'a');
	};
}
auto PutMaskedCol(uint32 mask, float4p cc) {
	return [=](string_accum& sa) {
		if (!is_pow2(mask))
			sa << "float" << count_bits(mask) << '(';
		sa << onlyif(mask & 1, cc[0]) << onlyif(mask & 2, cc[1]) << onlyif(mask & 4, cc[2]) << onlyif(mask & 8, cc[3]) << onlyif(!is_pow2(mask), ')');
	};
}
string_accum &BlendStateMult(string_accum &sa, D3D12_BLEND m, uint32 mask, bool dest, float4p cc) {
	static const char *multiplier[] = {
		0,
		0,					//D3D12_BLEND_ZERO:				
		0,					//D3D12_BLEND_ONE:				
		"S",				//D3D12_BLEND_SRC_COLOR:			
		"(1 - S)",			//D3D12_BLEND_INV_SRC_COLOR:		
		"S.a",				//D3D12_BLEND_SRC_ALPHA:			
		"(1 - S.a)",		//D3D12_BLEND_INV_SRC_ALPHA:		
		"D.a",				//D3D12_BLEND_DEST_ALPHA:		
		"(1 - D.a)",		//D3D12_BLEND_INV_DEST_ALPHA:	
		"D",				//D3D12_BLEND_DEST_COLOR:		
		"(1 - D)",			//D3D12_BLEND_INV_DEST_COLOR:	
		0,					//D3D12_BLEND_SRC_ALPHA_SAT:		
		0,					//x:
		0,					//x:
		0,					//D3D12_BLEND_BLEND_FACTOR:		
		0,					//D3D12_BLEND_INV_BLEND_FACTOR:	
		"S1",				//D3D12_BLEND_SRC1_COLOR:		
		"(1 - S1)",			//D3D12_BLEND_INV_SRC1_COLOR:	
		"S1.a",				//D3D12_BLEND_SRC1_ALPHA:		
		"(1 - S1.a)",		//D3D12_BLEND_INV_SRC1_ALPHA:	
	};

	char	s = dest ? 'D' : 'S';
	switch (m) {
		case D3D12_BLEND_ZERO:				return sa << '0';
		case D3D12_BLEND_ONE:				return sa << s << PutMask(mask);
		case D3D12_BLEND_SRC_ALPHA_SAT:		return sa << "saturate(" << s << PutMask(mask) << " * S.a)";
		case D3D12_BLEND_BLEND_FACTOR:		return sa << s << PutMask(mask) << " * " << PutMaskedCol(mask, cc);
		case D3D12_BLEND_INV_BLEND_FACTOR:	return sa << s << PutMask(mask) << " * (1 - " << PutMaskedCol(mask, cc) << ')';
		default:							return sa << s << PutMask(mask) << " * " << multiplier[m];
	}
}
string_accum &BlendStateFunc(string_accum &sa, D3D12_BLEND_OP BlendOp, D3D12_BLEND SrcBlend, D3D12_BLEND DestBlend, uint32 mask, float4p cc) {
	switch (BlendOp) {
		case D3D12_BLEND_OP_ADD:
			if (SrcBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa, SrcBlend, mask, false, cc) << onlyif(DestBlend != D3D12_BLEND_ZERO, " + ");
			if (DestBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa, DestBlend, mask, true, cc);
			return sa;

		case D3D12_BLEND_OP_SUBTRACT:
			if (SrcBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa, SrcBlend, mask, false, cc);
			if (DestBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa << " - ", DestBlend, mask, true, cc);
			return sa;

		case D3D12_BLEND_OP_MIN:
			return BlendStateMult(BlendStateMult(sa << "min(", SrcBlend, mask, false, cc) << ", ", DestBlend, mask, true, cc) << ')';

		case D3D12_BLEND_OP_MAX:
			return BlendStateMult(BlendStateMult(sa << "max(", SrcBlend, mask, false, cc) << ", ", DestBlend, mask, true, cc) << ')';
	}
	return sa << '?';
}

auto WriteBlend(const D3D12_RENDER_TARGET_BLEND_DESC& desc, float4p cc) {
	return [=](string_accum& a) {
		if (desc.RenderTargetWriteMask == 0) {
			a << "masked out";
			return;
		}
		
		a << "D" << PutMask(desc.RenderTargetWriteMask) << " = ";

		if (!desc.BlendEnable) {
			a << "S" << PutMask(desc.RenderTargetWriteMask);

		} else if (!desc.LogicOpEnable) {
			if ((desc.RenderTargetWriteMask & D3D12_COLOR_WRITE_ENABLE_ALPHA) && (desc.SrcBlend != desc.SrcBlendAlpha || desc.DestBlend != desc.DestBlendAlpha || desc.BlendOp != desc.BlendOpAlpha)) {
				BlendStateFunc(a << "float" << count_bits(desc.RenderTargetWriteMask) << '(', desc.BlendOp, desc.SrcBlend, desc.DestBlend, desc.RenderTargetWriteMask & 7, cc);
				BlendStateFunc(a << ", ", desc.BlendOpAlpha, desc.SrcBlendAlpha, desc.DestBlendAlpha, 8, cc) << ')';
			} else {
				BlendStateFunc(a, desc.BlendOp, desc.SrcBlend, desc.DestBlend, desc.RenderTargetWriteMask, cc);
			}
		} else {
			static const char *logic[] = {
				"0",		//D3D12_LOGIC_OP_CLEAR
				"1",		//D3D12_LOGIC_OP_SET
				"S",		//D3D12_LOGIC_OP_COPY
				"~S",		//D3D12_LOGIC_OP_COPY_INVERTED
				"D(leave)",	//D3D12_LOGIC_OP_NOOP
				"~D",		//D3D12_LOGIC_OP_INVERT
				"S & D",	//D3D12_LOGIC_OP_AND
				"~(S & D)",	//D3D12_LOGIC_OP_NAND
				"S | D",	//D3D12_LOGIC_OP_OR
				"~(S | D)",	//D3D12_LOGIC_OP_NOR
				"S ^ D",	//D3D12_LOGIC_OP_XOR
				"~(S ^ D)",	//D3D12_LOGIC_OP_EQUIV
				"S & ~D",	//D3D12_LOGIC_OP_AND_REVERSE
				"~S & D",	//D3D12_LOGIC_OP_AND_INVERTED
				"S | ~D",	//D3D12_LOGIC_OP_OR_REVERSE
				"~S | D",	//D3D12_LOGIC_OP_OR_INVERTED
			};
			a << logic[desc.LogicOp];
		}
	};
}

//-----------------------------------------------------------------------------
//	DX12ShaderState
//-----------------------------------------------------------------------------


dx::Resource MakeSimulatorResource0(const RESOURCE_DESC* rr, const VIEW_DESC &view) {
	dx::Resource	r;
	auto	format			= view.GetComponents();//rr->Format);

	switch (view.dim) {
		case dx12::VIEW_DESC::BUFFER:
			r.init(dx::RESOURCE_DIMENSION_BUFFER, format, view.first_mip, rr->Width);
			r.set_slices(view.first_slice, view.num_slices);
			break;

		case dx12::VIEW_DESC::TEXTURE1D:
			r.init(dx::RESOURCE_DIMENSION_TEXTURE1D, format, rr->Width, 0);
			r.set_mips(view.first_mip, view.num_mips);
			break;

		case dx12::VIEW_DESC::TEXTURE1DARRAY:
			r.init(dx::RESOURCE_DIMENSION_TEXTURE1DARRAY, format, rr->Width, 0, rr->DepthOrArraySize);
			r.set_mips(view.first_mip, view.num_mips);
			r.set_slices(view.first_slice, view.num_slices);
			break;

		case dx12::VIEW_DESC::TEXTURE2D:
			r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, format, rr->Width, rr->Height);
			r.set_mips(view.first_mip, view.num_mips);
			break;

		case dx12::VIEW_DESC::TEXTURE2DARRAY:
			r.init(dx::RESOURCE_DIMENSION_TEXTURE2DARRAY, format, rr->Width, rr->Height, rr->DepthOrArraySize);
			r.set_mips(view.first_mip, view.num_mips);
			r.set_slices(view.first_slice, view.num_slices);
			break;

		case dx12::VIEW_DESC::TEXTURE2DMS:
			r.init(dx::RESOURCE_DIMENSION_TEXTURE2DMS, format, rr->Width, rr->Height, rr->DepthOrArraySize);
			r.set_mips(view.first_mip, view.num_mips);
			r.set_slices(view.first_slice, view.num_slices);
			break;

		case dx12::VIEW_DESC::TEXTURE2DMSARRAY:
			r.init(dx::RESOURCE_DIMENSION_TEXTURE2DMSARRAY, format, rr->Width, rr->Height, rr->DepthOrArraySize);
			r.set_mips(view.first_mip, view.num_mips);
			r.set_slices(view.first_slice, view.num_slices);
			break;

		case dx12::VIEW_DESC::TEXTURE3D	:
			r.init(dx::RESOURCE_DIMENSION_TEXTURE3D, format, rr->Width, rr->Height, rr->DepthOrArraySize);
			r.set_mips(view.first_mip, view.num_mips);
			break;

		case dx12::VIEW_DESC::TEXTURECUBE:
			r.init(dx::RESOURCE_DIMENSION_TEXTURECUBE, format, rr->Width, rr->Height, 6);
			r.set_mips(view.first_mip, view.num_mips);
			break;

		case dx12::VIEW_DESC::TEXTURECUBEARRAY:
			r.init(dx::RESOURCE_DIMENSION_TEXTURECUBEARRAY, format, rr->Width, rr->Height, rr->DepthOrArraySize);
			r.set_mips(view.first_mip, view.num_mips);
			r.set_slices(view.first_slice, view.num_slices);
			break;
	}

	return r;
}

dx::cache_type::cache_block GetSimulatorResourceData(const DX12Assets &assets, const dx::cache_type &cache, const DESCRIPTOR &desc) {
	if (auto obj = assets.FindObject((uint64)desc.res)) {
		const RecResource	*rr		= obj->info;
		return cache(rr->gpu | GPU);
	}
	switch (desc.type) {
		case DESCRIPTOR::PSRV:
		case DESCRIPTOR::PUAV:
		case DESCRIPTOR::PCBV:
			return cache(desc.ptr | GPU);
		default:
			return none;
	}
}

dx::Resource MakeSimulatorResource(const DX12Assets &assets, const dx::cache_type &cache, const DESCRIPTOR &desc) {
	if (auto obj = assets.FindObject((uint64)desc.res)) {
		const RecResource	*rr		= obj->info;
		auto	r = MakeSimulatorResource0(rr, desc);
		r.set_mem(cache((rr->gpu | GPU) + uintptr_t((void*)r), r.length()));
		return r;
	}

	return {};
}

dx::Buffer MakeSimulatorConstantBuffer(const dx::cache_type &cache, const DESCRIPTOR &desc) {
	dx::Buffer	r;
	uint32		size = desc.cbv.SizeInBytes;

	switch (desc.type) {
		case DESCRIPTOR::CBV:
			r.set_mem(cache(desc.cbv.BufferLocation, size));
			break;
		case DESCRIPTOR::PCBV:
			r.set_mem(size ? cache(desc.ptr, size) : cache(desc.ptr));
			break;
		case DESCRIPTOR::IMM:
			r.set_mem(memory_block(unconst(desc.imm), size));
			break;
	}
	return r;
}

dx::Sampler MakeSimulatorSampler(const DESCRIPTOR &desc) {
	dx::Sampler	r;
	if (desc.type == DESCRIPTOR::SMP) {
		r.filter			= dx::TextureFilterMode(desc.smp.Filter);
		r.address_u			= (dx::TextureAddressMode)desc.smp.AddressU;
		r.address_v			= (dx::TextureAddressMode)desc.smp.AddressV;
		r.address_w			= (dx::TextureAddressMode)desc.smp.AddressW;
		r.min_lod			= desc.smp.MinLOD;
		r.max_lod			= desc.smp.MaxLOD;
		r.mip_lod_bias		= desc.smp.MipLODBias;
		r.comparison_func	= (dx::ComparisonFunction)desc.smp.ComparisonFunc;
		memcpy(r.border, desc.smp.BorderColor, 4 * sizeof(float));

	} else if (desc.type == DESCRIPTOR::SSMP) {
		r.filter			= dx::TextureFilterMode(desc.ssmp.Filter);
		r.address_u			= (dx::TextureAddressMode)desc.ssmp.AddressU;
		r.address_v			= (dx::TextureAddressMode)desc.ssmp.AddressV;
		r.address_w			= (dx::TextureAddressMode)desc.ssmp.AddressW;
		r.min_lod			= desc.ssmp.MinLOD;
		r.max_lod			= desc.ssmp.MaxLOD;
		r.mip_lod_bias		= desc.ssmp.MipLODBias;
		r.comparison_func	= (dx::ComparisonFunction)desc.ssmp.ComparisonFunc;
		r.border[0]			= r.border[1] = r.border[2] = float(desc.ssmp.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);
		r.border[3]			= float(desc.ssmp.BorderColor != D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK);
	}
	return r;
}

struct DX12ShaderState : DX12Shader {
	string	name;
	const char *entry	= nullptr;

	sparse_array<DESCRIPTOR>	cbv;
	sparse_array<DESCRIPTOR>	srv;
	sparse_array<DESCRIPTOR>	uav;
	sparse_array<DESCRIPTOR>	smp;

	DX12ShaderState() {}
	DX12ShaderState(const DX12Assets::ShaderRecord *rec) : DX12Shader(*rec), name(rec->name) {}
	void	init(const DX12Assets &assets, dx::cache_type &cache, const DX12Assets::ShaderRecord *rec, const DX12State *state, const char *entry = nullptr, const DX12RootState *local = nullptr, const void *local_data = nullptr);
	void	SetResources(dx::SimulatorDX *sim, const DX12Assets &assets, const dx::cache_type &cache) const;

	unique_ptr<dx::SimulatorDX>	MakeSimulator(DX12Assets &assets, const dx::cache_type &cache, bool debug) const;
};

unique_ptr<dx::SimulatorDX>	DX12ShaderState::MakeSimulator(DX12Assets &assets, const dx::cache_type &cache, bool debug) const {
	if (auto dxbc = DXBC()) {
		if (debug) {
			if (auto dxil = dxbc->GetBlob<dx::DXBC::ShaderDebugInfoDXIL>()) {
				shared_ptr<bitcode::Module>		mod(new bitcode::Module);
				mod->read(memory_reader(dxil->data()));
				auto	sim = new dx::SimulatorDXIL(mod, GetEntryPoint(*mod, entry));
				SetResources(sim, assets, cache);
				return sim;
			}
		}
		if (auto dxil = dxbc->GetBlob<dx::DXBC::DXIL>()) {
			shared_ptr<bitcode::Module>		mod(new bitcode::Module);
			mod->read(memory_reader(dxil->data()));
			auto	sim = new dx::SimulatorDXIL(mod, GetEntryPoint(*mod, entry));
			SetResources(sim, assets, cache);
			return sim;

		} else {
			dx::DXBC::UcodeHeader header;
			auto	sim = new dx::SimulatorDXBC(&header, dxbc->GetUCode(header));
			SetResources(sim, assets, cache);
			return sim;
		}
	}
	return nullptr;
}

void DX12ShaderState::SetResources(dx::SimulatorDX *sim, const DX12Assets &assets, const dx::cache_type &cache) const {
	for (auto &i : cbv)
		sim->cbv[i.index()]	= MakeSimulatorConstantBuffer(cache, *i);

	for (auto &i : srv)
		sim->srv[i.index()]	= MakeSimulatorResource(assets, cache, *i);

	for (auto &i : uav)
		sim->uav[i.index()]	= MakeSimulatorResource(assets, cache, *i);

	for (auto &i : smp)
		sim->smp[i.index()]	= MakeSimulatorSampler(*i);
}

void DX12ShaderState::init(const DX12Assets &assets, dx::cache_type &cache, const DX12Assets::ShaderRecord *rec, const DX12State *state, const char *_entry, const DX12RootState *local, const void *local_data) {
	*(DX12Shader*)this = *rec;
	if (_entry) {
		name	= _entry;
		entry	= rec->GetMangledName(_entry);
	} else {
		name	= rec->name;
	}

	auto	decl = DeclResources(entry);

	for (auto &i : decl.cb) {
		auto	desc = state->GetBound(stage, DESCRIPTOR::CBV, i.index(), 0, local, local_data);
		if (desc.type == DESCRIPTOR::PCBV || desc.type == DESCRIPTOR::IMM)
			desc.cbv.SizeInBytes = i->size * 16;
		cbv[i.index()] = desc;
	}

	for (auto &i : decl.srv)
		srv[i.index()] = state->GetBound(stage, DESCRIPTOR::SRV, i.index(), 0, local, local_data);

	for (auto &i : decl.uav)
		uav[i.index()] = state->GetBound(stage, DESCRIPTOR::UAV, i.index(), 0, local, local_data);

	for (auto &i : decl.smp)
		smp[i.index()] = state->GetBound(stage, DESCRIPTOR::SMP, i.index(), 0, local, local_data);
}

//-----------------------------------------------------------------------------
//	DX12Replay
//-----------------------------------------------------------------------------

static const RecCommandList::tag	breadcrumb_tags[] = {
	RecCommandList::tag_SetMarker,											//D3D12_AUTO_BREADCRUMB_OP_SETMARKER	= 0,
	RecCommandList::tag_BeginEvent,											//D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT	= 1,
	RecCommandList::tag_EndEvent,											//D3D12_AUTO_BREADCRUMB_OP_ENDEVENT	= 2,
	RecCommandList::tag_DrawInstanced,										//D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED	= 3,
	RecCommandList::tag_DrawIndexedInstanced,								//D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED	= 4,
	RecCommandList::tag_ExecuteIndirect,									//D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT	= 5,
	RecCommandList::tag_Dispatch,											//D3D12_AUTO_BREADCRUMB_OP_DISPATCH	= 6,
	RecCommandList::tag_CopyBufferRegion,									//D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION	= 7,
	RecCommandList::tag_CopyTextureRegion,									//D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION	= 8,
	RecCommandList::tag_CopyResource,										//D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE	= 9,
	RecCommandList::tag_CopyTiles,											//D3D12_AUTO_BREADCRUMB_OP_COPYTILES	= 10,
	RecCommandList::tag_ResolveSubresource,									//D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE	= 11,
	RecCommandList::tag_ClearRenderTargetView,								//D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW	= 12,
	RecCommandList::tag_ClearUnorderedAccessViewUint,						//D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW	= 13,
	RecCommandList::tag_ClearDepthStencilView,								//D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW	= 14,
	RecCommandList::tag_ResourceBarrier,									//D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER	= 15,
	RecCommandList::tag_ExecuteBundle,										//D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE	= 16,
	RecCommandList::tag_Present,											//D3D12_AUTO_BREADCRUMB_OP_PRESENT	= 17,
	RecCommandList::tag_ResolveQueryData,									//D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA	= 18,
	RecCommandList::tag_BeginSubmission,									//D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION	= 19,
	RecCommandList::tag_EndSubmission,										//D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION	= 20,
	RecCommandList::tag_DecodeFrame,										//D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME	= 21,
	RecCommandList::tag_ProcessFrames,										//D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES	= 22,
	RecCommandList::tag_AtomicCopyBufferUINT,								//D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT	= 23,
	RecCommandList::tag_AtomicCopyBufferUINT64,								//D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64	= 24,
	RecCommandList::tag_ResolveSubresourceRegion,							//D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION	= 25,
	RecCommandList::tag_WriteBufferImmediate,								//D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE	= 26,
	RecCommandList::tag_DecodeFrame1,										//D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1	= 27,
	RecCommandList::tag_SetProtectedResourceSession,						//D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION	= 28,
	RecCommandList::tag_DecodeFrame2,										//D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2	= 29,
	RecCommandList::tag_ProcessFrames1,										//D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1	= 30,
	RecCommandList::tag_BuildRaytracingAccelerationStructure,				//D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE	= 31,
	RecCommandList::tag_EmitRaytracingAccelerationStructurePostbuildInfo,	//D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO	= 32,
	RecCommandList::tag_CopyRaytracingAccelerationStructure,				//D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE	= 33,
	RecCommandList::tag_DispatchRays,										//D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS	= 34,
	RecCommandList::tag_InitializeMetaCommand,								//D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND	= 35,
	RecCommandList::tag_ExecuteMetaCommand,									//D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND	= 36,
	RecCommandList::tag_EstimateMotion,										//D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION	= 37,
	RecCommandList::tag_ResolveMotionvectorHeap,							//D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP	= 38,
	RecCommandList::tag_SetPipelineState1,									//D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1	= 39,
	RecCommandList::tag_InitializeExtensionCommand,							//D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND	= 40,
	RecCommandList::tag_ExecuteExtensionCommand,							//D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND	= 41,
	RecCommandList::tag_DispatchMesh,										//D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH	= 42,
	RecCommandList::tag_EncodeFrame,										//D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME	= 43,
	RecCommandList::tag_ResolveEncoderOutputMetadata,						//D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA	= 44
};

struct Breadcrumbs : D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT {
	struct Item {
		D3D12_AUTO_BREADCRUMB_OP	op;
		auto	tag() const { return breadcrumb_tags[op]; }
	};
	struct Node : D3D12_AUTO_BREADCRUMB_NODE {
		auto	begin() const { return (const Item*)pCommandHistory; }
		auto	end()	const { return begin() + *pLastBreadcrumbValue; }
	};

	Breadcrumbs() { pHeadAutoBreadcrumbNode = nullptr; }
	explicit operator bool() const { return !!pHeadAutoBreadcrumbNode; }

	const Node*	find_stopped() const {
		for (auto p = pHeadAutoBreadcrumbNode; p; p = p->pNext) {
			if (*p->pLastBreadcrumbValue && *p->pLastBreadcrumbValue < p->BreadcrumbCount)
				return (const Node*)p;
		}
		return nullptr;
	}

	uint32	num_succeeded(const Node *n) const {
		uint32	count	= 0;
		for (auto p = n->pNext; p; p = p->pNext) {
			if (p->pCommandQueue == n->pCommandQueue)
				++count;
		}
		return count;
	}

	const Node*	find_node(ID3D12CommandQueue* queue, ID3D12GraphicsCommandList* list) const {
		for (auto p = pHeadAutoBreadcrumbNode; p; p = p->pNext) {
			if (p->pCommandQueue == queue && p->pCommandList == list)
				return (const Node*)p;
		}
		return nullptr;
	}

	bool Dump(string_accum &a) {
		for (auto p = pHeadAutoBreadcrumbNode; p; p = p->pNext) {
			auto	last	= *p->pLastBreadcrumbValue;
			a << "\nlist: " << p->pCommandListDebugNameW << "; queue: " << p->pCommandQueueDebugNameW << "; last value: " << last << '\n';
			for (auto op : make_range_n(p->pCommandHistory, p->BreadcrumbCount))
				a << onlyif(last-- == 0, "**\n") << op << '\n';
		}
		return true;
	}
};

struct PageFault : D3D12_DRED_PAGE_FAULT_OUTPUT {
	PageFault() {PageFaultVA = 0; }
	explicit operator bool() const { return !!PageFaultVA; }
};

void ReportLive(ID3D12Device *device) {
	ISO_OUTPUTF("device refs:%u\n", get_refcount(device));
	com_ptr<ID3D12DebugDevice> debug;
	device->QueryInterface(&debug);
	debug->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
}

struct DX12Device {
	//static ID3D12Device*	last_device;
	com_ptr<ID3D12Device>	device;

//	void MessageFunc(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, const char *description) {
//		ISO_OUTPUT(description);
//	}

	DX12Device(IDXGIAdapter1* adapter) {
		//if (last_device)
		//	ReportLive(last_device);
	#ifdef _DEBUG
		com_ptr<ID3D12Debug> debug;
		if (SUCCEEDED(D3D12GetDebugInterface(COM_CREATE(&debug))))
			debug->EnableDebugLayer();
	#endif

	#if 1//def _DEBUG
		com_ptr<ID3D12DeviceRemovedExtendedDataSettings> dred;
		if (SUCCEEDED(D3D12GetDebugInterface(COM_CREATE(&dred)))) {
			dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		}
	#endif

		com_ptr<IDXGIFactory4>	factory;
		if (SUCCEEDED(CreateDXGIFactory1(COM_CREATE(&factory))) && SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, COM_CREATE(&device)))) {
			//last_device = device.get();
		#if 0//def _DEBUG
			device.query<ID3D12InfoQueue>()->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		#endif
		}
	}

	Breadcrumbs get_breadcrumbs() {
		Breadcrumbs	b;
		com_ptr<ID3D12DeviceRemovedExtendedData> dred;
		if (SUCCEEDED(device->QueryInterface(COM_CREATE(&dred))))
			dred->GetAutoBreadcrumbsOutput(&b);
		return b;
	}
	PageFault	get_pagefault() {
		PageFault	p;
		com_ptr<ID3D12DeviceRemovedExtendedData> dred;
		if (SUCCEEDED(device->QueryInterface(COM_CREATE(&dred))))
			dred->GetPageFaultAllocationOutput(&p);
		return p;
	}
};

struct DX12Replay : COMReplay<DX12Replay>, DX12Device, dx12::Uploader {
	struct CommandListInfo {
		small_set<ID3D12CommandQueue*, 8>	queues;
		ID3D12PipelineState					*pipeline	= 0;
		bool								closed		= false;
	};
	map<ID3D12GraphicsCommandList*, CommandListInfo>	cmd_infos;

	DX12Assets&						assets;
	dx::cache_type&					cache;
	hash_map<void*,IUnknown*,true>	obj2localobj;
	hash_map<void*,handle,true>		obj2localh;
	ID3D12GraphicsCommandList*		current_cmd		= 0;
	uint64							frequency		= 0;
	uint32							descriptor_sizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	com_ptr<ID3D12CommandAllocator>	temp_alloc[4];
	com_ptr<ID3D12RootSignature>	root_signature;
	com_ptr<ID3D12PipelineState>	pipeline_state;
	bool							disabled		= false;

	DX12Replay*	operator->()	{ return this; }

	IUnknown*	_lookup_lax(void* p) {
		if (p == 0)
			return nullptr;

		auto	r = obj2localobj[p];
		if (!r.exists()) {
			if (auto *obj = assets.FindObject((uint64)p))
				r = MakeLocal(obj);
		}
		return r;
	}
	template<typename T> T* lookup_lax(T *p) {
		return static_cast<T*>(_lookup_lax(p));
	}

	IUnknown*	_lookup(void *p) {
		if (p == 0)
			return nullptr;

		auto r	= _lookup_lax(p);
		abort	= abort || !r;
		return r;
	}

	template<typename T> T* lookup(T *p) {
		return static_cast<T*>(_lookup(p));
	}

	handle			lookup(const handle& p) {
		if (!p.valid())
			return p;
		auto	r = obj2localh[p];
		if (!r.exists()) {
			auto	*obj	= assets.FindObject((uint64)(void*)p);
			abort	= abort || !obj;
			if (obj)
				r = CreateEvent(nullptr, false, false, nullptr);
		}
		return r;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE	lookup(D3D12_CPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : assets.descriptor_heaps) {
			if (auto d = i->holds(h)) {
				if (auto heap = lookup((ID3D12DescriptorHeap*)i.obj->obj)) {
					auto cpu	= heap->GetCPUDescriptorHandleForHeapStart();
					cpu.ptr		+= descriptor_sizes[i->type] * i->index(h);
					return cpu;
				}
			}
		}
		return h;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE	lookup(D3D12_GPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : assets.descriptor_heaps) {
			if (auto d = i->holds(h)) {
				if (auto heap = lookup((ID3D12DescriptorHeap*)i.obj->obj)) {
					auto gpu	= heap->GetGPUDescriptorHandleForHeapStart();
					gpu.ptr		+= descriptor_sizes[i->type] * i->index(h);
					return gpu;
				}
			}
		}
		return h;
	}

	D3D12_GPU_VIRTUAL_ADDRESS lookup(D3D12_GPU_VIRTUAL_ADDRESS a, size_t size = 0) {
		if (auto *obj = assets.FindByGPUAddress(a)) {
			const RecResource	*r		= obj->info;
			ISO_ASSERT(a >= r->gpu && a + size <= r->gpu + r->data_size);
			auto	r2	= lookup((ID3D12Resource*)obj->obj);
			return r2->GetGPUVirtualAddress() + (a - r->gpu);
		}
		return 0;
	}

	const void *lookup_shader(const void *p) {
		if (auto *r = assets.FindShader((uint64)p))
			return r->data;
		return 0;
	}

	template<typename F1, typename F2> IUnknown* ReplayPP2(const void *p, F2 f) {
		typedef	typename function<F1>::P	P;
		typedef map_t<PM, P>				RTL1;

		auto	*t	= (TL_tuple<RTL1>*)p;
		auto	&pp	= t->template get<RTL1::count - 1>();
		auto	v0	= save(*pp);

		Replay2<F1>(device, p, f);
		return InitObject(v0, (ID3D12Object*)*pp);
	}
	
	template<typename F> void* ReplayPP(const void *p, F f) {
		return ReplayPP2<F>(p, f);
	}

	ID3D12Object*	InitObject(void *v0, ID3D12Object *v1);
	IUnknown*		MakeLocal(const DX12Assets::ObjectRecord *obj);
	void			Process(ID3D12GraphicsCommandList *cmd, uint16 id, const uint16 *p);
	void			RunGen();
	void			Reset();
	void			RunTo(uint32 addr, const sparse_set<uint32> &skip);
	uint32			GetFail();

	dx::Resource	GetResource(const DESCRIPTOR &d, dx::cache_type &cache);
	dx::Resource	GetResource(const DESCRIPTOR *d, dx::cache_type &cache)	{ if (d) return GetResource(*d, cache); return {}; }
	ISO_ptr_machine<void> GetBitmap(const char *name, const DESCRIPTOR &d);
	ISO_ptr_machine<void> GetBitmap(const char *name, const DESCRIPTOR *d)	{ if (d) return GetBitmap(name, *d); return none; }

	bool	WaitAll() {
		if (!device)
			return false;

		for (auto &info : cmd_infos) {
			for (auto &q : info.queues)
				if (!waiter.Wait(q))
					return false;
			info.queues.clear();
		}
		return true;
	}


	DX12Replay(DX12Assets &assets, dx::cache_type &cache, IDXGIAdapter1 *adapter = 0);

	~DX12Replay() {
		WaitAll();
		for (auto i : obj2localobj) {
			if (i)
				i->Release();
		}

		for (auto i : obj2localh)
			CloseHandle(i);
	}

};

DX12Replay::DX12Replay(DX12Assets & assets, dx::cache_type & cache, IDXGIAdapter1 * adapter) : DX12Device(adapter), dx12::Uploader(device), assets(assets), cache(cache) {
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		descriptor_sizes[i] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));

	for (auto i : with_index(temp_alloc))
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE(i.index()), COM_CREATE(&*i));

	queue->SetName(L"upload_queue");
	alloc->SetName(L"upload_alloc");
	list->SetName(L"upload_list");

	ROOT_SIGNATURE_DESC<1,0>	sig_desc;
	dx12::DESCRIPTOR_RANGE ranges[] = {
		{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0},
		{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0},
	};
	sig_desc.params[0].Table(ranges);
	root_signature = sig_desc.Create(device);

	technique	*t		= ISO::root("data")["default"]["copy_tex"];
	DX11Shader	*pass	= (*t)[0];
	const_memory_block	cs = pass->sub[SS_PIXEL];
	auto		pipeline_desc = MakePipelineDesc(
		PIPELINE::SUB<PIPELINE::ROOT_SIGNATURE>(root_signature),
		PIPELINE::SUB<PIPELINE::CS>(cs)
	);
	pipeline_state = pipeline_desc.Create(device.query<ID3D12Device2>());

	queue->GetTimestampFrequency(&frequency);

	thread_local const char16	*object_name = 0;
	DWORD	callback_cookie;
	auto	callback	= [&](D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, const char *description) {
		if (Severity != D3D12_MESSAGE_SEVERITY_INFO)
			ISO_TRACEF(object_name) << ": " << description << '\n';
	};

	auto	info = device.query<ID3D12InfoQueue1>();
	info->SetMuteDebugOutput(true);
	info->RegisterMessageCallback(make_staticlambda_end(callback), D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS, &callback, &callback_cookie);


	timer	start;
#if 1
	parallel_for(filter(assets.objects, [&](DX12Assets::ObjectRecord& obj) { return obj.used && obj.type == RecObject::RootSignature; }), [&](auto &obj) {
		object_name = obj.name;
		obj2localobj[obj.obj] = MakeLocal(&obj);
	});

	parallel_for(filter(assets.objects, [&](DX12Assets::ObjectRecord& obj) { return obj.used && obj.type == RecObject::PipelineState; }), [&](auto& obj) {
		object_name = obj.name;
		obj2localobj[obj.obj] = MakeLocal(&obj);
	});
#else
	for (auto &obj : filter(assets.objects, [](DX12Assets::ObjectRecord& obj) { return obj.used && obj.type == RecObject::PipelineState; }))
		obj2localobj[obj.obj] = MakeLocal(&obj);
#endif

	for (auto& obj : assets.objects) {
		if (obj.used && obj.type != RecObject::Handle && !obj2localobj[obj.obj].exists()) {
			object_name = obj.name;
			obj2localobj[obj.obj] = MakeLocal(&obj);
		}
	}
	ISO_OUTPUTF("MakeLocal took ") << (float)start << "s\n";

	Uploader::Begin(device);
	start.reset();
	Reset();
	ISO_OUTPUTF("Uploading took ") << (float)start << "s\n";
	Uploader::End();

	info->UnregisterMessageCallback(callback_cookie);
	info->SetMuteDebugOutput(false);
}

void DX12Replay::Reset() {
	dx12::Barriers<256>		barriers;
	dx12::Barriers<256>		barriers2;
	dx12::DescriptorHeap	dh_rtv(device, 256, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dx12::DescriptorHeap	dh_dsv(device, 256, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	dx12::DescriptorHeap	dh_cpu(device, 256, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dx12::DescriptorHeapGPU	dh_gpu(device, 256, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	dynamic_array<com_ptr<ID3D12Resource>>	buffers;

	ID3D12DescriptorHeap*	dh_gpu1 = dh_gpu;
	list->SetDescriptorHeaps(1, &dh_gpu1);

	CommandListWithAlloc	barriers_list(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	
	for (auto& obj : assets.objects) {
		if (!obj.Used())
			continue;

		if (auto *local = obj2localobj[obj.obj].or_default()) {
			if (obj.type == RecObject::GraphicsCommandList) {

				if (auto *cmd = (ID3D12GraphicsCommandList*)obj2localobj[obj.obj].or_default()) {
					if (!cmd_infos[cmd].closed) {
						cmd->Close();
						cmd_infos[cmd].closed = true;
					}
				}

			} else if (obj.type == RecObject::Resource) {
				auto	res		= (ID3D12Resource*)local;
				auto	rr		= (DX12Assets::ResourceRecord*)obj.aux;
				auto&	states	= rr->states.curr;

				//Clear if necessary
				if (obj.NeedsRestore()) {
					obj.MarkClean();
					
					RecResource		*rec	= obj.info;
					auto			data	= rec->HasData() ? cache(rec->gpu | GPU, rec->data_size) : none;
					auto			hprops	= rec->HeapProps();

					if (data) {
						if (hprops.Type == D3D12_HEAP_TYPE_UPLOAD || hprops.CPUPageProperty >= D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE) {
							UploadToUpload(res, data);
						} else {
							states.transition_to(Transitioner(list, barriers), res, D3D12_RESOURCE_STATE_COPY_DEST);
							states	= D3D12_RESOURCE_STATE_COPY_DEST;
							barriers.Flush(list);
							Upload(device, res, data);
						}

					} else if (rec->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
						states.transition_to(Transitioner(list, barriers), res, D3D12_RESOURCE_STATE_RENDER_TARGET);
						barriers.Flush(list);
						states	= D3D12_RESOURCE_STATE_RENDER_TARGET;
						device->CreateRenderTargetView(res, addr(D3D12_RENDER_TARGET_VIEW_DESC(RESOURCE_DESC(res))), dh_rtv.cpu(0));
						float	col[4] = {0,0,0,0};
						list->ClearRenderTargetView(dh_rtv.cpu(0), col, 0, nullptr);

					} else if (rec->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
						states.transition_to(Transitioner(list, barriers), res, D3D12_RESOURCE_STATE_DEPTH_WRITE);
						barriers.Flush(list);
						states	= D3D12_RESOURCE_STATE_DEPTH_WRITE;
						device->CreateDepthStencilView(res, addr(D3D12_DEPTH_STENCIL_VIEW_DESC(RESOURCE_DESC(res))), dh_dsv.cpu(0));
						list->ClearDepthStencilView(dh_dsv.cpu(0), D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL, 0, 0, 0, nullptr);
						ExecuteReset();
						list->SetDescriptorHeaps(1, &dh_gpu1);

					} else if (rec->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
						states.transition_to(Transitioner(list, barriers), res, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
						barriers.Flush(list);
						states	= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						device->CreateUnorderedAccessView(res, nullptr, addr(D3D12_UNORDERED_ACCESS_VIEW_DESC(RESOURCE_DESC(res))), dh_cpu.cpu(0));
						device->CopyDescriptorsSimple(1, dh_gpu.cpu(0), dh_cpu.cpu(0), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						uint32	vals[4] = {0,0,0,0};
						list->ClearUnorderedAccessViewUint(dh_gpu.gpu(0), dh_cpu.cpu(0), res, vals, 0, nullptr);
					} else {
						ISO_OUTPUTF("Can't reset ") << obj.GetName() << '\n';
					}
				}

				//Restore states
				states.transition_to(Transitioner(barriers_list, barriers2), res, rr->states.init);
//				barriers2.Flush(barriers_list);
				states = rr->states.init;
			}
		}
	}
	ExecuteReset();
	Wait();

	barriers2.Flush(barriers_list);
	barriers_list->Close();
#if 1
	for (auto& obj : assets.objects) {
		if (obj.Used() && obj.type == RecObject::CommandQueue) {
			D3D12_COMMAND_QUEUE_DESC *desc = obj.info;
			if (desc->Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
				auto	queue	= (ID3D12CommandQueue*)obj2localobj[obj.obj].or_default();
				barriers_list.Execute(queue);
			}
		}
	}

	Waiter	waiter(device);
	for (auto& obj : assets.objects) {
		if (obj.Used() && obj.type == RecObject::CommandQueue) {
			D3D12_COMMAND_QUEUE_DESC *desc = obj.info;
			if (desc->Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
				auto	queue	= (ID3D12CommandQueue*)obj2localobj[obj.obj].or_default();
				waiter.Wait(queue);
			}
		}
	}
#endif
}

void DX12Replay::RunTo(uint32 addr, const sparse_set<uint32> &skip) {
	Reset();
	fiber_generator<uint32>	fg([this]() { RunGen(); }, 1 << 17);
	for (auto i = fg.begin(); *i < addr; ++i)
		disabled = skip.count(*i);

	for (auto& u : assets.uses.slice(assets.GetBatch(addr)->use_offset)) {
		if (u.Type() == RecObject::ResourceWrite)
			assets.objects[u.index].MarkDirty();
	}
}

ID3D12Object *DX12Replay::InitObject(void *v0, ID3D12Object *v1) {
	if (v1) {
		if (auto *obj = assets.FindObject((uint64)v0)) {
			v1->SetName(obj->name);
			if (obj->type == RecObject::Resource) {
				const RecResource	*rr = obj->info;
				if (rr->HasData()) {
					auto	*r = (ID3D12Resource*)v1;
					void	*p = 0;
					if (SUCCEEDED(r->Map(0, 0, &p))) {
						memcpy(p, cache(rr->gpu | GPU, rr->data_size), rr->data_size);
						r->Unmap(0, 0);
					}
				}
			}
		}
	}
	obj2localobj[v0] = v1;
	return v1;
}

void SafeCreateShaderResourceView(ID3D12Device* device, ID3D12Resource* r, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	if (r && RESOURCE_DESC(r).validate(desc))
		device->CreateShaderResourceView(r, &desc, dest);
}
void SafeCreateUnorderedAccessView(ID3D12Device* device, ID3D12Resource* r, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	if (r && RESOURCE_DESC(r).validate(desc))
		device->CreateUnorderedAccessView(r, nullptr, &desc, dest);
}
void SafeCreateRenderTargetView(ID3D12Device* device, ID3D12Resource* r, const D3D12_RENDER_TARGET_VIEW_DESC& desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	if (r && RESOURCE_DESC(r).validate(desc))
		device->CreateRenderTargetView(r, &desc, dest);
}
void SafeCreateDepthStencilView(ID3D12Device* device, ID3D12Resource* r, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	if (r && RESOURCE_DESC(r).validate(desc))
		device->CreateDepthStencilView(r, &desc, dest);
}

IUnknown* DX12Replay::MakeLocal(const DX12Assets::ObjectRecord *obj) {
#if 0
	static int recurse = 0;
	ISO_TRACEF() << repeat("  ", recurse) << "Making local " << obj->type << ' ' << obj->name << '\n';
	stack_depth	sd(recurse);
#endif

	HRESULT		hr	= E_FAIL;
	IUnknown	*r	= 0;

	switch (obj->type) {
		case RecObject::Resource: {
			if (!obj->used)
				return nullptr;

			ID3D12Resource	*res	= 0;
			auto			rr		= (DX12Assets::ResourceRecord*)obj->aux;
			auto			*rec	= rr->res();
			auto			clear	= rr->clear_value();
			auto			state	= D3D12_RESOURCE_STATE_COMMON;

			switch (rec->alloc) {
				case RecResource::UnknownAlloc:
					hr		= device->CreateCommittedResource(HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, rec, state, clear, COM_CREATE(&res));
					break;

				case RecResource::Reserved:
					break;

				case RecResource::Placed: {
					auto		hobj	= assets.FindHeapByGPUAddress(rec->gpu);
					auto		heap	= (ID3D12Heap*)lookup(hobj->obj);
					RecHeap*	rh		= hobj->info;
					auto		offset	= rec->gpu - rh->gpu;
					hr		= device->CreatePlacedResource(heap, offset, rec, state, clear, COM_CREATE(&res));
					break;
				}
				default: {
					RESOURCE_DESC	desc	= *rec;
					if (desc.Alignment && desc.Alignment != 0x10000)
						desc.Alignment = 0;

					HEAP_PROPERTIES	hprops = rec->HeapProps();
					switch (hprops.Type) {
						case D3D12_HEAP_TYPE_UPLOAD:	state =  D3D12_RESOURCE_STATE_GENERIC_READ; break;
						case D3D12_HEAP_TYPE_READBACK:	state =  D3D12_RESOURCE_STATE_COPY_DEST; break;
					}
					hr		= device->CreateCommittedResource(hprops, D3D12_HEAP_FLAG_NONE, &desc, state, clear, COM_CREATE(&res));
					break;
				}
			}

			r	= res;
			rr->states.curr = state;
			break;
		}
		case RecObject::Handle:
			ISO_ASSERT(false);
//			return CreateEvent(nullptr, false, false, nullptr);

		case RecObject::PipelineState: {
			if (!obj->used)
				return nullptr;

			ID3D12PipelineState	*ps = 0;
			switch (*(int*)obj->info) {
				case 0:
					Replay(obj->info + 8, [&](const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc) {
						hr = device->CreateGraphicsPipelineState(desc, COM_CREATE(&ps));
					});
					break;
				case 1:
					Replay(obj->info + 8, [&](const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc) {
						hr = device->CreateComputePipelineState(desc, COM_CREATE(&ps));
					});
					break;
				case 2:
					Replay(obj->info + 8, [&](const D3D12_PIPELINE_STATE_STREAM_DESC &desc) {
						hr = device.as<ID3D12Device2>()->CreatePipelineState(&desc, COM_CREATE(&ps));
					});
					break;
			}
			r = ps;
			break;
		}

		case RecObject::RootSignature:
			hr = device->CreateRootSignature(1, obj->info, obj->info.length(), COM_CREATE2(ID3D12RootSignature, &r));
			break;

		case RecObject::Heap: {
			RecHeap	*rec = obj->info;
			if (rec->Properties.CPUPageProperty > D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE && (rec->Flags & D3D12_HEAP_FLAG_SHARED)) {
				auto	dummy = VirtualAlloc(nullptr, rec->SizeInBytes, MEM_COMMIT, PAGE_READWRITE);
				hr = device.as<ID3D12Device3>()->OpenExistingHeapFromAddress(dummy, COM_CREATE2(ID3D12Heap, &r));
			} else {
				hr = device->CreateHeap(rec, COM_CREATE2(ID3D12Heap, &r));
			}
			break;
		}
		case RecObject::CommandAllocator:
			hr = device->CreateCommandAllocator(*obj->info, COM_CREATE2(ID3D12CommandAllocator, &r));
			break;

		case RecObject::Fence:
			hr = device->CreateFence(~0ull, D3D12_FENCE_FLAG_NONE, COM_CREATE2(ID3D12Fence, &r));
			break;

		case RecObject::DescriptorHeap:	{
			RecDescriptorHeap	*d = obj->info;
			D3D12_DESCRIPTOR_HEAP_DESC	desc = {
				d->type,
				d->count,
				d->gpu.ptr ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
				1
			};

			hr = device->CreateDescriptorHeap(&desc, COM_CREATE2(ID3D12DescriptorHeap, &r));

			ID3D12DescriptorHeap		*heap	= (ID3D12DescriptorHeap*)r;
			D3D12_CPU_DESCRIPTOR_HANDLE	cpu0	= d->cpu;
			D3D12_CPU_DESCRIPTOR_HANDLE	cpu1	= heap->GetCPUDescriptorHandleForHeapStart();
			auto						stride0	= d->stride;
			auto						stride1	= descriptor_sizes[d->type];

			for (auto& i : *d) {
				if (assets.descriptor_used.count(cpu0)) {
					switch (i.type) {
						case DESCRIPTOR::CBV:	device->CreateConstantBufferView(&i.cbv, cpu1); break;
						case DESCRIPTOR::SRV:	SafeCreateShaderResourceView(device, lookup_lax(i.res), i.srv, cpu1); break;
						case DESCRIPTOR::UAV:	SafeCreateUnorderedAccessView(device, lookup_lax(i.res), i.uav, cpu1); break;
						case DESCRIPTOR::RTV:	SafeCreateRenderTargetView(device, lookup_lax(i.res), i.rtv, cpu1); break;
						case DESCRIPTOR::DSV:	SafeCreateDepthStencilView(device, lookup_lax(i.res), i.dsv, cpu1); break;
						case DESCRIPTOR::SMP:	device->CreateSampler(&i.smp, cpu1); break;
						default: break;
					}
				}
				cpu0.ptr += stride0;
				cpu1.ptr += stride1;
			}
			break;
		}
		case RecObject::QueryHeap:
			if (!obj->used)
				return nullptr;
			hr = device->CreateQueryHeap(obj->info, COM_CREATE2(ID3D12QueryHeap, &r));
			break;

		case RecObject::GraphicsCommandList: {
			const RecCommandList	*rec = obj->info;
			hr	= device->CreateCommandList(rec->node_mask, rec->list_type, temp_alloc[rec->list_type], lookup(rec->state), COM_CREATE2(ID3D12GraphicsCommandList, &r));
			auto	cmd = (ID3D12GraphicsCommandList*)r;
			cmd->Close();
			//ISO_ASSERT(cmd_infos.count(cmd) == 0);
			cmd_infos[cmd].closed = true;
			ISO_ASSERT(validate(cmd_infos));
			break;
		}
		case RecObject::CommandQueue:
			hr = device->CreateCommandQueue(obj->info, COM_CREATE2(ID3D12CommandQueue, &r));
			break;

		case RecObject::CommandSignature:
			Replay(obj->info, [&](const tuple<D3D12_COMMAND_SIGNATURE_DESC, ID3D12RootSignature*> &p) {
				hr = device->CreateCommandSignature(&p.get<0>(), p.get<1>(), COM_CREATE2(ID3D12CommandSignature, &r));
			});
			break;
//		case RecObject::Device:					hr = device->CreateDevice(obj->info, __uuidof(ID3D12), &r); break;
		default:
			return nullptr;
			ISO_ASSERT(false);

	}
	ISO_ASSERT(SUCCEEDED(hr));
	if (r)
		((ID3D12DeviceChild*)r)->SetName(obj->name);

	return r;
}

void DX12Replay::Process(ID3D12GraphicsCommandList *cmd, uint16 id, const uint16 *p) {
	switch (id) {
		//ID3D12GraphicsCommandList
//		case RecCommandList::tag_Close:	Replay(cmd, p, &ID3D12GraphicsCommandList::Close);  cmd_infos[cmd].closed = true; break;
//		case RecCommandList::tag_Reset:	Replay(cmd, p, &ID3D12GraphicsCommandList::Reset); break;
		case RecCommandList::tag_ClearState:							Replay(cmd, p, &ID3D12GraphicsCommandList::ClearState); break;
		case RecCommandList::tag_DrawInstanced:							Replay(cmd, p, &ID3D12GraphicsCommandList::DrawInstanced); break;
		case RecCommandList::tag_DrawIndexedInstanced:					Replay(cmd, p, &ID3D12GraphicsCommandList::DrawIndexedInstanced); break;

		case RecCommandList::tag_Dispatch:
			//Replay(cmd, p, &ID3D12GraphicsCommandList::Dispatch); break;
			Replay(p, [cmd](UINT ThreadGroupCountX,UINT ThreadGroupCountY,UINT ThreadGroupCountZ) {
				if (ThreadGroupCountX == 1440)
					ThreadGroupCountX = 1;
				cmd->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			});
			break;

		case RecCommandList::tag_CopyBufferRegion:						Replay(cmd, p, &ID3D12GraphicsCommandList::CopyBufferRegion); break;
		case RecCommandList::tag_CopyTextureRegion:						Replay(cmd, p, &ID3D12GraphicsCommandList::CopyTextureRegion); break;
		case RecCommandList::tag_CopyResource:							Replay(cmd, p, &ID3D12GraphicsCommandList::CopyResource); break;
		case RecCommandList::tag_CopyTiles:								Replay(cmd, p, &ID3D12GraphicsCommandList::CopyTiles); break;
		case RecCommandList::tag_ResolveSubresource:					Replay(cmd, p, &ID3D12GraphicsCommandList::ResolveSubresource); break;
		case RecCommandList::tag_IASetPrimitiveTopology:				Replay(cmd, p, &ID3D12GraphicsCommandList::IASetPrimitiveTopology); break;
		case RecCommandList::tag_RSSetViewports:						Replay(cmd, p, &ID3D12GraphicsCommandList::RSSetViewports); break;
		case RecCommandList::tag_RSSetScissorRects:						Replay(cmd, p, &ID3D12GraphicsCommandList::RSSetScissorRects); break;
		case RecCommandList::tag_OMSetBlendFactor:						Replay(cmd, p, &ID3D12GraphicsCommandList::OMSetBlendFactor); break;
		case RecCommandList::tag_OMSetStencilRef:						Replay(cmd, p, &ID3D12GraphicsCommandList::OMSetStencilRef); break;
		case RecCommandList::tag_SetPipelineState:
			Replay(cmd, p, &ID3D12GraphicsCommandList::SetPipelineState);
			break;

		case RecCommandList::tag_ResourceBarrier:
			COMParse2(p, [this](UINT num, counted2<const D3D12_RESOURCE_BARRIER,0> descs) {
				for (auto& i : descs) {
					if (i.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
						if (auto *obj = assets.FindObject((uint64)i.Transition.pResource)) {
							ISO_ASSERT(obj->type == RecObject::Resource);
							auto	rr		= (DX12Assets::ResourceRecord*)obj->aux;
							auto	sub		= i.Transition.Subresource;
							auto	subs	= rr->res()->NumSubresources(nullptr);
							rr->states.curr.set(sub, subs, i.Transition.StateAfter);
						}
					}
				}
			});
			Replay2<void(UINT,counted<const D3D12_RESOURCE_BARRIER,0>)>(cmd, p, &ID3D12GraphicsCommandList::ResourceBarrier);
			break;

		case RecCommandList::tag_ExecuteBundle:							Replay(cmd, p, &ID3D12GraphicsCommandList::ExecuteBundle); break;
		case RecCommandList::tag_SetDescriptorHeaps:					Replay2<void(UINT,counted<ID3D12DescriptorHeap *const, 0>)>(cmd, p, &ID3D12GraphicsCommandList::SetDescriptorHeaps); break;
		case RecCommandList::tag_SetComputeRootSignature:				Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootSignature); break;
		case RecCommandList::tag_SetGraphicsRootSignature:				Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootSignature); break;
		case RecCommandList::tag_SetComputeRootDescriptorTable:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable); break;
		case RecCommandList::tag_SetGraphicsRootDescriptorTable:		Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable); break;
		case RecCommandList::tag_SetComputeRoot32BitConstant:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRoot32BitConstant); break;
		case RecCommandList::tag_SetGraphicsRoot32BitConstant:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant); break;
		case RecCommandList::tag_SetComputeRoot32BitConstants:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRoot32BitConstants); break;
		case RecCommandList::tag_SetGraphicsRoot32BitConstants:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants); break;
		case RecCommandList::tag_SetComputeRootConstantBufferView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootConstantBufferView); break;
		case RecCommandList::tag_SetGraphicsRootConstantBufferView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView); break;
		case RecCommandList::tag_SetComputeRootShaderResourceView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootShaderResourceView); break;
		case RecCommandList::tag_SetGraphicsRootShaderResourceView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView); break;
		case RecCommandList::tag_SetComputeRootUnorderedAccessView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView); break;
		case RecCommandList::tag_SetGraphicsRootUnorderedAccessView:	Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView); break;
		case RecCommandList::tag_IASetIndexBuffer:						Replay(cmd, p, &ID3D12GraphicsCommandList::IASetIndexBuffer); break;
		case RecCommandList::tag_IASetVertexBuffers:					Replay2<void(UINT,UINT,counted<const D3D12_VERTEX_BUFFER_VIEW,1>)>(cmd, p, &ID3D12GraphicsCommandList::IASetVertexBuffers); break;
		case RecCommandList::tag_SOSetTargets:							Replay2<void(UINT,UINT,counted<const D3D12_STREAM_OUTPUT_BUFFER_VIEW,1>)>(cmd, p, &ID3D12GraphicsCommandList::SOSetTargets); break;
		case RecCommandList::tag_OMSetRenderTargets:					Replay2<void(UINT,counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0>,BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*)>(cmd, p, &ID3D12GraphicsCommandList::OMSetRenderTargets); break;
		case RecCommandList::tag_ClearDepthStencilView:					Replay(cmd, p, &ID3D12GraphicsCommandList::ClearDepthStencilView); break;
		case RecCommandList::tag_ClearRenderTargetView:					Replay2<void(D3D12_CPU_DESCRIPTOR_HANDLE,array<float, 4>,UINT,const D3D12_RECT*)>(cmd, p, &ID3D12GraphicsCommandList::ClearRenderTargetView); break;
		case RecCommandList::tag_ClearUnorderedAccessViewUint:			Replay2<void(D3D12_GPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,ID3D12Resource*,array<UINT,4>,UINT,counted<const D3D12_RECT,4>)>(cmd, p, &ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint); break;
		case RecCommandList::tag_ClearUnorderedAccessViewFloat:			Replay2<void(D3D12_GPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,ID3D12Resource*,array<float,4>,UINT,counted<const D3D12_RECT,4>)>(cmd, p, &ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat); break;
		case RecCommandList::tag_DiscardResource:						Replay(cmd, p, &ID3D12GraphicsCommandList::DiscardResource); break;
		case RecCommandList::tag_BeginQuery:							Replay(cmd, p, &ID3D12GraphicsCommandList::BeginQuery); break;
		case RecCommandList::tag_EndQuery:								Replay(cmd, p, &ID3D12GraphicsCommandList::EndQuery); break;
		case RecCommandList::tag_ResolveQueryData:						Replay(cmd, p, &ID3D12GraphicsCommandList::ResolveQueryData); break;
		case RecCommandList::tag_SetPredication:						Replay(cmd, p, &ID3D12GraphicsCommandList::SetPredication); break;
		case RecCommandList::tag_SetMarker:								Replay2<void(UINT,counted<const uint8,2>,UINT)>(cmd, p, &ID3D12GraphicsCommandList::SetMarker); break;
		case RecCommandList::tag_BeginEvent:							Replay2<void(UINT,counted<const uint8,2>,UINT)>(cmd, p, &ID3D12GraphicsCommandList::BeginEvent); break;
		case RecCommandList::tag_EndEvent:								Replay(cmd, p, &ID3D12GraphicsCommandList::EndEvent); break;
		case RecCommandList::tag_ExecuteIndirect:						Replay(cmd, p, &ID3D12GraphicsCommandList::ExecuteIndirect); break;
		//ID3D12GraphicsCommandList1
		case RecCommandList::tag_AtomicCopyBufferUINT:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::AtomicCopyBufferUINT); break;
		case RecCommandList::tag_AtomicCopyBufferUINT64:				Replay(cmd, p, &ID3D12GraphicsCommandListLatest::AtomicCopyBufferUINT64); break;
		case RecCommandList::tag_OMSetDepthBounds:						Replay(cmd, p, &ID3D12GraphicsCommandListLatest::OMSetDepthBounds); break;
		case RecCommandList::tag_SetSamplePositions:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetSamplePositions); break;
		case RecCommandList::tag_ResolveSubresourceRegion:				Replay(cmd, p, &ID3D12GraphicsCommandListLatest::ResolveSubresourceRegion); break;
		case RecCommandList::tag_SetViewInstanceMask:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetViewInstanceMask); break;
		//ID3D12GraphicsCommandList2
		case RecCommandList::tag_WriteBufferImmediate:					Replay2<void(UINT, counted<const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER, 0>, counted<const D3D12_WRITEBUFFERIMMEDIATE_MODE, 0>)>(cmd, p, &ID3D12GraphicsCommandListLatest::WriteBufferImmediate); break;
		//ID3D12GraphicsCommandList3
		case RecCommandList::tag_SetProtectedResourceSession:			Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetProtectedResourceSession); break;
		//ID3D12GraphicsCommandList4
		case RecCommandList::tag_BeginRenderPass:						Replay(cmd, p, &ID3D12GraphicsCommandListLatest::BeginRenderPass); break;
		case RecCommandList::tag_EndRenderPass:							Replay(cmd, p, &ID3D12GraphicsCommandListLatest::EndRenderPass); break;
		case RecCommandList::tag_InitializeMetaCommand:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::InitializeMetaCommand); break;
		case RecCommandList::tag_ExecuteMetaCommand:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::ExecuteMetaCommand); break;
		case RecCommandList::tag_BuildRaytracingAccelerationStructure:	Replay(cmd, p, &ID3D12GraphicsCommandListLatest::BuildRaytracingAccelerationStructure); break;
		case RecCommandList::tag_EmitRaytracingAccelerationStructurePostbuildInfo:	Replay(cmd, p, &ID3D12GraphicsCommandListLatest::EmitRaytracingAccelerationStructurePostbuildInfo); break;
		case RecCommandList::tag_CopyRaytracingAccelerationStructure:	Replay(cmd, p, &ID3D12GraphicsCommandListLatest::CopyRaytracingAccelerationStructure); break;
		case RecCommandList::tag_SetPipelineState1:						Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetPipelineState1); break;
		case RecCommandList::tag_DispatchRays:							Replay(cmd, p, &ID3D12GraphicsCommandListLatest::DispatchRays); break;
		//ID3D12GraphicsCommandList5
		case RecCommandList::tag_RSSetShadingRate:						Replay2<void(D3D12_SHADING_RATE, const array<D3D12_SHADING_RATE_COMBINER, 2>&)>(cmd, p, &ID3D12GraphicsCommandListLatest::RSSetShadingRate); break;
		case RecCommandList::tag_RSSetShadingRateImage:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::RSSetShadingRateImage); break;
		//ID3D12GraphicsCommandList6
		case RecCommandList::tag_DispatchMesh:							Replay(cmd, p, &ID3D12GraphicsCommandListLatest::DispatchMesh); break;
	}
}

void DX12Replay::RunGen() {
	bool	done		= false;
	uint32	batch_addr	= assets.device_object->info.size32();
	uint32	addr;
	DWORD	callback_cookie;
	auto	callback	= [&](D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, const char *description) {
		if (Severity != D3D12_MESSAGE_SEVERITY_INFO)
			ISO_TRACEF("Batch ") << assets.batches.index_of(assets.GetBatch(addr)) << " @ 0x" << hex(addr) << ": " << description << '\n';
	};

	auto	info = device.query<ID3D12InfoQueue1>();
	info->SetMuteDebugOutput(true);
	info->RegisterMessageCallback(make_staticlambda_end(callback), D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS, &callback, &callback_cookie);

	for (const COMRecording::header *c = assets.device_object->info, *e = assets.device_object->info.end(); !done && c < e; c = c->next()) {
		addr	= (char*)c - (char*)assets.device_object->info;
		//ISO_TRACEF("replay addr = 0x") << hex(addr) << '\n';
		auto	p = c->data();
		try { switch (c->id) {
		//ID3D12Device
			case RecDevice::tag_CreateCommandQueue:							ReplayPP(p,			&ID3D12Device::CreateCommandQueue); break;
			case RecDevice::tag_CreateCommandAllocator:						ReplayPP(p,			&ID3D12Device::CreateCommandAllocator); break;
			case RecDevice::tag_CreateGraphicsPipelineState:				ReplayPP(p,			&ID3D12Device::CreateGraphicsPipelineState); break;
			case RecDevice::tag_CreateComputePipelineState:					ReplayPP(p,			&ID3D12Device::CreateComputePipelineState); break;
			case RecDevice::tag_CreateCommandList:							Replay(p,			[this](UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **pp) {
				auto	v0	= save(*pp);
				device->CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState, riid, pp);
				InitObject(v0, (ID3D12Object*)*pp);
				});
				break;
			case RecDevice::tag_CheckFeatureSupport:						Replay(device, p,	&ID3D12Device::CheckFeatureSupport); break;
			case RecDevice::tag_CreateDescriptorHeap:						ReplayPP(p,			&ID3D12Device::CreateDescriptorHeap); break;
			case RecDevice::tag_CreateRootSignature:						ReplayPP2<HRESULT(UINT, counted<uint8, 2>, SIZE_T, REFIID, void**)>(p, &ID3D12Device::CreateRootSignature); break;
			case RecDevice::tag_CreateConstantBufferView:					Replay(device, p,	&ID3D12Device::CreateConstantBufferView); break;
			case RecDevice::tag_CreateShaderResourceView:					Replay(device, p,	&ID3D12Device::CreateShaderResourceView); break;
			case RecDevice::tag_CreateUnorderedAccessView:					Replay(device, p,	&ID3D12Device::CreateUnorderedAccessView); break;
			case RecDevice::tag_CreateRenderTargetView:						Replay(device, p,	&ID3D12Device::CreateRenderTargetView); break;
			case RecDevice::tag_CreateDepthStencilView:						Replay(device, p,	&ID3D12Device::CreateDepthStencilView); break;
			case RecDevice::tag_CreateSampler:								Replay(device, p,	&ID3D12Device::CreateSampler); break;
			case RecDevice::tag_CopyDescriptors:							Replay2<void(UINT,counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0>, counted<const UINT,0>, UINT, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3>, counted<const UINT,3>, D3D12_DESCRIPTOR_HEAP_TYPE)>(device, p, &ID3D12Device::CopyDescriptors); break;
			case RecDevice::tag_CopyDescriptorsSimple:						Replay(device, p,	&ID3D12Device::CopyDescriptorsSimple); break;
			case RecDevice::tag_CreateCommittedResource:					ReplayPP(p,			&ID3D12Device::CreateCommittedResource); break;
			case RecDevice::tag_CreateHeap:									ReplayPP(p,			&ID3D12Device::CreateHeap); break;
			case RecDevice::tag_CreatePlacedResource:						ReplayPP(p,			&ID3D12Device::CreatePlacedResource); break;
			case RecDevice::tag_CreateReservedResource:						ReplayPP(p,			&ID3D12Device::CreateReservedResource); break;
			case RecDevice::tag_CreateSharedHandle:							ReplayPP(p,			&ID3D12Device::CreateSharedHandle); break;
			case RecDevice::tag_OpenSharedHandle://							ReplayPP(p,			&ID3D12Device::OpenSharedHandle); break;
				Replay(p, [this](HANDLE handle, REFIID riid, void **pp) {
					void	*v0	= *pp;
					if (auto *obj = assets.FindObject((uint64)v0))
						InitObject(v0, (ID3D12Object*)MakeLocal(obj));
				});
				break;
			case RecDevice::tag_OpenSharedHandleByName:						Replay(device, p,	&ID3D12Device::OpenSharedHandleByName); break;
			case RecDevice::tag_MakeResident:								Replay2<HRESULT(UINT,counted<ID3D12Pageable* const, 0>)>(device, p, &ID3D12Device::MakeResident); break;
			case RecDevice::tag_Evict:										Replay2<HRESULT(UINT,counted<ID3D12Pageable* const, 0>)>(device, p, &ID3D12Device::Evict); break;
			case RecDevice::tag_CreateFence:								ReplayPP(p,			&ID3D12Device::CreateFence); break;
			case RecDevice::tag_CreateQueryHeap:							ReplayPP(p,			&ID3D12Device::CreateQueryHeap); break;
			case RecDevice::tag_SetStablePowerState:						Replay(device, p,	&ID3D12Device::SetStablePowerState); break;
			case RecDevice::tag_CreateCommandSignature:						ReplayPP(p,			&ID3D12Device::CreateCommandSignature); break;
		//ID3D12Device1
			case RecDevice::tag_CreatePipelineLibrary:						Replay(device, p,	&ID3D12DeviceLatest::CreatePipelineLibrary); break;
			case RecDevice::tag_SetEventOnMultipleFenceCompletion:			Replay(device, p,	&ID3D12DeviceLatest::SetEventOnMultipleFenceCompletion); break;
			case RecDevice::tag_SetResidencyPriority:						Replay(device, p,	&ID3D12DeviceLatest::SetResidencyPriority); break;
		//ID3D12Device2
			case RecDevice::tag_CreatePipelineState:						Replay(device, p,	&ID3D12DeviceLatest::CreatePipelineState); break;
		//ID3D12Device3
			case RecDevice::tag_OpenExistingHeapFromAddress:				Replay(device, p,	&ID3D12DeviceLatest::OpenExistingHeapFromAddress); break;
			case RecDevice::tag_OpenExistingHeapFromFileMapping:			Replay(device, p,	&ID3D12DeviceLatest::OpenExistingHeapFromFileMapping); break;
			case RecDevice::tag_EnqueueMakeResident:						Replay2<HRESULT(D3D12_RESIDENCY_FLAGS,UINT,counted<ID3D12Pageable* const, 1>,ID3D12Fence*,UINT64)>(device, p, &ID3D12DeviceLatest::EnqueueMakeResident); break;
		//ID3D12Device4
			case RecDevice::tag_CreateCommandList1:							Replay(device, p,	&ID3D12DeviceLatest::CreateCommandList1); break;
			case RecDevice::tag_CreateProtectedResourceSession:				Replay(device, p,	&ID3D12DeviceLatest::CreateProtectedResourceSession); break;
			case RecDevice::tag_CreateCommittedResource1:					Replay(device, p,	&ID3D12DeviceLatest::CreateCommittedResource1); break;
			case RecDevice::tag_CreateHeap1:								Replay(device, p,	&ID3D12DeviceLatest::CreateHeap1); break;
			case RecDevice::tag_CreateReservedResource1:					Replay(device, p,	&ID3D12DeviceLatest::CreateReservedResource1); break;
		//ID3D12Device5
			case RecDevice::tag_CreateLifetimeTracker:						Replay(device, p,	&ID3D12DeviceLatest::CreateLifetimeTracker); break;
			case RecDevice::tag_CreateMetaCommand:							Replay(device, p,	&ID3D12DeviceLatest::CreateMetaCommand); break;
			case RecDevice::tag_CreateStateObject:							Replay(device, p,	&ID3D12DeviceLatest::CreateStateObject); break;
		//ID3D12Device6
			case RecDevice::tag_SetBackgroundProcessingMode:				Replay(device, p,	&ID3D12DeviceLatest::SetBackgroundProcessingMode); break;
		//ID3D12Device7
			case RecDevice::tag_AddToStateObject:							Replay(device, p,	&ID3D12DeviceLatest::AddToStateObject); break;
			case RecDevice::tag_CreateProtectedResourceSession1:			Replay(device, p,	&ID3D12DeviceLatest::CreateProtectedResourceSession1); break;
		//ID3D12Device8
			case RecDevice::tag_CreateCommittedResource2:					Replay(device, p,	&ID3D12DeviceLatest::CreateCommittedResource2); break;
			case RecDevice::tag_CreatePlacedResource1:						Replay(device, p,	&ID3D12DeviceLatest::CreatePlacedResource1); break;
			case RecDevice::tag_CreateSamplerFeedbackUnorderedAccessView:	Replay(device, p,	&ID3D12DeviceLatest::CreateSamplerFeedbackUnorderedAccessView); break;

			case 0xfffe:	break;
			case 0xffff: {

				void	*obj0	= *(void**)p;
				c = c->next();
				p = c->data();
				void	*obj	= c->id == RecDevice::tag_WaitForSingleObjectEx ? nullptr : _lookup(obj0);

				switch (c->id) {
					//ID3D12CommandQueue
					case RecDevice::tag_CommandQueueUpdateTileMappings:	Replay(obj, p, &ID3D12CommandQueue::UpdateTileMappings); break;
					case RecDevice::tag_CommandQueueCopyTileMappings:	Replay(obj, p, &ID3D12CommandQueue::CopyTileMappings); break;

					case RecDevice::tag_CommandQueueExecuteCommandLists: {
						COMParse(p, [&, queue = (ID3D12CommandQueue*)obj](UINT NumCommandLists, counted<CommandRange,0> pp) {
							auto	cmds = alloc_auto(ID3D12CommandList*, NumCommandLists);
							for (int i = 0; i < NumCommandLists; i++) {
								addr	= batch_addr;

								auto		r		= pp[i];
								auto		*rec	= assets.FindObject(uint64(&*r));
								auto		mem		= assets.GetCommands(rec->info, r);
								intptr_t	offset	= batch_addr - intptr_t(mem.p);
								auto*		cmd		= (ID3D12GraphicsCommandList*)obj2localobj[rec->obj].or_default();
								cmds[i]				= cmd;
								auto		save_cmd = save(current_cmd, cmd);

								ISO_ASSERT(cmd_infos.count(cmd) && cmd_infos[cmd].closed);
								cmd->Reset(temp_alloc[D3D12_COMMAND_LIST_TYPE_DIRECT], cmd_infos[cmd].pipeline);
								cmd_infos[cmd].queues.insert(queue);

								if (assets.bound_heaps[batch_addr].exists()) {
									const DX12Assets::BoundDescriptorHeaps& h = assets.bound_heaps[batch_addr];
									ID3D12DescriptorHeap* heaps[2] = {
										(ID3D12DescriptorHeap*)_lookup(h.cbv_srv_uav->obj),
										(ID3D12DescriptorHeap*)_lookup(h.sampler->obj),
									};
									cmd->SetDescriptorHeaps(2, heaps);
								}
								
								batch_addr	+= r.extent();

								for (const COMRecording::header *p = mem, *e = mem.end(); p < e; p = p->next()) {
									addr = intptr_t(p) + offset;
									//ISO_TRACEF("replay addr = 0x") << hex(addr) << '\n';
									if (!disabled)
										Process(cmd, p->id, p->data());
									if (!fiber::yield(addr)) {
										done = true;
										break;
									}
								}

								HRESULT	hr = cmd->Close();
								ISO_ASSERT(SUCCEEDED(hr));
							}
							queue->ExecuteCommandLists(NumCommandLists, cmds);
							//Wait(queue);


						});
						break;
					}

					case RecDevice::tag_CommandQueueSetMarker:			Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p, &ID3D12CommandQueue::SetMarker); break;
					case RecDevice::tag_CommandQueueBeginEvent:			Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p, &ID3D12CommandQueue::BeginEvent); break;
					case RecDevice::tag_CommandQueueEndEvent:			Replay(obj, p, &ID3D12CommandQueue::EndEvent); break;
					case RecDevice::tag_CommandQueueSignal:				Replay(obj, p, &ID3D12CommandQueue::Signal); break;
					case RecDevice::tag_CommandQueueWait:				Replay(obj, p, &ID3D12CommandQueue::Wait); break;

				//ID3D12CommandAllocator
					case RecDevice::tag_CommandAllocatorReset:			break;

				//ID3D12Fence
					case RecDevice::tag_FenceSetEventOnCompletion:		Replay(obj, p, &ID3D12Fence::SetEventOnCompletion); break;
					case RecDevice::tag_FenceSignal:					Replay(obj, p, &ID3D12Fence::Signal); break;
					case RecDevice::tag_WaitForSingleObjectEx:			Replay(p, [obj](DWORD dwMilliseconds, BOOL bAlertable) {
						WaitForSingleObjectEx(obj, dwMilliseconds, bAlertable);
					});
						break;

				//ID3D12GraphicsCommandList
					case RecDevice::tag_GraphicsCommandListClose: {
						auto	cmd = (ID3D12GraphicsCommandList*)obj;
						ISO_ASSERT(cmd_infos.count(cmd));
						ISO_ASSERT(validate(cmd_infos));
						cmd_infos[cmd].closed = true;
						break;
					}

					case RecDevice::tag_GraphicsCommandListReset:
						Replay(p, [this, cmd = (ID3D12GraphicsCommandList*)obj](ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
							ISO_ASSERT(cmd_infos.count(cmd) && cmd_infos[cmd].closed);
							cmd_infos[cmd].closed	= false;
							cmd_infos[cmd].pipeline	= pInitialState;
						});
						break;
				}

				break;
			}

		} } catch (const char *error) {
			const char *e = string_end(error);
			while (--e > error && *e == '\r' || *e == '\n' || *e == '.' || *e == ' ')
				;
			ISO_OUTPUTF(str(error, e + 1)) << " at offset 0x" << hex((const char*)p - (const char*)assets.device_object->info) << '\n';
		}
	}
	
	info->UnregisterMessageCallback(callback_cookie);
	info->SetMuteDebugOutput(false);
	fiber::yield(~0u);
}

dx::Resource DX12Replay::GetResource(const DESCRIPTOR &d, dx::cache_type &cache) {
	WaitAll();
	if (auto obj = assets.FindObject((uint64)d.res)) {
		if (auto res = lookup_lax((ID3D12Resource*)obj->obj)) {
			RESOURCE_DESC	rdesc	= res->GetDesc();
			DXGI_FORMAT		format	= d.get_format();
			int				sub		= d.get_sub(rdesc);
			auto			fp		= rdesc.SubFootprint(device, sub);
			auto			width	= fp.Footprint.Width, height = fp.Footprint.Height;
			auto			rr		= (DX12Assets::ResourceRecord*)obj->aux;
			auto&			states	= rr->states.curr;

			list->ResourceBarrier(1, RESOURCE_BARRIER::Transition(res, states.get(sub), D3D12_RESOURCE_STATE_COPY_SOURCE, sub));
			states.set(sub, rr->res()->NumSubresources(), D3D12_RESOURCE_STATE_COPY_SOURCE);

			uint64			total_size;
			auto			buffer	= DownloadToReadback(device, list, res, &total_size, sub);
				
			ExecuteReset();
			Wait();

			dx::Resource	r(malloc_block(const_memory_block(dx12::ResourceData(buffer), total_size)));
			r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, format, width, height, 1, 1);
//			r.set_mem(malloc_block(const_memory_block(dx12::ResourceData(buffer), total_size)));
			return r;
		}
	}
	return {};
}


ISO_ptr_machine<void> DX12Replay::GetBitmap(const char *name, const DESCRIPTOR &d) {
	WaitAll();
	if (auto obj = assets.FindObject((uint64)d.res)) {
		if (auto res = lookup_lax((ID3D12Resource*)obj->obj)) {
			RESOURCE_DESC		rdesc	= res->GetDesc();
			VIEW_DESC			view	= d;
			int					sub		= rdesc.CalcSubresource(view);
			auto				fp		= rdesc.SubFootprint(device, sub);
			auto				width	= fp.Footprint.Width, height = fp.Footprint.Height;

			auto				rr		= (DX12Assets::ResourceRecord*)obj->aux;
			auto&				states	= rr->states.curr;

			com_ptr<ID3D12Resource>	tex;
			device->CreateCommittedResource(
				//dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
				dx12::HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE, D3D12_MEMORY_POOL_L0), D3D12_HEAP_FLAG_NONE,
				dx12::RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_TEXTURE_LAYOUT_UNKNOWN, 0x10000),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr, COM_CREATE(&tex)
			);

			dx12::DescriptorHeap	dh_cpu(device, 256, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dx12::DescriptorHeapGPU	dh_gpu(device, 256, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			view.CreateShaderResourceView(device, res, dh_cpu.cpu(0));
			VIEW_DESC(tex->GetDesc()).CreateUnorderedAccessView(device, tex, dh_cpu.cpu(1));

			device->CopyDescriptorsSimple(2, dh_gpu.cpu(0), dh_cpu.cpu(0), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


			ID3D12DescriptorHeap	*heaps[]	= {dh_gpu};
			list->SetDescriptorHeaps(num_elements(heaps), heaps);
			list->SetComputeRootSignature(root_signature);
			list->SetPipelineState(pipeline_state);
			list->SetComputeRootDescriptorTable(0, dh_gpu.gpu(0));

			list->ResourceBarrier(1, RESOURCE_BARRIER::Transition(res, states.get(sub), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, sub));
			list->Dispatch(width / 8, height / 8, 1);
			list->ResourceBarrier(1, RESOURCE_BARRIER::Transition(res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, states.get(sub)));
			list->ResourceBarrier(1, RESOURCE_BARRIER::Transition(tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

		#if 1
			ExecuteReset();
			Wait();

			ISO_ptr_machine<bitmap64>	bm(name, width, height);
			auto	dest	= bm->All();
			tex->ReadFromSubresource(dest.begin(), dest.pitch(), 0, sub, nullptr);
			return bm;

		#else
			uint64			total_size;
			auto			buffer	= DownloadToReadback(device, list, tex, &total_size, sub);

			ExecuteReset();
			Wait();
			dx12::ResourceData	data(buffer, sub);
			auto	fp2		= dx12::RESOURCE_DESC(tex).SubFootprint(device, sub);

			ISO_ptr_machine<bitmap64>	bm(name, width, height);
			copy(make_strided_block((ISO_rgba*)data, width, fp2.Footprint.RowPitch, height), bm->All());
			return bm;

		#endif
		}
	}
	return none;
}


uint32 DX12Replay::GetFail() {
	if (auto bc = get_breadcrumbs()) {
		//bc.Dump(lvalue(trace_accum()));
		auto	node	= bc.find_stopped();
		uint32	num		= bc.num_succeeded(node);
		auto	&call	= assets.calls[num];
		auto	mem		= assets.GetCommands(call);
		
		const COMRecording::header *p = mem;
		for (auto& i : *node) {
			auto	t = i.tag();
			while (p != mem.end() && p->id != t)
				p = p->next();
			if (p == mem.end())
				break;
			p = p->next();
		}
		return ((const char*)p - mem) + call.dest;
	}
	return 0;
}


//-----------------------------------------------------------------------------
//	DX12Connection
//-----------------------------------------------------------------------------

struct DX12Capture : anything {};

struct DX12Connection : DX12Assets {
	dx::cache_type				cache;
	CallStackDumper				stack_dumper;
	unique_ptr<DX12Replay>		replay;
	sparse_set<uint32>			fails;
	Disassembler::SharedFiles	files;
	string						shader_path;

	hash_map<DESCRIPTOR, ISO_ptr_machine<void>, true>	bitmaps;

	using ListStates = hash_map_with_key<const ObjectRecord*, ResourceStates2>;

	uint64					CommandTotal();
	void					ScanShader(const DX12State &state, memory_interface *mem, DX12Assets::ShaderRecord *shader, const char *entry = nullptr, const DX12Assets::ObjectRecord *local_obj = nullptr, const void *local_data = nullptr);
	void					ScanCommands(DX12State &state, const_memory_block commands, uint32 index, memory_interface *mem, ListStates &list_states);
	void					ScanCommands(progress prog, uint64 total, memory_interface *mem);

	bool					GetStatistics();
	ISO_ptr_machine<void>	GetBitmap(const DESCRIPTOR &d);
	ISO_ptr_machine<void>	GetBitmap(const ResourceRecord *r) { return GetBitmap(r->DefaultDescriptor()); }
	DX12State				GetStateAt(uint32 addr);
	void					Load(const DX12Capture *cap);
	bool					Save(const filename &fn);

	auto	GetGPU(D3D12_GPU_VIRTUAL_ADDRESS addr, uint64 size) {
		return cache(addr | GPU, size);
	}
	auto	GetGPU(const D3D12_GPU_VIRTUAL_ADDRESS_RANGE &r) {
		return GetGPU(r.StartAddress, r.SizeInBytes);
	}
	auto	GetTypedBuffer(D3D12_GPU_VIRTUAL_ADDRESS addr, uint64 size, uint64 stride, SoftType format = {}) {
		return TypedBuffer(GetGPU(addr, size), stride, format);
	}
	auto	GetTypedBuffer(const D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE &r, uint32 count, SoftType format = {}) {
		return GetTypedBuffer(r.StartAddress, r.StrideInBytes * count, r.StrideInBytes, format);
	}
	auto	GetTypedBuffer(const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE &r, SoftType format = {}) {
		return GetTypedBuffer(r.StartAddress, r.SizeInBytes, r.StrideInBytes, format);
	}

	DX12Connection() : stack_dumper(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES) {}
	DX12Connection(DXConnection *con) : stack_dumper(con->process, SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES, BuildSymbolSearchPath(con->process.Filename().dir())) {
		shader_path = separated_list(transformc(GetSettings("Paths/source_path"), [](const ISO::Browser2 &i) { return i.GetString(); }), ";");
	}

	void	AddMemory(uint64 addr, const memory_block &mem)	{ cache.add_block(addr, mem); }
	uint32	FindDescriptorChange(uint64 h, uint32 stride);
	uint32	FindObjectCreation(void *obj);
};

struct DX12CaptureModule {
	uint64			a, b;
	string			fn;
	DX12CaptureModule(const CallStackDumper::Module &m) : a(m.a), b(m.b), fn(m.fn) {}
};

struct DX12CaptureObject {
	uint32			type;
	string16		name;
	xint64			obj;
	ISO_openarray<uint8,64>	info;
	DX12CaptureObject(const DX12Assets::ObjectRecord &o) : type(o.type), name(o.name), obj((uint64)o.obj), info(o.info.size32()) {
		memcpy(info, o.info, o.info.size32());
	}
};

struct DX12CaptureShader {
	uint32			stage;
	string16		name;
	xint64			addr;
	ISO_openarray<uint8,64>	code;
	DX12CaptureShader(const DX12Assets::ShaderRecord &o) : stage(o.stage), name(o.name), addr(o.addr), code(o.data.size32()) {
		memcpy(code, o.data, o.data.size32());
	}
};

ISO_DEFUSERF(DX12Capture, anything, ISO::TypeUser::WRITETOBIN);
ISO_DEFUSERCOMPFV(DX12CaptureModule, ISO::TypeUser::WRITETOBIN, a, b, fn);
ISO_DEFUSERCOMPFV(DX12CaptureObject, ISO::TypeUser::WRITETOBIN, type, name, obj, info);
ISO_DEFUSERCOMPFV(DX12CaptureShader, ISO::TypeUser::WRITETOBIN, stage, name, addr, code);

typedef ISO_openarray<DX12CaptureModule,64>	DX12CaptureModules;
typedef ISO_openarray<DX12CaptureObject,64>	DX12CaptureObjects;
typedef ISO_openarray<DX12CaptureShader,64>	DX12CaptureShaders;


bool DX12Connection::Save(const filename &fn) {
	ISO_ptr<DX12Capture>	p(0);

	p->Append(ISO_ptr<DX12CaptureModules>("Modules", stack_dumper.GetModules()));
	p->Append(ISO_ptr<DX12CaptureObjects>("Objects", objects));
	p->Append(ISO_ptr<DX12CaptureShaders>("Shaders", shaders));

	for (auto &i : cache)
		p->Append(ISO::ptr<ISO::VStartBin<memory_block>>(NULL, i.start, memory_block(i.data(), i.size())));

	return FileHandler::Write(p, fn);
}

memory_block get_memory(ISO::Browser b) {
	return memory_block(b[0], b.Count() * b[0].GetSize());
}

void DX12Connection::Load(const DX12Capture *cap) {
	ISO_ptr<DX12CaptureObjects>	_objects;
	ISO_ptr<DX12CaptureShaders>	_shaders;

	shader_path	= "";

	for (auto&& i : GetSettings("Paths/source_path")) {
		shader_path << ';' << i.GetString();
	}

	for (int i = 0, n = cap->Count(); i < n; i++) {
		ISO_ptr<void>	p = (*cap)[i];
		if (p.IsID("Modules")) {
			auto	modules = ISO::Conversion::convert<DX12CaptureModules>(p);
			for (auto &i : *modules)
				stack_dumper.AddModule(i.fn, i.a, i.b);

		} else if (p.IsID("Objects")) {
			_objects = ISO::Conversion::convert<DX12CaptureObjects>(p);

		} else if (p.IsID("Shaders")) {
			_shaders = ISO::Conversion::convert<DX12CaptureShaders>(p, ISO::Conversion::CHECK_INSIDE);

		} else {
			ISO::Browser	b(p);
			AddMemory(b[0].Get<uint64>(0), get_memory(b[1]));
		}
	}

	if (_shaders) {
		for (auto &i : *_shaders)
			AddShader((dx::SHADERSTAGE)i.stage, memory_block(i.code, i.code.size()), i.addr, string16(i.name));
	}

	if (_objects) {
		objects.reserve(_objects->Count());
		for (auto &i : *_objects)
			AddObject((RecObject::TYPE)i.type, string16(i.name), memory_block(i.info, i.info.size()), (void*)i.obj.i, nullptr);
	}
	FinishLoading();
}

struct PIX : const_memory_block {
	enum EVENT {
		UNICODE_VERSION		= 0,
		ANSI_VERSION		= 1,
		BLOB_VERSION		= 2,
	};

	enum EventType {
		EndEvent                       = 0x000,
		BeginEvent_VarArgs             = 0x001,
		BeginEvent_NoArgs              = 0x002,
		SetMarker_VarArgs              = 0x007,
		SetMarker_NoArgs               = 0x008,

		EndEvent_OnContext             = 0x010,
		BeginEvent_OnContext_VarArgs   = 0x011,
		BeginEvent_OnContext_NoArgs    = 0x012,
		SetMarker_OnContext_VarArgs    = 0x017,
		SetMarker_OnContext_NoArgs     = 0x018,
	};

	static const uint64 EndMarker		= 0x00000000000FFF80;

	uint32 metadata;

	struct BeginMarker {
		uint64	:7, : 3, type : 10, timestamp : 44;
		uint8	b, g, r, a;
	};
	struct StringMarker {
		uint64	:7, :46, short_cut:1, ansi:1, size:5, alignment:4;
	};

	PIX(uint32 metadata, const void* data, uint32 size) : const_memory_block(data, size), metadata(metadata) {}

	string get_string() const {
		switch (metadata) {
			default: return {};
			case PIX::UNICODE_VERSION:	return string((const char16*)begin(), size() / 2);
			case PIX::ANSI_VERSION:		return string((const char*)begin(), size());
			case PIX::BLOB_VERSION: {
				auto	begin	= (const BeginMarker*)*this;
				auto	str		= (const StringMarker*)(begin + 1);
				if (str->ansi)
					return string((const char*)(str + 1));
				return string((const char16*)(str + 1));
			}
		}
	}
	auto get_colour() const {
		if (metadata == BLOB_VERSION) {
			auto	begin	= (const BeginMarker*)*this;
			return begin->r | (begin->g << 8) | (begin->b << 16) | (begin->a << 24);
		}
		return 0;
	}
};

uint64 DX12Connection::CommandTotal() {
	uint32	total	= device_object ? device_object->info.size32() : 0;
	for (auto &o : objects) {
		if (o.type == RecObject::GraphicsCommandList) {
			const RecCommandList	*rec = o.info;
			total	+= rec->buffer.size32();
		}
	}
	return total;
}

void DX12Connection::ScanShader(const DX12State &state, memory_interface *mem, DX12Assets::ShaderRecord *shader, const char *entry, const DX12Assets::ObjectRecord *local_obj, const void *local_data) {
	if (!shader)
		return;

	uses.emplace_back(shaders.index_of(shader), RecObject::Shader);

	DX12RootState	_local, *local = nullptr;
	if (local_obj) {
		_local.SetRootSignature(local_obj->info);
		local = &_local;
	}

	auto	stage		= shader->stage;
	if (stage == dx::SHADER_LIB)
		stage = dx::CS;

	auto	heap		= state.bound_heaps.get(false);
	auto&	pipeline	= state.GetPipeline(stage == dx::CS);

	auto	AddUseBound	= [&](DESCRIPTOR desc, bool write) {
		switch (desc.type) {
			case DESCRIPTOR::DESCH: {
				auto	cpu		= heap->get_cpu(heap->index(desc.h));
				auto	mod		= state.mod_descriptors[cpu];
				if (mod.exists()) {
					desc = mod;
				} else {
					descriptor_used.insert(cpu);
					desc = *heap->holds(desc.h);
				}
				switch (desc.type) {
					case DESCRIPTOR::CBV:
						AddUse(desc.cbv.BufferLocation, desc.cbv.SizeInBytes, cache, nullptr);
						break;
					case DESCRIPTOR::IBV:
					case DESCRIPTOR::VBV:
						break;
				}
			}
			// fall through
			default:
				if (desc.res)
					AddUse(FindObject((uint64)desc.res), write);
				break;

			case DESCRIPTOR::PCBV:
			case DESCRIPTOR::PSRV:
			case DESCRIPTOR::PUAV:
				if (write) {
					AddUse(FindByGPUAddress(desc.ptr), write);
				} else {
					AddUseGet(desc.ptr, 0, cache, mem);

				}
				break;
		}
	};

	auto	decl	= shader->DeclResources(entry);

	for (auto &i : decl.cb)
		AddUseBound(pipeline.GetBound(stage, DESCRIPTOR::CBV, i.index(), 0, heap, local, local_data), false);

	for (auto &i : decl.srv) {
		DESCRIPTOR	desc = pipeline.GetBound(stage, DESCRIPTOR::SRV, i.index(), 0, heap, local, local_data);
		AddUseBound(desc, false);
		if (i->dim == dx::RESOURCE_DIMENSION_RTACCEL) {
			if (auto data = GetSimulatorResourceData(*this, cache, desc)) {
				using	TOP_DESC	= tuple<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS, const_memory_block>;

				auto	desc	= rmap_unique<RTM, TOP_DESC>(data);
				auto	inst	= desc->get<1>();//AddUseGet(desc->InstanceDescs, desc->NumDescs * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), cache, mem);
			
				for (auto &i : make_range<D3D12_RAYTRACING_INSTANCE_DESC>(inst)) {
					if (auto bot_data = AddUseGet(i.AccelerationStructure, 0, cache, mem)) {
						auto	bot_desc	= rmap_unique<RTM, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>(bot_data);
						for (auto& j : make_range_n(bot_desc->pGeometryDescs, bot_desc->NumDescs)) {
							if (j.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES) {
								AddUse(j.Triangles.IndexBuffer, j.Triangles.IndexCount * ByteSize(j.Triangles.IndexFormat), cache, mem);
								AddUse(j.Triangles.VertexBuffer.StartAddress, j.Triangles.VertexCount * j.Triangles.VertexBuffer.StrideInBytes, cache, mem);
							} else {
								AddUse(j.AABBs.AABBs.StartAddress, j.AABBs.AABBCount * j.AABBs.AABBs.StrideInBytes, cache, mem);
							}
						}
					}
				}
			}

		}
	}

	for (auto &i : decl.uav)
		AddUseBound(pipeline.GetBound(stage, DESCRIPTOR::UAV, i.index(), 0, heap, local, local_data), true);
}

void DX12Connection::ScanCommands(DX12State &state, const_memory_block commands, uint32 batch_addr, memory_interface *mem, ListStates &list_states) {
	uint32		marker_tos	= -1;
	uint32		begin	= batch_addr;
	intptr_t	offset	= batch_addr - intptr_t(commands.p);

	auto AddUseDescriptor = [this, &state](D3D12_CPU_DESCRIPTOR_HANDLE cpu) {
		if (!state.mod_descriptors.check(cpu))
			descriptor_used.insert(cpu);
	};

	auto AddUseDescriptorState = [this, &state, &list_states](D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_RESOURCE_STATES states, bool use) {
		ObjectRecord	*heap;
		if (auto	d = state.GetDescriptor(this, cpu, &heap)) {
			Used(heap);
			if (auto *obj = FindObject((uint64)d->res)) {
				ISO_ASSERT(obj->type == RecObject::Resource);
				auto	rr		= (DX12Assets::ResourceRecord*)obj->aux;
				auto	res		= rr->res();
				auto	subs	= res->NumSubresources(nullptr);
				auto	sub		= d->get_sub(*res);

				auto	planes	= NumPlanes(res->Format);
				while (sub < subs) {
					list_states[obj]->init.set_init(sub, subs, states);
					sub += subs / planes;
				}

				if (use)
					AddUse(obj, states & D3D12_RESOURCE_STATE_WRITE);
				else
					obj->Used(states & D3D12_RESOURCE_STATE_WRITE);
			}
		}
		if (!state.mod_descriptors.check(cpu))
			descriptor_used.insert(cpu);
	};

	auto AddUseGraphics = [this, &state, mem]() {
		AddUse(state.bound_heaps.cbv_srv_uav);
		AddUse(state.bound_heaps.sampler);
		AddUse(state.graphics_pipeline.obj);
		AddUse(FindObject((uint64)state.graphics_pipeline.root_signature));
		for (auto& i : state.graphics_pipeline.shaders)
			ScanShader(state, mem, AddShader((dx::SHADERSTAGE)index_of(state.graphics_pipeline.shaders, i), i, mem));

		for (uint32 i = 0, n = state.graphics_pipeline.RTVFormats.NumRenderTargets; i < n; i++) {
			if (const DESCRIPTOR *d = state.GetDescriptor(this, state.targets[i]))
				AddUse(FindObject((uint64)d->res), true);
		}

		if (const DESCRIPTOR *d = state.GetDescriptor(this, state.depth))
			AddUse(FindObject((uint64)d->res), true);

		for (auto &i : state.vbv)
			AddUse(i.BufferLocation, i.SizeInBytes, cache, mem);

	};

	auto AddUseCompute = [this, &state, mem]() {
		AddUse(state.bound_heaps.cbv_srv_uav);
		AddUse(state.bound_heaps.sampler);
		AddUse(state.compute_pipeline.obj);
		AddUse(FindObject((uint64)state.compute_pipeline.root_signature));
		ScanShader(state, mem, AddShader(dx::CS, state.compute_pipeline.CS, mem));
	};

	BoundDescriptorHeaps	unbound_heaps;

	for (const COMRecording::header *p = commands, *e = commands.end(); p < e; p = p->next()) {
		uint32	addr	= intptr_t(p) + offset;
		uint32	next	= intptr_t(p->next()) + offset;
		state.ProcessCommand(this, p->id, p->data());

		switch (p->id) {
			case RecCommandList::tag_SetMarker: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
				PIX	pix(Metadata, pData, Size);
				AddMarker(pix.get_string(), next, next, pix.get_colour());
				});
				break;
			case RecCommandList::tag_BeginEvent: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
				PIX	pix(Metadata, pData, Size);
				marker_tos = AddMarker(pix.get_string(), next, marker_tos, pix.get_colour());
				});
				break;
			case RecCommandList::tag_EndEvent: COMParse(p->data(), [&]() {
				if (marker_tos >= 0) {
					if (marker_tos < markers.size()) {
						uint32	next1 = markers[marker_tos].b;
						markers[marker_tos].b = next;
						marker_tos	= next1;
					}
				} else {
					marker_tos = 0;
				}
				});
				break;

			case RecCommandList::tag_Close:
			case RecCommandList::tag_Reset:
				begin = next;
				break;
				/*
			case RecCommandList::tag_SetPipelineState:
				COMParse(p->data(), [&](ID3D12PipelineState* pPipelineState) {
					AddUse(pPipelineState);
				});
				break;
				*/
			case RecCommandList::tag_SetComputeRootDescriptorTable:
			case RecCommandList::tag_SetGraphicsRootDescriptorTable:
				COMParse(p->data(), [&](UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
					for (auto &i : descriptor_heaps) {
						if (i->holds(BaseDescriptor)) {
							bool	samp = i->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
							if (!state.bound_heaps.get(samp)) {
								unbound_heaps.set(samp, i.obj);
								state.bound_heaps.set(samp, i.obj);
							}
							break;
						}
					}
				});
				break;

			case RecCommandList::tag_DrawIndexedInstanced:
				AddUse(state.ibv.BufferLocation, state.ibv.SizeInBytes, cache, mem);
			case RecCommandList::tag_DrawInstanced:
			case RecCommandList::tag_DispatchMesh:
				AddUseGraphics();
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_Dispatch:
				AddUseCompute();
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_ExecuteIndirect:
				AddUse(state.indirect.signature);
				AddUse(state.indirect.arguments);
				AddUse(state.indirect.counts);

				if (state.IsGraphics())
					AddUseGraphics();
				else
					AddUseCompute();

				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_ResourceBarrier: COMParse2(p->data(), [&](UINT NumBarriers, counted<const D3D12_RESOURCE_BARRIER, 0> pBarriers) {
				for (const D3D12_RESOURCE_BARRIER *p = pBarriers, *e = p + NumBarriers; p != e; ++p) {
					switch (p->Type) {
						case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
							if (auto *obj = FindObject((uint64)p->Transition.pResource)) {
								ISO_ASSERT(obj->type == RecObject::Resource);
								auto	rr		= (DX12Assets::ResourceRecord*)obj->aux;
								auto	sub		= p->Transition.Subresource;
								auto	subs	= rr->res()->NumSubresources(nullptr);
								auto&	states	= list_states[obj].put();
								auto	cur		= states.curr.get(sub);

								ISO_ASSERTFI(cur == D3D12_RESOURCE_STATE_UNKNOWN || cur == p->Transition.StateBefore, "@0x%2[%1]: Bad transtion for %0", obj, p - pBarriers, hex(addr));
								ISO_ASSERTFI(rr->res()->validate_state(sub, p->Transition.StateAfter), "@0x%2[%1]: Missing states on %0", obj, p - pBarriers, hex(addr));
								states.curr.set(sub, subs, p->Transition.StateAfter);
								states.init.set_init(sub, subs, p->Transition.StateBefore);

								obj->Used(p->Transition.StateBefore & D3D12_RESOURCE_STATE_WRITE);
								obj->Used(p->Transition.StateAfter & D3D12_RESOURCE_STATE_WRITE);

								rr->events.push_back({ResourceRecord::Event::BARRIER, addr + ((char*)p - (char*)pBarriers.p)});
							}
							break;

							case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
								Used(FindObject((uint64)p->Aliasing.pResourceBefore));
								Used(FindObject((uint64)p->Aliasing.pResourceAfter));
								break;

							case D3D12_RESOURCE_BARRIER_TYPE_UAV:
								Used(FindObject((uint64)p->Transition.pResource));
								break;
					}
				}
				});
				break;

			case RecCommandList::tag_OMSetRenderTargets: COMParse(p->data(), [&](UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) {
				for (int i = 0; i < NumRenderTargetDescriptors; i++)
					AddUseDescriptorState(pRenderTargetDescriptors[i], D3D12_RESOURCE_STATE_RENDER_TARGET, false);
				if (pDepthStencilDescriptor)
					AddUseDescriptorState(*pDepthStencilDescriptor, D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
				});
				break;

			case RecCommandList::tag_ClearDepthStencilView: COMParse(p->data(), [&](D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects) {
				AddUseDescriptorState(DepthStencilView, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_ClearRenderTargetView: COMParse(p->data(), [&](D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT* pRects) {
				AddUseDescriptorState(RenderTargetView, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_ClearUnorderedAccessViewUint: COMParse(p->data(), [&](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, counted<const D3D12_RECT,4> pRects) {
				auto	d1		= state.GetDescriptor(this, ViewCPUHandle);
				auto	heap	= state.bound_heaps.get(false);
				ISO_ASSERT(heap->holds(ViewGPUHandleInCurrentHeap));
				auto	cpu2	= heap->get_cpu(heap->index(ViewGPUHandleInCurrentHeap));
				auto	d2		= state.GetDescriptor(this, cpu2);
				//ISO_ASSERT(d1->type == d2->type);
				AddUseDescriptor(ViewCPUHandle);
				AddBatch(state, begin, addr, next);
				begin = next;
				});
				break;
			case RecCommandList::tag_ClearUnorderedAccessViewFloat: COMParse(p->data(), [&](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, counted<const D3D12_RECT,4> pRects) {
				AddUseDescriptor(ViewCPUHandle);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_CopyBufferRegion: COMParse(p->data(), [this](ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes) {
				AddUse(FindObject((uint64)pDstBuffer), true);
				AddUse(FindObject((uint64)pSrcBuffer), false);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_CopyTextureRegion: COMParse2(p->data(), [this](const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox) {
				AddUse(FindObject((uint64)pDst), true);
				AddUse(FindObject((uint64)pSrc), false);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_CopyResource: COMParse(p->data(), [this](ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource) {
				AddUse(FindObject((uint64)pDstResource), true);
				AddUse(FindObject((uint64)pSrcResource), false);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_CopyTiles: COMParse(p->data(), [this](ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
				AddUse(FindObject((uint64)pTiledResource), true);
				AddUse(FindObject((uint64)pBuffer), false);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_ResolveSubresource: COMParse(p->data(), [this](ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) {
				AddUse(FindObject((uint64)pDstResource), true);
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_BeginQuery: COMParse(p->data(), [this](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
				AddUse(FindObject((uint64)pQueryHeap));
				});
				break;
			case RecCommandList::tag_EndQuery: COMParse(p->data(), [this](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
				AddUse(FindObject((uint64)pQueryHeap));
				});
				break;
			case RecCommandList::tag_ResolveQueryData: COMParse(p->data(), [this](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset) {
				AddUse(FindObject((uint64)pQueryHeap));
				AddUse(FindObject((uint64)pDestinationBuffer), true);
				});
				break;
			case RecCommandList::tag_SetPredication: COMParse(p->data(), [this](ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation) {
					AddUse(FindObject((uint64)pBuffer), true);
				});
				break;
			case RecCommandList::tag_WriteBufferImmediate: COMParse(p->data(), [this](UINT Count, counted<const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER, 0> pParams, const D3D12_WRITEBUFFERIMMEDIATE_MODE* pModes) {
				for (int i = 0; i < Count; i++)
					AddUse(FindByGPUAddress(pParams[i].Dest));
				});
				AddBatch(state, begin, addr, next);
				begin = next;
				break;

			case RecCommandList::tag_DispatchRays:
				AddUseCompute();
				
				COMParse(p->data(), [this, mem, &state](const D3D12_DISPATCH_RAYS_DESC *desc) {
					for (auto &sub : state.compute_pipeline.StateSubObjects()) {
						if (sub.Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY)
							AddShader(dx::SHADER_LIB, get<D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY>(sub)->DXILLibrary, mem);
					}

					string_param	name;

					if (auto raygen_table = AddUseGet(desc->RayGenerationShaderRecord.StartAddress, desc->RayGenerationShaderRecord.SizeInBytes, cache, mem)) {
						if (auto rec = state.compute_pipeline.FindShader(this, *(SHADER_ID*)raygen_table, name))
							ScanShader(state, mem, rec, name, FindObject((uint64)state.compute_pipeline.LocalPipeline(name)), raygen_table + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
					}
					//hitgroups
					if (auto hitgroup_table = AddUseGet(desc->HitGroupTable.StartAddress, desc->HitGroupTable.SizeInBytes, cache, mem)) {
						for (auto &id : make_strided<SHADER_ID>(hitgroup_table, desc->HitGroupTable.StrideInBytes)) {
							if (auto hitgroup = state.compute_pipeline.FindHitGroup(id)) {
								auto	local_obj	= FindObject((uint64)state.compute_pipeline.LocalPipeline(hitgroup->HitGroupExport));
	
								if (hitgroup->AnyHitShaderImport)
									ScanShader(state, mem, state.compute_pipeline.FindShader(this, hitgroup->AnyHitShaderImport), name, local_obj, &id + 1);

								if (hitgroup->ClosestHitShaderImport)
									ScanShader(state, mem, state.compute_pipeline.FindShader(this, hitgroup->ClosestHitShaderImport), name, local_obj, &id + 1);

								if (hitgroup->IntersectionShaderImport)
									ScanShader(state, mem, state.compute_pipeline.FindShader(this, hitgroup->IntersectionShaderImport), name, local_obj, &id + 1);
							}
						}
					}

					//miss
					if (auto miss_table = AddUseGet(desc->MissShaderTable.StartAddress, desc->MissShaderTable.SizeInBytes, cache, mem)) {
						for (auto &id : make_strided<SHADER_ID>(miss_table, desc->MissShaderTable.StrideInBytes)) {
							if (auto rec = state.compute_pipeline.FindShader(this, id, name))
								ScanShader(state, mem, rec, name, FindObject((uint64)state.compute_pipeline.LocalPipeline(name)), &id + 1);
						}
					}

					//callable
					if (auto callable_table	= AddUseGet(desc->CallableShaderTable.StartAddress, desc->CallableShaderTable.SizeInBytes, cache, mem)) {
						for (auto &id : make_strided<SHADER_ID>(callable_table, desc->CallableShaderTable.StrideInBytes)) {
							if (auto rec = state.compute_pipeline.FindShader(this, id, name))
								ScanShader(state, mem, rec, name, FindObject((uint64)state.compute_pipeline.LocalPipeline(name)), &id + 1);
						}
					}
				});

				AddBatch(state, begin, addr, next);
				begin = next;
				break;
		}
	}

	if (unbound_heaps.sampler || unbound_heaps.cbv_srv_uav) {
		bound_heaps[batch_addr] = unbound_heaps;
	}
}

void DX12Connection::ScanCommands(progress prog, uint64 total, memory_interface *mem) {
	DX12State	state;
	batches.reset();
	
	if (device_object) {
		uint32	batch_addr	= device_object->info.size32();
		uint32	marker_tos	= -1;

		for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); p < e; p	= p->next()) {
			uint32	offset = uint32((char*)p - (char*)device_object->info.p);
			if (p->id == 0xffff) {
				auto	obj	= FindObject(*(uint64*)p->data());
				Used(obj);

				p		= p->next();
				offset	= uint32((char*)p - (char*)device_object->info.p);

				switch (p->id) {
				//ID3D12CommandQueue
					case RecDevice::tag_CommandQueueSetMarker:	COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
						PIX	pix(Metadata, pData, Size);
						AddMarker(pix.get_string(), offset, offset, pix.get_colour());
					}); break;

					case RecDevice::tag_CommandQueueBeginEvent:	COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
						PIX	pix(Metadata, pData, Size);
						marker_tos = AddMarker(pix.get_string(), offset, marker_tos, pix.get_colour());
					}); break;

					case RecDevice::tag_CommandQueueEndEvent:	COMParse(p->data(), [&]() {
						if (marker_tos >= 0) {
							if (marker_tos < markers.size()) {
								uint32	next = markers[marker_tos].b;
								markers[marker_tos].b = offset;
								marker_tos	= next;
							}
						} else {
							marker_tos = 0;
						}
					}); break;

					case RecDevice::tag_CommandQueueSignal:		COMParse(p->data(), [&](ID3D12Fence *pFence, UINT64 Value) { Used(FindObject(uint64(pFence)), true); }); break;
					case RecDevice::tag_CommandQueueWait:		COMParse(p->data(), [&](ID3D12Fence *pFence, UINT64 Value) { Used(FindObject(uint64(pFence)), false); }); break;

					case RecDevice::tag_CommandQueueExecuteCommandLists: COMParse(p->data(), [&](UINT NumCommandLists, counted<CommandRange,0> pp) {

						ListStates	list_states;

						for (int i = 0; i < NumCommandLists; i++) {
							const CommandRange	&r	= pp[i];
							if (auto obj = FindObject(uint64(&*r))) {
								obj->Used(false);
								AddCall(offset, batch_addr, i);
								ScanCommands(state, GetCommands(obj->info, r), batch_addr, mem, list_states);
							}
							batch_addr += r.extent();
						}

						for (auto& i : list_states) {
							auto rr = (DX12Assets::ResourceRecord*)i.a->aux;
							rr->states.init.combine_init(i.b.init);
						}


					}); break;

				//ID3D12CommandAllocator
					case RecDevice::tag_CommandAllocatorReset:			break;

				//ID3D12Fence
					case RecDevice::tag_FenceSetEventOnCompletion:		COMParse(p->data(), [&](UINT64 Value, HANDLE hEvent) { Used(FindObject(uint64(hEvent)), false); }); break;
					//case RecDevice::tag_FenceSignal:					COMParse(p->data(), [&](UINT64 Value) { Used(obj0, true); }); break;
					//case RecDevice::tag_WaitForSingleObjectEx:		COMParse(p->data(), [&](DWORD dwMilliseconds, BOOL bAlertable) { Used(obj0, false); }); break;

				//ID3D12GraphicsCommandList
					case RecDevice::tag_GraphicsCommandListReset:		COMParse(p->data(), [&](ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
						Used(FindObject((uint64)pAllocator), true);
						Used(FindObject((uint64)pInitialState), false);
					}); break;
				}
			} else {
				switch (p->id) {
					case RecDevice::tag_CopyDescriptors: COMParse(p->data(), [&](UINT dest_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0> dest_starts, counted<const UINT,0> dest_sizes, UINT srce_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3> srce_starts, counted<const UINT,3> srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type)  {
						for (auto &desc : dest_starts(dest_num))
							Used(FindDescriptorHeapObject(desc), true);

						for (auto i : with_index(srce_starts(srce_num))) {
							auto	desc	= *i;
							auto	heap	= FindDescriptorHeapObject(desc);
							UINT	stride	= ((RecDescriptorHeap*)heap->info)->stride;
							Used(heap);
							for (UINT ssize = srce_sizes ? srce_sizes[i.index()] : 1; ssize--;) {
								if (!state.mod_descriptors.check(desc))
									descriptor_used.insert(desc);
								desc.ptr += stride;
							}
						}
					}); break;

					case RecDevice::tag_CopyDescriptorsSimple: COMParse(p->data(), [&](UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
						Used(FindDescriptorHeapObject(dest_start), true);
						auto	heap	= FindDescriptorHeapObject(srce_start);
						UINT	stride	= ((RecDescriptorHeap*)heap->info)->stride;
						Used(heap);
						while (num--) {
							if (!state.mod_descriptors.check(srce_start))
								descriptor_used.insert(srce_start);
							srce_start.ptr += stride;
						}
					}); break;
				}
				state.ProcessDevice(this, p->id, p->data());
			}
		}
	}
	AddBatch(state, total, total, total);	//dummy end batch

	for (auto &i : resources) {
		auto	res		= i.res();
		auto	subs	= res->NumSubresources(nullptr);
		auto	hprops	= res->HeapProps();

		D3D12_RESOURCE_STATES default_state = 
			hprops.Type == D3D12_HEAP_TYPE_UPLOAD	? D3D12_RESOURCE_STATE_GENERIC_READ
			: hprops.Type == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST
			: D3D12_RESOURCE_STATE_COMMON;
	#if 0
		if (res->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER || (res->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)) {
			if (!(i.states.init.state & D3D12_RESOURCE_STATE_DEPTH))
				i.states.init.state = D3D12_RESOURCE_STATE_COMMON;
			for (auto& s : i.states.init.substates) {
				if (!(*s & D3D12_RESOURCE_STATE_DEPTH))
					s = D3D12_RESOURCE_STATE_COMMON;
			}
		}
	#endif
		i.states.init.compact(subs, default_state);
		ISO_ASSERT(i.states.init.state != D3D12_RESOURCE_STATE_UNKNOWN);
	}
}

bool DX12Connection::GetStatistics() {
#if 0	// for hp2
	fails.insert({
		LastOp(batches[10]),
		LastOp(batches[21]),
		LastOp(batches[64]),
		LastOp(batches[106]),
		LastOp(batches[118])
	});
#endif
	for (;;) {
		if (!replay)
			replay = new DX12Replay(*this, cache);

		uint32		num	= batches.size32();

		com_ptr<ID3D12Resource>	timestamp_dest, stats_dest;
		com_ptr<ID3D12QueryHeap> timestamp_heap, stats_heap;

		if (!SUCCEEDED(replay->device->CreateQueryHeap(dx12::QUERY_HEAP_DESC(D3D12_QUERY_HEAP_TYPE_TIMESTAMP, num), COM_CREATE(&timestamp_heap)))
			||	!SUCCEEDED(replay->device->CreateCommittedResource(
				dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
				dx12::RESOURCE_DESC::Buffer(num * sizeof(uint64)),
				D3D12_RESOURCE_STATE_COPY_DEST, 0, COM_CREATE(&timestamp_dest)
			)))
			return false;

		if (!SUCCEEDED(replay->device->CreateQueryHeap(dx12::QUERY_HEAP_DESC(D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS, num), COM_CREATE(&stats_heap)))
			||	!SUCCEEDED(replay->device->CreateCommittedResource(
				dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
				dx12::RESOURCE_DESC::Buffer(num * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)),
				D3D12_RESOURCE_STATE_COPY_DEST, 0, COM_CREATE(&stats_dest)
			)))
			return false;

		fiber_generator<uint32>	fg([this]() { replay->RunGen(); }, 1 << 17);
		auto	p = fg.begin();

		for (auto &b : batches) {
			int		i		= batches.index_of(b);
			bool	last	= &b == &batches.back();
			//ISO_TRACEF("Batch %i\n", i);

			if (!last) {
				while (*p < b.begin)
					++p;
			}

			auto	cmd = replay->current_cmd;
			if (!cmd)
				break;

			cmd->EndQuery(timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, i);

			if (last) {
				cmd->ResolveQueryData(timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, num, timestamp_dest, 0);
				cmd->ResolveQueryData(stats_heap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0, num - 1, stats_dest, 0);
				while (*p < b.end)
					++p;
				break;
			}

			cmd->BeginQuery(stats_heap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, i);
			while (*p < b.end) {
				if (fails.count(*p)) {
					replay->disabled = true;
					++p;
					replay->disabled = false;
				} else {
					++p;
				}
			}
			cmd->EndQuery(stats_heap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, i);

		}

		replay->WaitAll();

		if (auto failed_at = replay->GetFail()) {
			ISO_OUTPUTF("Failed at 0x%08x: Batch %u\n", failed_at, batches.index_of(GetBatch(failed_at)));
			fails.insert(failed_at);
			replay.clear();
			continue;
		}

		uint64									*timestamps = nullptr;
		D3D12_QUERY_DATA_PIPELINE_STATISTICS	*stats		= nullptr;

		if (SUCCEEDED(timestamp_dest->Map(0, nullptr, (void**)&timestamps)) && SUCCEEDED(stats_dest->Map(0, nullptr, (void**)&stats))) {
			uint64	*tp = timestamps;
			for (auto &i : batches) {
				i.timestamp = *tp++ - timestamps[0];
				i.stats		= *stats++;
			}

			return true;
		}
		replay.clear();
		return false;
	}
}

DX12State DX12Connection::GetStateAt(uint32 addr) {
	DX12State	state;
	if (device_object) {
		uint32		batch_addr	= device_object->info.size32();
		if (addr >= batch_addr) {
			bool	done = false;
			for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); !done && p < e; p = p->next()) {
				if (p->id == 0xffff) {
					p = p->next();

					switch (p->id) {
						case RecDevice::tag_CommandQueueExecuteCommandLists:
							COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
								for (int i = 0; !done && i < NumCommandLists; i++) {
									const CommandRange	&r	= pp[i];
									if (r.extent()) {
										if (bound_heaps[batch_addr].exists())
											state.bound_heaps = bound_heaps[batch_addr];

										auto	mem		= GetCommands(r).slice_to(addr - batch_addr);
										for (const COMRecording::header *p = mem, *e = mem.end(); p < e; p = p->next())
											state.ProcessCommand(this, p->id, p->data());

										done = mem.length() == addr - batch_addr;
										batch_addr += r.extent();
									}
								}
							});
							break;
					}

				} else {
					state.ProcessDevice(this, p->id, p->data());
				}
			}
		}
	}
	return state;
}

ISO_ptr_machine<void> DX12Connection::GetBitmap(const DESCRIPTOR &d) {
	auto	p = bitmaps[d];
	if (p.exists())
		return p;

	if (auto obj = FindObject((uint64)d.res)) {
		auto	res = MakeSimulatorResource(*this, cache, d);
		if (res && is_texture(res.dim)) {
			p = dx::GetBitmap(obj->GetName(), res, res.format, res.width, res.height, res.depth, res.mips, 0);
			return p;
		}
	}
	return ISO_NULL;
}

template<typename F> bool CheckView(uint64 h, const void *p) {
	typedef	typename function<F>::P	P;
	typedef map_t<RTM, P>			RTL1;
	return h == ((TL_tuple<RTL1>*)p)->template get<RTL1::count - 1>().ptr;
}
template<typename F> bool CheckView(uint64 h, const void *p, F f) {
	return CheckView<F>(h, p);
}

uint32 DX12Connection::FindDescriptorChange(uint64 h, uint32 stride) {
	if (device_object) {
		bool	found = false;
		for (auto &c : make_next_range<const COMRecording::header>(device_object->info)) {
			switch (c.id) {
				case RecDevice::tag_CreateConstantBufferView:		found = CheckView(h, c.data(), &ID3D12Device::CreateConstantBufferView); break;
				case RecDevice::tag_CreateShaderResourceView:		found = CheckView(h, c.data(), &ID3D12Device::CreateShaderResourceView); break;
				case RecDevice::tag_CreateUnorderedAccessView:		found = CheckView(h, c.data(), &ID3D12Device::CreateUnorderedAccessView); break;
				case RecDevice::tag_CreateRenderTargetView:			found = CheckView(h, c.data(), &ID3D12Device::CreateRenderTargetView); break;
				case RecDevice::tag_CreateDepthStencilView:			found = CheckView(h, c.data(), &ID3D12Device::CreateDepthStencilView); break;

				case RecDevice::tag_CopyDescriptors: found = COMParse(c.data(), [h, stride](UINT dest_num, const D3D12_CPU_DESCRIPTOR_HANDLE *dest_starts, const UINT *dest_sizes, UINT srce_num, const D3D12_CPU_DESCRIPTOR_HANDLE *srce_starts, const UINT *srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type) {
					for (UINT s = 0, d = 0; d < dest_num; d++) {
						if (h >= dest_starts[d].ptr && h < dest_starts[d].ptr + dest_sizes[d] * stride)
							return true;
					}
					return false;
				}); break;

				case RecDevice::tag_CopyDescriptorsSimple: found = COMParse(c.data(), [h, stride](UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
					return h >= dest_start.ptr && h < dest_start.ptr + num * stride;
				}); break;
			}
			if (found)
				return (uint32)device_object->info.offset_of(&c);
		}
	}
	return 0;
}

template<typename F> bool CheckCreated(void *obj, const void *p) {
	typedef	typename function<F>::P	P;
	typedef map_t<PM, P>			RTL1;
	auto	r	= *((TL_tuple<RTL1>*)p)->template get<RTL1::count - 1>();
	return (IUnknown*)r == obj;
}
template<typename F> bool CheckCreated(void *obj, const void *p, F f) {
	return CheckCreated<F>(obj, p);
}

uint32 DX12Connection::FindObjectCreation(void *obj) {
	if (device_object) {
		bool	found = false;
		for (auto &c : make_next_range<const COMRecording::header>(device_object->info)) {
			switch (c.id) {
				case RecDevice::tag_CreateCommandQueue:				found = CheckCreated(obj, c.data(), &ID3D12Device::CreateCommandQueue); break;
				case RecDevice::tag_CreateCommandAllocator:			found = CheckCreated(obj, c.data(), &ID3D12Device::CreateCommandAllocator); break;
				case RecDevice::tag_CreateGraphicsPipelineState:	found = CheckCreated(obj, c.data(), &ID3D12Device::CreateGraphicsPipelineState); break;
				case RecDevice::tag_CreateComputePipelineState:		found = CheckCreated(obj, c.data(), &ID3D12Device::CreateComputePipelineState); break;
				case RecDevice::tag_CreateCommandList:				found = CheckCreated(obj, c.data(), &ID3D12Device::CreateCommandList); break;
				case RecDevice::tag_CreateDescriptorHeap:			found = CheckCreated(obj, c.data(), &ID3D12Device::CreateDescriptorHeap); break;
				case RecDevice::tag_CreateRootSignature:			found = CheckCreated<HRESULT(UINT, counted<uint8, 2>, SIZE_T, REFIID, void**)>(obj, c.data()); break;
				case RecDevice::tag_CreateCommittedResource:		found = CheckCreated(obj, c.data(), &ID3D12Device::CreateCommittedResource); break;
				case RecDevice::tag_CreateHeap:						found = CheckCreated(obj, c.data(), &ID3D12Device::CreateHeap); break;
				case RecDevice::tag_CreatePlacedResource:			found = CheckCreated(obj, c.data(), &ID3D12Device::CreatePlacedResource); break;
				case RecDevice::tag_CreateReservedResource:			found = CheckCreated(obj, c.data(), &ID3D12Device::CreateReservedResource); break;
				case RecDevice::tag_CreateSharedHandle:				found = CheckCreated(obj, c.data(), &ID3D12Device::CreateSharedHandle); break;
				case RecDevice::tag_CreateFence:					found = CheckCreated(obj, c.data(), &ID3D12Device::CreateFence); break;
				case RecDevice::tag_CreateQueryHeap:				found = CheckCreated(obj, c.data(), &ID3D12Device::CreateQueryHeap); break;
				case RecDevice::tag_CreateCommandSignature:			found = CheckCreated(obj, c.data(), &ID3D12Device::CreateCommandSignature); break;
			}
			if (found)
				return (uint32)device_object->info.offset_of(&c);
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	DX12StateControl
//-----------------------------------------------------------------------------

typedef dx::DXBC::BlobT<dx::DXBC::RootSignature>	RootSignatureBlob;

template<> const char *field_names<RootSignatureBlob::VISIBILITY>::s[]		= {"ALL", "VERTEX", "HULL", "DOMAIN", "GEOMETRY", "PIXEL" };
template<> const char *field_names<RootSignatureBlob::Range::TYPE>::s[]		= { "SRV", "UAV", "CBV", "SMP" };
template<> const char *field_names<RootSignatureBlob::Parameter::TYPE>::s[]	= { "TABLE", "CONSTANTS", "CBV", "SRV", "UAV" };

template<> static constexpr bool field_is_struct<dx::TextureFilterMode> = false;
template<> struct field_names<dx::TextureFilterMode>		: field_names<D3D12_FILTER> {};
template<> struct field_names<dx::TextureAddressMode>		: field_names<D3D12_TEXTURE_ADDRESS_MODE> {};
template<> struct field_names<dx::TextureBorderColour>		: field_names<D3D12_STATIC_BORDER_COLOR> {};
template<> struct field_names<dx::ComparisonFunction>		: field_names<D3D12_COMPARISON_FUNC> {};
template<> struct field_names<RootSignatureBlob::FLAGS>		: field_names<D3D12_HEAP_FLAGS> {};

template<> struct field_names<RootSignatureBlob::Descriptor::FLAGS>			{ static field_bit s[];	};
field_bit field_names<RootSignatureBlob::Descriptor::FLAGS>::s[]	= {
	{"NONE",								0	},
	{"DESCRIPTORS_VOLATILE",				1	},
	{"DATA_VOLATILE",						2	},
	{"DATA_STATIC_WHILE_SET_AT_EXECUTE",	4	},
	{"DATA_STATIC",							8	},
	0
};
MAKE_FIELDS(RootSignatureBlob::Sampler1, filter, address_u, address_v, address_w, mip_lod_bias, max_anisotropy, comparison_func, border, min_lod, max_lod, reg, space, visibility);
MAKE_FIELDS(RootSignatureBlob::Constants, base, space, num);
MAKE_FIELDS(RootSignatureBlob::Descriptor, reg, space, flags);
MAKE_FIELDS(RootSignatureBlob::Range, type, num, base, space, offset);
MAKE_FIELDS(RootSignatureBlob::Parameter, type, visibility);

struct DX12StateControl : ColourTree {
	enum TYPE {
		ST_SHADER	= 1,
		ST_OBJECT,
		ST_VERTICES,
		ST_OUTPUTS,
		ST_DESCRIPTOR,
		ST_DESCRIPTOR_REF,
		ST_GPU_ADDRESS,
	};
	static win::Colour	colours[];
	static uint8		cursor_indices[][3];

	DX12Connection *con;

	DX12StateControl(DX12Connection *con) : ColourTree(colours, DX12cursors, cursor_indices), con(con) {}

	HTREEITEM	AddShader(HTREEITEM h, const DX12ShaderState &shader);

	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		buffer_accum<256>	ba;
		PutFieldName(ba, tree.format, pf);

		uint64	v = pf->get_raw_value(p, offset);
		if (pf->is_type<D3D12_GPU_VIRTUAL_ADDRESS2>()) {
			tree.Add(h, ba << "0x" << hex(v) << con->ObjectName(v), 0, addr + offset);

		} else if (auto *rec = con->FindObject(v))
			tree.Add(h, ba << rec->name, ST_OBJECT, rec);
		else if (auto *rec = con->FindShader(v))
			tree.Add(h, ba << rec->name, ST_SHADER, rec);
		else
			tree.Add(h, ba << "0x" << hex(v), 0, addr + offset);
	}
};

win::Colour DX12StateControl::colours[] = {
	{0,0,0},
	{128,0,64},		//ST_SHADER
	{0,128,64},		//ST_OBJECT,
	{64,0,128},		//ST_VERTICES
	{0,0,0},		//ST_OUTPUTS,
	{0,128,128},	//ST_DESCRIPTOR,
	{0,128,64},		//ST_DESCRIPTOR_REF,
	{128,0,64},		//ST_GPU_ADDRESS,
};
uint8 DX12StateControl::cursor_indices[][3] = {
	{0,0,0},
	{1,2,0},		//ST_SHADER
	{1,2,0},		//ST_OBJECT,
	{1,2,0},		//ST_VERTICES
	{1,2,0},		//ST_OUTPUTS,
	{1,2,0},		//ST_DESCRIPTOR,
	{1,2,0},		//ST_DESCRIPTOR_REF,
	{1,2,0},		//ST_GPU_ADDRESS,
};


//-----------------------------------------------------------------------------
//	Shader Reflection
//-----------------------------------------------------------------------------

const C_type *to_c_type(ID3D12ShaderReflectionType *type) {
	D3D12_SHADER_TYPE_DESC	type_desc;
	if (FAILED(type->GetDesc(&type_desc)))
		return nullptr;

	if (type_desc.Class != D3D_SVC_STRUCT)
		return to_c_type(type_desc.Class, type_desc.Type, type_desc.Rows, type_desc.Columns);

	C_type_struct	ctype(type_desc.Name);
	for (int i = 0, n = type_desc.Members; i < n; i++) {
		type->GetMemberTypeByIndex(i)->GetDesc(&type_desc);
		ctype.add_atoffset(type->GetMemberTypeName(i), to_c_type(type->GetMemberTypeByIndex(i)), type_desc.Offset);
	}
	return ctypes.add(move(ctype));
}

const C_type *to_c_type(ID3D12ShaderReflectionConstantBuffer *cb) {
	D3D12_SHADER_BUFFER_DESC	cb_desc;
	if (FAILED(cb->GetDesc(&cb_desc)))
		return nullptr;

	C_type_struct	ctype(cb_desc.Name);
	for (int i = 0; i < cb_desc.Variables; i++) {
		ID3D12ShaderReflectionVariable*		var = cb->GetVariableByIndex(i);
		D3D12_SHADER_VARIABLE_DESC			var_desc;
		if (SUCCEEDED(var->GetDesc(&var_desc)) && (var_desc.uFlags & D3D_SVF_USED))
			ctype.add_atoffset(var_desc.Name, ::to_c_type(var->GetType()), var_desc.StartOffset);
	}
	return ctypes.add(move(ctype));
}

void AddShaderResource(RegisterTree &rt, HTREEITEM h, const dxil::meta_resources::record_srv_uac &res, const dxil::TypeSystem &types, const DESCRIPTOR *bound) {
	using namespace dxil;

	auto	type	= res.type->subtype;
	while (type->type == Type::Array)
		type = type->subtype;

	string_builder	a;
	a << res.shape << ' ' << res.name;

	switch (res.shape) {
		case ResourceKind::StructuredBuffer: {
			auto first = type->members[0];
			if (first->type == Type::Struct && !is_matrix(first))
				type = first;
		}
		case ResourceKind::TBuffer: {
			auto	ctype	= to_c_type(res.name, type, types);
			auto	h2		= StructureHierarchy(rt, h, ctypes, a, ctype, 0, nullptr);
			rt.SetParam(h2, DX12StateControl::ST_DESCRIPTOR, bound);
			if (bound)
				((const void**)(bound + 1))[-1] = ctype;	//HACK!!
			break;
		}

		case ResourceKind::RTAccelerationStructure: {
			auto	h2 = rt.Add(h, a, DX12StateControl::ST_VERTICES, bound);
			if (bound)
				rt.AddFields(h2, bound);
			break;
		}

		default: {
			auto	h2 = rt.Add(h, a, DX12StateControl::ST_DESCRIPTOR, bound);
			if (bound) {
				((const void**)(bound + 1))[-1] = nullptr;	//HACK!!
				rt.AddFields(h2, bound);
			}
			break;
		}
	}
}

void AddShaderResources(RegisterTree &rt, HTREEITEM h, const bitcode::Module &mod, const DX12ShaderState &shader, const DX12Connection *con) {
	using namespace dxil;

	if (auto m  = mod.GetMeta("dx.resources")) {
		meta_resources	res(m[0]);
		meta_entryPoint entry(mod.GetMetaFirst("dx.entryPoints"));
		TypeSystem		types(mod, entry.tags.find(entry.ShaderFlagsTag));

		for (auto &i : res.cbv) {
			auto	type	= i.type->subtype;
			if (type->type == Type::Array)
				type = type->subtype;

			if (auto bound = shader.cbv.check(i.lower)) {
				if (bound->type != DESCRIPTOR::NONE) {
					auto	data	= MakeSimulatorConstantBuffer(con->cache, *bound);
					auto	ctype	= to_c_type(i.name, type, types);
					auto	h2		= StructureHierarchy(rt, h, ctypes, "", ctype, 0, data);
					rt.SetParam(h2, DX12StateControl::ST_DESCRIPTOR, bound);

					((const void**)(bound + 1))[-1] = ctype;	//HACK!!
				}
			}
		}

		for (auto &i : res.srv)
			AddShaderResource(rt, h, i, types, shader.srv.check(i.lower));

		for (auto &i : res.uav)
			AddShaderResource(rt, h, i, types, shader.uav.check(i.lower));
	}

}

void AddShaderResources(RegisterTree &rt, HTREEITEM h, ID3D12ShaderReflection *refl, const DX12ShaderState &shader, const DX12Connection *con) {
	D3D12_SHADER_DESC	desc;
	refl->GetDesc(&desc);

	for (int i = 0; i < desc.BoundResources; i++) {
		D3D12_SHADER_INPUT_BIND_DESC	res_desc;
		refl->GetResourceBindingDesc(i, &res_desc);

		DESCRIPTOR *bound = 0;
		switch (res_desc.Type) {
			case D3D_SIT_CBUFFER:		bound = shader.cbv.check(res_desc.BindPoint); break;
			case D3D_SIT_TBUFFER:
			case D3D_SIT_TEXTURE:
			case D3D_SIT_STRUCTURED:	bound = shader.srv.check(res_desc.BindPoint); break;
			case D3D_SIT_SAMPLER:		bound = shader.smp.check(res_desc.BindPoint); break;
			default:					bound = shader.uav.check(res_desc.BindPoint); break;
		}

		if (bound) {
			buffer_accum<256>	ba;

			char	reg = 0;
			switch (res_desc.Type) {
				case D3D_SIT_CBUFFER:		reg = 'b'; break;
				case D3D_SIT_TBUFFER:
				case D3D_SIT_TEXTURE:
				case D3D_SIT_STRUCTURED:	reg = 't'; break;
				case D3D_SIT_SAMPLER:		reg = 's'; break;
				default:					reg = 'u'; break;
			}

			ba << reg << res_desc.BindPoint << ':';

			if (res_desc.Name[0])
				ba << res_desc.Name;
			else
				ba << to_string(res_desc.Type);

			if (bound->type == DESCRIPTOR::SRV || bound->type == DESCRIPTOR::UAV)
				ba << con->ObjectName(bound);

			HTREEITEM	h3 = rt.Add(h, ba, DX12StateControl::ST_DESCRIPTOR, bound);
			rt.AddFields(rt.AddText(h3, "Descriptor"), bound);

			switch (bound->type) {
				case DESCRIPTOR::CBV:
					StructureHierarchy(rt, h3, ctypes, "", to_c_type(refl->GetConstantBufferByIndex(res_desc.BindPoint)), 0, con->cache(bound->cbv.BufferLocation, bound->cbv.SizeInBytes));
					break;

				case DESCRIPTOR::PCBV:
					StructureHierarchy(rt, h3, ctypes, "", to_c_type(refl->GetConstantBufferByIndex(res_desc.BindPoint)), 0, con->cache(bound->ptr));
					break;

				case DESCRIPTOR::IMM:
					StructureHierarchy(rt, h3, ctypes, "", to_c_type(refl->GetConstantBufferByIndex(res_desc.BindPoint)), 0, bound->imm);
					break;

				default: {
					auto	type = to_c_type(refl->GetConstantBufferByName(res_desc.Name));
					((const void**)(bound + 1))[-1] = type;	//HACK!!
					break;
				}
			}
		}
	}
}

void AddShaderRegisters(RegisterTree &rt, HTREEITEM h, const DX12ShaderState &shader) {
	for (auto &i : shader.cbv)
		rt.AddFields(rt.AddText(h, "b" + to_string(i.index())), &i.t);

	for (auto &i : shader.srv)
		rt.AddFields(rt.AddText(h, "t" + to_string(i.index())), &i.t);

	for (auto &i : shader.uav)
		rt.AddFields(rt.AddText(h, "u" + to_string(i.index())), &i.t);

	for (auto &i : shader.smp)
		rt.AddFields(rt.AddText(h, "s" + to_string(i.index())), &i.t);

}

struct MyDxcBlob : com<IDxcBlob> {
	const_memory_block	b;
public:
	LPVOID STDMETHODCALLTYPE GetBufferPointer()	{ return unconst(b.begin()); }
	SIZE_T STDMETHODCALLTYPE GetBufferSize()	{ return b.size(); }

	MyDxcBlob() {}
	MyDxcBlob(const_memory_block b) : b(b) {}
	MyDxcBlob(IDxcBlob *blob)		: b(blob->GetBufferPointer(), blob->GetBufferSize()) {}
};

HTREEITEM AddShader(RegisterTree &rt, HTREEITEM h, const DX12ShaderState &shader, DX12Connection *con) {
	com_ptr<ID3D12ShaderReflection>	refl;
	D3D12_SHADER_DESC				desc;

	if (FAILED(D3DReflect(shader.data, shader.data.length(), IID_ID3D12ShaderReflection, (void**)&refl))) {
		com_ptr<IDxcContainerReflection>	dxc_refl;
		DxcCreateInstance(CLSID_DxcContainerReflection, COM_CREATE(&dxc_refl));

		MyDxcBlob	blob(shader.data);
		dxc_refl->Load(&blob);
		
		uint32 partIndex;
		dxc_refl->FindFirstPartKind("DXIL"_u32, &partIndex);
		dxc_refl->GetPartReflection(partIndex, COM_CREATE(&refl));
	}

	if (refl && SUCCEEDED(refl->GetDesc(&desc))) {
		HTREEITEM	h1 = rt.AddText(h, "ID3D12ShaderReflection");
		rt.AddFields(rt.AddText(h1, "D3D12_SHADER_DESC"), &desc);

		HTREEITEM	h2 = rt.AddText(h1, "ConstantBuffers");
		for (int i = 0; i < desc.ConstantBuffers; i++) {
			ID3D12ShaderReflectionConstantBuffer*	cb = refl->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC				cb_desc;
			cb->GetDesc(&cb_desc);

			HTREEITEM	h3 = rt.AddText(h2, cb_desc.Name);
			rt.AddText(h3, format_string("size = %i", cb_desc.Size));

			for (int i = 0; i < cb_desc.Variables; i++) {
				ID3D12ShaderReflectionVariable*		var = cb->GetVariableByIndex(i);
				D3D12_SHADER_VARIABLE_DESC			var_desc;
				var->GetDesc(&var_desc);

				HTREEITEM	h4 = rt.AddText(h3, var_desc.Name);
				rt.AddText(h4, format_string("Slot = %i", var->GetInterfaceSlot(0)));

				rt.AddFields(rt.AddText(h4, "D3D12_SHADER_VARIABLE_DESC"), &var_desc);

				D3D12_SHADER_TYPE_DESC				type_desc;
				ID3D12ShaderReflectionType*			type = var->GetType();
				type->GetDesc(&type_desc);
				rt.AddFields(rt.AddText(h4, "D3D12_SHADER_TYPE_DESC"), &type_desc);
			}
		}

		h2 = rt.AddText(h1, "BoundResources");
		for (int i = 0; i < desc.BoundResources; i++) {
			D3D12_SHADER_INPUT_BIND_DESC	res_desc;
			refl->GetResourceBindingDesc(i, &res_desc);
			rt.AddFields(rt.AddText(h2, res_desc.Name[0] ? res_desc.Name : to_string(res_desc.Type)), &res_desc);
		}

		h2 = rt.AddText(h1, "InputParameters");
		for (int i = 0; i < desc.InputParameters; i++) {
			D3D12_SIGNATURE_PARAMETER_DESC	in_desc;
			refl->GetInputParameterDesc(i, &in_desc);
			rt.AddFields(rt.AddText(h2, in_desc.SemanticName), &in_desc);
		}

		h2 = rt.AddText(h1, "OutputParameters");
		for (int i = 0; i < desc.OutputParameters; i++) {
			D3D12_SIGNATURE_PARAMETER_DESC	out_desc;
			refl->GetOutputParameterDesc(i, &out_desc);
			rt.AddFields(rt.AddText(h2, out_desc.SemanticName), &out_desc);
		}

	}


	bitcode::Module mod;
	if (ReadDXILModule(mod, shader.DXBC()->GetBlob<dx::DXBC::ShaderDebugInfoDXIL>())) {
		AddShaderResources(rt, rt.AddText(h, "Bound Resources"), mod, shader, con);
	} else if (refl && desc.BoundResources) {
		AddShaderResources(rt, rt.AddText(h, "Bound Resources"), refl, shader, con);
	} else {
		AddShaderRegisters(rt, rt.AddText(h, "Bound Registers"), shader);
	}

	if (auto* rts0 = shader.DXBC()->GetBlob<RootSignatureBlob>()) {
		auto	h1 = rt.AddText(h, "Root Signature");
		HTREEITEM	h3 = rt.AddText(h1, "Parameters");

		for (auto &i : rts0->parameters.entries(rts0)) {
			HTREEITEM	h4 = rt.AddText(h3, "Parameter");
			rt.AddFields(h4, &i);
			void		*p = i.ptr.get(rts0);
			switch (i.type) {
				case RootSignatureBlob::Parameter::TABLE: {
					auto	*p2 = (RootSignatureBlob::DescriptorTable*)p;
					for (auto &i : p2->entries(rts0))
						rt.AddFields(rt.AddText(h4, "Range"), &i);
					break;
				}
				case RootSignatureBlob::Parameter::CONSTANTS:
					rt.AddFields(h4, (RootSignatureBlob::Constants*)p);
					break;

				default:
					rt.AddFields(h4, (RootSignatureBlob::Descriptor*)p);
					break;
			}
		}

		rt.AddArray(rt.AddText(h1, "Samplers"), rts0->samplers.entries(rts0));
	}

	return h;
}

HTREEITEM DX12StateControl::AddShader(HTREEITEM h, const DX12ShaderState &shader) {
	return ::AddShader(lvalue(RegisterTree(*this, this)), h, shader, con);
}

//-----------------------------------------------------------------------------
//	DX12ShaderWindow
//-----------------------------------------------------------------------------

struct DX12ShaderWindow : SplitterWindow, CodeHelper {
	DX12StateControl	tree;
	TabControl3			*tabs[2];
	Disassembler::Files source;

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	DX12ShaderWindow(const WindowPos &wpos, DX12Connection *_con, const DX12ShaderState &shader, bool non_debug);
};

LRESULT	DX12ShaderWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_COMMAND:
			if (HIWORD(wParam) == EN_SETFOCUS && tabs[0] && tabs[1]) {
				Select((HWND)lParam, (EditControl)tabs[0]->GetSelectedControl(), tabs[1]);
				return 0;
			}
			break;

		case WM_KEYDOWN:
			if (wParam == VK_SPACE) {
				mode ^= Disassembler::SHOW_SOURCE;
				UpdateDisassembly(tabs[0]->GetItemControl(0), source);
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					break;
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

DX12ShaderWindow::DX12ShaderWindow(const WindowPos &wpos, DX12Connection *con, const DX12ShaderState &shader, bool non_debug)
	: SplitterWindow(SWF_VERT | SWF_PROP)
	, CodeHelper(HLSLcolourerRE(), none, GetSettings("General/shader source").Get(Disassembler::SHOW_SOURCE))
	, tree(con)
{
	Create(wpos, shader.name, CHILD | CLIPCHILDREN | VISIBLE);
	Rebind(this);

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY, 100);
	split2->Create(GetPanePos(0), 0, CHILD | CLIPCHILDREN | VISIBLE);

	// code tabs
	tabs[0] = new TabControl3(split2->_GetPanePos(0), "code", CHILD | CLIPSIBLINGS);
	tabs[0]->SetFont(win::Font::DefaultGui());

//	D2DTextWindow *tw = new D2DTextWindow(split2->_GetPanePos(0), "Original", CHILD | TextWindow::READONLY);
	D2DTextWindow *tw = new D2DTextWindow(tabs[0]->GetChildWindowPos(), "Original", CHILD | TextWindow::READONLY);

	bool	dxil = shader.IsDXIL();

	if (dxil) {
		bitcode::Module mod;
		ISO_VERIFY(
			(!non_debug && ReadDXILModule(mod, shader.DXBC()->GetBlob<dx::DXBC::ShaderDebugInfoDXIL>()))
			|| ReadDXILModule(mod, shader.DXBC()->GetBlob<dx::DXBC::DXIL>())
		);

//		FileOutput("D:\\test1.bin").write(shader.DXBC()->GetBlob(dx::DXBC::DXIL));
//		FileOutput("D:\\test2.bin").write(mod);
//		FileOutput("D:\\test2.dxbc").write(ReplaceDXIL(shader.DXBC(), mod, false));

//		GetDXILSource(mod, source, con->files);
//		GetDXILSourceLines(mod, source, locations, shader.entry);
		RemapFromHashLine(source, con->shader_path, con->files);

		FixLocations(0);
		SetDisassembly(*tw, Disassemble(const_memory_block(&mod), -1, true), source);

	#if 0
		// test writing
		{
			//FileOutput("D:\\test.dxbc").write(shader.data);

			dynamic_memory_writer	out(0x10000);
			out.write(bitcode::Module::MAGIC);
			bitcode::wvlc	vw(out);
			mod.write(bitcode::BlockWriter(vw).enterBlock(bitcode::MODULE_BLOCK_ID, 3));

			bitcode::Module mod2;
			memory_reader	mr(out.data());
			uint32 magic	= mr.get<uint32>();
			bitcode::vlc			vr(mr);
			bitcode::BlockReader	r(vr);
			uint32					code;
			if (r.nextRecord(code) == bitcode::ENTER_SUBBLOCK && code == bitcode::MODULE_BLOCK_ID)
				mod2.read(r.enterBlock(nullptr));

			D2DTextWindow *tw2 = new D2DTextWindow(tabs[0]->GetChildWindowPos(), "Written", CHILD | TextWindow::READONLY);
			SetDisassembly(*tw2, Disassemble(const_memory_block(&mod2), -1, true), source);
			tabs[0]->AddItemControl(*tw2);
		}
	#endif
	} else {
		auto		spdb = ParsedSPDB(memory_reader(shader.DXBC()->GetBlob(dx::DXBC::ShaderPDB)), con->files, con->shader_path);
		locations	= move(spdb.locations);
		source		= move(spdb.files);
		FixLocations(shader.GetUCodeAddr());
		SetDisassembly(*tw, Disassemble(shader.GetUCode(), shader.GetUCodeAddr(), false), source);
	}

	tw->Show();
	tabs[0]->AddItemControl(*tw, "Disassembly");

	com_ptr<ID3DBlob>			blob;
	if (SUCCEEDED(D3DDisassemble(shader.data, shader.data.length(), D3D_DISASM_ENABLE_COLOR_CODE, 0, &blob))) {
		void	*p		= blob->GetBufferPointer();
		size_t	size	= blob->GetBufferSize();
		tabs[0]->AddItemControl(MakeHTMLViewer(GetChildWindowPos(), "Shader", (const char*)p, size), "DirectX");
		tabs[0]->SetSelectedIndex(0);

	} else {
		com_ptr<IDxcLibrary>	lib;
		com_ptr<IDxcCompiler2>	compiler;
		DxcCreateInstance(CLSID_DxcLibrary, COM_CREATE(&lib));
		DxcCreateInstance(CLSID_DxcCompiler, COM_CREATE(&compiler));

		com_ptr<IDxcBlobEncoding>	source;
		lib->CreateBlobWithEncodingFromPinned(shader.data, shader.data.length(), CP_ACP, &source);

		com_ptr<IDxcBlobEncoding>	blob2;
		compiler->Disassemble(source, &blob2);

		auto	p		= (const char*)blob2->GetBufferPointer();
		size_t	size	= blob2->GetBufferSize();

		D2DTextWindow *tw2 = new D2DTextWindow(tabs[0]->GetChildWindowPos(), "DirectX", CHILD | TextWindow::READONLY);
		tw2->SetFont(colourer.font);
		tw2->SetText(p, size);
		tabs[0]->AddItemControl(*tw2);

//		tabs[0]->AddItemControl(MakeHTMLViewer(GetChildWindowPos(), "Shader", p, size), "DirectX");
		tabs[0]->SetSelectedIndex(0);
	}

	// tree
	split2->SetPanes(*tabs[0], tree.Create(split2->_GetPanePos(1), none, CHILD | VISIBLE | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT), 100);
//	tree.AddShader(TVI_ROOT, shader);
	tabs[0]->Show();

	// source tabs
	if (!source.empty()) {
		tabs[1] = new TabControl3(GetPanePos(1), "source", CHILD | VISIBLE);
		tabs[1]->SetFont(win::Font::DefaultGui());
		SourceTabs(*tabs[1], source);
		SetPanes(*split2, *tabs[1], 50);
		tabs[1]->ShowSelectedControl();
	} else {
		tabs[1] = 0;
		SetPanes(*split2, Control());
	}
}

Control MakeShaderViewer(const WindowPos &wpos, DX12Connection *con, const DX12ShaderState &shader, bool non_debug) {
	return *new DX12ShaderWindow(wpos, con, shader, non_debug);
}

//-----------------------------------------------------------------------------
//	DX12ShaderDebuggerWindow
//-----------------------------------------------------------------------------

class DX12ShaderDebuggerWindow : public MultiSplitterWindow, DebugWindow {
	Accelerator				accel;
	TabControl3				*tabs		= nullptr;
	DXRegisterWindow		*regs		= nullptr;
	DXLocalsWindow			*locals		= nullptr;
	LocalsWindow			*globals	= nullptr;
	DX12StateControl		tree;
	ComboControl			thread_control;
	
	//malloc_block			flat_global;
	dynamic_array<uint64>	bp_offsets;
	const void				*op;
	uint32					step_count	= 0, prev_step_count = 0;
	uint32					thread		= 0;
	uint8					running		= 0;

	bool	FindBreakpoint(int32 offset) {
		auto	i = lower_boundc(bp_offsets, offset);
		return i != bp_offsets.end() && *i == offset;
	}
	void	SetBreakpoint(int32 offset) {
		auto	i = lower_boundc(bp_offsets, offset);
		if (i == bp_offsets.end() || *i != offset)
			bp_offsets.insert(i, offset);
	}

	void	ClearBreakpoint(int32 offset) {
		auto	i = lower_boundc(bp_offsets, offset);
		if (i != bp_offsets.end() && *i == offset)
			bp_offsets.erase(i);
	}

	void	ToggleBreakpoint(int32 offset) {
		auto	i = lower_boundc(bp_offsets, offset);
		if (i == bp_offsets.end() || *i != offset)
			bp_offsets.insert(i, offset);
		else
			bp_offsets.erase(i);
	}

	void	Step(bool src, bool over);
public:
	unique_ptr<dx::SimulatorDX> sim;
	
	DX12ShaderDebuggerWindow(const WindowPos &wpos, const char *title, DX12ShaderState &shader, unique_ptr<dx::SimulatorDX> &&sim, DX12Connection *con, Disassembler::MODE mode);

	uint64		Address(const void *p)	const	{ return p ? base + sim->Offset(p) : 0; }
	Control&	control()						{ return MultiSplitterWindow::control(); }

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void		Update();

	void	SetThread(int _thread)	{
		thread = _thread;

//		for (int i = 0; i < sim->NumThreads(); i++)
//			thread_control.SetItemData(i, thread_control.Add(to_string(i)));
		thread_control.Select(thread);

		op = sim->Run(0);
		Update();
	}

	void	ShowPC(uint32 pc) {
		if (mode & Disassembler::STEP_SOURCE) {
			if (auto loc = OffsetToSource(pc)) {
				int	i = tabs->FindControlByID(loc->file);
				if (i >= 0) {
					tabs->SetSelectedIndex(i);
					EditControl edit	= tabs->GetItemControl(i);
					ShowSourceLine(edit, loc->line);
				}
			}

		} else {
			tabs->SetSelectedIndex(0);
			DebugWindow::SetSelection(GetLineStart(OffsetToLine(pc)));
			DebugWindow::EnsureVisible();
			DebugWindow::Invalidate(Margin());
		}
	}
};

void DX12ShaderDebuggerWindow::Update() {
	uint32	pc	= sim->Offset(op);
	ShowPC(pc);

#if 0
	SetPC(OffsetToLine(pc));
	if (auto loc = OffsetToSource(pc)) {
		int	i = tabs->FindControlByID(loc->file);
		if (i >= 0) {
			tabs->SetSelectedIndex(i);
			EditControl edit	= tabs->GetItemControl(i);
			ShowSourceLine(edit, loc->line);
		}
	}
#endif
	if (regs)
		regs->Update(pc, pc + base, thread);
	
	if (locals)
		locals->Update(pc, pc + base, thread);

	if (globals) {
		//auto	sim2	= (dx::SimulatorDXIL*)sim.get();
		//uint8*	dest	= flat_global.end();
		//for (auto *n = sim2->group_shared.head; n; n = n->next) {
		//	dest -= n->allocated().size();
		//	n->allocated().copy_to(dest);
		//}
		globals->Update();
	}
}

void DX12ShaderDebuggerWindow::Step(bool src, bool over) {
	if (src) {
		auto	start	= sim->Offset(op);
		auto	loc		= OffsetToSource(start);
		auto	funcid	= over && loc ? loc->func_id : 0;

		while (auto runto = NextSourceOffset(start, funcid)) {
			do {
				++step_count;
				op	= sim->Continue(op, 1);
			} while (op && sim->Offset(op) < runto && sim->Offset(op) >= start);

			if (!op || (sim->ThreadFlags(thread) & dx::SimulatorDX::THREAD_ACTIVE))
				return;

			start	= sim->Offset(op);
		}
	}
	do {
		++step_count;
		op	= sim->Continue(op, 1);
	} while (op && (!(sim->ThreadFlags(thread) & dx::SimulatorDX::THREAD_ACTIVE) || LegalAddress(Address(op)) != Address(op)));
}



DX12ShaderDebuggerWindow::DX12ShaderDebuggerWindow(const WindowPos &wpos, const char *title, DX12ShaderState &shader, unique_ptr<dx::SimulatorDX> &&_sim, DX12Connection *con, Disassembler::MODE mode)
	: MultiSplitterWindow(2, SWF_VERT | SWF_PROP | SWF_DELETE_ON_DESTROY)
	, DebugWindow(HLSLcolourerRE(), none, mode)
	, accel(GetAccelerator())
	, tree(con)
	, sim(move(_sim))
{
	accel.Add(VK_F11,	FVIRTKEY | FNOINVERT,	DebugWindow::ID_DEBUG_STEPIN);
	accel.Add(VK_F11,	FVIRTKEY | FSHIFT,		DebugWindow::ID_DEBUG_STEPOUT);

	MultiSplitterWindow::Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, CLIENTEDGE);// | COMPOSITED);

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY, 100);
	split2->Create(GetPanePos(0), 0, CHILD | VISIBLE);

	tabs = new TabControl3(split2->_GetPanePos(0), "source", CHILD | VISIBLE);
	tabs->SetFont(win::Font::DefaultGui());
	tabs->AddItemControl(DebugWindow::Create(tabs->GetChildWindowPos(), "Disassembly", CHILD | READONLY | SELECTIONBAR));

	tree.Create(split2->_GetPanePos(1), none, CHILD | VISIBLE | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT);
	tree.AddShader(TVI_ROOT, shader);

	tree.Show();

	if (shader.IsDXIL()) {
		const bitcode::Module *mod = sim->GetUCode();
		GetDXILSource(*mod, DebugWindow::files, con->files);
		GetDXILSourceLines(*mod, DebugWindow::files, DebugWindow::locations, shader.entry);
		DebugWindow::RemapFromHashLine(DebugWindow::files, con->shader_path, con->files);

		DebugWindow::FixLocations(0);
		DebugWindow::SetDisassembly(Disassemble(sim->GetUCode(), -1, true), true);

		if (HasFiles())
			CodeHelper::SourceTabs(*tabs, files);

		SplitterWindow	*split3			= new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
		split3->Create(GetPanePos(NumPanes() - 1), 0, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);

		auto	sim2	= (dx::SimulatorDXIL*)sim.get();
		regs	= new DXILRegisterWindow((new TitledWindow(split3->GetPanePos(0), "Registers"))->GetChildWindowPos(), sim2);
		locals	= new DXILLocalsWindow((new TitledWindow(split3->GetPanePos(1), "Locals"))->GetChildWindowPos(), sim2, ctypes);

		if (auto total = sim2->group_shared.total_alloc()) {
			globals	= new LocalsWindow(AppendPane(32768), "globals", ctypes);
			for (auto& i : sim2->globals)
				globals->AppendEntry(i.a->name, to_c_type(i.a->type->subtype), (uint64)i.b, true);

			//flat_global.resize(total);
			//globals = BinaryWindow(AppendPane(32768), ISO::MakeBrowser(flat_global));
			//SetPane(NumPanes() - 1, globals);
		}

	} else {
		auto	spdb		= make_shared<ParsedSPDB>(memory_reader(shader.DXBC()->GetBlob(dx::DXBC::ShaderPDB)), con->files, con->shader_path);
		auto	ucode_addr	= shader.GetUCodeAddr();

		DebugWindow::locations	= move(spdb->locations);
		DebugWindow::files		= move(spdb->files);
		DebugWindow::FixLocations(ucode_addr);
		DebugWindow::SetDisassembly(Disassemble(sim->GetUCode(), ucode_addr, false), true);

		// source tabs
		if (HasFiles())
			CodeHelper::SourceTabs(*tabs, files);

		SplitterWindow	*split3			= new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
		split3->Create(GetPanePos(NumPanes() - 1), 0, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);

		TitledWindow	*regs_title		= new TitledWindow(split3->GetPanePos(0), "Registers");
		TitledWindow	*locals_title	= new TitledWindow(split3->GetPanePos(1), "Locals");

		auto	sim2	= (dx::SimulatorDXBC*)sim.get();
		regs	= new DXBCRegisterWindow(regs_title->GetChildWindowPos(), sim2, spdb);
		locals	= new DXBCLocalsWindow(locals_title->GetChildWindowPos(), sim2, spdb, ctypes);
	}


#if 1
	thread_control.Create(control(), "thread", CHILD | OVERLAPPED | VISIBLE | VSCROLL | thread_control.DROPDOWNLIST | thread_control.HASSTRINGS, NOEX,
		Rect(wpos.rect.Width() - 64, 0, 64 - GetNonClientMetrics().iScrollWidth, GetNonClientMetrics().iSmCaptionHeight),
		'TC'
	);
	thread_control.SetFont(win::Font::SmallCaption());
	thread_control.MoveAfter(HWND_TOP);
#endif
	DebugWindow::Show();
	MultiSplitterWindow::Rebind(this);
}

LRESULT DX12ShaderDebuggerWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			SetAccelerator(control(), accel);
			return 0;
			
		case WM_SIZE: {
			Point	size(lParam);
			thread_control.Move(Point(size.x - 64, 0));
			break;
		}

		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_LBUTTONDOWN:
					SetAccelerator(control(), accel);
					if (regs)
						regs->RemoveOverlay();
					break;
			}
			return 0;

		case WM_CONTEXTMENU:
			ISO_TRACE("Hello");
			break;

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case 'TC':
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						thread = thread_control.Selected();
						Update();
						return 0;
					}
					break;

				case DebugWindow::ID_DEBUG_RUN:
					if (!running) {
						running = 1;
						RunThread([this]() {
							while (running == 1 && op) {
								++step_count;
								op	= sim->Continue(op, 1);

								if (sim->ThreadFlags(thread) & dx::SimulatorDX::THREAD_ACTIVE) {
									uint32	offset	= sim->Offset(op);
									auto	i		= lower_boundc(bp_offsets, offset);
									if (i != bp_offsets.end() && *i == offset)
										break;
								}

								//InvalidateMargin();
							}
							running = 0;
							JobQueue::Main().add([this] {
								Update();
							});
						});
					}
					return 0;

				case DebugWindow::ID_DEBUG_STEPIN:
				case DebugWindow::ID_DEBUG_STEPOVER:
					if (running) {
						running = 2;

					} else if (op) {
						prev_step_count	= step_count;
						Step(test_any(mode, Disassembler::STEP_SOURCE), id == ID_DEBUG_STEPOVER);
						Update();
					}
					return 0;

				case DebugWindow::ID_DEBUG_STEPBACK:
					if (running) {
						running = 2;

					} else if (prev_step_count < 10000) {
						//mode		-= Disassembler::STEP_SOURCE;
						step_count	= 0;
						op			= sim->Run(0);
						bool	src	= test_any(mode, Disassembler::STEP_SOURCE);

						for (uint32 target = prev_step_count; step_count < target;) {
							prev_step_count	= step_count;
							Step(src, false);
						}
						Update();
					}
					return 0;

				case DebugWindow::ID_DEBUG_BREAKPOINT: {
					EditControl	edit	= tabs->GetSelectedControl();
					int			line	= HIWORD(wParam) == 1 ? edit.GetLine(edit.GetSelection().cpMin) : lParam;
					int			i		= tabs->GetSelectedIndex();
					uint32		offset	= 0;

					if (i == 0) {
						offset	= LineToOffset(line);

					} else {
						++line;
						int		file	= (*nth(files.begin(), i - 1)).a;
						auto	locs	= locations.find_all(file, line);
						if (locs.empty() || abs((int)locs.front()->line - line) > 4)
							return 0;

						sort(locs, [](auto a, auto b) { return a->offset < b->offset; });
						uint32	next = ~0;
						auto	d	= locs.begin();
						for (auto i = d; i != locs.end(); ++i) {
							if ((*i)->offset != next)
								*d++ = *i;
							next = NextOffset((*i)->offset);
						}
						locs.erase(d, locs.end());
						offset	= locs.front()->offset;

						if (locs.size() > 1) {
							if (FindBreakpoint(offset)) {
								for (auto i : locs)
									ClearBreakpoint(i->offset);
							} else {
								for (auto i : locs)
									SetBreakpoint(i->offset);

							}
							edit.Invalidate(Margin());
							return 0;
						}
					
					}

					ToggleBreakpoint(offset);
					edit.Invalidate(Margin());
					return 0;
				}

				case DebugWindow::ID_DEBUG_SWITCHSOURCE:
					mode ^= Disassembler::STEP_SOURCE;
					ShowPC(sim->Offset(op));
					return 0;

				default:
					if (HIWORD(wParam) == EN_SETFOCUS) {
						SetAccelerator(control(), accel);
						return 0;
					}
					break;
			}
			break;
			
		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case d2d::PAINT_INFO::CODE: {
					auto	*info	= (d2d::PAINT_INFO*)nmh;
					uint32	pc		= sim->Offset(op);
						if (int id = nmh->idFrom) {
						uint32	pc_line	= ~0;
						if (auto loc = OffsetToSource(pc)) {
							if ((uint16)loc->file == id)
								pc_line = loc->line - 1;
						}
						dynamic_array<uint32>	bp1 = transformc(bp_offsets, [this, id](uint32 offset)->uint32 {
							if (auto loc = OffsetToSource(offset)) {
								if ((uint16)loc->file == id)
									return loc->line - 1;
							}
							return ~0;
						});
						app::DrawBreakpoints(nmh->hwndFrom, info->target, pc_line, bp1, mode & Disassembler::SHOW_LINENOS);

					} else {
						dynamic_array<uint32>	bp1 = transformc(bp_offsets, [this](uint32 offset)->uint32 { return OffsetToLine(offset); });
						app::DrawBreakpoints(nmh->hwndFrom, info->target, OffsetToLine(pc), bp1);
					}
					return 0;
				}

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					break;

				case NM_CLICK:
					if (wParam == 'RG')
						regs->AddOverlay((NMITEMACTIVATE*)lParam);
					break;
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return MultiSplitterWindow::Proc(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	DX12BatchWindow
//-----------------------------------------------------------------------------

Control MakeObjectView(const WindowPos &wpos, DX12Connection *con, const DX12Assets::ObjectRecord *rec);
Control MakeDescriptorView(const WindowPos &wpos, const char *title, DX12Connection *con, const DESCRIPTOR *desc);
Control MakeDescriptorHeapView(const WindowPos& wpos, DX12Connection* con, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc);
Control MakeRayTraceView(const WindowPos& wpos, const char *title, DX12Connection* con, const DESCRIPTOR *desc);

void AddRootState(RegisterTree &rt, HTREEITEM h, const DX12RootState &root, const DX12State &state, const void *root_data) {
	auto root_desc = root.GetRootSignatureDesc();
	if (!root_desc)
		return;

	auto h2 = rt.AddText(h, "Parameters");

	for (int i = 0; i < root_desc->NumParameters; i++) {
		auto&	param	= root_desc->pParameters[i];
		string_builder	ba;
		ba << i << ": ";

		switch (param.ParameterType) {
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
				HTREEITEM	h3		= rt.AddText(h2, ba << "Table: vis=" << get_field_name(param.ShaderVisibility));
				auto		dho		= param.DescriptorTable.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? state.bound_heaps.sampler : state.bound_heaps.cbv_srv_uav;
				const RecDescriptorHeap *dh		= dho ? (const RecDescriptorHeap*)dho->info : nullptr;
				const DESCRIPTOR		*desc	= dh ? dh->holds(root.slot[i].get(root_data)) : nullptr;
				uint32					start	= 0;

				for (auto &range : make_range_n(param.DescriptorTable.pDescriptorRanges, param.DescriptorTable.NumDescriptorRanges)) {
					static const char *types[] = {
						"SRV", "UAV", "CBV", "SMP"
					};
					char	reg_prefix = "tucs"[range.RangeType];
					if (range.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
						start = range.OffsetInDescriptorsFromTableStart;

					buffer_accum<256>	ba;
					ba << types[range.RangeType] << "space " << range.RegisterSpace;
					HTREEITEM	h4 = desc
						? rt.Add(h3, ba << " @ " << dho->name << '[' << dh->index(desc + start) << ']', DX12StateControl::ST_DESCRIPTOR_REF, dh->get_cpu(dh->index(desc + start)))
						: rt.AddText(h3, ba << " (unmapped)");

					uint32	n = range.NumDescriptors == UINT_MAX ? 8 : range.NumDescriptors;
					for (int i = 0; i < n; i++) {
						auto	name	= format_string("%c%i", reg_prefix, range.BaseShaderRegister + i);
						if (desc) {
							auto	desc2	= desc + start;
							auto	cpu		= dh->get_cpu(dh->index(desc2));
							auto	mod		= state.mod_descriptors[cpu];
							if (mod.exists())
								desc2	= &mod;
							if (desc2->is_valid(range.RangeType))
								rt.AddFields(rt.Add(h4, name, DX12StateControl::ST_DESCRIPTOR, desc2), desc2);

						} else {
							rt.AddText(h4, name);
						}
						++start;
					}

				}
				break;
			}
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				rt.AddText(h2, ba << "Constants: vis=" << get_field_name(param.ShaderVisibility));
				break;

			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
			case D3D12_ROOT_PARAMETER_TYPE_UAV: {
				HTREEITEM	h3		= rt.AddText(h2, ba << "Descriptor: vis=" << get_field_name(param.ShaderVisibility));
				char		c		= param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ? 'b' : param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ? 't' : 'u';
				auto		desc	=  DESCRIPTOR::make<DESCRIPTOR::PCBV>(root.slot[i].get(root_data));
				rt.Add(h3, format_string("%c%i[%i] : 0x%x", c, param.Descriptor.RegisterSpace, param.Descriptor.ShaderRegister, desc.ptr), DX12StateControl::ST_GPU_ADDRESS, desc.ptr);
				break;
			}
		}
	}

	rt.AddArray(rt.AddText(h, "Static Samplers"), root_desc->pStaticSamplers, root_desc->NumStaticSamplers);
}

HTREEITEM AddPipelineState(RegisterTree &rt, HTREEITEM h, const DX12PipelineBase &pipeline, const DX12State &state) {
	if (state.bound_heaps.cbv_srv_uav)
		rt.AddFields(rt.AddText(h, "cbv_srv_uav descriptor heap"), state.bound_heaps.cbv_srv_uav);

	if (state.bound_heaps.sampler)
		rt.AddFields(rt.AddText(h, "sampler descriptor heap"), state.bound_heaps.sampler);

	rt.AddFields(h, &pipeline);

	AddRootState(rt, h, pipeline, state, pipeline.root_data);

	return h;
}

class DX12BatchWindow : public StackWindow, public refs<DX12BatchWindow> {
	struct Target : dx::Resource {
		ISO_ptr_machine<void>	p;
		void init(DX12BatchWindow &bw, const char *name, dx::Resource &&r) {
			if (r) {
				dx::Resource::operator=(move(r));
				bw.addref();
				ConcurrentJobs::Get().add([this, name, &bw] {
					p = GetBitmap(name, *this);
					bw.release();
				});
			}
		}
	};

public:
	enum {ID = 'BA'};
	DX12Connection				*con;
	DX12StateControl			tree;
	DX12ShaderState				shaders[dx::NUM_STAGES];
	dynamic_array<pair<TypedBuffer, int>>	verts;
	dynamic_array<pair<TypedBuffer, int>>	vbv;
	dynamic_array<Target>		targets;
	hash_map<D3D12_CPU_DESCRIPTOR_HANDLE, DESCRIPTOR>	mod_descriptors;

	uint3p						groups = {0,0,0};

	indices						ix;
	Topology					topology;
	BackFaceCull				culling;
	float3x2					viewport;
	Point						screen_size;

	dynamic_array<DX12ShaderState*>		raytrace_shaders;

	static DX12BatchWindow*	Cast(Control c)	{ return (DX12BatchWindow*)StackWindow::Cast(c); }

	DX12Connection	*GetConnection() const {
		return con;
	}
	WindowPos AddView(bool new_tab) {
		return Dock(new_tab ? DOCK_ADDTAB : DOCK_PUSH);
	}
	void SelectBatch(BatchList &b, bool always_list = false) {
		int		batch	= ::SelectBatch(*this, GetMousePos(), b, always_list);
		if (batch >= 0)
			Parent()(WM_ISO_BATCH, batch);
	}
	Control ShowBitmap(ISO_ptr_machine<void> p, bool new_tab) {
		Control	c;
		if (p)
			c = BitmapWindow(AddView(new_tab), p, tag(p.ID()), true);
		return c;
	}

	DX12ShaderState*	AddShader(const DX12Assets::ShaderRecord *rec, const DX12State *state, const char *entry = nullptr, const DX12Assets::ObjectRecord *local_obj = nullptr, const void *local_data = nullptr) {
		DX12RootState	local;
		if (local_obj)
			local.SetRootSignature(local_obj->info);

		auto	s = new DX12ShaderState;
		s->init(*con, con->cache, rec, state, entry, local_obj ? &local : nullptr, local_data);
		raytrace_shaders.push_back(s);
		return s;
	}

	DX12BatchWindow(const WindowPos &wpos, text title, DX12Connection *con, const DX12State &state);
	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	uint32		InitSimulator(dx::SimulatorDX *sim, const DX12ShaderState& shader, uint32 thread, int num_threads, dynamic_array<uint32>& indices, Topology& top) const;
	
	unique_ptr<dx::SimulatorDX>	InitSimulator(const DX12ShaderState& shader, uint32 thread, int num_threads, dynamic_array<uint32>& indices, Topology& top) const {
		auto	sim		= shader.MakeSimulator(*con, con->cache, false);
		InitSimulator(sim, shader, thread, num_threads, indices, top);
		return sim;
	}

	Control		MakeShaderOutput(const WindowPos &wpos, dx::SHADERSTAGE stage, bool mesh);
	void		VertexMenu(dx::SHADERSTAGE stage, int i, ListViewControl lv);
	Control		DebugPixel(uint32 target, const Point &pt);
};

DX12BatchWindow::DX12BatchWindow(const WindowPos &wpos, text title, DX12Connection *con, const DX12State &state) : StackWindow(wpos, title, ID), con(con), tree(con) {
	addref();

	Rebind(this);
	clear(screen_size);

	HTREEITEM	h;

	DX12Assets::BatchInfo	batch	= state;
	if (state.op == RecCommandList::tag_ExecuteIndirect)
		batch = state.indirect.GetCommand(con->cache);

	switch (batch.op) {
		case RecCommandList::tag_DrawInstanced:
		case RecCommandList::tag_DrawIndexedInstanced:
		case RecCommandList::tag_DispatchMesh: {
			//----------------------------
			// graphics
			//----------------------------

			auto&	pipeline = state.graphics_pipeline;

			topology	= state.GetTopology();
			culling		= state.GetCull(true);

			if (auto *d = state.GetDescriptor(con, state.targets[0])) {
				if (auto *obj = con->FindObject(uint64(d->res))) {
					const RecResource *rec = obj->info;
					screen_size = Point(rec->Width, rec->Height);
				}
			}

			auto	vp	= state.GetViewport(0);
			viewport	= float3x2(
				float3{vp.Width / 2, vp.Height / 2, vp.MaxDepth - vp.MinDepth},
				float3{vp.TopLeftX + vp.Width / 2, vp.TopLeftY + vp.Height / 2, vp.MinDepth}
			);

			if (con->replay) {
				int		n = pipeline.RTVFormats.NumRenderTargets;
				targets.resize(n + 1);
				for (uint32 i = 0; i < n; i++)
					targets[i].init(*this, "render", con->replay->GetResource(state.GetDescriptor(con, state.targets[i]), con->cache));
				targets[n].init(*this, "depth", con->replay->GetResource(state.GetDescriptor(con, state.depth), con->cache));
			}

			WindowPos	treepos = GetChildWindowPos();

			if (batch.op == RecCommandList::tag_DispatchMesh) {
				// make compute grid
				groups		= {batch.compute.dim_x, batch.compute.dim_y, batch.compute.dim_z};
				groups		= select(groups == 0, one, groups);
				
				if (auto ms = con->FindShader((uint64)pipeline.shaders[dx::MS].pShaderBytecode)) {
					auto	thread_group = ms->GetThreadGroup();

					if (all(thread_group != 0)) {
						SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
						split2->Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE);
						split2->SetPos(400);
						split2->SetPane(1, *new ComputeGrid(split2->_GetPanePos(1), 0, 'MG', groups, thread_group));
						treepos = split2->GetPanePos(0);
					}
				}

			} else if (state.vbv && pipeline.InputLayout.elements) {
				// make mesh view
				int				ix_size	= 0;
				memory_block	ix_data	= none;
				if (batch.op == RecCommandList::tag_DrawIndexedInstanced) {
					ix_size	= ByteSize(state.ibv.Format);
					ix_data	= con->GetGPU(state.ibv.BufferLocation + batch.draw.index_offset * ix_size, batch.draw.vertex_count * ix_size);
				}
				
				ix = indices(ix_data, ix_size, batch.draw.vertex_offset, batch.draw.vertex_count);

				vbv.resize(pipeline.InputLayout.elements.size());
				auto	d	= pipeline.InputLayout.elements.begin();
				uint32	offset	= 0;
				for (auto &i : vbv) {
					D3D12_VERTEX_BUFFER_VIEW	view = state.vbv[d->InputSlot];
					if (d->AlignedByteOffset != D3D12_APPEND_ALIGNED_ELEMENT)
						offset = d->AlignedByteOffset;

					i.a = con->GetTypedBuffer(view.BufferLocation + offset, view.SizeInBytes - offset, view.StrideInBytes, dx::to_c_type(d->Format));
					offset += uint32(DXGI_COMPONENTS(d->Format).Bytes());
					++d;
				}

				SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
				split2->Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE);
				split2->SetPos(400);
				split2->SetPane(1, *MakeMeshView(split2->_GetPanePos(1), topology, vbv[0].a, ix, one, culling, MeshWindow::PERSPECTIVE));
				treepos = split2->GetPanePos(0);
			}

			tree.Create(treepos, none, CHILD | CLIPSIBLINGS | VISIBLE | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT);
			RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);

			h	= AddPipelineState(rt, TreeControl::Item("Pipeline").Bold().Insert(tree), pipeline, state);
			rt.AddArray(rt.AddText(h, "InputLayout"), (D3D12_INPUT_ELEMENT_DESC*)pipeline.InputLayout.elements.begin(), pipeline.InputLayout.elements.size());
			rt.AddFields(rt.AddText(h, "Rasterizer"), &pipeline.RasterizerState);

			h	= TreeControl::Item("Targets").Bold().Expand().Insert(tree);
		
			if (!pipeline.BlendState.IndependentBlendEnable) {
				buffer_accum<256>	ba;
				ba << "Blend: " << WriteBlend(pipeline.BlendState.RenderTarget[0], state.BlendFactor);
				rt.AddText(h, ba);
			}

			// --- render targets ---
			uint32 nt = pipeline.RTVFormats.NumRenderTargets;
			for (uint32 i = 0; i < nt; i++) {
				const DESCRIPTOR *d = state.GetDescriptor(con, state.targets[i]);
				buffer_accum<256>	ba;
				ba << "Target " << i << con->ObjectName(d);
				if (pipeline.BlendState.IndependentBlendEnable)
					ba << ' ' << WriteBlend(pipeline.BlendState.RenderTarget[i], state.BlendFactor);
				HTREEITEM	h2 = rt.Add(h, ba, tree.ST_DESCRIPTOR, d);

				if (d)
					rt.AddFields(rt.AddText(h2, "Descriptor"), d);
				rt.AddFields(rt.AddText(h2, "Blending"), &pipeline.BlendState.RenderTarget[i]);
			}

			// --- depth buffer ---
			const DESCRIPTOR *d = state.GetDescriptor(con, state.depth);
			buffer_accum<256>	ba;
			ba << "Depth Buffer" << con->ObjectName(d);

			HTREEITEM	h2	= rt.Add(h, ba, tree.ST_DESCRIPTOR, d);
			if (d)
				rt.AddFields(rt.AddText(h2, "Descriptor"), d);
			rt.AddFields(rt.AddText(h2, "DepthStencilState"), (D3D12_DEPTH_STENCIL_DESC1*)&pipeline.DepthStencilState);

			rt.AddArray(rt.AddText(h, "Viewports"), state.viewport);
			rt.AddArray(rt.AddText(h, "Scissor Rects"), state.scissor);

			// --- shaders ---
			h	= TreeControl::Item("Shaders").Bold().Expand().Insert(tree);
			static const char *shader_names[] = {
				"Vertex Shader",
				"Pixel Shader",
				"Domain Shader",
				"Hull Shader",
				"Geometry Shader",
				"Amplification Shader",
				"Mesh Shader",
			};
			for (int i = 0; i < dx::NUM_STAGES; i++) {
				dx::SHADERSTAGE	stage = dx::SHADERSTAGE(i);
				const D3D12_SHADER_BYTECODE	&shader = pipeline.shaders[i];
				if (shader.pShaderBytecode) {
					if (auto rec = con->FindShader((uint64)shader.pShaderBytecode)) {
						shaders[i].init(*con, con->cache, rec, &state);
						h2 = tree.AddShader(
							TreeControl::Item(buffer_accum<256>(shader_names[i]) << ": " << shaders[i].name).Image(tree.ST_SHADER).Param(i).Insert(tree, h),
							shaders[i]
						);
						if (stage == dx::VS) {
							HTREEITEM	h3 = rt.Add(h2, "Inputs", tree.ST_VERTICES, stage);
							if (state.op == RecCommandList::tag_DrawIndexedInstanced)
								rt.AddFields(rt.AddText(h3, "IndexBuffer"), &state.ibv);

							if (vbv) {
								int	x = 0;
								for (auto &i : state.vbv) {
									TypedBuffer	v	= con->GetTypedBuffer(i.BufferLocation, i.SizeInBytes, i.StrideInBytes, pipeline.InputLayout.GetVertexType(x));
									string_builder	title("VertexBuffer ");
									title << x;

									bool	per_instance	= false;

									for (auto &i : pipeline.InputLayout.elements) {
										if (i.InputSlot == x) {
											per_instance = i.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
											title << ' ' << i.SemanticName << onlyif(i.SemanticIndex, i.SemanticIndex);
										}
									}

									verts.emplace_back(move(v), per_instance);
									title << onlyif(per_instance, " (per instance)");

									rt.AddFields(TreeControl::Item(title).Image(tree.ST_VERTICES).StateImage(x + 1).Insert(tree, h3), &i);
									++x;
								}

								if (auto in = shaders[i].DXBC()->GetBlob<dx::DXBC::InputSignature>()) {
									for (int i = 0; i < pipeline.InputLayout.elements.size(); i++) {
										auto	&desc = pipeline.InputLayout.elements[i];
										if (auto *x = in->find_by_semantic(desc.SemanticName, desc.SemanticIndex))
											vbv[i].b = x->register_num;
										else
											vbv[i].b = -1;
									}
								} else  if (auto in = shaders[i].DXBC()->GetBlob<dx::DXBC::InputSignature1>()) {
									auto *blank = in->find_by_semantic(0, -1);
									for (int i = 0; i < pipeline.InputLayout.elements.size(); i++) {
										auto	&desc = pipeline.InputLayout.elements[i];
										if (auto *x = in->find_by_semantic(desc.SemanticName, desc.SemanticIndex))
											vbv[i].b = x->register_num;
										else if (blank)
											vbv[i].b = blank++->register_num;
										else
											vbv[i].b = -1;
									}
								}
							}
						}
						rt.Add(h2, "Outputs", tree.ST_OUTPUTS, stage);
					}
				}
			}
			break;
		}
		
		case RecCommandList::tag_Dispatch: {
			//----------------------------
			// compute
			//----------------------------
			if (auto cs = con->FindShader((uint64)state.compute_pipeline.CS.pShaderBytecode)) {
				shaders[0].init(*con, con->cache, cs, &state);
				auto	thread_group	= cs->GetThreadGroup();
				groups		= {batch.compute.dim_x, batch.compute.dim_y, batch.compute.dim_z};
				groups		= select(groups == 0, one, groups);

				SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
				split2->Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE);
				split2->SetPanes(
					tree.Create(split2->_GetPanePos(0), none, CHILD | CLIPSIBLINGS | VISIBLE | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT),
					*new ComputeGrid(split2->_GetPanePos(1), 0, 'CG', groups, thread_group),
					400
				);
			} else {
				tree.Create(GetChildWindowPos(), none, CHILD | CLIPSIBLINGS | VISIBLE | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT);
			}

			RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);

			AddPipelineState(rt, TreeControl::Item("Pipeline").Bold().Insert(tree), state.compute_pipeline, state);
			if (shaders[0])
				tree.AddShader(
					TreeControl::Item(buffer_accum<256>("Compute Shader: ") << shaders[0].name).Image(tree.ST_SHADER).Param(0).Insert(tree, TVI_ROOT),
					shaders[0]
				);

			break;
		}

		case RecCommandList::tag_DispatchRays: {
			//----------------------------
			// ray-tracing
			//----------------------------
			groups		= {batch.rays.dim_x, batch.rays.dim_y, batch.rays.dim_z};

			SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
			split2->Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE);
			split2->SetPanes(
				tree.Create(split2->_GetPanePos(0), none, CHILD | CLIPSIBLINGS | VISIBLE | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT),
				*new ComputeGrid(split2->_GetPanePos(1), 0, 'CG', groups, {1, 1, 1}),
				400
			);

			RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
			AddPipelineState(rt, TreeControl::Item("Global Pipeline").Bold().Insert(tree), state.compute_pipeline, state);
			
			string_param	name;

			// --- raygen ---
			if (auto raygen_table = con->cache(batch.rays.RayGenerationShaderRecord.StartAddress | GPU, batch.rays.RayGenerationShaderRecord.SizeInBytes)) {
				if (auto rec = state.compute_pipeline.FindShader(con, *(SHADER_ID*)raygen_table, name)) {
					DX12RootState	local;
					auto			local_obj	= con->FindObject((uint64)state.compute_pipeline.LocalPipeline(name));
					if (local_obj)
						local.SetRootSignature(local_obj->info);

					shaders[0].init(*con, con->cache, rec, &state, name, local_obj ? &local : nullptr, raygen_table + sizeof(SHADER_ID));
					auto	h2 = tree.AddShader(TreeControl::Item(buffer_accum<256>("RayGen Shader: ") << name).Image(tree.ST_SHADER).Param(0).Insert(tree), shaders[0]);

					if (local_obj)
						rt.Add(h2, buffer_accum<256>("Local RootSignature ") << local_obj->name, tree.ST_OBJECT, local_obj);
				}
			}

			// --- hitgroups ---
			if (auto hitgroup_table = con->cache(batch.rays.HitGroupTable.StartAddress | GPU, batch.rays.HitGroupTable.SizeInBytes)) {//, state.HitGroupTable.StrideInBytes);
				h = TreeControl::Item("Hit Groups").Bold().Expand().Insert(tree, TVI_ROOT);

				for (auto &id : make_strided<SHADER_ID>(hitgroup_table, batch.rays.HitGroupTable.StrideInBytes)) {
					auto	hitgroup	= state.compute_pipeline.FindHitGroup(id);
					auto	h2			= TreeControl::Item(hitgroup->HitGroupExport).Bold().Insert(tree, h);

					auto			local_obj	= con->FindObject((uint64)state.compute_pipeline.LocalPipeline(hitgroup->HitGroupExport));
					if (local_obj)
						rt.Add(h2, buffer_accum<256>("Local RootSignature") << local_obj->name, tree.ST_OBJECT, local_obj);

					if (hitgroup->AnyHitShaderImport) {
						if (auto rec = state.compute_pipeline.FindShader(con, hitgroup->AnyHitShaderImport)) {
							auto	s = AddShader(rec, &state, str8(hitgroup->AnyHitShaderImport), local_obj, &id + 1);
							tree.AddShader(TreeControl::Item(buffer_accum<256>("AnyHit Shader: ") << hitgroup->AnyHitShaderImport).Image(tree.ST_SHADER).Param(raytrace_shaders.size() + 0xff).Insert(tree, h2), *s);
						}
					}

					if (hitgroup->ClosestHitShaderImport) {
						if (auto rec = state.compute_pipeline.FindShader(con, hitgroup->ClosestHitShaderImport)) {
							auto	s = AddShader(rec, &state, str8(hitgroup->ClosestHitShaderImport), local_obj, &id + 1);
							tree.AddShader(TreeControl::Item(buffer_accum<256>("ClosestHit Shader: ") << hitgroup->ClosestHitShaderImport).Image(tree.ST_SHADER).Param(raytrace_shaders.size() + 0xff).Insert(tree, h2), *s);
						}
					}

					if (hitgroup->IntersectionShaderImport) {
						if (auto rec = state.compute_pipeline.FindShader(con, hitgroup->IntersectionShaderImport)) {
							auto	s = AddShader(rec, &state, str8(hitgroup->IntersectionShaderImport), local_obj, &id + 1);
							tree.AddShader(TreeControl::Item(buffer_accum<256>("Intersection Shader: ") << hitgroup->IntersectionShaderImport).Image(tree.ST_SHADER).Param(raytrace_shaders.size() + 0xff).Insert(tree, h2), *s);
						}
					}
				}
			}

			// --- miss ---
			if (auto miss_table = con->cache(batch.rays.MissShaderTable.StartAddress | GPU, batch.rays.MissShaderTable.SizeInBytes)) {//, state.MissShaderTable.StrideInBytes);
				h = TreeControl::Item("Miss").Bold().Insert(tree);
				for (auto &id : make_strided<SHADER_ID>(miss_table, batch.rays.MissShaderTable.StrideInBytes)) {
					if (auto rec = state.compute_pipeline.FindShader(con, id, name)) {
						auto	local_obj	= con->FindObject((uint64)state.compute_pipeline.LocalPipeline(name));
						auto	s			= AddShader(rec, &state, name, local_obj, &id + 1);
						auto	h2			= tree.AddShader(TreeControl::Item(buffer_accum<256>("Shader: ") << name).Image(tree.ST_SHADER).Param(raytrace_shaders.size() + 0xff).Insert(tree, h), *s);

						if (local_obj)
							rt.Add(h2, buffer_accum<256>("Local RootSignature ") << local_obj->name, tree.ST_OBJECT, local_obj);
					}
				}
			}

			// --- callable ---
			if (auto callable_table	= con->cache(batch.rays.CallableShaderTable.StartAddress | GPU, batch.rays.CallableShaderTable.SizeInBytes)) {//, state.CallableShaderTable.StrideInBytes);
				h = TreeControl::Item("Callable").Bold().Insert(tree);
				for (auto &id : make_strided<SHADER_ID>(callable_table, batch.rays.CallableShaderTable.StrideInBytes)) {
					if (auto rec = state.compute_pipeline.FindShader(con, id, name)) {
						auto	local_obj	= con->FindObject((uint64)state.compute_pipeline.LocalPipeline(name));
						auto	s			= AddShader(rec, &state, name, local_obj, &id + 1);
						auto	h2			= tree.AddShader(TreeControl::Item(buffer_accum<256>("Shader: ") << name).Image(tree.ST_SHADER).Param(raytrace_shaders.size() + 0xff).Insert(tree, h), *s);

						if (local_obj)
							rt.Add(h2, buffer_accum<256>("Local RootSignature ") << local_obj->name, tree.ST_OBJECT, local_obj);
					}
				}
			}

			break;
		}


		default: {
			//----------------------------
			// others
			//----------------------------
			tree.Create(GetChildWindowPos(), none, CHILD | CLIPSIBLINGS | VISIBLE | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT);
			RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
			AddPipelineState(rt, TreeControl::Item("Pipeline").Bold().Insert(tree), state.compute_pipeline, state);
			break;
		}
	}

	//----------------------------
	// indirect
	//----------------------------
	if (state.op == RecCommandList::tag_ExecuteIndirect) {
		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		h	= TreeControl::Item("Indirect").Bold().Expand().Insert(tree);

		auto	desc	= rmap_unique<RTM, D3D12_COMMAND_SIGNATURE_DESC>(state.indirect.signature->info);
		rt.AddFields(rt.AddText(h, "Command Signature"), (const map_t<RTM, D3D12_COMMAND_SIGNATURE_DESC>*)state.indirect.signature->info);

		auto	count	= state.indirect.command_count;

		if (state.indirect.counts) {
			const RecResource	*rec	= state.indirect.counts->info;
			uint32				*counts	= con->cache((rec->gpu | GPU) + state.indirect.counts_offset, rec->Width - state.indirect.counts_offset);
			if (counts && *counts < count)
				count = *counts;
			rt.AddFields(TreeControl::Item("Counts").Image(tree.ST_VERTICES).StateImage(verts.size32()).Insert(tree, h), (const RecResource*)state.indirect.arguments->info);
		}

		if (state.indirect.arguments) {
			const RecResource	*rec	= state.indirect.arguments->info;
			auto				args	= con->cache((rec->gpu | GPU) + state.indirect.arguments_offset, count * desc->ByteStride);
			const uint8*		p		= args;
			auto				h2		= TreeControl::Item("Arguments").Image(tree.ST_VERTICES).StateImage(verts.size32()).Insert(tree, h);
			for (int i = 0; i < count; i++) {
				auto	h3	= rt.AddText(h2, format_string("[%i]", i).begin());

				for (auto &i : make_range_n(desc->pArgumentDescs, desc->NumArgumentDescs)) {
					auto	h4	= rt.AddText(h3, get_field_name(i.Type));
					rt.AddFields(h4, IndirectFields[i.Type], p);
					p += IndirectSizes[i.Type];
				}

			}
		}

	}

	mod_descriptors = move(unconst(state).mod_descriptors);
}

LRESULT DX12BatchWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case DebugWindow::ID_DEBUG_PIXEL: {
					auto	*p = (pair<uint64,Point>*)lParam;
					DebugPixel(p->a, p->b);
					return 0;
				}
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CLICK: {
					if (nmh->hwndFrom == tree) {
						if (HTREEITEM hItem = tree.hot) {
							tree.hot	= 0;
							TreeControl::Item i	= tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE);
							bool	ctrl	= !!(GetKeyState(VK_CONTROL) & 0x8000);
							bool	new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);

							switch (i.Image()) {
								case DX12StateControl::ST_OBJECT:
									MakeObjectView(AddView(new_tab), con, i.Param());
									return 0;

								case DX12StateControl::ST_SHADER: {
									int	index = (int)i.Param();
									MakeShaderViewer(AddView(new_tab), con, index < 0x100 ? shaders[index] : *raytrace_shaders[index - 0x100], ctrl);
									return 0;
								}
								case DX12StateControl::ST_VERTICES:
									if (DESCRIPTOR* d = i.Param()) {
										MakeRayTraceView(AddView(new_tab), "RayTrace", con, d);

									} else if (int x = i.StateImage()) {
										MakeBufferWindow(AddView(new_tab), "Vertices", 'VI', verts[x - 1].a);

									} else {
										dynamic_array<named<TypedBuffer>>	named_verts;
										dynamic_array<named<TypedBuffer>>	named_insts;
										for (auto &i : verts)
											(i.b == 0 ? named_verts : named_insts).emplace_back(none, i.a);

										MeshVertexWindow	*m;

										if (named_insts) {
											SplitterWindow		*s	= new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_DELETE_ON_DESTROY);
											s->Create(AddView(new_tab), "Vertices", CHILD | CLIPSIBLINGS | VISIBLE);
											m	= new MeshVertexWindow(s->GetPanePos(0), "Vertices");
											TitledWindow		*t	= new TitledWindow(s->_GetPanePos(1), "Instances");
											VertexWindow		*vw	= MakeVertexWindow(t->GetChildWindowPos(), "Instances", 'VX', named_insts, named_insts.size());
										} else {
											m	= new MeshVertexWindow(AddView(new_tab), "Vertices");
										}

										VertexWindow		*vw	= MakeVertexWindow(m->GetPanePos(0), "Vertices", 'VI', named_verts, ix);
										MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1), topology, vbv[0].a, ix, one, culling, MeshWindow::PERSPECTIVE);
										m->SetPanes(*vw, *mw, 50);
									}
									return 0;

								case DX12StateControl::ST_OUTPUTS:
									MakeShaderOutput(AddView(new_tab), i.Param(), ctrl);
									return 0;

								case DX12StateControl::ST_DESCRIPTOR: {
									DESCRIPTOR	*d = i.Param();
									MakeDescriptorView(AddView(new_tab), "Desc", con, d);
									return 1;
								}
								case DX12StateControl::ST_DESCRIPTOR_REF: {
									auto	cpu		= (D3D12_CPU_DESCRIPTOR_HANDLE)i.Param();
									MakeDescriptorHeapView(AddView(new_tab), con, cpu);
									return 1;
								}
								case DX12StateControl::ST_GPU_ADDRESS: {
									uint64	gpu		= i.Param();
									BinaryWindow(AddView(new_tab), ISO::MakeBrowser(memory_block(con->cache(uint64(gpu)))));
									return 1;
								}
							}
						}
					}
					break;
				}

				case NM_RCLICK:
					switch (wParam) {
						case 'VO': VertexMenu(dx::VS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'PO': VertexMenu(dx::PS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'DO': VertexMenu(dx::DS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'HO': VertexMenu(dx::HS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'GO': VertexMenu(dx::GS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'AO': VertexMenu(dx::AS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'MO': case 'MG': VertexMenu(dx::MS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'CO': case 'CG': VertexMenu(dx::CS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
					}
					break;

				case TVN_SELCHANGING:
					return TRUE; // prevent selection

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					break;
			}
			return Parent()(message, wParam, lParam);
		}

		case WM_NCDESTROY:
			release();//delete this;
			return 0;
	}
	return StackWindow::Proc(message, wParam, lParam);
}


// returns starting thread

uint32 DX12BatchWindow::InitSimulator(dx::SimulatorDX *sim, const DX12ShaderState &shader, uint32 thread, int num_threads, dynamic_array<uint32> &indices, Topology &top) const {
	using namespace dx;

	unique_ptr<dx::SimulatorDX>	sim2;
	Signature		in		= shader.GetSignatureIn();
	Signature		out;

	switch (shader.stage) {
		case PS: {
			auto	prev	= shaders[GS] ? GS : shaders[DS] ? DS : shaders[MS] ? MS : VS;
			sim2			= InitSimulator(shaders[prev], 0, 0, indices, top);
			sim2->Run();

			out				= shaders[prev].GetSignatureOut();
			int		n		= triangle_row(64 / 4);
			float	duv		= 1 / float(n * 2 - 1);

			if (num_threads >= 0) {
				sim->SetNumThreads(max(num_threads, 4));
				float	u	= triangle_col(thread / 4) * 2 * duv;
				float	v	= triangle_row(thread / 4) * 2 * duv;
				for (auto& i : in) {
					GetTriangle(sim2, i.name.get(in), i.semantic_index, out, 0, 1, 2)
						.Interpolate4(sim->GetRegFile<float4p>(Operand::TYPE_INPUT, i.register_num).begin(), u, v, u + duv, v + duv);
				}
			} else {
				sim->SetNumThreads(triangle_number(n) * 4);
				for (auto& i : in) {
					auto	tri		= GetTriangle(sim2, i.name.get(in), i.semantic_index, out, 0, 1, 2);
					auto	dest	= sim->GetRegFile<float4p>(Operand::TYPE_INPUT, i.register_num).begin();
					for (int u = 0; u < n; u++) {
						float	uf0 = float(u * 2) * duv;
						for (int v = 0; v <= u; v++) {
							float	vf0 = float(v * 2) * duv;
							tri.Interpolate4(dest, uf0, vf0, uf0 + duv, vf0 + duv);
							dest += 4;
						}
					}
				}
			}
			thread &= ~3;
			break;
		}

		case HS: {
			sim2	= InitSimulator(shaders[VS], 0, sim->NumInputControlPoints(), indices, top);
			sim2->Run();
			
			out		= shaders[VS].GetSignatureOut();
			for (auto& i : in) {
				if (!sim->HasInput(i.register_num)) {
					if (auto *x = out.find_by_semantic(i.name.get(in), i.semantic_index))
						sim->SetPatchInput(i.register_num);
				}
			}

			sim->SetNumThreads(max(num_threads, sim->forks, sim->NumOutputControlPoints()));

			for (auto& i : in) {
				if (auto *x = out.find_by_semantic(i.name.get(in), i.semantic_index)) {
					auto	s = sim2->GetOutput<float4p>(x->register_num);
					if (sim->HasInput(i.register_num)) {
						copy(s, sim->GetRegFile<float4p>(Operand::TYPE_INPUT, i.register_num));
					} else {
						// set outputs in case there's no cp phase
						copy(s, sim->GetRegFile<float4p>(Operand::TYPE_OUTPUT_CONTROL_POINT, i.register_num));
					}
				}
			}

			top		= GetTopology(sim->tess_output);
			thread	= 0;
			break;
		}

		case DS: {
			sim2	= InitSimulator(shaders[HS], 0, 0, indices, top);
			sim2->Run();

			Tesselation	tess = GetTesselation(sim->tess_domain, sim2->GetRegFile<float4p>(Operand::TYPE_INPUT_PATCH_CONSTANT));

			if (num_threads < 0) {
				int	num	= tess.uvs.size32();
				num_threads	= num_threads == -1 ? min(num, 64) : num;
			}
			sim->SetNumThreads(max(num_threads, 1));

			auto	*uvs	= tess.uvs + thread;
			for (auto &i : sim->GetRegFile<float4p>(Operand::TYPE_INPUT_DOMAIN_POINT)) {
				i = float4{uvs->x, uvs->y, 1 - uvs->x - uvs->y, 0};
				++uvs;
			}

			((SimulatorDXIL*)sim)->CopyConsts((SimulatorDXIL*)sim2.get());
			//copy(sim2->PatchConsts(), sim->PatchConsts());

			out	= shaders[HS].GetSignatureOut();
			for (auto& i : in) {
				if (auto *x = out.find_by_semantic(i.name.get(in), i.semantic_index))
					rcopy(
						//sim->GetRegFile<float4p>(Operand::TYPE_INPUT_CONTROL_POINT, i.register_num),
						sim->GetRegFile<float4p>(Operand::TYPE_INPUT, i.register_num),
						//sim2->GetRegFile<float4p>(Operand::TYPE_OUTPUT_CONTROL_POINT, x->register_num).begin()
						sim2->GetRegFile<float4p>(Operand::TYPE_OUTPUT, x->register_num).begin()
					);
			}
			if (thread != -1)
				indices = move(tess.indices);
			return thread;
		}

		case VS: {
			bool	deindex	= false;
			switch (num_threads) {
				case -2:
					num_threads	= ix.max_index() + 1;
					indices	= ix;
					break;
				case -1:
					num_threads = min(ix.max_index() + 1, 64);
					break;
				case 0:
					num_threads = topology.VertsPerPrim();
					break;
				default:
					deindex = true;
					break;
			}
			top		= topology;

			sim->SetNumThreads(num_threads);

			for (auto& i : in) {
				switch (i.system_value) {
					case SV_UNDEFINED:
						for (auto& v : vbv) {
							if (v.b == i.register_num) {
								if (deindex)
									sim->SetInput(v.b, i.component_type, make_indexed_iterator(v.a.begin(), ix.begin() + thread));
								else
									sim->SetInput(v.b, i.component_type, v.a.begin() + thread);
								break;
							}
						}
						break;

					case SV_VERTEX_ID:
						if (deindex)
							sim->SetInput<int>(i.register_num, ix.begin() + thread);
						else
							sim->SetInput<int>(i.register_num, make_int_iterator(thread));
						break;
				}
			}

			break;
		}

		case GS: {
			// thread is really 'output'
			auto	in_topo = GetTopology(sim->input_prim);
			if (num_threads < 0) {
				int	num	= in_topo.verts_to_prims(ix.max_index() + 1);
				sim->SetNumThreads(num_threads == -1 ? min(num, 64) : num);
			} else {
				sim->SetNumThreads(max(num_threads, 1));
			}

			int		vert_begin	= in_topo.first_vert(thread / sim->max_output);
			int		vert_end	= in_topo.prims_to_verts(thread / sim->max_output + sim->NumThreads());

			auto&	prev		= shaders[DS] ? shaders[DS] : shaders[VS];
			sim2	= InitSimulator(prev, vert_begin, vert_end - vert_begin, indices, top);
			sim2->Run();
			out	= prev.GetSignatureOut();
			
			auto	input_size = highest_set_index(sim->input_mask) + 1;

			for (auto& i : in) {
				if (auto *x = out.find_by_semantic(i.name.get(in), i.semantic_index)) {
					auto	dst = sim->GetRegFile<float4p>(dx::Operand::TYPE_INPUT, i.register_num).begin();
					
					for (auto p : make_prim_container(in_topo, sim2->GetOutput<float4p>(x->register_num))) {
						auto dst2 = strided(&*dst, input_size * sizeof(float4p));
						copy(p, dst2);
						++dst;
					}
				}
			}
			top		= GetTopology(sim->output_topology);
			thread	= (thread / sim->max_output) * sim->max_output;
			break;
		}

		case MS:
			top		= GetTopology(sim->output_topology);

			if (shaders[AS] && !sim->_GetRegFile(Operand::TYPE_INPUT_PATCH_CONSTANT).begin()) {
				sim2				= shaders[AS].MakeSimulator(*con, con->cache, false);
				uint32	group_size	= sim2->GroupSize();
				InitSimulator(sim2, shaders[AS], thread / group_size * group_size, group_size, indices, top);
				sim2->Run();
				((SimulatorDXIL*)sim)->CopyConsts((SimulatorDXIL*)sim2.get());

				if (reduce_mul(sim->GetRegFile<uint32x4>(Operand::TYPE_INPUT_PATCH_CONSTANT)[0].xyz) == 0) {
					sim->SetNumThreads(0);
					return 0;
				}
			}
		case AS:
		case CS: {
			uint32x3	thread_group	= sim->thread_group;
			uint32		group_size		= sim->GroupSize();
			
			thread	= thread / group_size * group_size;

			sim->SetNumThreads(num_threads > 0 ? min(num_threads, group_size) : group_size);

			if (sim->HasInput(SimulatorDXBC::vThreadIDInGroup))
				rcopy(
					sim->GetRegFile<uint4p>(Operand::TYPE_INPUT, SimulatorDXBC::vThreadIDInGroup),
					transform(int_iterator<int>(thread), [&](int i) { return concat(split_index(i, thread_group.xy), i); })
				);

			sim->SetGroupInputs(thread, groups);
			break;
		}

		default:
			sim->SetNumThreads(max(num_threads, 1));
			break;
	}

	for (auto& i : in) {
		if (shader.stage != PS && shader.stage != DS && shader.stage != GS) {
			if (auto *x = out.find_by_semantic(i.name.get(in), i.semantic_index)) {
				sim->SetInput<float4p>(i.register_num, sim2->GetOutput<float4p>(x->register_num).begin());
				continue;
			}
		}
		switch (i.system_value) {
			case SV_INSTANCE_ID:
				//if (auto per_instance = top.chunks)
				//	sim->SetInput<int>(i.register_num, transform(make_int_iterator(thread), [per_instance](int i) { return i / per_instance; }));
				//else
					sim->SetInput<int>(i.register_num, scalar(0));
				break;

			case SV_CLIP_DISTANCE:
				break;

			case SV_CULL_DISTANCE:
				break;

			case SV_PRIMITIVE_ID:
				sim->SetInput<int>(i.register_num, transform(make_int_iterator(thread), [&top](int i) { return top.PrimFromVertex(i); }));
				break;
		}
	}

	return thread;
}


Control DX12BatchWindow::MakeShaderOutput(const WindowPos &wpos, dx::SHADERSTAGE stage, bool mesh) {
	using namespace dx;
	auto	&shader = shaders[stage == dx::CS ? 0 : stage];
	auto	dxbc	= shader.DXBC();
	if (!dxbc)
		return {};

	dynamic_array<uint32>	ib;
	Topology				top;

	auto	sim		= InitSimulator(shader, 0, mesh ? -2 : -1, ib, top);

	// find first MS dispatch
	if (!mesh && stage == dx::MS && sim->NumThreads() == 0) {
		if (auto& as_shader = shaders[AS]) {
			auto	as_sim			= as_shader.MakeSimulator(*con, con->cache, false);
			uint32	as_group_size	= as_sim->GroupSize();

			for (uint32 i = 0, n = reduce_mul(groups); i < n; i++) {
				InitSimulator(as_sim, as_shader, i * as_group_size, as_group_size, ib, top);
				as_sim->Run();
				((SimulatorDXIL*)sim.get())->CopyConsts((SimulatorDXIL*)as_sim.get());

				if (reduce_mul(sim->GetRegFile<uint32x4>(Operand::TYPE_INPUT_PATCH_CONSTANT)[0].xyz)) {
					InitSimulator(sim, shader, 0, -1, ib, top);
					break;
				}
			}
		}
	}

	Control	c	= app::MakeShaderOutput(wpos, sim, shader, ib);

	if (!mesh)
		return c;

	switch (stage) {
		default:
			return c;

		case dx::VS: {
			bool	final	= !shaders[dx::DS] && !shaders[dx::GS];
			int		out_reg	= 0;

			if (auto* sigs = shader.DXBC()->GetBlob<DXBC::OutputSignature>()) {
				if (auto *sig = find_if_check(sigs->Elements(), [](const dx::SIG::Element &i) { return i.system_value == dx::SV_POSITION; }))
					out_reg = sig->register_num;

			} else  if (auto* sigs = shader.DXBC()->GetBlob<DXBC::OutputSignature1>()) {
				if (auto *sig = find_if_check(sigs->Elements(), [](const dx::SIG::Element &i) { return i.system_value == dx::SV_POSITION; }))
					out_reg = sig->register_num;
			}

			MeshVertexWindow	*m		= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw		= MakeMeshView(m->_GetPanePos(1),
				top,
				temp_array<float4p>(sim->GetOutput<float4p>(out_reg)),
				ib.empty() ? ix : indices(ib),
				viewport, culling,
				final ? MeshWindow::SCREEN_PERSP : MeshWindow::PERSPECTIVE
			);
	
			if (final) {
				if (screen_size.x && screen_size.y)
					mw->SetScreenSize(screen_size);
				mw->flags.set(MeshWindow::FRUSTUM_EDGES);
			}

			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case dx::GS: {
			uint32				output_size	= highest_set_index(sim->output_mask & ~(1 << sim->oEmitted)) + 1;
			temp_array<uint32>	num_output	= sim->GetRegFile<uint32>(dx::Operand::TYPE_OUTPUT, sim->oEmitted);
			
			temp_array<float4p>	verts(reduce<op_add>(num_output));
			auto				pv			= verts.begin();
			auto				pn			= num_output.begin();

			for (auto &src : sim->GetRegFile<float4p>(dx::Operand::TYPE_OUTPUT_CONTROL_POINT)) {
				auto	s = strided(&src, output_size * sizeof(float4p));
				for (auto n = *pn++; n--; ++pv, ++s)
					*pv = *s;
			}

			MeshVertexWindow	*m		= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw		= MakeMeshView(m->GetPanePos(1),
				top,
				verts,
				verts.size(),
				viewport, culling,
				MeshWindow::SCREEN_PERSP
			);	

			if (screen_size.x && screen_size.y)
				mw->SetScreenSize(screen_size);
			mw->flags.set(MeshWindow::FRUSTUM_EDGES);

			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case dx::HS: {
			Tesselation			tess	= GetTesselation(sim->tess_domain, sim->GetRegFile<float4p>(dx::Operand::TYPE_INPUT_PATCH_CONSTANT));
			SplitterWindow		*m		= new SplitterWindow(GetChildWindowPos(), "Shader Output", SplitterWindow::SWF_VERT | SplitterWindow::SWF_PROP);
			MeshWindow			*mw		= MakeMeshView(m->GetPanePos(1),
				top,
				tess.uvs,
				tess.indices,
				one, culling,
				MeshWindow::PERSPECTIVE
			);	
			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case dx::DS: {
			bool				final	= !shaders[dx::GS];
			MeshVertexWindow	*m		= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw		= MakeMeshView(m->GetPanePos(1),
				top,
				temp_array<float4p>(sim->GetOutput<float4p>(0)),
				sim->NumThreads(),
				viewport, culling,
				final ? MeshWindow::SCREEN_PERSP : MeshWindow::PERSPECTIVE
			);

			if (final) {
				if (screen_size.x && screen_size.y)
					mw->SetScreenSize(screen_size);
				mw->flags.set(MeshWindow::FRUSTUM_EDGES);
			}

			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case MS: {
			uint32	group_size	= sim->GroupSize();
			auto	v			= VertexWindow::Cast(c);

			if (auto& as_shader = shaders[AS]) {
				timer	t;
				auto	as_sim			= as_shader.MakeSimulator(*con, con->cache, false);
				uint32	as_group_size	= as_sim->GroupSize();
				auto	stride			= v->buffers[0].stride;
				auto	num_groups		= reduce_mul(groups);

				struct GroupData {
					TypedBuffer				buffer;
					dynamic_array<uint32>	indexing;
					uint2p					offset;
					GroupData(uint32 stride) : buffer(stride), offset(zero) {}
				};

				temp_array<GroupData>		groups(num_groups, stride);

				parallel_for(groups, [&](GroupData &i) {
					int	x = groups.index_of(i);

					SimulatorDXIL	as_sim2 = *(SimulatorDXIL*)as_sim.get();
					InitSimulator(&as_sim2, as_shader, x * as_group_size, as_group_size, ib, top);

					as_sim2.Run();

					SimulatorDXIL	ms_sim = *(SimulatorDXIL*)sim.get();
					ms_sim.CopyConsts(&as_sim2);

					auto	ms_groups	= ms_sim.GetRegFile<uint32x4>(Operand::TYPE_INPUT_PATCH_CONSTANT)[0].xyz;
					auto&	buffer		= i.buffer;
					auto&	indexing	= i.indexing;

					for (uint32 j = (x == 0), n = reduce_mul(ms_groups); j < n; j++) {
						InitSimulator(&ms_sim, shader, j * group_size, group_size, ib, top);

						ms_sim.SetGroupInputs(j * group_size, ms_groups);
						ms_sim.Run();

						auto	prims	= ms_sim.GetRegFile<uint3p>(dx::Operand::TYPE_OUTPUT_CONTROL_POINT);
						auto	verts	= ms_sim.GetRegFile<uint8>(dx::Operand::TYPE_OUTPUT);
						uint32	nv		= buffer.size32();
						copy(make_deferred(prims) + nv, (uint3p*)indexing.expand(prims.size() * 3));

						auto	dest	= buffer.extend(verts.size32()).begin();
						for (auto &r : verts) {
							memcpy((void*)dest, &r, stride);
							++dest;
						}
					}
				});

				uint2p	offset = {v->buffers[0].size32(), v->indexing.size32()};
				for (auto& i : groups) {
					i.offset	= offset;
					offset.x	+= i.buffer.size32();
					offset.y	+= i.indexing.size32();
				}

				v->buffers[0].resize(offset.x);
				v->indexing.resize(offset.y);

				parallel_for(groups, [&](const GroupData &i) {
					auto	offset = i.offset;
					i.buffer.copy_to(v->buffers[0][offset.x]);
					copy(make_deferred(i.indexing) + offset.x, v->indexing + offset.y);
				});

				ISO_OUTPUTF("MS sim took ") << (float)t << "s\n";
				v->Show();

			} else {
				for (int i = 1, n = reduce_mul(groups); i < n; i++) {
					InitSimulator(sim, shader, i * group_size, group_size, ib, top);
					app::AddShaderOutput(c, sim, shader, ib);
				}
			}

			MeshVertexWindow	*m		= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw		= MakeMeshView(m->_GetPanePos(1),
				top,
				v->buffers[0],
				v->indexing,
				viewport, culling,
				MeshWindow::SCREEN_PERSP
			);

			if (screen_size.x && screen_size.y)
				mw->SetScreenSize(screen_size);
			mw->flags.set(MeshWindow::FRUSTUM_EDGES);

			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case AS:
		case CS: {
			uint32	group_size	= sim->GroupSize();
			uint32	num_groups	= reduce_mul(groups);
			for (int i = 1; i < num_groups; i++) {
				InitSimulator(sim, shader, i * group_size, group_size, ib, top);
				app::AddShaderOutput(c, sim, shader, ib);
			}
			return c;
		}
	}
}

void DX12BatchWindow::VertexMenu(dx::SHADERSTAGE stage, int i, ListViewControl lv) {
	auto	&shader = shaders[stage == dx::CS ? 0 : stage];
	Menu	menu	= Menu::Popup();

	menu.Append("Debug", 1);
	if (stage == dx::VS && ix) {
		menu.Separator();
		menu.Append(format_string("Next use of index %i", ix[i]), 2);
		menu.Append(format_string("Highlight all uses of index %i", ix[i]), 3);
	}

	menu.Append("Show Simulated Trace", 4);

	switch (menu.Track(*this, GetMousePos(), TPM_NONOTIFY | TPM_RETURNCMD)) {
		case 1: {
			dynamic_array<uint32>	ib;
			Topology				top;
			auto	sim		= shader.MakeSimulator(*con, con->cache, true);//make false to debug non-debug shader
			if (stage == dx::VS && ix)
				i = ix[i];

			auto	base	= InitSimulator(sim, shader, i, 0, ib, top);
			//auto	base	= InitSimulator(sim, shader, 0, -2, ib, top);

			auto	*debugger	= new DX12ShaderDebuggerWindow(GetChildWindowPos(), "Debugger", shader, move(sim), con, GetSettings("General/shader source").Get(Disassembler::SHOW_SOURCE));
			debugger->SetThread(i - base);
			PushView(debugger->control());
			break;
		}

		case 2: {
			int	x = ix[i];
			for (int j = 1, n = ix.num; j < n; ++j) {
				int	k = (i + j) % n;
				if (ix[k] == x) {
					lv.SetItemState(k, LVIS_SELECTED);
					lv.EnsureVisible(k);
					lv.SetSelectionMark(k);
					break;
				}
			}
			break;
		}
		case 3: {
			int	x = ix[i];
			for (int j = 0, n = ix.num; j < n; ++j) {
				if (ix[j] == x)
					lv.SetItemState(j, LVIS_SELECTED);
			}
			break;
		}
		case 4: {
			dynamic_array<uint32>	ib;
			Topology				top;
			auto	sim		= shader.MakeSimulator(*con, con->cache, false);
			auto	base	= InitSimulator(sim, shader, i, 0, ib, top);

			if (shader.IsDXIL()) {
				PushView(*new DXILTraceWindow(GetChildWindowPos(), (dx::SimulatorDXIL*)sim.get(), i - base, 1000));

			} else {
				PushView(*new DXBCTraceWindow(GetChildWindowPos(), (dx::SimulatorDXBC*)sim.get(), i - base, 1000));
			}
			break;
		}

	}
	menu.Destroy();
}

Control DX12BatchWindow::DebugPixel(uint32 target, const Point &pt) {
	using namespace dx;

	unique_ptr<dx::SimulatorDX>	sim2;
	dx::Signature			out;
	dynamic_array<uint32>	ib;
	Topology				top;

	if (auto &gs = shaders[dx::GS]) {
		sim2	= InitSimulator(gs, 0, -2, ib, top);
		out		= gs.GetSignatureOut();
	} else if (auto &ds = shaders[dx::DS]) {
		sim2	= InitSimulator(ds, 0, -2, ib, top);
		out		= ds.GetSignatureOut();
	} else {
		auto	&vs = shaders[dx::VS];
		sim2	= InitSimulator(vs, 0, -2, ib, top);
		out		= vs.GetSignatureOut();
	}
			
	sim2->Run();
			
	dynamic_array<float4p>	output_verts = sim2->GetOutput<float4p>(0);
	uint32	num_verts	= ib ? ib.size32() : output_verts.size32();
	uint32	num_prims	= top.verts_to_prims(num_verts);
	dynamic_array<cuboid>	exts(num_prims);
	cuboid	*ext		= exts;

	if (ib) {
		auto	prims = make_prim_iterator(top, make_indexed_iterator(output_verts.begin(), make_const(ib.begin())));
		for (auto &&i : make_range_n(prims, num_prims))
			*ext++ = get_extent<position3>(i);
	} else {
		auto	prims = make_prim_iterator(top, output_verts.begin());
		for (auto &&i : make_range_n(prims, num_prims))
			*ext++ = get_extent<position3>(i);
	}

	octree		oct;
	oct.init(exts, num_prims);

	uint32		thread	= (pt.x & 1) + (pt.y & 1) * 2;
	float2		qpt		= {pt.x & ~1, pt.y & ~1};
	float2		pt0		= (qpt - viewport.y.xy) / viewport.x.xy;
	ray3		ray		= ray3(position3(pt0, zero), (float3)z_axis);
	triangle3	tri3;
	int			v[3];

	if (ib) {
		auto	prims	= make_prim_iterator(top, make_indexed_iterator(output_verts.begin(), make_const(ib.begin())));
		int		face	= oct.shoot_ray(ray, 0.25f, [prims](int i, param(ray3) r, float &t) {
			return prim_check_ray(prims[i], r, t);
		});
		if (face < 0)
			return Control();

		tri3 = prim_triangle(prims[face]);
		copy(make_prim_iterator(top, ix.begin())[face], v);
	} else {
		auto	prims	= make_prim_iterator(top, output_verts.begin());
		int		face	= oct.shoot_ray(ray, 0.25f, [prims](int i, param(ray3) r, float &t) {
			return prim_check_ray(prims[i], r, t);
		});
		if (face < 0)
			return Control();

		tri3 = prim_triangle(prims[face]);
		copy(make_prim_iterator(top, int_iterator<int>(0))[face], v);
	}
	
	auto	&ps			= shaders[dx::PS];
	auto	*debugger	= new DX12ShaderDebuggerWindow(AddView(false), "Debugger", ps, ps.MakeSimulator(*con, con->cache, true), con, GetSettings("General/shader source").Get(Disassembler::SHOW_SOURCE));

	float2		pt1		= (qpt - viewport.y.xy + one) / viewport.x.xy;
	ray3		ray1	= ray3(position3(pt1, zero), (float3)z_axis);

	plane		p		= tri3.plane();
	float3x4	para	= tri3.inv_matrix();
	float3		uv0		= para * (ray & p);
	float3		uv1		= para * (ray1 & p);

	debugger->sim->SetNumThreads(4);
	debugger->SetThread(thread);

	auto in = ps.GetSignatureIn();
	for (auto& i : in) {
		GetTriangle(sim2, out.find_by_semantic(i.name.get(in), i.semantic_index), v[0], v[1], v[2])
			.Interpolate4(debugger->sim->GetRegFile<float4p>(dx::Operand::TYPE_INPUT, i.register_num).begin(), uv0.x, uv0.y, uv1.x, uv1.y);
	}

	debugger->Update();
	return debugger->control();
}

//-----------------------------------------------------------------------------
//	ViewDX12GPU
//-----------------------------------------------------------------------------

class ViewDX12GPU : public SplitterWindow, public DX12Connection {
public:
	static IDFMT		init_format;
	static win::Colour	colours[];
	static uint8		cursor_indices[][3];

private:
	RefControl<ToolBarControl>	toolbar_gpu;
	ColourTree			tree	= ColourTree(colours, DX12cursors, cursor_indices);
	win::Font			italics;
	HTREEITEM			context_item;
	IDFMT				format;

	void		TreeSelection(HTREEITEM hItem);
	void		TreeDoubleClick(HTREEITEM hItem);

public:
	static ViewDX12GPU*	Cast(Control c)	{
		return (ViewDX12GPU*)SplitterWindow::Cast(c);
	}
	WindowPos Dock(DockEdge edge, uint32 size = 0) {
		return Docker(GetPane(1)).Dock(edge, size);
	}

	Control SelectTab(ID id) {
		int			i;
		if (TabWindow *t = Docker::FindTab(GetPane(1), id, i)) {
			Control	c = t->GetItemControl(i);
			t->SetSelectedControl(t->GetItemControl(i));
			return c;
		}
		return Control();
	}

	void SetOffset(uint32 offset) {
		HTREEITEM h = TVI_ROOT;
		if (device_object && offset >= device_object->info.size32()) {
			auto	call = FindCallTo(offset);
			h = FindOffset(tree, TVI_ROOT, call->srce + call->index, false);
		}
		h = FindOffset(tree, h, offset, false);
		if (h != TVI_ROOT) {
			tree.SetSelectedItem(h);
			tree.EnsureVisible(h);
		}
	}
	void SelectBatch(uint32 batch) {
		SetOffset(batches[batch].end);
	}
	void SelectBatch(BatchList &b, bool always_list = false) {
		int batch = ::SelectBatch(*this, GetMousePos(), b, always_list);
		if (batch >= 0)
			SelectBatch(batch);
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	MakeTree();
	void	ExpandTree(HTREEITEM h);

	ViewDX12GPU(app::MainWindow &_main, const WindowPos &pos, DXConnection *con);
	ViewDX12GPU(app::MainWindow &_main, const WindowPos &pos);
};

//-----------------------------------------------------------------------------
//	DX12DescriptorList
//-----------------------------------------------------------------------------

const char *desc_types[] = {
	"NONE", "CBV", "SRV", "UAV", "RTV", "DSV", "SMP",
	"SSMP",
	"PCBV", "PSRV", "PUAV", "IMM", 0, 0, 0, 0,
};

template<> field fields<DESCRIPTOR>::f[] = {
	{"type",		0,4,	0,0,					desc_types},
	{"resource",	64,64,	field::MODE_CUSTOM,0,	sHex},
	{0,				128,0,	0,2,					(const char**)union_fields<
	_none,
	D3D12_CONSTANT_BUFFER_VIEW_DESC,
	D3D12_SHADER_RESOURCE_VIEW_DESC,
	D3D12_UNORDERED_ACCESS_VIEW_DESC,
	D3D12_RENDER_TARGET_VIEW_DESC,
	D3D12_DEPTH_STENCIL_VIEW_DESC,
	D3D12_SAMPLER_DESC,
	D3D12_STATIC_SAMPLER_DESC,
	D3D12_GPU_VIRTUAL_ADDRESS2,
	D3D12_GPU_VIRTUAL_ADDRESS2,
	D3D12_GPU_VIRTUAL_ADDRESS2,
	_none,
	_none,
	_none,
	_none,
	_none
	>::p},
	0,
};

void InitDescriptorHeapView(ListViewControl lv) {
	lv.SetView(ListViewControl::DETAIL_VIEW);
	lv.SetExtendedStyle(ListViewControl::GRIDLINES | ListViewControl::DOUBLEBUFFER | ListViewControl::FULLROWSELECT);
	lv.AddColumns(
		"index",		50,
		"cpu handle",	100,
		"gpu handle",	100
	);
	int	nc = MakeColumns(lv, fields<DESCRIPTOR>::f, IDFMT_CAMEL, 3);
	while (nc < 20)
		ListViewControl::Column("").Width(100).Insert(lv, nc++);
}

void GetHeapDispInfo(string_accum &sa, ListViewControl &lv, DX12Assets *assets, const RecDescriptorHeap *heap, int row, int col) {
	auto	&d	= heap->descriptors[row];
	switch (col) {
		case 0:		sa << row; break;
		case 1:		sa << "0x" << hex(heap->cpu.ptr + row * heap->stride); break;
		case 2:		sa << "0x" << hex(heap->gpu.ptr + row * heap->stride); break;
		default: {
			uint32	offset = 0;
			const uint32 *p = (const uint32*)&d;
			if (const field *pf = FieldIndex(fields<DESCRIPTOR>::f, col - 3, p, offset, true))
				RegisterList(lv, assets, col > 4 ? IDFMT_FIELDNAME|IDFMT_NOPREFIX : IDFMT_NOPREFIX).FillSubItem(sa, pf, p, offset).term();
			break;
		}
	}
}

struct DX12DescriptorHeapControl : CustomListView<DX12DescriptorHeapControl, Subclass<DX12DescriptorHeapControl, ListViewControl>> {
	enum {ID = 'DH'};
	DX12Assets &assets;
	const RecDescriptorHeap	*heap;

	DX12DescriptorHeapControl(const WindowPos &wpos, text caption, DX12Assets &assets, const RecDescriptorHeap *rec) : Base(wpos, caption, CHILD | CLIPSIBLINGS | VISIBLE | VSCROLL | OWNERDATA | SHOWSELALWAYS, CLIENTEDGE, ID), assets(assets), heap(rec) {
		addref();
		InitDescriptorHeapView(*this);
		SetCount(heap->count);//, LVSICF_NOINVALIDATEALL);
	}

	void	GetDispInfo(string_accum &sa, int row, int col) {
		GetHeapDispInfo(sa, *this, &assets, heap, row, col);
	}

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			uint64		h	= heap->cpu.ptr + heap->stride * row;

			switch (col) {
				case 0: {
					uint32	offset = main->FindDescriptorChange(h, heap->stride);
					main->SetOffset(offset);
					break;
				}
				case 4: {//resource
					if (auto *rec = main->FindObject((uint64)heap->descriptors[row].res)) {
						MakeObjectView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), main, rec);
						break;
					}
				}
				default:
					MakeDescriptorView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), to_string(hex(h)), main, &heap->descriptors[row]);
					break;
			}
		}
	}
};

Control MakeDescriptorHeapView(const WindowPos& wpos, DX12Connection* con, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc) {
	if (auto obj = con->FindDescriptorHeapObject(cpu_desc)) {
		const RecDescriptorHeap	*heap = obj->info;
		auto	index	= heap->index(cpu_desc);
		auto	c		= new DX12DescriptorHeapControl(wpos, obj->name, *con, heap);
		c->SetItemState(index, LVIS_SELECTED|LVIS_FOCUSED);
		c->EnsureVisible(index);
		c->SetSelectionMark(index);
		c->SetFocus();
		return *c;
	}

	return {};
}

struct DX12DescriptorList : CustomListView<DX12DescriptorList, Subclass<DX12DescriptorList, ListViewControl>> {
	enum {ID = 'DL'};
	DX12Connection *con;
	win::Font		bold;

	DX12DescriptorList(const WindowPos &wpos, DX12Connection *_con) : con(_con) {
		Create(wpos, "Descriptors", CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS | OWNERDATA, NOEX, ID);
		SetExtendedStyle(GRIDLINES | DOUBLEBUFFER | FULLROWSELECT);
		InitDescriptorHeapView(*this);
		//EnableGroups();
		bold = GetFont().GetParams().Weight(FW_BOLD);

		size_t	total = 0;
		for (auto &i : con->descriptor_heaps)
			total += i->count + 1;
		SetCount(total, LVSICF_NOINVALIDATEALL);
	}

	void	GetDispInfo(string_accum &sa, int row, int col) {
		const RecDescriptorHeap	*heap = nullptr;
		for (auto &i : con->descriptor_heaps) {
			if (row == 0) {
				if (col == 0)
					sa << i.obj->name << " : 0x" << hex(i->cpu.ptr) << " x " << i->count;
				return;
			}
			--row;
			if (row < i->count) {
				heap = i;
				break;
			}
			row -= i->count;
		}

		if (heap)
			GetHeapDispInfo(sa, *this, con, heap, row, col);
	}

	int		CustomDraw(NMLVCUSTOMDRAW *cd) {
		switch (cd->nmcd.dwDrawStage) {
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;
			case CDDS_ITEMPREPAINT:
				if (cd->nmcd.rc.top) {
					int	row = cd->nmcd.dwItemSpec;
					for (auto &i : con->descriptor_heaps) {
						if (row == 0) {
							DeviceContext	dc(cd->nmcd.hdc);
							Rect			rc(cd->nmcd.rc);
							dc.Select(bold);
							dc.TextOut(rc.TopLeft(), buffer_accum<100>() << i.obj->name << " : 0x" << hex(i->cpu.ptr) << " x " << i->count);
							//return CDRF_NEWFONT;
							return CDRF_SKIPDEFAULT | CDRF_SKIPPOSTPAINT | CDRF_NOTIFYSUBITEMDRAW;
						}
						--row;
						if (row < i->count)
							break;
						row -= i->count;
					}
				}
				break;

			case CDDS_ITEMPREPAINT|CDDS_SUBITEM:
				return CDRF_SKIPDEFAULT | CDRF_SKIPPOSTPAINT;
		}
		return CDRF_DODEFAULT;
	}

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			const RecDescriptorHeap	*heap = 0;
			for (auto &h : con->descriptor_heaps) {
				--row;
				if (row < h->count) {
					heap = h;
					break;
				}
				row -= h->count;
			}
			uint64		h	= heap->cpu.ptr + heap->stride * row;

			switch (col) {
				case 0: {
					uint32	offset = con->FindDescriptorChange(h, heap->stride);
					main->SetOffset(offset);
					break;
				}
				case 4: {//resource
					if (auto *rec = main->FindObject((uint64)heap->descriptors[row].res)) {
						MakeObjectView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), main, rec);
						break;
					}
				}
				default:
					MakeDescriptorView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), to_string(hex(h)), main, &heap->descriptors[row]);
					break;
			}
		}
	}
};

//-----------------------------------------------------------------------------
//	DX12ObjectWindow
//-----------------------------------------------------------------------------

class DX12ObjectWindow : public Window<DX12ObjectWindow> {
public:
	enum {ID = 'OB'};
	DX12Connection		*con;
	ColourTree			tree	= ColourTree(ViewDX12GPU::colours, DX12cursors, ViewDX12GPU::cursor_indices);
	DX12ObjectWindow(const WindowPos &wpos, text title, DX12Connection *con) : con(con) { Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, NOEX, ID); }

	void Init(const DX12Assets::ObjectRecord *obj);
	void Init(const DX12Assets::ResourceRecord *rr);
	void Init(const DESCRIPTOR *desc);
	void InitRayTrace(const DESCRIPTOR *desc);

	WindowPos	AddView(bool new_tab) {
		return Docker(*this).Dock(new_tab ? DOCK_ADDTAB : DOCK_PUSH);
	}

	auto	Split() {
		SplitterWindow	*split = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY, 350);
		split->Create(GetChildWindowPos(), none, CHILD | VISIBLE);
		tree.Create(split->_GetPanePos(0), none, Control::CHILD | Control::CLIPSIBLINGS | Control::VISIBLE | Control::VSCROLL | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT | tree.SHOWSELALWAYS, Control::CLIENTEDGE);
		return split;
	}

	auto	NotSplit() {
		tree.Create(GetChildWindowPos(), none, Control::CHILD | Control::CLIPSIBLINGS | Control::VISIBLE | Control::VSCROLL | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT | tree.SHOWSELALWAYS, Control::CLIENTEDGE);
	}

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case NM_CLICK: {
						if (nmh->hwndFrom == tree) {
							if (HTREEITEM hItem = tree.hot) {
								tree.hot	= 0;
								TreeControl::Item i	= tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE);
								bool	ctrl	= !!(GetKeyState(VK_CONTROL) & 0x8000);
								bool	new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);

								switch (i.Image()) {
									case DX12Assets::WT_OFFSET:
										if (auto main = static_cast<ViewDX12GPU*>(con))
											main->SetOffset(i.Param());
										break;

									case DX12Assets::WT_OBJECT:
										MakeObjectView(AddView(new_tab), con, i.Param());
										break;

									case DX12Assets::WT_SHADER:
										MakeShaderViewer(AddView(new_tab), con, (DX12Assets::ShaderRecord*)i.Param(), ctrl);
										break;

								}
							}
						}
						break;
					}

					case TVN_SELCHANGING:
						return TRUE; // prevent selection

					case LVN_GETDISPINFOW:
						return ReflectNotification(*this, wParam, (NMLVDISPINFOW*)nmh);

					case LVN_GETDISPINFOA:
						return ReflectNotification(*this, wParam, (NMLVDISPINFOA*)nmh);


					case NM_CUSTOMDRAW:
						if (nmh->hwndFrom == tree)
							return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
						return ReflectNotification(*this, wParam, (NMLVCUSTOMDRAW*)nmh);
				}
				break;
			}

			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}
};

void DX12ObjectWindow::Init(const DX12Assets::ObjectRecord *obj) {
	switch (obj->type) {
		case RecObject::Heap: {
			const RecHeap	*r = obj->info;
			BinaryWindow(Split()->_GetPanePos(1), ISO::MakeBrowser(memory_block(con->cache(uint64(r->gpu | GPU), r->SizeInBytes))));
			break;
		}

		case RecObject::Resource:
			Init((DX12Assets::ResourceRecord*)obj->aux);
			return;

		case RecObject::DescriptorHeap:
			new DX12DescriptorHeapControl(Split()->_GetPanePos(1), obj->GetName(), *con, obj->info);
			break;

		case RecObject::PipelineState: {
			NotSplit();
			RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);
			HTREEITEM		h	= rt.AddText(TVI_ROOT, obj->GetName(), 0);

			switch (*(int*)obj->info) {
				case 0:
					rt.AddFields(h, (const D3D12_GRAPHICS_PIPELINE_STATE_DESC*)(obj->info + 8));
					break;
				case 1:
					rt.AddFields(h, (const D3D12_COMPUTE_PIPELINE_STATE_DESC*)(obj->info + 8));
					break;
				case 2: {
					auto	t = rmap_unique<KM, D3D12_PIPELINE_STATE_STREAM_DESC>(obj->info + 8);
					for (auto &sub : make_next_range<dx12::PIPELINE::SUBOBJECT>(const_memory_block(t->pPipelineStateSubobjectStream, t->SizeInBytes))) {
						switch (sub.t.u.t) {
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE: {
								auto	v = get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>(sub);
								if (auto *rec = con->FindObject((uint64)v))
									rt.Add(rt.AddText(h, "pRootSignature"), buffer_accum<256>() << rec->name, DX12Assets::WT_OBJECT, rec);
								break;
							}
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:					rt.AddFields(rt.AddText(h, "VS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:					rt.AddFields(rt.AddText(h, "PS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:					rt.AddFields(rt.AddText(h, "DS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:					rt.AddFields(rt.AddText(h, "HS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:					rt.AddFields(rt.AddText(h, "GS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:					rt.AddFields(rt.AddText(h, "CS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:			rt.AddFields(rt.AddText(h, "StreamOutput"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:					rt.AddFields(rt.AddText(h, "BlendState"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:			rt.AddFields(rt.AddText(h, "SampleMask"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:			rt.AddFields(rt.AddText(h, "RasterizerState"),		&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:			rt.AddFields(rt.AddText(h, "DepthStencilState"),	&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:			rt.AddFields(rt.AddText(h, "InputLayout"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:	rt.AddField(rt.AddText(h, "IBStripCutValue"),		&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:	rt.AddField(rt.AddText(h, "PrimitiveTopologyType"),	&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:	rt.AddFields(rt.AddText(h, "RTVFormats"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:	rt.AddFields(rt.AddText(h, "DSVFormat"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:			rt.AddFields(rt.AddText(h, "SampleDesc"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:				rt.AddFields(rt.AddText(h, "NodeMask"),				&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:			rt.AddFields(rt.AddText(h, "CachedPSO"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:					rt.AddFields(rt.AddText(h, "Flags"),				&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:		rt.AddFields(rt.AddText(h, "DepthStencilState"),	&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:		rt.AddFields(rt.AddText(h, "ViewInstancing"),		&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:					rt.AddFields(rt.AddText(h, "AS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:					rt.AddFields(rt.AddText(h, "MS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>(sub)); break;
						}
					}
					break;
				}
			}
			tree.ExpandItem(h);
			return;
		}

		default:
			NotSplit();
			break;
	}

	RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);
	tree.ExpandItem(rt.AddFields(rt.AddText(TVI_ROOT, obj->GetName(), 0), obj));
}

void DX12ObjectWindow::Init(const DX12Assets::ResourceRecord *rr) {
	const RecResource	*r = rr->res();

	if (r->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
		BinaryWindow(Split()->_GetPanePos(1), ISO::MakeBrowser(memory_block(con->cache(uint64(r->gpu | GPU), r->data_size))));

	} else if (auto bm = con->GetBitmap(rr->DefaultDescriptor())) {
		BitmapWindow(Split()->_GetPanePos(1), bm, rr->GetName(), true);

	} else {
		NotSplit();
	}

	RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);
	tree.ExpandItem(rt.AddFields(rt.AddText(TVI_ROOT, rr->GetName(), 0), r));

	auto	h		= rt.AddText(TVI_ROOT, "Timeline");
#if 1
	for (auto& e : rr->events) {
		switch (e.type) {
			case DX12Assets::ResourceRecord::Event::READ:
				rt.Add(h, string_builder("read at ") << hex(e.addr), DX12Assets::WT_OFFSET, e.addr);
				break;
			case DX12Assets::ResourceRecord::Event::WRITE:
				rt.Add(h, string_builder("write at ") << hex(e.addr), DX12Assets::WT_OFFSET, e.addr);
				break;
			case DX12Assets::ResourceRecord::Event::BARRIER:
				rt.Add(h, string_builder("barrier at ") << hex(e.addr), DX12Assets::WT_OFFSET, e.addr);
				break;
		}
	}
#else
	int		index	= con->objects.index_of(obj);
	for (uint32 b = 0, nb = con->batches.size32() - 1; b < nb; b++) {
		auto	usage = con->GetUsage(b);
		for (const DX12Assets::use *u = usage.begin(), *ue = usage.end(); u != ue; ++u) {
			if (u->type == RecObject::Resource && u->index == index)
				rt.AddText(h, string_builder("read at batch ") << b);
			if (u->type == RecObject::ResourceWrite && u->index == index)
				rt.AddText(h, string_builder("written at batch ") << b);
		}
	}
#endif
}

void DX12ObjectWindow::Init(const DESCRIPTOR *desc) {
	if (auto buf = MakeSimulatorConstantBuffer(con->cache, *desc)) {
		BinaryWindow(Split()->_GetPanePos(1), ISO::MakeBrowser(buf.data()));

	} else if (auto bm = con->GetBitmap(*desc)) {
		auto	c = BitmapWindow(Split()->_GetPanePos(1), bm, 0, true);
		if (con->replay && (desc->type == DESCRIPTOR::RTV || desc->type == DESCRIPTOR::DSV)) {
			Menu menu	= Menu::Popup();
			Menu::Item("Debug this Pixel", DebugWindow::ID_DEBUG_PIXEL).Param(desc).AppendTo(menu);
			c(WM_ISO_CONTEXTMENU, (HMENU)menu);
		}

	} else {
		auto	res = MakeSimulatorResource(*con, con->cache, *desc);
		if (is_buffer(res.dim)) {
			auto	split = Split();

			auto	type = ((const C_type**)(desc + 1))[-1];
			if (!type)
				type = dx::to_c_type(desc->get_format());
			MakeBufferWindow(split->_GetPanePos(1), "Buffer", win::ID(), TypedBuffer(res, res.width, type));
			/*
			if (type) {
				MakeBufferWindow(split->_GetPanePos(1), "Buffer", win::ID(), TypedBuffer(res, res.width, type));
			} else {
				BinaryWindow(split->_GetPanePos(1), ISO::MakeBrowser(res.data()));
			}
			*/
		} else {
			NotSplit();
		}
	}

	RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);

	if (auto heap = con->FindDescriptorHeap(desc)) {
		auto	index	= heap->index(desc);
		auto	cpu		= heap->get_cpu(index);

		rt.AddText(TVI_ROOT, string_builder() << "CPU_DESCRIPTOR_HANDLE = 0x" << hex(cpu.ptr) << ' ' << con->ObjectName(cpu));
		rt.AddText(TVI_ROOT, string_builder() << "GPU_DESCRIPTOR_HANDLE = 0x" << hex(heap->get_gpu(cpu).ptr));
	}

	rt.AddFields(TreeControl::Item("Descriptor").Expand().Insert(rt.tree), desc);

	if (auto obj = con->FindObject((uint64)desc->res)) {
		auto		r		= (const RecResource*)obj->info;
		VIEW_DESC	view	= *desc;
		auto		mip		= r->GetMipRegion(view.first_mip);
		rt.AddFields(TreeControl::Item("Region").Expand().Insert(rt.tree), &mip);
//		rt.AddFields(rt.AddText(TVI_ROOT, obj->GetName()), (const RecResource*)obj->info);
	}
}

C_type_struct c_type_aabb("AABB", C_type::NONE, {
	{"MinX", C_type_float::get<float>()},
	{"MinY", C_type_float::get<float>()},
	{"MinZ", C_type_float::get<float>()},
	{"MaxX", C_type_float::get<float>()},
	{"MaxY", C_type_float::get<float>()},
	{"MaxZ", C_type_float::get<float>()}
});

template<typename T> struct fields<dynamic_array<T>> { static field f[]; };
template<typename T> field fields<dynamic_array<T>>::f[]	= {
	field::make("size", 64, 64, sDec),
	field::call<field::MODE_POINTER>("entries", 0, fields<T>::f, 1),
	0
};

void DX12ObjectWindow::InitRayTrace(const DESCRIPTOR *desc) {
	if (auto top_data = GetSimulatorResourceData(*con, con->cache, *desc)) {
		auto	split	= Split();
		MeshVertexWindow	*m	= new MeshVertexWindow(split->_GetPanePos(1), "Vertices");
		MeshWindow			*mw	= new MeshWindow(m->GetPanePos(1), one, BFC_NONE, MeshWindow::PERSPECTIVE);

		auto	split2	= new MultiSplitterWindow(m->_GetPanePos(0), "Vertices", 0, MultiSplitterWindow::SWF_HORZ);

		RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);
		using	TOP_DESC	= tuple<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS, const_memory_block>;
		auto	top_desc	= rmap_unique<RTM, TOP_DESC>(top_data);

		for (auto &i : make_range<D3D12_RAYTRACING_INSTANCE_DESC>(top_desc->get<1>())) {
			auto		h	= rt.AddFields(TreeControl::Item("Instance").Expand().Insert(rt.tree), &i);
			mat<float,4,3>	mat0;
			load(mat0, i.Transform);
			float3x4	transform	= get(transpose(mat0));

			if (auto bot_data = con->cache(i.AccelerationStructure | GPU)) {
				auto	bot_desc	= rmap_unique<RTM, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>(bot_data);
				rt.AddFields(TreeControl::Item("Bottom").Expand().Insert(rt.tree, h), bot_desc.get());

				for (auto& j : make_range_n(bot_desc->pGeometryDescs, bot_desc->NumDescs)) {
					if (j.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES) {
						if (j.Triangles.Transform3x4) {
							auto	t	= con->GetGPU(j.Triangles.Transform3x4, sizeof(float[3][4]));
							mat<float,4,3>	mat2;
							load(mat2, (float(*)[4])t);
							transform	= get(transpose(mat0)) * get(transpose(mat2));
						}

						int		size			= ByteSize(j.Triangles.IndexFormat);
						auto	IndexBuffer		= indices(con->GetGPU(j.Triangles.IndexBuffer, j.Triangles.IndexCount * size), size, 0, j.Triangles.IndexCount);
						auto	VertexBuffer	= con->GetTypedBuffer(j.Triangles.VertexBuffer, j.Triangles.VertexCount, dx::to_c_type(j.Triangles.VertexFormat));

						auto	mesh			= new MeshInstance(Topology(Topology::TRILIST), VertexBuffer, IndexBuffer, false);
						mesh->transform			= transform;
						mesh->extent			= (transform * mesh->extent).get_box();
						mw->AddDrawable(mesh);

						VertexWindow		*vw	= new VertexWindow(split2->AppendPane(100), "Vertices", 'VI', IndexBuffer);
						vw->AddBuffer(VertexBuffer, "v");
						vw->Show();

					} else {
						auto	VertexBuffer	= con->GetTypedBuffer(j.AABBs.AABBs,  j.AABBs.AABBCount, (const C_type*)&c_type_aabb);
						auto	aabbs			= new MeshAABBInstance(VertexBuffer);
						aabbs->transform		= transform;
						mw->AddDrawable(aabbs);

						VertexWindow		*vw	= new VertexWindow(split2->AppendPane(100), "Vertices", 'VI', {});
						vw->AddBuffer(VertexBuffer, "aabb");
						vw->Show();

					}
				}
			}

		}
	}
}

Control MakeObjectView(const WindowPos &wpos, DX12Connection *con, const DX12Assets::ObjectRecord *obj) {
	auto	win = new DX12ObjectWindow(wpos, obj->GetName(), con);
	win->Init(obj);
	return *win;
}

Control MakeResourceView(const WindowPos &wpos, DX12Connection *con, const DX12Assets::ResourceRecord *rr) {
	auto	win = new DX12ObjectWindow(wpos, rr->GetName(), con);
	win->Init(rr);
	return *win;
}

Control MakeDescriptorView(const WindowPos& wpos, const char* title, DX12Connection* con, const DESCRIPTOR *desc) {
	auto	win = new DX12ObjectWindow(wpos, title, con);
	win->Init(desc);
	return *win;
}

Control MakeRayTraceView(const WindowPos& wpos, const char* title, DX12Connection* con, const DESCRIPTOR *desc) {
	auto	win		= new DX12ObjectWindow(wpos, title, con);
	win->InitRayTrace(desc);
	return *win;
}

//-----------------------------------------------------------------------------
//	DX12ResourcesList
//-----------------------------------------------------------------------------

struct DX12ResourcesList : EditableListView<DX12ResourcesList, Subclass<DX12ResourcesList, EntryTable<DX12Assets::ResourceRecord>>> {
	enum {ID = 'RL'};
	DX12Connection*		con;
	BatchListArray		writtenat;
	ImageList			images;

	void	SortOnColumn(int col) {
		int	dir = SetColumn(GetHeader(), col);
		switch (col) {
			case 0:
				SortByIndex(ColumnTextSorter(*this, col, dir));
				break;
			case 1:
				SortByParam(UsageSorter<DX12Assets::ResourceRecord>(table, usedat, dir));
				break;
			case 2:
				SortByParam(UsageSorter<DX12Assets::ResourceRecord>(table, writtenat, dir));
				break;
			case 3:
				SortByParam(LambdaColumnSorter([](DX12Assets::ResourceRecord *a, DX12Assets::ResourceRecord *b) { return simple_compare(a->states.init.state, b->states.init.state); }, dir));
				break;
			default:
				SortByParam(IndirectFieldSorter(fields<DX12Assets::ResourceRecord>::f, col - 1, dir));
				break;
		}
	}
	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			auto	*t	= GetEntry(row);
			int		i	= GetEntryIndex(row);
			switch (col) {
	/*			case 0:
					EditName(row);
					break;*/
				case 1:
					ViewDX12GPU::Cast(from)->SelectBatch(usedat[i]);
					break;
				case 2:
					ViewDX12GPU::Cast(from)->SelectBatch(writtenat[i]);
					break;
				default: {
					Busy		bee;
					MakeResourceView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), con, t);
					break;
				}
			}
		}
	}

	DX12ResourcesList(const WindowPos &wpos, DX12Connection *_con) : Base(_con->resources, IDFMT_FOLLOWPTR | IDFMT_NOPREFIX), con(_con)
		, images(ImageList::Create(DeviceContext::ScreenCaps().LogPixels() * (2 / 3.f), ILC_COLOR32, 1, 1))
	{
		//CreateNoColumns(wpos, "Resources", ID, CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS);
		Create(wpos, "Resources", ID, CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS);
		addref();

		while (NumColumns())
			DeleteColumn(0);
		AddColumns(
			"name",			200,
			"used at",		100,
			"written at",	100,
			"state",		100
		);
		MakeColumns<RecResource>(*this, IDFMT_CAMEL | IDFMT_FOLLOWPTR, 4);
		//for (int i = 0, nc = NumColumns(); i < 13; i++)
		//	Column("").Width(100).Insert(*this, nc++);

		writtenat.resize(table.size());

		SetIcons(images);
		SetSmallIcons(images);
		SetView(DETAIL_VIEW);
		TileInfo(4).Set(*this);
		images.Add(win::Bitmap::Load("IDB_BUFFER", 0, images.GetIconSize()));
		images.Add(win::Bitmap::Load("IDB_NOTLOADED", 0, images.GetIconSize()));
			   
		addref();
		RunThread([this]{
			for (uint32 b = 0, nb = con->batches.size32() - 1; b < nb; b++) {
				auto	usage = con->GetUsage(b);
				for (const DX12Assets::use *u = usage.begin(), *ue = usage.end(); u != ue; ++u) {
					if (u->type == RecObject::Resource)
						usedat[con->resources.index_of((DX12Assets::ResourceRecord*)con->objects[u->index].aux)].push_back(b);
					if (u->type == RecObject::ResourceWrite)
						writtenat[con->resources.index_of((DX12Assets::ResourceRecord*)con->objects[u->index].aux)].push_back(b);
				}
			}

			static const uint32	cols[]	= {2, 9, 10, 22};

			for (auto &i : table) {
				JobQueue::Main().add([this, &i] {
					auto	*r	= i.res();
					int		x	= r->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? 0 : !r->HasData() ? 1 : GetThumbnail(*this, images, con, &i);
					int		j	= table.index_of(i);

					Item	item;
					item.Text(i.GetName()).Image(x).Param(&i).Insert(*this);
					item.Column(1).Text(WriteBatchList(lvalue(buffer_accum<256>()), usedat[j])).Set(*this);
					item.Column(2).Text(WriteBatchList(lvalue(buffer_accum<256>()), writtenat[j])).Set(*this);

					buffer_accum<256>	ba;
					item.Column(3).Text(PutConst(ba, format, i.states.init.state)).Set(*this);

					FillRow(*this, none, item.Column(4), format, i.res());

					//AddEntry(*i, j, x);
					GetItem(j).TileInfo(cols).Set(*this);
				});
			}

			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	DX12ShadersList
//-----------------------------------------------------------------------------

struct DX12ShadersList : EditableListView<DX12ShadersList, Subclass<DX12ShadersList, EntryTable<DX12Assets::ShaderRecord>>> {
	enum {ID = 'SH'};
	DX12Connection *con;

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			switch (col) {
	/*			case 0:
					EditName(row);
					break;*/
				case 1:
					ViewDX12GPU::Cast(from)->SelectBatch(GetBatches(row));
					break;
				default:
					Busy(), MakeShaderViewer(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), con, GetEntry(row), !!(GetKeyState(VK_CONTROL) & 0x8000));
					break;
			}
		}
	}

	DX12ShadersList(const WindowPos &wpos, DX12Connection *_con) : Base(_con->shaders), con(_con) {
		Create(wpos, "Shaders", ID, CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS);

		//Init();

		addref();
		RunThread([this]{
			con->GetUsedAt(usedat, RecObject::Shader);

			for (auto &i : table)
				AddEntry(i, table.index_of(i));
		#if 0
			const char *look_for = "dx.op.discard";
			for (auto &i : table) {
				bitcode::Module mod;
				if (ReadDXILModule(mod, i.DXBC()->GetBlob<dx::DXBC::DXIL>())) {
					for (auto& f : mod.functions) {
						for (auto& i : f->instructions) {
							if (i->op == bitcode::Operation::Call) {
								//ISO_TRACEF(i.funcCall->name) << '\n';
								if (i.funcCall->name == look_for) {
									ISO_TRACEF("found ") << look_for << " in " << f->name << '\n';
								}
							}
						}

					}
				}
			}
		#endif
			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	DX12ObjectsList
//-----------------------------------------------------------------------------

struct DX12ObjectsList : EditableListView<DX12ObjectsList, Subclass<DX12ObjectsList, EntryTable<DX12Assets::ObjectRecord>>> {
	enum {ID = 'OL'};
	DX12Connection *con;

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			switch (col) {
				case 0:
					main->SetOffset(main->FindObjectCreation(GetEntry(row)->obj));
					//EditName(row);
					break;
				case 1:
					ViewDX12GPU::Cast(from)->SelectBatch(GetBatches(row));
					break;
				default:
					Busy(), MakeObjectView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), con, GetEntry(row));
					break;
			}
		}
	}

	void	operator()(string_accum &sa, const field *pf, const uint32le *p, uint32 offset) const {
		uint64		v = pf->get_raw_value(p, offset);

		if (pf->is_type<D3D12_GPU_DESCRIPTOR_HANDLE>()) {
			D3D12_GPU_DESCRIPTOR_HANDLE h;
			h.ptr = v;
			sa << "0x" << hex(v) << con->ObjectName(h);

		} else if (pf->is_type<D3D12_CPU_DESCRIPTOR_HANDLE>()) {
			D3D12_CPU_DESCRIPTOR_HANDLE h;
			h.ptr = v;
			sa << "0x" << hex(v) << con->ObjectName(h);

		} else if (v == 0) {
			sa << "nil";

		} else if (auto *rec = con->FindObject(v)) {
			sa << rec->name;

		} else if (auto *rec = con->FindShader(v)) {
			sa << rec->name;

		} else {
			sa << "(unfound)0x" << hex(v);
		}
	}

	DX12ObjectsList(const WindowPos &wpos, DX12Connection *_con) : Base(_con->objects), con(_con) {
		Create(wpos, "Objects", ID, CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS);
		addref();
		int	nc = NumColumns();
		for (int i = 0; i < 8; i++)
			ListViewControl::Column("").Width(100).Insert(*this, nc++);

		SetView(DETAIL_VIEW);

		addref();
		RunThread([this]{
			for (uint32 b = 0, nb = con->batches.size32() - 1; b < nb; b++) {
				auto	usage = con->GetUsage(b);
				for (const DX12Assets::use *u = usage.begin(), *ue = usage.end(); u != ue; ++u) {
					if (u->type != RecObject::Shader)
						usedat[u->index].push_back(b);
				}
			}

			for (auto &i : table) {
				JobQueue::Main().add([this, &i] {
					AddEntry(this, i, table.index_of(i));
				});
			}

			release();
			return 0;
		});
	}
	~DX12ObjectsList() {}
};

//-----------------------------------------------------------------------------
//	DX12TimingWindow
//-----------------------------------------------------------------------------

class DX12TimingWindow : public TimingWindow {
	DX12Connection	&con;
	float			rfreq;
	
	void	Paint(const win::DeviceContextPaint &dc);
	int		GetBatch(float t)		const	{ return t < 0 ? 0 : con.batches.index_of(con.GetBatchByTime(uint64(t * con.replay->frequency))); }
	float	GetBatchTime(int i)		const	{ return float(con.batches[i].timestamp) * rfreq; }
	void	SetBatch(int i);
	void	GetTipText(string_accum &&acc, float t) const;

public:
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	DX12TimingWindow(const WindowPos &wpos, Control owner, DX12Connection &con) : TimingWindow(wpos, owner), con(con) {
		Reset();
		Rebind(this);
		Invalidate();
	}
	void	Reset();
	void	Refresh() {
		Reset();
		Invalidate();
	}
};

void DX12TimingWindow::Reset() {
	time		= 0;
	last_batch	= 0;
	rfreq		= 1.f / con.replay->frequency;
	tscale		= ((con.batches.back().timestamp - con.batches.front().timestamp)) * rfreq;

	float maxy	= 1;
	for (auto &b : con.batches)
		maxy = max(maxy, (b.stats.PSInvocations + b.stats.CSInvocations) / float(b.Duration()));
	yscale		= maxy * con.replay->frequency * 1.1f;
}

void DX12TimingWindow::SetBatch(int i) {
	if (i != last_batch) {
		int	x0	= TimeToClient(GetBatchTime(last_batch));
		int	x1	= TimeToClient(GetBatchTime(i));
		if (x1 < x0)
			swap(x0, x1);

		Invalidate(DragStrip().SetLeft(x0 - 10).SetRight(x1 + 10), false);
		last_batch = i;
		Update();
	}
}
void DX12TimingWindow::GetTipText(string_accum &&acc, float t) const {
	acc << "t=" << t * 1000 << "ms";
	int	i = GetBatch(t);
	if (i < con.batches.size())
		acc << "; batch=" << i - 1 << " (" << GetBatchTime(i) << "ms)";
}

LRESULT DX12TimingWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			return 0;

		case WM_PAINT:
			Paint(hWnd);
			break;

		case WM_ISO_BATCH:
			SetBatch(wParam + 1);
			break;

		case WM_RBUTTONDOWN:
			SetCapture(*this);
			SetFocus();
			break;

		case WM_LBUTTONDOWN: {
			SetCapture(*this);
			SetFocus();
			Point	mouse(lParam);
			int	batch = GetBatch(ClientToTime(mouse.x));
			if (batch && batch < con.batches.size() && DragStrip().Contains(mouse)) {
				owner(WM_ISO_BATCH, batch - 1);
				SetBatch(batch - 1);
			}
			break;
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			ReleaseCapture();
			return 0;

		case WM_MOUSEMOVE: {
			Point	mouse	= Point(lParam);
			if (wParam & MK_LBUTTON) {
				float	prev_time	= time;
				time -= (mouse.x - prevmouse.x) * tscale / GetClientRect().Width();
				Invalidate();
			} else if (wParam & MK_RBUTTON) {
				int	i = min(GetBatch(ClientToTime(mouse.x)), con.batches.size() - 1);
				if (i != last_batch && i)
					owner(WM_ISO_JOG, i - 1);
				SetBatch(i);
			}
			prevmouse	= mouse;
			break;
		}

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA: {
					NMTTDISPINFOA	*nmtdi	= (NMTTDISPINFOA*)nmh;
					GetTipText(fixed_accum(nmtdi->szText), ClientToTime(ToClient(GetMousePos()).x));
					break;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;
	}
	return TimingWindow::Proc(message, wParam, lParam);
}

D3D12_QUERY_DATA_PIPELINE_STATISTICS &operator+=(D3D12_QUERY_DATA_PIPELINE_STATISTICS &a, D3D12_QUERY_DATA_PIPELINE_STATISTICS &b) {
	a.IAVertices	+= b.IAVertices;
	a.IAPrimitives	+= b.IAPrimitives;
	a.VSInvocations	+= b.VSInvocations;
	a.GSInvocations	+= b.GSInvocations;
	a.GSPrimitives	+= b.GSPrimitives;
	a.CInvocations	+= b.CInvocations;
	a.CPrimitives	+= b.CPrimitives;
	a.PSInvocations	+= b.PSInvocations;
	a.HSInvocations	+= b.HSInvocations;
	a.DSInvocations	+= b.DSInvocations;
	a.CSInvocations	+= b.CSInvocations;
	return a;
}

void DX12TimingWindow::Paint(const DeviceContextPaint &dc) {
	Rect		client	= GetClientRect();

	d2d.Init(*this, client.Size());
	if (!d2d.Occluded()) {
		d2d::rect	dirty	= d2d.FromPixels(d2d.device ? dc.GetDirtyRect() : client);

		d2d.BeginDraw();
		d2d.SetTransform(identity);
		d2d.device->PushAxisAlignedClip(d2d::rect(dirty), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		d2d.Clear(colour(zero));

		auto		wh		= d2d.Size();
		float		w		= wh.x, h = wh.y;
		float2x3	trans	= GetTrans();
		d2d::rect	dirty2	= dirty / trans;

		int			batch0	= max(GetBatch(dirty2.left) - 1, 0);
		int			batch1	= min(GetBatch(dirty2.right), con.batches.size() - 1);

		{// batch bands
			d2d::SolidBrush	grey(d2d, colour(0.2f,0.2f,0.2f,1));
			for (int i = batch0 & ~1; i < batch1; i += 2)
				d2d.Fill(d2d::rect(TimeToClient(GetBatchTime(i + 0)), 0, TimeToClient(GetBatchTime(i + 1)), h), grey);
		}

		{// batch bars
			d2d::SolidBrush	cols[] = {
				{d2d, colour(1,0,0)},
				{d2d, colour(0,1,0)},
				{d2d, colour(0,0,1)}
			};

			float	xs	= trans.x.x;
			for (int i = batch0; i < batch1;) {
				//collect batches that are too close horizontally
				int		i0		= i++;
				uint64	tn		= con.batches[i0].timestamp + uint32(con.replay->frequency / xs);
				auto	stats	= con.batches[i0].stats;
				while (i < batch1 && con.batches[i].timestamp < tn) {
					stats += con.batches[i].stats;
					++i;
				}

				float	d	= (con.batches[i].timestamp - con.batches[i0].timestamp) * rfreq;
				float	t0	= GetBatchTime(i0);
				float	t1	= GetBatchTime(i);

				uint64	y0	= stats.PSInvocations, y1 = y0 + stats.CSInvocations, y2 = y1 + stats.VSInvocations;
				d2d.Fill(trans * d2d::rect(t0, 0, t1, y0 / d), cols[0]);
				d2d.Fill(trans * d2d::rect(t0, y0 / d, t1, y1 / d), cols[1]);
				d2d.Fill(trans * d2d::rect(t0, y1 / d, t1, y2 / d), cols[2]);
			}
		}

		d2d::Write	write;

		{// batch labels
			d2d::Font	font(write, L"arial", 20);
			font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
			d2d::SolidBrush	textbrush(d2d, colour(one));

			float	t0	= -100;
			float	s	= trans.x.x * rfreq;
			float	o	= trans.z.x;
			float	y	= 20;
			for (int i = batch0; i < batch1; i++) {
				float	t1	= ((con.batches[i].timestamp + con.batches[i+1].timestamp) / 2) * s + o;
				if (t1 > t0) {
					d2d.DrawText(d2d::rect(t1 - 100, h - y, t1 + 100, h), str16(buffer_accum<256>() << i), font, textbrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
					y = 20 + 24 - y;
					t0 = t1 + 50;
				}
			}
		}

		DrawGrid(d2d, d2d::Font(write, L"arial", 10));
		DrawMarker(d2d, d2d::point(TimeToClient(GetBatchTime(last_batch)), 0));

		d2d.device->PopAxisAlignedClip();

		if (d2d.EndDraw()) {
			d2d.DeInit();
			marker_geom.clear();
		}
	}
}

//-----------------------------------------------------------------------------
//	ViewDX12GPU
//-----------------------------------------------------------------------------

IDFMT	ViewDX12GPU::init_format = IDFMT_LEAVE | IDFMT_FOLLOWPTR;

win::Colour ViewDX12GPU::colours[] = {
	{0,0,0},		//WT_NONE,
	{64,0,0,1},		//WT_OFFSET,
	{0,0,0,0},		//WT_BATCH,
	{128,0,0,2},	//WT_MARKER,
	{0,64,0,1},		//WT_CALLSTACK,
	{0,0,64},		//WT_REPEAT,
	{0,128,0},		//WT_OBJECT
	{0,0,128},		//WT_SHADER
	{0,128,128},	//WT_DESCRIPTOR
};

uint8 ViewDX12GPU::cursor_indices[][3] = {
	{0,0,0},		//WT_NONE,
	{1,2,2},		//WT_OFFSET,
	{1,2,2},		//WT_BATCH,
	{0,0,0},		//WT_MARKER,
	{0,0,0},		//WT_CALLSTACK,
	{0,0,0},		//WT_REPEAT,
	{1,2,2},		//WT_OBJECT
	{1,2,2},		//WT_DESCRIPTOR
};


LRESULT ViewDX12GPU::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			tree.Create(_GetPanePos(0), none, CHILD | CLIPSIBLINGS | VISIBLE | VSCROLL | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT | tree.SHOWSELALWAYS, CLIENTEDGE);
			italics = tree.GetFont().GetParams().Italic(true);
			TabWindow	*t = new TabWindow;
			SetPanes(tree, t->Create(_GetPanePos(1), "tabs", CHILD | CLIPCHILDREN | VISIBLE), 400);
			t->SetFont(win::Font::DefaultGui());
			return 0;
		}

		case WM_CTLCOLORSTATIC:
			return (LRESULT)GetStockObject(WHITE_BRUSH);

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_FILE_SAVE: {
					filename	fn;
					if (GetSave(*this, fn, "Save Capture", "Binary\0*.ib\0Compressed\0*.ibz\0Text\0*.ix\0")) {
						Busy(), Save(fn);
					#ifndef ISO_EDITOR
						MainWindow::Get()->SetFilename(fn);
					#endif
					}
					return 1;
				}

				case ID_DX12GPU_EXPANDALL:
					tree.ExpandItem(context_item);
					for (auto i : tree.ChildItems(context_item))
						tree.ExpandItem(i);
					return 1;

				case ID_DX12GPU_OFFSETS:
					init_format = (format ^= IDFMT_OFFSETS);
					Busy(), MakeTree();
					return 1;

/*				case ID_DX12GPU_SHOWMARKERS:
					init_flags = flags.flip(PM4Tree::MARKERS);
					Busy(), MakeTree();
					break;
					*/
				case ID_DX12GPU_ALLOBJECTS:
					if (!SelectTab(DX12ObjectsList::ID))
						new DX12ObjectsList(Dock(DOCK_TAB), this);
					return 1;

				case ID_DX12GPU_ALLRESOURCES:
					if (!SelectTab(DX12ResourcesList::ID))
						new DX12ResourcesList(Dock(DOCK_TAB), this);
					return 1;

				case ID_DX12GPU_ALLDESCRIPTORS:
					if (!SelectTab(DX12DescriptorList::ID))
						new DX12DescriptorList(Dock(DOCK_TAB), this);
					return 1;

				case ID_DX12GPU_ALLSHADERS:
					if (!SelectTab(DX12ShadersList::ID))
						new DX12ShadersList(Dock(DOCK_TAB), this);
					return 1;

				case ID_DX12GPU_TIMER:
					if (Control c = SelectTab(DX12TimingWindow::ID)) {
						ConcurrentJobs::Get().add([this, c] {
							if (GetStatistics()) {
								CastByProc<DX12TimingWindow>(c)->Reset();
								c.Invalidate();
							}
						});
					} else {
						ConcurrentJobs::Get().add([this] {
							if (GetStatistics()) {
								JobQueue::Main().add([this] {
									new DX12TimingWindow(Dock(DOCK_TOP, 100), *this, *this);
								});
							}
						});
					}
					return 1;

				default: {
					static bool reentry = false;
					if (!reentry) {
						reentry = true;
						int	ret = GetPane(1)(message, wParam, lParam);
						reentry = false;
						return ret;
					}
					break;
				}
			}
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CLICK: {
					if (nmh->hwndFrom == *this)
						return 0;

					if (nmh->hwndFrom == tree) {
						uint32		flags;
						HTREEITEM	h = tree.HitTest(tree.ToClient(GetMousePos()), &flags);
						if (h && (flags & TVHT_ONITEMLABEL) && h == tree.GetSelectedItem())
							TreeSelection(h);
						break;
					}
					
					Control(nmh->hwndFrom).SetFocus();

					NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
					ListViewControl	lv		= nmh->hwndFrom;
					int				i		= nmlv->iItem;
					bool			new_tab	= !!(nmlv->uKeyFlags & LVKF_SHIFT);
					bool			ctrl	= !!(nmlv->uKeyFlags & LVKF_CONTROL);

					if (i >= 0) switch (wParam) {
						case DX12DescriptorList::ID: {
							DX12DescriptorList	*dl = DX12DescriptorList::Cast(lv);
							dl->LeftClick(*this, i, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
							break;
						}
						case DX12DescriptorHeapControl::ID: {
							DX12DescriptorHeapControl	*hc	= DX12DescriptorHeapControl::Cast(lv);
							hc->LeftClick(*this, i, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
							break;
						}
						default:
							return ReflectNotification(*this, wParam, (NMITEMACTIVATE*)nmh);
					}
					break;
				}

				case NM_RCLICK: {
					NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
					switch (wParam) {
						case DX12ResourcesList::ID: {
							NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
							ListViewControl	lv		= nmh->hwndFrom;
							NMITEMACTIVATE	nmlv2	= *nmlv;
							nmlv2.hdr.hwndFrom		= *this;
							return lv(WM_NOTIFY, wParam, &nmlv2);
						}
					}
					break;
				}

				case NM_DBLCLK:
					if (nmh->hwndFrom == tree) {
						TreeDoubleClick(tree.GetSelectedItem());
						return TRUE;	// prevent opening tree item
					}
					break;

				case LVN_COLUMNCLICK:
					switch (nmh->idFrom) {
						case DX12ObjectsList::ID:
							DX12ObjectsList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							break;
						case DX12ResourcesList::ID:
							DX12ResourcesList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							break;
						case DX12ShadersList::ID:
							DX12ShadersList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							break;
					}
					break;

				case TVN_ITEMEXPANDING: {
					NMTREEVIEW	*nmtv	= (NMTREEVIEW*)nmh;
					if (nmtv->hdr.hwndFrom == tree && nmtv->action == TVE_EXPAND && !(nmtv->itemNew.state & TVIS_EXPANDEDONCE) && !tree.GetChildItem(nmtv->itemNew.hItem))
						ExpandTree(nmtv->itemNew.hItem);
					return 0;
				}

				case LVN_GETDISPINFOW:
					return ReflectNotification(*this, wParam, (NMLVDISPINFOW*)nmh);

				case LVN_GETDISPINFOA:
					return ReflectNotification(*this, wParam, (NMLVDISPINFOA*)nmh);

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					return ReflectNotification(*this, wParam, (NMLVCUSTOMDRAW*)nmh);

				case TVN_SELCHANGED:
					if (nmh->hwndFrom == tree && tree.IsVisible())
						TreeSelection(tree.GetSelectedItem());
					break;

				case TCN_SELCHANGE: {
				//	TabControl2(nmh->hwndFrom).ShowSelectedControl();
					break;
				}

				//case TCN_DRAG:
				//	DragTab(*this, nmh->hwndFrom, nmh->idFrom, !!(GetKeyState(VK_SHIFT) & 0x8000));
				//	return 1;
				//
				//case TCN_CLOSE:
				//	TabControl2(nmh->hwndFrom).GetItemControl(nmh->idFrom).Destroy();
				//	return 1;
			}
			break;//return 0;
		}

		case WM_MOUSEACTIVATE:
			return MA_ACTIVATE;

		case WM_PARENTNOTIFY:
			if (LOWORD(wParam) == WM_DESTROY && GetPane(1) == Control(lParam)) {
				TabWindow	*t = new TabWindow;
				SetPane(1, t->Create(_GetPanePos(1), "tabs", CHILD | CLIPCHILDREN | VISIBLE));
				t->SetFont(win::Font::DefaultGui());
				return 1;
			}
			break;

		case WM_CONTEXTMENU: {
			if (context_item = tree.HitTest(tree.ToClient(GetMousePos()))) {
				Menu				menu = Menu(IDR_MENU_DX12GPU).GetSubMenuByPos(0);
				menu.Separator();
				menu.Append("Expand All", ID_DX12GPU_EXPANDALL);
			#if 0
				TreeControl::Item	item = tree.GetItem(context_item, TVIF_IMAGE|TVIF_PARAM);
				switch (item.Image()) {
					case WT_BATCH:
						break;

					case WT_CALLSTACK:
						break;

					case WT_REPEAT: {
						menu.Separator();
						menu.Append("Expand All", ID_DX12GPU_EXPANDALL);
						break;
					}
				}
			#endif
				menu.Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
			}
			break;
		}

		case WM_ISO_BATCH:
			SelectBatch(wParam);
			break;

		case WM_ISO_JOG: {
			uint32		batch	= wParam;
			uint32		addr	= batches[batch].end;
			DX12State	state	= GetStateAt(addr);

			replay->RunTo(addr, fails);

			if (state.IsGraphics() && state.graphics_pipeline.RTVFormats.NumRenderTargets > 0) {
			#if 1
				auto	p	= replay->GetBitmap("target", state.GetDescriptor(this, state.targets[0]));
				Control	c	= SelectTab('JG');
				if (c) {
					ISO::Browser2	b(p);
					c.SendMessage(WM_COMMAND, ID_EDIT, &b);
				} else {
					c		= BitmapWindow(Dock(DOCK_TAB), p, tag(p.ID()), true);
					c.id	= 'JG';
				}
			#else
				dx::Resource	res = replay->GetResource(state.GetDescriptor(this, state.targets[0]), cache);
				if (res) {
					auto	p	= dx::GetBitmap("target", res->DefaultDescriptor());
					Control	c	= SelectTab('JG');
					if (c) {
						ISO::Browser2	b(p);
						c.SendMessage(WM_COMMAND, ID_EDIT, &b);
					} else {
						c		= BitmapWindow(Dock(DOCK_TAB), p, tag(p.ID()), true);
						c.id	= 'JG';
					}
				}
			#endif
			}
			break;
		}
		case WM_DESTROY:
			return 0;
		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

void ViewDX12GPU::TreeSelection(HTREEITEM hItem) {
	TreeControl::Item	i		= tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_STATE);
	bool				new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);
	uint32				addr	= i.Param();

	switch (i.Image()) {
		case WT_BATCH: {
			auto		b		= GetBatch(addr);
			addr	= b->end;
			DX12State	state	= GetStateAt(addr);

			if (replay)
				replay->RunTo(addr, fails);
			
			new DX12BatchWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, DX12BatchWindow::ID), tree.GetItemText(hItem), this, state);
			break;
		}

		case WT_CALLSTACK: {
			Busy	bee;
			uint64	pc		= i.Image2() ? *(uint64*)i.Param() : (uint64)i.Param();
			auto	frame	= stack_dumper.GetFrame(pc);
			if (frame.file && exists(frame.file)) {
				auto	file	= files.get(frame.file);
				EditControl	c = MakeSourceWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'SC'), file, HLSLcolourerRE(), none, EditControl::READONLY);
				c.id	= 'SC';
				ShowSourceLine(c, frame.line);
			}
			break;
		}

		case WT_OBJECT: {
			Busy(), MakeObjectView(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'SC'), this, i.Param());
			break;
		}

		case WT_DESCRIPTOR: {
			auto	cpu		= (D3D12_CPU_DESCRIPTOR_HANDLE)i.Param();
		#if 1
			Busy(), MakeDescriptorHeapView(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'DV'), this, cpu);
		#elif 1
			auto	obj		= FindDescriptorHeapObject(cpu);
			Busy(), new DX12DescriptorHeapControl(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'DV'), "title", *this, obj->info);
		#else
			auto	desc	= FindDescriptor(cpu);
			Busy(), MakeDescriptorView(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'DV'), "title", this, desc);
		#endif
			break;
		}
	}
}

void ViewDX12GPU::TreeDoubleClick(HTREEITEM hItem) {
}

int	CountRepeats(const COMRecording::header *&P, const COMRecording::header *e) {
	const COMRecording::header *p = P;
	if (p->id == 0xfffe)
		p = p->next();

	int	count	= 0;
	int	id		= p->id;

	do {
		++count;
		P = p = p->next();
		if (p->id == 0xfffe)
			p = p->next();
	} while (p < e && p->id == id);

	return count;
}

static void PutRepeat(RegisterTree &rt, HTREEITEM h, field_info *nf, int id, uint32 addr, int count) {
	buffer_accum<256>	ba;
	if (rt.format & IDFMT_OFFSETS)
		ba.format("%08x: ", addr);

	auto	name	= str(nf[id].name);
	TreeControl::Item(ba << name.slice_to(name.find('%')) << " x " << count).Image(ViewDX12GPU::WT_REPEAT).Param(addr).Children(1).Insert(rt.tree, h);
}

static void PutCallstack(RegisterTree &rt, HTREEITEM h, const void **callstack) {
	TreeControl::Item("callstack").Image(ViewDX12GPU::WT_CALLSTACK).Param(callstack).Image2(1).Children(1).Insert(rt.tree, h);
}

//string_accum& FormatCommand(string_accum &sa, const char *_name, field *fields, const uint32 *p, IDFMT format, callback<string_accum&(string_accum&, const field*, const uint32le*, uint32)> cb) {
string_accum& FormatCommand(string_accum &sa, cstring name, field *fields, const uint32 *p, IDFMT format, DX12Assets &cb) {

	while (auto pc = name.find('%')) {
		sa << name.slice_to(pc);
		uint32	n;
		name = pc + 1 + from_string(pc + 1, n);

		uint32	offset	= 0;
		auto	p1		= p;
		auto	*f		= FieldIndex(fields, n, p1, offset);
		if (f->offset == field::MODE_CUSTOM)
			cb(sa, f, p1, offset);
		else
			PutConst(sa, format, f, p, 0);
	}
	return sa << name;
}

string_accum& FormatCommand(string_accum &sa, field_info &info, const uint32 *p, IDFMT format, DX12Assets &cb) {
	return FormatCommand(sa, info.name, info.fields, p, format, cb);
}

static HTREEITEM PutCommand(RegisterTree &rt, HTREEITEM h, field_info *nf, const COMRecording::header *p, uint32 addr, DX12Assets &assets) {
	buffer_accum<256>	ba;

	if (rt.format & IDFMT_OFFSETS)
		ba.format("%08x: ", addr);

	auto	name	= str(nf[p->id].name);
	auto	fields	= nf[p->id].fields;

	FormatCommand(ba, name, fields, (const uint32*)p->data(), rt.format, assets);

	if (name == "CopyDescriptors") {
		COMParse(p->data(), [&](UINT dest_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE, 0> dest_starts, counted<const UINT, 0> dest_sizes, UINT srce_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE, 3> srce_starts, counted<const UINT, 3> srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type) {
			ba	<< ifelse(dest_num == 1, assets.ObjectName(dest_starts[0]), " ...")
				<< " <-"
				<< ifelse(srce_num == 1, assets.ObjectName(srce_starts[0]), " ...");
		});
	}

	return rt.AddFields(rt.AddText(h, ba, addr), fields, (uint32*)p->data(), addr);
}

static void PutCommands(RegisterTree &rt, HTREEITEM h, field_info *nf, const_memory_block m, intptr_t offset, DX12Assets &assets, bool check_repeats) {
	for (const COMRecording::header *p = m, *e = (const COMRecording::header*)m.end(); p < e; ) {
		const void**	callstack	= 0;
		uint32			addr		= uint32((intptr_t)p + offset);

		if (p->id == 0xfffe) {
			callstack = (const void**)p->data();
			p = p->next();
		}

		auto	*p0	= p;
		if (check_repeats) {
			int		count	= CountRepeats(p, e);
			if (count > 2) {
				PutRepeat(rt, h, nf, p0->id, addr, count);
				continue;
			}
		}

		HTREEITEM	h2 = PutCommand(rt, h, nf, p0, addr, assets);
		if (callstack)
			PutCallstack(rt, h2, callstack);

		p = p0->next();
	}
}

bool HasBatches(const interval<uint32> &r, DX12Assets &assets) {
	if (!r.empty()) {
		const DX12Assets::BatchRecord	*b	= assets.GetBatch(r.begin());
		return b->end <= r.end() && b->op;
	}
	return false;
}

range<const DX12Assets::BatchRecord*> UsedBatches(const interval<uint32> &r, DX12Assets &assets) {
	if (!r.empty()) {
		const DX12Assets::BatchRecord	*b	= assets.GetBatch(r.begin());
		if (b->end <= r.end() && b->op)
			return make_range(b, assets.GetBatch(r.end()));
	}
	return empty;
}

string_accum& BatchRange(string_accum &sa, const interval<uint32> &r, DX12Assets &assets) {
	auto	used = UsedBatches(r, assets);
	if (used.empty())
		return sa;
	if (used.size() > 1)
		return sa << " (" << assets.batches.index_of(used.begin()) << " to " << assets.batches.index_of(used.end()) - 1 << ")";
	return sa << " (" << assets.batches.index_of(used.begin()) << ")";
}

struct MarkerStack {
	HTREEITEM						h;
	const DX12Assets::MarkerRecord	*m;
};

bool SubTree(RegisterTree &rt, HTREEITEM h, const_memory_block mem, uint32 start, DX12Assets &assets) {
	intptr_t offset	= start - intptr_t(mem.p);
	uint32	end		= start + mem.size32();
	uint32	boffset	= start;

	const DX12Assets::BatchRecord	*b	= assets.GetBatch(start);
	const DX12Assets::MarkerRecord	*m	= assets.GetMarker(start);
	dynamic_array<MarkerStack>		msp;

	bool	got_batch	= b->end <= end && b->op;

	while (b->end <= end && b->op) {
		while (msp.size() && b->end > msp.back().m->end())
			h	= msp.pop_back_value().h;

		while (m != assets.markers.end() && b->end > m->begin()) {
			MarkerStack	&ms = msp.push_back();
			ms.h	= h;
			ms.m	= m;
//			h		= TreeControl::Item(BatchRange(lvalue(buffer_accum<256>(m->str)), *m, assets)).Param(b->end - 1).Image(ViewDX12GPU::WT_MARKER).Expand().Insert(rt.tree, h);
			h		= TreeControl::Item(BatchRange(lvalue(buffer_accum<256>(m->str)), *m, assets)).Param(m->begin()).Image(ViewDX12GPU::WT_MARKER).Expand().Insert(rt.tree, h);
			++m;
		}

		buffer_accum<256>	ba;
		if (rt.format & IDFMT_OFFSETS)
			ba.format("%08x: ", b->begin);

		ba << "Batch " << assets.batches.index_of(b) << " : ";

		switch (b->op) {
			case RecCommandList::tag_DrawInstanced:
				ba << "draw " << b->Prim() << " x " << b->draw.vertex_count;
				if (b->draw.instance_count != 1)
					ba  << " x " << b->draw.instance_count;
				break;

			case RecCommandList::tag_DrawIndexedInstanced:
				ba << "draw indexed " << b->Prim() << " x " << b->draw.vertex_count;
				if (b->draw.instance_count != 1)
					ba  << " x " << b->draw.instance_count;
				break;

			case RecCommandList::tag_Dispatch:
				ba.format("dispatch %u x %u x %u", b->compute.dim_x, b->compute.dim_y, b->compute.dim_z);
				break;

			case RecCommandList::tag_ExecuteIndirect:
				ba.format("indirect %u", b->indirect.command_count);
				break;

//			case RecCommandList::tag_ClearDepthStencilView:
//			case RecCommandList::tag_ClearRenderTargetView:
//				ba.format("clear");
//				break;
			default: {
				if (auto p = assets.FindOp(mem.slice(boffset - start), b->op))
					FormatCommand(ba, GraphicsCommandList_commands[p->id], (const uint32*)p->data(), rt.format, assets);
				break;
			}
		}

		TreeControl::Item(ba).Image(ViewDX12GPU::WT_BATCH).Param(b->end).Children(1).Insert(rt.tree, h);
		boffset = b->end;
		++b;
	}

	if (boffset != end) {
		//ISO_ASSERT(boffset > end);
		//if (boffset > end)
		PutCommands(rt, got_batch? TreeControl::Item("Orphan").Bold().Insert(rt.tree, h) : h, GraphicsCommandList_commands, mem.slice(int(boffset - end)), offset, assets, true);
	}

	while (msp.size() && boffset > msp.back().m->end())
		h =  msp.pop_back_value().h;

	return got_batch;
}

void ViewDX12GPU::MakeTree() {
	tree.Show(SW_HIDE);
	tree.DeleteItem();

	RegisterTree	rt(tree, this, format);
	HTREEITEM		h		= TVI_ROOT;
	HTREEITEM		h1		= 0;//TreeControl::Item("Device").Bold().Insert(tree, h);

	if (device_object) {
		uint32	batch_addr	= device_object->info.size32();

		for (const COMRecording::header *p = device_object->info, *e = device_object->info.end(); p < e;) {
			const void**		callstack	= 0;
			uint32				addr		= uint32((char*)p - (char*)device_object->info.p);
			buffer_accum<256>	ba;
			HTREEITEM			h2;

			while (p->id == 0xfffe) {
				callstack	= (const void**)p->data();
				p = p->next();
			}

			if (p->id == 0xffff) {
				uint64		obj		= *(uint64*)p->data();
				p = p->next();

				while (p->id == 0xfffe) {
					callstack	= (const void**)p->data();
					p = p->next();
				}

				if (p->id == 0xffff)
					continue;

				if (rt.format & IDFMT_OFFSETS)
					ba.format("%08x: ", addr);

				if (auto *rec = FindObject(obj))
					ba << rec->name;
				else
					ba << "0x" << hex(obj);

				FormatCommand(ba << "->", Device_commands[p->id].name, Device_commands[p->id].fields, (uint32*)p->data(), rt.format, *this);

				addr	= uint32((char*)p - (char*)device_object->info.p);

				if (p->id == RecDevice::tag_CommandQueueExecuteCommandLists) {

					COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
						uint32	total	= 0;
						for (uint32 i = 0; i < NumCommandLists; i++)
							total += pp[i].extent();

						if (rt.format & IDFMT_OFFSETS)
							ba.format(" %08x: ", batch_addr);

						BatchRange(ba, make_interval_len(batch_addr, total), *this);
						h2		= TreeControl::Item(ba).Bold().Param(addr).Insert(tree, h);

						bool	has_batches = HasBatches(make_interval_len(batch_addr, total), *this);

						for (int i = 0; i < NumCommandLists; i++) {
							const CommandRange	&r	= pp[i];
							if (auto obj = FindObject(uint64(&*r))) {
								const RecCommandList	*rec = obj->info;
								auto	r2	= make_interval_len(batch_addr, pp[i].extent());

								buffer_accum<256>	ba;
								HTREEITEM	h3	= TreeControl::Item(BatchRange(ba << obj->name, r2, *this)).Bold().Expand(has_batches).Param(addr + i).Insert(tree, h2);
								if (r.extent())
									SubTree(rt, h3, rec->buffer.slice(r.begin(), r.extent()), r2.a, *this);
							}
							batch_addr += r.extent();
						}
					});
				} else {

					h2		= TreeControl::Item(ba).Param(addr).Insert(tree, h);

					if (p->id >= num_elements(Device_commands))
						break;

					rt.AddFields(h2, Device_commands[p->id].fields, (uint32*)p->data());
				}

				p	= p->next();
				h1	= 0;

			} else {
				if (!h1) {
					if (rt.format & IDFMT_OFFSETS)
						ba.format("%08x: ", addr);
					ba << device_object->name;
					h1	= TreeControl::Item(ba).Bold().Insert(tree, h);
				}

				auto	*p0		= p;
				if (p0->id >= num_elements(Device_commands))
					break;

				int		count	= CountRepeats(p, e);
				if (count > 2) {
					PutRepeat(rt, h1, Device_commands, p0->id, addr, count);
					callstack = 0;

				} else {
					h2	= PutCommand(rt, h1, Device_commands, p0, addr, *this);
					p	= p0->next();
				}
			}

			if (callstack)
				PutCallstack(rt, h2, callstack);
		}
	}

	tree.Show();
	tree.Invalidate();
}

void ViewDX12GPU::ExpandTree(HTREEITEM h) {
	RegisterTree		rt(tree, this, format);
	buffer_accum<1024>	ba;

	TreeControl::Item	item = tree.GetItem(h, TVIF_IMAGE|TVIF_PARAM);

	switch (item.Image()) {
		case WT_BATCH: {
			uint32		addr	= item.Param();
			auto		b		= GetBatch(addr);
			uint32		start	= b->begin;
			if (auto m = GetCommands(start))
				PutCommands(rt, h, GraphicsCommandList_commands, m.slice_to(addr - start), start - uintptr_t(m.begin()), *this, true);
			break;
		}

		case WT_CALLSTACK: {
			const uint64*	callstack = item.Param();
			for (int i = 0; i < 32 && callstack[i]; i++)
				rt.Add(h, ba.reset() << stack_dumper.GetFrame(callstack[i]), WT_CALLSTACK, callstack[i]);
			break;
		}

		case WT_REPEAT: {
			uint32		addr = item.Param();
			if (auto m = GetCommands(addr)) {
				const COMRecording::header *p	= m;
				int		count = CountRepeats(p, m.end());
				PutCommands(rt, h, addr < device_object->info.size32() ? Device_commands : GraphicsCommandList_commands, m.slice_to(unconst(p)), addr - uintptr_t(m.begin()), *this, false);
			}
			break;
		}
	}
}

ViewDX12GPU::ViewDX12GPU(MainWindow &main, const WindowPos &pos) : SplitterWindow(SWF_VERT|SWF_DOCK) {
	format			= init_format;
	context_item	= 0;

	SplitterWindow::Create(pos, "DX12 Dump", CHILD | CLIPCHILDREN);
	toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX12GPU);
	Rebind(this);
}
ViewDX12GPU::ViewDX12GPU(MainWindow &main, const WindowPos &pos, DXConnection *con) : SplitterWindow(SWF_VERT|SWF_DOCK), DX12Connection(con) {
	format			= init_format;
	context_item	= 0;

	SplitterWindow::Create(pos, "DX12 Dump", CHILD | CLIPCHILDREN);
	toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX12GPU);
	Rebind(this);
}

//-----------------------------------------------------------------------------
//	EditorDX12
//-----------------------------------------------------------------------------

class EditorDX12 : public app::EditorGPU, public app::MenuItemCallbackT<EditorDX12>, public Handles2<EditorDX12, AppEvent> {
	filename				fn;
	Menu					recent;
	ref_ptr<DXConnection>	con;
	dynamic_array<DWORD>	pids;

	void	Grab(ViewDX12GPU *view, Progress &progress);

	void	TogglePause() {
		con->Call<void>(con->paused ? INTF_Continue : INTF_Pause);
		con->paused = !con->paused;
	}

	virtual bool Matches(const ISO::Browser &b) {
		return b.Is("DX12GPUState") || b.Is<DX12Capture>(ISO::MATCH_NOUSERRECURSE);
	}

	virtual Control Create(app::MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		ViewDX12GPU	*view = new ViewDX12GPU(main, wpos);
		view->Load(p);

		RunThread([view,
			progress = Progress(WindowPos(*view, AdjustRect(view->GetRect().Centre(500, 30), Control::OVERLAPPED | Control::CAPTION, false)), "Collecting assets", view->objects.size32())
		]() mutable {
			view->ScanCommands(&progress, view->CommandTotal(), nullptr);
			//view->GetStatistics();
			JobQueue::Main().add([view] { view->MakeTree(); });
		});
		return *view;
	}

#ifndef ISO_EDITOR
	virtual bool	Command(MainWindow &main, ID id) {
		if ((main.GetDropDown(ID_ORBISCRUDE_GRAB).GetItemStateByID(ID_ORBISCRUDE_GRAB_DX12) & MF_CHECKED) && EditorGPU::Command(main, id))
			return true;

		switch (id) {
			case ID_ORBISCRUDE_PAUSE: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				if (menu.GetItemStateByID(ID_ORBISCRUDE_GRAB_DX12) & MF_CHECKED) {
					TogglePause();
					return true;
				}
				break;
			}

			case ID_ORBISCRUDE_GRAB: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				if (menu.GetItemStateByID(ID_ORBISCRUDE_GRAB_DX12) & MF_CHECKED) {
					
					if (!con->process.Active())
						con->OpenProcess(fn, string(main.DescendantByID(ID_EDIT + 2).GetText()), string(main.DescendantByID(ID_EDIT + 1).GetText()), "dx12crude.dll");

					main.SetTitle("new");
					//Grab(new ViewDX12GPU(main, main.Dock(DOCK_TAB)));

					auto	view		= new ViewDX12GPU(main, main.Dock(DOCK_TAB), con);
					RunThread([this, view,
						progress	= Progress(WindowPos(*view, AdjustRect(view->GetRect().Centre(500, 30), Control::OVERLAPPED | Control::CAPTION, false)), "Capturing", 0)
					]() mutable {
						Grab(view, progress);
					});
					return true;
				}
				break;
			}
		}
		return false;
	}
#endif

public:
	void operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN) {
			auto	main	= MainWindow::Get();
			Menu	menu	= main->GetDropDown(ID_ORBISCRUDE_GRAB);
			recent			= Menu::Create();
			recent.SetStyle(MNS_NOTIFYBYPOS);

		#if 0
			int		id		= 1;
			for (auto i : GetSettings("DX12/Recent"))
				Menu::Item(i.GetString(), id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);

			Menu::Item("New executable...", 0).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
			recent.Separator();

			pids.clear();
			ModuleSnapshot	snapshot(0, TH32CS_SNAPPROCESS);
			id		= 0x1000;
			for (auto i : snapshot.processes()) {
				filename	path;
				if (FindModule(i.th32ProcessID, "dx12crude.dll", path)) {
					pids.push_back(i.th32ProcessID);
					Menu::Item(i.szExeFile, id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
				}
			}
			Menu::Item("Set Executable for DX12", ID_ORBISCRUDE_GRAB_DX12).SubMenu(recent).InsertByPos(menu, 0);
		#else
			auto	cb = new_lambda<MenuCallback>([this](Control c, Menu m) {
				ISO_OUTPUTF("m = ") << m << '\n';
				while (m.RemoveByPos(0));

				int		id		= 1;
				for (auto i : GetSettings("DX12/Recent"))
					Menu::Item(i.GetString(), id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(m);
				Menu::Item("New executable...", 0).Param(static_cast<MenuItemCallback*>(this)).AppendTo(m);
				m.Separator();

				pids.clear();
				ModuleSnapshot	snapshot(0, TH32CS_SNAPPROCESS);
				id		= 0x1000;
				for (auto i : snapshot.processes()) {
					filename	path;
					if (FindModule(i.th32ProcessID, "dx12crude.dll", path)) {
						pids.push_back(i.th32ProcessID);
						Menu::Item(i.szExeFile, id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(m);
					}
				}
			});
			Menu::Item("Set Executable for DX12", ID_ORBISCRUDE_GRAB_DX12).SubMenu(recent).Param(cb).InsertByPos(menu, 0);
		#endif
		}
	}

	void	operator()(Control c, Menu::Item i) {
		ISO::Browser2	settings	= GetSettings("DX12/Recent");
		int				id			= i.ID();

		if (id && (GetKeyState(VK_RBUTTON) & 0x8000)) {
			Menu	popup = Menu::Popup();
			popup.Append("Remove", 2);
			int	r = popup.Track(c, GetMousePos(), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD);
			if (r == 2) {
				settings.Remove(id - 1);
				recent.RemoveByID(id);
			}
			return;
		}

		if (id == 0) {
			if (!GetOpen(c, fn, "Open File", "Windows Executable\0*.exe\0"))
				return;
			settings.Append().Set((const char*)fn);
			id	= settings.Count();
			Menu::Item(fn, id).Param(MenuItemCallback(this)).InsertByPos(recent, -2);
		}

		auto	*main	= MainWindow::Cast(c);
		Menu	m		= main->GetDropDown(ID_ORBISCRUDE_GRAB);
		m.RadioDirect(ID_ORBISCRUDE_GRAB_DX12, ID_ORBISCRUDE_GRAB_TARGETS, ID_ORBISCRUDE_GRAB_TARGETS_MAX);
		Menu::Item().Check(true).Type(MFT_RADIOCHECK).SetByPos(m, m.FindPosition(recent));

		recent.RadioDirect(id, 0, recent.Count());
		//CreateReadPipe(pipe);

		con	= new DXConnection;
		const char *dll_name	= "dx12crude.dll";

		if (id & 0x1000) {
			if (con->OpenProcess(pids[id & 0xfff], dll_name)) {
				fn = con->process.Filename();
				con->ConnectDebugOutput();
			}

		} else {
			fn = settings[id - 1].GetString();

			string	exec_dir, exec_cmd;
			if (DialogBase home = main->DescendantByID("Home")) {
				exec_cmd = home.ItemText(ID_EDIT + 1);
				exec_dir = home.ItemText(ID_EDIT + 2);
			}

			if (con->OpenProcess(fn, exec_dir, exec_cmd, dll_name)) {
				con->ConnectDebugOutput();

				if (resume_after) {
					ISO_OUTPUT("Resuming process\n");
					con->Call<void>(INTF_Continue);
				}
			}
		}
	}

	EditorDX12() {
		ISO::getdef<DX12Capture>();
	}

} editor_dx12;


struct ip_gpu_memory : ip_memory_interface {
	DX12Assets	*assets;
	ip_gpu_memory(IP4::socket_addr addr, DX12Assets *assets) : ip_memory_interface(addr), assets(assets) {}

	bool	_get(void *buffer, uint64 size, uint64 address) {
		if (address & GPU) {
			address &= ~GPU;
			if (auto obj = assets->FindByGPUAddress(address)) {
				const RecResource	*rec = obj->info;
				ISO_OUTPUTF("VRAM: 0x") << hex(rec->gpu) << " to 0x" << hex(rec->gpu + rec->data_size) << " (" << obj->GetName() << ": 0x" << hex(rec->data_size) << ")\n";
				if (auto mem = SocketCallRPC<malloc_block_all>(addr.connect_or_close(IP4::TCP()), INTF_ResourceData, uintptr_t(obj->obj))) {
					auto	offset	= address - rec->gpu;
					memcpy(buffer, mem + offset, min(mem.size() - offset, size));
					return true;
				}
			}
		}
		return ip_memory_interface::_get(buffer, size, address);
	}

};

void EditorDX12::Grab(ViewDX12GPU *view, Progress &progress) {
	auto	num_objects	= con->Call<uint32>(INTF_CaptureFrames, until_halt ? 0x7ffffff : num_frames);
	auto	objects		= con->Call<with_size<dynamic_array<RecObject2>>>(INTF_GetObjects);

	if (objects.empty()) {
		view->SendMessage(WM_ISO_ERROR, 0, (LPARAM)"\nFailed to capture");
		return;
	}

	view->objects.reserve(objects.size());

//	auto mem = con->MemoryInterface();
	ip_gpu_memory	mem(con->addr, view);

	progress.Reset("Collecting objects", num_objects);

//	static const uint64		gpu				= 0x8000000000000000ull;
	static const uint64		reserved_gpu	= 0x1000000000000000ull;
	static const uint64		fake_gpu		= 0x2000000000000000ull;

	uint64		total_reserved_gpu	= reserved_gpu;
	uint64		total_fake_gpu		= fake_gpu;
	uint64		total_vram			= 0;

	int						counts[RecObject::NUM_TYPES] = {};
	hash_multiset<string>	name_counts;

	for (auto &i : objects) {
		if (i.name) {
			if (int n = name_counts.insert(i.name))
				i.name << '(' << n << ')';
		} else {
			auto	type = undead(i.type);
			i.name << get_field_name(type) << '#' << counts[type]++;
		}

		switch (i.type) {
			case RecObject::Resource: {
				RecResource	*res = i.info;
 				switch (res->alloc) {
					case RecResource::UnknownAlloc:
					case RecResource::Placed:
						break;

					case RecResource::Reserved:
						res->gpu			= total_reserved_gpu;
						total_reserved_gpu	+= res->data_size;
						total_vram			+= res->data_size;
						break;

					default:
						if (!res->gpu) {
							res->gpu		= total_fake_gpu;
							total_fake_gpu	+= res->data_size;
						}
						total_vram			+= res->data_size;
						break;
				}
				break;
			}
			case RecObject::Heap: {
			#if 1
				RecHeap		*heap	= i.info;
//				heap->gpu			= total_fake_gpu;
//				total_fake_gpu		+= heap->SizeInBytes;
				total_vram			+= heap->SizeInBytes;
			#endif
				break;
			}
		}

		DX12Assets::ObjectRecord *obj = view->AddObject(i.type, move(i.name), move(i.info), i.obj, &mem);
		progress.Set(objects.index_of(i));
	}
	
	view->FinishLoading();

	uint64		total = view->CommandTotal();
	progress.Reset("Parsing commands", total);
	view->ScanCommands(&progress, total, &mem);

	progress.Reset("Grabbing VRAM", total_vram);
	total_vram = 0;

	if (total_reserved_gpu > reserved_gpu)
		view->cache.add_block(reserved_gpu, total_reserved_gpu - reserved_gpu);

//	if (total_fake_gpu > fake_gpu)
//		view->cache.add_block(fake_gpu, total_fake_gpu - fake_gpu);

	total_reserved_gpu = reserved_gpu;
	for (auto &i : view->objects) {
		switch (i.type) {
			case RecObject::Resource: {
				if (!i.used)
					continue;

				const RecResource	*rec = i.info;
				switch (rec->alloc) {
					case RecResource::UnknownAlloc:
						break;

					case RecResource::Placed: {
						auto	gpu	= rec->gpu | GPU;
						auto	p	= view->cache(gpu);
						if (!p || !p.contains(gpu + rec->data_size)) {
							ISO_OUTPUTF("VRAM: 0x") << hex(rec->gpu) << " to 0x" << hex(rec->gpu + rec->data_size) << " (" << i.GetName() << ": 0x" << hex(rec->data_size) << ")\n";
							auto	mem = con->Call<malloc_block_all>(INTF_ResourceData, uintptr_t(i.obj));
							view->cache.add_block(gpu, mem);
							total_vram += mem.size();
							progress.Set(total_vram);
						} else {
							ISO_OUTPUTF("Skipping VRAM for ") << i.GetName() << "\n";
						}
						break;
					}
					default: {
						ISO_OUTPUTF("VRAM: 0x") << hex(rec->gpu) << " to 0x" << hex(rec->gpu + rec->data_size) << " (" << i.GetName() << ": 0x" << hex(rec->data_size) << ")\n";
						auto	mem = con->Call<malloc_block_all>(INTF_ResourceData, uintptr_t(i.obj));
						view->cache.add_block(rec->gpu | GPU, mem);
						total_vram += mem.size();
						progress.Set(total_vram);
						break;
					}
				}
				break;
			}
#if 0
			case RecObject::Heap: {
				const RecHeap		*heap = i.info;
				ISO_OUTPUTF("VRAM: 0x") << hex(heap->gpu) << " to 0x" << hex(heap->gpu + heap->SizeInBytes) << " (0x" << hex(heap->SizeInBytes) << ")\n";
				auto	mem	= Call<malloc_block_all>(INTF_HeapData, uintptr_t(i.obj));
				view->cache.add_block(heap->gpu | GPU, mem);
				total_vram += mem.size();
				progress.Set(total_vram);
				break;
			}
#endif
		}
	}

	if (resume_after) {
		ISO_OUTPUT("Resuming process\n");
		con->Call<void>(INTF_Continue);
	} else {
		con->paused = true;
	}

	JobQueue::Main().add([view] { view->MakeTree(); });
}

void dx12_dummy() {}
