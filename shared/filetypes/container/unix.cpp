#include "iso/iso_files.h"
#include "archive_help.h"
#include "base/bits.h"

using namespace iso;

#define	XFS_AGF_VERSION	1
#define XFS_BIG_BLKNOS	0

force_inline uint32 divup(uint32 a, uint32 b) { return (a + b - 1) / b; }

typedef bigendian0::uint16	u16;
typedef bigendian0::uint32	u32;
typedef bigendian0::uint64	u64;
typedef bigendian0::int16	s16;
typedef bigendian0::int32	s32;
typedef bigendian0::int64	s64;
typedef	uint8				u8;
typedef	int8				s8;

template<int bits> struct block_types;

template<> struct block_types<32> {
	typedef u32	u;
	typedef s32	s;
};

template<> struct block_types<64> {
	typedef u64	u;
	typedef s64	s;
};


enum {
	XFS_UQUOTA_ACCT = 0x0001,	// User quota accounting is enabled.
	XFS_UQUOTA_ENFD = 0x0002,	// User quotas are enforced.
	XFS_UQUOTA_CHKD = 0x0004,	// User quotas have been checked and updated on disk.
	XFS_PQUOTA_ACCT = 0x0008,	// Project quota accounting is enabled.
	XFS_OQUOTA_ENFD = 0x0010,	// Other (group/project) quotas are enforced.
	XFS_OQUOTA_CHKD = 0x0020,	// Other (group/project) quotas have been checked.
	XFS_GQUOTA_ACCT = 0x0040,	// Group quota accounting is enabled
};

enum {
	XFS_SB_VERSION2_LAZYSBCOUNTBIT	= 0x02,
	XFS_SB_VERSION2_ATTR2BIT		= 0x08,
	XFS_SB_VERSION2_PARENTBIT		= 0x10,
};

#pragma pack(1)

template<int bits> struct XFS_types {
	typedef u64		ino;		// Unsigned 64 bit absolute inode number.
	typedef s64		off;		// Signed 64 bit file offset.
	typedef s64		daddr;		// Signed 64 bit device address.
	typedef u32		agnumber;	// Unsigned 32 bit Allocation Group (AG) number.
	typedef u32		agblock;	// Unsigned 32 bit AG relative block number.
	typedef u32		extlen;		// Unsigned 32 bit extent length in blocks.
	typedef s32		extnum;		// Signed 32 bit number of extents in a file.
	typedef s16		aextnum;	// # extents in an attribute fork
	typedef	s64		fsize;		// bytes in a file
	typedef u64		ufsize;		// unsigned bytes in a file
	typedef u32		dablk;		// Unsigned 32 bit block number for directories and extended attributes.
	typedef u32		dahash;		// Unsigned 32 bit hash of a directory file name or extended attribute name.
	typedef u64		dfsbno;		// Unsigned 64 bit filesystem block number combining AG number and block offset into the AG.
	typedef u64		drfsbno;	// Unsigned 64 bit raw filesystem block number.
	typedef u64		drtbno;		// Unsigned 64 bit extent number in the real-time sub-volume.
	typedef u64		dfiloff;	// Unsigned 64 bit block offset into a file.
	typedef u64		dfilblks;	// Unsigned 64 bit block count for a file.

	typedef u64		fileoff;	// block number in a file */
	typedef s64		sfiloff;	// signed block number in a file */
	typedef u64		filblks;	// number of blocks in a file */

	typedef u8		arch;		// architecture of an xfs fs */
	struct uuid { u8 v[16]; };

	typedef typename block_types<bits>::u
		fsblock,	// blockno in filesystem (agno|agbno)
		rfsblock,	// blockno in filesystem (raw)
		rtblock;	// extent (block) in realtime area
	typedef typename block_types<bits>::s
		srtblock;	// signed version of rtblock */

	struct sb {
		u32			sb_magicnum;
		u32			sb_blocksize;
		drfsbno		sb_dblocks;
		drfsbno		sb_rblocks;
		drtbno		sb_rextents;
		uuid		sb_uuid;
		dfsbno		sb_logstart;
		ino			sb_rootino;
		ino			sb_rbmino;
		ino			sb_rsumino;
		agblock		sb_rextsize;
		agblock		sb_agblocks;
		agnumber	sb_agcount;
		extlen		sb_rbmblocks;
		extlen		sb_logblocks;
		u16			sb_versionnum;
		u16			sb_sectsize;
		u16			sb_inodesize;
		u16			sb_inopblock;
		char		sb_fname[12];
		u8			sb_blocklog;
		u8			sb_sectlog;
		u8			sb_inodelog;
		u8			sb_inopblog;
		u8			sb_agblklog;
		u8			sb_rextslog;
		u8			sb_inprogress;
		u8			sb_imax_pct;
		u64			sb_icount;
		u64			sb_ifree;
		u64			sb_fdblocks;
		u64			sb_frextents;
		ino			sb_uquotino;
		ino			sb_gquotino;
		u16			sb_qflags;
		u8			sb_flags;
		u8			sb_shared_vn;
		extlen		sb_inoalignmt;
		u32			sb_unit;
		u32			sb_width;
		u8			sb_dirblklog;
		u8			sb_logsectlog;
		u16			sb_logsectsize;
		u32			sb_logsunit;
		u32			sb_features2;
	};

	// Free Space


	struct agf {
		enum {
			BTNUM_AGF	= 2
		};
		u32			agf_magicnum;
		u32			agf_versionnum;
		u32			agf_seqno;
		u32			agf_length;
		u32			agf_roots[BTNUM_AGF];
		u32			agf_spare0;
		u32			agf_levels[BTNUM_AGF];
		u32			agf_spare1;
		u32			agf_flfirst;
		u32			agf_fllast;
		u32			agf_flcount;
		u32			agf_freeblks;
		u32			agf_longest;
		u32			agf_btreeblks;
	};

	struct btree_sblock {
		u32			bb_magic;
		u16			bb_level;
		u16			bb_numrecs;
		u32			bb_leftsib;
		u32			bb_rightsib;
	};

	struct alloc_rec {
		u32 ar_startblock;
		u32 ar_blockcount;
	};// alloc_rec_t, alloc_key_t;

	typedef u32 alloc_ptr_t;

	// INodes

	struct agi {
		u32			agi_magicnum;
		u32			agi_versionnum;
		u32			agi_seqno;
		u32			agi_length;
		u32			agi_count;
		u32			agi_root;
		u32			agi_level;
		u32			agi_freecount;
		u32			agi_newino;
		u32			agi_dirino;
		u32			agi_unlinked[64];
	};

	struct btree_sblock inobt_block_t;

	struct inobt_rec {
		u32			ir_startino;
		u32			ir_freecount;
		u64			ir_free;
	};

	struct inobt_key {
		u32 ir_startino;
	};

	typedef u32 inobt_ptr;


	// Data Extents

	enum exntst {
		EXT_NORM,
		EXT_UNWRITTEN,
		EXT_INVALID
	};

	struct bmbt_rec {
		u64	l0, l1;
	};

	struct bmbt_irec {
		fileoff		br_startoff;
		fsblock		br_startblock;
		filblks		br_blockcount;
		exntst		br_state;
		bmbt_irec(bmbt_rec &r);
	};

	struct bmdr_block {
		u16			bb_level;
		u16			bb_numrecs;
	};

	struct bmbt_key {
		dfiloff br_startoff;
	};// bmbt_key_t, bmdr_key_t;

	typedef dfsbno bmbt_ptr_t, bmdr_ptr_t;

	enum {
		BMAP_MAGIC	= 0x424d4150 /* 'BMAP' */
	};

	struct btree_lblock {
		u32			bb_magic;
		u16			bb_level;
		u16			bb_numrecs;
		u64			bb_leftsib;
		u64			bb_rightsib;
	};

	// Directories

	typedef u16 dir2_data_off;
	typedef u32 dir2_dataptr;

	struct dir2_sf_off	{ u8 i[2]; };

	union  dir2_inou {
		u64			i8;
		u32			i4;
	};

	struct dir2_sf_hdr {
		u8				count;
		u8				i8count;
		dir2_inou		parent;
	};

	struct dir2_sf_entry {
		u8				namelen;
		dir2_sf_off		offset;
		u8				name[1];
		dir2_inou		inumber;
	};

	struct dir2_sf {
		dir2_sf_hdr		hdr;
		dir2_sf_entry	list[1];
	};



	// Block Directories

	enum {
		DIR2_DATA_FD_COUNT	= 3,
		DIR2_DATA_ALIGN		= 8
	};

	struct dir2_data_free {
		dir2_data_off		offset;
		dir2_data_off		length;
	};
	struct dir2_data_hdr {
		u32					magic;
		dir2_data_free	bestfree[DIR2_DATA_FD_COUNT];
	};

	struct dir2_data_entry {
		ino				inumber;
		u8				namelen;
		u8				name[1];
		dir2_data_off	tag;
	};

	struct dir2_data_unused {
		u16				freetag;			/* 0xffff */
		dir2_data_off	length;
		dir2_data_off	tag;
	};

	union dir2_data_union {
		dir2_data_entry		entry;
		dir2_data_unused	unused;
	};

	struct dir2_leaf_entry {
		dahash			hashval;
		dir2_dataptr	address;
	};

	struct dir2_block_tail {
		u32			count;
		u32			stale;
	};

	struct dir2_block {
		dir2_data_hdr		hdr;
		dir2_data_union		u[1];
		dir2_leaf_entry		leaf[1];
		dir2_block_tail		tail;
	};


	// Leaf Directories

	static const uint32	DIR2_LEAF_OFFSET = 1u<<31;

	struct dir2_data {
		dir2_data_hdr		hdr;
		dir2_data_union		u[1];
	};

	struct da_blkinfo {
		u32			forw;
		u32			back;
		u16			magic;
		u16			pad;
	};

	struct dir2_leaf_hdr {
		da_blkinfo	info;
		u16			count;
		u16			stale;
	};

	struct dir2_leaf_tail {
		u32			bestcount;
	};

	struct dir2_leaf {
		dir2_leaf_hdr		hdr;
		dir2_leaf_entry		ents[1];
		dir2_data_off		bests[1];
		dir2_leaf_tail		tail;
	};

	// Node Directories
	struct dir2_free_hdr {
		u32			magic;
		s32			firstdb;
		s32			nvalid;
		s32			nused;
	};

	struct dir2_free {
		dir2_free_hdr	hdr;
		dir2_data_off	bests[1];
	};

	struct da_intnode {
		struct da_node_hdr {
			da_blkinfo	info;
			u16			count;
			u16			level;
		} hdr;
		struct da_node_entry {
			dahash		hashval;
			dablk		before;
		} btree[1];
	};

	// Shortform Attributes

	struct attr_shortform {
		struct attr_sf_hdr {
			u16			totsize;
			u8			count;
		} hdr;

		enum {
			ATTR_ROOT,		// The attribute's namespace is "trusted".
			ATTR_SECURE,	// The attribute's namespace is "secure".
		};

		struct attr_sf_entry {
			u8			namelen;
			u8			valuelen;
			u8			flags;
			u8			nameval[1];
		} list[1];
	};


	// Leaf Attributes
	struct attr_leaf_map {
		u16 base;
		u16 size;
	};

	struct attr_leaf_hdr {
		da_blkinfo		info;
		u16				count;
		u16				usedbytes;
		u16				firstused;
		u8				holes;
		u8				pad1;
		attr_leaf_map	freemap[3];
	};

	struct attr_leaf_entry {
		u32			hashval;
		u16			nameidx;
		u8			flags;
		u8			pad2;
	};

	struct attr_leaf_name_local {
		u16			valuelen;
		u8			namelen;
		u8			nameval[1];
	};

	struct attr_leaf_name_remote {
		u32			valueblk;
		u32			valuelen;
		u8			namelen;
		u8			name[1];
	};

	struct attr_leafblock {
		attr_leaf_hdr			hdr;
		attr_leaf_entry			entries[1];
		attr_leaf_name_local	namelist;
		attr_leaf_name_remote	valuelist;
	};

	enum {
		ATTR_LEAF_MAGIC = 0xfbee
	};

	// Internal INodes
	//Quota Nodes

	enum {
		DQ_USER		= 0x0001,
		DQ_PROJ		= 0x0002,
		DQ_GROUP	= 0x0004,
	};

	struct disk_dquot {
		u16			d_magic;
		u8			d_version;
		u8			d_flags;
		u32			d_id;
		u64			d_blk_hardlimit;
		u64			d_blk_softlimit;
		u64			d_ino_hardlimit;
		u64			d_ino_softlimit;
		u64			d_bcount;
		u64			d_icount;
		u32			d_itimer;
		u32			d_btimer;
		u16			d_iwarns;
		u16			d_bwarns;
		u32			d_pad0;
		u64			d_rtb_hardlimit;
		u64			d_rtb_softlimit;
		u64			d_rtbcount;
		u32			d_rtbtimer;
		u16			d_rtbwarns;
		u16			d_pad;
	};

	struct dqblk {
		disk_dquot	dd_diskdq;
		char		dd_fill[32];
	};

	// On-disk INode

	enum dinode_fmt {
		DINODE_FMT_DEV,
		DINODE_FMT_LOCAL,
		DINODE_FMT_EXTENTS,
		DINODE_FMT_BTREE,
		DINODE_FMT_UUID
	};

	enum {
		DIFLAG_REALTIME		= 0x0001,		// The inode's data is located on the real-time device.
		DIFLAG_PREALLOC		= 0x0002,		// The inode's extents have been preallocated.
		DIFLAG_NEWRTBM		= 0x0004,		// Specifies the sb_rbmino uses the new real-time bm format
		DIFLAG_IMMUTABLE	= 0x0008,		// Specifies the inode cannot be modified.
		DIFLAG_APPEND		= 0x0010,		// The inode is in append only mode.
		DIFLAG_SYNC			= 0x0020,		// The inode is written synchronously.
		DIFLAG_NOATIME		= 0x0040,		// The inode's di_atime is not updated.
		DIFLAG_NODUMP		= 0x0080,		// Specifies the inode is to be ignored by xfsdump.
		DIFLAG_RTINHERIT	= 0x0100,		// For directory inodes, new inodes inherit the DIFLAG_REALTIME bit.
		DIFLAG_PROJINHERIT	= 0x0200,		// For directory inodes, new inodes inherit the di_projid value.
		DIFLAG_NOSYMLINKS	= 0x0400,		// For directory inodes, symlinks cannot be created.
		DIFLAG_EXTSIZE		= 0x0800,		// Specifies the extent size for real-time files or a and extent size hint for regular files.
		DIFLAG_EXTSZINHERIT	= 0x1000,		// For directory inodes, new inodes inherit the di_extsize value.
		DIFLAG_NODEFRAG		= 0x2000,		// Specifies the inode is to be ignored when defragmenting the filesystem.
	};

	struct timestamp {
		s32			t_sec;
		s32			t_nsec;
	};

	struct dinode_core {
		enum MODE {
			IFMT		= 0xF000,	//format mask
			IFSOCK		= 0xA000,	//socket
			IFLNK		= 0xC000,	//symbolic link
			IFREG		= 0x8000,	//regular file
			IFBLK		= 0x6000,	//block device
			IFDIR		= 0x4000,	//directory
			IFCHR		= 0x2000,	//character device
			IFIFO		= 0x1000,	//fifo

			ISUID		= 0x0800,	//SUID
			ISGID		= 0x0400,	//SGID
			ISVTX		= 0x0200,	//sticky bit

			IRWXU		= 0x01C0,	//user mask
			IRUSR		= 0x0100,	//read
			IWUSR		= 0x0080,	//write
			IXUSR		= 0x0040,	//execute

			IRWXG		= 0x0038,	//group mask
			IRGRP		= 0x0020,	//read
			IWGRP		= 0x0010,	//write
			IXGRP		= 0x0008,	//execute

			IRWXO		= 0x0007,	//other mask
			IROTH		= 0x0004,	//read
			IWOTH		= 0x0002,	//write
			IXOTH		= 0x0001,	//execute
		};

		u16				di_magic;
		flags<MODE,u16>	di_mode;
		s8				di_version;
		s8				di_format;
		u16				di_onlink;
		u32				di_uid;
		u32				di_gid;
		u32				di_nlink;
		u16				di_projid;
		u8				di_pad[8];
		u16				di_flushiter;
		timestamp		di_atime;
		timestamp		di_mtime;
		timestamp		di_ctime;
		fsize			di_size;
		drfsbno			di_nblocks;
		extlen			di_extsize;
		extnum			di_nextents;
		aextnum			di_anextents;
		u8				di_forkoff;
		s8				di_aformat;
		u32				di_dmevmask;
		u16				di_dmstate;
		u16				di_flags;
		u32				di_gen;
	};

	union di_u {
		bmdr_block		di_bmbt;
		bmbt_rec		di_bmx[1];
		dir2_sf			di_dir2sf;
		char			di_c[1];
		//	dev			di_dev;
		uuid			di_muuid;
		char			di_symlink[1];
	};

	union di_a {
		bmdr_block		di_abmbt;
		//	bmbt_rec	di_abmx[1];
		attr_shortform	di_attrsf;
	};

	struct dinode {
		dinode_core		di_core;
		u32				di_next_unlinked;
		di_u			u;
		di_a			a;
	};
};
#pragma pack()


template<> XFS_types<64>::bmbt_irec::bmbt_irec(bmbt_rec &r) {
	uint64	l0 = r.l0;
	uint64	l1 = r.l1;

	int		ext_flag = l0 >> 63;

	br_startoff		= (l0 >> 9) & bits64(54);
	br_startblock	= ((l0 & bits64(9)) << 43) | (l1 >> 21);
	br_blockcount	= (l1 & (1L << 21) - 1);
	br_state		= ext_flag ? EXT_UNWRITTEN : EXT_NORM;
}


// Partition Info

struct CHS {
	uint8	head, sector, cylinder;	//?
};

struct Partition {
	uint8	bootable;
	CHS		CHS_start;
	uint8	type;
	CHS		CHS_end;
	uint32	LBA_start;
	uint32	LBA_length;
};

#pragma pack(1)
struct MBR {
	uint8		code[0x1b8];
	uint32		disk_sig;
	uint16		pad;
	Partition	part[4];
	uint16		mbr_sig;
};
#pragma pack()


class XFS : public XFS_types<64> {
	istream_ref		file;
	sb			sb;
	uint32		blocksize;
	uint32		agblocks;
	uint32		inodebits;

public:
	uint32		GetAG(uint64 bno)		{ return uint32(bno >> sb.sb_agblklog);	}
	uint64		GetAGStart(uint32 ag)	{ return ag < 16 ? uint64(ag) * sb.sb_agblocks * blocksize : 0; }
	uint64		GetAGBits(uint64 ino)	{ return ino & ~bits64(inodebits); }

	istream_ref		GetAGFile(uint64 ag) {
#if 1
		return file;
#else
		if (ag < 16)
			return &file;

		WinInput	&agf = agfile[ag - 16];
		if (!agf.exists()) {
			char	name[256];
			sprintf(name, "W:\\ag%i", ag);
			agf.join(CreateFile(name,
				GENERIC_READ,
				FILE_SHARE_READ,
				0,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				0
			));
		}
		return &agf;
#endif
	}

	dinode *GetINode(uint64 ino, void *buffer) {
		uint8		sector[512];
		uint32		ag = uint32(ino >> inodebits);

//		if (ag >= 16)
//			return NULL;

//		auto	file = GetAGFile(ag);
		file.seek(GetAGStart(ag) + (ino & (bits64(inodebits) - 1)) * 256);
		if (file.read(sector)) {
			memcpy(buffer, sector + (ino & 1) * 256, 256);
			return (dinode*)buffer;
		}
		return NULL;
	}


	void	*GetBlocks(void *buffer, uint64 bno, uint64 count) {
		uint32	ag		= GetAG(bno);
//		auto	file	= GetAGFile(ag);
		file.seek(GetAGStart(ag) + (bno & bits64(sb.sb_agblklog)) * blocksize);
		file.readbuff(buffer, blocksize * count);
		return buffer;
	}

	malloc_block	GetBlocks(uint64 bno, uint64 count) {
		malloc_block	buffer(count * blocksize);
		GetBlocks(buffer, bno, count);
		return buffer;
	}

	XFS(istream_ref _file) : file(_file) {
		uint8		sector[512];

		file.read(sector);
		raw_copy(sector, sb);

		blocksize	= sb.sb_blocksize;
		agblocks	= sb.sb_agblocks;
		inodebits	= sb.sb_agblklog + sb.sb_inopblog;
	}

	uint64	Root()		{ return sb.sb_rootino; }
	uint32	BlockSize()	{ return blocksize;	}
};

ISO_ptr<void> GetAll(XFS &xfs, uint64 ino, tag id, bool raw) {
	char		inodebuffer[256];
	char		name[256];
	uint64		agbits		= xfs.GetAGBits(ino);
	XFS::dinode	*inode		= xfs.GetINode(ino, inodebuffer);

	if (!inode)
		return ISO_NULL;

	if (inode->di_core.di_mode.test(XFS::dinode_core::IFDIR)) {

		ISO_ptr<anything>	p(id);

		switch (inode->di_core.di_format) {
			case XFS::DINODE_FMT_LOCAL: {
				XFS::dir2_sf			&dir	= inode->u.di_dir2sf;
				XFS::dir2_sf_entry	*entry	= dir.list;
				int					n		= dir.hdr.count;
				bool				use4	= dir.hdr.i8count == 0;

				if (use4)
					entry = (XFS::dir2_sf_entry*)((char*)entry - 4);

				for (int i = 0; i < n; i++) {
					uint64	ino2;

					memcpy(name, entry->name, entry->namelen);
					name[entry->namelen] = 0;

					XFS::dir2_inou *inop = (XFS::dir2_inou*)(entry->name + entry->namelen);
					if (use4 && uint64(inop->i4) != (agbits | inop->i4)) {
						printf("Possible problem with %s!\n", entry->name);
					}
					ino2	= use4 ? uint64(inop->i4) : inop->i8;
					entry	= (XFS::dir2_sf_entry*)(entry->name + entry->namelen + (use4 ? 4 : 8));

					p->Append(GetAll(xfs, ino2, name, raw));
				}
				break;
			}

			case XFS::DINODE_FMT_EXTENTS: {
				uint64				size	= inode->di_core.di_size;
				malloc_block		dir_block(size);
				XFS::dir2_block		*dir	= dir_block;

				for (int i = 0, n = inode->di_core.di_nextents; i < n; i++) {
					XFS::bmbt_irec		rec		= inode->u.di_bmx[i];
					uint64				bno		= rec.br_startblock;
					uint64				count	= rec.br_blockcount;
					uint64				offset	= rec.br_startoff;
					if (!(offset & XFS::DIR2_LEAF_OFFSET))
						xfs.GetBlocks((char*)dir + offset * xfs.BlockSize(), rec.br_startblock, count);
				}
/*
				XFS::bmbt_irec			rec		= inode->u.di_bmx[0];
				uint32					count	= rec.br_blockcount;
				XFS::dir2_block			*dir	= (XFS::dir2_block*)xfs.GetBlocks(rec.br_startblock, count);
*/
				XFS::dir2_data_entry	*entry = &dir->u[0].entry;
				void					*end;

				if (inode->di_core.di_nblocks == 1) {
					XFS::dir2_block_tail	*tail	= (XFS::dir2_block_tail*)((char*)dir + size) - 1;
					XFS::dir2_leaf_entry	*leaf	= (XFS::dir2_leaf_entry*)tail - tail->count;
					end	= leaf;
				} else {
					end	= (char*)dir + size;
				}

				for (int i = 0; (void*)entry < end; i++) {
					if ((((char*)entry - (char*)dir) & (xfs.BlockSize() - 1)) == 0)
						entry = (XFS::dir2_data_entry*)((char*)entry + sizeof(XFS::dir2_data_hdr));
					XFS::dir2_data_unused	*unused = (XFS::dir2_data_unused*)entry;
					if (unused->freetag == 0xffff) {
						entry = (XFS::dir2_data_entry*)((char*)entry + uint32(unused->length));
					} else {
						memcpy(name, entry->name, entry->namelen);
						name[entry->namelen] = 0;

						uint32	entrylen	= (sizeof(XFS::dir2_data_entry) + entry->namelen -1 + 7) & ~7;
						if (i >= 2)
							p->Append(GetAll(xfs, entry->inumber, name, raw));
						entry = (XFS::dir2_data_entry*)((char*)entry + entrylen);
					}
				}
				break;
			}
			case XFS::DINODE_FMT_BTREE:
				printf("%s uses a btree\n", name);
				break;
		}
		return p;

	} else if (inode->di_core.di_mode.test(XFS::dinode_core::IFREG)) {

		switch (inode->di_core.di_format) {
			case XFS::DINODE_FMT_EXTENTS: {
				uint64		size	= inode->di_core.di_size;
				malloc_block	buffer(size);
				memory_writer	out(buffer);
				for (int i = 0, n = inode->di_core.di_nextents; i < n; i++) {
					XFS::bmbt_irec		rec		= inode->u.di_bmx[i];
					uint64				bno		= rec.br_startblock;
					uint64				count	= rec.br_blockcount;
					uint64				offset	= rec.br_startoff * xfs.BlockSize();
					out.seek(offset);

					while (count) {
						uint32	n			= (uint32)min(count, 1024);
						malloc_block blocks	= xfs.GetBlocks(bno, n);
						uint64	read		= min(uint64(n * xfs.BlockSize()), size - offset);
						offset	+= out.writebuff(blocks, read);
						bno		+= n;
						count	-= n;
					}
				}
				return ReadData1(id, memory_reader(buffer).me(), (uint32)size, raw);
			}
			case XFS::DINODE_FMT_BTREE:
				printf("%s uses a btree\n", name);
				break;
		}

	}
	return ISO_NULL;
}

class XFSFileHandler : FileHandler {
	const char*		GetDescription() override { return "XFS filesystem";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override { return false;		}
} xfs;

ISO_ptr<void> XFSFileHandler::Read(tag id, istream_ref file) {
#if 0
	uint32	maxblocks	= 256;
	void	*buffer		= malloc(4096 * maxblocks);
	uint32	bpag		= 0x745204;

	for (int i = 16; i < 32; i++) {
		streamptr	end		= out.tell();
		uint32		bno		= end / 4096;
		uint64		sec		= bpag * 8 * i - 0x3a19A8A4 + 0x2A4;

		file.seek(sec * 512 + end);

		while (bno < bpag) {
			uint32	n	= min(bpag - bno, maxblocks);

			file.readbuff(buffer, n * 4096);

			uint64	*p = (uint64*)buffer;
			uint64	*e = p + n * 4096 / 8;
			while (p < e && !*p++);

			if (p < e) {
				out.seek(streamptr(bno) * 4096);
				out.writebuff(buffer, n * 4096);
			}
			bno	+= n;
		}
	}
#endif
	XFS			xfs(file);
	return GetAll(xfs, xfs.Root(), id, WantRaw());
}

struct ext2_sb	{
	uint32		s_inodes_count;			//Count of inodes in the filesystem
	uint32		s_blocks_count;			//Count of blocks in the filesystem
	uint32		s_r_blocks_count;		//Count of the number of reserved blocks
	uint32		s_free_blocks_count;	//Count of the number of free blocks
	uint32		s_free_inodes_count;	//Count of the number of free inodes
	uint32		s_first_data_block;		//The first block which contains data
	uint32		s_log_block_size;		//Indicator of the block size
	uint32		s_log_frag_size;		//Indicator of the size of the fragments
	uint32		s_blocks_per_group;		//Count of the number of blocks in each block group
	uint32		s_frags_per_group;		//Count of the number of fragments in each block group
	uint32		s_inodes_per_group;		//Count of the number of inodes in each block group
	uint32		s_mtime;				//The time that the filesystem was last mounted
	uint32		s_wtime;				//The time that the filesystem was last written to
	uint16		s_mnt_count;			//The number of times the file system has been mounted
	int16		s_max_mnt_count;		//The number of times the file system can be mounted
	uint16		s_magic;				//Magic number indicating ex2fs
	uint16		s_state;				//Flags indicating the current state of the filesystem
	uint16		s_errors;				//Flags indicating the procedures for error reporting
	uint16		s_pad;					//padding
	uint32		s_lastcheck;			//The time that the filesystem was last checked
	uint32		s_checkinterval;		//The maximum time permissible between checks
	uint32		s_creator_os;			//Indicator of which OS created the filesystem
	uint32		s_rev_level;			//The revision level of the filesystem
	uint32		s_reserved[235];		//padding to 1024 bytes
};

struct ext2_bg {
	uint32		bg_block_bitmap;		//The address of the block containing the block bm for this group
	uint32		bg_inode_bitmap;		//The address of the block containing the inode bm for this group
	uint32		bg_inode_table;			//The address of the block containing the inode table for this group
	uint16		bg_free_blocks_count;	//The count of free blocks in this group
	uint16		bg_free_inodes_count;	//The count of free inodes in this group
	uint16		bg_used_dirs_count;		//The number inodes in this group which are directories
	uint16		bg_pad;					//padding
	uint32		bg_reserved[3];			//padding
};

enum {
	EXT2_NDIR_BLOCKS	= 12,	//direct block addresses
	EXT2_IND_BLOCK		= 12,	//the indirect block - which contains a list of addresses of blocks which contain the data
	EXT2_DIND_BLOCK		= 13,	//double indirect block. It contains the address of a block which has a list of indirect block addresses. Each indirect block then has another list is blocks.
	EXT2_TIND_BLOCK		= 14	//triple indirect block. It contains a list of double indirect blocks etc.
};

struct ext2_inode {
	enum MODE {
		IFMT		= 0xF000,	//format mask
		IFSOCK		= 0xA000,	//socket
		IFLNK		= 0xC000,	//symbolic link
		IFREG		= 0x8000,	//regular file
		IFBLK		= 0x6000,	//block device
		IFDIR		= 0x4000,	//directory
		IFCHR		= 0x2000,	//character device
		IFIFO		= 0x1000,	//fifo

		ISUID		= 0x0800,	//SUID
		ISGID		= 0x0400,	//SGID
		ISVTX		= 0x0200,	//sticky bit

		IRWXU		= 0x01C0,	//user mask
		IRUSR		= 0x0100,	//read
		IWUSR		= 0x0080,	//write
		IXUSR		= 0x0040,	//execute

		IRWXG		= 0x0038,	//group mask
		IRGRP		= 0x0020,	//read
		IWGRP		= 0x0010,	//write
		IXGRP		= 0x0008,	//execute

		IRWXO		= 0x0007,	//other mask
		IROTH		= 0x0004,	//read
		IWOTH		= 0x0002,	//write
		IXOTH		= 0x0001,	//execute
	};
	flags<MODE,uint16>	i_mode;			//File mode
	uint16				i_uid;			//Owner Uid
	uint32				i_size;			//Size in bytes
	uint32				i_atime;		//Access time
	uint32				i_ctime;		//Creation time
	uint32				i_mtime;		//Modification time
	uint32				i_dtime;		//Deletion Time
	uint16				i_gid;			//Group Id
	uint16				i_links_count;	//Links count
	uint32				i_blocks;		//Blocks count
	uint32				i_flags;		//File flags
	uint32				i_reserved1;	//OS dependent
	uint32				i_block[15];	//Pointers to blocks
	uint32				i_version;		//File version (for NFS)
	uint32				i_file_acl;		//File ACL
	uint32				i_dir_acl;		//Directory ACL
	uint32				i_faddr;		//Fragment address
	uint8				i_frag;			//Fragment number
	uint8				i_fsize;		//Fragment size
	uint16				i_pad1;
	uint32				i_reserved2[2];
};

enum EXT2_INO {
	EXT2_BAD_INO			= 1,	//Bad blocks inode
	EXT2_ROOT_INO			= 2,	//Root inode
	EXT2_ACL_IDX_INO		= 3,	//ACL inode
	EXT2_ACL_DATA_INO		= 4,	//ACL inode
	EXT2_BOOT_LOADER_INO	= 5,	//Boot loader inode
	EXT2_UNDEL_DIR_INO		= 6,	//Undelete directory inode
	EXT2_FIRST_INO			= 11,	//First non reserved inode
};

struct ext2_entry {
	uint32	inode;					//address if inode
	uint16	rec_len;				//length of this record
	uint8	name_len, name_len2;				//length of file name
	char	name[0];				//the file name
};

//|Superblock | Group Descriptors |Block Bitmap|INode Bitmap|INode Table|Data blocks|
//|-------------------------------|-------------------------------------------------|
//|This is the same for all groups| this is specific to each group                  |

class EXT2 {
	istream_ref		file;
	ext2_sb		sb;
	uint32		block_size;
	uint32		num_groups;

	struct group {
		ext2_bg		bg;
		ext2_inode	*inodes;
		uint8		*block_bm;
		uint8		*inode_bm;
	} *groups;

	uint64		GetBlock(uint32 bno) {
		return (sb.s_first_data_block + bno - 1) * block_size;
	}

	int			GetData(void *dest, uint32 *blocks, uint32 count) {
		int	n = 0;
		for (uint32 b; count-- && (b = *blocks++); dest = (uint8*)dest + block_size, n++) {
			file.seek(GetBlock(b));
			file.readbuff(dest, block_size);
		}
		return n;
	}

	int			GetData2(void *dest, uint32 bno) {
		malloc_block	block(block_size);
		file.seek(GetBlock(bno));
		file.readbuff(block, block_size);
		int	n = GetData(dest, (uint32*)block, block_size / sizeof(uint32));
		return n;
	}

	int			GetData3(void *dest, uint32 bno) {
		malloc_block	block(block_size);

		file.seek(GetBlock(bno));
		file.readbuff(block, block_size);

		int		n		= 0;
		uint32	*blocks	= (uint32*)block;
		for (uint32 count = block_size / sizeof(uint32), b; count-- && (b = *blocks++);)
			n += GetData2((uint8*)dest + n * block_size, b);

		return n;
	}
public:
	ext2_inode&	GetINode(uint32 ino) {
		ino--;
		uint32	g = ino / sb.s_inodes_per_group;
		uint32	o = ino % sb.s_inodes_per_group;
		return groups[g].inodes[o];
	}

	malloc_block	GetData(ext2_inode &i) {
		malloc_block	buffer(divup(i.i_size, block_size) * block_size);
		int		n		= GetData(buffer, i.i_block, EXT2_NDIR_BLOCKS);

		if (uint32 b = i.i_block[EXT2_IND_BLOCK])
			n += GetData2(buffer + n * block_size, b);

		if (uint32 b = i.i_block[EXT2_DIND_BLOCK])
			n += GetData3(buffer + n * block_size, b);

		if (uint32 b = i.i_block[EXT2_TIND_BLOCK]) {
			malloc_block	block(block_size);
			file.seek(GetBlock(b));
			file.readbuff(block, block_size);
			uint32	*blocks	= (uint32*)block;
			for (uint32 count = block_size / sizeof(uint32), b; count-- && (b = *blocks++);)
				n += GetData3(buffer + n * block_size, b);
		}
		return buffer;
	}

	EXT2(istream_ref _file) : file(_file) {
		file.seek(1024);
		sb				= file.get();

		if (sb.s_magic != 0xEF53)
			return;

		block_size		= 1024 << sb.s_log_block_size;
		num_groups		= divup(sb.s_blocks_count, sb.s_blocks_per_group);

		groups			= new group[num_groups];
		for (int i = 0; i < num_groups; i++)
			groups[i].bg = file.get();

		uint32	block_bm_size	= divup(divup(sb.s_blocks_per_group, 8), block_size);
		uint32	inode_bm_size	= divup(divup(sb.s_inodes_per_group, 8), block_size);
		uint32	group_desc_size	= divup(num_groups * sizeof(ext2_bg), block_size);

		for (int i = 0; i < num_groups; i++) {
			group	&g	= groups[i];
			file.seek((sb.s_first_data_block + i * sb.s_blocks_per_group + group_desc_size + g.bg.bg_block_bitmap) * block_size);
			file.readbuff(g.block_bm = new uint8[block_bm_size * block_size], block_bm_size * block_size);

			file.seek((sb.s_first_data_block + g.bg.bg_inode_bitmap) * block_size);
			file.readbuff(g.inode_bm = new uint8[inode_bm_size * block_size], inode_bm_size * block_size);

			file.seek((sb.s_first_data_block + g.bg.bg_inode_table) * block_size);
			file.readbuff(g.inodes = new ext2_inode[sb.s_inodes_per_group], sb.s_inodes_per_group * sizeof(ext2_inode));
		}
	}

	~EXT2() {
		if (Valid()) {
			for (int i = 0; i < num_groups; i++) {
				group	&g	= groups[i];
				delete[] g.block_bm;
				delete[] g.inode_bm;
				delete[] g.inodes;
			}
			delete[] groups;
		}
	}
	bool	Valid() { return sb.s_magic == 0xEF53; }
};

ISO_ptr<void> GetAll(EXT2 &ext2, uint32 ino, tag id, bool raw) {
	ext2_inode	&inode = ext2.GetINode(ino);

	switch (inode.i_mode & ext2_inode::IFMT) {
		case ext2_inode::IFDIR: {
			ISO_ptr<anything>	p(id);
			malloc_block	buffer	= ext2.GetData(inode);

			for (ext2_entry *entry = buffer, *end = (ext2_entry*)buffer.end(); entry < end && entry->rec_len; entry = (ext2_entry*)((uint8*)entry + entry->rec_len)) {
				if (entry->inode == 0)
					continue;
				if (entry->name[0] == '.' && (entry->name_len == 1 || entry->name[1] == '.'))
					continue;
				char name[256];
				memcpy(name, entry->name, entry->name_len);
				name[entry->name_len] = 0;
				p->Append(GetAll(ext2, entry->inode, name, raw));
			}

			return p;
		}

		case ext2_inode::IFREG:
			return ReadData1(id, memory_reader(ext2.GetData(inode)).me(), inode.i_size, raw);

		case ext2_inode::IFSOCK: {
			char	name[256];
			memcpy(name, inode.i_block, inode.i_size);
			name[inode.i_size] = 0;
			return ISO_ptr<string>(id, name);
		}

		case ext2_inode::IFBLK:
		case ext2_inode::IFCHR:
			return ISO_ptr<uint32>(id, inode.i_block[0]);

		default:
			return ISO_NULL;
	}
}

class EXT2FileHandler : FileHandler {
	const char*		GetDescription() override { return "ext2 filesystem";	}

	int				Check(istream_ref file) override {
		if (file.length() < 1024 + sizeof(ext2_sb))
			return CHECK_DEFINITE_NO;
		file.seek(1024);
		return file.get<ext2_sb>().s_magic == 0xEF53 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		EXT2	ext2(file);
		return ext2.Valid() ? GetAll(ext2, EXT2_ROOT_INO, id, WantRaw()) : ISO_NULL;
	}
} ext2;
