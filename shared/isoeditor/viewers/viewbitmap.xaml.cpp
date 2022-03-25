#include "pch.h"
#include "viewer.h"
#include "winrt/binding.h"
#include "winrt/UIHelper.h"

#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) EXTERN_C const GUID DECLSPEC_SELECTANY name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#include "viewbitmap.h"

namespace app {

using namespace iso_winrt;
using namespace Platform;
using namespace Collections;
using namespace Windows::UI::Xaml;

class ViewBitmap : public ViewBitmap_base, public bindable, public compose<ViewBitmap, Controls::Page, Markup::IComponentConnector2> {
	using ViewBitmap_base::flags;

	ptr<Controls::Image>					image;
	ptr<Controls::Canvas>					canvas;
	ptr<Controls::Border>					border;
	ptr<Controls::TextBlock>				text;
	ptr<Controls::CommandBar>				commands;

	ptr<Media::Imaging::SurfaceImageSource>	source;

	d2d::WinRT				target;
	com_ptr<ID2D1Bitmap>	d2d_bitmap;
	bool					update_bitmap;
	Platform::Point			mouse;

	bool CreateDeviceResources() {
		target.Init(source);
		if (!d2d_bitmap)
			update_bitmap = target.CreateBitmap(&d2d_bitmap, bitmap_rect.Width(), bitmap_rect.Height() * num_slices);
		return true;//checker.CreateDeviceResources(target);
	}
	void DiscardDeviceResources() {
		//checker.DiscardDeviceResources();
		d2d_bitmap.clear();
		target.DeInit();
	}
	void Render() {
		CreateDeviceResources();

		if (update_bitmap) {
			UpdateBitmap(target, d2d_bitmap);
			update_bitmap = false;
		}
		win::Rect	rect(0, 0,image->Width, image->Height);
		win::Point	offset;

		if (target.BeginDraw(rect, offset)) {

			target.SetTransform(identity);
			target.Clear(d2d::colour(0,0,0));
			DrawBitmapPaint(target, rect, d2d_bitmap);

			if (target.EndDraw())
				DiscardDeviceResources();
		}
	}

	void	SetChannels(int c) {
		if (c == 0)
			c = CHAN_RGB;
		channels = c;
		if (c & CHAN_DEPTH) {
			bitmap64	*bm = this->bm;
			depth_range = get_extent(make_range_n((uint32*)bm->ScanLine(0), bm->Width() * bm->Height()));
		}
		Render();
	}
	bool	GetToggle(object but) {
		return ptr<Controls::AppBarToggleButton>(but)->IsChecked->GetBoolean();
	}
	void	SetFlag(object but, FLAGS flag) {
		flags.set(flag, GetToggle(but));
		Render();
	}

public:
	ViewBitmap() {
		ptr<Windows::Foundation::Uri> resourceLocator = ref_new<Windows::Foundation::Uri>(L"ms-appx:///viewers/viewbitmap.xaml");
		Application::LoadComponent((ptr<Object>)this, resourceLocator, Controls::Primitives::ComponentResourceLocation::Application);

		canvas		= Common::FindDescendant(ptr<DependencyObject>(this), L"canvas");
		image		= Common::FindDescendant(ptr<DependencyObject>(this), L"source");
		border		= Common::FindDescendant(ptr<DependencyObject>(this), L"border");
		text		= Common::FindDescendant(ptr<DependencyObject>(this), L"text");
		auto	commands0	= Common::FindDescendant(ptr<DependencyObject>(this), L"commands");
		commands	= commands0;

		commands->Loaded += [this](object sender, object e) {
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"red"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetChannels(GetToggle(sender) ? CHAN_R : CHAN_RGB); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"green"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetChannels(GetToggle(sender) ? CHAN_G : CHAN_RGB); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"blue"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetChannels(GetToggle(sender) ? CHAN_B : CHAN_RGB); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"alpha"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetChannels(GetToggle(sender) ? CHAN_A : CHAN_RGB); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"mips"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetFlag(sender, SHOW_MIPS); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"vflip"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetFlag(sender, FLIP_Y); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"hflip"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetFlag(sender, FLIP_X); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"grid"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetFlag(sender, DISP_GRID); };
			if (ptr<Controls::AppBarToggleButton> but = Common::FindDescendant(sender, L"3d"))
				but->Click += [this](object sender, ptr<RoutedEventArgs> e) { SetFlag(sender, SEPARATE_SLICES); };
		};

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

void ViewBitmap::OnNavigatedTo(ptr<Navigation::NavigationEventArgs> e) {
	if (auto p = unbox<ISO_ptr_machine<void>>(e->Parameter()))
		SetBitmap(p);
	Page::OnNavigatedTo(e);
}

void ViewBitmap::OnPointerPressed(ptr<Input::PointerRoutedEventArgs> e) {
	mouse	= e->GetCurrentPoint(image)->Position;
}

void ViewBitmap::OnPointerMoved(ptr<Input::PointerRoutedEventArgs> e) {
	auto			curr	= e->GetCurrentPoint(image);
	Platform::Point mouse2	= curr->Position;

	d2d::point		pt		= ClientToTexel0(mouse2);
	int				slice	= CalcSlice(pt);

	if (slice >= 0) {
		text->Text = str(DumpTexelInfo(lvalue(buffer_accum<256>()), pt.x, pt.y, slice, flags.test(SHOW_MIPS) ? CalcMip(pt) : 0));
		Controls::Canvas::SetLeft(border, mouse2.X);
		Controls::Canvas::SetTop(border, mouse2.Y);
	}
	if (curr->Properties->IsLeftButtonPressed) {
		pos		= pos + mouse2 - mouse;
		mouse	= mouse2;
		flags.clear(AUTO_SCALE);
		Render();
	}
}

void ViewBitmap::OnPointerWheelChanged(ptr<Input::PointerRoutedEventArgs> e) {
	auto			curr	= e->GetCurrentPoint(image);
	int				wheel	= curr->Properties->MouseWheelDelta;
	Platform::Point pt		= curr->Position;

	float	mult	= iso::pow(1.05f, wheel / 64.f);
	zoom	*= mult;
	pos		= d2d::point(pt) + (pos - pt) * mult;
	
	flags.clear(AUTO_SCALE);
	Render();
}

class EditorBitmap : public Editor {
	virtual bool Matches(const ISO::Browser &b) {
		return ViewBitmap_base::Matches(b.GetTypeDef());
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		return main.AddView(typeof<ViewBitmap>(), box(p));
	}
} editorbitmap;

}
