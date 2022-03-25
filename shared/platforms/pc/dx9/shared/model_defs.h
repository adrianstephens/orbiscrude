#ifndef MODEL_DEFS_H
#define MODEL_DEFS_H

#include "systems/mesh/model_iso.h"
#include "graphics_defs.h"

namespace iso {
	struct PCSubMesh : SubMeshBase {
		ISO_openarray<PCVertexElement>	ve;
		uint32				stride;
		uint32				vb_offset;
		uint32				vb_size;
		uint32				ib_offset;
		uint32				ib_size;
	#ifdef GRAPHICS_H
		struct renderdata {
			int					vert_size;
			int					nverts;
			int					ntris;
			_VertexBuffer		vb;
			_IndexBuffer		ib;
			VertexDescription	vd;
		};
		void	Init(void *physram) {
			PCVertexElement	*elems	= ve.begin();
			void			*verts	= (void*)((uint8*)physram + vb_offset);
			uint16			*inds	= (uint16*)((uint8*)physram + ib_offset);
			(uint32&)ve = (uint32&)vb_offset = (uint32&)ib_offset = 0;
			new(&ve) VertexDescription(elems);
			((_VertexBuffer&)vb_offset).Init(verts, vb_size);
			((IndexBuffer<uint16>&)ib_offset).Init(inds, ib_size);
			ISO_ASSERT((const VertexDescription&)ve);
		}
		void	DeInit() {
			((VertexDescription&)ve).~VertexDescription();
			((_VertexBuffer&)vb_offset).~_VertexBuffer();
			((IndexBuffer<uint16>&)ib_offset).~IndexBuffer<uint16>();
		}
		void	Render(GraphicsContext &ctx) {
			ctx.SetVertexType((VertexDescription&)ve);
			ctx.SetVertices(0, (_VertexBuffer&)vb_offset, stride);
			ctx.SetIndices((_IndexBuffer&)ib_offset);
			ctx.DrawIndexedPrimitive(PRIM_TRILIST, 0, vb_size / stride, 0, ib_size / 3);
		}
	#endif
	};
}

#endif	// MODEL_DEFS_H
