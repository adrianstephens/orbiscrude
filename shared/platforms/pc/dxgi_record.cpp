#include "dxgi_record.h"
//#include <d3d12.h>

using namespace iso;

PresentDevice	*iso::last_present_device;

//-----------------------------------------------------------------------------
//	Wrap<IDXGIAdapterLatest>
//-----------------------------------------------------------------------------
//IDXGIAdapter
HRESULT Wrap<IDXGIAdapterLatest>::EnumOutputs(UINT Output, IDXGIOutput **ppOutput) {
	return orig->EnumOutputs(Output, ppOutput);
}
HRESULT Wrap<IDXGIAdapterLatest>::GetDesc(DXGI_ADAPTER_DESC *pDesc) {
	return orig->GetDesc(pDesc);
}
HRESULT Wrap<IDXGIAdapterLatest>::CheckInterfaceSupport(REFGUID InterfaceName, LARGE_INTEGER *pUMDVersion) {
	return orig->CheckInterfaceSupport(InterfaceName, pUMDVersion);
}
//IDXGIAdapter1
HRESULT Wrap<IDXGIAdapterLatest>::GetDesc1(DXGI_ADAPTER_DESC1 *pDesc) {
	return orig->GetDesc1(pDesc);
}
//IDXGIAdapter2
HRESULT Wrap<IDXGIAdapterLatest>::GetDesc2(DXGI_ADAPTER_DESC2 *pDesc) {
	return orig->GetDesc2(pDesc);
}
//IDXGIAdapter3
HRESULT Wrap<IDXGIAdapterLatest>::RegisterHardwareContentProtectionTeardownStatusEvent(HANDLE hEvent, DWORD *pdwCookie) {
	return orig->RegisterHardwareContentProtectionTeardownStatusEvent(hEvent, pdwCookie);
}
void	Wrap<IDXGIAdapterLatest>::UnregisterHardwareContentProtectionTeardownStatus(DWORD dwCookie) {
	return orig->UnregisterHardwareContentProtectionTeardownStatus(dwCookie);
}
HRESULT Wrap<IDXGIAdapterLatest>::QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo) {
		return orig->QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
}
HRESULT Wrap<IDXGIAdapterLatest>::SetVideoMemoryReservation(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, UINT64 Reservation) {
	return orig->SetVideoMemoryReservation( NodeIndex, MemorySegmentGroup, Reservation);
}
HRESULT Wrap<IDXGIAdapterLatest>::RegisterVideoMemoryBudgetChangeNotificationEvent(HANDLE hEvent, DWORD *pdwCookie) {
	return orig->RegisterVideoMemoryBudgetChangeNotificationEvent(hEvent, pdwCookie);
}
void	Wrap<IDXGIAdapterLatest>::UnregisterVideoMemoryBudgetChangeNotification(DWORD dwCookie) {
	return orig->UnregisterVideoMemoryBudgetChangeNotification(dwCookie);
}
//IDXGIAdapter4
HRESULT Wrap<IDXGIAdapterLatest>::GetDesc3(DXGI_ADAPTER_DESC3 *pDesc) {
	return orig->GetDesc3(pDesc);
}

//-----------------------------------------------------------------------------
//	Wrap<IDXGISwapChainLatest>
//-----------------------------------------------------------------------------
//IDXGISwapChain
HRESULT Wrap<IDXGISwapChainLatest>::Present(UINT SyncInterval, UINT Flags) {
	HRESULT	hr = orig->Present(SyncInterval, Flags);
	if (device)
		device->Present();
	return hr;
}
HRESULT Wrap<IDXGISwapChainLatest>::GetBuffer(UINT Buffer, REFIID riid, void **pp) {
	HRESULT	h = orig->GetBuffer(Buffer, riid, pp);
	IUnknown	*p = (IUnknown*)*pp;
	for (auto &&i = ReWrapper::begin(); i; ++i) {
		if (void *r = (*i)(riid, p)) {
			*pp = r;
			//p->Release();
			return S_OK;
		}
	}
	if (h == S_OK)
		*pp = static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(com_wrap_system->get_wrap(riid, *pp)));
	return h;
}
HRESULT Wrap<IDXGISwapChainLatest>::SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget) {
	return orig->SetFullscreenState(Fullscreen, pTarget);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **pp) {
	return orig->GetFullscreenState(pFullscreen, pp);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetDesc(DXGI_SWAP_CHAIN_DESC *desc) {
	return orig->GetDesc(desc);
}
HRESULT Wrap<IDXGISwapChainLatest>::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
	return orig->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
}
HRESULT Wrap<IDXGISwapChainLatest>::ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters) {
	return orig->ResizeTarget(pNewTargetParameters);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetContainingOutput(IDXGIOutput **pp) {
	return orig->GetContainingOutput(pp);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) {
	return orig->GetFrameStatistics(pStats);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetLastPresentCount(UINT *pLastPresentCount) {
	return orig->GetLastPresentCount(pLastPresentCount);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetDevice(REFIID riid, void **pp) {
	return orig->GetDevice(riid, pp);
}
//IDXGISwapChain1
HRESULT	Wrap<IDXGISwapChainLatest>::GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) {
	return orig->GetDesc1(pDesc);
}
HRESULT	Wrap<IDXGISwapChainLatest>::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) {
	return orig->GetFullscreenDesc(pDesc);
}
HRESULT	Wrap<IDXGISwapChainLatest>::GetHwnd(HWND *pHwnd) {
	return orig->GetHwnd(pHwnd);
}
HRESULT	Wrap<IDXGISwapChainLatest>::GetCoreWindow(REFIID refiid, void **ppUnk) {
	return orig->GetCoreWindow(refiid, ppUnk);
}
HRESULT	Wrap<IDXGISwapChainLatest>::Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters) {
	device->Present();
	return orig->Present1(SyncInterval, PresentFlags, pPresentParameters);
}
BOOL	Wrap<IDXGISwapChainLatest>::IsTemporaryMonoSupported() {
	return orig->IsTemporaryMonoSupported();
}
HRESULT	Wrap<IDXGISwapChainLatest>::GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) {
	return orig->GetRestrictToOutput(ppRestrictToOutput);
}
HRESULT	Wrap<IDXGISwapChainLatest>::SetBackgroundColor(const DXGI_RGBA *pColor) {
	return orig->SetBackgroundColor(pColor);
}
HRESULT	Wrap<IDXGISwapChainLatest>::GetBackgroundColor(DXGI_RGBA *pColor) {
	return orig->GetBackgroundColor(pColor);
}
HRESULT	Wrap<IDXGISwapChainLatest>::SetRotation(DXGI_MODE_ROTATION Rotation) {
	return orig->SetRotation(Rotation);
}
HRESULT	Wrap<IDXGISwapChainLatest>::GetRotation(DXGI_MODE_ROTATION *pRotation) {
	return orig->GetRotation(pRotation);
}
//IDXGISwapChain2
HRESULT Wrap<IDXGISwapChainLatest>::SetSourceSize(UINT Width, UINT Height) {
	return orig->SetSourceSize(Width, Height);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetSourceSize(UINT *pWidth, UINT *pHeight) {
	return orig->GetSourceSize(pWidth, pHeight);
}
HRESULT Wrap<IDXGISwapChainLatest>::SetMaximumFrameLatency(UINT MaxLatency) {
	return orig->SetMaximumFrameLatency(MaxLatency);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetMaximumFrameLatency(UINT *pMaxLatency) {
	return orig->GetMaximumFrameLatency(pMaxLatency);
}
HANDLE	Wrap<IDXGISwapChainLatest>::GetFrameLatencyWaitableObject() {
	return orig->GetFrameLatencyWaitableObject();
}
HRESULT Wrap<IDXGISwapChainLatest>::SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix) {
	return orig->SetMatrixTransform(pMatrix);
}
HRESULT Wrap<IDXGISwapChainLatest>::GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix) {
	return orig->GetMatrixTransform(pMatrix);
}
//IDXGISwapChain3
UINT	Wrap<IDXGISwapChainLatest>::GetCurrentBackBufferIndex() {
	return orig->GetCurrentBackBufferIndex();
}
HRESULT Wrap<IDXGISwapChainLatest>::CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport) {
	return orig->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
}
HRESULT Wrap<IDXGISwapChainLatest>::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) {
	return orig->SetColorSpace1(ColorSpace);
}
HRESULT Wrap<IDXGISwapChainLatest>::ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue) {
	return orig->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);
}
//-----------------------------------------------------------------------------
//	Wrap<IDXGIDeviceLatest>
//-----------------------------------------------------------------------------
//IDXGIDevice
HRESULT Wrap<IDXGIDeviceLatest>::GetAdapter(IDXGIAdapter **pAdapter) {
	return com_wrap_system->rewrap_carefully(orig->GetAdapter(pAdapter), pAdapter);
//	return orig->GetAdapter(pAdapter);
}
HRESULT Wrap<IDXGIDeviceLatest>::CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface) {
	return orig->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
}
HRESULT Wrap<IDXGIDeviceLatest>::QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources) {
	return orig->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
}
HRESULT Wrap<IDXGIDeviceLatest>::SetGPUThreadPriority(INT Priority) {
	return orig->SetGPUThreadPriority(Priority);
}
HRESULT Wrap<IDXGIDeviceLatest>::GetGPUThreadPriority(INT *pPriority) {
	return orig->GetGPUThreadPriority(pPriority);
}
//IDXGIDevice1
HRESULT Wrap<IDXGIDeviceLatest>::SetMaximumFrameLatency(UINT MaxLatency) {
	return orig->SetMaximumFrameLatency(MaxLatency);
}
HRESULT Wrap<IDXGIDeviceLatest>::GetMaximumFrameLatency(UINT *pMaxLatency) {
	return orig->GetMaximumFrameLatency(pMaxLatency);
}
//IDXGIDevice2
HRESULT Wrap<IDXGIDeviceLatest>::OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority) {
	return orig->OfferResources(NumResources, ppResources, Priority);
}
HRESULT Wrap<IDXGIDeviceLatest>::ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded) {
	return orig->ReclaimResources(NumResources, ppResources, pDiscarded);
}
HRESULT Wrap<IDXGIDeviceLatest>::EnqueueSetEvent(HANDLE hEvent) {
	return orig->EnqueueSetEvent(hEvent);
}
//IDXGIDevice3
void Wrap<IDXGIDeviceLatest>::Trim() {
	orig->Trim();
}
//IDXGIDevice4
HRESULT Wrap<IDXGIDeviceLatest>::OfferResources1(UINT NumResources, IDXGIResource* const* ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority, UINT Flags) {
	return orig->OfferResources1(NumResources, ppResources, Priority, Flags);
}
HRESULT Wrap<IDXGIDeviceLatest>::ReclaimResources1(UINT NumResources, IDXGIResource* const* ppResources, DXGI_RECLAIM_RESOURCE_RESULTS* pResults) {
	return orig->ReclaimResources1(NumResources, ppResources, pResults);

}

//-----------------------------------------------------------------------------
//	Wrap<IDXGIFactoryLatest>
//-----------------------------------------------------------------------------
//IDXGIFactory
HRESULT Wrap<IDXGIFactoryLatest>::EnumAdapters(UINT Adapter, IDXGIAdapter **pp) {
	return orig->EnumAdapters(Adapter, pp);
}
HRESULT Wrap<IDXGIFactoryLatest>::MakeWindowAssociation(HWND WindowHandle, UINT Flags) {
	return orig->MakeWindowAssociation(WindowHandle, Flags);
}
HRESULT Wrap<IDXGIFactoryLatest>::GetWindowAssociation(HWND *pWindowHandle) {
	return orig->GetWindowAssociation(pWindowHandle);
}
HRESULT Wrap<IDXGIFactoryLatest>::CreateSwapChain(IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **pp) {
	if (auto *device = com_wrap_system->unwrap_test(pDevice))
		return com_wrap_system->make_wrap_qi<IDXGISwapChainLatest>(orig->CreateSwapChain(device, desc, pp), pp, pDevice);
	return orig->CreateSwapChain(pDevice, desc, pp);
}
HRESULT Wrap<IDXGIFactoryLatest>::CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter **pp) {
	return orig->CreateSoftwareAdapter(Module, pp);
}
//IDXGIFactory1
HRESULT Wrap<IDXGIFactoryLatest>::EnumAdapters1(UINT Adapter, IDXGIAdapter1 **ppAdapter) {
	return orig->EnumAdapters1(Adapter, ppAdapter);
}
BOOL	Wrap<IDXGIFactoryLatest>::IsCurrent() {
	return orig->IsCurrent();
}
//IDXGIFactory2
BOOL	Wrap<IDXGIFactoryLatest>::IsWindowedStereoEnabled() {
	return orig->IsWindowedStereoEnabled();
}
HRESULT Wrap<IDXGIFactoryLatest>::CreateSwapChainForHwnd(IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) {
	if (auto *device = com_wrap_system->unwrap_test(pDevice))
		return com_wrap_system->make_wrap_qi<IDXGISwapChainLatest>(orig->CreateSwapChainForHwnd(device, hWnd, desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain), ppSwapChain, pDevice);
	return orig->CreateSwapChainForHwnd(pDevice, hWnd, desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
}
HRESULT Wrap<IDXGIFactoryLatest>::CreateSwapChainForCoreWindow(IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *desc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) {
	if (auto *device = com_wrap_system->unwrap_test(pDevice))
		return com_wrap_system->make_wrap_qi<IDXGISwapChainLatest>(orig->CreateSwapChainForCoreWindow(device, pWindow, desc, pRestrictToOutput, ppSwapChain), ppSwapChain, pDevice);
	return orig->CreateSwapChainForCoreWindow(pDevice, pWindow, desc, pRestrictToOutput, ppSwapChain);
}
HRESULT Wrap<IDXGIFactoryLatest>::GetSharedResourceAdapterLuid(HANDLE hResource, LUID *pLuid) {
	return orig->GetSharedResourceAdapterLuid(hResource, pLuid);
}
HRESULT Wrap<IDXGIFactoryLatest>::RegisterStereoStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie) {
	return orig->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT Wrap<IDXGIFactoryLatest>::RegisterStereoStatusEvent(HANDLE hEvent, DWORD *pdwCookie) {
	return orig->RegisterStereoStatusEvent(hEvent, pdwCookie);
}
void	Wrap<IDXGIFactoryLatest>::UnregisterStereoStatus(DWORD dwCookie) {
	return orig->UnregisterStereoStatus(dwCookie);
}
HRESULT Wrap<IDXGIFactoryLatest>::RegisterOcclusionStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie) {
	return orig->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT Wrap<IDXGIFactoryLatest>::RegisterOcclusionStatusEvent(HANDLE hEvent, DWORD *pdwCookie) {
	return orig->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
}
void	Wrap<IDXGIFactoryLatest>::UnregisterOcclusionStatus(DWORD dwCookie) {
	return orig->UnregisterOcclusionStatus(dwCookie);
}
HRESULT Wrap<IDXGIFactoryLatest>::CreateSwapChainForComposition(IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *desc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) {
	if (auto *device = com_wrap_system->unwrap_test(pDevice))
		return com_wrap_system->make_wrap_qi<IDXGISwapChainLatest>(orig->CreateSwapChainForComposition(device, desc, pRestrictToOutput, ppSwapChain), ppSwapChain, pDevice);
	return orig->CreateSwapChainForComposition(pDevice, desc, pRestrictToOutput, ppSwapChain);
}
//IDXGIFactory3
UINT	Wrap<IDXGIFactoryLatest>::GetCreationFlags() {
	return orig->GetCreationFlags();
}
//IDXGIFactory4
HRESULT Wrap<IDXGIFactoryLatest>::EnumAdapterByLuid(LUID AdapterLuid, REFIID riid, void **ppvAdapter) {
	return orig->EnumAdapterByLuid(AdapterLuid, riid, ppvAdapter);
}
HRESULT Wrap<IDXGIFactoryLatest>::EnumWarpAdapter(REFIID riid, void **ppvAdapter) {
	return orig->EnumWarpAdapter(riid, ppvAdapter);
}

//IDXGIFactory5
HRESULT Wrap<IDXGIFactoryLatest>::CheckFeatureSupport(DXGI_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) {
	return orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

//IDXGIFactory6
HRESULT Wrap<IDXGIFactoryLatest>::EnumAdapterByGpuPreference(UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void** ppvAdapter) {
	return com_wrap_system->get_wrap_qi<IDXGIAdapterLatest>(orig->EnumAdapterByGpuPreference(Adapter, GpuPreference, riid, ppvAdapter), (IDXGIAdapter**)ppvAdapter);
}
//IDXGIFactory7
HRESULT Wrap<IDXGIFactoryLatest>::RegisterAdaptersChangedEvent(HANDLE hEvent, DWORD* pdwCookie) {
	return orig->RegisterAdaptersChangedEvent(hEvent, pdwCookie);
}
HRESULT Wrap<IDXGIFactoryLatest>::UnregisterAdaptersChangedEvent(DWORD dwCookie) {
	return orig->UnregisterAdaptersChangedEvent(dwCookie);
}

//-----------------------------------------------------------------------------
//	Global
//-----------------------------------------------------------------------------

bool check_dxgi_interfaces(REFIID riid, void **pp) {
	if (is_any(riid, uuidof<IDXGIDevice3>(), uuidof<IDXGIDevice2>(), uuidof<IDXGIDevice1>(), uuidof<IDXGIDevice>())) {
		com_wrap_system->rewrap_carefully((IDXGIDeviceLatest**)pp);
		return true;
	}
	if (is_any(riid, uuidof<IDXGIFactory7>(), uuidof<IDXGIFactory6>(), uuidof<IDXGIFactory5>(), uuidof<IDXGIFactory4>(), uuidof<IDXGIFactory3>(), uuidof<IDXGIFactory2>(), uuidof<IDXGIFactory1>(), uuidof<IDXGIFactory>())) {
		com_wrap_system->rewrap_carefully((IDXGIFactoryLatest**)pp);
		return true;
	}
	return false;
}

HRESULT WINAPI Hooked_CreateDXGIFactory(REFIID riid, void **ppFactory) {
	return com_wrap_system->make_wrap_qi<IDXGIFactoryLatest>(get_orig(CreateDXGIFactory)(riid, ppFactory), (IDXGIFactory**)ppFactory);
}

HRESULT WINAPI Hooked_CreateDXGIFactory1(REFIID riid, void **ppFactory) {
	return com_wrap_system->make_wrap_qi<IDXGIFactoryLatest>(get_orig(CreateDXGIFactory1)(riid, ppFactory), (IDXGIFactory1**)ppFactory);
}

HRESULT WINAPI Hooked_CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory) {
	return com_wrap_system->make_wrap_qi<IDXGIFactoryLatest>(get_orig(CreateDXGIFactory2)(flags, riid, ppFactory), (IDXGIFactory2**)ppFactory);
}

void HookDXGI() {
	hook(CreateDXGIFactory,  "dxgi.dll");
	hook(CreateDXGIFactory1, "dxgi.dll");
	hook(CreateDXGIFactory2, "dxgi.dll");
}
