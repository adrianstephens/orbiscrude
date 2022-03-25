#ifndef MODEL_H
#define MODEL_H

#include "graphics.h"
#include "common/shader.h"
#include "shared/model_defs.h"

namespace iso {

ComponentType		GetComponentType(const ISO::Type *type);
ShaderConstants*	Bind(Model3 *m);
void				Draw(GraphicsContext &ctx, ISO_ptr<Model3> &model);
bool				IsBatched(ISO_ptr<Model3> &model);
void				AddBatch(ISO_ptr<Model3> &model, param(float3x4) world);
void				DrawBatches(int mode);

#ifdef ISO_EDITOR
struct SubMeshPlat : SubMesh {
	struct renderdata {
		int					vert_size:16, prim:8, :8;
		int					nverts;
		int					nindices;
		_VertexBuffer		vb;
		IndexBuffer<uint16>	ib;
		VertexDescription	vd;
	};
	void	Init(void *physram);
	void	DeInit();
	void	Render(GraphicsContext &ctx);
	renderdata *GetRenderData();
};
VertexElement* GetVertexElements(VertexElement *pve, const ISO::TypeComposite *vertex_type);

#else
typedef CONCAT2(ISO_PREFIX,SubMesh)	SubMeshPlat;
typedef CONCAT2(ISO_PREFIX,Texture)	TexturePlat;
#ifdef HAS_GRAPHICSBUFFERS
typedef CONCAT2(ISO_PREFIX,Buffer)	BufferPlat;
#endif
#endif

inline void Init(SubMeshPlat *p, void *physram)	{ p->Init(physram);	}
inline void DeInit(SubMeshPlat *p)				{ p->DeInit(); }

template<typename S> struct ShapeModel : S {
	pass	*tech;
	colour	col;
	ShapeModel(const S &s, pass *tech, param(colour) col) : S(s), tech(tech), col(col) {}
};
template<typename S> ShapeModel<S> make_shape_model(const S &s, pass *tech, param(colour) col) { return {s, tech, col}; }

} // namespace iso

#ifndef ISO_EDITOR

namespace ISO {
	ISO_DEFCALLBACK(CONCAT2(ISO_PREFIX,SubMesh), void);
	ISO_DEFCALLBACKPOD(CONCAT2(ISO_PREFIX,Texture), rint32);

	#ifdef HAS_GRAPHICSBUFFERS
	ISO_DEFUSERPOD(CONCAT2(ISO_PREFIX,Buffer), rint32);
	#endif
} // namespace ISO

#endif

#endif	// MODEL_H
