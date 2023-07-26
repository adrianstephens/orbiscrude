#include "light.h"
#include "utilities.h"
#include "object.h"
#include "scenegraph.h"
#include "scenetree.h"
#include "graphics.h"
#include "render.h"

namespace iso {

const int NUM_SHADER_LIGHTS = 2;

struct LightingBlock {
	SH			sh;
	float3		main_dir;

	float3		shadow_dir;
	colour		shadow_col;

	float3		best_dir;
	colour		best_col;

	float4		fog_dir;
	colour		fog_col;

	float		best_val;

	void		Clear() {
		clear(*this);
		shadow_dir	= z_axis;
	}

	colour		GetAverage() const {
		return colour(sh.GetAverage().rgba + shadow_col.rgba * (9 / 16.f));
	}

	void	AdjustCols(param(float3x4) m) {
		sh.AdjustCols(m);
		shadow_col.rgb	= m * shadow_col.rgb;
		best_col.rgb	= m * best_col.rgb;
		fog_col.rgb		= m * fog_col.rgb;
	}

	LightingBlock()		{ Clear(); }

	void AddLightData(RenderEvent &re, Light *light, Object *obj) {
		switch (light->type) {
			case ent::Light2::AMBIENT:
				sh.AddAmbient(colour(light->col));
				break;

			case ent::Light2::OMNI:
				break;

			case ent::Light2::OMNI | ent::Light2::SHADOW:
				if (len2(shadow_dir) == zero)
					break;
			case ent::Light2::DIRECTIONAL | ent::Light2::SHADOW:
				shadow_dir = obj->GetWorldMat() * light->matrix.z;
				shadow_col = light->col;
				break;

			case ent::Light2::DIRECTIONAL: {
				float3	dir = obj->GetWorldMat() * light->matrix.z;
				if (len2(light->col.rgb) == zero) { // black light is flag for setting lens flare direction
					main_dir = dir;
				} else {
					float	val = len2(light->col.rgb);
					if (val > best_val) {
						best_val	= val;
						best_dir	= light->matrix.z;
						best_col	= light->col;
					}
					sh.AddDir(dir, colour(light->col));
				}
				break;
			}

			case ent::Light2::FOG:
				fog_col	+= colour(light->col.rgb, light->spread);
				fog_dir	= concat(re.consts.iview.z, re.consts.view.w.z - (obj->GetWorldMat() * light->matrix.w).y) / light->range;
				break;
		}
	}
};

//-----------------------------------------------------------------------------
//	Light
//-----------------------------------------------------------------------------

SceneTree		omni_tree;

Light MakeLight(const ent::Light2 *t) {
	return Light(t->type, colour(t->colour.v, one), t->range, t->spread, t->matrix);
}

struct ShaderLight {
	float4			pos;
	colour			col;
	void Set(param(position3) _pos, param(colour) _col, float range) {
		pos = concat(_pos.v, 19.f / square(range));
		col = _col;
	}
};

struct LightInt : public e_link<LightInt>, DeleteOnDestroy<LightInt>, Light, aligner<16> {
	crc32			id;
	ObjectReference	obj;
	position3		pos;
	SceneNode		*node;

	LightInt(Object *_obj, crc32 _id, const Light &light)
		: DeleteOnDestroy<LightInt>(_obj)
		, Light(light), id(_id), obj(_obj), node(0)
	{
		if (_obj) {
			pos = _obj->GetWorldMat() * get_trans(matrix);
			_obj->AddHandler<LookupMessage>(this);
			_obj->SetHandler<MoveMessage>(this);
		}
	}

	~LightInt() {
		if (node)
			node->Destroy();
	}

	float CalcAttenuation(float d2) const {
		float	r2 = square(range);
		return max(r2 / (19.f * d2 + r2) - 0.05f, zero);
	}

	float CalcAttenuation(param(position3) pos2) const {
		return CalcAttenuation(len2(pos - pos2));
	}

	void Update(const Light *light) {
		col		= light->col;
		range	= light->range;
		spread	= light->spread;
		matrix	= light->matrix;
		if (node) {
			pos = obj->GetWorldMat() * get_trans(matrix);
			omni_tree.Move(cuboid::with_centre(pos, float3(range)), node);
		}
	}

	void operator()(LookupMessage &m) {
		if (m.id == ISO_CRC("colour", 0xfaf865ce))
			m.result = &col;
		else if (m.id == ISO_CRC("range", 0x93875a49))
			m.result = &range;
	}

	void operator()(MoveMessage &m) {
		if (node) {
			pos = obj->GetWorldMat() * get_trans(matrix);
			omni_tree.Move(cuboid::with_centre(pos, float3(range)), node);
		}
	}
};

class LightHandler {//: public DeleteOnDestroy<LightHandler> {
	static CreateWithWorld<LightHandler> maker;
	e_list<LightInt>	lights;

	ShaderLoop			num_lights;
	ShaderLight			shaderlights[NUM_SHADER_LIGHTS];
	float3				sunlight_dir;
	float4				fog_dir2;
	colour				fog_col2;

	LightingBlock		block;
	bool				platform_update;

#if 0
	struct Intersect {
		const LightInt*	light;
		float 			d2;
		Intersect(const LightInt* _light, float _d2) : light(_light), d2(_d2)	{}
	};

	static bool Sorter(Intersect &a, Intersect &b)	{
		return a.d2 > b.d2;
	};
	void SetBest(RenderObject *light_holder, SH &sh, ShaderLight *shaderlights, int max_lights) {
		static dynamic_array<Intersect> lints;

		if (const dynamic_array<const LightInt*> *lights = light_holder->GetIntersectedLights()) {
			position3	centre = light_holder->GetNode()->box.centre();
			lints.clear();
			for (dynamic_array<const LightInt*>::const_iterator i = lights->begin(), end = lights->end(); i != end; ++i)
				new (lints) Intersect(*i, len2((*i)->pos.xyz - centre));

			int	n	= lints.end() - lints.begin();
			if (n > max_lights) {
				firstn(lints.begin(), lints.end(), max_lights + 1, Sorter);
				selection_sort(lints.begin(), lints.end() + max_lights + 1, Sorter);

				const LightInt *light	= lints[max_lights - 1].light;
				float scale	= (lints[max_lights - 1].d2 - lints[max_lights - 2].d2) / (lints[max_lights].d2 - lints[max_lights - 2].d2);
				float atten	= light->CalcAttenuation(lints[max_lights - 1].d2);
				sh.AddDir(normalise(light->pos.xyz - centre), colour(light->col * atten * scale));

				shaderlights[max_lights - 1].Set(light->pos, light->col, light->range);

				for (int i = max_lights; i < n; i++) {
					const LightInt *light = lints[i].light;
					float atten = light->CalcAttenuation(lints[i].d2);
					sh.AddDir(normalise(light->pos.xyz - centre), colour(light->col * atten));
				}
				n = max_lights - 1;
			}

			for (int i = 0; i < n; i++) {
				const LightInt *light = lints[i].light;
				shaderlights[i].Set(light->pos, light->col, light->range);
			}
		}
	}
#endif
	void	Add(Object *obj, crc32 id, const Light &l) {
		LightInt	*light	= new LightInt(obj, id, l);
		if (l.type == ent::Light2::OMNI)
			omni_tree.Insert(cuboid::with_centre(light->pos, float3(light->range)), light);
		lights.push_back(light);
	}

	void	SetSH(float3p *v) {
		float4 v2[9];
		for (int i = 0; i < 9; i++)
			v2[i] = concat(v[i], one);
		block.sh.AddFromSH(v2);
	}

public:
	void	operator()(GetSetMessage &m) {
		if (m.type == "struct iso::Light") {
			for (e_list<LightInt>::iterator i = lights.begin(), e = lights.end(); i != e; ++i) {
				if (i->id == m.id) {
					if (m.p) {
						i->Update((Light*)m.p);
						return;
					} else {
						m.p = (Light*)i.get();
						return;
					}
				}
			}
			int	type;
			switch (m.id) {
				case "background"_crc32:			type = ent::Light2::BACKGROUND; break;
				case ISO_CRC("shadow", 0x7576822d): type = ent::Light2::DIRECTIONAL | ent::Light2::SHADOW; break;
				case ISO_CRC("ambient", 0x4311e2c8):type = ent::Light2::AMBIENT; break;
				case ISO_CRC("fog", 0x82a8ed13):	type = ent::Light2::FOG; break;
				default:
					return;
			}
			for (e_list<LightInt>::iterator i = lights.begin(), e = lights.end(); i != e; ++i) {
				if (i->type == type) {
					if (m.p)
						i->Update((Light*)m.p);
					else
						m.p = (Light*)i.get();
				}
			}
		}
	}

	void	operator()(RenderEvent &re) {
		block.Clear();
		for (e_list<LightInt>::iterator i = lights.begin(), e = lights.end(); i != e; ++i) {
			switch (i->type) {
				case ent::Light2::WORLD_FOG:
					fog_dir2 = concat(i->obj->GetWorldMat() * i->matrix.z, zero) / i->range;
					fog_col2 += colour(i->col.rgb, i->spread);
					break;
				default:
					block.AddLightData(re, i.get(), i->obj.get());
			}
		}
//		if (re.Required(RMASK_ANAGLYPH))
//			block.AdjustCols(ColourAdjust(0, 1, 1).GetMatrix());

		re.consts.average = block.GetAverage();
	}

	void	Create(const CreateParams &cp, crc32 id, const ent::Light2 *p) {
		Add(cp.obj, id, MakeLight(p));
	}
	void	Create(const CreateParams &cp, crc32 id, const Light *p) {
		Add(cp.obj, id, *p);
	}
	void	Create(const CreateParams &cp, crc32 id, const ent::SphericalHarmonics *p) {
		SetSH(**p);
	}

	LightHandler(World *w)
//	: DeleteOnDestroy<LightHandler>(w)
//	, Creator<LightHandler, ent::Light2>(w)
//	, Creator<LightHandler, Light>(w)
//	, Creator<LightHandler, ent::SphericalHarmonics>(w)
	{
		w->AddHandler<RenderEvent>(this);
		w->AddHandler<GetSetMessage>(this);
		w->AddHandler<CreateMessage>(Creation<Light>(this));
		w->AddHandler<CreateMessage>(Creation<ent::Light2>(this));
		w->AddHandler<CreateMessage>(Creation<ent::SphericalHarmonics>(this));

		AddShaderParameter(ISO_CRC("diffuse_irradiance", 0x24f51ff8),block.sh);
		AddShaderParameter(ISO_CRC("num_lights", 0x819de550),		num_lights);
		AddShaderParameter(ISO_CRC("lights", 0x38bcb2e8),			shaderlights);

		AddShaderParameter(ISO_CRC("shadowlight_dir", 0x556d461b),	block.shadow_dir);
		AddShaderParameter(ISO_CRC("shadowlight_col", 0xfc77ca7b),	block.shadow_col);
		AddShaderParameter(ISO_CRC("fog_dir1", 0x1e7fa614),			block.fog_dir);
		AddShaderParameter(ISO_CRC("fog_col1", 0x5364ddc0),			block.fog_col);
	}

//	~LightHandler() {
//		lights.deleteall();
//	}
};

CreateWithWorld<LightHandler> LightHandler::maker;

}//iso
