#include "iso/iso_files.h"
#include "archive_help.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	varint
//-----------------------------------------------------------------------------

class varint {
	uint64	v;

	static uint64 read(istream_ref file) {
		uint64	v = 0;
		for (int i = 0; i < 8; i++) {
			int	c = file.getc();
			if ((c & 0x80) == 0)
				return (v << 7) | c;
			v = (v << 7) | (c & 0x7f);
		}
		return (v << 8) | file.getc();
	};
public:
	varint()								{}
	varint(istream_ref file) : v(read(file))	{}
	varint&	operator=(istream_ref file)		{ v = read(file); return *this; }

	operator uint64()	const	{ return v; }

};

//-----------------------------------------------------------------------------
//	SQLiteFile
//-----------------------------------------------------------------------------

struct SQLiteFile {

	enum ENCODING {
		UTF8	= 1,
		UTF16le	= 2,
		UTF16be	= 3,
	};
	struct header : bigendian_types {
		fixed_string<16> id;		// The header string: "SQLite format 3\000"
		uint16	page_size;			// The database page size in bytes. Must be a power of two between 512 and 32768 inclusive, or the value 1 representing a page size of 65536.
		uint8	write_version;		// File format write version. 1 for legacy; 2 for WAL.
		uint8	read_version;		// File format read version. 1 for legacy; 2 for WAL.
		uint8	reserved_size;		// Bytes of unused "reserved" space at the end of each page. Usually 0.
		uint8	max_payload;		// Maximum embedded payload fraction. Must be 64.
		uint8	min_payload;		// Minimum embedded payload fraction. Must be 32.
		uint8	leaf_payload;		// Leaf payload fraction. Must be 32.
		uint32	change_count;		// File change counter.
		uint32	num_pages;			// Size of the database file in pages. The "in-header database size".
		uint32	first_free;			// Page number of the first freelist trunk page.
		uint32	num_free;			// Total number of freelist pages.
		uint32	schema_cookie;		// The schema cookie.
		uint32	schema_format;		// The schema format number. Supported schema formats are 1, 2, 3, and 4.
		uint32	cache_size;			// Default page cache size.
		uint32	largest_root_page;	// The page number of the largest root b-tree page when in auto-vacuum or incremental-vacuum modes, or zero otherwise.
		uint32	text_encoding;		// The database text encoding. A value of 1 means UTF-8. A value of 2 means UTF-16le. A value of 3 means UTF-16be.
		uint32	user_version;		// The "user version" as read and set by the user_version pragma.
		uint32	incremental_vacuum;	// True (non-zero) for incremental-vacuum mode. False (zero) otherwise.
		uint8	_reserved[24];		// Reserved for expansion. Must be zero.
		uint32	version_valid_for;	// The version-valid-for number.
		uint32	version;			// SQLITE_VERSION_NUMBER
	};

	enum BTREE_PAGE_TYPE {
		BTP_TABLE			= 5,
		BTP_INDEX			= 2,
		BTP_LEAF			= 8,

		BTP_INTERIOR_INDEX	= BTP_INDEX,			// the page is a non-leaf index b-tree page
		BTP_INTERIOR_TABLE	= BTP_TABLE,			// the page is a non-leaf table b-tree page
		BTP_LEAF_INDEX		= BTP_LEAF | BTP_INDEX,	// the page is a leaf index b-tree page
		BTP_LEAF_TABLE		= BTP_LEAF | BTP_TABLE,	// the page is a leaf table b-tree page
	};
	struct btree_header : packed_types<bigendian_types> {
		uint8	type;				// One of BTREE_PAGE_TYPE
		uint16	first_free;			// Byte offset into the page of the first freeblock
		uint16	num_cells;			// Number of cells on this page
		uint16	content;			// Offset to the first byte of the cell content area. A zero value is used to represent an offset of 65536, which occurs on an empty root page when using a 65536-byte page size.
		uint8	fragment_total;		// Number of fragmented free bytes within the cell content area
//		uint32	next;				// The right-most pointer (interior b-tree pages only)
	};

	struct freeblock : packed_types<bigendian_types> {
		uint16	next;
		uint16	size;
	};

	enum COL_TYPE {
		//Serial Type			Size				Meaning
		CT_NULL		= 0,	//	0					NULL
		CT_INT8		= 1,	//	1					8-bit twos-complement integer
		CT_INT16	= 2,	//	2					Big-endian 16-bit twos-complement integer
		CT_INT24	= 3,	//	3					Big-endian 24-bit twos-complement integer
		CT_INT32	= 4,	//	4					Big-endian 32-bit twos-complement integer
		CT_INT48	= 5,	//	6					Big-endian 48-bit twos-complement integer
		CT_INT64	= 6,	//	8					Big-endian 64-bit twos-complement integer
		CT_FLOAT64	= 7,	//	8					Big-endian IEEE 754-2008 64-bit floating point number
		CT_ZERO		= 8,	//	0					Integer constant 0. Only available for schema format 4 and higher.
		CT_ONE		= 9,	//	0					Integer constant 1. Only available for schema format 4 and higher.
		//10,11 Not used. Reserved for expansion.
		CT_BLOB		= 12,	//N>=12 even (N-12)/2	A BLOB that is (N-12)/2 bytes in length
		CT_STRING	= 13,	//N>=13 odd (N-13)/2	A string in the database encoding and (N-13)/2 bytes in length. The nul terminator is omitted.
	};

	istream_ref	file;
	header	h;
	uint32	page_size;
	uint32	usable;
	uint32	num_pages;

public:

	struct Page  {
		Page			*parent;
		uint32			index;

		SQLiteFile		*sql;
		uint32			page, next_page, ncells;
		uint32			*prev_pages;
		uint16be		*cells;
		varint			*rowids;
		uint8			type;

		uint32			next(int i)			const { return i < ncells - 1 ? prev_pages[i + 1] : next_page; }
		uint32			prev(int i)			const { return prev_pages[i]; }
		const const_memory_block payload(int i)	const;
		Page(SQLiteFile *_sql, uint32 _page, Page *_parent, uint32 _index);
		~Page();
	};

	struct Cell : const_memory_block {
		uint32		count;
		uint32		*offsets;
		uint8		*types;

		const const_memory_block	GetField(int i)	const	{ return slice(offsets[i], offsets[i + 1] - offsets[i]); }

		struct iterator {
			Cell			*cell;
			int				i;
			iterator(Cell *_cell) : cell(_cell), i(0)	{}
			iterator		&operator++()			{ ++i; return *this;				}
			operator		bool()			const	{ return i < cell->count;			}
			COL_TYPE		type()			const	{ return COL_TYPE(cell->types[i]);	}
			const const_memory_block operator*()	const	{ return cell->GetField(i);			}
		};
	public:
		Cell(const const_memory_block &m);
		~Cell();
		iterator	begin()	{ return iterator(this); }
	};

	struct iterator {
		Page			*page;
		int				i;

		iterator(Page *_page) : page(_page), i(0)	{}
		iterator		&next_child();

		iterator		&operator++();
		const varint	&key()		const	{ return page->rowids[i];	}
		bool			leaf()		const	{ return !!(page->type & BTP_LEAF); }
		operator		bool()		const	{ return !!page;			}
		Cell			operator*()	const	{ return page->payload(i);	}
	};

	SQLiteFile(istream_ref _file) : file(_file) {
		h			= file.get();
		page_size	= h.page_size;
		if (page_size == 1)
			page_size = 0x10000;
		usable		= page_size - h.reserved_size;
		num_pages	= uint32(file.length() / page_size);
	}

	iterator	begin(int base = 1)	{ return iterator(new Page(this, base, 0, 0)).next_child(); }
	bool		valid()				{ return h.id == "SQLite format 3"; }
};

//-------------------------------------
//	SQLiteFile::iterator
//-------------------------------------

SQLiteFile::iterator& SQLiteFile::iterator::next_child() {
	while (page) {
		while (!(page->type & BTP_LEAF)) {
			page	= new Page(page->sql, page->prev(i), page, i);
			i		= 0;
		}
		if (page->ncells > 0)
			break;
		i		= page->index;
		page	= page->parent;
	}
	return *this;
}

SQLiteFile::iterator& SQLiteFile::iterator::operator++() {
	while (page && (++i == page->ncells)) {
		i		= page->index;
		page	= page->parent;
	}
	return next_child();
}

//-------------------------------------
//	SQLiteFile::Page
//-------------------------------------

SQLiteFile::Page::Page(SQLiteFile *_sql, uint32 _page, Page *_parent, uint32 _index) : sql(_sql), page(_page), parent(_parent), index(_index) {
	uint64			page_start	= uint64(page - 1) * sql->page_size;
	sql->file.seek(page_start + (page == 1 ? sizeof(SQLiteFile::header) : 0));

	btree_header	b	= sql->file.get();
	type		= b.type;
	ncells		= b.num_cells;
	cells		= new uint16be[ncells];

	next_page	= type & BTP_LEAF ? 0 : (int)get(sql->file.get<uint32be>());
	prev_pages	= type & BTP_LEAF ? 0 : new uint32[ncells];
	rowids		= type & BTP_TABLE ? new varint[ncells] : 0;

	sql->file.readbuff(cells, ncells * 2);

	for (int i = 0; i < ncells; i++) {
		sql->file.seek(page_start + cells[i]);

		if (prev_pages)
			prev_pages[i] = sql->file.get<uint32be>();

		varint	payload;
		if (type != BTP_INTERIOR_TABLE)
			payload	= sql->file;

		if (rowids)
			rowids[i] = sql->file;
	}
}

SQLiteFile::Page::~Page() {
	delete[] cells;
	delete[] rowids;
	delete[] prev_pages;
}

const const_memory_block SQLiteFile::Page::payload(int i) const {
	if (type == BTP_INTERIOR_TABLE)
		return empty;

	sql->file.seek(uint64(page - 1) * sql->page_size + cells[i] + (prev_pages ? 4 : 0));

	varint	payload	= sql->file;
	uint32	total	= uint32(payload);

	if (rowids)
		(varint)sql->file;

	uint32	usable		= sql->usable;
	uint32	maxlocal	= type & BTP_INDEX ? (usable - 12) * 64 / 255 - 23 : usable - 35;

	malloc_block	mb(total);

	if (total <= maxlocal) {
		sql->file.readbuff(mb, total);
	} else {
#if 1
		uint32	minlocal = (usable - 12) * 32 / 255 - 23;
		int		surplus	= minlocal + (total - minlocal) % (usable - 4);
		uint32	offset;
		if (surplus <= maxlocal)
			offset = surplus;
		else
			offset = minlocal;
//		uint32	offset	= minlocal + (total - minlocal) % (usable - 4);
	//	if (offset > maxlocal)
	//		offset = minlocal;
#else
		uint32	minlocal	= (usable - 12) * 32 / 255 - 23;
		uint32	offset		= min(uint32(minlocal + (total - minlocal) % (usable - 4)), maxlocal);
#endif
		sql->file.readbuff(mb, offset);

		for (uint32 p = sql->file.get<uint32be>(); total < offset && p && p < sql->num_pages; ) {
			sql->file.seek(p * uint64(sql->page_size));
			p		= sql->file.get<uint32be>();
			offset	+= sql->file.readbuff((uint8*)mb + offset,  min(total - offset, usable - 4));
		}
	}
	return mb.detach();
}

//-------------------------------------
//	SQLiteFile::Cell
//-------------------------------------

SQLiteFile::Cell::Cell(const const_memory_block &mb) : const_memory_block(mb) {
	memory_reader	header(mb);
	varint		len		= istream_ref(header);
	streamptr	start	= header.tell();

	ISO_ASSERT(len <= mb.length());

	count	= 0;
	while (header.tell() < len) {
		count++;
		(varint)header;
	}
	offsets	= new uint32[count + 1];
	types	= new uint8[count];

	static const uint8 sizes[] = {0,1,2,3,4,6,8,8,0,0,0,0};

	header.seek(start);
	uint32	offset	= uint32(len);
	for (int i = 0; i < count; i++) {
		varint	type	= istream_ref(header);//header;
		offsets[i]		= offset;
		offset += (type < CT_BLOB ? sizes[type] : (type - CT_BLOB) / 2);
		types[i]		= type < CT_BLOB ? type : CT_BLOB + (type & 1);
	}
	offsets[count]		= offset;
};

SQLiteFile::Cell::~Cell() {
	delete[] offsets;
	delete[] types;
}

//-----------------------------------------------------------------------------
//	SQLiteFileHandler
//-----------------------------------------------------------------------------

class SQLiteFileHandler : public FileHandler {
	ISO_ptr<anything>		Read(tag id, SQLiteFile &sql, int base, const char **schema, size_t schema_size);

	const char*		GetExt() override { return "db";	}
	const char*		GetDescription() override { return "SQLite database";	}
	int				Check(istream_ref file) override { file.seek(0); return file.get<fixed_string<16> >() == "SQLite format 3" ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} sqlite;

ISO_ptr<anything> SQLiteFileHandler::Read(tag id, SQLiteFile &sql, int base, const char **schema, size_t schema_size) {
	ISO_ptr<anything>	p(id);
	for (SQLiteFile::iterator i = sql.begin(base); i; ++i) {
		int	j = 0;
		ISO_ptr<anything>	p2(j < schema_size ? schema[j++] : 0);
		SQLiteFile::Cell	cell = *i;
		for (SQLiteFile::Cell::iterator i = cell.begin(); i; ++i) {
			const_memory_block	mb(*i);
			tag2			id	= j < schema_size ? schema[j++] : 0;
			ISO_ptr<void>	p3;
			switch (i.type()) {
				case SQLiteFile::CT_INT8:		p3	= MakePtr(id, *(const uint8*)mb);			break;
				case SQLiteFile::CT_INT16:		p3	= MakePtr(id, *(const uint16*)mb);			break;
				case SQLiteFile::CT_INT24:		p3	= MakePtr(id, uint32(*(const uintn<3>*)mb));break;
				case SQLiteFile::CT_INT32:		p3	= MakePtr(id, *(const uint32*)mb);			break;
				case SQLiteFile::CT_INT48:		p3	= MakePtr(id, uint64(*(const uintn<6>*)mb));break;
				case SQLiteFile::CT_INT64:		p3	= MakePtr(id, *(const uint64*)mb);			break;
				case SQLiteFile::CT_FLOAT64:	p3	= MakePtr(id, *(const float64*)mb);			break;
				case SQLiteFile::CT_NULL:		p3	= ISO_ptr<void>(id);						break;
				case SQLiteFile::CT_ZERO:		p3	= ISO_ptr<int>(id, 0);						break;
				case SQLiteFile::CT_ONE:		p3	= ISO_ptr<int>(id, 1);						break;
				case SQLiteFile::CT_BLOB: {
					ISO_ptr<ISO_openarray<xint8> >	m(id, mb.size32());
					memcpy(*m, mb, mb.length());
					p3 = m;
					break;
				}
				case SQLiteFile::CT_STRING:
					ISO_ptr<string>	m(id, mb.length());
					memcpy(m->begin(), mb, mb.length());
					p3 = m;
					break;
			}
			p2->Append(p3);
		}
		p->Append(p2);
	}

	return p;
};

ISO_ptr<void> SQLiteFileHandler::Read(tag id, istream_ref file) {
	SQLiteFile	sql(file);
	if (!sql.valid())
		return ISO_NULL;


	const char *schema[] = {
		"sqlite_master",
		"type",			// text,
		"name",			// text,
		"tbl_name",		// text,
		"rootpage",		// integer,
		"sql",			// text
	};

	ISO_ptr<anything>	p = Read(id, sql, 1, schema, num_elements(schema));
	ISO::Browser			b(p);
	for (int i = 0, n = b.Count(); i < n; i++) {
		if (int base = b[i]["rootpage"].GetInt())
			p->Append(Read(b[i]["name"].GetString(), sql, base, 0, 0));
	}
	return p;
}

