#ifndef BITCODE_H
#define BITCODE_H

#include "base/defs.h"
#include "base/array.h"
#include "base/sparse_array.h"
#include "base/hash.h"
#include "base/strings.h"
#include "allocators/allocator.h"
#include "codec/vlc.h"
#include "dwarf.h"

//-----------------------------------------------------------------------------
//	LLVM bitcode
//-----------------------------------------------------------------------------

namespace bitcode {
using namespace iso;

enum AbbrevID {
	END_BLOCK 					= 0,
	ENTER_SUBBLOCK 				= 1,
	DEFINE_ABBREV 				= 2,	// defines an abbrev for the current block
	UNABBREV_RECORD 			= 3,	// emitted with a vbr6 for the record code, followed by a vbr6 for the # operands, followed by vbr6's for each operand
	FIRST_APPLICATION_ABBREV	= 4		// marker for the first abbrev assignment
};

enum BlockID {
	BLOCKINFO_BLOCK_ID			= 0,	// defines metadata about blocks
	// Block IDs 1-7 are reserved for future expansion
	FIRST_APPLICATION_BLOCKID	= 8,

	MODULE_BLOCK_ID				= FIRST_APPLICATION_BLOCKID,
	
	// Module sub-block ids
	PARAMATTR_BLOCK_ID,
	PARAMATTR_GROUP_BLOCK_ID,
	CONSTANTS_BLOCK_ID,
	FUNCTION_BLOCK_ID,
	IDENTIFICATION_BLOCK_ID,	// information on the bitcode versioning
	VALUE_SYMTAB_BLOCK_ID,
	METADATA_BLOCK_ID,
	METADATA_ATTACHMENT_ID,
	TYPE_BLOCK_ID_NEW,
	USELIST_BLOCK_ID,
	MODULE_STRTAB_BLOCK_ID,
	GLOBALVAL_SUMMARY_BLOCK_ID,
	OPERAND_BUNDLE_TAGS_BLOCK_ID,
	METADATA_KIND_BLOCK_ID,
	STRTAB_BLOCK_ID,
	FULL_LTO_GLOBALVAL_SUMMARY_BLOCK_ID,
	SYMTAB_BLOCK_ID,
	SYNC_SCOPE_NAMES_BLOCK_ID,
};

struct vlc : vlc_in<uint32, false, reader_intf> {
	typedef vlc_in<uint32, false, reader_intf> B;
	using B::B;

	template<typename T> uint32 get_vbr(uint32 n) {
		uint32 piece = get(n);
		if ((piece & bit(n - 1)) == 0)
			return T(piece);

		T result = 0;
		for (uint32 b = 0; ; b += n - 1) {
			result |= T(piece & bits(n - 1)) << b;
			if ((piece & bit(n - 1)) == 0)
				return result;
			piece = get(n);
		}
	}

	bool	isEndPos(size_t pos)		{ return pos >= file.length(); }
	bool	canSkipToPos(size_t pos)	{ return pos <= file.length(); }
	bool	AtEndOfStream()				{ return bits_left == 0 && isEndPos(file.tell()); }
	void	SkipToFourByteBoundary()	{ reset(); file.align(4); }
};

struct wvlc : vlc_out<uint32, false, writer_intf> {
	typedef vlc_out<uint32, false, writer_intf> B;
	using B::B;

	template<typename T> void put_vbr(T v, uint32 n) {
		uint32 threshold = bit(n - 1);
		while (v >= threshold) {
			put(((uint32)v & bits(n - 1)) | threshold, n);
			v >>= n - 1;
		}
		put((uint32)v, n);
	}

	void	SkipToFourByteBoundary()	{ flush(); file.align(4); }
};

inline int64 decode_svbr(uint64 v) {
	return v & 1 ? -int64(v >> 1) : int64(v >> 1);
}

inline uint64 encode_svbr(int64 v) {
	return v >= 0 ? v << 1 : (-v << 1) | 1;
}

//-------------------------------------
// Abbrev
//-------------------------------------

struct AbbrevOp {
	enum Encoding : uint8 {
		Literal		= 0,
		Fixed		= 1,	// A fixed width field, v specifies number of bits
		VBR			= 2,	// A VBR field where v specifies the width of each chunk
		_Array		= 3,	// A sequence of fields, next field specifies elt encoding
		Char6		= 4,	// A 6-bit fixed field which maps to [a-zA-Z0-9._]
		Blob		= 5,	// 32-bit aligned array of 8-bit characters

		ArrayFlag	= 8,
		Array_Fixed	= Fixed	| ArrayFlag,
		Array_VBR	= VBR	| ArrayFlag,
		Array_Char6	= Char6	| ArrayFlag,

	};
	Encoding	encoding;
	uint64		v;

	static bool isChar6(char c) {
		return between(c, 'a', 'z') || between(c, 'A', 'Z') || between(c, '0', '9') || (c == '.' || c == '_');
	}
	static uint32 EncodeChar6(char c) {
		return	between(c, 'a', 'z')	? c - 'a'
			:	between(c, 'A', 'Z')	? c - 'A' + 26
			:	between(c, '0', '9')	? c - '0' + 26 + 26
			:	c == '.'				? 62
			:	c == '_'				? 63
			:	-1;
	}
	static char DecodeChar6(uint32 v) {
		return	v < 26					? v + 'a'
			:	v < 26 + 26				? v - 26 + 'A'
			:	v < 26 + 26 + 10		? v - 26 - 26 + '0'
			:	v == 62					? '.'
			:	v == 63					? '_'
			:	0;
	}
	
	AbbrevOp() {}
	AbbrevOp(Encoding encoding, uint64 v = 0)	: encoding(encoding), v(v) {}
	AbbrevOp(uint64 v)							: encoding(Literal), v(v) {}

	bool	read(vlc& file) {
		if (file.get(1)) {
			encoding = Literal;
			v = file.get_vbr<uint64>(8);

		} else {
			encoding = (AbbrevOp::Encoding)file.get(3);
			if (encoding == Fixed || encoding == VBR) {
				v = file.get_vbr<uint64>(5);
				if (v == 0)
					encoding = Literal;	// handle fixed(0) and vbr(0) as a literal zero
			}
		}
		return true;
	}

	uint64	get_value(vlc& file) const {
		switch (encoding) {
			case Literal:
				return v;
			case Fixed: case Array_Fixed:
				return file.get((uint32)v);
			case VBR:	case Array_VBR:
				return file.get_vbr<uint64>((uint32)v);
			case Char6:	case Array_Char6:
				return DecodeChar6(file.get(6));
			default:
				ISO_ASSERT(0);
				return 0;
		}
	}

	void	skip(vlc& file) const {
		switch (encoding) {
			case Literal:
				break;
			case Fixed:
				file.seek_cur_bit(v);
				break;
			case VBR:
				file.get_vbr<uint64>((uint32)v);
				break;
			case Char6:
				file.seek_cur_bit(6);
				break;
			case Array_Fixed:
				file.seek_cur_bit(file.get_vbr<uint32>(6) * v);
				break;
			case Array_VBR:
				for (uint32 n = file.get_vbr<uint32>(6); n--;)
					file.get_vbr<uint64>((uint32)v);
				return;
			case Array_Char6:
				file.seek_cur_bit(file.get_vbr<uint32>(6) * 6);
				break;
			case Blob: {
				uint32 n = file.get_vbr<uint32>(6);
				file.SkipToFourByteBoundary();
				file.get_stream().seek_cur((n + 3) & ~3);
				break;
			}
			default:
				ISO_ASSERT(0);
		}
	}

	bool	write(wvlc& file) const {
		file.put_bit(encoding == Literal);
		if (encoding == Literal) {
			file.put_vbr(v, 8);
		} else {
			if (encoding & ArrayFlag) {
				file.put(_Array, 3);
				file.put_bit(false);
			}
			auto	e1 = encoding & ~ArrayFlag;
			file.put(e1, 3);
			if (e1 == Fixed || e1 == VBR)
				file.put_vbr(v, 5);
		}
		return true;
	}

	void	put_value(wvlc& file, uint64 x) const {
		switch (encoding) {
			case Literal:
				ISO_ASSERT(x == v); break;
			case Fixed: case Array_Fixed:
				file.put(x, (uint32)v); break;
			case VBR:	case Array_VBR:
				file.put_vbr(x, (uint32)v); break;
			case Char6:	case Array_Char6:
				file.put(EncodeChar6(x), 6); break;
			default:
				ISO_ASSERT(0);
		}
	}

};

class Abbrev : public refs<Abbrev>, public dynamic_array<AbbrevOp> {
public:
	Abbrev(initializer_list<AbbrevOp> v) : dynamic_array<AbbrevOp>(v) {}
	Abbrev(vlc& file) {
		read(file, file.get_vbr<uint32>(5));
		if (size() > 1) {
			auto	p = end() - 2;
			if (p->encoding == AbbrevOp::_Array) {
				p->encoding	= AbbrevOp::Encoding(p[1].encoding | AbbrevOp::ArrayFlag);
				p->v	= p[1].v;
				pop_back();
			}
		}
	}

	bool	write(wvlc& file) const {
		file.put_vbr(size() + !!(back().encoding & AbbrevOp::ArrayFlag), 5);
		for (auto &i : *this)
			i.write(file);
		return true;
	}

	uint32	get_code(vlc &file)					const { return front().get_value(file); }
	bool	read_record(vlc &file, dynamic_array<uint64> &vals, malloc_block *blob) const;

	void	put_code(wvlc &file, uint32 code)	const { return front().put_value(file, code); }
	bool	write_record(wvlc &file, range<const uint64*> vals, const malloc_block *blob) const;
	
	bool	skip(vlc &file) const;
};

struct Abbrevs {
	dynamic_array<ref_ptr<Abbrev>>		abbrevs;

	const Abbrev*	get_abbrev(AbbrevID id) const {
		return abbrevs[id - FIRST_APPLICATION_ABBREV];
	}
	AbbrevID		add_abbrev(const Abbrev &a) {
		abbrevs.push_back(dup(a));
		return AbbrevID(abbrevs.size() + (FIRST_APPLICATION_ABBREV - 1));
	}

	Abbrevs() {}
	Abbrevs(const Abbrevs *a) {
		if (a)
			abbrevs = a->abbrevs;
	}
};

//-------------------------------------
// Blocks
//-------------------------------------

class BlockReader : Abbrevs {
	vlc			&file;
	uint32		code_size;
	streamptr	end;

public:
	BlockReader(vlc &file);
	BlockReader(vlc &file, const Abbrevs *info);
	~BlockReader();
	operator vlc&() const { return file; }

	bool		verify()		const { return code_size != 0 && !file.AtEndOfStream(); }
	AbbrevID	nextRecord(uint32 &code, bool process_abbrev = true);
	AbbrevID	nextRecordNoSub(uint32 &code);
	bool		skipRecord(AbbrevID id)	const;
	bool		readRecord(AbbrevID id, dynamic_array<uint64> &v, malloc_block *Blob = nullptr) const;

	BlockReader	enterBlock(const Abbrevs *info) const { return {file, info}; }
};

class BlockWriter : Abbrevs {
	wvlc		&file;
	uint32		code_size;
	streamptr	start;

public:
	BlockWriter(wvlc &file) : file(file), start(0) {}
	BlockWriter(wvlc &file, uint32 code_size, const Abbrevs *info = nullptr);
	~BlockWriter();
	operator wvlc&() const { return file; }

	void	put_code(uint32 code) const {
		file.put(code, code_size);
	}
	auto	add_abbrev(const Abbrev &a) {
		put_code(DEFINE_ABBREV);
		a.write(file);
		return Abbrevs::add_abbrev(a);
	}

	void	write_record(uint32 code, range<const uint64*> v, AbbrevID abbrev = AbbrevID(0)) const {
		if (abbrev) {
			auto	a = get_abbrev(abbrev);
			put_code(abbrev);
			a->put_code(file, code);
			a->write_record(file, v, nullptr);
		} else  {
			put_code(UNABBREV_RECORD);
			file.put_vbr(code, 6);
			file.put_vbr(v.size32(), 6);
			for (auto &i : v)
				file.put_vbr(i, 6);
		}
	}
	void	write_record(uint32 code, initializer_list<uint64> v, AbbrevID abbrev = AbbrevID(0)) const {
		write_record(code, make_range(v), abbrev);
	}

	void	write_string(uint32 code, const string &s, AbbrevID char6_abbrev = AbbrevID(0)) const;

	BlockWriter	enterBlock(BlockID id, uint32 code_size, const Abbrevs *info = 0);
};

//-------------------------------------
// BlockInfoBlock
//-------------------------------------

struct BlockInfoBlock {
	struct BlockInfo : Abbrevs {
		uint32		id;
		string		name;
		dynamic_array<pair<uint32, string>>	record_names;
		BlockInfo(uint32 id) : id(id) {}
	};

	dynamic_array<BlockInfo>	info;

	enum BlockInfoCodes {
		BLOCKINFO_CODE_SETBID 		= 1,	// SETBID: [blockid#]
		BLOCKINFO_CODE_BLOCKNAME 	= 2,	// BLOCKNAME: [name]
		BLOCKINFO_CODE_SETRECORDNAME= 3		// BLOCKINFO_CODE_SETRECORDNAME: [id, name]
	};

	const BlockInfo* operator[](uint32 id) const {
		if (!info.empty() && info.back().id == id)
			return &info.back();

		for (auto &i : info)
			if (i.id == id)
				return &i;

		return nullptr;
	}
	BlockInfo *operator[](uint32 id) {
		if (!info.empty() && info.back().id == id)
			return &info.back();

		for (auto &i : info)
			if (i.id == id)
				return &i;

		return &info.push_back(id);
	}

	bool read(BlockReader &&reader, bool ignore_names = false);
	bool write(BlockWriter &&writer, bool ignore_names = false);
};

//-----------------------------------------------------------------------------
// Module subblock codes
//-----------------------------------------------------------------------------

enum IdentificationCodes {
	IDENTIFICATION_CODE_STRING			= 1,	 // IDENTIFICATION:      [strchr x N]
	IDENTIFICATION_CODE_EPOCH			= 2,	 // EPOCH:               [epoch#]
};

enum ModuleCodes {
	MODULE_CODE_VERSION					= 1,	// [version#]
	MODULE_CODE_TRIPLE					= 2,	// [strchr x N]
	MODULE_CODE_DATALAYOUT				= 3,	// [strchr x N]
	MODULE_CODE_ASM						= 4,	// [strchr x N]
	MODULE_CODE_SECTIONNAME				= 5,	// [strchr x N]
	MODULE_CODE_DEPLIB					= 6,	// [strchr x N] Deprecated
	MODULE_CODE_GLOBALVAR				= 7,	// [pointer type, isconst, initid, linkage, alignment, section, visibility, threadlocal]
	MODULE_CODE_FUNCTION				= 8,	// [type, callingconv, isproto, linkage, paramattrs, alignment, section, visibility, gc, unnamed_addr]
	MODULE_CODE_ALIAS_OLD				= 9,	// [alias type, aliasee val#, linkage, visibility]
	MODULE_CODE_GCNAME					= 11,	// [strchr x N]
	MODULE_CODE_COMDAT					= 12,	// [selection_kind, name]
	MODULE_CODE_VSTOFFSET				= 13,	// [offset]
	MODULE_CODE_ALIAS					= 14,	// [alias value type, addrspace, aliasee val#, linkage, visibility]
	MODULE_CODE_METADATA_VALUES_UNUSED	= 15,
	MODULE_CODE_SOURCE_FILENAME			= 16,	// [namechar x N]
	MODULE_CODE_HASH					= 17,	// [5*i32]
	MODULE_CODE_IFUNC					= 18,	// [ifunc value type, addrspace, resolver val#, linkage, visibility]
};

enum AttributeCodes {
	PARAMATTR_CODE_ENTRY_OLD			= 1,  // [paramidx0, attr0, paramidx1, attr1...] Deprecated
	PARAMATTR_CODE_ENTRY				= 2,  // [attrgrp0, attrgrp1, ...]
	PARAMATTR_GRP_CODE_ENTRY			= 3   // [grpid, idx, attr0, attr1, ...]
};

enum TypeCodes {
	TYPE_CODE_NUMENTRY					= 1,	// NUMENTRY:       [numentries]
	TYPE_CODE_VOID						= 2,	// VOID
	TYPE_CODE_FLOAT						= 3,	// FLOAT
	TYPE_CODE_DOUBLE					= 4,	// DOUBLE
	TYPE_CODE_LABEL						= 5,	// LABEL
	TYPE_CODE_OPAQUE					= 6,	// OPAQUE
	TYPE_CODE_INTEGER					= 7,	// INTEGER:        [width]
	TYPE_CODE_POINTER					= 8,	// POINTER:        [pointee type]
	TYPE_CODE_FUNCTION_OLD				= 9,	// FUNCTION:       [vararg, attrid, retty, paramty x N]
	TYPE_CODE_HALF						= 10,	// HALF
	TYPE_CODE_ARRAY						= 11,	// ARRAY:          [numelts, eltty]
	TYPE_CODE_VECTOR					= 12,	// VECTOR:         [numelts, eltty]
	TYPE_CODE_X86_FP80					= 13,	// X86 LONG DOUBLE
	TYPE_CODE_FP128						= 14,	// LONG DOUBLE		(112 bit mantissa)
	TYPE_CODE_PPC_FP128					= 15,	// PPC LONG DOUBLE (2 doubles)
	TYPE_CODE_METADATA					= 16,	// METADATA
	TYPE_CODE_X86_MMX					= 17,	// X86 MMX 64 bit vectors
	TYPE_CODE_STRUCT_ANON				= 18,	// STRUCT_ANON:    [ispacked, eltty x N]
	TYPE_CODE_STRUCT_NAME				= 19,	// STRUCT_NAME:    [strchr x N]
	TYPE_CODE_STRUCT_NAMED				= 20,	// STRUCT_NAMED:   [ispacked, eltty x N]
	TYPE_CODE_FUNCTION					= 21,	// FUNCTION:       [vararg, retty, paramty x N]
	TYPE_CODE_TOKEN						= 22,	// TOKEN
	TYPE_CODE_BFLOAT					= 23,	// BRAIN FLOATING POINT
	TYPE_CODE_X86_AMX					= 24,	// X86 AMX 8192 bit vectors
	TYPE_CODE_OPAQUE_POINTER			= 25,	// OPAQUE_POINTER: [addrspace]
};

enum OperandBundleTagCode {
	OPERAND_BUNDLE_TAG					= 1,	 //[strchr x N]
};

enum SyncScopeNameCode {
	SYNC_SCOPE_NAME						= 1,
};

enum ValueSymtabCodes {
	VST_CODE_ENTRY						= 1,	// [valueid, namechar x N]
	VST_CODE_BBENTRY					= 2,	// [bbid, namechar x N]
	VST_CODE_FNENTRY					= 3,	// [valueid, offset, namechar x N]
	VST_CODE_COMBINED_ENTRY				= 5,	// [valueid, refguid]
};

enum ModulePathSymtabCodes {
	MST_CODE_ENTRY						= 1,	// [modid, namechar x N]
	MST_CODE_HASH						= 2,	// [5*i32]
};

enum GlobalValueSummarySymtabCodes {
	FS_PERMODULE						= 1,	// PERMODULE: [valueid, flags, instcount, numrefs, numrefs x valueid, n x (valueid)]
	FS_PERMODULE_PROFILE				= 2,	// PERMODULE_PROFILE: [valueid, flags, instcount, numrefs, numrefs x valueid,  n x (valueid, hotness)]
	FS_PERMODULE_GLOBALVAR_INIT_REFS	= 3,	// PERMODULE_GLOBALVAR_INIT_REFS: [valueid, flags, n x valueid]
	FS_COMBINED							= 4,	// COMBINED: [valueid, modid, flags, instcount, numrefs, numrefs x valueid, n x (valueid)]
	FS_COMBINED_PROFILE					= 5,	// COMBINED_PROFILE: [valueid, modid, flags, instcount, numrefs, numrefs x valueid, n x (valueid, hotness)]
	FS_COMBINED_GLOBALVAR_INIT_REFS		= 6,	// COMBINED_GLOBALVAR_INIT_REFS: [valueid, modid, flags, n x valueid]
	FS_ALIAS							= 7,	// ALIAS: [valueid, flags, valueid]
	FS_COMBINED_ALIAS					= 8,	// COMBINED_ALIAS: [valueid, modid, flags, valueid]
	FS_COMBINED_ORIGINAL_NAME			= 9,	// COMBINED_ORIGINAL_NAME: [original_name_hash]
	FS_VERSION							= 10,	// VERSION of the summary, bumped when adding flags for instance.
	FS_TYPE_TESTS						= 11,	// The list of llvm.type.test type identifiers used by the following function that are used other than by an llvm.assume. [n x typeid]
	FS_TYPE_TEST_ASSUME_VCALLS			= 12,	// The list of virtual calls made by this function using llvm.assume(llvm.type.test) intrinsics that do not have all constant integer arguments. [n x (typeid, offset)]
	FS_TYPE_CHECKED_LOAD_VCALLS			= 13,	// The list of virtual calls made by this function using llvm.type.checked.load intrinsics that do not have all constant integer arguments. [n x (typeid, offset)]
	FS_TYPE_TEST_ASSUME_CONST_VCALL		= 14,	// Identifies a virtual call made by this function using an llvm.assume(llvm.type.test) intrinsic with all constant integer arguments. [typeid, offset, n x arg]
	FS_TYPE_CHECKED_LOAD_CONST_VCALL	= 15,	// Identifies a virtual call made by this function using an llvm.type.checked.load intrinsic with all constant integer arguments. [typeid, offset, n x arg]
	FS_VALUE_GUID						= 16,	// Assigns a GUID to a value ID. This normally appears only in combined summaries, but it can also appear in per-module summaries for PGO data. [valueid, guid]
	FS_CFI_FUNCTION_DEFS				= 17,	// The list of local functions with CFI jump tables. Function names are strings in strtab. [n * name]
	FS_CFI_FUNCTION_DECLS				= 18,	// The list of external functions with CFI jump tables. Function names are strings in strtab. [n * name]
	FS_PERMODULE_RELBF					= 19,	// Per-module summary that also adds relative block frequency to callee info. PERMODULE_RELBF: [valueid, flags, instcount, numrefs, numrefs x valueid, n x (valueid, relblockfreq)]
	FS_FLAGS							= 20,	// Index-wide flags
	FS_TYPE_ID							= 21,	// Maps type identifier to summary information for that type identifier. Produced by the thin link (only lives in combined index). TYPE_ID: [typeid, kind, bitwidth, align, size, bitmask, inlinebits, n x (typeid, kind, name, numrba, numrba x (numarg, numarg x arg, kind, info, byte, bit))]
	FS_TYPE_ID_METADATA					= 22,	// For background see overview at https://llvm.org/docs/TypeMetadata.html. The type metadata includes both the type identifier and the offset of the address point of the type (the address held by objects of that type which may not be the beginning of the virtual table). Vtable definitions are decorated with type metadata for the types they are compatible with. Maps type identifier to summary information for that type identifier computed from type metadata: the valueid of each vtable definition decorated with a type metadata for that identifier, and the offset from the corresponding type metadata. Exists in the per-module summary to provide information to thin link for index-based whole program devirtualization. TYPE_ID_METADATA: [typeid, n x (valueid, offset)]
	FS_PERMODULE_VTABLE_GLOBALVAR_INIT_REFS	= 23,// Summarizes vtable definition for use in index-based whole program devirtualization during the thin link. PERMODULE_VTABLE_GLOBALVAR_INIT_REFS: [valueid, flags, varflags, numrefs, numrefs x valueid, n x (valueid, offset)]
	FS_BLOCK_COUNT						= 24,	// The total number of basic blocks in the module.
	FS_PARAM_ACCESS						= 25,	// Range information for accessed offsets for every argument. [n x (paramno, range, numcalls, numcalls x (callee_guid, paramno, range))]
};

enum MetadataCodes {
	METADATA_STRING_OLD					= 1,	// [values]
	METADATA_VALUE						= 2,	// [type num, value num]
	METADATA_NODE						= 3,	// [n x md num]
	METADATA_NAME						= 4,	// [values]
	METADATA_DISTINCT_NODE				= 5,	// [n x md num]
	METADATA_KIND						= 6,	// [n x [id, name]]
	METADATA_LOCATION					= 7,	// [distinct, line, col, scope, inlined-at?]
	METADATA_OLD_NODE					= 8,	// [n x (type num, value num)]
	METADATA_OLD_FN_NODE				= 9,	// [n x (type num, value num)]
	METADATA_NAMED_NODE					= 10,	// [n x mdnodes]
	METADATA_ATTACHMENT					= 11,	// [m x [value, [n x [id, mdnode]]]
	METADATA_GENERIC_DEBUG				= 12,	// [distinct, tag, vers, header, n x md num]
	METADATA_SUBRANGE					= 13,	// [distinct, count, lo]
	METADATA_ENUMERATOR					= 14,	// [isUnsigned|distinct, value, name]
	METADATA_BASIC_TYPE					= 15,	// [distinct, tag, name, size, align, enc]
	METADATA_FILE						= 16,	// [distinct, filename, directory, checksumkind, checksum]
	METADATA_DERIVED_TYPE				= 17,	// [distinct, ...]
	METADATA_COMPOSITE_TYPE				= 18,	// [distinct, ...]
	METADATA_SUBROUTINE_TYPE			= 19,	// [distinct, flags, types, cc]
	METADATA_COMPILE_UNIT				= 20,	// [distinct, ...]
	METADATA_SUBPROGRAM					= 21,	// [distinct, ...]
	METADATA_LEXICAL_BLOCK				= 22,	// [distinct, scope, file, line, column]
	METADATA_LEXICAL_BLOCK_FILE			= 23,	// [distinct, scope, file, discriminator]
	METADATA_NAMESPACE					= 24,	// [distinct, scope, file, name, line, exportSymbols]
	METADATA_TEMPLATE_TYPE				= 25,	// [distinct, scope, name, type, ...]
	METADATA_TEMPLATE_VALUE				= 26,	// [distinct, scope, name, type, value, ...]
	METADATA_GLOBAL_VAR					= 27,	// [distinct, ...]
	METADATA_LOCAL_VAR					= 28,	// [distinct, ...]
	METADATA_EXPRESSION					= 29,	// [distinct, n x element]
	METADATA_OBJC_PROPERTY				= 30,	// [distinct, name, file, line, ...]
	METADATA_IMPORTED_ENTITY			= 31,	// [distinct, tag, scope, entity, line, name]
	METADATA_MODULE						= 32,	// [distinct, scope, name, ...]
	METADATA_MACRO						= 33,	// [distinct, macinfo, line, name, value]
	METADATA_MACRO_FILE					= 34,	// [distinct, macinfo, line, file, ...]
	METADATA_STRINGS					= 35,	// [count, offset] blob([lengths][chars])
	METADATA_GLOBAL_DECL_ATTACHMENT		= 36,	// [valueid, n x [id, mdnode]]
	METADATA_GLOBAL_VAR_EXPR			= 37,	// [distinct, var, expr]
	METADATA_INDEX_OFFSET				= 38,	// [offset]
	METADATA_INDEX						= 39,	// [bitpos]
	METADATA_LABEL						= 40,	// [distinct, scope, name, file, line]
	METADATA_STRING_TYPE				= 41,	// [distinct, name, size, align,...]
	METADATA_COMMON_BLOCK				= 44,	// [distinct, scope, name, variable,...]
	METADATA_GENERIC_SUBRANGE			= 45,	// [distinct, count, lo, up, stride]
	METADATA_ARG_LIST					= 46	// [n x [type num, value num]]
};

enum ConstantsCodes {
	CST_CODE_SETTYPE					= 1,	// [typeid]
	CST_CODE_NULL						= 2,	// 
	CST_CODE_UNDEF						= 3,	// 
	CST_CODE_INTEGER					= 4,	// [intval]
	CST_CODE_WIDE_INTEGER				= 5,	// [n x intval]
	CST_CODE_FLOAT						= 6,	// [fpval]
	CST_CODE_AGGREGATE					= 7,	// [n x value number]
	CST_CODE_STRING						= 8,	// [values]
	CST_CODE_CSTRING					= 9,	// [values]
	CST_CODE_CE_BINOP					= 10,	// [opcode, opval, opval]
	CST_CODE_CE_CAST					= 11,	// [opcode, opty, opval]
	CST_CODE_CE_GEP						= 12,	// [n x operands]
	CST_CODE_CE_SELECT					= 13,	// [opval, opval, opval]
	CST_CODE_CE_EXTRACTELT				= 14,	// [opty, opval, opval]
	CST_CODE_CE_INSERTELT				= 15,	// [opval, opval, opval]
	CST_CODE_CE_SHUFFLEVEC				= 16,	// [opval, opval, opval]
	CST_CODE_CE_CMP						= 17,	// [opty, opval, opval, pred]
	CST_CODE_INLINEASM_OLD				= 18,	// [sideeffect|alignstack, asmstr,conststr]
	CST_CODE_CE_SHUFVEC_EX				= 19,	// [opty, opval, opval, opval]
	CST_CODE_CE_INBOUNDS_GEP			= 20,	// [n x operands]
	CST_CODE_BLOCKADDRESS				= 21,	// [fnty, fnval, bb#]
	CST_CODE_DATA						= 22,	// [n x elements]
	CST_CODE_INLINEASM_OLD2				= 23,	// [sideeffect|alignstack| asmdialect,asmstr,conststr]
	CST_CODE_CE_GEP_WITH_INRANGE_INDEX	= 24,	// [opty, flags, n x operands]
	CST_CODE_CE_UNOP					= 25,	// [opcode, opval]
	CST_CODE_POISON						= 26,	// 
	CST_CODE_DSO_LOCAL_EQUIVALENT		= 27,	// [gvty, gv]
	CST_CODE_INLINEASM_OLD3				= 28,	// [sideeffect|alignstack| asmdialect|unwind, asmstr,conststr]
	CST_CODE_NO_CFI_VALUE				= 29,	// [ fty, f ]
	CST_CODE_INLINEASM					= 30,	// [fnty, sideeffect|alignstack| asmdialect|unwind, asmstr,conststr]
};

enum FunctionCodes {
	FUNC_CODE_DECLAREBLOCKS				= 1,	// [n]
	FUNC_CODE_INST_BINOP				= 2,	// [opcode, ty, opval, opval]
	FUNC_CODE_INST_CAST					= 3,	// [opcode, ty, opty, opval]
	FUNC_CODE_INST_GEP_OLD				= 4,	// [n x operands]
	FUNC_CODE_INST_SELECT				= 5,	// [ty, opval, opval, opval]
	FUNC_CODE_INST_EXTRACTELT			= 6,	// [opty, opval, opval]
	FUNC_CODE_INST_INSERTELT			= 7,	// [ty, opval, opval, opval]
	FUNC_CODE_INST_SHUFFLEVEC			= 8,	// [ty, opval, opval, opval]
	FUNC_CODE_INST_CMP					= 9,	// [opty, opval, opval, pred]
	FUNC_CODE_INST_RET					= 10,	// [opty,opval<both optional>]
	FUNC_CODE_INST_BR					= 11,	// [bb#, bb#, cond] or [bb#]
	FUNC_CODE_INST_SWITCH				= 12,	// [opty, op0, op1, ...]
	FUNC_CODE_INST_INVOKE				= 13,	// [attr, fnty, op0,op1, ...] 14 is unused.
	FUNC_CODE_INST_UNREACHABLE			= 15,	// 
	FUNC_CODE_INST_PHI					= 16,	// [ty, val0,bb0, ...] 17 is unused. 18 is unused.
	FUNC_CODE_INST_ALLOCA				= 19,	// [instty, opty, op, align]
	FUNC_CODE_INST_LOAD					= 20,	// [opty, op, align, vol] 21 is unused. 22 is unused.
	FUNC_CODE_INST_VAARG				= 23,	// [valistty, valist, instty] This store code encodes the pointer type, rather than the value type this is so information only available in the pointer type (e.g. address spaces) is retained.
	FUNC_CODE_INST_STORE_OLD			= 24,	// [ptrty,ptr,val, align, vol] 25 is unused.
	FUNC_CODE_INST_EXTRACTVAL			= 26,	// [n x operands]
	FUNC_CODE_INST_INSERTVAL			= 27,	// [n x operands] fcmp/icmp returning Int1TY or vector of Int1Ty. Same as CMP, exists to support legacy vicmp/vfcmp instructions.
	FUNC_CODE_INST_CMP2					= 28,	// [opty, opval, opval, pred] new select on i1 or [N x i1]
	FUNC_CODE_INST_VSELECT				= 29,	// [ty,opval,opval,predty,pred]
	FUNC_CODE_INST_INBOUNDS_GEP_OLD		= 30,	// [n x operands]
	FUNC_CODE_INST_INDIRECTBR			= 31,	// [opty, op0, op1, ...] 32 is unused.
	FUNC_CODE_DEBUG_LOC_AGAIN			= 33,	// 
	FUNC_CODE_INST_CALL					= 34,	// [attr, cc, fnty, fnid, args...]
	FUNC_CODE_DEBUG_LOC					= 35,	// [Line,Col,ScopeVal, IAVal]
	FUNC_CODE_INST_FENCE				= 36,	// [ordering, synchscope]
	FUNC_CODE_INST_CMPXCHG_OLD			= 37,	// [ptrty, ptr, cmp, val, vol, ordering, synchscope, failure_ordering?, weak?]
	FUNC_CODE_INST_ATOMICRMW_OLD		= 38,	// [ptrty, ptr, val, operation, align, vol, ordering, synchscope]
	FUNC_CODE_INST_RESUME				= 39,	// [opval]
	FUNC_CODE_INST_LANDINGPAD_OLD		= 40,	// [ty,val,val,num,id0,val0...]
	FUNC_CODE_INST_LOADATOMIC			= 41,	// [opty, op, align, vol, ordering, synchscope]
	FUNC_CODE_INST_STOREATOMIC_OLD		= 42,	// [ptrty,ptr,val, align, vol ordering, synchscope]
	FUNC_CODE_INST_GEP					= 43,	// [inbounds, n x operands]
	FUNC_CODE_INST_STORE				= 44,	// [ptrty,ptr,valty,val, align, vol]
	FUNC_CODE_INST_STOREATOMIC			= 45,	// [ptrty,ptr,val, align, vol
	FUNC_CODE_INST_CMPXCHG				= 46,	// [ptrty, ptr, cmp, val, vol, success_ordering, synchscope, failure_ordering, weak]
	FUNC_CODE_INST_LANDINGPAD			= 47,	// [ty,val,num,id0,val0...]
	FUNC_CODE_INST_CLEANUPRET			= 48,	// [val] or [val,bb#]
	FUNC_CODE_INST_CATCHRET				= 49,	// [val,bb#]
	FUNC_CODE_INST_CATCHPAD				= 50,	// [bb#,bb#,num,args...]
	FUNC_CODE_INST_CLEANUPPAD			= 51,	// [num,args...]
	FUNC_CODE_INST_CATCHSWITCH			= 52,	// [num,args...] or [num,args...,bb] 53 is unused. 54 is unused.
	FUNC_CODE_OPERAND_BUNDLE			= 55,	// [tag#, value...]
	FUNC_CODE_INST_UNOP					= 56,	// [opcode, ty, opval]
	FUNC_CODE_INST_CALLBR				= 57,	// [attr, cc, norm, transfs, fnty, fnid, args...]
	FUNC_CODE_INST_FREEZE				= 58,	// [opty, opval]
	FUNC_CODE_INST_ATOMICRMW			= 59,	// [ptrty, ptr, valty, val, operation, align, vol, ordering, synchscope]
	FUNC_CODE_BLOCKADDR_USERS			= 60,	// [value...]
};

enum UseListCodes {
	USELIST_CODE_DEFAULT				= 1,  // DEFAULT: [index..., value-id]
	USELIST_CODE_BB						= 2   // BB: [index..., bb-id]
};

enum StrtabCodes {
	STRTAB_BLOB							= 1,
};

enum SymtabCodes {
	SYMTAB_BLOB							= 1,
};

//-----------------------------------------------------------------------------
// instruction encoding
//-----------------------------------------------------------------------------

enum CallMarkersFlags {
	CALL_TAIL			= 1 << 0,
	CALL_CCONV			= 1 << 1,
	CALL_MUSTTAIL		= 1 << 14,
	CALL_EXPLICIT_TYPE	= 1 << 15,
	CALL_NOTAIL			= 1 << 16,
	CALL_FMF			= 1 << 17	// Call has optional fast-math-flags
};

struct AllocaPackedValues {
	union {
		uint64	u;
		struct { uint8	align_lo:5, call_argument:1, explicit_type:1, swift_error:1, align_hi:3; };
	};
	constexpr uint32	align() const { return align_lo + align_hi * 32; }
	AllocaPackedValues(uint64 u) : u(u) {}
};

enum class Linkage : uint8 {
	External			= 0,	// Externally visible function
	Appending			= 2,	// Special purpose, only applies to global arrays
	Internal			= 3,	// Rename collisions when linking (static functions)
	ExternalWeak		= 7,	// 
	Common				= 8,	// Tentative definitions
	Private				= 9,	// Like Internal, but omit from symbol table
	AvailableExternally	= 12,	// Available for inspection, not emission
	WeakAny				= 16,	// Keep one copy of named function when linking (weak)
	WeakODR				= 17,	// Same, but only replaced by something equivalent
	LinkOnceAny			= 18,	// Keep one copy of function when linking (inline)
	LinkOnceODR			= 19,	// Same, but only replaced by something equivalent
};

enum class UnnamedAddr : uint8 {
	None				= 0,
	Local				= 1,
	Global				= 2,
};

enum class Visibility : uint8 {
	Default				= 0,
	Hidden				= 1,
	Protected			= 2,
};

enum class DLLStorageClass : uint8 {
	Default				= 0,
	DLLImport			= 1,
	DLLExport			= 2,
};

enum class ThreadLocalMode : uint8 {
	NotThreadLocal		= 0,
	GeneralDynamicTLS	= 1,
	LocalDynamicTLS		= 2,
	InitialExecTLS		= 3,
	LocalExecTLS		= 4,
};

enum class CallingConv : uint8 {
	C					= 0,
	Fast				= 8,
	Cold				= 9,
	GHC					= 10,
	HiPE				= 11,
	WebKit_JS			= 12,
	AnyReg				= 13,
	PreserveMost		= 14,
	PreserveAll			= 15,
	FirstTargetCC		= 64,
	X86_StdCall			= 64,
	X86_FastCall		= 65,
	ARM_APCS			= 66,
	ARM_AAPCS			= 67,
	ARM_AAPCS_VFP		= 68,
	MSP430_INTR			= 69,
	X86_ThisCall		= 70,
	PTX_Kernel			= 71,
	PTX_Device			= 72,
	SPIR_FUNC			= 75,
	SPIR_KERNEL			= 76,
	Intel_OCL_BI		= 77,
	X86_64_SysV			= 78,
	X86_64_Win64		= 79,
	X86_VectorCall		= 80
};

//-----------------------------------------------------------------------------
// Attributes
//-----------------------------------------------------------------------------

enum class AttributeIndex {
	//0 is unused
	Alignment					= 1,
	AlwaysInline				= 2,
	ByVal						= 3,
	InlineHint					= 4,
	InReg						= 5,
	MinSize						= 6,
	Naked						= 7,
	Nest						= 8,
	NoAlias						= 9,
	NoBuiltin					= 10,
	NoCapture					= 11,
	NoDuplicate					= 12,
	NoImplicitFloat				= 13,
	NoInline					= 14,
	NonLazyBind					= 15,
	NoRedZone					= 16,
	NoReturn					= 17,
	NoUnwind					= 18,
	OptimizeForSize				= 19,
	ReadNone					= 20,
	ReadOnly					= 21,
	Returned					= 22,
	ReturnsTwice				= 23,
	SExt						= 24,
	StackAlignment				= 25,
	StackProtect				= 26,
	StackProtectReq				= 27,
	StackProtectStrong			= 28,
	StructRet					= 29,
	SanitizeAddress				= 30,
	SanitizeThread				= 31,
	SanitizeMemory				= 32,
	UwTable						= 33,
	ZExt						= 34,
	Builtin						= 35,
	Cold						= 36,
	OptimizeNone				= 37,
	InAlloca					= 38,
	NonNull						= 39,
	JumpTable					= 40,
	Dereferenceable				= 41,
	DereferenceableOrNull		= 42,
	Convergent					= 43,
	Safestack					= 44,
	Argmemonly					= 45,
	SwiftSelf					= 46,
	SwiftError					= 47,
	NoRecurse					= 48,
	InaccessiblememOnly			= 49,
	InaccessiblememOrArgmemonly	= 50,
	AllocSize					= 51,
	Writeonly					= 52,
	Speculatable				= 53,
	StrictFp					= 54,
	SanitizeHwaddress			= 55,
	NocfCheck					= 56,
	OptForFuzzing				= 57,
	Shadowcallstack				= 58,
	SpeculativeLoadHardening	= 59,
	Immarg						= 60,
	Willreturn					= 61,
	Nofree						= 62,
	Nosync						= 63,
	SanitizeMemtag				= 64,
	Preallocated				= 65,
	NoMerge						= 66,
	NullPointerIsvalid			= 67,
	Noundef						= 68,
	Byref						= 69,
	Mustprogress				= 70,
	NoCallback					= 71,
	Hot							= 72,
	NoProfile					= 73,
	VscaleRange					= 74,
	SwiftAsync					= 75,
	NoSanitizeCoverage			= 76,
	Elementtype					= 77,
	DisableSanitizerInstrumentation = 78,
	NoSanitizeBounds			= 79,
	AllocAlign					= 80,
	AllocatedPointer			= 81,
};

struct AttributeGroup : refs<AttributeGroup> {
	enum Slot : uint32 {
		InvalidSlot		= -2u,
		FunctionSlot	= -1u,
		ReturnSlot		= 0,
		Param1Slot		= 1,
	};
	uint32		slot		= InvalidSlot;
	uint64		params		= 0;
	sparse_array<uint64, AttributeIndex>	values;
	dynamic_array<pair<string, string>>		strs;

	void	set(AttributeIndex i)			{ params |= bit64((int)i); }
	void	set(AttributeIndex i, uint64 v)	{ set(i); values[i] = v; }

	bool	valid()					const	{ return slot != InvalidSlot; }
	bool	test(AttributeIndex i)	const	{ return params & bit64((int)i); }
	uint64	get(AttributeIndex i)	const	{ return test(i) ? (values[i].or_default(1)) : 0; }

	const char*	get(const char *s)	const {
		for (auto &i : strs) {
			if (i.a == s)
				return i.b ? i.b : "";
		}
		return nullptr;
	}
	bool	operator==(const AttributeGroup &b) const { return this == &b; }
};

struct _AttributeSet : refs<_AttributeSet>, dynamic_array<ref_ptr<AttributeGroup>> {
	void	add(AttributeGroup *g) {
		set(g->slot + 1) = g;
	}
};

struct AttributeSet : ref_ptr<_AttributeSet> {
	auto	operator[](int i) {
		return p && i > AttributeGroup::InvalidSlot && i + 1 < p->size() ? (*p)[i + 1] : nullptr;
	}
};

//-----------------------------------------------------------------------------
// Type
//-----------------------------------------------------------------------------

struct Typed;
struct TypedPointer;

struct Type {
	enum AddrSpace {
		Default			 = 0,
		DeviceMemory	 = 1,
		CBuffer			 = 2,
		GroupShared		 = 3,
		GenericPointer	 = 4,
		ImmediateCBuffer = 5,
		NumAddrSpaces,
	};
	enum TypeKind {
		Void,
		Float,
		Int,
		Vector,
		Pointer,
		Array,
		Function,
		Struct,
		Metadata,
		Label,
	};

	TypeKind	type;

	union {
		uint32		size;			// int, float, array, vector
		bool		vararg;			// function only
		struct {					// struct
			bool	packed:1, opaque:1;
		};
		AddrSpace	addrSpace;		// pointer
	};

	const Type*					subtype = nullptr;
	string						name;
	dynamic_array<const Type*>	members;	// the members for a struct, the parameters for functions

	Type(TypeKind type, uint32 size = 0, const Type *subtype = nullptr) : type(type), size(size), subtype(subtype) {}
	Type(TypeKind type, uint32 size, const Type *subtype, dynamic_array<const Type*> &&members) : type(type), size(size), subtype(subtype), members(move(members)) {}

	template<typename T> static constexpr enable_if_t<is_int<T>, Type>			make()	{ return {Int, BIT_COUNT<T>}; }
	template<typename T> static constexpr enable_if_t<is_float<T>, Type>		make()	{ return {Float, BIT_COUNT<T>}; }
	template<typename T> static constexpr enable_if_t<same_v<bool, T>, Type>	make()	{ return {Int, 1}; }
	template<typename T> static constexpr enable_if_t<same_v<void, T>, Type>	make()	{ return {Void}; }

	static Type		make_vector(const Type *subtype, uint32 size)							{ return {Vector, size, subtype}; }
	static Type		make_pointer(const Type *subtype, AddrSpace addrSpace)			{ return {Pointer, addrSpace, subtype}; }
	static Type		make_array(const Type *subtype, uint32 size)							{ return {Array, size, subtype}; }
	static Type		make_function(const Type *ret, dynamic_array<const Type*> &&members)	{ return {Function, 0, ret, move(members)}; }
	static Type		make_struct(dynamic_array<const Type*> &&members)						{ return {Struct, 0, nullptr, move(members)}; }

	template<typename T> static const Type* get()	{ static Type t = make<T>(); return &t; }

	bool	operator==(const Type &b)	const;
	uint32	bit_size(uint32 &alignment)	const;
	uint32	bit_offset(uint32 i)		const;
	uint32	bit_offset(uint32 i, const Type *&sub)	const;
	uint32	byte_size(uint32 &alignment)const	{ return (bit_size(alignment) + 7) / 8; }
	uint32	bit_size()					const	{ uint32 alignment; return bit_size(alignment); }
	uint32	byte_size()					const	{ uint32 alignment; return byte_size(alignment); }
	bool	is_scalar()					const	{ return type <= Int; }
	bool	is_float()					const	{ return type == Float; }

	uint64	aligned_size(uint64 i = 1)	const	{
		uint32	alignment, size	= bit_size(alignment);
		return align((size + 7) / 8, alignment) * i;
	}

	uint64	load(const void *p)			const	{ return p ? read_bits((const uint64*)p, 0, bit_size()) : 0; }
	void	store(void *p, rint64 val)	const	{ write_bits(p, val, 0, bit_size()); }

	template<typename T> enable_if_t<(is_builtin_int<T> && !is_signed<T>) || is_char<T> || same_v<T, bool> || is_enum<T>, T> get(const void *p) const {
		ISO_ASSERT(type == Type::Int);
		return (T)read_bits((uint64*)p, 0, size);
	}
	template<typename T> enable_if_t<is_signed<T> && !is_enum<T>, T> get(const void *p) const {
		ISO_ASSERT(type == Type::Int);
		return read_bits((T*)p, 0, size);
	}
	template<> float get<float>(const void *p) const;
	
	template<typename T> enable_if_t<is_builtin_int<T> || is_char<T> || same_v<T, bool> || is_enum<T>, void> set(void *p, T t) const {
		ISO_ASSERT(type == Int);
		write_bits(p, uint_for_t<T>(t), 0, size);
	}
	void set(void *p, float t)			const;
	void set(void *p, const Typed &t)	const;
	void set(void *p, const TypedPointer &t)	const;

	template<typename A, typename B> void set(void *p, pair<A,B> t)	const {
		*(pair<noref_t<A>, noref_t<B>>*)p = t;
	}

	template<typename T> T get(uint64 u)	const { return get<T>(&u); }
	template<typename T> uint64 set(T t)	const { uint64 u = 0; set(&u, t); return u; }

	friend TypeKind TypeType(const Type *t) { return t ? t->type : Void; }
};

struct Types : dynamic_array<const Type*> {
	using dynamic_array<const Type*>::operator=;
	const Type	*void_type = nullptr;
	const Type	*bool_type = nullptr;

	const Type *find(const Type &type) const {
		for (auto i : *this) {
			if (*i == type)
				return i;
		}
		return nullptr;
	}

	template<typename T> const Type* get() {
		return find(Type::make<T>());
	}
	template<> const Type* get<void>() {
		if (!void_type)
			void_type = find(Type::make<void>());
		return void_type;
	}
	template<> const Type* get<bool>() {
		if (!bool_type)
			bool_type = find(Type::make<bool>());
		return bool_type;
	}
};

//-----------------------------------------------------------------------------
// Value
//-----------------------------------------------------------------------------

struct Value;
struct Constant;
struct GlobalVar;
struct Alias;
struct Metadata;
struct Instruction;
struct Function;
struct Block;
struct DebugInfo;
struct Eval;

typedef dynamic_array<Value>	ValueArray;

struct ValueHeader {
	uint8	kind;
	constexpr ValueHeader(uint8 kind) : kind(kind) {}
};

template<typename T> struct ValueP : ValueHeader {
	ValueP() : ValueHeader(Value::best_index<const T*>) {}
};
template<typename T> struct ValueT : ValueHeader {
	T	t;
	ValueT(const T &t) : ValueHeader(Value::best_index<T>), t(t) {}
};

struct ForwardRef {
	size_t	index;
	explicit ForwardRef(size_t index) : index(index) {}
	bool	operator==(const ForwardRef &b) const { return index == b.index; }
};

struct InlineAsm : ValueP<InlineAsm> {
	union {
		uint32	flags;
		struct {
			bool	side_effects:1, align_stack:1, dialect:1, can_throw:1;
		};
	};
	string		code;
	string		constraints;
	InlineAsm(uint32 flags, string_ref code, string_ref constraints) : flags(flags), code(code), constraints(constraints) {}
};

struct Value {
	typedef type_list<
		_none,			//p = 0
		uint64,			//p & 7 == 1
		string,			//p & 7 == 2
		ValueArray,		//p & 7 == 3
		ForwardRef,		//p & 7 == 4
		_zero,			//p = 5
		const Eval*,
		const Constant*,
		const GlobalVar*,
		const Alias*,
		const Metadata*,
		const Instruction*,
		const Function*,
		const Block*,
		const DebugInfo*,
		const InlineAsm*
	> types;

	template<typename T>	static constexpr int best_index = meta::TL_find<best_match_t<T, types>, types>;

	static const ValueHeader* ptr(intptr_t i)				{ return (const ValueHeader*)i; }
	static const ValueHeader* ptr(const void *p, int i)		{ return ptr((intptr_t)p + i); }

	static auto						_get(_zero* t)			{ return zero; }
	static uint64					_get(uint64* t)			{ return intptr_t(t) & 7 ? intptr_t(t) >> 3 : ((ValueT<uint64>*)t)->t; }
	static cstring					_get(string* t)			{ return (const char*)(intptr_t(t) & ~7); }
	static embedded_array<Value>&	_get(ValueArray* t)		{ return *(embedded_array<Value>*)(intptr_t(t) & ~7); }
	static ForwardRef				_get(ForwardRef* t)		{ return ForwardRef(intptr_t(t) >> 3); }
	template<typename T> static constexpr T* _get(T** t)	{ return (T*)t; }

	const ValueHeader *p;

	Value(const ValueHeader *p = nullptr) : p(p) {}
	Value(const _none&)			: p(nullptr)					{}
	Value(uint64 u)				: p((u << 3 >> 3) == u ? ptr((u << 3) | 1) : new ValueT<uint64>(u)) {}
	Value(string &&s)			: p(ptr(s.detach(), 2))			{}
	Value(ValueArray &&a)		: p(ptr(make<embedded_array<Value>>(a), 3))	{}
	Value(const ForwardRef &f)	: p(ptr((f.index << 3) + 4))	{}
	Value(const _zero&)			: p(ptr(5))						{}
	template<typename T> Value(const T *p) : p(static_cast<const ValueHeader*>(p)) {}

	operator const ValueHeader*()	const	{ return p; }
	bool operator==(const Value &b) const	{ return p == b.p; }

	int				kind() const {
		if (int i = intptr_t(p) & 7)
			return i;
		return p ? p->kind : 0;
	}
	template<typename T>	bool			is()		const	{ return kind() == best_index<T>; }
	template<typename T>	decltype(auto)	get_known() const	{ ISO_ASSERT(kind() == best_index<T>); return _get((T*)p); }
	template<typename T>	optional<decltype(_get(declval<T*>()))>	get() const	{ if (kind() == best_index<T>) return _get((T*)p); return none; }

	const Type*		GetType(bool warn = true)	const;
	const string*	GetName(bool warn = true)	const;
};

struct Typed {
	const Type	*type;
	uint64		value;

	Typed(const Type *type = nullptr, uint64 value = 0) : type(type), value(value) {}
	Typed(const Value &v);
	template<typename E> static Typed Custom(const Value &v, E &&exec) {
		switch (v.kind()) {
			case Value::best_index<const GlobalVar*>:
			case Value::best_index<const Constant*>: {
				auto	c = (v.is<const GlobalVar*>() ? v.get_known<const GlobalVar*>()->value : v).get_known<const Constant*>();
				if (c->value.is<const Eval>())
					return Typed(c->type, c->value.get_known<const Eval*>()->Evaluate(c->type, exec));
			}
			//fallthrough
			default:
				return Typed(v);
		}
	}

	template<typename T> operator rawint<T>()	const { return (T)value; }
	template<typename T> operator T()	const { return type->get<T>(value); }
	template<typename T> operator T*()	const { return (T*)value; }
};

struct TypedPointerBase {
	const Type	*type;
	void		*p;

	TypedPointerBase(const Type *type = nullptr, void *p = nullptr) : type(type), p(p) {}
	template<typename T> operator T*()	const { return (T*)p; }

	void			store(rint64 val)	const	{ type->store(p, val); }
	Typed			load()				const	{ return {type, type->load(p)}; }
	template<typename T> T load()		const	{ return type->get<T>(p); }
	template<typename T> T extract(uint32 offset, uint32 size) const	{ return *(T*)((uint8*)p + offset / 8) & bits<T>(size); }
};

struct TypedPointer : TypedPointerBase {
	using TypedPointerBase::TypedPointerBase;

	TypedPointer(const Typed& t) : TypedPointerBase(t.type->subtype, (void*)t.value) {
		ISO_ASSERT(t.type->type == Type::Pointer || t.type->type == Type::Array);
	}

	TypedPointer	index(uint64 i)		const {
		return {type, (uint8*)p + type->byte_size() * i};
	}
	TypedPointer	deindex(uint64 i)	const {
		const Type	*sub;
		uint32		offset	= type->bit_offset(i, sub);
		return {sub, (uint8*)p + offset / 8};
	}
};


//-----------------------------------------------------------------------------
// Constant / GlobalVar / Alias
//-----------------------------------------------------------------------------

void LayoutValue(const Type *type, const Value &v, void* p, bool clear_undef);

struct TypedValue {
	const Type *type;
	Value		value;
	TypedValue(const Type *type, Value value = none) : type(type), value(value) {}
	void Layout(void* p, bool clear_undef) const { LayoutValue(type, value, p, clear_undef); }
//	operator Typed()	const { return {type, Typed(value).value}; }

};

struct Constant : ValueP<Constant>, TypedValue {
	Constant(const Type *type, Value value) : TypedValue(type, value) {}
};

struct Comdat {
	enum Selection : uint8 {
		Any					= 1,
		ExactMatch			= 2,
		Largest				= 3,
		NoDuplicates		= 4,
		SameSize			= 5,
	};
	Selection	sel;
	string		name;
};

struct GlobalVar : ValueP<GlobalVar>, TypedValue {
	union {
		uint32	flags	= 0;
		struct {
			Linkage			linkage			: 5;
			Visibility		visibility		: 2;
			DLLStorageClass dll_storage		: 2;
			UnnamedAddr		unnamed_addr	: 2;
			ThreadLocalMode threadlocal		: 3;
			bool			is_const		: 1;
			bool			external_init	: 1;
		};
	};
	string			name;
	uint64			align		= 0;
	const char*		section		= nullptr;
	const Comdat*	comdat		= nullptr;
	GlobalVar(const Type *type) : TypedValue(type) {}

	void Layout(void* p, bool clear_undef) const {
		ISO_ASSERT(type->type == Type::Pointer);
		LayoutValue(type->subtype, value, p, clear_undef);
	}

};

struct Alias : ValueP<Alias>, TypedValue {
	union {
		uint32	flags	= 0;
		struct {
			Linkage			linkage			: 5;
			Visibility		visibility		: 2;
			DLLStorageClass dll_storage		: 2;
			UnnamedAddr		unnamed_addr	: 2;
			ThreadLocalMode threadlocal		: 3;
		};
	};
	string			name;
	Alias(const Type *type, Value value) : TypedValue(type, value) {}
};

//-----------------------------------------------------------------------------
// Metadata
//-----------------------------------------------------------------------------

struct Metadata : ValueP<Metadata>, TypedValue {
	bool		distinct;

	Metadata(const Type *type, Value value, bool distinct = false) : TypedValue(type, value), distinct(distinct) {}
	uint64		get64()				const { return Typed(value).value; }
	operator Typed()				const { return {type, get64()}; }
	constexpr bool	is_constant()	const { return type || value.is<string>(); }
};

struct MetadataRef {
	const Metadata	*m;
	MetadataRef(const Metadata *m = nullptr) : m(m) {}

	dynamic_array<MetadataRef>	children() const {
		if (m)
			return transformc(m->value.get<ValueArray>(), [](const Value& v) { return v.get<const Metadata*>().or_default(); });
		return none;
	}
	bool	exists()			const { return !!m; }
	auto	operator->()		const { return m; }
	operator const Metadata*()	const { return m; }
	operator const DebugInfo*()	const { return m ? m->value.get_known<const DebugInfo*>() : nullptr; }
	operator string()			const { return m->value.get_known<string>(); }
	explicit operator float()	const { return m && m->type && m->type->type == Type::Float ? (Typed)m : 0.f; }
	explicit operator bool()	const { return m && (!m->type || m->type->type != Type::Int || m->get64()); }

	template<typename T, typename = enable_if_t<is_builtin_int<T> || is_char<T> || is_enum<T>>> explicit operator T() const	{
		return m && m->type && m->type->type == Type::Int ? T(m->get64()) : T();
	}
	template<typename T, int N>		operator array<T, N>()		const { return children(); }
	template<typename T>			operator dynamic_array<T>()	const { return children(); }
	template<typename T> explicit	operator const T*()			const { return m ? m->value.get_known<const T*>() : nullptr; }

	template<typename T> T get() const { return T(*this); }
	friend bool operator==(MetadataRef a, MetadataRef b) { return a.m == b.m; }
};

// not NULL
struct MetadataRef1 : MetadataRef {
	using MetadataRef::MetadataRef;
};

struct MetaString {
	const Metadata	*m;
	MetaString(const Metadata *m = nullptr) : m(m) {}
	bool	exists()			const	{ return !!m; }
	const char* operator*()		const	{ return m ? m->value.get_known<string>().begin() : nullptr; }
	operator const string()		const	{ return m->value.get_known<string>(); }
	explicit operator bool()	const	{ return operator*(); }
};

struct NamedMetadata : dynamic_array<MetadataRef> {
	string	name;
	NamedMetadata(string &&name, dynamic_array<MetadataRef>&& a) : dynamic_array<MetadataRef>(move(a)), name(move(name)) {}
};

struct AttachedMetadata {
	const char*		kind;
	const Metadata*	metadata;
};

//-----------------------------------------------------------------------------
// DebugInfo
//-----------------------------------------------------------------------------

struct DILocation;
struct DIFile;
struct DISubprogram;

struct DebugInfo : ValueP<DebugInfo> {
	enum Flags {
		None				= 0,
		Private				= 1,
		Protected			= 2,
		Public				= 3,
		FwdDecl				= 1 << 2,
		AppleBlock			= 1 << 3,
		BlockByrefStruct	= 1 << 4,
		Virtual				= 1 << 5,
		Artificial			= 1 << 6,
		Explicit			= 1 << 7,
		Prototyped			= 1 << 8,
		ObjcClassComplete	= 1 << 9,
		ObjectPointer		= 1 << 10,
		Vector				= 1 << 11,
		StaticMember		= 1 << 12,
		LValueReference		= 1 << 13,
		RValueReference		= 1 << 14,
	};

	MetadataCodes	kind;

	DebugInfo(MetadataCodes t) : kind(t) {}

	const char*			get_name()			const;
	const DIFile*		get_file()			const;
	const DISubprogram*	get_subprogram()	const;

	template<typename T> const T *as()		const { return kind == T::DIType ? (const T*)this : nullptr; }
	template<typename W> bool write(W&& w)	const { return false; }
};

template<MetadataCodes T> struct DebugInfoT : DebugInfo {
	static const auto DIType = T;
	DebugInfoT() : DebugInfo(T) {}
};


//-----------------------------------------------------------------------------
// Operation
//-----------------------------------------------------------------------------

enum class OverflowFlags : uint8 {
	NoUnsignedWrap		= 1 << 0,
	NoSignedWrap		= 1 << 1,
};

enum class FastMathFlags : uint8 {
	None				= 0,
	UnsafeAlgebra		= 1 << 0,	 // Legacy
	NoNaNs				= 1 << 1,
	NoInfs				= 1 << 2,
	NoSignedZeros		= 1 << 3,
	AllowReciprocal		= 1 << 4,
	AllowContract		= 1 << 5,
	ApproxFunc			= 1 << 6,
	AllowReassoc		= 1 << 7,
	FastNan				= UnsafeAlgebra | NoInfs | NoSignedZeros | AllowReciprocal,
	Fast				= UnsafeAlgebra | NoNaNs | NoInfs | NoSignedZeros | AllowReciprocal
};

enum class ExactFlags : uint8 {
	Exact				= 1 << 0
};

enum class FPredicate : uint8 {	// U L G E	Intuitive operation
	never		= 0,			// 0 0 0 0	Always false (always folded)
	oeq			= 1,			// 0 0 0 1	True if ordered and equal
	ogt			= 2,			// 0 0 1 0	True if ordered and greater than
	oge			= 3,			// 0 0 1 1	True if ordered and greater than or equal
	olt			= 4,			// 0 1 0 0	True if ordered and less than
	ole			= 5,			// 0 1 0 1	True if ordered and less than or equal
	one			= 6,			// 0 1 1 0	True if ordered and operands are unequal
	ord			= 7,			// 0 1 1 1	True if ordered (no nans)
	uno			= 8,			// 1 0 0 0	True if unordered: isnan(X) | isnan(Y)
	ueq			= 9,			// 1 0 0 1	True if unordered or equal
	ugt			= 10,			// 1 0 1 0	True if unordered or greater than
	uge			= 11,			// 1 0 1 1	True if unordered, greater than, or equal
	ult			= 12,			// 1 1 0 0	True if unordered or less than
	ule			= 13,			// 1 1 0 1	True if unordered, less than, or equal
	une			= 14,			// 1 1 1 0	True if unordered or not equal
	always		= 15,			// 1 1 1 1	Always true (always folded)
};

enum class IPredicate : uint8 {
	eq			= 0,
	ne			= 1,
	ugt			= 2,
	uge			= 3,
	ult			= 4,
	ule			= 5,
	sgt			= 6,
	sge			= 7,
	slt			= 8,
	sle			= 9,
};

enum class RMWOperations : uint8 {
	Xchg		= 0,
	Add			= 1,
	Sub			= 2,
	And			= 3,
	Nand		= 4,
	Or			= 5,
	Xor			= 6,
	Max			= 7,
	Min			= 8,
	UMax		= 9,
	UMin		= 10,
	FAdd		= 11,
	FSub		= 12
};

enum class AtomicOrderingCodes : uint8 {
	notatomic	= 0,
	unordered	= 1,
	monotonic	= 2,
	acquire		= 3,
	release		= 4,
	acqrel		= 5,
	seqcst		= 6
};

enum class AtomicSynchScopeCodes : uint8 {
	SingleThread 	= 0,
	CrossThread 	= 1
};

enum class CastOpcodes {
	Trunc			= 0,
	ZExt			= 1,
	SExt			= 2,
	FToU			= 3,
	FToS			= 4,
	UToF			= 5,
	SToF			= 6,
	FPTrunc			= 7,
	FPExt			= 8,
	PtrtoI			= 9,
	ItoPtr			= 10,
	Bitcast			= 11,
	AddrSpaceCast	= 12 
};

enum class UnaryOpcodes {
	FNeg		= 0
};

enum class BinaryOpcodes {
	Add			= 0,
	Sub			= 1,
	Mul			= 2,
	UDiv		= 3,
	SDiv		= 4,	 // overloaded for FP
	URem		= 5,
	SRem		= 6,	 // overloaded for FP
	Shl			= 7,
	Lshr		= 8,
	Ashr		= 9,
	And			= 10,
	Or			= 11,
	Xor			= 12,
	Div			= 13,	 //for disassembling
	Rem			= 14,	 //for disassembling
};

enum class Operation : uint8 {
	Nop,

	// Terminator Instructions
	Ret,			Br,				Switch,			IndirectBr,		Invoke,				Resume,			Unreachable,

	// Cast operators
	Trunc,			ZExt,			SExt,			FToU,			FToS,				UToF,			SToF,			FPTrunc,
	FPExt,			PtrToI,			IToPtr,			Bitcast,		AddrSpaceCast,

	// Unary operators
	FNeg,

	// Binary operators
	Add,			Sub,			Mul,			UDiv,			SDiv,				URem,			SRem,			Shl,
	Lshr,			Ashr,			And,			Or,				Xor,
	FAdd,			FSub,			FMul,			FDiv,			FRem,

	// Memory operators
	Alloca,			Load,			Store,			GetElementPtr,		Fence,			CmpXchg,		LoadAtomic,		StoreAtomic,
	AtomicRMW,

	// Other operators
	FCmp,			ICmp,
	Phi,			Call,			Select,			UserOp1,			UserOp2,		VAArg,			ExtractElement,	InsertElement,
	ShuffleVector,	ExtractValue,	InsertValue,	LandingPad,
};

inline Operation	operator+(Operation a, uint64 b)	{ return Operation((uint64)a + b); }
inline int			operator-(Operation a, Operation b)	{ return int(a) - int(b); }

inline bool			is_cast(Operation op)				{ return between(op, Operation::Trunc, Operation::AddrSpaceCast); }
inline bool			is_unary(Operation op)				{ return op == Operation::FNeg; }
inline bool			is_binary(Operation op)				{ return between(op, Operation::Add, Operation::FRem); }
inline bool			is_terminator(Operation op)			{ return is_any(op, Operation::Br, Operation::Unreachable, Operation::Switch, Operation::Ret); }

inline Operation	CastOperation(CastOpcodes opcode)	{ return Operation::Trunc + (int)opcode; }
inline CastOpcodes	CastOperation(Operation opcode)		{ return CastOpcodes(opcode - Operation::Trunc); }

inline Operation	UnaryOperation(UnaryOpcodes opcode)	{
	switch (opcode) {
		default: ISO_ASSERT(0);
		case UnaryOpcodes::FNeg:	return Operation::FNeg;
	}
}
inline UnaryOpcodes UnaryOperation(Operation opcode)	{
	switch (opcode) {
		default: ISO_ASSERT(0);
		case Operation::FNeg:		return UnaryOpcodes::FNeg;
	}
}

inline Operation	BinaryOperation(BinaryOpcodes opcode, bool fp)	{
	if (fp) {
		switch (opcode) {
			default: ISO_ASSERT(0);
			case BinaryOpcodes::Add:	return Operation::FAdd;
			case BinaryOpcodes::Sub:	return Operation::FSub;
			case BinaryOpcodes::Mul:	return Operation::FMul;
			case BinaryOpcodes::SDiv:	return Operation::FDiv;
			case BinaryOpcodes::SRem:	return Operation::FRem;
		}
	}
	return Operation::Add + (int)opcode;
}
inline BinaryOpcodes BinaryOperation(Operation opcode)	{
	switch (opcode) {
		default: return BinaryOpcodes(opcode - Operation::Add);
		case Operation::FAdd:	return BinaryOpcodes::Add;
		case Operation::FSub:	return BinaryOpcodes::Sub;
		case Operation::FMul:	return BinaryOpcodes::Mul;
		case Operation::FDiv:	return BinaryOpcodes::SDiv;
		case Operation::FRem:	return BinaryOpcodes::SRem;
	}
}

inline BinaryOpcodes FBinaryOperation(Operation opcode)	{
	switch (opcode) {
		default: ISO_ASSERT(0);
		case Operation::FAdd:	return BinaryOpcodes::Add;
		case Operation::FSub:	return BinaryOpcodes::Sub;
		case Operation::FMul:	return BinaryOpcodes::Mul;
		case Operation::FDiv:	return BinaryOpcodes::Div;
		case Operation::FRem:	return BinaryOpcodes::Rem;
	}
}

template<typename L, typename T> uint64 AtomicLambda(const Type *type, T val, void *dest, L&& lambda) {
	auto	orig	= type->load(dest);
	type->store(dest, lambda(Typed(type, orig), val));
	return orig;
}

template<typename E> auto EvaluateT(const Type* type, Operation op, uint32 flags, E &&exec) {
	switch (op) {
		default:
			ISO_ASSERT(0);
			
		// Cast operators
		case Operation::Trunc: 		return exec([mask = bits64(type->size)](uint64 a)	{ return a & mask; });
		case Operation::ZExt: 		return exec([mask = bits64(type->size)](uint64 a)	{ return a & mask; });
		case Operation::SExt: 		return exec([](Typed a)		{ return sign_extend(a.value, a.type->size); });
		case Operation::FToU:		return exec([](float a)		{ return uint64(a); });
		case Operation::FToS:		return exec([](float a)		{ return int64(a); });
		case Operation::UToF:		return exec([](uint64 a)	{ return float(a); });
		case Operation::SToF:		return exec([](int64 a)		{ return float(a); });
		case Operation::FPTrunc:	return exec([](float a)		{ return a; });
		case Operation::FPExt:		return exec([](float a)		{ return a; });
		
		// Nops
		case Operation::PtrToI:
		case Operation::IToPtr:
		case Operation::Bitcast:
		case Operation::AddrSpaceCast:
			return exec([type](Typed a) { return Typed(type, a.value); });

		// Unary operators
		case Operation::FNeg: 		return exec([](float a)				{ return -a; });

		// Binary operators
		case Operation::Add: 		return exec([](uint64 a, uint64 b)	{ return a + b; });
		case Operation::Sub:		return exec([](uint64 a, uint64 b)	{ return a - b; });
		case Operation::Mul:		return exec([](uint64 a, uint64 b)	{ return a * b; });
		case Operation::UDiv:		return exec([](uint64 a, uint64 b)	{ return a / b; });
		case Operation::SDiv:		return exec([](int64  a, int64  b)	{ return a / b; });
		case Operation::URem:		return exec([](uint64 a, uint64 b)	{ return a % b; });
		case Operation::SRem:		return exec([](int64  a, int64  b)	{ return a % b; });
		case Operation::Shl:		return exec([](uint64 a, uint64 b)	{ return a<< b; });
		case Operation::Lshr:		return exec([](uint64 a, uint64 b)	{ return a>> b; });
		case Operation::Ashr:		return exec([](int64  a, uint64 b)	{ return a>> b; });

		case Operation::And:
			return type->size == 1
				?	exec([](bool a, bool b)		{ return a && b; })
				:	exec([](uint64 a, uint64 b)	{ return a & b; });
		case Operation::Or:
			return type->size == 1
				?	exec([](bool a, bool b)		{ return a || b; })
				:	exec([](uint64 a, uint64 b)	{ return a | b; });
		case Operation::Xor:
			return type->size == 1
				?	exec([](bool a, bool b)		{ return a != b; })
				:	exec([](uint64 a, uint64 b)	{ return a ^ b; });

		case Operation::FAdd: 		return exec([](float a, float b)	{ return a + b; });
		case Operation::FSub:		return exec([](float a, float b)	{ return a - b; });
		case Operation::FMul:		return exec([](float a, float b)	{ return a * b; });
		case Operation::FDiv:		return exec([](float a, float b)	{ return a / b; });
		case Operation::FRem:		return exec([](float a, float b)	{ return mod(a, b); });

		// Memory operators
		//case Operation::Alloca:
		case Operation::LoadAtomic:
		case Operation::Load:				return exec([](TypedPointer a)	{ return a.load(); });
		//case Operation::StoreAtomic:
		case Operation::Store:				return exec([](TypedPointer a, Typed b)			{ a.store(b); return 0; });
//		case Operation::GetElementPtr:		return exec([](TypedPointer p, range<const Typed*> indices)	{ return p.index(indices[0]); });
		case Operation::GetElementPtr:		return exec([](TypedPointer p, uint64 index0, dynamic_array<uint64> indices)	{
			p = p.index(index0);
			for (uint64 i : indices)
				p = p.deindex(i);
			return p;
		});

		//case Operation::Fence:
		case Operation::CmpXchg:			return exec([type](void *dest, Typed cmp, Typed val) {
			auto	orig	= type->load(dest);
			bool	same	= orig == cmp.value;
			if (same)
				type->store(dest, val);
			return make_pair(move(orig), move(same));
		});
		case Operation::AtomicRMW:
			switch ((RMWOperations)flags) {
				default:
				case RMWOperations::Xchg:	return exec([type](void *dest, Typed  val)	{ return AtomicLambda(type, val, dest, [](Typed a,  Typed b)	{ return a.value; });	});
				case RMWOperations::Add:	return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return a + b; });		});
				case RMWOperations::Sub:	return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return a - b; });		});
				case RMWOperations::And:	return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return a & b; });		});
				case RMWOperations::Nand:	return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return ~(a & b); });	});
				case RMWOperations::Or:		return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return a | b; });		});
				case RMWOperations::Xor:	return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return a ^ b; });		});
				case RMWOperations::Max:	return exec([type](void *dest, int64  val)	{ return AtomicLambda(type, val, dest, [](int64 a,  int64 b)	{ return max(a, b); }); });
				case RMWOperations::Min:	return exec([type](void *dest, int64  val)	{ return AtomicLambda(type, val, dest, [](int64 a,  int64 b)	{ return min(a, b); }); });
				case RMWOperations::UMax:	return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return max(a, b); }); });
				case RMWOperations::UMin:	return exec([type](void *dest, uint64 val)	{ return AtomicLambda(type, val, dest, [](uint64 a, uint64 b)	{ return min(a, b); }); });
				case RMWOperations::FAdd:	return exec([type](void *dest, float  val)	{ return AtomicLambda(type, val, dest, [](float a, float b)		{ return a + b; });		});
				case RMWOperations::FSub:	return exec([type](void *dest, float  val)	{ return AtomicLambda(type, val, dest, [](float a, float b)		{ return a + b; });		});
			}


		// Other operators
		case Operation::FCmp:
			switch ((FPredicate)flags) {
				default:
				case FPredicate::never:	return exec([](float a, float b)	{ return false; });
				case FPredicate::oeq:	return exec([](float a, float b)	{ return a == b; });
				case FPredicate::ogt:	return exec([](float a, float b)	{ return a >  b; });
				case FPredicate::oge:	return exec([](float a, float b)	{ return a >= b; });
				case FPredicate::olt:	return exec([](float a, float b)	{ return a <  b; });
				case FPredicate::ole:	return exec([](float a, float b)	{ return a <= b; });
				case FPredicate::one:	return exec([](float a, float b)	{ return a != b; });
				case FPredicate::ord:	return exec([](float a, float b)	{ return (a == a) && (b == b); });
				case FPredicate::uno:	return exec([](float a, float b)	{ return (a != a) || (b != b); });
				case FPredicate::ueq:	return exec([](float a, float b)	{ return !(a != b); });
				case FPredicate::ugt:	return exec([](float a, float b)	{ return !(a <= b); });
				case FPredicate::uge:	return exec([](float a, float b)	{ return !(a <  b); });
				case FPredicate::ult:	return exec([](float a, float b)	{ return !(a >= b); });
				case FPredicate::ule:	return exec([](float a, float b)	{ return !(a >  b); });
				case FPredicate::une:	return exec([](float a, float b)	{ return !(a == b); });
				case FPredicate::always:return exec([](float a, float b)	{ return true; });
			}

		case Operation::ICmp:
			switch ((IPredicate)flags) {
				default:
				case IPredicate::eq:	return exec([](uint64 a, uint64 b)	{ return a == b; });
				case IPredicate::ne:	return exec([](uint64 a, uint64 b)	{ return a != b; });
				case IPredicate::ugt:	return exec([](uint64 a, uint64 b)	{ return a >  b; });
				case IPredicate::uge:	return exec([](uint64 a, uint64 b)	{ return a >= b; });
				case IPredicate::ult:	return exec([](uint64 a, uint64 b)	{ return a <  b; });
				case IPredicate::ule:	return exec([](uint64 a, uint64 b)	{ return a <= b; });
				case IPredicate::sgt:	return exec([](int64 a, int64 b)	{ return a >  b; });
				case IPredicate::sge:	return exec([](int64 a, int64 b)	{ return a >= b; });
				case IPredicate::slt:	return exec([](int64 a, int64 b)	{ return a <  b; });
				case IPredicate::sle:	return exec([](int64 a, int64 b)	{ return a <= b; });
			}

		//case Operation::Phi:
		//case Operation::Call:
		case Operation::Select:			return exec([](Typed a, Typed b, bool c)	{ return select(c, a, b); });
		//case Operation::UserOp1:
		//case Operation::UserOp2:
		//case Operation::VAArg:
		case Operation::ExtractElement:	return exec([](TypedPointer a, int b)			{ return a.index(b); });
		//case Operation::InsertElement:
		//case Operation::ShuffleVector:
		//case Operation::ExtractValue:
		//case Operation::InsertValue:
		//case Operation::LandingPad:
	}
}

inline uint64	Evaluate(const Type* type, Operation op, uint32 flags, range<const Typed*> args) {
	return EvaluateT(type, op, flags, [&args, type](auto &&lambda) {
		return type->set(call_array(lambda, args));
	});

}
inline uint64	Evaluate(const Type* type, Operation op, uint32 flags, initializer_list<const Typed> args) {
	return Evaluate(type, op, flags, make_range(args));
}

struct Eval : ValueP<Eval> {
	Operation	op;
	uint32		flags;
	Value		arg;
	Eval(Operation op, uint32 flags, const Value &arg) : op(op), flags(flags), arg(arg) {}

	decltype(auto)	args()			const { return arg.get_known<ValueArray>(); }
	ValueArray		args_always()	const { if (arg.is<ValueArray>()) return args(); return {arg}; }

	uint64	Evaluate(const Type *type) const {
		if (!arg.is<ValueArray>())
			return bitcode::Evaluate(type, op, flags, make_range({Typed(arg)}));
		return bitcode::Evaluate(type, op, flags, dynamic_array<const Typed>(args()));
	}

	template<typename E> auto Evaluate(const Type* type, E &&exec) const {
		return EvaluateT(type, op, flags, [this, exec](auto &&lambda) {
			exec(lambda, arg);
		});
	}

	ConstantsCodes	get_code() const {
		if (is_cast(op))
			return CST_CODE_CE_CAST;
		if (is_unary(op))
			return CST_CODE_CE_UNOP;
		if (is_binary(op))
			return CST_CODE_CE_BINOP;
		switch (op) {
			case Operation::GetElementPtr:	return flags & 1 ? CST_CODE_CE_INBOUNDS_GEP : CST_CODE_CE_GEP;
			case Operation::Select:			return CST_CODE_CE_SELECT;
			case Operation::ExtractElement:	return CST_CODE_CE_EXTRACTELT;
			case Operation::InsertElement:	return CST_CODE_CE_INSERTELT;
			case Operation::ShuffleVector:	return CST_CODE_CE_SHUFFLEVEC;
			case Operation::ICmp:
			case Operation::FCmp:			return CST_CODE_CE_CMP;
			default:						ISO_ASSERT(0); return ConstantsCodes(0);
		}
	}
};

//-----------------------------------------------------------------------------
// Function
//-----------------------------------------------------------------------------

struct Instruction : ValueP<Instruction> {
	Operation	op;
	union {
		uint32		flags = 0;
		struct {
			union {
				uint8			predicate;
				FPredicate		fpredicate;
				IPredicate		ipredicate;
				RMWOperations	rmw;
				uint8			call_tail;	//1 - tail, 3 - must tail
			};
			union {
				uint8			optimisation;
				FastMathFlags	float_flags;
				OverflowFlags	int_flags;
				ExactFlags		exact_flags;
				struct {		// atomic flags
					AtomicOrderingCodes	success:3, failure:3;
					bool		Volatile:1, SyncScope:1;
					bool		InBounds:1, Weak:1;
				};
				bool			alloca_argument:1;	// this alloca is used to represent the arguments to a call
			};
		};
	};

	string				name;
	uint32				id			= ~0U;
	uint32				align		= 0;
	const DILocation*	debug_loc	= nullptr;
	const Type*			type		= nullptr;
	ValueArray			args;
	dynamic_array<AttachedMetadata>	attachedMeta;

	// function calls
	AttributeSet		paramAttrs;
	const Function*		funcCall	= nullptr;

	Instruction(Operation op = Operation::Nop) : op(op) {}
	Instruction(Operation op, const Type *type) : op(op), type(type) {}
};

struct InstructionRef {
	Instruction		*i;
	InstructionRef(const InstructionRef&) = default;
	InstructionRef(InstructionRef&&) = default;
	InstructionRef(InstructionRef&) = default;
	template<typename...P>InstructionRef(P&&...p)	: i(new Instruction(forward<P>(p)...)) {}
	operator Instruction*()	const { return i; }
	auto	operator->()	const { return i; }
};

struct Block : ValueP<Block> {
	string			name;
	uint32			id		= ~0U;
	const InstructionRef*	first;
	dynamic_array<const Instruction*>	live;
	dynamic_array<const Block*>			preds;
};

struct UselistEntry {
	Value					value;
	dynamic_array<uint64>	shuffle;
	UselistEntry(const Value &value, dynamic_array<uint64> &shuffle) : value(value), shuffle(shuffle) {}
};

struct Function : ValueP<Function> {
	union {
		uint32	flags	= 0;
		struct {
			Linkage			linkage			: 5;
			Visibility		visibility		: 2;
			DLLStorageClass dll_storage		: 2;
			UnnamedAddr		unnamed_addr	: 2;
			CallingConv		calling_conv	: 8;
			bool			prototype		: 1;
		};
	};

	string			name;
	const Type*		type;
	Value			prolog_data;
	Value			prefix_data;
	Value			personality_function;
	const char*		section		= nullptr;
	const Comdat*	comdat		= nullptr;
	const char*		gc			= nullptr;

	dynamic_array<Instruction>		args;
	dynamic_array<InstructionRef>	instructions;
	dynamic_array<UselistEntry>		uselist;
	dynamic_array<AttachedMetadata>	attachedMeta;
	dynamic_array<Block>			blocks;
	AttributeSet					attrs;
	uint64							align		= 0;
	uint32							line_offset	= 0;

	Function(const Type *type) : type(type) {}

	void	GetLive()			const;

	const Instruction* InstructionByID(int i) const {
		auto	p = instructions.begin() + (i - 1);
		while (!~(*p)->id || (*p)->id < i)
			++p;
		return p->i;
	}
	auto	Instructions(const Block *block) const {
		return make_range(
			block->first,
			block == &blocks.back() ? instructions.end() : block[1].first
			//instructions.get_iterator(block->first),
			//block == &blocks.back() ? instructions.end() : instructions.get_iterator(block[1].first)
		);
	}
	dynamic_array<const Block*> Successors(const Block *block) const;
};

//-----------------------------------------------------------------------------
//	Module
//-----------------------------------------------------------------------------

struct Module {
	enum {
		MAGIC	= 0xDEC04342,
		VERSION	= 1,
	};

	arena_allocator<1024>			allocator;

	int								version;
	string							triple;
	string							datalayout;
	string							assembly;
	dynamic_array<NamedMetadata>	named_meta;
	dynamic_array<UselistEntry>		uselist;

	// for quick enumeration
	dynamic_array<GlobalVar*>		globalvars;
	dynamic_array<Function*>		functions;

	range<const MetadataRef*> GetMeta(const char *name) const {
		for (auto& i : named_meta) {
			if (i.name == name)
				return i;
		}
		return none;
	}
	MetadataRef GetMetaFirst(const char *name) const {
		if (auto m = GetMeta(name))
			return m[0];
		return nullptr;
	}

	bool	read(BlockReader &&reader);
	bool	write(BlockWriter &&writer) const;

	bool	read(istream_ref file) {
		uint32 magic = file.get<uint32>();
		if (magic != MAGIC)
			return false;

		vlc			v(file);
		BlockReader	r(v);
		uint32		code;
		return r.nextRecord(code) == bitcode::ENTER_SUBBLOCK && code == bitcode::MODULE_BLOCK_ID && read(r.enterBlock(nullptr));
	}

	bool	write(ostream_ref file) const {
		file.write(bitcode::Module::MAGIC);
		wvlc	v(file);
		return write(BlockWriter(v).enterBlock(MODULE_BLOCK_ID, 3));
	}
};

//-----------------------------------------------------------------------------
//	Debug Info
//-----------------------------------------------------------------------------

struct DIScoped {
	MetadataRef			scope;
	MetadataRef			file;
	uint64				line;
};

struct DIExternal : DIScoped {
	MetaString			name;
	MetaString			linkage_name;
	MetadataRef			type;
	bool				local;
	bool				definition;
	template<typename R>	bool read(R&& r)		{ return r.read (scope, name, linkage_name, file, line, type, local, definition); }
	template<typename W>	bool write(W&& w) const { return w.write(scope, name, linkage_name, file, line, type, local, definition); }
};

struct DITag {
	dwarf::TAG			tag;
	MetaString			name;
	template<typename R>	bool read(R&& r)		{ return r.read (tag, name); }
	template<typename W>	bool write(W&& w) const { return w.write(tag, name); }
};

struct DILocation : DebugInfoT<METADATA_LOCATION> {
	uint64			line	= 0;
	uint64			col		= 0;
	MetadataRef1	scope;
	MetadataRef		inlined_at;

	DILocation() {}
	DILocation(uint64 line, uint64 col, const Metadata *scope, const Metadata *inlined_at) : line(line), col(col), scope(scope), inlined_at(inlined_at) {}

	filename	get_filename()	const;
	bool		IsInScope(const DebugInfo *b) const;

	template<typename R>	bool read(R&& r)		{ return r.read(line, col, scope, inlined_at); }
	template<typename W>	bool write(W&& w) const	{ return w.write(line, col, scope, inlined_at); }
};

struct DIFile : public DebugInfoT<METADATA_FILE> {
	MetaString		file;
	MetaString		dir;
	template<typename R>	bool read(R&& r)		{ return r.read (file, dir); }
	template<typename W>	bool write(W&& w) const { return w.write(file, dir); }
};

struct DICompileUnit : public DebugInfoT<METADATA_COMPILE_UNIT> {
	dwarf::LANG			lang;
	MetadataRef			file;
	MetaString			producer;
	bool				optimised;
	MetaString			flags;
	uint64				runtime_version;
	MetaString			split_debug_filename;
	uint64				emission_kind;
	MetadataRef			enums;
	MetadataRef			retained_types;
	MetadataRef			subprograms;
	MetadataRef			globals;
	MetadataRef			imports;
	template<typename R>	bool read(R&& r)		{ return r.read (lang, file, producer, optimised, flags, runtime_version, split_debug_filename, emission_kind, enums, retained_types, subprograms, globals, imports); }
	template<typename W>	bool write(W&& w) const { return w.write(lang, file, producer, optimised, flags, runtime_version, split_debug_filename, emission_kind, enums, retained_types, subprograms, globals, imports); }
};

struct DIBasicType : public DebugInfoT<METADATA_BASIC_TYPE>, DITag {
	uint64				size_bits;
	uint64				align_bits;
	dwarf::ATE			encoding;
	template<typename R>	bool read(R&& r)		{ return DITag::read(r)  && r.read (size_bits, align_bits, encoding); }
	template<typename W>	bool write(W&& w) const { return DITag::write(w) && w.write(size_bits, align_bits, encoding); }
};

struct DITypeBase : public DITag, DIScoped {
	MetadataRef			base;
	uint64				size_bits;
	uint64				align_bits;
	uint64				offset_bits;
	DebugInfo::Flags	flags;
	template<typename R>	bool read(R&& r)		{ return DITag::read(r)  && r.read (file, line, scope, base, size_bits, align_bits, offset_bits, flags); }
	template<typename W>	bool write(W&& w) const { return DITag::write(w) && w.write(file, line, scope, base, size_bits, align_bits, offset_bits, flags); }
};

struct DIDerivedType : public DebugInfoT<METADATA_DERIVED_TYPE>, DITypeBase {
	MetadataRef			extra;
	template<typename R>	bool read(R&& r)		{ return DITypeBase::read(r)  && r.read (extra); }
	template<typename W>	bool write(W&& w) const { return DITypeBase::write(w) && w.write(extra); }
};

struct DICompositeType : public DebugInfoT<METADATA_COMPOSITE_TYPE>, DITypeBase {
	MetadataRef			elements;
	uint64				runtime_lang;
	MetadataRef			vtable;
	MetadataRef			template_params;
	MetaString			identifier;
	template<typename R>	bool read(R&& r)		{ return DITypeBase::read(r)  && r.read (elements, runtime_lang, vtable, template_params, identifier); }
	template<typename W>	bool write(W&& w) const { return DITypeBase::write(w) && w.write(elements, runtime_lang, vtable, template_params, identifier); }
};

struct DITemplateValueParameter : public DebugInfoT<METADATA_TEMPLATE_VALUE>, DITag {
	MetadataRef			type;
	MetadataRef			value;
	template<typename R>	bool read(R&& r)		{ return DITag::read(r)  && r.read (type, value); }
	template<typename W>	bool write(W&& w) const { return DITag::write(w) && w.write(type, value); }
};

struct DITemplateTypeParameter : public DebugInfoT<METADATA_TEMPLATE_TYPE> {
	MetaString			name;
	MetadataRef			type;
	template<typename R>	bool read(R&& r)		{ return r.read (name, type); }
	template<typename W>	bool write(W&& w) const { return w.write(name, type); }
};

struct DISubprogram : public DebugInfoT<METADATA_SUBPROGRAM>, DIExternal {
	uint64				scopeLine;
	MetadataRef			containing_type;
	dwarf::VIRTUALITY	virtuality;
	uint64				virtualIndex;
	Flags				flags;
	bool				optimised;
	MetadataRef			function;
	MetadataRef			template_params;
	MetadataRef			declaration;
	MetadataRef			variables;

	template<typename R>	bool read(R&& r)		{ return DIExternal::read(r)  && r.read (scopeLine, containing_type, virtuality, virtualIndex, flags, optimised, function, template_params, declaration, variables); }
	template<typename W>	bool write(W&& w) const { return DIExternal::write(w) && w.write(scopeLine, containing_type, virtuality, virtualIndex, flags, optimised, function, template_params, declaration, variables); }
};

struct DILexicalBlock : public DebugInfoT<METADATA_LEXICAL_BLOCK>, DIScoped {
	uint64				column;
	template<typename R>	bool read(R&& r)		{ return r.read(scope, file, line, column); }
	template<typename W>	bool write(W&& w) const { return w.write(scope, file, line, column); }
};

struct DIGlobalVariable : public DebugInfoT<METADATA_GLOBAL_VAR>, DIExternal {
	MetadataRef			variable;
	MetadataRef			static_data;
	template<typename R>	bool read(R&& r)		{ return DIExternal::read(r)  && r.read (variable, static_data); }
	template<typename W>	bool write(W&& w) const { return DIExternal::write(w) && w.write(variable, static_data); }
};

struct DILocalVariable : public DebugInfoT<METADATA_LOCAL_VAR>, DIScoped, DITag {
	MetadataRef			type;
	uint64				arg;
	Flags				flags;
	uint64				align_bits;
	template<typename R>	bool read(R&& r)		{ return r.read(tag, scope, name, file, line, type, arg, flags, align_bits); }
	template<typename W>	bool write(W&& w) const { return w.write(tag, scope, name, file, line, type, arg, flags, align_bits); }
};

struct DIExpression : public DebugInfoT<METADATA_EXPRESSION> {
	dynamic_array<uint64>	expr;
	template<typename R>	bool read(R&& r)		{ return expr.read(r, r.remaining()); }
	template<typename W>	bool write(W&& w) const { return w.write(expr); }
	uint64	Evaluate(uint64 v, uint32 &bit_size) const;
};

struct DISubroutineType : public DebugInfoT<METADATA_SUBROUTINE_TYPE> {
	MetadataRef			types;
	template<typename R>	bool read(R&& r)		{ uint64 dummy; return r.read(dummy, types); }
	template<typename W>	bool write(W&& w) const { return w.write(uint64(0), types); }
};

struct DISubrange : public DebugInfoT<METADATA_SUBRANGE> {
	uint64				count;
	int64				lower_bound;
	template<typename R>	bool read(R&& r)		{ return r.read(count, lower_bound); }
	template<typename W>	bool write(W&& w) const { return w.write(count, lower_bound); }
};

// extra ones:

struct DIGenericDebug : public DebugInfoT<METADATA_GENERIC_DEBUG>  {// [distinct, tag, vers, header, n x md num]
	dwarf::TAG			tag;
	MetadataRef			vers;
	MetadataRef			header;
	dynamic_array<uint64>	nums;
	template<typename R>	bool read(R&& r)		{ return r.read(tag, vers, header) && nums.read(r, r.remaining()); }
	template<typename W>	bool write(W&& w) const { return w.write(tag, vers, header, nums); }
};
struct DIEnumerator : public DebugInfoT<METADATA_ENUMERATOR> {// [isUnsigned|distinct, value, name]
	int64				value;
	MetaString			name;
	template<typename R>	bool read(R&& r)		{ return r.read(value, name); }
	template<typename W>	bool write(W&& w) const { return w.write(value, name); }
};
struct DILexicalBlockFile : public DebugInfoT<METADATA_LEXICAL_BLOCK_FILE> {// [distinct, scope, file, discriminator]
	MetadataRef			scope;
	MetadataRef			file;
	MetadataRef			discriminator;
	template<typename R>	bool read(R&& r)		{ return r.read(scope, file, discriminator); }
	template<typename W>	bool write(W&& w) const { return w.write(scope, file, discriminator); }
};
struct DINamespace : public DebugInfoT<METADATA_NAMESPACE>, DIScoped {// [distinct, scope, file, name, line, exportSymbols]
	MetaString			name;
	MetadataRef			exportSymbols;
	template<typename R>	bool read(R&& r)		{ return r.read(scope, file, name, line, exportSymbols); }
	template<typename W>	bool write(W&& w) const { return w.write(scope, file, name, line, exportSymbols); }
};
struct DIObjcProperty : public DebugInfoT<METADATA_OBJC_PROPERTY> {// [distinct, name, file, line, ...]
	MetaString			name;
	MetadataRef			file;
	uint64				line;
	dynamic_array<MetadataRef>	extra;
	template<typename R>	bool read(R&& r)		{ return r.read(name, file, line) && extra.read(r, r.remaining()); }
	template<typename W>	bool write(W&& w) const { return w.write(name, file, line, extra); }
};
struct DIImportedEntity : public DebugInfoT<METADATA_IMPORTED_ENTITY>, DIScoped, DITag {// [distinct, tag, scope, entity, line, name]
	template<typename R>	bool read(R&& r)		{ return r.read(tag, scope, file, line, name); }
	template<typename W>	bool write(W&& w) const { return w.write(tag, scope, file, line, name); }
};
struct DIModule : public DebugInfoT<METADATA_MODULE> {// [distinct, scope, name, ...]
	MetadataRef			scope;
	MetaString			name;
	dynamic_array<MetadataRef>	extra;
	template<typename R>	bool read(R&& r)		{ return r.read(scope, name); }
	template<typename W>	bool write(W&& w) const { return w.write(scope, name); }
};
struct DIMacro : public DebugInfoT<METADATA_MACRO> {// [distinct, macinfo, line, name, value]
	MetadataRef			macinfo;
	uint64				line;
	MetaString			name;
	MetadataRef			value;
	template<typename R>	bool read(R&& r)		{ return r.read(macinfo, line, name, value); }
	template<typename W>	bool write(W&& w) const { return w.write(macinfo, line, name, value); }
};
struct DIMacroFile : public DebugInfoT<METADATA_MACRO_FILE> {// [distinct, macinfo, line, file, ...]
	MetadataRef			macinfo;
	uint64				line;
	MetadataRef			file;
	dynamic_array<MetadataRef>	extra;
	template<typename R>	bool read(R&& r)		{ return r.read(macinfo, line, file) && extra.read(r, r.remaining()); }
	template<typename W>	bool write(W&& w) const { return w.write(macinfo, line, file, extra); }
};

struct DIGlobalVariableExpression : public DebugInfoT<METADATA_GLOBAL_VAR_EXPR> {// [distinct, var, expr]
	MetadataRef			var;
	MetadataRef			expr;
	template<typename R>	bool read(R&& r)		{ return r.read(var, expr); }
	template<typename W>	bool write(W&& w) const { return w.write(var, expr); }
};

struct DILabel : public DebugInfoT<METADATA_LABEL>, DIScoped {// [distinct, scope, name, file, line]
	MetaString			name;
	template<typename R>	bool read(R&& r)		{ return r.read(scope, name, file, line); }
	template<typename W>	bool write(W&& w) const { return w.write(scope, name, file, line); }
};
struct DIStringType : public DebugInfoT<METADATA_STRING_TYPE> {// [distinct, name, size, align,...]
	MetaString			name;
	uint64				size, align;
	dynamic_array<MetadataRef>	extra;
	template<typename R>	bool read(R&& r)		{ return r.read(name, size, align) && extra.read(r, r.remaining()); }
	template<typename W>	bool write(W&& w) const { return w.write(name, size, align, extra); }
};
struct DICommonBlock : public DebugInfoT<METADATA_COMMON_BLOCK> {// [distinct, scope, name, variable,...]
	MetadataRef			scope;
	MetaString			name;
	MetadataRef			variable;
	dynamic_array<MetadataRef>	extra;
	template<typename R>	bool read(R&& r)		{ return r.read(scope, name, variable) && extra.read(r, r.remaining()); }
	template<typename W>	bool write(W&& w) const { return w.write(scope, name, variable, extra); }
};
struct DIGenericSubrange : public DebugInfoT<METADATA_GENERIC_SUBRANGE> {// [distinct, count, lo, up, stride]
	uint64	count, lo, up, stride;
	template<typename R>	bool read(R&& r)		{ return r.read(count, lo, up, stride); }
	template<typename W>	bool write(W&& w) const { return w.write(count, lo, up, stride); }
};


//-----------------------------------------------------------------------------
// DebugInfo dispatch function
//-----------------------------------------------------------------------------

template<typename R, typename P> R process(const DebugInfo *d, P &&proc) {
	switch (d->kind) {
	#if 1
		case METADATA_LOCATION:				return proc(d->as<DILocation>());
		case METADATA_BASIC_TYPE:			return proc(d->as<DIBasicType>());
		case METADATA_FILE:					return proc(d->as<DIFile>());
		case METADATA_DERIVED_TYPE:			return proc(d->as<DIDerivedType>());
		case METADATA_COMPOSITE_TYPE:		return proc(d->as<DICompositeType>());
		case METADATA_SUBROUTINE_TYPE:		return proc(d->as<DISubroutineType>());
		case METADATA_COMPILE_UNIT:			return proc(d->as<DICompileUnit>());
		case METADATA_SUBPROGRAM:			return proc(d->as<DISubprogram>());
		case METADATA_LEXICAL_BLOCK:		return proc(d->as<DILexicalBlock>());
		case METADATA_TEMPLATE_TYPE:		return proc(d->as<DITemplateTypeParameter>());
		case METADATA_TEMPLATE_VALUE:		return proc(d->as<DITemplateValueParameter>());
		case METADATA_GLOBAL_VAR:			return proc(d->as<DIGlobalVariable>());
		case METADATA_LOCAL_VAR:			return proc(d->as<DILocalVariable>());
		case METADATA_EXPRESSION:			return proc(d->as<DIExpression>());
		case METADATA_SUBRANGE:				return proc(d->as<DISubrange>());
		case METADATA_LEXICAL_BLOCK_FILE:	return proc(d->as<DILexicalBlockFile>());
		case METADATA_NAMESPACE:			return proc(d->as<DINamespace>());
		case METADATA_GENERIC_DEBUG:		return proc(d->as<DIGenericDebug>());
		case METADATA_ENUMERATOR:			return proc(d->as<DIEnumerator>());
		case METADATA_OBJC_PROPERTY:		return proc(d->as<DIObjcProperty>());
		case METADATA_IMPORTED_ENTITY:		return proc(d->as<DIImportedEntity>());
		case METADATA_MODULE:				return proc(d->as<DIModule>());
		case METADATA_MACRO:				return proc(d->as<DIMacro>());
		case METADATA_MACRO_FILE:			return proc(d->as<DIMacroFile>());
		case METADATA_GLOBAL_VAR_EXPR:		return proc(d->as<DIGlobalVariableExpression>());
		case METADATA_LABEL:				return proc(d->as<DILabel>());
		case METADATA_STRING_TYPE:			return proc(d->as<DIStringType>());
		case METADATA_COMMON_BLOCK:			return proc(d->as<DICommonBlock>());
		case METADATA_GENERIC_SUBRANGE:		return proc(d->as<DIGenericSubrange>());
		#else
		case METADATA_FILE:				return proc(d->as<DIFile>());
		case METADATA_COMPILE_UNIT: 	return proc(d->as<DICompileUnit>());
		case METADATA_BASIC_TYPE:		return proc(d->as<DIBasicType>());
		case METADATA_DERIVED_TYPE:		return proc(d->as<DIDerivedType>());
		case METADATA_COMPOSITE_TYPE:	return proc(d->as<DICompositeType>());
		case METADATA_TEMPLATE_TYPE:	return proc(d->as<DITemplateTypeParameter>());
		case METADATA_TEMPLATE_VALUE:	return proc(d->as<DITemplateValueParameter>());
		case METADATA_SUBPROGRAM:		return proc(d->as<DISubprogram>());
		case METADATA_SUBROUTINE_TYPE:	return proc(d->as<DISubroutineType>());
		case METADATA_GLOBAL_VAR: 		return proc(d->as<DIGlobalVariable>());
		case METADATA_LOCAL_VAR:		return proc(d->as<DILocalVariable>());
		case METADATA_LEXICAL_BLOCK:	return proc(d->as<DILexicalBlock>());
		case METADATA_SUBRANGE:			return proc(d->as<DISubrange>());
		case METADATA_EXPRESSION:		return proc(d->as<DIExpression>());
		case METADATA_LOCATION: 		return proc(d->as<DILocation>());
		#endif
		default:						return proc(d);
	}
}

} //namespace bitcode

#endif // BITCODE_H
