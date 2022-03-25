#include "base/defs.h"
#include "base/tree.h"
#include "filename.h"
#include "stream.h"

#include <stdio.h>

using namespace iso;

struct NameOrID {
	uint16		i;
	char16		*s;
	NameOrID(const uint16 *p) {
		if (*p == 0xffff) {
			i = p[1];
			s = 0;
		} else {
			i = 0xffff;
			s = (char16*)p;
		}
	}
	size_t	len() { return s ? string_len(s) + 1 : 2; }
	operator uint16() const { return i; }
};

struct RES_HEADER {
	uint32	DataSize;
	uint32	HeaderSize;

	enum RESOURCE_TYPE {
		IRT_NONE			= 0,
		IRT_CURSOR			= 1,
		IRT_BITMAP			= 2,
		IRT_ICON			= 3,
		IRT_MENU			= 4,
		IRT_DIALOG			= 5,
		IRT_STRING			= 6,
		IRT_FONTDIR			= 7,
		IRT_FONT			= 8,
		IRT_ACCELERATOR		= 9,
		IRT_RCDATA			= 10,
		IRT_MESSAGETABLE	= 11,
		IRT_GROUP_CURSOR	= 12,
		IRT_GROUP_ICON		= 14,
		IRT_VERSION			= 16,
		IRT_DLGINCLUDE		= 17,
		IRT_PLUGPLAY		= 19,
		IRT_VXD				= 20,
		IRT_ANICURSOR		= 21,
		IRT_ANIICON			= 22,
		IRT_HTML			= 23,
		IRT_MANIFEST		= 24,
		IRT_TOOLBAR			= 241,
	};

	memory_block		data()			{ return memory_block((uint8*)this + HeaderSize, DataSize); }
	RES_HEADER			*next()			{ return align(data().end(), 4); }
	const_memory_block	data()	const	{ return const_memory_block((uint8*)this + HeaderSize, DataSize); }
	const RES_HEADER	*next() const	{ return align(data().end(), 4); }

	NameOrID			type()	const	{ return (const uint16*)(this + 1); }
	NameOrID			name()	const	{ return (const uint16*)(this + 1) + type().len(); }

	void				set_id(uint16 i) {
		uint16	*p =  (uint16*)(this + 1) + type().len();
		p[0] = 0xffff;
		p[1] = i;
	}
};

struct RES_ICONDIR {
	enum TYPE {
		ICON	= 1,
		CURSOR	= 2,
	};
	struct ENTRY {
		union {
			struct {
				uint8 Width;
				uint8 Height;
				uint8 ColorCount;
				uint8 reserved;
			} Icon;
			struct {
				uint16 Width;
				uint16 Height;
			} Cursor;
		};
		uint16	Planes;
		uint16	BitCount;
		uint32	BytesInRes;
		uint16	id;
	};

	uint16	Reserved;
	uint16	ResType;
	uint16	ResCount;
	range<ENTRY*>		entries()		{ return make_range_n((ENTRY*)(this + 1), ResCount); }
	range<const ENTRY*>	entries() const { return make_range_n((const ENTRY*)(this + 1), ResCount); }
};

struct ResMerger {
	malloc_block	data;
	set<int>		none_ids;
	set<int>		cursor_ids;
	set<int>		icon_ids;
	int				next_none;
	int				next_cursor;
	int				next_icon;

	ResMerger() : next_none(1), next_cursor(1), next_icon(1) {}

	void	merge(const char *fn, bool no_merge) {
		size_t			len = filelength(fn);

		if (len == 0) {
			printf("Can't find %s\n", fn);
			exit(-1);
		}

		printf("Merging %s\n", fn);

		map<int, int>	cursor_renames;
		map<int, int>	icon_renames;

		malloc_block	m(FileInput(fn), len);

		for (auto &i : make_next_range<RES_HEADER>(m)) {
			NameOrID		type	= i.type();
			NameOrID		name	= i.name();

			switch (type) {
				case RES_HEADER::IRT_NONE:
					if (!none_ids.insert(name)) {
						while (!none_ids.insert(next_none))
							++next_none;
						i.set_id(next_none);
					}
					break;

				case RES_HEADER::IRT_CURSOR:
					if (!cursor_ids.insert(name)) {
						while (!cursor_ids.insert(next_cursor))
							++next_cursor;
						i.set_id(next_cursor);
						cursor_renames[name] = next_cursor++;
					}
					break;

				case RES_HEADER::IRT_ICON:
					if (!icon_ids.insert(name)) {
						while (!icon_ids.insert(next_icon))
							++next_icon;
						i.set_id(next_icon);
						cursor_renames[name] = next_icon++;
					}
					break;

				case RES_HEADER::IRT_GROUP_CURSOR: {
					RES_ICONDIR	*dir	= i.data();
					for (auto &j : dir->entries()) {
						if (auto k = cursor_renames.find(j.id))
							j.id = *k;
					}
					break;
				}
				case RES_HEADER::IRT_GROUP_ICON: {
					RES_ICONDIR	*dir	= i.data();
					for (auto &j : dir->entries()) {
						if (auto k = icon_renames.find(j.id))
							j.id = *k;
					}
					break;
				}
			}
		}
		if (!no_merge)
			data += m;
	}
	void	write(const char *fn) {
		FileOutput(fn).write(data);
	}
};

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("%s <dest> <input>...\n", argv[0]);
		exit(-1);
	}

	ResMerger	merger;
	bool	no_merge = false;

	for (int i = 2; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'n':
					no_merge = true;
					break;
			}
		} else {
			merger.merge(argv[i], no_merge);
			no_merge = false;
		}
	}

	merger.write(argv[1]);

	return 0;
}