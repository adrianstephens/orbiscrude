#include "hook_com.h"
#include "base/list.h"
#include "thread.h"
#include "dx12_record.h"
#include "dxgi_record.h"
#include "shared\dxgi_helpers.h"

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d12")

using namespace dx12;

struct linked_recording : e_link<linked_recording>, COMRecording {
	typedef Recording::TYPE	TYPE;
	TYPE	type;
	linked_recording(TYPE _type) : type(_type) {}
};

struct RecObjectLink : e_link<RecObjectLink>, RecObject {
	Wrap<ID3D12Device>	*device;
	void	init(Wrap<ID3D12Device> *_device);
	virtual	memory_block	get_info()	{ return memory_block(); }
};

//-----------------------------------------------------------------------------
//	Capture
//-----------------------------------------------------------------------------

struct Capture {
	int								frames;
	Semaphore						semaphore;
	Semaphore						semaphore2;
	Wrap<ID3D12Device>				*device;
	malloc_block					names;

	Capture() : frames(0), semaphore(0), semaphore2(0) {}

	void	Set(int _frames) {
		frames = _frames + 1;
	}
	bool	Update() {
		bool	ret = frames > 1;
		if (frames && !--frames) {
			semaphore.UnLock();
			semaphore2.Lock();
		}
		return ret;
	}
	void	Wait() {
		semaphore.Lock();
	}
	void	Continue() {
		semaphore2.UnLock();
	}
};

static Capture capture;

//-----------------------------------------------------------------------------
//	Wraps
//-----------------------------------------------------------------------------

template<class T> class Wrap2<T, ID3D12Object> : public com_wrap<T> {
public:
	//IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D12Object>(this, riid, pp) ? S_OK : com_wrap<T>::QueryInterface(riid, pp);
	}
	//ID3D12Object
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *size, void *data)			{ return orig->GetPrivateData(guid, size, data); }
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT size, const void *data)		{ return orig->SetPrivateData(guid, size, data); }
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *data)	{ return orig->SetPrivateDataInterface(guid, data); }
	HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)											{ return orig->SetName(Name); }
};

template<class T> class Wrap2<T, ID3D12DeviceChild> : public Wrap2<T, ID3D12Object>, public RecObjectLink {
public:
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D12DeviceChild>(this, riid, pp) ? S_OK : Wrap2<T, ID3D12Object>::QueryInterface(riid, pp);
	}
	HRESULT	STDMETHODCALLTYPE GetDevice(REFIID riid, void **pp)		{ *pp = device; return S_OK; }
	//ID3D12Object
	HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)					{ name = Name; return orig->SetName(Name); }
};

template<class T> class Wrap2<T, ID3D12Pageable> : public Wrap2<T, ID3D12DeviceChild> {
public:
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D12Pageable>(this, riid, pp) ? S_OK : Wrap2<T, ID3D12DeviceChild>::QueryInterface(riid, pp);
	}
};

template<class T> class Wrap2<T, ID3D12CommandList> : public Wrap2<T, ID3D12DeviceChild> {
public:
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D12CommandList>(this, riid, pp) ? S_OK : Wrap2<T, ID3D12DeviceChild>::QueryInterface(riid, pp);
	}
	D3D12_COMMAND_LIST_TYPE	STDMETHODCALLTYPE GetType()		{ return orig->GetType(); }
};

template<> class Wrap<ID3D12DeviceChild>	: public Wrap2<ID3D12DeviceChild, ID3D12DeviceChild> {};
template<> class Wrap<ID3D12Pageable>		: public Wrap2<ID3D12Pageable, ID3D12Pageable> {};

template<> class Wrap<ID3D12RootSignature>	: public Wrap2<ID3D12RootSignature, ID3D12DeviceChild> {
public:
	malloc_block	blob;
	Wrap() { type = RootSignature; }
	void	init(Wrap<ID3D12Device> *_device, const memory_block &_blob) {
		Wrap2<ID3D12RootSignature, ID3D12DeviceChild>::init(_device);
		blob = _blob;
	}
	virtual	memory_block	get_info()	{ return blob; }
};

template<> class Wrap<ID3D12Heap> : public Wrap2<ID3D12Heap, ID3D12Pageable> {
public:
	Wrap() { type = Heap; }
	D3D12_HEAP_DESC STDMETHODCALLTYPE GetDesc()			{ return orig->GetDesc(); }
};

template<> class Wrap<ID3D12Resource> : public Wrap2<ID3D12Resource, ID3D12Pageable> {
public:
	malloc_block	rr;
	void			*mapped;
	size_t			data_size;
public:
	Wrap()	: mapped(0) { type = Resource; }
	void		init(Wrap<ID3D12Device> *_device, const D3D12_RESOURCE_DESC *desc);
	RecResource	*init_data();
	void		flush();
	virtual	memory_block	get_info()	{
		flush();
		return  rr;
	}

	HRESULT	STDMETHODCALLTYPE Map(UINT sub, const D3D12_RANGE *read, void **data);
	void	STDMETHODCALLTYPE Unmap(UINT sub, const D3D12_RANGE *written);
	D3D12_RESOURCE_DESC			STDMETHODCALLTYPE GetDesc()					{ return orig->GetDesc(); }
	D3D12_GPU_VIRTUAL_ADDRESS	STDMETHODCALLTYPE GetGPUVirtualAddress()	{ return orig->GetGPUVirtualAddress(); }
	HRESULT	STDMETHODCALLTYPE WriteToSubresource(UINT DstSubresource, const D3D12_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
	HRESULT	STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, UINT SrcSubresource, const D3D12_BOX *pSrcBox);
	HRESULT	STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS *pHeapFlags)	{ return orig->GetHeapProperties(pHeapProperties, pHeapFlags); }
};

template<> class Wrap<ID3D12CommandAllocator> : public Wrap2<ID3D12CommandAllocator, ID3D12Pageable> {
public:
	enum tag {
		tag_Reset,
	};
	static const char *tag_names[];
	Wrap()	{ type = CommandAllocator; }
	HRESULT	STDMETHODCALLTYPE Reset();
};

template<> class Wrap<ID3D12Fence> : public Wrap2<ID3D12Fence, ID3D12Pageable> {
public:
	enum tag {
		tag_SetEventOnCompletion,
		tag_Signal,
	};
	static const char *tag_names[];
	Wrap()	{ type = Fence; }
	UINT64	STDMETHODCALLTYPE GetCompletedValue()	{ return orig->GetCompletedValue();  }
	HRESULT	STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value, HANDLE hEvent);
	HRESULT	STDMETHODCALLTYPE Signal(UINT64 Value);
};

template<> class Wrap<ID3D12PipelineState> : public Wrap2<ID3D12PipelineState, ID3D12Pageable> {
public:
	enum STATE_TYPE { GFX, COMP } state_type;
	malloc_block	blob;

	Wrap()	{ type = PipelineState; }
	void	init(Wrap<ID3D12Device> *_device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc) {
		RecObjectLink::init(_device);
		state_type	= GFX;
		blob		= flatten_struct(*desc);
	}
	void	init(Wrap<ID3D12Device> *_device, const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc) {
		RecObjectLink::init(_device);
		state_type	= COMP;
		blob		= flatten_struct(*desc);
	}
	virtual	memory_block	get_info()	{ return blob; }
	HRESULT	STDMETHODCALLTYPE GetCachedBlob(ID3DBlob **pp) { return orig->GetCachedBlob(pp); }
};

template<> class Wrap<ID3D12DescriptorHeap> : public Wrap2<ID3D12DescriptorHeap, ID3D12Pageable>, public e_link<Wrap<ID3D12DescriptorHeap> > {
public:
	malloc_block	blob;
	Wrap()	{ type = DescriptorHeap; }
	void					init(Wrap<ID3D12Device> *_device, const D3D12_DESCRIPTOR_HEAP_DESC *desc);
	virtual	memory_block	get_info()								{ return blob; }
	DESCRIPTOR*				holds(D3D12_CPU_DESCRIPTOR_HANDLE h)	{ return ((RecDescriptorHeap*)blob)->holds(h); }

	D3D12_DESCRIPTOR_HEAP_DESC	STDMETHODCALLTYPE GetDesc()								{ return orig->GetDesc(); }
	D3D12_CPU_DESCRIPTOR_HANDLE	STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart()	{ return orig->GetCPUDescriptorHandleForHeapStart(); }
	D3D12_GPU_DESCRIPTOR_HANDLE	STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart()	{ return orig->GetGPUDescriptorHandleForHeapStart(); }
};

template<> class Wrap<ID3D12QueryHeap>			: public Wrap2<ID3D12QueryHeap, ID3D12Pageable> {
public:
	Wrap()	{ type = QueryHeap; }
};
template<> class Wrap<ID3D12CommandSignature>	: public Wrap2<ID3D12CommandSignature, ID3D12Pageable> {
public:
	Wrap()	{ type = CommandSignature; }
};

template<> class Wrap<ID3D12CommandList>		: public Wrap2<ID3D12CommandList, ID3D12CommandList> {};

template<> class Wrap<ID3D12GraphicsCommandList> : public Wrap2<ID3D12GraphicsCommandList, ID3D12CommandList>, RecCommandList {
public:
	linked_recording	recording;

	Wrap() : recording(Recording::CMDLIST) { type = GraphicsCommandList; }
	void	set_recording(bool enable);
	void	init(Wrap<ID3D12Device> *_device);

	HRESULT	STDMETHODCALLTYPE Close();
	HRESULT	STDMETHODCALLTYPE Reset(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState);
	void	STDMETHODCALLTYPE ClearState(ID3D12PipelineState *pPipelineState);
	void	STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void	STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation);
	void	STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
	void	STDMETHODCALLTYPE CopyBufferRegion(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes);
	void	STDMETHODCALLTYPE CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox);
	void	STDMETHODCALLTYPE CopyResource(ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource);
	void	STDMETHODCALLTYPE CopyTiles(ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags);
	void	STDMETHODCALLTYPE ResolveSubresource(ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);
	void	STDMETHODCALLTYPE IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);
	void	STDMETHODCALLTYPE RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports);
	void	STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects);
	void	STDMETHODCALLTYPE OMSetBlendFactor(const FLOAT BlendFactor[4]);
	void	STDMETHODCALLTYPE OMSetStencilRef(UINT StencilRef);
	void	STDMETHODCALLTYPE SetPipelineState(ID3D12PipelineState *pPipelineState);
	void	STDMETHODCALLTYPE ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers);
	void	STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList *pCommandList);
	void	STDMETHODCALLTYPE SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *pp);
	void	STDMETHODCALLTYPE SetComputeRootSignature(ID3D12RootSignature *pRootSignature);
	void	STDMETHODCALLTYPE SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature);
	void	STDMETHODCALLTYPE SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);
	void	STDMETHODCALLTYPE SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);
	void	STDMETHODCALLTYPE SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	void	STDMETHODCALLTYPE SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	void	STDMETHODCALLTYPE SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues);
	void	STDMETHODCALLTYPE SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues);
	void	STDMETHODCALLTYPE SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void	STDMETHODCALLTYPE SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void	STDMETHODCALLTYPE SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void	STDMETHODCALLTYPE SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void	STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void	STDMETHODCALLTYPE SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void	STDMETHODCALLTYPE IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView);
	void	STDMETHODCALLTYPE IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews);
	void	STDMETHODCALLTYPE SOSetTargets(UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews);
	void	STDMETHODCALLTYPE OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor);
	void	STDMETHODCALLTYPE ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects);
	void	STDMETHODCALLTYPE ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects);
	void	STDMETHODCALLTYPE ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects);
	void	STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects);
	void	STDMETHODCALLTYPE DiscardResource(ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion);
	void	STDMETHODCALLTYPE BeginQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
	void	STDMETHODCALLTYPE EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
	void	STDMETHODCALLTYPE ResolveQueryData(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset);
	void	STDMETHODCALLTYPE SetPredication(ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation);
	void	STDMETHODCALLTYPE SetMarker(UINT Metadata, const void *pData, UINT Size);
	void	STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void *pData, UINT Size);
	void	STDMETHODCALLTYPE EndEvent();
	void	STDMETHODCALLTYPE ExecuteIndirect(ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset);
};

template<> class Wrap<ID3D12CommandQueue> : public Wrap2<ID3D12CommandQueue, ID3D12Pageable>, D3D12_COMMAND_QUEUE_DESC {
public:
	struct Presenter : PresentDevice {
		void	Present();
	} present;
	
	Wrap()	{ type = CommandQueue; }
	void	init(Wrap<ID3D12Device> *_device, const D3D12_COMMAND_QUEUE_DESC *desc) {
		RecObjectLink::init(_device);
		*(D3D12_COMMAND_QUEUE_DESC*)this = *desc;
	}
	virtual	memory_block	get_info()	{ return memory_block(*static_cast<D3D12_COMMAND_QUEUE_DESC*>(this)); }

	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		if (riid == __uuidof(PresentDevice)) {
			*pp = &present;
			return S_OK;
		}
		return Wrap2<ID3D12CommandQueue, ID3D12Pageable>::QueryInterface(riid, pp);
	}

	void	STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags);
	void	STDMETHODCALLTYPE CopyTileMappings(ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags);
	void	STDMETHODCALLTYPE ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *pp);
	void	STDMETHODCALLTYPE SetMarker(UINT Metadata, const void *pData, UINT Size);
	void	STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void *pData, UINT Size);
	void	STDMETHODCALLTYPE EndEvent();
	HRESULT	STDMETHODCALLTYPE Signal(ID3D12Fence *pFence, UINT64 Value);
	HRESULT	STDMETHODCALLTYPE Wait(ID3D12Fence *pFence, UINT64 Value);
	HRESULT	STDMETHODCALLTYPE GetTimestampFrequency(UINT64 *pFrequency)								{ return orig->GetTimestampFrequency(pFrequency); }
	HRESULT	STDMETHODCALLTYPE GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp)		{ return orig->GetClockCalibration(pGpuTimestamp, pCpuTimestamp); }
	D3D12_COMMAND_QUEUE_DESC	STDMETHODCALLTYPE GetDesc()											{ return orig->GetDesc(); }
};

template<> class Wrap<ID3D12Device> : public Wrap2<ID3D12Device, ID3D12Object>, RecDevice {
public:
	linked_recording					recording;
	e_list<linked_recording>			recordings;
	e_list<Wrap<ID3D12DescriptorHeap> > descriptor_heaps;
	e_list<RecObjectLink>				objects;
#if 0
	Wrap(ID3D12Device *device) : recording(Recording::DEVICE) {
		set(device);
		set_recording(capture.Update());
	}
#else
	Wrap() : recording(Recording::DEVICE) {
		set_recording(capture.Update());
	}
#endif

	DESCRIPTOR*		find_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE h);
	void			set_recording(bool enable);

	UINT	STDMETHODCALLTYPE GetNodeCount() {
		return orig->GetNodeCount();
	}
	HRESULT	STDMETHODCALLTYPE CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *desc, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize);
	HRESULT	STDMETHODCALLTYPE CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID riid, void **pp);
	UINT	STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type)	{
		return orig->GetDescriptorHandleIncrementSize(type);
	}
	HRESULT	STDMETHODCALLTYPE CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void **pp);
	void	STDMETHODCALLTYPE CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest);
	void	STDMETHODCALLTYPE CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest);
	void	STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest);
	void	STDMETHODCALLTYPE CreateRenderTargetView(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest);
	void	STDMETHODCALLTYPE CreateDepthStencilView(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest);
	void	STDMETHODCALLTYPE CreateSampler(const D3D12_SAMPLER_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest);
	void	STDMETHODCALLTYPE CopyDescriptors(UINT dest_num, const D3D12_CPU_DESCRIPTOR_HANDLE *dest_starts, const UINT *dest_sizes, UINT srce_num, const D3D12_CPU_DESCRIPTOR_HANDLE *srce_starts, const UINT *srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type);
	void	STDMETHODCALLTYPE CopyDescriptorsSimple(UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type);
	D3D12_RESOURCE_ALLOCATION_INFO	STDMETHODCALLTYPE GetResourceAllocationInfo(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC *descs) {
		return orig->GetResourceAllocationInfo(visibleMask, numResourceDescs, descs);
	}
	D3D12_HEAP_PROPERTIES	STDMETHODCALLTYPE GetCustomHeapProperties(UINT nodeMask, D3D12_HEAP_TYPE heapType) {
		return orig->GetCustomHeapProperties(nodeMask, heapType);
	}
	HRESULT	STDMETHODCALLTYPE CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateHeap(const D3D12_HEAP_DESC *desc, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateReservedResource(const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateSharedHandle(ID3D12DeviceChild *pObject, const SECURITY_ATTRIBUTES *pAttributes, DWORD Access, LPCWSTR Name, HANDLE *pHandle);
	HRESULT	STDMETHODCALLTYPE OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle);
	HRESULT	STDMETHODCALLTYPE MakeResident(UINT NumObjects, ID3D12Pageable *const *pp);
	HRESULT	STDMETHODCALLTYPE Evict(UINT NumObjects, ID3D12Pageable *const *pp);
	HRESULT	STDMETHODCALLTYPE CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE GetDeviceRemovedReason()	{ return orig->GetDeviceRemovedReason(); }
	void	STDMETHODCALLTYPE GetCopyableFootprints(const D3D12_RESOURCE_DESC *desc, UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts, UINT *pNumRows, UINT64 *pRowSizeInBytes, UINT64 *pTotalBytes) {
		return orig->GetCopyableFootprints(desc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes);
	}
	HRESULT	STDMETHODCALLTYPE CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *desc, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE SetStablePowerState(BOOL Enable);
	HRESULT	STDMETHODCALLTYPE CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *desc, ID3D12RootSignature *pRootSignature, REFIID riid, void **pp);
	void	STDMETHODCALLTYPE GetResourceTiling(ID3D12Resource *pTiledResource, UINT *pNumTilesForEntireResource, D3D12_PACKED_MIP_INFO *pPackedMipDesc, D3D12_TILE_SHAPE *pStandardTileShapeForNonPackedMips,  UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,  D3D12_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)	{
		return orig->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips,  pNumSubresourceTilings, FirstSubresourceTilingToGet,  pSubresourceTilingsForNonPackedMips);
	}
	LUID	STDMETHODCALLTYPE GetAdapterLuid()	{ return orig->GetAdapterLuid(); }
};

//-----------------------------------------------------------------------------
//	RecObjectLink
//-----------------------------------------------------------------------------

void	RecObjectLink::init(Wrap<ID3D12Device> *_device) {
	device	= _device;
	device->objects.push_back(this);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12Resource>
//-----------------------------------------------------------------------------

void	Wrap<ID3D12Resource>::init(Wrap<ID3D12Device> *_device, const D3D12_RESOURCE_DESC *desc) {
	data_size = RESOURCE_DESC(desc).CalcTotalSize(_device->orig);
	RecResource	*r	= new(rr.create(sizeof(RecResource) + data_size)) RecResource(desc, orig->GetGPUVirtualAddress());
//	RecResource	*r	= new(rr.create(sizeof(RecResource))) RecResource(desc, orig->GetGPUVirtualAddress());
//	data_size		= r->CalcTotalSize(_device->orig);
	RecObjectLink::init(_device);
}

RecResource	*Wrap<ID3D12Resource>::init_data() {
//	if (rr.length() < sizeof(RecResource) + data_size)
//		rr.resize(sizeof(RecResource) + data_size);
	return rr;
}

void	Wrap<ID3D12Resource>::flush() {
	RecResource	*r	= init_data();
	if (mapped)
		memcpy(r->data(), mapped, data_size);
}

HRESULT	Wrap<ID3D12Resource>::Map(UINT sub, const D3D12_RANGE *read, void **data) {
	HRESULT	hr = orig->Map(sub, read, data);
	mapped = hr == S_OK ? *data : 0;
	return hr;
}
void	Wrap<ID3D12Resource>::Unmap(UINT sub, const D3D12_RANGE *written) {
	orig->Unmap(sub, written);
	if (!mapped || (written && written->End <= written->Begin))
		return;
	RecResource	*r	= init_data();
	D3D12BLOCK	b	= r->GetSubresource(device, sub);
	size_t	begin	= written ? written->Begin : 0;
	size_t	size	= written ? written->End - written->Begin : b.Size();
	r->CopyBufferRegion(b.offset + begin, (uint8*)mapped + begin, size, data_size);
}
HRESULT	Wrap<ID3D12Resource>::WriteToSubresource(UINT DstSubresource, const D3D12_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
	HRESULT	hr = orig->WriteToSubresource(DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
	if (hr == S_OK) {
		RecResource	*r	= init_data();
		D3D12BLOCK	b	= r->GetSubresource(device, DstSubresource);
		r->CopyTextureRegion(b, pDstBox ? *pDstBox : b.Box(), pSrcData, SrcRowPitch, SrcDepthPitch,data_size);
	}
	return hr;
}
HRESULT	Wrap<ID3D12Resource>::ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, UINT SrcSubresource, const D3D12_BOX *pSrcBox) {
	return orig->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, SrcSubresource, pSrcBox);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12CommandAllocator>
//-----------------------------------------------------------------------------
HRESULT	Wrap<ID3D12CommandAllocator>::Reset() {
	return device->recording.WithObject(this).RunRecord(this, &ID3D12CommandAllocator::Reset, RecDevice::tag_CommandAllocatorReset);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12Fence>
//-----------------------------------------------------------------------------
HRESULT	Wrap<ID3D12Fence>::SetEventOnCompletion(UINT64 Value, HANDLE hEvent) {
	return device->recording.WithObject(this).RunRecord(this, &ID3D12Fence::SetEventOnCompletion, RecDevice::tag_FenceSetEventOnCompletion, Value, hEvent);
}
HRESULT	Wrap<ID3D12Fence>::Signal(UINT64 Value) {
	return device->recording.WithObject(this).RunRecord(this, &ID3D12Fence::Signal, RecDevice::tag_FenceSignal, Value);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12PipelineState>
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	Wrap<ID3D12DescriptorHeap>
//-----------------------------------------------------------------------------

void Wrap<ID3D12DescriptorHeap>::init(Wrap<ID3D12Device> *_device, const D3D12_DESCRIPTOR_HEAP_DESC *desc) {
	RecObjectLink::init(_device);
	new(blob.create(sizeof(RecDescriptorHeap) + sizeof(DESCRIPTOR) * desc->NumDescriptors)) RecDescriptorHeap(desc->NumDescriptors, device->GetDescriptorHandleIncrementSize(desc->Type), orig->GetCPUDescriptorHandleForHeapStart());
//	count	= desc->NumDescriptors;
//	stride	= device->GetDescriptorHandleIncrementSize(desc->Type);
//	start	= orig->GetCPUDescriptorHandleForHeapStart();
//	backing	= new DESCRIPTOR[count];
	_device->descriptor_heaps.push_back(this);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12GraphicsCommandList>
//-----------------------------------------------------------------------------

void Wrap<ID3D12GraphicsCommandList>::init(Wrap<ID3D12Device> *_device) {
	RecObjectLink::init(_device);
	set_recording(device->recording.enable);
}

void Wrap<ID3D12GraphicsCommandList>::set_recording(bool enable) {
	if (enable && !recording.enable)
		device->recordings.push_back(&recording);
	recording.enable = enable;
}

HRESULT	Wrap<ID3D12GraphicsCommandList>::Close() {
	return recording.RunRecord(this, &ID3D12GraphicsCommandList::Close, tag_Close);
}
HRESULT	Wrap<ID3D12GraphicsCommandList>::Reset(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
	set_recording(device->recording.enable);
	return recording.RunRecord(this, &ID3D12GraphicsCommandList::Reset, tag_Reset, UnWrap(pAllocator), UnWrap(pInitialState));
}
void	Wrap<ID3D12GraphicsCommandList>::ClearState(ID3D12PipelineState *pPipelineState) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ClearState, tag_ClearState, UnWrap(pPipelineState));
}
void	Wrap<ID3D12GraphicsCommandList>::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::DrawInstanced, tag_DrawInstanced, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::DrawIndexedInstanced, tag_DrawIndexedInstanced, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::Dispatch, tag_Dispatch, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
void	Wrap<ID3D12GraphicsCommandList>::CopyBufferRegion(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes) {
	Wrap<ID3D12Resource>	*rdst = GetWrap(pDstBuffer), *rsrc = GetWrap(pSrcBuffer);
	recording.RunRecord(this, &ID3D12GraphicsCommandList::CopyBufferRegion, tag_CopyBufferRegion, rdst->orig, DstOffset, rsrc->orig, SrcOffset, NumBytes);
	rdst->init_data()->CopyBufferRegion(DstOffset, rsrc->rr, SrcOffset, NumBytes);
}
void	Wrap<ID3D12GraphicsCommandList>::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox) {
	D3D12_TEXTURE_COPY_LOCATION	dst		= *pDst,					src		= *pSrc;
	Wrap<ID3D12Resource>		*rdst	= GetWrap(dst.pResource),	*rsrc	= GetWrap(src.pResource);
	dst.pResource = rdst->orig;
	src.pResource = rsrc->orig;
	recording.RunRecord(this, &ID3D12GraphicsCommandList::CopyTextureRegion, tag_CopyTextureRegion, &dst, DstX, DstY, DstZ, &src, pSrcBox);

	RecResource	*rd	= rdst->init_data();
	RecResource	*rs	= rsrc->rr;

	rd->CopyTextureRegion(
		pDst->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT ? pDst->PlacedFootprint : rd->GetSubresource(device, pDst->SubresourceIndex),
		DstX, DstY, DstZ,
		rs,
		pSrc->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT ? pSrc->PlacedFootprint : rs->GetSubresource(device, pSrc->SubresourceIndex),
		pSrcBox,
		rdst->data_size
	);
}
void	Wrap<ID3D12GraphicsCommandList>::CopyResource(ID3D12Resource *pDst, ID3D12Resource *pSrc) {
	Wrap<ID3D12Resource>	*rdst = GetWrap(pDst), *rsrc = GetWrap(pSrc);
	recording.RunRecord(this, &ID3D12GraphicsCommandList::CopyResource, tag_CopyResource, rdst->orig, rsrc->orig);
	rdst->init_data()->CopyBufferRegion(0, rsrc->rr, 0, rdst->data_size);
}
void	Wrap<ID3D12GraphicsCommandList>::CopyTiles(ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::CopyTiles, tag_CopyTiles, UnWrap(pTiledResource), pTileRegionStartCoordinate, pTileRegionSize, UnWrap(pBuffer), BufferStartOffsetInBytes, Flags);
}
void	Wrap<ID3D12GraphicsCommandList>::ResolveSubresource(ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ResolveSubresource, tag_ResolveSubresource, UnWrap(pDstResource), DstSubresource, UnWrap(pSrcResource), SrcSubresource, Format);
}
void	Wrap<ID3D12GraphicsCommandList>::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::IASetPrimitiveTopology, tag_IASetPrimitiveTopology, PrimitiveTopology);
}
void	Wrap<ID3D12GraphicsCommandList>::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::RSSetViewports, tag_RSSetViewports, NumViewports, pViewports);
}
void	Wrap<ID3D12GraphicsCommandList>::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::RSSetScissorRects, tag_RSSetScissorRects, NumRects, pRects);
}
void	Wrap<ID3D12GraphicsCommandList>::OMSetBlendFactor(const FLOAT BlendFactor[4]) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::OMSetBlendFactor, tag_OMSetBlendFactor, BlendFactor);
}
void	Wrap<ID3D12GraphicsCommandList>::OMSetStencilRef(UINT StencilRef) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::OMSetStencilRef, tag_OMSetStencilRef, StencilRef);
}
void	Wrap<ID3D12GraphicsCommandList>::SetPipelineState(ID3D12PipelineState *pPipelineState) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetPipelineState, tag_SetPipelineState, UnWrap(pPipelineState));
}
void	Wrap<ID3D12GraphicsCommandList>::ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers) {
	D3D12_RESOURCE_BARRIER	*barriers = alloc_auto(D3D12_RESOURCE_BARRIER, NumBarriers);
	for (int i = 0; i < NumBarriers; i++) {
		barriers[i] = pBarriers[i];
		switch (barriers[i].Type) {
			case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
				barriers[i].Transition.pResource		= UnWrapCarefully(barriers[i].Transition.pResource);
				break;
			case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
				barriers[i].Aliasing.pResourceBefore	= UnWrap(barriers[i].Aliasing.pResourceBefore);
				barriers[i].Aliasing.pResourceAfter		= UnWrap(barriers[i].Aliasing.pResourceAfter);
				break;
			case D3D12_RESOURCE_BARRIER_TYPE_UAV:
				barriers[i].UAV.pResource				= UnWrap(barriers[i].UAV.pResource);
				break;
		}
	}
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ResourceBarrier, tag_ResourceBarrier, NumBarriers, barriers);
}
void	Wrap<ID3D12GraphicsCommandList>::ExecuteBundle(ID3D12GraphicsCommandList *pCommandList) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ExecuteBundle, tag_ExecuteBundle, UnWrap(pCommandList));
}
void	Wrap<ID3D12GraphicsCommandList>::SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *pp) {
#if 0
	ID3D12DescriptorHeap	**pp2 = alloc_auto(ID3D12DescriptorHeap*, NumDescriptorHeaps);
	for (int i = 0; i < NumDescriptorHeaps; i++)
		pp2[i] = UnWrap(pp[i]);
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetDescriptorHeaps, tag_SetDescriptorHeaps, NumDescriptorHeaps, pp2);
#else
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetDescriptorHeaps, tag_SetDescriptorHeaps, NumDescriptorHeaps, make_counted<0>(UnWrap(alloc_auto(ID3D12DescriptorHeap*, NumDescriptorHeaps), pp, NumDescriptorHeaps)));
#endif
}
void	Wrap<ID3D12GraphicsCommandList>::SetComputeRootSignature(ID3D12RootSignature *pRootSignature) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetComputeRootSignature, tag_SetComputeRootSignature, UnWrap(pRootSignature));
}
void	Wrap<ID3D12GraphicsCommandList>::SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetGraphicsRootSignature, tag_SetGraphicsRootSignature, UnWrap(pRootSignature));
}
void	Wrap<ID3D12GraphicsCommandList>::SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable, tag_SetComputeRootDescriptorTable, RootParameterIndex, BaseDescriptor);
}
void	Wrap<ID3D12GraphicsCommandList>::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable, tag_SetGraphicsRootDescriptorTable, RootParameterIndex, BaseDescriptor);
}
void	Wrap<ID3D12GraphicsCommandList>::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetComputeRoot32BitConstant, tag_SetComputeRoot32BitConstant, RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}
void	Wrap<ID3D12GraphicsCommandList>::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant, tag_SetGraphicsRoot32BitConstant, RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}
void	Wrap<ID3D12GraphicsCommandList>::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetComputeRoot32BitConstants, tag_SetComputeRoot32BitConstants, RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}
void	Wrap<ID3D12GraphicsCommandList>::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants, tag_SetGraphicsRoot32BitConstants, RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}
void	Wrap<ID3D12GraphicsCommandList>::SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetComputeRootConstantBufferView, tag_SetComputeRootConstantBufferView, RootParameterIndex, BufferLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView, tag_SetGraphicsRootConstantBufferView, RootParameterIndex, BufferLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetComputeRootShaderResourceView, tag_SetComputeRootShaderResourceView, RootParameterIndex, BufferLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView, tag_SetGraphicsRootShaderResourceView, RootParameterIndex, BufferLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView, tag_SetComputeRootUnorderedAccessView, RootParameterIndex, BufferLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView, tag_SetGraphicsRootUnorderedAccessView, RootParameterIndex, BufferLocation);
}
void	Wrap<ID3D12GraphicsCommandList>::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::IASetIndexBuffer, tag_IASetIndexBuffer, pView);
}
void	Wrap<ID3D12GraphicsCommandList>::IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::IASetVertexBuffers, tag_IASetVertexBuffers, StartSlot, NumViews, pViews);
}
void	Wrap<ID3D12GraphicsCommandList>::SOSetTargets(UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SOSetTargets, tag_SOSetTargets, StartSlot, NumViews, pViews);
}
void	Wrap<ID3D12GraphicsCommandList>::OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::OMSetRenderTargets, tag_OMSetRenderTargets, NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}
void	Wrap<ID3D12GraphicsCommandList>::ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ClearDepthStencilView, tag_ClearDepthStencilView, DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
}
void	Wrap<ID3D12GraphicsCommandList>::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ClearRenderTargetView, tag_ClearRenderTargetView, RenderTargetView, ColorRGBA, NumRects, pRects);
}
void	Wrap<ID3D12GraphicsCommandList>::ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint, tag_ClearUnorderedAccessViewUint, ViewGPUHandleInCurrentHeap, ViewCPUHandle, UnWrap(pResource), Values, NumRects, pRects);
}
void	Wrap<ID3D12GraphicsCommandList>::ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat, tag_ClearUnorderedAccessViewFloat, ViewGPUHandleInCurrentHeap, ViewCPUHandle, UnWrap(pResource), Values, NumRects, pRects);
}
void	Wrap<ID3D12GraphicsCommandList>::DiscardResource(ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::DiscardResource, tag_DiscardResource, UnWrap(pResource), pRegion);
}
void	Wrap<ID3D12GraphicsCommandList>::BeginQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::BeginQuery, tag_BeginQuery, UnWrap(pQueryHeap), Type, Index);
}
void	Wrap<ID3D12GraphicsCommandList>::EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::EndQuery, tag_EndQuery, UnWrap(pQueryHeap), Type, Index);
}
void	Wrap<ID3D12GraphicsCommandList>::ResolveQueryData(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ResolveQueryData, tag_ResolveQueryData, UnWrap(pQueryHeap), Type, StartIndex, NumQueries, UnWrap(pDestinationBuffer), AlignedDestinationBufferOffset);
}
void	Wrap<ID3D12GraphicsCommandList>::SetPredication(ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetPredication, tag_SetPredication, UnWrap(pBuffer), AlignedBufferOffset, Operation);
}
void	Wrap<ID3D12GraphicsCommandList>::SetMarker(UINT Metadata, const void *pData, UINT Size) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::SetMarker, tag_SetMarker, Metadata, pData, Size);
}
void	Wrap<ID3D12GraphicsCommandList>::BeginEvent(UINT Metadata, const void *pData, UINT Size) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::BeginEvent, tag_BeginEvent, Metadata, pData, Size);
}
void	Wrap<ID3D12GraphicsCommandList>::EndEvent() {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::EndEvent, tag_EndEvent);
}
void	Wrap<ID3D12GraphicsCommandList>::ExecuteIndirect(ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset) {
	recording.RunRecord(this, &ID3D12GraphicsCommandList::ExecuteIndirect, tag_ExecuteIndirect, UnWrap(pCommandSignature), MaxCommandCount, UnWrap(pArgumentBuffer), ArgumentBufferOffset, UnWrap(pCountBuffer), CountBufferOffset);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12CommandQueue>
//-----------------------------------------------------------------------------
void	Wrap<ID3D12CommandQueue>::Presenter::Present() {
	T_get_enclosing(this, &Wrap<ID3D12CommandQueue>::present)->device->set_recording(capture.Update());
}
void	Wrap<ID3D12CommandQueue>::UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags) {
	device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::UpdateTileMappings, RecDevice::tag_CommandQueueUpdateTileMappings, UnWrap(pResource), NumResourceRegions, pResourceRegionStartCoordinates, pResourceRegionSizes, UnWrap(pHeap), NumRanges, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}
void	Wrap<ID3D12CommandQueue>::CopyTileMappings(ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags) {
	device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::CopyTileMappings, RecDevice::tag_CommandQueueCopyTileMappings, UnWrap(pDstResource), pDstRegionStartCoordinate, UnWrap(pSrcResource), pSrcRegionStartCoordinate, pRegionSize, Flags);
}
void	Wrap<ID3D12CommandQueue>::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *pp) {
	ID3D12CommandList	**pp2 = alloc_auto(ID3D12CommandList*, NumCommandLists);
	for (int i = 0; i < NumCommandLists; i++)
		pp2[i] = UnWrap(pp[i]);
	device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::ExecuteCommandLists, RecDevice::tag_CommandQueueExecuteCommandLists, NumCommandLists, pp2);
}
void	Wrap<ID3D12CommandQueue>::SetMarker(UINT Metadata, const void *pData, UINT Size) {
	device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::SetMarker, RecDevice::tag_CommandQueueSetMarker, Metadata, pData, Size);
}
void	Wrap<ID3D12CommandQueue>::BeginEvent(UINT Metadata, const void *pData, UINT Size) {
	device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::BeginEvent, RecDevice::tag_CommandQueueBeginEvent, Metadata, pData, Size);
}
void	Wrap<ID3D12CommandQueue>::EndEvent() {
	device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::EndEvent, RecDevice::tag_CommandQueueEndEvent);
}
HRESULT	Wrap<ID3D12CommandQueue>::Signal(ID3D12Fence *pFence, UINT64 Value) {
	return device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::Signal, RecDevice::tag_CommandQueueSignal, UnWrap(pFence), Value);
}
HRESULT	Wrap<ID3D12CommandQueue>::Wait(ID3D12Fence *pFence, UINT64 Value) {
	return device->recording.WithObject(this).RunRecord(this, &ID3D12CommandQueue::Wait, RecDevice::tag_CommandQueueWait, UnWrap(pFence), Value);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12Device>
//-----------------------------------------------------------------------------

DESCRIPTOR *Wrap<ID3D12Device>::find_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE h) {
	for (auto &i : descriptor_heaps) {
		if (DESCRIPTOR *d = i.holds(h))
			return d;
	}
	return 0;
}

void Wrap<ID3D12Device>::set_recording(bool enable) {
	if (enable && !recording.enable) {
		capture.device = this;
		recordings.push_back(&recording);
	}
	recording.enable = enable;
}

HRESULT	Wrap<ID3D12Device>::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *desc, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateCommandQueue, tag_CreateCommandQueue, desc, (REFIID)riid, pp), (ID3D12CommandQueue**)pp, this, desc);
}
HRESULT	Wrap<ID3D12Device>::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateCommandAllocator, tag_CreateCommandAllocator, type, riid, pp), (ID3D12CommandAllocator**)pp, this);
}

HRESULT	Wrap<ID3D12Device>::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, REFIID riid, void **pp) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC	desc2 = *desc;
	desc2.pRootSignature = UnWrap(desc2.pRootSignature);
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateGraphicsPipelineState, tag_CreateGraphicsPipelineState, &desc2, riid, pp), (ID3D12PipelineState**)pp, this, desc);
}

HRESULT	Wrap<ID3D12Device>::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc, REFIID riid, void **pp) {
	D3D12_COMPUTE_PIPELINE_STATE_DESC	desc2 = *desc;
	desc2.pRootSignature = UnWrap(desc2.pRootSignature);
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateComputePipelineState, tag_CreateComputePipelineState, &desc2, riid, pp), (ID3D12PipelineState**)pp, this, desc);
}
HRESULT	Wrap<ID3D12Device>::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateCommandList, tag_CreateCommandList, nodeMask, type, UnWrap(pCommandAllocator), UnWrap(pInitialState), riid, pp), (ID3D12GraphicsCommandList**)pp, this);
}
HRESULT	Wrap<ID3D12Device>::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) {
	return recording.RunRecord(this, &ID3D12Device::CheckFeatureSupport, tag_CheckFeatureSupport, Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT	Wrap<ID3D12Device>::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateDescriptorHeap, tag_CreateDescriptorHeap, desc, riid, pp), (ID3D12DescriptorHeap**)pp, this, desc);
}
HRESULT	Wrap<ID3D12Device>::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateRootSignature, tag_CreateRootSignature, nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, pp), (ID3D12RootSignature**)pp, this, memory_block(unconst(pBlobWithRootSignature), blobLengthInBytes));
}
void	Wrap<ID3D12Device>::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	recording.RunRecord(this, &ID3D12Device::CreateConstantBufferView, tag_CreateConstantBufferView, desc, dest);
	DESCRIPTOR *d = find_descriptor(dest);
	d->type = DESCRIPTOR::CBV;
	if (desc) {
		d->cbv	= *desc;
	} else {
		clear(d->cbv);
	}
}

void	Wrap<ID3D12Device>::CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	recording.RunRecord(this, &ID3D12Device::CreateShaderResourceView, tag_CreateShaderResourceView, UnWrap(pResource), desc, dest);
	DESCRIPTOR *d	= find_descriptor(dest);
	d->type			= DESCRIPTOR::SRV;
	d->res			= pResource;
	if (desc) {
		d->srv	= *desc;
	} else {
		clear(d->srv);
		d->srv.Shader4ComponentMapping	= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	}
}

void	Wrap<ID3D12Device>::CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	recording.RunRecord(this, &ID3D12Device::CreateUnorderedAccessView, tag_CreateUnorderedAccessView, UnWrap(pResource), UnWrap(pCounterResource), desc, dest);
	DESCRIPTOR *d	= find_descriptor(dest);
	d->type			= DESCRIPTOR::UAV;
	d->res			= pResource;
	if (desc) {
		d->uav	= *desc;
	} else {
		clear(d->uav);
	}
}

void	Wrap<ID3D12Device>::CreateRenderTargetView(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	recording.RunRecord(this, &ID3D12Device::CreateRenderTargetView, tag_CreateRenderTargetView, UnWrapCarefully(pResource), desc, dest);
	
	DESCRIPTOR	*d	= find_descriptor(dest);
	d->type			= DESCRIPTOR::RTV;
	d->res			= pResource;
	if (desc) {
		d->rtv	= *desc;
		if (d->rtv.Format == DXGI_FORMAT_UNKNOWN)
			d->rtv.Format = pResource->GetDesc().Format;
	} else {
		clear(d->rtv);
		D3D12_RESOURCE_DESC	rdesc = pResource->GetDesc();
		d->rtv.Format = rdesc.Format;
		switch (rdesc.Dimension) {
			case D3D12_RESOURCE_DIMENSION_BUFFER:
				d->rtv.ViewDimension		= D3D12_RTV_DIMENSION_BUFFER;
				d->rtv.Buffer.NumElements	= rdesc.DepthOrArraySize;
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				d->rtv.ViewDimension		= D3D12_RTV_DIMENSION_TEXTURE1D;
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				d->rtv.ViewDimension		= D3D12_RTV_DIMENSION_TEXTURE2D;
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				d->rtv.ViewDimension		= D3D12_RTV_DIMENSION_TEXTURE3D;
				d->rtv.Texture3D.WSize		= rdesc.DepthOrArraySize;
				break;
		}
	}
}

void	Wrap<ID3D12Device>::CreateDepthStencilView(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	recording.RunRecord(this, &ID3D12Device::CreateDepthStencilView, tag_CreateDepthStencilView, UnWrap(pResource), desc, dest);
	
	DESCRIPTOR	*d	= find_descriptor(dest);
	d->type			= DESCRIPTOR::DSV;
	d->res			= pResource;
	if (desc) {
		d->dsv	= *desc;
		if (d->dsv.Format == DXGI_FORMAT_UNKNOWN)
			d->dsv.Format = pResource->GetDesc().Format;
	} else {
		clear(d->dsv);
		D3D12_RESOURCE_DESC	rdesc = pResource->GetDesc();
		d->dsv.Format = rdesc.Format;
		switch (rdesc.Dimension) {
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				d->dsv.ViewDimension	= D3D12_DSV_DIMENSION_TEXTURE1D;
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				d->dsv.ViewDimension	= D3D12_DSV_DIMENSION_TEXTURE2D;
				break;
		}
	}
}

void	Wrap<ID3D12Device>::CreateSampler(const D3D12_SAMPLER_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	recording.RunRecord(this, &ID3D12Device::CreateSampler, tag_CreateSampler, desc, dest);
	DESCRIPTOR *d	= find_descriptor(dest);
	d->type			= DESCRIPTOR::SMP;
	d->smp			= *desc;
}

void	Wrap<ID3D12Device>::CopyDescriptors(UINT dest_num, const D3D12_CPU_DESCRIPTOR_HANDLE *dest_starts, const UINT *dest_sizes, UINT srce_num, const D3D12_CPU_DESCRIPTOR_HANDLE *srce_starts, const UINT *srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type) {
	recording.RunRecord(this, &ID3D12Device::CopyDescriptors, tag_CopyDescriptors, dest_num, dest_starts, dest_sizes, srce_num, srce_starts, srce_sizes, type);
	UINT		ssize = 0;
	DESCRIPTOR	*sdesc;

	for (UINT s = 0, d = 0; d < dest_num; d++) {
		UINT		dsize	= dest_sizes[d];
		DESCRIPTOR	*ddesc	= find_descriptor(dest_starts[d]);
		while (dsize) {
			if (ssize == 0) {
				ssize	= srce_sizes[s];
				sdesc	= find_descriptor(srce_starts[s]);
				s++;
			}
			UINT	num = min(ssize, dsize);
			memcpy(ddesc, sdesc, sizeof(DESCRIPTOR) * num);
			ssize -= num;
			dsize -= num;
		}
	}
}

void	Wrap<ID3D12Device>::CopyDescriptorsSimple(UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
	recording.RunRecord(this, &ID3D12Device::CopyDescriptorsSimple, tag_CopyDescriptorsSimple, num, dest_start, srce_start, type);
	DESCRIPTOR		*sdesc	= find_descriptor(srce_start);
	DESCRIPTOR		*ddesc	= find_descriptor(dest_start);
	memcpy(ddesc, sdesc, sizeof(DESCRIPTOR) * num);
}

HRESULT	Wrap<ID3D12Device>::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateCommittedResource, tag_CreateCommittedResource, pHeapProperties, HeapFlags, desc, InitialResourceState, pOptimizedClearValue, riidResource, pp), (ID3D12Resource**)pp, this, desc);
}
HRESULT	Wrap<ID3D12Device>::CreateHeap(const D3D12_HEAP_DESC *desc, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateHeap, tag_CreateHeap, desc, riid, pp), (ID3D12Heap**)pp, this);
}
HRESULT	Wrap<ID3D12Device>::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreatePlacedResource, tag_CreatePlacedResource, UnWrap(pHeap), HeapOffset, desc, InitialState, pOptimizedClearValue, riid, pp), (ID3D12Resource**)pp, this, desc);
}
HRESULT	Wrap<ID3D12Device>::CreateReservedResource(const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateReservedResource, tag_CreateReservedResource, desc, InitialState, pOptimizedClearValue, riid, pp), (ID3D12Resource**)pp, this, desc);
}
HRESULT	Wrap<ID3D12Device>::CreateSharedHandle(ID3D12DeviceChild *pObject, const SECURITY_ATTRIBUTES *pAttributes, DWORD Access, LPCWSTR Name, HANDLE *pHandle) {
	return recording.RunRecord(this, &ID3D12Device::CreateSharedHandle, tag_CreateSharedHandle, UnWrap(pObject), pAttributes, Access, Name, pHandle);
}
HRESULT	Wrap<ID3D12Device>::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **pp) {
	return recording.RunRecord(this, &ID3D12Device::OpenSharedHandle, tag_OpenSharedHandle, NTHandle, riid, pp);
}
HRESULT	Wrap<ID3D12Device>::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle) {
	return recording.RunRecord(this, &ID3D12Device::OpenSharedHandleByName, tag_OpenSharedHandleByName, Name, Access, pNTHandle);
}
HRESULT	Wrap<ID3D12Device>::MakeResident(UINT NumObjects, ID3D12Pageable *const *pp) {
#if 0
	ID3D12Pageable	**pp2 = alloc_auto(ID3D12Pageable*, NumObjects);
	for (int i = 0; i < NumObjects; i++)
		pp2[i] = UnWrap(pp[i]);
	return recording.RunRecord(this, &ID3D12Device::MakeResident, tag_MakeResident, NumObjects, pp2);
#else
	return recording.RunRecord(this, &ID3D12Device::MakeResident, tag_MakeResident, NumObjects, make_counted<0>(UnWrap(alloc_auto(ID3D12Pageable*, NumObjects), pp, NumObjects)));
#endif

}
HRESULT	Wrap<ID3D12Device>::Evict(UINT NumObjects, ID3D12Pageable *const *pp) {
#if 0
	ID3D12Pageable	**pp2 = alloc_auto(ID3D12Pageable*, NumObjects);
	for (int i = 0; i < NumObjects; i++)
		pp2[i] = UnWrap(pp[i]);
	return recording.RunRecord(this, &ID3D12Device::Evict, tag_Evict, NumObjects, pp2);
#else
	return recording.RunRecord(this, &ID3D12Device::Evict, tag_Evict, NumObjects, make_counted<0>(UnWrap(alloc_auto(ID3D12Pageable*, NumObjects), pp, NumObjects)));
#endif
}
HRESULT	Wrap<ID3D12Device>::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateFence, tag_CreateFence, InitialValue, Flags, riid, pp), (ID3D12Fence**)pp, this);
}
HRESULT	Wrap<ID3D12Device>::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *desc, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateQueryHeap, tag_CreateQueryHeap, desc, riid, pp), (ID3D12QueryHeap**)pp, this);
}
HRESULT	Wrap<ID3D12Device>::SetStablePowerState(BOOL Enable) {
	return recording.RunRecord(this, &ID3D12Device::SetStablePowerState, tag_SetStablePowerState, Enable);
}
HRESULT	Wrap<ID3D12Device>::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *desc, ID3D12RootSignature *pRootSignature, REFIID riid, void **pp) {
	return MakeWrap(recording.RunRecord(this, &ID3D12Device::CreateCommandSignature, tag_CreateCommandSignature, desc, UnWrap(pRootSignature), riid, pp), (ID3D12CommandSignature**)pp, this);
}

//-----------------------------------------------------------------------------
//	Global
//-----------------------------------------------------------------------------

ReWrapperT<ID3D12Resource> rewrap_resource;

HRESULT WINAPI Hooked_D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** pp) {
	return _com_wrap::MakeWrap(get_orig(D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, pp), (ID3D12Device**)pp);
}

int Hook(int x) {
	HookDXGI();
	hook(D3D12CreateDevice, "d3d12.dll");
	ApplyHooks();
	return x * x;
}

CaptureStats Capture(int frames) {
	capture.Set(frames);
	capture.Wait();

	CaptureStats	stats;
	stats.num_cmdlists			= num_elements32(capture.device->recordings);
	stats.num_descriptor_heaps	= num_elements32(capture.device->descriptor_heaps);
	stats.num_objects			= num_elements32(capture.device->objects);

	uint32	names_len	= 0;
	for (auto &i : capture.device->objects) {
		names_len += i.name.length32() + 1;
	}

	stats.names = capture.names.create(names_len * 2);

	char16	*p = stats.names;
	for (auto &i : capture.device->objects) {
		uint32	len = i.name.length32();
		if (len == 0) {
			*p = 0;
		} else {
			memcpy(p, i.name.begin(), (len + 1) * 2);
		}
		p = p + len + 1;
	}

	return stats;
}

int Continue() {
	capture.Continue();
	return 0;
}

dynamic_array<Recording> GetRecordings() {
	dynamic_array<Recording>	r;
	for (auto &i : capture.device->recordings)
		new(r) Recording(i.type, i.recording.start, i.total);
	return r;
}

/*
dynamic_array<RecDescriptorHeap> GetDescriptorHeaps() {
	dynamic_array<RecDescriptorHeap>	r;
	for (auto &i : capture.device->descriptor_heaps)
		r.push_back(i);
	return r;
}
*/
dynamic_array<RecObject2> GetObjects() {
	dynamic_array<RecObject2>	r;
	for (auto &i : capture.device->objects) {
		auto	*obj	= static_cast<Wrap<ID3D12DeviceChild>*>(&i);
		new(r) RecObject2(i, obj, obj->orig, i.get_info());
	}
	return r;
}

EXT_RPC(Hook)
EXT_RPC(Capture)
EXT_RPC(Continue)
EXT_RPC(GetRecordings)
//EXT_RPC(GetDescriptorHeaps)
EXT_RPC(GetObjects)
