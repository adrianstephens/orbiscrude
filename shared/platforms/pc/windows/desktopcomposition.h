#ifndef DESKTOPCOMPOSITION_H
#define DESKTOPCOMPOSITION_H

#include "window.h"
#include "winrt/base.h"
#include "winrt/Windows.UI.Composition.0.h"
#include "winrt/Windows.UI.Composition.Interactions.0.h"
#include "winrt/Windows.UI.Composition.Desktop.0.h"
#include "winrt/Windows.UI.Xaml.h"
#include "winrt/Windows.UI.Xaml.Hosting.0.h"
#include "extra/colour.h"

namespace composition {
using namespace iso;
using namespace iso_winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Interactions;
using namespace Windows::UI::Composition::Desktop;
using namespace Windows::UI::Xaml::Hosting;

void init();
ptr<Xaml::UIElement>	LoadXamlControl(const wchar_t *xml);
ptr<Xaml::UIElement>	LoadXamlControl(uint32 id);
HWND					CreateDesktopWindowsXamlSource(HWND hWndParent, ptr<Xaml::UIElement> content);

struct DesktopWindow {
	HWND	hWnd;
	GUID    lastFocusRequestId;
	dynamic_array<pair<ptr<DesktopWindowXamlSource>, Platform::EventRegistrationToken>>	takeFocusEvents;
	dynamic_array<ptr<DesktopWindowXamlSource>>		xamlSources;

	ptr<DesktopWindowXamlSource>	GetFocusedIsland();
	ptr<DesktopWindowXamlSource>	GetIsland(HWND child);

	void							OnTakeFocusRequested(ptr<DesktopWindowXamlSource> sender, ptr<DesktopWindowXamlSourceTakeFocusRequestedEventArgs> args);
	bool							NavigateFocus(MSG* msg);

protected:
	int		MessageLoop(HACCEL accelerators);
	HWND	CreateDesktopWindowsXamlSource(DWORD extraStyles, ptr<Xaml::UIElement> content);
	void	ClearXamlIslands();
};


struct CompositionWindow {
	ptr<Compositor>				compositor;
	ptr<DesktopWindowTarget>	target;
	ptr<SpriteVisual>			root;

	ptr<VisualInteractionSource>	interaction;

public:
	CompositionWindow();
	~CompositionWindow();

	void	OnCreate(HWND hWnd) noexcept;
	void	OnDpiChanged(UINT dpi);
	void	OnResize(UINT width, UINT height);
	void	AddElement(float size, float x, float y, const colour &col);
	void	PointerDown(WPARAM wParam);
};

}

namespace iso { namespace win {
using composition::CompositionWindow;
} } // namespace iso::win

#endif //DESKTOPCOMPOSITION_H
