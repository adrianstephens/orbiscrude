#include "stream.h"
#include "filename.h"

namespace iso {

struct ar_header {
	char	magic[8];
	bool valid() const { return memcmp(magic, "!<arch>\n", 8) == 0; }
};

struct ar_fileheader {
	char	name[16];		// 0  15 File name ASCII
	char	modified[12];	// 16 27 File modification timestamp Decimal
	char	owner[6];		// 28 33 Owner ID Decimal
	char	group[6];		// 34 39 Group ID Decimal
	char	mode[8];		// 40 47 File mode Octal
	char	size[10];		// 48 57 File size in bytes Decimal
	char	magic[2];		// 58 59 File magic 0x60 0x0A
};

struct ranlib {
	uint32	ran_strx;	// string table index of
	uint32	ran_off;	// library member at this offset
};

struct ar_parser {
	istream_ref		file;
	malloc_block	longnames;
	streamptr		ptr;

	struct entry {
		filename	name;
		uint32		size;
	};

	ar_parser(istream_ref file) : file(file) {
		ar_header	a;
		ptr	= file.read(a) && a.valid() ? file.tell() : 0;
	}
	bool	valid() const {
		return !!ptr;
	}

	bool	next(entry &e) {
		for (;;) {
			if (!ptr)
				return false;

			file.seek(ptr);
			file.align(2);
			ar_fileheader	fh;
			if (!file.read(fh))
				return false;
					
			e.size = from_string<uint32>(fh.size);
			ptr	= file.tell() + e.size;

			int	i = 0;
			if (fh.name[0] == '/') {
				if (fh.name[1] == '/') {
					longnames.create(e.size);
					file.readbuff(longnames, e.size);
					continue;

				} else if (is_digit(fh.name[1])) {
					const char *s = longnames + from_string<uint32>(fh.name + 1);
					while ((e.name[i] = s[i]) && s[i] != '/')
						++i;
					e.name[i] = 0;
					return true;
				}
				e.name = ".SYMBOLS";
				return true;
			}

			if (str(fh.name).begins("#1/")) {
				i		= from_string<uint32>(fh.name + 3);
				file.readbuff(e.name, i);
				e.size -= i;
			} else {
				while (i < sizeof(fh.name) && (e.name[i] = fh.name[i])&& fh.name[i] != '/')
					++i;
			}

			e.name[i] = 0;
			return true;
		}
	}

};

} //namespace iso