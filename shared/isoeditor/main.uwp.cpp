#include "pch.h"
#include "winrt/binding.h"
#include "winrt/UIHelper.h"
#include "main.h"
#include "devices/device.h"
#include "linq.h"

//#include "viewers/viewbitmap.xaml.h"

using namespace iso_winrt;
using namespace Platform;
using namespace Windows;
using namespace ApplicationModel;
using namespace Activation;
using namespace Collections;
using namespace UI::Xaml;
using namespace linq;

extern "C" void ExitProcess() {}

namespace app {

MainWindow* MainWindow::me;
MainWindow::MainWindow()	{ me = this; }
MainWindow::~MainWindow()	{ me = 0; }

void MainWindow::AddView(Control c)						{}
void MainWindow::SetTitle(const char *title)			{}
void MainWindow::AddLoadFilters(multi_string filters)	{}

IsoEditor::IsoEditor() : max_expand(1024) {}

class IsoEditor2 : public IsoEditor, public bindable, public compose<IsoEditor2, Controls::Page, Markup::IComponentConnector2> {

	ptr<Controls::Frame>	Frame;

	void OnNavigationViewItemInvoked(ptr<Controls::NavigationView> sender, ptr<Controls::NavigationViewItemInvokedEventArgs> args) {
		if (args->IsSettingsInvoked) {
		} else {
			
			ptr<Controls::NavigationViewItem>	item = sender->MenuItems() | where([&](ptr<Controls::NavigationViewItem> i) { return i->Content == args->InvokedItem; }) | single();
			
			if (auto dev = unbox<DeviceCreate*>(item->DataContext)) {
				apartment_context ui_thread;
				co_await background();

				ISO_ptr<void>	p	= (*dev)(query<Controls::Control>(sender.get()));
				ISO::Browser2	b(p);
				Editor			*ed = Editor::Find(b);
				co_await ui_thread;

				if (ed) {
					ed->Create(*this, WindowPos(), (ISO_ptr<void,64>)b);
				}

				//Frame->Navigate({L"app.ViewBitmap", Interop::TypeKind::Custom}, box(p));
				//Frame->Navigate(typeof<ViewBitmap>(), nullptr);
			}

		}
	}

	bool TryGoBack() {
		if (Frame->CanGoBack) {
			Frame->GoBack();
			return true;
		}
		return false;
	}

	bool TryGoForward() {
		if (Frame->CanGoForward) {
			Frame->GoForward();
			return true;
		}
		return false;
	}

public:
	ptr<Controls::NavigationView>			NavigationViewControl;
	ptr<UI::Core::SystemNavigationManager>	SystemNavigationManager;

	IsoEditor2() {
		ptr<Windows::Foundation::Uri> resourceLocator = ref_new<Windows::Foundation::Uri>(L"ms-appx:///IsoEditor.xaml");
		Application::LoadComponent((ptr<Object>)this, resourceLocator, Controls::Primitives::ComponentResourceLocation::Application);

		NavigationViewControl	= Common::FindDescendant(ptr<DependencyObject>(this), L"NavigationViewControl");

		DeviceAdd	add((ptr<Controls::Control>)NavigationViewControl);
		for (auto &i : app::Device::all())
			i(add);

		//KeyDown		+= [this](object sender, ptr<Input::KeyRoutedEventArgs> e) {
		//	if (!e->Handled && e->Key == Windows::System::VirtualKey::E) {
		//	}
		//};

		Loaded		+= [this](object sender, object e) {
			NavigationViewControl->ItemInvoked += {this, &IsoEditor2::OnNavigationViewItemInvoked};

			Frame					= Common::FindDescendant(ptr<DependencyObject>(this), L"rootFrame");
			Frame->Navigated += [this](object s, object e) {
				SystemNavigationManager->AppViewBackButtonVisibility = Frame->CanGoBack ? UI::Core::AppViewBackButtonVisibility::Visible : UI::Core::AppViewBackButtonVisibility::Collapsed;
			};

			SystemNavigationManager = UI::Core::SystemNavigationManager::GetForCurrentView();
			SystemNavigationManager->BackRequested += [this](object sender, ptr<UI::Core::BackRequestedEventArgs> e) {
				if (!e->Handled)
					e->Handled = TryGoBack();
			};

			Frame->Navigate({L"app.TreeViewTest", Interop::TypeKind::Custom}, nullptr);

		};
	}

//	void Connect(int connectionId, ptr<Object> target) {
//	}

	ptr<Markup::IComponentConnector> GetBindingConnector(int connectionId, ptr<Object> target) {
		return nullptr;
	}

	bool	AddView(const Interop::TypeName& sourcePageType, object param) {
		return Frame->Navigate(sourcePageType, param);
	}

};

Control MainWindow::AddView(const Interop::TypeName& sourcePageType, object param)	{
	static_cast<IsoEditor2*>(this)->AddView(sourcePageType, param);
	return query<Controls::Control>(static_cast<IsoEditor2*>(this));
}


class App : public compose<App, Application, Markup::IXamlMetadataProvider> {
public:
	ptr<Markup::IXamlType>				GetXamlType(Interop::TypeName type)	{ return provider->GetXamlType(type); }
	ptr<Markup::IXamlType>				GetXamlType(hstring_ref fullName)	{ return provider->GetXamlType(fullName); }
	ptr<Array<Markup::XmlnsDefinition>>	GetXmlnsDefinitions()				{ return ptr<Array<Markup::XmlnsDefinition>>(); }

	void OnLaunched(ptr<Activation::LaunchActivatedEventArgs> e);

internal:
	App() : provider(new XamlTypeInfo::Provider()) {
		Suspending	+= [](object sender, ptr<SuspendingEventArgs> e) {
		};
	}

private:
	ptr<XamlTypeInfo::Provider> provider;
};


void App::OnLaunched(ptr<Activation::LaunchActivatedEventArgs> e) {
	auto rootFrame = (ptr<Controls::Frame>)Window::Current->Content;

	if (rootFrame == nullptr) {
		rootFrame = ref_new<Controls::Frame>();
		rootFrame->NavigationFailed += [](object sender, ptr<Navigation::NavigationFailedEventArgs> e) {
			throw FailureException(L"Failed to load Page " + e->SourcePageType().Name);
		};

//		if (e->PreviousExecutionState == ApplicationExecutionState::Terminated) {
			// TODO: Restore the saved session state only when appropriate, scheduling the
			// final launch steps after the restore is complete

//		}

		if (e->PrelaunchActivated == false) {
			if (rootFrame->Content == nullptr)
				rootFrame->Navigate(typeof<IsoEditor2>(), e->Arguments());
			Window::Current->Content = rootFrame;
			Window::Current->Activate();
		}
	} else {
		if (e->PrelaunchActivated == false) {
			if (rootFrame->Content == nullptr)
				rootFrame->Navigate(typeof<IsoEditor2>(), e->Arguments());
			Window::Current->Activate();
		}
	}
}

//-----------------------------------------------------------------------------
//	MainWindow
//-----------------------------------------------------------------------------

int GetTreeIcon(const ISO::Browser2 &b0) {
	return -1;
}

}	// namespace app

int __stdcall WinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	HRESULT	hr = RoInitialize(RO_INIT_MULTITHREADED);
	Application::Start([]() { ref_new<app::App>(); });
}
