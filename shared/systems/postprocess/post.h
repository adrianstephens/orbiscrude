#ifndef POSTEFFECTS_H
#define POSTEFFECTS_H

#include "iso/iso.h"
#include "graphics.h"
#include "maths/geometry.h"
#include "common/shader.h"

namespace iso {

#include "post.fx.h"

// geometry
//class rectangle;
//class _unit_rect;
//class parallelogram;
//class triangle;
//class quadrilateral;

//-----------------------------------------------------------------------------
// PostEffects
//-----------------------------------------------------------------------------

class PostEffects {
public:
	static layout_post	*shaders;

	GraphicsContext	&ctx;

	struct vertex {
		float2p		pos;
		void	operator=(param(float2) v) { pos = v; }
	};
	struct vertex_col : vertex {
		rgba8		col;
		void set(float x, float y, float r, float g, float b, float a = 1)	{ pos = float2{x,y}; col = float4{r,g,b,a}; }
		void set(param(float2) _pos, param(colour) _col)					{ pos = _pos; col = _col.rgba; }
	};
	struct vertex_tex : vertex {
		float2p		uv;
		void set(float x, float y, float u, float v)		{ pos = float2{x,y}; uv = float2{u,v}; }
		void set(param(float2) _pos, param(float2) _uv)		{ pos = _pos; uv = _uv; }
	};

	struct vertex_tex_col : vertex {
	#ifdef PLAT_WII
		float2p		uv;
		rgba8		col;
	#else
		rgba8		col;
		float2p		uv;
	#endif
		void set(float x, float y, float u, float v, float r, float g, float b, float a = 1)	{ pos = float2{x,y}; uv = float2{u,v}; col = float4{r,g,b,a}; }
		void set(float x, float y, float u, float v, param(colour) _col)						{ pos = float2{x,y}; uv = float2{u,v}; col = _col.rgba; }
		void set(param(float2) _pos, param(float2) _uv, param(colour) _col)						{ pos = _pos; uv = _uv; col = _col.rgba; }
	};

	PostEffects(GraphicsContext &ctx) : ctx(ctx) {}
	void	SetSourceSize(int w, int h);

	static vertex*			PutQuad(vertex*			p, param(float2) p0, param(float2) p1);
	static vertex*			PutQuad(vertex*			p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3);
	static vertex_col*		PutQuad(vertex_col*		p, param(float2) p0, param(float2) p1, param(colour) col);
	static vertex_col*		PutQuad(vertex_col*		p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(colour) col);
	static vertex_tex*		PutQuad(vertex_tex*		p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1);
	static vertex_tex*		PutQuad(vertex_tex*		p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1);
	static vertex_tex_col*	PutQuad(vertex_tex_col*	p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col);
	static vertex_tex_col*	PutQuad(vertex_tex_col*	p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1, param(colour) col);
#ifdef ISO_HAS_RECTS
	static vertex*			PutRect(vertex*			p, param(float2) p0, param(float2) p1);
	static vertex_col*		PutRect(vertex_col*		p, param(float2) p0, param(float2) p1, param(colour) col);
	static vertex_tex*		PutRect(vertex_tex*		p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1);
	static vertex_tex_col*	PutRect(vertex_tex_col*	p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col);
#else
	static vertex*			PutRect(vertex*			p, param(float2) p0, param(float2) p1)															{ return PutQuad(p, p0, p1); }
	static vertex_col*		PutRect(vertex_col*		p, param(float2) p0, param(float2) p1, param(colour) col)										{ return PutQuad(p, p0, p1, col); }
	static vertex_tex*		PutRect(vertex_tex*		p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1)					{ return PutQuad(p, p0, p1, uv0, uv1); }
	static vertex_tex_col*	PutRect(vertex_tex_col*	p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col)	{ return PutQuad(p, p0, p1, uv0, uv1, col); }
#endif
	static vertex*			Put(vertex *p, const unit_rect_t&)	{ return PutRect(p, float2(-one), float2(one)); }
	static vertex*			Put(vertex *p, const rect &rect)	{ return PutRect(p, to<float>(rect.a), to<float>(rect.b)); }

	void			DrawQuad(param(float2) p0, param(float2) p1);
	void			DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3);
	void			DrawQuad(param(float2) p0, param(float2) p1, param(colour) col);
	void			DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(colour) col);
	void			DrawQuad(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1);
	void			DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1);
	void			DrawQuad(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col);
	void			DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1, param(colour) col);
#ifdef ISO_HAS_RECTS
	void			DrawRect(param(float2) p0, param(float2) p1);
	void			DrawRect(param(float2) p0, param(float2) p1, param(colour) col);
	void			DrawRect(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1);
	void			DrawRect(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col);
#else
	void			DrawRect(param(float2) p0, param(float2) p1)																			{ DrawQuad(p0, p1); }
	void			DrawRect(param(float2) p0, param(float2) p1, param(colour) col)															{ DrawQuad(p0, p1, col); }
	void			DrawRect(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1)										{ DrawQuad(p0, p1, uv0, uv1); }
	void			DrawRect(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col)					{ DrawQuad(p0, p1, uv0, uv1, col); }
#endif
	void			Draw(const unit_rect_t&)			{ DrawRect(float2(-one), float2(one)); }
	void			Draw(const rect &rect)				{ DrawRect(to<float>(rect.a), to<float>(rect.b)); }
	void			Draw(const rectangle		&p)		{ DrawRect(p.a, p.b); }
	void			Draw(const parallelogram	&p)		{ DrawQuad(p.corner(CORNER_MM), p.corner(CORNER_PM), p.corner(CORNER_MP), p.corner(CORNER_PP)); }
	void			Draw(const quadrilateral	&p)		{ DrawQuad(p.pt0(), p.pt1(), p.pt2(), p.pt3()); }
	void			Draw(const triangle			&p);

	void			DrawPoly(const float2 *p, int n);

	void			FullScreenQuad(pass *pass, const ISO::Browser &parameters = ISO::Browser());
	void			FullScreenTri(pass *pass, const ISO::Browser &parameters = ISO::Browser());
	void			FullScreenQuad(pass *pass, param(colour) col, const ISO::Browser &parameters = ISO::Browser());
	void			FullScreenTri(pass *pass, param(colour) col, const ISO::Browser &parameters = ISO::Browser());

	void			SetupZOnly(BackFaceCull cull = BFC_BACK, DepthTest depth = DT_USUAL);
	void			RestoreFromZOnly();
	void			FixHDREnvironmentMap(Texture &in, Texture &out);
	void			GenerateMips(Texture &tex, int mips);
};

template<> VertexElements GetVE<PostEffects::vertex_col>();
template<> VertexElements GetVE<PostEffects::vertex_tex>();
template<> VertexElements GetVE<PostEffects::vertex_tex_col>();

}//namespace iso

#endif // POSTEFFECTS_H
