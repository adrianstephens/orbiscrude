#include "render.h"
#include "render_object.h"
#include "common/shader.h"
#include "profiler.h"
#include "base/algorithm.h"

namespace iso {

//template ISO::Type *ISO::getdef<ShaderConsts>();

//-----------------------------------------------------------------------------
//	Render Stage
//-----------------------------------------------------------------------------

class RenderStage : public HandlesGlobal<RenderStage, RenderEvent> {
public:
	void operator()(RenderEvent *re, uint32 stage) {
		switch (stage) {
			case RS_ZPASS:
				re->Next("Z PRE-PASS");
			#ifdef PLAT_X360
				for (int i0 = 0; i0 < 12; i0++) {
					int	i = (i0 & 8) * 2 + (i0 & 7);
					re->ctx.SetUVMode(i, U_WRAP | V_WRAP | W_CLAMP);
					re->ctx.SetTexFilter(i, TF_LINEAR_LINEAR_LINEAR);
				}
				re->ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
			#elif defined PLAT_PS3
				re->ctx.SetBlend(BLENDOP_ADD, BLEND_CONSTANT_COLOR, BLEND_INV_SRC_ALPHA);
				re->ctx.SetBlendConst(colour(half, half, half));	// multiply colour by .5 so we can get 0.0-2.0 LDR bloom on PS3
			#else
				re->ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
			#endif
				re->ctx.SetDepthTestEnable(true);
				re->ctx.SetDepthWriteEnable(true);
				re->ctx.SetBlendEnable(false);
				re->ctx.SetMask(CM_NONE);
				re->ctx.SetBackFaceCull(re->consts.view.det() < 0 ? BFC_FRONT : BFC_BACK);
				break;

			case RS_OPAQUE:
				re->Next("OPAQUE");
			#if !defined(PLAT_PC) && !defined(PLAT_WII)
				re->ctx.SetAlphaToCoverage(true);
			#endif
				re->ctx.SetMask(re->mask);
				re->ctx.SetBlendEnable(false);
			#ifdef ISO_HAS_ALPHATEST
				re->ctx.SetAlphaTest(AT_GREATER, 0);
				re->ctx.SetAlphaTestEnable(true);
			#endif
				break;

			case RS_ZONLY:
				re->Next("Z ONLY");
			#ifdef ISO_HAS_ALPHATEST
				re->ctx.SetAlphaTestEnable(false);
			#endif
				re->ctx.SetBlendEnable(false);
				re->ctx.SetMask(CM_NONE);
				break;

			case RS_TRANSP:
				re->Next("TRANSPARENT");
			#if !defined(PLAT_PC) && !defined(PLAT_WII)
				re->ctx.SetAlphaToCoverage(false);
			#endif
			#ifdef PLAT_X360
				re->ctx.Resolve(RT_DEPTH);
			#endif
				re->ctx.SetDepthWriteEnable(false);
				re->ctx.SetBlendEnable(true);
				re->ctx.SetMask(re->mask & ~CM_ALPHA);
				//TEST
				re->ctx.SetDepthTestEnable(false);
				break;
		}
	}

	void	operator()(RenderEvent &re) {
		if (!re.Excluded(RMASK_NOSHADOW)) {
			re.AddRenderItem(this, MakeKey(RS_ZPASS,	0), RS_ZPASS);
			re.AddRenderItem(this, MakeKey(RS_OPAQUE,	0), RS_OPAQUE);
			re.AddRenderItem(this, MakeKey(RS_ZONLY,	0), RS_ZONLY);
			re.AddRenderItem(this, MakeKey(RS_TRANSP,	0), RS_TRANSP);
		}
	}
} renderstage;

static_array<RenderEvent::RenderItem, 5000>	RenderEvent::items;

//-----------------------------------------------------------------------------
// RenderEvent
//-----------------------------------------------------------------------------

void RenderEvent::Collect(World *w) {
	PROFILE_CPU_EVENT("RC callbacks");

	w->Global()->Send(*this);
	w->Send(*this);

	PROFILE_CPU_EVENT_NEXT("RC sort");
	sort(items);
}

void RenderEvent::PostRender() {
#ifdef PLAT_WII
	ctx.ClearViewProj();
#endif
}

void RenderEvent::SetShaderParams() {
#ifdef PLAT_PS3
	ctx.EDGESetViewProj(viewProj);
#endif
#ifdef PLAT_WII
	ctx.SetViewProj(viewProj);
	WiiLight	wii_light;
	wii_light.Set(light.shadow_col, light.shadow_dir, float3(one, zero, zero), float3(one, zero, zero));
	ctx.SetLight(0, wii_light);
	wii_light.Set(light.best_col, light.best_dir, float3(one, zero, zero), float3(one, zero, zero));
	ctx.SetLight(1, wii_light);
	ctx.EnableLights(3);
	colour	ambient(light.sh.GetAmbient().rgb, 1);
	ctx.SetAmbient(0, ambient);
	ctx.SetAmbient(1, ambient);
#endif
}

//-----------------------------------------------------------------------------
//	RenderObject
//-----------------------------------------------------------------------------

struct RenderObjectTree : SceneTree, DeleteOnDestroy<RenderObjectTree> {
	friend CreateWithWorld<RenderObjectTree>;
	static CreateWithWorld<RenderObjectTree> maker;

	bool	dirty;
	void	operator()(RenderEvent &re) {
		if (dirty) {
			CreateTree();
			dirty = false;
		}
		RenderCollector	rc(&re);

		if (!re.Required(RMASK_DYNAMIC)) {
			static items vis_list;
			static items comp_vis_list;

			PROFILE_CPU_EVENT("Static Culling");
			CollectFrustum(re.consts.viewProj0, vis_list, comp_vis_list);

			PROFILE_CPU_EVENT_NEXT("Static Processing");
			rc.completely_visible = false;
			rc.SendTo(vis_list);

			rc.completely_visible = true;
			rc.SendTo(comp_vis_list);
		}

		if (!re.Excluded(RMASK_DYNAMIC)) {
			PROFILE_CPU_EVENT("Dynamic Processing");
			rc.completely_visible	= false;
			rc.SendTo(GetList());
		}
		re.AddExtent(GetBox());
	}
	void	operator()(RenderAddObjectMessage &m) {
		m.ro->node = Insert(m.box, m.ro);
		dirty = true;
	}
	RenderObjectTree(World *w) : DeleteOnDestroy<RenderObjectTree>(w), dirty(false) {
		w->AddHandler<RenderEvent>(this);
		w->SetHandler<RenderAddObjectMessage>(this);
	}
};

CreateWithWorld<RenderObjectTree>	RenderObjectTree::maker;

RenderCollector::RenderCollector(RenderEvent *_re)
	: MaskTester(_re->exclude & ~RMASK_DYNAMIC, _re->require & ~RMASK_DYNAMIC)
	, view(_re->consts.view), viewProj(_re->consts.viewProj)
	, re(_re)
	, time(_re->consts.time), quality(_re->quality)
	, completely_visible(false)
{}

void RenderCollector::SendTo(const SceneTree::items &array) {
	for (auto &i : array)
		(*(RenderObject*)i)(*this);
}

void RenderCollector::SendTo(const SceneTree::List &list) {
	for (auto &i : list)
		(*(RenderObject*)i.data)(*this);
}

//-----------------------------------------------------------------------------
//	Splitter
//-----------------------------------------------------------------------------

void Collect(SceneTree::List &list, RenderCollector &rc, float opacity) {
	if (opacity > 0) {
		rc.opacity = opacity;
		rc.SendTo(list);
	}
}
void Collect(SceneTree &tree, RenderCollector &rc, float opacity) {
	if (opacity > 0) {
		rc.opacity = opacity;

		static SceneTree::items vis_list;
		static SceneTree::items comp_vis_list;

		tree.CollectFrustum(rc.viewProj, vis_list, comp_vis_list, rc.completely_visible ? 0 : 0x3f);
		rc.SendTo(vis_list);
		save(rc.completely_visible, true), rc.SendTo(comp_vis_list);
		rc.SendTo(tree.GetList());
	}
}

struct Splitter : public RenderObject {
	SceneTree			children[2];
	float				value, value2, value3;
	ent::Splitter::Decision		split_decision;

	void operator()(DestroyMessage &m)		{ delete this; }
	void operator()(MoveMessage &m)			{ Move((obj->GetWorldMat() * node->box).get_box()); }

	Splitter(Object *_obj, ent::Splitter::Decision _split_decision, float _value, float _value2, float _value3)
		: RenderObject(this, _obj)
		, value(_value), value2(_value2), value3(_value3)
		, split_decision(_split_decision)
	{}

	float3x4 GetWorldMat() const {
		if (obj)
			return obj->GetWorldMat();
		return identity;
	}

	float GetDistanceFade(RenderCollector &rc, param(float3x4) world, float comp_dist) const {
		float dist = (rc.view * (world * node->box).centre()).v.z;
		return clamp((1.f - dist / comp_dist) * 8.f, 0.f, 1.f);
	}

	float GetFade(RenderCollector &rc, param(float3x4) world) const { // returns -1 for nothing, 0 for lo-LOD, 1 for hi-LOD, or something "fuzzy" for a blend in-between
		if (children[1].Empty() || rc.quality == 0)
			return 0;

		switch(split_decision) {
			case ent::Splitter::Distance:				return GetDistanceFade(rc, world, value);
			case ent::Splitter::SubView:				return rc.Test1(RMASK_SUBVIEW) ? 0 : 1;
			case ent::Splitter::Distance_NoSub:			return rc.Test1(RMASK_SUBVIEW) ? -1 : GetDistanceFade(rc, world, value);
			case ent::Splitter::Distance_LoSub:			return rc.Test1(RMASK_SUBVIEW) ? 0  : GetDistanceFade(rc, world, value);
			case ent::Splitter::Quality_Distance:		return GetDistanceFade(rc, world, rc.quality >= value ? value3 : value2);
			case ent::Splitter::Quality_Distance_NoSub:	return rc.Test1(RMASK_SUBVIEW) ? -1 : GetDistanceFade(rc, world, rc.quality >= value ? value3 : value2);
			case ent::Splitter::Quality_Distance_LoSub:	return rc.Test1(RMASK_SUBVIEW) ? 0  : GetDistanceFade(rc, world, rc.quality >= value ? value3 : value2);
			default:
				//ISO_ASSERT(false);
				return 1;
		}
	}

	void operator()(RenderCollector &rc) {
		PROFILE_CPU_EVENT("Splitter");
		ISO_ASSERT(obj);

		float3x4	world = GetWorldMat();
//		if (rc.completely_visible || node->box.is_visible(RenderEvent::viewProj * world))	{
			float fade = GetFade(rc, world);
			if (fade >= 0.f) {
				float opacity = rc.opacity;
				Collect(children[0], rc, opacity * min((1 - fade) * 2, 1.f));
				Collect(children[1], rc, opacity * min(fade * 2, 1.f));
				rc.opacity = opacity;
			}
//		}
	}
};

//-----------------------------------------------------------------------------
//	Splitter
//-----------------------------------------------------------------------------

struct NodeListDest {
	Object		*obj;
	SceneTree	&tree;
	callback<void(RenderAddObjectMessage&)>	old;

	void	operator()(RenderAddObjectMessage &m) {
		m.ro->node = tree.Insert(m.box, m.ro);
		//m.ro->tree = &tree;
	}
	NodeListDest(Object *_obj, SceneTree &_tree) : obj(_obj), tree(_tree) {
		old = obj->SetHandler<RenderAddObjectMessage>(this);
	}
	~NodeListDest() {
		tree.CreateTree();
		obj->SetHandler(old);
	}
};

template<> void TypeHandler<ent::Splitter>::Create(const CreateParams &cp, crc32 id, const ent::Splitter* t) {
	Object		*obj		= cp.obj;
	Splitter	*splitter	= new Splitter(obj, (ent::Splitter::Decision)t->split_decision, t->value, t->value2, t->value3);
//	float3x4	mat			= splitter->GetWorldMat();

	NodeListDest(cp.world, splitter->children[0]), obj->AddEntities(t->lorez);
	NodeListDest(cp.world, splitter->children[1]), obj->AddEntities(t->hirez);

	cuboid	box	= splitter->children[0].GetBox() | splitter->children[1].GetBox();
	cp.world->Send(RenderAddObjectMessage(splitter, box, identity));
}
TypeHandler<ent::Splitter>	thSplitter;


//-----------------------------------------------------------------------------
//	QualityToggle
//-----------------------------------------------------------------------------

template<> void TypeHandler<ent::QualityToggle>::Create(const CreateParams &cp, crc32 id, const ent::QualityToggle *t) {
	cp.obj->AddEntities(cp.quality >= t->quality_threshold ? t->hirez : t->lorez);
}

TypeHandler<ent::QualityToggle>	thQualityToggle;

//-----------------------------------------------------------------------------
//	Cluster (data-wise it's a Cluster, code-wise it's a Splitter)
//-----------------------------------------------------------------------------

template<> void TypeHandler<ent::Cluster>::Create(const CreateParams &cp, crc32 id, const ent::Cluster *t) {
	Object		*obj		= cp.obj;
	Splitter	*splitter	= new Splitter(obj, ent::Splitter::Distance, t->distance, 0, 0);
//	float3x4	mat			= splitter->GetWorldMat();

	if (t->lorez)
		NodeListDest(cp.world, splitter->children[0]), obj->AddEntities(t->lorez);

	NodeListDest(cp.world, splitter->children[1]),
		t->hirez.IsType<Node>() ? obj->AddEntitiesArray(static_cast<Node*>(t->hirez)->children) : (void)obj->AddEntities(t->hirez);

	cuboid	box	= splitter->children[0].GetBox() | splitter->children[1].GetBox();
	cp.world->Send(RenderAddObjectMessage(splitter, box, identity));
}

TypeHandler<ent::Cluster>	thCluster;

}//namespace iso
