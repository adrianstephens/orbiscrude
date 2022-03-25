#include "defs.h"
#include "array.h"
#include "stream.h"
#include "algorithm.h"
#include "..\filetypes\demangle.h"
#include "..\filetypes\elf.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	ELF
//-----------------------------------------------------------------------------

template<bool be, int bits> class ELF {
public:
	typedef	Elf_Ehdr	<be,bits>	Ehdr;
	typedef	Elf_Shdr	<be,bits>	Shdr;
	typedef	Elf_Sym		<be,bits>	Sym;
	typedef	Elf_Rel		<be,bits>	Rel;
	typedef	Elf_Rela	<be,bits>	Rela;

	int			num_sections;
	int			num_symbols, num_symbols0;
	Ehdr		header;
	Shdr		*sh;
	void		**data;

	Shdr					*sh_sym;
	Sym						*syms;
	int						*sym_indices;
	dynamic_array<string>	strings;
	dynamic_array<string>	sh_strings;

	int		NumSections() {
		return num_sections;
	}
	Shdr*	FindSection(ELF_SHT t)	{
		for (int i = 0, n = NumSections(); i < n; i++) {
			if (sh[i].sh_type == t)
				return sh + i;
		}
		return 0;
	}
	uint32	GetSectionSize(int i) {
		return sh[i].sh_size;
	}
	void	*GetSection(int i)	{
		return data[i];
	}
	void	*GetSection(Shdr *_sh)	{
		return GetSection(_sh - sh);
	}

	int		NumSymbols() {
		return num_symbols;
	}
	Sym		&GetSymbol(int i) {
		return syms[sym_indices[i]];
	}
	Sym		&GetSymbol0(int i) {
		return syms[i];
	}

	const char *GetString(int i) {
		if (i == 0)
			return 0;
		return strings[i - 1];
	}

	void SetString(int i, const char *s) {
		if (i)
			strings[i - 1] = s;
	}

	const char *GetSectionName(int i) {
		if (sh[i].sh_name == 0)
			return 0;
		return sh_strings[sh[i].sh_name - 1];
	}
	void SetSectionName(int i, const char *s) {
		if (int r = sh[i].sh_name)
			sh_strings[r - 1] = s;
	}

	void	RemoveSection(int i, bool verbose = false);
	void	Write(ostream &file);

	ELF(istream &file);
	~ELF();
};

template<bool be, int bits> ELF<be, bits>::ELF(istream &file) {
	char	*names;

	file.seek(0);
	header = file.get();
	file.seek(header.e_shoff);

	num_sections	= header.e_shnum;
	sh				= new Shdr[num_sections];
	data			= new void*[num_sections];

	for (int i = 0; i < num_sections; i++)
		sh[i] = file.get();

	for (int i = 0; i < num_sections; i++) {
		file.seek(sh[i].sh_offset);
		file.readbuff(data[i] = malloc(sh[i].sh_size), sh[i].sh_size);
	}

	names	= (char*)GetSection(header.e_shstrndx);
	for (int i = 0; i < num_sections; i++) {
		if (sh[i].sh_name) {
			sh_strings.push_back(names + int(sh[i].sh_name));
			sh[i].sh_name = sh_strings.size();
		}
	}

	sh_sym			= FindSection(SHT_SYMTAB);
	syms			= (Sym*)GetSection(sh_sym);
	names			= (char*)GetSection(sh_sym->sh_link);
	num_symbols		= sh_sym->sh_size / sizeof(Sym);
	num_symbols0	= num_symbols;
	sym_indices		= new int[num_symbols];

	for (int i = 0; i < num_symbols; i++) {
		sym_indices[i]	= i;
		Sym			&s	= syms[i];
		if (s.st_name) {
			strings.push_back(names + int(s.st_name));
			s.st_name = strings.size();
		}
	}
}

template<bool be, int bits> ELF<be, bits>::~ELF() {
	for (int i = 0; i < num_sections; i++)
		free(data[i]);
	delete[] data;
	delete[] sh;
}

template<bool be, int bits> void ELF<be, bits>::Write(ostream &file) {
	char	*names, *p;
	size_t	name_len;
	int		ix;

	size_t	start		= sizeof(header);
	header.e_shoff		= start;
	header.e_shnum		= num_sections;
	file.write(header);

	name_len = 1;
	for (int i = 0; i < num_sections; i++) {
		if (const char *name = GetSectionName(i))
			name_len += str(name).length() + 1;
	}

	ix				= header.e_shstrndx;
	free(data[ix]);
	data[ix]		= malloc(name_len);
	sh[ix].sh_size	= name_len;

	names	= (char*)data[ix];
	p		= names;
	*p++ = 0;
	for (int i = 0; i < num_sections; i++) {
		if (const char *name = GetSectionName(i)) {
			sh[i].sh_name	= p - names;
			int		len		= str(name).length() + 1;
			memcpy(p, name, len);
			p += len;
		}
	}

	name_len = 1;
	for (int i = 0; i < num_symbols; i++) {
		ELF32be::Sym	&sym = GetSymbol(i);
		if (const char *name = GetString(sym.st_name))
			name_len += str(name).length() + 1;
	}

	ix				= sh_sym->sh_link;
	free(data[ix]);
	data[ix]		= malloc(name_len);
	sh[ix].sh_size	= name_len;
	names	= (char*)data[ix];
	p		= names;
	*p++ = 0;

	size_t	sym_len	= num_symbols * sizeof(Sym);
	sh_sym->sh_size = sym_len;
	ix				= sh_sym - sh;
	data[ix]		= malloc(sym_len);
	Sym	*syms2		= (Sym*)data[ix];

	int	*rev_indices= new int[num_symbols0];
	for (int i = 0; i < num_symbols0; i++)
		rev_indices[i] = -1;

	for (int i = 0; i < num_symbols; i++) {
		Sym	&s = syms2[i] = GetSymbol(i);
		int	r	= sym_indices[i];
		ISO_ASSERT(r >= 0 && r < num_symbols0);
		rev_indices[r] = i;
		if (const char *name = GetString(s.st_name)) {
			s.st_name	= p - names;
			int		len	= str(name).length() + 1;
			memcpy(p, name, len);
			p += len;
		}
	}

	for (int i = 0; i < num_sections; i++) {
		Shdr	&s = sh[i];
		switch (s.sh_type) {
			case SHT_REL: {
				Rel		*r	= (Rel*)data[i];
				int		n	= s.sh_size / sizeof(*r);
				while (n--) {
					int	t = ELF32_R_TYPE(r->r_info);
					int	b = ELF32_R_SYM(r->r_info);
					r->r_info = ELF32_R_INFO(rev_indices[b], t);
					r++;
				}
				break;
			}
			case SHT_RELA: {
				Rela	*r	= (Rela*)data[i];
				int		n	= s.sh_size / sizeof(*r);
				while (n--) {
					int	t = ELF32_R_TYPE(r->r_info);
					int	b = ELF32_R_SYM(r->r_info);
					r->r_info = ELF32_R_INFO(rev_indices[b], t);
					r++;
				}
				break;
			}
		}
	}

	delete[] rev_indices;

	start	+= num_sections * sizeof(Shdr);
	for (int i = 0; i < num_sections; i++) {
		sh[i].sh_offset = start;
		start	+= sh[i].sh_size;
		file.write(sh[i]);
	}
	for (int i = 0; i < num_sections; i++)
		file.writebuff(data[i], sh[i].sh_size);
}

template<bool be, int bits> void ELF<be, bits>::RemoveSection(int i, bool verbose) {
	num_sections--;

	for (int j = i; j < num_sections; j++) {
		sh[j]	= sh[j + 1];
		data[j] = data[j + 1];
	}

	for (int j = 0; j < num_sections; j++) {
		if (sh[j].sh_link > i)
			sh[j].sh_link = sh[j].sh_link - 1;
		if (sh[j].sh_info > i)
			sh[j].sh_info = sh[j].sh_info - 1;
	}

	if (header.e_shstrndx > i)
		header.e_shstrndx = header.e_shstrndx - 1;

	sh_sym	= FindSection(SHT_SYMTAB);

	int	ns = 0;
	for (int j = 0; j < num_symbols; j++) {
		int		r = sym_indices[j];
		Sym&	s = syms[r];
		int16	x = int16(s.st_shndx);
		if (x == i) {
			if (verbose)
				printf("Removing symbol %s\n", GetString(s.st_name));
		} else {
			sym_indices[ns++] = r;
			if (x > i)
				s.st_shndx = x - 1;
		}
	}
	num_symbols = ns;
}

typedef ELF<true,32>	ELF32be;

bool ConvertObject(const char *in, const char *objout, int verbose) {

	FileInput	fin(in);

	unsigned char	e_ident[EI_NIDENT];
	fin.readbuff(e_ident, sizeof(e_ident));

	dynamic_array<string>	converted;

	if (e_ident[EI_MAG0] == 0x7f
	&&	e_ident[EI_MAG1] == 'E'
	&&	e_ident[EI_MAG2] == 'L'
	&&	e_ident[EI_MAG3] == 'F') {
		bool	be		= e_ident[EI_DATA]	== ELFDATA2MSB;
		int		bits	= e_ident[EI_CLASS] == ELFCLASS32 ? 32 : 64;
		if (!be || bits != 32)
			return false;

		ELF32be			elf(fin);
#if 0
		for (int i = 0; i < elf.NumSections(); i++) {
			if (const char *name = elf.GetSectionName(i)) {
				if (str(name) == ".group" || str(name) == ".gnu.attributes" || str(name).find('_') || str(name).find("startup")) {
					if (verbose)
						printf("Removing Section %s\n", name);
					elf.RemoveSection(i--, verbose);
				}
			}
		}
#endif
#if 1
		uint32	*uses = new uint32[elf.NumSections()];
		for (int i = 0, n = elf.NumSections(); i < n; i++)
			uses[i] = 0;

		for (int i = 0, n = elf.NumSymbols(); i < n; i++) {
			ELF32be::Sym	&sym	= elf.GetSymbol(i);
			int16			x		= int16(sym.st_shndx);
			if (sym.st_name && x >= 0 && x < elf.NumSections())
				uses[x]++;
		}

		for (int i = 0, n = elf.NumSections(); i < n; i++) {
			if (str(elf.GetSectionName(i), 10) == ".rela.text") {
				ELF32be::Shdr	&s = elf.sh[i];
				ELF32be::Rela	*r	= (ELF32be::Rela*)elf.GetSection(i);
				int		n	= s.sh_size / sizeof(*r);
				while (n--) {
					int				b		= ELF32_R_SYM(r->r_info);
					ELF32be::Sym	&sym	= elf.GetSymbol0(b);
					int16			x		= int16(sym.st_shndx);
					if (x >= 0 && x < elf.NumSections())
						uses[x]++;
					r++;
				}
			}
		}

		for (int i = 0, n = elf.NumSections(); i < n; i++) {
			ELF32be::Shdr	&s = elf.sh[i];
			if (s.sh_addralign > 4)
				s.sh_addralign = 4;

			switch (s.sh_type) {
				case SHT_REL:
				case SHT_RELA:
					uses[i] = uses[s.sh_info];
					break;
				case SHT_NULL:
				case SHT_SYMTAB:
				case SHT_STRTAB:
					uses[i] = 1;
					break;
			}
		}

		for (int i = 0, j = 0, n = elf.NumSections(); i < n; i++, j++) {
			if (uses[i] == 0) {
				const char *name = elf.GetSectionName(j);
				if (str(name).find(".ctors"))
					continue;
//				if (str(name).find(".debug"))
//					continue;
				if (verbose)
					printf("Removing Section %s\n", name);
				elf.RemoveSection(j--, verbose > 0);
			}
		}
#endif
		for (int i = 0, n = elf.NumSymbols(); i < n; i++) {
			ELF32be::Sym	&sym = elf.GetSymbol(i);
			if (const char *name = elf.GetString(sym.st_name)) {
				fixed_string<1024> name1 = name;
//				if (char *p = name1.find('.'))
//					*p = 0;
				fixed_string<1024> name2 = enmangle(name1);
				if (!name2.blank()) {
#if 1
					bool	found = false;
					for (int j = 0; !found && j < i; j++)
						found = name2 == elf.GetString(elf.GetSymbol(j).st_name);
					if (found)
						continue;
					elf.SetString(sym.st_name, name2);
#else
					bool	ok = true;
					for (char *p = name2.begin(), c; (c = *p++) && ok; ) {
						ok = ok && (
							(c >= 'A' && c <= 'Z')
						||	(c >= 'a' && c <= 'z')
						||	(c >= '0' && c <= '9')
						||	c == '_');
					}
					if (ok) {
						string	*s = find(converted, name2);
						if (s == converted.end()) {
							converted.push_back(name2);
							fout.write((const char*)format_string("%s = %s;\r\n", (const char*)name2, name));
						}
					}
#endif
				}
			}
		}

		for (int i = 0; i < elf.NumSections(); i++) {
			if (const char *name = elf.GetSectionName(i)) {
//				if (str(name).find(".debug"))
//					continue;
				if (str(name).find('_')) {
					if (verbose)
						printf("Renaming Section %s to ", name);
					if (const char *dot = str(name).rfind('.'))
						*const_cast<char*>(dot) = 0;
					if (verbose)
						printf("%s\n", name);
//					elf.SetSectionName(i, str(name).rfind('.'));
				}
				if (str(name) == ".text") {
					void		*text0	= elf.GetSection(i);
					uint32be	*text	= (uint32be*)text0;
					uint32		mask	= 0xfc1f0000;
					uint32		stfd	= 0xd8010000;
					uint32		lfd		= 0xc8010000;
					uint32		psqst	= 0xf0010000;
					uint32		psql	= 0xe0010000;
					uint32		mask2	= 0xffe00000;
					int			nst = 0, nl = 0;

					uint32		stored[32] = {0};

					for (uint32 size = elf.GetSectionSize(i) / 4; size--; text++) {
						uint32	a = *text;
						if ((a & mask)== stfd) {
							int		r		= (a >> 21) & 31;
#if 1
							if (r >= 14) {
								if (abs((uint16)a) < 0x800) {
									*text		= a ^ (stfd ^ psqst);
									stored[r]	= a;
									nst++;
								} else {
									printf("!Danger: stack offset > 12 bits (%i) at %s:0x%08x\n", (int16)a, in, (uint8*)text - (uint8*)text0);
									stored[r]	= 0;
								}
							}
#else
							uint32	used	= (a & 0x03e00000) | 0xfc000000;	// any double-prec opcode
							uint32	b		= a ^ (stfd ^ lfd);					// corresponding lfd
							bool	skip	= false;
							for (uint32be *p = text, *e = p + size; !skip && p < e; p++) {
								uint32	o = *p;
								if (o == b)
									break;
								skip = (o & mask2) == used;
							}
							if (skip) {
								dubs[r] = 0;
							} else {
								dubs[r] = a;
								*text	= a ^ (stfd ^ psqst);
								nst++;
							}
#endif
						} else if ((a & mask) == lfd) {
							int		r		= (a >> 21) & 31;
#if 1
							if (r >= 14) {
								if (stored[r] == (a ^ (stfd ^ lfd))) {
									*text = a ^ (lfd ^ psql);
									nl++;
								} else {
									if (verbose > 1)
										printf("Warning: skipping potential restore at offset 0x%08x\n", (uint8*)text - (uint8*)text0);
								}
							}
#else
							if (dubs[r]) {
								*text = a ^ (lfd ^ psql);
								nl++;
							}
#endif
						}
					}
					if (verbose && (nst || nl))
						printf("Changed %i stores and %i loads of doubles from the stack to paired-single stores and loads\n", nst, nl);
//					ISO_ASSERT(nl == nst);
				}
			}
		}

		if (objout)
			elf.Write(FileOutput(objout));

		return true;
	}

	return false;
}

int main(int argc, char *argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
//	const char *name = "_ZN3iso6ps_sinERKDv2_f";
	const char *name = "_Z14isolink_listent";

	//iso::ps_sin(float __vector(2) const&)
	//gcc:	_ZN3iso6ps_sinERKDv2_f
	//cw:	ps_sin__3isoFRC2

	//conv:	ps_sin__3isoFRCU8__vectorf
	//gcc:	_ZN3iso6ps_sinERKU8__vectorf

	fixed_string<1024> name2 = enmangle(name);

	const char *files[2];
	bool	verbose	= false;
	int		nfiles	= 0;

	for (int i = 1; i < argc; i++) {
		const char *s = argv[i];
		if (s[0] == '-') {
			switch (s[1]) {
				case 'v':
					verbose = s[2] != '-';
					break;
			}
		} else if (nfiles < num_elements(files)) {
			files[nfiles++] = s;
		}
	}

	if (nfiles < 2) {
		trace_accum("Usage: isoobj [-v] <input_obj> <output_obj>\n");
		exit(-1);
	}

	if (!ConvertObject(files[0], files[1], verbose)) {
		trace_accum("error\n");
		exit(-1);
	}
	return 0;
}