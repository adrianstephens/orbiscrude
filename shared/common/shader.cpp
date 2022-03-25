#include "shader.h"
#include "graphics.h"
#include "base/algorithm.h"
#include "profiler.h"
#include "crc_dictionary.h"

#if defined(PLAT_PS4) || defined(USE_DX11) || defined(USE_DX12)
#define SKIN_BUFFER
#elif defined(PLAT_PC)
#define SKIN_TEXTURE
#else
#define SKIN_REGISTERS
#endif

using namespace iso;

#ifdef PLAT_METAL
ISO_DEFCALLBACK(METALShaderLib, void);
#endif

namespace iso {

static initialise init(
	ISO::getdef<fx>(),
	ISO::getdef<pass>()
	
	#ifdef PLAT_METAL
	,ISO::getdef<METALShaderLib>()
	#endif
);


//-----------------------------------------------------------------------------
//	ShaderParameters
//-----------------------------------------------------------------------------

Texture			refmap;

struct ShaderParameter			{ arbitrary_ptr data; };
struct ShaderParameterIndirect	{ uint32 nregs; void *p; };

class ShaderParameters : public singleton<ShaderParameters> {
	hash_map<crc32, ShaderParameter, true, 9>	hash;

#ifdef SKIN_REGISTERS
	ShaderParameterIndirect	bones;
#elif defined(SKIN_TEXTURE)
	Texture					bones;
#elif defined(SKIN_BUFFER)
//	Buffer<float4x4>		bones;
	DataBuffer				bones;
#endif

public:
	arbitrary_ptr &get(crc32 name) {
		return hash[name]->data;
	}
	void SetSkinning(const float3x4 *mats, int nmats) {
#ifdef SKIN_REGISTERS
		bones.p		= unconst(mats);
	#ifdef PLAT_OPENGL
		bones.nregs	= nmats;
	#elif defined PLAT_METAL
		bones.nregs	= nmats * 64;
	#elif defined PLAT_PS4
		bones.nregs	= nmats * 16;
	#else
		bones.nregs	= nmats * 4;
	#endif
#elif defined(SKIN_TEXTURE)
		if (!bones)
			bones.Init(TEXF_A32B32G32R32F, 1024 * 4, 1, 1, 1, MEM_CPU_WRITE);
		memcpy(bones.Data(), mats, nmats * sizeof(mats[0]));
#elif defined(SKIN_BUFFER)
		Buffer<float4x4>		&bones2 = bones;
		if (!bones2)
			bones2.Init(1024, MEM_CPU_WRITE);
		memcpy(bones2.WriteData(), mats, nmats * sizeof(mats[0]));
#endif
	}

	ShaderParameters() {
		get("bones")	= &bones;
		get("_refmap")	= &refmap;
	}
};

arbitrary_ptr&	GetShaderParameter(crc32 name) {
	return ShaderParameters::single().get(name);
}

arbitrary_ptr	GetShaderParameter(crc32 name, const ISO::Browser &parameters) {
	if (ISO::Browser b = parameters[tag2(name)])
		return (void*)b;
	return ShaderParameters::single().get(name);
}

void SetSkinning(const float3x4 *mats, int nmats) {
	ShaderParameters::single().SetSkinning(mats, nmats);
}

//-----------------------------------------------------------------------------
//	pass
//-----------------------------------------------------------------------------

void Set(GraphicsContext &ctx, pass *pass, const ISO::Browser &parameters) {
	PROFILE_CPU_EVENT("Shader Set");
	ISO_ASSERT(pass);
	ctx.SetShader(*pass);
	ShaderParameters	&sp = ShaderParameters::single();
	for (ShaderParameterIterator i(*pass); i; ++i) {
		ShaderReg	r = i.Reg();
		const void *p;
		if (ISO::Browser b = parameters[i.Name()])
			p = b;
		else if (!(p = sp.get(i.Name())))
			p = i.Default();
		ctx.SetShaderConstants(r, p);
	}
}

//-----------------------------------------------------------------------------
//	ShaderConstants
//-----------------------------------------------------------------------------

void ShaderConstants::Set(GraphicsContext &ctx, pass *pass) const {
	PROFILE_CPU_EVENT("ShaderConstants Set");
	ctx.SetShader(*pass);
	const ConstantDescriptor	*cd = constants;
	for (int i = total; i--; cd++) {
		ShaderReg	reg		= cd->reg;
		const void *srce = reg.indirect ? *(const void**)cd->srce : cd->srce;
		if (srce && reg.count == 0) {
			ShaderParameterIndirect	*ind = (ShaderParameterIndirect*)srce;
			reg.count	= ind->nregs;
			srce		= ind->p;
		}
		ctx.SetShaderConstants(reg, srce);
	}
}

void ShaderConstants::Init(pass *pass, const ISO::Browser &parameters) {
	ShaderParameterIterator	i(*pass);
	ISO_CHECKHEAP(0);
	ConstantDescriptor	*cd = CreateConstants(i.Total()), *p;

	ShaderParameters	&sp = ShaderParameters::single();
	for (p = cd; i; ++i, ++p) {
		ShaderReg	reg		= i.Reg();
#ifdef SKIN_REGISTERS
		//temp hack!
		if (i.Name() == str("bones"))
			reg.count = 0;
#endif
		void*		srce	= parameters[i.Name()];
		if (!srce) {
			if (arbitrary_ptr &s = sp.get(i.Name())) {
				srce		= s;
			} else {
				srce		= (void*)&s;
				s			= i.DefaultPerm();
				reg.indirect= 1;
			}
		}
		p->reg		= reg;
		p->srce		= srce;
	#ifndef ISO_RELEASE
		p->name		= i.Name();
	#endif
	}
	total = p - cd;
}

#ifdef PLAT_X360
void ShaderConstants::InitSet(GraphicsContext &ctx, pass *pass, const ISO::Browser &parameters, uint32 *stride, VertexElements *ve) {
	if (!constants) {
		Init(pass, parameters);
		pass->Bind(ve, stride);
	}
	Set(ctx, pass);
}
#else
void ShaderConstants::InitSet(GraphicsContext &ctx, pass *pass, const ISO::Browser &parameters) {
	if (!constants)
		Init(pass, parameters);
	Set(ctx, pass);
}
#endif

}//namespace iso

