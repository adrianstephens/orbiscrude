#include "dx_gpu.h"
#include "extra/xml.h"
#include "windows/text_control.h"
#include "dx/dxgi_read.h"
#include "filename.h"
#include "conversion/channeluse.h"

using namespace app;

namespace iso { namespace dx {

ChannelUse::chans GetChannels(DXGI_COMPONENTS format) {
	ChannelUse::chans c;
	for (int i = 0; i < 4; i++)
		c[i] = format.GetChan(i);
	return c;
}

template<typename T> const void *copy_slices(const block<T, 3> &dest, const void *srce, DXGI_COMPONENTS format, uint64 depth_stride) {
	srce = copy_slices(dest, srce, format.Layout(), format.Type(), depth_stride);
	RearrangeChannels(dest, GetChannels(format));
	return srce;
}

ISO_ptr_machine<void> GetBitmap(const char *name, const void *srce, DXGI_COMPONENTS format, int width, int height, int depth, int mips, int flags) {
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

ISO_ptr_machine<void> GetBitmap(const char *name, SimulatorDXBC::Resource &rec) {
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
	CT(unorm8[4]),			//DXGI_FORMAT_R8G8_UNORM				= 49,
	CT(uint8[4]),			//DXGI_FORMAT_R8G8_UINT					= 50,
	CT(norm8[4]),			//DXGI_FORMAT_R8G8_SNORM				= 51,
	CT(int8[4]),			//DXGI_FORMAT_R8G8_SINT					= 52,
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
	const C_type *c_types[8][3] = {
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

	const C_type	*c = c_types[f.Type()][log2(info.bits) - 3];
	if (info.comps > 1)
		c = ctypes.add(C_type_array(c, info.comps));
	return c;
}

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

Topology2 GetTopology(PrimitiveType prim, uint32 chunks) {
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
	if (prim < num_elements(prim_conv))
		return Topology2(prim_conv[prim], chunks);
	Topology2	t(Topology::PATCH);
	t.SetNumCP(prim - 32);
	return  t;
}

Topology2 GetTopology(PrimitiveTopology prim) {
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

Topology2 GetTopology(TessellatorOutputPrimitive prim) {
	static const Topology::Type prim_conv[] = {
		Topology::UNKNOWN,
		Topology::POINTLIST,
		Topology::LINELIST,
		Topology::TRILIST,
		Topology::TRILIST,
	};
	return prim_conv[prim];
}

Tesselation GetTesselation(TessellatorDomain domain, range<stride_iterator<const SimulatorDXBC::Register>> pc) {
	switch (domain) {
		case dx::DOMAIN_ISOLINE:	return Tesselation(float2{pc[0].x, pc[1].x});
		case dx::DOMAIN_TRI:		return Tesselation(float4{pc[0].x, pc[1].x, pc[2].x}, pc[3].x);
		case dx::DOMAIN_QUAD:		return Tesselation(float4{pc[0].x, pc[1].x, pc[2].x, pc[3].x}, float2{pc[4].x, pc[5].x});
		default:					return Tesselation();
	}
}

SimulatorDXBC::Triangle GetTriangle(SimulatorDXBC &sim, const char *semantic_name, int semantic_index, OSGN *vs_out, OSG5 *gs_out, int i0, int i1, int i2) {
	if (vs_out) {
		if (auto *x = vs_out->find_by_semantic(semantic_name, semantic_index)) {
			auto y	= sim.GetRegFile(dx::Operand::TYPE_OUTPUT, x->register_num);
			return dx::SimulatorDXBC::Triangle(y[i0], y[i1], y[i2]);
		}
	} else {
		if (auto *x = gs_out->find_by_semantic(semantic_name, semantic_index)) {
			auto y	= sim.GetStreamFileAll(x->register_num);
			return dx::SimulatorDXBC::Triangle(y[i0], y[i1], y[i2]);
		}
	}
	return dx::SimulatorDXBC::Triangle(float4(zero), float4(zero), float4(zero));
}

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

} } //namespace iso::dx

DXCapturer::~DXCapturer() {
	if (paused) {
		if (!process.Terminate())
			process.TerminateSafe();
	}
}

filename DXCapturer::GetDLLPath(const char *dll_name) {
	filename	dll_path	= get_exec_dir().add_dir(dll_name);
	Resource	r(0, dll_name, "BIN");
	if (!check_writebuff(FileOutput(dll_path), r, r.length())) {
		ISO_OUTPUTF("Cannot write to ") << dll_path << '\n';
	}
	return dll_path;
}
	
bool DXCapturer::OpenProcess(uint32 id, const char *dll_name) {
	if (paused)
		process.Terminate();
	
	if (!process.Open(id))
		return false;

	paused	= process.IsSuspended();
	filename	path;
	return process.FindModule(dll_name, path);
}

bool DXCapturer::OpenProcess(const filename &app, const char *dir, const char *args, const char *dll_name, const char *dll_path) {
	if (paused)
		process.Terminate();

	if (process.Open(app, dir, args)) {
		filename	path;
		if (!process.FindModule(dll_name, path))
			process.InjectDLL(dll_path);

		//HANDLE	pipe2;
		//if (!DuplicateHandle(GetCurrentProcess(), pipe, process, &pipe2, 0, TRUE, DUPLICATE_SAME_ACCESS))
		//	pipe2 = 0;

		//remote.CallASync("RPC_InstallHooks", pipe2);
		process.Run();
		paused = true;
		return true;
	}
	return false;
}

void DXCapturer::ConnectDebugOutput() {
	static const int INTF_DebugOutput = 42;
	RunThread([addr = addr]() {
		ISO_OUTPUT("ConnectDebugOutput started");
		SocketWait	sock =	 addr.socket();
		for (uint32 delay = 1; delay < 10000 && !sock.exists(); delay <<= 1) {
			Sleep(delay);
			sock =	 addr.socket();
		}
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
				char	buffer[1024];
				auto read = sock.readbuff(buffer, 1023);
				buffer_accum<1024>	ba;
				ba << ansi_colour("91") << str(buffer, read) << ansi_colour("0");
				OutputDebugStringA(ba.term());
			}
			sock =	 addr.socket();
		}
		#endif
		ISO_OUTPUT("ConnectDebugOutput stopped");
	});
}


//void DXCapturer::UnPause() {
//	if (paused) {
//		remote.Call("RPC_Continue");
//		paused = false;
//	}
//}

struct type_converter {
	PDB_types			&pdb;
	TI					minTI;
	const char			*name;
	C_type_composite	*forward_struct;
	const CV::Bitfield	*bitfield;

	static const C_type *const float_types[];
	static const C_type *const signed_integral_types[];
	static const C_type *const unsigned_integral_types[];
	static const C_type *const int_types[];
	static const C_type *const special_types[];
	static const C_type *const special_types2[];

	static const C_type *simple(TI ti) {
	
		int	sub	= CV_SUBT(ti);
		const C_type *type = 0;
		switch (CV_TYPE(ti)) {
			case CV::SPECIAL:	type = special_types[sub]; break;
			case CV::SIGNED:	type = signed_integral_types[sub]; break;
			case CV::UNSIGNED:	type = unsigned_integral_types[sub]; break;
			case CV::BOOLEAN:	type = CT(bool); break;
			case CV::REAL:		type = float_types[sub]; break;
		//	case CV::COMPLEX:	type = "complex<" << float_types[sub] << '>'; break;
			case CV::SPECIAL2:	type = special_types2[sub];	break;
			case CV::INT:		type = int_types[sub];	break;
		//	case CV::CVRESERVED:type = "<reserved>";	break;
			default: ISO_ASSERT(0); return 0;
		}
		switch (CV_MODE(ti)) {
			case CV::TM_DIRECT:	return type;
			case CV::TM_NPTR:	return ctypes.add(C_type_pointer(type, 16, false));
			case CV::TM_NPTR32:	return ctypes.add(C_type_pointer(type, 32, false));
			case CV::TM_NPTR64:	return ctypes.add(C_type_pointer(type, 64, false));
			default: ISO_ASSERT(0); return 0;
		}
	}
	
	static bool is_conversion_operator(const char *name) {
		return str(name).begins("operator ");
	}

	const C_type *procTI(TI ti) {
		if (ti < 0x1000)
			return simple(ti);
		return process<const C_type*>(pdb.GetType(ti), *this);
	}

	CV::FieldList	*get_fieldlist(TI ti) {
		if (ti) {
			CV::FieldList	*list = pdb.GetType(ti)->as<CV::FieldList>();
			ISO_ASSERT(list->leaf == CV::LF_FIELDLIST || list->leaf == CV::LF_FIELDLIST_16t);
			return list;
		}
		return 0;
	}

//	const C_type *operator()(nullptr_t) {}
	template<typename T> const C_type *operator()(const T&, bool = false) { return 0; }

	const C_type *operator()(const CV::HLSL &t) {
		static const char *to_pssl[] = {
			0,							//CV_BI_HLSL_INTERFACE_POINTER			= 0x0200,
			"Texture1D",				//CV_BI_HLSL_TEXTURE1D					= 0x0201,
			"Texture1D_Array",			//CV_BI_HLSL_TEXTURE1D_ARRAY			= 0x0202,
			"Texture2D",				//CV_BI_HLSL_TEXTURE2D					= 0x0203,
			"Texture2D_Array",			//CV_BI_HLSL_TEXTURE2D_ARRAY			= 0x0204,
			"Texture3D",				//CV_BI_HLSL_TEXTURE3D					= 0x0205,
			"TextureCube",				//CV_BI_HLSL_TEXTURECUBE				= 0x0206,
			"TextureCube_Array",		//CV_BI_HLSL_TEXTURECUBE_ARRAY			= 0x0207,
			"Texture2D",				//CV_BI_HLSL_TEXTURE2DMS				= 0x0208,
			"Texture2D_Array",			//CV_BI_HLSL_TEXTURE2DMS_ARRAY			= 0x0209,
			"Sampler",					//CV_BI_HLSL_SAMPLER					= 0x020a,
			"Sampler",					//CV_BI_HLSL_SAMPLERCOMPARISON			= 0x020b,
			"DataBuffer",				//CV_BI_HLSL_BUFFER						= 0x020c,
			"PointBuffer",				//CV_BI_HLSL_POINTSTREAM				= 0x020d,
			"LineBuffer",				//CV_BI_HLSL_LINESTREAM					= 0x020e,
			"TriangleBuffer",			//CV_BI_HLSL_TRIANGLESTREAM				= 0x020f,
			"Inputpatch",				//CV_BI_HLSL_INPUTPATCH					= 0x0210,
			"Outputpatch",				//CV_BI_HLSL_OUTPUTPATCH				= 0x0211,
			"RW_Texture1d",				//CV_BI_HLSL_RWTEXTURE1D				= 0x0212,
			"RW_Texture1d_Array",		//CV_BI_HLSL_RWTEXTURE1D_ARRAY			= 0x0213,
			"RW_Texture2d",				//CV_BI_HLSL_RWTEXTURE2D				= 0x0214,
			"RW_Texture2d_Array",		//CV_BI_HLSL_RWTEXTURE2D_ARRAY			= 0x0215,
			"RW_Texture3d",				//CV_BI_HLSL_RWTEXTURE3D				= 0x0216,
			"RW_DataBuffer",			//CV_BI_HLSL_RWBUFFER					= 0x0217,
			"ByteBuffer",				//CV_BI_HLSL_BYTEADDRESS_BUFFER			= 0x0218,
			"RW_ByteBuffer",			//CV_BI_HLSL_RWBYTEADDRESS_BUFFER		= 0x0219,
			"RegularBuffer",			//CV_BI_HLSL_STRUCTURED_BUFFER			= 0x021a,
			"RW_RegularBuffer",			//CV_BI_HLSL_RWSTRUCTURED_BUFFER		= 0x021b,
			"AppendRegularBuffer",		//CV_BI_HLSL_APPEND_STRUCTURED_BUFFER	= 0x021c,
			"ConsumeRegularBuffer",		//CV_BI_HLSL_CONSUME_STRUCTURED_BUFFER	= 0x021d,
			0,							//CV_BI_HLSL_MIN8FLOAT					= 0x021e,
			0,							//CV_BI_HLSL_MIN10FLOAT					= 0x021f,
			0,							//CV_BI_HLSL_MIN16FLOAT					= 0x0220,
			0,							//CV_BI_HLSL_MIN12INT					= 0x0221,
			0,							//CV_BI_HLSL_MIN16INT					= 0x0222,
			0,							//CV_BI_HLSL_MIN16UINT					= 0x0223,
		};
		auto	*type = procTI(t.subtype);
		if (const char *pssl = to_pssl[t.kind - 0x200]) {
			if (auto *temp = builtin_ctypes().lookup(to_pssl[t.kind - 0x200]))
				type = ctypes.instantiate(temp, pssl, type);
		}
		return type;
	}
	const C_type *operator()(const CV::Alias &t) {
		return ctypes.add(t.name, procTI(t.utype));
	}
	const C_type *operator()(const CV::Pointer &t) {
		return ctypes.add(C_type_pointer(procTI(t.utype), 64, is_any(t.attr.ptrmode, CV::PTR_MODE_REF, CV::PTR_MODE_LVREF, CV::PTR_MODE_RVREF)));
	}
	const C_type *operator()(const CV::Array &t) {
		return ctypes.add(C_type_array(procTI(t.elemtype), uint32((int64)t.size / pdb.GetTypeSize(t.elemtype))));
	}
	const C_type *operator()(const CV::StridedArray &t) {
		return ctypes.add(C_type_array(procTI(t.elemtype), uint32((int64)t.size / t.stride), t.stride));
	}
	const C_type *operator()(const CV::Vector &t) {
		return ctypes.add(C_type_array(procTI(t.elemtype), t.count));
	}
	const C_type *operator()(const CV::Matrix &t) {
		if (t.matattr.row_major)
			return ctypes.add(C_type_array(ctypes.add(C_type_array(procTI(t.elemtype), t.cols)), t.rows));
		else
			return ctypes.add(C_type_array(ctypes.add(C_type_array(procTI(t.elemtype), t.rows)), t.cols));
	}
	const C_type *operator()(const CV::Proc &t) {
		return 0;
	}
	const C_type *operator()(const CV::BClass &t) {
		return 0;
	}
	const C_type *operator()(const CV::Enumerate &t) {
		return 0;
	}
//	const C_type *operator()(const CV::Member &t) {
//		auto	type = procTI(t.index);
//		name	= t.name();
//		offset	= t.offset;
//		return type;
//	}
//	const C_type *operator()(const CV::STMember &t) {
//		return 0;
//	}
//	const C_type *operator()(const CV::MFunc &t) {
//		return 0;
//	}
//	const C_type *operator()(const CV::Method &t) {
//		return 0;
//	}
//	const C_type *operator()(const CV::OneMethod &t) {
//		return 0;
//	}
	const C_type *operator()(const CV::NestType &t) {
		return 0;
	}
	const C_type *operator()(const CV::Class &t) {
		string	id		= t.name();
		auto	ptype	= (C_type_struct*)ctypes.lookup(id);

		if (!ptype || ptype != forward_struct) {
			if (ptype)
				return ptype;

			ptype = new C_type_struct(id);
			ctypes.add(id, ptype);
		
			if (t.property.fwdref) {
				auto	ti = pdb.LookupUDT(t.name());
				return save(forward_struct, ptype), procTI(ti);
			}
		}
		
		ptype->_size = (int64)t.size;

		if (auto *list = get_fieldlist(t.derived)) {
			for (auto &i : list->list())
				ptype->add(0, process<const C_type*>(&i, *this));
		}

		if (auto *list = get_fieldlist(t.field)) {
			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_BCLASS:
					case CV::LF_BINTERFACE: {
						auto	*b = i.as<CV::BClass>();
						ptype->add_atoffset(nullptr, procTI(b->index), (uint32)(int64)b->offset);
						break;
					}
				}
			}

			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_MEMBER: {
						auto	*m	= i.as<CV::Member>();
						auto	bf	= save(bitfield, nullptr);
						if (auto *t	= procTI(m->index)) {;
							int64	offset	= m->offset;
							if (bitfield) {
								ISO_ASSERT(t->type == C_type::INT);
								auto		*t1 = (const C_type_int*)t;
								ptype->add_atbit(m->name(), ctypes.add(C_type_int(bitfield->length, t1->flags & ~C_type_int::ENUM)), (uint32)offset * 8 + bitfield->position);
							} else {
								ptype->add_atoffset(m->name(), t, (uint32)offset);
							}
						}
						break;
					}
					case CV::LF_STMEMBER: {
						auto	*m	= i.as<CV::STMember>();
						auto	*t	= procTI(m->index);
						ptype->add_static(m->name, t);
						break;
					}
				}
			}
		}

		return ptype;
	}
	const C_type *operator()(const CV::Union &t) {
		string	id		= t.name();
		auto	ptype	= (C_type_union*)ctypes.lookup(id);

		if (!ptype || ptype != forward_struct) {
			if (ptype)
				return ptype;

			ptype = new C_type_union(id);
			ctypes.add(id, ptype);

			if (t.property.fwdref) {
				auto	ti = pdb.LookupUDT(t.name());
				return save(forward_struct, ptype), procTI(ti);
			}
		}

		ptype->_size = (int64)t.size;

		if (auto *list = get_fieldlist(t.field)) {
			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_MEMBER: {
						auto	*m	= i.as<CV::Member>();
						auto	*t	= procTI(m->index);
						ptype->add(m->name(), t);
						break;
					}
					case CV::LF_STMEMBER: {
						auto	*m	= i.as<CV::STMember>();
						auto	*t	= procTI(m->index);
						ptype->add_static(m->name, t);
						break;
					}
				}
			}
		}

		return ptype;
	}
	const C_type *operator()(const CV::Enum &t) {
		auto	underlying = procTI(t.utype);
		if (underlying->type != C_type::INT)
			return 0;

		C_type_enum		e(*(const C_type_int*)underlying, t.count);
		if (auto *list = get_fieldlist(t.field)) {
			for (auto &i : list->list()) {
				switch (i.leaf) {
					case CV::LF_ENUMERATE: {
						auto	en = i.as<CV::Enumerate>();
						e.emplace_back(en->name(), en->value);
						break;
					}
				}
			}
		}
		return ctypes.add(t.name, ctypes.add(e));

	}
	const C_type *operator()(const CV::ArgList &t) {
		return 0;
	}
	const C_type *operator()(const CV::Modifier &t) {
		return procTI(t.type);
	}
	const C_type *operator()(const CV::ModifierEx &t) {
		return procTI(t.type);
	}
	const C_type *operator()(const CV::Bitfield &t) {
		const C_type *sub = procTI(t.type);
		bitfield = &t;
		return sub;
	}

	type_converter(PDB_types &pdb) : pdb(pdb), minTI(pdb.MinTI()) {}
};

const C_type *const type_converter::float_types[] = {
	CT(float),
	CT(double),
	CT(float80),
	CT(float128),
	CT(float48),
	0,//CT(float32PP),
	CT(float16),
};
const C_type *const type_converter::signed_integral_types[] = {
	CT(char),
	CT(short),
	CT(int),
	CT(int64),
	0,//CT(int128),
};
const C_type *const type_converter::unsigned_integral_types[] = {
	CT(uint8),
	CT(uint16),
	CT(uint32),
	CT(uint64),
	CT(uint128),
};
const C_type *const type_converter::int_types[] = {
	CT(char),
	CT(unsigned char),	//>()wchar?>(),
	CT(short),
	CT(unsigned short),
	CT(int),
	CT(unsigned),
	CT(int64),
	CT(uint64),
	0,//CT(int128),
	CT(uint128),
	CT(char16_t),
	CT(char32_t),
};
const C_type *const type_converter::special_types[] = {
	0,	//"notype"
	0,	//"abs",
	0,	//"segment",
	0,	//"void",
	0,	//"currency",
	0,	//"nbasicstr",
	0,	//"fbasicstr",
	0,	//"nottrans",
	CT(HRESULT),	//"HRESULT",
};
const C_type *const type_converter::special_types2[] = {
	0,	//"bit",
	0,	//"paschar",
	0,	//"bool32ff",
};

const C_type *app::to_c_type(PDB_types &pdb, TI ti) {
	if (ti < 0x1000)
		return type_converter::simple(ti);

	type_converter	conv(pdb);
	return process<const C_type*>(pdb.GetType(ti), conv);
}

struct hlsl_type_getter {
	PDB_types		&pdb;

	const CV::HLSL *procTI(TI ti) {
		if (ti < pdb.MinTI())
			return 0;
		return process<const CV::HLSL*>(pdb.GetType(ti), *this);
	}
	const CV::HLSL *operator()(const CV::Leaf&, bool = false) {
		return 0;
	}
	const CV::HLSL *operator()(const CV::HLSL &t) {
		return &t;
	}
	const CV::HLSL *operator()(const CV::Alias &t) {
		return procTI(t.utype);
	}
	const CV::HLSL *operator()(const CV::Modifier &t) {
		return procTI(t.type);
	}
	const CV::HLSL *operator()(const CV::ModifierEx &t) {
		return procTI(t.type);
	}
	hlsl_type_getter(PDB_types &pdb) : pdb(pdb) {}
};

const CV::HLSL *app::get_hlsl_type(PDB_types &pdb, TI ti) {
	return hlsl_type_getter(pdb).procTI(ti);
}

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
		"\\b("
		"++|--|~|!|+|-|*|/"
		"|%|<<|>>|&|^|||&&|||"
		"||<|>|<=|>=|==|!=|="
		"|+=|-=|*=|/=|%=|<<=|>>=|&="
		"|^=||=|?|:|[|]|(|)"
		"|{|}|.|;|::|..."
		")\\b"
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
//	DXBCRegisterWindow
//-----------------------------------------------------------------------------

void DXBCRegisterWindow::Update(const dx::SimulatorDXBC &sim, dx::SHADERSTAGE stage, ParsedSPDB &spdb, uint64 pc, int thread) {
	if (!entries) {
		uint32	offset = 0;

		entries.emplace_back(0, "pc", nullptr, Entry::SIZE64);
		offset += 8;

		for (uint64 m = sim.InputMask(); m; m = clear_lowest(m)) {
			int		i		= lowest_set_index(m);
			auto	type	= sim.input_type(i, stage);
			offset = AddReg(type, i, offset);
		}

		for (int i = 0; i < sim.NumInputControlPoints(); i++)
			offset = AddReg(dx::Operand::TYPE_INPUT_CONTROL_POINT, i, offset);

		for (uint64 m = sim.OutputMask(); m; m = clear_lowest(m)) {
			int		i		= lowest_set_index(m);
			auto	type	= sim.output_type(i);
			if (type != dx::Operand::TYPE_OUTPUT)
				i = 0;
			offset = AddReg(sim.output_type(i), i, offset);
		}

		for (uint64 m = sim.PatchConstOutputMask(); m; m = clear_lowest(m))
			offset = AddReg(dx::Operand::TYPE_INPUT_PATCH_CONSTANT, lowest_set_index(m), offset);

		for (int i = 0; i < sim.NumOutputControlPoints(); i++)
			offset = AddReg(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, i, offset);

		for (int i = 0; i < sim.NumTemps(); i++)
			offset = AddReg(dx::Operand::TYPE_TEMP, i, offset);

		prev_regs.create(offset);
		SetPane(1, SendMessage(WM_ISO_NEWPANE));
	}

	struct RegUse {
		const CV::LOCALSYM	*parent;
		uint16		offset, size;
	};
	hash_map<const void*, RegUse>	uses;

	const CV::LOCALSYM	*parent = 0;
	if (auto *mod = spdb.HasModule(1)) {
		for (auto &s : mod->Symbols()) {
			switch (s.rectyp) {
				case CV::S_LOCAL:
					parent = s.as<CV::LOCALSYM>();
					break;
				case CV::S_DEFRANGE_HLSL: {
					auto	&r = *s.as<CV::DEFRANGESYMHLSL>();
					if (r.range().test({sim.AddressToOffset(pc), 1})) {
						const void	*off2	= sim.reg_data(thread, (dx::Operand::Type)r.regType, r.offsets());
						uses[off2]		= RegUse {parent, r.offsetParent, r.sizeInParent};
					}
				}
			}
		}
	}

	Entry	*y		= entries;
	uint32	*pregs	= prev_regs;

	((uint64*)pregs)[0]	= pc;
	y		+= 1;
	pregs	+= 2;

	uint32	active = sim.IsActive(thread) ? 0 : Entry::DISABLED;

	for (auto &i : regspecs) {
		uint32			offset	= i.index * 16;
		const uint32*	regs	= (const uint32*)sim.reg_data(thread, i.type, make_range_n(&offset, 1));

		for (int j = 0; j < 4; j++, y++) {
			buffer_accum<256>	acc(RegisterName(i.type, i.index, 1 << j));
			if (RegUse *use = uses.check(regs))
				DumpField(acc << '(' << get_name(use->parent), to_c_type(spdb, use->parent->typind), use->offset, true) << ')';
			y->name		= acc;
			y->flags	= active | (*regs == 0xcdcdcdcd ? Entry::UNSET : *regs != *pregs ? Entry::CHANGED : 0);
			*pregs++	= *regs++;
		}
	}

	RedrawWindow(*this, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

//-----------------------------------------------------------------------------
//	DXBCLocalsWindow
//-----------------------------------------------------------------------------

void DXBCLocalsWindow::Update(const dx::SimulatorDXBC &sim, ParsedSPDB &spdb, uint64 pc, int thread) {
	frame.clear();

	hash_map_with_key<const CV::SYMTYPE*, uint32>	locals;
	const CV::LOCALSYM	*parent	= 0;
	uint32			loc_size;
	uint64			loc_base;

//	entries.clear();
	tc.DeleteItem(TVI_ROOT);

	if (auto *mod = spdb.HasModule(1)) {
		for (auto &s : mod->Symbols()) {
			switch (s.rectyp) {
				case CV::S_LOCAL: {
					auto	*loc	= s.as<CV::LOCALSYM>();
					if (!parent || parent->name != loc->name || parent->typind != loc->typind) {
						parent		= loc;
						loc_size	= uint32(spdb.GetTypeSize(loc->typind));
						loc_base	= frame.add_chunk(loc_size);
					//	entries.emplace_back(loc->name, to_c_type(spdb, loc->typind), loc_base);
						AppendEntry(loc->name, to_c_type(spdb, loc->typind), loc_base);
					}
					break;
				}
				case CV::S_DEFRANGE_HLSL: {
					auto	&r = *s.as<CV::DEFRANGESYMHLSL>();
					if (r.range().test({sim.AddressToOffset(pc), 1})) {

						switch (r.regType) {
							case dx::Operand::TYPE_STREAM:
								continue;
						}

						ISO_ASSERT(r.offsetParent + r.sizeInParent <= loc_size);
						frame.add_block(loc_base + r.offsetParent, sim.reg_data(thread, (dx::Operand::Type)r.regType, r.offsets()), r.sizeInParent);
					}
					break;
				}
				case CV::S_GDATA_HLSL:
				case CV::S_LDATA_HLSL: {
					auto	*data	= s.as<CV::DATASYMHLSL>();
					loc_size	= uint32(spdb.GetTypeSize(data->typind));
					loc_base	= frame.add_chunk(loc_size);
					frame.add_block(loc_base, (uint8*)sim.grp[data->dataslot]->data().begin() + data->dataoff, loc_size);
					//entries.emplace_back(data->name, to_c_type(spdb, data->typind), loc_base);
					AppendEntry(data->name, to_c_type(spdb, data->typind), loc_base);
					break;
				}
			}
		}
	}

//	Redraw();
}

//-----------------------------------------------------------------------------
//	DXBCTraceWindow
//-----------------------------------------------------------------------------

class DXBCTraceWindow : public TraceWindow {
	struct RegAdder : buffer_accum<256>, ListViewControl::Item {
		ListViewControl			c;
		row_data				&row;
		uint8					types[2];
		bool					source;

		void	SetAddress(uint64 addr) {
			format("%010I64x", addr);
			*getp() = 0;
			Insert(c);
		}
		void	SetDis(const char *dis) {
			reset() << dis;
			*getp() = 0;
			Column(1).Set(c);
		}

		void	AddValue(int i, int reg, int mask, dx::SimulatorDXBC::Register &r) {
			row.fields[i].reg	= reg;
			row.fields[i].write	= !source;
			row.fields[i].mask	= mask;

			float	*f		= (float*)&r;
			for (int j = 0; j < 4; j++) {
				reset() << f[j] << '\0';
				Column(i * 4 + j + 2).Set(c);
			}
		}

		RegAdder(ListViewControl _c, row_data &_row) : ListViewControl::Item(getp()), c(_c), row(_row) {}
	};

	int InsertRegColumns(int nc, const char *name) {
		for (int i = 0; i < 4; i++)
			ListViewControl::Column(buffer_accum<64>(name) << '.' << "xyzw"[i]).Width(120).Insert(c, nc++);
		return nc;
	}
public:
	DXBCTraceWindow(const WindowPos &wpos, dx::SimulatorDXBC &sim, int thread, int max_steps = 0);
};

int get_mask(const dx::ASMOperand &o) {
	switch (o.selection_mode) {
		default:
		case dx::Operand::SELECTION_MASK:
			return o.swizzle_bits;

		case dx::Operand::SELECTION_SWIZZLE: {
			uint8	m = 0;
			for (int bits = o.swizzle_bits | 0x100; bits != 1; bits >>= 2)
				m |= 1 << (bits & 3);
			return m;
		}

		case dx::Operand::SELECTION_SELECT_1:
			return 1 << o.swizzle.x;
	}
}

DXBCTraceWindow::DXBCTraceWindow(const WindowPos &wpos, dx::SimulatorDXBC &sim, int thread, int max_steps) : TraceWindow(wpos, 4) {
	static Disassembler	*dis = Disassembler::Find("DXBC");

	int	nc = 2;
	nc = InsertRegColumns(nc, "dest");
	nc = InsertRegColumns(nc, "input 0");
	nc = InsertRegColumns(nc, "input 1");
	nc = InsertRegColumns(nc, "input 2");

	auto *p = sim.Begin();
	while(p->IsDeclaration())
		p = p->next();

	while (p && max_steps--) {
		RegAdder	ra(c, rows.push_back());
		ra.SetAddress(sim.Address(p));
		
		Disassembler::State	*state = dis->Disassemble(memory_block(unconst(p), p->Length * 4), sim.Address(p));
		buffer_accum<256>	ba;
		state->GetLine(ba, 0);
		ra.SetDis(ba);
		delete state;

		dx::ASMOperation	op(p);

		ra.source = true;
		for (int i = 1; i < op.ops.size(); i++)
			ra.AddValue(i, sim.reg(op.ops[i]), get_mask(op.ops[i]), sim.ref(thread, op.ops[i]));

		p	= sim.ProcessOp(p);
		ra.source = false;
		if (op.ops)
			ra.AddValue(0, sim.reg(op.ops[0]), get_mask(op.ops[0]), sim.ref(thread, op.ops[0]));
	}

	selected	= 0x100;
}

Control app::MakeDXBCTraceWindow(const WindowPos &wpos, dx::SimulatorDXBC &sim, int thread, int max_steps) {
	return *new DXBCTraceWindow(wpos, sim, thread, max_steps);
}

//-----------------------------------------------------------------------------
//	Shader Output
//-----------------------------------------------------------------------------

template<typename T> void FillShaderValues(ListViewControl c, const T *elements, dx::SimulatorDXBC &sim, int col, dx::Operand::Type type, const indices &ix) {
	if (ix) {
		for (auto &i : elements->Elements())
			col = FillColumn(c, col, make_indexed_container(sim.GetRegFile(type, i.register_num).begin(), ix), i.mask, scalar(true));

	} else {
		for (auto &i : elements->Elements())
			col = FillColumn(c, col, sim.GetRegFile(type, i.register_num), i.mask, scalar(true));
	}
}

template<typename T> void FillShaderValues(ListViewControl c, const T *elements, dx::SimulatorDXBC &sim, int col, dx::Operand::Type type, const indices &ix, const dynamic_bitarray<uint32> &enabled) {
	if (ix) {
		for (auto &i : elements->Elements())
			col = FillColumn(c, col, make_indexed_container(sim.GetRegFile(type, i.register_num).begin(), ix), i.mask, make_indexed_container(enabled, ix));

	} else {
		for (auto &i : elements->Elements())
			col = FillColumn(c, col, sim.GetRegFile(type, i.register_num), i.mask, make_indexed_container(enabled));
	}
}

static const C_type *to_c_type(const dx::SIG::Element &e) {
	static const C_type *c_types[4] = {
		0,
		CT(uint32),
		CT(int32),
		CT(float),
	};
	return ctypes.add(C_type_array(c_types[e.component_type], highest_set_index(e.mask) + 1));
}

template<typename T> const C_type *elements_to_c_type(const T *elements) {
	C_type_struct	type;
	for (auto &i : elements->Elements()) {
		string	name	= i.name.get(elements);
		if (i.semantic_index)
			name << i.semantic_index;
		type.add_atoffset(name, to_c_type(i), i.register_num * 16);
	}
	return ctypes.add(move(type));
}

template<typename T> TypedBuffer MakeTypedBuffer(dx::SimulatorDXBC &sim, dx::Operand::Type type, const T *elements) {
	auto			format	= elements_to_c_type(elements);
	auto			stride	= format->size32();
	uint32			num		= sim.NumThreads();
	malloc_block	mem(stride * num);

	char	*dest	= mem;
	for (auto &r : sim.GetRegFile(type, 0)) {
		memcpy(dest, &r, stride);
		dest += stride;
	}

	return TypedBuffer(move(mem), stride, format);
}

Control app::MakeShaderOutput(const WindowPos &wpos, dx::SimulatorDXBC &sim, const dx::Shader &shader, const indices &ix) {
	auto	*dxbc		= shader.DXBC();
	win::Colour col_in0		= MakeColour(2), col_in1  = FadeColour(col_in0);
	win::Colour col_out0	= MakeColour(1), col_out1 = FadeColour(col_out0);

	switch (shader.stage) {
		case dx::HS: {
			sim.Run();
			SplitterWindow		*s		= nullptr;;
			WindowPos			wpos2	= wpos;

			if (sim.ControlPointMask()) {
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
			
			auto	regs = sim.GetRegFile(dx::Operand::TYPE_INPUT_PATCH_CONSTANT);
			for (uint32 m = sim.PatchConstOutputMask(); m; m = clear_lowest(m)) {
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
				uint32				num_cp	= sim.NumOutputControlPoints();
				cp->vw.SetCount(num_cp);

				nc		= cp->AddColumn(0, "#", 50, RGB(255,255,255));
				if (auto *out = dxbc->GetBlob<dx::OSGN>()) {
					nc = SetShaderColumns(cp, out, nc, col_out0, col_out1);
					cp->FillShaderIndex(num_cp);

					dynamic_bitarray<uint32>	enabled(sim.NumThreads());
					enabled.slice(0, num_cp).set_all();
					FillShaderValues(cp->vw, out, sim, 1, dx::Operand::TYPE_OUTPUT_CONTROL_POINT, ix, enabled);
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
			auto	in_topo	= GetTopology(sim.gs_input);

			uint32	num		= sim.NumThreads();
			auto	*in		= dxbc->GetBlob<dx::ISGN>();
			auto	*out	= dxbc->GetBlob<dx::OSG5>();
			int		nc		= c->AddColumn(0, "#", 50, RGB(255,255,255));
			int		in_col	= nc;
			if (in)
				nc = SetShaderColumns(c, in, nc, col_in0, col_in1);

			int		out_col = nc;
			if (out)
				nc = SetShaderColumns(c, out, nc, col_out0, col_out1);

			int	x = 0;
			for (auto &i : sim.uav) {
				nc = MakeHeaders(c->vw, nc, i.format);
				c->AddColour(nc, MakeColour(x++));
			}

			c->FillShaderIndex(num);
			FillShaderValues(c->vw, in, sim, in_col, dx::Operand::TYPE_INPUT, ix);
			
			for (int i = 0; i < num; i++) {
				ListViewControl2::Item	item;
				int		v = in_topo.VertexFromPrim(i, false);
				for (int j = 0; j < sim.max_output - 1; j++)
					item.Insert(c->vw, v * sim.max_output + 1);
			}

			sim.Run();

			dynamic_bitarray<uint32>	enabled(num * sim.max_output);
			for (int i = 0; i < num; i++) {
				for (int j = 0; j < sim.max_output; j++)
					enabled[i * sim.max_output + j] = !sim.IsDiscarded(i);
			}

			if (out) {
				int		col = out_col;
				for (auto &i : out->Elements())
					col = FillColumn(c->vw, col, sim.GetStreamFileAll(i.register_num), i.mask, make_indexed_container(enabled));
			}

			c->vw.SetCount(num);
			c->vw.Show();

			return *c;
		}

		case dx::DS: {
			VertexOutputWindow *c	= new VertexOutputWindow(wpos, "Shader Output", 'DO');

			auto	*out	= dxbc->GetBlob<dx::OSGN>();
			int		nc		= c->AddColumn(0, "#", 50, RGB(255,255,255));
			if (ix)
				nc	= c->AddColumn(nc, "index", 50, RGB(255,255,255));

			int		in_col	= nc;
			int		fields	= sim.tess_domain == dx::DOMAIN_TRI ? 3 : 2;
			for (int i = 0; i < fields; i++)
				nc	= c->AddColumn(nc, string("Domain") << '.' << "xyz"[i], 50, nc & 1 ? col_in1 : col_in0);

			int		out_col = nc;
			if (out)
				nc = SetShaderColumns(c, out, nc, col_out0, col_out1);

			int	x = 0;
			for (auto &i : sim.uav) {
				nc = MakeHeaders(c->vw, nc, i.format);
				c->AddColour(nc, MakeColour(x++));
			}

			uint32		num		= ix ? ix.num : sim.NumThreads();
			c->FillShaderIndex(num);
			if (ix) {
				int	i = 0;
				for (auto x : ix)
					ListViewControl2::Item(to_string(x)).Index(i++).Column(1).Set(c->vw);

				FillColumn(c->vw, in_col, make_indexed_container(sim.GetRegFile(dx::Operand::TYPE_INPUT_DOMAIN_POINT).begin(), ix), bits(fields), scalar(true));
			} else {
				FillColumn(c->vw, in_col, sim.GetRegFile(dx::Operand::TYPE_INPUT_DOMAIN_POINT), bits(fields), scalar(true));
			}

			sim.Run();

			dynamic_bitarray<uint32>	enabled(sim.NumThreads());
			for (int i = 0; i < sim.NumThreads(); i++)
				enabled[i] = !sim.IsDiscarded(i);

			if (out)
				FillShaderValues(c->vw, out, sim, out_col, dx::Operand::TYPE_OUTPUT, ix, enabled);

			c->vw.SetCount(num);
			c->vw.Show();

			return *c;
		}

		default: {
#if 0
			VertexOutputWindow *c	= new VertexOutputWindow(wpos, "Shader Output", ("VPDHGC"[shader.stage] << 8) + 'O');

			auto	*in		= dxbc->GetBlob<dx::ISGN>();
			auto	*out	= dxbc->GetBlob<dx::OSGN>();
			int		nc		= c->AddColumn(0, "#", 50, RGB(255,255,255));
			if (ix)
				nc	= c->AddColumn(nc, "index", 50, RGB(255,255,255));

			int		in_col = nc;
			if (in)
				nc = SetShaderColumns(c, in, nc, col_in0, col_in1);

			int		out_col = nc;
			if (out)
				nc = SetShaderColumns(c, out, nc, col_out0, col_out1);

			int	x = 0;
			for (auto &i : sim.uav) {
				nc = MakeHeaders(c->vw, nc, i.format);
				c->AddColour(nc, MakeColour(x++));
			}

			uint32		num		= ix ? ix.num : sim.NumThreads();
			c->FillShaderIndex(num);
			if (ix) {
				int	i = 0;
				for (auto x : ix)
					ListViewControl2::Item(to_string(x)).Index(i++).Column(1).Set(c->vw);
			}
			FillShaderValues(*c, in, sim, in_col, dx::Operand::TYPE_INPUT, ix);

			sim.Run();

			dynamic_bitarray<uint32>	enabled(sim.NumThreads());
			for (int i = 0; i < sim.NumThreads(); i++)
				enabled[i] = !sim.IsDiscarded(i);

			if (out)
				FillShaderValues(*c, out, sim, out_col, dx::Operand::TYPE_OUTPUT, ix, enabled);

			c->vw.SetCount(num);
			c->vw.Show();

			return *c;
#else
			auto	in = MakeTypedBuffer(sim, dx::Operand::TYPE_INPUT, dxbc->GetBlob<dx::ISGN>());
			sim.Run();
			auto	out = MakeTypedBuffer(sim, dx::Operand::TYPE_OUTPUT, dxbc->GetBlob<dx::OSGN>());
			named<TypedBuffer>		buffers[2] = {
				{"in",	in},
				{"out",	out}
			};

			return *MakeVertexWindow(wpos, "Shader Output", ("VPDHGC"[shader.stage] << 8) + 'O', buffers, 2, ix);
#endif
		}
	}
}

