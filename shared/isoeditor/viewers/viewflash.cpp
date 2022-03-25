#include "base/vector.h"
#include "filetypes/2d/flash.h"
#include "filetypes/video/movie.h"
#include "iso/iso_convert.h"
#include "main.h"
#include "windows\d2d.h"
#include "windows\dib.h"
#include "viewer2d.h"

using namespace app;

ControlArrangement::Token trackbar_arrange[] = {
	ControlArrangement::VSplit(-32),
	ControlArrangement::ControlRect(0),
	ControlArrangement::ControlRect(1),
};

struct BitmapCache : map<const bitmap*, com_ptr2<ID2D1Bitmap> >	{
	ID2D1Bitmap*	Get(d2d::Target &target, const bitmap *bm) {
		com_ptr2<ID2D1Bitmap>	&d = (*this)[bm];
		if (!d)
			target.CreateBitmap(&d, bm->All());
		return d;
	}
};

struct FlashContext {
	d2d::Target		&target;
	BitmapCache		&cache;
	d2d::SolidBrush	white, textbrush;
	position2		pointer;
	bool			down;

	void	DrawShape(const flash::Object *object, flash::Shape *shape);
	void	DrawObject(const flash::Object *object, int f, param(float2x3) screen);
	void	DrawMovie(const flash::Movie *movie, int f, param(float2x3) screen);
	void	DrawFrame(const flash::Frame *frame, int f, param(float2x3) screen);

	void	Draw(ISO_ptr<void> flash, int f, param(float2x3) screen);

	bool	HitTest(const flash::Shape *shape, param(position2) c);
	bool	HitTest(const flash::Object *object, int f, param(float2x3) screen);

	FlashContext(d2d::Target &target, BitmapCache &cache, const Point &p, bool down)
		: target(target), cache(cache)
		, white(target, colour(1,1,1))
		, textbrush(target, colour(1,1,1,0.5f))
		//, pointer(float(p.x), float(p.y)), down(down)
	{}
};

//-----------------------------------------------------------------------------
//	FlashContext
//-----------------------------------------------------------------------------

bool FlashContext::HitTest(const flash::Shape *shape, param(position2) c) {
	int			n	= shape->b.Count();
	float2p		*p	= shape->b;
	position2	a(p[n - 1]);
	for (int i = 0; i < n; i++) {
		position2	b(p[i]);
		if (cross(b - a, c - a) < 0)
			return false;
		a = b;
	}
	return true;
}

bool FlashContext::HitTest(const flash::Object *object, int f, param(float2x3) screen) {
	float2x3		trans	= screen * (float2x3&)object->trans;
	ISO_ptr<void>	chr		= object->character;

	if (chr.IsType<flash::Movie>()) {
		flash::Movie	*movie	= chr;
		flash::Frame	*frame = (*movie)[min(f, movie->Count() - 1)];
		for (int i = 0, n = frame->Count(); i < n; i++) {
			if (HitTest((*frame)[i], f, trans))
				return true;
		}

	} else if (chr.IsType<flash::Shape>()) {
		return HitTest((flash::Shape*)chr, pointer / trans);

	} else if (chr.IsType<flash::Text>()) {
		flash::Text	*t		= chr;
		rectangle	rect	= rectangle(t->bounds);
		return rect.contains(pointer / trans);

	} else if (chr.IsType<anything>()) {
		anything	*a	= chr;
		position2	c	= pointer / trans;
		for (int i = 0, n = a->Count(); i < n; i++) {
			ISO_ptr<void>	p = (*a)[i];
			if (p.IsType<flash::Shape>() && HitTest((flash::Shape*)p, c))
				return true;
		}
	}
	return false;
}

void FlashContext::DrawShape(const flash::Object *object, flash::Shape *shape) {
	float2p		*pts	= shape->b;
#if 1
	com_ptr<ID2D1PathGeometry>	geom;
	com_ptr<ID2D1GeometrySink>	sink;
	target.CreatePath(&geom, &sink);

	sink->BeginFigure(d2d::point(pts[0].x, pts[0].y), D2D1_FIGURE_BEGIN_FILLED);
	sink->AddLines((d2d::point*)&pts[1], shape->b.Count() - 1);
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();

	if (shape->a.IsType<flash::Bitmap>()) {
		flash::Bitmap	*fbm	= shape->a;
		ID2D1Bitmap	*bm		= cache.Get(target, fbm->a);
		com_ptr<ID2D1BitmapBrush>	brush;
		target.CreateBrush(&brush, bm, float2x3(fbm->b) * scale(1/20.f, 1/20.f), D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP, object->col_trans.x.w);
		target.Fill(geom, brush);

	} else if (shape->a.IsType<flash::Solid>()) {
		flash::Solid	*fs	= shape->a;
		target.Fill(geom, d2d::SolidBrush(target, colour(*fs)));

	} else if (shape->a.IsType<flash::Gradient>()) {
		flash::Gradient	*gs	= shape->a;
		float2x3		m	= gs->matrix;
		com_ptr<ID2D1Brush>	brush;
		if (gs->flags & flash::Gradient::radial) {
			target.CreateBrush(&brush,
				d2d::rect(m * position2(-1, -1), m * position2(+1, +1)),
				m * vector2{gs->focal_point, 0},
				make_range_n((d2d::gradstop*)&*gs->entries, gs->entries.Count())
			);
		} else {
			target.CreateBrush(&brush,
				m * position2(-1, 0), m * position2(1, 0),
				make_range_n((d2d::gradstop*)&*gs->entries, gs->entries.Count())
			);
		}
		target.Fill(geom, brush);

	} else {
		target.Fill(geom, white);
	}
#else
	rectangle	rect(pts[0].x, pts[0].y, pts[0].x, pts[0].y);
	for (int i = 0, n = shape->b.Count(); i < n; i++)
		rect |= float2(pts[i][0], pts[i][1]);
	if (shape->a.IsType<flash::Bitmap>()) {
		flash::Bitmap	*fbm	= shape->a;
		ID2D1Bitmap	*&bm	= (ID2D1Bitmap*&)fbm->a.User();
		if (!bm)
			target.CreateBitmap(&bm, fbm->a);
		target.Draw(rect, bm);
	} else {
		target.Fill(rect, white);
	}
#endif
}

void FlashContext::DrawObject(const flash::Object *object, int f, param(float2x3) screen) {
	float2x3		trans	= screen * (float2x3&)object->trans;
	ISO_ptr<void>	chr		= object->character;

	if (chr.IsType<flash::Movie>()) {
		DrawMovie((flash::Movie*)chr, f, trans);

	} else if (chr.IsType<flash::Frame>()) {
		flash::Frame	*a	= chr;
		int			n	= a->Count();
		bool		hit	= false;

		for (int i = 0; i < n && !hit; i++) {
			flash::Object	*obj = (*a)[i];
			if (obj->flags & flash::Object::hittest)
				hit = HitTest(obj, f, trans);
		}

		uint32	mask = hit ? (down ? flash::Object::down : flash::Object::over) : flash::Object::up;
		for (int i = 0; i < n; i++) {
			flash::Object	*obj = (*a)[i];
			if (obj->flags == 0 || (obj->flags & mask))
				DrawObject(obj, f, trans);
		}

	} else if (chr.IsType<flash::Shape>()) {
		target.SetTransform(trans);
		DrawShape(object, chr);

	} else if (chr.IsType<flash::Text>()) {
		flash::Text	*t		= chr;
		rectangle	rect	= rectangle(t->bounds);
		target.SetTransform(trans);
		target.Fill(rect, textbrush);

	} else if (chr.IsType<anything>()) {
		target.SetTransform(trans);
		anything	*a = chr;
		for (int i = 0, n = a->Count(); i < n; i++) {
			ISO_ptr<void>	p = (*a)[i];
			if (p.IsType<flash::Shape>())
				DrawShape(object, p);
		}
	}
}

void FlashContext::DrawFrame(const flash::Frame *frame, int f, param(float2x3) screen) {
	int	num_objs = frame->Count();

	if (num_objs && !(*frame)[num_objs - 1].IsType<flash::Object>())
		num_objs--;

	for (int i = 0; i < num_objs; i++)
		DrawObject((*frame)[i], f, screen);
}

void FlashContext::DrawMovie(const flash::Movie *movie, int f, param(float2x3) screen) {
	DrawFrame((*movie)[min(f, movie->Count() - 1)], f, screen);
}

void FlashContext::Draw(ISO_ptr<void> flash, int f, param(float2x3) screen) {
	if (flash.IsType<flash::File>()) {
		flash::File	*file = flash;
		rectangle	rect = rectangle(file->rect);

		com_ptr<ID2D1SolidColorBrush>	background;
		target.CreateBrush(&background, colour(file->background));

		target.SetTransform(screen);
		target.Fill(rect, background);
		DrawMovie(&file->movie, f, screen);

	} else if (flash.IsType<flash::Movie>()) {
		DrawMovie(flash, f, screen);

	} else if (flash.IsType<flash::Frame>()) {
		DrawFrame(flash, f, screen);

	} else if (flash.IsType<flash::Object>()) {
		DrawObject(flash, f, screen);
	}
}

int CountFrames(ISO_ptr<void> p);

int CountFrameFrames(const flash::Frame *frame) {
	int	n = 0;
	for (int i = 0, o = frame->Count(); i < o; i++)
		n = max(n, CountFrames((*frame)[i]->character));
	return n;
}

int CountMovieFrames(const flash::Movie *movie) {
	int	n = movie->Count();
	for (int i = 0, f = n; i < f; i++)
		n = max(n, CountFrameFrames((*movie)[i]));
	return n;
}
int CountFrames(ISO_ptr<void> p) {
	return	p.IsType<flash::Movie>()	? CountMovieFrames(p)
		:	p.IsType<flash::Frame>()	? CountFrameFrames(p)
		:	1;
}

Point FrameSize(const flash::File *file) {
	return Point(file->rect.z - file->rect.x, file->rect.w - file->rect.y);
}

class ViewFlash : public Window<ViewFlash>, public WindowTimer<ViewFlash> {
	TrackBarControl		tb;
	ISO_ptr<void>		flash;
	float				fps;
	int					num;

	Viewer2D			target;
	BitmapCache			cache;
	int					frame;
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);

	ViewFlash(const WindowPos &wpos, ISO_ptr<void> p) : fps(20), frame(0) {
		flash	= p;
		if (flash.IsType<flash::File>()) {
			flash::File	*ff = flash;
			num = CountMovieFrames(&ff->movie);
			fps	= ff->framerate;
		} else {
			num = CountFrames(flash);
		}
		Create(wpos, NULL, CHILD | CLIPCHILDREN | VISIBLE, CLIENTEDGE);
	}
};

LRESULT ViewFlash::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			tb.Create(*this, NULL, VISIBLE | CHILD, NOEX, GetClientRect().Subbox(0, -32, 0, 0));
			tb(TBM_SETRANGEMIN, FALSE, 0);
			tb(TBM_SETRANGEMAX, FALSE, num - 1);
			target.Create(GetChildWindowPos(), NULL, VISIBLE | CHILD, NOEX);
			break;

		case WM_SIZE: {
			Rect	rects[2];
#if 1
			ControlArrangement::GetRects(trackbar_arrange, GetClientRect(), rects);
			target.Move(rects[0]);
			tb.Move(rects[1]);
#else
			GetClientRect().SplitAtY(-32, rects[0], rects[1]);
			tb.Move(rects[1]);
			if (target.Resize(rects[0].Size()))
				target.DeInit();
#endif
			target.Invalidate();
			break;
		}

		case WM_ISO_TIMER:
			tb.SetPos(frame = (frame + 1) % num);
			target.Invalidate();
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			SetFocus();
			SetAccelerator(*this, Accelerator());
			break;

		case WM_KEYDOWN:
			switch (wParam) {
				case VK_SPACE:
					if (IsRunning())
						Stop();
					else
						Timer::Start(1.f / fps);
					break;

				case VK_RETURN: {
					Clipboard	clip(*this);
					if (clip.Empty()) {
						int			width = 960, height = 720;
						d2d::DXI	d2d(width, height);
						BitmapCache	cache;

						d2d.BeginDraw();
						d2d.Clear(colour(0.5f,0.5f,0.5f));
						FlashContext(d2d, cache, Point(0, 0), false).Draw(flash, frame, identity);
						d2d.EndDraw();

						global_base	glob(DIB::CalcSize(width, height, 32, 1));
						DIB			*dib	= DIB::Init(glob.lock(), width, height, 32);
						copy(element_cast<DIBHEADER::RGBQUAD>(d2d.GetData()), dib->GetPixels());

						glob.unlock();
						clip.Set(CF_DIB, glob);
					}
					break;
				}
			}
			break;

		case WM_COMMAND: {
			int	id = LOWORD(wParam);
			switch (id) {
				case ID_EDIT_COPY:
					break;
			}
			break;
		}

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case d2d::PAINT: {
					auto	*info = (d2d::PAINT_INFO*)nmh;
					FlashContext(
						target, cache, ToClient(GetMousePos()), !!(GetKeyState(VK_LBUTTON) & 0x8000)
					).Draw(
						flash, frame, target.transformation()
					);
					break;
				}
			}
			break;
		}

		case WM_HSCROLL:
			switch (LOWORD(wParam)) {
				case SB_LINELEFT:
				case SB_LINERIGHT:
				case SB_THUMBTRACK:
					frame = tb.GetPos();
					target.Invalidate();
					break;
			}
			break;

		case WM_NCDESTROY:
			delete this;
			break;
		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}

class Flash_frames : public ISO::VirtualDefaults {
	Point				size;
	d2d::DXI			d2d;
	BitmapCache			cache;
	const flash::File	*flash;

	ISO_ptr<bitmap>	GetFrame(int i);
public:
	Flash_frames(const flash::File *_flash) : size(FrameSize(_flash)), d2d(size.x, size.y), flash(_flash) {}
	int				Count()			{ return CountMovieFrames(&flash->movie); }
//	ISO::Browser2	Index(int i)	{ return GetFrame(i);	}
};


ISO_ptr<bitmap>	Flash_frames::GetFrame(int i) {
	d2d.BeginDraw();
	d2d.Clear(colour(flash->background));
	FlashContext(d2d, cache, Point(0, 0), false).DrawMovie(&flash->movie, i, identity);
	d2d.EndDraw();

	auto	b = d2d.GetData();
	ISO_ptr<bitmap>	bm(0);
	bm->Create(size.x, size.y);
	copy(element_cast<Texel<B8G8R8A8>>(b), bm->All());

	return bm;
}

ISO_DEFVIRT(Flash_frames);

ISO_ptr<movie> MakeFlashMovie(ISO_ptr<flash::File> flash) {
	ISO_ptr<Flash_frames>	frames(0, flash);
	Point					size = FrameSize(flash);
	return ISO_ptr<movie>(0, frames, size.x, size.y, flash->framerate);
}

class EditorFlash : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->SameAs<flash::File>() || type->SameAs<flash::Movie>() || type->SameAs<flash::Frame>() || type->SameAs<flash::Object>();
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_ptr<void> &p) {
		return *new ViewFlash(pos, p);
	}
public:
	EditorFlash() {
		ISO_get_conversion(MakeFlashMovie);
	}
} editorflash;
