#include "iso/iso_convert.h"
#include "windows/d2d.h"
#include "maths/statistics.h"

namespace app {
using namespace iso;
using namespace win;

//-----------------------------------------------------------------------------
//	effect
//-----------------------------------------------------------------------------

const GUID CLSID_MyCustomEffect = {0xB7B36C92, 0x3498, 0x4A94, {0x9E, 0x95, 0x9F, 0x24, 0x6F, 0x92, 0x45, 0xBF}};
const GUID GUID_PixelShader		= {0xB7B36C92, 0x3498, 0x4A94, {0x9E, 0x95, 0x9F, 0x24, 0x6F, 0x92, 0x45, 0xC0}};

class MyCustomEffect : public com_inherit<type_list<ID2D1Transform>, com2<ID2D1DrawTransform, d2d::CustomEffect> > {
	friend d2d::CustomEffect;
	com_ptr2<ID2D1DrawInfo> draw_info;
	D2D1_RECT_L				rect;

	struct Constants {
		float2x3	transform;
		float2x3	itransform;
	} constants;

	MyCustomEffect()	{ clear(constants); constants.transform = constants.itransform = identity; }

//parameters
	d2d::matrix	get_transform()	const {
		return constants.transform;
	}
	HRESULT		set_transform(d2d::matrix x) {
		constants.transform		= x;
		constants.itransform	= inverse(constants.transform);
		return S_OK;
	}

public:
	enum {
		PARAM_transform,
	};

	static HRESULT Register(ID2D1Factory1 *factory) {
		return d2d::CustomEffect::Register<MyCustomEffect>(factory, CLSID_MyCustomEffect,
			"MyCustomEffect", "Isopod", "Sample", "This is a demo effect",
			"SourceOne",
			EFFECT_ACCESSOR(MyCustomEffect, transform)
		);

	}
	static HRESULT Register(IUnknown *factory) {
		return Register(temp_com_cast<ID2D1Factory1>(factory));
	}

// ID2D1EffectImpl
	STDMETHOD(Initialize)(ID2D1EffectContext* context, ID2D1TransformGraph* graph) {
		HRESULT	hr;
//		context->GetDpi(&constants.dpi.x, &constants.dpi.y);
#ifdef PLAT_WIN32
        Resource	data("d2d_shader", "SHADER");
		hr = context->LoadPixelShader(GUID_PixelShader, data, data.size32());
#endif
		// The graph consists of a single transform. In fact, this class is the transform,
        hr = graph->SetSingleTransformNode(this);
		return hr;
	}

	STDMETHOD(PrepareForRender)(D2D1_CHANGE_TYPE change_type) {
		return draw_info->SetPixelShaderConstantBuffer((BYTE*)&constants, sizeof(constants));
	}

// ID2D1DrawTransform
	STDMETHOD(SetDrawInfo)(ID2D1DrawInfo *_draw_info) {
		HRESULT	hr = S_OK;
		draw_info = _draw_info;
		hr = draw_info->SetPixelShader(GUID_PixelShader);
		return hr;
	}

// ID2D1Transform
	STDMETHOD(MapOutputRectToInputRects)(const D2D1_RECT_L *out, D2D1_RECT_L *in, UINT32 in_count) const {
		if (in_count != 1)
			return E_INVALIDARG;

		rectangle	r	= (constants.itransform * d2d::rect(*out));
		float2		pt0	= floor(r.a.v);
		float2		pt1	= ceil(r.b.v);

		in[0].left    = pt0.x;
		in[0].top     = pt0.y;
		in[0].right   = pt1.x;
		in[0].bottom  = pt1.y;
		return S_OK;
	}

	STDMETHOD(MapInputRectsToOutputRect)(const D2D1_RECT_L *in, const D2D1_RECT_L *opaque, UINT32 in_count, D2D1_RECT_L *out, D2D1_RECT_L *opaque_subrect) {
		if (in_count != 1)
			return E_INVALIDARG;

		clear(*opaque_subrect);

		rectangle	r	= (constants.transform * d2d::rect(*in));
		float2		pt0	= floor(r.a.v);
		float2		pt1	= ceil(r.b.v);

		out[0].left    = pt0.x;
		out[0].top     = pt0.y;
		out[0].right   = pt1.x;
		out[0].bottom  = pt1.y;

		rect = out[0];
		return S_OK;
	}

	STDMETHOD(MapInvalidRect)(UINT32 input_index, D2D1_RECT_L invalid_in, D2D1_RECT_L *invalid_out) const {
		// Indicate that the entire output may be invalid.
		invalid_out->left    = LONG_MIN;
		invalid_out->top     = LONG_MIN;
		invalid_out->right   = LONG_MAX;
		invalid_out->bottom  = LONG_MAX;
		return S_OK;
	}

	// ID2D1TransformNode
	STDMETHOD_(UINT32, GetInputCount)() const {
		return 1;
	}
};


//-----------------------------------------------------------------------------
//	histogram
//-----------------------------------------------------------------------------

enum CHANS {
	CHAN_R			= 1,
	CHAN_G			= 2,
	CHAN_B			= 4,
	CHAN_A			= 8,
	CHAN_INDEX		= 16,
	CHAN_MASK		= 32,
	CHAN_DEPTH		= 64,
	CHAN_RGB		= 7,
	CHAN_RGBA		= 15,
};

struct histogram {
	uint32	h[4][256];
	uint8	order[4][256];
	uint8	first_nonzero[4];

	struct scale_offset {
		float4 o, s;
		scale_offset(HDRpixel minpix, HDRpixel maxpix) {
			float4	a{minpix.r, minpix.g, minpix.b, minpix.a};
			float4	b{maxpix.r, maxpix.g, maxpix.b, maxpix.a};
			s	= reciprocal(select(a == b, float4(one), b - a)) * 255.f;
			o	= -a;
		}
	};

	histogram()		{ clear(h); }
	void	reset()	{ clear(h); }
	void	init_medians();
	void	add(const block<ISO_rgba, 1> &b);
	void	add(const block<ISO_rgba, 2> &b);
	void	init(bitmap64 *bm);

	void	add(const block<HDRpixel, 1> &b, const scale_offset &s);
	void	add(const block<HDRpixel, 2> &b, const scale_offset &s);
	void	init(HDRbitmap64 *bm, HDRpixel minpix, HDRpixel maxpix);

	uint32	median_value(int c) const {
		return h[c][order[c][(first_nonzero[c] + 256) / 2]];
	}
	uint32	max_value(int c) const {
		return h[c][order[c][255]];
	}
};
void DrawHistogram(d2d::Target &target, win::Rect &rect, const histogram &hist, int c);

//-----------------------------------------------------------------------------
//	marker
//-----------------------------------------------------------------------------

struct D2D_Marker {
	com_ptr<ID2D1PathGeometry>	geom;

	ID2D1PathGeometry	*get(d2d::Target &target) {
		if (!geom) {
			com_ptr<ID2D1GeometrySink>	sink;
			target.CreatePath(&geom, &sink);
			sink->BeginFigure(d2d::point(-1, 0), D2D1_FIGURE_BEGIN_FILLED);
			sink->AddLine(d2d::point(+1, 0));
			sink->AddLine(d2d::point(0, 1));
			sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			sink->Close();
		}
		return geom;
	}

	void	Draw(d2d::Target &target, d2d::point pos, d2d::colour col) {
		target.SetTransform(translate(pos) * scale(8, 8));
		target.Fill(get(target), d2d::SolidBrush(target, col));
	}
};

//-----------------------------------------------------------------------------
//	printers
//-----------------------------------------------------------------------------

template<int B> string_accum& DumpTexel(string_accum &acc, _bitmap<B> *bm, int x, int y) {
	ISO_rgba	col	= bm->ScanLine(y)[x];
	if (bm->ClutSize()) {
		acc.format( "%i =>", col.r);
		col = bm->Clut(col.r);
	}
	if (bm->Flags() & BMF_GREY)
		acc.format("%i", col.r);
	else
		acc.format("[%i, %i, %i, %i]/#%02X%02X%02X", col.r, col.g, col.b, col.a, col.r, col.g, col.b);
	return acc;
}

template<int B> string_accum& DumpTexel(string_accum &acc, _HDRbitmap<B> *hdr, int x, int y) {
	HDRpixel	&col = hdr->ScanLine(y)[x];
	if (hdr->Flags() & BMF_GREY)
		acc <<  col.r;
	else
		acc.formati("[%0, %1, %2, %3]", col.r, col.g, col.b, col.a);
	return acc;
}

//-----------------------------------------------------------------------------
//	ViewBitmap_base
//-----------------------------------------------------------------------------

class ViewBitmap_base {
protected:
	enum FLAGS {
		FLIP_X				= 1 << 0,
		FLIP_Y				= 1 << 1,
		DISP_HIST			= 1 << 2,
		BILINEAR			= 1 << 3,
		SHOW_MIPS			= 1 << 4,
		AUTO_SCALE			= 1 << 5,
		HAS_MIPS			= 1 << 6,
		CUBEMAP				= 1 << 7,
		DISP_GRID			= 1 << 8,
		SEPARATE_SLICES		= 1 << 9,
	};
	enum DRAG {
		DRAG_OFF			= 0,
		DRAG_BITMAP			= 1,
		DRAG_MIN			= 2,
		DRAG_MAX			= 3,
		DRAG_HIST			= 4,
		DRAG_HIST_WIDTH		= 5,
		DRAG_HIST_HEIGHT	= 6,
		DRAG_HIST_SIZE		= 7,
		DRAG_GRID_WIDTH		= 8,
		DRAG_GRID_HEIGHT	= 9,
		DRAG_GRID_SIZE		= 10,
		DRAG_SELECTION		= 11,
	};
	flags<FLAGS>			flags;
	DRAG					drag;
	int						frame;
	uint8					channels;
	float					gamma;
	d2d::point				grid;

	float					zoom;
	d2d::point				pos;

	ISO_ptr_machine<void>	bm;
	bitmap					prev_bm;
	ISO_ptr<bitmap_anim>	anim;
	win::Rect				bitmap_rect;
	uint32					bitmap_depth;
	int						num_slices, sel_slice;

	interval<HDRpixel>		col_range;
	interval<float>			disp_range;
	interval<float>			chan_range;
	interval<float>			alpha_range;
	interval<uint32>		depth_range;

	d2d::point	ClientToTexel0(const d2d::point &pt) {
		return (pt - pos) / zoom;
	}

	int		CalcSlice(d2d::point &ret);
	int		CalcMip(d2d::point &ret);

	void	Autoscale(int sw, int sh);
	bool	SetBitmap(const ISO_ptr_machine<void> &p);
	void	UpdateBitmap(d2d::Target &target, ID2D1Bitmap *d2d_bitmap);
	void	UpdateBitmapRect();

	void	DrawBitmapPaint(d2d::Target &target, win::Rect &rect, ID2D1Bitmap *d2d_bitmap);
	void	DrawSelection(d2d::Target &target, win::Rect &rect);
	void	DrawGammaCurve(d2d::Target &target, win::Rect &rect);
	string_accum&	DumpTexelInfo(string_accum &sa, float x, float y, int slice, int mip);

	ViewBitmap_base(): flags(0), drag(DRAG_OFF), channels(CHAN_RGB), gamma(1), grid(16, 16), zoom(1), pos(0, 0), bitmap_rect(0, 0, 0, 0), sel_slice(-1) {}
public:
	static bool Matches(const ISO::Type *type);
};

template<typename T, typename V, int B> T* test_cast(const ISO::ptr<V, B> &p, int crit = 0) {
	return p.IsType(ISO::getdef<T>(), crit) ? (T*)p : nullptr;
}

} //namespace app
