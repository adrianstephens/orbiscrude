#ifndef COFF_H
#define COFF_H

#include "base/defs.h"

//-----------------------------------------------------------------------------
//	Common Object File Format
//-----------------------------------------------------------------------------


namespace coff {
typedef	iso::packed_types<iso::littleendian_types>	packed;

struct TIMEDATE {
//	iso::uint32	secs2:5, mins:6, hours:5, day:5, month:4, year1980:7;
	iso::uint32	secs_from1970;
};

enum MACHINE {
	MACHINE_UNKNOWN			= 0x0000,	//The contents of this field are assumed to be applicable to any machine type
	MACHINE_AM33			= 0x01d3,	//Matsushita AM33
	MACHINE_AMD64			= 0x8664,	//x64
	MACHINE_ARM				= 0x01c0,	//ARM little endian
	MACHINE_THUMB			= 0x01c2,	//Thumb
	MACHINE_ARM64			= 0xaa64,	//ARM64 little endian
	MACHINE_ARMNT			= 0x01c4,	//ARM Thumb-2 little endian
	MACHINE_EBC				= 0x0ebc,	//EFI byte code
	MACHINE_I386			= 0x014c,	//Intel 386 or later processors and compatible processors
	MACHINE_IA64			= 0x0200,	//Intel Itanium processor family
	MACHINE_M32R			= 0x9041,	//Mitsubishi M32R little endian
	MACHINE_MIPS16			= 0x0266,	//MIPS16
	MACHINE_MIPSFPU			= 0x0366,	//MIPS with FPU
	MACHINE_MIPSFPU16		= 0x0466,	//MIPS16 with FPU
	MACHINE_POWERPC			= 0x01f0,	//Power PC little endian
	MACHINE_POWERPCFP		= 0x01f1,	//Power PC with floating point support
	MACHINE_X360			= 0x01f2,
	MACHINE_R3000			= 0x0162,	//MIPS little-endian, 0x160 big-endian
	MACHINE_R4000			= 0x0166,	//MIPS little endian
	MACHINE_R10000			= 0x0168,	// MIPS little-endian
	MACHINE_RISCV32			= 0x5032,	//RISC-V 32-bit address space
	MACHINE_RISCV64			= 0x5064,	//RISC-V 64-bit address space
	MACHINE_RISCV128		= 0x5128,	//RISC-V 128-bit address space
	MACHINE_SH3				= 0x01a2,	//Hitachi SH3
	MACHINE_SH3DSP			= 0x01a3,	//Hitachi SH3 DSP
	MACHINE_SH3E			= 0x01a4,	//SH3E little-endian
	MACHINE_SH4				= 0x01a6,	//Hitachi SH4
	MACHINE_SH5				= 0x01a8,	//Hitachi SH5
	MACHINE_WCEMIPSV2		= 0x0169,	//MIPS little-endian WCE v2

	MACHINE_ALPHA			= 0x0184,	// Alpha_AXP
	MACHINE_ALPHA64			= 0x0284,	// ALPHA64
	MACHINE_AXP64			= MACHINE_ALPHA64,
	MACHINE_TRICORE			= 0x0520,	// Infineon
	MACHINE_CEF				= 0x0CEF,
	MACHINE_CEE				= 0xC0EE,
};


struct FILE_HEADER : iso::littleendian_types {
	uint16		Machine;
	uint16		NumberOfSections;
	TIMEDATE	TimeDateStamp;
	uint32		PointerToSymbolTable;
	uint32		NumberOfSymbols;
	uint16		SizeOfOptionalHeader;
	uint16		Characteristics;
	bool		IsAnon()	const { return Machine == MACHINE_UNKNOWN && NumberOfSections == 0xffff; }
};

enum CHARACTERISTICS {
	RELOCS_STRIPPED			= 0x0001,
	EXECUTABLE_IMAGE		= 0x0002,
	LINE_NUMS_STRIPPED		= 0x0004,
	LOCAL_SYMS_STRIPPED		= 0x0008,
	AGGRESSIVE_WS_TRIM		= 0x0010,
	LARGE_ADDRESS_AWARE		= 0x0020,
	RESERVED				= 0x0040,
	BYTES_REVERSED_LO		= 0x0080,
	IS_32BIT				= 0x0100,
	DEBUG_STRIPPED			= 0x0200,
	REMOVABLE_RUN_FROM_SWAP	= 0x0400,
	NET_RUN_FROM_SWAP		= 0x0800,
	SYSTEM					= 0x1000,
	DLL						= 0x2000,
	UP_SYSTEM_ONLY			= 0x4000,
	BYTES_REVERSED_HI		= 0x8000,
};

struct OPTIONAL_HEADER : iso::littleendian_types {
	enum {
		MAGIC_NT32		= 0x10b,
		MAGIC_NT64		= 0x20b,
		MAGIC_ROM		= 0x107,

		MAGIC_OBJ		= 0x104,    // object files, eg as output
		MAGIC_DEMAND	= 0x10b,    // demand load format, eg normal ld output
		MAGIC_TARGET	= 0x101,	// target shlib
		MAGIC_HOST		= 0x123,	// host   shlib
	};
	uint16	Magic;
	uint8	MajorLinkerVersion;
	uint8	MinorLinkerVersion;
	uint32	SizeOfCode;
	uint32	SizeOfInitializedData;
	uint32	SizeOfUninitializedData;
	uint32	AddressOfEntryPoint;
	uint32	BaseOfCode;
};

struct SECTION_HEADER :	iso::littleendian_types {
	char	Name[8];
	xint32	VirtualSize;	// or PhysicalAddress
	xint32	VirtualAddress;
	xint32	SizeOfRawData;
	xint32	PointerToRawData;
	xint32	PointerToRelocations;
	xint32	PointerToLinenumbers;
	int16	NumberOfRelocations;
	int16	NumberOfLinenumbers;
	xint32	Characteristics;
};

enum SCN_CHARACTERISTICS {
//	SCN_						= 0x00000000,
//	SCN_						= 0x00000001,
//	SCN_						= 0x00000002,
//	SCN_						= 0x00000004,
	SCN_TYPE_NO_PAD				= 0x00000008,
//	SCN_						= 0x00000010,
	SCN_CNT_CODE				= 0x00000020,
	SCN_CNT_INITIALIZED_DATA	= 0x00000040,
	SCN_CNT_UNINITIALIZED_DATA	= 0x00000080,
	SCN_LNK_OTHER				= 0x00000100,
	SCN_LNK_INFO				= 0x00000200,
//	SCN_						= 0x00000400,
	SCN_LNK_REMOVE				= 0x00000800,
	SCN_LNK_COMDAT				= 0x00001000,
	SCN_GPREL					= 0x00008000,
	SCN_MEM_PURGEABLE			= 0x00020000,
	SCN_MEM_16BIT				= 0x00020000,
	SCN_MEM_LOCKED				= 0x00040000,
	SCN_MEM_PRELOAD				= 0x00080000,
	SCN_ALIGN_1BYTES			= 0x00100000,
	SCN_ALIGN_2BYTES			= 0x00200000,
	SCN_ALIGN_4BYTES			= 0x00300000,
	SCN_ALIGN_8BYTES			= 0x00400000,
	SCN_ALIGN_16BYTES			= 0x00500000,
	SCN_ALIGN_32BYTES			= 0x00600000,
	SCN_ALIGN_64BYTES			= 0x00700000,
	SCN_ALIGN_128BYTES			= 0x00800000,
	SCN_ALIGN_256BYTES			= 0x00900000,
	SCN_ALIGN_512BYTES			= 0x00A00000,
	SCN_ALIGN_1024BYTES			= 0x00B00000,
	SCN_ALIGN_2048BYTES			= 0x00C00000,
	SCN_ALIGN_4096BYTES			= 0x00D00000,
	SCN_ALIGN_8192BYTES			= 0x00E00000,
	SCN_LNK_NRELOC_OVFL			= 0x01000000,
	SCN_MEM_DISCARDABLE			= 0x02000000,
	SCN_MEM_NOT_CACHED			= 0x04000000,
	SCN_MEM_NOT_PAGED			= 0x08000000,
	SCN_MEM_SHARED				= 0x10000000,
	SCN_MEM_EXECUTE				= 0x20000000,
	SCN_MEM_READ				= 0x40000000,
	SCN_MEM_WRITE				= 0x80000000,
};

struct RELOC : packed {
	uint32		r_vaddr;	/* Reference Address */
	uint32		r_symndx;	/* Symbol index */
	uint16		r_type;		/* Type of relocation */
};


// 1 line number entry for every "breakpointable" source line in a section
// Line numbers are grouped on a per function basis; first entry in a function grouping will have l_lnno = 0 and in place of physical address will be the symbol table index of the function name
struct LINE_NUMBER : packed {
	union {
		int32	l_symndx;	/* Symbol Index */
		int32	l_paddr;	/* Physical Address */
	};
	uint16		l_lnno;		/* Line Number */
};

//-----------------------------------------------------------------------------
//	symbols
//-----------------------------------------------------------------------------

struct SYMBOL_NAME : packed {
	union {
		char	ShortName[8];
		struct {
			uint32	Short;		// if 0, use LongName
			uint32	Long;		// offset into string table
		};
	};
	explicit operator bool() {
		return Long != 0;
	}
	bool	is_short()	{
		return Short != 0;
	}
	bool	set_short(const char *v)	{
		char	*d = ShortName;
		while (d < ShortName + 8 && (*d++ = *v))
			++v;
		if (*v)
			return false;
		while (d < ShortName + 8)
			*d++ = 0;
		return true;
	}
	SYMBOL_NAME() {}
};

struct SYMBOL : packed {
	SYMBOL_NAME	Name;
	uint32		Value;
	int16		SectionNumber;
	uint16		Type;
	uint8		StorageClass;
	uint8		NumberOfAuxSymbols;
};

enum SYM_SECTION {
	SYM_UNDEFINED				= 0,
	SYM_ABSOLUTE				= -1,
	SYM_DEBUG					= -2,
	SYM_NTV						= -3,	// indicates symbol needs preload transfer vector
	SYM_PTV						= -4,	// indicates symbol needs postload transfer vector
};

// NOTE: types not really used

// Type of a symbol, in low 4 bits of Type
enum SYM_TYPE {
	SYM_TYPE_NULL				= 0,
	SYM_TYPE_VOID				= 1,
	SYM_TYPE_CHAR				= 2,
	SYM_TYPE_SHORT				= 3,
	SYM_TYPE_INT				= 4,
	SYM_TYPE_LONG				= 5,
	SYM_TYPE_FLOAT				= 6,
	SYM_TYPE_DOUBLE				= 7,
	SYM_TYPE_STRUCT				= 8,
	SYM_TYPE_UNION				= 9,
	SYM_TYPE_ENUM				= 10,
	SYM_TYPE_MOE				= 11,
	SYM_TYPE_BYTE				= 12,
	SYM_TYPE_WORD				= 13,
	SYM_TYPE_UINT				= 14,
	SYM_TYPE_UINT32				= 15,
	SYM_TYPE_PCODE				= 0x8000,
};

// in bits 4,5 of Type
enum SYM_DTYPE {
	SYM_DTYPE_NULL				= 0,
	SYM_DTYPE_POINTER			= 1,
	SYM_DTYPE_FUNCTION			= 2,
	SYM_DTYPE_ARRAY				= 3,
};

// StorageClass
enum SYM_CLASS {
	SYM_CLASS_END_OF_FUNCTION	= 0xFF,
	SYM_CLASS_NULL				= 0,
	SYM_CLASS_AUTOMATIC			= 1,
	SYM_CLASS_EXTERNAL			= 2,
	SYM_CLASS_STATIC			= 3,
	SYM_CLASS_REGISTER			= 4,
	SYM_CLASS_EXTERNAL_DEF		= 5,
	SYM_CLASS_LABEL				= 6,
	SYM_CLASS_UNDEFINED_LABEL	= 7,
	SYM_CLASS_MEMBER_OF_STRUCT	= 8,
	SYM_CLASS_ARGUMENT			= 9,
	SYM_CLASS_STRUCT_TAG		= 10,
	SYM_CLASS_MEMBER_OF_UNION	= 11,
	SYM_CLASS_UNION_TAG			= 12,
	SYM_CLASS_TYPE_DEFINITION	= 13,
	SYM_CLASS_UNDEFINED_STATIC	= 14,
	SYM_CLASS_ENUM_TAG			= 15,
	SYM_CLASS_MEMBER_OF_ENUM	= 16,
	SYM_CLASS_REGISTER_PARAM	= 17,
	SYM_CLASS_BIT_FIELD			= 18,
	SYM_CLASS_AUTOARG			= 19,
	SYM_CLASS_LASTENT			= 20,
	SYM_CLASS_BLOCK				= 100,
	SYM_CLASS_FUNCTION			= 101,
	SYM_CLASS_END_OF_STRUCT		= 102,
	SYM_CLASS_FILE				= 103,
	SYM_CLASS_SECTION			= 104,
	SYM_CLASS_WEAK_EXTERNAL		= 105,
	SYM_CLASS_HIDDEN			= 106,
	SYM_CLASS_CLR_TOKEN			= 107,
};

struct AUXSYMBOL_FUNC : packed {
	uint32	TagIndex;
	uint32	TotalSize;
	uint32	PointerToLinenumber;
	uint32	PointerToNextFunction;
	uint16	Unused;
};

struct AUXSYMBOL_BFEF : packed {
	uint32	Unused;
	uint16	Linenumber;
	uint16	Unused2[3];
	uint32	PointerToNextFunction;// (.bf only)
	uint16	Unused3;
};

struct AUXSYMBOL_WEAK : packed {
	uint32	TagIndex;
	uint32	Characteristics;
	uint16	Unused[5];
};

struct AUXSYMBOL_FILE {
	char	FileName[18];
};

struct AUXSYMBOL_SECT : packed {
	uint32	Length;
	uint16	NumberOfRelocations;
	uint16	NumberOfLinenumbers;
	uint32	CheckSum;
	uint16	Number;
	uint8	Selection;
	uint8	Unused[3];
};

struct SYMBOL_BIGOBJ : packed {
	SYMBOL_NAME	Name;
	uint32		Value;
	uint32		SectionNumber;
	uint16		Type;
	uint8		StorageClass;
	uint8		NumberOfAuxSymbols;
};

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

struct IMPORT_HEADER : iso::littleendian_types {
	enum TYPE {
		CODE			= 0,
		DATA			= 1,
		CONSTANT		= 2,
	};
	enum NAME_TYPE {
		ORDINAL			= 0,	// Import by ordinal
		NAME			= 1,	// Import name == public symbol name.
		NAME_NO_PREFIX	= 2,	// Import name == public symbol name skipping leading ?, @, or optionally _.
		NAME_UNDECORATE	= 3,	// Import name == public symbol name skipping leading ?, @, or optionally _ and truncating at first @
	};
	union {
		uint16	Ordinal;	// if grf & IMPORT_OBJECT_ORDINAL
		uint16	Hint;
	};
	uint16	flags;//Type:2, NameType:3, Reserved:11;
};

struct ANON_HEADER0 : iso::littleendian_types {
	uint16			Sig1;				// Must be MACHINE_UNKNOWN
	uint16			Sig2;				// Must be 0xffff
	uint16			Version;			// >= 1 (implies the CLSID field is present); >= 2 (implies the Flags field is present - otherwise V1)
	uint16			Machine;
	TIMEDATE		TimeDateStamp;
//	GUID			ClassID;			// only if Version >= 1
//	uint32			SizeOfData;
//	uint32			Flags;				// Version >= 2; 0x1 -> contains metadata
//	uint32			MetaDataSize;		// Size of CLR metadata
//	uint32			MetaDataOffset;		// Offset of CLR metadata
};

struct ANON_HEADER : ANON_HEADER0 {
	GUID			ClassID;			// only if Version >= 1
	uint32			SizeOfData;
	uint32			Flags;				// Version >= 2; 0x1 -> contains metadata
	uint32			MetaDataSize;		// Size of CLR metadata
	uint32			MetaDataOffset;		// Offset of CLR metadata
};

struct ANON_XBOXONE : iso::littleendian_types {
	static constexpr GUID guid = {0x0CB3FE38, 0xD9A5, 0x4DAB, {0xAC, 0x9B, 0xd6, 0xb6, 0x22, 0x26, 0x53, 0xc2}};//ClGlObjMagic
	uint16			a;					//0C13
	uint16			NumberOfSections;	//0007
	TIMEDATE		TimeDateStamp;
	uint32			b;					//004652CF
	uint32			c;					//00000010
	uint32			d;					//00000000
};

struct ANON_BIGOBJ : iso::littleendian_types {
	static constexpr GUID guid = {0xD1BAA1C7, 0xBAEE, 0x4ba9, {0xAF, 0x20, 0xFA, 0xF6, 0x6A, 0xA4, 0xDC, 0xB8}};//BigObjMagic
	uint32			NumberOfSections;
	uint32			PointerToSymbolTable;
	uint32			NumberOfSymbols;
};

struct ANON_RESOURCE : iso::littleendian_types {
	static constexpr GUID guid = {0x00000000, 0x0020, 0x0000, {0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00}};//WinResMagic

};


static const char BigObjMagic[] = {
	'\xc7', '\xa1', '\xba', '\xd1',
	'\xee', '\xba',
	'\xa9', '\x4b',
	'\xaf', '\x20', '\xfa', '\xf6', '\x6a', '\xa4', '\xdc', '\xb8',
};

static const char ClGlObjMagic[] = {
	'\x38', '\xfe', '\xb3', '\x0c',
	'\xa5', '\xd9',
	'\xab', '\x4d',
	'\xac', '\x9b', '\xd6', '\xb6', '\x22', '\x26', '\x53', '\xc2',
};

// The signature bytes that start a .res file.
static const char WinResMagic[] = {
	'\x00', '\x00', '\x00', '\x00',
	'\x20', '\x00',
	'\x00', '\x00',
	'\xff', '\xff', '\x00', '\x00', '\xff', '\xff', '\x00', '\x00',
};


//{0CB3FE38-D9A5-4DAB-AC9B-D6B6222653C2}


class RAW_COFF {
	const void	*h;
	iso::arbitrary_const_ptr	offset(iso::uint32 x)	const	{ return x ? (char*)h + x : 0; }

public:
	RAW_COFF(const void	*h) : h(h) {}

	const FILE_HEADER*		FileHeader()	const	{ return (FILE_HEADER*)h; }
	const OPTIONAL_HEADER*	OptHeader()		const	{ return (OPTIONAL_HEADER*)(FileHeader() + 1); }

	bool				IsAnon()		const	{ auto fh = FileHeader(); return fh->Machine == MACHINE_UNKNOWN && fh->NumberOfSections == 0xffff; }
	const ANON_HEADER	*GetAnon()		const	{ return IsAnon() ? (const ANON_HEADER*)h : 0; }
	bool				IsBig()			const	{ auto *anon = GetAnon(); return anon && anon->Version > 0 && anon->ClassID == ANON_BIGOBJ::guid; }
	const ANON_BIGOBJ	*GetBig()		const	{ return IsBig() ? (ANON_BIGOBJ*)(GetAnon() + 1) : 0; }
	auto				Machine()		const	{ auto *anon = GetAnon(); return anon ? anon->Machine : FileHeader()->Machine; }

	auto				GetMem(const SECTION_HEADER &sect)	const { return iso::const_memory_block(offset(sect.PointerToRawData), sect.SizeOfRawData); }

	auto				Sections()		const	{
		if (auto big = GetBig())
			return iso::make_range_n((const SECTION_HEADER*)(char*)(big + 1), big->NumberOfSections);
		auto	fh = FileHeader();
		return iso::make_range_n((const SECTION_HEADER*)((char*)(fh + 1) + fh->SizeOfOptionalHeader), fh->NumberOfSections);
	}
	auto&				Section(int i)	const	{ return Sections()[i]; }

	auto	GetSymbolTable() const {
		auto	fh = FileHeader();
		return iso::make_range_n((const SYMBOL*)offset(fh->PointerToSymbolTable), fh->NumberOfSymbols);
	}

	auto	GetSymbolTableBig() const {
		auto big = GetBig();
		return  iso::make_range_n((const SYMBOL_BIGOBJ*)offset(big->PointerToSymbolTable), big->NumberOfSymbols);
	}
};


} // namespace coff

#endif // COFF_H
