#ifndef MODEL_DEFS_H
#define MODEL_DEFS_H

#include "systems/mesh/model_iso.h"
#include "graphics_defs.h"

namespace iso {
	struct OGLSubMesh : SubMeshBase {
		ISO_openarray<OGLVertexElement>	ve;
		uint16	stride, prim;
		union { uint32	vb_offset,	vbo; };
		union { uint32	vb_size,	vao; };
		union { uint32	ib_offset,	ibo; };
		uint32	ib_size;
	#ifdef GRAPHICS_H
		void	Init(void *physram) {
			prim	= ib_offset & 31 ? PRIM_TRISTRIP : PRIM_TRILIST;
			void	*verts	= (uint8*)physram + vb_offset;
			void	*inds	= (uint8*)physram + (ib_offset & ~31);
			(new(&vbo) _VertexBuffer)->Init(verts, vb_size);
			(new(&ibo) _IndexBuffer )->Init(inds, ib_size);
			vao		= 0;
		}
		void	DeInit() {
			((_VertexBuffer*)&vbo)->~_VertexBuffer();
			((_IndexBuffer*)&ibo)->~_IndexBuffer();
			if (vao)
				((VertexDescription*)&vao)->~VertexDescription();
		}
		void	Render(GraphicsContext &ctx) {
			if (!vao)
				new (&vao) VertexDescription(ve, stride, ve.Count(), vbo);
			ctx._SetVertices(vao);
			ctx._SetIndices(ibo);
			ctx.DrawIndexedVertices(PrimType(prim), 0, ib_size / 2);
		}
	#else
		OGLSubMesh(SubMesh *p);
	#endif
	};
}

#endif	// MODEL_DEFS_H
