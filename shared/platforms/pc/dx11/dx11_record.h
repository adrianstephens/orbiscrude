#include <d3d11_4.h>
#include "dx\dxgi_helpers.h"

namespace iso {

typedef	ID3D11Device5				ID3D11DeviceLatest;	
typedef	ID3D11DeviceContext4		ID3D11DeviceContextLatest;
typedef	ID3D11BlendState1			ID3D11BlendStateLatest;	
typedef	ID3D11RasterizerState2		ID3D11RasterizerStateLatest;
typedef	ID3D11Query1				ID3D11QueryLatest;
typedef	ID3D11Texture2D1			ID3D11Texture2DLatest;
typedef	ID3D11Texture3D1			ID3D11Texture3DLatest;
typedef	ID3D11ShaderResourceView1	ID3D11ShaderResourceViewLatest;
typedef	ID3D11RenderTargetView1		ID3D11RenderTargetViewLatest;
typedef	ID3D11UnorderedAccessView1	ID3D11UnorderedAccessViewLatest;

template<> struct	Wrappable<ID3D11Device				> : T_type<ID3D11DeviceLatest				> {};
template<> struct	Wrappable<ID3D11DeviceContext		> : T_type<ID3D11DeviceContextLatest		> {};
template<> struct	Wrappable<ID3D11DeviceContext1		> : T_type<ID3D11DeviceContextLatest		> {};
template<> struct	Wrappable<ID3D11DeviceContext2		> : T_type<ID3D11DeviceContextLatest		> {};
template<> struct	Wrappable<ID3D11DeviceContext3		> : T_type<ID3D11DeviceContextLatest		> {};

template<> struct	Wrappable<ID3D11BlendState			> : T_type<ID3D11BlendStateLatest			> {};
template<> struct	Wrappable<ID3D11RasterizerState		> : T_type<ID3D11RasterizerStateLatest		> {};
template<> struct	Wrappable<ID3D11RasterizerState1	> : T_type<ID3D11RasterizerStateLatest		> {};

template<> struct	Wrappable<ID3D11Query				> : T_type<ID3D11QueryLatest				> {};
template<> struct	Wrappable<ID3D11Texture2D			> : T_type<ID3D11Texture2DLatest			> {};
template<> struct	Wrappable<ID3D11Texture3D			> : T_type<ID3D11Texture3DLatest			> {};
template<> struct	Wrappable<ID3D11ShaderResourceView	> : T_type<ID3D11ShaderResourceViewLatest	> {};
template<> struct	Wrappable<ID3D11RenderTargetView	> : T_type<ID3D11RenderTargetViewLatest		> {};
template<> struct	Wrappable<ID3D11UnorderedAccessView	> : T_type<ID3D11UnorderedAccessViewLatest	> {};

//TBD (?)
//ID3D11Fence
//ID3D11Multithread
//ID3D11VideoContext3
//ID3D11VideoDevice2

struct D3D11_INPUT_ELEMENT_DESC_rel {
	soft_pointer<const char, base_relative<uint32> >	SemanticName;
	UINT		SemanticIndex;
	DXGI_FORMAT Format;
	UINT		InputSlot;
	UINT		AlignedByteOffset;
	D3D11_INPUT_CLASSIFICATION InputSlotClass;
	UINT		InstanceDataStepRate;
	operator D3D11_INPUT_ELEMENT_DESC() const {
		D3D11_INPUT_ELEMENT_DESC desc;
		desc.SemanticName			= SemanticName;
		desc.SemanticIndex			= SemanticIndex;
		desc.Format					= Format;
		desc.InputSlot				= InputSlot;
		desc.AlignedByteOffset		= AlignedByteOffset;
		desc.InputSlotClass			= InputSlotClass;
		desc.InstanceDataStepRate	= InstanceDataStepRate;
		return desc;
	}
};

template<> struct PM<D3D11_INPUT_ELEMENT_DESC>			{ typedef D3D11_INPUT_ELEMENT_DESC_rel type; };
template<> struct RTM<D3D11_INPUT_ELEMENT_DESC>			{ typedef D3D11_INPUT_ELEMENT_DESC_rel type; };
template<> struct RTM<const D3D11_INPUT_ELEMENT_DESC>	{ typedef D3D11_INPUT_ELEMENT_DESC_rel type; };

template<class A>	void allocate(A &a, const D3D11_INPUT_ELEMENT_DESC &t, D3D11_INPUT_ELEMENT_DESC_rel*) {
	a.alloc(strlen(t.SemanticName) + 1);
}
template<class A>	void transfer(A &a, const D3D11_INPUT_ELEMENT_DESC &t0, D3D11_INPUT_ELEMENT_DESC_rel &t1) {
	char *p = (char*)a.alloc(strlen(t0.SemanticName) + 1);
	strcpy(p, t0.SemanticName);
	t1.SemanticName				= p;
	t1.SemanticIndex			= t0.SemanticIndex;
	t1.Format					= t0.Format;
	t1.InputSlot				= t0.InputSlot;
	t1.AlignedByteOffset		= t0.AlignedByteOffset;
	t1.InputSlotClass			= t0.InputSlotClass;
	t1.InstanceDataStepRate		= t0.InstanceDataStepRate;
}

}

namespace dx11 {
using namespace iso;

enum Interface : uint8 {
	INTF_Pause	= 1,
	INTF_Continue,
	INTF_CaptureFrames,
	INTF_GetRecordings,
	INTF_GetItems,
	INTF_ResourceData,
	INTF_GetResource,
};

struct CaptureStats {
	uint32	num_cmdlists;
	uint32	num_items;
};

template<typename D> struct RecView {
	//com_ptr2<ID3D11Resource>	resource;	//wrapped need to keep ref because accessed by GetResource
	ID3D11Resource*	resource;				//wrapped can't use in GetResource - might be freed
	soft_pointer<D, base_relative<uint32> >	pdesc;
	D							desc;
	void	init(ID3D11Resource *_resource, const D *_desc) {
		resource	= _resource;
		pdesc		= _desc ? &(desc = *_desc) : 0;
	}
};

template<class T> struct DX11ViewDesc;
template<> struct DX11ViewDesc<ID3D11ShaderResourceViewLatest>	{ typedef D3D11_SHADER_RESOURCE_VIEW_DESC	type; };
template<> struct DX11ViewDesc<ID3D11RenderTargetViewLatest>	{ typedef D3D11_RENDER_TARGET_VIEW_DESC		type; };
template<> struct DX11ViewDesc<ID3D11DepthStencilView>			{ typedef D3D11_DEPTH_STENCIL_VIEW_DESC		type; };
template<> struct DX11ViewDesc<ID3D11UnorderedAccessViewLatest>	{ typedef D3D11_UNORDERED_ACCESS_VIEW_DESC	type; };

template<class T> struct DX11ResourceDesc					{ typedef D3D11_BUFFER_DESC					type; };
//template<> struct DX11ResourceDesc<ID3D11Buffer>			{ typedef D3D11_BUFFER_DESC					type; };
template<> struct DX11ResourceDesc<ID3D11Texture1D>			{ typedef D3D11_TEXTURE1D_DESC				type; };
template<> struct DX11ResourceDesc<ID3D11Texture2D>			{ typedef D3D11_TEXTURE2D_DESC				type; };
template<> struct DX11ResourceDesc<ID3D11Texture3D>			{ typedef D3D11_TEXTURE3D_DESC				type; };

inline  DXGI_FORMAT	GetFormat(const D3D11_TEXTURE1D_DESC	*desc)	{ return desc->Format; }
inline  DXGI_FORMAT	GetFormat(const D3D11_TEXTURE2D_DESC	*desc)	{ return desc->Format; }
inline  DXGI_FORMAT	GetFormat(const D3D11_TEXTURE3D_DESC	*desc)	{ return desc->Format; }

inline  uint32	NumSubResources(const D3D11_BUFFER_DESC	*desc)		{ return 1; }
inline  uint32	NumSubResources(const D3D11_TEXTURE1D_DESC	*desc)	{ return desc->MipLevels * desc->ArraySize; }
inline  uint32	NumSubResources(const D3D11_TEXTURE2D_DESC	*desc)	{ return desc->MipLevels * desc->ArraySize; }
inline  uint32	NumSubResources(const D3D11_TEXTURE3D_DESC	*desc)	{ return desc->MipLevels; }

inline  uint32	CalcSubResource(const D3D11_BUFFER_DESC		*desc, uint32 mip, uint32 slice)	{ return 0; }
inline  uint32	CalcSubResource(const D3D11_TEXTURE1D_DESC	*desc, uint32 mip, uint32 slice)	{ return mip + desc->MipLevels * (slice + desc->ArraySize); }
inline  uint32	CalcSubResource(const D3D11_TEXTURE2D_DESC	*desc, uint32 mip, uint32 slice)	{ return mip + desc->MipLevels * (slice + desc->ArraySize); }
inline  uint32	CalcSubResource(const D3D11_TEXTURE3D_DESC	*desc, uint32 mip, uint32 slice)	{ return mip + desc->MipLevels; }

inline size_t GetResourceSize(const D3D11_TEXTURE1D_DESC *desc) { return size1D(DXGI_COMPONENTS(desc->Format), desc->Width, desc->MipLevels) * desc->ArraySize; }
inline size_t GetResourceSize(const D3D11_TEXTURE2D_DESC *desc) { return size2D(DXGI_COMPONENTS(desc->Format), desc->Width, desc->Height, desc->MipLevels) * desc->ArraySize; }
inline size_t GetResourceSize(const D3D11_TEXTURE3D_DESC *desc) { return size3D(DXGI_COMPONENTS(desc->Format), desc->Width, desc->Height, desc->Depth, desc->MipLevels); }

inline size_t GetSubResourceOffset(const D3D11_BUFFER_DESC *desc, int sub) {
	return 0;
}
inline size_t GetSubResourceOffset(const D3D11_TEXTURE1D_DESC *desc, int sub) {
	DXGI_COMPONENTS	comp(desc->Format);
	int				mip = sub % desc->MipLevels;
	return size1D(comp, desc->Width, desc->MipLevels) * (sub / desc->MipLevels) + size1D(comp, desc->Width, mip);
}
inline size_t GetSubResourceOffset(const D3D11_TEXTURE2D_DESC *desc, int sub) {
	DXGI_COMPONENTS	comp(desc->Format);
	int				mip = sub % desc->MipLevels;
	return size2D(comp, desc->Width, desc->Height, desc->MipLevels) * (sub / desc->MipLevels) + size2D(comp, desc->Width, desc->Height, mip);
}
inline size_t GetSubResourceOffset(const D3D11_TEXTURE3D_DESC *desc, int sub) {
	DXGI_COMPONENTS	comp(desc->Format);
	int				mip = sub % desc->MipLevels;
	return size3D(comp, desc->Width, desc->Height, desc->Depth, mip);
}

inline size_t GetSubResourceSize(const D3D11_TEXTURE1D_DESC *desc, int sub) {
	return mip_stride(desc->Format, desc->Width, sub % desc->MipLevels, false);
}
inline size_t GetSubResourceSize(const D3D11_TEXTURE2D_DESC *desc, int sub) {
	DXGI_COMPONENTS	comp(desc->Format);
	int				mip = sub % desc->MipLevels;
	return mip_stride(comp, desc->Width, mip, true) * mip_size(comp, desc->Height, mip);
}
inline size_t GetSubResourceSize(const D3D11_TEXTURE3D_DESC *desc, int sub) {
	DXGI_COMPONENTS	comp(desc->Format);
	int		mip = sub % desc->MipLevels;
	return mip_stride(comp, desc->Width, mip, true) * mip_size(comp, desc->Height, mip) *  mip_size(desc->Depth, mip);
}

inline uint32 GetSubResourceDims(const D3D11_TEXTURE1D_DESC *desc, int sub, uint32 &rows) {
	int		mip = sub % desc->MipLevels;
	rows	= desc->ArraySize;
	return mip_stride(desc->Format, desc->Width, mip, false);
}
inline uint32 GetSubResourceDims(const D3D11_TEXTURE2D_DESC *desc, int sub, uint32 &rows, uint32 &array) {
	DXGI_COMPONENTS	comp(desc->Format);
	int		mip = sub % desc->MipLevels;
	rows	= mip_size(comp, desc->Height, mip);
	array	= desc->ArraySize;
	return mip_stride(desc->Format, desc->Width, mip, false);
}
inline uint32 GetSubResourceDims(const D3D11_TEXTURE3D_DESC *desc, int sub, uint32 &rows, uint32 &depth) {
	DXGI_COMPONENTS	comp(desc->Format);
	int		mip = sub % desc->MipLevels;
	rows	= mip_size(comp, desc->Height, mip);
	depth	= mip_size(desc->Depth, mip);
	return mip_stride(desc->Format, desc->Width, mip, false);
}

struct RecItem {
	enum TYPE {
		Unknown,

		DepthStencilState,
		BlendState,
		RasterizerState,

		Buffer,
		Texture1D,
		Texture2D,
		Texture3D,
		
		ShaderResourceView,
		RenderTargetView,
		DepthStencilView,
		UnorderedAccessView,

		VertexShader,
		HullShader,
		DomainShader,
		GeometryShader,
		PixelShader,
		ComputeShader,
		InputLayout,
		
		SamplerState,
		Query,
		Predicate,
		Counter,
		ClassInstance,
		ClassLinkage,
		CommandList,
		DeviceContextState,
		DeviceContext,
		Device,
		Fence,

		NUM_TYPES,
		DEAD	= 1 << 7

	};
	TYPE			type;
	string16		name;

	RecItem(TYPE type = Unknown)		: type(type) {}
	RecItem(TYPE type, string16 &&name) : type(type), name(move(name)) {}
	constexpr bool is_dead() const		{ return !!(type & DEAD); }

	friend constexpr bool is_resource(TYPE id) {
		return between(id & ~DEAD, Buffer, Texture3D);
	}
	friend constexpr bool is_view(TYPE id) {
		return between(id & ~DEAD, ShaderResourceView, UnorderedAccessView);
	}
	friend constexpr bool is_shader(TYPE id) {
		return between(id & ~DEAD, VertexShader, ComputeShader);
	}
	friend constexpr TYPE undead(TYPE id) {
		return TYPE(id & ~DEAD);
	}
};

struct RecItem2 : RecItem {
	ID3D11DeviceChild	*obj;
	malloc_block_all	info;
	RecItem2()	{}
	RecItem2(RecItem &rec, ID3D11DeviceChild *obj) : RecItem(rec), obj(obj) {}// name.clear(); }
	RecItem2(TYPE type, string16 &&name, ID3D11DeviceChild *obj) : RecItem(type, move(name)), obj(obj) {}
	
	template<typename R>	bool read(R &&r) {
		return iso::read(r, type, name, obj, info);
	}
	template<typename W>	bool write(W &&w) const	{
		return iso::write(w, type, name, obj, info);
	}

	size_t GetSize() const {
		switch (undead(type)) {
			case Buffer:	return ((const D3D11_BUFFER_DESC*)info)->ByteWidth;
			case Texture1D:	return dx11::GetResourceSize((const D3D11_TEXTURE1D_DESC*)info);
			case Texture2D:	return dx11::GetResourceSize((const D3D11_TEXTURE2D_DESC*)info);
			case Texture3D:	return dx11::GetResourceSize((const D3D11_TEXTURE3D_DESC*)info);
			case VertexShader:
			case HullShader:
			case DomainShader:
			case GeometryShader:
			case PixelShader:
			case ComputeShader:
			case InputLayout: return info.length();
			default:		return 0;
		}
	}
	template<typename D> const_memory_block _GetData() const {
		return info.length() > sizeof(D) ? info.slice(sizeof(D)) : none;
	}
	const_memory_block GetData() const {
		switch (undead(type)) {
			case Buffer:	return _GetData<D3D11_BUFFER_DESC>();
			case Texture1D:	return _GetData<D3D11_TEXTURE1D_DESC>();
			case Texture2D:	return _GetData<D3D11_TEXTURE2D_DESC>();
			case Texture3D:	return _GetData<D3D11_TEXTURE3D_DESC>();
			default:		return none;
		}
	}
	uint32	NumSubResources() const {
		switch (undead(type)) {
			case Buffer:	return 1;
			case Texture1D:	return dx11::NumSubResources((const D3D11_TEXTURE1D_DESC*)info);
			case Texture2D:	return dx11::NumSubResources((const D3D11_TEXTURE2D_DESC*)info);
			case Texture3D:	return dx11::NumSubResources((const D3D11_TEXTURE3D_DESC*)info);
			default:		return 0;
		}
	}
	uint32 GetSubResourceDims(int sub, uint32 &rows, uint32 &depth) const {
		switch (undead(type)) {
			case Buffer:	rows = depth = 0; return ((const D3D11_BUFFER_DESC*)info)->ByteWidth;
			case Texture1D:	depth = 0; return dx11::GetSubResourceDims((const D3D11_TEXTURE1D_DESC*)info, sub, rows);
			case Texture2D:	return dx11::GetSubResourceDims((const D3D11_TEXTURE2D_DESC*)info, sub, rows, depth);
			case Texture3D:	return dx11::GetSubResourceDims((const D3D11_TEXTURE3D_DESC*)info, sub, rows, depth);
			default:		return rows = depth = 0;
		}
	}
	size_t GetSubResourceSize(int sub) const {
		switch (undead(type)) {
			case Buffer:	return ((const D3D11_BUFFER_DESC*)info)->ByteWidth;
			case Texture1D:	return dx11::GetSubResourceSize((const D3D11_TEXTURE1D_DESC*)info, sub);
			case Texture2D:	return dx11::GetSubResourceSize((const D3D11_TEXTURE2D_DESC*)info, sub);
			case Texture3D:	return dx11::GetSubResourceSize((const D3D11_TEXTURE3D_DESC*)info, sub);
			default:		return 0;
		}
	}
	size_t GetSubResourceOffset(int sub) const {
		switch (undead(type)) {
			case Texture1D:	return dx11::GetSubResourceOffset((const D3D11_TEXTURE1D_DESC*)info, sub);
			case Texture2D:	return dx11::GetSubResourceOffset((const D3D11_TEXTURE2D_DESC*)info, sub);
			case Texture3D:	return dx11::GetSubResourceOffset((const D3D11_TEXTURE3D_DESC*)info, sub);
			default:		return 0;
		}
	}
	template<typename D> const_memory_block _GetSubResourceData(int sub) const {
		if (info.length() > sizeof(D))
			return info.slice(sizeof(D) + dx11::GetSubResourceOffset((const D*)info, sub), dx11::GetSubResourceSize((const D*)info, sub));
		return none;
	}
	const_memory_block GetSubResourceData(int sub) const {
		switch (undead(type)) {
			case Buffer:	return _GetData<D3D11_BUFFER_DESC>();
			case Texture1D:	return _GetSubResourceData<D3D11_TEXTURE1D_DESC>(sub);
			case Texture2D:	return _GetSubResourceData<D3D11_TEXTURE2D_DESC>(sub);
			case Texture3D:	return _GetSubResourceData<D3D11_TEXTURE3D_DESC>(sub);
			default:		return none;
		}
	}
};

template<typename C> uint32 num_existing(const C &c) {
	for (auto &i : c) {
		if (!i)
			return (uint32)index_of(c, &i);
	}
	return num_elements32(c);
}

struct DeviceContext_State {
	enum { MAX_VB = 8 };

	struct Stage {
		ID3D11DeviceChild			*shader;
		ID3D11Buffer				*cb[8];
		ID3D11ShaderResourceView	*srv[8];
		ID3D11SamplerState			*smp[8];
	};
	struct VertexBuffer {
		ID3D11Buffer	*buffer;
		uint32			stride;
		xint32			offset;
	};
	struct IndexBuffer {
		ID3D11Buffer	*buffer;
		DXGI_FORMAT		format;
		xint32			offset;
	};

	//stages
	Stage						vs, ps, ds, hs, gs, cs;

	//UAV
	ID3D11UnorderedAccessView	*ps_uav[D3D11_PS_CS_UAV_REGISTER_COUNT];
	ID3D11UnorderedAccessView	*cs_uav[D3D11_PS_CS_UAV_REGISTER_COUNT];

	//SO
	ID3D11Buffer				*so_buffer[D3D11_SO_BUFFER_SLOT_COUNT];
	uint32						so_offset[D3D11_SO_BUFFER_SLOT_COUNT];

	//RS
	ID3D11RasterizerState		*rs;
	UINT						num_viewport, num_scissor;
	D3D11_VIEWPORT				viewport[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];	//should be D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE
	D3D11_RECT					scissor[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];	//should be D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE

	//IA
	ID3D11InputLayout			*ia;
	IndexBuffer					ib;
	VertexBuffer				vb[MAX_VB];
	D3D_PRIMITIVE_TOPOLOGY		prim;

	//OM
	ID3D11DepthStencilView		*dsv;
	ID3D11RenderTargetView		*rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11BlendState			*blend;
	FLOAT						blend_factor[4];
	UINT						sample_mask;
	ID3D11DepthStencilState		*depth_stencil;
	UINT						stencil_ref;

	DeviceContext_State()							{}
	DeviceContext_State(ID3D11DeviceContext *ctx)	{ Get(ctx); }

	auto					Stages()				const { return make_range(&vs, &cs + 1); };
	auto					Stages(bool compute)	const { return compute ? make_range_n(&cs, 1) : make_range(&vs, &cs); };
	const Stage&			GetStage(int i)			const { return (&vs)[i]; };
	const D3D11_VIEWPORT&	GetViewport(int i)		const { return viewport[i]; };
	const D3D11_RECT&		GetScissor(int i)		const { return scissor[i]; };

	void Get(ID3D11DeviceContext *ctx) {
		clear(*this);
		ctx->VSGetConstantBuffers(0, num_elements32(vs.cb), vs.cb);
		ctx->PSGetConstantBuffers(0, num_elements32(ps.cb), ps.cb);
		ctx->DSGetConstantBuffers(0, num_elements32(ds.cb), ds.cb);
		ctx->HSGetConstantBuffers(0, num_elements32(hs.cb), hs.cb);
		ctx->GSGetConstantBuffers(0, num_elements32(gs.cb), gs.cb);
		ctx->CSGetConstantBuffers(0, num_elements32(cs.cb), cs.cb);

		ctx->VSGetShaderResources(0, num_elements32(vs.srv), vs.srv);
		ctx->PSGetShaderResources(0, num_elements32(ps.srv), ps.srv);
		ctx->DSGetShaderResources(0, num_elements32(ds.srv), ds.srv);
		ctx->HSGetShaderResources(0, num_elements32(hs.srv), hs.srv);
		ctx->GSGetShaderResources(0, num_elements32(gs.srv), gs.srv);
		ctx->CSGetShaderResources(0, num_elements32(cs.srv), cs.srv);

		ctx->VSGetSamplers(0, num_elements32(vs.smp), vs.smp);
		ctx->PSGetSamplers(0, num_elements32(ps.smp), ps.smp);
		ctx->DSGetSamplers(0, num_elements32(ds.smp), ds.smp);
		ctx->HSGetSamplers(0, num_elements32(hs.smp), hs.smp);
		ctx->GSGetSamplers(0, num_elements32(gs.smp), gs.smp);
		ctx->CSGetSamplers(0, num_elements32(cs.smp), cs.smp);

		ctx->VSGetShader((ID3D11VertexShader   **)&vs.shader, 0, 0);
		ctx->PSGetShader((ID3D11PixelShader    **)&ps.shader, 0, 0);
		ctx->DSGetShader((ID3D11DomainShader   **)&ds.shader, 0, 0);
		ctx->HSGetShader((ID3D11HullShader     **)&hs.shader, 0, 0);
		ctx->GSGetShader((ID3D11GeometryShader **)&gs.shader, 0, 0);
		ctx->CSGetShader((ID3D11ComputeShader  **)&cs.shader, 0, 0);

		ctx->CSGetUnorderedAccessViews(0, num_elements32(cs_uav), cs_uav);
		ctx->OMGetRenderTargetsAndUnorderedAccessViews(num_elements32(rtv), rtv, &dsv, 0, num_elements32(ps_uav), ps_uav);
		ctx->OMGetBlendState(&blend, blend_factor, &sample_mask);
		ctx->OMGetDepthStencilState(&depth_stencil, &stencil_ref);

		ctx->IAGetInputLayout(&ia);
		ctx->IAGetIndexBuffer(&ib.buffer, &ib.format, (UINT*)&ib.offset);
		ctx->IAGetPrimitiveTopology(&prim);

		ID3D11Buffer	*buffer[MAX_VB];
		uint32			stride[MAX_VB];
		uint32			offset[MAX_VB];

		ctx->IAGetVertexBuffers(0, MAX_VB, buffer, stride, offset);
		for (int i = 0; i < MAX_VB; i++) {
			vb[i].buffer = buffer[i];
			vb[i].stride = stride[i];
			vb[i].offset = offset[i];
		}

		ctx->SOGetTargets(num_elements32(so_buffer), so_buffer);

		ctx->RSGetState(&rs);
		num_viewport	= num_elements32(viewport);
		num_scissor		= num_elements32(scissor);
		ctx->RSGetViewports(&num_viewport, viewport);
		ctx->RSGetScissorRects(&num_scissor, scissor);
	}
	void Put(ID3D11DeviceContext *ctx) const {
		ctx->VSSetConstantBuffers(0, num_existing(vs.cb), vs.cb);
		ctx->PSSetConstantBuffers(0, num_existing(ps.cb), ps.cb);
		ctx->DSSetConstantBuffers(0, num_existing(ds.cb), ds.cb);
		ctx->HSSetConstantBuffers(0, num_existing(hs.cb), hs.cb);
		ctx->GSSetConstantBuffers(0, num_existing(gs.cb), gs.cb);
		ctx->CSSetConstantBuffers(0, num_existing(cs.cb), cs.cb);

		ctx->VSSetShaderResources(0, num_existing(vs.srv), vs.srv);
		ctx->PSSetShaderResources(0, num_existing(ps.srv), ps.srv);
		ctx->DSSetShaderResources(0, num_existing(ds.srv), ds.srv);
		ctx->HSSetShaderResources(0, num_existing(hs.srv), hs.srv);
		ctx->GSSetShaderResources(0, num_existing(gs.srv), gs.srv);
		ctx->CSSetShaderResources(0, num_existing(cs.srv), cs.srv);

		ctx->VSSetSamplers(0, num_existing(vs.smp), vs.smp);
		ctx->PSSetSamplers(0, num_existing(ps.smp), ps.smp);
		ctx->DSSetSamplers(0, num_existing(ds.smp), ds.smp);
		ctx->HSSetSamplers(0, num_existing(hs.smp), hs.smp);
		ctx->GSSetSamplers(0, num_existing(gs.smp), gs.smp);
		ctx->CSSetSamplers(0, num_existing(cs.smp), cs.smp);

		ctx->VSSetShader((ID3D11VertexShader   *)vs.shader, 0, 0);
		ctx->PSSetShader((ID3D11PixelShader    *)ps.shader, 0, 0);
		ctx->DSSetShader((ID3D11DomainShader   *)ds.shader, 0, 0);
		ctx->HSSetShader((ID3D11HullShader     *)hs.shader, 0, 0);
		ctx->GSSetShader((ID3D11GeometryShader *)gs.shader, 0, 0);
		ctx->CSSetShader((ID3D11ComputeShader  *)cs.shader, 0, 0);

		ctx->CSSetUnorderedAccessViews(0, num_existing(cs_uav), cs_uav, 0);

		ctx->OMSetRenderTargetsAndUnorderedAccessViews(num_existing(rtv), rtv, dsv, 0, num_existing(ps_uav), ps_uav, 0);
		ctx->OMSetBlendState(blend, blend_factor, sample_mask);
		ctx->OMSetDepthStencilState(depth_stencil, stencil_ref);

		ctx->IASetInputLayout(ia);
		ctx->IASetIndexBuffer(ib.buffer, ib.format, ib.offset);
		ctx->IASetPrimitiveTopology(prim);

		ID3D11Buffer	*buffer[MAX_VB];
		uint32			stride[MAX_VB];
		uint32			offset[MAX_VB];

		for (int i = 0; i < MAX_VB; i++) {
			buffer[i] = vb[i].buffer;
			stride[i] = vb[i].stride;
			offset[i] = vb[i].offset;
		}
		ctx->IASetVertexBuffers(0, MAX_VB, buffer, stride, offset);

		ctx->SOSetTargets(num_existing(so_buffer), so_buffer, 0);

		ctx->RSSetState(rs);
		ctx->RSSetViewports(num_viewport, viewport);
		ctx->RSSetScissorRects(num_scissor, scissor);
	}
};

struct RecDeviceContext {
	enum tag : uint16 {
	//ID3D11DeviceContext
		tag_InitialState,
		tag_VSSetConstantBuffers,
		tag_PSSetShaderResources,
		tag_PSSetShader,
		tag_PSSetSamplers,
		tag_VSSetShader,
		tag_DrawIndexed,
		tag_Draw,
		tag_Map,
		tag_Unmap,
		tag_PSSetConstantBuffers,
		tag_IASetInputLayout,
		tag_IASetVertexBuffers,
		tag_IASetIndexBuffer,
		tag_DrawIndexedInstanced,
		tag_DrawInstanced,
		tag_GSSetConstantBuffers,
		tag_GSSetShader,
		tag_IASetPrimitiveTopology,
		tag_VSSetShaderResources,
		tag_VSSetSamplers,
		tag_Begin,
		tag_End,
		tag_SetPredication,
		tag_GSSetShaderResources,
		tag_GSSetSamplers,
		tag_OMSetRenderTargets,
		tag_OMSetRenderTargetsAndUnorderedAccessViews,
		tag_OMSetBlendState,
		tag_OMSetDepthStencilState,
		tag_SOSetTargets,
		tag_DrawAuto,
		tag_DrawIndexedInstancedIndirect,
		tag_DrawInstancedIndirect,
		tag_Dispatch,
		tag_DispatchIndirect,
		tag_RSSetState,
		tag_RSSetViewports,
		tag_RSSetScissorRects,
		tag_CopySubresourceRegion,
		tag_CopyResource,
		tag_UpdateSubresource,
		tag_CopyStructureCount,
		tag_ClearRenderTargetView,
		tag_ClearUnorderedAccessViewUint,
		tag_ClearUnorderedAccessViewFloat,
		tag_ClearDepthStencilView,
		tag_GenerateMips,
		tag_SetResourceMinLOD,
		tag_ResolveSubresource,
		tag_ExecuteCommandList,
		tag_HSSetShaderResources,
		tag_HSSetShader,
		tag_HSSetSamplers,
		tag_HSSetConstantBuffers,
		tag_DSSetShaderResources,
		tag_DSSetShader,
		tag_DSSetSamplers,
		tag_DSSetConstantBuffers,
		tag_CSSetShaderResources,
		tag_CSSetUnorderedAccessViews,
		tag_CSSetShader,
		tag_CSSetSamplers,
		tag_CSSetConstantBuffers,
		tag_ClearState,
		tag_Flush,
		tag_FinishCommandList,
	//ID3D11DeviceContext1
		tag_CopySubresourceRegion1,
		tag_UpdateSubresource1,
		tag_DiscardResource,
		tag_DiscardView ,
		tag_VSSetConstantBuffers1,
		tag_HSSetConstantBuffers1,
		tag_DSSetConstantBuffers1,
		tag_GSSetConstantBuffers1,
		tag_PSSetConstantBuffers1,
		tag_CSSetConstantBuffers1,
		tag_SwapDeviceContextState,
		tag_ClearView,
		tag_DiscardView1,
	//ID3D11DeviceContext2
		tag_UpdateTileMappings,
		tag_CopyTileMappings,
		tag_CopyTiles,
		tag_UpdateTiles,
		tag_ResizeTilePool,
		tag_TiledResourceBarrier,
		tag_SetMarkerInt,
		tag_BeginEventInt,
		tag_EndEvent,
	//ID3D11DeviceContext3
		tag_Flush1,
		tag_SetHardwareProtectionState,
	//ID3D11DeviceContext4
		tag_Signal,
		tag_Wait,

	//internal
		tag_DATA,
		tag_RLE_DATA,
	};
	friend constexpr bool is_compute(tag id) {
		return is_any(id, tag_Dispatch, tag_DispatchIndirect);
	}
	friend constexpr bool is_draw(tag id) {
		return is_any(id, tag_DrawIndexed, tag_Draw, tag_DrawIndexedInstanced, tag_DrawInstanced, tag_DrawAuto, tag_DrawIndexedInstancedIndirect, tag_DrawInstancedIndirect);
	}
	friend constexpr bool is_indexed(tag id) {
		return is_any(id, tag_DrawIndexed, tag_DrawIndexedInstanced, tag_DrawIndexedInstancedIndirect);
	}
	friend constexpr bool is_indirect(tag id) {
		return is_any(id, tag_DrawInstancedIndirect, tag_DrawIndexedInstancedIndirect, tag_DispatchIndirect);
	}
};

struct RecDevice : e_link<RecDevice> {
	enum tag {
		tag_CreateBuffer,
		tag_CreateTexture1D,
		tag_CreateTexture2D,
		tag_CreateTexture3D,
		tag_CreateShaderResourceView,
		tag_CreateUnorderedAccessView,
		tag_CreateRenderTargetView,
		tag_CreateDepthStencilView,
		tag_CreateInputLayout,
		tag_CreateVertexShader,
		tag_CreateGeometryShader,
		tag_CreateGeometryShaderWithStreamOutput,
		tag_CreatePixelShader,
		tag_CreateHullShader,
		tag_CreateDomainShader,
		tag_CreateComputeShader,
		tag_CreateClassLinkage,
		tag_CreateBlendState,
		tag_CreateDepthStencilState,
		tag_CreateRasterizerState,
		tag_CreateSamplerState,
		tag_CreateQuery,
		tag_CreatePredicate,
		tag_CreateCounter,
		tag_CreateDeferredContext,
		tag_SetPrivateData,
		tag_SetPrivateDataInterface,
	//ID3D11Device1
		tag_CreateDeferredContext1,
		tag_CreateBlendState1,
		tag_CreateRasterizerState1,
		tag_CreateDeviceContextState,
	//ID3D11Device2
		tag_CreateDeferredContext2,
	//ID3D11Device3
		tag_CreateTexture2D1,
		tag_CreateTexture3D1,
		tag_CreateRasterizerState2,
		tag_CreateShaderResourceView1,
		tag_CreateUnorderedAccessView1,
		tag_CreateRenderTargetView1,
		tag_CreateQuery1,
		tag_CreateDeferredContext3,
		tag_WriteToSubresource,
	//ID3D11Device4
		tag_RegisterDeviceRemovedEvent,
		tag_UnregisterDeviceRemoved,
	//ID3D11Device5
		tag_OpenSharedFence,
		tag_CreateFence,
	};
};

struct Recording {
	enum TYPE { DEVICE, CONTEXT}	type;
	malloc_block_all				recording;
	Recording() {}
	Recording(TYPE type, memory_block &recording)	: type(type), recording(recording) {}
	Recording(TYPE type, malloc_block &&recording)	: type(type), recording(move(recording)) {}

	template<typename R>	bool read(R &&r) {
		return iso::read(r, type, recording);
	}
	template<typename W>	bool write(W &&w) const	{
		return iso::write(w, type, recording);
	}
};

} // namespace dx11

namespace iso {

	template<> struct TL_fields<dx11::DeviceContext_State::Stage>			: T_type<type_list<
		ID3D11DeviceChild			*,
		ID3D11Buffer				*[8],
		ID3D11ShaderResourceView	*[8],
		ID3D11SamplerState			*[8]
	>> {};
	template<> struct TL_fields<dx11::DeviceContext_State::VertexBuffer>	: T_type<type_list<
		ID3D11Buffer	*,
		uint32,
		uint32
	>> {};
	template<> struct TL_fields<dx11::DeviceContext_State::IndexBuffer>		: T_type<type_list<
		ID3D11Buffer	*,
		DXGI_FORMAT,
		uint32
	>> {};

	template<> struct TL_fields<dx11::DeviceContext_State>					: T_type<type_list<
		as_tuple<dx11::DeviceContext_State::Stage>[6],
//		as_tuple<dx11::DeviceContext_State::Stage>,
//		as_tuple<dx11::DeviceContext_State::Stage>,
//		as_tuple<dx11::DeviceContext_State::Stage>,
//		as_tuple<dx11::DeviceContext_State::Stage>,
//		as_tuple<dx11::DeviceContext_State::Stage>,
//		as_tuple<dx11::DeviceContext_State::Stage>,
		ID3D11UnorderedAccessView	*[D3D11_PS_CS_UAV_REGISTER_COUNT],
		ID3D11UnorderedAccessView	*[D3D11_PS_CS_UAV_REGISTER_COUNT],

		//SO
		ID3D11Buffer				*[D3D11_SO_BUFFER_SLOT_COUNT],
		uint32						[D3D11_SO_BUFFER_SLOT_COUNT],

		//RS
		ID3D11RasterizerState		*,
		UINT						[2],
		D3D11_VIEWPORT				[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX],
		D3D11_RECT					[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX],

		//IA
		ID3D11InputLayout			*,
		as_tuple<dx11::DeviceContext_State::IndexBuffer>,
		as_tuple<dx11::DeviceContext_State::VertexBuffer>[dx11::DeviceContext_State::MAX_VB],
		D3D_PRIMITIVE_TOPOLOGY,

		//OM
		ID3D11DepthStencilView		*,
		ID3D11RenderTargetView		*[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT],
		ID3D11BlendState			*,
		FLOAT						[4],
		UINT,
		ID3D11DepthStencilState		*,
		UINT
	>> {};
}

