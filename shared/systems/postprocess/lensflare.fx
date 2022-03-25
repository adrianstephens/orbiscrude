#include "common.fxh"

#ifdef PLAT_PS3
//#pragma optionNV(fastprecision off)
//#pragma optionNV(fastmath off)
#endif

sampler_def2D(diffuse_samp,
	FILTER_MIN_MAG_MIP_LINEAR;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);
sampler_def2D(occlusion_samp,
	FILTER_MIN_MAG_MIP_POINT;
	ADDRESSU = CLAMP;
	ADDRESSV = CLAMP;
);

struct vertexLensFlareTestOcclusion {
	float4	position		: POSITION_OUT;
	float3	uv				: TEXCOORD0;
};

struct vertexOutputLensFlare {
	float4	position		: POSITION_OUT;
	float4	uv				: TEXCOORD0;
	float4	colour			: COLOR;
};

vertexLensFlareTestOcclusion VS_LensFlareTestOcclusion(float2 position:POSITION, float3 uv:TEXCOORD0) {
	vertexLensFlareTestOcclusion	v2;
	v2.position	= platform_fix(float3(position, 0));
	v2.uv		= platform_to_map(uv);
#if defined(PLAT_GLSL)
	v2.uv.y		= 1 - v2.uv.y;
#endif
	return v2;
}

vertexOutputLensFlare VS_LensFlare(float2 position:POSITION, float4 uv:TEXCOORD0, float4 colour:COLOR) {
	vertexOutputLensFlare v2;
	v2.position	= platform_fix(float3(position, 1));
	v2.uv		= uv;
	v2.colour	= colour;
	return v2;
}

float4 PS_LensFlareTestOcclusion(vertexLensFlareTestOcclusion v): OUTPUT0 {
#if defined(PLAT_WII)
	return 1;
#elif defined(PLAT_GLSL)
	float4	o	= float4(3, 3, 6, 6) * inv_tex_size.xyxy;
	float4	a	= v.uv.xyxy - o, b = v.uv.xyxy + o;
	o.yw = -o.yw;
	float4	c	= v.uv.xyxy - o, d = v.uv.xyxy + o;
	float	z	= v.uv.z;

	return (TestDepth(_zbuffer_shadow, a.zw, z)
		+	TestDepth(_zbuffer_shadow, a.xw, z)
		+	TestDepth(_zbuffer_shadow, d.xw, z)
		+	TestDepth(_zbuffer_shadow, d.zw, z)

		+	TestDepth(_zbuffer_shadow, a.zy, z)
		+	TestDepth(_zbuffer_shadow, a.xy, z)
		+	TestDepth(_zbuffer_shadow, d.xy, z)
		+	TestDepth(_zbuffer_shadow, d.zy, z)

		+	TestDepth(_zbuffer_shadow, c.zy, z)
		+	TestDepth(_zbuffer_shadow, c.xy, z)
		+	TestDepth(_zbuffer_shadow, b.xy, z)
		+	TestDepth(_zbuffer_shadow, b.zy, z)

		+	TestDepth(_zbuffer_shadow, c.zw, z)
		+	TestDepth(_zbuffer_shadow, c.xw, z)
		+	TestDepth(_zbuffer_shadow, b.xw, z)
		+	TestDepth(_zbuffer_shadow, b.zw, z)
	) * 0.0625;

#elif defined(PLAT_X360) || defined(PLAT_PS4)
	float2	uv	= v.uv.xy;
	float4 depth03 = float4(
		GetDepth(_zbuffer, uv, int2(-6, -6)),
		GetDepth(_zbuffer, uv, int2(-3, -6)),
		GetDepth(_zbuffer, uv, int2( 3, -6)),
		GetDepth(_zbuffer, uv, int2( 6, -6))
	);
	float4 depth47 = float4(
		GetDepth(_zbuffer, uv, int2(-6, -3)),
		GetDepth(_zbuffer, uv, int2(-3, -3)),
		GetDepth(_zbuffer, uv, int2( 3, -3)),
		GetDepth(_zbuffer, uv, int2( 6, -3))
	);
	float4 depth8B = float4(
		GetDepth(_zbuffer, uv, int2(-6,  3)),
		GetDepth(_zbuffer, uv, int2(-3,  3)),
		GetDepth(_zbuffer, uv, int2( 3,  3)),
		GetDepth(_zbuffer, uv, int2( 6,  3))
	);
	float4 depthCF = float4(
		GetDepth(_zbuffer, uv, int2(-6,  6)),
		GetDepth(_zbuffer, uv, int2(-3,  6)),
		GetDepth(_zbuffer, uv, int2( 3,  6)),
		GetDepth(_zbuffer, uv, int2( 6,  6))
	);
	return dot(.0625,
		float4(ztest_map(v.uv.z, depth03)) +
		float4(ztest_map(v.uv.z, depth47)) +
		float4(ztest_map(v.uv.z, depth8B)) +
		float4(ztest_map(v.uv.z, depthCF))
	);

#else
	float4	o = float4(3, 3, 6, 6) * inv_tex_size.xyxy;
	float4	a = v.uv.xyxy - o, b = v.uv.xyxy + o;
	o.yw = -o.yw;
	float4	c = v.uv.xyxy - o, d = v.uv.xyxy + o;

	float4	depth03 = float4(
		GetDepth(_zbuffer, a.zw),
		GetDepth(_zbuffer, a.xw),
		GetDepth(_zbuffer, d.xw),
		GetDepth(_zbuffer, d.zw)
	);
	float4	depth47 = float4(
		GetDepth(_zbuffer, a.zy),
		GetDepth(_zbuffer, a.xy),
		GetDepth(_zbuffer, d.xy),
		GetDepth(_zbuffer, d.zy)
	);
	float4	depth8B = float4(
		GetDepth(_zbuffer, c.zy),
		GetDepth(_zbuffer, c.xy),
		GetDepth(_zbuffer, b.xy),
		GetDepth(_zbuffer, b.zy)
	);
	float4	depthCF = float4(
		GetDepth(_zbuffer, c.zw),
		GetDepth(_zbuffer, c.xw),
		GetDepth(_zbuffer, b.xw),
		GetDepth(_zbuffer, b.zw)
	);
	return dot(.0625,
		float4(ztest_map(v.uv.z, depth03)) +
		float4(ztest_map(v.uv.z, depth47)) +
		float4(ztest_map(v.uv.z, depth8B)) +
		float4(ztest_map(v.uv.z, depthCF))
	);
#endif
}

float4 PS_LensFlare(vertexOutputLensFlare v) : OUTPUT0 {
	float4 uv		= v.uv;
	float4 colour	= v.colour;
#ifdef PLAT_WII
	return tex2D(diffuse_samp, uv.xy);
#else
	float	visibility	= PointSample(occlusion_samp, uv.zw).x;
	float4	sprite		= Add(float4(tex2D(diffuse_samp, uv.xy).rgb, visibility) * colour);
	return sprite;
#endif
}

technique lens_flare_test_occlusion {
	PASS(p0,VS_LensFlareTestOcclusion,PS_LensFlareTestOcclusion)
}

technique lens_flare {
	PASS(p0,VS_LensFlare,PS_LensFlare)
}
