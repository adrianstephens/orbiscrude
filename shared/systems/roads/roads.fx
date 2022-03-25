#include "common.fxh"
#include "lighting.fxh"

#include "cubic.fxh"
#include "shadow.fxh"

struct S_LightingTex {
	float4	position: POSITION_OUT;
	float4	ambient	: AMBIENT;
	float2	uv		: TEXCOORD0;
	float3	normal	: NORMAL;
	float3	worldpos: TEXCOORD3;
	fogtype	fog		: FOG;
	SHADOW_VO
};

DataBuffer<float4>	xsect_buffer : register(t0);
Texture2D			color_t : register(t0);
sampler_tex2D(color, color_t, FILTER_MIN_MAG_MIP_LINEAR;);

//-----------------------------------------------------------------------------
//	Bezier Spline
//-----------------------------------------------------------------------------

static const float4x4	bezier_blend = {
	-1,  3, -3,  1,
	 3, -6,  3,  0,
	-3,  3,  0,  0,
	 1,  0,  0,  0
};
static const float3x4	bezier_tangentblend = {
	-3,  9, -9,  3,
	 6,-12,  6,  0,
	-3,  3,  0,  0
};

struct RoadConsts {
	float		edge[4]		: TESS_FACTOR;
	float		inside[2]	: INSIDE_TESS_FACTOR;
	float4x4	blend[4]	: BLEND;
	float2		min_max_x	: MIN_MAX_X;
	uint		xsect_num	: NUM;
};

float4x4 offset_spline(float4x4 spline, float x) {
	float4x4	blend			= mul(bezier_blend, spline);
	float3x4	tangentblend	= mul(bezier_tangentblend, spline);

	float4	f1	= mul(cubic_param(1/3.0), blend);
	float4	f2	= mul(cubic_param(2/3.0), blend);
	float3	t1	= normalise(mul(quadratic_param(1/3.0), tangentblend));
	float3	t2	= normalise(mul(quadratic_param(2/3.0), tangentblend));
	float3	n1	= normalise(float3(t1.y, -t1.x, 0));
	float3	n2	= normalise(float3(t2.y, -t2.x, 0));
		
	float4	d0	= float4((spline[1] - spline[0]).xyz, 0);
	float4	d1	= float4((spline[2] - spline[3]).xyz, 0);

	float4	c0	= float4(spline[0].xyz + normalise(float3(d0.y, -d0.x, 0)) * x, 1);
	float4	c3	= float4(spline[3].xyz + normalise(float3(-d1.y, d1.x, 0)) * x, 1);
	
	return float4x4(
		c0,
		c0 + d0 * length((f1 + n1 * x - c0).xz) / length((f1 - spline[0]).xy),
		c3 + d1 * length((f2 + n2 * x - c3).xz) / length((f2 - spline[3]).xy),
		c3
	);
}

float4x4 offset_spline_vertical(float4x4 spline, float y) {
	return float4x4(
		spline[0] + float4(0,y,0,0),
		spline[1] + float4(0,y,0,0),
		spline[2] + float4(0,y,0,0),
		spline[3] + float4(0,y,0,0)
	);
}

BezierVertex VS_road(float4 v : POSITION_IN) {
	BezierVertex	c;
	c.v		= v;
	return c;
}

// patch Constant Function
PatchConsts HS_solidroadC(InputPatch<BezierVertex, 4> patch) {
	float	w	= patch[0].v.w;
	float	t	= 64;//1 / w;

    PatchConsts	consts;

	consts.edge[0]	= consts.edge[2] = consts.inside[1] = 2;
	consts.edge[1]	= consts.edge[3] = consts.inside[0] = t;

	float4x4	spline	= float4x4(patch[0].v, patch[1].v, patch[2].v, patch[3].v);
	float4x4	c0	= offset_spline(spline, -1.5);
	float4x4	c1	= offset_spline(spline, -0.5);
	float4x4	c2	= offset_spline(spline, +0.5);
	float4x4	c3	= offset_spline(spline, +1.5);

	float4x4	bx	= GetColumns(c0, c1, c2, c3, 0);
	float4x4	by	= GetColumns(c0, c1, c2, c3, 1);
	float4x4	bz	= GetColumns(c0, c1, c2, c3, 2);
	float4x4	bw	= GetColumns(c0, c1, c2, c3, 3);

	consts.blend[0] = mul(mul(bezier_blend, bx), bezier_blend);
	consts.blend[1] = mul(mul(bezier_blend, by), bezier_blend);
	consts.blend[2] = mul(mul(bezier_blend, bz), bezier_blend);
	consts.blend[3] = mul(mul(bezier_blend, bw), bezier_blend);

	return consts;
}

// hull shader
HULL_SHADER(HS_solidroadC, quad, fractional_even, triangle_ccw, 16)
BezierVertex HS_solidroad(InputPatch<BezierVertex, 4> patch, uint i : OUTPUT_CONTROL_POINT_ID) {
	return patch[i];
}

// domain shader
DOMAIN_SHADER(quad) 
S_TransformCol DS_solidroad(PatchConsts consts, float2 t : DOMAIN_LOCATION, const OutputPatch<BezierVertex, 16> unused) {
	float4		u = cubic_param(t.x);
	float4x4	c = float4x4(
		mul(u, consts.blend[0]),
		mul(u, consts.blend[1]),
		mul(u, consts.blend[2]),
		mul(u, consts.blend[3])
	);

	S_TransformCol	p;
	float3	pos	= mul(c, cubic_param(t.y));
	p.pos		= mul(float4(pos,1), viewProj);
	p.colour	= diffuse_colour;
	return p;
}

float4 PS_solidroad(S_TransformCol v): OUTPUT0 {
	return v.colour;
}

technique solidroad {
    pass p0 {
		SET_VS(VS_road);
		SET_HS(HS_solidroad);
		SET_DS(DS_solidroad);
		SET_PS(PS_solidroad);
	}
}

// patch Constant Function
RoadConsts HS_roadC(InputPatch<BezierVertex, 4> patch) {
	float	w	= patch[0].v.w;
	float	t	= 64;//1 / w;

    RoadConsts	consts;

	xsect_buffer.GetDimensions(consts.xsect_num);
	consts.min_max_x	= float2(xsect_buffer[0].x, xsect_buffer[consts.xsect_num - 1].x);

	consts.edge[0]	= consts.edge[2] = consts.inside[1] = consts.xsect_num - 1;
	consts.edge[1]	= consts.edge[3] = consts.inside[0] = t;

	float4x4	spline	= float4x4(patch[0].v, patch[1].v, patch[2].v, patch[3].v);

	float4x4	c0	= offset_spline(spline, consts.min_max_x.x);
	float4x4	c1	= offset_spline(spline, lerp(consts.min_max_x.x, consts.min_max_x.y, 1 / 3.0));
	float4x4	c2	= offset_spline(spline, lerp(consts.min_max_x.x, consts.min_max_x.y, 2 / 3.0));
	float4x4	c3	= offset_spline(spline, consts.min_max_x.y);

	float4x4	bx	= GetColumns(c0, c1, c2, c3, 0);
	float4x4	by	= GetColumns(c0, c1, c2, c3, 1);
	float4x4	bz	= GetColumns(c0, c1, c2, c3, 2);
	float4x4	bw	= GetColumns(c0, c1, c2, c3, 3);

	consts.blend[0] = mul(mul(bezier_blend, bx), bezier_blend);
	consts.blend[1] = mul(mul(bezier_blend, by), bezier_blend);
	consts.blend[2] = mul(mul(bezier_blend, bz), bezier_blend);
	consts.blend[3] = mul(mul(bezier_blend, bw), bezier_blend);

	return consts;
}

// hull shader
HULL_SHADER(HS_roadC, quad, fractional_even, triangle_ccw, 4)
BezierVertex HS_road(InputPatch<BezierVertex, 4> patch, uint i : OUTPUT_CONTROL_POINT_ID) {
	return patch[i];
}

// domain shader
DOMAIN_SHADER(quad) 
S_LightingTex DS_road(RoadConsts consts, float2 t : DOMAIN_LOCATION, const OutputPatch<BezierVertex, 4> unused) {
	float4		xsect = xsect_buffer[uint(t.y * (consts.xsect_num - 1) + .5)];

	float4		u = cubic_param(t.x);
	float4x4	c = float4x4(
		mul(u, offset_spline_vertical(consts.blend[0], xsect.y)),
		mul(u, offset_spline_vertical(consts.blend[1], xsect.y)),
		mul(u, offset_spline_vertical(consts.blend[2], xsect.y)),
		mul(u, offset_spline_vertical(consts.blend[3], xsect.y))
	);

	S_LightingTex	v;
	float3	pos		= mul(c, cubic_param((xsect.x - consts.min_max_x.x) / (consts.min_max_x.y - consts.min_max_x.x)));
	v.position		= mul(float4(pos, 1.0), viewProj);
	v.uv			= float2(xsect.z, t.x * 10);
	v.normal		= float3(0, 0, 1);
	v.worldpos		= pos;
	v.ambient		= light_ambient;//DiffuseLight(pos, norm);
	v.fog			= VSFog(pos);
	SHADOW_VS(v,pos)
	return v;
}

float4 PS_road(S_LightingTex v) : OUTPUT0 {
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

technique road {
    pass p0 {
		SET_VS(VS_road);
		SET_HS(HS_road);
		SET_DS(DS_road);
		SET_PS(PS_road);
	}
}


