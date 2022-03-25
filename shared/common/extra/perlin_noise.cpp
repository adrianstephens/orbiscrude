#include "perlin_noise.h"
#include "random.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	perlin noise
//-----------------------------------------------------------------------------

perlin_noise::perlin_noise(int64 seed) {
	rng<simple_random>	random(seed);

	for (int i = 0 ; i < B ; i++) {
		p.array[i] = i;
		float	x = float(random) - 0.5f;
		float	y = float(random) - 0.5f;
		float	z = float(random) - 0.5f;
		float	w = float(random) - 0.5f;
		g1[i] = x;
		g2[i] = normalise(float2{x, y});
		g3[i] = normalise(float3{x, y, z});
		g4[i] = normalise(float4{x, y, z, w});
	}

	for (int i = 0; i < B ; i++)
		swap(p.array[i], p.array[random.to(int(B))]);
}

float perlin_noise::noise1(float x) {
	float	t	= floor(x);
	int		bx	= int(t);
	float	rx0 = x - t,	rx1 = rx0 - 1;

	return smoothstep(at1(p[bx + 0], rx0), at1(p[bx + 1], rx1), rx0);
}

float perlin_noise::noise2(float x, float y) {
	float	t;

	t	= floor(x);
	int		bx	= int(t);
	float	rx0 = x - t,	rx1 = rx0 - 1;

	t	= floor(y);
	int		by	= int(t);
	float	ry0 = y - t,	ry1 = ry0 - 1;

	int		b0	= p[bx + 0];
	int		b1	= p[bx + 1];

	return smoothstep(
		smoothstep(at2(p[b0 + by + 0], rx0, ry0), at2(p[b1 + by + 0], rx1, ry0), rx0),
		smoothstep(at2(p[b0 + by + 1], rx0, ry1), at2(p[b1 + by + 1], rx1, ry1), rx0),
		ry0
	);
}

float perlin_noise::noise3(float x, float y, float z) {
	float	t;

	t	= floor(x);
	int		bx	= int(t);
	float	rx0 = x - t,	rx1 = rx0 - 1;

	t	= floor(y);
	int		by	= int(t);
	float	ry0 = y - t,	ry1 = ry0 - 1;

	t	= floor(z);
	int		bz	= int(t);
	float	rz0 = z - t,	rz1 = rz0 - 1;

	int		b0	= p[bx + 0];
	int		b1	= p[bx + 1];
	int		b00 = p[b0 + by + 0];
	int		b10 = p[b1 + by + 0];
	int		b01 = p[b0 + by + 1];
	int		b11 = p[b1 + by + 1];

	return smoothstep(
		smoothstep(
			smoothstep(at3(b00 + bz + 0, rx0,ry0,rz0), at3(b10 + bz + 0, rx1,ry0,rz0), rx0),
			smoothstep(at3(b01 + bz + 0, rx0,ry1,rz0), at3(b11 + bz + 0, rx1,ry1,rz0), rx0),
			ry0
		),
		smoothstep(
			smoothstep(at3(b00 + bz + 1, rx0,ry0,rz1), at3(b10 + bz + 1, rx1,ry0,rz1), rx0),
			smoothstep(at3(b01 + bz + 1, rx0,ry1,rz1), at3(b11 + bz + 1, rx1,ry1,rz1), rx0),
			ry0
		),
		rz0
	);
}

float perlin_noise::noise4(float x, float y, float z, float w) {
	float	t;

	t	= floor(x);
	int		bx	= int(t);
	float	rx0 = x - t,	rx1 = rx0 - 1;

	t	= floor(y);
	int		by	= int(t);
	float	ry0 = y - t,	ry1 = ry0 - 1;

	t	= floor(z);
	int		bz	= int(t);
	float	rz0 = z - t,	rz1 = rz0 - 1;

	t	= floor(w);
	float	rw0 = w - t,	rw1 = rw0 - 1;

	int		b0		= p[bx + 0];
	int		b1		= p[bx + 1];
	int		b00		= p[b0 + by + 0];
	int		b10		= p[b1 + by + 0];
	int		b01		= p[b0 + by + 1];
	int		b11		= p[b1 + by + 1];

	return smoothstep(
		smoothstep(
			smoothstep(
				smoothstep(at4(b00 + bz + 0, rx0,ry0,rz0,rw0), at4(b10 + bz + 0, rx1,ry0,rz0,rw0), rx0),
				smoothstep(at4(b01 + bz + 0, rx0,ry1,rz0,rw0), at4(b11 + bz + 0, rx1,ry1,rz0,rw0), rx0),
				ry0
			),
			smoothstep(
				smoothstep(at4(b00 + bz + 1, rx0,ry0,rz1,rw0), at4(b10 + bz + 1, rx1,ry0,rz1,rw0), rx0),
				smoothstep(at4(b01 + bz + 1, rx0,ry1,rz1,rw0), at4(b11 + bz + 1, rx1,ry1,rz1,rw0), rx0),
				ry0
			),
			rz0
		),
		smoothstep(
			smoothstep(
				smoothstep(at4(b00 + bz + 0, rx0,ry0,rz0,rw1), at4(b10 + bz + 0, rx1,ry0,rz0,rw1), rx0),
				smoothstep(at4(b01 + bz + 0, rx0,ry1,rz0,rw1), at4(b11 + bz + 0, rx1,ry1,rz0,rw1), rx0),
				ry0
			),
			smoothstep(
				smoothstep(at4(b00 + bz + 1, rx0,ry0,rz1,rw1), at4(b10 + bz + 1, rx1,ry0,rz1,rw1), rx0),
				smoothstep(at4(b01 + bz + 1, rx0,ry1,rz1,rw1), at4(b11 + bz + 1, rx1,ry1,rz1,rw1), rx0),
				ry0
			),
			rz0
		),
		rw0
	);
}
