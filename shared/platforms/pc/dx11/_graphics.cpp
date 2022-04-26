#include "graphics.h"
#include "filetypes\bitmap\bitmap.h"
#include "base\bits.h"
#include "base\algorithm.h"

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

namespace iso {

Graphics	graphics;
Mutex		immediate_mutex;

#if 1//def _DEBUG
bool _CheckResult(HRESULT hr, const char *file, uint32 line) {
	if (FAILED(hr)) {
		char			message[4096];
		toggle_accum	acc;
		acc << "D3D error 0x" << hex(uint32(hr));
		if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), message, sizeof(message), 0))
			acc << ": " << message;
		acc << " at " << file << " line " << line;
		ISO_OUTPUT(acc.detach()->begin());
		//iso_throw(acc.detach()->begin());
		return false;
	}
	return true;
}
#endif

float4x4	hardware_fix(param(float4x4) mat)	{ return translate(zero, zero, half) * scale(float3{one,  -one, -half}) * mat; }
float4x4	map_fix(param(float4x4) mat)		{ return translate(half, half, half) * scale(float3{half, half, -half}) * mat;}

point GetSize(ID3D11Resource *res) {
	D3D11_RESOURCE_DIMENSION	dim;
	res->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER: {
			D3D11_BUFFER_DESC 		desc;
			temp_com_cast<ID3D11Buffer>(res)->GetDesc(&desc);
			return {desc.ByteWidth, 0};
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC		desc;
			temp_com_cast<ID3D11Texture1D>(res)->GetDesc(&desc);
			return {desc.Width, 1};
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC		desc;
			temp_com_cast<ID3D11Texture2D>(res)->GetDesc(&desc);
			return {desc.Width, desc.Height};
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC		desc;
			temp_com_cast<ID3D11Texture3D>(res)->GetDesc(&desc);
			return {desc.Width, desc.Height};
		}
		default:
			return zero;
	}
}

point3 GetSize3D(ID3D11Resource *res) {
	D3D11_RESOURCE_DIMENSION	dim;
	res->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER: {
			D3D11_BUFFER_DESC 		desc;
			temp_com_cast<ID3D11Buffer>(res)->GetDesc(&desc);
			return {desc.ByteWidth, 0, 0};
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC		desc;
			temp_com_cast<ID3D11Texture1D>(res)->GetDesc(&desc);
			return {desc.Width, 1, desc.ArraySize};
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC		desc;
			temp_com_cast<ID3D11Texture2D>(res)->GetDesc(&desc);
			return {desc.Width, desc.Height, desc.ArraySize};
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC		desc;
			temp_com_cast<ID3D11Texture3D>(res)->GetDesc(&desc);
			return {desc.Width, desc.Height, desc.Depth};
		}
		default:
			return point3(zero);
	}
}

uint32 GetMipLevels(ID3D11Resource *res) {
	D3D11_RESOURCE_DIMENSION	dim;
	res->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC		desc;
			temp_com_cast<ID3D11Texture1D>(res)->GetDesc(&desc);
			return desc.MipLevels;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC		desc;
			temp_com_cast<ID3D11Texture2D>(res)->GetDesc(&desc);
			return desc.MipLevels;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC		desc;
			temp_com_cast<ID3D11Texture3D>(res)->GetDesc(&desc);
			return desc.MipLevels;
		}
		default:
			return 0;
	}
}

//-----------------------------------------------------------------------------
//	Buffers
//-----------------------------------------------------------------------------

ID3D11ShaderResourceView *Graphics::MakeDataBuffer(TexFormat format, int stride, void *data, uint32 count) {
	_Buffer		b;

	if (format)
		b.Init(data, count * stride, Bind(D3D11_BIND_SHADER_RESOURCE));
	else
		b.InitStructured(data, count, stride, Bind(D3D11_BIND_SHADER_RESOURCE));

	ID3D11ShaderResourceView	*r;
	b._Bind((DXGI_FORMAT)format, count, &r);
	return r;
}

bool _Buffer::_Bind(DXGI_FORMAT format, uint32 n, ID3D11ShaderResourceView **srv) {
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format					= format;
	desc.ViewDimension			= D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement	= 0;
	desc.Buffer.NumElements		= n;
	return SUCCEEDED(graphics.Device()->CreateShaderResourceView(b, &desc, srv));
}

bool _Buffer::Init(uint32 size, Memory loc) {
	b.clear();
	D3D11_BUFFER_DESC		desc;
	desc.ByteWidth			= size;
	desc.Usage				= GetUsage(loc, true);
	desc.BindFlags			= desc.Usage == D3D11_USAGE_STAGING ? 0 : GetBind(loc);
	desc.CPUAccessFlags		= GetCPUAccess(loc);
	desc.MiscFlags			= 0;

	return CheckResult(graphics.Device()->CreateBuffer(&desc, NULL, &b));
}

bool _Buffer::Init(const void *data, uint32 size, Memory loc) {
	D3D11_BUFFER_DESC		desc;
	desc.ByteWidth			= size;
	desc.Usage				= GetUsage(loc);
	desc.BindFlags			= desc.Usage == D3D11_USAGE_STAGING ? 0 : (GetBind(loc) | D3D11_BIND_SHADER_RESOURCE);
	desc.CPUAccessFlags		= GetCPUAccess(loc);
	desc.MiscFlags			= 0;

	D3D11_SUBRESOURCE_DATA	init;
	init.pSysMem			= data;
	init.SysMemPitch		= 0;
	init.SysMemSlicePitch	= 0;
	return CheckResult(graphics.Device()->CreateBuffer(&desc, &init, &b));
}

bool _Buffer::InitStructured(uint32 n, uint32 stride, Memory loc) {
	b.clear();
	D3D11_BUFFER_DESC		desc;
	desc.ByteWidth			= n * stride;
	desc.Usage				= GetUsage(loc, true);
	desc.BindFlags			= desc.Usage == D3D11_USAGE_STAGING ? 0 : GetBind(loc);
	desc.CPUAccessFlags		= GetCPUAccess(loc);
	desc.MiscFlags			= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = stride;

	return CheckResult(graphics.Device()->CreateBuffer(&desc, NULL, &b));
}

bool _Buffer::InitStructured(const void *data, uint32 n, uint32 stride, Memory loc) {
	D3D11_BUFFER_DESC		desc;
	desc.ByteWidth			= n * stride;
	desc.Usage				= GetUsage(loc);
	desc.BindFlags			= desc.Usage == D3D11_USAGE_STAGING ? 0 : GetBind(loc);
	desc.CPUAccessFlags		= GetCPUAccess(loc);
	desc.MiscFlags			= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = stride;

	D3D11_SUBRESOURCE_DATA	init;
	init.pSysMem			= data;
	init.SysMemPitch		= 0;
	init.SysMemSlicePitch	= 0;
	return CheckResult(graphics.Device()->CreateBuffer(&desc, &init, &b));
}

_Buffer _Buffer::MakeStaging(ID3D11DeviceContext *ctx) const {
	D3D11_BUFFER_DESC	desc = GetDesc();
	desc.Usage			= D3D11_USAGE_STAGING;
	desc.BindFlags		= 0;
	desc.CPUAccessFlags	= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags		= 0;

	_Buffer	b2;
	if (SUCCEEDED(graphics.Device()->CreateBuffer(&desc, NULL, &b2.b)))
		ctx->CopyResource(b2.b, b);
	return b2;
}

void *_Buffer::Begin() const {
	D3D11_MAPPED_SUBRESOURCE map;
	return CheckResult(graphics.Context()->Map(b, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map)) ? map.pData : 0;
}

void _Buffer::End() const {
	graphics.Context()->Unmap(b, 0);
}

//-----------------------------------------------------------------------------
//	Vertices
//-----------------------------------------------------------------------------

bool VertexDescription::Init(D3D11_INPUT_ELEMENT_DESC *ve, uint32 n, const void *vs) {
	return CheckResult(graphics.Device()->CreateInputLayout(ve,n, vs, sized_data(vs).length(), &input));
}

bool VertexDescription::Init(const VertexElement *ve, uint32 n, const void *vs) {
	static const char *const semantics[] = {
		0,
		"POSITION",
		"NORMAL",
		"COLOR",
		"TEXCOORD",
		"TANGENT",
		"BINORMAL",
		"BLENDWEIGHT",
		"BLENDINDICES",
		"PSIZE",
		"TESSFACTOR",
		"FOG",
		"DEPTH",
		"SAMPLE",
	};
	D3D11_INPUT_ELEMENT_DESC *ve2 = alloc_auto(D3D11_INPUT_ELEMENT_DESC, n);
	for (int i = 0; i < n; i++) {
		ve2[i].InputSlot			= ve[i].stream;
		ve2[i].AlignedByteOffset	= ve[i].offset;
		ve2[i].Format				= ve[i].type;
		ve2[i].SemanticName			= semantics[ve[i].usage & 0xff];
		ve2[i].SemanticIndex		= ve[i].usage >> 8;
		ve2[i].InputSlotClass		= D3D11_INPUT_PER_VERTEX_DATA;
		ve2[i].InstanceDataStepRate	= 0;
	}
	return Init(ve2, n, vs);
}

//-----------------------------------------------------------------------------
//	Texture/Surface
//-----------------------------------------------------------------------------

ID3D11ShaderResourceView *_MakeTexture(ID3D11Device *device, DX11Texture *pc, void *physram) {
	ID3D11ShaderResourceView	*srv = 0;
	ID3D11Texture2D				*tex;
	D3D11_TEXTURE2D_DESC		desc;

	desc.Width				= pc->width;
	desc.Height				= pc->height;
	desc.MipLevels			= pc->mips;
	desc.ArraySize			= pc->depth;
	desc.Format				= (DXGI_FORMAT)pc->format;
	desc.SampleDesc.Count	= 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage				= D3D11_USAGE_IMMUTABLE;
	desc.BindFlags			= D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags		= 0;
	desc.MiscFlags			= pc->cube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

	D3D11_SUBRESOURCE_DATA	*data	= alloc_auto(D3D11_SUBRESOURCE_DATA, pc->mips * pc->depth), *p = data;
	uint8					*src	= (uint8*)physram + pc->offset;
	DXGI_COMPONENTS			comp	= desc.Format;
	uint32					bpp		= comp.Bytes();
	uint32					blockx	= comp.IsBlock() ? 4 : 1, blocky = blockx;

	for (int a = 0; a < desc.ArraySize; a++) {
		for (int m = 0; m < desc.MipLevels; m++, p++) {
			int			mipw	= (max(desc.Width  >> m, 1) + blockx - 1) / blockx;
			int			miph	= (max(desc.Height >> m, 1) + blocky - 1) / blocky;
			p->pSysMem			= src;
			p->SysMemPitch		= mipw * bpp;
			p->SysMemSlicePitch	= 0;
			src += p->SysMemPitch * miph;
		}
	}

	return CheckResult(device->CreateTexture2D(&desc, data, &tex))
		&& CheckResult(device->CreateShaderResourceView(tex, NULL, &srv)) ? srv : 0;
}

#ifdef CROSS_PLATFORM
void Init(Texture *x, void *physram) {
	DX11Texture *pc = (DX11Texture*)x->raw();
	if (ISO::GetPtr(pc).IsType("DX11Texture")) {
		_Texture **ptex = x->write();
		if (*ptex)
			(*ptex)->AddRef();
		else
			*ptex = _MakeTexture(graphics.Device(), pc, physram);
	}
}
void DeInit(Texture *x)	{}

void Init(DX11Texture *x, void *physram) {
	if (ISO::GetPtr(x).IsType("DX11Texture")) {
		_DXholderKeep	*dx = (_DXholderKeep*)x;
		ID3D11ShaderResourceView **ptex = (ID3D11ShaderResourceView**)dx->write();
		if (*ptex)
			(*ptex)->AddRef();
		else
			*ptex = _MakeTexture(graphics.Device(), x, physram);
	}
}
void DeInit(DX11Texture *x)	{}
void Init(DataBuffer *x, void *physram) {}
void DeInit(DataBuffer *x)	{}
int Texture::Depth() const	{ return ((bitmap*)raw())->Depth(); }

#else

void Init(Texture *x, void *physram) {}
void DeInit(Texture *x)	{}

void Init(DX11Texture *x, void *physram) {
	_DXholder	*dx = (_DXholder*)x;
	ID3D11ShaderResourceView **ptex = (ID3D11ShaderResourceView**)dx->iso_write();
	*ptex = _MakeTexture(graphics.Device(), x, physram);
}
void DeInit(DX11Texture *x)	{
	_DXholder	*dx = (_DXholder*)x;
	dx->clear<ID3D11ShaderResourceView>();
}
void Init(DataBuffer *x, void *physram) {}
void DeInit(DataBuffer *x)	{}
int	Texture::Depth() const	{ return 1; }

#endif

void Surface::Init(TexFormat format, int width, int height, Memory loc) {
	fmt = format;
	mip = slice = num_slices = 0;

	if (width && height)
		*&*this = graphics.MakeTextureResource(format, width, height, 1, 1, loc);
}
/*
Surface::operator _Texture* () const {
	D3D11_SHADER_RESOURCE_VIEW_DESC	desc;
	iso::clear(desc);
	desc.ViewDimension			= D3D_SRV_DIMENSION_TEXTURE2D;
	desc.Format					= (DXGI_FORMAT)fmt;
	desc.Texture2D.MipLevels	= 1;

	ID3D11ShaderResourceView	*srv	= 0;
	return CheckResult(graphics.Device()->CreateShaderResourceView(*this, &desc, &srv)) ? srv : 0;
}
*/

bool Texture::Init(TexFormat format, int width, int height, int depth, int mips, Memory loc, void *data, int pitch) {
	ID3D11ShaderResourceView *&w = *write();
	if (w)
		w->Release();

	D3D11_SUBRESOURCE_DATA	*init_data = data ? alloc_auto(D3D11_SUBRESOURCE_DATA, mips * depth) : 0;
	if (data) {
		D3D11_SUBRESOURCE_DATA	*p = init_data;
		uint8	*d = (uint8*)data;
		for (int a = 0; a < depth; a++) {
			for (int m = 0; m < mips; m++, p++) {
				uint32			h	= max(height >> m, 1);
				p->pSysMem			= d;
				p->SysMemPitch		= pitch;
				p->SysMemSlicePitch	= pitch * h;
				d	+= p->SysMemSlicePitch;
			}
		}
	}

	return !!(w = graphics.MakeTexture(format, width, height, depth, mips, loc, init_data));
}

bool Texture::Init(TexFormat format, int width, int height, int depth, int mips, Memory loc) {
	ID3D11ShaderResourceView *&w = *write();
	if (w)
		w->Release();

	return !!(w = graphics.MakeTexture(format, width, height, depth, mips, loc, 0));
}

void Texture::DeInit() {
	Texture::~Texture();
//	tex = NULL;
}

ID3D11ShaderResourceView *_MakeTexture(bitmap *bm) {
	if (!bm)
		return 0;

	Memory	loc = bm->Flags() & BMF_VOLUME  ? MEM_VOLUME
				: bm->Flags() & BMF_CUBE	? MEM_CUBE
				: MEM_DEFAULT;

	int		mips	= bm->Mips() + 1, depth = bm->Depth(), subs = bm->IsVolume() ? mips : mips * depth;
	D3D11_SUBRESOURCE_DATA	*init_data = alloc_auto(D3D11_SUBRESOURCE_DATA, subs), *p = init_data;

	if (bm->IsVolume()) {
		for (int m = 0; m < mips; m++, p++) {
			auto	b			= bm->Mip3D(m);
			p->pSysMem			= b[0][0];
			p->SysMemPitch		= b[0].pitch();
			p->SysMemSlicePitch	= b.pitch();
		}
	} else {
		for (int a = 0; a < depth; a++) {
			for (int m = 0; m < mips; m++, p++) {
				auto	b			= bm->MipArray(m)[a];
				p->pSysMem			= b[0];
				p->SysMemPitch		= b.pitch();
				p->SysMemSlicePitch	= 0;
			}
		}
	}
	return graphics.MakeTexture(TEXF_A8B8G8R8, bm->BaseWidth(), bm->BaseHeight(), depth, mips, loc, init_data);
}

ID3D11ShaderResourceView *_MakeTexture(HDRbitmap *bm) {
	if (!bm)
		return 0;

	Memory	loc = bm->Flags() & BMF_VOLUME  ? MEM_VOLUME
				: bm->Flags() & BMF_CUBE	? MEM_CUBE
				: MEM_DEFAULT;

	int		mips	= bm->Mips() + 1, depth = bm->Depth(), subs = bm->IsVolume() ? mips : mips * depth;
	D3D11_SUBRESOURCE_DATA	*init_data = alloc_auto(D3D11_SUBRESOURCE_DATA, subs), *p = init_data;

	if (bm->IsVolume()) {
		for (int m = 0; m < mips; m++, p++) {
			auto	b			= bm->Mip3D(m);
			p->pSysMem			= b[0][0];
			p->SysMemPitch		= b[0].pitch();
			p->SysMemSlicePitch	= b.pitch();
		}
	} else {
		for (int a = 0; a < depth; a++) {
			for (int m = 0; m < mips; m++, p++) {
				auto	b			= bm->MipArray(m)[a];
				p->pSysMem			= b[0];
				p->SysMemPitch		= b.pitch();
				p->SysMemSlicePitch	= 0;
			}
		}
	}
	return graphics.MakeTexture(TEXF_A32B32G32R32F, bm->BaseWidth(), bm->BaseHeight(), depth, mips, loc, init_data);
}

Texture MakeTexture(bitmap *bm) {
	return Texture(_MakeTexture(bm));
}

Texture MakeTexture(HDRbitmap *bm) {
	return Texture(_MakeTexture(bm));
}

Texture::Info Texture::GetInfo() const {
	Info						info;
	com_ptr<ID3D11Resource>		res;
	D3D11_RESOURCE_DIMENSION	dim;

	(*this)->GetResource(&res);
	res->GetType(&dim);
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC			desc;
			((ID3D11Texture1D*)res.get())->GetDesc(&desc);
			info.format = (TexFormat)desc.Format;
			info.width	= desc.Width;
			info.height	= 1;
			info.depth	= desc.ArraySize;
			info.mips	= desc.MipLevels;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC			desc;
			((ID3D11Texture2D*)res.get())->GetDesc(&desc);
			info.format = (TexFormat)desc.Format;
			info.width	= desc.Width;
			info.height	= desc.Height;
			info.depth	= desc.ArraySize;
			info.mips	= desc.MipLevels;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC			desc;
			((ID3D11Texture3D*)res.get())->GetDesc(&desc);
			info.format = (TexFormat)desc.Format;
			info.width	= desc.Width;
			info.height	= desc.Height;
			info.depth	= desc.Depth;
			info.mips	= desc.MipLevels;
			break;
		}
	}
	return info;
}

Surface	Texture::GetSurface(int i) const {
	if (_Texture *tex = *this) {
		com_ptr<ID3D11Resource>			res;
		D3D11_SHADER_RESOURCE_VIEW_DESC	desc;
		tex->GetResource(&res);
		tex->GetDesc(&desc);
		return Surface(res, (TexFormat)desc.Format, i, 0, 1);
	}
	return {};
}

Surface	Texture::GetSurface(CubemapFace f, int i) const {
	if (_Texture *tex = *this) {
		com_ptr<ID3D11Resource>			res;
		D3D11_SHADER_RESOURCE_VIEW_DESC	desc;
		tex->GetResource(&res);
		tex->GetDesc(&desc);
		return Surface(res, (TexFormat)desc.Format, i, f, 1);
	}
	return {};
}

Texture::Texture(const Surface &s) {
	D3D11_SHADER_RESOURCE_VIEW_DESC	desc;
	desc.Format						= (DXGI_FORMAT)s.fmt;
	desc.Texture2D.MostDetailedMip	= s.mip;
	desc.Texture2D.MipLevels		= 1;
	desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE2D;

	CheckResult(graphics.Device()->CreateShaderResourceView(s, &desc, write()));
}

Texture Texture::As(TexFormat format) const {
	if (_Texture *tex = *this) {
		com_ptr<ID3D11Resource>			res;
		D3D11_SHADER_RESOURCE_VIEW_DESC	desc;
		tex->GetResource(&res);
		tex->GetDesc(&desc);

		desc.Format	= (DXGI_FORMAT)format;
		Texture		tex2;
		CheckResult(graphics.Device()->CreateShaderResourceView(res, &desc, tex2.write()));
		return tex2;
	}
	return {};
}

//-----------------------------------------------------------------------------
//	Shaders
//-----------------------------------------------------------------------------

D3D11_SAMPLER_DESC	default_sampler = {
	D3D11_FILTER_MIN_MAG_MIP_LINEAR,	//D3D11_FILTER Filter;
	D3D11_TEXTURE_ADDRESS_WRAP,			//D3D11_TEXTURE_ADDRESS_MODE AddressU;
	D3D11_TEXTURE_ADDRESS_WRAP,			//D3D11_TEXTURE_ADDRESS_MODE AddressV;
	D3D11_TEXTURE_ADDRESS_WRAP,			//D3D11_TEXTURE_ADDRESS_MODE AddressW;
	0,									//FLOAT MipLODBias;
	1,									//UINT	MaxAnisotropy;
	D3D11_COMPARISON_NEVER,				//D3D11_COMPARISON_FUNC ComparisonFunc;
	{0, 0, 0, 0},						//FLOAT BorderColor[ 4 ];
	0,									//FLOAT MinLOD;
	1e38f,								//FLOAT MaxLOD;
};

template<
	typename T,
	HRESULT (ID3D11Device::*F)(const void*, SIZE_T, ID3D11ClassLinkage*, T**)
> void InitShaderStage(ID3D11Device *device, DX11Shader::SubShader &sub) {
	if (T **w = sub.write<T>()) {
		if (T *t = *w) {
			t->AddRef();
		} else {
			sized_data	data = sub;
			(device->*F)(data, data.length(), 0, w);
		}
	}
}

void Init(DX11Shader *x, void *physram) {
	if (ID3D11Device *device = graphics.GetDevice()) {
		if (!x->Has(SS_VERTEX)) {
			InitShaderStage<ID3D11ComputeShader,	&ID3D11Device::CreateComputeShader>	(device, x->sub[SS_PIXEL]);
		} else {
			InitShaderStage<ID3D11PixelShader,		&ID3D11Device::CreatePixelShader>	(device, x->sub[SS_PIXEL]);
			InitShaderStage<ID3D11VertexShader,		&ID3D11Device::CreateVertexShader>	(device, x->sub[SS_VERTEX]);
			InitShaderStage<ID3D11GeometryShader,	&ID3D11Device::CreateGeometryShader>(device, x->sub[SS_GEOMETRY]);
			InitShaderStage<ID3D11HullShader,		&ID3D11Device::CreateHullShader>	(device, x->sub[SS_HULL]);
			InitShaderStage<ID3D11DomainShader,		&ID3D11Device::CreateDomainShader>	(device, x->sub[SS_LOCAL]);
		}
	}
}

void DeInit(DX11Shader *x) {
	for (int i = 0; i < SS_COUNT; i++)
		x->sub[i].clear<IUnknown>();
}

int ShaderParameterIterator::ArrayCount(const char *begin, const char *&end) {
	while (*--end != '[');

	int			len			= end - begin;
	int			index0		= from_string<int>(end + 1);
	const char *last_name	= begin;

	while (!bindings.empty()) {
		const char *name	= bindings.front().name.get(rdef);
		if (strncmp(begin, name, len) != 0)
			break;
		last_name	= name;
		bindings.pop_front();
	}

	int		index1 = from_string<int>(last_name + len + 1);
	return index1 - index0 + 1;
}

void ShaderParameterIterator::Next() {
	for (;;) {
		while (vars.empty()) {
			while (bindings.empty()) {
				if (++stage == SS_COUNT)
					return;

				if (dx::DXBC *dxbc = (dx::DXBC*)shader.sub[stage].raw()) {
					//rdef		= dxbc->GetBlob<dx::RDEF>();
					rdef		= dxbc->GetBlob<dx::RD11>();
					bindings	= rdef->Bindings();
				}
			}

			auto &bind	= bindings.pop_front_value();
			name = bind.name.get(rdef);

			switch (bind.type) {
				case dx::RDEF::Binding::CBUFFER: {
					cbuff_index	= bind.bind;
					auto	&b	= rdef->buffers.get(rdef)[cbuff_index];
					vars		= rdef->Variables(b);
					//reg			= ShaderReg(bind.bind, 1, 0, ShaderStage(stage), SPT_BUFFER);
					//return;
					break;
				}

				case dx::RDEF::Binding::TEXTURE:
					val	= 0;
					if (is_buffer(bind.dim)) {
						reg		= ShaderReg(bind.bind, bind.bind_count, 0, ShaderStage(stage), SPT_BUFFER);
					} else {
						const char	*end	= str(name).end();
						int			count	= bind.bind_count;
						if (end[-1] == ']')
							count = ArrayCount(name, end);
						if (end[-2] == '_' && end[-1] == 't')
							end -= 2;
						if (*end)
							name = temp_name = str(name, end);
						reg		= ShaderReg(bind.bind, count, 0, ShaderStage(stage), SPT_TEXTURE);
					}
					return;

				case dx::RDEF::Binding::SAMPLER: {
					const char	*end	= str(name).end();
					int			count	= bind.bind_count;
					if (end[-1] == ']')
						count = ArrayCount(name, end);
//					if (end[-2] == '_' && end[-1] == 's')
//						name = temp_name = str(name, end - 2);
					if (*end)
						name = temp_name = str(name, end);

					reg		= ShaderReg(bind.bind, count, 0, ShaderStage(stage), SPT_SAMPLER);
					val		= bind.samples ? (void*)((uint8*)&bind.samples + bind.samples) : (void*)&default_sampler;
					return;
				}

				default:
					reg		= ShaderReg(bind.bind, bind.bind_count, 0, ShaderStage(stage), SPT_BUFFER);
					val		= 0;
					return;
			}
		}
		auto	&v = vars.pop_front_value();
		if (v.flags & v.USED) {
			name	= v.name.get(rdef);
			val		= v.def.get(rdef);
			reg		= ShaderReg(v.offset, v.size, cbuff_index, ShaderStage(stage), SPT_VAL);
			return;
		}
	}
}

ShaderParameterIterator &ShaderParameterIterator::Reset() {
	stage		= -1;
	vars		= empty;
	bindings	= empty;
	Next();
	return *this;
}

int	ShaderParameterIterator::Total() const {
	int							total = 0;
	for (int i = 0; i < SS_COUNT; i++) {
		if (dx::DXBC *dxbc = (dx::DXBC*)shader.sub[i].raw()) {
			dx::RD11	*rdef	= dxbc->GetBlob<dx::RD11>();
			for (auto &i : rdef->Bindings())
				total += int(i.type != dx::RDEF::Binding::CBUFFER);
			for (auto b : rdef->Buffers()) {
				for (auto &v : rdef->Variables(b)) {
					if (v.flags & v.USED)
						++total;
				}
			}
		}
	}
	return total;
}

//-----------------------------------------------------------------------------
//	Graphics
//-----------------------------------------------------------------------------
#ifdef _DEBUG
//#define GRAPHICS_DEBUG
#endif

Graphics::Graphics() : fa(malloc_block(0x10000).detach()), frame(-1) {
	ISO::getdef<bitmap>();
}

Graphics::~Graphics() {
	if (context) {
		context->ClearState();
		context->Flush();
		context.clear();
	#ifdef GRAPHICS_DEBUG
		com_ptr<ID3D11Debug> debug;
		if (SUCCEEDED(device.query(&debug)))
			debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
	#endif
	}
}


bool Graphics::Init(IDXGIAdapter *adapter) {
	static const D3D_FEATURE_LEVEL	feature_levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	if (device) {
		if (!adapter || adapter == temp_com_cast<IDXGIAdapter>(device))
			return true;
		device.clear();
		context.clear();
	}

	frame = 0;

#ifdef GRAPHICS_DEBUG
	#define	D3D11_CREATE_DEVICE_DEBUG1	D3D11_CREATE_DEVICE_DEBUG
#else
	#define	D3D11_CREATE_DEVICE_DEBUG1	0
#endif

	D3D_FEATURE_LEVEL	feature_level;
	if (FAILED(D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL,
		D3D11_CREATE_DEVICE_DEBUG1 | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		feature_levels, num_elements32(feature_levels),
		D3D11_SDK_VERSION,
		&device, &feature_level, &context
	)))
		return false;

	if (feature_level < D3D_FEATURE_LEVEL_11_0) {
		D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS opts;
		device->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &opts, sizeof(opts));
		if (opts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
			features.set(COMPUTE);
	} else {
		features.set(COMPUTE);
		D3D11_FEATURE_DATA_DOUBLES opts;
		device->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &opts, sizeof(opts));
		if (opts.DoublePrecisionFloatShaderOps)
			features.set(DOUBLES);
	}
	D3D11_FEATURE_DATA_D3D11_OPTIONS3 opts;
	device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &opts, sizeof(opts));
	if (opts.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer)
		features.set(VPRT);

#ifdef GRAPHICS_DEBUG
	com_ptr<ID3D11Debug> debug;
	if (SUCCEEDED(device.query(&debug))) {
		com_ptr<ID3D11InfoQueue> queue;
		if (SUCCEEDED(debug.query(&queue))) {
			//queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
			queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

			D3D11_MESSAGE_ID hide[] = {
				D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
			};

			D3D11_INFO_QUEUE_FILTER	filter;
			clear(filter);
			filter.DenyList.NumIDs	= (UINT)num_elements(hide);
			filter.DenyList.pIDList = hide;
			queue->AddStorageFilterEntries(&filter);
		}
	}
#endif
	return true;
}

bool Graphics::StageState::Init(const void *_raw) {
	if (raw == _raw)
		return false;
	if (raw = _raw) {
		if (dx::RDEF *rdef = ((dx::DXBC*)_raw)->GetBlob<dx::RDEF>()) {
			int	x = 0;
			for (auto &i : rdef->Buffers())
				Set(graphics.FindConstBuffer(i.name.get(rdef), i.size, i.num_variables), x++);
		}
	}
	return true;
}

ConstBuffer *Graphics::FindConstBuffer(crc32 id, uint32 size, uint32 num) {
	auto	i = cb[CBKey(id, size, num)];
	if (!i.exists())
		i = new ConstBuffer(size);
	return i;
}

void Graphics::BeginScene(GraphicsContext &ctx) {
	fa.BeginFrame(frame % 2);

	ctx.context = context;
	immediate_mutex.lock();
	ctx._Begin();
}

void Graphics::EndScene(GraphicsContext &ctx) {
	fa.EndFrame(frame++ % 2);

	ctx.End();
	ctx.ClearTargets();
	immediate_mutex.unlock();
}

ID3D11Resource *Graphics::MakeTextureResource(TexFormat format, int width, int height, int depth, int mips, Memory loc, const D3D11_SUBRESOURCE_DATA *init_data) {
	DXGI_FORMAT	tex_format	= (DXGI_FORMAT)format;
	D3D11_USAGE	usage		= GetUsage(loc, !init_data);
	uint32		bind		= usage == D3D11_USAGE_STAGING ? 0 : GetBind(loc);
	uint32		cpu_access	= loc & _MEM_ACCESS_MASK;
	bool		is_depth	= bind & D3D11_BIND_DEPTH_STENCIL;

	switch (tex_format) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			tex_format	= DXGI_FORMAT_R32G32_TYPELESS;
			is_depth	= true;
			break;

		case DXGI_FORMAT_D32_FLOAT:
			tex_format	= DXGI_FORMAT_R32_TYPELESS;
			is_depth	= true;
			break;

		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			tex_format	= DXGI_FORMAT_R24G8_TYPELESS;
			is_depth	= true;
			break;

		case DXGI_FORMAT_D16_UNORM:
			tex_format	= DXGI_FORMAT_R16_TYPELESS;
			is_depth	= true;
			break;

		default:
			if (loc & (MEM_CASTABLE | MEM_DEPTH))
				tex_format = DXGI_COMPONENTS(tex_format).Type(DXGI_COMPONENTS::TYPELESS).GetFormat();
			break;
	}

	if (is_depth && !(bind & D3D11_BIND_UNORDERED_ACCESS))
		bind |= D3D11_BIND_DEPTH_STENCIL;

	if ((loc & MEM_FORCE2D) == MEM_FORCE2D) {
		D3D11_TEXTURE2D_DESC	tex_desc;
		tex_desc.Width				= width;
		tex_desc.Height				= height;
		tex_desc.MipLevels			= mips;
		tex_desc.ArraySize			= depth;
		tex_desc.Format				= tex_format;
		tex_desc.SampleDesc.Count	= 1;
		tex_desc.SampleDesc.Quality	= 0;
		tex_desc.Usage				= usage;
		tex_desc.BindFlags			= bind;
		tex_desc.CPUAccessFlags		= cpu_access;
		tex_desc.MiscFlags			= 0;

		ID3D11Texture2D		*tex	= 0;
		CheckResult(device->CreateTexture2D(&tex_desc, init_data, &tex));
		return tex;

	} else if (height == 1) {
		D3D11_TEXTURE1D_DESC	tex_desc;
		tex_desc.Width				= width;
		tex_desc.MipLevels			= mips;
		tex_desc.ArraySize			= depth;
		tex_desc.Format				= tex_format;
		tex_desc.Usage				= usage;
		tex_desc.BindFlags			= bind;
		tex_desc.CPUAccessFlags		= cpu_access;
		tex_desc.MiscFlags			= 0;

		ID3D11Texture1D		*tex	= 0;
		CheckResult(device->CreateTexture1D(&tex_desc, init_data, &tex));
		return tex;

	} else if (loc & MEM_VOLUME) {
		D3D11_TEXTURE3D_DESC	tex_desc;
		tex_desc.Width				= width;
		tex_desc.Height				= height;
		tex_desc.Depth				= depth;
		tex_desc.MipLevels			= mips;
		tex_desc.Format				= tex_format;
		tex_desc.Usage				= usage;
		tex_desc.BindFlags			= bind;
		tex_desc.CPUAccessFlags		= cpu_access;
		tex_desc.MiscFlags			= 0;

		ID3D11Texture3D		*tex	= 0;
		CheckResult(device->CreateTexture3D(&tex_desc, init_data, &tex));
		return tex;

	} else {
		D3D11_TEXTURE2D_DESC	tex_desc;
		tex_desc.Width				= width;
		tex_desc.Height				= height;
		tex_desc.MipLevels			= mips;
		tex_desc.ArraySize			= depth;
		tex_desc.Format				= tex_format;
		tex_desc.SampleDesc.Count	= 1;
		tex_desc.SampleDesc.Quality	= 0;
		tex_desc.Usage				= usage;
		tex_desc.BindFlags			= bind;
		tex_desc.CPUAccessFlags		= cpu_access;
		tex_desc.MiscFlags			= loc & MEM_CUBE ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

		ID3D11Texture2D		*tex	= 0;
		CheckResult(device->CreateTexture2D(&tex_desc, init_data, &tex));
		return tex;
	}
}

ID3D11ShaderResourceView *Graphics::MakeTextureView(ID3D11Resource *tex, TexFormat format) {
	D3D11_SHADER_RESOURCE_VIEW_DESC	desc;
	iso::clear(desc);
	desc.ViewDimension			= D3D_SRV_DIMENSION_TEXTURE2D;
	desc.Format					= (DXGI_FORMAT)format;
	desc.Texture2D.MipLevels	= 1;

	ID3D11ShaderResourceView	*srv	= 0;
	return CheckResult(device->CreateShaderResourceView(tex, &desc, &srv)) ? srv : 0;
}

ID3D11ShaderResourceView *Graphics::MakeTextureView(ID3D11Resource *tex, TexFormat format, int width, int height, int depth, int mips, Memory loc) {
	ID3D11ShaderResourceView		*srv	= 0;
	D3D11_SHADER_RESOURCE_VIEW_DESC	srv_desc;

	clear(srv_desc);
	srv_desc.Format	= (DXGI_FORMAT)format;

	switch (srv_desc.Format) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	srv_desc.Format = DXGI_FORMAT_R32G32_UINT;				break;
		case DXGI_FORMAT_D32_FLOAT:				srv_desc.Format = DXGI_FORMAT_R32_FLOAT;				break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:		srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;	break;
		case DXGI_FORMAT_D16_UNORM:				srv_desc.Format = DXGI_FORMAT_R16_UINT;					break;
		default:								break;
	}

	if ((loc & MEM_FORCE2D) == MEM_FORCE2D) {
		srv_desc.Texture2D.MipLevels		= mips;

		if (depth == 1) {
			srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE2D;
		} else {
			srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.ArraySize	= depth;
		}

	} else if (height == 1) {
		srv_desc.Texture1D.MipLevels		= mips;
		if (depth == 1) {
			srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE1D;
		} else {
			srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE1DARRAY;
			srv_desc.Texture1DArray.ArraySize	= depth;
		}

	} else if (loc & MEM_VOLUME) {
		srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture3D.MipLevels		= mips;

	} else {
		srv_desc.Texture2D.MipLevels		= mips;

		if (loc & MEM_CUBE) {
			if (depth <= 6) {
				srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURECUBE;
			} else {
				srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURECUBEARRAY;
				srv_desc.TextureCubeArray.NumCubes	= depth / 6;
			}
		} else {
			if (depth == 1) {
				srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE2D;
			} else {
				srv_desc.ViewDimension				= D3D_SRV_DIMENSION_TEXTURE2DARRAY;
				srv_desc.Texture2DArray.ArraySize	= depth;
			}
		}
	}
	return CheckResult(device->CreateShaderResourceView(tex, &srv_desc, &srv)) ? srv : 0;
}

ID3D11ShaderResourceView *Graphics::MakeTexture(TexFormat format, int width, int height, int depth, int mips, Memory loc, const D3D11_SUBRESOURCE_DATA *init_data) {
	com_ptr<ID3D11Resource>	tex = MakeTextureResource(format, width, height, depth, mips, loc, init_data);
	return MakeTextureView(tex, format, width, height, depth, mips, loc);
}

bool Graphics::MakeUAV(ID3D11Buffer *b, ID3D11UnorderedAccessView **uav) {
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	clear(uav_desc);
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	D3D11_BUFFER_DESC bd;
	b->GetDesc(&bd);

	if (bd.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) {
		uav_desc.Format				= DXGI_FORMAT_R32_TYPELESS; // Format must be DXGI_FORMAT_R32_TYPELESS, when creating Raw Unordered Access View
		uav_desc.Buffer.Flags		= D3D11_BUFFER_UAV_FLAG_RAW;
		uav_desc.Buffer.NumElements	= bd.ByteWidth / 4;
	} else if (bd.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
		uav_desc.Format				= DXGI_FORMAT_UNKNOWN;      // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
		uav_desc.Buffer.NumElements	= bd.ByteWidth / bd.StructureByteStride;
	} else {
		return false;
	}
	return CheckResult(device->CreateUnorderedAccessView(b, &uav_desc, uav));
}

bool Graphics::MakeUAV(ID3D11Resource *res, ID3D11UnorderedAccessView **uav) {
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	clear(uav_desc);

	D3D11_RESOURCE_DIMENSION	dim;
    res->GetType(&dim);

	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER: {
			D3D11_BUFFER_DESC bd;
			((ID3D11Buffer*)res)->GetDesc(&bd);

			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			if (bd.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) {
				uav_desc.Format				= DXGI_FORMAT_R32_TYPELESS; // Format must be DXGI_FORMAT_R32_TYPELESS, when creating Raw Unordered Access View
				uav_desc.Buffer.Flags		= D3D11_BUFFER_UAV_FLAG_RAW;
				uav_desc.Buffer.NumElements	= bd.ByteWidth / 4;
			} else if (bd.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
				uav_desc.Format				= DXGI_FORMAT_UNKNOWN;      // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
				uav_desc.Buffer.NumElements	= bd.ByteWidth / bd.StructureByteStride;
			} else {
				return false;
			}
			break;
		}

		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC	td;
			((ID3D11Texture1D*)res)->GetDesc(&td);
			//if (td.ArraySize > 1) {
			uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE1D;	//D3D11_UAV_DIMENSION_TEXTURE1DARRAY
			uav_desc.Format				= td.Format;
			uav_desc.Texture1D.MipSlice	= 0;
			break;
		}

		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC	td;
			((ID3D11Texture2D*)res)->GetDesc(&td);
			uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE2D;	//D3D11_UAV_DIMENSION_TEXTURE2DARRAY
			uav_desc.Format				= td.Format;
			uav_desc.Texture2D.MipSlice	= 0;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC	td;
			((ID3D11Texture3D*)res)->GetDesc(&td);
			uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE3D;
			uav_desc.Format				= td.Format;
			uav_desc.Texture3D.MipSlice	= 0;
			uav_desc.Texture3D.FirstWSlice= 0;
			uav_desc.Texture3D.WSize		= 1;
			break;
		}
	}

	return CheckResult(device->CreateUnorderedAccessView(res, &uav_desc, uav));
}

bool Graphics::MakeUAV(ID3D11ShaderResourceView *srv, ID3D11UnorderedAccessView **uav) {
	D3D11_SHADER_RESOURCE_VIEW_DESC		srv_desc;
	D3D11_UNORDERED_ACCESS_VIEW_DESC	uav_desc;
	clear(uav_desc);

    srv->GetDesc(&srv_desc);
	uav_desc.Format			= srv_desc.Format;
	uav_desc.ViewDimension	= (D3D11_UAV_DIMENSION)srv_desc.ViewDimension;

	switch (srv_desc.ViewDimension) {
		case D3D11_SRV_DIMENSION_BUFFER:
			uav_desc.Buffer.NumElements	= srv_desc.Buffer.NumElements;
			break;

		case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
			uav_desc.Texture1DArray.ArraySize	= srv_desc.Texture1DArray.ArraySize;
			break;

		case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
			uav_desc.Texture2DArray.ArraySize	= srv_desc.Texture2DArray.ArraySize;
			break;

		case D3D11_SRV_DIMENSION_TEXTURE3D:
			uav_desc.Texture3D.WSize	= 1;
			break;
	}

	com_ptr<ID3D11Resource>		res;
	srv->GetResource(&res);
	return CheckResult(device->CreateUnorderedAccessView(res, &uav_desc, uav));
}

ID3D11DeviceContext *ImmediateContext() {
	return graphics.Context();
}

// Call this method when the app suspends. It provides a hint to the driver that the app is entering an idle state and that temporary buffers can be reclaimed for use by other apps
void Graphics::Trim() {
	context->ClearState();
	if (auto dxgi = temp_com_cast<IDXGIDevice3>(device))
		dxgi->Trim();
}

//-----------------------------------------------------------------------------
//	ComputeContext
//-----------------------------------------------------------------------------

bool ComputeContext::Begin() {
	uav_min = uav_max = 0;
	if (context)
		return true;

	auto	device = graphics.GetDevice();

	if (immediate_mutex.try_lock()) {
		context = graphics.context;
		return true;
	}
	return CheckResult(device->CreateDeferredContext(0, &context));
}
void ComputeContext::Reset() {
	auto	nulls = alloc_auto(void*, 16);
	memset(nulls, 0, sizeof(void*) * 16);
//	context->CSSetShaderResources(0, 16, (ID3D11ShaderResourceView*const*)nulls);
	context->CSSetUnorderedAccessViews(0, 16, (ID3D11UnorderedAccessView*const*)nulls, 0);
}

void ComputeContext::End() {
	if (context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
		DingDong();
	} else {
		immediate_mutex.unlock();
		context = 0;
	}
}

void ComputeContext::DingDong() {
	if (context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
		com_ptr<ID3D11CommandList> commandlist;
		context->FinishCommandList(FALSE, &commandlist);
		with(immediate_mutex), graphics.Context()->ExecuteCommandList(commandlist, FALSE);
	}
}

void ComputeContext::SetShader(const DX11Shader &s) {
	ISO_ASSERT(!s.Has(SS_VERTEX));
	if (cs_state.Init(s.sub[SS_PIXEL].raw()))
		context->CSSetShader(s.sub[SS_PIXEL].get<ID3D11ComputeShader>(), NULL, 0);
}

void ComputeContext::SetShaderConstants(ShaderReg reg, const void *values) {
	if (!values)
		return;
	ISO_ASSERT(reg.stage == SS_PIXEL);//SS_COMPUTE);

	int		n		= reg.count;
	switch (reg.type) {
		case SPT_VAL:
			if (cs_state.Get(reg.buffer)->SetConstant(reg.offset, values, n))
				cs_state.dirty |= 1 << reg.buffer;
			break;
		case SPT_TEXTURE: {
			ID3D11ShaderResourceView	**view	= alloc_auto(ID3D11ShaderResourceView*, n);
			for (int i = 0; i < n; i++)
				view[i] = ((Texture*)values)[i];
			context->CSSetShaderResources(reg.offset, n, view);
			break;
		}
		case SPT_SAMPLER: {
			D3D11_SAMPLER_DESC	desc;
			desc.Filter 		= D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU 		= D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV 		= D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW 		= D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.MipLODBias 	= 0;
			desc.MaxAnisotropy 	= 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			desc.BorderColor[0] = 1;
			desc.BorderColor[1] = 1;
			desc.BorderColor[2] = 1;
			desc.BorderColor[3] = 1;
			desc.MinLOD 		= -3.402823466e+38f; // -FLT_MAX
			desc.MaxLOD 		= 3.402823466e+38f; // FLT_MAX

			ID3D11SamplerState 	**samp	= alloc_auto(ID3D11SamplerState *, n);
			for (int i = 0; i < n; i++)
				graphics.device->CreateSamplerState(&desc, &samp[i]);

			context->CSSetSamplers(reg.offset, n, samp);
			for (int i = 0; i < n; i++)
				samp[i]->Release();
			break;
		}
		case SPT_BUFFER: {
#if 1
			ID3D11ShaderResourceView	**view	= alloc_auto(ID3D11ShaderResourceView*, n);
			for (int i = 0; i < n; i++)
				view[i] = ((DataBuffer*)values)[i];
#else
			ID3D11ShaderResourceView	*const *view	= (ID3D11ShaderResourceView* const*)values;
#endif
			context->CSSetShaderResources(reg.offset, n, view);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//	GraphicsContext
//-----------------------------------------------------------------------------

void GraphicsContext::Begin() {
	if (!context)
		graphics.Device()->CreateDeferredContext(0, &context);
}

void GraphicsContext::_Begin() {
	context->ClearState();
	clear(raster);
	clear(blend);
	clear(depth);
	clear(samplers);
	clear(blendfactor);

	for (int i = 0; i < num_elements(stage_states); i++)
		stage_states[i].Clear();

	static const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
		FALSE,
		D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		blend.RenderTarget[i] = defaultRenderTargetBlendDesc;

	raster.FillMode			= D3D11_FILL_SOLID;
	raster.CullMode			= D3D11_CULL_NONE;
	raster.FrontCounterClockwise	= TRUE;
    raster.DepthClipEnable			= TRUE;

	static const D3D11_DEPTH_STENCILOP_DESC defaultStencilOp =	{
		D3D11_STENCIL_OP_KEEP,
		D3D11_STENCIL_OP_KEEP,
		D3D11_STENCIL_OP_KEEP,
		D3D11_COMPARISON_ALWAYS
	};
	depth.DepthWriteMask	= D3D11_DEPTH_WRITE_MASK_ALL;
	depth.DepthFunc			= D3D11_COMPARISON_LESS;
	depth.StencilReadMask	= 0xff;
	depth.StencilWriteMask	= 0xff;
	depth.FrontFace			= defaultStencilOp;
	depth.BackFace			= defaultStencilOp;

	update					= UPD_BLEND | UPD_DEPTH | UPD_RASTER;
	num_render_buffers		= 0;
	stencil_ref				= 0;

	SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
}

void GraphicsContext::End() {
	if (context->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED) {
		auto	wm(with(immediate_mutex));
		com_ptr<ID3D11CommandList> commandlist;
		context->FinishCommandList(FALSE, &commandlist);
		graphics.Context()->ExecuteCommandList(commandlist, FALSE);
	}
}

void GraphicsContext::FlushDeferred2() {
	if (update.test(UPD_BLEND))
		blendstate = graphics.GetBlendObject(blend);

	if (update.test_any(UPD_BLEND | UPD_BLENDFACTOR))
		context->OMSetBlendState(blendstate, (float*)&blendfactor, 0xffffffff);

	if (update.test(UPD_DEPTH))
		depthstate = graphics.GetDepthStencilObject(depth);

	if (update.test_any(UPD_DEPTH | UPD_STENCIL_REF))
		context->OMSetDepthStencilState(depthstate, stencil_ref);

	if (update.test(UPD_RASTER)) {
		rasterstate = graphics.GetRasterObject(raster);
		context->RSSetState(rasterstate);
	}

	if (update & (UPD_CBS * bits(SS_COUNT))) {
		ID3D11Buffer *buffs[16];
		if (update.test(UPD_CBS_PIXEL))		context->PSSetConstantBuffers(0, stage_states[SS_PIXEL	].Flush(context, buffs), buffs);
		if (update.test(UPD_CBS_VERTEX))	context->VSSetConstantBuffers(0, stage_states[SS_VERTEX	].Flush(context, buffs), buffs);
		if (update.test(UPD_CBS_GEOMETRY))	context->GSSetConstantBuffers(0, stage_states[SS_GEOMETRY].Flush(context, buffs), buffs);
		if (update.test(UPD_CBS_HULL))		context->HSSetConstantBuffers(0, stage_states[SS_HULL	].Flush(context, buffs), buffs);
		if (update.test(UPD_CBS_LOCAL))		context->DSSetConstantBuffers(0, stage_states[SS_LOCAL	].Flush(context, buffs), buffs);
	}

	if (update.test(UPD_TARGETS))
		FlushTargets();
	/*
	if (uint8 m = update & 0xff) {
		int		i = lowest_set_index(m);
		int		n = highest_set(m >> i);
		context->PSSetSamplers(i, n, (ID3D11SamplerState**)(samplerstates + i));
	}
	*/
	update = 0;
}

void GraphicsContext::SetWindow(const rect &rect) {
	viewport.TopLeftX	= rect.a.x;
	viewport.TopLeftY	= rect.a.y;
	point	size		= rect.extent();
	viewport.Width		= size.x;
	viewport.Height		= size.y;
	viewport.MinDepth	= 0;
	viewport.MaxDepth	= 1.0f;
	context->RSSetViewports(1, &viewport);
}

rect GraphicsContext::GetWindow() {
	return rect::with_length(point{viewport.TopLeftX, viewport.TopLeftY}, point{viewport.Width, viewport.Height});
}

template<typename T> const T *temp_addr(const T &t) { return &t; }

void GraphicsContext::SetRenderTarget(const Surface& s, RenderTarget i) {
	if (i == RT_DEPTH) {
		depth_buffer.clear();
		if (s)
			graphics.device->CreateDepthStencilView(s, temp_addr(s.DepthDesc()), &depth_buffer);
	} else {
		render_buffers[i].clear();
		if (s) {
			graphics.device->CreateRenderTargetView(s, temp_addr(s.RenderDesc()), &render_buffers[i]);
			if (num_render_buffers == 0 || (num_render_buffers == 1 && i == 0))
				SetWindow(rect(zero, s.Size()));
			num_render_buffers	= max(num_render_buffers, i + 1);
		} else {
			while (num_render_buffers && render_buffers[num_render_buffers - 1])
				num_render_buffers--;
		}
	}
	update.set(UPD_TARGETS);
}

void GraphicsContext::SetZBuffer(const Surface& s) {
	depth_buffer.clear();
	if (s)
		graphics.device->CreateDepthStencilView(s, temp_addr(s.DepthDesc()), &depth_buffer);
	update.set(UPD_TARGETS);
}

void GraphicsContext::Clear(param(colour) col, bool zbuffer) {
	if (zbuffer && depth_buffer)
		context->ClearDepthStencilView(depth_buffer, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
	context->ClearRenderTargetView(render_buffers[0], (const float*)&col);
}

void GraphicsContext::ClearZ() {
	context->ClearDepthStencilView(depth_buffer, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
}

void GraphicsContext::SetShader(const DX11Shader &s) {
	if (update.test_clear(UPD_TARGETS))
		FlushTargets();

	if (!s.Has(SS_VERTEX)) {
		if (stage_states[SS_COMPUTE].Init(s.sub[SS_PIXEL].raw()))
			context->CSSetShader(s.sub[SS_PIXEL].get<ID3D11ComputeShader>(), NULL, 0);

	} else {
		if (stage_states[SS_PIXEL].Init(s.sub[SS_PIXEL].raw()))
			context->PSSetShader(s.sub[SS_PIXEL].get<ID3D11PixelShader>(), NULL, 0);

		if (stage_states[SS_VERTEX].Init(s.sub[SS_VERTEX].raw()))
			context->VSSetShader(s.sub[SS_VERTEX].get<ID3D11VertexShader>(), NULL, 0);

		if (stage_states[SS_GEOMETRY].Init(s.sub[SS_GEOMETRY].raw()))
			context->GSSetShader(s.sub[SS_GEOMETRY].get<ID3D11GeometryShader>(), NULL, 0);

		if (stage_states[SS_HULL].Init(s.sub[SS_HULL].raw()))
			context->HSSetShader(s.sub[SS_HULL].get<ID3D11HullShader>(), NULL, 0);

		if (stage_states[SS_LOCAL].Init(s.sub[SS_LOCAL].raw()))
			context->DSSetShader(s.sub[SS_LOCAL].get<ID3D11DomainShader>(), NULL, 0);
	}
}

void GraphicsContext::SetShaderConstants(ShaderReg reg, const void *values) {
	if (!values)
		return;

	int		n		= reg.count;
	switch (reg.type) {
		case SPT_VAL:
			if (stage_states[reg.stage].Get(reg.buffer)->SetConstant(reg.offset, values, n)) {
				update.set(UPDATE(UPD_CBS << reg.stage));
				stage_states[reg.stage].dirty |= 1 << reg.buffer;
			}
			break;
		case SPT_TEXTURE: {
			ID3D11ShaderResourceView	**view	= alloc_auto(ID3D11ShaderResourceView*, n);
			for (int i = 0; i < n; i++)
				view[i] = ((Texture*)values)[i];
			switch (reg.stage) {
				case SS_PIXEL:		context->PSSetShaderResources(reg.offset, n, view); break;
				case SS_VERTEX:		context->VSSetShaderResources(reg.offset, n, view); break;
				case SS_GEOMETRY:	context->GSSetShaderResources(reg.offset, n, view); break;
				case SS_HULL:		context->HSSetShaderResources(reg.offset, n, view); break;
				case SS_LOCAL:		context->DSSetShaderResources(reg.offset, n, view); break;
				case SS_COMPUTE:	context->CSSetShaderResources(reg.offset, n, view); break;
				break;
			}
			break;
		}
		case SPT_SAMPLER: {
		#if 0
			D3D11_SAMPLER_DESC	desc;
			desc.Filter 		= D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU 		= D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV 		= D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW 		= D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.MipLODBias 	= 0;
			desc.MaxAnisotropy 	= 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			desc.BorderColor[0] = 1;
			desc.BorderColor[1] = 1;
			desc.BorderColor[2] = 1;
			desc.BorderColor[3] = 1;
			desc.MinLOD 		= -3.402823466e+38f; // -FLT_MAX
			desc.MaxLOD 		= 3.402823466e+38f; // FLT_MAX

			ID3D11SamplerState 	**samp	= alloc_auto(ID3D11SamplerState *, n);
			for (int i = 0; i < n; i++)
				graphics.device->CreateSamplerState(&desc, &samp[i]);
		#else
			ID3D11SamplerState 	**samp	= alloc_auto(ID3D11SamplerState *, n);
			for (int i = 0; i < n; i++)
				graphics.device->CreateSamplerState(((D3D11_SAMPLER_DESC*)values) + i, &samp[i]);
		#endif
			switch (reg.stage) {
				case SS_PIXEL:		context->PSSetSamplers(reg.offset, n, samp); break;
				case SS_VERTEX:		context->VSSetSamplers(reg.offset, n, samp); break;
				case SS_GEOMETRY:	context->GSSetSamplers(reg.offset, n, samp); break;
				case SS_HULL:		context->HSSetSamplers(reg.offset, n, samp); break;
				case SS_LOCAL:		context->DSSetSamplers(reg.offset, n, samp); break;
				case SS_COMPUTE:	context->CSSetSamplers(reg.offset, n, samp); break;
				break;
			}
			for (int i = 0; i < n; i++) {
				if (samp[i])
					samp[i]->Release();
			}
			break;
		}
		case SPT_BUFFER: {
#if 1
			ID3D11ShaderResourceView	**view	= alloc_auto(ID3D11ShaderResourceView*, n);
			for (int i = 0; i < n; i++)
				view[i] = ((DataBuffer*)values)[i];
#else
			ID3D11ShaderResourceView	*const *view	= (ID3D11ShaderResourceView* const*)values;
#endif
			switch (reg.stage) {
				case SS_PIXEL:		context->PSSetShaderResources(reg.offset, n, view); break;
				case SS_VERTEX:		context->VSSetShaderResources(reg.offset, n, view); break;
				case SS_GEOMETRY:	context->GSSetShaderResources(reg.offset, n, view); break;
				case SS_HULL:		context->HSSetShaderResources(reg.offset, n, view); break;
				case SS_LOCAL:		context->DSSetShaderResources(reg.offset, n, view); break;
				case SS_COMPUTE:	context->CSSetShaderResources(reg.offset, n, view); break;
				break;
			}
			break;
		}
	}
}

void GraphicsContext::MapStreamOut(uint8 b0, uint8 b1, uint8 b2, uint8 b3) {}
void GraphicsContext::SetStreamOut(int i, void *start, uint32 size, uint32 stride) {}
void GraphicsContext::GetStreamOut(int i, uint64 *pos) {}
void GraphicsContext::FlushStreamOut() {}

//-----------------------------------------------------------------------------
//	Graphics::Display
//-----------------------------------------------------------------------------

bool Graphics::Display::SetFormat(const RenderWindow *window, const point &size, TexFormat _format) {
	if (swapchain && width == size.x && height == size.y && format == _format)
		return true;

	graphics.Init();

	disp.clear();

	width		= size.x;
	height		= size.y;
	format		= _format;

	if (!swapchain) {
		com_ptr<IDXGIFactory3> dxgi	= GetDXGIFactory(graphics.Device());

		DXGI_SWAP_CHAIN_DESC1	desc;
		desc.Width				= width;
		desc.Height				= height;
		desc.Format				= (DXGI_FORMAT)format;
		desc.Stereo				= FALSE;
		desc.SampleDesc.Count	= 1;
		desc.SampleDesc.Quality	= 0;
		desc.BufferUsage		= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount		= 3;
		desc.Scaling			= DXGI_SCALING_STRETCH;
		desc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode			= DXGI_ALPHA_MODE_UNSPECIFIED;
		desc.Flags				= 0;

#ifdef PLAT_WINRT
		if (!CheckResult(dxgi->CreateSwapChainForCoreWindow(graphics.Device(), (IUnknown*)window, &desc, nullptr, &swapchain)))
#else
		if (!CheckResult(dxgi->CreateSwapChainForHwnd(graphics.Device(), (HWND)window, &desc, NULL, NULL, &swapchain)))
#endif
			return false;
	} else {
		if (!CheckResult(swapchain->ResizeBuffers(3, width, height, (DXGI_FORMAT)format, 0)))
			return false;
	}

	swapchain->GetBuffer(0, disp.uuid(), (void**)&disp);
	return true;
}

bool Graphics::Display::Present() const {
	bool	lost = swapchain && swapchain->Present(0, 0) == DXGI_ERROR_DEVICE_RESET;
	return true;
}

bool Graphics::Display::Present(const RECT &rect) const {
	DXGI_PRESENT_PARAMETERS	params;
	params.DirtyRectsCount	= 1;
	params.pDirtyRects		= unconst(&rect);
	params.pScrollRect		= 0;
	params.pScrollOffset		= 0;

	bool	lost = swapchain && swapchain->Present1(0, 0, &params) == DXGI_ERROR_DEVICE_RESET;
	return true;
}

//-----------------------------------------------------------------------------
//	ImmediateStream
//-----------------------------------------------------------------------------

void* _ImmediateStream::alloc(int count, uint32 vert_size, uint32 align) {
	if (count) {
		uint32	offset = ctx.immediate_vb.alloc(vert_size * count, align);
		ctx.SetVertices(0, ctx.immediate_vb.buffer, vert_size, offset);

		D3D11_MAPPED_SUBRESOURCE map;
		ctx.Context()->Map(ctx.immediate_vb.buffer, 0, offset == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
		return (char*)map.pData + offset;
	}
	return nullptr;
}


void _ImmediateStream::Draw(int count) {
	if (count == 0)
		return;

	ctx.Context()->Unmap(ctx.immediate_vb.buffer, 0);

	switch (prim) {
		case PRIM_QUADLIST: {
			int		count2	= count * 6 / 4;
			uint32	offset	= ctx.immediate_ib.alloc(count2);

			D3D11_MAPPED_SUBRESOURCE map;
			ctx.Context()->Map(ctx.immediate_ib.buffer, 0, offset == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
			uint16	*d		= (uint16*)map.pData + offset;
			for (int i = 0; i < count; i += 4) {
				*d++ = i + 3;
				*d++ = i + 0;
				*d++ = i + 1;
				*d++ = i + 2;
				*d++ = i + 3;
				*d++ = i + 1;
			}
			ctx.Context()->Unmap(ctx.immediate_ib.buffer, 0);
			ctx.SetIndices(ctx.immediate_ib.buffer);
			ctx.DrawIndexedVertices(PRIM_TRILIST, 0, count, offset, count2);
			break;
		}
		case PRIM_TRIFAN: {
			int		count2	= (count - 2) * 3;
			uint32	offset	= ctx.immediate_ib.alloc(count2);

			D3D11_MAPPED_SUBRESOURCE map;
			ctx.Context()->Map(ctx.immediate_ib.buffer, 0, offset == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
			uint16	*d		= (uint16*)map.pData + offset;
			for (int i = 0; i < count - 2; i++) {
				*d++ = 0;
				*d++ = i + 1;
				*d++ = i + 2;
			}
			ctx.Context()->Unmap(ctx.immediate_ib.buffer, 0);
			ctx.SetIndices(ctx.immediate_ib.buffer);
			ctx.DrawIndexedVertices(PRIM_TRILIST, 0, count, offset, count2);
			break;
		}

		default:
			ctx.DrawVertices(prim, 0, count);
			break;
	}
}

}
