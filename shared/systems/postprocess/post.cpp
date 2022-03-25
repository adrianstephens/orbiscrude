#include "post.h"
#include "utilities.h"
#include "base/algorithm.h"
#include "maths/geometry.h"

namespace iso {

layout_post *PostEffects::shaders;

//-----------------------------------------------------------------------------
//	PostEffectSystem
//-----------------------------------------------------------------------------

class PostSystem : public Handles2<PostSystem, AppEvent> {
	ISO_ptr<fx>	iso_fx;
public:
	void	operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN)
			PostEffects::shaders = (iso_fx = ISO::root("data")["post"]) ? (layout_post*)(ISO_ptr<technique>*)*iso_fx : 0;
	}
	~PostSystem() {
	}
} post_system;

//-----------------------------------------------------------------------------
// static helpers
//-----------------------------------------------------------------------------

template<typename T> struct VE;

template<> VertexElements GetVE<PostEffects::vertex>() {
	static VertexElement ve[] = {
		VertexElement(&PostEffects::vertex::pos, "position"_usage)
	};
	return ve;
};

template<> VertexElements GetVE<PostEffects::vertex_col>() {
	static VertexElement ve[] = {
		VertexElement(&PostEffects::vertex_col::pos, "position"_usage),
		VertexElement(&PostEffects::vertex_col::col, "colour"_usage)
	};
	return ve;
};

template<> VertexElements GetVE<PostEffects::vertex_tex>() {
	static VertexElement ve[] = {
		VertexElement(&PostEffects::vertex_tex::pos, "position"_usage),
		VertexElement(&PostEffects::vertex_tex::uv, "texcoord"_usage)
	};
	return ve;
};

template<> VertexElements GetVE<PostEffects::vertex_tex_col>() {
	static VertexElement ve[] = {
		VertexElement(&PostEffects::vertex_tex_col::pos, "position"_usage),
		VertexElement(&PostEffects::vertex_tex_col::col, "colour"_usage),
		VertexElement(&PostEffects::vertex_tex_col::uv, "texcoord"_usage)
	};
	return ve;
};

void PostEffects::SetSourceSize(int w, int h) {
#ifndef PLAT_X360
	static struct X {
		float2	x;
		X() { AddShaderParameter("inv_tex_size", x); }
	} x;
	x.x = reciprocal(float2{float(w), float(h)});
#endif
}

void PostEffects::SetupZOnly(BackFaceCull cull, DepthTest depth) {
	ctx.SetDepthTestEnable(true);
	ctx.SetDepthWriteEnable(true);
	ctx.SetDepthTest(depth);
	ctx.SetBlendEnable(false);
#ifdef ISO_HAS_ALPHATEST
	ctx.SetAlphaTestEnable(false);
#endif
	ctx.SetBackFaceCull(cull);
	ctx.SetMask(CM_NONE);
}

void PostEffects::RestoreFromZOnly() {
	ctx.SetZBuffer(Surface());
	ctx.SetMask(CM_ALL);
}

template<template<class> class T, typename P> inline void _PutPos(T<P> p, param(float2) p0, param(float2) p1) {
	p[0].pos = p0;
	p[1].pos = float2{p1.x, p0.y};
	p[2].pos = float2{p0.x, p1.y};
	p[3].pos = p1;
}
template<template<class> class T, typename P> inline void _PutPos(T<P> p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3) {
	p[0].pos = p0;
	p[1].pos = p1;
	p[2].pos = p2;
	p[3].pos = p3;
}
template<template<class> class T, typename P> inline void _PutCol(T<P> p, param(colour) col) {
	p[0].col = p[1].col = p[2].col = p[3].col = col.rgba;
}
template<template<class> class T, typename P> inline void _PutTex(T<P> p, param(float2) uv0, param(float2) uv1) {
	p[0].uv = uv0;
	p[1].uv = float2{uv1.x, uv0.y};
	p[2].uv = float2{uv0.x, uv1.y};
	p[3].uv = uv1;
}

template<template<class> class T, typename P> inline P *_Put(T<P> p, param(float2) p0, param(float2) p1) {
	_PutPos(p, p0, p1);
	return p.next();
}
template<template<class> class T, typename P> inline P *_Put4(T<P> p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3) {
	_PutPos(p, p0, p1, p2, p3);
	return p.next();
}
template<template<class> class T, typename P> inline P *_Put(T<P> p, param(float2) p0, param(float2) p1, param(colour) col) {
	_PutPos(p, p0, p1);
	_PutCol(p, col);
	return p.next();
}
template<template<class> class T, typename P> inline P *_Put4(T<P> p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(colour) col) {
	_PutPos(p, p0, p1, p2, p3);
	_PutCol(p, col);
	return p.next();
}
template<template<class> class T, typename P> inline P *_Put(T<P> p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1) {
	_PutPos(p, p0, p1);
	_PutTex(p, uv0, uv1);
	return p.next();
}
template<template<class> class T, typename P> inline P *_Put4(T<P> p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1) {
	_PutPos(p, p0, p1, p2, p3);
	_PutTex(p, uv0, uv1);
	return p.next();
}
template<template<class> class T, typename P> inline P *_Put(T<P> p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col) {
	_PutPos(p, p0, p1);
	_PutTex(p, uv0, uv1);
	_PutCol(p, col);
	return p.next();
}
template<template<class> class T, typename P> inline P *_Put4(T<P> p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1, param(colour) col) {
	_PutPos(p, p0, p1, p2, p3);
	_PutTex(p, uv0, uv1);
	_PutCol(p, col);
	return p.next();
}

PostEffects::vertex *PostEffects::PutQuad(vertex *p, param(float2) p0, param(float2) p1) {
	return _Put<QuadListT,vertex>(p, p0, p1);
}
PostEffects::vertex *PostEffects::PutQuad(vertex *p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3) {
	return _Put4<QuadListT,vertex>(p, p0, p1, p2, p3);
}
PostEffects::vertex_col *PostEffects::PutQuad(PostEffects::vertex_col *p, param(float2) p0, param(float2) p1, param(colour) col) {
	return _Put<QuadListT,vertex_col>(p, p0, p1, col);
}
PostEffects::vertex_col *PostEffects::PutQuad(PostEffects::vertex_col *p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(colour) col) {
	return _Put4<QuadListT,vertex_col>(p, p0, p1, p2, p3, col);
}
PostEffects::vertex_tex *PostEffects::PutQuad(PostEffects::vertex_tex *p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1) {
	return _Put<QuadListT,vertex_tex>(p, p0, p1, uv0, uv1);
}
PostEffects::vertex_tex *PostEffects::PutQuad(PostEffects::vertex_tex *p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1) {
	return _Put4<QuadListT,vertex_tex>(p, p0, p1, p2, p3, uv0, uv1);
}
PostEffects::vertex_tex_col *PostEffects::PutQuad(PostEffects::vertex_tex_col *p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col) {
	return _Put<QuadListT,vertex_tex_col>(p, p0, p1, uv0, uv1, col);
}
PostEffects::vertex_tex_col *PostEffects::PutQuad(PostEffects::vertex_tex_col *p, param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1, param(colour) col) {
	return _Put4<QuadListT,vertex_tex_col>(p, p0, p1, p2, p3, uv0, uv1, col);
}

void PostEffects::DrawQuad(param(float2) p0, param(float2) p1) {
	_Put<SingleQuadT,vertex>(ImmediateStream<vertex>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1);
}
void PostEffects::DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3) {
	_Put4<SingleQuadT,vertex>(ImmediateStream<vertex>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1, p2, p3);
}
void PostEffects::DrawQuad(param(float2) p0, param(float2) p1, param(colour) col) {
	_Put<SingleQuadT,vertex_col>(ImmediateStream<vertex_col>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1, col);
}
void PostEffects::DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(colour) col) {
	_Put4<SingleQuadT,vertex_col>(ImmediateStream<vertex_col>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1, p2, p3, col);
}
void PostEffects::DrawQuad(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1) {
	_Put<SingleQuadT,vertex_tex>(ImmediateStream<vertex_tex>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1, uv0, uv1);
}
void PostEffects::DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1) {
	_Put4<SingleQuadT,vertex_tex>(ImmediateStream<vertex_tex>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1, p2, p3, uv0, uv1);
}
void PostEffects::DrawQuad(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col) {
	_Put<SingleQuadT,vertex_tex_col>(ImmediateStream<vertex_tex_col>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1, uv0, uv1, col);
}
void PostEffects::DrawQuad4(param(float2) p0, param(float2) p1, param(float2) p2, param(float2) p3, param(float2) uv0, param(float2) uv1, param(colour) col) {
	_Put4<SingleQuadT,vertex_tex_col>(ImmediateStream<vertex_tex_col>(ctx, prim<SingleQuadT>(), verts<SingleQuadT>()).begin(), p0, p1, p2, p3, uv0, uv1, col);
}

#ifdef ISO_HAS_RECTS

PostEffects::vertex *PostEffects::PutRect(vertex *p, param(float2) p0, param(float2) p1) {
	p[0].pos = p0;
	p[1].pos = {p1.x, p0.y};
	p[2].pos = {p0.x, p1.y};
	return p + 3;
}
PostEffects::vertex_col *PostEffects::PutRect(vertex_col *p, param(float2) p0, param(float2) p1, param(colour) col) {
	p[0].pos = p0;
	p[1].pos = {p1.x, p0.y};
	p[2].pos = {p0.x, p1.y};
	p[0].col = p[1].col = p[2].col = col.rgba;
	return p + 3;
}
PostEffects::vertex_tex *PostEffects::PutRect(vertex_tex *p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1) {
	p[0].pos = p0;
	p[1].pos = {p1.x, p0.y};
	p[2].pos = {p0.x, p1.y};
	p[0].uv = uv0;
	p[1].uv = {uv1.x, uv0.y};
	p[2].uv = {uv0.x, uv1.y};
	return p + 3;
}
PostEffects::vertex_tex_col *PostEffects::PutRect(vertex_tex_col *p, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col) {
	p[0].pos = p0;
	p[1].pos = {p1.x, p0.y};
	p[2].pos = {p0.x, p1.y};
	p[0].uv = uv0;
	p[1].uv = {uv1.x, uv0.y};
	p[2].uv = {uv0.x, uv1.y};
	p[0].col = p[1].col = p[2].col = col.rgba;
	return p + 3;
}
void PostEffects::DrawRect(param(float2) p0, param(float2) p1) {
	PutRect(ImmediateStream<vertex>(ctx, PRIM_RECTLIST, 3).begin(), p0, p1);
}
void PostEffects::DrawRect(param(float2) p0, param(float2) p1, param(colour) col) {
	PutRect(ImmediateStream<vertex_col>(ctx, PRIM_RECTLIST, 3).begin(), p0, p1, col);
}
void PostEffects::DrawRect(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1) {
	PutRect(ImmediateStream<vertex_tex>(ctx, PRIM_RECTLIST, 3).begin(), p0, p1, uv0, uv1);
}
void PostEffects::DrawRect(param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1, param(colour) col) {
	PutRect(ImmediateStream<vertex_tex_col>(ctx, PRIM_RECTLIST, 3).begin(), p0, p1, uv0, uv1, col);
}

#endif

void PostEffects::Draw(const triangle &p) {
	ImmediateStream<float2p>	im(ctx, PRIM_TRILIST, 3);
	float2p	*d = im.begin();
	d[0] = p.corner(0).v;
	d[1] = p.corner(1).v;
	d[2] = p.corner(2).v;
}

#ifdef ISO_HAS_TRIFAN
void PostEffects::DrawPoly(const float2 *p, int n) {
	copy_n(p, ImmediateStream<vertex>(ctx, PRIM_TRIFAN, n).begin(), n);
}
#else
void PostEffects::DrawPoly(const float2 *p, int n) {
	ImmediateStream<vertex>	im(ctx, PRIM_TRILIST, n);
	auto	*d = im.begin();
	for (int i = 0; i < n; i++)
		*d++ = p[i & 1 ? n - ((i + 1) >> 1) : i >> 1];
}
#endif

void PostEffects::FullScreenQuad(pass *pass, const ISO::Browser &parameters) {
	Set(ctx, pass, parameters);
	DrawRect(float2(-one), float2(one), float2(zero), float2(one));
}

void PostEffects::FullScreenTri(pass *pass, const ISO::Browser &parameters) {
	Set(ctx, pass, parameters);
	ImmediateStream<vertex_tex>	ims(ctx, PRIM_TRILIST, 3);
	ims[0].set( 3, -1, 2, 0);
	ims[1].set(-1, -1, 0, 0);
	ims[2].set(-1,  3, 0, 2);
}

void PostEffects::FullScreenQuad(pass *pass, param(colour) col, const ISO::Browser &parameters) {
	Set(ctx, pass, parameters);
	DrawRect(float2(-one), float2(one), float2(zero), float2(one), col);
}

void PostEffects::FullScreenTri(pass *pass, param(colour) col, const ISO::Browser &parameters) {
	Set(ctx, pass, parameters);
	ImmediateStream<vertex_tex_col>	ims(ctx, PRIM_TRILIST, 3);
	ims[0].set( 3, -1, 2, 0, col);
	ims[1].set(-1, -1, 0, 0, col);
	ims[2].set(-1,  3, 0, 2, col);
}
//-----------------------------------------------------------------------------
// FixHDREnvironmentMap
//-----------------------------------------------------------------------------

void PostEffects::FixHDREnvironmentMap(Texture &in, Texture &out) {
	float	face;

	AddShaderParameter(ISO_CRC("face", 0x05147b67), face);
	AddShaderParameter(ISO_CRC("cube_samp", 0x29cd67b7), in);
	ctx.SetBlendEnable(false);
#ifdef ISO_HAS_ALPHATEST
	ctx.SetAlphaTestEnable(false);
#endif

	pass	*cubemapface_exp	= (*shaders->cubemapface_exp)[0];
	for (int i = 0; i < 6; i++) {
		face = i;
		ctx.SetRenderTarget(out.GetSurface(CubemapFace(i), 0));
		FullScreenQuad(cubemapface_exp);
	}
#if defined(PLAT_IOS) || defined(PLAT_MAC)
	ctx.GenerateMips(out);
#else
	pass	*cubemapface		= (*shaders->cubemapface)[0];
	AddShaderParameter(ISO_CRC("cube_samp", 0x29cd67b7), out);
	for (int i = 0; i < 6; i++) {
		face = i;
		for (int m = 1; m < 8; m++) {
#ifndef PLAT_PC
			//ctx.SetSamplerState(0, TS_MIP_MAX, m - 1);
#endif
			ctx.SetRenderTarget(out.GetSurface(CubemapFace(i), m));
			FullScreenQuad(cubemapface);
		}
	}
#ifndef PLAT_PC
	//ctx.SetSamplerState(0, TS_MIP_MAX, TSV_MAXLOD);
#endif
#endif
	ctx.SetRenderTarget(Surface());
}

//-----------------------------------------------------------------------------
// GenerateMips
//-----------------------------------------------------------------------------

#if 0
void PostEffects::GenerateMips(Texture &tex, int mips) {

	int	width = tex.Width(), height = tex.Height();
	// Z mips
	if (tex.IsDepth()) {
#if !defined(PLAT_IOS) && !defined(PLAT_MAC)
		ctx.SetDepthTestEnable(true);
		ctx.SetDepthWriteEnable(true);
		ctx.SetBlendEnable(false);
		ctx.SetAlphaTestEnable(false);
		ctx.SetMask(0);
		AddShaderParameter(ISO_CRC("_zbuffer", 0x21a4e037), tex);

		for (int m = 1; m < 8; m++) {
			ctx.SetZBuffer(tex.GetSurface(m));
			ctx.SetRenderTarget(Surface());
			ctx.ClearZ();
			ctx.SetWindow(rect(1, 1, (width >> m) - 2, (height >> m) - 2));
			Set((*shaders->depth_mip)[0]);
#ifndef PLAT_PC
			ctx.SetSamplerState(0, TS_MIP_MAX, m - 1);
#endif
			DrawRect(float2(-one), float2(one), float2(zero), float2(one));
			ctx.Resolve(RT_DEPTH);
		}

		// Restore state
		ctx.SetRenderTarget(Surface());
		ctx.SetZBuffer(Surface());

		ctx.SetDepthTestEnable(true);
		ctx.SetDepthWriteEnable(true);
		ctx.SetBackFaceCull(BFC_BACK);
		ctx.SetMask(CM_ALL);

		ctx.SetDepthTest(DT_USUAL);
#ifndef PLAT_PC
		ctx.SetSamplerState(0, TS_MIP_MAX, TSV_MAXLOD);
#endif
#endif
	} else {
	}
}
#endif

}//namespace iso
