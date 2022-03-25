#include "ar.h"

#include "iso/iso_files.h"
#include "../container/archive_help.h"
#include "bin.h"
#include "demangle.h"

using namespace iso;

class ARFileHandler : FileHandler {
	const char*		GetExt() override { return "a";	}
	const char*		GetDescription() override { return "gnu library file";	}

	int				Check(istream_ref file) override {
		ar_header	a;
		file.seek(0);
		return file.read(a) && a.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ar_parser	a(file);
		if (!a.valid())
			return ISO_NULL;

		bool	raw			= WantRaw();
		malloc_block		longnames;
		ISO_ptr<anything>	p(id);

		ar_parser::entry	e;
		while (a.next(e)) {
			if (e.name == "__.SYMDEF" || e.name == "__.SYMDEF SORTED") {
				malloc_block	m(file, e.size);
				uint32			size1 = *(uint32*)m;
				ranlib			*r		= m + sizeof(uint32);
				int				nr		= size1 / sizeof(ranlib);
				const char	*strings	= m + sizeof(uint32) * 2 + size1;

				ISO_ptr<ISO_openarray<pair<string,uint32> > > p1(e.name, nr);
				for (int i = 0; i < nr; i++)
					(*p1)[i] = make_pair(strings + r[i].ran_strx, r[i].ran_off);
				p->Append(p1);

			} else if (e.name == ".SYMBOLS") {
				uint32		n	= file.get();
				if (n > 0x100000) {
					n = (uint32be&)n;
					file.seek_cur(n * sizeof(uint32));
				} else {
					file.seek_cur(n * 4);
					n	= file.get();
					file.seek_cur(n * 2);
				}

				ISO_ptr<ISO_openarray<string> > names("symbols", n);
				for (int j = 0; j < n; j++)
					read(file, (*names)[j]);

				if (!ISO::root("variables")["nodemangle"].GetInt()) {
					for (int j = 0; j < n; j++)
						(*names)[j] = demangle((*names)[j]);
				}

				p->Append(names);

			} else {
				p->Append(ReadData1(e.name, file, e.size, raw));
			}
		}

		return p;
	}

	void	WriteFile(const char *name, const memory_block &m, ostream_ref file) {
		uint64			modtime	= 0;
		uint16			owner	= 502;
		uint16			group	= 20;
		uint16			mode	= 0100644;

		ar_fileheader	fh;
		memset(&fh, ' ', sizeof(fh));
		size_t			data_len = m.length();
		size_t			name_len = strlen(name);

		if (name_len < 16) {
			memcpy(fh.name, name, name_len);
			fh.name[name_len] = '/';
		} else {
			sprintf(fh.name, "#1/%i", (int)name_len);
			fh.name[strlen(fh.name)] = ' ';
			data_len += name_len;
		}

		to_string(fh.modified, modtime);
		to_string(fh.owner, owner);
		to_string(fh.group, group);
		to_string(fh.mode, oct(mode));
		to_string(fh.size, data_len);

		fh.magic[0] = 0x60;
		fh.magic[1] = 0x0a;

		file.align(2, '\n');
		file.write(fh);

		if (name_len >= 16)
			file.writebuff(name, name_len);

		file.writebuff(m, m.length());
	}

	bool	Write(ISO_ptr<void> p, ostream_ref file) override {
		file.write("!<arch>\n");

		if (p.IsType<anything>()) {
			anything *dir = p;
			for (int i = 0, n = dir->Count(); i < n; i++) {
				if (ISO_ptr<void> p = (*dir)[i])
					WriteFile(p.ID().get_tag(), GetRawData(p), file);
			}
		} else {
			WriteFile(p.ID().get_tag(), GetRawData(p), file);
		}

		return true;
	}


} arfile;

class LIBFileHandler : ARFileHandler {
	const char*		GetExt() override { return "lib";	}
	const char*		GetDescription() override { return "windows library file";	}
} lib;
