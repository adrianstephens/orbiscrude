#include "hook_com.h"
#include "hook_stack.h"
#include "base/list.h"
#include "dx11_record.h"
#include "dxgi_record.h"
#include "thread.h"

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")

using namespace dx11;

struct linked_recording : e_link<linked_recording>, COMRecording {
	typedef Recording::TYPE	TYPE;
	TYPE	type;
	linked_recording(TYPE _type) : type(_type) {}
};

struct RecItemLink : e_link<RecItemLink>, RecItem {
	Wrap<ID3D11DeviceLatest>	*device;
	RecItemLink() : device(0) {}
	void	init(Wrap<ID3D11DeviceLatest> *_device);
	template<typename...PP> void	init(Wrap<ID3D11DeviceLatest> *_device, PP...pp) {
		init(_device);
	}
};

#define RECORDCALLSTACK			if (recording.enable) RecordCallStack(0)
#define DEVICE_RECORDCALLSTACK	if (device->recording.enable) device->RecordCallStack(0)

//-----------------------------------------------------------------------------
//	Capture
//-----------------------------------------------------------------------------


HRESULT WINAPI Hooked_D3D11CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, CONST D3D_FEATURE_LEVEL *levels, UINT num, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext);
HRESULT WINAPI Hooked_D3D11CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, CONST D3D_FEATURE_LEVEL *levels, UINT num, UINT SDKVersion, CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext);

Win32Handle debug_pipe;

void WINAPI Hooked_OutputDebugStringA(LPCSTR lpOutputString) {
	DWORD      written;
	WriteFile(debug_pipe, lpOutputString, string_len32(lpOutputString), &written, NULL);
}

void WINAPI Hooked_OutputDebugStringW(LPCWSTR lpOutputString) {
	DWORD      written;
	WriteFile(debug_pipe, lpOutputString, string_len32(lpOutputString), &written, NULL);
}

struct Capture {
	int					frames;
	Semaphore			semaphore;
	Event				event;
	e_list<RecDevice>	devices;
	volatile bool		paused, capture;

	Capture() : frames(1), semaphore(0), paused(false), capture(false) {

		//if (debug_pipe = Win32Handle(pipe)) {
		//	hook(OutputDebugStringA, "kernel32.dll");
		//	hook(OutputDebugStringW, "kernel32.dll");
		//}

		HookDXGI();
		hook(D3D11CreateDevice,  "d3d11.dll");
		hook(D3D11CreateDeviceAndSwapChain, "d3d11.dll");
		ApplyHooks();
		socket_init();

		RunThread([this]() {
			Socket listener = IP4::socket_addr(PORT(4567)).listener();
			if (listener.exists()) for (;;) {
				IP4::socket_addr	addr;
				SocketWait			sock = addr.accept(listener);
				switch (sock.getc()) {
					case INTF_Pause:			SocketRPC(sock, [this]() { Pause(); }); break;
					case INTF_Continue:			SocketRPC(sock, [this]() { Continue(); }); break;
					case INTF_CaptureFrames:	SocketRPC(sock, [this](int frames) { return CaptureFrames(frames); }); break;
					case INTF_GetRecordings:	SocketRPC(sock, [this]() { return GetRecordings(); }); break;
					case INTF_GetItems:			SocketRPC(sock, [this]() { return GetItems(); }); break;
					case INTF_ResourceData:		SocketRPC(sock, ResourceData); break;
					case INTF_GetResource:		SocketRPC(sock, GetResource); break;
				}
			}
		});
	}
	void	Pause() {
		frames	= 1;
	}
	void	Continue() {
		event.signal();
	}

	CaptureStats							CaptureFrames(int frames);
	with_size<dynamic_array<Recording>>		GetRecordings();
	with_size<dynamic_array<RecItem2>>		GetItems();
	static malloc_block_all					ResourceData(uintptr_t _res);
	static RecItem2							GetResource(uintptr_t _view);

	void	Set(int _frames) {
		com_wrap_system->defer_deletes(_frames);
		if (paused) {
			frames = _frames;
			capture = true;
			Continue();
		} else {
			frames = _frames + 1;
			capture = true;
		}
		semaphore.lock();
	}

	bool	Update() {
		bool	ret = frames > 1;
		if (frames && !--frames) {
			if (capture) {
				capture = false;
				semaphore.unlock();
			}
			paused	= true;
			event.wait();
			paused	= false;
			ret		= frames > 0;
			if (!ret)
				com_wrap_system->defer_deletes(0);
		}
		com_wrap_system->end_frame();
		return ret;
	}
};

static Capture capture;

//-----------------------------------------------------------------------------
//	Wraps
//-----------------------------------------------------------------------------

template<class T> class Wrap2<T, ID3D11DeviceChild> : public com_wrap<T>, public RecItemLink {
public:
	//IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D11DeviceChild>(this, riid, pp) ? S_OK : com_wrap<T>::QueryInterface(riid, pp);
	}
	//ID3D11DeviceChild
	void	STDMETHODCALLTYPE GetDevice(ID3D11Device **pp)									{ orig->GetDevice(pp); com_wrap_system->rewrap(pp); }
	HRESULT	STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *size, void *data)			{ return orig->GetPrivateData(guid, size, data); }
	HRESULT	STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT size, const void *data)		{
		if (guid == WKPDID_D3DDebugObjectName)
			name = count_string((const char*)data, size);
		return orig->SetPrivateData(guid, size, data);
	}
	HRESULT	STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *data)	{ return orig->SetPrivateDataInterface(guid, data); }
};
template<> class Wrap<ID3D11DeviceChild> : public Wrap2<ID3D11DeviceChild, ID3D11DeviceChild> {};

template<> class Wrap<ID3D11DepthStencilState> : public Wrap2<ID3D11DepthStencilState, ID3D11DeviceChild> {
public:
	D3D11_DEPTH_STENCIL_DESC	desc;
	Wrap() { type = DepthStencilState; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_DEPTH_STENCIL_DESC *_desc)	{ RecItemLink::init(device); desc = *_desc; }
	void	STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_DESC *desc)					{ orig->GetDesc(desc); }
};

void assign(D3D11_RENDER_TARGET_BLEND_DESC1 &d, const D3D11_RENDER_TARGET_BLEND_DESC &s) {
	d.BlendEnable				= s.BlendEnable;
	d.LogicOpEnable				= FALSE;
	d.SrcBlend					= s.SrcBlend;
	d.DestBlend					= s.DestBlend;
	d.BlendOp					= s.BlendOp;
	d.SrcBlendAlpha				= s.SrcBlendAlpha;
	d.DestBlendAlpha			= s.DestBlendAlpha;
	d.BlendOpAlpha				= s.BlendOpAlpha;
	d.LogicOp					= D3D11_LOGIC_OP_CLEAR;
	d.RenderTargetWriteMask		= s.RenderTargetWriteMask;
}

void assign(D3D11_BLEND_DESC1 &d, const D3D11_BLEND_DESC &s) {
	d.AlphaToCoverageEnable		= s.AlphaToCoverageEnable;
	d.IndependentBlendEnable	= s.IndependentBlendEnable;
	for (int i = 0; i < 8; i++)
		assign(d.RenderTarget[i], s.RenderTarget[i]);
}

template<> class Wrap<ID3D11BlendStateLatest> : public Wrap2<ID3D11BlendStateLatest, ID3D11DeviceChild> {
public:
	D3D11_BLEND_DESC1	desc;
	Wrap() { type = BlendState; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_BLEND_DESC1 *_desc)	{ RecItemLink::init(device); desc = *_desc; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_BLEND_DESC *_desc)	{ RecItemLink::init(device); assign(desc, *_desc); }
	void	STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC *desc)					{ orig->GetDesc(desc); }
	void	STDMETHODCALLTYPE GetDesc1(D3D11_BLEND_DESC1 *desc)					{ orig->GetDesc1(desc); }
};

void assign(D3D11_RASTERIZER_DESC1 &d, const D3D11_RASTERIZER_DESC &s) {
	d.FillMode					= s.FillMode;
	d.CullMode					= s.CullMode;
	d.FrontCounterClockwise		= s.FrontCounterClockwise;
	d.DepthBias					= s.DepthBias;
	d.DepthBiasClamp			= s.DepthBiasClamp;
	d.SlopeScaledDepthBias		= s.SlopeScaledDepthBias;
	d.DepthClipEnable			= s.DepthClipEnable;
	d.ScissorEnable				= s.ScissorEnable;
	d.MultisampleEnable			= s.MultisampleEnable;
	d.AntialiasedLineEnable		= s.AntialiasedLineEnable;
	d.ForcedSampleCount			= 0;
}

template<> class Wrap<ID3D11RasterizerStateLatest> : public Wrap2<ID3D11RasterizerStateLatest, ID3D11DeviceChild> {
public:
	D3D11_RASTERIZER_DESC1	desc;
	Wrap() { type = RasterizerState; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_RASTERIZER_DESC1 *_desc)	{ RecItemLink::init(device); desc = *_desc; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_RASTERIZER_DESC *_desc)	{ RecItemLink::init(device); assign(desc, *_desc); }
	void	STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC *desc)					{ orig->GetDesc(desc); }
	//ID3D11RasterizerState1
	void	STDMETHODCALLTYPE GetDesc1(D3D11_RASTERIZER_DESC1 *desc)				{ orig->GetDesc1(desc); }
	//ID3D11RasterizerStateLatest
	void	STDMETHODCALLTYPE GetDesc2(D3D11_RASTERIZER_DESC2 *desc)				{ orig->GetDesc2(desc); }
};

template<> class Wrap<ID3D11Fence> : public Wrap2<ID3D11Fence, ID3D11DeviceChild> {
public:
	Wrap() { type = Fence; }
	void	init(Wrap<ID3D11DeviceLatest> *device, HANDLE hFence)								{ RecItemLink::init(device); }
	void	init(Wrap<ID3D11DeviceLatest> *device, UINT64 InitialValue, D3D11_FENCE_FLAG Flags)	{ RecItemLink::init(device); }
	HRESULT	STDMETHODCALLTYPE CreateSharedHandle(const SECURITY_ATTRIBUTES *pAttributes, DWORD dwAccess, LPCWSTR lpName, HANDLE *pHandle) { return orig->CreateSharedHandle(pAttributes, dwAccess, lpName, pHandle); }
	UINT64	STDMETHODCALLTYPE GetCompletedValue()									{ return orig->GetCompletedValue(); }
	HRESULT	STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value, HANDLE hEvent)		{ return orig->SetEventOnCompletion(Value, hEvent); }
};

template<class T> class Wrap2<T, ID3D11Resource> : public Wrap2<T, ID3D11DeviceChild> {
public:
	malloc_block	data;

	//void	init(Wrap<ID3D11DeviceLatest> *device, malloc_block &&_data) {
	//	RecItemLink::init(device);
	//	data	= move(_data);
	//}
	template<typename D> void init(Wrap<ID3D11DeviceLatest> *device, const D *desc, const D3D11_SUBRESOURCE_DATA *sub) {
		RecItemLink::init(device);
		data	= const_memory_block(desc);
	}
	void	init(Wrap<ID3D11DeviceLatest> *_device/*, malloc_block&& _data*/) {
		device	= _device;
		//data	= move(_data);
	}

	//IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D11Resource>(this, riid, pp) ? S_OK : Wrap2<T, ID3D11DeviceChild>::QueryInterface(riid, pp);
	}
	//ID3D11Resource
	void	STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *dim)			{ orig->GetType(dim); }
	void	STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority)		{ orig->SetEvictionPriority(EvictionPriority); }
	UINT	STDMETHODCALLTYPE GetEvictionPriority()								{ return orig->GetEvictionPriority(); }
};

template<> class Wrap<ID3D11Resource> : public Wrap2<ID3D11Resource, ID3D11Resource> {
public:
	DXGI_FORMAT	GetFormat()	{
		switch (type) {
			case Texture1D:	return dx11::GetFormat((D3D11_TEXTURE1D_DESC*)data);
			case Texture2D:	return dx11::GetFormat((D3D11_TEXTURE2D_DESC*)data);
			case Texture3D:	return dx11::GetFormat((D3D11_TEXTURE3D_DESC*)data);
			default:		return DXGI_FORMAT_UNKNOWN;
		}
	}

	memory_cube GetMemoryCube(UINT sub, const D3D11_BOX *box, const void *srce, UINT row_pitch, UINT depth_pitch) {
		uint32	width, rows = 1, depth = 1;
		if (box) {
			width	= stride(GetFormat(), box->right - box->left);
			rows	= box->bottom - box->top;
			depth	= box->back - box->front;
		} else switch (type) {
			case RecItem::Buffer:		width = ((const D3D11_BUFFER_DESC*)data)->ByteWidth; break;
			case RecItem::Texture1D:	width = GetSubResourceDims((D3D11_TEXTURE1D_DESC*)data, sub, rows); break;
			case RecItem::Texture2D:	width = GetSubResourceDims((D3D11_TEXTURE2D_DESC*)data, sub, rows, depth); break;
			case RecItem::Texture3D:	width = GetSubResourceDims((D3D11_TEXTURE3D_DESC*)data, sub, rows, depth); break;
		}
		return memory_cube(unconst(srce), width, rows, depth, row_pitch, depth_pitch);
	}
};

memory_cube GetMemoryCube(ID3D11Resource *rsrc, UINT sub, const D3D11_BOX *box, const void *srce, UINT row_pitch, UINT depth_pitch) {
	return com_wrap_system->get_wrap(rsrc)->GetMemoryCube(sub, box, srce, row_pitch, depth_pitch);
}

template<> class Wrap<ID3D11Buffer> : public Wrap2<ID3D11Buffer, ID3D11Resource> {
public:
	Wrap() { type = Buffer; }
	void	STDMETHODCALLTYPE GetDesc(D3D11_BUFFER_DESC *desc)					{ orig->GetDesc(desc); }
};

template<> class Wrap<ID3D11Texture1D> : public Wrap2<ID3D11Texture1D, ID3D11Resource> {
public:
	Wrap() { type = Texture1D; }
	void	STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE1D_DESC *desc)				{ orig->GetDesc(desc); }
};

template<> class Wrap<ID3D11Texture2DLatest> : public Wrap2<ID3D11Texture2DLatest, ID3D11Resource> {
public:
	Wrap() { type = Texture2D; }
	void	STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE2D_DESC *desc)				{ orig->GetDesc(desc); }
	//ID3D11Texture2D1
	void	STDMETHODCALLTYPE GetDesc1(D3D11_TEXTURE2D_DESC1 *desc)				{ orig->GetDesc1(desc); }
};

template<> class Wrap<ID3D11Texture3DLatest> : public Wrap2<ID3D11Texture3DLatest, ID3D11Resource> {
public:
	Wrap() { type = Texture3D; }
	void	STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE3D_DESC *desc)				{ orig->GetDesc(desc); }
	//ID3D11Texture3D1
	void	STDMETHODCALLTYPE GetDesc1(D3D11_TEXTURE3D_DESC1 *desc)				{ orig->GetDesc1(desc); }
};



ID3D11Resource* make_wrap_orphan(ID3D11Resource* p) {
	D3D11_RESOURCE_DIMENSION	dim;
	com_ptr<ID3D11Device>		dev;

	p->GetType(&dim);
	p->GetDevice(&dev);

	auto	*wdev = com_wrap_system->find_wrap_carefully(dev.get());

#if 1
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			return com_wrap_system->make_wrap((ID3D11Buffer*)p, wdev);
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			return com_wrap_system->make_wrap((ID3D11Texture1D*)p);
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			return com_wrap_system->make_wrap((ID3D11Texture2D*)p);
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			return com_wrap_system->make_wrap((ID3D11Texture3D*)p);
		default:
			return com_wrap_system->make_wrap((ID3D11Resource*)p);
	}
#else
	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER: {
			type = Buffer;
			D3D11_BUFFER_DESC		desc;
			((ID3D11Buffer*)orig)->GetDesc(&desc);
			init(wdev, &desc, 0);
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			type = Texture1D;
			D3D11_TEXTURE1D_DESC	desc; 
			((ID3D11Texture1D*)orig)->GetDesc(&desc);
			init(wdev, &desc, 0);
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			type = Texture2D;
			D3D11_TEXTURE2D_DESC	desc;
			((ID3D11Texture2D*)orig)->GetDesc(&desc);
			init(wdev, &desc, 0);
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			type = Texture3D;
			D3D11_TEXTURE3D_DESC	desc;
			((ID3D11Texture3D*)orig)->GetDesc(&desc);
			init(wdev, &desc, 0);
			break;
		}
	}
#endif
}

template<class T> class Wrap2<T, ID3D11View> : public Wrap2<T, ID3D11DeviceChild> {
public:
	typedef typename DX11ViewDesc<T>::type	DESC;

	RecView<DESC>	info;

	void	init(Wrap<ID3D11DeviceLatest> *device, ID3D11Resource *_resource, const DESC *desc) {
		Wrap2<T, ID3D11DeviceChild>::init(device);
		info.init(_resource, desc);
	}
	//IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D11View>(this, riid, pp) ? S_OK : Wrap2<T, ID3D11DeviceChild>::QueryInterface(riid, pp);
	}
	//ID3D11View
	void	STDMETHODCALLTYPE GetResource(ID3D11Resource **pp)	{
#if 1		// info.resource might've been freed
		orig->GetResource(pp);
		com_wrap_system->rewrap_carefully(pp);
#else
		*pp = info.resource;
		info.resource->AddRef();
#endif
	}

	//specific
	void	STDMETHODCALLTYPE GetDesc(DESC *desc)				{ orig->GetDesc(desc); }
};

template<> class Wrap<ID3D11View> : public Wrap2<ID3D11View, ID3D11DeviceChild> {
};

template<> class Wrap<ID3D11ShaderResourceViewLatest> : public Wrap2<ID3D11ShaderResourceViewLatest, ID3D11View> {
public:
	Wrap() { type = ShaderResourceView; }
	//ID3D11ShaderResourceView1
	void	STDMETHODCALLTYPE GetDesc1(D3D11_SHADER_RESOURCE_VIEW_DESC1 *desc)	{ orig->GetDesc1(desc); }
};

template<> class Wrap<ID3D11RenderTargetViewLatest> : public Wrap2<ID3D11RenderTargetViewLatest, ID3D11View> {
public:
	Wrap() { type = RenderTargetView; }
	//ID3D11RenderTargetView1
	void	STDMETHODCALLTYPE GetDesc1(D3D11_RENDER_TARGET_VIEW_DESC1 *desc)	{ orig->GetDesc1(desc); }
};

template<> class Wrap<ID3D11DepthStencilView> : public Wrap2<ID3D11DepthStencilView, ID3D11View> {
public:
	Wrap() { type = DepthStencilView; }
};

template<> class Wrap<ID3D11UnorderedAccessViewLatest> : public Wrap2<ID3D11UnorderedAccessViewLatest, ID3D11View> {
public:
	Wrap() { type = UnorderedAccessView; }
	//ID3D11UnorderedAccessView1
	void	STDMETHODCALLTYPE GetDesc1(D3D11_UNORDERED_ACCESS_VIEW_DESC1 *desc)	{ orig->GetDesc1(desc); }
};

template<> struct iso::unwrapped<ID3D11View*> {
	ID3D11View*		t;
	void	operator=(ID3D11View* _t) {
		t = com_wrap_system->unwrap_safe(_t);
/*			auto	p2 = static_cast<Wrap2<ID3D11RenderTargetView, ID3D11View>*>(_t);
			switch (p2->type) {
				case RecItem::ShaderResourceView:	t = com_wrap_base::UnWrapCarefully((ID3D11ShaderResourceView*)_t); return;
				case RecItem::RenderTargetView:		t = com_wrap_base::UnWrapCarefully((ID3D11RenderTargetView*)_t); return;
				case RecItem::DepthStencilView:		t = com_wrap_base::UnWrapCarefully((ID3D11DepthStencilView*)_t); return;
				case RecItem::UnorderedAccessView:	t = com_wrap_base::UnWrapCarefully((ID3D11UnorderedAccessView*)_t); return;
			}
		}
		t = com_wrap_base::UnWrapCarefully((ID3D11RenderTargetView*)_t);
		*/
	}
};

struct D3D11Blob {
	malloc_block blob;
};

template<class T> class Wrap2<T, D3D11Blob> : public Wrap2<T, ID3D11DeviceChild>, public D3D11Blob {
public:
	void	init(Wrap<ID3D11DeviceLatest> *device, const memory_block &_blob) {
		this->blob = _blob;
		RecItemLink::init(device);
	}
	void	init(Wrap<ID3D11DeviceLatest> *device, const void *code, SIZE_T length, ID3D11ClassLinkage *link) {
		this->blob = memory_block(unconst(code), length);
		RecItemLink::init(device);
	}
	void	init(Wrap<ID3D11DeviceLatest> *device, const void *code, SIZE_T length, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *link) {
		this->blob = memory_block(unconst(code), length);
		RecItemLink::init(device);
	}
};

template<> class Wrap<ID3D11VertexShader>	: public Wrap2<ID3D11VertexShader,	D3D11Blob> { public: Wrap() { type = VertexShader;		} };
template<> class Wrap<ID3D11HullShader>		: public Wrap2<ID3D11HullShader,	D3D11Blob> { public: Wrap() { type = HullShader;		} };
template<> class Wrap<ID3D11DomainShader>	: public Wrap2<ID3D11DomainShader,	D3D11Blob> { public: Wrap() { type = DomainShader;		} };
template<> class Wrap<ID3D11GeometryShader> : public Wrap2<ID3D11GeometryShader,D3D11Blob> { public: Wrap() { type = GeometryShader;	} };
template<> class Wrap<ID3D11PixelShader>	: public Wrap2<ID3D11PixelShader,	D3D11Blob> { public: Wrap() { type = PixelShader;		} };
template<> class Wrap<ID3D11ComputeShader>	: public Wrap2<ID3D11ComputeShader, D3D11Blob> { public: Wrap() { type = ComputeShader;		} };

template<> class Wrap<ID3D11InputLayout>	: public Wrap2<ID3D11InputLayout,	D3D11Blob> {
	public: Wrap() { type = InputLayout; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_INPUT_ELEMENT_DESC *desc, UINT num, const void *code, SIZE_T length) {
		RecItemLink::init(device);
		blob = save_params<RTM>(make_counted<1>(desc), num, make_memory_block<3>(code), length).detach();
	}
};

template<> class Wrap<ID3D11SamplerState> : public Wrap2<ID3D11SamplerState, ID3D11DeviceChild> {
public:
	D3D11_SAMPLER_DESC	desc;
	Wrap() { type = SamplerState; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_SAMPLER_DESC *_desc)	{ RecItemLink::init(device); desc = *_desc; }
	void	STDMETHODCALLTYPE GetDesc(D3D11_SAMPLER_DESC *desc)					{ orig->GetDesc(desc); }
};

template<class T> class Wrap2<T, ID3D11Asynchronous> : public Wrap2<T, ID3D11DeviceChild> {
public:
	//IUnknown
	HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D11Asynchronous>(this, riid, pp) ? S_OK : Wrap2<T, ID3D11DeviceChild>::QueryInterface(riid, pp);
	}
	//ID3D11Asynchronous
	UINT	STDMETHODCALLTYPE GetDataSize()										{ return orig->GetDataSize(); }
};

template<> class Wrap<ID3D11Asynchronous> : public Wrap2<ID3D11Asynchronous, ID3D11Asynchronous> {};

template<class T> class Wrap2<T, ID3D11Query> : public Wrap2<T, ID3D11Asynchronous> {
public:
	D3D11_QUERY_DESC	desc;
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_QUERY_DESC *_desc)	{ RecItemLink::init(device); desc = *_desc; }

	//IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interface<ID3D11Query>(this, riid, pp) ? S_OK : Wrap2<T, ID3D11Asynchronous>::QueryInterface(riid, pp);
	}
	//ID3D11Query
	void	STDMETHODCALLTYPE GetDesc(D3D11_QUERY_DESC *desc)					{ orig->GetDesc(desc); }
        
};

template<> class Wrap<ID3D11QueryLatest> : public Wrap2<ID3D11QueryLatest, ID3D11Query> {
public:
	Wrap() { type = Query; }
	//ID3D11QueryLatest
	void	STDMETHODCALLTYPE GetDesc1(D3D11_QUERY_DESC1 *desc)					{ orig->GetDesc1(desc); }
};

template<> class Wrap<ID3D11Predicate> : public Wrap2<ID3D11Predicate, ID3D11Query> {
public:
	Wrap() { type = Predicate; }
};

template<> class Wrap<ID3D11Counter> : public Wrap2<ID3D11Counter, ID3D11Asynchronous> {
public:
	D3D11_COUNTER_DESC	desc;
	Wrap() { type = Counter; }
	void	init(Wrap<ID3D11DeviceLatest> *device, const D3D11_COUNTER_DESC *_desc)	{ RecItemLink::init(device); desc = *_desc; }
	void	STDMETHODCALLTYPE GetDesc(D3D11_COUNTER_DESC *desc)					{ orig->GetDesc(desc); }
};

template<> class Wrap<ID3D11ClassInstance> : public Wrap2<ID3D11ClassInstance, ID3D11DeviceChild> {
public:
	Wrap() { type = ClassInstance; }
	void	STDMETHODCALLTYPE GetClassLinkage(ID3D11ClassLinkage **pp)			{ orig->GetClassLinkage(pp); }
	void	STDMETHODCALLTYPE GetDesc(D3D11_CLASS_INSTANCE_DESC *desc)			{ orig->GetDesc(desc); }
	void	STDMETHODCALLTYPE GetInstanceName(LPSTR name, SIZE_T *length)		{ orig->GetInstanceName(name, length);}
	void	STDMETHODCALLTYPE GetTypeName(LPSTR name, SIZE_T *length)			{ orig->GetTypeName(name, length); }
};

template<> class Wrap<ID3D11ClassLinkage> : public Wrap2<ID3D11ClassLinkage, ID3D11DeviceChild> {
public:
	Wrap() { type = ClassLinkage; }
	HRESULT	STDMETHODCALLTYPE GetClassInstance(LPCSTR name, UINT index, ID3D11ClassInstance **pp) {
		return orig->GetClassInstance(name, index, pp);
	}
	HRESULT	STDMETHODCALLTYPE CreateClassInstance(LPCSTR name, UINT ConstantBufferOffset, UINT ConstantVectorOffset, UINT TextureOffset, UINT SamplerOffset, ID3D11ClassInstance **pp) {
		return orig->CreateClassInstance(name, ConstantBufferOffset, ConstantVectorOffset, TextureOffset, SamplerOffset, pp);
	}
};

template<> class Wrap<ID3D11CommandList> : public Wrap2<ID3D11CommandList, ID3D11DeviceChild> {
public:
	Wrap() { type = CommandList; }
	UINT	STDMETHODCALLTYPE GetContextFlags()									{ return orig->GetContextFlags(); }
};

template<> class Wrap<ID3DDeviceContextState> : public Wrap2<ID3DDeviceContextState, ID3D11DeviceChild> {
public:
	using RecItemLink::init;
	Wrap() { type = DeviceContextState; }
	void	init(Wrap<ID3D11DeviceContextLatest> *ctx, ID3DDeviceContextState *next);
};

struct DeviceContextState {
	struct Stage {
		com_ptr2<ID3D11Buffer>				cb[8];
		com_ptr2<ID3D11ShaderResourceView>	srv[8];
		com_ptr2<ID3D11SamplerState>		smp[8];

		void	clear();
		void	SetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp);
		void	SetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp);
		void	SetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp);
	} vs, ps, ds, hs, gs, cs;

	com_ptr2<ID3D11InputLayout>			ia;

	void ClearAll();
};

void DeviceContextState::Stage::clear() {
	for (auto &i : cb)
		i.clear();
	for (auto &i : srv)
		i.clear();
	for (auto &i : smp)
		i.clear();
}

void DeviceContextState::ClearAll() {
	vs.clear();
	ps.clear();
	ds.clear();
	hs.clear();
	gs.clear();
	cs.clear();
	ia.clear();
}

void DeviceContextState::Stage::SetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp) {
	for (auto *i = cb + slot, *e = min(i + num, end(cb)); i < e; ++i, ++pp)
		*i = *pp;
}

void DeviceContextState::Stage::SetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp) {
	for (auto *i = srv + slot, *e = min(i + num, end(srv)); i < e; ++i, ++pp)
		*i = *pp;
}

void DeviceContextState::Stage::SetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp) {
	for (auto *i = smp + slot, *e = min(i + num, end(smp)); i < e; ++i, ++pp)
		*i = *pp;
}

template<> class Wrap<ID3D11DeviceContextLatest> : public Wrap2<ID3D11DeviceContextLatest, ID3D11DeviceChild>, DeviceContextState, public RecDeviceContext {
	com_ptr2<ID3DDeviceContextState>	dcs;

	linked_recording	recording;
public:
	struct map_entry : e_link<map_entry> {
		ID3D11Resource				*rsrc;
		uint32						sub;
		D3D11_MAP					type;
		void						*data;
		memory_cube					cube;

		map_entry(ID3D11Resource *_rsrc, uint32 _sub, D3D11_MAP _type, D3D11_MAPPED_SUBRESOURCE *mapped)
			: rsrc(_rsrc), sub(_sub), type(_type), data(0)
			, cube(GetMemoryCube(rsrc, sub, 0, mapped->pData, mapped->RowPitch, mapped->DepthPitch))
		{
			switch (type) {
				case D3D11_MAP_READ:
				case D3D11_MAP_READ_WRITE:
					 break;
				case D3D11_MAP_WRITE:
				case D3D11_MAP_WRITE_DISCARD:
				case D3D11_MAP_WRITE_NO_OVERWRITE: {
					size_t	size	= cube.strided_size();
					data			= mapped->pData;
					mapped->pData	= cube.data = malloc(size);
					memset(mapped->pData, 0xcd, size);
					break;
				}
			}
		}
		~map_entry() {
			if (data)
				free(cube.data);
		}
	};
	e_list<map_entry>	map_list;

	Wrap() : recording(Recording::CONTEXT) { type = DeviceContext; }

	void	set_recording(bool enable);
	void	RecordCallStack(const context &ctx);

	//IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		return check_interfaces<ID3D11DeviceContext,ID3D11DeviceContext1,ID3D11DeviceContext2,ID3D11DeviceContext3,ID3D11DeviceContextLatest>(this, riid, pp)
			? S_OK
			: Wrap2<ID3D11DeviceContextLatest, ID3D11DeviceChild>::QueryInterface(riid, pp);
	}
	//ID3D11DeviceContext
	void	STDMETHODCALLTYPE VSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp);
	void	STDMETHODCALLTYPE PSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp);
	void	STDMETHODCALLTYPE PSSetShader(ID3D11PixelShader *shader, ID3D11ClassInstance *const *pp, UINT numc);
	void	STDMETHODCALLTYPE PSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp);
	void	STDMETHODCALLTYPE VSSetShader(ID3D11VertexShader *shader, ID3D11ClassInstance *const *pp, UINT numc);
	void	STDMETHODCALLTYPE DrawIndexed(UINT IndexCount, UINT StartIndex, INT BaseVertex);
	void	STDMETHODCALLTYPE Draw(UINT VertexCount, UINT StartVertex);
	HRESULT	STDMETHODCALLTYPE Map(ID3D11Resource *rsrc, UINT sub, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *mapped);
	void	STDMETHODCALLTYPE Unmap(ID3D11Resource *rsrc, UINT sub);
	void	STDMETHODCALLTYPE PSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp);
	void	STDMETHODCALLTYPE IASetInputLayout(ID3D11InputLayout *pInputLayout);
	void	STDMETHODCALLTYPE IASetVertexBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *strides, const UINT *pOffsets);
	void	STDMETHODCALLTYPE IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset);
	void	STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndex, INT BaseVertex, UINT StartInstance);
	void	STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertex, UINT StartInstance);
	void	STDMETHODCALLTYPE GSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp);
	void	STDMETHODCALLTYPE GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *pp, UINT numc);
	void	STDMETHODCALLTYPE IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);
	void	STDMETHODCALLTYPE VSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp);
	void	STDMETHODCALLTYPE VSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp);
	void	STDMETHODCALLTYPE Begin(ID3D11Asynchronous *async);
	void	STDMETHODCALLTYPE End(ID3D11Asynchronous *async);
	HRESULT	STDMETHODCALLTYPE GetData(ID3D11Asynchronous *async, void *data, UINT size, UINT GetDataFlags);
	void	STDMETHODCALLTYPE SetPredication(ID3D11Predicate *pred, BOOL PredicateValue);
	void	STDMETHODCALLTYPE GSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp);
	void	STDMETHODCALLTYPE GSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp);
	void	STDMETHODCALLTYPE OMSetRenderTargets(UINT num, ID3D11RenderTargetView *const *ppRTV, ID3D11DepthStencilView *dsv);
	void	STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView *const *ppRTV, ID3D11DepthStencilView *dsv, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUAV, const UINT *pUAVInitialCounts);
	void	STDMETHODCALLTYPE OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask);
	void	STDMETHODCALLTYPE OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef);
	void	STDMETHODCALLTYPE SOSetTargets(UINT num, ID3D11Buffer *const *pp, const UINT *pOffsets);
	void	STDMETHODCALLTYPE DrawAuto();
	void	STDMETHODCALLTYPE DrawIndexedInstancedIndirect(ID3D11Buffer *buffer, UINT AlignedByteOffsetForArgs);
	void	STDMETHODCALLTYPE DrawInstancedIndirect(ID3D11Buffer *buffer, UINT AlignedByteOffsetForArgs);
	void	STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
	void	STDMETHODCALLTYPE DispatchIndirect(ID3D11Buffer *buffer, UINT AlignedByteOffsetForArgs);
	void	STDMETHODCALLTYPE RSSetState(ID3D11RasterizerState *pRasterizerState);
	void	STDMETHODCALLTYPE RSSetViewports(UINT num, const D3D11_VIEWPORT *pViewports);
	void	STDMETHODCALLTYPE RSSetScissorRects(UINT num, const D3D11_RECT *pRects);
	void	STDMETHODCALLTYPE CopySubresourceRegion(ID3D11Resource *dst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *src, UINT SrcSubresource, const D3D11_BOX *pSrcBox);
	void	STDMETHODCALLTYPE CopyResource(ID3D11Resource *dst, ID3D11Resource *src);
	void	STDMETHODCALLTYPE UpdateSubresource(ID3D11Resource *dst, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
	void	STDMETHODCALLTYPE CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView);
	void	STDMETHODCALLTYPE ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]);
	void	STDMETHODCALLTYPE ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *uav, const UINT Values[4]);
	void	STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *uav, const FLOAT Values[4]);
	void	STDMETHODCALLTYPE ClearDepthStencilView(ID3D11DepthStencilView *dsv, UINT ClearFlags, FLOAT Depth, UINT8 Stencil);
	void	STDMETHODCALLTYPE GenerateMips(ID3D11ShaderResourceView *pShaderResourceView);
	void	STDMETHODCALLTYPE SetResourceMinLOD(ID3D11Resource *rsrc, FLOAT MinLOD);
	FLOAT	STDMETHODCALLTYPE GetResourceMinLOD(ID3D11Resource *rsrc);
	void	STDMETHODCALLTYPE ResolveSubresource(ID3D11Resource *dst, UINT DstSubresource, ID3D11Resource *src, UINT SrcSubresource, DXGI_FORMAT Format);
	void	STDMETHODCALLTYPE ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState);
	void	STDMETHODCALLTYPE HSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp);
	void	STDMETHODCALLTYPE HSSetShader(ID3D11HullShader *shader, ID3D11ClassInstance *const *pp, UINT numc);
	void	STDMETHODCALLTYPE HSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp);
	void	STDMETHODCALLTYPE HSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp);
	void	STDMETHODCALLTYPE DSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp);
	void	STDMETHODCALLTYPE DSSetShader(ID3D11DomainShader *shader, ID3D11ClassInstance *const *pp, UINT numc);
	void	STDMETHODCALLTYPE DSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp);
	void	STDMETHODCALLTYPE DSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp);
	void	STDMETHODCALLTYPE CSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp);
	void	STDMETHODCALLTYPE CSSetUnorderedAccessViews(UINT slot, UINT num, ID3D11UnorderedAccessView *const *pp, const UINT *pUAVInitialCounts);
	void	STDMETHODCALLTYPE CSSetShader(ID3D11ComputeShader *shader, ID3D11ClassInstance *const *pp, UINT numc);
	void	STDMETHODCALLTYPE CSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp);
	void	STDMETHODCALLTYPE CSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp);
	void	STDMETHODCALLTYPE VSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp);
	void	STDMETHODCALLTYPE PSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp);
	void	STDMETHODCALLTYPE PSGetShader(ID3D11PixelShader **pps, ID3D11ClassInstance **ppc, UINT *numc);
	void	STDMETHODCALLTYPE PSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp);
	void	STDMETHODCALLTYPE VSGetShader(ID3D11VertexShader **pps, ID3D11ClassInstance **ppc, UINT *numc);
	void	STDMETHODCALLTYPE PSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp);
	void	STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout **pp);
	void	STDMETHODCALLTYPE IAGetVertexBuffers(UINT slot, UINT num, ID3D11Buffer **pp, UINT *strides, UINT *pOffsets);
	void	STDMETHODCALLTYPE IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset);
	void	STDMETHODCALLTYPE GSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp);
	void	STDMETHODCALLTYPE GSGetShader(ID3D11GeometryShader **pps, ID3D11ClassInstance **ppc, UINT *numc);
	void	STDMETHODCALLTYPE IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology);
	void	STDMETHODCALLTYPE VSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp);
	void	STDMETHODCALLTYPE VSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp);
	void	STDMETHODCALLTYPE GetPredication(ID3D11Predicate **pp, BOOL *pPredicateValue);
	void	STDMETHODCALLTYPE GSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp);
	void	STDMETHODCALLTYPE GSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp);
	void	STDMETHODCALLTYPE OMGetRenderTargets(UINT num, ID3D11RenderTargetView **ppRTV, ID3D11DepthStencilView **ppDSV);
	void	STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView **ppRTV, ID3D11DepthStencilView **ppDSV, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUAV);
	void	STDMETHODCALLTYPE OMGetBlendState(ID3D11BlendState **pp, FLOAT BlendFactor[4], UINT *pSampleMask);
	void	STDMETHODCALLTYPE OMGetDepthStencilState(ID3D11DepthStencilState **pp, UINT *pStencilRef);
	void	STDMETHODCALLTYPE SOGetTargets(UINT num, ID3D11Buffer **pp);
	void	STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState **pp);
	void	STDMETHODCALLTYPE RSGetViewports(UINT *pNum, D3D11_VIEWPORT *pViewports);
	void	STDMETHODCALLTYPE RSGetScissorRects(UINT *pNum, D3D11_RECT *pRects);
	void	STDMETHODCALLTYPE HSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp);
	void	STDMETHODCALLTYPE HSGetShader(ID3D11HullShader **pps, ID3D11ClassInstance **ppc, UINT *numc);
	void	STDMETHODCALLTYPE HSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp);
	void	STDMETHODCALLTYPE HSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp);
	void	STDMETHODCALLTYPE DSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp);
	void	STDMETHODCALLTYPE DSGetShader(ID3D11DomainShader **pps, ID3D11ClassInstance **ppc, UINT *numc);
	void	STDMETHODCALLTYPE DSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp);
	void	STDMETHODCALLTYPE DSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp);
	void	STDMETHODCALLTYPE CSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp);
	void	STDMETHODCALLTYPE CSGetUnorderedAccessViews(UINT slot, UINT num, ID3D11UnorderedAccessView **pp);
	void	STDMETHODCALLTYPE CSGetShader(ID3D11ComputeShader **pps, ID3D11ClassInstance **ppc, UINT *numc);
	void	STDMETHODCALLTYPE CSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp);
	void	STDMETHODCALLTYPE CSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp);
	void	STDMETHODCALLTYPE ClearState();
	void	STDMETHODCALLTYPE Flush();
	D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType();
	UINT	STDMETHODCALLTYPE GetContextFlags();
	HRESULT	STDMETHODCALLTYPE FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **pp);

	//ID3D11DeviceContext1
	void	STDMETHODCALLTYPE CopySubresourceRegion1(ID3D11Resource *dst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *src, UINT SrcSubresource, const D3D11_BOX *pSrcBox, UINT CopyFlags);
	void	STDMETHODCALLTYPE UpdateSubresource1(ID3D11Resource *dst, UINT DstSubresource, 	const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags);
	void	STDMETHODCALLTYPE DiscardResource(ID3D11Resource *rsrc);
	void	STDMETHODCALLTYPE DiscardView(ID3D11View *pResourceView);
	void	STDMETHODCALLTYPE VSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc);
	void	STDMETHODCALLTYPE HSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc);
	void	STDMETHODCALLTYPE DSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc);
	void	STDMETHODCALLTYPE GSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc);
	void	STDMETHODCALLTYPE PSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc);
	void	STDMETHODCALLTYPE CSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc);
	void	STDMETHODCALLTYPE VSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc);
	void	STDMETHODCALLTYPE HSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc);
	void	STDMETHODCALLTYPE DSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc);
	void	STDMETHODCALLTYPE GSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc);
	void	STDMETHODCALLTYPE PSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc);
	void	STDMETHODCALLTYPE CSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc);
	void	STDMETHODCALLTYPE SwapDeviceContextState(ID3DDeviceContextState *state, ID3DDeviceContextState **pp);
	void	STDMETHODCALLTYPE ClearView(ID3D11View *pView, const FLOAT Color[4], const D3D11_RECT *pRect, UINT num);
	void	STDMETHODCALLTYPE DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects, UINT num);

	//ID3D11DeviceContext2
	HRESULT	STDMETHODCALLTYPE UpdateTileMappings(ID3D11Resource *pTiledResource, UINT NumTiledResourceRegions, const D3D11_TILED_RESOURCE_COORDINATE *pTiledResourceRegionStartCoordinates, const D3D11_TILE_REGION_SIZE *pTiledResourceRegionSizes, ID3D11Buffer *pTilePool, UINT NumRanges, const UINT *pRangeFlags, const UINT *pTilePoolStartOffsets, const UINT *pRangeTileCounts, UINT Flags);
	HRESULT	STDMETHODCALLTYPE CopyTileMappings(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestRegionStartCoordinate, ID3D11Resource *pSourceTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pSourceRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, UINT Flags);
	void	STDMETHODCALLTYPE CopyTiles(ID3D11Resource *pTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, ID3D11Buffer *pBuffer, UINT64 BufferStartOffsetInBytes, UINT Flags);
	void	STDMETHODCALLTYPE UpdateTiles(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pDestTileRegionSize, const void *pSourceTileData, UINT Flags);
	HRESULT	STDMETHODCALLTYPE ResizeTilePool(ID3D11Buffer *pTilePool, UINT64 NewSizeInBytes);
	void	STDMETHODCALLTYPE TiledResourceBarrier(ID3D11DeviceChild *pTiledResourceOrViewAccessBeforeBarrier, ID3D11DeviceChild *pTiledResourceOrViewAccessAfterBarrier);
	BOOL	STDMETHODCALLTYPE IsAnnotationEnabled( void);
	void	STDMETHODCALLTYPE SetMarkerInt(LPCWSTR pLabel, INT Data);
	void	STDMETHODCALLTYPE BeginEventInt(LPCWSTR pLabel, INT Data);
	void	STDMETHODCALLTYPE EndEvent();
	
	//ID3D11DeviceContext3
	void	STDMETHODCALLTYPE Flush1(D3D11_CONTEXT_TYPE ContextType, HANDLE hEvent);
	void	STDMETHODCALLTYPE SetHardwareProtectionState(BOOL HwProtectionEnable);
	void	STDMETHODCALLTYPE GetHardwareProtectionState(BOOL *pHwProtectionEnable);

	//ID3D11DeviceContext4
	HRESULT	STDMETHODCALLTYPE Signal(ID3D11Fence *pFence, UINT64 Value);
	HRESULT	STDMETHODCALLTYPE Wait(ID3D11Fence *pFence, UINT64 Value);
};

template<> class Wrap<ID3D11DeviceLatest> : public com_wrap<ID3D11DeviceLatest>, public RecDevice {
	struct Presenter : PresentDevice {
		void	Present();
	} present;
public:
	linked_recording			recording;
	e_list<linked_recording>	recordings;
	e_list<RecItemLink>			items;
	CallStacks					callstacks;
	Wrap<ID3D11DeviceContextLatest>	*immediate;

	Wrap() : recording(Recording::DEVICE) {
		last_present_device = &present;
	}
	void	init(ID3D11DeviceContextLatest *_immediate) {
		immediate = com_wrap_system->make_wrap(_immediate, this);
		set_recording(capture.Update());
		//set_recording(capture.frames > 0);
	}

	void	set_recording(bool enable) {
		if (enable && !recording.enable) {
			capture.devices.push_back(this);
			recordings.push_back(&recording);
		}
		recording.enable = enable;
		immediate->set_recording(enable);
	}
	void	RecordCallStack(const context &ctx) {
		recording.Record(0xfffe, CallStacks::Stack<32>(callstacks, ctx));
	}

	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		if (riid == __uuidof(PresentDevice)) {
			*pp = &present;
			return S_OK;
		}
		if (check_interfaces<ID3D11Device,ID3D11Device1,ID3D11Device2,ID3D11Device3,ID3D11Device4,ID3D11DeviceLatest>(this, riid, pp))
			return S_OK;

		HRESULT	h = orig->QueryInterface(riid, pp);
		if (h == S_OK) {
			if (!check_dxgi_interfaces(riid, pp))
				*pp = static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(com_wrap_system->get_wrap(riid, *pp)));
		}
		return h;
	}
	ULONG STDMETHODCALLTYPE	Release() {
#if 0
		if (refs == 1) {
			capture.Update();
			//capture.event.Wait();
		}
#endif
		return com_wrap<ID3D11DeviceLatest>::Release();
	}
	//PresentDevice
	void	Present();

	//ID3D11Device
	HRESULT	STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Buffer **pp);
	HRESULT	STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Texture1D **pp);
	HRESULT	STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Texture2D **pp);
	HRESULT	STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Texture3D **pp);
	HRESULT	STDMETHODCALLTYPE CreateShaderResourceView(ID3D11Resource *rsrc, const D3D11_SHADER_RESOURCE_VIEW_DESC *desc, ID3D11ShaderResourceView **pp);
	HRESULT	STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D11Resource *rsrc, const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc, ID3D11UnorderedAccessView **pp);
	HRESULT	STDMETHODCALLTYPE CreateRenderTargetView(ID3D11Resource *rsrc, const D3D11_RENDER_TARGET_VIEW_DESC *desc, ID3D11RenderTargetView **pp);
	HRESULT	STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource *rsrc, const D3D11_DEPTH_STENCIL_VIEW_DESC *desc, ID3D11DepthStencilView **pp);
	HRESULT	STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *desc, UINT num, const void *bytecode, SIZE_T length, ID3D11InputLayout **pp);
	HRESULT	STDMETHODCALLTYPE CreateVertexShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11VertexShader **pp);
	HRESULT	STDMETHODCALLTYPE CreateGeometryShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11GeometryShader **pp);
	HRESULT	STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void *code, SIZE_T length, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *link, ID3D11GeometryShader **pp);
	HRESULT	STDMETHODCALLTYPE CreatePixelShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11PixelShader **pp);
	HRESULT	STDMETHODCALLTYPE CreateHullShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11HullShader **pp);
	HRESULT	STDMETHODCALLTYPE CreateDomainShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11DomainShader **pp);
	HRESULT	STDMETHODCALLTYPE CreateComputeShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11ComputeShader **pp);
	HRESULT	STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage **pp);
	HRESULT	STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC *desc, ID3D11BlendState **pp);
	HRESULT	STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *desc, ID3D11DepthStencilState **pp);
	HRESULT	STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC *desc, ID3D11RasterizerState **pp);
	HRESULT	STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC *desc, ID3D11SamplerState **pp);
	HRESULT	STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC *desc, ID3D11Query **pp);
	HRESULT	STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC *desc, ID3D11Predicate **pp);
	HRESULT	STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC *desc, ID3D11Counter **pp);
	HRESULT	STDMETHODCALLTYPE CreateDeferredContext(UINT flags, ID3D11DeviceContext **pp);
	HRESULT	STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format, UINT *support);
	HRESULT	STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNum);
	void	STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo);
	HRESULT	STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC *desc, D3D11_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength);
	HRESULT	STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize);
	HRESULT	STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *size, void *data);
	HRESULT	STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT size, const void *data);
	HRESULT	STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *data);
	D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel();
	UINT	STDMETHODCALLTYPE GetCreationFlags();
	HRESULT	STDMETHODCALLTYPE GetDeviceRemovedReason();
	void	STDMETHODCALLTYPE GetImmediateContext(ID3D11DeviceContext **pp);
	HRESULT	STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags);
	UINT	STDMETHODCALLTYPE GetExceptionMode();

	//ID3D11Device1
	void	STDMETHODCALLTYPE GetImmediateContext1(ID3D11DeviceContext1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateDeferredContext1(UINT flags, ID3D11DeviceContext1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateBlendState1(const D3D11_BLEND_DESC1 *desc, ID3D11BlendStateLatest **pp);
	HRESULT	STDMETHODCALLTYPE CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *desc, ID3D11RasterizerState1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateDeviceContextState(UINT Flags, const D3D_FEATURE_LEVEL*levels, UINT num, UINT SDKVersion, REFIID EmulatedInterface, D3D_FEATURE_LEVEL *chosen, ID3DDeviceContextState **pp);
	HRESULT	STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess, REFIID riid, void **pp);

	//ID3D11Device2
	void	STDMETHODCALLTYPE GetImmediateContext2(ID3D11DeviceContext2 **pp);
	HRESULT	STDMETHODCALLTYPE CreateDeferredContext2(UINT ContextFlags, ID3D11DeviceContext2 **pp);
	void	STDMETHODCALLTYPE GetResourceTiling(ID3D11Resource *pTiledResource, UINT *pNumTilesForEntireResource, D3D11_PACKED_MIP_DESC *pPackedMipDesc, D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips, UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet, D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips);
	HRESULT	STDMETHODCALLTYPE CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount, UINT Flags, UINT *pNumQualityLevels);

	//ID3D11Device3
	HRESULT	STDMETHODCALLTYPE CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *desc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *desc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc, ID3D11RasterizerStateLatest **pp);
	HRESULT	STDMETHODCALLTYPE CreateShaderResourceView1(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *desc, ID3D11ShaderResourceView1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateUnorderedAccessView1(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *desc, ID3D11UnorderedAccessView1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateRenderTargetView1(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC1 *desc, ID3D11RenderTargetView1 **pp);
	HRESULT	STDMETHODCALLTYPE CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc1, ID3D11QueryLatest **pp);
	void	STDMETHODCALLTYPE GetImmediateContext3(ID3D11DeviceContext3 **pp);
	HRESULT	STDMETHODCALLTYPE CreateDeferredContext3(UINT ContextFlags, ID3D11DeviceContext3 **pp);
	void	STDMETHODCALLTYPE WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
	void	STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox);

	//ID3D11Device4
	HRESULT	STDMETHODCALLTYPE RegisterDeviceRemovedEvent(HANDLE hEvent, DWORD *pdwCookie);
	void	STDMETHODCALLTYPE UnregisterDeviceRemoved(DWORD dwCookie);
        
	//ID3D11Device5
	HRESULT	STDMETHODCALLTYPE OpenSharedFence(HANDLE hFence, REFIID riid, void **pp);
	HRESULT	STDMETHODCALLTYPE CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags, REFIID riid, void **pp);
};

//-----------------------------------------------------------------------------
//	RecItemLink
//-----------------------------------------------------------------------------

void RecItemLink::init(Wrap<ID3D11DeviceLatest> *_device) {
	device	= _device;
	device->items.push_back(this);
}

void Wrap<ID3DDeviceContextState>::init(Wrap<ID3D11DeviceContextLatest> *ctx, ID3DDeviceContextState *next) {
	init(ctx->device);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D11DeviceContextLatest>
//-----------------------------------------------------------------------------

void Wrap<ID3D11DeviceContextLatest>::RecordCallStack(const context &ctx) {
	recording.Record(0xfffe, CallStacks::Stack<32>(device->callstacks, ctx));
}

void Wrap<ID3D11DeviceContextLatest>::set_recording(bool enable) {
	if (enable && !recording.enable) {
		device->recordings.push_back(&recording);
	}
	if (enable && recording.total == 0)
		recording.Record(tag_InitialState, DeviceContext_State(this));//DeviceContext_State(orig));
	recording.enable = enable;
}

//ID3D11DeviceContext
void	Wrap<ID3D11DeviceContextLatest>::VSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::VSSetConstantBuffers, tag_VSSetConstantBuffers, slot, num, make_counted<1>(pp));
	vs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::PSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::PSSetShaderResources, tag_PSSetShaderResources, slot, num, make_counted<1>(pp));
	ps.SetShaderResources(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::PSSetShader(ID3D11PixelShader *shader, ID3D11ClassInstance *const *pp, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::PSSetShader, tag_PSSetShader, shader, make_counted<2>(pp), num);
}
void	Wrap<ID3D11DeviceContextLatest>::PSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::PSSetSamplers, tag_PSSetSamplers, slot, num, make_counted<1>(pp));
	ps.SetSamplers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::VSSetShader(ID3D11VertexShader *shader, ID3D11ClassInstance *const *pp, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::VSSetShader, tag_VSSetShader, shader, make_counted<2>(pp), num);
}
void	Wrap<ID3D11DeviceContextLatest>::DrawIndexed(UINT IndexCount, UINT StartIndex, INT BaseVertex) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DrawIndexed, tag_DrawIndexed, IndexCount, StartIndex, BaseVertex);
}
void	Wrap<ID3D11DeviceContextLatest>::Draw(UINT VertexCount, UINT StartVertex) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::Draw, tag_Draw, VertexCount, StartVertex);
}
HRESULT Wrap<ID3D11DeviceContextLatest>::Map(ID3D11Resource *rsrc, UINT sub, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *mapped) {
	RECORDCALLSTACK;
	HRESULT	hr = recording.RunRecord2(this, &ID3D11DeviceContextLatest::Map, tag_Map, rsrc, sub, MapType, MapFlags, mapped);
	if (SUCCEEDED(hr) && MapType != D3D11_MAP_READ && recording.enable) {
		auto	*e = new map_entry(rsrc, sub, MapType, mapped);
		map_list.push_back(e);
	}
	return hr;
}
void	Wrap<ID3D11DeviceContextLatest>::Unmap(ID3D11Resource *rsrc, UINT sub) {
	for (auto &i : map_list) {
		if (i.rsrc == rsrc && i.sub == sub) {
			if (recording.enable) {
				if (i.data) {
					size_t	size	= i.cube.strided_copy_to(i.data, 0xcdcdcdcd);
					size_t	size2	= i.cube.copy_to(recording.get_space(tag_RLE_DATA, size), 0xcdcdcdcd);
					ISO_ASSERT(size == size2);
				} else {
					i.cube.copy_to(recording.get_space(tag_DATA, i.cube.size()));
				}
			}
			delete i.unlink();
			break;
		}
	}
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::Unmap, tag_Unmap, rsrc, sub);
}
void	Wrap<ID3D11DeviceContextLatest>::PSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::PSSetConstantBuffers, tag_PSSetConstantBuffers, slot, num, make_counted<1>(pp));
	ps.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::IASetInputLayout(ID3D11InputLayout *pInputLayout) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::IASetInputLayout, tag_IASetInputLayout, pInputLayout);
	ia = pInputLayout;
}
void	Wrap<ID3D11DeviceContextLatest>::IASetVertexBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *strides, const UINT *pOffsets) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::IASetVertexBuffers, tag_IASetVertexBuffers, slot, num, make_counted<1>(pp), make_counted<1>(strides), make_counted<1>(pOffsets));
}
void	Wrap<ID3D11DeviceContextLatest>::IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::IASetIndexBuffer, tag_IASetIndexBuffer, pIndexBuffer, Format, Offset);
}
void	Wrap<ID3D11DeviceContextLatest>::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndex, INT BaseVertex, UINT StartInstance) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DrawIndexedInstanced, tag_DrawIndexedInstanced, IndexCountPerInstance, InstanceCount, StartIndex, BaseVertex, StartInstance);
}
void	Wrap<ID3D11DeviceContextLatest>::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertex, UINT StartInstance) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DrawInstanced, tag_DrawInstanced, VertexCountPerInstance, InstanceCount, StartVertex, StartInstance);
}
void	Wrap<ID3D11DeviceContextLatest>::GSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::GSSetConstantBuffers, tag_GSSetConstantBuffers, slot, num, make_counted<1>(pp));
	gs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *pp, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::GSSetShader, tag_GSSetShader, pShader, make_counted<2>(pp), num);
}
void	Wrap<ID3D11DeviceContextLatest>::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::IASetPrimitiveTopology, tag_IASetPrimitiveTopology, Topology);
}
void	Wrap<ID3D11DeviceContextLatest>::VSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::VSSetShaderResources, tag_VSSetShaderResources, slot, num, make_counted<1>(pp));
	vs.SetShaderResources(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::VSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::VSSetSamplers, tag_VSSetSamplers, slot, num, make_counted<1>(pp));
	vs.SetSamplers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::Begin(ID3D11Asynchronous *async) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::Begin, tag_Begin, async);
}
void	Wrap<ID3D11DeviceContextLatest>::End(ID3D11Asynchronous *async) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::End, tag_End, async);
}
HRESULT Wrap<ID3D11DeviceContextLatest>::GetData(ID3D11Asynchronous *async, void *data, UINT size, UINT GetDataFlags) {
	return orig->GetData(com_wrap_system->unwrap(async), data, size, GetDataFlags);
}
void	Wrap<ID3D11DeviceContextLatest>::SetPredication(ID3D11Predicate *pred, BOOL PredicateValue) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::SetPredication, tag_SetPredication, pred, PredicateValue);
}
void	Wrap<ID3D11DeviceContextLatest>::GSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::GSSetShaderResources, tag_GSSetShaderResources, slot, num, make_counted<1>(pp));
	gs.SetShaderResources(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::GSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::GSSetSamplers, tag_GSSetSamplers, slot, num, make_counted<1>(pp));
	gs.SetSamplers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::OMSetRenderTargets(UINT num, ID3D11RenderTargetView *const *pp, ID3D11DepthStencilView *dsv) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::OMSetRenderTargets, tag_OMSetRenderTargets, num, make_counted<0>(pp), dsv);
}
void	Wrap<ID3D11DeviceContextLatest>::OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView *const *ppRTV, ID3D11DepthStencilView *dsv, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUAV, const UINT *pUAVInitialCounts) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews, tag_OMSetRenderTargetsAndUnorderedAccessViews,
		NumRTVs, make_counted<0>(ppRTV),
		dsv,
		UAVStartSlot, NumUAVs, make_counted<4>(ppUAV), make_counted<4>(pUAVInitialCounts)
	);
}
void	Wrap<ID3D11DeviceContextLatest>::OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::OMSetBlendState, tag_OMSetBlendState, pBlendState, BlendFactor, SampleMask);
}
void	Wrap<ID3D11DeviceContextLatest>::OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::OMSetDepthStencilState, tag_OMSetDepthStencilState, pDepthStencilState, StencilRef);
}
void	Wrap<ID3D11DeviceContextLatest>::SOSetTargets(UINT num, ID3D11Buffer *const *pp, const UINT *pOffsets) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::SOSetTargets, tag_SOSetTargets, num, make_counted<0>(pp), make_counted<0>(pOffsets));
}
void	Wrap<ID3D11DeviceContextLatest>::DrawAuto() {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DrawAuto, tag_DrawAuto);
}
void	Wrap<ID3D11DeviceContextLatest>::DrawIndexedInstancedIndirect(ID3D11Buffer *buffer, UINT AlignedByteOffsetForArgs) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DrawIndexedInstancedIndirect, tag_DrawIndexedInstancedIndirect, buffer, AlignedByteOffsetForArgs);
}
void	Wrap<ID3D11DeviceContextLatest>::DrawInstancedIndirect(ID3D11Buffer *buffer, UINT AlignedByteOffsetForArgs) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DrawInstancedIndirect, tag_DrawInstancedIndirect, buffer, AlignedByteOffsetForArgs);
}
void	Wrap<ID3D11DeviceContextLatest>::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::Dispatch, tag_Dispatch, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
void	Wrap<ID3D11DeviceContextLatest>::DispatchIndirect(ID3D11Buffer *buffer, UINT AlignedByteOffsetForArgs) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DispatchIndirect, tag_DispatchIndirect, buffer, AlignedByteOffsetForArgs);
}
void	Wrap<ID3D11DeviceContextLatest>::RSSetState(ID3D11RasterizerState *pRasterizerState) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::RSSetState, tag_RSSetState, pRasterizerState);
}
void	Wrap<ID3D11DeviceContextLatest>::RSSetViewports(UINT num, const D3D11_VIEWPORT *pViewports) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::RSSetViewports, tag_RSSetViewports, num, make_counted<0>(pViewports));
}
void	Wrap<ID3D11DeviceContextLatest>::RSSetScissorRects(UINT num, const D3D11_RECT *pRects) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::RSSetScissorRects, tag_RSSetScissorRects, num, make_counted<0>(pRects));
}
void	Wrap<ID3D11DeviceContextLatest>::CopySubresourceRegion(ID3D11Resource *dst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *src, UINT SrcSubresource, const D3D11_BOX *pSrcBox) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CopySubresourceRegion, tag_CopySubresourceRegion, dst, DstSubresource, DstX, DstY, DstZ, src, SrcSubresource, pSrcBox);
}
void	Wrap<ID3D11DeviceContextLatest>::CopyResource(ID3D11Resource *dst, ID3D11Resource *src) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CopyResource, tag_CopyResource, dst, src);
}

void	Wrap<ID3D11DeviceContextLatest>::UpdateSubresource(ID3D11Resource *dst, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
	if (recording.enable)
		//recording.Record(tag_DATA, const_memory_block(pSrcData, UpdateDataSize(dst, DstSubresource, pDstBox, SrcRowPitch, SrcDepthPitch)));
		recording.Record(tag_DATA, GetMemoryCube(dst, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch));
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::UpdateSubresource, tag_UpdateSubresource, dst, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void	Wrap<ID3D11DeviceContextLatest>::CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CopyStructureCount, tag_CopyStructureCount, pDstBuffer, DstAlignedByteOffset, pSrcView);
}
void	Wrap<ID3D11DeviceContextLatest>::ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ClearRenderTargetView, tag_ClearRenderTargetView, pRenderTargetView, *(const array<FLOAT, 4>*)ColorRGBA);
}
void	Wrap<ID3D11DeviceContextLatest>::ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *uav, const UINT Values[4]) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ClearUnorderedAccessViewUint, tag_ClearUnorderedAccessViewUint, uav, Values);
}
void	Wrap<ID3D11DeviceContextLatest>::ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *uav, const FLOAT Values[4]) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ClearUnorderedAccessViewFloat, tag_ClearUnorderedAccessViewFloat, uav, Values);
}
void	Wrap<ID3D11DeviceContextLatest>::ClearDepthStencilView(ID3D11DepthStencilView *dsv, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ClearDepthStencilView, tag_ClearDepthStencilView, dsv, ClearFlags, Depth, Stencil);
}
void	Wrap<ID3D11DeviceContextLatest>::GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::GenerateMips, tag_GenerateMips, pShaderResourceView);
}
void	Wrap<ID3D11DeviceContextLatest>::SetResourceMinLOD(ID3D11Resource *rsrc, FLOAT MinLOD) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::SetResourceMinLOD, tag_SetResourceMinLOD, rsrc, MinLOD);
}
FLOAT	Wrap<ID3D11DeviceContextLatest>::GetResourceMinLOD(ID3D11Resource *rsrc) {
	return orig->GetResourceMinLOD(com_wrap_system->unwrap(rsrc));
}
void	Wrap<ID3D11DeviceContextLatest>::ResolveSubresource(ID3D11Resource *dst, UINT DstSubresource, ID3D11Resource *src, UINT SrcSubresource, DXGI_FORMAT Format) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ResolveSubresource, tag_ResolveSubresource, dst, DstSubresource, src, SrcSubresource, Format);
}
void	Wrap<ID3D11DeviceContextLatest>::ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ExecuteCommandList, tag_ExecuteCommandList, pCommandList, RestoreContextState);
}
void	Wrap<ID3D11DeviceContextLatest>::HSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::HSSetShaderResources, tag_HSSetShaderResources, slot, num, make_counted<1>(pp));
	hs.SetShaderResources(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::HSSetShader(ID3D11HullShader *shader, ID3D11ClassInstance *const *pp, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::HSSetShader, tag_HSSetShader, shader, make_counted<2>(pp), num);
}
void	Wrap<ID3D11DeviceContextLatest>::HSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::HSSetSamplers, tag_HSSetSamplers, slot, num, make_counted<1>(pp));
	hs.SetSamplers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::HSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::HSSetConstantBuffers, tag_HSSetConstantBuffers, slot, num, make_counted<1>(pp));
	hs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::DSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DSSetShaderResources, tag_DSSetShaderResources, slot, num, make_counted<1>(pp));
	ds.SetShaderResources(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::DSSetShader(ID3D11DomainShader *shader, ID3D11ClassInstance *const *pp, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DSSetShader, tag_DSSetShader, shader, make_counted<2>(pp), num);
}
void	Wrap<ID3D11DeviceContextLatest>::DSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DSSetSamplers, tag_DSSetSamplers, slot, num, make_counted<1>(pp));
	ds.SetSamplers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::DSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DSSetConstantBuffers, tag_DSSetConstantBuffers, slot, num, make_counted<1>(pp));
	ds.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::CSSetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView *const *pp) {
	cs.SetShaderResources(slot, num, pp);
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CSSetShaderResources, tag_CSSetShaderResources, slot, num, make_counted<1>(pp));
}
void	Wrap<ID3D11DeviceContextLatest>::CSSetUnorderedAccessViews(UINT slot, UINT num, ID3D11UnorderedAccessView *const *pp, const UINT *pUAVInitialCounts) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CSSetUnorderedAccessViews, tag_CSSetUnorderedAccessViews, slot, num, make_counted<1>(pp), make_counted<1>(pUAVInitialCounts));
}
void	Wrap<ID3D11DeviceContextLatest>::CSSetShader(ID3D11ComputeShader *shader, ID3D11ClassInstance *const *pp, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CSSetShader, tag_CSSetShader, shader, make_counted<2>(pp), num);
}
void	Wrap<ID3D11DeviceContextLatest>::CSSetSamplers(UINT slot, UINT num, ID3D11SamplerState *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CSSetSamplers, tag_CSSetSamplers, slot, num, make_counted<1>(pp));
	cs.SetSamplers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::CSSetConstantBuffers(UINT slot, UINT num, ID3D11Buffer *const *pp) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CSSetConstantBuffers, tag_CSSetConstantBuffers, slot, num, make_counted<1>(pp));
	cs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::VSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp) {
	orig->VSGetConstantBuffers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::PSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp) {
	orig->PSGetShaderResources(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::PSGetShader(ID3D11PixelShader **pps, ID3D11ClassInstance **ppc, UINT *pNumClassInstances) {
	orig->PSGetShader(pps, ppc, pNumClassInstances);
	com_wrap_system->rewrap(pps);
	com_wrap_system->rewrap(ppc);
}
void	Wrap<ID3D11DeviceContextLatest>::PSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp) {
	orig->PSGetSamplers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::VSGetShader(ID3D11VertexShader **pps, ID3D11ClassInstance **ppc, UINT *pNumClassInstances) {
	orig->VSGetShader(pps, ppc, pNumClassInstances);
	com_wrap_system->rewrap(pps);
	com_wrap_system->rewrap(ppc);
}
void	Wrap<ID3D11DeviceContextLatest>::PSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp) {
	orig->PSGetConstantBuffers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::IAGetInputLayout(ID3D11InputLayout **pp) {
	*pp = ia;
//	orig->IAGetInputLayout(pp);
}
void	Wrap<ID3D11DeviceContextLatest>::IAGetVertexBuffers(UINT slot, UINT num, ID3D11Buffer **pp, UINT *strides, UINT *pOffsets) {
	orig->IAGetVertexBuffers(slot, num, pp, strides, pOffsets);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset) {
	orig->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
	com_wrap_system->rewrap(pIndexBuffer);
}
void	Wrap<ID3D11DeviceContextLatest>::GSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp) {
	orig->GSGetConstantBuffers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::GSGetShader(ID3D11GeometryShader **pps, ID3D11ClassInstance **ppc, UINT *pNumClassInstances) {
	orig->GSGetShader(pps, ppc, pNumClassInstances);
	com_wrap_system->rewrap(pps);
	com_wrap_system->rewrap(ppc);
}
void	Wrap<ID3D11DeviceContextLatest>::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology) {
	orig->IAGetPrimitiveTopology(pTopology);
}
void	Wrap<ID3D11DeviceContextLatest>::VSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp) {
	orig->VSGetShaderResources(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::VSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp) {
	orig->VSGetSamplers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::GetPredication(ID3D11Predicate **pp, BOOL *pPredicateValue) {
	orig->GetPredication(pp, pPredicateValue);
	com_wrap_system->rewrap(pp);
}
void	Wrap<ID3D11DeviceContextLatest>::GSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp) {
	orig->GSGetShaderResources(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::GSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp) {
	orig->GSGetSamplers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::OMGetRenderTargets(UINT num, ID3D11RenderTargetView **ppRTV, ID3D11DepthStencilView **ppDSV) {
	orig->OMGetRenderTargets(num, ppRTV, ppDSV);
	com_wrap_system->rewrap(ppRTV, num);
	com_wrap_system->rewrap(ppDSV);
}
void	Wrap<ID3D11DeviceContextLatest>::OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView **ppRTV, ID3D11DepthStencilView **ppDSV, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUAV) {
	orig->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRTV, ppDSV, UAVStartSlot, NumUAVs, ppUAV);
	com_wrap_system->rewrap(ppRTV, NumRTVs);
	com_wrap_system->rewrap(ppUAV, NumUAVs);
	com_wrap_system->rewrap(ppDSV);
}
void	Wrap<ID3D11DeviceContextLatest>::OMGetBlendState(ID3D11BlendState **pp, FLOAT BlendFactor[4], UINT *pSampleMask) {
	orig->OMGetBlendState(pp, BlendFactor, pSampleMask);
	com_wrap_system->rewrap(pp);
}
void	Wrap<ID3D11DeviceContextLatest>::OMGetDepthStencilState(ID3D11DepthStencilState **pp, UINT *pStencilRef) {
	orig->OMGetDepthStencilState(pp, pStencilRef);
	com_wrap_system->rewrap(pp);
}
void	Wrap<ID3D11DeviceContextLatest>::SOGetTargets(UINT num, ID3D11Buffer **pp) {
	orig->SOGetTargets(num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::RSGetState(ID3D11RasterizerState **pp) {
	orig->RSGetState(pp);
	com_wrap_system->rewrap(pp);
}
void	Wrap<ID3D11DeviceContextLatest>::RSGetViewports(UINT *pNum, D3D11_VIEWPORT *pViewports) {
	orig->RSGetViewports(pNum, pViewports);
}
void	Wrap<ID3D11DeviceContextLatest>::RSGetScissorRects(UINT *pNum, D3D11_RECT *pRects) {
	orig->RSGetScissorRects(pNum, pRects);
}
void	Wrap<ID3D11DeviceContextLatest>::HSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp) {
	orig->HSGetShaderResources(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::HSGetShader(ID3D11HullShader **pps, ID3D11ClassInstance **ppc, UINT *pNumClassInstances) {
	orig->HSGetShader(pps, ppc, pNumClassInstances);
	com_wrap_system->rewrap(pps);
	com_wrap_system->rewrap(ppc);
}
void	Wrap<ID3D11DeviceContextLatest>::HSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp) {
	orig->HSGetSamplers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::HSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp) {
	orig->HSGetConstantBuffers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::DSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp) {
	orig->DSGetShaderResources(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::DSGetShader(ID3D11DomainShader **pps, ID3D11ClassInstance **ppc, UINT *pNumClassInstances) {
	orig->DSGetShader(pps, ppc, pNumClassInstances);
	com_wrap_system->rewrap(pps);
	com_wrap_system->rewrap(ppc);
}
void	Wrap<ID3D11DeviceContextLatest>::DSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp) {
	orig->DSGetSamplers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::DSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp) {
	orig->DSGetConstantBuffers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::CSGetShaderResources(UINT slot, UINT num, ID3D11ShaderResourceView **pp) {
	orig->CSGetShaderResources(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::CSGetUnorderedAccessViews(UINT slot, UINT num, ID3D11UnorderedAccessView **pp) {
	orig->CSGetUnorderedAccessViews(slot, num, pp);
	com_wrap_system->rewrap_carefully(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::CSGetShader(ID3D11ComputeShader **pps, ID3D11ClassInstance **ppc, UINT *pNumClassInstances) {
	orig->CSGetShader(pps, ppc, pNumClassInstances);
	com_wrap_system->rewrap(pps);
	com_wrap_system->rewrap(ppc);
}
void	Wrap<ID3D11DeviceContextLatest>::CSGetSamplers(UINT slot, UINT num, ID3D11SamplerState **pp) {
	orig->CSGetSamplers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::CSGetConstantBuffers(UINT slot, UINT num, ID3D11Buffer **pp) {
	orig->CSGetConstantBuffers(slot, num, pp);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::ClearState() {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ClearState, tag_ClearState);
	ClearAll();
}
void	Wrap<ID3D11DeviceContextLatest>::Flush() {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::Flush, tag_Flush);
}
D3D11_DEVICE_CONTEXT_TYPE Wrap<ID3D11DeviceContextLatest>::GetType() {
	return orig->GetType();
}
UINT	Wrap<ID3D11DeviceContextLatest>::GetContextFlags() {
	return orig->GetContextFlags();
}
HRESULT Wrap<ID3D11DeviceContextLatest>::FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **pp) {
	RECORDCALLSTACK;
	return  recording.RunRecord2(this, &ID3D11DeviceContextLatest::FinishCommandList, tag_FinishCommandList, RestoreDeferredContextState, pp);
}
//ID3D11DeviceContext1
void	Wrap<ID3D11DeviceContextLatest>::CopySubresourceRegion1(ID3D11Resource *dst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *src, UINT SrcSubresource, const D3D11_BOX *pSrcBox, UINT CopyFlags) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CopySubresourceRegion1, tag_CopySubresourceRegion1, dst, DstSubresource, DstX, DstY, DstZ, src, SrcSubresource, pSrcBox, CopyFlags);
}
void	Wrap<ID3D11DeviceContextLatest>::UpdateSubresource1(ID3D11Resource *dst, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags) {
	if (recording.enable)
//		recording.Record(tag_DATA, const_memory_block(pSrcData, UpdateDataSize(dst, DstSubresource, pDstBox, SrcRowPitch, SrcDepthPitch)));
		recording.Record(tag_DATA, GetMemoryCube(dst, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch));
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::UpdateSubresource1, tag_UpdateSubresource1, dst, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
}
void	Wrap<ID3D11DeviceContextLatest>::DiscardResource(ID3D11Resource *rsrc) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DiscardResource, tag_DiscardResource, rsrc);
}
void	Wrap<ID3D11DeviceContextLatest>::DiscardView(ID3D11View *pResourceView) {
	orig->DiscardView(com_wrap_system->unwrap(pResourceView));
}
void	Wrap<ID3D11DeviceContextLatest>::VSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::VSSetConstantBuffers1, tag_VSSetConstantBuffers1, slot, num, make_counted<1>(pp), make_counted<1>(cnst), make_counted<1>(numc));
	vs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::HSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::HSSetConstantBuffers1, tag_HSSetConstantBuffers1, slot, num, make_counted<1>(pp), make_counted<1>(cnst), make_counted<1>(numc));
	hs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::DSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DSSetConstantBuffers1, tag_DSSetConstantBuffers1, slot, num, make_counted<1>(pp), make_counted<1>(cnst), make_counted<1>(numc));
	ds.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::GSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::GSSetConstantBuffers1, tag_GSSetConstantBuffers1, slot, num, make_counted<1>(pp), make_counted<1>(cnst), make_counted<1>(numc));
	gs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::PSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::PSSetConstantBuffers1, tag_PSSetConstantBuffers1, slot, num, make_counted<1>(pp), make_counted<1>(cnst), make_counted<1>(numc));
	ps.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::CSSetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer *const *pp, const UINT *cnst, const UINT *numc) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CSSetConstantBuffers1, tag_CSSetConstantBuffers1, slot, num, make_counted<1>(pp), make_counted<1>(cnst), make_counted<1>(numc));
	cs.SetConstantBuffers(slot, num, pp);
}
void	Wrap<ID3D11DeviceContextLatest>::VSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc) {
	orig->VSGetConstantBuffers1(slot, num, pp, cnst, numc);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::HSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc) {
	orig->HSGetConstantBuffers1(slot, num, pp, cnst, numc);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::DSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc) {
	orig->DSGetConstantBuffers1(slot, num, pp, cnst, numc);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::GSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc) {
	orig->GSGetConstantBuffers1(slot, num, pp, cnst, numc);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::PSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc) {
	orig->PSGetConstantBuffers1(slot, num, pp, cnst, numc);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::CSGetConstantBuffers1(UINT slot, UINT num, ID3D11Buffer **pp, UINT *cnst, UINT *numc) {
	orig->CSGetConstantBuffers1(slot, num, pp, cnst, numc);
	com_wrap_system->rewrap(pp, num);
}
void	Wrap<ID3D11DeviceContextLatest>::SwapDeviceContextState(ID3DDeviceContextState *state, ID3DDeviceContextState **pp) {
	RECORDCALLSTACK;
	recording.RunRecord2Wrap(this, &ID3D11DeviceContextLatest::SwapDeviceContextState, tag_SwapDeviceContextState, pp, state);
//	ReWrapCarefully(pp);
	dcs = state;
//	auto	*w = static_cast<Wrap<ID3DDeviceContextState>*>(*pp);
//	w->state.Get(this);
}
void	Wrap<ID3D11DeviceContextLatest>::ClearView(ID3D11View *pView, const FLOAT Color[4], const D3D11_RECT *pRect, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::ClearView, tag_ClearView, pView, Color, make_counted<3>(pRect), num);
}
void	Wrap<ID3D11DeviceContextLatest>::DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects, UINT num) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::DiscardView1, tag_DiscardView1, pResourceView, make_counted<2>(pRects), num);
}

//ID3D11DeviceContext2
HRESULT	Wrap<ID3D11DeviceContextLatest>::UpdateTileMappings(ID3D11Resource *pTiledResource, UINT NumTiledResourceRegions, const D3D11_TILED_RESOURCE_COORDINATE *pTiledResourceRegionStartCoordinates, const D3D11_TILE_REGION_SIZE *pTiledResourceRegionSizes, ID3D11Buffer *pTilePool, UINT NumRanges, const UINT *pRangeFlags, const UINT *pTilePoolStartOffsets, const UINT *pRangeTileCounts, UINT Flags) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceContextLatest::UpdateTileMappings, tag_UpdateTileMappings, pTiledResource, NumTiledResourceRegions, pTiledResourceRegionStartCoordinates, pTiledResourceRegionSizes, pTilePool, NumRanges, pRangeFlags, pTilePoolStartOffsets, pRangeTileCounts, Flags);
}
HRESULT	Wrap<ID3D11DeviceContextLatest>::CopyTileMappings(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestRegionStartCoordinate, ID3D11Resource *pSourceTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pSourceRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, UINT Flags) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceContextLatest::CopyTileMappings, tag_CopyTileMappings, pDestTiledResource, pDestRegionStartCoordinate, pSourceTiledResource, pSourceRegionStartCoordinate, pTileRegionSize, Flags);
}
void	Wrap<ID3D11DeviceContextLatest>::CopyTiles(ID3D11Resource *pTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, ID3D11Buffer *pBuffer, UINT64 BufferStartOffsetInBytes, UINT Flags) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::CopyTiles, tag_CopyTiles, pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer, BufferStartOffsetInBytes, Flags);
}
void	Wrap<ID3D11DeviceContextLatest>::UpdateTiles(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pDestTileRegionSize, const void *pSourceTileData, UINT Flags) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::UpdateTiles, tag_UpdateTiles, pDestTiledResource, pDestTileRegionStartCoordinate, pDestTileRegionSize, pSourceTileData, Flags);
}
HRESULT	Wrap<ID3D11DeviceContextLatest>::ResizeTilePool(ID3D11Buffer *pTilePool, UINT64 NewSizeInBytes) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceContextLatest::ResizeTilePool, tag_ResizeTilePool, pTilePool, NewSizeInBytes);
}
void	Wrap<ID3D11DeviceContextLatest>::TiledResourceBarrier(ID3D11DeviceChild *pTiledResourceOrViewAccessBeforeBarrier, ID3D11DeviceChild *pTiledResourceOrViewAccessAfterBarrier) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::TiledResourceBarrier, tag_TiledResourceBarrier, pTiledResourceOrViewAccessBeforeBarrier, pTiledResourceOrViewAccessAfterBarrier);
}
BOOL	Wrap<ID3D11DeviceContextLatest>::IsAnnotationEnabled() {
	return orig->IsAnnotationEnabled();
}
void	Wrap<ID3D11DeviceContextLatest>::SetMarkerInt(LPCWSTR pLabel, INT Data) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::SetMarkerInt, tag_SetMarkerInt, pLabel, Data);
}
void	Wrap<ID3D11DeviceContextLatest>::BeginEventInt(LPCWSTR pLabel, INT Data) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::BeginEventInt, tag_BeginEventInt, pLabel, Data);
}
void	Wrap<ID3D11DeviceContextLatest>::EndEvent() {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::EndEvent, tag_EndEvent);
}

//ID3D11DeviceContext3
void	Wrap<ID3D11DeviceContextLatest>::Flush1(D3D11_CONTEXT_TYPE ContextType, HANDLE hEvent) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::Flush1, tag_Flush1, ContextType, hEvent);
}
void	Wrap<ID3D11DeviceContextLatest>::SetHardwareProtectionState(BOOL HwProtectionEnable) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceContextLatest::SetHardwareProtectionState, tag_SetHardwareProtectionState, HwProtectionEnable);
}
void	Wrap<ID3D11DeviceContextLatest>::GetHardwareProtectionState(BOOL *pHwProtectionEnable) {
	orig->GetHardwareProtectionState(pHwProtectionEnable);
}

//ID3D11DeviceContext4
HRESULT	Wrap<ID3D11DeviceContextLatest>::Signal(ID3D11Fence *pFence, UINT64 Value) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceContextLatest::Signal, tag_Signal, pFence, Value);
}
HRESULT	Wrap<ID3D11DeviceContextLatest>::Wait(ID3D11Fence *pFence, UINT64 Value) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceContextLatest::Wait, tag_Wait, pFence, Value);
}

//-----------------------------------------------------------------------------
//	Wrap<ID3D11DeviceLatest>
//-----------------------------------------------------------------------------
void	Wrap<ID3D11DeviceLatest>::Presenter::Present() {
	T_get_enclosing(this, &Wrap<ID3D11DeviceLatest>::present)->set_recording(capture.Update());
}
//ID3D11Device
HRESULT Wrap<ID3D11DeviceLatest>::CreateBuffer(const D3D11_BUFFER_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Buffer **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateBuffer, tag_CreateBuffer, pp, desc, data);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateTexture1D(const D3D11_TEXTURE1D_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Texture1D **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateTexture1D, tag_CreateTexture1D, pp, desc, data);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateTexture2D(const D3D11_TEXTURE2D_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Texture2D **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateTexture2D, tag_CreateTexture2D, pp, desc, data);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateTexture3D(const D3D11_TEXTURE3D_DESC *desc, const D3D11_SUBRESOURCE_DATA *data, ID3D11Texture3D **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateTexture3D, tag_CreateTexture3D, pp, desc, data);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateShaderResourceView(ID3D11Resource *rsrc, const D3D11_SHADER_RESOURCE_VIEW_DESC *desc, ID3D11ShaderResourceView **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateShaderResourceView, tag_CreateShaderResourceView, pp, rsrc, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateUnorderedAccessView(ID3D11Resource *rsrc, const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc, ID3D11UnorderedAccessView **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateUnorderedAccessView, tag_CreateUnorderedAccessView, pp, rsrc, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateRenderTargetView(ID3D11Resource *rsrc, const D3D11_RENDER_TARGET_VIEW_DESC *desc, ID3D11RenderTargetView **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateRenderTargetView, tag_CreateRenderTargetView, pp, rsrc, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateDepthStencilView(ID3D11Resource *rsrc, const D3D11_DEPTH_STENCIL_VIEW_DESC *desc, ID3D11DepthStencilView **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateDepthStencilView, tag_CreateDepthStencilView, pp, rsrc, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *desc, UINT num, const void *code, SIZE_T length, ID3D11InputLayout **pp) {
	RECORDCALLSTACK;
	HRESULT	hr = recording.RunRecord(this, &ID3D11DeviceLatest::CreateInputLayout, tag_CreateInputLayout, make_counted<1>(desc), num, code, length, pp);
	return com_wrap_system->make_wrap(hr, pp, this, desc, num, code, length);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateVertexShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11VertexShader **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateVertexShader, tag_CreateVertexShader, pp, code, length, link);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateGeometryShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11GeometryShader **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateGeometryShader, tag_CreateGeometryShader, pp, code, length, link);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateGeometryShaderWithStreamOutput(const void *code, SIZE_T length, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *link, ID3D11GeometryShader **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateGeometryShaderWithStreamOutput, tag_CreateGeometryShaderWithStreamOutput, pp, code, length, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, link);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreatePixelShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11PixelShader **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreatePixelShader, tag_CreatePixelShader, pp, code, length, link);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateHullShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11HullShader **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateHullShader, tag_CreateHullShader, pp, code, length, link);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateDomainShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11DomainShader **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateDomainShader, tag_CreateDomainShader, pp, code, length, link);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateComputeShader(const void *code, SIZE_T length, ID3D11ClassLinkage *link, ID3D11ComputeShader **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateComputeShader, tag_CreateComputeShader, pp, code, length, link);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateClassLinkage(ID3D11ClassLinkage **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateClassLinkage, tag_CreateClassLinkage, pp);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateBlendState(const D3D11_BLEND_DESC *desc, ID3D11BlendState **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateBlendState, tag_CreateBlendState, pp, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *desc, ID3D11DepthStencilState **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateDepthStencilState, tag_CreateDepthStencilState, pp, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateRasterizerState(const D3D11_RASTERIZER_DESC *desc, ID3D11RasterizerState **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateRasterizerState, tag_CreateRasterizerState, pp, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateSamplerState(const D3D11_SAMPLER_DESC *desc, ID3D11SamplerState **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateSamplerState, tag_CreateSamplerState, pp, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateQuery(const D3D11_QUERY_DESC *desc, ID3D11Query **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateQuery, tag_CreateQuery, pp, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreatePredicate(const D3D11_QUERY_DESC *desc, ID3D11Predicate **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreatePredicate, tag_CreatePredicate, pp, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateCounter(const D3D11_COUNTER_DESC *desc, ID3D11Counter **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateCounter, tag_CreateCounter, pp, desc);
}
HRESULT Wrap<ID3D11DeviceLatest>::CreateDeferredContext(UINT flags, ID3D11DeviceContext **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateDeferredContext, tag_CreateDeferredContext, pp, flags);
}
HRESULT Wrap<ID3D11DeviceLatest>::OpenSharedResource(HANDLE hResource, REFIID riid, void **pp) {
	return orig->OpenSharedResource(hResource, riid, pp);
}
HRESULT Wrap<ID3D11DeviceLatest>::CheckFormatSupport(DXGI_FORMAT Format, UINT *support) {
	return orig->CheckFormatSupport(Format, support);
}
HRESULT Wrap<ID3D11DeviceLatest>::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNum) {
	return orig->CheckMultisampleQualityLevels(Format, SampleCount, pNum);
}
void Wrap<ID3D11DeviceLatest>::CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo) {
	orig->CheckCounterInfo(pCounterInfo);
}
HRESULT Wrap<ID3D11DeviceLatest>::CheckCounter(const D3D11_COUNTER_DESC *desc, D3D11_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength) {
	return orig->CheckCounter(desc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}
HRESULT Wrap<ID3D11DeviceLatest>::CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) {
	return orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT Wrap<ID3D11DeviceLatest>::GetPrivateData(REFGUID guid, UINT *size, void *data) {
	return orig->GetPrivateData(guid, size, data);
}
HRESULT Wrap<ID3D11DeviceLatest>::SetPrivateData(REFGUID guid, UINT size, const void *data) {
	return orig->SetPrivateData(guid, size, data);
}
HRESULT Wrap<ID3D11DeviceLatest>::SetPrivateDataInterface(REFGUID guid, const IUnknown *data) {
	return orig->SetPrivateDataInterface(guid, data);
}
D3D_FEATURE_LEVEL Wrap<ID3D11DeviceLatest>::GetFeatureLevel() {
	return orig->GetFeatureLevel();
}
UINT Wrap<ID3D11DeviceLatest>::GetCreationFlags() {
	return orig->GetCreationFlags();
}
HRESULT Wrap<ID3D11DeviceLatest>::GetDeviceRemovedReason() {
	return orig->GetDeviceRemovedReason();
}
void Wrap<ID3D11DeviceLatest>::GetImmediateContext(ID3D11DeviceContext **pp) {
	*pp = immediate;
	immediate->AddRef();
}
HRESULT Wrap<ID3D11DeviceLatest>::SetExceptionMode(UINT RaiseFlags) {
	return orig->SetExceptionMode(RaiseFlags);
}
UINT Wrap<ID3D11DeviceLatest>::GetExceptionMode() {
	return orig->GetExceptionMode();
}

//ID3D11Device1
void	Wrap<ID3D11DeviceLatest>::GetImmediateContext1(ID3D11DeviceContext1 **pp) {
	*pp = immediate;
	immediate->AddRef();
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateDeferredContext1(UINT flags, ID3D11DeviceContext1 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateDeferredContext1, tag_CreateDeferredContext1, pp, flags);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateBlendState1(const D3D11_BLEND_DESC1 *desc, ID3D11BlendStateLatest **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateBlendState1, tag_CreateBlendState1, pp, desc);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *desc, ID3D11RasterizerState1 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateRasterizerState1, tag_CreateRasterizerState1, pp, desc);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateDeviceContextState(UINT Flags, const D3D_FEATURE_LEVEL*levels, UINT num, UINT SDKVersion, REFIID EmulatedInterface, D3D_FEATURE_LEVEL *chosen, ID3DDeviceContextState **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateDeviceContextState, tag_CreateDeviceContextState, pp, Flags, make_counted<2>(levels), num, SDKVersion, EmulatedInterface, chosen);
}
HRESULT	Wrap<ID3D11DeviceLatest>::OpenSharedResource1(HANDLE hResource, REFIID riid, void **pp) {
	return orig->OpenSharedResource1(hResource, riid, pp);
}
HRESULT	Wrap<ID3D11DeviceLatest>::OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess, REFIID riid, void **pp) {
	return orig->OpenSharedResourceByName(lpName, dwDesiredAccess, riid, pp);
}

//ID3D11Device2
void	Wrap<ID3D11DeviceLatest>::GetImmediateContext2(ID3D11DeviceContext2 **pp) {
	*pp = immediate;
	immediate->AddRef();
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateDeferredContext2(UINT ContextFlags, ID3D11DeviceContext2 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateDeferredContext2, tag_CreateDeferredContext2, ContextFlags, pp);
}
void	Wrap<ID3D11DeviceLatest>::GetResourceTiling(ID3D11Resource *pTiledResource, UINT *pNumTilesForEntireResource, D3D11_PACKED_MIP_DESC *pPackedMipDesc, D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips, UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet, D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips) {
	orig->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips, pNumSubresourceTilings, FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount, UINT Flags, UINT *pNumQualityLevels) {
	return orig->CheckMultisampleQualityLevels1(Format, SampleCount, Flags, pNumQualityLevels);
}

//ID3D11Device3
HRESULT	Wrap<ID3D11DeviceLatest>::CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *desc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D1 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateTexture2D1, tag_CreateTexture2D1, desc, pInitialData, pp);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *desc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D1 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateTexture3D1, tag_CreateTexture3D1, desc, pInitialData, pp);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc, ID3D11RasterizerStateLatest **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateRasterizerState2, tag_CreateRasterizerState2, pRasterizerDesc, pp);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateShaderResourceView1(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *desc, ID3D11ShaderResourceView1 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateShaderResourceView1, tag_CreateShaderResourceView1, pResource, desc, pp);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateUnorderedAccessView1(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *desc, ID3D11UnorderedAccessView1 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateUnorderedAccessView1, tag_CreateUnorderedAccessView1, pResource, desc, pp);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateRenderTargetView1(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1, ID3D11RenderTargetView1 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateRenderTargetView1, tag_CreateRenderTargetView1, pResource, pDesc1, pp);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc1, ID3D11QueryLatest **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateQuery1, tag_CreateQuery1, pQueryDesc1, pp);
}
void	Wrap<ID3D11DeviceLatest>::GetImmediateContext3(ID3D11DeviceContext3 **pp) {
	*pp = immediate;
	immediate->AddRef();
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateDeferredContext3(UINT ContextFlags, ID3D11DeviceContext3 **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::CreateDeferredContext3, tag_CreateDeferredContext3, ContextFlags, pp);
}
void	Wrap<ID3D11DeviceLatest>::WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceLatest::WriteToSubresource, tag_WriteToSubresource, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void	Wrap<ID3D11DeviceLatest>::ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox) {
	orig->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, com_wrap_system->unwrap(pSrcResource), SrcSubresource, pSrcBox);
}

//ID3D11Device4
HRESULT	Wrap<ID3D11DeviceLatest>::RegisterDeviceRemovedEvent(HANDLE hEvent, DWORD *pdwCookie) {
	RECORDCALLSTACK;
	return recording.RunRecord2(this, &ID3D11DeviceLatest::RegisterDeviceRemovedEvent, tag_RegisterDeviceRemovedEvent, hEvent, pdwCookie);
}
void	Wrap<ID3D11DeviceLatest>::UnregisterDeviceRemoved(DWORD dwCookie) {
	RECORDCALLSTACK;
	recording.RunRecord2(this, &ID3D11DeviceLatest::UnregisterDeviceRemoved, tag_UnregisterDeviceRemoved, dwCookie);
}

//ID3D11Device5
HRESULT	Wrap<ID3D11DeviceLatest>::OpenSharedFence(HANDLE hFence, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::OpenSharedFence, tag_OpenSharedFence, riid, (ID3D11Fence**)pp, hFence);
}
HRESULT	Wrap<ID3D11DeviceLatest>::CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags, REFIID riid, void **pp) {
	RECORDCALLSTACK;
	return recording.RunRecord2Wrap(this, &ID3D11DeviceLatest::CreateFence, tag_CreateFence, riid, (ID3D11Fence**)pp, InitialValue, Flags);
}

//-----------------------------------------------------------------------------
//	Global
//-----------------------------------------------------------------------------

ReWrapperT<ID3D11Texture2D> rewrap_texture2D;

LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS *ep) {
	MiniDump(ep, get_exec_path().set_ext("dmp"));
	SetUnhandledExceptionFilter(0);

	for (;;) {
		OutputDebugStringA("In exception filter\n");
		capture.Update();
		Thread::sleep(1);
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}

com_ptr<IDXGIAdapter> GetDXGIAdaptor(IUnknown *device) {
	com_ptr<IDXGIDevice3> dxgi_device = com_cast<IDXGIDevice3>(device);
	IDXGIAdapter		*adapter;
	dxgi_device->GetAdapter(&adapter);
	return adapter;
}

com_ptr<IDXGIFactory5> GetDXGIFactory(IDXGIAdapter *adapter) {
	IDXGIFactory5	*factory;
	adapter->GetParent(IID_PPV_ARGS(&factory));
	return factory;
}

HRESULT WINAPI Hooked_D3D11CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, CONST D3D_FEATURE_LEVEL *levels, UINT num, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext) {
	SetUnhandledExceptionFilter(ExceptionFilter);

	IDXGIAdapter *pAdapter2	= com_wrap_system->unwrap_carefully(pAdapter);
	HRESULT	h = get_orig(D3D11CreateDevice)(pAdapter2, DriverType, Software, Flags, levels, num, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
	if (h == S_OK) {
		// get the DXGI factory that was used to create the Direct3D device
		auto	factory	= GetDXGIFactory(pAdapter2 ? pAdapter2 : (IDXGIAdapter*)GetDXGIAdaptor(*(ID3D11DeviceLatest**)ppDevice));
		
		Wrap<ID3D11DeviceLatest>	*device		= com_wrap_system->make_wrap(*(ID3D11DeviceLatest**)ppDevice, *(ID3D11DeviceContextLatest**)ppImmediateContext);
		*ppDevice			= device;
		*ppImmediateContext	= device->immediate;
	}
	return h;
}

HRESULT WINAPI Hooked_D3D11CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, CONST D3D_FEATURE_LEVEL *levels, UINT num, UINT SDKVersion, CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext) {
	HRESULT	h = get_orig(D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, levels, num, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
	if (h == S_OK) {
		// get the DXGI factory that was used to create the Direct3D device
		auto	factory	= GetDXGIFactory(pAdapter ? pAdapter : (IDXGIAdapter*)GetDXGIAdaptor(*(ID3D11DeviceLatest**)ppDevice));

		Wrap<ID3D11DeviceLatest>	*device		= com_wrap_system->make_wrap(*(ID3D11DeviceLatest**)ppDevice, *(ID3D11DeviceContextLatest**)ppImmediateContext);
		*ppSwapChain		= com_wrap_system->make_wrap(*(IDXGISwapChain3**)ppSwapChain, device);
		*ppDevice			= device;
		*ppImmediateContext	= device->immediate;
	}
	return h;
}

with_size<dynamic_array<RecItem2>> Capture::GetItems() {
	dynamic_array<RecItem2>	r;
	for (auto &d : devices) {
		for (auto &i : static_cast<Wrap<ID3D11DeviceLatest>&>(d).items) {
			auto	*obj	= static_cast<Wrap<ID3D11DeviceChild>*>(&i);
			auto	&rec	= r.emplace_back(i, obj);
			switch (rec.type) {
				case RecItem::DepthStencilState:
					rec.info	= &static_cast<Wrap<ID3D11DepthStencilState>*>(&i)->desc;
					break;
				case RecItem::BlendState:
					rec.info	= &static_cast<Wrap<ID3D11BlendStateLatest>*>(&i)->desc;
					break;
				case RecItem::RasterizerState:
					rec.info	= &static_cast<Wrap<ID3D11RasterizerStateLatest>*>(&i)->desc;
					break;

				case RecItem::Buffer:
				case RecItem::Texture1D:
				case RecItem::Texture2D:
				case RecItem::Texture3D:
					rec.info = static_cast<Wrap<ID3D11Buffer>*>(&i)->data;
					break;

				case RecItem::ShaderResourceView: {
					auto	view	= static_cast<Wrap<ID3D11ShaderResourceViewLatest>*>(&i);
					rec.info		= &view->info;
				#if 1
					if (!obj->is_orig_dead()) {
						ID3D11Resource *res;
						view->orig->GetResource(&res);
						*unconst((ID3D11Resource*const*)rec.info) = com_wrap_system->find_wrap_carefully(res);
					}
				#else
					com_ptr<ID3D11Resource>	res;
					view->GetResource(&res);
					*unconst((ID3D11Resource*const*)rec.info) = res;
				#endif

					//auto	wrap	= static_cast<Wrap<ID3D11ShaderResourceViewLatest>*>(&i);
					//rec.info		= &wrap->info;
					//if (!obj->is_orig_dead() && !com_wrap_system->is_wrap.count((_com_wrap*)wrap->info.resource)) {
					//	ID3D11Resource *res;
					//	wrap->orig->GetResource(&res);
					//	auto	res2	= com_wrap_system->find_wrap_carefully(res);
					//	*unconst((ID3D11Resource*const*)rec.info) = res2;
					//
					//	auto	&rec2	= r.emplace_back(i, obj);
					//	rec2.info		= res2->data;
					//	res2->Release();
					//}
					break;
				}
				case RecItem::RenderTargetView:
					rec.info	= &static_cast<Wrap<ID3D11RenderTargetViewLatest>*>(&i)->info;
					break;
				case RecItem::DepthStencilView:	
					rec.info	= &static_cast<Wrap<ID3D11DepthStencilView>*>(&i)->info;
					break;
				case RecItem::UnorderedAccessView:
					rec.info	= &static_cast<Wrap<ID3D11UnorderedAccessViewLatest>*>(&i)->info;
					break;

				case RecItem::VertexShader:
				case RecItem::HullShader:
				case RecItem::DomainShader:
				case RecItem::GeometryShader:
				case RecItem::PixelShader:
				case RecItem::ComputeShader:
				case RecItem::InputLayout:
					rec.info = static_cast<Wrap<ID3D11VertexShader>*>(&i)->blob;
					break;

				case RecItem::SamplerState:
					rec.info	= &static_cast<Wrap<ID3D11SamplerState>*>(&i)->desc;
					break;
				case RecItem::Query:
				case RecItem::Predicate:
					rec.info	= &static_cast<Wrap<ID3D11QueryLatest>*>(&i)->desc;
					break;
				case RecItem::Counter:
					rec.info	= &static_cast<Wrap<ID3D11Counter>*>(&i)->desc;
					break;
	#if 0
				case RecItem::DeviceContext: {
					auto	ctx		= static_cast<Wrap<ID3D11DeviceContextLatest>*>(&i);
					auto	state	= static_cast<DeviceContextState*>(ctx);
					state->Get(ctx->orig);
					rec.info		= memory_block(*state);
					break;
				}
	#endif

			}
			if (obj->is_orig_dead())
				rec.type = RecItem::TYPE(rec.type | RecItem::DEAD);
		}
	}
	return move(r);
}

template<typename T> RecItem2 GetResourceHelper(ID3D11Resource *res, RecItem::TYPE type) {
	typename DX11ResourceDesc<T>::type	desc;
	((T*)res)->GetDesc(&desc);
	RecItem2	rec(type, "", res);
	rec.info = &desc;
	return rec;
}

RecItem2 Capture::GetResource(uintptr_t _view) {
	auto	view	= (Wrap<ID3D11ShaderResourceViewLatest>*)_view;
	com_ptr<ID3D11Resource>		res;
	D3D11_RESOURCE_DIMENSION	dim;

	view->GetResource(&res);
	res->GetType(&dim);

	RecItem::TYPE	type;
	const_memory_block	info;
	switch (dim) {
		default:
		case D3D11_RESOURCE_DIMENSION_BUFFER:		return GetResourceHelper<ID3D11Buffer>(res, RecItem::Buffer);
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:	return GetResourceHelper<ID3D11Texture1D>(res, RecItem::Texture1D);
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:	return GetResourceHelper<ID3D11Texture2D>(res, RecItem::Texture2D);
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:	return GetResourceHelper<ID3D11Texture3D>(res, RecItem::Texture3D);
	}
}

struct Transferer {
	ID3D11Device					*device;
	com_ptr<ID3D11DeviceContext>	ctx;

	struct Info {
		D3D11_RESOURCE_DIMENSION	dim;
		int	width, height, depth;
		int	nmip, nsub;
		DXGI_COMPONENTS format;
		size_t			total_size;
	};

	Transferer(ID3D11Device *device) : device(device) {
		device->GetImmediateContext(&ctx);
	}
	ID3D11Resource *Transfer(ID3D11Resource *res, Info &info);
};

ID3D11Resource *Transferer::Transfer(ID3D11Resource *res, Info &info) {
	DXGI_FORMAT		format = DXGI_FORMAT_UNKNOWN;
	uint32			width = 0, height = 0, depth = 0, mips = 1, samples = 1, nsub = 1;
	size_t			total_size	= 0;
	res->GetType(&info.dim);

	ID3D11Resource	*ret;

	switch (info.dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER: {
			D3D11_BUFFER_DESC	desc;
			((ID3D11Buffer*)res)->GetDesc(&desc);
			total_size	= width		= desc.ByteWidth;

			ID3D11Buffer		*res2;
			desc.Usage				= D3D11_USAGE_STAGING;
			desc.BindFlags			= 0;
			desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
			desc.MiscFlags			= 0;
			device->CreateBuffer(&desc, 0, &res2);
			ctx->CopyResource(res2, res);
			ret = res2;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC	desc;
			((ID3D11Texture1D*)res)->GetDesc(&desc);
			total_size	= GetResourceSize(&desc);
			format		= desc.Format;
			width		= desc.Width;
			depth		= desc.ArraySize;
			mips		= desc.MipLevels;
			nsub		= mips * depth;

			ID3D11Texture1D		*res2;
			desc.Usage				= D3D11_USAGE_STAGING;
			desc.BindFlags			= 0;
			desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
			desc.MiscFlags			= 0;
			device->CreateTexture1D(&desc, 0, &res2);
			ctx->CopyResource(res2, res);
			ret = res2;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC	desc;
			((ID3D11Texture2D*)res)->GetDesc(&desc);
			total_size	= GetResourceSize(&desc);
			format		= desc.Format;
			width		= desc.Width;
			height		= desc.Height;
			depth		= desc.ArraySize;
			mips		= desc.MipLevels;
			samples		= desc.SampleDesc.Count;
			nsub		= mips * depth;

			ID3D11Texture2D		*res2;
			if (samples > 1) {
				com_ptr<ID3D11Texture2D>	res1;
				desc.Usage				= D3D11_USAGE_DEFAULT;
				desc.BindFlags			= D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
				desc.MiscFlags			= 0;
				desc.SampleDesc.Count	= 1;
				device->CreateTexture2D(&desc, 0, &res1);

				for (int i = 0; i < nsub; i++)
					ctx->ResolveSubresource(res1, i, res, i, format);

				desc.Usage				= D3D11_USAGE_STAGING;
				desc.BindFlags			= 0;
				desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
				desc.MiscFlags			= 0;
				device->CreateTexture2D(&desc, 0, &res2);

				ctx->CopyResource(res2, res1);

			} else {
				desc.Usage				= D3D11_USAGE_STAGING;
				desc.BindFlags			= 0;
				desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
				desc.MiscFlags			= 0;
				device->CreateTexture2D(&desc, 0, &res2);
				ctx->CopyResource(res2, res);
			}

			ret = res2;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC	desc;
			((ID3D11Texture3D*)res)->GetDesc(&desc);
			total_size	= GetResourceSize(&desc);
			format		= desc.Format;
			width		= desc.Width;
			height		= desc.Height;
			depth		= desc.Depth;
			mips		= desc.MipLevels;
			nsub		= mips;

			ID3D11Texture3D		*res2;
			desc.Usage				= D3D11_USAGE_STAGING;
			desc.BindFlags			= 0;
			desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
			desc.MiscFlags			= 0;
			device->CreateTexture3D(&desc, 0, &res2);
			ctx->CopyResource(res2, res);
			ret = res2;
			break;
		}
	}

	info.format		= format;
	info.width		= width;
	info.height		= height;
	info.depth		= depth;
	info.nsub		= nsub;
	info.nmip		= mips;
	info.total_size	= total_size;

	return ret;
}

malloc_block_all Capture::ResourceData(uintptr_t _res) {
	auto	res = (Wrap<ID3D11Resource>*)_res;
	malloc_block	out;

	if (res->orig && res->device) {
		Transferer	trans(res->device->orig);
		Transferer::Info	info;
		com_ptr<ID3D11Resource>	res2 = trans.Transfer(res->orig, info);

		uint8	*p = out.resize(info.total_size);
		for (int i = 0; i < info.nsub; i++) {
			D3D11_MAPPED_SUBRESOURCE	mapped;
			if (SUCCEEDED(trans.ctx->Map(res2, i, D3D11_MAP_READ, 0, &mapped))) {
				if (info.dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
					memcpy(p, mapped.pData, info.width);

				} else {
					int		mip			= i % info.nmip;
					uint32	dest_width	= mip_stride(info.format, info.width, mip, false);
					uint32	dest_stride = align_pow2(dest_width, 8);
					uint32	height		= mip_size(info.format, info.height, mip);
					size_t	sub_size	= dest_stride * height;
					if (dest_stride == mapped.RowPitch) {
						memcpy(p, mapped.pData, sub_size);
					} else {
						for (int y = 0; y < height; y++)
							memcpy(p + dest_stride * y, (const uint8*)mapped.pData + mapped.RowPitch * y, dest_width);
					}
					p += sub_size;
				}
			}
		}
	}
	return out;
}


CaptureStats Capture::CaptureFrames(int frames) {
	Set(frames);

	CaptureStats	stats;
	stats.num_cmdlists	= 0;
	stats.num_items		= 0;

	for (auto &d : devices) {
		auto	&d2 = static_cast<Wrap<ID3D11DeviceLatest>&>(d);
		stats.num_cmdlists	+= num_elements32(d2.recordings);
		stats.num_items		+= num_elements32(d2.items);
	}

	return stats;
}

with_size<dynamic_array<Recording>> Capture::GetRecordings() {
	dynamic_array<Recording>	r;
	for (auto &d : devices) {
		for (auto &i : static_cast<Wrap<ID3D11DeviceLatest>&>(d).recordings)
			r.emplace_back(i.type, i.get_buffer_reset());
	}
	return move(r);
}