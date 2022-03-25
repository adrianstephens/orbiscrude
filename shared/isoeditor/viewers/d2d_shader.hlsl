Texture2D		InputTexture : register(t0);
SamplerState	InputSampler : register(s0);

cbuffer constants : register(b0) {
	row_major float2x4	transform	: packoffset(c0);
	row_major float2x4	itransform	: packoffset(c2);
};

float2x3 load2x3(float2x4 p) {
	return float2x3(p._11_12_21, p._13_14_22);
}

float length2(float2 v) { return dot(v, v); }
float sum(float2 v)		{ return v.x + v.y; }
float sum(float3 v)		{ return v.x + v.y + v.z; }
float sum(float4 v)		{ return v.x + v.y + v.z + v.w; }

float4 function(float4 uv : TEXCOORD0) {
	float2		tex_size;
	InputTexture.GetDimensions(tex_size.x, tex_size.y);

	float2x3	t = load2x3(itransform);
	t._13_23	/= tex_size;

	float2	uv2		= mul(t, float3(uv.xy, 1)).xy;
	float2	pixel	= uv2 * tex_size;

//	float4	col		= InputTexture.Sample(InputSampler, uv2);
	float4	col		= InputTexture[pixel];//.Load(intuv2);

	float	d		= ddx(pixel.x);
	if (d < 0.1) {
		float2	f	= (frac(pixel) - 0.5) * 2;
		float	t	= 1;//1 - d * 10;
		float4	d	= float4(0, 0.5 - sqrt(3) / 2, -0.5, +0.5) * t;
		float	r	= lerp(1, 0.5, t);
		float3	r2	= r * r * col.rgb * 2;

		float3	x	= float3(length2(f - d.xy), length2(f - d.zw), length2(f - d.ww));
		bool4	x1	= bool4(x < r2, 1);
		return lerp(col, float4(x1), t);
	}

	return col;
}

float4 main(
	float4 pos		: SV_POSITION,
	float4 posScene	: SCENE_POSITION,
	float4 uv		: TEXCOORD0
) : SV_Target
{
	return function(uv);
}