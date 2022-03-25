#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "obj.h"
#include "mach.h"
#include "container/archive_help.h"
#include "bin.h"
#include "dwarf.h"

//-----------------------------------------------------------------------------
//	Apple Executable
//-----------------------------------------------------------------------------

using namespace iso;

#define	MACH_TYPE(x)	typename mach::x<be>
#define	MACH_TYPEDEF(x)	typedef MACH_TYPE(x)	x

using StartBinBlock	= ISO::VStartBin<const_memory_block>;

template<bool be> struct ISO::def<mach::str<be> > : ISO::VirtualT2<mach::str<be> > {
	static ISO::Browser2	Deref(const mach::str<be> &s)	{ return ISO_ptr<const char*>(0, (char*)&s + s.offset - sizeof(mach::command<be>)); }
};

//template<bool be> struct ISO_def<param_element<mach::str<be>, void*> > : ISO::VirtualT2<param_element<mach::str<be>, void*> > {
//	static ISO::Browser2	Deref(const param_element<mach::str<be>, void*> &s)	{ return ISO_ptr<const char*>(0, (char*)s.p + s->offset); }
//};

#define ISO_DEFUSERCOMP_MACH(...)		template<bool be>			_ISO_DEFUSERCOMP(mach::VA_HEAD(__VA_ARGS__)<be>, VA_NUM(__VA_ARGS__)-1, STRINGIFY2(VA_HEAD(__VA_ARGS__)), NONE)	{ CONCAT3(ISO_SETFIELDS_EXP, VA_MORE(__VA_ARGS__))(0 VA_TAIL(__VA_ARGS__)); } }
#define ISO_DEFUSERCOMP_MACHB(S,...)	template<bool be, int bits> _ISO_DEFUSERCOMPV((mach::S<be, bits>), VA_NUM(__VA_ARGS__), #S, NONE)							{ ISO_SETFIELDS(0, __VA_ARGS__); } }
#define ISO_DEFUSERCOMP_MACHC(C,...)	template<bool be>			_ISO_DEFUSERCOMPV((mach::commandT<mach::cmd::C, be>), VA_NUM(__VA_ARGS__), #C "_command", NONE)	{ ISO_SETFIELDS(0, __VA_ARGS__); } }

#define ISO_DEFUSER_MACHC(C,T)	template<bool be> _ISO_DEFUSER2((mach::commandT<mach::cmd::C, be>), mach::T<be>, #C "_command", NONE)

ISO_DEFUSERCOMP_MACHB(header,		cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags);
//ISO_DEFUSERCOMP_MACH(str,			offset);
ISO_DEFUSERCOMP_MACH(table,			offset, size);
ISO_DEFUSERCOMP_MACHB(nlist,		flags, sect, desc, value);
ISO_DEFUSERCOMP_MACH(tool_version,	tool, version);

ISO_DEFUSERENUMXFQV(mach::cmd, "cmd", 32, NONE, REQ_DYLD);

ISO_DEFUSERENUMXF(mach::nlist_enums::FLAGS,35,"FLAGS",8,NONE) {
	ISO_SETENUMSQ(0,
		PEXT, EXT, UNDF, ABS, INDR, PBUD, SECT, GSYM,
		PEXT, EXT, UNDF, ABS, INDR, PBUD, SECT, GSYM,
		FNAME, FUN, STSYM, LCSYM, BNSYM, OPT, RSYM, SLINE,
		ENSYM, SSYM, SO, OSO, LSYM, BINCL, SOL, PARAMS
	);
	ISO_SETENUMSQ(32,
		VERSION, OLEVEL, PSYM, EINCL, ENTRY, LBRAC, EXCL, RBRAC,
		BCOMM, ECOMM, ECOML, LENG
	);
}};
ISO_DEFUSERENUMXFQV(mach::nlist_enums::DESC,"DESC",8,NONE,
	REF_MASK, REF_UNDEFINED_NON_LAZY, REF_UNDEFINED_LAZY, REF_DEFINED, REF_PRIVATE_DEFINED, REF_PRIVATE_UNDEFINED_NON_LAZY, REF_PRIVATE_UNDEFINED_LAZY, REF_DYNAMIC,
	NO_DEAD_STRIP, DESC_DISCARDED, WEAK_REF, WEAK_DEF, REF_TO_WEAK, ARM_THUMB_DEF, SYMBOL_RESOLVER, ALIGN,
	MAX_LIBRARY_ORDINAL, DYNAMIC_LOOKUP_ORDINAL, EXECUTABLE_ORDINAL
);

ISO_DEFUSERCOMP_MACH(command,				cmd, cmdsize);
ISO_DEFUSERCOMP_MACH(str_command,			name);
ISO_DEFUSERCOMP_MACHB(section,				sectname, segname, addr, size, offset, align, reloff, nreloc, flags, reserved1, reserved2);
ISO_DEFUSERCOMP_MACH(fvmlib,				name, minor_version, header_addr);
ISO_DEFUSERCOMP_MACH(fvmlib_command,		lib);
ISO_DEFUSERCOMP_MACH(dylib,					name, timestamp, current_version, compatibility_version);
ISO_DEFUSERCOMP_MACH(dylib_command,			name, timestamp, current_version, compatibility_version);
ISO_DEFUSERCOMP_MACH(linkedit_data_command,	dataoff, datasize);
ISO_DEFUSERCOMP_MACH(version_min_command,	version, reserved);
ISO_DEFUSERCOMP_MACH(thread_command);	
ISO_DEFUSERCOMP_MACH(dyldinfo_command,		rebase, bind, weak_bind, lazy_bind, exprt);

ISO_DEFUSERCOMP_MACHC(SEGMENT,				segname, vmaddr, vmsize, fileoff, filesize, maxprot, initprot, flags, sections);
ISO_DEFUSERCOMP_MACHC(SEGMENT_64,			segname, vmaddr, vmsize, fileoff, filesize, maxprot, initprot, flags, sections);
ISO_DEFUSERCOMP_MACHC(PREBOUND_DYLIB,		name, nmodules, linked_modules);
ISO_DEFUSERCOMP_MACHC(ROUTINES,				init_address, init_module, reserved1, reserved2, reserved3, reserved4, reserved5, reserved6);
ISO_DEFUSERCOMP_MACHC(ROUTINES_64,			init_address, init_module, reserved1, reserved2, reserved3, reserved4, reserved5, reserved6);
ISO_DEFUSERCOMP_MACHC(SYMTAB,				sym, str);
ISO_DEFUSERCOMP_MACHC(DYSYMTAB,				localsym, extdefsym, undefsym, toc, modtab, extrefsym, indirectsym, extrel, locrel);
ISO_DEFUSERCOMP_MACHC(TWOLEVEL_HINTS,		offset, nhints);
ISO_DEFUSERCOMP_MACHC(PREBIND_CKSUM,		cksum);
ISO_DEFUSERCOMP_MACHC(UUID,					uuid);
ISO_DEFUSERCOMP_MACHC(ENCRYPTION_INFO,		cryptoff, cryptsize, cryptid);
ISO_DEFUSERCOMP_MACHC(ENCRYPTION_INFO_64,	cryptoff, cryptsize, cryptid);
ISO_DEFUSERCOMP_MACHC(SYMSEG,				offset, size);
ISO_DEFUSERCOMP_MACHC(IDENT,				cmd, cmdsize);
ISO_DEFUSERCOMP_MACHC(FVMFILE,				name, header_addr);
ISO_DEFUSERCOMP_MACHC(SOURCE_VERSION,		version);
ISO_DEFUSERCOMP_MACHC(MAIN,					entryoff, stacksize);
ISO_DEFUSERCOMP_MACHC(LINKER_OPTION,		count);
ISO_DEFUSERCOMP_MACHC(NOTE,					data_owner, offset, size);
ISO_DEFUSERCOMP_MACHC(BUILD_VERSION,		platform, minos, sdk, tools);

ISO_DEFUSER_MACHC(THREAD,				thread_command);
ISO_DEFUSER_MACHC(UNIXTHREAD,			thread_command);
ISO_DEFUSER_MACHC(LOADFVMLIB,			fvmlib_command);
ISO_DEFUSER_MACHC(IDFVMLIB,				fvmlib_command);
ISO_DEFUSER_MACHC(LOAD_DYLIB,			dylib_command);
ISO_DEFUSER_MACHC(ID_DYLIB,				dylib_command);
ISO_DEFUSER_MACHC(LOAD_DYLINKER,		str_command);
ISO_DEFUSER_MACHC(ID_DYLINKER,			str_command);
ISO_DEFUSER_MACHC(SUB_FRAMEWORK,		str_command);
ISO_DEFUSER_MACHC(SUB_UMBRELLA,			str_command);
ISO_DEFUSER_MACHC(SUB_CLIENT,			str_command);
ISO_DEFUSER_MACHC(SUB_LIBRARY,			str_command);
ISO_DEFUSER_MACHC(LOAD_WEAK_DYLIB,		dylib_command);
ISO_DEFUSER_MACHC(RPATH,				str_command);
ISO_DEFUSER_MACHC(CODE_SIGNATURE,		linkedit_data_command);
ISO_DEFUSER_MACHC(SEGMENT_SPLIT_INFO,	linkedit_data_command);
ISO_DEFUSER_MACHC(REEXPORT_DYLIB,		dylib_command);
ISO_DEFUSER_MACHC(LAZY_LOAD_DYLIB,		dylib_command);
ISO_DEFUSER_MACHC(DYLD_INFO,			dyldinfo_command);
ISO_DEFUSER_MACHC(DYLD_INFO_ONLY,		dyldinfo_command);
ISO_DEFUSER_MACHC(LOAD_UPWARD_DYLIB,	dylib_command);
ISO_DEFUSER_MACHC(VERSION_MIN_MACOSX,	version_min_command);
ISO_DEFUSER_MACHC(VERSION_MIN_IPHONEOS,	version_min_command);
ISO_DEFUSER_MACHC(FUNCTION_STARTS,		linkedit_data_command);
ISO_DEFUSER_MACHC(DYLD_ENVIRONMENT,		str_command);
ISO_DEFUSER_MACHC(DATA_IN_CODE,			linkedit_data_command);
ISO_DEFUSER_MACHC(DYLIB_CODE_SIGN_DRS,	linkedit_data_command);
ISO_DEFUSER_MACHC(LINKER_OPTIMIZATION_HINT,linkedit_data_command);
ISO_DEFUSER_MACHC(VERSION_MIN_TVOS,		version_min_command);
ISO_DEFUSER_MACHC(VERSION_MIN_WATCHOS,	version_min_command);
ISO_DEFUSER_MACHC(DYLD_EXPORTS_TRIE,	linkedit_data_command);
ISO_DEFUSER_MACHC(DYLD_CHAINED_FIXUPS,	linkedit_data_command);


template<bool be> struct MACH_commands : ISO::VirtualDefaults {
	const_memory_block		cmds;
	dynamic_array<uint32>	offsets;

	MACH_commands(const const_memory_block &block) : cmds(block) {}
	
	void Init(uint32 ncmds) {
		offsets.resize(ncmds);
		const uint8	*p = cmds;
		for (int i = 0, n = ncmds; i < n; i++) {
			offsets[i]	= uint32(p - (const uint8*)cmds);
			p			+= ((mach::command<be>*)p)->cmdsize;
		}
	}

	uint32 Count() {
		return offsets.size32();
	}

	ISO::Browser2 Index(int i) {
		const mach::command<be>	*cmd = cmds + offsets[i];
		switch (cmd->cmd) {
			case mach::cmd::SEGMENT:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::SEGMENT>());
			case mach::cmd::SYMTAB:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::SYMTAB>());
			case mach::cmd::SYMSEG:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::SYMSEG>());
			case mach::cmd::THREAD:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::THREAD>());
			case mach::cmd::UNIXTHREAD:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::UNIXTHREAD>());
			case mach::cmd::LOADFVMLIB:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::LOADFVMLIB>());
			case mach::cmd::IDFVMLIB:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::IDFVMLIB>());
			case mach::cmd::IDENT:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::IDENT>());
//			case mach::cmd::FVMFILE:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::FVMFILE>());
//			case mach::cmd::PREPAGE:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::PREPAGE>());
			case mach::cmd::DYSYMTAB:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::DYSYMTAB>());
			case mach::cmd::LOAD_DYLIB:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::LOAD_DYLIB>());
			case mach::cmd::ID_DYLIB:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::ID_DYLIB>());
			case mach::cmd::LOAD_DYLINKER:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::LOAD_DYLINKER>());
			case mach::cmd::ID_DYLINKER:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::ID_DYLINKER>());
			case mach::cmd::PREBOUND_DYLIB:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::PREBOUND_DYLIB>());
			case mach::cmd::ROUTINES:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::ROUTINES>());
			case mach::cmd::SUB_FRAMEWORK:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::SUB_FRAMEWORK>());
			case mach::cmd::SUB_UMBRELLA:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::SUB_UMBRELLA>());
			case mach::cmd::SUB_CLIENT:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::SUB_CLIENT>());
			case mach::cmd::SUB_LIBRARY:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::SUB_LIBRARY>());
			case mach::cmd::TWOLEVEL_HINTS:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::TWOLEVEL_HINTS>());
			case mach::cmd::PREBIND_CKSUM:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::PREBIND_CKSUM>());
			case mach::cmd::LOAD_WEAK_DYLIB:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::LOAD_WEAK_DYLIB>());
			case mach::cmd::SEGMENT_64:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::SEGMENT_64>());
			case mach::cmd::ROUTINES_64:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::ROUTINES_64>());
			case mach::cmd::UUID:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::UUID>());
			case mach::cmd::RPATH:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::RPATH>());
			case mach::cmd::CODE_SIGNATURE:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::CODE_SIGNATURE>());
			case mach::cmd::SEGMENT_SPLIT_INFO:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::SEGMENT_SPLIT_INFO>());
			case mach::cmd::REEXPORT_DYLIB:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::REEXPORT_DYLIB>());
			case mach::cmd::LAZY_LOAD_DYLIB:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::LAZY_LOAD_DYLIB>());
			case mach::cmd::ENCRYPTION_INFO:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::ENCRYPTION_INFO>());
			case mach::cmd::DYLD_INFO:				return ISO::MakeBrowser(*cmd->template as<mach::cmd::DYLD_INFO>());
			case mach::cmd::DYLD_INFO_ONLY:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::DYLD_INFO_ONLY>());
			case mach::cmd::LOAD_UPWARD_DYLIB:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::LOAD_UPWARD_DYLIB>());
			case mach::cmd::VERSION_MIN_MACOSX:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::VERSION_MIN_MACOSX>());
			case mach::cmd::VERSION_MIN_IPHONEOS:	return ISO::MakeBrowser(*cmd->template as<mach::cmd::VERSION_MIN_IPHONEOS>());
			case mach::cmd::FUNCTION_STARTS:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::FUNCTION_STARTS>());
			case mach::cmd::DYLD_ENVIRONMENT:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::DYLD_ENVIRONMENT>());
			case mach::cmd::MAIN:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::MAIN>());
			case mach::cmd::DATA_IN_CODE:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::DATA_IN_CODE>());
			case mach::cmd::SOURCE_VERSION:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::SOURCE_VERSION>());
			case mach::cmd::DYLIB_CODE_SIGN_DRS:	return ISO::MakeBrowser(*cmd->template as<mach::cmd::DYLIB_CODE_SIGN_DRS>());
			case mach::cmd::ENCRYPTION_INFO_64:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::ENCRYPTION_INFO_64>());
			case mach::cmd::LINKER_OPTION:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::LINKER_OPTION>());
			case mach::cmd::LINKER_OPTIMIZATION_HINT:return ISO::MakeBrowser(*cmd->template as<mach::cmd::LINKER_OPTIMIZATION_HINT>());
			case mach::cmd::VERSION_MIN_TVOS:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::VERSION_MIN_TVOS>());
			case mach::cmd::VERSION_MIN_WATCHOS:	return ISO::MakeBrowser(*cmd->template as<mach::cmd::VERSION_MIN_WATCHOS>());
			case mach::cmd::NOTE:					return ISO::MakeBrowser(*cmd->template as<mach::cmd::NOTE>());
			case mach::cmd::BUILD_VERSION:			return ISO::MakeBrowser(*cmd->template as<mach::cmd::BUILD_VERSION>());
			case mach::cmd::DYLD_EXPORTS_TRIE:		return ISO::MakeBrowser(*cmd->template as<mach::cmd::DYLD_EXPORTS_TRIE>());
			case mach::cmd::DYLD_CHAINED_FIXUPS:	return ISO::MakeBrowser(*cmd->template as<mach::cmd::DYLD_CHAINED_FIXUPS>());

			default:								return ISO::MakeBrowser(*cmd);
		}
	}
};

template<bool be>	ISO_DEFVIRTT(MACH_commands, be);

template<bool be, int bits> struct MACH {
	typedef mach::header<be, bits>	header;
	typedef mach::nlist<be,bits>	nlist;

	header	h;
	uint32	version;

	struct Symbols : dynamic_array<nlist> {
		STRINGTABLE	strings;
		void Add(const char *name, int sect, uint64 val, typename nlist::FLAGS flags, typename nlist::DESC desc) {
			auto	&s = this->push_back();
			s.strx = strings.add(name);
			s.flags = flags;
			s.sect = sect;
			s.desc = desc;
			s.value = val;
		}
	};

	MACH() { clear(h); version = 0; }
	MACH(mach::CPU_TYPE::T cputype, mach::CPU_SUBTYPE::T cpusubtype, uint32 _version) {
		clear(h);
		h.cputype		= cputype;
		h.cpusubtype	= cpusubtype;
		version			= _version;
	}
	anything	Read(const_memory_block p);
	bool		Write(ISO::Browser b, ostream_ref file, FileHandler *fh, const char *fn);
};


namespace dwarf {
ISO_ptr<void> ReadDwarf(Sections &_sections, bool be);
}

static const char *dwarf_sects[] = {
	"__debug_abbrev",	"__debug_aranges",	"__debug_frame",	"__debug_info",
	"__debug_line",		"__debug_loc",		"__debug_macinfo",	"__debug_pubnames",
	"__debug_pubtypes",	"__debug_ranges",	"__debug_str",		"__debug_types",
};

template<bool be, int bits> anything MACH<be, bits>::Read(const_memory_block m) {
	anything	a;

	h			= *m;
	a.Append(ISO::MakePtr("header", h));

	auto	cp	= ISO_ptr<MACH_commands<be> >("commands", m.slice(sizeof(h), h.sizeofcmds));
	cp->Init(h.ncmds);

	a.Append(cp);

	const mach::command<be>	*cmd = cp->cmds;
	for (int i = 0, n = h.ncmds; i < n; i++) {
		switch (cmd->cmd) {
			case mach::cmd::SEGMENT: {
				mach::segment_command<be,32>	*c	= (mach::segment_command<be,32>*)cmd;
				if (int n = c->nsects) {
					if (str(c->segname) == "__DWARF") {
					} else {
						ISO_ptr<anything>	p2(str(c->segname));
						for (mach::section<be,32> *s = (mach::section<be,32>*)(c + 1); n--; s++) {
							if (!(s->flags & s->ZEROFILL) && s->size)
								p2->Append(ISO_ptr<StartBinBlock>(str(s->sectname), xint64(s->addr), m.slice(s->offset ? iso::uint32(s->offset) : s->addr - c->vmaddr, s->size)));
						}
						a.Append(p2);
					}
				} else {
					ISO_ptr<StartBinBlock>	p3(str(c->segname), xint64(c->vmaddr), m.slice(c->fileoff,  c->filesize));
					a.Append(p3);
				}
				break;
			}
			case mach::cmd::SYMTAB: {
				auto	symtab = cmd->template as<mach::cmd::SYMTAB>();
				typedef	mach::nlist<be, bits>		symtype;

				const_memory_block	sym = m.slice(symtab->sym.offset, symtab->sym.size * sizeof(symtype));
				const_memory_block	str = m.slice(symtab->str.offset, symtab->str.size);

				bool		nodemangle	= !!ISO::root("variables")["nodemangle"].GetInt();

				ISO_ptr<anything>	p2("symtab");
				for (const symtype *i = sym, *e = i + symtab->sym.size; i != e; ++i) {
					tag name = (const char*)str + i->strx;
					if (!nodemangle)
						name = demangle(name);
					p2->Append(MakePtr(name, *i));
				}

				a.Append(p2);
				break;
			}

			case mach::cmd::SEGMENT_64: {
				mach::segment_command<be,64>	*c	= (mach::segment_command<be,64>*)cmd;
				if (int n = c->nsects) {
					if (str(c->segname) == "__DWARF") {
						dwarf::Sections	sects;
						for (mach::section<be,64> *s	= (mach::section<be,64>*)(c + 1); n--; s++) {
							auto i = find(dwarf_sects, str(s->sectname));
							if (i < end(dwarf_sects))
								sects[i - dwarf_sects] = m.slice(s->offset ? iso::uint32(s->offset) : s->addr - c->vmaddr, s->size);
						}
						a.Append(dwarf::ReadDwarf(sects, be));

					} else {
						ISO_ptr<anything>	p2(str(c->segname));
						for (mach::section<be,64> *s	= (mach::section<be,64>*)(c + 1); n--; s++) {
							if (!(s->flags & s->ZEROFILL) && s->size)
								p2->Append(ISO_ptr<StartBinBlock>(str(s->sectname), xint64(s->addr), m.slice(s->offset ? iso::uint32(s->offset) : s->addr - c->vmaddr, s->size)));
						}
						a.Append(p2);
					}
				} else {
					a.Append(ISO_ptr<StartBinBlock>(str(c->segname), xint64(c->vmaddr), m.slice(c->fileoff,  c->filesize)));
				}
				break;
			}
		}
		cmd	= (mach::command<be>*)((uint8*)cmd + cmd->cmdsize);
	}
	return a;
}

template<bool be, int bits> bool MACH<be, bits>::Write(ISO::Browser b, ostream_ref file, FileHandler *fh, const char *fn) {
	bool		putsize		= false;//!!ISO::root("variables")["data_size"].GetInt(1);
	bool		putend		= true;//!!ISO::root("variables")["data_end"].GetInt(int(!putsize));
	bool		has_bin		= fn && ISO::binary_data.Size() != 0 && str(fh->GetExt()) != "ib" && str(fh->GetExt()) != "ibz";

	dynamic_array<mach::section<be, bits> >	sections;

	mach::segment_command<be,bits>	seg(has_bin ? 2 : 1);
	mach::version_min_command<be>	ver(version & 0x80000000 ? mach::cmd::VERSION_MIN_IPHONEOS : mach::cmd::VERSION_MIN_MACOSX, version & 0x7fffffff);
	mach::commandT<mach::cmd::SYMTAB, be>		sym;

	h.magic			= header::MAGIC;
	h.filetype		= header::OBJECT;
	h.ncmds			= 3;
	h.sizeofcmds	= seg.cmdsize + ver.cmdsize + sym.cmdsize;
	h.flags			= header::SUBSECTIONS_VIA_SYMBOLS;

	file.write(h);
	file.seek_cur(h.sizeofcmds);

	streamptr	offset = file.tell();

	auto	&sect = sections.push_back();
	clear(sect);
	strcpy(sect.sectname, ISO::root("variables")["section"].GetString("__data"));
	strcpy(sect.segname, "__DATA");
	sect.offset		= uint32(offset);
	sect.addr		= 0;
	sect.align		= 4;

	clear(seg.segname);
	seg.vmaddr		= 0;
	seg.fileoff 	= uint32(offset);
	seg.maxprot 	= 7;
	seg.initprot 	= 7;
	seg.flags		= 0;

	size_t		total		= 0;
	Symbols		syms;

//	syms.Add("ltmp0", 1, 0, nlist::SECT, nlist::REF_UNDEFINED_NON_LAZY);

	for (int i = 0, count = b.Count(); i < count; i++) {
		ISO_ptr<void>	p		= *b[i];
		const char		*label	= p.ID().get_tag();
		if (!label)
			label = "";

		size_t	start	= total;
		syms.Add(format_string("_%s_bin", label), 1, start, nlist::SECT | nlist::EXT, nlist::REF_UNDEFINED_NON_LAZY);

		size_t	offset	= file.tell();
		fh->Write(p, ostream_offset(file).me());
		file.seek_end(0);
		total		= file.tell() - offset;

		if (putend)
			syms.Add(format_string("_%s_end", label), 1, total, nlist::SECT | nlist::EXT, nlist::REF_UNDEFINED_NON_LAZY);
		if (putsize)
			syms.Add(format_string("_%s_size", label), 0, total - start, nlist::ABS | nlist::EXT, nlist::REF_UNDEFINED_NON_LAZY);
	}

	sect.size		= uint32(total);

	if (has_bin) {
		streamptr	offset = file.tell();
		auto	&sect = sections.push_back();
		clear(sect);
		strcpy(sect.sectname, "__bin");
		strcpy(sect.segname, "__DATA");
		sect.align		= 4;
		sect.addr		= total;
		sect.offset		= offset;

		filename	name = filename(fn).name();
		syms.Add(format_string("_%s_binary_bin", (const char*)name), 2, total, nlist::SECT | nlist::EXT, nlist::REF_UNDEFINED_NON_LAZY);
		ISO::binary_data.Write(file);

		size_t	size	= file.tell() - offset;
		sect.size		= size;

		total += size;

		if (putend)
			syms.Add(format_string("_%s_binary_end", (const char*)name), 2, total, nlist::SECT | nlist::EXT, nlist::REF_UNDEFINED_NON_LAZY);
		if (putsize)
			syms.Add(format_string("_%s_binary_size", (const char*)name), 0, size, nlist::ABS | nlist::EXT, nlist::REF_UNDEFINED_NON_LAZY);

	}

#if 0
	{
		auto	&sect = sections.push_back();
		clear(sect);
		strcpy(sect.sectname, "__bitcode");
		strcpy(sect.segname, "__LLVM");
		sect.addr		= total++;
		sect.offset		= file.tell();
		sect.size		= 1;

		file.putc(0);
	}
	{
		auto	&sect = sections.push_back();
		clear(sect);
		strcpy(sect.sectname, "__cmdline");
		strcpy(sect.segname, "__LLVM");
		sect.addr		= total++;
		sect.offset		= file.tell();
		sect.size		= 1;

		file.putc(0);
	}
#endif

	sym.sym.offset 	= file.tell32();
	sym.sym.size 	= syms.size32();
	for (auto &i : syms)
		file.write(i);

	nlist	blank;
	clear(blank);
	file.write(blank);

	sym.str.offset 	= file.tell32();
	sym.str.size 	= syms.strings.size32();
	seg.vmsize 		= seg.filesize 	= file.tell() - offset;
	file.write(syms.strings);

	file.seek(sizeof(header));
	write(file, seg, sections, ver, sym);
	return true;
}

struct ISO_MACH : mapped_anything {
	ISO_MACH(const filename &fn)	: mapped_anything(fn) {}
	ISO_MACH(const istream_ref file)	: mapped_anything(file) {}
};
ISO_DEFUSERX(ISO_MACH, mapped_anything, "MACH");

class MACHFileHandler : public FileHandler {
	bool					IsMagic(uint32 i)	{
		return i == mach::header<false,32>::MAGIC || i ==  mach::header<true,32>::MAGIC || i == mach::header<false,64>::MAGIC || i ==  mach::header<true,64>::MAGIC;
	}
	const char*		GetDescription() override {
		return "MACH file";
	}
	int				Check(istream_ref file) override {
		file.seek(0);
		uint32 i = file.get<uint32le>();
		if (IsMagic(i))
			return CHECK_PROBABLE;

		if (i == FAT_CIGAM)
			return CHECK_PROBABLE;

		char	fn[64], *p = fn;
		file.seek(0);
		file.readbuff(fn, sizeof(fn));
		while (p < fn + 64 && *p)
			p++;
		if (p[-2] == '.' && p[-1] == 'o' && p[1] == 0 && p[2] == 0 && p[3] == 0) {
			int	*p2 = (int*)(intptr_t(p + 4) & ~3);
			if (*p2 == 0)
				++p2;
			if (*p2 == 0)
				++p2;
			if (IsMagic(*p2))
				return CHECK_PROBABLE;
		}
		return CHECK_DEFINITE_NO;
	}

	static int RightPlatform() {
		const char *exportfor = ISO::root("variables")["exportfor"].GetString();
		return str(exportfor) == "mac" ? 1
			: str(exportfor) == "ios" 	? (ISO::root("variables")["targetbits"].GetInt() == 64 ? 3 : 2)
			: 0;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file, const char *fn, FileHandler *fh, int plat) {
		p	= ISO_conversion::convert(p, 0, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
		ISO::Browser2	b	= GetItems(p);
		switch (plat) {
			case 1: return MACH<false,64>(mach::CPU_TYPE::X86_64, 	mach::CPU_SUBTYPE::I386_ALL,0x000A0700).Write(b, file, fh, fn);
			case 2: return MACH<false,32>(mach::CPU_TYPE::ARM, 		mach::CPU_SUBTYPE::ARM_V7,	0x800A0000).Write(b, file, fh, fn);
			case 3: return MACH<false,64>(mach::CPU_TYPE::ARM_64, 	mach::CPU_SUBTYPE::ARM_ALL, 0x800b0000).Write(b, file, fh, fn);
			default: return false;
		}
	}
	

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<ISO_MACH>	p(id, fn);
		switch (*(uint32le*)p->m) {
			case mach::header<false, 32>::MAGIC:	p->a = MACH<false, 32>().Read(p->m); return p;
			case mach::header<true, 32>::MAGIC:		p->a = MACH<true,  32>().Read(p->m); return p;
			case mach::header<false, 64>::MAGIC:	p->a = MACH<false, 64>().Read(p->m); return p;
			case mach::header<true, 64>::MAGIC:		p->a = MACH<true,  64>().Read(p->m); return p;
			#if 0
			case FAT_CIGAM: {
				uint32			n		= ((uint32be*)p->m)[1];
				mach::fat_arch*	archs	= p->m + 8;
				ISO_ptr<anything>	p(id);
				for (int i = 0; i < n; i++) {
					file.seek(archs[i].offset);
					p->Append(Read(0, istream_offset(file, archs[i].size).me()));
				}
				return p;
			}
			default: {
				char	*fn = p->m, *p = fn;
				while (p < fn + 64 && *p)
					p++;
				if (p[-2] == '.' && p[-1] == 'o' && p[1] == 0 && p[2] == 0 && p[3] == 0) {
					int	*p2 = (int*)(int(p + 4) & ~3);
					if (*p2 == 0)
						++p2;
					if (*p2 == 0)
						++p2;
					return Read(str(fn), istream_offset(file).me());
				}
			}
			#endif
		}
		return ISO_NULL;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		switch (file.get<uint32le>()) {
			case mach::header<false, 32>::MAGIC:	{ file.seek(0); ISO_ptr<ISO_MACH> p(id, file); p->a = MACH<false, 32>().Read(p->m); return p; }
			case mach::header<true, 32>::MAGIC:		{ file.seek(0); ISO_ptr<ISO_MACH> p(id, file); p->a = MACH<true,  32>().Read(p->m); return p; }
			case mach::header<false, 64>::MAGIC:	{ file.seek(0); ISO_ptr<ISO_MACH> p(id, file); p->a = MACH<false, 64>().Read(p->m); return p; }
			case mach::header<true, 64>::MAGIC:		{ file.seek(0); ISO_ptr<ISO_MACH> p(id, file); p->a = MACH<true,  64>().Read(p->m); return p; }

			case FAT_CIGAM: {
				uint32		n			= file.get<uint32be>();
				mach::fat_arch*	archs	= alloc_auto(mach::fat_arch, n);
				file.readbuff(archs, sizeof(mach::fat_arch) * n);
				ISO_ptr<anything>	p(id);
				for (int i = 0; i < n; i++) {
					file.seek(archs[i].offset);
					p->Append(Read(0, istream_offset(file, archs[i].size)));
				}
				return p;
			}

			default: {
				char	fn[64], *p = fn;
				file.seek(0);
				file.readbuff(fn, sizeof(fn));
				while (p < fn + 64 && *p)
					p++;
				if (p[-2] == '.' && p[-1] == 'o' && p[1] == 0 && p[2] == 0 && p[3] == 0) {
					int	*p2 = (int*)(intptr_t(p + 4) & ~3);
					if (*p2 == 0)
						++p2;
					if (*p2 == 0)
						++p2;
					file.seek((char*)p2 - fn);
					return Read(str(fn), istream_offset(file));
				}
				return ISO_NULL;
			}
		}
	}

	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		if (int plat = RightPlatform()) {
			filename::ext_t		ext = filename(fn.name()).ext();
			FileHandler			*fh	= ext.blank() ? NULL : FileHandler::Get(ext);
			if (!fh)
				fh = FileHandler::Get("bin");

			p	= ISO_conversion::convert(p, 0, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
			return Write(p, FileOutput(fn).me(), fn, fh, plat);
		}
		return false;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (int plat = RightPlatform())
			return Write(p, file, 0, FileHandler::Get("bin"), plat);
		return false;
	}
} machhandler;

class MACHOBJFileHandler : MACHFileHandler {
	const char*		GetExt() override { return "o"; }
	const char*		GetDescription() override { return "MACH object file"; }
	int				Check(istream_ref file) override { return CHECK_NO_OPINION; }
} machobj;

class DYLIBFileHandler : MACHFileHandler {
	const char*		GetExt() override { return "dylib"; }
	const char*		GetDescription() override { return "MACH Dynamic Library";}
	int				Check(istream_ref file) override { return CHECK_NO_OPINION; }
} dylib;

class FATArchHandler : FileHandler {
	const char*		GetExt() override { return "a"; }
	const char*		GetDescription() override { return "Universal archive"; }
	int				Check(istream_ref file) override { file.seek(0); return file.get<uint32le>() == FAT_CIGAM ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		if (file.get<uint32le>() != FAT_CIGAM)
			return ISO_NULL;

		FileHandler	*fh		= Get("a");
		if (fh == this)
			fh = GetNext(fh);

		uint32			n		= file.get<uint32be>();
		mach::fat_arch*	archs	= alloc_auto(mach::fat_arch, n);
		file.readbuff(archs, sizeof(mach::fat_arch) * n);
		ISO_ptr<anything>	p(id);
		for (int i = 0; i < n; i++) {
			file.seek(archs[i].offset);
			if (ISO_ptr<void> a = fh->Read(0, istream_offset(file, archs[i].size))) {
				p->Append(a);
			} else {
				file.seek(archs[i].offset);
				ISO_ptr<ISO_openarray<uint8> > p2(0);
				file.readbuff(p2->Create(archs[i].size), archs[i].size);
				p->Append(p2);
			}
		}
		return p;
	}
} fatarch;

class MACHSymDefFileHandler : public FileHandler {
	struct header {
		char	magic[20];
		uint32	length;
		bool	valid() {
			static const char id[20] = "__.SYMDEF";
			return memcmp(id, magic, 20) == 0;
		}
	};
	struct record {
		uint32	offset;
		uint32	value;
	};
	int				Check(istream_ref file) override {
		header	h;
		file.seek(0);
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		header	h;
		if (!file.read(h) || !h.valid())
			return ISO_NULL;

		malloc_block	records(file, h.length);
		uint32			stringlen = file.get();
		malloc_block	strings(file, stringlen);

		ISO_ptr<anything>	p(id);
		for (record *r = records; r < records.end(); ++r) {
			p->Append(ISO_ptr<xint32>((char*)strings + r->offset, r->value));
		}

		return p;
	}
} mach_symdef;

