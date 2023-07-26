#include "dx_gpu.h"
#include "extra/xml.h"
#include "windows/text_control.h"
#include "dx/dxgi_read.h"
#include "filename.h"
#include "conversion/channeluse.h"

using namespace app;

namespace iso { namespace dx {

#define CT(T)	ctypes.get_type<T>()

const C_type *dxgi_c_types[] = {
	0,						//DXGI_FORMAT_UNKNOWN					= 0,
	0,						//DXGI_FORMAT_R32G32B32A32_TYPELESS		= 1,
	CT(float[4]),			//DXGI_FORMAT_R32G32B32A32_FLOAT		= 2,
	CT(uint32[4]),			//DXGI_FORMAT_R32G32B32A32_UINT			= 3,
	CT(int[4]),				//DXGI_FORMAT_R32G32B32A32_SINT			= 4,
	0,						//DXGI_FORMAT_R32G32B32_TYPELESS		= 5,
	CT(float[3]),			//DXGI_FORMAT_R32G32B32_FLOAT			= 6,
	CT(uint32[3]),			//DXGI_FORMAT_R32G32B32_UINT			= 7,
	CT(int[3]),				//DXGI_FORMAT_R32G32B32_SINT			= 8,
	0,						//DXGI_FORMAT_R16G16B16A16_TYPELESS		= 9,
	CT(float16[4]),			//DXGI_FORMAT_R16G16B16A16_FLOAT		= 10,
	CT(unorm16[4]),			//DXGI_FORMAT_R16G16B16A16_UNORM		= 11,
	CT(uint16[4]),			//DXGI_FORMAT_R16G16B16A16_UINT			= 12,
	CT(norm16[4]),			//DXGI_FORMAT_R16G16B16A16_SNORM		= 13,
	CT(int16[4]),			//DXGI_FORMAT_R16G16B16A16_SINT			= 14,
	0,						//DXGI_FORMAT_R32G32_TYPELESS			= 15,
	CT(float[2]),			//DXGI_FORMAT_R32G32_FLOAT				= 16,
	CT(uint32[2]),			//DXGI_FORMAT_R32G32_UINT				= 17,
	CT(int[2]),				//DXGI_FORMAT_R32G32_SINT				= 18,
	0,						//DXGI_FORMAT_R32G8X24_TYPELESS			= 19,
	0,						//DXGI_FORMAT_D32_FLOAT_S8X24_UINT		= 20,
	0,						//DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS	= 21,
	0,						//DXGI_FORMAT_X32_TYPELESS_G8X24_UINT	= 22,
	0,						//DXGI_FORMAT_R10G10B10A2_TYPELESS		= 23,
	CT(unorm4_10_10_10_2),	//DXGI_FORMAT_R10G10B10A2_UNORM			= 24,
	CT(uint4_10_10_10_2),	//DXGI_FORMAT_R10G10B10A2_UINT			= 25,
	CT(float3_11_11_10),	//DXGI_FORMAT_R11G11B10_FLOAT			= 26,
	0,						//DXGI_FORMAT_R8G8B8A8_TYPELESS			= 27,
	CT(unorm8[4]),			//DXGI_FORMAT_R8G8B8A8_UNORM			= 28,
	CT(unorm8[4]),			//DXGI_FORMAT_R8G8B8A8_UNORM_SRGB		= 29,
	CT(uint8[4]),			//DXGI_FORMAT_R8G8B8A8_UINT				= 30,
	CT(norm8[4]),			//DXGI_FORMAT_R8G8B8A8_SNORM			= 31,
	CT(int8[4]),			//DXGI_FORMAT_R8G8B8A8_SINT				= 32,
	0,						//DXGI_FORMAT_R16G16_TYPELESS			= 33,
	CT(float16[2]),			//DXGI_FORMAT_R16G16_FLOAT				= 34,
	CT(unorm16[2]),			//DXGI_FORMAT_R16G16_UNORM				= 35,
	CT(uint16[2]),			//DXGI_FORMAT_R16G16_UINT				= 36,
	CT(norm16[2]),			//DXGI_FORMAT_R16G16_SNORM				= 37,
	CT(int16[2]),			//DXGI_FORMAT_R16G16_SINT				= 38,
	0,						//DXGI_FORMAT_R32_TYPELESS				= 39,
	0,						//DXGI_FORMAT_D32_FLOAT					= 40,
	CT(float),				//DXGI_FORMAT_R32_FLOAT					= 41,
	CT(uint32),				//DXGI_FORMAT_R32_UINT					= 42,
	CT(int),				//DXGI_FORMAT_R32_SINT					= 43,
	0,						//DXGI_FORMAT_R24G8_TYPELESS			= 44,
	0,						//DXGI_FORMAT_D24_UNORM_S8_UINT			= 45,
	0,						//DXGI_FORMAT_R24_UNORM_X8_TYPELESS		= 46,
	0,						//DXGI_FORMAT_X24_TYPELESS_G8_UINT		= 47,
	0,						//DXGI_FORMAT_R8G8_TYPELESS				= 48,
	CT(unorm8[2]),			//DXGI_FORMAT_R8G8_UNORM				= 49,
	CT(uint8[2]),			//DXGI_FORMAT_R8G8_UINT					= 50,
	CT(norm8[2]),			//DXGI_FORMAT_R8G8_SNORM				= 51,
	CT(int8[2]),			//DXGI_FORMAT_R8G8_SINT					= 52,
	0,						//DXGI_FORMAT_R16_TYPELESS				= 53,
	CT(float16),			//DXGI_FORMAT_R16_FLOAT					= 54,
	0,						//DXGI_FORMAT_D16_UNORM					= 55,
	CT(unorm16),			//DXGI_FORMAT_R16_UNORM					= 56,
	CT(uint16),				//DXGI_FORMAT_R16_UINT					= 57,
	CT(norm16),				//DXGI_FORMAT_R16_SNORM					= 58,
	CT(int16),				//DXGI_FORMAT_R16_SINT					= 59,
	0,						//DXGI_FORMAT_R8_TYPELESS				= 60,
	CT(unorm8),				//DXGI_FORMAT_R8_UNORM					= 61,
	CT(uint8),				//DXGI_FORMAT_R8_UINT					= 62,
	CT(norm8),				//DXGI_FORMAT_R8_SNORM					= 63,
	CT(int8),				//DXGI_FORMAT_R8_SINT					= 64,
	0,						//DXGI_FORMAT_A8_UNORM					= 65,
	0,						//DXGI_FORMAT_R1_UNORM					= 66,
	0,						//DXGI_FORMAT_R9G9B9E5_SHAREDEXP		= 67,
	0,						//DXGI_FORMAT_R8G8_B8G8_UNORM			= 68,
	0,						//DXGI_FORMAT_G8R8_G8B8_UNORM			= 69,
	0,						//DXGI_FORMAT_BC1_TYPELESS				= 70,
	0,						//DXGI_FORMAT_BC1_UNORM					= 71,
	0,						//DXGI_FORMAT_BC1_UNORM_SRGB			= 72,
	0,						//DXGI_FORMAT_BC2_TYPELESS				= 73,
	0,						//DXGI_FORMAT_BC2_UNORM					= 74,
	0,						//DXGI_FORMAT_BC2_UNORM_SRGB			= 75,
	0,						//DXGI_FORMAT_BC3_TYPELESS				= 76,
	0,						//DXGI_FORMAT_BC3_UNORM					= 77,
	0,						//DXGI_FORMAT_BC3_UNORM_SRGB			= 78,
	0,						//DXGI_FORMAT_BC4_TYPELESS				= 79,
	0,						//DXGI_FORMAT_BC4_UNORM					= 80,
	0,						//DXGI_FORMAT_BC4_SNORM					= 81,
	0,						//DXGI_FORMAT_BC5_TYPELESS				= 82,
	0,						//DXGI_FORMAT_BC5_UNORM					= 83,
	0,						//DXGI_FORMAT_BC5_SNORM					= 84,
	0,						//DXGI_FORMAT_B5G6R5_UNORM				= 85,
	0,						//DXGI_FORMAT_B5G5R5A1_UNORM			= 86,
	CT(unorm8[4]),			//DXGI_FORMAT_B8G8R8A8_UNORM			= 87,
	0,						//DXGI_FORMAT_B8G8R8X8_UNORM			= 88,
	0,						//DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM= 89,
	0,						//DXGI_FORMAT_B8G8R8A8_TYPELESS			= 90,
	0,						//DXGI_FORMAT_B8G8R8A8_UNORM_SRGB		= 91,
	0,						//DXGI_FORMAT_B8G8R8X8_TYPELESS			= 92,
	0,						//DXGI_FORMAT_B8G8R8X8_UNORM_SRGB		= 93,
	0,						//DXGI_FORMAT_BC6H_TYPELESS				= 94,
	0,						//DXGI_FORMAT_BC6H_UF16					= 95,
	0,						//DXGI_FORMAT_BC6H_SF16					= 96,
	0,						//DXGI_FORMAT_BC7_TYPELESS				= 97,
	0,						//DXGI_FORMAT_BC7_UNORM					= 98,
	0,						//DXGI_FORMAT_BC7_UNORM_SRGB			= 99,
	0,						//DXGI_FORMAT_AYUV						= 100,
	0,						//DXGI_FORMAT_Y410						= 101,
	0,						//DXGI_FORMAT_Y416						= 102,
	0,						//DXGI_FORMAT_NV12						= 103,
	0,						//DXGI_FORMAT_P010						= 104,
	0,						//DXGI_FORMAT_P016						= 105,
	0,						//DXGI_FORMAT_420_OPAQUE				= 106,
	0,						//DXGI_FORMAT_YUY2						= 107,
	0,						//DXGI_FORMAT_Y210						= 108,
	0,						//DXGI_FORMAT_Y216						= 109,
	0,						//DXGI_FORMAT_NV11						= 110,
	0,						//DXGI_FORMAT_AI44						= 111,
	0,						//DXGI_FORMAT_IA44						= 112,
	0,						//DXGI_FORMAT_P8						= 113,
	0,						//DXGI_FORMAT_A8P8						= 114,
	0,						//DXGI_FORMAT_B4G4R4A4_UNORM			= 115,
	0,						//										= 116,
	0,						//										= 117,
	0,						//										= 118,
	0,						//										= 119,
	0,						//										= 120,
	0,						//										= 121,
	0,						//										= 122,
	0,						//										= 123,
	0,						//										= 124,
	0,						//										= 125,
	0,						//										= 126,
	0,						//										= 127,
	0,						//										= 128,
	0,						//										= 129,
	0,						//DXGI_FORMAT_P208						= 130,
	0,						//DXGI_FORMAT_V208						= 131,
	0,						//DXGI_FORMAT_V408						= 132,
};

const C_type *to_c_type(DXGI_FORMAT f) {
	return dxgi_c_types[f];
}

const C_type *to_c_type(DXGI_COMPONENTS f) {
	static const C_type *c_types[8][3] = {
		//8 bits		16				32
		{0,				0,				0,			},	//TYPELESS,
		{0,				CT(float16),	CT(float),	},	//FLOAT,
		{0,				CT(float16),	CT(float),	},	//UFLOAT,
		{CT(uint8),		CT(uint16),		CT(uint32),	},	//UINT,
		{CT(int8),		CT(int16),		CT(int32),	},	//SINT,
		{CT(unorm8),	CT(unorm16),	CT(unorm32),},	//UNORM,
		{CT(norm8),		CT(norm16),		CT(norm32),	},	//SNORM,
		{0,				0,				0,			},	//SRGB
	};

	auto	info = f.GetLayoutInfo();
	if (info.bits == 0)
		return  nullptr;

	const C_type	*c = c_types[f.Type()][log2(info.bits) - 3];
	if (info.comps > 1)
		c = ctypes.add(C_type_array(c, info.comps));
	return c;
}

static const C_type *to_c_type(const SIG::Element &e) {
	static const C_type *c_types[4] = {
		0,
		CT(uint32),
		CT(int32),
		CT(float),
	};
	uint32	n = count_bits(e.mask);
	auto	t = c_types[e.component_type];
	return n == 1 ? t : ctypes.add(C_type_array(t, n));
}
/*
template<typename T> const C_type *to_c_type(const T *elements) {
	for (auto &i : elements->Elements()) {
		string	name	= i.name.get(elements);
		if (i.semantic_index)
			name << i.semantic_index;
		for (int m = i.mask; m; m = clear_lowest(m))
			nc	= c->AddColumn(nc, string(name) << '.' << "xyzw"[lowest_set_index(m)], 50, nc & 1 ? col1 : col0);
	}
	return nc;
}
*/
const C_type *sig_to_c_type(const dx::Signature &sig) {
	C_type_struct	type;
	for (auto &i : sig) {
		string	name	= i.name.get(sig);
		if (i.semantic_index)
			name << i.semantic_index;
		
		int		r	= i.register_num;
		if (r < 0) {
			switch (i.system_value) {
				default: ISO_ASSERT(0);
				case SV_DEPTH:	r = Decls::oDepth; break;
			}
		}

		type.add_atoffset(name, to_c_type(i), r * 16 + lowest_set_index(i.mask) * 4);
	}
	return ctypes.add(move(type));
}

ChannelUse::chans GetChannels(DXGI_COMPONENTS format) {
	ChannelUse::chans c;
	for (int i = 0; i < 4; i++)
		c[i] = format.GetChan(i);
	return c;
}

//template<typename T> const void *copy_slices(const block<T, 3> &dest, const void *srce, DXGI_COMPONENTS format, uint64 depth_stride) {
//	srce = copy_slices(dest, srce, format.Layout(), format.Type(), depth_stride);
//	RearrangeChannels(dest, GetChannels(format));
//	return srce;
//}

ISO_ptr_machine<void> GetBitmap(const char *name, const void *srce, DXGI_COMPONENTS format, int width, int height, int depth, int mips, int flags) {
	if (height == 0)
		height = 1;

	uint64	depth_stride = size2D(format, width, height, mips);

	if (mips > 1)
		flags |= BMF_MIPS;

	if (depth == 0)
		depth = 1;

	if (format.IsHDR()) {
		ISO_ptr_machine<HDRbitmap64>	bm(name);
		bm->Create(width, height * depth, flags, depth);

		if (mips > 1) {
			bm->SetMips(mips);
			for (int i = 0; i < mips; i++)
				srce = copy_slices(bm->Mip3D(i), srce, format, depth_stride);

		} else {
			copy_slices(bm->All3D(), srce, format, depth_stride);
		}
		return bm;

	} else {
		ISO_ptr_machine<bitmap64>	bm(name);
		bm->Create(width, height * depth, flags, depth);

		if (mips > 1) {
			bm->SetMips(mips);
			for (int i = 0; i < mips; i++)
				srce = copy_slices(bm->Mip3D(i), srce, format, depth_stride);

		} else {
			copy_slices(bm->All3D(), srce, format, depth_stride);
		}
		return bm;
	}
}

ISO_ptr_machine<void> GetBitmap(const char *name, Resource &rec) {
	switch (rec.dim) {
		case RESOURCE_DIMENSION_TEXTURE1D:
			return GetBitmap(name, rec, rec.format, rec.width, rec.depth, 1, rec.mips, 0);
		case RESOURCE_DIMENSION_TEXTURE2D:
			return GetBitmap(name, rec, rec.format, rec.width, rec.height, rec.depth, rec.mips, 0);
		case RESOURCE_DIMENSION_TEXTURE3D:
			return GetBitmap(name, rec, rec.format, rec.width, rec.height, rec.depth, rec.mips, BMF_VOLUME);
		default:
			return ISO_NULL;
	}
}

Topology GetTopology(D3D_PRIMITIVE_TOPOLOGY prim) {
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
	if (prim < D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
		return prim_conv[prim];

	Topology	t(Topology::PATCH);
	t.mul = prim - D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1;
	return  t;
}

Topology GetTopology(PrimitiveType prim) {
	static const Topology::Type prim_conv[] = {
		Topology::UNKNOWN,
		Topology::POINTLIST,
		Topology::LINELIST,
		Topology::TRILIST,
		Topology::UNKNOWN,
		Topology::UNKNOWN,
		Topology::LINELIST_ADJ,
		Topology::TRILIST_ADJ,
	};
	if (prim < PRIMITIVE_1_CONTROL_POINT_PATCH)
		return prim_conv[prim];

	Topology	t(Topology::PATCH);
	t.mul = prim - PRIMITIVE_1_CONTROL_POINT_PATCH + 1;
	return  t;
}

Topology GetTopology(PrimitiveTopology prim) {
	static const Topology::Type prim_conv[] = {
		Topology::UNKNOWN,
		Topology::POINTLIST,
		Topology::LINELIST,
		Topology::LINESTRIP,
		Topology::TRILIST,
		Topology::TRISTRIP,
		Topology::LINELIST_ADJ,
		Topology::LINESTRIP_ADJ,
		Topology::TRILIST_ADJ,
		Topology::TRISTRIP_ADJ,
	};
	return prim_conv[prim];
}

Topology GetTopology(TessellatorOutputPrimitive prim) {
	static const Topology::Type prim_conv[] = {
		Topology::UNKNOWN,
		Topology::POINTLIST,
		Topology::LINELIST,
		Topology::TRILIST,
		Topology::TRILIST,
	};
	return prim_conv[prim];
}

Tesselation GetTesselation(TessellatorDomain domain, range<stride_iterator<const float4p>> pc) {
	if (as<uint32>(pc[0]).x == 0xcdcdcdcd) {
		switch (domain) {
			case dx::DOMAIN_ISOLINE:	return Tesselation(float2{pc[0].w, pc[1].w});
			case dx::DOMAIN_TRI:		return Tesselation(float3{pc[0].w, pc[1].w, pc[2].w}, pc[3].w);
			case dx::DOMAIN_QUAD:		return Tesselation(float4{pc[0].w, pc[1].w, pc[2].w, pc[3].w}, float2{pc[4].w, pc[5].w});
			default:					return Tesselation();
		}
	} else {
		switch (domain) {
			case dx::DOMAIN_ISOLINE:	return Tesselation(float2{pc[0].x, pc[1].x});
			case dx::DOMAIN_TRI:		return Tesselation(float3{pc[0].x, pc[1].x, pc[2].x}, pc[3].x);
			case dx::DOMAIN_QUAD:		return Tesselation(float4{pc[0].x, pc[1].x, pc[2].x, pc[3].x}, float2{pc[4].x, pc[5].x});
			default:					return Tesselation();
		}
	}
}

Triangle GetTriangle(SimulatorDX* sim, const SIG::Element* e, int i0, int i1, int i2) {
	if (e) {
		auto y	= sim->GetOutput<float4p>(e->register_num);
		return Triangle(y[i0], y[i1], y[i2]);
	}
	return Triangle(float4(zero), float4(zero), float4(zero));
}

Triangle GetTriangle(SimulatorDX *sim, const char *semantic_name, int semantic_index, const dx::Signature &sig, int i0, int i1, int i2) {
	return GetTriangle(sim, sig.find_by_semantic(semantic_name, semantic_index), i0, i1, i2);
}
/*
template<typename T> int SetShaderColumns(ColourList *c, const T *elements, int nc, win::Colour col0, win::Colour col1) {
	for (auto &i : elements->Elements()) {
		string	name	= i.name.get(elements);
		if (i.semantic_index)
			name << i.semantic_index;
		for (int m = i.mask; m; m = clear_lowest(m))
			nc	= c->AddColumn(nc, string(name) << '.' << "xyzw"[lowest_set_index(m)], 50, nc & 1 ? col1 : col0);
	}
	return nc;
}
*/
int SetShaderColumns(ColourList *c, const dx::Signature &sig, int nc, win::Colour col0, win::Colour col1) {
	for (auto &i : sig) {
		string	name	= i.name.get(sig);
		if (i.semantic_index)
			name << i.semantic_index;
		for (int m = i.mask; m; m = clear_lowest(m))
			nc	= c->AddColumn(nc, string(name) << '.' << "xyzw"[lowest_set_index(m)], 50, nc & 1 ? col1 : col0);
	}
	return nc;
}

} } //namespace iso::dx

DXConnection::DXConnection() : paused(false), addr(IP4::localhost, 0) {
}//, PORT(4567)) {}


DXConnection::~DXConnection() {
	if (paused) {
		if (!process.Terminate())
			process.TerminateSafe();
	}
}

filename DXConnection::GetDLLPath(const char *dll_name) {
	filename	dll_path	= get_exec_dir().add_dir(dll_name);
	Resource	r(0, dll_name, "BIN");
	if (!check_writebuff(FileOutput(dll_path), r, r.length())) {
		ISO_OUTPUTF("Cannot write to ") << dll_path << '\n';
	}
	return dll_path;
}
	
bool DXConnection::OpenProcess(uint32 id, const char *dll_name) {
	if (paused)
		process.Terminate();
	
	if (!process.Open(id))
		return false;

	uint16	port;
	if (RunRemote(process.hProcess, (LPTHREAD_START_ROUTINE)GetProcAddress(LoadLibraryEx(dll_name, 0, DONT_RESOLVE_DLL_REFERENCES), "RPC_WhatPort"), 0, port)) {
		addr.port	= port;
	}

	paused	= process.IsSuspended();
	filename	path;
	return process.FindModule(dll_name, path);
}

bool DXConnection::OpenProcess(const filename &app, const char *dir, const char *args, const char *dll_name, const char *dll_path) {
	if (paused)
		process.Terminate();

	if (process.Open(app, dir, args)) {
		filename	path;
		auto		lib	= process.FindModule(dll_name, path);
		if (!lib) {
			process.InjectDLL(dll_path);
			lib	= process.FindModule(dll_name, path);
		}

		uint16	port;
		if (RunRemote(process.hProcess, (LPTHREAD_START_ROUTINE)GetProcAddress(LoadLibraryEx(dll_name, 0, DONT_RESOLVE_DLL_REFERENCES), "RPC_WhatPort"), 0, port)) {
			addr.port	= port;
		} else {
			addr.port	= 0;
			Socket	sock	= IP4::TCP();
			addr.bind(sock);
			IP4::socket_addr	addr2;
			addr2.local_addr(sock);
			addr.port	= addr2.port;
		}

		process.Run();
		paused = true;
		return true;
	}
	return false;
}

void DXConnection::ConnectDebugOutput() {
	static const int
		INTF_DebugOutput	= 42,
		INTF_Status			= 0x80,
		INTF_Text			= 0x81;

	RunThread([this]() {
		SocketWait	sock	= addr.connect_or_close(IP4::TCP());
		for (uint32 delay = 1; delay < 10000 && !sock.exists(); delay <<= 1) {
			Sleep(delay);
			sock =	 addr.connect_or_close(IP4::TCP());
		}
		ISO_OUTPUT("ConnectDebugOutput started\n");

	#if 0
		if (sock.exists()) {
			SocketCallRPC<void>(sock, INTF_DebugOutput);
			char16	buffer[1024];
			while (auto read = sock.readbuff(buffer, 1023 * 2)) {
				buffer_accum16<1024>	ba;
				ba << ansi_colour("91") << str(buffer, read / 2) << ansi_colour("0");
				OutputDebugStringW(ba.term());
			}
		}
	#else
		while (sock.exists()) {
			SocketCallRPC<void>(sock, INTF_DebugOutput);
			while (sock.select(1)) {
			#if 1
				switch (sock.getc()) {
					case INTF_Status: SocketRPC(sock, [](int status) {
						//OutputDebugStringA((string_builder() << ansi_colour("32") << "Status = " << status << ansi_colour("0") << '\n').term());
						MainWindow::Get()->CheckButton(ID_ORBISCRUDE_PAUSE, status == 0);

					}); break;

					case INTF_Text: SocketRPC(sock, [](with_size<string> s) {
						OutputDebugStringA((string_builder() << ansi_colour("31") << s << ansi_colour("0")).term());
					}); break;
				}
			#else
				char	buffer[1024];
				auto read = sock.readbuff(buffer, 1023);
				buffer_accum<1024>	ba;
				ba << ansi_colour("91") << str(buffer, read) << ansi_colour("0");
				OutputDebugStringA(ba.term());
			#endif
			}
			sock =	 addr.connect_or_close(IP4::TCP());
		}
	#endif
		ISO_OUTPUT("ConnectDebugOutput stopped\n");
	});
}


//void DXCapturer::UnPause() {
//	if (paused) {
//		remote.Call("RPC_Continue");
//		paused = false;
//	}
//}

int app::MakeHeaders(win::ListViewControl lv, int nc, DXGI_COMPONENTS fmt, const char *prefix, const char *suffix) {
	static const char *prefixes[] = {
		"un", "sn", "us", "ss", "ui", "si", "snz", "f", "?", "srgb", "ubn", "ubnz", "ubi", "ubs"
	};

	for (int i = 0, n = fmt.NumComps(); i < n; i++) {
		buffer_accum<64>	title;
		bool	used	= false;
		for (int j = 0; j < 4; j++) {
			if (fmt.GetChan(j) == DXGI_COMPONENTS::X + i) {
				if (!used) {
					title << prefix;
					used = true;
				}
				title << "xyzw"[j];
			}
		}
		if (!used)
			title << "unused";
		title << " (" << prefixes[fmt.type] << fmt.CompSize(i) << ')' << suffix;
		win::ListViewControl::Column(title).Width(75).Insert(lv, nc++);
	}
	return nc;
}

//-----------------------------------------------------------------------------
//	D3D_SHADER_VARIABLE_TYPE
//-----------------------------------------------------------------------------

#define CT(T)	ctypes.get_type<T>()
const C_type *to_c_type(D3D_SHADER_VARIABLE_TYPE type) {
	static const hash_map<D3D_SHADER_VARIABLE_TYPE,const C_type*> types = {
		{D3D_SVT_BOOL,	CT(int)		},
		{D3D_SVT_INT,	CT(int)		},
		{D3D_SVT_FLOAT,	CT(float)	},
		{D3D_SVT_UINT,	CT(uint32)	},
		{D3D_SVT_UINT8,	CT(int8)	},
		{D3D_SVT_DOUBLE,CT(double)	},
	};
	return types[type];
}
#undef CT

const C_type *app::to_c_type(
	D3D_SHADER_VARIABLE_CLASS	Class,
	D3D_SHADER_VARIABLE_TYPE	Type,
	uint32						rows,
	uint32						cols
) {
	auto	base = ::to_c_type(Type);

	if (Class == D3D_SVC_SCALAR)
		return base;
		/*
	int		stride = 0;
	switch (Type) {
		case D3D_SVT_BOOL:		stride = 4; break;
		case D3D_SVT_INT:		stride = 4; break;
		case D3D_SVT_FLOAT:		stride = 4; break;
		case D3D_SVT_UINT:		stride = 4; break;
		case D3D_SVT_UINT8:		stride = 1; break;
		case D3D_SVT_DOUBLE:	stride = 8; break;
	}
	*/
	switch (Class) {
		case D3D_SVC_VECTOR:
			return ctypes.add(C_type_array(base, cols));

		case D3D_SVC_MATRIX_ROWS:
			base = ctypes.add(C_type_array(base, rows));
			base = ctypes.add(C_type_array(base, cols));
			return base;

		case D3D_SVC_MATRIX_COLUMNS:
			base = ctypes.add(C_type_array(base, cols));
			base = ctypes.add(C_type_array(base, rows));
			return base;

		default:
			return nullptr;
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

void app::AddValue(RegisterTree &rt, HTREEITEM h, const char *name, const void *data,
	D3D_SHADER_VARIABLE_CLASS	Class,
	D3D_SHADER_VARIABLE_TYPE	Type,
	uint32						rows,
	uint32						cols
) {
	if (!data) {
		rt.AddText(h, name);
		return;
	}
	buffer_accum<256>	ba(name);
	if (Class == D3D_SVC_SCALAR) {
		rt.AddText(h, GetDataValue(ba << " = ", data, Type));

	} else {
		int			stride = 0;
		switch (Type) {
			case D3D_SVT_BOOL:		stride = 4; break;
			case D3D_SVT_INT:		stride = 4; break;
			case D3D_SVT_FLOAT:		stride = 4; break;
			case D3D_SVT_UINT:		stride = 4; break;
			case D3D_SVT_UINT8:		stride = 1; break;
			case D3D_SVT_DOUBLE:	stride = 8; break;
		}

		HTREEITEM	h2;
		switch (Class) {
			case D3D_SVC_VECTOR:
				ba << " = ";
				for (int i = 0; i < cols; i++)
					GetDataValue(ba << (i ? ',' : '{'), (uint8*)data + stride * i, Type);
				h2 = rt.AddText(h, ba << '}');
				for (int i = 0; i < cols; i++)
					rt.AddText(h2, GetDataValue(ba.reset(), (uint8*)data + stride * i, Type));
				break;

			case D3D_SVC_MATRIX_ROWS:
				h2 = rt.AddText(h, ba);
				for (int i = 0; i < rows; i++) {
					ba.reset() << '[' << i << "] = ";
					for (int j = 0; j < cols; j++)
						GetDataValue(ba << (j ? ',' : '{'), (uint8*)data + stride * (i * cols + j), Type);
					HTREEITEM h3 = rt.AddText(h2, ba << '}');
					for (int j = 0; j < cols; j++)
						rt.AddText(h3, GetDataValue(ba.reset(), (uint8*)data + stride * (i * cols + j), Type));
				}
				break;
			case D3D_SVC_MATRIX_COLUMNS:
				h2 = rt.AddText(h, ba);
				for (int i = 0; i < rows; i++) {
					ba.reset() << '[' << i << "] = ";
					for (int j = 0; j < cols; j++)
						GetDataValue(ba << (j ? ',' : '{'), (uint8*)data + stride * (j * rows + i), Type);
					HTREEITEM h3 = rt.AddText(h2, ba << '}');
					for (int j = 0; j < cols; j++)
						rt.AddText(h3, GetDataValue(ba.reset(), (uint8*)data + stride * (j * rows + i), Type));
				}
				break;
//			case D3D_SVC_OBJECT:
//			case D3D_SVC_STRUCT:
//			case D3D_SVC_INTERFACE_CLASS:
//			case D3D_SVC_INTERFACE_POINTER:
		}
	}
}

//-----------------------------------------------------------------------------
//	HTMLformat
//-----------------------------------------------------------------------------

void HTMLformat(RichEditControl &re, istream_ref in) {
	XMLreader::Data		data;
	XMLreader			xml(in);

	xml.SetFlag(XMLreader::UNQUOTEDATTRIBS);
	xml.SetFlag(XMLreader::NOEQUALATTRIBS);
	xml.SetFlag(XMLreader::SKIPUNKNOWNENTITIES);
	xml.SetFlag(XMLreader::SKIPBADNAMES);
	xml.SetFlag(XMLreader::GIVEWHITESPACE);

	CharFormat			stack[8], *sp = stack;

	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND:
				if (data.Is("font")) {
					sp[1] = sp[0];
					sp++;
					if (const char *col = data.Find("color")) {
						uint32	r, g, b;
						sscanf(col + 1, "%02x%02x%02x", &r, &g, &b);
						sp->Colour(Colour(r,g,b));
					}
				} else if (data.Is("body")) {
					if (const char *col = data.Find("bgcolor")) {
						uint32	r, g, b;
						sscanf(col + 1, "%02x%02x%02x", &r, &g, &b);
						re.SetBackground(Colour(r,g,b));
					}
				}
				break;

			case XMLreader::TAG_END:
				if (data.Is("font"))
					--sp;
				break;

			case XMLreader::TAG_CONTENT:
				re.SetSelection(CharRange::end());
				re.SetFormat(*sp);
				re.ReplaceSelection(string(data.Content()), false);
				break;
		}
	}
}

class ViewHTML : public D2DEditControl {
public:
	ViewHTML(const WindowPos &wpos, const char *title, const char *text, size_t len) : D2DEditControl(wpos, title, CHILD | VISIBLE) {
		SetFont(win::Font("Courier New", 12));
		//FileOutput("D:\\test.html").writebuff(text, len);
		HTMLformat(*this, memory_reader(const_memory_block(text, len)).me());
	}
};

Control app::MakeHTMLViewer(const WindowPos &wpos, const char *title, const char *text, size_t len) {
	return *new ViewHTML(wpos, title, text, len);
}

//-----------------------------------------------------------------------------
//	HLSLcolourer
//-----------------------------------------------------------------------------

SyntaxColourerRE _HLSLcolourerRE(const ISO::Browser &settings) {
	using namespace re2;
	const char*	keywords(
		"\\b("
		"AppendStructuredBuffer|asm|asm_fragment"
		"|BlendState|bool|break|Buffer|ByteAddressBuffer"
		"|case|cbuffer|centroid|class|column_major|compile|compile_fragment|CompileShader|const|continue|ComputeShader|ConsumeStructuredBuffer"
		"|default|DepthStencilState|DepthStencilView|discard|do|double|DomainShader|dword"
		"|else|export|extern"
		"|false|float|for|fxgroup"
		"|GeometryShader|groupshared"
		"|half|Hullshader"
		"|if|in|inline|inout|InputPatch|int|interface"
		"|line|lineadj|linear|LineStream"
		"|matrix|min16float|min10float|min16int|min12int|min16uint"
		"|namespace|nointerpolation|noperspective|NULL"
		"|out|OutputPatch"
		"|packoffset|pass|pixelfragment|PixelShader|point|PointStream|precise"
		"|RasterizerState|RenderTargetView|return|register|row_major|RWBuffer|RWByteAddressBuffer|RWStructuredBuffer|RWTexture1D|RWTexture1DArray|RWTexture2D|RWTexture2DArray|RWTexture3D"
		"|sample|sampler|SamplerState|SamplerComparisonState|shared|snorm|stateblock|stateblock_state|static|string|struct|switch|StructuredBuffer"
		"|tbuffer|technique|technique10|technique11|texture|Texture1D|Texture1DArray|Texture2D|Texture2DArray|Texture2DMS|Texture2DMSArray|Texture3D|TextureCube|TextureCubeArray|true|typedef|triangle|triangleadj|TriangleStream"
		"|uint|uniform|unorm|unsigned"
		"|vector|vertexfragment|VertexShader|void|volatile"
		"|while"
		")\\b"
	);

	const char*	functions(
		"\\b("
		"abort|abs|acos|all|AllMemoryBarrier|AllMemoryBarrierWithGroupSync|any|asdouble|asfloat|asin|asint|asuint|asuint|atan|atan2"
		"|ceil|CheckAccessFullyMapped|clamp|clip|cos|cosh|countbits|cross"
		"|D3DCOLORtoUBYTE4|ddx|ddx_coarse|ddx_fine|ddy|ddy_coarse|ddy_fine|degrees|determinant|DeviceMemoryBarrier|DeviceMemoryBarrierWithGroupSync|distance|dot|dst"
		"|errorf|EvaluateAttributeAtCentroid|EvaluateAttributeAtSample|EvaluateAttributeSnapped|exp|exp2"
		"|f16tof32|f32tof16|faceforward|firstbithigh|firstbitlow|floor|fma|fmod|frac|frexp|fwidth"
		"|GetRenderTargetSampleCount|GetRenderTargetSamplePosition|GroupMemoryBarrier|GroupMemoryBarrierWithGroupSync"
		"|InterlockedAdd|InterlockedAnd|InterlockedCompareExchange|InterlockedCompareStore|InterlockedExchange|InterlockedMax|InterlockedMin|InterlockedOr""InterlockedXor|isfinite|isinf|isnan"
		"|ldexp|length|lerp|lit|log|log10|log2"
		"|mad|max|min|modf|msad4|mul"
		"|noise|normalize"
		"|pow|printf|Process2DQuadTessFactorsAvg|Process2DQuadTessFactorsMax|Process2DQuadTessFactorsMin|ProcessIsolineTessFactors|ProcessQuadTessFactorsAvg|ProcessQuadTessFactorsMax|ProcessQuadTessFactorsMin|ProcessTriTessFactorsAvg|ProcessTriTessFactorsMax|ProcessTriTessFactorsMin"
		"|radians|rcp|reflect|refract|reversebits|round|rsqrt"
		"|saturate|sign|sin|sincos|sinh|smoothstep|sqrt|step"
		"|tan|tanh|tex1D|tex1D|tex1Dbias|tex1Dgrad|tex1Dlod|tex1Dproj|tex2D|tex2D|tex2Dbias|tex2Dgrad|tex2Dlod|tex2Dproj|tex3D|tex3D|tex3Dbias|tex3Dgrad|tex3Dlod|tex3Dproj|texCUBE|texCUBE|texCUBEbias|texCUBEgrad|texCUBElod|texCUBEproj|transpose|trunc"
		")\\b"
	);
	const char*	operators(
		"("
		"++|--|~|!|+|-|*|/"
		"|%|<<|>>|&|^|||&&|||"
		"||<|>|<=|>=|==|!=|="
		"|+=|-=|*=|/=|%=|<<=|>>=|&="
		"|^=||=|?|:|[|]|(|)"
		"|{|}|.|;|::|..."
		")"
	);
	const char*	semantics(
		"\\b("
		"BINORMAL[n]|BLENDINDICES[n]|BLENDWEIGHT[n]|COLOR[n]|NORMAL[n]|POSITION[n]|POSITIONT|PSIZE[n]|TANGENT[n]|TEXCOORD[n]"
		"|COLOR[n]|FOG|POSITION[n]|PSIZE|TESSFACTOR[n]|TEXCOORD[n]|COLOR[n]|TEXCOORD[n]|VFACE|VPOS"
		"|COLOR[n]|DEPTH[n]"
		"|SV_ClipDistance[n]|SV_CullDistance[n]"
		"|SV_Coverage|SV_Depth|SV_DepthGreaterEqual(n)|SV_DepthLessEqual(n)|SV_DispatchThreadID|SV_DomainLocation"
		"|SV_GroupID|SV_GroupIndex|SV_GroupThreadID|SV_GSInstanceID"
		"|SV_InnerCoverage|SV_InsideTessFactor|SV_InstanceID|SV_IsFrontFace|SV_OutputControlPointID"
		"|SV_Position|SV_PrimitiveID|SV_RenderTargetArrayIndex|SV_SampleIndex|SV_StencilRef|SV_Target[n]|SV_TessFactor|SV_VertexID|SV_ViewportArrayIndex"
		")\\b"
	);

	static const char* all[] = {
		keywords,
		functions,
		operators,
		semantics,
//		states,
	};
	ISO::Browser	cols = settings["Colours"];

	auto	font	= win::Font::Params16(settings["Font"].GetString());
	int		tabs	= settings["Tabs"].GetInt() * 20;
	d2d::Write	write;
	auto	ext = d2d::TextLayout(write, L"xxxx", 4, d2d::Font(write, font), 1000).GetExtent();
	
	return SyntaxColourerRE(min(cols.Count(), num_elements(all)), all, cols, ext.right * 72 * 20 / 96, font);
};

const SyntaxColourerRE& app::HLSLcolourerRE() {
	static SyntaxColourerRE c = _HLSLcolourerRE(GetSettings("PSSL"));
	return c;
}

//-----------------------------------------------------------------------------
//	Shader Output
//-----------------------------------------------------------------------------

void FillShaderValues(ListViewControl c, const dx::Signature &sig, dx::SimulatorDX *sim, int col, dx::Operand::Type type, const dynamic_array<uint32> &indices) {
	if (indices) {
		for (auto &i : sig)
			col = FillColumn(c, col, make_indexed_container(sim->GetRegFile<float4p>(type, i.register_num).begin(), indices), i.mask, scalar(true));

	} else {
		for (auto &i : sig)
			col = FillColumn(c, col, sim->GetRegFile<float4p>(type, i.register_num), i.mask, scalar(true));
	}
}

void FillShaderValues(ListViewControl c, const dx::Signature &sig, dx::SimulatorDX *sim, int col, dx::Operand::Type type, const dynamic_array<uint32> &indices, const dynamic_bitarray<uint32> &enabled) {
	if (indices) {
		for (auto &i : sig)
			col = FillColumn(c, col, make_indexed_container(sim->GetRegFile<float4p>(type, i.register_num).begin(), indices), i.mask, make_indexed_container(enabled, indices));

	} else {
		for (auto &i : sig)
			col = FillColumn(c, col, sim->GetRegFile<float4p>(type, i.register_num), i.mask, make_indexed_container(enabled));
	}
}

TypedBuffer MakeTypedBuffer(dx::SimulatorDX *sim, dx::Operand::Type type, const dx::Signature &sig) {
	auto			format	= sig_to_c_type(sig);
	auto			stride	= format->size32();
	auto			regs	= sim->GetRegFile<uint8>(type);
	uint32			num		= regs.size32();
	malloc_block	mem(stride * num);

	char	*dest	= mem;
	for (auto &r : regs) {
		memcpy(dest, &r, stride);
		dest += stride;
	}

	return TypedBuffer(move(mem), stride, format);
}

Control app::MakeShaderOutput(const WindowPos &wpos, dx::SimulatorDX *sim, const dx::Shader &shader, dynamic_array<uint32> &indices) {
	using namespace dx;
	auto	col_in0		= MakeColour(2), col_in1  = FadeColour(col_in0);
	auto	col_out0	= MakeColour(1), col_out1 = FadeColour(col_out0);

	uint32	num		= sim->NumThreads();

	switch (shader.stage) {
		case dx::HS: {
			sim->Run();
			SplitterWindow		*s		= nullptr;;
			WindowPos			wpos2	= wpos;

			if (sim->ControlPointMask()) {
				s		= new SplitterWindow(SplitterWindow::SWF_HORZ | SplitterWindow::SWF_DELETE_ON_DESTROY);
				s->Create(wpos, "Shader Output", Control::CHILD | Control::CLIPSIBLINGS | Control::VISIBLE);
				wpos2	= s->GetPanePos(0);
			}
			auto	cc		= new VertexOutputWindow(wpos2, "Shader Output", "HC");
			int		nc		= cc->AddColumn(0, "#", 50, RGB(255,255,255));
			auto	col		= MakeColour(0);
			nc	= cc->AddColumn(nc, "x", 50, col);
			nc	= cc->AddColumn(nc, "y", 50, col);
			nc	= cc->AddColumn(nc, "z", 50, col);
			nc	= cc->AddColumn(nc, "w", 50, col);
			
			auto	regs = sim->GetRegFile<float4p>(dx::Operand::TYPE_INPUT_PATCH_CONSTANT);
			for (uint32 m = sim->PatchConstOutputMask(); m; m = clear_lowest(m)) {
				int		i = lowest_set_index(m);
				char	text[64];
				ListViewControl2::Item	item(text);
				fixed_accum(text) << i;
				item.Insert(cc->vw);

				auto	o = regs[i];
				for (int j = 0; j < 4; j++) {
					fixed_accum(text) << o[j];
					item.NextColumn().Set(cc->vw);
				}
			}

			if (s) {
				VertexOutputWindow	*cp		= new VertexOutputWindow(s->GetPanePos(1), 0, 'HO');
				uint32				num_cp	= sim->NumOutputControlPoints();
				cp->vw.SetCount(num_cp);

				nc		= cp->AddColumn(0, "#", 50, RGB(255,255,255));
				if (auto out = shader.GetSignatureOut()) {
					SetShaderColumns(cp, out, nc, col_out0, col_out1);
					cp->FillShaderIndex(num_cp);

					dynamic_bitarray<uint32>	enabled(num);
					enabled.slice(0, num_cp).set_all();
					//FillShaderValues(cp->vw, out, sim, 1, dx::Operand::TYPE_OUTPUT_CONTROL_POINT, indices, enabled);
					FillShaderValues(cp->vw, out, sim, 1, dx::Operand::TYPE_OUTPUT, indices, enabled);
				}

				s->SetPanes(
					*new TitledWindow("Patch Constants", *cc),
					*new TitledWindow("Control Points", *cp)
				);
				cc->vw.Show();
				cp->vw.Show();
				return *s;
			}

			cc->vw.Show();
			return *cc;
		}

		case dx::GS: {
			VertexOutputWindow	*c	= new VertexOutputWindow(wpos, "Shader Output", 'GO');
			int		nc		= c->AddColumn(0, "#", 50, RGB(255,255,255));
			int		in_col	= nc;
			auto	in		= shader.GetSignatureIn();
			nc = SetShaderColumns(c, in, nc, col_in0, col_in1);

			int		out_col = nc;
			auto	out = shader.GetSignatureOut();
			nc = SetShaderColumns(c, out, nc, col_out0, col_out1);

			int	x = 0;
			for (auto &i : sim->uav) {
				nc = MakeHeaders(c->vw, nc, i->format);
				c->AddColour(nc, MakeColour(x++));
			}

			uint32	per_thread = max(sim->num_input, sim->max_output);
			
			for (auto i : int_range(num * per_thread)) {
				ListViewControl::Item().Insert(c->vw);
			}
			//c->vw.SetCount(num * sim->num_input);
			//c->FillShaderIndex(num);

			for (int i = 0; i < num; i++) {
				char				text[64];
				ListViewControl2::Item	item(text);
				fixed_accum(text) << i;
				item.Set(c->vw, i * per_thread);
			}

			uint32	input_size	= highest_set_index(sim->input_mask) + 1;
			for (auto &i : in) {
				int	row = 0;
				switch (i.component_type) {
					case dx::SIG::UINT32:
						for (auto &src : sim->GetRegFile<uint4p>(dx::Operand::TYPE_INPUT, i.register_num)) {
							FillColumn(c->vw, in_col, make_range_n(strided(&src, input_size * sizeof(float4p)), sim->num_input), i.mask, scalar(true), row);
							row += per_thread;
						}
						break;
					case dx::SIG::FLOAT32: 
						for (auto &src : sim->GetRegFile<float4p>(dx::Operand::TYPE_INPUT, i.register_num)) {
							FillColumn(c->vw, in_col, make_range_n(strided(&src, input_size * sizeof(float4p)), sim->num_input), i.mask, scalar(true), row);
							row += per_thread;
						}
						break;
				}
				in_col += count_bits(i.mask);
			}

			sim->Run();

			uint32				output_size	= highest_set_index(sim->output_mask & ~(1 << sim->oEmitted)) + 1;
			temp_array<uint32>	num_output	= sim->GetRegFile<uint32>(dx::Operand::TYPE_OUTPUT, sim->oEmitted);

			for (auto &i : out) {
				int	t	= 0;
				switch (i.component_type) {
					case dx::SIG::UINT32:
						for (auto &src : sim->GetRegFile<uint4p>(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, i.register_num)) {
							FillColumn(c->vw, out_col, make_range_n(strided(&src, output_size * sizeof(float4p)), num_output[t]), i.mask, scalar(true), t * per_thread);
							++t;
						}
						break;
					case dx::SIG::FLOAT32: 
						for (auto &src : sim->GetRegFile<float4p>(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, i.register_num)) {
							FillColumn(c->vw, out_col, make_range_n(strided(&src, output_size * sizeof(float4p)), num_output[t]), i.mask, scalar(true), t * per_thread);
							++t;
						}
						break;
				}
				out_col += count_bits(i.mask);
			}

			c->vw.Show();
			return *c;
		}

		case dx::DS: {
			VertexOutputWindow *c	= new VertexOutputWindow(wpos, "Shader Output", 'DO');

			int		nc	= c->AddColumn(0, "#", 50, RGB(255,255,255));
			if (indices)
				nc	= c->AddColumn(nc, "index", 50, RGB(255,255,255));

			int		in_col	= nc;
			int		fields	= sim->tess_domain == dx::DOMAIN_TRI ? 3 : 2;
			for (int i = 0; i < fields; i++)
				nc	= c->AddColumn(nc, string("Domain") << '.' << "xyz"[i], 50, nc & 1 ? col_in1 : col_in0);

			int		out_col = nc;
			auto	out = shader.GetSignatureOut();
			nc = SetShaderColumns(c, out, nc, col_out0, col_out1);

			int	x = 0;
			for (auto &i : sim->uav) {
				nc = MakeHeaders(c->vw, nc, i->format);
				c->AddColour(nc, MakeColour(x++));
			}

			uint32	num2	= indices ? indices.size32() : num;
			c->FillShaderIndex(num2);
			if (indices) {
				int	i = 0;
				for (auto x : indices)
					ListViewControl2::Item(to_string(x)).Index(i++).Column(1).Set(c->vw);
				FillColumn(c->vw, in_col, make_indexed_container(sim->GetRegFile<float4p>(dx::Operand::TYPE_INPUT_DOMAIN_POINT).begin(), indices), bits(fields), scalar(true));

			} else {
				FillColumn(c->vw, in_col, sim->GetRegFile<float4p>(dx::Operand::TYPE_INPUT_DOMAIN_POINT), bits(fields), scalar(true));
			}

			sim->Run();

			dynamic_bitarray<uint32>	enabled(num);
			for (int i = 0; i < num; i++)
				enabled[i] = !(sim->ThreadFlags(i) & sim->THREAD_DISCARDED);

			FillShaderValues(c->vw, out, sim, out_col, dx::Operand::TYPE_OUTPUT, indices, enabled);

			c->vw.SetCount(num2);
			c->vw.Show();

			return *c;
		}

		case dx::AS: {
			sim->Run();
			dynamic_array<uint4p>	regs	= sim->GetRegFile<uint4p>(dx::Operand::TYPE_INPUT_PATCH_CONSTANT);
			uint32					stride	= regs.size() * 16;
			auto					format	= ctypes.get_array_type<uint4p>(regs.size());
			named<TypedBuffer>		buffers[] = {
				{"out", TypedBuffer(regs.detach_raw(), stride, format)}
			};
			return *MakeVertexWindow(wpos, "Shader Output", 'AO', buffers, indices);
		}

		case dx::MS: {
			sim->Run();
			if (sim->output_topology == TOPOLOGY_LINELIST) {
				auto	prims	= sim->GetRegFile<uint2p>(Operand::TYPE_OUTPUT_CONTROL_POINT);
				indices.resize(prims.size() * 2);
				copy(prims, (uint2p*)indices.begin());
			} else {
				auto	prims	= sim->GetRegFile<uint3p>(Operand::TYPE_OUTPUT_CONTROL_POINT);
				indices.resize(prims.size() * 3);
				copy(prims, (uint3p*)indices.begin());
			}

			named<TypedBuffer>	buffers[] = {
				{"out",	MakeTypedBuffer(sim, dx::Operand::TYPE_OUTPUT,shader.GetSignatureOut())}
			};
			return *MakeVertexWindow(wpos, "Shader Output", 'MO', buffers, indices);
		}

		default: {
			TypedBuffer	inbuff	= MakeTypedBuffer(sim, dx::Operand::TYPE_INPUT, shader.GetSignatureIn());
			sim->Run();
			TypedBuffer	outbuff	= MakeTypedBuffer(sim, dx::Operand::TYPE_OUTPUT,shader.GetSignatureOut());

			named<TypedBuffer>		buffers[2] = {
				{"in",	move(inbuff)},
				{"out",	move(outbuff)}
			};

			return *MakeVertexWindow(wpos, "Shader Output", ("VPDHGAMC"[shader.stage] << 8) + 'O', buffers, indices);
		}
	}
}

void app::AddShaderOutput(Control c, dx::SimulatorDX *sim, const dx::Shader &shader, dynamic_array<uint32> &indices) {
	switch (shader.stage) {
		case dx::AS: {
			sim->Run();
			dynamic_array<uint4p>	regs	= sim->GetRegFile<uint4p>(dx::Operand::TYPE_INPUT_PATCH_CONSTANT);
			uint32					stride	= regs.size() * 16;
			auto					format	= ctypes.get_array_type<uint4p>(regs.size());
			TypedBuffer				buffers[] = {
				TypedBuffer(regs.detach_raw(), stride, format)
			};
			VertexWindow::Cast(c)->AddVertices(buffers, indices, 0);
			break;
		}

		case dx::MS: {
			sim->Run();
			auto	prims = sim->GetRegFile<uint3p>(dx::Operand::TYPE_OUTPUT_CONTROL_POINT);
			indices.resize(prims.size() * 3);
			copy(prims, (uint3p*)indices.begin());
			TypedBuffer				buffers[] = {
				MakeTypedBuffer(sim, dx::Operand::TYPE_OUTPUT,shader.GetSignatureOut())
			};
			auto	v	= VertexWindow::Cast(c);
			auto	nv	= v->NumUnique();
			for (auto &i : indices)
				i += nv;
			v->AddVertices(buffers, indices, 0);
			break;
		}

		default: {
			TypedBuffer	inbuff	= MakeTypedBuffer(sim, dx::Operand::TYPE_INPUT, shader.GetSignatureIn());
			sim->Run();
			TypedBuffer	outbuff	= MakeTypedBuffer(sim, dx::Operand::TYPE_OUTPUT,shader.GetSignatureOut());

			TypedBuffer		buffers[2] = {move(inbuff), move(outbuff)};
			VertexWindow::Cast(c)->AddVertices(buffers, indices, 0);
			break;
		}
	}
}

