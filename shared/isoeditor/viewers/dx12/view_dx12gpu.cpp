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
#include "extra/colourise.h"
#include "hashes/fnv.h"

#include "filetypes/3d/model_utils.h"
#include "view_dx12gpu.rc.h"
#include "resource.h"
#include "hook.h"
#include "hook_com.h"
#include "stack_dump.h"
#include "extra/xml.h"
#include "dx12/dx12_record.h"
#include "dx12/dx12_helpers.h"
#include "dx/dxgi_read.h"
#include "dx/sim_dxbc.h"
#include "..\dx_shared\dx_gpu.h"
#include "..\dx_shared\dx_fields.h" 

#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "dxgi1_4.h"

#define	IDR_MENU_DX12GPU	"IDR_MENU_DX12GPU"
using namespace app;
using namespace dx12;

template<> struct field_names<D3D12_HEAP_FLAGS>				{ static field_bit	s[]; };
template<> struct field_names<D3D12_RESOURCE_FLAGS>			{ static field_bit	s[]; };
template<> struct field_names<D3D12_RESOURCE_STATES>		{ static field_bit	s[]; };
template<> struct field_names<D3D12_RESOURCE_BARRIER_FLAGS>	{ static field_bit	s[]; };
template<> struct field_names<D3D12_DSV_FLAGS>				{ static field_bit	s[]; };
template<> struct field_names<D3D12_ROOT_SIGNATURE_FLAGS>	{ static field_bit	s[]; };
template<> struct field_names<D3D12_TILE_COPY_FLAGS>		{ static field_bit	s[]; };
template<> struct field_names<D3D12_TILE_RANGE_FLAGS>		{ static field_bit	s[]; };
template<> struct field_names<D3D12_CLEAR_FLAGS>			{ static field_bit	s[]; };
template<> struct field_names<D3D12_FENCE_FLAGS>			{ static field_bit	s[]; };
template<> struct field_names<D3D12_RENDER_PASS_FLAGS>		{ static field_bit	s[]; };

template<> struct field_names<D3D12_FILTER>					{ static field_value s[]; };
template<> struct field_names<D3D12_RESIDENCY_PRIORITY>		{ static field_value s[]; };


template<> static constexpr bool field_is_struct<D3D12_GPU_DESCRIPTOR_HANDLE> = false;
template<> static constexpr bool field_is_struct<D3D12_CPU_DESCRIPTOR_HANDLE> = false;

template<> struct field_names<D3D12_GPU_DESCRIPTOR_HANDLE>	: field_customs<D3D12_GPU_DESCRIPTOR_HANDLE>	{};
template<> struct field_names<D3D12_CPU_DESCRIPTOR_HANDLE>	: field_customs<D3D12_CPU_DESCRIPTOR_HANDLE>	{};

template<> struct fields<DXGI_FORMAT>						: value_field<DXGI_FORMAT>	{};
template<> struct fields<D3D12_CPU_DESCRIPTOR_HANDLE>		: value_field<D3D12_CPU_DESCRIPTOR_HANDLE>	{};
template<> struct fields<D3D12_WRITEBUFFERIMMEDIATE_MODE>	: value_field<D3D12_WRITEBUFFERIMMEDIATE_MODE>	{};
template<> struct fields<D3D12_TILE_RANGE_FLAGS>			: value_field<D3D12_TILE_RANGE_FLAGS> {};
template<> struct fields<D3D12_RESIDENCY_PRIORITY>			: value_field<D3D12_RESIDENCY_PRIORITY> {};
template<> struct fields<D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>: value_field<D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>	{};
template<> struct fields<D3D12_PRIMITIVE_TOPOLOGY_TYPE>		: value_field<D3D12_PRIMITIVE_TOPOLOGY_TYPE>	{};
template<> struct fields<D3D12_PIPELINE_STATE_FLAGS>		: value_field<D3D12_PIPELINE_STATE_FLAGS>	{};
template<> struct fields<D3D12_SHADING_RATE_COMBINER>		: value_field<D3D12_SHADING_RATE_COMBINER>	{};

//-----------------------------------------------------------------------------
//	DX12Assets
//-----------------------------------------------------------------------------

void MakeDefaultDescriptor(D3D12_SHADER_RESOURCE_VIEW_DESC &view, const D3D12_RESOURCE_DESC &res) {
	clear(view);
	view.Format						= res.Format;
	view.Shader4ComponentMapping	= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	switch (res.Dimension) {
		case D3D12_RESOURCE_DIMENSION_BUFFER:
			view.ViewDimension				= D3D12_SRV_DIMENSION_BUFFER;
			view.Buffer.FirstElement		= 0;
			view.Buffer.NumElements			= res.Width;
			view.Buffer.StructureByteStride	= DXGI_COMPONENTS(res.Format).Size();
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			if (res.DepthOrArraySize == 1) {
				view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE1D;
				view.Texture1D.MipLevels		= res.MipLevels;
			} else {
				view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				view.Texture1DArray.MipLevels	= res.MipLevels;
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (res.DepthOrArraySize == 1) {
				view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2D;
				view.Texture2D.MipLevels		= res.MipLevels;
			} else {
				view.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				view.Texture2DArray.MipLevels	= res.MipLevels;
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			view.ViewDimension			= D3D12_SRV_DIMENSION_TEXTURE3D;
			view.Texture3D.MipLevels	= res.MipLevels;
			break;
	}
}

DESCRIPTOR MakeDefaultDescriptor(RecObject2 *obj) {
	DESCRIPTOR	desc;
	clear(desc);
	desc.type			= DESCRIPTOR::SRV;
	desc.res			= (ID3D12Resource*)obj->obj;
	const RecResource	*r	= obj->info;
	MakeDefaultDescriptor(desc.srv, *r);
	return desc;
}

namespace iso {

template<typename W> bool write(W &w, const D3D12_SHADER_RESOURCE_VIEW_DESC &srv) {
	write(w, srv.Format, srv.ViewDimension, srv.Shader4ComponentMapping);
	switch (srv.ViewDimension) {
		case D3D12_SRV_DIMENSION_BUFFER:			{ auto &t = srv.Buffer;				return write(w, t.FirstElement, t.NumElements, t.StructureByteStride, t.Flags); }
		case D3D12_SRV_DIMENSION_TEXTURE1D:			{ auto &t = srv.Texture1D;			return write(w, t.MostDetailedMip, t.MipLevels, t.ResourceMinLODClamp); }
		case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:	{ auto &t = srv.Texture1DArray;		return write(w, t.MostDetailedMip, t.MipLevels, t.FirstArraySlice, t.ArraySize, t.ResourceMinLODClamp); }
		case D3D12_SRV_DIMENSION_TEXTURE2D:			{ auto &t = srv.Texture2D;			return write(w, t.MostDetailedMip, t.MipLevels, t.PlaneSlice, t.ResourceMinLODClamp); }
		case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:	{ auto &t = srv.Texture2DArray;		return write(w, t.MostDetailedMip, t.MipLevels, t.FirstArraySlice, t.ArraySize, t.PlaneSlice, t.ResourceMinLODClamp); }
		case D3D12_SRV_DIMENSION_TEXTURE2DMS:		return true;
		case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:	{ auto &t = srv.Texture2DMSArray;	return write(w, t.FirstArraySlice, t.ArraySize); }
		case D3D12_SRV_DIMENSION_TEXTURE3D:			{ auto &t = srv.Texture3D;			return write(w, t.MostDetailedMip, t.MipLevels, t.ResourceMinLODClamp); }
		case D3D12_SRV_DIMENSION_TEXTURECUBE:		{ auto &t = srv.TextureCube;		return write(w, t.MostDetailedMip, t.MipLevels, t.ResourceMinLODClamp); }
		case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:	{ auto &t = srv.TextureCubeArray;	return write(w, t.MostDetailedMip, t.MipLevels, t.First2DArrayFace, t.NumCubes, t.ResourceMinLODClamp); }
		default: return false;
	}
}
template<typename W> bool write(W &w, const D3D12_UNORDERED_ACCESS_VIEW_DESC &uav) {
	write(w, uav.Format, uav.ViewDimension);
	switch (uav.ViewDimension) {
		case D3D12_UAV_DIMENSION_BUFFER:			{ auto &t = uav.Buffer;				return write(w, t.FirstElement, t.NumElements, t.StructureByteStride, t.CounterOffsetInBytes, t.Flags); }
		case D3D12_UAV_DIMENSION_TEXTURE1D:			{ auto &t = uav.Texture1D;			return write(w, t.MipSlice); }
		case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:	{ auto &t = uav.Texture1DArray;		return write(w, t.MipSlice, t.FirstArraySlice, t.ArraySize); }
		case D3D12_UAV_DIMENSION_TEXTURE2D:			{ auto &t = uav.Texture2D;			return write(w, t.MipSlice, t.PlaneSlice); }
		case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:	{ auto &t = uav.Texture2DArray;		return write(w, t.MipSlice, t.FirstArraySlice, t.ArraySize, t.PlaneSlice); }
		case D3D12_UAV_DIMENSION_TEXTURE3D:			{ auto &t = uav.Texture3D;			return write(w, t.MipSlice, t.FirstWSlice, t.WSize); }
		default: return false;
	}
}
template<typename W> bool write(W &w, const D3D12_RENDER_TARGET_VIEW_DESC &rtv) {
	write(w, rtv.Format, rtv.ViewDimension);
	switch (rtv.ViewDimension) {
		case D3D12_RTV_DIMENSION_BUFFER:			{ auto &t = rtv.Buffer;				return write(w, t.FirstElement, t.NumElements); }
		case D3D12_RTV_DIMENSION_TEXTURE1D:			{ auto &t = rtv.Texture1D;			return write(w, t.MipSlice); }
		case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:	{ auto &t = rtv.Texture1DArray;		return write(w, t.MipSlice, t.FirstArraySlice, t.ArraySize); }
		case D3D12_RTV_DIMENSION_TEXTURE2D:			{ auto &t = rtv.Texture2D;			return write(w, t.MipSlice, t.PlaneSlice); }
		case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:	{ auto &t = rtv.Texture2DArray;		return write(w, t.MipSlice, t.FirstArraySlice, t.ArraySize, t.PlaneSlice); }
		case D3D12_RTV_DIMENSION_TEXTURE2DMS:		return true;
		case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:	{ auto &t = rtv.Texture2DMSArray;	return write(w, t.FirstArraySlice, t.ArraySize); }
		case D3D12_RTV_DIMENSION_TEXTURE3D:			{ auto &t = rtv.Texture3D;			return write(w, t.MipSlice, t.FirstWSlice, t.WSize); }
		default: return false;
	}
}
template<typename W> bool write(W &w, const D3D12_DEPTH_STENCIL_VIEW_DESC &dsv) {
	write(w, dsv.Format, dsv.ViewDimension, dsv.Flags);
	switch (dsv.ViewDimension) {
		case D3D12_DSV_DIMENSION_TEXTURE1D:			{ auto &t = dsv.Texture1D;			return write(w, t.MipSlice); }
		case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:	{ auto &t = dsv.Texture1DArray;		return write(w, t.MipSlice, t.FirstArraySlice, t.ArraySize); }
		case D3D12_DSV_DIMENSION_TEXTURE2D:			{ auto &t = dsv.Texture2D;			return write(w, t.MipSlice); }
		case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:	{ auto &t = dsv.Texture2DArray;		return write(w, t.MipSlice, t.FirstArraySlice, t.ArraySize); }
		case D3D12_DSV_DIMENSION_TEXTURE2DMS:		return true;
		case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:	{ auto &t = dsv.Texture2DMSArray;	return write(w, t.FirstArraySlice, t.ArraySize); }
		default: return false;
	}
}
template<typename W> bool write(W &w, const D3D12_CONSTANT_BUFFER_VIEW_DESC &cbv) {
	return write(w, cbv.BufferLocation, cbv.SizeInBytes);
}
template<typename W> bool write(W &w, const DESCRIPTOR &d) {
	write(w, d.type, d.res);
	switch (d.type) {
		case DESCRIPTOR::SRV:	return w.write(d.srv);
		case DESCRIPTOR::CBV:	return w.write(d.cbv);
		case DESCRIPTOR::UAV:	return w.write(d.uav);
		case DESCRIPTOR::RTV:	return w.write(d.rtv);
		case DESCRIPTOR::DSV:	return w.write(d.dsv);
	}
	return true;
}

uint64 hash(const DESCRIPTOR &d) {
	hash_stream<FNV<64>>	fnv;
	fnv.write(d);
	return fnv;
}

}

const Cursor DX12cursors[] {
	Cursor::LoadSystem(IDC_HAND),
	CompositeCursor(Cursor::LoadSystem(IDC_HAND), Cursor::Load(IDR_OVERLAY_ADD, 0)),
};

struct DX12Assets {
	enum {
		WT_NONE,
		WT_BATCH,
		WT_MARKER,
		WT_CALLSTACK,
		WT_REPEAT,
		WT_OBJECT,
	};

	struct MarkerRecord : interval<uint32> {
		string		str;
		uint32		colour:24, flags:8;
		MarkerRecord(string &&str, uint32 start, uint32 end, uint32 colour) : interval<uint32>(start, end), str(move(str)), colour(colour), flags(0) {}
	};
	struct CallRecord {
		uint32		addr, dest;
		CallRecord(uint32 _addr, uint32 _dest) : addr(_addr), dest(_dest) {}
		bool operator<(uint32 offset) const { return dest < offset; }
	};

	struct ShaderRecord {
		dx::SHADERSTAGE		stage;
		malloc_block		data;
		uint64				addr;
		string				name;
		string				GetName()		const	{ return name; }
		uint64				GetBase()		const	{ return addr; }
		uint64				GetSize()		const	{ return data.length(); }
	};

	struct ObjectRecord : RecObject2 {
		int					index;
		ObjectRecord(RecObject::TYPE type, string16 &&name, void *obj) : RecObject2(type, move(name), obj), index(-1) {}
		string				GetName()		const	{ return str8(name); }
		uint64				GetBase()		const	{ return uint64(obj); }
		uint64				GetSize()		const	{ return info.length(); }
	};

	struct ResourceRecord {
		ObjectRecord		*obj;
		const RecResource*	res()			const	{ return obj->info; }

		string				GetName()		const	{ return str8(obj->name); }
		uint64				GetBase()		const	{ return res()->gpu; }
		uint64				GetSize()		const	{ return res()->data_size; }
		ResourceRecord(ObjectRecord *_obj = 0) : obj(_obj) {}
	};

	struct DescriptorHeapRecord {
		ObjectRecord		*obj;
		DescriptorHeapRecord(ObjectRecord *_obj) : obj(_obj) {}
		const RecDescriptorHeap	*operator->()	const { return obj->info; }
		operator const RecDescriptorHeap*()		const { return obj->info; }
	};

	struct BatchInfo {
		RecCommandList::tag	op;
		union {
		struct {
			uint32			prim:8, instance_count:18;
			uint32			vertex_count;
			uint32			vertex_offset, instance_offset, index_offset;
		} draw;
		struct {
			uint32			dim_x, dim_y, dim_z;
		} compute;
		struct {
			uint32			command_count;
			ObjectRecord	*signature;
			ObjectRecord	*arguments;
			uint64			arguments_offset;
			ObjectRecord	*counts;
			uint64			counts_offset;
		} indirect;
		struct {
			uint32			dim_x, dim_y, dim_z;
			D3D12_GPU_VIRTUAL_ADDRESS_RANGE				RayGenerationShaderRecord;
			D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	MissShaderTable;
			D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	HitGroupTable;
			D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	CallableShaderTable;
		} rays;
	}; };

	struct BatchRecord : BatchInfo {
		uint32		start, end;
		uint32		use_offset;
		uint64		timestamp;
		D3D12_QUERY_DATA_PIPELINE_STATISTICS	stats;
		BatchRecord(const BatchInfo &info, uint32 start, uint32 end, uint32 use_offset) : BatchInfo(info), start(start), end(end), use_offset(use_offset), timestamp(0) {}	
		D3D_PRIMITIVE_TOPOLOGY	getPrim()	const { return (D3D_PRIMITIVE_TOPOLOGY)draw.prim; }
		uint64					Duration()	const { return this[1].timestamp - timestamp; }
	};

	struct use {
		union {
			struct {uint32 index:28, type:4; };
			uint32	u;
		};
		RecObject::TYPE	Type() const { return RecObject::TYPE(type); }
		use(const ShaderRecord*, uint32 _index) : index(_index), type(RecObject::Shader) {}
		use(const ObjectRecord *p, uint32 _index) : index(_index), type(p->type) {}
		use(uint32 _u) : u(_u) {}
	};

	filename								shader_path;
	dynamic_array<MarkerRecord>				markers;
	dynamic_array<use>						uses;
	dynamic_array<BatchRecord>				batches;

	dynamic_array<ObjectRecord>				objects;
	dynamic_array<ResourceRecord>			resources;
	dynamic_array<ShaderRecord>				shaders;
	dynamic_array<DescriptorHeapRecord>		descriptor_heaps;
	ObjectRecord*							device_object;

	hash_map<const void*, dynamic_array<use>>	pipeline_uses;
	hash_map<DESCRIPTOR, ISO_ptr_machine<void>>	views;
	dynamic_array<CallRecord>				calls;

	hash_map<uint64, ObjectRecord*>			object_map;
	interval_tree<uint64, ObjectRecord*>	vram_tree;

	DX12Assets() : device_object(0) {}

	void			AddCall(uint32 addr, uint32 dest) {
		calls.emplace_back(addr, dest);
	}

	BatchRecord*	AddBatch(const BatchInfo &info, uint32 start, uint32 end) {
		return &batches.emplace_back(info, start, end, uses.size32());
	}

	int				AddMarker(string16 &&s, uint32 start, uint32 end, uint32 col) {
		markers.emplace_back(move(s), start, end, col);
		return markers.size32() - 1;
	}

	ShaderRecord*	AddShader(const D3D12_SHADER_BYTECODE &b, dx::SHADERSTAGE stage, memory_interface *mem);

	ShaderRecord*	AddShader(dx::SHADERSTAGE stage, string16 &&name, malloc_block &&data, uint64 addr) {
		ShaderRecord *r = new(shaders) ShaderRecord;
		r->stage	= stage;
		r->addr		= addr;
		r->data		= move(data);
		r->name		= move(name);
		return r;
	}

	const BatchRecord*	GetBatchByTime(uint64 timestamp) const {
		return first_not(batches, [timestamp](const BatchRecord &r) { return r.timestamp < timestamp; });
	}
	const BatchRecord*	GetBatch(uint32 addr) const {
		return first_not(batches, [addr](const BatchRecord &r) { return r.end < addr; });
	}
	const MarkerRecord*	GetMarker(uint32 addr) const {
		return first_not(markers, [addr](const MarkerRecord &r) { return r.a < addr; });
	}

	range<use*>			GetUsage(uint32 from_batch, uint32 to_batch) const {
		if (from_batch > to_batch)
			swap(from_batch, to_batch);
		return make_range(uses + batches[from_batch].use_offset, uses + batches[min(to_batch + 1, batches.size())].use_offset);
	}
	range<use*>			GetUsage(uint32 batch) const {
		return GetUsage(batch, batch);
	}

	template<typename T> dynamic_array<T>&	GetTable();
	template<typename T> int GetIndex(const T *e) {
		return e - GetTable<T>();
	}

	template<typename T> void AddUse(const T *e, dynamic_array<use> &uses) {
		if (e)
			uses.emplace_back(e, GetIndex(e));
	}
	template<typename T> void AddUse(const T *e) {
		if (e)
			uses.emplace_back(e, GetIndex(e));
	}

	ObjectRecord*		FindObject(uint64 addr) {
		if (auto *p = object_map.check(addr))
			return *p;
		return 0;
	}
	ShaderRecord*		FindShader(uint64 addr) {
		for (auto &i : shaders) {
			if (i.GetBase() == addr)
				return &i;
		}
		return 0;
	}
	ObjectRecord*		FindByGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS a) {
		auto	i = vram_tree.lower_bound(a);
		return a >= i.key().a ? *i : 0;
	}
	const RecResource*	FindRecResource(ID3D12Resource *res) {
		if (auto *obj = FindObject((uint64)res))
			return obj->info;
		return 0;
	}
	const DESCRIPTOR*	FindDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : descriptor_heaps) {
			if (const DESCRIPTOR *d = i->holds(h))
				return d;
		}
		return 0;
	}
	const DESCRIPTOR*	FindDescriptor(D3D12_GPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : descriptor_heaps) {
			if (const DESCRIPTOR *d = i->holds(h))
				return d;
		}
		return 0;
	}

	ObjectRecord	*AddObject(RecObject::TYPE type, string16 &&name, malloc_block &&info, void *obj, dx::cache_type &cache, memory_interface *mem);
	void operator()(string_accum &sa, const field *pf, const uint32le *p, uint32 offset);
	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr);

	auto ObjectName(const DESCRIPTOR *d) {
		return [=](string_accum &a) {
			if (d) {
				if (auto obj = FindObject((uint64)d->res))
					a << " (" << obj->name << ')';
			}
		};
	}

	auto ObjectName(D3D12_GPU_DESCRIPTOR_HANDLE h) {
		return [=](string_accum &a) {
			for (auto &i : descriptor_heaps) {
				if (const DESCRIPTOR *d = i->holds(h)) {
					a << " (" << i.obj->name << '[' << i->index(d) << "])";
					break;
				}
			}
		};

	}
	auto ObjectName(D3D12_CPU_DESCRIPTOR_HANDLE h) {
		return [=](string_accum &a) {
			for (auto &i : descriptor_heaps) {
				if (const DESCRIPTOR *d = i->holds(h)) {
					a << " (" << i.obj->name << '[' << i->index(d) << "])";
					break;
				}
			}
		};

	}
};

DX12Assets::ShaderRecord*	DX12Assets::AddShader(const D3D12_SHADER_BYTECODE &b, dx::SHADERSTAGE stage, memory_interface *mem) {
	if (!b.pShaderBytecode || !b.BytecodeLength)
		return 0;
	ShaderRecord *r = FindShader((uint64)b.pShaderBytecode);
	if (!r) {
		r			= new(shaders) ShaderRecord;
		r->stage	= stage;
		r->addr		= (uint64)b.pShaderBytecode;
		if (mem)
			r->data = mem->get((uint64)b.pShaderBytecode, b.BytecodeLength);

		if (dx::DXBC *dxbc = r->data) {
			if (auto spdb = dxbc->GetBlob(dx::DXBC::ShaderPDB)) {
			#if 1
				r->name = GetPDBFirstFilename(memory_reader(spdb));
			#else
				auto	parsed = ParsedSPDB(memory_reader(spdb));
				auto &start = *parsed.locations.begin();
				r->name	= parsed.entry << '(' << parsed.files[start.file]->name << ')';
			#endif
			}
		}
	}
	return r;
}

DX12Assets::ObjectRecord* DX12Assets::AddObject(RecObject::TYPE type, string16 &&name, malloc_block &&info, void *obj, dx::cache_type &cache, memory_interface *mem) {
	ObjectRecord	*p	= new (objects) ObjectRecord(type, move(name), obj);
	p->info	= move(info);
	object_map[uint64(obj)] = p;
	switch (p->type) {
		case RecObject::Device:
			device_object			= p;
			break;

		case RecObject::GraphicsCommandList:
			if (mem) {
				RecCommandList	*r	= p->info;
				auto	ext = p->info.extend(r->buffer.length());
				r	= p->info;
				mem->get(ext, uint64(r->buffer.p));
				r->buffer.p = ext;
			} else {
				RecCommandList	*r	= p->info;
				r->buffer.p = r + 1;
			}
			break;

		case RecObject::Resource: {
			p->index		= resources.size32();
			RecResource	*r	= p->info;
			switch (r->alloc) {
				case RecResource::Committed:
					vram_tree.insert(r->gpu, r->gpu + r->data_size, p);
					break;

				case RecResource::Reserved: {
					uint32	ntiles = 1;
					uint64	addr	= (uint64)r->mapping;
					r->mapping		= new TileMapping[ntiles];
					if (mem)
						mem->get(r->mapping, ntiles * sizeof(TileMapping), addr);
					break;
				}
			}

			resources.emplace_back(p);
			break;
		}

		case RecObject::DescriptorHeap: {
			p->index		= descriptor_heaps.size32();
			descriptor_heaps.push_back(p);
			break;
		}

		case RecObject::PipelineState: {
			auto	uses	= pipeline_uses[p->info];
			switch (*(int*)p->info) {
				case 0: {
					auto	t = rmap_unique<KM, D3D12_GRAPHICS_PIPELINE_STATE_DESC>(p->info + 8);
					AddUse(AddShader(t->VS, dx::VS, mem), uses);
					AddUse(AddShader(t->PS, dx::PS, mem), uses);
					AddUse(AddShader(t->DS, dx::DS, mem), uses);
					AddUse(AddShader(t->HS, dx::HS, mem), uses);
					AddUse(AddShader(t->GS, dx::GS, mem), uses);
					break;
				}
				case 1: {
					const D3D12_COMPUTE_PIPELINE_STATE_DESC	*t	= p->info + 8;
					AddUse(AddShader(t->CS, dx::CS, mem), uses);
					break;
				}
				case 2: {
					auto	t = rmap_unique<KM, D3D12_PIPELINE_STATE_STREAM_DESC>(p->info + 8);
					for (auto &sub : make_next_range<D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT>(const_memory_block(t->pPipelineStateSubobjectStream, t->SizeInBytes))) {
						switch (sub.t.u.t) {
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS>(sub), dx::VS, mem), uses); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>(sub), dx::PS, mem), uses); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS>(sub), dx::DS, mem), uses); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS>(sub), dx::HS, mem), uses); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS>(sub), dx::GS, mem), uses); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>(sub), dx::CS, mem), uses); break;
								//case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>(sub), dx::AS, mem), uses); break;
								//case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:	AddUse(AddShader(get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>(sub), dx::MS, mem), uses); break;
							default:	break;
						}
					}
					break;
				}
			}
			break;
		}
		default:
			break;
	}
	return p;
}

void DX12Assets::operator()(string_accum &sa, const field *pf, const uint32le *p, uint32 offset) {
	uint64		v = pf->get_raw_value(p, offset);
	if (auto* rec = FindObject(v))
		sa << rec->name;
	else
		sa << "(unfound)0x" << hex(v);
}

bool SubTree(RegisterTree &rt, HTREEITEM h, const_memory_block mem, intptr_t offset, DX12Assets &assets);

void DX12Assets::operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
	buffer_accum<256>	ba;
	PutFieldName(ba, tree.format, pf);

	uint64		v = pf->get_raw_value(p, offset);

	if (pf->is_type<D3D12_GPU_DESCRIPTOR_HANDLE>()) {
		D3D12_GPU_DESCRIPTOR_HANDLE h;
		h.ptr = v;
		ba << "0x" << hex(v) << ObjectName(h);

	} else if (pf->is_type<D3D12_CPU_DESCRIPTOR_HANDLE>()) {
		D3D12_CPU_DESCRIPTOR_HANDLE h;
		h.ptr = v;
		ba << "0x" << hex(v) << ObjectName(h);

	} else if (v == 0) {
		ba << "nil";

	} else if (auto *rec = FindObject(v)) {
		tree.Add(h, ba << rec->name, WT_OBJECT, rec);
		return;

		if (pf == fields<CommandRange>::f) {
			const CommandRange	*r	= (const CommandRange*)p;
			SubTree(tree, h, rec->info.slice(r->begin(), r->extent()), rec->index - intptr_t(rec->info.p), *this);
		}

	} else if (auto *rec = FindShader(v)) {
		ba << rec->name;

	} else {
		ba << "(unfound)0x" << hex(v);
	}
	tree.AddText(h, ba, addr);
}

template<>	constexpr interval<uint32>::interval(const _none&)			: a(iso::maximum), b(-iso::maximum)	{}

interval<uint32> ResolveRange(const CommandRange &r, DX12Assets &assets) {
	if (!r.empty()) {
		if (auto *obj = assets.FindObject(uint64(&*r)))
			return r + obj->index;
	}
	return empty;
}

const_memory_block GetCommands(const CommandRange &r, DX12Assets &assets, uint32 &addr) {
	if (!r.empty()) {
		if (auto *obj = assets.FindObject(uint64(&*r))) {
			addr -= obj->index + r.begin();
			const RecCommandList	*rec	= obj->info;
			return rec->buffer.slice(r.begin(), min(addr, r.extent()));
		}
	}
	return empty;
}
const_memory_block GetCommands(const CommandRange &r, DX12Assets &assets) {
	if (!r.empty()) {
		if (auto *obj = assets.FindObject(uint64(&*r))) {
			const RecCommandList	*rec	= obj->info;
			return rec->buffer.slice(r.begin(), r.extent());
		}
	}
	return empty;
}

const_memory_block GetCommands(uint32 offset, DX12Assets &assets) {
	if (offset < assets.device_object->info.size32())
		return assets.device_object->info.slice(offset);

	for (auto &o : assets.objects) {
		if (o.type == RecObject::GraphicsCommandList) {
			const RecCommandList	*rec = o.info;
			if (offset - o.index < rec->buffer.size32())
				return rec->buffer.slice(offset - o.index);
		}
	}
	return empty;
}

template<> dynamic_array<DX12Assets::ShaderRecord>&	DX12Assets::GetTable()	{ return shaders; }
template<> dynamic_array<DX12Assets::ObjectRecord>&	DX12Assets::GetTable()	{ return objects; }

template<typename T> uint32 GetSize(const T &t)		{ return t.GetSize(); }

template<> const char *field_names<RecObject::TYPE>::s[]	= {
	"Unknown",
	"RootSignature",
	"Heap",
	"Resource",
	"CommandAllocator",
	"Fence",
	"PipelineState",
	"DescriptorHeap",
	"QueryHeap",
	"CommandSignature",
	"GraphicsCommandList",
	"CommandQueue",
	"Device",
	"HANDLE",
};

template<> const char *field_names<RecResource::Allocation>::s[]	= {
	"Unknown", "Reserved", "Placed", "Committed",
};

template<> field	fields<RecResource>::f[] = {
	field::call<D3D12_RESOURCE_DESC>(0, 0),
	field::make<RecResource>("alloc",	&RecResource::alloc),
	field::make<RecResource>("gpu",		&RecResource::gpu),
	0,
};

template<> field	fields<RecHeap>::f[] = {
	field::call<D3D12_HEAP_DESC>(0, 0),
	field::make<RecHeap>("gpu",	&RecHeap::gpu),
	0,
};

struct RecPipelineState;
template<> field	fields<RecPipelineState>::f[] = {
	field::make<int>("type",	0),
	field::call_union<
		D3D12_GRAPHICS_PIPELINE_STATE_DESC,
		D3D12_COMPUTE_PIPELINE_STATE_DESC,
		map_t<KM, D3D12_PIPELINE_STATE_STREAM_DESC>
	>(0, 64, 1),
	0,
};

template<> field	fields<DX12Assets::ObjectRecord>::f[] = {
	field::make("type",	container_cast<DX12Assets::ObjectRecord>(&DX12Assets::ObjectRecord::type)),
	//field::make2<DX12Assets::ObjectRecord>("obj",	&DX12Assets::ObjectRecord::obj),
	field::make("obj",	element_cast<uint64>(container_cast<DX12Assets::ObjectRecord>(&DX12Assets::ObjectRecord::obj))),
	field::call_union<
		void,					// Unknown,
		void,					// RootSignature,
		RecHeap*,				// Heap,
		RecResource*,			// Resource,
		void,					// CommandAllocator,
		void,					// Fence,
		RecPipelineState*,		// PipelineState,
		void,					// DescriptorHeap,
		void,					// QueryHeap,
		map_t<KM, D3D12_COMMAND_SIGNATURE_DESC>*,	// CommandSignature,
		void,					// GraphicsCommandList,
		void,					// CommandQueue,
		void					// Device,
	>(0, T_get_member_offset(&DX12Assets::ObjectRecord::info) * 8, 2),
	0,
};

template<> field	fields<DX12Assets::ResourceRecord>::f[] = {
	field::make<DX12Assets::ResourceRecord>(0,	&DX12Assets::ResourceRecord::obj),
	0,
};

template<> field	fields<DX12Assets::ShaderRecord>::f[] = {
	field::make<DX12Assets::ShaderRecord>("addr",	&DX12Assets::ShaderRecord::addr),
	field::make<DX12Assets::ShaderRecord>("stage",	&DX12Assets::ShaderRecord::stage),
	0,
};
//-----------------------------------------------------------------------------
//	D3D12_COMMAND_SIGNATURE_DESC
//-----------------------------------------------------------------------------

const C_type *GetSignatureType(const D3D12_COMMAND_SIGNATURE_DESC *desc) {
	C_type_struct	comp;
	auto	uint32_type	= ctypes.get_type<uint32>();
	auto	uint64_type	= ctypes.get_type<uint64>();

	for (auto &i : make_range_n(desc->pArgumentDescs, desc->NumArgumentDescs)) {
		switch (i.Type) {
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
				comp.add("VertexCountPerInstance", uint32_type);
				comp.add("VertexCountPerInstance", uint32_type);
				comp.add("InstanceCount", uint32_type);
				comp.add("StartIndexLocation", uint32_type);
				comp.add("StartInstanceLocation", uint32_type);
				break;

			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
				comp.add("IndexCountPerInstance", uint32_type);
				comp.add("InstanceCount", uint32_type);
				comp.add("StartIndexLocation", uint32_type);
				comp.add("BaseVertexLocation", uint32_type);
				comp.add("StartInstanceLocation", uint32_type);
				break;

			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
				comp.add("ThreadGroupCountX", uint32_type);
				comp.add("ThreadGroupCountY", uint32_type);
				comp.add("ThreadGroupCountZ", uint32_type);
				break;

			case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
				comp.add("BufferLocation", uint64_type);
				comp.add("SizeInBytes", uint32_type);
				comp.add("StrideInBytes", uint32_type);
				break;

			case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
				comp.add("BufferLocation", uint64_type);
				comp.add("SizeInBytes", uint32_type);
				//DXGI_FORMAT Format;
				break;

			case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
				comp.add("Value", uint32_type);
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
				comp.add("BufferLocation", uint64_type);
				comp.add("SizeInBytes", uint32_type);
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
				//D3D12_SHADER_RESOURCE_VIEW_DESC;
				break;

			case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
				//D3D12_UNORDERED_ACCESS_VIEW_DESC;
				break;
		}
	}
	return ctypes.add(move(comp));
}

auto IndirectFields = union_fields<
	D3D12_DRAW_ARGUMENTS,
	D3D12_DRAW_INDEXED_ARGUMENTS,
	D3D12_DISPATCH_ARGUMENTS,
	D3D12_VERTEX_BUFFER_VIEW,
	D3D12_INDEX_BUFFER_VIEW,
	uint64,
	D3D12_CONSTANT_BUFFER_VIEW_DESC,
	D3D12_SHADER_RESOURCE_VIEW_DESC,
	D3D12_UNORDERED_ACCESS_VIEW_DESC,
	D3D12_DISPATCH_RAYS_DESC,
	D3D12_DISPATCH_MESH_ARGUMENTS
>::p;

uint8 IndirectSizes[] = {
	(uint8)sizeof(D3D12_DRAW_ARGUMENTS),
	(uint8)sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
	(uint8)sizeof(D3D12_DISPATCH_ARGUMENTS),
	(uint8)sizeof(D3D12_VERTEX_BUFFER_VIEW),
	(uint8)sizeof(D3D12_INDEX_BUFFER_VIEW),
	(uint8)sizeof(uint64),
	(uint8)sizeof(D3D12_CONSTANT_BUFFER_VIEW_DESC),
	(uint8)sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC),
	(uint8)sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC),
	(uint8)sizeof(D3D12_DISPATCH_RAYS_DESC),
	(uint8)sizeof(D3D12_DISPATCH_MESH_ARGUMENTS)
};

bool IsGraphics(const D3D12_COMMAND_SIGNATURE_DESC *desc) {
	for (auto &i : make_range_n(desc->pArgumentDescs, desc->NumArgumentDescs)) {
		if (i.Type == D3D12_INDIRECT_ARGUMENT_TYPE_DRAW || i.Type == D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED)
			return true;
		if (i.Type == D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH)
			return false;
	}
	return false;
}

DX12Assets::BatchInfo GetCommand(const D3D12_COMMAND_SIGNATURE_DESC *desc, const void *args) {
	DX12Assets::BatchInfo	info;
	clear(info);
	uint32	offset = 0;
	for (auto &i : make_range_n(desc->pArgumentDescs, desc->NumArgumentDescs)) {
		switch (i.Type) {
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW: {
				auto p = (const D3D12_DRAW_ARGUMENTS*)((const char*)args + offset);
				info.op	= RecCommandList::tag_DrawInstanced;
				info.draw.instance_count	= p->InstanceCount;
				info.draw.vertex_count		= p->VertexCountPerInstance;
				info.draw.vertex_offset		= p->StartInstanceLocation;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED: {
				auto p = (const D3D12_DRAW_INDEXED_ARGUMENTS*)((const char*)args + offset);
				info.op	= RecCommandList::tag_DrawIndexedInstanced;
				info.draw.instance_count	= p->InstanceCount;
				info.draw.vertex_count		= p->IndexCountPerInstance;
				info.draw.vertex_offset		= p->StartInstanceLocation;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH: {
				auto p = (const D3D12_DISPATCH_ARGUMENTS*)((const char*)args + offset);
				info.op	= RecCommandList::tag_Dispatch;
				info.compute.dim_x	= p->ThreadGroupCountX;
				info.compute.dim_y	= p->ThreadGroupCountY;
				info.compute.dim_z	= p->ThreadGroupCountZ;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:{
				auto p = (const D3D12_DISPATCH_RAYS_DESC*)((const char*)args + offset);
				info.op	= RecCommandList::tag_DispatchRays;
				break;
			}
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH: {
				auto p = (const D3D12_DISPATCH_MESH_ARGUMENTS*)((const char*)args + offset);
				info.op	= RecCommandList::tag_DispatchMesh;
				info.compute.dim_x	= p->ThreadGroupCountX;
				info.compute.dim_y	= p->ThreadGroupCountY;
				info.compute.dim_z	= p->ThreadGroupCountZ;
				break;
			}
		}
		offset += IndirectSizes[i.Type];
	}
	return info;
}


//-----------------------------------------------------------------------------
//	DX12State
//-----------------------------------------------------------------------------
struct RootState {
	com_ptr<ID3D12RootSignatureDeserializer>	deserializer;
	dynamic_array<uint32>						offset;
	malloc_block								data;

	static DESCRIPTOR PointerDescriptor(DESCRIPTOR::TYPE type, D3D12_GPU_VIRTUAL_ADDRESS ptr) {
		DESCRIPTOR	d(type);
		d.ptr	= ptr;
		return d;
	}

	void				SetRootSignature(DX12Assets *assets, ID3D12RootSignature *p);
	DESCRIPTOR			GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const RecDescriptorHeap *dh) const;
	arbitrary_ptr		Slot(int i)			{ return data + offset[i]; }
	arbitrary_const_ptr	Slot(int i) const	{ return data + offset[i]; }
};

void RootState::SetRootSignature(DX12Assets *assets, ID3D12RootSignature *p) {
	deserializer.clear();
	if (auto *obj2 = assets->FindObject(uint64(p)))
		D3D12CreateRootSignatureDeserializer(obj2->info, obj2->info.length(), deserializer.uuid(), (void**)&deserializer);

	const D3D12_ROOT_SIGNATURE_DESC	*root_desc = deserializer->GetRootSignatureDesc();
	offset.resize(root_desc->NumParameters);

	uint32	o	= root_desc->NumStaticSamplers * sizeof(DESCRIPTOR);
	for (int i = 0; i < root_desc->NumParameters; i++) {
		offset[i] = o;
		const D3D12_ROOT_PARAMETER	&p = root_desc->pParameters[i];
		if (p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
			o += p.Constants.Num32BitValues * sizeof(uint32);
		else
			o += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);
	}
	data.resize(o);

	DESCRIPTOR	*d = data;
	for (int i = 0; i < root_desc->NumStaticSamplers; i++, d++) {
		clear(*d);
		d->type = DESCRIPTOR::SSMP;
		d->ssmp = root_desc->pStaticSamplers[i];
	}
}



DESCRIPTOR RootState::GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, const RecDescriptorHeap *dh) const {
	static const uint8	visibility[] = {
		0xff,
		1 << dx::VS,
		1 << dx::HS,
		1 << dx::DS,
		1 << dx::GS,
		1 << dx::PS,
	};

	const D3D12_ROOT_SIGNATURE_DESC	*root_desc	= deserializer->GetRootSignatureDesc();

	if (type == DESCRIPTOR::SMP) {
		for (int i = 0; i < root_desc->NumStaticSamplers; i++) {
			const D3D12_STATIC_SAMPLER_DESC	&p = root_desc->pStaticSamplers[i];
			if (visibility[p.ShaderVisibility] & (1 << stage) && p.ShaderRegister == bind && p.RegisterSpace == space)
				return ((const DESCRIPTOR*)data)[i];
		}
	}

	for (int i = 0; i < root_desc->NumParameters; i++) {
		const D3D12_ROOT_PARAMETER	&p = root_desc->pParameters[i];
		if (visibility[p.ShaderVisibility] & (1 << stage)) {

			switch (p.ParameterType) {
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					if (dh) {
						uint32	start = 0;
						for (int j = 0; j < p.DescriptorTable.NumDescriptorRanges; j++) {
							const D3D12_DESCRIPTOR_RANGE	&range = p.DescriptorTable.pDescriptorRanges[j];

							if (range.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
								start = range.OffsetInDescriptorsFromTableStart;

							if (range.RangeType == as<D3D12_DESCRIPTOR_RANGE_TYPE>(type) && bind >= range.BaseShaderRegister && bind < range.BaseShaderRegister + range.NumDescriptors) {
								if (const DESCRIPTOR *d = dh->holds(*(const D3D12_GPU_DESCRIPTOR_HANDLE*)Slot(i)))
									return d[bind - range.BaseShaderRegister + start];
							}
							start += range.NumDescriptors;
						}
					}
					break;

				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
					if (type == DESCRIPTOR::CBV && p.Descriptor.ShaderRegister == bind) {
						DESCRIPTOR	d(DESCRIPTOR::IMM);
						d.imm	= Slot(i);
						return d;
					}
					break;

				case D3D12_ROOT_PARAMETER_TYPE_CBV:
					if (type == DESCRIPTOR::CBV && p.Descriptor.ShaderRegister == bind)
						return PointerDescriptor(DESCRIPTOR::PCBV, *(const D3D12_GPU_VIRTUAL_ADDRESS*)Slot(i));
					break;

				case D3D12_ROOT_PARAMETER_TYPE_SRV:
					if (type == DESCRIPTOR::SRV && p.Descriptor.ShaderRegister == bind)
						return PointerDescriptor(DESCRIPTOR::PSRV, *(const D3D12_GPU_VIRTUAL_ADDRESS*)Slot(i));
					break;

				case D3D12_ROOT_PARAMETER_TYPE_UAV:
					if (type == DESCRIPTOR::UAV && p.Descriptor.ShaderRegister == bind)
						return PointerDescriptor(DESCRIPTOR::PUAV, *(const D3D12_GPU_VIRTUAL_ADDRESS*)Slot(i));
					break;
			}
		}
	}
	return DESCRIPTOR();
}

struct DX12PipelineBase {
	RootState					root;
	UINT						NodeMask	= 0;
	D3D12_CACHED_PIPELINE_STATE CachedPSO	= {0, 0};
	D3D12_PIPELINE_STATE_FLAGS	Flags		= D3D12_PIPELINE_STATE_FLAG_NONE;

	constexpr operator const void*() const { return CachedPSO.pCachedBlob; }
};

struct DX12ComputePipeline : DX12PipelineBase {
	D3D12_SHADER_BYTECODE		CS = {0, 0};

	void	Set(DX12Assets *assets, const D3D12_COMPUTE_PIPELINE_STATE_DESC *d) {
		root.SetRootSignature(assets, d->pRootSignature);
		CS			= d->CS;
		NodeMask	= d->NodeMask;
		CachedPSO	= d->CachedPSO;
		Flags		= d->Flags;
	}

	void Set(DX12Assets *assets, const D3D12_PIPELINE_STATE_STREAM_DESC *d) {
		*this = DX12ComputePipeline();//clear();
		for (auto &sub : make_next_range<D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT>(const_memory_block(d->pPipelineStateSubobjectStream, d->SizeInBytes))) {
			switch (sub.t.u.t) {
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:	root.SetRootSignature(assets, get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>(sub)); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:				CS			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:			NodeMask	= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:		CachedPSO	= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:				Flags		= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS>(sub); break;
				default: ISO_ASSERT(0);
			}
		}

	}
};


struct DX12GraphicsPipeline : DX12PipelineBase {
	struct STREAM_OUTPUT_DESC {
		dynamic_array<D3D12_SO_DECLARATION_ENTRY> declarations;
		dynamic_array<UINT> strides;
		UINT				RasterizedStream;
		void operator=(const D3D12_STREAM_OUTPUT_DESC &d) {
			declarations	= make_range_n(d.pSODeclaration, d.NumEntries);
			strides			= make_range_n(d.pBufferStrides, d.NumStrides);
		}
	};
	struct INPUT_LAYOUT_DESC {
		struct ELEMENT {
			string		SemanticName;
			UINT		SemanticIndex;
			DXGI_FORMAT	Format;
			UINT		InputSlot;
			UINT		AlignedByteOffset;
			D3D12_INPUT_CLASSIFICATION InputSlotClass;
			UINT		InstanceDataStepRate;
			ELEMENT(const D3D12_INPUT_ELEMENT_DESC &d) : SemanticName(d.SemanticName), SemanticIndex(d.SemanticIndex), Format(d.Format),
				InputSlot(d.InputSlot), AlignedByteOffset(d.AlignedByteOffset), InputSlotClass(d.InputSlotClass), InstanceDataStepRate(d.InstanceDataStepRate) {}
		};
		dynamic_array<ELEMENT> elements;

		void	operator=(const D3D12_INPUT_LAYOUT_DESC &d) {
			elements	= make_range_n(d.pInputElementDescs, d.NumElements);
		}
		const C_type *GetVertexType(int slot) const {
			C_type_struct	comp;
			uint32			offset = 0;
			for (auto &i : elements) {
				if (i.InputSlot == slot) {
					if (i.AlignedByteOffset != D3D12_APPEND_ALIGNED_ELEMENT)
						offset = i.AlignedByteOffset;
					auto	type = dx::to_c_type(i.Format);
					comp.add_atoffset(i.SemanticName, type, offset);
					offset += uint32(type->size());
				}
			}
			return ctypes.add(move(comp));
		}
	};
	struct VIEW_INSTANCING_DESC {
		dynamic_array<D3D12_VIEW_INSTANCE_LOCATION> locations;
		D3D12_VIEW_INSTANCING_FLAGS Flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
		void	operator=(const D3D12_VIEW_INSTANCING_DESC &d) {
			locations	= make_range_n(d.pViewInstanceLocations, d.ViewInstanceCount);
			Flags		= d.Flags;
		}
	};

	struct DEPTH_STENCIL_DESC : D3D12_DEPTH_STENCIL_DESC1 {
		void operator=(const D3D12_DEPTH_STENCIL_DESC &d) {
			D3D12_DEPTH_STENCIL_DESC1::operator=({
				d.DepthEnable,
				d.DepthWriteMask,
				d.DepthFunc,
				d.StencilEnable,
				d.StencilReadMask,
				d.StencilWriteMask,
				d.FrontFace,
				d.BackFace,
				true
				});
		}
		void operator=(const D3D12_DEPTH_STENCIL_DESC1 &d) {
			D3D12_DEPTH_STENCIL_DESC1::operator=(d);
		}
	};

	D3D12_SHADER_BYTECODE		VS = {0, 0};
	D3D12_SHADER_BYTECODE		PS = {0, 0};
	D3D12_SHADER_BYTECODE		DS = {0, 0};
	D3D12_SHADER_BYTECODE		HS = {0, 0};
	D3D12_SHADER_BYTECODE		GS = {0, 0};
	D3D12_SHADER_BYTECODE		AS = {0, 0};
	D3D12_SHADER_BYTECODE		MS = {0, 0};

	STREAM_OUTPUT_DESC					StreamOutput;
	D3D12_BLEND_DESC					BlendState;
	UINT								SampleMask;
	D3D12_RASTERIZER_DESC				RasterizerState;
	DEPTH_STENCIL_DESC					DepthStencilState;
	INPUT_LAYOUT_DESC					InputLayout;
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE	IBStripCutValue;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE		PrimitiveTopologyType;
	D3D12_RT_FORMAT_ARRAY				RTVFormats;
	DXGI_FORMAT							DSVFormat;
	DXGI_SAMPLE_DESC					SampleDesc;
	VIEW_INSTANCING_DESC				ViewInstancing;

	DX12GraphicsPipeline() {}

	auto	&shader(int i) const { return (&VS)[i]; }

	void Set(DX12Assets *assets, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *d) {
		root.SetRootSignature(assets, d->pRootSignature);
		VS						= d->VS;
		PS						= d->PS;
		DS						= d->DS;
		HS						= d->HS;
		GS						= d->GS;
		StreamOutput			= d->StreamOutput;
		BlendState				= d->BlendState;
		SampleMask				= d->SampleMask;
		RasterizerState			= d->RasterizerState;
		DepthStencilState		= d->DepthStencilState;
		InputLayout				= d->InputLayout;
		IBStripCutValue			= d->IBStripCutValue;
		PrimitiveTopologyType	= d->PrimitiveTopologyType;
		raw_copy(d->RTVFormats, RTVFormats.RTFormats);
		RTVFormats.NumRenderTargets =d->NumRenderTargets;
		DSVFormat				= d->DSVFormat;
		SampleDesc				= d->SampleDesc;
		NodeMask				= d->NodeMask;
		CachedPSO				= d->CachedPSO;
		Flags					= d->Flags;
	}

	void Set(DX12Assets *assets, const D3D12_PIPELINE_STATE_STREAM_DESC *d) {
		*this = DX12GraphicsPipeline();
		for (auto &sub : make_next_range<D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT>(const_memory_block(d->pPipelineStateSubobjectStream, d->SizeInBytes))) {
			switch (sub.t.u.t) {
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:		root.SetRootSignature(assets, get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>(sub)); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:					VS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:					PS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:					DS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:					HS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:					GS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS>(sub); break;
				//case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:					CS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:			StreamOutput			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:					BlendState				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:			SampleMask				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:			RasterizerState			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:			DepthStencilState		= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:			InputLayout				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:	IBStripCutValue			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:	PrimitiveTopologyType	= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:	RTVFormats				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:	DSVFormat				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:			SampleDesc				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:				NodeMask				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:			CachedPSO				= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:					Flags					= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:		DepthStencilState		= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:		ViewInstancing			= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:					AS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>(sub); break;
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:					MS						= get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>(sub); break;
				default: ISO_ASSERT(0);
			}
		}
	}
};

struct DX12State : DX12Assets::BatchInfo {
	DX12GraphicsPipeline						graphics_pipeline;
	DX12ComputePipeline							compute_pipeline;

	DX12Assets::ObjectRecord					*cbv_srv_uav_descriptor_heap	= 0;
	DX12Assets::ObjectRecord					*sampler_descriptor_heap		= 0;
	D3D12_INDEX_BUFFER_VIEW						ibv				= {0, 0, DXGI_FORMAT_UNKNOWN};
	dynamic_array<D3D12_VERTEX_BUFFER_VIEW>		vbv;
	D3D12_CPU_DESCRIPTOR_HANDLE					targets[8];
	D3D12_CPU_DESCRIPTOR_HANDLE					depth			= {0};
	float										BlendFactor[4]	= {0,0,0,0};
	uint32										StencilRef		= 0;
	D3D12_PRIMITIVE_TOPOLOGY					topology		= D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

	static_array<D3D12_VIEWPORT, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>	viewport;
	static_array<D3D12_RECT, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>		scissor;

	DX12State()		{ clear(targets); }
	void	Reset() { *this = DX12State(); }
	bool	IsGraphics() const;
	void	SetPipelineState(DX12Assets *assets, ID3D12PipelineState *p);

	const D3D12_VIEWPORT&	GetViewport(int i)	const { return viewport[i]; };
	const D3D12_RECT&		GetScissor(int i)	const { return scissor[i]; };

	Topology2 GetTopology() const {
		return dx::GetTopology((dx::PrimitiveType)topology);//(dx::PrimitiveType)draw.prim);
	}

	BackFaceCull CalcCull(bool flipped) const {
		return graphics_pipeline.RasterizerState.CullMode == D3D12_CULL_MODE_NONE ? BFC_NONE
			: (graphics_pipeline.RasterizerState.CullMode == D3D12_CULL_MODE_FRONT) ^ flipped ^ graphics_pipeline.RasterizerState.FrontCounterClockwise
			? BFC_BACK : BFC_FRONT;
	}

	indices	GetIndexing(dx::cache_type &cache, const DX12Assets::BatchInfo &batch) const {
		if (batch.op == RecCommandList::tag_DrawIndexedInstanced) {
			int		size	= DXGI_COMPONENTS(ibv.Format).Size();
			return indices(cache(ibv.BufferLocation + batch.draw.index_offset * size, batch.draw.vertex_count * size), size, batch.draw.vertex_offset, batch.draw.vertex_count);
		}
		return indices(0, 0, batch.draw.vertex_offset, batch.draw.vertex_count);
	}

	indices	GetIndexing(dx::cache_type &cache)	const {
		return GetIndexing(cache, *this);
	}

	DESCRIPTOR	GetBound(dx::SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space) const {
		DX12Assets::ObjectRecord	*dhobj	= type == DESCRIPTOR::SMP ? sampler_descriptor_heap : cbv_srv_uav_descriptor_heap;
		const RecDescriptorHeap		*dh		= dhobj ? (const RecDescriptorHeap*)dhobj->info : nullptr;
		return stage == dx::CS
			? compute_pipeline.root.GetBound(stage, type, bind, space, dh)
			: graphics_pipeline.root.GetBound(stage, type, bind, space, dh);
	}

	void	ProcessDevice(DX12Assets *assets, uint16 id, const void *p) {
		switch (id) {
			case RecDevice::tag_CreateConstantBufferView: COMParse(p, [assets](const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				unconst(assets->FindDescriptor(dest))->set(desc);
			}); break;
			case RecDevice::tag_CreateShaderResourceView: COMParse(p, [assets](ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				unconst(assets->FindDescriptor(dest))->set(pResource, desc);
			}); break;
			case RecDevice::tag_CreateUnorderedAccessView: COMParse(p, [assets](ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				unconst(assets->FindDescriptor(dest))->set(pResource, desc);
			}); break;
			case RecDevice::tag_CreateRenderTargetView: COMParse(p, [assets](ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				if (auto *rec = assets->FindRecResource(pResource))
					unconst(assets->FindDescriptor(dest))->set(pResource, desc, *rec);
			}); break;
			case RecDevice::tag_CreateDepthStencilView: COMParse(p, [assets](ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				if (auto *rec = assets->FindRecResource(pResource))
					unconst(assets->FindDescriptor(dest))->set(pResource, desc, *rec);
			}); break;
			case RecDevice::tag_CreateSampler: COMParse(p, [assets](const D3D12_SAMPLER_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				unconst(assets->FindDescriptor(dest))->set(desc);
			}); break;
			case RecDevice::tag_CopyDescriptors: COMParse(p, [assets](UINT dest_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0> dest_starts, counted<const UINT,0> dest_sizes, UINT srce_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3> srce_starts, counted<const UINT,3> srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type)  {
				UINT				ssize = 0;
				const DESCRIPTOR	*sdesc;

				for (UINT s = 0, d = 0; d < dest_num; d++) {
					UINT		dsize	= dest_sizes[d];
					DESCRIPTOR	*ddesc	= unconst(assets->FindDescriptor(dest_starts[d]));
					while (dsize) {
						if (ssize == 0) {
							ssize	= srce_sizes ? srce_sizes[s] : dsize;
							sdesc	= assets->FindDescriptor(srce_starts[s]);
							s++;
						}
						UINT	num = min(ssize, dsize);
						memcpy(ddesc, sdesc, sizeof(DESCRIPTOR) * num);
						ddesc += num;
						ssize -= num;
						dsize -= num;
					}
				}
			}); break;
			case RecDevice::tag_CopyDescriptorsSimple: COMParse(p, [assets](UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
				memcpy(unconst(assets->FindDescriptor(dest_start)), assets->FindDescriptor(srce_start), sizeof(DESCRIPTOR) * num);
			}); break;
		}
	}
	void	ProcessCommand(DX12Assets *assets, uint16 id, const void *p) {
		switch (id) {
			//ID3D12GraphicsCommandList
			case RecCommandList::tag_Close: COMParse(p, []() {
				});
				break;
			case RecCommandList::tag_Reset: COMParse(p, [this, assets](ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
					SetPipelineState(assets, pInitialState);
				});
				break;
			case RecCommandList::tag_ClearState: COMParse(p, [this, assets](ID3D12PipelineState *pPipelineState) {
					SetPipelineState(assets, pPipelineState);
				});
				break;
			case RecCommandList::tag_DrawInstanced: COMParse(p, [this](UINT vertex_count, UINT instance_count, UINT vertex_offset, UINT instance_offset) {
					draw.vertex_count		= vertex_count;
					draw.instance_count		= instance_count;
					draw.vertex_offset		= vertex_offset;
					draw.instance_offset	= instance_offset;
					draw.prim				= topology;
				});
				op	= (RecCommandList::tag)id;
				break;
			case RecCommandList::tag_DrawIndexedInstanced: COMParse(p, [this](UINT IndexCountPerInstance, UINT instance_count, UINT index_offset, INT vertex_offset, UINT instance_offset) {
					draw.vertex_count		= IndexCountPerInstance;
					draw.instance_count		= instance_count;
					draw.index_offset		= index_offset;
					draw.vertex_offset		= vertex_offset;
					draw.instance_offset	= instance_offset;
					draw.prim				= topology;
				});
				op	= (RecCommandList::tag)id;
				break;
			case RecCommandList::tag_Dispatch: COMParse(p, [this](UINT dim_x, UINT dim_y, UINT dim_z) {
					compute.dim_x			= dim_x;
					compute.dim_y			= dim_y;
					compute.dim_z			= dim_z;
				});
				op	= (RecCommandList::tag)id;
				break;
			case RecCommandList::tag_CopyBufferRegion: COMParse(p, [](ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes) {
				});
				break;
			case RecCommandList::tag_CopyTextureRegion: COMParse2(p, [](const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox) {
				});
				break;
			case RecCommandList::tag_CopyResource: COMParse(p, [](ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource) {
				});
				break;
			case RecCommandList::tag_CopyTiles: COMParse(p, [](ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
				});
				break;
			case RecCommandList::tag_ResolveSubresource: COMParse(p, [](ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) {
				});
				break;
			case RecCommandList::tag_IASetPrimitiveTopology: COMParse(p, [this](D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) {
					topology	= PrimitiveTopology;
				});
				break;
			case RecCommandList::tag_RSSetViewports: COMParse(p, [this](UINT n, const D3D12_VIEWPORT *pViewports) {
					viewport = make_range_n(pViewports, n);
				});
				break;
			case RecCommandList::tag_RSSetScissorRects: COMParse(p, [this](UINT n, const D3D12_RECT *pRects) {
					scissor = make_range_n(pRects, n);
				});
				break;
			case RecCommandList::tag_OMSetBlendFactor: COMParse(p, [this](const FLOAT BlendFactor[4]) {
					memcpy(this->BlendFactor, BlendFactor, sizeof(FLOAT) * 4);
				});
				break;
			case RecCommandList::tag_OMSetStencilRef: COMParse(p, [this](UINT StencilRef) {
					this->StencilRef = StencilRef;
				});
				break;
			case RecCommandList::tag_SetPipelineState: COMParse(p, [this, assets](ID3D12PipelineState *pPipelineState) {
					SetPipelineState(assets, pPipelineState);
				});
				break;
//			case RecCommandList::tag_ResourceBarrier: COMParse(p, [this](UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers) {
//				});
//				break;
			case RecCommandList::tag_ExecuteBundle: COMParse(p, [](ID3D12GraphicsCommandList *pCommandList) {
				});
				break;
			case RecCommandList::tag_SetDescriptorHeaps: COMParse(p, [this, assets](UINT n, ID3D12DescriptorHeap *const *pp) {
				cbv_srv_uav_descriptor_heap = sampler_descriptor_heap = 0;
				for (int i = 0; i < n; i++) {
					if (auto *obj = assets->FindObject((uint64)pp[i])) {
						ISO_ASSERT(obj->type == RecObject::DescriptorHeap);
						const RecDescriptorHeap	*h = obj->info;
						if (h->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
							sampler_descriptor_heap		= obj;
						else
							cbv_srv_uav_descriptor_heap = obj;
					}
				}
				});
				break;
			case RecCommandList::tag_SetComputeRootSignature: COMParse(p, [this, assets](ID3D12RootSignature *pRootSignature) {
					compute_pipeline.root.SetRootSignature(assets, pRootSignature);
				});
				break;
			case RecCommandList::tag_SetGraphicsRootSignature: COMParse(p, [this, assets](ID3D12RootSignature *pRootSignature) {
					graphics_pipeline.root.SetRootSignature(assets, pRootSignature);
				});
				break;
			case RecCommandList::tag_SetComputeRootDescriptorTable: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
					*compute_pipeline.root.Slot(RootParameterIndex) = BaseDescriptor;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootDescriptorTable: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
					*graphics_pipeline.root.Slot(RootParameterIndex) = BaseDescriptor;
				});
				break;
			case RecCommandList::tag_SetComputeRoot32BitConstant: COMParse(p, [this](UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
					((uint32*)compute_pipeline.root.Slot(RootParameterIndex))[DestOffsetIn32BitValues] = SrcData;
				});
				break;
			case RecCommandList::tag_SetGraphicsRoot32BitConstant: COMParse(p, [this](UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
					((uint32*)graphics_pipeline.root.Slot(RootParameterIndex))[DestOffsetIn32BitValues] = SrcData;
				});
				break;
			case RecCommandList::tag_SetComputeRoot32BitConstants: COMParse(p, [this](UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
					memcpy((uint32*)compute_pipeline.root.Slot(RootParameterIndex) + DestOffsetIn32BitValues, pSrcData, Num32BitValuesToSet / 4);
				});
				break;
			case RecCommandList::tag_SetGraphicsRoot32BitConstants: COMParse(p, [this](UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
					memcpy((uint32*)graphics_pipeline.root.Slot(RootParameterIndex) + DestOffsetIn32BitValues, pSrcData, Num32BitValuesToSet / 4);
				});
				break;
			case RecCommandList::tag_SetComputeRootConstantBufferView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*compute_pipeline.root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootConstantBufferView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*graphics_pipeline.root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetComputeRootShaderResourceView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*compute_pipeline.root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootShaderResourceView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*graphics_pipeline.root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetComputeRootUnorderedAccessView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*compute_pipeline.root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootUnorderedAccessView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*graphics_pipeline.root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_IASetIndexBuffer: COMParse(p, [this](const D3D12_INDEX_BUFFER_VIEW *pView) {
					ibv = *pView;
				});
				break;
			case RecCommandList::tag_IASetVertexBuffers: COMParse(p, [this](UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews) {
					vbv.assign(pViews, pViews + NumViews);
				});
				break;
			case RecCommandList::tag_SOSetTargets: COMParse(p, [](UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews) {
				});
				break;
			case RecCommandList::tag_OMSetRenderTargets: COMParse(p, [this](UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) {
					memcpy(targets, pRenderTargetDescriptors, NumRenderTargetDescriptors * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
					if (pDepthStencilDescriptor)
						depth = *pDepthStencilDescriptor;
					else
						depth.ptr = 0;
				});
				break;
			case RecCommandList::tag_ClearDepthStencilView: COMParse(p, [](D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_ClearRenderTargetView: COMParse(p, [](D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_ClearUnorderedAccessViewUint: COMParse(p, [](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_ClearUnorderedAccessViewFloat: COMParse(p, [](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_DiscardResource: COMParse(p, [](ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion) {
				});
				break;
			case RecCommandList::tag_BeginQuery: COMParse(p, [](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
				});
				break;
			case RecCommandList::tag_EndQuery: COMParse(p, [](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
				});
				break;
			case RecCommandList::tag_ResolveQueryData: COMParse(p, [](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset) {
				});
				break;
			case RecCommandList::tag_SetPredication: COMParse(p, [](ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation) {
				});
				break;
			case RecCommandList::tag_SetMarker: COMParse(p, [](UINT Metadata, const void *pData, UINT Size) {
				});
				break;
			case RecCommandList::tag_BeginEvent: COMParse(p, [](UINT Metadata, const void *pData, UINT Size) {
				});
				break;
			case RecCommandList::tag_EndEvent: COMParse(p, []() {
				});
				break;
			case RecCommandList::tag_ExecuteIndirect: COMParse(p, [this, assets](ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset) {
					indirect.command_count		= MaxCommandCount;
					indirect.signature			= assets->FindObject(uint64(pCommandSignature));
					indirect.arguments			= assets->FindObject(uint64(pArgumentBuffer));
					indirect.counts				= assets->FindObject(uint64(pCountBuffer));
					indirect.arguments_offset	= ArgumentBufferOffset;
					indirect.counts_offset		= CountBufferOffset;
				});
				op	= (RecCommandList::tag)id;
				break;

			//ID3D12GraphicsCommandList1
			case RecCommandList::tag_AtomicCopyBufferUINT: COMParse(p, [](ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges) {}); break;
			case RecCommandList::tag_AtomicCopyBufferUINT64: COMParse(p, [](ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges) {}); break;
			case RecCommandList::tag_OMSetDepthBounds: COMParse(p, [](FLOAT Min, FLOAT Max) {}); break;
			case RecCommandList::tag_SetSamplePositions: COMParse(p, [](UINT NumSamplesPerPixel, UINT NumPixels, D3D12_SAMPLE_POSITION *pSamplePositions) {}); break;
			case RecCommandList::tag_ResolveSubresourceRegion: COMParse(p, [](ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, ID3D12Resource *pSrcResource, UINT SrcSubresource, D3D12_RECT *pSrcRect, DXGI_FORMAT Format, D3D12_RESOLVE_MODE ResolveMode) {}); break;
			case RecCommandList::tag_SetViewInstanceMask: COMParse(p, [](UINT Mask) {}); break;
			//ID3D12GraphicsCommandList2
			case RecCommandList::tag_WriteBufferImmediate: COMParse(p, [](UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams, const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes) {}); break;
			//ID3D12GraphicsCommandList3
			case RecCommandList::tag_SetProtectedResourceSession: COMParse(p, [](ID3D12ProtectedResourceSession *pProtectedResourceSession) {}); break;
			//ID3D12GraphicsCommandList4
			case RecCommandList::tag_BeginRenderPass: COMParse(p, [this](UINT NumRenderTargets, counted<const D3D12_RENDER_PASS_RENDER_TARGET_DESC, 0> pRenderTargets, const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags) {
				for (int i = 0; i < NumRenderTargets; i++)
					targets[i] = pRenderTargets[i].cpuDescriptor;
				if (pDepthStencil)
					depth = pDepthStencil->cpuDescriptor;
				else
					depth.ptr = 0;
				});
				break;
			case RecCommandList::tag_EndRenderPass: COMParse(p, []() {}); break;
			case RecCommandList::tag_InitializeMetaCommand: COMParse(p, [](ID3D12MetaCommand *pMetaCommand, const void *pInitializationParametersData, SIZE_T InitializationParametersDataSizeInBytes) {}); break;
			case RecCommandList::tag_ExecuteMetaCommand: COMParse(p, [](ID3D12MetaCommand *pMetaCommand, const void *pExecutionParametersData, SIZE_T ExecutionParametersDataSizeInBytes) {}); break;
			case RecCommandList::tag_BuildRaytracingAccelerationStructure: COMParse(p, [](const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc, UINT NumPostbuildInfoDescs, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs) {}); break;
			case RecCommandList::tag_EmitRaytracingAccelerationStructurePostbuildInfo: COMParse(p, [](const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc, UINT NumSourceAccelerationStructures, const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData) {}); break;
			case RecCommandList::tag_CopyRaytracingAccelerationStructure: COMParse(p, [](D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData, D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) {}); break;
			case RecCommandList::tag_SetPipelineState1: COMParse(p, [](ID3D12StateObject *pStateObject) {}); break;
			case RecCommandList::tag_DispatchRays: COMParse(p, [this](const D3D12_DISPATCH_RAYS_DESC *pDesc) {
					rays.dim_x	= pDesc->Width;
					rays.dim_y	= pDesc->Height;
					rays.dim_z	= pDesc->Depth;
					rays.RayGenerationShaderRecord = pDesc->RayGenerationShaderRecord;
					rays.MissShaderTable = pDesc->MissShaderTable;
					rays.HitGroupTable = pDesc->HitGroupTable;
					rays.CallableShaderTable = pDesc->CallableShaderTable;
				});
				op	= (RecCommandList::tag)id;
				break;
			//ID3D12GraphicsCommandList5
			case RecCommandList::tag_RSSetShadingRate: COMParse(p, [](D3D12_SHADING_RATE baseShadingRate, const D3D12_SHADING_RATE_COMBINER* combiners) {}); break;
			case RecCommandList::tag_RSSetShadingRateImage: COMParse(p, [](ID3D12Resource* shadingRateImage) {}); break;
			//ID3D12GraphicsCommandList6
			case RecCommandList::tag_DispatchMesh: COMParse(p, [this](UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) {
					compute.dim_x	= ThreadGroupCountX;
					compute.dim_y	= ThreadGroupCountY;
					compute.dim_z	= ThreadGroupCountZ;
				});
				op	= (RecCommandList::tag)id;
				break;
		}
	}
};

bool DX12State::IsGraphics() const {
	return	op == RecCommandList::tag_ExecuteIndirect ? ::IsGraphics(rmap_unique<RTM, D3D12_COMMAND_SIGNATURE_DESC>(indirect.signature->info))
		:	op == RecCommandList::tag_DrawInstanced || op == RecCommandList::tag_DrawIndexedInstanced;
}
void DX12State::SetPipelineState(DX12Assets *assets, ID3D12PipelineState *p) {
	if (auto *obj = assets->FindObject(uint64(p))) {
		switch (*(int*)obj->info) {
			case 0:
				graphics_pipeline.Set(assets, rmap_unique<KM, D3D12_GRAPHICS_PIPELINE_STATE_DESC>(obj->info + 8));
				break;
			
			case 1:
				compute_pipeline.Set(assets, rmap_unique<KM, D3D12_COMPUTE_PIPELINE_STATE_DESC>(obj->info + 8));
				break;

			case 2: {
				auto	p		= rmap_unique<KM, D3D12_PIPELINE_STATE_STREAM_DESC>(obj->info + 8);
				bool	compute	= false;
				for (auto &sub : make_next_range<D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT>(const_memory_block(p->pPipelineStateSubobjectStream, p->SizeInBytes))) {
					if (compute = (sub.t.u.t == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS))
						break;
				}
				if (compute)
					compute_pipeline.Set(assets, p);
				else
					graphics_pipeline.Set(assets, p);
				break;
			}
		}
	}
}

auto PutMask(uint32 mask) {
	return [=](string_accum& sa) {
		if (mask != 15)
			sa << '.' << onlyif(mask & 1, 'r') << onlyif(mask & 2, 'g') << onlyif(mask & 4, 'b') << onlyif(mask & 8, 'a');
	};
}
auto PutMaskedCol(uint32 mask, const float *cc) {
	return [=](string_accum& sa) {
		if (!is_pow2(mask))
			sa << "float" << count_bits(mask) << '(';
		sa << onlyif(mask & 1, cc[0]) << onlyif(mask & 2, cc[1]) << onlyif(mask & 4, cc[2]) << onlyif(mask & 8, cc[3]) << onlyif(!is_pow2(mask), ')');
	};
}
string_accum &BlendStateMult(string_accum &sa, D3D12_BLEND m, uint32 mask, bool dest, const float *cc) {
	static const char *multiplier[] = {
		0,
		0,					//D3D12_BLEND_ZERO:				
		0,					//D3D12_BLEND_ONE:				
		"S",				//D3D12_BLEND_SRC_COLOR:			
		"(1 - S)",			//D3D12_BLEND_INV_SRC_COLOR:		
		"S.a",				//D3D12_BLEND_SRC_ALPHA:			
		"(1 - S.a)",		//D3D12_BLEND_INV_SRC_ALPHA:		
		"D.a",				//D3D12_BLEND_DEST_ALPHA:		
		"(1 - D.a)",		//D3D12_BLEND_INV_DEST_ALPHA:	
		"D",				//D3D12_BLEND_DEST_COLOR:		
		"(1 - D)",			//D3D12_BLEND_INV_DEST_COLOR:	
		0,					//D3D12_BLEND_SRC_ALPHA_SAT:		
		0,					//x:
		0,					//x:
		0,					//D3D12_BLEND_BLEND_FACTOR:		
		0,					//D3D12_BLEND_INV_BLEND_FACTOR:	
		"S1",				//D3D12_BLEND_SRC1_COLOR:		
		"(1 - S1)",			//D3D12_BLEND_INV_SRC1_COLOR:	
		"S1.a",				//D3D12_BLEND_SRC1_ALPHA:		
		"(1 - S1.a)",		//D3D12_BLEND_INV_SRC1_ALPHA:	
	};

	char	s = dest ? 'D' : 'S';
	switch (m) {
		case D3D12_BLEND_ZERO:				return sa << '0';
		case D3D12_BLEND_ONE:				return sa << s << PutMask(mask);
		case D3D12_BLEND_SRC_ALPHA_SAT:		return sa << "saturate(" << s << PutMask(mask) << " * S.a)";
		case D3D12_BLEND_BLEND_FACTOR:		return sa << s << PutMask(mask) << " * " << PutMaskedCol(mask, cc);
		case D3D12_BLEND_INV_BLEND_FACTOR:	return sa << s << PutMask(mask) << " * (1 - " << PutMaskedCol(mask, cc) << ')';
		default:							return sa << s << PutMask(mask) << " * " << multiplier[m];
	}
}
string_accum &BlendStateFunc(string_accum &sa, D3D12_BLEND_OP BlendOp, D3D12_BLEND SrcBlend, D3D12_BLEND DestBlend, uint32 mask, const float *cc) {
	switch (BlendOp) {
		case D3D12_BLEND_OP_ADD:
			if (SrcBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa, SrcBlend, mask, false, cc) << onlyif(DestBlend != D3D12_BLEND_ZERO, " + ");
			if (DestBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa, DestBlend, mask, true, cc);
			return sa;

		case D3D12_BLEND_OP_SUBTRACT:
			if (SrcBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa, SrcBlend, mask, false, cc);
			if (DestBlend != D3D12_BLEND_ZERO)
				BlendStateMult(sa << " - ", DestBlend, mask, true, cc);
			return sa;

		case D3D12_BLEND_OP_MIN:
			return BlendStateMult(BlendStateMult(sa << "min(", SrcBlend, mask, false, cc) << ", ", DestBlend, mask, true, cc) << ')';

		case D3D12_BLEND_OP_MAX:
			return BlendStateMult(BlendStateMult(sa << "max(", SrcBlend, mask, false, cc) << ", ", DestBlend, mask, true, cc) << ')';
	}
	return sa << '?';
}

auto WriteBlend(const D3D12_RENDER_TARGET_BLEND_DESC& desc, const float *cc) {
	return [&](string_accum& a) {
		if (desc.RenderTargetWriteMask == 0) {
			a << "masked out";
			return;
		}
		
		a << "D" << PutMask(desc.RenderTargetWriteMask) << " = ";

		if (!desc.BlendEnable) {
			a << "S" << PutMask(desc.RenderTargetWriteMask);

		} else if (!desc.LogicOpEnable) {
			if ((desc.RenderTargetWriteMask & D3D12_COLOR_WRITE_ENABLE_ALPHA) && (desc.SrcBlend != desc.SrcBlendAlpha || desc.DestBlend != desc.DestBlendAlpha || desc.BlendOp != desc.BlendOpAlpha)) {
				BlendStateFunc(a << "float" << count_bits(desc.RenderTargetWriteMask) << '(', desc.BlendOp, desc.SrcBlend, desc.DestBlend, desc.RenderTargetWriteMask & 7, cc);
				BlendStateFunc(a << ", ", desc.BlendOpAlpha, desc.SrcBlendAlpha, desc.DestBlendAlpha, 8, cc) << ')';
			} else {
				BlendStateFunc(a, desc.BlendOp, desc.SrcBlend, desc.DestBlend, desc.RenderTargetWriteMask, cc);
			}
		} else {
			static const char *logic[] = {
				"0",		//D3D12_LOGIC_OP_CLEAR
				"1",		//D3D12_LOGIC_OP_SET
				"S",		//D3D12_LOGIC_OP_COPY
				"~S",		//D3D12_LOGIC_OP_COPY_INVERTED
				"D(leave)",	//D3D12_LOGIC_OP_NOOP
				"~D",		//D3D12_LOGIC_OP_INVERT
				"S & D",	//D3D12_LOGIC_OP_AND
				"~(S & D)",	//D3D12_LOGIC_OP_NAND
				"S | D",	//D3D12_LOGIC_OP_OR
				"~(S | D)",	//D3D12_LOGIC_OP_NOR
				"S ^ D",	//D3D12_LOGIC_OP_XOR
				"~(S ^ D)",	//D3D12_LOGIC_OP_EQUIV
				"S & ~D",	//D3D12_LOGIC_OP_AND_REVERSE
				"~S & D",	//D3D12_LOGIC_OP_AND_INVERTED
				"S | ~D",	//D3D12_LOGIC_OP_OR_REVERSE
				"~S | D",	//D3D12_LOGIC_OP_OR_INVERTED
			};
			a << logic[desc.LogicOp];
		}
	};
}

//-----------------------------------------------------------------------------
//	DX12ShaderState
//-----------------------------------------------------------------------------

dx::SimulatorDXBC::Resource MakeSimulatorResource(DX12Assets &assets, dx::cache_type &cache, const DESCRIPTOR &desc) {
	dx::SimulatorDXBC::Resource	r;
	auto				*obj	= assets.FindObject((uint64)desc.res);
	if (!obj)
		return r;

	const RecResource	*rr		= obj->info;
	uint64				gpu		= rr->gpu;

	if (desc.type == DESCRIPTOR::SRV) {
		auto	&srv	= desc.srv;
		DXGI_COMPONENTS	format = srv.Format;
		format.chans = Rearrange((DXGI_COMPONENTS::SWIZZLE)format.chans, (DXGI_COMPONENTS::SWIZZLE)srv.Shader4ComponentMapping);

		switch (srv.ViewDimension) {
			case D3D12_SRV_DIMENSION_UNKNOWN:
			case D3D12_SRV_DIMENSION_BUFFER:
				r.init(dx::RESOURCE_DIMENSION_BUFFER, format, srv.Buffer.StructureByteStride, srv.Buffer.NumElements);
				gpu	+= srv.Buffer.FirstElement;
				break;

			case D3D12_SRV_DIMENSION_TEXTURE1D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1D, format, rr->Width, 0);
				r.set_mips(srv.Texture1D.MostDetailedMip, srv.Texture1D.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1DARRAY, format, rr->Width, 0, srv.Texture1DArray.ArraySize);
				r.set_mips(srv.Texture1DArray.MostDetailedMip, srv.Texture1DArray.MipLevels);
				r.set_slices(srv.Texture1DArray.FirstArraySlice, srv.Texture1DArray.ArraySize);
				break;

			case D3D12_SRV_DIMENSION_TEXTURE2D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, format, rr->Width, rr->Height);
				r.set_mips(srv.Texture2D.MostDetailedMip, srv.Texture2D.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2DARRAY, format, rr->Width, rr->Height, srv.Texture2DArray.FirstArraySlice + srv.Texture2DArray.ArraySize);
				r.set_mips(srv.Texture2DArray.MostDetailedMip, srv.Texture2DArray.MipLevels);
				r.set_slices(srv.Texture2DArray.FirstArraySlice, srv.Texture2DArray.ArraySize);
				break;

			//case D3D12_SRV_DIMENSION_TEXTURE2DMS:
			//case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
			case D3D12_SRV_DIMENSION_TEXTURE3D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE3D, format, rr->Width, rr->Height, rr->DepthOrArraySize);
				r.set_mips(srv.Texture3D.MostDetailedMip, srv.Texture3D.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				r.init(dx::RESOURCE_DIMENSION_TEXTURECUBE, format, rr->Width, rr->Height, 6);
				r.set_mips(srv.TextureCube.MostDetailedMip, srv.TextureCube.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURECUBEARRAY, format, rr->Width, rr->Height, srv.TextureCubeArray.First2DArrayFace + srv.TextureCubeArray.NumCubes * 6);
				r.set_mips(srv.TextureCubeArray.MostDetailedMip, srv.TextureCubeArray.MipLevels);
				r.set_slices(srv.TextureCubeArray.First2DArrayFace, srv.TextureCubeArray.NumCubes * 6);
				break;
		}

	} else if (desc.type == DESCRIPTOR::UAV) {
		auto	&uav	= desc.uav;
		switch (uav.ViewDimension) {
			case D3D12_UAV_DIMENSION_BUFFER:
				gpu	+= uav.Buffer.FirstElement;
				r.init(dx::RESOURCE_DIMENSION_BUFFER, uav.Format, uav.Buffer.StructureByteStride, uav.Buffer.NumElements);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE1D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1D, uav.Format, rr->Width, 0);
				r.set_mips(uav.Texture1D.MipSlice, 1);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1DARRAY, uav.Format, rr->Width, uav.Texture1DArray.FirstArraySlice + uav.Texture1DArray.ArraySize);
				r.set_mips(uav.Texture1DArray.MipSlice, 1);
				r.set_slices(uav.Texture1DArray.FirstArraySlice, uav.Texture1DArray.ArraySize);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE2D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, uav.Format, rr->Width, rr->Height);
				r.set_mips(uav.Texture2D.MipSlice, 1);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2DARRAY, uav.Format, rr->Width, rr->Height, uav.Texture2DArray.FirstArraySlice + uav.Texture2DArray.ArraySize);
				r.set_mips(uav.Texture2DArray.MipSlice, 1);
				r.set_slices(uav.Texture2DArray.FirstArraySlice, uav.Texture2DArray.ArraySize);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE3D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE3D, uav.Format, rr->Width, rr->Height, rr->DepthOrArraySize);
				r.set_mips(uav.Texture3D.MipSlice, 1);
				break;
		}
	}

	r.set_mem(cache(gpu + uintptr_t((void*)r), r.length()));
	return r;
}

dx::SimulatorDXBC::Buffer MakeSimulatorConstantBuffer(dx::cache_type &cache, const DESCRIPTOR &desc) {
	dx::SimulatorDXBC::Buffer	r;

	switch (desc.type) {
		case DESCRIPTOR::CBV:
			r.set_mem(cache(desc.cbv.BufferLocation, desc.cbv.SizeInBytes));
			break;
		case DESCRIPTOR::PCBV:
			r.set_mem(cache(desc.ptr, desc.cbv.SizeInBytes));
			break;
		case DESCRIPTOR::IMM:
			r.set_mem(memory_block(unconst(desc.imm), desc.cbv.SizeInBytes));
			break;
	}
	return r;
}

dx::SimulatorDXBC::Sampler MakeSimulatorSampler(DX12Assets &assets, const DESCRIPTOR &desc) {
	dx::SimulatorDXBC::Sampler	r;
	if (desc.type == DESCRIPTOR::SMP) {
		r.filter		= dx::TextureFilterMode(desc.smp.Filter);
		r.address_u		= (dx::TextureAddressMode)desc.smp.AddressU;
		r.address_v		= (dx::TextureAddressMode)desc.smp.AddressV;
		r.address_w		= (dx::TextureAddressMode)desc.smp.AddressW;
		r.minlod		= desc.smp.MinLOD;
		r.maxlod		= desc.smp.MaxLOD;
		r.bias			= desc.smp.MipLODBias;
		r.comparison	= (dx::ComparisonFunction)desc.smp.ComparisonFunc;
		memcpy(r.border, desc.smp.BorderColor, 4 * sizeof(float));

	} else if (desc.type == DESCRIPTOR::SSMP) {
		r.filter		= dx::TextureFilterMode(desc.ssmp.Filter);
		r.address_u		= (dx::TextureAddressMode)desc.ssmp.AddressU;
		r.address_v		= (dx::TextureAddressMode)desc.ssmp.AddressV;
		r.address_w		= (dx::TextureAddressMode)desc.ssmp.AddressW;
		r.minlod		= desc.ssmp.MinLOD;
		r.maxlod		= desc.ssmp.MaxLOD;
		r.bias			= desc.ssmp.MipLODBias;
		r.comparison	= (dx::ComparisonFunction)desc.ssmp.ComparisonFunc;
		r.border[0]		= r.border[1] = r.border[2] = float(desc.ssmp.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);
		r.border[3]		= float(desc.ssmp.BorderColor != D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK);
	}
	return r;
}

struct DX12ShaderState : dx::Shader {
	const char *name;
	
	sparse_array<DESCRIPTOR>	cbv;
	sparse_array<DESCRIPTOR>	srv;
	sparse_array<DESCRIPTOR>	uav;
	sparse_array<DESCRIPTOR>	smp;

	DX12ShaderState() {}
	DX12ShaderState(const DX12Assets::ShaderRecord *rec) : dx::Shader(rec->stage, rec->data, rec->addr), name(rec->name) {}
	void	init(DX12Assets &assets, dx::cache_type &cache, const DX12Assets::ShaderRecord *_rec, const DX12State *state);

	void	InitSimulator(dx::SimulatorDXBC &sim, DX12Assets &assets, dx::cache_type &cache) const {
		for (auto &i : cbv)
			sim.cbv[i.index()]	= MakeSimulatorConstantBuffer(cache, i);

		for (auto &i : srv)
			sim.srv[i.index()]	= MakeSimulatorResource(assets, cache, i);

		for (auto &i : uav)
			sim.uav[i.index()]	= MakeSimulatorResource(assets, cache, i);

		for (auto &i : smp)
			sim.smp[i.index()]	= MakeSimulatorSampler(assets, i);
	}
};

void DX12ShaderState::init(DX12Assets &assets, dx::cache_type &cache, const DX12Assets::ShaderRecord *rec, const DX12State *state) {
	stage	= rec->stage;
	data	= rec->data;
	addr	= rec->addr;
	name	= rec->name;

	dx::DeclReader	decl(GetUCode());

	for (auto &i : decl.cb) {
		auto	desc = state->GetBound(stage, DESCRIPTOR::CBV, i.index, 0);
		if (desc.type == DESCRIPTOR::PCBV || desc.type == DESCRIPTOR::IMM)
			desc.cbv.SizeInBytes = i.size * 16;
		cbv[i.index] = desc;
	}

	for (auto &i : decl.srv)
		srv[i.index] = state->GetBound(stage, DESCRIPTOR::SRV, i.index, 0);

	for (auto &i : decl.uav)
		uav[i.index] = state->GetBound(stage, DESCRIPTOR::UAV, i.index, 0);

	for (auto &i : decl.smp)
		smp[i.index] = state->GetBound(stage, DESCRIPTOR::SMP, i.index, 0);
}

//-----------------------------------------------------------------------------
//	DX12Replay
//-----------------------------------------------------------------------------

struct DX12Replay : COMReplay<DX12Replay> {
	com_ptr<ID3D12Device>	device;
	com_ptr<ID3D12Fence>	fence;
	Event					fence_event;
	uint32					fence_value;
	DX12Assets				&assets;
	dx::cache_type			&cache;
	hash_map<void*,void*>	obj2local;
	ID3D12GraphicsCommandList	*current_cmd;
	ID3D12CommandQueue			*current_queue;

	struct CommandListInfo : small_set<ID3D12CommandQueue*, 8> {
		bool	closed;
		CommandListInfo() : closed(false) {}
	};

	map<ID3D12CommandAllocator*,small_set<ID3D12GraphicsCommandList*, 8>>	alloc_cmds;
	map<ID3D12GraphicsCommandList*, CommandListInfo>						cmd_infos;

	uint32						descriptor_sizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	DX12Replay*	operator->()	{ return this; }

	arbitrary_ptr	_lookup(void *p) {
		if (p == 0)
			return p;
		void	*r = obj2local[p];
		if (!r) {
			auto	*obj	= assets.FindObject((uint64)p);
			abort			= !obj;
			//ISO_ASSERT(obj);
			if (obj) {
				r = obj2local[p] = MakeLocal(obj);
				abort	= !r;
				ISO_ASSERT(r);
			}
		}
		return r;
	}

	template<typename T> T* lookup(T *p) {
		return _lookup(p);
	}

	handle					lookup(const handle& p) {
		if (!p.valid())
			return p;
		void	*r = obj2local[p];
		if (!r) {
			auto	*obj	= assets.FindObject((uint64)(void*)p);
			abort			= !obj;
			//ISO_ASSERT(obj);
			if (obj)
				r = obj2local[p] = CreateEvent(nullptr, false, false, nullptr);
		}
		return r;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE	lookup(D3D12_CPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : assets.descriptor_heaps) {
			if (const DESCRIPTOR *d = i->holds(h)) {
				ID3D12DescriptorHeap		*heap	= (ID3D12DescriptorHeap*)obj2local[i.obj->obj].or_default();
				abort = !heap;
				if (heap) {
					D3D12_CPU_DESCRIPTOR_HANDLE cpu		= heap->GetCPUDescriptorHandleForHeapStart();
					cpu.ptr += descriptor_sizes[i->type] * i->index(h);
					return cpu;
				}
			}
		}
		return h;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE	lookup(D3D12_GPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : assets.descriptor_heaps) {
			if (const DESCRIPTOR *d = i->holds(h)) {
				ID3D12DescriptorHeap		*heap	= (ID3D12DescriptorHeap*)obj2local[i.obj->obj].or_default();
				abort = !heap;
				if (heap) {
					D3D12_GPU_DESCRIPTOR_HANDLE gpu		= heap->GetGPUDescriptorHandleForHeapStart();
					gpu.ptr += descriptor_sizes[i->type] * i->index(h);
					return gpu;
				}
			}
		}
		return h;
	}

	D3D12_GPU_VIRTUAL_ADDRESS lookup(D3D12_GPU_VIRTUAL_ADDRESS a, size_t size = 0) {
		auto						*obj	= assets.FindByGPUAddress(a);
		const RecResource			*r		= obj->info;
		ISO_ASSERT(a >= r->gpu && a + size <= r->gpu + r->data_size);
		auto						r2		= (ID3D12Resource*)obj2local[obj->obj].or_default();
		D3D12_GPU_VIRTUAL_ADDRESS	a2		= r2->GetGPUVirtualAddress();
		return a2 + (a - r->gpu);
	}

	const void *lookup_shader(const void *p) {
		auto *r = assets.FindShader((uint64)p);
		if (r)
			return r->data;
		return 0;
	}

	bool Init(IDXGIAdapter1 *adapter) {
		com_ptr<ID3D12Debug> debug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
			debug->EnableDebugLayer();

		com_ptr<IDXGIFactory4> factory;
		return SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))
			&& SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&device));
	}

	void *InitObject(void *v0, void *v1) {
		if (v1) {
			if (auto *obj = assets.FindObject((uint64)v0)) {
				((ID3D12Object*)v1)->SetName(obj->name);
				if (obj->type == RecObject::Resource) {
					const RecResource	*rr = obj->info;
					if (rr->HasData()) {
						auto	*r = (ID3D12Resource*)v1;
						void	*p = 0;
						if (SUCCEEDED(r->Map(0, 0, &p))) {
							memcpy(p, cache(rr->gpu, rr->data_size), rr->data_size);
							r->Unmap(0, 0);
						}
					}
				}
			}
		}
		return obj2local[v0] = v1;
	}

	template<typename F1, typename F2> void* ReplayPP2(const void *p, F2 f) {
		typedef	typename function<F1>::P	P;
		typedef map_t<PM, P>				RTL1;

		auto	*t	= (TL_tuple<RTL1>*)p;
		auto	&pp	= t->template get<RTL1::count - 1>();
		auto	v0	= save(*pp);

		Replay2<F1>(device, p, f);

		return InitObject(v0, *pp);
	}
	template<typename F> void* ReplayPP(const void *p, F f) {
		return ReplayPP2<F>(p, f);
	}

	void	Process(uint16 id, const void *p) {
		switch (id) {
			//ID3D12Device
			case RecDevice::tag_CreateCommandQueue:							ReplayPP(p,			&ID3D12Device::CreateCommandQueue); break;
			case RecDevice::tag_CreateCommandAllocator:						ReplayPP(p,			&ID3D12Device::CreateCommandAllocator); break;
			case RecDevice::tag_CreateGraphicsPipelineState:				ReplayPP(p,			&ID3D12Device::CreateGraphicsPipelineState); break;
			case RecDevice::tag_CreateComputePipelineState:					ReplayPP(p,			&ID3D12Device::CreateComputePipelineState); break;
			case RecDevice::tag_CreateCommandList:							Replay(p,			[this](UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **pp) {
					auto	v0	= save(*pp);
					device->CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState, riid, pp);
					alloc_cmds[pCommandAllocator].insert((ID3D12GraphicsCommandList*)*pp);
					InitObject(v0, *pp);
				});
				break;
			case RecDevice::tag_CheckFeatureSupport:						Replay(device, p,	&ID3D12Device::CheckFeatureSupport); break;
			case RecDevice::tag_CreateDescriptorHeap:						ReplayPP(p,			&ID3D12Device::CreateDescriptorHeap); break;
			case RecDevice::tag_CreateRootSignature:						ReplayPP2<HRESULT(UINT, counted<uint8, 2>, SIZE_T, REFIID, void**)>(p, &ID3D12Device::CreateRootSignature); break;
			case RecDevice::tag_CreateConstantBufferView:					Replay(device, p,	&ID3D12Device::CreateConstantBufferView); break;
			case RecDevice::tag_CreateShaderResourceView:					Replay(device, p,	&ID3D12Device::CreateShaderResourceView); break;
			case RecDevice::tag_CreateUnorderedAccessView:					Replay(device, p,	&ID3D12Device::CreateUnorderedAccessView); break;
			case RecDevice::tag_CreateRenderTargetView:						Replay(device, p,	&ID3D12Device::CreateRenderTargetView); break;
			case RecDevice::tag_CreateDepthStencilView:						Replay(device, p,	&ID3D12Device::CreateDepthStencilView); break;
			case RecDevice::tag_CreateSampler:								Replay(device, p,	&ID3D12Device::CreateSampler); break;
			case RecDevice::tag_CopyDescriptors:							Replay2<void(UINT,counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0>, counted<const UINT,0>, UINT, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3>, counted<const UINT,3>, D3D12_DESCRIPTOR_HEAP_TYPE)>(device, p, &ID3D12Device::CopyDescriptors); break;
			case RecDevice::tag_CopyDescriptorsSimple:						Replay(device, p,	&ID3D12Device::CopyDescriptorsSimple); break;
			case RecDevice::tag_CreateCommittedResource:					ReplayPP(p,			&ID3D12Device::CreateCommittedResource); break;
			case RecDevice::tag_CreateHeap:									ReplayPP(p,			&ID3D12Device::CreateHeap); break;
			case RecDevice::tag_CreatePlacedResource:						ReplayPP(p,			&ID3D12Device::CreatePlacedResource); break;
			case RecDevice::tag_CreateReservedResource:						ReplayPP(p,			&ID3D12Device::CreateReservedResource); break;
			case RecDevice::tag_CreateSharedHandle:							ReplayPP(p,			&ID3D12Device::CreateSharedHandle); break;
			case RecDevice::tag_OpenSharedHandle://							ReplayPP(p,			&ID3D12Device::OpenSharedHandle); break;
				Replay(p,			[this](HANDLE handle, REFIID riid, void **pp) {
					void	*v0	= *pp;
					if (auto *obj = assets.FindObject((uint64)v0)) {
						void	*v1 = MakeLocal(obj);
						InitObject(v0, v1);
					}
				});
				break;
			case RecDevice::tag_OpenSharedHandleByName:						Replay(device, p,	&ID3D12Device::OpenSharedHandleByName); break;
			case RecDevice::tag_MakeResident:								Replay2<HRESULT(UINT,counted<ID3D12Pageable* const, 0>)>(device, p, &ID3D12Device::MakeResident); break;
			case RecDevice::tag_Evict:										Replay2<HRESULT(UINT,counted<ID3D12Pageable* const, 0>)>(device, p, &ID3D12Device::Evict); break;
			case RecDevice::tag_CreateFence:								ReplayPP(p,			&ID3D12Device::CreateFence); break;
			case RecDevice::tag_CreateQueryHeap:							ReplayPP(p,			&ID3D12Device::CreateQueryHeap); break;
			case RecDevice::tag_SetStablePowerState:						Replay(device, p,	&ID3D12Device::SetStablePowerState); break;
			case RecDevice::tag_CreateCommandSignature:						ReplayPP(p,			&ID3D12Device::CreateCommandSignature); break;
			//ID3D12Device1
			case RecDevice::tag_CreatePipelineLibrary:						Replay(device, p,	&ID3D12DeviceLatest::CreatePipelineLibrary); break;
			case RecDevice::tag_SetEventOnMultipleFenceCompletion:			Replay(device, p,	&ID3D12DeviceLatest::SetEventOnMultipleFenceCompletion); break;
			case RecDevice::tag_SetResidencyPriority:						Replay(device, p,	&ID3D12DeviceLatest::SetResidencyPriority); break;
			//ID3D12Device2
			case RecDevice::tag_CreatePipelineState:						Replay(device, p,	&ID3D12DeviceLatest::CreatePipelineState); break;
			//ID3D12Device3
			case RecDevice::tag_OpenExistingHeapFromAddress:				Replay(device, p,	&ID3D12DeviceLatest::OpenExistingHeapFromAddress); break;
			case RecDevice::tag_OpenExistingHeapFromFileMapping:			Replay(device, p,	&ID3D12DeviceLatest::OpenExistingHeapFromFileMapping); break;
			case RecDevice::tag_EnqueueMakeResident:						Replay(device, p,	&ID3D12DeviceLatest::EnqueueMakeResident); break;
			//ID3D12Device4
			case RecDevice::tag_CreateCommandList1:							Replay(device, p,	&ID3D12DeviceLatest::CreateCommandList1); break;
			case RecDevice::tag_CreateProtectedResourceSession:				Replay(device, p,	&ID3D12DeviceLatest::CreateProtectedResourceSession); break;
			case RecDevice::tag_CreateCommittedResource1:					Replay(device, p,	&ID3D12DeviceLatest::CreateCommittedResource1); break;
			case RecDevice::tag_CreateHeap1:								Replay(device, p,	&ID3D12DeviceLatest::CreateHeap1); break;
			case RecDevice::tag_CreateReservedResource1:					Replay(device, p,	&ID3D12DeviceLatest::CreateReservedResource1); break;
			//ID3D12Device5
			case RecDevice::tag_CreateLifetimeTracker:						Replay(device, p,	&ID3D12DeviceLatest::CreateLifetimeTracker); break;
			case RecDevice::tag_CreateMetaCommand:							Replay(device, p,	&ID3D12DeviceLatest::CreateMetaCommand); break;
			case RecDevice::tag_CreateStateObject:							Replay(device, p,	&ID3D12DeviceLatest::CreateStateObject); break;
			//ID3D12Device6
			case RecDevice::tag_SetBackgroundProcessingMode:				Replay(device, p,	&ID3D12DeviceLatest::SetBackgroundProcessingMode); break;
			//ID3D12Device7
			case RecDevice::tag_AddToStateObject:							Replay(device, p,	&ID3D12DeviceLatest::AddToStateObject); break;
			case RecDevice::tag_CreateProtectedResourceSession1:			Replay(device, p,	&ID3D12DeviceLatest::CreateProtectedResourceSession1); break;
			//ID3D12Device8
			case RecDevice::tag_CreateCommittedResource2:					Replay(device, p,	&ID3D12DeviceLatest::CreateCommittedResource2); break;
			case RecDevice::tag_CreatePlacedResource1:						Replay(device, p,	&ID3D12DeviceLatest::CreatePlacedResource1); break;
			case RecDevice::tag_CreateSamplerFeedbackUnorderedAccessView:	Replay(device, p,	&ID3D12DeviceLatest::CreateSamplerFeedbackUnorderedAccessView); break;
		}
	}

	void	Process(ID3D12GraphicsCommandList *cmd, uint16 id, const void *p) {
		switch (id) {
			//ID3D12GraphicsCommandList
			case RecCommandList::tag_Close:									Replay(cmd, p, &ID3D12GraphicsCommandList::Close); cmd_infos[cmd].closed = false; break;
			case RecCommandList::tag_Reset:									Replay(cmd, p, &ID3D12GraphicsCommandList::Reset); break;
			case RecCommandList::tag_ClearState:							Replay(cmd, p, &ID3D12GraphicsCommandList::ClearState); break;
			case RecCommandList::tag_DrawInstanced:							Replay(cmd, p, &ID3D12GraphicsCommandList::DrawInstanced); break;
			case RecCommandList::tag_DrawIndexedInstanced:					Replay(cmd, p, &ID3D12GraphicsCommandList::DrawIndexedInstanced); break;
			case RecCommandList::tag_Dispatch:								Replay(cmd, p, &ID3D12GraphicsCommandList::Dispatch); break;
			case RecCommandList::tag_CopyBufferRegion:						Replay(cmd, p, &ID3D12GraphicsCommandList::CopyBufferRegion); break;
			case RecCommandList::tag_CopyTextureRegion:						Replay(cmd, p, &ID3D12GraphicsCommandList::CopyTextureRegion); break;
			case RecCommandList::tag_CopyResource:							Replay(cmd, p, &ID3D12GraphicsCommandList::CopyResource); break;
			case RecCommandList::tag_CopyTiles:								Replay(cmd, p, &ID3D12GraphicsCommandList::CopyTiles); break;
			case RecCommandList::tag_ResolveSubresource:					Replay(cmd, p, &ID3D12GraphicsCommandList::ResolveSubresource); break;
			case RecCommandList::tag_IASetPrimitiveTopology:				Replay(cmd, p, &ID3D12GraphicsCommandList::IASetPrimitiveTopology); break;
			case RecCommandList::tag_RSSetViewports:						Replay(cmd, p, &ID3D12GraphicsCommandList::RSSetViewports); break;
			case RecCommandList::tag_RSSetScissorRects:						Replay(cmd, p, &ID3D12GraphicsCommandList::RSSetScissorRects); break;
			case RecCommandList::tag_OMSetBlendFactor:						Replay(cmd, p, &ID3D12GraphicsCommandList::OMSetBlendFactor); break;
			case RecCommandList::tag_OMSetStencilRef:						Replay(cmd, p, &ID3D12GraphicsCommandList::OMSetStencilRef); break;
			case RecCommandList::tag_SetPipelineState:						Replay(cmd, p, &ID3D12GraphicsCommandList::SetPipelineState); break;
			case RecCommandList::tag_ResourceBarrier:						Replay2<void(UINT,counted<const D3D12_RESOURCE_BARRIER,0>)>(cmd, p, &ID3D12GraphicsCommandList::ResourceBarrier); break;
			case RecCommandList::tag_ExecuteBundle:							Replay(cmd, p, &ID3D12GraphicsCommandList::ExecuteBundle); break;
			case RecCommandList::tag_SetDescriptorHeaps:					Replay2<void(UINT,counted<ID3D12DescriptorHeap *const, 0>)>(cmd, p, &ID3D12GraphicsCommandList::SetDescriptorHeaps); break;
			case RecCommandList::tag_SetComputeRootSignature:				Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootSignature); break;
			case RecCommandList::tag_SetGraphicsRootSignature:				Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootSignature); break;
			case RecCommandList::tag_SetComputeRootDescriptorTable:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable); break;
			case RecCommandList::tag_SetGraphicsRootDescriptorTable:		Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable); break;
			case RecCommandList::tag_SetComputeRoot32BitConstant:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRoot32BitConstant); break;
			case RecCommandList::tag_SetGraphicsRoot32BitConstant:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant); break;
			case RecCommandList::tag_SetComputeRoot32BitConstants:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetComputeRoot32BitConstants); break;
			case RecCommandList::tag_SetGraphicsRoot32BitConstants:			Replay(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants); break;
			case RecCommandList::tag_SetComputeRootConstantBufferView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootConstantBufferView); break;
			case RecCommandList::tag_SetGraphicsRootConstantBufferView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView); break;
			case RecCommandList::tag_SetComputeRootShaderResourceView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootShaderResourceView); break;
			case RecCommandList::tag_SetGraphicsRootShaderResourceView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView); break;
			case RecCommandList::tag_SetComputeRootUnorderedAccessView:		Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView); break;
			case RecCommandList::tag_SetGraphicsRootUnorderedAccessView:	Replay2<void(UINT,lookedup<D3D12_GPU_VIRTUAL_ADDRESS>)>(cmd, p, &ID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView); break;
			case RecCommandList::tag_IASetIndexBuffer:						Replay(cmd, p, &ID3D12GraphicsCommandList::IASetIndexBuffer); break;
			case RecCommandList::tag_IASetVertexBuffers:					Replay2<void(UINT,UINT,counted<const D3D12_VERTEX_BUFFER_VIEW,1>)>(cmd, p, &ID3D12GraphicsCommandList::IASetVertexBuffers); break;
			case RecCommandList::tag_SOSetTargets:							Replay2<void(UINT,UINT,counted<const D3D12_STREAM_OUTPUT_BUFFER_VIEW,1>)>(cmd, p, &ID3D12GraphicsCommandList::SOSetTargets); break;
			case RecCommandList::tag_OMSetRenderTargets:					Replay2<void(UINT,counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0>,BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*)>(cmd, p, &ID3D12GraphicsCommandList::OMSetRenderTargets); break;
			case RecCommandList::tag_ClearDepthStencilView:					Replay(cmd, p, &ID3D12GraphicsCommandList::ClearDepthStencilView); break;
			case RecCommandList::tag_ClearRenderTargetView:					Replay2<void(D3D12_CPU_DESCRIPTOR_HANDLE,array<float, 4>,UINT,const D3D12_RECT*)>(cmd, p, &ID3D12GraphicsCommandList::ClearRenderTargetView); break;
			case RecCommandList::tag_ClearUnorderedAccessViewUint:			Replay(cmd, p, &ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint); break;
			case RecCommandList::tag_ClearUnorderedAccessViewFloat:			Replay(cmd, p, &ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat); break;
			case RecCommandList::tag_DiscardResource:						Replay(cmd, p, &ID3D12GraphicsCommandList::DiscardResource); break;
			case RecCommandList::tag_BeginQuery:							Replay(cmd, p, &ID3D12GraphicsCommandList::BeginQuery); break;
			case RecCommandList::tag_EndQuery:								Replay(cmd, p, &ID3D12GraphicsCommandList::EndQuery); break;
			case RecCommandList::tag_ResolveQueryData:						Replay(cmd, p, &ID3D12GraphicsCommandList::ResolveQueryData); break;
			case RecCommandList::tag_SetPredication:						Replay(cmd, p, &ID3D12GraphicsCommandList::SetPredication); break;
			case RecCommandList::tag_SetMarker:								Replay2<void(UINT,counted<const uint8,2>,UINT)>(cmd, p, &ID3D12GraphicsCommandList::SetMarker); break;
			case RecCommandList::tag_BeginEvent:							Replay2<void(UINT,counted<const uint8,2>,UINT)>(cmd, p, &ID3D12GraphicsCommandList::BeginEvent); break;
			case RecCommandList::tag_EndEvent:								Replay(cmd, p, &ID3D12GraphicsCommandList::EndEvent); break;
			case RecCommandList::tag_ExecuteIndirect:						Replay(cmd, p, &ID3D12GraphicsCommandList::ExecuteIndirect); break;
			//ID3D12GraphicsCommandList1
			case RecCommandList::tag_AtomicCopyBufferUINT:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::AtomicCopyBufferUINT); break;
			case RecCommandList::tag_AtomicCopyBufferUINT64:				Replay(cmd, p, &ID3D12GraphicsCommandListLatest::AtomicCopyBufferUINT64); break;
			case RecCommandList::tag_OMSetDepthBounds:						Replay(cmd, p, &ID3D12GraphicsCommandListLatest::OMSetDepthBounds); break;
			case RecCommandList::tag_SetSamplePositions:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetSamplePositions); break;
			case RecCommandList::tag_ResolveSubresourceRegion:				Replay(cmd, p, &ID3D12GraphicsCommandListLatest::ResolveSubresourceRegion); break;
			case RecCommandList::tag_SetViewInstanceMask:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetViewInstanceMask); break;
			//ID3D12GraphicsCommandList2
			case RecCommandList::tag_WriteBufferImmediate:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::WriteBufferImmediate); break;
			//ID3D12GraphicsCommandList3
			case RecCommandList::tag_SetProtectedResourceSession:			Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetProtectedResourceSession); break;
			//ID3D12GraphicsCommandList4
			case RecCommandList::tag_BeginRenderPass:						Replay(cmd, p, &ID3D12GraphicsCommandListLatest::BeginRenderPass); break;
			case RecCommandList::tag_EndRenderPass:							Replay(cmd, p, &ID3D12GraphicsCommandListLatest::EndRenderPass); break;
			case RecCommandList::tag_InitializeMetaCommand:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::InitializeMetaCommand); break;
			case RecCommandList::tag_ExecuteMetaCommand:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::ExecuteMetaCommand); break;
			case RecCommandList::tag_BuildRaytracingAccelerationStructure:	Replay(cmd, p, &ID3D12GraphicsCommandListLatest::BuildRaytracingAccelerationStructure); break;
			case RecCommandList::tag_EmitRaytracingAccelerationStructurePostbuildInfo:	Replay(cmd, p, &ID3D12GraphicsCommandListLatest::EmitRaytracingAccelerationStructurePostbuildInfo); break;
			case RecCommandList::tag_CopyRaytracingAccelerationStructure:	Replay(cmd, p, &ID3D12GraphicsCommandListLatest::CopyRaytracingAccelerationStructure); break;
			case RecCommandList::tag_SetPipelineState1:						Replay(cmd, p, &ID3D12GraphicsCommandListLatest::SetPipelineState1); break;
			case RecCommandList::tag_DispatchRays:							Replay(cmd, p, &ID3D12GraphicsCommandListLatest::DispatchRays); break;
			//ID3D12GraphicsCommandList5
			case RecCommandList::tag_RSSetShadingRate:						Replay(cmd, p, &ID3D12GraphicsCommandListLatest::RSSetShadingRate); break;
			case RecCommandList::tag_RSSetShadingRateImage:					Replay(cmd, p, &ID3D12GraphicsCommandListLatest::RSSetShadingRateImage); break;
			//ID3D12GraphicsCommandList6
			case RecCommandList::tag_DispatchMesh:							Replay(cmd, p, &ID3D12GraphicsCommandListLatest::DispatchMesh); break;
		}
	}

	void	*MakeLocal(DX12Assets::ObjectRecord *obj) {
		void		*r = 0;
		switch (obj->type) {
			case RecObject::Resource: {
				D3D12_RESOURCE_DESC			desc = *obj->info;
				if (desc.Alignment && desc.Alignment != 0x10000)
					desc.Alignment = 0;

				D3D12_HEAP_PROPERTIES	heap;
				heap.Type					= D3D12_HEAP_TYPE_DEFAULT;
				heap.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heap.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
				heap.CreationNodeMask		= 1;
				heap.VisibleNodeMask		= 1;

				device->CreateCommittedResource(
					&heap, D3D12_HEAP_FLAG_NONE,
					&desc, D3D12_RESOURCE_STATE_COMMON, 0,
					__uuidof(ID3D12Resource), &r
				);
				unconst(*obj->info) = ((ID3D12Resource*)r)->GetDesc();
				break;
			}
			case RecObject::Handle:
				r = CreateEvent(nullptr, false, false, nullptr);
				break;

			case RecObject::PipelineState:
				switch (*(int*)obj->info) {
					case 0:
						Replay(obj->info + 8, [this, &r](const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc) {
							device->CreateGraphicsPipelineState(desc, __uuidof(ID3D12PipelineState), &r);
						});
						break;
					case 1:
						Replay(obj->info + 8, [this, &r](const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc) {
							device->CreateComputePipelineState(desc, __uuidof(ID3D12PipelineState), &r);
						});
						break;
					case 2:
						Replay(obj->info + 8, [this, &r](const D3D12_PIPELINE_STATE_STREAM_DESC *desc) {
							device.as<ID3D12Device2>()->CreatePipelineState(desc, __uuidof(ID3D12PipelineState), &r);
						});
						break;

				}
				break;

			case RecObject::RootSignature:			device->CreateRootSignature(1, obj->info, obj->info.length(), __uuidof(ID3D12RootSignature), &r); break;
			case RecObject::Heap:					device->CreateHeap(obj->info, __uuidof(ID3D12Heap), &r); break;
			case RecObject::CommandAllocator:		device->CreateCommandAllocator(*obj->info, __uuidof(ID3D12CommandAllocator), &r); break;
			case RecObject::Fence:					device->CreateFence(*obj->info, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &r); break;
			case RecObject::DescriptorHeap:			device->CreateDescriptorHeap(obj->info, __uuidof(ID3D12DescriptorHeap), &r); break;
			case RecObject::QueryHeap:				device->CreateQueryHeap(obj->info, __uuidof(ID3D12QueryHeap), &r); break;
			case RecObject::GraphicsCommandList: {
				const RecCommandList	*rec = obj->info;
				device->CreateCommandList(rec->node_mask, rec->list_type, _lookup(rec->allocator), _lookup(rec->state), __uuidof(ID3D12GraphicsCommandList), &r);
				break;
			}
			case RecObject::CommandQueue:			device->CreateCommandQueue(obj->info, __uuidof(ID3D12CommandQueue), &r); break;
#if 0
			case RecObject::CommandSignature:		device->CreateCommandSignature(obj->info, __uuidof(ID3D12CommandSignature), &r); break;
			case RecObject::Device:					device->CreateDevice(obj->info, __uuidof(ID3D12), &r); break;
#endif
			default:
				ISO_ASSERT(false);

		}
		return r;
	}

	void	Wait(ID3D12CommandQueue *q) {
		q->Signal(fence, fence_value);
		fence->SetEventOnCompletion(fence_value++, fence_event);
		fence_event.wait();
	}

	void	Wait(ID3D12CommandAllocator *a) {
		for (auto &c : alloc_cmds[a]) {
			auto	&info = cmd_infos[c];
			if (info.closed) {
				c->Close();
				info.closed = false;
			}
			for (auto &q : info)
				Wait(q);
			info.clear();
		}
	}

	void	WaitAll() {
		for (auto &i : alloc_cmds.with_keys())
			Wait(i.key());
	}

	DX12Replay(DX12Assets &_assets, dx::cache_type &_cache, IDXGIAdapter1 *adapter = 0) : assets(_assets), cache(_cache), current_cmd(0), current_queue(0) {
		Init(adapter);
		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
			descriptor_sizes[i] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));

		device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fence);
		fence_value	= 0;

	}
	~DX12Replay() {
		WaitAll();
		for (auto i : obj2local) {
			if (i) {
				if (uintptr_t(i) < (uint64(1) << 32))
					CloseHandle(i);
				else
					((IUnknown*)i)->Release();
			}
		}
	}
	void	RunTo(uint32 addr);
	void	RunGen();

	dx::SimulatorDXBC::Resource GetResource(const DESCRIPTOR *d, dx::cache_type &cache);
};

#if 0
void DX12Replay::RunTo(uint32 addr) {
	bool		done	= false;

	for (const COMRecording::header *p = assets.device_object->info, *e = assets.device_object->info.end(); !done && p < e; p = p->next()) {
		//ISO_TRACEF("replay addr = 0x") << hex((char*)p - (char*)assets.device_object->info) << '\n';
		try {
			if (p->id == 0xffff) {
				void	*obj0	= *(void**)p->data();
				void	*obj	= _lookup(obj0);
				p = p->next();

				switch (p->id) {
					//ID3D12CommandQueue
					case RecDevice::tag_CommandQueueUpdateTileMappings:	Replay(obj, p->data(), &ID3D12CommandQueue::UpdateTileMappings); break;
					case RecDevice::tag_CommandQueueCopyTileMappings:	Replay(obj, p->data(), &ID3D12CommandQueue::CopyTileMappings); break;

					case RecDevice::tag_CommandQueueExecuteCommandLists:
						COMParse(p->data(), [&](UINT NumCommandLists, counted<CommandRange,0> pp) {
							for (int i = 0; i < NumCommandLists; i++) {
								uint32			end		= addr;
								memory_block	mem		= GetCommands(pp[i], assets, end);
								auto			*rec	= assets.FindObject((uint64)(const ID3D12CommandList*)pp[i]);
								if (!rec)
									continue;
								auto			*cmd	= (ID3D12GraphicsCommandList*)obj2local[rec->obj].or_default();

								cmd_infos[cmd].insert((ID3D12CommandQueue*)obj);

								for (const COMRecording::header *p = mem, *e = mem.end(); p < e; p = p->next())
									Process(cmd, p->id, p->data());

								done = end == mem.length();
								if (done) {
									cmd->Close();
									cmd_infos[cmd].closed = false;
								}
							}
						});
						Replay(obj, p->data(), &ID3D12CommandQueue::ExecuteCommandLists);
						break;

					case RecDevice::tag_CommandQueueSetMarker:			Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p->data(), &ID3D12CommandQueue::SetMarker); break;
					case RecDevice::tag_CommandQueueBeginEvent:			Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p->data(), &ID3D12CommandQueue::BeginEvent); break;
					case RecDevice::tag_CommandQueueEndEvent:			Replay(obj, p->data(), &ID3D12CommandQueue::EndEvent); break;
					case RecDevice::tag_CommandQueueSignal:				Replay(obj, p->data(), &ID3D12CommandQueue::Signal); break;
					case RecDevice::tag_CommandQueueWait:				Replay(obj, p->data(), &ID3D12CommandQueue::Wait); break;
					//ID3D12CommandAllocator
					case RecDevice::tag_CommandAllocatorReset: {
						Wait((ID3D12CommandAllocator*)obj);
						Replay(obj, p->data(), &ID3D12CommandAllocator::Reset);
						break;
					};
					//ID3D12Fence
					case RecDevice::tag_FenceSetEventOnCompletion:		Replay(obj, p->data(), &ID3D12Fence::SetEventOnCompletion); break;
					case RecDevice::tag_FenceSignal:					Replay(obj, p->data(), &ID3D12Fence::Signal); break;
					case RecDevice::tag_WaitForSingleObjectEx:			Replay(p->data(), [obj](DWORD dwMilliseconds, BOOL bAlertable) {
						WaitForSingleObjectEx(obj, dwMilliseconds, bAlertable);
					});
						break;
					//ID3D12GraphicsCommandList
					case RecDevice::tag_GraphicsCommandListClose: {
						auto	*cmd	= (ID3D12GraphicsCommandList*)obj;
						cmd_infos[cmd].closed = true;
						break;
					}

					case RecDevice::tag_GraphicsCommandListReset:		Replay(p->data(), [this, obj](ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
						auto	*cmd	= (ID3D12GraphicsCommandList*)obj;
						auto	&info	= cmd_infos[cmd];
						if (info.closed) {
							cmd->Close();
							info.closed = false;
						}
						alloc_cmds[pAllocator].insert(cmd);
					});
						break;

				}
			} else {
				Process(p->id, p->data());
			}

		} catch (const char *error) {
			const char *e = string_end(error);
			while (--e > error && *e == '\r' || *e == '\n' || *e == '.' || *e == ' ')
				;
			ISO_OUTPUTF(str(error, e + 1)) << " at offset 0x" << hex((char*)p - (char*)assets.device_object->info) << '\n';
		}
	}
}

#else
void DX12Replay::RunTo(uint32 addr) {
	fiber_generator<uint32>	fg([this]() { RunGen(); }, 1 << 17);
	for (auto i = fg.begin(); *i < addr; ++i)
		;
}
#endif

void DX12Replay::RunGen() {
	bool		done	= false;

	for (const COMRecording::header *p = assets.device_object->info, *e = assets.device_object->info.end(); !done && p < e; p = p->next()) {
		//ISO_TRACEF("replay addr = 0x") << hex((char*)p - (char*)assets.device_object->info) << '\n';
		try {
			if (p->id == 0xffff) {
				void	*obj0	= *(void**)p->data();
				void	*obj	= _lookup(obj0);
				p = p->next();

				switch (p->id) {
					//ID3D12CommandQueue
					case RecDevice::tag_CommandQueueUpdateTileMappings:	Replay(obj, p->data(), &ID3D12CommandQueue::UpdateTileMappings); break;
					case RecDevice::tag_CommandQueueCopyTileMappings:	Replay(obj, p->data(), &ID3D12CommandQueue::CopyTileMappings); break;

					case RecDevice::tag_CommandQueueExecuteCommandLists: {
						auto	save_queue = save(current_queue, (ID3D12CommandQueue*)obj);

						COMParse(p->data(), [&](UINT NumCommandLists, counted<CommandRange,0> pp) {
							for (int i = 0; i < NumCommandLists; i++) {
								auto	&range	= pp[i];
								if (auto *rec = assets.FindObject(uint64(&*range))) {
									const RecCommandList*	com		= rec->info;
									memory_block			mem		= com->buffer.slice(range.begin(), range.extent());
									auto*			cmd		= (ID3D12GraphicsCommandList*)obj2local[rec->obj].or_default();
									intptr_t		offset	= rec->index - intptr_t((void*)com->buffer);

									cmd_infos[cmd].insert((ID3D12CommandQueue*)obj);

									auto	save_cmd = save(current_cmd, cmd);
									for (const COMRecording::header *p = mem, *e = mem.end(); p < e; p = p->next()) {
										if (!fiber::yield(intptr_t(p) + offset)) {
											cmd->Close();
											cmd_infos[cmd].closed = false;
											done = true;
											break;
										}
										Process(cmd, p->id, p->data());
									}
								}
							}
						});
						Replay(obj, p->data(), &ID3D12CommandQueue::ExecuteCommandLists);
						break;
					}

					case RecDevice::tag_CommandQueueSetMarker:			Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p->data(), &ID3D12CommandQueue::SetMarker); break;
					case RecDevice::tag_CommandQueueBeginEvent:			Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p->data(), &ID3D12CommandQueue::BeginEvent); break;
					case RecDevice::tag_CommandQueueEndEvent:			Replay(obj, p->data(), &ID3D12CommandQueue::EndEvent); break;
					case RecDevice::tag_CommandQueueSignal:				Replay(obj, p->data(), &ID3D12CommandQueue::Signal); break;
					case RecDevice::tag_CommandQueueWait:				Replay(obj, p->data(), &ID3D12CommandQueue::Wait); break;
					//ID3D12CommandAllocator
					case RecDevice::tag_CommandAllocatorReset: {
						Wait((ID3D12CommandAllocator*)obj);
						Replay(obj, p->data(), &ID3D12CommandAllocator::Reset);
						break;
					};
					//ID3D12Fence
					case RecDevice::tag_FenceSetEventOnCompletion:		Replay(obj, p->data(), &ID3D12Fence::SetEventOnCompletion); break;
					case RecDevice::tag_FenceSignal:					Replay(obj, p->data(), &ID3D12Fence::Signal); break;
					case RecDevice::tag_WaitForSingleObjectEx:			Replay(p->data(), [obj](DWORD dwMilliseconds, BOOL bAlertable) {
						WaitForSingleObjectEx(obj, dwMilliseconds, bAlertable);
					});
						break;
					//ID3D12GraphicsCommandList
					case RecDevice::tag_GraphicsCommandListClose: {
						auto	*cmd	= (ID3D12GraphicsCommandList*)obj;
						cmd_infos[cmd].closed = true;
						break;
					}

					case RecDevice::tag_GraphicsCommandListReset:		Replay(p->data(), [this, obj](ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
						auto	*cmd	= (ID3D12GraphicsCommandList*)obj;
						auto	&info	= cmd_infos[cmd];
						if (info.closed) {
							cmd->Close();
							info.closed = false;
						}
						alloc_cmds[pAllocator].insert(cmd);
					});
						break;

				}
			} else {
				Process(p->id, p->data());
			}

		} catch (const char *error) {
			const char *e = string_end(error);
			while (--e > error && *e == '\r' || *e == '\n' || *e == '.' || *e == ' ')
				;
			ISO_OUTPUTF(str(error, e + 1)) << " at offset 0x" << hex((const char*)p - (const char*)assets.device_object->info) << '\n';
		}
	}
	fiber::yield(~0u);
}

dx::SimulatorDXBC::Resource DX12Replay::GetResource(const DESCRIPTOR *d, dx::cache_type &cache) {
	WaitAll();

	dx::SimulatorDXBC::Resource	r;
	auto					*obj	= assets.FindObject((uint64)d->res);
	auto					*pres	= obj2local.check(obj->obj);
	if (!pres)
		return r;

	ID3D12Resource			*res	= (ID3D12Resource*)*pres;
	const RecResource		*rr		= obj->info;
	int						sub		= 0, nsub = 1;
	int						mip		= 0;
	D3D12_RESOURCE_STATES	state	= D3D12_RESOURCE_STATE_COMMON;
	DXGI_FORMAT				format;

	switch (d->type) {
		case DESCRIPTOR::RTV: {
			auto	&rtv	= d->rtv;
			format			= rtv.Format;
			state			= D3D12_RESOURCE_STATE_RENDER_TARGET;

			switch (rtv.ViewDimension) {
				case D3D12_RTV_DIMENSION_BUFFER:
					//r.offset(rtv.Buffer.FirstElement);
					break;

				case D3D12_RTV_DIMENSION_TEXTURE1D:
					sub = rr->CalcSubresource(rtv.Texture1D.MipSlice, 0, 0);
					mip = rtv.Texture1D.MipSlice;
					break;

				case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
					sub = rr->CalcSubresource(rtv.Texture1DArray.MipSlice, rtv.Texture1DArray.FirstArraySlice, 0);
					mip = rtv.Texture1DArray.MipSlice;
					break;

				case D3D12_RTV_DIMENSION_TEXTURE2D:
					sub = rr->CalcSubresource(rtv.Texture2D.MipSlice, 0, 0);
					mip = rtv.Texture2D.MipSlice;
					break;

				case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
					sub = rr->CalcSubresource(rtv.Texture2DArray.MipSlice, rtv.Texture2DArray.FirstArraySlice, 0);
					mip = rtv.Texture2DArray.MipSlice;
					break;

				case D3D12_RTV_DIMENSION_TEXTURE2DMS:
					break;

				case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
					sub = rr->CalcSubresource(0, rtv.Texture2DMSArray.FirstArraySlice, 0);
					break;

				case D3D12_RTV_DIMENSION_TEXTURE3D:
					sub = rr->CalcSubresource(rtv.Texture3D.MipSlice, rtv.Texture3D.FirstWSlice, 0);
					mip = rtv.Texture3D.MipSlice;
					break;
			}
			break;
		}
		case DESCRIPTOR::DSV: {
			auto	&dsv	= d->dsv;
			format			= dsv.Format;
			state			= D3D12_RESOURCE_STATE_DEPTH_WRITE;

			switch (dsv.ViewDimension) {
				case D3D12_DSV_DIMENSION_TEXTURE1D:
					sub = rr->CalcSubresource(dsv.Texture1D.MipSlice, 0, 0);
					break;

				case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
					sub = rr->CalcSubresource(dsv.Texture1DArray.MipSlice, dsv.Texture1DArray.FirstArraySlice, 0);
					break;

				case D3D12_DSV_DIMENSION_TEXTURE2D:
					sub = rr->CalcSubresource(dsv.Texture2D.MipSlice, 0, 0);
					break;

				case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
					sub = rr->CalcSubresource(dsv.Texture2DArray.MipSlice, dsv.Texture2DArray.FirstArraySlice, 0);
					break;

				case D3D12_DSV_DIMENSION_TEXTURE2DMS:
					break;

				case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
					sub = rr->CalcSubresource(0, dsv.Texture2DMSArray.FirstArraySlice, 0);
					break;
			}
			break;
		}
	}

	DXGI_COMPONENTS	format2 = format;
	int	width				= mip_size(format2, rr->Width, mip);
	int	height				= mip_size(format2, rr->Height, mip);

#if 0
	D3D12_RESOURCE_DESC	desc;
	clear(desc);
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Width				= size(format2) * width * height;
	desc.Height				= 1;
	desc.DepthOrArraySize	= 1;
	desc.MipLevels			= 1;
//	desc.Format				= format;
	desc.SampleDesc.Count	= 1;

	D3D12_HEAP_PROPERTIES	heap_props;
	heap_props.Type					= D3D12_HEAP_TYPE_READBACK;
	heap_props.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
	heap_props.CreationNodeMask		= 1;
	heap_props.VisibleNodeMask		= 1;

	com_ptr<ID3D12Resource>	res2;
	device->CreateCommittedResource(
		&heap_props,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		0,
		__uuidof(ID3D12Resource), (void**)&res2
	);
#endif

	r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, format2, width, height, 1, 1);
	
	com_ptr<ID3D12CommandQueue>			cmd_queue;
	D3D12_COMMAND_QUEUE_DESC	qdesc = {D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_FLAG_NONE, 1};
	device->CreateCommandQueue(&qdesc, __uuidof(ID3D12CommandQueue), (void**)&cmd_queue);

	com_ptr<ID3D12CommandAllocator>		cmd_alloc;
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&cmd_alloc);

	com_ptr<ID3D12GraphicsCommandList>	cmd_list;
	device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc, 0, __uuidof(ID3D12GraphicsCommandList), (void**)&cmd_list);

	auto	*footprints = alloc_auto(D3D12_PLACED_SUBRESOURCE_FOOTPRINT, nsub);
	uint64	total_size;
	device->GetCopyableFootprints(rr, sub, nsub, 0, footprints, 0, 0, &total_size);

	D3D12_RESOURCE_DESC	rdesc;
	clear(rdesc);
	rdesc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	rdesc.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rdesc.Width				= total_size;
	rdesc.Height			= 1;
	rdesc.DepthOrArraySize	= 1;
	rdesc.MipLevels			= 1;
//	rdesc.Format			= format;
	rdesc.SampleDesc.Count	= 1;

	com_ptr<ID3D12Resource>	res2;
	D3D12_HEAP_PROPERTIES	heap_props;
	heap_props.Type					= D3D12_HEAP_TYPE_READBACK;
	heap_props.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
	heap_props.CreationNodeMask		= 1;
	heap_props.VisibleNodeMask		= 1;
	device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &rdesc, D3D12_RESOURCE_STATE_COPY_DEST, 0, __uuidof(ID3D12Resource), (void**)&res2);

	D3D12_RESOURCE_BARRIER	b;
	b.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Flags						= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	b.Transition.pResource		= res;
	b.Transition.StateBefore	= state;
	b.Transition.StateAfter		= D3D12_RESOURCE_STATE_COPY_SOURCE;
	b.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmd_list->ResourceBarrier(1, &b);

	D3D12_TEXTURE_COPY_LOCATION	srcloc, dstloc;
	srcloc.pResource		= res;
	srcloc.Type				= D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcloc.SubresourceIndex	= 0;

	dstloc.pResource		= res2;
	dstloc.Type				= D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dstloc.PlacedFootprint	= footprints[0];

	cmd_list->CopyTextureRegion(&dstloc, 0, 0, 0, &srcloc, 0);
	cmd_list->Close();

	ID3D12CommandList	*cmd_list0 = cmd_list;
	cmd_queue->ExecuteCommandLists(1, &cmd_list0);

	Wait(cmd_queue);

	void	*p;
	res2->Map(0, 0, &p);

#if 1
	r.set_mem(malloc_block(memory_block(p, r.length())).detach());
#else
	if (!rr->gpu) {
	}
	auto	block	= rr->GetSubresource(device, sub);
	r.set_mem(cache(rr->gpu + block.offset, r.length()));
#endif

	res2->Unmap(0, 0);
	return r;
}

//-----------------------------------------------------------------------------
//	DX12Connection
//-----------------------------------------------------------------------------

struct DX12Capture : anything {};

struct DX12Connection : DX12Assets {
	dx::cache_type		cache;
	CallStackDumper		stack_dumper;
	uint64				frequency;

	uint64					CommandTotal();
	void					GetAssets(progress prog, uint64 total);
	bool					GetStatistics();
	ISO_ptr_machine<void>	GetBitmap(const DESCRIPTOR &d);
	ISO_ptr_machine<void>	GetBitmap(const ResourceRecord *r) { return GetBitmap(MakeDefaultDescriptor(r->obj)); }
	void					GetStateAt(DX12State &state, uint32 addr);
	void					Load(const DX12Capture *cap);
	bool					Save(const filename &fn);

	DX12Connection() : stack_dumper(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES), frequency(0) {}
	DX12Connection(HANDLE process, const char *executable) : stack_dumper(process, SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES, BuildSymbolSearchPath(filename(executable).dir())), frequency(0) {
		shader_path = filename(executable).rem_dir().rem_dir().rem_dir().rem_dir();
	}

	void	AddMemory(uint64 addr, const memory_block &mem)	{ cache.add_block(addr, mem); }
	uint32	FindDescriptorChange(uint64 h, uint32 stride);
	uint32	FindObjectCreation(void *obj);
};

struct DX12CaptureModule {
	uint64			a, b;
	string			fn;
	DX12CaptureModule(const CallStackDumper::Module &m) : a(m.a), b(m.b), fn(m.fn) {}
};

struct DX12CaptureObject {
	uint32			type;
	string16		name;
	xint64			obj;
	ISO_openarray<uint8>	info;
	DX12CaptureObject(const DX12Assets::ObjectRecord &o) : type(o.type), name(o.name), obj((uint64)o.obj), info(o.info.size32()) {
		memcpy(info, o.info, o.info.size32());
	}
};

struct DX12CaptureShader {
	uint32			stage;
	string16		name;
	xint64			addr;
	ISO_openarray<uint8>	code;
	DX12CaptureShader(const DX12Assets::ShaderRecord &o) : stage(o.stage), name(o.name), addr(o.addr), code(o.data.size32()) {
		memcpy(code, o.data, o.data.size32());
	}
};
ISO_DEFUSERF(DX12Capture, anything, ISO::TypeUser::WRITETOBIN);

ISO_DEFUSERCOMPFV(DX12CaptureModule, ISO::TypeUser::WRITETOBIN, a, b, fn);
ISO_DEFUSERCOMPFV(DX12CaptureObject, ISO::TypeUser::WRITETOBIN, type, name, obj, info);
ISO_DEFUSERCOMPFV(DX12CaptureShader, ISO::TypeUser::WRITETOBIN, stage, name, addr, code);

bool DX12Connection::Save(const filename &fn) {
	ISO_ptr<DX12Capture>	p(0);

	p->Append(ISO_ptr<ISO_openarray<DX12CaptureModule>>("Modules", stack_dumper.GetModules()));
	p->Append(ISO_ptr<ISO_openarray<DX12CaptureObject>>("Objects", objects));
	p->Append(ISO_ptr<ISO_openarray<DX12CaptureShader>>("Shaders", shaders));

	for (auto &i : cache)
		p->Append(ISO::ptr<ISO::VStartBin<memory_block>>(NULL, i.start, memory_block(i.data(), i.size())));

	return FileHandler::Write(p, fn);
}

memory_block get_memory(ISO::Browser b) {
	return memory_block(b[0], b.Count() * b[0].GetSize());
}

void DX12Connection::Load(const DX12Capture *cap) {
	ISO_ptr<ISO_openarray<DX12CaptureObject>>	_objects;
	ISO_ptr<ISO_openarray<DX12CaptureShader>>	_shaders;

	for (int i = 0, n = cap->Count(); i < n; i++) {
		ISO_ptr<void>	p = (*cap)[i];
		if (p.IsID("Modules")) {
			ISO_openarray<DX12CaptureModule>	*modules = p;
			for (auto &i : *modules)
				stack_dumper.AddModule(i.fn, i.a, i.b);

		} else if (p.IsID("Objects")) {
			_objects = p;

		} else if (p.IsID("Shaders")) {
			_shaders = p;

		} else {
			ISO::Browser	b(p);
			AddMemory(b[0].Get<uint64>(0), get_memory(b[1]));
		}
	}

	for (auto &i : *_shaders)
		AddShader((dx::SHADERSTAGE)i.stage, string16(i.name), memory_block(i.code, i.code.size()), i.addr);

	objects.reserve(_objects->Count());
	for (auto &i : *_objects)
		AddObject((RecObject::TYPE)i.type, string16(i.name), memory_block(i.info, i.info.size()), (void*)i.obj.i, cache, 0);
}

void GetDescriptorsForBatch(DX12Assets &assets, const DX12State &state, DX12Assets::ShaderRecord *shader) {
	D3D12_SHADER_DESC				desc;
	com_ptr<ID3D12ShaderReflection>	refl;

	if (SUCCEEDED(D3DReflect(shader->data, shader->data.length(), IID_ID3D12ShaderReflection, (void**)&refl))
	&&	SUCCEEDED(refl->GetDesc(&desc))
	) {
		for (int i = 0; i < desc.BoundResources; i++) {
			D3D12_SHADER_INPUT_BIND_DESC	res_desc;
			refl->GetResourceBindingDesc(i, &res_desc);

			DESCRIPTOR::TYPE	type;
			switch (res_desc.Type) {
				case D3D_SIT_CBUFFER:		type = DESCRIPTOR::CBV;	break;
				case D3D_SIT_TBUFFER:
				case D3D_SIT_TEXTURE:
				case D3D_SIT_STRUCTURED:	type = DESCRIPTOR::SRV;	break;
				case D3D_SIT_SAMPLER:		type = DESCRIPTOR::SMP;	break;
				default:					type = DESCRIPTOR::UAV;	break;
			}

			DESCRIPTOR bound = state.GetBound(shader->stage, type, res_desc.BindPoint, res_desc.Space);
			if (bound.res)
				assets.AddUse(assets.FindObject((uint64)bound.res));
		}
	}
}

struct PIX : const_memory_block {
	enum EVENT {
		UNICODE_VERSION		= 0,
		ANSI_VERSION		= 1,
		BLOB_VERSION		= 2,
	};

	enum EventType {
		EndEvent                       = 0x000,
		BeginEvent_VarArgs             = 0x001,
		BeginEvent_NoArgs              = 0x002,
		SetMarker_VarArgs              = 0x007,
		SetMarker_NoArgs               = 0x008,

		EndEvent_OnContext             = 0x010,
		BeginEvent_OnContext_VarArgs   = 0x011,
		BeginEvent_OnContext_NoArgs    = 0x012,
		SetMarker_OnContext_VarArgs    = 0x017,
		SetMarker_OnContext_NoArgs     = 0x018,
	};

	static const uint64 EndMarker		= 0x00000000000FFF80;

	uint32 metadata;

	struct BeginMarker {
		uint64	:7, : 3, type : 10, timestamp : 44;
		uint8	b, g, r, a;
	};
	struct StringMarker {
		uint64	:7, :46, short_cut:1, ansi:1, size:5, alignment:4;
	};

	PIX(uint32 metadata, const void* data, uint32 size) : const_memory_block(data, size), metadata(metadata) {}

	string get_string() const {
		switch (metadata) {
			default: return {};
			case PIX::UNICODE_VERSION:	return string((const char16*)begin(), size() / 2);
			case PIX::ANSI_VERSION:		return string((const char*)begin(), size());
			case PIX::BLOB_VERSION: {
				auto	begin	= (const BeginMarker*)*this;
				auto	str		= (const StringMarker*)(begin + 1);
				if (str->ansi)
					return string((const char*)(str + 1));
				return string((const char16*)(str + 1));
			}
		}
	}
	auto get_colour() const {
		if (metadata == BLOB_VERSION) {
			auto	begin	= (const BeginMarker*)*this;
			return begin->r | (begin->g << 8) | (begin->b << 16) | (begin->a << 24);
		}
		return 0;
	}
};

uint64 DX12Connection::CommandTotal() {
	uint32	total			= device_object->info.size32();
	device_object->index	= 0;

	for (auto &o : objects) {
		if (o.type == RecObject::GraphicsCommandList) {
			const RecCommandList	*rec = o.info;
			o.index	= total;
			total	+= rec->buffer.size32();
		}
	}
	return total;
}

void DX12Connection::GetAssets(progress prog, uint64 total) {
	// Get all assets
	DX12State	state;
	batches.reset();

	uint32	marker_tos		= -1;
	device_object->index	= 0;

	for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); p < e; p	= p->next()) {
		uint32	offset = uint32((char*)p - (char*)device_object->info.p);
		if (p->id == 0xffff) {
			p = p->next();
			switch (p->id) {
				case RecDevice::tag_CommandQueueSetMarker: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
					PIX	pix(Metadata, pData, Size);
					AddMarker(pix.get_string(), offset, offset, pix.get_colour());
					});
					break;
				case RecDevice::tag_CommandQueueBeginEvent: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
					PIX	pix(Metadata, pData, Size);
					marker_tos = AddMarker(pix.get_string(), offset, marker_tos, pix.get_colour());
					});
					break;
				case RecDevice::tag_CommandQueueEndEvent: COMParse(p->data(), [&]() {
					if (marker_tos >= 0) {
						if (marker_tos < markers.size()) {
							uint32	next = markers[marker_tos].b;
							markers[marker_tos].b = offset;
							marker_tos	= next;
						}
					} else {
						marker_tos = 0;
					}
					});
					break;
				case RecDevice::tag_CommandQueueExecuteCommandLists:
					COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
						for (int i = 0; i < NumCommandLists; i++) {
							const CommandRange	&r	= pp[i];
							if (ObjectRecord *rec = FindObject(uint64(&*r))) {
								if (r.extent())
									AddCall(offset, rec->index + r.begin());
							}
						}
					});
					break;
			}
		}
	}

	for (auto &o : objects) {
		if (o.type == RecObject::GraphicsCommandList) {
			const RecCommandList	*rec		= o.info;
			uint32					last_open	= o.index;
			for (const COMRecording::header *p = rec->buffer, *e = rec->buffer.end(); p < e; p = p->next()) {
				uint32	offset		= (uint8*)p - rec->buffer + o.index;
				state.ProcessCommand(this, p->id, p->data());

				switch (p->id) {
					case RecCommandList::tag_Reset:
						last_open =  ((uint8*)p->next() - rec->buffer) + o.index;
						break;

					case RecCommandList::tag_SetMarker: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
							PIX	pix(Metadata, pData, Size);
							AddMarker(pix.get_string(), offset, offset, pix.get_colour());
						});
						break;
					case RecCommandList::tag_BeginEvent: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
							PIX	pix(Metadata, pData, Size);
							marker_tos = AddMarker(pix.get_string(), offset, marker_tos, pix.get_colour());
						});
						break;
					case RecCommandList::tag_EndEvent: COMParse(p->data(), [&]() {
							if (marker_tos >= 0) {
								if (marker_tos < markers.size()) {
									uint32	next = markers[marker_tos].b;
									markers[marker_tos].b = offset;
									marker_tos	= next;
								}
							} else {
								marker_tos = 0;
							}
						});
						break;

					case RecCommandList::tag_DrawInstanced:
					case RecCommandList::tag_DrawIndexedInstanced: {
						auto	*b = AddBatch(state, last_open, ((uint8*)p->next() - rec->buffer) + o.index);

						if (state.cbv_srv_uav_descriptor_heap)
							AddUse(state.cbv_srv_uav_descriptor_heap);

						if (state.sampler_descriptor_heap)
							AddUse(state.sampler_descriptor_heap);


						uses.append(pipeline_uses[state.graphics_pipeline].or_default());

						for (uint32 i = 0, n = state.graphics_pipeline.RTVFormats.NumRenderTargets; i < n; i++) {
							if (const DESCRIPTOR *d = FindDescriptor(state.targets[i]))
								AddUse(FindObject((uint64)d->res));
						}
						for (int i = 0; i < 5; i++) {
							auto	&shader = state.graphics_pipeline.shader(i);
							if (shader.pShaderBytecode)
								GetDescriptorsForBatch(*this, state, FindShader((uint64)shader.pShaderBytecode));
						}

						if (const DESCRIPTOR *d = FindDescriptor(state.depth))
							AddUse(FindObject((uint64)d->res));

						if (state.op == RecCommandList::tag_DrawIndexedInstanced)
							AddUse(FindByGPUAddress(state.ibv.BufferLocation));

						for (auto &i : state.vbv)
							AddUse(FindByGPUAddress(i.BufferLocation));

						break;
					}
					case RecCommandList::tag_Dispatch: {
						auto	*b = AddBatch(state, last_open, ((uint8*)p->next() - rec->buffer) + o.index);

						if (state.cbv_srv_uav_descriptor_heap)
							AddUse(state.cbv_srv_uav_descriptor_heap);

						if (state.sampler_descriptor_heap)
							AddUse(state.sampler_descriptor_heap);

						uses.append(pipeline_uses[state.compute_pipeline].or_default());
						if (auto code = state.compute_pipeline.CS.pShaderBytecode)
							GetDescriptorsForBatch(*this, state, FindShader((uint64)code));

						break;
					}
					case RecCommandList::tag_ExecuteIndirect: {
						auto	*b = AddBatch(state, last_open, ((uint8*)p->next() - rec->buffer) + o.index);

						if (state.cbv_srv_uav_descriptor_heap)
							AddUse(state.cbv_srv_uav_descriptor_heap);

						if (state.sampler_descriptor_heap)
							AddUse(state.sampler_descriptor_heap);

						break;
					}
				}
			}
		}
	}

	AddBatch(state, total, total);	//dummy end batch
}

bool DX12Connection::GetStatistics() {
	DX12Replay	replay(*this, cache);

	fiber_generator<uint32>	fg([&replay]() { replay.RunGen(); }, 1 << 17);
	auto	p = fg.begin();

	if (replay.current_queue->GetTimestampFrequency(&frequency) != S_OK)
		return false;

	uint32		num	= batches.size32();

	com_ptr<ID3D12Resource>	timestamp_dest, stats_dest;
	com_ptr<ID3D12QueryHeap> timestamp_heap, stats_heap;

	if (!SUCCEEDED(replay.device->CreateQueryHeap(dx12::QUERY_HEAP_DESC(D3D12_QUERY_HEAP_TYPE_TIMESTAMP, num), IID_PPV_ARGS(&timestamp_heap)))
	||	!SUCCEEDED(replay.device->CreateCommittedResource(
		dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
		dx12::RESOURCE_DESC::Buffer(num * sizeof(uint64)),
		D3D12_RESOURCE_STATE_COPY_DEST, 0, IID_PPV_ARGS(&timestamp_dest)
	)))
		return false;

	if (!SUCCEEDED(replay.device->CreateQueryHeap(dx12::QUERY_HEAP_DESC(D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS, num), IID_PPV_ARGS(&stats_heap)))
	||	!SUCCEEDED(replay.device->CreateCommittedResource(
		dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
		dx12::RESOURCE_DESC::Buffer(num * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)),
		D3D12_RESOURCE_STATE_COPY_DEST, 0, IID_PPV_ARGS(&stats_dest)
	)))
		return false;

	ID3D12CommandQueue		*last_queue;
	for (auto &b : batches) {
		int		i	= batches.index_of(b);
		bool	last	= &b == &batches.back();

		if (!last) {
			while (*p < b.start)
				++p;
		}

		replay.current_cmd->EndQuery(timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, i);

		if (last) {
			ISO_TRACEF("last batch!\n");
			replay.current_cmd->ResolveQueryData(timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, num, timestamp_dest, 0);
			replay.current_cmd->ResolveQueryData(stats_heap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0, num - 1, stats_dest, 0);
			last_queue = replay.current_queue;
			while (*p < b.end)
				++p;
			break;
		}

		replay.current_cmd->BeginQuery(stats_heap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, i);
		while (*p < b.end)
			++p;
		replay.current_cmd->EndQuery(stats_heap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, i);

	}

#if 0
	com_ptr<ID3D12CommandAllocator>		cmd_alloc;
	com_ptr<ID3D12CommandQueue>			cmd_queue;
	com_ptr<ID3D12GraphicsCommandList>	cmd;

	D3D12_COMMAND_QUEUE_DESC	queue_desc = {
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		D3D12_COMMAND_QUEUE_FLAG_NONE,
		0
	};

	if (!SUCCEEDED(replay.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_alloc)))
	||	!SUCCEEDED(replay.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&cmd_queue)))
	||	!SUCCEEDED(replay.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc, NULL, IID_PPV_ARGS(&cmd)))
	)
		return false;

	cmd->ResolveQueryData(timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, num, dest, 0);
	cmd->Close();

	ID3D12CommandList	*cmd0 = cmd;
	cmd_queue->ExecuteCommandLists(1, &cmd0);
	replay.Wait(cmd_queue);

#else
	replay.Wait(last_queue);

#endif

	uint64									*timestamps = nullptr;
	D3D12_QUERY_DATA_PIPELINE_STATISTICS	*stats		= nullptr;
	timestamp_dest->Map(0, nullptr, (void**)&timestamps);
	stats_dest->Map(0, nullptr, (void**)&stats);

	uint64	*tp = timestamps;
	for (auto &i : batches) {
		i.timestamp = *tp++ - timestamps[0];
		i.stats		= *stats++;
	}

	return true;
}

void DX12Connection::GetStateAt(DX12State &state, uint32 addr) {
	state.Reset();

	if (addr < device_object->info.size32())
		return;

	bool	done = false;
	for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); !done && p < e; p = p->next()) {
		if (p->id == 0xffff) {
			p = p->next();

			switch (p->id) {
				case RecDevice::tag_CommandQueueExecuteCommandLists:
					COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
						for (int i = 0; i < NumCommandLists; i++) {
							uint32	end	= addr;
							auto	mem = GetCommands(pp[i], *this, end);
							for (const COMRecording::header *p = mem, *e = mem.end(); p < e; p = p->next())
								state.ProcessCommand(this, p->id, p->data());

							done = end == mem.length();
						}
					});
					break;
			}

		} else {
			state.ProcessDevice(this, p->id, p->data());
		}
	}
}

ISO_ptr_machine<void> DX12Connection::GetBitmap(const DESCRIPTOR &d) {
	auto	p = views[d];
	if (p.exists())
		return p;

	auto	*obj = FindObject((uint64)d.res);
	if (!obj)
		return ISO_NULL;

	const RecResource	*r	= obj->info;
	if (!r->HasData())
		return ISO_NULL;
	
	auto	data	= cache(r->gpu, r->data_size);
	if (!data)
		return ISO_NULL;

	switch (r->Dimension) {
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			p = dx::GetBitmap(obj->GetName(), data, r->Format, r->Width, r->DepthOrArraySize, 1, r->MipLevels, 0);
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			p = dx::GetBitmap(obj->GetName(), data, r->Format, r->Width, r->Height, r->DepthOrArraySize, r->MipLevels, 0);
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			p = dx::GetBitmap(obj->GetName(), data, r->Format, r->Width, r->Height, r->DepthOrArraySize, r->MipLevels, BMF_VOLUME);
			break;
	}
	return p;
}

int GetThumbnail(Control control, ImageList &images, DX12Connection *con, const DESCRIPTOR &d) {
	if (d.res) {
		auto	*obj	= con->FindObject((uint64)d.res);
		const RecResource	*r	= obj->info;
		if (r->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			return 0;
		if (!r->HasData())
			return 1;
		int	index = images.Add(win::Bitmap::Load("IDB_WAIT", 0, images.GetIconSize()));

		void			*bits;
		if (win::Bitmap bitmap = win::Bitmap::CreateDIBSection(DIBHEADER(images.GetIconSize(), 32), &bits)) {
			ConcurrentJobs::Get().add([=] {
				MakeThumbnail(bits, con->GetBitmap(d), images.GetIconSize());
				images.Replace(index, bitmap);
				control.Invalidate(0, false);
			});
		}
		return index;
	}
	return images.Add(win::Bitmap::Load("IDB_BAD"));
}

template<typename F> bool CheckView(uint64 h, const void *p) {
	typedef	typename function<F>::P	P;
	typedef map_t<RTM, P>			RTL1;
	return h == ((TL_tuple<RTL1>*)p)->template get<RTL1::count - 1>().ptr;
}
template<typename F> bool CheckView(uint64 h, const void *p, F f) {
	return CheckView<F>(h, p);
}

uint32 DX12Connection::FindDescriptorChange(uint64 h, uint32 stride) {
	bool	found = false;
	for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); p < e; p = p->next()) {
		switch (p->id) {
			case RecDevice::tag_CreateConstantBufferView:		found = CheckView(h, p->data(), &ID3D12Device::CreateConstantBufferView); break;
			case RecDevice::tag_CreateShaderResourceView:		found = CheckView(h, p->data(), &ID3D12Device::CreateShaderResourceView); break;
			case RecDevice::tag_CreateUnorderedAccessView:		found = CheckView(h, p->data(), &ID3D12Device::CreateUnorderedAccessView); break;
			case RecDevice::tag_CreateRenderTargetView:			found = CheckView(h, p->data(), &ID3D12Device::CreateRenderTargetView); break;
			case RecDevice::tag_CreateDepthStencilView:			found = CheckView(h, p->data(), &ID3D12Device::CreateDepthStencilView); break;

			case RecDevice::tag_CopyDescriptors: found = COMParse(p->data(), [h, stride](UINT dest_num, const D3D12_CPU_DESCRIPTOR_HANDLE *dest_starts, const UINT *dest_sizes, UINT srce_num, const D3D12_CPU_DESCRIPTOR_HANDLE *srce_starts, const UINT *srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type) {
				for (UINT s = 0, d = 0; d < dest_num; d++) {
					if (h >= dest_starts[d].ptr && h < dest_starts[d].ptr + dest_sizes[d] * stride)
						return true;
				}
				return false;
			}); break;

			case RecDevice::tag_CopyDescriptorsSimple: found = COMParse(p->data(), [h, stride](UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
				return h >= dest_start.ptr && h < dest_start.ptr + num * stride;
			}); break;
		}
		if (found)
			return (const uint8*)p - (const uint8*)device_object->info;
	}
	return 0;
}

template<typename F> bool CheckCreated(void *obj, const void *p) {
	typedef	typename function<F>::P	P;
	typedef map_t<PM, P>			RTL1;
	auto	r	= *((TL_tuple<RTL1>*)p)->template get<RTL1::count - 1>();
	return (ID3D12Object*)r == obj;
}
template<typename F> bool CheckCreated(void *obj, const void *p, F f) {
	return CheckCreated<F>(obj, p);
}

uint32 DX12Connection::FindObjectCreation(void *obj) {
	bool	found = false;
	for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); p < e; p = p->next()) {
		switch (p->id) {
			case RecDevice::tag_CreateCommandQueue:				found = CheckCreated(obj, p->data(), &ID3D12Device::CreateCommandQueue); break;
			case RecDevice::tag_CreateCommandAllocator:			found = CheckCreated(obj, p->data(), &ID3D12Device::CreateCommandAllocator); break;
			case RecDevice::tag_CreateGraphicsPipelineState:	found = CheckCreated(obj, p->data(), &ID3D12Device::CreateGraphicsPipelineState); break;
			case RecDevice::tag_CreateComputePipelineState:		found = CheckCreated(obj, p->data(), &ID3D12Device::CreateComputePipelineState); break;
			case RecDevice::tag_CreateCommandList:				found = CheckCreated(obj, p->data(), &ID3D12Device::CreateCommandList); break;
			case RecDevice::tag_CreateDescriptorHeap:			found = CheckCreated(obj, p->data(), &ID3D12Device::CreateDescriptorHeap); break;
			case RecDevice::tag_CreateRootSignature:			found = CheckCreated<HRESULT(UINT, counted<uint8, 2>, SIZE_T, REFIID, void**)>(obj, p->data()); break;
			case RecDevice::tag_CreateCommittedResource:		found = CheckCreated(obj, p->data(), &ID3D12Device::CreateCommittedResource); break;
			case RecDevice::tag_CreateHeap:						found = CheckCreated(obj, p->data(), &ID3D12Device::CreateHeap); break;
			case RecDevice::tag_CreatePlacedResource:			found = CheckCreated(obj, p->data(), &ID3D12Device::CreatePlacedResource); break;
			case RecDevice::tag_CreateReservedResource:			found = CheckCreated(obj, p->data(), &ID3D12Device::CreateReservedResource); break;
			case RecDevice::tag_CreateSharedHandle:				found = CheckCreated(obj, p->data(), &ID3D12Device::CreateSharedHandle); break;
			case RecDevice::tag_CreateFence:					found = CheckCreated(obj, p->data(), &ID3D12Device::CreateFence); break;
			case RecDevice::tag_CreateQueryHeap:				found = CheckCreated(obj, p->data(), &ID3D12Device::CreateQueryHeap); break;
			case RecDevice::tag_CreateCommandSignature:			found = CheckCreated(obj, p->data(), &ID3D12Device::CreateCommandSignature); break;
		}
		if (found)
			return (const uint8*)p - (const uint8*)device_object->info;
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	DX12StateControl
//-----------------------------------------------------------------------------

template<> const char *field_names<dx::RTS0::VISIBILITY>::s[]		= {"ALL", "VERTEX", "HULL", "DOMAIN", "GEOMETRY", "PIXEL" };
template<> const char *field_names<dx::RTS0::Range::TYPE>::s[]		= { "SRV", "UAV", "CBV", "SMP" };
template<> const char *field_names<dx::RTS0::Parameter::TYPE>::s[]	= { "TABLE", "CONSTANTS", "CBV", "SRV", "UAV" };

template<> static constexpr bool field_is_struct<dx::TextureFilterMode> = false;
template<> struct field_names<dx::TextureFilterMode>		: field_names<D3D12_FILTER> {};
template<> struct field_names<dx::TextureAddressMode>		: field_names<D3D12_TEXTURE_ADDRESS_MODE> {};
template<> struct field_names<dx::TextureBorderColour>		: field_names<D3D12_STATIC_BORDER_COLOR> {};
template<> struct field_names<dx::ComparisonFunction>		: field_names<D3D12_COMPARISON_FUNC> {};
template<> struct field_names<dx::RTS0::FLAGS>				: field_names<D3D12_HEAP_FLAGS> {};

template<> struct field_names<dx::RTS0::Descriptor::FLAGS>			{ static field_bit s[];	};
field_bit field_names<dx::RTS0::Descriptor::FLAGS>::s[]	= {
	{"NONE",								0	},
	{"DESCRIPTORS_VOLATILE",				1	},
	{"DATA_VOLATILE",						2	},
	{"DATA_STATIC_WHILE_SET_AT_EXECUTE",	4	},
	{"DATA_STATIC",							8	},
	0
};
MAKE_FIELDS(dx::RTS0::Sampler, filter, address_u, address_v, address_w, mip_lod_bias, max_anisotropy, comparison_func, border, min_lod, max_lod, reg, space, visibility);
MAKE_FIELDS(dx::RTS0::Constants, base, space, num);
MAKE_FIELDS(dx::RTS0::Descriptor, reg, space, flags);
MAKE_FIELDS(dx::RTS0::Range, type, num, base, space, offset);
MAKE_FIELDS(dx::RTS0::Parameter, type, visibility);

struct DX12StateControl : ColourTree {
	enum TYPE {
		ST_TARGET	= 1,
		ST_SHADER,
		ST_TEXTURE,
		ST_OBJECT,
		ST_VERTICES,
		ST_OUTPUTS,
		ST_GPUREG,
		ST_DESCRIPTOR,
	};
	static win::Colour	colours[];
	static uint8		cursor_indices[][3];

	DX12Connection *con;

	DX12StateControl(DX12Connection *con) : ColourTree(colours, DX12cursors, cursor_indices), con(con) {}

	HTREEITEM	AddShader(HTREEITEM h, const DX12ShaderState &shader);

	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		buffer_accum<256>	ba;
		PutFieldName(ba, tree.format, pf);

		uint64	v = pf->get_raw_value(p, offset);
		if (auto *rec = con->FindObject(v))
			tree.Add(h, ba << rec->name, ST_OBJECT, rec);
		else if (auto *rec = con->FindShader(v))
			tree.Add(h, ba << rec->name, ST_SHADER, rec);
		else
			tree.Add(h, ba << "0x" << hex(v), ST_GPUREG, addr + offset);
	}
};

win::Colour DX12StateControl::colours[] = {
	{0,0,0},
	{128,0,0},		//ST_TARGET
	{128,0,64},		//ST_SHADER
	{0,128,0},		//ST_TEXTURE,
	{0,128,64},		//ST_OBJECT,
	{64,0,128},		//ST_VERTICES
	{0,0,0},		//ST_OUTPUTS,
	{0,0,0},		//ST_GPUREG,
	{0,0,0},		//ST_DESCRIPTOR,
};
uint8 DX12StateControl::cursor_indices[][3] = {
	{0,0,0},
	{1,2,0},	//ST_TARGET
	{1,2,0},	//ST_SHADER
	{1,2,0},	//ST_TEXTURE,
	{1,2,0},	//ST_OBJECT,
	{1,2,0},	//ST_VERTICES
	{1,2,0},	//ST_OUTPUTS,
	{1,2,0},	//ST_GPUREG,
	{1,2,0},	//ST_DESCRIPTOR,
};

void AddValue(RegisterTree &rt, HTREEITEM h, const char *name, const void *data, ID3D12ShaderReflectionType *type) {
	D3D12_SHADER_TYPE_DESC	type_desc;
	if (FAILED(type->GetDesc(&type_desc)))
		return;

	if (data)
		data = (uint8*)data + type_desc.Offset;

	if (type_desc.Class == D3D_SVC_STRUCT) {
		buffer_accum<256>	ba(name);
		HTREEITEM	h2 = rt.AddText(h, ba);
		for (int i = 0; i < type_desc.Members; i++)
			AddValue(rt, h2, type->GetMemberTypeName(i), data, type->GetMemberTypeByIndex(i));

	} else {
		AddValue(rt, h, name, data, type_desc.Class, type_desc.Type, type_desc.Rows, type_desc.Columns);
	}
}

void AddValues(RegisterTree &rt, HTREEITEM h, ID3D12ShaderReflectionConstantBuffer *cb, const void *data) {
	D3D12_SHADER_BUFFER_DESC	cb_desc;
	if (SUCCEEDED(cb->GetDesc(&cb_desc))) {
		for (int i = 0; i < cb_desc.Variables; i++) {
			ID3D12ShaderReflectionVariable*		var = cb->GetVariableByIndex(i);
			D3D12_SHADER_VARIABLE_DESC			var_desc;
			if (SUCCEEDED(var->GetDesc(&var_desc)) && (var_desc.uFlags & D3D_SVF_USED))
				AddValue(rt, h, var_desc.Name, data ? (const uint8*)data + var_desc.StartOffset : 0, var->GetType());
		}
	}
}

HTREEITEM AddShader(RegisterTree &rt, HTREEITEM h, const DX12ShaderState &shader, DX12Connection *con) {
	com_ptr<ID3D12ShaderReflection>	refl;
	D3D12_SHADER_DESC				desc;

	if (SUCCEEDED(D3DReflect(shader.data, shader.data.length(), IID_ID3D12ShaderReflection, (void**)&refl)) && SUCCEEDED(refl->GetDesc(&desc))) {
		auto	h1 = rt.AddText(h, "ID3D12ShaderReflection");

		rt.AddFields(rt.AddText(h1, "D3D12_SHADER_DESC"), &desc);

		HTREEITEM	h2 = rt.AddText(h1, "ConstantBuffers");
		for (int i = 0; i < desc.ConstantBuffers; i++) {
			ID3D12ShaderReflectionConstantBuffer*	cb = refl->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC				cb_desc;
			cb->GetDesc(&cb_desc);

			HTREEITEM	h3 = rt.AddText(h2, cb_desc.Name);
			rt.AddText(h3, format_string("size = %i", cb_desc.Size));

			for (int i = 0; i < cb_desc.Variables; i++) {
				ID3D12ShaderReflectionVariable*		var = cb->GetVariableByIndex(i);
				D3D12_SHADER_VARIABLE_DESC			var_desc;
				var->GetDesc(&var_desc);

				HTREEITEM	h4 = rt.AddText(h3, var_desc.Name);
				rt.AddText(h4, format_string("Slot = %i", var->GetInterfaceSlot(0)));

				rt.AddFields(rt.AddText(h4, "D3D12_SHADER_VARIABLE_DESC"), &var_desc);

				D3D12_SHADER_TYPE_DESC				type_desc;
				ID3D12ShaderReflectionType*			type = var->GetType();
				type->GetDesc(&type_desc);
				rt.AddFields(rt.AddText(h4, "D3D12_SHADER_TYPE_DESC"), &type_desc);
			}
		}

		h2 = rt.AddText(h1, "BoundResources");
		for (int i = 0; i < desc.BoundResources; i++) {
			D3D12_SHADER_INPUT_BIND_DESC	res_desc;
			refl->GetResourceBindingDesc(i, &res_desc);
			rt.AddFields(rt.AddText(h2, res_desc.Name), &res_desc);
		}

		h2 = rt.AddText(h1, "InputParameters");
		for (int i = 0; i < desc.InputParameters; i++) {
			D3D12_SIGNATURE_PARAMETER_DESC	in_desc;
			refl->GetInputParameterDesc(i, &in_desc);
			rt.AddFields(rt.AddText(h2, in_desc.SemanticName), &in_desc);
		}

		h2 = rt.AddText(h1, "OutputParameters");
		for (int i = 0; i < desc.OutputParameters; i++) {
			D3D12_SIGNATURE_PARAMETER_DESC	out_desc;
			refl->GetOutputParameterDesc(i, &out_desc);
			rt.AddFields(rt.AddText(h2, out_desc.SemanticName), &out_desc);
		}
		
		if (desc.BoundResources) {
			h2	= rt.AddText(h, "BoundResources");
			for (int i = 0; i < desc.BoundResources; i++) {
				D3D12_SHADER_INPUT_BIND_DESC	res_desc;
				refl->GetResourceBindingDesc(i, &res_desc);

				DESCRIPTOR *bound = 0;
				switch (res_desc.Type) {
					case D3D_SIT_CBUFFER:		bound = shader.cbv.check(res_desc.BindPoint); break;
					case D3D_SIT_TBUFFER:
					case D3D_SIT_TEXTURE:
					case D3D_SIT_STRUCTURED:	bound = shader.srv.check(res_desc.BindPoint); break;
					case D3D_SIT_SAMPLER:		bound = shader.smp.check(res_desc.BindPoint); break;
					default:					bound = shader.uav.check(res_desc.BindPoint); break;
				}

				if (bound) {
					HTREEITEM	h3 = rt.Add(h2, res_desc.Name, DX12StateControl::ST_DESCRIPTOR, bound);
					rt.AddFields(rt.AddText(h3, "Descriptor"), bound);

					switch (bound->type) {
						case DESCRIPTOR::CBV: {
							AddValues(rt, h3, refl->GetConstantBufferByIndex(res_desc.BindPoint), con->cache(bound->cbv.BufferLocation, bound->cbv.SizeInBytes));
							break;
						}
						case DESCRIPTOR::PCBV: {
							ID3D12ShaderReflectionConstantBuffer*	cb = refl->GetConstantBufferByIndex(res_desc.BindPoint);
							D3D12_SHADER_BUFFER_DESC	cb_desc;
							cb->GetDesc(&cb_desc);
							AddValues(rt, h3, cb, con->cache(bound->ptr, cb_desc.Size));
							break;
						}
						case DESCRIPTOR::IMM: {
							AddValues(rt, h3, refl->GetConstantBufferByIndex(res_desc.BindPoint), bound->imm);
							break;
						}
					}
				}
			}
		} else {
			h2	= rt.AddText(h, "BoundRegisters");
			for (auto &i : shader.cbv)
				rt.AddFields(rt.AddText(h2, "b" + to_string(i.index())), &i.t);

			for (auto &i : shader.srv)
				rt.AddFields(rt.AddText(h2, "t" + to_string(i.index())), &i.t);

			for (auto &i : shader.uav)
				rt.AddFields(rt.AddText(h2, "u" + to_string(i.index())), &i.t);

			for (auto &i : shader.smp)
				rt.AddFields(rt.AddText(h2, "s" + to_string(i.index())), &i.t);
		}
	}

	if (auto* rts0 = shader.DXBC()->GetBlob<dx::RTS0>()) {
		auto	h1 = rt.AddText(h, "Root Signature");
		HTREEITEM	h3 = rt.AddText(h1, "Parameters");

		for (auto &i : rts0->parameters.entries(rts0)) {
			HTREEITEM	h4 = rt.AddText(h3, "Parameter");
			rt.AddFields(h4, &i);
			void		*p = i.ptr.get(rts0);
			switch (i.type) {
				case dx::RTS0::Parameter::TABLE: {
					auto	*p2 = (dx::RTS0::DescriptorTable*)p;
					for (auto &i : p2->entries(rts0))
						rt.AddFields(rt.AddText(h4, "Range"), &i);
					break;
				}
				case dx::RTS0::Parameter::CONSTANTS:
					rt.AddFields(h4, (dx::RTS0::Constants*)p);
					break;

				default:
					rt.AddFields(h4, (dx::RTS0::Descriptor*)p);
					break;
			}
		}

		rt.AddArray(rt.AddText(h1, "Samplers"), rts0->samplers.entries(rts0));
	}

	return h;
}

HTREEITEM DX12StateControl::AddShader(HTREEITEM h, const DX12ShaderState &shader) {
	return ::AddShader(lvalue(RegisterTree(*this, this)), h, shader, con);
}

//-----------------------------------------------------------------------------
//	DX12ShaderWindow
//-----------------------------------------------------------------------------

struct DX12ShaderWindow : SplitterWindow, CodeHelper {
	DX12StateControl	tree;
	TabControl3			*tabs[2];

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	DX12ShaderWindow(const WindowPos &wpos, DX12Connection *_con, const DX12ShaderState &shader,  const char *shader_path);
};

LRESULT	DX12ShaderWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
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
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

DX12ShaderWindow::DX12ShaderWindow(const WindowPos &wpos, DX12Connection *con, const DX12ShaderState &shader, const char *shader_path)
	: SplitterWindow(SWF_VERT | SWF_PROP)
	, CodeHelper(HLSLcolourerRE(), none, GetSettings("General/shader source").GetInt(1))
	, tree(con)
{
	Create(wpos, shader.name, CHILD | CLIPCHILDREN | VISIBLE);
	Rebind(this);

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY, 100);
	split2->Create(GetPanePos(0), 0, CHILD | CLIPCHILDREN | VISIBLE);

	auto		spdb = ParsedSPDB(memory_reader(shader.DXBC()->GetBlob(dx::DXBC::ShaderPDB)), shader_path);
	locations = move(spdb.locations);
	FixLocations(shader.GetUCodeAddr());

	// code tabs
	tabs[0] = new TabControl3(split2->_GetPanePos(0), "code", CHILD | CLIPSIBLINGS);
	tabs[0]->SetFont(win::Font::DefaultGui());

//	D2DTextWindow *tw = new D2DTextWindow(split2->_GetPanePos(0), "Original", CHILD | TextWindow::READONLY);
	D2DTextWindow *tw = new D2DTextWindow(tabs[0]->GetChildWindowPos(), "Original", CHILD | TextWindow::READONLY);
	CodeHelper::SetDisassembly(*tw, shader.Disassemble(), spdb.files);
	tw->Show();
	tabs[0]->AddItemControl(*tw, "Disassembly");

	com_ptr<ID3DBlob>			blob;
	if (SUCCEEDED(D3DDisassemble(shader.data, shader.data.length(), D3D_DISASM_ENABLE_COLOR_CODE, 0, &blob))) {
		void	*p		= blob->GetBufferPointer();
		size_t	size	= blob->GetBufferSize();
		tabs[0]->AddItemControl(MakeHTMLViewer(GetChildWindowPos(), "Shader", (const char*)p, size), "DirectX");
		tabs[0]->SetSelectedIndex(0);
	}

	// tree
	split2->SetPanes(*tabs[0], tree.Create(split2->_GetPanePos(1), 0, CHILD | VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT), 100);
	tree.AddShader(TVI_ROOT, shader);
	tabs[0]->Show();

	// source tabs
	if (!spdb.files.empty()) {
		tabs[1] = new TabControl3(GetPanePos(1), "source", CHILD | VISIBLE);
		tabs[1]->SetFont(win::Font::DefaultGui());
		SourceTabs(*tabs[1], spdb.files);
		SetPanes(*split2, *tabs[1], 50);
		tabs[1]->ShowSelectedControl();
	} else {
		tabs[1] = 0;
		SetPane(0, *split2);
	}
}

Control MakeShaderViewer(const WindowPos &wpos, DX12Connection *con, const DX12ShaderState &shader, const char *shader_path) {
	return *new DX12ShaderWindow(wpos, con, shader, shader_path);
}

//-----------------------------------------------------------------------------
//	DX12ShaderDebuggerWindow
//-----------------------------------------------------------------------------

class DX12ShaderDebuggerWindow : public MultiSplitterWindow, DebugWindow {
	Accelerator				accel;
	TabControl3				*tabs;
	DXBCRegisterWindow		*regs;
	DXBCLocalsWindow		*locals;
	DX12StateControl		tree;
	ComboControl			thread_control;
	ListBoxControl			reg_overlay;
	ParsedSPDB				spdb;
	dynamic_array<uint64>	bp_offsets;
	uint64					pc;
	uint32					step_count;
	uint32					thread;
	dx::SHADERSTAGE			stage;

	void	ToggleBreakpoint(int32 offset) {
		if (DebugWindow::ToggleBreakpoint(OffsetToLine(offset))) {
			bp_offsets.insert(lower_boundc(bp_offsets, offset), offset);
		} else {
			bp_offsets.erase(lower_boundc(bp_offsets, offset));
		}
	}
public:
	dx::SimulatorDXBC		sim;
	
	Control&	control()	{ return MultiSplitterWindow::control(); }

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void		Update() {
		SetPC(AddressToLine(pc));
		regs->Update(sim, stage, spdb, pc, thread);
		if (locals)
			locals->Update(sim, spdb, pc, thread);
	}
	DX12ShaderDebuggerWindow(const WindowPos &wpos, const char *title, DX12ShaderState &shader, DX12Connection *con, int mode);

	void	SetThread(int _thread)	{
		thread = _thread;

		for (int i = 0; i < sim.NumThreads(); i++)
			thread_control.SetItemData(i, thread_control.Add(to_string(i)));
		thread_control.Select(thread);

		auto	*op = sim.Begin();
		while (op->IsDeclaration())
			op = op->next();
		pc	= sim.Address(op);
	
		Update();
	}
};

DX12ShaderDebuggerWindow::DX12ShaderDebuggerWindow(const WindowPos &wpos, const char *title, DX12ShaderState &shader, DX12Connection *con, int mode)
	: MultiSplitterWindow(2, SWF_VERT | SWF_PROP | SWF_DELETE_ON_DESTROY)
	, DebugWindow(HLSLcolourerRE(), none, mode)
	, accel(GetAccelerator())
	, tree(con)
	, spdb(lvalue(memory_reader(shader.DXBC()->GetBlob(dx::DXBC::ShaderPDB))))
	, step_count(0), thread(0), stage(shader.stage)
{
	MultiSplitterWindow::Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, CLIENTEDGE);// | COMPOSITED);

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
	split2->Create(GetPanePos(0), 0, CHILD | VISIBLE);

	DebugWindow::Create(split2->_GetPanePos(0), NULL, CHILD | READONLY | SELECTIONBAR);
	DebugWindow::locations	= move(spdb.locations);
	DebugWindow::files		= move(spdb.files);
	DebugWindow::FixLocations(shader.GetUCodeAddr());
	DebugWindow::SetDisassembly(shader.Disassemble(), true);

	tree.Create(split2->_GetPanePos(0), 0, CHILD | VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT);
	tree.AddShader(TVI_ROOT, shader);

	split2->SetPanes(DebugWindow::hWnd, tree, 100);
	tree.Show();
	SetPane(0, *split2);

	// source tabs
	tabs = 0;
	if (HasFiles()) {
		tabs = new TabControl3(GetPanePos(1), "source", CHILD | VISIBLE);
		tabs->SetFont(win::Font::DefaultGui());
		SourceTabs(*tabs);
		InsertPane(*tabs, 1);
	}

	int	regs_pane	= NumPanes() - 1;

	SplitterWindow	*split3 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
	split3->Create(GetPanePos(regs_pane), 0, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);
	SetPane(regs_pane, *split3);

	TitledWindow	*regs_title		= new TitledWindow(split3->GetPanePos(0), "Registers");
	TitledWindow	*locals_title	= new TitledWindow(split3->GetPanePos(0), "Locals");
	split3->SetPanes(*regs_title, *locals_title);

	regs	= new DXBCRegisterWindow(regs_title->GetChildWindowPos());
	locals	= new DXBCLocalsWindow(locals_title->GetChildWindowPos(), ctypes);
#if 1
	thread_control.Create(control(), "thread", CHILD | OVERLAPPED | VISIBLE | VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS, NOEX,
		Rect(wpos.rect.Width() - 64, 0, 64 - GetNonClientMetrics().iScrollWidth, GetNonClientMetrics().iSmCaptionHeight),
		'TC'
	);
	thread_control.SetFont(win::Font::SmallCaption());
	thread_control.MoveAfter(HWND_TOP);
#endif
	DebugWindow::Show();
	MultiSplitterWindow::Rebind(this);
}

LRESULT DX12ShaderDebuggerWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
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
					auto	*op	= sim.AddressToOp(pc);
					while (op) {
						++step_count;
						op	= sim.ProcessOp(op);
						uint32	offset	= sim.Offset(op);
						auto	i		= lower_boundc(bp_offsets, offset);
						if (i != bp_offsets.end() && *i == offset)
							break;
					}
					pc	= sim.Address(op);
					Update();
					return 0;
				}

				case DebugWindow::ID_DEBUG_STEPOVER:
					if (pc) {
						++step_count;
						auto	*op = sim.AddressToOp(pc);
						op	= sim.ProcessOp(op);
						pc	= sim.Address(op);
						Update();
					}
					return 0;

				case DebugWindow::ID_DEBUG_STEPBACK:
					if (step_count) {
						sim.Reset();
						auto	*op = sim.Run(--step_count);
						pc			= sim.Address(op);
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
				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					break;

				case NM_CLICK:
					if (wParam == 'RG')
						regs->AddOverlay((NMITEMACTIVATE*)lParam, sim);
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
//	DX12BatchWindow
//-----------------------------------------------------------------------------

Control MakeObjectView(const WindowPos &wpos, DX12Connection *_con, DX12Assets::ObjectRecord *rec);
Control MakeDescriptorView(const WindowPos &wpos, const char *title, DX12Connection *con, const DESCRIPTOR &desc);

class DX12BatchWindow : public StackWindow {
	struct Target : dx::SimulatorDXBC::Resource {
		ISO_ptr_machine<void>	p;
		void init(const char *name, const dx::SimulatorDXBC::Resource &r) {
			dx::SimulatorDXBC::Resource::operator=(r);
			ConcurrentJobs::Get().add([this, name] {
				p = dx::GetBitmap(name, *this);
			});
		}
	};

public:
	enum {ID = 'BA'};
	DX12Connection				*con;
	DX12StateControl			tree;
	DX12ShaderState				shaders[5];
	dynamic_array<TypedBuffer>	verts;
	dynamic_array<pair<TypedBuffer, int>>	vbv;
	dynamic_array<Target>		targets;

	indices						ix;
	Topology2					topology;
	BackFaceCull				culling;
	float3x2					viewport;
	Point						screen_size;

	static DX12BatchWindow*	Cast(Control c)	{ return (DX12BatchWindow*)StackWindow::Cast(c); }

	DX12Connection	*GetConnection() const {
		return con;
	}
	void AddView(Control c, bool new_tab) {
		Dock(new_tab ? DOCK_ADDTAB : DOCK_PUSH, c);
	}
	void SelectBatch(BatchList &b, bool always_list = false) {
		int		batch	= ::SelectBatch(*this, GetMousePos(), b, always_list);
		if (batch >= 0)
			Parent()(WM_ISO_BATCH, batch);
	}
	Control ShowBitmap(ISO_ptr_machine<void> p, bool new_tab) {
		Control	c;
		if (p) {
			c = BitmapWindow(GetChildWindowPos(), p, tag(p.ID()), true);
			AddView(c, new_tab);
		}
		return c;
	}
	DX12BatchWindow(const WindowPos &wpos, text title, DX12Connection *con, const DX12State &state, DX12Replay *replay);
	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	int			InitSimulator(dx::SimulatorDXBC &sim, DX12ShaderState &shader, int thread, dynamic_array<uint16> &indices, Topology2 &top);
	Control		MakeShaderOutput(dx::SHADERSTAGE stage, bool mesh);
	void		VertexMenu(dx::SHADERSTAGE stage, int i, ListViewControl lv);
	Control		DebugPixel(uint32 target, const Point &pt);
};

DX12BatchWindow::DX12BatchWindow(const WindowPos &wpos, text title, DX12Connection *con, const DX12State &state, DX12Replay *replay) : StackWindow(wpos, title, ID), con(con), tree(con) {
	Rebind(this);
	clear(screen_size);

	HTREEITEM	h;

	if (state.IsGraphics()) {
		topology	= state.GetTopology();
		culling		= state.CalcCull(true);
		ix			= state.GetIndexing(con->cache);

		if (state.op == RecCommandList::tag_ExecuteIndirect) {
			auto	desc	= rmap_unique<RTM, D3D12_COMMAND_SIGNATURE_DESC>(state.indirect.signature->info);
			const RecResource	*rr		= state.indirect.arguments->info;
			auto	args	= con->cache(rr->gpu + state.indirect.arguments_offset);
			auto	batch	= GetCommand(desc, args);
			ix			= state.GetIndexing(con->cache, batch);

		} else {
			ix			= state.GetIndexing(con->cache);
		}

		if (auto *d = con->FindDescriptor(state.targets[0])) {
			if (auto *obj = con->FindObject(uint64(d->res))) {
				const RecResource *rr = obj->info;
				screen_size = Point(rr->Width, rr->Height);
			}
		}

		auto	vp	= state.GetViewport(0);
		float3	vps{vp.Width / 2, vp.Height / 2, vp.MaxDepth - vp.MinDepth};
		float3	vpo{vp.TopLeftX + vp.Width / 2, vp.TopLeftY + vp.Height / 2, vp.MinDepth};
		viewport	= float3x2(
			vps,
			vpo
		);

		if (replay) {
			int		n = state.graphics_pipeline.RTVFormats.NumRenderTargets;
			targets.resize(n + 1);
			for (uint32 i = 0; i < n; i++)
				targets[i].init("render", replay->GetResource(con->FindDescriptor(state.targets[i]), con->cache));
			targets[n].init("depth", replay->GetResource(con->FindDescriptor(state.depth), con->cache));
		}

		// get vbv
		if (state.vbv && state.graphics_pipeline.InputLayout.elements) {
			vbv.resize(state.graphics_pipeline.InputLayout.elements.size());
			auto	d = state.graphics_pipeline.InputLayout.elements.begin();
			uint32	offset	= 0;
			for (auto &i : vbv) {
				D3D12_VERTEX_BUFFER_VIEW	view = state.vbv[d->InputSlot];
				if (d->AlignedByteOffset != D3D12_APPEND_ALIGNED_ELEMENT)
					offset = d->AlignedByteOffset;

				i.a = TypedBuffer(con->cache(view.BufferLocation + offset, view.SizeInBytes - offset), view.StrideInBytes, dx::to_c_type(d->Format));
				offset += uint32(DXGI_COMPONENTS(d->Format).Size());
				++d;
			}

			SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
			split2->Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE);
			split2->SetClientPos(400);

			split2->SetPanes(
				tree.Create(split2->_GetPanePos(0), 0, CHILD | CLIPSIBLINGS | VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT),
				*MakeMeshView(split2->_GetPanePos(1), topology, vbv[0].a, ix, one, culling, MeshWindow::PERSPECTIVE)
			);
		} else {
			tree.Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT);
		}

		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		//rt.AddFields(TreeControl::Item("Pipeline State").Bold().Expand().Insert(tree), state.pipeline);

		h	= TreeControl::Item("Descriptor Heaps").Bold().Expand().Insert(tree);
		if (state.cbv_srv_uav_descriptor_heap)
			rt.AddFields(rt.AddText(h, "cbv_srv_uav"), state.cbv_srv_uav_descriptor_heap);

		if (state.sampler_descriptor_heap)
			rt.AddFields(rt.AddText(h, "sampler"), state.sampler_descriptor_heap);

		h	= TreeControl::Item("Targets").Bold().Expand().Insert(tree);
		uint32 nt = state.graphics_pipeline.RTVFormats.NumRenderTargets;

		if (!state.graphics_pipeline.BlendState.IndependentBlendEnable) {
			buffer_accum<256>	ba;
			ba << "Blend: " << WriteBlend(state.graphics_pipeline.BlendState.RenderTarget[0], state.BlendFactor);
			rt.AddText(h, ba);
		}

		for (uint32 i = 0; i < nt; i++) {
			const DESCRIPTOR *d = con->FindDescriptor(state.targets[i]);
			buffer_accum<256>	ba;
			ba << "Target " << i << con->ObjectName(d);
			if (state.graphics_pipeline.BlendState.IndependentBlendEnable)
				ba << ' ' << WriteBlend(state.graphics_pipeline.BlendState.RenderTarget[i], state.BlendFactor);
			HTREEITEM	h2 = replay
				? rt.Add(h, ba, tree.ST_TARGET, i)
				: rt.Add(h, ba, tree.ST_DESCRIPTOR, d);

			if (d)
				rt.AddFields(rt.AddText(h2, "Descriptor"), d);
			rt.AddFields(rt.AddText(h2, "Blending"), &state.graphics_pipeline.BlendState.RenderTarget[i]);
		}

		const DESCRIPTOR *d = con->FindDescriptor(state.depth);
		buffer_accum<256>	ba;
		ba << "Depth Buffer" << con->ObjectName(d);

		HTREEITEM	h2	= replay
			? rt.Add(h, ba, tree.ST_TARGET, nt)
			: rt.Add(h, ba, tree.ST_DESCRIPTOR, d);
		if (d)
			rt.AddFields(rt.AddText(h2, "Descriptor"), d);
		rt.AddFields(rt.AddText(h2, "DepthStencilState"), (D3D12_DEPTH_STENCIL_DESC1*)&state.graphics_pipeline.DepthStencilState);

		rt.AddArray(rt.AddText(h, "Viewports"), state.viewport);
		rt.AddArray(rt.AddText(h, "Scissor Rects"), state.scissor);

		h	= TreeControl::Item("Shaders").Bold().Expand().Insert(tree);
		static const char *shader_names[] = {
			"Vertex Shader",
			"Pixel Shader",
			"Domain Shader",
			"Hull Shader",
			"Geometry Shader",
		};
		for (int i = 0; i < 5; i++) {
			dx::SHADERSTAGE	stage = dx::SHADERSTAGE(i);
			const D3D12_SHADER_BYTECODE	&shader = state.graphics_pipeline.shader(i);
			if (shader.pShaderBytecode) {
				if (auto rec = con->FindShader((uint64)shader.pShaderBytecode)) {
					shaders[i].init(*con, con->cache, rec, &state);
					h2 = tree.AddShader(
						TreeControl::Item(buffer_accum<256>(shader_names[i]) << ": " << shaders[i].name).Image(tree.ST_SHADER).Param(i).Insert(tree, h),
						shaders[i]
					);
					if (stage == dx::VS) {
						HTREEITEM	h3 = rt.Add(h2, "Inputs", tree.ST_VERTICES, stage);
						if (state.op == RecCommandList::tag_DrawIndexedInstanced)
							rt.AddFields(rt.AddText(h3, "IndexBuffer"), &state.ibv);

						int	x = 0;
						for (auto &i : state.vbv) {
							verts.emplace_back(con->cache(i.BufferLocation, i.SizeInBytes), i.StrideInBytes, state.graphics_pipeline.InputLayout.GetVertexType(x));
							rt.AddFields(TreeControl::Item(format_string("VertexBuffer %i", x)).Image(tree.ST_VERTICES).StateImage(x + 1).Insert(tree, h3), &i);
							++x;
						}

						auto	*in	= shaders[i].DXBC()->GetBlob<dx::ISGN>();
						for (int i = 0; i < state.graphics_pipeline.InputLayout.elements.size(); i++) {
							auto	&desc = state.graphics_pipeline.InputLayout.elements[i];
							if (auto *x = in->find_by_semantic(desc.SemanticName, desc.SemanticIndex))
								vbv[i].b = x->register_num;
							else
								vbv[i].b = -1;
						}
					}
					rt.Add(h2, "Outputs", tree.ST_OUTPUTS, stage);
				}
			}
		}
		
	} else {
		tree.Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT);
		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);

		//rt.AddFields(TreeControl::Item("Pipeline State").Bold().Expand().Insert(tree), state.pipeline);

		h	= TreeControl::Item("Descriptor Heaps").Bold().Expand().Insert(tree);
		if (state.cbv_srv_uav_descriptor_heap)
			rt.AddFields(rt.AddText(h, "cbv_srv_uav"), state.cbv_srv_uav_descriptor_heap);

		if (state.sampler_descriptor_heap)
			rt.AddFields(rt.AddText(h, "sampler"), state.sampler_descriptor_heap);

		if (auto code = state.compute_pipeline.CS.pShaderBytecode) {
			if (auto rec = con->FindShader((uint64)code)) {
				shaders[0].init(*con, con->cache, rec, &state);
				tree.AddShader(
					TreeControl::Item(buffer_accum<256>("Compute Shader") << shaders[0].name).Image(tree.ST_SHADER).Param(0).Insert(tree, TVI_ROOT),
					shaders[0]
				);
			}
		}
	}

	if (state.op == RecCommandList::tag_ExecuteIndirect) {
		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		h	= TreeControl::Item("Indirect").Bold().Expand().Insert(tree);

		auto	desc	= rmap_unique<RTM, D3D12_COMMAND_SIGNATURE_DESC>(state.indirect.signature->info);

		rt.AddFields(rt.AddText(h, "Command Signature"), (const map_t<RTM, D3D12_COMMAND_SIGNATURE_DESC>*)state.indirect.signature->info);

		auto	count = state.indirect.command_count;

		if (state.indirect.counts) {
			const RecResource	*rr		= state.indirect.counts->info;
			uint32	*counts				= con->cache(rr->gpu + state.indirect.counts_offset, rr->Width - state.indirect.counts_offset);
			if (counts && *counts < count)
				count = *counts;
			rt.AddFields(TreeControl::Item("Counts").Image(tree.ST_VERTICES).StateImage(verts.size32()).Insert(tree, h), (const RecResource*)state.indirect.arguments->info);
		}

		if (state.indirect.arguments) {
			const RecResource	*rr		= state.indirect.arguments->info;
		#if 0
			const C_type		*type	= GetSignatureType(desc);
			verts.emplace_back(con->cache(rr->gpu + state.indirect.arguments_offset, count * desc->ByteStride), desc->ByteStride, type);
			rt.AddFields(TreeControl::Item("Arguments").Image(tree.ST_VERTICES).StateImage(verts.size32()).Insert(tree, h), (const RecResource*)state.indirect.arguments->info);
			auto&	v	= verts.back();
			StructureHierarchy(rt, h2, ctypes, format_string("[%i]", i), type, 0, v[i]);
		#else
			auto	args = con->cache(rr->gpu + state.indirect.arguments_offset, count * desc->ByteStride);
			const uint8*	p	= args;
			auto	h2	= TreeControl::Item("Arguments").Image(tree.ST_VERTICES).StateImage(verts.size32()).Insert(tree, h);
			for (int i = 0; i < count; i++) {
				auto	h3	= rt.AddText(h2, format_string("[%i]", i).begin());

				for (auto &i : make_range_n(desc->pArgumentDescs, desc->NumArgumentDescs)) {
					auto	h4	= rt.AddText(h3, get_field_name(i.Type));
					rt.AddFields(h4, IndirectFields[i.Type], p);
					p += IndirectSizes[i.Type];
				}

			}
		#endif
		}

	}
}

LRESULT DX12BatchWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case DebugWindow::ID_DEBUG_PIXEL:
					auto	*p = (pair<uint64,Point>*)lParam;
					if (Control c = DebugPixel(p->a, p->b))
						AddView(c, false);
					return 0;
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CLICK: {
					if (nmh->hwndFrom == tree) {
						if (HTREEITEM hItem = tree.hot) {
							tree.hot	= 0;
							TreeControl::Item i	= tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE);
							bool	ctrl	= !!(GetKeyState(VK_CONTROL) & 0x8000);
							bool	new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);

							switch (i.Image()) {
								case DX12StateControl::ST_TARGET: {
									int	t = i.Param();
									if (auto p = targets[t].p) {
										Menu menu	= Menu::Popup();
										Menu::Item("Debug this Pixel", DebugWindow::ID_DEBUG_PIXEL).Param(t).AppendTo(menu);
										Control	c =	BitmapWindow(GetChildWindowPos(), p, tag(p.ID()), true);
										c(WM_ISO_CONTEXTMENU, (HMENU)menu);
										AddView(c, new_tab);
									}
									break;
								}
								case DX12StateControl::ST_OBJECT:
									AddView(MakeObjectView(GetChildWindowPos(), con, i.Param()), new_tab);
									break;

								case DX12StateControl::ST_SHADER: {
									AddView(MakeShaderViewer(GetChildWindowPos(), con, shaders[(int)i.Param()], con->shader_path), new_tab);
									break;
								}
								case DX12StateControl::ST_VERTICES:
									if (int x = i.StateImage()) {
										AddView(MakeBufferWindow(GetChildWindowPos(), "Vertices", 'VI', verts[x - 1]), new_tab);

									} else {
										int			nb		= verts.size32();
										auto		buffers	= new_auto(named<TypedBuffer>, nb);
										for (int i = 0; i < nb; i++)
											buffers[i]	= {none, verts[i]};

										MeshVertexWindow	*m	= new MeshVertexWindow(GetChildWindowPos(), "Vertices");
										VertexWindow		*vw	= MakeVertexWindow(GetChildWindowPos(), "Vertices", 'VI', buffers, nb, ix);
										MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1), topology, vbv[0].a, ix, one, culling, MeshWindow::PERSPECTIVE);
										m->SetPanes(*vw, *mw, 50);
										AddView(*m, new_tab);
									}
									break;

								case DX12StateControl::ST_OUTPUTS:
									AddView(MakeShaderOutput(i.Param(), ctrl), new_tab);
									break;

								case DX12StateControl::ST_DESCRIPTOR: {
									DESCRIPTOR	*d = i.Param();
									AddView(MakeDescriptorView(GetChildWindowPos(), "Desc", con, *d), new_tab);
									break;
								}
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

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return StackWindow::Proc(message, wParam, lParam);
}

int DX12BatchWindow::InitSimulator(dx::SimulatorDXBC &sim, DX12ShaderState &shader, int thread, dynamic_array<uint16> &indices, Topology2 &top) {
	int	start = thread < 0 ? 0 : thread;
	
	sim.Init(shader.GetUCode(), shader.GetUCodeAddr());

	switch (shader.stage) {
		case dx::PS: {
			dx::SimulatorDXBC	sim2;
			dx::OSGN			*vs_out = 0;
			dx::OSG5			*gs_out = 0;

			if (auto &gs = shaders[dx::GS]) {
				InitSimulator(sim2, gs, 0, indices, top);
				gs_out	= gs.DXBC()->GetBlob<dx::OSG5>();
			} else if (auto &ds = shaders[dx::DS]) {
				InitSimulator(sim2, ds, 0, indices, top);
				vs_out	= ds.DXBC()->GetBlob<dx::OSGN>();
			} else {
				auto	&vs = shaders[dx::VS];
				InitSimulator(sim2, vs, 0, indices, top);
				vs_out	= vs.DXBC()->GetBlob<dx::OSGN>();
			}
			
			sim2.Run();
			
			shader.InitSimulator(sim, *con, con->cache);

			auto	*ps_in	= shader.DXBC()->GetBlob<dx::ISGN>();
			int		n		= triangle_row(64 / 4);
			float	duv		= 1 / float(n * 2 - 1);

			if (thread >= 0) {
				sim.SetNumThreads(4);
				float	u	= triangle_col(thread / 4) * 2 * duv;
				float	v	= triangle_row(thread / 4) * 2 * duv;
				for (auto &in : ps_in->Elements())
					sim.InterpolateInputQuad(0, in.register_num, GetTriangle(sim2, in.name.get(ps_in), in.semantic_index, vs_out, gs_out, 0, 1, 2), u, v, u + duv, v + duv);
				return thread & 3;

			} else {
				sim.SetNumThreads(triangle_number(triangle_row(64 / 4)) * 4);
				for (auto &in : ps_in->Elements()) {
					auto	tri = GetTriangle(sim2, in.name.get(ps_in), in.semantic_index, vs_out, gs_out, 0, 1, 2);
					int		r	= in.register_num;
					for (int u = 0, t = 0; u < n; u++) {
						float	uf0 = float(u * 2) * duv;
						for (int v = 0; v <= u; v++, t += 4) {
							float	vf0 = float(v * 2) * duv;
							sim.InterpolateInputQuad(t, r, tri, uf0, vf0, uf0 + duv, vf0 + duv);
						}
					}
				}
				return 0;
			}
		}

		case dx::HS: {
			auto	&vs = shaders[dx::VS];
			dx::SimulatorDXBC	sim2(sim.NumInputControlPoints());
			InitSimulator(sim2, vs, 0, indices, top);
			sim2.Run();

			auto	 *vs_out	= vs.DXBC()->GetBlob<dx::OSGN>();
			auto	 *hs_in		= shader.DXBC()->GetBlob<dx::ISGN>();
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
					auto	s = sim2.GetRegFile(dx::Operand::TYPE_OUTPUT, x->register_num);
					if (sim.HasInput(in.register_num)) {
						copy(s, sim.GetRegFile(dx::Operand::TYPE_INPUT, in.register_num));
					} else {
						// set outputs in case there's no cp phase
						copy(s, sim.GetRegFile(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, in.register_num));
					}
				}
			}

			top		= GetTopology(sim.tess_output);
			shader.InitSimulator(sim, *con, con->cache);
			return thread;
		}

		case dx::DS: {
			auto	&hs = shaders[dx::HS];
			dx::SimulatorDXBC	sim2;
			InitSimulator(sim2, hs, 0, indices, top);
			sim2.Run();

			Tesselation	tess = GetTesselation(sim.tess_domain, sim2.GetRegFile(dx::Operand::TYPE_INPUT_PATCH_CONSTANT));

			if (thread < 0) {
				int	num	= tess.uvs.size32();
				sim.SetNumThreads(thread == -1 ? min(num, 64) : num);
			} else if (!sim.NumThreads()) {
				sim.SetNumThreads(1);
			}

			auto	*uvs	= tess.uvs + start;
			for (auto &i : sim.GetRegFile(dx::Operand::TYPE_INPUT_DOMAIN_POINT)) {
				i = float4{uvs->x, uvs->y, 1 - uvs->x - uvs->y, 0};
				++uvs;
			}

			//copy(sim2.PatchConsts(), sim.PatchConsts());

			auto	 *hs_out	= hs.DXBC()->GetBlob<dx::OSGN>();
			auto	 *ds_in		= shader.DXBC()->GetBlob<dx::ISGN>();
			for (auto &in : ds_in->Elements()) {
				if (auto *x = hs_out->find_by_semantic(in.name.get(ds_in), in.semantic_index))
					rcopy(sim.GetRegFile(dx::Operand::TYPE_INPUT_CONTROL_POINT, in.register_num), sim2.GetRegFile(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, x->register_num).begin());
			}
			shader.InitSimulator(sim, *con, con->cache);
			if (thread != -1)
				indices = move(tess.indices);
			return 0;
		}

		case dx::VS:
			if (thread < 0) {
				int	num	= ix.max_index() + 1;
				sim.SetNumThreads(thread == -1 ? min(num, 64) : num);
			} else if (!sim.NumThreads()) {
				sim.SetNumThreads(topology.VertsPerPrim());
			}

			shader.InitSimulator(sim, *con, con->cache);

			for (auto &i : vbv) {
				if (i.b >= 0) {
					if (ix && thread >= 0)
						sim.SetRegFile(dx::Operand::TYPE_INPUT, i.b, make_indexed_iterator(i.a.begin(), ix.begin() + start));
					else
						sim.SetRegFile(dx::Operand::TYPE_INPUT, i.b, i.a.begin() + start);
				}
			}

			indices	= ix;
			top		= topology;
			return 0;

		case dx::GS: {
			auto	in_topology	= GetTopology(sim.gs_input);
			auto	&vs			= shaders[dx::VS];
			int		vs_start	= in_topology.VertexFromPrim(start / sim.max_output, false);

			dx::SimulatorDXBC	sim2(in_topology.VertexFromPrim((start + sim.NumThreads()) / sim.max_output + 1, false) - vs_start);
			InitSimulator(sim2, vs, vs_start, indices, top);
			sim2.Run();
			
			if (thread < 0) {
				int	num	= in_topology.PrimFromVertex(ix.max_index(), false) + 1;
				sim.SetNumThreads(thread == -1 ? min(num, 64) : num);
			} else if (!sim.NumThreads()) {
				sim.SetNumThreads(1);
			}

			shader.InitSimulator(sim, *con, con->cache);

			auto	 *vs_out	= vs.DXBC()->GetBlob<dx::OSGN>();
			auto	 *gs_in		= shader.DXBC()->GetBlob<dx::ISGN>();
			for (auto &in : gs_in->Elements()) {
				if (auto *x = vs_out->find_by_semantic(in.name.get(gs_in), in.semantic_index))
					sim.SetRegFile(dx::Operand::TYPE_INPUT, in.register_num, sim2.GetRegFile(dx::Operand::TYPE_OUTPUT, x->register_num).begin());
			}
			top	= GetTopology(sim.gs_output);
			return 0;
		}

		default:
			shader.InitSimulator(sim, *con, con->cache);
			return 0;
	}
}

Control DX12BatchWindow::MakeShaderOutput(dx::SHADERSTAGE stage, bool mesh) {
	dx::SimulatorDXBC		sim;
	dynamic_array<uint16>	ib;
	Topology2				top;

	auto	&shader = shaders[stage == dx::CS ? 0 : stage];
	InitSimulator(sim, shader, mesh ? -2 : -1, ib, top);

	Control				c	= app::MakeShaderOutput(GetChildWindowPos(), sim, shader);

	if (!mesh)
		return c;

	switch (stage) {
		default:
			return c;

		case dx::VS: {
			bool			final	= !shaders[dx::DS] && !shaders[dx::GS];
			int				out_reg	= 0;
			if (auto *sig = find_if_check(shader.DXBC()->GetBlob<dx::OSGN>()->Elements(), [](const dx::SIG::Element &i) { return i.system_value == dx::SV_POSITION; }))
				out_reg = sig->register_num;

			malloc_block	output_verts(sizeof(float4p) * sim.NumThreads());
			copy(sim.GetRegFile(dx::Operand::TYPE_OUTPUT, out_reg), (float4p*)output_verts);

			MeshVertexWindow	*m	= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1),
				top,
				TypedBuffer(output_verts, sizeof(float[4]), ctypes.get_type<float[4]>()),
				ib.empty() ? ix : indices(ib),
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
			uint32				num_verts	= sim.NumThreads() * sim.MaxOutput();
			malloc_block		output_verts(sizeof(float4p) * num_verts);
			copy(sim.GetStreamFileAll(0), (float4p*)output_verts);

			MeshVertexWindow	*m	= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1),
				top,
				TypedBuffer(output_verts, sizeof(float4p), ctypes.get_type<float[4]>()),
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
			Tesselation			tess	= GetTesselation(sim.tess_domain, sim.GetRegFile(dx::Operand::TYPE_INPUT_PATCH_CONSTANT));
			SplitterWindow		*m		= new SplitterWindow(GetChildWindowPos(), "Shader Output", SplitterWindow::SWF_VERT | SplitterWindow::SWF_PROP);
			MeshWindow			*mw		= MakeMeshView(m->GetPanePos(1),
				top,
				TypedBuffer(memory_block(tess.uvs.begin(), tess.uvs.end()), sizeof(tess.uvs[0]), ctypes.get_type<float[2]>()),
				tess.indices,
				one, culling,
				MeshWindow::PERSPECTIVE
			);	
			m->SetPanes(c, *mw, 50);
			return *m;
		}

		case dx::DS: {
			bool				final	= !shaders[dx::GS];
			malloc_block		output_verts(sizeof(float4p) * sim.NumThreads());
			copy(sim.GetRegFile(dx::Operand::TYPE_OUTPUT), (float4p*)output_verts);

			MeshVertexWindow	*m	= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
			MeshWindow			*mw	= MakeMeshView(m->GetPanePos(1),
				top,
				TypedBuffer(output_verts, sizeof(float4p), ctypes.get_type<float[4]>()),
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

void DX12BatchWindow::VertexMenu(dx::SHADERSTAGE stage, int i, ListViewControl lv) {
	auto	&shader = shaders[stage];
	Menu	menu	= Menu::Popup();

	menu.Append("Debug", 1);
	if (stage == dx::VS && ix) {
		menu.Separator();
		menu.Append(format_string("Next use of index %i", ix[i]), 2);
		menu.Append(format_string("Highlight all uses of index %i", ix[i]), 3);
	}

	menu.Append("Show Simulated Trace", 4);

	switch (menu.Track(*this, GetMousePos(), TPM_NONOTIFY | TPM_RETURNCMD)) {
		case 1: {
			dynamic_array<uint16>	ib;
			Topology2				top;
			auto	*debugger	= new DX12ShaderDebuggerWindow(GetChildWindowPos(), "Debugger", shader, con, GetSettings("General/shader source").GetInt(1));
			int		thread		= InitSimulator(debugger->sim, shader, i, ib, top);
			debugger->SetThread(thread);
			PushView(debugger->control());
			break;
		}

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
		case 3: {
			int	x = ix[i];
			for (int j = 0, n = ix.num; j < n; ++j) {
				if (ix[j] == x)
					lv.SetItemState(j, LVIS_SELECTED);
			}
			break;
		}
		case 4: {
			dynamic_array<uint16>	ib;
			Topology2				top;
			dx::SimulatorDXBC		sim;
			int						thread = InitSimulator(sim, shader, i, ib, top);
			PushView(MakeDXBCTraceWindow(GetChildWindowPos(), sim, thread, 1000));
			break;
		}

	}
	menu.Destroy();
}

Control DX12BatchWindow::DebugPixel(uint32 target, const Point &pt) {
	dx::SimulatorDXBC	sim2;
	dx::OSGN			*vs_out = 0;
	dx::OSG5			*gs_out = 0;

	dynamic_array<uint16>	ib;
	Topology2				top;

	if (auto &gs = shaders[dx::GS]) {
		InitSimulator(sim2, gs, -2, ib, top);
		gs_out	= gs.DXBC()->GetBlob<dx::OSG5>();
	} else if (auto &ds = shaders[dx::DS]) {
		InitSimulator(sim2, ds, -2, ib, top);
		vs_out	= ds.DXBC()->GetBlob<dx::OSGN>();
	} else {
		auto	&vs = shaders[dx::VS];
		InitSimulator(sim2, vs, -2, ib, top);
		vs_out	= vs.DXBC()->GetBlob<dx::OSGN>();
	}
			
	sim2.Run();
			
	dynamic_array<float4p>	output_verts;
	if (gs_out)
		output_verts = sim2.GetStreamFileAll(0);
	else
		output_verts = sim2.GetRegFile(dx::Operand::TYPE_OUTPUT, 0);

	uint32	num_verts	= ib ? ib.size32() : output_verts.size32();
	uint32	num_prims	= top.p2v.verts_to_prims(num_verts);

	dynamic_array<cuboid>	exts(num_prims);
	cuboid					*ext	= exts;
	if (ib) {
		auto	prims = make_prim_iterator(top.p2v, make_indexed_iterator(output_verts.begin(), make_const(ib.begin())));
		for (auto &&i : make_range_n(prims, num_prims))
			*ext++ = get_extent<position3>(i);
	} else {
		auto	prims = make_prim_iterator(top.p2v, output_verts.begin());
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
		auto	prims	= make_prim_iterator(top.p2v, make_indexed_iterator(output_verts.begin(), make_const(ib.begin())));
		int		face	= oct.shoot_ray(ray, 0.25f, [prims](int i, param(ray3) r, float &t) {
			return prim_check_ray(prims[i], r, t);
		});
		if (face < 0)
			return Control();

		tri3 = prim_triangle(prims[face]);
		copy(make_prim_iterator(top.p2v, ix.begin())[face], v);
	} else {
		auto	prims	= make_prim_iterator(top.p2v, output_verts.begin());
		int		face	= oct.shoot_ray(ray, 0.25f, [prims](int i, param(ray3) r, float &t) {
			return prim_check_ray(prims[i], r, t);
		});
		if (face < 0)
			return Control();

		tri3 = prim_triangle(prims[face]);
		copy(make_prim_iterator(top.p2v, int_iterator<int>(0))[face], v);
	}
	
	auto	&ps			= shaders[dx::PS];
	auto	*debugger	= new DX12ShaderDebuggerWindow(GetChildWindowPos(), "Debugger", ps, con, GetSettings("General/shader source").GetInt(1));
	auto	*ps_in		= ps.DXBC()->GetBlob<dx::ISGN>();

	float2		pt1		= (qpt - viewport.y.xy + one) / viewport.x.xy;
	ray3		ray1	= ray3(position3(pt1, zero), (float3)z_axis);

	plane		p		= tri3.plane();
	float3x4	para	= tri3.inv_matrix();
	float3		uv0		= para * (ray & p);
	float3		uv1		= para * (ray1 & p);

	debugger->sim.Init(ps.GetUCode(), ps.GetUCodeAddr());
	debugger->sim.SetNumThreads(4);
	debugger->SetThread(thread);
	ps.InitSimulator(debugger->sim, *con, con->cache);

	for (auto &in : ps_in->Elements())
		debugger->sim.InterpolateInputQuad(0, in.register_num, GetTriangle(sim2, in.name.get(ps_in), in.semantic_index, vs_out, gs_out, v[0], v[1], v[2]), uv0.x, uv0.y, uv1.x, uv1.y);

	debugger->Update();
	return debugger->control();
}

//-----------------------------------------------------------------------------
//	ViewDX12GPU
//-----------------------------------------------------------------------------

class ViewDX12GPU :  public SplitterWindow, public DX12Connection {
public:
	static IDFMT		init_format;
private:
	static win::Colour	colours[];
	static uint8		cursor_indices[][3];

	RefControl<ToolBarControl>	toolbar_gpu;
	ColourTree			tree	= ColourTree(colours, DX12cursors, cursor_indices);
	win::Font			italics;
	HTREEITEM			context_item;
	IDFMT				format;

	void		TreeSelection(HTREEITEM hItem);
	void		TreeDoubleClick(HTREEITEM hItem);

public:
	static ViewDX12GPU*	Cast(Control c)	{
		return (ViewDX12GPU*)SplitterWindow::Cast(c);
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
		HTREEITEM h = TVI_ROOT;
		if (offset >= device_object->info.size32()) {
			CallRecord	*call = lower_boundc(calls, offset);
			h = FindOffset(tree, TVI_ROOT, call[-1].addr);
		}
		h = FindOffset(tree, h, offset);
		if (h != TVI_ROOT) {
			tree.SetSelectedItem(h);
			tree.EnsureVisible(h);
		}
	}
	void SelectBatch(uint32 batch) {
		SetOffset(batches[batch].end);
	}
	void SelectBatch(BatchList &b, bool always_list = false) {
		int batch = ::SelectBatch(*this, GetMousePos(), b, always_list);
		if (batch >= 0)
			SelectBatch(batch);
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	MakeTree();
	void	ExpandTree(HTREEITEM h);

	ViewDX12GPU(app::MainWindow &_main, const WindowPos &pos, HANDLE process, const char *proc_path);
	ViewDX12GPU(app::MainWindow &_main, const WindowPos &pos);
};

//-----------------------------------------------------------------------------
//	ListViews
//-----------------------------------------------------------------------------

void GetUsedAt(BatchListArray &usedat, const DX12Assets *assets, RecObject::TYPE type) {
	for (uint32 b = 0, nb = assets->batches.size32() - 1; b < nb; b++) {
		auto	usage = assets->GetUsage(b);
		for (const DX12Assets::use *u = usage.begin(), *ue = usage.end(); u != ue; ++u) {
			if (u->type == type)
				usedat[u->index].push_back(b);
		}
	}
}

//-----------------------------------------------------------------------------
//	DX12DescriptorList
//-----------------------------------------------------------------------------

const char *desc_types[] = {
	"NONE", "CBV", "SRV", "UAV", "RTV", "DSV", "SMP",
	"SSMP",
	"PCBV", "PSRV", "PUAV", "IMM", 0, 0, 0, 0,
};

template<> field fields<DESCRIPTOR>::f[] = {
	{"type",		0,4,	0,0,					desc_types},
	{"resource",	64,64,	field::MODE_CUSTOM,0,	sHex},
	{0,				128,0,	0,2,					(const char**)union_fields<
	_none,
	D3D12_CONSTANT_BUFFER_VIEW_DESC,
	D3D12_SHADER_RESOURCE_VIEW_DESC,
	D3D12_UNORDERED_ACCESS_VIEW_DESC,
	D3D12_RENDER_TARGET_VIEW_DESC,
	D3D12_DEPTH_STENCIL_VIEW_DESC,
	D3D12_SAMPLER_DESC,
	D3D12_STATIC_SAMPLER_DESC,
	D3D12_GPU_VIRTUAL_ADDRESS,
	D3D12_GPU_VIRTUAL_ADDRESS,
	D3D12_GPU_VIRTUAL_ADDRESS
	>::p},
	0,
};

Control MakeDescriptorView(const WindowPos &wpos, const char *title, DX12Connection *con, const DESCRIPTOR &desc) {
	TreeControl		tree(wpos, title, Control::CHILD | Control::CLIPSIBLINGS | Control::VISIBLE | Control::VSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, Control::CLIENTEDGE);
	RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);

	rt.AddFields(rt.AddText(TVI_ROOT, "Descriptor"), &desc);

	DX12Connection::ObjectRecord	*obj	= con->FindObject((uint64)desc.res);
	const RecResource				*r		= 0;
	if (obj) {
		r = obj->info;
		rt.AddFields(rt.AddText(TVI_ROOT, obj->GetName()), r);
	}

	switch (desc.type) {
		case DESCRIPTOR::CBV: {
			SplitterWindow	*split = new SplitterWindow(wpos, 0, SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
			split->SetPanes(tree, BinaryWindow(split->_GetPanePos(1), ISO::MakeBrowser(memory_block(con->cache(uint64(desc.cbv.BufferLocation), desc.cbv.SizeInBytes)))), 350);
			return *split;
		}

		case DESCRIPTOR::SRV:
			switch (desc.srv.ViewDimension) {
				case D3D12_SRV_DIMENSION_BUFFER:
					if (obj) {
						DXGI_COMPONENTS	format	= r->Format;
						if (desc.srv.Format) {
							format = desc.srv.Format;
							format.chans = Rearrange((DXGI_COMPONENTS::SWIZZLE)format.chans, (DXGI_COMPONENTS::SWIZZLE)desc.srv.Shader4ComponentMapping);
						}
						uint32	stride	= desc.srv.Buffer.StructureByteStride;
						if (!stride)
							stride = format.Size();
						SplitterWindow		*split = new SplitterWindow(wpos, 0, SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
						split->SetPanes(tree, MakeBufferWindow(split->_GetPanePos(1), "", ID(),
							TypedBuffer(con->cache(uint64(r->gpu) + desc.srv.Buffer.FirstElement * stride, desc.srv.Buffer.NumElements * stride), stride, dx::to_c_type(format))
							), 350);
						return *split;
					}
					break;
				default:
					if (auto bm = con->GetBitmap(desc)) {
						SplitterWindow	*split = new SplitterWindow(wpos, 0, SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
						split->SetPanes(tree, BitmapWindow(split->_GetPanePos(1), bm, 0, true), 350);
						return *split;
					}
					break;
			}
			break;

		case DESCRIPTOR::UAV:
//			D3D12_UNORDERED_ACCESS_VIEW_DESC
			break;

		case DESCRIPTOR::RTV:
			if (auto bm = con->GetBitmap(desc)) {
				SplitterWindow	*split = new SplitterWindow(wpos, 0, SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
				split->SetPanes(tree, BitmapWindow(split->_GetPanePos(1), bm, 0, true), 350);
				return *split;
			}
			break;

		case DESCRIPTOR::DSV:
			if (auto bm = con->GetBitmap(desc)) {
				SplitterWindow	*split = new SplitterWindow(wpos, 0, SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
				split->SetPanes(tree, BitmapWindow(split->_GetPanePos(1), bm, 0, true), 350);
				return *split;
			}
			break;

		case DESCRIPTOR::PCBV:
		case DESCRIPTOR::PSRV:
		case DESCRIPTOR::PUAV:
		case DESCRIPTOR::IMM:
		case DESCRIPTOR::VBV:
		case DESCRIPTOR::IBV:
			break;
	}
	return tree;
}

void InitDescriptorHeapView(ListViewControl lv) {
	lv.SetView(ListViewControl::DETAIL_VIEW);
	lv.SetExtendedStyle(ListViewControl::GRIDLINES | ListViewControl::DOUBLEBUFFER | ListViewControl::FULLROWSELECT);
	lv.AddColumns(
		"index",		50,
		"cpu handle",	100,
		"gpu handle",	100
	);
	int	nc = MakeColumns(lv, fields<DESCRIPTOR>::f, IDFMT_CAMEL, 3);
	while (nc < 20)
		ListViewControl::Column("").Width(100).Insert(lv, nc++);
}

void GetHeapDispInfo(string_accum &sa, ListViewControl &lv, DX12Assets *assets, const RecDescriptorHeap *heap, int row, int col) {
	auto	&d	= heap->descriptors[row];
	switch (col) {
		case 0:		sa << row; break;
		case 1:		sa << "0x" << hex(heap->cpu.ptr + row * heap->stride);
		case 2:		sa << "0x" << hex(heap->gpu.ptr + row * heap->stride);
		default: {
			uint32	offset = 0;
			const uint32 *p = (const uint32*)&d;
			if (const field *pf = FieldIndex(fields<DESCRIPTOR>::f, col - 3, p, offset, true))
				RegisterList(lv, assets, col > 4 ? IDFMT_FIELDNAME : IDFMT_LEAVE).FillSubItem(sa, pf, p, offset).term();
			break;
		}
	}
}

struct DX12DescriptorHeapControl : CustomListView<DX12DescriptorHeapControl, Subclass<DX12DescriptorHeapControl, ListViewControl>> {
	enum {ID = 'DH'};
	DX12Assets &assets;
	const RecDescriptorHeap	*heap;

	DX12DescriptorHeapControl(const WindowPos &wpos, const char *caption, DX12Assets &assets, const RecDescriptorHeap *rec) : Base(wpos, caption, CHILD | CLIPSIBLINGS | VISIBLE | VSCROLL, CLIENTEDGE | OWNERDATA, ID), assets(assets), heap(rec) {
		InitDescriptorHeapView(*this);
		SetCount(heap->count, LVSICF_NOINVALIDATEALL);
	}

	void	GetDispInfo(string_accum &sa, int row, int col) {
		GetHeapDispInfo(sa, *this, &assets, heap, row, col);
	}

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			uint64		h	= heap->cpu.ptr + heap->stride * row;

			switch (col) {
				case 0: {
					uint32	offset = main->FindDescriptorChange(h, heap->stride);
					main->SetOffset(offset);
					break;
				}
				case 4: {//resource
					if (auto *rec = main->FindObject((uint64)heap->descriptors[row].res)) {
						MakeObjectView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), main, rec);
						break;
					}
				}
				default:
					MakeDescriptorView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), to_string(hex(h)), main, heap->descriptors[row]);
					break;
			}
		}
	}
};

struct DX12DescriptorList : CustomListView<DX12DescriptorList, Subclass<DX12DescriptorList, ListViewControl>> {
	enum {ID = 'DL'};
	DX12Connection *con;
	win::Font		bold;

	DX12DescriptorList(const WindowPos &wpos, DX12Connection *_con) : con(_con) {
		Create(wpos, "Descriptors", CHILD | CLIPSIBLINGS | VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA, NOEX, ID);
		InitDescriptorHeapView(*this);
		//EnableGroups();
		bold = GetFont().GetParams().Weight(FW_BOLD);

		size_t	total = 0;
		for (auto &i : con->descriptor_heaps)
			total += i->count + 1;
		SetCount(total, LVSICF_NOINVALIDATEALL);
	}

	void	GetDispInfo(string_accum &sa, int row, int col) {
		const RecDescriptorHeap	*heap = nullptr;
		for (auto &i : con->descriptor_heaps) {
			if (row == 0) {
				if (col == 0)
					sa << i.obj->name << " : 0x" << hex(i->cpu.ptr) << " x " << i->count;
				return;
			}
			--row;
			if (row < i->count) {
				heap = i;
				break;
			}
			row -= i->count;
		}

		if (heap)
			GetHeapDispInfo(sa, *this, con, heap, row, col);
	}

	int		CustomDraw(NMLVCUSTOMDRAW *cd) {
		switch (cd->nmcd.dwDrawStage) {
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;
			case CDDS_ITEMPREPAINT:
				if (cd->nmcd.rc.top) {
					int	row = cd->nmcd.dwItemSpec;
					for (auto &i : con->descriptor_heaps) {
						if (row == 0) {
							DeviceContext	dc(cd->nmcd.hdc);
							Rect			rc(cd->nmcd.rc);
							dc.Select(bold);
							dc.TextOut(rc.TopLeft(), buffer_accum<100>() << i.obj->name << " : 0x" << hex(i->cpu.ptr) << " x " << i->count);
							//return CDRF_NEWFONT;
							return CDRF_SKIPDEFAULT | CDRF_SKIPPOSTPAINT | CDRF_NOTIFYSUBITEMDRAW;
						}
						--row;
						if (row < i->count)
							break;
						row -= i->count;
					}
				}
				break;

			case CDDS_ITEMPREPAINT|CDDS_SUBITEM:
				return CDRF_SKIPDEFAULT | CDRF_SKIPPOSTPAINT;
		}
		return CDRF_DODEFAULT;
	}

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			const RecDescriptorHeap	*heap = 0;
			for (auto &h : con->descriptor_heaps) {
				if (row < h->count) {
					heap = h;
					break;
				}
				row -= h->count;
			}
			uint64		h	= heap->cpu.ptr + heap->stride * row;

			switch (col) {
				case 0: {
					uint32	offset = con->FindDescriptorChange(h, heap->stride);
					main->SetOffset(offset);
					break;
				}
				case 4: {//resource
					if (auto *rec = main->FindObject((uint64)heap->descriptors[row].res)) {
						MakeObjectView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), main, rec);
						break;
					}
				}
				default:
					MakeDescriptorView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), to_string(hex(h)), main, heap->descriptors[row]);
					break;
			}
		}
	}
};

//-----------------------------------------------------------------------------
//	DX12ResourcesList
//-----------------------------------------------------------------------------

struct DX12ResourcesList :  EditableListView<DX12ResourcesList, Subclass<DX12ResourcesList, EntryTable<DX12Assets::ResourceRecord>>> {
	enum {ID = 'RL'};
	DX12Connection *con;
	ImageList		images;

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			switch (col) {
	/*			case 0:
					EditName(row);
					break;*/
				case 1:
					ViewDX12GPU::Cast(from)->SelectBatch(GetBatches(row));
					break;
				default: {
					Busy		bee;
					auto		*t	= GetEntry(row);
					auto		*r	= t->res();
					if (r->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
						//memory_block	data = con->cache(uint64(r->data.start), r->data.size);
						BinaryWindow(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), ISO::MakeBrowser(memory_block(con->cache(uint64(r->gpu), r->data_size))));

					} else if (auto bm = con->GetBitmap(t)) {
						BitmapWindow(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), bm, t->GetName(), true);
					}
					break;
				}
			}
		}
	}

	//void	GetDispInfo(string_accum &sa, int row, int col) {
	//	row = sorted_order[row];
	//	const T	&u = table[row];
	//	switch (col) {
	//		case 0:		sa << u.GetName(); break;
	//		case 1:		WriteBatchList(sa, usedat[row]).term(); break;
	//		case 2:		WriteBatchList(sa, writtenat[row]).term(); break;
	//		case 3:		sa << GetSize((T&)u); break;
	//		case 5:		sa << "0x" << hex(u.obj); break;
	//		default: {
	//			uint32	offset = 0;
	//			const uint32 *p = (const uint32*)&u;
	//			if (const field *pf = FieldIndex(fields<T>::f, col - 4, p, offset, true))
	//				RegisterList(*this, this, format | (col > 5) * IDFMT_FIELDNAME).FillSubItem(sa, pf, p, offset).term();
	//			break;
	//		}
	//	}
	//}

	DX12ResourcesList(const WindowPos &wpos, DX12Connection *_con) : Base(_con->resources), con(_con)
		, images(ImageList::Create(DeviceContext::ScreenCaps().LogPixels() * (2 / 3.f), ILC_COLOR32, 1, 1))
	{
		Create(wpos, "Resources", CHILD | CLIPSIBLINGS | VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS, NOEX, ID);
		for (int i = 0, nc = NumColumns(); i < 8; i++)
			Column("").Width(100).Insert(*this, nc++);

		SetIcons(images);
		SetSmallIcons(images);
		SetView(DETAIL_VIEW);
		TileInfo(4).Set(*this);
		images.Add(win::Bitmap::Load("IDB_BUFFER", 0, images.GetIconSize()));
		images.Add(win::Bitmap::Load("IDB_NOTLOADED", 0, images.GetIconSize()));
			   
		RunThread([this]{
			addref();

			for (uint32 b = 0, nb = con->batches.size32() - 1; b < nb; b++) {
				auto	usage = con->GetUsage(b);
				for (const DX12Assets::use *u = usage.begin(), *ue = usage.end(); u != ue; ++u) {
					if (u->type == RecObject::Resource)
						usedat[con->objects[u->index].index].push_back(b);
				}
			}

			//GetUsedAt(usedat, con, RecObject::Resource);
			static const uint32	cols[]	= {2, 9, 10, 22};

			for (auto *i = table.begin(), *e = table.end(); i != e; ++i) {
				JobQueue::Main().add([this, i] {
					auto	*r	= i->res();
					int		x	= r->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? 0 : !r->HasData() ? 1 : GetThumbnail(*this, images, con, i);
					int		j	= i - table.begin();
					AddEntry(*i, j, x);
					GetItem(j).TileInfo(cols).Set(*this);
				});
			}

			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	DX12ShadersList
//-----------------------------------------------------------------------------

struct DX12ShadersList : EditableListView<DX12ShadersList, Subclass<DX12ShadersList, EntryTable<DX12Assets::ShaderRecord>>> {
	enum {ID = 'SH'};
	DX12Connection *con;

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			switch (col) {
	/*			case 0:
					EditName(row);
					break;*/
				case 1:
					ViewDX12GPU::Cast(from)->SelectBatch(GetBatches(row));
					break;
				default:
					Busy(), MakeShaderViewer(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), con, GetEntry(row), con->shader_path);
					break;
			}
		}
	}

	DX12ShadersList(const WindowPos &wpos, DX12Connection *_con) : Base(_con->shaders), con(_con) {
		Create(wpos, "Shaders", CHILD | CLIPSIBLINGS | VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS, NOEX, ID);
		Init();

		RunThread([this]{
			addref();
			GetUsedAt(usedat, con, RecObject::Shader);

			for (auto *i = table.begin(), *e = table.end(); i != e; ++i) {
				int		j	= i - table.begin();
				AddEntry(*i, j);
				GetItem(j).Set(*this);
			}

			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	DX12ObjectsList
//-----------------------------------------------------------------------------

Control MakeObjectTreeView(const WindowPos &wpos, DX12Connection *con, DX12Assets::ObjectRecord *obj) {
	TreeControl		tree(wpos, obj->GetName(), Control::CHILD | Control::CLIPSIBLINGS | Control::VISIBLE | Control::VSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, Control::CLIENTEDGE);
	RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);
	HTREEITEM		h	= rt.AddText(TVI_ROOT, obj->GetName(), 0);

	switch (obj->type) {
		case RecObject::RootSignature: {
			com_ptr<ID3D12RootSignatureDeserializer>	ds;
			HRESULT			hr = D3D12CreateRootSignatureDeserializer(obj->info, obj->info.length(), ds.uuid(), (void**)&ds);
			const D3D12_ROOT_SIGNATURE_DESC *desc = ds->GetRootSignatureDesc();
			HTREEITEM		h2;

			h2 = rt.AddText(h, "Parameters");
			for (int i = 0; i < desc->NumParameters; i++) {
				HTREEITEM					h3		= rt.AddText(h2, format_string("[%i]", i));
				const D3D12_ROOT_PARAMETER	*param	= desc->pParameters + i;
				if (param->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
					rt.AddField(h3, field::make("ParameterType", &D3D12_ROOT_PARAMETER::ParameterType), (uint32*)param);
					rt.AddArray(h3, param->DescriptorTable.pDescriptorRanges, param->DescriptorTable.NumDescriptorRanges);
					rt.AddField(h3, field::make("ShaderVisibility", &D3D12_ROOT_PARAMETER::ShaderVisibility), (uint32*)param);
				} else {
					rt.AddFields(h3, param);
				}
			}

			h2 = rt.AddText(h, "Static Samplers");
			rt.AddArray(h2, desc->pStaticSamplers, desc->NumStaticSamplers);

			rt.AddField(h, field::make("Flags", &D3D12_ROOT_SIGNATURE_DESC::Flags), (uint32*)desc);
			break;
		}

		case RecObject::PipelineState:
			switch (*(int*)obj->info) {
				case 0:
					rt.AddFields(h, (const D3D12_GRAPHICS_PIPELINE_STATE_DESC*)(obj->info + 8));
					break;
				case 1:
					rt.AddFields(h, (const D3D12_COMPUTE_PIPELINE_STATE_DESC*)(obj->info + 8));
					break;
				case 2: {
					auto	t = rmap_unique<KM, D3D12_PIPELINE_STATE_STREAM_DESC>(obj->info + 8);
					for (auto &sub : make_next_range<D3D12_PIPELINE_STATE_STREAM_DESC_SUBOBJECT>(const_memory_block(t->pPipelineStateSubobjectStream, t->SizeInBytes))) {
						switch (sub.t.u.t) {
							//case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:		rt.AddField(rt.AddText(h, "pRootSignature"),		&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:					rt.AddFields(rt.AddText(h, "VS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:					rt.AddFields(rt.AddText(h, "PS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:					rt.AddFields(rt.AddText(h, "DS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:					rt.AddFields(rt.AddText(h, "HS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:					rt.AddFields(rt.AddText(h, "GS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:					rt.AddFields(rt.AddText(h, "CS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:			rt.AddFields(rt.AddText(h, "StreamOutput"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:					rt.AddFields(rt.AddText(h, "BlendState"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:			rt.AddFields(rt.AddText(h, "SampleMask"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:			rt.AddFields(rt.AddText(h, "RasterizerState"),		&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:			rt.AddFields(rt.AddText(h, "DepthStencilState"),	&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:			rt.AddFields(rt.AddText(h, "InputLayout"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:	rt.AddField(rt.AddText(h, "IBStripCutValue"),		&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:	rt.AddField(rt.AddText(h, "PrimitiveTopologyType"),	&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:	rt.AddFields(rt.AddText(h, "RTVFormats"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:	rt.AddFields(rt.AddText(h, "DSVFormat"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:			rt.AddFields(rt.AddText(h, "SampleDesc"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:				rt.AddFields(rt.AddText(h, "NodeMask"),				&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:			rt.AddFields(rt.AddText(h, "CachedPSO"),			&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:					rt.AddFields(rt.AddText(h, "Flags"),				&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:		rt.AddFields(rt.AddText(h, "DepthStencilState"),	&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:		rt.AddFields(rt.AddText(h, "ViewInstancing"),		&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:					rt.AddFields(rt.AddText(h, "AS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>(sub)); break;
							case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:					rt.AddFields(rt.AddText(h, "MS"),					&get<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>(sub)); break;
						}
					}
					break;
				}
			}
			break;

		//case RecObject::CommandSignature:
		//	rt.AddFields(h, (const map_t<RTM, D3D12_COMMAND_SIGNATURE_DESC>*)obj->info);
		//	//rt.AddFields(h, (D3D12_COMMAND_SIGNATURE_DESC*)rmap_struct<RTM, D3D12_COMMAND_SIGNATURE_DESC>(obj->info));
		//	break;

		case RecObject::CommandQueue:
			rt.AddFields(h, (const D3D12_COMMAND_QUEUE_DESC*)obj->info);
			break;

		default:
			tree.ExpandItem(rt.AddFields(h, obj));
			break;
	}

	return tree;
}

Control MakeObjectView(const WindowPos &wpos, DX12Connection *con, DX12Assets::ObjectRecord *obj) {

	switch (obj->type) {
		case RecObject::Heap: {
			const RecHeap		*r = obj->info;

			SplitterWindow	*split = new SplitterWindow(wpos, obj->GetName(), SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
			split->SetPanes(MakeObjectTreeView(split->_GetPanePos(0), con, obj), BinaryWindow(split->_GetPanePos(1), ISO::MakeBrowser(memory_block(con->cache(uint64(r->gpu), r->SizeInBytes)))), 350);
			return *split;
		}

		case RecObject::Resource: {
			const RecResource	*r = obj->info;

			if (r->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
				SplitterWindow	*split = new SplitterWindow(wpos, obj->GetName(), SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
				split->SetPanes(MakeObjectTreeView(split->_GetPanePos(0), con, obj), BinaryWindow(split->_GetPanePos(1), ISO::MakeBrowser(memory_block(con->cache(uint64(r->gpu), r->data_size)))), 350);
				return *split;

			} else {
				DESCRIPTOR	d = MakeDefaultDescriptor(obj);
				if (auto bm = con->GetBitmap(d)) {
					SplitterWindow	*split = new SplitterWindow(wpos, obj->GetName(), SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
					split->SetPanes(MakeObjectTreeView(split->_GetPanePos(0), con, obj), BitmapWindow(split->_GetPanePos(1), bm, obj->GetName(), true), 350);
					return *split;
				}
			}
			break;
		}

		case RecObject::DescriptorHeap: {
			SplitterWindow	*split = new SplitterWindow(wpos, obj->GetName(), SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
			split->SetPanes(MakeObjectTreeView(split->_GetPanePos(0), con, obj), *new DX12DescriptorHeapControl(split->_GetPanePos(1), obj->GetName(), *con, obj->info), 350);
			return *split;
		}

		default:
			break;
	}
	return MakeObjectTreeView(wpos, con, obj);
}

struct DX12ObjectsList : EditableListView<DX12ObjectsList, Subclass<DX12ObjectsList, EntryTable<DX12Assets::ObjectRecord>>> {
	enum {ID = 'OL'};
	DX12Connection *con;

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			switch (col) {
				case 0:
					main->SetOffset(main->FindObjectCreation(GetEntry(row)->obj));
					//EditName(row);
					break;
				case 1:
					ViewDX12GPU::Cast(from)->SelectBatch(GetBatches(row));
					break;
				default:
					Busy(), MakeObjectView(main->Dock(flags & LVKF_SHIFT ? DOCK_TAB : DOCK_TABID, 'XX'), con, GetEntry(row));
					break;
			}
		}
	}

	void	operator()(string_accum &sa, const field *pf, const uint32le *p, uint32 offset) const {
		uint64		v = pf->get_raw_value(p, offset);

		if (pf->is_type<D3D12_GPU_DESCRIPTOR_HANDLE>()) {
			D3D12_GPU_DESCRIPTOR_HANDLE h;
			h.ptr = v;
			sa << "0x" << hex(v) << con->ObjectName(h);

		} else if (pf->is_type<D3D12_CPU_DESCRIPTOR_HANDLE>()) {
			D3D12_CPU_DESCRIPTOR_HANDLE h;
			h.ptr = v;
			sa << "0x" << hex(v) << con->ObjectName(h);

		} else if (v == 0) {
			sa << "nil";

		} else if (auto *rec = con->FindObject(v)) {
			sa << rec->name;

		} else if (auto *rec = con->FindShader(v)) {
			sa << rec->name;

		} else {
			sa << "(unfound)0x" << hex(v);
		}
	}

	//void	GetDispInfo(string_accum &sa, int row, int col) {
	//	row = sorted_order[row];
	//	const T	&u = table[row];
	//	switch (col) {
	//		case 0:		sa << u.GetName(); break;
	//		case 1:		WriteBatchList(sa, usedat[row]).term(); break;
	//		case 2:		WriteBatchList(sa, writtenat[row]).term(); break;
	//		case 3:		sa << GetSize((T&)u); break;
	//		case 5:		sa << "0x" << hex(u.obj); break;
	//		default: {
	//			uint32	offset = 0;
	//			const uint32 *p = (const uint32*)&u;
	//			if (const field *pf = FieldIndex(fields<T>::f, col - 4, p, offset, true))
	//				RegisterList(*this, this, format | (col > 5) * IDFMT_FIELDNAME).FillSubItem(sa, pf, p, offset).term();
	//			break;
	//		}
	//	}
	//}

	DX12ObjectsList(const WindowPos &wpos, DX12Connection *_con) : Base(_con->objects), con(_con) {
		Create(wpos, "Objects", CHILD | CLIPSIBLINGS | VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS, NOEX, ID);
		addref();
		int	nc = NumColumns();
		for (int i = 0; i < 8; i++)
			ListViewControl::Column("").Width(100).Insert(*this, nc++);

		SetView(DETAIL_VIEW);

		RunThread([this]{
			addref();

			for (uint32 b = 0, nb = con->batches.size32() - 1; b < nb; b++) {
				auto	usage = con->GetUsage(b);
				for (const DX12Assets::use *u = usage.begin(), *ue = usage.end(); u != ue; ++u) {
					if (u->type != RecObject::Shader)
						usedat[u->index].push_back(b);
				}
			}

			for (auto *i = table.begin(), *e = table.end(); i != e; ++i) {
				JobQueue::Main().add([this, i] {
					int	j = i - table.begin();
					AddEntry(this, *i, j);
				});
			}

			release();
			return 0;
		});
	}
	~DX12ObjectsList() {}
};

//-----------------------------------------------------------------------------
//	DX12TimingWindow
//-----------------------------------------------------------------------------

class DX12TimingWindow : public TimingWindow {
	DX12Connection	&con;
	float			rfreq;
	
	void	Paint(const win::DeviceContextPaint &dc);
	int		GetBatch(float t)		const	{ return t < 0 ? 0 : con.batches.index_of(con.GetBatchByTime(uint64(t * con.frequency))); }
	float	GetBatchTime(int i)		const	{ return float(con.batches[i].timestamp) * rfreq; }
	void	SetBatch(int i);
	void	GetTipText(string_accum &&acc, float t) const;
	void	Reset();

public:
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	DX12TimingWindow(const WindowPos &wpos, Control owner, DX12Connection &con) : TimingWindow(wpos, owner), con(con) {
		Reset();
		Rebind(this);
	}
	void Refresh() {
		Reset();
		Invalidate();
	}
};

void DX12TimingWindow::Reset() {
	time		= 0;
	last_batch	= 0;
	rfreq		= 1.f / con.frequency;
	tscale		= ((con.batches.back().timestamp - con.batches.front().timestamp)) * rfreq;

	float maxy	= 1;
	for (auto &b : con.batches)
		maxy = max(maxy, (b.stats.PSInvocations + b.stats.CSInvocations) / float(b.Duration()));
	yscale		= maxy * con.frequency * 1.1f;
}

void DX12TimingWindow::SetBatch(int i) {
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
void DX12TimingWindow::GetTipText(string_accum &&acc, float t) const {
	acc << "t=" << t * 1000 << "ms";
	int	i = GetBatch(t);
	if (i < con.batches.size())
		acc << "; batch=" << i - 1 << " (" << GetBatchTime(i) << "ms)";
}

LRESULT DX12TimingWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
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
					break;
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

D3D12_QUERY_DATA_PIPELINE_STATISTICS &operator+=(D3D12_QUERY_DATA_PIPELINE_STATISTICS &a, D3D12_QUERY_DATA_PIPELINE_STATISTICS &b) {
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

void DX12TimingWindow::Paint(const DeviceContextPaint &dc) {
	Rect		client	= GetClientRect();
	d2d::rect	dirty	= d2d.FromPixels(d2d.device ? dc.GetDirtyRect() : client);

	d2d.Init(*this, client.Size());
	if (!d2d.Occluded()) {
		d2d.BeginDraw();
		d2d.SetTransform(identity);
		d2d.device->PushAxisAlignedClip(d2d::rect(dirty), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		d2d.Clear(colour(zero));

		auto		wh		= d2d.Size();
		float		w		= wh.x, h = wh.y;
		float2x3	trans	= GetTrans();
		d2d::rect	dirty2	= dirty / trans;

		int			batch0	= max(GetBatch(dirty2.left) - 1, 0);
		int			batch1	= min(GetBatch(dirty2.right), con.batches.size() - 1);

		{// batch bands
			d2d::SolidBrush	grey(d2d, colour(0.2f,0.2f,0.2f,1));
			for (int i = batch0 & ~1; i < batch1; i += 2)
				d2d.Fill(d2d::rect(TimeToClient(GetBatchTime(i + 0)), 0, TimeToClient(GetBatchTime(i + 1)), h), grey);
		}

		{// batch bars
			d2d::SolidBrush	cols[2] = {
				{d2d, colour(1,0,0)},
				{d2d, colour(0,1,0)}
			};

			float	xs	= trans.xx;
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
			float	s	= trans.xx * rfreq;
			float	o	= trans.z.x;
			for (int i = batch0; i < batch1; i++) {
				float	t1	= ((con.batches[i].timestamp + con.batches[i+1].timestamp) / 2) * s + o;
				if (t1 > t0) {
					d2d.DrawText(d2d::rect(t1 - 100, h - 20, t1 + 100, h), str16(buffer_accum<256>() << i), font, textbrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
					t0 = t1 + 30;
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
//	ViewDX12GPU
//-----------------------------------------------------------------------------

IDFMT	ViewDX12GPU::init_format = IDFMT_LEAVE | IDFMT_FOLLOWPTR;
win::Colour ViewDX12GPU::colours[] = {
	{0,0,0},		//WT_NONE,
	{0,0,0,2},		//WT_BATCH,
	{128,0,0,2},	//WT_MARKER,
	{0,64,0,1},		//WT_CALLSTACK,
	{0,0,64},		//WT_REPEAT,
	{0,128,0},		//WT_OBJECT
};

uint8 ViewDX12GPU::cursor_indices[][3] = {
	{0,0,0},		//WT_NONE,
	{1,2,2},		//WT_BATCH,
	{0,0,0},		//WT_MARKER,
	{0,0,0},		//WT_CALLSTACK,
	{0,0,0},		//WT_REPEAT,
	{1,2,2},		//WT_OBJECT
};


LRESULT ViewDX12GPU::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			tree.Create(_GetPanePos(0), NULL, CHILD | CLIPSIBLINGS | VISIBLE | VSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, CLIENTEDGE);
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
					break;
				}

				case ID_DX12GPU_OFFSETS:
					init_format = (format ^= IDFMT_OFFSETS);
					Busy(), MakeTree();
					break;

/*				case ID_DX12GPU_SHOWMARKERS:
					init_flags = flags.flip(PM4Tree::MARKERS);
					Busy(), MakeTree();
					break;
					*/
				case ID_DX12GPU_ALLOBJECTS:
					if (!SelectTab(DX12ObjectsList::ID))
						new DX12ObjectsList(Dock(DOCK_TAB), this);
					break;

				case ID_DX12GPU_ALLRESOURCES:
					if (!SelectTab(DX12ResourcesList::ID))
						new DX12ResourcesList(Dock(DOCK_TAB), this);
					break;

				case ID_DX12GPU_ALLDESCRIPTORS:
					if (!SelectTab(DX12DescriptorList::ID))
						new DX12DescriptorList(Dock(DOCK_TAB), this);
					break;

				case ID_DX12GPU_ALLSHADERS:
					if (!SelectTab(DX12ShadersList::ID))
						new DX12ShadersList(Dock(DOCK_TAB), this);
					break;

				case ID_DX12GPU_TIMER:
					if (Control c = SelectTab(DX12TimingWindow::ID)) {
						Busy	bee;
						GetStatistics();
						CastByProc<DX12TimingWindow>(c)->Refresh();
					} else {
						Busy	bee;
						GetStatistics();
						new DX12TimingWindow(Dock(DOCK_TOP, 100), *this, *this);
					}
					break;

				default: {
					static bool reentry = false;
					if (!reentry) {
						reentry = true;
						int	ret = GetPane(1)(message, wParam, lParam);
						reentry = false;
						return ret;
					}
					break;
				}
			}
			return 0;

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

					NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
					ListViewControl	lv		= nmh->hwndFrom;
					int				i		= nmlv->iItem;
					bool			new_tab	= !!(nmlv->uKeyFlags & LVKF_SHIFT);
					bool			ctrl	= !!(nmlv->uKeyFlags & LVKF_CONTROL);

					if (i >= 0) switch (wParam) {
						case DX12DescriptorList::ID: {
							DX12DescriptorList	*dl = DX12DescriptorList::Cast(lv);
							dl->LeftClick(*this, i, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
							break;
						}
						case DX12DescriptorHeapControl::ID: {
							DX12DescriptorHeapControl	hc	= (DX12DescriptorHeapControl&)lv;
							hc.LeftClick(*this, i, nmlv->iSubItem, nmlv->ptAction, nmlv->uKeyFlags);
							break;
						}
						default:
							return ReflectNotification(*this, wParam, (NMITEMACTIVATE*)nmh);
					}
					break;
				}

				case NM_RCLICK: {
					NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
					switch (wParam) {
						case DX12ResourcesList::ID: {
							NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
							ListViewControl	lv		= nmh->hwndFrom;
							NMITEMACTIVATE	nmlv2	= *nmlv;
							nmlv2.hdr.hwndFrom		= *this;
							return lv(WM_NOTIFY, wParam, &nmlv2);
						}
					}
					break;
				}

				case NM_DBLCLK:
					if (nmh->hwndFrom == tree) {
						TreeDoubleClick(tree.GetSelectedItem());
						return TRUE;	// prevent opening tree item
					}
					break;

				case LVN_COLUMNCLICK:
					switch (nmh->idFrom) {
						case DX12ObjectsList::ID:
							DX12ObjectsList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							break;
						case DX12ResourcesList::ID:
							DX12ResourcesList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							break;
						case DX12ShadersList::ID:
							DX12ShadersList::Cast(nmh->hwndFrom)->SortOnColumn(((NMLISTVIEW*)nmh)->iSubItem);
							break;
					}
					break;

				case TVN_ITEMEXPANDING: {
					NMTREEVIEW	*nmtv	= (NMTREEVIEW*)nmh;
					if (nmtv->hdr.hwndFrom == tree && nmtv->action == TVE_EXPAND && !(nmtv->itemNew.state & TVIS_EXPANDEDONCE) && !tree.GetChildItem(nmtv->itemNew.hItem))
						ExpandTree(nmtv->itemNew.hItem);
					return 0;
				}

				case LVN_GETDISPINFOW:
					return ReflectNotification(*this, wParam, (NMLVDISPINFOW*)nmh);

				case LVN_GETDISPINFOA:
					return ReflectNotification(*this, wParam, (NMLVDISPINFOA*)nmh);

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh);
					return ReflectNotification(*this, wParam, (NMLVCUSTOMDRAW*)nmh);

				case TVN_SELCHANGED:
					if (nmh->hwndFrom == tree && tree.IsVisible())
						TreeSelection(tree.GetSelectedItem());
					break;

				case TCN_SELCHANGE: {
					TabControl2(nmh->hwndFrom).ShowSelectedControl();
					break;
				}

				case TCN_DRAG:
					DragTab(*this, nmh->hwndFrom, nmh->idFrom, !!(GetKeyState(VK_SHIFT) & 0x8000));
					return 1;

				case TCN_CLOSE:
					TabControl2(nmh->hwndFrom).GetItemControl(nmh->idFrom).Destroy();
					return 1;
			}
			return 0;
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
				Menu	m = Menu(IDR_MENU_DX12GPU).GetSubMenuByPos(0);
				m.Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
			}
			break;
		}

		case WM_ISO_BATCH:
			SelectBatch(wParam);
			break;

		case WM_ISO_JOG: {
			uint32		batch	= wParam;
			uint32		addr	= batches[batch].end;
			DX12State	state;
			GetStateAt(state, addr);

			DX12Replay	replay(*this, cache);
			replay.RunTo(addr);

			if (state.IsGraphics()) {
				dx::SimulatorDXBC::Resource	res = replay->GetResource(FindDescriptor(state.targets[0]), cache);
				auto	p	= dx::GetBitmap("target", res);
				Control	c	= SelectTab('JG');
				if (c) {
					ISO::Browser2	b(p);
					c.SendMessage(WM_COMMAND, ID_EDIT, &b);
				} else {
					c		= BitmapWindow(Dock(DOCK_TAB), p, tag(p.ID()), true);
					c.id	= 'JG';
				}
			}
			break;
		}
		case WM_DESTROY:
			return 0;
		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

void ViewDX12GPU::TreeSelection(HTREEITEM hItem) {
	TreeControl::Item	i		= tree.GetItem(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_STATE);
	bool				new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);
	switch (i.Image()) {
		case WT_BATCH: {
			uint32		addr	= i.Param();
			DX12State	state;
			GetStateAt(state, addr);
#if 0
			DX12Replay	replay(*this, cache);
			replay.RunTo(addr);
			AddTab(*new DX12BatchWindow(GetChildWindowPos(), str(tree.GetItemText(hItem)), this, state, &replay), new_tab);
#else
			new DX12BatchWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, DX12BatchWindow::ID), tree.GetItemText(hItem), this, state, 0);
#endif
			break;
		}

		case WT_CALLSTACK: {
			Busy	bee;
			uint64	pc		= i.Image2() ? *(uint64*)i.Param() : (uint64)i.Param();
			auto	frame	= stack_dumper.GetFrame(pc);
			if (frame.file && exists(frame.file)) {
				EditControl	c = MakeSourceWindow(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'SC'), frame.file, HLSLcolourerRE(), malloc_block::unterminated(lvalue(FileInput(frame.file))), 0, 0, EditControl::READONLY);
				c.id	= 'SC';
				ShowSourceLine(c, frame.line);
			}
			break;
		}

		case WT_OBJECT: {
			Busy(), MakeObjectView(Dock(new_tab ? DOCK_TAB : DOCK_TABID, 'SC'), this, i.Param());
			break;
		}
	}
}

void ViewDX12GPU::TreeDoubleClick(HTREEITEM hItem) {
}

field_info GraphicsCommandList_commands[] = {
	//ID3D12GraphicsCommandList10
	{"Close",								ff()},
	{"Reset",								ff(fp<ID3D12CommandAllocator*>("pAllocator"),fp<ID3D12PipelineState*>("pInitialState"))},
	{"ClearState",							ff(fp<ID3D12PipelineState*>("pPipelineState"))},
	{"DrawInstanced",						ff(fp<UINT>("draw.vertex_count"),fp<UINT>("draw.instance_count"),fp<UINT>("draw.vertex_offset"),fp<UINT>("draw.instance_offset"))},
	{"DrawIndexedInstanced",				ff(fp<UINT>("IndexCountPerInstance"),fp<UINT>("draw.instance_count"),fp<UINT>("draw.index_offset"),fp<INT>("BaseVertexLocation"),fp<UINT>("draw.instance_offset"))},
	{"Dispatch",							ff(fp<UINT>("compute.dim_x"),fp<UINT>("compute.dim_y"),fp<UINT>("compute.dim_z"))},
	{"CopyBufferRegion",					ff(fp<ID3D12Resource*>("pDstBuffer"),fp<UINT64>("DstOffset"),fp<ID3D12Resource*>("pSrcBuffer"),fp<UINT64>("SrcOffset"),fp<UINT64>("NumBytes"))},
	{"CopyTextureRegion",					ff(fp<const D3D12_TEXTURE_COPY_LOCATION*>("pDst"),fp<UINT>("DstX"),fp<UINT>("DstY"),fp<UINT>("DstZ"),fp<const D3D12_TEXTURE_COPY_LOCATION*>("pSrc"),fp<const D3D12_BOX*>("pSrcBox"))},
	{"CopyResource",						ff(fp<ID3D12Resource*>("pDstResource"),fp<ID3D12Resource*>("pSrcResource"))},
	{"CopyTiles",							ff(fp<ID3D12Resource*>("pTiledResource"),fp<const D3D12_TILED_RESOURCE_COORDINATE*>("pTileRegionStartCoordinate"),fp<const D3D12_TILE_REGION_SIZE*>("pTileRegionSize"),fp<ID3D12Resource*>("pBuffer"),fp<UINT64>("BufferStartOffsetInBytes"),fp<D3D12_TILE_COPY_FLAGS>("Flags"))},
	{"ResolveSubresource",					ff(fp<ID3D12Resource*>("pDstResource"),fp<UINT>("DstSubresource"),fp<ID3D12Resource*>("pSrcResource"),fp<UINT>("SrcSubresource"),fp<DXGI_FORMAT>("Format"))},
	{"IASetPrimitiveTopology",				ff(fp<D3D12_PRIMITIVE_TOPOLOGY>("PrimitiveTopology"))},
	{"RSSetViewports",						ff(fp<UINT>("NumViewports"),fp<counted<const D3D12_VIEWPORT,0>>("pViewports"))},
	{"RSSetScissorRects",					ff(fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 0>>("pRects"))},
	{"OMSetBlendFactor",					ff(fp<const FLOAT>("BlendFactor[4]"))},
	{"OMSetStencilRef",						ff(fp<UINT>("StencilRef"))},
	{"SetPipelineState",					ff(fp<ID3D12PipelineState*>("pPipelineState"))},
	{"ResourceBarrier",						ff(fp<UINT>("NumBarriers"),fp<counted<const D3D12_RESOURCE_BARRIER, 0>>("pBarriers"))},
	{"ExecuteBundle",						ff(fp<ID3D12GraphicsCommandList*>("pCommandList"))},
	{"SetDescriptorHeaps",					ff(fp<UINT>("NumDescriptorHeaps"),fp<counted<ID3D12DescriptorHeap* const, 0>>("pp"))},
	{"SetComputeRootSignature",				ff(fp<ID3D12RootSignature*>("pRootSignature"))},
	{"SetGraphicsRootSignature",			ff(fp<ID3D12RootSignature*>("pRootSignature"))},
	{"SetComputeRootDescriptorTable",		ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_DESCRIPTOR_HANDLE>("BaseDescriptor"))},
	{"SetGraphicsRootDescriptorTable",		ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_DESCRIPTOR_HANDLE>("BaseDescriptor"))},
	{"SetComputeRoot32BitConstant",			ff(fp<UINT>("RootParameterIndex"),fp<UINT>("SrcData"),fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetGraphicsRoot32BitConstant",		ff(fp<UINT>("RootParameterIndex"),fp<UINT>("SrcData"),fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetComputeRoot32BitConstants",		ff(fp<UINT>("RootParameterIndex"),fp<UINT>("Num32BitValuesToSet"),fp<counted<const uint32, 1>>("pSrcData"), fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetGraphicsRoot32BitConstants",		ff(fp<UINT>("RootParameterIndex"),fp<UINT>("Num32BitValuesToSet"),fp<counted<const uint32, 1>>("pSrcData"), fp<UINT>("DestOffsetIn32BitValues"))},
	{"SetComputeRootConstantBufferView",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS>("BufferLocation"))},
	{"SetGraphicsRootConstantBufferView",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS>("BufferLocation"))},
	{"SetComputeRootShaderResourceView",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS>("BufferLocation"))},
	{"SetGraphicsRootShaderResourceView",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS>("BufferLocation"))},
	{"SetComputeRootUnorderedAccessView",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS>("BufferLocation"))},
	{"SetGraphicsRootUnorderedAccessView",	ff(fp<UINT>("RootParameterIndex"),fp<D3D12_GPU_VIRTUAL_ADDRESS>("BufferLocation"))},
	{"IASetIndexBuffer",					ff(fp<const D3D12_INDEX_BUFFER_VIEW*>("pView"))},
	{"IASetVertexBuffers",					ff(fp<UINT>("StartSlot"),fp<UINT>("NumViews"),fp<counted<const D3D12_VERTEX_BUFFER_VIEW,1>>("pViews"))},
	{"SOSetTargets",						ff(fp<UINT>("StartSlot"),fp<UINT>("NumViews"),fp<counted<const D3D12_STREAM_OUTPUT_BUFFER_VIEW, 1>>("pViews"))},
	{"OMSetRenderTargets",					ff(fp<UINT>("NumRenderTargetDescriptors"),fp<counted<const D3D12_CPU_DESCRIPTOR_HANDLE, 0>>("pRenderTargetDescriptors"), fp<BOOL>("RTsSingleHandleToDescriptorRange"), fp<const D3D12_CPU_DESCRIPTOR_HANDLE*>("pDepthStencilDescriptor"))},
	{"ClearDepthStencilView",				ff(fp<D3D12_CPU_DESCRIPTOR_HANDLE>("DepthStencilView"),fp<D3D12_CLEAR_FLAGS>("ClearFlags"),fp<FLOAT>("Depth"),fp<UINT8>("Stencil"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 4>>("pRects"))},
	{"ClearRenderTargetView",				ff(fp<D3D12_CPU_DESCRIPTOR_HANDLE>("RenderTargetView"),fp<const FLOAT[4]>("ColorRGBA[4]"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 2>>("pRects"))},
	{"ClearUnorderedAccessViewUint",		ff(fp<D3D12_GPU_DESCRIPTOR_HANDLE>("ViewGPUHandleInCurrentHeap"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("ViewCPUHandle"),fp<ID3D12Resource*>("pResource"),fp<const UINT>("Values[4]"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 4>>("pRects"))},
	{"ClearUnorderedAccessViewFloat",		ff(fp<D3D12_GPU_DESCRIPTOR_HANDLE>("ViewGPUHandleInCurrentHeap"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("ViewCPUHandle"),fp<ID3D12Resource*>("pResource"),fp<const FLOAT>("Values[4]"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 4>>("pRects"))},
	{"DiscardResource",						ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_DISCARD_REGION*>("pRegion"))},
	{"BeginQuery",							ff(fp<ID3D12QueryHeap*>("pQueryHeap"),fp<D3D12_QUERY_TYPE>("Type"),fp<UINT>("Index"))},
	{"EndQuery",							ff(fp<ID3D12QueryHeap*>("pQueryHeap"),fp<D3D12_QUERY_TYPE>("Type"),fp<UINT>("Index"))},
	{"ResolveQueryData",					ff(fp<ID3D12QueryHeap*>("pQueryHeap"),fp<D3D12_QUERY_TYPE>("Type"),fp<UINT>("StartIndex"),fp<UINT>("NumQueries"),fp<ID3D12Resource*>("pDestinationBuffer"),fp<UINT64>("AlignedDestinationBufferOffset"))},
	{"SetPredication",						ff(fp<ID3D12Resource*>("pBuffer"),fp<UINT64>("AlignedBufferOffset"),fp<D3D12_PREDICATION_OP>("Operation"))},
	{"SetMarker",							ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"BeginEvent",							ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"EndEvent",							ff()},
	{"ExecuteIndirect",						ff(fp<ID3D12CommandSignature*>("pCommandSignature"),fp<UINT>("MaxCommandCount"),fp<ID3D12Resource*>("pArgumentBuffer"),fp<UINT64>("ArgumentBufferOffset"),fp<ID3D12Resource*>("pCountBuffer"),fp<UINT64>("CountBufferOffset"))},
	//ID3D12GraphicsCommandList1
	{"AtomicCopyBufferUINT",				ff(fp<ID3D12Resource*>("pDstBuffer"),fp<UINT64>("DstOffset"),fp<ID3D12Resource*>("pSrcBuffer"),fp<UINT64>("SrcOffset"),fp<UINT>("Dependencies"),fp<counted<ID3D12Resource *const, 4>>("ppDependentResources"),fp<const D3D12_SUBRESOURCE_RANGE_UINT64*>("pDependentSubresourceRanges"))},
	{"AtomicCopyBufferUINT64",				ff(fp<ID3D12Resource*>("pDstBuffer"),fp<UINT64>("DstOffset"),fp<ID3D12Resource*>("pSrcBuffer"),fp<UINT64>("SrcOffset"),fp<UINT>("Dependencies"),fp<counted<ID3D12Resource *const, 4>>("ppDependentResources"),fp<const D3D12_SUBRESOURCE_RANGE_UINT64*>("pDependentSubresourceRanges"))},
	{"OMSetDepthBounds",					ff(fp<FLOAT>("Min"),fp<FLOAT>("Max"))},
	{"SetSamplePositions",					ff(fp<UINT>("NumSamplesPerPixel"),fp<UINT>("NumPixels"),fp<D3D12_SAMPLE_POSITION*>("pSamplePositions"))},
	{"ResolveSubresourceRegion",			ff(fp<ID3D12Resource*>("pDstResource"),fp<UINT>("DstSubresource"),fp<UINT>("DstX"),fp<UINT>("DstY"),fp<ID3D12Resource*>("pSrcResource"),fp<UINT>("SrcSubresource"),fp<D3D12_RECT*>("pSrcRect"),fp<DXGI_FORMAT>("Format"),fp<D3D12_RESOLVE_MODE>("ResolveMode"))},
	{"SetViewInstanceMask",					ff(fp<UINT>("Mask"))},
	//ID3D12GraphicsCommandList2
	{"WriteBufferImmediate",				ff(fp<UINT>("Count"),fp<counted<const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER, 0>>("pParams"),fp<counted<const D3D12_WRITEBUFFERIMMEDIATE_MODE, 0>>("pModes"))},
	//ID3D12GraphicsCommandList3
	{"SetProtectedResourceSession",			ff(fp<ID3D12ProtectedResourceSession*>("pProtectedResourceSession"))},
	//ID3D12GraphicsCommandList4
	{"BeginRenderPass",						ff(fp<UINT>("NumRenderTargets"),fp<counted<const D3D12_RENDER_PASS_RENDER_TARGET_DESC, 0>>("pRenderTargets"),fp<const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC*>("pDepthStencil"),fp<D3D12_RENDER_PASS_FLAGS>("Flags"))},
	{"EndRenderPass",						ff()},
	{"InitializeMetaCommand",				ff(fp<ID3D12MetaCommand*>("pMetaCommand"),fp<const void*>("pInitializationParametersData"),fp<SIZE_T>("InitializationParametersDataSizeInBytes"))},
	{"ExecuteMetaCommand",					ff(fp<ID3D12MetaCommand*>("pMetaCommand"),fp<const void*>("pExecutionParametersData"),fp<SIZE_T>("ExecutionParametersDataSizeInBytes"))},
	{"BuildRaytracingAccelerationStructure",ff(fp<const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC*>("pDesc"),fp<UINT>("NumPostbuildInfoDescs"),fp<counted<const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC, 1>>("pPostbuildInfoDescs"))},
	{"EmitRaytracingAccelerationStructurePostbuildInfo",ff(fp<const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC*>("pDesc"),fp<UINT>("NumSourceAccelerationStructures"),fp<counted<const D3D12_GPU_VIRTUAL_ADDRESS, 1>>("pSourceAccelerationStructureData"))},
	{"CopyRaytracingAccelerationStructure",	ff(fp<D3D12_GPU_VIRTUAL_ADDRESS>("DestAccelerationStructureData"),fp<D3D12_GPU_VIRTUAL_ADDRESS>("SourceAccelerationStructureData"),fp<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE>("Mode"))},
	{"SetPipelineState1",					ff(fp<ID3D12StateObject*>("pStateObject"))},
	{"DispatchRays",						ff(fp<const D3D12_DISPATCH_RAYS_DESC*>("pDesc"))},
	//ID3D12GraphicsCommandList5
	{"RSSetShadingRate",					ff(fp<D3D12_SHADING_RATE>("baseShadingRate"), fp<const D3D12_SHADING_RATE_COMBINER*>("combiners"))},
	{"RSSetShadingRateImage",				ff(fp<ID3D12Resource*>("shadingRateImage"))},
	//ID3D12GraphicsCommandList6
	{"DispatchMesh",						ff(fp<UINT>("ThreadGroupCountX"), fp<UINT>(" ThreadGroupCountY"), fp<UINT>(" ThreadGroupCountZ"))},
};

field_info Device_commands[] = {
	//ID3D12Device
	{"CreateCommandQueue",					ff(fp<const D3D12_COMMAND_QUEUE_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateCommandAllocator",				ff(fp<D3D12_COMMAND_LIST_TYPE>("type"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateGraphicsPipelineState",			ff(fp<const D3D12_GRAPHICS_PIPELINE_STATE_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateComputePipelineState",			ff(fp<const D3D12_COMPUTE_PIPELINE_STATE_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateCommandList",					ff(fp<UINT>("nodeMask"),fp<D3D12_COMMAND_LIST_TYPE>("type"),fp<ID3D12CommandAllocator*>("pCommandAllocator"),fp<ID3D12PipelineState*>("pInitialState"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CheckFeatureSupport",					ff(fp<D3D12_FEATURE>("Feature"),fp<void*>("pFeatureSupportData"),fp<UINT>("FeatureSupportDataSize"))},
	{"CreateDescriptorHeap",				ff(fp<const D3D12_DESCRIPTOR_HEAP_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateRootSignature",					ff(fp<UINT>("nodeMask"),fp<counted<const uint8,2>>("pBlobWithRootSignature"),fp<SIZE_T>("blobLengthInBytes"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateConstantBufferView",			ff(fp<const D3D12_CONSTANT_BUFFER_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateShaderResourceView",			ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_SHADER_RESOURCE_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateUnorderedAccessView",			ff(fp<ID3D12Resource*>("pResource"),fp<ID3D12Resource*>("pCounterResource"),fp<const D3D12_UNORDERED_ACCESS_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateRenderTargetView",				ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_RENDER_TARGET_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateDepthStencilView",				ff(fp<ID3D12Resource*>("pResource"),fp<const D3D12_DEPTH_STENCIL_VIEW_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CreateSampler",						ff(fp<const D3D12_SAMPLER_DESC*>("desc"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest"))},
	{"CopyDescriptors",						ff(fp<UINT>("dest_num"),fp<counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0>>("dest_starts"),fp<counted<const UINT,0>>("dest_sizes"),fp<UINT>("srce_num"),fp<counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3>>("srce_starts"),fp<counted<const UINT,3>>("srce_sizes"),fp<D3D12_DESCRIPTOR_HEAP_TYPE>("type"))},
	{"CopyDescriptorsSimple",				ff(fp<UINT>("num"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("dest_start"),fp<D3D12_CPU_DESCRIPTOR_HANDLE>("srce_start"),fp<D3D12_DESCRIPTOR_HEAP_TYPE>("type"))},
	{"CreateCommittedResource",				ff(fp<const D3D12_HEAP_PROPERTIES*>("pHeapProperties"),fp<D3D12_HEAP_FLAGS>("HeapFlags"),fp<const D3D12_RESOURCE_DESC*>("desc"),fp<D3D12_RESOURCE_STATES>("InitialResourceState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riidResource"),fp<void**>("pp"))},
	{"CreateHeap",							ff(fp<const D3D12_HEAP_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreatePlacedResource",				ff(fp<ID3D12Heap*>("pHeap"),fp<UINT64>("HeapOffset"),fp<const D3D12_RESOURCE_DESC*>("desc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateReservedResource",				ff(fp<const D3D12_RESOURCE_DESC*>("desc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateSharedHandle",					ff(fp<ID3D12DeviceChild*>("pObject"),fp<const SECURITY_ATTRIBUTES*>("pAttributes"),fp<DWORD>("Access"),fp<LPCWSTR>("Name"),fp<HANDLE*>("pHandle"))},
	{"OpenSharedHandle",					ff(fp<HANDLE>("NTHandle"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"OpenSharedHandleByName",				ff(fp<LPCWSTR>("Name"),fp<DWORD>("Access"),fp<HANDLE*>("pNTHandle"))},
	{"MakeResident",						ff(fp<UINT>("NumObjects"),fp<counted<ID3D12Pageable* const, 0>>("pp"))},
	{"Evict",								ff(fp<UINT>("NumObjects"),fp<counted<ID3D12Pageable* const, 0>>("pp"))},
	{"CreateFence",							ff(fp<UINT64>("InitialValue"),fp<D3D12_FENCE_FLAGS>("Flags"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"CreateQueryHeap",						ff(fp<const D3D12_QUERY_HEAP_DESC*>("desc"),fp<REFIID>("riid"),fp<void**>("pp"))},
	{"SetStablePowerState",					ff(fp<BOOL>("Enable"))},
	{"CreateCommandSignature",				ff(fp<const D3D12_COMMAND_SIGNATURE_DESC*>("desc"),fp<ID3D12RootSignature*>("pRootSignature"),fp<REFIID>("riid"),fp<void**>("pp"))},
	//ID3D12Device1
#if 1
	{"CreatePipelineLibrary",				ff(fp<const void*>("pLibraryBlob"),fp<SIZE_T>("BlobLength"),fp<REFIID>("riid"),fp<void **>("ppPipelineLibrary"))},
	{"SetEventOnMultipleFenceCompletion",	ff(fp<ID3D12Fence* const*>("ppFences"),fp<const UINT64*>("pFenceValues"),fp<UINT>("NumFences"),fp<D3D12_MULTIPLE_FENCE_WAIT_FLAGS>("Flags"), fp<HANDLE>("hEvent"))},
	{"SetResidencyPriority",				ff(fp<UINT>("NumObjects"), fp<counted<const ID3D12Pageable*,0>>("ppObjects"),fp<counted<uint32, 0>>("pPriorities"))},
	{"SetResidencyPriority",				ff(fp<UINT>("NumObjects"), fp<counted<const ID3D12Pageable*,0>>("ppObjects"),fp<counted<const D3D12_RESIDENCY_PRIORITY, 0>>("pPriorities"))},
	//ID3D12Device2
	{"CreatePipelineState",					ff(fp<const D3D12_PIPELINE_STATE_STREAM_DESC*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppPipelineState"))},
	//ID3D12Device3
	{"OpenExistingHeapFromAddress",			ff(fp<const void*>("pAddress"),fp<REFIID>("riid"),fp<void **>("ppvHeap"))},
	{"OpenExistingHeapFromFileMapping",		ff(fp<HANDLE>("hFileMapping"),fp<REFIID>("riid"),fp<void **>("ppvHeap"))},
	{"EnqueueMakeResident",					ff(fp<D3D12_RESIDENCY_FLAGS>("Flags"),fp<UINT>("NumObjects"), fp<ID3D12Pageable* const*>("ppObjects"),fp<ID3D12Fence*>("pFenceToSignal"), fp<UINT64>("FenceValueToSignal"))},
	//ID3D12Device4
	{"CreateCommandList1",					ff(fp<UINT>("nodeMask"),fp<D3D12_COMMAND_LIST_TYPE>("type"),fp<D3D12_COMMAND_LIST_FLAGS>("flags"),fp<REFIID>("riid"),fp<void **>("ppCommandList"))},
	{"CreateProtectedResourceSession",		ff(fp<const D3D12_PROTECTED_RESOURCE_SESSION_DESC*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppSession"))},
	{"CreateCommittedResource1",			ff(fp<const D3D12_HEAP_PROPERTIES*>("pHeapProperties"),fp<D3D12_HEAP_FLAGS>("HeapFlags"),fp<const D3D12_RESOURCE_DESC*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialResourceState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riidResource"),fp<void **>("ppvResource"))},
	{"CreateHeap1",							ff(fp<const D3D12_HEAP_DESC*>("pDesc"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riid"),fp<void **>("ppvHeap"))},
	{"CreateReservedResource1",				ff(fp<const D3D12_RESOURCE_DESC*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riid"),fp<void **>("ppvResource"))},
	//ID3D12Device5
	{"CreateLifetimeTracker",				ff(fp<ID3D12LifetimeOwner*>("pOwner"),fp<REFIID>("riid"),fp<void **>("ppvTracker"))},
	{"CreateMetaCommand",					ff(fp<REFGUID>("CommandId"),fp<UINT>("NodeMask"),fp<const void*>("pCreationParametersData"),fp<SIZE_T>("CreationParametersDataSizeInBytes"),fp<REFIID>("riid"),fp<void **>("ppMetaCommand"))},
	{"CreateStateObject",					ff(fp<const D3D12_STATE_OBJECT_DESC*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppStateObject"))},
	//ID3D12Device6
	{"SetBackgroundProcessingMode",			ff(fp<D3D12_BACKGROUND_PROCESSING_MODE>("Mode"),fp<D3D12_MEASUREMENTS_ACTION>("MeasurementsAction"),fp<HANDLE>("hEventToSignalUponCompletion"),fp<BOOL*>("pbFurtherMeasurementsDesired"))},
	//ID3D12Device7
	{"AddToStateObject",					ff(fp<const D3D12_STATE_OBJECT_DESC*>("pAddition"),fp<ID3D12StateObject*>("pStateObjectToGrowFrom"),fp<REFIID>("riid"),fp<void **>("ppNewStateObject"))},
	{"CreateProtectedResourceSession1",		ff(fp<const D3D12_PROTECTED_RESOURCE_SESSION_DESC1*>("pDesc"),fp<REFIID>("riid"),fp<void **>("ppSession"))},
	//ID3D12Device8
	{"CreateCommittedResource2",			ff(fp<const D3D12_HEAP_PROPERTIES*>("pHeapProperties"),fp<D3D12_HEAP_FLAGS>("HeapFlags"),fp<const D3D12_RESOURCE_DESC1*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialResourceState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<ID3D12ProtectedResourceSession*>("pProtectedSession"),fp<REFIID>("riidResource"),fp<void **>("ppvResource"))},
	{"CreatePlacedResource1",				ff(fp<ID3D12Heap*>("pHeap"),fp<UINT64>("HeapOffset"),fp<const D3D12_RESOURCE_DESC1*>("pDesc"),fp<D3D12_RESOURCE_STATES>("InitialState"),fp<const D3D12_CLEAR_VALUE*>("pOptimizedClearValue"),fp<REFIID>("riid"),fp<void **>("ppvResource"))},
	{"CreateSamplerFeedbackUnorderedAccessView",ff(fp<ID3D12Resource*>("pTargetedResource"),fp<ID3D12Resource*>("pFeedbackResource"), fp<D3D12_CPU_DESCRIPTOR_HANDLE>("DestDescriptor"))},

	//ID3D12CommandQueue
	{"UpdateTileMappings",					ff(fp<ID3D12Resource*>("pResource"),fp<UINT>("NumResourceRegions"),fp<counted<const D3D12_TILED_RESOURCE_COORDINATE, 1>>("pResourceRegionStartCoordinates"), fp<const D3D12_TILE_REGION_SIZE*>("pResourceRegionSizes"), fp<ID3D12Heap*>("pHeap"), fp<UINT>("NumRanges"), fp<counted<const D3D12_TILE_RANGE_FLAGS, 5>>("pRangeFlags"),fp<counted<const UINT, 5>>("pHeapRangeStartOffsets"), fp<counted<const UINT,5>>("pRangeTileCounts"),fp<D3D12_TILE_MAPPING_FLAGS>("Flags"))},
	{"CopyTileMappings",					ff(fp<ID3D12Resource*>("pDstResource"),fp<const D3D12_TILED_RESOURCE_COORDINATE*>("pDstRegionStartCoordinate"),fp<ID3D12Resource*>("pSrcResource"),fp<const D3D12_TILED_RESOURCE_COORDINATE*>("pSrcRegionStartCoordinate"),fp<const D3D12_TILE_REGION_SIZE*>("pRegionSize"),fp<D3D12_TILE_MAPPING_FLAGS>("Flags"))},
	{"ExecuteCommandLists",					ff(fp<UINT>("NumCommandLists"),fp<counted<ID3D12CommandList* const, 0>>("pp"))},
	{"SetMarker",							ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"BeginEvent",							ff(fp<UINT>("Metadata"),fp<const void*>("pData"),fp<UINT>("Size"))},
	{"EndEvent",							ff()},
	{"Signal",								ff(fp<ID3D12Fence*>("pFence"),fp<UINT64>("Value"))},
	{"Wait",								ff(fp<ID3D12Fence*>("pFence"),fp<UINT64>("Value"))},
	//ID3D12CommandAllocator
	{"Reset",								ff()},
	//ID3D12Fence
	{"SetEventOnCompletion",				ff(fp<UINT64>("Value"),fp<HANDLE>("hEvent"))},
	{"Signal",								ff(fp<UINT64>("Value"))},
	//Event
	{"WaitForSingleObjectEx",				ff(fp<DWORD>("dwMilliseconds"),fp<BOOL>("bAlertable"))},
	//ID3D12GraphicsCommandList
	{"GraphicsCommandListClose",			ff()},
	{"GraphicsCommandListReset",			ff(fp<ID3D12CommandAllocator*>("pAllocator"),fp<ID3D12PipelineState*>("pInitialState"))},
#endif
};

int	CountRepeats(const COMRecording::header *&P, const COMRecording::header *e) {
	const COMRecording::header *p = P;
	if (p->id == 0xfffe)
		p = p->next();

	int	count	= 0;
	int	id		= p->id;

	do {
		++count;
		P = p = p->next();
		if (p->id == 0xfffe)
			p = p->next();
	} while (p < e && p->id == id);

	return count;
}

static void PutRepeat(RegisterTree &rt, HTREEITEM h, field_info *nf, int id, uint32 addr, int count) {
	buffer_accum<256>	ba;
	if (rt.format & IDFMT_OFFSETS)
		ba.format("%08x: ", addr);
	TreeControl::Item(ba << nf[id].name << " x " << count).Image(ViewDX12GPU::WT_REPEAT).Param(addr).Children(1).Insert(rt.tree, h);
}

static void PutCallstack(RegisterTree &rt, HTREEITEM h, const void **callstack) {
	TreeControl::Item("callstack").Image(ViewDX12GPU::WT_CALLSTACK).Param(callstack).Image2(1).Children(1).Insert(rt.tree, h);
}

static HTREEITEM PutCommand(RegisterTree &rt, HTREEITEM h, field_info *nf, const COMRecording::header *p, uint32 addr, DX12Assets &assets) {
	buffer_accum<256>	ba;

	if (rt.format & IDFMT_OFFSETS)
		ba.format("%08x: ", addr);

	return rt.AddFields(rt.AddText(h, ba << nf[p->id].name, addr), nf[p->id].fields, (uint32*)p->data());
}

static void PutCommands(RegisterTree &rt, HTREEITEM h, field_info *nf, const_memory_block m, intptr_t offset, DX12Assets &assets, bool check_repeats) {
	for (const COMRecording::header *p = m, *e = (const COMRecording::header*)m.end(); p < e; ) {
		const void**	callstack	= 0;
		uint32			addr		= uint32((intptr_t)p + offset);

		if (p->id == 0xfffe) {
			callstack = (const void**)p->data();
			p = p->next();
		}

		auto	*p0	= p;
		if (check_repeats) {
			int		count	= CountRepeats(p, e);
			if (count > 2) {
				PutRepeat(rt, h, nf, p0->id, addr, count);
				continue;
			}
		}

		HTREEITEM	h2 = PutCommand(rt, h, nf, p0, addr, assets);
		if (callstack)
			PutCallstack(rt, h2, callstack);

		p = p0->next();
	}
}

bool HasBatches(const interval<uint32> &r, DX12Assets &assets) {
	if (!r.empty()) {
		const DX12Assets::BatchRecord	*b	= assets.GetBatch(r.begin());
		return b->end <= r.end() && b->op;
	}
	return false;
}

range<const DX12Assets::BatchRecord*> UsedBatches(const interval<uint32> &r, DX12Assets &assets) {
	if (!r.empty()) {
		const DX12Assets::BatchRecord	*b	= assets.GetBatch(r.begin());
		if (b->end <= r.end() && b->op)
			return make_range(b, assets.GetBatch(r.end()));
	}
	return empty;
}

string_accum& BatchRange(string_accum &sa, const interval<uint32> &r, DX12Assets &assets) {
	auto	used = UsedBatches(r, assets);
	if (used.empty())
		return sa;
	if (used.size() > 1)
		return sa << " (" << assets.batches.index_of(used.begin()) << " to " << assets.batches.index_of(used.end()) - 1 << ")";
	return sa << " (" << assets.batches.index_of(used.begin()) << ")";
}

struct MarkerStack {
	HTREEITEM						h;
	const DX12Assets::MarkerRecord	*m;
};

bool SubTree(RegisterTree &rt, HTREEITEM h, const_memory_block mem, intptr_t offset, DX12Assets &assets) {
	uint32	start	= intptr_t(mem.p) + offset;
	uint32	end		= start + mem.size32();
	uint32	boffset	= start;

	const DX12Assets::BatchRecord	*b	= assets.GetBatch(start);
	const DX12Assets::MarkerRecord	*m	= assets.GetMarker(start);
	dynamic_array<MarkerStack>		msp;

	bool	got_batch	= b->end <= end && b->op;

	while (b->end <= end && b->op) {
		while (msp.size() && b->end > msp.back().m->end())
			h	= msp.pop_back_value().h;

		while (m != assets.markers.end() && b->end > m->begin()) {
			MarkerStack	&ms = msp.push_back();
			ms.h	= h;
			ms.m	= m;
			h		= TreeControl::Item(BatchRange(lvalue(buffer_accum<256>(m->str)), *m, assets)).Param(b->end - 1).Image(ViewDX12GPU::WT_MARKER).Expand().Insert(rt.tree, h);
			++m;
		}

		buffer_accum<256>	ba;
		ba << "Batch " << assets.batches.index_of(b) << " : ";

		switch (b->op) {
			case RecCommandList::tag_DrawInstanced:
				ba.format("draw %s x %u", get_field_name(b->getPrim()), b->draw.vertex_count);
				break;

			case RecCommandList::tag_DrawIndexedInstanced:
				ba.format("draw %s x %u, indexed", get_field_name(b->getPrim()), b->draw.vertex_count);
				break;

			case RecCommandList::tag_Dispatch:
				ba.format("dispatch %u x %u x %u", b->compute.dim_x, b->compute.dim_y, b->compute.dim_z);
				break;

			case RecCommandList::tag_ExecuteIndirect:
				ba.format("indirect %u", b->indirect.command_count);
				break;
		}

		HTREEITEM		h3	= TreeControl::Item(ba).Image(ViewDX12GPU::WT_BATCH).Param(b->end).Insert(rt.tree, h);
		PutCommands(rt, h3, GraphicsCommandList_commands, mem.slice(boffset - start, b->end - boffset), offset, assets, true);
		boffset = b->end;
		++b;
	}

	if (boffset != end) {
		PutCommands(rt, got_batch? TreeControl::Item("Orphan").Bold().Insert(rt.tree, h) : h, GraphicsCommandList_commands, mem.slice(int(boffset - end)), offset, assets, true);
	}

	while (msp.size() && boffset > msp.back().m->end())
		h =  msp.pop_back_value().h;

	return got_batch;
}

void ViewDX12GPU::MakeTree() {
	tree.Show(SW_HIDE);
	tree.DeleteItem();

	RegisterTree	rt(tree, this, format);
	HTREEITEM		h		= TVI_ROOT, h1;
#if 0
	h1		= TreeControl::Item("Unsubmitted?").Bold().Insert(tree, h);
	for (auto &obj : objects) {
		if (obj.type == RecObject::GraphicsCommandList) {
			const RecCommandList	*rec		= obj.info;
			HTREEITEM	h3	= TreeControl::Item(obj.name).Bold().Expand(true).Param(obj.index).Insert(tree, h1);
			SubTree(rt, h3, rec->buffer, obj.index - intptr_t(rec->buffer.p), *this);
		}
	}
#endif
	h1		= TreeControl::Item("Device").Bold().Insert(tree, h);

	const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end();
	while (p < e) {
		const void**		callstack	= 0;
		uint32				addr		= uint32((char*)p - (char*)device_object->info.p);
		buffer_accum<256>	ba;
		HTREEITEM			h2;

		while (p->id == 0xfffe) {
			callstack	= (const void**)p->data();
			p = p->next();
		}

		if (p->id == 0xffff) {
			uint64		obj		= *(uint64*)p->data();
			p = p->next();

			while (p->id == 0xfffe) {
				callstack	= (const void**)p->data();
				p = p->next();
			}

			if (rt.format & IDFMT_OFFSETS)
				ba.format("%08x: ", addr);

			if (auto *rec = FindObject(obj))
				ba << rec->name;
			else
				ba << "0x" << hex(obj);

			h2	= TreeControl::Item(ba << "::" << Device_commands[p->id].name).Bold().Param(addr).Insert(tree, h1);

			if (p->id == RecDevice::tag_CommandQueueExecuteCommandLists) {
				COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
					bool	has_batches = false;
					for (int i = 0; !has_batches && i < NumCommandLists; i++)
						has_batches = HasBatches(ResolveRange(pp[i], *this), *this);

					for (int i = 0; i < NumCommandLists; i++) {
						const CommandRange	&r	= pp[i];
						if (ObjectRecord *obj = FindObject(uint64(&*r))) {
							const RecCommandList	*rec = obj->info;
							buffer_accum<256>	ba;
							HTREEITEM	h3	= TreeControl::Item(BatchRange(ba << obj->name, ResolveRange(r, *this), *this)).Bold().Expand(has_batches).Param(addr).Insert(tree, has_batches ? h : h2);
							if (r.extent())
								SubTree(rt, h3, rec->buffer.slice(r.begin(), r.extent()), obj->index - intptr_t(rec->buffer.p), *this);
						}
					}
					//if (has_batches)
						h1	= TreeControl::Item("Device").Bold().Insert(tree, h);
				});
			} else {
				rt.AddFields(h2, Device_commands[p->id].fields, (uint32*)p->data());
			}

			p = p->next();

		} else {

			auto	*p0		= p;
			int		count	= CountRepeats(p, e);
			if (count > 2) {
				PutRepeat(rt, h1, Device_commands, p0->id, addr, count);
				callstack = 0;

			} else {
				h2	= PutCommand(rt, h1, Device_commands, p0, addr, *this);
				p	= p0->next();
			}
		}

		if (callstack)
			PutCallstack(rt, h2, callstack);
	}


	tree.Show();
	tree.Invalidate();
}

void ViewDX12GPU::ExpandTree(HTREEITEM h) {
	RegisterTree		rt(tree, this, format);
	buffer_accum<1024>	ba;

	TreeControl::Item	item = tree.GetItem(h, TVIF_IMAGE|TVIF_PARAM);

	switch (item.Image()) {
		case WT_CALLSTACK: {
			const uint64*	callstack = item.Param();
			for (int i = 0; i < 32 && callstack[i]; i++)
				rt.Add(h, ba.reset() << stack_dumper.GetFrame(callstack[i]), WT_CALLSTACK, callstack[i]);
			break;
		}

		case WT_REPEAT: {
			uint32		offset = item.Param();
			if (auto m = GetCommands(offset, *this)) {
				const COMRecording::header *p	= m;
				int		count = CountRepeats(p, m.end());

				PutCommands(rt, h, offset < device_object->info.size32() ? Device_commands : GraphicsCommandList_commands, m.slice_to(unconst(p)), offset - uintptr_t(m.begin()), *this, false);
			}
			break;
		}
	}
}

ViewDX12GPU::ViewDX12GPU(MainWindow &main, const WindowPos &pos) : SplitterWindow(SWF_VERT|SWF_DOCK) {
	format			= init_format;
	context_item	= 0;

	SplitterWindow::Create(pos, "DX12 Dump", CHILD | CLIPCHILDREN);
	toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX12GPU);
	Rebind(this);
}
ViewDX12GPU::ViewDX12GPU(MainWindow &main, const WindowPos &pos, HANDLE process, const char *executable) : SplitterWindow(SWF_VERT|SWF_DOCK), DX12Connection(process, executable) {
	format			= init_format;
	context_item	= 0;

	SplitterWindow::Create(pos, "DX12 Dump", CHILD | CLIPCHILDREN);
	toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX12GPU);
	Rebind(this);
}

//-----------------------------------------------------------------------------
//	EditorDX12
//-----------------------------------------------------------------------------

class EditorDX12 : public app::EditorGPU, public app::MenuItemCallbackT<EditorDX12>, DXCapturer {
	filename	fn;
	Menu		recent;
	dynamic_array<DWORD>	pids;

	void	Grab(ViewDX12GPU *view, Progress &progress);

	void	TogglePause() {
		Call<void>(paused ? INTF_Continue : INTF_Pause);
		paused = !paused;
	}

	virtual bool Matches(const ISO::Browser &b) {
		return b.Is("DX12GPUState") || b.Is<DX12Capture>(ISO::MATCH_NOUSERRECURSE);
	}

	virtual Control Create(app::MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		ViewDX12GPU	*view = new ViewDX12GPU(main, wpos);
		view->Load(p);

		RunThread([view,
			progress = Progress(WindowPos(*view, AdjustRect(view->GetRect().Centre(500, 30), Control::OVERLAPPED | Control::CAPTION, false)), "Collecting assets", view->objects.size32())
		]() mutable {
			view->GetAssets(&progress, view->CommandTotal());
			//view->GetStatistics();
			JobQueue::Main().add([view] { view->MakeTree(); });
		});
		return *view;
	}

#ifndef ISO_EDITOR
	virtual bool	Command(MainWindow &main, ID id, MODE mode) {
		if (mode == MODE_home) {
			MultiSplitterWindow	*ms = MultiSplitterWindow::Cast(main.GetChild());
			EditControl	e(ms->AppendPane(100), "Hello from DX12", Control::VISIBLE | Control::CHILD);
			ms->SetPane(ms->NumPanes() - 1, e);
			return false;
		}

		if ((main.GetDropDown(ID_ORBISCRUDE_GRAB).GetItemStateByID(ID_ORBISCRUDE_GRAB_DX12) & MF_CHECKED) && EditorGPU::Command(main, id, mode))
			return true;

		switch (id) {
			case 0: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				recent			= Menu::Create();
				recent.SetStyle(MNS_NOTIFYBYPOS);

				int		id		= 1;
				for (auto i : GetSettings("DX12/Recent"))
					Menu::Item(i.GetString(), id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);

				Menu::Item("New executable...", 0).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
				recent.Separator();

				pids.clear();
				ModuleSnapshot	snapshot(0, TH32CS_SNAPPROCESS);
				id		= 0x1000;
				for (auto i : snapshot.processes()) {
					filename	path;
					if (FindModule(i.th32ProcessID, "dx12crude.dll", path)) {
						pids.push_back(i.th32ProcessID);
						Menu::Item(i.szExeFile, id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
					}
				}

				Menu::Item("Set Executable for DX12", ID_ORBISCRUDE_GRAB_DX12).SubMenu(recent).InsertByPos(menu, 0);
				break;
			}
					
			case ID_ORBISCRUDE_PAUSE: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				if (mode == MODE_click && (menu.GetItemStateByID(ID_ORBISCRUDE_GRAB_DX12) & MF_CHECKED)) {
					TogglePause();
					return true;
				}
				break;
			}

			case ID_ORBISCRUDE_GRAB: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				if (mode == MODE_click && (menu.GetItemStateByID(ID_ORBISCRUDE_GRAB_DX12) & MF_CHECKED)) {
					
					if (!process.Active())
						OpenProcess(fn, string(main.DescendantByID(ID_EDIT + 2).GetText()), string(main.DescendantByID(ID_EDIT + 1).GetText()), "dx12crude.dll");

					main.SetTitle("new");
					//Grab(new ViewDX12GPU(main, main.Dock(DOCK_TAB)));

					auto	view		= new ViewDX12GPU(main, main.Dock(DOCK_TAB), process, fn);
					RunThread([this, view,
						progress	= Progress(WindowPos(*view, AdjustRect(view->GetRect().Centre(500, 30), Control::OVERLAPPED | Control::CAPTION, false)), "Capturing", 0)
					]() mutable {
						Grab(view, progress);
					});
				}
				break;
			}
		}
		return false;
	}
#endif

public:
	void	operator()(Control c, Menu::Item i) {
		ISO::Browser2	settings	= GetSettings("DX12/Recent");
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
		m.RadioDirect(ID_ORBISCRUDE_GRAB_DX12, ID_ORBISCRUDE_GRAB_TARGETS, ID_ORBISCRUDE_GRAB_TARGETS_MAX);
		Menu::Item().Check(true).Type(MFT_RADIOCHECK).SetByPos(m, m.FindPosition(recent));

		recent.RadioDirect(id, 0, recent.Count());
		//CreateReadPipe(pipe);

		const char *dll_name	= "dx12crude.dll";

		if (id & 0x1000) {
			OpenProcess(pids[id & 0xfff], dll_name);
			fn = process.Filename();

		} else {
			fn = settings[id - 1].GetString();

			string	exec_dir, exec_cmd;
			if (DialogBase home = main->DescendantByID("Home")) {
				exec_cmd = home.ItemText(ID_EDIT + 1);
				exec_dir = home.ItemText(ID_EDIT + 2);
			}

			OpenProcess(fn, exec_dir, exec_cmd, dll_name);
			if (resume_after) {
				ISO_OUTPUT("Resuming process\n");
				Call<void>(INTF_Continue);
			}
		}

		ConnectDebugOutput();

	}

	EditorDX12() {
		ISO::getdef<DX12Capture>();
	}

} editor_dx12;

void EditorDX12::Grab(ViewDX12GPU *view, Progress &progress) {
//	RunThread([this, view,
//		progress = Progress(WindowPos(*view, AdjustRect(view->GetRect().Centre(500, 30), Control::OVERLAPPED | Control::CAPTION, false)), "Capturing", 0)
//	]() mutable {
		auto	num_objects	= Call<uint32>(INTF_CaptureFrames, until_halt ? 0x7ffffff : num_frames);
		auto	objects		= Call<with_size<dynamic_array<RecObject2>>>(INTF_GetObjects);

		if (objects.empty()) {
			view->SendMessage(WM_ISO_ERROR, 0, (LPARAM)"\nFailed to capture");
			return;
		}

		int						counts[RecObject::NUM_TYPES] = {};
		hash_multiset<string>	name_counts;

		view->objects.reserve(objects.size());

		//dx::memory_interface	mem(process.hProcess);
		auto mem = MemoryInterface();

		progress.Reset("Collecting objects", num_objects);

		uint64		total_reserved_gpu	= 0x4000000000000000ull;
		uint64		total_fake_gpu		= 0x8000000000000000ull;
		uint64		total_vram			= 0;

		for (auto &i : objects) {
			if (i.name) {
				if (int n = name_counts.insert(i.name))
					i.name << '(' << n << ')';
			} else {
				i.name << get_field_name(i.type) << '#' << counts[i.type]++;
			}

			switch (i.type) {
				case RecObject::Resource: {
					RecResource	*res = i.info;
					switch (res->alloc) {
						case RecResource::Committed:
							if (!res->gpu) {
								res->gpu		= total_fake_gpu;
								total_fake_gpu	+= res->data_size;
							}
							total_vram += res->data_size;
							break;

						case RecResource::Reserved: {
							res->gpu			= total_reserved_gpu;
							//res2.alloc		= RecResource::Committed;
							total_reserved_gpu	+= res->data_size;
							total_vram			+= res->data_size;
							break;
						}
					}
					break;
				}
				case RecObject::Heap: {
				#if 1
					RecHeap		*heap	= i.info;
					heap->gpu			= total_fake_gpu;
					total_fake_gpu		+= heap->SizeInBytes;
					total_vram			+= heap->SizeInBytes;
				#endif
					break;
				}
			}

			DX12Assets::ObjectRecord *obj = view->AddObject(i.type,
				move(i.name),
				move(i.info),
				i.obj,
				view->cache, &mem
			);
			progress.Set(objects.index_of(i));
		}

		progress.Reset("Grabbing VRAM", total_vram);
		total_vram = 0;

		if (total_reserved_gpu > 0x4000000000000000ull)
			view->cache.add_block(0x4000000000000000ull, total_reserved_gpu - 0x4000000000000000ull);

		if (total_fake_gpu > 0x8000000000000000ull)
			view->cache.add_block(0x8000000000000000ull, total_fake_gpu - 0x8000000000000000ull);

		total_reserved_gpu = 0x4000000000000000ull;
		for (auto &i : view->objects) {
			switch (i.type) {
				case RecObject::Resource: {
					const RecResource	*res = i.info;
					if (res->alloc == RecResource::Committed || res->alloc == RecResource::Reserved) {
						ISO_OUTPUTF("VRAM: 0x") << hex(res->gpu) << " to 0x" << hex(res->gpu + res->data_size) << " (0x" << hex(res->data_size) << ")\n";
						auto	mem = Call<malloc_block_all>(INTF_ResourceData, uintptr_t(i.obj));
						view->cache.add_block(res->gpu, mem);
						total_vram += mem.size();
						progress.Set(total_vram);
					}
					break;
				}

				case RecObject::Heap: {
					const RecHeap		*heap = i.info;
					ISO_OUTPUTF("VRAM: 0x") << hex(heap->gpu) << " to 0x" << hex(heap->gpu + heap->SizeInBytes) << " (0x" << hex(heap->SizeInBytes) << ")\n";
					auto	mem	= Call<malloc_block_all>(INTF_HeapData, uintptr_t(i.obj));
					view->cache.add_block(heap->gpu, mem);
					total_vram += mem.size();
					progress.Set(total_vram);
					break;
				}
			}
		}

		if (resume_after) {
			ISO_OUTPUT("Resuming process\n");
			Call<void>(INTF_Continue);
		} else {
			paused = true;
		}

		uint64		total = view->CommandTotal();
		progress.Reset("Parsing commands", total);
		view->GetAssets(&progress, total);
		//view->GetStatistics();

		JobQueue::Main().add([view] { view->MakeTree(); });

//	});
}


static struct test {
	test() {
		auto	f = fields<map_t<KM, D3D12_COMMAND_SIGNATURE_DESC>>::f;
	}
} _test;

void dx12_dummy() {}
