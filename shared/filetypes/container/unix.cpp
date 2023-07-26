#include "iso/iso_files.h"
#include "archive_help.h"
#include "base/bits.h"
#include "extra/json.h"
#include "bin.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	XFS
//-----------------------------------------------------------------------------

#define	XFS_AGF_VERSION	1
#define XFS_BIG_BLKNOS	0

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

//-----------------------------------------------------------------------------
//	EXT2/3/4
//-----------------------------------------------------------------------------
#undef ERROR

namespace EXT {

enum class FEATURE_COMPAT {
	DIR_PREALLOC	= 0x0001,
	IMAGIC_INODES	= 0x0002,
	HAS_JOURNAL		= 0x0004,
	EXT_ATTR		= 0x0008,
	RESIZE_INODE	= 0x0010,
	DIR_INDEX		= 0x0020,
	EXT2_SUPP		= EXT_ATTR,
	EXT3_SUPP		= EXT_ATTR,
	EXT4_SUPP		= EXT_ATTR,
};
constexpr bool operator&(FEATURE_COMPAT a, FEATURE_COMPAT b) { return uint32(a) & uint32(b); }

enum class FEATURE_RO_COMPAT {
	SPARSE_SUPER	= 0x0001,
	LARGE_FILE		= 0x0002,
	BTREE_DIR		= 0x0004,
	HUGE_FILE		= 0x0008,
	GDT_CSUM		= 0x0010,
	DIR_NLINK		= 0x0020,
	EXTRA_ISIZE		= 0x0040,
	QUOTA			= 0x0100,
	BIGALLOC		= 0x0200,
	METADATA_CSUM	= 0x0400,
	EXT2_SUPP		= SPARSE_SUPER| LARGE_FILE| BTREE_DIR,
	EXT3_SUPP		= SPARSE_SUPER| LARGE_FILE| BTREE_DIR,
	EXT4_SUPP		= SPARSE_SUPER| LARGE_FILE| GDT_CSUM| DIR_NLINK | EXTRA_ISIZE | BTREE_DIR |HUGE_FILE | BIGALLOC | METADATA_CSUM | QUOTA,
};
constexpr bool operator&(FEATURE_RO_COMPAT a, FEATURE_RO_COMPAT b) { return uint32(a) & uint32(b); }

// if there is a bit set in the incompatible feature set that the kernel doesn’t know about, it should refuse to mount the filesystem
enum class FEATURE_INCOMPAT {
	COMPRESSION		= 0x0001,
	FILETYPE		= 0x0002,
	RECOVER			= 0x0004,
	JOURNAL_DEV		= 0x0008,
	META_BG			= 0x0010,	
	EXTENTS			= 0x0040,
	BIT64			= 0x0080,	
	MMP				= 0x0100,	
	FLEX_BG			= 0x0200,	
	EA_INODE		= 0x0400,
	DIRDATA			= 0x1000,
	BG_USE_META_CSUM= 0x2000,
	LARGEDIR		= 0x4000,	// >2GB or 3-lvl htree
	INLINEDATA		= 0x8000,
	EXT2_SUPP		= FILETYPE| META_BG,
	EXT3_SUPP		= FILETYPE| RECOVER| META_BG,
	EXT4_SUPP		= FILETYPE| RECOVER| META_BG| EXTENTS| BIT64 | FLEX_BG| MMP,
};
constexpr bool operator&(FEATURE_INCOMPAT a, FEATURE_INCOMPAT b) { return uint32(a) & uint32(b); }

enum class OS : uint32 {
	LINUX		= 0,
	HURD		= 1,
	MASIX		= 2,
	FREEBSD		= 3,
	LITES		= 4,
};

enum class FS_STATE : uint16 {
	VALID		= 0x0001,  /* Unmounted cleanly */
	ERROR		= 0x0002,  /* Errors detected */
	ORPHAN		= 0x0004,  /* Orphans being recovered */
};

enum class FS_FLAGS {
	SIGNED_HASH		= 0x0001,	/* Signed dirhash in use */
	UNSIGNED_HASH	= 0x0002,	/* Unsigned dirhash in use */
	TEST_FILESYS	= 0x0004,	/* to test development code */
};

enum class MOUNT_FLAGS : uint32 {
	GRPID				= 0x00004, /* Create files with directory's group */
	DEBUG				= 0x00008, /* Some debugging messages */
	ERRORS_CONT			= 0x00010, /* Continue on errors */
	ERRORS_RO			= 0x00020, /* Remount fs ro on errors */
	ERRORS_PANIC		= 0x00040, /* Panic on errors */
	ERRORS_MASK			= 0x00070,
	MINIX_DF			= 0x00080, /* Mimics the Minix statfs */
	NOLOAD				= 0x00100, /* Don't use existing journal*/
	DATA_FLAGS			= 0x00C00, /* Mode for data writes: */
	JOURNAL_DATA		= 0x00400, /* Write data to journal */
	ORDERED_DATA		= 0x00800, /* Flush data before commit */
	WRITEBACK_DATA		= 0x00C00, /* No data ordering */
	UPDATE_JOURNAL		= 0x01000, /* Update the journal format */
	NO_UID32			= 0x02000 , /* Disable 32-bit UIDs */
	XATTR_USER			= 0x04000, /* Extended user attributes */
	POSIX_ACL			= 0x08000, /* POSIX Access Control Lists */
	NO_AUTO_DA_ALLOC	= 0x10000, /* No auto delalloc mapping */
	BARRIER				= 0x20000, /* Use block barriers */
	QUOTA				= 0x80000, /* Some quota option set */
	USRQUOTA			= 0x100000, /* "old" user quota */
	GRPQUOTA			= 0x200000, /* "old" group quota */
	DIOREAD_NOLOCK		= 0x400000, /* Enable support for dio read nolocking */
	JOURNAL_CHECKSUM	= 0x800000, /* Journal checksums */
	JOURNAL_ASYNC_COMMIT= 0x1000000, /* Journal Async Commit */
	MBLK_IO_SUBMIT		= 0x4000000, /* multi-block io submits */
	DELALLOC			= 0x8000000, /* Delalloc support */
	DATA_ERR_ABORT		= 0x10000000, /* Abort on file data write */
	BLOCK_VALIDITY		= 0x20000000, /* Block validity checking */
	DISCARD				= 0x40000000, /* Issue DISCARD requests */
	INIT_INODE_TABLE	= 0x80000000, /* Initialize uninitialized itables */
//#define EXT4_MOUNT2_EXPLICIT_DELALLOC   0x00000001 /* User explicitly specified delalloc */
};

enum class ERROR_POLICY : uint16 {
	CONTINUE		= 1,   /* Continue execution */
	RO				= 2,   /* Remount fs read-only */
	PANIC			= 3,   /* Panic */
};


/*
* Default mount options
*/
enum class DEFAULT_MOUNT {
	DEBUG			= 0x0001,
	BSDGROUPS		= 0x0002,
	XATTR_USER		= 0x0004,
	ACL				= 0x0008,
	UID16			= 0x0010,
	JMODE			= 0x0060,
	JMODE_DATA		= 0x0020,
	JMODE_ORDERED	= 0x0040,
	JMODE_WBACK		= 0x0060,
	NOBARRIER		= 0x0100,
	BLOCK_VALIDITY	= 0x0200,
	DISCARD			= 0x0400,
	NODELALLOC		= 0x0800,
};

enum class HASH_VERSION : uint8 {
	LEGACY				= 0,
	HALF_MD4			= 1,
	TEA					= 2,
	LEGACY_UNSIGNED		= 3,
	HALF_MD4_UNSIGNED	= 4,
	TEA_UNSIGNED		= 5,
};

struct superblock	{
	enum {MAGIC = 0xEF53};
//0x000
	uint32 	s_inodes_count;
	uint32 	s_blocks_count;
	uint32 	s_r_blocks_count;
	uint32 	s_free_blocks_count;
	uint32 	s_free_inodes_count;
	uint32 	s_first_data_block;
	uint32 	s_log_block_size;
	uint32 	s_log_cluster_size;
	uint32 	s_blocks_per_group;
	uint32 	s_clusters_per_group;
	uint32 	s_inodes_per_group;
	uint32 	s_mtime;
	uint32 	s_wtime;
	uint16 	s_mnt_count;
	uint16 	s_max_mnt_count;
	uint16 	s_magic;
	FS_STATE 		state;
	ERROR_POLICY 	errors;
	uint16 	s_minor_rev_level;
	uint32 	s_lastcheck;
	uint32 	s_checkinterval;
	OS 		creator_os;
	uint32 	s_rev_level;	//0: old, 1:EXT4_DYNAMIC_REV
	uint16 	s_def_resuid;
	uint16 	s_def_resgid;

	//These fields are for EXT4_DYNAMIC_REV superblocks only

	uint32 	s_first_ino;
	uint16 	s_inode_size;
	uint16 	s_block_group_nr;
	FEATURE_COMPAT 		feature_compat;
	FEATURE_INCOMPAT 	feature_incompat;
	FEATURE_RO_COMPAT 	feature_ro_compat;
	uint8 	s_uuid[16];
	char 	s_volume_name[16];
	char 	s_last_mounted[64];
	uint32 	s_algorithm_usage_bitmap;
	uint8 	s_prealloc_blocks;
	uint8 	s_prealloc_dir_blocks;
	uint16 	s_reserved_gdt_blocks;
	uint8 	s_journal_uuid [16];
	uint32 	s_journal_inum;
	uint32 	s_journal_dev;
	uint32 	s_last_orphan;
	uint32 	s_hash_seed [4];
	HASH_VERSION 	def_hash_version;
	uint8 	s_jnl_backup_type;
	uint16 	s_desc_size;

//0x100
	DEFAULT_MOUNT	default_mount_opts;
	uint32 	s_first_meta_bg;

	//ext4
	uint32 	s_mkfs_time;
	uint32 	s_jnl_blocks[17];
	uint32 	s_blocks_count_hi;
	uint32 	s_r_blocks_count_hi;
	uint32 	s_free_blocks_count_hi;
	uint16 	s_min_extra_isize;
	uint16 	s_want_extra_isize;
	FS_FLAGS 	flags;
	uint16 	s_raid_stride;
	uint16 	s_mmp_update_interval;
	uint64 	s_mmp_block;
	uint32 	s_raid_stripe_width;
	uint8 	s_log_groups_per_flex;
	uint8 	s_checksum_type;
	uint16 	s_reserved_pad;
	uint64 	s_kbytes_written;
	uint32 	s_snapshot_inum;
	uint32 	s_snapshot_id;
	uint64 	s_snapshot_r_blocks_count;
	uint32 	s_snapshot_list;
	uint32 	s_error_count;
	uint32 	s_first_error_time;
	uint32 	s_first_error_ino;
	uint64 	s_first_error_block;
	uint8 	s_first_error_func[32];
	uint32 	s_first_error_line;
	uint32 	s_last_error_time;
	uint32 	s_last_error_ino;
	uint32 	s_last_error_line;
	uint64 	s_last_error_block;
	uint8 	s_last_error_func[32];
//0x200
	uint8 	s_mount_opts[64];
	uint32 	s_usr_quota_inum;
	uint32 	s_grp_quota_inum;
	uint32 	s_overhead_clusters;
	uint32 	s_reserved[108];
	uint32 	s_checksum;
//0x400

	constexpr bool	valid() const { return s_magic == MAGIC; }

};

static_assert(sizeof(superblock)==1024, "?");


//32 bytes for ext2
//
struct blockgroup {
	uint32		bg_block_bitmap;		//The address of the block containing the block bm for this group
	uint32		bg_inode_bitmap;		//The address of the block containing the inode bm for this group
	uint32		bg_inode_table;			//The address of the block containing the inode table for this group
	uint16		bg_free_blocks_count;	//The count of free blocks in this group
	uint16		bg_free_inodes_count;	//The count of free inodes in this group
	uint16		bg_used_dirs_count;		//The number inodes in this group which are directories

//ext3
	uint16 		bg_flags;
	uint32 		bg_exclude_bitmap;
	uint16 		bg_block_bitmap_csum;
	uint16 		bg_inode_bitmap_csum;
	uint16 		bg_itable_unused;
	uint16 		bg_checksum;

//0x020
	uint32 		bg_block_bitmap_hi;
	uint32 		bg_inode_bitmap_hi;
	uint32 		bg_inode_table_hi;
	uint16 		bg_free_blocks_count_hi;
	uint16 		bg_free_inodes_count_hi;
	uint16 		bg_used_dirs_count_hi;
	uint16 		bg_itable_unused_hi;
	uint32 		bg_exclude_bitmap_hi;
	uint16 		bg_block_bitmap_csum_hi;
	uint16 		bg_inode_bitmap_csum_hi;
	uint32 		bg_reserved;
//0x040

	auto	block_bitmap()		const { return lo_hi(bg_block_bitmap, bg_block_bitmap_hi); }
	auto	inode_bitmap()		const { return lo_hi(bg_inode_bitmap, bg_inode_bitmap_hi); }
	auto	inode_table()		const { return lo_hi(bg_inode_table, bg_inode_table_hi); }
	auto	free_blocks_count()	const { return lo_hi(bg_free_blocks_count, bg_free_blocks_count_hi); }
	auto	free_inodes_count()	const { return lo_hi(bg_free_inodes_count, bg_free_inodes_count_hi); }
	auto	used_dirs_count()	const { return lo_hi(bg_used_dirs_count, bg_used_dirs_count_hi); }
	auto	exclude_bitmap()	const { return lo_hi(bg_exclude_bitmap, bg_exclude_bitmap_hi); }
	auto	block_bitmap_csum()	const { return lo_hi(bg_block_bitmap_csum, bg_block_bitmap_csum_hi); }
	auto	inode_bitmap_csum()	const { return lo_hi(bg_inode_bitmap_csum, bg_inode_bitmap_csum_hi); }
	auto	itable_unused()		const { return lo_hi(bg_itable_unused, bg_itable_unused_hi); }

};

enum {
	NDIR_BLOCKS	= 12,	//direct block addresses
	IND_BLOCK		= 12,	//the indirect block - which contains a list of addresses of blocks which contain the data
	DIND_BLOCK		= 13,	//double indirect block. It contains the address of a block which has a list of indirect block addresses. Each indirect block then has another list is blocks.
	TIND_BLOCK		= 14,	//triple indirect block. It contains a list of double indirect blocks etc.

	N_BLOCKS
};

struct inode {
	enum MODE : uint16 {
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

	enum class FLAGS : uint32 {
		SECRM			= 0x00000001, /* Secure deletion */
		UNRM			= 0x00000002, /* Undelete */
		COMPR			= 0x00000004, /* Compress file */
		SYNC			= 0x00000008, /* Synchronous updates */
		IMMUTABLE		= 0x00000010, /* Immutable file */
		APPEND			= 0x00000020, /* writes to file may only append */
		NODUMP			= 0x00000040, /* do not dump file */
		NOATIME			= 0x00000080, /* do not update atime */
		DIRTY			= 0x00000100,
		COMPRBLK		= 0x00000200, /* One or more compressed clusters */
		NOCOMPR			= 0x00000400, /* Don't compress */
		ECOMPR			= 0x00000800, /* Compression error */
		INDEX			= 0x00001000, /* hash-indexed directory */
		IMAGIC			= 0x00002000, /* AFS directory */
		JOURNAL_DATA	= 0x00004000, /* file data should be journaled */
		NOTAIL			= 0x00008000, /* file tail should not be merged */
		DIRSYNC			= 0x00010000, /* dirsync behaviour (directories only) */
		TOPDIR			= 0x00020000, /* Top of directory hierarchies*/
		HUGE_FILE		= 0x00040000, /* Set to each huge file */
		EXTENTS			= 0x00080000, /* Inode uses extents */
		EA_INODE		= 0x00200000, /* Inode used for large EA */
		EOFBLOCKS		= 0x00400000, /* Blocks allocated beyond EOF */
		RESERVED		= 0x80000000, /* reserved for ext4 lib */

		USER_VISIBLE	= 0x004BDFFF, /* User visible flags */
		USER_MODIFIABLE	= 0x004B80FF, /* User modifiable flags */

		// Flags that should be inherited by new inodes from their parent
		INHERITED		= SECRM | UNRM | COMPR | SYNC | NODUMP | NOATIME | NOCOMPR | JOURNAL_DATA | NOTAIL | DIRSYNC,

		// Flags that are appropriate for regular files (all but dir-specific ones)
		REG_MASK		= ~(DIRSYNC | TOPDIR),

		// Flags that are appropriate for non-directories/regular files
		OTHER_MASK		= NODUMP | NOATIME,
	};
	friend constexpr bool operator&(FLAGS a, FLAGS b) { return uint32(a) & uint32(b); }

	MODE		i_mode;			//File mode
	uint16		i_uid;			//Owner Uid
	uint32		i_size;			//Size in bytes
	uint32		i_atime;		//Access time
	uint32		i_ctime;		//Creation time
	uint32		i_mtime;		//Modification time
	uint32		i_dtime;		//Deletion Time
	uint16		i_gid;			//Group Id
	uint16		i_links_count;	//Links count
	uint32		i_blocks;		//Blocks count
	FLAGS		flags;			//File flags
	uint32		i_osd1;			//OS dependent
	uint32		i_block[N_BLOCKS];	//Pointers to blocks
	uint32		i_version;		//File version (for NFS)
	uint32		i_file_acl;		//File ACL

	uint32		i_size_hi;	//i_dir_acl;	/Directory ACL in ext2
	uint32		i_faddr;	/* Obsoleted fragment address */
	union {
		struct {
			uint16	l_i_blocks_high; /* were l_i_reserved1 */
			uint16	l_i_file_acl_high;
			uint16	l_i_uid_high;	/* these 2 fields */
			uint16	l_i_gid_high;	/* were reserved2[0] */
			uint16	l_i_checksum_lo;/* crc32c(uuid+inum+inode) LE */
			uint16	l_i_reserved;
		} linux2;
		struct {
			uint16	h_i_reserved1;	/* Obsoleted fragment number/size which are removed in ext4 */
			uint16	h_i_mode_high;
			uint16	h_i_uid_high;
			uint16	h_i_gid_high;
			uint32	h_i_author;
		} hurd2;
		struct {
			uint16	h_i_reserved1;	/* Obsoleted fragment number/size which are removed in ext4 */
			uint16	m_i_file_acl_high;
			uint32	m_i_reserved2[2];
		} masix2;
		struct {
			uint8	i_frag;			//Fragment number
			uint8	i_fsize;		//Fragment size
			uint16	i_pad1;
			uint32	i_reserved2[2];
		} ext2;
	};
//0x080
	uint16		i_extra_isize;
	uint16		i_checksum_hi;	/* crc32c(uuid+inum+inode) BE */
	uint32		i_ctime_extra;	/* extra Change time	(nsec << 2 | epoch) */
	uint32		i_mtime_extra;	/* extra Modification time(nsec << 2 | epoch) */
	uint32		i_atime_extra;	/* extra Access time	(nsec << 2 | epoch) */
	uint32		i_crtime;		/* File Creation time */
	uint32		i_crtime_extra; /* extra FileCreationtime (nsec << 2 | epoch) */
	uint32		i_version_hi;	/* high 32 bits for 64-bit version */

	auto	size() const	{ return lo_hi(i_size, i_size_hi); }
};

enum INO {
	BAD_INO			= 1,	//Bad blocks inode
	ROOT_INO		= 2,	//Root inode
	ACL_IDX_INO		= 3,	//ACL inode
	ACL_DATA_INO	= 4,	//ACL inode
	BOOT_LOADER_INO	= 5,	//Boot loader inode
	UNDEL_DIR_INO	= 6,	//Undelete directory inode
	EXT4_RESIZE_INO	= 7,	//Reserved group descriptors inode
	EXT4_JOURNAL_INO= 8,	//Journal inode
	FIRST_INO		= 11,	//First non reserved inode
};

struct entry {
	enum TYPE : uint8 {
		UNKNOWN		= 0,
		REG_FILE	= 1,
		DIR			= 2,
		CHRDEV		= 3,
		BLKDEV		= 4,
		FIFO		= 5,
		SOCK		= 6,
		SYMLINK		= 7,
		MAX			= 8,
		DIR_CSUM	= 0xDE,
	};
	uint32	inode;		//address if inode
	uint16	rec_len;	//length of this record
	uint8	name_len;	//length of file name
	TYPE	type;
//	char	name[0];	//the file name
	
	auto	name()	const { return str((char*)(this + 1), name_len); }
	auto	next()	const { return (entry*)((uint8*)this + rec_len); }
};

struct extent_tail {
	uint32	et_checksum;    /* crc32c(uuid+inum+extent_block) */
};

struct extent {
	uint32	ee_block;   /* first logical block extent covers */
	uint16	ee_len;     /* number of blocks covered by extent */
	uint16	ee_start_hi;    /* high 16 bits of physical block */
	uint32	ee_start_lo;    /* low 32 bits of physical block */
	constexpr uint64	start()	const { return ee_start_lo | (uint64(ee_start_hi) << 32); }
};

struct extent_idx {
	uint32	ei_block;   /* index covers logical blocks from 'block' */
	uint32	ei_leaf_lo; /* pointer to the physical block of the next level. leaf or next index could be there */
	uint16	ei_leaf_hi; /* high 16 bits of physical block */
	uint16	ei_unused;
	constexpr uint64	child()	const { return ei_leaf_lo | (uint64(ei_leaf_hi) << 32); }
};

// Each block (leaves and indexes), even inode-stored has header.
struct extent_header {
	enum {MAGIC = 0xf30a};
	uint16	eh_magic;   /* probably will support different formats */
	uint16	eh_entries; /* number of valid entries */
	uint16	eh_max;     /* capacity of store in entries */
	uint16	eh_depth;   /* has tree real underlying blocks? */
	uint32	eh_generation;  /* generation of the tree */
	constexpr bool	valid() const { return eh_magic == MAGIC; }
	auto	entries()	const { return make_range_n((const extent*)(this + 1), eh_entries); }
	auto	nodes()		const { return make_range_n((const extent_idx*)(this + 1), eh_entries); }
};

class EXT : public superblock {
protected:
	struct group {
		blockgroup		*bg;
		malloc_block	block_bm_cache;
		malloc_block	inode_bm_cache;
		malloc_block	inodes_cache;

		group(blockgroup &bg) : bg(&bg) {}

		memory_block	block_bm(const EXT &ext4) {
			if (!block_bm_cache)
				block_bm_cache	= ext4.group_block_bm(ext4.groups.index_of(this), bg->block_bitmap());
			return block_bm_cache;
		}

		memory_block	inode_bm(const EXT &ext4) {
			if (!inode_bm_cache)
				inode_bm_cache = ext4.group_inode_bm(bg->inode_bitmap());
			return inode_bm_cache;
		}

		auto	inodes(const EXT &ext4) {
			if (!inodes_cache)
				inodes_cache = ext4.group_inodes(bg->inode_table());
			return make_strided<inode>(inodes_cache, ext4.s_inode_size);
		}

	};

	istream_ptr	file;
	uint32		block_size;

	malloc_block			bgs;
	dynamic_array<group>	groups;

	malloc_block	group_block_bm(int i, uint64 bm) const {
		auto	group_desc_size	= div_round_up(groups.size(), block_size);
		auto	block_bm_size	= div_round_up(div_round_up(s_blocks_per_group, 8), block_size);
		file.seek((s_first_data_block + i * s_blocks_per_group + group_desc_size + bm) * block_size);
		return malloc_block(file, block_bm_size * block_size);
	}

	malloc_block	group_inode_bm(uint64 bm) const {
		auto	inode_bm_size	= div_round_up(div_round_up(s_inodes_per_group, 8), block_size);
		file.seek((s_first_data_block + bm) * block_size);
		return malloc_block(file, inode_bm_size * block_size);
	}

	malloc_block	group_inodes(uint64 table) const {
		file.seek((s_first_data_block + table) * block_size);
		return malloc_block(file, s_inodes_per_group * s_inode_size);
	}

	template<int N>	int	ReadBlocks(void *dest, int bno) {
		file.seek((s_first_data_block + bno - 1) * block_size);
		return ReadBlocks<N>(dest, make_range<uint32>(malloc_block(file, block_size)));
	}

	template<>	int	ReadBlocks<0>(void *dest, int bno) {
		file.seek((s_first_data_block + bno - 1) * block_size);
		file.readbuff(dest, block_size);
		return 1;
	}

	template<int N>	int	ReadBlocks(void *dest, range<const uint32*> blocks) {
		int	n = 0;
		for (auto b : blocks) {
			if (!b)
				break;
			n += ReadBlocks<N - 1>((uint8*)dest + n * block_size, b);
		}
		return n;
	}


public:
	EXT(istream_ref _file) : file(_file.clone()) {
		file.seek(1024);
		if (!file.read(*(superblock*)this) || !valid())
			return;

		block_size		= 1024 << s_log_block_size;

		file.seek_cur(0x800);	//?

		uint32	num_groups		= div_round_up(s_blocks_count, s_blocks_per_group);
		uint32	group_stride	= feature_incompat & FEATURE_INCOMPAT::BIT64 ? 0x40 : 0x20;
		bgs.read(file, num_groups * group_stride);

		groups	= make_strided<blockgroup>(bgs, group_stride);
	}

	void			GetDataExtent(memory_block buffer, const extent_header *h);
	malloc_block	GetData(const inode &i);

	inode&			GetINode(uint32 ino) {
		--ino;
		uint32	g = ino / s_inodes_per_group;
		uint32	o = ino % s_inodes_per_group;
		return groups[g].inodes(*this)[o];
	}
};

void EXT::GetDataExtent(memory_block buffer, const extent_header* h) {
	ISO_ASSERT(h->valid());

	if (h->eh_depth == 0) {
		for (auto& e : h->entries()) {
			file.seek(e.start() * block_size);
			if (e.ee_len <= 0x8000)
				buffer.slice(e.ee_block * block_size, e.ee_len * block_size).read(file);
		}

	} else {
		for (auto& e : h->nodes()) {
			file.seek(e.child() * block_size);
			GetDataExtent(buffer.slice(e.ei_block * block_size), malloc_block(file, block_size));
		}
	}

}

malloc_block EXT::GetData(const inode &i) {
	malloc_block	buffer(i.size());

	if ((feature_incompat & FEATURE_INCOMPAT::EXTENTS) && (i.flags & inode::FLAGS::EXTENTS)) {
		GetDataExtent(buffer, (const extent_header*)i.i_block);

	} else {
		int	n = ReadBlocks<1>(buffer, make_range_n(i.i_block, NDIR_BLOCKS));

		if (uint32 b = i.i_block[IND_BLOCK])
			n += ReadBlocks<1>(buffer + n * block_size, b);

		if (uint32 b = i.i_block[DIND_BLOCK])
			n += ReadBlocks<2>(buffer + n * block_size, b);

		if (uint32 b = i.i_block[TIND_BLOCK])
			n += ReadBlocks<3>(buffer + n * block_size, b);
	}
	return buffer;
}


ISO_ptr<void> GetAll(EXT &ext2, uint32 ino, tag id, bool raw) {
	inode	&inode = ext2.GetINode(ino);

	switch (inode.i_mode & inode::IFMT) {
		case inode::IFDIR: {
			ISO_ptr<anything>	p(id);
			malloc_block	buffer	= ext2.GetData(inode);

			for (auto &e : make_next_range<entry>(buffer)) {
				if (e.rec_len == 0)
					break;
				if (e.inode == 0)
					continue;
				if (e.name() == "." || e.name() == "..")
					continue;
				p->Append(GetAll(ext2, e.inode, e.name(), raw));
			}

			return p;
		}

		case inode::IFREG:
			return ReadData1(id, memory_reader(ext2.GetData(inode)).me(), inode.i_size, raw);

		case inode::IFSOCK: {
			char	name[256];
			memcpy(name, inode.i_block, inode.i_size);
			name[inode.i_size] = 0;
			return ISO_ptr<string>(id, name);
		}

		case inode::IFBLK:
		case inode::IFCHR:
			return ISO_ptr<uint32>(id, inode.i_block[0]);

		default:
			return ISO_NULL;
	}
}

}//namespace EXT

struct malloc_block_deref : ISO::VirtualDefaults {
	void*	t;
	ISO::TypeArray	type;
	auto	Deref() const { return ISO::Browser(&type, t); }
	malloc_block_deref(malloc_block &&m) : t(m), type(ISO::getdef<xint8>(), m.size32(), 1) {
		m.p = nullptr;
	}
	~malloc_block_deref()	{ iso::free(t); }
};
ISO_DEFVIRT(malloc_block_deref);

typedef with_refs<EXT::EXT>	refEXT;

struct EXTfile : ISO::VirtualDefaults {
	ref_ptr<refEXT>	ext;
	uint32			ino;
	EXT::inode		inode;

	EXTfile(refEXT *ext, uint32 ino) : ext(ext), ino(ino) {
		inode = ext->GetINode(ino);
	};

	ISO::Browser2	Deref() {
		//auto	&inode = ext->GetINode(ino);
		return ISO::ptr<malloc_block_deref,64>(none, ext->GetData(inode));
	}
};

struct EXTdir : ISO::VirtualDefaults {
	ref_ptr<refEXT>	ext;
	uint32			ino;
	anything		entries;

	EXTdir(refEXT *ext, uint32 ino) : ext(ext), ino(ino) {};

	//uint32			Count()	{
	//	if (!entries)
	//		Init();
	//	return entries.size();
	//}

	ISO::Browser2	Deref() {
		if (!entries)
			Init();
		return ISO::MakeBrowser(entries);
	}


	void Init() {
		auto			&inode	= ext->GetINode(ino);
		malloc_block	buffer	= ext->GetData(inode);

		for (auto &e : make_next_range<EXT::entry>(buffer)) {
			if (e.rec_len == 0)
				break;
			if (e.inode == 0)
				continue;
			if (e.name() == "." || e.name() == "..")
				continue;
			switch (e.type) {
				case EXT::entry::DIR:
					entries.Append(ISO_ptr<EXTdir>(e.name(), ext, e.inode));
					break;

				case EXT::entry::REG_FILE:
					entries.Append(ISO_ptr<EXTfile>(e.name(), ext, e.inode));
					break;
			}
		}

	}
};

ISO_DEFUSERVIRTF(EXTfile, ISO::Virtual::DEFER);
ISO_DEFUSERVIRTXF(EXTdir, "Folder", ISO::Virtual::DEFER);

class EXTFileHandler : FileHandler {
	const char*		GetDescription() override { return "ext2/3/4 filesystem"; }

	int				Check(istream_ref file) override {
		if (file.length() < 1024 + sizeof(EXT::superblock))
			return CHECK_DEFINITE_NO;
		file.seek(1024);
		return file.get<EXT::superblock>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
	#if 1
		auto	ext4	= ref_ptr<refEXT>::make(file);
		if (ext4->valid())
			return ISO_ptr<EXTdir>(id, ext4, EXT::ROOT_INO);
		return ISO_NULL;
	#else
		EXT	ext4(file);
		return ext4.Valid() ? GetAll(ext4, ROOT_INO, id, WantRaw()) : ISO_NULL;
	#endif
	}
} ext;

//-----------------------------------------------------------------------------
//	Logical Volume Manager (LVM)
//-----------------------------------------------------------------------------

namespace LVM {
	//The checksum is calculated using weak CRC-32 (CRC without XOR with 0xffffffff) using the polynominal 0xedb88320 and initial value 0xf597a6cf.

	struct label_header {
		uint64		sig;//[8];	// "LABELONE"
		uint64		sector;		// The sector number of the physical volume label header
		uint32		checksum;	// CRC-32 for offset 20 to end of the physical volume label sector
		uint32		size;		// The offset, in bytes, relative from the start of the physical volume label header
		uint64		type;		// "LVM2 001"

		bool	is_valid() const { return sig == "LABELONE"_u64; }
	};

	struct area {
		uint64		offset;		// The offset, in bytes, relative from the start of the physical volume
		uint64		size;
		constexpr bool	is_terminator() const { return offset == 0; }
	};

	struct raw_locn {// 24 bytes
		enum FLAGS {
			IGNORED	= 1,
		};
		uint64		offset;		// The offset, in bytes, relative from the start of the metadata area
		uint64		size;
		uint32		checksum;
		uint32		flags;
	};

	struct mda_header {//512 bytes
		uint32		checksum;	// Checksum CRC-32 for offset 4 to end of the metadata area header
		char		sig[16];	// " LVM2 x[5A%r0N*>"
		uint32		version;
		uint64		offset;		// The offset, in bytes, of the metadata area relative from the start of the physical volume
		uint64		size;
		raw_locn	raw[4];		// List of raw location descriptors. The last descriptor in the list is terminator and consists of 0-byte values.
		uint8		unused[376];
		constexpr bool	is_terminator() const { return size == 0; }
	};

	struct pv_header {
		char		id[32];	//Physical volume identifier; contains a UUID stored as an ASCII string
		uint64		size;	//Physical volume size
	};

	struct metadata_item {
		enum TYPE {NONE, NIL, INT, STRING, ARRAY, OBJECT, NUM_TYPES} type;
		
		typedef	pair<string,metadata_item,0>	element;
		typedef compact_array<metadata_item>	array;
		typedef	compact_array<element>			object;

		union {
			int64	i;
			string	s;
			array	a;
			object	o;
		};

		template<typename T, typename = void> struct get_s {
			static T f(const metadata_item *p) {
				return *p;
			}
		};

		static metadata_item	none;
		static void				read_object(text_reader<reader_intf> &r, object &o, char term);
		static metadata_item	read_atom(text_reader<reader_intf> &r);

		constexpr metadata_item()					: type(NONE),	i(0)	{}
		constexpr metadata_item(const _none&)		: type(NONE),	i(0)	{}
		constexpr metadata_item(TYPE t)				: type(t),		i(0)	{}
		constexpr metadata_item(int64 i)			: type(INT),	i(i)	{}
		constexpr metadata_item(metadata_item&&	j)	: type(j.type),	i(j.i)	{ j.type = NIL; }
		constexpr metadata_item(string&& s)			: type(STRING),	s(move(s))	{}
		metadata_item(array&&	a)	: type(ARRAY),	a(move(a))	{}
		metadata_item(object&&	o)	: type(OBJECT),	o(move(o))	{}

		~metadata_item() {
			switch (type) {
				case STRING:	s.~string(); 	break;
				case ARRAY:		a.~array();		break;
				case OBJECT:	o.~object(); 	break;
				default:						break;
			}
		}

		explicit 	operator bool()				const	{ return type != NONE; }

		metadata_item&	operator[](int i)		const	{
			return	type == ARRAY	&& i < a.size() ? a[i]
				:	type == OBJECT	&& i < o.size()	? o[i].b
				:	none;
		}
		metadata_item&	operator[](const metadata_item &i)	const	{
			return	i.type == INT		? operator[](i.i)
				:	i.type == STRING	? operator/(i.s)
				:	none;
		}
		metadata_item&	operator/(const char *s)	const	{
			if (type == metadata_item::OBJECT) {
				for (auto &i : o) {
					if (i.a == s)
						return i.b;
				}
			}
			return none;
		}
		metadata_item&	operator/(const metadata_item &i)	const	{ return operator[](i); }
		metadata_item&	operator/(int i)					const	{ return operator[](i); }

		const char*	get_name(int i)				const	{ return type == OBJECT ? o[i].a : 0; }
		template<typename T> optional<T> get()	const	{ return get_s<T>::f(this); }
		template<typename T> T	get(T def)		const	{ return get<T>().or_default(def); }
		getter<const metadata_item>	get()		const	{ return *this; }
		size_t		size()						const	{ return type == ARRAY ? a.size() : type == OBJECT ? o.size() : 0; }
		auto		begin()						const	{ return make_indexed_iterator(*this, make_int_iterator(0)); }
		auto		end()						const	{ return make_indexed_iterator(*this, make_int_iterator((int)size())); }
		range<element*>	items()					const	{ if (type == metadata_item::OBJECT) return {o.begin(), o.end()}; return {}; }

		template<typename T>	operator T()	const	{ return get(T()); }

		bool	read(text_reader<reader_intf> &r) {
			type = OBJECT;
			read_object(r, o, 0);
			return true;
		}
		bool	read(istream_ref r)		{
			return read(lvalue(text_reader<reader_intf>(r)));
		}

	};

	metadata_item	metadata_item::none;

	template<> force_inline optional<const char*> metadata_item::get<const char*>() const {
		if (type == STRING)
			return s.begin();
		return {};
	}

	template<> force_inline optional<string> metadata_item::get<string>() const {
		if (type == STRING)
			return s;
		return {};
	}

	template<typename T> struct metadata_item::get_s<T, enable_if_t<is_num<T>>> {
		static optional<T> f(const metadata_item *p) {
			if (p->type == metadata_item::INT)
				return T(p->i);
			return {};
		}
	};

	template<typename T> struct metadata_item::get_s<dynamic_array<T>> {
		static optional<dynamic_array<T>> f(const metadata_item *p) {
			if (p->type == metadata_item::ARRAY) {
				dynamic_array<T>	a;
				for (auto &i : p->a)
					a.push_back(i.get<T>());
				return a;
			}
			return {};
		}
	};

	template<typename T, int N> struct metadata_item::get_s<iso::array<T, N>> {
		static optional<iso::array<T, N>> f(const metadata_item* p) {
			if (p->type == metadata_item::ARRAY && p->a.size() == N) {
				iso::array<T, N>	a;
				for (int i = 0; i < N; i++)
					a[i] = p->a[i].get<T>();
				return a;
			}
			return {};
		}
	};

	void	metadata_item::read_object(text_reader<reader_intf> &r, object &o, char term) {
		int	c = skip_whitespace(r);
		while (c != term) {
			if (c == '#') {
				c = skip_whitespace(r, skip_chars(r, ~char_set('\n')));
				continue;
			}

			auto name = read_token(r, char_set::wordchar, c);
			c = skip_whitespace(r);

			if (c == '=') {
				o.emplace_back(name, read_atom(r));
			} else if (c == '{') {
				read_object(r, o.emplace_back(name, OBJECT).b.o, '}');
			}
			c = skip_whitespace(r);
		}
	}

	metadata_item metadata_item::read_atom(text_reader<reader_intf> &r) {
		int	c = skip_whitespace(r);
		while (c == '#')
			c = skip_whitespace(r, skip_chars(r, ~char_set('\n')));

		switch (c) {
			case '[': {
				metadata_item::array	a;
				c = skip_whitespace(r);
				while (c != ']') {
					r.put_back(c);
					a.push_back(read_atom(r));
					c = skip_whitespace(r);
					if (c == ',')
						c = skip_whitespace(r);
				}
				return a;
			}

			case '"': {
				string_builder	b;
				for (; (c = r.getc()) != '"';) {
					if (c == '\\') {
						char32	c0 = get_escape(r);
						if (between(c0, 0xd800, 0xdfff)) {
							if ((c = r.getc()) == '\\') {
								c = get_escape(r);
								if (auto c2 = from_utf16_surrogates(c0, c)) {
									b << c2;
									continue;
								}
							}
							b << c0 << c;
						} else {
							b << c0;
						}
					} else {
						b.putc(c);
					}
				}
				return string(b.term());
			}

			default:
				return read_integer(r, c);
		}
		return {};
	}

	enum STATUS {
		ALLOCATABLE	= 1 << 0,	// Is allocatable [physical volume only]
		RESIZEABLE	= 1 << 1,	// Can be re-sized [volume group only]
		READ		= 1 << 2,	// Can be read
		VISIBLE		= 1 << 3,	// Is visible [logical volume only]
		WRITE		= 1 << 4,	// Can be written
	};
	enum FLAGS {
		FLAGS0		= 0,
	};

	struct physical_volume {
		string	id;
		string	device;
		STATUS	status;
		FLAGS	flags;
		uint64	dev_size;
		uint64	pe_start;
		uint64	pe_count;

		physical_volume(const metadata_item& m) : id((const char*)(m/"id")), device((const char*)(m/"device")), pe_start(m/"pe_start"), pe_count(m/"pe_count") {}
	};

	struct segment {
		enum TYPE {
			cache,
			cache_pool,
			error,
			free,
			linear,
			mirror,
			raid0,
			raid0_meta,
			raid1,
			raid10,
			raid10_near,
			raid4,
			raid5,
			raid5_la,
			raid5_ls,
			raid5_n,
			raid5_ra,
			raid5_rs,
			raid6,
			raid6_la_6,
			raid6_n_6,
			raid6_nc,
			raid6_nr,
			raid6_ra_6,
			raid6_rs_6,
			raid6_zr,
			snapshot,
			striped,
			thin,
			thin_pool,
			vdo,
			vdo_pool,
			writecache,
			zero,
		};
		struct stripe {
			physical_volume	*volume	= nullptr;
			uint64			start_extent;
			stripe(const metadata_item& m) : start_extent(m[1]) {}
		};

		TYPE	type;
		uint64	start_extent;
		uint64	extent_count;
		stripe	stripes;

		segment(const metadata_item& m) : start_extent(m/"start_extent"), extent_count(m/"extent_count"), stripes(m/"stripes") {
			type = TYPE(which(0, str((m/"type").get("")),
				"cache", "cache_pool", "error", "free", "linear", "mirror", "raid0", "raid0_meta", "raid1", "raid10", "raid10_near", "raid4", "raid5", "raid5_la", "raid5_ls", "raid5_n", "raid5_ra", "raid5_rs",
				"raid6", "raid6_la_6", "raid6_n_6", "raid6_nc", "raid6_nr", "raid6_ra_6", "raid6_rs_6", "raid6_zr", "snapshot", "striped", "thin", "thin_pool", "vdo", "vdo_pool", "writecache", "zero"
			));
		}

	};

	struct logical_volume {
		string	id;
		STATUS	status;
		FLAGS	flags;
		string	creation_host;
		uint64	creation_time;
		uint64	read_ahead;
		dynamic_array<segment>	segments;

		logical_volume(const metadata_item& m) : id((const char*)(m/"id")), creation_host((const char*)(m/"creation_host")), creation_time(m/"creation_time"), read_ahead(m/"read_ahead") {
			for (int i = 1;;++i) {
				auto&	ms = m / format_string("segment%i", i);
				if (!ms)
					break;
				segments.emplace_back(ms);
			}
		}

	};

	struct volume_group {
		string	id;
		uint32	seqno;
		STATUS	status;
		FLAGS	flags;
		dynamic_array<string>	tags;
		uint64	extent_size;
		uint64	max_lv			= 0;
		uint64	max_pv			= 0;
		uint64	metadata_copies	= 0;

		dynamic_array<physical_volume>	physical_volumes;
		dynamic_array<logical_volume>	logical_volumes;

		volume_group(const metadata_item& m) : id((const char*)(m/"id")), seqno(m/"seqno"), extent_size(m/"extent_size"),
		//, tags(m / "tags") {}
			physical_volumes(m/"physical_volumes"),
			logical_volumes(m/"logical_volumes")
		{}
	};
}

ISO_DEFUSERCOMPV(LVM::segment::stripe, 
	start_extent
);

ISO_DEFUSERCOMPV(LVM::segment, 
//	type,
	start_extent,
	extent_count,
	stripes
);

ISO_DEFUSERCOMPV(LVM::physical_volume, 
	id,
	device,
//	status,
//	flags,
	dev_size,
	pe_start,
	pe_count
);
ISO_DEFUSERCOMPV(LVM::logical_volume, 
	id,
//	status,
//	flags,
	creation_host,
	creation_time,
	read_ahead,
	segments
);


ISO_DEFUSERCOMPV(LVM::volume_group, 
	id,
	seqno,
//	status,
//	flags,
//	tags,
	extent_size,
//	max_lv,
//	max_pv,
//	metadata_copies,
	physical_volumes,
	logical_volumes
);

template<typename S> struct reader_defaults : stream_defaults<S> {
	auto	get_block(size_t size)		{ return malloc_block(*static_cast<S*>(this), size); }
};


class LVMStream : public reader_defaults<LVMStream> {
	istream_ptr		file;
	streamptr		offset;
	streamptr		ptr, end;
public:
	LVMStream(istream_ref file, const LVM::volume_group &group, const LVM::segment &seg) : file(file.clone()), ptr(0) {
		offset	= (group.physical_volumes[0].pe_start + (seg.stripes.start_extent + seg.start_extent) * group.extent_size) * 512;
		end		= seg.extent_count * group.extent_size * 512;
	}
	LVMStream(const LVMStream &b) : file(b.file.clone()), offset(b.offset), ptr(0), end(b.end) {}

	size_t		readbuff(void *buffer, size_t size) {
		file.seek(offset + ptr);
		return file.readbuff(buffer, size);
	}
	void		seek(streamptr offset)	{ ptr = offset;}
	streamptr	tell()					{ return ptr; }
	streamptr	length()				{ return end; }
	int			getc()					{ ISO_ASSERT(0); return EOF; }
};

struct ISO_LVM : LVM::volume_group {
	dynamic_array<BigBin>	logical_sectors;

	ISO_LVM(istream_ref file, const LVM::metadata_item& m) : LVM::volume_group(m) {
		for (auto &i : logical_volumes)
			logical_sectors.emplace_back(LVMStream(file, *this, i.segments[0]));
	}
};

ISO_DEFUSERCOMPBV(ISO_LVM, LVM::volume_group, logical_sectors);

class LVMFileHandler : FileHandler {
	const char*		GetDescription() override { return "Logical Volume Manager (LVM)"; }

	int				Check(istream_ref file) override {
		if (file.length() < 1024)
			return CHECK_DEFINITE_NO;
		file.seek(512);
		return file.get<LVM::label_header>().is_valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		file.seek(512);
		auto	header = file.get<LVM::label_header>();
		file.seek(512 + header.size);

		auto	pv		= file.get<LVM::pv_header>();

		dynamic_array<LVM::area>	data_areas;
		dynamic_array<LVM::area>	meta_areas;

		LVM::area	area;
		while (file.read(area) && !area.is_terminator())
			data_areas.push_back(area);

		while (file.read(area) && !area.is_terminator())
			meta_areas.push_back(area);

		for (auto& i : meta_areas) {
			file.seek(i.offset);
			auto	mda = file.get<LVM::mda_header>();
			for (auto& j : mda.raw) {
				ISO_OUTPUTF("raw locn 0x") << hex(j.offset) << ":0x" << hex(j.size) << '\n';
				if (j.size) {
					file.seek(i.offset + j.offset);

					LVM::metadata_item	val	= file.get();

					auto&	vg2			= val/"vg2";

					return ISO_ptr<ISO_LVM>(id, file, vg2);

					return ISO::MakePtr<LVM::volume_group>(id, vg2);

					uint32	extent_size = (vg2/"extent_size").get();
					LVM::physical_volume	pv((vg2/"physical_volumes")[0]);
					dynamic_array<LVM::logical_volume>	logical(vg2/"logical_volumes");

				}

			}
		}

		return ISO_NULL;
	}
} lvm;

