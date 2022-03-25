#ifndef MODEL_UTILS_H
#define MODEL_UTILS_H

#include "maths/polygon.h"
#include "systems/mesh/model_iso.h"

namespace iso {

struct MeshBuilder {
	dynamic_array<SubMesh::face>	f;
	dynamic_array<uint16>			map;
	uint32							num_verts;

	void		_ClearBatch() {
		f.clear();
		map.raw_data().clear_contents();
		num_verts = 0;
	}

	MeshBuilder() {}
	MeshBuilder(uint32 max_tris, uint32 max_verts) {
		ReserveFaces(max_tris);
		ReserveVerts(max_verts);
	}
	void	ReserveFaces(uint32 n) {
		f.reserve(n);
	}
	void	ReserveVerts(uint32 n) {
		map.resize(n);
		_ClearBatch();
	}

	void	AddFace(uint32 v0, uint32 v1, uint32 v2);
	int		TryAdd(uint32 v);
	void	Purge(SubMesh::face *destf, void *destv, const void *srcev, uint32 vert_size);
	void	Purge(SubMesh *sm, const void *srcev, uint32 vert_size) {
		Purge(sm->indices, sm->VertData(), srcev, vert_size);
	}

	uint32	NumFaces()						const { return f.size32(); }
	uint32	NumVerts()						const { return num_verts; }
	void	GetFaces(SubMesh::face *destf)	const { memcpy(destf, f, f.size() * sizeof(SubMesh::face)); }
};

inline stride_iterator<void> GetArrayComponent(const ISO::Element *e, void *p, uint32 stride) {
	return e ? stride_iterator<void>((uint8*)p + e->offset, stride) : stride_iterator<void>(0, 0);
}
inline stride_iterator<void> GetVertexComponent(const ISO::Type *type, void *p, uint32 i) {
	return GetArrayComponent(&(*((ISO::TypeComposite*)type->SkipUser()))[i], p, type->GetSize());
}
inline stride_iterator<void> GetVertexComponent(const ISO::Type *type, void *p, tag2 id) {
	return GetArrayComponent(((ISO::TypeComposite*)type->SkipUser())->Find(id), p, type->GetSize());
}

template<typename T> stride_iterator<T> GetArrayComponent(const ISO::Element *e, void *p, uint32 stride)	{ return GetArrayComponent(e, p, stride); }
template<typename T> stride_iterator<T> GetVertexComponent(const ISO::Type *type, void *p, uint32 i)		{ return GetVertexComponent(type, p, i); }
template<typename T> stride_iterator<T> GetVertexComponent(const ISO::Type *type, void *p, tag2 id)			{ return GetVertexComponent(type, p, id); }
template<typename T> stride_iterator<T> GetVertexComponent(ISO_ptr<void> p, uint32 i)			{ return GetVertexComponent(p.GetType(), p, i); }
template<typename T> stride_iterator<T> GetVertexComponent(ISO_ptr<void> p, tag2 id)			{ return GetVertexComponent(p.GetType(), p, id); }

struct ModelBuilder : ISO_ptr<Model3>, MeshBuilder {
	const ISO::Type					*vert_type;
	ISO::TypeOpenArray				*vertarray_type;
	ISO_ptr<technique>				t;
	ISO_ptr<void>					p;
	uint32							vert_size;
	uint32							submesh_flags;

	void		_SetVertType(const ISO::Type *_vert_type);
	void		_SetVertArrayType(const ISO::Type *_vertarray_type);
	SubMesh*	_AddMesh(uint32 _num_verts, uint32 _num_indices);

	ModelBuilder(tag id, ISO_ptr<technique> _t = ISO_NULL, ISO_ptr<void> _p = ISO_NULL, uint32 _flags = 0);

	void	SetMaterial(ISO_ptr<technique> _t, ISO_ptr<void> _p = ISO_NULL, uint32 _flags = 0);
	SubMesh	*AddMesh(const ISO::Type *_vert_type, uint32 _num_verts, uint32 _num_indices);

	stride_iterator<void> GetVertexComponent(void *p, uint32 i)	{ return iso::GetVertexComponent(vert_type, p, i); }
	stride_iterator<void> GetVertexComponent(void *p, tag2 id)	{ return iso::GetVertexComponent(vert_type, p, id); }
	template<typename T> stride_iterator<T> GetVertexComponent(void *p, uint32 i)	{ return GetVertexComponent(p, i); }
	template<typename T> stride_iterator<T> GetVertexComponent(void *p, tag2 id)	{ return GetVertexComponent(p, id); }

	void	InitVerts(const ISO::Type *_vert_type, uint32 _maxv);
	void	Purge(const void *verts);
};

ISO_ptr<void> RemoveDoubles(ISO_ptr<void> verts);

//-----------------------------------------------------------------------------
//	normals
//-----------------------------------------------------------------------------

class NormalRecord : public aligner<16> {
public:
	float3			normal;
	uint32			smooth;
	NormalRecord	*next;

	NormalRecord()							: normal(zero), smooth(0), next(0)	{}
	NormalRecord(param(float3) n, uint32 s)	: normal(n), smooth(s), next(0) 	{}
	~NormalRecord()							{ delete next;	}
	void		Add(param(float3) n, uint32 s);
	float3		Get(uint32 s);
	void		Normalise();
};

inline float3 GetNormal(param(position3) p0, param(position3) p1, param(position3) p2) {
	return cross(p1 - p0, p2 - p0);
}
template<typename I> inline float3 GetNormal(I i) {
	return GetNormal(position3(i[0]), position3(i[1]), position3(i[2]));
}
template<typename I> inline float GetArea(I i) {
	return len(GetNormal(i)) * half;
}

template<typename I> void GetFaceNormals(float3 *face_norms, I faces, uint32 nfaces) {
	while (nfaces--) {
		*face_norms++	= GetNormal(*faces);
		++faces;
	}
}

template<typename I> inline void AddNormal(NormalRecord *norms, I i, const float3 &fn) {
	norms[i[0]].Add(fn, 1);
	norms[i[1]].Add(fn, 1);
	norms[i[2]].Add(fn, 1);
}
template<typename I> void AddNormals(NormalRecord *norms, const float3 *face_norms, I faces, uint32 nfaces) {
	while (nfaces--) {
		AddNormal(norms, *faces, *face_norms++);
		++faces;
	}
}

//-----------------------------------------------------------------------------
//	tangents
//-----------------------------------------------------------------------------

template<typename FC, typename VC, typename NI, typename UI, typename TI>
void GenerateTangents(const FC &faces, VC verts, NI norms, UI uvs, TI tans) {
	size_t	nverts = verts.size();
	dynamic_array<float3>	tan1(nverts, zero), tan2(nverts, zero);

	for (auto &face : faces) {
		int			i0	= face[0];
		int			i1	= face[1];
		int			i2	= face[2];
		ISO_ASSERT(i0 < nverts && i1 < nverts && i2 < nverts);

		float2		t0	= uvs[i0];
		float2		t1	= (float2)uvs[i1] - t0;
		float2		t2	= (float2)uvs[i2] - t0;

		float2x2	m	= float2x2(t1, t2);
		if (m.det() == zero)
			continue;

		m = inverse(m);

		float3	v0	= (float3)verts[i0];
		float3	v1	= (float3)verts[i1] - v0;
		float3	v2	= (float3)verts[i2] - v0;
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
		float3	n	= (float3)norms[i];
		float3	s	= (float3)tan1[i];
		//float3	t	= (float3)tan2[i];
		tans[i] = all(s == zero) ? float4(zero) : concat(normalise(orthogonalise(s, n)), one);
	}
}

bool GenerateTangents(SubMesh *submesh, const char *uvs);

//-----------------------------------------------------------------------------
//	cache
//-----------------------------------------------------------------------------

struct MeshCacheParams {
	uint32	size;
	float	valence_scale;
	float	valence_power;
	float	*table;

	float ValenceScore(int rfaces) const {
		return valence_scale * pow(float(rfaces), -valence_power);
	}

	MeshCacheParams(uint32 _size, float decay_power	= 1.5f, float last_tri_score = 0.75f, float _valence_scale = 2.0f, float _valence_power = 0.5f);
	~MeshCacheParams() {
		delete[] table;
	}
};

template<typename T> T *MeshSort(T *faces, uint32 nfaces, stride_iterator<float3p> verts, param(float3) centre);
template<typename T> T *MeshOptimise(T *faces, uint32 nfaces, uint32 nverts, const MeshCacheParams &params);

const ISO::Element *GetVertexComponentElement(const ISO_ptr<void> &verts, tag2 id);

void TransformVertices(const ISO_ptr<void> &verts, uint32 count1, uint32 count2, param(float3x4) tm);
void OffsetBones(const ISO_ptr<void> &verts, uint32 count1, uint32 count2, int bone_offset);
void ModelMerge(Model3 *model1, Model3 *model2, const float3x4 *tm = 0, int bone_offset = 0);
void Reindex(Model3 *model);
void AppendIndices(SubMesh *d, SubMesh *s, int dcount, int scount, int offset);
ISO_ptr<SubMesh> MakeSubMeshLike(const SubMesh *a);
void Interpolate(const ISO::Type *type, void *d, const void *p0, const void *p1, float t);
void Interpolate(const ISO::Type *type, void *d, const void *p0, const void *p1, const void *p2, float2 t);

ISO_ptr<void> AnythingToStruct(anything &params, tag2 id = tag2());
ISO_ptr<void> AnythingToNamedStruct(tag2 name, anything &params, tag2 id);
ISO_ptr<void> AssignToStruct(ISO::Type *t, anything &params, tag2 id);

uint32 GetCacheCost(const uint32 *indices, uint32 ntris, uint32 fifo_size = 14, uint32 alternatePrim = 256);
template<typename T> uint32 GetCacheCost(const T *indices, uint32 ntris, uint32 fifo_size = 14) {
	uint32	*temp	= new uint32[ntris * 3];
	copy_n(indices, temp, ntris * 3);
	uint32 cost = GetCacheCost(temp, ntris, fifo_size);
	delete[] temp;
	return cost;
}

uint32 VertexCacheOptimizerHillclimber(const uint32 *indices, uint32 ntris, uint32 nverts, uint32 *outIndices, uint32 hillClimberIterations, uint32 fifo_size = 14, uint32 alternatePrim = 256);
template<typename T> uint32 VertexCacheOptimizerHillclimber(const T *indices, uint32 ntris, uint32 nverts, T *outIndices, uint32 hillClimberIterations, uint32 fifo_size = 14, uint32 alternatePrim = 256) {
	uint32	*temp	= new uint32[ntris * 3];
	uint32	*out	= new uint32[ntris * 3];
	copy_n(indices, temp, ntris * 3);
	uint32 cost = VertexCacheOptimizerHillclimber(temp, ntris, nverts, out, hillClimberIterations, fifo_size, alternatePrim);
	copy_n(out, outIndices, ntris * 3);
	delete[] temp;
	return cost;
}

void VertexCacheOptimizerForsyth(const uint32* indices, uint32 ntris, uint32 nverts, uint32 *outIndices, uint32 cache_size = 32);
template<typename T> void VertexCacheOptimizerForsyth(const T *indices, uint32 ntris, uint32 nverts, T *outIndices, uint32 cache_size = 32) {
	uint32	*temp	= new uint32[ntris * 3];
	uint32	*out	= new uint32[ntris * 3];
	copy_n(indices, temp, ntris * 3);
	VertexCacheOptimizerForsyth(temp, ntris, nverts, out);
	copy_n(out, outIndices, ntris * 3);
	delete[] temp;
}

}


#endif //MODEL_UTILS_H
