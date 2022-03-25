#ifndef PERLIN_NOISE_H
#define PERLIN_NOISE_H

#include "base/vector.h"

namespace iso {

//-----------------------------------------------------------------------------
//	perlin noise
//-----------------------------------------------------------------------------

struct perlin_noise {
	enum	{
		B	= 0x100,
		BM	= 0xff,
	};
	struct perm {
		uint8	array[B];
		uint8	operator[](int i) { return array[i & BM]; }
	} p;

	float	g1[B];
	float2p	g2[B];
	float3p	g3[B];
	float4p	g4[B];

	float	at1(int i, float x) {
		return x * g1[i & BM];
	}
	float	at2(int i, float x, float y) {
		float2p	&p = g2[i & BM];
		return x * p.x + y * p.y;
	}
	float	at3(int i, float x, float y, float z) {
		float3p	&p = g3[i & BM];
		return x * p.x + y * p.y + z * p.z;
	}
	float	at4(int i, float x, float y, float z, float w) {
		float4p	&p = g4[i & BM];
		return x * p.x + y * p.y + z * p.z + w * p.w;
	}

	perlin_noise(int64 seed);
	float	noise1(float x);
	float	noise2(float x, float y);
	float	noise3(float x, float y, float z);
	float	noise4(float x, float y, float z, float w);

	float	noise(param(float2) p) { return noise2(p.x, p.y); }
	float	noise(param(float3) p) { return noise3(p.x, p.y, p.z); }
	float	noise(param(float4) p) { return noise4(p.x, p.y, p.z, p.w); }
};

}//namespace iso

#endif // PERLIN_NOISE_H
