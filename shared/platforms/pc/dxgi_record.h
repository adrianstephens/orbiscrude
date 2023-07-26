#ifndef DXGI_RECORD_H
#define DXGI_RECORD_H

#include "hook_com.h"
#include <dxgi1_6.h>

bool check_dxgi_interfaces(REFIID riid, void **pp);

namespace iso {

struct __declspec(uuid("290ff5cb-3a21-4740-bfda-2697ca13deae")) PresentDevice : com<IUnknown> {
	virtual void	Present() = 0;
};

extern PresentDevice	*last_present_device;

typedef IDXGIAdapter4	IDXGIAdapterLatest;
typedef IDXGISwapChain3	IDXGISwapChainLatest;
typedef IDXGIDevice4	IDXGIDeviceLatest;
typedef IDXGIFactory7	IDXGIFactoryLatest;

template<> struct	Wrappable<IDXGIAdapter		> : T_type<IDXGIAdapterLatest	> {};
template<> struct	Wrappable<IDXGIAdapter1		> : T_type<IDXGIAdapterLatest	> {};
template<> struct	Wrappable<IDXGIAdapter2		> : T_type<IDXGIAdapterLatest	> {};
template<> struct	Wrappable<IDXGIAdapter3		> : T_type<IDXGIAdapterLatest	> {};
template<> struct	Wrappable<IDXGIAdapter4		> : T_type<IDXGIAdapterLatest	> {};

template<> struct	Wrappable<IDXGISwapChain	> : T_type<IDXGISwapChainLatest	> {};
template<> struct	Wrappable<IDXGISwapChain1	> : T_type<IDXGISwapChainLatest	> {};
template<> struct	Wrappable<IDXGISwapChain2	> : T_type<IDXGISwapChainLatest	> {};
template<> struct	Wrappable<IDXGISwapChain3	> : T_type<IDXGISwapChainLatest	> {};

template<> struct	Wrappable<IDXGIDevice		> : T_type<IDXGIDeviceLatest	> {};
template<> struct	Wrappable<IDXGIDevice1		> : T_type<IDXGIDeviceLatest	> {};
template<> struct	Wrappable<IDXGIDevice2		> : T_type<IDXGIDeviceLatest	> {};
template<> struct	Wrappable<IDXGIDevice3		> : T_type<IDXGIDeviceLatest	> {};

template<> struct	Wrappable<IDXGIFactory		> : T_type<IDXGIFactoryLatest	> {};
template<> struct	Wrappable<IDXGIFactory1		> : T_type<IDXGIFactoryLatest	> {};
template<> struct	Wrappable<IDXGIFactory2		> : T_type<IDXGIFactoryLatest	> {};
template<> struct	Wrappable<IDXGIFactory3		> : T_type<IDXGIFactoryLatest	> {};
template<> struct	Wrappable<IDXGIFactory4		> : T_type<IDXGIFactoryLatest	> {};
template<> struct	Wrappable<IDXGIFactory5		> : T_type<IDXGIFactoryLatest	> {};
template<> struct	Wrappable<IDXGIFactory6		> : T_type<IDXGIFactoryLatest	> {};
template<> struct	Wrappable<IDXGIFactory7		> : T_type<IDXGIFactoryLatest	> {};

//-----------------------------------------------------------------------------
//	Wraps
//-----------------------------------------------------------------------------

template<class T> class Wrap2<T, IDXGIObject> : public com_wrap<T> {
public:
	//IUnknown
	HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID riid, void **ppv) {
		return check_interface<IDXGIObject>(this, riid, ppv) ? S_OK : com_wrap<T>::QueryInterface(riid, ppv);
	}
	//IDXGIObject
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)	{ return orig->SetPrivateData(Name, DataSize, pData); }
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)	{ return orig->SetPrivateDataInterface(Name, pUnknown); }
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)		{ return orig->GetPrivateData(Name, pDataSize, pData); }
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **pp)									{
		HRESULT	h = orig->GetParent(riid, pp);
		check_dxgi_interfaces(riid, pp);
		return h;
	}
};

template<> class Wrap<IDXGIAdapterLatest> : public Wrap2<IDXGIAdapterLatest, IDXGIObject> {
	//IDXGIAdapter
	HRESULT STDMETHODCALLTYPE EnumOutputs(UINT Output, IDXGIOutput **ppOutput);
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC *pDesc);
	HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(REFGUID InterfaceName, LARGE_INTEGER *pUMDVersion);
	//IDXGIAdapter1
	HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1 *pDesc);
	//IDXGIAdapter2
	HRESULT STDMETHODCALLTYPE GetDesc2(DXGI_ADAPTER_DESC2 *pDesc);
	//IDXGIAdapter3
	HRESULT STDMETHODCALLTYPE RegisterHardwareContentProtectionTeardownStatusEvent(HANDLE hEvent, DWORD *pdwCookie);
	void	STDMETHODCALLTYPE UnregisterHardwareContentProtectionTeardownStatus(DWORD dwCookie);
	HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo);
	HRESULT STDMETHODCALLTYPE SetVideoMemoryReservation(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, UINT64 Reservation);
	HRESULT STDMETHODCALLTYPE RegisterVideoMemoryBudgetChangeNotificationEvent(HANDLE hEvent, DWORD *pdwCookie);
	void	STDMETHODCALLTYPE UnregisterVideoMemoryBudgetChangeNotification(DWORD dwCookie);
	//IDXGIAdapter4
	HRESULT STDMETHODCALLTYPE GetDesc3(DXGI_ADAPTER_DESC3 *pDesc);
};

template<> class Wrap<IDXGISwapChainLatest> : public Wrap2<IDXGISwapChainLatest, IDXGIObject> {
	PresentDevice	*device;
public:
	//IDXGIDeviceSubObject
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **pp);
	//IDXGISwapChain
	HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags);
	HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void **pp);
	HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget);
	HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **pp);
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *desc);
	HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
	HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters);
	HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **pp);
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats);
	HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount);
	//IDXGISwapChain1
	HRESULT	STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc);
	HRESULT	STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc);
	HRESULT	STDMETHODCALLTYPE GetHwnd(HWND *pHwnd);
	HRESULT	STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void **ppUnk);
	HRESULT	STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters);
	BOOL	STDMETHODCALLTYPE IsTemporaryMonoSupported();
	HRESULT	STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput);
	HRESULT	STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *pColor);
	HRESULT	STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *pColor);
	HRESULT	STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation);
	HRESULT	STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *pRotation);
	//IDXGISwapChain2
	HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height);
	HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *pWidth, UINT *pHeight);
	HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency);
	HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency);
	HANDLE	STDMETHODCALLTYPE GetFrameLatencyWaitableObject();
	HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix);
	HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix);
	//IDXGISwapChain3
	UINT	STDMETHODCALLTYPE GetCurrentBackBufferIndex();
	HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport);
	HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace);
	HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue);

	void	init(IUnknown *pDevice) {
		device = query<PresentDevice>(pDevice).detach();
		if (!device)
			device = last_present_device;
	}
};

template<> class Wrap<IDXGIDeviceLatest> : public Wrap2<IDXGIDeviceLatest, IDXGIObject> {
public:
	//IDXGIDevice
	HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter **pAdapter);
	HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface);
	HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources);
	HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority);
	HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *pPriority);
	//IDXGIDevice1
	HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency);
	HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency);
	//IDXGIDevice2
	HRESULT STDMETHODCALLTYPE OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority);
	HRESULT STDMETHODCALLTYPE ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded);
	HRESULT STDMETHODCALLTYPE EnqueueSetEvent(HANDLE hEvent);
	//IDXGIDevice3
	void	STDMETHODCALLTYPE Trim();
	//IDXGIDevice4
	HRESULT STDMETHODCALLTYPE OfferResources1(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority, UINT Flags);
	HRESULT STDMETHODCALLTYPE ReclaimResources1(UINT NumResources, IDXGIResource *const *ppResources, DXGI_RECLAIM_RESOURCE_RESULTS *pResults);
};

template<> class Wrap<IDXGIFactoryLatest> : public Wrap2<IDXGIFactoryLatest, IDXGIObject> {
public:
	//IDXGIFactory
	HRESULT STDMETHODCALLTYPE EnumAdapters(UINT Adapter, IDXGIAdapter **pp);
	HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND WindowHandle, UINT Flags);
	HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND *pWindowHandle);
	HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **pp);
	HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter **pp);
	//IDXGIFactory1
	HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT Adapter, IDXGIAdapter1 **ppAdapter);
	BOOL	STDMETHODCALLTYPE IsCurrent();
	//IDXGIFactory2
	BOOL	STDMETHODCALLTYPE IsWindowedStereoEnabled();
	HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain);
	HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *desc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain);
	HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(HANDLE hResource, LUID *pLuid);
	HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie);
	HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(HANDLE hEvent, DWORD *pdwCookie);
	void	STDMETHODCALLTYPE UnregisterStereoStatus(DWORD dwCookie);
	HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie);
	HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(HANDLE hEvent, DWORD *pdwCookie);
	void	STDMETHODCALLTYPE UnregisterOcclusionStatus(DWORD dwCookie);
	HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *desc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain);
	//IDXGIFactory3
    UINT	STDMETHODCALLTYPE GetCreationFlags();
	//IDXGIFactory4
	HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(LUID AdapterLuid, REFIID riid, void **ppvAdapter);
	HRESULT STDMETHODCALLTYPE EnumWarpAdapter(REFIID riid, void **ppvAdapter);
	//IDXGIFactory5
	HRESULT STDMETHODCALLTYPE CheckFeatureSupport(DXGI_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize);
	//IDXGIFactory6
	HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void **ppvAdapter);
	//IDXGIFactory7
	HRESULT STDMETHODCALLTYPE RegisterAdaptersChangedEvent(HANDLE hEvent, DWORD *pdwCookie);
	HRESULT STDMETHODCALLTYPE UnregisterAdaptersChangedEvent(DWORD dwCookie);
};


} // namespace iso

  //-----------------------------------------------------------------------------
//	Global
//-----------------------------------------------------------------------------

void HookDXGI();

#endif // DXGI_RECORD_H

