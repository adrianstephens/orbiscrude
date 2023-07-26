#include "iso/iso_convert.h"
#include "roads_editor.h"
#include "extra/octree.h"
#include "filetypes/3d/model_utils.h"

using namespace iso;

hash_map<crc32, ISO_ptr<RoadProfile> >	road_profiles;

//-----------------------------------------------------------------------------
//	RoadSegList
//-----------------------------------------------------------------------------

RoadSegList::RoadSegList(const dynamic_array<Junction2*> &jl) {
	if (jl.empty())
		return;

	Junction	*j0 = jl.back();
	for (auto &j1 : jl) {
		for (auto &rs : j1->roadsegs) {
			if (connected(j0, rs)) {
				push_back(rs);
				break;
			}
		}
		j0 = j1;
	}
}

static void Follow(dynamic_array<ent::RoadSeg*> &out, Junction2 *j, uint32 id, dynamic_array<ent::RoadSeg*> &array) {
	ent::RoadSeg	**found = 0;
	do {
		for (auto &i : j->roadsegs) {
			if (i->id == id) {
				if (found = find_check(array, i)) {
					array.erase(found);
					j	= i->jtn[i->jtn[0] == j];
					out.push_back(i);
					break;
				}
			}
		}
	} while (found);
}

RoadSegList::RoadSegList(dynamic_array<ent::RoadSeg*> &array) {
	while (!array.empty()) {
		uint32	id = array[0]->id;
		Follow(*this, array[0]->jtn[0], id, array);
		if (!empty()) {
			reverse(*this);
			Follow(*this, back()->jtn[1], id, array);
			return;
		}
		array.erase_unordered(array.begin());
	}
}

RoadSegList RoadSegList::TraceRound(RoadSeg *rs, bool dir, int max) {
	RoadSegList	rsl;

	int			n	= 0;
	Junction2	*j0 = rs->jtn[dir], *j;

	do {
		rsl.push_back(rs);
		j	= rs->jtn[!dir];
		rs	= prev(j, rs);
		dir = rs->jtn[1] == j;
		n++;
	} while (n != max && (j != j0 || rs != rsl.front() || n <= 2));

	return rsl;
}

RoadSegList RoadSegList::TraceID(RoadSeg *rs, float epsilon) {
	RoadSegList	rsl;
	int		id = rs->id;

	rsl.push_back(rs);
	for (int iend = 0; iend < 2; iend++) {
		int			end = iend;
		RoadSeg		*rs1 = rs;
		RoadSeg		*rsnext;
		do {
			Junction2 *jtn = rs1->jtn[end];
			rsnext = NULL;
			for (auto &rs2 : jtn->roadsegs) {
				if (!find_check(rsl, rs2) && rs2->id == id && colinear(rs1->handle[end], rs2->handle[rs2->end(jtn)], epsilon))
					rsnext = rs2;
			}
			if (rsnext) {
				end = rsnext->jtn[0] == jtn;
				rsl.push_back(rsnext);
			}
		} while (rs1 = rsnext);

		if (iend == 0)
			reverse(rsl);
	}
	return rsl;
}

void RoadSegList::SortStar(const Junction2 *j) {
	if (empty())
		return;

	if (!j)
		j = (const Junction2*)shared_junction(back(), front());

	for (auto prs0 = begin(), prs1 = prs0; ++prs1 != end(); prs0 = prs1) {
		RoadSeg	*rs0	= *prs0;
		RoadSeg	*rs1	= *prs1;
		float3	v0		= rs0->handle[rs0->jtn[1] == j];
		float3	v1		= rs1->handle[rs1->jtn[1] == j];

		for (auto prs2 = prs1; ++prs2 != end();) {
			RoadSeg		*rs2	= *prs2;
			float3		v2		= rs2->handle[rs2->jtn[1] == j];

			if (cross(v1.xy, v0.xy) < zero
			? (cross(v1.xy, v2.xy) >= zero || cross(v2.xy, v0.xy) >= zero)
			: (cross(v1.xy, v2.xy) >  zero && cross(v2.xy, v0.xy) >  zero)
			) {
				// need to swap the two..
				swap(*prs2, *prs1);
				prs1	= prs2;
				v1		= v2;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	RoadSegList
//-----------------------------------------------------------------------------

void Straighten(ent::Junction *j, ent::RoadSeg* rs0, ent::RoadSeg* rs1) {
	int			i0	= rs0->jtn[0] != j;
	int			i1	= rs1->jtn[0] != j;

	Junction*	j0	= rs0->jtn[!i0];
	Junction*	j1	= rs1->jtn[!i1];

	position3	pj	(j->pos);
	position3	pj0	(j0->pos);
	position3	pj1	(j1->pos);

	float3		dir	= normalise(pj1 - pj0);

	rs0->handle[i0]	= -dir * len(pj0 - pj) / 3.f;
	rs1->handle[i1]	=  dir * len(pj1 - pj) / 3.f;
}

void Straighten(ent::Junction *j, ent::RoadSeg *rs) {
	int			i	= rs->jtn[0] != j;
	rs->handle[i]	= (position3(rs->jtn[!i]->pos) - position3(rs->jtn[i]->pos)) / 3;
}

void Straighten(ent::RoadSeg *rs) {
	float3		dir	= (position3(rs->jtn[1]->pos) - position3(rs->jtn[0]->pos)) / 3;
	rs->handle[0]	=  dir;
	rs->handle[1]	= -dir;
}

void Straighten(const dynamic_array<ent::Junction*> &junctions) {
	for (auto i = junctions.begin(), e = junctions.end() - 2; i < e; ++ i) {
		Junction	*j0 = i[0], *j1 = i[1], *j2 = i[2];
		float3		p0	= j0->pos, p1 = j1->pos, p2 = j2->pos;
		float2		u	= float3(p2 - p1).xy;
		float2		v	= float3(p1 - p0).xy;
		float2		w	= float3(p2 - p0).xy;
		float		ulen = len(u);
		float		vlen = len(v);
		float		wlen = len(w);

		if (between(dot(u, v) / (ulen * vlen), .707f, .995f)) {
			float	dir		= dot(v, w) / (vlen * wlen);
			float	cor		= sqrt(1 - dir * dir) * vlen / 2;
			float2	vcor	= perp(w) * sign(cross(w, v)) * cor / wlen;
			j1->pos.x	+= vcor.x;
			j1->pos.y	+= vcor.y;
		}
	}
}

RoadSeg* FindRoadSeg(Junction2* j, int id) {
	for (auto &i : j->roadsegs) {
		if (i->id == id)
			return i;
	}
	return 0;
}

void Remove(Junction2 *j) {
	for (auto &rs : j->roadsegs) {
		Junction2 *j2 = rs->jtn[rs->jtn[0] == j];
		j2->roadsegs.Remove(find(j2->roadsegs, rs));
		//delete rs;
	}
	j->roadsegs.Clear();
}

void AddSorted(Junction2 *j, ISO_ptr<RoadSeg> rs) {
	if (j->roadsegs.Count() < 2) {
		j->roadsegs.push_back(rs);
		return;
	}

	float3	v		= rs->handle[rs->jtn[1] == j];

	RoadSeg	*rs0	= j->roadsegs.back();
	float3	v0		= rs0->handle[rs0->jtn[1] == j];
	for (auto prs1 = j->roadsegs.begin(); prs1 != j->roadsegs.end(); ++prs1) {
		RoadSeg	*rs1	= *prs1;
		float3	v1		= rs1->handle[rs1->jtn[1] == j];

		if (cross(v1.xy, v0.xy) < zero
		? (cross(v1.xy, v.xy) >= zero || cross(v.xy, v0.xy) >= zero)
		: (cross(v1.xy, v.xy) >  zero && cross(v.xy, v0.xy) >  zero)
		) {
			j->roadsegs.Insert(prs1 - j->roadsegs.begin(), rs);
			break;
		}
		v0	= v1;
	}
}

RoadSeg *iso::prev(const Junction2 *j, const RoadSeg *rs) {
	auto	i = find(j->roadsegs, rs);
	if (i == j->roadsegs.begin())
		i = j->roadsegs.end();
	return i[-1];
}


//-----------------------------------------------------------------------------
//	GroundPatch
//-----------------------------------------------------------------------------

ISO_ptr<GroundPatch> MakeGroundPatch(ISO_ptr<GroundType> type, const RoadSegList &rslist, float bulge) {
	ISO_ptr<GroundPatch>	gp(0);
	gp->type		= type;
	gp->uv_trans	= identity;

	RoadSeg	*rs		= rslist.back();
	for (auto &rs1 : rslist) {
		if (rs == rs1) {
			rs->gp[0]			= rs->gp[1] = gp;
			rs->gp_handle[0]	= rs->gp_handle[1] = float3{-rs->handle[0].y, rs->handle[0].x, bulge};
		} else {
			bool	dir	= connected(rs->jtn[0], rs1);
			rs->gp[dir]			= gp;
			rs->gp_handle[dir]	= float3{-rs->handle[dir].y, rs->handle[dir].x, bulge};
		}
		gp->edges.push_back(ISO::GetPtr<32>(rs1));
		rs	= rs1;
	}
	return gp;
}

ISO_ptr<Scene> MakeGroundPatches(ISO_ptr<Scene> scene) {
	dynamic_array<ISO_ptr<GroundPatch> > patches;
	auto	&children = scene->root->children;

	Junction2	*j;
	for (auto &i : children) {
		if (i.IsType<RoadSeg>()) {
			RoadSeg		*rs = i;
			j	= rs->jtn[0];
			AddSorted(j, i);
			j	= rs->jtn[1];
			AddSorted(j, i);
		}
	}
	for (auto &i : children) {
		if (i.IsType<RoadSeg>()) {
			RoadSeg	*rs = i;
			if (!rs->gp[0]) {
				patches.push_back(MakeGroundPatch(ISO_NULL, RoadSegList::TraceRound(rs, false), 5.f));
			}
		}
	}
	for (auto &i : children) {
		if (i.IsType<RoadSeg>()) {
			RoadSeg		*rs = i;
			j	= rs->jtn[0];
			j->roadsegs.Clear();
			j	= rs->jtn[1];
			j->roadsegs.Clear();
			rs->gp[0].Clear();
			rs->gp[1].Clear();
		}
	}

	for (auto &i : patches)
		children.Append(i);

	return scene;
}

//-----------------------------------------------------------------------------
//	GetRoadType
//-----------------------------------------------------------------------------

 string GetRoadType(const ISO::Browser2 &b) {
	string	value;

	int		fccClass = -1, fccPrimary = 0, fccSecondary = 0;
	if (value = b["FCC"].Get(value)) {
		fccClass		= value[0];
		fccPrimary		= value[1] - '0';
		fccSecondary	= value[2] - '0';
	}

	if (b["ACC"].GetInt() < 4 && fccClass == 'A' && fccPrimary > 0x2 && fccPrimary < 0x6)
		fccPrimary = 0x2;

	// oneway
	int	oneway = 0;
	if (value = b["OneWay"].Get<string>())
		oneway = value[0] == 'F' ? 1 : -1;

	// elevation
	float	z[] = {0, 0};
	if (float z0 = b["F_ZLEV"].Get<float>())
		z[oneway == -1] = z0 * 8;

	if (float z1 = b["T_ZLEV"].Get<float>())
		z[oneway != -1] = z1 * 8;

	// type
	string	type	= b["Type"].Get<string>();

	// profile
	string	profile_group, profile;
	if (fccClass != -1 || z[0] || z[1] || oneway) {
		// fcc
		if (fccClass == 'A') {
			// roads
			if (fccPrimary < 0x6) {
				static const char *tags[] = {"", "Fwy", "Major Hwy", "Minor Hwy", "Rd", "Trail"};
				profile_group = tags[fccPrimary];

				if (fccSecondary == 0x2 || fccSecondary == 0x6)
					profile += "Tunnel";

				// patch missing freeway type
				if (!type && fccPrimary == 0x1)
					type = "Fwy";

			} else if (fccPrimary == 0x6) {
				static const char *tags[] = {"Major Ramp", "Minor Ramp", "Minor Ramp", "Minor Ramp", "Alley", "Ferry", "Ferry", "", "Ferry", "Ferry"};
				profile_group = tags[fccSecondary];

				// patch missing ramp type
				if (!type && fccSecondary >= 0x0 && fccSecondary <= 0x3)
					type = "Ramp";

			} else if (fccPrimary == 0x7) {
				static const char *tags[] = {"Walkway", "Walkway", "Stairway", "Alley", "Driveway", "Parking", "", "", "", ""};
				profile_group = tags[fccSecondary];

			}

		} else if (fccClass == 'B') {
			// railroad
			profile_group = "Railroad";

			if (fccSecondary == 0x2)
				profile += "Tunnel";

			else if (fccSecondary == 0x3)
				profile += "Elevated";
		}

		// oneway
		if (oneway)
			profile += " Oneway";

		// elevation
		if ((z[0] || z[1]) && !(fccClass == 'A' && fccPrimary < 0x6 && (fccSecondary == 0x2 || fccSecondary == 0x6)))
			profile += " Elevated";
	}

	buffer_accum<256>	name;
	if (type)
		name << type;
	if (profile_group)
		name << onlyif(type, '|') << profile_group;
	if (profile)
		name << ": " << profile;

	return str(name);
 }

#if 0
#if 1//def SHP_LEADDOG
	// primary
	if (value = b["STREET_ALT"].Get<string>()) {
		name << "Av " << to_upper(value.begin());

	// alternate
	} else if (value = b["STREET"].Get<string>()) {
		switch (value[0]) {
			case 'C': name << "Cll";	break;
			case 'K': name << "Cra";	break;
			case 'D': name << "Diag";	break;
			case 'T': name << "Trv";	break;
			case 'A': name << "Av";		break;
		}

		const char *p = skip_whitespace(value + 2);
		if (*p)
			name << ' ' << value;
	}
#else
	// prefix
	if (value = b["PREFIX"])
		name << value << ' ';

	// primary
	name << b["NAME"].Get<string>();

	// suffix
	if (value = b["SUFFIX"].Get<string>())
		name << ' ' << value;
#endif
	return name;
}
#endif


void Optimise(const dynamic_array<ent::Junction*> &junctions) {
	float		err	= 0.f;

	for (auto j0 : junctions) {
		Junction2 *j = (Junction2*)j0;
		if (j->roadsegs.Count() == 2) {
			if (j->roadsegs[0]->id == j->roadsegs[1]->id) {
				int			dir		= j->roadsegs[0]->jtn[0] == j;
				RoadSeg		*rs0	= j->roadsegs[dir],			*rs1	= j->roadsegs[!dir];
				float		len0	= len(rs0->GetSpline()),	len1	= len(rs1->GetSpline());
				int			end0	= rs0->jtn[0] == j,			end1	= rs1->jtn[0] == j;
				float		t		= len(rs0->handle[!end0]) / (len(rs0->handle[!end0]) + len(rs1->handle[!end1]));

				Junction2	*j0		= rs0->jtn[end0],			*j1		= rs1->jtn[end1];
				float3		h0		= rs0->handle[end0] / t,	h1		= rs1->handle[end1] / (1 - t);

				position3	p0		= position3(j0->pos);
				position3	p1		= position3(j1->pos);

				err += (float)abs(len(bezier_spline{p0, p0 + h0, p1 + h1, p1}) - (len0 + len1)) / (len0 + len1);

				if (err < .005f) {
					ISO_ptr<RoadSeg> prsnew(0, *rs0);
					prsnew->jtn[0]		= ISO::GetPtr(j0);
					prsnew->jtn[1]		= ISO::GetPtr(j1);
					prsnew->handle[0]	= h0;
					prsnew->handle[1]	= h1;

					j0->roadsegs.push_back(prsnew);
					j1->roadsegs.push_back(prsnew);

					Remove(j);

				} else {
					err = 0.f;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	RoadSegList
//-----------------------------------------------------------------------------

ISO_ptr<void> MakeRoads(ISO_ptr<void> roads) {
	auto		*datum	= GeoDatum::find_by("World Geodetic System 1984 (Global)");;
	auto&		pts		= *ISO_conversion::convert<ISO_openarray<double2p> >(ISO::Browser(roads)[0]["points"]);
	UTM			origin	= LLA(pts[0].x, pts[0].y).to_UTM(datum->re);
	LocalSpace	local(datum, origin);

	rectangle	ext(empty);
	for (auto i : ISO::Browser(roads)) {
		auto&	pts	= *ISO_conversion::convert<ISO_openarray<double2p> >(i["points"]);
		for (auto &pt : pts)
			ext |= local.LLA2Local(LLA(pt.x, pt.y));
	}

	dynamic_array<ISO_ptr<Junction2> >	junctions;
	dynamic_octree	oct(cuboid(position3(ext.a, 0.1f), position3(ext.b, len(ext.extent()))));
	hash_map<uint32, dynamic_array<ent::RoadSeg*> > roadsbyid;

	for (auto i : ISO::Browser(roads)) {
		auto&		pts	= *ISO_conversion::convert<ISO_openarray<double2p> >(i["points"]);

		ISO_ptr<Junction2>	prev, next;
		for (auto &pt : pts) {
			position2	pos = local.LLA2Local(LLA(pt.x, pt.y));
			ray3		ray(position3(pos, -1), float3{1,1,1000000});
			void		*found	= oct.shoot_ray(ray, 0.25f, [&](void *_j, param(ray3) r, float &t)->bool {
				Junction	*j = (Junction*)_j;
				return len2(float3(j->pos).xy - r.p.v.xy) < 1;
			});

			if (found) {
				next = ISO::GetPtr((Junction*)found);
			} else {
				next.Create(0);
				next->pos	= concat(pos.v, 0.1f);
				oct.add(cuboid::with_centre(position3(next->pos), float3(one)), next);
				//scene->root->children.Append(next);
				junctions.push_back(next);
			}

			if (prev && !connected(prev.get(), next.get())) {
				// roadseg
				ISO_ptr<RoadSeg>	rs(0);
				rs->id		= crc32(i["NAME"].Get<string>());
				string	type = GetRoadType(i);
				rs->type	= road_profiles[crc32(type)];
				rs->jtn[0]	= prev;
				rs->jtn[1]	= next;

				if (RoadSeg *rs0 = FindRoadSeg(prev, rs->id))
					Straighten(prev, rs0, rs);
				else
					Straighten(prev, rs);

				if (RoadSeg *rs1 = FindRoadSeg(next, rs->id))
					Straighten(next, rs, rs1);
				else
					Straighten(next, rs);

				prev->roadsegs.push_back(rs);
				next->roadsegs.push_back(rs);

				//scene->root->children.Append(rs);
				//roadsegs.push_back(rs);
				roadsbyid[rs->id]->push_back(rs);
			}
			prev = next;
		}
	}

	for (auto &i : roadsbyid) {
		while (!i.empty()) {
			RoadSegList	road(i);
			if (road.size() > 1) {
				dynamic_array<Junction*> juncs = road.GetJunctionList();
				Straighten(juncs);
				Optimise(juncs);
			}
		}
	}

	ISO_ptr<Scene>	scene(0);
	scene->root.Create(0);

#if 1
	for (auto &j : junctions) {
		for (auto &rs : j->roadsegs) {
			if (rs->jtn[0] == j)
				scene->root->children.Append(rs);
		}
	}
	for (auto &j : junctions) {
		j->roadsegs.Clear();
	}
#else
	for (auto &j : junctions) {
		if (j->roadsegs)
			scene->root->children.Append(j);
	}
#endif
	CheckHasExternals(scene, ISO::TRAV_DEEP);
	return scene;
}

//-----------------------------------------------------------------------------
//	RoadSegList
//-----------------------------------------------------------------------------

bool Near(float x, float y)		{ return abs(x - y) < .1f; }

float GetXSect(Model3 *model, ISO_openarray<RoadProfile::Section> &xsect) {
// find miny, maxy
	interval<float>	ext_y(empty);

	for (auto &_sm : model->submeshes) {
		SubMesh	*sm		= _sm;
		int		nverts	= sm->NumVerts();
		auto	verts	= sm->VertComponentData<float3p>(0);
		auto	uvs		= sm->VertComponentData<float2p>("texcoord");
		for (int i = 0; i < nverts; i++)
			ext_y |= verts[i].y;
	}

// find minv, maxv, across
	struct Vertex {
		float3p	pos;
		float2p	uv;
	};
	dynamic_array<Vertex>	across;
	float	minv, maxv;

	for (auto &_sm : model->submeshes) {
		SubMesh	*sm		= _sm;
		int		nverts	= sm->NumVerts();
		auto	verts	= sm->VertComponentData<float3p>(0);
		if (auto uvs = sm->VertComponentData<float2p>("texcoord")) {
			for (int i = 0; i < nverts; i++) {
				if (verts[i].y == ext_y.a)
					minv = uvs[i].y;
				else if (verts[i].y == ext_y.b)
					maxv = uvs[i].y;

				if (Near(verts[i].y, ext_y.a)) {
					Vertex	&v = across.push_back();
					v.pos	= verts[i];
					v.uv	= uvs[i];
				}
			}
		}
	}

	sort(across, [](const Vertex &a, const Vertex &b) { return a.pos.x < b.pos.x; });

	xsect.Create(across.size32());

// scan for array
	for (int i = 0; i < across.size32(); i++) {
		auto	&v	= across[i];
		auto	&x	= xsect[i];
		x.x		= v.pos.x;
		x.y		= v.pos.z;
		x.u0	= v.uv.x;
		x.u1	= across[i == 0 ? across.size() - 1 : i - 1].uv.x;
	}

	return (maxv - minv) / ext_y.extent();	//texscale
}

void GenerateLowRes(ISO_openarray<RoadProfile::Section> &xsect, ISO_openarray<RoadProfile::Section> &xsect_low) {
	int		hi	= 0;
	xsect_low.Append(xsect[0]);
	xsect_low.back().y	= 0;

	do {
		float	u	= xsect[hi].u1;
		bool	dir	= xsect[hi + 1].u1 <= u;
		while (hi < xsect.Count() - 1 && (dir ? xsect[hi + 1].u1 <= u : xsect[hi + 1].u1 >= u))
			u	= xsect[++hi].u1;

		xsect_low.push_back(xsect[hi]);
		xsect_low.back().y = 0;
		hi++;
	} while (hi < xsect.Count() - 1);
}

ISO_ptr<RoadProfile> NodeToProfile(ISO_ptr<Node> node) {
	Model3			*model = 0;
	ent::Splitter	*splitter = 0;

	for (auto &i : node->children) {
		if (i.IsType<Model3>()) {
			model = i;
			break;
		} else if (i.IsType<Splitter>()) {
			splitter = i;
		}
	}

	if (!model) {
		if (splitter) {
			for (auto &i : *(anything*)splitter->hirez) {
				if (i.IsType<Model3>()) {
					model = i;
					break;
				}
			}
		}
	}

	if (!model)
		return ISO_NULL;

	ISO_ptr<RoadProfile>	rp(0);
	for (auto i : ISO::Browser2(model->submeshes[0]->parameters)) {
		if (i.Is<Texture>()) {
			rp->tex = i;
			break;
		}
	}

	rp->vscale	= GetXSect(model, rp->xsect.sections);

	if (splitter) {
		model	= splitter->lorez;
		GetXSect(model, rp->xsect_low.sections);
	} else {
		GenerateLowRes(rp->xsect.sections, rp->xsect_low.sections);
	}

	return rp;
}

ISO_ptr<RoadProfile> SceneToProfile(ISO_ptr<Scene> scene) {
	return NodeToProfile(scene->root);
}

struct RoadProfiles : anything {};
ISO_DEFCALLBACK(RoadProfiles, anything);

//-----------------------------------------------------------------------------
//	RoadSegList
//-----------------------------------------------------------------------------

void Init(RoadProfiles *x, void *physram) {
	for (auto &i : *x) {
		if (ISO_ptr<void> p = ISO::Browser(i)["profile"])
			road_profiles[i.ID()] = p;
		else
			road_assets[i.ID()] = i;
	}
}
void DeInit(RoadProfiles *x) {}

static initialise init(
	ISO_get_operation(MakeRoads),
	ISO_get_conversion(NodeToProfile),
	ISO_get_conversion(SceneToProfile),
	ISO_get_operation(MakeGroundPatches),
	ISO::getdef<RoadProfile>(),
	ISO::getdef<RoadProfiles>()
);

