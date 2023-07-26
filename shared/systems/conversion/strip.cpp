#include "strip.h"
#include "base/list.h"
#include "base/algorithm.h"
#include "iso/iso_convert.h"
#include "maths/comp_geom.h"
#include "systems/mesh/model_iso.h"

using namespace iso;

//	1----2
//	|  / |
//	| /  |
//	0----3
bool CheckQuad(float3 v0, float3 v1, float3 v2, float3 v3) {
	float3	n	= normalise(cross(v1 - v0, v2 - v1));	//normal to face2
	return	abs(dot(n, v3 - v1)) < 0.001f				//check flat
		&&	dot(cross(v3 - v0, v1 - v0), n) > zero		//check convex at v0
		&&	dot(cross(v2 - v3, v1 - v2), n) > zero;		//check convex at v2
}

bool CheckQuad(stride_iterator<float3p> verts, int i0, int i1, int i2, int i3) {
	return CheckQuad(verts[i0], verts[i1], verts[i2], verts[i3]);
}

void Strip::Reverse() {
	reverse(begin(), end());
	if (size() & 1)
		dir = !dir;
}

Strip *CombineStrips(Strip &a, Strip &b) {
	size_t	sizea	= a.size();
	size_t	sizeb	= b.size();
	Strip	*strip	= new Strip(sizea + sizeb, (sizea & 1) == 0);
	copy_n(reversed(a).begin(), strip->begin(), sizea);
	copy_n(b.begin(), strip->begin() + sizea, sizeb);
	return strip;
}

struct StripTri;

struct StripEdge : cg::tri_edge<StripTri, StripEdge> {
	int			vert;
	bool		quad;
	StripEdge() : quad(false) {}
};

struct StripTri : e_link<StripTri>, cg::tri_face<StripTri, StripEdge> {
	int			index;
	int			shared;
};

class StripEdges {
	typedef	pair<int, StripEdge*>	item;
	typedef	dynamic_array<item>		type;
	type	*p;
public:
	StripEdges(int n)	{ p = new type[n]; }
	~StripEdges()		{ delete[] p; }

	StripEdge	*find(int v0, int v1)	{
		type	&a	= p[v0];
		for (size_t i = 0, n = a.size(); i < n; i++) {
			if (a[i].a == v1)
				return a[i].b;
		}
		return 0;
	}
	void		add(StripEdge &e, int v0, int v1) {
		e.vert	= v0;
		e.set_flip(find(v1, v0));
		p[v0].emplace_back(v1, &e);
	}
};

struct StripTris : array<e_list<StripTri>, 5> {
	typedef e_list<StripTri>::iterator iterator;

	void	MakeStrip(Strip &strip, StripEdge *edge, int maxlen);

	void	Add(StripTri *tri) {
		tri->shared = tri->count_shared();
		(*this)[tri->shared].push_back(tri);
	}

	void	Relink(StripTri *tri) {
		ISO_ASSERT(tri->shared >= 0 && tri->shared <= 3);
		(*this)[tri->shared].push_back(tri->unlink());
	}
	void	ClearFlip(StripEdge *edge) {
		StripEdge *flip = edge->flip;
		if (flip && flip->flip) {
			StripTri *tri	= flip->tri;
			--tri->shared;
			Relink(tri);
			flip->flip		= 0;
		}
	}
};

void StripTris::MakeStrip(Strip &strip, StripEdge *edge, int maxlen) {
	for (size_t n = strip.size(); edge && n < maxlen; n++, edge = edge->flip) {
		edge	= edge->link[n & 1];

		strip.push_back((n & 1 ? edge->next : edge)->vert);

		ClearFlip(edge->link[n & 1]);

		edge->tri->unlink();
	}
}

StripList::StripList(range<array<uint32,3>*> faces, range<stride_iterator<float3p>> verts, int32 maxstriplen) : total(0) {
//StripList::StripList(array<uint16,3> *faces, int32 num_faces, position3 *verts, int32 num_verts, int32 maxlen) : total(0) {
	StripEdges	edges(verts.size32());
	StripTris	tris;

	StripTri	*tri_array = new StripTri[faces.size()];
	e_list<StripTri>	temp;

	for (auto &v : faces) {
		int			i	= int(faces.index_of(v));
		StripTri	&tri = tri_array[i];
		tri.index = i;

		int			v0	= v[0], v1 = v[1], v2 = v[2];

		if (edges.find(v0, v1) || edges.find(v1, v2) || edges.find(v2, v0)) {
			tri[0].vert = v0;
			tri[1].vert = v1;
			tri[2].vert = v2;
			tris[4].push_back(&tri);
		} else {
			edges.add(tri[0], v0, v1);
			edges.add(tri[1], v1, v2);
			edges.add(tri[2], v2, v0);
			temp.push_back(&tri);
			tri[0].quad = tri[0].flip && (tri[0].flip->quad = CheckQuad(verts.begin(), v1, v2, v0, tri[0].flip->prev->vert));
			tri[1].quad = tri[1].flip && (tri[1].flip->quad = CheckQuad(verts.begin(), v2, v0, v1, tri[1].flip->prev->vert));
			tri[2].quad = tri[2].flip && (tri[2].flip->quad = CheckQuad(verts.begin(), v0, v1, v2, tri[2].flip->prev->vert));
		}
	}

	while (!temp.empty())
		tris.Add(temp.pop_front());

	for (;;) {
//		for (int i = 0; i < 4; i++) {
//			for (StripTris::iterator t = tri_list[i].begin(); t != tri_list[i].end(); ++t)
//				ISO_ASSERT(t->shared == i);
//		}

		int	i;
		for (i = 0; i < 5 && tris[i].empty(); i++);
		if (i > 4)
			break;

		StripTri	*tri	= &*tris[i].begin();
		StripEdge	*edge	= &(*tri)[0];

		for (int i = 0, b = 4; i < 3; i++) {
			if (StripEdge *flip = (*tri)[i].flip) {
				if (flip->tri->shared < b) {
					b		= flip->tri->shared;
					edge	= &(*tri)[i];
				}
			}
		}

		Strip	strip1, strip2;

		strip1.push_back(edge->vert);
		tris.ClearFlip(edge->prev);

		tris.MakeStrip(strip1, edge->prev, maxstriplen);
#if 1
		if (!edge->prev->flip || edge->prev->flip->tri->next == 0) {
			strip2.push_back(edge->prev->vert);
		} else {
			tris[0].push_back(edge->tri);
			edge->flip = 0;
			tris.MakeStrip(strip2, edge, maxstriplen);
		}
#else
		strip2.push_back(edge->prev->vert);
#endif
		Append(CombineStrips(strip2, strip1));
	}

	delete[] tri_array;
}

StripList::~StripList() {
	for (int i = 0; i < size(); i++)
		delete (*this)[i];
}

//-----------------------------------------------------------------------------
//	StripListGroup
//-----------------------------------------------------------------------------
class StripListGroup : public dynamic_array<StripList*> {
public:
	void		Make(StripList &strips, int maxgrouplen, bool windingorder = true);

	StripListGroup()	{}
	StripListGroup(StripList &strips, int maxgrouplen, bool windingorder = true);
	~StripListGroup();
};

void StripListGroup::Make(StripList &strips, int maxgrouplen, bool windingorder) {
	// Sort the list
	for (int i = 0; i < strips.size() - 1;) {
		if (strips[i + 1]->size() > strips[i]->size()) {
			iso::swap(strips[i], strips[i+1]);
			if (i != 0)
				i--;
		} else {
			i++;
		}
	}

	// Assign strips to groups
	for (auto &i : strips) {
		Strip	*strip	= exchange(i, nullptr);
		size_t	count	= strip->size();

		size_t	j;
		for (j = 0; j < size(); j++) {
			if ((*this)[j]->Total() + count < maxgrouplen)
				break;
		}
		if (j == size())
			push_back(new StripList);

		StripList	*striplist = (*this)[j];
		if (windingorder) {
			if (count & 1) {
				if (striplist->Total() & 1)
					strip->Reverse();
				striplist->Append(strip);
			} else {
				strip->Reverse();
				striplist->Prepend(strip);
			}
		} else {
			striplist->Append(strip);
		}
	}

	// Sort the groups
	for (size_t i = 0; i < size() - 1;) {
		if ((*this)[i + 1]->Total() > (*this)[i]->Total()) {
			iso::swap((*this)[i], (*this)[i + 1]);
			if (i != 0)
				i--;
		} else {
			i++;
		}
	}

}

StripListGroup::StripListGroup(StripList &strips, int maxgrouplen, bool windingorder) {
	Make(strips, maxgrouplen, windingorder);
}

StripListGroup::~StripListGroup() {
	for (auto i : *this)
		delete i;
}

ISO_ptr<Model> Stripify(holder<ISO_ptr<Model3> > p) {
	Model3	&m = *p.t;
	ISO_ptr<Model>	m2(p.t.ID());

	for (auto s = m.submeshes.begin(), e = m.submeshes.end(); s != e; ++s) {
		SubMesh					*sm		= (SubMesh*)(SubMeshBase*)s;
		ISO_ptr<SubMeshN<1> >	sm1(0, *sm);
		sm1->verts	= sm->verts;
		sm1->flags |= SubMeshBase::STRIP;

//		dynamic_array<position3>	verts = sm->VertComponentRange<float3p>(0);
		StripList	strips(sm->indices, sm->VertComponentRange<float3p>(0));

		int	numo	= 0, numf = 0, numi = -2;

		for (Strip *strip : strips) {
			numi	+= int(strip->size() + 2);
			if (strip->size() & 1)
				numo++;
			else if (strip->dir)
				numf++;
		}

		if (!(numo & 1) && numf != 0)
			numi++;

		uint32	*p1	= sm1->indices.Create(uint32(numi)).begin()->begin(), *p0 = p1;

		// all the unflipped even-length ones first
		for (Strip *strip : strips) {
			int	numv	= int(strip->size());
			if ((numv & 1) || strip->dir)
				continue;
			if (p1 > p0)
				p1 += 2;
			copy_n(strip->begin(), p1, numv);
			if (p1 > p0) {
				p1[-2] = p1[-3];
				p1[-1] = p1[0];
			}
			p1 += numv;
		}

		// all the odd-length ones second
		for (Strip *strip : strips) {
			size_t	numv	= strip->size();
			if (!(numv & 1))
				continue;

			if (p1 > p0)
				p1 += 2;

			if (((p1 - p0) & 1) ^ strip->dir)
				copy_n(reversed(*strip).begin(), p1, numv);
			else
				copy_n(strip->begin(), p1, numv);

			if (p1 > p0) {
				p1[-2] = p1[-3];
				p1[-1] = p1[0];
			}
			p1 += numv;
		}

		if (!(numo & 1) && numf != 0) {
			p1[0] = p1[-1];
			p1 += 1;
		}

		// all the flipped even-length ones third
		for (Strip *strip : strips) {
			size_t	numv	= strip->size();
			if ((numv & 1) || !strip->dir)
				continue;
			if (p1 > p0)
				p1 += 2;
			copy_n(strip->begin(), p1, numv);
			if (p1 > p0) {
				p1[-2] = p1[-3];
				p1[-1] = p1[0];
			}
			p1 += numv;
		}
		ISO_ASSERT(p1 - p0 == numi);

		m2->submeshes.Append(sm1);
	}
	return m2;
}


ISO_ptr<Model> Quadify(holder<ISO_ptr<Model3> > p) {
	Model3	&m = *p.t;
	ISO_ptr<Model>	m2(p.t.ID());

	for (SubMesh *sm : m.submeshes) {
		ISO_ptr<SubMeshN<4>>	sm1(0, *sm);
		sm1->verts	= sm->verts;
		sm1->SetVertsPerPrim(4);

		dynamic_array<position3>	verts = sm->VertComponentRange<float3p>(0);
		StripList	strips(sm->indices, verts);

		int		numi	= 0;
		for (Strip *strip : strips)
			numi += int(strip->size() - 1) / 2;

		uint32	*p1		= sm1->indices.Create(uint32(numi)).begin()->begin();
		for (Strip *strip : strips) {
			if (strip->dir) {
				for (auto *si = strip->begin(), *se = strip->end() - 2; si < se; si += 2) {
					p1[0] = si[2];
					p1[1] = si[1];
					p1[2] = si[0];
					p1[3] = si[si + 1 < se ? 3 : 2];
					p1 += 4;
				}
			} else {
				for (auto *si = strip->begin(), *se = strip->end() - 2; si < se; si += 2) {
					p1[0] = si[0];
					p1[1] = si[1];
					p1[2] = si[2];
					p1[3] = si[si + 1 < se ? 3 : 2];
					p1 += 4;
				}
			}
		}
		m2->submeshes.Append(sm1);
	}
	return m2;
}

static initialise init(
	ISO_get_operation(Stripify),
	ISO_get_operation(Quadify)
);

