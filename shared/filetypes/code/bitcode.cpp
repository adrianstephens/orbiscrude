#include "base/defs.h"
#include "base/array.h"
#include "base/hash.h"
#include "base/strings.h"
#include "codec/vlc.h"

//-----------------------------------------------------------------------------
//	LLVM bitcode
//-----------------------------------------------------------------------------

using namespace iso;

namespace bitc {

#define assert	ISO_ASSERT
#define llvm_unreachable(x)		ISO_ASSERT2(0, x);unreachable()
#define report_fatal_error(x)	ISO_ASSERT2(0, x)

enum StandardWidths {
	BlockIDWidth	= 8,		// We use VBR-8 for block IDs.
	CodeLenWidth	= 4,		// Codelen are VBR-4.
	BlockSizeWidth	= 32		// BlockSize up to 2^32 32-bit words = 16GB per block.
};

// The standard abbrev namespace always has a way to exit a block, enter a nested block, define abbrevs, and define an unabbreviated record
enum FixedAbbrevIDs {
	END_BLOCK 					= 0,	// Must be zero to guarantee termination for broken bitcode.
	ENTER_SUBBLOCK 				= 1,
	DEFINE_ABBREV 				= 2,	// defines an abbrev for the current block.  It consists of a vbr5 for # operand infos.  Each operand info is emitted with a single bit to indicate if it is a literal encoding.  If so, the value is emitted with a vbr8.  If not, the encoding is emitted as 3 bits followed by the info value as a vbr5 if needed.
	UNABBREV_RECORD 			= 3,	// emitted with a vbr6 for the record code, followed by a vbr6 for the # operands, followed by vbr6's for each operand.
	FIRST_APPLICATION_ABBREV	= 4		// just a marker for the first abbrev assignment.
};

// StandardBlockIDs - All bitcode files can optionally include a BLOCKINFO block, which contains metadata about other blocks in the file
enum StandardBlockIDs {
	BLOCKINFO_BLOCK_ID			= 0,	// defines metadata about blocks, for example, standard abbrevs that should be available to all blocks of a specified id.
	// Block IDs 1-7 are reserved for future expansion
	FIRST_APPLICATION_BLOCKID	= 8
};

// BlockInfoCodes - The blockinfo block contains metadata about user-defined blocks
enum BlockInfoCodes {
	// DEFINE_ABBREV has magic semantics here, applying to the current SETBID'd block, instead of the BlockInfo block.
	BLOCKINFO_CODE_SETBID 		= 1,	// SETBID: [blockid#]
	BLOCKINFO_CODE_BLOCKNAME 	= 2,	// BLOCKNAME: [name]
	BLOCKINFO_CODE_SETRECORDNAME= 3		// BLOCKINFO_CODE_SETRECORDNAME: [id, name]
};

// The only top-level block type defined is for a module.
enum BlockIDs {
	// Blocks
	MODULE_BLOCK_ID = FIRST_APPLICATION_BLOCKID,

	// Module sub-block id's
	PARAMATTR_BLOCK_ID,
	PARAMATTR_GROUP_BLOCK_ID,

	CONSTANTS_BLOCK_ID,
	FUNCTION_BLOCK_ID,

	UNUSED_ID1,

	VALUE_SYMTAB_BLOCK_ID,
	METADATA_BLOCK_ID,
	METADATA_ATTACHMENT_ID,

	TYPE_BLOCK_ID_NEW,

	USELIST_BLOCK_ID
};

// MODULE blocks have a number of optional fields and subblocks.
enum ModuleCodes {
	MODULE_CODE_VERSION 		= 1,    // VERSION:		[version#]
	MODULE_CODE_TRIPLE 			= 2,    // TRIPLE:		[strchr x N]
	MODULE_CODE_DATALAYOUT 		= 3,    // DATALAYOUT:	[strchr x N]
	MODULE_CODE_ASM 			= 4,    // ASM:			[strchr x N]
	MODULE_CODE_SECTIONNAME		= 5,    // SECTIONNAME:	[strchr x N]

	// FIXME: Remove DEPLIB in 4.0.
	MODULE_CODE_DEPLIB 			= 6,    // DEPLIB:		[strchr x N]
	MODULE_CODE_GLOBALVAR 		= 7,	// GLOBALVAR:	[pointer type, isconst, initid, linkage, alignment, section, visibility, threadlocal]
	MODULE_CODE_FUNCTION 		= 8,	// FUNCTION:	[type, callingconv, isproto, linkage, paramattrs, alignment, section, visibility, gc, unnamed_addr]
	MODULE_CODE_ALIAS 			= 9,	// ALIAS:		[alias type, aliasee val#, linkage, visibility]
	MODULE_CODE_PURGEVALS 		= 10,	// MODULE_CODE_PURGEVALS: [numvals]
	MODULE_CODE_GCNAME 			= 11	// GCNAME:		[strchr x N]
};

// PARAMATTR blocks have code for defining a parameter attribute set
enum AttributeCodes {
	// FIXME: Remove `PARAMATTR_CODE_ENTRY_OLD' in 4.0
	PARAMATTR_CODE_ENTRY_OLD 	= 1, // ENTRY: [paramidx0, attr0, paramidx1, attr1...]
	PARAMATTR_CODE_ENTRY 		= 2, // ENTRY: [paramidx0, attrgrp0, paramidx1, attrgrp1, ...]
	PARAMATTR_GRP_CODE_ENTRY 	= 3  // ENTRY: [id, attr0, att1, ...]
};

// TYPE blocks have codes for each type primitive they use
enum TypeCodes {
	TYPE_CODE_NUMENTRY 			= 1,    // NUMENTRY: [numentries]

	// Type Codes
	TYPE_CODE_VOID 				= 2,	// VOID
	TYPE_CODE_FLOAT 			= 3,	// FLOAT
	TYPE_CODE_DOUBLE 			= 4,	// DOUBLE
	TYPE_CODE_LABEL 			= 5,	// LABEL
	TYPE_CODE_OPAQUE 			= 6,	// OPAQUE
	TYPE_CODE_INTEGER 			= 7,	// INTEGER: [width]
	TYPE_CODE_POINTER 			= 8,	// POINTER: [pointee type]

	TYPE_CODE_FUNCTION_OLD 		= 9,	// FUNCTION: [vararg, attrid, retty, paramty x N]

	TYPE_CODE_HALF 				= 10,	// HALF

	TYPE_CODE_ARRAY 			= 11,	// ARRAY: [numelts, eltty]
	TYPE_CODE_VECTOR 			= 12,	// VECTOR: [numelts, eltty]

	// These are not with the other floating point types because they're a late addition, and putting them in the right place breaks binary compatibility
	TYPE_CODE_X86_FP80 			= 13,	// X86 LONG DOUBLE
	TYPE_CODE_FP128 			= 14,	// LONG DOUBLE (112 bit mantissa)
	TYPE_CODE_PPC_FP128 		= 15,	// PPC LONG DOUBLE (2 doubles)

	TYPE_CODE_METADATA 			= 16,	// METADATA

	TYPE_CODE_X86_MMX 			= 17,	// X86 MMX

	TYPE_CODE_STRUCT_ANON 		= 18,	// STRUCT_ANON: [ispacked, eltty x N]
	TYPE_CODE_STRUCT_NAME 		= 19,	// STRUCT_NAME: [strchr x N]
	TYPE_CODE_STRUCT_NAMED 		= 20,	// STRUCT_NAMED: [ispacked, eltty x N]

	TYPE_CODE_FUNCTION 			= 21	// FUNCTION: [vararg, retty, paramty x N]
};

// The type symbol table only has one code (TST_ENTRY_CODE)
enum TypeSymtabCodes {
	TST_CODE_ENTRY 				= 1		// TST_ENTRY: [typeid, namechar x N]
};

// The value symbol table only has one code (VST_ENTRY_CODE).
enum ValueSymtabCodes {
	VST_CODE_ENTRY 				= 1,	// VST_ENTRY: [valid, namechar x N]
	VST_CODE_BBENTRY 			= 2		// VST_BBENTRY: [bbid, namechar x N]
};

enum MetadataCodes {
	METADATA_STRING 			= 1,	// MDSTRING:		[values] 2 is unused. 3 is unused.
	METADATA_NAME 				= 4,	// STRING:			[values] 5 is unused.
	METADATA_KIND 				= 6,	// [n x [id, name]] 7 is unused.
	METADATA_NODE 				= 8,	// NODE:			[n x (type num, value num)]
	METADATA_FN_NODE 			= 9,	// FN_NODE:			[n x (type num, value num)]
	METADATA_NAMED_NODE 		= 10,	// NAMED_NODE:		[n x mdnodes]
	METADATA_ATTACHMENT 		= 11	// [m x [value, [n x [id, mdnode]]]
};

// The constants block (CONSTANTS_BLOCK_ID) describes emission for each constant and maintains an implicit current type value.
enum ConstantsCodes {
	CST_CODE_SETTYPE 			= 1,	// SETTYPE:			[typeid]
	CST_CODE_NULL 				= 2,	// NULL
	CST_CODE_UNDEF 				= 3,	// UNDEF
	CST_CODE_INTEGER 			= 4,	// INTEGER:			[intval]
	CST_CODE_WIDE_INTEGER 		= 5,	// WIDE_INTEGER:	[n x intval]
	CST_CODE_FLOAT 				= 6,	// FLOAT:			[fpval]
	CST_CODE_AGGREGATE 			= 7,	// AGGREGATE:		[n x value number]
	CST_CODE_STRING 			= 8,	// STRING:			[values]
	CST_CODE_CSTRING 			= 9,	// CSTRING:			[values]
	CST_CODE_CE_BINOP 			= 10,	// CE_BINOP:		[opcode, opval, opval]
	CST_CODE_CE_CAST 			= 11,	// CE_CAST:			[opcode, opty, opval]
	CST_CODE_CE_GEP 			= 12,	// CE_GEP:			[n x operands]
	CST_CODE_CE_SELECT 			= 13,	// CE_SELECT:		[opval, opval, opval]
	CST_CODE_CE_EXTRACTELT 		= 14,	// CE_EXTRACTELT:	[opty, opval, opval]
	CST_CODE_CE_INSERTELT 		= 15,	// CE_INSERTELT:	[opval, opval, opval]
	CST_CODE_CE_SHUFFLEVEC 		= 16,	// CE_SHUFFLEVEC:	[opval, opval, opval]
	CST_CODE_CE_CMP 			= 17,	// CE_CMP:			[opty, opval, opval, pred]
	CST_CODE_INLINEASM_OLD 		= 18,	// INLINEASM:		[sideeffect|alignstack, asmstr,conststr]
	CST_CODE_CE_SHUFVEC_EX 		= 19,	// SHUFVEC_EX:		[opty, opval, opval, opval]
	CST_CODE_CE_INBOUNDS_GEP 	= 20,	// INBOUNDS_GEP:	[n x operands]
	CST_CODE_BLOCKADDRESS 		= 21,	// CST_CODE_BLOCKADDRESS [fnty, fnval, bb#]
	CST_CODE_DATA 				= 22,	// DATA:			[n x elements]
	CST_CODE_INLINEASM 			= 23	// INLINEASM:		[sideeffect|alignstack|asmdialect,asmstr,conststr]
};

// CastOpcodes - encode which cast a CST_CODE_CE_CAST or a XXX refers to
enum CastOpcodes {
	CAST_TRUNC 					= 0,
	CAST_ZEXT 					= 1,
	CAST_SEXT 					= 2,
	CAST_FPTOUI 				= 3,
	CAST_FPTOSI 				= 4,
	CAST_UITOFP 				= 5,
	CAST_SITOFP 				= 6,
	CAST_FPTRUNC 				= 7,
	CAST_FPEXT 					= 8,
	CAST_PTRTOINT 				= 9,
	CAST_INTTOPTR 				= 10,
	CAST_BITCAST 				= 11,
	CAST_ADDRSPACECAST 			= 12
};

// BinaryOpcodes - encode which binop a CST_CODE_CE_BINOP or a XXX refers to
enum BinaryOpcodes {
	BINOP_ADD 					= 0,
	BINOP_SUB 					= 1,
	BINOP_MUL 					= 2,
	BINOP_UDIV 					= 3,
	BINOP_SDIV 					= 4,    // overloaded for FP
	BINOP_UREM 					= 5,
	BINOP_SREM 					= 6,    // overloaded for FP
	BINOP_SHL 					= 7,
	BINOP_LSHR 					= 8,
	BINOP_ASHR 					= 9,
	BINOP_AND 					= 10,
	BINOP_OR 					= 11,
	BINOP_XOR 					= 12
};

// Tencode AtomicRMW operations
enum RMWOperations {
	RMW_XCHG 					= 0,
	RMW_ADD 					= 1,
	RMW_SUB 					= 2,
	RMW_AND 					= 3,
	RMW_NAND 					= 4,
	RMW_OR 						= 5,
	RMW_XOR 					= 6,
	RMW_MAX 					= 7,
	RMW_MIN 					= 8,
	RMW_UMAX 					= 9,
	RMW_UMIN 					= 10
};

// OverflowingBinaryOperatorOptionalFlags - flags for serializing OverflowingBinaryOperator's SubclassOptionalData contents
enum OverflowingBinaryOperatorOptionalFlags {
	OBO_NO_UNSIGNED_WRAP 		= 0,
	OBO_NO_SIGNED_WRAP 			= 1
};

// PossiblyExactOperatorOptionalFlags - flags for serializing PossiblyExactOperator's SubclassOptionalData contents
enum PossiblyExactOperatorOptionalFlags {
	PEO_EXACT 					= 0
};

// Encoded AtomicOrdering values
enum AtomicOrderingCodes {
	ORDERING_NOTATOMIC 			= 0,
	ORDERING_UNORDERED 			= 1,
	ORDERING_MONOTONIC 			= 2,
	ORDERING_ACQUIRE 			= 3,
	ORDERING_RELEASE 			= 4,
	ORDERING_ACQREL 			= 5,
	ORDERING_SEQCST 			= 6
};

// Encoded SynchronizationScope values
enum AtomicSynchScopeCodes {
	SYNCHSCOPE_SINGLETHREAD 	= 0,
	SYNCHSCOPE_CROSSTHREAD 		= 1
};

// The function body block (FUNCTION_BLOCK_ID) describes function bodies.  It can contain a constant block (CONSTANTS_BLOCK_ID)
enum FunctionCodes {
	FUNC_CODE_DECLAREBLOCKS 	= 1, // DECLAREBLOCKS: [n]

	FUNC_CODE_INST_BINOP 		= 2, // BINOP:		[opcode, ty, opval, opval]
	FUNC_CODE_INST_CAST 		= 3, // CAST:		[opcode, ty, opty, opval]
	FUNC_CODE_INST_GEP 			= 4, // GEP:		[n x operands]
	FUNC_CODE_INST_SELECT 		= 5, // SELECT:		[ty, opval, opval, opval]
	FUNC_CODE_INST_EXTRACTELT 	= 6, // EXTRACTELT:	[opty, opval, opval]
	FUNC_CODE_INST_INSERTELT 	= 7, // INSERTELT:	[ty, opval, opval, opval]
	FUNC_CODE_INST_SHUFFLEVEC 	= 8, // SHUFFLEVEC:	[ty, opval, opval, opval]
	FUNC_CODE_INST_CMP 			= 9, // CMP:		[opty, opval, opval, pred]

	FUNC_CODE_INST_RET 			= 10, // RET:		[opty,opval<both optional>]
	FUNC_CODE_INST_BR 			= 11, // BR:		[bb#, bb#, cond] or [bb#]
	FUNC_CODE_INST_SWITCH 		= 12, // SWITCH:	[opty, op0, op1, ...]
	FUNC_CODE_INST_INVOKE 		= 13, // INVOKE:	[attr, fnty, op0,op1, ...]
	FUNC_CODE_INST_UNREACHABLE 	= 15, // UNREACHABLE

	FUNC_CODE_INST_PHI 			= 16, // PHI:		[ty, val0,bb0, ...]
	FUNC_CODE_INST_ALLOCA 		= 19, // ALLOCA:	[instty, op, align]
	FUNC_CODE_INST_LOAD 		= 20, // LOAD:		[opty, op, align, vol]
	FUNC_CODE_INST_VAARG 		= 23, // VAARG:		[valistty, valist, instty] This store code encodes the pointer type, rather than the value type this is so information only available in the pointer type (e.g. address spaces) is retained
	FUNC_CODE_INST_STORE 		= 24, // STORE:		[ptrty,ptr,val, align, vol]
	FUNC_CODE_INST_EXTRACTVAL 	= 26, // EXTRACTVAL:[n x operands]
	FUNC_CODE_INST_INSERTVAL 	= 27, // INSERTVAL:	[n x operands] fcmp/icmp returning Int1TY or vector of Int1Ty. Same as CMP, exists to support legacy vicmp/vfcmp instructions
	FUNC_CODE_INST_CMP2 		= 28, // CMP2:		[opty, opval, opval, pred] new select on i1 or [N x i1]
	FUNC_CODE_INST_VSELECT 		= 29, // VSELECT:	[ty,opval,opval,predty,pred]
	FUNC_CODE_INST_INBOUNDS_GEP	= 30, // INBOUNDS_GEP:	[n x operands]
	FUNC_CODE_INST_INDIRECTBR 	= 31, // INDIRECTBR:[opty, op0, op1, ...]
	FUNC_CODE_DEBUG_LOC_AGAIN 	= 33, // DEBUG_LOC_AGAIN

	FUNC_CODE_INST_CALL 		= 34, // CALL:		[attr, cc, fnty, fnid, args...]

	FUNC_CODE_DEBUG_LOC 		= 35, // DEBUG_LOC:	[Line,Col,ScopeVal, IAVal]
	FUNC_CODE_INST_FENCE 		= 36, // FENCE:		[ordering, synchscope]
	FUNC_CODE_INST_CMPXCHG 		= 37, // CMPXCHG:	[ptrty,ptr,cmp,new, align, vol, ordering, synchscope]
	FUNC_CODE_INST_ATOMICRMW 	= 38, // ATOMICRMW:	[ptrty,ptr,val, operation, align, vol, ordering, synchscope]
	FUNC_CODE_INST_RESUME 		= 39, // RESUME:	[opval]
	FUNC_CODE_INST_LANDINGPAD 	= 40, // LANDINGPAD:[ty,val,val,num,id0,val0...]
	FUNC_CODE_INST_LOADATOMIC 	= 41, // LOAD:		[opty, op, align, vol, ordering, synchscope]
	FUNC_CODE_INST_STOREATOMIC 	= 42  // STORE:		[ptrty,ptr,val, align, vol ordering, synchscope]
};

enum UseListCodes {
	USELIST_CODE_ENTRY 			= 1   // USELIST_CODE_ENTRY: TBD
};

enum AttributeKindCodes {
	// 							= 0 is unused
	ATTR_KIND_ALIGNMENT 		= 1,
	ATTR_KIND_ALWAYS_INLINE 	= 2,
	ATTR_KIND_BY_VAL 			= 3,
	ATTR_KIND_INLINE_HINT 		= 4,
	ATTR_KIND_IN_REG 			= 5,
	ATTR_KIND_MIN_SIZE 			= 6,
	ATTR_KIND_NAKED 			= 7,
	ATTR_KIND_NEST 				= 8,
	ATTR_KIND_NO_ALIAS 			= 9,
	ATTR_KIND_NO_BUILTIN 		= 10,
	ATTR_KIND_NO_CAPTURE 		= 11,
	ATTR_KIND_NO_DUPLICATE 		= 12,
	ATTR_KIND_NO_IMPLICIT_FLOAT	= 13,
	ATTR_KIND_NO_INLINE 		= 14,
	ATTR_KIND_NON_LAZY_BIND 	= 15,
	ATTR_KIND_NO_RED_ZONE 		= 16,
	ATTR_KIND_NO_RETURN 		= 17,
	ATTR_KIND_NO_UNWIND 		= 18,
	ATTR_KIND_OPTIMIZE_FOR_SIZE	= 19,
	ATTR_KIND_READ_NONE 		= 20,
	ATTR_KIND_READ_ONLY 		= 21,
	ATTR_KIND_RETURNED 			= 22,
	ATTR_KIND_RETURNS_TWICE 	= 23,
	ATTR_KIND_S_EXT 			= 24,
	ATTR_KIND_STACK_ALIGNMENT 	= 25,
	ATTR_KIND_STACK_PROTECT 	= 26,
	ATTR_KIND_STACK_PROTECT_REQ	= 27,
	ATTR_KIND_STACK_PROTECT_STRONG= 28,
	ATTR_KIND_STRUCT_RET 		= 29,
	ATTR_KIND_SANITIZE_ADDRESS 	= 30,
	ATTR_KIND_SANITIZE_THREAD 	= 31,
	ATTR_KIND_SANITIZE_MEMORY 	= 32,
	ATTR_KIND_UW_TABLE 			= 33,
	ATTR_KIND_Z_EXT 			= 34,
	ATTR_KIND_BUILTIN 			= 35,
	ATTR_KIND_COLD 				= 36,
	ATTR_KIND_OPTIMIZE_NONE 	= 37,
	ATTR_KIND_IN_ALLOCA 		= 38,
	ATTR_KIND_NON_NULL 			= 39,
	ATTR_KIND_JUMP_TABLE 		= 40
};

} // End bitc namespace

// BitCodeAbbrevOp
// describes one or more operands in an abbreviation - one of:
// 1. a literal integer value ("the operand is always 17").
// 2. an encoding specification ("this operand encoded like so").
class BitCodeAbbrevOp {
	uint64		Val;			// A literal value or data for an encoding.
	bool		IsLiteral : 1;	// Indicate whether this is a literal value or not.
	unsigned	Enc : 3;		// The encoding to use.
public:
	enum Encoding {
		Fixed	= 1,	// A fixed width field, Val specifies number of bits.
		VBR		= 2,	// A VBR field where Val specifies the width of each chunk.
		Array	= 3,	// A sequence of fields, next field species elt encoding.
		Char6	= 4,	// A 6-bit fixed field which maps to [a-zA-Z0-9._].
		Blob	= 5		// 32-bit aligned array of 8-bit characters.
	};

	explicit BitCodeAbbrevOp(uint64 V) : Val(V), IsLiteral(true) {}
	explicit BitCodeAbbrevOp(Encoding E, uint64 Data = 0) : Val(Data), IsLiteral(false), Enc(E) {}

	bool		isLiteral()			const { return IsLiteral; }
	bool		isEncoding()		const { return !IsLiteral; }
	uint64		getLiteralValue()	const { assert(isLiteral()); return Val; }
	Encoding	getEncoding()		const { assert(isEncoding()); return (Encoding)Enc; }
	uint64		getEncodingData()	const { assert(isEncoding() && hasEncodingData()); return Val; }
	bool		hasEncodingData()	const { return hasEncodingData(getEncoding()); }

	static bool hasEncodingData(Encoding E) {
		return E == Fixed || E == VBR;
	}

	// isChar6 - Return true if this character is legal in the Char6 encoding.
	static bool isChar6(char C) {
		return (C >= 'a' && C <= 'z')
			|| (C >= 'A' && C <= 'Z')
			|| (C >= '0' && C <= '9')
			|| (C == '.' || C == '_');
	}
	static unsigned EncodeChar6(char C) {
		return	C >= 'a' && C <= 'z'	? C - 'a'
			:	C >= 'A' && C <= 'Z'	? C - 'A' + 26
			:	C >= '0' && C <= '9'	? C - '0' + 26 + 26
			:	C == '.'				? 62
			:	C == '_'				? 63
			:	-1;
	}

	static char DecodeChar6(unsigned V) {
		return	V < 26					? V + 'a'
			:	V < 26 + 26				? V - 26 + 'A'
			:	V < 26 + 26 + 10		? V - 26 - 26 + '0'
			:	V == 62					? '.'
			:	V == 63					? '_'
			:	-1;
	}
};

// BitCodeAbbrev - An abbreviation allows a complex record that has redundancy to be stored in a specialized format instead of the fully-general, fully-vbr, format
class BitCodeAbbrev : public refs<BitCodeAbbrev>, public dynamic_array<BitCodeAbbrevOp> {};

// BitstreamReader
// used to read from an LLVM bitcode stream, maintaining information that is global to decoding the entire file
// While a file is being read multiple cursors can be independently advanced or skipped around within the file; these are represented by the/ BitstreamCursor class

class BitstreamReader {
public:
	struct BlockInfo {
		unsigned								id;
		dynamic_array<ref_ptr<BitCodeAbbrev>>	abbrevs;
		string									name;
		dynamic_array<pair<unsigned, string> >	record_names;
		BlockInfo(unsigned id) : id(id) {}
	};

	istream_ref						file;
	dynamic_array<BlockInfo>	records;
	bool						ignore_names;

public:
	BitstreamReader(istream_ref file) : file(file) {}

	void CollectBlockInfoNames()				{ ignore_names = false; }
	bool isIgnoringBlockInfoNames()				{ return ignore_names; }
	bool hasBlockInfoRecords()		const		{ return !records.empty(); }

	const BlockInfo *getBlockInfo(unsigned id) const {
		if (!records.empty() && records.back().id == id)
			return &records.back();

		for (auto &i : records)
			if (i.id == id)
				return &i;

		return nullptr;
	}

	BlockInfo &getOrCreateBlockInfo(unsigned id) {
		if (const BlockInfo *BI = getBlockInfo(id))
			return *unconst(BI);
		return records.push_back(id);
	}
};


// BitstreamEntry

struct BitstreamEntry {
	enum KIND {
		Error,		// Malformed bitcode was found.
		EndBlock,	// We've reached the end of the current block, or the end of the file, which is treated like a series of EndBlock records
		SubBlock,	// This is the start of a new subblock of a specific id.
		Record		// This is a record with a specific AbbrevID
	} kind;

	unsigned id;

	BitstreamEntry(KIND kind) : kind(kind) {}
	BitstreamEntry(KIND kind, unsigned id) : kind(kind), id(id) {}

	static BitstreamEntry getError() 					{ return Error; }
	static BitstreamEntry getEndBlock() 				{ return EndBlock; }
	static BitstreamEntry getSubBlock(unsigned id) 		{ return BitstreamEntry(SubBlock, id);	}
	static BitstreamEntry getRecord(unsigned AbbrevID)	{ return BitstreamEntry(Record, AbbrevID);	}
};

// BitstreamCursor A position within a bitcode file
// There may be multiple independent cursors reading within one bitstream, each maintaining their own local state

class BitstreamCursor : vlc_in<uint32, false, istream_ref> {
	friend class Deserializer;
	typedef uint32 word_t;

	BitstreamReader		*BitStream;
	size_t				NextChar;
	unsigned			CurCodeSize;

	dynamic_array<ref_ptr<BitCodeAbbrev>> CurAbbrevs;

	struct Block {
		unsigned PrevCodeSize;
		dynamic_array<ref_ptr<BitCodeAbbrev>> PrevAbbrevs;
		explicit Block(unsigned PCS) : PrevCodeSize(PCS) {}
	};

	dynamic_array<Block> BlockScope;

	void SkipToFourByteBoundary() {
		reset();
	}

	void popBlockScope() {
		CurCodeSize = BlockScope.back().PrevCodeSize;
		CurAbbrevs = BlockScope.pop_back_value().PrevAbbrevs;
	}

	void	readAbbreviatedLiteral(const BitCodeAbbrevOp &Op, dynamic_array<uint64> &Vals);
	uint64	readAbbreviatedField(const BitCodeAbbrevOp &Op);
	void	skipAbbreviatedField(const BitCodeAbbrevOp &Op);

public:
	BitstreamCursor(BitstreamReader &R)	: vlc_in<uint32, false, istream_ref>(R.file), BitStream(&R), NextChar(0), CurCodeSize(2) {}

	bool	isEndPos(size_t pos)		{ return pos > file.length(); }
	bool	canSkipToPos(size_t pos)	{ return pos < file.length(); }
	uint32	getWord(size_t pos)			{ file.seek(pos); return file.get<uint32>(); }
	bool	AtEndOfStream()				{ return bits_left == 0 && isEndPos(NextChar); }

	// getAbbrevIDWidth - Return the number of bits used to encode an abbrev #.
	unsigned getAbbrevIDWidth() const { return CurCodeSize; }

	// GetCurrentBitNo - Return the bit # of the bit we are reading.
	uint64 GetCurrentBitNo() const { return NextChar * 8 - bits_left; }

	enum {
		AF_DontPopBlockAtEnd		= 1,	// the advance() method does not automatically pop the block scope when the end of a block is reached
		AF_DontAutoprocessAbbrevs	= 2		// abbrev entries are returned just like normal records.
	};

	// advance - Advance the current bitstream, returning the next entry in the stream
	BitstreamEntry advance(unsigned Flags = 0) {
		while (1) {
			unsigned Code = ReadCode();
			if (Code == bitc::END_BLOCK) {
				// Pop the end of the block unless Flags tells us not to
				if (!(Flags & AF_DontPopBlockAtEnd) && ReadBlockEnd())
					return BitstreamEntry::getError();
				return BitstreamEntry::getEndBlock();
			}

			if (Code == bitc::ENTER_SUBBLOCK)
				return BitstreamEntry::getSubBlock(ReadSubBlockID());

			if (Code == bitc::DEFINE_ABBREV && !(Flags & AF_DontAutoprocessAbbrevs)) {
				ReadAbbrevRecord();
				continue;
			}

			return BitstreamEntry::getRecord(Code);
		}
	}

	// convenience function for clients that don't expect any subblocks
	BitstreamEntry advanceSkippingSubblocks(unsigned Flags = 0) {
		while (1) {
			// If we found a normal entry, return it.
			BitstreamEntry Entry = advance(Flags);
			if (Entry.kind != BitstreamEntry::SubBlock)
				return Entry;

			// If we found a sub-block, just skip over it and check the next entry.
			if (SkipBlock())
				return BitstreamEntry::getError();
		}
	}

	// reset the stream to the specified bit number
	void JumpToBit(uint64 BitNo)	{ seek_bit(BitNo); }
	uint32 Read(unsigned NumBits)	{ return get(NumBits); }

	uint64 Read64(unsigned NumBits) {
		if (NumBits <= 32)
			return Read(NumBits);

		uint64 V = Read(32);
		return V | (uint64)Read(NumBits - 32) << 32;
	}

	uint32 ReadVBR(unsigned NumBits) {
		uint32 Piece = Read(NumBits);
		if ((Piece & (1U << (NumBits - 1))) == 0)
			return Piece;

		uint32 Result = 0;
		unsigned NextBit = 0;
		while (1) {
			Result |= (Piece & ((1U << (NumBits - 1)) - 1)) << NextBit;
			if ((Piece & (1U << (NumBits - 1))) == 0)
				return Result;
			NextBit += NumBits - 1;
			Piece = Read(NumBits);
		}
	}

	// ReadVBR64 - Read a VBR that may have a value up to 64-bits in size.  The chunk size of the VBR must still be <= 32 bits though.
	uint64 ReadVBR64(unsigned NumBits) {
		uint32 Piece = Read(NumBits);
		if ((Piece & (1U << (NumBits - 1))) == 0)
			return uint64(Piece);

		uint64 Result = 0;
		unsigned NextBit = 0;
		while (1) {
			Result |= uint64(Piece & ((1U << (NumBits - 1)) - 1)) << NextBit;
			if ((Piece & (1U << (NumBits - 1))) == 0)
				return Result;
			NextBit += NumBits - 1;
			Piece = Read(NumBits);
		}
	}
	unsigned ReadCode()			{ return Read(CurCodeSize); }
	unsigned ReadSubBlockID()	{ return ReadVBR(bitc::BlockIDWidth); }

	// SkipBlock - Having read the ENTER_SUBBLOCK abbrevid and a id, skip over the body of this block.  If the block record is malformed, return true.
	bool SkipBlock() {
		// Read and ignore the codelen value.  Since we are skipping this block, we don't care what code widths are used inside of it.
		ReadVBR(bitc::CodeLenWidth);
		SkipToFourByteBoundary();
		unsigned NumFourBytes = Read(bitc::BlockSizeWidth);

		// Check that the block wasn't partially defined, and that the offset isn't bogus.
		size_t SkipTo = GetCurrentBitNo() + NumFourBytes * 4 * 8;
		if (AtEndOfStream() || !canSkipToPos(SkipTo / 8))
			return true;

		JumpToBit(SkipTo);
		return false;
	}

	// EnterSubBlock - Having read the ENTER_SUBBLOCK abbrevid, enter the block, and return true if the block has an error.
	bool EnterSubBlock(unsigned id, unsigned *NumWordsP = nullptr);

	bool ReadBlockEnd() {
		if (BlockScope.empty())
			return true;

		SkipToFourByteBoundary();
		popBlockScope();
		return false;
	}

	// getAbbrev - Return the abbreviation for the specified AbbrevId.
	const BitCodeAbbrev *getAbbrev(unsigned AbbrevID) {
		return CurAbbrevs[AbbrevID - bitc::FIRST_APPLICATION_ABBREV];
	}

	// skipRecord - Read the current record and discard it.
	void skipRecord(unsigned AbbrevID);

	unsigned readRecord(unsigned AbbrevID, dynamic_array<uint64> &Vals, string *Blob = nullptr);

	void ReadAbbrevRecord();
	bool ReadBlockInfoBlock();
};


// EnterSubBlock - Having read the ENTER_SUBBLOCK abbrevid, enter the block, and return true if the block has an error.
bool BitstreamCursor::EnterSubBlock(unsigned id, unsigned *NumWordsP) {
	// Save the current block's state on BlockScope
	BlockScope.push_back(Block(CurCodeSize));
	swap(BlockScope.back().PrevAbbrevs, CurAbbrevs);

	// Add the abbrevs specific to this block to the CurAbbrevs list.
	if (const BitstreamReader::BlockInfo *Info = BitStream->getBlockInfo(id))
		CurAbbrevs.insert(CurAbbrevs.end(), Info->abbrevs.begin(), Info->abbrevs.end());

	// Get the codesize of this block.
	CurCodeSize = ReadVBR(bitc::CodeLenWidth);

	SkipToFourByteBoundary();
	unsigned NumWords = Read(bitc::BlockSizeWidth);
	if (NumWordsP)
		*NumWordsP = NumWords;

	// Validate that this block is sane.
	return CurCodeSize == 0 || AtEndOfStream();
}

uint64 BitstreamCursor::readAbbreviatedField(const BitCodeAbbrevOp &Op) {
	assert(!Op.isLiteral() && "Not to be used with literals!");

	// Decode the value as we are commanded.
	switch (Op.getEncoding()) {
		case BitCodeAbbrevOp::Array:
		case BitCodeAbbrevOp::Blob:
			llvm_unreachable("Should not reach here");
		case BitCodeAbbrevOp::Fixed:
			return Read((unsigned)Op.getEncodingData());
		case BitCodeAbbrevOp::VBR:
			return ReadVBR64((unsigned)Op.getEncodingData());
		case BitCodeAbbrevOp::Char6:
			return BitCodeAbbrevOp::DecodeChar6(Read(6));
	}
	llvm_unreachable("invalid abbreviation encoding");
}

void BitstreamCursor::skipAbbreviatedField(const BitCodeAbbrevOp &Op) {
	assert(!Op.isLiteral() && "Not to be used with literals!");

	// Decode the value as we are commanded.
	switch (Op.getEncoding()) {
		case BitCodeAbbrevOp::Array:
		case BitCodeAbbrevOp::Blob:
			llvm_unreachable("Should not reach here");
		case BitCodeAbbrevOp::Fixed:
			Read((unsigned)Op.getEncodingData());
			break;
		case BitCodeAbbrevOp::VBR:
			ReadVBR64((unsigned)Op.getEncodingData());
			break;
		case BitCodeAbbrevOp::Char6:
			Read(6);
			break;
	}
}

// skipRecord - Read the current record and discard it.
void BitstreamCursor::skipRecord(unsigned AbbrevID) {
	// Skip unabbreviated records by reading past their entries.
	if (AbbrevID == bitc::UNABBREV_RECORD) {
		unsigned Code = ReadVBR(6);
		(void)Code;
		unsigned NumElts = ReadVBR(6);
		for (unsigned i = 0; i != NumElts; ++i)
			(void)ReadVBR64(6);
		return;
	}

	const BitCodeAbbrev *Abbv = getAbbrev(AbbrevID);

	for (auto i = Abbv->begin(), e = Abbv->end(); i != e; ++i) {
		const BitCodeAbbrevOp &Op = *i;
		if (Op.isLiteral())
			continue;

		if (Op.getEncoding() != BitCodeAbbrevOp::Array && Op.getEncoding() != BitCodeAbbrevOp::Blob) {
			skipAbbreviatedField(Op);
			continue;
		}

		if (Op.getEncoding() == BitCodeAbbrevOp::Array) {
			// Array case.  Read the number of elements as a vbr6.
			unsigned NumElts = ReadVBR(6);

			// Get the element encoding.
			assert(i + 2 == e && "array op not second to last?");
			const BitCodeAbbrevOp &EltEnc = *++i;

			// Read all the elements.
			for (; NumElts; --NumElts)
				skipAbbreviatedField(EltEnc);
			continue;
		}

		assert(Op.getEncoding() == BitCodeAbbrevOp::Blob);
		// Blob case.  Read the number of bytes as a vbr6.
		unsigned NumElts = ReadVBR(6);
		SkipToFourByteBoundary();  // 32-bit alignment

		// Figure out where the end of this blob will be including tail padding.
		size_t NewEnd = GetCurrentBitNo() + ((NumElts + 3)&~3) * 8;

		// If this would read off the end of the bitcode file, just set the record to empty and return.
		if (!canSkipToPos(NewEnd / 8)) {
			NextChar = file.length();
			break;
		}

		// Skip over the blob.
		JumpToBit(NewEnd);
	}
}

unsigned BitstreamCursor::readRecord(unsigned AbbrevID, dynamic_array<uint64> &Vals, string *Blob) {
	if (AbbrevID == bitc::UNABBREV_RECORD) {
		unsigned Code = ReadVBR(6);
		unsigned NumElts = ReadVBR(6);
		for (unsigned i = 0; i != NumElts; ++i)
			Vals.push_back(ReadVBR64(6));
		return Code;
	}

	const BitCodeAbbrev *Abbv = getAbbrev(AbbrevID);

	// Read the record code first.
	const BitCodeAbbrevOp &CodeOp = Abbv->front();
	unsigned Code;
	if (CodeOp.isLiteral())
		Code = CodeOp.getLiteralValue();
	else {
		if (CodeOp.getEncoding() == BitCodeAbbrevOp::Array || CodeOp.getEncoding() == BitCodeAbbrevOp::Blob)
			report_fatal_error("Abbreviation starts with an Array or a Blob");
		Code = readAbbreviatedField(CodeOp);
	}

	for (auto i = Abbv->begin(), e = Abbv->end(); i != e; ++i) {
		const BitCodeAbbrevOp &Op = *i;
		if (Op.isLiteral()) {
			Vals.push_back(Op.getLiteralValue());
			continue;
		}

		if (Op.getEncoding() != BitCodeAbbrevOp::Array && Op.getEncoding() != BitCodeAbbrevOp::Blob) {
			Vals.push_back(readAbbreviatedField(Op));
			continue;
		}

		if (Op.getEncoding() == BitCodeAbbrevOp::Array) {
			// Array case.  Read the number of elements as a vbr6.
			unsigned NumElts = ReadVBR(6);

			// Get the element encoding.
			if (i + 2 != e)
				report_fatal_error("Array op not second to last");
			const BitCodeAbbrevOp &EltEnc = *++i;
			if (!EltEnc.isEncoding())
				report_fatal_error("Array element type has to be an encoding of a type");
			if (EltEnc.getEncoding() == BitCodeAbbrevOp::Array || EltEnc.getEncoding() == BitCodeAbbrevOp::Blob)
				report_fatal_error("Array element type can't be an Array or a Blob");

			// Read all the elements.
			for (; NumElts; --NumElts)
				Vals.push_back(readAbbreviatedField(EltEnc));
			continue;
		}

		assert(Op.getEncoding() == BitCodeAbbrevOp::Blob);
		// Blob case.  Read the number of bytes as a vbr6.
		unsigned NumElts = ReadVBR(6);
		SkipToFourByteBoundary();  // 32-bit alignment

		// Figure out where the end of this blob will be including tail padding.
		size_t CurBitPos = GetCurrentBitNo();
		size_t NewEnd = CurBitPos + ((NumElts + 3)&~3) * 8;

		// If this would read off the end of the bitcode file, just set the record to empty and return.
		if (!canSkipToPos(NewEnd / 8)) {
			Vals.append(NumElts, 0);
			NextChar = file.length();
			break;
		}

		// Otherwise, inform the streamer that we need these bytes in memory.
//		const char *Ptr = (const char*)BitStream->getBitcodeBytes().getPointer(CurBitPos / 8, NumElts);
		file.seek(CurBitPos / 8);

		// If we can return a reference to the data, do so to avoid copying it.
		if (Blob) {
			Blob->read(file, NumElts);
		} else {
			// Otherwise, unpack into Vals with zero extension.
			for (; NumElts; --NumElts)
				Vals.push_back(file.getc());
		}
		// Skip over tail padding.
		JumpToBit(NewEnd);
	}

	return Code;
}


void BitstreamCursor::ReadAbbrevRecord() {
	BitCodeAbbrev *Abbv = new BitCodeAbbrev();
	unsigned NumOpInfo = ReadVBR(5);
	for (unsigned i = 0; i != NumOpInfo; ++i) {
		bool IsLiteral = Read(1);
		if (IsLiteral) {
			Abbv->push_back(BitCodeAbbrevOp(ReadVBR64(8)));
			continue;
		}

		BitCodeAbbrevOp::Encoding E = (BitCodeAbbrevOp::Encoding)Read(3);
		if (BitCodeAbbrevOp::hasEncodingData(E)) {
			uint64 Data = ReadVBR64(5);

			// As a special case, handle fixed(0) (i.e., a fixed field with zero bits) and vbr(0) as a literal zero.  This is decoded the same way, and avoids a slow path in Read() to have to handle reading zero bits.
			if ((E == BitCodeAbbrevOp::Fixed || E == BitCodeAbbrevOp::VBR) && Data == 0) {
				Abbv->push_back(BitCodeAbbrevOp(0));
				continue;
			}

			Abbv->push_back(BitCodeAbbrevOp(E, Data));
		} else
			Abbv->push_back(BitCodeAbbrevOp(E));
	}

	if (Abbv->empty())
		report_fatal_error("Abbrev record with no operands");
	CurAbbrevs.push_back(Abbv);
}

bool BitstreamCursor::ReadBlockInfoBlock() {
	// If this is the second stream to get to the block info block, skip it.
	if (BitStream->hasBlockInfoRecords())
		return SkipBlock();

	if (EnterSubBlock(bitc::BLOCKINFO_BLOCK_ID))
		return true;

	dynamic_array<uint64>		Record;
	BitstreamReader::BlockInfo	*CurBlockInfo = nullptr;

	// Read all the records for this module.
	while (1) {
		BitstreamEntry Entry = advanceSkippingSubblocks(AF_DontAutoprocessAbbrevs);

		switch (Entry.kind) {
			case BitstreamEntry::SubBlock: // Handled for us already.
			case BitstreamEntry::Error:
				return true;
			case BitstreamEntry::EndBlock:
				return false;
			case BitstreamEntry::Record:
				// The interesting case.
				break;
		}

		// Read abbrev records, associate them with CurBID.
		if (Entry.id == bitc::DEFINE_ABBREV) {
			if (!CurBlockInfo)
				return true;
			ReadAbbrevRecord();

			// ReadAbbrevRecord installs the abbrev in CurAbbrevs.  Move it to the appropriate BlockInfo.
			CurBlockInfo->abbrevs.push_back(move(CurAbbrevs.back()));
			CurAbbrevs.pop_back();
			continue;
		}

		// Read a record.
		Record.clear();
		switch (readRecord(Entry.id, Record)) {
			default:
				break;  // Default behavior, ignore unknown content

			case bitc::BLOCKINFO_CODE_SETBID:
				if (Record.size() < 1) return true;
				CurBlockInfo = &BitStream->getOrCreateBlockInfo((unsigned)Record[0]);
				break;

			case bitc::BLOCKINFO_CODE_BLOCKNAME: {
				if (!CurBlockInfo)
					return true;
				if (BitStream->isIgnoringBlockInfoNames())
					break;  // Ignore name
				string name;
				for (unsigned i = 0, e = Record.size32(); i != e; ++i)
					name += (char)Record[i];
				CurBlockInfo->name = name;
				break;
			}
			case bitc::BLOCKINFO_CODE_SETRECORDNAME: {
				if (!CurBlockInfo)
					return true;
				if (BitStream->isIgnoringBlockInfoNames())
					break;  // Ignore name
				string name;
				for (unsigned i = 1, e = Record.size32(); i != e; ++i)
					name += (char)Record[i];
				CurBlockInfo->record_names.push_back(make_pair((unsigned)Record[0], name));
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Filehandler
//-----------------------------------------------------------------------------

#include "iso/iso_files.h"

class BitcodeFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Bitcode"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		uint32	magic = file.get<uint32>();

		ISO_ptr<anything> p(id);

		BitstreamReader	reader(file);
		BitstreamCursor	cursor(reader);

		for (;;) {
			auto	entry = cursor.advance();
			if (entry.kind == BitstreamEntry::EndBlock)
				break;
		}


		return p;
	}
} bitcode;
