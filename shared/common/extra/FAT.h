#ifndef FAT_H
#define FAT_H

#include "base/defs.h"

namespace iso { namespace FAT {

enum TYPE {
	FAT12,
	FAT16,
	FAT32,
};

enum VALUE {				//FAT12  FAT16 FAT32
	FREE		= 0,		//0x000 0x0000 0x0000000	Free.
	ALLOC		= 2,		//0x002..MAX				Allocated. Value of the entry is the cluster number of the next cluster following this corresponding cluster. MAX is the Maximum Valid Cluster Number
	DEFECTIVE	= -9,		//0xFF7 0xFFF7 0xFFFFFF7	Defective cluster
	RESERVED	= -8,		//0xFF8 0xFFF8 0xFFFFFF8	Reserved and should not be used. May be interpreted as an allocated cluster and the final cluster in the file (indicating end-of-file condition)
	FINAL		= -1,		//0xFFF 0xFFFF 0xFFFFFFFF	Cluster is allocated and is the final cluster for the file (indicates end-of-file)
};

enum ENTRIES {
	MEDIA		= 0,	// BPB_Media byte value in its low 8 bits, and all other bits are set to 1
	EOC			= 1,	// high two bits are dirty volume flags
};
//For FAT16:	ClnShutBitMask = 0x8000;		HrdErrBitMask = 0x4000;  
//For FAT32:	ClnShutBitMask = 0x08000000;	HrdErrBitMask = 0x04000000; 

#pragma pack(1)

struct Entry {
	struct Date {
		uint16	day:5, month:4, year:7;
	};
	struct Time {
		uint16	seconds2:5, mins:6, hours:5;
	};
	enum ATTR {
		ATTR_READ_ONLY	= 0x01,
		ATTR_HIDDEN		= 0x02,
		ATTR_SYSTEM		= 0x04,
		ATTR_VOLUME_ID	= 0x08,		//Only the root directory (see Section 6.x below) can contain one entry with this attribute
		ATTR_DIRECTORY	= 0x10,
		ATTR_ARCHIVE	= 0x20,
		ATTR_LONG_NAME	= ATTR_READ_ONLY | ATTR_HIDDEN |  ATTR_SYSTEM | ATTR_VOLUME_ID,
	};
	char	Name[11];		//0		11	"Short" file name limited to 11 characters (8.3 format)  
	uint8	Attr;			//11	1
	uint8	NTRes;			//12	1	Reserved. Must be set to 0.  
	uint8	CrtTimeTenth;	//13	1	Component of the file creation time. Count of tenths of a second. Valid range is:  0 <= CrtTimeTenth <= 199  
	Time	CrtTime;		//14	2	Creation time
	Date	CrtDate;		//16	2	Creation date
	Date	LstAccDate;		//18	2	Last access date
	uint16	FstClusHI;		//20	2	High word of first data cluster number for file/directory described by this entry.  Only valid for volumes formatted FAT32. Must be set to 0 on volumes formatted FAT12/FAT16
	Time	WrtTime;		//22	2	Last modification (write) time. Value must be equal to CrtTime at file creation
	Date	WrtDate;		//24	2	Last modification (write) date. Value must be equal to CrtDate at file creation
	uint16	FstClusLO;		//26	2	Low word of first data cluster number for file/directory described by this entry 
	uint32	FileSize;		//28	4	32-bit quantity containing size in bytes of file/directory described by this entry

	bool	end()			const { return Name[0] == 0; }
	bool	used()			const { return Name[0] && Name[0] != char(0xe5); }
	bool	is_dir()		const { return !!(Attr & ATTR_DIRECTORY); }
	bool	is_longname()	const { return Attr == ATTR_LONG_NAME; }

	fixed_string<13> name() const {
		fixed_string<13> n;
		char	*d = n;
		for (const char *s = Name, *e = s + 8; s < e && *s != ' '; ++s)
			*d++ = *s;
		if (Name[8] != ' ') {
			*d++ = '.';
			for (const char *s = Name + 8, *e = s + 3; s < e && *s != ' '; ++s)
				*d++ = *s;
		}
		*d = 0;
		return n;
	}
	const Entry	*next() const;
};

struct LongEntry {
	enum {LAST_LONG_ENTRY = 0x40};
	uint8	Ord;			//0		1	 The order of this entry in the sequence. Must be masked with LAST_LONG_ENTRY for the last long entry in the set. Therefore, each sequence of entries begins with the contents of this field masked with LAST_LONG_ENTRY.
	char16	Name1[5];		//1		10	 Contains characters 1 through 5 constituting a portion of the long name
	uint8	Attr;			//11	1	 must be ATTR_LONG_NAME
	uint8	Type;			//12	1	 Must be 0
	uint8	Chksum;			//13	1	 Checksum of name in the associated short name directory entry at the end of the long name directory entry set
	char16	Name2[6];		//14	12	 Contains characters 6 through 11 constituting a portion of the long name
	uint16	FstClusLO;		//26	2	 Must be 0
	char16	Name3[2];		//28	4	 Contains characters 12 and 13 constituting a portion of the long name

	bool	is_longname()		const { return Attr == Entry::ATTR_LONG_NAME; }
	const Entry	*get_entry()	const { return (const Entry*)this + (Ord & ~LongEntry::LAST_LONG_ENTRY); }
	void	get_chars(char16 *p) {
		p += ((Ord & ~LongEntry::LAST_LONG_ENTRY) - 1) * 13;
		memcpy(p, Name1, sizeof(Name1));
		memcpy(p + 5, Name2, sizeof(Name2));
		memcpy(p + 11, Name3, sizeof(Name3));
	}

	static uint8 calc_ChkSum(uint8 *name)  {
		uint8 sum = 0;
		for (uint8 *end = name + 11; name != end; name++)
			sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *name;
		return sum;
	}
};

const Entry	*Entry::next() const {
	return (is_longname() ? ((LongEntry*)this)->get_entry() : this) + 1;
}

TYPE Type(uint32 clusters) {
	return clusters < 4085 ? FAT12 : clusters < 65525 ? FAT16 : FAT32;
}

struct BPB {				//off	size
	uint8	jmpBoot[3];		//0		3	Jump instruction to boot code
	char	OEMName[8];		//3		8	OEM Name Identifier
	uint16	BytsPerSec;		//11	2	Bytes per sector: 512, 1024, 2048 or 4096.  
	uint8	SecPerClus;		//13	1	Sectors per allocation unit. The legal values are 1, 2, 4, 8, 16, 32, 64, and 128.  
	uint16	RsvdSecCnt;		//14	2	Reserved sectors in the reserved region of the volume starting at the first sector of the volume. This field is used to align the start of the data area to integral multiples of the cluster size with respect to the start of the partition/media.  
	uint8	NumFATs;		//16	1	The count of file allocation tables (FATs) on the volume
	uint16	RootEntCnt;		//17	2	For FAT12 and FAT16 volumes, this field contains the count of 32-byte directory entries in the root directory. For FAT32 volumes, this field must be set to 0
	uint16	TotSec16;		//19	2	
	uint8	Media;			//21	1	The legal values for this field are 0xF0, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, and 0xFF.  0xF8=nonremovable media
	uint16	FATSz16;		//22	2	16-bit count of sectors occupied by one FAT (0 on FAT32)
	uint16	SecPerTrk;		//24	2	Sectors per track for interrupt 0x13
	uint16	NumHeads;		//26	2	Number of heads for interrupt 0x13
	uint32	HiddSec;		//28	4	Count of hidden sectors preceding the partition that contains this FAT volume
	uint32	TotSec32;		//32	4	Total count of sectors on the volume. This count includes the count of all sectors in all four regions of the volume.  

	uint32			FATSectors()	const;
	uint32			TotalSectors()	const { return TotSec16 ? TotSec16 : TotSec32; }
	uint32			ClusterSize()	const { return BytsPerSec * SecPerClus; }
	uint32			RootSectors()	const { return div_round_up(uint32(RootEntCnt * sizeof(Entry)), BytsPerSec); }
	uint32			ClusterCount()	const { return (TotalSectors() - (RsvdSecCnt + (NumFATs * FATSectors()) + RootSectors())) / SecPerClus; }
	TYPE			Type()			const { return FAT::Type(ClusterCount()); }
	uint8			*Sector(int i)	const { return (uint8*)this + i * BytsPerSec; }
	memory_block	Cluster(int i)	const { return memory_block(Sector(RsvdSecCnt + FATSectors() * NumFATs + RootSectors() + SecPerClus * i), ClusterSize()); }

	uint8			*FAT(int i)		const { return Sector(RsvdSecCnt + FATSectors() * i); }
	memory_block	Root()			const { return memory_block(Sector(RsvdSecCnt + FATSectors() * NumFATs), RootEntCnt * sizeof(Entry)); }
	int				ClusterNext(TYPE type, int i) const {
		uint8	*p = FAT(0);
		switch (type) {
			case FAT::FAT12:
				p += i * 3 / 2;
				return i & 1 ? (p[0] >> 4) | (p[1] << 4) : p[0] | ((p[1] & 0xf) << 8);
			case FAT::FAT16:
				return ((int16*)p)[i];
			default:
				return ((int32*)p)[i & 0xffffff];
		}
	}
};

struct BPB12 : BPB {
	uint8	DrvNum;			//36	1	Interrupt 0x13 drive number (0x80 or 0x00)
	uint8	Reserved1;		//37	1	0
	uint8	BootSig;		//38	1	Extended boot signature. Set 0x29 if either of the following two fields are non-zero
	uint32	VolID;			//39	4	Volume serial number
	char	VolLab[11];		//43	11	Volume label
	char	FilSysType[8];	//54	8	"FAT12   ", "FAT16   ", or "FAT     ".    
	uint8	_[448];			//62	448 0  
	uint32	Signature;		//510	2	0xaa55
};

struct BPB32 : BPB {
	uint32	FATSz32;		//36	4	Sectors occupied by one FAT.  Note that FATSz16 must be 0 for media formatted FAT32.  
	uint16	ExtFlags;		//40	2	Bits 0-3: number of active FAT (if mirroring is disabled). Bit 7: 0=mirrored at runtime into all FATs, 1=only one FAT is active
	uint16	FSVer;			//42	2	Version number of the FAT32 volume
	uint32	RootClus;		//44	4	First cluster of the root directory.
	uint16	FSInfo;			//48	2	Sector number of FSINFO structure in the reserved area of the FAT32 volume. Usually 1.   
	uint16	BkBootSec;		//50	2	Set to 0 or 6. If non-zero, indicates the sector number in the reserved area of the volume of a copy of the boot record.  
	char	Reserved[12];	//52	12	0
	uint8	DrvNum;			//64	1	Interrupt 0x13 drive number (0x80 or 0x00)
	uint8	Reserved1;		//65	1	0
	uint8	BootSig;		//66	1	Extended boot signature. Set to 0x29 if either of the following two fields are non-zero
	uint32	VolID;			//67	4	Volume serial number
	uint32	VolLab[11];		//71	11	Volume label
	char	FilSysType[8];	//82	8	"FAT32			 "
	uint8	_[420];			//90	420	0  
	uint32	Signature;		//510	2	0xaa55
};

inline uint32 BPB::FATSectors() const {
	return FATSz16 ? FATSz16 : ((BPB32*)this)->FATSz32;
}

struct FSInfo {
	enum {
		LEADSIG		= 0x41615252,
		STRUCSIG	= 0x61417272,
		TRAILSIG	= 0xAA550000
	};
	uint32	LeadSig;		//0		4
	uint8	Reserved1[480];	//4		480	0
	uint32	StrucSig;		//484	4
	uint32	Free_Count;		//488	4	Last known free cluster count on the volume. 0xFFFFFFFF indicates the free count is not known
	uint32	Nxt_Free;		//492	4	Cluster number of the first available (free) cluster on the volume. 0xFFFFFFFF indicates that there exists no information about the first available (free) cluster
	uint8	Reserved2[12];	//496	12	0
	uint32	TrailSig;		//508	4
};

#pragma pack()

} } //namespace iso::FAT

#endif // FAT_H
