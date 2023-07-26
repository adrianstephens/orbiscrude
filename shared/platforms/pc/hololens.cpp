#include "winrt/base.h"
#include "winrt/Windows.ApplicationModel.Activation.h"
#include "winrt/Windows.ApplicationModel.Core.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.Metadata.h"
#include "winrt/Windows.Gaming.Input.h"
#include "winrt/Windows.Graphics.Display.h"
#include "winrt/Windows.Graphics.Holographic.h"
#include "winrt/Windows.Graphics.DirectX.Direct3D11.h"
#include "winrt/Windows.Perception.People.h"
#include "winrt/Windows.Perception.Spatial.h"
#include "winrt/Windows.Storage.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/Windows.UI.Core.h"
#include "winrt/Windows.UI.Input.Spatial.h"
#include "winrt/Windows.UI.ViewManagement.h"

#include "app.h"
#include "utilities.h"
#include "windows/d2d.h"
#include "dx/dxgi_helpers.h"
#include "base/coroutine.h"

//#include <Windows.Graphics.DirectX.Direct3D11.Interop.h>
//copied from Windows.Graphics.DirectX.Direct3D11.Interop.h to avoid the namespace problems
STDAPI CreateDirect3D11DeviceFromDXGIDevice(
	_In_         IDXGIDevice* dxgiDevice,
	_COM_Outptr_ IInspectable** graphicsDevice);

STDAPI CreateDirect3D11SurfaceFromDXGISurface(
	_In_         IDXGISurface* dgxiSurface,
	_COM_Outptr_ IInspectable** graphicsSurface);

struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))	IDirect3DDxgiInterfaceAccess : public IUnknown {
	IFACEMETHOD(GetInterface)(REFIID iid, _COM_Outptr_ void** p) = 0;
};

using namespace iso;
using namespace iso_winrt;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;

#ifdef PLAT_WIN32
#include <HolographicSpaceInterop.h>
template<> struct uuid<IHolographicSpaceInterop>			{ define_guid(0x5C4EE536,0x6A98,0x4B86,0xA1,0x70,0x58,0x70,0x13,0xD6,0xFD,0x4B); };
#endif


auto operator*(const Platform::Numerics::Matrix4x4 &a, const Platform::Numerics::Matrix4x4 &b) {
	return (const float4x4&)a * (const float4x4&)b;
}

point	get(const Platform::Size &s) { return {s.Width, s.Height}; }
rect	get(const Platform::Rect &r) { return {point{r.X, r.Y}, point{r.Width, r.Height}}; }

static float4 get_fov(const float4x4 &proj) {
	float	sx = rlen(proj.x.xyw), sy = rlen(proj.y.xyw);

	return {
		(one + proj.z.x) * sx,
		(one + proj.z.y) * sy,
		(one - proj.z.x) * sx,
		(one - proj.z.y) * sy,
	};
}

// Manages DirectX device resources that are specific to a holographic camera, such as the back buffer, ViewProjection constant buffer, and viewport

struct CameraResources {
	ptr<HolographicCamera> holographic_camera;

	Surface		back_buffer;
	Texture		depth_buffer;

	DXGI_FORMAT	format;
	point		size;
	rect		viewport;

	bool		stereo;

	float3x4	eye_view[2];
	float4x4	eye_proj[2];

	CameraResources(HolographicCamera *camera) : holographic_camera(camera), size(get(camera->RenderTargetSize())), viewport(zero, size), stereo(camera->IsStereo) {}

	void CreateResourcesForBackBuffer(ID3D11Device* device, HolographicCameraRenderingParameters *cameraParameters, bool depth);
	bool UpdateViewProjectionBuffer(GraphicsContext &ctx, HolographicCameraPose *cameraPose, SpatialCoordinateSystem *coordinateSystem);

	RenderView	GetView(int i) const {
		RenderView	v;
		v.window	= viewport;
		v.offset	= eye_view[i];
		v.fov		= get_fov(eye_proj[i]);
		v.display	= back_buffer;
		v.depth		= depth_buffer;
		return v;
	}
};

//-----------------------------------------------------------------------------
//	CameraResources
//-----------------------------------------------------------------------------

// Updates resources associated with a holographic camera's swap chain.
// The app does not access the swap chain directly, but it does create resource views for the back buffer.
void CameraResources::CreateResourcesForBackBuffer(ID3D11Device* device, HolographicCameraRenderingParameters *cameraParameters, bool depth) {

	// Get the holographic camera's back buffer.
	// Holographic apps do not create a swap chain themselves; instead, buffers are owned by the system. The Direct3D back buffer resources are provided to the app using WinRT interop APIs.
#ifdef USE_DX12
	com_ptr<ID3D12Resource> cameraBackBuffer;
#else
	com_ptr<ID3D11Resource> cameraBackBuffer;
#endif
	com_cast<IDirect3DDxgiInterfaceAccess>(cameraParameters->Direct3D11BackBuffer().get())->GetInterface(COM_CREATE(&cameraBackBuffer));
	//com_ptr<ID3D11Resource> cameraBackBuffer = com_cast<ID3D11Resource>(cameraParameters->Direct3D11BackBuffer().get());

	// Determine if the back buffer has changed. If so, ensure that the render target view is for the current back buffer.
	if (back_buffer != cameraBackBuffer) {
		// This can change every frame as the system moves to the next buffer in the swap chain. This mode of operation will occur when certain rendering modes are activated.
		back_buffer = Surface(cameraBackBuffer);

		// Check for render target size changes.
		point	currentSize = get(holographic_camera->RenderTargetSize());
		if (any(size != currentSize)) {
			size = currentSize;
			depth_buffer.DeInit();
		}
	}

	// Refresh depth stencil resources, if needed.
	if (depth && !depth_buffer)
		depth_buffer = TextureT<uint16>(size.x, size.y, stereo ? 2 : 1, 1, MEM_DEPTH);
}

// Updates the view/projection constant buffer for a holographic camera.
bool CameraResources::UpdateViewProjectionBuffer(GraphicsContext &ctx, HolographicCameraPose *cameraPose, SpatialCoordinateSystem *coordinateSystem) {
	// The system changes the viewport on a per-frame basis for system optimizations.
	viewport = get(cameraPose->Viewport());

	// The projection transform for each frame is provided by the HolographicCameraPose.
	HolographicStereoTransform cameraProjectionTransform = cameraPose->ProjectionTransform();

	// Get a container object with the view and projection matrices for the given pose in the given coordinate system.

	// If TryGetViewTransform returns a null pointer, that means the pose and coordinate system cannot be understood relative to one another; content cannot be rendered in this coordinate system for the duration of the current frame
	// This usually means that positional tracking is not active for the current frame, in which case it is possible to use a SpatialLocatorAttachedFrameOfReference to render content that is not world-locked instead
	if (auto viewTransformContainer = cameraPose->TryGetViewTransform(coordinateSystem)) {
		// Update the view matrices. Holographic cameras (such as Microsoft HoloLens) are constantly moving relative to the world. The view matrices need to be updated every frame.
		HolographicStereoTransform viewCoordinateSystemTransform = viewTransformContainer->Value();
		eye_view[0] = load<float3x4>((float(*)[4])&viewCoordinateSystemTransform.Left);
		eye_view[1] = load<float3x4>((float(*)[4])&viewCoordinateSystemTransform.Right);
		eye_proj[0] = load<float4x4>((float(*)[4])&cameraProjectionTransform.Left);
		eye_proj[1] = load<float4x4>((float(*)[4])&cameraProjectionTransform.Right);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	HololensOutput
//-----------------------------------------------------------------------------

void SetSpaceD3D(HolographicSpace *space) {
	// The holographic space might need to determine which adapter supports holograms, in which case it will specify a non-zero PrimaryAdapterId
	graphics.Init(GetAdapter(force_cast<LUID>(space->PrimaryAdapterId())));

	// Wrap the native device using a WinRT interop object
	IInspectable *pobject;
	if (SUCCEEDED(CreateDirect3D11DeviceFromDXGIDevice(temp_com_cast<IDXGIDevice3>(graphics.Device()), &pobject))) {
		space->SetDirect3D11Device(pobject);
		pobject->Release();
	}
}

struct HololensOutput : RenderOutput {
	ptr<HolographicSpace>						space;
	ptr<SpatialLocator>							locator;
	ptr<SpatialStationaryFrameOfReference>		stationaryReferenceFrame;

	Platform::EventRegistrationToken			cameraAddedToken;
	Platform::EventRegistrationToken			cameraRemovedToken;
	Platform::EventRegistrationToken			locatabilityChangedToken;
	Platform::EventRegistrationToken			isAvailableChangedEventToken;
	Platform::EventRegistrationToken			sourcePressedEventToken;


	bool	canGetDefaultHolographicDisplay;
	bool	canCommitDirect3D11DepthBuffer;
	bool	canUseWaitForNextFrameReadyAPI;

	hash_map<uint32, unique_ptr<CameraResources>>	cameras;
	Mutex										cameras_lock;

	ptr<HolographicFrame>						frame;


	//input
	ptr<SpatialInteractionManager>		interactionManager;	// API objects used to process gesture input, and generate gesture events
	ptr<SpatialInteractionSourceState>	sourceState;		// Used to indicate that a Pressed input event was received this frame

	void OnLocatabilityChanged(SpatialLocator *sender) {
		switch (sender->Locatability()) {
			case SpatialLocatability::Unavailable:
				// Holograms cannot be rendered.
				//winrt::hstring message(L"Warning! Positional tracking is " + std::to_wstring(int(sender.Locatability())) + L".\n");
				//	OutputDebugStringW(message.data());
				break;

			// In the following three cases, it is still possible to place holograms using a SpatialLocatorAttachedFrameOfReference.
			case SpatialLocatability::PositionalTrackingActivating:
				// The system is preparing to use positional tracking.

			case SpatialLocatability::OrientationOnly:
				// Positional tracking has not been activated.

			case SpatialLocatability::PositionalTrackingInhibited:
				// Positional tracking is temporarily inhibited. User action may be required in order to restore positional tracking.
				break;

			case SpatialLocatability::PositionalTrackingActive:
				// Positional tracking is active. World-locked content can be rendered.
				break;
		}
	}

	// Checks if the user performed an input gesture since the last call to this method.
	// Allows the main update loop to check for asynchronous changes to the user input state.
	SpatialInteractionSourceState *CheckForInput() {
		return exchange(sourceState, nullptr);
	}

	HololensOutput(HolographicSpace *_space, Flags flags) : space(_space) {
		canGetDefaultHolographicDisplay		= Platform::Metadata::ApiInformation::IsMethodPresent(name_of<HolographicDisplay>(), L"GetDefault");
		canCommitDirect3D11DepthBuffer		= Platform::Metadata::ApiInformation::IsMethodPresent(name_of<HolographicCameraRenderingParameters>(), L"CommitDirect3D11DepthBuffer");
		canUseWaitForNextFrameReadyAPI		= Platform::Metadata::ApiInformation::IsMethodPresent(name_of<HolographicSpace>(), L"WaitForNextFrameReady");

		if (Platform::Metadata::ApiInformation::IsPropertyPresent(name_of<HolographicCamera>(), L"Display")) {
			isAvailableChangedEventToken = HolographicSpace::IsAvailableChanged += [this]() {
				if (!HolographicSpace::IsAvailable())
					space = nullptr;
			};
		}

		SetSpaceD3D(space);

		if (canGetDefaultHolographicDisplay) {
			if (auto defaultHolographicDisplay = HolographicDisplay::GetDefault())
				locator = defaultHolographicDisplay->SpatialLocator;
		}
		if (!locator)
			locator = SpatialLocator::GetDefault();

		locatabilityChangedToken = locator->LocatabilityChanged += {this, &HololensOutput::OnLocatabilityChanged};
		stationaryReferenceFrame = locator->CreateStationaryFrameOfReferenceAtCurrentLocation();

		cameraAddedToken = space->CameraAdded += [this](HolographicSpace *space, HolographicSpaceCameraAddedEventArgs *args) {
			HolographicCamera	*camera = args->Camera();
			auto	lock = with(cameras_lock);
			cameras[camera->Id] = new CameraResources(camera);
		};
		cameraRemovedToken = space->CameraRemoved += [this](HolographicSpace *space, HolographicSpaceCameraRemovedEventArgs *args) {
			auto	lock = with(cameras_lock);
			cameras[args->Camera->Id].remove();
			//clear rendertargets and flush
		};

#ifdef PLAT_WINRT
		interactionManager = SpatialInteractionManager::GetForCurrentView();
		sourcePressedEventToken = interactionManager->SourcePressed += [this](SpatialInteractionManager *sender, SpatialInteractionSourceEventArgs *args) {
			sourceState = args->State;
		};
#endif
	}

	~HololensOutput() {
		if (space) {
			space->CameraAdded		-= cameraAddedToken;
			space->CameraRemoved	-= cameraRemovedToken;
		}

		if (locator)
			locator->LocatabilityChanged -= locatabilityChangedToken;

		interactionManager->SourcePressed -= sourcePressedEventToken;

		HolographicSpace::IsAvailableChanged -= isAvailableChangedEventToken;
	}

	Flags	GetFlags()	{ return RenderOutput::DIMENSIONS_3; }

	void	BeginFrame(GraphicsContext &ctx) {
		graphics.BeginScene(ctx);

		if (canUseWaitForNextFrameReadyAPI)
			space->WaitForNextFrameReady();
		else if (frame)
			frame->WaitForFrameToFinish();

		// The HolographicFrame has information that the app needs in order to update and render the current frame
		// The app begins each new frame by calling CreateNextFrame
		frame = space->CreateNextFrame();

		// Get a prediction of where holographic cameras will be when this frame is presented
		ptr<HolographicFramePrediction> prediction = frame->CurrentPrediction();

		// Back buffers can change from frame to frame. Validate each buffer, and recreate resource views and depth buffers as needed
		{
			auto	lock = with(cameras_lock);
			for (auto&& pose : prediction->CameraPoses()) {
				if (CameraResources *cam = get(cameras[pose->HolographicCamera->Id].get())) {
					cam->CreateResourcesForBackBuffer(graphics.Device(), frame->GetRenderingParameters(pose), canCommitDirect3D11DepthBuffer);
					cam->UpdateViewProjectionBuffer(ctx, pose, stationaryReferenceFrame->CoordinateSystem());
				}

				// On HoloLens 2, the platform can achieve better image stabilization results if it has a stabilization plane and a depth buffer.
				// Note that the SetFocusPoint API includes an override which takes velocity as a parameter, which is recommended for stabilizing holograms in motion
				// The HolographicCameraRenderingParameters class provides access to set the image stabilization parameters
				ptr<HolographicCameraRenderingParameters> renderingParameters = frame->GetRenderingParameters(pose);

				// SetFocusPoint informs the system about a specific point in your scene to prioritize for image stabilization
				// The focus point is set independently for each holographic camera
				// When setting the focus point, put it on or near content that the user is looking at:
				//	if (stationaryReferenceFrame)
				//		renderingParameters->SetFocusPoint(stationaryReferenceFrame->CoordinateSystem(), m_spinningCubeRenderer->GetPosition());
			}
		}

	}
	void	EndFrame(GraphicsContext &ctx) {
		graphics.EndScene(ctx);

		if (frame && frame->PresentUsingCurrentPrediction(HolographicFramePresentWaitBehavior::DoNotWaitForFrameToFinish) == HolographicFramePresentResult::DeviceRemoved) {
			auto	lock = with(cameras_lock);
			cameras.clear();

			// Ensure system references to the back buffer are released by clearing the render target from the graphics pipeline state, and then flushing the Direct3D context
			//ID3D11RenderTargetView* nullViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {nullptr};
			//context->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
			//context->Flush();

			SetSpaceD3D(space);
		}
	}

	RenderView	GetView(int i) {
		return {identity, one, {zero, zero}, {}, {}, i};
	}
	
	generator<RenderView>	generate_views() {
		for (CameraResources *c : cameras) {
			if (c->stereo) {
				co_yield c->GetView(0);
				co_yield c->GetView(1);
			} else {
				co_yield c->GetView(0);
			}
		}
	}

	virtual_container<RenderView> Views() {
		return generate_views();
	}

	point	DisplaySize() {
		return {0, 0};
	}
	void	SetSize(RenderWindow *window, const point &size) {}
};

struct Hololens : RenderOutputFinderPri<Hololens, 0> {

#ifdef PLAT_WINRT
	struct HologramViewProvider : public runtime<HologramViewProvider, IFrameworkViewSource, IFrameworkView> {
		ptr<HolographicSpace>		space;
		Event						event;

		HologramViewProvider() {}

		ptr<IFrameworkView> CreateView() {
			return this;
		}
		void Initialize(ptr<CoreApplicationView> view) {}
		void Uninitialize() {}
		void SetWindow(ptr<CoreWindow> window) {}
		void Load(ptr<Platform::String> entrypoint) {}
		void Run() {
			auto window = CoreWindow::GetForCurrentThread();
			space = HolographicSpace::CreateForCoreWindow(window);
			event.signal();
			window->Activate();
			window->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
		}
	};

	ptr<Windows::ApplicationModel::Core::CoreApplicationView>	view;
#else
	struct HologramWindow : win::Window<HologramWindow> {
		ptr<IHolographicSpace> space;
		LRESULT Proc(win::MSG_ID message, WPARAM wParam, LPARAM lParam) {
			switch (message) {
				case WM_DESTROY:
					PostQuitMessage(0);
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
			return 0;
		}
		HologramWindow() {
			Create(win::WindowPos(), "hologram", win::Control::NOSTYLE);
			auto holographicSpaceInterop = get_activation_factory<HolographicSpace, IHolographicSpaceInterop>();
			holographicSpaceInterop->CreateForWindow(hWnd, iso_winrt::uuidof<IHolographicSpace>(), (void**)&space);
		}
	};
	
	unique_ptr<HologramWindow>	view;
#endif

	RenderOutput::Flags Capability(RenderOutput::Flags flags) {
		return HolographicSpace::IsAvailable() ? RenderOutput::DIMENSIONS_3 : RenderOutput::NONE;
	}

	RenderOutput *Create(RenderWindow *window, const point &size, RenderOutput::Flags flags) {
		if ((flags & RenderOutput::DIMENSIONS_3) && HolographicSpace::IsAvailable()) {
#ifdef PLAT_WINRT
			auto provider = ref_new<HologramViewProvider>();
			view	= Windows::ApplicationModel::Core::CoreApplication::CreateNewView(provider);
			provider->event.wait();
			return new HololensOutput(provider->space, flags);
#else
			view.emplace();
			if (ptr<HolographicSpace> space = view->space)
				return new HololensOutput(space, flags);
#endif
		}
		return 0;
	}
	Hololens() {
#ifndef PLAT_WINRT
		RoInitialize(RO_INIT_MULTITHREADED);
#endif
	}
} hololens;

extern "C" void *include_hololens() { return &hololens; }
