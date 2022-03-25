cbuffer constants : register(b0) {
	row_major float2x4	itransform	: packoffset(c0);
	float4				sizes		: packoffset(c2);
	float4				colour1		: packoffset(c3);
	float4				colour2		: packoffset(c4);
	int					mode		: packoffset(c5.x);
};

float2x3 load2x3(float2x4 p) {
	return float2x3(p._11_12_21, p._13_14_22);
}

float notf(float a) {
	return 1 - a;
}

float andf(float a, float b) {
	return a * b;
}

float orf(float a, float b) {
	return notf(andf(notf(a), notf(b)));
}

float xorf(float a, float b) {
//	return a + b + a * b * (a + b - a * b - 3);
	return orf(andf(a, notf(b)), andf(notf(a), b));
}

float gamma(float v) {
	return v < 0.0031308 ? 12.92 * v : pow(v, 1 / 2.4) * 1.055 - 0.055;
}

float inv_gamma(float v) {
	return v < 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

float4 main(
	float4 pos		: SV_POSITION,
	float4 posScene	: SCENE_POSITION
) : SV_Target
{
	float2		total	= sizes.xy + sizes.zw;
	float2x3	trans	= load2x3(itransform);
	float2		grid	= mul(trans, float3(posScene.xy, 1)).xy / total;
#if 0
	float2		d		= float2(ddx(grid.x), ddy(grid.y));
#else
	float2		d		= trans._m00_m11 / total;
#endif

	grid	+= min(d, 0);
	d		= abs(d);

	float2		n		= floor(d);	// whole cycles
	float2		x0		= frac(grid);
	float2		x1		= x0 + frac(d);
	x0 *= total;
	x1 *= total;
	float2		f		= min(x1, sizes.xy) - min(x0, sizes.xy) + clamp(x1 - total, 0, sizes.xy);

	float2		a		= (n * sizes.xy + f) / (d * total);
	float		g		= mode ? andf(a.x, a.y) : xorf(a.x, a.y);

	return lerp(colour1, colour2, inv_gamma(g));
}