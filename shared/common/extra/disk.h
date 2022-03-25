#ifndef DISK_H
#define DISK_H

#include "base/defs.h"

namespace iso {

union CHS {
	struct { uint8	head, sector:6; };
	struct Cylinder {
		uint8	bytes[3];
		operator uint16() const			{ return bytes[2] | ((bytes[1] & 0xc0) << 2); }
		Cylinder &operator=(uint16 c)	{ bytes[1] = c; bytes[2] = (bytes[2] & 0x3f) | ((c >> 2) & 0xc0); return *this; }
	} cylinder;
};

#pragma pack(1)
struct MBR {
	enum TYPE {
		EMPTY				= 0x00,		FAT12				= 0x01,		XENIX_ROOT			= 0x02,		XENIX_USR			= 0x03,
		FAT16				= 0x04,		DOS_EXT				= 0x05,		FAT16B				= 0x06,		NTFS				= 0x07,
		FAT32_CHS			= 0x0B,		FAT32_LBA			= 0x0C,		FAT16_LBA			= 0x0E,		WIN98_EXT			= 0x0F,
		OPUS				= 0x10,		FAT12_HIDDEN		= 0x11,		FAT16_HIDDEN		= 0x14,		DOS_EXT_HIDDEN		= 0x15,
		FAT16B_HIDDEN		= 0x16,		NTFS_HIDDEN			= 0x17,		HIBERNATION			= 0x18,		FAT32_CHS_HIDDEN	= 0x1B,
		FAT32_LBA_HIDDEN	= 0x1C,		FAT16_LBA_HIDDEN	= 0x1E,		WIN98_EXT_HIDDEN	= 0x1F,
		HP_SPEEDSTOR		= 0x21,		MS_MOBILE			= 0x25,		RECOVERY			= 0x27,		ATHEOS				= 0x2A,
		SYLLABLEOS			= 0x2B,
		NOS					= 0x32,		JFS					= 0x35,		THEOS				= 0x38,		PLAN9				= 0x39,
		THEOS_4G			= 0x3A,		THEOS_EXT			= 0x3B,		PARTITIONMAGIC		= 0x3C,		NETWARE_HIDDEN		= 0x3D,
		OS_32				= 0x3F,
		VENIX				= 0x40,		QNX_PRIMARY			= 0x4D,		QNX_SECONDARY		= 0x4E,		QNX_TERTIARY		= 0x4F,
		DM4					= 0x50,		DM6_AUX1			= 0x51,		CP_M				= 0x52,		DM6_AUX3			= 0x53,
		DM6					= 0x54,		EZD					= 0x55,		EDISK				= 0x5C,		APTI				= 0x5D,
		APTI_WIN98_EXT		= 0x5E,		APTI_DOS_EXT		= 0x5F,		SPEEDSTOR			= 0x61,		UNIXWARE			= 0x63,
		NETWARE_286			= 0x64,		NETWARE_386			= 0x65,
		APTI_FAT12			= 0x72,		IBM_PCIX			= 0x75,		NOVELL				= 0x77,		APTI_FAT16_CHS		= 0x79,
		APTI_FAT16_LBA		= 0x7A,		APTI_FAT16B			= 0x7B,		APTI_FAT32_LBA		= 0x7C,		APTI_FAT32_CHS		= 0x7D,
		FIX					= 0x7E,		AODPS				= 0x7F,
		MINIX				= 0x81,		LINUX_SWAP			= 0x82,		LINUX_DATA			= 0x83,		APM_HIBERNATION		= 0x84,
		LINUX_EXTENDED		= 0x85,		FAT16B_MIRROR		= 0x86,		NTFS_MIRROR			= 0x87,		PARTITION_TEXT		= 0x88,
		LINUX_KERNEL		= 0x8A,		FAT32_CHS_MIRROR	= 0x8B,		FAT32_LBA_MIRROR	= 0x8C,		FAT12_MIRROR		= 0x8D,
		LINUX_LVM			= 0x8E,
		FAT16_MIRROR		= 0x90,		DOS_EXT_MIRROR		= 0x91,		FAT16B_MIRROR2		= 0x92,		FAT32_CHS_HIDDEN2	= 0x97,
		FAT32_LBA_HIDDEN2	= 0x98,		FAT16_LBA_HIDDEN2	= 0x9A,		WIN98_EXT_HIDDEN2	= 0x9B,
		FREEBSD				= 0xA5,		OPENBSD				= 0xA6,		NEXT				= 0xA7,		DARWIN				= 0xA8,
		NETBSD				= 0xA9,		DARWIN_BOOT			= 0xAB,		ADFS				= 0xAD,		HFS					= 0xAF,
		FAT16B_MIRROR3		= 0xB6,		BSDI				= 0xB7,		FAT32_CHS_MIRROR2	= 0xBB,		FAT32_LBA_MIRROR2	= 0xBC,
		SOLARIS_BOOT		= 0xBE,		NEW_SOLARIS_X86		= 0xBF,
		DRDOS_FAT			= 0xC0,		DRDOS_FAT12			= 0xC1,		FAT16_SECURE		= 0xC4,		DOS_EXT_SECURE		= 0xC5,
		FAT16B_SECURE		= 0xC6,		NTFS_SECURE			= 0xC7,		FAT32_CHS_SECURE	= 0xCB,		FAT32_LBA_SECURE	= 0xCC,
		FAT16_LBA_SECURE	= 0xCE,		WIN98_EXT_SECURE	= 0xCF,
		DELL_RESTORE		= 0xDB,		DELL_DIAG			= 0xDE,
		BEOS				= 0xEB,		SKYOS				= 0xEC,		GPT_PROTECT			= 0xEE,		EFI					= 0xEF,
		LINUX_RAID			= 0xFD,
	};
	uint8		code[0x1b8];
	uint32		disk_sig;
	uint16		pad;
	struct Partition {
		uint8	flags;
		CHS		CHS_start;
		uint8	type;
		CHS		CHS_end;
		uint32	LBA_start;
		uint32	LBA_length;
	}			part[4];
	uint16		mbr_sig;	//0xaa55
};
#pragma pack()

struct PartitionTableHeader {
	uint64	Signature;			// "EFI PART"
	uint32	Revision;			// (for GPT version 1.0 (through at least UEFI version 2.3.1), the value is 00h 00h 01h 00h)
	uint32	HeaderSize;			// in little endian (in bytes, usually 5Ch 00h 00h 00h or 92 bytes)
	uint32	HeaderCRC;			// CRC32 of header (offset +0 up to header size), with this field zeroed during calculation
	uint32	Reserved;			// must be zero
	uint64	CurrentLBA;			// (location of this header copy)
	uint64	BackupLBA;			// (location of the other header copy)
	uint64	FirstUsableLBA;		// for partitions (primary partition table last LBA + 1)
	uint64	LastUsableLBA;		// (secondary partition table first LBA - 1)
	GUID	DiskGUID;			// (also referred as UUID on UNIXes)
	uint64	EntriesLBA;			// of array of partition entries (always 2 in primary copy)
	uint32	NumberPartitions;	// of partition entries in array
	uint32	PartitionSize;		// of a single partition entry (usually 80h or 128)
	uint32	PartitionsCRC32;	// of partition array;	//
	// must be zeroes for the rest of the block (420 bytes for a sector size of 512 bytes; but can be more with larger sector sizes
};

struct PartitionEntry {
	enum FlagBit {
		System		= 0,	// (disk partitioning utilities must preserve the partition as is)
		Ignore		= 1,	// EFI firmware should ignore the content of the partition and not try to read from it
		Legacy		= 2,	// Legacy BIOS bootable (equivalent to active flag (typically bit 7 set) at offset +0h in partition entries of the MBR partition table)[9]
		ReadOnly	= 60,
		Hidden		= 62,
		NoAutomount	= 63,	// (i.e., do not assign drive letter)
	};
	GUID	Type;
	GUID	ID;
	uint64	FirstLBA;
	uint64	LastLBA;	// (inclusive, usually odd)
	uint64	Flags;
	char16	Name[36];	// (36 UTF-16LE code units)
};

}//namespace iso
#endif //DISK_H
