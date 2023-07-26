#include "model_utils.h"
#include "base/algorithm.h"
#include "maths/comp_geom.h"
#include "extra/indexer.h"
#include "iso/iso_convert.h"

namespace iso {

const ISO::Element *GetVertexComponentElement(const ISO_ptr<void> &verts, tag2 id) {
	ISO::TypeOpenArray	*type	= (ISO::TypeOpenArray*)verts.GetType();
	return ((ISO::TypeComposite*)type->subtype->SkipUser())->Find(id);
}

//-----------------------------------------------------------------------------
//	rearrange faces for cache-coherency
//-----------------------------------------------------------------------------

MeshCacheParams::MeshCacheParams(uint32 _size, float decay_power, float last_tri_score, float _valence_scale, float _valence_power)
	: size(_size), valence_scale(_valence_scale), valence_power(_valence_power)
{
	table = new float[size + 3];
	for (uint32 i = 0; i < size + 3; i++) {
		table[i]
			= i < 3		? last_tri_score
			: i < size	? pow(1 - float(i - 3) / (size - 3), decay_power)
			: 0;
	}
}

template<typename T> struct VertData {
	float	score;
	int		cache;
	int		nfaces;
	int		rfaces;
	T		*faces;

	VertData() : cache(-1), nfaces(0), rfaces(0), faces(NULL)	{}
	~VertData() { delete[] faces; }
};

template<typename T> struct FaceData {
	float	score;
	T		v[3];
};

float TestCache(range<uint16*> faces, int cachesize);

template<typename T> void MeshOptimise(range<T*> faces, uint32 nverts, const MeshCacheParams &params) {
	uint32	nfaces = faces.size32() / 3;
	if (nfaces == 0)
		return;

//	float	test1 = TestCache(faces, nfaces, 16);

	temp_array<FaceData<T>>	fd(nfaces);
	temp_array<VertData<T>>	vd(nverts);

	for (uint32 i = 0; i < nfaces; i++) {
		for (uint32 j = 0; j < 3; j++) {
			int	v		= faces[i * 3 + j];
			fd[i].v[j]	= v;
			vd[v].nfaces++;
		}
	}

	for (uint32 i = 0; i < nfaces; i++) {
		for (uint32 j = 0; j < 3; j++) {
			VertData<T>	&v		= vd[faces[i * 3 + j]];
			if (v.faces == NULL)
				v.faces = new T[v.nfaces];
			v.faces[v.rfaces++] = i;
		}
	}

	for (uint32 vi = 0; vi < nverts; vi++)
		vd[vi].score = params.ValenceScore(vd[vi].rfaces);

	for (uint32 i = 0; i < nfaces; i++) {
		FaceData<T>	&f = fd[i];
		f.score		= vd[f.v[0]].score + vd[f.v[1]].score + vd[f.v[2]].score;
	}

	T		*cache	= alloc_auto(T, params.size + 3);
	int		ncache	= 0;
	int		besti	= 0;
	float	bests	= 0;

	for (uint32 di = 0; di < nfaces; di++) {

		if (bests < 2) {
			for (uint32 fi = 0; fi < nfaces; fi++) {
				if (fd[fi].score > bests) {
					bests = fd[fi].score;
					besti = fi;
				}
			}
		}

		fd[besti].score = 0;

		// update FIFO
		int	maxc = 0;
		for (uint32 i = 0; i < 3; i++) {
			uint32		vi	= fd[besti].v[i];
			VertData<T>	&v	= vd[vi];

			faces[di * 3 + i]	= vi;

			// remove used face
			for (int j = 0; j < v.rfaces; j++) {
				if (v.faces[j] == besti) {
					v.faces[j] = v.faces[--v.rfaces];
					break;
				}
			}

			int	c = v.cache;
			if (c < 0)
				c = ncache++;
			if (c > maxc)
				maxc = c;
			for (uint32 j = c; j--; ) {
				uint32	c = cache[j];
				cache[j + 1] = c;
				vd[c].cache = j + 1;
			}
			cache[0] = vi;
		}

		// update affected verts
		for (uint32 i = 0; i < maxc + 1; i++) {
			VertData<T>	&v	= vd[cache[i]];
			v.cache			= i < params.size ? i : -1;
			v.score			= params.table[i] + params.ValenceScore(v.rfaces);
		}

		if (ncache > params.size)
			ncache = params.size;

		// update affected faces (& find next one)
		besti = 0;
		bests = 0;
		for (uint32 i = 0; i < ncache; i++) {
			VertData<T>	&v	= vd[cache[i]];
			for (uint32 j = 0; j < v.rfaces; j++) {
				uint32		fi = v.faces[j];
				FaceData<T>	&f = fd[fi];
				f.score = vd[f.v[0]].score + vd[f.v[1]].score + vd[f.v[2]].score;
				if (f.score > bests) {
					bests = f.score;
					besti = fi;
				}
			}
		}
	}

	delete[] fd;
	delete[] vd;

//	float	test16	= TestCache(faces, nfaces, 16);
//	float	test32	= TestCache(faces, nfaces, 32);
//	float	test1024= TestCache(faces, nfaces, 1024);
//	float	test4 = TestCache(faces, nfaces, 4);
}

float TestCache(range<uint16*> faces, uint32 cachesize) {
	int	cache[1024];
	for (uint32 i = 0; i < cachesize; i++)
		cache[i] = -1;

	uint32	cp		= 0;
	uint32	misses	= 0;

	for (auto v : faces) {
		bool	hit	= false;
		for (uint32 i = 0; i < cachesize; i++) {
			if (cache[i] == v) {
				hit = true;
				break;
			}
		}
		if (!hit) {
			cache[cp] = v;
			cp = (cp + 1) % cachesize;
			misses++;
		}
	}

	return float(misses) / faces.size() * 3;
}

template void MeshOptimise(range<uint32*> faces, uint32 nverts, const MeshCacheParams &params);
template void MeshOptimise(range<uint16*> faces, uint32 nverts, const MeshCacheParams &params);

//-----------------------------------------------------------------------------
//	MeshSort sort faces from back to front
//-----------------------------------------------------------------------------

template<typename T> void MeshSort(range<T*> faces, stride_iterator<float3p> verts, param(float3) centre) {
	uint32		nfaces = faces.size() / 3;
	temp_array<float>	d(nfaces);
	temp_array<uint32>	x = int_range(nfaces);

	for (uint32 i = 0; i < nfaces; i++) {
		float3	v0	= (float3)verts[faces[3 * i + 0]];
		float3	v1	= (float3)verts[faces[3 * i + 1]];
		float3	v2	= (float3)verts[faces[3 * i + 2]];
		float3	n	= normalise(cross(v1 - v0, v2 - v0));
		d[i] = dot(n, v0.xyz - centre);
	}
	sort(x, [&d](uint32 a, uint32 b) { return d[a] < d[b]; });

	temp_array<T>	old_faces(faces);
	for (uint32 i = 0; i < nfaces; i++)
		memcpy(faces.begin() + 3 * i, old_faces.begin() + 3 * x[i], sizeof(T) * 3);
}

template void MeshSort<uint32>(range<uint32*> faces, stride_iterator<float3p> verts, param(float3) centre);
template void MeshSort<uint16>(range<uint16*> faces, stride_iterator<float3p> verts, param(float3) centre);

//-----------------------------------------------------------------------------
//	normals
//-----------------------------------------------------------------------------

void NormalRecord::Add(param(float3) n, uint32 s) {
	NormalRecord	*v = this;
	if (smooth) {
		while (!(s & v->smooth)) {
			if (!v->next) {
				v->next = new NormalRecord(n, s);
				return;
			}
			v	= v->next;
		}
	}
	v->normal	+= n;
	v->smooth	|= s;
}

float3 NormalRecord::Get(uint32 s) {
	NormalRecord	*v = this;
	while (!(s & v->smooth) && v->next)
		v = v->next;
	return v->normal;
}

void NormalRecord::Normalise() {
	for (NormalRecord *ptr = next, *prev = this; ptr; ptr = ptr->next) {
		if (ptr->smooth & smooth) {
			normal		+= ptr->normal;
			prev->next	= ptr->next;
			ptr->next	= NULL;
			delete ptr;
			ptr			= prev;
		} else {
			prev		= ptr;
		}
	}
	normal = normalise(normal);
	if (next)
		next->Normalise();
}

struct AdjacencyTri;
struct AdjacencyEdge	: cg::tri_edge<AdjacencyTri, AdjacencyEdge> { int vert;};
struct AdjacencyTri		: cg::tri_face<AdjacencyTri, AdjacencyEdge> {};

class AdjacencyEdges {
	typedef	pair<int, AdjacencyEdge*>	item;
	typedef	dynamic_array<item>			type;
	type	*p;
public:
	AdjacencyEdges(int n)	{ p = new type[n]; }
	~AdjacencyEdges()		{ delete[] p; }

	AdjacencyEdge	*find(int v0, int v1)	{
		type	&a	= p[v0];
		for (size_t i = 0, n = a.size(); i < n; i++) {
			if (a[i].a == v1)
				return a[i].b;
		}
		return 0;
	}
	void	add(AdjacencyEdge &e, int v0, int v1) {
		e.vert	= v0;
		e.set_flip(find(v1, v0));
		p[v0].emplace_back(v1, &e);
	}
};

void GetFaceGroups(const float3 *face_norms, array<uint16,3> *faces, int num_faces, int num_verts) {
	AdjacencyTri	*tris = new AdjacencyTri[num_faces];

	AdjacencyEdges	edges(num_verts);
	for (int i = 0; i < num_faces; i++) {
		AdjacencyTri			&t	= tris[i];
		array<uint16,3>	&f	= faces[i];
		edges.add(t[0], f[0], f[1]);
		edges.add(t[1], f[1], f[2]);
		edges.add(t[2], f[2], f[0]);
	}
}

//-----------------------------------------------------------------------------
//	Tangents
//-----------------------------------------------------------------------------
/*
void GenerateTangents(uint16 *faces, uint32 nfaces, stride_iterator<float3p> verts, uint32 nverts, uint32 norm_offset, uint32 uv_offset, uint32 tan_offset) {
	aligned<float3, 16>	*tan1	= new aligned<float3, 16>[nverts];
	aligned<float3, 16>	*tan2	= new aligned<float3, 16>[nverts];
	memset(tan1, 0, nverts * sizeof(float3));
	memset(tan2, 0, nverts * sizeof(float3));

	for (int i = 0; i < nfaces; i++) {
		int			i0	= faces[i * 3 + 0];
		int			i1	= faces[i * 3 + 1];
		int			i2	= faces[i * 3 + 2];
		ISO_ASSERT(i0 < nverts && i1 < nverts && i2 < nverts);

		float3p		&p0	= verts[i0];
		float3p		&p1	= verts[i1];
		float3p		&p2	= verts[i2];

		float2		t0	= *(float2p*)((uint8*)&p0 + uv_offset);
		float2		t1	= *(float2p*)((uint8*)&p1 + uv_offset) - t0;
		float2		t2	= *(float2p*)((uint8*)&p2 + uv_offset) - t0;

		float2x2	m	= float2x2(t1, t2);
		if (m.det() == zero)
			continue;

		m = inverse(m);

		float3	v0	= p0;
		float3	v1	= p1 - v0;
		float3	v2	= p2 - v0;
		float3	sdir(v1 * m.xx + v2 * m.xy);
		float3	tdir(v1 * m.yx + v2 * m.yy);

		tan1[i0] += sdir;
		tan1[i1] += sdir;
		tan1[i2] += sdir;

		tan2[i0] += tdir;
		tan2[i1] += tdir;
		tan2[i2] += tdir;
	}

	for (int i = 0; i < nverts; i++) {
		uint8	*p	= (uint8*)&verts[i];
		float3	n	= *(float3p*)(p + norm_offset);
		float3	s	= tan1[i];
		float3	t	= tan2[i];

		*(float4p*)(p + tan_offset) = all(s == zero) ? float4(zero) : float4(normalise(orthogonalise(s, n)), one);
	}

	delete[] tan1;
	delete[] tan2;
}
*/
bool GenerateTangents(SubMesh *submesh, const char *uvs) {
#if 0
	ISO::TypeOpenArray			*type	= (ISO::TypeOpenArray*)submesh->verts.GetType();
	const ISO::TypeComposite	*comp	= (ISO::TypeComposite*)type->subtype->SkipUser();
	const ISO::Element			*e;

	if (!(e = comp->Find("tangent")))
		return false;
	uint32 tan_offset	= e->offset;

	if (!(e = comp->Find("normal")))
		return false;
	uint32 norm_offset = e->offset;

	if (!(e = comp->Find(uvs)))
		return false;
	*/

	uint32 uv_offset = e->offset;
	GenerateTangents(&submesh->indices[0][0], submesh->indices.Count(), submesh->VertComponentData(0), ((ISO_openarray<void>*)submesh->verts)->Count(), norm_offset, uv_offset, tan_offset);
#else
	auto	uv		= submesh->VertComponentData<float2p>(uvs);
	if (!uv)
		return false;
	auto	norm	= submesh->VertComponentData<float3p>("normal");
	if (!norm)
		return false;
	auto	tan		= submesh->VertComponentData<float4p>("tangent");
	if (!tan)
		return false;

	GenerateTangents(
		submesh->indices,
		submesh->VertComponentRange<float3p>(0),
		norm, uv, tan
	);
#endif
	return true;
}

//-----------------------------------------------------------------------------
//	ModelBuilder
//-----------------------------------------------------------------------------

void MeshBuilder::AddFace(uint32 v0, uint32 v1, uint32 v2) {
	f.emplace_back(v0, v1, v2);
	//SubMesh::face	*face = f._expand();
	//(*face)[0] = v0;
	//(*face)[1] = v1;
	//(*face)[2] = v2;
}

int MeshBuilder::TryAdd(uint32 v) {
	auto	&i = map[v];
	if (i == 0) {
		if (num_verts >= max_verts_per_mesh)
			return -1;
		i = ++num_verts;
	}
	return i - 1;
}

void MeshBuilder::Purge(SubMesh::face *destf, void *destv, const void *srcev, uint32 vert_size) {
	GetFaces(destf);
	for (uint32 i = 0, n = map.size32(); i < n; ++i) {
		if (map[i])
			memcpy((char*)destv + (map[i] - 1) * vert_size, (const char*)srcev + i * vert_size, vert_size);
	}
	_ClearBatch();
}

ModelBuilder::ModelBuilder(tag id, ISO_ptr<technique> _t, ISO_ptr<void> _p, uint32 _flags) : ISO_ptr<Model3>(id), vert_type(0), t(_t), p(_p), submesh_flags(_flags) {
}

void ModelBuilder::SetMaterial(ISO_ptr<technique> _t, ISO_ptr<void> _p, uint32 _flags) {
	t				= _t;
	p				= _p;
	submesh_flags	= _flags;
}

void ModelBuilder::_SetVertType(const ISO::Type *_vert_type) {
	ISO_ASSERT(_vert_type->GetType() != ISO::OPENARRAY);
	vert_type		= _vert_type;
	vertarray_type	= new ISO::TypeOpenArray(vert_type);
	vert_size		= vert_type->GetSize();
}

void ModelBuilder::_SetVertArrayType(const ISO::Type *_vertarray_type) {
	ISO_ASSERT(_vertarray_type->GetType() == ISO::OPENARRAY);
	vertarray_type	= (ISO::TypeOpenArray*)_vertarray_type;
	vert_type		= _vertarray_type->SubType();
	vert_size		= vert_type->GetSize();
}

SubMesh *ModelBuilder::_AddMesh(uint32 num_verts, uint32 num_indices) {
	ISO_ptr<SubMesh>	mesh(0);
	mesh->technique		= t;
	mesh->parameters	= p;
	mesh->flags			= submesh_flags;

	mesh->indices.Create(num_indices);
	mesh->verts = MakePtr(vertarray_type);
	((ISO_openarray<void>*)mesh->verts)->Create(vert_type, num_verts);

	(*this)->submeshes.Append(mesh);
	return mesh;
}

SubMesh *ModelBuilder::AddMesh(const ISO::Type *_vert_type, uint32 num_verts, uint32 num_indices) {
	_SetVertType(_vert_type);
	return _AddMesh(num_verts, num_indices);
}

void ModelBuilder::InitVerts(const ISO::Type *_vert_type, uint32 _max_verts) {
	_SetVertType(_vert_type);
	ReserveVerts(_max_verts);
}

void ModelBuilder::Purge(const void *verts) {
	SubMesh *mesh = _AddMesh((uint32)NumVerts(), (uint32)NumFaces());
	MeshBuilder::Purge(mesh->indices, mesh->VertData(), verts, vert_size);
	mesh->UpdateExtent();
}

//-----------------------------------------------------------------------------

void TransformVertices(const ISO_ptr<void> &verts, uint32 count1, uint32 count2, param(float3x4) tm) {
	ISO::TypeOpenArray		*vertstype	= (ISO::TypeOpenArray*)verts.GetType()->SkipUser();
	uint32					vertsize	= vertstype->subsize;
	const ISO::TypeComposite &comp		= *(const ISO::TypeComposite*)vertstype->subtype->SkipUser();
	char					*verts1		= (char*)vertstype->ReadPtr(verts);

	for (auto &e : comp) {
		tag2	id = comp.GetID(&e);
		if (id == "position" || id == "centre") {
			stride_iterator<float3p> s((float3p*)(verts1 + e.offset), vertsize);
			for_each(s + count1, s + count1 + count2, [tm](float3p &v) { position3 p = position3(v); v = (tm * p).v; });

		} else if (id == "normal" || id == "tangent0") {
			stride_iterator<float3p> s((float3p*)(verts1 + e.offset), vertsize);
			for_each(s + count1, s + count1 + count2, [tm](float3p &v) { v = tm * float3(v); });
		}
	}
}

void OffsetBones(const ISO_ptr<void> &verts, uint32 count1, uint32 count2, int bone_offset) {
	ISO::TypeOpenArray		*vertstype	= (ISO::TypeOpenArray*)verts.GetType();
	uint32					vertsize	= vertstype->subsize;
	const ISO::TypeComposite &comp		= *(const ISO::TypeComposite*)vertstype->subtype->SkipUser();
	char					*verts1		= (char*)vertstype->ReadPtr(verts);

	for (auto &e : comp) {
		tag2	id = comp.GetID(&e);
		if (id == "bones") {
			int		nb	= TypeType(e.type) == ISO::ARRAY ? ((ISO::TypeArray*)e.type.get())->count : 1;
			for (int j = count1; j < count1 + count2; j++) {
				uint16	*v	= (uint16*)(verts1 + e.offset + vertsize * j);
				for (int k = 0; k < nb; k++)
					v[k] += bone_offset;
			}
		}
	}
}

void AppendIndices(SubMesh *d, SubMesh *s, int dcount, int scount, int offset) {
	auto		*pd = &d->indices[0][0] + dcount * 3;
	const auto	*ps = &s->indices[0][0];
	for (int i = 0; i < scount * 3; i++)
		*pd++ = *ps++ + offset;
}

void ModelMerge(Model3 *model1, Model3 *model2, const float3x4 *tm, int bone_offset) {
	if (!model1 || !model2)
		return;

	for (SubMeshBase *submesh2 : model2->submeshes) {
		SubMeshBase	*submesh1 = NULL;
		for (SubMeshBase *submesh : model1->submeshes) {
			if (CompareData(submesh2->technique,  submesh->technique)
			&&	CompareData(submesh2->parameters, submesh->parameters)) {
				submesh1 = submesh;
				break;
			}
		}
		if (!submesh1) {
			ISO_ptr<SubMesh>	submesh = Duplicate(ISO::GetPtr(submesh2));
			submesh->verts	= Duplicate(submesh->verts);
			model1->submeshes.Append(submesh);

			ISO_openarray<void>	*verts2			= submesh->verts;
			int					count2			= verts2->Count();

			if (tm)
				TransformVertices(submesh->verts, 0, count2, *tm);

			if (bone_offset)
				OffsetBones(submesh->verts, 0, count2, bone_offset);

		} else {
			SubMesh				*sm1			= (SubMesh*)submesh1;
			SubMesh				*sm2			= (SubMesh*)submesh2;
			ISO::TypeOpenArray	*vertstype		= (ISO::TypeOpenArray*)sm1->verts.GetType();
			uint32				vertsize		= vertstype->subsize;
			ISO_openarray<void>	*verts1			= sm1->verts;
			ISO_openarray<void>	*verts2			= sm2->verts;
			int					count1			= verts1->Count();
			int					count2			= verts2->Count();

			verts1->Resize(vertsize, 4, count1 + count2);
			memcpy(verts1->GetElement(count1, vertsize), verts2->GetElement(0, vertsize), vertsize * count2);

			if (tm)
				TransformVertices(sm1->verts, count1, count2, *tm);

			if (bone_offset)
				OffsetBones(sm1->verts, count1, count2, bone_offset);

			int					icount1			= sm1->indices.Count();
			int					icount2			= sm2->indices.Count();

			sm1->indices.Resize(icount1 + icount2);
			AppendIndices(sm1, sm2, icount1, icount2, count1);

			if (tm) {
				sm1->UpdateExtent();
			} else {
				submesh1->minext	= min(float3(submesh1->minext), float3(submesh2->minext));
				submesh1->maxext	= max(float3(submesh1->maxext), float3(submesh2->maxext));
			}

			model1->minext		= min(float3(model1->minext), float3(submesh2->minext));
			model1->maxext		= max(float3(model1->maxext), float3(submesh2->maxext));
		}
	}
}


ISO_ptr<SubMesh> MakeSubMeshLike(const SubMesh *a) {
	ISO_ptr<SubMesh>	sm(0);
	sm->technique	= a->technique;
	sm->parameters	= a->parameters;
	sm->verts		= MakePtr(a->verts.GetType());
	return sm;
}

void Interpolate(const ISO::Type *type, void *d, const void *p0, const void *p1, float t) {
	switch (type->GetType()) {
		case ISO::INT: {
			const ISO::TypeInt *i = (const ISO::TypeInt*)type;
			i->set(d, lerp(i->get(const_cast<void*>(p0)), i->get(const_cast<void*>(p1)), t));
			break;
		}

		case ISO::FLOAT: {
			const ISO::TypeFloat *f = (const ISO::TypeFloat*)type;
			f->set(d, lerp(f->get(const_cast<void*>(p0)), f->get(const_cast<void*>(p1)), t));
			break;
		}

		case ISO::COMPOSITE:
			for (auto &e : *(const ISO::TypeComposite*)type)
				Interpolate(e.type, (char*)d + e.offset, (const char*)p0 + e.offset, (const char*)p1 + e.offset, t);
			break;

		case ISO::ARRAY: {
			const ISO::TypeArray *a = (const ISO::TypeArray*)type;
			uint32	offset = 0;
			for (int i = 0, n = a->Count(); i < n; i++, offset += a->subsize)
				Interpolate(a->subtype, (char*)d + offset, (const char*)p0 + offset, (const char*)p1 + offset, t);
			break;
		}
		case ISO::USER:
			Interpolate(((const ISO::TypeUser*)type)->subtype, d, p0, p1, t);
			break;
	}
}

void Interpolate(const ISO::Type *type, void *d, const void *p0, const void *p1, const void *p2, float2 t) {
	switch (type->GetType()) {
		case ISO::INT: {
			const ISO::TypeInt *i = (const ISO::TypeInt*)type;
			i->set(d, bilerp(i->get(const_cast<void*>(p0)), i->get(const_cast<void*>(p1)), i->get(const_cast<void*>(p2)), t));
			break;
		}

		case ISO::FLOAT: {
			const ISO::TypeFloat *f = (const ISO::TypeFloat*)type;
			f->set(d, bilerp(f->get(const_cast<void*>(p0)), f->get(const_cast<void*>(p1)), f->get(const_cast<void*>(p2)), t));
			break;
		}

		case ISO::COMPOSITE:
			for (auto &e : *(const ISO::TypeComposite*)type)
				Interpolate(e.type, (char*)d + e.offset, (const char*)p0 + e.offset, (const char*)p1 + e.offset, (const char*)p2 + e.offset, t);
			break;

		case ISO::ARRAY: {
			const ISO::TypeArray *a = (const ISO::TypeArray*)type;
			uint32	offset = 0;
			for (int i = 0, n = a->Count(); i < n; i++, offset += a->subsize)
				Interpolate(a->subtype, (char*)d + offset, (const char*)p0 + offset, (const char*)p1 + offset, (const char*)p2 + offset, t);
			break;
		}
		case ISO::USER:
			Interpolate(((const ISO::TypeUser*)type)->subtype, d, p0, p1, p2, t);
			break;
	}
}

//-----------------------------------------------------------------------------

ISO_ptr<void> AnythingToStruct(anything &params, tag2 id) {
	if (int n = params.Count()) {
		ISO::TypeComposite	*comp = new(n) ISO::TypeComposite;
		for (int i = 0; i < n; ++i)
			comp->Add(params[i].GetType(), params[i].ID());

		ISO_ptr<void>	p	= MakePtr(comp, id);
		ISO::Browser	b(p);
		for (int i = 0; i < n; ++i)
			b[i].Set(params[i]);

		return p;
	}
	return ISO_NULL;
}

ISO_ptr<void> AnythingToNamedStruct(tag2 name, anything &params, tag2 id) {
	int		n = params.Count();
	ISO::TypeUserComp	*comp	= new(n) ISO::TypeUserComp(name, 0, ISO::TypeUserComp::DONTKEEP);
	for (int i = 0; i < n; ++i)
		comp->Add(params[i].GetType(), params[i].ID());

	ISO_ptr<void>	p	= MakePtr(comp, id);
	ISO::Browser		b(p);
	for (int i = 0; i < n; ++i)
		b[i].Set(params[i]);

	return p;
}

ISO_ptr<void> AssignToStruct(ISO::Type *t, anything &params, tag2 id) {
	auto	p		= MakePtr(t, id);
	ISO::Browser	b(p);
	for (int i = 0, n = params.Count(); i < n; ++i)
		b[i].Set(params[i]);

	return p;
}

bool HasDoubles(const ISO::Type *type) {
	while (type->GetType() == ISO::ARRAY)
		type = type->SubType();
	return type->Is<double>();
}

const ISO::Type *RemoveDoubles(const ISO::Type *type) {
	if (!HasDoubles(type))
		return type;

	switch (type->GetType()) {
		case ISO::ARRAY: {
			auto	array = (const ISO::TypeArray*)type;
			return new ISO::TypeArray(RemoveDoubles(array->subtype), array->count);
		}
		default:
			if (type->Is<double>())
				return ISO::getdef<float>();
			return type;
	}
	return type;
}

ISO_ptr<void> RemoveDoubles(ISO_ptr<void> verts) {
	ISO::Browser	b(verts);

	if (auto comp = ISO::TypeAs<ISO::COMPOSITE>(b[0].GetTypeDef()->SkipUser())) {
		for (auto &el : *comp) {
			if (HasDoubles(el.type)) {
				ISO::TypeCompositeN<64>	builder(0);
				for (auto &el : *comp)
					builder.Add(RemoveDoubles(el.type), comp->GetID(comp->index_of(el)));

				return ISO_conversion::convert(verts, new ISO::TypeArray(builder.Duplicate(), b.Count()));
			}
		}
	}

	return verts;
}

} // namespace iso
