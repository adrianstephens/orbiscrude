#include "base/defs.h"
#include "base/strings.h"
#include "base/array.h"
#include "base/algorithm.h"
#include "stream.h"
#include "crc32.h"
#include "windows/win_file.h"

using namespace iso;

#define	VOL_FILE	1
#define	VOL_RDONLY	2
#define	VOL_SWAB	4
#define	VOL_DIRTY	8
#define	VOL_NONINIT	16
#define	VOL_VALID	32

// Size that TiVo rounds the partitions down to whole increments of.
#define	MFS_PARTITION_ROUND		1024

#define	SABLOCKSEC				1630000

#define	TIVO_BOOT_MAGIC         0x1492
#define	TIVO_BOOT_AMIGC         0x9214

// Prime number used in hash for finding base inode of fsid.
#define	MFS_FSID_HASH			0x106d9

//	For	inode_flags	below.
#define	INODE_CHAINED			0x80000000	// I have no idea what this really is.

struct bitmap_header : bigendian_types {
	uint64			nbits;		// Number of bits in this map
	uint64			freeblocks;	// Number of free blocks in this map
	uint64			last;		// Last bit set ???
	uint64			nints;		// Number of ints in this map
};
// Size of each bitmap is (nints + (nbits < 8? 1: 2)) * 4
// In bitmap, MSB is first, LSB last

struct zone_map_ptr : bigendian_types {
	uint64			sector;		// Sector of next table
	uint64			sbackup;	// Sector of backup of next table
	uint64			length;		// Length of next table in sectors
	uint64			size;		// Size of partition of next table
	uint64			min;		// Minimum allocation of next table
};

struct zone_header : bigendian_types {
	enum zone_type {
		ztInode			= 0,
		ztApplication 	= 1,
		ztMedia 		= 2,
		ztMax 			= 3,
		ztPad 			= 0xffffffff
	};
	uint64			sector;		// Sector of this table
	uint64			backup;		// Sector of backup of this table
	uint64			next_sector;
	uint64			next_backup;
	uint64			next_zone_size;
	uint64			zone_first;
	uint64			zone_last;
	uint64			zone_size;
	uint64			free;
	uint32			next_size;
	uint32			size;
	uint32			min_chunk;
	uint32			next_min_chunk;
	uint32			logstamp;	// Last log stamp
	uint32			type;
	uint32			checksum;	// Checksum of ???
	uint32			_64;		//00000000
	uint32			num_bitmap;	//Followed by num addresses, pointing to mmapped memory from /tmp/fsmem for bitmaps
};

struct volume_header : bigendian_types {
	uint32			_00;
	uint32			abbafeed;
	uint32			checksum;
	uint32			_0c;
	uint32			root_fsid;		// Maybe?
	uint32			_14;
	uint32			_18;
	uint32			_1c;
	uint32			_20;
	char			partitionlist[128];
	uint32			_a4;
	uint32			_a8;
	uint32			total_sectors;
	uint32			_b0;
	uint32			logstart;
	uint32			lognsectors;
	uint32			logstamp;
	uint32			_c0;
	uint32			_c4;
	uint32			_c8;
	uint32			_cc;
	zone_map_ptr	zonemap;
	uint32			_f4;
	uint32			_f8;
	uint32			_fc;
};

//	Format	of	mac	partition table.
struct mac_partition : bigendian_types {
	enum {MAGIC = 0x504d};
	uint16		signature;
	uint16		res1;
	uint32		map_count;
	uint32		start_block;
	uint32		block_count;
	char		name[32];
	char		type[32];
	uint32		data_start;
	uint32		data_count;
	uint32		status;
};

enum fsid_type {
	tyNone	 = 0,
	tyFile	 = 1,
	tyStream = 2,
	tyDir	 = 4,
	tyDb	 = 8,
};

struct mfs_inode : bigendian_types {
	uint32		fsid;			// This FSID
	uint32		refcount;		// References to this FSID
	uint32		unk1;
	uint32		unk2;			// Seems to be related to last ?transaction? block used
	uint32		inode;			// Should be *sectornum - 1122) / 2
	uint32		unk3;			// Also block size?
	uint32		size;			// In bytes or blocksize sized blocks
	uint32		blocksize;
	uint32		blockused;
	uint32		lastmodified;	// In seconds since epoch
	uint8		type;			// For files not referenced by filesystem
	uint8		unk6;			// Always 8?
	uint16		beef;			// Placeholder
	uint32		sig;			// Seems to be 0x91231ebc
	uint32		checksum;
	uint32		inode_flags;	// It seems to be flags at least.
	uint32		numblocks;		// Number of data blocks.  0 = in this sector
	struct {
		uint32	sector;
		uint32	count;
	} datablocks[0];
};

struct fs_entry : bigendian_types	{
	uint32		fsid;
	uint8		entry_length;
	uint8		type;
	uint8		name[0];
};

uint32 mfs_compute_crc(void *data, uint32 size, uint32 off) {
	static const uint8 deadfood[] = { 0xde, 0xad, 0xf0, 0x0d };
	crc32	crc(~0);
	crc.write(memory_block(data, off), deadfood, memory_block((uint8*)data + off + 4, size - off - 4));
	return ~crc;
}
bool mfs_check_crc(void *data, uint32 size, uint32 &crc) {
	return crc == mfs_compute_crc(data, size, (uint8*)&crc - (uint8*)data);
}
void mfs_update_crc(void *data, uint32 size, uint32 &crc) {
	crc = mfs_compute_crc(data, size, (uint8*)&crc - (uint8*)data);
}

struct MFS {
	string			hda, hdb;

	struct volume {
		uint32		start;
		uint32		sectors;

		volume(uint32 _start, uint32 _sectors) : start(_start), sectors(_sectors & ~(MFS_PARTITION_ROUND - 1)) {}
	};

	struct zone_map	{
		zone_header		*map;
		zone_map		*next;
		zone_map		*next_loaded;
	};

	struct zone_map_head {
		uint32			size;
		uint32			free;
		zone_map		*next;
	};

	dynamic_array<volume>	volumes;

	//	TiVo partition map information
	struct tivo_partition_table {
		int						vol_flags;
		int						allocated;
		int						refs;
		uint32					devsize;
		dynamic_array<mac_partition>	partitions;
		tivo_partition_table	*next;
		tivo_partition_table	*parent;
	};

	tivo_partition_table	*partition_tables;


	uint32	get_sector(uint32 s) {
		for (volume *i = volumes.begin(), *e = volumes.end(); i != e; ++i) {
			if (s < i->sectors)
				return s + i->start;
			s -= i->sectors;
		}
		return 0;
	}

	bool	add_drive(win_filereader &r) {
		uint8			sector[512];
		r.readbuff(sector, 512);

		// Find out what the magic is in the bootblock.
		bool	swap	= false;
		switch (*(uint16be*)sector) {
			case TIVO_BOOT_MAGIC:
				break;
			case TIVO_BOOT_AMIGC:
				swap = true;
				break;
			default:
				return false;
		}
		tivo_partition_table*	table	= new tivo_partition_table;
		int						maxsec	= 1;
		for (int cursec = 1; cursec <= maxsec; cursec++) {
			r.seek(uint64(cursec) * 512);
			r.readbuff(sector, 512);

			if (swap)
				copy_n((uint16*)sector, (uint16be*)sector, 256);

			mac_partition *part = (mac_partition*)sector;
			if (part->signature != mac_partition::MAGIC)
				break;

			if (cursec == 1)
				maxsec = part->map_count;

			table->partitions.push_back(*part);
		}

		table->allocated	= table->partitions[0].start_block;
		table->next			= partition_tables;
		partition_tables	= table;
		return true;
	}

	bool	add_super(win_filereader &r, uint32 start) {
		uint8			sector[512];

		r.seek(start * uint64(512));
		r.readbuff(sector, 512);
		volume_header	vh = *(volume_header*)sector;

		// Verify the checksum.
		//if (!mfs_check_crc(vol, sizeof(volume_header), vol->checksum))
		//	return false;

		// Load the partition list from MFS.
		uint32	total = 0;
		string_scan	ss(vh.partitionlist);
		for (;;) {
			count_string tok = ss.get_token();
			if (!tok.begins("/dev/hd"))
				break;
			char	drive	= tok.begin()[7];
			int		vol;
			from_string(tok.slice(8).begin(), vol);

			mac_partition	&part	= partition_tables->partitions[vol - 1];
			uint32			sectors	= part.block_count & ~(MFS_PARTITION_ROUND - 1);
			total			+= sectors;
			new (volumes) volume(part.start_block, sectors);
		}

		// If the sectors mismatch, report it.. But continue anyway.
		if (total != vh.total_sectors)
			ISO_TRACEF("Volume size (%u) mismatch with reported size (%u)", total, (uint32)vh.total_sectors);

		for (uint64 zone_sec = vh.zonemap.sector; zone_sec;) {
			uint32		sec		= get_sector(zone_sec);
			r.seek(uint64(sec) * 512);
			r.readbuff(sector, 512);
			zone_header	*zone	= (zone_header*)sector;

			ISO_TRACEF("Zone map:") << "type=" << zone->type << '\n'
				<<	" true loc="	<< hex(uint64(sec) * 512) << '\n'
				<<	" sector="		<< zone->sector
				<<	" size="		<< zone->size
				<<	" backup="		<< zone->backup
				<<	'\n'
				<<	" next_sector="	<< zone->next_sector
				<<	" next_size="	<< zone->next_size
				<<	" next_backup="	<< zone->next_backup
				<<	'\n'
				<<	" zone_first="	<< zone->zone_first
				<<	" zone_last="	<< zone->zone_last
				<<	" zone_size="	<< zone->zone_size
				<<	" min_chunk="	<< zone->min_chunk
				<<	'\n'
				<<	" free="		<< zone->free
				<<	" checksum="	<< hex(zone->checksum)
				<<	" logstamp="	<< zone->logstamp
				<<	" num_bitmap="	<< zone->num_bitmap
				<<	'\n';

			zone_sec	= zone->next_sector;
		}

		return true;
	}

	MFS() : partition_tables(0) {
		win_filereader r(CreateFileA("\\\\.\\physicaldrive1", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0));
		add_drive(r);

		for (mac_partition *i = partition_tables->partitions.begin(), *e = partition_tables->partitions.end(); i != e; ++i) {
			if (i->name == cstr("MFS application region"))
				add_super(r, i->start_block);
		}

	}

};// test_mfs;
