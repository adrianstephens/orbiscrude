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

static const float g	= 9.81;
static const float M_PI = 3.141592657;

sampler2D	transmittanceSampler;
sampler2D	skyIrradianceSampler;
sampler3D	inscatterSampler;
sampler2D	noiseSampler;

float2x2	worldToWind;	// world space to wind space
float2x2	windToWorld;	// wind space to world space
float3		worldSunDir;	// sun direction in world space
float		hdrExposure;

// cloud factors
float		octaves;
float		lacunarity;
float		gain;
float		norm;
float		clamp1;
float		clamp2;
float4		cloudsColor;

static const float SUN_INTENSITY = 100.0;
static const float3 earthPos = float3(0.0, 0.0, 6360010.0);

// ----------------------------------------------------------------------------
// PHYSICAL MODEL PARAMETERS
// ----------------------------------------------------------------------------

static const float SCALE	= 1000.0;
static const float Rg		= 6360.0 * SCALE;
static const float Rt		= 6420.0 * SCALE;
static const float RL		= 6421.0 * SCALE;

static const float AVERAGE_GROUND_REFLECTANCE = 0.1;

// Rayleigh
static const float	HR		= 8.0 * SCALE;
static const float3 betaR	= float3(5.8e-3, 1.35e-2, 3.31e-2) / SCALE;

// Mie
// DEFAULT
static const float	HM		= 1.2 * SCALE;
static const float3 betaMSca = float(4e-3).xxx / SCALE;
static const float3 betaMEx = betaMSca / 0.9;
static const float	mieG	= 0.8;

// CLEAR SKY
/*
static const float HM = 1.2 * SCALE;
static const float3 betaMSca = 20e-3.xxx / SCALE;
static const float3 betaMEx = betaMSca / 0.9;
static const float mieG = 0.76;
*/
// PARTLY CLOUDY
/*
static const float HM = 3.0 * SCALE;
static const float3 betaMSca = 3e-3.xxx / SCALE;
static const float3 betaMEx = betaMSca / 0.9;
static const float mieG = 0.65;
*/

// ----------------------------------------------------------------------------
// PARAMETERIZATION OPTIONS
// ----------------------------------------------------------------------------

static const int RES_R = 32;
static const int RES_MU = 128;
static const int RES_MU_S = 32;
static const int RES_NU = 8;

#define TRANSMITTANCE_NON_LINEAR
#define INSCATTER_NON_LINEAR

// ----------------------------------------------------------------------------
// PARAMETERIZATION FUNCTIONS
// ----------------------------------------------------------------------------

float2 getTransmittanceUV(float r, float mu) {
	float uR, uMu;
#ifdef TRANSMITTANCE_NON_LINEAR
	uR = sqrt((r - Rg) / (Rt - Rg));
	uMu = atan((mu + 0.15) / (1.0 + 0.15) * tan(1.5)) / 1.5;
#else
	uR = (r - Rg) / (Rt - Rg);
	uMu = (mu + 0.15) / (1.0 + 0.15);
#endif
	return float2(uMu, uR);
}

float2 getIrradianceUV(float r, float muS) {
	float uR = (r - Rg) / (Rt - Rg);
	float uMuS = (muS + 0.2) / (1.0 + 0.2);
	return float2(uMuS, uR);
}

float4 texture4D(sampler3D table, float r, float mu, float muS, float nu) {
	float H = sqrt(Rt * Rt - Rg * Rg);
	float rho = sqrt(r * r - Rg * Rg);
#ifdef INSCATTER_NON_LINEAR
	float rmu = r * mu;
	float delta = rmu * rmu - r * r + Rg * Rg;
	float4 cst = rmu < 0.0 && delta > 0.0 ? float4(1.0, 0.0, 0.0, 0.5 - 0.5 / float(RES_MU)) : float4(-1.0, H * H, H, 0.5 + 0.5 / float(RES_MU));
	float uR = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));
	float uMu = cst.w + (rmu * cst.x + sqrt(delta + cst.y)) / (rho + cst.z) * (0.5 - 1.0 / float(RES_MU));
	// paper formula
	//float uMuS = 0.5 / float(RES_MU_S) + max((1.0 - exp(-3.0 * muS - 0.6)) / (1.0 - exp(-3.6)), 0.0) * (1.0 - 1.0 / float(RES_MU_S));
	// better formula
	float uMuS = 0.5 / float(RES_MU_S) + (atan(max(muS, -0.1975) * tan(1.26 * 1.1)) / 1.1 + (1.0 - 0.26)) * 0.5 * (1.0 - 1.0 / float(RES_MU_S));
#else
	float uR = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));
	float uMu = 0.5 / float(RES_MU) + (mu + 1.0) / 2.0 * (1.0 - 1.0 / float(RES_MU));
	float uMuS = 0.5 / float(RES_MU_S) + max(muS + 0.2, 0.0) / 1.2 * (1.0 - 1.0 / float(RES_MU_S));
#endif
	float lerp = (nu + 1.0) / 2.0 * (float(RES_NU) - 1.0);
	float uNu = floor(lerp);
	lerp = lerp - uNu;
	return tex3D(table, float3((uNu + uMuS) / float(RES_NU), uMu, uR)) * (1.0 - lerp) +	tex3D(table, float3((uNu + uMuS + 1.0) / float(RES_NU), uMu, uR)) * lerp;
}

void getMuMuSNu(float2 uv, float r, float4 dhdH, out float mu, out float muS, out float nu) {
	float x = uv.x - 0.5;
	float y = uv.y - 0.5;
#ifdef INSCATTER_NON_LINEAR
	if (y < float(RES_MU) / 2.0) {
		float d = 1.0 - y / (float(RES_MU) / 2.0 - 1.0);
		d = min(max(dhdH.z, d * dhdH.w), dhdH.w * 0.999);
		mu = (Rg * Rg - r * r - d * d) / (2.0 * r * d);
		mu = min(mu, -sqrt(1.0 - (Rg / r) * (Rg / r)) - 0.001);
	} else {
		float d = (y - float(RES_MU) / 2.0) / (float(RES_MU) / 2.0 - 1.0);
		d = min(max(dhdH.x, d * dhdH.y), dhdH.y * 0.999);
		mu = (Rt * Rt - r * r - d * d) / (2.0 * r * d);
	}
	muS = fmod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
	// paper formula
	//muS = -(0.6 + log(1.0 - muS * (1.0 -  exp(-3.6)))) / 3.0;
	// better formula
	muS = tan((2.0 * muS - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);
	nu = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
#else
	mu = -1.0 + 2.0 * y / (float(RES_MU) - 1.0);
	muS = mod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
	muS = -0.2 + muS * 1.2;
	nu = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
#endif
}

// ----------------------------------------------------------------------------
// UTILITY FUNCTIONS
// ----------------------------------------------------------------------------

// transmittance(=transparency) of atmosphere for infinite ray (r,mu)
// (mu=cos(view zenith angle)), intersections with ground ignored
float3 transmittance(float r, float mu) {
	return tex2D(transmittanceSampler, getTransmittanceUV(r, mu)).rgb;
}

// transmittance(=transparency) of atmosphere for infinite ray (r,mu)
// (mu=cos(view zenith angle)), or zero if ray intersects ground
float3 transmittanceWithShadow(float r, float mu) {
	return mu < -sqrt(1.0 - (Rg / r) * (Rg / r)) ? (float3)0 : transmittance(r, mu);
}

// transmittance(=transparency) of atmosphere between x and x0
// assume segment x,x0 not intersecting ground
// r=||x||, mu=cos(zenith angle of [x,x0) ray at x), v=unit direction vector of [x,x0) ray
float3 transmittance(float r, float mu, float3 v, float3 x0) {
	float	r1 = length(x0);
	float	mu1 = dot(x0, v) / r;
	return mu > 0 ? min(transmittance(r, mu) / transmittance(r1, mu1), 1.0) : min(transmittance(r1, -mu1) / transmittance(r, -mu), 1);
}

// transmittance(=transparency) of atmosphere between x and x0
// assume segment x,x0 not intersecting ground
// d = distance between x and x0, mu=cos(zenith angle of [x,x0) ray at x)
float3 transmittance(float r, float mu, float d) {
	float	r1	= sqrt(r * r + d * d + 2.0 * r * mu * d);
	float	mu1 = (r * mu + d) / r1;
	return mu > 0.0 ? min(transmittance(r, mu) / transmittance(r1, mu1), 1.0) : min(transmittance(r1, -mu1) / transmittance(r, -mu), 1.0);
}

float3 irradiance(sampler2D s, float r, float muS) {
	return tex2D(s, getIrradianceUV(r, muS)).rgb;
}

// Rayleigh phase function
float phaseFunctionR(float mu) {
	return (3.0 / (16.0 * M_PI)) * (1 + mu * mu);
}

// Mie phase function
float phaseFunctionM(float mu) {
	return 1.5 / (4 * M_PI) * (1 - mieG * mieG) * pow(1 + mieG * mieG - 2 * mieG * mu, -3.0 / 2.0) * (1 + mu * mu) / (2.0 + mieG * mieG);
}

// approximated single Mie scattering (cf. approximate Cm in paragraph "Angular precision")
float3 getMie(float4 rayMie) { // rayMie.rgb=C*, rayMie.w=Cm,r
	return rayMie.rgb * rayMie.w / max(rayMie.r, 1e-4) * (betaR.r / betaR);
}

// ----------------------------------------------------------------------------
// PUBLIC FUNCTIONS
// ----------------------------------------------------------------------------

// incident sun light at given position (radiance)
// r	= length(x)
// muS	= dot(x,s) / r
float3 sunRadiance(float r, float muS) {
	return transmittanceWithShadow(r, muS) * SUN_INTENSITY;
}

// incident sky light at given position, integrated over the hemisphere (irradiance)
// r	= length(x)
// muS	= dot(x,s) / r
float3 skyIrradiance(float r, float muS) {
	return irradiance(skyIrradianceSampler, r, muS) * SUN_INTENSITY;
}

// scattered sunlight between two points
// camera	= observer
// viewdir	= unit vector towards observed point
// sundir	= unit vector towards the sun
// return scattered light and extinction coefficient
float3 skyRadiance(float3 camera, float3 viewdir, float3 sundir, out float3 extinction) {
	float3	result;
	float	r	= length(camera);
	float	rMu = dot(camera, viewdir);
	float	mu	= rMu / r;
	float	r0	= r;
	float	mu0 = mu;

	float deltaSq = sqrt(rMu * rMu - r * r + Rt*Rt);
	float din = max(-rMu - deltaSq, 0.0);
	if (din > 0) {
		camera += din * viewdir;
		rMu += din;
		mu = rMu / Rt;
		r = Rt;
	}

	if (r <= Rt) {
		float nu	= dot(viewdir, sundir);
		float muS	= dot(camera, sundir) / r;

		float4 inScatter = texture4D(inscatterSampler, r, rMu / r, muS, nu);
		extinction = transmittance(r, mu);

		float3	inScatterM	= getMie(inScatter);
		float	phase		= phaseFunctionR(nu);
		float	phaseM		= phaseFunctionM(nu);
		return inScatter.rgb * phase + inScatterM * phaseM * SUN_INTENSITY;
	} else {
		extinction	= (float3)1;
		return (float3)0;
	}
}

// scattered sunlight between two points
// camera	= observer
// pos		= point on the ground
// sundir	= unit vector towards the sun
// return scattered light and extinction coefficient
float3 inScattering(float3 camera, float3 pos, float3 sundir, out float3 extinction) {
	float3	viewdir = pos - camera;
	float	d = length(viewdir);
	viewdir = viewdir / d;

	float	r = length(camera);
	float	rMu = dot(camera, viewdir);
	float	mu = rMu / r;
	float	r0 = r;
	float	mu0 = mu;

	float deltaSq = sqrt(rMu * rMu - r * r + Rt*Rt);
	float din = max(-rMu - deltaSq, 0.0);
	if (din > 0.0) {
		camera += din * viewdir;
		rMu += din;
		mu = rMu / Rt;
		r = Rt;
		d -= din;
	}

	if (r <= Rt) {
		float	nu = dot(viewdir, sundir);
		float	muS = dot(camera, sundir) / r;

		if (r < Rg + 600.0) {
			// avoids imprecision problems in aerial perspective near ground
			float f = (Rg + 600.0) / r;
			r = r * f;
			rMu = rMu * f;
			pos = pos * f;
		}

		float r1	= length(pos);
		float rMu1	= dot(pos, viewdir);
		float mu1	= rMu1 / r1;
		float muS1	= dot(pos, sundir) / r1;

		extinction = mu > 0 ? min(transmittance(r, mu) / transmittance(r1, mu1), 1) :  min(transmittance(r1, -mu1) / transmittance(r, -mu), 1);

		float4	inScatter0	= texture4D(inscatterSampler, r, mu, muS, nu);
		float4	inScatter1	= texture4D(inscatterSampler, r1, mu1, muS1, nu);
		float4	inScatter	= max(inScatter0 - inScatter1 * extinction.rgbr, 0.0);

		// avoids imprecision problems in Mie scattering when sun is below horizon
		inScatter.w *= smoothstep(0.00, 0.02, muS);

		float3	inScatterM	= getMie(inScatter);
		float	phase		= phaseFunctionR(nu);
		float	phaseM		= phaseFunctionM(nu);
		return inScatter.rgb * phase + inScatterM * phaseM * SUN_INTENSITY;
	} else {
		extinction	= (float3)1;
		return (float3)0;
	}
}

void sunRadianceAndSkyIrradiance(float3 worldP, float3 worldS, out float3 sunL, out float3 skyE) {
	float3	worldV	= normalize(worldP); // vertical vector
	float	r		= length(worldP);
	float	muS		= dot(worldV, worldS);
	sunL = sunRadiance(r, muS);
	skyE = skyIrradiance(r, muS);
}

float3 hdr(float3 L) {
	L	= L * hdrExposure;
	L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1 / 2.2) : 1 - exp(-L.r);
	L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1 / 2.2) : 1 - exp(-L.g);
	L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1 / 2.2) : 1 - exp(-L.b);
	return L;
}

// ----------------------------------------------------------------------------
// CLOUDS
// ----------------------------------------------------------------------------

float4 cloudColor(float3 worldP, float3 eyePos, float3 worldSunDir) {
	const float a = 23.0 / 180.0 * M_PI;
	float2x2	m = float2x2(cos(a), sin(a), -sin(a), cos(a));

	float2	st	= worldP.xy / 1000000.0;
	float	g	= 1.0;
	float	r	= 0.0;
	for (float i = 0.0; i < octaves; i += 1.0) {
		r -= g * (2.0 * tex2D(noiseSampler, st).r - 1.0);
		st = mul(st, m) * lacunarity;
		g *= gain;
	}

	float	v	= saturate((r * norm - clamp1) / (clamp2 - clamp1));
	float	t	= saturate((r * norm * 3.0 - clamp1) / (clamp2 - clamp1));
	float3	PP	= worldP + earthPos;

	float3	Lsun, Esky, extinction;
	sunRadianceAndSkyIrradiance(PP, worldSunDir, Lsun, Esky);

	float3	inscatter	= inScattering(eyePos + earthPos, PP, worldSunDir, extinction);

	return	float4(v * (Lsun * max(worldSunDir.z, 0) + Esky / 10) / M_PI * extinction + inscatter, t) * cloudsColor;
}

float4 cloudColorV(float3 eyePos, float3 V, float3 worldSunDir) {
	float3 P = eyePos + V * (3000.0 - eyePos.z) / V.z;
	return cloudColor(P, eyePos, worldSunDir);
}

// ----------------------------------------------------------------------------
//	SHADER ENTRY
// ----------------------------------------------------------------------------

//CLOUDS

float4 VS_clouds(float3 pos : POSITION_IN, out float3 worldP) : POSITION_OUT {
	float4	wpos = mul(float4(pos, 1.0), viewProj);
	worldP = wpos.xyz;
	return wpos;
}

float4 PS_clouds(float3 worldP : POSITION) : OUTPUT0 {
	float4	color = cloudColor(worldP, eyePos(), worldSunDir);
	color.rgb = hdr(color.rgb);
	return color;
}

technique clouds {
	PASS(p0, VS_clouds, PS_clouds)
};

// SKY

float4 VS_sky(float3 pos : POSITION_IN, out float3 viewDir) : POSITION_OUT {
	viewDir = mul(float4(mul(float4(pos, 1), iproj).xyz, 0), iview).xyz;
	return float4(pos.xy, 0.9999999, 1.0);
}

float4 PS_sky(float3 viewDir) : OUTPUT0 {
	float3 v			= normalize(viewDir);
	float3 sunColor		= step(cos(3.1415926 / 180.0), dot(v, worldSunDir)) * SUN_INTENSITY;
	float3 extinction;
	float3 inscatter	= skyRadiance(eyePos() + earthPos, v, worldSunDir, extinction);
	float3 finalColor	= sunColor * extinction + inscatter;
	return float4(hdr(finalColor), 1);
}
technique sky {
	PASS(p0, VS_sky, PS_sky)
};

// SKYMAP

float4 VS_skymap(float2 pos : POSITION_IN, out float2 u : TEXCOORD0) : POSITION_OUT {
	u = pos * 1.1;
	return float4(pos, 0, 1);
}

float4 PS_skymap(float2 u : TEXCOORD0) : OUTPUT0 {
	float	l		= dot(u, u);

	if (l <= 1.02) {
		if (l > 1.0) {
			u = u / l;
			l = 1.0 / l;
		}
		// inverse stereographic projection, from skymap coordinates to world space directions
		float3	r		= float3(2 * u, 1 - l) / (1 + l);
	#ifdef CLOUDS
		float4 cloudL	= cloudColorV(float3(0.0), r, worldSunDir);
		return float4(cloudL.rgb * cloudL.a + result.rgb * (1.0 - cloudL.a), 0);
	#else
		float3	extinction;
		return float4(skyRadiance(earthPos, r, worldSunDir, extinction) / 32, 0);
	#endif
	} else {
		// below horizon:
		// use average fresnel * average sky radiance
		// to simulate multiple reflections on waves
		const float avgFresnel = 0.17;
		return float4(skyIrradiance(earthPos.z, worldSunDir.z) / M_PI * avgFresnel, 0);
	}
}

technique skymap {
	PASS(p0, VS_skymap, PS_skymap)
};
