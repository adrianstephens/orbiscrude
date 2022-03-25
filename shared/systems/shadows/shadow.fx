#include "common.fxh"
#include "shadow.fxh"

#ifdef __PSSL__
#pragma PSSL_target_output_format(default FMT_32_ABGR)
#endif

sampler_def2D(_shadowmap,
	FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
sampler_def2D(_near,
	FILTER_MIN_MAG_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
sampler_def2D(_far,
	FILTER_MIN_MAG_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
sampler_def2D(_global,
	FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

float3 		blur_samples[2];

//-----------------------------------------------------------------------------
//	vertex shaders
//-----------------------------------------------------------------------------

struct S_Blur {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
};

S_Blur VS_Blur(float2 position:POSITION, in float2 uv:TEXCOORD0) {
	S_Blur	v;
	v.uv	= uv;
	v.pos	= platform_fix(float3(position, 0));
	return v;
}

#ifndef USE_SHADOWS
float4 VS_Splat(float2 position:POSITION) : POSITION_OUT {
	return float4(position,0,0);
}

#elif defined PLAT_GLSL

float4	uv_adjust;
float4 VS_Splat(float2 position:POSITION, in float2 uv0:TEXCOORD0, out float2 uv0_out:TEXCOORD0, out float2 uv1_out:TEXCOORD1) : POSITION_OUT {
	uv0_out	= uv0;
	uv1_out	= uv0 * uv_adjust.xy + uv_adjust.zw;
	return platform_fix(float3(position, 0));
}

#else

struct S_Splat {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
	float4	uvg		: TEXCOORD1;
};

S_Splat VS_Splat(float2 position:POSITION, in float2 uv:TEXCOORD0) {
	S_Splat	v;
	v.uv	= uv;
	v.uvg	= (uv.xyxy - shadow_nearfar_add) /  shadow_nearfar_mult;
	v.pos	= platform_fix(float3(position, 0));
	return v;
}
#endif
//-----------------------------------------------------------------------------
//	pixel shaders
//-----------------------------------------------------------------------------

#ifdef PLAT_X360 // force uv clamping on the 360
float2 uv_clamp(float2 uv) { return saturate(uv); }
float4 uv_clamp(float4 uv) { return saturate(uv); }
#else
float2 uv_clamp(float2 uv) { return uv; }
float4 uv_clamp(float4 uv) { return uv; }
#endif

float4 ShadowBlur(sampler2D s, float2 uv) {
	return (tex2D(s, uv_clamp(uv + blur_samples[0].xy))
		+	tex2D(s, uv_clamp(uv - blur_samples[0].xy))) * blur_samples[0].z
		+  (tex2D(s, uv_clamp(uv + blur_samples[1].xy))
		+	tex2D(s, uv_clamp(uv - blur_samples[1].xy))) * blur_samples[1].z;
}
float4 PS_ShadowBlur_ColorTarget(S_Blur v) : OUTPUT0 {
	return ShadowBlur(_shadowmap, v.uv).xxxx;
}
float4 PS_ShadowBlur_DepthTarget(S_Blur v, out float depth : OUTPUT_DEPTH) : OUTPUT0 {
	depth = ShadowBlur(_shadowmap, v.uv).x;
	return depth.xxxx;
}
float4 PS_ShadowBlur_RG16(S_Blur v) : OUTPUT0 {
#ifdef PLAT_PS3
	return EncodeRG16(ShadowBlur(_shadowmap, v.uv).rg);
#else
	return ShadowBlur(_shadowmap, v.uv).rgrg;
#endif
}

#ifndef USE_SHADOWS
float4 PS_Splat() : OUTPUT0 {
	return float4(0,0,0,0);
}

#elif defined PLAT_GLSL
float4 PS_Splat(float2 uv0 : TEXCOORD0, float2 uvg : TEXCOORD1) : OUTPUT0 {
	return furthest(PointSample(_global, uv0).x, PointSample(_global, uvg).x);
}

#else
float4 PS_Splat(S_Splat v) : OUTPUT0 {
	float2 nf	= float2(PointSample(_near, uv_clamp(v.uv)).x, PointSample(_far, uv_clamp(v.uv)).x);
	float2 g	= float2(PointSample(_global, uv_clamp(v.uvg.xy)).x, PointSample(_global, uv_clamp(v.uvg.zw)).x);
	//return float4(nf, 0, 1);
	return EncodeRG16(nearest(nf, g));
}
#endif
//-----------------------------------------------------------------------------
//	techniques
//-----------------------------------------------------------------------------

technique shadowblur {
	PASS(colortarget,VS_Blur,PS_ShadowBlur_ColorTarget)
#ifndef PLAT_GLSL
	PASS(depthtarget,VS_Blur,PS_ShadowBlur_DepthTarget)
	PASS(rg16,VS_Blur,PS_ShadowBlur_RG16)
#endif
}

technique splat {
	PASS(p0,VS_Splat,PS_Splat)
}
