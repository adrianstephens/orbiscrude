#include "roads.h"
#include "render_object.h"
#include "mesh/shapes.h"

using namespace iso;

Junction *iso::shared_junction(const RoadSeg *rs0, const RoadSeg *rs1) {
	return	connected(rs0->jtn[0], rs1)	? rs0->jtn[0].get()
		:	connected(rs0->jtn[1], rs1)	? rs0->jtn[1].get()
		:	0;
}

bool iso::connected(const ent::Junction *j, const ent::RoadSeg *rs) {
	return rs->jtn[0] == j || rs->jtn[1] == j;
}

bool iso::connected(const ent::Junction2 *a, const ent::Junction *b) {
	if (a == b)
		return true;

	for (auto &rs : a->roadsegs) {
		if (rs->jtn[rs->jtn[0] == a] == b)
			return true;
	}

	return false;
}

struct RenderJunction : public RenderItem {
	const ent::Junction		*j;

	RenderJunction(const ent::Junction *_j) : RenderItem(this), j(_j) {}
	cuboid	GetBox() const { return cuboid::with_centre(position3(j->pos), vector3{1,1,0}); }

	void operator()(DestroyMessage &m)	{ delete this; }
	void operator()(RenderEvent *re, uint32 extra) {
		static pass *coloured = *ISO::root("data")["default"]["specular"][0];
		AddShaderParameter(ISO_CRC("diffuse_colour", 0x6d407ef0), colour(0,1,0));
		Draw(re, circle3(plane(vector3{0,0,1}, position3(j->pos)), circle(position2(j->pos.x, j->pos.y), 1)), coloured);
	}
	void operator()(RenderCollector &rc) {
		float	d = rc.Distance(position3(j->pos));
		rc.re->AddRenderItem(this, MakeKey(RS_OPAQUE, d), 0);
	}
};

struct RenderRoadProfile {
	DataBufferT<float4>	xsect;
	RenderRoadProfile(ent::RoadProfile *p) {
		xsect.Init((float4*)p->xsect.sections.begin(), p->xsect.sections.Count());
	}
};

hash_map<ent::RoadProfile*,RenderRoadProfile*>	render_profiles;
hash_map<crc32, ISO_ptr<void> >					iso::road_assets;

struct RenderRoadSeg : public RenderItem {
	const ent::RoadSeg		*rs;
	cuboid					box;
	RenderRoadProfile		*rp;
	VertexBuffer<float4p>	vb;

	RenderRoadSeg(const ent::RoadSeg *_rs) : RenderItem(this), rs(_rs), rp(0) {
		box = cuboid(position3(rs->jtn[0]->pos), position3(rs->jtn[1]->pos)).expand(float3{2,2,0});
		if (rs->type) {
			rp = render_profiles[rs->type];
			if (!rp)
				rp = render_profiles[rs->type] = new RenderRoadProfile(rs->type);
		}
		bezier_spline	b = rs->GetSpline();
		vb.Init((float4p*)&b, 4);
	}
	cuboid	GetBox() const { return box; }

	void operator()(DestroyMessage &m)	{ delete this; }
	void operator()(RenderEvent *re, uint32 extra) {

		if (rp) {
			static pass *shader = *ISO::root("data")["roads"]["road"][0];
			Set(re->ctx, shader, ISO::MakeBrowser(re->consts));

			re->ctx.SetBuffer(SS_HULL, rp->xsect, 0);
			re->ctx.SetBuffer(SS_LOCAL, rp->xsect, 0);
			re->ctx.SetTexture(SS_PIXEL, rs->type->tex, 0);
		} else {
			static pass *shader = *ISO::root("data")["roads"]["solidroad"][0];
			Set(re->ctx, shader, ISO::MakeBrowser(re->consts));
		}

#if 1
		re->ctx.SetVertices(vb);
		re->ctx.DrawVertices(PatchPrim(4), 0, 4);
#else
		bezier_spline	b = rs->GetSpline();

//		re->ctx.SetFillMode(FILL_WIREFRAME);
		ImmediateStream<float4p>	im(re->ctx, PatchPrim(4), 4);
		float4p	*p = im.begin();
		p[0] = b.c0;
		p[1] = b.c1;
		p[2] = b.c2;
		p[3] = b.c3;
#endif
	}

	void operator()(RenderCollector &rc) {
		float	d = rc.Distance(box.centre());
		rc.re->AddRenderItem(this, MakeKey(RS_OPAQUE, d), 0);
	}
};


struct RenderGroundPatch : public RenderItem {
	const ent::GroundPatch		*gp;
	position3					centre;
	interval<float>				radii;

	RenderGroundPatch(const ent::GroundPatch *_gp) : RenderItem(this), gp(_gp), radii(empty) {
		RoadSeg	*rs0 = gp->edges.back();
		Junction	*j		= rs0->jtn[rs0->gp[0] == gp];
		position3	t		= position3(zero);
		int			n		= 0;

		for (auto &rs : gp->edges) {
			j	= rs->other(j);
			t	+= j->pos;
			n++;
		}

		centre = t / (float)n;

		for (auto &rs : gp->edges) {
			j		= rs->other(j);
			radii	|= len(j->pos - centre.v);;
		}
	}
	cuboid	GetBox() const {
		return cuboid(position3(centre - float3{radii.a / 4, radii.a / 4, 0}), position3(centre + float3{radii.a / 4, radii.a / 4, radii.b}));
	}

	void operator()(DestroyMessage &m)	{ delete this; }
	void operator()(RenderEvent *re, uint32 extra) {
		pass *shader;

		if (auto tex = road_assets["bricks"_crc32].put()) {
			static pass *shader1 = *ISO::root("data")["default"]["norm_specular"][0];
			shader	= shader1;
			AddShaderParameter("normal_samp", tex);
		} else {
			static pass *shader0 = *ISO::root("data")["default"]["specular"][0];
			shader	= shader0;
		}

		AddShaderParameter(ISO_CRC("diffuse_colour", 0x6d407ef0), colour(0.1f,0.1f,0.1f, .5f));
//		Draw(re, circle3(plane(vector3(0,0,1), centre), circle(centre.xy, radius / 2)), coloured);
		Draw(re, GetBox(), shader);
	}
	void operator()(RenderCollector &rc) {
		float	d = rc.Distance(centre);
		rc.re->AddRenderItem(this, MakeKey(RS_TRANSP, -d), 0);
	}
};

namespace iso {
#if 0
template<> void TypeHandler<ent::Junction>::Create(const CreateParams &cp, crc32 id, const ent::Junction *j) {
	if (j->roadsegs.Count() > 2) {
		RenderJunction	*r		= new RenderJunction(j);
		cp.world->AddBox(r->GetBox());
		cp.world->Send(RenderAddObjectMessage(r, r->GetBox()));
	}
	for (auto &rs : j->roadsegs) {
		if (rs->jtn[0] == j) {
			RenderRoadSeg	*r		= new RenderRoadSeg(rs);
			cp.world->AddBox(r->GetBox());
			cp.world->Send(RenderAddObjectMessage(r, r->GetBox()));
		}
	}

}
extern "C" {
	struct TypeHandlerJunction : TypeHandler<ent::Junction> {
		TypeHandlerJunction() { ISO::getdef<ent::Junction>(); }
	} thJunction;
}
#endif

template<> void TypeHandler<ent::RoadSeg>::Create(const CreateParams &cp, crc32 id, const ent::RoadSeg *t) {
	RenderRoadSeg	*r		= new RenderRoadSeg(t);
	cp.world->AddBox(r->GetBox());
	cp.world->Send(RenderAddObjectMessage(r, r->GetBox()));
}

extern "C" {
	struct TypeHandlerRoadSeg : TypeHandler<ent::RoadSeg> {
		TypeHandlerRoadSeg() {
			ISO::getdef<ent::RoadSeg>();
			ISO::getdef<ent::Junction2>();
		}
	} thRoadSeg;
}

template<> void TypeHandler<ent::GroundPatch>::Create(const CreateParams &cp, crc32 id, const ent::GroundPatch *t) {
	RenderGroundPatch	*r		= new RenderGroundPatch(t);
	cp.world->AddBox(r->GetBox());
	cp.world->Send(RenderAddObjectMessage(r, r->GetBox()));
}

extern "C" {
	TypeHandler<ent::GroundPatch> thGroundPatch;
}

} // namespace iso
