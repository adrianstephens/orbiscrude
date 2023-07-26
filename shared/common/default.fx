#include "common.fxh"
#include "lighting.fxh"
#include "font.fxh"
#include "pack.fxh"

#define SHADOW_VO
#define SHADOW_VS(v,p)
#define GETSHADOW(v)	1

sampler_def2D(diffuse_samp, FILTER_MIN_MAG_MIP_LINEAR;);
sampler_def2D(diffuse_samp2,FILTER_MIN_MAG_MIP_LINEAR;);
sampler_def2D(normal_samp, FILTER_MIN_MAG_MIP_LINEAR;);

sampler_def2D(linear_samp,
	FILTER_MIN_MAG_MIP_LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

sampler_def2D(linear_samp2,
	FILTER_MIN_MAG_MIP_LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

sampler_defCUBE(sky_samp,
	FILTER_MIN_MAG_MIP_LINEAR;
#ifdef PLAT_WII
	ADDRESSU = WRAP;
	ADDRESSV = WRAP;
#else
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
#endif
);

//------------------------------------

static const float	glossiness		= 60;
static const float	flip_normals	= 1;
float4			clip_tint		= {1,0,0,1};
float4			matrices[64];
int				mip;

//------------------------------------

struct S_Ray {
	float4	pos		: POSITION_OUT;
	float4	p0		: TEXCOORD0;
	float4	p1		: TEXCOORD1;
};

struct S_Col {
	float4	pos		: POSITION_OUT;
	float4	colour	: COLOR;
};

struct S_Tex {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
};

struct S_ColTex {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
	float4	colour	: COLOR;
};

struct S_ColTex2 {
	float4	pos		: POSITION_OUT;
	float4	uv01	: TEXCOORD0;
	float4	colour	: COLOR;
};

struct S_Lighting {
	float4	position: POSITION_OUT;
	float4	ambient	: AMBIENT;
	float3	normal	: NORMAL;
	float3	worldpos: TEXCOORD3;
	fogtype	fog		: FOG;
	SHADOW_VO
};

struct S_Lighting4 {
	float4	position: POSITION_OUT;
	float4	ambient	: AMBIENT;
	float3	normal	: NORMAL;
	noperspective float4 worldpos: TEXCOORD3;
	fogtype	fog		: FOG;
	SHADOW_VO
};

struct S_LightingTex {
	float4	position: POSITION_OUT;
	float4	ambient	: AMBIENT;
	float2	uv		: TEXCOORD0;
	float3	normal	: NORMAL;
	float3	worldpos: TEXCOORD3;
	fogtype	fog		: FOG;
	SHADOW_VO
};

//------------------------------------

S_Ray VS_ScreenRay(float3 position : POSITION) {
	S_Ray	v;
	v.pos	= platform_fix(position);

	float4x4	wvp			= transpose(worldViewProj);
	wvp[2]	= float4(0,0,1,-1);
	v.p1	= mul(float4(v.pos.xy, 0, 1), cofactors(wvp));

	wvp[2]	= float4(0,0,1,+1);
	v.p0	= mul(float4(v.pos.xy, 0, 1), cofactors(wvp));
	return v;
}

float4 VS_Trivial3D(float3 position:POSITION) : POSITION_OUT {
	return mul(float4(position, 1.0), worldViewProj);
}

#ifdef PLAT_WII
float4 VS_Background(float3 position:POSITION, inout float2 uv:TEXCOORD0) : POSITION_OUT {
	return mul(float4(position, 1.0), worldViewProj);
}
#else
S_Tex VS_Background(float2 position:POSITION_IN) {
	S_Tex	v;
	float4	pos	= float4(position, far_depth, 1);
	v.uv		= position;
	v.pos		= pos;//float4(position, far_depth, 1);
	return v;
}
#endif

S_Col VS_Screen(float3 position:POSITION) {
	S_Col	v;
	v.colour	= tint;//diffuse_colour;
	v.pos		= platform_fix(position);
	return v;
}

S_Tex VS_ScreenTex(float3 position:POSITION, in float2 uv:TEXCOORD0) {
	S_Tex	v;
	v.pos	= platform_fix(position);
	v.uv	= uv;
	return v;
}

S_Col VS_NoVP(float3 position:POSITION) {
	S_Col	v;
	v.colour	= tint;//diffuse_colour;
	v.pos		= float4(mul(float4(position, 1), (float4x3)world), 1);
	return v;
}

S_Col VS_Transform(float3 position:POSITION) {
	S_Col	v;
	v.colour	= tint;//diffuse_colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_Col VS_TransformI(int3 position:POSITION) {
	S_Col	v;
	v.colour	= tint;//diffuse_colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_Col VS_Transform4(float4 position:POSITION) {
	S_Col	v;
	v.colour	= tint;//diffuse_colour;
	v.pos		= mul(position, worldViewProj);
	return v;
}

#ifdef PLAT_GLSL
#define indices_t float4
#else
#define indices_t int4
#endif

S_Col VS_Transform_idx(float3 position:POSITION, indices_t idx:BLENDINDICES) {
	S_Col	v;
	float4x4	iworld;
	int		i	= idx.x;
	iworld[0]	= matrices[i + 0];
	iworld[1]	= matrices[i + 1];
	iworld[2]	= matrices[i + 2];
	iworld[3]	= float4(0,0,0,1);
	iworld		= mul(transpose(iworld), worldViewProj);
	v.colour	= tint;//diffuse_colour;
	v.pos		= mul(float4(position, 1), iworld);
	return v;
}

S_Col VS_TransformCol(float3 position:POSITION, float4 colour:COLOR) {
	S_Col	v;
	v.colour	= colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_ColTex VS_TransformTex(float3 position:POSITION, float2 uv:TEXCOORD0) {
	S_ColTex	v;
	v.uv		= uv;
	v.colour	= tint;//diffuse_colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_ColTex VS_TransformColTex(float3 position:POSITION, float2 uv:TEXCOORD0, float4 colour:COLOR) {
	S_ColTex	v;
	v.uv		= uv;
	v.colour	= colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_ColTex2 VS_TransformColTex2(float3 position:POSITION, float2 uv0:TEXCOORD0, float2 uv1:TEXCOORD1, float4 colour:COLOR) {
	S_ColTex2	v;
	v.uv01		= float4(uv0, uv1);
	v.colour	= colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_Lighting VS_TransformLighting(float3 position:POSITION, float3 normal:NORMAL) {
	S_Lighting	v;

	float3x3 it_world = inverse_transpose((float3x3)world);

	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= mul(normal,					it_world) * flip_normals;
	v.position		= mul(float4(pos, 1.0),			ViewProj());
	v.normal		= norm;
	v.worldpos		= pos;
	v.ambient		= float4(0,0,0,1);//light_ambient;//DiffuseLight(pos, norm);
	v.fog			= VSFog(pos);

	SHADOW_VS(v,pos)
	return v;
}
S_Lighting4 VS_TransformLighting4(float4 position:POSITION, float3 normal:NORMAL) {
	S_Lighting4	v;

	float4x4 it_world = inverse_transpose((float4x4)world);

	float4	pos		= mul(position,			world);
	float3	pos3	= pos.xyz / pos.w;
	float3	norm	= mul(float4(normal, 0), it_world).xyz * flip_normals;
	v.position		= mul(position, worldViewProj);
//	v.position		= mul(pos,				ViewProj());
	v.normal		= norm;
	v.worldpos		= position;
	v.ambient		= light_ambient;//DiffuseLight(pos3, norm);
	v.fog			= VSFog(pos3);
	SHADOW_VS(v,pos3)
	return v;
}

S_LightingTex VS_TransformLightingTex(float3 position:POSITION, float3 normal:NORMAL, float2 uv:TEXCOORD0) {
	S_LightingTex	v;

	float3x3 it_world = inverse_transpose((float3x3)world);

	float3	pos		= mul(float4(position, 1.0),	(float4x3)world).xyz;
	float3	norm	= mul(normal,					it_world) * flip_normals;
	v.position		= mul(float4(pos, 1.0), 		ViewProj());
	v.uv			= uv;
	v.normal		= norm;
	v.worldpos		= pos;
	v.ambient		= light_ambient;//DiffuseLight(pos, norm);
	v.fog			= VSFog(pos);
	SHADOW_VS(v,pos)
	return v;
}

//-----------------------------------------------------------------------------
//	pixel shaders
//-----------------------------------------------------------------------------

float4 PS_Background(S_Tex v) : OUTPUT0 {
#ifdef PLAT_WII
	return tex2D(sky_samp, uv);
#else
	return float4(texCUBE(sky_samp, mul(float4(v.uv, near_depth, 1), worldViewProj).xzy).rgb, 1);
#endif
}

float4 PS_Black() : OUTPUT0 {
	return 0;
}
float4 PS_White() : OUTPUT0 {
	return 1;
}
float4 PS_Col(S_Col v): OUTPUT0 {
	return v.colour;
}
float4 PS_Tex(S_ColTex v) : OUTPUT0 {
	return float4(tex2D(diffuse_samp, v.uv).rgb, 1);
}
float4 PS_TexCol(S_ColTex v) : OUTPUT0 {
	return tex2D(diffuse_samp, v.uv) * v.colour;
}

float4 PS_Blend(S_Col v) : OUTPUT0 {
	return Blend(v.colour);
}

float4 PS_Add(S_Col v) : OUTPUT0 {
	return Add(v.colour);
}

float4 PS_TexBlend(S_ColTex v) : OUTPUT0 {
	return Blend(tex2D(diffuse_samp, v.uv) * v.colour);
}

#ifdef PLAT_WII
float4 PS_TexBlendMask(float2 uv0:TEXCOORD0, float2 uv1:TEXCOORD1, float4 colour:COLOR) : OUTPUT0 {
	float4	t0 = tex2D(linear_samp, uv0);
	float4	t1 = tex2D(linear_samp2, uv1);
	return Blend(float4(t0.rgb, t0.a * t1.r) * colour);
}
#else
float4 PS_TexBlendMask(S_ColTex2 v) : OUTPUT0 {
	float4	t0 = tex2D(linear_samp, v.uv01.xy);
	float4	t1 = tex2D(linear_samp2, v.uv01.zw);
	return Blend(float4(t0.rgb, t0.a * t1.r) * v.colour);
}
#endif

float4 PS_TexAdd(S_ColTex v) : OUTPUT0 {
	return Add(tex2D(diffuse_samp, v.uv) * v.colour);
}

float4 PS_Blend2(float2 uv:TEXCOORD0) : COLOR {
	float4 c1 = tex2D(diffuse_samp, uv);
	float4 c2 = tex2D(diffuse_samp2, uv);
	return float4(c1.rgb * (1 - c2.a) + c2.rgb, 1);
}

#ifdef PLAT_WII
float4 PS_TexDistMask(in float2 uv0:TEXCOORD0, in float2 uv1:TEXCOORD1, in float4 colour:COLOR) : OUTPUT0 {
	float4	t = tex2D(linear_samp, uv0);
	return Blend(float4(t.rgb, 1) * colour) * Distance0(0.5, t.a) * tex2D(linear_samp2, uv1).r;
}
#else
float4 PS_TexDistMask(S_ColTex2 v) : OUTPUT0 {
	float4	t = tex2D(linear_samp, v.uv01.xy);
	return Distance1(Blend(float4(t.rgb, tex2D(linear_samp2, v.uv01.zw).r) * v.colour), 0.5, t.a, max(abs(ddx(v.uv01.x)), abs(ddx(v.uv01.y))) * 16);
}
#endif

float4 PS_Specular(S_Lighting v) : OUTPUT0 {
	return FogSpecularLight(
		v.worldpos.xyz,
		normalise(v.normal),
		tint,
		v.ambient,
		glossiness,
		1,
		GETSHADOW(v),
		v.fog
	);
}

float4 PS_SpecularClip(S_Lighting4 v) : OUTPUT0 {
	bool	clip = any(abs(v.worldpos.xyz) > v.worldpos.w);
	return FogSpecularLight(
		v.worldpos.xyz,
		normalise(v.normal),
		clip ? clip_tint : tint,
		v.ambient,
		glossiness,
		1,
		GETSHADOW(v),
		v.fog
	);
}

float4 PS_SpecularTex(S_LightingTex v) : OUTPUT0 {
	return FogSpecularLight(
		v.worldpos.xyz,
		normalise(v.normal),
		tex2D(diffuse_samp, v.uv) * tint,
		v.ambient,
		glossiness,
		1,
		GETSHADOW(v),
		v.fog
	);
}

float4 PS_SpecularNorm(S_Lighting v) : OUTPUT0 {
	float3x3	basis	= GetBasisYZ(float3(0,0,1), v.normal.xyz);

//	float3		nm		= twice(tex2D(normal_samp, mul(basis, v.worldpos).xy).xyz - .5);
	float3		nm		= tex2D(normal_samp, mul(basis, v.worldpos).xy).xyz - .5;
	float		na		= length(nm);
	float		toksvig	= na / (na + glossiness * (1 - na));

	float		glossiness2 = toksvig * glossiness;
	float		gloss_scale	= (1 + glossiness2) / (1 + glossiness);

	return FogSpecularLight(
		v.worldpos.xyz,
		normalise(mul(nm, basis)),
		tint,
		v.ambient,
		glossiness2,
		gloss_scale,
		GETSHADOW(v),
		v.fog
	);
}

float4 PS_SpecularNormTex(S_LightingTex v) : OUTPUT0 {
#if defined PLAT_IOS || defined PLAT_ANDROID
	float2x2	dT		= float2x2(ddx(v.uv), ddy(v.uv));
	float2x2	rdT		= cofactors(dT);
	float3		dWdT0	= rdT[0].x * ddx(v.worldpos) + rdT[1].x * ddy(v.worldpos);
#else
	float2x3	dW		= float2x3(ddx(v.worldpos), ddy(v.worldpos));
	float2x2	dT		= float2x2(ddx(v.uv), ddy(v.uv));
	float2x3	dWdT	= mul(cofactors(dT), dW);
	float3		dWdT0	= dWdT[0];
#endif

	return FogSpecularLight(
		v.worldpos.xyz,
		GetNormal(normal_samp, v.uv, v.normal.xyz, dWdT0),
		tex2D(diffuse_samp, v.uv) * tint,
		v.ambient,
		glossiness,
		1,
		GETSHADOW(v),
		v.fog
	);
}
//-----------------------------------------------------------------------------
//	common techniques
//-----------------------------------------------------------------------------

technique tex_blend {
	PASS(p0,VS_TransformTex,PS_TexBlend)
}

technique background {
	pass p0 {
		SET_VS(VS_Background);
		SET_PS(PS_Background);
	}
//	PASS(p0,VS_Background,PS_Background)
}

technique coloured {
	PASS(p0,VS_Transform,PS_Col)
	PASS(p1,VS_TransformI,PS_Col)
}

technique coloured4 {
	PASS(p0,VS_Transform4,PS_Col)
}

technique coloured_novp {
	PASS(p0,VS_NoVP,PS_Col)
}

technique screen_coloured {
	PASS(p0,VS_Screen,PS_Col)
}

technique col_vc {
	PASS(p0,VS_TransformCol,PS_Col)
}

technique blend {
	PASS(p0,VS_Transform,PS_Blend)
}

technique blend4 {
	PASS(p0,VS_Transform4,PS_Blend)
}

technique blend_vc {
	PASS(p0,VS_TransformCol,PS_Blend)
}

technique add {
	PASS(p0,VS_Transform,PS_Add)
}

technique add_vc {
	PASS(p0,VS_TransformCol,PS_Add)
}

technique tex_vc {
	PASS(p0,VS_TransformColTex,PS_TexCol)
}

technique tex {
	PASS(p0,VS_TransformTex,PS_Tex)
}

//technique tex_blend {
//	PASS(p0,VS_TransformTex,PS_TexBlend)
//}

technique tex_blend_vc {
	PASS(p0,VS_TransformColTex,PS_TexBlend)
}

technique tex_blend_mask {
	PASS(p0,VS_TransformColTex2,PS_TexBlendMask)
}

technique tex_dist_mask {
	PASS(p0,VS_TransformColTex2,PS_TexDistMask)
}

technique tex_add_vc {
	PASS(p0,VS_TransformColTex,PS_TexAdd)
}

technique specular {
	PASS(p0,VS_TransformLighting,PS_Specular)
}

technique specular4 {
	PASS(p0,VS_TransformLighting4,PS_SpecularClip)
}

technique tex_specular {
	PASS(p0,VS_TransformLightingTex,PS_SpecularTex)
}

technique norm_specular {
	PASS(p0,VS_TransformLighting,PS_SpecularNorm)
}

technique normtex_specular {
	PASS(p0,VS_TransformLightingTex,PS_SpecularNormTex)
}

technique trivial3D {
#ifdef PLAT_X360
	PASS_0(p0, VS_Trivial3D)
#elif defined(PLAT_PS3)
	PASS(p0, VS_Trivial3D, PS_Black)
#else
	PASS(p0, VS_Trivial3D, PS_White)
#endif
}

technique blend_idx {
	PASS(p0,VS_Transform_idx,PS_Blend)
}

//-----------------------------------------------------------------------------
//	depth buffer
//-----------------------------------------------------------------------------
#define PLAT_PC

#if defined(PLAT_PC)// && !defined(USE_DX11)

#ifdef USE_DX11
float4 PS_DepthMin(S_Tex v) : OUTPUT0 {
	float2	uv	= v.uv;
	float	dx	= ddx(uv.x), dy = ddy(uv.y);
	float	v0 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(0, 0), mip).x;
	float	v1 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(dx, 0), mip).x;
	float	v2 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(0, dy), mip).x;
	float	v3 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(dx, dy), mip).x;
	return min(min(v0, v1), min(v2, v3));
}
#else
float4 PS_DepthMin(S_Tex v) : OUTPUT0 {
	float2	uv	= v.uv;
	float	dx	= ddx(uv.x) * 2, dy = ddy(uv.y) * 2;
	float	v0 = PointSample(_zbuffer, uv + float2(0, 0)).x;
	float	v1 = PointSample(_zbuffer, uv + float2(dx, 0)).x;
	float	v2 = PointSample(_zbuffer, uv + float2(0, dy)).x;
	float	v3 = PointSample(_zbuffer, uv + float2(dx, dy)).x;
	return min(min(v0, v1), min(v2, v3));
}
#endif

float3 RayIntersectMip(sampler2D s, float3 p0, float3 p1) {
	const int mips = 10;

	float3	p = p0;
	float3	v = p1 - p0;
#if 0
	int steps = max(min(max(v.x, v.y) / v.z, 10), 1);
	v = v / steps;

	for (int i = mips; i--;) {
		v *= 0.5;
		for (int j = 0; j < steps && p.z < tex2Dlod(s, float4(p.xy,0,i)).x; j++)
			p += v;
	}
#else
	for (int i = mips; i--;) {
		v *= 0.5;
		if (p.z < tex2Dlod(s, float4(p.xy,0,i)).x)
			p += v;
	}
#endif
	return p;
}

bool RayIntersect(sampler2D s, inout float3 p, float3 p1) {
	const int num_steps_lin = 100;
	const int num_steps_bin = 10;

	float3	v = (p1 - p) / num_steps_lin;

	int i;
	for (i = 0; i < num_steps_lin && p.z < tex2D(s, p.xy).x; i++)
		p += v;

	if (p1.z < 0.99999f && i == num_steps_lin)
		return false;

	p -= v;
	for (i = 0; i < num_steps_bin; i++) {
		v *= 0.5;
		if (p.z < tex2D(s, p.xy).x)
			p += v;
		else
			p -= v;
	}
	return true;
}

bool unitcube_clip(inout float3 a, inout float3 b) {
	if (any(min(a, b) > 1) || any(max(a, b) < -1))			//entirely outside box
		return false;

	float3	d	= b - a;
	float3	va	= (-1 - a) / d;
	float3	vb	= (+1 - a) / d;
	float3	vt0	= lerp(vb, va, step(0, d));
	float3	vt1	= lerp(va, vb, step(0, d));
	float1	t0	= max(max_component(vt0), 0);
	float1	t1	= min(min_component(vt1), 1);
	if (t0 > t1)
		return false;
	b	= a + d * t1;
	a	= a + d * t0;
	return true;
}

bool unitcube_clip(inout float4 a, inout float4 b) {
	if (a.w < 0)
		a = -a;
	if (b.w < 0)
		b = -b;

	if (any(a.xyz > a.w & b.xyz > b.w) || any(a.xyz < -a.w & b.xyz < -b.w))
		return false;		//entirely outside box

	float4	d	= b - a;
	float3	va	= -(a.xyz + a.w) / (d.xyz + d.w);
	float3	vb	= -(a.xyz - a.w) / (d.xyz - d.w);

	float3	vt0	= lerp(lerp(0, vb, step(a.xyz, a.w)), va, step(a.xyz, -a.w));
	float3	vt1	= lerp(lerp(1, vb, step(b.xyz, b.w)), va, step(b.xyz, -b.w));

	float1	t0	= max(max_component(vt0), 0);
	float1	t1	= min(min_component(vt1), 1);
	if (t0 > t1)
		return false;

	b	= a + d * t1;
	a	= a + d * t0;
	return true;
}

bool plane_clip(float4 plane, inout float3 p0, inout float3 p1) {
	float	d0 = dot(plane, float4(p0, 1));
	float	d1 = dot(plane, float4(p1, 1));
	if (d0 < 0 && d1 < 0)
		return false;
	if (d0 < 0)
		p0 -= (p1 - p0) * d0 / (d1 - d0);
	else if (d1 < 0)
		p1 -= (p1 - p0) * d1 / (d1 - d0);
	return true;
}

bool plane_clip(float4 plane, inout float4 p0, inout float4 p1) {
	float	d0 = dot(plane, p0);
	float	d1 = dot(plane, p1);
	if (d0 < 0 && d1 < 0)
		return false;
	if (d0 < 0)
		p0 -= (p1 - p0) * d0 / (d1 - d0);
	else if (d1 < 0)
		p1 -= (p1 - p0) * d1 / (d1 - d0);
	return true;
}

float4 PS_DepthRay(S_Ray v) : OUTPUT0 {
//	clip(-world_far.w);
//	clip(-world_near.w);

//	if (!unitcube_clip(v.p0, v.p1))
//		clip(-1);

	float4	plane	= mul(float4(0,0,1,-1), cofactors(worldViewProj));
	if (!plane_clip(plane, v.p0, v.p1))
		clip(-1);

	float3	p0	= project(v.p0);
	float3	p1	= project(v.p1);

	if (!unitcube_clip(p0, p1))
		clip(-1);

	p0	= p0 * 0.5 + 0.5;
	p1	= p1 * 0.5 + 0.5;

	if (p1.z < p0.z) {
		float3	t = p0;
		p0 = p1;
		p1 = t;
	}
	if (!RayIntersect(linear_samp, p0, p1))
		clip(-1);
	return float4(tex2D(diffuse_samp, p0.xy).rgb, 1);
}

technique depth_min {
	PASS(p0,VS_ScreenTex,PS_DepthMin)
}
technique depth_ray {
	PASS(p0,VS_ScreenRay,PS_DepthRay)
}

#endif

//-----------------------------------------------------------------------------
//	Font
//-----------------------------------------------------------------------------

S_ColTex VS_Font(float3 position:POSITION, float2 uv:TEXCOORD0, float4 colour:COLOR) {
	S_ColTex	v;
	v.uv		= UnnormaliseUV(font_size.xy, uv);
	v.colour	= colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

float4 PS_FontDist(S_ColTex v): OUTPUT0 {
	return FontDist(v.uv, v.colour);
}
float4 PS_FontDistOutline(S_ColTex v): OUTPUT0 {
	return FontDistOutline(v.uv, v.colour);
}

technique font_dist {
	PASS(p0,VS_Font,PS_FontDist)
}
technique font_dist_outline {
	PASS(p0,VS_Font,PS_FontDistOutline)
}

//-----------------------------------------------------------------------------
//	Grid
//-----------------------------------------------------------------------------

#if 1//ndef PLAT_METAL

S_Lighting4 VS_TransformLightingGrid(float3 screen_pos:POSITION, float3 normal:NORMAL, out float3 tangent:TANGENT0) {
	S_Lighting4	v;

	float3x3 it_world = inverse_transpose((float3x3)world);
	normal			*= flip_normals;

	float4	pos		= platform_fix(screen_pos);
	float4	wpos	= mul(pos,	iviewProj);

	v.position		= pos;
	v.normal		= mul(normal, it_world);
	v.worldpos		= wpos;
	v.ambient		= light_ambient;//DiffuseLight(pos, norm);
	v.fog			= VSFog(project(wpos));
	SHADOW_VS(v,pos3)

	tangent			= normalise(mul(perp(normal),	it_world));
	return v;
}

float2 grid_alpha(float2 x, float2 w) {
	float2	f	= frac(x);
	float2	dw	= w / 2;
	return smoothstep(dw, w, f) * smoothstep((1 - dw), (1 - w), f);
}

float4 PS_SpecularGrid(S_Lighting4 v, float3 tangent:TANGENT0) : OUTPUT0 {
	float3	pos	= project(v.worldpos);
	float3	n	= normalise(v.normal);
	float3	dx	= tangent;
	float3	dy	= cross(n, dx);
	float2	x	= float2(dot(pos, dx), dot(pos, dy));

	float2	f	= fwidth(x);
//	float2	f	= abs(ddx_fine(x)) + abs(ddy_fine(x));

	float2	w0	= log2(f) / 2;
	float2	w	= floor(w0) + 3;
	float2	s	= exp2(-w * 2);

	float2	a2	= lerp(
		grid_alpha(x * s, f * s * 2),
		grid_alpha(x * s / 4, f * s / 2),
		frac(w0)
	);
	float	a	= 1 - a2.x * a2.y;
	return Blend(tint) * a;
}

technique specular_grid {
	PASS(p0,VS_TransformLightingGrid,PS_SpecularGrid)
}

float traverseUniformGrid(float3 o, float3 d) {
	float3 inc 	= 1 / max(abs(d), 1e-8);
	float3 x 	= frac(-o * sign(d)) * inc;
	o += abs(d) * 1e-2;

//	float accum = -1;

	// traverse the uniform grid
//	float	t;
//	while ((t = min(x.x, min(x.y, x.z))) < 8) {
	while (true) {
		float	t	= min(x.x, min(x.y, x.z));
		if (t >= 8)
			break;
		float3 	p 	= o + d * t;

		float3	f 	= frac(p);
		float	a	= min(min(f.x, f.y), f.z);
		float	b	= max(max(f.x, f.y), f.z);
		float	c	= f.x + f.y + f.z;

//		if (c - b < 2e-2)
//			accum += 1.0;

		if (c - b < 2e-2)
			return t;
		//float	y = clamp(1.0 - (c - b - 2e-2) * 100.0, 0.0, 1.0) / (t * 1.0);//* (4.0 - z);
		//accum = max(accum, y);

		// step to the next ray-cell intersection
		x += step(x.xyz, x.yzx) * step(x.xyz, x.zxy) * inc;
	}

//	return accum;
	return -1;
}

float3 sort3(float3 v) {
	float	a = min(v.x, v.y);
	float	b = max(v.x, v.y);
	return float3(min(a, v.z), max(a, min(b, v.z)), max(b, v.z));
}

static const float3	attractor	= float3(0,0,4);
static const float		k			= 1.0;

float traverseUniformGrid2(float3 o, float3 d) {
	float	t	= 0.0;
	for(int i = 0; i < 20; ++i) {
		float3 	p	= o + d * t;

		float3	z	= attractor - p;
		float	z2	= dot(z, z);
		p += k * z / z2;

		float3 	x	= frac(-p * sign(d) + 1e-2);
		float3	y	= sort3(x);

		if (y.y < 2e-2)
			return t;

		x = sort3((x - 1e-2) / abs(d));
//		t += x.y;
		t += x.y / (1.0 + abs(k) / max(z2 - 1.5, 0.001));

		if (t > 8.0)
			break;
	}

	return -1;
}

#ifndef PLAT_GLSL

float4 PS_SpaceGrid(S_Ray v, out float depth : OUTPUT_DEPTH) : OUTPUT0 {

	float3	p0	= project(v.p0);
	float3	p1	= project(v.p1);
	float3	d	= normalise(p1 - p0);
//	return float4(frac(d), 1);

	float	t = traverseUniformGrid2(p0, d);
	if (t < 0) {
		depth	= far_depth;
		return float4(0,0,0,1);
	}

	float4	p	= mul(float4(p0 + d * t, 1), worldViewProj);
	depth		= p.z / p.w;
	return float4(1/t,1/t,1/t,1);
//	return float4(t,t,t,1);
}

S_Ray VS_SpaceGrid(float2 position : POSITION) {
	S_Ray	v;
	v.pos	= platform_fix(position);

	float4x4	wvp			= cofactors(worldViewProj);
	v.p0	= mul(wvp, platform_fix(float3(position.xy, -1)));
	v.p1	= mul(wvp, platform_fix(float3(position.xy, +1)));
	return v;
}

technique space_grid {
	PASS(p0,VS_SpaceGrid,PS_SpaceGrid)
}

#endif
#endif

#if defined(PLAT_PC)

//-----------------------------------------------------------------------------
//	Oculus Rift
//-----------------------------------------------------------------------------

sampler_def2D(ovr_samp, FILTER_MIN_MAG_MIP_LINEAR;);
float4		EyeToSourceUVTrans;
float4x4	EyeRotationStart, EyeRotationEnd;

float2 TimewarpTexCoord(float2 TexCoord, float4x4 rotMat) {
	float3 transformed	= mul(rotMat, float4(TexCoord.xy, 1, 1)).xyz;
	float2 flattened	= transformed.xy / transformed.z;
	return EyeToSourceUVTrans.xy * flattened + EyeToSourceUVTrans.zw;
}

float4 VS_oculus(
	in float2	Position			: POSITION,
	in float	timewarpLerpFactor	: POSITION1,
	in float	Vignette			: POSITION2,
	in float2	TexCoordR			: TEXCOORD0,
	in float2	TexCoordG			: TEXCOORD1,
	in float2	TexCoordB			: TEXCOORD2,
	out float2	oTexCoordR			: TEXCOORD0,
	out float2	oTexCoordG			: TEXCOORD1,
	out float2	oTexCoordB			: TEXCOORD2,
	out float	oVignette			: TEXCOORD3
) : POSITION_OUT {
	float4x4 lerpedEyeRot = lerp(EyeRotationStart, EyeRotationEnd, timewarpLerpFactor);
	oTexCoordR	= TimewarpTexCoord(TexCoordR, lerpedEyeRot);
	oTexCoordG	= TimewarpTexCoord(TexCoordG, lerpedEyeRot);
	oTexCoordB	= TimewarpTexCoord(TexCoordB, lerpedEyeRot);
	oVignette	= Vignette;
	return float4(Position, 0.5, 1.0);
}

//Distortion pixel shader
float4 PS_oculus(
	in float4 pos : POSITION_OUT,
	in float2 oTexCoordR : TEXCOORD0,
	in float2 oTexCoordG : TEXCOORD1,
	in float2 oTexCoordB : TEXCOORD2,
	in float  oVignette  : TEXCOORD3
) : OUTPUT0 {
	return float4(
		tex2D(ovr_samp, oTexCoordR).r,
		tex2D(ovr_samp, oTexCoordG).g,
		tex2D(ovr_samp, oTexCoordB).b,
		1
	) * oVignette;
}

technique oculus {
	PASS(p0,VS_oculus,PS_oculus)
}

#endif

#ifdef PLAT_PS4
//-----------------------------------------------------------------------------
//	Morpheus
//-----------------------------------------------------------------------------
SamplerState rgb2yuv_sampler;

float3 rgb2yuv(float3 color) {
	return	float3(
		0.18187266f	* color.r +	0.61183125f	* color.g +	0.06176484f	* color.b + 0.0625f,
		-0.1002506f * color.r - 0.3372494f	* color.g + 0.4375f		* color.b + 0.5f,
		0.4375f		* color.r - 0.3973838f	* color.g - 0.0401162f	* color.b + 0.5f
	);
}

struct rgb2yuv_data {
	Texture2D<float4>		srcTex;
	RW_Texture2D<float>		dstTex;
	float4					scale;
	uint					height;
};
struct rgb2yuv_SRT {
	rgb2yuv_data*	data;
};

COMPUTE_SHADER(32,2,1)
void rgb2yuv_CS(uint2 groupID : S_GROUP_ID, uint2 threadID : S_GROUP_THREAD_ID, rgb2yuv_SRT srt : S_SRT_DATA) : S_TARGET_OUTPUT {
	uint2				pos			= groupID * uint2(32, 2) + threadID;

	Texture2D			srcTex		= srt.data->srcTex;
	RW_Texture2D<float> dstTex		= srt.data->dstTex;
	const float4		scale		= srt.data->scale;
	const uint			height		= srt.data->height;

	// 8:9 -> 16:9  trim center and normalize
	uint2	dst_y	= pos * 2;
	uint2	dst_uv	= uint2(pos.x * 2 + 0, pos.y + height);

	// 6 pixels are used for 1 UV value calcuration.
	// for cache hit, center -> L,R is a bit better.
	float3	srcC0 =	srcTex.SampleLOD(rgb2yuv_sampler, (pos * 2 + float2( 0, 0)) * scale.xy + scale.zw, 0, 0).rgb;
	float3	srcR0 =	srcTex.SampleLOD(rgb2yuv_sampler, (pos * 2 + float2(+1, 0)) * scale.xy + scale.zw, 0, 0).rgb;
	float3	srcL0 =	srcTex.SampleLOD(rgb2yuv_sampler, (pos * 2 + float2(-1, 0)) * scale.xy + scale.zw, 0, 0).rgb;
	float3	srcC1 =	srcTex.SampleLOD(rgb2yuv_sampler, (pos * 2 + float2( 0, 1)) * scale.xy + scale.zw, 0, 0).rgb;
	float3	srcR1 =	srcTex.SampleLOD(rgb2yuv_sampler, (pos * 2 + float2(+1, 1)) * scale.xy + scale.zw, 0, 0).rgb;
	float3	srcL1 =	srcTex.SampleLOD(rgb2yuv_sampler, (pos * 2 + float2(-1, 1)) * scale.xy + scale.zw, 0, 0).rgb;

	// 4 pixels for Y
	dstTex[dst_y + uint2(0, 0)]		= rgb2yuv(srcC0).x;
	dstTex[dst_y + uint2(1, 0)]		= rgb2yuv(srcR0).x;
	dstTex[dst_y + uint2(0, 1)]		= rgb2yuv(srcC1).x;
	dstTex[dst_y + uint2(1, 1)]		= rgb2yuv(srcR1).x;

	// 6 pixels for 1 UV. Filtering as Up:Down = 1:1, Left:Center:Right = 1:2:1.
	float2	uv		= rgb2yuv((srcL0 + srcL1 + 2 * (srcC1 + srcC0) + srcR0 + srcR1) / 8).yz;
	dstTex[dst_uv + uint2(0, 0)]	= uv.x;
	dstTex[dst_uv + uint2(1, 0)]	= uv.y;
}

technique rgb2yuv {
	pass p0 {
		SET_CS(rgb2yuv_CS);
	}
};

#endif

#if 1//defined(USE_DX11) || defined(PLAT_PS4)

float3	point_size = float3(1 / 50.f, 1 / 50.f, 1);

//-----------------------------------------------------------------------------
//	ThickPoint
//-----------------------------------------------------------------------------

float4 Clip(float4 p0, float4 p1) {
	return p0.z < 0
		? p0 + (p1 - p0) * -p0.z / (p1.z - p0.z)
		: p0;
}

struct ThickPoint {
	float4 pos;
};

ThickPoint VS_thickpoint(float3 pos : POSITION_IN) : POSITION_IN {
	ThickPoint	p;
	p.pos	= mul(float4(pos, 1.0), worldViewProj);
	return p;
}

ThickPoint VS_thickpointI(int3 pos : POSITION_IN) : POSITION_IN {
	ThickPoint	p;
	p.pos	= mul(float4(pos, 1.0), worldViewProj);
	return p;
}

ThickPoint VS_thickpoint4(float4 pos : POSITION_IN) : POSITION_IN {
	ThickPoint	p;
	p.pos	= mul(pos, worldViewProj);
	return p;
}

GEOM_SHADER(4)
void GS_thickpoint(point ThickPoint v[1] : POSITION_IN, inout TriangleStream<S_ColTex> tris) {
	S_ColTex	p;
	p.colour = tint;//diffuse_colour;

	float4	pw	= v[0].pos;
	if (pw.z < -pw.w)
		return;

	float2 pos	= pw.xy / pw.w;
	float4 size	= float4(-point_size.xy, point_size.xy);

	p.pos	= float4(pos + size.xy, 0, 1);
	p.uv	= float2(-1, -1);
	tris.Append(p);

	p.pos	= float4(pos + size.zy, 0, 1);
	p.uv	= float2(+1, -1);
	tris.Append(p);

	p.pos	= float4(pos + size.xw, 0, 1);
	p.uv	= float2(-1, +1);
	tris.Append(p);

	p.pos	= float4(pos + size.zw, 0, 1);
	p.uv	= float2(+1, +1);
	tris.Append(p);
}

float4 PS_thickpoint(S_ColTex p) : OUTPUT0 {
	float	r2	= dot(p.uv, p.uv);
	float	d	= max(abs(ddx(r2)), abs(ddy(r2)));
	return p.colour * smoothstep(1, 1 - d, r2);
}

technique thickpoint {
	pass p0 {
		SET_VS(VS_thickpoint);
		SET_GS(GS_thickpoint);
		SET_PS(PS_thickpoint);
	}
	pass p1 {
		SET_VS(VS_thickpointI);
		SET_GS(GS_thickpoint);
		SET_PS(PS_thickpoint);
	}
}
technique thickpoint4 {
	pass p0 {
		SET_VS(VS_thickpoint4);
		SET_GS(GS_thickpoint);
		SET_PS(PS_thickpoint);
	}
}
//-----------------------------------------------------------------------------
//	Thick line
//-----------------------------------------------------------------------------

void Thickline(ThickPoint v[2] : POSITION_IN, inout TriangleStream<S_ColTex> tris) {
	S_ColTex	p;
	p.colour = tint;//diffuse_colour;

	float4 pw0		= Clip(v[0].pos, v[1].pos);
	float4 pw1		= Clip(v[1].pos, pw0);

	float2 pos0		= pw0.xy / pw0.w;
	float2 pos1		= pw1.xy / pw1.w;

	float2 across	= point_size.xy * perp(normalise(pos1 - pos0));

	p.pos	= float4(pos0 - across, 0, 1);
	p.uv	= float2(-1, -1);
	tris.Append(p);

	p.pos	= float4(pos1 - across, 0, 1);
	p.uv	= float2(+1, -1);
	tris.Append(p);

	p.pos	= float4(pos0 + across, 0, 1);
	p.uv	= float2(-1, +1);
	tris.Append(p);

	p.pos	= float4(pos1 + across, 0, 1);
	p.uv	= float2(+1, +1);
	tris.Append(p);
}

GEOM_SHADER(4)
void GS_thickline(line ThickPoint v[2] : POSITION_IN, inout TriangleStream<S_ColTex> tris) {
	Thickline(v, tris);
}

float4 PS_thickline(S_ColTex p) : OUTPUT0 {
	float	d = max(abs(ddx(p.uv.y)), abs(ddy(p.uv.y)));
	return p.colour * smoothstep(1, 1 - d, abs(p.uv.y));
}

technique thickline {
	pass p0 {
		SET_VS(VS_thickpoint);
		SET_GS(GS_thickline);
		SET_PS(PS_thickline);
	}
}

technique thickline4 {
	pass p0 {
		SET_VS(VS_thickpoint4);
		SET_GS(GS_thickline);
		SET_PS(PS_thickline);
	}
}
//-----------------------------------------------------------------------------
//	Thick line with round ends
//-----------------------------------------------------------------------------

void ThicklineRound(ThickPoint v[2] : POSITION_IN, inout TriangleStream<S_ColTex> tris) {
	S_ColTex	p;
	p.colour		= tint;//diffuse_colour;

	float4 pw0		= Clip(v[0].pos, v[1].pos);
	float4 pw1		= Clip(v[1].pos, pw0);

	float2 pos0		= pw0.xy / pw0.w;
	float2 pos1		= pw1.xy / pw1.w;

	float2 d		= normalise(pos1 - pos0);
	float2 along	= point_size.xy * d;
	float2 across	= point_size.xy * perp(d);

	p.pos	= float4(pos0 - along - across, 0, 1);
	p.uv	= float2(-1, -1);
	tris.Append(p);

	p.pos	= float4(pos0 - along + across, 0, 1);
	p.uv	= float2(-1, +1);
	tris.Append(p);

	p.pos	= float4(pos0 - across, 0, 1);
	p.uv	= float2(0, -1);
	tris.Append(p);

	p.pos	= float4(pos0 + across, 0, 1);
	p.uv	= float2(0, +1);
	tris.Append(p);

	p.pos	= float4(pos1 - across, 0, 1);
	p.uv	= float2(0, -1);
	tris.Append(p);

	p.pos	= float4(pos1 + across, 0, 1);
	p.uv	= float2(0, +1);
	tris.Append(p);

	p.pos	= float4(pos1 + along - across, 0, 1);
	p.uv	= float2(+1, -1);
	tris.Append(p);

	p.pos	= float4(pos1 + along + across, 0, 1);
	p.uv	= float2(+1, +1);
	tris.Append(p);
}

GEOM_SHADER(8)
void GS_thicklineR(line ThickPoint v[2] : POSITION_IN, inout TriangleStream<S_ColTex> tris) {
	ThicklineRound(v, tris);
}

technique thicklineR {
	pass p0 {
		SET_VS(VS_thickpoint);
		SET_GS(GS_thicklineR);
		SET_PS(PS_thickpoint);
	}
}

//-----------------------------------------------------------------------------
//	Thick line with Adjacency
//-----------------------------------------------------------------------------

float2 intersect_lines(float2 pos0, float2 dir0, float2 pos1, float2 dir1) {
	float	d = cross2(dir0, dir1);
	return abs(d) < 1e-2
		? pos1
		: (dir0 * cross2(pos1, dir1) - dir1 * cross2(pos0, dir0)) / d;
}

float2 intersect_offsets(float2 pos, float2 dir0, float2 dir1, float2 point_size) {
	return intersect_lines(pos + perp(dir0) * point_size, dir0, pos + perp(dir1) * point_size, dir1);
}

void ThicklineA(ThickPoint v[4], inout TriangleStream<S_ColTex> tris) {
	S_ColTex	p;
	p.colour = tint;//diffuse_colour;

	float4	pwa		= Clip(v[0].pos, v[1].pos);
	float4	pw0		= Clip(v[1].pos, v[2].pos);
	float4	pw1		= Clip(v[2].pos, pw0);
	float4	pw2		= Clip(v[3].pos, pw1);

	float2	posa	= pwa.xy / pwa.w;
	float2	pos0	= pw0.xy / pw0.w;
	float2	pos1	= pw1.xy / pw1.w;
	float2	pos2	= pw2.xy / pw2.w;

	float2	d1		= normalise(pos1 - pos0);
	float2	d0		= all(pos0 == posa) ? d1 : normalise(pos0 - posa);
	float2	d2		= all(pos1 == pos2) ? d1 : normalise(pos2 - pos1);

	bool	bevel0	= dot(d0, d1) < -.5;
	bool	bevel1	= dot(d1, d2) < -.5;

	if (bevel0) {
		// extra triangle for bevel
		if (cross2(d0, d1) < 0)
			p.pos	= float4(intersect_offsets(pos0, +d0, -d1, point_size.xy), 0, 1);//A
		else
			p.pos	= float4(intersect_offsets(pos0, +d1, -d0, point_size.xy), 0, 1);//B
		p.uv	= float2(-1, -1);
		tris.Append(p);
	}

	if (bevel0 && cross2(d0, d1) < 0)
		p.pos	= float4(intersect_offsets(pos0, -d0, +d1, point_size.xy), 0, 1);
	else
		p.pos	= float4(intersect_offsets(pos0, +d1, +d0, point_size.xy), 0, 1);//C
	p.uv	= float2(-1, -1);
	tris.Append(p);

	if (bevel0 && cross2(d0, d1) > 0)
		p.pos	= float4(intersect_offsets(pos0, +d0, -d1, point_size.xy), 0, 1);
	else
		p.pos	= float4(intersect_offsets(pos0, -d0, -d1, point_size.xy), 0, 1);
	p.uv	= float2(-1, +1);
	tris.Append(p);

	if (bevel1 && cross2(d1, d2) < 0)
		p.pos	= float4(intersect_offsets(pos1, -d2, +d1, point_size.xy), 0, 1);//A
	else
		p.pos	= float4(intersect_offsets(pos1, +d2, +d1, point_size.xy), 0, 1);//C
	p.uv	= float2(+1, -1);
	tris.Append(p);

	if (bevel1 && cross2(d1, d2) > 0)
		p.pos	= float4(intersect_offsets(pos1, +d2, -d1, point_size.xy), 0, 1);//B
	else
		p.pos	= float4(intersect_offsets(pos1, -d2, -d1, point_size.xy), 0, 1);
	p.uv	= float2(+1, +1);

	tris.Append(p);
}

GEOM_SHADER(5)
void GS_thicklineA(lineadj ThickPoint v[4] : POSITION_IN, inout TriangleStream<S_ColTex> tris) {
	ThicklineA(v, tris);
}

technique thicklineA {
	pass p0 {
		SET_VS(VS_thickpoint);
		SET_GS(GS_thicklineA);
		SET_PS(PS_thickline);
	}
}

//-----------------------------------------------------------------------------
//	Circle tesselator
//-----------------------------------------------------------------------------

struct Circle {
	float4	v : CIRCLE;
};

struct CircleConsts {
	float	tess[2] : TESS_FACTOR;
};

Circle VS_circle(float4 v : POSITION_IN) {
	Circle c = {v};
	return c;
}

// patch Constant Function
CircleConsts HS_circleC(InputPatch<Circle, 1> c, uint id : PRIMITIVE_ID) {
	CircleConsts	cc;

	float	w	= mul(float4(c[0].v.xyz, 1.0), worldViewProj).w;

	cc.tess[0]	= 1;
	cc.tess[1]	= c[0].v.w * length(worldViewProj[0].xyz) * point_size.z / w;
	return cc;
}

// hull shader
HULL_SHADER(HS_circleC, isoline, fractional_even, line, 1)
Circle HS_circle(InputPatch<Circle, 1> c, uint id : PRIMITIVE_ID, uint i : OUTPUT_CONTROL_POINT_ID) {
	return c[0];
}

// domain shader
DOMAIN_SHADER(isoline)
S_Col DS_circle(CircleConsts cc, float t : DOMAIN_LOCATION, const OutputPatch<Circle, 1> c) {
	S_Col	p;
	float2	sc;
	sincos(t * 2 * pi, sc.y, sc.x);
	float3	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.pos		= mul(float4(pos, 1.0), worldViewProj);
	p.colour	= tint;//diffuse_colour;
	return p;
}

technique circletess {
	pass p0 {
		SET_VS(VS_circle);
		SET_HS(HS_circle);
		SET_DS(DS_circle);
		SET_PS(PS_Col);
	}
}

//-----------------------------------------------------------------------------
//	Ellipse tesselator
//-----------------------------------------------------------------------------

struct Ellipse {
	float4	v		: CIRCLE;
	float	ratio	: CIRCLE2;
};

Ellipse VS_ellipse(float4 v : POSITION0, float ratio : POSITION1) {
	float4	c = platform_fix(v.xy);
	float4	a = platform_fix(v.xy + v.zw);
	Ellipse	e;
	e.v.xy = project(c).xy;
	e.v.zw = project(a).xy - e.v.xy;
	return e;
}

// patch Constant Function
CircleConsts HS_ellipseC(InputPatch<Ellipse, 1> c, uint id : PRIMITIVE_ID) {
	CircleConsts	cc;
	cc.tess[0]	= 1;
	cc.tess[1]	= length(c[0].v.zw) * point_size.z;
	return cc;
}

// hull shader
HULL_SHADER(HS_ellipseC, isoline, fractional_even, line, 1)
Ellipse HS_ellipse(InputPatch<Ellipse, 1> c, uint id : PRIMITIVE_ID, uint i : OUTPUT_CONTROL_POINT_ID) {
	return c[0];
}

// domain shader
DOMAIN_SHADER(isoline)
S_Col DS_ellipse(CircleConsts cc, float t : DOMAIN_LOCATION, const OutputPatch<Ellipse, 1> c) {
	S_Col	p;
	float2	sc;
	sincos(t * 2 * pi, sc.y, sc.x);
	p.pos		= float4(c[0].v.xy + sc.x * c[0].v.zw + (sc.y * c[0].ratio) * perp(c[0].v.zw), 0, 1);
	p.colour	= tint;//diffuse_colour;
	return p;
}

technique ellipsetess {
	pass p0 {
		SET_VS(VS_ellipse);
		SET_HS(HS_ellipse);
		SET_DS(DS_ellipse);
		SET_PS(PS_Col);
	}
}

GEOM_SHADER(4)
void GS_ellipse(point Ellipse e[1], inout TriangleStream<S_ColTex> tris) {
	float2	c = e[0].v.xy;
	float2	x = e[0].v.zw;
	float2	y = e[0].ratio * perp(x);

	S_ColTex	p;
	p.colour	= tint;//diffuse_colour;

	p.uv		= float2(-1, -1);
	p.pos		= float4(c - x - y, 0, 1);
	tris.Append(p);
	p.uv		= float2(+1, -1);
	p.pos		= float4(c + x - y, 0, 1);
	tris.Append(p);
	p.uv		= float2(-1, +1);
	p.pos		= float4(c - x + y, 0, 1);
	tris.Append(p);
	p.uv		= float2(+1, +1);
	p.pos		= float4(c + x + y, 0, 1);
	tris.Append(p);
}

technique ellipseext {
	pass p0 {
		SET_VS(VS_ellipse);
		SET_GS(GS_ellipse);
		SET_PS(PS_thickpoint);
	}
}

//-----------------------------------------------------------------------------
//	Thick circle tesselator
//-----------------------------------------------------------------------------

// domain shader
DOMAIN_SHADER(isoline)
ThickPoint DS_thickcircle(CircleConsts cc, float t : DOMAIN_LOCATION, const OutputPatch<Circle, 1> c) : POSITION_IN {
	ThickPoint	p;
	float2	sc;
	sincos(t * 2 * pi, sc.y, sc.x);
	float3	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.pos	= mul(float4(pos, 1.0), worldViewProj);
	return p;
}

technique thickcircletess {
	pass p0 {
		SET_VS(VS_circle);
		SET_HS(HS_circle);
		SET_DS(DS_thickcircle);
		SET_GS(GS_thickline);
		SET_PS(PS_thickline);
	}
}

//-----------------------------------------------------------------------------
//	Thick circle tesselator with Adjacency
//-----------------------------------------------------------------------------

struct ThickPoints {
	ThickPoint	p[3];
};

// domain shader
DOMAIN_SHADER(isoline)
ThickPoints DS_thickcircleA(CircleConsts cc, float t : DOMAIN_LOCATION, const OutputPatch<Circle, 1> c) : POSITION_IN {
	ThickPoints	p;
	float2	sc;
	float3	pos;
	float	dt		= 1 / cc.tess[1];

	sincos((t - dt) * 2 * pi, sc.y, sc.x);
	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.p[0].pos	= mul(float4(pos, 1.0), worldViewProj);

	sincos(t * 2 * pi, sc.y, sc.x);
	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.p[1].pos	= mul(float4(pos, 1.0), worldViewProj);

	sincos((t + dt) * 2 * pi, sc.y, sc.x);
	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.p[2].pos	= mul(float4(pos, 1.0), worldViewProj);

	return p;
}

GEOM_SHADER(5)
void GS_thickcircleA(line ThickPoints v[2] : POSITION_IN, inout TriangleStream<S_ColTex> tris) {
	ThickPoint	v2[4];
	v2[0] = v[0].p[0];
	v2[1] = v[0].p[1];
	v2[2] = v[1].p[1];
	v2[3] = v[1].p[2];
	ThicklineA(v2, tris);
}

GEOM_SHADER(5)
void GS_thickcircleA2(line ThickPoints v[2] : POSITION_IN, inout PointStream<S_ColTex> tris, inout PointStream<S_ColTex> points) {
	S_ColTex	p;
	p.colour = tint;//diffuse_colour;

	p.pos	= v[1].p[0].pos;
	p.uv	= float2(1, 0);
	tris.Append(p);

	p.pos	= v[0].p[0].pos;
	p.uv	= float2(0, 1);
	points.Append(p);
}

technique thickcircletessA {
	pass p0 {
		SET_VS(VS_circle);
		SET_HS(HS_circle);
		SET_DS(DS_thickcircleA);
		SET_GS(GS_thickcircleA);
		//SET_GS_SO(GS_thickcircleA2, NULL, "0:S_POSITION.xyzw", "0:TEXCOORD0.xy", NULL, 0);
		SET_PS(PS_thickline);
	}
}
#endif


#if defined(USE_DX11) || defined(PLAT_PS4)
//compute

RW_Texture2D<float2>	dst2;
Texture2D<float2>		src_depth : t0;
Texture2D<uint2>		src_stencil : t1;

COMPUTE_SHADER(16,16,1)
void copy_depth_CS(uint2 pos : DISPATCH_THREAD_ID) {
	dst2[pos]	= float2(src_depth[pos].r, src_stencil[pos].r);
}

technique copy_depth {
	pass p0 {
		SET_CS(copy_depth_CS);
	}
};

RW_Texture2D<float4>	copy_dst;
Texture2D<float4>		copy_src;
COMPUTE_SHADER(8,8,1)
void copy_tex_CS(uint2 pos : DISPATCH_THREAD_ID) {
	copy_dst[pos]	= copy_src[pos];
}
technique copy_tex {
	pass p0 {
		SET_CS(copy_tex_CS);
	}
};
#endif


//-----------------------------------------------------------------------------
//	TestMesh
//-----------------------------------------------------------------------------

#ifdef HAS_SM6

#if SHADER_MODEL >= 0x600
struct TestPayload {
	float3	pos;
	float3	col;
};

MESH_SHADER(triangle,3,1,1)
void test_MS(uint thread : GROUP_THREAD_ID, uint3 group : GROUP_ID,
	in payload		TestPayload	payload,
	out vertices	S_Col	Vertices[256],
	out indices		uint3	Triangles[128]
) {
	SetMeshOutputCounts(3, 1);
	
	if (thread == 0) {
		uint3	indices = {0,1,2};
		Triangles[0]	= indices;
	}
	
	float3	pos	= float3((thread >> 1) + group.x, (thread & 1) + group.y, 0) + payload.pos;
	
	//Vertices[thread].pos	= float4(pos / 2, 1);//mul(pos, worldViewProj);
	Vertices[thread].pos	= mul(float4(pos, 1), worldViewProj);
	Vertices[thread].colour	= float4(payload.col, 1);
}

AMPLIFICATION_SHADER(1,1,1)
void test_AS(uint thread : GROUP_THREAD_ID, uint3 group : GROUP_ID) {
	TestPayload	payload;
	payload.pos = float3(group.x * 5,0,0);
	payload.col = float3(1,group.x,group.y);
	DispatchMesh(4, 3, 1, payload);
}
#endif

technique test_mesh {
	pass p0 {
		AmplificationShader = {as_6_5, test_AS};
		MeshShader = {ms_6_5, test_MS};
		//SetMeshShader(CompileShader(ms_6_5, test_MS()))
		PixelShader = {ps_6_0, PS_Col};
	}
};

#endif

//-----------------------------------------------------------------------------
//	Meshlet
//-----------------------------------------------------------------------------

struct Meshlet {
	uint	PrimCount, PrimOffset;
	float4	sphere;
	uint	cone;
	float	apex;
};

bool IsConeDegenerate(uint packed) {
	return (packed >> 24) == 0xff;
}

float4 UnpackCone(uint packed) {
	return float4(unpack4u(packed)) / 255 * float4(2,2,2,1) - float4(1,1,1,0);
}

uint3 UnpackPrimitive(uint packed) {
	return uint3(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF);
}

float3 get_proj_scale(float4x4 m) {
	return rsqrt(square(m[0].xyz + m[0].w) + square(m[1].xyz + m[1].w) + square(m[2].xyz + m[2].w));
}

bool sphere_check(float4 sphere, float4x4 to_proj, float3 proj_scale) {
	float4	centre	= mul(float4(sphere.xyz, 1), to_proj);
	float4	a		= abs(centre);
	float3	b		= a.xyz - a.w;
	return all(b * proj_scale < sphere.w);
}

bool cone_check(float3 centre, float4 cone, float apex, float3 eye) {
	return dot(normalize(centre + cone.xyz * apex - eye), cone.xyz) < cone.w;
}

struct MeshletVertex {
	float3	position;
	float3	normal;
	float2	uv;
};

#if 0
struct MeshletVertexOut {
	float4	pos		: POSITION_OUT;
	float4	colour	: COLOR;
	uint	meshlet	: MESHLET;
	uint	index	: INDEX;
};

MeshletVertexOut meshletVS(MeshletVertex vin, uint meshlet, uint index) {
	MeshletVertexOut	v;
	v.colour	= float4(
		((meshlet >> 0) & 3) / 3.f,
		((meshlet >> 2) & 3) / 3.f,
		((meshlet >> 4) & 3) / 3.f,
		1
	);
	//tint;//diffuse_colour;
	//v.pos		= float4(vin.position, 1);
	v.pos		= mul(float4(vin.position, 1), worldViewProj);
	v.meshlet	= meshlet;
	v.index		= index;
	return v;
}

#else

typedef S_Lighting MeshletVertexOut;

MeshletVertexOut meshletVS(MeshletVertex vin, uint meshlet, uint index) {
	return VS_TransformLighting(vin.position, vin.normal);
}

#endif

StructuredBuffer<MeshletVertex> vertices	: register(t0);
Buffer<uint>					indices		: register(t1);
StructuredBuffer<Meshlet>		meshlets	: register(t2);
Buffer<uint>					prims		: register(t3);



#ifdef HAS_SM6

#if SHADER_MODEL >= 0x600

struct MeshletPayload {
	uint meshlets[32];
};

MESH_SHADER(triangle,128,1,1)
void meshlet_MS(
	uint							thread	: SV_GroupThreadID,
	uint							group	: SV_GroupID,
	in payload MeshletPayload		payload,
	out vertices MeshletVertexOut	out_verts[64],
	out indices uint3				out_tris[126]
) {
	uint	m		= payload.meshlets[group];
	Meshlet meshlet = meshlets[m];

	SetMeshOutputCounts(64, meshlet.PrimCount);

	if (thread < 64) {
		uint index = indices.Load(m * 64 + thread);
		out_verts[thread] = meshletVS(vertices[index], m, index);
	}

	if (thread < meshlet.PrimCount)
		out_tris[thread] = UnpackPrimitive(prims[meshlet.PrimOffset + thread]);
}


// The groupshared payload data to export to dispatched mesh shader threadgroups
groupshared MeshletPayload s_Payload;

AMPLIFICATION_SHADER(32,1,1)
void meshlet_AS(uint dtid : SV_DispatchThreadID) {
	bool visible = false;
	uint	num_meshlets, stride;
	meshlets.GetDimensions(num_meshlets, stride);

	if (dtid < num_meshlets) {
		Meshlet	meshlet = meshlets[dtid];
		float3	scale	= get_proj_scale(worldViewProj);
		visible = sphere_check(meshlet.sphere, worldViewProj, scale) && (
			true//IsConeDegenerate(meshlet.cone) || cone_check(meshlet.sphere.xyz, UnpackCone(meshlet.cone), meshlet.apex, eyePos())
		);
	}
	
	// Compact visible meshlets into the export payload array
	if (visible) {
		uint index = WavePrefixCountBits(visible);
		s_Payload.meshlets[index] = dtid;
	}

	// Dispatch the required number of MS threadgroups to render the visible meshlets
	DispatchMesh(WaveActiveCountBits(visible), 1, 1, s_Payload);
}
#endif

technique meshlet {
	pass p0 {
		SET_AS(meshlet_AS);
		SET_MS(meshlet_MS);
		SET_PS(PS_Specular);
	}
};

#else

AppendStructuredBuffer<int>		visible_meshlets;
AppendStructuredBuffer<uint4>	meshlet_indices;

COMPUTE_SHADER(64,1,1)
void meshlet_CS(uint i : SV_DispatchThreadID) {
	uint	num_meshlets, stride;
	meshlets.GetDimensions(num_meshlets, stride);
	if (i < num_meshlets) {
		Meshlet	meshlet = meshlets[i];
		float3	scale	= get_proj_scale(worldViewProj);
		bool	visible = sphere_check(meshlet.sphere, worldViewProj, scale)/* && (
			IsConeDegenerate(meshlet.cone) || cone_check(meshlet.sphere.xyz, UnpackCone(meshlet.cone), meshlet.apex, eyePos())
		)*/;
		
		if (visible)
			visible_meshlets.Append(i);
	}
}

COMPUTE_SHADER(128,1,1)
void meshlet_CS2(StructuredBuffer<int> visible_meshlets, uint i : SV_GroupThreadID, uint g : SV_GroupID) {
	uint	m = visible_meshlets[g];
	Meshlet	meshlet = meshlets[m];
	
	if (i < meshlet.PrimCount) {
		uint3	x = UnpackPrimitive(prims[meshlet.PrimOffset + i]) + m * 64;
		uint4	x2 = uint4(
			indices.Load(x.x),
			indices.Load(x.y),
			indices.Load(x.z),
			m
		);
		meshlet_indices.Append(x2);
	}

}

StructuredBuffer<uint4>	final_indices : register(t1);

struct nothing {};

void null_VS(uint index : SV_VertexID) {
}

GEOM_SHADER(3)
void meshlet_GS(point nothing _[1], uint p : SV_PrimitiveID, inout TriangleStream<MeshletVertexOut> tris) {
	uint4	i = final_indices[p];
	tris.Append(meshletVS(vertices[i.x], i.w, i.x));
	tris.Append(meshletVS(vertices[i.y], i.w, i.y));
	tris.Append(meshletVS(vertices[i.z], i.w, i.z));
}

technique meshlet {
	pass p0 {
		ComputeShader = {cs_5_0, meshlet_CS};
	}
	pass p1 {
		ComputeShader = {cs_5_0, meshlet_CS2};
	}
	pass p2 {
		SET_VS(null_VS);
		SET_GS(meshlet_GS);
		SET_PS(PS_Col);
	}
};

#endif
