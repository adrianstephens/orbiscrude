#include "app.h"
#include "utilities.h"
#include "graphics.h"

#include "winrt/base.h"
#include "winrt/Windows.ApplicationModel.h"
#include "winrt/Windows.ApplicationModel.Activation.h"
#include "winrt/Windows.ApplicationModel.Core.h"
#include "winrt/Windows.Foundation.Metadata.h"
#include "winrt/Windows.Gaming.Input.h"
#include "winrt/Windows.Graphics.Display.h"
#include "winrt/Windows.Storage.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/Windows.UI.Core.h"
#include "winrt/Windows.UI.Input.h"

using namespace iso_winrt;

using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;

//#define RUN2
//--------------------------------------------------------------------------------------

namespace iso {

Application	*iso_app;
HANDLE		close_event;

char	*update_dir = "update:\\";

point GetSize(const Windows::Foundation::Size &size) {
	float	dpi = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi();
	return point{int(size.Width * dpi / 96), int(size.Height * dpi / 96)};
}

point GetSize(const Windows::Foundation::Rect &rect) {
	float	dpi = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi();
	return point{int(rect.Width * dpi / 96), int(rect.Height * dpi / 96)};
}

point GetSize(const RenderWindow *win) {
	return GetSize(((Windows::UI::Core::CoreWindow*)win)->Bounds);
}

filename UserDir() {
	static filename user_dir = str(Package::Current->InstalledLocation->Path().raw());
	return user_dir;
}


Application::Application(const char *title) {
	auto	win = CoreWindow::GetForCurrentThread();
	window = (RenderWindow*)win.get();

	win->SizeChanged += [this](CoreWindow *win, WindowSizeChangedEventArgs *args) {
		if (output)
			output->SetSize((RenderWindow*)win, GetSize(args->Size));
	};

	AppEvent(AppEvent::PRE_GRAPHICS).send();
//	graphics.Init();
//	SetOutput(GetSize(win->Bounds), RenderOutput::NONE);
}

bool Application::SetOutput(const point &size, RenderOutput::Flags flags) {
	if (!output || any(size != DisplaySize()) || flags != GetOutputFlags()) {
		output = 0;
		if (RenderOutput *o = RenderOutputFinder::FindAndCreate(window, size, flags))
			output = o;
	}
	return true;
}

#ifdef RUN2
void Application::Run() {
	iso_app = this;
	close_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	((CoreWindow*)window)->Activate();

	for (;;) {
		Sleep(1000);
	}
	//WaitForSingleObject(close_event, INFINITE);
}
#else
void Application::Run() {
	if (!output)
		SetOutput(GetSize(window), RenderOutput::NONE);

	while (true) {//!exiting) {
		PROFILE_CPU_EVENT("Update");
		AppEvent(AppEvent::UPDATE).send();

		PROFILE_CPU_EVENT_NEXT("Render");
		BeginFrame();
		AppEvent(AppEvent::RENDER).send();
		EndFrame();

		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
	}
}
#endif

}

using namespace iso;

struct ViewProvider : public runtime<ViewProvider, IFrameworkViewSource, IFrameworkView> {
	bool		exiting;
public:
	ViewProvider() : exiting(false) {}

	// IFrameworkViewSource
	ptr<IFrameworkView> CreateView() {
		return this;
	}

	// IFrameworkView
	void Initialize(ptr<CoreApplicationView> applicationView) {

		applicationView->Activated	+= [this](CoreApplicationView *applicationView, IActivatedEventArgs *args) {
#ifdef RUN2
			IsoMain(0);
#else
			applicationView->CoreWindow()->Activate();
#endif
		};

		CoreApplication::Suspending += [this](Platform::Object *sender, SuspendingEventArgs *args) {
		};

		CoreApplication::Resuming	+= [this](Platform::Object *sender, Platform::Object *args) {
		};

	}

	void Uninitialize() {
	}

	void SetWindow(ptr<CoreWindow> window) {
		//window->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
		//auto	vis_settings = Windows::UI::Input::PointerVisualizationSettings::GetForCurrentView();
		//vis_settings->IsContactFeedbackEnabled = false;
		//vis_settings->IsBarrelButtonFeedbackEnabled = false;

		window->Closed += [this](CoreWindow*, CoreWindowEventArgs *args) {
			SetEvent(close_event);
		};

		window->VisibilityChanged += [this](CoreWindow*, VisibilityChangedEventArgs *args) {
		};
		/*
		using DisplayInformation = Windows::Graphics::Display::DisplayInformation;
		auto	display = DisplayInformation::GetForCurrentView();

		display->DpiChanged += [this](DisplayInformation*, Platform::Object*) {
		};

		display->OrientationChanged += [this](DisplayInformation*, Platform::Object*) {
		};

		display->StereoEnabledChanged += [this](DisplayInformation*, Platform::Object*) {
		};

		DisplayInformation::DisplayContentsInvalidated += [this](DisplayInformation*, Platform::Object*) {
		};
		*/
	}

	void Load(ptr<Platform::String> entrypoint) {
	}

#ifdef RUN2
	void Run() {
		while (true) {//!exiting) {
			PROFILE_CPU_EVENT("Update");
			AppEvent(AppEvent::UPDATE).send();

			PROFILE_CPU_EVENT_NEXT("Render");
			iso_app->BeginFrame();
			AppEvent(AppEvent::RENDER).send();
			iso_app->EndFrame();

			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
		}
	}
#else
	void Run() {
		IsoMain(0);
	}
#endif
};

int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	HRESULT	hr = RoInitialize(RO_INIT_MULTITHREADED);
	CoreApplication::Run(ref_new<ViewProvider>());
}
