#ifndef PE_H
#define PE_H

#include "coff.h"
#include "base/strings.h"
#include "base/pointer.h"


//-----------------------------------------------------------------------------
//	PORTABLE EXECUTABLE FORMAT
//-----------------------------------------------------------------------------
namespace pe {
using namespace coff;

//	DOS .EXE header

struct DOS_HEADER : iso::littleendian_types {
	enum {MAGIC = 'ZM', PARAGRAPH = 16, PAGE = 512};
	uint16	magic;			//	Magic number
	uint16	cblp;			//	Bytes on last page of file
	uint16	cp;				//	Pages in file
	uint16	crlc;			//	Relocations
	uint16	cparhdr;		//	Size of header in paragraphs
	uint16	minalloc;		//	Minimum extra paragraphs needed
	uint16	maxalloc;		//	Maximum extra paragraphs needed
	uint16	ss;				//	Initial (relative) SS value
	uint16	sp;				//	Initial SP value
	uint16	csum;			//	Checksum
	uint16	ip;				//	Initial IP value
	uint16	cs;				//	Initial (relative) CS value
	uint16	lfarlc;			//	File address of relocation table
	uint16	ovno;			//	Overlay number
};

struct DOS_RELOC : iso::littleendian_types {
	uint16	offset;
	uint16	segment;
};

struct EXE_HEADER : DOS_HEADER {
	uint16	res[4];			//	Reserved words
	uint16	oemid;			//	OEM identifier (for e_oeminfo)
	uint16	oeminfo;		//	OEM information; e_oemid specific
	uint16	res2[10];		//	Reserved words
	int32	lfanew;			//	File address of new exe header
};


struct DATA_DIRECTORY : iso::littleendian_types {
	// Directory Entries
	enum {
		EXPORT			= 0,	// EXPORT_DIRECTORY		Export Directory
		IMPORT			= 1,	// IMPORT_DIRECTORY		Import Directory
		RESOURCE		= 2,	// Resource Directory
		EXCEPTION		= 3,	// Exception Directory
		SECURITY		= 4,	// Security Directory
		BASERELOC		= 5,	// Base Relocation Table
		DEBUG_DIR		= 6,	// Debug Directory
		COPYRIGHT		= 7,	// (X86 usage)
		ARCHITECTURE	= 7,	// Architecture Specific Data
		GLOBALPTR		= 8,	// RVA of GP
		TLS				= 9,
		LOAD_CONFIG		= 10,	// Load Configuration Directory
		BOUND_IMPORT	= 11,	// Bound Import Directory in headers
		IAT				= 12,	// Import Address Table
		DELAY_IMPORT	= 13,
		CLR_DESCRIPTOR	= 14,
	};
	uint32 VirtualAddress;
	uint32 Size;
};

struct OPTIONAL_HEADER32 : coff::OPTIONAL_HEADER {
	uint32		BaseOfData;
	uint32		ImageBase;
	uint32		SectionAlignment;
	uint32		FileAlignment;
	uint16		MajorOperatingSystemVersion;
	uint16		MinorOperatingSystemVersion;
	uint16		MajorImageVersion;
	uint16		MinorImageVersion;
	uint16		MajorSubsystemVersion;
	uint16		MinorSubsystemVersion;
	uint32		Win32VersionValue;
	uint32		SizeOfImage;
	uint32		SizeOfHeaders;
	uint32		CheckSum;
	uint16		Subsystem;
	uint16		DllCharacteristics;
	uint32		SizeOfStackReserve;
	uint32		SizeOfStackCommit;
	uint32		SizeOfHeapReserve;
	uint32		SizeOfHeapCommit;
	uint32		LoaderFlags;
	uint32		NumberOfRvaAndSizes;
	DATA_DIRECTORY DataDirectory[16];
};

struct OPTIONAL_HEADER64 : coff::OPTIONAL_HEADER {
	uint64		ImageBase;
	uint32		SectionAlignment;
	uint32		FileAlignment;
	uint16		MajorOperatingSystemVersion;
	uint16		MinorOperatingSystemVersion;
	uint16		MajorImageVersion;
	uint16		MinorImageVersion;
	uint16		MajorSubsystemVersion;
	uint16		MinorSubsystemVersion;
	uint32		Win32VersionValue;
	uint32		SizeOfImage;
	uint32		SizeOfHeaders;
	uint32		CheckSum;
	uint16		Subsystem;
	uint16		DllCharacteristics;
	uint64		SizeOfStackReserve;
	uint64		SizeOfStackCommit;
	uint64		SizeOfHeapReserve;
	uint64		SizeOfHeapCommit;
	uint32		LoaderFlags;
	uint32		NumberOfRvaAndSizes;
	DATA_DIRECTORY DataDirectory[16];
};

//EXPORT
struct EXPORT_DIRECTORY : iso::littleendian_types {
	uint32		ExportFlags;			// Reserved, must be 0.
	TIMEDATE	TimeDateStamp;			// The time and date that the export data was created.
	xint16		MajorVersion;			// The major version number. The major and minor version numbers can be set by the user.
	xint16		MinorVersion;			// The minor version number.
	xint32		DLLName;				// The address of the ASCII string that contains the name of the DLL. This address is relative to the image base.
	uint32		OrdinalBase;			// The starting ordinal number for exports in this image. This field specifies the starting ordinal number for the export address table. It is usually set to 1.
	uint32		NumberEntries;			// The number of entries in the export address table.
	uint32		NumberNames;			// The number of entries in the name pointer table. This is also the number of entries in the ordinal table.
	xint32		FunctionTable;			// RVA of functions
	xint32		NameTable;				// RVA of names
	xint32		OrdinalTable;			// RVA from base of image
};

//IMPORT
struct IMPORT_DIRECTORY : iso::littleendian_types {
	struct DESCRIPTOR {
		union {
			uint32	Characteristics;	// 0 for terminating null import descriptor
			uint32	OriginalFirstThunk;	// RVA to original unbound IAT (PIMAGE_THUNK_DATA)
		};
		TIMEDATE	TimeDateStamp;			// 0 if not bound, -1 if bound, and real date\time stamp in IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND)	// O.W. date/time stamp of DLL bound to (Old BIND)
		uint32		ForwarderChain;			// -1 if no forwarders
		uint32		DllName;
		uint32		FirstThunk;				// RVA to IAT (if bound this IAT has actual addresses)
	};

	struct BY_NAME {
		uint16		hint;
		const char	name[];
	};

	struct THUNK_DATA64 {
		static const uint64 ORDINAL = 0x8000000000000000ull;
		union {
			uint64 ForwarderString;	// PBYTE
			uint64 Function;		// Puint32
			uint64 Ordinal;
			uint64 AddressOfData;	// PIMAGE_IMPORT_BY_NAME
		};
		bool is_ordinal()	const	{ return !!(Ordinal & ORDINAL); }
		bool exists()		const	{ return Ordinal != 0; }
	};

	struct THUNK_DATA32 {
		static const uint32 ORDINAL = 0x80000000;
		union {
			uint32 ForwarderString;	// PBYTE
			uint32 Function;		// Puint32
			uint32 Ordinal;
			uint32 AddressOfData;	// PIMAGE_IMPORT_BY_NAME
		};
		bool is_ordinal()	const	{ return !!(Ordinal & ORDINAL); }
		bool exists()		const	{ return Ordinal != 0; }
	};

	DESCRIPTOR	desc[];
};

//TLS
struct TLS_DIRECTORY64 : iso::littleendian_types {
	typedef	void TLS_CALLBACK(void *DllHandle, uint32 Reason, void *Reserved);
	xint64	StartAddressOfRawData;
	xint64	EndAddressOfRawData;
	xint64	AddressOfIndex;		  // Puint32
	xint64	AddressOfCallBacks;	  // TLS_CALLBACK**;
	xint32	SizeOfZeroFill;
	union {
		uint32	Characteristics;
		struct {
			uint32 Reserved0	: 20;
			uint32 Alignment	: 4;
			uint32 Reserved1	: 8;
		};
	};
};

struct TLS_DIRECTORY32 : iso::littleendian_types {
	typedef	void TLS_CALLBACK(void *DllHandle, uint32 Reason, void *Reserved);
	xint32	StartAddressOfRawData;
	xint32	EndAddressOfRawData;
	xint32	AddressOfIndex;			// Puint32
	xint32	AddressOfCallBacks;		// TLS_CALLBACK**
	xint32	SizeOfZeroFill;
	union {
		uint32 Characteristics;
		struct {
			uint32 Reserved0	: 20;
			uint32 Alignment	: 4;
			uint32 Reserved1	: 8;
		};
	};
};

//BOUND_IMPORT
struct BOUND_IMPORT_DIRECTORY : iso::littleendian_types {
	struct DESCRIPTOR {
		struct REF {
			TIMEDATE	TimeDateStamp;
			iso::offset_pointer<char, uint16, BOUND_IMPORT_DIRECTORY>	ModuleName;
			uint16		Reserved;
		};

		TIMEDATE	TimeDateStamp;
		iso::offset_pointer<char, uint16, BOUND_IMPORT_DIRECTORY>	ModuleName;
		uint16		NumberOfModuleForwarderRefs;
		REF			Refs[1];
		const DESCRIPTOR *next() const { return (const DESCRIPTOR*)&Refs[NumberOfModuleForwarderRefs]; }
	};
	DESCRIPTOR	desc[];
};

//DELAY_IMPORT
struct DELAY_IMPORT_DIRECTORY : iso::littleendian_types {
	struct DESCRIPTOR {
		union {
			uint32 AllAttributes;
			struct {
				uint32 RvaBased				: 1;	// Delay load version 2
				uint32 ReservedAttributes	: 31;
			};
		};

		xint32		DllName;						// RVA to the name of the target library (NULL-terminate ASCII string)
		xint32		ModuleHandle;					// RVA to the HMODULE caching location (PHMODULE)
		xint32		ImportAddressTable;				// RVA to the start	of the IAT (PIMAGE_THUNK_DATA)
		xint32		ImportNameTable;				// RVA to the start	of the name	table (PIMAGE_THUNK_DATA::AddressOfData)
		xint32		BoundImportAddressTable;		// RVA to an optional bound	IAT
		xint32		UnloadInformationTable;			// RVA to an optional unload info table
		TIMEDATE	TimeDateStamp;					// 0 if	not	bound, otherwise, date/time	of the target DLL
	};
	DESCRIPTOR	desc[];
};

//DEBUG_DIR
struct DEBUG_DIRECTORY : iso::littleendian_types {
	enum TYPE {
		UNKNOWN			= 0,
		COFF			= 1,
		CODEVIEW		= 2,
		FPO				= 3,
		MISC			= 4,
		EXCEPTION		= 5,
		FIXUP			= 6,
		OMAP_TO_SRC		= 7,
		OMAP_FROM_SRC	= 8,
		BORLAND			= 9,
		RESERVED10		= 10,
		CLSID			= 11,
		VC_FEATURE		= 12,
		POGO			= 13,
		ILTCG			= 14,
		MPX				= 15,
	};
	template<TYPE T> struct DATA;

	uint32		Characteristics;
	TIMEDATE	TimeDateStamp;
	xint16		MajorVersion;
	xint16		MinorVersion;
	xint32		Type;
	xint32		SizeOfData;
	xint32		AddressOfRawData;
	xint32		PointerToRawData;
};


template<> struct DEBUG_DIRECTORY::DATA<DEBUG_DIRECTORY::COFF> {
	uint32		NumberOfSymbols;
	uint32		LvaToFirstSymbol;
	uint32		NumberOfLinenumbers;
	uint32		LvaToFirstLinenumber;
	uint32		RvaToFirstByteOfCode;
	uint32		RvaToLastByteOfCode;
	uint32		RvaToFirstByteOfData;
	uint32		RvaToLastByteOfData;
};

template<> struct DEBUG_DIRECTORY::DATA<DEBUG_DIRECTORY::CODEVIEW> {
	uint32			Format;	//'RSDS'
	GUID			Guid;
	iso::embedded_string	fn;
};

template<> struct DEBUG_DIRECTORY::DATA<DEBUG_DIRECTORY::FPO> {
	enum {
		FPO					= 0,
		TRAP				= 1,
		TSS					= 2,
		NONFPO				= 3,
		SIZE_OF_RFPO_DATA	= 16,
	};
	uint32		ulOffStart;			// offset 1st byte of function code
	uint32		cbProcSize;			// # bytes in function
	uint32		cdwLocals;			// # bytes in locals/4
	uint16		cdwParams;			// # bytes in params/4
	uint16		cbProlog : 8;		// # bytes in prolog
	uint16		cbRegs   : 3;		// # regs saved
	uint16		fHasSEH  : 1;		// TRUE if SEH in func
	uint16		fUseBP   : 1;		// TRUE if EBP has been allocated
	uint16		reserved : 1;		// reserved for future use
	uint16		cbFrame  : 2;		// frame type
};

template<> struct DEBUG_DIRECTORY::DATA<DEBUG_DIRECTORY::MISC> {
	enum {EXENAME = 1};
	uint32		DataType;			// type of misc data, see defines
	uint32		Length;				// total length of record, rounded to four byte multiple.
	uint8		Unicode;			// TRUE if data is unicode string
	uint8		Reserved[3];
	uint8		Data[1];			// Actual data
};

template<> struct DEBUG_DIRECTORY::DATA<DEBUG_DIRECTORY::VC_FEATURE> {
	uint32	Count_PRE_VC11;	//Pre-VC++ 11.00
	uint32	Count_C_CPP;	//C/C++
	uint32	Count_GS;		//GS
	uint32	Count_SDL;		//sdl
	uint32	Count_GUARDN;	//guardN
};

//LOAD_CONFIG
struct CODE_INTEGRITY : iso::littleendian_types {
	xint16		Flags;
	xint16		Catalog;			// 0xFFFF means not available
	xint32		CatalogOffset;
	xint32		Reserved;
};

struct LOAD_CONFIG_DIRECTORY32 : iso::littleendian_types {
	uint32		Size;
	TIMEDATE	TimeDateStamp;
	xint16		MajorVersion;
	xint16		MinorVersion;
	xint32		GlobalFlagsClear;
	xint32		GlobalFlagsSet;
	xint32		CriticalSectionDefaultTimeout;
	xint32		DeCommitFreeBlockThreshold;
	xint32		DeCommitTotalFreeThreshold;
	xint32		LockPrefixTable;
	xint32		MaximumAllocationSize;
	xint32		VirtualMemoryThreshold;
	xint32		ProcessHeapFlags;
	xint32		ProcessAffinityMask;
	xint16		CSDVersion;
	xint16		Reserved1;
	xint32		EditList;
	xint32		SecurityCookie;
	xint32		SEHandlerTable;
	xint32		SEHandlerCount;
	xint32		GuardCFCheckFunctionPointer;
	xint32		GuardCFDispatchFunctionPointer;
	xint32		GuardCFFunctionTable;
	xint32		GuardCFFunctionCount;
	xint32		GuardFlags;
	CODE_INTEGRITY CodeIntegrity;
};

struct LOAD_CONFIG_DIRECTORY64 : iso::littleendian_types {
	uint32		Size;
	TIMEDATE	TimeDateStamp;
	xint16		MajorVersion;
	xint16		MinorVersion;
	xint32		GlobalFlagsClear;
	xint32		GlobalFlagsSet;
	xint32		CriticalSectionDefaultTimeout;
	xint64		DeCommitFreeBlockThreshold;
	xint64		DeCommitTotalFreeThreshold;
	xint64		LockPrefixTable;
	xint64		MaximumAllocationSize;
	xint64		VirtualMemoryThreshold;
	xint64		ProcessAffinityMask;
	xint32		ProcessHeapFlags;
	xint16		CSDVersion;
	xint16		Reserved1;
	xint64		EditList;
	xint64		SecurityCookie;
	xint64		SEHandlerTable;
	xint64		SEHandlerCount;
	xint64		GuardCFCheckFunctionPointer;
	xint64		GuardCFDispatchFunctionPointer;
	xint64		GuardCFFunctionTable;
	xint64		GuardCFFunctionCount;
	xint32		GuardFlags;
	CODE_INTEGRITY CodeIntegrity;
};

//EXCEPTION
struct EXCEPTION_DIRECTORY : iso::littleendian_types {
	struct CE_ENTRY {
		xint32 FuncStart;
		uint32 PrologLen:8, FuncLen:22, ThirtyTwoBit:1, ExceptionFlag:1;
	};
	struct ARM_ENTRY {
		xint32 BeginAddress;
		uint32 Flag:2, FunctionLength:11, Ret:2, H:1, Reg:3, R:1, L:1, C:1, StackAdjust:10;
	};
	struct ARM64_ENTRY {
		xint32 BeginAddress;
		uint32 Flag:2, FunctionLength:11, RegF:3, RegI:4, H:1, CR:2, FrameSize:9;
	};
	struct ALPHA64_ENTRY {
		xint64 BeginAddress;
		xint64 EndAddress;
		xint64 ExceptionHandler;
		xint64 HandlerData;
		xint64 PrologEndAddress;
	};
	struct ALPHA_ENTRY {
		xint32 BeginAddress;
		xint32 EndAddress;
		xint32 ExceptionHandler;
		xint32 HandlerData;
		xint32 PrologEndAddress;
	};
	struct ENTRY {
		xint32 BeginAddress;
		xint32 EndAddress;
		xint32 UnwindInfoAddress;
	};
	ENTRY	Entries[];
};

enum UNWIND_OP {
	UWOP_PUSH_NONVOL		= 0,	//1 node		Push a nonvolatile integer register, decrementing RSP by 8. info indicates register: RAX=0,RCX=1,RDX=2,RBX=3,RSP=4,RBP=5,RSI=6,RDI=7,R8 to R15=8..15
	UWOP_ALLOC_LARGE		= 1,	//2 or 3 nodes	Allocate area on the stack. info=0: size = next slot * 8; info=1: size = next two slots
	UWOP_ALLOC_SMALL		= 2,	//1 node		Allocate area on the stack of info * 8 + 8
	UWOP_SET_FPREG			= 3,	//1 node		Set frame pointer register as rsp + offset*16
	UWOP_SAVE_NONVOL		= 4,	//2 nodes		Save a nonvolatile integer register on the stack. info = register, offset = next*8
	UWOP_SAVE_NONVOL_FAR	= 5,	//3 nodes		Save a nonvolatile integer register on the stack. info = register, offset = next 2 slots
	UWOP_SAVE_XMM128		= 8,	//2 nodes		Save all 128 bits of a nonvolatile XMM register on the stack. info = register, offset = next * 16
	UWOP_SAVE_XMM128_FAR	= 9,	//3 nodes		Save all 128 bits of a nonvolatile XMM register on the stack. info = register, offset = next 2 slots
	UWOP_PUSH_MACHFRAME		= 10,	//1 node		Push a machine frame. info = 0 => stack: RSP+32, SS, RSP+24, Old RSP, RSP+16, EFLAGS, RSP+8, CS, RSP, RIP; info = 1 => stack: RSP+40, SS, RSP+32, Old RSP, RSP+24, EFLAGS, RSP+16, CS, RSP+8, RIP, RSP, Error code
};

struct UNWIND_INFO : iso::littleendian_types {
	enum FLAGS {
		EHANDLER	= 1 << 0,	// function has an exception handler - called when looking for functions that need to examine exceptions
		UHANDLER	= 1 << 1,	// function has a termination handler - called when unwinding an exception
		CHAINED		= 1 << 2,	// this is a copy of a previous EXCEPTION_DIRECTORY::ENTRY for chaining
	};
	struct HANDLER {
		xint32	address;
		uint8	variable[];
	};
	union CODE {
		struct {
			uint8	prolog_offset;
			uint8	unwind_code:4, info:4;
		};
		uint16	u;
	};
	uint8	version:3, flags:5;
	uint8	prolog_size;	// Length of the function prolog in bytes
	uint8	num_codes;
	uint8	frame_reg:4, frame_offset:4;
	CODE	codes[];
	// HANDLER or chained EXCEPTION_DIRECTORY::ENTRY
};


enum COMDAT_SECTION {
	COMDAT_SELECT_NODUPLICATES	= 1,
	COMDAT_SELECT_ANY			= 2,
	COMDAT_SELECT_SAME_SIZE		= 3,
	COMDAT_SELECT_EXACT_MATCH	= 4,
	COMDAT_SELECT_ASSOCIATIVE	= 5,
	COMDAT_SELECT_LARGEST		= 6,
};

//-----------------------------------------------------------------------------
//	resources
//-----------------------------------------------------------------------------

enum RESOURCE_TYPE {
	IRT_NONE			= 0,
	IRT_CURSOR			= 1,
	IRT_BITMAP			= 2,
	IRT_ICON			= 3,
	IRT_MENU			= 4,
	IRT_DIALOG			= 5,
	IRT_STRING			= 6,
	IRT_FONTDIR			= 7,
	IRT_FONT			= 8,
	IRT_ACCELERATOR		= 9,
	IRT_RCDATA			= 10,
	IRT_MESSAGETABLE	= 11,
	IRT_GROUP_CURSOR	= 12,
	IRT_GROUP_ICON		= 14,
	IRT_VERSION			= 16,
	IRT_DLGINCLUDE		= 17,
	IRT_PLUGPLAY		= 19,
	IRT_VXD				= 20,
	IRT_ANICURSOR		= 21,
	IRT_ANIICON			= 22,
	IRT_HTML			= 23,
	IRT_MANIFEST		= 24,
	IRT_TOOLBAR			= 241,
};

struct RESOURCE_DIRECTORY_ENTRY : iso::littleendian_types {
	union {
		struct {
			uint32 NameOffset:31;
			uint32 NameIsString:1;
		};
		uint32	Name;
		uint16	Id;
	};
	union {
		uint32	OffsetToData;
		struct {
			uint32	OffsetToDirectory:31;
			uint32	DataIsDirectory:1;
		};
	};
};

struct RESOURCE_DIR_STRING : iso::littleendian_types {
	uint16		Length;
	char		NameString[1];
};

struct RESOURCE_DIR_STRING_U : iso::littleendian_types {
	uint16		Length;
	wchar_t		NameString[1];
};

struct RESOURCE_DATA_ENTRY : iso::littleendian_types {
	uint32		OffsetToData;
	uint32		Size;
	uint32		CodePage;
	uint32		Reserved;
};

struct RESOURCE_DIRECTORY : iso::littleendian_types {
	uint32		Characteristics;
	TIMEDATE	TimeDateStamp;
	uint16		MajorVersion;
	uint16		MinorVersion;
	uint16		NumberOfNamedEntries;
	uint16		NumberOfIdEntries;
	int			size() const { return NumberOfIdEntries + NumberOfNamedEntries; }
	iso::range<const RESOURCE_DIRECTORY_ENTRY*>	entries() const { return iso::make_range_n((const RESOURCE_DIRECTORY_ENTRY*)(this + 1), NumberOfIdEntries + NumberOfNamedEntries); }
};

struct RESOURCE_ICONDIR : iso::littleendian_types {
	enum TYPE {
		ICON	= 1,
		CURSOR	= 2,
	};
	struct ENTRY {
		iso::uint8	Width;
		iso::uint8	Height;
		iso::uint8	ColorCount;
		iso::uint8	Reserved;		// Reserved (must be 0)
		uint16		Planes;			// Color Planes
		uint16		BitCount;		// Bits per pixel
		uint32		BytesInRes;
		uint32		ImageOffset;
	};
	uint16	Reserved;
	uint16	Type;
	uint16	Count;
	iso::range<const ENTRY*>	entries() const { return iso::make_range_n((const ENTRY*)(this + 1), Count); }
	bool	valid()		const { return Reserved == 0 && (Type == ICON || Type == CURSOR); }
};

} // namespace pe

#endif	//PE_H
