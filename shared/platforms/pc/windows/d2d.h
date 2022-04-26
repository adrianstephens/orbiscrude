#ifndef D2D_H
#define D2D_H

#include "maths/geometry.h"

#ifdef PLAT_WINRT
#include "winrt/window.h"
#include <windows.ui.xaml.media.dxinterop.h>
#else
#include "window.h"
#endif

#include "extra\xml.h"
#include "filetypes\bitmap\bitmap.h"

#if _MSC_VER < 1700
#include <d2d1.h>
#include "d3d10.h"
#pragma comment(lib, "d2d1.lib")
#else
#define _D2D1_1HELPER_H_
#include <d2d1_1.h>
#include <d2d1effects_2.h>
#include <D2D1EffectAuthor.h>
#include <d3d11_1.h>
#pragma comment(lib, "d2d1.lib")
#endif

#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")

//#include <wincodec.h>

#include "com.h"

namespace iso {

namespace d2d {
	struct point;
}

namespace simd {
	template<> struct element_type_s<d2d::point>	{ typedef float type; };
	template<> struct element_type_s<win::Point>	{ typedef int type; };

	inline int32x2 as_vec(const win::Point &p)	{ return {p.x, p.y}; }
	template<> static constexpr bool is_vec<d2d::point> = true;
}

template<> static constexpr int num_elements_v<d2d::point> = 2;
template<> static constexpr int num_elements_v<win::Point> = 2;

namespace d2d {

//-----------------------------------------------------------------------------
//	structures
//-----------------------------------------------------------------------------

#ifndef _D2D1_1_H_
typedef ID2D1RenderTarget	ID2D1DeviceContext;
#endif

struct texel {
	uint8 b, g, r, a;
	texel()	{}
	texel(uint8 r, uint8 g, uint8 b, uint8 a = 255) : b(b), g(g), r(r), a(255) {}
	texel&	operator=(const ISO_rgba &c) { b = c.b; g = c.g; r = c.r; a = c.a; return *this; }
};

struct colour : D2D1_COLOR_F {
	colour()										{}
	colour(float _r, float _g, float _b, float _a=1){ r = _r; g = _g; b = _b; a = _a; }
	colour(COLORREF c)								{ r = GetRValue(c) / 255.f; g = GetGValue(c) / 255.f; b = GetBValue(c) / 255.f; a = 1; }
	colour(win::Colour c)							{ *this = (COLORREF)c;		}
	colour(param(iso::colour) c)					{ *this = (colour&)c;		}
	template<typename R, typename G, typename B> colour(const iso::colour::colour_const<R, G, B>&) : colour(R(), G(), B()) {}
	operator iso::colour() const					{ return iso::colour(load<float4>(&r));	}
	operator COLORREF() const						{ return RGB(int(clamp(r, 0, 1) * 255), int(clamp(g, 0, 1) * 255), int(clamp(b, 0, 1) * 255));	}
};

struct point : D2D_POINT_2F {
	point()												{}
	point(const D2D_POINT_2F &p)	: D2D_POINT_2F(p)	{}
	point(const D2D_SIZE_F &p)		: D2D_POINT_2F((D2D_POINT_2F&)p)	{}
	point(float _x, float _y)						{ x = _x; y = _y; }
	point(param(position2) p)						{ x = p.v.x; y = p.v.y;	}
	point(param(float2) p)							{ x = p.x; y = p.y; }
	point(param(iso::point) p)						{ x = p.x; y = p.y; }
	point(param(win::Point) p)						{ x = p.x; y = p.y; }
#ifdef PLAT_WINRT
	point(const iso_winrt::Platform::Point &p)		{ x = p.X; y = p.Y; }
#endif
	operator float2()						const	{ return {x, y}; }
	operator position2()					const	{ return {x, y}; }
	operator const D2D1_SIZE_F&()			const	{ return *(D2D1_SIZE_F*)this;	}
	operator D2D1_SIZE_U() const					{ D2D1_SIZE_U u = {(UINT)x, (UINT)y}; return u;	}

	point	operator*(float b)				const	{ return {x * b, y * b}; }
	point	operator/(float b)				const	{ return *this * reciprocal(b); }
	point&	operator+=(const point &b)				{ x += b.x; y += b.y; return *this; }
	point&	operator-=(const point &b)				{ x -= b.x; y -= b.y; return *this; }

	friend point operator-(const point &a, const point &b)	{ return point(a.x - b.x, a.y - b.y); }
	friend point operator+(const point &a, const point &b)	{ return point(a.x + b.x, a.y + b.y); }
	friend point align(const point &p, const point &a)		{ return point(p.x - mod(p.x, a.x), p.y - mod(p.y, a.y)); }
	friend float2	as_vec(const point &p)					{ return {p.x, p.y}; }

};

struct rect : D2D_RECT_F {
	static rect with_ext(float x, float y, float w, float h) { return rect(x, y, x + w, y + h); }

	rect()											{}
	rect(decltype(infinity))						{ left = top = minimum; right = bottom = maximum; }
	rect(float x0, float y0, float x1, float y1)	{ left = x0; top = y0; right = x1; bottom = y1; }
	rect(const point &a, const point &b)			{ left = a.x; top = a.y; right = b.x; bottom = b.y; }
	rect(param(rectangle) r)						{ memcpy(this, &r, sizeof(*this)); }
	rect(param(win::Rect) r)						{ left = r.Left(); top = r.Top(); right = r.Right(); bottom = r.Bottom(); }
#ifdef PLAT_WINRT
	rect(const iso_winrt::Platform::Rect &p)		{ left = p.X; top = p.Y; right = p.X + p.Width; bottom = p.Y + p.Height; }
#endif
	operator rectangle() const						{ return rectangle(*(float4p*)this); }

	float		Width()						const	{ return right - left; }
	float		Height()					const	{ return bottom - top; }
	const point& TopLeft()					const	{ return ((point*)this)[0]; }
	const point& BottomRight()				const	{ return ((point*)this)[1]; }
	point		BottomLeft()				const	{ return point(left, bottom); }
	point		TopRight()					const	{ return point(right, top); }
	point		Size()						const	{ return point(Width(), Height());	}
	point		Centre()					const	{ return point((left + right) / 2, (top + bottom) / 2); }
	rect		Grow(float x0, float y0, float x1, float y1) const {
		return rect(left - x0, top - y0, right + x1, bottom + y1);
	}
	rect		Adjust(float xs, float ys, float xp, float yp) const {
		float	w	= Width(), h = Height();
		return rect(left + w * (1 - xs) * xp, top + h * (1 - ys) * yp, left + w * xs, top + h * ys);
	}

	rect		Subbox(float x, float y, float w, float h) const {
		rect r;
		r.left		= x < 0 ? max(right  + x, left)		: min(left	 + x, right);
		r.top		= y < 0 ? max(bottom + y, top)		: min(top	 + y, bottom);
		r.right		= w > 0 ? min(r.left + w, right)	: max(right  + w, left);
		r.bottom	= h > 0 ? min(r.top  + h, bottom)	: max(bottom + h, top);
		return r;
	}
	bool		Overlaps(const rect &r)		const {
		return r.left < right && r.right > left && r.top < bottom && r.bottom > top;
	}
	bool		Contains(const point &p)		const {
		return p.x >= left && p.x < right && p.y >= top && p.y < bottom;
	}

	rect&	operator&=(const rect &r)	{ left = max(left, r.left); right = min(right, r.right); top = max(top, r.top); bottom = min(bottom, r.bottom); return *this; }

	friend rect operator+(const rect &r, const point &p)	{ return rect(r.TopLeft() + p, r.BottomRight() + p); }
	friend rect operator-(const rect &r, const point &p)	{ return rect(r.TopLeft() - p, r.BottomRight() - p); }
	friend rect	operator&(const rect &a, const rect &b)		{ return rect(a) &= b; }
	friend rect	operator*(const rect &a, float b)			{ return rect(a.left * b, a.top * b, a.right * b, a.bottom * b); }
};

struct vector2 : D2D_VECTOR_2F {
	vector2()											{}
	vector2(const D2D_VECTOR_2F &p) : D2D_VECTOR_2F(p)	{}
	vector2(float _x, float _y)							{ x = _x; y = _y; }
	vector2(param(float2) p)							{ x = p.x; y = p.y; }
};

struct vector3 : D2D_VECTOR_3F {
	vector3()											{}
	vector3(const D2D_VECTOR_3F &p) : D2D_VECTOR_3F(p)	{}
	vector3(float _x, float _y, float _z)				{ x = _x; y = _y; z = _z;	}
	vector3(param(float3) p)							{ x = p.x; y = p.y;	z = p.z; }
};

struct vector4 : D2D_VECTOR_4F {
	vector4()											{}
	vector4(const D2D_VECTOR_4F &p) : D2D_VECTOR_4F(p)	{}
	vector4(float _x, float _y, float _z, float _w)		{ x = _x; y = _y; z = _z; w = _w; }
	vector4(param(float4) p)							{ x = p.x; y = p.y;	z = p.z; w = p.w; }
	float4 to_float4() const							{ return load<float4>(&x); }
};

struct ellipse : D2D1_ELLIPSE {
	ellipse(param(rectangle) r)							{ float2 size = r.half_extent(); point = d2d::point(r.centre()); radiusX = size.x; radiusY = size.y; }
	ellipse(param(circle) c)							{ point = d2d::point(c.centre()); radiusX = radiusY = c.radius(); }
	ellipse(const d2d::point &c, float r)				{ point = c; radiusX = radiusY = r; }
	ellipse(const d2d::point &c, float rx, float ry)	{ point = c; radiusX = rx; radiusY = ry; }
	ellipse(const d2d::point &c, const d2d::point &r)	{ point = c; radiusX = r.x; radiusY = r.y; }
};

struct bezier_segment : D2D1_BEZIER_SEGMENT {
	bezier_segment(const point &p1, const point &p2, const point &p3) {
		point1 = p1;
		point2 = p2;
		point3 = p3;
	};
};

struct triangle : D2D1_TRIANGLE {
	triangle(param(iso::triangle) t) {
		point1 = point(t.corner(0));
		point2 = point(t.corner(1));
		point3 = point(t.corner(2));
	}
	triangle(const point &p1, const point &p2, const point &p3) {
		point1 = p1;
		point2 = p2;
		point3 = p3;
	};
};

struct matrix : D2D1_MATRIX_3X2_F {
	matrix()											{}
	matrix(const float2x3 &m)							{ *(D2D1_MATRIX_3X2_F*)this = (D2D1_MATRIX_3X2_F&)m; }
	operator float2x3() const							{ return float2x3(load<float4>(&_11), load<float2>(&_31)); }
};

#ifdef _D2D1_1_H_
struct matrix5x4 : D2D1_MATRIX_5X4_F {
	matrix5x4(param(float4) r, param(float4) g, param(float4) b, param(float4) a, param(float4) add) {
		(float4p&)_11 = r;
		(float4p&)_21 = g;
		(float4p&)_31 = b;
		(float4p&)_41 = a;
		(float4p&)_51 = add;
	}
	matrix5x4(param(float4x4) m, param(float4) offset) {
		(float4p&)_11 = m.x;
		(float4p&)_21 = m.y;
		(float4p&)_31 = m.z;
		(float4p&)_41 = m.w;
		(float4p&)_51 = offset;
	}
	float4p&	operator[](int i) { return *(float4p*)m[i]; }
};
#endif

struct gradstop {
	float	d;
	colour	col;
};

struct GradientStops : com_ptr<ID2D1GradientStopCollection> {
	inline bool	Create(ID2D1DeviceContext *device, const gradstop *cols, int num, D2D1_EXTEND_MODE extend = D2D1_EXTEND_MODE_CLAMP);
	GradientStops(ID2D1DeviceContext *device, const range<gradstop*> &grads, D2D1_EXTEND_MODE extend = D2D1_EXTEND_MODE_CLAMP)						{ Create(device, grads.begin(), int(grads.size()), extend); }
	GradientStops(ID2D1DeviceContext *device, const gradstop *cols, int num, D2D1_EXTEND_MODE extend = D2D1_EXTEND_MODE_CLAMP)						{ Create(device, cols, num, extend); }
//	template<int N> GradientStops(ID2D1DeviceContext *device, const array<gradstop, N> &col, D2D1_EXTEND_MODE extend = D2D1_EXTEND_MODE_CLAMP){ Create(device, cols, N, extend); }
//	template<int N> GradientStops(ID2D1DeviceContext *device, const gradstop (&cols)[N], D2D1_EXTEND_MODE extend = D2D1_EXTEND_MODE_CLAMP)			{ Create(device, cols, N, extend); }
};

//-----------------------------------------------------------------------------
// CreateBitmap
//-----------------------------------------------------------------------------

template<typename T> bool CreateBitmap(ID2D1DeviceContext *device, ID2D1Bitmap **bm, const block<T, 2> &block) {
	uint32			width	= block.template size<1>(), height = block.template size<2>();
	malloc_block	buffer(sizeof(texel) * width * height);
	copy(block, make_block((texel*)buffer, width, height));

	D2D1_BITMAP_PROPERTIES	props	= {{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0, 0};
	D2D1_SIZE_U				size	= {width, height};
	return SUCCEEDED(device->CreateBitmap(size, buffer, width * sizeof(texel), props, bm));
}

template<typename T> bool FillBitmap(ID2D1DeviceContext *device, ID2D1Bitmap *bm, const block<T, 2> &block) {
	uint32			width	= block.template size<1>(), height = block.template size<2>();
	malloc_block	buffer(sizeof(texel) * width * height);
	copy(block, make_block((texel*)buffer, width, height));
	return SUCCEEDED(bm->CopyFromMemory(0, buffer, width * sizeof(texel)));
}

//-----------------------------------------------------------------------------
//	Target
//-----------------------------------------------------------------------------

struct Target {
	com_ptr<ID2D1Factory1>			factory;
	com_ptr<ID2D1DeviceContext>		device;

	operator ID2D1DeviceContext*()	const	{ return device; }
//	operator ID2D1Factory*()		const	{ return factory; }

	//-------------
	// bitmaps
	//-------------

	bool CreateBitmap(ID2D1Bitmap **bm, uint32 width, uint32 height, DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE alpha = D2D1_ALPHA_MODE_PREMULTIPLIED) {
		D2D1_BITMAP_PROPERTIES	props	= {{format, alpha}, 0, 0};
		D2D1_SIZE_U				size	= {width, height};
		return SUCCEEDED(device->CreateBitmap(size, props, bm));
	}

	template<typename T> bool CreateBitmap(ID2D1Bitmap **bm, const block<T, 2> &block) {
		return d2d::CreateBitmap(device, bm, block);
	}

	template<typename T> bool FillBitmap(ID2D1Bitmap *bm, const block<T, 2> &block) {
		return d2d::FillBitmap(device, bm, block);
	}

	//-------------
	// brushes
	//-------------
	//bitmap
	bool CreateBrush(ID2D1BitmapBrush **brush, ID2D1Bitmap *bm, const matrix &mat, D2D1_EXTEND_MODE xmode=D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE ymode=D2D1_EXTEND_MODE_WRAP, float alpha=1, bool bilinear=true) {
		D2D1_BITMAP_BRUSH_PROPERTIES	props1 = {xmode, ymode, D2D1_BITMAP_INTERPOLATION_MODE(bilinear)};
		D2D1_BRUSH_PROPERTIES			props2 = {alpha, mat};
		return SUCCEEDED(device->CreateBitmapBrush(bm, &props1, &props2, brush));
	}
	//solid colour
	bool CreateBrush(ID2D1SolidColorBrush **brush, const colour &col) {
		return SUCCEEDED(device->CreateSolidColorBrush(&col, NULL, brush));
	}
	//linear gradient
	bool CreateBrush(ID2D1LinearGradientBrush **brush, const point &p0, const point &p1, const range<gradstop*> &grads) {
		D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES	props	= {p0, p1};
		return SUCCEEDED(device->CreateLinearGradientBrush(&props, NULL, GradientStops(device, grads), brush));
	}
	//radial gradient
	bool CreateBrush(ID2D1RadialGradientBrush **brush, const rect &r, const point &offset, const range<gradstop*> &grads) {
		D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES	props;
		props.center	= r.Centre();
		props.radiusX	= (r.right - r.left) / 2;
		props.radiusY	= (r.top - r.bottom) / 2;
		props.gradientOriginOffset	= offset;
		return SUCCEEDED(device->CreateRadialGradientBrush(&props, NULL, GradientStops(device, grads), brush));
	}

	//create brush
	bool CreateBrush(ID2D1Brush **brush, ID2D1Bitmap *bm, const matrix &mat, D2D1_EXTEND_MODE xmode=D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE ymode=D2D1_EXTEND_MODE_WRAP, float alpha=1, bool bilinear=true) {
		return CreateBrush((ID2D1BitmapBrush**)brush, bm, mat, xmode, ymode, alpha, bilinear);
	}
	bool CreateBrush(ID2D1Brush **brush, const colour &col) {
		return CreateBrush((ID2D1SolidColorBrush**)brush, col);
	}
	bool CreateBrush(ID2D1Brush **brush, const point &p0, const point &p1, const range<gradstop*> &grads) {
		return CreateBrush((ID2D1LinearGradientBrush**)brush, p0, p1, grads);
	}
	bool CreateBrush(ID2D1Brush **brush, const rect &rect, const point &offset, const range<gradstop*> &grads) {
		return CreateBrush((ID2D1RadialGradientBrush**)brush, rect, offset, grads);
	}

	//-------------
	// path
	//-------------
	void CreatePath(ID2D1PathGeometry **path, ID2D1GeometrySink **sink = 0) {
		factory->CreatePathGeometry(path);
		if (sink)
			(*path)->Open(sink);
	}


#ifdef _D2D1_1_H_
	//-------------
	// effect
	//-------------
	bool CreateEffect(ID2D1Effect **effect, const IID &iid) {
		return SUCCEEDED(device->CreateEffect(iid, effect));
	}
#endif

	//-------------
	// Target
	//-------------
	Target()					{
#if 0//def _DEBUG
		D2D1_FACTORY_OPTIONS options;
		options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &factory);
#else
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
#endif
	}

	void		DeInit()			{ device.clear(); }
	win::Point	PixelSize()	const	{ if (device) return force_cast<win::Point>(device->GetPixelSize()); return win::Point(0, 0); }
	point		Size()		const	{ if (device) return device->GetSize(); return point(0, 0); }
	void		UseDIPS()	const	{ device->SetUnitMode(D2D1_UNIT_MODE_DIPS); }
	void		UsePixels() const	{ device->SetUnitMode(D2D1_UNIT_MODE_PIXELS); }

#if 0
	point		ToPixels(const point &p)			const	{ float x, y; factory->GetDesktopDpi(&x, &y); return point(p.x * x / 96, p.y * y / 96); }
	point		FromPixels(const point &p)			const	{ float x, y; factory->GetDesktopDpi(&x, &y); return point(p.x * 96 / x, p.y * 96 / y); }
	rect		ToPixels(const rect &p)				const	{ float x, y; factory->GetDesktopDpi(&x, &y); return rect(p.left * x / 96, p.top * y / 96, p.right * x / 96, p.bottom * y / 96); }
	rect		FromPixels(const rect &p)			const	{ float x, y; factory->GetDesktopDpi(&x, &y); return rect(p.left * 96 / x, p.top * 96 / y, p.right * 96 / x, p.bottom * 96 / y); }
#endif
	void		BeginDraw()							const	{ device->BeginDraw();	}
	bool		EndDraw()							const	{ return device->EndDraw() == D2DERR_RECREATE_TARGET; }
	void		SetTransform(const matrix &mat)		const	{ device->SetTransform(mat);	}
	void		SetTransform(param(float2x3) mat)	const	{ device->SetTransform((matrix*)&mat);	}

	void	Clear(const colour &col) const {
		device->Clear(&col);
	}
	void	Fill(const rect &r, ID2D1Brush *brush) const {
		device->FillRectangle(&r, brush);
	}
	void	Draw(const rect &dest, ID2D1Bitmap *bm, float alpha = 1, bool bilinear = true) const {
		device->DrawBitmap(bm, &dest, alpha, D2D1_BITMAP_INTERPOLATION_MODE(bilinear));
	}
	void	Draw(const rect &dest, const rect &srce, ID2D1Bitmap *bm, float alpha = 1, bool bilinear = true) const {
		device->DrawBitmap(bm, &dest, alpha, D2D1_BITMAP_INTERPOLATION_MODE(bilinear), &srce);
	}
	void	Fill(ID2D1Geometry *geom, ID2D1Brush *brush, ID2D1Brush *opacity = 0) const {
		device->FillGeometry(geom, brush, opacity);
	}
	void	Draw(ID2D1Geometry *geom, ID2D1Brush *brush, float width = 1, ID2D1StrokeStyle *style = 0) const {
		device->DrawGeometry(geom, brush, width, style);
	}
#ifdef _D2D1_1_H_
	void	DrawImage(const point &dest, ID2D1Image *image,
		const rect			&image_rect,
		D2D1_INTERPOLATION_MODE	interp	= D2D1_INTERPOLATION_MODE_LINEAR,
		D2D1_COMPOSITE_MODE		comp	= D2D1_COMPOSITE_MODE_SOURCE_OVER
	) const {
		device->DrawImage(image, &dest, &image_rect, interp, comp);
	}
	void	DrawImage(ID2D1Effect *effect,
		const rect			&image_rect,
		D2D1_INTERPOLATION_MODE	interp	= D2D1_INTERPOLATION_MODE_LINEAR,
		D2D1_COMPOSITE_MODE		comp	= D2D1_COMPOSITE_MODE_SOURCE_OVER
	) const {
		device->DrawImage(effect, 0, &image_rect, interp, comp);
	}
	void	DrawImage(ID2D1Effect *effect,
		D2D1_INTERPOLATION_MODE	interp	= D2D1_INTERPOLATION_MODE_LINEAR,
		D2D1_COMPOSITE_MODE		comp	= D2D1_COMPOSITE_MODE_SOURCE_OVER
	) const {
		device->DrawImage(effect, 0, 0, interp, comp);
	}
	void	DrawImage(const point &dest, ID2D1Effect *effect,
		D2D1_INTERPOLATION_MODE	interp	= D2D1_INTERPOLATION_MODE_LINEAR,
		D2D1_COMPOSITE_MODE		comp	= D2D1_COMPOSITE_MODE_SOURCE_OVER
	) const {
		device->DrawImage(effect, &dest, 0, interp, comp);
	}
	void	DrawImage(const point &dest, ID2D1Effect *effect,
		const rect			&image_rect,
		D2D1_INTERPOLATION_MODE	interp	= D2D1_INTERPOLATION_MODE_LINEAR,
		D2D1_COMPOSITE_MODE		comp	= D2D1_COMPOSITE_MODE_SOURCE_OVER
	) const {
		device->DrawImage(effect, &dest, &image_rect, interp, comp);
	}
	void	DrawImage(const point &dest, ID2D1Image *image,
		D2D1_INTERPOLATION_MODE	interp	= D2D1_INTERPOLATION_MODE_LINEAR,
		D2D1_COMPOSITE_MODE		comp	= D2D1_COMPOSITE_MODE_SOURCE_OVER
	) const {
		device->DrawImage(image, &dest, 0, interp, comp);
	}
#endif
	void	FillEllipse(const ellipse &ellipse, ID2D1Brush *brush) const {
		device->FillEllipse(ellipse, brush);
	}
	void	DrawEllipse(const ellipse &ellipse, ID2D1Brush *brush, float width = 1, ID2D1StrokeStyle *style = 0) const {
		device->DrawEllipse(ellipse, brush, width, style);
	}
	void	Draw(const rect &r, ID2D1Brush *brush, float width = 1, ID2D1StrokeStyle *style = 0) const {
	    device->DrawRectangle(r, brush, width, style);
	}

	void	DrawLine(const point &p0, const point &p1
		, ID2D1Brush				*brush
		, float						width	= 1
		, ID2D1StrokeStyle			*style	= 0
	) const {
		device->DrawLine(p0, p1, brush, width, style);
	}

	void	DrawText(const rect &r, string_param16 &&s
		, IDWriteTextFormat			*font
		, ID2D1Brush				*brush
		, D2D1_DRAW_TEXT_OPTIONS	opts	= D2D1_DRAW_TEXT_OPTIONS_NONE
		, DWRITE_MEASURING_MODE		meas	= DWRITE_MEASURING_MODE_NATURAL
	) const {
		if (s)
			device->DrawText(s, s.size32(), font, &r, brush, opts, meas);
    }
	void	DrawText(const rect &r, const wchar_t *s, size_t len
		, IDWriteTextFormat			*font
		, ID2D1Brush				*brush
		, D2D1_DRAW_TEXT_OPTIONS	opts	= D2D1_DRAW_TEXT_OPTIONS_NONE
		, DWRITE_MEASURING_MODE		meas	= DWRITE_MEASURING_MODE_NATURAL
	) const {
		if (len)
			device->DrawText(s, UINT(len), font, &r, brush, opts, meas);
    }
	void	DrawText(const point &p
		, IDWriteTextLayout			*layout
		, ID2D1Brush				*brush
		, D2D1_DRAW_TEXT_OPTIONS	opts	= D2D1_DRAW_TEXT_OPTIONS_NONE
	) const {
		device->DrawTextLayout(p, layout, brush, opts);
	}
};

//-----------------------------------------------------------------------------
//	single-shot classes
//-----------------------------------------------------------------------------

struct Clipped {
	com_ptr2<ID2D1DeviceContext>		device;
	Clipped(ID2D1DeviceContext *_device, const rect &r, D2D1_ANTIALIAS_MODE mode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE) : device(_device) {
		device->PushAxisAlignedClip(&r, mode);
	}
	~Clipped() { device->PopAxisAlignedClip(); }
};

struct LayerParameters : D2D1_LAYER_PARAMETERS {
	LayerParameters(
		const rect				_contentBounds		= infinity,
		ID2D1Geometry*			_geometricMask		= 0,
		D2D1_ANTIALIAS_MODE		_maskAntialiasMode	= D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
		matrix					_maskTransform		= float2x3(identity),
		float					_opacity			= 1.0,
		ID2D1Brush*				_opacityBrush		= 0,
		D2D1_LAYER_OPTIONS		_layerOptions		= D2D1_LAYER_OPTIONS_NONE
	) {
		contentBounds		= _contentBounds;
		geometricMask		= _geometricMask;
		maskAntialiasMode	= _maskAntialiasMode;
		maskTransform		= _maskTransform;
		opacity				= _opacity;
		opacityBrush		= _opacityBrush;
		layerOptions		= _layerOptions;
	}
};

struct Layered {
	com_ptr2<ID2D1DeviceContext>		device;
	Layered(ID2D1DeviceContext *_device, ID2D1Layer *layer, const D2D1_LAYER_PARAMETERS &params = LayerParameters()) : device(_device) {
		device->PushLayer(&params, layer);
	}
	~Layered() { device->PopLayer(); }
};

inline bool GradientStops::Create(ID2D1DeviceContext *device, const gradstop *cols, int num, D2D1_EXTEND_MODE extend) {
	return SUCCEEDED(device->CreateGradientStopCollection((const D2D1_GRADIENT_STOP*)cols, num, D2D1_GAMMA_2_2, extend, get_addr()));
}

struct Geometry : com_ptr<ID2D1Geometry> {
	struct Sink  : com_ptr<ID2D1GeometrySink> {
		Sink(ID2D1GeometrySink *p) : com_ptr<ID2D1GeometrySink>(p) {}
		Sink(Sink &&s) = default;
		~Sink() { get()->Close(); }
	};

	Geometry(ID2D1Factory *factory, ID2D1Geometry *geom, const d2d::matrix &transform) {
		factory->CreateTransformedGeometry(geom, transform, (ID2D1TransformedGeometry**)get_addr());
	}
	Geometry(ID2D1Factory *factory, ID2D1Geometry **geoms, uint32 num, bool alternate = true) {
		factory->CreateGeometryGroup(alternate ? D2D1_FILL_MODE_ALTERNATE : D2D1_FILL_MODE_WINDING, geoms, num, (ID2D1GeometryGroup**)get_addr());
	}
	Geometry(ID2D1Factory *factory) {
		factory->CreatePathGeometry((ID2D1PathGeometry**)get_addr());
	}
	Geometry(ID2D1Factory *factory, const rect &rect) {
		factory->CreateRectangleGeometry(&rect, (ID2D1RectangleGeometry**)get_addr());
	}
	Geometry(ID2D1Factory *factory, const ellipse &ellipse) {
		factory->CreateEllipseGeometry(&ellipse, (ID2D1EllipseGeometry**)get_addr());
	}
	Geometry(const Target &t) {
		t.factory->CreatePathGeometry((ID2D1PathGeometry**)get_addr());
	}
	Geometry(const Target &t, const rect &rect) {
		t.factory->CreateRectangleGeometry(&rect, (ID2D1RectangleGeometry**)get_addr());
	}
	Geometry(const Target &t, const ellipse &ellipse) {
		t.factory->CreateEllipseGeometry(&ellipse, (ID2D1EllipseGeometry**)get_addr());
	}
	Sink	Open() {
		ID2D1GeometrySink	*sink;
		query<ID2D1PathGeometry>()->Open(&sink);
		return sink;
	}
};

struct Bitmap : com_ptr<ID2D1Bitmap> {
	Bitmap()	{}
	Bitmap(ID2D1DeviceContext *device, uint32 width, uint32 height, DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE alpha = D2D1_ALPHA_MODE_PREMULTIPLIED) {
		D2D1_BITMAP_PROPERTIES	props	= {{format, alpha}, 0, 0};
		D2D1_SIZE_U				size	= {width, height};
		device->CreateBitmap(size, props, get_addr());
	}
	template<typename T> Bitmap(ID2D1DeviceContext *device, const block<T, 2> &block) {
		d2d::CreateBitmap(device, get_addr(), block);
	}
};

struct SolidBrush : com_ptr<ID2D1SolidColorBrush> {
	SolidBrush() {}
	SolidBrush(ID2D1DeviceContext *device, const colour &col) {
		device->CreateSolidColorBrush(&col, NULL, get_addr());
	}
};

struct LinearGradientBrush : com_ptr<ID2D1LinearGradientBrush> {
	struct Props : D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES {
		Props(const point &p0, const point &p1) {
			startPoint	= p0;
			endPoint	= p1;
		}
		const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES*	operator&() const	{ return this; }
	};
	LinearGradientBrush(ID2D1DeviceContext *device, const point &p0, const point &p1, ID2D1GradientStopCollection *grads) {
		device->CreateLinearGradientBrush(&Props(p0, p1), NULL, grads, get_addr());
	}
	LinearGradientBrush(ID2D1DeviceContext *device, const point &p0, const point &p1, const range<gradstop*> &grads) {
		device->CreateLinearGradientBrush(&Props(p0, p1), NULL, GradientStops(device, grads), get_addr());
	}
	void	SetTransform(const float2x3 &mat)		const	{ get()->SetTransform(matrix(mat));	}
};

struct RadialGradientBrush : com_ptr<ID2D1RadialGradientBrush> {
	struct Props : D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES {
		Props(const rect &rect, const point &offset) {
			center					= rect.Centre();
			radiusX					= (rect.right - rect.left) / 2;
			radiusY					= (rect.top - rect.bottom) / 2;
			gradientOriginOffset	= offset;
		}
		const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES*	operator&() const	{ return this; }
	};
	RadialGradientBrush(ID2D1DeviceContext *device, const rect &rect, const point &offset, ID2D1GradientStopCollection* grads) {
		device->CreateRadialGradientBrush(&Props(rect, offset), NULL, grads, get_addr());
	}
	RadialGradientBrush(ID2D1DeviceContext *device, const rect &rect, const point &offset, const range<gradstop*> &grads) {
		device->CreateRadialGradientBrush(&Props(rect, offset), NULL, GradientStops(device, grads), get_addr());
	}
	void	SetTransform(const float2x3 &mat)		const	{ get()->SetTransform(matrix(mat));	}
};

#ifdef PLAT_WIN32
struct BitmapBrush : com_ptr<ID2D1Brush> {
	win::Bitmap						win_bm;
	com_ptr<ID2D1Bitmap>			d2d_bm;
	D2D1_BITMAP_BRUSH_PROPERTIES	props1;
	D2D1_BRUSH_PROPERTIES			props2;

	bool CreateBitmap(ID2D1DeviceContext *device, ID2D1Bitmap **bm, const win::Bitmap::Params &bmp) {
		return d2d::CreateBitmap(device, bm, make_block((Texel<R8G8B8>*)bmp.Scanline(0), bmp.Width(), bmp.Height()));
	}
	bool CreateDeviceResources(ID2D1DeviceContext *device) {
		return get() || (
			(d2d_bm || CreateBitmap(device, &d2d_bm, win_bm))
				? SUCCEEDED(device->CreateBitmapBrush(d2d_bm, &props1, &props2, (ID2D1BitmapBrush**)get_addr()))
				: SUCCEEDED(device->CreateSolidColorBrush(colour(0.5f,0.5f,0.5f), (ID2D1SolidColorBrush**)get_addr()))
		);
	}
	void DiscardDeviceResources() {
		clear();
		d2d_bm.clear();
	}

	BitmapBrush(win::Bitmap _bm, const matrix &transform) : win_bm(_bm) {
		props1.extendModeX			= props1.extendModeY = D2D1_EXTEND_MODE_WRAP;
		props1.interpolationMode	= D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
		props2.opacity				= 1;
		props2.transform			= transform;
	}
	void SetTransform(const float2x3 &mat)		const	{ get()->SetTransform(matrix(mat));	}
};
#endif

#ifdef _D2D1_1_H_
struct Effect : com_ptr<ID2D1Effect> {
	Effect(ID2D1DeviceContext *device, const IID &iid)	{ device->CreateEffect(iid, get_addr()); }
	Effect &SetInput(ID2D1Effect *b)					{ get()->SetInputEffect(0, b); return *this; }
	Effect &SetInput(ID2D1Image *b)						{ get()->SetInput(0, b); return *this; }
	Effect &SetInput(int i, ID2D1Effect *b)				{ get()->SetInputEffect(i, b); return *this; }
	Effect &SetInput(int i, ID2D1Image *b)				{ get()->SetInput(i, b); return *this; }
	template<typename T> Effect &SetValue(uint32 i, const T &t) {
		get()->SetValue(i, (const BYTE*)&t, sizeof(T));
		return *this;
	}
};
#endif

//-----------------------------------------------------------------------------
//	Write
//-----------------------------------------------------------------------------

struct Write : com_ptr<IDWriteFactory> {
	bool CreateTextLayout(IDWriteTextLayout **layout, string_param16 &&s, IDWriteTextFormat *font, float width, float height) {
		return SUCCEEDED(get()->CreateTextLayout(s, s.size32(), font, width, height, layout));
	}
	bool CreateTextLayout(IDWriteTextLayout **layout, const wchar_t *s, size_t len, IDWriteTextFormat *font, float width, float height) {
		return SUCCEEDED(get()->CreateTextLayout(s, UINT(len), font, width, height, layout));
	}
	Write() {
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&*this);
	}
};

struct Font : com_ptr<IDWriteTextFormat> {
#ifdef PLAT_WIN32
	enum {
		BOLD		= CFE_BOLD,
		ITALIC		= CFE_ITALIC,
		UNDERLINE	= CFE_UNDERLINE,
		STRIKEOUT	= CFE_STRIKEOUT,
	};
#else
	enum {
		BOLD		= 1,
		ITALIC		= 2,
		UNDERLINE	= 4,
		STRIKEOUT	= 8,
	};
#endif
	Font() {}
	Font(IDWriteFactory *write, string_param16 &&name, float size, int flags = 0) {
		write->CreateTextFormat(name, NULL,
			flags & BOLD	? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_MEDIUM,
			flags & ITALIC	? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			size, L"", get_addr()
		);
	}
	Font(IDWriteFactory *write, string_param16 &&name, float size, float spacing, int flags) : Font(write, move(name), size, flags) {
		get()->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, spacing, size);

	}
#ifdef PLAT_WIN32
	Font(IDWriteFactory *write, const win::Font::Params16 &p) : Font(write, p.Name(), win::DeviceContext::ScreenCaps().PerInchY<96>(p.LogicalHeight()), win::DeviceContext::ScreenCaps().PerInchY<96>(p.Height()), p.Effects()) {}
	Font(IDWriteFactory *write, HFONT h) : Font(write, win::Font(h).GetParams16()) {}
#endif

};

struct TextLayout : com_ptr<IDWriteTextLayout> {
	struct Range : DWRITE_TEXT_RANGE {
		Range(uint32 _start, uint32 _length)	{ startPosition = _start; length = _length; }
		Range(const interval<uint32> &i)		{ startPosition = i.begin(); length = i.extent(); }
#ifdef PLAT_WIN32
		Range(const CHARRANGE &chrg)			{ startPosition = chrg.cpMin; length = chrg.cpMax - chrg.cpMin; }
#endif
		uint32		begin()			const { return startPosition; }
		uint32		end()			const { return startPosition + length; }
		bool		empty()			const { return length == 0; }
		operator interval<uint32>() const { return interval<uint32>(begin(), end()); }
	};
	TextLayout() {}
	TextLayout(IDWriteTextLayout *p) : com_ptr<IDWriteTextLayout>(p) {}
	TextLayout(IDWriteFactory *write, string_param16 &&s, IDWriteTextFormat *font, float width, float height = 0) {
		write->CreateTextLayout(s, s.size32(), font, width, height, get_addr());
	}
	TextLayout(IDWriteFactory *write, const wchar_t *s, size_t len, IDWriteTextFormat *font, float width, float height = 0) {
		write->CreateTextLayout(s, UINT(len), font, width, height, get_addr());
	}
	TextLayout(IDWriteFactory *write, string_param16 &&s, IDWriteTextFormat *font, const point &size) {
		write->CreateTextLayout(s, s.size32(), font, size.x, size.y, get_addr());
	}
	TextLayout(IDWriteFactory *write, const wchar_t *s, size_t len, IDWriteTextFormat *font, const point &size) {
		write->CreateTextLayout(s, UINT(len), font, size.x, size.y, get_addr());
	}

	void	SetDrawingEffect(IUnknown* effect, const Range &range) {
		get()->SetDrawingEffect(effect, range);
	}
	rect	GetExtent() {
		DWRITE_TEXT_METRICS	metrics;
		get()->GetMetrics(&metrics);
		return rect::with_ext(metrics.left, metrics.top, metrics.width, metrics.height);
	}
};

#if 0
//-----------------------------------------------------------------------------
//	Imaging
//-----------------------------------------------------------------------------

struct Imaging : com_ptr<IWICImagingFactory2> {
	Imaging() { create(CLSID_WICImagingFactory2); }
};

struct ImageEncoder : com_ptr<IWICImageEncoder> {
	ImageEncoder() {}
	ImageEncoder(IWICImagingFactory2 *imaging, ID2D1Device *d2d) {
		imaging->CreateImageEncoder(d2d, get_addr());
	}
};
#endif

//-----------------------------------------------------------------------------
//	Targets
//-----------------------------------------------------------------------------

#ifdef PLAT_WIN32

//notifications
enum {
	PAINT	= 0x1000,
};

// display info
struct PAINT_INFO : win::Notification {
	void *target;
	PAINT_INFO(win::Control from, void *target) : Notification(from, PAINT), target(target) {}
};


struct WND : Target {
	uint32	dpi = 0;
	bool	Init(HWND hWnd, const win::Point &size) {
		if (!device || (PixelSize() != size && !Resize(size))) {
			device.clear();
			D2D1_RENDER_TARGET_PROPERTIES	props	= {
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				{DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN},
				0, 0,
				D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
			};
			D2D1_HWND_RENDER_TARGET_PROPERTIES	hwnd_props = {
				hWnd,
				(D2D1_SIZE_U&)size,
				D2D1_PRESENT_OPTIONS_NONE
			};
//			factory->CreateHwndRenderTarget(props, hwnd_props, (ID2D1HwndRenderTarget**)&device);
			com_ptr<ID2D1HwndRenderTarget>		target0;
			if (SUCCEEDED(factory->CreateHwndRenderTarget(props, hwnd_props, &target0))) {
				target0.query(&device);
				dpi = GetDpiForWindow(hWnd);
			}
		}
		return !!device;
	}
	bool	Resize(const win::Point &size)	const	{ return device && FAILED(device.query<ID2D1HwndRenderTarget>()->Resize((D2D1_SIZE_U*)&size));	}
	bool	Occluded()						const	{ return !!(device.query<ID2D1HwndRenderTarget>()->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED); }

	point	ToPixels(const point &p)		const	{ return point(p.x * dpi / 96, p.y * dpi / 96); }
	point	FromPixels(const point &p)		const	{ return point(p.x * 96 / dpi, p.y * 96 / dpi); }
	rect	ToPixels(const rect &p)			const	{ return rect(p.left * dpi / 96, p.top * dpi / 96, p.right * dpi / 96, p.bottom * dpi / 96); }
	rect	FromPixels(const rect &p)		const	{ return rect(p.left * 96 / dpi, p.top * 96 / dpi, p.right * 96 / dpi, p.bottom * 96 / dpi); }
};

class Window : public win::Window<Window>, public WND {
protected:
	struct DeviceContextPaint : public win::DeviceContextPaint {
		Window	&target;
		bool	unoccluded;
		DeviceContextPaint(Window &target) : win::DeviceContextPaint(target), target(target) {
			unoccluded = target.BeginDraw();
		}
		~DeviceContextPaint()	{ if (unoccluded) target.EndDraw(); }
		operator bool()			{ return unoccluded; }
	};

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE:
				if (WND::Resize(win::Point(lParam)))
					DeInit();
				break;

			case WM_LBUTTONDOWN:
				return Parent().Notify(*this, NM_CLICK);

			case WM_RBUTTONDOWN:
				return Parent().Notify(*this, NM_RCLICK);

			case WM_MOUSEMOVE:
			case WM_MOUSEWHEEL:
			case WM_NOTIFY:
			//case WM_COMMAND:
				return Parent()(message, wParam, lParam);

			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}
	bool	BeginDraw() {
		WND::Init(hWnd, GetClientRect().Size());
		if (Occluded())
			return false;
		WND::BeginDraw();
		return true;
	}
};

struct DC : Target {
	DC() {
		D2D1_RENDER_TARGET_PROPERTIES	props = {
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE},
			0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
		};
//		factory->CreateDCRenderTarget(&props, (ID2D1DCRenderTarget**)&device);
		com_ptr<ID2D1DCRenderTarget>		target0;
		factory->CreateDCRenderTarget(&props, &target0);
		target0.query(&device);
	}
	bool	Bind(HDC hdc, const RECT &rect) {
		return SUCCEEDED(device.query<ID2D1DCRenderTarget>()->BindDC(hdc, &rect));
	}
};
#endif

#ifdef PLAT_WINRT
struct WinRT : Target {
	com_ptr<ISurfaceImageSourceNative> native;

	bool	Init(IUnknown *image_source) {
		native = query<ISurfaceImageSourceNative>(image_source);
		if (!native)
			return false;

		if (!device) {
			com_ptr<ID3D11Device>	dx11;

			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
				nullptr, 0,
				D3D11_SDK_VERSION,
				&dx11,
				nullptr,
				nullptr
			);
			com_ptr<IDXGIDevice>	dxgi_device	= dx11.query<IDXGIDevice>();

			com_ptr<ID2D1Device> d2device;
			if (SUCCEEDED(factory->CreateDevice(dxgi_device, &d2device)))
				d2device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &device);

			native->SetDevice(dxgi_device);
		}
		return !!device;
	}

	bool	BeginDraw(const win::Rect &rect, win::Point &offset) {
		// Begin drawing - returns a target surface and an offset to use as the top left origin when drawing.
		com_ptr<IDXGISurface>	surface;
		HRESULT begin = native->BeginDraw(rect, &surface, &offset);
//		HRESULT begin = native->BeginDraw(rect, __uuidof(IDXGISurface), (void**)&surface, &offset);

		if (SUCCEEDED(begin)) {
			com_ptr<ID2D1Bitmap1> bitmap;
			device->CreateBitmapFromDxgiSurface(surface, nullptr, &bitmap);

			device->SetTarget(bitmap);
			device->BeginDraw();
			return true;
		}

		return false;
	}

	// Ends drawing updates started by a previous BeginDraw call.
	bool EndDraw() {
		HRESULT	hr = device->EndDraw();
		device->SetTarget(nullptr);
		native->EndDraw();
		return hr == D2DERR_RECREATE_TARGET;
	}

	void	DeInit() {
		native.clear();
		Target::DeInit();
	}
//	bool	Occluded() const	{ return !!(device.query<ID2D1HwndRenderTarget>()->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED); }
};
#endif

#ifndef PLAT_WINRT
struct DXI : Target {
	com_ptr<ID3D10Device1>		device;
	com_ptr<ID3D10Texture2D>	tex, tex_mem;
	com_ptr<IDXGISurface>		surface;

	DXI() {}
	DXI(uint32 width, uint32 height, DXGI_FORMAT fmt = DXGI_FORMAT_B8G8R8A8_UNORM) {
		Init(width, height, fmt);
	}
	bool Init(uint32 width, uint32 height, DXGI_FORMAT fmt = DXGI_FORMAT_B8G8R8A8_UNORM) {
		HRESULT	hr;
		if (FAILED(hr = D3D10CreateDevice1(
			NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL,
			fmt == DXGI_FORMAT_B8G8R8A8_UNORM ? D3D10_CREATE_DEVICE_BGRA_SUPPORT : 0,
			D3D10_FEATURE_LEVEL_10_0,
			D3D10_1_SDK_VERSION,
			&device
		)))
			return false;

		D3D10_TEXTURE2D_DESC	desc = {
			width, height, 1, 1,
			fmt, {1, 0},
			D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE, 0, 0
		};

		if (FAILED(hr = device->CreateTexture2D(&desc, NULL, &tex)) || FAILED(hr = tex.query(&surface)))
			return false;

		desc.Usage			= D3D10_USAGE_STAGING;
		desc.BindFlags		= 0;
		desc.CPUAccessFlags	= D3D10_CPU_ACCESS_READ;
		hr = device->CreateTexture2D(&desc, NULL, &tex_mem);

		D2D1_RENDER_TARGET_PROPERTIES	props = {
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			{fmt, D2D1_ALPHA_MODE_IGNORE},
			0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
		};

//		hr = factory->CreateDxgiSurfaceRenderTarget(surface, &props, &device);
		com_ptr<ID2D1RenderTarget>		target0;
		return SUCCEEDED(factory->CreateDxgiSurfaceRenderTarget(surface, &props, &target0)) && SUCCEEDED(target0.query(&device));
	}

	auto	GetData() {
		D3D10_MAPPED_TEXTURE2D	mapped;
		D3D10_TEXTURE2D_DESC	desc;
		tex->GetDesc(&desc);

		device->CopyResource(tex_mem, tex);
		tex_mem->Map(0, D3D10_MAP_READ, 0, &mapped);
		return make_strided_block(mapped.pData, mapped.RowPitch, desc.Width, desc.Height);
	}
};
#endif

//-----------------------------------------------------------------------------
//	custom effect
//-----------------------------------------------------------------------------

#ifdef _D2D1_EFFECT_AUTHOR_H_

template<size_t N> constexpr auto& get_type(const constexpr_string<char, N>&)	{ return "string"; }
constexpr auto& get_type(const char*)				{ return "string"; }
constexpr auto& get_type(const char*&)				{ return "string"; }
constexpr auto& get_type(const bool&)				{ return "bool"; }
constexpr auto& get_type(const uint32&)				{ return "uint32"; }
constexpr auto& get_type(const int32&)				{ return "int32"; }
constexpr auto& get_type(const float&)				{ return "float"; }
constexpr auto& get_type(const D2D_VECTOR_2F&)	    { return "vector2"; }
constexpr auto& get_type(const D2D_VECTOR_3F&)	    { return "vector3"; }
constexpr auto& get_type(const D2D_VECTOR_4F&)	    { return "vector4"; }
constexpr auto& get_type(const D2D1_MATRIX_3X2_F&)	{ return "matrix3x2"; }
constexpr auto& get_type(const D2D_MATRIX_4X3_F&)	{ return "matrix4x3"; }
constexpr auto& get_type(const D2D_MATRIX_4X4_F&)	{ return "matrix4x4"; }
constexpr auto& get_type(const D2D_MATRIX_5X4_F&)	{ return "matrix5x4"; }
constexpr auto& get_type(const uint8*)				{ return "blob"; }
constexpr auto& get_type(const IUnknown*)			{ return "iunknown"; }
constexpr auto& get_type(const ID2D1ColorContext*)	{ return "colorcontext"; }
constexpr auto& get_type(const GUID&)				{ return "clsid"; }
//constexpr auto& get_type(<ENUMERATION>)			{ return "enum"; }

template<typename T> constexpr auto& get_type()		{ return get_type(*(T*)1); }


template<int N, typename T, typename... C> constexpr auto make_property_tag(const char (&name)[N], const T &value, const C&... children) {
	return make_xml_tag("Property",
		meta::make_tuple(make_xml_attrib("name", name), make_xml_attrib("type", get_type(value)), make_xml_attrib("value", value)),
		children...
	);
}

template<typename I> constexpr auto make_inputs1(const I &ins) {
	return make_xml_tag("Input", meta::make_tuple(make_xml_attrib("name", ins)));
}
template<typename I0, typename... I> constexpr auto make_inputs1(const meta::tuple<I0, I...> &ins) {
	return meta::make_tuple(make_xml_tag("Input", meta::make_tuple(make_xml_attrib("name", ins.head))), make_inputs1(ins.tail));
}

constexpr auto make_inputs(const meta::tuple<> &ins) {
	return make_xml_tag("Inputs", meta::make_tuple());
}
template<typename I> constexpr auto make_inputs(const I &ins) {
	return make_xml_tag("Inputs", meta::make_tuple(), make_inputs1(ins));
}


class CustomEffect : public com<ID2D1EffectImpl> {
protected:
	template<int N, typename T> struct parameter {
		constexpr_string<char,N>		name;
		constexpr_string<wchar_t,N+1>	name16;
		PD2D1_PROPERTY_SET_FUNCTION set;
		PD2D1_PROPERTY_GET_FUNCTION get;
		constexpr parameter(const constexpr_string<char,N> &name, PD2D1_PROPERTY_SET_FUNCTION set, PD2D1_PROPERTY_GET_FUNCTION get) : name(name), name16(name+"\0"), set(set), get(get) {}
		constexpr operator D2D1_PROPERTY_BINDING() const {
			return D2D1_PROPERTY_BINDING{name16.begin(), set, get};
		}
		friend constexpr auto make_parameter_tag(const parameter &p) {
			return make_xml_tag("Property",
				meta::make_tuple(make_xml_attrib("name", p.name), make_xml_attrib("type", get_type<T>())),
				make_property_tag("DisplayName", p.name)
			);
		}
	};
	template<typename T, int N> static auto make_parameter(const char (&name)[N], PD2D1_PROPERTY_SET_FUNCTION set, PD2D1_PROPERTY_GET_FUNCTION get) {
		return parameter<N - 1,T>(name, set, get);
	}
public:
	template<class C, class T, T> struct getter;
	template<class C, class T, T> struct setter;

	// ID2D1EffectImpl
	STDMETHOD(SetGraph)(ID2D1TransformGraph* graph) {
		return E_NOTIMPL;
	}

	template<typename T, int N, int A, int C, int D, typename I, typename...P> static HRESULT Register(ID2D1Factory1 *factory, const GUID &guid,
		const char (&name)[N], const char (&author)[A], const char (&category)[C], const char (&description)[D],
		const I& inputs, const P&...params
	) {
		auto props = make_xml_tag("Effect", meta::make_tuple(),
			make_property_tag("DisplayName", name),
			make_property_tag("Author",	author),
			make_property_tag("Category", category),
			make_property_tag("Description", description),
			make_inputs(inputs),
			make_parameter_tag(params)...
		);

		D2D1_PROPERTY_BINDING bindings[] = {
			params...
		};
		return factory->RegisterEffectFromString(guid, meta::make_array<wchar_t>(xml_constexpr_string(props) + "\0").begin(), bindings, sizeof...(params), [](IUnknown **pp) {
			*pp = static_cast<ID2D1EffectImpl*>(new T());
			return S_OK;
		});
	}

};

//#define FIX_FIELD(C, F)	static_cast<deref_t<decltype(&C::F)> C::*>(&C::F)

#define EFFECT_PARAMETER(N, C, S, G)	make_parameter<typename setter<C, decltype(S), S>::type>(N, setter<C, decltype(S), S>::set, getter<C, decltype(G), G>::get)
#define EFFECT_FIELD(C, F)		EFFECT_PARAMETER(#F, C, &C::F, &C::F)
//#define EFFECT_FIELD2(C, F)		EFFECT_PARAMETER(#F, FIX_FIELD(C, F), FIX_FIELD(C, F))
#define EFFECT_ACCESSOR(C, N)	EFFECT_PARAMETER(#N, C, &C::set_##N, &C::get_##N)

// member value
template<typename C, typename C1, typename T, T C1::*P> struct CustomEffect::setter<C, T C1::*, P> {
	typedef T type;
	static HRESULT CALLBACK set(IUnknown *effect, const BYTE *data, UINT32 dataSize) {
		if (dataSize != sizeof(T))
			return E_INVALIDARG;
		static_cast<C*>(static_cast<ID2D1EffectImpl*>(effect))->*P = *reinterpret_cast<const T*>(data);
		return S_OK;
	}
};

template<typename C, typename C1, typename T, T C1::*P> struct CustomEffect::getter<C, T C1::*, P> {
	static HRESULT CALLBACK get(const IUnknown *effect, BYTE *data, UINT32 dataSize, UINT32 *actualSize) {
		if (actualSize)
			*actualSize = sizeof(T);
		if (dataSize > 0 && data) {
			if (dataSize < sizeof(T))
				return E_NOT_SUFFICIENT_BUFFER;
			*reinterpret_cast<T*>(data) = static_cast<const C*>(static_cast<const ID2D1EffectImpl*>(effect))->*P;
		}
		return S_OK;
	}
};

// member function
template<typename C, typename C1, typename T, HRESULT(C::*P)(T)> struct CustomEffect::setter<C, HRESULT(C1::*)(T), P> {
	typedef T type;
	static HRESULT CALLBACK set(IUnknown *effect, const BYTE *data, UINT32 dataSize) {
		if (dataSize != sizeof(T))
			return E_INVALIDARG;
		return (static_cast<C*>(static_cast<ID2D1EffectImpl*>(effect))->*P)(*reinterpret_cast<const T*>(data));
	}
};

template<typename C, typename C1, typename T, T(C::*P)() const> struct CustomEffect::getter<C, T(C1::*)() const, P> {
	static HRESULT CALLBACK get(const IUnknown *effect, BYTE *data, UINT32 dataSize, UINT32 *actualSize) {
		if (actualSize)
			*actualSize = sizeof(T);
		if (dataSize > 0 && data) {
			if (dataSize < sizeof(T))
				return E_NOT_SUFFICIENT_BUFFER;
			*reinterpret_cast<T*>(data) = (static_cast<const C*>(static_cast<const ID2D1EffectImpl*>(effect))->*P)();
		}
		return S_OK;
	}
};

#endif

//-----------------------------------------------------------------------------
//	custom text effect
//-----------------------------------------------------------------------------

class DECLSPEC_UUID("6c07e470-2f96-11de-8c30-0800200c9a66") TextEffect : public IUnknown {
public:
	STDMETHOD(DrawGlyphRun)(void *context, ID2D1DeviceContext *device, D2D1_POINT_2F baseline, const DWRITE_GLYPH_RUN *glyphs, const DWRITE_GLYPH_RUN_DESCRIPTION *desc, DWRITE_MEASURING_MODE measure)=0;
	STDMETHOD(DrawGeometry)(void *context, ID2D1DeviceContext *device, ID2D1Geometry *geometry)=0;
};

class TextRenderer : public com<IDWriteTextRenderer> {
	Target		&t;
	com_ptr2<ID2D1SolidColorBrush>	brush;
	bool		pixel_snapping;

	HRESULT	DrawGeometry(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, ID2D1Geometry *geometry, IUnknown *effect) {
		matrix		mat = (float2x3)translate(baselineOriginX, baselineOriginY);
		t.device->SetTransform(mat);
		if (effect) {
			if (com_ptr<TextEffect> effect2 = querier(effect))
				return effect2->DrawGeometry(context, t.device, geometry);
		}
		t.device->FillGeometry(geometry, brush);
		return S_OK;
	}

	HRESULT	DrawRect(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, const rect &rect, IUnknown *effect) {
		HRESULT hr;
		com_ptr<ID2D1RectangleGeometry> rect_geom;
		if (SUCCEEDED(hr = t.factory->CreateRectangleGeometry(&rect, &rect_geom)))
			hr = DrawGeometry(context, baselineOriginX, baselineOriginY, rect_geom, effect);
		return hr;
	}
public:
	TextRenderer(Target &_t, ID2D1SolidColorBrush *_brush) : t(_t), brush(_brush), pixel_snapping(true) {
	}

	STDMETHOD(IsPixelSnappingDisabled)(void *context, BOOL *isDisabled) {
	    *isDisabled = !pixel_snapping;
		return S_OK;
	}
	STDMETHOD(GetCurrentTransform)(void *context, DWRITE_MATRIX *transform) {
		t.device->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
		return S_OK;
	}
	STDMETHOD(GetPixelsPerDip)(void *context, FLOAT *pixelsPerDip) {
		float x, y;
		t.device->GetDpi(&x, &y);
		*pixelsPerDip = x / 96;
		return S_OK;
	}
	STDMETHOD(DrawGlyphRun)(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, const DWRITE_GLYPH_RUN *glyphRun, const DWRITE_GLYPH_RUN_DESCRIPTION *glyphRunDescription, IUnknown *effect) {
		if (effect) {
#if 0
			HRESULT hr = S_OK;
			com_ptr<ID2D1PathGeometry>	geometry;
			com_ptr<ID2D1GeometrySink>	sink;
			if (SUCCEEDED(hr = t.factory->CreatePathGeometry(&geometry))
			&&	SUCCEEDED(hr = geometry->Open(&sink))
			&&	SUCCEEDED(hr = glyphRun->fontFace->GetGlyphRunOutline(
				glyphRun->fontEmSize,
				glyphRun->glyphIndices,
				glyphRun->glyphAdvances,
				glyphRun->glyphOffsets,
				glyphRun->glyphCount,
				glyphRun->isSideways,
				glyphRun->bidiLevel % 2,
				sink
			))
			&&	SUCCEEDED(hr = sink->Close())
			)
				return DrawGeometry(context, baselineOriginX, baselineOriginY, geometry, effect);
			return hr;
#else
			if (com_ptr<TextEffect> effect2 = querier(effect))
				return effect2->DrawGlyphRun(context, t.device, point(baselineOriginX, baselineOriginY), glyphRun, glyphRunDescription, measuringMode);
#endif
		}
		t.device->DrawGlyphRun(point(baselineOriginX, baselineOriginY), glyphRun, glyphRunDescription, brush, measuringMode);
		return S_OK;
	}
	STDMETHOD(DrawUnderline)(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE const *underline, IUnknown *effect) {
		return DrawRect(context, baselineOriginX, baselineOriginY, rect(0, underline->offset, underline->width, underline->offset + underline->thickness), effect);
	}
	STDMETHOD(DrawStrikethrough)(void *context, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *effect) {
		return DrawRect(context, baselineOriginX, baselineOriginY, rect(0, strikethrough->offset, strikethrough->width, strikethrough->offset + strikethrough->thickness), effect);
	}
	STDMETHOD(DrawInlineObject)(void *context, FLOAT originX, FLOAT originY, IDWriteInlineObject *inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown *effect) {
		return E_NOTIMPL;
	}

	void	PixelSnapping(bool enable) {
		pixel_snapping = enable;
	}
};

} // namespace d2d

inline d2d::point	operator*(const float2x3 &m, const d2d::point &r)	{ return m * position2((float2)r); }
inline d2d::rect	operator*(const float2x3 &m, const d2d::rect &r)	{ return d2d::rect(m * r.TopLeft(), m * r.BottomRight()); }
inline d2d::rect	operator/(const d2d::rect &r, const float2x3 &m)	{ return inverse(m) * r; }
} // namespace iso

#endif
