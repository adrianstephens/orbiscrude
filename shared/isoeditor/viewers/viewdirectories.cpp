#include "main.h"
#include "isoeditor/devices/directories.h"
#include "container/archive_help.h"

using namespace iso;

#include "windows/d2d.h"

namespace app {

float sum(float *p, float *e) {
	float	s = 0;
	for (float *i = p; i != e; ++i)
		s += *i;
	return s;
}

void normalise(float *p, float *e, float area) {
	float	s = area / sum(p, e);
	for (float *i = p; i != e; ++i)
		*i *= s;
}

struct Squarifier : d2d::rect {
	dynamic_array<d2d::rect> rects;

	void	add_row(float *p, float *e) {
		float	area	= sum(p, e);
		float	x		= left, y = top;

		if (Width() >= Height()) {
			float	w	= Height() == 0 ? 0 : area / Height();
			float	rw	= Height() / area;
			for (float *i = p; i < e; ++i) {
				float	h = *i * rw;
				new(rects) d2d::rect(x, y, x + w, y + h);
				y		+= h;
			}
			left	+= w;
		} else {
			float	h	= Width() == 0 ? 0 : area / Width();
			float	rh	= Width() / area;
			for (float *i = p; i < e; ++i) {
				float	w = *i * rh;
				new(rects) d2d::rect(x, y, x + w, y + h);
				x		+= w;
			}
			top		+= h;
		}
	}

	void squarify(float *start, float *next, float *end, float w) {
		float	w2		= w * w;
		float	sum		= 0;
		float	minv	= 1e38f;
		float	maxv	= 0;
		float	ratio	= 1e38f;

		for (;next != end; ++next) {
			float	add		= *next;
			if (add == 0)
				continue;

			sum		= sum + add;
			minv	= min(minv, add);
			maxv	= max(maxv, add);

			float	s2		= sum * sum;
			float	ratio1	= max((w2 * maxv) / s2, s2 / (w2 * minv));

			if (ratio >= ratio1) {
				ratio	= ratio1;

			} else {
				add_row(start, next);
				start	= next;
				w		= min(Width(), Height());
				if (w == 0)
					break;
				w2		= w * w;
				sum		= minv	= maxv	= add;
				ratio	= max(w2 / add, add / w2);
			}
		}
		add_row(start, end);
	}

	Squarifier(const d2d::rect &rect, float *p, float *e) : d2d::rect(rect) {
		normalise(p, e, Width() * Height());
		squarify(p, p, e, min(Width(), Height()));
	}
};

class TextEffectOutline : public com<d2d::TextEffect> {
	com_ptr<ID2D1SolidColorBrush> fill, stroke;
	float	width;
public:
	TextEffectOutline(ID2D1SolidColorBrush *_fill, ID2D1SolidColorBrush *_stroke, float _width) : fill(_fill), stroke(_stroke), width(_width) {
		fill->AddRef();
		stroke->AddRef();
	}
	STDMETHOD(Draw)(void *context, ID2D1DeviceContext *device, ID2D1Geometry *geometry) {
		device->DrawGeometry(geometry, stroke, width);
		device->FillGeometry(geometry, fill);
		return S_OK;
	}
};

struct ShadowBuffer : public com_ptr<ID2D1Bitmap1> {
	ShadowBuffer(ID2D1DeviceContext *device) {
		D2D1_BITMAP_PROPERTIES1	props	= {{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0, 0, D2D1_BITMAP_OPTIONS_TARGET};
		device->CreateBitmap(device->GetPixelSize(), 0, 0, &props, get_addr());
	}
};

struct ShadowDrawer {
	ID2D1DeviceContext		*device;
	ID2D1Bitmap1			*output;
	d2d::rect				bounds;
	d2d::matrix				transform;
	d2d::colour				colour;
	float					ssize;
	float					alpha;
	com_ptr<ID2D1Image>		image;

	ShadowDrawer(ID2D1DeviceContext *_device, ID2D1Bitmap1 *_output, const d2d::rect &_bounds, float size, d2d::colour _colour, float _alpha = 1) : device(_device), output(_output), bounds(_bounds), colour(_colour), alpha(_alpha) {
		device->GetTransform(&transform);
		ssize = (transform._11 + transform._12) * size;

		D2D1_SIZE_U		destsize = output->GetPixelSize();
		bounds.right	= min(bounds.right, destsize.width);
		bounds.bottom	= min(bounds.bottom, destsize.height);
		bounds.left		= max(bounds.left, 0);
		bounds.top		= max(bounds.top, 0);
		bounds			= bounds.Grow(ssize, ssize, ssize, ssize);

		if (bounds.right <= bounds.left || bounds.bottom <= bounds.top)
			return;

		device->GetTarget(&image);
		device->SetTarget(output);

		device->SetTransform(d2d::matrix(translate(-bounds.left, -bounds.top) * (float2x3)transform));
		d2d::rect		bounds2 = bounds - bounds.TopLeft();
		device->PushAxisAlignedClip(bounds2, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

		d2d::colour	clear(0,0,0,0);
		device->Clear(&clear);
	}

	~ShadowDrawer() {
		if (image) {
			device->PopAxisAlignedClip();
			device->SetTarget(image);
			device->SetTransform(d2d::matrix(float2x3(identity)));
		#if 1
			d2d::matrix5x4	cmat(
				float4{1, 0, 0, 0},
				float4{0, 1, 0, 0},
				float4{0, 0, 1, 0},
				float4{0, 0, 0, alpha},
				float4{0, 0, 0, 0}
			);
			d2d::rect		bounds2 = bounds - bounds.TopLeft();
			device->DrawImage(
				d2d::Effect(device, CLSID_D2D1ColorMatrix)
				.SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, cmat)
				//.SetValue(D2D1_COLORMATRIX_PROP_ALPHA_MODE, D2D1_COLORMATRIX_ALPHA_MODE_STRAIGHT)
				.SetInput(
				d2d::Effect(device, CLSID_D2D1Composite)
					.SetInput(0, d2d::Effect(device, CLSID_D2D1Shadow)
						.SetInput(output)
						.SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, ssize / 3)
						.SetValue(D2D1_SHADOW_PROP_COLOR, colour).get()
					)
					.SetInput(1, output)
					.get()
				).get()
				, &bounds.TopLeft()
				, &bounds2
			);
		#else
			device->DrawImage(d2d::Effect(device, CLSID_D2D1Composite)
				.SetInput(0, d2d::Effect(device, CLSID_D2D1Shadow)
					.SetInput(output)
					.SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, ssize / 3)
					.SetValue(D2D1_SHADOW_PROP_COLOR, shadow)
				)
				.SetInput(1, output)
				//.SetValue(D2D1_SHADOW_PROP_COLOR, colour(1,1,1,1))
				, &bounds.TopLeft()
			);
		#endif
			device->SetTransform(transform);
		}
	}
};

class TextEffectShadow : public com<d2d::TextEffect> {
	ID2D1Bitmap1			*output;
	com_ptr2<ID2D1SolidColorBrush>	fill;
	d2d::colour				shadow;
	float					size;
	float					alpha;
public:
	TextEffectShadow(ID2D1Bitmap1 *_output, ID2D1SolidColorBrush *_fill, const d2d::colour &_shadow, float _size) : output(_output), fill(_fill), shadow(_shadow), size(_size), alpha(1) {
	}
	void	SetAlpha(float _alpha) {
		alpha	= _alpha;
	}
	void	SetFill(ID2D1SolidColorBrush *_fill) {
		fill	= _fill;
	}

	HRESULT DrawGlyphRun(void *context, ID2D1DeviceContext *device, D2D1_POINT_2F baseline, const DWRITE_GLYPH_RUN *glyphs, const DWRITE_GLYPH_RUN_DESCRIPTION *desc, DWRITE_MEASURING_MODE measure) {
		d2d::rect			bounds;
		device->GetGlyphRunWorldBounds(baseline, glyphs, measure, &bounds);

		ShadowDrawer sd(device, output, bounds, size, shadow, alpha);
		device->DrawGlyphRun(baseline, glyphs, desc, fill, measure);
		return S_OK;
	}
	HRESULT DrawGeometry(void *context, ID2D1DeviceContext *device, ID2D1Geometry *geometry) {
		d2d::matrix			transform;
		d2d::rect			bounds;
		device->GetTransform(&transform);
		geometry->GetBounds(&transform, &bounds);

		ShadowDrawer sd(device, output, bounds, size, shadow, alpha);
		device->FillGeometry(geometry, fill);
		return S_OK;
	}
};

d2d::point OptimiseLayout(IDWriteTextLayout *layout, float width, float height) {
	d2d::point	bestsize;
	float		bestr	= 0;

	for (float w0 = 0, w1 = 10000;;) {
		DWRITE_TEXT_METRICS	metrics;
		layout->GetMetrics(&metrics);

		float	a	= width * metrics.height, b = height * metrics.width;
		float	r	= a < b ? a / b : b / a;

//		if (r == bestr)
//			break;

		if (r > bestr) {
			bestr		= r;
			bestsize	= d2d::point(metrics.width, metrics.height);
		}

		if (a < b) {
			if (w1 == metrics.width)
				break;
			w1 = metrics.width;
		} else {
			if (w0 == metrics.width)
				break;
			w0 = metrics.width;
		}

		layout->SetMaxWidth((w0 + w1) / 2);
	}
	layout->SetMaxWidth(bestsize.x);
	layout->SetMaxHeight(bestsize.y);
	return bestsize;
}

struct Drawer {
	static	d2d::gradstop	grad[2];

	int							mode;
	d2d::Write					write;
	d2d::Target					&target;
	d2d::rect					visible;
	const dirs::Dir*			selected_dir;
	const dirs::Entry*			selected_entry;
	float						minsize;
	float						rootsize;

	d2d::SolidBrush				black, white, selected, file;
	d2d::GradientStops			shadow_g;
	d2d::RadialGradientBrush	shadow_corner;
	d2d::LinearGradientBrush	shadow_edge;
	d2d::Font					font;

	ShadowBuffer				shadow_output;
	com_ptr<TextEffectShadow>	shadow;
	com_ptr<d2d::TextRenderer>	text_renderer;

	float	Draw(const dirs::Dir &dir, const d2d::rect &rect);
	void	Draw(const dirs::Entry &entry, const d2d::rect &rect);

	void	DrawShadowRect(const d2d::rect &rect, ID2D1Brush *fill, float size);

	Drawer(d2d::Target &_target, const d2d::rect &root, const d2d::rect &_visible, const dirs::Dir *_selected_dir, const dirs::Entry *_selected_entry)
		: mode(1), target(_target)
		, visible(_visible), selected_dir(_selected_dir), selected_entry(_selected_entry)
		, black(target,		colour(0,0,0))
		, white(target,		colour(1,1,1))
		, selected(target,	colour(1,0,0))
		, file(target,		colour(0.7f,0.7f,0.7f))
		, shadow_g(target,	grad)
		, shadow_corner(target,	d2d::rect(-1,-1,1,1), d2d::point(0,0), shadow_g)
		, shadow_edge(target,	d2d::point(0,0), d2d::point(1,0), shadow_g)
		, font(write, L"arial", .1f)
		, shadow_output(target)
		, shadow(new TextEffectShadow(shadow_output, white, colour(0,0,0,2), 0.1f))
		, text_renderer(new d2d::TextRenderer(target, black))
	{
		font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

		minsize		= 16;
		rootsize	= (root.Width() * root.Height());
	}
};

d2d::gradstop	Drawer::grad[] = {
	{0, d2d::colour(0,0,0,0.25f)},
	{1, d2d::colour(0,0,0,0)},
};

void Drawer::DrawShadowRect(const d2d::rect &rect, ID2D1Brush *fill, float size) {
	target.Fill(rect, fill);

	shadow_corner.SetTransform(translate(rect.left, rect.top) * scale(size));
	target.Fill(d2d::rect(rect.left - size, rect.top - size, rect.left, rect.top), shadow_corner);

	shadow_corner.SetTransform(translate(rect.right, rect.top) * scale(size));
	target.Fill(d2d::rect(rect.right, rect.top - size, rect.right + size, rect.top), shadow_corner);

	shadow_corner.SetTransform(translate(rect.left, rect.bottom) * scale(size));
	target.Fill(d2d::rect(rect.left - size, rect.bottom, rect.left, rect.bottom + size), shadow_corner);

	shadow_corner.SetTransform(translate(rect.right, rect.bottom) * scale(size));
	target.Fill(d2d::rect(rect.right, rect.bottom, rect.right + size, rect.bottom + size), shadow_corner);

	shadow_edge.SetTransform(translate(0, rect.top) * rotate2D(-pi / two) * scale(size));
	target.Fill(d2d::rect(rect.left, rect.top - size, rect.right, rect.top), shadow_edge);

	shadow_edge.SetTransform(translate(rect.left, 0) * rotate2D(pi) * scale(size));
	target.Fill(d2d::rect(rect.left - size, rect.top, rect.left, rect.bottom), shadow_edge);

	shadow_edge.SetTransform(translate(0, rect.bottom) * rotate2D(pi / two) * scale(size));
	target.Fill(d2d::rect(rect.left, rect.bottom, rect.right, rect.bottom + size), shadow_edge);

	shadow_edge.SetTransform(translate(rect.right, 0) * scale(size));
	target.Fill(d2d::rect(rect.right, rect.top, rect.right + size, rect.bottom), shadow_edge);
}


void Drawer::Draw(const dirs::Entry &entry, const d2d::rect &rect) {
	if (!rect.Overlaps(visible) || !entry.name)
		return;

	float		w		= rect.Width(), h = rect.Height();
	float		e		= min(w, h) / 10;

	switch (mode) {
		case 0:
			if (min(w, h) < 8) {
				target.SetTransform(translate(rect.left, rect.top));
				target.Fill(d2d::rect(e, e, w - e, h - e), &entry == selected_entry ? selected : file);
				return;
			}
			break;
		case 1:
			if (min(w, h) < 8)
				return;
	}

	float		sqrta	= sqrt(w * h);
	float2x3	mat		= translate(rect.left, rect.top) * scale(sqrta);

	if (h > w * 2) {
		mat		= translate(w, 0) * mat * rotate2D(pi / two);
		swap(w, h);
	}

	target.SetTransform(mat);

	d2d::rect		srect(e / sqrta, e / sqrta, (w - e) / sqrta, (h - e) / sqrta);

//	target.Fill(srect, &entry == selected_entry ? selected : file);
	DrawShadowRect(srect, &entry == selected_entry ? selected : file, 0.05f);
	target.DrawText(srect, str16(entry.name), font, white, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

float Drawer::Draw(const dirs::Dir &dir, const d2d::rect &_rect) {
	float	leave	= 0.5f;
	if (!_rect.Overlaps(visible))
		return leave;

	d2d::rect		rect	= _rect;
	float		w		= rect.Width(), h = rect.Height();

	// adjust rect & draw
	switch (mode) {
		case 0:
			if (min(w, h) < 8) {
				target.SetTransform(identity);
				target.Fill(rect, &dir == selected_dir ? selected : file);
				return leave;
			}
			break;
		case 1: {
			rect	= rect.Adjust(.95f, .95f, .1f, .1f);
			w		= rect.Width();
			h		= rect.Height();
			float	sqrta	= sqrt(w * h);
			target.SetTransform(translate(rect.left, rect.top) * scale(sqrta));
			DrawShadowRect(d2d::rect(0, 0, w / sqrta, h / sqrta), &dir == selected_dir ? selected : file, 0.05f);
			if (min(w, h) < 8)
				return leave;
			break;
		}
	}

	dynamic_array<float>	areas(dir.subdirs.size() + dir.entries.size());
	float	*a = areas;

	for (auto i = dir.subdirs.begin(), e = dir.subdirs.end(); i != e; ++i, ++a)
		*a = (*i)->size;

	for (auto i = dir.entries.begin(), e = dir.entries.end(); i != e; ++i, ++a)
		*a = (*i)->size;

	Squarifier	sq(rect, areas.begin(), areas.end());

	d2d::rect	*r		= sq.rects;
	for (auto i = dir.subdirs.begin(), e = dir.subdirs.end(); i != e; ++i, ++r) {
		float	fudge = min(dir.size * 0.5f / (*i)->size, 1);
		leave = min(leave, Draw(**i, *r) * fudge);
	}

	for (auto i = dir.entries.begin(), e = dir.entries.end(); i != e; ++i, ++r)
		Draw(**i, *r);


	float	relsize	= w * h / rootsize;
	if (relsize > leave / 10 && relsize < 0.5f && dir.name && dir.name != ".") {
		float2x3	mat		= translate(rect.left, rect.top);
		if (h > w * 2) {
			mat		= translate(w, 0) * mat * rotate2D(pi / two);
			swap(w, h);
		}

		float		alpha	= &dir == selected_dir
			? 1
			:	min(min(min(
					max(linearstep(leave, leave * 0.9f, relsize), 0.1f),
					linearstep(0.5f, 0.45f, relsize)
				),	linearstep(leave * 0.1f, leave * 0.2f, relsize)
			),	1);

		switch (mode) {
			case 0: {
				d2d::TextLayout	layout(write, str16(dir.name), font, 1000, 1000);
				d2d::point		size = OptimiseLayout(layout, w, h);
				target.SetTransform(mat * scale(float2{w, h} / size));

				shadow->SetAlpha(alpha);
				layout->SetDrawingEffect(shadow, d2d::TextLayout::Range(0, dir.name.size32()));
				layout->Draw(NULL, text_renderer, 0, 0);
				break;
			}
			case 1: {
				float		sqrta	= sqrt(w * h);
				target.SetTransform(mat * scale(sqrta));

				d2d::TextLayout	layout(write, str16(dir.name), font, w / sqrta, h / sqrta);
				shadow->SetAlpha(alpha);
				layout->SetDrawingEffect(shadow, d2d::TextLayout::Range(0, dir.name.size32()));
				if (&dir == selected_dir) {
					shadow->SetFill(selected);
					layout->Draw(NULL, text_renderer, 0, 0);
					shadow->SetFill(white);
				} else {
					layout->Draw(NULL, text_renderer, 0, 0);
				}
				break;
			}
		}
	}
	return leave;
}

struct Finder {
	int					mode;
	d2d::point			mouse;
	d2d::rect			rect;
	const dirs::Dir		*dir;
	const dirs::Entry	*entry;

	bool	Check(const dirs::Dir &dir, const d2d::rect &rect);
	bool	Check(const dirs::Entry &entry, const d2d::rect &rect);
	Finder(const d2d::point &mouse) : mode(1), mouse(mouse), rect(0, 0, 0, 0), dir(0), entry(0) {}
};

bool Finder::Check(const dirs::Entry &entry, const d2d::rect &rect) {
	if (rect.Contains(mouse)) {
		float		w		= rect.Width(), h = rect.Height();
		float		e		= min(w, h) / 10;
		if (min(w, h) >= 8 && rect.Grow(-e,-e,-e,-e).Contains(mouse)) {
			this->rect	= rect;
			this->entry	= &entry;
			return true;
		}
	}
	return false;
}

bool Finder::Check(const dirs::Dir &dir, const d2d::rect &_rect) {
	d2d::rect		rect	= _rect;

	switch (mode) {
		case 1: {
			rect	= rect.Adjust(.95f, .95f, .1f, .1f);
			break;
		}
	}

	if (rect.Contains(mouse)) {
		if (min(rect.Width(), rect.Height()) >= 8) {
			dynamic_array<float>	areas(dir.subdirs.size() + dir.entries.size());
			float	*a = areas;

			for (auto i = dir.subdirs.begin(), e = dir.subdirs.end(); i != e; ++i, ++a)
				*a = (*i)->size;

			for (auto i = dir.entries.begin(), e = dir.entries.end(); i != e; ++i, ++a)
				*a = (*i)->size;

			Squarifier	sq(rect, areas.begin(), areas.end());

			d2d::rect	*r	= sq.rects;
			for (auto i = dir.subdirs.begin(), e = dir.subdirs.end(); i != e; ++i, ++r) {
				if (Check(**i, *r))
					return true;
			}

			for (auto i = dir.entries.begin(), e = dir.entries.end(); i != e; ++i, ++r) {
				if (Check(**i, *r))
					return true;
			}
		}
		this->rect	= rect;
		this->dir	= &dir;
		return true;
	}
	return false;
}

class ViewDirectories : public Window<ViewDirectories> {
	dirs::Dir			*root;
	d2d::WND			target;
	d2d::rect			rect;
	float				zoom;
	Point				pos;
	Point				prevmouse;

	d2d::rect			selected_rect;
	const dirs::Dir		*selected_dir;
	const dirs::Entry	*selected_entry;

	bool CreateDeviceResources() {
		return target.Init(hWnd, GetClientRect().Size());
	}
	void DiscardDeviceResources() {
		target.DeInit();
	}

	static Rect CalcRect(param(rectangle) r) {
		float2	p0		= floor(r.a.v);
		float2	p1		= ceil(r.b.v) - p0;
		return Rect(p0.x, p0.y, p1.x, p1.y);
	}
	float2x3	CalcMat() const {
		return translate(position2(pos.x, pos.y)) * scale(zoom);
	}

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	ViewDirectories(const WindowPos &pos, dirs::Dir *root) : root(root), pos(0,0), zoom(1), selected_dir(0), selected_entry(0) {
		Create(pos, NULL, CHILD | VISIBLE);
	}
};

LRESULT ViewDirectories::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			break;

		case WM_SIZE: {
			Point	size(lParam);
			if (target.Resize(size))
				DiscardDeviceResources();

			if (zoom == 1 && pos == Point(0,0))
				rect = GetClientRect();

			Invalidate();
			return 0;
		}

		case WM_LBUTTONDOWN:
			prevmouse	= Point(lParam);
			SetFocus();
			break;

		case WM_LBUTTONDBLCLK:
			zoom	= 1;
			pos		= Point(0, 0);
			Invalidate();
			return 0;

		case WM_MOUSEMOVE: {
			Point	mouse	= Point(lParam);
			if (wParam & MK_LBUTTON) {
				pos			+= mouse - prevmouse;
				prevmouse	= mouse;
				Invalidate();
			} else {
				TrackMouse(TME_LEAVE);
				float2x3	mat		= CalcMat();
				Finder		finder(inverse(mat) * (position2)(float2)d2d::point(mouse));
				if (finder.Check(*root, rect)) {
					if (selected_dir || selected_entry) {
						if (selected_dir ? (selected_dir == finder.dir) : (selected_entry == finder.entry))
							return 0;

						Invalidate(CalcRect(mat * selected_rect));
					}

					d2d::rect	sel		= mat * finder.rect;
					selected_rect	= finder.rect;
					selected_dir	= finder.dir;
					selected_entry	= finder.entry;
					Invalidate(CalcRect(mat * selected_rect));

				} else if (selected_dir || selected_entry) {
					Invalidate(CalcRect(mat * selected_rect));
					selected_dir	= 0;
					selected_entry	= 0;
				}
			}
			return 0;
		}

		case WM_MOUSELEAVE:
			if (selected_dir || selected_entry) {
				Invalidate(CalcRect(CalcMat() * selected_rect));
				selected_dir	= 0;
				selected_entry	= 0;
			}
			return 0;

		case WM_MOUSEWHEEL:
			if (GetRect().Contains(Point(lParam))) {
				Point	pt		= ToClient(Point(lParam));
				float	mult	= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				zoom	*= mult;
				pos		= pt + (pos - pt) * mult;
				Invalidate();
				return 0;
			}
			break;

		case WM_KEYDOWN:
			switch (wParam) {
				case VK_UP:
					Invalidate();
					break;

				case VK_DOWN:
					Invalidate();
					break;
			}
			break;

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			if (CreateDeviceResources() && !target.Occluded()) {
				target.BeginDraw();
				target.SetTransform(identity);
				target.device->PushAxisAlignedClip(d2d::rect(dc.GetDirtyRect()), D2D1_ANTIALIAS_MODE_ALIASED);

				target.Clear(colour(0.1f,0.1f,0.1f));

				Drawer		draw(target, rect, dc.GetDirtyRect(), selected_dir, selected_entry);
				d2d::rect		full(CalcMat() * rect);
				draw.Draw(*root, full);

				target.device->PopAxisAlignedClip();
				if (target.EndDraw())
					DiscardDeviceResources();
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			break;

		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}

dirs::Dir	*MakeDir(ISO_ptr<iso::Folder> f) {
	auto	d = new dirs::Dir(f.ID().get_tag());
	for (auto& i : *f) {
		if (i.IsType<iso::Folder>()) {
			auto	d2 = MakeDir(i);
			d->subdirs.push_back(d2);
			d->size += d2->size;
		} else {
			uint32	size = ISO::Browser2(i).Count();
			d->entries.push_back(new dirs::Entry(i.ID().get_tag(), size));
			d->size += size;
		}

	}
	return d;
}


class EditorDirs : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return	type->SameAs<dirs::Dir>()
			||	type->SameAs<iso::Folder>();

		//			/*type->SameAs<dirs::NTFSFiles>()
//			|| type->SameAs<dirs::FATFiles>()
//			|| */type->SameAs<dirs::Dir>();
	}
	virtual Control	Create(MainWindow &main, const WindowPos &pos, const ISO_VirtualTarget &b) {
		if (b.Is<iso::Folder>()) {
			return *new ViewDirectories(pos, MakeDir(b));

		}

/*		if (b.Is<dirs::NTFSFiles>()) {
			dirs::NTFSFiles *p = b;
			return *new ViewDirectories(pos, &p->dirs[NTFS::FILE_ID::Root]);
		} else if (b.Is<dirs::FATFiles>()) {
			dirs::FATFiles *p = b;
			return *new ViewDirectories(pos, p->root);
		} else */{
			return *new ViewDirectories(pos, b);
		}
	}
public:
} editor_dirs;
}
