#include "common.fxh"
#include "perlin_noise.fxh"

//-----------------------------------------------------------------------------
//	uniform variables
//-----------------------------------------------------------------------------

sampler_def2D(diffuse_samp,
	FILTER_MIN_MAG_MIP_LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

#ifdef PLAT_PS3
sampler_def2D(msaa2to1_samp,
	MIPFILTER = POINT;
	MINFILTER = QUINCUNX;
	MAGFILTER = LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
#else
sampler_def2D(msaa2to1_samp,
	FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
#endif

sampler_def2D(msaa4to1_samp,
	FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

sampler_def2D(jitter_samp,
	FILTER_MIN_MAG_MIP_POINT;
	ADDRESSU = WRAP;
	ADDRESSV = WRAP;
);

#ifdef PLAT_PS3
sampler_def2D(dof_samp,
	MIPFILTER = POINT;
	MINFILTER = GAUSSIANQUAD;
	MAGFILTER = GAUSSIANQUAD;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
#else
sampler_def2D(dof_samp,
	FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
#endif

sampler_def2D(bloom_samp,
	FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

sampler_defCUBE(cube_samp,
	FILTER_MIN_MAG_MIP_LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

#ifdef USE_DX11
Texture2D yuv_samp_t[3];  SamplerState yuv_samp_s[3];
static const sampler2D yuv_samp[3]  = {
	{yuv_samp_t[0], yuv_samp_s[0]},
	{yuv_samp_t[1], yuv_samp_s[1]},
	{yuv_samp_t[2], yuv_samp_s[2]}
};
#else
sampler2D	yuv_samp[3];
#endif

#if defined PLAT_PS4 || defined USE_DX11
Texture2D	yuv_tex;
#else
sampler2D	yuv_tex;
#endif

float4		fill_colour	= float4(0,0,0,1);
float		z_depth		= 1;
Matrix		colour_matrix;
float3 		filter_samples[8];
float		bilateral_factor;
float		face;
Matrix		uv_matrix;

//DOF
float4		dof_params;// focus_depth, focus_range, near_sharpness, far_sharpness;
float4		gaussian_offset;

//MOTION BLUR
Matrix		velocity_matrix;
float4		velocity_matrix_z;
float4		jitter_uv;
float		velocity_scale;
float4		uv_thresh;
float4		uv_offset;

//-----------------------------------------------------------------------------
//	vertex shaders
//-----------------------------------------------------------------------------

float4 VS_Pos2D(float2 position:POSITION) : POSITION_OUT {
	return platform_fix(float3(position, z_depth));
}

struct S_Colour {
	float4	pos		: POSITION_OUT;
	float4	col		: COLOR0;
};
S_Colour VS_Colour(float2 position:POSITION, in float4 col:COLOR0) {
	S_Colour	v;
	v.col = col;
	v.pos = platform_fix(float3(position, 0));
	return v;
}
struct S_PassThrough {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
};
struct S_PassThrough3 {
	float4	pos		: POSITION_OUT;
	float2	uv0		: TEXCOORD0;
	float2	uv1		: TEXCOORD1;
	float2	uv2		: TEXCOORD2;
};

float2 map_transform(float2 uv, float4x4 mat) {
	return (project(mul(float4(uv * 2 - 1, 0, 1), mat)).xy + 1) * 0.5;
}

float2 map_transform0(float2 uv, float4x4 mat) {
	return project(mul(float4(uv, 0, 1), mat)).xy;
}

S_PassThrough VS_PassThrough(float2 position:POSITION, in float2 uv:TEXCOORD0) {
	S_PassThrough	v;
	v.uv	= uv;
	v.pos	= platform_fix(float3(position, 0));
	return v;
}
S_PassThrough VS_UVTransform(float2 position:POSITION, in float2 uv:TEXCOORD0) {
	S_PassThrough	v;
	v.uv	= map_transform(uv, uv_matrix);
	v.pos	= platform_fix(float3(position, 0));
	return v;
}
S_PassThrough3 VS_UVTransform3(float2 position:POSITION, in float2 uv:TEXCOORD0) {
	S_PassThrough3	v;
	v.uv0	= map_transform0(uv, uv_matrix);
	v.uv1	= map_transform0(uv * float2(0.5, 0.5) + float2(0,   1), uv_matrix);
	v.uv2	= map_transform0(uv * float2(0.5, 0.5) + float2(0.5, 1), uv_matrix);
	v.pos	= platform_fix(float3(position, 0));
	return v;
}

S_PassThrough VS_Transform(float2 position:POSITION, in float2 uv:TEXCOORD0) {
	S_PassThrough	v;
	v.uv	= uv;
	v.pos	= mul(float4(position, 0, 1), worldViewProj);
	return v;
}

struct S_MotionBlur {
	float4	pos		: POSITION_OUT;
	float2	uv		: TEXCOORD0;
	float4	old_pos	: TEXCOORD1;
};
S_MotionBlur VS_MotionBlur(in float2 position:POSITION, in float2 uv:TEXCOORD0) {
	S_MotionBlur	v;
	v.uv		= uv;
	v.old_pos	= uv.x * velocity_matrix[0] + uv.y * velocity_matrix[1] + velocity_matrix[3];
	v.pos		= platform_fix(float3(position, 0));
	return v;
}

//-----------------------------------------------------------------------------
//	pixel shaders
//-----------------------------------------------------------------------------

float4 PS_Fill() : OUTPUT0 {
	return fill_colour;
}

float4 PS_Colour(S_Colour v) : OUTPUT0 {
	return v.col;
}

float4 ColourFilter(float3 diffuse) {
	return mul(float4(diffuse, 1), colour_matrix);
}

float4 PS_Msaa2to1Copy(S_PassThrough v) : OUTPUT0 {
	return tex2D(msaa2to1_samp, v.uv + float2( 0.0001, 0.0));
}

float4 PS_Msaa4to1Copy(S_PassThrough v) : OUTPUT0 {
	return tex2D(msaa4to1_samp, v.uv);
}

float4 ExpAlpha(float4 c, float p) {
	return float4(pow(p, c.a * 255 - 128) * c.rgb, 1.0);
}
float4 PS_CubemapFaceExp(S_PassThrough v) : OUTPUT0 {
#ifdef PLAT_X360
	float3	t = float3(v.uv + 1, face);
	float4	z;
	asm {
		tfetchCube z, t, cube_samp, MinFilter=point, MagFilter=point, UseComputedLOD=false
	};
	return ExpAlpha(z, 1.03);
#else
	float3	t = float3(v.uv * 2 - 1, 1);// - frac(face/2) * 4);
	if (face == 0)
		t = float3(1, -t.y, -t.x);
	else if (face == 1)
		t = float3(-1, -t.y, t.x);
	else if (face == 2)
		t = float3(t.x, 1, t.y);
	else if (face == 3)
		t = float3(t.x, -1, -t.y);
	else if (face == 4)
		t = float3(t.x, -t.y, 1);
	else if (face == 5)
		t = float3(-t.x, -t.y, -1);
	return ExpAlpha(texCUBE(cube_samp, t), 1.03);
#endif
}

float4 PS_CubemapFace(S_PassThrough v) : OUTPUT0 {
#ifdef PLAT_X360
	float3	t = float3(v.uv + 1, face);
	float4	z;
	asm {
		tfetchCube z, t, cube_samp, MinFilter=linear, MagFilter=linear	//, MipFilter=point
	};
	return z;
#else
	float3	t = float3(v.uv * 2 - 1, 1);// - frac(face/2) * 4);
	if (face == 0)
		t = float3(1, -t.y, -t.x);
	else if (face == 1)
		t = float3(-1, -t.y, t.x);
	else if (face == 2)
		t = float3(t.x, 1, t.y);
	else if (face == 3)
		t = float3(t.x, -1, -t.y);
	else if (face == 4)
		t = float3(t.x, -t.y, 1);
	else if (face == 5)
		t = float3(-t.x, -t.y, -1);
	return texCUBE(cube_samp, t);
#endif
}

float4 PS_Tex(S_PassThrough v) : OUTPUT0 {
	return tex2D(_screen, v.uv);
}

float4 PS_Adjust(S_PassThrough v) : OUTPUT0 {
	return ColourFilter(tex2D(_screen, v.uv).rgb);
}

highp float rand(highp float2 uv) {
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
//	return frac(cos(frac(dot(p, float2(23.1406926327792690, 2.6651441426902251))) * 3.1415926535 / 2) * 43758.5453);
}

static const float3x3 yuv_conversion = {
	1.164f,  1.164f, 1.164f,
	  0.0f, -0.392f, 2.017f,
	1.596f, -0.813f,   0.0f
};

float3 yuv2rgb(float3 yuv) {
	return mul(yuv - float3(16.0/255, 0.5, 0.5), yuv_conversion);
}

#ifdef PLAT_PS4
#define GetDimensions	GetDimensionsFast
#endif

//float lumaThreshold;
//float chromaThreshold;

float3 YUV(sampler2D y, sampler2D uv, float2 st) {
	return yuv2rgb(float3(tex2D(y, st).r, tex2D(uv, st).rg));
}

float3 YUV2(sampler2D y, sampler2D uv, float2 st) {
#if defined PLAT_PS4 || defined USE_DX11
	float	width, height;
	y.t.GetDimensions(width, height);
	float	f		= (height - 32) / (height * 2);
#else
	float	f		= 1;
#endif
	float2	st_u	= float2(st.x, st.y * f);
	float2	st_v	= float2(st.x, 1 + (st.y - 1) * f);
	return yuv2rgb(float3(tex2D(y, st).r, tex2D(uv, st_u).r, tex2D(uv, st_v).r));
}

float3 YUV3(sampler2D y, sampler2D u, sampler2D v, float2 st) {
	return yuv2rgb(float3(tex2D(y, st).r, tex2D(u, st).r, tex2D(v, st).r));
}

float3 YUYV(sampler2D y, sampler2D uv, float2 st) {
#if defined PLAT_PS4 || defined USE_DX11
	float	width, height;
	y.t.GetDimensions(width, height);
	int2	sti		= int2(st * float2(width, height));
	float4	yuyv	= y.t[sti];
	return yuv2rgb(sti.x & 1 ? yuyv.xyw : yuyv.zyw);
#else
	float4	yuyv	= tex2D(y, st);
	return yuv2rgb(int(st.x * 640) & 1 ? yuyv.xyw : yuyv.zyw);
#endif
}

float2 camera_smear(float2 uv) {
	uv = lerp(uv, 1 - 0.01 / (uv - 0.8), step(0.9, uv));
	return lerp(0.01 / (0.2 - uv), uv, step(0.1, uv));
}

float4 PS_YUV(S_PassThrough v) : OUTPUT0 {
	return float4(YUV(yuv_samp[0], yuv_samp[1], camera_smear(v.uv)), 1);
}

float4 PS_YUV2(S_PassThrough v) : OUTPUT0 {
#if defined PLAT_PS4 || defined USE_DX11
	float	width, height;
	yuv_samp[1].t.GetDimensionsFast(width, height);
	float	f		= (height - 32) / (height * 2);
#else
	float	f		= 1;
#endif
	float2	uv_u	= float2(v.uv.x, v.uv.y * f);
	float2	uv_v	= float2(v.uv.x, 1 + (v.uv.y - 1) * f);

	float3	yuv = float3(
		(tex2D(yuv_samp[0], v.uv).r - 16.0 / 255.0),
		tex2D(yuv_samp[1], uv_u).r - 0.5,
		tex2D(yuv_samp[1], uv_v).r - 0.5
	);
	return float4(YUV2(yuv_samp[0], yuv_samp[1], v.uv), 1);
}

float4 PS_YUV3(S_PassThrough3 v) : OUTPUT0 {
#if defined PLAT_PS4 || defined USE_DX11
	return float4(yuv2rgb(float3(
		yuv_tex[v.uv0].r,
		yuv_tex[v.uv1].r,
		yuv_tex[v.uv2].r
	)), 1);
#else
	return float4(YUV3(yuv_samp[0], yuv_samp[1], yuv_samp[2], v.uv0), 1);
#endif
}

float4 PS_YUYV(S_PassThrough v) : OUTPUT0 {
	float	alpha	= max(1 - max(max_component(abs(v.uv - 0.5)) -.45, 0) * 20, 0);
	return float4(YUYV(yuv_samp[0], yuv_samp[1], v.uv) * alpha, 1);
}

float2 DirToTexel360(float3 d) {
	return float2(atan2(d.x, d.y) / (2 * pi) + 0.5, acos(d.z / length(d)) / pi);
//	return d.xy + 0.5;
}

float4 PS_YUV_360(S_PassThrough v) : OUTPUT0 {
	float4	d = mul(float4(v.uv, 1, 1), iviewProj);
	return float4(YUV(yuv_samp[0], yuv_samp[1], DirToTexel360(d.xyz)), 1);
}

float4 PS_YUV2_360(S_PassThrough v) : OUTPUT0 {
	float4	d = mul(float4(v.uv, 1, 1), iviewProj);
	return float4(YUV2(yuv_samp[0], yuv_samp[1], DirToTexel360(d.xyz)), 1);
}

float4 PS_YUV3_360(S_PassThrough v) : OUTPUT0 {
	float4	d = mul(float4(v.uv, 1, 1), iviewProj);
	return float4(YUV3(yuv_samp[0], yuv_samp[1], yuv_samp[2], DirToTexel360(d.xyz)), 1);
}

float4 PS_LIDAR(S_PassThrough v) : OUTPUT0 {
#if defined PLAT_PS4 || defined USE_DX11
	return 0;
#else
	return float4(tex2D(yuv_tex, camera_smear(v.uv)).rgb / 5, 1);
#endif
}

float4 PS_DepthMip(S_PassThrough v, out float depth : OUTPUT_DEPTH) : OUTPUT0 {
#ifdef PLAT_GLSL
	depth = 0;
#else
	depth = tex2D(_zbuffer, v.uv).x;
#endif
	return depth;
}

float4 PS_BrightPass(S_PassThrough v) : OUTPUT0 {
	float4	p = hdr(tex2D(_screen, v.uv));
//	float4	p = hdr(PointSample(_screen, uv));
	p = max(p - 1, 0);
	return p / (p + 0.66f);
}

float4 MotionBlur2(float2 uv, float4 old_pos, float4 diffuse, float depth) {
	float jitter	= tex2D(jitter_samp, uv * jitter_uv.xy + jitter_uv.zw).g;
	float2 vel		= (project(old_pos + depth * velocity_matrix_z).xy - uv) * velocity_scale * diffuse.a;
	float2 uv2		= uv + vel * (jitter - 1);
	float4 uv01		= float4(uv2, uv2 + vel);
	float4 uv_diff01= (uv01 - uv.xyxy) * uv_thresh;

	float4 sample0	= tex2D(_screen, uv01.xy);
	float4 sample1	= tex2D(_screen, uv01.zw);
	sample0.a *= len2(uv_diff01.xz);
	sample1.a *= len2(uv_diff01.yw);

	diffuse.a	= 1;
	diffuse.rgb += sample0.rgb * sample0.a + sample1.rgb * sample1.a;
	diffuse.rgb /= diffuse.a + sample0.a + sample1.a;
	return diffuse;
}
float4 MotionBlur4(float2 uv, float4 old_pos, float4 diffuse, float depth) {
	float jitter	= tex2D(jitter_samp, uv * jitter_uv.xy + jitter_uv.zw).g;
	float2 vel		= (project(old_pos + depth * velocity_matrix_z).xy - uv) * velocity_scale * diffuse.a;
	float2 uv2		= uv + vel * (jitter - 2);
	float4 uv01		= float4(uv2, uv2 + vel);
	float4 uv23		= uv01 + vel.xyxy * 2;

	float4 sample0	= tex2D(_screen, uv01.xy);
	float4 sample1	= tex2D(_screen, uv01.zw);
	float4 sample2	= tex2D(_screen, uv23.xy);
	float4 sample3	= tex2D(_screen, uv23.zw);

	float4 uv_diff01 = (uv01 - uv.xyxy) * uv_thresh;
	float4 uv_diff23 = (uv23 - uv.xyxy) * uv_thresh;

	sample0.a *= len2(uv_diff01.xz);
	sample1.a *= len2(uv_diff01.yw);
	sample2.a *= len2(uv_diff23.xz);
	sample3.a *= len2(uv_diff23.yw);

	diffuse.a = 1;
	diffuse.rgb += sample0.rgb * sample0.a + sample1.rgb * sample1.a + sample2.rgb * sample2.a + sample3.rgb * sample3.a;
	diffuse.rgb /= diffuse.a + sample0.a + sample1.a + sample2.a + sample3.a;
	return diffuse;
}

float4 Bloom(float2 uv) {
	return tex2D(bloom_samp, uv);
}

float4 Dof(float2 uv, float depth) {
	float z				= ZBToZ(depth);
	float blurriness	= saturate(max(z - dof_params.x, 0) / dof_params.y);
	float sharpness		= lerp(dof_params.z, dof_params.w, blurriness);

#ifdef PLAT_PS3
	float3 gaussian		= tex2D(dof_samp, uv).rgb;
	sharpness			= min(0, sharpness); // don't make it any sharper on PS3 or it will heighten aliasing
#else
	float4	uv4			= saturate(gaussian_offset + float4(uv, uv));
	float3 gaussian		= (
		tex2D(dof_samp, uv4.xy).rgb
	+	tex2D(dof_samp, uv4.zw).rgb
	) * .5;
#endif
	return float4(gaussian.rgb, 1) * -sharpness;
}

float4 ScreenSample(float2 uv) {
	return PointSample(_screen, uv);
}

float4 PS_FinalComposition(S_PassThrough v) : OUTPUT0 {
	float4	diffuse	= ScreenSample(v.uv);
#ifndef PLAT_GLSL
	diffuse	+= Bloom(v.uv);
#endif
	return ColourFilter(hdr(diffuse).rgb);
}

float4 PS_FinalComposition_Dof(S_PassThrough v) : OUTPUT0 {
	float4	diffuse	= ScreenSample(v.uv);
	float	depth	= GetDepth(_zbuffer, v.uv);
	diffuse.rgb		= PremultBlend(diffuse, Dof(v.uv, depth)).rgb;
	diffuse			+= Bloom(v.uv);
	return ColourFilter(hdr(diffuse).rgb);
}

float4 PS_FinalComposition_MB2(S_MotionBlur v) : OUTPUT0 {
	float4	diffuse	= ScreenSample(v.uv);
	float	depth	= GetDepth(_zbuffer, v.uv);
	diffuse			= MotionBlur2(v.uv, v.old_pos, diffuse, depth) + Bloom(v.uv);
	return ColourFilter(hdr(diffuse).rgb);
}

float4 PS_FinalComposition_Dof_MB2(S_MotionBlur v) : OUTPUT0 {
	float4	diffuse	= ScreenSample(v.uv);
	float	depth	= GetDepth(_zbuffer, v.uv);
	diffuse.rgb		= PremultBlend(diffuse, Dof(v.uv, depth)).rgb;
	diffuse			= MotionBlur2(v.uv, v.old_pos, diffuse, depth) + Bloom(v.uv);
	return ColourFilter(hdr(diffuse).rgb);
}

float4 PS_FinalComposition_MB4(S_MotionBlur v) : OUTPUT0 {
	float4	diffuse	= ScreenSample(v.uv);
	float	depth	= GetDepth(_zbuffer, v.uv);
	diffuse			= MotionBlur4(v.uv, v.old_pos, diffuse, depth) + Bloom(v.uv);
	return ColourFilter(hdr(diffuse).rgb);
}

float4 PS_FinalComposition_Dof_MB4(S_MotionBlur v) : OUTPUT0 {
	float4	diffuse	= ScreenSample(v.uv);
	float	depth	= GetDepth(_zbuffer, v.uv);
	diffuse.rgb		= PremultBlend(diffuse, Dof(v.uv, depth)).rgb;
	diffuse			= MotionBlur4(v.uv, v.old_pos, diffuse, depth) + Bloom(v.uv);
	return ColourFilter(hdr(diffuse).rgb);
}

//-----------------------------------------------------------------------------
//	end of composition
//-----------------------------------------------------------------------------

float4 PS_ZCopy(S_PassThrough v, out float depth:OUTPUT_DEPTH) : OUTPUT0 {
	depth = GetDepth(_zbuffer, v.uv);
	return float4(0,0,0,0);
}

float4 PS_ZDownres(S_PassThrough v, out float depth:OUTPUT_DEPTH) : OUTPUT0 {
	float4	uv2		= float4(v.uv, v.uv) + uv_offset;
	float4 depths	= float4(
		GetDepth(_zbuffer, uv2.xy),
		GetDepth(_zbuffer, uv2.xw),
		GetDepth(_zbuffer, uv2.zy),
		GetDepth(_zbuffer, uv2.zw)
	);
	depths.xy	= nearest(depths.xy, depths.zw);
	depth		= nearest(depths.x, depths.y);
	return float4(0,0,0,0);
}

float4 PS_Downsample4x4(S_PassThrough v) : OUTPUT0 {
	return (PointSample(_screen, v.uv, -1, -1)
		+	PointSample(_screen, v.uv, +1, -1)
		+	PointSample(_screen, v.uv, -1, +1)
		+	PointSample(_screen, v.uv, +1, +1)
		) / 4;
}

//-----------------------------------------------------------------------------
//	Convolve
//-----------------------------------------------------------------------------

float4 Sample2(sampler2D t, float2 uv, float3 s) { return (tex2D(t, uv + s.xy) + tex2D(t, uv - s.xy)) * s.z; }

float4 PS_Convolve0(S_PassThrough v) : OUTPUT0 {
	return tex2D(_screen, v.uv) * filter_samples[0].z * 2;
}
float4 PS_Convolve1(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0]);
}
float4 PS_Convolve2(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0])
		+	Sample2(_screen, v.uv, filter_samples[1]);
}
float4 PS_Convolve3(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0])
		+	Sample2(_screen, v.uv, filter_samples[1])
		+	Sample2(_screen, v.uv, filter_samples[2]);
}
float4 PS_Convolve4(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0])
		+	Sample2(_screen, v.uv, filter_samples[1])
		+	Sample2(_screen, v.uv, filter_samples[2])
		+	Sample2(_screen, v.uv, filter_samples[3]);
}
float4 PS_Convolve5(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0])
		+	Sample2(_screen, v.uv, filter_samples[1])
		+	Sample2(_screen, v.uv, filter_samples[2])
		+	Sample2(_screen, v.uv, filter_samples[3])
		+	Sample2(_screen, v.uv, filter_samples[4]);
}
float4 PS_Convolve6(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0])
		+	Sample2(_screen, v.uv, filter_samples[1])
		+	Sample2(_screen, v.uv, filter_samples[2])
		+	Sample2(_screen, v.uv, filter_samples[3])
		+	Sample2(_screen, v.uv, filter_samples[4])
		+	Sample2(_screen, v.uv, filter_samples[5]);
}
float4 PS_Convolve7(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0])
		+	Sample2(_screen, v.uv, filter_samples[1])
		+	Sample2(_screen, v.uv, filter_samples[2])
		+	Sample2(_screen, v.uv, filter_samples[3])
		+	Sample2(_screen, v.uv, filter_samples[4])
		+	Sample2(_screen, v.uv, filter_samples[5])
		+	Sample2(_screen, v.uv, filter_samples[6]);
}
float4 PS_Convolve8(S_PassThrough v) : OUTPUT0 {
	return	Sample2(_screen, v.uv, filter_samples[0])
		+	Sample2(_screen, v.uv, filter_samples[1])
		+	Sample2(_screen, v.uv, filter_samples[2])
		+	Sample2(_screen, v.uv, filter_samples[3])
		+	Sample2(_screen, v.uv, filter_samples[4])
		+	Sample2(_screen, v.uv, filter_samples[5])
		+	Sample2(_screen, v.uv, filter_samples[6])
		+	Sample2(_screen, v.uv, filter_samples[7]);
}

//-----------------------------------------------------------------------------
//	Bilateral Filter
//-----------------------------------------------------------------------------

float4 Bilateral2(sampler2D t, float2 uv, float3 s, float4 centre, inout float wsum) {
	float4	t0	= tex2D(t, uv - s.xy);
	float4	t1	= tex2D(t, uv + s.xy);
    float2	w	= exp2(bilateral_factor * float2(len2(t0 - centre), len2(t1 - centre))) * s.z;
	wsum += w.x + w.y;
	return t0 * w.x + t1 * w.y;
}

float4 PS_Bilateral0(S_PassThrough v) : OUTPUT0 {
	return tex2D(_screen, v.uv);
}
float4 PS_Bilateral1(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+ Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum);
	return total / wsum;
}
float4 PS_Bilateral2(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+	Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[1], centre, wsum);
	return total / wsum;
}
float4 PS_Bilateral3(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+	Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[1], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[2], centre, wsum);
	return total / wsum;
}
float4 PS_Bilateral4(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+	Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[1], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[2], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[3], centre, wsum);
	return total / wsum;
}
float4 PS_Bilateral5(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+	Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[1], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[2], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[3], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[4], centre, wsum);
	return total / wsum;
}
float4 PS_Bilateral6(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+	Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[1], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[2], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[3], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[4], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[5], centre, wsum);
	return total / wsum;
}
float4 PS_Bilateral7(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+	Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[1], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[2], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[3], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[4], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[5], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[6], centre, wsum);
	return total / wsum;
}
float4 PS_Bilateral8(S_PassThrough v) : OUTPUT0 {
	float	wsum	= 1;
	float4	centre	= tex2D(_screen, v.uv);
	float4	total	= centre
		+	Bilateral2(_screen, v.uv, filter_samples[0], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[1], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[2], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[3], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[4], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[5], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[6], centre, wsum)
		+	Bilateral2(_screen, v.uv, filter_samples[7], centre, wsum);
	return total / wsum;
}

float4 vive_vs(
	float4 position		: POSITION,
	inout float2 UVred	: TEXCOORD0,
	inout float2 UVGreen: TEXCOORD1,
	inout float2 UVblue	: TEXCOORD2
) : POSITION_OUT {
	return position;
}

float4 vive_ps(
	float4 screen	: POSITION_OUT,
	float2 UVred	: TEXCOORD0,
	float2 UVgreen	: TEXCOORD1,
	float2 UVblue	: TEXCOORD2
) : OUTPUT0 {
	return float4(
		tex2D(_screen, UVred).x,
		tex2D(_screen, UVgreen).y,
		tex2D(_screen, UVblue).z,
		1
	);
}

//-----------------------------------------------------------------------------
//	techniques
//-----------------------------------------------------------------------------

technique yuv {
	PASS(p0, VS_UVTransform, PS_YUV)
	PASS(p1, VS_UVTransform, PS_YUV_360)
}

technique fill {
	PASS(p0, VS_Pos2D, PS_Fill)
}

technique colour {
	PASS(p0, VS_Colour, PS_Colour)
}

technique brightpass {
	PASS(p0, VS_PassThrough, PS_BrightPass)
}

technique cubemapface_exp {
	PASS(p0, VS_PassThrough, PS_CubemapFaceExp)
}

technique cubemapface {
	PASS(p0, VS_PassThrough, PS_CubemapFace)
}


technique yuv2 {
	PASS(p0, VS_UVTransform, PS_YUV2)
	PASS(p1, VS_UVTransform, PS_YUV2_360)
}

technique yuv3 {
	PASS(p0, VS_UVTransform3, PS_YUV3)
	PASS(p1, VS_UVTransform, PS_YUV3_360)
}

technique composition {
	PASS(p0,VS_PassThrough,PS_FinalComposition)
	PASS(p1,VS_PassThrough,PS_FinalComposition_Dof)
	PASS(p2,VS_MotionBlur,PS_FinalComposition_MB2)
	PASS(p3,VS_MotionBlur,PS_FinalComposition_Dof_MB2)
	PASS(p4,VS_MotionBlur,PS_FinalComposition_MB4)
	PASS(p5,VS_MotionBlur,PS_FinalComposition_Dof_MB4)
}

#ifndef PLAT_GLSL
technique depth_mip {
	PASS(p0,VS_PassThrough,PS_DepthMip)
}

technique zcopy {
	PASS(p0,VS_PassThrough,PS_ZCopy)
}

technique zdownres {
	PASS(p0,VS_PassThrough,PS_ZDownres)
}
#endif

technique downsample4x4 {
	PASS(p0,VS_PassThrough,PS_Downsample4x4)
}

technique copy {
	PASS(p0,VS_PassThrough,PS_Tex)
}

technique adjust {
	PASS(p0,VS_PassThrough,PS_Adjust)
}

technique convolve {
	PASS(p0,VS_PassThrough,PS_Convolve0)
	PASS(p1,VS_PassThrough,PS_Convolve1)
	PASS(p2,VS_PassThrough,PS_Convolve2)
	PASS(p3,VS_PassThrough,PS_Convolve3)
	PASS(p4,VS_PassThrough,PS_Convolve4)
	PASS(p5,VS_PassThrough,PS_Convolve5)
	PASS(p6,VS_PassThrough,PS_Convolve6)
	PASS(p7,VS_PassThrough,PS_Convolve7)
	PASS(p8,VS_PassThrough,PS_Convolve8)
}

technique bilateral {
	PASS(p0,VS_PassThrough,PS_Bilateral0)
	PASS(p1,VS_PassThrough,PS_Bilateral1)
	PASS(p2,VS_PassThrough,PS_Bilateral2)
	PASS(p3,VS_PassThrough,PS_Bilateral3)
	PASS(p4,VS_PassThrough,PS_Bilateral4)
	PASS(p5,VS_PassThrough,PS_Bilateral5)
	PASS(p6,VS_PassThrough,PS_Bilateral6)
	PASS(p7,VS_PassThrough,PS_Bilateral7)
	PASS(p8,VS_PassThrough,PS_Bilateral8)
}

technique msaa2to1copy {
	PASS(p0,VS_PassThrough,PS_Msaa2to1Copy)
}

technique msaa4to1copy {
	PASS(p0,VS_PassThrough,PS_Msaa4to1Copy)
}

technique vive_distort {
	pass p0 {
		SET_VS(vive_vs);
		SET_PS(vive_ps);
	}
}

technique lidar {
	PASS(p0, VS_UVTransform, PS_LIDAR)
}

