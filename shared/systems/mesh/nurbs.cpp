#include "nurbs.h"
#include "render_object.h"
#include "model.h"

using namespace iso;

float4x4 NURBS3GetKnotMatrix(float *k) {
	float	k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5];

	float	d0	= (k0 - k3) * (k1 - k3) * (k2 - k3);
	float	d1	= d0 * (k1 - k4) * (k2 - k4);
	float	d3	= (k2 - k3) * (k2 - k4) * (k2 - k5);
	float	d2	= d3 * (k1 - k3) * (k1 - k4);
	float4	d	= reciprocal(float4{d0, d1, d2, d3});

	//			t^3		t^2			t				1
	float4	n0{ 1,		-3 * k3,	 3 * k3 * k3,	-k3 * k3 * k3};
	float4	n3{-1,		 3 * k2,	-3 * k2 * k2,	 k2 * k2 * k2};

	//from mathics
	//w1,1:		+01233+01234+01244-01334-01344-02334-02344+03344-12334-12344+13344+23344
	//w1,t:		-3(0123+0124-0134-0234-1234+3344)
	//w1,t2:	+3(012-034-134-234+334+344)
	//w1,t3:	-01-02+03+04-12+13+14+23+24-33-34-44
	//manually factored:
	//w1,1:		+012(33+44+34)+34(+34(0+1+2)-(3+4)(01+02+12))
	//w1,t:	-3(	+012(3+4)+34(34-(01+02+12))	)
	//w1,t2:+3(	+012-34(0+1+2-3-4)			)
	//w1,t3:	+(3+4-0)(1+2-3-4)-12+34

	//from mathics
	//w2,1:		-11223-11224-11225+11234+11235+11245-11345+12234+12235+12245-12345-22345
	//w2,t:		+3(1122-1234-1235-1245+1345+2345)
	//w2,t2:	-3(112+122-123-124-125+345)
	//w2,t3:	+11+12-13-14-15+22-23-24-25+34+35+45
	//manually factored:
	//w2,1:		-345(11+22+12)+12(-12(3+4+5)+(1+2)(+34+35+45))
	//w2,t:	+3(	+345(1+2)+12(12-34-35-45)	)
	//w2,t2:-3(	+345+12(1+2-3-4-5))			)
	//w2,t3:	+(1+2-5)(1+2-3-4)-12+34

	float	k12			= k1 * k2,				k1p2		= k1 + k2,			k012	= k0 * k12;
	float	k34			= k3 * k4,				k3p4		= k3 + k4,			k345	= k34 * k5;
	float	k01p02p12	= k0 * k1p2 + k12,		k34p35p45	= k34 + k3p4 * k5;
	float	k1p2m3m4	= k1p2 - k3p4;

	float4	n1{
		+ (k3p4 - k0) * k1p2m3m4 - k12 + k34,
		+3 * (k012 - k34 * (k0 + k1p2m3m4)),
		-3 * (k012 * k3p4 + k34 * (k34 - k01p02p12)),
		+ k012 * (k3 * k3 + k4 * k4 + k34) + k34 * (+k34 * (k0 + k1p2) - k3p4 * k01p02p12)
	};

	float4	n2{
		+ (k1p2 - k5) * k1p2m3m4 - k12 + k34,
		-3 * (k345 + k12 * (k1p2m3m4 - k5)),
		+3 * (k345 * k1p2 + k12 * (k12 - k34p35p45)),
		- k345 * (k1 * k1 + k2 * k2 + k12) + k12 * (-k12 * (k3p4 + k5) + k1p2 * k34p35p45)
	};
	return float4x4(n0 * d.x, n1 * d.y, n2 * d.z, n3 * d.w);
}

float4x4 NURBS3GetControlMatrix(float4 *c) {
	return float4x4(
		c[0] * c[0].w,
		c[1] * c[1].w,
		c[2] * c[2].w,
		c[3] * c[3].w
	);
}

float4 NURBS3GetWeights(float *k, float t) {
	float	l = (k[3] - t) / ((k[3] - k[2]) * (k[3] - k[1]));
	float	r = (t - k[2]) / ((k[3] - k[2]) * (k[4] - k[2]));
	float	a = r * (t - k[2]) / (k[5] - k[2]);
	float	b = (r * (k[4] - t) + l * (t - k[1])) / (k[4] - k[1]);
	float	c = (l * (k[3] - t)) / (k[3] - k[0]);

	return float4{
		c * (k[3] - t),
		b * (k[4] - t) + c * (t - k[0]),
		a * (k[5] - t) + b * (t - k[1]),
		a * (t - k[2])
	};
}
/*
iso::float3 NURBS3Evaluate(float *k, iso::float4 *cp, float t) {
	float4 w	= NURBS3GetWeights(k, t);
	float4 bw	= w * float4(cp[0].w, cp[1].w, cp[2].w, cp[3].w);
	return (cp[0].xyz * cp[0].w * b.x + cp[1].xyz * cp[1].w * b.y + cp[2].xyz * cp[2].w * b.z + cp[3].xyz * cp[3].w * b.w) / sum(bw);
}
*/
float4 _NURBS3Evaluate(float *k, float4 *c, float t) {
	float4x4	km	= NURBS3GetKnotMatrix(k);
	float4x4	cm	= NURBS3GetControlMatrix(c);
	return cm * km * cubic_param(t);
}

position3 NURBS3Evaluate(float *k, int n, float4 *cp, float t) {
	while (t > k[1]) {
		cp++;
		k++;
	}
	return project(_NURBS3Evaluate(k, cp, t));
}

position3 NURBS3Evaluate2D(float *ku, int nu, float *kv, int nv, float4 *cp, param(iso::float2) uv) {
	float	u = uv.x, v = uv.y;
	while (v > kv[1]) {
		cp += nu;
		kv++;
	}

	while (u > ku[1]) {
		cp++;
		ku++;
	}

	float4		u2	= NURBS3GetKnotMatrix(ku) * cubic_param(u);
	float4x4	cv;
	for (int i = 0; i < 4; i++) {
		cv[i] = NURBS3GetControlMatrix(cp) * u2;
		cp += nu;
	}

	return cv * NURBS3GetKnotMatrix(kv) * cubic_param(v);
}


//-----------------------------------------------------------------------------
//	NurbsModel
//-----------------------------------------------------------------------------

#if defined PLAT_PS4 || defined USE_DX11

struct RenderSubPatch {
	ShaderConstants		sc[2];
	Buffer<float4p>		control;
	uint32				num_verts;
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

namespace iso {

template<> void TypeHandler<NurbsModel>::Create(const CreateParams &cp, crc32 id, const NurbsModel *t) {
	Object				*obj	= cp.obj;
	RenderNurbsObject	*ro		= new RenderNurbsObject(obj, ISO::GetPtr(t));
	cp.world->Send(RenderAddObjectMessage(ro, ro->GetBox(), obj->GetWorldMat()));
}

extern "C" TypeHandler<NurbsModel> thNurbsModel;

} // namespace iso

#endif
