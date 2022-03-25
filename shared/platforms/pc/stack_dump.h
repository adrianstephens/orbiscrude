#ifndef STACK_DUMP_H
#define STACK_DUMP_H

#include "hook_stack.h"
#include "base/tree.h"
#include "base/hash.h"
#include "vm.h"
#include "code/cvinfo.h"
#include <intrin.h>

#define STACK_COMPRESSION
//#define USE_DBGHELP

#ifndef USE_DBGHELP
#include "code/pdb.h"
#endif

namespace iso {

string BuildSymbolSearchPath(const char *exec_dir = get_exec_path().dir());

#ifndef USE_DBGHELP

string demangle_vc(const char* mangled, uint32 flags = 0x7f);

class ProcessSymbols {
	uint32		options;
	string		search_path;

public:
	typedef CV::SubSectionT<CV::DEBUG_S_LINES>			Lines;
	typedef CV::SubSectionT<CV::DEBUG_S_LINES>::line	Line;
	typedef	range<next_iterator<const CV::SYMTYPE>>		ObjSymbols;
	typedef	range<next_iterator<const CV::SubSection>>	ObjStreams;

	typedef callback_ref<bool(uint64, void*, size_t)>	get_memory_t;

	class PDB2 : public PDB {
		struct ObjectInfo {
			MOD					*mod;
			//SC				*first_sc;
			PDB_types			*types;
			const_memory_block	debugT;
			mapped_file			obj;
			ObjSymbols		Symbols()		const	{ return mod->Symbols(); }
			ObjStreams		SubStreams()	const	{ return mod->SubStreams(); }
			ObjStreams		SubStreams2()	const;
		};

		hash_map<string, PDB_types*>		type_servers;
		dynamic_array<const CV::PUBSYM32*>	sorted_syms;
		dynamic_array<ObjectInfo>			objs;

		void			LoadObjectFile(ObjectInfo &obj);
	public:
		PDB2() {}
		PDB2(istream_ref file);

		auto&			Modules()	const { return objs; }

		const Line*		LineFromOffset(uint32 addr, const char **fn, uint32 *length, bool &nostep) const;
		uint32			SourceToOffset(const char *src_fn, uint32 &line, uint32 &best_line);

		ObjSymbols		GetSymbols(const SC2 &sc, PDB_types *&types);
		ObjSymbols		GetProcSymbols(CV::segmented32 seg_addr, uint32 &offset, PDB_types *&types);

		const CV::PUBSYM32*	SymFromOffset(CV::segmented32 seg_addr)	const {
			auto	sym	= upper_boundc(sorted_syms, seg_addr, [](CV::segmented32 addr, const CV::PUBSYM32 *sym) { return compare(addr, sym->addr) < 0; })[-1];
			return sym->addr.seg == seg_addr.seg ? sym : nullptr;
		}
		const CV::PUBSYM32** SymFromSection(int sect) {
			return lower_boundc(sorted_syms, sect,	[](const CV::PUBSYM32 *sym, int sect) { return sym->addr.seg < sect; });
		}
		const SC2*		GetSC(CV::segmented32 seg_addr) const {
			return lower_boundc(sec_contribs, seg_addr, [](const SC &sc, CV::segmented32 seg_addr) { return sc.compare(seg_addr.seg, seg_addr.off) > 0; });
		}
	};

	struct ModuleInfo {
		string			fn;
		filename		fn_pdb;
		bool			cantload;
		uint64			base;
		unique_ptr<PDB2>	pdb;
		malloc_block	function_data;	//fpo or pdata

		ModuleInfo(const char *fn = 0) : fn(fn), cantload(false) {}
		CV::segmented32	ToSegmented(uint64 addr)			const { return pdb->ToSegmented(addr - base); }
		uint64			FromSegmented(CV::segmented32 addr)	const { return pdb->FromSegmented(addr) + base; }
		void			SetFunctionData(const const_memory_block &data);
		void			SetDebug(const const_memory_block &data, const get_memory_t &mem);
		void*			GetFunctionTableEntry(uint32 addr) const {
			RUNTIME_FUNCTION *f = lower_boundc(make_range<RUNTIME_FUNCTION>(function_data), addr, [](const RUNTIME_FUNCTION &f, uint32 addr) { return f.BeginAddress <= addr; } );
			return f == function_data ? 0 : f - 1;
		}
	};

	typedef	map<interval<uint64>, ModuleInfo>	Modules;
	Modules		modules;

	void				SetOptions(uint32 _options, const char *_search_path);
	ModuleInfo*			AddModule(const char *fn, uint64 base, uint64 end, bool mapped, const get_memory_t &mem);
	ModuleInfo*			AddModule(HANDLE file, uint64 base);
	Modules::iterator	GetModuleIterator(uint64 addr)		const	{ return modules.lower_bound(addr); }
	ModuleInfo*			GetModule(uint64 addr)				const	{ auto i = GetModuleIterator(addr); return i == modules.end() || i.key().a > addr ? 0 : &*i; }
	void*				GetFunctionTableEntry(uint64 addr)	const;
	PDB2*				GetPdb(ModuleInfo *mod)				const;
	const Line*			LineFromAddr(uint64 addr, const char **fn, uint32 *length, bool &nostep);
	const CV::PROCSYM32* ProcFromAddr(uint64 addr, uint32 &offset, PDB_types *&types);
	const CV::PUBSYM32*	SymFromAddr(uint64 addr, uint64 *disp, uint32 *line, const char **src_fn, const char **mod_fn);
	uint64				SourceToAddr(const char *src_fn, uint32 &line);

	const CV::PUBSYM32*	SymFromName(const char *sym, ModuleInfo *&mod);
};


#endif

//-----------------------------------------------------------------------------
//	CallStackDumper
//-----------------------------------------------------------------------------

struct CallStackFrame {
	uint64				pc;
	uint32				line_displacement;
	uint64				symbol_displacement;
	uint32				line;
	filename			file;
	filename			module_fn;
	fixed_string<256>	symbol;

#ifdef USE_DBGHELP
	CallStackFrame(uint64 pc, HANDLE process, const char *module_fn);
#else
	CallStackFrame(uint64 pc, ProcessSymbols &syms);
#endif

	string_accum &Dump(string_accum &a) const;
	friend string_accum &operator<<(string_accum &a, const CallStackFrame &f) { return f.Dump(a); }
};

struct CallStackDumper {
#ifdef USE_DBGHELP
	struct Module : interval<uint64> {
		filename	fn;
		Module(const interval<uint64> &i, const char *fn) : interval<uint64>(i), fn(fn) {}
		Module(const MODULEENTRY32 &e) : interval<uint64>((uint64)e.modBaseAddr, (uint64)e.modBaseAddr + e.modBaseSize), fn(e.szExePath) {}
	};
	struct Modules : dynamic_array<Module> {
		Modules()	{}
		Modules(HANDLE process) : dynamic_array<Module>(ModuleSnapshot(GetProcessId(process), TH32CS_SNAPMODULE).modules()) {}
		Module	*find(const void *pc) {
			for (auto &i : *this) {
				if (i.contains((uint64)pc))
					return &i;
			}
			return 0;
		}
	} modules;
	HANDLE process;
#else
	ProcessSymbols	syms;
	struct Module : interval<uint64> {
		const char *fn;
		Module(const interval<uint64> &i, const char *fn) : interval<uint64>(i), fn(fn) {}
	};
#endif

public:
	CallStackDumper(uint32 options = 0);
	CallStackDumper(HANDLE _process, uint32 options, const char *search_path);
	~CallStackDumper();

	void			AddModule(const char *fn, uint64 a, uint64 b);
	void			AddModule(HANDLE file, uint64 base);
	const char*		GetModule(uint64 addr);

	CallStackFrame	GetFrame(uint64 addr);
	void			Dump(const uint64 *addr, uint32 num_frames);

	template<int N> void Dump(const CallStacks::Stack<N> &stack)				{ Dump(stack.stack, N); }
	template<int N> void Dump(CallStacks&, const CallStacks::Stack<N> &stack)	{ Dump(stack.stack, N); }
	template<int N> void Dump(CompressedCallStacks &cs, const CompressedCallStacks::Stack<N> &stack) {
		void	*addr[N];
		Dump(addr, cs.Decompress(stack.stack, addr, N));
	}
	range<virtual_iterator<const Module>>	GetModules() const;
};

} //namespace iso
#endif //STACK_DUMP_H

