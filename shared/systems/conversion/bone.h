#ifndef BONES_H
#define BONES_H

#include "iso/iso.h"
#include "base/vector.h"
#include "vq.h"

namespace iso {

enum BONE {
	BONE_ROOT,
	BONE_HIPS,
	BONE_SPINE1,
	BONE_SPINE2,
	BONE_SPINE3,
	BONE_NECK1,
	BONE_NECK2,
	BONE_HEAD,

	BONE_LEFT_CLAVICLE,
	BONE_LEFT_SHOULDER,
	BONE_LEFT_ARM,
	BONE_LEFT_FOREARM,
	BONE_LEFT_HAND,
	BONE_LEFT_THUMB1,
	BONE_LEFT_THUMB2,
	BONE_LEFT_THUMB3,
	BONE_LEFT_INDEX1,
	BONE_LEFT_INDEX2,
	BONE_LEFT_INDEX3,
	BONE_LEFT_MIDDLE1,
	BONE_LEFT_MIDDLE2,
	BONE_LEFT_MIDDLE3,
	BONE_LEFT_RING1,
	BONE_LEFT_RING2,
	BONE_LEFT_RING3,
	BONE_LEFT_PINKY1,
	BONE_LEFT_PINKY2,
	BONE_LEFT_PINKY3,

	BONE_RIGHT_CLAVICLE,
	BONE_RIGHT_SHOULDER,
	BONE_RIGHT_ARM,
	BONE_RIGHT_FOREARM,
	BONE_RIGHT_HAND,
	BONE_RIGHT_THUMB1,
	BONE_RIGHT_THUMB2,
	BONE_RIGHT_THUMB3,
	BONE_RIGHT_INDEX1,
	BONE_RIGHT_INDEX2,
	BONE_RIGHT_INDEX3,
	BONE_RIGHT_MIDDLE1,
	BONE_RIGHT_MIDDLE2,
	BONE_RIGHT_MIDDLE3,
	BONE_RIGHT_RING1,
	BONE_RIGHT_RING2,
	BONE_RIGHT_RING3,
	BONE_RIGHT_PINKY1,
	BONE_RIGHT_PINKY2,
	BONE_RIGHT_PINKY3,

	BONE_LEFT_THIGH,
	BONE_LEFT_LEG,
	BONE_LEFT_FOOT,
	BONE_LEFT_TOES,

	BONE_RIGHT_THIGH,
	BONE_RIGHT_LEG,
	BONE_RIGHT_FOOT,
	BONE_RIGHT_TOES,

	BONE_ROLL	= 0x100,
};

struct BoneMapping {
	const char *name;
	BONE		bone;
};

inline BONE operator|(BONE a, BONE b) { return BONE(int(a) | int(b)); }
BONE		GetParent(BONE a);
const char*	GetName(BONE a);
float4		GetRotCone(BONE a);
BONE		LookupBone(const BoneMapping *m, size_t num, const char *name);
template<int N> BONE LookupBone(const BoneMapping (&m)[N], const char *name) {
	return LookupBone(m, N, name);
}


//-----------------------------------------------------------------------------
//	Bone Opt
//-----------------------------------------------------------------------------

typedef pair<int,float>	bone_weight;
struct bone_palette : ISO_openarray<ISO_openarray<bone_weight> > {};

struct VQbone {
	float		weight;
	dynamic_array<bone_weight>	v;

	VQbone &operator=(const VQbone &b) {
		weight	= b.weight;
		v		= b.v;
		return *this;
	}

	int		count()	const {
		return int(v.size());
	}

	void	set(const uint16 *i, const float *w, int n) {
		v.resize(n);
		weight	= 1;

		for (int j = 0; j < n; j++)
			v[j] = make_pair(i[j], w[j]);

		for (int j = 0; j < n - 1; j++) {
			for (int k = j + 1; k < n; k++) {
				if (v[j].b < v[k].b)
					swap(v[j], v[k]);
			}
		}
		while (n && v[n - 1].b == 0)
			n--;
		v.resize(n);
	}

	void	add(int i, float w) {
		for (int j = 0, n = count(); j < n; j++) {
			if (v[j].a == i) {
				v[j].b += w;
				return;
			}
		}
		new(v) bone_weight(i, w);
	}

	void	quantise(float q) {
		int	n = count();
		for (int j = 0; j < n; j++)
			v[j].b = floor(v[j].b * q + 0.5f) / q;
		while (n && v[n - 1].b == 0)
			n--;
		v.resize(n);
	}

	void	use_closest() {
		v.resize(1);
		v[0].b = 1;
	}

	void	reset()			{ v.clear(); weight = 1;	}
	void	scale(float f)	{ weight *= f;				}

	VQbone()	{}
	~VQbone()	{}
};

inline void swap(VQbone &a, VQbone &b) {
	swap(a.v, b.v);
	swap(a.weight, b.weight);
}


int compare(const VQbone &b1, const VQbone &b2);

struct VQBoneOptimiser {
	typedef	VQbone		element, welement;

	VQbone				*vec;
	size_t				*order;
	uint32				count, count2;

	uint32				size()			const		{ return count2;		}
	const VQbone&		data(size_t i)	const		{ return vec[i];		}
	VQbone*				data()						{ return vec;			}
	float				weightedsum(VQbone &s, size_t i);

	static	void		reset(VQbone &s)			{ return s.reset();		}
	static	void		scale(VQbone &s, float f)	{ s.scale(f);			}

	float				distsquared(const VQbone &a, VQbone &b, float max = 1e38f);
	float				norm(const VQbone &e);

	void				Init(int _count);
	int					Quantise(int quant);
	int					Build(int num, VQbone *palette, uint32 *indices);

	VQBoneOptimiser() : vec(0)	{}
	~VQBoneOptimiser() { delete[] vec; }
};

} // namespace iso

#endif
