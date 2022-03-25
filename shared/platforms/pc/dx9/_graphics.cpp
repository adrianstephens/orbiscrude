#include "graphics.h"
#include "filetypes\bitmap\bitmap.h"
#include "stream.h"
#include "directx\include\dxerr.h"

namespace iso {

Graphics	graphics;

#ifdef _DEBUG
bool _CheckResult(HRESULT hr, const char *file, uint32 line) {
	if (FAILED(hr))
		throw_accum("D3D error " << DXGetErrorStringA(hr) << "(0x" << xint32(hr) << ") at " << file << " line " << line);
	return true;
}
#endif

float4x4	hardware_fix(param(float4x4) mat)	{ return translate(zero, zero, half) * scale(float3(one,  -one, -half)) * mat; }
float4x4	map_fix(param(float4x4) mat)		{ return translate(half, half, half) * scale(float3(half, half, -half)) * mat;}

//-----------------------------------------------------------------------------
//	Texture/Surface
//-----------------------------------------------------------------------------
bool Surface::Init(TexFormat fmt, int width, int height) {
	bool	zbuffer = fmt >= D3DFMT_D16_LOCKABLE && fmt < D3DFMT_L16;
	return CheckResult(zbuffer
		? graphics.Device()->CreateDepthStencilSurface(width, height, (D3DFORMAT)fmt, D3DMULTISAMPLE_NONE, 0, false, &surf, NULL)
		: graphics.Device()->CreateRenderTarget(width, height, (D3DFORMAT)fmt, D3DMULTISAMPLE_NONE, 0, false, &surf, NULL)
	);
}

#ifndef ISO_EDITOR

template<D3DFORMAT f> struct D3DTexel;
template<> struct D3DTexel<D3DFMT_X8R8G8B8> {
	uint8 b,g,r,x;
	template<typename T2> void operator=(const array_vec<T2,3> &v) { r = v.x; g = v.y; b = v.z; x = 0; }
};
template<> struct D3DTexel<D3DFMT_R8G8B8>	: array_vec<uint8,3> {};

template<> void Init<Texture>(Texture *x, void *physram) {
	if (PCTexture *pc = (PCTexture*)x->get()) {
		_Texture **ptex = x->iso_write();
		if (*ptex) {
			(*ptex)->AddRef();
			return;
		}

		D3DLOCKED_RECT	lr;
		uint8			*srce = (uint8*)physram + pc->offset;
		uint32			bpp	= 0, block = 1;
		switch (pc->format) {
			case D3DFMT_DXT1:			bpp = 4; block = 4; break;
			case D3DFMT_DXT2:
			case D3DFMT_DXT4:			bpp = 8; block = 4; break;
			case D3DFMT_A8:
			case D3DFMT_R3G3B2:
			case D3DFMT_L8:
			case D3DFMT_A4L4:			bpp = 8; break;
			case D3DFMT_R5G6B5:
			case D3DFMT_A1R5G5B5:
			case D3DFMT_A4R4G4B4:
			case D3DFMT_A8R3G3B2:
			case D3DFMT_R16F:
			case D3DFMT_A8L8:
			case D3DFMT_L16:			bpp	= 16; break;
			case D3DFMT_R8G8B8:			bpp = 24; break;
			case D3DFMT_A8R8G8B8:
			case D3DFMT_A2R10G10B10:
			case D3DFMT_G16R16:
			case D3DFMT_G16R16F:
			case D3DFMT_R32F:			bpp	= 32; break;
			case D3DFMT_A16B16G16R16:
			case D3DFMT_A16B16G16R16F:
			case D3DFMT_G32R32F:		bpp = 64; break;
			case D3DFMT_A32B32G32R32F:	bpp = 128; break;
		}
		if (pc->cube) {
			IDirect3DCubeTexture9	*tex;
			if (CheckResult(graphics.Device()->CreateCubeTexture(pc->width, pc->mips, 0, pc->format, D3DPOOL_MANAGED, &tex, NULL))) {
				for (int f = 0; f < 6; f++) {
					for (int level = 0; level < pc->mips; level++) {
						uint32	w			= align(pc->width >> level, block), h = align(pc->height >> level, block);
						uint32	srce_size	= (w * h * bpp) / 8;
						if (CheckResult(tex->LockRect(D3DCUBEMAP_FACES(f), 0, &lr, NULL, 0)))
							memcpy(lr.pBits, srce, srce_size);
						tex->UnlockRect(D3DCUBEMAP_FACES(f), 0);
						srce += srce_size;
					}
				}
				*ptex	= tex;
			}
		} else {
			IDirect3DTexture9		*tex;
			if (FAILED(graphics.Device()->CreateTexture(pc->width, pc->height, pc->mips, 0, pc->format, D3DPOOL_MANAGED, &tex, NULL))) {
				if (pc->format == D3DFMT_R8G8B8 && CheckResult(graphics.Device()->CreateTexture(pc->width, pc->height, pc->mips, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &tex, NULL))) {
					for (int level = 0; level < pc->mips; level++) {
						uint32	w			= align(pc->width >> level, block);
						uint32	h			= align(pc->height >> level, block);
						uint32	line_size	= w * bpp / 8 * block;
						if (CheckResult(tex->LockRect(level, &lr, NULL, 0))) {
							uint8	*dest = (uint8*)lr.pBits;
							for (int y = 0; y < h; y++, srce += line_size, dest += lr.Pitch)
								copy_n((D3DTexel<D3DFMT_R8G8B8>*)srce, (D3DTexel<D3DFMT_X8R8G8B8>*)dest, w);
							tex->UnlockRect(level);
						}
					}
				}
			} else {
				for (int level = 0; level < pc->mips; level++) {
					uint32	w			= align(pc->width >> level, block);
					uint32	h			= align(pc->height >> level, block);
					uint32	line_size	= w * bpp / 8 * block;
					if (CheckResult(tex->LockRect(level, &lr, NULL, 0))) {
						if (lr.Pitch == line_size) {
							uint32	srce_size	= h * line_size / block;
							memcpy(lr.pBits, srce, srce_size);
							srce += srce_size;
						} else {
							uint8	*dest = (uint8*)lr.pBits;
							for (int y = 0; y < h; y++, srce += line_size, dest += lr.Pitch)
								memcpy(dest, srce, line_size);
						}
						tex->UnlockRect(level);
					}
				}
			}
			*ptex	= tex;
		}
		*x->write() = *ptex;
	}
}

int	Texture::Depth() const	{ return 1; }

#else

template<> void Init<Texture>(Texture *x, void *physram) {}
int Texture::Depth() const	{ return ((bitmap*)raw())->Depth(); }

_Texture *_MakeTexture(bitmap *bm) {
	if (!bm)
		return 0;

//	bm->Unpalette();

	int	width	= bm->Width();
	int	height	= bm->Height();
	int	depth	= bm->Depth();

	if (bm->Flags() & BMF_VOLUME) {
		IDirect3DVolumeTexture9		*tex;
		if (!CheckResult(graphics.Device()->CreateVolumeTexture(width, height, depth, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL)))
			return 0;

		D3DLOCKED_BOX			lr;
		if (!CheckResult(tex->LockBox(0, &lr, NULL, 0)))
			return 0;
		copy(
			bm->All3D(),
			make_strided_block((Texel<B8G8R8A8>*)lr.pBits, lr.RowPitch / 4, height / depth, lr.SlicePitch / 4, width, depth)
		);
		tex->UnlockBox(0);
		return static_cast<_Texture*>(static_cast<IDirect3DBaseTexture9*>(tex));

	} else if (depth == 6) {
		IDirect3DCubeTexture9	*tex;
		if (!CheckResult(graphics.Device()->CreateCubeTexture(width, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL)))
			return 0;

		D3DLOCKED_RECT			lr;
		for (int f = 0; f < 6; f++) {
			if (!CheckResult(tex->LockRect(D3DCUBEMAP_FACES(f), 0, &lr, NULL, 0)))
				return 0;
			copy(
				bm->Slice(f),
				make_strided_block((Texel<B8G8R8A8>*)lr.pBits, width, lr.Pitch / 4, width)
			);
			tex->UnlockRect(D3DCUBEMAP_FACES(f), 0);
		}
		return static_cast<_Texture*>(static_cast<IDirect3DBaseTexture9*>(tex));
	} else {
		IDirect3DTexture9		*tex;
		if (!CheckResult(graphics.Device()->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL)))
			return 0;

		D3DLOCKED_RECT			lr;
		if (!CheckResult(tex->LockRect(0, &lr, NULL, 0)))
			return 0;
		copy(
			bm->All(),
			make_strided_block((Texel<B8G8R8A8>*)lr.pBits, width, lr.Pitch / 4, height)
		);
		tex->UnlockRect(0);
		return static_cast<_Texture*>(static_cast<IDirect3DBaseTexture9*>(tex));
	}
	return 0;
}
#endif

Texture MakeTexture(bitmap *bm) {
	Texture	t;
	int	width	= bm->Width();
	int	height	= bm->Height();
	int	depth	= bm->Depth();

	t.Init(TEXF_A8R8G8B8, width, height, depth, 1, MEM_DYNAMIC);
	LockedRect	lr	= t.Data();
	copy(
		bm->All(),
		make_strided_block((Texel<B8G8R8A8>*)lr, width, lr.Pitch / 4, height)
	);
	return t;
}

Texture MakeTexture(HDRbitmap *bm) {
	Texture	t;
	int	width	= bm->Width();
	int	height	= bm->Height();
	int	depth	= bm->Depth();

	t.Init(TEXF_A32B32G32R32F, width, height, depth, 1, MEM_DYNAMIC);
	LockedRect	lr	= t.Data();
	copy(
		bm->All(),
		make_strided_block((HDRpixel*)lr, width, lr.Pitch / 4, height)
	);
	return t;
}

Texture MakeTexture(vbitmap *bm) {
	Texture	t;
	int	width	= bm->Width();
	int	height	= bm->Height();
	int	depth	= bm->Depth();

	TexFormat	fmt;
	switch (bm->format.channels()) {
	case 1:
		fmt	= TEXF_L8;
		break;
	case 2:
		fmt	= TEXF_A8L8;
		break;
	case 3:
		fmt	= TEXF_R8G8B8;
		break;
	case 4:
		fmt	= TEXF_A8R8G8B8;
		break;
	}

	t.Init(fmt, width, height, depth, 1, MEM_DYNAMIC);
	LockedRect	lr	= t.Data();
	vbitmap_loc	loc(*bm);
	loc.get(make_strided_block((Texel<B8G8R8A8>*)lr, width, lr.Pitch / 4, height));
	return t;
}

template<> void DeInit<Texture>(Texture *x) {
	x->~Texture();
}

bool Texture::Init(TexFormat format, int width, int height, int depth, int mips, uint32 flags) {
	uint32	usage	= flags & ~(MEM_SYSTEM | D3DUSAGE_WRITEONLY);
	D3DPOOL	pool	= flags & MEM_SYSTEM ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT;//flags & MEM_DYNAMIC ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;

	if (depth == 1) {
		return CheckResult(graphics.Device()->CreateTexture(width, height, 1, usage, (D3DFORMAT)format, pool, (IDirect3DTexture9**)write(), NULL));
	} else if (depth == 6) {
		return CheckResult(graphics.Device()->CreateCubeTexture(width, 1, usage, (D3DFORMAT)format, pool, (IDirect3DCubeTexture9**)write(), NULL));
	} else {
		return false;
	}
}

void Texture::DeInit() {
	Texture::~Texture();
//	tex = NULL;
}

//-----------------------------------------------------------------------------
//	Graphics
//-----------------------------------------------------------------------------

Graphics::Graphics() : d3d(NULL) {
	ISO::getdef<bitmap>();
}

Graphics::~Graphics() {
//	if (device)
//		device->Release();
	if (d3d)
		d3d->Release();
}

bool Graphics::Init(HWND hWnd, int adapter) {
	if (device)
		return true;

	if (!d3d)
		d3d = Direct3DCreate9(D3D_SDK_VERSION);

	return ReInit(hWnd, adapter);
}

bool Graphics::ReInit(HWND hWnd, int adapter) {
	device.clear();

	while (hWnd) {
		RECT	r;
		::GetClientRect(hWnd, &r);
		if (r.right > 0 && r.bottom > 0)
			break;
		hWnd = ::GetParent(hWnd);
	}

	// Set up the structure used to create the D3DDevice.
	D3DPRESENT_PARAMETERS		pp;
	clear(pp);

	pp.BackBufferFormat			= D3DFMT_UNKNOWN;
	pp.BackBufferCount			= 1;
	pp.SwapEffect				= D3DSWAPEFFECT_COPY;
	pp.Windowed					= TRUE;
	pp.EnableAutoDepthStencil	= FALSE;
	pp.AutoDepthStencilFormat	= D3DFMT_D24S8;

	// Create the Direct3D device.
	return SUCCEEDED(d3d->CreateDevice(adapter, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &pp, &device));
}

int Graphics::FindAdapter(const char *name) {
	for (uint32 a = 0, n = d3d->GetAdapterCount(); a < n; a++) {
		MONITORINFOEX	monitor;
		monitor.cbSize = sizeof(monitor);
		if (GetMonitorInfo(d3d->GetAdapterMonitor(a), &monitor) && monitor.szDevice[0]) {
			DISPLAY_DEVICE	disp;
			clear(disp);
			disp.cb = sizeof(disp);
			if (EnumDisplayDevices(monitor.szDevice, 0, &disp, 0)) {
				if (strstr(disp.DeviceName, name))
					return a;
			}
		}
	}
	return D3DADAPTER_DEFAULT;
}

void Graphics::BeginScene(GraphicsContext &ctx) {
	device->BeginScene();
	ctx.Begin();

	ctx.SetFillMode(FILL_SOLID);
	ctx.SetRenderState(D3DRS_ZENABLE,	D3DZB_TRUE);
	ctx.SetRenderState(D3DRS_ZFUNC,		D3DCMP_GREATEREQUAL);
	ctx.SetRenderState(D3DRS_SRCBLEND,	D3DBLEND_ONE);
	ctx.SetRenderState(D3DRS_DESTBLEND,	D3DBLEND_INVSRCALPHA);
	ctx.Device()->SetDepthStencilSurface(0);

	for (int i = 0; i < 8; i++) {
		ctx.SetSamplerState(i, TS_ADDRESS_U,	D3DTADDRESS_WRAP);
		ctx.SetSamplerState(i, TS_ADDRESS_V,	D3DTADDRESS_WRAP);
		ctx.SetSamplerState(i, TS_FILTER_MAG,		D3DTEXF_LINEAR);
		ctx.SetSamplerState(i, TS_FILTER_MIN,		D3DTEXF_LINEAR);
		ctx.SetSamplerState(i, TS_FILTER_MIP,		D3DTEXF_LINEAR);
	}
}

void Graphics::EndScene(GraphicsContext &ctx) {
//	CheckResult(device->EndScene());
//	device->Present(NULL, NULL, NULL, NULL);
}

//-----------------------------------------------------------------------------
//	GraphicsContext
//-----------------------------------------------------------------------------

void GraphicsContext::SetWindow(const rect &rect) {
	D3DVIEWPORT9	viewport;
	viewport.X		= rect.Left();
	viewport.Y		= rect.Top();
	viewport.Width	= rect.Width();
	viewport.Height	= rect.Height();
	viewport.MinZ	= 0;
	viewport.MaxZ	= 1.0f;
	device->SetViewport(&viewport);
}

rect GraphicsContext::GetWindow() {
	D3DVIEWPORT9	viewport;
	device->GetViewport(&viewport);
	return rect(viewport.X, viewport.Y, viewport.Width, viewport.Height);
}

void GraphicsContext::Clear(param(colour) col, bool zbuffer) {
	uint32	flags = D3DCLEAR_TARGET;
	IDirect3DSurface9 *depth;
	if (zbuffer && SUCCEEDED(device->GetDepthStencilSurface(&depth)) && depth) {
		depth->Release();
		flags |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
	}
	float4 t = col * 255;
	CheckResult(device->Clear(0L, NULL, flags, D3DCOLOR_ARGB(int(t.w), int(t.x), int(t.y), int(t.z)), 0.0f, 0L));
}

void GraphicsContext::ClearZ() {
	CheckResult(graphics.Device()->Clear(0L, NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0L));
}

void GraphicsContext::SetShaderConstants(ShaderReg reg, const void *values) {
	if (!values)
		return;
	uint32	loc = reg.reg, count = reg.count;
	switch (reg.type) {
		case SPT_VBOOL:		device->SetVertexShaderConstantB(loc, (BOOL*)values, count); break;
		case SPT_VINT:		device->SetVertexShaderConstantI(loc, (int*)values, count); break;
		case SPT_VFLOAT:	device->SetVertexShaderConstantF(loc, (float*)values, count); break;
		case SPT_VSAMPLER:
			for (int i = 0; i < count; i++)
				device->SetTexture(D3DVERTEXTEXTURESAMPLER0 + loc + i, ((Texture*)values)[i]);
			break;
		case SPT_PBOOL:		device->SetPixelShaderConstantB(loc, (BOOL*)values, count); break;
		case SPT_PINT:		device->SetPixelShaderConstantI(loc, (int*)values, count); break;
		case SPT_PFLOAT:	device->SetPixelShaderConstantF(loc, (float*)values, count); break;
		case SPT_PSAMPLER:
			for (int i = 0; i < count; i++)
				device->SetTexture(loc + i, ((Texture*)values)[i]);
			break;
	}
}

const void* ShaderParameterIterator::DefaultPerm() const {
	if (cdesc.DefaultValue) {
		void *p = malloc(cdesc.Bytes);
		memcpy(p, cdesc.DefaultValue, cdesc.Bytes);
		return p;
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	Graphics::Display
//-----------------------------------------------------------------------------

bool Graphics::Display::SetSize(HWND hWnd, const point &size) {
	if (width == size.x && height == size.y && disp)
		return true;

	width	= size.x;
	height	= size.y;

	graphics.Init(hWnd);
	disp.clear();

	if (width && height) {
		D3DPRESENT_PARAMETERS pp;
		clear(pp);

		pp.BackBufferWidth			= width;
		pp.BackBufferHeight			= height;
		pp.BackBufferFormat			= D3DFMT_UNKNOWN;
		pp.BackBufferCount			= 1;
		pp.SwapEffect				= D3DSWAPEFFECT_COPY;
		pp.Windowed					= TRUE;
		pp.hDeviceWindow			= hWnd;

		swapchain.clear();
		return	CheckResult(graphics.Device()->CreateAdditionalSwapChain(&pp, &swapchain))
		&&		CheckResult(swapchain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &disp));

	}
	return false;
}

bool Graphics::Display::Present(const RECT *srce_rect, const RECT *dest_rect) const {
	graphics.Device()->EndScene();
	bool	lost = swapchain &&  swapchain->Present(srce_rect, dest_rect, NULL, NULL, 0) == D3DERR_DEVICELOST;
	return true;
}

//-----------------------------------------------------------------------------
//	PixelShader/VertexShader
//-----------------------------------------------------------------------------

void PCShader::Init(void *physram) {
	if (void *raw = vs.raw()) {
		IDirect3DVertexShader9	**w = vs.write();
		if (IDirect3DVertexShader9 *t = *w)
			t->AddRef();
		else
			graphics.Device()->CreateVertexShader((DWORD*)raw, w);
	}
	if (void *raw = ps.raw()) {
		IDirect3DPixelShader9	**w = ps.write();
		if (IDirect3DPixelShader9 *t = *w)
			t->AddRef();
		else
			graphics.Device()->CreatePixelShader((DWORD*)raw, w);
	}
}

template<> void Init<PCShader>(PCShader *x, void *physram) {
	x->Init(physram);
}

template<> void DeInit<PCShader>(PCShader *x) {
	x->vs.clear();
	x->ps.clear();
}

//-----------------------------------------------------------------------------
//	IndexBuffer/VertexBuffer
//-----------------------------------------------------------------------------

bool _VertexBuffer::Create(uint32 size, Memory loc) {
	vb.clear();
	return SUCCEEDED(graphics.Device()->CreateVertexBuffer(size, loc, NULL, D3DPOOL_MANAGED, &vb, NULL));
}

bool _VertexBuffer::Init(void *data, uint32 size, Memory loc) {
	if (data) {
		if (Create(size, loc)) {
			memcpy(Begin(), data, size);
			End();
			return true;
		}
	}
	return false;
}

template<> bool _IndexBuffer::Create<2>(uint32 size, Memory loc) {
	ib.clear();
	return SUCCEEDED(graphics.Device()->CreateIndexBuffer(size, loc, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, NULL));
}
template<> bool _IndexBuffer::Create<4>(uint32 size, Memory loc) {
	ib.clear();
	return SUCCEEDED(graphics.Device()->CreateIndexBuffer(size, loc, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib, NULL));
}

com_ptr<IDirect3DVertexBuffer9>	_ImmediateStream::vb;
IndexBuffer<uint16>				_ImmediateStream::ib;
int								_ImmediateStream::vbi, _ImmediateStream::ibi, _ImmediateStream::vbsize, _ImmediateStream::ibsize;

_ImmediateStream::_ImmediateStream(GraphicsContext &_ctx, PrimType _prim, int _count, int tsize) : ctx(_ctx), prim(_prim), count(_count) {
	int	size = tsize * count;
	if (size * 2 > vbsize) {
		vbsize = size * 3;
		vb.clear();
		CheckResult(graphics.Device()->CreateVertexBuffer(vbsize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, NULL, D3DPOOL_DEFAULT, &vb, NULL));
		vbi = 0;
	}
	if (vbi + size > vbsize) {
		CheckResult(vb->lock(0, size, (void**)&p, D3DLOCK_DISCARD));
		vbi = 0;
	} else {
		CheckResult(vb->lock(vbi, size, (void**)&p, D3DLOCK_NOOVERWRITE));
	}
}

void _ImmediateStream::Draw(IDirect3DVertexDeclaration9 *vd, uint32 tsize) {
	vb->Unlock();
	if (count == 0)
		return;

	graphics.Device()->SetVertexDeclaration(vd);

	if (prim == PRIM_QUADLIST) {
		uint16	*pi, *d;
		int		size = count * 6 / 4;
		if (size * 2 > ibsize) {
			ibsize = size * 3;
			ib.Create(ibsize, MEM_DYNAMIC);
			ibi = 0;
		}
		if (ibi + size > ibsize) {
			CheckResult(ib->lock(0, size, (void**)&pi, D3DLOCK_DISCARD));
			ibi = 0;
		} else {
			CheckResult(ib->lock(ibi, size, (void**)&pi, D3DLOCK_NOOVERWRITE));
		}
		d = pi;
		for (int i = 0; i < count; i += 4) {
			*d++ = i + 3;
			*d++ = i + 0;
			*d++ = i + 1;
			*d++ = i + 2;
			*d++ = i + 3;
			*d++ = i + 1;
		}
		ib->Unlock();
		ctx.SetIndices(ib);
		CheckResult(graphics.Device()->SetStreamSource(0, vb, vbi, tsize));
		CheckResult(graphics.Device()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, count, ibi / sizeof(uint16), count / 2));
		ibi += size;
	} else {
		CheckResult(graphics.Device()->SetStreamSource(0, vb, vbi, tsize));
		ctx.DrawVertices(prim, 0, count);
	}
	vbi += tsize * count;
}

}
