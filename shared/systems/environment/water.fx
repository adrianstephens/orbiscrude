/**
 * Real-time Realistic Ocean Lighting using Seamless Transitions from Geometry to BRDF
 * Copyright (c) 2009 INRIA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /**
  * Author: Eric Bruneton
  */

#include "common.fxh"
#include "lighting.fxh"

static const float g	= 9.81;
static const float M_PI = 3.141592657;

sampler_defCUBE(_refmap, FILTER_MIN_MAG_MIP_LINEAR;MaxLOD=15;);
sampler_def2D(water_tex, FILTER_MIN_MAG_MIP_LINEAR;MaxLOD=15;);
StructuredBuffer<float4>	waves;		// waves parameters (h, omega, kx, ky) in wind space

float4		worldToWind;	// world space to wind space
int			nbWaves;		// number of waves
float		heightOffset;	// so that surface height is centered around z = 0
float2		sigmaSqTotal;	// total x and y variance in wind space
float4		lods;			// grid cell size in pixels, angle under which a grid cell is seen, and parameters of the geometric series used for wavelengths
float		nyquistMin;		// Nmin parameter
float		nyquistMax;		// Nmax parameter
float3		seaColor;		// sea bottom color


float2 WorldToWind(float2 v) {
	return mul(float2x2(worldToWind), v);
}

float2 WindToWorld(float2 v) {
	return mul(float2x2(worldToWind * float4(1,-1,-1,1)), v);
}

// ---------------------------------------------------------------------------
// REFLECTED SKY RADIANCE
// ---------------------------------------------------------------------------

// V, N, Tx, Ty in world space
float2 U(float2 zeta, float3 V, float3 N, float3 Tx, float3 Ty) {
	float3 f = normalize(float3(-zeta, 1)); // tangent space
	float3 F = f.x * Tx + f.y * Ty + f.z * N; // world space
	float3 R = V - 2 * dot(F, V) * F;
	return R.xy / (1 + R.z);
}

//Schlick's approximation:
// R(theta) = R0 + (1 - R0)(1 - cos(theta))^5, where R0 = ((n1 - n2) / (n1 + n2))^2 (n1, n2 are indices of refraction, theta is angle between N and V)

float meanFresnel(float cosThetaV, float sigmaV) {
	return pow(1 + cosThetaV, 5 * exp(-2.69 * sigmaV)) / (1 + 22.7 * pow(sigmaV, 1.5));
}

// V, N in world space
float meanFresnel(float3 V, float3 N, float2 sigmaSq) {
	float2	v		= WorldToWind(V.xy); // view direction in wind space
	float2	t		= v * v / (1 - square(V.z)); // cos^2 and sin^2 of view direction
	float	sigmaV2 = dot(t, sigmaSq); // slope variance in view direction
	return	meanFresnel(dot(V, N), sqrt(sigmaV2));
}
#if 0
// V, N, Tx, Ty in world space;
float3 meanSkyRadiance(float3 V, float3 N, float3 Tx, float3 Ty, float2 sigmaSq) {
	float4 result = float4(0.0);

	const float eps = 0.001;
	float2 u0	= U(float2(0, 0), V, N, Tx, Ty);
	float2 dux	= 2 * (U(float2(eps, 0), V, N, Tx, Ty) - u0) / eps * sqrt(sigmaSq.x);
	float2 duy	= 2 * (U(float2(0, eps), V, N, Tx, Ty) - u0) / eps * sqrt(sigmaSq.y);

	return tex2Dgrad(skySampler, u0 * (0.5 / 1.1) + 0.5, dux * (0.5 / 1.1), duy * (0.5 / 1.1));
}
#endif

// ----------------------------------------------------------------------------
//	SHADER ENTRY
// ----------------------------------------------------------------------------

//OCEAN

float4 VS_ocean(
	float2	position,
	out float2 u		: UV,		// coordinates in wind space used to compute P(u)
	out float3 P		: P,		// wave point P(u) in world space
	out float3 dPdu		: DPDU,		// dPdu in wind space, used to compute N
	out float3 dPdv		: DPDV,		// dPdv in wind space, used to compute N
	out float2 sigmaSq	: SIGMASQ,	// variance of unresolved waves in wind space
	out float lod		: LOD
) : POSITION_OUT {

#if 0
	float3	eye			= eyePos();
	float3	worldDir	= project(mul(float4(position, -1, 1), iviewProj)) - eye;
	float	t			= (heightOffset - eye.z) / worldDir.z;
	u		= WorldToWind(eye.xy + t * worldDir.xy);
#else
//	float3	eyeDir		= normalize(mul(float4(position, 0, 1), iproj).xyz);
	float3	eyeDir		= mul(float4(position, 0, 1), iproj).xyz;
	eyeDir /= eyeDir.z;
	float3	worldDir	= mul(float4(eyeDir, 0), iview).xyz;
//	float3	worldDir	= project(mul(float4(position, 0, 1), iviewProj));
	float	t			= - eyePos().z / worldDir.z;
	u		= WorldToWind(eyePos().xy + t * worldDir.xy);
#endif

	dPdu	= float3(1, 0, 0);
	dPdv	= float3(0, 1, 0);
	sigmaSq = sigmaSqTotal;
	lod		= t  * lods.y; // size in meters of one grid cell, projected on the sea surface

	float3	dP		= float3(0, 0, 0);
	int		iMin	= max(0, floor((log2(nyquistMin * lod) - lods.z) * lods.w));
	for (int i = iMin; i < nbWaves; ++i) {
		float4	wt		= waves[i];
		float	phase	= wt.y * time - dot(wt.zw, u);
		float	s		= sin(phase);
		float	c		= cos(phase);
		float	overk	= g / square(wt.y);
		float	wp		= 2;//smoothstep(nyquistMin, nyquistMax, (2 * M_PI) * overk / lod);

		float3	factor	= wp * wt.x * float3(wt.zw * overk, 1);
		dP += factor * float3(s, s, c);

		float3	dPd = factor * float3(c, c, -s);
		dPdu -= dPd * wt.z;
		dPdv -= dPd * wt.w;

		sigmaSq -= square(wt.zw * overk) * (1 - sqrt(1 - square(wt.x / overk)));
	}

	P = float3(WindToWorld(u + dP.xy), dP.z);
//	P = eyePos() + t * worldDir;
	return mul(float4(P, 1), viewProj);
}

float4 PS_ocean(
	float4 unused	: POSITION_OUT,
	float2 u		: UV,		// coordinates in wind space used to compute P(u)
	float3 P		: P,		// wave point P(u) in world space
	float3 dPdu		: DPDU,		// dPdu in wind space, used to compute N
	float3 dPdv		: DPDV,		// dPdv in wind space, used to compute N
	float2 sigmaSq	: SIGMASQ,	// variance of unresolved waves in wind space
	float lod		: LOD
) : OUTPUT0 {
//	return float4(P, 1);

	int	iMAX	= min(int(ceil((log2(nyquistMax * lod) - lods.z) * lods.w)), nbWaves - 1);
	int iMax	= int(floor((log2(nyquistMin * lod) - lods.z) * lods.w));
	int iMin	= max(0, int(floor((log2(nyquistMin * lod / lods.x) - lods.z) * lods.w)));
#if 1
	for (int i = iMin; i <= iMAX; ++i) {
		float4	wt		= waves[i];
		float	phase	= wt.y * time - dot(wt.zw, u);
		float	s		= sin(phase);
		float	c		= cos(phase);
		float	overk	= g / square(wt.y);
		float	wp		= 2;//smoothstep(nyquistMin, nyquistMax, (2 * M_PI) * overk / lod);
		float	wn		= 2;//smoothstep(nyquistMin, nyquistMax, (2 * M_PI) * overk / lod * lods.x);

		float3	factor	= (1 - wp) * wn * wt.x * float3(wt.zw * overk, 1);

		float3	dPd = factor * float3(c, c, -s);
		dPdu -= dPd * wt.z;
		dPdv -= dPd * wt.w;

		if (i < iMax) {
			float kh	= wt.x / overk;
			float wkh	= (1 - wn) * kh;
			sigmaSq -= square(wt.zw * overk) * (sqrt(1 - square(wkh)) - sqrt(1 - square(kh)));
		}
	}
#endif
	sigmaSq = max(sigmaSq, 2e-5);
	float3	V			= eyeDir(P);
	float3	windNormal	= normalize(cross(dPdu, dPdv));
	float3	N			= float3(WindToWorld(windNormal.xy), windNormal.z);
	if (dot(V, N) > 0)
		N = reflect(N, V); // reflects backfacing normals
	

	float3 Ty = normalize(cross(N, float3(worldToWind.x, -worldToWind.y, 0)));	//was[0]
	float3 Tx = cross(Ty, N);

	float fresnel	= 0.02 + 0.98 * meanFresnel(V, N, sigmaSq);
	return float4(fresnel, fresnel, fresnel, 1);
	float spec		= SpecularCalc(float3(0,1,0), N, V, 60);

//	N = float3(0,0,1);
	float3	R		= ReflectionCalc(N, V);
	float3	color	= fresnel * texCUBE(_refmap, R.xzy).rgb;
#ifdef SUN_CONTRIB
	color += reflectedSunRadiance(worldSunDir, V, N, Tx, Ty, sigmaSq) * Lsun;
#endif

#ifdef SKY_CONTRIB
	color += fresnel * meanSkyRadiance(V, N, Tx, Ty, sigmaSq);
#endif

#ifdef SEA_CONTRIB
	color += (1.0 - fresnel) * seaColor * Esky / M_PI;;
#endif

	return float4(color, 1);//float4(hdr(color), 0);
}

struct PatchVertex {
	 float2	v : POSITION;
};
struct PatchConsts {
	float		edge[4]		: TESS_FACTOR;
	float		inside[2]	: INSIDE_TESS_FACTOR;
	float3x2	grid2world	: GRID2WORLD;
};

PatchVertex VS_dummy(PatchVertex v) {
	return v;
}

// domain shader
DOMAIN_SHADER(quad) 
float4 DS_ocean(PatchConsts consts, float2 t : DOMAIN_LOCATION,
	out float2 u		: UV,		// coordinates in wind space used to compute P(u)
	out float3 P		: P,		// wave point P(u) in world space
	out float3 dPdu		: DPDU,		// dPdu in wind space, used to compute N
	out float3 dPdv		: DPDV,		// dPdv in wind space, used to compute N
	out float2 sigmaSq	: SIGMASQ,	// variance of unresolved waves in wind space
	out float lod		: LOD
) : POSITION_OUT {
	float2		pos = mul(float3(t, 1), consts.grid2world);
#ifdef PLAT_PC
	pos.y = -pos.y;
#endif
	return VS_ocean(pos, u, P, dPdu, dPdv, sigmaSq, lod);
}

// patch Constant Function
PatchConsts HS_oceanC(InputPatch<PatchVertex, 4> patch, uint id : PRIMITIVE_ID) {	
	float			x	= 64, y = 64;
    PatchConsts		consts;
	consts.edge[0]		= y;
	consts.edge[1]		= x;
	consts.edge[2]		= y;
	consts.edge[3]		= x;
	consts.inside[0]	= x;
	consts.inside[1]	= y;

	consts.grid2world[0] = patch[1].v - patch[0].v;//float2(2.2, 0);
	consts.grid2world[1] = patch[2].v - patch[0].v;//float2(0, 1);
	consts.grid2world[2] = patch[0].v;//float2(-1.1, 0.0001);
	return consts;
}

// hull shader
HULL_SHADER(HS_oceanC, quad, fractional_even, triangle_ccw, 4)
void HS_ocean(InputPatch<PatchVertex, 4> patch) {
}

technique ocean {
    pass p0 {
		SET_VS(VS_dummy);
		SET_HS(HS_ocean);
		SET_DS(DS_ocean);
		SET_PS(PS_ocean);
	}
}
