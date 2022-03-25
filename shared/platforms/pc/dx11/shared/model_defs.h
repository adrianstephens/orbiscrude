#ifndef MODEL_DEFS_H
#define MODEL_DEFS_H

#include "systems/mesh/model_iso.h"
#include "graphics_defs.h"

namespace iso {
	struct DX11SubMesh : SubMeshBase {
		union {
			struct {
				ISO_openarray<D3D11_INPUT_ELEMENT_DESC>	ve;
				uint32				stride;
				uint32				vb_offset;
				uint32				vb_size;
				uint32				ib_offset;
				uint32				ib_size;
			};
	#ifdef GRAPHICS_H
			struct {
				indirect32<VertexDescription>		vd;
				uint32								_stride:16, prim:8, :8;
				indirect32<_VertexBuffer>			vb;
				uint32								_vb_size;
				indirect32<IndexBuffer<uint16> >	ib;
				uint32								_ib_size;
			};
	#endif
		};
	#ifdef GRAPHICS_H
		void	Init(void *physram) {
			ISO_openarray<D3D11_INPUT_ELEMENT_DESC>	ve2 = move(ve);
			void		*verts	= (void*)((uint8*)physram + vb_offset);
			uint16		*inds	= (uint16*)((uint8*)physram + ib_offset);
			(uint32&)ve			= vb_offset = ib_offset = 0;

			if (prim == 0)
				prim = PRIM_TRILIST;

			pass		*p		= (*technique)[0];
			vd.get().Init(ve2, ve2.Count(), p->sub[SS_VERTEX].raw());
			vb.get().Init(verts, vb_size);
			ib.get().Init(inds, ib_size);
		}
		void	DeInit() {
			//((VertexDescription&)ve).~VertexDescription();
			//((_GraphicsBuffer&)vb_offset).~_GraphicsBuffer();
			//((IndexBuffer<uint16>&)ib_offset).~IndexBuffer<uint16>();
		}
		void	Render(GraphicsContext &ctx) {
			ctx.SetVertexType(*&vd);
			ctx.SetVertices(0, vb, _stride);
			ctx.SetIndices(ib);
			ctx.DrawIndexedPrimitive((PrimType)prim, 0, vb_size / _stride, 0, ib_size / 3);
		}
	#endif
		DX11SubMesh()	{ clear(*this); }
		~DX11SubMesh()	{}
	};
}

#endif	// MODEL_DEFS_H
