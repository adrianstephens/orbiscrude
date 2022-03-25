#include "base/coroutine.h"
#include "winrt.h"

#if 0

#include "winrt/windows.applicationmodel.h"
#include "winrt/windows.storage.h"
#include "winrt/windows.graphics.h"
#include "winrt/windows.graphics.display.h"
#include "winrt/windows.ui.h"
#include "winrt/windows.ui.core.h"
#include "winrt/windows.ui.input.h"
#include "winrt/windows.ui.xaml.h"
#include "winrt/windows.ui.xaml.controls.h"
#include "winrt/windows.ui.viewmanagement.h"

using namespace iso_winrt;
using namespace Platform;
using namespace Windows;
using namespace Foundation;
using namespace UI::Composition;

struct test_wrl2 {
	int init() {
		HRESULT	hr;
		hr = RoInitialize(RO_INIT_MULTITHREADED);
		if (FAILED(hr))
			return 0;

		ptr<MemoryBuffer> mem	= ref_new<MemoryBuffer>(1024);
		ptr<IMemoryBufferReference> mem2 = mem->CreateReference();

		mem->Close();

		ptr<Uri> uri2	= ref_new<Uri>(L"http://www.microsoft.com");
		ISO_TRACEF("Domain name: ") << uri2->Domain() << '\n';

		return 1;
	}
	test_wrl2() {
		init();
	}
} _test_wrl2;

#else

#include "winrt/windows.storage.h"
#include "winrt/windows.applicationmodel.h"
#include "winrt/windows.networking.h"
#include "winrt/windows.web.h"
#include "winrt/windows.web.syndication.h"
#include "winrt/windows.ui.h"
#include "winrt/windows.ui.xaml.h"
#include "winrt/windows.ui.xaml.controls.h"

using namespace iso;

using namespace iso_winrt;
using namespace Windows;
using namespace Foundation;
using namespace ApplicationModel;
using namespace ApplicationModel::Core;
using namespace ApplicationModel::Activation;

using namespace Web::Syndication;
using namespace UI;
using namespace Xaml::Controls;

struct App : public runtime<App, IFrameworkViewSource, IFrameworkView> {
	ptr<IFrameworkView> CreateView() {
		return this;
	}
	void Initialize(CoreApplicationView* applicationView) {}
	void SetWindow(UI::Core::CoreWindow* window) {}
	void Load(hstring_ref entryPoint) {}
	void Run() {}
	void Uninitialize() {}
};

class XamlApp : public compose<XamlApp, Xaml::Application> {
public:
	void OnLaunched(ApplicationModel::Activation::LaunchActivatedEventArgs* args) {
		auto txtBlock				= ref_new<Xaml::Controls::TextBlock>();
		txtBlock->Text				= L"Hello World ( Welcome to UWP/C++ tutorial)";
		txtBlock->TextAlignment		= Xaml::TextAlignment::Center;
		txtBlock->VerticalAlignment = Xaml::VerticalAlignment::Center;

		ptr<Xaml::Window>	window	= Xaml::Window::Current;
		window->Content = txtBlock;
		window->Activate();

		auto	dialog  = ref_new<Xaml::Controls::ContentDialog>();
		dialog->Title	= L"No wifi connection";
		dialog->Content	= L"Check connection and try again.";
		dialog->CloseButtonText(L"Ok");
		dialog->ShowAsync();
	}
};


struct test_wrl2 {
	void init() {
		HRESULT	hr;

		// Initialize the Windows Runtime.
		hr = RoInitialize(RO_INIT_MULTITHREADED);
		if (FAILED(hr))
			return;

#if 0
	
		Xaml::Application::Start([](auto &&) {
			new XamlApp;
		});  

		auto	app = new App;
		CoreApplication::Run(app);

//		auto viewProviderFactory = new ViewProviderFactory;
//		CoreApplication::Run(viewProviderFactory);

//		auto	app = ref_new<Windows::ApplicationModel::Core::CoreApplication>();
//			::MainView::CoreWindow::Dispatcher.RunAsync(
//			CoreDispatcherPriority.Normal,

		ptr<ContentDialog>	dialog  = ref_new<ContentDialog>();
		dialog->Title(L"No wifi connection");
//		dialog->Title(PropertyValue::CreateString(L"No wifi connection"));

//		dialog->Content(props->CreateString(L"Check connection and try again."));
		dialog->CloseButtonText(L"Ok");

#else
		ptr<Uri> uri	= ref_new<Uri>(L"http://kennykerr.ca/feed");
		ptr<SyndicationClient>	client	= ref_new<SyndicationClient>();

		auto	feed = client->RetrieveFeedAsync(uri);
		feed->Progress([](const RetrievalProgress &p) {
		//->Progress([](IAsyncOperationWithProgress<SyndicationFeed*, RetrievalProgress>* asyncInfo, RetrievalProgress p) {
			ISO_TRACEF("Progress: ") << p.BytesRetrieved << " (" << p.TotalBytesToRetrieve << ")\n";
		});
		feed->Completed([](ptr<SyndicationFeed> syn) {
		//->Completed([](IAsyncOperationWithProgress<SyndicationFeed*, RetrievalProgress>* asyncInfo, AsyncStatus asyncStatus) {
		//	auto	syn = asyncInfo->GetResults();
			for (auto i : syn->Items()) {
				ISO_TRACEF("ASync ") << i->Title()->Text() << '\n';
			}
		});

		for (auto i : (co_await client->RetrieveFeedAsync(uri))->Items()) {
			ISO_TRACEF("Await ") << i->Title()->Text() << '\n';
		}
		for (auto i : sync(client->RetrieveFeedAsync(uri))->Items()) {
			ISO_TRACEF("Sync ") << i->Title()->Text() << '\n';
		}

		ptr<Uri> uri2	= ref_new<Uri>(L"http://www.microsoft.com");
		ISO_TRACEF("Domain name: ") << uri2->Domain() << '\n';
#endif
	}
	test_wrl2() {
		init();
	}
} _test_wrl2;

void test_my_wrl() {
//	test_wrl2	test;
}
#endif