#include "common.fxh"
#include "lighting.fxh"
//#include "shadow.fxh"

#define SHADOW_VO
#define SHADOW_VS(v,p)	
#define GETSHADOW(v)	1

sampler_def2D(diffuse_samp, FILTER_MIN_MAG_MIP_LINEAR;);
sampler_def2D(diffuse_samp2,FILTER_MIN_MAG_MIP_LINEAR;);
sampler_def2D(color,FILTER_MIN_MAG_MIP_LINEAR;);

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

float			glossiness		= 60;
float4			diffuse_colour	= {1,1,1,1};
float			flip_normals	= 1;
float4			matrices[64];
int				mip;

//------------------------------------

struct S_Background {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
};

struct S_World {
	float4	pos		: POSITION_OUT;
	float4	world	: TEXCOORD0;
};
struct S_World2 {
	float4	pos			: POSITION_OUT;
	float4	world_near	: TEXCOORD0;
	float4	world_far	: TEXCOORD1;
};
struct S_TransformCol {
	float4	pos		: POSITION_OUT;
	float4	colour	: COLOR;
};

struct S_TransformTex {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
};

struct S_TransformColTex {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
	float4	colour	: COLOR;
};

struct S_TransformColTex2 {
	float4	pos		: POSITION_OUT;
	float4	uv01	: TEXCOORD0;
	float4	colour	: COLOR;
};

struct S_Lighting {
	float4	position: POSITION_OUT;
	float4	ambient	: AMBIENT;
	float3	normal	: TEXCOORD1;
	float3	worldpos: TEXCOORD3;
	fogtype	fog		: FOG;
	SHADOW_VO
};

struct S_LightingTex {
	float4	position: POSITION_OUT;
	float4	ambient	: AMBIENT;
	float2	uv		: TEXCOORD0;
	float3	normal	: TEXCOORD1;
	float3	worldpos: TEXCOORD3;
	fogtype	fog		: FOG;
	SHADOW_VO
};
//------------------------------------

S_Background VS_PassThrough(float3 position:POSITION, in float2 uv:TEXCOORD0) {
	S_Background	v;
	v.pos	= platform_fix(position);
	v.uv	= uv;
	return v;
}

S_Background VS_ScreenToWorld(float3 position:POSITION) {
	S_Background	v;
	v.pos			= platform_fix(position);
	v.uv			= v.pos.xy;
	return v;
}

S_World VS_ShowWorld(float3 position : POSITION) {
	S_World	v;
	v.pos	= mul(float4(position, 1.0), worldViewProj);
//	v.world	= mul(inverse_transpose(worldViewProj), v.pos);
	v.world	= float4(position, 1);
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
S_Background VS_Background(float2 position:POSITION_IN) {
	S_Background	v;
	v.uv		= position;
	v.pos		= float4(position, far_depth, 1);
	return v;
}
#endif

S_TransformCol VS_Transform(float3 position:POSITION) {
	S_TransformCol	v;
	v.colour	= diffuse_colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_TransformCol VS_Transform4(float4 position:POSITION) {
	S_TransformCol	v;
	v.colour	= diffuse_colour;
	v.pos		= mul(position, worldViewProj);
	return v;
}

S_TransformCol VS_Transform_idx(float3 position:POSITION, int4 idx:BLENDINDICES) {
	S_TransformCol	v;
	float4x4	iworld;
	iworld[0]	= matrices[idx.x + 0];
	iworld[1]	= matrices[idx.x + 1];
	iworld[2]	= matrices[idx.x + 2];
	iworld[3]	= float4(0,0,0,1);
	iworld		= mul(transpose(iworld), worldViewProj);
	v.colour	= diffuse_colour;
	v.pos		= mul(float4(position, 1), iworld);
	return v;
}

S_TransformCol VS_TransformColour(float3 position:POSITION, float4 colour:COLOR) {
	S_TransformCol	v;
	v.colour	= colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_TransformColTex VS_TransformTex(float3 position:POSITION, float2 uv:TEXCOORD0) {
	S_TransformColTex	v;
	v.uv		= uv;
	v.colour	= diffuse_colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_TransformColTex VS_TransformColourTex(float3 position:POSITION, float2 uv:TEXCOORD0, float4 colour:COLOR) {
	S_TransformColTex	v;
	v.uv		= uv;
	v.colour	= colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_TransformColTex2 VS_TransformColourTex2(float3 position:POSITION, float2 uv0:TEXCOORD0, float2 uv1:TEXCOORD1, float4 colour:COLOR) {
	S_TransformColTex2	v;
	v.uv01		= float4(uv0, uv1);
	v.colour	= colour;
	v.pos		= mul(float4(position, 1), worldViewProj);
	return v;
}

S_Lighting VS_TransformLighting(float3 position:POSITION, float3 normal:NORMAL) {
	S_Lighting	v;

	float3x3 it_world = inverse_transpose((float3x3)world);

	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(normal,			it_world)) * flip_normals;
	v.position		= mul(float4(pos, 1.0), 		ViewProj());
	v.normal		= norm;
	v.worldpos		= pos;
	v.ambient		= light_ambient;//DiffuseLight(pos, norm);
	v.fog			= VSFog(pos);
	SHADOW_VS(v,pos)
	return v;
}
S_Lighting VS_TransformLighting4(float4 position:POSITION, float3 normal:NORMAL) {
	S_Lighting	v;

	float4x4 it_world = inverse_transpose((float4x4)world);

	float4	pos		= mul(position,			world);
	float3	pos3	= pos.xyz / pos.w;
	float3	norm	= (normalise(mul(float4(normal, 0),	it_world)) * flip_normals).xyz;
	v.position		= mul(position, worldViewProj);
//	v.position		= mul(pos,				ViewProj());
	v.normal		= norm;
	v.worldpos		= pos3;
	v.ambient		= light_ambient;//DiffuseLight(pos3, norm);
	v.fog			= VSFog(pos3);
	SHADOW_VS(v,pos3)
	return v;
}

S_LightingTex VS_TransformLightingTex(float3 position:POSITION, float3 normal:NORMAL, float2 uv:TEXCOORD0) {
	S_LightingTex	v;

	float3x3 it_world = inverse_transpose((float3x3)world);

	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(normal,			it_world)) * flip_normals;
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

float4 PS_Background(S_Background v) : OUTPUT0 {
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
float4 PS_Col(S_TransformCol v): OUTPUT0 {
	return v.colour;
}
float4 PS_Tex(S_TransformColTex v) : OUTPUT0 {
	return float4(tex2D(diffuse_samp, v.uv).rgb, 1);
}
float4 PS_TexCol(S_TransformColTex v) : OUTPUT0 {
	return tex2D(diffuse_samp, v.uv) * v.colour;
}

float4 PS_Blend(S_TransformCol v) : OUTPUT0 {
	return Blend(v.colour);
}

float4 PS_Add(S_TransformCol v) : OUTPUT0 {
	return Add(v.colour);
}

float4 PS_TexBlend(S_TransformColTex v) : OUTPUT0 {
	return Blend(tex2D(diffuse_samp, v.uv) * v.colour);
}

#ifdef PLAT_WII
float4 PS_TexBlendMask(float2 uv0:TEXCOORD0, float2 uv1:TEXCOORD1, float4 colour:COLOR) : OUTPUT0 {
	float4	t0 = tex2D(linear_samp, uv0);
	float4	t1 = tex2D(linear_samp2, uv1);
	return Blend(float4(t0.rgb, t0.a * t1.r) * colour);
}
#else
float4 PS_TexBlendMask(S_TransformColTex2 v) : OUTPUT0 {
	float4	t0 = tex2D(linear_samp, v.uv01.xy);
	float4	t1 = tex2D(linear_samp2, v.uv01.zw);
	return Blend(float4(t0.rgb, t0.a * t1.r) * v.colour);
}
#endif

float4 PS_TexAdd(S_TransformColTex v) : OUTPUT0 {
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
float4 PS_TexDistMask(S_TransformColTex2 v) : OUTPUT0 {
	float4	t = tex2D(linear_samp, v.uv01.xy);
	return Distance1(Blend(float4(t.rgb, tex2D(linear_samp2, v.uv01.zw).r) * v.colour), 0.5, t.a, max(abs(ddx(v.uv01.x)), abs(ddx(v.uv01.y))) * 16);
}
#endif

float4 PS_Specular(S_Lighting v) : OUTPUT0 {
	return FogSpecularLight(
		v.worldpos.xyz,
		normalise(v.normal),
		diffuse_colour,
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
		tex2D(color, v.uv),
		v.ambient,
		glossiness,
		1,
		GETSHADOW(v),
		v.fog
	);
}

#if defined(PLAT_PC)// && !defined(USE_DX11)

#ifdef USE_DX11
float4 PS_DepthMin(S_Background v) : OUTPUT0 {
	float2	uv	= v.uv;
	float	dx	= ddx(uv.x), dy = ddy(uv.y);
	float	v0 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(0, 0), mip).x;
	float	v1 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(dx, 0), mip).x;
	float	v2 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(0, dy), mip).x;
	float	v3 = _zbuffer.t.SampleLevel(point_sampler, uv + float2(dx, dy), mip).x;
	return min(min(v0, v1), min(v2, v3));
}
#else
float4 PS_DepthMin(S_Background v) : OUTPUT0 {
	float2	uv	= v.uv;
	float	dx	= ddx(uv.x) * 2, dy = ddy(uv.y) * 2;
	float	v0 = PointSample(_zbuffer, uv + float2(0, 0)).x;
	float	v1 = PointSample(_zbuffer, uv + float2(dx, 0)).x;
	float	v2 = PointSample(_zbuffer, uv + float2(0, dy)).x;
	float	v3 = PointSample(_zbuffer, uv + float2(dx, dy)).x;
	return min(min(v0, v1), min(v2, v3));
}
#endif

float3 RayIntersect2(sampler2D s, float3 p0, float3 p1) {
	const int num_steps_lin = 20;
	
	float2	dx = ddx(p0);
	float2	dy = ddy(p0);

	float3	p = p0;
	float3	v = (p1 - p0) / ((p1 - p0).z * num_steps_lin);

	float3	prevp	= p;
	float	prevh	= 0;
	float	h		= 0;

	for (int i = 0; i < num_steps_lin; i++)	{
		h	= s.t.SampleGrad(s.s, p, dx, dy).a;
		if (p.z >= h)
			break;

		prevp	= p;
		prevh	= h;
		p		+= v;
	}
	return p;
	float delta1 = h		- p.z;
	float delta2 = prevh	- prevp.z;

	float ratio = delta1 / (delta1 + delta2);
	return lerp(p, prevp, ratio);
}


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

float3 RayIntersectMip2(sampler2D s, float3 p0, float3 p1) {
	const int mips = 10;
	
	float3	p = p0;
	float3	v = p1 - p0;

	for (int i = mips; i--;) {
		v *= 0.5;
		if (p.z < tex2D(s, p.xy).x)
			p += v;
	}

	return p;
}

float3 RayIntersect(sampler2D s, float3 p0, float3 p1) {
	const int num_steps_lin = 20;
	const int num_steps_bin = 10;

	float3	p = p0;
	float3	v = (p1 - p0) / ((p1 - p0).z * num_steps_lin);

	int i;
	for (i = 0; i < num_steps_lin && p.z < tex2D(s, p.xy).x; i++)
		p += v;

	for (i = 0; i < num_steps_bin; i++) {
		v *= 0.5;
		if (p.z < tex2D(s, p.xy).x)
			p += v;
		else
			p -= v;
	}
	return p;
}

float4 PS_DepthRay(S_Background v) : OUTPUT0 {
	float4x4	wvp			= transpose(worldViewProj);
	wvp[2]					= float4(0,0,1,-1);
	float4		world_far	= mul(float4(v.uv, 0, 1), cofactors(wvp));
	wvp[2]					= float4(0,0,1,+1);
	float4		world_near	= mul(float4(v.uv, 0, 1), cofactors(wvp));

//	clip(-world_far.w);
//	clip(-world_near.w);

	float3	p0	= project(world_near) * float3(0.5, 0.5, 0.5) + float3(0.5, 0.5, 0.5);
	float3	p1	= project(world_far)  * float3(0.5, 0.5, 0.5) + float3(0.5, 0.5, 0.5);
	if (any(min(p0.xy, p1.xy) > 1) || any(max(p0.xy, p1.xy) < 0))
		clip(-1);

	if (p1.z < p0.z) {
		float3	t = p0;
		p0 = p1;
		p1 = t;
	}
#if 1
	float3	p	= RayIntersectMip(linear_samp, p0, p1);
#else
	float3	p	= p0;
#endif

	float2	t	= abs(p.xy - 0.5);
	clip(0.5 - max(t.x, t.y));
	return float4(tex2D(diffuse_samp, p.xy).rgb, 1);
}
#endif

//-----------------------------------

technique background {
	pass p0 {
		SET_VS(VS_Background);
		SET_PS(PS_Background);
	}
//	PASS(p0,VS_Background,PS_Background)
}

technique coloured {
	PASS(p0,VS_Transform,PS_Col)
}

technique blend {
	PASS(p0,VS_Transform,PS_Blend)
}

technique blend4 {
	PASS(p0,VS_Transform4,PS_Blend)
}

technique blend_vc {
	PASS(p0,VS_TransformColour,PS_Blend)
}

technique add {
	PASS(p0,VS_Transform,PS_Add)
}

technique add_vc {
	PASS(p0,VS_TransformColour,PS_Add)
}

technique tex_vc {
	PASS(p0,VS_TransformColourTex,PS_TexCol)
}

technique tex {
	PASS(p0,VS_TransformTex,PS_Tex)
}

technique tex_blend {
	PASS(p0,VS_TransformTex,PS_TexBlend)
}

technique tex_blend_vc {
	PASS(p0,VS_TransformColourTex,PS_TexBlend)
}

technique tex_blend_mask {
	PASS(p0,VS_TransformColourTex2,PS_TexBlendMask)
}

technique tex_dist_mask {
	PASS(p0,VS_TransformColourTex2,PS_TexDistMask)
}

technique tex_add_vc {
	PASS(p0,VS_TransformColourTex,PS_TexAdd)
}

technique specular {
	PASS(p0,VS_TransformLighting,PS_Specular)
}

technique specular4 {
	PASS(p0,VS_TransformLighting4,PS_Specular)
}

technique tex_specular {
	PASS(p0,VS_TransformLightingTex,PS_SpecularTex)
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

#ifdef PLAT_PC
technique blend_idx {
	PASS(p0,VS_Transform_idx,PS_Blend)
}

//#ifndef USE_DX11
technique depth_min {
	PASS(p0,VS_PassThrough,PS_DepthMin)
}
technique depth_ray {
	PASS(p0,VS_ScreenToWorld,PS_DepthRay)
}

//technique depth_ray2 {
//	PASS(p0,VS_ShowWorld,PS_DepthRay)
//}
//#endif

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

#if defined(USE_DX11) || defined(PLAT_PS4)

float3	point_size = float3(1 / 50.f, 1 / 50.f, 1);

//-----------------------------------------------------------------------------
//	ThickPoint
//-----------------------------------------------------------------------------

float4 Clip(float4 p0, float4 p1) {
	return p0.z < 0
		? p0 + (p1 - p0) * -p0.z / (p1.z - p0.z)
		: p0;
}

float cross2(float2 a, float2 b) {
	return a.x * b.y - a.y * b.x;
}

float2 perp(float2 a) {
	return float2(-a.y, a.x);
}

struct ThickPoint {
	float4 pos;
};
 
ThickPoint VS_thickpoint(float3 pos : POSITION_IN) : POSITION_IN {
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
void GS_thickpoint(point ThickPoint v[1] : POSITION_IN, inout TriangleStream<S_TransformColTex> tris) {
	S_TransformColTex	p;
	p.colour = diffuse_colour;
 
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

float4 PS_thickpoint(S_TransformColTex p) : OUTPUT0 {
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

void Thickline(ThickPoint v[2] : POSITION_IN, inout TriangleStream<S_TransformColTex> tris) {
	S_TransformColTex	p;
	p.colour = diffuse_colour;
 
	float4 pw0		= Clip(v[0].pos, v[1].pos);
	float4 pw1		= Clip(v[1].pos, pw0);

	float2 pos0		= pw0.xy / pw0.w;
	float2 pos1		= pw1.xy / pw1.w;

	float2 d		= normalise(pos1 - pos0);
	float2 across	= point_size.xy * perp(d);

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
void GS_thickline(line ThickPoint v[2] : POSITION_IN, inout TriangleStream<S_TransformColTex> tris) {
	Thickline(v, tris);
}

float4 PS_thickline(S_TransformColTex p) : OUTPUT0 {
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
//-----------------------------------------------------------------------------
//	Thick line with Adjacency
//-----------------------------------------------------------------------------

float2 intersect_lines(float2 pos0, float2 dir0, float2 pos1, float2 dir1) {
	float	d = cross2(dir0, dir1);
	return d == 0
		? pos1
		: (dir0 * cross2(pos1, dir1) - dir1 * cross2(pos0, dir0)) / d;
}

float2 intersect_offsets(float2 pos, float2 dir0, float2 dir1, float2 point_size) {
	return intersect_lines(pos + perp(dir0) * point_size, dir0, pos + perp(dir1) * point_size, dir1);
}

void ThicklineA(ThickPoint v[4], inout TriangleStream<S_TransformColTex> tris) {
	S_TransformColTex	p;
	p.colour = diffuse_colour;
 
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
void GS_thicklineA(lineadj ThickPoint v[4] : POSITION_IN, inout TriangleStream<S_TransformColTex> tris) {
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

Circle VS_circle(float4	v : POSITION_IN) {
	Circle	c;
	c.v		= v;
	return c;
}

// patch Constant Function
CircleConsts HS_circleC(InputPatch<Circle, 1> c, uint id : PRIMITIVE_ID) {	
	CircleConsts	cc;

	float	w	= mul(float4(c[0].v.xyz, 1.0), worldViewProj).w;

	cc.tess[0]	= 1;
	cc.tess[1]	= c[0].v.w * point_size.z / w;
	return cc;
}

// hull shader
HULL_SHADER(HS_circleC, isoline, fractional_even, line, 1)
Circle HS_circle(InputPatch<Circle, 1> c, uint id : PRIMITIVE_ID, uint i : OUTPUT_CONTROL_POINT_ID) {
	return c[0];
}

// domain shader
DOMAIN_SHADER(isoline) 
S_TransformCol DS_circle(CircleConsts cc, float t : DOMAIN_LOCATION, const OutputPatch<Circle, 1> c) {
	S_TransformCol	p;
	float2	sc;
	sincos(t * 2 * 3.1415926535, sc.y, sc.x);
	float3	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.pos	= mul(float4(pos, 1.0), worldViewProj);
	p.colour	= diffuse_colour;
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
//	Thick circle tesselator
//-----------------------------------------------------------------------------

// domain shader
DOMAIN_SHADER(isoline) 
ThickPoint DS_thickcircle(CircleConsts cc, float t : DOMAIN_LOCATION, const OutputPatch<Circle, 1> c) : POSITION_IN {
	ThickPoint	p;
	float2	sc;
	sincos(t * 2 * 3.1415926535, sc.y, sc.x);
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
	float	scale	= 2 * 3.1415926535;

	sincos((t - dt) * scale, sc.y, sc.x);
	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.p[0].pos	= mul(float4(pos, 1.0), worldViewProj);

	sincos(t * scale, sc.y, sc.x);
	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.p[1].pos	= mul(float4(pos, 1.0), worldViewProj);

	sincos((t + dt) * scale, sc.y, sc.x);
	pos = c[0].v.xyz + float3(sc * c[0].v.w, 0);
	p.p[2].pos	= mul(float4(pos, 1.0), worldViewProj);

	return p;
}

GEOM_SHADER(5)
void GS_thickcircleA(line ThickPoints v[2] : POSITION_IN, inout TriangleStream<S_TransformColTex> tris) {
	ThickPoint	v2[4];
	v2[0] = v[0].p[0];
	v2[1] = v[0].p[1];
	v2[2] = v[1].p[1];
	v2[3] = v[1].p[2];
	ThicklineA(v2, tris);
}

GEOM_SHADER(5)
void GS_thickcircleA2(line ThickPoints v[2] : POSITION_IN, inout PointStream<S_TransformColTex> tris, inout PointStream<S_TransformColTex> points) {
	S_TransformColTex	p;
	p.colour = diffuse_colour;

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

