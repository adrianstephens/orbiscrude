#include "postprocess/post.h"
#include "extra/filters.h"
#include "shader.h"
#include "iso/iso.h"
#include "render.h"
#include "profiler.h"
#include "tweak.h"
#include "object.h"

using namespace iso;

TWEAK(bool, SHADOW_ENABLE, true);

#include "shadow.fx.h"

#undef NEAR
#undef FAR

#if defined(PLAT_PS3)
 #define	TEXF_SHADOW1	TEXF_D16_LIN2
 #define	TEXF_SHADOW2	TEXF_R16G16_LIN | TEXF_TILEABLE
 #define	MEM_SHADOW2		MEM_HOST
#elif defined(PLAT_X360)
 #define	TEXF_SHADOW1	TEXF_D24S8
 #define	TEXF_SHADOW2	TEXF_R16G16
#elif defined(PLAT_IOS) || defined(PLAT_ANDROID)
 #define	TEXF_SHADOW1	TexFormat(TEXF_D16 | TEXF_SHADOW)
#elif defined(PLAT_PS4)
 #define	TEXF_SHADOW1	TEXF_D32F
 #define	TEXF_SHADOW2	TEXF_R32G32F
#elif defined(PLAT_PC)
 #define	TEXF_SHADOW1	TEXF_D24S8
#elif defined(PLAT_MAC)
 #define	TEXF_SHADOW1	TEXF_D16
 #define	TEXF_SHADOW2	TEXF_R16G16
#elif defined(PLAT_XONE)
 #define	TEXF_SHADOW1	TEXF_D32F
#endif

#ifdef PLAT_IOS
#define ShadowMapSize0	512
#define ShadowMapSize1	512
#define ShadowMapSize2	1024
#else
#define ShadowMapSize0	768
#define ShadowMapSize1	768
#define ShadowMapSize2	1600
#endif

const float SHADOW_ROUND_TO = .0001f;

struct MapSettings {
	int		res;			// shadow resolution (int)
	float	bias, slope;	// depth bias/slope
	float	esm_c;			// tuned exponential shadow map C
	float	blur;			// exponential shadow map blurring gaussian coefficient to use
//	void	Set(int r, float c, float b) {
//		res = r; bias = 0; slope = -1.25f; esm_c = c; blur = b;
//	}
};
struct PlaneSettings {
	float	near_z, far_z, blend;
	void	Set(float n, float f, float b) {
		near_z = n; far_z = f; blend = b;
	}
};

struct ShadowMapParams {
	int			size;
	float		zstart;
	float		zbias;
	float		zslope;
	float		exp;		// tuned exponential shadow map C
	float		blur;		// blurring gaussian coefficient to use
	float		blend;		// blend to next map
};
typedef ShadowMapParams	ShadowParams[3];

ShadowParams def_shadow_params = {
	{ ShadowMapSize0, 1,	-1, -1.25f, 1250, 1, .2f	},
	{ ShadowMapSize1, 4,	-1, -1.25f, 1250, 1, .2f	},
	{ ShadowMapSize2, 16,	-1, -1.25f, 675, 6.8f		}
};

cuboid Align(param(frustum) f, float align) {
	cuboid	ext	= f.get_box();
	ext.a	= position3(concat(floor(ext.a.v.xy / align) * align, -one));
	ext.b	= position3(concat(ceil (ext.b.v.xy / align) * align, +one));
	return ext;
}

//-----------------------------------------------------------------------------
// Shadows
//-----------------------------------------------------------------------------

class Shadows : public DeleteOnDestroy<Shadows> {
public:
	enum {
		SET_NEAR,
		SET_FAR,
		SET_GLOBAL,
		SET_COUNT,
	};

	enum {
		MAP_NEARFAR,
		MAP_GLOBAL,
		MAP_COUNT,
	};

	static ISO_ptr<fx>		iso_fx;
	static layout_shadow	*shaders;

	MapSettings		settings[SET_COUNT];
	PlaneSettings	planes[SET_COUNT - 1];
	float			expscale[SET_COUNT];		// exponential shadow map C based on map.esm_c and the depth range
	Texture			maps[MAP_COUNT];

	float4x4		global_proj, global_texproj;
	float4			nearfar_mult, nearfar_add, blend_factor;

	int				dirty;
	obb3			extent;
	World			*world;

	enum BlurMode {
		bm_SinglePass,
		bm_Horizontal,
		bm_Vertical,
	};

	enum BlurBufferType {
		bbt_Color,
		bbt_Depth,
		bbt_RG16,
	};

	static float4x4 GetShadowProjection(param(float3) shadow_dir, param(obb3) extent, float *depth_range = NULL);
	static void		BlurCoeffs(BlurMode mode, float blur, const point &size, float3 *samples);

	void	SetDepthRange(float range) {
		for (int i = 0; i < num_elements(settings); i++)
			expscale[i] = settings[i].esm_c * (range / 435.f);
	}

	void	DrawShadowMap(GraphicsContext &ctx, param(float4x4) proj, uint32 flags0, uint32 flags1);
	void	Blur(GraphicsContext &ctx, BlurMode blur_mode, float blur, const Texture &srce, const Texture &dest, float bias, float slope, BlurBufferType buffer_type, int border);
	void	Splat(GraphicsContext &ctx, const Texture &near, const Texture &far, const Texture &global, const Texture &splat);

public:
	void	SetExtent(param(obb3) _extent)	{
		extent = _extent;
	}
	void	SetParams(ShadowParams p)		{
		for (int i = 0; i < 3; i++) {
			settings[i].res		= p[i].size;
			settings[i].bias	= p[i].zbias;
			settings[i].slope	= p[i].zslope;
			settings[i].esm_c	= p[i].exp;
			settings[i].blur	= p[i].blur;
		}
		planes[0].Set(p[0].zstart, p[1].zstart, p[0].blend);
		planes[1].Set(p[1].zstart, p[2].zstart, p[1].blend);
	}

	void	Do(GraphicsContext &ctx, param(float3) shadow_dir, param(float3x4) iview, param(float4x4) proj);

	void	operator()(RenderEvent *re, uint32 extra) {
	#ifdef ISO_HAS_ALPHATEST
		re->ctx.SetAlphaTestEnable(true);
		re->ctx.SetAlphaTest(AT_GREATER, 128);
	#endif
	}
	void	operator()(MoveMessage &m) {
		dirty = 1;
	}

	template<uint32 CRC> void	Create(const CreateParams &cp, crc32 id, const void *p);
	Shadows(World *w);
};

ISO_ptr<fx>		Shadows::iso_fx;
layout_shadow*	Shadows::shaders;

struct ShadowsMaker : Handles2<ShadowsMaker, WorldEvent>  {
	void	operator()(WorldEvent *ev) {
		if (ev->state == WorldEvent::BEGIN)
			::new Shadows(ev->world);
	}
} shadows_maker;

Shadows::Shadows(World *w) : DeleteOnDestroy<Shadows>(w), dirty(1), world(w) {
	if (!shaders)
		shaders = (iso_fx = ISO::root("data")["shadow"]) ? (layout_shadow*)(ISO_ptr<technique>*)*iso_fx : 0;

	/*	TBD
	static ShaderVal	m[] = {
		{"_shadow_nearfar_map", maps[MAP_NEARFAR]	},
		{"_shadow_global_map",  maps[MAP_GLOBAL]	},
		{"shadow_global_proj",  &global_texproj		},
		{"shadow_nearfar_mult", &nearfar_mult		},
		{"shadow_nearfar_add",  &nearfar_add		},
		{"shadow_expscale",     expscale			},
		{"shadow_blend",        &blend_factor		},
	};
	*/

	AddShaderParameter(ISO_CRC("_shadow_nearfar_map", 0xbad36a03),	maps[MAP_NEARFAR]);
	AddShaderParameter(ISO_CRC("_shadow_global_map", 0x78f3b825),	maps[MAP_GLOBAL]);
	AddShaderParameter(ISO_CRC("shadow_global_proj", 0xe5969b0f),	global_texproj);
	AddShaderParameter(ISO_CRC("shadow_nearfar_mult", 0x9dfda779),	nearfar_mult);
	AddShaderParameter(ISO_CRC("shadow_nearfar_add", 0x74a2f491),	nearfar_add);
	AddShaderParameter(ISO_CRC("shadow_expscale", 0x9bdfebf2),		expscale);
	AddShaderParameter(ISO_CRC("shadow_blend", 0x4ddf3d55), 		blend_factor);

	SetParams(def_shadow_params);
	//void IsoLinkAdd(tag2 id, Texture &s);
	//IsoLinkAdd("shadow", maps[0]);
	w->AddHandler<CreateMessage>(CreationCRC<ISO_CRC("ShadowExtent", 0x55e92767)>(this));
	w->AddHandler<CreateMessage>(CreationCRC<ISO_CRC("ShadowParams", 0xfc2970dc)>(this));
	w->SetHandler<MoveMessage>(this);
	//maps[MAP_GLOBAL].Init(TEXF_SHADOW1, settings[SET_GLOBAL].res, settings[SET_GLOBAL].res);
}

template<> void	Shadows::Create<ISO_CRC("ShadowExtent", 0x55e92767)>(const CreateParams &cp, crc32 id, const void *p) {
	SetExtent(obb3(*(float3x4p*)ISO::Browser(ISO::GetPtr(p))["matrix"]));
}
template<> void	Shadows::Create<ISO_CRC("ShadowParams", 0xfc2970dc)>(const CreateParams &cp, crc32 id, const void *p) {
	SetParams((ShadowParams&)p);
}

//ViewSurface	vs("ShaderMap", shadows.maps[0], float4x4(translate(-4,-4,-4) * scale(10,10,10)));

float4x4 Shadows::GetShadowProjection(param(float3) shadow_dir, param(obb3) extent, float *depth_range) {
	// Find the orthogonal projection of each axis of extent
	float3		projx = orthogonalise(extent.x, shadow_dir);
	float3		projy = orthogonalise(extent.y, shadow_dir);
	float3		projz = orthogonalise(extent.z, shadow_dir);
	float		normx = len2(projx);
	float		normy = len2(projy);
	float		normz = len2(projz);

	// Choose axis with the longest "shadow" to be the y axis (which will best-align shadow to the extent)
	float3		y = normalise(normx > normy && normx > normz ? projx : normy > normz ? projy : projz);
	float3		z = -shadow_dir;
	float3		x = cross(y, z);

	float3x3	shadowview	= transpose(float3x3(x, cross(z, x), z));
	cuboid		box			= (float3x4(shadowview) * extent).get_box();

	if (depth_range)
		*depth_range = box.extent().z;

	// Transform it into [-1, 1]^3 space
	return float4x4(box.inv_matrix() * shadowview);
}

void Shadows::BlurCoeffs(BlurMode mode, float blur, const point &size, float3 *samples) {
	switch(mode) {
		case bm_SinglePass: {
			float2	scale	= reciprocal(to<float>(size));
			samples[0] = concat(scale, .25f);
			samples[1] = float3{scale.x, -scale.y, .25f};
			break;
		}
		case bm_Horizontal:
			gaussian_kernel(samples, 4, blur, 1, size.x, 0.01f);
			break;

		case bm_Vertical:
			gaussian_kernel(samples, 4, blur, 1, size.y, 0.01f);
			break;
	}
}

void Shadows::Blur(GraphicsContext &ctx, BlurMode mode, float blur, const Texture &srce, const Texture &dest, float bias, float slope, BlurBufferType buffer_type, int border) {
	point	size	= srce.Size();
	float3	blur_samples[4];
	BlurCoeffs(mode, blur, size, blur_samples);

	AddShaderParameter(ISO_CRC("_shadowmap", 0x5e77f0a0),	srce);
	AddShaderParameter(ISO_CRC("blur_samples", 0x23bffceb),	blur_samples);

	if (buffer_type == bbt_Depth) {
		ctx.SetRenderTarget(Surface());
		ctx.SetZBuffer(dest.GetSurface());
		PostEffects(ctx).SetupZOnly(BFC_NONE, DT_ALWAYS);
	} else {
		ctx.SetRenderTarget(dest.GetSurface());
		ctx.SetZBuffer(Surface());
		ctx.SetMask(CM_ALL);
	}

	if (border > 0)
		ctx.ClearZ();

	ctx.SetWindow(rect(point(border), size - point(border * 2)));
	ctx.SetDepthBias(bias, slope);
	ctx.SetBlendEnable(false);
#ifdef ISO_HAS_ALPHATEST
	ctx.SetAlphaTestEnable(false);
#endif

	Set(ctx, (*shaders->shadowblur)[buffer_type]);
	float2	half_texel	= 0.5f / to<float>(size);
	PostEffects(ctx).DrawRect(float2(-one), float2(one), half_texel, float2(one) + half_texel);

	ctx.Resolve(buffer_type == bbt_Depth ? RT_DEPTH : RT_COLOUR0);
	ctx.SetDepthBias(0, 0);
	ctx.SetMask(CM_ALL);
}

void Shadows::Splat(GraphicsContext &ctx, const Texture &near_z, const Texture &far_z, const Texture &global, const Texture &splat) {
	AddShaderParameter(ISO_CRC("_near", 0x2b696c0a),	near_z);
	AddShaderParameter(ISO_CRC("_far", 0x3c9d3d8c), 	far_z);
	AddShaderParameter(ISO_CRC("_global", 0x11366c00), 	global);

	ctx.SetRenderTarget(splat.GetSurface());
	ctx.SetZBuffer(Surface());

	ctx.SetDepthTestEnable(false);
	ctx.SetDepthWriteEnable(false);
	ctx.SetBlendEnable(false);
#ifdef ISO_HAS_ALPHATEST
	ctx.SetAlphaTestEnable(false);
#endif
	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetMask(CM_ALL);

#ifdef PLAT_X360
	for (int i = 0; i < 3; i++) {
		ctx.SetUVMode(i, ALL_CLAMP);
		ctx.SetTexFilter(i, TF_LINEAR_LINEAR_NEAREST);
	}
	PostEffects(ctx).FullScreenQuad((*shaders->splat)[0]);
	for (int i = 0; i < 3; i++) {
		ctx.SetUVMode(i, ALL_WRAP);
		ctx.SetTexFilter(i, TF_LINEAR_LINEAR_LINEAR);
	}
#else
	PostEffects(ctx).FullScreenQuad((*shaders->splat)[0]);
#endif
}

void Shadows::DrawShadowMap(GraphicsContext &ctx, param(float4x4) proj, uint32 flags0, uint32 flags1) {
	PROFILE_EVENT(ctx, "Shadow");

	ShaderConsts	rs(identity, proj, world->Time());
	RenderEvent	re(ctx, rs, flags0, flags1);
	re.AddRenderItem(this, MakeKey(RS_TRANSP, 0), RS_TRANSP);
	re.Collect(world);
	re.Render();

	ctx.Resolve(RT_DEPTH);
}

cuboid MakeRegion(param(float3x4) iview, param(float4x4) proj, param(float4x4) global_proj, const PlaneSettings &p) {
	return Align(frustum(global_proj * iview * inverse(set_perspective_z(proj, p.near_z, p.far_z))), SHADOW_ROUND_TO);
}

void Shadows::Do(GraphicsContext &ctx, param(float3) shadow_dir, param(float3x4) iview, param(float4x4) proj) {
	if (!SHADOW_ENABLE)
		return;

#ifdef PLAT_X360 // Make sure it's not set to a 64-bit render target, which it can be even if the render target is set to NULL - 360 peculiarity
	ctx.SetRenderTarget(Surface((TexFormat)D3DFMT_LE_X8R8G8B8, 64, 64));
#endif
	ctx.SetRenderTarget(Surface());
	PostEffects(ctx).SetupZOnly(BFC_FRONT);

	// Global shadows
	if (dirty > 0) {
		if (--dirty)
			return;
		if (volume(extent) <= 0.f) {
		#if 1
			extent = obb3(cuboid(position3(-100,-100,-100), position3(100,100,100)));
		#else
			extent = obb3(RenderObject::GetTree().GetBox());
			if (extent.volume() <= 0.f) {
				dirty = 1;
				return;
			}
		#endif
		}
		float		depth_range;
		MapSettings	&set	= settings[SET_GLOBAL];
		Texture		&map	= maps[MAP_GLOBAL];
		global_proj			= GetShadowProjection(shadow_dir, extent, &depth_range);
		global_texproj		= translate(0,0,-0.001f) * map_fix(global_proj);

	  #ifdef PLAT_PS3
		depth_range	= -depth_range;
	  #endif
		SetDepthRange(depth_range);

		map.Init(TEXF_SHADOW1, set.res, set.res);
		ctx.SetDepthBias(set.bias, set.slope);
		ctx.SetZBuffer(map.GetSurface());
		ctx.ClearZ();
	  #ifdef PLAT_IOS
		ctx.SetWindow(rect(point(1), point(set.res - 1)));
	  #endif

	  #ifdef PLAT_WII
		DrawShadowMap(global_proj, RMASK_DYNAMIC | RMASK_SORT, RMASK_UPPERSHADOW | RMASK_MIDDLESHADOW);
	  #else
		DrawShadowMap(ctx, global_proj, RMASK_DYNAMIC | RMASK_SORT | RMASK_NOSHADOW, 0);
	  #endif

		ctx.SetDepthBias(0, 0);

	#ifdef PLAT_X360
		Blur(ctx, bm_Horizontal, set.blur, map, map, set.bias, set.slope, bbt_Depth, 1);
		Blur(ctx, bm_Vertical, set.blur, map, map, set.bias, set.slope, bbt_Depth, 1);
	#elif defined(PLAT_PS3)
		Texture	_shadowmap(TEXF_D16_LIN2, set.res, set.res);
		ctx.SetZBuffer(_shadowmap.GetSurface());
		ctx.SetRenderTarget(Surface());
		ctx.ClearZ();
		Blur(ctx, bm_Horizontal, set.blur, map, _shadowmap, set.bias, set.slope, bbt_Depth, 1);
		Blur(ctx, bm_Vertical, set.blur, _shadowmap, map, set.bias, set.slope, bbt_Depth, 1);
	#endif
	//	PostEffects::GenerateMips(map, 8);
	}

	// Near + farPlane shadows
	int	res = settings[0].res;

#ifdef PLAT_IOS
	maps[MAP_NEARFAR].Init(TEXF_SHADOW1, res * 2, res);
	ctx.SetZBuffer(maps[MAP_NEARFAR].GetSurface());
	ctx.Clear(colour(zero), true);
//	ctx.ClearZ();
#else
	Texture		temp_map[2];
	temp_map[0].Init(TEXF_SHADOW1, res, res);
	temp_map[1].Init(TEXF_SHADOW1, res, res);
#endif
	cuboid		regions[2];
	for (int i = 0; i < num_elements(planes); i++) {
		MapSettings		&set	= settings[i];

		regions[i] = MakeRegion(iview, proj, global_proj, planes[i]);
		ctx.SetDepthBias(set.bias, set.slope);

#ifdef PLAT_IOS
		ctx.SetWindow(rect(point{res * i, 0}, point{res, res}));
		DrawShadowMap(ctx, float4x4(regions[i].inv_matrix()) * global_proj, RMASK_FADING | RMASK_SORT | RMASK_NOSHADOW, RMASK_DYNAMIC);
#else
		ctx.SetZBuffer(temp_map[i].GetSurface());
		ctx.ClearZ();
		DrawShadowMap(ctx, float4x4(regions[i].inv_matrix()) * global_proj, RMASK_FADING | RMASK_SORT | RMASK_NOSHADOW, RMASK_DYNAMIC);
#endif
	}

	ctx.SetDepthBias(0, 0);

	float4	s		= one / concat(regions[0].extent().xy, regions[1].extent().xy);
	float4	t		= concat(regions[0].centre().v.xy, regions[1].centre().v.xy);
	nearfar_mult	= s * 2;
	nearfar_add		= (-t - one) * s + half;

#ifndef PLAT_IOS
	static const float	cutoff = .99f;		// percentage of texture to cutoff at so we don't sample outside it
	blend_factor	= concat(
		solve_line(float2{cutoff - planes[0].blend, one}, float2{cutoff, zero}),
		solve_line(float2{cutoff - planes[1].blend, one}, float2{cutoff, zero})
	);

	MapSettings	&set		= settings[SET_NEAR];
	Texture		&dst		= maps[MAP_NEARFAR];

  #ifdef PLAT_X360
	dst.Init(TEXF_SHADOW2, set.res, set.res);
	Splat(ctx, temp_map[0], temp_map[1], maps[MAP_GLOBAL], dst);
	Blur(ctx, bm_SinglePass, set.blur, dst, dst, set.bias, set.slope, bbt_RG16, 0);
  #elif defined(PLAT_PS3)
	dst.Init(TEXF_SHADOW2, set.res, set.res, 1, 1, MEM_HOST);
	Texture temp(TexFormat(TEXF_R16G16_LIN | TEXF_TILEABLE), set.res, set.res);
	Splat(ctx, temp_map[0], temp_map[1], maps[MAP_GLOBAL], temp);
	Blur(ctx, bm_SinglePass, set.blur, temp, dst, set.bias, set.slope, bbt_RG16, 0);
  #elif defined(PLAT_PS4)
	dst.Init(TEXF_SHADOW2, set.res, set.res);
	Texture temp(TEXF_SHADOW2, set.res, set.res);
	Splat(ctx, temp_map[0], temp_map[1], maps[MAP_GLOBAL], temp);
	Blur(ctx, bm_SinglePass, set.blur, temp, dst, set.bias, set.slope, bbt_RG16, 0);
  #endif

	ctx.SetRenderTarget(Surface());
	ctx.SetZBuffer(Surface());

  #if !defined PLAT_PS4 && !defined PLAT_XONE
	ctx.SetTexture(maps[MAP_NEARFAR],	14);
	ctx.SetTexture(maps[MAP_GLOBAL],	15);
  #endif

  #ifdef PLAT_X360
	ctx.SetUVMode(14, ALL_CLAMP);
	ctx.SetTexFilter(14, TF_LINEAR_LINEAR_NEAREST);
	ctx.SetUVMode(15, ALL_CLAMP);
	ctx.SetTexFilter(15, TF_ANISO_ANISO_LINEAR);
	ctx.SetSamplerState(15, TS_ANISO_MAX,	4);
  #endif

#endif
	PostEffects(ctx).RestoreFromZOnly();
}

// TEMP!!!
void DoShadows(GraphicsContext &ctx, param(float3) shadow_dir, param(float3x4) iview, param(float4x4) proj) {
	static Shadows shadows(World::Current());
	shadows.Do(ctx, shadow_dir, iview, proj);
}

namespace iso {

	template<> void TypeHandlerCRC<ISO_CRC("ShadowExtent", 0x55e92767)>::Create(const CreateParams &cp, crc32 id, const void *t) {
		new Shadows(cp.world);
	}

	template<> void TypeHandlerCRC<ISO_CRC("ShadowParams", 0xfc2970dc)>::Create(const CreateParams &cp, crc32 id, const void *t) {
		new Shadows(cp.world);
	}

} // namespace iso
