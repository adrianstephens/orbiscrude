#ifndef PATCH_H
#define PATCH_H

#include "maths/geometry.h"
#include "common/shader.h"

namespace iso {

struct SubPatch {
	uint32						flags;
	ISO_ptr<iso::technique>		technique;
	ISO_ptr<void>				parameters;
	ISO_ptr<void>				verts;
};

struct PatchModel3 {
	float3p						minext;
	float3p						maxext;
	ISO_openarray<SubPatch>		subpatches;

	friend void Init(PatchModel3*, void*);
	friend void DeInit(PatchModel3*);
};


} // namespace iso

ISO_DEFUSERCOMPV(iso::SubPatch, flags, technique, parameters, verts);
template<> struct ISO::def<iso::PatchModel3> : public ISO::TypeUserCallback {
	ISO::TypeCompositeN<3>	comp;
	def() : ISO::TypeUserCallback((PatchModel3*)0, "PatchModel3", &comp) {
		typedef PatchModel3 _S, _T;
		ISO::Element	*fields = comp.fields;
		ISO_SETFIELDS3(0, minext, maxext, subpatches);
	}
};

struct TesselationInfo {
	iso::sphere			*spheres;
	float				*curvy;
	iso::pair<int,int>	*shared;
	int					num_shared;
	int					total;

	void	Init(iso::PatchModel3 *model);
	void	Calculate(param(iso::float4x4) matrix, float factor, float *tess);
	int		Total()	const { return total; }

	TesselationInfo() : spheres(0), curvy(0), shared(0)	{}
	~TesselationInfo();
};

#ifdef PLAT_X360

struct Tesselation {
	struct iterator {
		IndexBuffer<float>	&ib;
		int					si;
		iterator(IndexBuffer<float> &_ib) : ib(_ib), si(0)	{}
		operator	IndexBuffer<float>&() const	{ return ib;	}
	};

	IndexBuffer<float>	ib[4];
	int					db;
	void			Create(int n) {
		for (int i = 0; i < 4; i++)
			ib[i].Create(n * 4);
		db = 0;
	}
	void			Next()		{ db = (db + 1) & 3;	}
	operator		float*()	{ return ib[db].Data(); }
	iterator		begin()		{ return iterator(ib[db]);	}
};

#else

struct Tesselation {
	typedef float	*iterator;
	float			*p;
	void			Create(int n) { p = new float[n * 4]; }
	void			Next()		{}
	operator		float*()	{ return p;		}
	iterator		begin()		{ return p;		}

	Tesselation() : p(0)		{}
	~Tesselation()				{ delete[] p;	}
};

void Draw(iso::GraphicsContext &ctx, iso::ISO_ptr<iso::PatchModel3> &patch, Tesselation::iterator it);

#endif

#endif// PATCH_H
