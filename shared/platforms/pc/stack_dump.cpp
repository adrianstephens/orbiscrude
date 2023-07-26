#include "stack_dump.h"
#include "directory.h"
#include "vm.h"
#include <psapi.h>

#ifdef USE_DBGHELP
#pragma comment(lib, "dbghelp")
#else
#include "filetypes/code/coff.h"
#include "filetypes/code/ar.h"
#include "comms/http.h"
#endif

using namespace iso;

//-----------------------------------------------------------------------------
//	CallStackDumper
//-----------------------------------------------------------------------------

#ifdef _M_X64
#define ADDRESSFORMAT	"0x%I64x"
#else
#define ADDRESSFORMAT	"0x%.8X"
#endif

string iso::BuildSymbolSearchPath(const char *exec_dir) {
	char	temp[MAX_PATH] = {0};
	string	path;
	path << exec_dir << ";.\\";

//	if (GetWindowsDirectoryA(temp, MAX_PATH))
//		path << ";" << temp;
//
//	if (GetSystemDirectoryA(temp, MAX_PATH))
//		path << ";" << temp;

	if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", temp, sizeof(temp)))
		path << ";" << temp;

	if (GetEnvironmentVariableA("_NT_ALT_SYMBOL_PATH", temp, sizeof(temp)))
		path << ";" << temp;

#if 0
	win::RegKey	key(HKEY_CURRENT_USER, "Software\\Microsoft\\VisualStudio\\11.0\\Debugger");
	if (key) {
		if (string dir = key.value("SymbolCacheDir").get_text())
			path << ";srv*" << dir << "\\MicrosoftPublicSymbols;srv*" << dir;
	}
#endif
	return replace(path, "\"", "");
}

string_accum &CallStackFrame::Dump(string_accum &a) const {
#if 0
	if (line)
		a << file.name_ext() << "(" << line << "): ";
#endif
	if (module_fn)
		a << filename(module_fn).name_ext() << '!';

	if (!symbol.blank()) {
		a << symbol;
		if (symbol_displacement)
			a.format(" + 0x%X bytes", symbol_displacement);
	} else {
		a << "0x" << hex(uintptr_t(pc));
	}

	return a;
}

#ifdef USE_DBGHELP
template<int N> struct SymbolBuffer : SYMBOL_INFO {
	char	buffer[N];
	SymbolBuffer() { SizeOfStruct = sizeof(SYMBOL_INFO); MaxNameLen = N; }
};

struct SourceInfo : IMAGEHLP_LINE64 {
	SourceInfo() { SizeOfStruct = sizeof(*this); }
};

CallStackFrame::CallStackFrame(uint64 pc, HANDLE process, const char *module_fn) : pc(pc), module_fn(module_fn), line(0), symbol_displacement(0), line_displacement(0) {
	SymbolBuffer<256>	symbol_buffer;
	SourceInfo			source;
	if (SymFromAddr(process, uintptr_t(pc), &symbol_displacement, &symbol_buffer))
		symbol	= symbol_buffer.Name;

	if (SymGetLineFromAddr64(process, uintptr_t(pc), (DWORD*)&line_displacement, &source)) {
		line	= source.LineNumber;
		file	= source.FileName;
	}
}

CallStackDumper::CallStackDumper(HANDLE _process, uint32 options, const char *search_path) : modules(_process), process(_process) {
	if (!process)
		process = HANDLE(0x12345678);
	SymSetOptions(options);
	SymInitialize(process, search_path, TRUE);
}

CallStackDumper::CallStackDumper(uint32 options) : process(GetCurrentProcess()) {
	SymSetOptions(options);
	SymInitialize(process, 0, FALSE);
}

CallStackDumper::~CallStackDumper() {
	SymCleanup(process);
}

void CallStackDumper::AddModule(const char *fn, uint64 a, uint64 b) {
	modules.emplace_back({a, b}, fn);
	SymLoadModuleEx(process, 0, fn, 0, a, b - a, 0, 0);
}

range<virtual_iterator<const CallStackDumper::Module>> CallStackDumper::GetModules() const {
	return modules;
}

CallStackFrame CallStackDumper::GetFrame(uint64 addr) {
	return CallStackFrame(addr, process, GetModule(addr));
}

#else

//-----------------------------------------------------------------------------
//	ProcessSymbols::PDB2
//-----------------------------------------------------------------------------

ProcessSymbols::PDB2::PDB2(istream_ref file) {
	MSF::EC	ec;
	ref_ptr<MSF::reader>	msf = new MSF::reader(file, &ec);
	ISO_ASSERT(ec == MSF::EC_OK);

	PDBinfo	info;
	info.load(msf, snPDB);
	PDB::load(info, msf);

	for (auto &i : Symbols()) {
		if (i.rectyp == CV::S_PUB32)
			sorted_syms.push_back(i.as<CV::PUBSYM32>());
	}
	sort(sorted_syms, [](const CV::PUBSYM32 *a, const CV::PUBSYM32 *b) { return compare(a->addr, b->addr) < 0; });

	objs.resize(mods.size());

	for (int i = 0; i < mods.size(); i++) {
		auto	&mod	= mods[i];
		auto	&obj	= objs[i];

		obj.mod			= &mod;
		//obj.first_sc	= find(sec_contribs, mod.modi->sc);

		// change file offsets to point to global pdb names

		if (auto file_stream = CV::GetFirstSubStream<CV::DEBUG_S_FILECHKSMS>(mod.SubStreams())) {
			for (auto &substream : mod.SubStreams()) {
				switch (substream.type) {
					case CV::DEBUG_S_LINES: {
						auto	lines = substream.as<CV::DEBUG_S_LINES>();
						for (auto &i : lines->entries())
							unconst(i.offFile) = file_stream->at(i.offFile)->name;
						//obj.lines.push_back(lines);
						break;
					}

					case CV::DEBUG_S_INLINEELINES:
						for (auto &i : substream.as<CV::DEBUG_S_INLINEELINES>()->entries())
							unconst(i.fileId) = file_stream->at(i.fileId)->name;
						break;
				}
			}
		}
	}
}

void ProcessSymbols::PDB2::LoadObjectFile(ObjectInfo &obj) {
	filename	fn = obj.mod->ObjFile();
	obj.obj	 = mapped_file(fn);

	if (fn.ext() == ".lib") {
		memory_reader	mi(obj.obj);
		ar_parser::entry	e;
		for (ar_parser a(mi); a.next(e);) {
			if (e.name == obj.mod->Module()) {
				obj.obj = mapped_file(fn, mi.tell(), e.size);
				break;
			}
		}
	}
	
	coff::RAW_COFF	raw(obj.obj);
	for (auto &sect : raw.Sections()) {
		if (str(sect.Name) == cstr(".debug$T")) {
			obj.debugT = raw.GetMem(sect);
			if (obj.debugT && *obj.debugT == 4u) {
				for (auto &i : make_next_range<CV::TYPTYPE>(obj.debugT + 4)) {
					switch (i.leaf) {
						case CV::LF_TYPESERVER2: {
							auto	ts		= i.as<CV::TypeServer2>();
							auto	entry	= type_servers[ts->name];
							if (!entry.exists()) {
								MSF::EC		error;
								PDBinfo		info;
								ref_ptr<MSF::reader>	msf = new MSF::reader(FileInput(ts->name), &error);
								ISO_VERIFY(!error && info.load(msf, snPDB));
								entry = new PDB_types;
								entry->load(info, msf);
							}
							obj.types = entry;
						}
					}
				}
			}
		}
	}
}

const ProcessSymbols::Line *ProcessSymbols::PDB2::LineFromOffset(uint32 addr, const char **fn, uint32 *length, bool &nostep) const {
	auto	seg_addr	= ToSegmented(addr);
	auto	sc			= first_not(sec_contribs, [seg_addr](const SC &sc) { return sc.compare(seg_addr) > 0; });
	auto	&obj		= objs[sc->imod];
	
	*fn		= 0;
	nostep	= false;

	const Lines	*lines = 0;
	for (auto &sub : obj.SubStreams()) {
		if (sub.type == CV::DEBUG_S_LINES) {
			auto	lines0 = sub.as<CV::DEBUG_S_LINES>();
			if (compare(lines0->addr, sc->addr()) == 0) {
				lines = lines0;
				break;
			}
		}
	}
	
	if (!lines)
		return 0;

	const Line	*last	= 0, *pline = 0;
	uint32		file	= 0;
	uint32		off		= seg_addr.off - lines->addr.off;
	
	for (auto &i : lines->entries()) {
		pline = first_not(i.lines(), [off](const Lines::line &e) { return e.offset <= off; });

		if (pline == i.lines().begin())
			break;

		last	= pline - 1;
		file	= i.offFile;

		if (pline != i.lines().end())
			break;

		pline	= last;
	}
	*length	= (pline->offset <= off ? lines->cb : pline->offset) - off;
	*fn		= FileName(file);

	if (last->linenumStart == Lines::NEVER_STEPINTO || last->linenumStart == Lines::ALWAYS_STEPINTO) {
		nostep = last->linenumStart == Lines::NEVER_STEPINTO;
		++last;
	}
	return last;
}

uint32 ProcessSymbols::PDB2::SourceToOffset(const char *src_fn, uint32 &line, uint32 &best_line) {
	uint32	best_addr	= 0;

	if (NI ni = filename_id(src_fn)) {
		for (auto &obj : objs) {
			for (auto block : transformc(filter(obj.SubStreams(), [](const CV::SubSection &sub) { return sub.type == CV::DEBUG_S_LINES; }), [](const CV::SubSection &sub) { return sub.as<CV::DEBUG_S_LINES>(); })) {
				for (auto &entry : block->entries()) {
					if (entry.offFile == ni) {
						for (auto &l : entry.lines()) {
							if (between(l.linenumStart, line, best_line - 1)) {
								best_line = l.linenumStart;
								best_addr = l.offset + block->addr.off + Section(block->addr.seg).VirtualAddress;
								if (best_line == line)
									return best_addr;
							}
						}
					}
				}
			}
		}
	}

	return best_addr;
}

ProcessSymbols::ObjStreams ProcessSymbols::PDB2::ObjectInfo::SubStreams2() const {
	if (obj) {
		coff::RAW_COFF	raw(obj);
		for (auto &sect : raw.Sections()) {
			if (str(sect.Name) == ".debug$S") {
				auto	mem = raw.GetMem(sect);
				if (*mem == 4u)
					return make_next_range<const CV::SubSection>(mem + 4);
			}
		}
	}
	return none;
}
ProcessSymbols::ObjSymbols ProcessSymbols::PDB2::GetSymbols(const SC2 &sc, PDB_types *&types) {
	auto	&obj	= objs[sc.imod];

	if (sc.isect_coff) {
		if (!obj.obj)
			LoadObjectFile(obj);

		types = obj.types;
	
		coff::RAW_COFF	raw(obj.obj);
		auto	sect = raw.Section(sc.isect_coff);
		if (str(sect.Name) == ".debug$S")
			return CV::GetFirstSubStream<CV::DEBUG_S_SYMBOLS>(make_next_range<const CV::SubSection>(raw.GetMem(sect) + 4))->entries();
	}

	types = this;
	return obj.mod->Symbols();
}

ProcessSymbols::ObjSymbols ProcessSymbols::PDB2::GetProcSymbols(CV::segmented32 seg_addr, uint32 &offset, PDB_types *&types) {
	auto	sc		= GetSC(seg_addr);
	auto	syms	= GetSymbols(*sc, types);
	offset			= sc->isect_coff ? seg_addr.off - sc->off : seg_addr.off;

	for (auto si = syms.begin(), se = syms.end(); si != se;) {
		switch (si->rectyp) {
			case CV::S_GPROC32: case CV::S_GPROC32_ID: case CV::S_LPROC32: case CV::S_LPROC32_ID: {
				auto	proc = si->as<CV::PROCSYM32>();
				if (proc->encompasses(offset))
					return proc->children(syms);

				if (!proc->pEnd)
					return none;

				si = proc->skip_children(syms.begin());
				break;
			}
			default:
				++si;
				break;
		}

	}
	return none;
}

//-----------------------------------------------------------------------------
//	ProcessSymbols::ModuleInfo
//-----------------------------------------------------------------------------

void ProcessSymbols::ModuleInfo::SetFunctionData(const const_memory_block &data) {
	const RUNTIME_FUNCTION	*a = data, *b = a + data.length() / sizeof(*a);
	while (a < b && a->BeginAddress == 0)
		++a;
	while (b > a && b[-1].BeginAddress == 0)
		--b;
	function_data = malloc_block(data.slice(a, b));
}

void ProcessSymbols::ModuleInfo::SetDebug(const const_memory_block &data, const get_memory_t &mem) {
	fn_pdb = fn;
	for (auto &d : Module::DebugDir(data)) {
		switch (d.Type) {
			case IMAGE_DEBUG_TYPE_CODEVIEW: {
				malloc_block	data2(d.SizeOfData);
				if (mem(d.PointerToRawData, data2, d.SizeOfData)) {
					const DEBUG_DIRECTORY_CODEVIEW	*cv = data2;
					fn_pdb	= cv->fn;
					if (!fn_pdb.dir())
						fn_pdb.add_dir(format_string("%08x%04x%04x%016I64x%x", cv->Guid.Data1, cv->Guid.Data2, cv->Guid.Data3, (uint64)*(uint64be*)cv->Guid.Data4, cv->Age)).add_dir(cv->fn);
				}
				break;
			}
			case IMAGE_DEBUG_TYPE_FPO:
				function_data.resize(d.SizeOfData);
				mem(d.PointerToRawData, function_data, d.SizeOfData);
				break;
		}
	}
}

//-----------------------------------------------------------------------------
//	ProcessSymbols
//-----------------------------------------------------------------------------

ProcessSymbols::ModuleInfo *ProcessSymbols::AddModule(const char *fn, uint64 base, uint64 end, bool mapped, const get_memory_t &mem) {
	auto	&mod	= modules.emplace({base, end}, fn);
	mod.base		= base;

	malloc_block	data(0x1000);
	if (mem(base, data, 0x1000)) {
		ModuleBase	m(data);
		if (auto p = m.SectionByName(".pdata")) {
			malloc_block	pdata(p->SizeOfRawData);
			if (mem(base + m.FixRVA(p->VirtualAddress, p->PointerToRawData, mapped), pdata, p->SizeOfRawData))
				mod.SetFunctionData(pdata);
		}

		auto			debug = m.DataDir(IMAGE_DIRECTORY_ENTRY_DEBUG);
		malloc_block	ddata(debug.Size);
		if (mem(base + m.FixRVA(debug.VirtualAddress, mapped), ddata, debug.Size))
			mod.SetDebug(ddata, [&mem, m, base, mapped](uint64 addr, void *buffer, size_t size) {
				if (auto *sect = m.SectionByRaw(addr))
					return mem(base + m.FixRVA(sect->VirtualAddress, sect->PointerToRawData, mapped) + (addr - sect->PointerToRawData), buffer, size);
				return false;
			});
	}

	return &mod;
}

ProcessSymbols::ModuleInfo* ProcessSymbols::AddModule(HANDLE file, uint64 base) {
	mapped_file	mf(file);
	RawModule	raw(mf);
	uint64		end		= raw.calc_end();

	char	fn[512];
	GetMappedFileName(GetCurrentProcess(), mf, fn, sizeof(fn));

	auto	&mod	= modules.emplace(interval<uint64>(base, base + end), filename::cleaned(fn));
	mod.base		= base;

	if (auto p = raw.SectionByName(".pdata"))
		mod.SetFunctionData(raw.GetMem(*p));
	
	mod.SetDebug(raw.GetMem(raw.DataDir(IMAGE_DIRECTORY_ENTRY_DEBUG)), [raw](uint64 addr, void *buffer, size_t size) {
		if (auto src = raw.DataByRaw(addr)) {
			memcpy(buffer, src, size);
			return true;
		}
		return false;
	});

	return &mod;
}

ProcessSymbols::PDB2 *ProcessSymbols::GetPdb(ModuleInfo *mod) const {
	if (!mod)
		return nullptr;

	if (!mod->pdb && !mod->cantload) {
		if (!is_file(mod->fn_pdb)) {
			bool	got = false;
			for (auto path : parts<';'>(search_path)) {
				if (path.begins("srv*")) {
					dynamic_array<string>	server = parts<'*'>(path);
					filename	fn_dest = filename(server[1]).add_dir(mod->fn_pdb);
					if (exists(fn_dest)) {
						mod->fn_pdb	= fn_dest;
						got		= true;
						break;
					}
					create_dir(fn_dest.dir());

					socket_init();

					for (int i = 2; i < server.size(); i++) {
						HTTP	http("orbiscrude", server[i]);
						auto	in	= http.Get(filename(http.path).add_dir(mod->fn_pdb).convert_to_fwdslash());
						if (in.exists()) {
							FileOutput	out(fn_dest);
							if (out.exists()) {
								stream_copy<1024>(out, in);
								mod->fn_pdb	= fn_dest;
								got		= true;
							}
							break;
						}
					}

				} else {
					for (auto d = directory_recurse(path, mod->fn_pdb); d; ++d) {
						mod->fn_pdb	= d;
						got		= true;
						break;
					}
					if (!got) {
						filename	pattern(mod->fn_pdb.name_ext());
						for (auto d = directory_recurse(path, pattern); d; ++d) {
							if (is_file((filename)d)) {
								mod->fn_pdb	= d;
								got		= true;
								break;
							}
						}
					}
				}
				if (got)
					break;
			}
			if (!got) {
				mod->cantload = true;
				return nullptr;
			}
		}
	
		ISO_OUTPUTF("Loading symbols ") << mod->fn_pdb << '\n';
		mod->pdb = new ProcessSymbols::PDB2(FileInput(mod->fn_pdb));
	}
	return mod->pdb;
}

void *ProcessSymbols::GetFunctionTableEntry(uint64 addr) {
	if (auto mod = GetModule(addr))
		return mod->GetFunctionTableEntry(uint32(addr - mod->base));
	return 0;
}

const CV::PUBSYM32*	ProcessSymbols::SymFromAddr(uint64 addr, uint64 *disp, uint32 *line, const char **src_fn, const char **mod_fn) {
	*disp	= 0;
	*line	= 0;
	*src_fn	= 0;
	*mod_fn	= 0;

	if (auto mod = GetModule(addr)) {
		*mod_fn	= mod->fn;
		if (auto pdb = GetPdb(mod)) {
			uint32	length;
			bool	nostep;
			if (auto *p = pdb->LineFromOffset(uint32(addr - mod->base), src_fn, &length, nostep))
				*line = p->linenumStart;

			CV::segmented32	seg_addr	= pdb->ToSegmented(addr - mod->base);
			if (auto sym = pdb->SymFromOffset(seg_addr)) {
				*disp	= seg_addr.off - sym->addr.off;
				return sym;
			}
		}
	}
	return nullptr;
}

const CV::PUBSYM32 *ProcessSymbols::SymFromName(const char *name, ModuleInfo *&mod) {
	for (auto &i : modules) {
		if (auto pdb = GetPdb(&i)) {
			if (auto sym = pdb->LookupSym(count_string(name))) {
				ISO_ASSERT(sym->rectyp == CV::S_PUB32);
				mod = &i;
				return sym->as<CV::PUBSYM32>();
			}
		}
	}
	return nullptr;
}

const ProcessSymbols::Line *ProcessSymbols::LineFromAddr(uint64 addr, const char **fn, uint32 *length, bool &nostep) {
	if (auto mod = GetModule(addr)) {
		if (auto *pdb = GetPdb(mod))
			return pdb->LineFromOffset(uint32(addr - mod->base), fn, length, nostep);
	}
	return 0;
}

const CV::PROCSYM32 *ProcessSymbols::ProcFromAddr(uint64 addr, uint32 &offset, PDB_types *&types) {
	if (auto mod = GetModule(addr)) {
		if (auto *pdb = GetPdb(mod)) {
			auto	seg		= pdb->ToSegmented(addr - mod->base);
			auto	sc		= pdb->GetSC(seg);
			auto	syms	= pdb->GetSymbols(*sc, types);
			offset			= sc->isect_coff ? seg.off - sc->off : seg.off;

			for (auto si = syms.begin(), se = syms.end(); si != se;) {
				switch (si->rectyp) {
					case CV::S_GPROC32: case CV::S_GPROC32_ID: case CV::S_LPROC32: case CV::S_LPROC32_ID: {
						auto	proc = si->as<CV::PROCSYM32>();
						if (proc->encompasses(offset))
							return proc;

						if (!proc->pEnd)
							return 0;

						si = proc->skip_children(syms.begin());
						break;
					}
					default:
						++si;
						break;
				}

			}

		}
	}
	return nullptr;
}

uint64 ProcessSymbols::SourceToAddr(const char *src_fn, uint32 &line) {
	uint64	best_addr	= 0;
	uint32	best_line	= line + 16;

	for (auto &mod : with_iterator(modules)) {
		if (PDB2 *pdb = mod->pdb) {
			best_addr = mod->base + pdb->SourceToOffset(src_fn, line, best_line);
			if (best_line == line)
				return best_addr;
		}
	}
	line = best_line;
	return best_addr;
}

void ProcessSymbols::SetOptions(uint32 _options, const char *_search_path)  {
	options		= _options;
	search_path	= _search_path;
}

//-----------------------------------------------------------------------------
//	CallStackDumper
//-----------------------------------------------------------------------------

CallStackFrame::CallStackFrame(uint64 pc, ProcessSymbols &syms) : pc(pc), line_displacement(0), symbol_displacement(0), line(0) {
	const char	*src_fn, *mod_fn;
	if (auto sym = syms.SymFromAddr(pc, &symbol_displacement, &line, &src_fn, &mod_fn)) {
		symbol	= demangle_vc(sym->name);
		file	= src_fn;
	}
	module_fn	= mod_fn;
}

CallStackDumper::CallStackDumper(HANDLE process, uint32 options, const char *search_path) {
	syms.SetOptions(options, search_path);
	if (process) {
		ModuleSnapshot	m(GetProcessId(process), TH32CS_SNAPMODULE);
		for (auto e : m.modules())
			AddModule(e.szExePath, (uint64)e.modBaseAddr, (uint64)e.modBaseAddr + e.modBaseSize);
	}
}

CallStackDumper::CallStackDumper(uint32 options) {
	syms.SetOptions(options, 0);
}

CallStackDumper::~CallStackDumper() {}

void CallStackDumper::AddModule(const char *fn, uint64 a, uint64 b) {
	if (mapped_file	mf = fn) {
		syms.AddModule(fn, a, b, false, [&mf, a](uint64 addr, void *buffer, size_t size) {
			memcpy(buffer, mf + (addr - a), size);
			return true;
		});
	}
}
void CallStackDumper::AddModule(HANDLE file, uint64 base) {
	syms.AddModule(file, base);
}

const char *CallStackDumper::GetModule(uint64 addr) {
	auto	mod = syms.GetModule(addr);
	return mod ? mod->fn : 0;
}

CallStackFrame CallStackDumper::GetFrame(uint64 addr) {
	return CallStackFrame(addr, syms);
}

range<virtual_iterator<const CallStackDumper::Module>> CallStackDumper::GetModules() const {
	typedef decltype(syms.modules.begin()) I;
	return transformc(with_iterator(syms.modules), [](const I &i) { return Module(i.key(), i->fn); });
}

#endif


void CallStackDumper::Dump(const uint64 *addr, uint32 num_frames) {
	buffer_accum<512>	ba;
	while (num_frames-- && *addr)
		ISO_OUTPUT((GetFrame(*addr++).Dump(ba.reset() << "    ") << '\n').term());
}


