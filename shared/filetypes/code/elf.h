#ifndef ELF_H
#define ELF_H

#include "base/defs.h"

template<bool be, int bits> struct Elf_types;
template<bool be, int bits> struct Elf_Ehdr;
template<bool be, int bits> struct Elf_Phdr;
template<bool be, int bits> struct Elf_Shdr;
template<bool be, int bits> struct Elf_Sym;
template<bool be, int bits> struct Elf_Rel;
template<bool be, int bits> struct Elf_Rela;

template<bool be> struct Elf_types<be, 32> {
	typedef iso::endian_types<be>	t;
	typedef typename t::uint32	Addr;		// 4 Unsigned program address
	typedef typename t::uint32	Off;		// 4 Unsigned file offset
	typedef typename t::uint16	Half;		// 2 Unsigned medium integer
	typedef typename t::int32	Sword;		// 4 Signed large integer
	typedef typename t::uint32	Word;		// 4 Unsigned large integer
	typedef Sword				Sxword;		// 4 Unsigned program address
	typedef Word				Xword;		// 4 Unsigned program address

	typedef Elf_Ehdr<be,32>		Ehdr;
	typedef Elf_Phdr<be,32>		Phdr;
	typedef Elf_Shdr<be,32>		Shdr;
	typedef Elf_Sym<be,32>		Sym;
	typedef Elf_Rel<be,32>		Rel;
	typedef Elf_Rela<be,32>		Rela;
};

template<bool be> struct Elf_types<be, 64> {
	typedef iso::endian_types<be>	t;
	typedef typename t::uint64	Addr;		// 8 Unsigned program address
	typedef typename t::uint64	Off;		// 8 Unsigned file offset
	typedef typename t::uint16	Half;		// 2 Unsigned medium integer
	typedef typename t::int32	Sword;		// 4 Signed large integer
	typedef typename t::uint32	Word;		// 4 Unsigned large integer
	typedef typename t::int64	Sxword;		// 8 Unsigned program address
	typedef typename t::uint64	Xword;		// 8 Unsigned program address

	typedef Elf_Ehdr<be,64>		Ehdr;
	typedef Elf_Phdr<be,64>		Phdr;
	typedef Elf_Shdr<be,64>		Shdr;
	typedef Elf_Sym<be,64>		Sym;
	typedef Elf_Rel<be,64>		Rel;
	typedef Elf_Rela<be,64>		Rela;
};

#define	Elf_Addr	typename Elf_types<be,bits>::Addr
#define	Elf_Off		typename Elf_types<be,bits>::Off
#define	Elf_Half	typename Elf_types<be,bits>::Half
#define	Elf_Sword	typename Elf_types<be,bits>::Sword
#define	Elf_Word	typename Elf_types<be,bits>::Word
#define	Elf_Xword	typename Elf_types<be,bits>::Xword
#define	Elf_Sxword	typename Elf_types<be,bits>::Sxword

//--------------------	FILE HEADER

struct Elf_Ident {
	enum {MAGIC = '\177ELF'};
	iso::uint32be	magic;
	iso::uint8		file_class;
	iso::uint8		encoding;
	iso::uint8		version;
	//64 bit only
	iso::uint8		osabi;
	iso::uint8		abiversion;
	iso::uint8		pad[7];
};

enum {
	EI_MAG0			= 0,				// File identification
	EI_MAG1			= 1,				// File identification
	EI_MAG2			= 2,				// File identification
	EI_MAG3			= 3,				// File identification
	EI_CLASS		= 4,				// File class
	EI_DATA			= 5,				// Data encoding
	EI_VERSION		= 6,				// File version
	EI_PAD			= 7,				// Start of padding bytes

	EI64_OSABI		= 7,				// OS/ABI
	EI64_ABIVERSION	= 8,				// ABI version
	EI64_PAD		= 9,				// Start of padding bytes

	EI_NIDENT		= 16,				// Size of e_ident[]
};

enum {
	ELFCLASSNONE	= 0,				// Invalid class
	ELFCLASS32		= 1,				// 32­bit objects
	ELFCLASS64		= 2,				// 64­bit objects
};

enum {
	ELFDATANONE		= 0,				// Invalid data encoding
	ELFDATA2LSB		= 1,				// little endian
	ELFDATA2MSB		= 2,				// big endian
};

enum {
	ELFOSABI_SYSV	= 0,				// System V ABI
	ELFOSABI_HPUX	= 1,				// HP-UX operating system
	ELFOSABI_STANDALONE	= 255,			//Standalone (embedded)application
};

enum {
	ET_NONE			= 0,				// No file type
	ET_REL			= 1,				// Relocatable file
	ET_EXEC			= 2,				// Executable file
	ET_DYN			= 3,				// Shared object file
	ET_CORE			= 4,				// Core file
	ET_LOOS			= 0xfe00,			// Environment-specific use
	ET_HIOS			= 0xfeff,			// Environment-specific use
	ET_LOPROC		= 0xff00,			// Processor­specific
	ET_HIPROC		= 0xffff,			// Processor­specific
};

enum {
	EM_NONE		 	= 0,				// e_machine
	EM_M32		 	= 1,				// AT&T WE 32100
	EM_SPARC	 	= 2,				// Sun SPARC
	EM_386		 	= 3,				// Intel 80386
	EM_68K		 	= 4,				// Motorola 68000
	EM_88K		 	= 5,				// Motorola 88000
	EM_486		 	= 6,				// Intel 80486
	EM_860		 	= 7,				// Intel i860
	EM_MIPS		 	= 8,				// MIPS RS3000 Big-Endian
	EM_S370		 	= 9,				// IBM System/370 Processor
	EM_MIPS_RS3_LE	= 10,				// MIPS RS3000 Little-Endian
	EM_RS6000		= 11,				// RS6000
	EM_UNKNOWN12	= 12,
	EM_UNKNOWN13	= 13,
	EM_UNKNOWN14	= 14,
	EM_PA_RISC		= 15,				// PA-RISC
	EM_PARISC		= EM_PA_RISC,		// Alias: GNU compatibility
	EM_nCUBE		= 16,				// nCUBE
	EM_VPP500		= 17,				// Fujitsu VPP500
	EM_SPARC32PLUS	= 18,				// Sun SPARC 32+
	EM_960			= 19,				// Intel 80960
	EM_PPC			= 20,				// PowerPC
	EM_PPC64		= 21,				// 64-bit PowerPC
	EM_S390			= 22,				// IBM System/390 Processor
	EM_UNKNOWN22	= EM_S390,			// Alias: Older published name
	EM_SPE			= 23,
	EM_UNKNOWN24	= 24,
	EM_UNKNOWN25	= 25,
	EM_UNKNOWN26	= 26,
	EM_UNKNOWN27	= 27,
	EM_UNKNOWN28	= 28,
	EM_UNKNOWN29	= 29,
	EM_UNKNOWN30	= 30,
	EM_UNKNOWN31	= 31,
	EM_UNKNOWN32	= 32,
	EM_UNKNOWN33	= 33,
	EM_UNKNOWN34	= 34,
	EM_UNKNOWN35	= 35,
	EM_V800			= 36,				// NEX V800
	EM_FR20			= 37,				// Fujitsu FR20
	EM_RH32			= 38,				// TRW RH-32
	EM_RCE			= 39,				// Motorola RCE
	EM_ARM			= 40,				// Advanced RISC Marchines ARM
	EM_ALPHA		= 41,				// Digital Alpha
	EM_SH			= 42,				// Hitachi SH
	EM_SPARCV9		= 43,				// Sun SPARC V9 (64-bit)
	EM_TRICORE		= 44,				// Siemens Tricore embedded processor
	EM_ARC			= 45,				// Argonaut RISC Core, Argonaut Technologies Inc.
	EM_H8_300		= 46,				// Hitachi H8/300
	EM_H8_300H		= 47,				// Hitachi H8/300H
	EM_H8S			= 48,				// Hitachi H8S
	EM_H8_500		= 49,				// Hitachi H8/500
	EM_IA_64		= 50,				// Intel IA64
	EM_MIPS_X		= 51,				// Stanford MIPS-X
	EM_COLDFIRE		= 52,				// Motorola ColdFire
	EM_68HC12		= 53,				// Motorola M68HC12
	EM_MMA			= 54,				// Fujitsu MMA Mulimedia Accelerator
	EM_PCP			= 55,				// Siemens PCP
	EM_NCPU			= 56,				// Sony nCPU embedded RISC processor
	EM_NDR1			= 57,				// Denso NDR1 microprocessor
	EM_STARCORE		= 58,				// Motorola Star*Core processor
	EM_ME16			= 59,				// Toyota ME16 processor
	EM_ST100		= 60,				// STMicroelectronics ST100 processor
	EM_TINYJ		= 61,				// Advanced Logic Corp. TinyJ embedded processor family
	EM_AMD64		= 62,				// AMDs x86-64 architecture
	EM_X86_64		= EM_AMD64,			// (compatibility)
	EM_PDSP			= 63,				// Sony DSP Processor
	EM_UNKNOWN64	= 64,
	EM_UNKNOWN65	= 65,
	EM_FX66			= 66,				// Siemens FX66 microcontroller
	EM_ST9PLUS		= 67,				// STMicroelectronics ST9+8/16 bit microcontroller
	EM_ST7			= 68,				// STMicroelectronics ST7 8-bit microcontroller
	EM_68HC16		= 69,				// Motorola MC68HC16 Microcontroller
	EM_68HC11		= 70,				// Motorola MC68HC11 Microcontroller
	EM_68HC08		= 71,				// Motorola MC68HC08 Microcontroller
	EM_68HC05		= 72,				// Motorola MC68HC05 Microcontroller
	EM_SVX			= 73,				// Silicon Graphics SVx
	EM_ST19			= 74,				// STMicroelectronics ST19 8-bit microcontroller
	EM_VAX			= 75,				// Digital VAX
	EM_CRIS			= 76,				// Axis Communications 32-bit embedded processor
	EM_JAVELIN		= 77,				// Infineon Technologies 32-bit embedded processor
	EM_FIREPATH		= 78,				// Element 14 64-bit DSP Processor
	EM_ZSP			= 79,				// LSI Logic 16-bit DSP Processor
	EM_MMIX			= 80,				// Donald Knuth's educational 64-bit processor
	EM_HUANY		= 81,				// Harvard University machine-independent object files
	EM_PRISM		= 82,				// SiTera Prism
	EM_AVR			= 83,				// Atmel AVR 8-bit microcontroller
	EM_FR30			= 84,				// Fujitsu FR30
	EM_D10V			= 85,				// Mitsubishi D10V
	EM_D30V			= 86,				// Mitsubishi D30V
	EM_V850			= 87,				// NEC v850
	EM_M32R			= 88,				// Mitsubishi M32R
	EM_MN10300		= 89,				// Matsushita MN10300
	EM_MN10200		= 90,				// Matsushita MN10200
	EM_PJ			= 91,				// picoJava
	EM_OPENRISC		= 92,				// OpenRISC 32-bit embedded processor
	EM_ARC_A5		= 93,				// ARC Cores Tangent-A5
	EM_XTENSA		= 94,				// Tensilica Xtensa architecture
};

enum {
	EV_NONE			= 0,				// Invalid version
	EV_CURRENT		= 1,				// Current version
};

template<bool be, int bits> struct Elf_Ehdr {
	unsigned char	e_ident[EI_NIDENT];	//Mark the file as an object file and provide machine­independent data with which to decode and interpret the file's contents.
	Elf_Half		e_type;				//Object file type (ET_..)
	Elf_Half		e_machine;			//specifies the required architecture (EM_...)
	Elf_Word		e_version;			//object file version (EV_...)
	Elf_Addr		e_entry;			//run address
	Elf_Off			e_phoff;			//program header table's file offset
	Elf_Off			e_shoff;			//section header table's file offset
	Elf_Word		e_flags;			//processor­specific flags (EF_...)
	Elf_Half		e_ehsize;			//ELF header's size
	Elf_Half		e_phentsize;		//size of each entry in the program header table
	Elf_Half		e_phnum;			//number of entries in the program header table
	Elf_Half		e_shentsize;		//size of each section header
	Elf_Half		e_shnum;			//number of entries in the section header table
	Elf_Half		e_shstrndx;			//section header table index of section name string table
};

//--------------------	PROGRAM HEADER

enum ELF_PT {
	PT_NULL				= 0,			//Unused - nables the program header table to contain ignored entries
	PT_LOAD				= 1,			//loadable segment, described by p_filesz and p_memsz. The bytes from the file are mapped to the beginning of the memory segment
	PT_DYNAMIC			= 2,			//dynamic linking information
	PT_INTERP			= 3,			//null-terminated path name to invoke as an interpreter
	PT_NOTE				= 4,			//auxiliary information
	PT_SHLIB			= 5,			//Reserved but has unspecified semantics
	PT_PHDR				= 6,			//program header table in the file and in the memory image of the program
	PT_TLS				= 7,			//thread-local storage template
//OS-specific semantics.
	PT_LOOS				= 0x60000000,
	PT_UNWIND			= 0x6464e550,	//stack unwind tables.
	PT_EH_FRAME			= 0x6474e550,	//stack unwind table - equivalent to PT_UNWIND
	PT_GNU_STACK		= 0x6474e551,	//stack flags
	PT_GNU_RELRO		= 0x6474e552,	//read only after relocation
	PT_OS_SCE			= 0x6fffff00,
	PT_HIOS				= 0x6fffffff,
//processor-specific semantics.
	PT_LOPROC			= 0x70000000,
	PT_HIPROC			= 0x7fffffff,
};

enum ELF_PF {
	PF_X		= 0x1,					//Execute
	PF_W		= 0x2,					//Write
	PF_R		= 0x4,					//Read
	PF_MASKPROC	= 0xf0000000,			//Unspecified
};

template<bool be, int bits> struct Elf_Phdr {
	Elf_Word	p_type;					//kind of segment this array element describes
	Elf_Off		p_offset;				//offset from the beginning of the file at which the first byte of the segment resides
	Elf_Addr	p_vaddr;				//virtual address at which the first byte of the segment resides in memory
	Elf_Addr	p_paddr;				//segment's physical address (when relevant)
	Elf_Word	p_filesz;				//number of bytes in the file image of the segment
	Elf_Word	p_memsz;				//number of bytes in the memory image of the segment
	Elf_Word	p_flags;
	Elf_Word	p_align;
} ;

template<bool be> struct Elf_Phdr<be, 64> {
	enum {bits = 64};
	Elf_Word	p_type;
	Elf_Word	p_flags;
	Elf_Off		p_offset;
	Elf_Addr	p_vaddr;
	Elf_Addr	p_paddr;
	Elf_Xword	p_filesz;
	Elf_Xword	p_memsz;
	Elf_Xword	p_align;
};

//--------------------	SECTIONS

enum {
	SHN_UNDEF		= 0,				//undefined
	SHN_LORESERVE	= 0xff00,			//lower bound of the range of reserved indexes
	SHN_LOPROC		= 0xff00,			//reserved for processor­specific semantics
	SHN_HIPROC		= 0xff1f,			//	"
	SHN_LOOS		= 0xff20,			//Environment-specific use
	SHN_HIOS		= 0xff3f,			//Environment-specific use
	SHN_ABS			= 0xfff1,			//
	SHN_COMMON		= 0xfff2,			//common symbols
	SHN_HIRESERVE	= 0xffff,			//upper bound of the range of reserved indexes
};

enum ELF_SHT {
	SHT_NULL		= 0,				//section header as inactive
	SHT_PROGBITS	= 1,				//information defined by the program
	SHT_SYMTAB		= 2,				//symbol table
	SHT_STRTAB		= 3,				//string table
	SHT_RELA		= 4,				//relocation entries with explicit addends
	SHT_HASH		= 5,				//hash table
	SHT_DYNAMIC		= 6,				//information for dynamic linking
	SHT_NOTE		= 7,				//marks the file in some way
	SHT_NOBITS		= 8,				//occupies no space in file
	SHT_REL			= 9,				//relocation entries without explicit addends
	SHT_SHLIB		= 10,				//reserved
	SHT_DYNSYM		= 11,				//symbol table for linking only
	SHT_LOOS		= 0x60000000,		//Environment-specific use
	SHT_HIOS		= 0x6fffffff,		//Environment-specific use
	SHT_LOPROC		= 0x70000000,		//Processor­ specific
	SHT_HIPROC		= 0x7fffffff,	 	//Processor­ specific
	SHT_LOUSER		= 0x80000000,
	SHT_HIUSER		= 0xffffffff,

	SHT_PS3_RELA	= SHT_LOPROC + 0xa4,
};

enum {
	SHF_WRITE		= 0x1,				//contains writable data
	SHF_ALLOC		= 0x2,				//occupies memory during xecution
	SHF_EXECINSTR	= 0x4,				//contains executable machine instructions
	SHF_MASKOS		= 0x0f000000,		//environment-specific use
	SHF_MASKPROC	= 0xf0000000,		//processor­specific semantics
};

template<bool be, int bits> struct Elf_Shdr {
	Elf_Word		sh_name;			//name of the section
	Elf_Word		sh_type;			//categorizes the section's contents and semantics
	Elf_Xword		sh_flags;			//miscellaneous attributes
	Elf_Addr		sh_addr;			//address
	Elf_Off			sh_offset;			//file offset to first byte in section
	Elf_Off			sh_size;			//section's size in bytes
	Elf_Word		sh_link;			//section header table index link
	Elf_Word		sh_info;			//extra information
	Elf_Off			sh_addralign;		//address alignment constraints
	Elf_Off			sh_entsize;			//size in bytes of each entry (when appropriate)
};

//--------------------	SYMBOLS

enum ST_BINDING {
	STB_LOCAL		= 0,				//not visible outside the object file containing their definition
	STB_GLOBAL		= 1,				//visible to all object files being combined
	STB_WEAK		= 2,				//like global symbols, but lower precedence
	STB_LOOS		= 10,				//environment-specific use
	STB_HIOS		= 12,
	STB_LOPROC		= 13,
	STB_HIPROC		= 15,
};

enum ST_TYPE {
	STT_NOTYPE		= 0,				//The symbol's type is not specified
	STT_OBJECT		= 1,				//associated with a data object
	STT_FUNC		= 2,				//associated with a function
	STT_SECTION		= 3,				//associated with a section
	STT_FILE		= 4,				//name of the source file
	STT_LOOS		= 10,				//environment-specific use
	STT_HIOS		= 12,
	STT_LOPROC		= 13,
	STT_HIPROC		= 15,
};

enum ST_VISIBILITY {
	STV_DEFAULT		= 0,
	STV_INTERNAL	= 1,
	STV_HIDDEN		= 2,
	STV_PROTECTED	= 3,
};

template<bool be, int bits> struct _Elf_Sym : Elf_types<be, bits> {
	Elf_Word		st_name;			//index into the object file's symbol string table
	Elf_Addr		st_value;			//value of the associated symbol
	Elf_Word		st_size;			//associated size
	unsigned char	st_info;			//symbol's type and binding attributes
	unsigned char	st_other;			//no defined meaning (=0)
	Elf_Half		st_shndx;			//section header table index
};

template<bool be> struct _Elf_Sym<be, 64> : Elf_types<be, 64> {
	enum {bits = 64};
	Elf_Word		st_name;			//Symbol name, index in string tbl
	unsigned char	st_info;			//Type and binding attributes
	unsigned char	st_other;			//No defined meaning, 0
	Elf_Half		st_shndx;			//Associated section index
	Elf_Addr		st_value;			//Value of the symbol
	Elf_Off			st_size;			//Associated symbol size
};

template<bool be, int bits> struct Elf_Sym : _Elf_Sym<be, bits> {
	typedef	_Elf_Sym<be, bits>	B;
	ST_BINDING		binding()		const	{ return ST_BINDING(B::st_info >> 4);		}
	ST_TYPE			type()			const	{ return ST_TYPE(B::st_info & 15);			}
	ST_VISIBILITY	visibility()	const	{ return ST_VISIBILITY(B::st_other & 3);	}

	void			set(ST_BINDING b, ST_TYPE t, ST_VISIBILITY v = STV_DEFAULT)	{
		B::st_info = (b << 4) + (t & 0xf);
		B::st_other = v;
	}
};

//--------------------	DYNAMIC

enum DT_TAG {
	//Name				Value			d_un		Executable	Shared Object
	DT_NULL				= 0,			//ignored	mandatory mandatory
	DT_NEEDED			= 1,			//d_val		optional optional
	DT_PLTRELSZ			= 2,			//d_val		optional optional
	DT_PLTGOT			= 3,			//d_ptr		optional optional
	DT_HASH				= 4,			//d_ptr		mandatory mandatory
	DT_STRTAB			= 5,			//d_ptr		mandatory mandatory
	DT_SYMTAB			= 6,			//d_ptr		mandatory mandatory
	DT_RELA				= 7,			//d_ptr		mandatory optional
	DT_RELASZ			= 8,			//d_val		mandatory optional
	DT_RELAENT			= 9,			//d_val		mandatory optional
	DT_STRSZ			= 10,			//d_val		mandatory mandatory
	DT_SYMENT			= 11,			//d_val		mandatory mandatory
	DT_INIT				= 12,			//d_ptr		optional optional
	DT_FINI				= 13,			//d_ptr		optional optional
	DT_SONAME			= 14,			//d_val		ignored optional
	DT_RPATH			= 15,			//d_val		optional ignored (LEVEL2)
	DT_SYMBOLIC			= 16,			//ignored	ignored optional  (LEVEL2)
	DT_REL				= 17,			//d_ptr		mandatory optional
	DT_RELSZ			= 18,			//d_val		mandatory optional
	DT_RELENT			= 19,			//d_val		mandatory optional
	DT_PLTREL			= 20,			//d_val		optional optional
	DT_DEBUG			= 21,			//d_ptr		optional ignored
	DT_TEXTREL			= 22,			//ignored	optional optional (LEVEL2)
	DT_JMPREL			= 23,			//d_ptr		optional optional
	DT_BIND_NOW			= 24,			//ignored	optional optional (LEVEL2)
	DT_INIT_ARRAY		= 25,			//d_ptr		optional optional
	DT_FINI_ARRAY		= 26,			//d_ptr		optional optional
	DT_INIT_ARRAYSZ		= 27,			//d_val		optional optional
	DT_FINI_ARRAYSZ		= 28,			//d_val		optional optional
	DT_RUNPATH			= 29,			//d_val		optional optional
	DT_FLAGS			= 30,			//d_val		optional optional
	DT_ENCODING			= 32,			//unspecified unspecified unspecified
	DT_PREINIT_ARRAY	= 32,			//d_ptr		optional ignored
	DT_PREINIT_ARRAYSZ	= 33,			//d_val		optional ignored
	DT_LOOS				= 0x6000000D,	//unspecified unspecified unspecified
	DT_HIOS				= 0x6ffff000,	//unspecified unspecified unspecified
	DT_LOPROC			= 0x70000000,	//unspecified unspecified unspecified
	DT_HIPROC			= 0x7fffffff,	//unspecified unspecified unspecified
};

enum DT_FLAGS {
	DF_ORIGIN		= 0x1,
	DF_SYMBOLIC		= 0x2,
	DF_TEXTREL		= 0x4,
	DF_BIND_NOW		= 0x8,
	DF_STATIC_TLS	= 0x10,
};

template<bool be, int bits> struct Elf_Dyn : Elf_types<be, bits> {
	Elf_Sxword	d_tag;
   	union {
   		Elf_Xword	d_val;
   		Elf_Addr	d_ptr;
	} d_un;
};

//--------------------	RELOC

inline iso::uint32	ELF_TOP(iso::uint32 i)		{ return i >> 8;		}
inline iso::uint32	ELF_TOP(iso::uint64 i)		{ return i >> 32;		}
inline iso::uint8	ELF_BOTTOM(iso::uint32 i)	{ return iso::uint8(i);	}
inline iso::uint32	ELF_BOTTOM(iso::uint64 i)	{ return iso::uint32(i);}

template<int bits> inline iso::uint_bits_t<bits>	ELF_TOPBOTTOM(iso::uint32 t, iso::uint32 b);
template<> inline	iso::uint32	ELF_TOPBOTTOM<32>(iso::uint32 t, iso::uint32 b)	{ return (t << 8) + (b & 0xff); }
template<> inline	iso::uint64	ELF_TOPBOTTOM<64>(iso::uint32 t, iso::uint32 b)	{ return (iso::uint64(t) << 32) + b; }

template<bool be, int bits> struct Elf_Rel {
	Elf_Addr		r_offset;
	Elf_Xword		r_info;
	iso::uint32		symbol()		const	{ return ELF_TOP(r_info);		}
	iso::uint32		type()			const	{ return ELF_BOTTOM(r_info);	}
	void			set(iso::uint32 sym, iso::uint32 type)	{ r_info = ELF_TOPBOTTOM<bits>(sym, type);	}
};

template<bool be, int bits> struct Elf_Rela : Elf_Rel<be, bits> {
	Elf_Sxword		r_addend;
};

enum ELF_RELOC_386 {
	R_386_NONE				= 0,
	R_386_32				= 1,
	R_386_PC32				= 2,
	R_386_GOT32				= 3,
	R_386_PLT32				= 4,
	R_386_COPY				= 5,
	R_386_GLOB_DAT			= 6,
	R_386_JMP_SLOT			= 7,
	R_386_RELATIVE			= 8,
	R_386_GOTOFF			= 9,
	R_386_GOTPC				= 10,
	R_386_32PLT				= 11,
	R_386_TLS_GD_PLT		= 12,
	R_386_TLS_LDM_PLT		= 13,
	R_386_TLS_TPOFF			= 14,
	R_386_TLS_IE			= 15,
	R_386_TLS_GOTIE			= 16,
	R_386_TLS_LE			= 17,
	R_386_TLS_GD			= 18,
	R_386_TLS_LDM			= 19,
	R_386_16				= 20,
	R_386_PC16				= 21,
	R_386_8					= 22,
	R_386_PC8				= 23,
	R_386_UNKNOWN24			= 24,
	R_386_UNKNOWN25			= 25,
	R_386_UNKNOWN26			= 26,
	R_386_UNKNOWN27			= 27,
	R_386_UNKNOWN28			= 28,
	R_386_UNKNOWN29			= 29,
	R_386_UNKNOWN30			= 30,
	R_386_UNKNOWN31			= 31,
	R_386_TLS_LDO_32		= 32,
	R_386_UNKNOWN33			= 33,
	R_386_UNKNOWN34			= 34,
	R_386_TLS_DTPMOD32		= 35,
	R_386_TLS_DTPOFF32		= 36,
	R_386_UNKNOWN37			= 37,
	R_386_SIZE32			= 38,
	R_386_NUM				= 39
};

enum ELF_RELOC_PPC {
	R_PPC_NONE				= 0,
	R_PPC_ADDR32			= 1,
	R_PPC_ADDR24			= 2,
	R_PPC_ADDR16			= 3,
	R_PPC_ADDR16_LO			= 4,
	R_PPC_ADDR16_HI			= 5,
	R_PPC_ADDR16_HA			= 6,
	R_PPC_ADDR14			= 7,
	R_PPC_ADDR14_BRTAKEN	= 8,
	R_PPC_ADDR14_BRNTAKEN	= 9,
	R_PPC_REL24				= 10,
	R_PPC_REL14				= 11,
	R_PPC_REL14_BRTAKEN		= 12,
	R_PPC_REL14_BRNTAKEN	= 13,
	R_PPC_GOT16				= 14,
	R_PPC_GOT16_LO			= 15,
	R_PPC_GOT16_HI			= 16,
	R_PPC_GOT16_HA			= 17,
	R_PPC_PLTREL24			= 18,
	R_PPC_COPY				= 19,
	R_PPC_GLOB_DAT			= 20,
	R_PPC_JMP_SLOT			= 21,
	R_PPC_RELATIVE			= 22,
	R_PPC_LOCAL24PC			= 23,
	R_PPC_UADDR32			= 24,
	R_PPC_UADDR16			= 25,
	R_PPC_REL32				= 26,
	R_PPC_PLT32				= 27,
	R_PPC_PLTREL32			= 28,
	R_PPC_PLT16_LO			= 29,
	R_PPC_PLT16_HI			= 30,
	R_PPC_PLT16_HA			= 31,
	R_PPC_SDAREL16			= 32,
	R_PPC_SECTOFF			= 33,
	R_PPC_SECTOFF_LO		= 34,
	R_PPC_SECTOFF_HI		= 35,
	R_PPC_SECTOFF_HA		= 36,
	R_PPC_ADDR30			= 37,
// Relocs added to support TLS.
	R_PPC_TLS				= 67,
	R_PPC_DTPMOD32			= 68,
	R_PPC_TPREL16			= 69,
	R_PPC_TPREL16_LO		= 70,
	R_PPC_TPREL16_HI		= 71,
	R_PPC_TPREL16_HA		= 72,
	R_PPC_TPREL32			= 73,
	R_PPC_DTPREL16			= 74,
	R_PPC_DTPREL16_LO		= 75,
	R_PPC_DTPREL16_HI		= 76,
	R_PPC_DTPREL16_HA		= 77,
	R_PPC_DTPREL32			= 78,
	R_PPC_GOT_TLSGD16		= 79,
	R_PPC_GOT_TLSGD16_LO	= 80,
	R_PPC_GOT_TLSGD16_HI	= 81,
	R_PPC_GOT_TLSGD16_HA	= 82,
	R_PPC_GOT_TLSLD16		= 83,
	R_PPC_GOT_TLSLD16_LO	= 84,
	R_PPC_GOT_TLSLD16_HI	= 85,
	R_PPC_GOT_TLSLD16_HA	= 86,
	R_PPC_GOT_TPREL16		= 87,
	R_PPC_GOT_TPREL16_LO	= 88,
	R_PPC_GOT_TPREL16_HI	= 89,
	R_PPC_GOT_TPREL16_HA	= 90,
	R_PPC_GOT_DTPREL16		= 91,
	R_PPC_GOT_DTPREL16_LO	= 92,
	R_PPC_GOT_DTPREL16_HI	= 93,
	R_PPC_GOT_DTPREL16_HA	= 94,
	// The remaining relocs are from the Embedded ELF ABI and are not in the SVR4 ELF ABI
	R_PPC_EMB_NADDR32		= 101,
	R_PPC_EMB_NADDR16		= 102,
	R_PPC_EMB_NADDR16_LO	= 103,
	R_PPC_EMB_NADDR16_HI	= 104,
	R_PPC_EMB_NADDR16_HA	= 105,
	R_PPC_EMB_SDAI16		= 106,
	R_PPC_EMB_SDA2I16		= 107,
	R_PPC_EMB_SDA2REL		= 108,
	R_PPC_EMB_SDA21			= 109,
	R_PPC_EMB_MRKREF		= 110,
	R_PPC_EMB_RELSEC16		= 111,
	R_PPC_EMB_RELST_LO		= 112,
	R_PPC_EMB_RELST_HI		= 113,
	R_PPC_EMB_RELST_HA		= 114,
	R_PPC_EMB_BIT_FLD		= 115,
	R_PPC_EMB_RELSDA		= 116,
};

enum ELF_RELOC_PPC64 {
	R_PPC64_NONE				= 0,
	R_PPC64_ADDR32				= 1,
	R_PPC64_ADDR24				= 2,
	R_PPC64_ADDR16				= 3,
	R_PPC64_ADDR16_LO			= 4,
	R_PPC64_ADDR16_HI			= 5,
	R_PPC64_ADDR16_HA			= 6,
	R_PPC64_ADDR14				= 7,
	R_PPC64_ADDR14_BRTAKEN		= 8,
	R_PPC64_ADDR14_BRNTAKEN		= 9,
	R_PPC64_REL24				= 10,
	R_PPC64_REL14				= 11,
	R_PPC64_REL14_BRTAKEN		= 12,
	R_PPC64_REL14_BRNTAKEN		= 13,
	R_PPC64_GOT16				= 14,
	R_PPC64_GOT16_LO			= 15,
	R_PPC64_GOT16_HI			= 16,
	R_PPC64_GOT16_HA			= 17,
	// 18 unused.	= 32-bit reloc is R_PPC_PLTREL24.
	R_PPC64_COPY				= 19,
	R_PPC64_GLOB_DAT			= 20,
	R_PPC64_JMP_SLOT			= 21,
	R_PPC64_RELATIVE			= 22,
	// 23 unused.	= 32-bit reloc is R_PPC_LOCAL24PC.
	R_PPC64_UADDR32				= 24,
	R_PPC64_UADDR16				= 25,
	R_PPC64_REL32				= 26,
	R_PPC64_PLT32				= 27,
	R_PPC64_PLTREL32			= 28,
	R_PPC64_PLT16_LO			= 29,
	R_PPC64_PLT16_HI			= 30,
	R_PPC64_PLT16_HA			= 31,
	// 32 unused.	= 32-bit reloc is R_PPC_SDAREL16.
	R_PPC64_SECTOFF				= 33,
	R_PPC64_SECTOFF_LO			= 34,
	R_PPC64_SECTOFF_HI			= 35,
	R_PPC64_SECTOFF_HA			= 36,
	R_PPC64_REL30				= 37,
	R_PPC64_ADDR64				= 38,
	R_PPC64_ADDR16_HIGHER		= 39,
	R_PPC64_ADDR16_HIGHERA		= 40,
	R_PPC64_ADDR16_HIGHEST		= 41,
	R_PPC64_ADDR16_HIGHESTA		= 42,
	R_PPC64_UADDR64				= 43,
	R_PPC64_REL64				= 44,
	R_PPC64_PLT64				= 45,
	R_PPC64_PLTREL64			= 46,
	R_PPC64_TOC16				= 47,
	R_PPC64_TOC16_LO			= 48,
	R_PPC64_TOC16_HI			= 49,
	R_PPC64_TOC16_HA			= 50,
	R_PPC64_TOC					= 51,
	R_PPC64_PLTGOT16			= 52,
	R_PPC64_PLTGOT16_LO			= 53,
	R_PPC64_PLTGOT16_HI			= 54,
	R_PPC64_PLTGOT16_HA			= 55,
	// The following relocs were added in the 64-bit PowerPC ELF ABI revision 1.2.
	R_PPC64_ADDR16_DS			= 56,
	R_PPC64_ADDR16_LO_DS		= 57,
	R_PPC64_GOT16_DS			= 58,
	R_PPC64_GOT16_LO_DS			= 59,
	R_PPC64_PLT16_LO_DS			= 60,
	R_PPC64_SECTOFF_DS			= 61,
	R_PPC64_SECTOFF_LO_DS		= 62,
	R_PPC64_TOC16_DS			= 63,
	R_PPC64_TOC16_LO_DS			= 64,
	R_PPC64_PLTGOT16_DS			= 65,
	R_PPC64_PLTGOT16_LO_DS		= 66,
	// Relocs added to support TLS.	PowerPC64 ELF ABI revision 1.5.
	R_PPC64_TLS					= 67,
	R_PPC64_DTPMOD64			= 68,
	R_PPC64_TPREL16				= 69,
	R_PPC64_TPREL16_LO			= 70,
	R_PPC64_TPREL16_HI			= 71,
	R_PPC64_TPREL16_HA			= 72,
	R_PPC64_TPREL64				= 73,
	R_PPC64_DTPREL16			= 74,
	R_PPC64_DTPREL16_LO			= 75,
	R_PPC64_DTPREL16_HI			= 76,
	R_PPC64_DTPREL16_HA			= 77,
	R_PPC64_DTPREL64			= 78,
	R_PPC64_GOT_TLSGD16			= 79,
	R_PPC64_GOT_TLSGD16_LO		= 80,
	R_PPC64_GOT_TLSGD16_HI		= 81,
	R_PPC64_GOT_TLSGD16_HA		= 82,
	R_PPC64_GOT_TLSLD16			= 83,
	R_PPC64_GOT_TLSLD16_LO		= 84,
	R_PPC64_GOT_TLSLD16_HI		= 85,
	R_PPC64_GOT_TLSLD16_HA		= 86,
	R_PPC64_GOT_TPREL16_DS		= 87,
	R_PPC64_GOT_TPREL16_LO_DS	= 88,
	R_PPC64_GOT_TPREL16_HI		= 89,
	R_PPC64_GOT_TPREL16_HA		= 90,
	R_PPC64_GOT_DTPREL16_DS		= 91,
	R_PPC64_GOT_DTPREL16_LO_DS	= 92,
	R_PPC64_GOT_DTPREL16_HI		= 93,
	R_PPC64_GOT_DTPREL16_HA		= 94,
	R_PPC64_TPREL16_DS			= 95,
	R_PPC64_TPREL16_LO_DS		= 96,
	R_PPC64_TPREL16_HIGHER		= 97,
	R_PPC64_TPREL16_HIGHERA		= 98,
	R_PPC64_TPREL16_HIGHEST		= 99,
	R_PPC64_TPREL16_HIGHESTA	= 100,
	R_PPC64_DTPREL16_DS			= 101,
	R_PPC64_DTPREL16_LO_DS		= 102,
	R_PPC64_DTPREL16_HIGHER		= 103,
	R_PPC64_DTPREL16_HIGHERA	= 104,
	R_PPC64_DTPREL16_HIGHEST	= 105,
	R_PPC64_DTPREL16_HIGHESTA	= 106,
	// These are GNU extensions to enable C++ vtable garbage collection.
	R_PPC64_GNU_VTINHERIT		= 253,
	R_PPC64_GNU_VTENTRY			= 254,
};

enum {
	R_AMD64_NONE			= 0,	// No reloc
	R_AMD64_64				= 1,	// Direct 64 bit
	R_AMD64_PC32			= 2,	// PC relative 32 bit signed
	R_AMD64_GOT32			= 3,	// 32 bit GOT entry
	R_AMD64_PLT32			= 4,	// 32 bit PLT address
	R_AMD64_COPY			= 5,	// Copy symbol at runtime
	R_AMD64_GLOB_DAT		= 6,	// Create GOT entry
	R_AMD64_JUMP_SLOT		= 7,	// Create PLT entry
	R_AMD64_RELATIVE		= 8,	// Adjust by program base
	R_AMD64_GOTPCREL		= 9,	// 32 bit signed PC relative offset to GOT
	R_AMD64_32				= 10,	// Direct 32 bit zero extended
	R_AMD64_32S				= 11,	// Direct 32 bit sign extended
	R_AMD64_16				= 12,	// Direct 16 bit zero extended
	R_AMD64_PC16			= 13,	// 16 bit sign extended pc relative
	R_AMD64_8				= 14,	// Direct 8 bit sign extended
	R_AMD64_PC8				= 15,	// 8 bit sign extended pc relative

	// TLS relocations
	R_AMD64_DTPMOD64		= 16,	// ID of module containing symbol
	R_AMD64_DTPOFF64		= 17,	// Offset in module's TLS block
	R_AMD64_TPOFF64			= 18,	// Offset in initial TLS block
	R_AMD64_TLSGD			= 19,	// 32 bit signed PC relative offset to two GOT entries for GD symbol
	R_AMD64_TLSLD			= 20,	// 32 bit signed PC relative offset to two GOT entries for LD symbol
	R_AMD64_DTPOFF32		= 21,	// Offset in TLS block
	R_AMD64_GOTTPOFF		= 22,	// 32 bit signed PC relative offset to GOT entry for IE symbol
	R_AMD64_TPOFF32			= 23,	// Offset in initial TLS block

	R_AMD64_PC64			= 24,	// 64-bit PC relative
	R_AMD64_GOTOFF64		= 25,	// 64-bit GOT offset
	R_AMD64_GOTPC32			= 26,	// 32-bit PC relative offset to GOT

	R_AMD64_GOT64			= 27,	// 64-bit GOT entry offset
	R_AMD64_GOTPCREL64		= 28,	// 64-bit PC relative offset to GOT entry
	R_AMD64_GOTPC64			= 29,	// 64-bit PC relative offset to GOT
	R_AMD64_GOTPLT64		= 30,	// Like GOT64, indicates that PLT entry needed
	R_AMD64_PLTOFF64		= 31,	// 64-bit GOT relative offset to PLT entry

	R_AMD64_SIZE32			= 32,
	R_AMD64_SIZE64			= 33,

	R_AMD64_GOTPC32_TLSDESC	= 34,	// 32-bit PC relative to TLS descriptor in GOT
	R_AMD64_TLSDESC_CALL	= 35,	// Relaxable call through TLS descriptor
	R_AMD64_TLSDESC			= 36,	// 2 by 64-bit TLS descriptor
	R_AMD64_IRELATIVE		= 37,	// Adjust indirectly by program base
	// GNU vtable garbage collection extensions.
	R_AMD64_GNU_VTINHERIT	= 250,
	R_AMD64_GNU_VTENTRY		= 251
};

#endif	//ELF_H