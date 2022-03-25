#include "main.h"
#include "graphics.h"
#include "thread.h"
#include "jobs.h"
#include "vm.h"
#include "disassembler.h"
#include "base/functions.h"

#include "iso/iso.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"

#include "viewers/viewer.h"
#include "common/shader.h"

#include "windows/filedialogs.h"
#include "windows/text_control.h"
#include "windows/dib.h"
#include "extra/indexer.h"
#include "extra/colourise.h"

#include "filetypes/3d/model_utils.h"
#include "view_dx12gpu.rc.h"
#include "resource.h"
#include "hook.h"
#include "hook_com.h"
#include "hook_stack.h"
#include "extra/xml.h"
#include "dx12/dx12_record.h"
#include "dx/dxgi_read.h"
#include "dx/sim_dxbc.h"
#include "..\dx_shared\dx_gpu.h"
#include "..\dx_shared\dx_fields.h" 
#include "..\debug.h"

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
template<> struct field_names<D3D12_FILTER>					{ static field_value s[]; };
template<> struct field_names<D3D12_DSV_FLAGS>				{ static field_bit	s[]; };
template<> struct field_names<D3D12_ROOT_SIGNATURE_FLAGS>	{ static field_bit	s[]; };
template<> struct field_names<D3D12_TILE_COPY_FLAGS>		{ static field_bit	s[]; };
template<> struct field_names<D3D12_TILE_RANGE_FLAGS>		{ static field_bit	s[]; };
template<> struct field_names<D3D12_CLEAR_FLAGS>			{ static field_bit	s[]; };
template<> struct field_names<D3D12_FENCE_FLAGS>			{ static field_bit	s[]; };

template<> struct field_names<D3D12_GPU_DESCRIPTOR_HANDLE>	: field_customs<D3D12_GPU_DESCRIPTOR_HANDLE>	{};
template<> struct field_names<D3D12_CPU_DESCRIPTOR_HANDLE>	: field_customs<D3D12_CPU_DESCRIPTOR_HANDLE>	{};
template<> struct fields<D3D12_CPU_DESCRIPTOR_HANDLE>		: value_field<D3D12_CPU_DESCRIPTOR_HANDLE>	{};

//template<> struct field_names<D3D12_GPU_DESCRIPTOR_HANDLE>	: field_names_none		{};
//template<> struct field_names<D3D12_CPU_DESCRIPTOR_HANDLE>	: field_names_none		{};
//template<> struct field_is_struct<D3D12_GPU_DESCRIPTOR_HANDLE> : T_true {};
//template<> struct field_is_struct<D3D12_CPU_DESCRIPTOR_HANDLE> : T_true {};

template<> struct fields<D3D12_TILE_RANGE_FLAGS> : value_field<D3D12_TILE_RANGE_FLAGS> {};

Control BinaryWindow(const WindowPos &wpos, const ISO_browser2 &b);
Control BitmapWindow(const WindowPos &wpos, const ISO_ptr<void> &p, const char *text, bool auto_scale);
Control ThumbnailWindow(const WindowPos &wpos, const ISO_ptr<void> &p, const char *_text);
bool	MakeThumbnail(void *dest, const ISO_ptr<void> &p, const POINT &size);

C_types	DX11_ctypes;

template<> struct C_types::type_getter<float16> {
	static const C_type *f(C_types &types)	{ return C_type_float::get<float16>(); }
};
template<typename I, I S> struct C_types::type_getter<scaled<I,S> > {
	static const C_type *f(C_types &types)	{ return C_type_int::get<I>(); }
};

const C_type *dxgi_c_types[] = {
	0,									//DXGI_FORMAT_UNKNOWN					= 0,
	0,									//DXGI_FORMAT_R32G32B32A32_TYPELESS		= 1,
	DX11_ctypes.get_type<float[4]>(),	//DXGI_FORMAT_R32G32B32A32_FLOAT		= 2,
	DX11_ctypes.get_type<uint32[4]>(),	//DXGI_FORMAT_R32G32B32A32_UINT			= 3,
	DX11_ctypes.get_type<int[4]>(),		//DXGI_FORMAT_R32G32B32A32_SINT			= 4,
	0,									//DXGI_FORMAT_R32G32B32_TYPELESS		= 5,
	DX11_ctypes.get_type<float[3]>(),	//DXGI_FORMAT_R32G32B32_FLOAT			= 6,
	DX11_ctypes.get_type<uint32[3]>(),	//DXGI_FORMAT_R32G32B32_UINT			= 7,
	DX11_ctypes.get_type<int[3]>(),		//DXGI_FORMAT_R32G32B32_SINT			= 8,
	0,									//DXGI_FORMAT_R16G16B16A16_TYPELESS		= 9,
	DX11_ctypes.get_type<float16[4]>(),	//DXGI_FORMAT_R16G16B16A16_FLOAT		= 10,
	DX11_ctypes.get_type<unorm16[4]>(),	//DXGI_FORMAT_R16G16B16A16_UNORM		= 11,
	DX11_ctypes.get_type<uint16[4]>(),	//DXGI_FORMAT_R16G16B16A16_UINT			= 12,
	DX11_ctypes.get_type<norm16[4]>(),	//DXGI_FORMAT_R16G16B16A16_SNORM		= 13,
	DX11_ctypes.get_type<int16[4]>(),	//DXGI_FORMAT_R16G16B16A16_SINT			= 14,
	0,									//DXGI_FORMAT_R32G32_TYPELESS			= 15,
	DX11_ctypes.get_type<float[2]>(),	//DXGI_FORMAT_R32G32_FLOAT				= 16,
	DX11_ctypes.get_type<uint32[2]>(),	//DXGI_FORMAT_R32G32_UINT				= 17,
	DX11_ctypes.get_type<int[2]>(),		//DXGI_FORMAT_R32G32_SINT				= 18,
	0,									//DXGI_FORMAT_R32G8X24_TYPELESS			= 19,
	0,									//DXGI_FORMAT_D32_FLOAT_S8X24_UINT		= 20,
	0,									//DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS	= 21,
	0,									//DXGI_FORMAT_X32_TYPELESS_G8X24_UINT	= 22,
	0,									//DXGI_FORMAT_R10G10B10A2_TYPELESS		= 23,
	0,									//DXGI_FORMAT_R10G10B10A2_UNORM			= 24,
	0,									//DXGI_FORMAT_R10G10B10A2_UINT			= 25,
	0,									//DXGI_FORMAT_R11G11B10_FLOAT			= 26,
	0,									//DXGI_FORMAT_R8G8B8A8_TYPELESS			= 27,
	DX11_ctypes.get_type<unorm8[4]>(),	//DXGI_FORMAT_R8G8B8A8_UNORM			= 28,
	DX11_ctypes.get_type<unorm8[4]>(),	//DXGI_FORMAT_R8G8B8A8_UNORM_SRGB		= 29,
	DX11_ctypes.get_type<uint8[4]>(),	//DXGI_FORMAT_R8G8B8A8_UINT				= 30,
	DX11_ctypes.get_type<norm8[4]>(),	//DXGI_FORMAT_R8G8B8A8_SNORM			= 31,
	DX11_ctypes.get_type<int8[4]>(),	//DXGI_FORMAT_R8G8B8A8_SINT				= 32,
	0,									//DXGI_FORMAT_R16G16_TYPELESS			= 33,
	DX11_ctypes.get_type<float16[2]>(),	//DXGI_FORMAT_R16G16_FLOAT				= 34,
	DX11_ctypes.get_type<unorm16[2]>(),	//DXGI_FORMAT_R16G16_UNORM				= 35,
	DX11_ctypes.get_type<uint16[2]>(),	//DXGI_FORMAT_R16G16_UINT				= 36,
	DX11_ctypes.get_type<norm16[2]>(),	//DXGI_FORMAT_R16G16_SNORM				= 37,
	DX11_ctypes.get_type<int16[2]>(),	//DXGI_FORMAT_R16G16_SINT				= 38,
	0,									//DXGI_FORMAT_R32_TYPELESS				= 39,
	0,									//DXGI_FORMAT_D32_FLOAT					= 40,
	DX11_ctypes.get_type<float>(),		//DXGI_FORMAT_R32_FLOAT					= 41,
	DX11_ctypes.get_type<uint32>(),		//DXGI_FORMAT_R32_UINT					= 42,
	DX11_ctypes.get_type<int>(),		//DXGI_FORMAT_R32_SINT					= 43,
	0,									//DXGI_FORMAT_R24G8_TYPELESS			= 44,
	0,									//DXGI_FORMAT_D24_UNORM_S8_UINT			= 45,
	0,									//DXGI_FORMAT_R24_UNORM_X8_TYPELESS		= 46,
	0,									//DXGI_FORMAT_X24_TYPELESS_G8_UINT		= 47,
	0,									//DXGI_FORMAT_R8G8_TYPELESS				= 48,
	DX11_ctypes.get_type<unorm8[4]>(),	//DXGI_FORMAT_R8G8_UNORM				= 49,
	DX11_ctypes.get_type<uint8[4]>(),	//DXGI_FORMAT_R8G8_UINT					= 50,
	DX11_ctypes.get_type<norm8[4]>(),	//DXGI_FORMAT_R8G8_SNORM				= 51,
	DX11_ctypes.get_type<int8[4]>(),	//DXGI_FORMAT_R8G8_SINT					= 52,
	0,									//DXGI_FORMAT_R16_TYPELESS				= 53,
	DX11_ctypes.get_type<float16>(),	//DXGI_FORMAT_R16_FLOAT					= 54,
	0,									//DXGI_FORMAT_D16_UNORM					= 55,
	DX11_ctypes.get_type<unorm16>(),	//DXGI_FORMAT_R16_UNORM					= 56,
	DX11_ctypes.get_type<uint16>(),		//DXGI_FORMAT_R16_UINT					= 57,
	DX11_ctypes.get_type<norm16>(),		//DXGI_FORMAT_R16_SNORM					= 58,
	DX11_ctypes.get_type<int16>(),		//DXGI_FORMAT_R16_SINT					= 59,
	0,									//DXGI_FORMAT_R8_TYPELESS				= 60,
	DX11_ctypes.get_type<unorm8>(),		//DXGI_FORMAT_R8_UNORM					= 61,
	DX11_ctypes.get_type<uint8>(),		//DXGI_FORMAT_R8_UINT					= 62,
	DX11_ctypes.get_type<norm8>(),		//DXGI_FORMAT_R8_SNORM					= 63,
	DX11_ctypes.get_type<int8>(),		//DXGI_FORMAT_R8_SINT					= 64,
	0,									//DXGI_FORMAT_A8_UNORM					= 65,
	0,									//DXGI_FORMAT_R1_UNORM					= 66,
	0,									//DXGI_FORMAT_R9G9B9E5_SHAREDEXP		= 67,
	0,									//DXGI_FORMAT_R8G8_B8G8_UNORM			= 68,
	0,									//DXGI_FORMAT_G8R8_G8B8_UNORM			= 69,
	0,									//DXGI_FORMAT_BC1_TYPELESS				= 70,
	0,									//DXGI_FORMAT_BC1_UNORM					= 71,
	0,									//DXGI_FORMAT_BC1_UNORM_SRGB			= 72,
	0,									//DXGI_FORMAT_BC2_TYPELESS				= 73,
	0,									//DXGI_FORMAT_BC2_UNORM					= 74,
	0,									//DXGI_FORMAT_BC2_UNORM_SRGB			= 75,
	0,									//DXGI_FORMAT_BC3_TYPELESS				= 76,
	0,									//DXGI_FORMAT_BC3_UNORM					= 77,
	0,									//DXGI_FORMAT_BC3_UNORM_SRGB			= 78,
	0,									//DXGI_FORMAT_BC4_TYPELESS				= 79,
	0,									//DXGI_FORMAT_BC4_UNORM					= 80,
	0,									//DXGI_FORMAT_BC4_SNORM					= 81,
	0,									//DXGI_FORMAT_BC5_TYPELESS				= 82,
	0,									//DXGI_FORMAT_BC5_UNORM					= 83,
	0,									//DXGI_FORMAT_BC5_SNORM					= 84,
	0,									//DXGI_FORMAT_B5G6R5_UNORM				= 85,
	0,									//DXGI_FORMAT_B5G5R5A1_UNORM			= 86,
	DX11_ctypes.get_type<unorm8[4]>(),	//DXGI_FORMAT_B8G8R8A8_UNORM			= 87,
	0,									//DXGI_FORMAT_B8G8R8X8_UNORM			= 88,
	0,									//DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM= 89,
	0,									//DXGI_FORMAT_B8G8R8A8_TYPELESS			= 90,
	0,									//DXGI_FORMAT_B8G8R8A8_UNORM_SRGB		= 91,
	0,									//DXGI_FORMAT_B8G8R8X8_TYPELESS			= 92,
	0,									//DXGI_FORMAT_B8G8R8X8_UNORM_SRGB		= 93,
	0,									//DXGI_FORMAT_BC6H_TYPELESS				= 94,
	0,									//DXGI_FORMAT_BC6H_UF16					= 95,
	0,									//DXGI_FORMAT_BC6H_SF16					= 96,
	0,									//DXGI_FORMAT_BC7_TYPELESS				= 97,
	0,									//DXGI_FORMAT_BC7_UNORM					= 98,
	0,									//DXGI_FORMAT_BC7_UNORM_SRGB			= 99,
	0,									//DXGI_FORMAT_AYUV						= 100,
	0,									//DXGI_FORMAT_Y410						= 101,
	0,									//DXGI_FORMAT_Y416						= 102,
	0,									//DXGI_FORMAT_NV12						= 103,
	0,									//DXGI_FORMAT_P010						= 104,
	0,									//DXGI_FORMAT_P016						= 105,
	0,									//DXGI_FORMAT_420_OPAQUE				= 106,
	0,									//DXGI_FORMAT_YUY2						= 107,
	0,									//DXGI_FORMAT_Y210						= 108,
	0,									//DXGI_FORMAT_Y216						= 109,
	0,									//DXGI_FORMAT_NV11						= 110,
	0,									//DXGI_FORMAT_AI44						= 111,
	0,									//DXGI_FORMAT_IA44						= 112,
	0,									//DXGI_FORMAT_P8						= 113,
	0,									//DXGI_FORMAT_A8P8						= 114,
	0,									//DXGI_FORMAT_B4G4R4A4_UNORM			= 115,
	0,									//										= 116,
	0,									//										= 117,
	0,									//										= 118,
	0,									//										= 119,
	0,									//										= 120,
	0,									//										= 121,
	0,									//										= 122,
	0,									//										= 123,
	0,									//										= 124,
	0,									//										= 125,
	0,									//										= 126,
	0,									//										= 127,
	0,									//										= 128,
	0,									//										= 129,
	0,									//DXGI_FORMAT_P208						= 130,
	0,									//DXGI_FORMAT_V208						= 131,
	0,									//DXGI_FORMAT_V408						= 132,
};

const C_type *to_c_type(DXGI::FORMAT_PLUS f) {
	const C_type *c_types[8][3] = {
		//8 bits							16									32
		{0,									0,									0,									},	//TYPELESS,
		{0,									DX11_ctypes.get_type<float16>(),	DX11_ctypes.get_type<float>(),		},	//FLOAT,
		{0,									DX11_ctypes.get_type<float16>(),	DX11_ctypes.get_type<float>(),		},	//UFLOAT,
		{DX11_ctypes.get_type<uint8>(),		DX11_ctypes.get_type<uint16>(),		DX11_ctypes.get_type<uint32>(),		},	//UINT,
		{DX11_ctypes.get_type<int8>(),		DX11_ctypes.get_type<int16>(),		DX11_ctypes.get_type<int32>(),		},	//SINT,
		{DX11_ctypes.get_type<unorm8>(),	DX11_ctypes.get_type<unorm16>(),	DX11_ctypes.get_type<unorm32>(),	},	//UNORM,
		{DX11_ctypes.get_type<norm8>(),		DX11_ctypes.get_type<norm16>(),		DX11_ctypes.get_type<norm32>(),		},	//SNORM,
		{0,									0,									0,									},	//SRGB
	};

	auto	info = f.GetInfo();

	const C_type	*c = c_types[info.type][log2(info.bits) - 3];
	if (info.comps > 1)
		c = DX11_ctypes.add(C_type_array(c, info.comps));
	return c;
}

template<typename T, int N> string_accum& operator<<(string_accum &sa, const fixed_array<T, N> &a) {
	for (auto &i : a)
		sa << i << ' ';
	return sa;
}

//-----------------------------------------------------------------------------
//	DX12Assets
//-----------------------------------------------------------------------------

struct DX12Assets {

	struct MarkerRecord : interval<uint32> {
		string		str;
		uint32		colour:24, flags:8;
		MarkerRecord(string &&_str, uint32 _start, uint32 _end) : interval<uint32>(_start, _end), str(move(_str)), colour(0), flags(0) {}
	};
	struct CallRecord {
		uint32		addr, dest;
		CallRecord(uint32 _addr, uint32 _dest) : addr(_addr), dest(_dest) {}
		bool operator<(uint32 offset) const { return dest < offset; }
	};

	struct ShaderRecord {
		SHADERSTAGE			stage;
		uint64				addr;
		malloc_block		code;
		string				name;
		string				GetName()		const	{ return name; }
		uint64				GetBase()		const	{ return addr; }
		uint64				GetSize()		const	{ return code.length(); }
		memory_block		GetUCode()		const {
			if (dx::SHEX *shex = ((dx::DXBC*)code)->GetBlob<dx::SHEX>())
				return shex->GetUCode();
			return empty;
		}
		uint64				GetUCodeAddr()	const	{
			if (dx::SHEX *shex = ((dx::DXBC*)code)->GetBlob<dx::SHEX>())
				return addr + ((char*)shex->instructions - (char*)code);
			return addr;
		}
	};

	struct ObjectRecord : RecObject2 {
		int					index;
		ObjectRecord(RecObject::TYPE type, string16 &&name, void *obj) : RecObject2(type, move(name), obj), index(-1) {}
		string				GetName()		const	{ return str8(name); }
		uint64				GetBase()		const	{ return uint64(obj); }
		uint64				GetSize()		const	{ return info.length(); }
	};

	struct ResourceRecord : RecResource {
		ObjectRecord		*obj;
		ISO_ptr<void>		p;
		string				GetName()		const	{ return str8(obj->name); }
		uint64				GetBase()		const	{ return gpu; }
		uint64				GetSize()		const	{ return data.length(); }
		ResourceRecord(ObjectRecord *_obj) : RecResource(*_obj->info), obj(_obj) {}
	};

	struct DescriptorHeapRecord {
		ObjectRecord		*obj;
		DescriptorHeapRecord(ObjectRecord *_obj) : obj(_obj) {}
		RecDescriptorHeap	*operator->()	const { return obj->info; }
		operator RecDescriptorHeap*()		const { return obj->info; }
	};

	struct BatchInfo { union {
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
	}; };

	struct BatchRecord : BatchInfo {
		uint32		addr;
		uint32		op:6, use_offset:26;
		BatchRecord(uint16 _op, uint32 _addr, uint32 _use_offset) : addr(_addr), op(_op), use_offset(_use_offset) {}	
		D3D_PRIMITIVE_TOPOLOGY	getPrim() const { return (D3D_PRIMITIVE_TOPOLOGY)draw.prim; }
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

	dynamic_array<MarkerRecord>				markers;
	dynamic_array<use>						uses;
	dynamic_array<BatchRecord>				batches;

	dynamic_array<ObjectRecord>				objects;
	dynamic_array<ResourceRecord>			resources;
	dynamic_array<ShaderRecord>				shaders;
	dynamic_array<DescriptorHeapRecord>		descriptor_heaps;
	ObjectRecord*							device_object;

	hash_map<void*, dynamic_array<use> >	pipeline_uses;
	dynamic_array<CallRecord>				calls;

	hash_map<uint64, ObjectRecord*>			object_map;
	interval_tree<uint64, ObjectRecord*>	vram_tree;

	DX12Assets() : device_object(0) {}

	void			AddCall(uint32 addr, uint32 dest) {
		new(calls) CallRecord(addr, dest);
	}

	BatchRecord*	AddBatch(uint16 op, uint32 addr) {
		return new(batches) BatchRecord(op, addr, uses.size32());
	}

	int				AddMarker(string16 &&s, uint32 start, uint32 end) {
		new(markers) MarkerRecord(move(s), start, end);
		return markers.size32() - 1;
	}

	ShaderRecord*	AddShader(D3D12_SHADER_BYTECODE &b, SHADERSTAGE stage, memory_interface *mem) {
		if (!b.pShaderBytecode || !b.BytecodeLength)
			return 0;
		ShaderRecord *r = FindShader((uint64)b.pShaderBytecode);
		if (!r) {
			r			= new(shaders) ShaderRecord;
			r->stage	= stage;
			r->addr		= (uint64)b.pShaderBytecode;
			if (mem)
				r->code = mem->get((uint64)b.pShaderBytecode, b.BytecodeLength);

			dx::DXBC	*h		= r->code;
			if (SPDB *spdb = h->GetBlob<SPDB>()) {
				ParsedSPDB	parsed(spdb);
				auto &start = *parsed.locations.begin();
				r->name	= parsed.entry << '(' << parsed.files[start.file].name << ')';
			}
		}
		return r;
	}
	
	ShaderRecord*	AddShader(SHADERSTAGE stage, string16 &&name, malloc_block &&code, uint64 addr) {
		ShaderRecord *r = new(shaders) ShaderRecord;
		r->stage	= stage;
		r->addr		= addr;
		r->code		= keep(code);
		r->name		= move(name);
		return r;
	}

	const BatchRecord*	GetBatch(uint32 addr) const {
		return lower_boundc(batches, addr, [](const BatchRecord &r, uint32 addr) { return r.addr < addr; });
	}
	const MarkerRecord*	GetMarker(uint32 addr) const {
		return lower_boundc(markers, addr, [](const MarkerRecord &r, uint32 addr) { return r.a < addr; });
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
			new(uses) use(e, GetIndex(e));
	}
	template<typename T> void AddUse(const T *e) {
		if (e)
			new(uses) use(e, GetIndex(e));
	}


	ObjectRecord*	FindObject(uint64 addr) {
		if (auto *p = object_map.check(addr))
			return *p;
		return 0;
	}
	ShaderRecord*	FindShader(uint64 addr) {
		for (auto &i : shaders) {
			if (i.GetBase() == addr)
				return &i;
		}
		return 0;
	}
	ObjectRecord*	FindByGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS a) {
		auto	i = vram_tree.lower_bound(a);
		return a >= i.key().a ? *i : 0;
	}
	RecResource*	FindResource(ID3D12Resource *res) {
		if (auto *obj = FindObject((uint64)res))
			return obj->info;
		return 0;
	}
	DESCRIPTOR*		FindDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : descriptor_heaps) {
			if (DESCRIPTOR *d = i->holds(h))
				return d;
		}
		return 0;
	}
	DESCRIPTOR*		FindDescriptor(D3D12_GPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : descriptor_heaps) {
			if (DESCRIPTOR *d = i->holds(h))
				return d;
		}
		return 0;
	}

	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr);
	
	ObjectRecord	*AddObject(RecObject::TYPE type, string16 &&name, malloc_block &&info, void *obj, cache_type &cache, memory_interface *mem) {
		ObjectRecord	*p	= new (objects) ObjectRecord(type, move(name), obj);
		object_map[uint64(obj)] = p;
		switch (p->type) {
			case RecObject::Device:
				device_object			= p;
				p->info					= keep(info);
				break;

			case RecObject::Resource: {
				RecResource		*r		= p->info = keep(info);
				p->index				= resources.size32();
				if (r->gpu && r->data.length32()) {
					ISO_TRACEF("VRAM: 0x") << hex(r->gpu) << "to 0x" << hex(r->gpu + r->data.length32()) << '\n';
					vram_tree.insert(r->gpu, r->gpu + r->data.length32(), p);
					if (mem && r->data.start)
						cache.add_block(r->gpu, keep(mem->get((uint64)r->data.start, r->data.length32())));
				}
				new(resources) ResourceRecord(p);
				break;
			}

			case RecObject::DescriptorHeap: {
				RecDescriptorHeap	*h	= p->info = keep(info);
				p->index				= descriptor_heaps.size32();
				descriptor_heaps.push_back(p);
				break;
			}

			case RecObject::PipelineState: {
				auto	&uses	= pipeline_uses[info];
				p->info = keep(info);
				if (info.length() >= sizeof(T_map<RTM,D3D12_GRAPHICS_PIPELINE_STATE_DESC>::type)) {
					auto	pipeline = rmap_struct<KM, D3D12_GRAPHICS_PIPELINE_STATE_DESC>(p->info);
					D3D12_GRAPHICS_PIPELINE_STATE_DESC	*t	= pipeline;
					AddUse(AddShader(t->VS, VS, mem), uses);
					AddUse(AddShader(t->PS, PS, mem), uses);
					AddUse(AddShader(t->DS, DS, mem), uses);
					AddUse(AddShader(t->HS, HS, mem), uses);
					AddUse(AddShader(t->GS, GS, mem), uses);
				} else {
					D3D12_COMPUTE_PIPELINE_STATE_DESC	*t	= p->info;
					AddUse(AddShader(t->CS, CS, mem), uses);
				}
				break;
			}
			default:
				p->info = keep(info);
				break;
		}
		return p;
	}
};

bool SubTree(RegisterTree &rt, HTREEITEM h, memory_block mem, intptr_t offset, DX12Assets &assets);

void DX12Assets::operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
	buffer_accum<256>	ba;
	PutFieldName(ba, tree.format, pf, tree.prefix);

	uint64		v = pf->get_raw_value(p, offset);

	if (pf->values == (const char**)field_names<D3D12_GPU_DESCRIPTOR_HANDLE>::s) {
		D3D12_GPU_DESCRIPTOR_HANDLE h;
		h.ptr = v;
		ba << "0x" << hex(v);
		for (auto &i : descriptor_heaps) {
			if (DESCRIPTOR *d = i->holds(h)) {
				ba << " (" << i.obj->name << '[' << i->index(d) << "])";
				break;
			}
		}

	} else if (pf->values == (const char**)field_names<D3D12_CPU_DESCRIPTOR_HANDLE>::s) {
		D3D12_CPU_DESCRIPTOR_HANDLE h;
		h.ptr = v;
		ba << "0x" << hex(v);
		for (auto &i : descriptor_heaps) {
			if (DESCRIPTOR *d = i->holds(h)) {
				ba << " (" << i.obj->name << '[' << i->index(d) << "])";
				break;
			}
		}

	} else if (auto *rec = FindObject(v)) {
		ba << rec->name;

		if (pf == fields<CommandRange>::f) {
			const CommandRange	*r	= (const CommandRange*)p;
			SubTree(tree, h, rec->info.sub_block(r->begin(), r->length()), rec->index - intptr_t(rec->info.start), *this);
		}

	} else if (auto *rec = FindShader(v)) {
		ba << rec->name;

	} else {
		ba << "(unfound)0x" << hex(v);
	}
	tree.AddText(h, ba, addr);
}

interval<uint32> ResolveRange(const CommandRange &r, DX12Assets &assets) {
	if (r.length()) {
		if (auto *obj = assets.FindObject(uint64(&*r)))
			return r + obj->index;
	}
	return empty;
}

memory_block GetCommands(const CommandRange &r, DX12Assets &assets, uint32 &addr) {
	if (r.length()) {
		if (auto *obj = assets.FindObject(uint64(&*r))) {
			addr -= obj->index + r.begin();
			return obj->info.sub_block(r.begin(), min(addr, r.length()));
		}
	}
	return empty;
}

template<> dynamic_array<DX12Assets::ShaderRecord>&			DX12Assets::GetTable()	{ return shaders; }
template<> dynamic_array<DX12Assets::ObjectRecord>&			DX12Assets::GetTable()	{ return objects; }
template<> dynamic_array<DX12Assets::ResourceRecord>&		DX12Assets::GetTable()	{ return resources;	}

template<typename T> uint32 GetSize(const T &t)				{ return t.GetSize(); }

const char *field_names<SHADERSTAGE>::s[]	= {
	"VS",
	"PS",
	"DS",
	"HS",
	"GS",
	"CS",
};
const char *field_names<RecObject::TYPE>::s[]	= {
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

field	fields<RecResource>::f[] = {
	field::call(fields<D3D12_RESOURCE_DESC>::f, 0),
	make_field<RecResource>("gpu",	&RecResource::gpu),
	0,
};

field	fields<DX12Assets::ObjectRecord>::f[] = {
	make_field<DX12Assets::ObjectRecord>("type",	&DX12Assets::ObjectRecord::type),
	make_field<DX12Assets::ObjectRecord>("obj",		&DX12Assets::ObjectRecord::obj),
	field::call_union(union_fields<void,void,void,RecResource*,void,void,void,void,void,void,void,void,void>::p, T_get_member_offset(&DX12Assets::ObjectRecord::info) * 8, 2),
	0,
};



field	fields<DX12Assets::ResourceRecord>::f[] = {
	field::call(fields<RecResource>::f, 0),
	0,
};

field	fields<DX12Assets::ShaderRecord>::f[] = {
	make_field<DX12Assets::ShaderRecord>("addr",	&DX12Assets::ShaderRecord::addr),
	make_field<DX12Assets::ShaderRecord>("stage",	&DX12Assets::ShaderRecord::stage),
	0,
};

//-----------------------------------------------------------------------------
//	DX12State
//-----------------------------------------------------------------------------

struct DX12State : DX12Assets::BatchInfo {
	RecCommandList::tag							mode;

	struct RootState {
		com_ptr<ID3D12RootSignatureDeserializer>	deserializer;
		dynamic_array<uint32>						offset;
		malloc_block								data;
		void				SetRootSignature(DX12Assets *assets, ID3D12RootSignature *p);
		DESCRIPTOR			GetBound(SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, RecDescriptorHeap *dh) const;
		arbitrary_ptr		Slot(int i)			{ return data + offset[i]; }
		arbitrary_const_ptr	Slot(int i) const	{ return data + offset[i]; }
	};

//	memory_block	pipeline;
	malloc_block	pipeline;
	bool			graphics;
	RootState		graphics_root;
	RootState		compute_root;

	DX12Assets::ObjectRecord					*cbv_srv_uav_descriptor_heap;
	DX12Assets::ObjectRecord					*sampler_descriptor_heap;
	D3D12_INDEX_BUFFER_VIEW						ibv;
	dynamic_array<D3D12_VERTEX_BUFFER_VIEW>		vbv;
	D3D12_CPU_DESCRIPTOR_HANDLE					targets[8];
	D3D12_CPU_DESCRIPTOR_HANDLE					depth;
	float										BlendFactor[4];
	uint32										StencilRef;

	array<D3D12_VIEWPORT, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>	viewport;
	array<D3D12_RECT, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>		scissor;

	DX12State()		{ Reset(); }
	void	Reset() { clear(*this); }

	void	SetPipelineState(DX12Assets *assets, ID3D12PipelineState *p);

	const D3D12_VIEWPORT&	GetViewport(int i)	const { return viewport[i]; };
	const D3D12_RECT&		GetScissor(int i)	const { return scissor[i]; };

	Topology GetTopology() const {
		static const Topology::Type prim_conv[] = {
			Topology::UNKNOWN,
			Topology::POINTLIST,
			Topology::LINELIST,
			Topology::LINESTRIP,
			Topology::TRILIST,
			Topology::TRISTRIP,
			Topology::UNKNOWN,
			Topology::UNKNOWN,
			Topology::UNKNOWN,
			Topology::UNKNOWN,
			Topology::LINELIST_ADJ,
			Topology::LINESTRIP_ADJ,
			Topology::TRILIST_ADJ,
			Topology::TRISTRIP_ADJ,
		};
		if (draw.prim < num_elements(prim_conv))
			return prim_conv[draw.prim];
		Topology	t(Topology::PATCH);
		t.SetNumCP(draw.prim - 32);
		return  t;
	}

	BackFaceCull CalcCull(bool flipped) const {
		if (graphics) {
			D3D12_GRAPHICS_PIPELINE_STATE_DESC*		graphics_state = pipeline;
			return graphics_state->RasterizerState.CullMode == D3D12_CULL_MODE_NONE ? BFC_NONE
				: (graphics_state->RasterizerState.CullMode == D3D12_CULL_MODE_FRONT) ^ flipped ^ graphics_state->RasterizerState.FrontCounterClockwise
				? BFC_CW : BFC_CCW;
		}
		return (BackFaceCull)0;
	}

	indices	GetIndexing(cache_type &cache)	const {
		if (mode == RecCommandList::tag_DrawIndexedInstanced) {
			int		size	= DXGI::size(ibv.Format);
			return indices(cache(ibv.BufferLocation + draw.index_offset * size, draw.vertex_count * size), size, draw.vertex_offset, draw.vertex_count);
		}
		return indices(0, 0, draw.vertex_offset, draw.vertex_count);
	}

	DESCRIPTOR	GetBound(SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space) const {
		DX12Assets::ObjectRecord	*dhobj	= type == DESCRIPTOR::SMP ? sampler_descriptor_heap : cbv_srv_uav_descriptor_heap;
		RecDescriptorHeap			*dh		= dhobj ? dhobj->info : (RecDescriptorHeap*)0;
		return stage == CS
			? compute_root.GetBound(stage, type, bind, space, dh)
			: graphics_root.GetBound(stage, type, bind, space, dh);
	}

	void	ProcessDevice(DX12Assets *assets, uint16 id, const void *p) {
		switch (id) {
			case RecDevice::tag_CreateConstantBufferView:	COMParse(p, [this, assets](const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				assets->FindDescriptor(dest)->set(desc);
			}); break;
			case RecDevice::tag_CreateShaderResourceView:	COMParse(p, [this, assets](ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				assets->FindDescriptor(dest)->set(pResource, desc);
			}); break;
			case RecDevice::tag_CreateUnorderedAccessView:	COMParse(p, [this, assets](ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				assets->FindDescriptor(dest)->set(pResource, desc);
			}); break;
			case RecDevice::tag_CreateRenderTargetView:		COMParse(p, [this, assets](ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				assets->FindDescriptor(dest)->set(pResource, desc, *assets->FindResource(pResource));
			}); break;
			case RecDevice::tag_CreateDepthStencilView:		COMParse(p, [this, assets](ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				assets->FindDescriptor(dest)->set(pResource, desc, *assets->FindResource(pResource));
			}); break;
			case RecDevice::tag_CreateSampler:				COMParse(p, [this, assets](const D3D12_SAMPLER_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
				assets->FindDescriptor(dest)->set(desc);
			}); break;
			case RecDevice::tag_CopyDescriptors:			COMParse(p, [this, assets](UINT dest_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,0> dest_starts, counted<const UINT,0> dest_sizes, UINT srce_num, counted<const D3D12_CPU_DESCRIPTOR_HANDLE,3> srce_starts, counted<const UINT,3> srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE type)  {
				UINT		ssize = 0;
				DESCRIPTOR	*sdesc;

				for (UINT s = 0, d = 0; d < dest_num; d++) {
					UINT		dsize	= dest_sizes[d];
					DESCRIPTOR	*ddesc	= assets->FindDescriptor(dest_starts[d]);
					while (dsize) {
						if (ssize == 0) {
							ssize	= srce_sizes[s];
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
			case RecDevice::tag_CopyDescriptorsSimple:		COMParse(p, [this, assets](UINT num, D3D12_CPU_DESCRIPTOR_HANDLE dest_start, D3D12_CPU_DESCRIPTOR_HANDLE srce_start, D3D12_DESCRIPTOR_HEAP_TYPE type) {
				memcpy(assets->FindDescriptor(dest_start), assets->FindDescriptor(srce_start), sizeof(DESCRIPTOR) * num);
			}); break;
		}
	}
	void	ProcessCommand(DX12Assets *assets, uint16 id, const void *p) {
		switch (id) {
			case RecCommandList::tag_Close: COMParse(p, [this]() {
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
				});
				mode	= (RecCommandList::tag)id;
				break;
			case RecCommandList::tag_DrawIndexedInstanced: COMParse(p, [this](UINT IndexCountPerInstance, UINT instance_count, UINT index_offset, INT vertex_offset, UINT instance_offset) {
					draw.vertex_count		= IndexCountPerInstance;
					draw.instance_count		= instance_count;
					draw.index_offset		= index_offset;
					draw.vertex_offset		= vertex_offset;
					draw.instance_offset	= instance_offset;
				});
				mode	= (RecCommandList::tag)id;
				break;
			case RecCommandList::tag_Dispatch: COMParse(p, [this](UINT dim_x, UINT dim_y, UINT dim_z) {
					compute.dim_x			= dim_x;
					compute.dim_y			= dim_y;
					compute.dim_z			= dim_z;
				});
				mode	= (RecCommandList::tag)id;
				break;
			case RecCommandList::tag_CopyBufferRegion: COMParse(p, [this](ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes) {
				});
				break;
			case RecCommandList::tag_CopyTextureRegion: COMParse2(p, [this](const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox) {
				});
				break;
			case RecCommandList::tag_CopyResource: COMParse(p, [this](ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource) {
				});
				break;
			case RecCommandList::tag_CopyTiles: COMParse(p, [this](ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
				});
				break;
			case RecCommandList::tag_ResolveSubresource: COMParse(p, [this](ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) {
				});
				break;
			case RecCommandList::tag_IASetPrimitiveTopology: COMParse(p, [this](D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) {
					draw.prim	= PrimitiveTopology;
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
					memcpy(this->BlendFactor, BlendFactor, sizeof(BlendFactor));
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
			case RecCommandList::tag_ExecuteBundle: COMParse(p, [this](ID3D12GraphicsCommandList *pCommandList) {
				});
				break;
			case RecCommandList::tag_SetDescriptorHeaps: COMParse(p, [this, assets](UINT n, ID3D12DescriptorHeap *const *pp) {
				cbv_srv_uav_descriptor_heap = sampler_descriptor_heap = 0;
				for (int i = 0; i < n; i++) {
					if (auto *obj = assets->FindObject((uint64)pp[i])) {
						ISO_ASSERT(obj->type == RecObject::DescriptorHeap);
						RecDescriptorHeap	*h = obj->info;
						if (h->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
							sampler_descriptor_heap		= obj;
						else
							cbv_srv_uav_descriptor_heap = obj;
					}
				}
				});
				break;
			case RecCommandList::tag_SetComputeRootSignature: COMParse(p, [this, assets](ID3D12RootSignature *pRootSignature) {
					compute_root.SetRootSignature(assets, pRootSignature);
				});
				break;
			case RecCommandList::tag_SetGraphicsRootSignature: COMParse(p, [this, assets](ID3D12RootSignature *pRootSignature) {
					graphics_root.SetRootSignature(assets, pRootSignature);
				});
				break;
			case RecCommandList::tag_SetComputeRootDescriptorTable: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
					*compute_root.Slot(RootParameterIndex) = BaseDescriptor;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootDescriptorTable: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
					*graphics_root.Slot(RootParameterIndex) = BaseDescriptor;
				});
				break;
			case RecCommandList::tag_SetComputeRoot32BitConstant: COMParse(p, [this](UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
					((uint32*)compute_root.Slot(RootParameterIndex))[DestOffsetIn32BitValues] = SrcData;
				});
				break;
			case RecCommandList::tag_SetGraphicsRoot32BitConstant: COMParse(p, [this](UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
					((uint32*)graphics_root.Slot(RootParameterIndex))[DestOffsetIn32BitValues] = SrcData;
				});
				break;
			case RecCommandList::tag_SetComputeRoot32BitConstants: COMParse(p, [this](UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
					memcpy((uint32*)compute_root.Slot(RootParameterIndex) + DestOffsetIn32BitValues, pSrcData, Num32BitValuesToSet / 4);
				});
				break;
			case RecCommandList::tag_SetGraphicsRoot32BitConstants: COMParse(p, [this](UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues) {
					memcpy((uint32*)graphics_root.Slot(RootParameterIndex) + DestOffsetIn32BitValues, pSrcData, Num32BitValuesToSet / 4);
				});
				break;
			case RecCommandList::tag_SetComputeRootConstantBufferView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*compute_root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootConstantBufferView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*graphics_root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetComputeRootShaderResourceView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*compute_root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootShaderResourceView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*graphics_root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetComputeRootUnorderedAccessView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*compute_root.Slot(RootParameterIndex) = BufferLocation;
				});
				break;
			case RecCommandList::tag_SetGraphicsRootUnorderedAccessView: COMParse(p, [this](UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
					*graphics_root.Slot(RootParameterIndex) = BufferLocation;
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
			case RecCommandList::tag_SOSetTargets: COMParse(p, [this](UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews) {
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
			case RecCommandList::tag_ClearDepthStencilView: COMParse(p, [this](D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_ClearRenderTargetView: COMParse(p, [this](D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_ClearUnorderedAccessViewUint: COMParse(p, [this](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_ClearUnorderedAccessViewFloat: COMParse(p, [this](D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects) {
				});
				break;
			case RecCommandList::tag_DiscardResource: COMParse(p, [this](ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion) {
				});
				break;
			case RecCommandList::tag_BeginQuery: COMParse(p, [this](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
				});
				break;
			case RecCommandList::tag_EndQuery: COMParse(p, [this](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) {
				});
				break;
			case RecCommandList::tag_ResolveQueryData: COMParse(p, [this](ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset) {
				});
				break;
			case RecCommandList::tag_SetPredication: COMParse(p, [this](ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation) {
				});
				break;
			case RecCommandList::tag_SetMarker: COMParse(p, [this](UINT Metadata, const void *pData, UINT Size) {
				});
				break;
			case RecCommandList::tag_BeginEvent: COMParse(p, [this](UINT Metadata, const void *pData, UINT Size) {
				});
				break;
			case RecCommandList::tag_EndEvent: COMParse(p, [this]() {
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
				mode	= (RecCommandList::tag)id;
				break;
		}
	}
};

void DX12State::SetPipelineState(DX12Assets *assets, ID3D12PipelineState *p) {
	graphics	= false;

	if (auto *obj = assets->FindObject(uint64(p))) {
	#if 1
		if (obj->info.length() >= sizeof(T_map<RTM,D3D12_GRAPHICS_PIPELINE_STATE_DESC>::type)) {
			pipeline	= rmap_struct<KM, D3D12_GRAPHICS_PIPELINE_STATE_DESC>(obj->info);
			D3D12_GRAPHICS_PIPELINE_STATE_DESC*		graphics_state = pipeline;
			graphics_root.SetRootSignature(assets, graphics_state->pRootSignature);
			graphics	= true;
		} else {
			pipeline	= rmap_struct<KM, D3D12_COMPUTE_PIPELINE_STATE_DESC>(obj->info);
			D3D12_COMPUTE_PIPELINE_STATE_DESC*		compute_state = pipeline;
			compute_root.SetRootSignature(assets, compute_state->pRootSignature);
		}
	#else
		pipeline = obj->info;
		if (obj->info.length() >= sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC)) {
			D3D12_GRAPHICS_PIPELINE_STATE_DESC*		graphics_state = pipeline;
			graphics_root.SetRootSignature(assets, graphics_state->pRootSignature);
			graphics	= true;
		} else {
			D3D12_COMPUTE_PIPELINE_STATE_DESC*		compute_state = pipeline;
			compute_root.SetRootSignature(assets, compute_state->pRootSignature);
		}
	#endif
	}
}

void DX12State::RootState::SetRootSignature(DX12Assets *assets, ID3D12RootSignature *p) {
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

DESCRIPTOR PointerDescriptor(DESCRIPTOR::TYPE type, D3D12_GPU_VIRTUAL_ADDRESS ptr) {
	DESCRIPTOR	d(type);
	d.ptr	= ptr;
	return d;
}

DESCRIPTOR DX12State::RootState::GetBound(SHADERSTAGE stage, DESCRIPTOR::TYPE type, int bind, int space, RecDescriptorHeap *dh) const {
	static const uint8	visibility[] = {
		0xff,
		1 << VS,
		1 << HS,
		1 << DS,
		1 << GS,
		1 << PS,
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
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
					uint32	start = 0;
					for (int j = 0; j < p.DescriptorTable.NumDescriptorRanges; j++) {
						const D3D12_DESCRIPTOR_RANGE	&range = p.DescriptorTable.pDescriptorRanges[j];

						if (range.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
							start = range.OffsetInDescriptorsFromTableStart;

						if (range.RangeType == as<D3D12_DESCRIPTOR_RANGE_TYPE>(type) && bind >= range.BaseShaderRegister && bind < range.BaseShaderRegister + range.NumDescriptors) {
							if (DESCRIPTOR *d = dh->holds(*(const D3D12_GPU_DESCRIPTOR_HANDLE*)Slot(i)))
								return d[bind - range.BaseShaderRegister + start];
						}
						start += range.NumDescriptors;
					}
					break;
				}

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

//-----------------------------------------------------------------------------
//	ShaderState
//-----------------------------------------------------------------------------

dx::SimulatorDXBC::Resource MakeSimulatorResource(DX12Assets &assets, cache_type &cache, const DESCRIPTOR &desc) {
	dx::SimulatorDXBC::Resource	r;
	auto		*obj	= assets.FindObject((uint64)desc.res);
	if (!obj)
		return r;

	RecResource	*rr		= obj->info;
	uint64		gpu		= rr->gpu;

	if (desc.type == DESCRIPTOR::SRV) {
		switch (desc.srv.ViewDimension) {
			case D3D12_SRV_DIMENSION_UNKNOWN:
			case D3D12_SRV_DIMENSION_BUFFER:
				r.init(dx::RESOURCE_DIMENSION_BUFFER, desc.srv.Format, desc.srv.Buffer.StructureByteStride, desc.srv.Buffer.NumElements);
				gpu	+= desc.srv.Buffer.FirstElement;
				break;

			case D3D12_SRV_DIMENSION_TEXTURE1D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1D, desc.srv.Format, rr->Width, 0);
				r.set_mips(desc.srv.Texture1D.MostDetailedMip, desc.srv.Texture1D.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1DARRAY, desc.srv.Format, rr->Width, 0, desc.srv.Texture1DArray.ArraySize);
				r.set_mips(desc.srv.Texture1DArray.MostDetailedMip, desc.srv.Texture1DArray.MipLevels);
				r.set_slices(desc.srv.Texture1DArray.FirstArraySlice, desc.srv.Texture1DArray.ArraySize);
				break;

			case D3D12_SRV_DIMENSION_TEXTURE2D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, desc.srv.Format, rr->Width, rr->Height);
				r.set_mips(desc.srv.Texture2D.MostDetailedMip, desc.srv.Texture2D.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2DARRAY, desc.srv.Format, rr->Width, rr->Height, desc.srv.Texture2DArray.FirstArraySlice + desc.srv.Texture2DArray.ArraySize);
				r.set_mips(desc.srv.Texture2DArray.MostDetailedMip, desc.srv.Texture2DArray.MipLevels);
				r.set_slices(desc.srv.Texture2DArray.FirstArraySlice, desc.srv.Texture2DArray.ArraySize);
				break;

			//case D3D12_SRV_DIMENSION_TEXTURE2DMS:
			//case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
			case D3D12_SRV_DIMENSION_TEXTURE3D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE3D, desc.srv.Format, rr->Width, rr->Height, rr->DepthOrArraySize);
				r.set_mips(desc.srv.Texture3D.MostDetailedMip, desc.srv.Texture3D.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				r.init(dx::RESOURCE_DIMENSION_TEXTURECUBE, desc.srv.Format, rr->Width, rr->Height, 6);
				r.set_mips(desc.srv.TextureCube.MostDetailedMip, desc.srv.TextureCube.MipLevels);
				break;

			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURECUBEARRAY, desc.srv.Format, rr->Width, rr->Height, desc.srv.TextureCubeArray.First2DArrayFace + desc.srv.TextureCubeArray.NumCubes * 6);
				r.set_mips(desc.srv.TextureCubeArray.MostDetailedMip, desc.srv.TextureCubeArray.MipLevels);
				r.set_slices(desc.srv.TextureCubeArray.First2DArrayFace, desc.srv.TextureCubeArray.NumCubes * 6);
				break;
		}

	} else if (desc.type == DESCRIPTOR::UAV) {
		switch (desc.uav.ViewDimension) {
			case D3D12_UAV_DIMENSION_BUFFER:
				gpu	+= desc.uav.Buffer.FirstElement;
				r.init(dx::RESOURCE_DIMENSION_BUFFER, desc.srv.Format, desc.uav.Buffer.StructureByteStride, desc.uav.Buffer.NumElements);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE1D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1D, desc.srv.Format, rr->Width, 0);
				r.set_mips(desc.uav.Texture1D.MipSlice, 1);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE1DARRAY, desc.srv.Format, rr->Width, desc.uav.Texture1DArray.FirstArraySlice + desc.uav.Texture1DArray.ArraySize);
				r.set_mips(desc.uav.Texture1DArray.MipSlice, 1);
				r.set_slices(desc.uav.Texture1DArray.FirstArraySlice, desc.uav.Texture1DArray.ArraySize);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE2D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2D, desc.srv.Format, rr->Width, rr->Height);
				r.set_mips(desc.uav.Texture2D.MipSlice, 1);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE2DARRAY, desc.srv.Format, rr->Width, rr->Height, desc.uav.Texture2DArray.FirstArraySlice + desc.uav.Texture2DArray.ArraySize);
				r.set_mips(desc.uav.Texture2DArray.MipSlice, 1);
				r.set_slices(desc.uav.Texture2DArray.FirstArraySlice, desc.uav.Texture2DArray.ArraySize);
				break;

			case D3D12_UAV_DIMENSION_TEXTURE3D:
				r.init(dx::RESOURCE_DIMENSION_TEXTURE3D, desc.srv.Format, rr->Width, rr->Height, rr->DepthOrArraySize);
				r.set_mips(desc.uav.Texture3D.MipSlice, 1);
				break;
		}
	}

	r.set_mem(cache(gpu + uintptr_t((void*)r), r.length()));
	return r;
}

dx::SimulatorDXBC::Buffer MakeSimulatorConstantBuffer(cache_type &cache, const DESCRIPTOR &desc) {
	dx::SimulatorDXBC::Buffer	r;
	
	switch (desc.type) {
		case DESCRIPTOR::CBV:
			r.init(cache(desc.cbv.BufferLocation, desc.cbv.SizeInBytes));
			break;
		case DESCRIPTOR::PCBV:
			r.init(cache(desc.ptr, desc.cbv.SizeInBytes));
			break;
		case DESCRIPTOR::IMM:
			r.init(memory_block(unconst(desc.imm), desc.cbv.SizeInBytes));
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

struct ShaderState {
	const DX12Assets::ShaderRecord	*rec;

	hash_map<uint32, DESCRIPTOR>	cbv;
	hash_map<uint32, DESCRIPTOR>	srv;
	hash_map<uint32, DESCRIPTOR>	uav;
	hash_map<uint32, DESCRIPTOR>	smp;

	ShaderState() : rec(0) {}
	ShaderState(const DX12Assets::ShaderRecord *_rec) : rec(_rec) {}
	void	init(DX12Assets &assets, cache_type &cache, const DX12Assets::ShaderRecord *_rec, const DX12State *state);
	dx::DXBC *DXBC() const { return rec->code; }

	void	InitSimulator(DX12Assets &assets, cache_type &cache, dx::SimulatorDXBC &sim) const {
		for (auto &i : cbv.with_keys())
			sim.cbv[i.key]	= MakeSimulatorConstantBuffer(cache, i.val);

		for (auto &i : srv.with_keys())
			sim.srv[i.key]	= MakeSimulatorResource(assets, cache, i.val);

		for (auto &i : uav.with_keys())
			sim.uav[i.key]	= MakeSimulatorResource(assets, cache, i.val);

		for (auto &i : smp.with_keys())
			sim.smp[i.key]	= MakeSimulatorSampler(assets, i.val);
	}
};

void ShaderState::init(DX12Assets &assets, cache_type &cache, const DX12Assets::ShaderRecord *_rec, const DX12State *state) {
	rec	= _rec;
	dx::DeclReader	decl(rec->GetUCode());
	SHADERSTAGE		stage = rec->stage;

	for (auto &i : decl.cbv) {
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
		smp[i.index]	= state->GetBound(stage, DESCRIPTOR::SMP, i.index, 0);
}

//-----------------------------------------------------------------------------
//	DX12Replay
//-----------------------------------------------------------------------------

struct DX12Replay : COMReplay<DX12Replay> {
	com_ptr<ID3D12Device>		device;
	com_ptr<ID3D12Fence>		fence;
	win::Handle					fence_event;
	uint32						fence_value;
	DX12Assets					&assets;
	hash_map<void*,void*>	obj2local;

	map<ID3D12CommandAllocator*,small_set<ID3D12GraphicsCommandList*, 8> >	alloc_cmds;
	hash_map<ID3D12GraphicsCommandList*,small_set<ID3D12CommandQueue*, 8> >	cmd_queues;

	uint32						descriptor_sizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	
	DX12Replay*	operator->()	{ return this; }

	void	*_lookup(void *p) {
		if (p == 0)
			return p;
		void	*r = obj2local[p];
		if (!r) {
			auto	*obj	= assets.FindObject((uint64)p);
			abort			= !obj;
			//ISO_ASSERT(obj);
			if (obj) {
				r = obj2local[p] = MakeLocal(obj);
				ISO_ASSERT(r);
			}
		}
		return r;
	}

	template<typename T> T* lookup(T *p) {
		return (T*)_lookup(p);
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
			if (DESCRIPTOR *d = i->holds(h)) {
				ID3D12DescriptorHeap		*heap	= (ID3D12DescriptorHeap*)obj2local[i.obj->obj];
				D3D12_CPU_DESCRIPTOR_HANDLE cpu		= heap->GetCPUDescriptorHandleForHeapStart();
				cpu.ptr += descriptor_sizes[i->type] * i->index(h);
				return cpu;
			}
		}
		return h;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE	lookup(D3D12_GPU_DESCRIPTOR_HANDLE h) {
		for (auto &i : assets.descriptor_heaps) {
			if (DESCRIPTOR *d = i->holds(h)) {
				ID3D12DescriptorHeap		*heap	= (ID3D12DescriptorHeap*)obj2local[i.obj->obj];
				D3D12_GPU_DESCRIPTOR_HANDLE gpu		= heap->GetGPUDescriptorHandleForHeapStart();
				gpu.ptr += descriptor_sizes[i->type] * i->index(h);
				return gpu;
			}
		}
		return h;
	}

	D3D12_GPU_VIRTUAL_ADDRESS lookup(D3D12_GPU_VIRTUAL_ADDRESS a, size_t size = 0) {
		auto						*obj	= assets.FindByGPUAddress(a);
		RecResource					*r		= obj->info;
		ISO_ASSERT(a >= r->gpu && a + size <= r->gpu + r->data.length());
		auto						r2		= (ID3D12Resource*)obj2local[obj->obj];
		D3D12_GPU_VIRTUAL_ADDRESS	a2		= r2->GetGPUVirtualAddress();
		return a2 + (a - r->gpu);
	}

	const void *lookup_shader(const void *p) {
		auto *r = assets.FindShader((uint64)p);
		if (r)
			return r->code;
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
			if (auto *obj = assets.FindObject((uint64)v0))
				((ID3D12Object*)v1)->SetName(obj->name);
		}
		return obj2local[v0] = v1;
	}

	template<typename F1, typename F2> void* ReplayPP2(const void *p, F2 f) {
		typedef	typename function<F1>::P		P;
		typedef typename T_map<PM, P>::type		RTL1;
		
		auto	*t	= (tuple<RTL1>*)p;
		auto	&pp	= t->get<TL_count<RTL1>::value - 1>();
		auto	v0	= save(*pp);

		Replay2<F1>(device, p, f);

		return InitObject(v0, *pp);
	}
	template<typename F> void* ReplayPP(const void *p, F f) {
		return ReplayPP2<F>(p, f);
	}

	void	Process(uint16 id, const void *p) {
		switch (id) {
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
		}
	}
	
	void	Process(ID3D12GraphicsCommandList *cmd, uint16 id, const void *p) {
		switch (id) {
			case RecCommandList::tag_Close:									Replay(cmd, p, &ID3D12GraphicsCommandList::Close); break;
			case RecCommandList::tag_Reset:									Replay(p, [this, cmd](ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
					alloc_cmds[pAllocator].insert(cmd);
					cmd->Reset(pAllocator, pInitialState);
				});
				break;
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
			case RecCommandList::tag_ClearRenderTargetView:					Replay(cmd, p, &ID3D12GraphicsCommandList::ClearRenderTargetView); break;
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
				*obj->info = ((ID3D12Resource*)r)->GetDesc();
				break;
			}
			case RecObject::Handle:
				r = CreateEvent(nullptr, false, false, nullptr);
				break;

			default:
				ISO_ASSERT(false);

			#if 0
			case RecObject::RootSignature:			device->CreateRootSignature(desc, &r); break;
			case RecObject::Heap:					device->CreateHeap(desc, &r); break;
			case RecObject::Resource:				device->CreateResource(desc, &r); break;
			case RecObject::CommandAllocator:		device->CreateCommandAllocator(desc, &r); break;
			case RecObject::Fence:					device->CreateFence(desc, &r); break;
			case RecObject::PipelineState:			device->CreatePipelineState(desc, &r); break;
			case RecObject::DescriptorHeap:			device->CreateDescriptorHeap(desc, &r); break;
			case RecObject::QueryHeap:				device->CreateQueryHeap(desc, &r); break;
			case RecObject::CommandSignature:		device->CreateCommandSignature(desc, &r); break;
			case RecObject::GraphicsCommandList:	device->CreateGraphicsCommandList(desc, &r); break;
			case RecObject::CommandQueue:			device->CreateCommandQueue(desc, &r); break;
			case RecObject::Device:					device->CreateDevice(desc, &r); break;
			#endif
		}
		return r;
	}

	void	Wait(ID3D12CommandQueue *q) {
		q->Signal(fence, fence_value);
		fence->SetEventOnCompletion(fence_value++, fence_event);
		WaitForSingleObject(fence_event, INFINITE);
	}

	void	Wait(ID3D12CommandAllocator *a) {
		for (auto &c : alloc_cmds[a]) {
			for (auto &q : cmd_queues[c])
				Wait(q);
		}
	}

	DX12Replay(DX12Assets &_assets, IDXGIAdapter1 *adapter = 0) : assets(_assets) {
		Init(adapter);

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
			descriptor_sizes[i] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));

		device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fence);
		fence_event	= CreateEvent(nullptr, false, false, nullptr);
		fence_value	= 0;
	}
	~DX12Replay() {
		for (auto &i : alloc_cmds.with_keys())
			Wait(i.key());

		for (auto i : obj2local) {
			if (i) {
				if (uintptr_t(i) < (uint64(1) << 32))
					CloseHandle(i);
				else
					((IUnknown*)i)->Release();
			}
		}
	}
};

void Replay(DX12Assets &assets, uint32 addr) {
	DX12Replay	replay(assets);
	bool		done	= false;

	for (const COMRecording::header *p = assets.device_object->info, *e = assets.device_object->info.end(); !done && p < e; p = p->next()) {
		//ISO_TRACEF("replay addr = 0x") << hex((char*)p - (char*)assets.device_object->info) << '\n';
		try {
			if (p->id == 0xffff) {
				auto	*obj	= replay._lookup(*(void**)p->data());
				p = p->next();

				switch (p->id) {
					//ID3D12CommandQueue
					case RecDevice::tag_CommandQueueUpdateTileMappings:	replay.Replay(obj, p->data(), &ID3D12CommandQueue::UpdateTileMappings); break;
					case RecDevice::tag_CommandQueueCopyTileMappings:	replay.Replay(obj, p->data(), &ID3D12CommandQueue::CopyTileMappings); break;
					
					case RecDevice::tag_CommandQueueExecuteCommandLists:
						COMParse(p->data(), [&](UINT NumCommandLists, counted<CommandRange,0> pp) {
							for (int i = 0; i < NumCommandLists; i++) {
								uint32			end		= addr;
								memory_block	mem		= GetCommands(pp[i], assets, end);
								auto			*rec	= assets.FindObject((uint64)(const ID3D12CommandList*)pp[i]);
								if (!rec)
									continue;
								auto			*cmd	= (ID3D12GraphicsCommandList*)replay.obj2local[rec->obj];

								replay.cmd_queues[cmd].insert((ID3D12CommandQueue*)obj);

								for (const COMRecording::header *p = mem, *e = mem.end(); p < e; p = p->next())
									replay.Process(cmd, p->id, p->data());

								done = end == mem.length();
								if (done)
									cmd->Close();
							}
						});
						replay.Replay(obj, p->data(), &ID3D12CommandQueue::ExecuteCommandLists);
						break;

					case RecDevice::tag_CommandQueueSetMarker:			replay.Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p->data(), &ID3D12CommandQueue::SetMarker); break;
					case RecDevice::tag_CommandQueueBeginEvent:			replay.Replay2<void(UINT,counted<const uint8,2>,UINT)>(obj, p->data(), &ID3D12CommandQueue::BeginEvent); break;
					case RecDevice::tag_CommandQueueEndEvent:			replay.Replay(obj, p->data(), &ID3D12CommandQueue::EndEvent); break;
					case RecDevice::tag_CommandQueueSignal:				replay.Replay(obj, p->data(), &ID3D12CommandQueue::Signal); break;
					case RecDevice::tag_CommandQueueWait:				replay.Replay(obj, p->data(), &ID3D12CommandQueue::Wait); break;
					//ID3D12CommandAllocator
					case RecDevice::tag_CommandAllocatorReset: {
						replay.Wait((ID3D12CommandAllocator*)obj);
						replay.Replay(obj, p->data(), &ID3D12CommandAllocator::Reset);
						break;
					};
					//ID3D12Fence
					case RecDevice::tag_FenceSetEventOnCompletion:		replay.Replay(obj, p->data(), &ID3D12Fence::SetEventOnCompletion); break;
					case RecDevice::tag_FenceSignal:					replay.Replay(obj, p->data(), &ID3D12Fence::Signal); break;
					case RecDevice::tag_WaitForSingleObjectEx:			replay.Replay(p->data(), [obj](DWORD dwMilliseconds, BOOL bAlertable) {
						WaitForSingleObjectEx(obj, dwMilliseconds, bAlertable);
					});
						break;
				}
			} else {
				replay.Process(p->id, p->data());
			}

		} catch (const char *error) {
			const char *e = string_end(error);
			while (--e > error && *e == '\r' || *e == '\n' || *e == '.' || *e == ' ')
				;
			ISO_TRACEF(str(error, e + 1)) << " at offset 0x" << hex((char*)p - (char*)assets.device_object->info) << '\n';
		}
	}
}

//-----------------------------------------------------------------------------
//	DX12Connection
//-----------------------------------------------------------------------------

struct DX12Connection : DX12Assets {
	cache_type	cache;
	
	void			GetAssets(progress &prog);
	ISO_ptr<void>	GetBitmap(ResourceRecord *t);
	void			GetStateAt(DX12State &state, uint32 addr);

	void	AddMemory(uint64 addr, const memory_block &mem)					{ memcpy(cache.add_block(addr, mem.length32()), mem, mem.length()); }
	uint32	FindDescriptorChange(uint64 h, uint32 stride);
	uint32	FindObjectCreation(void *obj);
};

void GetDescriptorsForBatch(DX12Assets &assets, const DX12State &state, DX12Assets::ShaderRecord *shader, SHADERSTAGE stage) {
	D3D12_SHADER_DESC				desc;
	com_ptr<ID3D12ShaderReflection>	refl;

	D3DReflect(shader->code, shader->code.length(), IID_ID3D12ShaderReflection, (void**)&refl);
	refl->GetDesc(&desc);

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

		DESCRIPTOR bound = state.GetBound(stage, type, res_desc.BindPoint, res_desc.Space);
		if (bound.res)
			assets.AddUse(assets.FindObject((uint64)bound.res));
	}
}

enum PIX {
	PIX_EVENT_UNICODE_VERSION = 0,
	PIX_EVENT_ANSI_VERSION = 1,
};

void DX12Connection::GetAssets(progress &prog) {
	// Get all assets
	DX12State	state;
	batches.reset();

	uint32	marker_tos	= -1;
	uint32	total		= 0;

	for (auto &o : objects) {
		if (o.type == RecObject::Device || o.type == RecObject::GraphicsCommandList) {
			o.index	= total;
			total	+= o.info.length32();
		}
	}

	for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); p < e; p	= p->next()) {
		uint32	addr = uint32((char*)p - (char*)device_object->info.start);

		if (p->id == 0xffff && p->next()->id == RecDevice::tag_CommandQueueExecuteCommandLists) {
			p = p->next();
			COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
				for (int i = 0; i < NumCommandLists; i++) {
					const CommandRange	&r	= pp[i];
					if (ObjectRecord *rec = FindObject(uint64(&*r))) {
						if (r.length())
							AddCall(addr, rec->index + r.begin());
					}
				}
			});
		}
	}

	for (auto &o : objects) {
		if (o.type == RecObject::GraphicsCommandList) {
			for (const COMRecording::header *p = o.info, *e = o.info.end(); p < e; p = p->next()) {
				uint32	offset = (uint8*)p - o.info + o.index;
				state.ProcessCommand(this, p->id, p->data());

				switch (p->id) {
					case RecCommandList::tag_SetMarker: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
							if (Metadata == PIX_EVENT_UNICODE_VERSION)
								AddMarker(str((wchar_t*)(char*)pData, Size / 2), offset, offset);
							else if (Metadata == PIX_EVENT_ANSI_VERSION)
								AddMarker(str((char*)pData, Size), offset, offset);
						});
						break;
					case RecCommandList::tag_BeginEvent: COMParse(p->data(), [&](UINT Metadata, counted<char, 2> pData, UINT Size) {
							if (Metadata == PIX_EVENT_UNICODE_VERSION)
								marker_tos = AddMarker(str((wchar_t*)(char*)pData, Size / 2), offset, marker_tos);
							else if (Metadata == PIX_EVENT_ANSI_VERSION)
								marker_tos = AddMarker(str((char*)pData, Size), offset, marker_tos);
						});
						break;
					case RecCommandList::tag_EndEvent: COMParse(p->data(), [&]() {
							uint32	next = markers[marker_tos].b;
							markers[marker_tos].b = offset;
							marker_tos	= next;
						});
						break;

					case RecCommandList::tag_DrawInstanced:
					case RecCommandList::tag_DrawIndexedInstanced: {
						auto	*b = AddBatch(p->id, ((uint8*)p->next() - o.info) + o.index);

						if (state.cbv_srv_uav_descriptor_heap)
							AddUse(state.cbv_srv_uav_descriptor_heap);

						if (state.sampler_descriptor_heap)
							AddUse(state.sampler_descriptor_heap);

						b->draw	= state.draw;

						D3D12_GRAPHICS_PIPELINE_STATE_DESC*		graphics_state = state.pipeline;
						uses.append(pipeline_uses[graphics_state]);

						for (uint32 i = 0, n = graphics_state->NumRenderTargets; i < n; i++) {
							if (DESCRIPTOR *d = FindDescriptor(state.targets[i]))
								AddUse(FindObject((uint64)d->res));
						}

						if (DESCRIPTOR *d = FindDescriptor(state.depth))
							AddUse(FindObject((uint64)d->res));

						if (state.mode == RecCommandList::tag_DrawIndexedInstanced)
							AddUse(FindByGPUAddress(state.ibv.BufferLocation));

						for (auto &i : state.vbv)
							AddUse(FindByGPUAddress(i.BufferLocation));
					
						for (int i = 0; i < 5; i++) {
							SHADERSTAGE	s = SHADERSTAGE(i);
							const D3D12_SHADER_BYTECODE	&shader = (&graphics_state->VS)[s];
							if (shader.pShaderBytecode)
								GetDescriptorsForBatch(*this, state, FindShader((uint64)shader.pShaderBytecode), s);
						}
						break;
					}
					case RecCommandList::tag_Dispatch: {
						auto	*b = AddBatch(p->id, ((uint8*)p->next() - o.info) + o.index);

						if (state.cbv_srv_uav_descriptor_heap)
							AddUse(state.cbv_srv_uav_descriptor_heap);

						if (state.sampler_descriptor_heap)
							AddUse(state.sampler_descriptor_heap);

						b->compute	= state.compute;

						D3D12_COMPUTE_PIPELINE_STATE_DESC*		compute_state = state.pipeline;
						uses.append(pipeline_uses[compute_state]);
						if (compute_state->CS.pShaderBytecode)
							GetDescriptorsForBatch(*this, state, FindShader((uint64)compute_state->CS.pShaderBytecode), CS);

						break;
					}
					case RecCommandList::tag_ExecuteIndirect: {
						auto	*b = AddBatch(p->id, ((uint8*)p->next() - o.info) + o.index);

						if (state.cbv_srv_uav_descriptor_heap)
							AddUse(state.cbv_srv_uav_descriptor_heap);

						if (state.sampler_descriptor_heap)
							AddUse(state.sampler_descriptor_heap);
						
						b->indirect	= state.indirect;

						break;
					}
				}
			}
		}
	}
	AddBatch(0, total);	//dummy end batch
}

void DX12Connection::GetStateAt(DX12State &state, uint32 addr) {
	state.Reset();

	if (addr < device_object->info.length32())
		return;

	bool	done = false;
	for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); !done && p < e; p = p->next()) {
		if (p->id == 0xffff) {
			p = p->next();
			
			switch (p->id) {
				case RecDevice::tag_CommandQueueExecuteCommandLists:
					COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
						for (int i = 0; i < NumCommandLists; i++) {
							uint32			end	= addr;
							memory_block	mem = GetCommands(pp[i], *this, end);
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

ISO_ptr<void> DX12Connection::GetBitmap(ResourceRecord *t) {
	if (!t || !t->HasBitmap())
		return ISO_NULL;
	if (!t->p) {
		auto	data = cache(t->gpu, t->data.length());
		if (!data)
			return ISO_NULL;
		switch (t->Dimension) {
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				t->p = DXGI::GetBitmap(t->GetName(), data, t->Format, t->Width, t->DepthOrArraySize, 1, 0);
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
				t->p = DXGI::GetBitmap(t->GetName(), data, t->Format, t->Width, t->Height, t->DepthOrArraySize, 0);
				break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				t->p = DXGI::GetBitmap(t->GetName(), data, t->Format, t->Width, t->Height, t->DepthOrArraySize, BMF_VOLUME);
				break;
		}
	}
	return t->p;
}

int GetThumbnail(Control control, ImageList &images, DX12Connection *con, DX12Assets::ResourceRecord *t) {
	if (t) {
		if (t->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			return 0;
		if (!t->HasBitmap())
			return 1;
		int	index = images.Add(win::Bitmap::Load("IDB_WAIT", 0, images.GetIconSize()));
		ConcurrentJobs::Get().add([control, images, index, con, t] {
			void			*bits;
			win::Bitmap		bitmap(win::Bitmap::CreateDIBSection(DIBHEADER(images.GetIconSize(), 32), &bits));
			MakeThumbnail(bits, con->GetBitmap(t), images.GetIconSize());
			images.Replace(index, bitmap);
			control.Invalidate(0, false);
		});
		return index;
	}
	return images.Add(win::Bitmap::Load("IDB_BAD"));
}

template<typename F> bool CheckView(uint64 h, const void *p) {
	typedef	typename function<F>::P			P;
	typedef typename T_map<RTM, P>::type		RTL1;
	return h == ((tuple<RTL1>*)p)->get<TL_count<RTL1>::value - 1>().ptr;
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
			return (uint8*)p - (uint8*)device_object->info;
	}
	return 0;
}

template<typename F> bool CheckCreated(void *obj, const void *p) {
	typedef	typename function<F>::P			P;
	typedef typename T_map<PM, P>::type		RTL1;
	auto	r	= *((tuple<RTL1>*)p)->get<TL_count<RTL1>::value - 1>();
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
			return (uint8*)p - (uint8*)device_object->info;
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	DX12BufferWindow
//-----------------------------------------------------------------------------

struct VertexBufferView : D3D12_VERTEX_BUFFER_VIEW {
	const C_type *type;
	uint64	GetBase()				const	{ return BufferLocation; }
	uint64	GetSize()				const	{ return SizeInBytes; }
	uint32	GetStride()				const	{ return StrideInBytes; }
	uint32	GetNumRecords()			const	{ return SizeInBytes / StrideInBytes; }
	uint32	GetNumFields()			const	{ return NumElements(type); }
	uint64	GetOffset(uint64 row)	const	{ return StrideInBytes * row; }
	VertexBufferView(const D3D12_VERTEX_BUFFER_VIEW &v, const C_type *_type) : D3D12_VERTEX_BUFFER_VIEW(v), type(_type) {};
};

struct DX12BufferWindow : public Window<DX12BufferWindow> {
	VertexWindow		vw;
	VertexBufferView	buff;
	memory_block		mem;

	uint64		GetElementOffset(uint32 row) {
		return buff.GetStride() * row;
	}

	void	Fill() {
		int		nc		= VertexWindow::Column("#").Width(50).Insert(vw, 0) + 1;
		VertexWindow::Column("offset").Width(50).Insert(vw, nc++);
		MakeHeaders(vw, nc, buff.type, buffer_accum<512>());
		vw.SetCount(buff.GetNumRecords());
	}

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE: {
				vw.Create(GetChildWindowPos(), WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | LVS_OWNERDATA);
				Fill();
				return 0;
			}

			case WM_SIZE:
				vw.Resize(Point(lParam));
				break;

			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case LVN_GETDISPINFO: {
						NMLVDISPINFO		*nmd = (NMLVDISPINFO*)nmh;
						buffer_accum<64>	ba;
						int					row = nmd->item.iItem, col = nmd->item.iSubItem;
						if (col == 0) {
							ba << row;

						} else if (col == 1) {
							ba << "0x" << hex(GetElementOffset(row));

						} else {
							uint64	offset	= GetElementOffset(row);
							if (~offset) {
								void	*data = mem + offset;
								int				shift;
								const C_type	*subtype = GetNth(data, buff.type, col - 2, shift);
								DumpData(ba, data, subtype, shift);
							}
						}
						nmd->item.pszText = ba;
						return 1;
					}
				}
				break;
			}

			case WM_NCDESTROY:
				delete this;
			case WM_DESTROY:
				return 0;
		}
		return Super(message, wParam, lParam);
	}


	DX12BufferWindow(const WindowPos &wpos, const char *title, cache_type &cache, const VertexBufferView &_buff)
		: buff(_buff), mem(cache(buff.GetBase(), buff.GetSize()))
	{
		Create(wpos, title, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, 0);
	}
};


struct DX12VertexInputWindow : public Window<DX12VertexInputWindow>, ColumnColours {
	dynamic_array<VertexBufferView>	buffers;
	indices							indexing;
	cache_block						data[16];

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				vw.Create(GetChildWindowPos(), WS_CHILD | WS_CLIPSIBLINGS | LVS_OWNERDATA);
				vw.id	= 'VI';
				return 0;

			case WM_SIZE:
				vw.Resize(Point(lParam));
				break;

			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case NM_CUSTOMDRAW:
						return CustomDraw((NMCUSTOMDRAW*)nmh, Parent());

					case LVN_GETDISPINFO: {
						NMLVDISPINFO		*nmd = (NMLVDISPINFO*)nmh;
						buffer_accum<64>	ba;
						int					row = nmd->item.iItem, col = nmd->item.iSubItem;

						if (col-- == 0) {
							ba << row;

						} else {
							if (indexing) {
								row = indexing[row];
								if (col-- == 0)
									ba << row;
							}
							for (int b = 0; b < 16 && col >= 0; b++) {
								const VertexBufferView &buff = buffers[b];
								int		nc = buff.GetNumFields();
								if (col < nc) {
									if (row < buff.GetNumRecords() && data[b]) {
										int				shift;
										void			*data2		= data[b] + buff.GetOffset(row);
										const C_type	*subtype	= GetNth(data2, buff.type, col, shift);
										DumpData(ba, data2, subtype, shift);
									}
								}
								col -= nc;
							}
						}
						nmd->item.pszText = ba;
						return 1;
					}

					case MeshNotification::SET:
						return Notification((MeshNotification*)nmh);
				}
				return Parent()(message, wParam, lParam);
			}

			case WM_NCDESTROY:
				delete this;
				return 0;

			default:
				if (message >= LVM_FIRST && message < LVM_FIRST + 0x100)
					return vw.SendMessage(message, wParam, lParam);
				break;
		}
		return Super(message, wParam, lParam);
	}

	DX12VertexInputWindow(const WindowPos &wpos, const char *title,
		cache_type &cache,
		const VertexBufferView *_buffers, const char **names, uint32 *dividers, int nb,
		indices &_indexing
	) : buffers(_buffers, _buffers + nb), indexing(_indexing) {

		Create(wpos, title, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0);
		int		nc	= AddColumn(0, "#", 50, RGB(255,255,255));
		if (indexing)
			nc = AddColumn(nc, "index", 50, RGB(224,224,224));

		for (int i = 0; i < nb; i++) {
			const VertexBufferView	&buff = buffers[i];
			buffer_accum<512>		ba;
			if (names && names[i])
				ba << names[i];
			else
				ba << 'b' << i;
			nc		= MakeHeaders(vw, nc, buff.type, ba);
			AddColour(nc, MakeColour(i + 1));
			data[i]	= cache(buff.GetBase(), buff.GetSize());
		}
		SetColumnWidths(vw.GetHeader());
		vw.SetCount(_indexing.num);
		vw.Show();
	}

};

//-----------------------------------------------------------------------------
//	DX12MeshWindow
//-----------------------------------------------------------------------------

MeshWindow *MakeMeshView(const WindowPos &wpos,
	Topology topology,
	const void *verts, uint32 stride, DXGI::FORMAT_PLUS fmt,
	const indices &ix,
	param(float3x2) viewport, BackFaceCull cull, MeshWindow::MODE mode
) {
	MeshWindow	*c			= new MeshWindow(wpos);
	cuboid		ext			= empty;
	bool		use_w		= mode != MeshWindow::PERSPECTIVE;
	uint32		num_prims	= topology.p2v.verts_to_prims(ix.num);
	int			num_hw		= topology.NumHWVertices(ix.num);
	uint32		num_recs	= ix.max_index() + 1;

	if (num_hw && verts) {
		aligned<float3, 16>	*prim_norms	= new aligned<float3, 16>[num_prims];

		if (ix) {
			float4p		*vec	= c->vb.Begin(num_recs);
			float4p		*v0		= vec;

			for (auto &i : make_range_n(make_param_iterator(strided(verts, stride), fmt), num_recs)) {
				*vec = i;
				if (!use_w)
					vec->w	= 1;
				vec++;
			}

			uint32	*i0			= c->ib.Begin(num_hw, MEM_CPU_READ);
			uint32	*idx		= i0;
			dynamic_bitarray<uint32>	used(num_recs);
			for (int i = 0; i < ix.num; i++) {
				uint32	j	= min(ix[i], num_recs - 1);
				used[j]		= true;
				*idx		= j;
				idx			= topology.Adjust(idx, i);
			}
			c->ib.End();
			for (int x = 0; (x = used.next_set(x)) >= 0; x++) {
				if (v0[x].w)
					ext |= project(float4(v0[x]));
			}

			auto	v		= make_indexed_iterator(make_stride_iterator((float3p*)v0, sizeof(*v0)), i0);
			GetFaceNormals(prim_norms, 
				make_prim_iterator(topology.hw_p2v, v),
				num_prims
			);

			int		ix0		= used.lowest_set();
			int		ix1		= used.highest_set();
			NormalRecord	*vert_norms	= new NormalRecord[ix1 - ix0 + 1];
			AddNormals(vert_norms - ix0, prim_norms,
				make_prim_iterator(topology.hw_p2v, i0),
				num_prims
			);

			float3p	*norm	= c->vb_norm.Begin(num_recs);
			for (int i = 0; i < ix1 - ix0 + 1; i++) {
				vert_norms[i].Normalise();
				norm[i + ix0] = vert_norms[i].Get(1);
			}
			delete[] vert_norms;

			c->vb_norm.End();
			c->vb.End();

		} else {
			float4p		*vec		= c->vb.Begin(num_hw);
			float4p		*v0			= vec;

			for (auto &i : make_range_n(make_param_iterator(strided(verts, stride), fmt), ix.num)) {
				*vec = i;
				if (!use_w)
					vec->w	= 1;
				ext |= project(float4(*vec));
				vec = topology.Adjust(vec, i);
			}

			auto	v = make_stride_iterator((float3p*)v0, sizeof(*v0));
			GetFaceNormals(prim_norms,
				make_prim_iterator(topology.hw_p2v, v),
				num_prims
			);
			float3p	*norm		= c->vb_norm.Begin(num_hw);
			for (int i = 0; i < num_hw; i++)
				norm[i] = prim_norms[topology.PrimFromVertex(i, true) / topology.hw_mul];

			c->vb_norm.End();
			c->vb.End();
		}
		delete[] prim_norms;
	}

	c->cull	= cull;
	c->flags.set(MeshWindow::FILL);
	c->Init(topology, ix.num, viewport, ext, mode);
	return c;
}

MeshWindow *MakeMeshView(const WindowPos &wpos, cache_type &cache, const DX12State &state, const D3D12_VERTEX_BUFFER_VIEW &vb, DXGI::FORMAT_PLUS fmt) {
	return MakeMeshView(wpos,
		state.GetTopology(),
		cache(vb.BufferLocation, vb.SizeInBytes), vb.StrideInBytes, fmt,
		state.GetIndexing(cache),
		identity, state.CalcCull(true), MeshWindow::PERSPECTIVE
	);
}
//-----------------------------------------------------------------------------
//	DX12StateControl
//-----------------------------------------------------------------------------

const char *field_names<dx::RTS0::VISIBILITY>::s[] =		{"ALL", "VERTEX", "HULL", "DOMAIN", "GEOMETRY", "PIXEL" };
const char *field_names<dx::RTS0::Range::TYPE>::s[] =		{ "SRV", "UAV", "CBV", "SMP" };
const char *field_names<dx::RTS0::Parameter::TYPE>::s[] =	{ "TABLE", "CONSTANTS", "CBV", "SRV", "UAV" };
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
MAKE_FIELDS2(dx::RTS0::Sampler, filter, address_u, address_v, address_w, mip_lod_bias, max_anisotropy, comparison_func, border, min_lod, max_lod, reg, space, visibility);
MAKE_FIELDS2(dx::RTS0::Constants, base, space, num);
MAKE_FIELDS2(dx::RTS0::Descriptor, reg, space, flags);
MAKE_FIELDS2(dx::RTS0::Range, type, num, base, space, offset);
MAKE_FIELDS2(dx::RTS0::Parameter, type, visibility);


void AddShaderReflection(RegisterTree &rt, HTREEITEM h, ID3D12ShaderReflection *refl) {
	D3D12_SHADER_DESC				desc;
	if (FAILED(refl->GetDesc(&desc)))
		return;

	rt.AddFields(rt.AddText(h, "D3D12_SHADER_DESC"), &desc);

	HTREEITEM	h2 = rt.AddText(h, "ConstantBuffers");
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

	h2 = rt.AddText(h, "BoundResources");
	for (int i = 0; i < desc.BoundResources; i++) {
		D3D12_SHADER_INPUT_BIND_DESC	res_desc;
		refl->GetResourceBindingDesc(i, &res_desc);
		rt.AddFields(rt.AddText(h2, res_desc.Name), &res_desc);
	}

	h2 = rt.AddText(h, "InputParameters");
	for (int i = 0; i < desc.InputParameters; i++) {
		D3D12_SIGNATURE_PARAMETER_DESC	in_desc;
		refl->GetInputParameterDesc(i, &in_desc);
		rt.AddFields(rt.AddText(h2, in_desc.SemanticName), &in_desc);
	}

	h2 = rt.AddText(h, "OutputParameters");
	for (int i = 0; i < desc.OutputParameters; i++) {
		D3D12_SIGNATURE_PARAMETER_DESC	out_desc;
		refl->GetOutputParameterDesc(i, &out_desc);
		rt.AddFields(rt.AddText(h2, out_desc.SemanticName), &out_desc);
	}
}

string_accum& GetDataValue(string_accum &sa, const void *data, D3D_SHADER_VARIABLE_TYPE type) {
	switch (type) {
		case D3D_SVT_BOOL:		return sa << !!*(int*)data;
		case D3D_SVT_INT:		return sa << *(int*)data;
		case D3D_SVT_FLOAT:		return sa << *(float*)data;
		case D3D_SVT_UINT:		return sa << *(uint32*)data;
		case D3D_SVT_UINT8:		return sa << *(int8*)data;
		case D3D_SVT_DOUBLE:	return sa << *(double*)data;
		default:				return sa << "<unsupported>";
	}
}

void AddValue(RegisterTree &rt, HTREEITEM h, const char *name, const void *data, ID3D12ShaderReflectionType *type) {
	D3D12_SHADER_TYPE_DESC	type_desc;
	if (FAILED(type->GetDesc(&type_desc)))
		return;
		
	buffer_accum<256>	ba(name);
	if (type_desc.Class == D3D_SVC_SCALAR) {
		rt.AddText(h, GetDataValue(ba << " = ", data, type_desc.Type));

	} else {
		int			stride = 0;
		switch (type_desc.Type) {
			case D3D_SVT_BOOL:		stride = 4; break;
			case D3D_SVT_INT:		stride = 4; break;
			case D3D_SVT_FLOAT:		stride = 4; break;
			case D3D_SVT_UINT:		stride = 4; break;
			case D3D_SVT_UINT8:		stride = 1; break;
			case D3D_SVT_DOUBLE:	stride = 8; break;
		}

		HTREEITEM	h2;
		switch (type_desc.Class) {
			case D3D_SVC_VECTOR:
				ba.reset() << '{';
				for (int i = 0; i < type_desc.Columns; i++)
					GetDataValue(ba << (i ? ',' : '{'), (uint8*)data + stride * i, type_desc.Type);
				h2 = rt.AddText(h, ba << '}');
				for (int i = 0; i < type_desc.Columns; i++)
					rt.AddText(h2, GetDataValue(ba.reset(), (uint8*)data + stride * i, type_desc.Type));
				break;

			case D3D_SVC_MATRIX_ROWS:
				h2 = rt.AddText(h, ba);
				for (int i = 0; i < type_desc.Rows; i++) {
					ba.reset() << '[' << i << "] = {";
					for (int j = 0; j < type_desc.Columns; j++)
						GetDataValue(ba << (i ? ',' : '{'), (uint8*)data + stride * (i * type_desc.Columns + j), type_desc.Type);
					HTREEITEM h3 = rt.AddText(h2, ba << '}');
					for (int j = 0; j < type_desc.Columns; j++)
						rt.AddText(h3, GetDataValue(ba.reset(), (uint8*)data + stride * (i * type_desc.Columns + j), type_desc.Type));
				}
				break;
			case D3D_SVC_MATRIX_COLUMNS:
				h2 = rt.AddText(h, ba);
				for (int i = 0; i < type_desc.Rows; i++) {
					ba.reset() << '[' << i << "] = {";
					for (int j = 0; j < type_desc.Columns; j++)
						GetDataValue(ba << (i ? ',' : '{'), (uint8*)data + stride * (j * type_desc.Rows + i), type_desc.Type);
					HTREEITEM h3 = rt.AddText(h2, ba << '}');
					for (int j = 0; j < type_desc.Columns; j++)
						rt.AddText(h3, GetDataValue(ba.reset(), (uint8*)data + stride * (j * type_desc.Rows + i), type_desc.Type));
				}
				break;
//			case D3D_SVC_OBJECT:
			case D3D_SVC_STRUCT:
				HTREEITEM	h2 = rt.AddText(h, ba);
				for (int i = 0; i < type_desc.Members; i++) {
					AddValue(rt, h2, type->GetMemberTypeName(i), data, type->GetMemberTypeByIndex(i));
				}
				break;
//			case D3D_SVC_INTERFACE_CLASS:
//			case D3D_SVC_INTERFACE_POINTER:
		}
	}
}

void AddValues(RegisterTree &rt, HTREEITEM h, ID3D12ShaderReflectionConstantBuffer *cb, const void *data) {
	D3D12_SHADER_BUFFER_DESC	cb_desc;
	if (SUCCEEDED(cb->GetDesc(&cb_desc))) {
		for (int i = 0; i < cb_desc.Variables; i++) {
			ID3D12ShaderReflectionVariable*		var = cb->GetVariableByIndex(i);
			D3D12_SHADER_VARIABLE_DESC			var_desc;
			if (SUCCEEDED(var->GetDesc(&var_desc)) && (var_desc.uFlags & D3D_SVF_USED)) {
				ID3D12ShaderReflectionType*		type = var->GetType();
				AddValue(rt, h, var_desc.Name, (const uint8*)data + var_desc.StartOffset, type);
			}
		}
	}
}


void AddRootSignature(RegisterTree &rt, HTREEITEM h, dx::RTS0 *rts0) {
	HTREEITEM	h3 = rt.AddText(h, "Parameters");

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

	rt.AddArray(rt.AddText(h, "Samplers"), rts0->samplers.entries(rts0));
}

HTREEITEM AddShader(RegisterTree &rt, HTREEITEM h, const ShaderState &shader, DX12Connection *con) {
	D3D12_SHADER_DESC				desc;
	com_ptr<ID3D12ShaderReflection>	refl;

	if (FAILED(D3DReflect(shader.rec->code, shader.rec->code.length(), IID_ID3D12ShaderReflection, (void**)&refl)) || FAILED(refl->GetDesc(&desc)))
		return h;

	AddShaderReflection(rt, rt.AddText(h, "ID3D12ShaderReflection"), refl);

	HTREEITEM	h2	= rt.AddText(h, "BoundResources");
	for (int i = 0; i < desc.BoundResources; i++) {
		D3D12_SHADER_INPUT_BIND_DESC	res_desc;
		refl->GetResourceBindingDesc(i, &res_desc);
		
		DESCRIPTOR *bound = 0;
		DESCRIPTOR::TYPE	type;
		switch (res_desc.Type) {
			case D3D_SIT_CBUFFER:		bound = shader.cbv.check(res_desc.BindPoint); break;
			case D3D_SIT_TBUFFER:
			case D3D_SIT_TEXTURE:
			case D3D_SIT_STRUCTURED:	bound = shader.srv.check(res_desc.BindPoint); break;
			case D3D_SIT_SAMPLER:		bound = shader.smp.check(res_desc.BindPoint); break;
			default:					bound = shader.uav.check(res_desc.BindPoint); break;
		}

		if (bound) {
			HTREEITEM	h3 = rt.AddText(h2, res_desc.Name);
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

	if (auto *rts0 = shader.DXBC()->GetBlob<dx::RTS0>())
		AddRootSignature(rt, rt.AddText(h, "Root Signature"), rts0);

	return h;
}

struct DX12StateControl : ColourTree {
	enum TYPE {
		ST_TARGET_COLOUR	= 1,
		ST_TARGET_DEPTH,
		ST_TARGET_STENCIL,
		ST_SHADER,
		ST_TEXTURE,
		ST_OBJECT,
		ST_VERTICES,
		ST_OUTPUTS,
		ST_GPUREG,
	};
	static COLORREF		colours[];
	static const Cursor	cursors[];
	static uint8		cursor_indices[][3];

	DX12Connection *con;

	DX12StateControl(DX12Connection *_con) : ColourTree(colours, cursors, cursor_indices), con(_con) {}

	HTREEITEM	AddShader(HTREEITEM h, const ShaderState &shader) {
		return ::AddShader(RegisterTree(*this, this), h, shader, con);
	}

	void operator()(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		buffer_accum<256>	ba;
		PutFieldName(ba, tree.format, pf, tree.prefix);

		uint64	v = pf->get_raw_value(p, offset);
		if (auto *rec = con->FindObject(v))
			tree.Add(h, ba << rec->name, ST_OBJECT, rec);
		else if (auto *rec = con->FindShader(v))
			tree.Add(h, ba << rec->name, ST_SHADER, rec);
		else
			tree.Add(h, ba << "0x" << hex(v), ST_GPUREG, addr + offset);
	}
};

COLORREF DX12StateControl::colours[] = {
	RGB(0,0,0),
	RGB(128,0,0),		//ST_TARGET_COLOUR
	RGB(128,64,0),		//ST_TARGET_DEPTH,
	RGB(128,64,0),		//ST_TARGET_STENCIL,
	RGB(128,0,64),		//ST_SHADER
	RGB(0,128,0),		//ST_TEXTURE,
	RGB(0,128,64),		//ST_OBJECT,
	RGB(64,0,128),		//ST_VERTICES
	RGB(0,0,0),			//ST_OUTPUTS,
	RGB(0,0,0),			//ST_GPUREG,
};
const Cursor DX12StateControl::cursors[] {
	Cursor::LoadSystem(IDC_HAND),
	CompositeCursor(Cursor::LoadSystem(IDC_HAND), Cursor::Load(IDR_OVERLAY_ADD, 0)),
};
uint8 DX12StateControl::cursor_indices[][3] = {
	{0,0,0},
	{1,2,0},	//ST_TARGET_COLOUR
	{1,2,0},	//ST_TARGET_DEPTH,
	{1,2,0},	//ST_TARGET_STENCIL,
	{1,2,0},	//ST_SHADER
	{1,2,0},	//ST_TEXTURE,
	{1,2,0},	//ST_OBJECT,
	{1,2,0},	//ST_VERTICES
	{1,2,0},	//ST_OUTPUTS,
	{1,2,0},	//ST_GPUREG,
};

//-----------------------------------------------------------------------------
//	DX12ShaderWindow
//-----------------------------------------------------------------------------

EditControl MakeSourceWindow(const WindowPos &wpos, const char *title, const SyntaxColourer &colourer, win::Font font, const char *s, int *active, size_t num_active, DWORD style, DWORD styleEx) {
	if (s) {
		TextWindow *tw = new TextWindow(wpos, title, style, styleEx);
		tw->SendMessage(EM_EXLIMITTEXT, 0, ~0);
		tw->SetFont(font);
		tw->SetText2(colourer.RTF(s, active, num_active, colourer.tabstop));
		return *tw;
	}
	return EditControl();
}

SyntaxColourer HLSLcolourer(const ISO_browser &settings) {
	static const char *keywords[] = {
		"AppendStructuredBuffer", "asm", "asm_fragment",
		"BlendState", "bool", "break", "Buffer", "ByteAddressBuffer",
		"case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment", "CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer",
		"default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword",
		"else", "export", "extern",
		"false", "float", "for", "fxgroup",
		"GeometryShader", "groupshared",
		"half", "Hullshader",
		"if", "in", "inline", "inout", "InputPatch", "int", "interface",
		"line", "lineadj", "linear", "LineStream",
		"matrix", "min16float", "min10float", "min16int", "min12int", "min16uint",
		"namespace", "nointerpolation", "noperspective", "NULL",
		"out", "OutputPatch",
		"packoffset", "pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise",
		"RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer", "RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D",
		"sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state", "static", "string", "struct", "switch", "StructuredBuffer",
		"tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS", "Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream",
		"uint", "uniform", "unorm", "unsigned",
		"vector", "vertexfragment", "VertexShader", "void", "volatile",
		"while",
	};

	static const char *functions[] = {
		 "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asuint", "asuint", "atan", "atan2",
		 "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross",
		 "D3DCOLORtoUBYTE4", "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync", "distance", "dot", "dst",
		 "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2",
		 "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth",
		 "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync",
		 "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange", "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr","InterlockedXor", "isfinite", "isinf", "isnan",
		 "ldexp", "length", "lerp", "lit", "log", "log10", "log2",
		 "mad", "max", "min", "modf", "msad4", "mul",
		 "noise", "normalize",
		 "pow", "printf", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg", "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin",
		 "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt",
		 "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step",
		 "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj", "tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc",
	 };
	static const char *operators[] = {
		"++",	"--",	"~",	"!",	"+",	"-",	"*",	"/",
		"%",	"<<",	">>",	"&",	"^",	"|",	"&&",	"||",
		",",	"<",	">",	"<=",	">=",	"==",	"!=",	"=",
		"+=",	"-=",	"*=",	"/=",	"%=",	"<<=",	">>=",	"&=",
		"^=",	"|=",	"?",	":",	"[",	"]",	"(",	")",
		"{",	"}",	".",	";",	"::",	"...",
	};
	static const char *semantics[] = {
		"BINORMAL[n]", "BLENDINDICES[n]", "BLENDWEIGHT[n]", "COLOR[n]", "NORMAL[n]", "POSITION[n]", "POSITIONT", "PSIZE[n]", "TANGENT[n]", "TEXCOORD[n]",
		"COLOR[n]", "FOG", "POSITION[n]", "PSIZE", "TESSFACTOR[n]", "TEXCOORD[n]", "COLOR[n]", "TEXCOORD[n]", "VFACE", "VPOS",
		"COLOR[n]", "DEPTH[n]",
		"SV_ClipDistance[n]", "SV_CullDistance[n]",
		"SV_Coverage", "SV_Depth", "SV_DepthGreaterEqual(n)", "SV_DepthLessEqual(n)", "SV_DispatchThreadID", "SV_DomainLocation",
		"SV_GroupID", "SV_GroupIndex", "SV_GroupThreadID", "SV_GSInstanceID",
		"SV_InnerCoverage", "SV_InsideTessFactor", "SV_InstanceID", "SV_IsFrontFace", "SV_OutputControlPointID",
		"SV_Position", "SV_PrimitiveID", "SV_RenderTargetArrayIndex", "SV_SampleIndex", "SV_StencilRef", "SV_Target[n]", "SV_TessFactor", "SV_VertexID", "SV_ViewportArrayIndex",
	};

	static range<const char**> all[] = {
		keywords,
		functions,
		operators,
		semantics,
//		states,
	};
	ISO_browser	cols = settings["Colours"];
	return SyntaxColourer(min(cols.Count(), num_elements(all)), all, cols, settings["Tabs"].GetInt() * 20);
};

Disassembler::State *DisassembleShader(const memory_block &ucode, uint64 addr) {
	static Disassembler	*dis = Disassembler::Find("DXBC");
	if (dis) {
		Disassembler::SymbolList	symbols;
		return dis->Disassemble(ucode, addr, symbols);
	}
	return 0;
}

struct DX12ShaderWindow : SplitterWindow {
	DX12StateControl	tree;
	TabControl3			*tabs[2];
	auto_ptr<Disassembler::State>	dis;
	ParsedSPDB			spdb;
	int					showsource;

	memory_block		ucode;
	uint64				ucode_addr;

	LRESULT		Proc(UINT message, WPARAM wParam, LPARAM lParam);

	void ShowCode(EditControl edit, uint32 addr, bool combine_lines) {
		if (~addr) {
			edit.SetSelection(edit.GetLineRange(dis->OffsetToMixedLine(&spdb.locations, addr, combine_lines)));
			edit.EnsureVisible();
		}
	}

	DX12ShaderWindow(const WindowPos &wpos, const char *title, DX12Connection *_con, const ShaderState &shader, int _showsource);
};

LRESULT	DX12ShaderWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_COMMAND:
			if (HIWORD(wParam) == EN_SETFOCUS && tabs[0] && tabs[1]) {
				EditControl	c((HWND)lParam);
				int			line	= c.GetLine(c.GetSelection().cpMin);
				int			id		= LOWORD(wParam);
				switch (id) {
					case 0: {
						if (auto loc = dis->MixedLineToSource(showsource ? &spdb.locations : 0, line, showsource==1))
							ShowSourceTabLine(*tabs[1], loc->file, loc->line);
						break;
					}
					default: {
						EditControl	edit	= tabs[0]->GetSelectedControl();
						if (auto loc = spdb.locations.find(id, line + 1))
							ShowSourceLine(edit, dis->OffsetToMixedLine(&spdb.locations, loc->offset, showsource==1));
						break;
					}
				}
				return 0;
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh, *this);
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


DX12ShaderWindow::DX12ShaderWindow(const WindowPos &wpos, const char *title, DX12Connection *con, const ShaderState &shader, int _showsource)
	: SplitterWindow(SWF_VERT | SWF_PROP), showsource(_showsource)
	, tree(con), spdb(shader.DXBC()->GetBlob<SPDB>())
{
	Create(wpos, title, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, 0);
	Rebind(this);

	ucode		= shader.rec->GetUCode();
	ucode_addr	= shader.rec->GetUCodeAddr();

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
	split2->Create(_GetPanePos(0), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0);

	// code tabs
	tabs[0] = new TabControl3(split2->_GetPanePos(0), "code", WS_CHILD | WS_CLIPSIBLINGS, 0);
	tabs[0]->SetFont(win::Font::DefaultGui());
#if 0
	com_ptr<ID3DBlob>			blob;
	if (SUCCEEDED(D3DDisassemble(shader.rec->code, shader.rec->code.length(), D3D_DISASM_ENABLE_COLOR_CODE, 0, &blob))) {
		void	*p		= blob->GetBufferPointer();
		size_t	size	= blob->GetBufferSize();
		Control	dis		= MakeHTMLViewer(split2->_GetPanePos(0), "Disassembly", (const char*)p, size);
		tabs[0]->AddItemControl(dis, "DirectX");
	}
#endif
	ISO_browser2	settings	= GetSettings("PSSL");
	auto			colourer	= HLSLcolourer(settings);
	
	dis = DisassembleShader(ucode, ucode_addr);

	TextWindow *tw = new TextWindow(split2->_GetPanePos(0), "Original", ES_READONLY);
	tw->SendMessage(EM_EXLIMITTEXT, 0, ~0);
	tw->SetText2(DumpDisassemble(temp_accum(1024 * 1024), dis, showsource ? &spdb : 0, colourer, showsource==1));
	tw->Show();
	tabs[0]->AddItemControl(*tw, "Disassembly");

	// tree
	split2->SetPanes(*tabs[0], tree.Create(split2->_GetPanePos(0), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT, 0), 100);
	tree.AddShader(TVI_ROOT, shader);
	tabs[0]->Show();

	// source tabs
	if (!spdb.files.empty()) {
		tabs[1] = new TabControl3(_GetPanePos(1), "source", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0);
		tabs[1]->SetFont(win::Font::DefaultGui());
		win::Font		font		= win::Font::Params(settings["Font"].GetString());
		for (auto &i : spdb.files.with_keys()) {
			EditControl c = MakeSourceWindow(tabs[1]->GetChildWindowPos(), filename(i.val.name).name_ext(), colourer, font, i.val.source, 0, 0, ES_READONLY, 0);
			c.SetID(i.key);
			tabs[1]->AddItemControl(c);
		}
		SetPanes(*split2, *tabs[1], 50);
		//ShowSource(showsource ? sdb : 0, dis[0], *tabs[1], 0, showsource==1);
		tabs[1]->ShowSelectedControl();
	} else {
		tabs[1] = 0;
		SetPane(0, *split2);
	}
}

Control MakeShaderViewer(const WindowPos &wpos, DX12Connection *_con, const ShaderState &shader) {
	return *new DX12ShaderWindow(wpos, shader.rec->GetName(), _con, shader, GetSettings("General/shader source").GetInt(1));
}

//-----------------------------------------------------------------------------
//	DX12ShaderDebuggerWindow
//-----------------------------------------------------------------------------

class DX12RegisterWindow : public RegisterWindow {
public:
	DX12RegisterWindow(const WindowPos &wpos) : RegisterWindow(wpos) {}
	void Init(const dx::SimulatorDXBC &sim, const ParsedSPDB &spdb);
	void Update(const dx::SimulatorDXBC &sim, const ParsedSPDB &spdb, uint64 pc);
};

void DX12RegisterWindow::Init(const dx::SimulatorDXBC &sim, const ParsedSPDB &spdb) {
	uint32	offset = 0;

	new(entries) Entry(0, "pc", 0, Entry::SIZE64);
	offset += 8;

	for (int i = 0; i < sim.NumTemps() * 4; i++, offset += 4)
		new(entries) Entry(offset, format_string("r%i.%c", i / 4, "xyzw"[i % 4]), float_field);

	for (uint64 m = sim.InputMask(); m; m = clear_lowest(m)) {
		int	i = lowest_set_index(m);
		for (int j = 0; j < 4; j++, offset += 4)
			new(entries) Entry(offset, format_string("v%i.%c", i, "xyzw"[j]), float_field);
	}

	for (uint64 m = sim.OutputMask(); m; m = clear_lowest(m)) {
		int	i = lowest_set_index(m);
		for (int j = 0; j < 4; j++, offset += 4)
			new(entries) Entry(offset, format_string("o%i.%c", i, "xyzw"[j]), float_field);
	}

	prev_regs.create(offset);
	Update(sim, spdb, sim.Address(sim.Begin()));

	if (Control p = GetPane(0))
		p.Destroy();
	if (Control p = GetPane(1))
		p.Destroy();

	SetPane(1, SendMessage(WM_ISO_NEWPANE));
}

void DX12RegisterWindow::Update(const dx::SimulatorDXBC &sim, const ParsedSPDB &spdb, uint64 pc) {
	Entry	*y		= entries;
	uint32			*pregs	= prev_regs;

	((uint64*)pregs)[0]	= pc;
	y		+= 1;
	pregs	+= 2;


	const uint32	*regs	= (const uint32*)sim.GetReg(dx::Operand::TYPE_TEMP, 0, 0);
	for (int i = 0; i < sim.NumTemps() * 4; i++, y++) {
		y->flags	= *regs != *pregs	? Entry::CHANGED	: 0;
		*pregs++	= *regs++;
	}

	for (uint64 m = sim.InputMask(); m; m = clear_lowest(m)) {
		const uint32	*regs	= (const uint32*)sim.GetReg(dx::Operand::TYPE_INPUT, 0, lowest_set_index(m));
		for (int j = 0; j < 4; j++, y++) {
			y->flags	= *regs != *pregs	? Entry::CHANGED	: 0;
			*pregs++	= *regs++;
		}
	}

	for (uint64 m = sim.OutputMask(); m; m = clear_lowest(m)) {
		const uint32	*regs	= (const uint32*)sim.GetReg(dx::Operand::TYPE_OUTPUT, 0, lowest_set_index(m));
		for (int j = 0; j < 4; j++, y++) {
			y->flags	= *regs != *pregs	? Entry::CHANGED	: 0;
			*pregs++	= *regs++;
		}
	}

	RedrawWindow(*this, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

class DX12ShaderDebuggerWindow : public MultiSplitterWindow {
	Accelerator				accel;
	TabControl3				*tabs;
	DX12RegisterWindow		*regs;
	DX12StateControl		tree;
	DebugWindow				code_window;
	ParsedSPDB				spdb;
	auto_ptr<Disassembler::State>	dis;
	dynamic_array<int>		bp;
	uint64					pc;
	uint32					step_count;

	void	ToggleBreakpoint(int y) {
		auto	i = lower_boundc(bp, y);
		if (i == bp.end() || *i != y)
			bp.insert(i, y);
		else
			bp.erase(i);
		code_window.Invalidate(code_window.Margin());
	}
public:
	dx::SimulatorDXBC		sim;

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	void	Update() {
		code_window.SetPC(code_window.AddressToLine(pc));
		regs->Update(sim, spdb, pc);
	}
	DX12ShaderDebuggerWindow(const WindowPos &wpos, const char *title, ShaderState &shader, DX12Connection *con, int _showsource);
};

DX12ShaderDebuggerWindow::DX12ShaderDebuggerWindow(const WindowPos &wpos, const char *title, ShaderState &shader, DX12Connection *con, int _showsource)
	: MultiSplitterWindow(3, SWF_VERT | SWF_PROP | SWF_DELETE_ON_DESTROY)
	, sim(shader.rec->GetUCode(), shader.rec->GetUCodeAddr(), 1)
	, spdb(shader.DXBC()->GetBlob<SPDB>())
	, accel( DebugAccelerator())
	, tree(con), code_window(bp, HLSLcolourer(GetSettings("PSSL")), _showsource)
{
	Create(wpos, title, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, WS_EX_CLIENTEDGE | WS_EX_COMPOSITED);

	ISO_browser2	settings	= GetSettings("PSSL");
	auto			colourer	= HLSLcolourer(settings);

	dis = DisassembleShader(shader.rec->GetUCode(), shader.rec->GetUCodeAddr());

	SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_FROM2ND | SplitterWindow::SWF_DELETE_ON_DESTROY);
	split2->Create(GetPanePos(0), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0);

	code_window.Create(split2->_GetPanePos(0), NULL, ES_READONLY | ES_SELECTIONBAR, 0);
	code_window.Init(dis, &spdb);

	tree.Create(split2->_GetPanePos(0), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT, 0);
	tree.AddShader(TVI_ROOT, shader);

	split2->SetPanes(code_window, tree, 100);
	tree.Show();
	SetPane(0, *split2);

	// source tabs
	if (!spdb.files.empty()) {
		tabs = new TabControl3(GetPanePos(1), "source", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0);
		tabs->SetFont(win::Font::DefaultGui());
		win::Font		font		= win::Font::Params(settings["Font"].GetString());
		for (auto &i : spdb.files.with_keys()) {
			EditControl c = MakeSourceWindow(tabs->GetChildWindowPos(), filename(i.val.name).name_ext(), colourer, font, i.val.source, 0, 0, ES_READONLY, 0);
			c.SetID(i.key);
			tabs->AddItemControl(c);
		}
		SetPane(1, *tabs);
		ShowSourceTabLine(*tabs, code_window.LineToSource(0));
		tabs->ShowSelectedControl();
	} else {
		SetPaneSize(1, 0);
		tabs = 0;
	}

	regs = new DX12RegisterWindow(GetPanePos(2));
	regs->Init(sim, spdb);
	SetPane(2, *regs);

	auto	*op = sim.Begin();
	while (op->IsDeclaration())
		op = op->next();

	pc			= sim.Address(op);
	step_count	= 0;

	code_window.Show();
	Rebind(this);
}

LRESULT DX12ShaderDebuggerWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			SetAccelerator(*this, accel);
			return 0;

		case WM_PARENTNOTIFY:
			switch (LOWORD(wParam)) {
				case WM_LBUTTONDOWN:
					SetAccelerator(*this, accel);
					break;
			}
			return 0;

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_DEBUG_RUN:
					return 0;

				case ID_DEBUG_STEPOVER:
					if (pc) {
						++step_count;
						auto	*op = sim.AddressToOp(pc);
						op	= sim.ProcessOp(op);
						pc	= sim.Address(op);
						Update();
					}
					return 0;

				case ID_DEBUG_STEPBACK:
					if (step_count) {
						sim.Reset();
						auto	*op = sim.Run(--step_count);
						pc			= sim.Address(op);
						Update();
					}
					return 0;

				case ID_DEBUG_BREAKPOINT: {
					int		line	= code_window.GetLine(code_window.GetSelection().cpMin);
					line = code_window.OffsetToLine(code_window.LineToOffset(line));
					ToggleBreakpoint(line);
					return 0;
				}
				default:
					if (HIWORD(wParam) == EN_SETFOCUS) {
						SetAccelerator(*this, accel);
						EditControl	c((HWND)lParam);
						int	line = c.GetLine(c.GetSelection().cpMin);
						if (int id = LOWORD(wParam)) {
							code_window.ShowCode(id, line);
						} else {
							ShowSourceTabLine(*tabs, code_window.LineToSource(line));
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
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh, *this);
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
//	DX12TraceWindow
//-----------------------------------------------------------------------------

class DX12TraceWindow : public Window<DX12TraceWindow> {
	ListViewControl			c;
	union row_data {
		struct field_data {
			uint16	reg:9, write:1;
		} fields[8];
		struct {
			uint64	:10, exec:1;
		};
	};
	dynamic_array<row_data>	rows;
	int						selected;

	struct RegAdder : buffer_accum<256>, ListViewControl::Item {
		ListViewControl			c;
		row_data				&row;
		dx::SimulatorDXBC		&sim;
		int						thread;
		uint8					types[2];
		bool					source;

		void	SetAddress(uint64 addr) {
			format("%010I64x", addr);
			Insert(c);
		}
		void	SetDis(const char *dis) {
			reset() << dis;
			*getp() = 0;
			Column(1).Set(c);
		}

		void	PutAt(int n, int reg) {
			*this << '\0';
			row.fields[n].reg	= reg;
			row.fields[n].write	= !source;
			Column(n + 2).Set(c);
		}

		void	AddValue(const dx::ASMOperand &o) {
			void	*p = sim.GetValue(thread, o);
			reset() << *(uint32*)p;
		}


		RegAdder(ListViewControl _c, dx::SimulatorDXBC &_sim, int _thread, row_data &_row) : ListViewControl::Item(getp()), c(_c), sim(_sim), thread(_thread), row(_row) {
			row.exec = sim.IsActive(thread);
		}
	};

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				return 0;

			case WM_SIZE:
				c.Resize(Point(lParam));
				break;

			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case NM_CLICK: {
						c.SetFocus();
						NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
						int	col, item = c.HitTest(nmlv->ptAction, col);
						if (item >= 0 && col >= 2) {
							selected = rows[item].fields[col - 2].reg;
							c.Invalidate();
						}
						break;
					}

					case NM_CUSTOMDRAW: {
						NMCUSTOMDRAW	*nmcd	= (NMCUSTOMDRAW*)nmh;

						switch (nmcd->dwDrawStage) {
							case CDDS_PREPAINT:
								return CDRF_NOTIFYITEMDRAW;

							case CDDS_ITEMPREPAINT: {
								NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmh;
								if (!(rows[nmcd->dwItemSpec].exec & 1))
									nmlvcd->clrText = RGB(128,128,128);
								return CDRF_NOTIFYSUBITEMDRAW;
							}
							case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
								NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmh;
								if (nmlvcd->iSubItem >= 2) {
									row_data::field_data	&f = rows[nmcd->dwItemSpec].fields[nmlvcd->iSubItem - 2];
									int		sel			= f.reg == selected ? 128 : 240;
									nmlvcd->clrTextBk	= f.write ? RGB(255,sel,sel) : RGB(sel,255,sel);
								} else {
									nmlvcd->clrTextBk	= nmlvcd->iSubItem == 1 ? RGB(240,240,255) : RGB(255,255,255);
								}
								break;
							}
						}
						break;
					}
				}
				return Parent()(message, wParam, lParam);
			}
			case WM_NCDESTROY:
				delete this;
			case WM_DESTROY:
				return 0;
		}
		return Super(message, wParam, lParam);
	}

	DX12TraceWindow(const WindowPos &wpos, dx::SimulatorDXBC &sim, int thread, int max_steps = 0);
};

DX12TraceWindow::DX12TraceWindow(const WindowPos &wpos, dx::SimulatorDXBC &sim, int thread, int max_steps) {
	static Disassembler	*dis = Disassembler::Find("DXBC");

	Create(wpos, 0, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, WS_EX_CLIENTEDGE);
	c.Create(GetChildWindowPos(), 0, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0);
	c.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);// | LVS_EX_FULLROWSELECT);
	c.SetFont(win::Font("Courier New", 11));

	int	nc = 0;
	ListViewControl::Column("address").		Width(100).Insert(c, nc++);
	ListViewControl::Column("instruction").	Width(400).Insert(c, nc++);
	ListViewControl::Column("dest").		Width(120).Insert(c, nc++);
	ListViewControl::Column("input 0").		Width(120).Insert(c, nc++);
	ListViewControl::Column("input 1").		Width(120).Insert(c, nc++);
	ListViewControl::Column("input 2").		Width(120).Insert(c, nc++);

	Disassembler::SymbolList	symbols;

	auto *p = sim.Begin();
	while(p->IsDeclaration())
		p = p->next();

	while (p && max_steps--) {
		RegAdder	ra(c, sim, thread, *rows._expand());
		ra.SetAddress(sim.Address(p));
		
		Disassembler::State	*state = dis->Disassemble(memory_block(unconst(p), p->Length * 4), sim.Address(p), symbols);
		buffer_accum<256>	ba;
		state->GetLine(ba, 0);
		ra.SetDis(ba + 34);
		delete state;

		dx::ASMOperation	op(p);
		for (int i = 0; i < op.ops.size(); i++)
			ra.AddValue(op.ops[i]);

		p	= sim.ProcessOp(p);
	}

	selected	= 0x100;
}

//-----------------------------------------------------------------------------
//	DX12Window	(StackWindow that keeps DX12Connection)
//-----------------------------------------------------------------------------

class DX12Window : public StackWindow {
protected:
	DX12Connection	*con;
public:
	static DX12Window*	Cast(Control c)	{ return (DX12Window*)StackWindow::Cast(c); }
	DX12Window(const WindowPos &wpos, const char *title, DX12Connection *_con, ID id = ID()) : StackWindow(wpos, title, id), con(_con) {}

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_NOTIFY:
				return Parent()(message, wParam, lParam);
		}
		return StackWindow::Proc(message, wParam, lParam);
	}

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
	Control ShowBitmap(ISO_ptr<void> p, bool new_tab) {
		Control	c;
		if (p) {
			c = BitmapWindow(GetChildWindowPos(), p, tag(p.ID()), true);
			AddView(c, new_tab);
		}
		return c;
	}
};

//-----------------------------------------------------------------------------
//	DX12BatchWindow
//-----------------------------------------------------------------------------

class DX12BatchWindow : public DX12Window {
public:
	enum {ID = 'BA'};
	DX12StateControl				tree;
	ShaderState						shaders[5];
	dynamic_array<VertexBufferView>	verts;
	dynamic_array<dx::SimulatorDXBC::TypedBuffer>	vbv;
	indices							ix;
	Topology						topology;
	BackFaceCull					culling;
	float3x2						viewport;
	Point							screen_size;

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	DX12BatchWindow(const WindowPos &wpos, const char *title, DX12Connection *con, const DX12State &state);
	Control MakeShaderOutput(SHADERSTAGE stage);
	Control MakeShaderOutput2();
	void	VertexMenu(SHADERSTAGE stage, int i, ListViewControl lv);
};

const C_type *GetVertexType(const D3D12_INPUT_LAYOUT_DESC &layout, int slot) {
	C_type_struct	comp;
	uint32			offset = 0;
	for (const D3D12_INPUT_ELEMENT_DESC *i = layout.pInputElementDescs, *e = i + layout.NumElements; i != e; i++) {
		if (i->InputSlot == slot) {
			if (i->AlignedByteOffset != D3D12_APPEND_ALIGNED_ELEMENT)
				offset = i->AlignedByteOffset;
			comp.C_type_composite::add(i->SemanticName, dxgi_c_types[i->Format], offset);
			offset += uint32(dxgi_c_types[i->Format]->size());
		}
	}
	return DX11_ctypes.add(comp);
}

const C_type *GetVertexType(const D3D12_COMMAND_SIGNATURE_DESC &desc) {
	C_type_struct	comp;
	auto	uint32_type	= DX11_ctypes.get_type<uint32>();
	auto	uint64_type	= DX11_ctypes.get_type<uint64>();

	for (auto *i = desc.pArgumentDescs, *e = i + desc.NumArgumentDescs; i != e; ++i) {
		switch (i->Type) {
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
	return DX11_ctypes.add(comp);
}

const D3D12_INPUT_ELEMENT_DESC *MatchBySemantic(D3D12_INPUT_LAYOUT_DESC &layout, const char *name, int index) {
	for (const D3D12_INPUT_ELEMENT_DESC *d = layout.pInputElementDescs, *de = d + layout.NumElements; d != de; ++d) {
		if (istr(name) == d->SemanticName && index == d->SemanticIndex)
			return d;
	}
	return 0;
}

DX12BatchWindow::DX12BatchWindow(const WindowPos &wpos, const char *title, DX12Connection *con, const DX12State &state) : DX12Window(wpos, title, con, ID), tree(con) {
	Rebind(this);
	clear(screen_size);

	HTREEITEM	h;

	if (state.graphics) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC*		graphics_state = state.pipeline;

		topology	= state.GetTopology();
		culling		= state.CalcCull(true);
		ix			= state.GetIndexing(con->cache);

		if (auto *d = con->FindDescriptor(state.targets[0])) {
			if (auto *obj = con->FindObject(uint64(d->res))) {
				RecResource *rr = obj->info;
				screen_size = Point(rr->Width, rr->Height);
			}
		}

		auto	vp	= state.GetViewport(0);
		float3	vps(vp.Width / 2, vp.Height / 2, vp.MaxDepth - vp.MinDepth);
		float3	vpo(vp.TopLeftX + vp.Width / 2, vp.TopLeftY + vp.Height / 2, vp.MinDepth);
		viewport	= float3x2(
			vps,
			vpo
		);

		if (state.vbv && graphics_state->InputLayout.pInputElementDescs) {
			SplitterWindow	*split2 = new SplitterWindow(SplitterWindow::SWF_VERT | SplitterWindow::SWF_DELETE_ON_DESTROY);
			split2->Create(GetChildWindowPos(), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0);
			split2->SetClientPos(400);

			split2->SetPane(0, tree.Create(split2->_GetPanePos(0), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT, 0));
			MeshWindow	*mv = MakeMeshView(split2->_GetPanePos(1), con->cache, state, state.vbv[0], graphics_state->InputLayout.pInputElementDescs[0].Format);
			split2->SetPane(1, *mv);
			split2->GetPane(1).Show();
		} else {
			tree.Create(GetChildWindowPos(), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT, 0);
		}

		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		rt.AddFields(TreeControl::Item("Pipeline State").Bold().Expand().Insert(tree), graphics_state);

		h	= TreeControl::Item("Descriptor Heaps").Bold().Expand().Insert(tree);
		if (state.cbv_srv_uav_descriptor_heap)
			rt.AddFields(rt.AddText(h, "cbv_srv_uav"), state.cbv_srv_uav_descriptor_heap);

		if (state.sampler_descriptor_heap)
			rt.AddFields(rt.AddText(h, "sampler"), state.sampler_descriptor_heap);

		h	= TreeControl::Item("Targets").Bold().Expand().Insert(tree);
		for (uint32 i = 0, n = graphics_state->NumRenderTargets; i < n; i++) {
			buffer_accum<256>	ba;
			ba << "Target " << i << ' ';
			HTREEITEM	h2 = TreeControl::Item(ba).Image(tree.ST_TARGET_COLOUR).StateImage(i).Param(i).Insert(tree, h);
			if (DESCRIPTOR *d = con->FindDescriptor(state.targets[i]))
				rt.AddFields(rt.AddText(h2, "Descriptor"), d);
			rt.AddFields(rt.AddText(h2, "Blending"), &graphics_state->BlendState.RenderTarget[i]);
		}

		HTREEITEM	h2	= TreeControl::Item("Depth Buffer").Image(tree.ST_TARGET_DEPTH).Insert(tree, h);
		if (DESCRIPTOR *d = con->FindDescriptor(state.depth))
			rt.AddFields(rt.AddText(h2, "Descriptor"), d);
		rt.AddFields(rt.AddText(h2, "DepthStencilState"), &graphics_state->DepthStencilState);

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
			SHADERSTAGE	stage = SHADERSTAGE(i);
			const D3D12_SHADER_BYTECODE	&shader = (&graphics_state->VS)[stage];
			if (shader.pShaderBytecode) {
				auto	rec = con->FindShader((uint64)shader.pShaderBytecode);
				shaders[i].init(*con, con->cache, rec, &state);
				h2 = tree.AddShader(
					TreeControl::Item(shader_names[i]).Image(tree.ST_SHADER).Param(i).Insert(tree, h),
					shaders[i]
				);
				if (stage == VS) {
					HTREEITEM	h3 = rt.Add(h2, "Inputs", tree.ST_VERTICES, stage);
					if (state.mode == RecCommandList::tag_DrawIndexedInstanced)
						rt.AddFields(rt.AddText(h3, "IndexBuffer"), &state.ibv);

					int	x = 0;
					for (auto &i : state.vbv) {
						new(verts) VertexBufferView(i, GetVertexType(graphics_state->InputLayout, x));
						rt.AddFields(TreeControl::Item(format_string("VertexBuffer %i", x)).Image(tree.ST_VERTICES).StateImage(x + 1).Insert(tree, h3), &i);
						++x;
					}

					uint32	offset	= 0;
					auto	isgn	= shaders[VS].DXBC()->GetBlob<dx::ISGN>();
					for (auto &i : isgn->Elements()) {
						auto	&r = vbv.set(i.register_num);

						if (const D3D12_INPUT_ELEMENT_DESC *desc = MatchBySemantic(graphics_state->InputLayout, i.name.get(isgn), i.semantic_index)) {
							D3D12_VERTEX_BUFFER_VIEW		view = state.vbv[desc->InputSlot];
							if (desc->AlignedByteOffset != D3D12_APPEND_ALIGNED_ELEMENT)
								offset = desc->AlignedByteOffset;

							r.init(con->cache(view.BufferLocation + offset, view.SizeInBytes - offset), view.StrideInBytes, desc->Format);
							offset += uint32(DXGI::size(desc->Format));
						}
					}
				}
				rt.Add(h2, "Outputs", tree.ST_OUTPUTS, stage);
			}
		}


	} else {
		D3D12_COMPUTE_PIPELINE_STATE_DESC*		compute_state = state.pipeline;

		tree.Create(GetChildWindowPos(), 0, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT, 0);
		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);

		rt.AddFields(TreeControl::Item("Pipeline State").Bold().Expand().Insert(tree), compute_state);

		h	= TreeControl::Item("Descriptor Heaps").Bold().Expand().Insert(tree);
		if (state.cbv_srv_uav_descriptor_heap)
			rt.AddFields(rt.AddText(h, "cbv_srv_uav"), state.cbv_srv_uav_descriptor_heap);

		if (state.sampler_descriptor_heap)
			rt.AddFields(rt.AddText(h, "sampler"), state.sampler_descriptor_heap);

		if (compute_state->CS.pShaderBytecode) {
			auto	rec = con->FindShader((uint64)compute_state->CS.pShaderBytecode);
			shaders[0].init(*con, con->cache, rec, &state);
			tree.AddShader(
				TreeControl::Item("Compute Shader").Image(tree.ST_SHADER).Param(0).Insert(tree, TVI_ROOT),
				shaders[0]
			);
		}
	}

	if (state.mode == RecCommandList::tag_ExecuteIndirect) {
		RegisterTree	rt(tree, &tree, IDFMT_FOLLOWPTR);
		h	= TreeControl::Item("Indirect").Bold().Expand().Insert(tree);
		malloc_block	m	= rmap_struct<RTM, D3D12_COMMAND_SIGNATURE_DESC>(state.indirect.signature->info);
		D3D12_COMMAND_SIGNATURE_DESC	*desc	= m;

		rt.AddFields(rt.AddText(h, "Command Signature"), (T_map<RTM, D3D12_COMMAND_SIGNATURE_DESC>::type*)state.indirect.signature->info);

		if (state.indirect.arguments) {
			RecResource	*rr		= state.indirect.arguments->info;
			D3D12_VERTEX_BUFFER_VIEW v;
			v.StrideInBytes		= desc->ByteStride;
			v.BufferLocation	= rr->gpu + state.indirect.arguments_offset;
			v.SizeInBytes		= state.indirect.command_count * v.StrideInBytes;
			const C_type *type	= GetVertexType(*desc);

			new(verts) VertexBufferView(v, type);
			rt.AddFields(TreeControl::Item("Arguments").Image(tree.ST_VERTICES).StateImage(verts.size32()).Insert(tree, h), (RecResource*)state.indirect.arguments->info);
		}

		if (state.indirect.counts) {
			RecResource	*rr		= state.indirect.counts->info;
			D3D12_VERTEX_BUFFER_VIEW v;
			v.StrideInBytes		= 4;
			v.BufferLocation	= rr->gpu + state.indirect.counts_offset;
			v.SizeInBytes		= rr->Width - state.indirect.counts_offset;
			const C_type *type	= DX11_ctypes.get_type<uint32>();

			new(verts) VertexBufferView(v, type);
			rt.AddFields(TreeControl::Item("Counts").Image(tree.ST_VERTICES).StateImage(verts.size32()).Insert(tree, h), (RecResource*)state.indirect.arguments->info);
		}
	}
}

Control MakeObjectView(const WindowPos &wpos, DX12Connection *_con, DX12Assets::ObjectRecord *rec);

LRESULT DX12BatchWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TVN_SELCHANGED: {
					NMTREEVIEW			*nmtv	= (NMTREEVIEW*)nmh;
					TreeControl			t		= nmh->hwndFrom;
					TreeControl::Item	i		= t.GetItem(nmtv->itemNew.hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE);
					if (t == tree) {
						bool	ctrl	= !!(GetKeyState(VK_CONTROL) & 0x8000);
						bool	new_tab	= !!(GetKeyState(VK_SHIFT) & 0x8000);

						switch (i.Image()) {
							case DX12StateControl::ST_TARGET_COLOUR: {
								break;
							}

							case DX12StateControl::ST_OBJECT:
								AddView(MakeObjectView(GetChildWindowPos(), con, i.Param()), new_tab);
								break;

							case DX12StateControl::ST_SHADER: {
								AddView(MakeShaderViewer(GetChildWindowPos(), con, shaders[(int)i.Param()]), new_tab);
								break;
							}
							case DX12StateControl::ST_VERTICES:
								if (int x = i.StateImage()) {
									AddView(*new DX12BufferWindow(GetChildWindowPos(), "Vertices", con->cache, verts[x - 1]), new_tab);

								} else {
									MeshVertexWindow		*m	= new MeshVertexWindow(GetChildWindowPos(), "Vertices");
									DX12VertexInputWindow	*c	= new DX12VertexInputWindow(GetChildWindowPos(), "Vertices", con->cache, verts, 0, 0, verts.size32(), ix);
									MeshWindow	*mw	= MakeMeshView(m->GetPanePos(1),
										topology,
										vbv[0], vbv[0].stride, vbv[0].format,
										ix,
										identity, culling, MeshWindow::PERSPECTIVE
									);
									m->SetPanes(*c, *mw, 50);
									c->Show();
									mw->Show();
									AddView(*m, new_tab);
								}
								break;

							case DX12StateControl::ST_OUTPUTS:
								if (ctrl)
									AddView(MakeShaderOutput2(), new_tab);
								else
									AddView(MakeShaderOutput(i.Param()), new_tab);
								break;
						}
					}
					break;
				}

				case NM_RCLICK:
					switch (wParam) {
						case 'VO': VertexMenu(VS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'PO': VertexMenu(PS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'DO': VertexMenu(DS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'HO': VertexMenu(HS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'GO': VertexMenu(GS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
						case 'CO': VertexMenu(CS, ((NMITEMACTIVATE*)nmh)->iItem, nmh->hwndFrom); return 0;
					}
					break;

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree)
						return tree.CustomDraw((NMCUSTOMDRAW*)nmh, *this);
					break;
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return DX12Window::Proc(message, wParam, lParam);
}

int SetShaderColumns(ColVertexWindow *c, dx::SIGT<dx::SIG::Element> *elements, int nc) {
	int		x		= 0;
	for (auto &i : elements->Elements()) {
		auto	col		= c->MakeColour(x++);
		for (int m = i.mask; m; m = clear_lowest(m))
			nc	= c->AddColumn(nc, string(i.name.get(elements)) << '.' << "xyzw"[lowest_set_index(m)], 50, col);
	}
	return nc;
}

void FillShaderIndex(ColVertexWindow *c, int num) {
	for (int i = 0; i < num; i++) {
		char				text[64];
		VertexWindow::Item	item(text);
		fixed_accum(text) << i;
		item.Insert(c->vw);
	}
}

int FillShaderColumn(ColVertexWindow *c, int col, dx::SimulatorDXBC::RegFile &rf, uint8 mask, dynamic_bitarray<uint32> *enabled = 0) {
	int	t = 0;
	for (auto &r : rf) {
		char				text[64] = "-";
		VertexWindow::Item	item(text);
		item.Index(t).Column(col - 1);

		bool	en	= !enabled || (*enabled)[t];
		float	*o = (float*)r;

		for (int m = mask; m; m = clear_lowest(m)) {
			int	i = lowest_set_index(m);
			if (en)
				fixed_accum(text) << o[i];
			item.NextColumn().Set(c->vw);
		}
		t++;
	}
	return col + count_bits(mask);
}

void FillShaderValues(ColVertexWindow *c, dx::SIGT<dx::SIG::Element> *elements, dx::SimulatorDXBC &sim, int col, dynamic_bitarray<uint32> *enabled = 0) {
	auto	type = col > 1 ? dx::Operand::TYPE_OUTPUT : dx::Operand::TYPE_INPUT;
	for (auto &i : elements->Elements())
		col = FillShaderColumn(c, col, sim.GetRegFile(type, i.register_num), i.mask, enabled);
}

Control DX12BatchWindow::MakeShaderOutput(SHADERSTAGE stage) {
	auto	&shader = shaders[stage == CS ? 0 : stage];
	int		num		= 64;

	switch (stage) {
		case PS:	num	= triangle_number(from_triangle_number(num / 4)) * 4; break;
		case VS:	num	= min(ix.num, num); break;
	}

	dx::SimulatorDXBC	sim(shader.rec->GetUCode(), shader.rec->GetUCodeAddr(), num);
	shader.InitSimulator(*con, con->cache, sim);

	switch (stage) {
		case PS: {
			dx::SimulatorDXBC	sim2(shaders[VS].rec->GetUCode(), shaders[VS].rec->GetUCodeAddr(), 3);
			shaders[VS].InitSimulator(*con, con->cache, sim2);
			sim2.SetVertexInputs(vbv, vbv.size32(), (uint16*)ix.p);
			sim2.Run();

			auto	 *ps_in		= shader.DXBC()->GetBlob<dx::ISGN>();
			auto	 *vs_out	= shaders[VS].DXBC()->GetBlob<dx::OSGN>();

			for (auto &in : ps_in->Elements()) {
				if (auto *x = vs_out->find_by_semantic(in.name.get(ps_in), in.semantic_index))
					sim.RasteriseInput(in.register_num, sim2.GetOutput(x->register_num));
			}
			break;
		}
		case VS:
			sim.SetVertexInputs(vbv, vbv.size32(), (uint16*)ix.p);
			break;
	}

	ColVertexWindow	*c	= new ColVertexWindow(GetChildWindowPos(), "Shader Output", ("VPDHGC"[stage] << 8) + 'O');
	int		nc	= c->AddColumn(0, "#", 50, RGB(255,255,255));

	dx::DXBC *dxbc	= shader.DXBC();
	auto	 *in	= dxbc->GetBlob<dx::ISGN>();
	auto	 *out	= dxbc->GetBlob<dx::OSGN>();

	if (in)
		nc = SetShaderColumns(c, in, nc);
	int		out_col = nc;
	if (out)
		nc = SetShaderColumns(c, out, nc);

	int	x = 0;
	for (auto &i : sim.uav) {
		nc = MakeHeaders(c->vw, nc, i.format);
		c->AddColour(nc, c->MakeColour(x++));
	}

	FillShaderIndex(c, num);
	FillShaderValues(c, in, sim, 1);
	sim.Run();

	dynamic_bitarray<uint32>	enabled(num);
	for (int i = 0; i < num; i++)
		enabled[i] = !sim.IsDiscarded(i);

	FillShaderValues(c, out, sim, out_col, &enabled);

	c->vw.SetCount(num);
	c->vw.Show();

	return *c;
}

Control DX12BatchWindow::MakeShaderOutput2() {
	auto	&shader = shaders[VS];
	uint32				num	= ix.num;
	dx::SimulatorDXBC	sim(shader.rec->GetUCode(), shader.rec->GetUCodeAddr(), ix.num);
	shader.InitSimulator(*con, con->cache, sim);

	sim.SetVertexInputs(vbv, vbv.size32(), (uint16*)ix.p);

	MeshVertexWindow	*m	= new MeshVertexWindow(GetChildWindowPos(), "Shader Output");
	ColVertexWindow		*c	= new ColVertexWindow(m->_GetPanePos(0), "Shader Output", 'VO');
	int		nc	= c->AddColumn(0, "#", 50, RGB(255,255,255));

	dx::DXBC *dxbc	= shader.DXBC();
	auto	 *in	= dxbc->GetBlob<dx::ISGN>();
	auto	 *out	= dxbc->GetBlob<dx::OSGN>();

	if (in)
		nc = SetShaderColumns(c, in, nc);
	int		out_col = nc;
	if (out)
		nc = SetShaderColumns(c, out, nc);

	FillShaderIndex(c, num);
	FillShaderValues(c, in, sim, 1);
	sim.Run();
	FillShaderValues(c, out, sim, out_col);

	c->vw.SetCount(num);
	c->vw.Show();

	auto	*output_verts	= new float4p[num], *p = output_verts;
	for (auto &i : sim.GetOutput(0))
		*p++ = (float4p&)i;

	MeshWindow	*mw	= MakeMeshView(m->GetPanePos(1),
		topology,
		output_verts, sizeof(float[4]), DXGI_FORMAT_R32G32B32A32_FLOAT,
		indices(num),
		viewport, culling, MeshWindow::SCREEN_PERSP
	);
	delete[] output_verts;

	mw->SetScreenSize(screen_size);
	mw->flags.set(MeshWindow::FRUSTUM_EDGES);

	m->SetPanes(*c, *mw, 50);
	c->Show();
	mw->Show();
	return *m;
}

void DX12BatchWindow::VertexMenu(SHADERSTAGE stage, int i, ListViewControl lv) {
	auto	&shader = shaders[stage];
	Menu	menu	= Menu::Popup();

	menu.Append("Debug", 1);
	if (stage == VS && ix) {
		menu.Separator();
		menu.Append(format_string("Next use of index %i", ix[i]), 2);
		menu.Append(format_string("Highlight all uses of index %i", ix[i]), 3);
	}

	menu.Append("Show Simulated Trace", 4);

	switch (menu.Track(*this, GetMousePos(), TPM_NONOTIFY | TPM_RETURNCMD)) {
		case 1: {
			auto	*debugger = new DX12ShaderDebuggerWindow(GetChildWindowPos(), "Debugger", shader, con, GetSettings("General/shader source").GetInt(1));
			shader.InitSimulator(*con, con->cache, debugger->sim);
			switch (stage) {
				case VS:
					debugger->sim.SetVertexInputs(vbv, vbv.size32(), (uint16*)ix.p, i);
					break;
				case PS: {
					dx::SimulatorDXBC	sim2(shaders[VS].rec->GetUCode(), shaders[VS].rec->GetUCodeAddr(), 3);
					shaders[VS].InitSimulator(*con, con->cache, sim2);
					sim2.SetVertexInputs(vbv, vbv.size32(), (uint16*)ix.p);
					sim2.Run();

					auto	 *ps_in		= shader.DXBC()->GetBlob<dx::ISGN>();
					auto	 *vs_out	= shaders[VS].DXBC()->GetBlob<dx::OSGN>();

					int		n			= from_triangle_number(16);
					float	u			= (triangle_col(n, i / 4) * 2 + (i & 1)) / float(n * 2 - 1);
					float	v			= (triangle_row(n, i / 4) * 2 + (i / 2 & 1)) / float(n * 2 - 1);

					for (auto &in : ps_in->Elements()) {
						if (auto *x = vs_out->find_by_semantic(in.name.get(ps_in), in.semantic_index))
							debugger->sim.InterpolateInput(0, in.register_num, sim2.GetOutput(x->register_num), u, v);
					}
					break;
				}
			}
			debugger->Update();
			PushView(*debugger);
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
			uint32				num	= ix.num;
			dx::SimulatorDXBC	sim(shader.rec->GetUCode(), shader.rec->GetUCodeAddr(), ix.num);
			shader.InitSimulator(*con, con->cache, sim);

			switch (stage) {
				case VS:
					sim.SetVertexInputs(vbv, vbv.size32(), (uint16*)ix.p);
					break;
			}

			PushView(*new DX12TraceWindow(GetChildWindowPos(), sim, i, 1000));
			break;
		}

	}
	menu.Destroy();
}


//-----------------------------------------------------------------------------
//	ViewDX12GPU
//-----------------------------------------------------------------------------

class ViewDX12GPU :  public SplitterWindow, public DX12Connection {
public:
	enum {
		WT_NONE,
		WT_BATCH,
		WT_MARKER,
		WT_CALLSTACK,
	};
	static IDFMT		init_format;
private:
	static RefControl<ToolBarControl>	toolbar_gpu;
	TreeControl			tree;
	win::Font			italics;

	TabControl2			tabs;
	SplitterWindow		*root;
	HTREEITEM			context_item;
	IDFMT				format;
	CallStackDumper		stack_dumper;

	void		TreeSelection(HTREEITEM hItem);
	void		TreeDoubleClick(HTREEITEM hItem);

public:
	static ViewDX12GPU*	Cast(Control c)	{
		return (ViewDX12GPU*)SplitterWindow::Cast(c);
	}

	void LoadCapture(const struct DX12Capture *cap);
	bool SaveCapture(const filename &fn);
	
	void AddTab(Control c, bool new_tab) {
		if (!new_tab && !!c.id) {
			int			i;
			if (TabWindow *t = FindTab(GetPane(1), c.id, i)) {
				t->SetItemControl(c, i);
				t->SetSelectedControl(c);
				return;
			}
		}
		tabs.AddItemControl(c);
		tabs.SetSelectedControl(c);
	}
	Control GetChildParent() const {
		return tabs;
	}

	WindowPos GetChildWindowPos() const {
		return tabs.GetChildWindowPos();
	}

	bool SelectTab(ID id) {
		int			i;
		if (TabWindow *t = FindTab(GetPane(1), id, i)) {
			t->SetSelectedControl(t->GetItemControl(i));
			return true;
		}
		return false;
	}
			
	void SetOffset(uint32 offset) {
		HTREEITEM h = TVI_ROOT;
		if (offset >= device_object->info.length32()) {
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
		SetOffset(batches[batch].addr);
	}
	void SelectBatch(BatchList &b, bool always_list = false) {
		int batch = ::SelectBatch(*this, GetMousePos(), b, always_list);
		if (batch >= 0)
			SelectBatch(batch);
	}

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	void	MakeTree();
	void	ExpandTree(HTREEITEM h);

	ViewDX12GPU(app::MainWindow &_main, const WindowPos &pos, HANDLE process, const char *proc_path);
	ViewDX12GPU(app::MainWindow &_main, const WindowPos &pos);
	~ViewDX12GPU();
};

RefControl<ToolBarControl>	ViewDX12GPU::toolbar_gpu;

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

field fields<DESCRIPTOR>::f[] = {
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
	TreeControl		tree(wpos, title, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE);
	RegisterTree	rt(tree, con, IDFMT_FOLLOWPTR);
	HTREEITEM		h	= rt.AddText(TVI_ROOT, title);
	rt.AddFields(h, &desc);
	return tree;
}

void InitDescriptorHeapView(ListViewControl lv) {
	lv.SetView(ListViewControl::DETAIL_VIEW);
	lv.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
	ListViewControl::Column("index").Width(50).Insert(lv, 0);
	ListViewControl::Column("cpu handle").Width(100).Insert(lv, 1);
	ListViewControl::Column("gpu handle").Width(100).Insert(lv, 2);
	int	nc = MakeColumns(lv, fields<DESCRIPTOR>::f, IDFMT_CAMEL, 3);
	while (nc < 10)
		ListViewControl::Column("").Width(100).Insert(lv, nc++);
}

void FillDescriptorHeapView(RegisterList &&rl, const RecDescriptorHeap &heap, int begin, int end, int group = I_GROUPIDNONE) {
	for (int j = begin; j < end; j++) {
		const DESCRIPTOR	&d		= heap.descriptors[j];
		uint64				cpu		= heap.cpu.ptr + j * heap.stride;
		uint64				gpu		= heap.gpu.ptr + j * heap.stride;
		ListViewControl::Item		item;

		item.Text(to_string(j)).GroupId(group).Param(&d).Insert(rl.lv);
		rl.AddText(item.Column(1), buffer_accum<256>("0x") << hex(cpu));
		rl.AddText(item.Column(2), buffer_accum<256>("0x") << hex(gpu));

		ISO_ASSERT(d.type < DESCRIPTOR::_NUM);
		rl.FillRow(item.Column(3), fields<DESCRIPTOR>::f, (uint32*)&d);
	}
}

void FillDescriptorHeapView(ListViewControl lv, DX12Assets &assets, const RecDescriptorHeap &heap, int begin, int end, int group = I_GROUPIDNONE) {
	FillDescriptorHeapView(RegisterList(lv, ptr([&assets](string_accum &sa, uint64 v) {
		if (auto *rec = assets.FindObject(v))
			sa << rec->name;
		else
			sa << "0x" << hex(v);
	})), heap, begin, end, group);
}

void FillDescriptorHeapView(ListViewControl lv, DX12Assets &assets, const RecDescriptorHeap &heap) {
	FillDescriptorHeapView(lv, assets, heap, 0, heap.count);
}


struct DX12DescriptorHeapControl : ListViewControl {
	enum {ID = 'DH'};

	DX12DescriptorHeapControl(const WindowPos &wpos, DX12Assets &assets, const DX12Assets::ObjectRecord *rec) : ListViewControl(wpos, rec->GetName(), WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_VSCROLL, WS_EX_CLIENTEDGE, ID) {
		user = rec;
		InitDescriptorHeapView(*this);
		FillDescriptorHeapView(*this, assets, *rec->info);
	}

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			const DX12Assets::ObjectRecord	*rec	= user;
			const RecDescriptorHeap			*heap	= rec->info;
			uint64		h	= heap->cpu.ptr + heap->stride * row;

			switch (col) {
				case 0: {
					uint32	offset = main->FindDescriptorChange(h, heap->stride);
					main->SetOffset(offset);
					break;
				}
				default:
					main->AddTab(MakeDescriptorView(GetChildWindowPos(), to_string(hex(h)), main, heap->descriptors[row]), !!(flags & LVKF_SHIFT));
					break;
			}
		}
	}
};

struct DX12DescriptorList : Subclass<DX12DescriptorList, ListViewControl>, refs<DX12DescriptorList> {
	enum {ID = 'DL'};
	DX12Connection *con;

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				addref();
				break;
			case WM_NCDESTROY:
				release();
			case WM_DESTROY:
				return 0;
		}
		return Super(message, wParam, lParam);
	}

	DX12DescriptorList(const WindowPos &wpos, DX12Connection *_con) : con(_con) {
		Create(wpos, "Descriptors", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, ID);
		InitDescriptorHeapView(*this);
		EnableGroups();

		RunThread([this]{
			addref();

			for (auto &i : con->descriptor_heaps) {
				int	id = con->descriptor_heaps.index_of(i);
				buffer_accum<256>	a;
				a << i.obj->name << " : 0x" << hex(i->cpu.ptr) << " x " << i->count;
				Group(str16(a)).ID(id).Insert(*this);
				for (int j = 0; j < i->count; j += 64) {
					JobQueue::Main().add([this, i, j, id] {
						FillDescriptorHeapView(*this, *con, *i, j, min(j + 64, i->count), id);
					});
				}
			}

			release();
		});
	}

	void	LeftClick(Control from, int row, int col, const Point &pt, uint32 flags) {
		if (ViewDX12GPU *main = ViewDX12GPU::Cast(from)) {
			RecDescriptorHeap	*heap = 0;
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
				default:
					main->AddTab(MakeDescriptorView(GetChildWindowPos(), to_string(hex(h)), main, heap->descriptors[row]), !!(flags & LVKF_SHIFT));
					break;
			}
		}
	}
};

//-----------------------------------------------------------------------------
//	DX12ResourcesList
//-----------------------------------------------------------------------------

struct DX12ResourcesList : EditableEntryTable<DX12ResourcesList, DX12Assets::ResourceRecord> {
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
					auto		*r	= GetEntry(row);
					if (r->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
						//main->AddTab(*new DX12BufferWindow(GetChildWindowPos(), e->GetName(), con->cache, const VertexBufferView &_buff);
						memory_block	data = con->cache(uint64(r->data.start), r->data.size);
						main->AddTab(BinaryWindow(main->GetChildWindowPos(), MakeBrowser(data)), !!(flags & LVKF_SHIFT));

					} else if (ISO_ptr<bitmap> bm = con->GetBitmap(r)) {
						main->AddTab(BitmapWindow(main->GetChildWindowPos(), bm, r->GetName(), true), !!(flags & LVKF_SHIFT));
					}
					break;
				}
			}
		}
	}

	DX12ResourcesList(const WindowPos &wpos, DX12Connection *_con)
		: EditableEntryTable<DX12ResourcesList, DX12Assets::ResourceRecord>(_con->resources)
		, con(_con)
		, images(ImageList::Create(DeviceContext::ScreenCaps().LogPixels() * (2 / 3.f), ILC_COLOR32, 1, 1))
	{
		Create(wpos, "Resources", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, ID);
		Init();

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
				int		x	= GetThumbnail(*this, images, con, i);
				int		j	= i - table.begin();
				AddEntry(*i, j, x);
				GetItem(j).TileInfo(cols).Set(*this);
			}

			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	DX12ShadersList
//-----------------------------------------------------------------------------

struct DX12ShadersList : EditableEntryTable<DX12ShadersList, DX12Assets::ShaderRecord> {
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
					Busy(), main->AddTab(MakeShaderViewer(main->GetChildWindowPos(), con, GetEntry(row)), !!(flags & LVKF_SHIFT));
					break;
			}
		}
	}

	DX12ShadersList(const WindowPos &wpos, DX12Connection *_con)
		: EditableEntryTable<DX12ShadersList, DX12Assets::ShaderRecord>(_con->shaders)
		, con(_con)
	{
		Create(wpos, "Shaders", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, ID);
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

Control MakeObjectView(const WindowPos &wpos, DX12Connection *_con, DX12Assets::ObjectRecord *rec) {
	switch (rec->type) {
//		case RecObject::Heap:
		case RecObject::DescriptorHeap:
			return DX12DescriptorHeapControl(wpos, *_con, rec);
	}

	TreeControl		tree(wpos, rec->GetName(), WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE);
	RegisterTree	rt(tree, IDFMT_FOLLOWPTR);
	rt.cb = _con;

	HTREEITEM		h	= rt.AddText(TVI_ROOT, rec->GetName(), 0);
	switch (rec->type) {
		case RecObject::RootSignature: {
			com_ptr<ID3D12RootSignatureDeserializer>	ds;
			HRESULT		hr = D3D12CreateRootSignatureDeserializer(rec->info, rec->info.length(), ds.uuid(), (void**)&ds);
			const D3D12_ROOT_SIGNATURE_DESC *desc = ds->GetRootSignatureDesc();
			HTREEITEM	h2;

			h2 = rt.AddText(h, "Parameters");
			for (int i = 0; i < desc->NumParameters; i++) {
				HTREEITEM					h3		= rt.AddText(h2, format_string("[%i]", i));
				const D3D12_ROOT_PARAMETER	*param	= desc->pParameters + i;
				if (param->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
					rt.AddField(h3, &make_field("ParameterType", &D3D12_ROOT_PARAMETER::ParameterType), (uint32*)param);
					rt.AddArray(h3, param->DescriptorTable.pDescriptorRanges, param->DescriptorTable.NumDescriptorRanges);
					rt.AddField(h3, &make_field("ShaderVisibility", &D3D12_ROOT_PARAMETER::ShaderVisibility), (uint32*)param);
				} else {
					rt.AddFields(h3, param);
				}
			}

			h2 = rt.AddText(h, "Static Samplers");
			rt.AddArray(h2, desc->pStaticSamplers, desc->NumStaticSamplers);

			rt.AddField(h, &make_field("Flags", &D3D12_ROOT_SIGNATURE_DESC::Flags), (uint32*)desc);
			break;
		}

		case RecObject::Resource:
			rt.AddFields(h, (RecResource*)rec->info);
			break;

//		case RecObject::CommandAllocator:
//		case RecObject::Fence:

		case RecObject::PipelineState:
			if (rec->info.length() >= sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC))
				rt.AddFields(h, (D3D12_GRAPHICS_PIPELINE_STATE_DESC*)rec->info);
			else
				rt.AddFields(h, (D3D12_COMPUTE_PIPELINE_STATE_DESC*)rec->info);
			break;

//		case RecObject::QueryHeap:
		case RecObject::CommandSignature:
			rt.AddFields(h, (T_map<RTM, D3D12_COMMAND_SIGNATURE_DESC>::type*)rec->info);
//			rt.AddFields(h, (D3D12_COMMAND_SIGNATURE_DESC*)rmap_struct<RTM, D3D12_COMMAND_SIGNATURE_DESC>(rec->info));
			break;

//		case RecObject::GraphicsCommandList:

		case RecObject::CommandQueue:
			rt.AddFields(h, (D3D12_COMMAND_QUEUE_DESC*)rec->info);
			break;

//		case RecObject::Device:
			break;
		default:
			rt.AddFields(h, rec);
			break;
	}
	return tree;
}

struct DX12ObjectsList : EditableEntryTable<DX12ObjectsList, DX12Assets::ObjectRecord> {
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
					Busy(), main->AddTab(MakeObjectView(main->GetChildWindowPos(), con, GetEntry(row)), !!(flags & LVKF_SHIFT));
					break;
			}
		}
	}

	DX12ObjectsList(const WindowPos &wpos, DX12Connection *_con)
		: EditableEntryTable<DX12ObjectsList, DX12Assets::ObjectRecord>(_con->objects)
		, con(_con)
	{
		Create(wpos, "Objects", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, ID);
		Init();
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
				int	j = i - table.begin();
				AddEntry(*i, j);
			}

			release();
			return 0;
		});
	}
};

//-----------------------------------------------------------------------------
//	Load/Save
//-----------------------------------------------------------------------------

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
	DX12CaptureObject(const DX12Assets::ObjectRecord &o) : type(o.type), name(o.name), obj((uint64)o.obj), info(o.info.length32()) {
		memcpy(info, o.info, o.info.length32());
	}
};

struct DX12CaptureShader {
	uint32			stage;
	string16		name;
	xint64			addr;
	ISO_openarray<uint8>	code;
	DX12CaptureShader(const DX12Assets::ShaderRecord &o) : stage(o.stage), name(o.name), addr(o.addr), code(o.code.length32()) {
		memcpy(code, o.code, o.code.length32());
	}
};
struct DX12Capture : anything {};
ISO_DEFUSERF(DX12Capture, anything, ISO_type_user::WRITETOBIN);

ISO_DEFUSERCOMPF(DX12CaptureModule, 3, ISO_type_user::WRITETOBIN) {
	ISO_SETFIELD(0,a),
	ISO_SETFIELD(1,b),
	ISO_SETFIELD(2,fn);
}};

ISO_DEFUSERCOMPF(DX12CaptureObject, 4, ISO_type_user::WRITETOBIN) {
	ISO_SETFIELD(0,type),
	ISO_SETFIELD(1,name),
	ISO_SETFIELD(2,obj),
	ISO_SETFIELD(3,info);
}};

ISO_DEFUSERCOMPF(DX12CaptureShader, 4, ISO_type_user::WRITETOBIN) {
	ISO_SETFIELD(0,stage),
	ISO_SETFIELD(1,name),
	ISO_SETFIELD(2,addr),
	ISO_SETFIELD(3,code);
}};

bool ViewDX12GPU::SaveCapture(const filename &fn) {
	ISO_ptr<DX12Capture>	p(0);

	p->Append(ISO_ptr<ISO_openarray<DX12CaptureModule> >("Modules", stack_dumper.modules));
	p->Append(ISO_ptr<ISO_openarray<DX12CaptureObject> >("Objects", objects));
	p->Append(ISO_ptr<ISO_openarray<DX12CaptureShader> >("Shaders", shaders));

	for (auto i = cache.begin(); i != cache.end(); ++i) {
		ISO_ptr<pair<xint64,memory_block> >	b0(NULL, make_pair(i->start, memory_block(i->data(), i->size())));
		ISO_ptr<void>	b = MakePtrIndirect<StartBin>(b0);
		p->Append(b);
	}

	return FileHandler::Write(p, fn);
}

memory_block get_memory(ISO_browser b) {
	return memory_block(b[0], b.Count() * b[0].GetSize());
}

void ViewDX12GPU::LoadCapture(const DX12Capture *cap) {
	ISO_ptr<ISO_openarray<DX12CaptureObject> >	_objects;
	ISO_ptr<ISO_openarray<DX12CaptureShader> >	_shaders;

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
			ISO_browser	b(p);
			AddMemory(b[0].Get<uint64>(0), get_memory(b[1]));
		}
	}

	for (auto &i : *_shaders)
		AddShader((SHADERSTAGE)i.stage, string16(i.name), memory_block(i.code, i.code.size()), i.addr);

	objects.reserve(_objects->Count());
	for (auto &i : *_objects)
		AddObject((RecObject::TYPE)i.type, string16(i.name), memory_block(i.info, i.info.size()), (void*)i.obj.i, cache, 0);
}

//-----------------------------------------------------------------------------
//	ViewDX12GPU
//-----------------------------------------------------------------------------

IDFMT	ViewDX12GPU::init_format = IDFMT_LEAVE | IDFMT_FOLLOWPTR;

LRESULT ViewDX12GPU::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			tree.Create(_GetPanePos(0), NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE);
			italics = tree.GetFont().GetParams().Italic(true);

			TabWindow	*t = new TabWindow;
			t->Create(_GetPanePos(1), "tabs", WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, 0);
			t->SetFont(win::Font::DefaultGui());
			tabs = *t;
			SetPanes(tree, tabs, 400);
			return 0;
		}

		case WM_CTLCOLORSTATIC:
			return (LRESULT)GetStockObject(WHITE_BRUSH);

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_FILE_SAVE: {
					filename	fn;
					if (GetSave(*this, fn, "Save Capture", "Binary\0*.ib\0Compressed\0*.ibz\0Text\0*.ix\0")) {
						Busy(), SaveCapture(fn);
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
						AddTab(*new DX12ObjectsList(GetChildWindowPos(), this), true);
					break;

				case ID_DX12GPU_ALLRESOURCES:
					if (!SelectTab(DX12ResourcesList::ID))
						AddTab(*new DX12ResourcesList(GetChildWindowPos(), this), true);
					break;

				case ID_DX12GPU_ALLDESCRIPTORS:
					if (!SelectTab(DX12DescriptorList::ID))
						AddTab(*new DX12DescriptorList(GetChildWindowPos(), this), true);
					break;

				case ID_DX12GPU_ALLSHADERS:
					if (!SelectTab(DX12ShadersList::ID))
						AddTab(*new DX12ShadersList(GetChildWindowPos(), this), true);
					break;

				default: {
					static bool reentry = false;
					if (!reentry) {
						reentry = true;
						int	ret = tabs.GetSelectedControl()(message, wParam, lParam);
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
						default: {
							NMITEMACTIVATE	nmlv2	= *nmlv;
							nmlv2.hdr.hwndFrom		= *this;
							return lv(WM_NOTIFY, wParam, &nmlv2);
						}
					}
					break;
				}

				case NM_RCLICK: {
					NMITEMACTIVATE	*nmlv	= (NMITEMACTIVATE*)nmh;
					switch (wParam) {
						case DX12ResourcesList::ID: {
							break;
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

				case NM_CUSTOMDRAW:
					if (nmh->hwndFrom == tree) {
						NMCUSTOMDRAW *nmcd = (NMCUSTOMDRAW*)nmh;
						switch (nmcd->dwDrawStage) {
							case CDDS_PREPAINT:
								return CDRF_NOTIFYITEMDRAW;

							case CDDS_ITEMPREPAINT: {
								NMTVCUSTOMDRAW	*nmtvcd = (NMTVCUSTOMDRAW*)nmh;
								static COLORREF cols[] = {
									RGB(0,0,0), RGB(192,0,0), RGB(0,0,192), RGB(128,0,128)
								};
								TreeControl::Item	item = tree.GetItem((HTREEITEM)nmcd->dwItemSpec, TVIF_STATE | TVIF_IMAGE);
								nmtvcd->clrText = cols[item.state >> 12];
								if (item.Image() == WT_CALLSTACK) {
									DeviceContext	dc(nmcd->hdc);
									dc.Select(italics);
									return CDRF_NEWFONT;
								}
								return CDRF_DODEFAULT;//CDRF_NEWFONT;
							}
						}
					}
					break;

				case TVN_SELCHANGED: {
					TreeControl	t = nmh->hwndFrom;
					if (t == tree && tree.IsVisible()) {
						TreeSelection(tree.GetSelectedItem());

					}
					break;
				}

				case TCN_SELCHANGE: {
					TabControl2	tab(nmh->hwndFrom);
					tab.ShowSelectedControl();
					break;
				}

				case TCN_DRAG: {
					TabControl2		tab(nmh->hwndFrom);
					Control			c		= tab.GetItemControl(nmh->idFrom);
					bool			top		= !!(GetKeyState(VK_SHIFT) & 0x8000);
					StackWindow		*bw		= StackWindow::Cast(c);

					if (!bw) {
						bw	= new StackWindow(tab.GetChildWindowPos(), str(c.GetText()));
						bw->SetID(c.id);
						c.SetParent(*bw);
						c	= *bw;
					}

					if (top && bw->Depth() == 0)
						top = false;

					if (top) {
						c	= bw->PopView();
						bw	= new StackWindow(tab.GetChildWindowPos(), str(c.GetText()));
						bw->SetID(c.id);
						c.SetParent(*bw);
						c = *bw;
					}

					SeparateWindow	*sw = new SeparateWindow(c, c.GetRect());
					sw->SetOwner(*this);
					if (!top)
						tab.RemoveItem(nmh->idFrom);

					if (bw->NumViews() > 1)
						sw->CreateToolbar(IDR_TOOLBAR_NAV);

					ReleaseCapture();
					sw->Show();
					sw->SendMessage(WM_SYSCOMMAND, SC_MOVE | 2);
					sw->Update();
					return 1;
				}
				case TCN_CLOSE:
					TabControl2(nmh->hwndFrom).GetItemControl(nmh->idFrom).Destroy();
					return 1;
			}
			return 0;
		}

		case WM_PARENTNOTIFY:
			if (LOWORD(wParam) == WM_DESTROY) {
				TabControl2 tab	= Control(lParam);
				if (tab == tabs) {
					if (SplitterWindow *sw = SplitterWindow::Cast(tab.Parent())) {
						Control	sib = sw->GetPane(1 - sw->WhichPane(tab));
						while (sw = SplitterWindow::Cast(sib))
							sib = sw->GetPane(0);

						if (TabWindow::Cast(sib)) {
							tabs = TabControl2(sib);
							return 0;
						}
					}
					TabWindow	*t = new TabWindow;
					t->Create(GetPanePos(1), "tabs", WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, 0);
					t->SetFont(win::Font::DefaultGui());
					SetPane(1, tabs = *t);
					return 0;
				}
			}
			break;

		case WM_CONTEXTMENU: {
			if (context_item = tree.HitTest(tree.ToClient(GetMousePos()))) {
				Menu	m = Menu(IDR_MENU_DX12GPU).GetSubMenuByPos(0);
				m.Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
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

			//Replay(*this, addr);

			AddTab(*new DX12BatchWindow(GetChildWindowPos(), str(tree.GetItemText(hItem)), this, state), new_tab);
			break;
		}

		case WT_CALLSTACK: {
			void	*pc		= i.Param();
			if (i.Image2())
				pc = *(void**)pc;
			auto	frame	= stack_dumper.GetFrame(pc);
			if (frame.file && exists(frame.file)) {
				ISO_browser2	settings	= GetSettings("PSSL");
				auto			colourer	= HLSLcolourer(settings);
				win::Font		font		= win::Font::Params(settings["Font"].GetString());

				FileInput		file(frame.file);
				string			source(file.length());
				file.readbuff(source, file.length());
				EditControl	c = MakeSourceWindow(GetChildWindowPos(), frame.file, colourer, font, source, 0, 0, ES_READONLY, 0);
				c.SetID('SC');
				AddTab(c, new_tab);
				ShowSourceLine(c, frame.line);
			}
			break;
		}

	}
}

void ViewDX12GPU::TreeDoubleClick(HTREEITEM hItem) {
}

field_info GraphicsCommandList_commands[] = {
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
	{"ClearRenderTargetView",				ff(fp<D3D12_CPU_DESCRIPTOR_HANDLE>("RenderTargetView"),fp<const FLOAT>("ColorRGBA[4]"),fp<UINT>("NumRects"),fp<counted<const D3D12_RECT, 2>>("pRects"))},
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
};

field_info Device_commands[] = {
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
};

static HTREEITEM PutCommand(RegisterTree &rt, HTREEITEM h, field_info *nf, const COMRecording::header *p, uint32 addr, DX12Assets &assets) {
	buffer_accum<256>	ba;

	if (rt.format & IDFMT_OFFSETS)
		ba.format("%08x: ", addr);

	return rt.AddFields(rt.AddText(h, ba << nf[p->id].name, addr), nf[p->id].fields, (uint32*)p->data());
}

static void PutCommands(RegisterTree &rt, HTREEITEM h, field_info *nf, memory_block m, intptr_t offset, DX12Assets &assets) {
	for (const COMRecording::header *p = m, *e = (const COMRecording::header*)m.end(); p < e; p = p ->next())
		PutCommand(rt, h, nf, p, uint32((intptr_t)p + offset), assets);
}

bool HasBatches(const interval<uint32> &r, DX12Assets &assets) {
	if (r.length()) {
		const DX12Assets::BatchRecord	*b	= assets.GetBatch(r.begin());
		return b->addr <= r.end() && b->op;
	}
	return false;
}

range<const DX12Assets::BatchRecord*> UsedBatches(const interval<uint32> &r, DX12Assets &assets) {
	if (r.length()) {
		const DX12Assets::BatchRecord	*b	= assets.GetBatch(r.begin());
		if (b->addr <= r.end() && b->op)
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

bool SubTree(RegisterTree &rt, HTREEITEM h, memory_block mem, intptr_t offset, DX12Assets &assets) {
	uint32	start	= intptr_t(mem.start) + offset;
	uint32	end		= start + mem.length32();
	uint32	boffset	= start;

	const DX12Assets::BatchRecord	*b	= assets.GetBatch(start);
	const DX12Assets::MarkerRecord	*m	= assets.GetMarker(start);
	dynamic_array<MarkerStack>		msp;

	bool	got_batch	= b->addr <= end && b->op;

	while (b->addr <= end && b->op) {
		while (msp.size() && b->addr > msp.back().m->end())
			h	= msp.pop_back_value().h;

		while (m != assets.markers.end() && b->addr > m->begin()) {
			MarkerStack	&ms = msp.push_back();
			ms.h	= h;
			ms.m	= m;
			h		= TreeControl::Item(BatchRange(buffer_accum<256>(m->str), *m, assets)).Bold().Param(b->addr - 1).Insert(rt.tree, h);
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

		HTREEITEM		h3	= TreeControl::Item(ba).Bold().Image(ViewDX12GPU::WT_BATCH).Param(b->addr).Insert(rt.tree, h);
		PutCommands(rt, h3, GraphicsCommandList_commands, mem.sub_block(boffset - start, b->addr - boffset), offset, assets);
		boffset = b->addr;
		++b;
	}

	if (boffset != end) {
		PutCommands(rt, got_batch? TreeControl::Item("Orphan").Bold().Insert(rt.tree, h) : h, GraphicsCommandList_commands, mem.sub_block(int(boffset - end)), offset, assets);
	}

	while (msp.size() && boffset > msp.back().m->end())
		h =  msp.pop_back_value().h;

	return got_batch;
}


void ViewDX12GPU::MakeTree() {
	tree.Show(SW_HIDE);
	tree.DeleteItem();

	RegisterTree	rt(tree, this, format);
	HTREEITEM		h		= TVI_ROOT;
	HTREEITEM		h1		= TreeControl::Item("Device").Bold().Insert(tree, h);
	const void**	callstack = 0;

	for (const COMRecording::header *p = device_object->info, *e = (const COMRecording::header*)device_object->info.end(); p < e; p = p->next()) {
		buffer_accum<256>	ba;
		uint32				addr = uint32((char*)p - (char*)device_object->info.start);
		HTREEITEM			h2;

		if (p->id == 0xfffe) {
			callstack	= (const void**)p->data();
		} else {
			if (p->id == 0xffff) {
				uint64		obj		= *(uint64*)p->data();
				p = p->next();

				if (rt.format & IDFMT_OFFSETS)
					ba.format("%08x: ", addr);

				auto		*rec	= FindObject(obj);
				if (rec) {
					ba << rec->name;
				} else {
					ba << "0x" << hex(obj);
				}

				h2	= TreeControl::Item(ba << "::" << Device_commands[p->id].name).Bold().Param(addr).Insert(tree, h1);

				if (p->id == RecDevice::tag_CommandQueueExecuteCommandLists) {
					COMParse(p->data(), [&,this](UINT NumCommandLists, counted<CommandRange,0> pp) {
						bool	has_batches = false;
						for (int i = 0; !has_batches && i < NumCommandLists; i++)
							has_batches = HasBatches(ResolveRange(pp[i], *this), *this);

						if (has_batches)
							h2	= h;
	//					if (NumCommandLists > 1)
	//						h2 = TreeControl::Item("ExecuteCommandLists").Bold().Param(addr).Insert(tree, h2);

						for (int i = 0; i < NumCommandLists; i++) {
							const CommandRange	&r	= pp[i];
							if (ObjectRecord *rec = FindObject(uint64(&*r))) {
								buffer_accum<256>	ba;
	//							if (NumCommandLists == 1)
	//								ba << "ExecuteCommandList: ";

								HTREEITEM	h3	= TreeControl::Item(BatchRange(ba << rec->name, ResolveRange(r, *this), *this)).Bold().Param(addr).Insert(tree, h2);
								if (r.length())
									SubTree(rt, h3, rec->info.sub_block(r.begin(), r.length()), rec->index - intptr_t(rec->info.start), *this);
							}
						}
						if (has_batches)
							h1	= TreeControl::Item("Device").Bold().Insert(tree, h);
					});
				} else {
					rt.AddFields(h2, Device_commands[p->id].fields, (uint32*)p->data());
				}

			} else {
				h2 = PutCommand(rt, h1, Device_commands, p, addr, *this);
			}
			if (callstack) {
				TreeControl::Item("callstack").Image(WT_CALLSTACK).Param(callstack).Image2(1).Children(1).Insert(tree, h2);
//				HTREEITEM	h3 = rt.Add(h2, "callstack", WT_CALLSTACK, callstack[0]);
//				for (int i = 0; i < 32 && callstack[i]; i++)
//					rt.Add(h3, ba.reset() << stack_dumper.GetFrame(callstack[i]), WT_CALLSTACK, callstack[i]);
				callstack = 0;
			}
		}
	}

	tree.Show();
	tree.Invalidate();
}

void ViewDX12GPU::ExpandTree(HTREEITEM h) {
	RegisterTree		rt(tree, this, format);
	buffer_accum<256>	ba;
	
	TreeControl::Item	item = tree.GetItem(h, TVIF_IMAGE|TVIF_PARAM);

	switch (item.Image()) {
		case WT_CALLSTACK: {
			const void**	callstack = item.Param();
			for (int i = 0; i < 32 && callstack[i]; i++)
				rt.Add(h, ba.reset() << stack_dumper.GetFrame(callstack[i]), WT_CALLSTACK, callstack[i]);
		}
	}
}
ViewDX12GPU::ViewDX12GPU(MainWindow &main, const WindowPos &pos) : SplitterWindow(SWF_VERT)
	, stack_dumper(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES)
{
	root			= this;
	format			= init_format;
	context_item	= 0;

	SplitterWindow::Create(pos, "DX12 Dump", WS_CHILD | WS_CLIPCHILDREN, 0);
	toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX12GPU);
	Rebind(this);
}
ViewDX12GPU::ViewDX12GPU(MainWindow &main, const WindowPos &pos, HANDLE process, const char *executable) : SplitterWindow(SWF_VERT)
	, stack_dumper(process, SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES, BuildSymbolSearchPath(filename(executable).dir()))
{
	root			= this;
	format			= init_format;
	context_item	= 0;

	SplitterWindow::Create(pos, "DX12 Dump", WS_CHILD | WS_CLIPCHILDREN, 0);
	toolbar_gpu = main.CreateToolbar(IDR_TOOLBAR_DX12GPU);
	Rebind(this);
}

ViewDX12GPU::~ViewDX12GPU() {
	toolbar_gpu.Destroy();
}


//-----------------------------------------------------------------------------
//	EditorDX12RSX
//-----------------------------------------------------------------------------

class EditorDX12 : public app::Editor, app::MenuItemCallbackT<EditorDX12> {
	Process		proc;
	HMODULE		dll_local;
	HMODULE		dll_remote;
	filename	fn;
	Menu		recent;

	template<typename R, typename T> R Call(const char *func, const T &t) {
		uintptr_t	func_local	= (uintptr_t)GetProcAddress(dll_local, func);
		R			r;
		proc.RunRemote((LPTHREAD_START_ROUTINE)(func_local + (uintptr_t)dll_remote - (uintptr_t)dll_local), t, r);
		return		r;
	}
	template<typename T> int	Call(const char *func, const T &t)	{ return Call<int, T>(func, t);	}
	template<typename R> R		Call(const char *func)				{ return Call<R>(func, 0);	}
	int							Call(const char *func)				{ return Call(func, 0);	}

	template<typename R, typename T> dynamic_array<R> CallArray(const char *func, size_t max, const T &t) {
		uintptr_t		func_local	= (uintptr_t)GetProcAddress(dll_local, func);
		malloc_block	r(flat_array<R>::size(max));
		if (proc.RunRemote((LPTHREAD_START_ROUTINE)(func_local + (uintptr_t)dll_remote - (uintptr_t)dll_local), &t, sizeof(T), r, r.length()))
			return *(flat_array<R>*)r;
		return dynamic_array<R>();
	}
	template<typename R> dynamic_array<R> CallArray(const char *func, size_t max) {
		return CallArray<R>(func, max, 0);
	}

	virtual bool Matches(const ISO_browser &b) {
		return b.Is("DX12GPUState") || b.Is<DX12Capture>(ISOMATCH_NOUSERRECURSE);
	}

	virtual Control Create(app::MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &p) {
		ViewDX12GPU	*view = new ViewDX12GPU(main, wpos);
		view->LoadCapture(p);

		RunThread([view] {
			ProgressBarControl	pb(*view, 0, WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION, 0, AdjustRect(view->GetRect().Centre(500, 30), WS_OVERLAPPED | WS_CAPTION, false));
			Progress	prog(pb, "Collecting assets", view->objects.size32());
			view->GetAssets(prog);
			view->MakeTree();
			pb.PostMessage(WM_CLOSE);
		});
		return *view;
	}
#ifndef ISO_EDITOR
	virtual bool	Command(MainWindow &main, ID id, MODE mode) {
		switch (id) {
			case 0: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				recent			= Menu::Create();
				recent.SetStyle(MNS_NOTIFYBYPOS);

				int		id		= 1;
				for (auto i : GetSettings("DX12/Recent"))
					Menu::Item(i.GetString()).ID(id++).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
				Menu::Item("New executable...").ID(0).Param(static_cast<MenuItemCallback*>(this)).AppendTo(recent);
				
				Menu::Item("Set Executable for DX12").ID(ID_ORBISCRUDE_GRAB_DX12).SubMenu(recent).InsertByPos(menu, 0);
				break;
			}

			case ID_ORBISCRUDE_GRAB: {
				Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
				if (mode == MODE_click && (menu.GetItemStateByID(ID_ORBISCRUDE_GRAB_DX12) & MF_CHECKED)) {
					proc.Run();
					CaptureStats				stats	= Call<CaptureStats>("RPC_Capture", 1);
					dynamic_array<RecObject2>	objects = CallArray<RecObject2>("RPC_GetObjects", stats.num_objects);

					if (objects.empty()) {
						main.AddView(ErrorControl(main.GetChildWindowPos(), "\nFailed to capture"));
						break;
					}


					ViewDX12GPU	*view = new ViewDX12GPU(main, main.GetChildWindowPos(), proc.hProcess, fn);
					main.AddView(*view);
					main.SetTitle("new");

					malloc_block		names	= proc.GetMemory(stats.names);
					uint16				*pnames	= names;
					int					counts[RecObject::NUM_TYPES] = {};

					view->objects.reserve(objects.size());

					dx_memory_interface		mem(proc.hProcess);
					hash_multiset<string>	name_counts;

					for (auto &i : objects) {
						buffer_accum<256>	name;
						
						if (pnames[0]) {
							name << str(pnames);
							if (int n = name_counts.insert(str(name)))
								name << '(' << n << ')';
						} else {
							name << get_field_name(i.type) << '#' << counts[i.type]++;
						}
						DX12Assets::ObjectRecord	*p	= view->AddObject(i.type,
							str(name),
							proc.GetMemory(i.info),
							i.obj,
							view->cache, &mem
						);
						pnames	+= string_len(pnames) + 1;
					}

					ProgressBarControl	pb(*view);
					Progress	prog(pb, "Collecting assets", stats.num_objects);

					Call("RPC_Continue");
					view->GetAssets(prog);
					view->MakeTree();
				}
				break;
			}
		}
		return false;
	}
#endif
public:
	void	operator()(Control c, Menu::Item i) {
		ISO_browser2	settings	= GetSettings("DX12/Recent");
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
			Menu::Item(fn).ID(id).Param(MenuItemCallback(this)).InsertByPos(recent, -1);
		}

		fn = settings[id - 1].GetString();
		recent.RadioDirect(id, 0, recent.Count());

		Menu	m	=  MainWindow::Cast(c)->GetDropDown(ID_ORBISCRUDE_GRAB);
		m.RadioDirect(ID_ORBISCRUDE_GRAB_DX12, ID_ORBISCRUDE_GRAB_TARGETS, ID_ORBISCRUDE_GRAB_TARGETS_MAX);
		Menu::Item().Check(true).Type(MFT_RADIOCHECK).SetByPos(m, m.FindPosition(recent));

		const char *dll_name	= "dx12crude.dll";
		filename	dll_path	= get_exec_dir().add_dir(dll_name);
		Resource	r(0, dll_name, "BIN");
		if (!check_writebuff(FileOutput(dll_path), r, r.length())) {
			ISO_OUTPUTF("Cannot write to ") << dll_path << '\n';
		}

		proc.Open(fn, 0, 0);
		dll_local	= GetModuleHandleA(dll_name);
		if (!dll_local)
			dll_local = LoadLibraryA(dll_name);

		filename	path;
		if (FindModule(dll_name, path))
			proc.InjectDLL(path);

		dll_remote	= proc.FindModule(dll_name, path);

		Call("RPC_Hook", 42);
		//proc.Run();

	}
	EditorDX12() {
		ISO_getdef<DX12Capture>();
	}
} editor_dx12;

void dx12_dummy() {}
