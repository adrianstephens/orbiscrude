#include "filetypes/code/pe.h"
#include "filename.h"

using namespace iso;

template<typename T> struct OffsetTable {
	uint8	*base;
	uint32	*table;
	const	T*	operator[](int i) { return (T*)(table[i] ? base + table[i] : 0); }
	OffsetTable(uint8 *_base, uint32 *_table) : base(_base), table(_table) {}
};

struct Offsetter {
	uint8	*base;
	Offsetter(void *_base) : base((uint8*)_base) {}
	arbitrary_ptr						get(uint32 off)			const { return off ? base + off : 0; }
	template<typename T> OffsetTable<T>	get_table(uint32 off)	const { return OffsetTable<T>(base, get(off)); }
};

struct DLLPatcher : Offsetter {
	pe::EXPORT_DIRECTORY	*exp;

	uint16	FindOrdinal(const char *name) {
		OffsetTable<char>	names	= get_table<char>(exp->NameTable);
		for (int i = 0, n = exp->NumberEntries; i < n; ++i) {
			if (str(names[i]) == name)
				return i;
		}
		return -1;
	}
	const void	*Lookup(uint16 ord) {
		return ord != 0xffff ? get_table<void>(exp->FunctionTable)[ord] : 0;
	}
	const void	*Lookup(uint16 ord, const char *name) {
		return Lookup(str(get_table<char>(exp->NameTable)[ord]) == name ? ord : FindOrdinal(name));
	}
	const void	*Lookup(const pe::IMPORT_DIRECTORY::BY_NAME *entry) {
		return Lookup(entry->hint, entry->name);
	}

	DLLPatcher(void *_base, uint32 _exp) : Offsetter(_base), exp(get(_exp)) {}
	DLLPatcher(HINSTANCE h) : Offsetter(h) {
		pe::EXE_HEADER			*exe	= (pe::EXE_HEADER*)base;
		pe::FILE_HEADER			*fh		= get(exe->lfanew + 4);
		pe::OPTIONAL_HEADER32	*opt	= (pe::OPTIONAL_HEADER32*)(fh + 1);
		exp	= get(opt->DataDirectory[pe::DATA_DIRECTORY::EXPORT].VirtualAddress);
	}
};

struct DLLPatchee : Offsetter {
	pe::IMPORT_DIRECTORY	*imp;
	void**					iat;

	DLLPatchee(void *_base, uint32 _imp) : Offsetter(_base), imp(get(_imp)) {}
	DLLPatchee(HMODULE h) : Offsetter((void*)((intptr_t)h & ~3)) {
		pe::EXE_HEADER			*exe	= (pe::EXE_HEADER*)base;
		pe::FILE_HEADER			*fh		= get(exe->lfanew + 4);
		pe::OPTIONAL_HEADER32	*opt	= (pe::OPTIONAL_HEADER32*)(fh + 1);
		imp	= get(opt->DataDirectory[pe::DATA_DIRECTORY::IMPORT].VirtualAddress);
		iat	= get(opt->DataDirectory[pe::DATA_DIRECTORY::IAT].VirtualAddress);
	}
};


HMODULE LoadDLL(const filename &fn) {
	HMODULE		h = LoadLibraryEx(fn, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	DLLPatchee	patchee(h);

	for (int j = 0; patchee.imp->desc[j].Characteristics; ++j) {
		pe::IMPORT_DIRECTORY::DESCRIPTOR &desc = patchee.imp->desc[j];
		DLLPatcher	patcher(GetModuleHandle(patchee.get(desc.DllName)));
		const void	**addr	= patchee.get(desc.FirstThunk);
		OffsetTable<pe::IMPORT_DIRECTORY::BY_NAME>	names	= patchee.get_table<pe::IMPORT_DIRECTORY::BY_NAME>(desc.OriginalFirstThunk);

		for (int i = 0; names[i]; ++i) {
			const void	*patch = patcher.Lookup(names[i]);
			addr[i] = patch;
			//ISO_TRACEF(names[i]->name) << "\t at: 0x" << hex((uint32)patch) << '\n';
		}
	}
	return h;
}
