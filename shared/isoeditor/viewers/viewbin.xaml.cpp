#include "pch.h"
#include "viewer.h"
#include "winrt/binding.h"
#include "winrt/UIHelper.h"
#include "viewbin.h"

namespace app {

using namespace iso_winrt;
using namespace Platform;
using namespace Collections;
using namespace Windows::UI::Xaml;

class ViewBin : public ViewBin_base, public bindable, public compose<ViewBin, Controls::Page, Markup::IComponentConnector2> {
	ptr<Controls::Image>					image;
	ptr<Controls::Canvas>					canvas;
	ptr<Controls::Border>					border;
	ptr<Controls::TextBlock>				text;

	ptr<Media::Imaging::SurfaceImageSource>	source;
	d2d::Write				write;
	d2d::Font				font;

	d2d::WinRT				target;
	Platform::Point			mouse;
	int64					scroll;

	bool CreateDeviceResources() {
		target.Init(source);
		return true;//checker.CreateDeviceResources(target);
	}
	void DiscardDeviceResources() {
		//checker.DiscardDeviceResources();
		target.DeInit();
	}
	void Render() {
		CreateDeviceResources();

		win::Rect	rect(0, 0,image->Width, image->Height);
		win::Point	offset;

		if (target.BeginDraw(rect, offset)) {

			target.SetTransform(identity);
			target.Clear(d2d::colour(0,0,0));
			Paint(target, rect, 0, scroll, write, font);

			if (target.EndDraw())
				DiscardDeviceResources();
		}
	}

public:
	ViewBin() : font(write, L"Courier New", 15) {
		ptr<Windows::Foundation::Uri> resourceLocator = ref_new<Windows::Foundation::Uri>(L"ms-appx:///viewers/viewbin.xaml");
		Application::LoadComponent((ptr<Object>)this, resourceLocator, Controls::Primitives::ComponentResourceLocation::Application);

		canvas	= Common::FindDescendant(ptr<DependencyObject>(this), L"canvas");
		image	= Common::FindDescendant(ptr<DependencyObject>(this), L"source");
		border	= Common::FindDescendant(ptr<DependencyObject>(this), L"border");
		text	= Common::FindDescendant(ptr<DependencyObject>(this), L"text");

//		write.CreateFont(&font, L"Courier New", 15);
		font->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		SetFontMetrics(write, font);

		SizeChanged += [this](object sender, ptr<SizeChangedEventArgs> e) {
			Size	size = e->NewSize;
			image->Width = size.Width;
			image->Height = size.Height;
			source	= ref_new<Media::Imaging::SurfaceImageSource>(size.Width, size.Height, true);
			image->Source = source;
			DiscardDeviceResources();

			Render();
		};
	}
	void OnNavigatedTo(ptr<Navigation::NavigationEventArgs> e);
	void OnPointerPressed(ptr<Input::PointerRoutedEventArgs> e);
	void OnPointerMoved(ptr<Input::PointerRoutedEventArgs> e);
	void OnPointerWheelChanged(ptr<Input::PointerRoutedEventArgs> e);
};

void ViewBin::OnNavigatedTo(ptr<Navigation::NavigationEventArgs> e) {
	if (auto p = unbox<ISO_ptr_machine<void>>(e->Parameter()))
		SetBinary(p);
	Page::OnNavigatedTo(e);
}

void ViewBin::OnPointerPressed(ptr<Input::PointerRoutedEventArgs> e) {
	mouse	= e->GetCurrentPoint(image)->Position;
}

void ViewBin::OnPointerMoved(ptr<Input::PointerRoutedEventArgs> e) {
}

void ViewBin::OnPointerWheelChanged(ptr<Input::PointerRoutedEventArgs> e) {
	auto			curr	= e->GetCurrentPoint(image);
	int				wheel	= curr->Properties->MouseWheelDelta;
	Platform::Point pt		= curr->Position;

	if ((bool)Window::Current->CoreWindow->GetAsyncKeyState(Windows::System::VirtualKey::Control)) {
		float	mult	= iso::pow(1.05f, wheel / 64.f);
		zoom	= max(zoom / mult, 1/30.f);
		//ChangedSize();
	} else {
		//VScroll(si.MoveBy(-s / WHEEL_DELTA * 5));
		//SetScroll(si);
	}

	Render();
}

//-----------------------------------------------------------------------------
//	Editor
//-----------------------------------------------------------------------------

class EditorBin : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return	ViewBin_base::Matches(type);
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_ptr_machine<void> &p) {
		return main.AddView(typeof<ViewBin>(), box(p));
	}
} editorbin;


}
