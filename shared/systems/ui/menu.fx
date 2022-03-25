#include "menu.fxh"

//-----------------------------------------------------------------------------
//	vertex shaders
//-----------------------------------------------------------------------------

struct S_Transform {
	float4	pos		: POSITION_OUT;
	float4	colour	: COLOR0;
};
S_Transform VS_Transform(float3 position:POSITION) {
	S_Transform	v;
	v.colour	= diffuse_colour;
	v.pos		= ui_pos(position);
	return v;
}

struct S_TransformVPos {
	float4	pos		: POSITION_OUT;
	float4	colour	: COLOR0;
	float4	vpos	: TEXCOORD6;
};
S_TransformVPos VS_TransformVPos(float3 position:POSITION) {
	S_TransformVPos	v;
	v.colour	= diffuse_colour;
	v.pos		= ui_pos(position, v.vpos);
	return v;
}

struct S_TransformColourTex {
	float4	pos			: POSITION_OUT;
	float2	uv			: TEXCOORD0;
	float4	colour_mul	: COLOR0;
	float4	colour_add	: COLOR1;
};
S_TransformColourTex VS_TransformColourTex(float3 position:POSITION, float2 uv:TEXCOORD0, float4 colour:COLOR) {
	S_TransformColourTex	v;
	v.uv		= uv;
	ui_colours(colour, v.colour_mul, v.colour_add);
	v.pos		= ui_pos(position);
	return v;
}

struct S_TransformColourTexVPos {
	float4	pos			: POSITION_OUT;
	float2	uv			: TEXCOORD0;
	float4	colour_mul	: COLOR0;
	float4	colour_add	: COLOR1;
	float4	vpos		: TEXCOORD6;
};
S_TransformColourTexVPos VS_TransformColourTexVPos(float3 position:POSITION, float2 uv:TEXCOORD0, float4 colour:COLOR) {
	S_TransformColourTexVPos	v;
	v.uv		= uv;
	ui_colours(colour, v.colour_mul, v.colour_add);
	v.pos		= ui_pos(position, v.vpos);
	return v;
}


struct S_TransformColourTexUnnorm {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
	float4	colour	: COLOR0;
};
S_TransformColourTexUnnorm VS_TransformColourTexUnnorm(float3 position:POSITION, float2 uv:TEXCOORD0, float4 colour:COLOR) {
	S_TransformColourTexUnnorm	v;
	v.uv		= UnnormaliseUV(font_size.xy, uv);
	v.colour	= colour;
	v.pos		= ui_pos(position);
	return v;
}

struct S_TransformColourTexUnnormVPos {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
	float4	colour	: COLOR0;
	float4	vpos	: TEXCOORD6;
};
S_TransformColourTexUnnormVPos VS_TransformColourTexUnnormVPos(float3 position:POSITION, float2 uv:TEXCOORD0, float4 colour:COLOR) {
	S_TransformColourTexUnnormVPos	v;
	v.uv		= UnnormaliseUV(font_size.xy, uv);
	v.colour	= colour;
	v.pos		= ui_pos(position, v.vpos);
	return v;
}

//-----------------------------------------------------------------------------
//	pixel shaders
//-----------------------------------------------------------------------------

float4 PS_DestPixel(float4 vpos) {
	//float3 p = vpos.xyz / vpos.w;
	return tex2D(_screen, vpos.xy / vpos.w);
}

float4 PS_Blend(S_Transform v): OUTPUT0 {
	return Blend(v.colour);
}
float4 PS_BlendVPos(S_TransformVPos v) : OUTPUT0 {
	vpos_clip(v.vpos);
	return Blend(v.colour);
}

float4 PS_TexBlend(S_TransformColourTex v): OUTPUT0 {
	return Blend(tex2D(diffuse_samp, v.uv) * v.colour_mul + v.colour_add);
}
float4 PS_TexBlendVPos(S_TransformColourTexVPos v): OUTPUT0 {
	vpos_clip(v.vpos);
	return Blend(tex2D(diffuse_samp, v.uv) * v.colour_mul + v.colour_add);
}
float4 PS_TexPreMult(S_TransformColourTex v): OUTPUT0 {
	return tex2D(diffuse_samp, v.uv) * v.colour_mul + v.colour_add;
}

float4 PS_TexAdjBlend(S_TransformColourTex v): OUTPUT0 {
	float4	c = tex2D(diffuse_samp, v.uv) * v.colour_mul + v.colour_add;
	return Blend(adjust_colour(c));
}

float4 PS_FontDist(S_TransformColourTexUnnorm v): OUTPUT0 {
	return FontDist(v.uv, v.colour);
}
float4 PS_FontDistVPos(S_TransformColourTexUnnormVPos v): OUTPUT0 {
	vpos_clip(v.vpos);
	return FontDist(v.uv, v.colour);
}

float4 PS_FontDistOutline(S_TransformColourTexUnnorm v): OUTPUT0 {
	return FontDistOutline(v.uv, v.colour);
}
float4 PS_FontDistOutlineVPos(S_TransformColourTexUnnormVPos v): OUTPUT0 {
	vpos_clip(v.vpos);
	return FontDistOutline(v.uv, v.colour);
}

float4 PS_FontDistGlow(S_TransformColourTexUnnorm v): OUTPUT0 {
	return FontDistGlow(v.uv, v.colour);
}
float4 PS_FontDistGlowVPos(S_TransformColourTexUnnormVPos v): OUTPUT0 {
	vpos_clip(v.vpos);
	return FontDistGlow(v.uv, v.colour);
}

//-----------------------------------
//		techniques
//-----------------------------------

technique font_dist {
	PASS(p0,VS_TransformColourTexUnnorm,PS_FontDist)
	PASS(p1,VS_TransformColourTexUnnormVPos,PS_FontDistVPos)
}
technique font_dist_outline {
	PASS(p0,VS_TransformColourTexUnnorm,PS_FontDistOutline)
	PASS(p1,VS_TransformColourTexUnnormVPos,PS_FontDistOutlineVPos)
}
technique font_dist_glow {
	PASS(p0,VS_TransformColourTexUnnorm,PS_FontDistGlow)
	PASS(p1,VS_TransformColourTexUnnormVPos,PS_FontDistGlowVPos)
}

technique blend_solid {
	PASS(p0,VS_Transform,PS_Blend)
	PASS(p1,VS_TransformVPos,PS_BlendVPos)
}

technique blend_texture {
	PASS(normal,VS_TransformColourTex,PS_TexBlend)
	PASS(vpos,VS_TransformColourTexVPos,PS_TexBlendVPos)
	PASS(premult,VS_TransformColourTex,PS_TexPreMult)
	PASS(multiply,VS_TransformColourTex,PS_TexPreMult)	// mult done with blend mode
}

technique colouradj_texture {
	PASS(normal,VS_TransformColourTex,PS_TexAdjBlend)
}
