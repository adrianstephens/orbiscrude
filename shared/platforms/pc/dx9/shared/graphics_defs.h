#ifndef GRAPHICS_DEFS_H
#define GRAPHICS_DEFS_H

#include "dx/dx_helpers.h"
#include "d3d9.h"

namespace iso {

struct PCTexture  {
	D3DFORMAT	format;
	uint16		width, height;
	uint32		depth:10, mips:4, cube:1, array:1;
	uint32		offset;
};
struct PCVertexElement : D3DVERTEXELEMENT9 {};

class PCShader {
public:
	DXwrapperKeep<IDirect3DVertexShader9,32>	vs;
	DXwrapperKeep<IDirect3DPixelShader9,32>		ps;
	void		Bind(PCVertexElement *ve, uint32 *strides) const {}
	void		Init(void *physram);
};

} //namespace iso
#endif	// GRAPHICS_DEFS_H

