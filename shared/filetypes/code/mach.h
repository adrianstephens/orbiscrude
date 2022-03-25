#ifndef MACH_H
#define MACH_H

#include "base/defs.h"
#include "base/strings.h"
#include "base/iterator.h"

namespace mach {

struct CPU_STATE { enum T {
	USER,
	SYSTEM,
	IDLE,
	NICE,
}; };

//	Machine types known by all.
struct CPU_TYPE { enum T {
	ANY			= -1,
	ARCH_MASK	= 0xff000000,		// mask for architecture bits
	ARCH_64		= 0x01000000,		// 64 bit ABI

	VAX			= 1,
	MC680x0		= 6,
	X86			= 7,
	I386		= X86,		// compatibility

	// MIPS		= 8
	MC98000		= 10,
	HPPA		= 11,
	ARM			= 12,
	MC88000		= 13,
	SPARC		= 14,
	I860		= 15,
	//ALPHA		= 16,
	POWERPC		= 18,

	X86_64		= X86 		| ARCH_64,
	POWERPC_64	= POWERPC 	| ARCH_64,
	ARM_64		= ARM 		| ARCH_64,
}; };

//	Machine threadtypes. (not defined - for most machine types/subtypes)
struct CPU_THREADTYPE { enum T {
	NONE		= 0,
	INTEL_HTT	= 1,
}; };

// Machine subtypes (these are defined here, instead of in a machine dependent directory, so that any program can get all definitions regardless of where is it compiled).
struct CPU_SUBTYPE { enum T {
// Capability bits used in the definition of cpu_subtype.
	MASK 	= 0xff000000,	// mask for feature flags
	LIB64 	= 0x80000000,	// 64 bit libraries

// 	Object files that are hand-crafted to run on any implementation of an architecture are tagged with MULTIPLE
//	This functions essentially the same as the "ALL" subtype of an architecture except that it allows us to easily find object files that may need to be modified whenever a new implementation of an architecture comes out.
//	It is the responsibility of the implementor to make sure the software handles unsupported implementations elegantly.
	MULTIPLE 		= -1,
	LITTLEENDIAN 	= 0,
	BIGENDIAN 		= 1,

// VAX subtypes
	VAX_ALL			= 0,
	VAX780			= 1,
	VAX785			= 2,
	VAX750			= 3,
	VAX730			= 4,
	UVAXI			= 5,
	UVAXII			= 6,
	VAX8200			= 7,
	VAX8500			= 8,
	VAX8600			= 9,
	VAX8650			= 10,
	VAX8800			= 11,
	UVAXIII			= 12,

 // 680x0 subtypes
	MC680x0_ALL		= 1,
	MC68030			= 1,	// compat
	MC68040			= 2,
	MC68030_ONLY	= 3,

// I386 subtypes
#define INTEL(f, m)	f + (m << 4)
	I386_ALL		= INTEL(3, 0),
	I386			= INTEL(3, 0),
	I486			= INTEL(4, 0),
	I486SX			= INTEL(4, 8),	// 8 << 4 = 128
	I586			= INTEL(5, 0),
	PENT			= INTEL(5, 0),
	PENTPRO			= INTEL(6, 1),
	PENTII_M3		= INTEL(6, 3),
	PENTII_M5		= INTEL(6, 5),
	CELERON			= INTEL(7, 6),
	CELERON_MOBILE	= INTEL(7, 7),
	PENTIUM_3		= INTEL(8, 0),
	PENTIUM_3_M		= INTEL(8, 1),
	PENTIUM_3_XEON	= INTEL(8, 2),
	PENTIUM_M		= INTEL(9, 0),
	PENTIUM_4		= INTEL(10, 0),
	PENTIUM_4_M		= INTEL(10, 1),
	ITANIUM			= INTEL(11, 0),
	ITANIUM_2		= INTEL(11, 1),
	XEON			= INTEL(12, 0),
	XEON_MP			= INTEL(12, 1),
#undef INTEL

// X86 subtypes.
	X86_ALL			= 3,
	X86_64_ALL		= 3,
	X86_ARCH1		= 4,

//	Mips subtypes.
	MIPS_ALL		= 0,
	MIPS_R2300		= 1,
	MIPS_R2600		= 2,
	MIPS_R2800		= 3,
	MIPS_R2000a		= 4,	// pmax
	MIPS_R2000		= 5,
	MIPS_R3000a		= 6,	// 3max
	MIPS_R3000		= 7,

// MC98000 (PowerPC) subtypes
	MC98000_ALL		= 0,
	MC98601			= 1,

// HPPA subtypes
	HPPA_ALL		= 0,
	HPPA_7100		= 0,	// compat
	HPPA_7100LC		= 1,

// MC88000 subtypes.
	MC88000_ALL		= 0,
	MC88100			= 1,
	MC88110			= 2,

// SPARC subtypes
	SPARC_ALL		= 0,

// I860 subtypes
	I860_ALL		= 0,
	I860_860		= 1,

// PowerPC subtypes
	POWERPC_ALL		= 0,
	POWERPC_601		= 1,
	POWERPC_602		= 2,
	POWERPC_603		= 3,
	POWERPC_603e	= 4,
	POWERPC_603ev	= 5,
	POWERPC_604		= 6,
	POWERPC_604e	= 7,
	POWERPC_620		= 8,
	POWERPC_750		= 9,
	POWERPC_7400	= 10,
	POWERPC_7450	= 11,
	POWERPC_970		= 100,

// ARM subtypes
	ARM_ALL			= 0,
	ARM_V4T			= 5,
	ARM_V6			= 6,
	ARM_V5TEJ		= 7,
	ARM_XSCALE		= 8,
	ARM_V7			= 9,
}; };

// CPU families
// These are meant to identify the CPU's marketing name - an application can map these to (possibly) localized strings.
struct CPU_FAMILY { enum T {
	UNKNOWN				= 0,
	POWERPC_G3			= 0xcee41549,
	POWERPC_G4			= 0x77c184ae,
	POWERPC_G5			= 0xed76d8aa,
	INTEL_6_13			= 0xaa33392b,
	INTEL_YONAH			= 0x73d67300,
	INTEL_MEROM			= 0x426f69ef,
	INTEL_PENRYN		= 0x78ea4fbc,
	INTEL_NEHALEM		= 0x6b5a4cd2,
	INTEL_WESTMERE		= 0x573b5eec,
	INTEL_SANDYBRIDGE	= 0x5490b78c,
	ARM_9				= 0xe73283ae,
	ARM_11				= 0x8ff620d8,
	ARM_XSCALE			= 0x53b005f5,
	ARM_13				= 0x0cc90e64,
	ARM_14				= 0x96077ef1,

// The following synonyms are deprecated:
	INTEL_6_14			= INTEL_YONAH,
	INTEL_6_15			= INTEL_MEROM,
	INTEL_6_23			= INTEL_PENRYN,
	INTEL_6_26			= INTEL_NEHALEM,
	INTEL_CORE			= INTEL_YONAH,
	INTEL_CORE2			= INTEL_MEROM,
}; };

struct fat_header : iso::bigendian_types {
//	uint32	magic;		// FAT_MAGIC
	uint32	nfat_arch;	// number of structs that follow
};

struct fat_arch : iso::bigendian_types {
	int32		cputype;	// cpu specifier (int)
	int32		cpusubtype;	// machine specifier (int)
	uint32		offset;		// file offset to this object file
	uint32		size;		// size of this object file
	uint32		align;		// alignment as a power of 2
};

using iso::uint8;

template<bool be, int bits> using mach_uint	= iso::endian_t<iso::uint_bits_t<bits>, be>;
template<bool be, int bits> using mach_xint	= iso::endian_t<iso::constructable<iso::baseint<16, iso::uint_bits_t<bits>>>, be>;

#define	mach_uint16		mach_uint<be, 16>
#define	mach_uint32		mach_uint<be, 32>
#define	mach_uint64		mach_uint<be, 64>
#define	mach_xint32		mach_xint<be, 32>
#define	mach_xint64		mach_xint<be, 64>
#define	vm_prot_t		iso::endian_t<iso::int32, be>

#define FAT_MAGIC	0xcafebabe
#define FAT_CIGAM	0xbebafeca	// NXSwapLong(FAT_MAGIC)

/*
 * The layout of the file depends on the filetype.	For all but the MH_OBJECT file type the segments are padded out and aligned on a segment alignment
 * boundary for efficient demand pageing.	The MH_EXECUTE, MH_FVMLIB, MH_DYLIB, MH_DYLINKER and MH_BUNDLE file types also have the headers included as part
 * of their first segment.
 *
 * The file type MH_OBJECT is a compact format intended as output of the assembler and input (and possibly output) of the link editor (the .o
 * format).	All sections are in one unnamed segment with no segment padding.
 * This format is used as an executable format when the file is so small the segment padding greatly increases its size.
 *
 * The file type MH_PRELOAD is an executable format intended for things that are not executed under the kernel (proms, stand alones, kernels, etc).	The
 * format can be executed under the kernel but may demand paged it and not preload it before execution.
 *
 * A core file is in MH_CORE format and can be any in an arbritray legal Mach-O file.
 */

/* The load commands directly follow the mach_header. The total size of all of the commands is given by the sizeofcmds field in the mach_header. All
 * load commands must have as their first two fields cmd and cmdsize. The cmd field is filled in with a constant for that command type.	Each command type
 * has a structure specifically for it.	The cmdsize field is the size in bytes of the particular load command structure plus anything that follows it that
 * is a part of the load command (i.e. section structures, strings, etc.). To advance to the next load command the cmdsize can be added to the offset or
 * pointer of the current load command.	The cmdsize for 32-bit architectures MUST be a multiple of 4 bytes and for 64-bit architectures MUST be a multiple
 * of 8 bytes (these are forever the maximum alignment of any load commands). The padded bytes must be zero. All tables in the object file must also
 * follow these rules so the file can be memory mapped.	Otherwise the pointers to these tables will not work well or at all on some machines. With all
 * padding zeroed like objects will compare byte for byte.*/

/* After MacOS X 10.1 when a new load command is added that is required to be understood by the dynamic linker for the image to execute properly the
 * LC_REQ_DYLD bit will be or'ed into the load command constant. If the dynamic linker sees such a load command it it does not understand will issue a
 * "unknown load command required for execution" error and refuse to use the image.	Other load commands without this bit that are not understood will
 * simply be ignored.*/

struct cmd {
	enum CMD : unsigned int {
		REQ_DYLD				= 0x80000000,
		SEGMENT					= 0x01,				// segment of this file to be mapped
		SYMTAB					= 0x02,				// link-edit stab symbol table info
		SYMSEG					= 0x03,				// link-edit gdb symbol table info (obsolete)
		THREAD					= 0x04,				// thread
		UNIXTHREAD				= 0x05,				// unix thread (includes a stack)
		LOADFVMLIB				= 0x06,				// load a specified fixed VM shared library
		IDFVMLIB				= 0x07,				// fixed VM shared library identification
		IDENT					= 0x08,				// object identification info (obsolete)
		FVMFILE					= 0x09,				// fixed VM file inclusion (internal use)
		PREPAGE					= 0x0a,				// prepage command (internal use)
		DYSYMTAB				= 0x0b,				// dynamic link-edit symbol table info
		LOAD_DYLIB				= 0x0c,				// load a dynamically linked shared library
		ID_DYLIB				= 0x0d,				// dynamically linked shared lib ident
		LOAD_DYLINKER			= 0x0e,				// load a dynamic linker
		ID_DYLINKER				= 0x0f,				// dynamic linker identification
		PREBOUND_DYLIB			= 0x10,				// modules prebound for a dynamically linked shared library
		ROUTINES				= 0x11,				// image routines
		SUB_FRAMEWORK			= 0x12,				// sub framework
		SUB_UMBRELLA			= 0x13,				// sub umbrella
		SUB_CLIENT				= 0x14,				// sub client
		SUB_LIBRARY				= 0x15,				// sub library
		TWOLEVEL_HINTS			= 0x16,				// two-level namespace lookup hints
		PREBIND_CKSUM			= 0x17,				// prebind checksum
		LOAD_WEAK_DYLIB			= 0x18 | REQ_DYLD,	// load a dynamically linked shared library that is allowed to be missing (all symbols are weak imported).
		SEGMENT_64				= 0x19,				// 64-bit segment of this file to be mapped
		ROUTINES_64				= 0x1a,				// 64-bit image routines
		UUID					= 0x1b,				// the uuid
		RPATH					= 0x1c | REQ_DYLD,	// runpath additions
		CODE_SIGNATURE			= 0x1d,				// local of code signature
		SEGMENT_SPLIT_INFO		= 0x1e,				// local of info to split segments
		REEXPORT_DYLIB			= 0x1f | REQ_DYLD,	// load and re-export dylib
		LAZY_LOAD_DYLIB			= 0x20,				// delay load of dylib until first use
		ENCRYPTION_INFO			= 0x21,				// encrypted segment information
		DYLD_INFO				= 0x22,				// compressed dyld information
		DYLD_INFO_ONLY			= 0x22 | REQ_DYLD,	// compressed dyld information only
		LOAD_UPWARD_DYLIB		= 0x23 | REQ_DYLD,	// load upward dylib
		VERSION_MIN_MACOSX		= 0x24,				// build for MacOSX min OS version
		VERSION_MIN_IPHONEOS	= 0x25,				// build for iPhoneOS min OS version
		FUNCTION_STARTS			= 0x26,				// compressed table of function start addresses
		DYLD_ENVIRONMENT		= 0x27,				// string for dyld to treat like environment variable
		MAIN					= 0x28 | REQ_DYLD,	// replacement for LC_UNIXTHREAD
		DATA_IN_CODE			= 0x29,				// table of non-instructions in __text
		SOURCE_VERSION			= 0x2A,				// source version used to build binary
		DYLIB_CODE_SIGN_DRS		= 0x2B,				// Code signing DRs copied from linked dylibs
		ENCRYPTION_INFO_64		= 0x2C,				// 64-bit encrypted segment information
		LINKER_OPTION			= 0x2D,				// linker options in MH_OBJECT files
		LINKER_OPTIMIZATION_HINT= 0x2E,				// optimization hints in MH_OBJECT files
		VERSION_MIN_TVOS		= 0x2F,				// build for AppleTV min OS version
		VERSION_MIN_WATCHOS		= 0x30,				// build for Watch min OS version
		NOTE					= 0x31,				// arbitrary data included within a Mach-O file
		BUILD_VERSION			= 0x32,				// build for platform min OS version
		DYLD_EXPORTS_TRIE		= 0x33 | REQ_DYLD,	// used with linkedit_data_command, payload is trie
		DYLD_CHAINED_FIXUPS		= 0x34 | REQ_DYLD,	// used with linkedit_data_command

	};
	template<int bits> 	static constexpr CMD SEGMENT_ 				= SEGMENT;
	template<> 			static constexpr CMD SEGMENT_<64> 			= SEGMENT_64;
	template<int bits> 	static constexpr CMD ROUTINES_ 				= ROUTINES;
	template<> 			static constexpr CMD ROUTINES_<64> 			= ROUTINES_64;
	template<int bits> 	static constexpr CMD ENCRYPTION_INFO_ 		= ENCRYPTION_INFO;
	template<> 			static constexpr CMD ENCRYPTION_INFO_<64> 	= ENCRYPTION_INFO_64;
};

template<cmd::CMD C, bool be> struct commandT;

template<bool be> struct command {
	mach_uint32 cmd;		// type of load command
	mach_uint32 cmdsize;	// total size of command in bytes
	command(cmd::CMD c, size_t size) { cmd = c; cmdsize = (iso::uint32)size; }
	const command				*next()	const { return (const command*)((const char*)this + cmdsize); }
	template<cmd::CMD C> auto	as()	const { return static_cast<const commandT<C, be>*>(this); }
};

template<bool be> struct _header {
	// Constants for the filetype field of the mach_header
	enum FILETYPE {
		OBJECT					= 1,			// relocatable object file
		EXECUTE					= 2,			// demand paged executable file
		FVMLIB					= 3,			// fixed VM shared library file
		CORE					= 4,			// core file
		PRELOAD					= 5,			// preloaded executable file
		DYLIB					= 6,			// dynamically bound shared library
		DYLINKER				= 7,			// dynamic link editor
		BUNDLE					= 8,			// dynamically bound bundle file
		DYLIB_STUB				= 9,			// shared library stub for static linking only, no section contents
		DSYM					= 10,			// companion file with only debug sections
		KEXT_BUNDLE				= 11,			// x86_64 kexts
	};
	// Constants for the flags field of the mach_header
	enum FLAGS {
		NOUNDEFS				= 0x1,			// the object file has no undefinedreferences
		INCRLINK				= 0x2,			// the object file is the output of an incremental link against a base file and can't be link edited again
		DYLDLINK				= 0x4,			// the object file is input for the dynamic linker and can't be staticly link edited again
		BINDATLOAD				= 0x8,			// the object file's undefined references are bound by the dynamic linker when loaded.
		PREBOUND				= 0x10,			// the file has its dynamic undefined references prebound.
		SPLIT_SEGS				= 0x20,			// the file has its read-only and read-write segments split
		LAZY_INIT				= 0x40,			// the shared library init routine is to be run lazily via catching memory faults to its writeable segments (obsolete)
		TWOLEVEL				= 0x80,			// the image is using two-level name space bindings
		FORCE_FLAT				= 0x100,		// the executable is forcing all images to use flat name space bindings
		NOMULTIDEFS				= 0x200,		// this umbrella guarantees no multiple defintions of symbols in its sub-images so the two-level namespace hints can always be used.
		NOFIXPREBINDING			= 0x400,		// do not have dyld notify the prebinding agent about this executable
		PREBINDABLE				= 0x800,		// the binary is not prebound but can have its prebinding redone. only used when MH_PREBOUND is not set.
		ALLMODSBOUND			= 0x1000,		// indicates that this binary binds to all two-level namespace modules of its dependent libraries. only used when MH_PREBINDABLE and MH_TWOLEVEL are both set.
		SUBSECTIONS_VIA_SYMBOLS	= 0x2000,		// safe to divide up the sections into sub-sections via symbols for dead code stripping
		CANONICAL				= 0x4000,		// the binary has been canonicalized via the unprebind operation
		WEAK_DEFINES			= 0x8000,		// the final linked image contains external weak symbols
		BINDS_TO_WEAK			= 0x10000,		// the final linked image uses weak symbols

		ALLOW_STACK_EXECUTION	= 0x20000,		// When this bit is set, all stacks in the task will be given stack execution privilege.	Only used in MH_EXECUTE filetypes.
		ROOT_SAFE				= 0x40000,		// When this bit is set, the binary declares it is safe for use in processes with uid zero
		SETUID_SAFE				= 0x80000,		// When this bit is set, the binary declares it is safe for use in processes when issetugid() is true
		NO_REEXPORTED_DYLIBS	= 0x100000,		// When this bit is set on a dylib, the static linker does not need to examine dependent dylibs to see if any are re-exported
		PIE						= 0x200000,		// When this bit is set, the OS will load the main executable at a random address.Only used in MH_EXECUTE filetypes.
		DEAD_STRIPPABLE_DYLIB	= 0x400000,		// Only for use on dylibs.	When linking against a dylib that has this bit set, the static linker will automatically not create a LC_LOAD_DYLIB load command to the dylib if no symbols are being referenced from the dylib.
		HAS_TLV_DESCRIPTORS		= 0x800000,		// Contains a section of type S_THREAD_LOCAL_VARIABLES
		NO_HEAP_EXECUTION		= 0x1000000,	// When this bit is set, the OS will run the main executable with a non-executable heap even on platforms (e.g. i386) that don't require it. Only used in MH_EXECUTE filetypes.
	};

	mach_uint32			magic;			// mach magic number identifier
	mach_uint32			cputype;		// cpu specifier
	mach_uint32			cpusubtype;		// machine specifier
	mach_uint32			filetype;		// type of file
	mach_uint32			ncmds;			// number of load commands
	mach_uint32			sizeofcmds;		// the size of all the load commands
	mach_uint32			flags;			// flags
};

template<bool be, int bits> struct header;

template<bool be, int bits, unsigned _MAGIC> struct _header2 : _header<be> {
	enum {
		MAGIC	= _MAGIC
	};
	auto commands()	const {
		return iso::make_range_n(iso::make_next_iterator((const command<be>*)(static_cast<const header<be, bits>*>(this) + 1)), this->ncmds);
	};
};

template<> struct header<false, 32> : _header2<false, 32, 0xfeedfaceu> {};
template<> struct header<true, 32>	: _header2<true, 32, 0xcefaedfeu> {};

template<> struct header<false, 64>	: _header2<false, 64, 0xfeedfacfu> {
	uint8		reserved[4];
};
template<> struct header<true, 64>	: _header2<true, 64, 0xcffaedfeu> {
	uint8		reserved[4];
};

template<cmd::CMD C, bool be> struct commandT_base : command<be> {
	commandT_base() : command<be>(C, sizeof(commandT<C, be>)) {}
	commandT_base(size_t size) : command<be>(C, size) {}
};

/* A variable length string in a load command is represented by an str union.	The strings are stored just after the load command structure and
 * the offset is from the start of the load command structure.	The size of the string is reflected in the cmdsize field of the load command.
 * Once again any padded bytes to bring the cmdsize field to a multiple of 4 bytes must be zero.*/

template<bool be> struct str {
	mach_uint32	offset;	// offset to the string
	operator const char*() const { return (const char*)this - sizeof(mach::command<be>) + offset; }
};

template<bool be> struct table {
	mach_uint32	offset;	// or first index
	mach_uint32	size;	// or num;
	iso::const_memory_block mem(const void *p) const { return {(const char*)p + offset, size}; }
	template<typename T> iso::range<const T*> range(const T *p) const { return {p + offset, p + (offset + size)}; }
};

template<bool be> struct str_command : command<be> {
	str<be>		name;
};


/* The segment load command indicates that a part of this file is to be mapped into the task's address space. The size of this segment in memory,
 * vmsize, maybe equal to or larger than the amount to map from this file, filesize. The file is mapped starting at fileoff to the beginning of
 * the segment in memory, vmaddr. The rest of the memory of the segment, if any, is allocated zero fill on demand. The segment's maximum virtual
 * memory protection and initial virtual memory protection are specified by the maxprot and initprot fields. If the segment has sections then the
 * section structures directly follow the segment command and their size is reflected in cmdsize. */

struct segment_header {
// Constants for the flags field
	enum FLAGS {
		HIGHVM				= 0x1,	// the file contents for this segment is for the high part of the VM space, the low part is zero filled (for stacks in core files)
		FVMLIB				= 0x2,	// this segment is the VM that is allocated by a fixed VM library, for overlap checking in the link editor
		NORELOC				= 0x4,	// this segment has nothing that was relocated in it and nothing relocated to it, that is it maybe safely replaced without relocation
		PROTECTED_VERSION_1	= 0x8,	// This segment is protected.	If the segment starts at file offset 0, the first page of the segment is not protected.	All other pages of the segment are protected.
	};
	iso::fixed_string<16>	segname;	// segment name
};

/* A segment is made up of zero or more sections. Non-MH_OBJECT files have all of their segments with the proper sections in each, and padded to the
 * specified segment alignment when produced by the link editor. The first segment of a MH_EXECUTE and MH_FVMLIB format file contains the mach_header
 * and load commands of the object file before its first section. The zero fill sections are always last in their segment (in all formats).	This
 * allows the zeroed segment padding to be mapped into memory where zero fill sections might be. The gigabyte zero fill sections, those with the section
 * type S_GB_ZEROFILL, can only be in a segment with sections of this type. These segments are then placed after all other segments.
 *
 * The MH_OBJECT format has all of its sections in one segment for compactness.	There is no padding to a specified segment boundary and the
 * mach_header and load commands are not part of the segment.
 *
 * Sections with the same section name, sectname, going into the same segment, segname, are combined by the link editor. The resulting section is aligned
 * to the maximum alignment of the combined sections and is the new section's alignment. The combined sections are aligned to their original alignment in
 * the combined section. Any padded bytes to get the specified alignment are zeroed.
 *
 * The format of the relocation entries referenced by the reloff and nreloc fields of the section structure for mach object files is described in the
 * header file <reloc.h>.*/

struct section_header {
	enum FLAGS {
		TYPE								= 0x000000ff,
		ATTRIBUTES							= 0xffffff00,

		REGULAR								= 0x0,	// regular section
		ZEROFILL							= 0x1,	// zero fill on demand section
		CSTRING_LITERALS					= 0x2,	// section with only literal C strings
		LITERALS4							= 0x3,	// section with only 4 byte literals
		LITERALS8							= 0x4,	// section with only 8 byte literals
		LITERAL_POINTERS					= 0x5,	// section with only pointers to literals
		NON_LAZY_SYMBOL_POINTERS			= 0x6,	// section with only non-lazy symbol pointers
		LAZY_SYMBOL_POINTERS				= 0x7,	// section with only lazy symbol pointers
		SYMBOL_STUBS						= 0x8,	// section with only symbol stubs, byte size of stub in	the reserved2 field
		MOD_INIT_FUNC_POINTERS				= 0x9,	// section with only function pointers for initialization
		MOD_TERM_FUNC_POINTERS				= 0xa,	// section with only function pointers for termination
		COALESCED							= 0xb,	// section contains symbols that are to be coalesced
		GB_ZEROFILL							= 0xc,	// zero fill on demand section (that can be larger than 4 gigabytes)
		INTERPOSING							= 0xd,	// section with only pairs of function pointers for interposing
		LITERALS16							= 0xe,	// section with only 16 byte literals
		DTRACE_DOF							= 0xf,	// section contains DTrace Object Format
		LAZY_DYLIB_SYMBOL_POINTERS			= 0x10,	// section with only lazy symbol pointers to lazy loaded dylibs
		// Section types to support thread local variables
		THREAD_LOCAL_REGULAR				= 0x11,	// template of initial values for TLVs
		THREAD_LOCAL_ZEROFILL				= 0x12,	// template of initial values for TLVs
		THREAD_LOCAL_VARIABLES				= 0x13,	// TLV descriptors
		THREAD_LOCAL_VARIABLE_POINTERS		= 0x14,	// pointers to TLV descriptors
		THREAD_LOCAL_INIT_FUNCTION_POINTERS	= 0x15,	// functions to call to initialize TLV values

		ATTRIBUTES_USR						= 0xff000000,	// User setable attributes
		ATTR_PURE_INSTRUCTIONS				= 0x80000000,	// section contains only true machine instructions
		ATTR_NO_TOC							= 0x40000000,	// section contains coalesced symbols that are not to be in a ranlib table of contents
		ATTR_STRIP_STATIC_SYMS				= 0x20000000,	// ok to strip static symbols in this section in files with the MH_DYLDLINK flag
		ATTR_NO_DEAD_STRIP					= 0x10000000,	// no dead stripping
		ATTR_LIVE_SUPPORT					= 0x08000000,	// blocks are live if they reference live blocks
		ATTR_SELF_MODIFYING_CODE			= 0x04000000,	// Used with i386 code stubs written on by dyld
		ATTR_DEBUG							= 0x02000000,	// a debug section

		ATTRIBUTES_SYS						= 0x00ffff00,	// system setable attributes
		ATTR_SOME_INSTRUCTIONS				= 0x00000400,	// section contains some machine instructions
		ATTR_EXT_RELOC						= 0x00000200,	// section has external relocation entries
		ATTR_LOC_RELOC						= 0x00000100,	// section has local relocation entries
	};
	iso::fixed_string<16>	sectname;	// name of this section
	iso::fixed_string<16>	segname;	// segment this section goes in
};

template<bool be, int bits> struct section : section_header {
	using type = mach_xint<be, bits>;
	type		addr;			// memory address of this section
	type		size;			// size in bytes of this section
	mach_xint32	offset;			// file offset of this section
	mach_uint32	align;			// section alignment (power of 2)
	mach_xint32	reloff;			// file offset of relocation entries
	mach_uint32	nreloc;			// number of relocation entries
	mach_xint32	flags;			// flags (section type and attributes)
	mach_uint32	reserved1;		// reserved (for offset or index)
	mach_uint32	reserved2;		// reserved (for count or sizeof)
};

//template<bool be, int bits> struct section;
//
//template<bool be> struct section<be, 32> : section_header {	// for 32-bit architectures
//	mach_xint32	addr;			// memory address of this section
//	mach_xint32	size;			// size in bytes of this section
//	mach_xint32	offset;			// file offset of this section
//	mach_uint32	align;			// section alignment (power of 2)
//	mach_xint32	reloff;			// file offset of relocation entries
//	mach_uint32	nreloc;			// number of relocation entries
//	mach_xint32	flags;			// flags (section type and attributes)
//	mach_uint32	reserved1;		// reserved (for offset or index)
//	mach_uint32	reserved2;		// reserved (for count or sizeof)
//};
//
//template<bool be> struct section<be,64> : section_header {	// for 64-bit architectures
//	mach_xint64	addr;			// memory address of this section
//	mach_xint64	size;			// size in bytes of this section
//	mach_xint32	offset;			// file offset of this section
//	mach_uint32	align;			// section alignment (power of 2)
//	mach_xint32	reloff;			// file offset of relocation entries
//	mach_uint32	nreloc;			// number of relocation entries
//	mach_xint32	flags;			// flags (section type and attributes)
//	mach_uint32	reserved1;		// reserved (for offset or index)
//	mach_uint32	reserved2;		// reserved (for count or sizeof)
//	mach_uint32	reserved3;		// reserved
//};

template<bool be, int bits> struct segment : segment_header {	// for 32-bit architectures
	using type = mach_xint<be, bits>;
	type		vmaddr;			// memory address of this segment
	type		vmsize;			// memory size of this segment
	type		fileoff;		// file offset of this segment
	type		filesize;		// amount to map from the file
	vm_prot_t	maxprot;		// maximum VM protection
	vm_prot_t	initprot;		// initial VM protection
	mach_uint32	nsects;			// number of sections in segment
	mach_xint32	flags;			// flags
	auto		sections()					const { return iso::make_range_n((section<be, bits>*)(this + 1), nsects); }
	bool		contains(intptr_t a)		const { return a >= vmaddr && a < vmaddr + vmsize; }
	bool		contains_file(intptr_t a)	const { return a >= fileoff && a < fileoff + filesize; }
};

//template<bool be, int bits> struct segment : segment_header {	// for 32-bit architectures
//	mach_xint32	vmaddr;			// memory address of this segment
//	mach_xint32	vmsize;			// memory size of this segment
//	mach_xint32	fileoff;		// file offset of this segment
//	mach_xint32	filesize;		// amount to map from the file
//	vm_prot_t	maxprot;		// maximum VM protection
//	vm_prot_t	initprot;		// initial VM protection
//	mach_uint32	nsects;			// number of sections in segment
//	mach_xint32	flags;			// flags
//	auto		sections()				const { return iso::make_range_n((section<be, 32>*)(this + 1), nsects); }
//	bool		contains(intptr_t a)	const { return a >= vmaddr && a < vmaddr + vmsize; }
//};

// The 64-bit segment load command indicates that a part of this file is to be mapped into a 64-bit task's address space.
// If the 64-bit segment has sections then section_64 structures directly follow the 64-bit segment command and their size is reflected in cmdsize.
//template<bool be> struct segment<be, 64> : segment_header {	// for 64-bit architectures
//	mach_xint64	vmaddr;			// memory address of this segment
//	mach_xint64	vmsize;			// memory size of this segment
//	mach_xint64	fileoff;		// file offset of this segment
//	mach_xint64	filesize;		// amount to map from the file
//	vm_prot_t	maxprot;		// maximum VM protection
//	vm_prot_t	initprot;		// initial VM protection
//	mach_uint32	nsects;			// number of sections in segment
//	mach_xint32	flags;			// flags
//	auto		sections()				const { return iso::make_range_n((section<be, 64>*)(this + 1), nsects); }
//	bool		contains(intptr_t a)	const { return a >= vmaddr && a < vmaddr + vmsize; }
//};

template<bool be> struct commandT<cmd::SEGMENT, be> : command<be>, segment<be, 32> {	// for 32-bit architectures
	commandT(int n = 0) : command<be>(cmd::SEGMENT, sizeof(*this) + sizeof(section<be, 32>) * n) { this->nsects = n; }
};

// The 64-bit segment load command indicates that a part of this file is to be mapped into a 64-bit task's address space.
// If the 64-bit segment has sections then section_64 structures directly follow the 64-bit segment command and their size is reflected in cmdsize.
template<bool be> struct commandT<cmd::SEGMENT_64, be> : command<be>, segment<be, 64> {	// for 64-bit architectures
	commandT(int n = 0) : command<be>(cmd::SEGMENT_64, sizeof(*this) + sizeof(section<be, 64>) * n) { this->nsects = n; }
};

template<bool be, int bits> using segment_command = commandT<cmd::SEGMENT_<bits>, be>;


// The currently known segment names and the section names in those segments
#define	SEG_PAGEZERO		"__PAGEZERO"		// the pagezero segment which has no protections and catches NULL references for MH_EXECUTE files

#define	SEG_TEXT			"__TEXT"			// the tradition UNIX text segment
#define	SECT_TEXT			"__text"			// the real text part of the text section no headers, and no padding
#define SECT_FVMLIB_INIT0	"__fvmlib_init0"	// the fvmlib initialization section
#define SECT_FVMLIB_INIT1	"__fvmlib_init1"	// the section following the fvmlib initialization section

#define	SEG_DATA			"__DATA"			// the tradition UNIX data segment
#define	SECT_DATA			"__data"			// the real initialized data section no padding, no bss overlap
#define	SECT_BSS			"__bss"				// the real uninitialized data section no padding
#define SECT_COMMON			"__common"			// the section common symbols are allocated in by the link editor

#define	SEG_OBJC			"__OBJC"			// objective-C runtime segment
#define SECT_OBJC_SYMBOLS	"__symbol_table"	// symbol table
#define SECT_OBJC_MODULES	"__module_info"		// module information
#define SECT_OBJC_STRINGS	"__selector_strs"	// string table
#define SECT_OBJC_REFS		"__selector_refs"	// string table

#define	SEG_ICON			"__ICON"			// the icon segment
#define	SECT_ICON_HEADER	"__header"			// the icon headers
#define	SECT_ICON_TIFF		"__tiff"			// the icons in tiff format

#define	SEG_LINKEDIT		"__LINKEDIT"		// the segment containing all structs created and maintained by the link editor. Created with -seglinkedit option to ld(1) for MH_EXECUTE and FVMLIB file types only
#define SEG_UNIXSTACK		"__UNIXSTACK"		// the unix stack segment
#define SEG_IMPORT			"__IMPORT"			// the segment for the self (dyld) modifing code stubs that has read, write and execute permissions

// Fixed virtual memory shared libraries are identified by two things.	The target pathname (the name of the library as found for execution), and the minor version number.
// The address of where the headers are loaded is in header_addr.
// (THIS IS OBSOLETE and no longer supported)
template<bool be> struct fvmlib {
	str<be>		name;			// library's target pathname
	mach_xint32	minor_version;	// library's minor version number
	mach_xint32	header_addr;	// library's header address
};

// A fixed virtual shared library (filetype == MH_FVMLIB in the mach header) contains a fvmlib_command (cmd == LC_IDFVMLIB) to identify the library.
// An object that uses a fixed virtual shared library also contains a fvmlib_command (cmd == LC_LOADFVMLIB) for each library it uses.
// (THIS IS OBSOLETE and no longer supported).
template<bool be> struct fvmlib_command : command<be> {
	fvmlib<be>	lib;		// the library identification
};
template<bool be> struct commandT<cmd::LOADFVMLIB, be> 	: fvmlib_command<be> {};
template<bool be> struct commandT<cmd::IDFVMLIB, be> 	: fvmlib_command<be> {};

/* Dynamicly linked shared libraries are identified by two things.	The pathname (the name of the library as found for execution), and the
 * compatibility version number. The pathname must match and the compatibility number in the user of the library must be greater than or equal to the
 * library being used.	The time stamp is used to record the time a library was built and copied into user so it can be use to determined if the library used
 * at runtime is exactly the same as used to built the program. */
template<bool be> struct dylib {
	str<be>		name;					// library's path name
	mach_uint32	timestamp;				// library's build time stamp
	mach_xint32	current_version;		// library's current version number
	mach_xint32	compatibility_version;	// library's compatibility vers number
};

/* A dynamically linked shared library (filetype == MH_DYLIB in the mach header) contains a dylib_command (cmd == LC_ID_DYLIB) to identify the library.
 * An object that uses a dynamically linked shared library also contains a dylib_command (cmd == LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, or
 * LC_REEXPORT_DYLIB) for each library it uses. */
template<bool be> struct dylib_command : command<be>, dylib<be> {};
template<bool be> struct commandT<cmd::LOAD_DYLIB, 			be> : dylib_command<be> {};
template<bool be> struct commandT<cmd::LOAD_WEAK_DYLIB, 	be> : dylib_command<be> {};
template<bool be> struct commandT<cmd::ID_DYLIB, 			be> : dylib_command<be> {};
template<bool be> struct commandT<cmd::REEXPORT_DYLIB, 		be> : dylib_command<be> {};
template<bool be> struct commandT<cmd::LAZY_LOAD_DYLIB, 	be> : dylib_command<be> {};
template<bool be> struct commandT<cmd::LOAD_UPWARD_DYLIB, 	be> : dylib_command<be> {};

/* A dynamically linked shared library may be a subframework of an umbrella framework. If so it will be linked with "-umbrella umbrella_name" where
 * Where "umbrella_name" is the name of the umbrella framework. A subframework can only be linked against by its umbrella framework or other subframeworks
 * that are part of the same umbrella framework. Otherwise the static link editor produces an error and states to link against the umbrella framework.
 * The name of the umbrella framework for subframeworks is recorded in the following structure. */
template<bool be> struct commandT<cmd::SUB_FRAMEWORK, be> : str_command<be> {};;//tr<be> 	umbrella;	// the umbrella framework name

/* For dynamically linked shared libraries that are subframework of an umbrella framework they can allow clients other than the umbrella framework or other
 * subframeworks in the same umbrella framework. To do this the subframework is built with "-allowable_client client_name" and an LC_SUB_CLIENT load
 * command is created for each -allowable_client flag.	The client_name is usually a framework name. It can also be a name used for bundles clients
 * where the bundle is built with "-client_name client_name". */
template<bool be> struct commandT<cmd::SUB_CLIENT, be> : str_command<be> {};//	str<be> 	client;		// the client name

/* A dynamically linked shared library may be a sub_umbrella of an umbrella framework. If so it will be linked with "-sub_umbrella umbrella_name" where
 * Where "umbrella_name" is the name of the sub_umbrella framework.	When staticly linking when -twolevel_namespace is in effect a twolevel namespace
 * umbrella framework will only cause its subframeworks and those frameworks listed as sub_umbrella frameworks to be implicited linked in.	Any other
 * dependent dynamic libraries will not be linked it when -twolevel_namespace is in effect.	The primary library recorded by the static linker when
 * resolving a symbol in these libraries will be the umbrella framework. Zero or more sub_umbrella frameworks may be use by an umbrella framework.
 * The name of a sub_umbrella framework is recorded in the following structure.*/
template<bool be> struct commandT<cmd::SUB_UMBRELLA, be> : str_command<be> {};//	str<be> 	sub_umbrella;	// the sub_umbrella framework name

/* A dynamically linked shared library may be a sub_library of another shared library.	If so it will be linked with "-sub_library library_name" where
 * Where "library_name" is the name of the sub_library shared library.	When staticly linking when -twolevel_namespace is in effect a twolevel namespace
 * shared library will only cause its subframeworks and those frameworks listed as sub_umbrella frameworks and libraries listed as sub_libraries to
 * be implicited linked in.	Any other dependent dynamic libraries will not be linked it when -twolevel_namespace is in effect.	The primary library
 * recorded by the static linker when resolving a symbol in these libraries will be the umbrella framework (or dynamic library). Zero or more sub_library
 * shared libraries may be use by an umbrella framework or (or dynamic library). The name of a sub_library framework is recorded in the following structure.
 * For example /usr/lib/libobjc_profile.A.dylib would be recorded as "libobjc". */
template<bool be> struct commandT<cmd::SUB_LIBRARY, be> : str_command<be> {};//	str<be> 	sub_library;	// the sub_library name

/* A program (filetype == MH_EXECUTE) that is prebound to its dynamic libraries has one of these for each library that
 * the static linker used in prebinding. It contains a bit vector for the modules in the library. The bits indicate which modules are bound (1) and
 * which are not (0) from the library.	The bit for module 0 is the low bit of the first byte.	So the bit for the Nth module is:
 * (linked_modules[N/8] >> N%8) & 1 */
template<bool be> struct commandT<cmd::PREBOUND_DYLIB, be> : command<be> {
	str<be>		name;			// library's path name
	mach_uint32	nmodules;		// number of modules in library
	str<be>		linked_modules;	// bit vector of linked modules
};

/* A program that uses a dynamic linker contains a dylinker_command to identify the name of the dynamic linker (LC_LOAD_DYLINKER).	And a dynamic linker
 * contains a dylinker_command to identify the dynamic linker (LC_ID_DYLINKER). A file can have at most one of these.
 * This struct is also used for the LC_DYLD_ENVIRONMENT load command and contains string for dyld to treat like environment variable.*/
template<bool be> struct commandT<cmd::LOAD_DYLINKER, 		be> : str_command<be> {};
template<bool be> struct commandT<cmd::ID_DYLINKER, 		be> : str_command<be> {};
template<bool be> struct commandT<cmd::DYLD_ENVIRONMENT,	be> : str_command<be> {};

/* Thread commands contain machine-specific data structures suitable for use in the thread state primitives. The machine specific data structures
 * follow the struct thread_command as follows. Each flavor of machine specific data structure is preceded by an unsigned
 * long constant for the flavor of that data structure, an mach_uint32 that is the count of longs of the size of the state data structure and then
 * the state data structure follows. This triple may be repeated for many flavors. The constants for the flavors, counts and state data structure
 * definitions are expected to be in the header file <machine/thread_status.h>. These machine specific data structures sizes must be multiples of
 * 4 bytes The cmdsize reflects the total size of the thread_command and all of the sizes of the constants for the flavors, counts and state
 * data structures.
 *
 * For executable objects that are unix processes there will be one thread_command (cmd == LC_UNIXTHREAD) created for it by the link-editor.
 * This is the same as a LC_THREAD, except that a stack is automatically created (based on the shell's limit for the stack size). Command arguments
 * and environment variables are copied onto that stack. */
template<bool be> struct thread_command : command<be> {
	// mach_uint32 flavor			flavor of thread state
	// mach_uint32 count			count of longs in thread state
	// struct XXX_thread_state state	thread state for this flavor
	// ...
};
template<bool be> struct commandT<cmd::THREAD, 		be> : thread_command<be> {};
template<bool be> struct commandT<cmd::UNIXTHREAD,	be> : thread_command<be> {};

/* The routines command contains the address of the dynamic shared library initialization routine and an index into the module table for the module
 * that defines the routine.	Before any modules are used from the library the dynamic linker fully binds the module that defines the initialization routine
 * and then calls it.	This gets called before any module initialization routines (used for C++ static constructors) in the library.*/
template<bool be> struct commandT<cmd::ROUTINES, be> : command<be> {	// for 32-bit architectures
	mach_xint32	init_address;	// address of initialization routine
	mach_uint32	init_module;	// index into the module table that the init routine is defined in
	mach_uint32	reserved1;
	mach_uint32	reserved2;
	mach_uint32	reserved3;
	mach_uint32	reserved4;
	mach_uint32	reserved5;
	mach_uint32	reserved6;
};

template<bool be> struct commandT<cmd::ROUTINES_64, be> : command<be> {	// for 64-bit architectures
	mach_xint64	init_address;	// address of initialization routine
	mach_uint64	init_module;	// index into the module table that the init routine is defined in
	mach_uint64	reserved1;
	mach_uint64	reserved2;
	mach_uint64	reserved3;
	mach_uint64	reserved4;
	mach_uint64	reserved5;
	mach_uint64	reserved6;
};
template<bool be, int bits> using routine_command = commandT<cmd::ROUTINES_<bits>, be>;	// for 64-bit architectures

struct nlist_enums {
	enum FLAGS {
	//masks
		STAB	= 0xe0,		// if any of these bits set, a symbolic debugging entry
		PEXT	= 0x10,		// private external symbol bit
		TYPE	= 0x0e,		// mask for the type bits
		EXT		= 0x01,		// external symbol bit, set for external symbols
		
		UNDF	= 0x00,		// undefined, n_sect == NO_SECT
		ABS		= 0x02,		// absolute, n_sect == NO_SECT
		INDR	= 0x0a,		// indirect
		PBUD	= 0x0c,		// prebound undefined (defined in a dylib)
		SECT	= 0x0e,		// defined in section number n_sect
	// STAB values
		GSYM	= 0x20,		// global symbol: name,,NO_SECT,type,0
		FNAME	= 0x22,		// procedure name (f77 kludge): name,,NO_SECT,0,0
		FUN		= 0x24,		// procedure: name,,n_sect,linenumber,address
		STSYM	= 0x26,		// static symbol: name,,n_sect,type,address
		LCSYM	= 0x28,		// .lcomm symbol: name,,n_sect,type,address
		BNSYM	= 0x2e,		// begin nsect sym: 0,,n_sect,0,address
		OPT		= 0x3c,		// emitted with gcc2_compiled and in gcc source
		RSYM	= 0x40,		// register sym: name,,NO_SECT,type,register
		SLINE	= 0x44,		// src line: 0,,n_sect,linenumber,address
		ENSYM	= 0x4e,		// end nsect sym: 0,,n_sect,0,address
		SSYM	= 0x60,		// structure elt: name,,NO_SECT,type,struct_offset
		SO		= 0x64,		// source file name: name,,n_sect,0,address
		OSO		= 0x66,		// object file name: name,,0,0,st_mtime
		LSYM	= 0x80,		// local sym: name,,NO_SECT,type,offset
		BINCL	= 0x82,		// include file beginning: name,,NO_SECT,0,sum
		SOL		= 0x84,		// #included file name: name,,n_sect,0,address
		PARAMS	= 0x86,		// compiler parameters: name,,NO_SECT,0,0
		VERSION	= 0x88,		// compiler version: name,,NO_SECT,0,0
		OLEVEL	= 0x8A,		// compiler -O level: name,,NO_SECT,0,0
		PSYM	= 0xa0,		// parameter: name,,NO_SECT,type,offset
		EINCL	= 0xa2,		// include file end: name,,NO_SECT,0,0
		ENTRY	= 0xa4,		// alternate entry: name,,n_sect,linenumber,address
		LBRAC	= 0xc0,		// left bracket: 0,,NO_SECT,nesting level,address
		EXCL	= 0xc2,		// deleted include file: name,,NO_SECT,0,sum
		RBRAC	= 0xe0,		// right bracket: 0,,NO_SECT,nesting level,address
		BCOMM	= 0xe2,		// begin common: name,,NO_SECT,0,0
		ECOMM	= 0xe4,		// end common: name,,n_sect,0,0
		ECOML	= 0xe8,		// end common (local name): 0,,n_sect,0,address
		LENG	= 0xfe,		// second stab entry with length information
	};
	friend FLAGS operator|(FLAGS a, FLAGS b) { return FLAGS(int(a) | int(b)); }

	enum DESC : iso::uint16 {
		REF_MASK						= 7,
		REF_UNDEFINED_NON_LAZY			= 0,
		REF_UNDEFINED_LAZY				= 1,
		REF_DEFINED						= 2,
		REF_PRIVATE_DEFINED				= 3,
		REF_PRIVATE_UNDEFINED_NON_LAZY	= 4,
		REF_PRIVATE_UNDEFINED_LAZY		= 5,

		REF_DYNAMIC						= 0x0010,
		NO_DEAD_STRIP					= 0x0020,	// symbol is not to be dead stripped
		DESC_DISCARDED					= 0x0020,	// symbol is discarded
		WEAK_REF						= 0x0040,	// symbol is weak referenced
		WEAK_DEF						= 0x0080,	// coalesed symbol is a weak definition
		REF_TO_WEAK						= 0x0080,	// reference to a weak symbol
		ARM_THUMB_DEF					= 0x0008,	// symbol is a Thumb function (ARM)
		SYMBOL_RESOLVER					= 0x0100,

		ALIGN							= 1		<< 8,
		MAX_LIBRARY_ORDINAL				= 0xfd	<< 8,
		DYNAMIC_LOOKUP_ORDINAL			= 0xfe	<< 8,
		EXECUTABLE_ORDINAL				= 0xff	<< 8,
	};
};

template<bool be, int bits> struct nlist : nlist_enums {
	mach_uint32				strx;	// index into the string table
	iso::compact<FLAGS,8>	flags;
	uint8					sect;	// section number or NO_SECT
	DESC					desc;
	mach_xint<be, bits>		value;
	
	const char *name(const char *s) const { return s ? s + strx : ""; }
	FLAGS		type()	const	{ return FLAGS(flags & TYPE); }
	FLAGS		stab()	const	{ return FLAGS(flags & STAB); }
};

// The symtab_command contains the offsets and sizes of the link-edit 4.3BSD "stab" style symbol table information as described in the header files <nlist.h> and <stab.h>.
template<bool be> struct commandT<cmd::SYMTAB, be> : command<be> {
	table<be>	sym;		// symbol table
	table<be>	str;		// string table
	commandT() : command<be>(cmd::SYMTAB, sizeof(*this)) {}
};

/* This is the second set of the symbolic information which is used to support the data structures for the dynamically link editor.
 * The original set of symbolic information in the symtab_command which contains the symbol and string tables must also be present when this load command is present.
 * The symbol table is organized into three groups of symbols:
 *	local symbols (static and debugging symbols) - grouped by module
 *	defined external symbols - grouped by module (sorted by name if not lib)
 *	undefined external symbols (sorted by name if MH_BINDATLOAD is not set, and in order the were seen by the static linker if MH_BINDATLOAD is set)
 * This load command contains a the offsets and sizes of the following new symbolic information tables:
 *	table of contents
 *	module table
 *	reference symbol table
 *	indirect symbol table
 * The first three tables are only present if the file is a dynamically linked shared library.
 * For executable and object modules, which are files containing only one module, the information that would be in these three tables is determined as follows:
 * 	table of contents - the defined external symbols are sorted by name
 *	module table - the file contains only one module so everything in the file is part of the module.
 *	reference symbol table - is the defined and undefined external symbols
 *
 * For dynamically linked shared library files this load command also contains offsets and sizes to the pool of relocation entries for all sections separated into two groups:
 *	external relocation entries
 *	local relocation entries
 * For executable and object modules the relocation entries continue to hang off the section structures.*/
template<bool be> struct commandT<cmd::DYSYMTAB, be> : command<be> {
	enum {
		INDIRECT_SYMBOL_LOCAL	= 0x80000000,	// non-lazy symbol pointer section for a defined symbol which strip(1) has removed
		INDIRECT_SYMBOL_ABS		= 0x40000000,
	};

	struct toc_entry {
		mach_uint32 symbol_index;			// the defined external symbol (index into the symbol table)
		mach_uint32 module_index;			// index into the module table this symbol is defined in
	};

	struct module_32 {
		mach_uint32	module_name;			// the module name (index into string table)

		mach_uint32	iextdefsym;				// index into externally defined symbols
		mach_uint32	nextdefsym;				// number of externally defined symbols
		mach_uint32	irefsym;				// index into reference symbol table
		mach_uint32	nrefsym;				// number of reference symbol table entries
		mach_uint32	ilocalsym;				// index into symbols for local symbols
		mach_uint32	nlocalsym;				// number of local symbols

		mach_uint32	iextrel;				// index into external relocation entries
		mach_uint32	nextrel;				// number of external relocation entries

		mach_uint32	iinit_iterm;			// low 16 bits are the index into the init section, high 16 bits are the index into the term section
		mach_uint32	ninit_nterm;			// low 16 bits are the number of init section entries, high 16 bits are the number of term section entries

		mach_uint32	objc_module_info_addr;	// for this module address of the start of the (__OBJC,__module_info) section
		mach_uint32	objc_module_info_size;	// for this module size of the (__OBJC,__module_info) section
	};

	struct dylib_module_64 {
		mach_uint32 module_name;			// the module name (index into string table)

		mach_uint32 iextdefsym;				// index into externally defined symbols
		mach_uint32 nextdefsym;				// number of externally defined symbols
		mach_uint32 irefsym;				// index into reference symbol table
		mach_uint32 nrefsym;				// number of reference symbol table entries
		mach_uint32 ilocalsym;				// index into symbols for local symbols
		mach_uint32 nlocalsym;				// number of local symbols

		mach_uint32 iextrel;				// index into external relocation entries
		mach_uint32 nextrel;				// number of external relocation entries

		mach_uint32 iinit_iterm;			// low 16 bits are the index into the init section, high 16 bits are the index into the term section
		mach_uint32 ninit_nterm;			// low 16 bits are the number of init section entries, high 16 bits are the number of term section entries

		mach_uint32	objc_module_info_size;	// for this module size of the (__OBJC,__module_info) section
		mach_uint64	objc_module_info_addr;	// for this module address of the start of the (__OBJC,__module_info) section
	};

	// The entries in the reference symbol table are used when loading the module (both by the static and dynamic link editors) and if the module is unloaded or replaced.
	// Therefore all external symbols (defined and undefined) are listed in the module's reference table. The flags describe the type of reference that is being made.
	// The constants for the flags are defined in <mach-o/nlist.h> as they are also used for symbol table entries.
	struct reference {
		mach_uint32	isym:24,	// index into the symbol table
					flags:8;	// flags to indicate the type of reference
	};

	/*The symbols indicated by symoff and nsyms of the LC_SYMTAB load command are grouped into the following three groups:
	*	local symbols (further grouped by the module they are from)
	*	defined external symbols (further grouped by the module they are from)
	*	undefined symbols
	*
	* The local symbols are used only for debugging. The dynamic binding process may have to use them to indicate to the debugger the local symbols for a module that is being bound.
	* The last two groups are used by the dynamic binding process to do the binding (indirectly through the module table and the reference symbol table when this is a dynamically linked shared library file).*/

	table<be>	localsym;		// local symbols
	table<be>	extdefsym;		// externally defined symbols
	table<be>	undefsym;		// undefined symbols

	/*For the for the dynamic binding process to find which module a symbolis defined in the table of contents is used (analogous to the ranlib
	* structure in an archive) which maps defined external symbols to modules they are defined in.	This exists only in a dynamically linked shared
	* library file.	For executable and object modules the defined external symbols are sorted by name and is use as the table of contents. */
	table<be>	toc;			// table of contents

	/*To support dynamic binding of "modules" (whole object files) the symbol table must reflect the modules that the file was created from.	This is
	* done by having a module table that has indexes and counts into the merged tables for each module.	The module structure that these two entries
	* refer to is described below.	This exists only in a dynamically linked shared library file.	For executable and object modules the file only
	* contains one module so everything in the file belongs to the module.*/
	table<be>	modtab;			// module table

	/*To support dynamic module binding the module structure for each module indicates the external references (defined and undefined) each module
	* makes.	For each module there is an offset and a count into the reference symbol table for the symbols that the module references.
	* This exists only in a dynamically linked shared library file.	For executable and object modules the defined external symbols and the
	* undefined external symbols indicates the external references. */
	table<be>	extrefsym;		// referenced symbol table

	/*The sections that contain "symbol pointers" and "routine stubs" have indexes and (implied counts based on the size of the section and fixed
	* size of the entry) into the "indirect symbol" table for each pointer and stub. For every section of these two types the index into the
	* indirect symbol table is stored in the section header in the field reserved1.	An indirect symbol table entry is simply a 32bit index into
	* the symbol table to the symbol that the pointer or stub is referring to. The indirect symbol table is ordered to match the entries in the section.*/
	table<be>	indirectsym;	// indirect symbol table

	/*To support relocating an individual module in a library file quickly the external relocation entries for each module in the library need to be
	* accessed efficiently.	Since the relocation entries can't be accessed through the section headers for a library file they are separated into
	* groups of local and external entries further grouped by module.	In this case the presents of this load command who's extreloff, nextrel,
	* locreloff and nlocrel fields are non-zero indicates that the relocation entries of non-merged sections are not referenced through the section
	* structures (and the reloff and nreloc fields in the section headers are set to zero).
	*
	* Since the relocation entries are not accessed through the section headers this requires the r_address field to be something other than a section
	* offset to identify the item to be relocated.	In this case r_address is set to the offset from the vmaddr of the first LC_SEGMENT command.
	* For MH_SPLIT_SEGS images r_address is set to the the offset from the vmaddr of the first read-write LC_SEGMENT command.
	*
	* The relocation entries are grouped by module and the module table entries have indexes and counts into them for the group of external
	* relocation entries for that the module.
	*/

	// For sections that are merged across modules there must not be any remaining external relocation entries for them (for merged sections remaining relocation entries must be local)
	table<be> extrel;		// external relocation entries

	//All the local relocation entries are grouped together (they are not grouped by their module since they are only used if the object is moved from it staticly link edited address)
	table<be> locrel;		// local relocation entries
};

// The twolevel_hints_command contains the offset and number of hints in the two-level namespace lookup hints table.
/* The entries in the two-level namespace lookup hints table are twolevel_hint structs. These provide hints to the dynamic link editor where to start
 * looking for an undefined symbol in a two-level namespace image. The isub_image field is an index into the sub-images (sub-frameworks and
 * sub-umbrellas list) that made up the two-level image that the undefined symbol was found in when it was built by the static link editor. If
 * isub-image is 0 the the symbol is expected to be defined in library and not in the sub-images. If isub-image is non-zero it is an index into the array
 * of sub-images for the umbrella with the first index in the sub-images being 1. The array of sub-images is the ordered list of sub-images of the umbrella
 * that would be searched for a symbol that has the umbrella recorded as its primary library. The table of contents index is an index into the
 * library's table of contents.	This is used as the starting point of the binary search or a directed linear search. */

template<bool be> struct hint {
	mach_uint32 isub_image:8,	// index into the sub images
				itoc:24;		// index into the table of contents
};
template<> struct hint<true> {
	iso::uint32	isub_image:8,
				itoc:24;
};

template<bool be> struct commandT<cmd::TWOLEVEL_HINTS, be> : command<be> {
	mach_uint32	offset;		// offset to the hint table
	mach_uint32	nhints;		// number of hints in the hint table
	hint<be>	hints[1];
};


/* The prebind_cksum_command contains the value of the original check sum for prebound files or zero.	When a prebound file is first created or modified
 * for other than updating its prebinding information the value of the check sum is set to zero.	When the file has it prebinding re-done and if the value of
 * the check sum is zero the original check sum is calculated and stored in cksum field of this load command in the output file.	If when the prebinding
 * is re-done and the cksum field is non-zero it is left unchanged from the input file. */
template<bool be> struct commandT<cmd::PREBIND_CKSUM, be> : command<be> {
	mach_xint32	cksum;		// the check sum or zero
};

// The uuid load command contains a single 128-bit unique random number that identifies an object produced by the static link editor.
template<bool be> struct commandT<cmd::UUID, be> : command<be> {
	GUID	uuid;			// the 128-bit uuid
};

 // The rpath_command contains a path which at runtime should be added to the current run path used to find @rpath prefixed dylibs.
template<bool be> struct commandT<cmd::RPATH, be> : str_command<be> {};//	str<be>	path;			// path to add to run path

// The linkedit_data_command contains the offsets and sizes of a blob of data in the __LINKEDIT segment.
template<bool be> struct linkedit_data_command : command<be> {
	mach_xint32	dataoff;	// file offset of data in __LINKEDIT segment
	mach_xint32	datasize;	// file size of data in __LINKEDIT segment
};
template<bool be> struct commandT<cmd::CODE_SIGNATURE,		be>	: linkedit_data_command<be> {};
template<bool be> struct commandT<cmd::SEGMENT_SPLIT_INFO,	be>	: linkedit_data_command<be> {};
template<bool be> struct commandT<cmd::FUNCTION_STARTS,		be>	: linkedit_data_command<be> {};
template<bool be> struct commandT<cmd::DATA_IN_CODE,		be>	: linkedit_data_command<be> {};
template<bool be> struct commandT<cmd::DYLIB_CODE_SIGN_DRS,	be> : linkedit_data_command<be> {};
template<bool be> struct commandT<cmd::LINKER_OPTIMIZATION_HINT,be> : linkedit_data_command<be> {};
template<bool be> struct commandT<cmd::DYLD_EXPORTS_TRIE,	be> : linkedit_data_command<be> {};
template<bool be> struct commandT<cmd::DYLD_CHAINED_FIXUPS,	be> : linkedit_data_command<be> {};

// The encryption_info_command contains the file offset and size of an of an encrypted segment.
template<bool be> struct commandT<cmd::ENCRYPTION_INFO, be> : command<be> {
	mach_xint32	cryptoff;	// file offset of encrypted range
	mach_xint32	cryptsize;	// file size of encrypted range
	mach_uint32	cryptid;	// which enryption system, 0 means not-encrypted yet
};

template<bool be> struct commandT<cmd::ENCRYPTION_INFO_64, be> : command<be> {
	mach_xint32	cryptoff;	// file offset of encrypted range
	mach_xint32	cryptsize;	// file size of encrypted range
	mach_xint32	cryptid;	// which enryption system,  0 means not-encrypted yet
	mach_xint32	pad;
};

template<bool be, int bits> using encryption_info_command = commandT<cmd::ENCRYPTION_INFO_<bits>, be>;


// The version_min_command contains the min OS version on which this binary was built to run.
template<bool be> struct version_min_command : command<be> {
	mach_xint32	version;	// X.Y.Z is encoded in nibbles xxxx.yy.zz
	mach_uint32	reserved;	// zero
	version_min_command(cmd::CMD c, iso::uint32 v) : command<be>(c, sizeof(*this)) { version = v; reserved = 0; }
};

template<bool be> struct commandT<cmd::VERSION_MIN_MACOSX,		be>	: version_min_command<be> {};
template<bool be> struct commandT<cmd::VERSION_MIN_IPHONEOS,	be>	: version_min_command<be> {};
template<bool be> struct commandT<cmd::VERSION_MIN_TVOS,		be>	: version_min_command<be> {};
template<bool be> struct commandT<cmd::VERSION_MIN_WATCHOS,		be>	: version_min_command<be> {};

/* The dyld_info_command contains the file offsets and sizes of the new compressed form of the information dyld needs to
 * load the image.	This information is used by dyld on Mac OS X 10.6 and later. All information pointed to by this command
 * is encoded using byte streams, so no endian swapping is needed to interpret it. */
template<bool be> struct dyldinfo_command : command<be> {
	/*Dyld rebases an image whenever dyld loads it at an address different from its preferred address.
	The rebase information is a stream of byte sized opcodes whose symbolic names start with REBASE_OPCODE_
	Conceptually the rebase information is a table of tuples:	<seg-index, seg-offset, type>
	The opcodes are a compressed way to encode the table by only encoding when a column changes. In addition simple patterns like "every n'th offset for m times" can be encoded in a few bytes
	*/
	enum {
		REBASE_TYPE_POINTER								= 1,
		REBASE_TYPE_TEXT_ABSOLUTE32						= 2,
		REBASE_TYPE_TEXT_PCREL32						= 3,

		REBASE_OPCODE_MASK								= 0xF0,
		REBASE_IMMEDIATE_MASK							= 0x0F,
		REBASE_OPCODE_DONE								= 0x00,
		REBASE_OPCODE_SET_TYPE_IMM						= 0x10,
		REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB		= 0x20,
		REBASE_OPCODE_ADD_ADDR_ULEB						= 0x30,
		REBASE_OPCODE_ADD_ADDR_IMM_SCALED				= 0x40,
		REBASE_OPCODE_DO_REBASE_IMM_TIMES				= 0x50,
		REBASE_OPCODE_DO_REBASE_ULEB_TIMES				= 0x60,
		REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB			= 0x70,
		REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB = 0x80,
	};
	table<be>	rebase;		// rebase info

	/*Dyld binds an image during the loading process if the image requires any pointers to be initialized to symbols in other images.
	The bind information is a stream of byte sized opcodes whose symbolic names start with BIND_OPCODE_.
	Conceptually the bind information is a table of tuples: <seg-index, seg-offset, type, symbol-library-ordinal, symbol-name, addend>
	The opcodes are a compressed way to encode the table by only encoding when a column changes. In addition simple patterns like for runs of pointers initialzed to the same value can be encoded in a few bytes
	*/
	enum {
		BIND_TYPE_POINTER							= 1,
		BIND_TYPE_TEXT_ABSOLUTE32					= 2,
		BIND_TYPE_TEXT_PCREL32						= 3,

		BIND_SPECIAL_DYLIB_SELF						= 0,
		BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE			= -1,
		BIND_SPECIAL_DYLIB_FLAT_LOOKUP				= -2,

		BIND_SYMBOL_FLAGS_WEAK_IMPORT				= 0x1,
		BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION		= 0x8,

		BIND_OPCODE_MASK							= 0xF0,
		BIND_IMMEDIATE_MASK							= 0x0F,
		BIND_OPCODE_DONE							= 0x00,
		BIND_OPCODE_SET_DYLIB_ORDINAL_IMM			= 0x10,
		BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB			= 0x20,
		BIND_OPCODE_SET_DYLIB_SPECIAL_IMM			= 0x30,
		BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM	= 0x40,
		BIND_OPCODE_SET_TYPE_IMM					= 0x50,
		BIND_OPCODE_SET_ADDEND_SLEB					= 0x60,
		BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB		= 0x70,
		BIND_OPCODE_ADD_ADDR_ULEB					= 0x80,
		BIND_OPCODE_DO_BIND							= 0x90,
		BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB			= 0xA0,
		BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED		= 0xB0,
		BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB = 0xC0,
	};
	table<be>	bind;	// binding info

	/*Some C++ programs require dyld to unique symbols so that all images in the process use the same copy of some code/data.
	* This step is done after binding. The content of the weak_bind info is an opcode stream like the bind_info.	But it is sorted
	* alphabetically by symbol name.	This enable dyld to walk all images with weak binding information in order and look
	* for collisions.	If there are no collisions, dyld does no updating.	That means that some fixups are also encoded
	* in the bind_info.	For instance, all calls to "operator new" are first bound to libstdc++.dylib using the information
	* in bind_info.	Then if some image overrides operator new that is detected when the weak_bind information is processed
	* and the call to operator new is then rebound. */
	table<be>	weak_bind;	// weak binding info

	/*Some uses of external symbols do not need to be bound immediately. Instead they can be lazily bound on first use.	The lazy_bind
	* are contains a stream of BIND opcodes to bind all lazy symbols. Normal use is that dyld ignores the lazy_bind section when
	* loading an image.	Instead the static linker arranged for the lazy pointer to initially point to a helper function which
	* pushes the offset into the lazy_bind area for the symbol needing to be bound, then jumps to dyld which simply adds
	* the offset to lazy_bind_off to get the information on what to bind. */
	table<be>	lazy_bind;	// lazy binding info

	/*The symbols exported by a dylib are encoded in a trie.
	This is a compact representation that factors out common prefixes and reduces LINKEDIT pages in RAM because it encodes all information (name, address, flags) in one small, contiguous range.
	The export area is a stream of nodes.The first node sequentially is the start node for the trie.

	Nodes for a symbol start with a uleb128 that is the length of the exported symbol information for the string so far.
	If there is no exported symbol, the node starts with a zero byte. If there is exported info, it follows the length.
	First is a uleb128 containing flags. Normally, it is followed by a uleb128 encoded offset which is location of the content named by the symbol from the mach_header for the image.
	If the flags is EXPORT_SYMBOL_FLAGS_REEXPORT, then following the flags is a uleb128 encoded library ordinal, then a zero terminated UTF8 string.
	If the string is zero length, then the symbol is re-export from the specified dylib with the same name.

	After the optional exported symbol information is a byte of how many edges (0-255) that this node has leaving it, followed by each edge.
	Each edge is a zero terminated UTF8 of the addition chars in the symbol, followed by a uleb128 offset for the node that edge points to.
	*/
	enum {
		KIND_MASK			= 0x03,
		KIND_REGULAR		= 0x00,
		KIND_THREAD_LOCAL	= 0x01,
		KIND_ABSOLUTE		= 0x02,
		WEAK_DEFINITION		= 0x04,
		REEXPORT			= 0x08,
		STUB_AND_RESOLVER	= 0x10,
	};
	table<be>	exprt;		// lazy binding info
};
template<bool be> struct commandT<cmd::DYLD_INFO, be>		: dyldinfo_command<be> {};
template<bool be> struct commandT<cmd::DYLD_INFO_ONLY, be>	: dyldinfo_command<be> {};


/* The symseg_command contains the offset and size of the GNU style symbol table information as described in the header file <symseg.h>.
 * The symbol roots of the symbol segments must also be aligned properly in the file.	So the requirement of keeping the offsets aligned to a
 * multiple of a 4 bytes translates to the length field of the symbol roots also being a multiple of a long.	Also the padding must again be
 * zeroed. (THIS IS OBSOLETE and no longer supported).*/
template<bool be> struct commandT<cmd::SYMSEG, be> : command<be> {
	mach_xint32	offset;		// symbol segment offset
	mach_xint32	size;		// symbol segment size in bytes
};

/* The ident_command contains a free format string table following the ident_command structure.	The strings are null terminated and the size of
 * the command is padded out with zero bytes to a multiple of 4 bytes (THIS IS OBSOLETE and no longer supported).*/
template<bool be> struct commandT<cmd::IDENT, be> : command<be> {
};

/* The fvmfile_command contains a reference to a file to be loaded at the specified virtual address.
* (Presently, this command is reserved for internal use. The kernel ignores this command when loading a program into memory).*/
template<bool be> struct commandT<cmd::FVMFILE, be> : command<be> {
	str<be>		name;			// files pathname
	mach_xint32	header_addr;	// files virtual address
};

template<bool be> struct commandT<cmd::MAIN, be> : command<be> {
    mach_xint64	entryoff;	// file (__TEXT) offset of main()
    mach_xint64	stacksize;	// if not zero, initial stack size
};

// The source_version_command is an optional load command containing the version of the sources used to build the binary.
template<bool be> struct commandT<cmd::SOURCE_VERSION, be> : command<be> {
	mach_xint64	version;	// A.B.C.D.E packed as a24.b10.c10.d10.e10
};
/*
* The build_version_command contains the min OS version on which this 
* binary was built to run for its platform.  The list of known platforms and
* tool values following it.
*/
template<bool be> struct tool_version {
	enum TOOL {
		CLANG	= 1,
		SWIFT	= 2,
		LD		= 3,
	};
	mach_xint32	tool;		// enum for the tool
	mach_xint32	version;	// version number of the tool
};
template<bool be> struct commandT<cmd::BUILD_VERSION, be> : command<be> {
	enum PLATFORM {
		MACOS				= 1,
		IOS					= 2,
		TVOS				= 3,
		WATCHOS				= 4,
		BRIDGEOS			= 5,
		MACCATALYST			= 6,
		IOSSIMULATOR		= 7,
		TVOSSIMULATOR		= 8,
		WATCHOSSIMULATOR	= 9,
		DRIVERKIT			= 10,
	};

	mach_xint32	platform;	// platform
	mach_xint32	minos;		// X.Y.Z is encoded in nibbles xxxx.yy.zz
	mach_xint32	sdk;		// X.Y.Z is encoded in nibbles xxxx.yy.zz
	mach_xint32	ntools;		// number of tool entries following this
	auto	tools() const { return iso::make_range_n((tool_version<be>*)(this + 1), ntools); }
};


// The LC_DATA_IN_CODE load commands uses a linkedit_data_command to point to an array of data_in_code_entry entries. Each entry describes a range of data in a code section.
template<bool be> struct data_in_code_entry {
	enum {
		DICE_KIND_DATA              = 0x0001,
		DICE_KIND_JUMP_TABLE8       = 0x0002,
		DICE_KIND_JUMP_TABLE16      = 0x0003,
		DICE_KIND_JUMP_TABLE32      = 0x0004,
		DICE_KIND_ABS_JUMP_TABLE32  = 0x0005,
	};
	mach_xint32	offset;		// from mach_header to start of data range
	mach_uint16	length;		// number of bytes in data range
	mach_uint16	kind;		// a DICE_KIND_* value
};

template<bool be> struct commandT<cmd::LINKER_OPTION, be> : command<be> {
	mach_xint32	count;	// number of strings
	//concatenation of zero terminated UTF8 strings, zero filled at end to align
};

template<bool be> struct commandT<cmd::NOTE, be> : command<be> {
	iso::fixed_string<16>	data_owner;	// owner name for this LC_NOTE
	mach_xint64	offset;		// file offset of this data
	mach_xint64	size;		// length of data region
};

// Sections of type S_THREAD_LOCAL_VARIABLES contain an array of tlv_descriptor structures.
template<bool be> struct tlv_descriptor {
	void*			(*thunk)(tlv_descriptor*);
	unsigned long	key;
	unsigned long	offset;
};

#undef	mach_uint16
#undef	mach_uint32
#undef	mach_uint64
#undef	mach_xint32
#undef	mach_xint64
#undef	vm_prot_t


} // namespace mach

#endif // MACH_H
