#ifndef CIL_H
#define CIL_H

#include "base/defs.h"

struct ILMETHOD_SECT : iso::littleendian_types {
	enum SectKind {
		EHTable		= 1,
		OptILTable	= 2,
		KindMask	= 0x3F,	// The mask for decoding the type code
		FatFormat	= 0x40,	// fat format
		MoreSects	= 0x80,	// there is another attribute after this one
	};
	enum ClauseFlags {
		FILTER		= 1,	// EH entry is for a filter
		FINALLY		= 2,	// finally clause
		FAULT		= 4,	// Fault clause (finally that is called on exception only)
		DUPLICATED	= 8,	// duplicated clause (this clause was duplicated to a funclet which was pulled out of line)
	};
	struct Small {
		uint8	kind, size;
		struct Clause {
			uint16			flags;
			uint16			try_offset;
			uint32			try_length:8;
			uint32			handler_offset:16;
			uint32			handler_length:8;
			union {
				uint32		class_token;
				uint32		filter_offset;
			};
		} clauses[1];
	};
	struct Fat {
		uint32	kind:8, size:24;
		struct Clause {
			ClauseFlags	flags;
			uint32		try_offset;
			uint32		try_length;
			uint32		handler_offset;
			uint32		handler_length;
			union {
				uint32	class_token;
				uint32	filter_offset;
			};
		} clauses[1];
	};

	union {
		Small	small;
		Fat		fat;
	};
};

struct ILMETHOD : iso::littleendian_types {
	enum MethodFlags {
		InitLocals      = 0x10,	// call default constructor on all local vars
		MoreSects       = 0x08,	// there is another attribute after this one
		CompressedIL    = 0x40,	// Not used.
		TinyFormat      = 0x02,
		SmallFormat     = 0x00,
		FatFormat       = 0x03,
	};

	// Used when the method is tiny (< 64 bytes), and there are no local vars
	struct Tiny {
		uint8		flags;
		uint32		GetCodeSize()		const { return flags >> 2; }
		uint32		GetMaxStack()		const { return 8; }
		auto		GetCode()			const { return iso::const_memory_block(this + 1, flags >> 2); }
		uint32		GetLocalVarSig()	const { return 0; }
		const ILMETHOD_SECT* GetSect()	const { return 0; }
	};

	struct Fat {
		uint32		flags:12, size:4, maxstack:16;
		uint32		code_size;
		uint32		local_var_sig;		// token that indicates the signature of the local vars (0 means none)
		uint32		GetCodeSize()		const { return code_size; }
		uint32		GetMaxStack()		const { return maxstack; }
		auto		GetCode()			const { return iso::const_memory_block((const uint32*)this + size, code_size); }
		uint32		GetLocalVarSig()	const { return local_var_sig; }
		const ILMETHOD_SECT* GetSect()	const { return flags & MoreSects ? iso::align((const ILMETHOD_SECT*)GetCode().end(), 4) : 0; }
	};

	union {
		Tiny	tiny;
		Fat		fat;
	};
	bool		IsTiny()			const { return (tiny.flags & 3) == TinyFormat; }

	uint32		GetCodeSize()		const { return IsTiny() ? tiny.GetCodeSize()	: fat.GetCodeSize(); }
	uint32		GetMaxStack()		const { return IsTiny() ? tiny.GetMaxStack()	: fat.GetMaxStack(); }
	auto		GetCode()			const { return IsTiny() ? tiny.GetCode()		: fat.GetCode(); }
	uint32		GetLocalVarSig()	const { return IsTiny() ? tiny.GetLocalVarSig() : fat.GetLocalVarSig(); }
	const ILMETHOD_SECT* GetSect()	const { return IsTiny() ? tiny.GetSect()		: fat.GetSect(); }

	// Code follows the Header, then immedately after the code comes any sections (COR_ILMETHOD_SECT).
};

struct cil {
	enum OP {
		NOP				= 0x00,
		BREAK			= 0x01,
		LDARG_0			= 0x02,
		LDARG_1			= 0x03,
		LDARG_2			= 0x04,
		LDARG_3			= 0x05,
		LDLOC_0			= 0x06,
		LDLOC_1			= 0x07,
		LDLOC_2			= 0x08,
		LDLOC_3			= 0x09,
		STLOC_0			= 0x0A,
		STLOC_1			= 0x0B,
		STLOC_2			= 0x0C,
		STLOC_3			= 0x0D,
		LDARG_S			= 0x0E,
		LDARGA_S		= 0x0F,
		STARG_S			= 0x10,
		LDLOC_S			= 0x11,
		LDLOCA_S		= 0x12,
		STLOC_S			= 0x13,
		LDNULL			= 0x14,
		LDC_I4_M1		= 0x15,
		LDC_I4_0		= 0x16,
		LDC_I4_1		= 0x17,
		LDC_I4_2		= 0x18,
		LDC_I4_3		= 0x19,
		LDC_I4_4		= 0x1A,
		LDC_I4_5		= 0x1B,
		LDC_I4_6		= 0x1C,
		LDC_I4_7		= 0x1D,
		LDC_I4_8		= 0x1E,
		LDC_I4_S		= 0x1F,
		LDC_I4			= 0x20,
		LDC_I8			= 0x21,
		LDC_R4			= 0x22,
		LDC_R8			= 0x23,
		UNUSED_24		= 0x24,
		DUP				= 0x25,
		POP				= 0x26,
		JMP				= 0x27,
		CALL			= 0x28,
		CALLI			= 0x29,
		RET				= 0x2A,
		BR_S			= 0x2B,
		BRFALSE_S		= 0x2C,
		BRTRUE_S		= 0x2D,
		BEQ_S			= 0x2E,
		BGE_S			= 0x2F,
		BGT_S			= 0x30,
		BLE_S			= 0x31,
		BLT_S			= 0x32,
		BNE_UN_S		= 0x33,
		BGE_UN_S		= 0x34,
		BGT_UN_S		= 0x35,
		BLE_UN_S		= 0x36,
		BLT_UN_S		= 0x37,
		BR				= 0x38,
		BRFALSE			= 0x39,
		BRTRUE			= 0x3A,
		BEQ				= 0x3B,
		BGE				= 0x3C,
		BGT				= 0x3D,
		BLE				= 0x3E,
		BLT				= 0x3F,
		BNE_UN			= 0x40,
		BGE_UN			= 0x41,
		BGT_UN			= 0x42,
		BLE_UN			= 0x43,
		BLT_UN			= 0x44,
		SWITCHOP		= 0x45,
		LDIND_I1		= 0x46,
		LDIND_U1		= 0x47,
		LDIND_I2		= 0x48,
		LDIND_U2		= 0x49,
		LDIND_I4		= 0x4A,
		LDIND_U4		= 0x4B,
		LDIND_I8		= 0x4C,
		LDIND_I			= 0x4D,
		LDIND_R4		= 0x4E,
		LDIND_R8		= 0x4F,
		LDIND_REF		= 0x50,
		STIND_REF		= 0x51,
		STIND_I1		= 0x52,
		STIND_I2		= 0x53,
		STIND_I4		= 0x54,
		STIND_I8		= 0x55,
		STIND_R4		= 0x56,
		STIND_R8		= 0x57,
		ADD				= 0x58,
		SUB				= 0x59,
		MUL				= 0x5A,
		DIV				= 0x5B,
		DIV_UN			= 0x5C,
		REM				= 0x5D,
		REM_UN			= 0x5E,
		AND				= 0x5F,
		OR				= 0x60,
		XOR				= 0x61,
		SHL				= 0x62,
		SHR				= 0x63,
		SHR_UN			= 0x64,
		NEG				= 0x65,
		NOT				= 0x66,
		CONV_I1			= 0x67,
		CONV_I2			= 0x68,
		CONV_I4			= 0x69,
		CONV_I8			= 0x6A,
		CONV_R4			= 0x6B,
		CONV_R8			= 0x6C,
		CONV_U4			= 0x6D,
		CONV_U8			= 0x6E,
		CALLVIRT		= 0x6F,
		CPOBJ			= 0x70,
		LDOBJ			= 0x71,
		LDSTR			= 0x72,
		NEWOBJ			= 0x73,
		CASTCLASS		= 0x74,
		ISINST			= 0x75,
		CONV_R_UN		= 0x76,
		UNUSED_77		= 0x77,
		UNUSED_78		= 0x78,
		UNBOX			= 0x79,
		THROW			= 0x7A,
		LDFLD			= 0x7B,
		LDFLDA			= 0x7C,
		STFLD			= 0x7D,
		LDSFLD			= 0x7E,
		LDSFLDA			= 0x7F,
		STSFLD			= 0x80,
		STOBJ			= 0x81,
		CONV_OVF_I1_UN	= 0x82,
		CONV_OVF_I2_UN	= 0x83,
		CONV_OVF_I4_UN	= 0x84,
		CONV_OVF_I8_UN	= 0x85,
		CONV_OVF_U1_UN	= 0x86,
		CONV_OVF_U2_UN	= 0x87,
		CONV_OVF_U4_UN	= 0x88,
		CONV_OVF_U8_UN	= 0x89,
		CONV_OVF_I_UN	= 0x8A,
		CONV_OVF_U_UN	= 0x8B,
		BOX				= 0x8C,
		NEWARR			= 0x8D,
		LDLEN			= 0x8E,
		LDELEMA			= 0x8F,
		LDELEM_I1		= 0x90,
		LDELEM_U1		= 0x91,
		LDELEM_I2		= 0x92,
		LDELEM_U2		= 0x93,
		LDELEM_I4		= 0x94,
		LDELEM_U4		= 0x95,
		LDELEM_I8		= 0x96,
		LDELEM_I		= 0x97,
		LDELEM_R4		= 0x98,
		LDELEM_R8		= 0x99,
		LDELEM_REF		= 0x9A,
		STELEM_I		= 0x9B,
		STELEM_I1		= 0x9C,
		STELEM_I2		= 0x9D,
		STELEM_I4		= 0x9E,
		STELEM_I8		= 0x9F,
		STELEM_R4		= 0xA0,
		STELEM_R8		= 0xA1,
		STELEM_REF		= 0xA2,
		LDELEM			= 0xA3,
		STELEM			= 0xA4,
		UNBOX_ANY		= 0xA5,
		UNUSED_A6		= 0xA6,
		UNUSED_A7		= 0xA7,
		UNUSED_A8		= 0xA8,
		UNUSED_A9		= 0xA9,
		UNUSED_AA		= 0xAA,
		UNUSED_AB		= 0xAB,
		UNUSED_AC		= 0xAC,
		UNUSED_AD		= 0xAD,
		UNUSED_AE		= 0xAE,
		UNUSED_AF		= 0xAF,
		UNUSED_B0		= 0xB0,
		UNUSED_B1		= 0xB1,
		UNUSED_B2		= 0xB2,
		CONV_OVF_I1		= 0xB3,
		CONV_OVF_U1		= 0xB4,
		CONV_OVF_I2		= 0xB5,
		CONV_OVF_U2		= 0xB6,
		CONV_OVF_I4		= 0xB7,
		CONV_OVF_U4		= 0xB8,
		CONV_OVF_I8		= 0xB9,
		CONV_OVF_U8		= 0xBA,
		UNUSED_BB		= 0xBB,
		UNUSED_BC		= 0xBC,
		UNUSED_BD		= 0xBD,
		UNUSED_BE		= 0xBE,
		UNUSED_BF		= 0xBF,
		UNUSED_C0		= 0xC0,
		UNUSED_C1		= 0xC1,
		REFANYVAL		= 0xC2,
		CKFINITE		= 0xC3,
		UNUSED_C4		= 0xC4,
		UNUSED_C5		= 0xC5,
		MKREFANY		= 0xC6,
		UNUSED_C7		= 0xC7,
		UNUSED_C8		= 0xC8,
		UNUSED_C9		= 0xC9,
		UNUSED_CA		= 0xCA,
		UNUSED_CB		= 0xCB,
		UNUSED_CC		= 0xCC,
		UNUSED_CD		= 0xCD,
		UNUSED_CE		= 0xCE,
		UNUSED_CF		= 0xCF,
		LDTOKEN			= 0xD0,
		CONV_U2			= 0xD1,
		CONV_U1			= 0xD2,
		CONV_I			= 0xD3,
		CONV_OVF_I		= 0xD4,
		CONV_OVF_U		= 0xD5,
		ADD_OVF			= 0xD6,
		ADD_OVF_UN		= 0xD7,
		MUL_OVF			= 0xD8,
		MUL_OVF_UN		= 0xD9,
		SUB_OVF			= 0xDA,
		SUB_OVF_UN		= 0xDB,
		ENDFINALLY		= 0xDC,
		LEAVE			= 0xDD,
		LEAVE_S			= 0xDE,
		STIND_I			= 0xDF,
		CONV_U			= 0xE0,

		_OP_NUM,
		PREFIX1			= 0xfe,
	};

	enum OP_FE {
		ARGLIST			= 0x00,
		CEQ				= 0x01,
		CGT				= 0x02,
		CGT_UN			= 0x03,
		CLT				= 0x04,
		CLT_UN			= 0x05,
		LDFTN			= 0x06,
		LDVIRTFTN		= 0x07,
		UNUSED_FE08		= 0x08,
		LDARG			= 0x09,
		LDARGA			= 0x0A,
		STARG			= 0x0B,
		LDLOC			= 0x0C,
		LDLOCA			= 0x0D,
		STLOC			= 0x0E,
		LOCALLOC		= 0x0F,
		UNUSED_FE10		= 0x10,
		ENDFILTER		= 0x11,
		UNALIGNED_		= 0x12,
		VOLATILE_		= 0x13,
		TAIL_			= 0x14,
		INITOBJ			= 0x15,
		CONSTRAINED_	= 0x16,
		CPBLK			= 0x17,
		INITBLK			= 0x18,
		NO_				= 0x19,
		RETHROW			= 0x1A,
		UNUSED_FE1B		= 0x1B,
		SIZEOF			= 0x1C,
		REFANYTYPE		= 0x1D,
		READONLY_		= 0x1E,
		_OP_FE_NUM,
	};

	enum PARAMS {
		NONE,

		TARGET_8,		// Short branch target, represented as 1 signed byte from the beginning of the instruction following the current instruction.
		TARGET_32,		// Branch target, represented as a 4-byte signed integer from the beginning of the instruction following the current instruction.

		VAR_8,			// 1-byte integer representing an argument or local variable
		VAR_16,			// 2-byte integer representing an argument or local variable

		INLINE_S8,		// 1-byte integer, signed or unsigned depending on instruction
		INLINE_S32,		// 4-byte integer
		INLINE_S64,		// 8-byte integer
		INLINE_F32,		// 4-byte floating point number
		INLINE_F64,		// 8-byte floating point number
		SWITCH,			// Special for the switch instructions

		TOKEN,					// Arbitrary metadata token (4 bytes) , used for ldtoken instruction, see Partition III for details
		STRING		= TOKEN,	// Metadata token (4 bytes) representing a UserString
		TYPE		= TOKEN,	// Metadata token (4 bytes) representing a TypeDef, TypeRef, or TypeSpec
		METHOD		= TOKEN,	// Metadata token (4 bytes) representing a MethodRef (i.e., a MemberRef to a method) or MethodDef
		FIELD		= TOKEN,	// Metadata token (4 bytes) representing a FieldRef (i.e., a MemberRef to a field) or FieldDef
		SIGNATURE	= TOKEN,	// Metadata token (4 bytes) representing a standalone signature

	};

	enum INPUTS {//8bits
		POP_0		= 0,
		POP_1		= 1,
		POP_I		= 4 + 1,
		POP_I8		= 16 * 1 + 1,
		POP_R4		= 16 * 2 + 1,
		POP_R8		= 16 * 3 + 1,
		POP_8		= 64 + 1,
		POP_REF		= 128 + 1,
		POP_VAR		= 255,
	};

	enum OUTPUTS {//4 bits (maybe 3)
		PUSH_0		= 0,	// no output value
		PUSH_1		= 1,	// one output value, type defined by data flow.
		PUSH_2		= 2,
		PUSH_I		= 3,	// push one native integer or pointer
		PUSH_I8		= 4,	// push one 8-byte integer
		PUSH_R4		= 5,	// push one 4-byte floating point number
		PUSH_R8		= 6,	// push one 8-byte floating point number
		PUSH_REF	= 7,	// push one object reference
		PUSH_VAR	= 8,	// variable number of items pushed, see Partition III for details
	};

	enum FLOW {//3 bits
		FLOW_NEXT,			// control flow unaltered (“fall through”)
		FLOW_BREAK,
		FLOW_BRANCH,		// unconditional branch
		FLOW_CALL,			// method call
		FLOW_COND_BRANCH,	// conditional branch
		FLOW_META,			// unused operation or prefix code
		FLOW_RETURN,		// return from method
		FLOW_THROW,			// throw or rethrow an exception
	};

	union FLAGS {
		iso::uint32	u;
		struct { iso::uint32	_params:4, _outputs:4, _inputs:8, _flow:3; };
		PARAMS	params()	const { return PARAMS(_params); }
		OUTPUTS	outputs()	const { return OUTPUTS(_outputs); }
		INPUTS	inputs()	const { return INPUTS(_inputs); }
		FLOW	flow()		const { return FLOW(_flow); }
		FLAGS()					: u(0)	{}
		FLAGS(iso::uint32 _u)	: u(_u)	{}
	};

	static int	param_len(PARAMS p) {
		static const char param_len[] = {
			0,	//NONE,
			1,	//TARGET_8,
			4,	//TARGET_32,
			1,	//VAR_8,
			2,	//VAR_16,
			1,	//INLINE_S8,
			4,	//INLINE_S32,
			8,	//INLINE_S64,
			4,	//INLINE_F32,
			8,	//INLINE_F64,
			4,	//SWITCH,
			4,	//TOKEN,
//			4,	//STRING,
//			4,	//TYPE,
//			4,	//METHOD,
//			4,	//FIELD,
//			4,	//SIGNATURE,
		};
		return param_len[p];
	}

#define OPDEF(name, mnemonic,inputs, outputs, params, flow)	(params) + ((outputs) << 4) + ((inputs) << 8) + ((flow) << 16)
	static inline FLAGS	get_flags(OP op) {
		static const iso::uint32 flags[] = {
			OPDEF(NOP,				"nop",				POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x00
			OPDEF(BREAK,			"break",			POP_0,					PUSH_0,		NONE,			FLOW_BREAK),				// 0x01
			OPDEF(LDARG_0,			"ldarg.0",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x02
			OPDEF(LDARG_1,			"ldarg.1",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x03
			OPDEF(LDARG_2,			"ldarg.2",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x04
			OPDEF(LDARG_3,			"ldarg.3",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x05
			OPDEF(LDLOC_0,			"ldloc.0",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x06
			OPDEF(LDLOC_1,			"ldloc.1",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x07
			OPDEF(LDLOC_2,			"ldloc.2",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x08
			OPDEF(LDLOC_3,			"ldloc.3",			POP_0,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x09
			OPDEF(STLOC_0,			"stloc.0",			POP_1,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x0A
			OPDEF(STLOC_1,			"stloc.1",			POP_1,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x0B
			OPDEF(STLOC_2,			"stloc.2",			POP_1,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x0C
			OPDEF(STLOC_3,			"stloc.3",			POP_1,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x0D
			OPDEF(LDARG_S,			"ldarg.s",			POP_0,					PUSH_1,		VAR_8,			FLOW_NEXT),					// 0x0E
			OPDEF(LDARGA_S,			"ldarga.s",			POP_0,					PUSH_I,		VAR_8,			FLOW_NEXT),					// 0x0F
			OPDEF(STARG_S,			"starg.s",			POP_1,					PUSH_0,		VAR_8,			FLOW_NEXT),					// 0x10
			OPDEF(LDLOC_S,			"ldloc.s",			POP_0,					PUSH_1,		VAR_8,			FLOW_NEXT),					// 0x11
			OPDEF(LDLOCA_S,			"ldloca.s",			POP_0,					PUSH_I,		VAR_8,			FLOW_NEXT),					// 0x12
			OPDEF(STLOC_S,			"stloc.s",			POP_1,					PUSH_0,		VAR_8,			FLOW_NEXT),					// 0x13
			OPDEF(LDNULL,			"ldnull",			POP_0,					PUSH_REF,	NONE,			FLOW_NEXT),					// 0x14
			OPDEF(LDC_I4_M1,		"ldc.i4.m1",		POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x15
			OPDEF(LDC_I4_0,			"ldc.i4.0",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x16
			OPDEF(LDC_I4_1,			"ldc.i4.1",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x17
			OPDEF(LDC_I4_2,			"ldc.i4.2",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x18
			OPDEF(LDC_I4_3,			"ldc.i4.3",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x19
			OPDEF(LDC_I4_4,			"ldc.i4.4",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x1A
			OPDEF(LDC_I4_5,			"ldc.i4.5",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x1B
			OPDEF(LDC_I4_6,			"ldc.i4.6",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x1C
			OPDEF(LDC_I4_7,			"ldc.i4.7",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x1D
			OPDEF(LDC_I4_8,			"ldc.i4.8",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x1E
			OPDEF(LDC_I4_S,			"ldc.i4.s",			POP_0,					PUSH_I,		INLINE_S8,		FLOW_NEXT),					// 0x1F
			OPDEF(LDC_I4,			"ldc.i4",			POP_0,					PUSH_I,		INLINE_S32,		FLOW_NEXT),					// 0x20
			OPDEF(LDC_I8,			"ldc.i8",			POP_0,					PUSH_I8,	INLINE_S64,		FLOW_NEXT),					// 0x21
			OPDEF(LDC_R4,			"ldc.r4",			POP_0,					PUSH_R4,	INLINE_F32,		FLOW_NEXT),					// 0x22
			OPDEF(LDC_R8,			"ldc.r8",			POP_0,					PUSH_R8,	INLINE_F64,		FLOW_NEXT),					// 0x23
			OPDEF(UNUSED49,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x24
			OPDEF(DUP,				"dup",				POP_1,					PUSH_2,		NONE,			FLOW_NEXT),					// 0x25
			OPDEF(POP,				"pop",				POP_1,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x26
			OPDEF(JMP,				"jmp",				POP_0,					PUSH_0,		METHOD,			FLOW_BRANCH),				// 0x27
			OPDEF(CALL,				"call",				POP_VAR,				PUSH_VAR,	METHOD,			FLOW_CALL),					// 0x28
			OPDEF(CALLI,			"calli",			POP_VAR,				PUSH_VAR,	SIGNATURE,		FLOW_CALL),					// 0x29
			OPDEF(RET,				"ret",				POP_VAR,				PUSH_0,		NONE,			FLOW_RETURN),				// 0x2A
			OPDEF(BR_S,				"br.s",				POP_0,					PUSH_0,		TARGET_8,		FLOW_BRANCH),				// 0x2B
			OPDEF(BRFALSE_S,		"brfalse.s",		POP_I,					PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x2C
			OPDEF(BRTRUE_S,			"brtrue.s",			POP_I,					PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x2D
			OPDEF(BEQ_S,			"beq.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x2E
			OPDEF(BGE_S,			"bge.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x2F
			OPDEF(BGT_S,			"bgt.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x30
			OPDEF(BLE_S,			"ble.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x31
			OPDEF(BLT_S,			"blt.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x32
			OPDEF(BNE_UN_S,			"bne.un.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x33
			OPDEF(BGE_UN_S,			"bge.un.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x34
			OPDEF(BGT_UN_S,			"bgt.un.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x35
			OPDEF(BLE_UN_S,			"ble.un.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x36
			OPDEF(BLT_UN_S,			"blt.un.s",			POP_1+POP_1,			PUSH_0,		TARGET_8,		FLOW_COND_BRANCH),			// 0x37
			OPDEF(BR,				"br",				POP_0,					PUSH_0,		TARGET_32,		FLOW_BRANCH),				// 0x38
			OPDEF(BRFALSE,			"brfalse",			POP_I,					PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x39
			OPDEF(BRTRUE,			"brtrue",			POP_I,					PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x3A
			OPDEF(BEQ,				"beq",				POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x3B
			OPDEF(BGE,				"bge",				POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x3C
			OPDEF(BGT,				"bgt",				POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x3D
			OPDEF(BLE,				"ble",				POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x3E
			OPDEF(BLT,				"blt",				POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x3F
			OPDEF(BNE_UN,			"bne.un",			POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x40
			OPDEF(BGE_UN,			"bge.un",			POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x41
			OPDEF(BGT_UN,			"bgt.un",			POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x42
			OPDEF(BLE_UN,			"ble.un",			POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x43
			OPDEF(BLT_UN,			"blt.un",			POP_1+POP_1,			PUSH_0,		TARGET_32,		FLOW_COND_BRANCH),			// 0x44
			OPDEF(SWITCH,			"switch",			POP_I,					PUSH_0,		SWITCH,			FLOW_COND_BRANCH),			// 0x45
			OPDEF(LDIND_I1,			"ldind.i1",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x46
			OPDEF(LDIND_U1,			"ldind.u1",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x47
			OPDEF(LDIND_I2,			"ldind.i2",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x48
			OPDEF(LDIND_U2,			"ldind.u2",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x49
			OPDEF(LDIND_I4,			"ldind.i4",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x4A
			OPDEF(LDIND_U4,			"ldind.u4",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x4B
			OPDEF(LDIND_I8,			"ldind.i8",			POP_I,					PUSH_I8,	NONE,			FLOW_NEXT),					// 0x4C
			OPDEF(LDIND_I,			"ldind.i",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x4D
			OPDEF(LDIND_R4,			"ldind.r4",			POP_I,					PUSH_R4,	NONE,			FLOW_NEXT),					// 0x4E
			OPDEF(LDIND_R8,			"ldind.r8",			POP_I,					PUSH_R8,	NONE,			FLOW_NEXT),					// 0x4F
			OPDEF(LDIND_REF,		"ldind.ref",		POP_I,					PUSH_REF,	NONE,			FLOW_NEXT),					// 0x50
			OPDEF(STIND_REF,		"stind.ref",		POP_I+POP_I,			PUSH_0,		NONE,			FLOW_NEXT),					// 0x51
			OPDEF(STIND_I1,			"stind.i1",			POP_I+POP_I,			PUSH_0,		NONE,			FLOW_NEXT),					// 0x52
			OPDEF(STIND_I2,			"stind.i2",			POP_I+POP_I,			PUSH_0,		NONE,			FLOW_NEXT),					// 0x53
			OPDEF(STIND_I4,			"stind.i4",			POP_I+POP_I,			PUSH_0,		NONE,			FLOW_NEXT),					// 0x54
			OPDEF(STIND_I8,			"stind.i8",			POP_I+POP_I8,			PUSH_0,		NONE,			FLOW_NEXT),					// 0x55
			OPDEF(STIND_R4,			"stind.r4",			POP_I+POP_R4,			PUSH_0,		NONE,			FLOW_NEXT),					// 0x56
			OPDEF(STIND_R8,			"stind.r8",			POP_I+POP_R8,			PUSH_0,		NONE,			FLOW_NEXT),					// 0x57
			OPDEF(ADD,				"add",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x58
			OPDEF(SUB,				"sub",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x59
			OPDEF(MUL,				"mul",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x5A
			OPDEF(DIV,				"div",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x5B
			OPDEF(DIV_UN,			"div.un",			POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x5C
			OPDEF(REM,				"rem",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x5D
			OPDEF(REM_UN,			"rem.un",			POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x5E
			OPDEF(AND,				"and",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x5F
			OPDEF(OR,				"or",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x60
			OPDEF(XOR,				"xor",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x61
			OPDEF(SHL,				"shl",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x62
			OPDEF(SHR,				"shr",				POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x63
			OPDEF(SHR_UN,			"shr.un",			POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0x64
			OPDEF(NEG,				"neg",				POP_1,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x65
			OPDEF(NOT,				"not",				POP_1,					PUSH_1,		NONE,			FLOW_NEXT),					// 0x66
			OPDEF(CONV_I1,			"conv.i1",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x67
			OPDEF(CONV_I2,			"conv.i2",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x68
			OPDEF(CONV_I4,			"conv.i4",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x69
			OPDEF(CONV_I8,			"conv.i8",			POP_1,					PUSH_I8,	NONE,			FLOW_NEXT),					// 0x6A
			OPDEF(CONV_R4,			"conv.r4",			POP_1,					PUSH_R4,	NONE,			FLOW_NEXT),					// 0x6B
			OPDEF(CONV_R8,			"conv.r8",			POP_1,					PUSH_R8,	NONE,			FLOW_NEXT),					// 0x6C
			OPDEF(CONV_U4,			"conv.u4",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x6D
			OPDEF(CONV_U8,			"conv.u8",			POP_1,					PUSH_I8,	NONE,			FLOW_NEXT),					// 0x6E
			OPDEF(CALLVIRT,			"callvirt",			POP_VAR,				PUSH_VAR,	METHOD,			FLOW_CALL),					// 0x6F
			OPDEF(CPOBJ,			"cpobj",			POP_I+POP_I,			PUSH_0,		TYPE,			FLOW_NEXT),					// 0x70
			OPDEF(LDOBJ,			"ldobj",			POP_I,					PUSH_1,		TYPE,			FLOW_NEXT),					// 0x71
			OPDEF(LDSTR,			"ldstr",			POP_0,					PUSH_REF,	STRING,			FLOW_NEXT),					// 0x72
			OPDEF(NEWOBJ,			"newobj",			POP_VAR,				PUSH_REF,	METHOD,			FLOW_CALL),					// 0x73
			OPDEF(CASTCLASS,		"castclass",		POP_REF,				PUSH_REF,	TYPE,			FLOW_NEXT),					// 0x74
			OPDEF(ISINST,			"isinst",			POP_REF,				PUSH_I,		TYPE,			FLOW_NEXT),					// 0x75
			OPDEF(CONV_R_UN,		"conv.r.un",		POP_1,					PUSH_R8,	NONE,			FLOW_NEXT),					// 0x76
			OPDEF(UNUSED58,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x77
			OPDEF(UNUSED1,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x78
			OPDEF(UNBOX,			"unbox",			POP_REF,				PUSH_I,		TYPE,			FLOW_NEXT),					// 0x79
			OPDEF(THROW,			"throw",			POP_REF,				PUSH_0,		NONE,			FLOW_THROW),				// 0x7A
			OPDEF(LDFLD,			"ldfld",			POP_REF,				PUSH_1,		FIELD,			FLOW_NEXT),					// 0x7B
			OPDEF(LDFLDA,			"ldflda",			POP_REF,				PUSH_I,		FIELD,			FLOW_NEXT),					// 0x7C
			OPDEF(STFLD,			"stfld",			POP_REF+POP_1,			PUSH_0,		FIELD,			FLOW_NEXT),					// 0x7D
			OPDEF(LDSFLD,			"ldsfld",			POP_0,					PUSH_1,		FIELD,			FLOW_NEXT),					// 0x7E
			OPDEF(LDSFLDA,			"ldsflda",			POP_0,					PUSH_I,		FIELD,			FLOW_NEXT),					// 0x7F
			OPDEF(STSFLD,			"stsfld",			POP_1,					PUSH_0,		FIELD,			FLOW_NEXT),					// 0x80
			OPDEF(STOBJ,			"stobj",			POP_I+POP_1,			PUSH_0,		TYPE,			FLOW_NEXT),					// 0x81
			OPDEF(CONV_OVF_I1_UN,	"conv.ovf.i1.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x82
			OPDEF(CONV_OVF_I2_UN,	"conv.ovf.i2.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x83
			OPDEF(CONV_OVF_I4_UN,	"conv.ovf.i4.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x84
			OPDEF(CONV_OVF_I8_UN,	"conv.ovf.i8.un",	POP_1,					PUSH_I8,	NONE,			FLOW_NEXT),					// 0x85
			OPDEF(CONV_OVF_U1_UN,	"conv.ovf.u1.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x86
			OPDEF(CONV_OVF_U2_UN,	"conv.ovf.u2.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x87
			OPDEF(CONV_OVF_U4_UN,	"conv.ovf.u4.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x88
			OPDEF(CONV_OVF_U8_UN,	"conv.ovf.u8.un",	POP_1,					PUSH_I8,	NONE,			FLOW_NEXT),					// 0x89
			OPDEF(CONV_OVF_I_UN,	"conv.ovf.i.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x8A
			OPDEF(CONV_OVF_U_UN,	"conv.ovf.u.un",	POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x8B
			OPDEF(BOX,				"box",				POP_1,					PUSH_REF,	TYPE,			FLOW_NEXT),					// 0x8C
			OPDEF(NEWARR,			"newarr",			POP_I,					PUSH_REF,	TYPE,			FLOW_NEXT),					// 0x8D
			OPDEF(LDLEN,			"ldlen",			POP_REF,				PUSH_I,		NONE,			FLOW_NEXT),					// 0x8E
			OPDEF(LDELEMA,			"ldelema",			POP_REF+POP_I,			PUSH_I,		TYPE,			FLOW_NEXT),					// 0x8F
			OPDEF(LDELEM_I1,		"ldelem.i1",		POP_REF+POP_I,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x90
			OPDEF(LDELEM_U1,		"ldelem.u1",		POP_REF+POP_I,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x91
			OPDEF(LDELEM_I2,		"ldelem.i2",		POP_REF+POP_I,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x92
			OPDEF(LDELEM_U2,		"ldelem.u2",		POP_REF+POP_I,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x93
			OPDEF(LDELEM_I4,		"ldelem.i4",		POP_REF+POP_I,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x94
			OPDEF(LDELEM_U4,		"ldelem.u4",		POP_REF+POP_I,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x95
			OPDEF(LDELEM_I8,		"ldelem.i8",		POP_REF+POP_I,			PUSH_I8,	NONE,			FLOW_NEXT),					// 0x96
			OPDEF(LDELEM_I,			"ldelem.i",			POP_REF+POP_I,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x97
			OPDEF(LDELEM_R4,		"ldelem.r4",		POP_REF+POP_I,			PUSH_R4,	NONE,			FLOW_NEXT),					// 0x98
			OPDEF(LDELEM_R8,		"ldelem.r8",		POP_REF+POP_I,			PUSH_R8,	NONE,			FLOW_NEXT),					// 0x99
			OPDEF(LDELEM_REF,		"ldelem.ref",		POP_REF+POP_I,			PUSH_REF,	NONE,			FLOW_NEXT),					// 0x9A
			OPDEF(STELEM_I,			"stelem.i",			POP_REF+POP_I+POP_I,	PUSH_0,		NONE,			FLOW_NEXT),					// 0x9B
			OPDEF(STELEM_I1,		"stelem.i1",		POP_REF+POP_I+POP_I,	PUSH_0,		NONE,			FLOW_NEXT),					// 0x9C
			OPDEF(STELEM_I2,		"stelem.i2",		POP_REF+POP_I+POP_I,	PUSH_0,		NONE,			FLOW_NEXT),					// 0x9D
			OPDEF(STELEM_I4,		"stelem.i4",		POP_REF+POP_I+POP_I,	PUSH_0,		NONE,			FLOW_NEXT),					// 0x9E
			OPDEF(STELEM_I8,		"stelem.i8",		POP_REF+POP_I+POP_I8,	PUSH_0,		NONE,			FLOW_NEXT),					// 0x9F
			OPDEF(STELEM_R4,		"stelem.r4",		POP_REF+POP_I+POP_R4,	PUSH_0,		NONE,			FLOW_NEXT),					// 0xA0
			OPDEF(STELEM_R8,		"stelem.r8",		POP_REF+POP_I+POP_R8,	PUSH_0,		NONE,			FLOW_NEXT),					// 0xA1
			OPDEF(STELEM_REF,		"stelem.ref",		POP_REF+POP_I+POP_REF,	PUSH_0,		NONE,			FLOW_NEXT),					// 0xA2
			OPDEF(LDELEM,			"ldelem",			POP_0,					PUSH_0,		TYPE,			FLOW_NEXT),					// 0xA3
			OPDEF(STELEM,			"stelem",			POP_0,					PUSH_0,		TYPE,			FLOW_NEXT),					// 0xA4
			OPDEF(UNBOX_ANY,		"unbox.any",		POP_0,					PUSH_0,		TYPE,			FLOW_NEXT),					// 0xA5
			OPDEF(UNUSED5,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xA6
			OPDEF(UNUSED6,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xA7
			OPDEF(UNUSED7,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xA8
			OPDEF(UNUSED8,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xA9
			OPDEF(UNUSED9,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xAA
			OPDEF(UNUSED10,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xAB
			OPDEF(UNUSED11,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xAC
			OPDEF(UNUSED12,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xAD
			OPDEF(UNUSED13,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xAE
			OPDEF(UNUSED14,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xAF
			OPDEF(UNUSED15,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xB0
			OPDEF(UNUSED16,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xB1
			OPDEF(UNUSED17,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xB2
			OPDEF(CONV_OVF_I1,		"conv.ovf.i1",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xB3
			OPDEF(CONV_OVF_U1,		"conv.ovf.u1",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xB4
			OPDEF(CONV_OVF_I2,		"conv.ovf.i2",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xB5
			OPDEF(CONV_OVF_U2,		"conv.ovf.u2",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xB6
			OPDEF(CONV_OVF_I4,		"conv.ovf.i4",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xB7
			OPDEF(CONV_OVF_U4,		"conv.ovf.u4",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xB8
			OPDEF(CONV_OVF_I8,		"conv.ovf.i8",		POP_1,					PUSH_I8,	NONE,			FLOW_NEXT),					// 0xB9
			OPDEF(CONV_OVF_U8,		"conv.ovf.u8",		POP_1,					PUSH_I8,	NONE,			FLOW_NEXT),					// 0xBA
			OPDEF(UNUSED50,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xBB
			OPDEF(UNUSED18,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xBC
			OPDEF(UNUSED19,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xBD
			OPDEF(UNUSED20,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xBE
			OPDEF(UNUSED21,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xBF
			OPDEF(UNUSED22,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xC0
			OPDEF(UNUSED23,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xC1
			OPDEF(REFANYVAL,		"refanyval",		POP_1,					PUSH_I,		TYPE,			FLOW_NEXT),					// 0xC2
			OPDEF(CKFINITE,			"ckfinite",			POP_1,					PUSH_R8,	NONE,			FLOW_NEXT),					// 0xC3
			OPDEF(UNUSED24,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xC4
			OPDEF(UNUSED25,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xC5
			OPDEF(MKREFANY,			"mkrefany",			POP_I,					PUSH_1,		TYPE,			FLOW_NEXT),					// 0xC6
			OPDEF(UNUSED59,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xC7
			OPDEF(UNUSED60,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xC8
			OPDEF(UNUSED61,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xC9
			OPDEF(UNUSED62,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xCA
			OPDEF(UNUSED63,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xCB
			OPDEF(UNUSED64,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xCC
			OPDEF(UNUSED65,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xCD
			OPDEF(UNUSED66,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xCE
			OPDEF(UNUSED67,			"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0xCF
			OPDEF(LDTOKEN,			"ldtoken",			POP_0,					PUSH_I,		TOKEN,			FLOW_NEXT),					// 0xD0
			OPDEF(CONV_U2,			"conv.u2",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xD1
			OPDEF(CONV_U1,			"conv.u1",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xD2
			OPDEF(CONV_I,			"conv.i",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xD3
			OPDEF(CONV_OVF_I,		"conv.ovf.i",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xD4
			OPDEF(CONV_OVF_U,		"conv.ovf.u",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xD5
			OPDEF(ADD_OVF,			"add.ovf",			POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0xD6
			OPDEF(ADD_OVF_UN,		"add.ovf.un",		POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0xD7
			OPDEF(MUL_OVF,			"mul.ovf",			POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0xD8
			OPDEF(MUL_OVF_UN,		"mul.ovf.un",		POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0xD9
			OPDEF(SUB_OVF,			"sub.ovf",			POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0xDA
			OPDEF(SUB_OVF_UN,		"sub.ovf.un",		POP_1+POP_1,			PUSH_1,		NONE,			FLOW_NEXT),					// 0xDB
			OPDEF(ENDFINALLY,		"endfinally",		POP_0,					PUSH_0,		NONE,			FLOW_RETURN),				// 0xDC
			OPDEF(LEAVE,			"leave",			POP_0,					PUSH_0,		TARGET_32,		FLOW_BRANCH),				// 0xDD
			OPDEF(LEAVE_S,			"leave.s",			POP_0,					PUSH_0,		TARGET_8,		FLOW_BRANCH),				// 0xDE
			OPDEF(STIND_I,			"stind.i",			POP_I+POP_I,			PUSH_0,		NONE,			FLOW_NEXT),					// 0xDF
			OPDEF(CONV_U,			"conv.u",			POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0xE0
		};
		return op < iso::num_elements(flags) ? flags[op] : 0;
	}

	static inline FLAGS	get_flags(OP_FE op) {
		static const iso::uint32 flags[] = {
			OPDEF(ARGLIST,			"arglist",			POP_0,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x00
			OPDEF(CEQ,				"ceq",				POP_1+POP_1,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x01
			OPDEF(CGT,				"cgt",				POP_1+POP_1,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x02
			OPDEF(CGT_UN,			"cgt.un",			POP_1+POP_1,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x03
			OPDEF(CLT,				"clt",				POP_1+POP_1,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x04
			OPDEF(CLT_UN,			"clt.un",			POP_1+POP_1,			PUSH_I,		NONE,			FLOW_NEXT),					// 0x05
			OPDEF(LDFTN,			"ldftn",			POP_0,					PUSH_I,		METHOD,			FLOW_NEXT),					// 0x06
			OPDEF(LDVIRTFTN,		"ldvirtftn",		POP_REF,				PUSH_I,		METHOD,			FLOW_NEXT),					// 0x07
			OPDEF(UNUSED_FE08,		"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x08
			OPDEF(LDARG,			"ldarg",			POP_0,					PUSH_1,		VAR_16,			FLOW_NEXT),					// 0x09
			OPDEF(LDARGA,			"ldarga",			POP_0,					PUSH_I,		VAR_16,			FLOW_NEXT),					// 0x0A
			OPDEF(STARG,			"starg",			POP_1,					PUSH_0,		VAR_16,			FLOW_NEXT),					// 0x0B
			OPDEF(LDLOC,			"ldloc",			POP_0,					PUSH_1,		VAR_16,			FLOW_NEXT),					// 0x0C
			OPDEF(LDLOCA,			"ldloca",			POP_0,					PUSH_I,		VAR_16,			FLOW_NEXT),					// 0x0D
			OPDEF(STLOC,			"stloc",			POP_1,					PUSH_0,		VAR_16,			FLOW_NEXT),					// 0x0E
			OPDEF(LOCALLOC,			"localloc",			POP_I,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x0F
			OPDEF(UNUSED_FE10,		"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x10
			OPDEF(ENDFILTER,		"endfilter",		POP_I,					PUSH_0,		NONE,			FLOW_RETURN),				// 0x11
			OPDEF(UNALIGNED,		"unaligned.",		POP_0,					PUSH_0,		INLINE_S8,		FLOW_META),					// 0x12
			OPDEF(VOLATILE,			"volatile.",		POP_0,					PUSH_0,		NONE,			FLOW_META),					// 0x13
			OPDEF(TAILCALL,			"tail.",			POP_0,					PUSH_0,		NONE,			FLOW_META),					// 0x14
			OPDEF(INITOBJ,			"initobj",			POP_I,					PUSH_0,		TYPE,			FLOW_NEXT),					// 0x15
			OPDEF(CONSTRAINED_,		"constrained.",		POP_0,					PUSH_0,		TYPE,			FLOW_NEXT),					// 0x16
			OPDEF(CPBLK,			"cpblk",			POP_I+POP_I+POP_I,		PUSH_0,		NONE,			FLOW_NEXT),					// 0x17
			OPDEF(INITBLK,			"initblk",			POP_I+POP_I+POP_I,		PUSH_0,		NONE,			FLOW_NEXT),					// 0x18
			OPDEF(NO_,				"no.",				POP_0,					PUSH_0,		INLINE_S8,		FLOW_NEXT),					// 0x19
			OPDEF(RETHROW,			"rethrow",			POP_0,					PUSH_0,		NONE,			FLOW_THROW),				// 0x1A
			OPDEF(UNUSED_FE1B,		"unused",			POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x1B
			OPDEF(SIZEOF,			"sizeof",			POP_0,					PUSH_I,		TYPE,			FLOW_NEXT),					// 0x1C
			OPDEF(REFANYTYPE,		"refanytype",		POP_1,					PUSH_I,		NONE,			FLOW_NEXT),					// 0x1D
			OPDEF(READONLY_,		"readonly.",		POP_0,					PUSH_0,		NONE,			FLOW_NEXT),					// 0x1E
		};
		return op < iso::num_elements(flags) ? flags[op] : 0;
	}

	static int oplen(const void *_p) {
		const iso::uint8	*p		= (const iso::uint8*)_p;
		FLAGS	flags;
		OP		op		= (OP)*p++;

		if (op == SWITCHOP)
			return *(iso::packed<iso::uint32le>*)p * 4 + 5;

		if (op < _OP_NUM) {
			flags	= get_flags(op);

		} else if (op == PREFIX1) {
			OP_FE	op	= (OP_FE)*p++;
			if (op < _OP_FE_NUM) {
				flags	= get_flags(op);
			} else {
				return -2;
			}
		} else {
			return -1;
		}

		return p - (const iso::uint8*)_p + param_len(flags.params());
	}

};

#endif //CIL_H

