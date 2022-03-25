#include "desktopcomposition.h"
#include "winrt/base.h"
#include "winrt/Windows.System.h"
#include "winrt/Windows.UI.Composition.Desktop.h"
#include "winrt/Windows.UI.Composition.Interactions.h"
#include "winrt/Windows.UI.Input.h"
#include "winrt/Windows.Graphics.h"

#include "winrt/Windows.UI.Xaml.Interop.h"
#include "winrt/Windows.UI.Xaml.Controls.h"
#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "winrt/Windows.UI.Xaml.Hosting.h"
#include "winrt/Windows.UI.Xaml.Markup.h"

#include "winrt/Windows.ApplicationModel.Activation.h"
#include "winrt/Windows.ApplicationModel.Core.h"

#include "winrt/Microsoft.Toolkit.Win32.UI.XamlHost.h"

#include <DispatcherQueue.h>
#include "windows.ui.xaml.hosting.desktopwindowxamlsource.h"
#include "windows/d2d.h"

#define XAMLRESOURCE  256

STDMETHODIMP iso_winrt::Microsoft::Toolkit::Win32::UI::XamlHost::IXamlApplication_raw::_get_WindowsXamlManager(Windows::Foundation::IClosable* *value) { return E_FAIL; }
STDMETHODIMP iso_winrt::Microsoft::Toolkit::Win32::UI::XamlHost::IXamlApplication_raw::_get_IsDisposed(bool *value) { return E_FAIL; }

namespace composition {
using namespace Windows::UI::Input;
using namespace Windows::Graphics;
using namespace Platform::Numerics;

using namespace Windows::UI::Xaml::Controls;

class App : public compose<App, Windows::UI::Xaml::Application, Microsoft::Toolkit::Win32::UI::XamlHost::IXamlApplication> {
public:
	void OnLaunched(ptr<Windows::ApplicationModel::Activation::LaunchActivatedEventArgs> e) {}
};
struct MainUserControl : Windows::ApplicationModel::Core::CoreApplicationView {};


DECLARE_INTERFACE_IID_(ICompositorDesktopInterop, IUnknown, "29E691FA-4567-4DCA-B319-D0F207EB6807") {
	IFACEMETHOD(CreateDesktopWindowTarget)(
		_In_ HWND hwndTarget,
		_In_ BOOL isTopmost,
		_COM_Outptr_ IDesktopWindowTarget ** result
		) PURE;

	IFACEMETHOD(EnsureOnThread)(
		_In_ DWORD threadId
		) PURE;
};

DECLARE_INTERFACE_IID_(ICompositorInterop, IUnknown, "25297D5C-3AD4-4C9C-B5CF-E36A38512330") {
	IFACEMETHOD(CreateCompositionSurfaceForHandle)(
		_In_ HANDLE swapChain,
		_COM_Outptr_ ICompositionSurface ** result
		) PURE;

	IFACEMETHOD(CreateCompositionSurfaceForSwapChain)(
		_In_ IUnknown * swapChain,
		_COM_Outptr_ ICompositionSurface ** result
		) PURE;

	IFACEMETHOD(CreateGraphicsDevice)(
		_In_ IUnknown * renderingDevice,
		_COM_Outptr_ ICompositionGraphicsDevice ** result
		) PURE;
};

DECLARE_INTERFACE_IID_(ICompositionDrawingSurfaceInterop, IUnknown, "FD04E6E3-FE0C-4C3C-AB19-A07601A576EE") {
	IFACEMETHOD(BeginDraw)(
		_In_opt_ const RECT * updateRect,
		_In_ REFIID iid,
		_COM_Outptr_ void ** updateObject,
		_Out_ POINT * updateOffset
		) PURE;

	IFACEMETHOD(EndDraw)(
		) PURE;

	IFACEMETHOD(Resize)(
		_In_ SIZE sizePixels
		) PURE;

	IFACEMETHOD(Scroll)(
		_In_opt_ const RECT * scrollRect,
		_In_opt_ const RECT * clipRect,
		_In_ int offsetX,
		_In_ int offsetY
		) PURE;

	IFACEMETHOD(ResumeDraw)(
		) PURE;

	IFACEMETHOD(SuspendDraw)(
		) PURE;
};

auto CreateDispatcherQueueController() {
	ptr<Windows::System::DispatcherQueueController> controller;
	hrcheck(CreateDispatcherQueueController(DispatcherQueueOptions{ sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_STA}, (ABI::Windows::System::IDispatcherQueueController**)&controller));
	return controller;
}

auto CreateDesktopWindowTarget(ptr<Compositor> compositor, HWND hWnd) {
	ptr<DesktopWindowTarget> target;
	hrcheck(query<ICompositorDesktopInterop>(compositor, "29E691FA-4567-4DCA-B319-D0F207EB6807"_guid)->CreateDesktopWindowTarget(hWnd, true, (IDesktopWindowTarget**)&target));
	return target;
}

auto ConfigureInteraction(SpriteVisual *root) {
	auto	interaction = VisualInteractionSource::Create(root);
	interaction->PositionXSourceMode(InteractionSourceMode::EnabledWithInertia);
	interaction->PositionYSourceMode(InteractionSourceMode::EnabledWithInertia);
	interaction->ScaleSourceMode(InteractionSourceMode::EnabledWithInertia);
	interaction->ManipulationRedirectionMode(VisualInteractionSourceRedirectionMode::CapableTouchpadAndPointerWheel);
	return interaction;
}

#if 0
auto MakeTracker(Compositor *compositor, VisualInteractionSource *interaction, Vector3 max) {

	auto	tracker = InteractionTracker::CreateWithOwner(compositor, this);
	tracker->InteractionSources()->Add(interaction);
	tracker->MinPosition({0, 0, 0});
	tracker->MaxPosition(max);
	tracker->MinScale(0.2f);
	tracker->MaxScale(3.0f);
}
#endif

struct CompositionD2D : d2d::Target {
	com_ptr<ID3D11Device>			d3d11device;
	ptr<ICompositionSurface>		surface;
	ptr<CompositionSurfaceBrush>	surfaceBrush;

	CompositionD2D() {}
	CompositionD2D(Compositor *compositor, uint32 width, uint32 height, DirectX::DirectXPixelFormat pixelFormat, DirectX::DirectXAlphaMode alphaMode) {
		Init(compositor, width, height, pixelFormat, alphaMode);
	}
	bool Init(Compositor *compositor, int32 width, int32 height, DirectX::DirectXPixelFormat pixelFormat, DirectX::DirectXAlphaMode alphaMode) {
		if (FAILED(D3D11CreateDevice(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			pixelFormat == DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized ? D3D11_CREATE_DEVICE_BGRA_SUPPORT : 0,
			nullptr, 0,
			D3D11_SDK_VERSION,
			&d3d11device,
			nullptr,
			nullptr
		)))
			return false;

		com_ptr<ID2D1Device> d2device;
		if (FAILED(factory->CreateDevice(d3d11device.as<IDXGIDevice>(), &d2device)) || FAILED(d2device.query(&device)))
			return false;

		com_ptr<ICompositionGraphicsDevice>	graphics_device;
		hrcheck(query<ICompositorInterop>(compositor, "25297D5C-3AD4-4C9C-B5CF-E36A38512330"_guid)->CreateGraphicsDevice(d2device, (Composition::ICompositionGraphicsDevice**)&graphics_device));

		auto	virt_surface	= graphics_device.as<Composition::ICompositionGraphicsDevice2>()->CreateVirtualDrawingSurface({width, height}, pixelFormat, alphaMode);
		surface			= query<ICompositionDrawingSurfaceInterop>(virt_surface, "FD04E6E3-FE0C-4C3C-AB19-A07601A576EE"_guid).as<ICompositionSurface>();
		surfaceBrush	= compositor->CreateSurfaceBrush(surface);

		surfaceBrush->Stretch					= CompositionStretch::None;
		surfaceBrush->HorizontalAlignmentRatio	= 0;
		surfaceBrush->VerticalAlignmentRatio	= 0;
		surfaceBrush->TransformMatrix			= ToTranslation(Vector2{20.0f, 20.0f});

		return true;
	}
};

CompositionWindow::CompositionWindow() {}
CompositionWindow::~CompositionWindow() {}

void CompositionWindow::OnCreate(HWND hWnd) noexcept {
//	static auto init		= RoInitialize(RO_INIT_MULTITHREADED);
	static auto controller	= CreateDispatcherQueueController();

	compositor		= ref_new<Compositor>();
	target			= CreateDesktopWindowTarget(compositor, hWnd);
	root			= compositor->CreateSpriteVisual();
	root->RelativeSizeAdjustment = { 1.05f, 1.05f };
	root->Brush		= compositor->CreateColorBrush({ 0xFF, 0xFF, 0xFF , 0xFF });
	target->Root	= root;

	OnDpiChanged(GetDpiForWindow(hWnd));

//	auto	bd = ref_new<Microsoft::UI::Xaml::Controls::BackdropMaterial>();
}

void CompositionWindow::OnDpiChanged(UINT dpi) {
	auto scaleFactor = (float)dpi / 100;
	if (root && scaleFactor > 0)
		root->Scale = { scaleFactor, scaleFactor, 1.0 };
}

void CompositionWindow::OnResize(UINT width, UINT height) {
}

void CompositionWindow::AddElement(float size, float x, float y, const colour &col) {
	if (root) {
//		auto	visual		= compositor->CreateSpriteVisual();

		auto	col8		= to<uint8>(col.rgba * 255.f);
		auto	element		= compositor->CreateSpriteVisual();
		element->Brush		= compositor->CreateColorBrush({col8.a, col8.r, col8.g, col8.b});
		element->Size		= { size, size };
		element->Offset		= { x, y, 0, };

		auto	animation	= compositor->CreateVector3KeyFrameAnimation();
		float	bottom		= 600 - size;
		animation->InsertKeyFrame(1, { x, bottom, 0 });

		animation->Duration		= Platform::Duration(2);
		animation->DelayTime	= Platform::Duration(3);
		element->StartAnimation(L"Offset", animation);

		auto	visuals		= root.as<ContainerVisual>()->Children();
		visuals->InsertAtTop(element);
//		visuals->InsertAtTop(visual);
	}
}

void CompositionWindow::PointerDown(WPARAM wParam) {
	auto pp = PointerPoint::GetCurrentPoint(GET_POINTERID_WPARAM(wParam));
	interaction->TryRedirectForManipulation(pp);
}

ptr<Xaml::UIElement> LoadXamlControl(const wchar_t *xml) {
	auto content = Xaml::Markup::XamlReader::Load(xml);
	return content.as<Xaml::UIElement>();
}


ptr<Xaml::UIElement> LoadXamlControl(uint32 id) {
	if (auto rc = ::FindResource(nullptr, MAKEINTRESOURCE(id), MAKEINTRESOURCE(XAMLRESOURCE))) {
		if (HGLOBAL rcData = ::LoadResource(nullptr, rc))
			return LoadXamlControl((wchar_t*)::LockResource(rcData));
	}
	return nullptr;
}

const auto static invalidReason = static_cast<XamlSourceFocusNavigationReason>(-1);

XamlSourceFocusNavigationReason GetReasonFromKey(WPARAM key) {
	switch (key) {
		case VK_TAB: {
			byte keyboardState[256] = {};
			::GetKeyboardState(keyboardState);
			return keyboardState[VK_SHIFT] & 0x80 ? XamlSourceFocusNavigationReason::Last : XamlSourceFocusNavigationReason::First;
		}
		case VK_LEFT:	return XamlSourceFocusNavigationReason::Left;
		case VK_RIGHT:	return XamlSourceFocusNavigationReason::Right;
		case VK_UP:		return XamlSourceFocusNavigationReason::Up;
		case VK_DOWN:	return XamlSourceFocusNavigationReason::Down;
		default:		return invalidReason;
	}
}

bool GetDirFromReason(XamlSourceFocusNavigationReason reason) {
	return	reason != XamlSourceFocusNavigationReason::First
		&&	reason != XamlSourceFocusNavigationReason::Down
		&&	reason != XamlSourceFocusNavigationReason::Right;
}

WPARAM GetKeyFromReason(XamlSourceFocusNavigationReason reason) {
	switch (reason) {
		case XamlSourceFocusNavigationReason::Last:
		case XamlSourceFocusNavigationReason::First:
			return VK_TAB;
		case XamlSourceFocusNavigationReason::Left:
			return VK_LEFT;
		case XamlSourceFocusNavigationReason::Right:
			return VK_RIGHT;
		case XamlSourceFocusNavigationReason::Up:
			return VK_UP;
		case XamlSourceFocusNavigationReason::Down:
			return VK_DOWN;
		default:
			return -1;
	}
}

void init() {
	static auto app = ref_new<App>();
	static auto controller	= CreateDispatcherQueueController();
}

HWND CreateDesktopWindowsXamlSource(HWND hWndParent, ptr<Xaml::UIElement> content) {
	ptr<DesktopWindowXamlSource> desktopSource = ref_new<DesktopWindowXamlSource>();

	auto	interop = query<IDesktopWindowXamlSourceNative>(desktopSource);
	hrcheck(interop->AttachToWindow(hWndParent));

	HWND	xamlSourceWindow;
	hrcheck(interop->get_WindowHandle(&xamlSourceWindow));

	desktopSource->Content = content;

	return xamlSourceWindow;
}

HWND DesktopWindow::CreateDesktopWindowsXamlSource(DWORD extraWindowStyles, ptr<Xaml::UIElement> content) {
	ptr<DesktopWindowXamlSource> desktopSource = ref_new<DesktopWindowXamlSource>();

	auto	interop = desktopSource.as<IDesktopWindowXamlSourceNative>();
	hrcheck(interop->AttachToWindow(hWnd));	// Parent the DesktopWindowXamlSource object to current window

	HWND	xamlSourceWindow;				// Lifetime controlled desktopSource
	hrcheck(interop->get_WindowHandle(&xamlSourceWindow));

	SetWindowLongW(xamlSourceWindow, GWL_STYLE, GetWindowLongW(xamlSourceWindow, GWL_STYLE) | extraWindowStyles);

	desktopSource->Content = content;

	takeFocusEvents.emplace_back(desktopSource, desktopSource->TakeFocusRequested += {this, &DesktopWindow::OnTakeFocusRequested});
	xamlSources.push_back(desktopSource);

	return xamlSourceWindow;
}

ptr<DesktopWindowXamlSource> DesktopWindow::GetFocusedIsland() {
	for (auto& xamlSource : xamlSources) {
		if (xamlSource->HasFocus)
			return xamlSource;
	}
	return nullptr;
}

ptr<DesktopWindowXamlSource> DesktopWindow::GetIsland(HWND child) {
	for (auto& xamlSource : xamlSources) {
		HWND island;
		hrcheck(xamlSource.as<IDesktopWindowXamlSourceNative>()->get_WindowHandle(&island));
		if (child == island)
			return xamlSource;
	}
	return nullptr;
}

bool DesktopWindow::NavigateFocus(MSG* msg) {
	if (msg->message == WM_KEYDOWN) {
		auto	   reason = GetReasonFromKey(msg->wParam);
		if (reason != invalidReason) {
			auto		nextIsland = GetIsland(GetNextDlgTabItem(hWnd, GetFocus(), GetDirFromReason(reason)));
			ISO_ASSERT(!nextIsland->HasFocus);

			RECT		rect;
			ISO_VERIFY(::GetWindowRect(GetFocus(), &rect));

			HWND		islandWnd;
			hrcheck(nextIsland.as<IDesktopWindowXamlSourceNative>()->get_WindowHandle(&islandWnd));

			POINT		pt   = {rect.left, rect.top};
			::ScreenToClient(islandWnd, &pt);
			auto request		= ref_new<XamlSourceFocusNavigationRequest>(reason, Platform::Rect{float(pt.x), float(pt.y), float(rect.right - rect.left), float(rect.bottom - rect.top)});
			lastFocusRequestId	= request->CorrelationId;
			return nextIsland->NavigateFocus(request)->WasFocusMoved();
		}
	}

	const bool islandIsFocused	  = !!GetFocusedIsland();
	byte	   keyboardState[256] = {};
	ISO_VERIFY(::GetKeyboardState(keyboardState));
	return (!islandIsFocused || (keyboardState[VK_MENU] & 0x80)) && !!IsDialogMessage(hWnd, msg);
}

int DesktopWindow::MessageLoop(HACCEL accelerators) {
	MSG msg = {};
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		// When multiple child windows are present it is needed to pre dispatch messages to all DesktopWindowXamlSource instances so keyboard accelerators and keyboard focus work correctly
		BOOL processed = FALSE;
		for (auto& xamlSource : xamlSources) {
			hrcheck(xamlSource.as<IDesktopWindowXamlSourceNative2>()->PreTranslateMessage(&msg, &processed));
			if (processed)
				break;
		}

		if (!processed && !TranslateAccelerator(msg.hwnd, accelerators, &msg)) {
			if (!NavigateFocus(&msg)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
	}

	return msg.wParam;
}

void DesktopWindow::OnTakeFocusRequested(ptr<DesktopWindowXamlSource> sender, ptr<DesktopWindowXamlSourceTakeFocusRequestedEventArgs> args) {
	if (args->Request->CorrelationId != lastFocusRequestId) {
		auto	reason	= args->Request->Reason();
		HWND	senderHwnd;
		hrcheck(sender.as<IDesktopWindowXamlSourceNative>()->get_WindowHandle(&senderHwnd));

		MSG		msg = {senderHwnd, WM_KEYDOWN, GetKeyFromReason(reason)};
		if (!NavigateFocus(&msg))
			::SetFocus(::GetNextDlgTabItem(hWnd, senderHwnd, GetDirFromReason(reason)));

	} else {
		auto	request		= ref_new<XamlSourceFocusNavigationRequest>(XamlSourceFocusNavigationReason::Restore);
		lastFocusRequestId	= request->CorrelationId;
		sender->NavigateFocus(request);
	}
}

void DesktopWindow::ClearXamlIslands() {
	for (auto& i : takeFocusEvents)
		i.a->TakeFocusRequested -= i.b;
	takeFocusEvents.clear();

	for (auto& i : xamlSources)
		i->Close();
	xamlSources.clear();
}


} // namespace composition