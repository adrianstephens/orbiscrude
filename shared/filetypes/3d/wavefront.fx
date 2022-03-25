#include "lighting.fxh"

float		Ns		= 60;
float		Ni		= 1;
float		d		= 1;
float		illum	= 1;
float3		Ka		= {0, 0, 0};
float3		Kd		= {1, 1, 1};
float3		Ks		= {1, 1, 1};

sampler_def2D(map_Ka,	FILTER_MIN_MAG_MIP_LINEAR;);
sampler_def2D(map_Kd,	FILTER_MIN_MAG_MIP_LINEAR;);
sampler_def2D(map_Ks,	FILTER_MIN_MAG_MIP_LINEAR;);
sampler_def2D(map_Bump,	FILTER_MIN_MAG_MIP_LINEAR;);

//------------------------------------

struct vertexOutput {
	float4	position		: POSITION_OUT;
	float3	normal			: NORMAL;
	float2	uv				: TEXCOORD0;
	float3	worldpos		: TEXCOORD1;
};

struct vertexOutput_unlit {
	float4	position		: POSITION_OUT;
	float2	uv				: TEXCOORD0;
};

struct vertexOutputBump {
	float4	position		: POSITION_OUT;
	highp float3	normal	: NORMAL;
	float2	uv				: TEXCOORD0;
	float3	worldpos		: TEXCOORD1;
	highp float3	tangent	: TANGENT0;
};
//------------------------------------

vertexOutput_unlit VS_unlit_untex(float3 position : POSITION) {
	vertexOutput_unlit v2;
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	return v2;
}

vertexOutput_unlit VS_unlit(float3 position : POSITION, float2 uv : TEXCOORD0) {
	vertexOutput_unlit v2;
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.uv			= uv;
    return v2;
}

vertexOutput VS_untex(float3 position : POSITION, float3 normal : NORMAL) {
	vertexOutput v2;
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(normal,			(float3x3)world));
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= norm;
	return v2;
}

vertexOutput VS(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD0) {
	vertexOutput v2;
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(normal,			(float3x3)world));
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= norm;
	v2.uv			= uv;
    return v2;
}

vertexOutputBump VS_bump(float3 position : POSITION, float3	normal : NORMAL, float2 uv : TEXCOORD0, float4	tangent : TANGENT0) {
	vertexOutputBump v2;
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(normal,			(float3x3)world));
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= norm;
	v2.uv			= uv;
	v2.tangent		= mul(float4(tangent.xyz, 1.0),(float4x3)world);
    return v2;
}


float4 PS_unlit_untex(vertexOutput_unlit v) : OUTPUT0 {
	return float4(Kd, 1);
}

float4 PS_unlit(vertexOutput_unlit v) : OUTPUT0 {
	return tex2D(map_Kd, v.uv) * float4(Kd, 1);
}

float4 PS_tex(vertexOutput v) : OUTPUT0 {
	return SpecularLight(
		v.worldpos.xyz,
		v.normal.xyz,
		tex2D(map_Kd, v.uv) * float4(Kd, 1),
		float4(Ka, 1),
		Ns,
		1,
		1
	);
}

float4 PS_untex(vertexOutput v) : OUTPUT0 {
	return float4(v.normal, 1);
	return SpecularLight(
		v.worldpos.xyz,
		v.normal.xyz,
		float4(1,1,1,1),//float4(Kd, 1),
		float4(1,1,1,1),//float4(Ka, 1),
		60,//Ns,
		1,
		1
	);
}

float4 PS_bump(vertexOutputBump v) : OUTPUT0 {
	return SpecularLight(
		v.worldpos.xyz,
		//v.normal.xyz,
		//v.tangent.xyz,
		mul(tex2D(map_Bump, v.uv).xyz - float3(.5, .5, 0), GetBasisZX(v.normal, v.tangent)),
//		GetNormal(map_Bump, v.uv, 0, v.normal.xyz, v.tangent),
		tex2D(map_Kd, v.uv) * float4(Kd, 1),
		float4(Ka, 1),
		Ns,
		1,
		1
	);
}

technique unlit_untextured {
	PASS(p0,VS_unlit_untex,PS_unlit_untex)
}

technique unlit_textured {
	PASS(p0,VS_unlit,PS_tex)
}

technique lit_untextured {
	PASS(p0,VS_untex,PS_untex)
}

technique lit_textured {
	PASS(p0,VS,PS_tex)
}

technique bumpmapped {
	PASS(p0,VS_bump,PS_bump)
}
