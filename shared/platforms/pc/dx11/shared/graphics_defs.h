#ifndef GRAPHICS_DEFS_H
#define GRAPHICS_DEFS_H

#include "base/defs.h"
#include "base/pointer.h"
#include "dx/dx_helpers.h"
#include "dx/dxgi_helpers.h"

#include <d3d11.h>

#define HAS_GRAPHICSBUFFERS

namespace iso {

struct DX11Texture  {
	uint16		width, height;
	uint32		format:8, depth:10, mips:4, cube:1, array:1;
	uint32		offset;
	friend void Init(DX11Texture*,void*);
	friend void DeInit(DX11Texture*);
};

struct DX11Buffer  {
	uint32		size:24, format:8;
	uint32		offset;
};

struct DX11CompactSampler {
	enum MODE { NORMAL = 0, COMP = 1, MIN = 2, MAX = 3 };
	enum ADDR { WRAP = 0, MIRROR = 1, CLAMP = 2, BORDER = 3 };
	enum COMP { NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS};

	union {
		uint32	u;
		struct {
			uint32	mip:1, mag:1, min:1, aniso:1, reduction:2;
			uint32	addru:2, addrv:2, addrw:2;
			uint32	comp:3, max_aniso:4, mip_bias:8;
		};
	};

	DX11CompactSampler(const D3D11_SAMPLER_DESC &d) :
		mip(d.Filter >> D3D11_MIP_FILTER_SHIFT), mag(d.Filter >> D3D11_MAG_FILTER_SHIFT), min(d.Filter >> D3D11_MIN_FILTER_SHIFT), aniso(d.Filter / D3D11_ANISOTROPIC_FILTERING_BIT), reduction(d.Filter >> D3D11_FILTER_REDUCTION_TYPE_SHIFT),
		addru(d.AddressU - 1), addrv(d.AddressV - 1), addrw(d.AddressW - 1),
		comp(d.ComparisonFunc - 1), max_aniso(d.MaxAnisotropy), mip_bias(int(d.MipLODBias * 4))
	{}
	DX11CompactSampler(uint32 _u) : u(_u) {}
	operator D3D11_SAMPLER_DESC() const {
		D3D11_SAMPLER_DESC	d;
		d.Filter	= D3D11_FILTER(
				(min << D3D11_MIN_FILTER_SHIFT)
			|	(mag << D3D11_MAG_FILTER_SHIFT)
			|	(mip << D3D11_MIP_FILTER_SHIFT)
			|	(aniso * D3D11_ANISOTROPIC_FILTERING_BIT)
			|	(reduction << D3D11_FILTER_REDUCTION_TYPE_SHIFT)
		);
		d.AddressU			= D3D11_TEXTURE_ADDRESS_MODE(addru + 1);
		d.AddressV			= D3D11_TEXTURE_ADDRESS_MODE(addrv + 1);
		d.AddressW			= D3D11_TEXTURE_ADDRESS_MODE(addrw + 1);
		d.ComparisonFunc	= D3D11_COMPARISON_FUNC(comp + 1);
		d.MaxAnisotropy		= max_aniso;
		d.MipLODBias		= mip_bias / 4.f;
		d.MinLOD			= 0;
		d.MaxLOD			= 15;
		return d;
	}
};

enum ShaderStage {
	SS_PIXEL,
	SS_VERTEX,
	SS_GEOMETRY,
	SS_HULL,
	SS_LOCAL,
	SS_COMPUTE,
	SS_COUNT = 5
};

class DX11Shader {
public:
	typedef _DXwrapperOpenArray<32> SubShader;
	SubShader	sub[SS_COUNT];

	void		Bind(D3D11_INPUT_ELEMENT_DESC *ve, uint32 n, uint32 *strides) const {}
	bool		Has(int s)			const	{ return sub[s].exists(); }
	bool		StreamoutEnabled()	const	{ return false; }
};

} //namespace iso
#endif	// GRAPHICS_DEFS_H

