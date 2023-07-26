#include "plist.h"

#include "iso/iso_files.h"
#include "extra/date.h"
#include "hashes/SHA.h"
#include "archive_help.h"
#include "filetypes/bin.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	binary plist
//-----------------------------------------------------------------------------

class BPLISTFileHandler : public FileHandler {
public:
	const char*		GetExt() override { return "bplist"; }
	const char*		GetDescription() override { return "OSX Binary Property List"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<bplist::Header>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		bplist_reader	reader(file);
		if (!reader.valid())
			return ISO_NULL;
	#if 0
		ISO_ptr<anything>	p(id);
		for (int i = 0, n = reader.table.size(); i < n; ++i)
			p->Append(reader.get_element(i));
		return p;
	#endif
/*		auto	p = reader.get_root(id);
		ISO::Browser2	b(p);
		ISO::Browser2	objects = b["$objects"];
		if (int root = b["$top"]["root"].GetInt()) {
			ISO::Browser2	objects = b[root];
		}*/
		return ISO::Duplicate<32>(reader.get_root(id));
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		bplist_writer	writer(file);
		writer.set_max_elements(writer.count_elements(p));
		writer.set_root(writer.add(ISO_ptr_machine<void>(p)));
		return true;
	}
} bpl;

//-----------------------------------------------------------------------------
//	ascii plist
//-----------------------------------------------------------------------------

class PLISTFileHandler : public FileHandler {
	const char*		GetDescription() override { return "OSX ASCII Property List"; }
	const char*		GetExt() override { return "plist"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		auto	text = make_text_reader(file);
		fixed_string<256>	line;
		while (text.read_line(line)) {
			string_scan	ss(line);
			const char	*p = ss.skip_whitespace().getp();
			if (str(p).begins("//"))
				continue;
			return *p == '(' || *p == '{' ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
		}
		return CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		if (bpl.Check(file) > 0)
			return bpl.Read(id, file);

		file.seek(0);
		PLISTreader	reader(file);
		return reader.get_item(id);
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		PLISTwriter	writer(file);
		writer.put_item(ISO::Browser(p));
		return true;
	}
} plist;

//-----------------------------------------------------------------------------
//	xml plist
//-----------------------------------------------------------------------------

class XPLISTFileHandler : public FileHandler {
	const char*		GetDescription() override { return "XML Property List"; }
	const char*		GetExt() override { return "plist"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return XPLISTreader(file).valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		XPLISTreader	reader(file);
		if (!reader.valid())
			return ISO_NULL;
		return reader.get_item(id);
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		XPLISTwriter	writer(file);
		writer.put_item(ISO::Browser(p));
		return true;
	}
} xplist;

//-----------------------------------------------------------------------------
//	.DS_Store
//-----------------------------------------------------------------------------

struct BuddyAllocator {
	struct BTreeHeader {
		uint32be	root;		// block number of the root node of the B-tree
		uint32be	height;		// number of levels of internal nodes
		uint32be	num_recs;	// number of records in the tree
		uint32be	num_nodes;	// number of nodes in the tree
		uint32be	page_size;	// always 0x1000
	};

	struct BTreeNode {
		uint32be	P;			//0: leaf, else internal
		uint32be	count;
		//leaf:		followed by <count> records
		//internal: followed by <count> x {block number of the left child; record}
		//The ordering of records is by case-insensitive comparison of their filenames, secondarily sorted on the structure ID (record type) field
	};

	struct _BlockAddress {
		uint32	log2size:5, offset:27;
	};
	struct BlockAddress : T_swap_endian_type<_BlockAddress>::type {
		uint32	Offset()	const	{ return get().offset * 32; }
		uint32	Size()		const	{ return 1 << get().log2size; }
	};

	struct Directory {
		typedef next_iterator<Directory> iterator;
		union {
			_pascal_string<uint8,char>							name;
			after<packed<uint32be>, _pascal_string<uint8,char>>	block;
		};
		BTreeHeader*	header(const BuddyAllocator *a)	const	{ return a->GetAllocBlock(block.get()); }
		Directory*		next()							const	{ return (Directory*)(&block + 1); }
	};

	struct FreeList {
		typedef next_iterator<FreeList> iterator;
		uint32be		count;
		uint32be		offsets[1];
		FreeList*		next()	const	{ return (FreeList*)(offsets + count); }
	};

	struct BookKeeping {
		struct directories {
			typedef Directory::iterator iterator;
			uint32be	num_dirs;
			Directory	dirs[];
			iterator	begin() { return dirs; }
			iterator	end()	{ return iterator(dirs, num_dirs, 0); }
		};

		uint32be		num_alloc;
		uint32be		unknown;
		BlockAddress	alloc[];

		range<BlockAddress*>				GetAllocBlocks() {
			return make_range_n(alloc, num_alloc);
		}
		range<Directory::iterator>			GetDirectories();/* {
			return *(directories*)(alloc + align(num_alloc, 256));
		}*/
		range<next_iterator<FreeList> >		FreeLists() {
			FreeList	*f0 = (FreeList*)&*GetDirectories().end();
			return range<FreeList::iterator>(f0, FreeList::iterator(f0, 32, 0));
		}
	};

	enum {MAGIC = 'Bud1'};
	uint32be	magic;
	uint32be	bookkeeping_offset;
	uint32be	bookkeeping_size;
	uint32be	bookkeeping_offset2;
	uint8		unknown[16];

	BookKeeping		*GetBookKeeping() const {
		return (BookKeeping*)((char*)this + bookkeeping_offset);
	}
	memory_block	GetBlock(const BlockAddress &b) const {
		return memory_block((char*)this + b.Offset(), b.Size());
	}
	memory_block	GetAllocBlock(int i) const {
		return GetBlock(GetBookKeeping()->alloc[i]);
	}
	bool			valid() const {
		return magic == MAGIC && bookkeeping_offset == bookkeeping_offset2;
	}
};

//template<> struct T_has_begin<BuddyAllocator::BookKeeping::directories&>	: T_true {};
//template<> struct T_has_begin<BuddyAllocator::BookKeeping::directories&, void_t<decltype(begin(declval<BuddyAllocator::BookKeeping::directories>()))>>	: T_true {};

range<BuddyAllocator::Directory::iterator>			BuddyAllocator::BookKeeping::GetDirectories() {
	return *(directories*)(alloc + align(num_alloc, 256));
}

struct DSStoreVersion {
	uint32be	version;	//?	1
};

struct DSStore : DSStoreVersion, BuddyAllocator {
	struct Record {
		enum TYPE {
			BKGD	= 'BKGD',		//12-byte blob, directories only. Indicates the background of the Finder window viewing this directory (in icon mode). see BKGD_s
			ICVO	= 'ICVO',		//bool, directories only. Unknown meaning. Always seems to be 1, so presumably 0 is the default value.
			Iloc	= 'Iloc',		//16-byte blob, attached to files and directories. See iloc_s
			LSVO	= 'LSVO',		//bool, attached to directories. Purpose unknown.
			bwsp	= 'bwsp',		//A blob containing a binary plist. This contains the size and layout of the window (including whether optional parts like the sidebar or path bar are visible). This appeared in Snow Leopard (10.6).
					 				//The plist contains the keys WindowBounds (a string in the same format in which AppKit saves window frames); SidebarWidth (a float), and booleans ShowSidebar, ShowToolbar, ShowStatusBar, and ShowPathbar. Sometimes contains ViewStyle (a string), TargetURL (a string), and TargetPath (an array of strings).
			cmmt	= 'cmmt',		//ustr, containing a file's "Spotlight Comments". (The comment is also stored in the com.apple.metadata:kMDItemFinderComment xattr; this copy may be historical.)
			dilc	= 'dilc',		//32-byte blob, attached to files and directories. Unknown, may indicate the icon location when files are displayed on the desktop.
			dscl	= 'dscl',		//bool, attached to subdirectories. Indicates that the subdirectory is open (disclosed) in list view.
			extn	= 'extn',		//ustr. Often contains the file extension of the file, but sometimes contains a different extension. Purpose unknown.
			fwi0	= 'fwi0',		//16-byte blob, directories only. Finder window information. See fwi0_s
			fwsw	= 'fwsw',		//long, directories only. Finder window sidebar width, in pixels/points. Zero if collapsed.
			fwvh	= 'fwvh',		//shor, directories only. Finder window vertical height. If present, it overrides the height defined by the rect in fwi0. The Finder seems to create these (at least on 10.4) even though it will do the right thing for window height with only an fwi0 around, perhaps this is because the stored height is weird when accounting for toolbars and status bars.
			GRP0	= 'GRP0',		//ustr. Unknown; I've only seen this once.
			icgo	= 'icgo',		//8-byte blob, directories (and files?). Unknown. Probably two integers, and often the value 00 00 00 00 00 00 00 04.
			icsp	= 'icsp',		//8-byte blob, directories only. Unknown, usually all but the last two bytes are zeroes.
			icvo	= 'icvo',		//18- or 26-byte blob, directories only. Icon view options. There seem to be two formats for this blob.
					 				//If the first 4 bytes are "icvo", then 8 unknown bytes (flags?), then 2 bytes corresponding to the selected icon view size, then 4 unknown bytes 6e 6f 6e 65 (the text "none", guess that this is the "keep arranged by" setting?).
					 				//If the first 4 bytes are "icv4", then: two bytes indicating the icon size in pixels, typically 48; a 4CC indicating the "keep arranged by" setting (or none for none or grid for align to grid); another 4CC, either botm or rght, indicating the label position w.r.t. the icon; and then 12 unknown bytes (flags?).
					 				//Of the flag bytes, the low-order bit of the second byte is 1 if "Show item info" is checked, and the low-order bit of the 12th (last) byte is 1 if the "Show icon preview" checkbox is checked. The tenth byte usually has the value 4, and the remainder are zero.
			icvp	= 'icvp',		//A blob containing a plist, giving settings for the icon view. Appeared in Snow Leopard (10.6), probably supplanting 'icvo'.
					 				//The plist holds a dictionary with several key-value pairs: booleans showIconPreview, showItemInfo, and labelOnBottom; numbers scrollPositionX, scrollPositionY, gridOffsetX, gridOffsetY, textSize, iconSize, gridSpacing, and viewOptionsVersion; string arrangeBy.
					 				//The value of the backgroundType key (an integer) presumably controls the presence of further optional keys such as backgroundColorRed/backgroundColorGreen/backgroundColorBlue.
			icvt	= 'icvt',		//shor, directories only. Icon view text label (filename) size, in points.
			info	= 'info',		//40- or 48-byte blob, attached to directories and files. Unknown. The first 8 bytes look like a timestamp as in dutc.
			logS	= 'logS',
			lg1S	= 'lg1S',		//comp, directories only. Appears to contain the logical size in bytes of the directory's contents, perhaps as a cache to speed up display in the Finder. I think that 'logS' appeared in 10.7 and was supplanted by 'lg1S' in 10.8. See also 'ph1S'.
			lssp	= 'lssp',		//8-byte blob, directories only. Unknown. Possibly the scroll position in list view mode?
			lsvo	= 'lsvo',		//76-byte blob, directories only. List view options. Seems to contain the columns displayed in list view, their widths, and their sort ordering if any. Presumably supplanted by lsvp and/or lsvP.
					 				//These list view settings are shared between list view and the list portion of coverflow view.
			lsvt	= 'lsvt',		//shor, directories only. List view text (filename) size, in points.
			lsvp	= 'lsvp',		//A blob containing a binary plist. List view settings, perhaps supplanting the 'lsvo' record. Appeared in Snow Leopard (10.6).
					 				//The plist contains boolean values for the keys showIconPreview, useRelativeDates, and calculateAllSizes; numbers for scrollPositionX, scrollPositionY, textSize, iconSize, and viewOptionsVersion (typically 1); and a string, sortColumn.
					 				//There is also a columns key containing the set of columns, their widths, visibility, column ordering, sort ordering. The only difference between lsvp and lsvP appears to be the format of the columns specification: an array or a dictionary.
			lsvP	= 'lsvP',		//A blob containing a binary plist. Often occurs with lsvp, but may have appeared in 10.7 or 10.8.
			modD	= 'modD',
			moDD	= 'moDD',		//dutc timestamps; directories only. One or both may appear. Typically the same as the directory's modification date. Unknown purpose; appeared in 10.7 or 10.8. Possibly used to detect when logS needs to be recalculated?
			phyS	= 'phyS',
			ph1S	= 'ph1S',		//comp, directories only. This number is always a multiple of 8192 and slightly larger than 'logS' / 'lg1S', which always seems to be present if this is (though the reverse is not always true). Presumably it is the corresponding physical size (an integer number of 8k-byte disk blocks).
			pict	= 'pict',		//Variable-length blob, directories only. Despite the name, this contains not a PICT image but an Alias record (see Inside Macintosh: Files) which resolves to the file containing the actual background image. See also 'BKGD'.
			vSrn	= 'vSrn',		//long, attached to directories. Always appears to contain the value 1. Appeared in 10.7 or 10.8.
			vstl	= 'vstl',		//type, directories only. Indicates the style of the view
		};
		enum DATA {
			data_long	= 'long',	//An integer (4 bytes)
			data_shor	= 'shor',	//A short integer? Still stored as four bytes, but the first two are always zero.
			data_bool	= 'bool',	//A boolean value, stored as one byte.
			data_blob	= 'blob',	//An arbitrary block of bytes, stored as an integer followed by that many bytes of data
			data_type	= 'type',	//Four bytes, containing a FourCharCode.
			data_ustr	= 'ustr',	//A Unicode text string, stored as an integer character count followed by 2*count bytes of data in UTF-16.
			data_comp	= 'comp',	//An eight-byte (64-bit) integer
			data_dutc	= 'dutc',	//A datestamp, represented as an 8-byte integer count of the number of (1/65536)-second intervals since the Mac epoch in 1904
		};

		struct BKGD_s {
			enum TYPE {
				Default = 'DefB',
				Solid	= 'ClrB',
				Picture	= 'PctB',
			};
			uint32be	type;
			union {
				struct { uint16be r, g, b; };
				struct { uint32be pict_len; };	//length of the blob stored in the 'pict' record
			};
		};
		struct Iloc_s {
			uint32be	x, y;
			uint8		unknown[8];	// 6x ff, 2x 0
		};
		struct fwi0_s {
			enum VIEW {
				icon		= 'icnv',
				column		= 'clmv',
				list		= 'Nlsv',
				coverflow	= 'Flwv',
			};
			uint16be	top, left, bottom, right;
			uint32be	view;
			uint32be	unknown;	//either zeroes or 00 01 00 00.
		};

		struct Header {
			packed<uint32be>	id;
			packed<uint32be>	type;
		};
	};

	static ISO_ptr<void> ReadData(byte_reader &br);
	void				AddNode(ISO_ptr<anything> &p, uint32 block);
	ISO_ptr<void>		ReadDirectory(tag id, const BTreeHeader *header);
	ISO_ptr<anything>	directories;
};

inline DateTime MacTime(uint64 v) {
	return DateTime(1904, 1, 1) + Duration::Secs(v / 65536.0);
}

ISO_ptr<void> DSStore::ReadData(byte_reader &br) {
	const Record::Header	*d		= br.get_ptr();
	auto					id		= str((char*)&d->id, 4);
	switch (d->type) {
		case 'long':	return ISO_ptr<uint32>(id, br.get<uint32be>());
		case 'shor':	return ISO_ptr<uint16>(id, br.get<uint16be>());
		case 'bool':	return ISO_ptr<bool8>(id, br.get<uint8>());
		case 'blob': {
			const_memory_block	mb = br.get_block(br.get<uint32be>());
			switch (d->id) {
				case 'BKGD': return ISO::MakePtr(id, (const Record::BKGD_s*)mb);
				case 'Iloc': return ISO::MakePtr(id, (const Record::Iloc_s*)mb);
				case 'fwi0': return ISO::MakePtr(id, (const Record::fwi0_s*)mb);
				case 'bwsp':
				case 'icvp':
				case 'lsvp':
				case 'lsvP': return bpl.Read(id, memory_reader(mb).me());
				default:	return ISO::MakePtr(id, mb);
			}
		}
		case Record::data_type:	return ISO_ptr<uint32>(id, br.get<uint32be>());
		case Record::data_ustr: {
			uint32	len = br.get<uint32be>();
			return ISO_ptr<string>(id, str((const char16be*)br.get_block(len * 2), len));
		}
		case Record::data_comp:	return ISO_ptr<uint64>(id, br.get<uint64be>());
		case Record::data_dutc:	return ISO_ptr<DateTime>(id, MacTime(br.get<uint64be>()));
		default:				return ISO_NULL;
	}
}

void DSStore::AddNode(ISO_ptr<anything> &p, uint32 block) {
	const BTreeNode *node	= GetAllocBlock(block);
	byte_reader		br(node + 1);

	for (int i = 0; i < node->count; i++) {
		if (node->P)
			AddNode(p, br.get<uint32be>());
		uint32	name_len	= br.get<uint32be>();
		tag2	name		= str((const char16be*)br.get_block(name_len * 2), name_len);
		int		j			= p->GetIndex(name);
		ISO_ptr<anything>	p1 = j < 0 ? p->Append(ISO_ptr<anything>(name)) : (*p)[j];
		p1->Append(ReadData(br));
	}
}

ISO_ptr<void> DSStore::ReadDirectory(tag id, const BTreeHeader *header) {
	ISO_ptr<anything>	p(id);//, header->num_recs);
	AddNode(p, header->root);
	return p;
}

ISO_DEFCOMPV(DSStore::Record::BKGD_s, type, r, g, b);
ISO_DEFCOMPV(DSStore::Record::Iloc_s, x, y);
ISO_DEFCOMPV(DSStore::Record::fwi0_s, top, left, bottom, right, view);
ISO_DEFUSERCOMPV(DSStore, version, directories);

class DSStoreFileHandler : public FileHandler {
	const char*		GetExt() override { return "DS_Store"; }
	const char*		GetDescription() override { return "DS_Store"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		uint32	len	= file.size32();
		DSStore	*p	= (DSStore*)ISO::MakeRawPtrSize<32>(ISO::getdef<DSStore>(), id, len);

		readn(file, p, len);

		p->directories.Create(0);
		for (auto &i : p->GetBookKeeping()->GetDirectories())
			p->directories->Append(p->ReadDirectory(str(i.name), p->GetAllocBlock(i.block.get())));

		return ISO_ptr<DSStore>::Ptr(p);
	}
} dsstore;

//-----------------------------------------------------------------------------
//	AppleDouble
//-----------------------------------------------------------------------------

struct AppleDouble {
	enum {
		SINGLE	= 0x00051600,
		DOUBLE	= 0x00051607,
	};
	enum {
		Data_fork		= 1,	// standard Macintosh data fork
		Resource_fork	= 2,	// standard Macintosh resource fork
		Real_name		= 3,	// file's name in its home file system
		Comment			= 4,	// standard Macintosh comments
		Icon_BW			= 5,	// standard Macintosh black-and-white icon
		Icon_color		= 6,	// Macintosh color icon
		file_info		= 7,	// file information: attributes and so on
		Finder_info		= 9,	// standard Macintosh Finder information
	};
	struct header : packed_types<bigendian_types> {
		uint32	magic;
		uint32	version;
		char	filesystem[16];
		uint16	count;
		bool valid() const { return magic == DOUBLE; }
	};
	struct entry : bigendian_types {
		uint32	id;
		uint32	offset;
		uint32	size;
	};
};

class AppleDoubleFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Mac Resource Fork"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		AppleDouble::header	h;
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		static const char *names[] = {
			0,
			"Data_fork",
			"Resource_fork",
			"Real_name",
			"Comment",
			"Icon_BW",
			"Icon_color",
			"file_info",
			0,
			"Finder_info",
		};

		AppleDouble::header	h	= file.get();
		if (!h.valid())
			return ISO_NULL;

		int					n	= h.count;
		AppleDouble::entry	*e	= alloc_auto(AppleDouble::entry, n);
		file.readbuff(e, sizeof(AppleDouble::entry) * n);

		ISO_ptr<anything>	p(id);
		for (int i = 0; i < n; i++) {
			file.seek(e[i].offset);
			tag	name = e[i].id < num_elements(names) ? names[e[i].id] : 0;
			p->Append(ReadRaw(name, file, e[i].size));
		}
		return p;
	}
} appledouble;

//-----------------------------------------------------------------------------
//	MBDB (iPhone backup)
//-----------------------------------------------------------------------------
struct MBDB {
	struct File : packed_types<bigendian_types> {
		uint16	Mode;		// Unix file permissions. file mode: 0xAxxx symbolic link (aka S_IFLNK or 00120000), 0x4xxx directory (aka S_IFDIR or 0040000), 0x8xxx regular file (aka S_IFREG or 0100000), Mask out ~ 0xf000 (aka S_IFMT) for file permissions
		uint64	inode;		// inode number
		uint32	uid;		// owner
		uint32	gid;		// group
		uint32	mtime;		// time of last modification
		uint32	atime;		// time of last access
		uint32	ctime;		// time of last change of status
		uint64	length;		// file size (always 0 for link or directory)
	};

	//char	id[6];	// 'mbdb\5\0'

	string	Domain;				//	Backup domain
	string	Path;
	string	LinkTarget;			// absolute path
	string	DataHash;			// SHA-1 of file contents, actual file objects only
	string	encryptionKey;		// Encryption key for encrypted backups
	File	file;

	uint8	protectionclass;	// unknown
	uint8	PropertyCount;		// number of properties following

	struct Property {
		string	name;
		string	value;	// can be a string or binary content
	};

	static string read_string(istream_ref file) {
		string	s;
		uint16	len = file.get<uint16be>();
		if (len != 0xffff && len != 0) {
			file.readbuff(s.alloc(len), len);
			s[len] = 0;
		}
		return s;
	}

	bool	read(istream_ref file) {
		Domain				= read_string(file);	//	Backup domain
		Path				= read_string(file);
		LinkTarget			= read_string(file);	// absolute path
		DataHash			= read_string(file);	// SHA-1 of file contents, actual file objects only
		encryptionKey		= read_string(file);	// Encryption key for encrypted backups

		file.read(file);

		protectionclass		= file.getc();	// unknown
		PropertyCount		= file.getc();	// number of properties following
		return true;
	}

};

/*
DOMAINS
"AppDomain-com.some.user.installed.app",
"CameraRollDomain",
"DatabaseDomain"
"HomeDomain",
"KeychainDomain",
"ManagedPreferencesDomain",
"MediaDomain",
"MobileDeviceDomain",
"RootDomain",
"SystemPreferencesDomain",
"WirelessDomain",
... others?
*/

class MBDBFileHandler : public FileHandler {
	const char*		GetExt() override { return "mbdb"; }
	const char*		GetDescription() override { return "iPhone backup database"; }
	int				Check(istream_ref file) override {
		char	test[6];
		file.seek(0);
		return file.read(test) && memcmp(test, "mbdb\5\0", 6) == 0 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	//To determine the actual filename corresponding to a record (this will be the actual file in the mobile backup directory), calculate a sha-1 checksum of the Domain and Path seperated by '-' as follows:
	//  SHA1(<Domain>-<Path>)
	string GetFullPath(const char *domain, const char *path) {
		return path ? str(domain) + "-" + str(path) : string(domain);
	}
	auto GetFilename(const char *domain, const char *path) {
		string	full = GetFullPath(domain, path);
		SHA1	hash(full, full.length());
		//ISO_TRACEF() << hash.getcode() << ": " << full << '\n';
		return to_string((SHA1::CODE)hash);
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		char	test[6];
		file.seek(0);
		if (!file.read(test) || memcmp(test, "mbdb\5\0", 6) != 0)
			return ISO_NULL;

		ISO_ptr<Folder>	p(id);

		for (streamptr end = file.length(); file.tell() < end; ) {
			MBDB	mbdb;
			file.read(mbdb);

			const char *name	= 0;
			ISO_ptr<Folder>	p1	= GetDir(GetDir0(p, mbdb.Domain), mbdb.Path, &name);

			ISO_ptr<anything>	props(name);
			filename			hash	= GetFilename(mbdb.Domain, mbdb.Path).begin();
			filename			fn2		= FileHandler::FindAbsolute(hash);
			if (fn2.exists()) {
				uint32	size	= uint32(filelength(fn2));
				props->Append(ReadRaw(hash, FileInput(fn2).me(), size));
			}

			//props->Append(ISO_ptr<string>("hash", GetFilename(mbdb.Domain, mbdb.Path)));
			for (int i = 0; i < mbdb.PropertyCount; i++) {
				string	name = MBDB::read_string(file);
				string	data = MBDB::read_string(file);
				props->Append(ISO_ptr<string>(name, data));
			}
			p1->Append(props);
		}
		return p;
	}
} mbdb;

//-----------------------------------------------------------------------------
//	XML plist (eg. GarageBand)
//-----------------------------------------------------------------------------

class XMLplistFileHandler : public FileHandler {
	const char*		GetDescription() override { return "XML plist"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		XPLISTreader	xml(file);
		return xml.get_item(id);
	}
} xml_plist;
