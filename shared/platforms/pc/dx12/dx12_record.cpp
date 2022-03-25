#include "hook_com.h"
#include "hook_stack.h"
#include "base/list.h"
#include "thread.h"
#include "dx12_record.h"
#include "dxgi_record.h"
#include "dx\dxgi_helpers.h"
#include "windows/nt.h"
#include <d3d11on12.h>
#include <d3d12sdklayers.h>

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "d3d11")

using namespace dx12;

struct chunked_recording : COMRecording {
	uint32				start;
	chunked_recording() : start(0) {}
	bool	empty()	const { return start == total; }
};

struct RecObjectLink : e_link<RecObjectLink>, RecObject {
	static e_list<RecObjectLink>	orphans;
	static Mutex					orphan_mutex;
	Wrap<ID3D12DeviceLatest>	*device;

	RecObjectLink() : device(0) {
		with(orphan_mutex), orphans.push_back(this);
	}
	~RecObjectLink() {
		if (is_linked())
			with(orphan_mutex), unlink();
	}

	void	init(Wrap<ID3D12DeviceLatest> *_device);
	virtual	const_memory_block	get_info()	{ return none; }
};

struct RecDescriptorHeapLink : e_link<RecDescriptorHeapLink> {
	malloc_block	heap;
	RecDescriptorHeap	*operator->() { return heap; }
	void init(
		D3D12_DESCRIPTOR_HEAP_TYPE	type, uint32 count, uint32 stride,
		D3D12_CPU_DESCRIPTOR_HANDLE	cpu, D3D12_GPU_DESCRIPTOR_HANDLE	gpu
	) {
		RecDescriptorHeap	*dh = heap.create(uintptr_t(((RecDescriptorHeap*)0)->descriptors + count));
		dh->type	= type;
		dh->cpu		= cpu;
		dh->gpu		= gpu;
		dh->stride	= stride;
		dh->count	= count;

		fill_new_n(dh->descriptors, count);
	}
};

Mutex					RecObjectLink::orphan_mutex;
e_list<RecObjectLink>	RecObjectLink::orphans;

#define RECORDCALLSTACK			if (recording.enable) RecordCallStack(0)
#define DEVICE_RECORDCALLSTACK	if (device->recording.enable) device->RecordCallStack(0)

//-----------------------------------------------------------------------------
//	Capture
//-----------------------------------------------------------------------------

Socket debug_sock;
#if 0
void WINAPI Hooked_OutputDebugStringW(LPCWSTR lpOutputString) {
	debug_sock.writebuff(lpOutputString, string_len32(lpOutputString));
}
void WINAPI Hooked_OutputDebugStringA(LPCSTR lpOutputString) {
	Hooked_OutputDebugStringW(string16(lpOutputString));
}
#else
void WINAPI Hooked_OutputDebugStringA(LPCSTR lpOutputString) {
	debug_sock.writebuff(lpOutputString, string_len32(lpOutputString));
}
void WINAPI Hooked_OutputDebugStringW(LPCWSTR lpOutputString) {
	Hooked_OutputDebugStringA(string(lpOutputString));
}
#endif

DWORD	WINAPI Hooked_WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable);
DWORD	WINAPI Hooked_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
HRESULT WINAPI Hooked_D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** pp);

HRESULT WINAPI Hooked_D3D11On12CreateDevice(
	IUnknown*				pDevice,
	UINT					Flags,
	const D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT					FeatureLevels,
	IUnknown* const*		ppCommandQueues,
	UINT					NumQueues,
	UINT					NodeMask,
	ID3D11Device**			ppDevice,
	ID3D11DeviceContext**	ppImmediateContext,
	D3D_FEATURE_LEVEL*		pChosenFeatureLevel
) {
	IUnknown**	ppCommandQueues2 = alloc_auto(IUnknown*, NumQueues);
	for (int i = 0; i < NumQueues; i++)
		ppCommandQueues2[i] = com_wrap_system->unwrap_carefully(ppCommandQueues[i]);

	return get_orig(D3D11On12CreateDevice)(
		com_wrap_system->unwrap_carefully(pDevice),
			Flags,
			pFeatureLevels,
			FeatureLevels,
			ppCommandQueues2,
			NumQueues,
			NodeMask,
			ppDevice,
			ppImmediateContext,
			pChosenFeatureLevel
		);
}

struct Capture {
	int			frames			= 1;
	Semaphore	sema_wait_cap	= Semaphore(0, 1);	// wait for capture
	Semaphore	sema_update		= Semaphore(0, 1);	// blocks update
	bool		paused			= false;

	Wrap<ID3D12DeviceLatest>	*device;

	Capture() {
		HookDXGI();
		hook(D3D12CreateDevice, "d3d12.dll");
		hook(D3D11On12CreateDevice, "d3d11.dll");
		hook(WaitForSingleObject, "kernel32.dll");
		hook(WaitForSingleObjectEx, "kernel32.dll");

		ApplyHooks();
		socket_init();

		RunThread([this]() {
			Socket listener = IP4::socket_addr(PORT(4567)).listener();
			if (listener.exists()) for (;;) {
				IP4::socket_addr	addr;
				Socket				sock = addr.accept(listener);

				switch (sock.getc()) {
					case INTF_GetMemory:		SocketRPC(sock, [this](uint64 address, uint64 size) {
						return const_memory_block((void*)address, size);
					}); break;

					case INTF_Pause:			SocketRPC(sock, [this]() {
						frames = 1;
					}); break;

					case INTF_Continue:			SocketRPC(sock, [this]() {
						sema_update.unlock();
					}); break;

					case INTF_CaptureFrames:	SocketRPC(sock, [this](int _frames) {
						com_wrap_system->defer_deletes(_frames);

						if (paused) {
							frames = _frames;
							sema_update.unlock();
						} else {
							frames = _frames + 1;
						}
						
						sema_wait_cap.try_lock();	// wait for frames
						sema_wait_cap.lock();		// wait for frames
						return CaptureFrames(device);

					}); break;

					case INTF_GetObjects:		SocketRPC(sock, [this]() {
						return GetObjects();
					}); break;

					case INTF_ResourceData:		SocketRPC(sock, ResourceData); break;
					case INTF_HeapData:			SocketRPC(sock, HeapData); break;

					case INTF_DebugOutput:
						if (!debug_sock.exists()) {
							hook(OutputDebugStringA, "kernel32.dll");
							hook(OutputDebugStringW, "kernel32.dll");
						}
						debug_sock = move(sock);
						break;
				}
			}
		});
	}

	static uint32					CaptureFrames(Wrap<ID3D12DeviceLatest> *device);
	with_size<dynamic_array<RecObject2>>	GetObjects();
	static malloc_block_all			ResourceData(uintptr_t _res);
	static malloc_block_all			HeapData(uintptr_t _heap);

	bool	Update() {
		if (frames && --frames == 0) {
			auto	w = with(RecObjectLink::orphan_mutex);
			sema_wait_cap.unlock();
			paused = true;
			sema_update.lock();
			paused = false;
			com_wrap_system->defer_deletes(0);
		}
		com_wrap_system->end_frame();
		return frames > 0;
	}
	void	Continue() {
		sema_update.unlock();
	}
};

static Capture capture;

//-----------------------------------------------------------------------------
//	Wraps
//-----------------------------------------------------------------------------

template<class T> class Wrap2<T, ID3D12Object> : public com_wrap<T>, public RecObjectLink {
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

template<class T> class Wrap2<T, ID3D12DeviceChild> : public Wrap2<T, ID3D12Object> {
public:
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D12DeviceChild>(this, riid, pp) ? S_OK : Wrap2<T, ID3D12Object>::QueryInterface(riid, pp);
	}
	HRESULT	STDMETHODCALLTYPE GetDevice(REFIID riid, void **pp)		{ *pp = device; device->AddRef(); return S_OK; }
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
	chunked_recording	recording;

	Wrap2() {}
	bool	set_recording(bool enable);
	void	init(Wrap<ID3D12DeviceLatest> *_device);

	void	RecordCallStack(const context &ctx) {
		recording.Record(0xfffe, CallStacks::Stack<32>(device->callstacks, ctx));
	}
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D12CommandList>(this, riid, pp) ? S_OK : Wrap2<T, ID3D12DeviceChild>::QueryInterface(riid, pp);
	}
	D3D12_COMMAND_LIST_TYPE	STDMETHODCALLTYPE GetType()		{ return orig->GetType(); }
};

template<> class Wrap<ID3D12LifetimeOwner> : public com_wrap<ID3D12LifetimeOwner> {
	void STDMETHODCALLTYPE LifetimeStateUpdated(D3D12_LIFETIME_STATE NewState)	{ orig->LifetimeStateUpdated(NewState); }
};

template<> class Wrap<ID3D12LifetimeTracker> : public Wrap2<ID3D12LifetimeTracker, ID3D12DeviceChild> {
public:
	void	init(Wrap<ID3D12DeviceLatest> *_device, ID3D12LifetimeOwner* pOwner) {
		RecObjectLink::init(_device);
	}

	HRESULT STDMETHODCALLTYPE DestroyOwnedObject(ID3D12DeviceChild *pObject)	{ return orig->DestroyOwnedObject(pObject); }
};
template<> class Wrap<ID3D12MetaCommand> : public Wrap2<ID3D12MetaCommand, ID3D12Pageable> {
public:
	void	init(Wrap<ID3D12DeviceLatest>* _device, REFGUID CommandId, UINT NodeMask, const void* pCreationParametersData, SIZE_T CreationParametersDataSizeInBytes) {
		RecObjectLink::init(_device);
	}

	UINT64 STDMETHODCALLTYPE GetRequiredParameterResourceSize(D3D12_META_COMMAND_PARAMETER_STAGE Stage, UINT ParameterIndex) { return orig->GetRequiredParameterResourceSize(Stage, ParameterIndex); }
};
template<> class Wrap<ID3D12PipelineLibraryLatest> : public Wrap2<ID3D12PipelineLibraryLatest, ID3D12DeviceChild> {
public:
	void	init(Wrap<ID3D12DeviceLatest>* _device, const void* pLibraryBlob, SIZE_T BlobLength) {
		RecObjectLink::init(_device);
	}

	//ID3D12PipelineLibrary
	HRESULT STDMETHODCALLTYPE StorePipeline(LPCWSTR pName, ID3D12PipelineState *pPipeline)																	{ return orig->StorePipeline(pName, pPipeline); }
	HRESULT STDMETHODCALLTYPE LoadGraphicsPipeline(LPCWSTR pName, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid,  void **ppPipelineState)	{ return orig->LoadGraphicsPipeline(pName, pDesc, riid, ppPipelineState); }
	HRESULT STDMETHODCALLTYPE LoadComputePipeline(LPCWSTR pName, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid,  void **ppPipelineState)		{ return orig->LoadComputePipeline(pName, pDesc, riid, ppPipelineState); }
	SIZE_T	STDMETHODCALLTYPE GetSerializedSize()																											{ return orig->GetSerializedSize(); }
	HRESULT STDMETHODCALLTYPE Serialize(void *pData, SIZE_T DataSizeInBytes)																				{ return orig->Serialize(pData, DataSizeInBytes); }

	//ID3D12PipelineLibrary1
	HRESULT STDMETHODCALLTYPE LoadPipeline(LPCWSTR pName, const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState)				{ return orig->LoadPipeline(pName, pDesc, riid, ppPipelineState); }
};

template<class T> class Wrap2<T, ID3D12ProtectedSession> : public Wrap2<T, ID3D12DeviceChild> {
public:
	void	init(Wrap<ID3D12DeviceLatest>* _device, const D3D12_PROTECTED_RESOURCE_SESSION_DESC* desc) {
		RecObjectLink::init(_device);
	}
	void	init(Wrap<ID3D12DeviceLatest>* _device, const D3D12_PROTECTED_RESOURCE_SESSION_DESC1* desc) {
		RecObjectLink::init(_device);
	}

	HRESULT STDMETHODCALLTYPE GetStatusFence(REFIID riid,  void **ppFence)	{ return orig->GetStatusFence(riid,  ppFence); }
	D3D12_PROTECTED_SESSION_STATUS STDMETHODCALLTYPE GetSessionStatus()		{ return orig->GetSessionStatus(); }
};

template<> class Wrap<ID3D12ProtectedResourceSession> : public Wrap2<ID3D12ProtectedResourceSession, ID3D12ProtectedSession> {
	D3D12_PROTECTED_RESOURCE_SESSION_DESC STDMETHODCALLTYPE GetDesc()		{ return orig->GetDesc();  }
};

template<> class Wrap<ID3D12StateObject> : public Wrap2<ID3D12StateObject, ID3D12Pageable> {
public:
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_STATE_OBJECT_DESC* pDesc) {
		RecObjectLink::init(_device);
	}
	void	init(Wrap<ID3D12DeviceLatest>* _device, const D3D12_STATE_OBJECT_DESC* pAddition, ID3D12StateObject* pStateObjectToGrowFrom) {
		RecObjectLink::init(_device);
	}
};

template<> class Wrap<ID3D12StateObjectProperties> : public com_wrap<ID3D12StateObjectProperties> {
	void*	STDMETHODCALLTYPE GetShaderIdentifier(LPCWSTR pExportName)				{ return orig->GetShaderIdentifier(pExportName); }
	UINT64	STDMETHODCALLTYPE GetShaderStackSize(LPCWSTR pExportName)				{ return orig->GetShaderStackSize(pExportName); }
	UINT64	STDMETHODCALLTYPE GetPipelineStackSize()								{ return orig->GetPipelineStackSize(); }
	void	STDMETHODCALLTYPE SetPipelineStackSize(UINT64 PipelineStackSizeInBytes)	{ return orig->SetPipelineStackSize(PipelineStackSizeInBytes); }
};
	

//-----------------------------------------------------------------------------
//	Wrap<ID3D12Device>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12DeviceLatest> : public Wrap2<ID3D12DeviceLatest, ID3D12Object>, RecDevice {
public:
	CallStacks							callstacks;
	chunked_recording					recording;
	e_list<RecDescriptorHeapLink>		descriptor_heaps;
	e_list<RecObjectLink>				objects;
	hash_set_with_key<HANDLE, true>		handles;

	Wrap() {
		type = Device;
		set_recording(capture.Update());
		init(this);
	}
	~Wrap() {
		set_recording(capture.Update());
	}

	virtual	const_memory_block	get_info()	{
		return recording.get_buffer_reset();
	}

	DESCRIPTOR*		find_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE h);
	void			set_recording(bool enable);

	void			RecordCallStack(const context &ctx) {
		recording.Record(0xfffe, CallStacks::Stack<32>(callstacks, ctx));
	}
	
	ULONG	Release()	{
		ULONG	n = Wrap2<ID3D12DeviceLatest, ID3D12Object>::Release();
		if (n == 0)
			set_recording(capture.Update());
		return n;
	}

	//ID3D12Device
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
	
	//ID3D12Device1
	HRESULT STDMETHODCALLTYPE CreatePipelineLibrary(const void *pLibraryBlob, SIZE_T BlobLength, REFIID riid, void **ppPipelineLibrary);
	HRESULT STDMETHODCALLTYPE SetEventOnMultipleFenceCompletion(ID3D12Fence *const *ppFences, const UINT64 *pFenceValues, UINT NumFences, D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent);
	HRESULT STDMETHODCALLTYPE SetResidencyPriority(UINT NumObjects, ID3D12Pageable *const *ppObjects, const D3D12_RESIDENCY_PRIORITY *pPriorities);

	//ID3D12Device2
	HRESULT STDMETHODCALLTYPE CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState);

	//ID3D12Device3
	HRESULT STDMETHODCALLTYPE OpenExistingHeapFromAddress(const void *pAddress, REFIID riid, void **ppvHeap);
	HRESULT STDMETHODCALLTYPE OpenExistingHeapFromFileMapping(HANDLE hFileMapping, REFIID riid, void **ppvHeap);
	HRESULT STDMETHODCALLTYPE EnqueueMakeResident(D3D12_RESIDENCY_FLAGS Flags, UINT NumObjects, ID3D12Pageable *const *ppObjects, ID3D12Fence *pFenceToSignal, UINT64 FenceValueToSignal);
	
	//ID3D12Device4
	HRESULT STDMETHODCALLTYPE CreateCommandList1(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags, REFIID riid, void **ppCommandList);
	HRESULT STDMETHODCALLTYPE CreateProtectedResourceSession(const D3D12_PROTECTED_RESOURCE_SESSION_DESC *pDesc, REFIID riid, void **ppSession);
	HRESULT STDMETHODCALLTYPE CreateCommittedResource1(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource);
	HRESULT STDMETHODCALLTYPE CreateHeap1(const D3D12_HEAP_DESC *pDesc, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvHeap);
	HRESULT STDMETHODCALLTYPE CreateReservedResource1(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvResource);
	D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE GetResourceAllocationInfo1(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC* pResourceDescs, D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1) {
		return orig->GetResourceAllocationInfo1(visibleMask, numResourceDescs, pResourceDescs, pResourceAllocationInfo1);
	}

	//ID3D12Device5
	HRESULT STDMETHODCALLTYPE CreateLifetimeTracker(ID3D12LifetimeOwner *pOwner, REFIID riid, void **ppvTracker);
	void	STDMETHODCALLTYPE RemoveDevice() {
		return orig->RemoveDevice();
	}
	HRESULT STDMETHODCALLTYPE EnumerateMetaCommands(UINT* pNumMetaCommands, D3D12_META_COMMAND_DESC* pDescs) {
		return orig->EnumerateMetaCommands(pNumMetaCommands, pDescs);
	}
	HRESULT STDMETHODCALLTYPE EnumerateMetaCommandParameters(REFGUID CommandId, D3D12_META_COMMAND_PARAMETER_STAGE Stage, UINT* pTotalStructureSizeInBytes, UINT* pParameterCount, D3D12_META_COMMAND_PARAMETER_DESC* pParameterDescs) {
		return orig->EnumerateMetaCommandParameters(CommandId, Stage, pTotalStructureSizeInBytes, pParameterCount, pParameterDescs);
	}
	HRESULT STDMETHODCALLTYPE CreateMetaCommand(REFGUID CommandId, UINT NodeMask, const void *pCreationParametersData, SIZE_T CreationParametersDataSizeInBytes, REFIID riid, void **ppMetaCommand);
	HRESULT STDMETHODCALLTYPE CreateStateObject(const D3D12_STATE_OBJECT_DESC *pDesc, REFIID riid, void **ppStateObject);
	void	STDMETHODCALLTYPE GetRaytracingAccelerationStructurePrebuildInfo(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo) {
		return orig->GetRaytracingAccelerationStructurePrebuildInfo(pDesc, pInfo);

	}
	D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS STDMETHODCALLTYPE CheckDriverMatchingIdentifier(D3D12_SERIALIZED_DATA_TYPE SerializedDataType, const D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER* pIdentifierToCheck) {
		return orig->CheckDriverMatchingIdentifier(SerializedDataType, pIdentifierToCheck);
	}
	
	//ID3D12Device6
	HRESULT STDMETHODCALLTYPE SetBackgroundProcessingMode(D3D12_BACKGROUND_PROCESSING_MODE Mode, D3D12_MEASUREMENTS_ACTION MeasurementsAction, HANDLE hEventToSignalUponCompletion, BOOL *pbFurtherMeasurementsDesired);
	
	//ID3D12Device7
	HRESULT STDMETHODCALLTYPE AddToStateObject(const D3D12_STATE_OBJECT_DESC *pAddition, ID3D12StateObject *pStateObjectToGrowFrom, REFIID riid, void **ppNewStateObject);
	HRESULT STDMETHODCALLTYPE CreateProtectedResourceSession1(const D3D12_PROTECTED_RESOURCE_SESSION_DESC1 *pDesc, REFIID riid, void **ppSession);

	//ID3D12Device8
	D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE GetResourceAllocationInfo2(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC1* pResourceDescs, D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1) {
		return orig->GetResourceAllocationInfo2(visibleMask, numResourceDescs, pResourceDescs, pResourceAllocationInfo1);
	}
	HRESULT STDMETHODCALLTYPE CreateCommittedResource2(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource);
	HRESULT STDMETHODCALLTYPE CreatePlacedResource1(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource);
	void	STDMETHODCALLTYPE CreateSamplerFeedbackUnorderedAccessView(ID3D12Resource *pTargetedResource, ID3D12Resource *pFeedbackResource, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);
	void	STDMETHODCALLTYPE GetCopyableFootprints1(const D3D12_RESOURCE_DESC1* pResourceDesc, UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts, UINT* pNumRows, UINT64* pRowSizeInBytes, UINT64* pTotalBytes) {
		return orig->GetCopyableFootprints1(pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes);
	}
		
};

void	RecObjectLink::init(Wrap<ID3D12DeviceLatest> *_device) {
	device	= _device;
	with(orphan_mutex), device->objects.push_back(unlink());
}

//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12DeviceChild>	: public Wrap2<ID3D12DeviceChild, ID3D12DeviceChild> {};
template<> class Wrap<ID3D12Pageable>		: public Wrap2<ID3D12Pageable, ID3D12Pageable> {};

template<> class Wrap<ID3D12RootSignature>	: public Wrap2<ID3D12RootSignature, ID3D12DeviceChild> {
	malloc_block	blob;
public:
	Wrap() { type = RootSignature; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes) {
		Wrap2<ID3D12RootSignature, ID3D12DeviceChild>::init(_device);
		blob = memory_block(unconst(pBlobWithRootSignature), blobLengthInBytes);
	}
	virtual	const_memory_block	get_info()	{ return blob; }
};

template<> class Wrap<ID3D12Heap> : public Wrap2<ID3D12Heap, ID3D12Pageable>, public RecHeap {
public:
	Wrap() { type = Heap; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_HEAP_DESC *desc, ID3D12ProtectedResourceSession* pProtectedSession = nullptr) {
		RecObjectLink::init(_device);
		RecHeap::init(*desc);
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, HANDLE handle) {
		RecObjectLink::init(_device);
		RecHeap::init(orig->GetDesc());
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, const void* pAddress) {
		RecObjectLink::init(_device);
		RecHeap::init(orig->GetDesc());
	}
	virtual	const_memory_block	get_info()			{ return static_cast<RecHeap*>(this); }
	D3D12_HEAP_DESC STDMETHODCALLTYPE GetDesc()	{ return orig->GetDesc(); }
};

//-----------------------------------------------------------------------------
//	Wrap<ID3D12Resource>
//-----------------------------------------------------------------------------

struct Waiter {
	com_ptr<ID3D12Fence>	fence;
	HANDLE					fence_event;
	uint32					fence_value;

	Waiter(ID3D12Device *device) {
		fence_value	= 0;
		device->CreateFence(fence_value, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fence);
		fence_event	= CreateEvent(nullptr, false, false, nullptr);
	}
	~Waiter() {
		CloseHandle(fence_event);
	}
	void	Wait(ID3D12CommandQueue *q) {
		q->Signal(fence, ++fence_value);
		fence->SetEventOnCompletion(fence_value, fence_event);
		WaitForSingleObject(fence_event, INFINITE);
	}
};

struct Transferer : Waiter {
	ID3D12Device						*device;
	com_ptr<ID3D12CommandQueue>			cmd_queue;
	com_ptr<ID3D12CommandAllocator>		cmd_alloc;
	com_ptr<ID3D12GraphicsCommandList>	cmd_list;

	Transferer(ID3D12Device *_device) : Waiter(_device), device(_device) {
		D3D12_COMMAND_QUEUE_DESC	qdesc = {D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_FLAG_NONE, 1};

		device->CreateCommandQueue(&qdesc, __uuidof(ID3D12CommandQueue), (void**)&cmd_queue);
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&cmd_alloc);
		device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc, 0, __uuidof(ID3D12GraphicsCommandList), (void**)&cmd_list);
	}

	void Transition(ID3D12Resource *res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
		if (from != to) {
			D3D12_RESOURCE_BARRIER	b;
			b.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			b.Flags						= D3D12_RESOURCE_BARRIER_FLAG_NONE;
			b.Transition.pResource		= res;
			b.Transition.StateBefore	= from;
			b.Transition.StateAfter		= to;
			b.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			cmd_list->ResourceBarrier(1, &b);
		}
	}

	ID3D12Resource* Transfer(ID3D12Resource *res, D3D12_RESOURCE_STATES state, uint64 *total_size);
	void			Wait() { Waiter::Wait(cmd_queue); }
};

ID3D12Resource* Transferer::Transfer(ID3D12Resource *res, D3D12_RESOURCE_STATES state, uint64 *total_size) {
	RESOURCE_DESC	desc		= res->GetDesc();
	uint32			nsub		= desc.MipLevels * desc.ArraySize() * desc.PlaneCount(device);
	auto			*footprints = alloc_auto(D3D12_PLACED_SUBRESOURCE_FOOTPRINT, nsub);

	device->GetCopyableFootprints(&desc, 0, nsub, 0, footprints, 0, 0, total_size);

	D3D12_RESOURCE_DESC	desc2;
	clear(desc2);
	desc2.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	desc2.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc2.Width				= *total_size;
	desc2.Height			= 1;
	desc2.DepthOrArraySize	= 1;
	desc2.MipLevels			= 1;
	desc2.SampleDesc.Count	= 1;

	ID3D12Resource		*res2;
	D3D12_HEAP_PROPERTIES	heap_props2;
	heap_props2.Type				= D3D12_HEAP_TYPE_READBACK;
	heap_props2.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props2.MemoryPoolPreference= D3D12_MEMORY_POOL_UNKNOWN;
	heap_props2.CreationNodeMask	= 1;
	heap_props2.VisibleNodeMask		= 1;
	device->CreateCommittedResource(&heap_props2, D3D12_HEAP_FLAG_NONE, &desc2, D3D12_RESOURCE_STATE_COPY_DEST, 0, __uuidof(ID3D12Resource), (void**)&res2);

	Transition(res, state, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_TEXTURE_COPY_LOCATION	srcloc, dstloc;
	srcloc.pResource		= res;
	dstloc.pResource		= res2;

	if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
		//srcloc.Type	= D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		//dstloc.Type	= D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		//srcloc.SubresourceIndex	= 0;
		//dstloc.SubresourceIndex	= 0;
		//cmd_list->CopyTextureRegion(&dstloc, 0, 0, 0, &srcloc, 0);
		cmd_list->CopyResource(res2, res);

	} else {
		srcloc.Type	= D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstloc.Type	= D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		for (int i = 0; i < nsub; i++) {
			srcloc.SubresourceIndex	= i;
			dstloc.PlacedFootprint	= footprints[i];
			cmd_list->CopyTextureRegion(&dstloc, 0, 0, 0, &srcloc, 0);
		}
	}

	Transition(res, D3D12_RESOURCE_STATE_COPY_SOURCE, state);

	cmd_list->Close();

	ID3D12CommandList	*cmd_list0 = cmd_list;
	cmd_queue->ExecuteCommandLists(1, &cmd_list0);

	Wait();
	return res2;
}

void TransferResource(ID3D12Resource *res, const memory_block &out) {
	void			*p;
	D3D12_RANGE		range	= {0, out.length()};
	if (SUCCEEDED(res->Map(0, &range, &p))) {
		memcpy(out, p, out.length());
		res->Unmap(0, 0);
	}
}

template<> class Wrap<ID3D12Resource> : public Wrap2<ID3D12Resource, ID3D12Pageable>, public RecResource {
public:
    D3D12_RESOURCE_STATES state;

	Wrap() { type = Resource; }

	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession = nullptr) {
		RecObjectLink::init(_device);
		RecResource::init(_device->orig, Committed, *desc);
		state	= InitialState;
		if (desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			gpu		= orig->GetGPUVirtualAddress();
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1 *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession = nullptr) {
		RecObjectLink::init(_device);
		RecResource::init(_device->orig, Committed, *desc);
		state	= InitialState;
		if (desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			gpu		= orig->GetGPUVirtualAddress();
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue) {
		RecObjectLink::init(_device);
		RecResource::init(_device->orig, Placed, *desc);
		state	= InitialState;
		gpu		= HeapOffset;
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1* desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue) {
		RecObjectLink::init(_device);
		RecResource::init(_device->orig, Committed, *desc);
		state	= InitialState;
		gpu		= HeapOffset;
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession = nullptr) {
		RecObjectLink::init(_device);
		RecResource::init(_device->orig, Reserved, *desc);
		state	= InitialState;
		//mapping = new TileMapping[div_round_up(data_size, D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES)];
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, HANDLE handle) {
		RecObjectLink::init(_device);
		RecResource::init(_device->orig, UnknownAlloc, orig->GetDesc());
		state	= D3D12_RESOURCE_STATE_COMMON;
	}

	void	set_state(D3D12_RESOURCE_STATES _state) {
		state = _state;
	};

	virtual	const_memory_block get_info() {
		return static_cast<RecResource*>(this);
	}

	HRESULT						STDMETHODCALLTYPE Map(UINT sub, const D3D12_RANGE *read, void **data)	{ return orig->Map(sub, read, data); }
	void						STDMETHODCALLTYPE Unmap(UINT sub, const D3D12_RANGE *written)			{ orig->Unmap(sub, written); }
	D3D12_RESOURCE_DESC			STDMETHODCALLTYPE GetDesc()												{ return orig->GetDesc(); }
	D3D12_GPU_VIRTUAL_ADDRESS	STDMETHODCALLTYPE GetGPUVirtualAddress()								{ return orig->GetGPUVirtualAddress(); }
	HRESULT	STDMETHODCALLTYPE WriteToSubresource(UINT DstSubresource, const D3D12_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
		return orig->WriteToSubresource(DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
	}
	HRESULT	STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, UINT SrcSubresource, const D3D12_BOX *pSrcBox) {
		return orig->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, SrcSubresource, pSrcBox);
	}
	HRESULT	STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS *pHeapFlags) {
		return orig->GetHeapProperties(pHeapProperties, pHeapFlags);
	}
};

ID3D12Resource* make_wrap_orphan(ID3D12Resource* p) {
	auto	w	= com_wrap_system->make_wrap(p);
	*static_cast<D3D12_RESOURCE_DESC*>(w) = p->GetDesc();
	return w;
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12CommandAllocator>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12CommandAllocator> : public Wrap2<ID3D12CommandAllocator, ID3D12Pageable> {
	D3D12_COMMAND_LIST_TYPE	list_type;
public:
	Wrap() { type = CommandAllocator; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_COMMAND_LIST_TYPE _list_type) {
		RecObjectLink::init(_device);
		list_type = _list_type;
	}
	HRESULT	STDMETHODCALLTYPE Reset() {
		DEVICE_RECORDCALLSTACK;
		return device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandAllocator::Reset, RecDevice::tag_CommandAllocatorReset);
	}
	virtual	const_memory_block	get_info()	{ return &list_type; }
};

//-----------------------------------------------------------------------------
//	Wrap<ID3D12Fence>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12FenceLatest> : public Wrap2<ID3D12FenceLatest, ID3D12Pageable> {
	uint64 current_value;
public:
	Wrap() { type = Fence; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, UINT64 InitialValue, D3D12_FENCE_FLAGS Flags) {
		current_value	= InitialValue;
		RecObjectLink::init(_device);
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, HANDLE handle) {
		RecObjectLink::init(_device);
	}
	virtual	const_memory_block	get_info()	{ return &current_value; }

	//ID3D12Fence
	UINT64	STDMETHODCALLTYPE GetCompletedValue() { return orig->GetCompletedValue(); }
	HRESULT	STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value, HANDLE hEvent) {
		device->handles.insert(hEvent);
		DEVICE_RECORDCALLSTACK;
		return device->recording.WithObject(this).RunRecord2(this, &ID3D12Fence::SetEventOnCompletion, RecDevice::tag_FenceSetEventOnCompletion, Value, hEvent);
	}
	HRESULT	STDMETHODCALLTYPE Signal(UINT64 Value) {
		current_value	= Value;
		DEVICE_RECORDCALLSTACK;
		return device->recording.WithObject(this).RunRecord2(this, &ID3D12Fence::Signal, RecDevice::tag_FenceSignal, Value);
	}
	
	//ID3D12Fence1
	D3D12_FENCE_FLAGS STDMETHODCALLTYPE GetCreationFlags() {return orig->GetCreationFlags(); }
};

//-----------------------------------------------------------------------------
//	Wrap<ID3D12PipelineState>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12PipelineState> : public Wrap2<ID3D12PipelineState, ID3D12Pageable> {
	malloc_block	blob;
public:
	Wrap() { type = PipelineState; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc) {
		RecObjectLink::init(_device);
		//blob = map_struct<RTM>(*desc);
		blob = map_struct<RTM>(make_tuple(0, *desc));
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc) {
		RecObjectLink::init(_device);
		//blob = map_struct<RTM>(*desc);
		blob = map_struct<RTM>(make_tuple(1, *desc));
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_PIPELINE_STATE_STREAM_DESC *desc) {
		RecObjectLink::init(_device);
		//blob = map_struct<RTM>(*desc);
		blob = map_struct<RTM>(make_tuple(2, *desc));
	}

	virtual	const_memory_block	get_info() { return blob; }
	HRESULT	STDMETHODCALLTYPE GetCachedBlob(ID3DBlob **pp) { return orig->GetCachedBlob(pp); }
};

//-----------------------------------------------------------------------------
//	Wrap<ID3D12DescriptorHeap>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12DescriptorHeap> : public Wrap2<ID3D12DescriptorHeap, ID3D12Pageable>, public RecDescriptorHeapLink {
public:
	Wrap() { RecObject::type = DescriptorHeap; }
//	~Wrap() { delete[] backing; }
	void		init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_DESCRIPTOR_HEAP_DESC *desc) {
		RecObjectLink::init(_device);
		RecDescriptorHeapLink::init(
			desc->Type,
			desc->NumDescriptors,
			device->GetDescriptorHandleIncrementSize(desc->Type),
			orig->GetCPUDescriptorHandleForHeapStart(),
			desc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE ? orig->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{0}
		);
		_device->descriptor_heaps.push_back(this);
	}
	virtual	const_memory_block	get_info() { return heap; }
	D3D12_DESCRIPTOR_HEAP_DESC	STDMETHODCALLTYPE GetDesc() { return orig->GetDesc(); }
	D3D12_CPU_DESCRIPTOR_HANDLE	STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart() { return orig->GetCPUDescriptorHandleForHeapStart(); }
	D3D12_GPU_DESCRIPTOR_HANDLE	STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart() { return orig->GetGPUDescriptorHandleForHeapStart(); }
};

//-----------------------------------------------------------------------------
//	Wrap<ID3D12QueryHeap>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12QueryHeap> : public Wrap2<ID3D12QueryHeap, ID3D12Pageable> {
public:
	Wrap() { type = QueryHeap; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_QUERY_HEAP_DESC *desc) {
		RecObjectLink::init(_device);
	}
};

//-----------------------------------------------------------------------------
//	Wrap<ID3D12CommandSignature>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12CommandSignature> : public Wrap2<ID3D12CommandSignature, ID3D12Pageable> {
	malloc_block	blob;
public:
	Wrap() { type = CommandSignature; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_COMMAND_SIGNATURE_DESC *desc, ID3D12RootSignature *pRootSignature) {
		blob = map_struct<RTM>(*desc);
		RecObjectLink::init(_device);
	}
	virtual	const_memory_block	get_info() { return blob; }
};

//-----------------------------------------------------------------------------
//	Wrap<ID3D12CommandList>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12CommandList>		: public Wrap2<ID3D12CommandList, ID3D12CommandList> {};

template<class T> void Wrap2<T, ID3D12CommandList>::init(Wrap<ID3D12DeviceLatest> *_device) {
	RecObjectLink::init(_device);
	set_recording(device->recording.enable);
}

template<class T> bool Wrap2<T, ID3D12CommandList>::set_recording(bool enable) {
	recording.enable	= enable;
	recording.start		= uint32(recording.total);
	return enable;
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12GraphicsCommandList>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12GraphicsCommandListLatest> : public Wrap2<ID3D12GraphicsCommandListLatest, ID3D12CommandList>, RecCommandList {
public:
	Wrap() { type = GraphicsCommandList; }
	~Wrap() {}
	void	init(Wrap<ID3D12DeviceLatest> *_device, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState) {
		RecCommandList::init(nodeMask, type, pCommandAllocator, pInitialState);
		Wrap2<ID3D12GraphicsCommandListLatest, ID3D12CommandList>::init(_device);
	}
	void	init(Wrap<ID3D12DeviceLatest> *_device, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags) {
		RecCommandList::init(nodeMask, type, flags);
		Wrap2<ID3D12GraphicsCommandListLatest, ID3D12CommandList>::init(_device);
	}
	virtual	const_memory_block	get_info() {
		buffer	= recording.get_buffer_reset();
		recording.start = 0;
		return (RecCommandList*)this;
	}

	//ID3D12GraphicsCommandList
	HRESULT	STDMETHODCALLTYPE Close() {
		if (device->recording.enable) {
			device->RecordCallStack(0);
			device->recording.WithObject(this).Record(RecDevice::tag_GraphicsCommandListClose);
		}
		return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::Close, tag_Close);
	}
	HRESULT	STDMETHODCALLTYPE Reset(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
		if (set_recording(device->recording.enable)) {
			device->RecordCallStack(0);
			device->recording.WithObject(this).Record(RecDevice::tag_GraphicsCommandListReset, pAllocator, pInitialState);
		}
		return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::Reset, tag_Reset, pAllocator, pInitialState);
	}
	void	STDMETHODCALLTYPE ClearState(ID3D12PipelineState *pPipelineState) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ClearState, tag_ClearState, pPipelineState);
	}
	void	STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
		RECORDCALLSTACK;
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::DrawInstanced, tag_DrawInstanced, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}
	void	STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) {
		RECORDCALLSTACK;
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::DrawIndexedInstanced, tag_DrawIndexedInstanced, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}
	void	STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) {
		RECORDCALLSTACK;
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::Dispatch, tag_Dispatch, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}
	void	STDMETHODCALLTYPE CopyBufferRegion(ID3D12Resource *pDst, UINT64 DstOffset, ID3D12Resource *pSrc, UINT64 SrcOffset, UINT64 NumBytes) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::CopyBufferRegion, tag_CopyBufferRegion, pDst, DstOffset, pSrc, SrcOffset, NumBytes);
/*		Wrap<ID3D12Resource>	*rsrc = GetWrap(pSrc);
		if (rsrc->RecResource::data) {
			rsrc->flush();
			Wrap<ID3D12Resource>	*rdst = GetWrap(pDst);
			rdst->CopyBufferRegion(DstOffset, rsrc, SrcOffset, NumBytes);
		}*/
	}
	void	STDMETHODCALLTYPE CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::CopyTextureRegion, tag_CopyTextureRegion, pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
		/*Wrap<ID3D12Resource>	*rsrc = GetWrap(pSrc->pResource);
		if (rsrc->RecResource::data) {
			rsrc->flush();
			Wrap<ID3D12Resource>	*rdst = GetWrap(pDst->pResource);
			rdst->CopyTextureRegion(
				pDst->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT ? pDst->PlacedFootprint : rdst->GetSubresource(device, pDst->SubresourceIndex),
				DstX, DstY, DstZ,
				rsrc,
				pSrc->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT ? pSrc->PlacedFootprint : rsrc->GetSubresource(device, pSrc->SubresourceIndex),
				pSrcBox
			);
		}*/
	}
	void	STDMETHODCALLTYPE CopyResource(ID3D12Resource *pDst, ID3D12Resource *pSrc) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::CopyResource, tag_CopyResource, pDst, pSrc);
		/*Wrap<ID3D12Resource>	*rsrc = GetWrap(pSrc);
		if (rsrc->RecResource::data) {
			rsrc->flush();
			Wrap<ID3D12Resource>	*rdst = GetWrap(pDst);
			rdst->CopyBufferRegion(0, rsrc, 0, rdst->RecResource::data.length());
		}*/
	}
	void	STDMETHODCALLTYPE CopyTiles(ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::CopyTiles, tag_CopyTiles, pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer, BufferStartOffsetInBytes, Flags);
	}
	void	STDMETHODCALLTYPE ResolveSubresource(ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ResolveSubresource, tag_ResolveSubresource, pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
	}
	void	STDMETHODCALLTYPE IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::IASetPrimitiveTopology, tag_IASetPrimitiveTopology, PrimitiveTopology);
	}
	void	STDMETHODCALLTYPE RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::RSSetViewports, tag_RSSetViewports, NumViewports, make_counted<0>(pViewports));
	}
	void	STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::RSSetScissorRects, tag_RSSetScissorRects, NumRects, make_counted<0>(pRects));
	}
	void	STDMETHODCALLTYPE OMSetBlendFactor(const FLOAT BlendFactor[4]) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::OMSetBlendFactor, tag_OMSetBlendFactor, BlendFactor);
	}
	void	STDMETHODCALLTYPE OMSetStencilRef(UINT StencilRef) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::OMSetStencilRef, tag_OMSetStencilRef, StencilRef);
	}
	void	STDMETHODCALLTYPE SetPipelineState(ID3D12PipelineState *pPipelineState) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetPipelineState, tag_SetPipelineState, pPipelineState);
	}
	void	STDMETHODCALLTYPE ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ResourceBarrier, tag_ResourceBarrier, NumBarriers, make_counted<0>(pBarriers));
		for (const D3D12_RESOURCE_BARRIER *p = pBarriers, *e = p + NumBarriers; p != e; ++p) {
			switch (p->Type) {
				case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
					com_wrap_system->get_wrap(p->Transition.pResource)->set_state(p->Transition.StateAfter);
					break;
				case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
				case D3D12_RESOURCE_BARRIER_TYPE_UAV:
					break;
			}
		}
	}
	void	STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList *pCommandList) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ExecuteBundle, tag_ExecuteBundle, pCommandList);
	}
	void	STDMETHODCALLTYPE SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *pp) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetDescriptorHeaps, tag_SetDescriptorHeaps, NumDescriptorHeaps, make_counted<0>(pp));
	}
	void	STDMETHODCALLTYPE SetComputeRootSignature(ID3D12RootSignature *pRootSignature) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetComputeRootSignature, tag_SetComputeRootSignature, pRootSignature);
	}
	void	STDMETHODCALLTYPE SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetGraphicsRootSignature, tag_SetGraphicsRootSignature, pRootSignature);
	}
	void	STDMETHODCALLTYPE SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetComputeRootDescriptorTable, tag_SetComputeRootDescriptorTable, RootParameterIndex, BaseDescriptor);
	}
	void	STDMETHODCALLTYPE SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetGraphicsRootDescriptorTable, tag_SetGraphicsRootDescriptorTable, RootParameterIndex, BaseDescriptor);
	}
	void	STDMETHODCALLTYPE SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetComputeRoot32BitConstant, tag_SetComputeRoot32BitConstant, RootParameterIndex, SrcData, DestOffsetIn32BitValues);
	}
	void	STDMETHODCALLTYPE SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetGraphicsRoot32BitConstant, tag_SetGraphicsRoot32BitConstant, RootParameterIndex, SrcData, DestOffsetIn32BitValues);
	}
	void	STDMETHODCALLTYPE SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetComputeRoot32BitConstants, tag_SetComputeRoot32BitConstants,
			RootParameterIndex, Num32BitValuesToSet, make_counted<1>((uint32*)pSrcData), DestOffsetIn32BitValues
		);
	}
	void	STDMETHODCALLTYPE SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetGraphicsRoot32BitConstants, tag_SetGraphicsRoot32BitConstants,
			RootParameterIndex, Num32BitValuesToSet, make_counted<1>((uint32*)pSrcData), DestOffsetIn32BitValues
		);
	}
	void	STDMETHODCALLTYPE SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetComputeRootConstantBufferView, tag_SetComputeRootConstantBufferView, RootParameterIndex, BufferLocation);
	}
	void	STDMETHODCALLTYPE SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetGraphicsRootConstantBufferView, tag_SetGraphicsRootConstantBufferView, RootParameterIndex, BufferLocation);
	}
	void	STDMETHODCALLTYPE SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetComputeRootShaderResourceView, tag_SetComputeRootShaderResourceView, RootParameterIndex, BufferLocation);
	}
	void	STDMETHODCALLTYPE SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetGraphicsRootShaderResourceView, tag_SetGraphicsRootShaderResourceView, RootParameterIndex, BufferLocation);
	}
	void	STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetComputeRootUnorderedAccessView, tag_SetComputeRootUnorderedAccessView, RootParameterIndex, BufferLocation);
	}
	void	STDMETHODCALLTYPE SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetGraphicsRootUnorderedAccessView, tag_SetGraphicsRootUnorderedAccessView, RootParameterIndex, BufferLocation);
	}
	void	STDMETHODCALLTYPE IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::IASetIndexBuffer, tag_IASetIndexBuffer, pView);
	}
	void	STDMETHODCALLTYPE IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::IASetVertexBuffers, tag_IASetVertexBuffers, StartSlot, NumViews, make_counted<1>(pViews));
	}
	void	STDMETHODCALLTYPE SOSetTargets(UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SOSetTargets, tag_SOSetTargets, StartSlot, NumViews, make_counted<1>(pViews));
	}
	void	STDMETHODCALLTYPE OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::OMSetRenderTargets, tag_OMSetRenderTargets, NumRenderTargetDescriptors, make_counted<0>(pRenderTargetDescriptors), RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
	}
	void	STDMETHODCALLTYPE ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ClearDepthStencilView, tag_ClearDepthStencilView, DepthStencilView, ClearFlags, Depth, Stencil, NumRects, make_counted<4>(pRects));
	}
	void	STDMETHODCALLTYPE ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ClearRenderTargetView, tag_ClearRenderTargetView, RenderTargetView, *(array<float, 4>*)ColorRGBA, NumRects, make_counted<2>(pRects));
	}
	void	STDMETHODCALLTYPE ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ClearUnorderedAccessViewUint, tag_ClearUnorderedAccessViewUint, ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, make_counted<4>(pRects));
	}
	void	STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ClearUnorderedAccessViewFloat, tag_ClearUnorderedAccessViewFloat, ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, make_counted<4>(pRects));
	}
	void	STDMETHODCALLTYPE DiscardResource(ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::DiscardResource, tag_DiscardResource, pResource, pRegion);
	}
	void	STDMETHODCALLTYPE BeginQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::BeginQuery, tag_BeginQuery, pQueryHeap, Type, Index);
	}
	void	STDMETHODCALLTYPE EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::EndQuery, tag_EndQuery, pQueryHeap, Type, Index);
	}
	void	STDMETHODCALLTYPE ResolveQueryData(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ResolveQueryData, tag_ResolveQueryData, pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
	}
	void	STDMETHODCALLTYPE SetPredication(ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation) {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetPredication, tag_SetPredication, pBuffer, AlignedBufferOffset, Operation);
	}
	void	STDMETHODCALLTYPE SetMarker(UINT Metadata, const void *pData, UINT Size) {
		RECORDCALLSTACK;
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetMarker, tag_SetMarker, Metadata, make_memory_block<2>(pData), Size);
	}
	void	STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void *pData, UINT Size) {
		RECORDCALLSTACK;
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::BeginEvent, tag_BeginEvent, Metadata, make_memory_block<2>(pData), Size);
	}
	void	STDMETHODCALLTYPE EndEvent() {
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::EndEvent, tag_EndEvent);
	}
	void	STDMETHODCALLTYPE ExecuteIndirect(ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset) {
		RECORDCALLSTACK;
		recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ExecuteIndirect, tag_ExecuteIndirect, pCommandSignature, MaxCommandCount, pArgumentBuffer, ArgumentBufferOffset, pCountBuffer, CountBufferOffset);
	}

	//ID3D12GraphicsCommandList1
	void	STDMETHODCALLTYPE AtomicCopyBufferUINT(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::AtomicCopyBufferUINT, tag_AtomicCopyBufferUINT, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies, make_counted<4>(ppDependentResources), make_counted<4>(pDependentSubresourceRanges));
	}
	void	STDMETHODCALLTYPE AtomicCopyBufferUINT64(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::AtomicCopyBufferUINT64, tag_AtomicCopyBufferUINT64, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies, make_counted<4>(ppDependentResources), make_counted<4>(pDependentSubresourceRanges));
	}
	void	STDMETHODCALLTYPE OMSetDepthBounds(FLOAT Min, FLOAT Max) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::OMSetDepthBounds, tag_OMSetDepthBounds, Min, Max);
	}
	void	STDMETHODCALLTYPE SetSamplePositions(UINT NumSamplesPerPixel, UINT NumPixels, D3D12_SAMPLE_POSITION *pSamplePositions) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetSamplePositions, tag_SetSamplePositions, NumSamplesPerPixel, NumPixels, pSamplePositions);
	}
	void	STDMETHODCALLTYPE ResolveSubresourceRegion(ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, ID3D12Resource *pSrcResource, UINT SrcSubresource, D3D12_RECT *pSrcRect, DXGI_FORMAT Format, D3D12_RESOLVE_MODE ResolveMode) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ResolveSubresourceRegion, tag_ResolveSubresourceRegion, pDstResource, DstSubresource, DstX, DstY, pSrcResource, SrcSubresource, pSrcRect, Format, ResolveMode);
	}
	void	STDMETHODCALLTYPE SetViewInstanceMask(UINT Mask) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetViewInstanceMask, tag_SetViewInstanceMask, Mask);
	}

	//ID3D12GraphicsCommandList2
	void	STDMETHODCALLTYPE WriteBufferImmediate(UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams, const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::WriteBufferImmediate, tag_WriteBufferImmediate, Count, make_counted<0>(pParams), make_counted<0>(pModes));
	}

	//ID3D12GraphicsCommandList3
	void	STDMETHODCALLTYPE SetProtectedResourceSession(ID3D12ProtectedResourceSession *pProtectedResourceSession) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetProtectedResourceSession, tag_SetProtectedResourceSession, pProtectedResourceSession);
	}

	//ID3D12GraphicsCommandList4
	void	STDMETHODCALLTYPE BeginRenderPass(UINT NumRenderTargets, const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets, const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::BeginRenderPass, tag_BeginRenderPass, NumRenderTargets, make_counted<0>(pRenderTargets), pDepthStencil, Flags);
	}
	void	STDMETHODCALLTYPE EndRenderPass() {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::EndRenderPass, tag_EndRenderPass);
	}
	void	STDMETHODCALLTYPE InitializeMetaCommand(ID3D12MetaCommand *pMetaCommand, const void *pInitializationParametersData, SIZE_T InitializationParametersDataSizeInBytes) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::InitializeMetaCommand, tag_InitializeMetaCommand, pMetaCommand, make_memory_block<2>(pInitializationParametersData), InitializationParametersDataSizeInBytes);
	}
	void	STDMETHODCALLTYPE ExecuteMetaCommand(ID3D12MetaCommand *pMetaCommand, const void *pExecutionParametersData, SIZE_T ExecutionParametersDataSizeInBytes) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::ExecuteMetaCommand, tag_ExecuteMetaCommand, pMetaCommand, make_memory_block<2>(pExecutionParametersData), ExecutionParametersDataSizeInBytes);
	}
	void	STDMETHODCALLTYPE BuildRaytracingAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc, UINT NumPostbuildInfoDescs, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::BuildRaytracingAccelerationStructure, tag_BuildRaytracingAccelerationStructure, pDesc, NumPostbuildInfoDescs, make_counted<1>(pPostbuildInfoDescs));
	}
	void	STDMETHODCALLTYPE EmitRaytracingAccelerationStructurePostbuildInfo(const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc, UINT NumSourceAccelerationStructures, const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::EmitRaytracingAccelerationStructurePostbuildInfo, tag_EmitRaytracingAccelerationStructurePostbuildInfo, pDesc, NumSourceAccelerationStructures, make_counted<1>(pSourceAccelerationStructureData));
	}
	void	STDMETHODCALLTYPE CopyRaytracingAccelerationStructure(D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData, D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::CopyRaytracingAccelerationStructure, tag_CopyRaytracingAccelerationStructure, DestAccelerationStructureData, SourceAccelerationStructureData, Mode);
	}
	void	STDMETHODCALLTYPE SetPipelineState1(ID3D12StateObject *pStateObject) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::SetPipelineState1, tag_SetPipelineState1, pStateObject);
	}
	void	STDMETHODCALLTYPE DispatchRays(const D3D12_DISPATCH_RAYS_DESC *pDesc) {
		 return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::DispatchRays, tag_DispatchRays, pDesc);
	}

	//ID3D12GraphicsCommandList5
	void	STDMETHODCALLTYPE RSSetShadingRate(D3D12_SHADING_RATE baseShadingRate, const D3D12_SHADING_RATE_COMBINER* combiners) {
		return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::RSSetShadingRate, tag_RSSetShadingRate, baseShadingRate, combiners);
	}
	void	STDMETHODCALLTYPE RSSetShadingRateImage(ID3D12Resource* shadingRateImage) {
		return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::RSSetShadingRateImage, tag_RSSetShadingRateImage, shadingRateImage);
	}

	//ID3D12GraphicsCommandList6
	void	STDMETHODCALLTYPE DispatchMesh(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) {
		return recording.RunRecord2(this, &ID3D12GraphicsCommandListLatest::DispatchMesh, tag_DispatchMesh, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}
};

CommandRange::CommandRange(ID3D12CommandList *_t) : holder<const ID3D12CommandList*>(_t) {
	auto	*w	= com_wrap_system->get_wrap(_t);
	a			= w->recording.start;
	b			= uint32(w->recording.total);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12CommandQueue>
//-----------------------------------------------------------------------------

template<> class Wrap<ID3D12CommandQueue> : public Wrap2<ID3D12CommandQueue, ID3D12Pageable>, D3D12_COMMAND_QUEUE_DESC {
public:
	struct Presenter : PresentDevice {
		void	Present();
	} present;

	Wrap() { type = CommandQueue; }
	void	init(Wrap<ID3D12DeviceLatest> *_device, const D3D12_COMMAND_QUEUE_DESC *desc) {
		RecObjectLink::init(_device);
		*(D3D12_COMMAND_QUEUE_DESC*)this = *desc;
	}
	virtual	const_memory_block	get_info() { return static_cast<D3D12_COMMAND_QUEUE_DESC*>(this); }

	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		if (riid == __uuidof(PresentDevice)) {
			*pp = &present;
			return S_OK;
		}
		return Wrap2<ID3D12CommandQueue, ID3D12Pageable>::QueryInterface(riid, pp);
	}
	void	STDMETHODCALLTYPE UpdateTileMappings(
		ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes,
		ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts,
		D3D12_TILE_MAPPING_FLAGS Flags
	) {
		DEVICE_RECORDCALLSTACK;
		device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::UpdateTileMappings, RecDevice::tag_CommandQueueUpdateTileMappings,
			pResource, NumResourceRegions, make_counted<1>(pResourceRegionStartCoordinates), make_counted<1>(pResourceRegionSizes),
			pHeap, NumRanges, make_counted<5>(pRangeFlags), make_counted<5>(pHeapRangeStartOffsets), make_counted<5>(pRangeTileCounts),
			Flags
		);
#if 0
		Wrap<ID3D12Resource>	*rsrc = GetWrap(pResource);
		Tiler					tiler(*rsrc, rsrc->mapping);
		TileSource				src(pHeap, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts);

		for (UINT s = 0, d = 0; d < NumResourceRegions; d++) {
			const D3D12_TILED_RESOURCE_COORDINATE	&dstart	= pResourceRegionStartCoordinates[d];
			const D3D12_TILE_REGION_SIZE			&dsize	= pResourceRegionSizes[d];
			Tiler0									tiler0	= tiler.SubResource(dstart.Subresource);

			if (dsize.UseBox)
				tiler0.FillBox(src, dstart.X, dstart.Y, dstart.Z, dsize.Width, dsize.Height, dsize.Depth);
			else
				tiler0.Fill(src, dstart.X, dstart.Y, dstart.Z, dsize.NumTiles);
		}
#endif
	}
	void	STDMETHODCALLTYPE CopyTileMappings(
		ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
		ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
		const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags
	) {
		DEVICE_RECORDCALLSTACK;
		device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::CopyTileMappings, RecDevice::tag_CommandQueueCopyTileMappings,
			pDstResource, pDstRegionStartCoordinate,
			pSrcResource, pSrcRegionStartCoordinate,
			pRegionSize, Flags
		);
#if 0
		Wrap<ID3D12Resource>	*rsrc = GetWrap(pDstResource);
		Wrap<ID3D12Resource>	*rdst = GetWrap(pSrcResource);
		Tiler					stiler(*rsrc, rsrc->mapping);
		Tiler					dtiler(*rdst, rdst->mapping);

		Tiler0					dtiler0	= dtiler.SubResource(pDstRegionStartCoordinate->Subresource);
		Tiler0					stiler0	= dtiler.SubResource(pSrcRegionStartCoordinate->Subresource);

		if (pRegionSize->UseBox)
			dtiler0.CopyBox(stiler0,
				pDstRegionStartCoordinate->X, pDstRegionStartCoordinate->Y, pDstRegionStartCoordinate->Z,
				pSrcRegionStartCoordinate->X, pSrcRegionStartCoordinate->Y, pSrcRegionStartCoordinate->Z,
				pRegionSize->Width, pRegionSize->Height, pRegionSize->Depth
			);
		else
			dtiler0.Copy(stiler0,
				pDstRegionStartCoordinate->X, pDstRegionStartCoordinate->Y, pDstRegionStartCoordinate->Z,
				pSrcRegionStartCoordinate->X, pSrcRegionStartCoordinate->Y, pSrcRegionStartCoordinate->Z,
				pRegionSize->NumTiles
			);
#endif
	}
	void	STDMETHODCALLTYPE ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *pp) {
		DEVICE_RECORDCALLSTACK;
#if 0
		device->recording.WithObject(this).Record(RecDevice::tag_CommandQueueExecuteCommandLists, NumCommandLists, make_counted<0>(pp));
		device->set_recording(capture.Update());
		device->recording.Run2(this, &ID3D12CommandQueue::ExecuteCommandLists, NumCommandLists, make_counted<0>(pp));
#else
		with(RecObjectLink::orphan_mutex), device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::ExecuteCommandLists, RecDevice::tag_CommandQueueExecuteCommandLists,
			NumCommandLists, make_counted<0>(pp)
		);
#endif
	}
	void	STDMETHODCALLTYPE SetMarker(UINT Metadata, const void *pData, UINT Size) {
		DEVICE_RECORDCALLSTACK;
		device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::SetMarker, RecDevice::tag_CommandQueueSetMarker, Metadata, make_memory_block<2>(pData), Size);
	}
	void	STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void *pData, UINT Size) {
		DEVICE_RECORDCALLSTACK;
		device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::BeginEvent, RecDevice::tag_CommandQueueBeginEvent, Metadata, make_memory_block<2>(pData), Size);
	}
	void	STDMETHODCALLTYPE EndEvent() {
		DEVICE_RECORDCALLSTACK;
		device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::EndEvent, RecDevice::tag_CommandQueueEndEvent);
	}
	HRESULT	STDMETHODCALLTYPE Signal(ID3D12Fence *pFence, UINT64 Value) {
		DEVICE_RECORDCALLSTACK;
		return device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::Signal, RecDevice::tag_CommandQueueSignal, pFence, Value);
	}
	HRESULT	STDMETHODCALLTYPE Wait(ID3D12Fence *pFence, UINT64 Value) {
		DEVICE_RECORDCALLSTACK;
		return device->recording.WithObject(this).RunRecord2(this, &ID3D12CommandQueue::Wait, RecDevice::tag_CommandQueueWait, pFence, Value);
	}
	HRESULT	STDMETHODCALLTYPE GetTimestampFrequency(UINT64 *pFrequency) { return orig->GetTimestampFrequency(pFrequency); }
	HRESULT	STDMETHODCALLTYPE GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp) { return orig->GetClockCalibration(pGpuTimestamp, pCpuTimestamp); }
	D3D12_COMMAND_QUEUE_DESC	STDMETHODCALLTYPE GetDesc() { return orig->GetDesc(); }
};

void	Wrap<ID3D12CommandQueue>::Presenter::Present() {
	T_get_enclosing(this, &Wrap<ID3D12CommandQueue>::present)->device->set_recording(capture.Update());
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D12DeviceLatest>
//-----------------------------------------------------------------------------

DESCRIPTOR *Wrap<ID3D12DeviceLatest>::find_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE h) {
	for (auto &i : descriptor_heaps) {
		if (const DESCRIPTOR *d = i->holds(h))
			return unconst(d);
	}
	return 0;
}

void Wrap<ID3D12DeviceLatest>::set_recording(bool enable) {
	capture.device		= enable ? this : nullptr;
	recording.enable	= enable;
	recording.start		= uint32(recording.total);
}

HRESULT	Wrap<ID3D12DeviceLatest>::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *desc, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateCommandQueue, tag_CreateCommandQueue, riid, (ID3D12CommandQueue**)pp, desc);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateCommandAllocator, tag_CreateCommandAllocator, riid, (ID3D12CommandAllocator**)pp, type);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateGraphicsPipelineState, tag_CreateGraphicsPipelineState, riid, (ID3D12PipelineState**)pp, desc);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateComputePipelineState, tag_CreateComputePipelineState, riid, (ID3D12PipelineState**)pp, desc);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateCommandList, tag_CreateCommandList, riid, (ID3D12GraphicsCommandList**)pp, nodeMask, type, pCommandAllocator, pInitialState);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D12Device::CheckFeatureSupport, tag_CheckFeatureSupport, Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateDescriptorHeap, tag_CreateDescriptorHeap, riid, (ID3D12DescriptorHeap**)pp, desc);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateRootSignature, tag_CreateRootSignature, riid, (ID3D12RootSignature**)pp, nodeMask, make_memory_block<2>(pBlobWithRootSignature), blobLengthInBytes);
}
void	Wrap<ID3D12DeviceLatest>::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CreateConstantBufferView, tag_CreateConstantBufferView, desc, dest);
	DESCRIPTOR *d = find_descriptor(dest);
	d->set(desc);
}
void	Wrap<ID3D12DeviceLatest>::CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CreateShaderResourceView, tag_CreateShaderResourceView, pResource, desc, dest);
	if (pResource) {
		DESCRIPTOR *d	= find_descriptor(dest);
		d->set(pResource, desc);
	}
}

void	Wrap<ID3D12DeviceLatest>::CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CreateUnorderedAccessView, tag_CreateUnorderedAccessView, pResource, pCounterResource, desc, dest);
	if (pResource) {
		DESCRIPTOR *d	= find_descriptor(dest);
		d->set(pResource, desc);
	}
}

void	Wrap<ID3D12DeviceLatest>::CreateRenderTargetView(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CreateRenderTargetView, tag_CreateRenderTargetView, pResource, desc, dest);
	if (pResource) {
		DESCRIPTOR	*d	= find_descriptor(dest);
		d->set(pResource, desc, pResource->GetDesc());
	}
}
void	Wrap<ID3D12DeviceLatest>::CreateDepthStencilView(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CreateDepthStencilView, tag_CreateDepthStencilView, pResource, desc, dest);
	if (pResource) {
		DESCRIPTOR	*d	= find_descriptor(dest);
		d->set(pResource, desc, pResource->GetDesc());
	}
}
void	Wrap<ID3D12DeviceLatest>::CreateSampler(const D3D12_SAMPLER_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CreateSampler, tag_CreateSampler, desc, dest);
	DESCRIPTOR *d	= find_descriptor(dest);
	d->type			= DESCRIPTOR::SMP;
	d->smp			= *desc;
}
void	Wrap<ID3D12DeviceLatest>::CopyDescriptors(UINT dest_num, const D3D12_CPU_DESCRIPTOR_HANDLE *dest_starts, const UINT *dest_sizes, UINT srce_num, const D3D12_CPU_DESCRIPTOR_HANDLE *srce_starts, const UINT *srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CopyDescriptors, tag_CopyDescriptors,
		dest_num, make_counted<0>(dest_starts), make_counted<0>(dest_sizes),
		srce_num, make_counted<3>(srce_starts), make_counted<3>(srce_sizes),
		type
	);
	UINT		ssize = 0;
	DESCRIPTOR	*sdesc;

	for (UINT s = 0, d = 0; d < dest_num; d++) {
		UINT		dsize	= dest_sizes[d];
		DESCRIPTOR	*ddesc	= find_descriptor(dest_starts[d]);
		while (dsize) {
			if (ssize == 0) {
				sdesc	= find_descriptor(srce_starts[s]);
				ssize	= srce_sizes ? srce_sizes[s] : dsize;
				s++;
			}
			UINT	num = min(ssize, dsize);
			memcpy(ddesc, sdesc, sizeof(DESCRIPTOR) * num);
			ddesc += num;
			ssize -= num;
			dsize -= num;
		}
	}
}
void	Wrap<ID3D12DeviceLatest>::CopyDescriptorsSimple(UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D12Device::CopyDescriptorsSimple, tag_CopyDescriptorsSimple,
		num, dest_start, srce_start, type
	);
	memcpy(find_descriptor(dest_start), find_descriptor(srce_start), sizeof(DESCRIPTOR) * num);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateCommittedResource, tag_CreateCommittedResource, riidResource, (ID3D12Resource**)pp, pHeapProperties, HeapFlags, desc, InitialResourceState, pOptimizedClearValue);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateHeap(const D3D12_HEAP_DESC *desc, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateHeap, tag_CreateHeap, riid, (ID3D12Heap**)pp, desc);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreatePlacedResource, tag_CreatePlacedResource, riid, (ID3D12Resource**)pp, pHeap, HeapOffset, desc, InitialState, pOptimizedClearValue);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateReservedResource(const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateReservedResource, tag_CreateReservedResource, riid, (ID3D12Resource**)pp, desc, InitialState, pOptimizedClearValue);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateSharedHandle(ID3D12DeviceChild *pObject, const SECURITY_ATTRIBUTES *pAttributes, DWORD Access, LPCWSTR Name, HANDLE *pHandle) {
	RECORDCALLSTACK;
	HRESULT	r = recording.RunRecord2(this, &ID3D12Device::CreateSharedHandle, tag_CreateSharedHandle, pObject, pAttributes, Access, Name, pHandle);
	if (SUCCEEDED(r))
		device->handles.insert(*pHandle);
	return r;
}
HRESULT	Wrap<ID3D12DeviceLatest>::OpenSharedHandle(HANDLE handle, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	device->handles.insert(handle);
	return	riid == __uuidof(ID3D12Heap)		? recording.RunRecord2Wrap(this, &ID3D12Device::OpenSharedHandle, tag_OpenSharedHandle, riid, (ID3D12Heap**)pp, handle)
		:	riid == __uuidof(ID3D12Resource)	? recording.RunRecord2Wrap(this, &ID3D12Device::OpenSharedHandle, tag_OpenSharedHandle, riid, (ID3D12Resource**)pp, handle)
		:	riid == __uuidof(ID3D12Fence)		? recording.RunRecord2Wrap(this, &ID3D12Device::OpenSharedHandle, tag_OpenSharedHandle, riid, (ID3D12Fence**)pp, handle)
		: recording.RunRecord2(this, &ID3D12Device::OpenSharedHandle, tag_OpenSharedHandle, handle, riid, pp);
}
HRESULT	Wrap<ID3D12DeviceLatest>::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pHandle) {
	RECORDCALLSTACK;
	HRESULT	r = recording.RunRecord2(this, &ID3D12Device::OpenSharedHandleByName, tag_OpenSharedHandleByName, Name, Access, pHandle);
	if (SUCCEEDED(r))
		device->handles.insert(*pHandle);
	return r;
}
HRESULT	Wrap<ID3D12DeviceLatest>::MakeResident(UINT NumObjects, ID3D12Pageable *const *pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D12Device::MakeResident, tag_MakeResident, NumObjects, make_counted<0>(pp));
}
HRESULT	Wrap<ID3D12DeviceLatest>::Evict(UINT NumObjects, ID3D12Pageable *const *pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D12Device::Evict, tag_Evict, NumObjects, make_counted<0>(pp));
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateFence, tag_CreateFence, riid, (ID3D12Fence**)pp, InitialValue, Flags);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *desc, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateQueryHeap, tag_CreateQueryHeap, riid, (ID3D12QueryHeap**)pp, desc);
}
HRESULT	Wrap<ID3D12DeviceLatest>::SetStablePowerState(BOOL Enable) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D12Device::SetStablePowerState, tag_SetStablePowerState, Enable);
}
HRESULT	Wrap<ID3D12DeviceLatest>::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *desc, ID3D12RootSignature *pRootSignature, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D12Device::CreateCommandSignature, tag_CreateCommandSignature, riid, (ID3D12CommandSignature**)pp, desc, pRootSignature);
}

//ID3D12Device1
HRESULT Wrap<ID3D12DeviceLatest>::CreatePipelineLibrary(const void* pLibraryBlob, SIZE_T BlobLength, REFIID riid, void** ppPipelineLibrary) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreatePipelineLibrary, tag_CreatePipelineLibrary, riid, (ID3D12PipelineLibrary**)ppPipelineLibrary, pLibraryBlob, BlobLength);
}
HRESULT Wrap<ID3D12DeviceLatest>::SetEventOnMultipleFenceCompletion(ID3D12Fence* const* ppFences, const UINT64* pFenceValues, UINT NumFences, D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent) {
	return recording.RunRecord2(this, &ID3D12DeviceLatest::SetEventOnMultipleFenceCompletion, tag_SetEventOnMultipleFenceCompletion, make_counted<2>(ppFences), make_counted<2>(pFenceValues), NumFences, Flags, hEvent);
}
HRESULT Wrap<ID3D12DeviceLatest>::SetResidencyPriority(UINT NumObjects, ID3D12Pageable* const* ppObjects, const D3D12_RESIDENCY_PRIORITY* pPriorities) {
	return recording.RunRecord2(this, &ID3D12DeviceLatest::SetResidencyPriority, tag_SetResidencyPriority, NumObjects, make_counted<0>(ppObjects), make_counted<0>(pPriorities));
}

//ID3D12Device2

HRESULT Wrap<ID3D12DeviceLatest>::CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc, REFIID riid, void** ppPipelineState) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreatePipelineState, tag_CreatePipelineState, riid, (ID3D12PipelineState**)ppPipelineState, pDesc);
}

//ID3D12Device3
HRESULT Wrap<ID3D12DeviceLatest>::OpenExistingHeapFromAddress(const void* pAddress, REFIID riid, void** ppvHeap) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::OpenExistingHeapFromAddress, tag_OpenExistingHeapFromAddress, riid, (ID3D12Heap**)ppvHeap, pAddress);
}
HRESULT Wrap<ID3D12DeviceLatest>::OpenExistingHeapFromFileMapping(HANDLE hFileMapping, REFIID riid, void** ppvHeap) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::OpenExistingHeapFromFileMapping, tag_OpenExistingHeapFromFileMapping, riid, (ID3D12Heap**)ppvHeap, hFileMapping);
}
HRESULT Wrap<ID3D12DeviceLatest>::EnqueueMakeResident(D3D12_RESIDENCY_FLAGS Flags, UINT NumObjects, ID3D12Pageable* const* ppObjects, ID3D12Fence* pFenceToSignal, UINT64 FenceValueToSignal) {
	return recording.RunRecord2(this, &ID3D12DeviceLatest::EnqueueMakeResident, tag_EnqueueMakeResident, Flags, NumObjects, make_counted<1>(ppObjects), pFenceToSignal, FenceValueToSignal);
}

//ID3D12Device4
HRESULT Wrap<ID3D12DeviceLatest>::CreateCommandList1(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags, REFIID riid, void** ppCommandList) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateCommandList1, tag_CreateCommandList1, riid, (ID3D12GraphicsCommandListLatest**)ppCommandList, nodeMask, type, flags);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreateProtectedResourceSession(const D3D12_PROTECTED_RESOURCE_SESSION_DESC* pDesc, REFIID riid, void** ppSession) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateProtectedResourceSession, tag_CreateProtectedResourceSession, riid, (ID3D12ProtectedResourceSession**)ppSession, pDesc);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreateCommittedResource1(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riidResource, void** ppvResource) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateCommittedResource1, tag_CreateCommittedResource1, riidResource, (ID3D12Resource**)ppvResource, pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreateHeap1(const D3D12_HEAP_DESC* pDesc, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riid, void** ppvHeap) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateHeap1, tag_CreateHeap1, riid, (ID3D12Heap**)ppvHeap, pDesc, pProtectedSession);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreateReservedResource1(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riid, void** ppvResource) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateReservedResource1, tag_CreateReservedResource1, riid, (ID3D12Resource**)ppvResource, pDesc, InitialState, pOptimizedClearValue, pProtectedSession);
}

//ID3D12Device5
HRESULT Wrap<ID3D12DeviceLatest>::CreateLifetimeTracker(ID3D12LifetimeOwner* pOwner, REFIID riid, void** ppvTracker) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateLifetimeTracker, tag_CreateLifetimeTracker, riid, (ID3D12LifetimeTracker**)ppvTracker, pOwner);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreateMetaCommand(REFGUID CommandId, UINT NodeMask, const void* pCreationParametersData, SIZE_T CreationParametersDataSizeInBytes, REFIID riid, void** ppMetaCommand) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateMetaCommand, tag_CreateMetaCommand, riid, (ID3D12MetaCommand**)ppMetaCommand, CommandId, NodeMask, pCreationParametersData, CreationParametersDataSizeInBytes);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreateStateObject(const D3D12_STATE_OBJECT_DESC* pDesc, REFIID riid, void** ppStateObject) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateStateObject, tag_CreateStateObject, riid, (ID3D12StateObject**)ppStateObject, pDesc);
}

//ID3D12Device6
HRESULT Wrap<ID3D12DeviceLatest>::SetBackgroundProcessingMode(D3D12_BACKGROUND_PROCESSING_MODE Mode, D3D12_MEASUREMENTS_ACTION MeasurementsAction, HANDLE hEventToSignalUponCompletion, BOOL* pbFurtherMeasurementsDesired) {
	return recording.RunRecord2(this, &ID3D12DeviceLatest::SetBackgroundProcessingMode, tag_SetBackgroundProcessingMode, Mode, MeasurementsAction, hEventToSignalUponCompletion, pbFurtherMeasurementsDesired);
}

//ID3D12Device7
HRESULT Wrap<ID3D12DeviceLatest>::AddToStateObject(const D3D12_STATE_OBJECT_DESC* pAddition, ID3D12StateObject* pStateObjectToGrowFrom, REFIID riid, void** ppNewStateObject) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::AddToStateObject, tag_AddToStateObject, riid, (ID3D12StateObject**)ppNewStateObject, pAddition, pStateObjectToGrowFrom);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreateProtectedResourceSession1(const D3D12_PROTECTED_RESOURCE_SESSION_DESC1* pDesc, REFIID riid, void** ppSession) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateProtectedResourceSession1, tag_CreateProtectedResourceSession1, riid, (ID3D12ProtectedResourceSession**)ppSession, pDesc);
}

//ID3D12Device8
HRESULT Wrap<ID3D12DeviceLatest>::CreateCommittedResource2(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riidResource, void** ppvResource) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreateCommittedResource2, tag_CreateCommittedResource2, riidResource, (ID3D12Resource**)ppvResource, pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession);
}
HRESULT Wrap<ID3D12DeviceLatest>::CreatePlacedResource1(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource) {
	return recording.RunRecord2Wrap(this, &ID3D12DeviceLatest::CreatePlacedResource1, tag_CreatePlacedResource1, riid, (ID3D12Resource**)ppvResource, pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue);
}
void Wrap<ID3D12DeviceLatest>::CreateSamplerFeedbackUnorderedAccessView(ID3D12Resource* pTargetedResource, ID3D12Resource* pFeedbackResource, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
	return recording.RunRecord2(this, &ID3D12DeviceLatest::CreateSamplerFeedbackUnorderedAccessView, tag_CreateSamplerFeedbackUnorderedAccessView, pTargetedResource, pFeedbackResource, DestDescriptor);
}

//-----------------------------------------------------------------------------
//	Global
//-----------------------------------------------------------------------------

ReWrapperT<ID3D12Resource>	rewrap_resource;

enum OBJECT_INFORMATION_CLASS {
	ObjectBasicInformation,
	ObjectNameInformation,
	ObjectTypeInformation,
	ObjectAllInformation,
	ObjectDataInformation
};

struct OBJECT_NAME_INFORMATION : NT::UNICODE_STRING {
	WCHAR	buffer[256];
};

typedef LONG T_NtQueryObject(
	HANDLE						Handle,
	OBJECT_INFORMATION_CLASS	bjectInformationClass,
	PVOID						ObjectInformation,
	ULONG						ObjectInformationLength,
	PULONG						ReturnLength
);

string16 GetHandleName(HANDLE h) {
	static T_NtQueryObject	*NtQueryObject = (T_NtQueryObject*)GetProcAddress(LoadLibraryA("ntdll.dll"), "NtQueryObject");

	OBJECT_NAME_INFORMATION	name;
	ULONG					size;
	NtQueryObject(h, ObjectNameInformation, &name, sizeof(name), &size);
	return str(name);
}

HRESULT WINAPI Hooked_D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** pp) {
	return com_wrap_system->make_wrap(get_orig(D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, pp), (ID3D12Device**)pp);
}

DWORD WINAPI Hooked_WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable) {
	if (capture.device && capture.device->recording.enable && capture.device->handles.count(hHandle))
		capture.device->recording.WithObject(hHandle).Record(RecDevice::tag_WaitForSingleObjectEx, dwMilliseconds, bAlertable);
	return get_orig(WaitForSingleObjectEx)(hHandle, dwMilliseconds, bAlertable);
}

DWORD WINAPI Hooked_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
	if (capture.device && capture.device->recording.enable && capture.device->handles.count(hHandle))
		capture.device->recording.WithObject(hHandle).Record(RecDevice::tag_WaitForSingleObjectEx, dwMilliseconds, FALSE);
	return get_orig(WaitForSingleObjectEx)(hHandle, dwMilliseconds, FALSE);
}

uint32 Capture::CaptureFrames(Wrap<ID3D12DeviceLatest> *device) {
	auto	num_objects	= num_elements32(device->objects) + num_elements32(RecObjectLink::orphans) + num_elements32(device->handles);

	com_ptr<ID3D12InfoQueue>	info_queue;
	if (SUCCEEDED(device->orig->QueryInterface(&info_queue))) {
		info_queue->SetBreakOnID(D3D12_MESSAGE_ID_OBJECT_ACCESSED_WHILE_STILL_IN_USE, false);
		info_queue->SetBreakOnID(D3D12_MESSAGE_ID_RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH, false);

	//	BOOL	x = info_queue-> GetBreakOnID(D3D12_MESSAGE_ID_OBJECT_ACCESSED_WHILE_STILL_IN_USE);
		info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);

		D3D12_MESSAGE_ID hide[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
		};
		D3D12_INFO_QUEUE_FILTER filter;
		clear(filter);
		filter.DenyList.NumIDs	= num_elements32(hide);
		filter.DenyList.pIDList = hide;
		info_queue->AddStorageFilterEntries(&filter);
	}
	return num_objects;
}
with_size<dynamic_array<RecObject2>> Capture::GetObjects() {
	dynamic_array<RecObject2>	r;
	for (auto &i : RecObjectLink::orphans) {
		auto	*obj	= static_cast<Wrap<ID3D12DeviceChild>*>(&i);
		r.emplace_back(i, obj, i.get_info());
	}

	for (auto &i : device->objects) {
		auto	*obj	= static_cast<Wrap<ID3D12DeviceChild>*>(&i);
		auto	&rec	= r.emplace_back(i, obj, i.get_info());
		if (obj->is_orig_dead())
			rec.type = RecObject::TYPE(rec.type | RecObject::DEAD);
	}

	for (auto &i : device->handles)
		r.emplace_back(i);

	return move(r);
}

malloc_block_all Capture::ResourceData(uintptr_t _res) {
	auto	res	= (Wrap<ID3D12Resource>*)_res;
	malloc_block	out;

	if (res->orig && res->device) {
		out.resize(res->data_size);

		bool			done	= false;
		if (res->alloc != RecResource::Reserved) {
			D3D12_HEAP_PROPERTIES	heap_props;
			D3D12_HEAP_FLAGS		heap_flags;
			res->GetHeapProperties(&heap_props, &heap_flags);

			if (heap_props.Type == D3D12_HEAP_TYPE_UPLOAD) {
				TransferResource(res, out);
				done = true;
			} else  if (heap_props.Type == D3D12_HEAP_TYPE_READBACK) {
				TransferResource(res, out);
				done = true;
			}
		}
		if (!done) {
			Transferer	trans(res->device->orig);
			uint64		total_size = 0;
			com_ptr<ID3D12Resource>	res2 = trans.Transfer(res->orig, res->state, &total_size);
			ISO_ASSERT(total_size == out.size());
			TransferResource(res2, out);
		}
	}
	return out;
}

malloc_block_all Capture::HeapData(uintptr_t _heap) {
	Wrap<ID3D12Heap> *heap	= (Wrap<ID3D12Heap>*)_heap;
	malloc_block	out;

	if (heap->orig && heap->device) {
		D3D12_HEAP_DESC	desc	= heap->GetDesc();
		uint64			size	= desc.SizeInBytes;
		out.resize(size);

		if (desc.Properties.Type == D3D12_HEAP_TYPE_UPLOAD || desc.Properties.Type == D3D12_HEAP_TYPE_READBACK) {

			D3D12_RESOURCE_DESC	rdesc;
			clear(rdesc);
			rdesc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
			rdesc.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			rdesc.Format			= DXGI_FORMAT_UNKNOWN;
			rdesc.Width				= size;
			rdesc.Height			= 1;
			rdesc.DepthOrArraySize	= 1;
			rdesc.MipLevels			= 1;
			rdesc.SampleDesc.Count	= 1;
			rdesc.Flags				= desc.Flags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER ? D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER : D3D12_RESOURCE_FLAG_NONE;

			com_ptr<ID3D12Resource>	buffer;
			if (SUCCEEDED(heap->device->orig->CreatePlacedResource(heap->orig, 0, &rdesc, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, __uuidof(ID3D12Resource), (void**)&buffer)))
				TransferResource(buffer, out);

		} else if (!(desc.Flags & D3D12_HEAP_FLAG_DENY_BUFFERS)) {
			Transferer	trans(heap->device->orig);
			D3D12_RESOURCE_DESC	rdesc;
			clear(rdesc);
			rdesc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
			rdesc.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			rdesc.Format			= DXGI_FORMAT_UNKNOWN;
			rdesc.Width				= size;
			rdesc.Height			= 1;
			rdesc.DepthOrArraySize	= 1;
			rdesc.MipLevels			= 1;
			rdesc.SampleDesc.Count	= 1;
			rdesc.Flags				= desc.Flags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER ? D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER : D3D12_RESOURCE_FLAG_NONE;

			com_ptr<ID3D12Resource>	buffer;
			if (SUCCEEDED(heap->device->orig->CreatePlacedResource(heap->orig, 0, &rdesc, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, __uuidof(ID3D12Resource), (void**)&buffer))) {
				uint64	total_size		= 0;
				com_ptr<ID3D12Resource>	res2 = trans.Transfer(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, &total_size);
				ISO_ASSERT(total_size == size);
				TransferResource(res2, out);
			}

		} else {
			Transferer	trans(heap->device->orig);

			uint64	offset = 0;
			while (size) {
				uint64				pixels	= size / 16;
				//uint64			width	= min(isqrt(pixels), D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
				uint64				width	= min(pixels, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
				uint32				array	= 1;//min(pixels / width, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
				//uint64	rsize			= min(size, 1 << 27);
				uint64		rsize			= width * array * 16;

				D3D12_RESOURCE_DESC	rdesc;
				clear(rdesc);
				rdesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE1D;
//				rdesc.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				rdesc.Format			= DXGI_FORMAT_R32G32B32A32_TYPELESS;
				rdesc.Width				= width;
				rdesc.Height			= 1;
				rdesc.DepthOrArraySize	= array;
				rdesc.MipLevels			= 1;
				rdesc.SampleDesc.Count	= 1;
				//rdesc.Flags				= desc.Flags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER ? D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER : D3D12_RESOURCE_FLAG_NONE;

				com_ptr<ID3D12Resource>	buffer;
				if (FAILED(heap->device->orig->CreatePlacedResource(heap->orig, offset, &rdesc, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, __uuidof(ID3D12Resource), (void**)&buffer)))
					break;

				uint64	total_size		= 0;
				com_ptr<ID3D12Resource>	res2 = trans.Transfer(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, &total_size);
				ISO_ASSERT(total_size == rsize);
				TransferResource(res2, out.slice(offset, rsize));

				offset	+= rsize;
				size	-= rsize;
			}
		}
	}
	return out;
}
