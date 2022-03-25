#include "common.fxh"
#include "lighting.fxh"
#include "particle.fxh"

sampler_def2DArray(diffuse_samp,
	FILTER_MIN_MAG_MIP_LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = WRAP;
);

sampler_def2D(normal_samp,
	FILTER_MIN_MAG_MIP_LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = WRAP;
);

//------------------------------------

struct vertexOutput {
	float4	position		: POSITION_OUT;
	float4	uv				: TEXCOORD0;
	highp float4 colour		: COLOR;
};

struct vertexOutput_soft {
	float4	position		: POSITION_OUT;
	float4	position2		: TEXCOORD1;
	float4	uv				: TEXCOORD0;
	highp float4 colour		: COLOR;
};

struct vertexOutput_norm {
	float4	position		: POSITION_OUT;
	float4	uv				: TEXCOORD0;
	float3	normal			: TEXCOORD1;
	float3	tangent			: TEXCOORD2;
	float3	worldpos		: TEXCOORD3;
	highp float4 colour		: COLOR;
};

//------------------------------------

vertexOutput VS_TransformColourTex3D(ParticleVertex v) {
	vertexOutput v2;
	v2.position	= mul(float4(v.position, 1.0), worldViewProj);
#ifdef PLAT_WII
	v2.uv		= v.uv;
	v2.colour	= v.colour;
#else
	v2.uv		= VS_texARRAY(v.uv);
	v2.colour	= Blend(v.colour * tint);
#endif
	return v2;
}

vertexOutput_soft VS_TransformColourTex3D_Soft(ParticleVertex v) {
	vertexOutput_soft v2;
	v2.position	= mul(float4(v.position, 1.0), worldViewProj);
	v2.position2= platform_to_map(v2.position);
#ifdef PLAT_WII
	v2.uv		= v.uv;
	v2.colour	= v.colour;
#else
	v2.uv		= VS_texARRAY(v.uv);
	v2.colour	= Blend(v.colour * tint);
#endif
	return v2;
}

vertexOutput_norm VS_TransformNormTex3D(ParticleVertex v) {
	vertexOutput_norm v2;
	v2.position	= mul(float4(v.position, 1.0),	worldViewProj);
	v2.normal	= normalise(v.normal);
	v2.tangent	= normalise(v.tangent.xyz);
	v2.worldpos	= v.position;
#ifdef PLAT_WII
	v2.uv		= v.uv;
#else
	v2.uv		= VS_texARRAY(v.uv);
#endif
	v2.colour	= v.colour;
	return v2;
}

//-----------------------------------

highp float4 PS_Textured3D(vertexOutput v): OUTPUT0 {
#ifdef PLAT_WII
	return Blend(v.colour * tint) * tex2D(diffuse_samp, v.uv.xy);
#else
	highp float4	a = v.colour;
	highp float4	b = PS_texARRAY(diffuse_samp, v.uv.xyz);
	return a * b;
//	return v.colour * PS_texARRAY(diffuse_samp, v.uv.xyz);
#endif
}

float4 PS_Textured3D_Soft(vertexOutput_soft v): OUTPUT0 {
#ifdef PLAT_WII
	return v.colour * tex2D(diffuse_samp, v.uv.xy);
#else
	float4	p	= v.position2;
	float	a	= saturate((GetDepth2(_zbuffer, p.xy / p.w) - p.w) * v.uv.w);
	return v.colour * a * PS_texARRAY(diffuse_samp, v.uv.xyz);
#endif
}

float4 PS_TexturedNorm3D(vertexOutput_norm v): OUTPUT0 {
#ifdef PLAT_WII
	float3	normal	= v.normal;
	float4	diffuse	= saturate(dot(normal, shadowlight_dir));
	return Blend(diffuse * tex2D(diffuse_samp, v.uv.xy));
#else
	float3	normal	= normalise(mul(GetNormalU(normal_samp, v.uv.xy), GetBasisZX(v.normal, v.tangent)));
	float4	diffuse	= DiffuseIrradiance(normal) + saturate(dot(normal, shadowlight_dir)) * float4(shadowlight_col, 1);
	return Blend(diffuse * PS_texARRAY(diffuse_samp, v.uv.xyz) * v.colour);
#endif
}

//-----------------------------------


technique textured_col {
	PASS(p0, VS_TransformColourTex3D, PS_Textured3D)
}

#if !defined(PLAT_WII)

PARTICLE_TECHNIQUE1(particles,			vertexOutput,		VS_TransformColourTex3D,		PS_Textured3D)
PARTICLE_TECHNIQUE2(particles2,			vertexOutput,		VS_TransformColourTex3D,		PS_Textured3D)
PARTICLE_TECHNIQUE1(particles_soft,		vertexOutput_soft,	VS_TransformColourTex3D_Soft,	PS_Textured3D_Soft)
PARTICLE_TECHNIQUE2(particles_soft2,	vertexOutput_soft,	VS_TransformColourTex3D_Soft,	PS_Textured3D_Soft)
PARTICLE_TECHNIQUE1(particles_norm,		vertexOutput_norm,	VS_TransformNormTex3D,			PS_TexturedNorm3D)
PARTICLE_TECHNIQUE2(particles_norm2,	vertexOutput_norm,	VS_TransformNormTex3D,			PS_TexturedNorm3D)

#endif
