#include "common.fxh"
#include "lighting.fxh"
#include "brdf.fxh"

sampler_def2D(baseColorTexture, FILTER_MIN_MAG_MIP_LINEAR;);
float4		baseColorFactor;

sampler_def2D(metallicRoughnessTexture, FILTER_MIN_MAG_MIP_LINEAR;);
float		metallicFactor;
float		roughnessFactor;

sampler_def2D(normalTexture, FILTER_MIN_MAG_MIP_LINEAR;);
float		normalScale;

sampler_def2D(occlusionTexture, FILTER_MIN_MAG_MIP_LINEAR;);
float		occlusionStrength;

sampler_def2D(emissiveTexture, FILTER_MIN_MAG_MIP_LINEAR;);
float3		emissiveFactor;

uniform AreaLight		area_light = {
	0.5, 0.6, 1, {1,1,1}
};

struct VSOutput {
	float4	position	: SV_Position;
	float3	normal		: normal;
	float4	ambient		: AMBIENT;
};

struct VSOutput_tex {
	float4	position	: SV_Position;
	float3	normal		: normal;
	float4	ambient		: AMBIENT;
	float2	uv			: TEXCOORD0;
};

struct MaterialParams {
	float3 DiffuseColor;
	float Roughness;
	float3 SpecularColor;
	float Metalness;
	float Anisotropy;
};
struct DirectLighting {
	float3 Diffuse;
	float3 Specular;
	float3 Transmission;
};


DirectLighting DefaultLitBxDF(BxDFContext Context, MaterialParams mat, float falloff, AreaLight area_light) {
	DirectLighting	direct;
	float3 falloff_color = area_light.FalloffColor * Context.Ldot.y * falloff;
	direct.Diffuse		= falloff_color * Diffuse_Lambert(mat.DiffuseColor);
	direct.Specular		= falloff_color * SpecularGGX(mat.Roughness, mat.Anisotropy, mat.SpecularColor, Context, area_light);
	direct.Transmission	= 0;
	return direct;
}


VSOutput vs(float3 position:POSITION, float3 normal:NORMAL) {
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= mul(normal,					(float3x3)world);

	VSOutput vsOutput;
	vsOutput.position	= mul(float4(pos, 1.0), ViewProj());
	vsOutput.normal		= norm;
	vsOutput.ambient	= 0;//DiffuseLight(pos, norm);
	return vsOutput;
}

VSOutput vs_int(int3 position:POSITION, float3 normal:NORMAL) {
	return vs(position, normal);
}

float3 ps(VSOutput v) : SV_Target0 {
	float3	L	= shadowlight_dir;
	float3	N	= v.normal;
	float3	V	= eyeDir(v.position.xyz);

	MaterialParams material = {
		float3(1,1,1) * baseColorFactor, roughnessFactor,
		{1, 1, 1}, metallicFactor, 0
	};
	
	BxDFContext		Context = MakeBxDFContext(N, V, L);
	DirectLighting	direct	= DefaultLitBxDF(Context, material, 1, area_light);
	return direct.Diffuse + direct.Specular + direct.Transmission;
}

VSOutput_tex vs_tex(float3	position:POSITION, float3 normal:NORMAL, float2 uv:TEXCOORD0) {
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= mul(normal,					(float3x3)world);

	VSOutput_tex vsOutput;
	vsOutput.position	= mul(float4(pos, 1.0), ViewProj());
	vsOutput.normal		= norm;
	vsOutput.ambient	= 0;//DiffuseLight(pos, norm);
	vsOutput.uv			= uv;
	return vsOutput;
}

float3 ps_tex(VSOutput_tex v) : SV_Target0 {
#if 0
	return SpecularLight(
		v.position.xyz,
		normalise(v.normal),
		tint,
		v.ambient,
		metallicFactor,
		1,
		1	//GETSHADOW(v)
	);
#else

	float3	L	= shadowlight_dir;
	float3	N	= v.normal;
	float3	V	= eyeDir(v.position.xyz);

	MaterialParams material = {
		tex2D(baseColorTexture, v.uv).rgb * baseColorFactor, roughnessFactor,
		{1, 1, 1}, metallicFactor, 0
	};
	
	BxDFContext		Context = MakeBxDFContext(N, V, L);
	DirectLighting	direct	= DefaultLitBxDF(Context, material, 1, area_light);
	return direct.Diffuse + direct.Specular + direct.Transmission;
#endif
}

technique notex {
	PASS(p0, vs, ps)
};
technique tex {
	PASS(p0, vs_tex, ps_tex)
};
technique notex_int {
	PASS(p0, vs_int, ps)
};
