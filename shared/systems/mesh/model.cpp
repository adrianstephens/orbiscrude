#include "maths/geometry.h"
#include "graphics.h"
#include "common/shader.h"
#include "shared/model_defs.h"
#include "model.h"
#include "shapes.h"
#include "light.h"
#include "render_object.h"
#include "shader.h"
#include "utilities.h"
#include "base/hash.h"
#include "base/algorithm.h"
#include "profiler.h"
#include "vector_string.h"

namespace iso {

#ifndef ISO_EDITOR
initialise	init(
	ISO::getdef<TexturePlat>(), ISO::getdef<SubMeshBase>(), ISO::getdef<SubMeshPlat>(), ISO::getdef<Texture>()
	#ifdef HAS_GRAPHICSBUFFERS
	, ISO::getdef<BufferPlat>(), ISO::getdef<DataBuffer>()
	#endif
);
#endif

//-----------------------------------------------------------------------------
//	Model3
//-----------------------------------------------------------------------------

struct S_comptype {
	const ISO::Type *iso;
	ComponentType	comp;
};
template<typename T> S_comptype make_comptype() { S_comptype t = {ISO::getdef<T>(), GetComponentType<T>()}; return t; }

static S_comptype known_types[] = {
	#define TYPE(x)		make_comptype<x>()

	TYPE(float),
	TYPE(float[2]),
	TYPE(float[3]),
	TYPE(float[4]),

	TYPE(unorm8[4]),

	TYPE(uint32),

	TYPE(int8[4]),

	TYPE(uint8[4]),
	TYPE(int16[2]),
	TYPE(int16[4]),
	TYPE(uint16[2]),
	TYPE(uint16[4]),
//	TYPE(unorm8[4]),

#ifndef PLAT_MAC
	TYPE(float16[2]),
	TYPE(float16[4]),
#endif

#ifndef PLAT_PS3
	TYPE(norm16[2]),
	TYPE(norm16[4]),
	TYPE(unorm16[2]),
	TYPE(unorm16[4]),
	TYPE(norm8[4]),
#endif

#ifdef PLAT_PC
	TYPE(double),
	TYPE(double[2]),
	TYPE(double[3]),
	TYPE(double[4]),
#endif

	#undef TYPE
	{ISO::getdef<uint8[3]>(), GetComponentType<unorm8[4]>()},
};

ComponentType GetComponentType(const ISO::Type *type) {
	for (int i = 0; i < num_elements(known_types); i++) {
		if (type->SameAs(known_types[i].iso))
			return known_types[i].comp;
	}
	return GetComponentType<void>();
}

#if 0//def PLAT_X360
#define MAX_INSTANCES	64

struct BatchData : Matrix {};

struct BatchVB : e_link<BatchVB>, VertexBuffer {
	BatchVB()	{ Create(MAX_INSTANCES * sizeof(BatchData)); }
};

struct Batcher : e_link<Batcher> {
	static	e_list<Batcher>			list;
	static	e_list<BatchVB>			pool[2];
	static	int							flip;
	static	int							poolsize;

	RenderSubMesh		*rsm;
	VertexBuffer		vb_indices;
	e_list<BatchVB>		vb_instances;
	int					i;

	Batcher(RenderSubMesh *_rsm, ISO_openarray<uint16[3]> indices) : rsm(_rsm), i(0) {
		int		n	= indices.Count() * 3;
		uint16	*s	= indices[0];
		uint32	*d	= (uint32*)vb_indices.Begin(n * 4);
		while (n--)
			*d++ = *s++;
		vb_indices.End();
	}

	void	Add(param(Matrix) world) {
		PROFILE_CPU_EVENT("Batcher Add");
		if (!next)
			list.push_back(this);
		if ((i % MAX_INSTANCES) == 0) {
			if (pool[flip].empty()) {
				pool[flip].push_back(new BatchVB);
				poolsize++;
			}
			vb_instances.push_back(pool[flip].pop_front());
		}

		((Matrix*)vb_instances.tail()->_Data())[i % MAX_INSTANCES] = world;
		++i;
	}
	void	Render(int mode);

	static void	RenderAll(int mode) {
//		PROFILE_CPU_EVENT("Batcher RenderAll");
		while (!list.empty())
			list.pop_front()->Render(mode);
		flip = 1 - flip;
	}
};

e_list<Batcher>	Batcher::list;
e_list<BatchVB>	Batcher::pool[2];
int				Batcher::flip;
int				Batcher::poolsize;
#endif

#if 0

bool IsBatched(ISO_ptr<Model3> &model)							{ return !!(((RenderSubMesh*)model.User())->flags & RMASK_BATCHING);	}
void AddBatch(ISO_ptr<Model3> &model, param(float3x4) world)	{ ((RenderSubMesh*)model.User())->AddBatch(world);		}
void DrawBatches(int mode)										{ Batcher::RenderAll(mode);}

#else

bool IsBatched(ISO_ptr<Model3> &model)							{ return false;}
void AddBatch(ISO_ptr<Model3> &model, param(float3x4) world)	{}
void DrawBatches(int mode)										{}

#endif

//-----------------------------------------------------------------------------

inline cuboid SubMeshBox(SubMeshBase *submesh) {
	return cuboid(
		position3(submesh->minext),
		position3(submesh->maxext)
	);
}

#ifdef ISO_EDITOR
#ifdef PLAT_MAC
void SubMeshPlat::Init(void *physram) {}
void SubMeshPlat::DeInit() {}
void SubMeshPlat::Render(GraphicsContext &ctx) {}
#else

ISO_ptr<void> RemoveDoubles(ISO_ptr<void> verts);

VertexElement* GetVertexElements(VertexElement *pve, const ISO::TypeComposite *vertex_type) {
	uint32			tex_index = 0;
	for (auto i : vertex_type->Components()) {
		ComponentType	type = GetComponentType(i.type.get());
		if (type != GetComponentType<void>()) {
			USAGE2	usage(i.id);

			if (usage == USAGE_TEXCOORD)
				tex_index	= max(tex_index, usage.index) + 1;
			else if (usage == USAGE_UNKNOWN)
				usage		= USAGE2(USAGE_TEXCOORD, tex_index++);

			*pve++ = VertexElement(int(i.offset), type, usage);
		}
	}
	return pve;
}

void SubMeshPlat::Init(void *physram) {
	if (!verts)
		return;

	renderdata	*rd = new renderdata;
	verts.User() = rd;

	PrimType	prim	= PRIM_TRILIST;
	uint8		vpp		= GetVertsPerPrim();
	uint8		vpp2	= vpp;

	if (vpp > 3)
		prim	= vpp == 4 ? PRIM_QUADLIST : PatchPrim(vpp);

	if (flags & STRIP) {
		vpp2	-= 2;
		prim	= StripPrim(prim);
	}
	if (flags & ADJACENCY) {
		vpp2	*= 2;
		prim	= AdjacencyPrim(prim);
	}

	ISO::Browser	b(RemoveDoubles(verts));
	rd->vert_size	= b[0].GetSize();
	rd->nverts		= b.Count();
	rd->nindices	= indices.Count() * vpp2;
	rd->prim		= prim;

	rd->vb.Init(b[0], rd->nverts * rd->vert_size);
	rd->ib.Init(indices[0].begin(), rd->nindices);

	VertexElement	ve[16];
	auto		pve = GetVertexElements(ve, (const ISO::TypeComposite*)b[0].GetTypeDef()->SkipUser());

#ifdef USE_DX11
	pass		*p	= (*technique)[0];
	const void	*vs	= p->sub[SS_VERTEX].raw();
	rd->vd.Init(ve, pve - ve, vs);
#else
	rd->vd = ve;
#endif
}
void SubMeshPlat::DeInit() {
	if (verts && verts.Header()->refs == 0xffff) {
		delete (renderdata*)verts.User().get();
		verts.User() = 0;
	}
}
void SubMeshPlat::Render(GraphicsContext &ctx) {
	PROFILE_CPU_EVENT("SubMeshPlat");
	if (!verts.User())
		Init(0);

	renderdata	*rd = (renderdata*)verts.User().get();
	ctx.SetVertexType(rd->vd);
	ctx.SetVertices(0, rd->vb, rd->vert_size);
	ctx.SetIndices(rd->ib);
	ctx.DrawIndexedVertices(PrimType(rd->prim), 0, rd->nverts, 0, rd->nindices);
}

SubMeshPlat::renderdata *SubMeshPlat::GetRenderData() {
	if (!verts.User())
		Init(0);

	return (renderdata*)verts.User().get();
}


#endif
#endif

//-----------------------------------------------------------------------------
//	Combiner
//-----------------------------------------------------------------------------

class ModelShaderConstants {
	static hash_map<Model3*, ModelShaderConstants>	hash;

	ShaderConstants		*sc;

	static int CountPasses(Model3 *model) {
	#if 1
		return model->submeshes.Count() * 4;
	#else
		int	t = 0;
		for (ISO_openarray<SubMeshPtr>::iterator i = model->submeshes.begin(), ie = model->submeshes.end(); i != ie; ++i)
			t += (*i)->technique->Count();
		return t;
	#endif
	}

public:
	ModelShaderConstants() : sc(0)	{}
//	void	Set(uint32 key)			{ sc = make((Model3*)key, ISO::Browser()); }
//	void	Clear(uint32 key)		{ delete_array<ShaderConstants>(sc, CountPasses((Model3*)key));	}

	static ShaderConstants* get(World *w, Model3 *m) {
		ShaderConstants *&sc = hash[m]->sc;
		if (!sc) {
			auto	*c = w->GetItem<ShaderConsts>(crc32());
			sc = make(m, ISO::MakeBrowser(*c));
		}
		return sc;
	}
	static void				remove(Model3 *m) {
		if (ModelShaderConstants *msc = hash.remove(m))
			delete_array(msc->sc, CountPasses(m));
	}
	static ShaderConstants*	make(Model3 *m, ISO::Browser b);
};

hash_map<Model3*, ModelShaderConstants>	ModelShaderConstants::hash;

ShaderConstants *ModelShaderConstants::make(Model3 *m, ISO::Browser b) {
	ShaderConstants	*sc = new_array<ShaderConstants>(CountPasses(m)), *sc1 = sc;
	for (ISO_openarray<SubMeshPtr>::iterator i = m->submeshes.begin(), ie = m->submeshes.end(); i != ie; ++i, sc1 += 4) {
		SubMeshBase		*sm		= *i;
		ShaderConstants	*sc2	= sc1;
		for (auto t = sm->technique->begin(), te = sm->technique->end(); t != te; ++t, ++sc2) {
//			sc2->Init(*t, b ? b : ISO::Browser(sm->parameters));
			if (b) {
				ISO::combiner<ISO::Browser,ISO::Browser>	combined(b, ISO::Browser(sm->parameters));
				sc2->Init(*t, ISO::MakeBrowser(combined));
			} else {
				sc2->Init(*t, ISO::Browser(sm->parameters));
			}
		}
	}
	return sc;
}

ISO_INIT(Model3)	{}
ISO_DEINIT(Model3)	{ ModelShaderConstants::remove(p); }

ShaderConstants *Bind(World *w, Model3 *m) {
	return ModelShaderConstants::get(w, m);
}

void PostFix(World *world, GraphicsContext &ctx, Model3 *m) {
	ShaderConstants	*sc = Bind(world, m);
#ifdef PLAT_OGL
	for (ISO_openarray<SubMeshPtr>::iterator i = m->submeshes.begin(), ie = m->submeshes.end(); i != ie; ++i, sc += 4) {
		SubMeshPlat	*sm	= (SubMeshPlat*)*i;
		if (sm->vao == 0) {
			ShaderConstants	*sc2 = sc;
			for (anything::iterator t = sm->technique->begin(), te = sm->technique->end(); t != te; ++t, ++sc2) {
				sc2->Set(ctx, *t);
				sm->Render(ctx);
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
//	RenderModelObject
//-----------------------------------------------------------------------------
/*
struct LookupBrowser : ISO::VirtualDefaults {
	Object			*obj;
	LookupMessage	message;

	int			GetIndex(tag2 &id) {
		message.id		= id.get_crc32();
		message.result	= 0;
		for (Object *o = obj; o; o = o->Parent()) {
			o->Send(message);
			if (message.result)
				return 0;
		}
		return -1;
	}
	ISO::Browser	Index(int i) {
		return ISO::Browser(message.type, message.result);
	}
	LookupBrowser(Object *_obj) : obj(_obj), message(crc32(), 1) {}
};
*/
struct RenderModelObject : public RenderObject {
	ISO_ptr<Model3>			model;
	RenderParameters		params;
	ShaderConstants			*shader_constants;
	float3x4				world;

	cuboid GetBox() const				{ return cuboid(position3(model->minext), position3(model->maxext)); }
	void operator()(DestroyMessage &m)	{ delete this; }
	void operator()(MoveMessage &m)		{ m.Update(Move((obj->GetWorldMat() * GetBox()).get_box()));	}

	RenderModelObject(World *world, Object *_obj, const ISO_ptr<Model3> &_model)
		: RenderObject(this, _obj)
		, model(_model), shader_constants(0)
	{
		_obj->AddHandler<LookupMessage>(this);
		_obj->flags.clear(Object::LOOKUPEVENT);
		shader_constants	= ModelShaderConstants::get(world, _model);

		for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
			SubMeshBase	*sm	= model->submeshes[i];
			sm->flags = (sm->flags & ~RMASK_EDGES) | (sm->technique->Count() > 2 ? RMASK_EDGES : 0);
		}
	}

	void operator()(LookupMessage &m) {
		if (m.flags & 1)
			return;
		PROFILE_CPU_EVENT("RMO Lookup");
		for (int i = 0; i < model->submeshes.Count(); i++) {
			ISO_ptr<SubMeshBase> sm	= model->submeshes[i];
			ISO::Browser	b(sm->parameters), b2;
			if (b2 = b.GetMember(m.id)) {
				m.set(b2);
				m.flags |= 1;
				break;
			}
		}
	}

	void operator()(RenderEvent *re, uint32 extra) {
		re->consts.SetWorld(world);
		re->consts.tint.a	= params.opacity;

	#if 0
		if (params.flags != re->flags) {
		#ifdef PLAT_PS3
			re->ctx.SetBlendConst(colour(half, half, half, int(params.flags & RF_MOTIONBLUR))); // multiply colour by .5 so we can get 0.0-2.0 LDR bloom on PS3
		#elif !defined PLAT_WII
			re->ctx.SetBlendConst(colour(one, one, one, int(params.flags & RF_MOTIONBLUR)));
		#endif
			re->flags = params.flags;
		}
	#endif
	#ifdef PLAT_PS3
		re->ctx.EDGESetWorld(re->world);
	#endif

//		PROFILE_CPU_EVENT(PROFILE_DYNAMIC(buffer_accum<256>() << GetLabel(model.ID()) << world.translation()));
		buffer_accum<256>	b;
		b << hex(intptr_t(&model)) << " - " << extra;
		AlwaysProfileGpuEvent	pfl(re->ctx, b);

		int			index	= extra & 0xff;
		SubMeshBase	*sm		= model->submeshes[index];
		if (Pose *pose = obj->Property<Pose>()) {
			int	n = pose->Count();
			if (!pose->skinmats)
				pose->GetSkinMats(re->ctx.allocator().alloc<float3x4>(n));
			SetSkinning(pose->skinmats, n);
		}

		int	pass = (extra >> 8) & 3;
		if (pass < sm->technique->Count()) {
			if (shader_constants)
				shader_constants[index * 4 + pass].Set(re->ctx, (*sm->technique)[pass]);
			((SubMeshPlat*)sm)->Render(re->ctx);
		}
	}

	void	operator()(RenderCollector &rc) {
		ISO_ASSERT(obj);

		if (obj->flags.test_clear(Object::LOOKUPEVENT)) {
//			if (count(obj->FindEvent<LOOKUP_EVENT>()->list) > 1)
//				shader_constants = ModelShaderConstants::make(model, ISO::MakeBrowser(LookupBrowser(obj)));
//			else
				shader_constants = ModelShaderConstants::get(World::Current(), model);
		}

		params	= rc;
		obj->Send(RenderMessage(params, rc.Time()));

		if (params.opacity) {
			uint32	xtra	= params.opacity < 1 ? RMASK_FADING : 0;
			if (!rc.Test(xtra))
				return;

			world =		obj->GetWorldMat();
			float4x4	worldViewProj(rc.viewProj * world);
			cuboid		box(position3(model->minext), position3(model->maxext));

			if (rc.completely_visible || is_visible(box, worldViewProj)) {
				RenderEvent	*re = rc.re;
				for (int j = 0, n = model->submeshes.Count(); j < n; j++) {
					uint32	f = model->submeshes[j]->flags;
					if (rc.Test(f)) {
						f	= rc.Adjust(f | xtra);
						re->AddExtent(box);

					#ifdef PLAT_IOS

						uint32	d = (uint32&)model->submeshes[j];
						uint32	o = f & RMASK_DRAWLAST ? 2 : f & RMASK_DRAWFIRST ? 0 : 1;
						if (rc.mask & RMASK_NOSHADOW) {
							re->AddRenderItem(this, MakeKey(f & RMASK_USETEXTURE ? RS_TRANSP : RS_OPAQUE + o, d), j | 0x100);
						} else if (f & (RMASK_SORT | RMASK_FADING)) {
							float	d = rc.Distance(position3(SubMeshBox(model->submeshes[j]).centre() + get_trans(world)));
							if (!(f & RMASK_SORT))
								re->AddRenderItem(this, MakeKey(RS_ZONLY, 1), j | 0x100);
							re->AddRenderItem(this, MakeKey(RS_TRANSP, -d), j);
						} else {
							if (f & RMASK_EDGES)
								re->AddRenderItem(this, MakeKey(RS_EDGES, d), j | 0x200);
							re->AddRenderItem(this, MakeKey(RS_OPAQUE + o, d), j);
						}

					#else

						float	d = rc.Distance(position3(SubMeshBox(model->submeshes[j]).centre() + get_trans(world)));
						if (rc.mask & RMASK_NOSHADOW) {
							re->AddRenderItem(this, MakeKey(f & RMASK_USETEXTURE ? RS_TRANSP : RS_OPAQUE, d), j | 0x100);
						} else if (f & (RMASK_SORT | RMASK_FADING)) {
							if (!(f & RMASK_SORT))
								rc.re->AddRenderItem(this, MakeKey(RS_ZONLY, 1), j | 0x100);
							re->AddRenderItem(this, MakeKey(f & RMASK_DRAWLAST ? RS_TRANSP + 8 : RS_TRANSP, -d), j);
						} else {
							if (f & RMASK_EDGES)
								re->AddRenderItem(this, MakeKey(RS_EDGES, d), j | 0x200);
						#ifdef PLAT_WII
							re->AddRenderItem(this, MakeKey(flags & RMASK_UPPERSHADOW ? RS_OPAQUE_UPPERSHADOW : flags & RMASK_MIDDLESHADOW ? RS_OPAQUE_MIDDLESHADOW : RS_OPAQUE_LOWERSHADOW, d), j);
						#else
							re->AddRenderItem(this, f & RMASK_DRAWLAST ? MakeKey(RS_OPAQUE, 0xfffffe) : MakeKey(RS_OPAQUE, d), j);
						#endif
						}

					#endif
					}
				}
			}
		}
	}
};

void Draw(GraphicsContext &ctx, ISO_ptr<Model3> &model) {
	PROFILE_EVENT(ctx, GetLabel(model.ID()));

	ShaderConstants		*sc = ModelShaderConstants::get(World::Current(), model);
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		SubMeshBase	*sm = model->submeshes[i];
		sc[i * 4].Set(ctx, (*sm->technique)[0]);
		((SubMeshPlat*)sm)->Render(ctx);
	}
}

template<> void TypeHandler<Model3>::Create(const CreateParams &cp, crc32 id, const Model3 *t) {
	RenderModelObject	*rm = new RenderModelObject(cp.world, cp.obj, ISO::GetPtr(t));
	RenderAddObjectMessage	m(rm, rm->GetBox(), cp.obj->GetWorldMat());
	cp.world->Send(m);
	cp.world->AddBox(m.box);
}
TypeHandler<Model3>		thModel3;


template<typename S> struct RenderShapeModel : RenderObject, ShapeModel<S> {
	float3x4	world;

	RenderShapeModel(World *world, Object *obj, const ShapeModel<S> &s) : RenderObject(this, obj), ShapeModel<S>(s) {}

	cuboid	GetBox()	const			{ return S::get_box(); }
	void operator()(DestroyMessage &m)	{ delete this; }
	void operator()(MoveMessage &m)		{ m.Update(Move((obj->GetWorldMat() * GetBox()).get_box()));	}

	void	operator()(RenderCollector &rc) {
		world			= obj->GetWorldMat();
		float4x4	worldViewProj(rc.viewProj * world);
		cuboid		box	= S::get_box();

		if (rc.completely_visible || iso::is_visible(box, worldViewProj)) {
			RenderEvent	*re = rc.re;
			re->AddExtent(box);

			float	d = rc.Distance(position3(S::centre() + get_trans(world)));
			re->AddRenderItem(this, MakeKey(RS_OPAQUE, d), 0);
		}
	}

	void operator()(RenderEvent *re, uint32 extra) {
		re->consts.tint	= this->col;
		Draw(re, *(S*)this, this->tech, world);
	}

};

template<typename S> struct TypeHandlerShapeModel : TypeHandler<ShapeModel<S>> {
	void	Create(const CreateParams &cp, crc32 id, const ShapeModel<S> *p) {
		RenderShapeModel<S>	*rm = new RenderShapeModel<S>(cp.world, cp.obj, *p);
		RenderAddObjectMessage	m(rm, rm->GetBox(), cp.obj->GetWorldMat());
		cp.world->Send(m);
		cp.world->AddBox(m.box);
	}
	TypeHandlerShapeModel() : TypeHandler<ShapeModel<S>>(this) {}
};

TypeHandlerShapeModel<cuboid>		th_cuboid;
TypeHandlerShapeModel<sphere>		th_sphere;
TypeHandlerShapeModel<cylinder>		th_cylinder;
TypeHandlerShapeModel<cone>			th_cone;
TypeHandlerShapeModel<obb3>			th_obb3;
TypeHandlerShapeModel<circle3>		th_circle3;
#if 0
TypeHandlerShapeModel<ellipsoid>	th_ellipsoid;
TypeHandlerShapeModel<tetrahedron>	th_tetrahedron;

//TypeHandlerShape<circle>		th_circle;
//TypeHandlerShape<ellipse>		th_ellipse;
//TypeHandlerShape<rectangle>	th_rectangle;
#endif
}	// namespace iso
