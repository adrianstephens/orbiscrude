#include "base/strings.h"
#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Microsoft Compound Document
//-----------------------------------------------------------------------------

struct OLEStream {	// 'Ole'
	enum FLAGS {
		LINKED		= 0x00000001,	//OLEStream structure MUST be for a linked object.
		EMBEDDED	= 0x00000000,	//OLEStream structure MUST be for an embedded object.
		HINT		= 0x00001000,	//implementation-specific hint
	};
	uint32		Version;
	uint32		Flags;
	uint32		LinkUpdateOption;
	uint32		Reserved1;

	uint32		ReservedMonikerStreamSize;
	//ReservedMonikerStream;

	uint32		RelativeSourceMonikerStreamSize;	// (optional)
	//RelativeSourceMonikerStream

	uint32		AbsoluteSourceMonikerStreamSize;	// (optional)
	//AbsoluteSourceMonikerStream

	uint32		ClsidIndicator;
	GUID		Clsid;

	uint32		ReservedDisplayName;
	uint32		Reserved2;
	FILETIME	LocalUpdateTime;
	FILETIME	LocalCheckUpdateTime;
	FILETIME	RemoteUpdateTime;
};

struct LengthPrefixedAnsiString {
	uint32	length;
	char	chars[];
};

struct LengthPrefixedUnicodeString {
	uint32	length;
	char16	chars[];
};

struct ClipboardFormatOrUnicodeString {
	enum {
		ABSENT		= 0,
		CLIPBOARD1	= 0xffffffff,
		CLIPBOARD2	= 0xfffffffe
	};
	uint32	MarkerOrLength;
	union {
		uint32	Format;
		char16	Unicode[];
	};
};

struct CompObjStream {	// 'CompObj'
	enum {MARKER = 0x71B239F4};
	uint32	Reserved1;	//ignored
	uint32	Version;	//ignored
	uint8	Reserved2[20];	//ignored;
	/*
	LengthPrefixedAnsiString		AnsiUserType;
	LengthPrefixedAnsiString		AnsiClipboardFormat;
	LengthPrefixedAnsiString		Reserved1;
	uint32							UnicodeMarker;
	LengthPrefixedUnicodeString		UnicodeUserType;
	ClipboardFormatOrUnicodeString	UnicodeClipboardFormat;
	LengthPrefixedUnicodeString		Reserved2;
	*/
};


struct ODT {	// 'ObjInfo'
	enum {
		CF2_RTF		= 0x0001,
		CF2_TEXT	= 0x0002,
		CF2_METAFILE= 0x0003,
		CF2_BITMAP	= 0x0004,
		CF2_DIB		= 0x0005,
		CF2_HTML	= 0x000A,
		CF2_UNICODE	= 0x0014,
	};
	uint16	ODTPersist1;
	uint16	CF;
	uint16	ODTPersist2;// (optional)
};

struct CompDocHeader {
	static const uint64 MAGIC = 0xD0CF11E0A1B11AE1;
	enum SecID {
		FREE		= -1,	// Free sector, may exist in the file, but is not part of any stream
		ENDOFCHAIN	= -2,	// Trailing SecID in a SecID chain
		SAT			= -3,	// Sector is used by the sector allocation table
		MSAT		= -4,	// Sector is used by the master sector allocation table
	};
	//						Offset Size Contents
	uint64be magic;			//0		8	Compound document file identifier: D0H CFH 11H E0H A1H B1H 1AH E1H
	GUID	id;				//8		16	Unique identifier (UID) of this file (not of interest in the following, may be all 0)
	uint16	revision;		//24	2	Revision number of the file format (most used is 003EH)
	uint16	version;		//26	2	Version number of the file format (most used is 0003H)
	uint16	byteorder;		//28	2	Byte order identifier: FEH FFH = Little-EndianFFH FEH = Big-Endian
	uint16	sector_size;	//30	2	Size of a sector in the compound document file in power-of-two (ssz)
	uint16	short_size;		//32	2	Size of a short-sector in the short-stream container stream in power-of-two (sssz)
	uint8	unused1[10];	//34	10	Not used
	uint32	num_sectors;	//44	4	Total number of sectors used for the sector allocation table
	uint32	first_sector;	//48	4	SecID of first sector of the directory stream
	uint32	unused2;		//52	4	Not used
	uint32	min_size;		//56	4	Minimum size of a standard stream (in bytes, minimum allowed 4096); smaller streams are stored as short-streams
	uint32	first_short;	//60	4	SecID of first sector of the short-sector allocation table, or �2
	uint32	num_short;		//64	4	Total number of sectors used for the short-sector allocation table
	uint32	first_master;	//68	4	SecID of first sector of the master sector allocation table, or �2
	uint32	num_master;		//72	4	Total number of sectors used for the master sector allocation table
	uint8	alloc[436];		//76	436	First part of the master sector allocation table containing 109 SecIDs

//	streamptr	sector_offset(uint32 id)	const	{ return 512 + (id << sector_size); }
	streamptr	sector_offset(uint32 id)	const	{ return (id + 1) << sector_size; }
	streamptr	short_offset(uint32 id)		const	{ return id << short_size; }

	bool	valid() const { return magic == MAGIC; }
};

struct CompDocDirEntry {
	enum TYPE {
		Empty		= 0,
		UserStorage	= 1,
		UserStream	= 2,
		LockBytes	= 3,
		Property	= 4,
		RootStorage	= 5,
	};
	enum COLOUR {
		RED, BLACK
	};
	enum {UNUSED = uint32(-1)};
	char16	name[32];
	uint16	name_size;
	uint8	type;
	uint8	colour;
	uint32	left, right, root;
	GUID	guid;
	uint32	flags;
	packed<uint64>	creation, modification;
	uint32	sec_id;
	uint32	size;
	uint32	unused;

	auto	get_name() const { return str(name, name_size / 2 - 1); }
};

struct CompDocMaster {
	const CompDocHeader	&header;
	CompDocDirEntry		root;
	int					*msat, *nsat, *ssat;
	malloc_block		shortcont;

	uint32		sector_size()	const	{ return 1 << header.sector_size; }
	uint32		short_size()	const	{ return 1 << header.short_size; }
	int			next(int id)	const	{ return nsat[id]; }

	size_t		read_sector(istream_ref file, uint32 id, void *buffer) const {
		file.seek(header.sector_offset(id));
		return file.readbuff(buffer, sector_size());
	}

	size_t		read_sector(istream_ref file, uint32 id, void *buffer, size_t size) const {
		file.seek(header.sector_offset(id));
		return file.readbuff(buffer, size);
	}

	size_t		chain_length(istream_ref file, uint32 id) const {
		size_t	size	= 0;
		size_t	ss		= sector_size();
		while (id != CompDocHeader::ENDOFCHAIN) {
			size	+= ss;
			id		= nsat[id];
		}
		return size;
	}
	void		read_chain(istream_ref file, uint32 id, void *buffer, size_t size) const {
		uint8	*p = (uint8*)buffer;
		size_t	ss = sector_size();
		while (size && id != CompDocHeader::ENDOFCHAIN) {
			size_t	r = read_sector(file, id, p, min(ss, size));
			p		+= r;
			size	-= r;
			id		= nsat[id];
		}
	}
	void		read_chain2(istream_ref file, uint32 id, void *buffer, size_t size) const {
		if (size >= header.min_size)
			return read_chain(file, id, buffer, size);

		uint8	*p = (uint8*)buffer;
		size_t	ss = short_size();
		while (id != CompDocHeader::ENDOFCHAIN) {
			size_t	r = min(ss, size);
			memcpy(p, (uint8*)shortcont + ss * id, r);
			p		+= r;
			size	-= r;
			id		= ssat[id];
		}
	}

	CompDocMaster(istream_ref file, const CompDocHeader &_header) : header(_header) {
		uint32	num		= header.num_master;
		uint32	m_size	= 109 + (num << (header.sector_size - 2));
		msat			= new int[m_size];
		memcpy(msat, header.alloc, sizeof(header.alloc));

		int		sect	= header.first_master;
		int		*p		= msat + 109;
		while (num--) {
			p		+= read_sector(file, sect, p) / 4;
			sect	= *--p;
		}

		while (msat[m_size - 1] == CompDocHeader::FREE)
			--m_size;

		uint32	n_size	= m_size << (header.sector_size - 2);
		nsat			= new int[n_size];
		for (int i = 0; i < m_size; i++)
			read_sector(file, msat[i], nsat + (i << (header.sector_size - 2)));

		size_t	s_size	= header.num_short << header.sector_size;
		ssat			= new int[s_size / 4];
		read_chain(file, header.first_short, ssat, s_size);

		file.seek(header.sector_offset(header.first_sector));
		root = file.get();

		shortcont.create(root.size);
		read_chain(file, root.sec_id, shortcont, root.size);
	}
	~CompDocMaster() { delete[] msat; }
};

struct CompDocReader :  CompDocMaster {
	malloc_block		dir_buff;

	CompDocReader(istream_ref file, const CompDocHeader &header) : CompDocMaster(file, header), dir_buff(chain_length(file, header.first_sector)) {
		read_chain(file, header.first_sector, dir_buff, dir_buff.length());
	}

	const CompDocDirEntry*	get(int i) const { return ((CompDocDirEntry*)dir_buff) + i; }

	const CompDocDirEntry* find(const char *name, int i = 0) {
		uint32	stack[32],	*sp = stack;

		for (;;) {
			auto	*e		= get(i);
			if (e->get_name() == name)
				return e;

			if (e->right != CompDocDirEntry::UNUSED)
				*sp++ = e->right;

			i = e->left;
			if (i == CompDocDirEntry::UNUSED) {
				if (sp == stack)
					return 0;
				i = *--sp;
			}
		}
	}

	malloc_block	read(istream_ref file, const CompDocDirEntry *e) const {
		malloc_block	data(e->size);
		read_chain2(file, e->sec_id, data, e->size);
		return move(data);
	}

	size_t	read(istream_ref file, const CompDocDirEntry *e, const memory_block &data) const {
		uint32	size = min(e->size, data.size32());
		read_chain2(file, e->sec_id, data, size);
		return size;
	}

};

}