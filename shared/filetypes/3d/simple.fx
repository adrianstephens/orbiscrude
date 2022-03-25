#include "lighting.fxh"

sampler_def2D(DiffuseTexture,FILTER_MIN_MAG_MIP_LINEAR;);

//------------------------------------

struct vertexInput {
	float3	position		: POSITION;
	float3	normal			: NORMAL;
	float2	uv				: TEXCOORD0;
};

struct vertexInput_col {
	float3	position		: POSITION;
	float3	normal			: NORMAL;
	float3	colour			: COLOR0;
	float2	uv				: TEXCOORD0;
};

struct vertexInputBT {
	float3	position		: POSITION;
	float4	tangent			: TANGENT;
	float3	binormal		: BINORMAL;
	float2	uv				: TEXCOORD0;
};

struct vertexInputBT_col {
	float3	position		: POSITION;
	float4	tangent			: TANGENT;
	float3	binormal		: BINORMAL;
	float3	colour			: COLOR0;
	float2	uv				: TEXCOORD0;
};

float3	get_norm(in vertexInput v)				{ return v.normal;	}
void	set_norm(inout vertexInput v, float3 n)	{ v.normal = n;		}

struct vertexOutput {
	float4	position		: POSITION_OUT;
	float4	ambient			: AMBIENT;
	float3	worldpos		: TEXCOORD1;
	float3	normal			: TEXCOORD2;
	float	fog				: FOG;
};
#define INPUTS_vertexOutput
#define PARAMS_vertexOutput

struct vertexOutput_tex {
	float4	position		: POSITION_OUT;
	float4	ambient			: AMBIENT;
	float2	uv				: TEXCOORD0;
	float3	worldpos		: TEXCOORD1;
	float3	normal			: TEXCOORD2;
	float	fog				: FOG;
};
#define INPUTS_vertexOutput_tex
#define PARAMS_vertexOutput_tex

struct vertexOutput_col {
	float4	position		: POSITION_OUT;
	float4	ambient			: AMBIENT;
	float4	colour			: COLOR0;
	float3	worldpos		: TEXCOORD1;
	float3	normal			: TEXCOORD2;
	float	fog				: FOG;
};

struct vertexOutput_tex_col {
	float4	position		: POSITION_OUT;
	float4	ambient			: AMBIENT;
	float4	colour			: COLOR0;
	float2	uv				: TEXCOORD0;
	float3	worldpos		: TEXCOORD1;
	float3	normal			: TEXCOORD2;
	float	fog				: FOG;
};

#define PATCH_TEX	1

//------------------------------------

vertexOutput VS_pos(float3 position : POSITION) {
	vertexOutput v2;
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= float3(0, 0, 0);
	v2.ambient		= float4(.25, .25, .25, 1);
	v2.fog			= 0;
    return v2;
}

vertexOutput VS(float3 position : POSITION, float3 normal : NORMAL) {
	vertexOutput v2;
	float3	pos		= mul(float4(position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(normal,			(float3x3)world));
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= norm;
	v2.ambient		= DiffuseLight(pos, norm);
	v2.fog			= VSFog(pos);
    return v2;
}

vertexOutput VS_bt(float3 position : POSITION, float4 tangent : TANGENT, float3 binormal : BINORMAL) {
	return VS(position, cross(binormal, tangent.xyz) * tangent.w);
}

vertexOutput_tex VS_tex(vertexInput v) {
	vertexOutput_tex v2;
	float3	pos		= mul(float4(v.position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(v.normal,		(float3x3)world));
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= norm;
	v2.uv			= v.uv;
	v2.ambient		= DiffuseLight(pos, norm);
	v2.fog			= VSFog(pos);
    return v2;
}

vertexOutput_col VS_col(vertexInput_col v) {
	vertexOutput_col v2;
	float3	pos		= mul(float4(v.position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(v.normal,		(float3x3)world));
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= norm;
	v2.colour		= float4(v.colour, 1);
	v2.ambient		= DiffuseLight(pos, norm);
	v2.fog			= VSFog(pos);
    return v2;
}

vertexOutput_tex_col VS_tex_col(vertexInput_col v) {
	vertexOutput_tex_col v2;
	float3	pos		= mul(float4(v.position, 1.0),	(float4x3)world);
	float3	norm	= normalise(mul(v.normal,		(float3x3)world));
	v2.position		= mul(float4(pos, 1.0), 		ViewProj());
	v2.worldpos		= pos;
	v2.normal		= norm;
	v2.colour		= float4(v.colour, 1);
	v2.uv			= v.uv;
	v2.ambient		= DiffuseLight(pos, norm);
	v2.fog			= VSFog(pos);
    return v2;
}

float4 VS_shadow(float3 pos: POSITION) : POSITION {
	return mul(float4(pos, 1.0), worldViewProj);
}

float4 NoLight(float4 colour, float4 ambient) {
	return colour;// * ambient;
}

float4 PS(vertexOutput v) : OUTPUT0 {
	return Blend(Fog(NoLight(
		float4(1,1,1,1),
		v.ambient
	), v.fog));
}

float4 PS_lit(vertexOutput v) : OUTPUT0 {
	return Blend(Fog(DiffuseLight(
		v.worldpos.xyz,
		v.normal.xyz,
		float4(1,1,1,1),
		v.ambient,
		1
	), v.fog));
}

float4 PS_tex(vertexOutput_tex v) : OUTPUT0 {
	return Blend(Fog(NoLight(
		tex2D(DiffuseTexture, v.uv),
		v.ambient
	), v.fog));

}

float4 PS_tex_lit(vertexOutput_tex v) : OUTPUT0 {
	return Blend(Fog(DiffuseLight(
		v.worldpos.xyz,
		v.normal.xyz,
		tex2D(DiffuseTexture, v.uv),
		v.ambient,
		1
	), v.fog));
}

float4 PS_col(vertexOutput_col v) : OUTPUT0 {
	return Blend(Fog(NoLight(
		v.colour,
		v.ambient
	), v.fog));
}


float4 PS_col_lit(vertexOutput_col v) : OUTPUT0 {
	return Blend(Fog(DiffuseLight(
		v.worldpos.xyz,
		v.normal.xyz,
		v.colour,
		v.ambient,
		1
	), v.fog));
}

float4 PS_tex_col(vertexOutput_tex_col v) : OUTPUT0 {
	return Blend(Fog(NoLight(
		v.colour * tex2D(DiffuseTexture, v.uv),
		v.ambient
	), v.fog));
}


float4 PS_tex_col_lit(vertexOutput_tex_col v) : OUTPUT0 {
	return Blend(Fog(DiffuseLight(
		v.worldpos.xyz,
		v.normal.xyz,
		v.colour * tex2D(DiffuseTexture, v.uv),
		v.ambient,
		1
	), v.fog));
}

//-----------------------------------------------------------------------------

#define TECHNIQUE(NAME,VS,PS)\
technique NAME {\
	PASS(p0, VS, PS)\
	PASS_0(shadow, VS_shadow)\
}

TECHNIQUE(unlit,		VS_pos,		PS)
TECHNIQUE(lite,			VS,			PS_lit)
TECHNIQUE(tex,			VS_tex,		PS_tex)
TECHNIQUE(tex_lit,		VS_tex,		PS_tex_lit)
TECHNIQUE(col,			VS_col,		PS_col)
TECHNIQUE(col_lit,		VS_col,		PS_col_lit)
TECHNIQUE(tex_col,		VS_tex_col,	PS_tex_col)
TECHNIQUE(tex_col_lit,	VS_tex_col,	PS_tex_col_lit)

TECHNIQUE(lite_bt,		VS_bt,		PS_lit)


#include "patch.fxh"
PATCH_TECHNIQUE(textured,			VS_tex,			PS_tex,		vertexOutput_tex)

#include "skin.fxh"
SKIN_TECHNIQUE(textured,			VS_tex,			PS_tex,		vertexOutput_tex)
