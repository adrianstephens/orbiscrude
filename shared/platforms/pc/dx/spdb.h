#ifndef SPDB_H
#define SPDB_H

#include "dx/dx_shaders.h"
#include "base/array.h"
#include "base/tree.h"
#include "base/strings.h"
#include "base/algorithm.h"
#include "disassembler.h"

#include "filetypes/code/pdb.h"

namespace iso {

//-----------------------------------------------------------------------------
//	SDBG
//-----------------------------------------------------------------------------

struct SDBGDummy {};

struct SDBGHeader {
	template<typename T> struct ptr32 : offset_pointer<T, uint32, SDBGDummy> {};
	template<typename T> struct array {
		uint32		num;
		ptr32<T>	ptr;
		range<const T*>	get(const SDBGDummy *base) const { return range<const T*>(ptr.get(base), ptr.get(base) + num); }
	};

	struct FileHeader {
		offset_pointer<char, uint32>	filename;		// offset into the ascii Database where the filename sits.
		int32							filenameLen;	// filename path. Absolute for root file, relative for other headers
		offset_pointer<char, uint32>	source;			// offset into the ascii Database where this file's source lives
		int32							sourceLen;		// bytes in source file. Valid for all file headers
	};

	// Partly understood, many unknown/guessed elements. Completely understood how this fits in in the overall structure Details of each assembly instruction
	struct AsmInstruction {
		int32	instructionNum;

		uint32	opCode;

		int32	unknown_a[2];
		int32	destRegister;
		int32	unknown_b;

		int32	destXMask;			// 00 if writing to this component in dest register, -1 if not writing
		int32	destYMask;			// 01		"				"				"				"
		int32	destZMask;			// 02		"				"				"				"
		int32	destWMask;			// 03		"				"				"				"

		struct Component {
			int32 varID;			// matches SDBGVariable below
			float lowBounds[2];		// what's this? defaults	0.0 to -QNAN. Some kind of bound.
			float highBounds[2];	// what's this?			 -0.0 to	QNAN. Some kind of bound.
			float minBound;			// min value this components's dest can be
			float maxBound;			// max value	"			"			"
			int32 unknown_a[2];
		} component[4];

		int32 unknown_c[9];

		// I don't know what this is, but it's 9 int32s and 4 of them, so sounds like something that's per-component
		struct Something {
			int32 unknown[9];
		} somethings[4];

		int32 unknown_d[2];

		int32 symbol;				// symbol, usually virtual I think, that links this instruction to somewhere in hlsl - e.g. a line number and such

		int32 callstackDepth;		// 0-indexed current level of the callstack. ie. 0 is in the main function, 1 is in a sub-function, etc etc.

		array<void> scopes;			// The scopeIDs that show the call trace in each instruction (or rather, where this instruction takes place).
		array<void> varTypes;		// The Type IDs of variables involved in this instruction. Possibly in source,source,dest order but/ maybe not.
	};

	// Mostly understood, a couple of unknown elements and/or not sure how it fits together in the grand scheme
	struct Variable {
		int32	symbolID;			// Symbol this assignment depends on
		uint32	type;
		int32	unknown[2];
		int32	typeID;				// refers to SDBGType. -1 if a constant
		union {
			int32 component;		// x=0,y=1,z=2,w=3
			float value;			// const value
		};
	};

	// Mostly understood, a couple of unknown elements and/or not sure how it fits together in the grand scheme
	struct InputRegister {
		int32 varID;
		int32 type;					// 2 = from cbuffer, 0 = from input signature, 6 = from texture, 7 = from sampler
		int32 cbuffer_register;		// -1 if input signature
		int32 cbuffer_packoffset;	// index of input signature
		int32 component;			// x=0,y=1,z=2,w=3
		int32 initValue;			// I think this is a value? -1 or some value. Or maybe an index.
	};

	struct Symbol {
		int32 fileID;				// index into SDBGFileHeader array
		int32 lineNum;
		int32 characterNum;			// not column, so after a tab would just be 1.
		array<void> symbol;			// offset can be 0 for 'virtual' symbols
	};

	struct Scope {
		int32 type;					// what kind of type I have no idea. 0 = Globals, 1 = Locals, 3 = Structure, 4 = Function
		int32 symbolNameOffset;		// offset from start of ascii Database
		int32 symbolNameLength;
		array<void> scopeTree;
	};

	struct Type {
		int32 symbolID;
		int32 isFunction;			// 0 / 1
		int32 type;					// 0 == scalar, 1 == dynamic_array, 3 == matrix, 4 == texture/sampler
		int32 typeNumRows;			// number of floats in the height of the base type (mostly for matrices)
		int32 typeNumColumns;		// number of floats in the width of the base type. 0 for functions or structure types
		int32 scopeID;				// if type is a complex type (including function return type), the scope of this type.
		int32 arrayDimension;		// 0, 1, 2, ...
		int32 arrayLenOffset;		// offset into the int32 database. Contains an array length for each dimension
		int32 stridesOffset;		// offset into the int32 database. Contains the stride for that level, for each dimension.
		// so with array[a][b][c] it has b*c*baseSize, then c*baseSize then baseSize
		int32 numFloats;			// number of floats in this type (or maybe 32bit words, not sure).
		int32 varID;				// Variable ID, or -1 if this variable isn't used.
	};
	
	int32 header_size;

	int32 compilerSigOffset;	// offset from asciiOffset at the end of this structure.
	int32 entryFuncOffset;		// offset from asciiOffset at the end of this structure.
	int32 profileOffset;		// offset from asciiOffset at the end of this structure.

	uint32 shaderFlags;			// Shader flags - same as from reflection.

	// All offsets are after this header.
	array<FileHeader>		files;			// total unique files opened and used via #include
	array<AsmInstruction>	instructions;	// assembly instructions
	array<Variable>			variables;		// Looks to be the variables (one per component) used in the shader
	array<InputRegister>	inputs;			// This lists which bits of which inputs are used - e.g. the components in input signature elements and cbuffers.
	array<Symbol>			symbols;		// This is a symbol table definitely, also includes 'virtual' symbols to match up ASM instructions to lines.
	array<Scope>			scopes;			// These are scopes - like for structures/functions. Also Globals/Locals lists of variables in scope for reference in ASM instructions
	array<Type>				types;			// Type specifications

	ptr32<int32>			int32s;	// offset after this header. Same principle as ASCII db, but for int32s
	ptr32<char>				ascii;	// offset after this header to the ASCII data. This is a general "ascii database section"
};

struct SDBG : SDBGHeader, SDBGDummy {
	enum { code = 0x47424453 };	//	SDBG
};

//-----------------------------------------------------------------------------
//	ParsedSPDB
//-----------------------------------------------------------------------------
/*
struct DXShaderDebug {
	struct File {;
		string	name;
		string	source;
		File(string &&_name, string &&_source) : name(_name), source(move(_source)) {}
		File(string &&_name, const memory_block &_source) : name(_name), source(str((const char*)_source, _source.length())) {}
	};
	struct Location {
		uint32	offset;
		uint32	file, line;
		Location(uint32 _offset, uint32 _file, uint32 _line) : offset(_offset), file(_file), line(_line) {}
		operator uint32() const { return offset; }
		bool operator==(const Location &b) const { return file == b.file && line == b.line; }
		bool operator!=(const Location &b) const { return !operator==(b); }
	};
	dynamic_array<File>			files;
	dynamic_array<Location>		locations;
};
*/
struct ParsedSPDB : PDB {
	uint32					flags;
	Disassembler::Files		files;
	Disassembler::Locations	locations;
	string					compiler, profile, entry, defines;

	ParsedSPDB(istream_ref file, const char *path = 0);
	CV::DATASYMHLSL	*FindUniform(uint16 type, uint16 slot);
};

struct Uniforms {
	dynamic_array<CV::DATASYMHLSL*> smp, srv, uav;
	dynamic_array<dynamic_array<CV::DATASYMHLSL*>> cbs;
	Uniforms(const PDB &pdb);
};

struct ParsedSDBGC {
	uint32					flags;
	Disassembler::Files		files;
	Disassembler::Locations	locations;
	string					compiler, profile, entry;

	dynamic_array<SDBG::AsmInstruction>		instructions;
	dynamic_array<SDBG::Variable>			variables;
	dynamic_array<SDBG::InputRegister>		inputs;			
	dynamic_array<SDBG::Symbol>				symbols;		
	dynamic_array<SDBG::Scope>				scopes;			
	dynamic_array<SDBG::Type>				types;			
	dynamic_array<int32>					int32s;
											
	ParsedSDBGC(const SDBG *sdbg);
	void GetFileLineFromIndex(uint32 instruction, int32 &file, int32 &line) const;
	void GetFileLineFromOffset(uint32 offset, int32 &file, int32 &line) const;
};

string GetPDBFirstFilename(istream_ref file);


}	// namespace iso

#endif	//	SPDB_H
