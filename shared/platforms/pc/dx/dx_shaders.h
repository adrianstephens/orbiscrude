#ifndef DX_SHADERS_H
#define DX_SHADERS_H

#include "base/defs.h"
#include "base/pointer.h"
#include "base/strings.h"
#include "base/maths.h"
#include "base/interval.h"

namespace iso { namespace dx {

enum SHADERSTAGE : int8 {
	VS,	PS,	DS,	HS,	GS,	AS,	MS,
	NUM_STAGES,
	CS11	= AS,
	CS		= NUM_STAGES,
	SHADER_LIB,
	XS		= -1,
};

enum class ShaderType : uint16 {
	// D3D10
	Pixel			= 0,
	Vertex			= 1,
	Geometry		= 2,

	// D3D11
	Hull			= 3,
	Domain			= 4,
	Compute			= 5,

	// D3D12
	Library			= 6,
	RayGeneration	= 7,
	Intersection	= 8,
	AnyHit			= 9,
	ClosestHit		= 10,
	Miss			= 11,
	Callable		= 12,
	Mesh			= 13,
	Amplification	= 14,

	Max,
};

template<> constexpr SHADERSTAGE iso::conv<SHADERSTAGE>(ShaderType type) {
	return (SHADERSTAGE[]){
		PS,	//Pixel			= 0,
		VS,	//Vertex		= 1,
		GS,	//Geometry		= 2,
		HS,	//Hull			= 3,
		DS,	//Domain		= 4,
		CS,	//Compute		= 5,
		XS,	//Library		= 6,
		XS,	//RayGeneration	= 7,
		XS,	//Intersection	= 8,
		XS,	//AnyHit		= 9,
		XS,	//ClosestHit	= 10,
		XS,	//Miss			= 11,
		XS,	//Callable		= 12,
		MS,	//Mesh			= 13,
		AS,	//Amplification	= 14,
	}[(int)type];
}

enum SystemValue {
	SV_UNDEFINED						= 0,	//	read		write
	SV_POSITION							= 1,	//	?			PS
	SV_CLIP_DISTANCE					= 2,	//	ALL			-VS
	SV_CULL_DISTANCE					= 3,	//	ALL			-VS
	SV_RENDER_TARGET_ARRAY_INDEX		= 4,	//	PS			GS
	SV_VIEWPORT_ARRAY_INDEX				= 5,	//	PS			GS
	SV_VERTEX_ID						= 6,	//	VS			-
	SV_PRIMITIVE_ID						= 7,	// GS,PS,HS,DS	GS,PS	
	SV_INSTANCE_ID						= 8,	//	ALL
	SV_IS_FRONT_FACE					= 9,	//
	SV_SAMPLE_INDEX						= 10,	//	PS			PS
	SV_FINAL_QUAD_EDGE_TESSFACTOR		= 11,	//	DS			HS
	SV_FINAL_QUAD_INSIDE_TESSFACTOR		= 12,	//
	SV_FINAL_TRI_EDGE_TESSFACTOR		= 13,	//
	SV_FINAL_TRI_INSIDE_TESSFACTOR		= 14,	//
	SV_FINAL_LINE_DETAIL_TESSFACTOR		= 15,	//
	SV_FINAL_LINE_DENSITY_TESSFACTOR	= 16,	//
	SV_BARYCENTRICS						= 23,	//
	SV_SHADINGRATE						= 24,	//	PS			VS,GS
	SV_CULLPRIMITIVE					= 25,	//
	SV_TARGET							= 64,	//	-			ALL
	SV_DEPTH							= 65,	//	-			PS
	SV_COVERAGE							= 66,	//	PS			PS
	SV_DEPTH_GREATER_EQUAL				= 67,	//	-			PS
	SV_DEPTH_LESS_EQUAL					= 68,	//	-			PS
	SV_STENCIL_REF						= 69,	//	-			PS
	SV_INNER_COVERAGE					= 70,	//	PS			PS

	//DXBC
	SV_FINAL_QUAD_EDGE_TESSFACTOR0		= 11,	//SEMANTIC_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_EDGE_TESSFACTOR1,				//SEMANTIC_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_EDGE_TESSFACTOR2,				//SEMANTIC_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_EDGE_TESSFACTOR3,				//SEMANTIC_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR,
	SV_FINAL_QUAD_INSIDE_TESSFACTOR0,			//SEMANTIC_FINAL_QUAD_U_INSIDE_TESSFACTOR,
	SV_FINAL_QUAD_INSIDE_TESSFACTOR1,			//SEMANTIC_FINAL_QUAD_V_INSIDE_TESSFACTOR,
	SV_FINAL_TRI_EDGE_TESSFACTOR0,				//SEMANTIC_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_TRI_EDGE_TESSFACTOR1,				//SEMANTIC_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_TRI_EDGE_TESSFACTOR2,				//SEMANTIC_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR,
	SV_FINAL_TRI_INSIDE_TESSFACTOR0,
	SV_FINAL_LINE_DETAIL_TESSFACTOR0,
	SV_FINAL_LINE_DENSITY_TESSFACTOR0,
};

enum class ResourceType : uint32 {
	Invalid = 0,
	Sampler,
	CBV,
	SRVTyped,
	SRVRaw,
	SRVStructured,
	UAVTyped,
	UAVRaw,
	UAVStructured,
	UAVStructuredWithCounter,
	NumEntries
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

	///added so DXIL ResourceDimension can map to something
	RESOURCE_DIMENSION_TYPED_BUFFER,				//(DXIL)
	RESOURCE_DIMENSION_CBUFFER,						//(DXIL)
	RESOURCE_DIMENSION_SAMPLER,						//(DXIL)
	RESOURCE_DIMENSION_TBUFFER,						//(DXIL)
	RESOURCE_DIMENSION_RTACCEL,						//(DXIL)
	RESOURCE_DIMENSION_FEEDBACK_TEXTURE2D,			//(DXIL)
	RESOURCE_DIMENSION_FEEDBACK_TEXTURE2DARRAY,		//(DXIL)
	RESOURCE_DIMENSION_STRUCTURED_BUFFER_COUNTER,	//(DXIL)
	RESOURCE_DIMENSION_SAMPLER_COMPARISON,			//(DXIL)

	NUM_DIMENSIONS,
};

constexpr bool is_texture(ResourceDimension d) {
	return between(d, RESOURCE_DIMENSION_TEXTURE1D, RESOURCE_DIMENSION_TEXTURECUBEARRAY);
}
constexpr bool is_array(ResourceDimension d) {
	return between(d, RESOURCE_DIMENSION_TEXTURE1DARRAY, RESOURCE_DIMENSION_TEXTURECUBEARRAY);
}
constexpr bool is_buffer(ResourceDimension d) {
	return d == RESOURCE_DIMENSION_BUFFER || d >= RESOURCE_DIMENSION_RAW_BUFFER;
}
constexpr int dimensions(ResourceDimension d) {
	return is_buffer(d) || d == RESOURCE_DIMENSION_TEXTURE1D || d == RESOURCE_DIMENSION_TEXTURE1DARRAY ? 1
		: d == RESOURCE_DIMENSION_TEXTURE3D ? 3
		: 2;
}
constexpr int uses_z(ResourceDimension d) {
	return between(d, RESOURCE_DIMENSION_TEXTURE3D, RESOURCE_DIMENSION_TEXTURECUBEARRAY);
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

	template<typename T> static T wrap(T v)		{ return frac(select(v < zero, one - v, v)); }
	template<typename T> static T mirror(T v)	{ return select(v < one, v, two - v); }

	static float address(float v, uint32 s, TextureAddressMode mode) {
		switch (mode) {
			default:
			case TEXTURE_ADDRESS_MODE_WRAP:			return wrap(v / s) * s;
			case TEXTURE_ADDRESS_MODE_MIRROR:		return mirror(wrap(v / s * half)) * s;
			case TEXTURE_ADDRESS_MODE_CLAMP:		return clamp(v, 0, s - 1);
			case TEXTURE_ADDRESS_MODE_MIRROR_ONCE:	return mirror(clamp(v / s, 0, 2)) * s;
		}
	}

	struct Comparer {
		ComparisonFunction	comp;
		float				ref;
		Comparer(ComparisonFunction comp, float ref) : comp(comp), ref(ref) {}
		float	operator()(float f) {
			switch (comp) {
				default:
				case COMPARISON_NEVER:			return 0;
				case COMPARISON_LESS:			return f < ref;
				case COMPARISON_EQUAL:			return f == ref;
				case COMPARISON_LESS_EQUAL:		return f <= ref;
				case COMPARISON_GREATER:		return f > ref;
				case COMPARISON_NOT_EQUAL:		return f != ref;
				case COMPARISON_GREATER_EQUAL:	return f >= ref;
				case COMPARISON_ALWAYS:			return 1;
			}
		}
	};

	friend float clamp_lod(const Sampler *samp, float lod)	{ return samp ? clamp(lod, samp->min_lod, samp->max_lod) : lod; }
	friend float bias_lod(const Sampler *samp, float lod)	{ return samp ? lod + samp->mip_lod_bias : lod; }
};

enum SamplerMode {
	SAMPLER_MODE_DEFAULT = 0,
	SAMPLER_MODE_COMPARISON,
	SAMPLER_MODE_MONO,

	NUM_SAMPLERS,
};

enum InterpolationMode : uint8 {
	INTERPOLATION_UNDEFINED = 0,
	INTERPOLATION_CONSTANT,
	INTERPOLATION_LINEAR,
	INTERPOLATION_LINEAR_CENTROID,
	INTERPOLATION_LINEAR_NOPERSPECTIVE,
	INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID,
	INTERPOLATION_LINEAR_SAMPLE,
	INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE,

	NUM_INTERPOLATIONS,
};

enum TessellatorDomain {
	DOMAIN_UNDEFINED = 0,
	DOMAIN_ISOLINE,
	DOMAIN_TRI,
	DOMAIN_QUAD,

	NUM_DOMAINS,
};

enum TessellatorPartitioning {
	PARTITIONING_UNDEFINED = 0,
	PARTITIONING_INTEGER,
	PARTITIONING_POW2,
	PARTITIONING_FRACTIONAL_ODD,
	PARTITIONING_FRACTIONAL_EVEN,

	NUM_PARTITIONINGS,
};

enum TessellatorOutputPrimitive {
	OUTPUT_PRIMITIVE_UNDEFINED = 0,
	OUTPUT_PRIMITIVE_POINT,
	OUTPUT_PRIMITIVE_LINE,
	OUTPUT_PRIMITIVE_TRIANGLE_CW,
	OUTPUT_PRIMITIVE_TRIANGLE_CCW,

	NUM_OUTPUT_PRIMITIVES,
};

enum PrimitiveTopology {
	TOPOLOGY_UNDEFINED = 0,
	TOPOLOGY_POINTLIST,
	TOPOLOGY_LINELIST,
	TOPOLOGY_LINESTRIP,
	TOPOLOGY_TRIANGLELIST,
	TOPOLOGY_TRIANGLESTRIP,
	TOPOLOGY_LINELIST_ADJ,
	TOPOLOGY_LINESTRIP_ADJ,
	TOPOLOGY_TRIANGLELIST_ADJ,
	TOPOLOGY_TRIANGLESTRIP_ADJ,

	NUM_TOPOLOGYS,
};

enum PrimitiveType {
	PRIMITIVE_UNDEFINED = 0,
	PRIMITIVE_POINT,
	PRIMITIVE_LINE,
	PRIMITIVE_TRIANGLE,
	PRIMITIVE_LINE_ADJ	= 6,
	PRIMITIVE_TRIANGLE_ADJ,
	PRIMITIVE_1_CONTROL_POINT_PATCH,
	PRIMITIVE_2_CONTROL_POINT_PATCH,
	PRIMITIVE_3_CONTROL_POINT_PATCH,
	PRIMITIVE_4_CONTROL_POINT_PATCH,
	PRIMITIVE_5_CONTROL_POINT_PATCH,
	PRIMITIVE_6_CONTROL_POINT_PATCH,
	PRIMITIVE_7_CONTROL_POINT_PATCH,
	PRIMITIVE_8_CONTROL_POINT_PATCH,
	PRIMITIVE_9_CONTROL_POINT_PATCH,
	PRIMITIVE_10_CONTROL_POINT_PATCH,
	PRIMITIVE_11_CONTROL_POINT_PATCH,
	PRIMITIVE_12_CONTROL_POINT_PATCH,
	PRIMITIVE_13_CONTROL_POINT_PATCH,
	PRIMITIVE_14_CONTROL_POINT_PATCH,
	PRIMITIVE_15_CONTROL_POINT_PATCH,
	PRIMITIVE_16_CONTROL_POINT_PATCH,
	PRIMITIVE_17_CONTROL_POINT_PATCH,
	PRIMITIVE_18_CONTROL_POINT_PATCH,
	PRIMITIVE_19_CONTROL_POINT_PATCH,
	PRIMITIVE_20_CONTROL_POINT_PATCH,
	PRIMITIVE_21_CONTROL_POINT_PATCH,
	PRIMITIVE_22_CONTROL_POINT_PATCH,
	PRIMITIVE_23_CONTROL_POINT_PATCH,
	PRIMITIVE_24_CONTROL_POINT_PATCH,
	PRIMITIVE_25_CONTROL_POINT_PATCH,
	PRIMITIVE_26_CONTROL_POINT_PATCH,
	PRIMITIVE_27_CONTROL_POINT_PATCH,
	PRIMITIVE_28_CONTROL_POINT_PATCH,
	PRIMITIVE_29_CONTROL_POINT_PATCH,
	PRIMITIVE_30_CONTROL_POINT_PATCH,
	PRIMITIVE_31_CONTROL_POINT_PATCH,
	PRIMITIVE_32_CONTROL_POINT_PATCH,

	NUM_PRIMITIVES,
};

struct DXBC_base {
	enum FourCC {
		GenericShader				= "SHDR"_u32,
		GenericShaderEx				= "SHEX"_u32,
		ResourceDef					= "RDEF"_u32,
		InputSignature				= "ISGN"_u32,
		InputSignature1				= "ISG1"_u32,
		PatchConstantSignature		= "PCSG"_u32,
		PatchConstantSignature1		= "PSG1"_u32,
		OutputSignature				= "OSGN"_u32,
		OutputSignature5			= "OSG5"_u32,
		OutputSignature1			= "OSG1"_u32,
		ShaderMidLevel				= "SMID"_u32,
		Effects10Binary				= "FX10"_u32,
		EffectsLVM10ConstantTable	= "CTAB"_u32,
		EffectsLVM10Literals		= "CLI4"_u32,
		EffectsLVM10ExeCode			= "FXLC"_u32,
		ShaderStatistics			= "STAT"_u32,
		ShaderDbgInfo				= "SDBG"_u32,
		ShaderDetails				= "SDTL"_u32,
		LegacyShader				= "Aon9"_u32,
		CompilePerf					= "PERF"_u32,
		InterfaceData				= "IFCE"_u32,
		ShaderFeatureInfo			= "SFI0"_u32,
		XnaShader					= "XNAS"_u32,
		XnaPrepassShader			= "XNAP"_u32,
		ShaderPDB					= "SPDB"_u32,
		CompilerReport				= "CRPT"_u32,
		CompileReplay				= "RPLY"_u32,
		PrivateData					= "PRIV"_u32,
		LibraryFunctionSignature	= "LFS0"_u32,
		LibraryHeader				= "LIBH"_u32,
		LibraryFunction				= "LIBF"_u32,
		PSOLibraryIndex				= "PSO1"_u32,
		PSODriverNative				= "PSO2"_u32,
		RootSignature				= "RTS0"_u32,
		DXIL						= "DXIL"_u32,
		PipelineStateValidation		= "PSV0"_u32,
		RuntimeData					= "RDAT"_u32,
		ShaderHash					= "HASH"_u32,
		ShaderSourceInfo			= "SRCI"_u32,
		CompilerVersion				= "VERS"_u32,
		ShaderDebugInfoDXIL			= "ILDB"_u32,	// This is an LLVM module with debug information. It's an augmented version of the original DXIL module. For historical reasons, this is sometimes referred to as 'the PDB of the program'.
		ShaderDebugName				= "ILDN"_u32,	// This is a name for an external entity holding the debug information.

		XboxPrecompiledShader		= "XBPC"_u32,
		XboxDX12Info				= "XB12"_u32,
		XboxSemanticHash			= "XHSH"_u32,
		XboxPDBPath					= "XPDP"_u32,
	};

	template<FourCC T> struct BlobT;

	struct BlobHeader {
		FourCC		code;
		uint32		size;
		const_memory_block		data()	const { return {this + 1, size}; }
		template<FourCC T>		const BlobT<T>*	as() const { return code == T ? (const BlobT<T>*)(this + 1) : nullptr; }
		template<typename T>	const T*		as() const { return code == T::code ? (const T*)(this + 1) : nullptr; }
	};

	struct UcodeHeader {
		uint16		MinorVersion: 4, MajorVersion: 4, _: 8;
		ShaderType	ProgramType : 16;
		UcodeHeader()	{}
		UcodeHeader(uint8 MinorVersion, uint8 MajorVersion, ShaderType ProgramType) : MinorVersion(MinorVersion), MajorVersion(MajorVersion), _(0), ProgramType(ProgramType) {}
	};

	struct Version {
		uint16	Major;
		uint16	Minor;
	};
};

struct DXBC : DXBC_base {
	uint32		HeaderFourCC;
	uint8		digest[16];
	Version		version;
	uint32		size;
	uint32		num_blobs;
	offset_pointer<BlobHeader, uint32, DXBC, false>	blobs[];

	const_memory_block	data()	const {
		return {this, size};
	}
	constexpr bool		valid() const {
		return true;//HeaderFourCC == "DXBC"_u32;
	}
	constexpr bool		valid(size_t _size) const {
		return HeaderFourCC == "DXBC"_u32 && size == _size;
	}

	auto				Blobs() const {
		return with_param2(make_range_n(blobs, valid() ? num_blobs : 0), this);
	}
	const BlobHeader*	FindBlob(uint32 c) const {
		for (auto &i : Blobs()) {
			if (i.code == c)
				return &i;
		}
		return nullptr;
	}
	const_memory_block	GetBlob(uint32 c) const {
		if (auto b = FindBlob(c))
			return b->data();
		return none;
	}
	template<typename T> const T*		GetBlob()	const {
		if (auto b = FindBlob(T::code))
			return b->data();
		return nullptr;
	}
	template<FourCC T> const BlobT<T>*	GetBlob()	const {
		if (auto b = FindBlob(T))
			return b->data();
		return nullptr;
	}
	template<FourCC... T> const common_base_t<BlobT<T>...>*	GetBlobM()	const {
		for (auto i : Blobs()) {
			if (is_any(i->code, T...))
				return b->data();
		}
		return nullptr;
	}

	template<FourCC... C, typename T> bool	GetBlobM(T &result)	const {
		bool	got = false;
		for (auto i : Blobs()) {
			bool	dummy[] = {(got = got || (i.code == C && ((result = i.as<C>()), true)))...};
			if (got)
				return true;
		}
		return false;
	}
	const_memory_block		GetUCode(UcodeHeader &header)	const;
};

//-----------------------------------------------------------------------------
//	RDEF
//-----------------------------------------------------------------------------

template<> struct DXBC::BlobT<DXBC::ResourceDef>  {
	template<typename T> struct ptr16 : offset_pointer<T, uint16, BlobT> {};
	template<typename T> struct ptr32 : offset_pointer<T, uint32, BlobT> {};
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
		range<Variable*>	Variables(const BlobT *rdef)	const { return make_range_n(variables.get(rdef), num_variables); }
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
			RTACCELERATIONSTRUCTURE,
			UAV_FEEDBACKTEXTURE,
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

struct RD11 : DXBC::BlobT<DXBC::ResourceDef>  {
	enum { code = DXBC::ResourceDef, MAGIC = "RD11"_u32 };

	uint32			magic;//'RD11'
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
	enum ComponentType {
		UNKNOWN = 0,
		UINT32	= 1,
		SINT32	= 2,
		FLOAT32 = 3,
		UINT16	= 4,
		SINT16	= 5,
		FLOAT16 = 6,
		UINT64	= 7,
		SINT64	= 8,
		FLOAT64 = 9,
	};

	template<typename T> struct ptr32 : offset_pointer<T, uint32, SIG> {};

	struct Element {
		ptr32<const char>	name;
		uint32				semantic_index;
		SystemValue			system_value;
		ComponentType		component_type;
		uint32				register_num;
		uint8				mask;
		uint8				rwMask;		   // output: shader never writes these; input: shader always reads these
		uint16				unused;
	};

	struct Stream {
		uint32		stream;
	};

	struct Element7 : Stream, Element {};

	struct ElementDXIL : Element7 {
		enum MinPrecision : uint32 { Default = 0, Float16 = 1, Float2_8 = 2, Reserved = 3, SInt16 = 4, UInt16 = 5, Any16 = 0xf0, Any10 = 0xf1 };
		MinPrecision MinPrecision;
	};

	uint32 num_elements;
	uint32 offset_to_elements;
};

template<typename T> struct SIGT : SIG {
	T	elements[1];
	range<const T*>	Elements() const { return make_range_n(&elements[0], num_elements); }
	const T	*find_by_semantic(const char *name, int index) const {
		for (auto &i : Elements()) {
			if (istr(i.name.get(this)) == name && (index < 0 || i.semantic_index == index))
				return &i;
		}
		return 0;
	}
};

struct Signature : range<stride_iterator<SIG::Element>> {
	const SIG	*base;
	Signature() : base(nullptr) {}
	template<typename T> Signature(const SIGT<T> *s) : range<stride_iterator<SIG::Element>>(element_cast<SIG::Element>(s->Elements())), base(s) {}
	operator const SIG*() const { return base; }
	
	const SIG::Element	*find_by_semantic(const char *name, int index) const {
		for (auto &i : *this) {
			if (istr(i.name.get(base)) == name && (index < 0 || i.semantic_index == index))
				return &i;
		}
		return 0;
	}
};


template<> struct DXBC::BlobT<DXBC::InputSignature>			: SIGT<SIG::Element>	{};
template<> struct DXBC::BlobT<DXBC::PatchConstantSignature>	: SIGT<SIG::Element>	{};
template<> struct DXBC::BlobT<DXBC::OutputSignature>		: SIGT<SIG::Element>	{};

template<> struct DXBC::BlobT<DXBC::InputSignature1>		: SIGT<SIG::ElementDXIL> {};
template<> struct DXBC::BlobT<DXBC::PatchConstantSignature1>: SIGT<SIG::ElementDXIL>{};
template<> struct DXBC::BlobT<DXBC::OutputSignature1>		: SIGT<SIG::ElementDXIL> {};

template<> struct DXBC::BlobT<DXBC::OutputSignature5>		: SIGT<SIG::Element7>	{};

//-----------------------------------------------------------------------------
//	SHDR / SHEX
//-----------------------------------------------------------------------------

template<> struct DXBC::BlobT<DXBC::GenericShader> : UcodeHeader {
	uint32	len;
	uint32	instructions[];
	const_memory_block	data()	const	{ return {instructions, (len - 2) * 4}; }
};

template<> struct DXBC::BlobT<DXBC::GenericShaderEx> : UcodeHeader {
	uint32	len;
	uint32	instructions[];
	const_memory_block	data() const	{ return {instructions, (len - 2) * 4}; }
};

//-----------------------------------------------------------------------------
//	RTS0	- root signature
//-----------------------------------------------------------------------------

template<> struct DXBC::BlobT<DXBC::RootSignature> {
	enum { code = 0x30535452 };	//	RTS0
	template<typename T> struct ptr32 : offset_pointer<T, uint32, BlobT> {};
	template<typename T> struct Table {
		uint32		num;
		ptr32<T>	ptr;
		range<T*>	entries(const BlobT *rts0) const	{ return make_range_n(ptr.get(rts0), num); }
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
	struct Sampler1 : Sampler {
		uint32		reg, space;
		VISIBILITY	visibility;
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
	Table<Sampler1>		samplers;
	FLAGS				flags;
};

//-----------------------------------------------------------------------------
//	ShaderSourceInfo
//-----------------------------------------------------------------------------

template<> struct DXBC::BlobT<DXBC::ShaderSourceInfo> {

	struct Args {
		struct Entry {
			embedded_string	name;
			after<embedded_string, embedded_string>	value;
			const Entry *next() const { return (const Entry*)get_after(&value); }
		};
		uint32	Flags;		 // Reserved, must be set to zero
		uint32	SizeInBytes; // Length of all argument pairs, including their null terminators, not including this header
		uint32	Count;		 // Number of arguments
		Entry	_entries[0];
		auto	entries()	const { return make_range_n(make_next_iterator(_entries), Count); }
	};

	struct SourceNames {
		struct Entry {
			uint32 AlignedSizeInBytes;	// Size of the data including this header and padding. Aligned to 4-byte boundary
			uint32 Flags;				// Reserved, must be set to 0
			uint32 NameSizeInBytes;		// Size of the file name, *including* the null terminator
			uint32 ContentSizeInBytes;	// Size of the file content, *including* the null terminator
			embedded_string	filename;
			const Entry*	next() const { return (const Entry*)((const uint8*)this + AlignedSizeInBytes); }
		};
		uint32	Flags;				// Reserved, must be set to 0
		uint32	Count;				// The number of data entries
		uint16	EntriesSizeInBytes;	// The total size of the data entries following this header.
		Entry	_entries[0];
		auto	entries()	const { return make_range_n(make_next_iterator(_entries), Count); }
	};

	struct SourceContents {
		struct Entry {
			uint32 AlignedSizeInBytes;	// Size of the entry including this header and padding. Aligned to 4-byte boundary.
			uint32 Flags;				// Reserved, must be set to 0.
			uint32 ContentSizeInBytes;	// Size of the data following this header, *including* the null terminator
			const_memory_block	data() const { return { this + 1, ContentSizeInBytes}; }
			const Entry*		next() const { return (const Entry*)((const uint8*)this + AlignedSizeInBytes); }
		};
		enum CompressType : uint16 { None, Zlib };
		uint32			AlignedSizeInBytes;				// Size of the entry including this header. Aligned to 4-byte boundary.
		uint16			Flags;							// Reserved, must be set to 0.
		CompressType	CompressType;					// The type of compression used to compress the data
		uint32			EntriesSizeInBytes;				// The size of the data entries following this header.
		uint32			UncompressedEntriesSizeInBytes;	// Total size of the data entries when uncompressed.
		uint32			Count;							// The number of data entries
		Entry			_entries[0];
		// Followed by (compressed) `Count` data entries with the header SourceContentsEntry
		auto	entries()	const { return make_range_n(make_next_iterator(_entries), Count); }
	};

	struct Section {
		enum Type : uint16 {
			SourceContents = 0,
			SourceNames	   = 1,
			Args		   = 2,
		};
		uint32		AlignedSizeInBytes;	 // Size of the section, including this header, and the padding. Aligned to 4-byte boundary.
		uint16		Flags;				 // Reserved, must be set to zero.
		Type		Type;				 // The type of data following this header.
		const Section *next() const { return (const Section*)((const uint8*)this + AlignedSizeInBytes); }
	};

	uint32	AlignedSizeInBytes;	// Total size of the contents including this header
	uint16	Flags;				// Reserved, must be set to zero.
	uint16	Count;				// The number of sections in the source info
	Section	_sections[0];

	auto	sections()	const { return make_range_n(make_next_iterator(_sections), Count); }
};

//-----------------------------------------------------------------------------
// ShaderHash
//-----------------------------------------------------------------------------

template<> struct DXBC::BlobT<DXBC::ShaderHash> {
	enum Flags : uint32 {
		None			= 0,
		IncludesSource	= 1, // This flag indicates that the shader hash was computed taking into account source information (-Zss)
	};
	Flags	flags;
	xint8	digest[16];
};

//-----------------------------------------------------------------------------
// DXIL
//-----------------------------------------------------------------------------

template<> struct DXBC::BlobT<DXBC::DXIL> : UcodeHeader {
	uint32	SizeInUint32	= 0;			// Size in uint32 units including this header.
	uint32	DxilMagic		= "DXIL"_u32;	// 0x4C495844, ASCII "DXIL".
	uint32	DxilVersion		= 262;			// DXIL version.
	uint32	BitcodeOffset	= 16;			// Offset to LLVM bitcode (from DxilMagic).
	uint32	BitcodeSize		= 0;			// Size of LLVM bitcode.

	BlobT(ShaderType type) : UcodeHeader(6, 6, type) {}
	void	set_total_size(uint32 size) {
		BitcodeSize		= size - sizeof(*this);
		SizeInUint32	= (size + 3) / 4;
	}
	void	set_bitcode_size(uint32 size) {
		set_total_size(size + sizeof(*this));
	}
	constexpr bool		valid()	const	{ return DxilMagic == "DXIL"_u32; }
	const_memory_block	data()	const	{ return {(const uint8*)&DxilMagic + BitcodeOffset, BitcodeSize}; }
};

// This is an LLVM module with debug information. It's an augmented version of the original DXIL module. For historical reasons, this is sometimes referred to as 'the PDB of the program'.
template<> struct DXBC::BlobT<DXBC::ShaderDebugInfoDXIL>	: DXBC::BlobT<DXBC::DXIL> {};
template<> struct DXBC::BlobT<DXBC::ShaderStatistics>		: DXBC::BlobT<DXBC::DXIL> {};

template<> struct DXBC::BlobT<DXBC::CompilerVersion> {
	uint16	Major;
	uint16	Minor;
	uint32	VersionFlags;
	uint32	CommitCount;
	uint32	VersionStringListSizeInBytes;
	// Followed by VersionStringListSizeInBytes bytes, containing up to two null-terminated strings, sequentially:
	//  1. CommitSha
	//  1. CustomVersionString
	// Followed by [0-3] zero bytes to align to a 4-byte boundary.
};

// This is a name for an external entity holding the debug information
template<> struct DXBC::BlobT<DXBC::ShaderDebugName> {
	uint16			Flags;			// Reserved, must be set to zero
	uint16			NameLength;		// Length of the debug name, without null terminator
	embedded_string	filename;
};

template<> struct DXBC::BlobT<DXBC::ShaderFeatureInfo> {
	static const uint64
		Feature_Doubles							= 0x0001,
		Feature_ComputeShadersPlusRawAndStructuredBuffersViaShader4X	= 0x0002,
		Feature_UAVsAtEveryStage				= 0x0004,
		Feature_64UAVs							= 0x0008,
		Feature_MinimumPrecision				= 0x0010,
		Feature_11_1_DoubleExtensions			= 0x0020,
		Feature_11_1_ShaderExtensions			= 0x0040,
		Feature_LEVEL9ComparisonFiltering		= 0x0080,
		Feature_TiledResources					= 0x0100,
		Feature_StencilRef						= 0x0200,
		Feature_InnerCoverage					= 0x0400,
		Feature_TypedUAVLoadAdditionalFormats	= 0x0800,
		Feature_ROVs							= 0x1000,
		Feature_ViewportAndRTArrayIndexFromAnyShaderFeedingRasterizer	= 0x2000,
		Feature_WaveOps							= 0x4000,
		Feature_Int64Ops						= 0x8000,
		Feature_ViewID							= 0x10000,
		Feature_Barycentrics					= 0x20000,
		Feature_NativeLowPrecision				= 0x40000,
		Feature_ShadingRate						= 0x80000,
		Feature_Raytracing_Tier_1_1				= 0x100000,
		Feature_SamplerFeedback					= 0x200000,
		Feature_AtomicInt64OnTypedResource		= 0x400000,
		Feature_AtomicInt64OnGroupShared		= 0x800000,
		Feature_DerivativesInMeshAndAmpShaders	= 0x1000000,
		Feature_ResourceDescriptorHeapIndexing	= 0x2000000,
		Feature_SamplerDescriptorHeapIndexing	= 0x4000000,
		Feature_AtomicInt64OnHeapResource		= 0x10000000,
		// SM 6.7+
		Feature_AdvancedTextureOps				= 0x20000000,
		Feature_WriteableMSAATextures			= 0x40000000;

	uint64	FeatureFlags;
};

struct PSV_Resources {
	enum { code = DXBC::PipelineStateValidation };
	template<int> struct ResourceBindInfo;
	template<> struct ResourceBindInfo<0> {
		ResourceType	type;
		uint32			space;
		uint32			LowerBound;
		uint32			UpperBound;
	};
	template<> struct ResourceBindInfo<1> : public ResourceBindInfo<0> {
		enum ResourceKind {
			Unknown = 0,
			Texture1D,
			Texture2D,
			Texture2DMS,
			Texture3D,
			TextureCube,
			Texture1DArray,
			Texture2DArray,
			Texture2DMSArray,
			TextureCubeArray,
			TypedBuffer,
			RawBuffer,
			StructuredBuffer,
			CBuffer,
			Sampler,
			TBuffer,
			RTAccelerationStructure,
			FeedbackTexture2D,
			FeedbackTexture2DArray,
			StructuredBufferWithCounter,
			SamplerComparison,
		};
		ResourceKind	kind;
		uint32			flags;	// special characteristics of the resource
	};

	uint32					RuntimeInfo_size;
	uint8					info;

	template<int N> auto GetResourceBindings() const {
		auto	p		= (const uint32*)(&info + RuntimeInfo_size);
		uint32	count	= p[1] >= sizeof(ResourceBindInfo<N>) ? p[0] : 0;
		return make_range_n(strided((const ResourceBindInfo<N>*)(count ? p + 2 : p + 1), count ? p[1] : 0), count);
	}
};

//-----------------------------------------------------------------------------
// RDAT
//-----------------------------------------------------------------------------

typedef offset_pointer<const char, uint32, const char*>	RDAT_string;

template<> struct DXBC::BlobT<DXBC::RuntimeData> {
	enum {
		Version = 0x10,
	};


	enum Type : uint32 {
		Invalid			= 0,
		StringBuffer	= 1,
		IndexArrays		= 2,
		ResourceTable	= 3,
		FunctionTable	= 4,
		RawBytes		= 5,
		SubobjectTable	= 6,
	};

	struct TableHeader {
		uint32	count;
		uint32	stride;
		template<typename T> auto	get() { return make_range_n(stride_iterator<T>((T*)(this + 1), stride), count); }
	};

	struct PartHeader {

		Type	type;
		uint32	size;  // Not including this header.  Must be 4-byte aligned
		auto						raw()	{ return const_memory_block(this + 1, size); }
		template<typename T> auto	table() { return ((TableHeader*)(this + 1))->get<T>(); }
	};

	struct IndexTableHeader : PartHeader {
		uint32	table[];

		range<const uint32*> row(uint32 i) const {
			if (i < size - 1 && table[i] + i < size)
				return make_range_n(&table[i] + 1, table[i]);
			return none;
		}
	};

	uint32	version;
	uint32	count;
	offset_pointer<PartHeader, uint32, BlobT>	offsets[];

	auto GetPart(int i) {
		return offsets[i].get(this);
	}
	auto	tables() const { return make_range_n(make_param_iterator2(&offsets[0], this), count); }
};

//-----------------------------------------------------------------------------

inline const_memory_block	DXBC::GetUCode(UcodeHeader &header) const {
	if (valid()) {
		if (auto shex = GetBlob<GenericShaderEx>()) {
			header = *shex;
			return shex->data();
		}
		if (auto shdr = GetBlob<GenericShader>()) {
			header = *shdr;
			return shdr->data();
		}
		if (auto dxil = GetBlob<DXIL>()) {
			header = *dxil;
			return dxil->data();
		}
	}
	return empty;
}


} }	// namespace iso::dx

#endif	//	DX_SHADERS_H
