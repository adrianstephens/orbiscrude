#ifndef DX_OPCODES_H
#define DX_OPCODES_H

#include "base/defs.h"
#include "base/array.h"
#include "base/strings.h"

namespace iso { namespace dx {

enum OpcodeType {
	OPCODE_ADD = 0,
	OPCODE_AND,
	OPCODE_BREAK,
	OPCODE_BREAKC,
	OPCODE_CALL,
	OPCODE_CALLC,
	OPCODE_CASE,
	OPCODE_CONTINUE,
	OPCODE_CONTINUEC,
	OPCODE_CUT,
	OPCODE_DEFAULT,
	OPCODE_DERIV_RTX,
	OPCODE_DERIV_RTY,
	OPCODE_DISCARD,
	OPCODE_DIV,
	OPCODE_DP2,
	OPCODE_DP3,
	OPCODE_DP4,
	OPCODE_ELSE,
	OPCODE_EMIT,
	OPCODE_EMITTHENCUT,
	OPCODE_ENDIF,
	OPCODE_ENDLOOP,
	OPCODE_ENDSWITCH,
	OPCODE_EQ,
	OPCODE_EXP,
	OPCODE_FRC,
	OPCODE_FTOI,
	OPCODE_FTOU,
	OPCODE_GE,
	OPCODE_IADD,
	OPCODE_IF,
	OPCODE_IEQ,
	OPCODE_IGE,
	OPCODE_ILT,
	OPCODE_IMAD,
	OPCODE_IMAX,
	OPCODE_IMIN,
	OPCODE_IMUL,
	OPCODE_INE,
	OPCODE_INEG,
	OPCODE_ISHL,
	OPCODE_ISHR,
	OPCODE_ITOF,
	OPCODE_LABEL,
	OPCODE_LD,
	OPCODE_LD_MS,
	OPCODE_LOG,
	OPCODE_LOOP,
	OPCODE_LT,
	OPCODE_MAD,
	OPCODE_MIN,
	OPCODE_MAX,
	OPCODE_CUSTOMDATA,
	OPCODE_MOV,
	OPCODE_MOVC,
	OPCODE_MUL,
	OPCODE_NE,
	OPCODE_NOP,
	OPCODE_NOT,
	OPCODE_OR,
	OPCODE_RESINFO,
	OPCODE_RET,
	OPCODE_RETC,
	OPCODE_ROUND_NE,
	OPCODE_ROUND_NI,
	OPCODE_ROUND_PI,
	OPCODE_ROUND_Z,
	OPCODE_RSQ,
	OPCODE_SAMPLE,
	OPCODE_SAMPLE_C,
	OPCODE_SAMPLE_C_LZ,
	OPCODE_SAMPLE_L,
	OPCODE_SAMPLE_D,
	OPCODE_SAMPLE_B,
	OPCODE_SQRT,
	OPCODE_SWITCH,
	OPCODE_SINCOS,
	OPCODE_UDIV,
	OPCODE_ULT,
	OPCODE_UGE,
	OPCODE_UMUL,
	OPCODE_UMAD,
	OPCODE_UMAX,
	OPCODE_UMIN,
	OPCODE_USHR,
	OPCODE_UTOF,
	OPCODE_XOR,
	OPCODE_DCL_RESOURCE,
	OPCODE_DCL_CONSTANT_BUFFER,
	OPCODE_DCL_SAMPLER,
	OPCODE_DCL_INDEX_RANGE,
	OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY,
	OPCODE_DCL_GS_INPUT_PRIMITIVE,
	OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT,
	OPCODE_DCL_INPUT,
	OPCODE_DCL_INPUT_SGV,
	OPCODE_DCL_INPUT_SIV,
	OPCODE_DCL_INPUT_PS,
	OPCODE_DCL_INPUT_PS_SGV,
	OPCODE_DCL_INPUT_PS_SIV,
	OPCODE_DCL_OUTPUT,
	OPCODE_DCL_OUTPUT_SGV,
	OPCODE_DCL_OUTPUT_SIV,
	OPCODE_DCL_TEMPS,
	OPCODE_DCL_INDEXABLE_TEMP,
	OPCODE_DCL_GLOBAL_FLAGS,

	OPCODE_RESERVED0,

	OPCODE_LOD,
	OPCODE_GATHER4,
	OPCODE_SAMPLE_POS,
	OPCODE_SAMPLE_INFO,

	OPCODE_RESERVED1,

	OPCODE_HS_DECLS,
	OPCODE_HS_CONTROL_POINT_PHASE,
	OPCODE_HS_FORK_PHASE,
	OPCODE_HS_JOIN_PHASE,

	OPCODE_EMIT_STREAM,
	OPCODE_CUT_STREAM,
	OPCODE_EMITTHENCUT_STREAM,
	OPCODE_INTERFACE_CALL,

	OPCODE_BUFINFO,
	OPCODE_DERIV_RTX_COARSE,
	OPCODE_DERIV_RTX_FINE,
	OPCODE_DERIV_RTY_COARSE,
	OPCODE_DERIV_RTY_FINE,
	OPCODE_GATHER4_C,
	OPCODE_GATHER4_PO,
	OPCODE_GATHER4_PO_C,
	OPCODE_RCP,
	OPCODE_F32TOF16,
	OPCODE_F16TOF32,
	OPCODE_UADDC,
	OPCODE_USUBB,
	OPCODE_COUNTBITS,
	OPCODE_FIRSTBIT_HI,
	OPCODE_FIRSTBIT_LO,
	OPCODE_FIRSTBIT_SHI,
	OPCODE_UBFE,
	OPCODE_IBFE,
	OPCODE_BFI,
	OPCODE_BFREV,
	OPCODE_SWAPC,

	OPCODE_DCL_STREAM,
	OPCODE_DCL_FUNCTION_BODY,
	OPCODE_DCL_FUNCTION_TABLE,
	OPCODE_DCL_INTERFACE,

	OPCODE_DCL_INPUT_CONTROL_POINT_COUNT,
	OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT,
	OPCODE_DCL_TESS_DOMAIN,
	OPCODE_DCL_TESS_PARTITIONING,
	OPCODE_DCL_TESS_OUTPUT_PRIMITIVE,
	OPCODE_DCL_HS_MAX_TESSFACTOR,
	OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT,
	OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT,

	OPCODE_DCL_THREAD_GROUP,
	OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED,
	OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW,
	OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED,
	OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW,
	OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED,
	OPCODE_DCL_RESOURCE_RAW,
	OPCODE_DCL_RESOURCE_STRUCTURED,
	OPCODE_LD_UAV_TYPED,
	OPCODE_STORE_UAV_TYPED,
	OPCODE_LD_RAW,
	OPCODE_STORE_RAW,
	OPCODE_LD_STRUCTURED,
	OPCODE_STORE_STRUCTURED,
	OPCODE_ATOMIC_AND,
	OPCODE_ATOMIC_OR,
	OPCODE_ATOMIC_XOR,
	OPCODE_ATOMIC_CMP_STORE,
	OPCODE_ATOMIC_IADD,
	OPCODE_ATOMIC_IMAX,
	OPCODE_ATOMIC_IMIN,
	OPCODE_ATOMIC_UMAX,
	OPCODE_ATOMIC_UMIN,
	OPCODE_IMM_ATOMIC_ALLOC,
	OPCODE_IMM_ATOMIC_CONSUME,
	OPCODE_IMM_ATOMIC_IADD,
	OPCODE_IMM_ATOMIC_AND,
	OPCODE_IMM_ATOMIC_OR,
	OPCODE_IMM_ATOMIC_XOR,
	OPCODE_IMM_ATOMIC_EXCH,
	OPCODE_IMM_ATOMIC_CMP_EXCH,
	OPCODE_IMM_ATOMIC_IMAX,
	OPCODE_IMM_ATOMIC_IMIN,
	OPCODE_IMM_ATOMIC_UMAX,
	OPCODE_IMM_ATOMIC_UMIN,
	OPCODE_SYNC,

	OPCODE_DADD,
	OPCODE_DMAX,
	OPCODE_DMIN,
	OPCODE_DMUL,
	OPCODE_DEQ,
	OPCODE_DGE,
	OPCODE_DLT,
	OPCODE_DNE,
	OPCODE_DMOV,
	OPCODE_DMOVC,
	OPCODE_DTOF,
	OPCODE_FTOD,

	OPCODE_EVAL_SNAPPED,
	OPCODE_EVAL_SAMPLE_INDEX,
	OPCODE_EVAL_CENTROID,

	OPCODE_DCL_GS_INSTANCE_COUNT,

	OPCODE_ABORT,
	OPCODE_DEBUGBREAK,

	OPCODE_RESERVED2,

	OPCODE_DDIV,
	OPCODE_DFMA,
	OPCODE_DRCP,

	OPCODE_MSAD,

	OPCODE_DTOI,
	OPCODE_DTOU,
	OPCODE_ITOD,
	OPCODE_UTOD,

	OPCODE_RESERVED3,

	OPCODE_GATHER4_FEEDBACK,
	OPCODE_GATHER4_C_FEEDBACK,
	OPCODE_GATHER4_PO_FEEDBACK,
	OPCODE_GATHER4_PO_C_FEEDBACK,
	OPCODE_LD_FEEDBACK,
	OPCODE_LD_MS_FEEDBACK,
	OPCODE_LD_UAV_TYPED_FEEDBACK,
	OPCODE_LD_RAW_FEEDBACK,
	OPCODE_LD_STRUCTURED_FEEDBACK,
	OPCODE_SAMPLE_L_FEEDBACK,
	OPCODE_SAMPLE_C_LZ_FEEDBACK,

	OPCODE_SAMPLE_CLAMP_FEEDBACK,
	OPCODE_SAMPLE_B_CLAMP_FEEDBACK,
	OPCODE_SAMPLE_D_CLAMP_FEEDBACK,
	OPCODE_SAMPLE_C_CLAMP_FEEDBACK,

	OPCODE_CHECK_ACCESS_FULLY_MAPPED,

	NUM_OPCODES,
};

enum CustomDataClass {
	CUSTOMDATA_COMMENT = 0,
	CUSTOMDATA_DEBUGINFO,
	CUSTOMDATA_OPAQUE,
	CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER,
	CUSTOMDATA_SHADER_MESSAGE,
	CUSTOMDATA_SHADER_CLIP_PLANE_CONSTANT_BUFFER_MAPPINGS,

	NUM_CUSTOMDATA_CLASSES,
};

enum ResinfoRetType {
	RETTYPE_FLOAT = 0,
	RETTYPE_RCPFLOAT,
	RETTYPE_UINT,

	NUM_RETTYPES,
};

enum ExtendedOpcodeType {
	EXTENDED_OPCODE_EMPTY = 0,
	EXTENDED_OPCODE_SAMPLE_CONTROLS,
	EXTENDED_OPCODE_RESOURCE_DIM,
	EXTENDED_OPCODE_RESOURCE_RETURN_TYPE,

	NUM_EXTENDED_TYPES,
};

enum OperandIndexType {
	INDEX_IMMEDIATE32 = 0,					// 0
	INDEX_IMMEDIATE64,						// 0
	INDEX_RELATIVE,							// [r1]
	INDEX_IMMEDIATE32_PLUS_RELATIVE,		// [r1+0]
	INDEX_IMMEDIATE64_PLUS_RELATIVE,		// [r1+0]

	NUM_INDEX_TYPES
};

enum ExtendedOperandType {
	EXTENDED_OPERAND_EMPTY = 0,
	EXTENDED_OPERAND_MODIFIER,

	NUM_EXTENDED_OPERAND_TYPES,
};

enum OperandModifier {
	OPERAND_MODIFIER_NONE = 0,
	OPERAND_MODIFIER_NEG,
	OPERAND_MODIFIER_ABS,
	OPERAND_MODIFIER_ABSNEG,

	NUM_MODIFIERS,
};

enum MinimumPrecision {
	PRECISION_DEFAULT,
	PRECISION_FLOAT16,
	PRECISION_FLOAT10,
	PRECISION_SINT16,
	PRECISION_UINT16,

	NUM_PRECISIONS,
};

enum CBufferAccessPattern {
	ACCESS_IMMEDIATE_INDEXED	= 0,
	ACCESS_DYNAMIC_INDEXED,

	NUM_PATTERNS,
};


struct Opcode {
	union {
		struct { uint32
			Type:11,
			ResinfoReturn:2,
			Saturate:1,:4,
			TestNonZero:1,
			PreciseValues:4,
			HasOrderPreservingCounter:1,
			Length:7,
			Extended:1;
		};
		struct { uint32 :11,
			Class:21;
		} custom;
		union {
			struct { uint32 :11, Flags:4; };
			struct { uint32 :11, Threads:1, TGSM:1, UAV_Group:1, UAV_Global:1; };
		} sync;
		struct {uint32	// OPCODE_DCL_GLOBAL_FLAGS
			:11,
			RefactoringAllowed:1,
			DoubleFloatOps:1,
			ForceEarlyDepthStencil:1,
			EnableRawStructuredBufs:1,
			SkipOptimisation:1,
			EnableMinPrecision:1,
			EnableD3D11_1DoubleExtensions:1,
			EnableD3D11_1ShaderExtensions:1;
		} global_flags;
		struct {uint32	// OPCODE_DCL_CONSTANT_BUFFER
			:11,
			AccessPattern:1;
		};
		struct {uint32	// OPCODE_DCL_SAMPLER
			:11,
			SamplerMode:4;
		};
		struct {uint32	// OPCODE_DCL_RESOURCE
			:11,
			ResourceDim:5,
			SampleCount:7;
		};
		struct {uint32	// OPCODE_DCL_INPUT_PS
			:11,
			InterpolationMode:4;
		};
		struct {uint32	// OPCODE_DCL_INPUT_CONTROL_POINT_COUNT && OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT
			:11,
			ControlPointCount:6;
		};
		struct {uint32	// OPCODE_DCL_TESS_DOMAIN
			:11,
			TessDomain:2;
		};
		struct {uint32	// OPCODE_DCL_TESS_PARTITIONING
			:11,
			TessPartitioning:3;
		};
		struct {uint32	// OPCODE_DCL_GS_INPUT_PRIMITIVE
			:11,
			InputPrimitive:6;
		};
		struct {uint32	// OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY
			:11,
			OutputPrimitiveTopology:6;
		};
		struct {uint32	// OPCODE_DCL_TESS_OUTPUT_PRIMITIVE
			:11,
			OutputPrimitive:3;
		};
		struct {uint32	// OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED
			:16,
			GloballyCoherant:1;
		};
		uint32	u;
	};

	bool IsDeclaration() const {
		return between(Type, OPCODE_DCL_RESOURCE, OPCODE_DCL_GLOBAL_FLAGS)
			|| between(Type, OPCODE_DCL_STREAM, OPCODE_DCL_RESOURCE_STRUCTURED)
			|| Type == OPCODE_DCL_GS_INSTANCE_COUNT
			|| Type == OPCODE_HS_DECLS
			|| (Type == OPCODE_CUSTOMDATA && custom.Class == CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER);
	}
	size_t			NumOperands() const;
	OpcodeType		Op()		const { return OpcodeType(Type); }

	const uint32	*operands()	const	{ return (uint32*)(this + 1); }
	uint32			length()	const	{ return Type == OPCODE_CUSTOMDATA ? ((uint32*)this)[1] : Length; }
	const Opcode	*next()		const	{ return this + length(); }
};

struct DeclarationCount {
	uint32	// OPCODE_DCL_INTERFACE
		TableLength:16,
		NumInterfaces:16;
};

struct ResourceReturnType {
	enum Type {
		TYPE_UNORM = 1,
		TYPE_SNORM,
		TYPE_SINT,
		TYPE_UINT,
		TYPE_FLOAT,
		TYPE_MIXED,
		TYPE_DOUBLE,
		TYPE_CONTINUED,
		TYPE_UNUSED,
	};
	union {
		struct {Type	x:4, y:4, z:4, w:4; };
		uint16	u;
	};
	ResourceReturnType(uint32 _u = 0) : u(uint16(_u)) {}
};

struct TexelOffset {
	union {
		struct {int16	x:4, y:4, z:4; };
		uint16	u;
	};
	TexelOffset(uint32 _u = 0)			: u(uint16(_u)) {}
	TexelOffset(int8 x, int8 y, int8 z)	: x(x), y(y), z(z) {}
};


union ExtendedOpcode {
	struct {uint32 Type:6, :25, Extended:1; };
	struct {int32  :9, TexelOffsetU:4, TexelOffsetV:4, TexelOffsetW:4;};	//OPCODE_EX_SAMPLE_CONTROLS
	struct {uint32 :9, TexelOffsets:12;};									//OPCODE_EX_SAMPLE_CONTROLS
	struct {uint32 :6, ResourceDim:5, BufferStride:12;};					//OPCODE_EX_RESOURCE_DIM
	struct {uint32 :6, ReturnType:16;};										//OPCODE_EX_RESOURCE_RETURN_TYPE
	uint32	u;
	operator ResourceReturnType()	const { return ReturnType; }
	operator TexelOffset()			const { return TexelOffsets; }
};

struct Operand {
	enum Type {
		TYPE_TEMP					= 0,
		TYPE_INPUT,
		TYPE_OUTPUT,
		TYPE_INDEXABLE_TEMP,
		TYPE_IMMEDIATE32,
		TYPE_IMMEDIATE64,
		TYPE_SAMPLER,
		TYPE_RESOURCE,
		TYPE_CONSTANT_BUFFER,
		TYPE_IMMEDIATE_CONSTANT_BUFFER,
		TYPE_LABEL					= 10,
		TYPE_INPUT_PRIMITIVEID,
		TYPE_OUTPUT_DEPTH,
		TYPE_NULL,
		TYPE_RASTERIZER,
		TYPE_OUTPUT_COVERAGE_MASK,
		TYPE_STREAM,
		TYPE_FUNCTION_BODY,
		TYPE_FUNCTION_TABLE,
		TYPE_INTERFACE,
		TYPE_FUNCTION_INPUT			= 20,
		TYPE_FUNCTION_OUTPUT,
		TYPE_OUTPUT_CONTROL_POINT_ID,
		TYPE_INPUT_FORK_INSTANCE_ID,
		TYPE_INPUT_JOIN_INSTANCE_ID,
		TYPE_INPUT_CONTROL_POINT,
		TYPE_OUTPUT_CONTROL_POINT,
		TYPE_INPUT_PATCH_CONSTANT,
		TYPE_INPUT_DOMAIN_POINT,
		TYPE_THIS_POINTER,
		TYPE_UNORDERED_ACCESS_VIEW	= 30,
		TYPE_THREAD_GROUP_SHARED_MEMORY,
		TYPE_INPUT_THREAD_ID,
		TYPE_INPUT_THREAD_GROUP_ID,
		TYPE_INPUT_THREAD_ID_IN_GROUP,
		TYPE_INPUT_COVERAGE_MASK,
		TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED,
		TYPE_INPUT_GS_INSTANCE_ID,
		TYPE_OUTPUT_DEPTH_GREATER_EQUAL,
		TYPE_OUTPUT_DEPTH_LESS_EQUAL,
		TYPE_CYCLE_COUNTER			= 40,
		TYPE_OUTPUT_STENCIL_REF,
		TYPE_INNER_COVERAGE,

		NUM_TYPES,
	};
	enum SelectionMode {
		SELECTION_MASK = 0,
		SELECTION_SWIZZLE,
		SELECTION_SELECT_1,
	};
	enum NumComponents {
		NUMCOMPS_0 = 0,
		NUMCOMPS_1,
		NUMCOMPS_4,
		NUMCOMPS_N,
	};
	union {
		struct {uint32
			num_components:2,
			selection_mode:2,
			swizzle_bits:8,
			type:8,
			index_dim:2, index0:3, index1:3, index2:3,
			extended:1;
		};
		struct { uint32 :4, x:1, y:1, z:1, w:1; } mask;		// SELECTION_MASK
		struct { uint32	:4, x:2, y:2, z:2, w:2; } swizzle;	// SELECTION_SWIZZLE & SELECTION_SELECT_1
	};
};

struct ExtendedOperand {
	uint32	Type:6, Modifier:8, MinPrecision:3, NonUniform:1, :13, extended:1;
};

struct ASMIndex;

struct ASMOperand : Operand {
	dynamic_array<ASMIndex> indices;
	uint32					values[4];
	OperandModifier			modifier;
	MinimumPrecision		precision;
	uint32					funcNum;

	ASMOperand()					: modifier(OPERAND_MODIFIER_NONE), precision(PRECISION_DEFAULT)	{}
	ASMOperand(const uint32 *&p)	: modifier(OPERAND_MODIFIER_NONE), precision(PRECISION_DEFAULT)	{ Extract(p); }

	bool	Extract(const uint32 *&p);
	int		Index()			const;
	int		IndexMulti()	const;
	int		Mask()			const;
	int		NumComponents()	const { return num_components == NUMCOMPS_4 ? 4 : num_components; }
};

struct ASMIndex {
	ASMOperand	operand;
	uint64		index;
	bool		absolute;	// use index as an absolute value
	bool		relative;	// use the operand

	ASMIndex() : index(0), absolute(false), relative(false) {}
};

struct ASMOperation : Opcode {
	dynamic_array<ASMOperand>	ops;
	ResourceReturnType			ret;
	TexelOffset					tex_offset;
	uint32						dim:4, stride:12;
	ASMOperation(const Opcode *op);
};

string_accum &RegisterName(string_accum &a, const Operand::Type type, range<uint32*> indices, int fields);
inline string RegisterName(const Operand::Type type, range<uint32*> indices, int fields) {
	string_builder	b;
	RegisterName(b, type, indices, fields);
	return b;
}
inline string_accum &RegisterName(string_accum &a, const Operand::Type type, uint32 index, int fields) {
	return RegisterName(a, type, make_range_n(&index, 1), fields);
}
inline string RegisterName(const Operand::Type type, uint32 index, int fields) {
	return RegisterName(type, make_range_n(&index, 1), fields);
}


} }	// namespace iso::dx

#endif // DX_OPCODES_H

