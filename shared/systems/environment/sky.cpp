#include "base/defs.h"
#include "base/vector.h"
#include "graphics.h"
#include "shader.h"
#include "render.h"
#include "postprocess/post.h"
#include "utilities.h"

using namespace iso;

struct Sky : Handles2<Sky, AppEvent>, Handles2<Sky, WorldEvent> {
	static PtrBrowser	data;
	static pass			*clouds_pass, *sky_pass, *skymap_pass;
	static Texture		transmittanceTex, irradianceTex, inscatterTex, noiseTex;

	Texture				skyTex;
	spherical_dir		sun;

	// CLOUDS
	float	octaves, lacunarity, gain, norm, clamp1, clamp2;
	colour	cloudColor;

	// RENDERING OPTIONS
	float	hdrExposure;

	void	InitStatic();

	void	operator()(RenderEvent &re) {
		re.AddRenderItem(this, _MakeKey(RS_PRE, 0), 0);
	}
	void	operator()(RenderEvent *re, uint32 extra);
	void	operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN)
			InitStatic();
	}
	void	operator()(WorldEvent *ev) {
		ev->world->AddHandler<RenderEvent>(this);
	}
} sky;

PtrBrowser	Sky::data;
pass		*Sky::clouds_pass, *Sky::sky_pass, *Sky::skymap_pass;
Texture		Sky::transmittanceTex, Water::irradianceTex, Water::inscatterTex, Water::noiseTex;


void Sky::InitStatic() {
	data				= root["data"]["sky"];

	ISO_browser2	fx	= data["fx"];
	clouds_pass			= *fx["clouds"][0];
	sky_pass			= *fx["sky"][0];
	skymap_pass			= *fx["skymap"][0];

	transmittanceTex	= *(Texture*)data["transmittance"];
	irradianceTex		= *(Texture*)data["irradiance"];
	inscatterTex		= *(Texture*)data["inscatter"];
	noiseTex			= *(Texture*)data["noise"];

	skyTex.Init(TEXF_R16G16B16A16F, 512, 512);

	octaves = 10;
	lacunarity = 2.2f;
	gain = 0.7f;
	norm = 0.5f;
	clamp1 = -0.15f;
	clamp2 = 0.2f;
	cloudColor = colour(1, 1, 1, 1);

	nyquistMin = 1.0f;
	nyquistMax = 1.5f;
	hdrExposure = 0.4f;
}

// ----------------------------------------------------------------------------
// CLOUDS
// ----------------------------------------------------------------------------

void Sky::operator()(RenderEvent *re, uint32 extra) {
	GraphicsContext	&ctx = re->ctx;
	PostEffects		post(ctx);

	float3 sun_dir = sun;
	mapping	m[] = {
		{"worldSunDir",			&sun_dir		},
		{"hdrExposure",			&hdrExposure	},
		{"octaves",				&octaves		},
		{"lacunarity",			&lacunarity		},
		{"gain",				&gain			},
		{"norm",				&norm			},
		{"clamp1",				&clamp1			},
		{"clamp2",				&clamp2			},
		{"cloudsColor",			&cloudColor		},
		{"skyIrradianceSampler",&irradianceTex	},
		{"inscatterSampler",	&inscatterTex	},
	};
	post.FullScreenTri(skymap_pass, mappings(m));
	post.FullScreenTri(clouds_pass, mappings(m));

	post.FullScreenTri(sky_pass, mappings(m2));
	post.FullScreenTri(clouds_pass, mappings(m));
}

