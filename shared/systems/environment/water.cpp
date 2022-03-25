#include "base/defs.h"
#include "base/vector.h"
#include "graphics.h"
#include "shader.h"
#include "render.h"
#include "object.h"
#include "vector_string.h"
#include "mesh/shapes.h"
#include "extra/random.h"
#include "utilities.h"

using namespace iso;

struct Water : public DeleteOnDestroy<Water> {
	static ISO::PtrBrowser	data;
	static pass			*ocean_pass;
	static Texture		*water_tex;
	static Texture		*bottom_tex;

	VertexBuffer<float2p>	vb;
	IndexBuffer<uint16>		ib;
	Buffer<float4>			waves;

	// RENDERING OPTIONS
	float	gridSize, nyquistMin, nyquistMax;
	colour	seaColor;
	plane	groundPlane;

	// WAVES PARAMETERS (INPUT)
	int		nbWaves;
	float	lambdaMin, lambdaMax, heightMax, waveDirection, waveDispersion, U0;

	// WAVE STATISTICS (OUTPUT)
	float2	sigmaSq;
	float	meanHeight;
	float3	waveTotal;

	void	InitStatic();
	void	GenerateWaves(float4 *waves);

	void	operator()(RenderEvent &re) {
		re.AddRenderItem(this, _MakeKey(RS_PRE, 1), 0);
	}
	void	operator()(RenderEvent *re, uint32 extra);

	Water(World *world) : DeleteOnDestroy<Water>(world) {
		if (!data)
			InitStatic();

		world->AddHandler<RenderEvent>(this);
	}

	~Water() {}
};

ISO::PtrBrowser	Water::data;
pass		*Water::ocean_pass;
Texture		*Water::water_tex;
Texture		*Water::bottom_tex;

void Water::InitStatic() {
	data			= ISO::root("data")["water_data"];
	ocean_pass		= *data["shader"][0];
	water_tex		= data["texture"][0];
	bottom_tex		= data["texture"][2];

	nbWaves			= 60;
	//lambdaMin		= 0.1f;//0.02f;
	lambdaMin		= 0.02f;
	lambdaMax		= 30;
	heightMax		= 0.32f;
	waveDirection	= pi / 2;//2.4f;
	U0				= 10.0f;
	waveDispersion	= 1.25f;

	gridSize		= 8.0f / 1024.f;
	nyquistMin		= 1.0f;
	nyquistMax		= 1.5f;
	seaColor		= colour(10, 40, 120, 255) * (0.1f / 255);

	groundPlane		= plane(extend_left<3>(sincos(degrees(85.f))), 0);

	waves.Init(nbWaves, MEM_CPU_WRITE);
	GenerateWaves(waves.WriteData());

	auto	x = waves.Size();
}

// ----------------------------------------------------------------------------
// WAVES GENERATION
// ----------------------------------------------------------------------------
/*
inline float gauss_random(Random &random, float mean, float std) {
	static float	y2;
	static bool		use_last = false;

	float x1, x2, w, y1;

	if (use_last) {
		y1 = y2;
		use_last = false;
	} else {
		do {
			x1	= random.from(-1.f, 1.f);
			x2	= random.from(-1.f, 1.f);
			w	= square(x1) + square(x2);
		} while (w >= 1);
		w	= sqrt((-2 * log(w)) / w);
		y1	= x1 * w;
		y2	= x2 * w;
		use_last = true;
	}
	return mean + y1 * std;
}
*/

//uv is plane position in wind space
float3 CalcWave(param(float4) wt, param(float2) uv, float time) {
	float	phase	= wt.y * time - dot(wt.zw, uv);
	return concat(wt.zw * 9.81f / square(wt.y) * sin(phase), cos(phase)) * wt.x;
}

void Water::GenerateWaves(float4 *waves) {
	rng<simple_random>	random(1234567);
	float	g		= 9.81f;
	float	log2lambdaMin		= log2(lambdaMin);
	float	log2lambdaMax		= log2(lambdaMax);

	sigmaSq			= zero;
	waveTotal		= zero;
	meanHeight		= 0.f;

#define nbAngles 5 // even
	float	dangle	= 3.f / nbAngles;
	float	angles[nbAngles];

	for (int i = 0; i < nbAngles; i++)
		angles[i]	= (3.f * (i / float(nbAngles) - 0.5f));

#if  0
	float	s = 0;
	for (int i = 0; i < nbAngles; i++)
		s += angles[i] = exp(-0.5* square(angles[i]));
	for (int i = 0; i < nbAngles; i++)
		angles[i] /= s;
#endif

	float	step		= (log2lambdaMax - log2lambdaMin) / (nbWaves - 1); // dlambda/di
	float	omega0		= g / U0;

	for (int i = 0; i < nbWaves; ++i) {
		if (i % nbAngles == 0) { // scramble angle order
			for (int k = 0; k < nbAngles; k++)
				swap(angles[random.to(nbAngles)], angles[random.to(nbAngles)]);
		}

		float	lambda			= pow(2.0f, log2lambdaMin + step * i);
		float	knorm			= 2 * pi / lambda;
		float	omega			= sqrt(g * knorm);
	#if 1
		float	ktheta			= waveDispersion * (angles[i % nbAngles] + random.from(-0.4f, 0.4f) * dangle) / (1 + 40 * pow(omega0 / omega, 4));
	#else
		float	ktheta			= waveDispersion * grandom(random, 0, 1);
	#endif
		float2	sincos_ktheta	= sincos(ktheta);

		float	amplitude		= 3 * heightMax * g * sqrt(8.1e-3f / pow(omega, 5) * exp(-0.74f * pow(omega0 / omega, 4)) * 0.5f * sqrt(knorm * g) * nbAngles * step);
		amplitude		= min(amplitude, 1 / knorm);

		waves[i]		= concat(amplitude, omega, knorm * sincos_ktheta);

		sigmaSq			+= square(sincos_ktheta) * (1 - sqrt(1 - square(knorm * amplitude)));
		meanHeight		-= knorm * square(amplitude) / 2;
		waveTotal		+= concat(abs(sincos_ktheta) * amplitude, amplitude);
	}
}

float3 householder(param(float3) p) {
	float3	t = p * sign(p.x);
	t.x += 1;//one;
	return t * rsqrt(t.x * len(p) * two);
}

float2 inverse2D(param(float2) r) {
	return {r.x, -r.y};
}
float2 rotate2D(param(float2) v, param(float2) r) {
	return r * v.x + perp(r) * v.y;
}

plane extend(param(plane) p, const ellipsoid &e) {
	float3x4	m	= e.matrix();
	float3		n	= p.normal();
	float		s0	= p.dist(get_trans(m));	// should be 0
	float3		w	= transpose((const float3x3&)m) * n;
	float		s	= len(w);
	return plane(n, s - s0);
}

void Water::operator()(RenderEvent *re, uint32 extra) {
	GraphicsContext	&ctx	= re->ctx;
	float2	worldToWind		= sincos(waveDirection);
	float2	windToWorld		= inverse2D(worldToWind);

	frustum		fr(inverse(re->consts.viewProj0));
	ellipsoid	el(position3(zero), concat(rotate2D(waveTotal.xy, windToWorld), zero), concat(rotate2D(perp(waveTotal.xy), windToWorld), zero), waveTotal.z);

	plane	fplanes[6];
	for (auto i : planes<3>())
		fplanes[i]	= extend(fr.plane(i), el);

	frustum		fr2 = frustum_from_planes(fplanes);
	float4x4	fr2b = (float4x4&)fr2 * (fr.x.x / fr2.x.x);//iviewProj0

	float	num = 64, du = 2 / num;

	quadrilateral quad = XYPlaneQuad(re->consts.viewProj0, du);

	float4	lods{
		gridSize * re->window.extent().x,
		atan(gridSize * 2),	// angle under which a screen pixel is viewed from the camera
		log2(lambdaMin),
		(nbWaves - 1) / (log2(lambdaMax) - log2(lambdaMin))
	};
	float3		sun{0,0,1};
	float		heightOffset	= -meanHeight;
	float		time			= re->consts.time;
	float		fnbWaves		= nbWaves;
	DataBuffer	buffer(waves);

	float4x4	screenToCamera	= re->consts.iproj;
//	float4x4	screenToCamera	= re->view * fr2b * inverse(hardware_fix(identity));
	float4x4	screenToWorld	= re->consts.iview * re->consts.iproj;
	position3	worldCamera1	= project(fr2b * float4{0,0,-1,0});

	ShaderVal	m[] = {
		{"worldToWind2",	&worldToWind	},
		{"nbWaves",			&nbWaves		},
		{"heightOffset",	&heightOffset	},
		{"sigmaSqTotal",	&sigmaSq		},
		{"lods",			&lods			},
		{"nyquistMin",		&nyquistMin		},
		{"nyquistMax",		&nyquistMax		},
		{"seaColor",		&seaColor		},
		{"waves",			&buffer			},
		{"water_tex",		water_tex		},
		{"bottom_tex",		bottom_tex		},
		{"time",			&time			},
		{"groundPlane",		&groundPlane	},

		{"screenToCamera",	&screenToCamera		},
		{"cameraToWorld",	&re->consts.iview	},
		{"worldToScreen",	&re->consts.viewProj},
		{"worldCamera1",	&worldCamera1		},
		{"worldCamera",		&re->consts.iview.w	},
		{"worldSunDir",		&sun				},

		{"screenToWorld",	&screenToWorld	},

	};
	ctx.SetDepthTestEnable(true);
	Set(ctx, ocean_pass, ISO::MakeBrowser(ShaderVals(m)));

//	ctx.SetBackFaceCull(BFC_NONE);
//	ctx.SetFillMode(FILL_WIREFRAME);
	{
		ImmediateStream<float2p>	im(ctx, PatchPrim(4), 4);
		float2p	*p = im.begin();
		p[0] = quad[0].v;
		p[1] = quad[1].v;
		p[2] = quad[2].v;
		p[3] = quad[3].v;
	}
	ctx.SetFillMode(FILL_SOLID);
}

namespace ent {
	using namespace iso;

	struct Water {
	};

} // namespace ent

namespace iso {

	template<> void TypeHandler<ent::Water>::Create(const CreateParams &cp, crc32 id, const ent::Water *t) {
		new Water(cp.world);
	}

	TypeHandler<ent::Water> thWater;

} // namespace iso
