#include "iso/iso_files.h"
#include "extra/disk.h"
#include "extra/date.h"
#include "codec/apple_compression.h"
#include "hashes/SHA.h"
#include "comms/bz2_stream.h"
#include "comms/zlib_stream.h"
#include "archive_help.h"
#include "plist.h"
#include "bin.h"

#ifdef PLAT_PC
#include "windows/win_file.h"
#endif

using namespace iso;

//-----------------------------------------------------------------------------
//	HFS+ structs
//-----------------------------------------------------------------------------
#pragma pack(1)

struct HFSPlus {

	#undef	SF_ARCHIVED
	#undef	SF_IMMUTABLE
	#undef	SF_APPEND
	#undef	UF_NODUMP
	#undef	UF_IMMUTABLE
	#undef	UF_APPEND
	#undef	UF_OPAQUE

	struct BSDInfo : bigendian_types {
		enum AdminFlags {
			SF_ARCHIVED		= 1 << 0,	//File has been archived
			SF_IMMUTABLE	= 1 << 1,	//File may not be changed
			SF_APPEND		= 1 << 2,	//Writes to file may only append
		};
		enum OwnerFlags {
			UF_NODUMP		= 1 << 0,	//Do not dump (back up or archive) this file
			UF_IMMUTABLE	= 1 << 1,	//File may not be changed
			UF_APPEND		= 1 << 2,	//Writes to file may only append
			UF_OPAQUE		= 1 << 3,	//Directory is opaque (see below)
		};

		enum FileMode {
			ISUID	= 0004000,	// set user id on execution
			ISGID	= 0002000,	// set group id on execution
			ISTXT	= 0001000,	// sticky bit

			IRWXU	= 0000700,	// RWX mask for owner
			IRUSR	= 0000400,	// R for owner
			IWUSR	= 0000200,	// W for owner
			IXUSR	= 0000100,	// X for owner

			IRWXG	= 0000070,	// RWX mask for group
			IRGRP	= 0000040,	// R for group
			IWGRP	= 0000020,	// W for group
			IXGRP	= 0000010,	// X for group

			IRWXO	= 0000007,	// RWX mask for other
			IROTH	= 0000004,	// R for other
			IWOTH	= 0000002,	// W for other
			IXOTH	= 0000001,	// X for other

			IFMT	= 0170000,	// type of file mask
			IFIFO	= 0010000,	// named pipe (fifo)
			IFCHR	= 0020000,	// character special
			IFDIR	= 0040000,	// directory
			IFBLK	= 0060000,	// block special
			IFREG	= 0100000,	// regular
			IFLNK	= 0120000,	// symbolic link
			IFSOCK	= 0140000,	// socket
			IFWHT	= 0160000,	// whiteout
		};

		uint32	ownerID;
		uint32	groupID;
		uint8	adminFlags;
		uint8	ownerFlags;
		uint16	fileMode;
		uint32	special;
	//	union {
	//		uint32	iNodeNum;
	//		uint32	linkCount;
	//		uint32	rawDevice;
	//	} special;
	};


	struct ForkData : bigendian_types {
		struct Extent {
			uint32		startBlock;
			uint32		blockCount;
		};
		uint64	logicalSize;
		uint32	clumpSize;
		uint32	totalBlocks;
		Extent	extents[8];

		void read(istream_ref file, size_t blocksize, void *buffer, size_t read) const {
			char	*p	= (char*)buffer, *pe = p + read;
			for (const Extent *ext = extents; p < pe; ext++) {
				file.seek(ext->startBlock * blocksize);
				p	+= file.readbuff(p, min(ext->blockCount * blocksize, pe - p));
			}
		}
	};

	enum VolumeAttributes {
		// Bits 0-6 are reserved
		kVolumeHardwareLockBit		=  7,
		kVolumeUnmountedBit			=  8,
		kVolumeSparedBlocksBit		=  9,
		kVolumeNoCacheRequiredBit	= 10,
		kBootVolumeInconsistentBit	= 11,
		kCatalogNodeIDsReusedBit	= 12,
		kVolumeJournaledBit			= 13,
		// Bit 14 is reserved
		kVolumeSoftwareLockBit		= 15
		// Bits 16-31 are reserved
	};

	enum CatalogNodeID {
		kRootParentID				= 1,
		kRootFolderID				= 2,
		kExtentsFileID				= 3,
		kCatalogFileID				= 4,
		kBadBlockFileID				= 5,
		kAllocationFileID			= 6,
		kStartupFileID				= 7,
		kAttributesFileID			= 8,
		kRepairCatalogFileID		= 14,
		kBogusExtentFileID			= 15,
		kFirstUserCatalogNodeID		= 16
	};

	struct VolumeHeader : bigendian_types {
		uint16			signature;
		uint16			version;
		uint32			attributes;
		uint32			lastMountedVersion;
		uint32			journalInfoBlock;

		uint32			createDate;
		uint32			modifyDate;
		uint32			backupDate;
		uint32			checkedDate;

		uint32			fileCount;
		uint32			folderCount;

		uint32			blockSize;
		uint32			totalBlocks;
		uint32			freeBlocks;

		uint32			nextAllocation;
		uint32			rsrcClumpSize;
		uint32			dataClumpSize;
		CatalogNodeID	nextCatalogID;

		uint32			writeCount;
		uint64			encodingsBitmap;

		uint32			finderInfo[8];

		ForkData		allocationFile;
		ForkData		extentsFile;
		ForkData		catalogFile;
		ForkData		attributesFile;
		ForkData		startupFile;

		bool valid() const {
			return signature == 'H+';
		}
	};

	struct BTNodeDescriptor : bigendian_types {
		enum {
			kBTLeafNode		= -1,
			kBTIndexNode	=  0,
			kBTHeaderNode	=  1,
			kBTMapNode		=  2
		};

		uint32		fLink;
		uint32		bLink;
		int8		kind;
		uint8		height;
		uint16		numRecords;
		uint16		reserved;
	};

	struct BTHeaderRec : bigendian_types {
		enum BTreeTypes{
			kHFSBTreeType				=   0,	// control file
			kUserBTreeType				= 128,	// user btree type starts from 128
			kReservedBTreeType			= 255
		};
		enum {
			kBTBadCloseMask				= 0x00000001,
			kBTBigKeysMask				= 0x00000002,
			kBTVariableIndexKeysMask	= 0x00000004
		};
		struct iterator {
			const BTNodeDescriptor	*node;
			const uint16			*offsets;
			iterator(const BTNodeDescriptor *node, const uint16 *offsets) : node(node), offsets(offsets) {}
			const void*	get()					const	{ return node ? (const char*)node + *offsets : nullptr; }
			bool operator==(const iterator &b)	const	{ return offsets == b.offsets; }
			bool operator!=(const iterator &b)	const	{ return offsets != b.offsets; }
		};
		template<typename T> struct iteratorT : iterator {
			iteratorT(nullptr_t = nullptr)	: iterator(nullptr, nullptr) {}
			iteratorT(const BTNodeDescriptor *node, const uint16be *offsets) : iterator(node, offsets) {}
			const T*	operator->()			const	{ return (T*)this->get(); }
			const T*	operator*()				const	{ return (T*)this->get(); }
			iteratorT&	operator--()					{ ++this->offsets; return *this; }
			iteratorT&	operator++()					{ --this->offsets; return *this; }
			iteratorT	operator+(int i)		const	{ return iteratorT(this->node, this->offsets - i); }
			iteratorT	operator-(int i)		const	{ return iteratorT(this->node, this->offsets + i); }
			int			operator-(const iteratorT &b) const	{ return b.offsets - this->offsets; }
		};
		struct container {
			const BTNodeDescriptor	*node;
			const uint16be			*offsets;
			int						numRecords;
			container(const BTNodeDescriptor *node, const uint16be *offsets, int numRecords) : node(node), offsets(offsets), numRecords(numRecords) {}
			iterator	begin()	const	{ return iterator(node, offsets - 1); }
			iterator	end()	const	{ return iterator(node, offsets - 1 - numRecords); }
		};
		template<typename T> struct containerT : container {
			typedef iteratorT<T>	iterator, const_iterator;
			typedef const T *element, *reference;
			using container::container;
			iterator	begin()	const	{ return (iterator&&)container::begin(); }
			iterator	end()	const	{ return (iterator&&)container::end(); }
			auto		front()	const	{ return *begin(); }
			auto		back()	const	{ return *--end(); }
		};

		uint16		treeDepth;
		uint32		rootNode;
		uint32		leafRecords;
		uint32		firstLeafNode;
		uint32		lastLeafNode;
		uint16		nodeSize;
		uint16		maxKeyLength;
		uint32		totalNodes;
		uint32		freeNodes;
		uint16		reserved1;
		uint32		clumpSize;      // misaligned
		uint8		btreeType;
		uint8		keyCompareType;
		uint32		attributes;     // long aligned again
		uint32		reserved3[16];

		const BTNodeDescriptor	*Node(int i)								const	{ return (BTNodeDescriptor*)((char*)this + i * nodeSize);	}
		const BTNodeDescriptor	*Root()										const	{ return Node(rootNode);	}
		const void				*Record(const BTNodeDescriptor *n, int i)	const	{ return (char*)n + int(((uint16*)((char*)n + nodeSize))[~i]);	}
		container				Records(const BTNodeDescriptor *node)		const	{ return container(node, (const uint16*)((char*)node + nodeSize), node->numRecords); }
	};

	typedef _pascal_string<uint16be, char16be> String;

	struct point : bigendian_types {
		int16	v;
		int16	h;
	};

	struct rect : bigendian_types {
		int16	top;
		int16	left;
		int16	bottom;
		int16	right;
	};

	typedef uint32be		FourCharCode;
	typedef FourCharCode	OSType;

	struct FileInfo {
		enum {
			kHardLinkFileType = 0x686C6E6B,	// hlnk
			kHFSPlusCreator   = 0x6866732B,	// hfs+
			kSymLinkFileType  = 0x736C6E6B,	// slnk
			kSymLinkCreator   = 0x72686170,	// rhap
		};
		OSType	fileType;           // The type of the file
		OSType	fileCreator;        // The file's creator
		uint16	finderFlags;
		point	location;           // File's location in the folder.
		uint16	reservedField;
		bool	IsSymLink()		const { return fileType == kSymLinkFileType && fileCreator == kSymLinkCreator; }
		bool	IsHardLink()	const { return fileType == kHardLinkFileType && fileCreator == kHFSPlusCreator; }
	};

	struct ExtendedFileInfo {
		int16	reserved1[4];
		uint16	extendedFinderFlags;
		int16	reserved2;
		int32	putAwayFolderID;
	};

	struct FolderInfo {
		rect	windowBounds;       // The position and dimension of the folder's window
		uint16	finderFlags;
		point	location;			// Folder's location in the parent folder. If set to {0, 0}, the Finder will place the item automatically
		uint16	reservedField;
	};

	struct ExtendedFolderInfo {
		point	scrollPosition;	// Scroll position (for icon views)
		int32	reserved1;
		uint16	extendedFinderFlags;
		int16	reserved2;
		int32	putAwayFolderID;
	};

	struct CatalogKey : bigendian_types {
		uint16				keyLength;
		uint32				parentID;
		String				nodeName;
		const void			*Data()	const {
			return (const char*)this + int(keyLength) + 2;
		}
		template<typename T> const T* as() const {
			return (const T*)Data();
		}
		char				*GetName(char *name) const {
			int	n = nodeName.len;
			for (int i = 0; i < n; i++)
				name[i] = nodeName.buffer[i] < ' ' ? '?' : get(nodeName.buffer[i]);
			name[n] = 0;
			return name;
		}
	};
	enum {
		kFolderRecord		= 1,
		kFileRecord			= 2,
		kFolderThreadRecord	= 3,
		kFileThreadRecord	= 4
	};

	struct CatalogFolder : bigendian_types {
		int16				recordType;
		uint16				flags;
		uint32				valence;
		uint32				folderID;
		uint32				createDate;
		uint32				contentModDate;
		uint32				attributeModDate;
		uint32				accessDate;
		uint32				backupDate;
		BSDInfo				permissions;
		FolderInfo			userInfo;
		ExtendedFolderInfo	finderInfo;
		uint32				textEncoding;
		uint32				reserved;
	};

	struct CatalogFile : bigendian_types {
		enum {
			kFileLockedBit		= 0,
			kFileLockedMask		= 1,
			kThreadExistsBit	= 1,
			kThreadExistsMask	= 2
		};

		int16				recordType;
		uint16				flags;
		uint32				reserved1;
		uint32				fileID;
		uint32				createDate;
		uint32				contentModDate;
		uint32				attributeModDate;
		uint32				accessDate;
		uint32				backupDate;
		BSDInfo				permissions;
		FileInfo			userInfo;
		ExtendedFileInfo	finderInfo;
		uint32				textEncoding;
		uint32				reserved2;

		ForkData		dataFork;
		ForkData		resourceFork;
	};

	struct CatalogThread : bigendian_types {
		int16		recordType;
		int16		reserved;
		uint32		parentID;
		String		nodeName;
	};


	struct BTree : BTNodeDescriptor, BTHeaderRec {
		char				user[128];
		char				map[1];
		const BTNodeDescriptor	*Node(int i)	const	{ return (BTNodeDescriptor*)((char*)this + i * nodeSize);	}
		const BTNodeDescriptor	*Root()			const	{ return Node(rootNode);	}
		void					*User()					{ return user;				}
		void					*Map()					{ return map;				}
	};

	struct BTCatalog : BTree {
		typedef BTHeaderRec::containerT<CatalogKey>	container;
		typedef container::iterator iterator;
		container			Records(const BTNodeDescriptor *node)	const	{ return container(node, (const uint16*)((char*)node + nodeSize), node->numRecords); }
		iterator			Find(uint32le id, iterator &end)		const;
		const CatalogKey*	FindEntry(uint32le nid, const count_string &name)	const;
	};
};

#pragma pack()

HFSPlus::BTCatalog::iterator HFSPlus::BTCatalog::Find(uint32le nid, HFSPlus::BTCatalog::iterator &end) const {
	for (const HFSPlus::BTNodeDescriptor *node = Root(); node;) {
		auto	records = Records(node);

		if (node->fLink && nid > records.back()->parentID) {
			auto next = Node(node->fLink);
			if (nid >= Records(next).front()->parentID) {
				node = Node(node->fLink);
				continue;
			}
		}

		auto	i	= first_not(records, [nid](const HFSPlus::CatalogKey *key) {
			return key->parentID < nid;
		});

		if (i == records.end() || i->parentID > nid)
			--i;

		switch (node->kind) {
			case HFSPlus::BTNodeDescriptor::kBTIndexNode:
				node = Node(i->as<uint32be>()[0]);
				break;

			case HFSPlus::BTNodeDescriptor::kBTHeaderNode:
				node = i->as<HFSPlus::BTHeaderRec>()->Root();
				break;

			case HFSPlus::BTNodeDescriptor::kBTLeafNode:
				end	= records.end();
				return i;

			default:
				return nullptr;
		}
	}
	return nullptr;
}

const HFSPlus::CatalogKey *HFSPlus::BTCatalog::FindEntry(uint32le nid, const count_string &name) const {
	HFSPlus::BTCatalog::iterator	end;
	HFSPlus::BTCatalog::iterator	i = Find(nid, end);
	if (*i) {
		while (i->parentID == nid) {
			if (str(i->nodeName) == name)
				return *i;

			if (++i == end) {
				auto	recs = Records(Node(i.node->fLink));
				i	= recs.begin();
				end	= recs.end();
			}
		}
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
//	UDIF - Universal Disk Format
//-----------------------------------------------------------------------------

struct UDIF {
	enum BlockType {
		BT_ZERO		= 0x00000000,
		BT_RAW		= 0x00000001,
		BT_IGNORE	= 0x00000002,
		BT_COMMENT	= 0x7ffffffe,
		BT_ADC		= 0x80000004,
		BT_ZLIB		= 0x80000005,
		BT_BZLIB	= 0x80000006,
		BT_END		= 0xffffffff,
	};

#pragma pack(1)
	struct Checksum : bigendian_types {
		enum {NONE = 0, CRC32 = 2};
		uint32	type;
		uint32	bits;
		uint32	data[32];
	};

	struct ID : bigendian_types {
		uint32	data1;
		uint32	data2;
		uint32	data3;
		uint32	data4;
	};

	struct ResourceFile : bigendian_types {
		enum FLAGS {
			Flattened	= 1
		};
		enum IMAGE {
			Device		= 1,
			Partition	= 2,
		};
		uint32	Signature;
		uint32	Version;
		uint32	HeaderSize;
		uint32	Flags;

		uint64	RunningDataForkOffset;
		uint64	DataForkOffset;
		uint64	DataForkLength;
		uint64	RsrcForkOffset;
		uint64	RsrcForkLength;

		uint32	SegmentNumber;
		uint32	SegmentCount;
		ID		SegmentID;

		Checksum	DataForkChecksum;

		uint64	XMLOffset;
		uint64	XMLLength;
		char	Reserved1[120];

		Checksum	MasterChecksum;

		uint32	ImageVariant;
		uint64	SectorCount;
		char	Reserved2[12];

		bool	verify() const {
			return Signature == 'koly';
		}
	};

	struct Block : bigendian_types {
		uint32	Type;
		uint32	Comment;			// "+beg" or "+end", if EntryType is comment (0x7FFFFFFE) else reserved.
		uint64	SectorNumber;		// Start sector of this chunk
		uint64	SectorCount;		// Number of sectors in this chunk
		uint64	CompressedOffset;	// Start of chunk in data fork
		uint64	CompressedLength;	// Count of bytes of chunk, in data fork

		iso::uint64	SectorEnd()			const { return SectorNumber + SectorCount; }
		iso::uint64	CompressedEnd()		const { return CompressedOffset + CompressedLength; }
		size_t		Read(istream_ref file, void *dest, malloc_block &temp)	const;

		void		InitRaw(iso::uint64 start_sector, iso::uint64 num_sectors) {
			Type				= UDIF::BT_RAW;
			Comment				= 0;
			SectorNumber		= start_sector;
			SectorCount			= num_sectors;
			CompressedOffset	= start_sector * 0x200;
			CompressedLength	= num_sectors * 0x200;
		}
	};

	struct BlockTable : bigendian_types {
		uint32		Signature;	//'mish'
		uint32		Version;
		uint64		FirstSectorNumber;
		uint64		SectorCount;

		uint64		DataStart;
		uint32		BuffersNeeded;
		uint32		BlocksDescriptor;

		char		Reserved1[24];
		Checksum	checksum;
		uint32		BlocksRunCount;
		Block		blocks[1];

		static size_t	CalcSize(iso::uint32 BlocksRunCount) { return size_t(&((UDIF::BlockTable*)0)->blocks[BlocksRunCount]); }
		size_t			CalcSize() const { return CalcSize(BlocksRunCount); }
		iso::uint64		FindEndSector() const {
			iso::uint64		end_sector		= 0;
			for (const Block *b = blocks, *e = b + BlocksRunCount; b != e; b++)
				end_sector	= max(end_sector, b->SectorNumber + b->SectorCount);
			return FirstSectorNumber + end_sector;
		}
		iso::uint64	CompressedEnd()		const {
			return blocks[BlocksRunCount - 1].CompressedEnd();
		}
		const Block*	FindBlock(iso::uint64 sector) const {
			return lower_bound(blocks, blocks + BlocksRunCount, sector, [](const Block &b, iso::uint64 sector) {
				return b.SectorEnd() <= sector;
			});
		}
	};

	struct DriverDescriptor : bigendian_types {
		uint32	block;
		uint16	size;
		uint16	type;
	};

	struct DriverDescriptorTable : bigendian_types {
		uint16	Signature;
		uint16	BlkSize;
		uint32	BlkCount;
		uint16	DevType;
		uint16	DevId;
		uint32	Data;
		uint16	DrvrCount;
		DriverDescriptor dds[1];
	};
#pragma pack()

	struct BlockTableEntry {
		ISO_ptr<void>	p;
		uint32			attributes;
		int				id;
		string			name;
		BlockTable		*block_table;
		BlockTableEntry(const BlockTableEntry&) = default;
		BlockTableEntry(const ISO::Browser2 &b);
		BlockTableEntry(uint32 attributes, int id, const char *name, BlockTable *data) : attributes(attributes), id(id), name(name), block_table(data) {}
		ISO_ptr<void> ToISO();
	};

	dynamic_array<BlockTableEntry>	block_tables;

	UDIF()				{}
	UDIF(const UDIF &b) : block_tables(b.block_tables) {}

	UDIF(istream_ref file) {
		ResourceFile	koly;
		file.seek_end(-int(sizeof(koly)));
		file.read(koly);
		if (koly.verify()) {
			file.seek(koly.XMLOffset);
			istream_offset	offset(file, koly.XMLLength);
			XPLISTreader	reader(offset);
			if (reader.valid()) {
				for (auto i : ISO::Browser2(reader.get_item(0))["resource-fork"]["blkx"])
					new (block_tables) BlockTableEntry(i);
			}
		}
	}

	uint64	FindEndSector() const {
		uint64	end_sector = 0;
		for (auto &i : block_tables)
			end_sector	= max(end_sector, i.block_table->FindEndSector());
		return end_sector;
	}

	uint32	CalculateMasterChecksum() const {
		uint32		crc	= 0;
		for (auto &i : block_tables) {
			if (i.block_table->checksum.type == Checksum::CRC32)
				crc = CRC32::calc(i.block_table->checksum.data, 4, crc);
		}
		return crc;
	}

	void	AddBlockTable(const char *name, uint32 id, uint32 attributes, UDIF::Block *blocks, int num_blocks);
	void	WriteDescriptors(ostream_ref file);
};

class UDIFPartStream : UDIF::BlockTableEntry, public stream_defaults<UDIFPartStream> {
	istream_ptr		file;
	streamptr		end, ptr;
	streamptr		block_offset, block_size;
	uint32			block_type;
	malloc_block	data, temp;

public:
	UDIFPartStream(istream_ref file, const UDIF::BlockTableEntry &e) : UDIF::BlockTableEntry(e), file(file.clone()), ptr(0), block_offset(0), block_size(0) {
		end	= (block_table->FindEndSector() - block_table->FirstSectorNumber) * 0x200;
	}
	UDIFPartStream(const UDIFPartStream &b) : UDIF::BlockTableEntry(b), file(b.file.clone()), ptr(0), end(b.end), block_offset(0), block_size(b.block_size) {}
	size_t		readbuff(void *buffer, size_t size);
	void		seek(streamptr offset)	{ ptr = offset;}
	streamptr	tell()					{ return ptr; }
	streamptr	length()				{ return end; }
	int			getc()					{ uint8 c; return readbuff(&c, 1) ? c : EOF; }
};

//-----------------------------------------------------------------------------
//	iOS images
//-----------------------------------------------------------------------------

struct IMG2 {
	enum {MAGIC = 'IMG2'};
	uint32	magic;
	uint8	creator[4];
	uint16	headerLength;
	uint16	version;
	uint32	format;
	uint32	flags;
	uint32	numOfBlocks;
	uint32	startOffset;
	uint32	fileLength;
	uint32	commentOffset;
	uint32	commentLength;
	uint32	creatorOffset;
	uint32	creatorLength;
	uint8	padding[16];
};

struct IMG3 {
	enum {MAGIC = 'Img3'};

	struct TAG {
		enum {
			VERS	= 'VERS',	// iBoot version of the image
			SEPO	= 'SEPO',	// Security Epoch
			SDOM	= 'SDOM',	// Security Domain
			PROD	= 'PROD',	// Production Mode
			CHIP	= 'CHIP',	// Chip to be used with. example: "0x8900" for S5L8900.
			BORD	= 'BORD',	// Board to be used with
			KBAG	= 'KBAG',	// contains the KEY and IV required to decrypt encrypted with the GID-key
			SHSH	= 'SHSH',	// RSA encrypted SHA1 hash of the file
			CERT	= 'CERT',	// Certificate
			ECID	= 'ECID',	// Exclusive Chip ID unique to every device with iPhone OS.
			TYPE	= 'TYPE',	// Type of image, should contain the same string as 'iden' of the header
			DATA	= 'DATA',	// Real content of the file
		};
		uint32	magic;
		uint32	total_length;
		uint32	data_length;
	};

	uint32	magic;
	uint32	fullSize;		// full size of fw image
	uint32	sizeNoPack;		// size of fw image without header
	uint32	sigCheckArea;	// although that is just my name for it, this is the size of the start of the data section (the code) up to the start of the RSA signature (SHSH section)
	uint8	iden;			// identifier of image, used when bootrom is parsing images list to find LLB (illb), LLB parsing it to find iBoot (ibot), etc.
	TAG		tags[];
};

//Signature Check: Decryption is done using the modulus at cert + 0xA15; 0xC to SHSH is SHAed

//-----------------------------------------------------------------------------
//	ApplePartitionMap
//-----------------------------------------------------------------------------

struct ApplePartitionMap : bigendian_types {
	enum {
		SIGNATURE	= 'PM',

		VALID		= 0x00000001,	// Entry is valid (A/UX only)
		ALLOCATED	= 0x00000002,	// Entry is allocated (A/UX only)
		INUSE		= 0x00000004,	// Entry in use (A/UX only)
		HASBOOT		= 0x00000008,	// Entry contains boot information (A/UX only)
		READABLE	= 0x00000010,	// Partition is readable (A/UX only)
		WRITABLE	= 0x00000020,	// Partition is writable (Macintosh & A/UX)
		POSINDEP	= 0x00000040,	// Boot code is position independent (A/UX only)
		CHAIN_COMP	= 0x00000100,	// Partition contains chain-compatible driver (Macintosh only)
		REAL_DRVR	= 0x00000200,	// Partition contains a real driver (Macintosh only)
		CHAIN_DRVR	= 0x00000400,	// Partition contains a chain driver (Macintosh only)
		AUTOMOUNT	= 0x40000000,	// Automatically mount at startup (Macintosh only)
		SETUP		= 0x80000000,	// The startup partition (Macintosh only)
	};

	uint16	signature;		//(0x504D)
	uint16	reserved0;
	uint32	num_partitions;	//4–7 Total Number of partitions Yes
	uint32	start_sector;	//8–11 Starting sector of partition Yes
	uint32	num_sectors;	//12–15 Size of partition in sectors Yes
	char	name[32];		//16–47 Name of partition in ASCII No
	char	type[32];		//48–79 Type of partition in ASCII No
	uint32	start_data;		//80–83 Starting sector of data area in partition No
	uint32	size_data;		//84–87 Size of data area in sectors No
	uint32	status;			//88–91 Status of partition (see table 5-8) No
	uint32	boot_code;		//92–95 Starting sector of boot code No
	uint32	boot_size;		//96–99 Size of boot code in sectors No
	uint32	boot_addr;		//100–103 Address of boot loader code No
	uint32	reserved1;		//104–107 Reserved No
	uint32	boot_entry;		//108–111 Boot code entry point No
	uint32	reserved2;		//112–115 Reserved No
	uint32	boot_cksum;		//116–119 Boot code checksum No
	char	processor[16];	//120–135 Processor type No
	char	pad[512-136];	//136–511 Reserved No
};


//-----------------------------------------------------------------------------
//	HFS+
//-----------------------------------------------------------------------------

struct HFS;

struct HFS_file : ISO::VirtualDefaults {
	ISO_ptr<HFS>				hfs;
	const HFSPlus::CatalogFile	*cat;

	HFS_file(const HFS *_hfs) : hfs(ISO_ptr<HFS>::Ptr(unconst(_hfs))), cat(0) {}
	ISO::Browser2	Deref();
};

struct HFS_folder : ISO::VirtualDefaults {
	ISO_ptr<HFS>				hfs;
	uint32						nid;
	anything					entries;

	HFS_folder(const HFS *hfs, uint32 nid) : hfs(ISO_ptr<HFS>::Ptr(unconst(hfs))), nid(nid) {}
	ISO::Browser2	Deref();
};

struct HFS : HFS_folder {
	istream_ptr				file;
	HFSPlus::VolumeHeader	vol;
	uint32					blocksize;
	HFSPlus::BTCatalog		*catalog;

	malloc_block ReadFork(const HFSPlus::ForkData &fork) {
		malloc_block	mb(uint32(fork.logicalSize));
		fork.read(file, blocksize, mb, fork.logicalSize);
		return mb;
	}
	anything ReadFolder(uint32 nid);

	void	Init() {
		file.seek(0x400);
		vol = file.get();
		if (vol.valid()) {
			blocksize	= vol.blockSize;
			catalog		= ReadFork(vol.catalogFile).detach();
		}
	}
#ifdef PLAT_PC
	HFS(const filename &fn)	: HFS_folder(this, HFSPlus::kRootParentID), file(new WinFileInput(CreateFileA(fn, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0))) {
		Init();
	}
#endif
	HFS(istream_ref file) : HFS_folder(this, HFSPlus::kRootParentID), file(file.clone()) {
		Init();
	}
	~HFS() {
		delete catalog;
	}
};

ISO::Browser2 HFS_file::Deref()	{
	auto	cat = this->cat;

	if (cat->userInfo.IsHardLink()) {
		if (auto meta = hfs->catalog->FindEntry(HFSPlus::kRootFolderID, "\0\0\0\0HFS+ Private Data")) {
			if (auto inode = hfs->catalog->FindEntry(meta->as<HFSPlus::CatalogFolder>()->folderID, count_string(format_string("iNode%i", int(cat->permissions.special)))))
				cat	= inode->as<HFSPlus::CatalogFile>();
		}

	}
	return ISO::MakePtr(0, hfs->ReadFork(cat->dataFork));
}

anything HFS::ReadFolder(uint32 nid) {
	anything					entries;

	HFSPlus::BTCatalog::iterator	end;
	HFSPlus::BTCatalog::iterator	i = catalog->Find(nid, end);
	if (!*i)
		return entries;

	char			name[256];
	while (i->parentID == nid) {
		switch (*i->as<uint16be>()) {
			case HFSPlus::kFolderRecord: {
				ISO_ptr<HFS_folder>		folder(i->GetName(name), this, i->as<HFSPlus::CatalogFolder>()->folderID);
				entries.Append(folder);
				break;
			}
			case HFSPlus::kFileRecord: {
				ISO_ptr<HFS_file>		file(i->GetName(name), this);
				file->cat	= i->as<HFSPlus::CatalogFile>();
				entries.Append(file);
				break;
			}
			case HFSPlus::kFolderThreadRecord:	break;
			case HFSPlus::kFileThreadRecord:	break;
		}
		if (++i == end) {
			auto	recs = catalog->Records(catalog->Node(i.node->fLink));
			i	= recs.begin();
			end	= recs.end();
		}
	}
	return entries;
}

ISO::Browser2 HFS_folder::Deref()	{
	if (!entries && nid)
		entries = hfs->ReadFolder(nid);
	return ISO::MakeBrowser(entries);
}

ISO_DEFVIRT(HFS);
ISO_DEFUSERVIRTXF(HFS_file, "file", ISO::TypeUserSave::DONTKEEP);
ISO_DEFUSERVIRTXF(HFS_folder, "folder", ISO::TypeUserSave::DONTKEEP);

class HFSFileHandler : public FileHandler {
	const char*		GetExt() override { return "hfs";	}
	const char*		GetDescription() override { return "Mac HFS+ Disk image"; }
	int				Check(istream_ref file) override {
		if (file.length() < 0x800)
			return CHECK_DEFINITE_NO;
		file.seek(0x400);
		return file.get<HFSPlus::VolumeHeader>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return ISO_ptr<HFS>(id, file); }
#ifdef PLAT_PC
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override { return ISO_ptr<HFS>(id, fn); }
#endif
} hfs;

//-----------------------------------------------------------------------------
//	DMG
//-----------------------------------------------------------------------------

void UDIF::AddBlockTable(const char *name, uint32 id, uint32 attributes, UDIF::Block *blocks, int num_blocks) {
	malloc_block	data(BlockTable::CalcSize(num_blocks + 1));
	BlockTable		*block_table	= data;
	clear(*block_table);
	block_table->Signature			= 'mish';
	block_table->Version			= 1;
	block_table->FirstSectorNumber	= blocks[0].SectorNumber;
	block_table->SectorCount		= blocks[num_blocks - 1].SectorNumber + blocks[num_blocks - 1].SectorCount - block_table->FirstSectorNumber;
//	block_table->DataStart			= blocks[0].SectorNumber * 0x200;
	block_table->BlocksDescriptor	= block_tables.size32();
	block_table->BlocksRunCount		= num_blocks + 1;

	Block	*blocks2	= block_table->blocks;
	uint32	max_sectors	= 0;
	for (int i = 0; i < num_blocks; i++) {
		*blocks2				= *blocks;
		max_sectors				= max(max_sectors, blocks2->SectorCount);
		blocks2->SectorNumber	-= block_table->FirstSectorNumber;
		blocks2++;
		blocks++;
	}

	block_table->BuffersNeeded	= max_sectors + 8;

	blocks2->Type				= UDIF::BT_END;
	blocks2->Comment			= 0;
	blocks2->SectorNumber		= blocks2[-1].SectorEnd();
	blocks2->SectorCount		= 0;
	blocks2->CompressedOffset	= blocks2[-1].CompressedEnd();
	blocks2->CompressedLength	= 0;

	new(block_tables) BlockTableEntry(attributes, id, name, block_table);
}

void UDIF::WriteDescriptors(ostream_ref file) {
	ISO_ptr<anything>	plist(0);
	ISO_ptr<anything>	rf("resource-fork");
	ISO_ptr<anything>	blkx("blkx");
	plist->Append(rf);
	rf->Append(blkx);

	for (auto &i : block_tables)
		blkx->Append(i.ToISO());

	uint64		end		= block_tables.back().block_table->CompressedEnd();

	streamptr	xml		= file.tell();
	XPLISTwriter(file).put_item(ISO::Browser(plist));
	streamptr	xml_end = file.tell();

	ResourceFile	koly;
	clear(koly);

	koly.Signature				= 'koly';
	koly.Version				= 4;
	koly.HeaderSize				= uint32(sizeof(koly));
	koly.Flags					= ResourceFile::Flattened;
	koly.DataForkLength			= end;

	koly.SegmentNumber			= 1;
	koly.SegmentCount			= 1;

	koly.XMLOffset				= xml;
	koly.XMLLength				= xml_end - xml;
//	koly.MasterChecksum.bits	= 32;
//	koly.MasterChecksum.type	= Checksum::CRC32;
//	koly.MasterChecksum.data[0]	= CalculateMasterChecksum();
	koly.ImageVariant			= ResourceFile::Device;
	koly.SectorCount			= FindEndSector();

	file.write(koly);
}

UDIF::BlockTableEntry::BlockTableEntry(const ISO::Browser2 &b) : p(b) {
	attributes	= b["Attributes"].GetInt();
	id			= b["ID"].GetInt();
	name		= b["Name"].GetString();
	block_table	= b["Data"][0];
}

ISO_ptr<void> UDIF::BlockTableEntry::ToISO() {
	uint32	size	= uint32(block_table->CalcSize());
	ISO_ptr<ISO_openarray<uint8> > d("Data", size);
	memcpy(*d, block_table, size);

	ISO_ptr<anything>	p(0);
	p->Append(ISO_ptr<string>("Attributes", format_string("0x%x", attributes)));
	p->Append(ISO_ptr<string>("CFName", name));
	p->Append(d);
	p->Append(ISO_ptr<string>("ID", to_string(id)));
	p->Append(ISO_ptr<string>("Name", name));

	return p;
}

size_t UDIF::Block::Read(istream_ref file, void *dest, malloc_block &temp) const {
	streamptr	in_offs		= CompressedOffset;
	size_t		in_size		= CompressedLength;
	size_t		out_size	= SectorCount * 0x200;

	switch (Type) {
		case BT_ZERO:
			memset(dest, 0, out_size);
			return out_size;

		case BT_RAW:
			file.seek(in_offs);
			file.readbuff(dest,	in_size);
			return in_size;

		case BT_ADC:
			file.seek(in_offs);
			file.readbuff(temp.create(in_size), in_size);
			transcode(ADC::decoder(), memory_block(dest, out_size), temp, &out_size);
			return out_size;

		case BT_ZLIB:
			file.seek(in_offs);
			return zlib_reader(file).readbuff(dest, out_size);

		case BT_BZLIB: {
			bz_stream	z;
			clear(z);
			BZ2_bzDecompressInit(&z, 0, 0);
			file.seek(in_offs);
			file.readbuff(temp.create(in_size), in_size);
			z.next_in	= temp;
			z.next_out	= (char*)dest;
			z.avail_out = unsigned(out_size);
			do {
				z.avail_in	= 32768;
			} while (BZ2_bzDecompress(&z) != BZ_STREAM_END);
			BZ2_bzDecompressEnd(&z);
			return (uint64(z.total_out_hi32) << 32) | z.total_out_lo32;
		}
	}
	return 0;
}

#if 0
class DMGstream : UDIF, public istream_defaults<DMGstream> {
	istream_ref			file;
	streamptr		ptr, end;
	streamptr		block_offset, block_size;
	uint32			block_type;
	malloc_block	data, temp;

public:
	DMGstream(istream_ref _file) : UDIF(_file), file(_file), ptr(0), block_offset(0), block_size(0) {
		end = FindEndSector() * 0x200;
	}
	DMGstream(const DMGstream &b) : UDIF(b), file(*b.file.clone()), ptr(0), end(b.end), block_offset(0), block_size(b.block_size) {}
	size_t		readbuff(void *buffer, size_t size);
	void		seek(streamptr offset)	{ ptr = offset; }
	streamptr	tell()					{ return ptr; }
	streamptr	length()				{ return end; }
};

size_t DMGstream::readbuff(void *buffer, size_t size) {
	size_t	done = 0;
	while (size) {
		if (ptr < block_offset || ptr >= block_offset + block_size) {
			uint32	sec		= ptr / 0x200;
			Block	*block	= 0;
			for (auto &i : block_tables) {
				BlockTable	*block_table	= i.block_table;
				streamptr	part_offset		= block_table->FirstSectorNumber;
				if (sec < part_offset)
					break;
				if (sec < part_offset + block_table->SectorCount) {
					for (Block *b = block_table->blocks, *e = b + block_table->BlocksRunCount; b != e; b++) {
						if (b->Type == BT_END)
							break;

						if (sec < part_offset + b->SectorNumber)
							return int(done);

						if (sec < part_offset + b->SectorNumber + b->SectorCount) {
							block			= b;
							block_type		= block->Type;
							block_offset	= (part_offset + block->SectorNumber) * 0x200;
							block_size		= block->SectorCount * 0x200;
							break;
						}
					}
				}
			}

			if (!block)
				return int(done);

			if (block_type != BT_ZERO)
				block->Read(file, data.create(uint32(block_size)), temp);
		}

		size_t	c = min(size_t(block_offset + block_size - ptr), size);
		if (block_type == BT_ZERO)
			memset(buffer, 0, c);
		else
			memcpy(buffer, (char*)data + ptr - block_offset, c);

		size	-= c;
		done	+= c;
		ptr		+= c;
		buffer	= (char*)buffer + c;
	}
	return int(done);
}
#endif

size_t UDIFPartStream::readbuff(void *buffer, size_t size) {
	size_t	done = 0;
	while (size) {
		if (ptr < block_offset || ptr >= block_offset + block_size) {
			uint32				sec		= ptr / 0x200;
			const UDIF::Block	*block	= block_table->FindBlock(sec);
			if (sec < block->SectorNumber || block->Type == UDIF::BT_END)
				return int(done);
			block_type		= block->Type;
			block_offset	= block->SectorNumber	* 0x200;
			block_size		= block->SectorCount	* 0x200;

			if (block_type != UDIF::BT_ZERO && block_type != UDIF::BT_IGNORE)
				block->Read(file, data.create(uint32(block_size)), temp);
		}

		size_t	c = min(size_t(block_offset + block_size - ptr), size);
		if (block_type == UDIF::BT_ZERO || block_type == UDIF::BT_IGNORE)
			memset(buffer, 0, c);
		else
			memcpy(buffer, (char*)data + ptr - block_offset, c);

		size	-= c;
		done	+= c;
		ptr		+= c;
		buffer	= (char*)buffer + c;
	}
	return int(done);
}

void InitBlocks(UDIF::Block *blocks, uint64 start_sector, uint64 num_sectors, uint32 max_sectors) {
	while (num_sectors > max_sectors) {
		blocks->InitRaw(start_sector, max_sectors);
		start_sector	+= max_sectors;
		num_sectors		-= max_sectors;
		blocks++;
	}
	blocks->InitRaw(start_sector, num_sectors);
};

#ifdef PLAT_PC
void SectorsToDMG(HANDLE h) {
	static const uint32 max_sectors = 0x800;

	UDIF		udif;
	uint8		sector[512];
	DWORD		size;
	ReadFile(h, sector, sizeof(sector), &size, NULL);

	MBR			&mbr	= *(MBR*)sector;
	if (mbr.part[0].type != MBR::GPT_PROTECT) {
		for (int i = 0; i < 4; i++) {
			MBR::Partition &p	= mbr.part[i];
			if (p.LBA_length) {
				UDIF::Block		block;
				block.InitRaw(p.LBA_start, p.LBA_length);
				udif.AddBlockTable(0, i - 1, 0x50, &block, 1);
			}
		}

	} else {
		UDIF::Block		block;
		block.InitRaw(0, 1);
		udif.AddBlockTable("Protective Master Boot Record", -1, 0x50, &block, 1);

		ReadFile(h, sector, sizeof(sector), &size, NULL);
		PartitionTableHeader	pth = *(PartitionTableHeader*)sector;
		block.InitRaw(1, 1);
		udif.AddBlockTable("GPT Header", 0, 0x50, &block, 1);

		LARGE_INTEGER	seek;
		seek.QuadPart = pth.EntriesLBA * 512;
		SetFilePointerEx(h, seek, &seek, SEEK_SET);

		int				n = pth.NumberPartitions;
		malloc_block	table(sizeof(PartitionEntry) * n);
		ReadFile(h, table, sizeof(PartitionEntry) * n, &size, NULL);
		block.InitRaw(pth.EntriesLBA, div_round_up(sizeof(PartitionEntry) * n, 0x200));
		udif.AddBlockTable("GPT Partition Data", 1, 0x50, &block, 1);

		PartitionEntry	*p = table;
		for (int i = 0; i < n; i++, p++) {
			if (p->Type.Data1 != 0) {
				uint64			num_sectors	= p->LastLBA - p->FirstLBA + 1;
				uint32			num_blocks	= uint32(div_round_up(num_sectors, max_sectors));
				UDIF::Block*	blocks		= new UDIF::Block[num_blocks];
				InitBlocks(blocks, p->FirstLBA, num_sectors, max_sectors);
				udif.AddBlockTable(str8(p->Name), i + 2, 0x50, blocks, num_blocks);
				delete[] blocks;
			}
		}
	}

	WinFileOutput	file(h);
	file.seek(udif.FindEndSector() * 0x200);
	udif.WriteDescriptors(file);
	SetEndOfFile(h);
}

struct MakeDMG {
	MakeDMG() {
		SectorsToDMG(CreateFileA("Q:\\machd.dmg", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
	}
};// make_dmg;
#endif

//-----------------------------------------------------------------------------
//	DMGFileHandler
//-----------------------------------------------------------------------------

class DMGFileHandler : public FileHandler {
	const char*		GetExt() override { return "dmg";	}
	const char*		GetDescription() override { return "Mac OSX Disk image"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		switch (file.get<uint32>()) {
			case IMG3::MAGIC: return CHECK_PROBABLE;
			case IMG2::MAGIC: return CHECK_PROBABLE;
			default: {
				if (file.length() > sizeof(UDIF::ResourceFile)) {
					UDIF::ResourceFile	koly;
					file.seek_end(-sizeof(UDIF::ResourceFile));
					if (file.read(koly) && koly.verify())
						return CHECK_PROBABLE;
				}
				return CHECK_NO_OPINION;
			}
		}
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		IMG3		img3(file.get());
		if (img3.magic == IMG3::MAGIC) {
			ISO_ptr<anything>	p(id);
			for (uint32 tell = sizeof(img3); tell < img3.fullSize;) {
				file.seek(tell);
				IMG3::TAG	tag	= file.get();
				char		id2[5];
				*(uint32be*)id2 = tag.magic;
				id2[4] = 0;
				p->Append(ReadRaw(id2, file, tag.data_length));
				tell += tag.total_length;
			}
			return p;
		}

		UDIF	udif(file);
		ISO_ptr<anything>	p(id);
		for (auto &i : udif.block_tables) {
			reader_mixout<UDIFPartStream>	s(file, i);
			p->Append(ISO_ptr<BigBin>(i.name, s));
		}
		return p;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return false;
	}
} dmg;
