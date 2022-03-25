#include "common.fxh"
#include "lighting.fxh"

#define SEA_BOTTOM

static const float g	= 9.81;
static const float M_PI = 3.141592657;

sampler_defCUBE(_refmap,	FILTER_MIN_MAG_MIP_LINEAR; MaxLOD=15;);
sampler_def2D(water_tex,	FILTER_MIN_MAG_MIP_LINEAR; MaxLOD=15;);
sampler_def2D(bottom_tex,	FILTER_MIN_MAG_MIP_LINEAR; MaxLOD=15;);

DataBuffer<float4>	waves;		// waves parameters (h, omega, kx, ky) in wind space

uniform column_major float4x4 screenToCamera; // screen space to camera space
uniform column_major float4x4 cameraToWorld; // camera space to world space
uniform column_major float4x4 worldToScreen; // world space to screen space
uniform float3	worldCamera1;	// camera position in world space
uniform float3	worldCamera;	// camera position in world space

uniform column_major float4x4 screenToWorld;

uniform int		nbWaves;		// number of waves
uniform float2	sigmaSqTotal;	// total x and y variance in wind space
uniform float4	lods;			// grid cell size in pixels, angle under which a grid cell is seen, and parameters of the geometric series used for wavelengths
uniform float	nyquistMin;		// Nmin parameter
uniform float	nyquistMax;		// Nmax parameter
uniform float3	seaColor;		// sea bottom color
uniform float2	worldToWind2;
uniform float4	groundPlane;

float2 inverse2D(float2 r) {
	return float2(r.x, -r.y);
}
float2 rotate2D(float2 v, float2 r) {
	return float2(v.x * r.x - v.y * r.y, v.x * r.y + v.y * r.x);
}

float2 WorldToWind(float2 v) {
	return rotate2D(v, worldToWind2);
}

float2 WindToWorld(float2 v) {
	return rotate2D(v, inverse2D(worldToWind2));
}

float3 WindToWorld(float3 v) {
	return float3(WindToWorld(v.xy), v.z);
}

float IntersectPlane(float3 P, float3 D, float4 plane) {
	return -dot(float4(P, 1), plane) / dot(D, plane.xyz);
}

// ---------------------------------------------------------------------------
// Lighting calcs
// ---------------------------------------------------------------------------

// V, N in world space
float meanFresnel(float3 V, float3 N, float2 sigmaSq) {
	float2	t		= square(WorldToWind(V.xy)) / (1 - square(V.z));
	float	sigmaV	= sqrt(dot(t, sigmaSq)); // slope variance in view direction
	return pow(1 + dot(V, N), 5.0 * exp(-2.69 * sigmaV)) / (1 + 22.7 * pow(sigmaV, 1.5));
}

float3 meanSkyRadiance(float3 V, float3 N, float3 Tx, float3 Ty, float2 sigmaSq) {
	float3	u0	= reflect(V, N);
	float3	dux = -4 * (dot(V, N) * Tx + dot(V, Tx) * N) * sqrt(sigmaSq.x);
	float3	duy = -4 * (dot(V, N) * Ty + dot(V, Ty) * N) * sqrt(sigmaSq.y);

	return texCUBEgrad(_refmap, u0, dux, duy).rgb;
}

float3 oceanBottom(float3 P, float3 V, float3 N, float3 Tx, float3 Ty, float2 sigmaSq) {
	const float eta		= 1/1.34;
	const float size	= 8;

	float	k	= 1 - square(eta) * (1 - square(dot(N, V)));
	if (k < 0)
		return 0;

	float3	R	= eta * V - (eta * dot(N, V) + sqrt(k)) * N;

	float3	dRx	= eta * cross(N, cross(Tx, V)) + sqrt(k) * Tx - square(eta) * dot(Tx, V) * rsqrt(k) * N;
	float3	dRy	= eta * cross(N, cross(Ty, V)) + sqrt(k) * Ty - square(eta) * dot(Ty, V) * rsqrt(k) * N;

	float	dotPg	= dot(float4(P, 1), groundPlane);
	float	dotRg	= dot(R, groundPlane.xyz);

	float	d0	= dotPg / dotRg;
	float3	u0	= P - d0 * R;

	float	d	= abs(d0);// * length(R);

	float3	dux = 2 * dotPg / square(dotRg) * (dot(dRx, groundPlane.xyz) * R + dotRg * dRx) * sqrt(sigmaSq.x);
	float3	duy = 2 * dotPg / square(dotRg) * (dot(dRy, groundPlane.xyz) * R + dotRg * dRy) * sqrt(sigmaSq.y);
//	return tex2D(bottom_tex, u0.xy / size).rgb;
	return lerp(seaColor, tex2Dgrad(bottom_tex, u0.xy / size, (ddx(P.xy) + dux.xy) / size, (ddy(P.xy) + duy.xy) / size).rgb, exp(-d / float3(10,16,20)));
}

// ---------------------------------------------------------------------------
// Vertex Shader
// ---------------------------------------------------------------------------

float4 VS(
	float4 pos			:POSITION,
	out float2 u		:U,			 // coordinates in wind space used to compute P(u)
	out float3 P		:P,			// wave point P(u) in world space
	out float3 dPdu		:DPDU,		// dPdu in wind space, used to compute N
	out float3 dPdv		:DPDV,		// dPdv in wind space, used to compute N
	out float2 sigmaSq	:SIGMASQ,	// variance of unresolved waves in wind space
	out float lod		:LOD
) : POSITION_OUT {
	float3 cameraDir	= normalize(mul(screenToCamera, pos).xyz);
	float3 worldDir		= mul((float3x3)cameraToWorld, cameraDir);
//	float3 worldDir		= normalise(mul((float3x3)screenToWorld, pos.xyz));
	if (worldDir.z >= 0)
		worldDir.z = -0.00001f;
	float t				= -worldCamera1.z / worldDir.z;

	u	= WorldToWind(worldCamera1.xy + t * worldDir.xy);
	P	= float3(u, 0);
	lod = -t / worldDir.z * lods.y; // size in meters of one grid cell, projected on the sea surface

	dPdu	= float3(1, 0, 0);
	dPdv	= float3(0, 1, 0);
	sigmaSq	= sigmaSqTotal;
	
	int iMin = max(0, int(floor((log2(nyquistMin * lod) - lods.z) * lods.w)));
	for (int i = iMin; i < nbWaves; i++) {
		float4	wt		= waves[i];
		float	phase	= wt.y * time - dot(wt.zw, u);
		float	s		= sin(phase);
		float	c		= cos(phase);
		float	overk	= g / square(wt.y);

		float	wp		= smoothstep(nyquistMin, nyquistMax, 2 * M_PI * overk / lod);
		float3	factor	= wp * wt.x * float3(wt.zw * overk, 1);
		P += factor * float3(s, s, c);

		float3 dPd = factor * float3(c, c, -s);
		dPdu -= dPd * wt.z;
		dPdv -= dPd * wt.w;

		float	kh		= wt.x / overk;
		sigmaSq -= square(wt.zw * overk) * (1 - sqrt(1 - square(kh)));
	}

	P = WindToWorld(P);

	return mul(worldToScreen, float4(P, 1));
}

// ---------------------------------------------------------------------------
// Pixel shader
// ---------------------------------------------------------------------------

float4 PS(
	float4 unused	:POSITION_OUT,
	float2 u		:UV,		 // coordinates in wind space used to compute P(u)
	float3 P		:P,			// wave point P(u) in world space
	float3 dPdu		:DPDU,		// dPdu in wind space, used to compute N
	float3 dPdv		:DPDV,		// dPdv in wind space, used to compute N
	float2 sigmaSq	:SIGMASQ,	// variance of unresolved waves in wind space
	float lod		:LOD
) : OUTPUT0 {

	float	d = dot(groundPlane, float4(P, 1));
	if (d < 0) {
		float3	V	= normalize(P - worldCamera);
		float	k	= IntersectPlane(P, V, groundPlane);
		return tex2D(bottom_tex, (P + k * V).xy / 8);
	}

	int iMAX = min(int(ceil((log2(nyquistMax * lod) - lods.z) * lods.w)), nbWaves - 1);
	int iMax = int(floor((log2(nyquistMin * lod) - lods.z) * lods.w));
	int iMin = max(0, int(floor((log2(nyquistMin * lod / lods.x) - lods.z) * lods.w)));

	for (int i = iMin; i <= iMAX; i++) {
		float4	wt		= waves[i];
		float	phase	= wt.y * time - dot(wt.zw, u);
		float	s		= sin(phase);
		float	c		= cos(phase);
		float	overk	= g / square(wt.y);

		float	wp		= smoothstep(nyquistMin, nyquistMax, 2 * M_PI * overk / lod);
		float	wn		= smoothstep(nyquistMin, nyquistMax, 2 * M_PI * overk / lod * lods.x);

		float3	factor = (1 - wp) * wn * wt.x * float3(wt.zw * overk, 1);

		float3	dPd = factor * float3(c, c, -s);
		dPdu -= dPd * wt.z;
		dPdv -= dPd * wt.w;

		if (i < iMax) {
			float kh	= wt.x / overk;
			float wkh	= (1 - wn) * kh;
			sigmaSq -= square(wt.zw * overk) * (sqrt(1 - square(wkh)) - sqrt(1 - square(kh)));
		}
	}
	sigmaSq = max(sigmaSq, 2e-5);

	float3 V	= normalize(P - worldCamera);
	float3 N	= WindToWorld(normalize(cross(dPdu, dPdv)));
	if (dot(V, N) > 0)
		N = reflect(N, V); // reflects backfacing normals

	float3 Ty	= normalize(cross(N, float3(-worldToWind2.y, worldToWind2.x, 0)));
	float3 Tx	= cross(Ty, N);

	float	fresnel = 0.02 + 0.98 * meanFresnel(V, N, sigmaSq);
	float3	color	= fresnel * meanSkyRadiance(V, N, Tx, Ty, sigmaSq);

#ifdef SEA_BOTTOM
	color += (1 - fresnel) * oceanBottom(P, V, N, Tx, Ty, sigmaSq);
#else
	color += (1 - fresnel) * seaColor;
#endif

	return float4(color, 1);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float4 mean_value_coords(float4 x, float4 y) {
	float4	A	= x * y.yzwx - y * x.yzwx;	//	s[i] x s[i+1]
	float4	D	= x * x.yzwx + y * y.yzwx;	//	s[i] . s[i+1]
	float4	r	= sqrt(x * x + y * y);		//	|s[i]|

	float4	rA	= A.yzwx * A.zwxy * A.wxyz;	//	avoid dividing by A by multiplying by all other A's
	float4	t	= (r * r.yzwx - D) * rA;	//	(r[i] r[i+1] - D[i]) / A[i]

	if (any(r == 0))
		return step(r, 0);
	float4	mu	= (t + t.wxyz) / r;	//	(t[i-1] + t[i]) / r[i]
	return mu / (mu.x + mu.y + mu.z + mu.w);
}

struct PatchVertex {
	 float2	v : POSITION;
};
struct PatchConsts {
	float		edge[4]		: TESS_FACTOR;
	float		inside[2]	: INSIDE_TESS_FACTOR;
//	float2		quad[4]		: QUAD;
};

PatchVertex VS_dummy(PatchVertex v) {
	return v;
}

// domain shader
DOMAIN_SHADER(quad) 
float4 DS_ocean(OutputPatch<PatchVertex, 4> patch, PatchConsts consts, float2 t : DOMAIN_LOCATION,
	out float2 u		: UV,		// coordinates in wind space used to compute P(u)
	out float3 P		: P,		// wave point P(u) in world space
	out float3 dPdu		: DPDU,		// dPdu in wind space, used to compute N
	out float3 dPdv		: DPDV,		// dPdv in wind space, used to compute N
	out float2 sigmaSq	: SIGMASQ,	// variance of unresolved waves in wind space
	out float lod		: LOD
) : POSITION_OUT {
//	float2		pos = mul(float3(t, 1), consts.grid2world);
	float4		bary	= mean_value_coords(float4(0,1,1,0) - t.x, float4(0,0,1,1) - t.y);
#if 0
	float2		pos		= consts.quad[0] * bary.x
						+ consts.quad[1] * bary.y
						+ consts.quad[2] * bary.w
						+ consts.quad[3] * bary.z;
#else
	float2		pos		= patch[0].v * bary.x
						+ patch[1].v * bary.y
						+ patch[2].v * bary.w
						+ patch[3].v * bary.z;
#endif
#if defined PLAT_PC || defined PLAT_XONE
	pos.y = -pos.y;
#endif
	return VS(float4(pos, 0, 1), u, P, dPdu, dPdv, sigmaSq, lod);
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

//	consts.quad[0] = patch[0].v;
//	consts.quad[1] = patch[1].v;
//	consts.quad[2] = patch[2].v;
//	consts.quad[3] = patch[3].v;

	return consts;
}

// hull shader
HULL_SHADER(HS_oceanC, quad, fractional_even, triangle_ccw, 4)
PatchVertex HS_ocean(InputPatch<PatchVertex, 4> patch, uint i : OUTPUT_CONTROL_POINT_ID) {
	return patch[i];
}

technique ocean {
    pass p0 {
		SET_VS(VS_dummy);
		SET_HS(HS_ocean);
		SET_DS(DS_ocean);
		SET_PS(PS);
	}
}

