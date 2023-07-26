#include "graphics.h"
#include "base/strings.h"
#include "base/hash.h"
#include "dx/dxgi_helpers.h"

#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <dxcapi.h>

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxcompiler.lib")

namespace iso {

bool CheckResult(HRESULT h) {
	if (h == S_OK)
		return true;
//	h = graphics.device->GetDeviceRemovedReason();
	MessageBoxA(NULL, (buffer_accum<256>() << Win32Error(h)).term(), "D3D12", MB_ICONERROR | MB_OK);
	return false;
}

template<typename T> bool SetNextName(T *r, const char *prefix = "res") {
	static int i = 0;
	return SUCCEEDED(r->SetName((str16(prefix) << "#" << i++).term()));
}

bool CreateCommittedResource(
	ID3D12Device*					device,
	const D3D12_HEAP_PROPERTIES*	pHeapProperties,
	D3D12_HEAP_FLAGS				HeapFlags,
	const D3D12_RESOURCE_DESC*		pDesc,
	D3D12_RESOURCE_STATES			InitialResourceState,
	const D3D12_CLEAR_VALUE*		pOptimizedClearValue,
	ID3D12Resource**				ppvResource,
	const char*						prefix = "res"
) {
	HRESULT hr = device->CreateCommittedResource(
		pHeapProperties,
		HeapFlags,
		pDesc,
		InitialResourceState,
		pOptimizedClearValue,
		__uuidof(ID3D12Resource),
		(void**)ppvResource
	);
	return CheckResult(hr) && SetNextName(*ppvResource, prefix);
}

//-----------------------------------------------------------------------------
//	DescriptorAllocator
//-----------------------------------------------------------------------------

struct DescriptorAllocator {
	struct Block : dx12::DescriptorHeap {
		Block	*next	= 0;
		uint64	used	= 0;
		Block(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type) : dx12::DescriptorHeap(device, 64, type) {}
	};
	Block			*avail, *pend, *full;
	DescriptorAllocator() :	avail(0), pend(0), full(0) {}
	D3D12_CPU_DESCRIPTOR_HANDLE	alloc(D3D12_DESCRIPTOR_HEAP_TYPE type);
	void						free(D3D12_CPU_DESCRIPTOR_HANDLE h);
};

D3D12_CPU_DESCRIPTOR_HANDLE	DescriptorAllocator::alloc(D3D12_DESCRIPTOR_HEAP_TYPE type) {
	Block	*block = avail;
	if (!block) {
		block	= avail = pend;
		pend	= 0;
		if (!block)
			block	= avail = new Block(graphics.device, type);
	}

	int	i = lowest_clear_index(block->used);
	if (!~(block->used |= bit64(i))) {
		avail = block->next;
		block->next = full;
		full = block;
	}
	return block->cpu(i);
}

void DescriptorAllocator::free(D3D12_CPU_DESCRIPTOR_HANDLE h) {
	for (Block *b = avail; b; b = b->next) {
		if (b->contains(h, 64)) {
			ISO_ASSERT(b->used & bit64(b->index_of(h)));
			b->used &= ~bit64(b->index_of(h));
			return;
		}
	}
	for (Block *b = pend; b; b = b->next) {
		if (b->contains(h, 64)) {
			b->used &= ~bit64(b->index_of(h));
			return;
		}
	}
	for (Block *b = full, *prev = 0; b; prev = b, b = b->next) {
		if (b->contains(h, 64)) {
			b->used = ~bit64(b->index_of(h));
			if (prev)
				prev->next = b->next;
			else
				full = b->next;
			b->next = pend;
			pend	= b;
			return;
		}
	}
	ISO_ASSERT(0);
}

//-----------------------------------------------------------------------------
//	FrameDescriptorAllocator
//-----------------------------------------------------------------------------

struct FrameDescriptorAllocator : dx12::DescriptorHeap {
	atomic<int>	p;
	int			g;
	int			b;
	int			ends[Graphics::NumFrames];

	void		init(ID3D12Device *device, uint32 num, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
		dx12::DescriptorHeap::init(device, num, type, flags);
		b			= num;
		iso::clear(ends);
	}
	void	BeginFrame(int index) {
		g = ends[index];
	}
	void	EndFrame(int index) {
		ends[index] = p;
	}
	int		alloc_index(uint32 n = 1) {
		int t, p0;
		do {
			p0	= p;
			t	= p0;
			if (p0 < g || p0 + n >= b) {
				if (p0 >= g)
					t = 0;
				ISO_ASSERT(t + n < g);
			}
		} while (!p.cas(p0, t + n));
		return t;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE	alloc(uint32 n = 1) {
		return cpu(alloc_index(n));
	}
};

//-----------------------------------------------------------------------------
//	VRAMAllocator
//-----------------------------------------------------------------------------

struct FrameAllocator2 : FrameAllocator {
	void		*ends[Graphics::NumFrames];
	FrameAllocator2() {}
	FrameAllocator2(const memory_block &m) {
		init(m);
	}
	void	init(const memory_block &m) {
		FrameAllocator::init(m);
		for (auto &i : ends)
			i = m;
	}

	void	BeginFrame(int index)	{ set_get(ends[index]); }
	void	EndFrame(int index)		{ ends[index] = getp(); }
};

struct VRAMAllocator : FrameAllocator2 {
	com_ptr<ID3D12Heap>			heap;
	com_ptr<ID3D12Resource>		resource;
	D3D12_GPU_VIRTUAL_ADDRESS	gpu_base;

	bool						init(ID3D12Device *device, size_t size, bool gpu_only = false);
	D3D12_GPU_VIRTUAL_ADDRESS	to_gpu(const void *p) { return gpu_base + to_offset(p); }

//	void	BeginFrame(int index)	{ FrameAllocator2::BeginFrame(index); }
//	void	EndFrame(int index)		{ FrameAllocator2::EndFrame(index); }
};

#if 0
bool VRAMAllocator::init(ID3D12Device *device, size_t size, bool gpu_only) {
	D3D12_HEAP_DESC hdesc = {};
	hdesc.SizeInBytes					= size;
	hdesc.Alignment						= 64 * 1024;
	hdesc.Flags							= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
	hdesc.Properties.Type				= D3D12_HEAP_TYPE_UPLOAD;
	hdesc.Properties.CPUPageProperty	= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	hdesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	hdesc.Properties.CreationNodeMask	= 1;
	hdesc.Properties.VisibleNodeMask	= 1;

	D3D12_RESOURCE_DESC rdesc = {};
	rdesc.Dimension						= D3D12_RESOURCE_DIMENSION_BUFFER;
	rdesc.Alignment						= 64 * 1024;
	rdesc.Width							= size;
	rdesc.Height						= 1;
	rdesc.DepthOrArraySize				= 1;
	rdesc.MipLevels						= 1;
	rdesc.Format						= DXGI_FORMAT_UNKNOWN;
	rdesc.SampleDesc.Count				= 1;
	rdesc.SampleDesc.Quality			= 0;
	rdesc.Layout						= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rdesc.Flags							= D3D12_RESOURCE_FLAG_NONE;

	void*	mem;
	if (!CheckResult(device->CreateHeap(&hdesc, __uuidof(ID3D12Heap), (void**)&heap))
	||	!CheckResult(device->CreatePlacedResource(heap, 0, &rdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&resource))
	||	!CheckResult(resource->Map(0, NULL, &mem))
	)
		return false;

	*(FrameAllocator*)this	= FrameAllocator(memory_block(mem, size));
	gpu_base = resource->GetGPUVirtualAddress();

	return true;
}
#else

bool VRAMAllocator::init(ID3D12Device *device, size_t size, bool gpu_only) {
	CreateCommittedResource(device, 
		dx12::HEAP_PROPERTIES(gpu_only ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		dx12::RESOURCE_DESC::Buffer(size, gpu_only ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE),
		gpu_only ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		&resource
	);
	void*	mem;
	resource->Map(0, nullptr, &mem);
	*(FrameAllocator*)this = FrameAllocator(memory_block(mem, size));
	gpu_base = resource->GetGPUVirtualAddress();

	return true;
}

#endif

DescriptorAllocator			descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
FrameDescriptorAllocator	frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
FrameAllocator2				cpu_temp_allocator;
VRAMAllocator				gpu_temp_allocator;
Graphics					graphics;

hash_map<PackedGraphicsState, com_ptr2<ID3D12PipelineState> >	pso_hash;
hash_map<const void*, com_ptr2<ID3D12PipelineState> >			compute_pso_hash;
//atomic<fixed_pool<Resource,65536> >							ResourceHandle::pool;
dx12::Uploader				uploader;

//------------------------------------------------------------

float4x4	hardware_fix(param(float4x4) mat)	{ return translate(zero, zero, half) * scale(one,  -one, -half) * mat; }
float4x4	map_fix(param(float4x4) mat)		{ return translate(half, half, half) * scale(half, half, -half) * mat;}


void Resource::Reset() {
	if (cpu_handle.ptr) {
		descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].free(cpu_handle);
		if (uav_handle.ptr)
			descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].free(uav_handle);
	}
}

void ResourceHandle::release() {
	if (Resource *p = get()) {
		ID3D12Resource	*res	= p->get();
		if (!res || res->Release() == 0) {
			p->Reset();
			pool.release(p);
		}
	}
}

//-----------------------------------------------------------------------------
//	Buffer
//-----------------------------------------------------------------------------

bool _Buffer::Bind(TexFormat format, uint32 stride, uint32 num) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
	iso::clear(srv_desc);

	// check if structured
	if (format && stride == ByteSize(format))
		stride = 0;
	else
		format = TEXF_UNKNOWN;	//(The Format must be DXGI_FORMAT_UNKNOWN when creating a View of a Structured Buffer)

	srv_desc.Format						= GetDXGI(format);
	srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_BUFFER;
	srv_desc.Shader4ComponentMapping	= GetSwizzle(format);
	srv_desc.Buffer.FirstElement		= 0;
	srv_desc.Buffer.NumElements			= num;
	srv_desc.Buffer.StructureByteStride	= stride;
    //desc.Buffer.Flags;

	Resource	*p = get();
	p->cpu_handle	= descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//	p->states		= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	graphics.device->CreateShaderResourceView(*p, &srv_desc, p->cpu_handle);
	return true;
}

bool _Buffer::Init(uint32 size, Memory loc) {
	Resource	*p	= alloc();
	return CreateCommittedResource(graphics.device,
		dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		dx12::RESOURCE_DESC::Buffer(size),
		p->states = D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		&*p
	);
}

bool _Buffer::Init(const void *data, uint32 size, Memory loc) {
	Init(size, loc);
	Resource		*p		= get();
	ID3D12Resource	*res	= *p;
	void			*dest;
	if (!CheckResult(res->Map(0, nullptr, &dest)))
		return false;
	memcpy(dest, data, size);
	res->Unmap(0, nullptr);
	return true;
}

Resource Graphics::MakeDataBuffer(TexFormat format, uint32 count, stride_t stride, Memory loc, const void *data) {

	Resource	res;
	if (count == 0)
		return res;

	res.states = data ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COMMON;
	uint64		size = fullmul(count, (uint32)stride);

	CreateCommittedResource(device,
		dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		dx12::RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAGS(loc & _MEM_RESOURCEFLAGS)),
		res.states,
		nullptr,
		&res
	);

	if (data) {
		uploader.Begin(device);
		uploader.Upload(device, res, 0, const_memory_block(data, size));
		uploader.End();
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
	clear(srv_desc);
	srv_desc.Format						= GetDXGI(format);
	srv_desc.Shader4ComponentMapping	= GetSwizzle(format);
	srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.NumElements			= count;
	srv_desc.Buffer.StructureByteStride	= srv_desc.Format ? 0 : stride;

	res.cpu_handle	= descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	res.states		= data ? D3D12_RESOURCE_STATE_COPY_DEST
		: loc & MEM_DEPTH	? D3D12_RESOURCE_STATE_DEPTH_WRITE
		: loc & MEM_TARGET	? D3D12_RESOURCE_STATE_RENDER_TARGET
		: D3D12_RESOURCE_STATE_COMMON;

	device->CreateShaderResourceView(res, &srv_desc, res.cpu_handle);

	if (loc & MEM_WRITABLE) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC	uav_desc;
		clear(uav_desc);
		uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_BUFFER;
		res.uav_handle			= descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CreateUnorderedAccessView(res, NULL, &uav_desc, res.uav_handle);
	}
	return res;
}

void Init(DataBuffer *x, void *physram)	{
	if (auto *pc = (DX12Buffer*)x->raw()) {
		TexFormat	format	= pc->format_or_stride & bit(31) ? TEXF_UNKNOWN : TexFormat(pc->format_or_stride);
		stride_t	stride(pc->format_or_stride & bit(31) ? pc->format_or_stride & ~bit(31) : ByteSize((TexFormat)pc->format_or_stride));
		uint32		count	= pc->width;
		uint8		*data	= (uint8*)physram + pc->offset;
		Memory		loc		= MEM_DEFAULT;

		if (!GetSwizzle(format))
			format = TexFormat(format | (D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING << 8));

		x->alloc(graphics.MakeDataBuffer(format, count, stride, loc, data));
	}
}

void DeInit(DataBuffer *x) {
	x->~DataBuffer();
}

//-----------------------------------------------------------------------------
//	Vertices
//-----------------------------------------------------------------------------

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
	D3D12_INPUT_ELEMENT_DESC *ve2 = new D3D12_INPUT_ELEMENT_DESC[n];
	for (int i = 0; i < n; i++) {
		ve2[i].InputSlot			= ve[i].stream;
		ve2[i].AlignedByteOffset	= ve[i].offset;
		ve2[i].Format				= ve[i].type;
		ve2[i].SemanticName			= semantics[ve[i].usage & 0xff];
		ve2[i].SemanticIndex		= ve[i].usage >> 8;
		ve2[i].InputSlotClass		= D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		ve2[i].InstanceDataStepRate	= 0;
	}
	pInputElementDescs = ve2;
	NumElements	= n;

	return true;
}

//-----------------------------------------------------------------------------
//	Texture
//-----------------------------------------------------------------------------

ID3D12Resource *Graphics::MakeTextureResource(TexFormat format, int width, int height, int depth, int mips, Memory loc, const void *data, int pitch) {
	DXGI_FORMAT			dxgi = GetDXGI(format);
	D3D12_CLEAR_VALUE	clear_val, *pclear_val = nullptr;
	
	if (loc & (MEM_DEPTH | MEM_TARGET)) {
		pclear_val = &clear_val;
		clear(clear_val);
		clear_val.Format	= dxgi;
		if (loc & MEM_TARGET)
			clear_val.Color[0] = 1;
	}

	switch (dxgi) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	dxgi = DXGI_FORMAT_R32G32_TYPELESS;	break;
		case DXGI_FORMAT_D32_FLOAT:				dxgi = DXGI_FORMAT_R32_TYPELESS;	break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:		dxgi = DXGI_FORMAT_R24G8_TYPELESS;	break;
		case DXGI_FORMAT_D16_UNORM:				dxgi = DXGI_FORMAT_R16_TYPELESS;	break;
		default:
			if ((loc & MEM_WRITABLE) && DXGI_COMPONENTS(dxgi).Bytes() == 4)
				dxgi = DXGI_FORMAT_R8G8B8A8_TYPELESS;
			else if (loc & MEM_CASTABLE)
				dxgi = DXGI_COMPONENTS(dxgi).Type(DXGI_COMPONENTS::TYPELESS).GetFormat();
			break;
	}

	ID3D12Resource			*res	= 0;
	D3D12_RESOURCE_STATES	states	= data ? D3D12_RESOURCE_STATE_COPY_DEST
		: loc & MEM_DEPTH	? D3D12_RESOURCE_STATE_DEPTH_WRITE
		: loc & MEM_TARGET	? D3D12_RESOURCE_STATE_RENDER_TARGET
		: D3D12_RESOURCE_STATE_COMMON;

	CreateCommittedResource(device, dx12::HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		dx12::RESOURCE_DESC(
			(loc & MEM_FORCE2D) == MEM_FORCE2D	? D3D12_RESOURCE_DIMENSION_TEXTURE2D
			: height == 1						? D3D12_RESOURCE_DIMENSION_TEXTURE1D
			: loc & MEM_VOLUME					? D3D12_RESOURCE_DIMENSION_TEXTURE3D
			: D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			width, height, depth, mips,
			dxgi,
			1, 0,
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			D3D12_RESOURCE_FLAGS(loc & _MEM_RESOURCEFLAGS)
		),
		states,
		pclear_val,
		&res
	);

	if (data) {
		height = adjust_size(dxgi, height);
		uint32		num		= loc & MEM_VOLUME ? mips : mips * depth;

		uploader.Begin(device);

		if ((loc & MEM_FORCE2D) == MEM_VOLUME) {
			for (int m = 0; m < mips; m++) {
				uint32	stride	= pitch >> m, stride2 = stride * max(height >> m, 1);
				uploader.Upload(device, res, m, dx12::Uploader::Block(data, stride, stride2));
				data = (uint8*)data + max(depth >> m, 1) * stride2;
			}

		} else {
			for (int d = 0; d < depth; d++) {
				for (int m = 0; m < mips; m++) {
					uint32	stride	= pitch >> m, stride2 = stride * max(height >> m, 1);
					uploader.Upload(device, res, m + (mips * d), dx12::Uploader::Block(data, stride, stride2));
					//*p++ = D3D12Block(data, stride, stride2);
					data = (uint8*)data + stride2;
				}
			}
		}
		uploader.End();
	}

	return res;
}

Resource Graphics::MakeTexture(TexFormat format, int width, int height, int depth, int mips, Memory loc, const void *data, int pitch) {
	ID3D12Resource	*res	= MakeTextureResource(format, width, height, depth, mips, loc, data, pitch);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
	clear(srv_desc);
	srv_desc.Shader4ComponentMapping = GetSwizzle(format);

	DXGI_FORMAT			dxgi = GetDXGI(format);
	switch (dxgi) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	dxgi	= DXGI_FORMAT_R32G32_UINT;			break;
		case DXGI_FORMAT_D32_FLOAT:				dxgi	= DXGI_FORMAT_R32_FLOAT;			break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:		dxgi	= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;break;
		case DXGI_FORMAT_D16_UNORM:				dxgi	= DXGI_FORMAT_R16_UINT;				break;
		default: break;
	}
	
	srv_desc.Format = dxgi;

	if ((loc & MEM_FORCE2D) == MEM_FORCE2D) {
		if (depth == 1) {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels		= mips;
		} else {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels	= mips;
			srv_desc.Texture2DArray.ArraySize	= depth;
		}

	} else if (height == 1) {
		if (depth == 1) {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE1D;
			srv_desc.Texture1D.MipLevels		= mips;
		} else {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			srv_desc.Texture1DArray.MipLevels	= mips;
			srv_desc.Texture2DArray.ArraySize	= depth;
		}

	} else if (loc & MEM_VOLUME) {
		srv_desc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture3D.MipLevels			= mips;

	} else if (loc & MEM_CUBE) {
		if (depth < 12) {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURECUBE;
			srv_desc.TextureCube.MipLevels		= mips;
		} else {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			srv_desc.TextureCubeArray.MipLevels	= mips;
			srv_desc.TextureCubeArray.NumCubes	= depth / 6;
		}
	} else {
		if (depth == 1) {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels		= mips;
		} else {
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels	= mips;
			srv_desc.Texture2DArray.ArraySize	= depth;
			//srv_desc.Texture2DArray.PlaneSlice	=
		}
	}

	Resource result;
	*&result = res;

	result.cpu_handle	= descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	result.states		= data ? D3D12_RESOURCE_STATE_COPY_DEST
		: loc & MEM_DEPTH	? D3D12_RESOURCE_STATE_DEPTH_WRITE
		: loc & MEM_TARGET	? D3D12_RESOURCE_STATE_RENDER_TARGET
		: D3D12_RESOURCE_STATE_COMMON;

	device->CreateShaderResourceView(res, &srv_desc, result.cpu_handle);

	if (loc & MEM_WRITABLE) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC	uav_desc;
		clear(uav_desc);
		uav_desc.Format = srv_desc.Format;
		switch (srv_desc.ViewDimension) {
			case D3D12_SRV_DIMENSION_TEXTURE1D:			uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_TEXTURE1D;		break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:	uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_TEXTURE1DARRAY;	break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:			uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_TEXTURE2D;		break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:	uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_TEXTURE2DARRAY;	break;
			case D3D12_SRV_DIMENSION_TEXTURE3D:			uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_TEXTURE3D;		break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:		uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_TEXTURE2DARRAY;	break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:	uav_desc.ViewDimension	= D3D12_UAV_DIMENSION_TEXTURE2DARRAY;	break;
		}
		result.uav_handle = descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CreateUnorderedAccessView(res, NULL, &uav_desc, result.uav_handle);
	}
	return result;
}

void Init(DX11Texture *x, void *physram) {}
void DeInit(DX11Texture *x)	{}

void Init(Texture *x, void *physram) {
	if (DX12Texture *pc = (DX12Texture*)x->raw()) {
		uint32		width	= pc->width;
		uint32		height	= pc->height;
		uint32		mips	= pc->mips;
		uint32		depth	= pc->depth;
		TexFormat	format	= (TexFormat)pc->format;
		uint8		*data	= (uint8*)physram + pc->offset;
		Memory		loc		= pc->cube ? MEM_CUBE : depth > 1 && !pc->array ? MEM_VOLUME : MEM_DEFAULT;

		if (!GetSwizzle(format))
			format = TexFormat(format | (D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING << 8));
		x->alloc(graphics.MakeTexture(format, width, height, depth, mips, loc, data, stride(GetDXGI(format), width)));
	}
}

void DeInit(Texture *x)	{
	x->~Texture();
}

bool Texture::Init(TexFormat format, int width, int height, int depth, int mips, Memory loc, void *data, int pitch) {
	release();
	alloc(graphics.MakeTexture(format, width, height, depth, mips, loc, data, pitch));
	return true;
}

bool Texture::Init(TexFormat format, int width, int height, int depth, int mips, Memory loc) {
	release();
	alloc(graphics.MakeTexture(format, width, height, depth, mips, loc, NULL, 0));
	return true;
}

void Texture::DeInit() {
	Texture::~Texture();
}

int	 Texture::Depth() const {
	return 1;
}

Surface	Texture::GetSurface(int i) const {
	const _Texture	*tex	= *this;
	TexFormat		fmt		= (TexFormat)tex->GetDesc().Format;
	return Surface(*this, fmt, i, 0, 1);
}

Surface	Texture::GetSurface(CubemapFace f, int i) const {
	const _Texture	*tex	= *this;
	TexFormat		fmt		= (TexFormat)tex->GetDesc().Format;
	return Surface(*this, fmt, i, f, 1);
}

//-----------------------------------------------------------------------------
//	InputResourceOffsets
//-----------------------------------------------------------------------------

struct InputResourceOffsets {
	typedef dx::DXBC::BlobT<dx::DXBC::RootSignature>	RTS;

	enum Type {
		SRV,
		UAV,
		CBV,
		SMP,
		NumTotalTypes,
	};
	enum {
		ROOT		= 0x8000,
		VAL_MASK	= 0x7FFF,
	};

	uint16	const_num;
	uint16	descr_num;

	uint16	slots[6][NumTotalTypes][32];

	void Init(const RTS *sig);
};

void InputResourceOffsets::Init(const RTS *sig) {
	uint32	const_offset	= 0;
	uint32	descr_offset	= 0;

	for (auto &i : sig->parameters.entries(sig)) {
		void		*p = i.ptr.get(sig);
		switch (i.type) {
			case RTS::Parameter::TABLE: {
				auto	*p2 = (RTS::DescriptorTable*)p;
				for (auto &i : p2->entries(sig))
					descr_offset += i.num;
				break;
			}
			case RTS::Parameter::CONSTANTS: {
				auto	*p2 = (RTS::Constants*)p;
				const_offset	+= p2->num;
				break;
			}
			default:
				++descr_offset;
				break;
		}
	}

	const_num		= const_offset;
	descr_num		= descr_offset;
	const_offset	= 0;
	descr_offset	= 0;

	for (auto &i : sig->parameters.entries(sig)) {
		void		*p = i.ptr.get(sig);
		switch (i.type) {
			case RTS::Parameter::TABLE: {
				auto	*p2		= (RTS::DescriptorTable*)p;
				auto	s		= slots[i.visibility];
				uint32	offset	= 0;
				for (auto &i : p2->entries(sig)) {
					if (~i.offset)
						offset = i.offset;
					auto	s2	= s[i.type] + i.base;
					for (int j = 0; j < i.num; j++)
						*s2++ = descr_offset++;
				}
				break;
			}
			case RTS::Parameter::CONSTANTS: {
				auto	*p2		= (RTS::Constants*)p;
				slots[i.visibility][CBV][p2->base] = const_offset | ROOT;
				const_offset	+= p2->num;
				break;
			}
			case RTS::Parameter::CBV: {
				auto	*p2 = (RTS::Descriptor*)p;
				slots[i.visibility][CBV][p2->reg] = descr_offset++;
				break;
			}
			case RTS::Parameter::SRV: {
				auto	*p2 = (RTS::Descriptor*)p;
				slots[i.visibility][SRV][p2->reg] = descr_offset++;
				break;
			}
			case RTS::Parameter::UAV: {
				auto	*p2 = (RTS::Descriptor*)p;
				slots[i.visibility][UAV][p2->reg] = descr_offset++;
				break;
			}
		}
	}

	int	s = ROOT;
	for (auto &i : sig->samplers.entries(sig))
		slots[i.visibility][SMP][i.reg] = s++;
}

//-----------------------------------------------------------------------------
//	ConstBuffer
//-----------------------------------------------------------------------------

void ConstBuffer::init(uint32 _size) {
	mem.create(_size);
	handle	= descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ConstBuffer::~ConstBuffer() {
	if (gpu)
		descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].free(handle);
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstBuffer::FixBuffer() {
	if (!gpu) {
		gpu = gpu_temp_allocator.alloc(mem.length(), 256);
		memcpy(gpu, mem, mem.length());
		D3D12_CONSTANT_BUFFER_VIEW_DESC cdesc = { gpu_temp_allocator.to_gpu(gpu), align_pow2(mem.size32(), 8) };
		graphics.device->CreateConstantBufferView(&cdesc, handle);
	}
	return handle;
}

ConstBuffer *Graphics::FindConstBuffer(crc32 id, uint32 size, uint32 num) {
	uint32		id2 = (uint32)id + (size << 4) + num;
	ConstBuffer	*&p	= cb[id2];
	if (!p)
		p = new ConstBuffer(size);
	return p;
}

//-----------------------------------------------------------------------------
//	Graphics
//-----------------------------------------------------------------------------

bool Graphics::Init(IDXGIAdapter1 *adapter) {
	if (device)
		return true;

#ifdef _DEBUG
	// Enable the D3D12 debug layer.
	com_ptr<ID3D12Debug> debug;
	if (SUCCEEDED(D3D12GetDebugInterface(COM_CREATE(&debug))))
		debug->EnableDebugLayer();
#endif

	com_ptr<IDXGIFactory4> factory;
	if (!CheckResult(CreateDXGIFactory1(COM_CREATE(&factory)))
	||	!CheckResult(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&device))
	)
		return false;

#if 0//def _DEBUG
	com_ptr<ID3D12InfoQueue>	info_queue = device.query<ID3D12InfoQueue>();
	info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif

	// create the command queue.
	if (!queue.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT))
		return false;

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		frame_descriptor_alloc[i].init(device, 1024, (D3D12_DESCRIPTOR_HEAP_TYPE)i, i < 2 ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

	if (!gpu_temp_allocator.init(device, 0x100000))
		return false;

	cpu_temp_allocator.init(malloc_block(64 * 1024).detach());

	return true;
}

FrameAllocator &Graphics::allocator() {
	return cpu_temp_allocator;
}

void Graphics::BeginScene(GraphicsContext &ctx) {
//	ctx.InitFrame();

	int	index = frame % NumFrames;

	cpu_temp_allocator.BeginFrame(index);
	gpu_temp_allocator.BeginFrame(index);
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		frame_descriptor_alloc[i].BeginFrame(index);

	ctx.Begin();

//	Resource	*res	= disp.surface[index].get();
//	ctx.barriers.Transition(ctx.cmd->list, res, D3D12_RESOURCE_STATE_RENDER_TARGET);
//	ctx.barriers.Flush(ctx.cmd->list);

}

void Graphics::EndScene(GraphicsContext &ctx) {
	int	index = frame % NumFrames;

	// Indicate that the back buffer will now be used to present.
	//ctx.barriers.Transition(ctx.cmd->list, disp._GetDispBuffer(), D3D12_RESOURCE_STATE_PRESENT);
	ctx.End();

	cpu_temp_allocator.EndFrame(index);
	gpu_temp_allocator.EndFrame(index);
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		frame_descriptor_alloc[i].EndFrame(index);

	for (auto &i : cb)
		i->gpu = 0;

	++frame;
}

//-----------------------------------------------------------------------------
//	Graphics::Display
//-----------------------------------------------------------------------------

Graphics::Display::~Display() {
	if (swapchain) {
		graphics.queue.Flush();

		for (auto &i : surface) {
			if (Resource *p = i.get()) {
				p->get()->Release();
				p->Reset();
			}
		}
	}
}

bool Graphics::Display::SetFormat(const RenderWindow *window, const point &size, TexFormat _format) {
	if (swapchain) {
		if (all(Size() == size) && format == _format)
			return true;
	}

	if (swapchain) {
		graphics.queue.Flush();

		for (auto &i : surface) {
			if (Resource *p = i.get()) {
				p->get()->Release();
				p->Reset();
			}
		}

		if (!CheckResult(swapchain->ResizeBuffers((UINT)num_elements(surface), size.x, size.y, GetDXGI(format), 0)))
			return false;

	} else {
		graphics.Init((HWND)window, GetAdapter(0));

		// Create swap chain
		DXGI_SWAP_CHAIN_DESC1 desc;
		clear(desc);
		desc.BufferCount		= (UINT)num_elements(surface);
		desc.Width				= size.x;
		desc.Height				= size.y;
		desc.Format				= GetDXGI(format);
		desc.BufferUsage		= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SampleDesc.Count	= 1;
		desc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;

		com_ptr<IDXGIFactory4> dxgi;
		com_ptr<IDXGISwapChain1> _swapchain;
		if (!CheckResult(CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)&dxgi))
		||	!CheckResult(dxgi->CreateSwapChainForHwnd(graphics.queue, (HWND)window, &desc, nullptr, nullptr, &_swapchain))
		||	!CheckResult(dxgi->MakeWindowAssociation((HWND)window, DXGI_MWA_NO_ALT_ENTER))
		||	!CheckResult(_swapchain.query(&swapchain))
		)
			return false;
	}

	for (int i = 0; i < num_elements(surface); i++) {
		Resource	*r = surface[i].alloc();
		if (!CheckResult(swapchain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&*r)))
			return false;

		r->get()->SetName(str16(format_string("backbuffer %i", i)));
		r->states	= D3D12_RESOURCE_STATE_PRESENT;
	}

	return true;
}

point Graphics::Display::Size() const {
	uint32		width, height;
	swapchain->GetSourceSize(&width, &height);
	return {width, height};
}

bool Graphics::Display::Present() const {
	// Present the frame.
	if (!CheckResult(swapchain->Present(0, 0)))
		return false;

//	graphics.frame = swapchain->GetCurrentBackBufferIndex();

	return true;
}

void Graphics::Display::MakePresentable(GraphicsContext &ctx) const {
	// Indicate that the back buffer will now be used to present.
	ctx.barriers.Transition(ctx.cmd->list, surface[swapchain->GetCurrentBackBufferIndex()].get(), D3D12_RESOURCE_STATE_PRESENT);
}

//-----------------------------------------------------------------------------
//	Graphics::StageState
//-----------------------------------------------------------------------------

com_ptr<ID3D12ShaderReflection> GetReflection(const_memory_block shader) {
	com_ptr<ID3D12ShaderReflection>	refl;
	D3D12_SHADER_DESC				desc;

	if (D3DReflect(shader, shader.length(), COM_CREATE(&refl)) != S_OK) {
		com_ptr<IDxcContainerReflection>	dxc_refl;
		DxcCreateInstance(CLSID_DxcContainerReflection, COM_CREATE(&dxc_refl));

		struct MyDxcBloc : com<IDxcBlob> {
			const_memory_block	b;
		public:
			LPVOID STDMETHODCALLTYPE GetBufferPointer()	{ return unconst(b.begin()); }
			SIZE_T STDMETHODCALLTYPE GetBufferSize()	{ return b.size(); }

			MyDxcBloc(const_memory_block b) : b(b) {}
		};

		MyDxcBloc	blob(shader);
		dxc_refl->Load(&blob);

		uint32 partIndex;
		dxc_refl->FindFirstPartKind("DXIL"_u32, &partIndex);
		dxc_refl->GetPartReflection(partIndex, COM_CREATE(&refl));
	}

	return refl;
}

bool Graphics::StageState::Init(const void *_shader) {
	if (shader == _shader)
		return false;

	if (shader = _shader) {
		dirty	= cb_used = srv_used = uav_used = 0;
		if (auto rdef = ((dx::DXBC*)_shader)->GetBlob<dx::DXBC::ResourceDef>()) {
			int	x = 0;
			for (auto &i : rdef->Buffers())
				SetCB(graphics.FindConstBuffer(crc32(i.name.get(rdef)), i.size, i.num_variables), x++);
		} else {
			auto refl = GetReflection(((dx::DXBC*)_shader)->data());
			D3D12_SHADER_DESC				desc;
			refl->GetDesc(&desc);
			for (int i = 0; i < desc.ConstantBuffers; i++) {
				auto	cb = refl->GetConstantBufferByIndex(i);
				D3D12_SHADER_BUFFER_DESC	desc;
				cb->GetDesc(&desc);
				SetCB(graphics.FindConstBuffer(crc32(desc.Name), desc.Size, desc.Variables), i);
			}
		}
	}
	return true;
}

void Graphics::StageState::Flush(ID3D12Device *device, ID3D12GraphicsCommandList *cmd_list, int root_item, bool compute) {

	auto	&allocator	= frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];

	D3D12_CPU_DESCRIPTOR_HANDLE	dest_starts[8];
	UINT						dest_sizes[8];
	D3D12_CPU_DESCRIPTOR_HANDLE	srce_starts[8];
	UINT						srce_sizes[8];

	//cbv
	if (dirty & 1) {
		int	n = highest_set_index(cb_used) + 1, j = 0;
		int	d = allocator.alloc_index(n);
		for (int m = cb_used, i = 0; m; m >>= 1, i++) {
			if (m & 1) {
				srce_starts[j]	= cb[i]->FixBuffer();
				srce_sizes[j]	= 1;
				dest_starts[j]	= allocator.cpu(d + i);
				dest_sizes[j]	= 1;
				++j;
			}
		}
		device->CopyDescriptors(j, dest_starts, dest_sizes, j, srce_starts, srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		if (compute)
			cmd_list->SetComputeRootDescriptorTable(root_item + 0, allocator.gpu(d));
		else
			cmd_list->SetGraphicsRootDescriptorTable(root_item + 0, allocator.gpu(d));
	}

	//srv
	if (dirty & 2) {
		int	n = highest_set_index(srv_used) + 1, j = 0;
		int	d = allocator.alloc_index(n);
		for (int m = srv_used, i = 0; m; m >>= 1, i++) {
			if (m & 1) {
				srce_starts[j]	= srv[i];
				srce_sizes[j]	= 1;
				dest_starts[j]	= allocator.cpu(d + i);
				dest_sizes[j]	= 1;
				++j;
			}
		}
		device->CopyDescriptors(j, dest_starts, dest_sizes, j, srce_starts, srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		if (compute)
			cmd_list->SetComputeRootDescriptorTable(root_item + 1, allocator.gpu(d));
		else
			cmd_list->SetGraphicsRootDescriptorTable(root_item + 1, allocator.gpu(d));
	}

	//uav
	if (dirty & 4) {
		int	n = highest_set_index(uav_used) + 1, j = 0;
		int	d = allocator.alloc_index(n);
		for (int m = uav_used, i = 0; m; m >>= 1, i++) {
			if (m & 1) {
				srce_starts[j]	= uav[i];
				srce_sizes[j]	= 1;
				dest_starts[j]	= allocator.cpu(d + i);
				dest_sizes[j]	= 1;
				++j;
			}
		}
		device->CopyDescriptors(j, dest_starts, dest_sizes, j, srce_starts, srce_sizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		if (compute)
			cmd_list->SetComputeRootDescriptorTable(root_item + 2, allocator.gpu(d));
		else
			cmd_list->SetGraphicsRootDescriptorTable(root_item + 2, allocator.gpu(d));
	}
}

//-----------------------------------------------------------------------------
//	ComputeContext
//-----------------------------------------------------------------------------

ID3D12RootSignature *DefaultComputeSignature(ID3D12Device *device) {
	dx12::ROOT_SIGNATURE_DESC<4,0>	sigdesc(D3D12_ROOT_SIGNATURE_FLAG_NONE);

	dx12::DESCRIPTOR_RANGE ranges[4] = {
		{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, 0},
		{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0},
		{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, UINT_MAX, 0},
		{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, UINT_MAX, 1},
	};

	sigdesc.params[0].Table(1, ranges + 3, D3D12_SHADER_VISIBILITY_ALL);
	sigdesc.params[1].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_ALL);
	sigdesc.params[2].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_ALL);
	sigdesc.params[3].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_ALL);

	return sigdesc.Create(device);
}

void ComputeContext::Begin() {
	stage.Reset();

	cmd = q.alloc_cmd_list(graphics.device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	cmd->Reset();

	if (!root_signature)
		root_signature = DefaultComputeSignature(graphics.device);
	cmd->list->SetComputeRootSignature(root_signature);

	ID3D12DescriptorHeap	*heaps[]	= {
		frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
	};
	cmd->list->SetDescriptorHeaps(num_elements32(heaps), heaps);
}

void ComputeContext::End() {
	barriers.Flush(cmd->list);
	cmd->list->Close();

	q.Submit(cmd);
	cmd = 0;
}

void ComputeContext::DingDong() {
	End();
	Begin();
}

void ComputeContext::SetShader(const DX12Shader &s) {
	ISO_ASSERT(!s.Has(SS_VERTEX) && !s.Has(SS_MESH));

	sized_data	ss = s.sub[SS_PIXEL].raw();
	if (stage.Init(ss)) {
		com_ptr2<ID3D12PipelineState> &p = compute_pso_hash[ss];
		if (!p) {
			D3D12_COMPUTE_PIPELINE_STATE_DESC	state = {};
			state.pRootSignature		= root_signature;
			state.CS.BytecodeLength		= ss.length();
			state.CS.pShaderBytecode	= ss;
			state.NodeMask				= 0;
			state.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE;
			graphics.device->CreateComputePipelineState(&state, __uuidof(ID3D12PipelineState), (void**)&p);
		}
		cmd->list->SetPipelineState(p);
	}
	return;
}

void ComputeContext::SetShaderConstants(ShaderReg reg, const void *values) {
	if (!values)
		return;

	int	n = reg.count;
	switch (reg.type) {
		case SPT_VAL:
			if (stage.GetCB(reg.buffer)->SetConstant(reg.offset, values, n))
				stage.dirty |= 1;
			break;

		case SPT_SAMPLER:
			break;

		case SPT_TEXTURE: {
			ResourceHandle	*p = (ResourceHandle*)values;
			for (int i = 0; i < n; i++)
				SetSRV(p[i].get(), reg.offset + i);
			break;
		}
		case SPT_BUFFER: {
			ResourceHandle	*p = (ResourceHandle*)values;
			for (int i = 0; i < n; i++)
				SetSRV0(p[i].get(), reg.offset + i);
			break;
		}
	}
}
//-----------------------------------------------------------------------------
//	GraphicsContext
//-----------------------------------------------------------------------------

void GraphicsContext::Begin() {
	if (!root_signature) {
		dx12::ROOT_SIGNATURE_DESC<22, 1>	sigdesc(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		dx12::DESCRIPTOR_RANGE ranges[4] = {
			{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, 0},
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0},
			{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, UINT_MAX, 0},
			{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, UINT_MAX, 1},
		};

		sigdesc.params[0].Table(1, ranges + 3, D3D12_SHADER_VISIBILITY_PIXEL);
		sigdesc.params[1].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_PIXEL);
		sigdesc.params[2].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_PIXEL);
		sigdesc.params[3].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_PIXEL);

		sigdesc.params[4].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_VERTEX);
		sigdesc.params[5].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_VERTEX);
		sigdesc.params[6].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_VERTEX);

		sigdesc.params[7].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_GEOMETRY);
		sigdesc.params[8].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_GEOMETRY);
		sigdesc.params[9].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_GEOMETRY);

		sigdesc.params[10].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_HULL);
		sigdesc.params[11].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_HULL);
		sigdesc.params[12].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_HULL);

		sigdesc.params[13].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_DOMAIN);
		sigdesc.params[14].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_DOMAIN);
		sigdesc.params[15].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_DOMAIN);

		sigdesc.params[16].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_AMPLIFICATION);
		sigdesc.params[17].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_AMPLIFICATION);
		sigdesc.params[18].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_AMPLIFICATION);

		sigdesc.params[19].Table(1, ranges + 0, D3D12_SHADER_VISIBILITY_MESH);
		sigdesc.params[20].Table(1, ranges + 1, D3D12_SHADER_VISIBILITY_MESH);
		sigdesc.params[21].Table(1, ranges + 2, D3D12_SHADER_VISIBILITY_MESH);

		sigdesc.samplers[0].Init(0);
		sigdesc.samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		ISO_VERIFY(sigdesc.Create(graphics.device, &root_signature));
	}

	cmd = graphics.queue.alloc_cmd_list(graphics.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	cmd->Begin();

	ID3D12DescriptorHeap	*heaps[]	= {
		frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
		frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER],
	};
	cmd->list->SetDescriptorHeaps(num_elements(heaps), heaps);

	state.reset();
	clear(samplers);
	prim	= PRIM_UNKNOWN;

	for (auto &i : stages)
		i.Reset();

	cmd->list->SetGraphicsRootSignature(root_signature);
}

void GraphicsContext::End() {
	barriers.Flush(cmd->list);
	cmd->list->Close();

	graphics.queue.Submit(cmd);

	for (int i = 0; i < state.NumRenderTargets; i++)
		cmd->PendingRelease(render_buffers[i]);
	state.NumRenderTargets = 0;

	cmd = nullptr;
}

void GraphicsContext::FlushDeferred2() {
	using namespace dx12;
#if 1
	if (update.test(UPD_TARGETS))
		cmd->list->OMSetRenderTargets(state.NumRenderTargets, rtv, false, depth_buffer ? &dsv : 0);
#endif

	bool	mesh = !state.shader->Has(SS_VERTEX);

	if (update.test(UPD_GRAPHICS)) {
		com_ptr2<ID3D12PipelineState> &p = pso_hash[state];
		if (!p) {
		#if 1
			if (mesh) {
				auto	desc = MakePipelineDesc(
					PIPELINE::SUB<PIPELINE::ROOT_SIGNATURE>(root_signature)
					,PIPELINE::SUB<PIPELINE::PS>((const_memory_block)state.shader->sub[SS_PIXEL])
					,PIPELINE::SUB<PIPELINE::MS>((const_memory_block)state.shader->sub[SS_LOCAL])
					,PIPELINE::SUB<PIPELINE::AS>((const_memory_block)state.shader->sub[SS_HULL])
					,PIPELINE::SUB<PIPELINE::BLEND>(state)
					//,PIPELINE::SUB<PIPELINE::PRIMITIVE_TOPOLOGY>(D3D12_PRIMITIVE_TOPOLOGY_TYPE(state.PrimitiveTopologyType))
					,PIPELINE::SUB<PIPELINE::RASTERIZER>(state.rasterizer)
					,PIPELINE::SUB<PIPELINE::DEPTH_STENCIL>(state.depthstencil)
					,PIPELINE::SUB<PIPELINE::RENDER_TARGET_FORMATS>(state)
					,PIPELINE::SUB<PIPELINE::DEPTH_STENCIL_FORMAT>(state.DSVFormat)
					,PIPELINE::SUB<PIPELINE::SAMPLE_DESC>(state)
				);
				graphics.device.as<ID3D12Device2>()->CreatePipelineState(dx12::PIPELINE_STATE_STREAM_DESC(desc), COM_CREATE(&p));
			} else {
				auto	desc = MakePipelineDesc(
					PIPELINE::SUB<PIPELINE::ROOT_SIGNATURE>(root_signature)
					,PIPELINE::SUB<PIPELINE::PS>((const_memory_block)state.shader->sub[SS_PIXEL])
					,PIPELINE::SUB<PIPELINE::VS>((const_memory_block)state.shader->sub[SS_VERTEX])
					,PIPELINE::SUB<PIPELINE::DS>((const_memory_block)state.shader->sub[SS_LOCAL])
					,PIPELINE::SUB<PIPELINE::HS>((const_memory_block)state.shader->sub[SS_HULL])
					,PIPELINE::SUB<PIPELINE::GS>((const_memory_block)state.shader->sub[SS_GEOMETRY])
					,PIPELINE::SUB<PIPELINE::BLEND>(state)
					,PIPELINE::SUB<PIPELINE::PRIMITIVE_TOPOLOGY>(D3D12_PRIMITIVE_TOPOLOGY_TYPE(state.PrimitiveTopologyType))
					//,PIPELINE::SUB<PIPELINE::STREAM_OUTPUT>(*state.stream_output)
					,PIPELINE::SUB<PIPELINE::INPUT_LAYOUT>(*state.input_layout)
					,PIPELINE::SUB<PIPELINE::RASTERIZER>(state.rasterizer)
					,PIPELINE::SUB<PIPELINE::DEPTH_STENCIL>(state.depthstencil)
					,PIPELINE::SUB<PIPELINE::RENDER_TARGET_FORMATS>(state)
					,PIPELINE::SUB<PIPELINE::DEPTH_STENCIL_FORMAT>(state.DSVFormat)
					,PIPELINE::SUB<PIPELINE::SAMPLE_DESC>(state)
				);
				graphics.device.as<ID3D12Device2>()->CreatePipelineState(dx12::PIPELINE_STATE_STREAM_DESC(desc), COM_CREATE(&p));
			}
		#else
			D3D12_GRAPHICS_PIPELINE_STATE_DESC	desc = {};
			state.GetDesc(desc);
			desc.pRootSignature = root_signature;
			graphics.device->CreateGraphicsPipelineState(&desc, COM_CREATE(&p));
		#endif
		}
		cmd->list->SetPipelineState(p);
	}
	if (update & UPD_SHADER_ANY) {
		if (update.test(UPD_SHADER_PIXEL))
			stages[SS_PIXEL].Flush(graphics.device, cmd->list, 1, false);
		if (update.test(UPD_SHADER_VERTEX))
			stages[SS_VERTEX].Flush(graphics.device, cmd->list, 4, false);
		if (update.test(UPD_SHADER_GEOMETRY))
			stages[SS_GEOMETRY].Flush(graphics.device, cmd->list, 7, false);
		if (update.test(UPD_SHADER_HULL))
			stages[SS_HULL].Flush(graphics.device, cmd->list, mesh ? 16 : 10, false);
		if (update.test(UPD_SHADER_LOCAL))
			stages[SS_LOCAL].Flush(graphics.device, cmd->list, mesh ? 19 : 13, false);
	}

	if (int m = (update / _UPD_SAMPLER) & 0xff) {
		auto	&allocator	= frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
		int		n = highest_set_index(m) + 1;
		int		d = allocator.alloc_index(n);
		cmd->list->SetGraphicsRootDescriptorTable(0, allocator.gpu(d));

		for (auto *s = samplers; m; m >>= 1, ++s, ++d) {
			if (m & 1)
				graphics.device->CreateSampler(s, allocator.cpu(d));
		}
	}
	update.clear_all(UPD_TARGETS | UPD_GRAPHICS | UPD_SHADER_ANY | UPD_SAMPLER_ALL);
}

void GraphicsContext::SetWindow(const rect &rect) {
	viewport.TopLeftX	= rect.a.x;
	viewport.TopLeftY	= rect.a.y;
	point	size		= rect.extent();
	viewport.Width		= size.x;
	viewport.Height		= size.y;
	viewport.MinDepth	= 0;
	viewport.MaxDepth	= 1.0f;
	cmd->list->RSSetViewports(1, &viewport);
	cmd->list->RSSetScissorRects(1, dx12::RECT(rect.a.x, rect.a.y, rect.b.x, rect.b.y));
}

rect GraphicsContext::GetWindow() {
	return rect::with_length(point{viewport.TopLeftX, viewport.TopLeftY}, point{viewport.Width, viewport.Height});
}

void GraphicsContext::Clear(param(colour) col, bool zbuffer) {
	cmd->list->ClearRenderTargetView(rtv[0], (float*)&col, 0, 0);
	if (zbuffer)
		ClearZ();
}

void GraphicsContext::ClearZ() {
	cmd->list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0, 0, 0, NULL);
}

void GraphicsContext::_SetRenderTarget(int i, Resource *r, const D3D12_RENDER_TARGET_VIEW_DESC &desc) {
	rtv[i]					= frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].alloc();
	state.RTVFormats[i]		= desc.Format;
	state.NumRenderTargets	= max(state.NumRenderTargets, i + 1);
	graphics.device->CreateRenderTargetView(*r, &desc, rtv[i]);
	barriers.Transition(cmd->list, r, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmd->PendingRelease(render_buffers[i]);
}

void GraphicsContext::_SetZBuffer(Resource *r, const D3D12_DEPTH_STENCIL_VIEW_DESC &desc) {
	dsv						= frame_descriptor_alloc[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].alloc();
	state.DSVFormat			= desc.Format;
	graphics.device->CreateDepthStencilView(*r, &desc, dsv);
	barriers.Transition(cmd->list, r, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	cmd->PendingRelease(depth_buffer);
}

void GraphicsContext::_ClearRenderTarget(int i) {
	cmd->PendingRelease(render_buffers[i]);

	if (i == state.NumRenderTargets - 1)  {
		while (i && !render_buffers[i - 1])
			i--;
		state.NumRenderTargets = i;
	}
}

void GraphicsContext::_ClearZBuffer() {
	cmd->PendingRelease(depth_buffer);
}

void GraphicsContext::SetRenderTarget(Surface&& s, RenderTarget i) {
	if (Resource *r = s.get()) {
		if (i == RT_DEPTH) {
			_SetZBuffer(r, s.DepthDesc());
			depth_buffer		= move(s);
		} else {
			if (i == RT_COLOUR0)
				SetWindow(rect(zero, s.GetSize()));
			_SetRenderTarget(i, r, s.RenderDesc());
			render_buffers[i]	= move(s);
		}
	} else {
		if (i == RT_DEPTH)
			_ClearZBuffer();
		else
			_ClearRenderTarget(i);
	}
	update.set_all(UPD_MISC|UPD_TARGETS);
}

void GraphicsContext::SetZBuffer(Surface&& s) {
	if (Resource *r = s.get()) {
		_SetZBuffer(r, s.DepthDesc());
		depth_buffer	= move(s);
	} else {
		_ClearZBuffer();
	}
	update.set_all(UPD_MISC|UPD_TARGETS);
}

void GraphicsContext::SetRenderTarget(const Surface& s, RenderTarget i) {
	if (Resource *r = s.get()) {
		if (i == RT_DEPTH) {
			_SetZBuffer(r, s.DepthDesc());
			depth_buffer	= s;
		} else {
			if (i == RT_COLOUR0)
				SetWindow(rect(zero, s.GetSize()));
			_SetRenderTarget(i, r, s.RenderDesc());
			render_buffers[i]	= s;
		}
	} else {
		if (i == RT_DEPTH)
			_ClearZBuffer();
		else
			_ClearRenderTarget(i);
	}
	update.set_all(UPD_MISC|UPD_TARGETS);
}

void GraphicsContext::SetZBuffer(const Surface& s) {
	if (Resource *r = s.get()) {
		_SetZBuffer(r, s.DepthDesc());
		depth_buffer	= s;
	} else {
		_ClearZBuffer();
	}
	update.set_all(UPD_MISC|UPD_TARGETS);
}

Surface	GraphicsContext::GetRenderTarget(int i) {
	if (i == RT_DEPTH)
		return depth_buffer;
	else
		return render_buffers[i];
}

Surface	GraphicsContext::GetZBuffer() {
	return depth_buffer;
}

void GraphicsContext::SetShader(const DX12Shader &s) {
	if (!s.Has(SS_VERTEX) && !s.Has(SS_MESH)) {
		sized_data	ss = s.sub[SS_PIXEL].raw();
		if (stages[SS_COMPUTE].Init(ss)) {
			if (!compute_root_signature)
				compute_root_signature = DefaultComputeSignature(graphics.device);
			cmd->list->SetComputeRootSignature(compute_root_signature);

			com_ptr2<ID3D12PipelineState> &p = compute_pso_hash[ss];
			if (!p) {
				D3D12_COMPUTE_PIPELINE_STATE_DESC	state = {};
				state.pRootSignature		= compute_root_signature;
				state.CS.BytecodeLength		= ss.length();
				state.CS.pShaderBytecode	= ss;
				state.NodeMask				= 0;
				state.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE;
				graphics.device->CreateComputePipelineState(&state, __uuidof(ID3D12PipelineState), (void**)&p);
			}
			cmd->list->SetPipelineState(p);
		}
		return;
	}

	uint32	set	=
		(stages[SS_PIXEL].Init(s.sub[SS_PIXEL].raw())		? UPD_SHADER_PIXEL		: 0)
	|	(stages[SS_VERTEX].Init(s.sub[SS_VERTEX].raw())		? UPD_SHADER_VERTEX		: 0)
	|	(stages[SS_GEOMETRY].Init(s.sub[SS_GEOMETRY].raw())	? UPD_SHADER_GEOMETRY	: 0)
	|	(stages[SS_HULL].Init(s.sub[SS_HULL].raw())			? UPD_SHADER_HULL		: 0)
	|	(stages[SS_LOCAL].Init(s.sub[SS_LOCAL].raw())		? UPD_SHADER_LOCAL		: 0);

	if (set) {
		update.set_all(set | UPD_MISC);
		state.shader = &s;
	}
}

void GraphicsContext::SetShaderConstants(ShaderReg reg, const void *values) {
	if (!values)
		return;

	int	n = reg.count;
	switch (reg.type) {
		case SPT_VAL:
			if (stages[reg.stage].GetCB(reg.buffer)->SetConstant(reg.offset, values, n)) {
				update.set(UPDATE(UPD_SHADER << reg.stage));
				stages[reg.stage].dirty |= 1;
			}
			break;

		case SPT_SAMPLER: {
			D3D12_SAMPLER_DESC	*p = (D3D12_SAMPLER_DESC*)values;
			for (int i = 0; i < n; i++)
				samplers[reg.offset + i] = p[i];
			update.set_all(bits(n) * _UPD_SAMPLER);
			break;
		}

		case SPT_TEXTURE: {
			ResourceHandle	*p = (ResourceHandle*)values;
			for (int i = 0; i < n; i++)
				SetSRV((ShaderStage)reg.stage, p[i].get(), reg.offset + i);
			break;
		}
		case SPT_BUFFER: {
			ResourceHandle	*p = (ResourceHandle*)values;
			for (int i = 0; i < n; i++)
				SetSRV0((ShaderStage)reg.stage, p[i].get(), reg.offset + i);
			break;
		}
	}
}

void GraphicsContext::DrawMesh(uint32 dimx, uint32 dimy, uint32 dimz) {
	FlushDeferred();
	cmd->list.as<ID3D12GraphicsCommandList6>()->DispatchMesh(dimx, dimy, dimz);
}

void GraphicsContext::Dispatch(uint32 dimx, uint32 dimy, uint32 dimz) {
	if (update.test(UPD_SHADER_COMPUTE))
		stages[SS_COMPUTE].Flush(graphics.device, cmd->list, 1, true);

	barriers.Flush(cmd->list);
	cmd->list->Dispatch(dimx, dimy, dimz);
}

void	GraphicsContext::MapStreamOut(uint8 b0, uint8 b1, uint8 b2, uint8 b3)	{}
void	GraphicsContext::SetStreamOut(int i, void *start, uint32 size, uint32 stride)	{}
void	GraphicsContext::GetStreamOut(int i, uint64 *pos)	{}
void	GraphicsContext::FlushStreamOut()	{}

//-----------------------------------------------------------------------------
//	_ImmediateStream
//-----------------------------------------------------------------------------

_VertexBuffer		_ImmediateStream::vb;
void*				_ImmediateStream::vbp;
int					_ImmediateStream::vbi, _ImmediateStream::vbsize;

_ImmediateStream::_ImmediateStream(GraphicsContext &ctx, PrimType prim, int count, uint32 vert_size) : ctx(ctx), count(count), prim(prim) {
	int	size = vert_size * count;
	if (size * 2 > vbsize) {
		vbsize = size * 3;
		vb.Init(vbsize, MEM_CPU_WRITE);
		vbi = 0;
		vb->get()->Map(0, NULL, &vbp);
	} else if (vbi + size > vbsize) {
		vbi = 0;
	}

	p = (char*)vbp + vbi;
	ctx.SetVertices(0, vb, vert_size, vbi);

	vbi += vert_size * count;
}
void _ImmediateStream::Draw() {
	ctx.DrawVertices(prim, 0, count);
}

//-----------------------------------------------------------------------------
//	Shaders
//-----------------------------------------------------------------------------

D3D12_SAMPLER_DESC	default_sampler = {
	D3D12_FILTER_MIN_MAG_MIP_LINEAR,	//D3D11_FILTER Filter;
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//D3D11_TEXTURE_ADDRESS_MODE AddressU;
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//D3D11_TEXTURE_ADDRESS_MODE AddressV;
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//D3D11_TEXTURE_ADDRESS_MODE AddressW;
	0,									//FLOAT MipLODBias;
	1,									//UINT	MaxAnisotropy;
	D3D12_COMPARISON_FUNC_NEVER,		//D3D11_COMPARISON_FUNC ComparisonFunc;
	{0, 0, 0, 0},						//FLOAT BorderColor[ 4 ];
	0, 1e38f,							//FLOAT MinLOD, MaxLOD;
};

void InitShaderStage(ID3D12Device *device, DX12Shader::SubShader &sub) {
}

void Init(DX12Shader *x, void *physram) {
	ID3D12Device *device	= graphics.device;
	if (!x->Has(SS_VERTEX) && !x->Has(SS_MESH)) {
		InitShaderStage(device, x->sub[SS_PIXEL]);
	} else {
		InitShaderStage(device, x->sub[SS_PIXEL]);
		InitShaderStage(device, x->sub[SS_VERTEX]);
		InitShaderStage(device, x->sub[SS_GEOMETRY]);
		InitShaderStage(device, x->sub[SS_HULL]);
		InitShaderStage(device, x->sub[SS_LOCAL]);
	}
}

void DeInit(DX12Shader *x) {
	for (int i = 0; i < SS_COUNT; i++)
		x->sub[i].clear<IUnknown>();
}

int ShaderParameterIterator::ArrayCount(const char *begin, const char *&end) {
	while (*--end != '[');

	int			len			= end - begin;
	int			index0		= from_string<int>(end + 1);
	const char *last_name	= 0;

	while (!bindings.empty()) {
		last_name	= bindings.pop_front_value().rdef.name.get(rdef);
		if (strncmp(begin, last_name, len) != 0)
			break;
	}

	int		index1 = from_string<int>(last_name + len + 1);
	return index1 - index0 + 1;
}

ShaderParameterIterator::~ShaderParameterIterator() {
	if (refl)
		refl->Release();
}

void ShaderParameterIterator::Next() {
	for (;;) {
		while (vars.empty()) {
			while (bindings.empty()) {
				if (++stage == SS_COUNT)
					return;

				if (dx::DXBC *dxbc = (dx::DXBC*)shader.sub[stage].raw()) {
					if (rdef = dxbc->GetBlob<dx::RD11>()) {
						bindings = rdef->Bindings();

					} else if (auto psv = dxbc->GetBlob<dx::PSV_Resources>()) {
						if (refl)
							refl->Release();
						refl	= GetReflection(dxbc->data()).detach();
						//bindings = psv->GetResourceBindings<0>();
						D3D12_SHADER_DESC				desc;
						refl->GetDesc(&desc);
						bindings = make_range_n(strided((const void*)0, 1), desc.BoundResources);
					}
				}
			}

			if (rdef) {
				auto &bind	= bindings.pop_front_value().rdef;
				name = bind.name.get(rdef);

				switch (bind.type) {
					case RDEF::Binding::CBUFFER: {
						cbuff_index	= bind.bind;
						auto	b	= rdef->buffers.get(rdef)[cbuff_index];
						vars		= rdef->Variables(b);
						break;
					}

					case RDEF::Binding::TEXTURE:
						val	= 0;
						if (bind.dim == dx::RESOURCE_DIMENSION_BUFFER) {
							reg = ShaderReg(bind.bind, bind.bind_count, 0, ShaderStage(stage), SPT_BUFFER);
						} else {
							const char	*end	= str(name).end();
							int			count	= bind.bind_count;
							if (end[-1] == ']')
								count = ArrayCount(name, end);
							if (end[-2] == '_' && end[-1] == 't')
								name = temp_name = str(name, end - 2);
							reg = ShaderReg(bind.bind, count, 0, ShaderStage(stage), SPT_TEXTURE);
						}
						return;

					case RDEF::Binding::SAMPLER: {
						const char	*end	= str(name).end();
						int			count	= bind.bind_count;
						if (end[-1] == ']')
							count = ArrayCount(name, end);
	//					if (end[-2] == '_' && end[-1] == 's')
	//						name = temp_name = str(name, end - 2);

						reg		= ShaderReg(bind.bind, count, 0, ShaderStage(stage), SPT_SAMPLER);
						val		= bind.samples ? (void*)((uint8*)&bind.samples + bind.samples) : (void*)&default_sampler;
						return;
					}

					default:
						reg = ShaderReg(bind.bind, bind.bind_count, 0, ShaderStage(stage), SPT_BUFFER);
						val	= 0;
						return;
				}
			} else {
				D3D12_SHADER_INPUT_BIND_DESC	bind;
				refl->GetResourceBindingDesc(intptr_t(&bindings.pop_front_value()), &bind);
				
				name	= bind.Name;
				val		= 0;
				switch (bind.Type) {
					case D3D_SIT_CBUFFER: {
						cbuff_index	= bind.BindPoint;
						cb = refl->GetConstantBufferByIndex(cbuff_index);
						D3D12_SHADER_BUFFER_DESC	desc;
						cb->GetDesc(&desc);
						vars = make_range_n(strided((const void*)0, 1), desc.Variables);
						break;
					}

					case D3D_SIT_TBUFFER:
					case D3D_SIT_STRUCTURED:
						reg = ShaderReg(bind.BindPoint, bind.BindCount, 0, ShaderStage(stage), SPT_BUFFER);
						return;

					case D3D_SIT_TEXTURE: {
						const char	*end	= str(name).end();
						int			count	= bind.BindCount;
						if (end[-1] == ']')
							count = ArrayCount(name, end);
						if (end[-2] == '.' && end[-1] == 't')
							name = temp_name = str(name, end - 2);
						reg = ShaderReg(bind.BindPoint, count, 0, ShaderStage(stage), SPT_TEXTURE);
						return;
					}

					case D3D_SIT_SAMPLER:
						reg = ShaderReg(bind.BindPoint, bind.BindCount, 0, ShaderStage(stage), SPT_SAMPLER);
						return;
				}
			}
		}

		auto	&v = vars.pop_front_value();
		if (rdef) {
			if (v.flags & v.USED) {
				name	= v.name.get(rdef);
				val		= v.def.get(rdef);
				reg		= ShaderReg(v.offset, v.size, cbuff_index, ShaderStage(stage), SPT_VAL);
				return;
			}
		} else {
			ID3D12ShaderReflectionVariable*	var = cb->GetVariableByIndex(intptr_t(&v));
			D3D12_SHADER_VARIABLE_DESC		desc;
			var->GetDesc(&desc);
			if (desc.uFlags &  D3D_SVF_USED) {
				name	= desc.Name;
				val		= desc.DefaultValue;
				reg		= ShaderReg(desc.StartOffset, desc.Size, cbuff_index, ShaderStage(stage), SPT_VAL);
				return;
			}
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
			if (auto rdef	= dxbc->GetBlob<dx::RD11>()) {
				for (auto &i : rdef->Bindings())
					total += int(i.type != RDEF::Binding::CBUFFER);

				for (auto &&i : rdef->Buffers()) {
					for (auto &v : rdef->Variables(i)) {
						if (v.flags & v.USED)
							++total;
					}
				}

			} else if (auto psv = dxbc->GetBlob<dx::PSV_Resources>()) {
				auto	refl	= GetReflection(dxbc->data()).detach();
				D3D12_SHADER_DESC	desc;
				refl->GetDesc(&desc);
				total += desc.BoundResources - desc.ConstantBuffers;

				for (int i = 0; i < desc.ConstantBuffers; i++) {
					auto	cb = refl->GetConstantBufferByIndex(i);
					D3D12_SHADER_BUFFER_DESC	desc;
					cb->GetDesc(&desc);
					total += desc.Variables;
				}
			}

		}
	}
	return total;
}

}