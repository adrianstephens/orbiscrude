#include "lensflare.h"
#include "post.h"
#include "iso/iso.h"
#include "utilities.h"
#include "shader.h"
#include "object.h"
#include "graphics.h"

using namespace iso;

typedef uint16 AtlasRegion[4];

struct LensSprite {
	AtlasRegion			region;
	float 				t;
	float 				scale;
	float3p				colour;
	float				falloff;
};
struct LensFlare {
	Texture						tex;
	ISO_openarray<LensSprite>	sprites;
};

struct LensFlareTestVertex {
	float2p		pos;
	float3p		uv;
};
struct LensFlareVertex {
	float2p		pos;
	float4p		uv;
	float4p		col;
};

ISO_DEFUSERCOMPV(LensSprite, region, t, scale, colour, falloff);
ISO_DEFUSERCOMPV(LensFlare, tex, sprites);

// ----------------------------------------------------------------------------

namespace iso {
template<> VertexElements GetVE<LensFlareTestVertex>() {
	static VertexElement ve[] = {
		VertexElement(&LensFlareTestVertex::pos,	"position"_usage),
		VertexElement(&LensFlareTestVertex::uv,		"texcoord"_usage)
	};
	return ve;
};

template<> VertexElements GetVE<LensFlareVertex>() {
	static VertexElement ve[] = {
		VertexElement(&LensFlareVertex::pos,		"position"_usage),
		VertexElement(&LensFlareVertex::uv,			"texcoord"_usage),
		VertexElement(&LensFlareVertex::col,		"colour"_usage)
	};
	return ve;
};
}
// ----------------------------------------------------------------------------

const int MAX_LENS_FLARES = 256;

#if defined(PLAT_X360) || defined(PLAT_PC) || defined(PLAT_XONE)
#define LFTEX_FORMAT	TEXF_A8R8G8B8
#define LFTEX_WIDTH		MAX_LENS_FLARES
#define LFTEX_HEIGHT	4
#elif defined(PLAT_PS3)
#define LFTEX_FORMAT	TEXF_A8R8G8B8_LIN
#define LFTEX_WIDTH		MAX_LENS_FLARES
#define LFTEX_HEIGHT	4
#elif defined(PLAT_IOS) || defined(PLAT_MAC) || defined(PLAT_ANDROID)
#define LFTEX_FORMAT	TEXF_R8G8B8A8
#define LFTEX_WIDTH		MAX_LENS_FLARES
#define LFTEX_HEIGHT	16
#elif defined(PLAT_PS4)
#define LFTEX_FORMAT	TEXF_R8G8B8A8
#define LFTEX_WIDTH		MAX_LENS_FLARES
#define LFTEX_HEIGHT	4
#endif


struct LensFlareObject {
	float4		pos;	// could be dir
//	LensFlare	*lensflare;
	int			preset;
};

struct LensFlares
	: Handles2<LensFlares, AppEvent>
	, Handles2<LensFlares, WorldEvent>
{
	ISO_ptr<void>					hold;
	ISO::OpenArrayView<LensFlare>	presets;
	dynamic_array<LensFlareObject>	lens_flare_objects;
	Texture							occlusion;
	float2							epsilon_line;
	ISO_ptr<fx>						shaders;

	void	SetPresets(ISO_ptr<ISO_openarray<LensFlare> > p) {
		if (hold = p)
			presets	= *p;
	}

	void operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN) {
			ISO::getdef<LensFlare>();
			SetPresets(ISO::root("data")["lensflare_presets"]);
			shaders = ISO::root("data")["lensflare"];
		}
	}
	void operator()(WorldEvent *ev) {
		if (ev->state == WorldEvent::END)
			lens_flare_objects.clear();
	}

	void	PreRender(GraphicsContext &ctx, param(float3x4) view, param(float4x4) proj);
	void	Render(GraphicsContext &ctx, param(float3x4) view, param(float4x4) proj, float brightness);

	void	Add(param(float4) pos, int preset) {
		LensFlareObject	*obj = new(lens_flare_objects) LensFlareObject;
		obj->pos		= pos;
		obj->preset		= preset;
//		obj->lensflare	= &presets[preset];
	}
} lens_flares;

// ----------------------------------------------------------------------------
void LensFlares::PreRender(GraphicsContext &ctx, param(float3x4) view, param(float4x4) proj) {
	dynamic_array<LensFlareTestVertex> vertices;
	for (int i = 0; i < lens_flare_objects.size(); i++) {
		float3	light_pos = view * lens_flare_objects[i].pos;
		if (light_pos.z > zero) {
			float3	light_proj = project(proj * light_pos);
			if (all(abs(light_proj.xy) < one)) {
				int	n = vertices.size32();
			#ifdef PLAT_X360
				n++;
			#endif
				LensFlareTestVertex *v = new (vertices) LensFlareTestVertex;
				v->pos = float2{float(n) / LFTEX_WIDTH * 2 - 1, .5f / LFTEX_HEIGHT * 2 - 1};
				v->uv = light_proj;
			}
		}
	}

	if (vertices.size()) {
		occlusion.Init(LFTEX_FORMAT, LFTEX_WIDTH, LFTEX_HEIGHT);
		ctx.SetRenderTarget(occlusion.GetSurface());
		Set(ctx, (*((*shaders)["lens_flare_test_occlusion"]))[0]);
		ImmediateStream<LensFlareTestVertex> ims(ctx, PRIM_POINTLIST, vertices.size32());
		memcpy(ims.begin(), vertices, vertices.size() * sizeof(LensFlareTestVertex));
	}
}

// ----------------------------------------------------------------------------
void LensFlares::Render(GraphicsContext &ctx, param(float3x4) view, param(float4x4) proj, float brightness) {
	dynamic_array<LensFlareVertex> vertices;

	float2	inv_atlas = reciprocal(to<float>(presets[0].tex.Size()));
	float2	aspect_ratio{proj.x.x, proj.y.y};

	int occ_test = 0;
	for (int i = 0; i < lens_flare_objects.size(); i++) {
		float3	light_pos = view * lens_flare_objects[i].pos;
		if (light_pos.z > zero) {
			float3	light_proj = project(proj * light_pos);
			if (all(abs(light_proj.xy) < one)) {

				// orient rotation matrix with light direction
				float2		u_dir		= all(light_proj.xy == zero) ? float2{one, zero} : normalise(light_proj.xy), v_dir = perp(u_dir);
				float2		diag1		= (u_dir - v_dir)  * aspect_ratio;
				float2		diag2		= (u_dir + v_dir)  * aspect_ratio;
				float3		light_view	= normalise(light_pos.xyz);

				LensFlare	*flare		= &presets[lens_flare_objects[i].preset];//lensflare;
				for (const LensSprite *i = flare->sprites.begin(), *e = flare->sprites.end(); i != e; ++i) {
					float alpha = (iso::pow(float(light_view.z), i->falloff) * 9 - 1) / 9;
					if (alpha > 0) {
						LensFlareVertex			*v = new(vertices) LensFlareVertex[verts<QuadListT>()];
						QuadListT<LensFlareVertex>	q(v);

						float2	pos		= light_proj.xy * i->t; // push/pull it relative to center of screen, which is (0, 0)
						float	scale	= i->scale;
						q[0].pos = pos - diag2 * scale;
						q[1].pos = pos + diag1 * scale;
						q[2].pos = pos - diag1 * scale;
						q[3].pos = pos + diag2 * scale;

						const AtlasRegion &region = i->region;
						float4	uv	= float4{float(region[0]), float(region[1]), float(region[0] + region[2]), float(region[1] + region[3])}
									* concat(inv_atlas, inv_atlas);
						float2	occ_test_coord{(occ_test + half) / LFTEX_WIDTH, .5f / LFTEX_HEIGHT};
						q[0].uv = concat(uv.xy, occ_test_coord);
						q[1].uv = concat(uv.zy, occ_test_coord);
						q[2].uv = concat(uv.xw, occ_test_coord);
						q[3].uv = concat(uv.zw, occ_test_coord);

						q[0].col = q[1].col = q[2].col = q[3].col = concat(i->colour * brightness, alpha);
					}
				}

				occ_test++;
			}
		}
	}

	if (vertices.size()) {
		AddShaderParameter(ISO_CRC("diffuse_samp", 0xe31becbe), presets[0].tex);
		AddShaderParameter(ISO_CRC("occlusion_samp", 0xeea9845c), occlusion);

		Set(ctx, (*((*shaders)["lens_flare"]))[0]);
		ctx.SetBlendEnable(true);

		ImmediateStream<LensFlareVertex> ims(ctx, prim<QuadListT>(), vertices.size32());
		memcpy(ims.begin(), vertices, vertices.size() * sizeof(LensFlareVertex));
		ctx.SetBlendEnable(false);
	}

	occlusion.DeInit();
}

// ----------------------------------------------------------------------------
namespace iso {
template<> void TypeHandlerCRC<ISO_CRC("LensFlareObject", 0x6064edb3)>::Create(const CreateParams &cp, crc32 id, const void *t) {
	ISO::Browser	b(ISO::GetPtr(t));
	lens_flares.Add(
		float4(position3(((float3x4p*)b["matrix"])->w)),
		b.GetMember(ISO_CRC("preset", 0x2c5fe432)).GetInt()
	);
}
TypeHandlerCRC<ISO_CRC("LensFlareObject", 0x6064edb3)> thLensFlareObj;
}

namespace iso {
template<> void TypeHandlerCRC<ISO_CRC("LensFlarePresets", 0x28e9444c)>::Create(const CreateParams &cp, crc32 id, const void *t) {
	lens_flares.SetPresets(ISO::GetPtr(t));
}
TypeHandlerCRC<ISO_CRC("LensFlarePresets", 0x28e9444c)> thLensFlarePresets;
}

// Render
void LensFlarePreRender(GraphicsContext &ctx, param(float3x4) view, param(float4x4) proj) {
	lens_flares.PreRender(ctx, view, proj);
}
void LensFlareRender(GraphicsContext &ctx, param(float3x4) view, param(float4x4) proj, float brightness) {
	lens_flares.Render(ctx, view, proj, brightness);
}
