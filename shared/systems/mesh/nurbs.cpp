#include "nurbs.h"
#include "maths/bspline.h"
#include "render_object.h"
#include "model.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	NurbsModel
//-----------------------------------------------------------------------------

#if defined PLAT_PS4 || defined USE_DX11

struct RenderSubPatch {
	ShaderConstants			sc[2];
	DataBufferT<float4p>	control;
	uint32					num_verts;
	RenderSubPatch(SubPatch &subpatch);
	void	Draw(GraphicsContext &ctx, SubPatch &subpatch, int pass);
};


RenderSubPatch::RenderSubPatch(SubPatch &subpatch) {
	ISO_openarray<float4p>	*verts = subpatch.verts;
	uint32	nu	= ISO::Browser(subpatch.parameters)["u_count"].GetInt();
	uint32	nc	= verts->Count();
	uint32	nv = nc / nu;

	num_verts = (nu - 3) * (nv - 3);//nc - 3(nu + nc / nu) + 9
	control.Init(verts->begin(), nc);

	for (int i = 0, nt = subpatch.technique->Count(); i < nt; i++)
		sc[i].Init((*subpatch.technique)[i], ISO::Browser(subpatch.parameters));
}

void RenderSubPatch::Draw(GraphicsContext &ctx, SubPatch &subpatch, int pass) {
	sc[pass].Set(ctx, (*subpatch.technique)[pass]);
	ctx.SetBuffer(SS_VERTEX, control, 0);
	ctx.DrawVertices(PatchPrim(16), 0, num_verts * 16);
}

struct RenderNurbsObject : public RenderObject {
	ISO_ptr<NurbsModel>		model;
	RenderParameters		params;

	cuboid				GetBox()	const { return cuboid(position3(model->minext), position3(model->maxext)); }

	RenderNurbsObject(Object *_obj, ISO_ptr<void> t) : RenderObject(this, _obj), model(t) {}

	void operator()(MoveMessage &m)		{ Move((obj->GetWorldMat() * GetBox()).get_box()); }
	void operator()(DestroyMessage &m)	{ delete this; }
	void operator()(RenderEvent *re, uint32 extra);
	void operator()(RenderCollector &rc);
};

void RenderNurbsObject::operator()(RenderEvent *re, uint32 extra) {
	PROFILE_CPU_EVENT("RenderPatchObject");
	re->consts.SetWorld(obj->GetWorldMat());

	int		pass = extra >> 8;
	for (int i = 0, n = model->subpatches.Count(); i < n; i++) {
		SubPatch		&subpatch	= model->subpatches[i];
		RenderSubPatch	*rsp		= (RenderSubPatch*)subpatch.verts.User().get();
	#ifdef ISO_EDITOR
		if (!rsp) {
			rsp = new RenderSubPatch(subpatch);
			subpatch.verts.User() = rsp;
		}
	#endif
		rsp->Draw(re->ctx, subpatch, pass);
	}
}

void RenderNurbsObject::operator()(RenderCollector &rc) {
	uint32				flags	= model->subpatches[0].flags;
	if (rc.Test(flags)) {
	//	Don't bother frustum culling because a single patch will span most of the level
	//	if (rc.completely_visible || rm->GetAABB().IsVisible(re->worldViewProj)) {
		flags	= rc.Adjust(flags);
		params	= rc;
		obj->Send(RenderMessage(params, rc.Time()));

		if (float opacity = params.opacity) {
			uint32 d = 1;
			uint32 j = 0;
			if (opacity < 1 || (flags & RMASK_SORT)) {
				if (!(flags & RMASK_SORT))
					rc.re->AddRenderItem(this, MakeKey(RS_ZONLY, 1), j | 0x100);
				rc.re->AddRenderItem(this, MakeKey(RS_TRANSP, 0 - d), j);
			} else if (flags & RMASK_DRAWLAST)
				rc.re->AddRenderItem(this, MakeKey(RS_OPAQUE, 0xffffff), j);
			else {
			#ifdef PLAT_WII
				rc.re->AddRenderItem(this, MakeKey(RS_OPAQUE_PATCHES, d), j);
			#else
				rc.re->AddRenderItem(this, MakeKey(RS_OPAQUE, d), j);
			#endif
			}
		}
	}
}

namespace iso {

void Draw(GraphicsContext &ctx, SubPatch &subpatch) {
	RenderSubPatch	*rsp	= (RenderSubPatch*)subpatch.verts.User().get();
#ifdef ISO_EDITOR
	if (!rsp) {
		rsp = new RenderSubPatch(subpatch);
		subpatch.verts.User() = rsp;
	}
#endif
	ctx.SetBuffer(SS_VERTEX, rsp->control, 0);
	ctx.DrawVertices(PatchPrim(16), 0, rsp->num_verts * 16);
}

void Draw(GraphicsContext &ctx, ISO_ptr<NurbsModel> &patch) {
	for (int i = 0, n = patch->subpatches.Count(); i < n; i++)
		Draw(ctx, patch->subpatches[i]);
}


template<> void TypeHandler<NurbsModel>::Create(const CreateParams &cp, crc32 id, const NurbsModel *t) {
	Object				*obj	= cp.obj;
	RenderNurbsObject	*ro		= new RenderNurbsObject(obj, ISO::GetPtr(t));
	cp.world->Send(RenderAddObjectMessage(ro, ro->GetBox(), obj->GetWorldMat()));
}

extern "C" TypeHandler<NurbsModel> thNurbsModel;

} // namespace iso

#endif
