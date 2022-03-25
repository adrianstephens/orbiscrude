#include "../filetypes/code/elf.h"
#include "base/strings.h"
#include "extra/indexer.h"
#include "vm.h"

#include <stdio.h>

using namespace iso;

struct ELF : Elf_types<false, 64> {
	const uint8	*start;
	const Ehdr	*h;
	const Phdr	*ph;
	const Shdr	*sh;
	const char	*shn;

	const Sym	*syms;
	const char	*sym_names;
	uint32		nsyms;

	const Shdr*	get_section(uint32 i) {
		return sh + i;
	}
	const Shdr*	get_section(const char *name) {
		for (const Shdr *s = sh, *e = s + h->e_shnum; s != e; ++s) {
			if (str(shn + s->sh_name) == name)
				return s;
		}
		return 0;
	}
	memory_block	get_section_data(const Shdr *s)	{
		return memory_block(const_cast<uint8*>(start) + s->sh_offset, s->sh_size);
	}
	memory_block	get_section_data(uint32 i) {
		return get_section_data(sh + i);
	}
	const Sym*		get_symbol(const char *s) {
		for (const Sym *i = syms, *e = i + nsyms; i != e; ++i) {
			if (str(sym_names + int(i->st_name)) == s)
				return i;
		}
		return 0;
	}
	const char*		get_name(const Sym *s) {
		return sym_names + int(s->st_name);
	}

	void Init(const uint8 *p) {
		static const uint8 verify[] = {0x7f, 'E', 'L', 'F', ELFCLASS64, ELFDATA2LSB, EV_CURRENT};
		if (!p || memcmp(((Ehdr*)p)->e_ident, verify, sizeof(verify)) != 0) {
			clear(*this);
			return;
		}

		start	= p;
		h		= (Ehdr*)p;
		ph		= (Phdr*)(p + h->e_phoff);
		sh		= (Shdr*)(p + h->e_shoff);
		shn		= get_section_data(h->e_shstrndx);
		syms	= 0;
		nsyms	= 0;

		for (int i = 1, n = h->e_shnum; i < n; i++) {
			if (sh[i].sh_type == SHT_SYMTAB) {
				syms		= get_section_data(i);
				sym_names	= get_section_data(sh[i].sh_link);
				nsyms		= sh[i].sh_size / sizeof(Sym);
				break;
			}
		}
	}
	ELF()				{ clear(*this); }
	ELF(const uint8 *p)	{ Init(p); }

	bool	IsValid()	{ return !!start; }
};

int main(int argc, char *argv[]) {

	if (argc < 2) {
		printf("%s <elffile>\n", argv[0]);
		exit(-1);
	}

	mapped_file	m(argv[1], 0, true);
	ELF			elf(m);
	if (!elf.IsValid()) {
		printf("%s not a valid elf file\n", argv[1]);
		exit(-1);
	}

	int	*indices	= new int[elf.nsyms];
	for (int i = 0; i < elf.nsyms; i++)
		indices[i] = i;

	auto	idx		= make_range_n(indices, elf.nsyms);
	auto	syms	= make_index_container(elf.syms, idx);

	sort(syms, [](const ELF::Sym &a, const ELF::Sym &b) {
		return a.st_value < b.st_value;
	});

	for (auto i = syms.begin(), e = syms.end(); i != e; ++i) {
		const ELF::Sym	&a = *i;

		if (a.type() == STT_FUNC && a.st_size == 0) {
			auto j = i;
			while (++j != e) {
				if (j->type() == STT_FUNC) {
					unconst(a).st_size = j->st_value - a.st_value;
					break;
				}
			}
		}
	}

	return 0;
}
