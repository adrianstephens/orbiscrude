#include "common.fxh"

//cbuffer VSConstants : register(b0) {
//	float4x4	worldViewProj;
//	float3		ViewerPos;
//};

uniform int	flags;

struct VSInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
};

static const float3 child_bones[] = {
	{0, 10.1851, -0.833919},			//head_top
	{6.79073, -3.50408, -0.225038},		//ear_left
	{-7.09389, -3.38294, -0.230426},	//ear_right
	{0, -10.6462, 8.52201},				//chin
	{0, 6.67188, 8.42607}				//forehead
};


static const float3 woman_bones[] = {
	{3.40569e-17, 11.9194, -0.160459},	//head_top
	{7.84788, -1.82635, 0.565226},		//ear_left
	{-7.70647, -1.79085, 0.563352},		//ear_right
	{-0.0353508, -11.2668, 10.1071},	//chin
	{9.87202e-08, 8.41268, 9.10363},	//forehead
};

struct VSOutput {
	float4	position : SV_Position;
	float3	normal	: normal;
	float	dist[5]	: TEXCOORD0;
};

VSOutput vs(VSInput vsInput) {
	VSOutput vsOutput;

	float3	pos		= vsInput.position;

	if (flags & 512) {

		float	total	= 0;

		for (int i = 0; i < 5; i++) {
			total += vsOutput.dist[i] = min(1.f / distance(pos, woman_bones[i]), 1.f);
		}

		for (int i = 0; i < 5; i++) {
			vsOutput.dist[i] /= total;
			pos += (child_bones[i] - woman_bones[i]) * vsOutput.dist[i];
		}
	}

	vsOutput.position	= mul(float4(pos, 1.0), worldViewProj);
	vsOutput.normal		= vsInput.normal;

	return vsOutput;
}

float3 ps(VSOutput vsOutput) : SV_Target0 {
	return float3(vsOutput.dist[0], vsOutput.dist[1], vsOutput.dist[2]);
}

technique simple {
	PASS(p0, vs, ps)
};
