#include "base/vector.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "base/tree.h"
//#include "extra/indexer.h"
#include "extra/octree.h"
#include "extra/gpu_helpers.h"
#include "extra/rasterise.h"
#include "maths/csg.h"
#include "maths/statistics.h"
#include "maths/polygon.h"
#include "filetypes/3d/model_utils.h"
#include "vector_iso.h"
#include "utilities.h"

using namespace iso;
//-----------------------------------------------------------------------------
//	Trivial mappings
//-----------------------------------------------------------------------------

ISO_ptr<Model3> DefaultSceneModel(const Scene &p) {
	return ((ISO_ptr<Node>)(p.root->children[0]))->children[0];
}

ISO_ptr<Node> DefaultSceneNode(const Scene &p) {
	return ISO_ptr<Node>(p.root->children[0]);
}

ISO_ptr<Model3> DefaultNodeModel(Node &p) {
	return p.children.FindByType<Model3>();
}

ISO_ptr<technique> DefaultTechnique(const fx &p) {
	return p[0];
}

ISO_ptr<Model3> ModelFromSubMesh(const ISO_ptr<SubMesh> &p) {
	ISO_ptr<Model3>	p2(0);
	p2->submeshes.Append(p);
	p2->minext = p->minext;
	p2->maxext = p->maxext;
	return p2;
}

//-----------------------------------------------------------------------------
//	Model3
//-----------------------------------------------------------------------------

void FixMatrix(float3x4p &m, const float3x4 &tm) {
	m = tm * float3x4(m);
}

ISO_ptr<void> FixMatrix(ISO_ptr<void> &p, const float3x4 &tm) {
	if (ISO::Browser(p)["matrix"]) {
		ISO_ptr<void> p2	= Duplicate(p);
		float3x4p	*m		= ISO::Browser(p2)["matrix"];
		float3x4p	m2		= *m;
		m2 = tm * float3x4(m2);
		*m = m2;
		return p2;
	}
	return p;
}

ISO_ptr<void> FixAnything(ISO_ptr<void> &p, const float3x4 &tm) {
	if (p) {
		if (p.GetType()->SameAs<anything>()) {
			anything *s 	= p;
			ISO_ptr<anything> d(NULL);
			for (int i = 0, n = s->Count(); i < n; i++)
				d->Append(FixMatrix((*s)[i], tm));
			return d;
		}
		return FixMatrix(p, tm);
	}
	return ISO_ptr<void>(NULL);
}

void MergeModelsTransfer(ISO_ptr<void> child, ent::Splitter *splitter, const float3x4 &tm) {
	if (!splitter->hirez)
		splitter->hirez = ISO_ptr<anything>(0);

	ISO_ptr<anything>	hirez = splitter->hirez;
	if (child.GetType()->SameAs<anything>()) {
		anything	*s = child;
		for (int i = 0, n = s->Count(); i < n; i++)
			hirez->Append(FixMatrix((*s)[i], tm));
	} else {
		hirez->Append(FixMatrix(child, tm));
	}
}

ISO_ptr<Animation> FixAnimation(ISO_ptr<Animation> anim, const float3x4 &tm, BasePose *basepose) {
	return anim;
}

struct MergeModelsState {
	ISO_ptr<Model3>			model;
	ISO_ptr<BasePose>		basepose, my_basepose;
	ISO_ptr<anything>		children;
	ISO_ptr<ent::Splitter>	splitter;
	ISO_ptr<Bone>			root_bone;
	int						bone_offset;

	MergeModelsState() : model(0), children(0), splitter(0) {
		splitter->value			 = 0;
		splitter->value2		 = 0;
		splitter->value3		 = 0;
		splitter->split_decision = ent::Splitter::Distance;
	}

	void	Recurse(const anything &src, const float3x4 &tm) {
		bone_offset	= basepose ? basepose->Count() : 0;
		my_basepose.Clear();
		for (auto &i : src) {
			if (i.IsType("BasePose"))
				my_basepose = i;
		}
		for (auto &i : src)
			Recurse(i, tm);
	}
	void	Recurse(ISO_ptr<void> child, const float3x4 &tm);
};

void MergeModelsState::Recurse(ISO_ptr<void> child, const float3x4 &tm) {
	if (!child)
		return;

	if (child.IsExternal())
		child = FileHandler::ExpandExternals(child);

	if (child.IsType("Animation")) {
		children->Append(FixAnimation(child, tm, my_basepose));

	} else if (child.GetType()->SameAs<anything>()) {
		Recurse(*(ISO_ptr<anything>&)child, tm);

	} else if (child.IsType("Model3")) {
		ModelMerge(model, child, &tm, bone_offset);

	} else if (child.IsType("Node")) {
		Recurse(((Node*)child)->children, tm * float3x4(((Node*)child)->matrix));

	} else if (child.IsType("Scene")) {
		Recurse(((Scene*)child)->root->children, tm);

	} else if (child.IsType("BasePose")) {
		if (!basepose)
			basepose.Create();
		BasePose	*bp = child;
		for (BasePose::iterator i = bp->begin(), e = bp->end(); i != e; ++i) {
			ISO_ptr<Bone>	j = Duplicate(*i);
			FixMatrix(j->basepose, tm);
			j->parent = j->parent ? (*basepose)[j->parent.ID()] : root_bone;
			basepose->Append(j);
		}
		for (BasePose::iterator i = basepose->begin(), e = basepose->end(); !root_bone && i != e; ++i) {
			if (!(*i)->parent)
				root_bone = *i;
		}

	} else if (child.IsType("Collision_OBB")) {
		ISO_ptr<Collision_OBB>		p = Duplicate(child);
		children->Append(p);
		p->obb = tm * float3x4(p->obb);

	} else if (child.IsType("Collision_Sphere")) {
		ISO_ptr<Collision_Sphere>	p = Duplicate(child);
		children->Append(p);
		p->centre	= (tm * position3(p->centre)).v;

	} else if (child.IsType("Collision_Cylinder")) {
		ISO_ptr<Collision_Cylinder>	p = Duplicate(child);
		children->Append(p);
		p->centre	= (tm * position3(p->centre)).v;
		p->dir		= tm * float3(p->dir);

	} else if (child.IsType("Collision_Cone")) {
		ISO_ptr<Collision_Cone>		p = Duplicate(child);
		p->centre	= (tm * position3(p->centre)).v;
		p->dir		= tm * float3(p->dir);
		children->Append(p);

	} else if (child.IsType("Collision_Capsule")) {
		ISO_ptr<Collision_Capsule>	p = Duplicate(child);
		children->Append(p);
		p->centre	= (tm * position3(p->centre)).v;
		p->dir		= tm * float3(p->dir);

	} else if (child.IsType("Collision_Patch")) {
		ISO_ptr<Collision_Patch>	p = Duplicate(child);
		children->Append(p);
		for (int i = 0; i < 16; i++)
			p->p[i] = (tm * position3(p->p[i])).v;

	} else if (child.IsType("Spline")) {
		ISO_ptr<ent::Spline>		p = Duplicate(child);
		children->Append(p);
		for (int i = 0, n = p->pts.Count(); i < n; i++)
			p->pts[i] = (tm * position3(p->pts[i])).v;

	} else if (child.IsType("Cluster")) {
		ent::Cluster	*cluster 	= child;
		splitter->value				= max(splitter->value, cluster->distance);
		splitter->split_decision	= ent::Splitter::Distance;

		ModelMerge(model, cluster->lorez, &tm, bone_offset);
		MergeModelsTransfer(cluster->hirez, splitter, tm);

	} else if (child.IsType("Splitter")) {
		ent::Splitter	*splitter2 	= child;
		splitter->value				= max(splitter->value, splitter2->value);
		splitter->value2			= max(splitter->value2, splitter2->value2);
		splitter->value3			= max(splitter->value3, splitter2->value3);
		splitter->split_decision	= splitter2->split_decision;

		Recurse(splitter2->lorez, tm);
		MergeModelsTransfer(splitter2->hirez, splitter, tm);

	} else if (child.IsType("QualityToggle")) {
		ISO_ptr<ent::QualityToggle>	p = Duplicate(child);
		p->lorez = FixAnything(p->lorez, tm);
		p->hirez = FixAnything(p->hirez, tm);
		children->Append(p);

	} else if (!child.IsType("Light2") || ((ent::Light2*)child)->type != ent::Light2::AMBIENT) {
		children->Append(FixMatrix(child, tm));
	}
}

ISO_ptr<void> MergeModels(const anything &children) {
	MergeModelsState	mm;
	mm.Recurse(children, identity);

	if (mm.model->submeshes) {
		mm.model->UpdateExtents();
		CheckHasExternals(mm.model, ISO::DUPF_DEEP);
	}

	if (mm.splitter->hirez) {
		if (mm.model->submeshes) {
			mm.splitter->lorez = mm.model;
			mm.splitter.SetFlags(mm.model.Flags() & ISO::Value::HASEXTERNAL);
		}
		mm.children->Append(mm.splitter);
	} else {
		if (mm.model->submeshes) {
			mm.children->Append(mm.model);
			mm.children.SetFlags(mm.model.Flags() & ISO::Value::HASEXTERNAL);
		}
		if (mm.basepose) {
			mm.children->Append(mm.basepose);
		}
	}
	if (mm.children->Count())
		return mm.children;
	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	normals
//-----------------------------------------------------------------------------

void SetNormals(NormalRecord *norms, ISO_ptr<SubMesh> submesh) {
	ISO::TypeOpenArray	*vertstype1	= (ISO::TypeOpenArray*)submesh->verts.GetType();
	ISO::TypeComposite	*verttype1	= (ISO::TypeComposite*)vertstype1->subtype.get();
	uint32				vertsize1	= vertstype1->subsize;
	ISO_openarray<char>	*verts1		= submesh->verts;
	int					nverts		= verts1->Count();

	ISO::TypeCompositeN<64>	builder(0);
	for (int j = 0, n = verttype1->Count(); j < n; j++)
		builder.Add((*verttype1)[j]);
	builder.Add<float[3]>("normal");

	ISO::TypeComposite	*verttype2	= builder.Duplicate();
	ISO::TypeOpenArray	*vertstype2	= new ISO::TypeOpenArray(verttype2);
	uint32				vertsize2	= vertstype2->subsize;
	ISO_openarray<void>	verts2(vertsize2, nverts);

	for (int i = 0; i < nverts; i++) {
		float3			n	= norms[i].Get(1);
		memcpy(verts2.GetElement(i, vertsize2), *verts1 + i * vertsize1, vertsize1);
		((float3p*)verts2.GetElement(i + 1, vertsize2))[-1] = n;
	}

	submesh->verts = MakePtr(vertstype2);
	*(ISO_openarray<void>*)submesh->verts = verts2;
}

ISO_ptr<Model3> MakeNormals(ISO_ptr<Model3> model, float threshold) {
	model			= ISO_conversion::convert<Model3>(FileHandler::ExpandExternals(model));
	int		nsubs	= model->submeshes.Count();
#if 1
	for (int i = 0; i < nsubs; i++) {
		ISO_ptr<SubMesh> submesh = model->submeshes[i];
		int				nverts	= ((ISO_openarray<char>*)submesh->verts)->Count();
		int				ntris	= submesh->indices.Count();

		float3			*fnorms	= new float3[ntris];
		NormalRecord	*norms	= new NormalRecord[nverts];

		GetFaceNormals(fnorms,
			make_prim_iterator(
				Prim2Vert::trianglelist(),
				make_indexed_iterator(stride_iterator<float3p>(submesh->verts, ((ISO::TypeOpenArray*)submesh->verts.GetType())->subsize), make_const(&submesh->indices[0][0]))
			),
			ntris
		);
		AddNormals(norms, fnorms,
			make_prim_iterator(
				Prim2Vert::trianglelist(),
				&submesh->indices[0][0]
			),
			ntris
		);

		delete[] fnorms;
		for (int i = 0; i < nverts; i++)
			norms[i].Normalise();

		SetNormals(norms, submesh);
		delete[] norms;
	}
#else
	int		nverts	= 0;
	int		ntris	= 0;
	for (int i = 0; i < nsubs; i++) {
		ISO_ptr<SubMesh>	submesh = model->submeshes[i];
		nverts				+= ((ISO_openarray<char>*)submesh->verts)->Count();
		ntris				+= submesh->indices.Count();
	}

	float3			*fnorms	= new float3[ntris];
	for (int i = 0, foffset = 0; i < nsubs; i++) {
		ISO_ptr<SubMesh> submesh = model->submeshes[i];
		GetFaceNormals(fnorms + foffset, submesh);
		foffset += submesh->indices.Count();
	}

	NormalRecord	*norms	= new NormalRecord[nverts];
	for (int i = 0, foffset = 0, voffset = 0; i < nsubs; i++) {
		ISO_ptr<SubMesh>	submesh		= model->submeshes[i];
		AddNormals(norms + voffset, fnorms + foffset, submesh);
		foffset += submesh->indices.Count();
		voffset += ((ISO_openarray<char>*)submesh->verts)->Count();
	}
	delete[] fnorms;

	for (int i = 0; i < nverts; i++)
		norms[i].Normalise();

	for (int i = 0, offset = 0; i < nsubs; i++) {
		ISO_ptr<SubMesh>	submesh		= model->submeshes[i];
		SetNormals(norms + offset, submesh);
		offset += ((ISO_openarray<char>*)submesh->verts)->Count();
	}

	delete[] norms;
#endif
	return model;
}

//-----------------------------------------------------------------------------
//	strip duplicate verts
//-----------------------------------------------------------------------------

struct BrowserComparer {
	bool operator()(const ISO::Browser2 &a, const ISO::Browser2 &b) const {
		uint32	s = a.GetSize();
		return s && b.GetSize() == s && memcmp(a, b, s) == 0;
	}
};

template<typename T> T xor_together_n(T *p, int n) {
	T	r = 0;
	while (n--)
		r ^= *p++;
	return r;
}

uint32	hash(const ISO::Browser2 &a) { return xor_together_n<uint32>(a, a.GetSize() / 4); }

ISO_ptr<Model3> ReindexModel(ISO_ptr<Model3> model, float weight_threshold, float verts_threshold) {
	for (auto i = model->submeshes.begin(), e = model->submeshes.end(); i != e; ++i) {
		SubMesh			*sm	= *i;
		ISO::Browser2	b1(sm->verts);
		uint16			*faces		= &sm->indices[0][0];
		int				nfaces		= sm->indices.Count();
		int				nverts		= b1.Count();

		uint32	score0	= GetCacheCost(faces, nfaces);

		VertexCacheOptimizerForsyth(faces, nfaces, nverts, faces);
		uint32	score3	= GetCacheCost(faces, nfaces);

		MeshOptimise(faces, nfaces, nverts, MeshCacheParams(32));
		uint32	score1	= GetCacheCost(faces, nfaces);
		uint32	score2	= VertexCacheOptimizerHillclimber(faces, nfaces, nverts, faces, 10);
	#if 0
		Indexer<uint16>		indexer(nverts);
		indexer.ProcessFirst(b1, equal_to());//keygen<ISO::Browser2>());

		WingedEdges	we(make_prim_iterator(Prim2Vert::trianglelist(), (uint16*)sm->indices.begin()), nfaces);
	#endif
	}
	return model;
}

//-----------------------------------------------------------------------------
//	optimise skin
//-----------------------------------------------------------------------------

void memswap(void *p0, void *p1, size_t size) {
	char	*a = (char*)p0, *b = (char*)p1;
	while (size--)
		swap(*a++, *b++);
}

struct mapping {
	uint16	*fwd, *bak;

	mapping(int n) {
		fwd = new uint16[n];
		bak = new uint16[n];
		for (int i = 0; i < n; i++)
			fwd[i] = bak[i] = i;
	}
	~mapping() {
		delete[] fwd;
		delete[] bak;
	}

	uint16	operator[](uint16 i)	const	{ return fwd[i]; }
	uint16	rev(uint16 i)			const	{ return bak[i]; }

	void	swap(uint16 a, uint16 b) {
		uint16	a0 = bak[a], b0 = bak[b];
		fwd[a0] = b;
		fwd[b0] = a;
		bak[a]	= b0;
		bak[b]	= a0;
	}
};

struct weight_sorter {
	float	*w;
	uint16	*b;

	struct weight {
		friend void swap(const weight &a, const weight &b) { swap(a.w, b.w); swap(a.b, b.b); }
		float	&w;
		uint16	&b;
		weight(float *_w, uint16 *_b) : w(*_w), b(*_b) {}
		bool	operator>(const weight &x)	const { return w == x.w ? b < x.b : w > x.w; }
	};

	weight_sorter(float *_w, uint16 *_b) : w(_w), b(_b) {};
	bool	operator==(const weight_sorter &b)	{ return w == b.w;	}
	bool	operator!=(const weight_sorter &b)	{ return w != b.w;	}
	weight			operator*()					{ return weight(w, b);	}
	weight_sorter&	operator++()				{ ++w; ++b; return *this;	}
	weight_sorter&	operator--()				{ --w; --b; return *this;	}

	weight_sorter	begin()	{ return weight_sorter(w + 0, b + 0); }
	weight_sorter	end()	{ return weight_sorter(w + 4, b + 4); }
};

void MakeNewModel(ISO_ptr<Model3> &newmodel, const ISO_ptr<Model3> model, int sub) {
	if (!newmodel) {
		newmodel.Create(model.ID());
		newmodel->minext = model->minext;
		newmodel->maxext = model->maxext;
		for (int i = 0; i < sub; i++)
			newmodel->submeshes.Append(model->submeshes[i]);
	}
}

string ExternalAppend(const char *name, const char *extra) {
	if (name[strlen(name) - 1] == '\'')
		return str(name).slice(0, -1) + extra + "\'";
	else
		return str(name) + extra;
}

ISO_ptr<Model3> OptimiseSkinModel(ISO_ptr<Model3> model, float weight_threshold, float verts_threshold) {
	ISO_ptr<SubMesh>	submesh	= model->submeshes[0];
	if (!submesh->technique.IsExternal())
		return model;

	if (weight_threshold == 0)
		weight_threshold = 0.01f;
	if (verts_threshold == 0)
		verts_threshold = 0.5f;

	ISO_ptr<Model3>	newmodel;
	for (int i = 0, nsubs = model->submeshes.Count(); i < nsubs; i++) {
		ISO_ptr<SubMesh>	submesh1	= model->submeshes[i];

		ISO::Browser2		b1(submesh1->verts);
		ISO::Browser			b2			= b1[0];
		ISO::Browser			bones1		= b2["bones"];
		ISO::Browser			weights1	= b2["weights"];

		if (!bones1 || !weights1 || !weights1.Is<float[4]>()) {
			if (newmodel)
				newmodel->submeshes.Append(submesh1);
			continue;
		}

		uint8	*verts1		= b2;
		uint32	bones		= uint32(bones1 - verts1);
		uint32	weights		= uint32(weights1 - verts1);
		int		vertsize1	= b2.GetSize();
		int		nverts		= b1.Count();
		int		ntris		= submesh1->indices.Count();

		int		nw_tab[5]	= {0,0,0,0,0};
		for (uint8 *v = verts1, *ve = v + vertsize1 * 4; v < ve; v += vertsize1) {
			float	*w	= (float*)(v + weights);
			uint16	*b	= (uint16*)(v + bones);
			selection_sort(weight_sorter(w, b), weight_sorter(w + 4, b + 4), greater());
			int		nw	= 0;
			for (int j = 0; j < 4; j++)
				nw += int(w[j] > weight_threshold);
			nw_tab[nw]++;
		}
		int		max_nw		= 4;
		while (nw_tab[max_nw] == 0)
			max_nw--;

		// are over half the verts single-bone weighted?
		if (nw_tab[1] > nverts * verts_threshold) {
			MakeNewModel(newmodel, model, i);

			ISO_ptr<SubMesh>	submesh2(0);
			submesh2->flags			= submesh1->flags;
			submesh2->parameters	= submesh1->parameters;
			submesh2->verts			= Duplicate(submesh1->verts);
			submesh2->technique.CreateExternal(ExternalAppend(submesh1->technique.External(), "1"));

			verts1 = ISO::Browser(submesh2->verts)[0];

			int					ntris1 = ntris, ntris2 = 0;
			int					nverts1 = nverts - nw_tab[1], nverts2 = nw_tab[1], verts2offset = nverts1;
			mapping				map(nverts);

			// if some verts are not single-bone weighted, must partition the submesh
			if (nverts2 != nverts) {
				submesh1		= Duplicate(submesh1);

				// partition
				int i1 = 0, i2 = nverts;
				while (i1 < i2) {
					uint8	*v1 = verts1 + i1 * vertsize1;
					float	*w	= (float*)(v1 + weights);
					if (w[1] <= weight_threshold) {
						i2--;
						memswap(v1, verts1 + i2 * vertsize1, vertsize1);
						map.swap(i1, i2);
					} else {
						i1++;
					}
				}
				for (int i = 0; i < ntris; i++) {
					SubMesh::face	&f = submesh1->indices[i];
					bool	all = true, none = true;
					for (int j = 0; j < 3; j++) {
						if (map[f[j]] < verts2offset)
							all = false;
						else
							none = false;
					}
					if (!all && !none) {
						for (int j = 0; j < 3; j++) {
							i2	= map[f[j]];
							if (i2 >= i1) {
								memswap(verts1 + i1 * vertsize1, verts1 + i2 * vertsize1, vertsize1);
								map.swap(i1, i2);
								i1++;
							}
						}
					}
				}

				nverts1	= i1;

				i1 = 0;
				i2 = ntris;
				while (i1 < i2) {
					SubMesh::face	&f1 = submesh1->indices[i1];
					bool			move	= true;
					for (int j = 0; j < 3; j++) {
						f1[j] = map[f1[j]];
						if (f1[j] < verts2offset)
							move = false;
					}
					if (move) {
						i2--;
						SubMesh::face	&f2 = submesh1->indices[i2];
						uint16			t;
						t = f1[0]; f1[0] = f2[0]; f2[0] = t - verts2offset;
						t = f1[1]; f1[1] = f2[1]; f2[1] = t - verts2offset;
						t = f1[2]; f1[2] = f2[2]; f2[2] = t - verts2offset;
					} else {
						i1++;
					}
				}
				ntris1	= i1;
				ntris2	= ntris - ntris1;

				memcpy(submesh2->indices.Create(ntris2), &submesh1->indices[i1], ntris2 * sizeof(SubMesh::face));
				submesh1->indices.Resize(ntris1);
			} else {
				submesh2->indices = submesh1->indices.Dup();
				submesh1.Clear();
			}

			// make new vert type
			ISO::TypeComposite		*verttype1	= (ISO::TypeComposite*)b2.GetTypeDef();
			ISO::TypeCompositeN<64>	builder(0);
			for (int j = 0, n = verttype1->Count(); j < n; j++) {
				if (!(verttype1->GetID(j) == "bones" || verttype1->GetID(j) == "weights"))
					builder.Add((*verttype1)[j]);
			}
			uint32	bone	= builder.Add<uint32>("bones");
			ISO::TypeComposite	*verttype2	= builder.Duplicate();
			uint32				vertsize2	= verttype2->GetSize();

			// convert verts
			ISO_openarray<void>	verts2(vertsize2, nverts2);
			uint8				*v1 = verts1 + verts2offset * vertsize1, *v2 = verts2;
			ISO_conversion::batch_convert(
				v1, vertsize1, verttype1,
				v2, vertsize2, verttype2,
				nverts2, false, 0
			);
			for (int i = 0; i < nverts2; i++, v1 += vertsize1, v2 += vertsize2)
				*(uint32*)(v2 + bone) = ((uint16*)(v1 + bones))[0];

			if (submesh1) {
				submesh1->verts = submesh2->verts;
				ISO::Browser(submesh1->verts).Resize(nverts1);
				submesh1->UpdateExtents();

				verts1	= ISO::Browser(submesh1->verts)[0];
				nverts	= nverts1;
				ntris	= ntris1;
			}

			submesh2->verts = MakePtr(new ISO::TypeOpenArray(verttype2, vertsize2));
			*(ISO_openarray<void>*)submesh2->verts = verts2;
			submesh2->UpdateExtents();
			newmodel->submeshes.Append(submesh2);
		}

		// check for maximum of 2 weights
		if (max_nw == 2) {
			MakeNewModel(newmodel, model, i);

			// make new vert type
			ISO::TypeComposite		*verttype1	= (ISO::TypeComposite*)b2.GetTypeDef();
			ISO::TypeCompositeN<64>	builder(0);
			for (int j = 0, n = verttype1->Count(); j < n; j++) {
				if (!(verttype1->GetID(j) == "bones" || verttype1->GetID(j) == "weights"))
					builder.Add((*verttype1)[j]);
			}
			uint32	bone	= builder.Add<uint16[2]>("bones");
			uint32	lerp	= builder.Add<float>("weights");
			ISO::TypeComposite	*verttype2	= builder.Duplicate();
			uint32				vertsize2	= verttype2->GetSize();

			// convert verts
			ISO_openarray<void>	verts2(vertsize2, nverts);
			uint8				*v1 = verts1, *v2 = verts2;
			ISO_conversion::batch_convert(
				v1, vertsize1, verttype1,
				v2, vertsize2, verttype2,
				nverts, false, 0
			);
			for (int i = 0; i < nverts; i++, v1 += vertsize1, v2 += vertsize2) {
				*(array_vec<uint16,2>*)(v2 + bone) = *(array_vec<uint16,2>*)(v1 + bones);
				*(float*)(v2 + lerp) = ((float*)(v1 + weights))[1];
			}

			submesh1->verts = MakePtr(new ISO::TypeOpenArray(verttype2, vertsize2));
			*(ISO_openarray<void>*)submesh1->verts = verts2;
			submesh1->technique.CreateExternal(ExternalAppend(submesh1->technique.External(), "2"));
		}

		if (newmodel && submesh1)
			newmodel->submeshes.Append(submesh1);
	}
	if (newmodel) {
		CheckHasExternals(newmodel, ISO::DUPF_DEEP);
		return newmodel;
	}
	return model;
}

const ISO::Type *GetBaseType(const ISO::Type *type) {
	type	= type->SkipUser();
	while (type && (type->GetType() == ISO::ARRAY || type->GetType() == ISO::OPENARRAY))
		type = type->SubType()->SkipUser();
	return type;
}

void OptimiseSkinRecurse(ISO::Browser2 b, float weight_threshold, float verts_threshold) {
	for (int i = 0, n = b.Count(); i < n; i++) {
		ISO::Browser2	b2		= b[i];
		if (b2.GetType() == ISO::REFERENCE && (*b2).Is<Model3>()) {
			b2.Set(OptimiseSkinModel(ISO_conversion::convert<Model3>(*b2, ISO_conversion::RECURSE | ISO_conversion::ALLOW_EXTERNALS), weight_threshold, verts_threshold));
			continue;
		}
		switch (TypeType(GetBaseType(b2.GetTypeDef()))) {
			case ISO::REFERENCE:
			case ISO::COMPOSITE:
			case ISO::VIRTUAL:
				OptimiseSkinRecurse(b2, weight_threshold, verts_threshold);
				break;
		}
	}
}

ISO_ptr<void> OptimiseSkin(ISO_ptr<void> p, float weight_threshold, float verts_threshold) {
	if (p.IsExternal())
		p = FileHandler::ExpandExternals(p);

	OptimiseSkinRecurse(p, weight_threshold, verts_threshold);
	return p;
}

//-----------------------------------------------------------------------------
//	mesh creators
//-----------------------------------------------------------------------------

#include "systems/mesh/shape_gen.h"

struct ShapeMesh : ISO_ptr<SubMesh> {
	struct vertex {
		float3p position;
		float3p normal;
		void set(param(float3) p, param(float3) n) { position = p; normal = n; }
		operator position3() const { return position3(position); }
	};

	ShapeMesh(tag id) : ISO_ptr<SubMesh>(id) {
		get()->technique = ISO::root("data")["simple"]["lite"];
	}
	~ShapeMesh() {
		get()->UpdateExtents();
	}

	vertex	*BeginVerts(uint32 n) {
		auto	verts	= ISO::MakePtr<ISO_openarray<vertex> > (0, n);
		get()->verts	= verts;
		return verts->begin();
	}

	uint16	*BeginPrims(uint32 n) {
		return &get()->indices.Create(n)[0][0];
	}
};

ISO_DEFUSERCOMPV(ShapeMesh::vertex, position, normal);


ISO_ptr<SubMesh> SphereMesh(int n, float r) {
	if (n == 0)
		n = 16;
	if (r == 0)
		r = 1;
	ShapeMesh	mesh(0);
	SphereVerts(mesh, n, r);
	return move(mesh);
}
ISO_ptr<SubMesh> TorusMesh(int n, int m, float r_outer, float r_inner) {
	if (n == 0)
		n = 16;
	if (m == 0)
		m = n;
	if (r_outer == 0)
		r_outer = r_inner == 0 ? 1 : r_inner * 2;
	if (r_inner == 0)
		r_inner = r_outer / 2;
	ShapeMesh	mesh(0);
	TorusVerts(mesh, n, m, r_outer, r_inner);
	return move(mesh);
}
ISO_ptr<SubMesh> CylinderMesh(int n, float r, float h) {
	if (n == 0)
		n = 16;
	if (r == 0)
		r = 1;
	if (h == 0)
		h = 1;
	ShapeMesh	mesh(0);
	CylinderVerts(mesh, n, r, h);
	return move(mesh);
}
ISO_ptr<SubMesh> ConeMesh(int n, float r, float h) {
	if (n == 0)
		n = 16;
	if (r == 0)
		r = 1;
	if (h == 0)
		h = 1;
	ShapeMesh	mesh(0);
	ConeVerts(mesh, n, r, h);
	return move(mesh);
}
ISO_ptr<SubMesh> BoxMesh(float3p dims) {
	if (dims.x == 0)
		dims.x = 1;
	if (dims.y == 0)
		dims.y = dims.x;
	if (dims.z == 0)
		dims.z = dims.y;
	ShapeMesh	mesh(0);
	BoxVerts(mesh, float3(dims));
	return move(mesh);
}

ISO_ptr<SubMesh> TransformMesh(ISO_ptr<SubMesh> sm, float3x4p mat) {
	TransformVertices(sm->verts, 0, sm->NumVerts(), mat);
	return sm;
}
/*
ISO_ptr<Model3> TransformMesh(ISO_ptr<Model3> model, float3x4p mat) {
	for (auto &sm0 : model->submeshes) {
		SubMesh	*sm = (SubMesh*)sm0;
		TransformVertices(sm->verts, 0, sm->NumVerts(), mat);
	}
	return model;
}
*/


//-----------------------------------------------------------------------------
//	CSG2
//-----------------------------------------------------------------------------

struct AABBTree3 {
	struct Node {
		range<uint32*>	indices;
		cuboid			extent;
		Node			*child[2];

		Node(range<uint32*> indices, const position3 *centroids) : indices(indices), extent(get_extent(make_indexed_container(centroids, range<const uint32*>(indices)))) {
			child[0] = child[1] = nullptr;
		}
	};

	dynamic_array<uint32>	all_indices;
	Node	*root;

	AABBTree3(range<const array<uint16, 3>*> indices, stride_iterator<float3p> verts) : all_indices(int_range(indices.size32())) {
		dynamic_array<position3>	centroids = transformc(indices, [&verts](const uint16 *i) {
			return (float3(verts[i[0]]) + float3(verts[i[1]]) + float3(verts[i[2]])) / 3;
		});

		root	= new Node(all_indices, centroids);

		Node	*stack[32], **sp = stack;
		for (Node *n = root; n;) {
			if (n->indices.size() > 100) {
				auto	indices	= exchange(n->indices, none);
				int		axis 	= max_component_index(n->extent.extent());
				auto	xfaces	= make_indexed_container(centroids, indices);
				auto	m 		= median(xfaces, [axis](const position3 &a, const position3 &b) { return a.v[axis] < b.v[axis]; });

				n->child[0]		= new Node(make_range(indices.begin(), m.inner()), centroids);
				n->child[1]		= new Node(make_range(m.inner(), indices.end()), centroids);

				*sp++	= n->child[1];
				n = n->child[0];

			} else if (sp > stack) {
				n = *--sp;

			} else {
				n = nullptr;
			}
		}
	}

	~AABBTree3() {
		Node	*stack[32], **sp = stack;
		for (Node *n = root; n;) {
			if (n->child[0] || n->child[1]) {
				*sp++ = n->child[1];
				delete exchange(n, n->child[0]);
			} else {
				delete n;
				n = sp > stack ? *--sp : nullptr;
			}
		}
	}
};

bool collect(const AABBTree3 &a, const AABBTree3 &b, callback_ref<bool(const AABBTree3::Node*, const AABBTree3::Node*)> cb) {
	pair<const AABBTree3::Node*, const AABBTree3::Node*>	stack[32], *sp = stack;

	for (const AABBTree3::Node *na = a.root, *nb = b.root;;) {
		bool	treea = na->indices.empty();
		bool	treeb = nb->indices.empty();

		if (!treea && !treeb) {
			// both indices
			if (!cb(na, nb))
				return true;

			if (sp == stack)
				break;
			--sp;
			na = sp->a;
			nb = sp->b;

		} else if (treeb) {
			// go down b
			bool	can0 = overlap(na->extent, nb->child[0]->extent);
			bool	can1 = overlap(na->extent, nb->child[1]->extent);
			if (can0 && can1)
				*sp++ = make_pair(na, nb->child[1]);

			nb = nb->child[can0 ? 0 : 1];

		} else if (treea) {
			// go down a
			bool	can0 = overlap(nb->extent, na->child[0]->extent);
			bool	can1 = overlap(nb->extent, na->child[1]->extent);
			if (can0 && can1)
				*sp++ = make_pair(na->child[1], nb);
			na = na->child[can0 ? 0 : 1];

		} else {

			if ((sp - stack) & 1) {
				// go down b
				bool	can0 = overlap(na->extent, nb->child[0]->extent);
				bool	can1 = overlap(na->extent, nb->child[1]->extent);
				if (can0 && can1)
					*sp++ = make_pair(na, nb->child[1]);

				nb = nb->child[can0 ? 0 : 1];

			} else {
				// go down a
				bool	can0 = overlap(nb->extent, na->child[0]->extent);
				bool	can1 = overlap(nb->extent, na->child[1]->extent);
				if (can0 && can1)
					*sp++ = make_pair(na->child[1], nb);
				na = na->child[can0 ? 0 : 1];
			}

		}
	}
	return false;

}

bool collect(const AABBTree3 &tree, param(position3) pos, callback_ref<bool(const AABBTree3::Node*, param(position3))> cb) {
	const AABBTree3::Node*	stack[32], **sp = stack;

	for (const AABBTree3::Node *n = tree.root;;) {
		if (n->indices.empty()) {
			bool	can0 = n->child[0]->extent.contains(pos);
			bool	can1 = n->child[1]->extent.contains(pos);
			if (can0 && can1)
				*sp++ = n->child[1];

			n = n->child[can0 ? 0 : 1];

		} else {
			if (!cb(n, pos))
				return true;

			if (sp == stack)
				break;
			n = *--sp;
		}
	}
	return false;
}

bool contains(const AABBTree3 &tree, array<uint16, 3> *i, stride_iterator<float3p> v, param(position3) pos) {
	float	winding = 0;

	collect(tree, pos, [&winding, v, i](const AABBTree3::Node* node, param(position3) pos) {
		for (auto x : node->indices) {
			auto	&f = i[x];
			auto	tri	= triangle3(position3(v[f[0]]), position3(v[f[1]]), position3(v[f[2]]));
//			auto	tri	= triangle3(v[f[0]], v[f[1]], v[f[2]]);
			winding += tri.solid_angle(pos);
		}
		return true;
	});

	winding /= pi * 4;
	return winding > 0.9f;
}

auto get_intersections(const AABBTree3 &a, const AABBTree3 &b, array<uint16, 3> *ia, stride_iterator<float3p> va, array<uint16, 3> *ib, stride_iterator<float3p> vb) {
	dynamic_array<pair<uint32,uint32>>	intersections;
	collect(a, b, [va, vb, ia, ib, &intersections](const AABBTree3::Node *nodea, const AABBTree3::Node *nodeb) {
		for (auto xa : nodea->indices) {
			auto		&fa		= ia[xa];
			auto		tria	= triangle3(position3(va[fa[0]]), position3(va[fa[1]]), position3(va[fa[2]]));
			for (auto xb : nodeb->indices) {
				auto		&fb = ib[xb];
				auto		trib = triangle3(position3(vb[fb[0]]), position3(vb[fb[1]]), position3(vb[fb[2]]));
				if (intersect(tria, trib))
					intersections.emplace_back(xa, xb);
			}
		}
		return true;
	});
	return intersections;
}

typedef pair<int,int>	edge_id;
struct edge {
	edge_id	next;
	edge() : next(0,0) {}
};

auto cut_mesh(const AABBTree3 &btree, const pair<uint32,uint32> *i, const pair<uint32,uint32> *e, uint32 na, array<uint16, 3> *ia, stride_iterator<float3p> va, array<uint16, 3> *ib, stride_iterator<float3p> vb) {
	dynamic_array<triangle3>	cut_tris;

	for (int xa = 0; xa < na; ++xa) {
		auto		&fa = ia[xa];
		if (i != e && i->a == xa) {
			// split up triangle
	#if 0
			static const position2 uncut[] = {{0, 0}, {1, 0}, {0, 1}};

			dynamic_array<dynamic_array<position2>>	splits;
			splits.emplace_back(uncut);

			dynamic_array<position2>	front;
			dynamic_array<position2>	back;
			auto		ma	= tri.matrix();

			while (i != e && i->a == xa) {
				int			xb	= i->b;
				auto		&fb = ib[xb];
				plane		planeb(vb[fb[0]], vb[fb[1]], vb[fb[2]]);

				dynamic_array<dynamic_array<float2>>	more_splits;

				for (auto &split : splits) {
					convex_polygon	poly2 = convex_polygon(split);
					front.resize(split.size32() + 1);
					back.resize(split.size32() + 1);
					auto	n		= poly2.split(front, back, (planeb / ma) & z_axis);
					if (n.a >= 3 && n.b >= 3) {
						split = range<position2*>(front, front + n.a);
						more_splits.emplace_back(range<position2*>(back, back + n.b));
					}
				}
				splits.append(more_splits);
				++i;
			}
			for (auto &split : splits) {
				for (auto a = split.begin(), b = split.end() - 1; a + 1 != b; ++a, --b) {
					if (!contains(btree, ib, vb, (a[0] + a[1] + b[0]) / 3)) {
						cut_tris.emplace_back(a[0], a[1], b[0]);
					}

					if (a + 2 == b)
						break;

					if (!contains(btree, ib, vb, (b[0] + a[1] + b[-1]) / 3)) {
						cut_tris.emplace_back(b[0], a[1], b[-1]);
					}
				}
			}
#elif 0
			dynamic_array<dynamic_array<position3>>	splits;
			auto	&uncut = splits.push_back();
			uncut.push_back(va[fa[0]]);
			uncut.push_back(va[fa[1]]);
			uncut.push_back(va[fa[2]]);

			dynamic_array<position3>	front;
			dynamic_array<position3>	back;

			while (i != e && i->a == xa) {
				int			xb	= i->b;
				auto		&fb = ib[xb];
				plane		planeb(vb[fb[0]], vb[fb[1]], vb[fb[2]]);

				dynamic_array<dynamic_array<float3>>	more_splits;

				for (auto &split : splits) {
					auto	poly2	= convex_polygon<range<position3*>>(split);
					front.resize(split.size32() + 1);
					back.resize(split.size32() + 1);
					auto	n		= poly2.split(front, back, planeb);
					if (n.a >= 3 && n.b >= 3) {
						split = range<position3*>(front, front + n.a);
						more_splits.emplace_back(range<position3*>(back, back + n.b));
					}
				}
				splits.append(more_splits);
				++i;
			}
			for (auto &split : splits) {
				for (auto a = split.begin(), b = split.end() - 1; a + 1 != b; ++a, --b) {
					if (!contains(btree, ib, vb, (a[0] + a[1] + b[0]) / 3)) {
						cut_tris.emplace_back(a[0], a[1], b[0]);
					}

					if (a + 2 == b)
						break;

					if (!contains(btree, ib, vb, (b[0] + a[1] + b[-1]) / 3)) {
						cut_tris.emplace_back(b[0], a[1], b[-1]);
					}
				}
			}
#else
			if (i + 1 == e || i[1].a != xa) {
				position3	uncut[3] = {position3(va[fa[0]]), position3(va[fa[1]]), position3(va[fa[2]])};
				position3	cut[4];

				int		xb		= i->b;
				auto	&fb		= ib[xb];
				auto	planeb	= plane(position3(vb[fb[0]]), position3(vb[fb[1]]), position3(vb[fb[2]]));

				auto	poly2	= convex_polygon<range<position3*>>(uncut);
				auto	n		= poly2.clip(cut, planeb);

				for (auto a = cut, b = cut + n - 1; a + 1 != b; ++a, --b) {
					cut_tris.emplace_back(a[0], a[1], b[0]);
					if (a + 2 == b)
						break;
					cut_tris.emplace_back(b[0], a[1], b[-1]);
				}
				++i;

			} else {
				auto	tria	= triangle3(position3(va[fa[0]]), position3(va[fa[1]]), position3(va[fa[2]]));
				auto	planea	= tria.plane();

				// collect intersecting edges
				hash_map_with_key<edge_id, edge>	edges;
				while (i != e && i->a == xa) {
					int			xb	= i->b;
					auto		&fb = ib[xb];
					int			i0	= fb[0],  i1 = fb[1],  i2 = fb[2];
					position3	v0	= position3(vb[i0]), v1 = position3(vb[i1]), v2 = position3(vb[i2]);
					float		d0	= planea.dist(v0), d1 = planea.dist(v1), d2 = planea.dist(v2);
					edge_id	x0, x1;
					switch ((d0 < 0) + (d1 < 0) * 2 + (d2 < 0) * 4) {
						case 1: x0 = edge_id(i0, i1); x1 = edge_id(i0, i2); break;
						case 2: x0 = edge_id(i1, i2); x1 = edge_id(i1, i0); break;
						case 3: x0 = edge_id(i1, i2); x1 = edge_id(i0, i2); break;
						case 4: x0 = edge_id(i2, i0); x1 = edge_id(i2, i1); break;
						case 5: x0 = edge_id(i0, i1); x1 = edge_id(i2, i1); break;
						case 6: x0 = edge_id(i2, i0); x1 = edge_id(i1, i0); break;
						default: ISO_ASSERT(0);
					}
					// x0 = 1/5: i0, i1		x1=	2/6: i1, i0
					//		2/3: i1, i2			4/5: i2, i1
					//		4/6: i2, i0			1/3: i0, i2
					auto	&edge0 = edges[x0].put();
					ISO_ASSERT(edge0.next == edge_id(0, 0));
					edge0.next = x1;
					++i;
				}

				// connect intersecting edges into paths
				dynamic_array<dynamic_array<edge_id>>	paths;
				while (!edges.empty()) {
					dynamic_array<edge_id>	path;
					auto p = edges.begin()->a, p0 = p;
					for (;;) {
						path.push_back(p);
						auto	p2 = edges[p];
						if (!p2.exists() || (p = p2->next) == p0)
							break;
						edges.remove(&p2);
					}
					bool	add = true;
					for (auto &i : paths) {
						if (i.front() == p) {
							i = path.append(slice(i, 1));
							add = false;
							break;
						}
					}
					if (add)
						paths.push_back(path);
				}

				float3x4		ma	= inverse(tria.matrix());

				for (auto &path : paths) {
					dynamic_array<position2> poly = transformc(path, [vb, ma](edge_id i) {
						position3	v0	= ma * position3(vb[i.a]), v1 = ma * position3(vb[i.b]);
						return lerp(v0, v1, v0.v.z / (v0.v.z - v1.v.z)).v.xy;
					});

					for (auto &e : path)
						ISO_TRACEF() << e.a << ", " << e.b << '\n';
					ISO_TRACEF() << '\n';
				}
			}
#endif

		} else {
			if (!contains(btree, ib, vb, position3((va[fa[0]] + va[fa[1]] + va[fa[2]]) / 3))) {
				cut_tris.emplace_back(position3(va[fa[0]]), position3(va[fa[1]]), position3(va[fa[2]]));
			}
		}

	}
	return cut_tris;
}

struct interleaved_pos {
	uint64	u;
	static uint64	spread(uint32 i)	{ return part_bits<1, 2, 21>(i >> 2); }
	interleaved_pos(param(position3) p) {
		iorf	*i = (iorf*)&p;
		u = (spread(i[0].m) << 0) | (spread(i[1].m) << 1) | (spread(i[2].m) << 2);
	}
};

csg	MeshToCSG(const SubMesh *sm) {
	stride_iterator<float3p>	v = sm->VertComponentData<float3p>(0);
	dynamic_array<csgnode::polygon> poly;

	for (auto &i : sm->indices)
		new (poly) csgnode::polygon(intptr_t(i.begin()), position3(v[i[0]]), position3(v[i[1]]), position3(v[i[2]]));

	return move(poly);
}

void CSGToMesh(SubMesh *sm, const SubMesh *sma, const SubMesh *smb, const csg &csg) {
	auto	polyc	= csg.polys();

	int		ntris	= 0, nverts = 0;
	cuboid	ext(none);

	for (auto &i : polyc) {
		ntris	+= i.verts.size32() - 2;
		nverts	+= i.verts.size32();
		ext		|= position3(i.matrix.w);
		ext		|= position3(i.matrix.w) + i.matrix.x;
		ext		|= position3(i.matrix.w) + i.matrix.y;
	}

	auto	ext_mat	= translate(position3(float3(3))) * ext.inv_matrix();
	hash_map<interleaved_pos, uint32>	vhash;

	sm->NumFaces(ntris);
	sm->NumVerts(nverts);

	const ISO::Type			*vt		= sm->VertType();
	auto					vc		= sm->VertComponentData<char>(0), vc0 = vc;
	const ISO::Element		*norms	= GetVertexComponentElement(sm->verts, "normal");
	auto					*ic		= sm->indices.begin();

	dynamic_array<uint32>	indices;
	for (auto &i : polyc) {
		indices.clear();

		uint16			*face	= (uint16*)i.face_id;
		const SubMesh	*psm	= face >= (uint16*)sma->indices.begin() && face < (uint16*)sma->indices.end() ? sma : smb;

		stride_iterator<char>	vp(psm->VertData(), psm->VertSize());
		const void				*v0 = &vp[face[0]];
		const void				*v1 = &vp[face[1]];
		const void				*v2 = &vp[face[2]];

		for (auto &v : i.verts) {
			position3	pos 	= i.matrix * position3(v, zero);
			auto		posi	= vhash[ext_mat * pos];
			if (!posi.exists()) {
				posi = vc - vc0;
				if (i.flipped) {
					Interpolate(vt, &*vc, v0, v2, v1, v);
					if (norms)
						negate(*(float3p*)(&*vc + norms->offset));
				} else {
					Interpolate(vt, &*vc, v0, v1, v2, v);
				}
				++vc;
			}
			indices.push_back(posi);
		}

		ic = convex_to_tris(ic, indices);
	}
	sm->NumVerts(uint32(vc - vc0));
	sm->UpdateExtents();
}

ISO_ptr<SubMesh> UnionMesh(ISO_ptr<SubMesh> a, ISO_ptr<SubMesh> b) {
	ISO_ptr<SubMesh>	sm	= MakeSubMeshLike(a);
	CSGToMesh(sm, a, b, MeshToCSG(a) | MeshToCSG(b));
	return sm;
}

ISO_ptr<SubMesh> IntersectMesh(ISO_ptr<SubMesh> a, ISO_ptr<SubMesh> b) {
	ISO_ptr<SubMesh>	sm	= MakeSubMeshLike(a);
	CSGToMesh(sm, a, b, MeshToCSG(a) & MeshToCSG(b));
	return sm;
}

ISO_ptr<SubMesh> SubtractMesh(ISO_ptr<SubMesh> a, ISO_ptr<SubMesh> b) {
	ISO_ptr<SubMesh>	sm	= MakeSubMeshLike(a);
#if 1

	auto		va = a->VertComponentData<float3p>(0);
	auto		vb = b->VertComponentData<float3p>(0);
	array<uint16, 3> *ia = a->indices;
	array<uint16, 3> *ib = b->indices;

	AABBTree3	atree(a->indices, va);
	AABBTree3	btree(b->indices, vb);

	auto		intersections = get_intersections(atree, btree, ia, va, ib, vb);
	sort(intersections);
	auto		cut_tris = cut_mesh(btree, intersections.begin(), intersections.end(), a->indices.size32(), ia, va, ib, vb);

#if 0
	auto		intersections2 = get_intersections(btree, atree, ib, vb, ia, va);
	sort(intersections2);
	auto		cut_tris2 = cut_mesh(atree, intersections2.begin(), intersections2.end(), b->indices.size32(), ib, vb, ia, va);
	cut_tris.append(cut_tris2);
#endif

	// make mesh
	int		ntris	= cut_tris.size32(), nverts = nverts = ntris * 3;
	cuboid	ext		= atree.root->extent;
	auto	ext_mat	= translate(position3(float3(3))) * ext.inv_matrix();
	hash_map<interleaved_pos, uint32>	vhash;

	sm->NumFaces(ntris);
	sm->NumVerts(nverts);

	auto	vc		= sm->VertComponentData<float3p>(0), vc0 = vc;
	auto	*ic		= sm->indices.begin();

	dynamic_array<uint32>	indices;
	for (auto &i : cut_tris) {
		indices.clear();

		for (int j = 0; j < 3; j++) {
			position3	pos 	= i.corner(j);
			auto		posi	= vhash[ext_mat * pos];
			if (!posi.exists()) {
				*vc	= pos.v;
				posi = vc - vc0;
				++vc;
			}
			indices.push_back(posi);
		}

		ic = convex_to_tris(ic, indices);
	}
	sm->NumVerts(uint32(vc - vc0));
	sm->UpdateExtents();

#else

	CSGToMesh(sm, a, b, MeshToCSG(a) - MeshToCSG(b));
#endif
	return sm;
}

#include "bitmap/bitmap.h"

struct bitmap_vertex {
	float3p		pos;
	float3p		col;
};
ISO_DEFUSERCOMPV(bitmap_vertex, pos, col);

ISO_ptr<SubMesh> BitmapToMesh(ISO_ptr<bitmap> bm, int grid) {
	if (grid == 0)
		grid = 1;

	int	bw	= bm->Width(), bh = bm->Height();
	int	gw	= bw / grid, gh = bh / grid;

	ISO_ptr<SubMesh>	m(bm.ID());
	m->technique = ISO::root("data")["default"]["col_vc"];

	int		nv		= (gw + 1) * (gh + 1), nt = gw * gh * 2;
	auto	verts	= ISO::MakePtr<ISO_openarray<bitmap_vertex> >(0, nv);
	m->verts		= verts;

	auto	p = verts->begin();
	for (int y = 0; y <= gh; y++) {
		auto	c	= bm->ScanLine(y * grid);
		for (int x = 0; x <= gw; x++) {
			float	fx = float(x), fy = float(y);
			p->pos = float3{fx, fy, sqrt(2 - square(fx * 2 / gw - 1) - square(fy * 2 / gh - 1)) * gw / 10};
			p++->col = float4((HDRpixel)*c++).xyz;
		}
	}


	m->indices.Create(nt);
	auto	i = m->indices.begin();
	for (int y = 0; y < gh; y++) {
		for (int x = 0; x < gw; x++) {
			int	j = y * (gw + 1) + x;
			i[0][0] = j;
			i[0][1] = j + 1;
			i[0][2] = j + gw + 1;

			i[1][0] = j + gw + 1;
			i[1][1] = j + 1;
			i[1][2] = j + gw + 2;

			i += 2;
		}
	}

	m->UpdateExtents();
	return m;
}

#include "maths/comp_geom.h"

//-----------------------------------------------------------------------------
//	SimplifyMesh0
//-----------------------------------------------------------------------------

struct Mesh {
	struct Tri;

	struct Edge : cg::tri_edge<Tri, Edge> {
		int		vert;
		bool	dirty;
	};

	struct Tri : cg::tri_face<Tri, Edge> {
		int		index;
		bool	active;
		Tri(int index) : index(index), active(true) {}
	};

	struct interleaved_pair {
		uint64	u;
		interleaved_pair(int a, int b) : u(interleave(lo_hi(a, b))) {}
	};

	dynamic_array<Tri>					tris;
	hash_map<interleaved_pair, Edge*>	edges;

	Edge*	find_edge(int v0, int v1) {
		return edges[interleaved_pair(v0, v1)].or_default();
	}

	void	add_edge(Edge &e, int v0, int v1) {
		e.vert	= v0;
		e.set_flip(find_edge(v1, v0));
		edges[interleaved_pair(v0, v1)] = &e;
	}

	void	collapse_edge(Edge *e);

	Mesh(range<SubMesh::face*> faces) {
		tris.reserve(faces.size());

		for (auto &v : faces) {
			Tri		&tri	= tris.push_back(faces.index_of(v));
			int		v0		= v[0], v1 = v[1], v2 = v[2];

			if (find_edge(v0, v1) || find_edge(v1, v2) || find_edge(v2, v0)) {
				tri[0].vert = v0;
				tri[1].vert = v1;
				tri[2].vert = v2;

			} else {
				add_edge(tri[0], v0, v1);
				add_edge(tri[1], v1, v2);
				add_edge(tri[2], v2, v0);
			}
		}

	}
};

void Mesh::collapse_edge(Edge* e0) {
	ISO_ASSERT(e0->tri->active);
	Edge	*en = e0->next, *e1 = e0->flip;
	int		vi0 = e0->vert, vi1 = en->vert;

	// remove tris
	e0->tri->active = false;
	if (e1)
		e1->tri->active = false;

	// patch all of v1's edges to point at v0
	for (auto &edge : en->vertex_edges()) {
		ISO_ASSERT(edge.vert == vi1);
		edge.vert	= vi0;
		edge.dirty	= true;
		if (edge.flip)
			edge.flip->dirty = true;
	}

	// fix next edge at v1
	if (Edge *et = en->flip)
		et->set_flip(en->next->flip);

	// fix next edge at v0
	if (e1) {
		Edge	*ep = e1->next;
		if (Edge *et = ep->flip)
			et->set_flip(ep->next->flip);
	}

}

struct sort_on_b {
	template<typename A, typename B> bool operator()(const pair<A,B> &a, const pair<A,B> &b) { return a.b < b.b; }
};

ISO_ptr<SubMesh> RemoveEdges(ISO_ptr<SubMesh> sm, float min_len) {
	cuboid	ext		= cuboid((position3)sm->minext, (position3)sm->maxext);
	auto	ext_mat	= translate(position3(float3(3))) * ext.inv_matrix();

	if (min_len == 0)
		min_len = reduce_max(max(ext.b, -ext.a).v) / 1000.f;

	// collapse identical (within precision) vertices (create vertex mapping)
	uint32	index	= 0;
	hash_map<interleaved_pos, uint32>	vhash;
	dynamic_array<uint16>			vmap = transformc(sm->VertComponentRange<float3p>(0), [&vhash, &index, ext_mat](const float3p &pos) {
		uint32	i	= index++;
		auto	p	= vhash[ext_mat * position3(pos)];
		if (p.exists())
			return (uint32)p;
		p = i;
		return i;
	});

	// create faces with mapped vertices
	dynamic_array<SubMesh::face>	faces = transformc(sm->indices, [&vmap](const SubMesh::face &face) {
		SubMesh::face	face2;
		face2[0] = vmap[face[0]];
		face2[1] = vmap[face[1]];
		face2[2] = vmap[face[2]];
		return face2;
	});

	// create connected mesh
	Mesh	mesh(faces);

	// calculate edge lengths
	auto	verts	= sm->VertComponentData<float3p>(0);
	dynamic_array<pair<Mesh::Edge*, float>> edges = transformc(mesh.edges, [verts](Mesh::Edge *e) {
		int		v0	= e->vert, v1 = e->next->vert;
		e->dirty = false;
		return make_pair(e, len(position3(verts[v1]) - position3(verts[v0])));
	});

	sort(edges, sort_on_b());

	// collapse edges
	for (auto ei = edges.begin(); ei->b < min_len; ++ei) {
		auto	e = ei->a;
		if (e->tri->active) {
			if (e->dirty) {
				int		v0	= e->vert, v1 = e->next->vert;
				float	b	= len(position3(verts[v1]) - position3(verts[v0]));
//				ei->b		= b;
//				e->dirty	= false;
				if (b >= min_len)
					continue;
			}
			mesh.collapse_edge(ei->a);
		}
	}

	// find remaining verts
	dynamic_bitarray<uint32>	remain(vmap.size32());
	int		num_tris = 0;
	for (auto &t : mesh.tris) {
		if (t.active) {
			remain.set(t[0].vert);
			remain.set(t[1].vert);
			remain.set(t[2].vert);
			++num_tris;
		}
	}

	// recreate submesh
	ISO_ptr<SubMesh> sm2 = MakeSubMeshLike(sm);
	sm2->NumVerts(remain.count_set());
	sm2->NumFaces(num_tris);

	auto	verts2	= sm2->VertComponentData<float3p>(0);
	uint32	vs		= sm2->VertSize();
	int		nv2		= 0;

	for (auto &i : vmap) {
		int	i0 = vmap.index_of(i);
		if (remain[i0]) {
			memcpy(&verts2[nv2], &verts[i0], vs);
			i = nv2++;
		}
	}

	auto	faces2	= sm2->indices.begin();
	for (auto &t : mesh.tris) {
		if (t.active) {
			(*faces2)[0] = vmap[t[0].vert];
			(*faces2)[1] = vmap[t[1].vert];
			(*faces2)[2] = vmap[t[2].vert];
			++faces2;
		}
	}
	sm2->UpdateExtents();
	return sm2;
}


//-----------------------------------------------------------------------------
//	SimplifyMesh
//-----------------------------------------------------------------------------

struct Simplify {
	dynamic_array<float3p>	pos;
	dynamic_array<float3p>	norms;
	dynamic_array<float3p>	cols;

	struct MeshVertex;
	struct MeshFace;

	struct Edge : cg::edge_mixin<Edge> {
		using cg::edge_mixin<Edge>::edge_mixin;
		MeshVertex*	v;		// origin vertex
		MeshFace*	f;		// left f
		//float4		posQ;

		MeshVertex	*v0()	const	{ return v; }
		MeshVertex	*v1()	const	{ return flip()->v; }
		MeshFace	*f0()	const	{ return f; }
		MeshFace	*f1()	const	{ return flip()->f; }
	};

	struct MeshVertex : cg::vertex_mixin<MeshVertex, Edge> {
		int					index;
		quadric_params<3>	q;
		MeshVertex(int i) : index(i) {}
	};


	struct MeshFace : cg::face_mixin<MeshFace, Edge> {
		plane				p;
		quadric_params<3>	q;
	};

	dynamic_array<MeshVertex>	verts;
	dynamic_array<MeshFace>		faces;
	dynamic_array<Edge::pair_t>	edges;

	Simplify(const range<stride_iterator<float3p>> &pos, int ntris) : pos(pos), verts(int_range(pos.size32())) {
		faces.reserve(ntris);
		edges.reserve(pos.size() + ntris);		//E = V + F - 2
	}

	float4	get_volume_grad(const MeshFace &f) const {
		position3	p[8];
		int			x = 0;
		for (auto &e : f.edges())
			p[x++] = position3(pos[e.v->index]);

		return concat(f.p.normal() * f.q.a, dot(p[0], cross(p[1], p[2])) / 2);
	}

	float4	get_volume_grad(const Edge &e) const {
		float4	vol(zero);
		for (auto &i : e.vertex_edges()) {
			if (i.f)
				vol += get_volume_grad(*i.f);
		}

		int	notfirst = 0;
		for (auto &i : e.flip()->vertex_edges()) {
			if (notfirst++ && i.f)
				vol += get_volume_grad(*i.f);
		}
		return vol / 3;
	}

	float	get_error(const Edge &e) const {
		quadric_params<3>	Q	= e.v0()->q + e.v1()->q;
		quadric				G	= Q.get_grad_quadric();
		quadric				Q0	= (quadric)Q - G;

		float4		posQ0	= Q0.centre4();			// pos only
		float4		posQ1	= Q.centre4();			// pos and attributes

		float4				V	= get_volume_grad(e);

		float3x3			A0	= swizzle<0,1,2>(Q0);
		float4				B0	= concat(Q0.column(3).xyz, V.w);
		float4x4			Avol0(
			concat(A0.x, V.x),
			concat(A0.y, V.y),
			concat(A0.z, V.z),
			concat(V.xyz, zero)
		);
		float4		posQ2	= B0 / Avol0;	// pos and vol

		float3x3			A	= swizzle<0,1,2>((const quadric&)Q);
		float4				B	= concat(Q.column(3).xyz, V.w);
		float4x4			Avol(
			concat(A.x, V.x),
			concat(A.y, V.y),
			concat(A.z, V.z),
			concat(V.xyz, zero)
		);
		float4		posQ	= B / Avol;		// pos, attrib, vol

		//e.posQ	= posQ;
		//e.posQ	= Q.centre4();

		float		valsQ[3];
		for (int i = 0; i < 3; i++)
			valsQ[i] = Q.get_param(i, posQ);

		return Q.evaluate(posQ, valsQ);
	}

	const Edge	*find_edge(const MeshVertex &v0, const MeshVertex &v1)	{
		for (auto &i : v0.edges()) {
			if (i.v == &v1)
				return &i;
		}
		return nullptr;
	}

	Edge	*make_edge(MeshVertex &v0, MeshVertex &v1) {
		auto &p	= edges.push_back();
		p.h0.v	= &v1;
		p.h1.v	= &v0;
		if (!v0.e)
			v0.e = &p.h0;
		if (!v1.e)
			v1.e = &p.h1;
		return &p.h0;
	}

	MeshFace *AddTri(int i0, int i1, int i2) {
		MeshFace	&f	= faces.push_back();
		Edge		*e0	= unconst(find_edge(verts[i0], verts[i1]));
		Edge		*e1	= unconst(find_edge(verts[i1], verts[i2]));
		Edge		*e2 = unconst(find_edge(verts[i2], verts[i0]));

		int	first = e1 ? 1 : e2 ? 2 : 0;
		if (!e0)
			e0 = make_edge(verts[i0], verts[i1]);
		if (!e1)
			e1 = make_edge(verts[i1], verts[i2]);
		if (!e2)
			e2 = make_edge(verts[i2], verts[i0]);

		e0->f = e1->f = e2->f = &f;

		switch (first) {
			case 0:
				splice(e0, e1);
				splice(e1, e2);
				splice(e2, e0);
				break;
			case 1:
				splice(e1, e2);
				splice(e2, e0);
				splice(e0, e1);
				break;
			case 2:
				splice(e2, e0);
				splice(e0, e1);
				splice(e1, e2);
				break;
		}

		return &f;
	}

	void	InitQuadrics();
};
/*
// Calculate the ideal vert location of the collapsed edge based on the quadric: (C - (1/a)*B*B^T) * p_min = b_1 - (1/a)*B*b_2
real3x3 C = real3x3
( real3( q.A[ 0 ], q.A[ 1 ], q.A[ 2 ] )
	, real3( q.A[ 1 ], q.A[ 3 ], q.A[ 4 ] )
	, real3( q.A[ 2 ], q.A[ 4 ], q.A[ 5 ] ) );

real3x3 BB( 0.0 );
for ( uint32 i = 0; i < 3; i++ )
	for ( uint32 j = 0; j < 3; j++ )
	{
		for ( uint32 k = 0; k < 6; k++ )
		{
			BB[ i ][ j ] += -q.g[ k ][ i ] * -q.g[ k ][ j ]; // This matrix is symmetric, so there should be room for optimization
		}
	}
real3 Bb( 0.0 );
for ( uint32 i = 0; i < 3; i++ )
{
	for ( uint32 k = 0; k < 6; k++ )
	{
		Bb[ i ] += -q.g[ k ][ i ] * q.b[ 3 + k ];
	}
}

real3x3 A = C - BB * (real( 1.0 ) / q.a);
real3 b = real3( q.b[ 0 ], q.b[ 1 ], q.b[ 2 ] ) - Bb / q.a;

real4x4 AVol = real4x4( real4( A[ 0 ], gVol[ 0 ] ), real4( A[ 1 ], gVol[ 1 ] ), real4( A[ 2 ], gVol[ 2 ] ), real4( gVol, 0.0 ) );
real4 bVol = real4( -b, dVol );
real3 posQ = (inverse( AVol ) * bVol).getXYZ();
*/
void Simplify::InitQuadrics() {
	int	i = 0;
	for (auto &v : verts) {
		for (auto &e : v.edges())
			++i;
	}


	for (auto &f : faces) {
		position3	p[8];
		float3		n[8];
		float3		c[8];
		int			x = 0;
		for (auto &e : f.edges()) {
			int	i = e.v->index;
			p[x] = position3(pos[i]);
			//n[x] = norms[i];
			c[x] = float3(cols[i]);
			++x;
		}

		f.p = plane(p[0], p[1], p[2]);
		auto	G = f.q.add_triangle(p[0], p[1], p[2]);

		//auto	gN	= G * mat<float,4,3>(transpose(float3x3(n[0], n[1], n[2])), float3(zero));
		//f.q.add_gradient(0, gN.x);	// gradient of x component of normal
		//f.q.add_gradient(1, gN.y);	// gradient of y component of normal
		//f.q.add_gradient(2, gN.z);	// gradient of z component of normal

		auto	gC	= G * mat<float,4,3>(transpose(float3x3(c[0], c[1], c[2])), float3(zero));
		f.q.add_gradient(0, gC.x);	// gradient of x component of colour
		f.q.add_gradient(1, gC.y);	// gradient of y component of colour
		f.q.add_gradient(2, gC.z);	// gradient of z component of colour
	}

	for (auto &v : verts) {
		for (auto &e : v.edges()) {
			if (e.f)
				v.q += e.f->q;
		}
	}
}

ISO_ptr<SubMesh> SimplifyMesh(ISO_ptr<SubMesh> sm, float factor) {
	Simplify	simp(sm->VertComponentRange<float3p>(0), sm->NumFaces());

	for (auto e : sm->VertComponents()) {
		switch (USAGE2(e.id).usage) {
			case USAGE_COLOR:
				ISO::Conversion::batch_convert(
					sm->_VertComponentData(e.offset), e.type.get(),
					simp.cols.resize(simp.verts.size())
				);
		}
	}

	for (auto &f : sm->indices)
		simp.AddTri(f[0], f[1], f[2]);

	simp.InitQuadrics();

	dynamic_array<pair<Simplify::Edge*, float>>	errors;
	for (auto &e : simp.edges) {
		errors.emplace_back(&e.h0, simp.get_error(e.h0));
	}
	sort(errors, [](const pair<Simplify::Edge*, float> &a, const pair<Simplify::Edge*, float> &b) { return a.b < b.b; });

	return sm;
}

//-----------------------------------------------------------------------------
//	uv remapping
//-----------------------------------------------------------------------------

ISO_ptr<Model3> ToMercator(ISO_ptr<Model3> m) {

	ISO_ptr<Model3> m2(0);
	for (auto i : m->submeshes) {
		SubMesh* sm = i;
		ISO_ptr<SubMesh>	sm2(0);
		sm2->verts		= Duplicate(sm->verts);
		sm2->indices	= Duplicate(sm->indices);
		sm2->parameters	= Duplicate(sm->parameters);
		sm2->technique	= sm->technique;

		m2->submeshes.Append(sm2);

		auto	tex			= ISO::Conversion::convert<bitmap>(ISO::Browser(sm->parameters)["map_Kd"].GetPtr(), ISO::ConversionFlags::RECURSE|ISO::ConversionFlags::EXPAND_EXTERNALS);
		uint32	width		= tex->Width();
		uint32	height		= tex->Height();
		float2	tex_scale	= { (float)width, (float)height };
		float2	tex2_scale	= { (float)width, (float)height };
		float2	uv_scale = { one, one };// / 1.7f	};
		ISO_ptr<bitmap>		tex2("map_Kd", width, height);

		fill(tex2->All(), 0);
#if 0
		for (int y = 0; y < height; y++) {
			auto	scanline = tex2->ScanLine(y);
			for (int x = 0; x < width; x++)
				scanline[x] = { x * 255 / width, y * 255 / height, 0 };
		}
#endif
		
		int			nverts2	= 0;
		auto		verts	= sm->VertDataIterator();
		auto		verts2	= sm2->VertDataIterator();
		field_access<float3p>	pos		= sm2->VertComponent("position")->offset;
		field_access<float3p>	norm	= sm2->VertComponent("normal")->offset;
		field_access<float2p>	uv		= sm2->VertComponent("texcoord0")->offset;
		
		auto		uv2		= verts2 + uv;
		uint32		checks	= 0;

		cuboid			box(position3(sm->minext), position3(sm->maxext));
		float3			centre = box.centre();
		dynamic_octree	oct(box - centre);

		auto	find = [&checks](void* i, param(position3) v, float& d) {
			++checks;
			auto	d1 = dist(*(position3*)i, v);
			if (d1 <= d) {
				d = d1;
				return true;
			}
			return false;
		};

		auto	add_vert = [&](void *vert)->int {
			auto	p	= position3(pos[vert]) - centre;
			if (auto i = (float3*)oct.find(p, find, 1e-6)) {
				if (approx_equal(*i, p))
					return i - verts2;
			}
			void*	vert2 = (void*)(verts2 + nverts2);
			pos[vert2]	= p;
			norm[vert2] = norm[vert];
			uv[vert2]	= (float2{ atan2(p.v.xz) / pi, -normalise(p.v).y } + 1)* half * uv_scale;
			oct.add(p, vert2);
			//ISO_ASSERT(oct.find(p, find, 1e-6) == vert2);
			return nverts2++;
		};

		temp_array<int>	map = transformc(sm->VertDataRange(), add_vert);

		auto	face2 = sm2->indices.begin();
		for (auto& face : sm->indices) {
			void	*v0 = (void*)(verts + face[0]);
			void	*v1 = (void*)(verts + face[1]);
			void	*v2 = (void*)(verts + face[2]);

			int		i0 = (*face2)[0] = map[face[0]];
			int		i1 = (*face2)[1] = map[face[1]];
			int		i2 = (*face2)[2] = map[face[2]];

			RasteriseTriangle(
				tex2->All(),	{ position2(uv2[i0] * tex2_scale), position2(uv2[i1] * tex2_scale), position2(uv2[i2] * tex2_scale) },
				tex->All(),		{ position2(uv[v0] * tex_scale), position2(uv[v1] * tex_scale), position2(uv[v2] * tex_scale) }
			);

			++face2;
		}

		sm2->NumVerts(nverts2);
		sm2->UpdateExtents();

		ISO::Browser(sm2->parameters).SetMember("map_Kd", tex2);
	}

	m2->UpdateExtents();
	return m2;
}

//-----------------------------------------------------------------------------
//	stitch
//-----------------------------------------------------------------------------


struct Stitcher {
	struct MeshVertex;
	struct MeshFace;

	struct Edge : cg::edge_mixin<Edge> {
		using cg::edge_mixin<Edge>::edge_mixin;
		MeshVertex*	v;		// origin vertex
		MeshFace*	f;		// left f

		MeshVertex	*v0()	const	{ return v; }
		MeshVertex	*v1()	const	{ return flip()->v; }
		MeshFace	*f0()	const	{ return f; }
		MeshFace	*f1()	const	{ return flip()->f; }
	};

	struct MeshVertex : cg::vertex_mixin<MeshVertex, Edge> {
		int		index;
		MeshVertex(int i) : index(i) {}
	};

	struct MeshFace : cg::face_mixin<MeshFace, Edge> {
	};

	dynamic_array<MeshVertex>	verts;
	dynamic_array<MeshFace>		faces;
	dynamic_array<Edge::pair_t>	edges;

	Stitcher(int nverts, int ntris) : verts(int_range(nverts)) {
		faces.reserve(ntris);
		edges.reserve(ntris * 3);
	}

	static const Edge *find_edge(const MeshVertex &v0, const MeshVertex &v1) {
		for (auto &i : v0.edges()) {
			if (i.v == &v1)
				return &i;
		}
		return nullptr;
	}

	Edge	*make_edge(MeshVertex &v0, MeshVertex &v1) {
		auto &p	= edges.push_back();
		p.h0.v	= &v1;
		p.h1.v	= &v0;
		if (!v0.e)
			v0.e = &p.h0;
		if (!v1.e)
			v1.e = &p.h1;
		return &p.h0;
	}

	void	join_edges(Edge* a, Edge* b) {
		Edge* bv = b->vnext;
		Edge* ax = a->flip();
		a->fnext = b;
		b->vnext = ax;
		if (ax->vnext == ax)
			ax->vnext = bv;
	}

	MeshFace *AddTri(int i0, int i1, int i2) {
		MeshFace	&f	= faces.push_back();
		Edge		*e0	= unconst(find_edge(verts[i0], verts[i1]));
		Edge		*e1	= unconst(find_edge(verts[i1], verts[i2]));
		Edge		*e2 = unconst(find_edge(verts[i2], verts[i0]));

		if (int needed = int(!e0) + int(!e1) + int(!e2)) {
			int	first = e1 ? 1 : e2 ? 2 : 0;
			if (!e0)
				e0 = make_edge(verts[i0], verts[i1]);
			if (!e1)
				e1 = make_edge(verts[i1], verts[i2]);
			if (!e2)
				e2 = make_edge(verts[i2], verts[i0]);

			//if (first != 0) {
			//	swap(e0, first == 1 ? e1 : e2);
			//	swap(e1, e2);
			//}
		}
		
		ISO_ASSERT(!e0->f && !e1->f && !e2->f);
		e0->f = e1->f = e2->f = &f;

		join_edges(e0, e1);
		join_edges(e1, e2);
		join_edges(e2, e0);
		//splice(e0->flip(), e1);
		//splice(e1->flip(), e2);
		//splice(e2->flip(), e0);

		for (auto& i : edges) {
			ISO_ASSERT(i.h0.validate());
			ISO_ASSERT(i.h1.validate());
		}
		return &f;
	}
};


ISO_ptr<Model3> StitchMesh(ISO_ptr<Model3> m) {
	ISO_ptr<Model3> m2(0);
	int	bad_edges = 0;
	int	total_edges = 0;

	for (auto i : m->submeshes) {
		SubMesh* sm = i;

		Stitcher	simp(sm->NumVerts(), sm->NumFaces());
		//simp.AddTri(0, 1, 2);
		//simp.AddTri(0, 3, 1);


		for (auto &f : sm->indices)
			simp.AddTri(f[0], f[1], f[2]);

		total_edges += simp.edges.size32();
		dynamic_bitarray<uint32>	disconnected(simp.edges.size32());
		auto	bi = disconnected.begin();
		for (auto& e2 : simp.edges)
			*bi++ = !e2.h1.f;

		for (auto i : disconnected.where(true)) {
			auto	e = &simp.edges[i].h1;
			auto	e0 = e;
			int		n = 0;
			while (!e->f) {
				++n;
				e = e->f0_next();
				if (e == e0)
					break;
			}
		}

		for (auto& e2 : simp.edges) {
			if (!e2.h1.f) {
				++bad_edges;
				//ISO_TRACEF("stitch2: ") << e2.h1.v->index << " to " << e2.h0.v->index << "\n";
			}
		}

	}
	return m;
}

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO::getdef<SubMesh>(),

	ISO_get_cast(DefaultSceneModel),
	ISO_get_cast(DefaultSceneNode),
	ISO_get_cast(DefaultNodeModel),
	ISO_get_cast(DefaultTechnique),
//	ISO_get_cast(DefaultPass),
	ISO_get_cast(ModelFromSubMesh),

	ISO_get_operation(MergeModels),
	ISO_get_operation(OptimiseSkinModel),
	ISO_get_operation(ReindexModel),
	ISO_get_operation_external(OptimiseSkin),
	ISO_get_operation(SphereMesh),
	ISO_get_operation(TorusMesh),
	ISO_get_operation(CylinderMesh),
	ISO_get_operation(ConeMesh),
	ISO_get_operation(BoxMesh),
	ISO_get_operation(TransformMesh),
	ISO_get_operation(UnionMesh),
	ISO_get_operation(IntersectMesh),
	ISO_get_operation(SubtractMesh),
	ISO_get_operation(BitmapToMesh),
	ISO_get_operation(SimplifyMesh),
	ISO_get_operation(RemoveEdges),
	ISO_get_operation(ToMercator),
	ISO_get_operation(StitchMesh)
);
