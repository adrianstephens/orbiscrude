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

struct S_TransformColourTexUnnormGeom {
	float4	pos		: POSITION_IN;
	float2	size	: POSITION1;
	float4	uv		: TEXCOORD0;
	float4	colour	: COLOR0;
};

S_TransformColourTexUnnormGeom GVS_TransformColourTexUnnorm(S_TransformColourTexUnnormGeom v) {
	v.pos		= ui_pos(v.pos);
	v.size		= mul(float4(v.size, 0, 0), worldViewProj);
	v.uv		= float4(UnnormaliseUV(font_size.xy, v.uv.xy), UnnormaliseUV(font_size.xy, v.uv.zw));
	return v;
}

GEOM_SHADER(4)
void GS_TransformColourTexUnnorm(point S_TransformColourTexUnnormGeom v[1], inout TriangleStream<S_TransformColourTexUnnorm> tris) {
	S_TransformColourTexUnnorm	p;
	float4		pos		= v[0].pos;
	float2		size	= v[0].size;
	float4		uv		= v[0].uv;
	
	p.colour	= v[0].colour;
	
	p.pos		= pos;
	p.uv		= uv.xy;
	tris.Append(p);
	
	p.pos		= pos + float4(size.x, 0, 0, 0);
	p.uv		= uv.zy;
	tris.Append(p);
	
	p.pos		= pos + float4(0, size.y, 0, 0);
	p.uv		= uv.xw;
	tris.Append(p);
	
	p.pos		= pos + float4(size.xy, 0, 0);
	p.uv		= uv.zw;
	tris.Append(p);
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
	PASS_G(p2,GVS_TransformColourTexUnnorm,PS_FontDistGlow,GS_TransformColourTexUnnorm)
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

//-----------------------------------------------------------------------------
//	slug-like font rendering
//-----------------------------------------------------------------------------

Buffer<uint2>	band_buffer		: t0;
Buffer<uint>	indices_buffer	: t1;
Buffer<float2>	curve_buffer	: t2;
Buffer<float>	rational_curves	: t2;
Texture2D		palette			: t3;
SamplerState	slug_sampler {
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};
static const float epsilon = 0.01;


int2 SlugRootCode(float y0, float y1, float y2) {
	int a = asint(y0);
	int b = asint(y1);
	int c = asint(y2);
	return int2(~a & (b | c) | (~b & c), a & (~b | ~c) | (b & ~c));
}

float saturate1(float x) { return clamp(x + 0.5, 0, 1); }

//-------------------------------------
// stroke
//-------------------------------------

static const float2 gauss_quad4[] = {
	float2(0.6521451548625461,	0.3399810435848563),
	float2(0.3478548451374538,	0.8611363115940526),
};
static const float2 gauss_quad8[] = {
	float2(0.3626837833783620,	0.1834346424956498),
	float2(0.3137066458778873,	0.5255324099163290),
	float2(0.2223810344533745,	0.7966664774136267),
	float2(0.1012285362903763,	0.9602898564975363),
};
static const float2 gauss_quad16[] = {
	float2(0.1894506104550685,	0.0950125098376374),
	float2(0.1826034150449236,	0.2816035507792589),
	float2(0.1691565193950025,	0.4580167776572274),
	float2(0.1495959888165767,	0.6178762444026438),
	float2(0.1246289712555339,	0.7554044083550030),
	float2(0.0951585116824928,	0.8656312023878318),
	float2(0.0622535239386479,	0.9445750230732326),
	float2(0.0271524594117541,	0.9894009349916499),
};

float bezier_length(float2 p0, float2 p1, float2 p2, float te) {
	float2	c0	= 2 * (p1 - p0);
	float2	c1	= 2 * (p0 - 2 * p1 + p2);
	
	float	sum = 0;
	
    for (int i = 0; i < 2; i++) {
		float	t1 = (1 - gauss_quad4[i].y) * te / 2;
		float	t2 = (1 + gauss_quad4[i].y) * te / 2;
		sum += gauss_quad4[i].x * (length(c0 + c1 * t1) + length(c0 + c1 * t2));
    }

    return sum * te / 2;
}

float line_length(float2 p0, float2 p2, float te) {
	return length(p2 - p0) * te;
}

float is_straight(float2 p0, float2 p1, float2 p2) {
	float2	b = p0 - p1;
	float2	a = b - (p1 - p2);
	return any(abs(a) < epsilon * abs(b));
}

// from: http://research.microsoft.com/en-us/um/people/hoppe/ravg.pdf

float line_get_param(float2 p0, float2 p2) {
	return saturate(dot(p0, p0 - p2) / len2(p2 - p0));
}
float2 line_evaluate(float2 p0, float2 p2, float t) {
	return lerp(p0, p2, t);
}

float bezier_get_param(float2 p0, float2 p1, float2 p2) {
	float	x = cross2(p0, p2);
    float	y = cross2(p1, p0);
    float	z = cross2(p2, p1);

	float2	s = x * (p2 - p0) + 2 * (y * (p2 - p1) + z * (p1 - p0));
    float	r = (y * z - x * x * 0.25) / len2(s);
    return saturate((0.5 * x + y + r * dot(s, p2 - p1 * 2 + p0)) / (x + y + z));
}

float2 bezier_evaluate(float2 p0, float2 p1, float2 p2, float t) {
	return lerp(lerp(p0, p1, t), lerp(p1, p2, t), t);
}

float3 bezier_stroke(float2 uv, uint num_curves, uint indices_offset, uint curves_offset, float width) {
	float	mind	= width;
	float	bestt	= 0;
	int		besti	= 0;
	for (uint i = 0; i < num_curves; i++) {
		uint	j	= curves_offset + indices_buffer[indices_offset + i] * 2;
		float2	p0	= curve_buffer[j + 0] - uv;
		float2	p1	= curve_buffer[j + 1] - uv;
		float2	p2	= curve_buffer[j + 2] - uv;
		
		bool	straight = is_straight(p0, p1, p2);
		
		float	t	= straight ? line_get_param(p0, p2) : bezier_get_param(p0, p1, p2);
		float2	p	= straight ? line_evaluate(p0, p2, t) : bezier_evaluate(p0, p1, p2, t);
		
		float	d	= length(p);
		if (d < width) {
			float	x = straight ? line_length(p0, p2, t) : bezier_length(p0, p1, p2, t);
			return float3(x, d, i);
			mind	= d;
			bestt	= bezier_length(p0, p1, p2, t);
			besti	= i;
		}
	}
	return float3(bestt, mind, besti);
}

//-------------------------------------
// non-rational
//-------------------------------------

float2 SolvePoly(float2 p0, float2 p1, float2 p2) {
	float2	a = p0 - p1 * 2 + p2;
	float2	b = p0 - p1;
	float	d = sqrt(max(b.y * b.y - a.y * p0.y, 0)); // Clamp discriminant to zero
	
	float2	t = (float2(-d, +d) + b.y) / a.y;
	if (abs(a.y) < epsilon * abs(b.y))
		t = p0.y / (b.y * 2); // Handle linear case where |a| near 0
	
	// Return x coordinates at t.xy
	return (a.x * t - b.x * 2) * t + p0.x;
}

float slug_hcoverage(float2 uv, float pix, uint num_curves, uint indices_offset, uint curves_offset, bool sorted) {
	float	hcoverage	= 0;
	
	for (uint i = 0; i < num_curves; i++) {
		uint	j	= curves_offset + indices_buffer[indices_offset + i] * 2;
		float2	p0	= curve_buffer[j + 0] - uv;
		float2	p1	= curve_buffer[j + 1] - uv;
		float2	p2	= curve_buffer[j + 2] - uv;
		
		if (sorted && max(max(p0.x, p1.x), p2.x) * pix < -0.5)
			break;

		int2	code = SlugRootCode(p0.y, p1.y, p2.y);
		if ((code.x | code.y) < 0) {
			float2 r = SolvePoly(p0, p1, p2) * pix;
			if (code.x < 0)
				hcoverage += saturate1(r.x);
			if (code.y < 0)
				hcoverage -= saturate1(r.y);
		}
	}
	return hcoverage;
}

float slug_vcoverage(float2 uv, float pix, uint num_curves, uint indices_offset, uint curves_offset, bool sorted) {
	float	vcoverage	= 0;
	
	for (uint i = 0; i < num_curves; i++) {
		uint	j	= curves_offset + indices_buffer[indices_offset + i] * 2;
		float2	p0	= curve_buffer[j + 0] - uv;
		float2	p1	= curve_buffer[j + 1] - uv;
		float2	p2	= curve_buffer[j + 2] - uv;
		
		if (sorted && max(max(p0.y, p1.y), p2.y) * pix < -0.5)
			break;
		
		int2	code = SlugRootCode(p2.x, p1.x, p0.x);
		if ((code.x | code.y) < 0) {
			float2 r = SolvePoly(p2.yx, p1.yx, p0.yx) * pix;
			if (code.x < 0)
				vcoverage += saturate1(r.x);
			if (code.y < 0)
				vcoverage -= saturate1(r.y);
		}
	}
	return vcoverage;
}

//-------------------------------------
// rational
//-------------------------------------

float2 SolvePoly(float2 p0, float3 p1, float2 p2) {
	float2	p1w	= p1.xy * p1.z;
	float2	a = p0 - p1w * 2 + p2;
	float2	b = p0 - p1w;
	float	d = sqrt(max(b.y * b.y - a.y * p0.y, 0)); // Clamp discriminant to zero
	
	float2	t = (float2(-d, +d) + b.y) / a.y;
	if (abs(a.y) < epsilon * abs(b.y))
		t = p0.y / (b.y * 2); // Handle linear case where |a| near 0
	
	// Return x coordinates at t.xy
	float2	w	= 1 + 2 * t * (1 - t) * (p1.z - 1);
	return ((a.x * t - b.x * 2) * t + p0.x) / w;
}


float slug_hcoverage_rational(float2 uv, float pix, uint num_curves, uint indices_offset, uint curves_offset, bool sorted) {
	float	hcoverage	= 0;
	
	for (uint i = 0; i < num_curves; i++) {
		uint	j	= curves_offset + indices_buffer[indices_offset + i] * 5;
		float2	p0	= float2(rational_curves[j + 0], rational_curves[j + 1]) - uv;
		float3	p1	= float3(float2(rational_curves[j + 2], rational_curves[j + 3]) - uv, rational_curves[j + 4]);
		float2	p2	= float2(rational_curves[j + 5], rational_curves[j + 6]) - uv;
		
		if (sorted && max(max(p0.x, p1.x), p2.x) * pix < -0.5)
			break;

		int2	code = SlugRootCode(p0.y, p1.y, p2.y);
		if ((code.x | code.y) < 0) {
			float2 r = SolvePoly(p0, p1, p2) * pix;
			if (code.x < 0)
				hcoverage += saturate1(r.x);
			if (code.y < 0)
				hcoverage -= saturate1(r.y);
		}
	}
	return hcoverage;
}

float slug_vcoverage_rational(float2 uv, float pix, uint num_curves, uint indices_offset, uint curves_offset, bool sorted) {
	float	vcoverage	= 0;
	
	for (uint i = 0; i < num_curves; i++) {
		uint	j	= curves_offset + indices_buffer[indices_offset + i] * 5;
		float2	p0	= float2(rational_curves[j + 0], rational_curves[j + 1]) - uv;
		float3	p1	= float3(float2(rational_curves[j + 2], rational_curves[j + 3]) - uv, rational_curves[j + 4]);
		float2	p2	= float2(rational_curves[j + 5], rational_curves[j + 6]) - uv;
		
		if (sorted && max(max(p0.y, p1.y), p2.y) * pix < -0.5)
			break;
		
		int2	code = SlugRootCode(p2.x, p1.x, p0.x);
		if ((code.x | code.y) < 0) {
			float2 r = SolvePoly(p2.yx, p1.yxz, p0.yx) * pix;
			if (code.x < 0)
				vcoverage += saturate1(r.x);
			if (code.y < 0)
				vcoverage -= saturate1(r.y);
		}
	}
	return vcoverage;
}

//-------------------------------------
// banded
//-------------------------------------

float slug_hcoverage(float2 uv, float pix, uint3 offsets) {
	uint2	bands		= band_buffer[offsets.x + uint(floor(uv.y * 8))];
	return slug_hcoverage(uv, pix, bands.y, offsets.y + bands.x, offsets.z, true);
}

float slug_vcoverage(float2 uv, float pix, uint3 offsets) {
	uint2	bands		= band_buffer[offsets.x + 8 + uint(floor(uv.x * 8))];
	return slug_vcoverage(uv, pix, bands.y, offsets.y + bands.x, offsets.z, true);
}

float2 slug_coverage(float2 uv, float2 pix, uint3 offsets) {
	return float2(
		slug_hcoverage(uv, pix.x, offsets),
		slug_vcoverage(uv, pix.y, offsets)
	);
}

float2 slug_coverage_ms(float2 uv, float2 pix, uint3 offsets, uint2 samples) {
	float2	coverage	= 0;
	for (uint i = 0; i < samples.x; i++)
		coverage.x += slug_hcoverage(uv + ddy(uv) * i / samples.x, pix.x, offsets);

	for (uint i = 0; i < samples.y; i++)
		coverage.y += slug_hcoverage(uv + ddx(uv) * i / samples.y, pix.y, offsets);
		
	return coverage / samples;
}


float slug_hcoverage_rational(float2 uv, float pix, uint3 offsets) {
	uint2	bands		= band_buffer[offsets.x + uint(floor(uv.y * 8))];
	return slug_hcoverage_rational(uv, pix, bands.y, offsets.y + bands.x, offsets.z, true);
}

float slug_vcoverage_rational(float2 uv, float pix, uint3 offsets) {
	uint2	bands		= band_buffer[offsets.x + 8 + uint(floor(uv.x * 8))];
	return slug_vcoverage_rational(uv, pix, bands.y, offsets.y + bands.x, offsets.z, true);
}

float2 slug_coverage_rational(float2 uv, float2 pix, uint3 offsets) {
	return float2(
		slug_hcoverage_rational(uv, pix.x, offsets),
		slug_vcoverage_rational(uv, pix.y, offsets)
	);
}

float2 slug_coverage_ms_rational(float2 uv, float2 pix, uint3 offsets, uint2 samples) {
	float2	coverage	= 0;
	for (uint i = 0; i < samples.x; i++)
		coverage.x += slug_hcoverage_rational(uv + ddy(uv) * i / samples.x, pix.x, offsets);

	for (uint j = 0; j < samples.y; j++)
		coverage.y += slug_hcoverage_rational(uv + ddx(uv) * j / samples.y, pix.y, offsets);
		
	return coverage / samples;
}

//-------------------------------------
// gradient
//-------------------------------------

float process_extend(uint mode, float t) {
	switch (mode) {
		case 0:		//NONE
		case 1:		//PAD
			return saturate(t) * 63.0 / 64 + 0.5 / 64;
		case 3:		//MIRROR
			t = frac(t * 0.5);
			return  (t < 0.5 ? t : 1 - t) * 2 * 63.0 / 64 + 0.5 / 64;
		default:	//WRAP
			return frac(t);
	}
}

const static float nan = 0. / 0.;

float process_gradient(uint mode, float2 uv, float2 p) {
	switch (mode) {
		default:
		case 1:		//LINEAR
			return uv.x;
		case 2: {	//SWEEP
			float	a = atan2(uv.y, uv.x);
			if (a < 0)
				a += 2 * pi;
			a *= p.x;
			//return a;
			return a < 0 || a > 1 ? nan : a;
		}
		case 3:	{	//RADIAL	(r1<1 and !swapped)
			float	d = square(uv.x) - square(uv.y);
			if (d < 0)
				return nan;
			float	x = sqrt(d) - uv.x / p.y;
			return x < 0 ? nan : p.x + x;
		}
		case 4:		//RADIAL0
			return (sqrt(dot(uv, uv)) - p.x) * p.y;// / (1 - p.x);

		case 5:	{	//RADIAL1	(r1==1)
			float	x	= dot(uv, uv) / uv.x;
			return x < 0 ? nan : p.x + x;
		}
		case 6:	{	//RADIAL_GT1	(r1>1)
			float	x	= sqrt(dot(uv, uv)) - uv.x / p.y;
			return p.x + x;
		}
		case 7:	{	//RADIAL_LT1_SWAP	(r1<1 and swapped)
			float	d = square(uv.x) - square(uv.y);
			if (d < 0)
				return nan;
			float	x = sqrt(d) + uv.x / p.y;
			return x > 0 ? nan : 1 - p.x + x;
		}
		case 8:	{	//RADIAL_SAME	(r0 == r1)
			float	d = p.x - square(uv.y);
			return d < 0 ? nan : uv.x + sqrt(d);
		}
	}
}

//-------------------------------------
// shaders
//-------------------------------------

float4 debug_colour(float2 v) {
	return float4(abs(v), (v.x < 0 ? 0.25 : 0) + (v.y < 0 ? 0.5 : 0), 1);
}

struct SlugVertex {
	float4	pos			: POSITION_OUT;
	float4	col			: COLOR;	//or gradient uv, mode if col.a == 0
	float2	uv			: TEXCOORD0;
	uint3	offsets		: TEXCOORD1;
	float2	params		: TEXCOORD2;
};

float4 slug_PS(SlugVertex v) : OUTPUT0 {
	float2	per_pix		= float2(ddx(v.uv.x), ddy(v.uv.y));
	float2	pix			= 0.5 / abs(per_pix);

	float3	s		= bezier_stroke(v.uv, v.offsets.x, v.offsets.y, v.offsets.z, 0.01);
//	float	alpha	= smoothstep(0.01, 0.009, s.y);
	
	float	dot		= abs(frac(s.x * 32) - 0.5);
	
	dot	= sqrt(square(dot / 32) + square(s.y));
	float	alpha	= smoothstep(0.01, 0.009, dot);
	
//	float	dot		= smoothstep(0.5, 0.4, abs(frac(s.x * 32) - 0.5));
//	float	dot		= abs(frac(s.x * 32) - 0.5);
//	alpha = alpha * dot;
	
	float3	col		= s.z ? float3(0,1,0) : float3(1,0,0);
	
	return float4(col * alpha, alpha);
	
	return float4(0, 1, 0, 1);

	
	float2	coverage	= float2(
		slug_hcoverage(v.uv, pix.x, v.offsets.x, v.offsets.y, v.offsets.z, false),
		slug_vcoverage(v.uv, pix.y, v.offsets.x, v.offsets.y, v.offsets.z, false)
	);
	float	t = max(coverage.x, coverage.y);
 
 	if (v.col.a > 1) {
		uint	mode	= uint(v.col.a) - 1;
		float	t1		= process_gradient(mode & 15, v.col.rg, v.params);
		if (isnan(t1))
			discard;
		float2	uv		= float2(process_extend(mode >> 4, t1), v.col.b);
		return Blend(palette.Sample(slug_sampler, uv), t);
	} else {
		return Blend(v.col, t);
	}
}

float4 slug_banded_PS(SlugVertex v) : OUTPUT0 {
	float2	per_pix		= float2(ddx(v.uv.x), ddy(v.uv.y));
	
#if 1
	uint2	hband	= band_buffer[v.offsets.x + uint(floor(v.uv.y * 8))];
	float3	s		= bezier_stroke(v.uv, hband.y, v.offsets.y + hband.x, v.offsets.z, 0.01);
	
	uint2	vband		= band_buffer[v.offsets.x + 8 + uint(floor(v.uv.x * 8))];
	float3	s2		= bezier_stroke(v.uv, vband.y, v.offsets.y + vband.x, v.offsets.z, 0.01);
	
	if (s2.y < s.y)
		s = s2;
	
	//return s.y < 0.01 ? float4(1,0,0,1) : float4(0,0,0,0);
	float	dot		= abs(frac(s.x * 32) - 0.5);
	dot	= sqrt(square(dot / 32) + square(s.y));
	float	alpha	= smoothstep(0.01, 0.009, dot);
	return Blend(v.col, alpha);
#endif
	
  #if 1
	float2	pix			= 0.5 / abs(per_pix);
  #else
	float2	pix			= 1 / (abs(per_pix) * 16);
  #endif

  #if 0
	float2	coverage	= slug_coverage_ms(v.uv, pix, v.offsets, uint2(4, 4));
  #else
	float2	coverage	= slug_coverage(v.uv, pix, v.offsets);
  #endif
  
#if 1
	float	t = max(coverage.x, coverage.y);
#elif 1
	float2	d2	= abs(coverage-0.5)*2*1.141;
	float	d	= d2.x * d2.y / length(d2);
//	t = (0.5 - d) * 2;
	float	t	= d;//saturate(1 - d);

#else
  
	float	t = coverage.x * coverage.y;
 #endif
 
 #if 0
//	return Blend(float4(v.col.rgb, v.col.a * min(coverage.x, 0.f) * min(coverage.y, 0.f)));
//	return Blend(float4(v.col.rgb, v.col.a * coverage.x));
//	float	d	= max(abs(per_pix.x), abs(per_pix.y)) * 4;
//	return Distance2(float4(0,0,0,1), float4(1,0,0,1), 0, 1, t, d);
	
	uint	i = uint(floor(t * 8));
	return float4(i & 1, (i >> 1) & 1, (i >> 2) & 1, 1);
#else
	if (v.col.a > 1) {
	#if 0
//		return debug_colour(v.uv-.5);
		return Blend(debug_colour(v.col.rg), 1);
	#elif 0
		uint	mode	= uint(v.col.a) - 1;
		float	t1		= process_gradient(mode & 15, v.col.rg, v.params);
		if (isnan(t1))
			discard;
		//return float4(frac(t1), frac(t1), t1 > 0, t);
		return float4((float3)(t1 * t), t);
	#else
		uint	mode	= uint(v.col.a) - 1;
		float	t1		= process_gradient(mode & 15, v.col.rg, v.params);
		if (isnan(t1))
			discard;
		float2	uv		= float2(process_extend(mode >> 4, t1), v.col.b);
		return Blend(palette.Sample(slug_sampler, uv), t);
	#endif
	} else {
		return Blend(v.col, t);
	}
 #endif
}

SlugVertex slug_VS(float3 position:POSITION_IN, float4 col:COLOR, float2 uv:TEXCOORD0, uint3 offsets : TEXCOORD1) {
	SlugVertex	v;
	v.pos		= mul(float4(position, 1), worldViewProj);
	v.col		= col;
	v.uv		= uv;
	v.offsets	= offsets;
	v.params	= 0;
	return v;
}

technique test_slug {
	PASS(p0,slug_VS,slug_PS)
}

struct SlugVertexGeom {
	float3	pos		: POSITION_IN;
	float4	col		: COLOR;
	float2	size	: TEXCOORD0;
	uint3	offsets	: TEXCOORD1;
};

SlugVertexGeom slug_GVS(SlugVertexGeom v) {
	return v;
}

GEOM_SHADER(4)
void slug_GS(point SlugVertexGeom v[1], inout TriangleStream<SlugVertex> tris) {
	SlugVertex	p;
	float2		expand	= float2(0.1, 0.1);
	float4		pos	= mul(float4(v[0].pos - float3(expand, 0), 1), worldViewProj);
	float4		dx	= worldViewProj[0] * v[0].size.x * (1 + expand.x * 2);
	float4		dy	= worldViewProj[1] * v[0].size.y * (1 + expand.y * 2);
	
	float4		col		= v[0].col;
	float2x2	col_mat	= {
	   0,0,
	   0,0,
	};
	
	if (col.a == 0) {
		uint	mode	= col.b * 255;
		if (mode == 0) {
			//col = palette[col.rg * 255 + 0.5];
			
		} else  {
			//uint	w, h;
			//palette.GetDimensions(w, h);
			
			col.a		= mode + 1;
			//col.b		= ((col.r + col.g * 256) * 255 + 0.5) / h;
			col.b		= (col.r / 256 + col.g) * 255 / 256;
			
			uint	m	= v[0].offsets.z - 4;
			col_mat[0]	= curve_buffer[m + 0];
			col_mat[1]	= curve_buffer[m + 1];
			col.rg		= curve_buffer[m + 2];
			
			p.params	= curve_buffer[m + 3];
			
			if (mode == 1)
				col.g	= col.b;
		}
	}
	
	p.offsets	= v[0].offsets;
	
	p.pos		= pos;
	p.uv		= -expand;
	p.col		= float4(mul(p.uv,col_mat)+ col.xy, col.zw);
	tris.Append(p);
	
	p.pos		= pos + dx;
	p.uv		= float2(1 + expand.x, -expand.y);
	p.col		= float4(mul(p.uv,col_mat)+ col.xy, col.zw);
	tris.Append(p);
	
	p.pos		= pos + dy;
	p.uv		= float2(-expand.x, 1 + expand.y);
	p.col		= float4(mul(p.uv,col_mat)+ col.xy, col.zw);
	tris.Append(p);
	
	p.pos		= pos + dx + dy;
	p.uv		= expand + 1;
	p.col		= float4(mul(p.uv,col_mat)+ col.xy, col.zw);
	tris.Append(p);
}

GEOM_SHADER(4)
void slug_GS2(point SlugVertexGeom v[1], inout TriangleStream<SlugVertex> tris) {
	SlugVertex	p;
	float2		expand	= float2(0.1, 0.1);
	float2		uv	= v[0].pos.xy - expand;
	float2		duv	= v[0].size * (1 + expand * 2);
	
	float4		pos	= mul(float4(uv, v[0].pos.z, 1), worldViewProj);
	float4		dx	= worldViewProj[0] * duv.x;
	float4		dy	= worldViewProj[1] * duv.y;
	
	float4		col		= v[0].col;
	float2x2	col_mat	= {
	   0,0,
	   0,0,
	};
	
	if (col.a == 0) {
		uint	mode	= col.b * 255;
		if (mode == 0) {
			//col = palette[col.rg * 255 + 0.5];
			
		} else  {
			//uint	w, h;
			//palette.GetDimensions(w, h);
			
			col.a		= mode + 1;
			//col.b		= ((col.r + col.g * 256) * 255 + 0.5) / h;
			col.b		= (col.r / 256 + col.g) * 255 / 256;
			
			uint	m	= v[0].offsets.z - 4;
			col_mat[0]	= curve_buffer[m + 0];
			col_mat[1]	= curve_buffer[m + 1];
			col.rg		= curve_buffer[m + 2];
			
			p.params	= curve_buffer[m + 3];
			
			if (mode == 1)
				col.g	= col.b;
		}
	}
	
	p.offsets	= v[0].offsets;
	
	p.pos		= pos;
	p.uv		= uv;
	p.col		= float4(mul(p.uv,col_mat)+ col.xy, col.zw);
	tris.Append(p);
	
	p.pos		= pos + dx;
	p.uv		= uv + float2(duv.x, 0);
	p.col		= float4(mul(p.uv, col_mat)+ col.xy, col.zw);
	tris.Append(p);
	
	p.pos		= pos + dy;
	p.uv		= uv + float2(0, duv.y);
	p.col		= float4(mul(p.uv, col_mat)+ col.xy, col.zw);
	tris.Append(p);
	
	p.pos		= pos + dx + dy;
	p.uv		= uv + duv;
	p.col		= float4(mul(p.uv, col_mat)+ col.xy, col.zw);
	tris.Append(p);
}

technique test_slug_geom {
	pass p0 {
	   SET_VS(slug_GVS);
	   SET_GS(slug_GS2);
	   SET_PS(slug_PS);
	}
}

technique test_slug_banded_geom {
	pass p0 {
	   SET_VS(slug_GVS);
	   SET_GS(slug_GS);
	   SET_PS(slug_banded_PS);
	}
}


