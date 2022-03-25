#ifndef DX_SHADERS_H
#define DX_SHADERS_H

#include "base/defs.h"
#include "base/pointer.h"

namespace iso { namespace dx {

enum SHADERSTAGE {
	VS,
	PS,
	DS,
	HS,
	GS,
	CS,
};

enum SystemValue {
	SV_UNDEFINED = 0,
	SV_POSITION,
	SV_CLIP_DISTANCE,
	SV_CULL_DISTANCE,
	SV_RENDER_TARGET_ARRAY_INDEX,
	SV_VIEWPORT_ARRAY_INDEX,
	SV_VERTEX_ID,
	SV_PRIMITIVE_ID,
	SV_INSTANCE_ID,
	SV_IS_FRONT_FACE,
	SV_SAMPLE_INDEX,
	SV_FINAL_QUAD_EDGE_TESSFACTOR0,		//		SEMANTIC_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_EDGE_TESSFACTOR1,		//		SEMANTIC_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_EDGE_TESSFACTOR2,		//		SEMANTIC_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_EDGE_TESSFACTOR3,		//		SEMANTIC_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_INSIDE_TESSFACTOR0,	//		SEMANTIC_FINAL_QUAD_U_INSIDE_TESSFACTOR,
	SV_FINAL_QUAD_INSIDE_TESSFACTOR1,	//		SEMANTIC_FINAL_QUAD_V_INSIDE_TESSFACTOR,
	SV_FINAL_TRI_EDGE_TESSFACTOR0,		//		SEMANTIC_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_TRI_EDGE_TESSFACTOR1,		//		SEMANTIC_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_TRI_EDGE_TESSFACTOR2,		//		SEMANTIC_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_TRI_INSIDE_TESSFACTOR,
	SV_FINAL_LINE_DETAIL_TESSFACTOR,
	SV_FINAL_LINE_DENSITY_TESSFACTOR,

	SV_TARGET = 64,
	SV_DEPTH,
	SV_COVERAGE,
	SV_DEPTH_GREATER_EQUAL,
	SV_DEPTH_LESS_EQUAL,
};

enum ResourceDimension {
	RESOURCE_DIMENSION_UNKNOWN = 0,
	RESOURCE_DIMENSION_BUFFER,
	RESOURCE_DIMENSION_TEXTURE1D,
	RESOURCE_DIMENSION_TEXTURE2D,
	RESOURCE_DIMENSION_TEXTURE2DMS,
	RESOURCE_DIMENSION_TEXTURE3D,
	RESOURCE_DIMENSION_TEXTURECUBE,
	RESOURCE_DIMENSION_TEXTURE1DARRAY,
	RESOURCE_DIMENSION_TEXTURE2DARRAY,
	RESOURCE_DIMENSION_TEXTURE2DMSARRAY,
	RESOURCE_DIMENSION_TEXTURECUBEARRAY,
	RESOURCE_DIMENSION_RAW_BUFFER,
	RESOURCE_DIMENSION_STRUCTURED_BUFFER,

	NUM_DIMENSIONS,
};

inline bool is_array(ResourceDimension d) {
	return between(d, RESOURCE_DIMENSION_TEXTURE1DARRAY, RESOURCE_DIMENSION_TEXTURECUBEARRAY);
}
inline bool is_buffer(ResourceDimension d) {
	return d == RESOURCE_DIMENSION_BUFFER || d >= RESOURCE_DIMENSION_RAW_BUFFER;
}
inline int dimensions(ResourceDimension d) {
	return is_buffer(d) || d == RESOURCE_DIMENSION_TEXTURE1D || d == RESOURCE_DIMENSION_TEXTURE1DARRAY ? 1
		: d == RESOURCE_DIMENSION_TEXTURE3D ? 3
		: 2;
}

enum TextureAddressMode {
	TEXTURE_ADDRESS_MODE_WRAP = 1,
	TEXTURE_ADDRESS_MODE_MIRROR,
	TEXTURE_ADDRESS_MODE_CLAMP,
	TEXTURE_ADDRESS_MODE_BORDER,
	TEXTURE_ADDRESS_MODE_MIRROR_ONCE,
};

struct TextureFilterMode {
	enum FILTER {
		POINT		= 0,
		LINEAR		= 1
	};
	enum REDUCTION {
		STANDARD	= 0,
		COMPARISON	= 1,
		MINIMUM		= 2,
		MAXIMUM		= 3
	};
	union {
		struct { uint32	mip:2, mag:2, min:2, aniso:1, reduction:2;};
		uint32	u;
	};
	TextureFilterMode(uint32 _u = 0) : u(_u) {}
};

enum TextureBorderColour {
	BORDER_COLOR_TRANSPARENT_BLACK	= 0,
	BORDER_COLOR_OPAQUE_BLACK,
	BORDER_COLOR_OPAQUE_WHITE,
};

enum ComparisonFunction {
	COMPARISON_NEVER			= 1,
	COMPARISON_LESS				= 2,
	COMPARISON_EQUAL			= 3,
	COMPARISON_LESS_EQUAL		= 4,
	COMPARISON_GREATER			= 5,
	COMPARISON_NOT_EQUAL		= 6,
	COMPARISON_GREATER_EQUAL	= 7,
	COMPARISON_ALWAYS			= 8
};

struct Sampler {
	TextureFilterMode	filter;
	TextureAddressMode	address_u, address_v, address_w;
	float				mip_lod_bias;
	uint32				max_anisotropy;
	ComparisonFunction	comparison_func;
	float				border[4];
	float				min_lod, max_lod;
};

struct DXBC {
	enum FourCC {
		GenericShader				= 0x52444853,	//	SHDR
		GenericShaderEx				= 0x58454853,	//	SHEX
		ResourceDef					= 0x46454452,	//	RDEF
		InputSignature				= 0x4e475349,	//	ISGN
		InputSignature11_1			= 0x31475349,	//	ISG1
		PatchConstantSignature		= 0x47534350,	//	PCSG
		PatchConstantSignature11_1	= 0x31475350,	//	PSG1
		OutputSignature				= 0x4e47534f,	//	OSGN
		OutputSignature5			= 0x3547534f,	//	OSG5
		OutputSignature11_1			= 0x3147534f,	//	OSG1
		ShaderMidLevel				= 0x44494d53,	//	SMID
		Effects10Binary				= 0x30315846,	//	FX10
		EffectsLVM10ConstantTable	= 0x42415443,	//	CTAB
		EffectsLVM10Literals		= 0x34494c43,	//	CLI4
		EffectsLVM10ExeCode			= 0x434c5846,	//	FXLC
		ShaderStatistics			= 0x54415453,	//	STAT
		ShaderDbgInfo				= 0x47424453,	//	SDBG
		ShaderDetails				= 0x4c544453,	//	SDTL
		LegacyShader				= 0x396e6f41,	//	Aon9
		CompilePerf					= 0x46524550,	//	PERF
		InterfaceData				= 0x45434649,	//	IFCE
		ShaderFeatureInfo			= 0x30494653,	//	SFI0
		XnaShader					= 0x53414e58,	//	XNAS
		XnaPrepassShader			= 0x50414e58,	//	XNAP
		ShaderPDB					= 0x42445053,	//	SPDB
		CompilerReport				= 0x54505243,	//	CRPT
		CompileReplay				= 0x594c5052,	//	RPLY
		PrivateData					= 0x56495250,	//	PRIV
		LibraryFunctionSignature	= 0x3053464c,	//	LFS0
		LibraryHeader				= 0x4842494c,	//	LIBH
		LibraryFunction				= 0x4642494c,	//	LIBF
		PSOLibraryIndex				= 0x314f5350,	//	PSO1
		PSODriverNative				= 0x324f5350,	//	PSO2
		RootSignature				= 0x30535452,	//	RTS0
		DXIL						= 0x4c495844,	//	DXIL
		XboxPrecompiledShader		= 0x43504258,	//	XBPC
		XboxDX12Info				= 0x32314258,	//	XB12
		XboxSemanticHash			= 0x48534858,	//	XHSH
		XboxPDBPath					= 0x50445058,	//	XPDP
	};

	struct Version {
		uint16	Major;
		uint16	Minor;
	};

	struct BlobHeader {
		FourCC		code;
		uint32		size;
	};

	uint32		DXBCHeaderFourCC;
	uint8		md5digest[16];
	Version		version;
	uint32		size;
	uint32		num_blobs;

	constexpr bool		valid() const {
		return DXBCHeaderFourCC == "DXBC"_u32;
	}

	BlobHeader				*GetBlob(uint32 i) const {
		return (BlobHeader*)((uint8*)this + ((uint32*)(this + 1))[i]);
	}
	BlobHeader*				FindBlob(FourCC c) const {
		if (valid()) {
			for (uint32 i = 0, n = num_blobs; i < n; i++) {
				BlobHeader	*b = GetBlob(i);
				if (b->code == c)
					return b;
			}
		}
		return 0;
	}
	const_memory_block		GetBlob(FourCC c) const {
		if (BlobHeader *b = FindBlob(c))
			return const_memory_block(b + 1, b->size);
		return none;
	}
	template<typename T> T	*GetBlob()	const {
		if (BlobHeader *b = FindBlob(FourCC(T::code)))
			return (T*)(b + 1);
		return 0;
	}

	memory_block		GetUCode()		const;
};

//-----------------------------------------------------------------------------
//	RDEF
//-----------------------------------------------------------------------------

struct RDEF {
	enum { code = 0x46454452 };	//	RDEF
	template<typename T> struct ptr16 : offset_pointer<T, uint16, RDEF> {};
	template<typename T> struct ptr32 : offset_pointer<T, uint32, RDEF> {};
	enum Stage {
		PIXEL_SHADER	= 0xffff,
		VERTEX_SHADER	= 0xfffe,
		GEOMETRY_SHADER	= 0x4753,
		HULL_SHADER		= 0x4853,
		DOMAIN_SHADER	= 0x4453,
		COMPUTE_SHADER	= 0x4353,
	};
	struct Type {
		uint16			clss;	// D3D10_SHADER_VARIABLE_CLASS
		uint16			type;	// D3D10_SHADER_VARIABLE_TYPE
		uint16			rows;
		uint16			cols;
		uint16			elements;
		uint16			num_members;
		ptr16<void>		members;
	};
	struct Variable {
		enum Flags {
			USERPACKED			= 1,
			USED				= 2,
			INTERFACE_POINTER	= 4,
			INTERFACE_PARAMETER	= 8,
		};
		ptr32<const char>	name;
		uint32				offset;
		uint32				size;
		uint32				flags;
		ptr32<Type>			type;
		ptr32<const void>	def;
		//uint32				unknown[4];	//ffffffff 00000000 ffffffff 00000000
	};
	struct Buffer {
		enum Type {
			CBUFFER,
			TBUFFER,
			INTERFACE_POINTERS,
			RESOURCE_BIND_INFO,
	    };
		enum Flags {
			USERPACKED	= 1,
		};
		ptr32<const char>	name;
		uint32				num_variables;
		ptr32<Variable>		variables;
		uint32				size;
		uint32				flags;
		Type				type;
		range<Variable*>	Variables(const RDEF *rdef)	const { return make_range_n(variables.get(rdef), num_variables); }
	};
	struct Binding {
		enum Type {
			CBUFFER,
			TBUFFER,
			TEXTURE,
			SAMPLER,
			UAV_RWTYPED,
			STRUCTURED,
			UAV_RWSTRUCTURED,
			BYTEADDRESS,
			UAV_RWBYTEADDRESS,
			UAV_APPEND_STRUCTURED,
			UAV_CONSUME_STRUCTURED,
			UAV_RWSTRUCTURED_WITH_COUNTER,
		};
		enum Return {
			R_UNORM		= 1,
			R_SNORM		= 2,
			R_SINT		= 3,
			R_UINT		= 4,
			R_FLOAT		= 5,
			R_MIXED		= 6,
			R_DOUBLE	= 7,
			R_CONTINUED	= 8,
		};
		enum Flags {
			USERPACKED			= 0x1,
			COMPARISON_SAMPLER	= 0x2,
			TEXTURE_COMPONENT_0	= 0x4,
			TEXTURE_COMPONENT_1	= 0x8,
			TEXTURE_COMPONENTS	= 0xc,
			UNUSED				= 0x10,
		};

		ptr32<const char>	name;
		Type				type;
		Return				ret;		//Resource return type
		ResourceDimension	dim;		//Resource view dimension
		uint32				samples;	//Number of samples (0 if not MS texture)
		uint32				bind;
		uint32				bind_count;
		uint32				flags;		//Shader input flags 
	};

	uint32				num_buffers;
	ptr32<Buffer>		buffers;
	uint32				num_bindings;
	ptr32<Binding>		bindings;
	uint16				version;
	uint16				type;		//0xfffe means vertex
	uint32				flags;
	ptr32<const char>	compiler;

	range<Buffer*>	Buffers()	const { return make_range_n(buffers.get(this), num_buffers); }
	range<Binding*>	Bindings()	const { return make_range_n(bindings.get(this), num_bindings); }
};

struct RD11 : RDEF {
	enum { MAGIC = 'RD11' };

	uint32be		magic;//'RD11'
	uint32			sizeof_header;	//0x3c
	uint32			sizeof_buffer;	//0x18
	uint32			sizeof_binding;	//0x20
	uint32			sizeof_variable;//0x28
	uint32			sizeof_type;	//0x0c
	uint32			unknown;		//0

	bool			IsRD11() const { return magic == MAGIC; }

	range<param_iterator<stride_iterator<Buffer>, const RD11*> >	Buffers()	const {
		return make_range_n(make_param_iterator(make_stride_iterator((Buffer*)buffers.get(this), IsRD11() ? sizeof_buffer : sizeof(Buffer)), this), num_buffers);
	}
	range<stride_iterator<Binding> >								Bindings()	const {
		return make_range_n(make_stride_iterator(bindings.get(this), IsRD11() ? sizeof_binding : sizeof(Binding)), num_bindings);
	}
	range<stride_iterator<Variable> >								Variables(const Buffer &b) const {
		return make_range_n(make_stride_iterator(b.variables.get(this), IsRD11() ? sizeof_variable : sizeof(Variable)), b.num_variables);
	}
};

//-----------------------------------------------------------------------------
//	SIG
//-----------------------------------------------------------------------------

struct SIG {
	template<typename T> struct ptr32 : offset_pointer<T, uint32, SIG> {};
	enum ComponentType {
		UNKNOWN = 0,
		UINT32,
		SINT32,
		FLOAT32,
	};
	struct Element {
		ptr32<const char>	name;
		uint32				semantic_index;
		SystemValue			system_value;
		ComponentType		component_type;
		uint32				register_num;
		uint8				mask;
		uint8				rwMask;
		uint16				unused;
	};

	struct Stream {
		uint32		stream;
	};

	struct Element7 : Stream, Element {};

	uint32 num_elements;
	uint32 unknown;
};

template<typename T> struct SIGT : SIG {
	T	elements[1];
	range<const T*>	Elements() const { return make_range_n(&elements[0], num_elements); }
	const T	*find_by_semantic(const char *name, int index) {
		for (auto &i : Elements()) {
			if (istr(i.name.get(this)) == name && i.semantic_index == index)
				return &i;
		}
		return 0;
	}
};

struct ISGN : SIGT<SIG::Element>	{ enum { code = 0x4e475349 }; };
struct ISG1 : SIGT<SIG::Element>	{ enum { code = 0x31475349 }; };
struct PCSG : SIGT<SIG::Element>	{ enum { code = 0x47534350 }; };
struct PSG1 : SIGT<SIG::Element>	{ enum { code = 0x31475350 }; };
struct OSGN : SIGT<SIG::Element>	{ enum { code = 0x4e47534f }; };
struct OSG5 : SIGT<SIG::Element7>	{ enum { code = 0x3547534f }; };
struct OSG1 : SIGT<SIG::Element>	{ enum { code = 0x3147534f }; };

//-----------------------------------------------------------------------------
//	SHDR
//-----------------------------------------------------------------------------

struct SHDR {
	enum { code = 0x52444853 };	//	SHDR
	uint32	MinorVersion : 4, MajorVersion : 4, : 8, ProgramType : 16;
	uint32	len;
	uint32	instructions[0];

	memory_block	GetUCode()	{ return memory_block(instructions, (len - 2) * 4); }
};

//-----------------------------------------------------------------------------
//	SHEX
//-----------------------------------------------------------------------------

struct SHEX {
	enum { code = 0x58454853 };	//	SHEX
	uint32	MinorVersion : 4, MajorVersion : 4, : 8, ProgramType : 16;
	uint32	len;
	uint32	instructions[0];

	memory_block	GetUCode()	{ return memory_block(instructions, (len - 2) * 4); }
};

inline memory_block		DXBC::GetUCode() const {
	if (valid()) {
		if (auto shex = GetBlob<SHEX>())
			return shex->GetUCode();
		if (auto shdr = GetBlob<SHDR>())
			return shdr->GetUCode();
	}
	return empty;
}

//-----------------------------------------------------------------------------
//	RTS0	- root signature
//-----------------------------------------------------------------------------

struct RTS0 {
	enum { code = 0x30535452 };	//	RTS0
	template<typename T> struct ptr32 : offset_pointer<T, uint32, RTS0> {};
	template<typename T> struct Table {
		uint32		num;
		ptr32<T>	ptr;
		range<T*>	entries(const RTS0 *rts0) const	{ return make_range_n(ptr.get(rts0), num); }
	};
	enum FLAGS {
		NONE								= 0,
		ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT	= 0x1,
		DENY_VERTEX_SHADER_ROOT_ACCESS		= 0x2,
		DENY_HULL_SHADER_ROOT_ACCESS		= 0x4,
		DENY_DOMAIN_SHADER_ROOT_ACCESS		= 0x8,
		DENY_GEOMETRY_SHADER_ROOT_ACCESS	= 0x10,
		DENY_PIXEL_SHADER_ROOT_ACCESS		= 0x20,
		ALLOW_STREAM_OUTPUT					= 0x40
	};
	enum VISIBILITY {
		ALL			= 0,
		VERTEX		= 1,
		HULL		= 2,
//		DOMAIN		= 3,
		GEOMETRY	= 4,
		PIXEL		= 5
	};

	struct Sampler {
		TextureFilterMode	filter;
		TextureAddressMode	address_u, address_v, address_w;
		float				mip_lod_bias;
		uint32				max_anisotropy;
		ComparisonFunction	comparison_func;
		TextureBorderColour	border;
		float				min_lod, max_lod;
		uint32				reg, space;
		VISIBILITY			visibility;
    };

	struct Constants {
		uint32	base, space, num;
	};
	struct Descriptor {
		enum FLAGS {
			NONE								= 0,
			DESCRIPTORS_VOLATILE				= 1,
			DATA_VOLATILE						= 2,
			DATA_STATIC_WHILE_SET_AT_EXECUTE	= 4,
			DATA_STATIC							= 8
		};
		uint32	reg, space;
		FLAGS	flags;
	};
	struct Range {
		enum TYPE { SRV = 0, UAV = 1, CBV = 2, SMP = 3 };
		TYPE	type;
		uint32	num, base, space;
		Descriptor::FLAGS	flags;
		uint32	offset;
	};
	typedef Table<Range> DescriptorTable;
	
	struct Parameter {
		enum TYPE { TABLE = 0, CONSTANTS = 1, CBV = 2, SRV = 3, UAV = 4 };
		TYPE		type;
		VISIBILITY	visibility;
		ptr32<void>	ptr;
	};

	uint32				num_tables;	//? == 2
	Table<Parameter>	parameters;
	Table<Sampler>		samplers;
	FLAGS				flags;
};

} }	// namespace iso::dx

#endif	//	DX_SHADERS_H
