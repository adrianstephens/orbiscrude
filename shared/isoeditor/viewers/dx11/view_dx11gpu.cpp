#include "main.h"
#include "graphics.h"
#include "thread.h"
#include "jobs.h"
#include "fibers.h"
#include "vm.h"
#include "disassembler.h"
#include "base/functions.h"

#include "iso/iso.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"

#include "common/shader.h"

#include "windows/filedialogs.h"
#include "windows/text_control.h"
#include "windows/dib.h"
#include "extra/indexer.h"

#include "..\dx_shared\dx_gpu.h"
#include "..\dx_shared\view_dxbc.h"
#include "..\dx_shared\dx_fields.h"

#include "filetypes/3d/model_utils.h"
#include "view_dx11gpu.rc.h"
#include "resource.h"
#include "hook_com.h"
#include "stack_dump.h"
#include "dx11/dx11_record.h"
#include "dx/dxgi_read.h"
#include "dx/spdb.h"
#include "dx/sim_dxbc.h"

#include <d3dcompiler.h>

#define	IDR_MENU_DX11GPU	"IDR_MENU_DX11GPU"
using namespace app;
using namespace dx11;

template<> struct fields<D3D11_COUNTER_TYPE> : value_field<D3D11_COUNTER_TYPE> {};

namespace iso {
template<typename T> struct fields<RecView<T> >		{ static field f[]; };
template<typename T> field fields<RecView<T> >::f[] = {
	{"resource",	0, 64, field::MODE_CUSTOM, 0, sHex},
	{"desc",		64, 0, field::MODE_RELPTR, 0, (const char**)fields<T>::f},
	0
};
template<> field fields<const_memory_block>::f[] = {
	{"data",		0, 64, field::MODE_CUSTOM, 0, sHex},
	0
};
//template<typename T> struct value_field<RecView<T>*>		{ static field f[]; };
//template<typename T> field value_field<RecView<T>*>::f[] =	{ field::call<RecView<T>, field::MODE_POINTER>(0, 0), 0 };

//field value_field<D3D11_SAMPLER_DESC*>::f[] = { field::call_ptr(fields<D3D11_SAMPLER_DESC>::f, 0), 0 };
template<> struct field_names<D3D11_FENCE_FLAG>	{ static field_bit s[];	};
}

const Cursor DX11cursors[] {
	Cursor::LoadSystem(IDC_HAND),
	CompositeCursor(Cursor::LoadSystem(IDC_HAND), Cursor::Load(IDR_OVERLAY_ADD, 0)),
};

//-----------------------------------------------------------------------------
//	DX11Assets
//-----------------------------------------------------------------------------
dx::SHADERSTAGE GetStage(RecItem::TYPE type) {
	switch (type) {
		case RecItem::VertexShader:		return dx::VS;
		case RecItem::HullShader:		return dx::HS;
		case RecItem::DomainShader:		return dx::DS;
		case RecItem::GeometryShader:	return dx::GS;
		case RecItem::PixelShader:		return dx::PS;
		case RecItem::ComputeShader:	return dx::CS;
		default: return (dx::SHADERSTAGE)-1;
	}
}

struct DX11Assets {
	struct use {
		uint32	index:31, written:1;
		use(uint32 index, bool written) : index(index), written(written) {}
	};

	struct BatchRecord {
		uint32		addr;
		uint32		op:6, use_offset;
		const void	*data;
		uint64		timestamp;
		D3D11_QUERY_DATA_PIPELINE_STATISTICS	stats;
		ID3D11RenderTargetView					*rtv;

		union {
			struct {		// for draw batch
				uint32	prim:8, num_instances:18;
				uint32	num_verts;
			};
			uint32	dim[3];	// for compute batch
		};
		BatchRecord(uint16 op, uint32 addr, uint32 use_offset, const void *data) : addr(addr), op(op), use_offset(use_offset), data(data), timestamp(0), rtv(0) {}	
		D3D_PRIMITIVE_TOPOLOGY	Prim()		const { return (D3D_PRIMITIVE_TOPOLOGY)prim; }
		uint64					Duration()	const { return this[1].timestamp - timestamp; }
	};

	struct MarkerRecord {
		string	s;
		uint64	addr;
		uint32	colour:24, flags:8;
	};

	struct ItemRecord : RecItem2 {
		ISO_ptr64<void>		p;
		ItemRecord(TYPE type, string16 &&name, ID3D11DeviceChild *obj) : RecItem2(type, move(name), obj) {}
		string				GetName()		const	{ return str8(name); }
	};

	struct ShaderRecord {
		ItemRecord			*item;
		dx::SHADERSTAGE		stage;
		string				entry;
		string				file;

		ShaderRecord(ItemRecord *item, dx::SHADERSTAGE stage) : item(item), stage(stage) {}

		string				GetName()		const { return item->name; }
		uint64				GetSize()		const { return item->info.length(); }
		const dx::DXBC*		DXBC()			const { return item->info; }
		const_memory_block	GetUCode()		const { dx::DXBC::UcodeHeader header; return DXBC()->GetUCode(header); }
		uint64				GetUCodeAddr()	const { return GetUCode() - (const char*)item->info; }
	};
	
	dynamic_array<MarkerRecord>			markers;
	dynamic_array<use>					uses;
	dynamic_array<BatchRecord>			batches;

	order_array<ItemRecord>				items;
	dynamic_array<ShaderRecord>			shaders;
	hash_map<uint64, ItemRecord*>		item_map;

	void AddUse(const ItemRecord *item, bool written = false) {
		uses.emplace_back(GetIndex(item), written);
		if (is_view(item->type)) {
			const RecView<D3D11_SHADER_RESOURCE_VIEW_DESC>	*r = item->info;
			AddUse(r->resource, is_any(item->type, RecItem::RenderTargetView, RecItem::DepthStencilView, RecItem::UnorderedAccessView));
		}
	}

	void AddUse(ID3D11DeviceChild *child, bool written = false) {
		if (ItemRecord *item = FindItem(child))
			AddUse(item, written);
	}

	BatchRecord*	AddBatch(uint16 op, uint32 addr, uint32 uses_start, const void *data) {
		return &batches.emplace_back(op, addr, uses_start, data);
	}

	void			PushMarker(const string &s, uint64 addr) {
		MarkerRecord	&m	= markers.push_back();
		m.s			= s;
		m.addr		= addr;
	}
	void			PopMarker(uint64 addr) {
		MarkerRecord	&m	= markers.push_back();
		m.addr		= addr;
	}

	ShaderRecord&	AddShader(dx::SHADERSTAGE stage, ItemRecord &item) {
		auto &r = shaders.emplace_back(&item, stage);

		if (auto spdb = r.DXBC()->GetBlob(dx::DXBC::ShaderPDB)) {
			item.name	= GetPDBFirstFilename(memory_reader(spdb));
			/*
			auto	parsed	= ParsedSPDB(memory_reader(spdb), con->files, shader_path);
			if (auto start = parsed.locations.begin()) {
				r.entry	= parsed.entry;
				r.file		= parsed.files[start->file]->name;
				item.name	= to_string(parsed.entry << '(' << parsed.files[start->file]->name << ')');
			}
			*/
		}
		return r;
	}

	ItemRecord&	AddItem(dx11::RecItem::TYPE type, string16 &&name, malloc_block &&info, ID3D11DeviceChild *obj) {
		auto	&p	= items.emplace_back(type, move(name), obj);
		item_map[(uint64)obj] = &p;
		p.info	= info.detach();
		switch (type) {
			case RecItem::VertexShader:		AddShader(dx::VS, p); break;
			case RecItem::HullShader:		AddShader(dx::HS, p); break;
			case RecItem::DomainShader:		AddShader(dx::DS, p); break;
			case RecItem::GeometryShader:	AddShader(dx::GS, p); break;
			case RecItem::PixelShader:		AddShader(dx::PS, p); break;
			case RecItem::ComputeShader:	AddShader(dx::CS, p); break;
		}
		return p;
	}
	void	AddState(const DeviceContext_State *state);

	int		GetBatchByTime(uint64 timestamp) const {
		return first_not(batches, [timestamp](const BatchRecord &r) { return r.timestamp < timestamp; }) - batches;
	}
	int		GetBatchNumber(uint32 addr) const {
		return first_not(batches, [addr](const BatchRecord &r) { return r.addr < addr; }) - batches;
	}

	range<use*>	GetUsage(uint32 from_batch, uint32 to_batch) const {
		if (from_batch > to_batch)
			swap(from_batch, to_batch);
		return make_range(uses + batches[from_batch].use_offset, uses + batches[min(to_batch + 1, batches.size())].use_offset);
	}
	range<use*>	GetUsage(uint32 batch) const {
		return GetUsage(batch, batch);
	}

	template<typename T> auto&	GetTable();

	template<typename T> int GetIndex(const T *e) {
		return GetTable<T>().index_of(e);
	}

	ItemRecord*		FindItem(ID3D11DeviceChild *obj) const {
		if (auto *p = item_map.check(uint64(obj)))
			return *p;
		return 0;
	}
	ShaderRecord*	FindShader(ID3D11DeviceChild *obj) {
		for (auto &i : shaders) {
			if (i.item->obj == obj)
				return &i;
		}
		return 0;
	}

	template<typename T, typename U> T *FindByRecord(const U *rec) {
		for (auto &i : GetTable<T>()) {
			if (i == *rec)
				return &i;
		}
		return 0;
	}
};

template<> auto&	DX11Assets::GetTable<DX11Assets::ShaderRecord>()	{ return shaders; }
template<> auto&	DX11Assets::GetTable<DX11Assets::ItemRecord>()		{ return items; }
template<typename T> uint32 GetSize(const T &t)				{ return (uint32)t.GetSize(); }

void DX11Assets::AddState(const DeviceContext_State *state) {
	if (state->dsv && !FindItem(state->dsv))
		AddItem(dx11::RecItem::DepthStencilView, 0, empty, state->dsv);

	for (auto &i : state->rtv) {
		if (i && !FindItem(i))
			AddItem(dx11::RecItem::RenderTargetView, 0, empty, i);
	}
}

template<> const char *field_names<RecItem::TYPE>::s[]	= {
	"Unknown",
	"DepthStencilState",
	"BlendState",
	"RasterizerState",
	"Buffer",
	"Texture1D",
	"Texture2D",
	"Texture3D",
	"ShaderResourceView",
	"RenderTargetView",
	"DepthStencilView",
	"UnorderedAccessView",
	"VertexShader",
	"HullShader",
	"DomainShader",
	"GeometryShader",
	"PixelShader",
	"ComputeShader",
	"InputLayout",
	"SamplerState",
	"Query",
	"Predicate",
	"Counter",
	"ClassInstance",
	"ClassLinkage",
	"CommandList",
	"DeviceContextState",
	"DeviceContext",
	"Device",
	"Fence",
};

struct INPUT_LAYOUT {
	rel_counted<const D3D11_INPUT_ELEMENT_DESC_rel,0> desc;
	UINT num;
};

template<> field fields<INPUT_LAYOUT>::f[] = {
#undef S
#define	S INPUT_LAYOUT
	_MAKE_FIELD_IDX(S, 0, num),
	_MAKE_FIELD_IDX(S, 1, desc),
	0
};

template<> field	fields<DX11Assets::ItemRecord>::f[] = {
	field::make("type",	&DX11Assets::ItemRecord::type),
	field::make("obj",	&DX11Assets::ItemRecord::obj),
	field::call_union<
		void,						//Unknown
		D3D11_DEPTH_STENCIL_DESC*,	//DepthStencilState
		D3D11_BLEND_DESC1*,			//BlendState
		D3D11_RASTERIZER_DESC1*,	//RasterizerState
	
		D3D11_BUFFER_DESC*,			//Buffer
		D3D11_TEXTURE1D_DESC*,		//Texture1D
		D3D11_TEXTURE2D_DESC*,		//Texture2D
		D3D11_TEXTURE3D_DESC*,		//Texture3D
	
		RecView<D3D11_SHADER_RESOURCE_VIEW_DESC>*,	//ShaderResourceView
		RecView<D3D11_RENDER_TARGET_VIEW_DESC>*,	//RenderTargetView
		RecView<D3D11_DEPTH_STENCIL_VIEW_DESC>*,	//DepthStencilView
		RecView<D3D11_UNORDERED_ACCESS_VIEW_DESC>*,	//UnorderedAccessView
	
		void,						//VertexShader
		void,						//HullShader
		void,						//DomainShader
		void,						//GeometryShader
		void,						//PixelShader
		void,						//ComputeShader

		INPUT_LAYOUT*,				//InputLayout
		D3D11_SAMPLER_DESC*,		//SamplerState
		D3D11_QUERY_DESC*,			//Query
		D3D11_QUERY_DESC*,			//Predicate
		D3D11_COUNTER_DESC*,		//Counter
		void,						//ClassInstance
		void,						//ClassLinkage
		void,						//CommandList
		void,						//DeviceContextState
		void,						//DeviceContext
		void,						//Device
		void						//Fence
	>(0, T_get_member_offset(&DX11Assets::ItemRecord::info) * 8, 2),
	0,
};

template<> field	fields<DX11Assets::ShaderRecord>::f[] = {
	field::make("stage",	&DX11Assets::ShaderRecord::stage),
	field::make("entry",	&DX11Assets::ShaderRecord::entry),
	field::make("file",		&DX11Assets::ShaderRecord::file),
	0,
};

template<> field	fields<DeviceContext_State::VertexBuffer>::f[] = {
	field::make("buffer",	&DeviceContext_State::VertexBuffer::buffer),
	field::make("stride",	&DeviceContext_State::VertexBuffer::stride),
	field::make("offset",	&DeviceContext_State::VertexBuffer::offset),
	0,
};

template<> field	fields<DeviceContext_State::IndexBuffer>::f[] = {
	field::make("buffer",	&DeviceContext_State::IndexBuffer::buffer),
	field::make("format",	&DeviceContext_State::IndexBuffer::format),
	field::make("offset",	&DeviceContext_State::IndexBuffer::offset),
	0,
};

//-----------------------------------------------------------------------------
//	DX11State
//-----------------------------------------------------------------------------


struct memory_block1 : const_memory_block {
	bool	owned;
	memory_block1() : owned(false) {}
	memory_block1(const const_memory_block &b)	: const_memory_block(b), owned(false) {}
	memory_block1(malloc_block &&b)				: const_memory_block(b.detach()), owned(true) {}
	memory_block1(memory_block1 &&b)			{ raw_copy(b, *this); b.p = 0; } 
	memory_block1& operator=(memory_block1 &&b)	{ raw_swap(*this, b); return *this; }
	~memory_block1() { if (owned) iso::free(unconst(p)); }
};
	
void ApplyRLE(memory_block1 &m, const const_memory_block &rle) {
	if (!m.owned)
		m = malloc_block(m);

	uint32	*dest	= unconst((const uint32*)m);
	for (const uint32 *p = rle; p != rle.end();) {
		uint32	token	= *p++;
		uint32	len		= token >> 16;
		dest += token & 0xffff;
		if (dest + len > m.end())
			break;
		memcpy(dest, p, len * 4);
		p		+= len;
		dest	+= len;
	}
}

struct DeviceContext_State2 : DeviceContext_State {
	RecDeviceContext::tag	draw_id;
	uint32					num_rtv;
	ID3D11Buffer			*IndirectBuffer;
	uint32					IndirectOffset;
	uint32					VertexCount, BaseVertex, StartIndex;
	uint32					InstanceCount, StartInstance;
	uint3p					ThreadGroupCount;

	DeviceContext_State2()	{ Reset(); }
	void		Reset()		{ clear(*this); }
};

struct DX11State : DeviceContext_State2 {
	typedef	hash_map<tagged_pointer<ID3D11Resource>, memory_block1>	mods_t;
	mods_t		mods;

	const_memory_block	GetModdedData(DX11Assets::ItemRecord *item) {
		return mods[tagged_pointer<ID3D11Resource>((ID3D11Resource*)item->obj, 0)].or_default(item->GetData());
	}
	TypedBuffer		GetModded(const DX11Assets *assets, ID3D11Resource *res, const C_type *type, uint32 stride, uint32 offset, uint32 num) {
		if (auto *item = assets->FindItem(res))
			return TypedBuffer(GetModdedData(item).slice(offset, stride * num), stride, type);
		return TypedBuffer();
	}

	bool		IsIndexed() const {
		return is_indexed(draw_id);
	}

	indices	GetIndexing(const DX11Assets *assets) const {
		if (IsIndexed()) {
			if (DX11Assets::ItemRecord	*item = assets->FindItem(ib.buffer)) {
				const D3D11_BUFFER_DESC	*desc	= item->info;
				uint32					stride	= ib.format == DXGI_FORMAT_R16_UINT ? 2 : 4;
				return indices(desc + 1, stride, ib.offset / stride + StartIndex, VertexCount);
			}
			ISO_ASSERT(false);
		}
		return indices(0, 0, BaseVertex, VertexCount);
	}

	Topology2 GetTopology() const {
		return {dx::GetTopology(prim), InstanceCount > 1 ? VertexCount : 0};
	}

	BackFaceCull CalcCull(bool flipped) const {
		return BFC_NONE;
	}

	void		Reset() {
		DeviceContext_State2::Reset();
		mods.clear();
	}
};

struct DX11StateParser {
	DX11State				&state;
	ID3DDeviceContextState	*curr_state;
	const_memory_block		last_data;
	bool					rle_data;
	hash_map<ID3DDeviceContextState*, DeviceContext_State>	states;

	DX11StateParser(DX11State &state) : state(state), curr_state(0), last_data(none) {}

	bool	Process1(DX11Assets *assets, const COMRecording::header *h) {
		const uint16 *p = h->data();
		switch (h->id) {
			case RecDeviceContext::tag_InitialState:
				static_cast<DeviceContext_State&>(state) = *(DeviceContext_State*)p;
				break;
			case RecDeviceContext::tag_VSSetConstantBuffers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp) {
				copy_n(pp.begin(), state.vs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_PSSetShaderResources: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11ShaderResourceView*,1> pp) {
				copy_n(pp.begin(), state.ps.srv + slot, num);
			}); break;
			case RecDeviceContext::tag_PSSetShader: COMParse(p, [this](ID3D11PixelShader *shader, counted<ID3D11ClassInstance*,2> pp, UINT numc) {
				state.ps.shader = shader;
			}); break;
			case RecDeviceContext::tag_PSSetSamplers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11SamplerState*,1> pp) {
				copy_n(pp.begin(), state.ps.smp + slot, num);
			}); break;
			case RecDeviceContext::tag_VSSetShader: COMParse(p, [this](ID3D11VertexShader *shader, counted<ID3D11ClassInstance*,2> pp, UINT numc) {
				state.vs.shader = shader;
			}); break;
			case RecDeviceContext::tag_DrawIndexed: COMParse(p, [this](UINT IndexCount, UINT StartIndex, INT BaseVertex) {
				state.VertexCount			= IndexCount;
				state.StartIndex			= StartIndex;
				state.BaseVertex			= BaseVertex;
				state.StartInstance			= 0;
				state.InstanceCount			= 1;
			}); return true;
			case RecDeviceContext::tag_Draw: COMParse(p, [this](UINT VertexCount, UINT StartVertex) {
				state.VertexCount			= VertexCount;
				state.BaseVertex			= StartVertex;
				state.StartInstance			= 0;
				state.InstanceCount			= 1;
				}); return true;
			case RecDeviceContext::tag_Map: COMParse(p, [](ID3D11Resource *rsrc, UINT sub, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *mapped) {
			}); break;
			case RecDeviceContext::tag_Unmap: COMParse(p, [this, assets](ID3D11Resource *rsrc, UINT sub) {
				auto	&m = state.mods[tagged_pointer<ID3D11Resource>(rsrc,sub)].put();
				if (rle_data) {
					if (!m) {
						if (auto *rec = assets->FindItem(rsrc))
							m	= rec->GetSubResourceData(sub);
					}
					ApplyRLE(m, last_data);
				} else {
					m = exchange(last_data, none);
					//mods[tagged_pointer<ID3D11Resource>(rsrc,sub)] = exchange(last_data, none);
				}
			}); break;
			case RecDeviceContext::tag_PSSetConstantBuffers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp) {
				copy_n(pp.begin(), state.ps.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_IASetInputLayout: COMParse(p, [this](ID3D11InputLayout *pInputLayout) {
				state.ia = pInputLayout;
			}); break;
			case RecDeviceContext::tag_IASetVertexBuffers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp, counted<const UINT,1> strides, counted<const UINT,1> offsets) {
				for (int i = 0; i < num; i++) {
					auto	&d = state.vb[i + slot];
					d.buffer	= pp[i];
					d.stride	= strides[i];
					d.offset	= offsets[i];
				}
			}); break;
			case RecDeviceContext::tag_IASetIndexBuffer: COMParse(p, [this](ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset) {
				state.ib.buffer	= pIndexBuffer;
				state.ib.format	= Format;
				state.ib.offset	= Offset;
			}); break;
			case RecDeviceContext::tag_DrawIndexedInstanced: COMParse(p, [this](UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndex, INT BaseVertex, UINT StartInstance) {
				state.VertexCount			= IndexCountPerInstance;
				state.InstanceCount			= InstanceCount;
				state.StartIndex			= StartIndex;
				state.BaseVertex			= BaseVertex;
				state.StartInstance			= StartInstance;
			}); return true;
			case RecDeviceContext::tag_DrawInstanced: COMParse(p, [this](UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertex, UINT StartInstance) {
				state.VertexCount			= VertexCountPerInstance;
				state.InstanceCount			= InstanceCount;
				state.BaseVertex			= StartVertex;
				state.StartInstance			= StartInstance;
			}); return true;
			case RecDeviceContext::tag_GSSetConstantBuffers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp) {
				copy_n(pp.begin(), state.gs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_GSSetShader: COMParse(p, [this](ID3D11GeometryShader *shader, counted<ID3D11ClassInstance*,2> pp, UINT numc) {
				state.gs.shader = shader;
			}); break;
			case RecDeviceContext::tag_IASetPrimitiveTopology: COMParse(p, [this](D3D11_PRIMITIVE_TOPOLOGY Topology) {
				state.prim	= Topology;
			}); break;
			case RecDeviceContext::tag_VSSetShaderResources: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11ShaderResourceView*,1> pp) {
				copy_n(pp.begin(), state.vs.srv + slot, num);
			}); break;
			case RecDeviceContext::tag_VSSetSamplers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11SamplerState*,1> pp) {
				copy_n(pp.begin(), state.vs.smp + slot, num);
			}); break;
			case RecDeviceContext::tag_Begin: COMParse(p, [](ID3D11Asynchronous *async) {
			}); break;
			case RecDeviceContext::tag_End: COMParse(p, [](ID3D11Asynchronous *async) {
			}); break;
			case RecDeviceContext::tag_SetPredication: COMParse(p, [](ID3D11Predicate *pred, BOOL PredicateValue) {
			}); break;
			case RecDeviceContext::tag_GSSetShaderResources: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11ShaderResourceView*,1> pp) {
				copy_n(pp.begin(), state.gs.srv + slot, num);
			}); break;
			case RecDeviceContext::tag_GSSetSamplers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11SamplerState*,1> pp) {
				copy_n(pp.begin(), state.gs.smp + slot, num);
			}); break;
			case RecDeviceContext::tag_OMSetRenderTargets: COMParse(p, [this](UINT num, counted<ID3D11RenderTargetView *const,0> ppRTV, ID3D11DepthStencilView *pDSV) {
				copy_n(ppRTV.begin(), state.rtv, state.num_rtv = num);
				while (state.num_rtv && !state.rtv[state.num_rtv - 1])
					--state.num_rtv;
				state.dsv	= pDSV;
			}); break;
			case RecDeviceContext::tag_OMSetRenderTargetsAndUnorderedAccessViews: COMParse(p, [this](UINT num, counted<ID3D11RenderTargetView *const, 0> ppRTV, ID3D11DepthStencilView *pDSV, UINT UAVStartSlot, UINT NumUAVs, counted<ID3D11UnorderedAccessView *const,4> ppUAV, counted<const UINT,4> pUAVInitialCounts) {
				if (num <= num_elements(state.rtv))
					copy_n(ppRTV.begin(), state.rtv, state.num_rtv = num);
				copy_n(ppUAV.begin(), state.ps_uav + UAVStartSlot, NumUAVs);
				while (state.num_rtv && !state.rtv[state.num_rtv - 1])
					--state.num_rtv;
				state.dsv	= pDSV;
			}); break;
			case RecDeviceContext::tag_OMSetBlendState: COMParse(p, [this](ID3D11BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask) {
				state.blend		= pBlendState;
				if (BlendFactor)
					memcpy(state.blend_factor, BlendFactor, sizeof(FLOAT[4]));
				state.sample_mask = SampleMask;
			}); break;
			case RecDeviceContext::tag_OMSetDepthStencilState: COMParse(p, [this](ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef) {
				state.depth_stencil	= pDepthStencilState;
				state.stencil_ref		= StencilRef;
			}); break;
			case RecDeviceContext::tag_SOSetTargets: COMParse(p, [this](UINT num, counted<ID3D11Buffer*,0> pp, counted<const UINT,0> offsets) {
				copy_n(pp.begin(), state.so_buffer, num);
				copy_n(offsets.begin(), state.so_offset, num);
			}); break;
			case RecDeviceContext::tag_DrawAuto: COMParse(p, []() {
			}); return true;
			case RecDeviceContext::tag_DrawIndexedInstancedIndirect: COMParse(p, [this](ID3D11Buffer *buffer, UINT offset) {
				state.IndirectBuffer = buffer;
				state.IndirectOffset = offset;
			}); return true;
			case RecDeviceContext::tag_DrawInstancedIndirect: COMParse(p, [this](ID3D11Buffer *buffer, UINT offset) {
				state.IndirectBuffer = buffer;
				state.IndirectOffset = offset;
			}); return true;
			case RecDeviceContext::tag_Dispatch: COMParse(p, [this](UINT x, UINT y, UINT z) {
				state.ThreadGroupCount = uint32x3{x, y, z};
			}); return true;
			case RecDeviceContext::tag_DispatchIndirect: COMParse(p, [this](ID3D11Buffer *buffer, UINT AlignedByteOffsetForArgs) {
				state.IndirectBuffer = buffer;
				state.IndirectOffset = AlignedByteOffsetForArgs;
			}); return true;
			case RecDeviceContext::tag_RSSetState: COMParse(p, [this](ID3D11RasterizerState *pRasterizerState) {
				state.rs = pRasterizerState;
			}); break;
			case RecDeviceContext::tag_RSSetViewports: COMParse(p, [this](UINT num, counted<const D3D11_VIEWPORT,0> viewports) {
				copy_n(viewports.begin(), state.viewport, state.num_viewport = num);
			}); break;
			case RecDeviceContext::tag_RSSetScissorRects: COMParse(p, [this](UINT num, counted<const D3D11_RECT,0> rects) {
				copy_n(rects.begin(), state.scissor, state.num_scissor = num);
			}); break;
			case RecDeviceContext::tag_CopySubresourceRegion: COMParse(p, [](ID3D11Resource *dst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *src, UINT SrcSubresource, const D3D11_BOX *pSrcBox) {
			});  return true;
			case RecDeviceContext::tag_CopyResource: COMParse(p, [](ID3D11Resource *dst, ID3D11Resource *src) {
			});  return true;
			case RecDeviceContext::tag_UpdateSubresource: COMParse(p, [this](ID3D11Resource *dst, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
				state.mods[tagged_pointer<ID3D11Resource>(dst,DstSubresource)] = exchange(last_data, none);
			}); return true;
			case RecDeviceContext::tag_CopyStructureCount: COMParse(p, [](ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView) {
			}); break;
			case RecDeviceContext::tag_ClearRenderTargetView: COMParse(p, [](ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) {
			});  return true;
			case RecDeviceContext::tag_ClearUnorderedAccessViewUint: COMParse(p, [](ID3D11UnorderedAccessView *uav, const UINT Values[4]) {
			});  return true;
			case RecDeviceContext::tag_ClearUnorderedAccessViewFloat: COMParse(p, [](ID3D11UnorderedAccessView *uav, const FLOAT Values[4]) {
			});  return true;
			case RecDeviceContext::tag_ClearDepthStencilView: COMParse(p, [](ID3D11DepthStencilView *dsv, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
			});  return true;
			case RecDeviceContext::tag_GenerateMips: COMParse(p, [](ID3D11ShaderResourceView *pShaderResourceView) {
			});  return true;
			case RecDeviceContext::tag_SetResourceMinLOD: COMParse(p, [](ID3D11Resource *rsrc, FLOAT MinLOD) {
			}); break;
			case RecDeviceContext::tag_ResolveSubresource: COMParse(p, [](ID3D11Resource *dst, UINT DstSubresource, ID3D11Resource *src, UINT SrcSubresource, DXGI_FORMAT Format) {
			}); break;
			case RecDeviceContext::tag_ExecuteCommandList: COMParse(p, [](ID3D11CommandList *pCommandList, BOOL RestoreContextState) {
			});  return true;
			case RecDeviceContext::tag_HSSetShaderResources: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11ShaderResourceView*,1> pp) {
				copy_n(pp.begin(), state.hs.srv + slot, num);
			}); break;
			case RecDeviceContext::tag_HSSetShader: COMParse(p, [this](ID3D11HullShader *shader, counted<ID3D11ClassInstance*,2> pp, UINT numc) {
				state.hs.shader = shader;
			}); break;
			case RecDeviceContext::tag_HSSetSamplers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11SamplerState*,1> pp) {
				copy_n(pp.begin(), state.hs.smp + slot, num);
			}); break;
			case RecDeviceContext::tag_HSSetConstantBuffers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp) {
				copy_n(pp.begin(), state.hs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_DSSetShaderResources: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11ShaderResourceView*,1> pp) {
				copy_n(pp.begin(), state.ds.srv + slot, num);
			}); break;
			case RecDeviceContext::tag_DSSetShader: COMParse(p, [this](ID3D11DomainShader *shader, counted<ID3D11ClassInstance*,2> pp, UINT numc) {
				state.ds.shader = shader;
			}); break;
			case RecDeviceContext::tag_DSSetSamplers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11SamplerState*,1> pp) {
				copy_n(pp.begin(), state.ds.smp + slot, num);
			}); break;
			case RecDeviceContext::tag_DSSetConstantBuffers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp) {
				copy_n(pp.begin(), state.ds.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_CSSetShaderResources: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11ShaderResourceView*,1> pp) {
				copy_n(pp.begin(), state.cs.srv + slot, num);
			}); break;
			case RecDeviceContext::tag_CSSetUnorderedAccessViews: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11UnorderedAccessView *const,1> pp, counted<const UINT,1> pUAVInitialCounts) {
				copy_n(pp.begin(), state.cs_uav + slot, num);
			}); break;
			case RecDeviceContext::tag_CSSetShader: COMParse(p, [this](ID3D11ComputeShader *shader, counted<ID3D11ClassInstance*,2> pp, UINT numc) {
				state.cs.shader = shader;
			}); break;
			case RecDeviceContext::tag_CSSetSamplers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11SamplerState*,1> pp) {
				copy_n(pp.begin(), state.cs.smp + slot, num);
			}); break;
			case RecDeviceContext::tag_CSSetConstantBuffers: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp) {
				copy_n(pp.begin(), state.cs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_ClearState: COMParse(p, []() {
			}); break;
			case RecDeviceContext::tag_Flush: COMParse(p, []() {
			}); break;
			case RecDeviceContext::tag_FinishCommandList: COMParse(p, [](BOOL RestoreDeferredContextState, ID3D11CommandList **pp) {
			});  return true;
		//ID3D11DeviceContext1
			case RecDeviceContext::tag_CopySubresourceRegion1: COMParse(p, [](ID3D11Resource *dst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *src, UINT SrcSubresource, const D3D11_BOX *pSrcBox, UINT CopyFlags) {
			}); break;
			case RecDeviceContext::tag_UpdateSubresource1: COMParse(p, [this](ID3D11Resource *dst, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags) {
				state.mods[tagged_pointer<ID3D11Resource>(dst,DstSubresource)] = exchange(last_data, none);
			}); break;
			case RecDeviceContext::tag_DiscardResource: COMParse(p, [](ID3D11Resource *rsrc) {
			}); break;
			case RecDeviceContext::tag_DiscardView: COMParse(p, [](ID3D11View *pResourceView) {
			}); break;
			case RecDeviceContext::tag_VSSetConstantBuffers1: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp, const UINT *cnst, const UINT *numc) {
				copy_n(pp.begin(), state.vs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_HSSetConstantBuffers1: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp, const UINT *cnst, const UINT *numc) {
				copy_n(pp.begin(), state.hs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_DSSetConstantBuffers1: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp, const UINT *cnst, const UINT *numc) {
				copy_n(pp.begin(), state.ds.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_GSSetConstantBuffers1: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp, const UINT *cnst, const UINT *numc) {
				copy_n(pp.begin(), state.gs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_PSSetConstantBuffers1: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp, const UINT *cnst, const UINT *numc) {
				copy_n(pp.begin(), state.ps.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_CSSetConstantBuffers1: COMParse(p, [this](UINT slot, UINT num, counted<ID3D11Buffer*,1> pp, const UINT *cnst, const UINT *numc) {
				copy_n(pp.begin(), state.cs.cb + slot, num);
			}); break;
			case RecDeviceContext::tag_SwapDeviceContextState: COMParse(p, [this](ID3DDeviceContextState *state, ID3DDeviceContextState **pp) {
				if (pp && curr_state)
					ISO_ASSERT(*pp == curr_state);

				ID3DDeviceContextState	*prev = pp ? *pp : curr_state;
				states[prev]	= this->state;
				curr_state		= state;
				static_cast<DeviceContext_State&>(this->state) = states[state];
			}); break;
			case RecDeviceContext::tag_ClearView: COMParse(p, [](ID3D11View *pView, const FLOAT Color[4], const D3D11_RECT *pRect, UINT num) {
			}); break;
			case RecDeviceContext::tag_DiscardView1: COMParse(p, [](ID3D11View *pResourceView, const D3D11_RECT *pRects, UINT num) {
			}); break;
		//ID3D11DeviceContext2
			case RecDeviceContext::tag_UpdateTileMappings:
			case RecDeviceContext::tag_CopyTileMappings:
			case RecDeviceContext::tag_CopyTiles:
			case RecDeviceContext::tag_UpdateTiles:
			case RecDeviceContext::tag_ResizeTilePool:
			case RecDeviceContext::tag_TiledResourceBarrier:
			case RecDeviceContext::tag_SetMarkerInt:
			case RecDeviceContext::tag_BeginEventInt:
			case RecDeviceContext::tag_EndEvent:
		//ID3D11DeviceContext3
			case RecDeviceContext::tag_Flush1:
			case RecDeviceContext::tag_SetHardwareProtectionState:
		//ID3D11DeviceContext4
			case RecDeviceContext::tag_Signal:
			case RecDeviceContext::tag_Wait:
				break;
		//internal
			case RecDeviceContext::tag_DATA:
				last_data = h->block();
				rle_data	= false;
				break;
			case RecDeviceContext::tag_RLE_DATA:
				last_data	= h->block();
				rle_data	= true;
				break;
		}
		return false;
	}
	bool	Process(DX11Assets *assets, const COMRecording::header *h) {
		bool	r = Process1(assets, h);
		if (r)
			state.draw_id = (RecDeviceContext::tag)h->id;
		return r;
	}
};

//-----------------------------------------------------------------------------
//	DX11Replay
//-----------------------------------------------------------------------------
namespace iso {
uintptr_t hash(const ID3D11DeviceChild *k) {
	return (uintptr_t)k / 16;
}
}

template<> struct hash_s<ID3D11DeviceChild*> {
	static inline uintptr_t f(const ID3D11DeviceChild *t) { return (uintptr_t)t / 16; }
};

struct DX11Replay : COMReplay<DX11Replay> {
	struct mapping : D3D11_MAPPED_SUBRESOURCE {
		D3D11_MAPPED_SUBRESOURCE	*orig;
		ID3D11Resource				*rsrc;
		uint32						sub;
	};
	dynamic_array<mapping>	open_mappings;

	com_ptr<ID3D11Device1>				device;
	com_ptr<ID3D11DeviceContext1>		ctx;
	D3D_FEATURE_LEVEL					feature_level;
	hash_map<void*,arbitrary_ptr>		obj2local;
	const_memory_block					last_data;
	bool								rle_data;

	DX11Replay*	operator->() {
		return this;
	}
	template<typename T> T* lookup(T *p) {
		if (!p)
			return p;
		if (T *p2 = obj2local[p].or_default(nullptr))
			return p2;
		abort = true;
		return 0;
	}
	const void* lookup(const void *p)	{
		return last_data;
	}

	bool	Init(IDXGIAdapter *adapter = 0);

	bool	Process(const COMRecording::header *h);

	struct ResourceData {
		struct Block : D3D11_SUBRESOURCE_DATA {
			void set(const void *p, uint32 pitch1, uint32 pitch2 = 0) {
				pSysMem				= p;
				SysMemPitch			= pitch1;
				SysMemSlicePitch	= pitch2;
			}
		};

		Block			*blocks;
		ResourceData() : blocks(0) {}
		template<typename D> ResourceData(const D *desc, size_t size) : blocks(0) {
			if (size > sizeof(*desc)) {
				blocks = new Block[NumSubResources(desc)];
				GetData(desc);
			}
		}

		~ResourceData() { delete[] blocks; }

		void GetData(const D3D11_BUFFER_DESC	*desc)	{
			blocks[0].set(desc + 1, 0);
		}
		void GetData(const D3D11_TEXTURE1D_DESC	*desc)	{
			DXGI_COMPONENTS	comp	= desc->Format;
			uint8			*p		= (uint8*)(desc + 1);
			for (int a = 0, i = 0; a < desc->ArraySize; a++) {
				for (int m = 0; m < desc->MipLevels; m++, i++) {
					blocks[i].set(p, 0);
					p += dxgi_align(mip_stride(comp, desc->Width, m));
				}
			}
		}
		void GetData(const D3D11_TEXTURE2D_DESC	*desc)	{
			DXGI_COMPONENTS	comp	= desc->Format;
			uint8			*p		= (uint8*)(desc + 1);
			for (int a = 0, i = 0; a < desc->ArraySize; a++) {
				for (int m = 0; m < desc->MipLevels; m++, i++) {
					size_t	w = dxgi_align(mip_stride(comp, desc->Width, m));
					blocks[i].set(p, uint32(w));
					p += w * mip_size(comp, desc->Height, m);
				}
			}
		}
		void GetData(const D3D11_TEXTURE3D_DESC	*desc)	{
			DXGI_COMPONENTS	comp	= desc->Format;
			uint8			*p		= (uint8*)(desc + 1);
			for (int m = 0, i = 0; m < desc->MipLevels; m++, i++) {
				size_t	w = dxgi_align(mip_stride(comp, desc->Width, m));
				size_t	s = w * mip_size(comp, desc->Height, m);
				blocks[i].set(p, uint32(w), uint32(s));
				p += s * mip_size(comp, desc->Depth, m);
			}
		}

		operator D3D11_SUBRESOURCE_DATA*() const { return blocks; }
	};

	DX11Replay(IDXGIAdapter *adapter = 0) {
		Init(adapter);
	}

	void	CreateObject(const RecItem2 &item);
	void	CreateObjects(const DX11Assets &assets);
	void	RunTo(const dynamic_array<Recording> &recordings, uint32 addr);
	void	RunGen(const dynamic_array<Recording> &recordings);
	dx::Resource GetResource(const DX11Assets::ItemRecord *item);
};

bool DX11Replay::Init(IDXGIAdapter *adapter) {
	D3D_FEATURE_LEVEL	feature_levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	com_ptr<ID3D11Device>			_device;
	com_ptr<ID3D11DeviceContext>	_context;
	if (FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_HARDWARE, NULL,
			D3D11_CREATE_DEVICE_DEBUG,
			feature_levels, num_elements32(feature_levels),
			D3D11_SDK_VERSION,
			&_device, &feature_level, &_context
		))
		|| FAILED(_device.query(&device))
		|| FAILED(_context.query(&ctx))
	)
		return false;

	com_ptr<ID3D11Debug>			debug;
	if (SUCCEEDED(device.query(&debug))) {
		com_ptr<ID3D11InfoQueue> queue;
		if (SUCCEEDED(debug.query(&queue))) {
			//queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
			queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, false);

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
	return true;
}

void UnmapCopy(ID3D11Resource *res, int sub, const D3D11_MAPPED_SUBRESOURCE &map, const_memory_block &data, bool rle) {
	if (!map.pData)
		return;

	D3D11_RESOURCE_DIMENSION dim;
	res->GetType(&dim);

	uint32	width, rows = 1, depth = 1;

	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER: {
			D3D11_BUFFER_DESC	desc;
			((ID3D11Buffer*)res)->GetDesc(&desc);
			width = desc.ByteWidth;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC	desc;
			((ID3D11Texture1D*)res)->GetDesc(&desc);
			width = GetSubResourceDims(&desc, sub, rows);
			break;
		}

		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC desc;
			((ID3D11Texture2D*)res)->GetDesc(&desc);
			width = GetSubResourceDims(&desc, sub, rows, depth);
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC desc;
			((ID3D11Texture3D*)res)->GetDesc(&desc);
			width = GetSubResourceDims(&desc, sub, rows, depth);
			break;
		}
	}

	memory_cube	cube(map.pData, width, rows, depth, map.RowPitch, map.DepthPitch);
	if (rle) {
		cube.copy_from_rle(data);
	} else {
		cube.copy_from(data);
	}

}

bool DX11Replay::Process(const COMRecording::header *h) {
	const uint16 *p = h->data();
//	ISO_TRACEF("process: %i\n", h->id);

	switch (h->id) {
		case RecDeviceContext::tag_InitialState: {
			auto	r = Remap<dx11::DeviceContext_State>(p);
			r->Put(ctx);
			break;
		}

		case RecDeviceContext::tag_VSSetConstantBuffers:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>)>(ctx, p, &ID3D11DeviceContext1::VSSetConstantBuffers); break;
		case RecDeviceContext::tag_PSSetShaderResources:						Replay2<void(UINT, UINT, counted<ID3D11ShaderResourceView *const, 1>)>(ctx, p, &ID3D11DeviceContext1::PSSetShaderResources); break;
		case RecDeviceContext::tag_PSSetShader:									Replay2<void(ID3D11PixelShader*, counted<ID3D11ClassInstance *const,2>, UINT)>(ctx, p, &ID3D11DeviceContext1::PSSetShader); break;
		case RecDeviceContext::tag_PSSetSamplers:								Replay2<void(UINT, UINT, counted<ID3D11SamplerState *const, 1>)>(ctx, p, &ID3D11DeviceContext1::PSSetSamplers); break;
		case RecDeviceContext::tag_VSSetShader:									Replay2<void(ID3D11VertexShader*, counted<ID3D11ClassInstance *const,2>, UINT)>(ctx, p, &ID3D11DeviceContext1::VSSetShader); break;
		case RecDeviceContext::tag_DrawIndexed:									Replay(ctx, p, &ID3D11DeviceContext1::DrawIndexed);	return true;
		case RecDeviceContext::tag_Draw:										Replay(ctx, p, &ID3D11DeviceContext1::Draw); return true;
		case RecDeviceContext::tag_Map:
			//Replay(ctx, p, &ID3D11DeviceContext1::Map);
			COMParse(p, [this](ID3D11Resource *rsrc, UINT sub, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *mapped) {
				if (ID3D11Resource *local = lookup(rsrc)) {
					if (MapType != D3D11_MAP_READ) {
						auto &mapping = open_mappings.push_back();
						mapping.orig	= mapped;
						mapping.rsrc	= rsrc;
						mapping.sub		= sub;
						ctx->Map(local, sub, MapType, MapFlags, &mapping);
					} else {
						D3D11_MAPPED_SUBRESOURCE mapped;
						ctx->Map(local, sub, MapType, MapFlags, &mapped);
					}
				}
			});
			break;
		case RecDeviceContext::tag_Unmap:
			//Replay(ctx, p, &ID3D11DeviceContext1::Unmap);
			COMParse(p, [this](ID3D11Resource *rsrc, UINT sub) {
				if (ID3D11Resource *local = lookup(rsrc)) {
					if (last_data) {
						for (auto &i : open_mappings) {
							if (i.rsrc == rsrc && i.sub == sub) {
								UnmapCopy(local, sub, i, last_data, rle_data);
								last_data = none;
								open_mappings.erase_unordered(&i);
								break;
							}
						}
					}

					ctx->Unmap(local, sub);
				}
			});
			break;
		case RecDeviceContext::tag_PSSetConstantBuffers:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>)>(ctx, p, &ID3D11DeviceContext1::PSSetConstantBuffers); break;
		case RecDeviceContext::tag_IASetInputLayout:							Replay(ctx, p, &ID3D11DeviceContext1::IASetInputLayout); break;
		case RecDeviceContext::tag_IASetVertexBuffers:							Replay2<void(UINT, UINT, counted<ID3D11Buffer *const,1>, UINT*, const UINT*)>(ctx, p, &ID3D11DeviceContext1::IASetVertexBuffers); break;
		case RecDeviceContext::tag_IASetIndexBuffer:							Replay(ctx, p, &ID3D11DeviceContext1::IASetIndexBuffer); break;
		case RecDeviceContext::tag_DrawIndexedInstanced:						Replay(ctx, p, &ID3D11DeviceContext1::DrawIndexedInstanced); return true;
		case RecDeviceContext::tag_DrawInstanced:								Replay(ctx, p, &ID3D11DeviceContext1::DrawInstanced); return true;
		case RecDeviceContext::tag_GSSetConstantBuffers:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>)>(ctx, p, &ID3D11DeviceContext1::GSSetConstantBuffers); break;
		case RecDeviceContext::tag_GSSetShader:									Replay2<void(ID3D11GeometryShader*, counted<ID3D11ClassInstance *const,2>, UINT)>(ctx, p, &ID3D11DeviceContext1::GSSetShader); break;
		case RecDeviceContext::tag_IASetPrimitiveTopology:						Replay(ctx, p, &ID3D11DeviceContext1::IASetPrimitiveTopology); break;
		case RecDeviceContext::tag_VSSetShaderResources:						Replay2<void(UINT, UINT, counted<ID3D11ShaderResourceView *const, 1>)>(ctx, p, &ID3D11DeviceContext1::VSSetShaderResources); break;
		case RecDeviceContext::tag_VSSetSamplers:								Replay2<void(UINT, UINT, counted<ID3D11SamplerState *const, 1>)>(ctx, p, &ID3D11DeviceContext1::VSSetSamplers); break;
		case RecDeviceContext::tag_Begin:										Replay(ctx, p, &ID3D11DeviceContext1::Begin); break;
		case RecDeviceContext::tag_End:											Replay(ctx, p, &ID3D11DeviceContext1::End); break;
		case RecDeviceContext::tag_SetPredication:								Replay(ctx, p, &ID3D11DeviceContext1::SetPredication); break;
		case RecDeviceContext::tag_GSSetShaderResources:						Replay2<void(UINT, UINT, counted<ID3D11ShaderResourceView *const, 1>)>(ctx, p, &ID3D11DeviceContext1::GSSetShaderResources); break;
		case RecDeviceContext::tag_GSSetSamplers:								Replay2<void(UINT, UINT, counted<ID3D11SamplerState *const, 1>)>(ctx, p, &ID3D11DeviceContext1::GSSetSamplers); break;
		case RecDeviceContext::tag_OMSetRenderTargets:							Replay2<void(UINT, counted<ID3D11RenderTargetView *const,0>, ID3D11DepthStencilView*)>(ctx, p, &ID3D11DeviceContext1::OMSetRenderTargets); break;
		case RecDeviceContext::tag_OMSetRenderTargetsAndUnorderedAccessViews:	Replay2<void(UINT, counted<ID3D11RenderTargetView *const,0>, ID3D11DepthStencilView*, UINT, UINT, counted<ID3D11UnorderedAccessView *const,4>, const UINT*)>(ctx, p, &ID3D11DeviceContext1::OMSetRenderTargetsAndUnorderedAccessViews); break;
		case RecDeviceContext::tag_OMSetBlendState:								Replay(ctx, p, &ID3D11DeviceContext1::OMSetBlendState); break;
		case RecDeviceContext::tag_OMSetDepthStencilState:						Replay(ctx, p, &ID3D11DeviceContext1::OMSetDepthStencilState); break;
		case RecDeviceContext::tag_SOSetTargets:								Replay2<void(UINT, counted<ID3D11Buffer *const,0>, counted<const UINT,0>)>(ctx, p, &ID3D11DeviceContext1::SOSetTargets); break;
		case RecDeviceContext::tag_DrawAuto:									Replay(ctx, p, &ID3D11DeviceContext1::DrawAuto); return true;
		case RecDeviceContext::tag_DrawIndexedInstancedIndirect:				Replay(ctx, p, &ID3D11DeviceContext1::DrawIndexedInstancedIndirect); return true;
		case RecDeviceContext::tag_DrawInstancedIndirect:						Replay(ctx, p, &ID3D11DeviceContext1::DrawInstancedIndirect); return true;
		case RecDeviceContext::tag_Dispatch:									Replay(ctx, p, &ID3D11DeviceContext1::Dispatch); return true;
		case RecDeviceContext::tag_DispatchIndirect:							Replay(ctx, p, &ID3D11DeviceContext1::DispatchIndirect); return true;
		case RecDeviceContext::tag_RSSetState:									Replay(ctx, p, &ID3D11DeviceContext1::RSSetState); break;
		case RecDeviceContext::tag_RSSetViewports:								Replay(ctx, p, &ID3D11DeviceContext1::RSSetViewports); break;
		case RecDeviceContext::tag_RSSetScissorRects:							Replay(ctx, p, &ID3D11DeviceContext1::RSSetScissorRects); break;
		case RecDeviceContext::tag_CopySubresourceRegion:						Replay(ctx, p, &ID3D11DeviceContext1::CopySubresourceRegion); return true;
		case RecDeviceContext::tag_CopyResource:								Replay(ctx, p, &ID3D11DeviceContext1::CopyResource); return true;

		case RecDeviceContext::tag_UpdateSubresource:							Replay2<void(ID3D11Resource*, UINT, const D3D11_BOX*, iso::lookedup<const void*>, UINT, UINT)>(ctx, p, &ID3D11DeviceContext1::UpdateSubresource); break;
		case RecDeviceContext::tag_CopyStructureCount:							Replay(ctx, p, &ID3D11DeviceContext1::CopyStructureCount); return true;
		case RecDeviceContext::tag_ClearRenderTargetView:						Replay2<void(ID3D11RenderTargetView*,const array<FLOAT,4>)>(ctx, p, &ID3D11DeviceContext1::ClearRenderTargetView); return true;
		case RecDeviceContext::tag_ClearUnorderedAccessViewUint:				Replay(ctx, p, &ID3D11DeviceContext1::ClearUnorderedAccessViewUint); return true;
		case RecDeviceContext::tag_ClearUnorderedAccessViewFloat:				Replay(ctx, p, &ID3D11DeviceContext1::ClearUnorderedAccessViewFloat); return true;
		case RecDeviceContext::tag_ClearDepthStencilView:						Replay(ctx, p, &ID3D11DeviceContext1::ClearDepthStencilView); return true;
		case RecDeviceContext::tag_GenerateMips:								Replay(ctx, p, &ID3D11DeviceContext1::GenerateMips); return true;
		case RecDeviceContext::tag_SetResourceMinLOD:							Replay(ctx, p, &ID3D11DeviceContext1::SetResourceMinLOD); break;
		case RecDeviceContext::tag_ResolveSubresource:							Replay(ctx, p, &ID3D11DeviceContext1::ResolveSubresource); return true;
		case RecDeviceContext::tag_ExecuteCommandList:							Replay(ctx, p, &ID3D11DeviceContext1::ExecuteCommandList); break;
		case RecDeviceContext::tag_HSSetShaderResources:						Replay2<void(UINT, UINT, counted<ID3D11ShaderResourceView *const, 1>)>(ctx, p, &ID3D11DeviceContext1::HSSetShaderResources); break;
		case RecDeviceContext::tag_HSSetShader:									Replay2<void(ID3D11HullShader*, counted<ID3D11ClassInstance *const,2>, UINT)>(ctx, p, &ID3D11DeviceContext1::HSSetShader); break;
		case RecDeviceContext::tag_HSSetSamplers:								Replay2<void(UINT, UINT, counted<ID3D11SamplerState *const, 1>)>(ctx, p, &ID3D11DeviceContext1::HSSetSamplers); break;
		case RecDeviceContext::tag_HSSetConstantBuffers:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>)>(ctx, p, &ID3D11DeviceContext1::HSSetConstantBuffers); break;
		case RecDeviceContext::tag_DSSetShaderResources:						Replay2<void(UINT, UINT, counted<ID3D11ShaderResourceView *const, 1>)>(ctx, p, &ID3D11DeviceContext1::DSSetShaderResources); break;
		case RecDeviceContext::tag_DSSetShader:									Replay2<void(ID3D11DomainShader*, counted<ID3D11ClassInstance *const,2>, UINT)>(ctx, p, &ID3D11DeviceContext1::DSSetShader); break;
		case RecDeviceContext::tag_DSSetSamplers:								Replay2<void(UINT, UINT, counted<ID3D11SamplerState *const, 1>)>(ctx, p, &ID3D11DeviceContext1::DSSetSamplers); break;
		case RecDeviceContext::tag_DSSetConstantBuffers:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>)>(ctx, p, &ID3D11DeviceContext1::DSSetConstantBuffers); break;
		case RecDeviceContext::tag_CSSetShaderResources:						Replay2<void(UINT, UINT, counted<ID3D11ShaderResourceView *const, 1>)>(ctx, p, &ID3D11DeviceContext1::CSSetShaderResources); break;
		case RecDeviceContext::tag_CSSetUnorderedAccessViews:					Replay2<void(UINT, UINT, counted<ID3D11UnorderedAccessView *const, 1>, counted<const UINT, 1>)>(ctx, p, &ID3D11DeviceContext1::CSSetUnorderedAccessViews); break;
		case RecDeviceContext::tag_CSSetShader:									Replay2<void(ID3D11ComputeShader*, counted<ID3D11ClassInstance *const,2>, UINT)>(ctx, p, &ID3D11DeviceContext1::CSSetShader); break;
		case RecDeviceContext::tag_CSSetSamplers:								Replay2<void(UINT, UINT, counted<ID3D11SamplerState *const, 1>)>(ctx, p, &ID3D11DeviceContext1::CSSetSamplers); break;
		case RecDeviceContext::tag_CSSetConstantBuffers:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>)>(ctx, p, &ID3D11DeviceContext1::CSSetConstantBuffers); break;
		case RecDeviceContext::tag_ClearState:									Replay(ctx, p, &ID3D11DeviceContext1::ClearState); break;
		case RecDeviceContext::tag_Flush:										Replay(ctx, p, &ID3D11DeviceContext1::Flush); break;
		case RecDeviceContext::tag_FinishCommandList:							Replay(ctx, p, &ID3D11DeviceContext1::FinishCommandList); break;
	//ID3D11DeviceContext1
		case RecDeviceContext::tag_CopySubresourceRegion1:						Replay(ctx, p, &ID3D11DeviceContext1::CopySubresourceRegion1); break;
		case RecDeviceContext::tag_UpdateSubresource1:							Replay2<void(ID3D11Resource*, UINT, const D3D11_BOX*, iso::lookedup<const void*>, UINT, UINT, UINT)>(ctx, p, &ID3D11DeviceContext4::UpdateSubresource1); break;
		case RecDeviceContext::tag_DiscardResource:								Replay(ctx, p, &ID3D11DeviceContext1::DiscardResource); break;
		case RecDeviceContext::tag_DiscardView:									Replay(ctx, p, &ID3D11DeviceContext1::DiscardView); break;
		case RecDeviceContext::tag_VSSetConstantBuffers1:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>,const UINT*, const UINT*)>(ctx, p, &ID3D11DeviceContext1::VSSetConstantBuffers1); break;
		case RecDeviceContext::tag_HSSetConstantBuffers1:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>,const UINT*, const UINT*)>(ctx, p, &ID3D11DeviceContext1::HSSetConstantBuffers1); break;
		case RecDeviceContext::tag_DSSetConstantBuffers1:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>,const UINT*, const UINT*)>(ctx, p, &ID3D11DeviceContext1::DSSetConstantBuffers1); break;
		case RecDeviceContext::tag_GSSetConstantBuffers1:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>,const UINT*, const UINT*)>(ctx, p, &ID3D11DeviceContext1::GSSetConstantBuffers1); break;
		case RecDeviceContext::tag_PSSetConstantBuffers1:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>,const UINT*, const UINT*)>(ctx, p, &ID3D11DeviceContext1::PSSetConstantBuffers1); break;
		case RecDeviceContext::tag_CSSetConstantBuffers1:						Replay2<void(UINT, UINT, counted<ID3D11Buffer *const, 1>,const UINT*, const UINT*)>(ctx, p, &ID3D11DeviceContext1::CSSetConstantBuffers1); break;
		case RecDeviceContext::tag_SwapDeviceContextState:						Replay(ctx, p, &ID3D11DeviceContext1::SwapDeviceContextState); break;
		case RecDeviceContext::tag_ClearView:									Replay(ctx, p, &ID3D11DeviceContext1::ClearView); return true;
		case RecDeviceContext::tag_DiscardView1:								Replay(ctx, p, &ID3D11DeviceContext1::DiscardView1); break;
	//ID3D11DeviceContext2
		case RecDeviceContext::tag_UpdateTileMappings:
		case RecDeviceContext::tag_CopyTileMappings:
		case RecDeviceContext::tag_CopyTiles:
		case RecDeviceContext::tag_UpdateTiles:
		case RecDeviceContext::tag_ResizeTilePool:
		case RecDeviceContext::tag_TiledResourceBarrier:
		case RecDeviceContext::tag_SetMarkerInt:
		case RecDeviceContext::tag_BeginEventInt:
		case RecDeviceContext::tag_EndEvent:
	//ID3D11DeviceContext3
		case RecDeviceContext::tag_Flush1:
		case RecDeviceContext::tag_SetHardwareProtectionState:
	//ID3D11DeviceContext4
		case RecDeviceContext::tag_Signal:
		case RecDeviceContext::tag_Wait:
			break;
	//internal
		case RecDeviceContext::tag_DATA:
			last_data	= h->block();
			rle_data	= false;
			break;
		case RecDeviceContext::tag_RLE_DATA:
			last_data	= h->block();
			rle_data	= true;
			break;
	}
	return false;
}

void DX11Replay::CreateObject(const RecItem2 &item) {
	ID3D11DeviceChild	*local	= 0;
	HRESULT				hr		= E_FAIL;
	bool				dead	= item.is_dead();
	
	switch (undead(item.type)) {
		case RecItem::DepthStencilState:	hr = device->CreateDepthStencilState	(item.info, (ID3D11DepthStencilState**)&local); break;
		case RecItem::BlendState:			hr = device->CreateBlendState1			(item.info, (ID3D11BlendState1		**)&local); break;
		case RecItem::RasterizerState:		hr = device->CreateRasterizerState1		(item.info, (ID3D11RasterizerState1	**)&local); break;
		case RecItem::Buffer:				hr = device->CreateBuffer				(item.info, ResourceData((const D3D11_BUFFER_DESC	*)item.info, item.info.length()), (ID3D11Buffer		**)&local); break;
		case RecItem::Texture1D:			hr = device->CreateTexture1D			(item.info, ResourceData((const D3D11_TEXTURE1D_DESC*)item.info, item.info.length()), (ID3D11Texture1D	**)&local); break;
		case RecItem::Texture2D: {
			const D3D11_TEXTURE2D_DESC		*desc = item.info;
			hr = desc->SampleDesc.Count > 1
				? device->CreateTexture2D(desc, 0, (ID3D11Texture2D**)&local)
				: device->CreateTexture2D(desc, ResourceData(desc, item.info.length()), (ID3D11Texture2D**)&local);
			break;
		}
		case RecItem::Texture3D: 
			hr = device->CreateTexture3D(item.info, ResourceData((const D3D11_TEXTURE3D_DESC*)item.info, item.info.length()), (ID3D11Texture3D	**)&local);
			break;

		case RecItem::ShaderResourceView: {
			const RecView<D3D11_SHADER_RESOURCE_VIEW_DESC>	*r = item.info;
			if (ID3D11Resource *res = obj2local[r->resource].or_default())
				hr = device->CreateShaderResourceView(res, r->pdesc, (ID3D11ShaderResourceView**)&local);
			break;
		}
		case RecItem::RenderTargetView: {
			const RecView<D3D11_RENDER_TARGET_VIEW_DESC>	*r = item.info;
			if (ID3D11Resource *res = obj2local[r->resource].or_default()) {
				D3D11_RENDER_TARGET_VIEW_DESC	rtvdesc2, *rtvdesc = r->pdesc;
				if (rtvdesc && rtvdesc->Format) {
					rtvdesc2	= *rtvdesc;
					rtvdesc		= &rtvdesc2;
					D3D11_RESOURCE_DIMENSION	dim;
					res->GetType(&dim);
					switch (dim) {
						case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
							D3D11_TEXTURE2D_DESC		desc;
							((ID3D11Texture2D*)res)->GetDesc(&desc);
							if (DXGI_COMPONENTS(desc.Format).Type())
								rtvdesc2.Format = DXGI_FORMAT_UNKNOWN;
							break;
						}
					}
				}
				hr = device->CreateRenderTargetView(res, rtvdesc, (ID3D11RenderTargetView**)&local);
			}
			break;
		}
		case RecItem::DepthStencilView: {
			const RecView<D3D11_DEPTH_STENCIL_VIEW_DESC>		*r = item.info;
			if (ID3D11Resource *res = obj2local[r->resource].or_default())
				hr = device->CreateDepthStencilView(res, r->pdesc, (ID3D11DepthStencilView**)&local); 
			break;
		}
		case RecItem::UnorderedAccessView: {
			const RecView<D3D11_UNORDERED_ACCESS_VIEW_DESC>	*r = item.info;
			if (ID3D11Resource *res = obj2local[r->resource].or_default())
				hr = device->CreateUnorderedAccessView(res, r->pdesc, (ID3D11UnorderedAccessView**)&local);
			break;
		}
		case RecItem::VertexShader:			hr = device->CreateVertexShader			(item.info, item.info.length(), 0, (ID3D11VertexShader	**)&local); break;
		case RecItem::HullShader:			hr = device->CreateHullShader			(item.info, item.info.length(), 0, (ID3D11HullShader	**)&local); break;
		case RecItem::DomainShader:			hr = device->CreateDomainShader			(item.info, item.info.length(), 0, (ID3D11DomainShader	**)&local); break;
		case RecItem::GeometryShader:		hr = device->CreateGeometryShader		(item.info, item.info.length(), 0, (ID3D11GeometryShader**)&local); break;
		case RecItem::PixelShader:			hr = device->CreatePixelShader			(item.info, item.info.length(), 0, (ID3D11PixelShader	**)&local); break;
		case RecItem::ComputeShader:		hr = device->CreateComputeShader		(item.info, item.info.length(), 0, (ID3D11ComputeShader	**)&local); break;
		case RecItem::InputLayout:			Replay(item.info, [this, &local, &hr](counted<const D3D11_INPUT_ELEMENT_DESC,1> desc, UINT num, counted<const uint8,3> code, SIZE_T length) {
			D3D11_INPUT_ELEMENT_DESC	dummy = {0};
			hr = device->CreateInputLayout(desc ? desc : &dummy, num, code, length, (ID3D11InputLayout**)&local);
		});  break;
		case RecItem::SamplerState:			hr = device->CreateSamplerState			(item.info, (ID3D11SamplerState		**)&local); break;
		case RecItem::Query:				hr = device->CreateQuery				(item.info,	(ID3D11Query			**)&local); break;
		case RecItem::Predicate:			hr = device->CreatePredicate			(item.info, (ID3D11Predicate		**)&local); break;
		case RecItem::Counter:				hr = device->CreateCounter				(item.info, (ID3D11Counter			**)&local); break;
		case RecItem::ClassLinkage:			hr = device->CreateClassLinkage			((ID3D11ClassLinkage				**)&local); break;
		case RecItem::DeviceContext:		hr = device->CreateDeferredContext		(0,			(ID3D11DeviceContext	**)&local); break;
	}
	if (local)
		obj2local[item.obj]		= local;
}

void DX11Replay::CreateObjects(const DX11Assets &assets) {
	for (auto &i : assets.items)
		CreateObject(i);
}

void DX11Replay::RunTo(const dynamic_array<Recording> &recordings, uint32 addr) {
	fiber_generator<uint32>	fg([this, &recordings]() { RunGen(recordings); }, 1 << 17);
	for (auto i = fg.begin(); *i < addr; ++i)
		;
}

void DX11Replay::RunGen(const dynamic_array<Recording> &recordings) {
	uint32	offset = 0;
	if (!fiber::yield(offset))
		return;

	for (auto &i : recordings) {
		if (i.type == Recording::CONTEXT) {
			for (const COMRecording::header *p = i.recording, *e = i.recording.end(); p < e; p = p->next()) {
				Process(p);
				if (!fiber::yield((uint8*)p - i.recording + offset))
					return;
			}
		}
		offset += i.recording.size32();
	}
	fiber::yield(offset);
}

dx::Resource DX11Replay::GetResource(const DX11Assets::ItemRecord *item) {
	dx::Resource	r;

	if (!item)
		return r;

	ID3D11Resource *res = is_view(item->type)		? ((const RecView<D3D11_SHADER_RESOURCE_VIEW_DESC>*)item->info)->resource//.get()
						: is_resource(item->type)	? (ID3D11Resource*)item->obj
						: nullptr;

	ID3D11Resource *local	= obj2local[res].or_default();
	if (!local)
		return r;

	D3D11_RESOURCE_DIMENSION dim;
	DXGI_FORMAT		format = DXGI_FORMAT_UNKNOWN;
	uint32			width = 0, height = 0, depth = 0, mips = 0, samples = 1;
	local->GetType(&dim);

	switch (dim) {
		case D3D11_RESOURCE_DIMENSION_BUFFER: {
			D3D11_BUFFER_DESC	desc;
			((ID3D11Buffer*)local)->GetDesc(&desc);
			width	= desc.ByteWidth;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
			D3D11_TEXTURE1D_DESC	desc;
			((ID3D11Texture1D*)local)->GetDesc(&desc);
			format	= desc.Format;
			width	= desc.Width;
			depth	= desc.ArraySize;
			mips	= desc.MipLevels;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
			D3D11_TEXTURE2D_DESC	desc;
			((ID3D11Texture2D*)local)->GetDesc(&desc);
			format	= desc.Format;
			width	= desc.Width;
			height	= desc.Height;
			depth	= desc.ArraySize;
			mips	= desc.MipLevels;
			samples	= desc.SampleDesc.Count;
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
			D3D11_TEXTURE3D_DESC	desc;
			((ID3D11Texture3D*)local)->GetDesc(&desc);
			format	= desc.Format;
			width	= desc.Width;
			height	= desc.Height;
			depth	= desc.Depth;
			mips	= desc.MipLevels;
			break;
		}
	}

	int				mip		= 0, nmip	= 1;
	int				slice	= 0, nslice = 1;

	switch (item->type) {
		case RecItem::ShaderResourceView: {
			const RecView<D3D11_SHADER_RESOURCE_VIEW_DESC>	*r = item->info;
			if (D3D11_SHADER_RESOURCE_VIEW_DESC *desc = r->pdesc) {
				if (desc->Format)
					format = desc->Format;

				switch (desc->ViewDimension) {
					case D3D11_SRV_DIMENSION_TEXTURE1D:
						mip		= desc->Texture1D.MostDetailedMip;
						nmip	= desc->Texture1D.MipLevels;
						break;
					case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
						mip		= desc->Texture1DArray.MostDetailedMip;
						nmip	= desc->Texture1DArray.MipLevels;
						slice	= desc->Texture1DArray.FirstArraySlice;
						nslice	= desc->Texture1DArray.ArraySize;
						break;
					case D3D11_SRV_DIMENSION_TEXTURE2D:
						mip		= desc->Texture2D.MostDetailedMip;
						nmip	= desc->Texture2D.MipLevels;
						break;
					case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
						mip		= desc->Texture2DArray.MostDetailedMip;
						nmip	= desc->Texture2DArray.MipLevels;
						slice	= desc->Texture2DArray.FirstArraySlice;
						nslice	= desc->Texture2DArray.ArraySize;
						break;
					case D3D11_SRV_DIMENSION_TEXTURE3D:
						mip		= desc->Texture3D.MostDetailedMip;
						nmip	= desc->Texture3D.MipLevels;
						break;
					case D3D11_SRV_DIMENSION_TEXTURECUBE:
						mip		= desc->TextureCube.MostDetailedMip;
						nmip	= desc->TextureCube.MipLevels;
						break;
					case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
						mip		= desc->TextureCubeArray.MostDetailedMip;
						nmip	= desc->TextureCubeArray.MipLevels;
						slice	= desc->TextureCubeArray.First2DArrayFace;
						nslice	= desc->TextureCubeArray.NumCubes * 6;
						break;
				}
			}
			break;
		}
		case RecItem::RenderTargetView: {
			const RecView<D3D11_RENDER_TARGET_VIEW_DESC>	*r = item->info;
			if (D3D11_RENDER_TARGET_VIEW_DESC *desc = r->pdesc) {
				if (desc->Format)
					format = desc->Format;

				switch (desc->ViewDimension) {
					case D3D11_RTV_DIMENSION_TEXTURE1D:
						mip		= desc->Texture1D.MipSlice;
						break;
					case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
						mip		= desc->Texture1DArray.MipSlice;
						slice	= desc->Texture1DArray.FirstArraySlice;
						nslice	= desc->Texture1DArray.ArraySize;
						break;
					case D3D11_RTV_DIMENSION_TEXTURE2D:
						mip		= desc->Texture2D.MipSlice;
						break;
					case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
						mip		= desc->Texture2DArray.MipSlice;
						slice	= desc->Texture2DArray.FirstArraySlice;
						nslice	= desc->Texture2DArray.ArraySize;
						break;
					case D3D11_RTV_DIMENSION_TEXTURE3D:
						mip		= desc->Texture3D.MipSlice;
						slice	= desc->Texture3D.FirstWSlice;
						nslice	= desc->Texture3D.WSize;
						break;
				}
			}
			break;
		}
		case RecItem::DepthStencilView: {
			const RecView<D3D11_DEPTH_STENCIL_VIEW_DESC>	*r = item->info;
			if (D3D11_DEPTH_STENCIL_VIEW_DESC *desc = r->pdesc) {
				//if (desc->Format)
				//	format = desc->Format;

				switch (desc->ViewDimension) {
					case D3D11_DSV_DIMENSION_TEXTURE1D:
						mip		= desc->Texture1D.MipSlice;
						break;
					case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
						mip		= desc->Texture1DArray.MipSlice;
						slice	= desc->Texture1DArray.FirstArraySlice;
						nslice	= desc->Texture1DArray.ArraySize;
						break;
					case D3D11_DSV_DIMENSION_TEXTURE2D:
						mip		= desc->Texture2D.MipSlice;
						break;
					case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
						mip		= desc->Texture2DArray.MipSlice;
						slice	= desc->Texture2DArray.FirstArraySlice;
						nslice	= desc->Texture2DArray.ArraySize;
						break;
				}
			}
			break;
		}
		case RecItem::UnorderedAccessView: {
			const RecView<D3D11_UNORDERED_ACCESS_VIEW_DESC>	*r = item->info;
			if (D3D11_UNORDERED_ACCESS_VIEW_DESC *desc = r->pdesc) {
				if (desc->Format)
					format = desc->Format;

				switch (desc->ViewDimension) {
					case D3D11_UAV_DIMENSION_TEXTURE1D:
						mip		= desc->Texture1D.MipSlice;
						break;
					case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
						mip		= desc->Texture1DArray.MipSlice;
						slice	= desc->Texture1DArray.FirstArraySlice;
						nslice	= desc->Texture1DArray.ArraySize;
						break;
					case D3D11_UAV_DIMENSION_TEXTURE2D:
						mip		= desc->Texture2D.MipSlice;
						break;
					case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
						mip		= desc->Texture2DArray.MipSlice;
						slice	= desc->Texture2DArray.FirstArraySlice;
						nslice	= desc->Texture2DArray.ArraySize;
						break;
					case D3D11_UAV_DIMENSION_TEXTURE3D:
						mip		= desc->Texture3D.MipSlice;
						slice	= desc->Texture3D.FirstWSlice;
						nslice	= desc->Texture3D.WSize;
						break;
				}
			}
			break;
		}
	}

	DXGI_COMPONENTS	format2 = format;
	int	mip_width		= mip_size(format2, width, mip);
	int	mip_height		= mip_size(format2, height, mip);
	int	sub				= mip + slice * mips;

	if (height == 0) {
		r.init(dx::RESOURCE_DIMENSION_BUFFER, format2, width, 1, 1, 1);

		D3D11_BUFFER_DESC	desc;
		desc.ByteWidth			= desc.StructureByteStride = width;
		desc.Usage				= D3D11_USAGE_STAGING;
		desc.BindFlags			= 0;
		desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
		desc.MiscFlags			= 0;
		com_ptr<ID3D11Buffer>	res2;
		device->CreateBuffer(&desc, 0, &res2);

		ctx->CopyResource(res2, local);

		D3D11_MAPPED_SUBRESOURCE	mapped;
		ctx->Map(res2, 0, D3D11_MAP_READ, 0, &mapped);
		r.set_mem(malloc_block(r.length()).detach());

		memcpy((uint8*)r, mapped.pData, mapped.RowPitch);

		ctx->Unmap(res2, 0);

	} else {
		r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, format2, width, height, 1, 1);
	
		D3D11_TEXTURE2D_DESC	desc;
		desc.Width				= width;
		desc.Height				= height;
		desc.MipLevels			= nmip;
		desc.ArraySize			= nslice;
		desc.Format				= format;
		desc.SampleDesc.Count	= 1;//samples;
		desc.SampleDesc.Quality	= 0;

		com_ptr<ID3D11Texture2D>	res2;

		if (samples > 1) {
			com_ptr<ID3D11Texture2D>	res1;
			desc.Usage				= D3D11_USAGE_DEFAULT;
			desc.BindFlags			= D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags		= D3D11_CPU_ACCESS_READ;
			desc.MiscFlags			= 0;
			device->CreateTexture2D(&desc, 0, &res1);

			ctx->ResolveSubresource(res1, 0, local, sub, format);

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
			ctx->CopySubresourceRegion(
				res2, 0, 0, 0, 0,
				local, sub, 0
			);
		}

		D3D11_MAPPED_SUBRESOURCE	mapped;
		ctx->Map(res2, 0, D3D11_MAP_READ, 0, &mapped);
		r.set_mem(malloc_block(r.length()).detach());

		uint32	rstride	= dxgi_align(stride(format2, width));
		for (int y = 0; y < r.height; y++)
			memcpy((uint8*)r + rstride * y, (uint8*)mapped.pData + mapped.RowPitch * y, mapped.RowPitch);

		ctx->Unmap(res2, 0);
	}

	return r;
}

//-----------------------------------------------------------------------------
//	DX11Connection
//-----------------------------------------------------------------------------

struct DX11Capture : anything {};

struct DX11Connection : DX11Assets {
	enum {
		WT_NONE,
		WT_BATCH,
		WT_MARKER,
		WT_CALLSTACK,
		WT_RESOURCE,
		WT_DATA,
	};

//	ref_ptr<DXConnection>		con;

	CallStackDumper				stack_dumper;
	Disassembler::SharedFiles	files;
	dynamic_array<Recording>	recordings;
	uint64						frequency	= 0;
	string						shader_path;

	DX11Connection() : stack_dumper(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES) {}
	DX11Connection(DXConnection *con) : stack_dumper(con->process, SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES, BuildSymbolSearchPath(con->process.Filename().dir())) {
		shader_path = separated_list(transformc(GetSettings("Paths/source_path"), [](const ISO::Browser2 &i) { return i.GetString(); }), ";");
	}
	
	void					GetAssets(progress prog);
	bool					GetStatistics();
	ISO_ptr_machine<void>	GetBitmap(ItemRecord *t);

	const_memory_block		GetDataAt(uint32 addr);
	void					GetStateAt(DX11State &state, uint32 addr);
	uint32					FindObjectCreation(ID3D11DeviceChild *obj);

	bool					Save(const filename &fn);
	void					Load(const DX11Capture *cap);

	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		buffer_accum<256>	ba;
		PutFieldName(ba, tree.format, pf);

		uint64		v = pf->get_raw_value(p, offset);
		if (pf == fields<const_memory_block>::f) {
			tree.Add(h, ba, WT_DATA, p);
		} else if (auto *rec = FindItem((ID3D11DeviceChild*)v)) {
			tree.Add(h, ba << rec->name, WT_RESOURCE, rec);
		} else {
			tree.AddText(h, ba << "(unfound)0x" << hex(v), addr);
		}
	}
};

const_memory_block DX11Connection::GetDataAt(uint32 addr) {
	for (auto &i : recordings) {
		if (i.type == Recording::CONTEXT) {
			if (addr <= i.recording.size32())
				return i.recording.slice(addr);
		}
		addr -= i.recording.size32();
	}
	return none;
}

void DX11Connection::GetStateAt(DX11State &state, uint32 addr) {
	state.Reset();
//	for (auto &i : items) {
//		if (i.type == RecItem::DeviceContext)
//			state.Init(i.info);
//	}
	DX11StateParser	parser(state);
	for (auto &i : recordings) {
		if (i.type == Recording::CONTEXT) {
			for (const COMRecording::header *p = i.recording, *e = i.recording + min(i.recording.size32(), addr); p < e; p = p->next())
				parser.Process(this, p);
			if (addr <= i.recording.size32())
				break;
		}
		addr -= i.recording.size32();
	}
}

template<typename T> T get(lookup<T> &p) { return p.t; }

template<typename F> bool CheckCreated(ID3D11DeviceChild *obj, const void *p) {
	typedef	typename function<F>::P			P;
	typedef typename meta::map<PM, P>::type		RTL1;
	auto	r	= *((TL_tuple<RTL1>*)p)->template get<RTL1::count - 1>();
	return (ID3D11DeviceChild*)get(r) == obj;
}
template<typename F> bool CheckCreated(ID3D11DeviceChild *obj, const void *p, F f) {
	return CheckCreated<F>(obj, p);
}

uint32 DX11Connection::FindObjectCreation(ID3D11DeviceChild *obj) {
	bool	found = false;

	for (auto &i : recordings) {
		if (i.type == Recording::DEVICE) {
			for (const COMRecording::header *p = i.recording, *e = i.recording.end(); p < e; p = p->next()) {
				switch (p->id) {
					case RecDevice::tag_CreateBuffer:							found = CheckCreated(obj, p->data(), &ID3D11Device::CreateBuffer); break;				
					case RecDevice::tag_CreateTexture1D:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateTexture1D); break;			
					case RecDevice::tag_CreateTexture2D:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateTexture2D); break;			
					case RecDevice::tag_CreateTexture3D:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateTexture3D); break;			
					case RecDevice::tag_CreateShaderResourceView:				found = CheckCreated(obj, p->data(), &ID3D11Device::CreateShaderResourceView); break;	
					case RecDevice::tag_CreateUnorderedAccessView:				found = CheckCreated(obj, p->data(), &ID3D11Device::CreateUnorderedAccessView); break;	
					case RecDevice::tag_CreateRenderTargetView:					found = CheckCreated(obj, p->data(), &ID3D11Device::CreateRenderTargetView); break;		
					case RecDevice::tag_CreateDepthStencilView:					found = CheckCreated(obj, p->data(), &ID3D11Device::CreateDepthStencilView); break;		
					case RecDevice::tag_CreateInputLayout:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateInputLayout); break;			
					case RecDevice::tag_CreateVertexShader:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateVertexShader); break;			
					case RecDevice::tag_CreateGeometryShader:					found = CheckCreated(obj, p->data(), &ID3D11Device::CreateGeometryShader); break;		
					case RecDevice::tag_CreateGeometryShaderWithStreamOutput:	found = CheckCreated(obj, p->data(), &ID3D11Device::CreateGeometryShaderWithStreamOutput); break;
					case RecDevice::tag_CreatePixelShader:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreatePixelShader); break;			
					case RecDevice::tag_CreateHullShader:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateHullShader); break;			
					case RecDevice::tag_CreateDomainShader:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateDomainShader); break;			
					case RecDevice::tag_CreateComputeShader:					found = CheckCreated(obj, p->data(), &ID3D11Device::CreateComputeShader); break;		
					case RecDevice::tag_CreateClassLinkage:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateClassLinkage); break;			
					case RecDevice::tag_CreateBlendState:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateBlendState); break;			
					case RecDevice::tag_CreateDepthStencilState:				found = CheckCreated(obj, p->data(), &ID3D11Device::CreateDepthStencilState); break;	
					case RecDevice::tag_CreateRasterizerState:					found = CheckCreated(obj, p->data(), &ID3D11Device::CreateRasterizerState); break;		
					case RecDevice::tag_CreateSamplerState:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreateSamplerState); break;			
					case RecDevice::tag_CreateQuery:							found = CheckCreated(obj, p->data(), &ID3D11Device::CreateQuery); break;				
					case RecDevice::tag_CreatePredicate:						found = CheckCreated(obj, p->data(), &ID3D11Device::CreatePredicate); break;			
					case RecDevice::tag_CreateCounter:							found = CheckCreated(obj, p->data(), &ID3D11Device::CreateCounter); break;				
					case RecDevice::tag_CreateDeferredContext:					found = CheckCreated(obj, p->data(), &ID3D11Device::CreateDeferredContext); break;		
				//ID3D11Device1
					case RecDevice::tag_CreateDeferredContext1:					found = CheckCreated(obj, p->data(), &ID3D11Device1::CreateDeferredContext1); break;		
					case RecDevice::tag_CreateBlendState1:						found = CheckCreated(obj, p->data(), &ID3D11Device1::CreateBlendState1); break;			
					case RecDevice::tag_CreateRasterizerState1:					found = CheckCreated(obj, p->data(), &ID3D11Device1::CreateRasterizerState1); break;		
					case RecDevice::tag_CreateDeviceContextState:				found = CheckCreated(obj, p->data(), &ID3D11Device1::CreateDeviceContextState); break;	
				//ID3D11Device2
					case RecDevice::tag_CreateDeferredContext2:					found = CheckCreated(obj, p->data(), &ID3D11Device2::CreateDeferredContext2); break;		
				//ID3D11Device3
					case RecDevice::tag_CreateTexture2D1:						found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateTexture2D1); break;			
					case RecDevice::tag_CreateTexture3D1:						found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateTexture3D1); break;			
					case RecDevice::tag_CreateRasterizerState2:					found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateRasterizerState2); break;		
					case RecDevice::tag_CreateShaderResourceView1:				found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateShaderResourceView1); break;	
					case RecDevice::tag_CreateUnorderedAccessView1:				found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateUnorderedAccessView1); break;	
					case RecDevice::tag_CreateRenderTargetView1:				found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateRenderTargetView1); break;	
					case RecDevice::tag_CreateQuery1:							found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateQuery1); break;				
					case RecDevice::tag_CreateDeferredContext3:					found = CheckCreated(obj, p->data(), &ID3D11Device3::CreateDeferredContext3); break;		
				//ID3D11Device5
					case RecDevice::tag_OpenSharedFence:						found = CheckCreated(obj, p->data(), &ID3D11Device5::OpenSharedFence); break;			
					case RecDevice::tag_CreateFence:							found = CheckCreated(obj, p->data(), &ID3D11Device5::CreateFence); break;				
				}
				if (found)
					return (uint8*)p - (uint8*)i.recording;
			}
		}
	}
	return 0;
}

struct DX11CaptureItem {
	uint32					type;
	string16				name;
	xint64					obj;
	ISO_openarray<uint8>	info;
	DX11CaptureItem(const DX11Assets::ItemRecord &o) : type(o.type), name(o.name), obj((uint64)o.obj), info(o.info.size32()) {
		memcpy(info, o.info, o.info.size32());
	}
};

struct DX11CaptureModule {
	uint64			a, b;
	string			fn;
	DX11CaptureModule(const CallStackDumper::Module &m) : a(m.a), b(m.b), fn(m.fn) {}
};

namespace ISO {
ISO_DEFUSERCOMPF(DX11CaptureItem, 4, ISO::TypeUser::WRITETOBIN) {
	ISO_SETFIELD(0,type),
	ISO_SETFIELD(1,name),
	ISO_SETFIELD(2,obj),
	ISO_SETFIELD(3,info);
}};

ISO_DEFUSERCOMPF(DX11CaptureModule, 3, ISO::TypeUser::WRITETOBIN) {
	ISO_SETFIELD(0,a),
	ISO_SETFIELD(1,b),
	ISO_SETFIELD(2,fn);
}};

ISO_DEFUSERF(DX11Capture, anything, ISO::TypeUser::WRITETOBIN);
}

bool DX11Connection::Save(const filename &fn) {
	ISO_ptr<DX11Capture>	p(0);

	p->Append(ISO_ptr<ISO_openarray<DX11CaptureModule> >("Modules", stack_dumper.GetModules()));
	p->Append(ISO_ptr<ISO_openarray<DX11CaptureItem> >("Items", items));
	for (auto &i : recordings) {
		p->Append(ISO_ptr<memory_block>(i.type == Recording::DEVICE ? "Device" : "Commands", move(i.recording)));
	}

	return FileHandler::Write(p, fn);
}

void DX11Connection::Load(const DX11Capture *cap) {
	ISO_ptr<ISO_openarray<DX11CaptureItem> >	_items;

	for (int i = 0, n = cap->Count(); i < n; i++) {
		ISO_ptr<void>	p = (*cap)[i];
		if (p.IsID("Items")) {
			_items = p;

		} else if (p.IsID("Device")) {
			memory_block	*a = p;
			recordings.emplace_back(Recording::DEVICE, malloc_block(*a));

		} else if (p.IsID("Commands")) {
			memory_block	*a = p;
			recordings.emplace_back(Recording::CONTEXT, malloc_block(*a));
		}
	}

	items.reserve(_items->Count());
	for (auto &i : *_items)
		AddItem((RecItem::TYPE)i.type, string16(i.name), memory_block(i.info, i.info.size()), (ID3D11DeviceChild*)i.obj.i);

}

ISO_ptr_machine<void> GetBitmap0(dx11::RecItem2 *t, const void *srce) {
	tag id	= t->name;
	switch (t->type) {
		case RecItem::Texture1D: {
			const D3D11_TEXTURE1D_DESC *desc	= t->info;
			return	dx::GetBitmap(id, srce, desc->Format, desc->Width, desc->ArraySize, 1, desc->MipLevels, 0);
		}
		case RecItem::Texture2D: {
			const D3D11_TEXTURE2D_DESC *desc	= t->info;
			return dx::GetBitmap(id, srce, desc->Format, desc->Width, desc->Height, desc->ArraySize, desc->MipLevels, 0);
		}
		case RecItem::Texture3D: {
			const D3D11_TEXTURE3D_DESC *desc	= t->info;
			return dx::GetBitmap(id, srce, desc->Format, desc->Width, desc->Height, desc->Depth, desc->MipLevels, BMF_VOLUME);
		}
	}
	return ISO_NULL;
}

ISO_ptr_machine<void> DX11Connection::GetBitmap(ItemRecord *t) {
	if (!t)
		return ISO_NULL;
	if (!t->p) {
		if (const void *srce = t->GetData())
			t->p = GetBitmap0(t, srce);
	}
	return t->p;
}

void DX11Connection::GetAssets(progress prog) {
	// Get all assets
	DX11State		state;
	DX11StateParser	parser(state);
	batches.reset();

	uint32		total		= 0;
	uint32		usage_start = 0;

	for (auto &i : recordings) {
		if (i.type == Recording::CONTEXT) {
			for (const COMRecording::header *h = i.recording, *e =  (const COMRecording::header*)i.recording.end(); h < e; h = h->next()) {
				auto	p = h->data();
				switch (h->id) {
					case RecDeviceContext::tag_InitialState:
						AddState((const DeviceContext_State*)p);
						break;
					case RecDeviceContext::tag_Unmap: COMParse(p, [this](ID3D11Resource *rsrc, UINT sub) {
						AddUse(rsrc, true);
					}); break;
					case RecDeviceContext::tag_SOSetTargets: COMParse(p, [this](UINT num, counted<ID3D11Buffer*,0> pp, counted<const UINT,0> offsets) {
						for (int i = 0; i < num; i++)
							AddUse(pp[i], true);
					}); break;
					case RecDeviceContext::tag_CopySubresourceRegion: COMParse(p, [](ID3D11Resource *dst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *src, UINT SrcSubresource, const D3D11_BOX *pSrcBox) {
					}); break;
					case RecDeviceContext::tag_CopyResource: COMParse(p, [](ID3D11Resource *dst, ID3D11Resource *src) {
					}); break;
					case RecDeviceContext::tag_UpdateSubresource: COMParse(p, [this](ID3D11Resource *dst, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
						AddUse(dst, true);
					}); break;
					case RecDeviceContext::tag_CopyStructureCount: COMParse(p, [this](ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView) {
						AddUse(pDstBuffer, true);
					}); break;
					case RecDeviceContext::tag_ClearRenderTargetView: COMParse(p, [this](ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) {
						AddUse(pRenderTargetView, true);
					}); break;
					case RecDeviceContext::tag_ClearUnorderedAccessViewUint: COMParse(p, [this](ID3D11UnorderedAccessView *uav, const UINT Values[4]) {
						AddUse(uav, true);
					}); break;
					case RecDeviceContext::tag_ClearUnorderedAccessViewFloat: COMParse(p, [this](ID3D11UnorderedAccessView *uav, const FLOAT Values[4]) {
						AddUse(uav, true);
					}); break;
					case RecDeviceContext::tag_ClearDepthStencilView: COMParse(p, [this](ID3D11DepthStencilView *dsv, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
						AddUse(dsv, true);
					}); break;
					case RecDeviceContext::tag_GenerateMips: COMParse(p, [this](ID3D11ShaderResourceView *pShaderResourceView) {
						AddUse(pShaderResourceView, true);
					}); break;
				}

				if (parser.Process(this, h)) {
					auto	*b		= AddBatch(h->id, ((uint8*)h->next() - i.recording) + total, usage_start, h->data());
					auto	tag		= (RecDeviceContext::tag)h->id;
					bool	compute	= is_compute(tag);
					bool	draw	= is_draw(tag);

					if (compute || draw) {
						for (auto &s : state.Stages(compute)) {
							if (ShaderRecord *shader = FindShader(s.shader)) {
								AddUse(shader->item);

								dx::DeclResources	decl;
								Read(decl, shader->GetUCode());

								for (auto &i : decl.cb)
									AddUse(s.cb[i.index()]);
								for (auto &i : decl.srv)
									AddUse(s.srv[i.index()]);
								for (auto &i : decl.smp)
									AddUse(s.smp[i.index()]);

								for (auto &i : decl.uav)
									AddUse((compute ? state.cs_uav : state.ps_uav)[i.index()]);
							}
						}
					}

					if (is_indirect(tag))
						AddUse(state.IndirectBuffer);

					if (compute) {
						store(state.ThreadGroupCount, b->dim);

					} else if (draw) {
						b->prim				= state.prim;
						b->num_instances	= state.InstanceCount;
						b->num_verts		= state.VertexCount;
						b->rtv				= state.rtv[0];

						if (is_indexed(tag))
							AddUse(state.ib.buffer);

						if (auto *ia = FindItem(state.ia)) {
							COMParse2(ia->info, [this, &state](counted<const D3D11_INPUT_ELEMENT_DESC,1> desc, UINT num, counted<const uint8,3> code, SIZE_T length) {
								for (int i = 0; i < num; i++)
									AddUse(state.vb[i].buffer);
							});
						}

						AddUse(state.dsv);

						for (auto &i : state.rtv)
							AddUse(i);

						AddUse(state.blend);
						AddUse(state.depth_stencil);
					}

					usage_start = uses.size32();
				}
			}
		}

		total += i.recording.size32();
	}

	AddBatch(0, total, uses.size32(), nullptr);	//dummy end batch
}

bool DX11Connection::GetStatistics() {
	DX11Replay	replay;
	replay.CreateObjects(*this);

	D3D11_QUERY_DESC		desc;
	desc.MiscFlags		= 0;

	fiber_generator<uint32>	fg([this, &replay]() { replay.RunGen(recordings); }, 1 << 17);
	auto	rp	= fg.begin();

#if 1
	com_ptr<ID3D11Query>	query_disjoint;

	desc.MiscFlags	= 0;
	desc.Query		= D3D11_QUERY_TIMESTAMP_DISJOINT;
	replay.device->CreateQuery(&desc, &query_disjoint);
	replay.ctx->Begin(query_disjoint);

	dynamic_array<ID3D11Query*>	queries(batches.size() * 2);
	auto	q	= queries.begin();

	for (auto &b : batches) {
		int		i		= batches.index_of(b);

		desc.Query		= D3D11_QUERY_TIMESTAMP;
		replay.device->CreateQuery(&desc, q);
		replay.ctx->End(q[0]);

		desc.Query		= D3D11_QUERY_PIPELINE_STATISTICS;
		replay.device->CreateQuery(&desc, q + 1);
		replay.ctx->Begin(q[1]);

		while (*rp < b.addr) {
//			ISO_TRACEF("run: 0x%08x\n", *rp);
			++rp;
		}

		replay.ctx->End(q[1]);

		q += 2;
	}

	replay.ctx->End(query_disjoint);
	D3D10_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
	while (replay.ctx->GetData(query_disjoint, &disjoint, sizeof(disjoint), 0) == S_FALSE)
		Sleep(1);

	frequency = disjoint.Frequency;

	// Get all the timestamps
	q = queries.begin();
	for (auto &i : batches) {
		while (replay.ctx->GetData(q[0], &i.timestamp, sizeof(uint64), 0) == S_FALSE)
			Sleep(1);
		while (replay.ctx->GetData(q[1], &i.stats, sizeof(i.stats), 0) == S_FALSE)
			Sleep(1);
		q += 2;
	}

	for (auto i : queries)
		i->Release();

	auto	t0 = batches[0].timestamp;
	for (auto &i : batches)
		i.timestamp -= t0;

	// Check whether timestamps were disjoint during the last frame
	return !disjoint.Disjoint;

#else
	dynamic_array<ID3D11Query*>	queries(batches.size() * 4);
	auto	q	= queries.begin();

	for (auto &b : batches) {
		int		i		= batches.index_of(b);

		desc.Query		= D3D11_QUERY_TIMESTAMP_DISJOINT;
		replay.device->CreateQuery(&desc, q + 0);
		replay.ctx->Begin(q[0]);

		desc.Query		= D3D11_QUERY_TIMESTAMP;
		replay.device->CreateQuery(&desc, q + 1);
		replay.ctx->End(q[1]);

		desc.Query		= D3D11_QUERY_PIPELINE_STATISTICS;
		replay.device->CreateQuery(&desc, q + 2);
		replay.ctx->Begin(q[2]);

		while (*rp < b.addr)
			++rp;

		replay.ctx->End(q[2]);

		desc.Query		= D3D11_QUERY_TIMESTAMP;
		replay.device->CreateQuery(&desc, q + 3);
		replay.ctx->End(q[3]);

		replay.ctx->End(q[0]);

		q += 4;
	}

	// Get all the timestamps
	q = queries.begin();
	uint64	time_total = 0;
	for (auto &i : batches) {
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
		uint64	time_start, time_end;
		while (replay.ctx->GetData(q[0], &disjoint, sizeof(disjoint), 0) == S_FALSE)
			Sleep(1);
		ISO_ASSERT(!disjoint.Disjoint);
		frequency = disjoint.Frequency;
		while (replay.ctx->GetData(q[1], &time_start, sizeof(uint64), 0) == S_FALSE)
			Sleep(1);
		while (replay.ctx->GetData(q[2], &i.stats, sizeof(i.stats), 0) == S_FALSE)
			Sleep(1);
		while (replay.ctx->GetData(q[3], &time_end, sizeof(uint64), 0) == S_FALSE)
			Sleep(1);

		i.timestamp = time_total;
		time_total += time_end - time_start;
		q += 4;
	}

	for (auto i : queries)
		i->Release();
	
	return true;

#endif
}

//-----------------------------------------------------------------------------
//	DX11ShaderState
//-----------------------------------------------------------------------------

union RESOURCE_DESC {
	D3D11_BUFFER_DESC		buffer;
	D3D11_TEXTURE1D_DESC	tex1d;
	D3D11_TEXTURE2D_DESC	tex2d;
	D3D11_TEXTURE3D_DESC	tex3d;
};

struct DX11ShaderState : dx::Shader {
	struct CBV : dx::Buffer {
		DX11Assets::ItemRecord	*item;
		void	init(DX11Assets::ItemRecord	*_item, const D3D11_BUFFER_DESC *desc, const void *data) {
			item	= _item;
			p		= (void*)data;
			n		= desc->ByteWidth;
		}
	};
	struct SRV : dx::Resource {
		DX11Assets::ItemRecord	*item;
		bool	own_mem;
		int		first_sub;
		using dx::Resource::init;
		SRV() : item(0), own_mem(false), first_sub(0) {}
		~SRV() { if (own_mem) iso::free(p); }
		SRV&	init(DX11Assets::ItemRecord	*_item, const D3D11_SHADER_RESOURCE_VIEW_DESC *desc, DX11Assets::ItemRecord *res);
	};
	struct UAV : dx::Resource {
		DX11Assets::ItemRecord	*item;
		bool	own_mem;
		int		first_sub;
		using dx::Resource::init;
		UAV() : item(0), own_mem(false), first_sub(0) {}
		~UAV() { if (own_mem) iso::free(p); }
		UAV&	init(DX11Assets::ItemRecord	*_item, const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc, DX11Assets::ItemRecord *res);
	};

	const char16 *name;
	hash_map<uint32, CBV>		cb;
	hash_map<uint32, SRV>		srv;
	hash_map<uint32, UAV>		uav;
	hash_map<uint32, DX11Assets::ItemRecord*>	smp;
	hash_map<dx::SystemValue, uint32>	inputs, outputs;
	uint32						non_system_inputs;

	DX11ShaderState()	{}
	DX11ShaderState(const DX11Assets::ShaderRecord *rec) : dx::Shader(rec->stage, rec->item->info), name(rec->item->name) {}
	DX11ShaderState(const DX11Assets::ItemRecord *item) : dx::Shader(GetStage(item->type), item->info), name(item->name) {}
	bool	init(DX11Connection *con, const DX11State::Stage &stage, ID3D11UnorderedAccessView *const *stage_uav, const DX11State::mods_t &mods);
	void	InitSimulator(dx::SimulatorDXBC &sim, uint32 start, const Topology2 &top) const;

	auto	GetThreadGroup() const { return dx::GetThreadGroupDXBC(GetUCode()); }

	Disassembler::State *Disassemble() const {
		static Disassembler	*dis = Disassembler::Find("DXBC");
		if (dis)
			return dis->Disassemble(GetUCode(), GetUCodeAddr());
		return 0;
	}
};

bool DX11ShaderState::init(DX11Connection *con, const DX11State::Stage &stage, ID3D11UnorderedAccessView *const *stage_uav, const DX11State::mods_t &mods) {
	non_system_inputs = 0;

	auto rec = con->FindShader(stage.shader);
	if (!rec)
		return false;

	name = rec->item->name;

	dx::Shader::init(rec->stage, rec->item->info);
	
	dx::DeclResources	decl;
	Read(decl, GetUCode());

	for (auto &i : decl.inputs) {
		if (i->system)
			inputs[i->system]	= i.index();
		else
			non_system_inputs |= 1 << i.index();
	}
	for (auto &i : decl.outputs) {
		if (i->system)
			outputs[i->system]	= i.index();
	}

	for (auto &i : decl.cb) {
		if (auto *item = con->FindItem(stage.cb[i.index()])) {
			cb[i.index()]->init(item, item->info, get(mods[tagged_pointer<ID3D11Resource>(stage.cb[i.index()], 0)].or_default(item->GetData())));
		}
	}
	for (auto &i : decl.srv) {
		if (auto *item = con->FindItem(stage.srv[i.index()])) {
			const RecView<D3D11_SHADER_RESOURCE_VIEW_DESC> *desc = item->info;
			auto	&s	= srv[i.index()]->init(item, desc->pdesc, con->FindItem(desc->resource));
			bool	mod = false;
			for (int i = s.first_sub, n = i + s.num_subs(); i < n; i++) {
				if (mods[tagged_pointer<ID3D11Resource>((ID3D11Resource*)s.item->obj, i)].exists()) {
					mod = true;
					break;
				}
			}
			if (mod) {
				intptr_t	offset = (uint8*)s - (const uint8*)s.item->GetData();
				duplicate(s);
				s.own_mem	= true;
				for (int i = s.first_sub, n = i + s.num_subs(); i < n; i++) {
					auto	mod = mods[tagged_pointer<ID3D11Resource>((ID3D11Resource*)s.item->obj, i)];
					if (mod.exists())
						(s + (s.item->GetSubResourceOffset(i) - offset)).copy_from(mod->begin());
				}
			}

		}
	}

	if (stage_uav) {
		for (auto &i : decl.uav) {
			if (auto *item = con->FindItem(stage_uav[i.index()])) {
				const RecView<D3D11_UNORDERED_ACCESS_VIEW_DESC> *desc = item->info;
				auto	&s	= uav[i.index()]->init(item, desc->pdesc, con->FindItem(desc->resource));
				bool	mod = false;
				for (int i = s.first_sub, n = i + s.num_subs(); i < n; i++) {
					if (mods[tagged_pointer<ID3D11Resource>((ID3D11Resource*)s.item->obj, i)].exists()) {
						mod = true;
						break;
					}
				}
				if (mod) {
					intptr_t	offset = (uint8*)s - (const uint8*)s.item->GetData();
					duplicate(s);
					s.own_mem	= true;
					for (int i = s.first_sub, n = i + s.num_subs(); i < n; i++) {
						auto	mod = mods[tagged_pointer<ID3D11Resource>((ID3D11Resource*)s.item->obj, i)];
						if (mod.exists())
							(s + (s.item->GetSubResourceOffset(i) - offset)).copy_from(mod->begin());
					}
				}
			}
		}
	}

	for (auto &i : decl.smp) {
		if (auto *item = con->FindItem(stage.smp[i.index()]))
			smp[i.index()] = item;
	}

	return true;
}

DX11ShaderState::SRV& DX11ShaderState::SRV::init(DX11Assets::ItemRecord	*_item, const D3D11_SHADER_RESOURCE_VIEW_DESC *vdesc, DX11Assets::ItemRecord *res) {
	item		= _item;
	if (res) {
		const RESOURCE_DESC	*rdesc	= res->info;
		set_mem(unconst(res->GetData()));

		switch (vdesc ? vdesc->ViewDimension : D3D11_SRV_DIMENSION_UNKNOWN) {
			case D3D11_SRV_DIMENSION_UNKNOWN: {
				switch (res->type) {
					case RecItem::Buffer:
						init(dx::RESOURCE_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, rdesc->buffer.StructureByteStride, rdesc->buffer.ByteWidth);
						break;
					case RecItem::Texture1D:
						init(dx::RESOURCE_DIMENSION_TEXTURE1D, rdesc->tex1d.Format, rdesc->tex1d.Width, 0, rdesc->tex1d.ArraySize, rdesc->tex1d.MipLevels);
						break;
					case RecItem::Texture2D:
						init(dx::RESOURCE_DIMENSION_TEXTURE2D, rdesc->tex2d.Format, rdesc->tex2d.Width, rdesc->tex2d.Height, rdesc->tex2d.ArraySize, rdesc->tex2d.MipLevels);
						break;
					case RecItem::Texture3D:
						init(dx::RESOURCE_DIMENSION_TEXTURE3D, rdesc->tex3d.Format, rdesc->tex3d.Width, rdesc->tex3d.Height, rdesc->tex3d.Depth, rdesc->tex3d.MipLevels);
						break;
				}
				break;
			}
			case D3D11_SRV_DIMENSION_BUFFER: {
				uint32	element_size = rdesc->buffer.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ? rdesc->buffer.StructureByteStride : DXGI_COMPONENTS(vdesc->Format).Bytes();
				init(dx::RESOURCE_DIMENSION_BUFFER, vdesc->Format, element_size, vdesc->Buffer.NumElements);
				set_mem(slice(vdesc->Buffer.FirstElement * element_size));
				break;
			}

			case D3D11_SRV_DIMENSION_TEXTURE1D:
				init(dx::RESOURCE_DIMENSION_TEXTURE1D, vdesc->Format, rdesc->tex1d.Width, 0);
				set_mips(first_sub = vdesc->Texture1D.MostDetailedMip, vdesc->Texture1D.MipLevels);
				break;

			case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
				init(dx::RESOURCE_DIMENSION_TEXTURE1DARRAY, vdesc->Format, rdesc->tex1d.Width, 0, vdesc->Texture1DArray.ArraySize);
				set_mips(vdesc->Texture1DArray.MostDetailedMip, vdesc->Texture1DArray.MipLevels);
				set_slices(vdesc->Texture1DArray.FirstArraySlice, vdesc->Texture1DArray.ArraySize);
				first_sub = vdesc->Texture1DArray.FirstArraySlice * rdesc->tex1d.MipLevels + vdesc->Texture1DArray.MostDetailedMip;
				break;

			case D3D11_SRV_DIMENSION_TEXTURE2D:
				init(dx::RESOURCE_DIMENSION_TEXTURE2D, vdesc->Format, rdesc->tex3d.Width, rdesc->tex3d.Height);
				set_mips(first_sub = vdesc->Texture2D.MostDetailedMip, vdesc->Texture2D.MipLevels);
				break;

			case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
				init(dx::RESOURCE_DIMENSION_TEXTURE2DARRAY, vdesc->Format, rdesc->tex2d.Width, rdesc->tex2d.Height, vdesc->Texture2DArray.FirstArraySlice + vdesc->Texture2DArray.ArraySize);
				set_mips(vdesc->Texture2DArray.MostDetailedMip, vdesc->Texture2DArray.MipLevels);
				set_slices(vdesc->Texture2DArray.FirstArraySlice, vdesc->Texture2DArray.ArraySize);
				first_sub = vdesc->Texture2DArray.FirstArraySlice * rdesc->tex2d.MipLevels + vdesc->Texture2DArray.MostDetailedMip;
				break;

			//case D3D11_SRV_DIMENSION_TEXTURE2DMS:
			//case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
			case D3D11_SRV_DIMENSION_TEXTURE3D:
				init(dx::RESOURCE_DIMENSION_TEXTURE3D, vdesc->Format, rdesc->tex3d.Width, rdesc->tex3d.Height, rdesc->tex3d.Depth);
				set_mips(first_sub = vdesc->Texture3D.MostDetailedMip, vdesc->Texture3D.MipLevels);
				break;

			case D3D11_SRV_DIMENSION_TEXTURECUBE:
				init(dx::RESOURCE_DIMENSION_TEXTURECUBE, vdesc->Format, rdesc->tex2d.Width, rdesc->tex2d.Height, 6);
				set_mips(vdesc->TextureCube.MostDetailedMip, vdesc->TextureCube.MipLevels);
				first_sub = vdesc->TextureCube.MostDetailedMip * 6;
				break;

			case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
				init(dx::RESOURCE_DIMENSION_TEXTURECUBEARRAY, vdesc->Format, rdesc->tex2d.Width, rdesc->tex2d.Height, vdesc->TextureCubeArray.First2DArrayFace + vdesc->TextureCubeArray.NumCubes * 6);
				set_mips(vdesc->TextureCubeArray.MostDetailedMip, vdesc->TextureCubeArray.MipLevels);
				set_slices(vdesc->TextureCubeArray.First2DArrayFace, vdesc->TextureCubeArray.NumCubes * 6);
				first_sub = (vdesc->TextureCubeArray.First2DArrayFace * rdesc->tex2d.MipLevels + vdesc->TextureCube.MostDetailedMip) * 6;
				break;
		}
	}
	return *this;
}

DX11ShaderState::UAV& DX11ShaderState::UAV::init(DX11Assets::ItemRecord *_item, const D3D11_UNORDERED_ACCESS_VIEW_DESC *vdesc, DX11Assets::ItemRecord *res) {
	item		= _item;
	if (res) {
		const RESOURCE_DESC	*rdesc	= res->info;
		set_mem(unconst(res->GetData()));

		switch (vdesc ? vdesc->ViewDimension : D3D11_SRV_DIMENSION_UNKNOWN) {
			case D3D11_SRV_DIMENSION_UNKNOWN: {
				switch (res->type) {
					case RecItem::Buffer:
						init(dx::RESOURCE_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, rdesc->buffer.StructureByteStride, rdesc->buffer.ByteWidth);
						break;
					case RecItem::Texture1D:
						init(dx::RESOURCE_DIMENSION_TEXTURE1D, rdesc->tex1d.Format, rdesc->tex1d.Width, 0, rdesc->tex1d.ArraySize);
						break;
					case RecItem::Texture2D:
						init(dx::RESOURCE_DIMENSION_TEXTURE2D, rdesc->tex2d.Format, rdesc->tex2d.Width, rdesc->tex2d.Height, rdesc->tex2d.ArraySize);
						break;
					case RecItem::Texture3D:
						init(dx::RESOURCE_DIMENSION_TEXTURE3D, rdesc->tex3d.Format, rdesc->tex3d.Width, rdesc->tex3d.Height, rdesc->tex3d.Depth);
						break;
				}
				break;
			}
			case D3D11_UAV_DIMENSION_BUFFER: {
				uint32	element_size = rdesc->buffer.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ? rdesc->buffer.StructureByteStride : DXGI_COMPONENTS(vdesc->Format).Bytes();
				init(dx::RESOURCE_DIMENSION_BUFFER, vdesc->Format, element_size, vdesc->Buffer.NumElements);
				set_mem(slice(vdesc->Buffer.FirstElement * element_size));
				break;
			}

			case D3D11_UAV_DIMENSION_TEXTURE1D:
				init(dx::RESOURCE_DIMENSION_TEXTURE1D, vdesc->Format, rdesc->tex1d.Width, 0);
				set_mips(first_sub = vdesc->Texture1D.MipSlice, 1);
				break;

			case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
				init(dx::RESOURCE_DIMENSION_TEXTURE1DARRAY, vdesc->Format, rdesc->tex2d.Width, vdesc->Texture1DArray.FirstArraySlice + vdesc->Texture1DArray.ArraySize);
				set_mips(vdesc->Texture1DArray.MipSlice, 1);
				set_slices(vdesc->Texture1DArray.FirstArraySlice, vdesc->Texture1DArray.ArraySize);
				first_sub = vdesc->Texture1DArray.FirstArraySlice * rdesc->tex1d.MipLevels + vdesc->Texture1DArray.MipSlice;
				break;

			case D3D11_UAV_DIMENSION_TEXTURE2D:
				init(dx::RESOURCE_DIMENSION_TEXTURE2D, vdesc->Format, rdesc->tex2d.Width, rdesc->tex2d.Height);
				set_mips(first_sub = vdesc->Texture2D.MipSlice, 1);
				break;

			case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
				init(dx::RESOURCE_DIMENSION_TEXTURE2DARRAY, vdesc->Format, rdesc->tex2d.Width, rdesc->tex2d.Height, vdesc->Texture2DArray.FirstArraySlice + vdesc->Texture2DArray.ArraySize);
				set_mips(vdesc->Texture2DArray.MipSlice, 1);
				set_slices(vdesc->Texture2DArray.FirstArraySlice, vdesc->Texture2DArray.ArraySize);
				first_sub = vdesc->Texture2DArray.FirstArraySlice * rdesc->tex2d.MipLevels + vdesc->Texture2DArray.MipSlice;
				break;

			case D3D11_UAV_DIMENSION_TEXTURE3D:
				init(dx::RESOURCE_DIMENSION_TEXTURE3D, vdesc->Format, rdesc->tex3d.Width, rdesc->tex3d.Height, rdesc->tex3d.Depth);
				set_mips(first_sub = vdesc->Texture3D.MipSlice, 1);
				break;
		}
	}
	return *this;
};

void DX11ShaderState::InitSimulator(dx::SimulatorDXBC &sim, uint32 start, const Topology2 &top) const {
	for (auto i : with_iterator(cb))
		sim.cbv[i.hash()]	= *i;

	for (auto i : with_iterator(srv))
		sim.srv[i.hash()]	= *i;

	for (auto i : with_iterator(uav))
		sim.uav[i.hash()]	= *i;

	for (auto i : with_iterator(smp))
		sim.smp[i.hash()]	= *(*i)->info;

	for (auto &i : with_iterator(inputs)) {
		switch (i.hash()) {
			case dx::SV_VERTEX_ID:
				if (top.chunks)
					sim.SetInput<int>(*i, transform(make_int_iterator(start), [per_instance = top.chunks](int i) { return i % per_instance; }));
				else
					sim.SetInput<int>(*i, make_int_iterator(start));
				break;
			case dx::SV_PRIMITIVE_ID:
				sim.SetInput<int>(*i, transform(make_int_iterator(start), [&top](int i) { return top.PrimFromVertex(i, false); }));
				break;
			case dx::SV_INSTANCE_ID:
				if (top.chunks)
					sim.SetInput<int>(*i, transform(make_int_iterator(start), [per_instance = top.chunks](int i) { return i / per_instance; }));
				else
					sim.SetInput<int>(*i, scalar(0));
				break;

			default:
				break;
		}
	}
}

Control MakeBoundView(const WindowPos &wpos, const DX11ShaderState &stage, int index) {
	int		i	= index & 63;
	switch ((index / 64) & 3) {
		case 0: {//cb
			if (DX11ShaderState::CBV	&cbv = stage.cb[i])
				return MakeBufferWindow(wpos, cbv.item->GetName(), 'BF', TypedBuffer(cbv, 16, ctypes.get_type<float[4]>()));
			break;
		}
		case 1: {//srv
			if (DX11ShaderState::SRV	&srv = stage.srv[i])
				return is_buffer(srv.dim)
					? MakeBufferWindow(wpos, srv.item->GetName(), 'BF', TypedBuffer(srv, srv.width, dx::to_c_type(srv.format)))//, 16, ctypes.get_type<float[4]>()))
					: BitmapWindow(wpos, dx::GetBitmap(0, srv), srv.item->GetName(), true);
			break;
		}
		case 2: {//uav
			if (DX11ShaderState::UAV	&uav = stage.uav[i])
				return is_buffer(uav.dim)
					? MakeBufferWindow(wpos, uav.item->GetName(), 'BF', TypedBuffer(uav, uav.width, dx::to_c_type(uav.format)))//, 16, ctypes.get_type<float[4]>()))
					: BitmapWindow(wpos, dx::GetBitmap(0, uav), uav.item->GetName(), true);
			break;
		};
	}
	return Control();
}

//-----------------------------------------------------------------------------
//	DX11StateControl
//-----------------------------------------------------------------------------

struct DX11StateControl : public ColourTree {
	enum {ID = 'SC'};
	enum TYPE {
		ST_TARGET	= 1,
		ST_SHADER,
		ST_RESOURCE,
		ST_BOUND_RESOURCE,
		ST_VERTICES,
		ST_OUTPUTS,
	};
	static win::Colour	colours[];
	static uint8		cursor_indices[][3];

	DX11Connection		*con;
	DX11StateControl(DX11Connection *_con) : ColourTree(colours, DX11cursors, cursor_indices), con(_con) {}

	HTREEITEM	AddShader(HTREEITEM h, const DX11ShaderState &shader);

	HWND		Create(const WindowPos &wpos) {
		return ColourTree::Create(wpos, "state", CHILD | CLIPSIBLINGS | VISIBLE | HASLINES | HASBUTTONS | LINESATROOT, NOEX, ID);
	}
		
	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		buffer_accum<256>	ba;
		PutFieldName(ba, tree.format, pf);

		uint64		v = pf->get_raw_value(p, offset);
		if (auto *rec = con->FindItem((ID3D11DeviceChild*)v)) {
			tree.Add(h, ba << rec->name, ST_RESOURCE, rec);
		} else {
			tree.AddText(h, ba << "0x" << hex(v));//, ST_GPUREG, addr + offset);
		}
	}
};

win::Colour DX11StateControl::colours[] = {
	{0,0,0},
	{128,0,0},		//ST_TARGET
	{128,0,64},		//ST_SHADER
	{0,128,0},		//ST_RESOURCE,
	{64,128,0},		//ST_BOUND_RESOURCE
	{64,0,128},		//ST_VERTICES
	{0,0,0},		//ST_OUTPUTS,
};
uint8 DX11StateControl::cursor_indices[][3] = {
	{0,0,0},
	{1,2,0},	//ST_TARGET
	{1,2,0},	//ST_SHADER
	{1,2,0},	//ST_RESOURCE,
	{1,2,0},	//ST_BOUND_RESOURCE
	{1,2,0},	//ST_VERTICES
	{1,2,0},	//ST_OUTPUTS,
};

HTREEITEM AddShaderReflection(RegisterTree &rt, HTREEITEM h, ID3D11ShaderReflection *refl) {
	D3D11_SHADER_DESC				desc;
	refl->GetDesc(&desc);

	rt.AddFields(rt.AddText(h, "D3D11_SHADER_DESC"), &desc);

	HTREEITEM	h2 = rt.AddText(h, "ConstantBuffers");
	for (int i = 0; i < desc.ConstantBuffers; i++) {
		ID3D11ShaderReflectionConstantBuffer*	cb = refl->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC				cb_desc;
		cb->GetDesc(&cb_desc);

		HTREEITEM	h3 = rt.AddText(h2, cb_desc.Name);
		rt.AddText(h3, format_string("size = %i", cb_desc.Size));

		for (int i = 0; i < cb_desc.Variables; i++) {
			ID3D11ShaderReflectionVariable*		var = cb->GetVariableByIndex(i);
			D3D11_SHADER_VARIABLE_DESC			var_desc;
			var->GetDesc(&var_desc);

			HTREEITEM	h4 = rt.AddText(h3, var_desc.Name);
			rt.AddText(h4, format_string("Slot = %i", var->GetInterfaceSlot(0)));

			rt.AddFields(rt.AddText(h4, "D3D11_SHADER_VARIABLE_DESC"), &var_desc);

			D3D11_SHADER_TYPE_DESC				type_desc;
			ID3D11ShaderReflectionType*			type = var->GetType();
			type->GetDesc(&type_desc);
			rt.AddFields(rt.AddText(h4, "D3D11_SHADER_TYPE_DESC"), &type_desc);
		}
	}

	h2 = rt.AddText(h, "BoundResources");
	for (int i = 0; i < desc.BoundResources; i++) {
		D3D11_SHADER_INPUT_BIND_DESC	res_desc;
		refl->GetResourceBindingDesc(i, &res_desc);
		rt.AddFields(rt.AddText(h2, res_desc.Name), &res_desc);
	}

	h2 = rt.AddText(h, "InputParameters");
	for (int i = 0; i < desc.InputParameters; i++) {
		D3D11_SIGNATURE_PARAMETER_DESC	in_desc;
		refl->GetInputParameterDesc(i, &in_desc);
		rt.AddFields(rt.AddText(h2, in_desc.SemanticName), &in_desc);
	}

	h2 = rt.AddText(h, "OutputParameters");
	for (int i = 0; i < desc.OutputParameters; i++) {
		D3D11_SIGNATURE_PARAMETER_DESC	out_desc;
		refl->GetOutputParameterDesc(i, &out_desc);
		rt.AddFields(rt.AddText(h2, out_desc.SemanticName), &out_desc);
	}
	return h;
}

void AddValue(RegisterTree &rt, HTREEITEM h, const char *name, const void *data, ID3D11ShaderReflectionType *type) {
	D3D11_SHADER_TYPE_DESC	type_desc;
	if (FAILED(type->GetDesc(&type_desc)))
		return;

	if (type_desc.Class == D3D_SVC_STRUCT) {
		buffer_accum<256>	ba(name);
		HTREEITEM	h2 = rt.AddText(h, ba);
		for (int i = 0; i < type_desc.Members; i++)
			AddValue(rt, h2, type->GetMemberTypeName(i), data, type->GetMemberTypeByIndex(i));

	} else {
		AddValue(rt, h, name, data, type_desc.Class, type_desc.Type, type_desc.Rows, type_desc.Columns);
	}
}


HTREEITEM	DX11StateControl::AddShader(HTREEITEM h, const DX11ShaderState &shader) {
	com_ptr<ID3D11ShaderReflection>	refl;
	RegisterTree	rt(*this, this, IDFMT_FOLLOWPTR);

	if (SUCCEEDED(D3DReflect(shader.data, shader.data.length(), IID_ID3D11ShaderReflection, (void**)&refl))) {
		AddShaderReflection(rt, rt.AddText(h, "ID3D11ShaderReflection"), refl);

		HTREEITEM		h2 = rt.AddText(h, "BoundValues");
		D3D11_SHADER_DESC	desc;
		refl->GetDesc(&desc);

		for (int i = 0; i < desc.BoundResources; i++) {
			D3D11_SHADER_INPUT_BIND_DESC	res_desc;
			refl->GetResourceBindingDesc(i, &res_desc);

			switch (res_desc.Type) {
				case D3D_SIT_CBUFFER:
					if (auto *bound = shader.cb.check(res_desc.BindPoint)) {
						HTREEITEM	h3 = rt.Add(h2, format_string("%s : b%i", res_desc.Name, res_desc.BindPoint), ST_BOUND_RESOURCE, shader.stage * 256 + 0 + res_desc.BindPoint);
						ID3D11ShaderReflectionConstantBuffer	*cb = refl->GetConstantBufferByName(res_desc.Name);
						D3D11_SHADER_BUFFER_DESC				cb_desc;
						cb->GetDesc(&cb_desc);

						uint8	*data	= *bound;
						for (int i = 0; i < cb_desc.Variables; i++) {
							ID3D11ShaderReflectionVariable*		var = cb->GetVariableByIndex(i);
							D3D11_SHADER_VARIABLE_DESC			var_desc;
							if (SUCCEEDED(var->GetDesc(&var_desc)) && (var_desc.uFlags & D3D_SVF_USED))
								AddValue(rt, h3, var_desc.Name, (const uint8*)data + var_desc.StartOffset, var->GetType());
						}
					}
					break;

				case D3D_SIT_TBUFFER:
				case D3D_SIT_TEXTURE:
				case D3D_SIT_STRUCTURED:
					if (auto *bound = shader.srv.check(res_desc.BindPoint))
						rt.AddFields(rt.Add(h2, format_string("%s : t%i", res_desc.Name, res_desc.BindPoint), ST_BOUND_RESOURCE, shader.stage * 256 + 64 + res_desc.BindPoint), bound->item);
					break;

				case D3D_SIT_SAMPLER:
					if (auto *bound = shader.smp.check(res_desc.BindPoint))
						rt.AddFields(rt.AddText(h2, format_string("%s : s%i", res_desc.Name, res_desc.BindPoint)), bound);
					break;

				default:
					if (auto *bound = shader.uav.check(res_desc.BindPoint))
						rt.AddFields(rt.Add(h2, format_string("%s : u%i", res_desc.Name, res_desc.BindPoint), ST_BOUND_RESOURCE, shader.stage * 256 + 128 + res_desc.BindPoint), bound->item);
					break;
			}
		}

	}

	HTREEITEM	h2	= rt.AddText(h, "Registers");
	dx::DeclResources	decl;
	Read(decl, shader.GetUCode());

//	for (auto &i : decl.inputs) {
//		if (i.system)
//			rt.AddText(h2, "input");//(dx::SystemValue)i.system, i.index);
//	}
//	for (auto &i : decl.outputs) {
//		if (i.system)
//			rt.AddText(h2, "output");//outputs[(dx::SystemValue)i.system]	= i.index;
//	}

	for (auto &i : decl.cb) {
		if (auto *bound = shader.cb.check(i.index()))
			rt.AddFields(rt.Add(h2, buffer_accum<256>("b") << i.index(), ST_BOUND_RESOURCE, shader.stage * 256 + 0 + i.index()), bound->item);
	}
	for (auto &i : decl.srv) {
		if (auto *bound = shader.srv.check(i.index()))
			rt.AddFields(rt.Add(h2, buffer_accum<256>("t") << i.index(), ST_BOUND_RESOURCE, shader.stage * 256 + 64 + i.index()), bound->item);
	}
	for (auto &i : decl.uav) {
		if (auto *bound = shader.uav.check(i.index()))
			rt.AddFields(rt.Add(h2, buffer_accum<256>("u") << i.index(), ST_BOUND_RESOURCE, shader.stage * 256 + 128 + i.index()), bound->item);
	}
	for (auto &i : decl.smp) {
		if (auto *bound = shader.smp.check(i.index()))
			rt.AddFields(rt.AddText(h2, buffer_accum<256>("s") << i.index()), bound);
	}
		
	return h;
}

Control MakeItemView(const WindowPos &wpos, DX11Connection *con, DX11Assets::ItemRecord *rec);

//-----------------------------------------------------------------------------
//	DX11ShaderWindow
//-----------------------------------------------------------------------------

struct DX11ShaderWindow : SplitterWindow, CodeHelper {
	DX11StateControl		tree;
	TabControl3				*tabs[2];
	const DX11ShaderState	&shader;
	
	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	DX11ShaderWindow(const WindowPos &wpos, DX11Connection *_con, const DX11ShaderState &shader);
};

LRESULT	DX11ShaderWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_COMMAND:
			if (HIWORD(wParam) == EN_SETFOCUS && tabs[0] && tabs[1]) {
				Select((HWND)lParam, (EditControl)tabs[0]->GetSelectedControl(), tabs[1]);
				return 0;
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					break;

				case NM_CLICK:
					if (nmh->hwndFrom == tree) {
						if (HTREEITEM hItem = tree.hot) {
							TreeControl::Item	i = tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE);
							bool	new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);
							switch (i.Image()) {
								case DX11StateControl::ST_RESOURCE:
									Docker(*this).Dock(new_tab ? DOCK_RIGHT : DOCK_TABID,  MakeItemView(GetChildWindowPos(), tree.con, i.Param()));
									break;

								case DX11StateControl::ST_BOUND_RESOURCE:
									Docker(*this).Dock(new_tab ? DOCK_RIGHT : DOCK_TABID,  MakeBoundView(GetChildWindowPos(), shader, i.Param()));
									break;
							}
						}
					}
					break;
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

DX11ShaderWindow::DX11ShaderWindow(const WindowPos &wpos, DX11Connection *con, const DX11ShaderState &shader)
	: SplitterWindow(SWF_VERT | SWF_PROP)
	, CodeHelper(HLSLcolourerRE(), none, GetSettings("General/shader source").Get(Disassembler::SHOW_SOURCE))
	, tree(con)
	, shader(shader)
{
	Create(wpos, str8(shader.name), CHILD | CLIPCHILDREN | VISIBLE);
	Rebind(this);

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
	split2->Create(_GetPanePos(0), 0, CHILD | CLIPSIBLINGS | VISIBLE);

	ParsedSPDB		spdb(memory_reader(shader.DXBC()->GetBlob(dx::DXBC::ShaderPDB)), con->files, con->shader_path);
	locations = move(spdb.locations);
	FixLocations(shader.GetUCodeAddr());

	// code tabs
	tabs[0] = new TabControl3(split2->_GetPanePos(0), "code", CHILD | CLIPSIBLINGS);
	tabs[0]->SetFont(win::Font::DefaultGui());
	
	D2DTextWindow *tw = new D2DTextWindow(split2->_GetPanePos(0), "Original", CHILD | TextWindow::READONLY);
	CodeHelper::SetDisassembly(*tw, shader.Disassemble(), spdb.files);

	tw->Show();
	tabs[0]->AddItemControl(*tw, "Disassembly");

	// tree
	TitledWindow	*tree_title		= new TitledWindow(split2->_GetPanePos(1), "Bound");
	tree.Create(tree_title->GetChildWindowPos());
	split2->SetPanes(*tabs[0], *tree_title, 100);
	tree.AddShader(TVI_ROOT, shader);
	tabs[0]->Show();

	// source tabs
	if (!spdb.files.empty()) {
		tabs[1] = new TabControl3(_GetPanePos(1), "source", CHILD | CLIPSIBLINGS | VISIBLE);
		tabs[1]->SetFont(win::Font::DefaultGui());
		SourceTabs(*tabs[1], spdb.files);
		SetPanes(*split2, *tabs[1], 50);
		tabs[1]->ShowSelectedControl();
	} else {
		tabs[1] = 0;
		SetPane(0, *split2);
	}
}

//-----------------------------------------------------------------------------
//	DX11ShaderDebuggerWindow
//-----------------------------------------------------------------------------

class DX11ShaderDebuggerWindow : public MultiSplitterWindow, DebugWindow {
	Accelerator				accel;
	TabControl3				*tabs;
	DXBCRegisterWindow		*regs;
	DXBCLocalsWindow		*locals;
	DX11StateControl		tree;
	ComboControl			thread_control;
	const DX11ShaderState	&shader;
	dynamic_array<int>		bp;
	dynamic_array<uint64>	bp_offsets;
	const void				*op;
	uint32					step_count;
	uint32					thread;

	void	ToggleBreakpoint(int32 offset) {
		auto	i = lower_boundc(bp_offsets, offset);
		if (i == bp_offsets.end() || *i != offset)
			bp_offsets.insert(i, offset);
		else
			bp_offsets.erase(i);
		DebugWindow::Invalidate(Margin());
	}
public:
	dx::SimulatorDXBC		sim;

	uint64		Address(const void *p)	const	{ return p ? base + sim.Offset(p) : 0; }
	Control&	control()						{ return *(MultiSplitterWindow*)this; }

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	Update() {
		uint32	pc	= sim.Offset(op);
		SetPC(OffsetToLine(pc));
		regs->Update(pc, pc + base, thread);
		if (locals)
			locals->Update(pc, pc + base, thread);
	}
	DX11ShaderDebuggerWindow(const WindowPos &wpos, const char *title, const DX11ShaderState &shader, DX11Connection *con, Disassembler::MODE mode);

	void	SetThread(int _thread)	{
		thread = _thread;

		for (int i = 0; i < sim.NumThreads(); i++)
			thread_control.SetItemData(i, thread_control.Add(to_string(i)));
		thread_control.Select(thread);

		op = (const dx::Opcode*)sim.Run(0);
	
		Update();
	}
};

DX11ShaderDebuggerWindow::DX11ShaderDebuggerWindow(const WindowPos &wpos, const char *title, const DX11ShaderState &shader, DX11Connection *con, Disassembler::MODE mode)
	: MultiSplitterWindow(2, SWF_VERT | SWF_PROP | SWF_DELETE_ON_DESTROY)
	, DebugWindow(HLSLcolourerRE(), none, mode)
	, accel(GetAccelerator())
	, tree(con)
	, shader(shader)
	, step_count(0), thread(0)
{
	MultiSplitterWindow::Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, CLIENTEDGE);// | COMPOSITED);

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
	split2->Create(GetPanePos(0), 0, CHILD | CLIPSIBLINGS | VISIBLE);

	auto	spdb = make_shared<ParsedSPDB>(memory_reader(shader.DXBC()->GetBlob(dx::DXBC::ShaderPDB)), con->files, con->shader_path);

	DebugWindow::Create(split2->_GetPanePos(0), NULL, CHILD | READONLY | SELECTIONBAR);
	DebugWindow::locations	= move(spdb->locations);
	DebugWindow::files		= move(spdb->files);
	DebugWindow::FixLocations(shader.GetUCodeAddr());
	DebugWindow::SetDisassembly(shader.Disassemble(), true);

	TitledWindow	*tree_title	= new TitledWindow(split2->_GetPanePos(1), "Bound");
	tree.Create(tree_title->GetChildWindowPos());
	tree.AddShader(TVI_ROOT, shader);

	split2->SetPanes(DebugWindow::hWnd, *tree_title, 100);
	SetPane(0, *split2);
	tree.Show();
	
	// source tabs
	tabs = 0;
	if (HasFiles()) {
		tabs = new TabControl3(GetPanePos(1), "source", CHILD | CLIPSIBLINGS | VISIBLE);
		tabs->SetFont(win::Font::DefaultGui());
		SourceTabs(*tabs);
		InsertPane(*tabs, 1);
	}
	
	int	regs_pane	= NumPanes() - 1;

	if (spdb->HasModule(1)) {
		SplitterWindow	*split3 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
		split3->Create(GetPanePos(regs_pane), 0, CHILD | CLIPSIBLINGS | VISIBLE);
		SetPane(regs_pane, *split3);

		TitledWindow	*regs_title		= new TitledWindow(split3->GetPanePos(0), "Registers");
		TitledWindow	*locals_title	= new TitledWindow(split3->GetPanePos(1), "Locals");
		split3->SetPanes(*regs_title, *locals_title);

		regs	= new DXBCRegisterWindow(regs_title->GetChildWindowPos(), &sim, spdb);
		locals	= new DXBCLocalsWindow(locals_title->GetChildWindowPos(), &sim, spdb, ctypes);

	} else {
		regs	= new DXBCRegisterWindow(GetPanePos(regs_pane), &sim, spdb);
		locals	= nullptr;
		SetPane(regs_pane, *regs);
	}

#if 1
	thread_control.Create(control(), "thread", CHILD | OVERLAPPED | VISIBLE | VSCROLL | thread_control.DROPDOWNLIST | thread_control.HASSTRINGS, NOEX,
		Rect(wpos.rect.Width() - 64, 0, 64 - GetNonClientMetrics().iScrollWidth, GetNonClientMetrics().iSmCaptionHeight),
		'TC'
	);
	thread_control.SetFont(win::Font::SmallCaption());
	thread_control.MoveAfter(HWND_TOP);
#endif
	DebugWindow::Show();
	MultiSplitterWindow::Rebind(this);
}

LRESULT DX11ShaderDebuggerWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			SetAccelerator(control(), accel);
			return 0;

		case WM_SIZE: {
			Point	size(lParam);
			thread_control.Move(Point(size.x - 64, 0));
			break;
		}

		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_LBUTTONDOWN:
					SetAccelerator(control(), accel);
					regs->RemoveOverlay();
					break;
			}
			return 0;

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case 'TC':
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						thread = thread_control.Selected();
						Update();
						return 0;
					}
					break;

				case DebugWindow::ID_DEBUG_RUN: {
					while (op) {
						++step_count;
						op	= sim.Continue(op, 1);
						uint32	offset	= sim.Offset(op);
						auto	i		= lower_boundc(bp_offsets, offset);
						if (i != bp_offsets.end() && *i == offset)
							break;
					}
					Update();
					return 0;
				}

				case DebugWindow::ID_DEBUG_STEPOVER:
					if (op) {
						++step_count;
						op	= sim.Continue(op, 1);
						Update();
					}
					return 0;

				case DebugWindow::ID_DEBUG_STEPBACK:
					if (step_count) {
						op = sim.Run(--step_count);
						Update();
					}
					return 0;

				case DebugWindow::ID_DEBUG_BREAKPOINT: {
					int		line	= GetLine(GetSelection().cpMin);
					uint32	offset	= LineToOffset(line);
					ToggleBreakpoint(offset);
					return 0;
				}
				default:
					if (HIWORD(wParam) == EN_SETFOCUS) {
						SetAccelerator(control(), accel);
						EditControl	c((HWND)lParam);
						int	line = c.GetLine(c.GetSelection().cpMin);
						if (int id = LOWORD(wParam)) {
							ShowCode(id, line);
						} else if (tabs) {
							ShowSourceTabLine(*tabs, LineToSource(line));
						}
						return 0;
					}
					break;
			}
			break;
			
		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case d2d::PAINT_INFO::CODE: {
					auto	*info	= (d2d::PAINT_INFO*)nmh;
					uint32	pc		= sim.Offset(op);
					if (int id = nmh->idFrom) {
						uint32	pc_line	= ~0;
						if (auto loc = OffsetToSource(pc)) {
							if ((uint16)loc->file == id)
								pc_line = loc->line - 1;
						}
						dynamic_array<uint32>	bp1 = transformc(bp_offsets, [this, id](uint32 offset)->uint32 {
							if (auto loc = OffsetToSource(offset)) {
								if ((uint16)loc->file == id)
									return loc->line - 1;
							}
							return ~0;
						});
						app::DrawBreakpoints(nmh->hwndFrom, info->target, pc_line, bp1);

					} else {
						dynamic_array<uint32>	bp1 = transformc(bp_offsets, [this](uint32 offset)->uint32 { return OffsetToLine(offset); });
						app::DrawBreakpoints(nmh->hwndFrom, info->target, OffsetToLine(pc), bp1);
					}
					return 0;
				}

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					break;

				case NM_CLICK:
					if (wParam == 'RG') {
						regs->AddOverlay((NMITEMACTIVATE*)lParam);

					} else if (nmh->hwndFrom == tree) {
						if (HTREEITEM hItem = tree.hot) {
							TreeControl::Item	i = tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE);
							bool	new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);
							switch (i.Image()) {
								case DX11StateControl::ST_RESOURCE:
									Docker(control()).Dock(new_tab ? DOCK_RIGHT : DOCK_TABID,  MakeItemView(MultiSplitterWindow::GetChildWindowPos(), tree.con, i.Param()));
									break;

								case DX11StateControl::ST_BOUND_RESOURCE:
									Docker(control()).Dock(new_tab ? DOCK_RIGHT : DOCK_TABID,  MakeBoundView(MultiSplitterWindow::GetChildWindowPos(), shader, i.Param()));
									break;

							}
						}
					}
					break;
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return MultiSplitterWindow::Proc(message, wParam, lParam);
}

//-----------------------------------------------------------------------------
//	DX11BatchWindow
//-----------------------------------------------------------------------------

class DX11BatchWindow : public StackWindow {
	struct Target : dx::Resource {
		ISO_ptr_machine<void>	p;
		void init(const char *name, dx::Resource &&r) {
			dx::Resource::operator=(move(r));
			ConcurrentJobs::Get().add([this, name] {
				p = dx::GetBitmap(name, *this);
			});
		}
	};

	struct VertStream {
		TypedBuffer				buffer;
		const dx::SIG::Element	*element = 0;
		uint32					offset;
		uint32					input_slot;
	};

public:
	enum {ID = 'BA'};

	DX11Connection				*con;
	DX11State					state;
	DX11StateControl			tree;
	DX11ShaderState				shaders[5];
	dynamic_array<VertStream>	verts;
	TypedBuffer					index_buffer;
	dynamic_array<Target>		targets;
	
	indices						ix;
	Topology2					topology;
	BackFaceCull				culling;
	float3x2					viewport;
	Point						screen_size;

	const DX11ShaderState&	GetShader(int stage) const {
		return shaders[stage == dx::CS ? 0 : stage];
	}

	static DX11BatchWindow*	Cast(Control c)	{
		return (DX11BatchWindow*)StackWindow::Cast(c);
	}
	WindowPos AddView(bool new_tab) {
		return Dock(new_tab ? DOCK_ADDTAB : DOCK_PUSH);
	}
	void SelectBatch(BatchList &b, bool always_list = false) {
		int		batch	= ::SelectBatch(*this, GetMousePos(), b, always_list);
		if (batch >= 0)
			Parent()(WM_ISO_BATCH, batch);
	}
	Control ShowBitmap(ISO_ptr_machine<void> p, bool new_tab) {
		Control	c;
		if (p) {
			c = BitmapWindow(AddView(new_tab), p, tag(p.ID()), true);
		}
		return c;
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	DX11BatchWindow(const WindowPos &wpos, string_param title, DX11Connection *con, uint32 addr, DX11Replay *replay);
	int		InitSimulator(dx::SimulatorDXBC &sim, const DX11ShaderState &shader, int thread, dynamic_array<uint32> &indices, Topology2 &top);
	Control	MakeShaderOutput(const WindowPos &wpos, dx::SHADERSTAGE stage, bool mesh);
	void	VertexMenu(dx::SHADERSTAGE stage, int i, ListViewControl lv);
	Control	DebugPixel(uint32 target, const Point &pt);
};

string get_name(const dx::DXBC::BlobT<dx::DXBC::InputSignature> *isgn, const dx::SIG::Element &e) {
	if (e.semantic_index)
		return string(e.name.get(isgn)) << e.semantic_index;
	return e.name.get(isgn);
}

template<typename T> auto AddItem(RegisterTree &rt, HTREEITEM h, const char *title, DX11Assets::ItemRecord *item) {
	return rt.AddFields(rt.AddText(h, format_string(title) << ": " << item->name), (const T*)item->info);
}

DX11BatchWindow::DX11BatchWindow(const WindowPos &wpos, string_param title, DX11Connection *con, uint32 addr, DX11Replay *replay) : StackWindow(wpos, title, ID), con(con), tree(con) {
	Rebind(this);
	clear(screen_size);

	con->GetStateAt(state, addr);

	DX11Assets::ItemRecord	*indirect_buffer	= is_indirect(state.draw_id) ? con->FindItem(state.IndirectBuffer) : nullptr;
	const void				*indirect_commands	= indirect_buffer ? (const char*)(replay ? replay->GetResource(indirect_buffer) : state.GetModdedData(indirect_buffer)) + state.IndirectOffset : nullptr;

	if (is_compute(state.draw_id)) {
		//compute
		const DX11State::Stage	&stage	= state.GetStage(dx::CS11);
		shaders[0].init(con, stage, state.cs_uav, state.mods);
		
		if (indirect_commands)
			state.ThreadGroupCount = *(const uint3p*)indirect_commands;

		SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
		split2->Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE);
		split2->SetPanes(
			tree.Create(split2->_GetPanePos(0)),
			*new ComputeGrid(split2->_GetPanePos(1), 0, 'CG', state.ThreadGroupCount, shaders[0].GetThreadGroup()),
			400
		);

		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		HTREEITEM	h1 = tree.AddShader(TreeControl::Item(buffer_accum<256>("Compute Shader: ") << shaders[0].name).Image(tree.ST_SHADER).StateImage(0).Insert(tree), shaders[0]);
		rt.Add(h1, "Outputs", tree.ST_OUTPUTS, dx::CS);

		if (indirect_buffer) {
			HTREEITEM	h	= TreeControl::Item("Indirect").Bold().Expand().Insert(tree);
			rt.AddFields(h, indirect_buffer);
			if (indirect_commands)
				rt.AddFields(h,	(const uint32(*)[3])&state.ThreadGroupCount);
		}

	} else if (is_draw(state.draw_id)) {
		//draw
		topology	= state.GetTopology();
		culling		= state.CalcCull(true);

		if (indirect_commands) {
			if (is_indexed(state.draw_id)) {
				auto	args = (D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS*)indirect_commands;
				state.VertexCount	= args->IndexCountPerInstance;
				state.BaseVertex	= args->BaseVertexLocation;
				state.StartIndex	= args->StartIndexLocation;
				state.StartInstance	= args->StartInstanceLocation;
				state.InstanceCount	= args->InstanceCount;
			} else {
				auto	args = (D3D11_DRAW_INSTANCED_INDIRECT_ARGS*)indirect_commands;
				state.VertexCount	= args->VertexCountPerInstance;
				state.BaseVertex	= args->StartVertexLocation;
				state.StartInstance	= args->StartInstanceLocation;
				state.InstanceCount	= args->InstanceCount;
			}
		}

		if (state.IsIndexed()) {
			uint32	stride	= state.ib.format == DXGI_FORMAT_R16_UINT ? 2 : 4;
			index_buffer	= state.GetModded(con, state.ib.buffer, dx::to_c_type(state.ib.format), stride, state.ib.offset + state.StartIndex * stride, state.VertexCount);
			ix = indices(index_buffer, stride, state.BaseVertex, state.VertexCount);
		} else {
			ix = indices(0, 0, state.BaseVertex, state.VertexCount);
		}

		auto	vp	= state.GetViewport(0);
		float3	vps{vp.Width / 2, -vp.Height / 2, vp.MaxDepth - vp.MinDepth};
		float3	vpo{vp.TopLeftX + vp.Width / 2, vp.TopLeftY + vp.Height / 2, vp.MinDepth};
		viewport	= float3x2(vps, vpo);

		if (auto *rtv = con->FindItem(state.rtv[0])) {
			if (const RecView<D3D11_RENDER_TARGET_VIEW_DESC> *v	= rtv->info) {
				int		mip = v->pdesc ? v->pdesc->Texture2D.MipSlice : 0;
				if (auto *r = con->FindItem(v->resource)) {
					const D3D11_TEXTURE2D_DESC	*desc = r->info;
					screen_size.x	= desc->Width >> mip;
					screen_size.y	= desc->Height >> mip;
				}
			}
		}

		if (replay) {
			targets.resize(state.num_rtv + 1);
			for (uint32 i = 0; i < state.num_rtv; i++)
				targets[i].init("render", replay->GetResource(con->FindItem(state.rtv[i])));
			targets[state.num_rtv].init("depth", replay->GetResource(con->FindItem(state.dsv)));
		}

		for (int i = 0; i < 5; i++)
			shaders[i].init(con, state.GetStage(i), i == dx::PS ? state.ps_uav : nullptr, state.mods);

		if (shaders[dx::VS].non_system_inputs) {
			if (DX11Assets::ItemRecord *ia = con->FindItem(state.ia)) {
				COMParse2(ia->info, [this, con](counted<const D3D11_INPUT_ELEMENT_DESC,1> desc, UINT num, counted<const uint8,3> code, SIZE_T length) {
					verts.resize(num);

					auto	num_verts = ix.max_index() + 1;
					auto	rec		= con->FindShader(state.GetStage(dx::VS).shader);
					auto	*in		= rec->DXBC()->GetBlob<dx::DXBC::InputSignature>();

					uint32	offset = 0;
					for (int i = 0; i < num; i++) {
						auto	&d	= desc[i];
						int		j	= d.InputSlot;

						if (d.AlignedByteOffset != D3D11_APPEND_ALIGNED_ELEMENT)
							offset = d.AlignedByteOffset;

						verts[i].input_slot		= d.InputSlot;
						verts[i].buffer			= state.GetModded(con, state.vb[j].buffer, dx::to_c_type(d.Format), state.vb[j].stride, state.vb[j].offset + offset, num_verts);
						verts[i].buffer.divider	= d.InstanceDataStepRate;
						verts[i].offset			= offset;
						verts[i].element		= in ? in->find_by_semantic(d.SemanticName, d.SemanticIndex) : 0;

						offset				+= verts[i].buffer.stride;
					}
				});
			}
		}

		if (verts && verts[0].buffer) {
			SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
			split2->Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE);
			split2->SetClientPos(400);

			split2->SetPanes(
				tree.Create(split2->_GetPanePos(0)),
				*MakeMeshView(split2->_GetPanePos(1), topology, verts[0].buffer, ix, one, culling, MeshWindow::PERSPECTIVE)
			);
		} else {
			tree.Create(GetChildWindowPos());
		}

		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		HTREEITEM	h	= TreeControl::Item("Targets").Bold().Expand().Insert(tree);

		DX11Assets::ItemRecord *blend = con->FindItem(state.blend);
		for (uint32 i = 0; i < state.num_rtv; i++) {
			buffer_accum<256>	ba;
			ba << "Target " << i << ' ';
			HTREEITEM	h2 = rt.Add(h, ba, replay ? tree.ST_TARGET : 0, i);
			if (DX11Assets::ItemRecord *item = con->FindItem(state.rtv[i])) {
				rt.AddFields(rt.AddText(h2, format_string("RTV: ") << item->name), (const RecView<D3D11_RENDER_TARGET_VIEW_DESC>*)item->info);
				ID3D11DeviceChild *res	= *item->info;
				if (DX11Assets::ItemRecord *item2 = con->FindItem(res)) {
					rt.AddFields(rt.AddText(h2, "Resource"), item2);
				}
			}
			if (blend)
				rt.AddFields(rt.AddText(h2, "Blending"), &((const D3D11_BLEND_DESC1*)blend->info)->RenderTarget[i]);
		}

		HTREEITEM	h2	= rt.Add(h, "Depth Buffer", replay ? tree.ST_TARGET : 0, state.num_rtv);
		if (DX11Assets::ItemRecord *item = con->FindItem(state.dsv)) {
			rt.AddFields(rt.AddText(h2, "DSV"), item);
			ID3D11DeviceChild *res	= *item->info;
			if (DX11Assets::ItemRecord *item2 = con->FindItem(res)) {
				rt.AddFields(rt.AddText(h2, "Resource"), item2);
			}
		}
		if (DX11Assets::ItemRecord *item = con->FindItem(state.depth_stencil)) {
			rt.AddFields(rt.AddText(h2, format_string("DepthStencilState: ") << item->name), (const D3D11_DEPTH_STENCIL_DESC*)item->info);
			rt.AddField(h2, field::make<UINT>("stencil_ref", 0), (uint32le*)&state.stencil_ref, 0, 0);
		}

		const D3D11_RASTERIZER_DESC1	*raster_desc = 0;

		if (DX11Assets::ItemRecord *item = con->FindItem(state.rs)) {
			raster_desc = item->info;
			h2 = rt.AddText(h, format_string("RasterizerState: ") << item->name);
			rt.AddFields(h2, raster_desc);
			rt.AddField(h2, field::make<float[4]>("blend_factor", 0), (uint32le*)&state.blend_factor, 0, 0);
			rt.AddField(h2, field::make("sample_mask", 0, 32, 0, 0, sHex), (uint32le*)&state.sample_mask, 0, 0);
			rt.AddFields(rt.AddText(h2, "viewport"), &state.viewport[0]);
			if (raster_desc->ScissorEnable)
				rt.AddFields(rt.AddText(h2, "scissor"), &state.scissor[0]);
		}
	
		h	= TreeControl::Item("Shaders").Bold().Expand().Insert(tree);
		static const char *shader_names[] = {
			"Vertex Shader",
			"Pixel Shader",
			"Domain Shader",
			"Hull Shader",
			"Geometry Shader",
		};
		for (int i = 0; i < 5; i++) {
			if (auto &s = shaders[i]) {
				HTREEITEM	h1 = tree.AddShader(TreeControl::Item(buffer_accum<256>(shader_names[i]) << ": " << s.name).Image(tree.ST_SHADER).StateImage(i).Insert(tree, h), s);

				switch (i) {
					case dx::VS: {
						HTREEITEM	h2	= rt.Add(h1, "Inputs", tree.ST_VERTICES, 0);
						if (state.IsIndexed())
							rt.AddFields(rt.AddText(h2, "IndexBuffer"), &state.ib);

						if (s.non_system_inputs) {
							if (DX11Assets::ItemRecord *item = con->FindItem(state.ia))
								rt.AddFields(rt.AddText(h2, format_string("InputLayout: ") << item->name), (const INPUT_LAYOUT*)item->info);
						}

						int		vi	= 0;
					#if 0
						for (auto &i : s.GetSignatureIn()) {
							if (DX11Assets::ItemRecord *item = con->FindItem(vb.buffer)) {
								HTREEITEM	h = rt.AddFields(rt.AddText(h2, format_string("[%i]", vi)), &vb);
								for (auto &v : verts) {
									if (v.input_slot == vi && v.element)
										rt.AddText(h, format_string("%s: v%i @ %i", get_name(in, *v.element).begin(), v.element->register_num, v.offset));
								}
							}
							++vi;
						}
					#else
						auto	*in	= s.DXBC()->GetBlob<dx::DXBC::InputSignature>();
						for (auto &vb : state.vb) {
							bool	used = false;
							for (auto &v : verts) {
								if (used = v.input_slot == vi && v.element)
									break;
							}
							if (used) {
								if (DX11Assets::ItemRecord *item = con->FindItem(vb.buffer)) {
									HTREEITEM	h = rt.AddFields(rt.AddText(h2, format_string("[%i]", vi)), &vb);
									for (auto &v : verts) {
										if (v.input_slot == vi && v.element)
											rt.AddText(h, format_string("%s: v%i @ %i", get_name(in, *v.element).begin(), v.element->register_num, v.offset));
									}
								}
							}
							++vi;
						}
					#endif
						break;
					}
					case dx::GS: {
						HTREEITEM	h2	= rt.AddText(h1, "Viewports");
						int n = state.num_viewport;
						if (raster_desc->ScissorEnable)
							n = max(n, state.num_scissor);
						for (int i = 0; i < n; i++) {
							HTREEITEM	h3	= rt.AddText(h2, format_string("[%i]", i));
							if (i < state.num_viewport)
								rt.AddFields(h3, &state.viewport[i]);
							if (raster_desc->ScissorEnable && i < state.num_scissor)
								rt.AddFields(h3, &state.scissor[i]);
						}
						break;
					}
				}
				rt.Add(h1, "Outputs", tree.ST_OUTPUTS, i);
			}
		}

		if (indirect_buffer) {
			HTREEITEM	h	= TreeControl::Item("Indirect").Bold().Expand().Insert(tree);
			rt.AddFields(h, indirect_buffer);
			if (indirect_commands)
				rt.AddFields(h,
					is_indexed(state.draw_id) ? fields<D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS>::f : fields<D3D11_DRAW_INSTANCED_INDIRECT_ARGS>::f,
					indirect_commands
				);
		}

	}

	if (con->frequency) {
		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		HTREEITEM		h	= TreeControl::Item("Statistics").Bold().Expand().Insert(tree);
		rt.AddFields(h, &con->batches[con->GetBatchNumber(addr)].stats);
	}
}

LRESULT DX11BatchWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case DebugWindow::ID_DEBUG_PIXEL: {
					auto	*p = (pair<uint64,Point>*)lParam;
					DebugPixel(p->a, p->b);
					return 0;
				}
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CLICK: {
					if (nmh->hwndFrom == tree) {
						if (HTREEITEM hItem = tree.hot) {
							tree.hot	= 0;
							TreeControl::Item	i = tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE);
							bool	ctrl	= !!(GetKeyState(VK_CONTROL) & 0x8000);
							bool	new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);

							switch (i.Image()) {
								case DX11StateControl::ST_TARGET:{
									int	t = i.Param();
									if (auto p = targets[t].p) {
										Menu menu	= Menu::Popup();
										Menu::Item("Debug this Pixel", DebugWindow::ID_DEBUG_PIXEL).Param(t).AppendTo(menu);
										Control	c =	BitmapWindow(AddView(new_tab), p, tag(p.ID()), true);
										c(WM_ISO_CONTEXTMENU, (HMENU)menu);
									}
									return 0;
								}
								case DX11StateControl::ST_RESOURCE:
									MakeItemView(AddView(new_tab), con, i.Param());
									return 0;

								case DX11StateControl::ST_BOUND_RESOURCE: {
									int		index	= i.Param();
									MakeBoundView(AddView(new_tab), GetShader(index / 256), index);
									return 0;
								}

								case DX11StateControl::ST_SHADER:
									new DX11ShaderWindow(AddView(new_tab), con, shaders[i.StateImage()]);
									return 0;

								case DX11StateControl::ST_VERTICES: {
									if (verts) {
										int			nb		= verts.size32();
										temp_array<named<TypedBuffer>>	buffers(nb);
										auto		title	= str<256>(GetText()) << "\\Inputs";

										auto		*isgn	= shaders[dx::VS].DXBC()->GetBlob<dx::DXBC::InputSignature>();
										for (auto &v : verts) {
											int		i	= verts.index_of(v);
											buffers[i]	= {v.element ? get_name(isgn, *v.element) : "unused", move(v.buffer)};
										}

										if (topology.type == Topology::UNKNOWN) {
											MakeVertexWindow(AddView(new_tab), title, 'VI', buffers, ix, state.InstanceCount);
										} else {
											MeshVertexWindow	*m	= new MeshVertexWindow(AddView(new_tab), title);
											VertexWindow		*vw	= MakeVertexWindow(m->_GetPanePos(0), 0, 'VI', buffers, ix, state.InstanceCount);
											MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1), topology, verts[0].buffer, ix, one, culling, MeshWindow::PERSPECTIVE);
											m->SetPanes(*vw, *mw, 50);
										}
									}
									return 0;
								}

								case DX11StateControl::ST_OUTPUTS:
									Busy(), MakeShaderOutput(AddView(new_tab), i.Param(), ctrl);
									return 0;
							}
						}
					}
					break;
				}

				case NM_RCLICK:
					switch (wParam) {
						case 'VO': VertexMenu(dx::VS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'PO': VertexMenu(dx::PS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'DO': VertexMenu(dx::DS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'HO': VertexMenu(dx::HS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'GO': VertexMenu(dx::GS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'CO': VertexMenu(dx::CS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'HC': VertexMenu(dx::HS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'VI': VertexMenu(dx::VS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'CG': VertexMenu(dx::VS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
					}
					break;

				case TVN_SELCHANGING:
					return TRUE; // prevent selection

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					break;

			}
			return Parent()(message, wParam, lParam);
		}
/*
		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_RBUTTONDOWN: {
					Point	pt	= ToScreen(Point(lParam));
					auto	c	= DescendantAt(pt);
					uint32	i;
					if (c.id == 'CG' && ~(i = GetComputeIndex(c, c.ToClient(pt)))) {
						Menu	menu	= Menu::Popup();
						menu.Append("Debug", 1);
						menu.Append("Show Simulated Trace", 4);

						switch (menu.Track(*this, pt, TPM_NONOTIFY | TPM_RETURNCMD)) {
							case 1: {
								auto		*debugger	= new DX11ShaderDebuggerWindow(GetChildWindowPos(), "Debugger", shaders[0], con, GetSettings("General/shader source").GetInt(1));
								Topology2	top;
								dynamic_array<uint16>	ib;
								int			thread		= InitSimulator(debugger->sim, shaders[0], i, ib, top);
								debugger->SetThread(thread);
								PushView(debugger->control());
								break;
							}

							case 4: {
								dx::SimulatorDXBC	sim;
								Topology2			top;
								dynamic_array<uint16>	ib;
								int					thread = InitSimulator(sim, shaders[0], i, ib, top);
								PushView(MakeDXBCTraceWindow(GetChildWindowPos(), sim, thread, 1000));
								break;
							}

						}
						menu.Destroy();
					}
					break;
				}
				return 0;
			}
			break;
*/

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return StackWindow::Proc(message, wParam, lParam);
}

int DX11BatchWindow::InitSimulator(dx::SimulatorDXBC &sim, const DX11ShaderState &shader, int thread, dynamic_array<uint32> &indices, Topology2 &top) {
	using namespace dx;
	int	start = thread < 0 ? 0 : thread;

	DXBC::UcodeHeader header;
	sim.Init(&header, shader.DXBC()->GetUCode(header));

	switch (shader.stage) {
		case dx::PS: {
			SimulatorDXBC	sim2;
			dx::Signature	out;

			if (auto &gs = shaders[dx::GS]) {
				InitSimulator(sim2, gs, 0, indices, top);
				out		= gs.GetSignatureOut();
			} else if (auto &ds = shaders[dx::DS]) {
				InitSimulator(sim2, ds, 0, indices, top);
				out	= ds.GetSignatureOut();
			} else {
				auto	&vs = shaders[dx::VS];
				InitSimulator(sim2, vs, 0, indices, top);
				out	= vs.GetSignatureOut();
			}
			
			sim2.Run();
			
			shader.InitSimulator(sim, start, top);

			auto	*ps_in	= shader.DXBC()->GetBlob<dx::DXBC::InputSignature>();
			int		n		= triangle_row(64 / 4);
			float	duv		= 1 / float(n * 2 - 1);

			if (thread >= 0) {
				sim.SetNumThreads(4);
				float	u	= triangle_col(thread / 4) * 2 * duv;
				float	v	= triangle_row(thread / 4) * 2 * duv;
				for (auto &in : ps_in->Elements())
					GetTriangle(&sim2, in.name.get(ps_in), in.semantic_index, out, 0, 1, 2)
					.Interpolate4(sim.GetOutput<float4p>(in.register_num).begin(), u, v, u + duv, v + duv);
				return thread & 3;

			} else {
				sim.SetNumThreads(triangle_number(triangle_row(64 / 4)) * 4);
				for (auto &in : ps_in->Elements()) {
					auto	tri		= GetTriangle(&sim2, in.name.get(ps_in), in.semantic_index, out, 0, 1, 2);
					auto	dest	= sim.GetRegFile<float4p>(Operand::TYPE_INPUT, in.register_num).begin();
					for (int u = 0; u < n; u++) {
						float	uf0 = float(u * 2) * duv;
						for (int v = 0; v <= u; v++) {
							float	vf0 = float(v * 2) * duv;
							tri.Interpolate4(dest, uf0, vf0, uf0 + duv, vf0 + duv);
							dest	+= 4;
						}
					}
				}
				return 0;
			}
		}

		case dx::HS: {
			auto	&vs = shaders[dx::VS];
			dx::SimulatorDXBC	sim2;
			InitSimulator(sim2, vs, 0, indices, top);
			sim2.Run();

			auto	 *vs_out	= vs.DXBC()->GetBlob<DXBC::OutputSignature>();
			auto	 *hs_in		= shader.DXBC()->GetBlob<DXBC::InputSignature>();
			for (auto &in : hs_in->Elements()) {
				if (!sim.HasInput(in.register_num)) {
					if (auto *x = vs_out->find_by_semantic(in.name.get(hs_in), in.semantic_index)) {
						sim.SetPatchInput(in.register_num);
					}
				}
			}

			sim.SetNumThreads(max(sim.forks, sim.NumOutputControlPoints()));

			for (auto &in : hs_in->Elements()) {
				if (auto *x = vs_out->find_by_semantic(in.name.get(hs_in), in.semantic_index)) {
					auto	s = sim2.GetOutput<float4p>(x->register_num);
					if (sim.HasInput(in.register_num)) {
						copy(s, sim.GetRegFile<float4p>(dx::Operand::TYPE_INPUT, in.register_num));
					} else {
						// set outputs in case there's no cp phase
						copy(s, sim.GetRegFile<float4p>(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, in.register_num));
					}
				}
			}
			top		= GetTopology(sim.tess_output);
			shader.InitSimulator(sim, start, top);
			return thread;
		}

		case dx::DS: {
			auto	&hs = shaders[dx::HS];
			dx::SimulatorDXBC	sim2;
			InitSimulator(sim2, hs, 0, indices, top);
			sim2.Run();

			Tesselation	tess = GetTesselation(sim.tess_domain, sim2.GetRegFile<float4p>(dx::Operand::TYPE_INPUT_PATCH_CONSTANT));

			if (thread < 0) {
				int	num	= tess.uvs.size32();
				sim.SetNumThreads(thread == -1 ? min(num, 64) : num);

			} else if (!sim.NumThreads()) {
				sim.SetNumThreads(1);
			}

			start	= tess.indices[start];
			auto	*uvs	= tess.uvs + start;
			for (auto &i : sim.GetRegFile<float4p>(dx::Operand::TYPE_INPUT_DOMAIN_POINT)) {
				i = float4{uvs->x, uvs->y, 1 - uvs->x - uvs->y, 0};
				++uvs;
			}

			auto	 *hs_out	= hs.DXBC()->GetBlob<DXBC::OutputSignature>();
			auto	 *ds_in		= shader.DXBC()->GetBlob<DXBC::InputSignature>();
			for (auto &in : ds_in->Elements()) {
				if (sim.HasPatchInput(in.register_num)) {
					if (auto *x = hs_out->find_by_semantic(in.name.get(ds_in), in.semantic_index))
						rcopy(sim.GetRegFile<float4p>(dx::Operand::TYPE_INPUT_CONTROL_POINT, in.register_num), sim2.GetRegFile<float4p>(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, x->register_num).begin());
				}
			}
			shader.InitSimulator(sim, start, top);
			if (thread != -1)
				indices = move(tess.indices);
			return 0;
		}

		case dx::VS: {
			top		= topology;
			bool	deindex = (bool)ix;
			uint32	nv		= ix.size32();
			if (thread < 0) {
				if (deindex) {
					int	maxi = ix.max_index() + 1;
					if (maxi < nv && state.InstanceCount == 1) {
						nv		= maxi;
						deindex	= false;
						indices	= ix;
					}
				}
				uint32	num = nv * state.InstanceCount;
				if (thread == -1) {
					sim.SetNumThreads(min(num, 64));
				} else {
					sim.SetNumThreads(num);
				}
			} else if (!sim.NumThreads()) {
				sim.SetNumThreads(top.VertsPerPrim());
			}

			shader.InitSimulator(sim, start, top);

			for (auto &i : verts) {
				if (auto *e = i.element) {
					if (i.buffer.divider) {
						sim.SetInput(e->register_num, e->component_type, make_indexed_iterator(i.buffer.clamped_begin(), transform(make_int_iterator(start), [d = i.buffer.divider * nv](int i) { return i / d; })));
					} else {
						auto	mod = transform(make_int_iterator(start), [nv](int i) { return i % nv; });
						if (deindex)
							sim.SetInput(e->register_num, e->component_type, make_indexed_iterator(i.buffer.clamped_begin(), make_indexed_iterator(ix.begin(), mod)));
						else
							sim.SetInput(e->register_num, e->component_type, make_indexed_iterator(i.buffer.clamped_begin(), mod));
					}
				}
			}

			return 0;
		}

		case dx::GS: {
			auto	in_topology	= GetTopology(sim.input_prim ? sim.input_prim : dx::PRIMITIVE_POINT);
			auto	&vs			= shaders[dx::VS];
			int		vs_start	= in_topology.VertexFromPrim(start / sim.max_output);

			dx::SimulatorDXBC	sim2;
			sim2.SetNumThreads(in_topology.VertexFromPrim((start + sim.NumThreads()) / sim.max_output + 1) - vs_start);
			InitSimulator(sim2, vs, vs_start, indices, top);
			sim2.Run();
			
			if (thread < 0) {
				int	num	= in_topology.PrimFromVertex(ix.max_index()) + 1;
				sim.SetNumThreads(thread == -1 ? min(num, 64) : num);
			} else if (!sim.NumThreads()) {
				sim.SetNumThreads(1);
			}

			shader.InitSimulator(sim, 0, top);

			auto	 *vs_out	= vs.DXBC()->GetBlob<DXBC::OutputSignature>();
			auto	 *gs_in		= shader.DXBC()->GetBlob<DXBC::InputSignature>();
			for (auto &in : gs_in->Elements()) {
				if (auto *x = vs_out->find_by_semantic(in.name.get(gs_in), in.semantic_index))
					sim.SetInput<float4p>(in.register_num, sim2.GetOutput<float4p>(x->register_num).begin());
			}
			top	= GetTopology(sim.output_topology);
			return 0;
		}

		case dx::CS: {
			uint32x3	group		= {sim.thread_group[0], sim.thread_group[1], sim.thread_group[2]};
			uint32x3	dim2		= state.ThreadGroupCount;
			uint32		group_size	= group.x * group.y * group.z;
			uint32x3	total		= group * dim2;
			uint32		start		= 0;

			if (thread < 0) {
				int	num	= total.x * total.y * total.z;
				if (thread == -1) {
					sim.SetNumThreads(min(num, 64));
				} else {
					sim.SetNumThreads(num);
				}
				thread = 0;
			} else if (!sim.NumThreads()) {
				sim.SetNumThreads(group_size);
				start	= thread / group_size * group_size;
				thread %= group_size;
			}

			shader.InitSimulator(sim, start, top);
			if (sim.HasInput(dx::SimulatorDXBC::vThreadID))
				rcopy(
					sim.GetRegFile<uint4p>(dx::Operand::TYPE_INPUT, dx::SimulatorDXBC::vThreadID),
					transform(int_iterator<int>(start), [&](int i) { return concat(split_index(i, total.xy), i); })
				);
			if (sim.HasInput(dx::SimulatorDXBC::vThreadGroupID))
				rcopy(
					sim.GetRegFile<uint4p>(dx::Operand::TYPE_INPUT, dx::SimulatorDXBC::vThreadGroupID),
					scalar(split_index(start / group_size, dim2))
				);
			if (sim.HasInput(dx::SimulatorDXBC::vThreadIDInGroup))
				rcopy(
					sim.GetRegFile<uint4p>(dx::Operand::TYPE_INPUT, dx::SimulatorDXBC::vThreadIDInGroup),
					transform(int_iterator<int>(start), [&](int i) { return concat(split_index(i, group.xy), i % group_size); })
			);

			return thread;
		}
					 
		default:
			shader.InitSimulator(sim, start, top);
			return 0;
	}
}

Control DX11BatchWindow::MakeShaderOutput(const WindowPos &wpos, dx::SHADERSTAGE stage, bool mesh) {
	using namespace dx;

	auto				&shader = GetShader(stage);
	dx::SimulatorDXBC	sim;

//	if (stage == dx::CS && mesh) {
//		sim.Init(shader.GetUCode(), shader.GetUCodeAddr());
//		return app::MakeComputeGrid(GetChildWindowPos(), dim, sim.thread_group);
//	}

	dynamic_array<uint32>	ib;
	Topology2				top;
	
	InitSimulator(sim, shader, mesh ? -2 : -1, ib, top);
	
	Control		c	= app::MakeShaderOutput(wpos, &sim, shader, ib);

	if (!mesh)
		return c;

	switch (stage) {
		default:
			return c;

		case dx::VS: {
			bool				final	= !shaders[dx::DS] && !shaders[dx::GS];
			int					out_reg	= 0;
			if (auto *sig = find_if_check(shader.DXBC()->GetBlob<DXBC::OutputSignature>()->Elements(), [](const dx::SIG::Element &i) { return i.system_value == dx::SV_POSITION; }))
				out_reg = sig->register_num;

			MeshVertexWindow	*m		= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw		= MakeMeshView(m->GetPanePos(1),
				top,
				temp_array<float4p>(sim.GetOutput<float4p>(out_reg)),
				ib.empty() ? indices(sim.NumThreads()) : indices(ib),
				viewport, culling,
				final ? MeshWindow::SCREEN_PERSP : MeshWindow::PERSPECTIVE
			);

			if (final) {
				if (screen_size.x && screen_size.y)
					mw->SetScreenSize(screen_size);
				mw->flags.set(MeshWindow::FRUSTUM_EDGES);
			}

			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case dx::GS: {
			uint32				num_verts		= sim.NumThreads() * sim.MaxOutput();
			MeshVertexWindow	*m	= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1),
				top,
				temp_array<float4p>(sim.GetOutput<float4p>(0)),
				num_verts,
				viewport, culling,
				MeshWindow::SCREEN_PERSP
			);	

			if (screen_size.x && screen_size.y)
				mw->SetScreenSize(screen_size);
			mw->flags.set(MeshWindow::FRUSTUM_EDGES);

			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case dx::HS: {
			Tesselation			tess	= GetTesselation(sim.tess_domain, sim.GetRegFile<float4p>(dx::Operand::TYPE_INPUT_PATCH_CONSTANT));
			SplitterWindow		*m		= new SplitterWindow(GetChildWindowPos(), "Shader Output", SplitterWindow::SWF_VERT | SplitterWindow::SWF_PROP);
			MeshWindow			*mw		= MakeMeshView(m->GetPanePos(1),
				top,
				tess.uvs,
				tess.indices,
				one, culling,
				MeshWindow::PERSPECTIVE
			);	
			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case dx::DS: {
			bool				final	= !shaders[dx::GS];
			MeshVertexWindow	*m	= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1),
				top,
				temp_array<float4p>(sim.GetOutput<float4p>(0)),
				ib,
				viewport, culling,
				final ? MeshWindow::SCREEN_PERSP : MeshWindow::PERSPECTIVE
			);

			if (final) {
				if (screen_size.x && screen_size.y)
					mw->SetScreenSize(screen_size);
				mw->flags.set(MeshWindow::FRUSTUM_EDGES);
			}

			m->SetPanes(c, *mw, 50);
			return *m;
		}

	}
}

void DX11BatchWindow::VertexMenu(dx::SHADERSTAGE stage, int i, ListViewControl lv) {
	auto	&shader = shaders[stage == dx::CS ? 0 : stage];
	Menu	menu	= Menu::Popup();

	menu.Append("Debug", 1);
	menu.Append("Show Simulated Trace", 4);

	if (stage == dx::VS && ix) {
		menu.Separator();
		menu.Append(format_string("Next use of index %i", ix[i]), 2);
		menu.Append(format_string("Highlight all uses of index %i", ix[i]), 3);
	}

	menu.Separator();
	menu.Append("Save...", 5);

	switch (menu.Track(*this, GetMousePos(), TPM_NONOTIFY | TPM_RETURNCMD)) {
		//Debug
		case 1: {
			dynamic_array<uint32>	ib;
			Topology2				top;
			auto	*debugger	= new DX11ShaderDebuggerWindow(GetChildWindowPos(), "Debugger", shader, con, GetSettings("General/shader source").Get(Disassembler::SHOW_SOURCE));
			int		thread		= InitSimulator(debugger->sim, shader, i, ib, top);
			debugger->SetThread(thread);
			PushView(debugger->control());
			break;
		}

		//Next use of index
		case 2: {
			int	x = ix[i];
			for (int j = 1, n = ix.num; j < n; ++j) {
				int	k = (i + j) % n;
				if (ix[k] == x) {
					lv.SetItemState(k, LVIS_SELECTED);
					lv.EnsureVisible(k);
					lv.SetSelectionMark(k);
					break;
				}
			}
			break;
		}

		//Highlight all uses of index
		case 3: {
			int	x = ix[i];
			for (int j = 0, n = ix.num; j < n; ++j) {
				if (ix[j] == x)
					lv.SetItemState(j, LVIS_SELECTED);
			}
			break;
		}

		//Simulated Trace
		case 4: {
			dynamic_array<uint32>	ib;
			Topology2				top;
			dx::SimulatorDXBC		sim;
			int						thread = InitSimulator(sim, shader, i, ib, top);
			PushView(*new DXBCTraceWindow(GetChildWindowPos(), &sim, thread, 1000));
			break;
		}
		//Save
		case 5:
			if (auto *vo = CastByProc<VertexWindow>(lv.Parent())) {
				filename	fn;
				if (GetSave(*this, fn, "Save Capture", "Text\0*.txt\0")) {
					stream_accum<FileOutput, 1024>	sa(fn);
					for (auto &b : vo->buffers) {
						if (b.stride) {
							for (auto &&i : b)
								sa << i <<'\n';
							sa << '\n';
						}
					}
				}
			}
			break;

	}
	menu.Destroy();
}

Control DX11BatchWindow::DebugPixel(uint32 target, const Point &pt) {
	using namespace dx;

	SimulatorDXBC			sim2;
	dx::Signature			out;
	dynamic_array<uint32>	ib;
	Topology2				top;

	if (auto &gs = shaders[dx::GS]) {
		InitSimulator(sim2, gs, -2, ib, top);
		out	= gs.GetSignatureOut();
	} else if (auto &ds = shaders[dx::DS]) {
		InitSimulator(sim2, ds, -2, ib, top);
		out	= ds.GetSignatureOut();
	} else {
		auto	&vs = shaders[dx::VS];
		InitSimulator(sim2, vs, -2, ib, top);
		out	= vs.GetSignatureOut();
	}
			
	sim2.Run();
			
	dynamic_array<float4p>	output_verts = sim2.GetOutput<float4p>(0);
	uint32	num_verts	= ib ? ib.size32() : output_verts.size32();
	uint32	num_prims	= top.verts_to_prims(num_verts);
	dynamic_array<cuboid>	exts(num_prims);
	cuboid	*ext		= exts;

	if (ib) {
		auto	prims = make_prim_iterator(top, make_indexed_iterator(output_verts.begin(), make_const(ib.begin())));
		for (auto &&i : make_range_n(prims, num_prims))
			*ext++ = get_extent<position3>(i);
	} else {
		auto	prims = make_prim_iterator(top, output_verts.begin());
		for (auto &&i : make_range_n(prims, num_prims))
			*ext++ = get_extent<position3>(i);
	}

	octree		oct;
	oct.init(exts, num_prims);

	uint32		thread	= (pt.x & 1) + (pt.y & 1) * 2;
	float2		qpt		= {pt.x & ~1, pt.y & ~1};
	float2		pt0		= (qpt - viewport.y.xy) / viewport.x.xy;
	ray3		ray		= ray3(position3(pt0, zero), (float3)z_axis);
	triangle3	tri3;
	int			v[3];

	if (ib) {
		auto	prims	= make_prim_iterator(top, make_indexed_iterator(output_verts.begin(), make_const(ib.begin())));
		int		face	= oct.shoot_ray(ray, 0.25f, [prims](int i, param(ray3) r, float &t) {
			return prim_check_ray(prims[i], r, t);
		});
		if (face < 0)
			return Control();

		tri3 = prim_triangle(prims[face]);
		copy(make_prim_iterator(top, ib.begin())[face], v);
	} else {
		auto	prims	= make_prim_iterator(top, output_verts.begin());
		int		face	= oct.shoot_ray(ray, 0.25f, [prims](int i, param(ray3) r, float &t) {
			return prim_check_ray(prims[i], r, t);
		});
		if (face < 0)
			return Control();

		tri3 = prim_triangle(prims[face]);
		copy(make_prim_iterator(top, int_iterator<int>(0))[face], v);
	}
	
	auto	&ps			= shaders[dx::PS];
	auto	*debugger	= new DX11ShaderDebuggerWindow(AddView(false), "Debugger", ps, con, GetSettings("General/shader source").Get(Disassembler::SHOW_SOURCE));

	float3x4	para	= tri3.inv_matrix();
	float3		uv0		= para * position3(pt0, one);
	float3		uv1		= uv0 + para.x / viewport.x;
	float3		uv2		= uv0 + para.y / viewport.x;

	DXBC::UcodeHeader header;
	debugger->sim.Init(&header, ps.DXBC()->GetUCode(header));
	debugger->sim.SetNumThreads(4);
	ps.InitSimulator(debugger->sim, 0, Topology2());

	auto	*ps_in		= ps.DXBC()->GetBlob<DXBC::InputSignature>();
	for (auto &in : ps_in->Elements())
		GetTriangle(&sim2, in.name.get(ps_in), in.semantic_index, out, v[0], v[1], v[2])
			.Interpolate4(debugger->sim.GetRegFile<float4p>(dx::Operand::TYPE_INPUT, in.register_num).begin(), uv0.xy, uv1.xy, uv2.xy);

	debugger->SetThread(thread);
	return debugger->control();
}

//-----------------------------------------------------------------------------
//	ViewDX11GPU
//-----------------------------------------------------------------------------

class ViewDX11GPU :  public SplitterWindow, public DX11Connection {
public:
	static IDFMT		init_format;
private:
	static win::Colour	colours[];
	static uint8		cursor_indices[][3];

	ColourTree			tree	= ColourTree(colours, DX11cursors, cursor_indices);
	win::Font			italics;
	RefControl<ToolBarControl>	toolbar_gpu;
	HTREEITEM			context_item;
	IDFMT				format;

	void		TreeSelection(HTREEITEM hItem);
	void		TreeDoubleClick(HTREEITEM hItem);

public:
	static ViewDX11GPU*	Cast(Control c)	{
		return (ViewDX11GPU*)SplitterWindow::Cast(c);
	}

	WindowPos Dock(DockEdge edge, uint32 size = 0) {
		return Docker(GetPane(1)).Dock(edge, size);
	}

	Control SelectTab(ID id) {
		int			i;
		if (TabWindow *t = Docker::FindTab(GetPane(1), id, i)) {
			Control	c = t->GetItemControl(i);
			t->SetSelectedControl(t->GetItemControl(i));
			return c;
		}
		return Control();
	}

	void SetOffset(uint32 offset) {
		HTREEITEM h = FindOffset(tree, TVI_ROOT, offset);
		if (h != TVI_ROOT) {
			tree.SetSelectedItem(h);
			tree.EnsureVisible(h);
		}
	}
	void SelectBatch(uint32 batch) {
		SetOffset(batches[batch].addr);
	}
	void SelectBatch(BatchList &b, bool always_list = false) {
		int batch = ::SelectBatch(*this, GetMousePos(), b, always_list);
		if (batch >= 0)
			SelectBatch(batch);
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	MakeTree();
	void	ExpandTree(HTREEITEM h);

	ViewDX11GPU(MainWindow &main, const WindowPos &pos) : SplitterWindow(SWF_VERT|SWF_DOCK) {
		format			= init_format;
		context_item	= 0;

		SplitterWindow::Create(pos, "DX11 Dump", CHILD | CLIPCHILDREN);
		toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX11GPU);
		Rebind(this);
		SetFocus();
	}

	ViewDX11GPU(MainWindow &main, const WindowPos &pos, DXConnection *con) : SplitterWindow(SWF_VERT|SWF_DOCK), DX11Connection(con) {
		format			= init_format;
		context_item	= 0;

		SplitterWindow::Create(pos, con->process.Filename().name(), CHILD | CLIPCHILDREN);
		toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX11GPU);
		Rebind(this);
		SetFocus();
	}
};

//-----------------------------------------------------------------------------
//	ListViews
//-----------------------------------------------------------------------------

void GetUsedAt(BatchListArray &usedat, const DX11Assets *assets, int *index) {
	for (uint32 b = 0, nb = assets->batches.size32() - 1; b < nb; b++) {
		for (auto u : assets->GetUsage(b)) {
			int	i = index ? index[u.index] : u.index;
			if (i >= 0)
				usedat[i].push_back(b);
		}
	}
}

void GetUsedAt(BatchListArray &usedat, BatchListArray &writtenat, const DX11Assets *assets, int *index) {
	for (uint32 b = 0, nb = assets->batches.size32() - 1; b < nb; b++) {
		for (auto u : assets->GetUsage(b)) {
			int	i = index ? index[u.index] : u.index;
			if (i >= 0) {
				usedat[i].push_back(b);
				if (u.written)
					writtenat[i].push_back(b);
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	DX11ItemsList
//-----------------------------------------------------------------------------

Control MakeItemView(const WindowPos &wpos, DX11Connection *con, DX11Assets::ItemRecord *rec) {
	switch (rec->type) {
		case RecItem::VertexShader:		
		case RecItem::HullShader:		
		case RecItem::DomainShader:		
		case RecItem::GeometryShader:	
		case RecItem::PixelShader:		
		case RecItem::ComputeShader:
			return *new DX11ShaderWindow(wpos, con, DX11ShaderState(rec));

		case RecItem::Buffer: {
			const D3D11_BUFFER_DESC	*desc = rec->info;
			return MakeBufferWindow(wpos, rec->GetName(), 'BF', TypedBuffer(memory_block(unconst(desc + 1), desc->ByteWidth), 0));//16, ctypes.get_type<float[4]>()));
		}

		case RecItem::Texture1D:
		case RecItem::Texture2D:
		case RecItem::Texture3D:
			if (auto p = con->GetBitmap(rec))
				return BitmapWindow(wpos, p, rec->GetName(), true);
	}

	TreeControl		tree(wpos, rec->GetName(), Control::CHILD | Control::CLIPSIBLINGS | Control::VISIBLE | Control::VSCROLL | TreeControl::HASLINES | TreeControl::HASBUTTONS | TreeControl::LINESATROOT | TreeControl::SHOWSELALWAYS, Control::CLIENTEDGE);
	RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);
	rt.AddFields(rt.AddText(TVI_ROOT, rec->GetName(), 0), rec);
	return tree;
}

struct DX11ItemsList : EditableListView<DX11ItemsList, Subclass<DX11ItemsList, EntryTable<DX11Assets::ItemRecord, DX11Assets::ItemRecord, order_array<DX11Assets::ItemRecord>>>> {
	typedef DX11Assets::ItemRecord	T;
	enum {ID = 'OL'};
	DX11Connection		*con;
	BatchListArray		writtenat;
	dynamic_array<int>	sorted_order;
	win::Font			italics;

	void SortOnColumn1(int col) {
		int	dir = SetColumn(GetHeader(), col);
		switch (col) {
			case 0:
				sort(sorted_order, [this](int a, int b) {
					return SortCompare(table[a].GetName(), table[b].GetName());
				});
				break;
			case 1:
				sort(sorted_order, [this](int a, int b) {
					return SortCompare(usedat[a], usedat[b]);
				});
				break;
			case 2:
				sort(sorted_order, [this](int a, int b) {
					return SortCompare(writtenat[a], writtenat[b]);
				});
				break;
			case 3:
				sort(sorted_order, [this](int a, int b) {
					return SortCompare(table[a].GetSize(), table[b].GetSize());
				});
				break;
			default:
				sort(sorted_order, [this, col](int a, int b) {
					uint32			offseta = 0, offsetb = 0;
					const uint32	*pa		= (const uint32*)&table[a], *pb = (const uint32*)&table[b];
					const field		*pfa	= FieldIndex(fields<T>::f, col - 4, pa, offseta, true);
					const field		*pfb	= FieldIndex(fields<T>::f, col - 4, pb, offsetb, true);
					return SortCompare(pfa ? pfa->get_value(pa, offseta) : 0, pfb ? pfb->get_value(pb, offsetb) : 0);
				});
				break;
		}

		Invalidate();
	}

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (row < sorted_order.size()) {
			row	= sorted_order[row];
			if (ViewDX11GPU *main = ViewDX11GPU::Cast(from)) {
				switch (col) {
					case 0:
						main->SetOffset(main->FindObjectCreation(table[row].obj));
						//EditName(row);
						break;
					case 1:
						main->SelectBatch(usedat[row]);
						break;
					case 2:
						main->SelectBatch(writtenat[row]);
						break;
					default:
						Busy(), MakeItemView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), con, &table[row]);
						break;
				}
			}
		}
	}

	void	GetDispInfo(string_accum &sa, int row, int col) {
		row = sorted_order[row];
		const T	&u = table[row];
		switch (col) {
			case 0:		sa << u.GetName(); break;
			case 1:		WriteBatchList(sa, usedat[row]).term(); break;
			case 2:		WriteBatchList(sa, writtenat[row]).term(); break;
			case 3:		sa << GetSize((T&)u); break;
			case 5:		sa << "0x" << hex(u.obj); break;
			default: {
				uint32	offset = 0;
				const uint32 *p = (const uint32*)&u;
				if (const field *pf = FieldIndex(fields<T>::f, col - 4, p, offset, true))
					RegisterList(*this, this, format | (col > 5) * IDFMT_FIELDNAME).FillSubItem(sa, pf, p, offset).term();
				break;
			}
		}
	}

	void	operator()(string_accum &sa, const field *pf, const uint32le *p, uint32 offset) const {
		uint64		v = pf->get_raw_value(p, offset);
		if (auto *rec = con->FindItem((ID3D11DeviceChild*)v)) {
			sa << rec->name;
		} else {
			sa << "(unfound)0x" << hex(v);
		}
	}

	int		CustomDraw(NMLVCUSTOMDRAW *cd) {
		switch (cd->nmcd.dwDrawStage) {
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;
			case CDDS_ITEMPREPAINT: {
				if (cd->nmcd.dwItemSpec < sorted_order.size()) {
					int	row = sorted_order[cd->nmcd.dwItemSpec];
					const T	&u = table[row];
					if (u.is_dead()) {
						DeviceContext(cd->nmcd.hdc).Select(italics);
						return CDRF_NEWFONT;
					}
				}
				break;
			}
		}
		return CDRF_DODEFAULT;
	}

	DX11ItemsList(const WindowPos &wpos, DX11Connection *con)
		: Base(con->items)
		, con(con), writtenat(usedat.size()), sorted_order(int_range(usedat.size32()))
	{
		Create(wpos, "Items", ID, CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS | OWNERDATA);
		Column("name").Width(200).Insert(*this, 0);
		Column("used at").Width(100).Insert(*this, 1);
		Column("modified at").Width(100).Insert(*this, 2);
		Column("size").Width(100).Insert(*this, 3);
		int	nc = MakeColumns(*this, fields<T>::f, IDFMT_CAMEL | IDFMT_FOLLOWPTR, 4);

		format = IDFMT_FOLLOWPTR;
		for (int i = 0; i < 8; i++)
			Column("").Width(100).Insert(*this, nc++);

		italics = GetFont().GetParams().Italic(true);

		SetView(DETAIL_VIEW);
		SetCount(table.size32());
		GetUsedAt(usedat, writtenat, con, 0);
	}
};

//-----------------------------------------------------------------------------
//	DX11ResourcesList
//-----------------------------------------------------------------------------

struct ResourceTable {
	struct Record {
		RecItem::TYPE		type;

		UINT				Width			= 0;
		UINT				Height			= 0;
		UINT				Depth			= 0;	//ArraySize
		UINT				MipLevels		= 0;
		DXGI_FORMAT			Format			= DXGI_FORMAT_UNKNOWN;
		DXGI_SAMPLE_DESC	SampleDesc		= {0, 0};	//2d
		D3D11_USAGE			Usage			= D3D11_USAGE_DEFAULT;
		UINT				BindFlags		= 0;
		UINT				CPUAccessFlags	= 0;
		UINT				MiscFlags		= 0;

		DX11Assets::ItemRecord	*item;

		Record(DX11Assets::ItemRecord &item) : type(item.type), item(&item) {
			if (!item.info)
				return;

			switch (undead(type)) {
				case RecItem::Buffer: {
					const D3D11_BUFFER_DESC	*d	= item.info;
					Width			= d->ByteWidth;
					Height			= 0;
					Depth			= d->StructureByteStride;
					MipLevels		= 0;
					Format			= DXGI_FORMAT_UNKNOWN;
					Usage			= d->Usage;
					BindFlags		= d->BindFlags;
					CPUAccessFlags	= d->CPUAccessFlags;
					MiscFlags		= d->MiscFlags;
					break;
				}
				case RecItem::Texture1D: {
					const D3D11_TEXTURE1D_DESC	*d	= item.info;
					Width			= d->Width;
					Height			= 1;
					Depth			= d->ArraySize;
					MipLevels		= d->MipLevels;
					Format			= d->Format;
					Usage			= d->Usage;
					BindFlags		= d->BindFlags;
					CPUAccessFlags	= d->CPUAccessFlags;
					MiscFlags		= d->MiscFlags;
					break;
				}
				case RecItem::Texture2D: {
					const D3D11_TEXTURE2D_DESC	*d	= item.info;
					Width			= d->Width;
					Height			= d->Height;
					Depth			= d->ArraySize;
					MipLevels		= d->MipLevels;
					Format			= d->Format;
					Usage			= d->Usage;
					BindFlags		= d->BindFlags;
					CPUAccessFlags	= d->CPUAccessFlags;
					MiscFlags		= d->MiscFlags;
					break;
				}
				case RecItem::Texture3D: {
					const D3D11_TEXTURE3D_DESC	*d	= item.info;
					Width			= d->Width;
					Height			= d->Height;
					Depth			= d->Depth;
					MipLevels		= d->MipLevels;
					Format			= d->Format;
					Usage			= d->Usage;
					BindFlags		= d->BindFlags;
					CPUAccessFlags	= d->CPUAccessFlags;
					MiscFlags		= d->MiscFlags;
					break;
				}
			}
		}
		string				GetName()		const	{ return item->GetName(); }
		uint64				GetSize()		const	{ return item->GetSize(); }

	};
	dynamic_array<Record>	records;
	dynamic_array<int>		index;

	template<typename A> ResourceTable(A &items) : index(items.size()) {
		int	*px	= index;
		for (auto &i : items) {
			int	x = -1;
			switch (undead(i.type)) {
				case RecItem::Buffer:
				case RecItem::Texture1D:
				case RecItem::Texture2D:
				case RecItem::Texture3D:
					x	= records.size32();
					records.push_back(i);
					break;
			}
			*px++ = x;
		}
	}
};

template<> field	fields<ResourceTable::Record>::f[] = {
#undef S
#define	S ResourceTable::Record
	_MAKE_FIELD(S,type)
	_MAKE_FIELD(S,Width)
	_MAKE_FIELD(S,Height)
	_MAKE_FIELD(S,Depth)
	_MAKE_FIELD(S,MipLevels)
	_MAKE_FIELD(S,Format)
	_MAKE_FIELD(S,SampleDesc)//2d
	_MAKE_FIELD(S,Usage)
	_MAKE_FIELD(S,BindFlags)
	_MAKE_FIELD(S,CPUAccessFlags)
	_MAKE_FIELD(S,MiscFlags)
	TERMINATOR
};

struct DX11ResourcesList : ResourceTable, EditableListView<DX11ResourcesList, Subclass<DX11ResourcesList, EntryTable<ResourceTable::Record>>> {
	enum {ID = 'RL'};
	DX11Connection *con;
	ImageList		images;

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX11GPU *main = ViewDX11GPU::Cast(from)) {
			switch (col) {
	/*			case 0:
					EditName(row);
					break;*/
				case 1:
					ViewDX11GPU::Cast(from)->SelectBatch(GetBatches(row));
					break;
				default: {
					Busy		bee;
					if (auto e = GetEntry(row)) {
						auto		*r	= e->item;
						if (r->type == RecItem::Buffer) {
							const D3D11_BUFFER_DESC *desc	= r->info;
							BinaryWindow(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'id'), ISO::MakeBrowser(r->info + sizeof(D3D11_BUFFER_DESC)));

						} else if (auto bm = con->GetBitmap(r)) {
							BitmapWindow(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'id'), bm, r->GetName(), true);
						}
					}
					break;
				}
			}
		}
	}

	DX11ResourcesList(const WindowPos &wpos, DX11Connection *_con) : ResourceTable(_con->items), Base(records), con(_con)
		, images(ImageList::Create(DeviceContext::ScreenCaps().LogPixels() * (2 / 3.f), ILC_COLOR32, 1, 1))
	{
		Create(wpos, "Resources", ID, CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS);

		SetIcons(images);
		SetSmallIcons(images);
		SetView(DETAIL_VIEW);
		TileInfo(4).Set(*this);
		images.Add(win::Bitmap::Load("IDB_BUFFER", 0, images.GetIconSize()));
		images.Add(win::Bitmap::Load("IDB_NOTLOADED", 0, images.GetIconSize()));

		RunThread([this]{
			addref();
			GetUsedAt(usedat, con, this->index);

			static const uint32	cols[]	= {2, 9, 10, 22};

			for (auto &i : records) {
				int		x	= GetThumbnail(*this, images, con, i.item);
				int		j	= records.index_of(i);
				AddEntry(i, j, x);
				GetItem(j).TileInfo(cols).Set(*this);
			}

			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	DX11ShadersList
//-----------------------------------------------------------------------------

struct DX11ShadersList : EditableListView<DX11ShadersList, Subclass<DX11ShadersList, EntryTable<DX11Assets::ShaderRecord>>> {
	enum {ID = 'SH'};
	DX11Connection *con;

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX11GPU *main = ViewDX11GPU::Cast(from)) {
			switch (col) {
	/*			case 0:
					EditName(row);
					break;*/
				case 1:
					main->SelectBatch(GetBatches(row));
					break;
				default:
					Busy(), new DX11ShaderWindow(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'id'), con, GetEntry(row));
					break;
			}
		}
	}

	DX11ShadersList(const WindowPos &wpos, DX11Connection *_con) : Base(_con->shaders), con(_con) {
		Create(wpos, "Shaders", ID, CHILD | CLIPSIBLINGS | VISIBLE | REPORT | AUTOARRANGE | SINGLESEL | SHOWSELALWAYS);

		RunThread([this]{
			addref();
			
			auto_array<int>	index(alloc_auto(int, con->items.size()), con->items.size(), -1);
			for (auto &i : table)
				index[con->GetIndex(i.item)] = table.index_of(i);

			GetUsedAt(usedat, con, index);

			for (auto *i = table.begin(), *e = table.end(); i != e; ++i) {
				int		j	= table.index_of(i);
				AddEntry(*i, j);
				GetItem(j).Set(*this);
			}

			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	DX11TimingWindow
//-----------------------------------------------------------------------------

class DX11TimingWindow : public TimingWindow {
	DX11Connection	&con;
	float			rfreq;

	void	Paint(const win::DeviceContextPaint &dc);
	int		GetBatch(float t)		const	{ return t < 0 ? 0 : con.GetBatchByTime(uint64(t * con.frequency)); }
	float	GetBatchTime(int i)		const	{ return float(con.batches[i].timestamp) * rfreq; }
	void	SetBatch(int i);
	void	GetTipText(string_accum &&acc, float t) const;
	void	Reset();

public:
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	DX11TimingWindow(const WindowPos &wpos, Control owner, DX11Connection &con) : TimingWindow(wpos, owner), con(con) {
		Reset();
		Rebind(this);
	}
	void Refresh() {
		Reset();
		Invalidate();
	}
};

void DX11TimingWindow::Reset() {
	time		= 0;
	last_batch	= 0;
	rfreq		= 1.f / con.frequency;
	tscale		= ((con.batches.back().timestamp - con.batches.front().timestamp)) * rfreq;

	float maxy	= 1;
	for (auto &b : con.batches)
		maxy = max(maxy, (b.stats.PSInvocations + b.stats.CSInvocations) / float(b.Duration()));
	yscale		= maxy * con.frequency * 1.1f;
}

void DX11TimingWindow::SetBatch(int i) {
	if (i != last_batch) {
		int	x0	= TimeToClient(GetBatchTime(last_batch));
		int	x1	= TimeToClient(GetBatchTime(i));
		if (x1 < x0)
			swap(x0, x1);

		Invalidate(DragStrip().SetLeft(x0 - 10).SetRight(x1 + 10), false);
		last_batch = i;
		Update();
	}
}
void DX11TimingWindow::GetTipText(string_accum &&acc, float t) const {
	acc << "t=" << t * 1000 << "ms";
	int	i = GetBatch(t);
	if (i < con.batches.size())
		acc << "; batch=" << i - 1 << " (" << GetBatchTime(i) << "ms)";
}

LRESULT DX11TimingWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			return 0;

		case WM_PAINT:
			Paint(hWnd);
			break;

		case WM_ISO_BATCH:
			SetBatch(wParam + 1);
			break;

		case WM_RBUTTONDOWN:
			SetCapture(*this);
			SetFocus();
			break;

		case WM_LBUTTONDOWN: {
			SetCapture(*this);
			SetFocus();
			Point	mouse(lParam);
			int	batch = GetBatch(ClientToTime(mouse.x));
			if (batch && batch < con.batches.size() && DragStrip().Contains(mouse)) {
				owner(WM_ISO_BATCH, batch - 1);
				SetBatch(batch - 1);
			}
			break;
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			ReleaseCapture();
			return 0;

		case WM_MOUSEMOVE: {
			Point	mouse	= Point(lParam);
			if (wParam & MK_LBUTTON) {
				float	prev_time	= time;
				time -= (mouse.x - prevmouse.x) * tscale / GetClientRect().Width();
				Invalidate();
			} else if (wParam & MK_RBUTTON) {
				int	i = min(GetBatch(ClientToTime(mouse.x)), con.batches.size() - 1);
				if (i != last_batch && i)
					owner(WM_ISO_JOG, i - 1);
				SetBatch(i);
			}
			prevmouse	= mouse;
			break;
		}

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA: {
					NMTTDISPINFOA	*nmtdi	= (NMTTDISPINFOA*)nmh;
					GetTipText(fixed_accum(nmtdi->szText), ClientToTime(ToClient(GetMousePos()).x));
					return 0;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;
	}
	return TimingWindow::Proc(message, wParam, lParam);
}

D3D11_QUERY_DATA_PIPELINE_STATISTICS &operator+=(D3D11_QUERY_DATA_PIPELINE_STATISTICS &a, D3D11_QUERY_DATA_PIPELINE_STATISTICS &b) {
	a.IAVertices	+= b.IAVertices;
	a.IAPrimitives	+= b.IAPrimitives;
	a.VSInvocations	+= b.VSInvocations;
	a.GSInvocations	+= b.GSInvocations;
	a.GSPrimitives	+= b.GSPrimitives;
	a.CInvocations	+= b.CInvocations;
	a.CPrimitives	+= b.CPrimitives;
	a.PSInvocations	+= b.PSInvocations;
	a.HSInvocations	+= b.HSInvocations;
	a.DSInvocations	+= b.DSInvocations;
	a.CSInvocations	+= b.CSInvocations;
	return a;
}

void DX11TimingWindow::Paint(const DeviceContextPaint &dc) {
	Rect		client	= GetClientRect();
	d2d::rect	dirty	= d2d.FromPixels(d2d.device ? dc.GetDirtyRect() : client);

	d2d.Init(*this, client.Size());
	if (!d2d.Occluded()) {
		d2d.BeginDraw();
//		d2d.UsePixels();
		d2d.SetTransform(identity);
		d2d.device->PushAxisAlignedClip(dirty, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		d2d.Clear(colour(zero));

		auto		wh		= d2d.Size();
		float		w		= wh.x, h = wh.y;
		float2x3	trans	= GetTrans();
		d2d::rect	dirty2	= dirty / trans;

		int			batch0	= max(GetBatch(dirty2.left) - 1, 0);
		int			batch1	= min(GetBatch(dirty2.right), con.batches.size() - 1);

		{// batch bands
			d2d::SolidBrush	cols[2] = {
				{d2d, colour(0.2f,0.2f,0.2f,1)},
				{d2d, colour(0.3f,0.3f,0.3f,1)},
			};
			for (int i = batch0; i < batch1; ++i)
				d2d.Fill(d2d::rect(TimeToClient(GetBatchTime(i + 0)), 0, TimeToClient(GetBatchTime(i + 1)), h), cols[i & 1]);
		}

		{// batch bars
			d2d::SolidBrush	cols[2] = {
				{d2d, colour(1,0,0)},	//PS red
				{d2d, colour(0,1,0)}	//CS green
			};

			float	xs	= trans.x.x;
			for (int i = batch0; i < batch1;) {
				//collect batches that are too close horizontally
				int		i0		= i++;
				uint64	tn		= con.batches[i0].timestamp + uint32(con.frequency / xs);
				auto	stats	= con.batches[i0].stats;
				while (i < batch1 && con.batches[i].timestamp < tn) {
					stats += con.batches[i].stats;
					++i;
				}

				float	d	= (con.batches[i].timestamp - con.batches[i0].timestamp) * rfreq;
				float	t0	= GetBatchTime(i0);
				float	t1	= GetBatchTime(i);
				d2d.Fill(trans * d2d::rect(t0, 0, t1, stats.PSInvocations / d), cols[0]);
				d2d.Fill(trans * d2d::rect(t0, stats.PSInvocations / d, t1, (stats.PSInvocations + stats.CSInvocations) / d), cols[1]);
			}
		}

		d2d::Write	write;

		{// batch labels
			d2d::Font	font(write, L"arial", 20);
			font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
			d2d::SolidBrush	textbrush(d2d, colour(one));

			float	t0	= -100;
			float	s	= trans.x.x * rfreq;
			float	o	= trans.z.x;
			for (int i = batch0; i < batch1; i++) {
				float	t1	= ((con.batches[i].timestamp + con.batches[i+1].timestamp) / 2) * s + o;
				if (t1 > t0) {
					d2d.DrawText(d2d::rect(t1 - 100, h - 20, t1 + 100, h), str16(buffer_accum<256>() << i), font, textbrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
					t0 = t1 + 60;
				}
			}
		}

		DrawGrid(d2d, d2d::Font(write, L"arial", 10));
		DrawMarker(d2d, d2d::point(TimeToClient(GetBatchTime(last_batch)), 0));

		d2d.device->PopAxisAlignedClip();

		if (d2d.EndDraw()) {
			d2d.DeInit();
			marker_geom.clear();
		}
	}
}

//-----------------------------------------------------------------------------
//	ViewDX11GPU
//-----------------------------------------------------------------------------

IDFMT	ViewDX11GPU::init_format = IDFMT_LEAVE | IDFMT_FOLLOWPTR;
win::Colour ViewDX11GPU::colours[] = {
	{0,0,0},		//WT_NONE,
	{0,0,0,2},		//WT_BATCH,
	{128,0,0,2},	//WT_MARKER,
	{0,64,0,1},		//WT_CALLSTACK,
	{128,0,0},		//WT_RESOURCE,
	{128,0,0},		//WT_DATA
};

uint8 ViewDX11GPU::cursor_indices[][3] = {
	{0,0,0},		//WT_NONE,
	{1,2,2},		//WT_BATCH,
	{1,2,2},		//WT_MARKER,
	{1,2,2},		//WT_CALLSTACK,
	{1,2,2},		//WT_REPEAT,
	{1,2,2},		//WT_DATA,
};
LRESULT ViewDX11GPU::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			tree.Create(_GetPanePos(0), none, CHILD | CLIPSIBLINGS | VISIBLE | VSCROLL | tree.HASLINES | tree.HASBUTTONS | tree.LINESATROOT | tree.SHOWSELALWAYS, CLIENTEDGE);
			italics = tree.GetFont().GetParams().Italic(true);
			TabWindow	*t = new TabWindow;
			SetPanes(tree, t->Create(_GetPanePos(1), "tabs", CHILD | CLIPCHILDREN | VISIBLE), 400);
			t->SetFont(win::Font::DefaultGui());
			return 0;
		}

		case WM_CTLCOLORSTATIC:
			return (LRESULT)GetStockObject(WHITE_BRUSH);

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_FILE_SAVE: {
					filename	fn;
					if (GetSave(*this, fn, "Save Capture", "Binary\0*.ib\0Compressed\0*.ibz\0Text\0*.ix\0")) {
						Busy(), Save(fn);
					#ifndef ISO_EDITOR
						MainWindow::Get()->SetFilename(fn);
					#endif
					}
					return 0;
				}

				case ID_DX11GPU_RAW:
					return 0;

				case ID_DX11GPU_OFFSETS:
					init_format = (format ^= IDFMT_OFFSETS);
					Busy(), MakeTree();
					return 0;

				case ID_DX11GPU_ALLOBJECTS:
					if (!SelectTab(DX11ItemsList::ID))
						new DX11ItemsList(Dock(DOCK_TAB), this);
					return 0;

				case ID_DX11GPU_ALLRESOURCES:
					if (!SelectTab(DX11ResourcesList::ID))
						new DX11ResourcesList(Dock(DOCK_TAB), this);
					return 0;

				case ID_DX11GPU_ALLSHADERS:
					if (!SelectTab(DX11ShadersList::ID))
						new DX11ShadersList(Dock(DOCK_TAB), this);
					return 0;

				case ID_DX11GPU_TIMER:
					if (Control c = SelectTab(DX11TimingWindow::ID)) {
						Busy	bee;
						GetStatistics();
						CastByProc<DX11TimingWindow>(c)->Refresh();
					} else {
						Busy	bee;
						GetStatistics();
						new DX11TimingWindow(Dock(DOCK_TOP, 100), *this, *this);
					}
					return 0;
					/*
				default: {
					static bool reentry = false;
					if (reentry)
						break;
					reentry = true;
					int	ret = GetPane(1)(message, wParam, lParam);
					reentry = false;
					return ret;
				}*/
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CLICK: {
					if (nmh->hwndFrom == *this)
						return 0;

					Control(nmh->hwndFrom).SetFocus();

					if (nmh->hwndFrom == tree) {
						uint32		flags;
						HTREEITEM	h = tree.HitTest(tree.ToClient(GetMousePos()), &flags);
						if (h && (flags & TVHT_ONITEMLABEL) && h == tree.GetSelectedItem())
							TreeSelection(h);
						break;
					}

					return ReflectNotification(*this, wParam, (NMITEMACTIVATE*)nmh);
				}

				case NM_RCLICK:
					switch (wParam) {
						case DX11ItemsList::ID:
						case DX11ShadersList::ID:
							return ReflectNotification(*this, wParam, (NMITEMACTIVATE*)nmh);
					}
					break;

				case NM_DBLCLK:
					if (nmh->hwndFrom == tree) {
						TreeDoubleClick(tree.GetSelectedItem());
						return TRUE;	// prevent opening tree item
					}
					break;

				case LVN_COLUMNCLICK:
					switch (nmh->idFrom) {
						case DX11ItemsList::ID:
							DX11ItemsList::Cast(nmh->hwndFrom)->SortOnColumn1(((NMLISTVIEW*)nmh)->iSubItem);
							return 0;
						case DX11ResourcesList::ID:
							DX11ResourcesList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							return 0;
						case DX11ShadersList::ID:
							DX11ShadersList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							return 0;
					}
					break;

				case LVN_GETDISPINFOW:
					return ReflectNotification(*this, wParam, (NMLVDISPINFOW*)nmh);

				case LVN_GETDISPINFOA:
					return ReflectNotification(*this, wParam, (NMLVDISPINFOA*)nmh);

				case TVN_ITEMEXPANDING: {
					NMTREEVIEW	*nmtv	= (NMTREEVIEW*)nmh;
					if (nmtv->hdr.hwndFrom == tree && nmtv->action == TVE_EXPAND && !(nmtv->itemNew.state & TVIS_EXPANDEDONCE) && !tree.GetChildItem(nmtv->itemNew.hItem))
						ExpandTree(nmtv->itemNew.hItem);
					return 0;
				}
				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					return ReflectNotification(*this, wParam, (NMLVCUSTOMDRAW*)nmh);

				case TVN_SELCHANGED:
					if (nmh->hwndFrom == tree && tree.IsVisible()) {
						TreeSelection(tree.GetSelectedItem());
						return 0;
					}
					break;

				case TCN_SELCHANGE:
					TabControl2(nmh->hwndFrom).ShowSelectedControl();
					return 0;

				case TCN_DRAG:
					DragTab(*this, nmh->hwndFrom, nmh->idFrom, !!(GetKeyState(VK_SHIFT) & 0x8000));
					return 1;

				case TCN_CLOSE:
					TabControl2(nmh->hwndFrom).GetItemControl(nmh->idFrom).Destroy();
					return 1;
			}
			break;
		}

		case WM_MOUSEACTIVATE:
			return MA_ACTIVATE;

		case WM_PARENTNOTIFY:
			if (LOWORD(wParam) == WM_DESTROY && GetPane(1) == Control(lParam)) {
				TabWindow	*t = new TabWindow;
				SetPane(1, t->Create(_GetPanePos(1), "tabs", CHILD | CLIPCHILDREN | VISIBLE));
				t->SetFont(win::Font::DefaultGui());
				return 1;
			}
			break;

		case WM_CONTEXTMENU: {
			if (context_item = tree.HitTest(tree.ToClient(GetMousePos()))) {
				Menu	m = Menu(IDR_MENU_DX11GPU).GetSubMenuByPos(0);
				m.Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
			}
			break;
		}
		case WM_ISO_BATCH:
			SelectBatch(wParam);
			break;

		case WM_ISO_JOG: {
			auto		&batch	= batches[wParam];
			uint32		addr	= batch.addr;

			DX11Replay	replay;
			replay.CreateObjects(*this);
			replay.RunTo(recordings, addr);

			dx::Resource	res = replay->GetResource(FindItem(batch.rtv));
			auto	p	= dx::GetBitmap("target", res);
			Control	c	= SelectTab('JG');
			if (c) {
				ISO::Browser2	b(p);
				c.SendMessage(WM_COMMAND, ID_EDIT, &b);
			} else {
				c		= BitmapWindow(Dock(DOCK_TABID, 'JG'), p, tag(p.ID()), true);
				c.id	= 'JG';
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

void ViewDX11GPU::TreeSelection(HTREEITEM hItem) {
	TreeControl::Item	i		= tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM| TVIF_SELECTEDIMAGE | TVIF_STATE);
	bool				new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);
	switch (i.Image()) {
		case WT_BATCH: {
			Busy		bee;
			uint32		addr	= i.Param();

			if (frequency) {
				DX11Replay	replay;
				replay.CreateObjects(*this);
				replay.RunTo(recordings, addr);
				new DX11BatchWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, DX11BatchWindow::ID), tree.GetItemText(hItem), this, addr, &replay);
			} else {
				new DX11BatchWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, DX11BatchWindow::ID), tree.GetItemText(hItem), this, addr, 0);
			}
			break;
		}
		case WT_CALLSTACK: {
			Busy	bee;
			uint64	pc		= i.Image2() ? *(uint64*)i.Param() : (uint64)i.Param();
			auto	frame	= stack_dumper.GetFrame(pc);
			if (frame.file && exists(frame.file)) {
				auto	file	= files.get(frame.file);
				EditControl	c = MakeSourceWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'SC'), file, HLSLcolourerRE(), none, EditControl::READONLY);
				c.id	= 'SC';
				ShowSourceLine(c, frame.line);
			}
			break;
		}
		case WT_RESOURCE:
			Busy(), MakeItemView(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'SC'), this, i.Param());
			break;

		case WT_DATA: {
			Busy		bee;
			uint16		*p		= i.Param();
			COMRecording::header	*h	= (COMRecording::header*)(p - 2);
			BinaryWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'SC'), ISO::MakeBrowser(h->block()));
			break;
		}
	}
}

void ViewDX11GPU::TreeDoubleClick(HTREEITEM hItem) {
}

field_info DX11DeviceContext_commands[] = {
	{"<InitialState>",									fields<dx11::DeviceContext_State>::f},
	{"VSSetConstantBuffers(%0, %1,)",					ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"))},
	{"PSSetShaderResources(%0, %1,)",					ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11ShaderResourceView*,1>>("pc"))},
	{"PSSetShader(%0)",									ff(fp<ID3D11PixelShader*>("shader"), fp<counted<ID3D11ClassInstance*,2>>("pc"), fp<UINT>("numc"))},
	{"PSSetSamplers",									ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11SamplerState*,1>>("pc"))},
	{"VSSetShader(%0)",									ff(fp<ID3D11VertexShader*>("shader"), fp<counted<ID3D11ClassInstance*,2>>("pc"), fp<UINT>("numc"))},
	{"DrawIndexed(%0,%1,%2)",							ff(fp<UINT>("IndexCount"), fp<UINT>("StartIndex"), fp<INT>("BaseVertex"))},
	{"Draw(%0,%1)",										ff(fp<UINT>("VertexCount"), fp<UINT>("StartVertex"))},
	{"Map",												ff(fp<ID3D11Resource*>("rsrc"), fp<UINT>("sub"), fp<D3D11_MAP>("MapType"), fp<UINT>("MapFlags"), fp<D3D11_MAPPED_SUBRESOURCE*>("mapped"))},
	{"Unmap",											ff(fp<ID3D11Resource*>("rsrc"), fp<UINT>("sub"))},
	{"PSSetConstantBuffers",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"))},
	{"IASetInputLayout",								ff(fp<ID3D11InputLayout*>("pInputLayout"))},
	{"IASetVertexBuffers",								ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"), fp<counted<const UINT,1>>("strides"), fp<counted<const UINT,1>>("pOffsets"))},
	{"IASetIndexBuffer",								ff(fp<ID3D11Buffer*>("pIndexBuffer"), fp<DXGI_FORMAT>("Format"), fp<UINT>("Offset"))},
	{"DrawIndexedInstanced(%0,%1,%2,%3,%4)",			ff(fp<UINT>("IndexCountPerInstance"), fp<UINT>("InstanceCount"), fp<UINT>("StartIndex"), fp<INT>("BaseVertex"), fp<UINT>("StartInstance"))},
	{"DrawInstanced(%0,%1,%2<%3)",						ff(fp<UINT>("VertexCountPerInstance"), fp<UINT>("InstanceCount"), fp<UINT>("StartVertex"), fp<UINT>("StartInstance"))},
	{"GSSetConstantBuffers",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"))},
	{"GSSetShader(%0)",									ff(fp<ID3D11GeometryShader*>("pShader"), fp<counted<ID3D11ClassInstance*,2>>("pc"), fp<UINT>("numc"))},
	{"IASetPrimitiveTopology",							ff(fp<D3D11_PRIMITIVE_TOPOLOGY>("Topology"))},
	{"VSSetShaderResources",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11ShaderResourceView*,1>>("pc"))},
	{"VSSetSamplers",									ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11SamplerState*,1>>("pc"))},
	{"Begin",											ff(fp<ID3D11Asynchronous*>("async"))},
	{"End",												ff(fp<ID3D11Asynchronous*>("async"))},
	{"SetPredication",									ff(fp<ID3D11Predicate*>("pred"), fp<BOOL>("PredicateValue"))},
	{"GSSetShaderResources",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11ShaderResourceView*,1>>("pc"))},
	{"GSSetSamplers",									ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11SamplerState*,1>>("pc"))},
	{"OMSetRenderTargets",								ff(fp<UINT>("num"), fp<counted<ID3D11RenderTargetView*const,0>>("ppRTV"), fp<ID3D11DepthStencilView*>("dsv"))},
	{"OMSetRenderTargetsAndUnorderedAccessViews",		ff(fp<UINT>("NumRTVs"), fp<counted<ID3D11RenderTargetView*const,0>>("ppRTV"), fp<ID3D11DepthStencilView*>("dsv"), fp<UINT>("UAVStartSlot"), fp<UINT>("NumUAVs"), fp<counted<ID3D11UnorderedAccessView*const,4>>("ppUAV"), fp<const UINT*>("pUAVInitialCounts"))},
	{"OMSetBlendState",									ff(fp<ID3D11BlendState*>("pBlendState"), fp<const FLOAT>("BlendFactor[4]"), fp<UINT>("SampleMask"))},
	{"OMSetDepthStencilState",							ff(fp<ID3D11DepthStencilState*>("pDepthStencilState"), fp<UINT>("StencilRef"))},
	{"SOSetTargets",									ff(fp<UINT>("num"), fp<counted<ID3D11Buffer*,0>>("pc"), fp<counted<const UINT,0>>("offsets"))},
	{"DrawAuto",										ff()},
	{"DrawIndexedInstancedIndirect(%0)",				ff(fp<ID3D11Buffer*>("buffer"), fp<UINT>("AlignedByteOffsetForArgs"))},
	{"DrawInstancedIndirect(%0,%1)",					ff(fp<ID3D11Buffer*>("buffer"), fp<UINT>("AlignedByteOffsetForArgs"))},
	{"Dispatch(%0,%1,%2)",								ff(fp<UINT>("ThreadGroupCountX"), fp<UINT>("ThreadGroupCountY"), fp<UINT>("ThreadGroupCountZ"))},
	{"DispatchIndirect(%0,%1)",							ff(fp<ID3D11Buffer*>("buffer"), fp<UINT>("AlignedByteOffsetForArgs"))},
	{"RSSetState",										ff(fp<ID3D11RasterizerState*>("pRasterizerState"))},
	{"RSSetViewports",									ff(fp<UINT>("num"), fp<counted<const D3D11_VIEWPORT,0>>("viewports"))},
	{"RSSetScissorRects",								ff(fp<UINT>("num"), fp<counted<const D3D11_RECT,0>>("rects"))},
	{"CopySubresourceRegion",							ff(fp<ID3D11Resource*>("dst"), fp<UINT>("DstSubresource"), fp<UINT>("DstX"), fp<UINT>("DstY"), fp<UINT>("DstZ"), fp<ID3D11Resource*>("src"), fp<UINT>("SrcSubresource"), fp<const D3D11_BOX*>("pSrcBox"))},
	{"CopyResource",									ff(fp<ID3D11Resource*>("dst"), fp<ID3D11Resource*>("src"))},
	{"UpdateSubresource(%0,%1)",						ff(fp<ID3D11Resource*>("dst"), fp<UINT>("DstSubresource"), fp<const D3D11_BOX*>("pDstBox"), fp<const void*>("pSrcData"), fp<UINT>("SrcRowPitch"), fp<UINT>("SrcDepthPitch"))},
	{"CopyStructureCount",								ff(fp<ID3D11Buffer*>("pDstBuffer"), fp<UINT>("DstAlignedByteOffset"), fp<ID3D11UnorderedAccessView*>("pSrcView"))},
	{"ClearRenderTargetView(%0)",						ff(fp<ID3D11RenderTargetView*>("pRenderTargetView"), fp<const FLOAT[4]>("ColorRGBA[4]"))},
	{"ClearUnorderedAccessViewUint",					ff(fp<ID3D11UnorderedAccessView*>("uav"), fp<const UINT>("Values[4]"))},
	{"ClearUnorderedAccessViewFloat",					ff(fp<ID3D11UnorderedAccessView*>("uav"), fp<const FLOAT>("Values[4]"))},
	{"ClearDepthStencilView(%0)",						ff(fp<ID3D11DepthStencilView*>("dsv"), fp<UINT>("ClearFlags"), fp<FLOAT>("Depth"), fp<UINT8>("Stencil"))},
	{"GenerateMips",									ff(fp<ID3D11ShaderResourceView*>("pShaderResourceView"))},
	{"SetResourceMinLOD",								ff(fp<ID3D11Resource*>("rsrc"), fp<FLOAT>("MinLOD"))},
	{"ResolveSubresource",								ff(fp<ID3D11Resource*>("dst"), fp<UINT>("DstSubresource"), fp<ID3D11Resource*>("src"), fp<UINT>("SrcSubresource"), fp<DXGI_FORMAT>("Format"))},
	{"ExecuteCommandList",								ff(fp<ID3D11CommandList*>("pCommandList"), fp<BOOL>("RestoreContextState"))},
	{"HSSetShaderResources",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11ShaderResourceView*,1>>("pc"))},
	{"HSSetShader(%0)",									ff(fp<ID3D11HullShader*>("shader"), fp<counted<ID3D11ClassInstance*,2>>("pc"), fp<UINT>("numc"))},
	{"HSSetSamplers",									ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11SamplerState*,1>>("pc"))},
	{"HSSetConstantBuffers",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"))},
	{"DSSetShaderResources",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11ShaderResourceView*,1>>("pc"))},
	{"DSSetShader(%0)",									ff(fp<ID3D11DomainShader*>("shader"), fp<counted<ID3D11ClassInstance*,2>>("pc"), fp<UINT>("numc"))},
	{"DSSetSamplers",									ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11SamplerState*,1>>("pc"))},
	{"DSSetConstantBuffers",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"))},
	{"CSSetShaderResources",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11ShaderResourceView*,1>>("pc"))},
	{"CSSetUnorderedAccessViews",						ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11UnorderedAccessView*,1>>("pp"), fp<counted<UINT,1>>("pUAVInitialCounts"))},
	{"CSSetShader(%0)",									ff(fp<ID3D11ComputeShader*>("shader"), fp<counted<ID3D11ClassInstance*,2>>("pc"), fp<UINT>("numc"))},
	{"CSSetSamplers",									ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11SamplerState*,1>>("pc"))},
	{"CSSetConstantBuffers",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"))},
	{"ClearState",										ff()},
	{"Flush",											ff()},
	{"FinishCommandList",								ff(fp<BOOL>("RestoreDeferredContextState"), fp<ID3D11CommandList**>("pp"))},
//ID3D11DeviceContext1
	{"CopySubresourceRegion1",							ff(fp<ID3D11Resource*>("dst"), fp<UINT>("DstSubresource"), fp<UINT>("DstX"), fp<UINT>("DstY"), fp<UINT>("DstZ"), fp<ID3D11Resource*>("src"), fp<UINT>("SrcSubresource"), fp<const D3D11_BOX*>("pSrcBox"), fp<UINT>("CopyFlags"))},
	{"UpdateSubresource1",								ff(fp<ID3D11Resource*>("dst"), fp<UINT>("DstSubresource"), fp<const D3D11_BOX*>("pDstBox"), fp<const void*>("pSrcData"), fp<UINT>("SrcRowPitch"), fp<UINT>("SrcDepthPitch"), fp<UINT>("CopyFlags"))},
	{"DiscardResource",									ff(fp<ID3D11Resource*>("rsrc"))},
	{"DiscardView",										ff(fp<ID3D11View*>("pResourceView"))},
	{"VSSetConstantBuffers1",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"), fp<const UINT*>("cnst"), fp<const UINT*>("numc"))},
	{"HSSetConstantBuffers1",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"), fp<const UINT*>("cnst"), fp<const UINT*>("numc"))},
	{"DSSetConstantBuffers1",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"), fp<const UINT*>("cnst"), fp<const UINT*>("numc"))},
	{"GSSetConstantBuffers1",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"), fp<const UINT*>("cnst"), fp<const UINT*>("numc"))},
	{"PSSetConstantBuffers1",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"), fp<const UINT*>("cnst"), fp<const UINT*>("numc"))},
	{"CSSetConstantBuffers1",							ff(fp<UINT>("slot"), fp<UINT>("num"), fp<counted<ID3D11Buffer*,1>>("pc"), fp<const UINT*>("cnst"), fp<const UINT*>("numc"))},
	{"SwapDeviceContextState",							ff(fp<ID3DDeviceContextState*>("state"), fp<ID3DDeviceContextState**>("pp"))},
	{"ClearView",										ff(fp<ID3D11View*>("pView"), fp<const FLOAT>("Color[4]"), fp<const D3D11_RECT*>("pRect"), fp<UINT>("num"))},
	{"DiscardView1",									ff(fp<ID3D11View*>("pResourceView"), fp<const D3D11_RECT*>("pRects"), fp<UINT>("num"))},
//ID3D11DeviceContext2
	{"UpdateTileMappings",								ff(fp<ID3D11Resource*>("pTiledResource"), fp<UINT>("NumTiledResourceRegions"), fp<const D3D11_TILED_RESOURCE_COORDINATE*>("pTiledResourceRegionStartCoordinates"), fp<const D3D11_TILE_REGION_SIZE*>("pTiledResourceRegionSizes"), fp<ID3D11Buffer*>("pTilePool"), fp<UINT>("NumRanges"), fp<const UINT*>("pRangeFlags"), fp<const UINT*>("pTilePoolStartOffsets"), fp<const UINT*>("pRangeTileCounts"), fp<UINT>("Flags"))},
	{"CopyTileMappings",								ff(fp<ID3D11Resource*>("pDestTiledResource"), fp<const D3D11_TILED_RESOURCE_COORDINATE*>("pDestRegionStartCoordinate"), fp<ID3D11Resource*>("pSourceTiledResource"), fp<const D3D11_TILED_RESOURCE_COORDINATE*>("pSourceRegionStartCoordinate"), fp<const D3D11_TILE_REGION_SIZE*>("pTileRegionSize"), fp<UINT>("Flags"))},
	{"CopyTiles",										ff(fp<ID3D11Resource*>("pTiledResource"), fp<const D3D11_TILED_RESOURCE_COORDINATE*>("pTileRegionStartCoordinate"), fp<const D3D11_TILE_REGION_SIZE*>("pTileRegionSize"), fp<ID3D11Buffer*>("pBuffer"), fp<UINT64>("BufferStartOffsetInBytes"), fp<UINT>("Flags"))},
	{"UpdateTiles",										ff(fp<ID3D11Resource*>("pDestTiledResource"), fp<const D3D11_TILED_RESOURCE_COORDINATE*>("pDestTileRegionStartCoordinate"), fp<const D3D11_TILE_REGION_SIZE*>("pDestTileRegionSize"), fp<const void*>("pSourceTileData"), fp<UINT>("Flags"))},
	{"ResizeTilePool",									ff(fp<ID3D11Buffer*>("pTilePool"), fp<UINT64>("NewSizeInBytes"))},
	{"TiledResourceBarrier",							ff(fp<ID3D11DeviceChild*>("pTiledResourceOrViewAccessBeforeBarrier"), fp<ID3D11DeviceChild*>("pTiledResourceOrViewAccessAfterBarrier"))},
	{"SetMarkerInt",									ff()},
	{"BeginEventInt",									ff(fp<LPCWSTR>("pLabel"), fp<INT>("Data"))},
	{"EndEvent",										ff(fp<LPCWSTR>("pLabel"), fp<INT>("Data"))},
//ID3D11DeviceContext3
	{"Flush1",											ff(fp<D3D11_CONTEXT_TYPE>("ContextType"), fp<HANDLE>("hEvent"))},
	{"SetHardwareProtectionState",						ff(fp<BOOL>("HwProtectionEnable"))},
//ID3D11DeviceContext4
	{"Signal",											ff(fp<ID3D11Fence*>("pFence"), fp<UINT64>("Value"))},
	{"Wait",											ff(fp<ID3D11Fence*>("pFence"), fp<UINT64>("Value"))},
	{"DATA",											fields<const_memory_block>::f},
	{"RLE_DATA",										fields<const_memory_block>::f},
};

field_info DX11Device_commands[] = {
	{"CreateBuffer %2",									ff(fp<const D3D11_BUFFER_DESC*>("desc"), fp<const D3D11_SUBRESOURCE_DATA*>("data"), fp<ID3D11Buffer**>("pp"))},
	{"CreateTexture1D %2",								ff(fp<const D3D11_TEXTURE1D_DESC*>("desc"), fp<const D3D11_SUBRESOURCE_DATA*>("data"), fp<ID3D11Texture1D**>("pp"))},
	{"CreateTexture2D %2",								ff(fp<const D3D11_TEXTURE2D_DESC*>("desc"), fp<const D3D11_SUBRESOURCE_DATA*>("data"), fp<ID3D11Texture2D**>("pp"))},
	{"CreateTexture3D %2",								ff(fp<const D3D11_TEXTURE3D_DESC*>("desc"), fp<const D3D11_SUBRESOURCE_DATA*>("data"), fp<ID3D11Texture3D**>("pp"))},
	{"CreateShaderResourceView %2",						ff(fp<ID3D11Resource*>("rsrc"), fp<const D3D11_SHADER_RESOURCE_VIEW_DESC*>("desc"), fp<ID3D11ShaderResourceView**>("pp"))},
	{"CreateUnorderedAccessView %2",					ff(fp<ID3D11Resource*>("rsrc"), fp<const D3D11_UNORDERED_ACCESS_VIEW_DESC*>("desc"), fp<ID3D11UnorderedAccessView**>("pp"))},
	{"CreateRenderTargetView %2",						ff(fp<ID3D11Resource*>("rsrc"), fp<const D3D11_RENDER_TARGET_VIEW_DESC*>("desc"), fp<ID3D11RenderTargetView**>("pp"))},
	{"CreateDepthStencilView %2",						ff(fp<ID3D11Resource*>("rsrc"), fp<const D3D11_DEPTH_STENCIL_VIEW_DESC*>("desc"), fp<ID3D11DepthStencilView**>("pp"))},
	{"CreateInputLayout %4",							ff(fp<counted<const D3D11_INPUT_ELEMENT_DESC,1>>("desc"), fp<UINT>("num"), fp<const void*>("bytecode"), fp<SIZE_T>("length"), fp<ID3D11InputLayout**>("pp"))},
	{"CreateVertexShader %3",							ff(fp<const void*>("code"), fp<SIZE_T>("length"), fp<ID3D11ClassLinkage*>("link"), fp<ID3D11VertexShader**>("pp"))},
	{"CreateGeometryShader %3",							ff(fp<const void*>("code"), fp<SIZE_T>("length"), fp<ID3D11ClassLinkage*>("link"), fp<ID3D11GeometryShader**>("pp"))},
	{"CreateGeometryShaderWithStreamOutput %8",			ff(fp<const void*>("code"), fp<SIZE_T>("length"), fp<const D3D11_SO_DECLARATION_ENTRY*>("pSODeclaration"), fp<UINT>("NumEntries"), fp<const UINT*>("pBufferStrides"), fp<UINT>("NumStrides"), fp<UINT>("RasterizedStream"), fp<ID3D11ClassLinkage*>("link"), fp<ID3D11GeometryShader**>("pp"))},
	{"CreatePixelShader %3",							ff(fp<const void*>("code"), fp<SIZE_T>("length"), fp<ID3D11ClassLinkage*>("link"), fp<ID3D11PixelShader**>("pp"))},
	{"CreateHullShader %3",								ff(fp<const void*>("code"), fp<SIZE_T>("length"), fp<ID3D11ClassLinkage*>("link"), fp<ID3D11HullShader**>("pp"))},
	{"CreateDomainShader %3",							ff(fp<const void*>("code"), fp<SIZE_T>("length"), fp<ID3D11ClassLinkage*>("link"), fp<ID3D11DomainShader**>("pp"))},
	{"CreateComputeShader %3",							ff(fp<const void*>("code"), fp<SIZE_T>("length"), fp<ID3D11ClassLinkage*>("link"), fp<ID3D11ComputeShader**>("pp"))},
	{"CreateClassLinkage",								ff(fp<ID3D11ClassLinkage**>("pp"))},
	{"CreateBlendState %1",								ff(fp<const D3D11_BLEND_DESC*>("desc"), fp<ID3D11BlendState**>("pp"))},
	{"CreateDepthStencilState %1",						ff(fp<const D3D11_DEPTH_STENCIL_DESC*>("desc"), fp<ID3D11DepthStencilState**>("pp"))},
	{"CreateRasterizerState %1",						ff(fp<const D3D11_RASTERIZER_DESC*>("desc"), fp<ID3D11RasterizerState**>("pp"))},
	{"CreateSamplerState %1",							ff(fp<const D3D11_SAMPLER_DESC*>("desc"), fp<ID3D11SamplerState**>("pp"))},
	{"CreateQuery %1",									ff(fp<const D3D11_QUERY_DESC*>("desc"), fp<ID3D11Query**>("pp"))},
	{"CreatePredicate %1",								ff(fp<const D3D11_QUERY_DESC*>("desc"), fp<ID3D11Predicate**>("pp"))},
	{"CreateCounter %1",								ff(fp<const D3D11_COUNTER_DESC*>("desc"), fp<ID3D11Counter**>("pp"))},
	{"CreateDeferredContext",							ff(fp<UINT>("flags"), fp<ID3D11DeviceContext**>("pp"))},
//	{"OpenSharedResource",								ff(fp<HANDLE>("hResource"), fp<REFIID>("riid"), fp<void**>("pp"))},
//	{"CheckFormatSupport",								ff(fp<DXGI_FORMAT>("Format"), fp<UINT*>("support"))},
//	{"CheckMultisampleQualityLevels",					ff(fp<DXGI_FORMAT>("Format"), fp<UINT>("SampleCount"), fp<UINT*>("pNum"))},
//	{"CheckCounterInfo",								ff(fp<D3D11_COUNTER_INFO*>("pCounterInfo"))},
//	{"CheckCounter",									ff(fp<const D3D11_COUNTER_DESC*>("desc"), fp<D3D11_COUNTER_TYPE*>("pType"), fp<UINT*>("pActiveCounters"), fp<LPSTR>("szName"), fp<UINT*>("pNameLength"), fp<LPSTR>("szUnits"), fp<UINT*>("pUnitsLength"), fp<LPSTR>("szDescription"), fp<UINT*>("pDescriptionLength"))},
//	{"CheckFeatureSupport",								ff(fp<D3D11_FEATURE>("Feature"), fp<void*>("pFeatureSupportData"), fp<UINT>("FeatureSupportDataSize"))},
	{"SetPrivateData",									ff(fp<REFGUID>("guid"), fp<UINT>("size"), fp<const void*>("data"))},
	{"SetPrivateDataInterface",							ff(fp<REFGUID>("guid"), fp<const IUnknown*>("data"))},
//	{"SetExceptionMode",								ff(fp<UINT>("RaiseFlags"))},
//ID3D11Device1
	{"CreateDeferredContext1 %1",						ff(fp<UINT>("flags"), fp<ID3D11DeviceContext1**>("pp"))},
	{"CreateBlendState1 %1",							ff(fp<const D3D11_BLEND_DESC1*>("desc"), fp<ID3D11BlendState1**>("pp"))},
	{"CreateRasterizerState1 %1",						ff(fp<const D3D11_RASTERIZER_DESC1*>("desc"), fp<ID3D11RasterizerState1**>("pp"))},
	{"CreateDeviceContextState %5",						ff(fp<UINT>("Flags"), fp<counted<const D3D_FEATURE_LEVEL*,2>>("levels"), fp<UINT>("num"), fp<UINT>("SDKVersion"), fp<REFIID>("EmulatedInterface"), fp<D3D_FEATURE_LEVEL*>("chosen"), fp<ID3DDeviceContextState**>("pp"))},
//	{"OpenSharedResource1",								ff(fp<HANDLE>("hResource"), fp<REFIID>("riid"), fp<void**>("pp"))},
//	{"OpenSharedResourceByName",						ff(fp<LPCWSTR>("lpName"), fp<DWORD>("dwDesiredAccess"), fp<REFIID>("riid"), fp<void**>("pp"))},
//ID3D11Device2
	{"CreateDeferredContext2 %1",						ff(fp<UINT>("ContextFlags"), fp<ID3D11DeviceContext2*>("pp"))},
//ID3D11Device3
	{"CreateTexture2D1 %2",								ff(fp<const D3D11_TEXTURE2D_DESC1*>("desc"), fp<const D3D11_SUBRESOURCE_DATA*>("pInitialData"), fp<ID3D11Texture2D1*>("pp"))},
	{"CreateTexture3D1 %2",								ff(fp<const D3D11_TEXTURE3D_DESC1*>("desc"), fp<const D3D11_SUBRESOURCE_DATA*>("pInitialData"), fp<ID3D11Texture3D1*>("pp"))},
	{"CreateRasterizerState2 %1",						ff(fp<const D3D11_RASTERIZER_DESC2*>("pRasterizerDesc"), fp<ID3D11RasterizerState2*>("pp"))},
	{"CreateShaderResourceView1 %2",					ff(fp<ID3D11Resource*>("pResource"), fp<const D3D11_SHADER_RESOURCE_VIEW_DESC1*>("desc"), fp<ID3D11ShaderResourceView1*>("pp"))},
	{"CreateUnorderedAccessView1 %2",					ff(fp<ID3D11Resource*>("pResource"), fp<const D3D11_UNORDERED_ACCESS_VIEW_DESC1*>("desc"), fp<ID3D11UnorderedAccessView1*>("pp"))},
	{"CreateRenderTargetView1 %2",						ff(fp<ID3D11Resource*>("pResource"), fp<const D3D11_RENDER_TARGET_VIEW_DESC1*>("desc"), fp<ID3D11RenderTargetView1*>("pp"))},
	{"CreateQuery1 %1",									ff(fp<const D3D11_QUERY_DESC1*>("pQueryDesc1"), fp<ID3D11Query1*>("pp"))},
	{"CreateDeferredContext3 %1",						ff(fp<UINT>("ContextFlags"), fp<ID3D11DeviceContext3*>("pp"))},
	{"WriteToSubresource %0[%1]",						ff(fp<ID3D11Resource*>("pDstResource"), fp<UINT>("DstSubresource"), fp<const D3D11_BOX*>("pDstBox"), fp<const void*>("pSrcData"), fp<UINT>("SrcRowPitch"), fp<UINT>("SrcDepthPitch"))},
//ID3D11Device4
	{"RegisterDeviceRemovedEvent",						ff(fp<HANDLE>("hEvent"), fp<DWORD*>("pdwCookie"))},
	{"UnregisterDeviceRemoved",							ff(fp<DWORD>("dwCookie"))},
//ID3D11Device5
	{"OpenSharedFence",									ff(fp<HANDLE>("hFence"), fp<REFIID>("ReturnedInterface"), fp<void*>("pp"))},
	{"CreateFence",										ff(fp<UINT64>("InitialValue"), fp<D3D11_FENCE_FLAG>("Flags"), fp<REFIID>("ReturnedInterface"), fp<void*>("pp"))},
};


struct RegisterString {
	string_accum	&sa;
	DX11Connection	*con;
	void	Open(const char* title, uint32 addr) {}
	void	Close() {}
	void	Line(const char* name, const char* value, uint32 addr) {
		//if (name)
		//	sa << name << "=";
		sa << value;
	}
	void	Callback(const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		//if (pf->name)
		//	sa << pf->name << "=";
		uint64		v = pf->get_raw_value(p, offset);
		if (auto *rec = con->FindItem((ID3D11DeviceChild*)v))
			sa << rec->name;
		else
			sa << "(unfound)0x" << hex(v);
	}
	RegisterString(string_accum	&sa, DX11Connection *con) : sa(sa), con(con) {}
};


static void PutCallstack(RegisterTree &rt, HTREEITEM h, const void **callstack) {
	TreeControl::Item("callstack").Image(ViewDX11GPU::WT_CALLSTACK).Param(callstack).Image2(1).Children(1).Insert(rt.tree, h);
}

static void CommandString(RegisterString &&reg, const field_info &f, IDFMT format, const void *data) {
	for (auto s = f.name, n = s; ; s = n + 2) {
		n = strchr(s, '%');
		if (!n) {
			reg.sa << s;
			return;
		}

		if (is_digit(n[1])) {
			reg.sa << str(s, n);
			FieldPutter(reg, format).AddField(f.fields + from_digit(n[1]), (const uint32*)data);
		} else {
			reg.sa << str(s, n + 2);
		}
	}
}

static void PutCommands(RegisterTree &rt, HTREEITEM h, field_info *nf, const_memory_block m, intptr_t offset, DX11Connection *con) {
	for (const COMRecording::header *p = m, *e = (const COMRecording::header*)m.end(); p < e; p = p->next()) {
		const void**	callstack	= 0;
		uint32			addr		= uint32((intptr_t)p + offset);

		while (p->id == 0xfffe) {
			callstack = (const void**)p->data();
			p = p->next();
		}

		if (p == e)
			break;

		HTREEITEM	h2;
		if (p->id == 0xffff) {
			h2 = rt.AddText(h, "WithObject", addr);
			p = p->next();//ignore next
		} else {
			buffer_accum<256>	ba;
			if (rt.format & IDFMT_OFFSETS)
				ba.format("%08x: ", addr);

			CommandString(RegisterString(ba, con), nf[p->id], rt.format, (uint32*)p->data());

			h2 = rt.AddFields(rt.AddText(h, ba, addr), nf[p->id].fields, (uint32*)p->data());
		}

		if (callstack)
			PutCallstack(rt, h2, callstack);
	}
}

void ViewDX11GPU::MakeTree() {
	tree.Show(SW_HIDE);
	tree.DeleteItem();

	RegisterTree	rt(tree, this, format);

	DX11State		state;
	HTREEITEM		h		= TVI_ROOT;
	BatchRecord		*b		= batches.begin();
	BatchRecord		*be		= batches.end() - 1;
	uint32			boffset	= 0;
	uint32			total	= 0;

	for (Recording *r = recordings.begin(), *re = recordings.end(); r != re; ++r) {
		if (r->type == Recording::CONTEXT) {
			HTREEITEM		h2		= TreeControl::Item(format_string("CommandList %i", recordings.index_of(r))).Bold().Expand().Param(total).Insert(tree, h);
			memory_block	m		= r->recording;
			intptr_t		offset	= total - intptr_t(m.p);

			if (boffset == 0)
				boffset = total;

			while (b != be && b->addr - total <= m.size32()) {
				buffer_accum<256>	ba;
				ba << "Batch " << batches.index_of(b) << " : ";
				CommandString(RegisterString(ba, this), DX11DeviceContext_commands[b->op], rt.format, b->data);
				/*
				switch (b->op) {
					case RecDeviceContext::tag_DrawIndexed:
						ba.format("draw %s x %u, indexed", get_field_name(b->Prim()), b->num_verts);
						break;
					case RecDeviceContext::tag_Draw:
						ba.format("draw %s x %u", get_field_name(b->Prim()), b->num_verts);
						break;
					case RecDeviceContext::tag_DrawIndexedInstanced:
						ba.format("draw %s x %u, indexed instanced x %u", get_field_name(b->Prim()), b->num_verts, b->num_instances);
						break;
					case RecDeviceContext::tag_DrawInstanced:
						ba.format("draw %s x %u, instanced x %u", get_field_name(b->Prim()), b->num_verts, b->num_instances);
						break;
					case RecDeviceContext::tag_DrawAuto:
						ba.format("draw %s x %u,", get_field_name(b->Prim()), b->num_verts);
						break;
					case RecDeviceContext::tag_DrawIndexedInstancedIndirect:
						ba.format("draw %s x %u, indexed instanced x %u", get_field_name(b->Prim()), b->num_verts, b->num_instances);
						break;
					case RecDeviceContext::tag_DrawInstancedIndirect:
						ba.format("draw %s x %u, instanced x %u", get_field_name(b->Prim()), b->num_verts, b->num_instances);
						break;
					case RecDeviceContext::tag_Dispatch:
					case RecDeviceContext::tag_DispatchIndirect:
						ba.format("dispatch %u x %u x %u", uint32(b->dim.x), uint32(b->dim.y), uint32(b->dim.z));
						break;
					default:
						CommandString(RegisterString(ba, this), DX11DeviceContext_commands[b->op], rt.format, b->data);
						break;
				}
				*/
				HTREEITEM		h3	= TreeControl::Item(ba).Image(WT_BATCH).Param(b->addr).Children(1).Insert(tree, h2);
//				PutCommands(rt, h3, DX11DeviceContext_commands, m.slice(boffset - total, b->addr - boffset), offset);
				boffset = b->addr;
				++b;
			}

			total += m.size32();

			if (boffset != total) {
				HTREEITEM		h3	= TreeControl::Item("Orphan").Bold().Children(1).Insert(tree, h2);
				PutCommands(rt, h3, DX11DeviceContext_commands, m.slice(int(boffset - total)), offset, this);
				boffset = total;
			}

		} else {
			HTREEITEM	h2	= TreeControl::Item("DeviceCommands").Bold().Param(total).Insert(tree, h);
			PutCommands(rt, h2, DX11Device_commands, r->recording, total - intptr_t(r->recording.p), this);
			total += r->recording.size32();
		}
	}

	tree.Show();
	tree.Invalidate();
}

void ViewDX11GPU::ExpandTree(HTREEITEM h) {
	RegisterTree		rt(tree, this, format);
	TreeControl::Item	item = tree.GetItem(h, TVIF_IMAGE|TVIF_PARAM);

	switch (item.Image()) {
		case WT_BATCH: {
			int		bi		= GetBatchNumber(item.Param());
			uint32	bstart	= bi ? batches[bi - 1].addr : 0;
			uint32	total	= 0;

			for (Recording *r = recordings.begin(), *re = recordings.end(); r != re; ++r) {
				if (r->type == Recording::CONTEXT) {
					if (!bstart)
						bstart = total;
					if (bstart < total + r->recording.length()) {
						PutCommands(rt, h, DX11DeviceContext_commands, r->recording.slice(bstart - total, batches[bi].addr - bstart), bstart, this);
						return;
					}
				}
				total += r->recording.size32();
			}
			break;
		}
		case WT_CALLSTACK: {
			const uint64*	callstack = item.Param();
			for (int i = 0; i < 32 && callstack[i]; i++)
				rt.Add(h, buffer_accum<1024>() << stack_dumper.GetFrame(callstack[i]), WT_CALLSTACK, callstack[i]);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//	EditorDX11RSX
//-----------------------------------------------------------------------------

class EditorDX11 : public app::EditorGPU, public app::MenuItemCallbackT<EditorDX11>, public Handles2<EditorDX11, AppEvent> {
	filename				fn;
	Menu					recent;
	ref_ptr<DXConnection>	con;
	dynamic_array<DWORD>	pids;

	void	Grab(ViewDX11GPU *view);

	void	TogglePause() {
		con->Call<void>(con->paused ? INTF_Continue : INTF_Pause);
		con->paused = !con->paused;
	}

	virtual bool Matches(const ISO::Browser &b) {
		return b.Is("DX11GPUState") || b.Is<DX11Capture>(ISO::MATCH_NOUSERRECURSE);
	}
	virtual Control Create(app::MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		ViewDX11GPU	*view = new ViewDX11GPU(main, wpos);
		view->Load(p);

		RunThread([view,
			progress = Progress(WindowPos(*view, AdjustRect(view->GetRect().Centre(500, 30), Control::OVERLAPPED | Control::CAPTION, false)), "Collecting assets", view->items.size32())
		]() mutable {
			view->GetAssets(&progress);
			//view->GetStatistics();
			view->MakeTree();
		});
		return *view;
	}

#ifndef ISO_EDITOR
	virtual bool	Command(MainWindow &main, ID id) {
		if ((main.GetDropDown(ID_ORBISCRUDE_GRAB).GetItemStateByID(ID_ORBISCRUDE_GRAB_DX11) & MF_CHECKED) && EditorGPU::Command(main, id))
			return true;

		switch (id) {
			case ID_ORBISCRUDE_PAUSE: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				if (menu.GetItemStateByID(ID_ORBISCRUDE_GRAB_DX11) & MF_CHECKED) {
					TogglePause();
				}
				break;
			}

			case ID_ORBISCRUDE_GRAB: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				if (menu.GetItemStateByID(ID_ORBISCRUDE_GRAB_DX11) & MF_CHECKED) {

					if (!con->process.Active())
						con->OpenProcess(fn, string(main.DescendantByID(ID_EDIT + 2).GetText()), string(main.DescendantByID(ID_EDIT + 1).GetText()), "dx11crude.dll");

					main.SetTitle("new");
					Grab(new ViewDX11GPU(main, main.Dock(DOCK_TAB), con));
				}
				break;
			}
		}
		return false;
	}
#endif

public:
	void operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN) {
			auto	main	= MainWindow::Get();
			Menu	menu	= main->GetDropDown(ID_ORBISCRUDE_GRAB);
			recent			= Menu::Create();
			recent.SetStyle(MNS_NOTIFYBYPOS);

		#if 0
			int		id		= 1;
			for (auto i : GetSettings("DX11/Recent"))
				Menu::Item(i.GetString(), id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
			Menu::Item("New executable...", 0).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
			recent.Separator();

			pids.clear();
			ModuleSnapshot	snapshot(0, TH32CS_SNAPPROCESS);
			id		= 0x1000;
			for (auto i : snapshot.processes()) {
				filename	path;
				if (FindModule(i.th32ProcessID, "dx11crude.dll", path)) {
					pids.push_back(i.th32ProcessID);
					Menu::Item(i.szExeFile, id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
				}
			}
			Menu::Item("Set Executable for DX11", ID_ORBISCRUDE_GRAB_DX11).SubMenu(recent).InsertByPos(menu, 0);
		#else
			auto	cb = new_lambda<MenuCallback>([this](Control c, Menu m) {
				while (m.RemoveByPos(0));

				int		id		= 1;
				for (auto i : GetSettings("DX11/Recent"))
					Menu::Item(i.GetString(), id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(m);
				Menu::Item("New executable...", 0).Param(static_cast<MenuItemCallback*>(this)).AppendTo(m);
				m.Separator();

				pids.clear();
				ModuleSnapshot	snapshot(0, TH32CS_SNAPPROCESS);
				id		= 0x1000;
				for (auto i : snapshot.processes()) {
					filename	path;
					if (FindModule(i.th32ProcessID, "dx11crude.dll", path)) {
						pids.push_back(i.th32ProcessID);
						Menu::Item(i.szExeFile, id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(m);
					}
				}
				});
			Menu::Item("Set Executable for DX11", ID_ORBISCRUDE_GRAB_DX11).SubMenu(recent).Param(cb).InsertByPos(menu, 0);
		#endif
		}
	}

	void	operator()(Control c, Menu::Item i) {
		ISO::Browser2	settings	= GetSettings("DX11/Recent");
		int				id			= i.ID();

		if (id && (GetKeyState(VK_RBUTTON) & 0x8000)) {
			Menu	popup = Menu::Popup();
			popup.Append("Remove", 2);
			int	r = popup.Track(c, GetMousePos(), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD);
			if (r == 2) {
				settings.Remove(id - 1);
				recent.RemoveByID(id);
			}
			return;
		}

		if (id == 0) {
			if (!GetOpen(c, fn, "Open File", "Windows Executable\0*.exe\0"))
				return;
			settings.Append().Set((const char*)fn);
			id	= settings.Count();
			Menu::Item(fn, id).Param(MenuItemCallback(this)).InsertByPos(recent, -2);
		}

		auto	*main	= MainWindow::Cast(c);
		Menu	m		= main->GetDropDown(ID_ORBISCRUDE_GRAB);
		m.RadioDirect(ID_ORBISCRUDE_GRAB_DX11, ID_ORBISCRUDE_GRAB_TARGETS, ID_ORBISCRUDE_GRAB_TARGETS_MAX);
		Menu::Item().Check(true).Type(MFT_RADIOCHECK).SetByPos(m, m.FindPosition(recent));

		recent.RadioDirect(id, 0, recent.Count());
		//CreateReadPipe(pipe);

		con	= new DXConnection;
		const char *dll_name	= "dx11crude.dll";

		if (id & 0x1000) {
			con->OpenProcess(pids[id & 0xfff], dll_name);
			fn = con->process.Filename();

		} else {
			fn = settings[id - 1].GetString();

			string	exec_dir, exec_cmd;
			if (DialogBase home = main->DescendantByID("Home")) {
				exec_cmd = home.ItemText(ID_EDIT + 1);
				exec_dir = home.ItemText(ID_EDIT + 2);
			}

			con->OpenProcess(fn, exec_dir, exec_cmd, dll_name);
			if (resume_after) {
				ISO_OUTPUT("Resuming process\n");
				con->Call<void>(INTF_Continue);
			}
		}
	}

	EditorDX11() {
		ISO::getdef<DX11Capture>();
		ISO::getdef<memory_block>();
	}
} editor_dx11;

void	EditorDX11::Grab(ViewDX11GPU *view) {
	RunThread([this, view,
		progress = Progress(WindowPos(*view, AdjustRect(view->GetRect().Centre(500, 30), Control::OVERLAPPED | Control::CAPTION, false)), "Capturing", 0)
	]() mutable {
		auto	stats = con->Call<CaptureStats>(INTF_CaptureFrames, until_halt ? 0x7ffffff : num_frames);

		if (stats.num_items == 0) {
			view->SendMessage(WM_ISO_ERROR, 0, (LPARAM)"\nFailed to capture");
			return;
		}

		int						counts[RecItem::NUM_TYPES] = {};
		hash_multiset<string>	name_counts;

		auto	items = con->Call<with_size<dynamic_array<RecItem2>>>(INTF_GetItems);
		for (auto &i : items) {
			if (i.name) {
				if (int n = name_counts.insert(i.name))
					i.name << '(' << n << ')';
			} else {
				auto	type = undead(i.type);
				i.name << get_field_name(type) << '#' << counts[type]++;
			}
			view->AddItem(i.type, move(i.name), move(i.info), i.obj);
		}

		progress.Reset("Collecting assets", stats.num_items);

		view->recordings = con->Call<with_size<dynamic_array<Recording>>>(INTF_GetRecordings);

		for (auto &i : view->items) {
			switch (i.type) {
				case RecItem::ShaderResourceView: {
					const RecView<D3D11_SHADER_RESOURCE_VIEW_DESC>	*v = i.info;
					if (!view->FindItem((ID3D11DeviceChild*)v->resource)) {
						auto	r = con->Call<RecItem2>(INTF_GetResource, uintptr_t(i.obj));
						unconst(v->resource) = static_cast<ID3D11Resource*>(r.obj);
						auto	type = undead(r.type);
						r.name << get_field_name(type) << '#' << counts[type]++;
						view->AddItem(r.type, move(r.name), move(r.info), r.obj);
					}
					break;
				}
				case RecItem::RenderTargetView:
				case RecItem::DepthStencilView:
				case RecItem::UnorderedAccessView:
					break;
			}
		}

		for (auto &i : view->items) {
			switch (i.type) {
				case RecItem::Buffer:
				case RecItem::Texture1D:
				case RecItem::Texture2D:
				case RecItem::Texture3D:
					con->Call<malloc_block_all>(INTF_ResourceData, uintptr_t(i.obj)).copy_to(i.info.extend(i.GetSize()));
					break;
			}
			progress.Set(view->items.index_of(i) + 1);
		}

		if (resume_after) {
			ISO_OUTPUT("Resuming process\n");
			con->Call<void>(INTF_Continue);
		} else {
			con->paused = true;
		}

		view->GetAssets(&progress);
		//view->GetStatistics();
		view->MakeTree();
		view->SetFocus();
	});
}


void dx11_dummy() {}
