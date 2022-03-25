#ifndef ROADS_H
#define ROADS_H

#include "base/vector.h"
#include "maths/bezier.h"
#include "scenegraph.h"
#include "base/algorithm.h"

namespace ent {
using namespace iso;

	struct RoadProfile {
		struct Section {
			float	x, y, u0, u1;
		};
		struct CrossSection  {
			ISO_openarray<Section>	sections;
			uint8 		lkerb, rkerb, ledge, redge, lflex, rflex, ground;
		};

		CrossSection		xsect;
		CrossSection		xsect_low;

		ISOTexture			tex;
		float				vscale;

		ISO_openarray<float>	lanes;
		ISO_openarray<float>	ped_lanes;
	};

	struct RoadType {
		enum {
			RT_RANKSMASK	= 0xFF,
			RT_SIDEWALK		= 0x1 << 8,
			RT_TRAFFIC		= 0x1 << 9,
			RT_LEFTTURN		= 0x1 << 10,
			RT_USEKERB		= 0x1 << 11,
			RT_CHECKWALLS	= 0x1 << 12,
			RT_NOCROSSING	= 0x1 << 13,
			RT_PARKEDCARS	= 0x1 << 14,
			RT_FORCEKERB	= 0x1 << 15,
		};

		int						flags;
		RoadProfile				*profile;
	};

	struct GroundType {
		ISO_ptr<void>			tex[2];
	};

	struct Junction;
	struct Junction2;
	struct GroundPatch;

	struct RoadSeg {
		uint32					id;
		ISO_ptr<RoadProfile>	type;
	//	ISO_ptr<RoadType>		type;
		ISO_ptr<Junction2>		jtn[2];
		float3p					handle[2];
		ISO_ptr<GroundPatch>	gp[2];
		float3p					gp_handle[2];

		bezier_spline	GetSpline()						const;
		Junction*		other(const Junction *j)		const	{ return jtn[j == jtn[0]]; }
		bool			end(const Junction *j)			const	{ return j == jtn[1]; }
		bool			connects(const Junction *j)		const	{ return j == jtn[0] || j == jtn[1]; }
//		Junction*		Common(const RoadSeg *rs)		const	{ return rs->Connects(jtn[0]) ? jtn[0] : rs->Connects(jtn[1]) ? jtn[1] : NULL;}
	};

	struct Junction {
		float3p					pos;
	};

	struct Junction2 : Junction {
		ISO_openarray<ISO_ptr<RoadSeg> > roadsegs;
	};

	struct GroundPatch {
		ISO_ptr<GroundType>		type;
		ISO_openarray<ISO_ptr<RoadSeg> > edges;
		float2x3p				uv_trans;
	};

	inline bezier_spline RoadSeg::GetSpline() const {
		return bezier_spline(jtn[0]->pos, jtn[0]->pos + handle[0], jtn[1]->pos + handle[1], jtn[1]->pos);
	}

} // namespace ent

namespace iso {
using namespace ent;

extern hash_map<crc32, ISO_ptr<void> >			road_assets;

bool		connected(const Junction2 *a, const Junction *b);
bool		connected(const Junction *j, const RoadSeg *rs);
Junction*	shared_junction(const RoadSeg *rs0, const RoadSeg *rs1);
RoadSeg*	prev(const Junction2 *j, const RoadSeg *rs);

}//namespace iso

ISO_DEFUSERCOMPV(ent::RoadSeg, id, type, jtn, handle, gp, gp_handle);
ISO_DEFUSERCOMPV(ent::Junction, pos);
ISO_DEFUSERCOMPV(ent::Junction2, pos, roadsegs);
ISO_DEFUSERCOMPV(ent::GroundPatch, type, edges, uv_trans);

ISO_DEFUSERCOMPV(ent::RoadProfile::Section, x, y, u0, u1);
ISO_DEFUSERCOMPV(ent::RoadProfile::CrossSection, sections, lkerb, rkerb, ledge, redge, lflex, rflex, ground);
ISO_DEFUSERCOMPV(ent::RoadProfile, xsect, xsect_low, tex, vscale, lanes, ped_lanes);
ISO_DEFUSERCOMPV(ent::RoadType, flags, profile);
ISO_DEFUSERCOMPV(ent::GroundType, tex);

#endif // ROADS_H
