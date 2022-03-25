#ifndef MODEL_DEFS_H
#define MODEL_DEFS_H

#include "systems/mesh/model_iso.h"
#include "graphics_defs.h"

namespace iso {
	struct METALSubMesh : SubMeshBase {
		struct offset_size { uint32 offset, size; };
		ISO_openarray<MTLVertexElement>	ve;
		uint16			stride, prim;
		offset_size		vb;
		offset_size		ib;
	#ifdef GRAPHICS_H
		void	Init(void *physram) {
			new(&vb) _GraphicsBuffer((uint8*)physram + vb.offset, vb.size);
			new(&ib) _GraphicsBuffer((uint8*)physram + ib.offset, ib.size);
		}
		void	DeInit() {
			((_GraphicsBuffer*)&vb)->DeInit();
			((_GraphicsBuffer*)&ib)->DeInit();
		}
		void	Render(GraphicsContext &ctx) {
			auto	vbp = (_GraphicsBuffer*)&vb;
			auto	ibp = (IndexBuffer<uint16>*)&ib;
			ctx.SetIndices(*ibp);
			ctx.SetVertexType(VertexDescription(VertexElements((VertexElement*)ve.begin(), ve.size()), stride));
			ctx.SetVertices(0, *vbp);
			ctx.DrawIndexedVertices(PRIM_TRILIST, 0, ibp->Size());
		}
	#else
		METALSubMesh(SubMesh *p);
	#endif
	};
}

#endif	// MODEL_DEFS_H
