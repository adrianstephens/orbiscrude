#include "bone.h"
#include "maths/geometry.h"
#include "base/algorithm.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "scenegraph.h"
#include "systems/mesh/model_iso.h"

using namespace iso;

BONE iso::GetParent(BONE a) {
	static uint8 parents[] = {
		BONE_ROOT,
		BONE_ROOT,					//BONE_HIPS,

		BONE_HIPS,					//BONE_SPINE1,
		BONE_SPINE1,				//BONE_SPINE2,
		BONE_SPINE2,				//BONE_SPINE3,
		BONE_SPINE3,				//BONE_NECK1,
		BONE_NECK1,					//BONE_NECK2,
		BONE_NECK2,					//BONE_HEAD,

		BONE_SPINE2,				//BONE_LEFT_CLAVICLE,
		BONE_LEFT_CLAVICLE,			//BONE_LEFT_SHOULDER,
		BONE_LEFT_SHOULDER,			//BONE_LEFT_ARM,
		BONE_LEFT_ARM,				//BONE_LEFT_FOREARM,
		BONE_LEFT_FOREARM,			//BONE_LEFT_HAND,
		BONE_LEFT_HAND,				//BONE_LEFT_THUMB1,
		BONE_LEFT_THUMB1,			//BONE_LEFT_THUMB2,
		BONE_LEFT_THUMB2,			//BONE_LEFT_THUMB3,
		BONE_LEFT_HAND,				//BONE_LEFT_INDEX1,
		BONE_LEFT_INDEX1,			//BONE_LEFT_INDEX2,
		BONE_LEFT_INDEX2,			//BONE_LEFT_INDEX3,
		BONE_LEFT_HAND,				//BONE_LEFT_MIDDLE1,
		BONE_LEFT_MIDDLE1,			//BONE_LEFT_MIDDLE2,
		BONE_LEFT_MIDDLE2,			//BONE_LEFT_MIDDLE3,
		BONE_LEFT_HAND,				//BONE_LEFT_RING1,
		BONE_LEFT_RING1,			//BONE_LEFT_RING2,
		BONE_LEFT_RING2,			//BONE_LEFT_RING3,
		BONE_LEFT_HAND,				//BONE_LEFT_PINKY1,
		BONE_LEFT_PINKY1,			//BONE_LEFT_PINKY2,
		BONE_LEFT_PINKY2,			//BONE_LEFT_PINKY3,

		BONE_SPINE2,				//BONE_RIGHT_CLAVICLE,
		BONE_RIGHT_CLAVICLE,		//BONE_RIGHT_SHOULDER,
		BONE_RIGHT_SHOULDER,		//BONE_RIGHT_ARM,
		BONE_RIGHT_ARM,				//BONE_RIGHT_FOREARM,
		BONE_RIGHT_FOREARM,			//BONE_RIGHT_HAND,
		BONE_RIGHT_HAND,			//BONE_RIGHT_THUMB1,
		BONE_RIGHT_THUMB1,			//BONE_RIGHT_THUMB2,
		BONE_RIGHT_THUMB2,			//BONE_RIGHT_THUMB3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_INDEX1,
		BONE_RIGHT_INDEX1,			//BONE_RIGHT_INDEX2,
		BONE_RIGHT_INDEX2,			//BONE_RIGHT_INDEX3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_MIDDLE1,
		BONE_RIGHT_MIDDLE1,			//BONE_RIGHT_MIDDLE2,
		BONE_RIGHT_MIDDLE2,			//BONE_RIGHT_MIDDLE3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_RING1,
		BONE_RIGHT_RING1,			//BONE_RIGHT_RING2,
		BONE_RIGHT_RING2,			//BONE_RIGHT_RING3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_PINKY1,
		BONE_RIGHT_PINKY1,			//BONE_RIGHT_PINKY2,
		BONE_RIGHT_PINKY2,			//BONE_RIGHT_PINKY3,

		BONE_HIPS,					//BONE_LEFT_THIGH,
		BONE_LEFT_THIGH,			//BONE_LEFT_LEG,
		BONE_LEFT_LEG,				//BONE_LEFT_FOOT,
		BONE_LEFT_FOOT,				//BONE_LEFT_TOES,

		BONE_HIPS,					//BONE_RIGHT_THIGH,
		BONE_RIGHT_THIGH,			//BONE_RIGHT_LEG,
		BONE_RIGHT_LEG,				//BONE_RIGHT_FOOT,
		BONE_RIGHT_FOOT,			//BONE_RIGHT_TOES,
	};
	return (BONE)parents[a];
}

const char *iso::GetName(BONE a) {
	static const char *names[] = {
		"root",			//BONE_ROOT,
		"hips",			//BONE_HIPS,
		"spine1",		//BONE_SPINE1,
		"spine2",		//BONE_SPINE2,
		"spine3",		//BONE_SPINE3,
		"neck1",		//BONE_NECK1,
		"neck2",		//BONE_NECK2,
		"head",			//BONE_HEAD,

		"Lclavicle",	//BONE_LEFT_CLAVICLE,
		"Lshoulder",	//BONE_LEFT_SHOULDER,
		"Larm",			//BONE_LEFT_ARM,
		"Lforearm",		//BONE_LEFT_FOREARM,
		"Lhand",		//BONE_LEFT_HAND,
		"Lthumb1",		//BONE_LEFT_THUMB1,
		"Lthumb2",		//BONE_LEFT_THUMB2,
		"Lthumb3",		//BONE_LEFT_THUMB3,
		"Lindex1",		//BONE_LEFT_INDEX1,
		"Lindex2",		//BONE_LEFT_INDEX2,
		"Lindex3",		//BONE_LEFT_INDEX3,
		"Lmiddle1",		//BONE_LEFT_MIDDLE1,
		"Lmiddle2",		//BONE_LEFT_MIDDLE2,
		"Lmiddle3",		//BONE_LEFT_MIDDLE3,
		"Lring1",		//BONE_LEFT_RING1,
		"Lring2",		//BONE_LEFT_RING2,
		"Lring3",		//BONE_LEFT_RING3,
		"Lpinky1",		//BONE_LEFT_PINKY1,
		"Lpinky2",		//BONE_LEFT_PINKY2,
		"Lpinky3",		//BONE_LEFT_PINKY3,

		"Rclavicle",	//BONE_RIGHT_CLAVICLE,
		"Rshoulder",	//BONE_RIGHT_SHOULDER,
		"Rarm",			//BONE_RIGHT_ARM,
		"Rforearm",		//BONE_RIGHT_FOREARM,
		"Rhand",		//BONE_RIGHT_HAND,
		"Rthumb1",		//BONE_RIGHT_THUMB1,
		"Rthumb2",		//BONE_RIGHT_THUMB2,
		"Rthumb3",		//BONE_RIGHT_THUMB3,
		"Rindex1",		//BONE_RIGHT_INDEX1,
		"Rindex2",		//BONE_RIGHT_INDEX2,
		"Rindex3",		//BONE_RIGHT_INDEX3,
		"Rmiddle1",		//BONE_RIGHT_MIDDLE1,
		"Rmiddle2",		//BONE_RIGHT_MIDDLE2,
		"Rmiddle3",		//BONE_RIGHT_MIDDLE3,
		"Rring1",		//BONE_RIGHT_RING1,
		"Rring2",		//BONE_RIGHT_RING2,
		"Rring3",		//BONE_RIGHT_RING3,
		"Rpinky1",		//BONE_RIGHT_PINKY1,
		"Rpinky2",		//BONE_RIGHT_PINKY2,
		"Rpinky3",		//BONE_RIGHT_PINKY3,

		"Lthigh",		//BONE_LEFT_THIGH,
		"Lleg",			//BONE_LEFT_LEG,
		"Lfoot",		//BONE_LEFT_FOOT,
		"Ltoes",		//BONE_LEFT_TOES,

		"Rthigh",		//BONE_RIGHT_THIGH,
		"Rleg",			//BONE_RIGHT_LEG,
		"Rfoot",		//BONE_RIGHT_FOOT,
		"Rtoes",		//BONE_RIGHT_TOES,
	};
	if (a & BONE_ROLL) {
		static char name[16];
		sprintf(name, "%s_roll", names[a & 0xff]);
		return name;
	}
	return names[a];
}

float4 iso::GetRotCone(BONE a) {
	static const float4 cones[] = {			// cones, dir.xyz,cos(angle)
		{	0,	0,	1,	0},				//BONE_ROOT,
		{ 0.0f,  0.0f,  1.0f, 0.95f},	//BONE_HIPS,
		{ 1.0f,  0.0f,  0.0f, 0.64f},	//BONE_SPINE1,
		{	0,	0,	1,	0},				//BONE_SPINE2,
		{	0,	0,	1,	0},				//BONE_SPINE3,
		{	0,	0,	1,	0},				//BONE_NECK1,
		{	0,	0,	1,	0},				//BONE_NECK2,
		{ 1.0f,  0.7f,  0.0f, 0.76f},	//BONE_HEAD,

		{	0,	0,	1,	0},				//BONE_LEFT_CLAVICLE,
		{ 1.0f,  0.0f,  0.0f, 0.17f},	//BONE_LEFT_SHOULDER,
		{	0,	0,	1,	0},				//BONE_LEFT_ARM,
		{ 0.0f,  0.0f, -1.0f, 0.00f},	//BONE_LEFT_FOREARM,
		{ 1.0f,  0.0f,  0.0f, 0.76f},	//BONE_LEFT_HAND,
		{	0,	0,	1,	0},				//BONE_LEFT_THUMB1,
		{	0,	0,	1,	0},				//BONE_LEFT_THUMB2,
		{	0,	0,	1,	0},				//BONE_LEFT_THUMB3,
		{	0,	0,	1,	0},				//BONE_LEFT_INDEX1,
		{	0,	0,	1,	0},				//BONE_LEFT_INDEX2,
		{	0,	0,	1,	0},				//BONE_LEFT_INDEX3,
		{	0,	0,	1,	0},				//BONE_LEFT_MIDDLE1,
		{	0,	0,	1,	0},				//BONE_LEFT_MIDDLE2,
		{	0,	0,	1,	0},				//BONE_LEFT_MIDDLE3,
		{	0,	0,	1,	0},				//BONE_LEFT_RING1,
		{	0,	0,	1,	0},				//BONE_LEFT_RING2,
		{	0,	0,	1,	0},				//BONE_LEFT_RING3,
		{	0,	0,	1,	0},				//BONE_LEFT_PINKY1,
		{	0,	0,	1,	0},				//BONE_LEFT_PINKY2,
		{	0,	0,	1,	0},				//BONE_LEFT_PINKY3,

		{	0,	0,	1,	0},				//BONE_RIGHT_CLAVICLE,
		{ 1.0f,  0.0f,  0.0f, 0.17f},	//BONE_RIGHT_SHOULDER,
		{	0,	0,	1,	0},				//BONE_RIGHT_ARM,
		{ 0.0f,  0.0f,  1.0f, 0.00f},	//BONE_RIGHT_FOREARM,
		{ 1.0f,  0.0f,  0.0f, 0.76f},	//BONE_RIGHT_HAND,
		{	0,	0,	1,	0},				//BONE_RIGHT_THUMB1,
		{	0,	0,	1,	0},				//BONE_RIGHT_THUMB2,
		{	0,	0,	1,	0},				//BONE_RIGHT_THUMB3,
		{	0,	0,	1,	0},				//BONE_RIGHT_INDEX1,
		{	0,	0,	1,	0},				//BONE_RIGHT_INDEX2,
		{	0,	0,	1,	0},				//BONE_RIGHT_INDEX3,
		{	0,	0,	1,	0},				//BONE_RIGHT_MIDDLE1,
		{	0,	0,	1,	0},				//BONE_RIGHT_MIDDLE2,
		{	0,	0,	1,	0},				//BONE_RIGHT_MIDDLE3,
		{	0,	0,	1,	0},				//BONE_RIGHT_RING1,
		{	0,	0,	1,	0},				//BONE_RIGHT_RING2,
		{	0,	0,	1,	0},				//BONE_RIGHT_RING3,
		{	0,	0,	1,	0},				//BONE_RIGHT_PINKY1,
		{	0,	0,	1,	0},				//BONE_RIGHT_PINKY2,
		{	0,	0,	1,	0},				//BONE_RIGHT_PINKY3,

		{ 1.0f,  0.0f,  0.0f, 0.34f},	//BONE_LEFT_THIGH,
		{ 0.5f, -1.0f,  0.0f, 0.50f},	//BONE_LEFT_LEG,
		{ 1.0f,  1.0f,  0.0f, 0.50f},	//BONE_LEFT_FOOT,
		{	0,	0,	1,	0},				//BONE_LEFT_TOES,

		{ 1.0f,  0.0f,  0.0f, 0.34f},	//BONE_RIGHT_THIGH,
		{ 0.5f, -1.0f,  0.0f, 0.50f},	//BONE_RIGHT_LEG,
		{ 1.0f,  1.0f,  0.0f, 0.50f},	//BONE_RIGHT_FOOT,
		{	0,	0,	1,	0},				//BONE_RIGHT_TOES,
	};
	return cones[a];
}

BONE iso::LookupBone(const BoneMapping *m, size_t num, const char *name) {
	for (int i = 0; i < num; i++) {
		if (m[i].name == str(name))
			return m[i].bone;
	}
	return BONE_ROOT;
}

//-----------------------------------------------------------------------------
//	Bone optimisation
//-----------------------------------------------------------------------------

typedef pair<int,float>	MatrixPaletteWeight;
typedef ISO_openarray<MatrixPaletteWeight>  MatrixPaletteEntry;
struct MatrixPalette		: ISO_openarray<MatrixPaletteEntry> {};
ISO_DEFUSER(MatrixPalette, ISO_openarray<MatrixPaletteEntry>);

template<typename A, typename B, class P> bool sort_on_a(const pair<A,B> &a, const pair<A,B> &b) { return P()(a.a, b.a); }
template<typename A, typename B, class P> bool sort_on_b(const pair<A,B> &a, const pair<A,B> &b) { return P()(a.b, b.b); }

struct VQ_bone {
	float	weight;
	int		n;
	float	w[4];
	uint16	i[4];

	void	Set(float *_w, uint16 *_i, int _n) {
		n		= _n;
		weight	= 1;

		for (int j = 0; j < n; j++) {
			w[j] = _w[j];
			i[j] = _i[j];
		}

		for (int j = 0; j < n - 1; j++) {
			for (int k = j + 1; k < n; k++) {
				if (w[j] < w[k]) {
					swap(w[j], w[k]);
					swap(i[j], i[k]);
				}
			}
		}
		while (n && w[n - 1] == 0)
			n--;
	}

	void	Quantise(float q) {
		for (int j = 0; j < n; j++)
			w[j] = floor(w[j] * q + 0.5f) / q;
		while (n && w[n - 1] == 0)
			n--;
	}
};

struct VQ_bone2 {
	dynamic_array<bone_weight>	vec;
	bool						sorted;

	VQ_bone2()			{}
	VQ_bone2(const VQ_bone2 &b2) : vec(b2.vec), sorted(b2.sorted) {}
	void operator=(const VQ_bone2 &b2) { vec.assign(b2.vec.begin(), b2.vec.end()); sorted = b2.sorted; }

	void	reset()	{
		vec.reset();
	}

	int		count()	const {
		return int(vec.size());
	}

	void	add(int i, float w) {
		for (size_t j = 0, n = vec.size(); j < n; j++) {
			if (vec[j].a == i) {
				vec[j].b += w;
				return;
			}
		}
		vec.emplace_back(i, w);
		sorted = false;
	}

	void	scale(float f) {
		for (size_t j = 0, n = vec.size(); j < n; j++)
			vec[j].b *= f;
	}

	void	operator=(const VQ_bone &b) {
		vec.resize(b.n);
		for (int j = 0; j < b.n; j++) {
			vec[j].a = b.i[j];
			vec[j].b = b.w[j];
		}
	}

	void	sort() {
		if (!sorted) {
			iso::sort(vec, sort_on_b<int,float,greater>);
			sorted = true;
		}
	}
};

int compare(const VQ_bone &b1, const VQ_bone &b2) {
	int	n = min(b1.n, b2.n);
	for (int j = 0; j < n; j++) {
		if (b1.i[j] < b2.i[j])
			return -1;
		if (b1.i[j] > b2.i[j])
			return +1;
	}

	if (b1.n < b2.n)
		return -1;
	if (b1.n > b2.n)
		return +1;

	for (int j = 0; j < 4; j++) {
		if (b1.w[j] < b2.w[j])
			return -1;
		if (b1.w[j] > b2.w[j])
			return +1;
	}
	return 0;
}

bool less1(const VQ_bone &b1, const VQ_bone &b2) {
	return compare(b1, b2) < 0;
}

struct VQBoneOptimiser1 {

	typedef	VQ_bone		element;
	typedef	VQ_bone2	welement;

	size_t				size()						{ return vecs.size();		}
	VQ_bone&			data(size_t i)				{ return vecs[i];			}
	float				weightedsum(VQ_bone2 &s, size_t i);
	static	void		reset(VQ_bone2 &s)			{ return s.reset();			}
	static	void		scale(VQ_bone2 &s, float f)	{ s.scale(f);				}

	float				distsquared(const VQ_bone &a, VQ_bone2 &b, float max = 1e38f);
	float				norm(const VQ_bone &e);
	float				norm(const VQ_bone2 &e);

	ISO_ptr<BasePose>	bp;
	ISO_ptr<Model3>		model;

	int			nb, nr;
	int			*parents, *roots;
	float		*cost;
	float		*extent;
	size_t	*indices;

	dynamic_array<VQ_bone>	vecs;
	bone_weight				*temp;

	float&		Cost(int i, int j)	const	{ return cost[i * nb + j]; }

public:
	ISO_ptr<Model3>			GetModel()	const	{ return model;	}
	ISO_ptr<MatrixPalette>	MakePalette(int num);
	VQBoneOptimiser1(ISO_ptr<BasePose> _bp, ISO_ptr<Model3> _model, int quant);
	~VQBoneOptimiser1();
};


float VQBoneOptimiser1::weightedsum(VQ_bone2 &s, size_t i) {
	VQ_bone	&e	= vecs[i];
	float	w	= e.weight;
	for (int j = 0; j < e.n; j++)
		s.add(e.i[j], e.w[j] * w);
	return w;
}

float VQBoneOptimiser1::distsquared(const VQ_bone &a, VQ_bone2 &b, float max) {
#if 0
	float	total	= 0;
	for (int k = 0, n = b.count(); total < max && k < n; k++) {
		int		i		= b.vec[k].a;
		float	w		= b.vec[k].b;
		float	cost	= 1e38f;
		for (int j = 0; cost && j < a.n; j++)
			cost = min(cost, Cost(a.i[j], i));
//		total	+= square((cost + extent[i]) * w);
		total	+= square(cost * w);
	}

	return total;
#elif 1
	int		an	= a.n;
	int		bn = b.count();

	for (int i = 0; i < bn; i++)
		temp[i] = b.vec[i];

	float	total	= 0;
	for (int i = 0; i < an; i++) {
		float	aw		= a.w[i];
		int		ai		= a.i[i];
		float	bestc	= 1e38f;
		int		bestj	= -1;

		for (int j = 0; j < bn; j++) {
			if (temp[j].b == 0)
				continue;

			if (temp[j].a == ai) {
				bestj = j;
				bestc = 0;
				break;
			}

			float	c = Cost(temp[j].a, ai);
			if (c < bestc) {
				bestj	= j;
				bestc	= c;
			}
		}

		if (bestj >= 0) {
			int		bi	= temp[bestj].a;
			float	bw	= temp[bestj].b;

			if (ai == bi) {
				if (bw > aw) {
					bw -= aw;
					aw = 0;
				} else {
					aw -= bw;
					bw = 0;
				}
			} else {
#if 0
				float	ac	= Cost(roots[0], ai);
				float	bc	= Cost(roots[0], bi);
				if (bc < ac) {
					float	d	= sqrt(abs(1 - square(bc / ac)));
					aw		-= (1 - d) * bw;
					bw		*= d;
				} else {
					float	d	= sqrt(abs(1 - square(ac / bc)));
					aw		*= d;
					bw		-= (1 - d) * aw;
				}
#else
				float	ac	= Cost(roots[0], ai);
				float	bc	= ac + bestc;
				float	d	= sqrt(abs(1 - square(ac / bc)));
				aw		*= d;
				bw		-= (1 - d) * aw;
				if (bw < 0)
					bw = 0;
//				bw		*= (1 - d);
#endif
			}
			temp[bestj].b = bw;
		}

		total	+= square(aw);
	}

	for (int i = 0; total < max && i < bn; i++)
		total	+= square(temp[i].b);
	return total;
#else
	b.sort();

	int		an	= a.n;
	float	aw[4];
	int		ai[4];
	for (int i = 0; i < an; i++) {
		aw[i] = a.w[i];
		ai[i] = a.i[i];
	}

	float	total	= 0;
	for (int k = 0, n = b.count(); total < max && k < n; k++) {
		int		i		= b.vec[k].a;
		float	w		= b.vec[k].b;
		if (an) {
			float	cost	= Cost(ai[0], i);
			int		minj	= 0;
			for (int j = 1; j < an; j++) {
				float	costj = Cost(ai[j], i);
				if (costj < cost) {
					cost = costj;
					minj = j;
				}
			}
			total	+= square((cost + extent[i]) * (w - aw[minj]));
			--an;
			ai[minj] = ai[an];
			aw[minj] = aw[an];
		} else {
			total	+= square((Cost(roots[0], i) + extent[i]) * w);
		}
	}

	return total;
#endif
}

float VQBoneOptimiser1::norm(const VQ_bone &e) {
	float	norm = 0;
	for (int j = 0; j < e.n; j++)
		norm += square((Cost(roots[0], e.i[j]) + extent[e.i[j]]) * e.w[j]);
	return norm;
}

float VQBoneOptimiser1::norm(const VQ_bone2 &e) {
	float	norm = 0;
	for (int j = 0, n = e.count(); j < n; j++)
		norm += square((Cost(roots[0], e.vec[j].a) + extent[e.vec[j].a]) * e.vec[j].b);
	return norm;
}

VQBoneOptimiser1::VQBoneOptimiser1(ISO_ptr<BasePose> _bp, ISO_ptr<Model3> _model, int quant) : bp(_bp), model(Duplicate(_model)) {
	nb		= bp->Count();
	nr		= 0;

	parents	= new int[nb];
	roots	= new int[nb];
	cost	= new float[nb * nb];
	extent	= new float[nb];
	temp	= new bone_weight[nb];

	for (int i = 0; i < nb * nb; i++)
		cost[i] = -1;

	for (int i = 0; i < nb; i++) {
		if ((parents[i] = bp->GetIndex((*bp)[i]->parent.ID())) < 0)
			roots[nr++] = i;
	}

	for (int i = 0; i < nb; i++) {
		Cost(i, i)	= 0;
		Bone	*b		= (*bp)[i];
		int		pi		= parents[i];
		if (pi >= 0) {
			float	dist = len(float3(b->basepose.w));
			Cost(i, pi) = Cost(pi, i) = dist;
			for (int j = 0; j < nb; j++) {
				if (Cost(i, j) < 0) {
					float c = Cost(pi, j);
					if (c >= 0)
						Cost(i, j) = Cost(j, i) = c + dist;
				}
			}
		}
	}

	aligned<cuboid,16>	*ext = new aligned<cuboid,16>[nb];
	for (int i = 0; i < nb; i++)
		ext[i] = empty;

	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		ISO_ptr<SubMesh>	submesh		= model->submeshes[i];
		ISO::TypeOpenArray	*vertstype	= (ISO::TypeOpenArray*)submesh->verts.GetType();
		ISO::TypeComposite*vertsstruct	= (ISO::TypeComposite*)vertstype->subtype->SkipUser();
		const ISO::Element*	weights		= vertsstruct->Find("weights");
		const ISO::Element*	bones		= vertsstruct->Find("bones");
		if (!weights || !bones)
			continue;

		submesh				= Duplicate(submesh);
		submesh->verts		= Duplicate(submesh->verts);
		model->submeshes[i]	= submesh;

		uint32				vertsize	= vertstype->subsize;
		ISO_openarray<char>	*verts		= submesh->verts;
		int					count		= verts->Count();
		char				*vertsp		= *verts;

		size_t				start		= vecs.size();
		vecs.resize(start + count);
		VQ_bone	*put	= &vecs[start];
		for (int i = 0; i < count; i++, put++) {
			put->Set(
				(float*)(vertsp + i * vertsize + weights->offset),
				(uint16*)(vertsp + i * vertsize + bones->offset),
				4
			);
			put->weight = start + i;	// original index
			float3	v = load<float3>((float*)(vertsp + i * vertsize));
			for (int j = 0; j < put->n && put->w[j] > 0.25f; j++)
				ext[put->i[j]] += v;
		}
	}

	for (int i = 0; i < nb; i++)
		extent[i] = len(ext[i].extent());

	delete[] ext;

	// quantise and collect

	size_t	nv	= vecs.size();
	for (int i = 0; i < nv; i++)
		vecs[i].Quantise(quant);

	sort(vecs, (bool(*)(const VQ_bone&,const VQ_bone&))less1);

	int	nv2 = 0;
	indices = new size_t[nv];
	indices[int(vecs[0].weight)] = 0;
	vecs[0].weight			= 1;

	for (int i = 1; i < nv; i++) {
		int	o = int(vecs[i].weight);
		if (compare(vecs[i], vecs[nv2]) == 0) {
			vecs[nv2].weight++;
		} else {
			++nv2;
			vecs[nv2] = vecs[i];
			vecs[nv2].weight = 1;
		}
		indices[o] = nv2;
	}
	vecs.resize(nv2 + 1);
}

VQBoneOptimiser1::~VQBoneOptimiser1() {
	delete[] parents;
	delete[] roots;
	delete[] cost;
	delete[] extent;
	delete[] indices;
	delete[] temp;
}

ISO_ptr<MatrixPalette> VQBoneOptimiser1::MakePalette(int num) {
	vq<VQBoneOptimiser1>	vqt(num);
	vqt.build(*this);

	int	size = (int)vqt.size();
	ISO_ptr<MatrixPalette>	pal(0);
	pal->Create(size);
	for (int i = 0; i < size; i++) {
		const VQ_bone2		&b = vqt[i];
		MatrixPaletteWeight	*e = (*pal)[i].Create(b.count());
		for (int j = 0, n = b.count(); j < n; j++) {
			e[j].a = b.vec[j].a;
			e[j].b = b.vec[j].b;
		}
	}

	uint32	*index = new uint32[vecs.size()];
	vqt.generate_indices(*this, index);

	int			start	= 0;
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		ISO_ptr<SubMesh>	submesh		= model->submeshes[i];
		ISO::TypeOpenArray	*vertstype	= (ISO::TypeOpenArray*)submesh->verts.GetType();
		ISO::TypeComposite*vertsstruct	= (ISO::TypeComposite*)vertstype->subtype->SkipUser();
		const ISO::Element*	weights		= vertsstruct->Find("weights");
		const ISO::Element*	bones		= vertsstruct->Find("bones");
		if (!weights || !bones)
			continue;

		uint32				vertsize	= vertstype->subsize;
		ISO_openarray<char>	*verts		= submesh->verts;
		int					count		= verts->Count();
		char				*vertsp		= *verts;

		for (int j = 0; j < count; j++) {
			float	*w	= (float*)(vertsp + j * vertsize + weights->offset);
			uint16	*i	= (uint16*)(vertsp + j * vertsize + bones->offset);
			w[0] = 1;
			i[0] = (uint16)index[indices[start + j]];
			w[1] = w[2] = w[3] = 0;
			i[1] = i[2] = i[3] = 0;
		}
		start += count;
	}

	return pal;
}

//-----------------------------------------------------------------------------
//	Bone Opt
//-----------------------------------------------------------------------------

float VQBoneOptimiser::weightedsum(VQbone &s, size_t i) {
	const VQbone	&e	= data(i);
	float			w	= e.weight / s.weight;
	for (int j = 0, n = e.count(); j < n; j++)
		s.add(e.v[j].a, e.v[j].b * w);
	return w;
}

float VQBoneOptimiser::distsquared(const VQbone &b1, VQbone &b2, float max) {
	float	total	= 0;
	float	scale	= 1 / b2.weight;
	for (int i = 0, j, na = b1.count(), nb = b2.count(); total < max && i < nb; i++) {
		for (j = 0; j < na; j++) {
			if (b1.v[j].a == b2.v[j].a)
				break;
		}
		if (j < na)
			total += square(b1.v[j].b * scale - b2.v[i].b);
		else
			total += square(b2.v[i].b);
	}
	return total * square(b2.weight);
}

float VQBoneOptimiser::norm(const VQbone &e) {
	float	norm = 0;
	for (int j = 0, n = e.count(); j < n; j++)
		norm += square(e.v[j].b);
	return norm;
}

void VQBoneOptimiser::Init(int _count) {
	count	= count2 = _count;
	vec		= new VQbone[count];
	order	= new size_t[count];
}

int iso::compare(const VQbone &b1, const VQbone &b2) {
	int	n1 = b1.count(), n2 = b2.count();

	for (int j = 0, n = min(n1, n2); j < n; j++) {
		if (b1.v[j].a < b2.v[j].a)
			return -1;
		if (b1.v[j].a > b2.v[j].a)
			return +1;
	}

	if (n1 < n2)
		return -1;
	if (n1 > n2)
		return +1;

	for (int j = 0; j < n1; j++) {
		if (b1.v[j].b < b2.v[j].b)
			return -1;
		if (b1.v[j].b > b2.v[j].b)
			return +1;
	}
	return 0;
}

static bool less1(const VQbone &b1, const VQbone &b2) { return compare(b1, b2) < 0; }

int VQBoneOptimiser::Quantise(int quant) {
	// quantise and collect
	for (int i = 0; i < count; i++)
		vec[i].quantise(quant);

	sort(vec, vec + count, (bool(*)(const VQbone&,const VQbone&))less1);

	count2	= 0;
	order[int(vec[0].weight)]	= 0;
	vec[0].weight				= 1;

	for (int i = 1; i < count; i++) {
		int	o = int(vec[i].weight);
		if (compare(vec[i], vec[count2]) == 0) {
			vec[count2].weight++;
		} else {
			++count2;
			swap(vec[count2], vec[i]);
			vec[count2].weight = 1;
		}
		order[o] = count2;
	}
	count2++;
	return count2;
}

int VQBoneOptimiser::Build(int num, VQbone *palette, uint32 *indices) {
	vq<VQBoneOptimiser>	vqt(num);
	num = (int)vqt.build(*this);
	vqt.generate_indices(*this, indices);

	for (int i = 0; i < num; i++)
		swap(palette[i], vqt[i]);

	return num;
}

//-------------------------------------

struct VQBoneOptimiser2 : public VQBoneOptimiser {
	ISO_ptr<Model3>		model;
public:
	VQBoneOptimiser2(ISO_ptr<BasePose> _bp, ISO_ptr<Model3> _model, int quant);
	ISO_ptr<MatrixPalette>	MakePalette(int num);
	ISO_ptr<Model3>			GetModel()	const	{ return model;	}
};

VQBoneOptimiser2::VQBoneOptimiser2(ISO_ptr<BasePose> _bp, ISO_ptr<Model3> _model, int quant) : model(Duplicate(_model)) {
	int	total	= 0;
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		ISO_ptr<SubMesh>	submesh		= model->submeshes[i];
		total += ISO::Browser(submesh->verts).Count();
	}

	Init(total);
	VQbone	*p	= data();

	int	offset	= 0;
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		ISO_ptr<SubMesh>	submesh		= model->submeshes[i];
		ISO::TypeOpenArray	*vertstype	= (ISO::TypeOpenArray*)submesh->verts.GetType();
		ISO::TypeComposite*vertsstruct	= (ISO::TypeComposite*)vertstype->subtype->SkipUser();
		const ISO::Element*	weights		= vertsstruct->Find("weights");
		const ISO::Element*	bones		= vertsstruct->Find("bones");
		if (!weights || !bones)
			continue;

		submesh				= Duplicate(submesh);
		submesh->verts		= Duplicate(submesh->verts);
		model->submeshes[i]	= submesh;

		uint32				vertsize	= vertstype->subsize;
		ISO_openarray<char>	*verts		= submesh->verts;
		int					nverts		= verts->Count();
		char				*vertsp		= *verts;

		for (int i = 0; i < nverts; i++, p++) {
			p->set((uint16*)(vertsp + i * vertsize + bones->offset), (float*)(vertsp + i * vertsize + weights->offset), 4);
			p->weight = offset + i;	// original index
		}

		offset += nverts;
	}
	Quantise(quant);
}

ISO_ptr<MatrixPalette> VQBoneOptimiser2::MakePalette(int num) {
	VQbone		*palette	= new VQbone[num];
	uint32		*indices	= new uint32[count2];
	int			size		= Build(num, palette, indices);

	int	offset	= 0;
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		ISO_ptr<SubMesh>	submesh		= model->submeshes[i];
		ISO::TypeOpenArray	*vertstype	= (ISO::TypeOpenArray*)submesh->verts.GetType();
		ISO::TypeComposite*vertsstruct	= (ISO::TypeComposite*)vertstype->subtype->SkipUser();
		const ISO::Element*	weights		= vertsstruct->Find("weights");
		const ISO::Element*	bones		= vertsstruct->Find("bones");
		if (!weights || !bones)
			continue;

		uint32				vertsize	= vertstype->subsize;
		ISO_openarray<char>	*verts		= submesh->verts;
		int					nverts		= verts->Count();
		char				*vertsp		= *verts;

		for (int j = 0; j < nverts; j++) {
			float	*w	= (float*)(vertsp + j * vertsize + weights->offset);
			uint16	*i	= (uint16*)(vertsp + j * vertsize + bones->offset);
			w[0] = 1;
			i[0] = (uint16)indices[order[offset + j]];
			w[1] = w[2] = w[3] = 0;
			i[1] = i[2] = i[3] = 0;
		}
		offset += nverts;
	}

	ISO_ptr<MatrixPalette>	pal(0);
	pal->Create(size);
	for (int i = 0; i < size; i++) {
		const VQbone		&b = palette[i];
		MatrixPaletteWeight	*e = (*pal)[i].Create(b.count());
		for (int j = 0, n = b.count(); j < n; j++) {
			e[j].a = b.v[j].a;
			e[j].b = b.v[j].b * b.weight;
		}
	}

	delete[] palette;
	delete[] indices;

	return pal;
}

ISO_ptr<Node> OptimiseBonesRecurse(ISO_ptr<Node> p, int num, int quant) {
	if (p.IsExternal())
		p = FileHandler::ExpandExternals(p);

	ISO_ptr<Model3>		model;
	ISO_ptr<BasePose>	basepose;
	bool				change	= false;
	for (int i = 0, n = p->children.Count(); i < n; i++) {
		ISO_ptr<void>	child = p->children[i];
		if (child.IsType("Model3")) {
			model	= child;
		} else if (child.IsType("BasePose")) {
			basepose = child;
		} else if (child.IsType("Node")) {
			if (child = OptimiseBonesRecurse(child, num, quant)) {
				if (!change) {
					p = Duplicate(p);
					change = true;
				}
				p->children[i] = child;
			}
		}
	}
	if (basepose && model) {
		if (!change) {
			p = Duplicate(p);
			change = true;
		}
		VQBoneOptimiser1	ob(basepose, model, quant);
		for (int i = 0, n = p->children.Count(); i < n; i++) {
			if (p->children[i].IsType("Model3"))
				p->children[i]	= ob.GetModel();
		}
		p->children.Append(ob.MakePalette(num));
	}

	return change ? p : ISO_ptr<Node>();
}

ISO_ptr<Node> OptimiseBones(ISO_ptr<Node> p, int n, int q) {
	p	= ISO_conversion::convert<Node>(p, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
	ISO_ptr<Node> p2 = OptimiseBonesRecurse(p, n ? n : 256, q ? q : 16);
	return p2 ? p2 : p;
}

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(OptimiseBones)
);
