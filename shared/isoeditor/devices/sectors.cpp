#include "main.h"
#include "device.h"
#include "extra/date.h"
#include "extra/disk.h"
#include "base/algorithm.h"
#include "vm.h"

#include <windows.h>
#include <winioctl.h>
#include <Setupapi.h>

#pragma comment(lib, "setupapi.lib")

using namespace iso;

//-----------------------------------------------------------------------------
//	Sectors
//-----------------------------------------------------------------------------

struct Sector {
	void		*mem;
	size_t		size;
	Sector(size_t size) : size(size)	{ mem = vmem::reserve_commit(size); }
	~Sector()							{ vmem::decommit_release(mem, size); }
};

template<> struct ISO::def<Sector> : ISO::VirtualT2<Sector> {
	def() : ISO::VirtualT2<Sector>(VIRTSIZE) {}

	static uint32			Count(Sector &a)		{ return uint32(a.size);	}
	static ISO::Browser		Index(Sector &a, int i)	{ return ISO::MakeBrowser(((xint8*)a.mem)[i]);	}
	static ISO::Browser2	Deref(Sector &a)		{ return ISO::ptr<ISO::memory_block_deref>(0, (const void*)a.mem, uint32(a.size)); }
};

class Sectors : public ISO::VirtualDefaults {
	HANDLE	h;
	uint64	offset;
	uint64	size;

	uint32	chunk_size;
	uint32	num_chunks;

public:
	Sectors(HANDLE h) : h(h), offset(0), size(0), chunk_size(0), num_chunks(0) {
		STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR	alignment;
		DISK_GEOMETRY_EX					geom;
		DWORD								cb;

		clear(alignment);
		clear(geom);

		if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geom, sizeof(geom), &cb, NULL)) {
			size	= geom.DiskSize.QuadPart;
			STORAGE_PROPERTY_QUERY	query = {StorageAccessAlignmentProperty, PropertyStandardQuery};
			uint32	sector_size = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
				&query, sizeof(STORAGE_PROPERTY_QUERY),
				&alignment, sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),
				&cb, NULL
			) ? alignment.BytesPerPhysicalSector : geom.Geometry.BytesPerSector;

			chunk_size	= max(sector_size, 1 << 20);
			num_chunks	= (size + chunk_size - 1) / chunk_size;
		}
	}

	Sectors(HANDLE h, uint32 sector_size, uint64 first_sector, uint64 num_sectors) : h(h), offset(first_sector * sector_size), size(num_sectors * sector_size) {
		chunk_size		= max(sector_size, 1 << 20);
		num_chunks		= (size + chunk_size - 1) / chunk_size;
	}

	~Sectors()					{ CloseHandle(h); }
	uint32			Count()		{ return num_chunks; }

	ISO::Browser2	Index(int i) {
		uint64	a = uint64(chunk_size) * i;
		uint32	b = min(size - a, uint64(chunk_size));

		LARGE_INTEGER	seek;
		DWORD			read;

		seek.QuadPart = a + offset;
		SetFilePointerEx(h, seek, &seek, 0);

		ISO_ptr<Sector>	p(0, b);
		ReadFile(h, p->mem, b, &read, NULL);
		return p;
	}
};
ISO_DEFUSERVIRTX(Sectors, "BigBin");

struct Partition {
	string		name;
	MBR::TYPE	type;
	const char*	type2;
	Sectors		sectors;
	Partition(HANDLE h, MBR::Partition &p) : sectors(h, 512, p.LBA_start, p.LBA_length), type(MBR::TYPE(p.type)), type2(0) {}
	Partition(HANDLE h, PartitionEntry &p, const char *type2) : name(p.Name), sectors(h, 512, p.FirstLBA, p.LastLBA - p.FirstLBA + 1), type(MBR::GPT_PROTECT), type2(type2) {}
};

ISO_DEFUSERENUMF(MBR::TYPE,256, 8, ISO::Type::FLAGS2(ISO::Type::NONE, ISO::TypeEnum::DISCRETE | ISO::TypeEnum::HEX)) {
#define ENUM(x) #x, MBR::x
	Init(0,
	ENUM(EMPTY),				ENUM(FAT12),				ENUM(XENIX_ROOT),			ENUM(XENIX_USR),
	ENUM(FAT16),				ENUM(DOS_EXT),				ENUM(FAT16B),				ENUM(NTFS),
	ENUM(FAT32_CHS),			ENUM(FAT32_LBA),			ENUM(FAT16_LBA),			ENUM(WIN98_EXT),
	//0x10
	ENUM(OPUS),					ENUM(FAT12_HIDDEN),			ENUM(FAT16_HIDDEN),			ENUM(DOS_EXT_HIDDEN),
	ENUM(FAT16B_HIDDEN),		ENUM(NTFS_HIDDEN),			ENUM(HIBERNATION),			ENUM(FAT32_CHS_HIDDEN),
	ENUM(FAT32_LBA_HIDDEN),		ENUM(FAT16_LBA_HIDDEN),		ENUM(WIN98_EXT_HIDDEN),
	//0x20
	ENUM(HP_SPEEDSTOR),			ENUM(MS_MOBILE),			ENUM(RECOVERY),				ENUM(ATHEOS),
	ENUM(SYLLABLEOS),
	//0x30
	ENUM(NOS),					ENUM(JFS),					ENUM(THEOS),				ENUM(PLAN9),
	ENUM(THEOS_4G),				ENUM(THEOS_EXT),			ENUM(PARTITIONMAGIC),		ENUM(NETWARE_HIDDEN),
	ENUM(OS_32),
	//0x40
	ENUM(VENIX),				ENUM(QNX_PRIMARY),			ENUM(QNX_SECONDARY),		ENUM(QNX_TERTIARY),
	//0x50
	ENUM(DM4),					ENUM(DM6_AUX1),				ENUM(CP_M),					ENUM(DM6_AUX3),
	ENUM(DM6),					ENUM(EZD),					ENUM(EDISK),				ENUM(APTI),
	ENUM(APTI_WIN98_EXT),		ENUM(APTI_DOS_EXT),
	//0x60
	ENUM(SPEEDSTOR),			ENUM(UNIXWARE),				ENUM(NETWARE_286),			ENUM(NETWARE_386),
	//0x70
	ENUM(APTI_FAT12),			ENUM(IBM_PCIX),				ENUM(NOVELL),				ENUM(APTI_FAT16_CHS),
	ENUM(APTI_FAT16_LBA),		ENUM(APTI_FAT16B),			ENUM(APTI_FAT32_LBA),		ENUM(APTI_FAT32_CHS),
	ENUM(FIX),					ENUM(AODPS),
	//0x80
	ENUM(MINIX),				ENUM(LINUX_SWAP),			ENUM(LINUX_DATA),			ENUM(APM_HIBERNATION),
	ENUM(LINUX_EXTENDED),		ENUM(FAT16B_MIRROR),		ENUM(NTFS_MIRROR),			ENUM(PARTITION_TEXT),
	ENUM(LINUX_KERNEL),			ENUM(FAT32_CHS_MIRROR),		ENUM(FAT32_LBA_MIRROR),		ENUM(FAT12_MIRROR),
	ENUM(LINUX_LVM),
	//0x90
	ENUM(FAT16_MIRROR),			ENUM(DOS_EXT_MIRROR),		ENUM(FAT16B_MIRROR2),		ENUM(FAT32_CHS_HIDDEN2),
	ENUM(FAT32_LBA_HIDDEN2),	ENUM(FAT16_LBA_HIDDEN2),	ENUM(WIN98_EXT_HIDDEN2),
	//0xa0
	ENUM(FREEBSD),				ENUM(OPENBSD),				ENUM(NEXT),					ENUM(DARWIN),
	ENUM(NETBSD),				ENUM(DARWIN_BOOT),			ENUM(ADFS),					ENUM(HFS),
	//0xb0
	ENUM(FAT16B_MIRROR3),		ENUM(BSDI),					ENUM(FAT32_CHS_MIRROR2),	ENUM(FAT32_LBA_MIRROR2),
	ENUM(SOLARIS_BOOT),			ENUM(NEW_SOLARIS_X86),
	//0xc0
	ENUM(DRDOS_FAT),			ENUM(DRDOS_FAT12),			ENUM(FAT16_SECURE),			ENUM(DOS_EXT_SECURE),
	ENUM(FAT16B_SECURE),		ENUM(NTFS_SECURE),			ENUM(FAT32_CHS_SECURE),		ENUM(FAT32_LBA_SECURE),
	ENUM(FAT16_LBA_SECURE),		ENUM(WIN98_EXT_SECURE),
	//0xd0
	ENUM(DELL_RESTORE),			ENUM(DELL_DIAG),
	//0xe0
	ENUM(BEOS),					ENUM(SKYOS),				ENUM(GPT_PROTECT),			ENUM(EFI),
	//0xf0
	ENUM(LINUX_RAID)
	);
#undef ENUM
} };

ISO_DEFUSERCOMPV(Partition, name, type, type2, sectors);

//Partition type						Globally unique identifier
struct {const char *name; GUID id;} guid_partition_types[] = {
//(None)
{"Unused entry",						{0x00000000,0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{"MBR partition scheme",				{0x024DEE41,0x33E7,0x11D3,0x9D,0x69,0x00,0x08,0xC7,0x81,0xF3,0x9F}},
{"EFI System partition",				{0xC12A7328,0xF81F,0x11D2,0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B}},
{"BIOS Boot partition",					{0x21686148,0x6449,0x6E6F,0x74,0x4E,0x65,0x65,0x64,0x45,0x46,0x49}},
{"Intel Fast Flash (iFFS)",				{0xD3BFE2DE,0x3DAF,0x11DF,0xBA,0x40,0xE3,0xA5,0x56,0xD8,0x95,0x93}},
{"Sony boot partition",					{0xF4019732,0x066E,0x4E12,0x82,0x73,0x34,0x6C,0x56,0x41,0x49,0x4F}},
{"Lenovo boot partition",				{0xBFBFAFE7,0xA34F,0x448A,0x9A,0x5B,0x62,0x13,0xEB,0x73,0x6C,0x22}},

//Windows
{"Microsoft Reserved",					{0xE3C9E316,0x0B5C,0x4DB8,0x81,0x7D,0xF9,0x2D,0xF0,0x02,0x15,0xAE}},
{"Basic data partition",				{0xEBD0A0A2,0xB9E5,0x4433,0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7}},
{"Logical Disk Manager metadata",		{0x5808C8AA,0x7E8F,0x42E0,0x85,0xD2,0xE1,0xE9,0x04,0x34,0xCF,0xB3}},
{"Logical Disk Manager data",			{0xAF9B60A0,0x1431,0x4F62,0xBC,0x68,0x33,0x11,0x71,0x4A,0x69,0xAD}},
{"Windows Recovery Environment",		{0xDE94BBA4,0x06D1,0x4D40,0xA1,0x6A,0xBF,0xD5,0x01,0x79,0xD6,0xAC}},
{"IBM General Parallel File System",	{0x37AFFC90,0xEF7D,0x4E96,0x91,0xC3,0x2D,0x7A,0xE0,0x55,0xB1,0x74}},
{"Storage Spaces",						{0xE75CAF8F,0xF680,0x4CEE,0xAF,0xA3,0xB0,0x01,0xE5,0x6E,0xFC,0x2D}},

//HP-UX
{"Data partition",						{0x75894C1E,0x3AEB,0x11D3,0xB7,0xC1,0x7B,0x03,0xA0,0x00,0x00,0x00}},
{"Service Partition",					{0xE2A1E728,0x32E3,0x11D6,0xA6,0x82,0x7B,0x03,0xA0,0x00,0x00,0x00}},

//Linux
{"Linux filesystem data",				{0x0FC63DAF,0x8483,0x4772,0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4}},
{"RAID partition",						{0xA19D880F,0x05FC,0x4D3B,0xA0,0x06,0x74,0x3F,0x0F,0x84,0x91,0x1E}},
{"Root partition (x86)",				{0x44479540,0xF297,0x41B2,0x9A,0xF7,0xD1,0x31,0xD5,0xF0,0x45,0x8A}},
{"Root partition (x86,0x64)",			{0x4F68BCE3,0xE8CD,0x4DB1,0x96,0xE7,0xFB,0xCA,0xF9,0x84,0xB7,0x09}},
{"Root partition (AArch32)",			{0x69DAD710,0x2CE4,0x4E3C,0xB1,0x6C,0x21,0xA1,0xD4,0x9A,0xBE,0xD3}},
{"Root partition (AArch64)",			{0xB921B045,0x1DF0,0x41C3,0xAF,0x44,0x4C,0x6F,0x28,0x0D,0x3F,0xAE}},
{"Swap partition",						{0x0657FD6D,0xA4AB,0x43C4,0x84,0xE5,0x09,0x33,0xC8,0x4B,0x4F,0x4F}},
{"Logical Volume Manager (LVM)",		{0xE6D6D379,0xF507,0x44C2,0xA2,0x3C,0x23,0x8F,0x2A,0x3D,0xF9,0x28}},
{"/home partition",						{0x933AC7E1,0x2EB4,0x4F13,0xB8,0x44,0x0E,0x14,0xE2,0xAE,0xF9,0x15}},
{"/srv (server data) partition",		{0x3B8F8425,0x20E0,0x4F3B,0x90,0x7F,0x1A,0x25,0xA7,0x6F,0x98,0xE8}},
{"Plain dm-crypt partition",			{0x7FFEC5C9,0x2D00,0x49B7,0x89,0x41,0x3E,0xA1,0x0A,0x55,0x86,0xB7}},
{"LUKS partition",						{0xCA7D7CCB,0x63ED,0x4C53,0x86,0x1C,0x17,0x42,0x53,0x60,0x59,0xCC}},
{"Reserved",							{0x8DA63339,0x0007,0x60C0,0xC4,0x36,0x08,0x3A,0xC8,0x23,0x09,0x08}},

//FreeBSD
{"Boot partition",						{0x83BD6B9D,0x7F41,0x11DC,0xBE,0x0B,0x00,0x15,0x60,0xB8,0x4F,0x0F}},
{"Data partition",						{0x516E7CB4,0x6ECF,0x11D6,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B}},
{"Swap partition",						{0x516E7CB5,0x6ECF,0x11D6,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B}},
{"Unix File System (UFS) partition",	{0x516E7CB6,0x6ECF,0x11D6,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B}},
{"Vinum volume manager partition",		{0x516E7CB8,0x6ECF,0x11D6,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B}},
{"ZFS partition",						{0x516E7CBA,0x6ECF,0x11D6,0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B}},

//OS X
{"(HFS+) partition",					{0x48465300,0x0000,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"Apple UFS",							{0x55465300,0x0000,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"Apple APFS",							{0x7C3457EF,0x0000,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"ZFS",									{0x6A898CC3,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Apple RAID partition",				{0x52414944,0x0000,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"Apple RAID partition, offline",		{0x52414944,0x5F4F,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"Apple Boot partition (Recovery HD)",	{0x426F6F74,0x0000,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"Apple Label",							{0x4C616265,0x6C00,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"Apple TV Recovery partition",			{0x5265636F,0x7665,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},
{"Apple Core Storage",					{0x53746F72,0x6167,0x11AA,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC}},

//Solaris
{"illumos Boot partition",				{0x6A82CB45,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Root partition",						{0x6A85CF4D,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Swap partition",						{0x6A87C46F,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Backup partition",					{0x6A8B642B,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"/usr partition",						{0x6A898CC3,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"/var partition",						{0x6A8EF2E9,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"/home partition",						{0x6A90BA39,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Alternate sector",					{0x6A9283A5,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Reserved partition",					{0x6A945A3B,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Reserved partition",					{0x6A9630D1,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Reserved partition",					{0x6A980767,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Reserved partition",					{0x6A96237F,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},
{"Reserved partition",					{0x6A8D2AC7,0x1DD2,0x11B2,0x99,0xA6,0x08,0x00,0x20,0x73,0x66,0x31}},

//NetBSD
{"Swap partition",						{0x49F48D32,0xB10E,0x11DC,0xB9,0x9B,0x00,0x19,0xD1,0x87,0x96,0x48}},
{"FFS partition",						{0x49F48D5A,0xB10E,0x11DC,0xB9,0x9B,0x00,0x19,0xD1,0x87,0x96,0x48}},
{"LFS partition",						{0x49F48D82,0xB10E,0x11DC,0xB9,0x9B,0x00,0x19,0xD1,0x87,0x96,0x48}},
{"RAID partition",						{0x49F48DAA,0xB10E,0x11DC,0xB9,0x9B,0x00,0x19,0xD1,0x87,0x96,0x48}},
{"Concatenated partition",				{0x2DB519C4,0xB10F,0x11DC,0xB9,0x9B,0x00,0x19,0xD1,0x87,0x96,0x48}},
{"Encrypted partition",					{0x2DB519EC,0xB10F,0x11DC,0xB9,0x9B,0x00,0x19,0xD1,0x87,0x96,0x48}},
{"ChromeOS ChromeOS kernel",			{0xFE3A2A5D,0x4F32,0x41A7,0xB7,0x25,0xAC,0xCC,0x32,0x85,0xA3,0x09}},
{"ChromeOS rootfs",						{0x3CB8E202,0x3B7E,0x47DD,0x8A,0x3C,0x7F,0xF2,0xA1,0x3C,0xFC,0xEC}},
{"ChromeOS future use",					{0x2E0A753D,0x9E48,0x43B0,0x83,0x37,0xB1,0x51,0x92,0xCB,0x1B,0x5E}},

//Haiku
{"Haiku BFS",							{0x42465331,0x3BA3,0x10F1,0x80,0x2A,0x48,0x61,0x69,0x6B,0x75,0x21}},

//MidnightBSD
{"Boot partition",						{0x85D5E45E,0x237C,0x11E1,0xB4,0xB3,0xE8,0x9A,0x8F,0x7F,0xC3,0xA7}},
{"Data partition",						{0x85D5E45A,0x237C,0x11E1,0xB4,0xB3,0xE8,0x9A,0x8F,0x7F,0xC3,0xA7}},
{"Swap partition",						{0x85D5E45B,0x237C,0x11E1,0xB4,0xB3,0xE8,0x9A,0x8F,0x7F,0xC3,0xA7}},
{"Unix File System (UFS) partition",	{0x0394EF8B,0x237E,0x11E1,0xB4,0xB3,0xE8,0x9A,0x8F,0x7F,0xC3,0xA7}},
{"Vinum volume manager partition",		{0x85D5E45C,0x237C,0x11E1,0xB4,0xB3,0xE8,0x9A,0x8F,0x7F,0xC3,0xA7}},
{"ZFS partition",						{0x85D5E45D,0x237C,0x11E1,0xB4,0xB3,0xE8,0x9A,0x8F,0x7F,0xC3,0xA7}},

//Ceph
{"Ceph Journal",						{0x45B0969E,0x9B03,0x4F30,0xB4,0xC6,0xB4,0xB8,0x0C,0xEF,0xF1,0x06}},
{"Ceph dm-crypt Encrypted Journal",		{0x45B0969E,0x9B03,0x4F30,0xB4,0xC6,0x5E,0xC0,0x0C,0xEF,0xF1,0x06}},
{"Ceph OSD",							{0x4FBD7E29,0x9D25,0x41B8,0xAF,0xD0,0x06,0x2C,0x0C,0xEF,0xF0,0x5D}},
{"Ceph dm-crypt OSD",					{0x4FBD7E29,0x9D25,0x41B8,0xAF,0xD0,0x5E,0xC0,0x0C,0xEF,0xF0,0x5D}},
{"Ceph disk in creation",				{0x89C57F98,0x2FE5,0x4DC0,0x89,0xC1,0xF3,0xAD,0x0C,0xEF,0xF2,0xBE}},
{"Ceph dm-crypt disk in creation",		{0x89C57F98,0x2FE5,0x4DC0,0x89,0xC1,0x5E,0xC0,0x0C,0xEF,0xF2,0xBE}},

//OpenBSD
{"Data partition",						{0x824CC7A0,0x36A8,0x11E3,0x89,0x0A,0x95,0x25,0x19,0xAD,0x3F,0x61}},

//QNX
{"Power-safe (QNX6) file system",		{0xCEF5A9AD,0x73BC,0x4601,0x89,0xF3,0xCD,0xEE,0xEE,0xE3,0x21,0xA1}},

//Plan 9
{"Plan 9 partition",					{0xC91818F9,0x8025,0x47AF,0x89,0xD2,0xF0,0x30,0xD7,0x00,0x0C,0x2C}},

//VMware ESX
{"vmkcore (coredump partition)",		{0x9D275380,0x40AD,0x11DB,0xBF,0x97,0x00,0x0C,0x29,0x11,0xD1,0xB8}},
{"VMFS filesystem partition",			{0xAA31E02A,0x400F,0x11DB,0x95,0x90,0x00,0x0C,0x29,0x11,0xD1,0xB8}},
{"VMware Reserved",						{0x9198EFFC,0x31C0,0x11DB,0x8F,0x78,0x00,0x0C,0x29,0x11,0xD1,0xB8}},

};

HANDLE DuplicateHandle(HANDLE h) {
	HANDLE	h2;
	return DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &h2, 0, FALSE, DUPLICATE_SAME_ACCESS) ? h2 : INVALID_HANDLE_VALUE;
}

struct DiskDescription {
	string		description;
	uint64		disk_size;
	uint64		cylinders;
	uint32		tracks_per_cylinder;	//TracksPerCylinder;
	uint32		sectors_per_track;		//SectorsPerTrack;
	uint32		sector_size;			//BytesPerSector;

	ISO_openarray<pointer<Partition> >	partitions;
	Sectors		sectors;

//	uint32	cache_line;		//BytesPerCacheLine;
//	uint32	logical_sector;	//BytesPerLogicalSector;
//	uint32	physical_sector;//BytesPerPhysicalSector;

	DiskDescription(HANDLE h) : sectors(h) {
		DISK_GEOMETRY_EX	geom;
		DWORD				size;
		clear(geom);
		if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geom, sizeof(geom), &size, NULL)) {
			disk_size				= geom.DiskSize.QuadPart;
			cylinders				= geom.Geometry.Cylinders.QuadPart;
			tracks_per_cylinder		= geom.Geometry.TracksPerCylinder;
			sectors_per_track		= geom.Geometry.SectorsPerTrack;
			sector_size				= geom.Geometry.BytesPerSector;
		}

		STORAGE_PROPERTY_QUERY		query = {StorageDeviceProperty, PropertyStandardQuery};
		STORAGE_DESCRIPTOR_HEADER	desc;
		if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
			&query, sizeof(query),
			&desc, sizeof(desc),
			&size, NULL
		)) {
			malloc_block	mb(desc.Size);
			STORAGE_DEVICE_DESCRIPTOR	*desc2 = mb;
			if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
				&query, sizeof(query),
				desc2, desc.Size,
				&size, NULL
			)) {
				buffer_accum<256>	ba;
				if (desc2->VendorIdOffset)
					ba << (char*)mb + desc2->VendorIdOffset;
				if (desc2->ProductIdOffset) {
					ba << (char*)mb + desc2->ProductIdOffset;
					if (desc2->ProductRevisionOffset)
						ba << " rev:" << skip_whitespace((char*)mb + desc2->ProductRevisionOffset);
				}
				if (desc2->SerialNumberOffset)
					ba << " S/N:" << skip_whitespace((char*)mb + desc2->SerialNumberOffset);
				description = ba;
			}
		}

		Sector	s(512);
		ReadFile(h, s.mem, 512, &size, NULL);
		MBR		&mbr	= *(MBR*)s.mem;
		if (mbr.part[0].type != MBR::GPT_PROTECT) {
			for (int i = 0; i < 4; i++) {
				if (mbr.part[i].LBA_length)
					partitions.Append(new Partition(DuplicateHandle(h), mbr.part[i]));
			}
		}  else {
			ReadFile(h, s.mem, 512, &size, NULL);
			PartitionTableHeader	pth = *(PartitionTableHeader*)s.mem;

			int	n = pth.NumberPartitions;

			LARGE_INTEGER	seek;
			seek.QuadPart = pth.EntriesLBA * 512;
			SetFilePointerEx(h, seek, &seek, SEEK_SET);

			malloc_block	table(sizeof(PartitionEntry) * n);
			ReadFile(h, table, sizeof(PartitionEntry) * n, &size, NULL);
			PartitionEntry	*pe = table;
			for (int i = 0; i < n; i++, pe++) {
				const char *type = 0;
				for (auto &j : guid_partition_types) {
					if (pe->Type == j.id)
						type = j.name;
				}
				if (type != guid_partition_types[0].name)
					partitions.Append(new Partition(DuplicateHandle(h), *pe, type));
			}
		}
	}
};

ISO_DEFUSERCOMPV(DiskDescription,description, disk_size, cylinders, tracks_per_cylinder, sectors_per_track, sector_size, partitions, sectors);

HANDLE GetDisk(const char *dev) {
	HANDLE	h = CreateFileA(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
	if (h == INVALID_HANDLE_VALUE) {
		DWORD	err = GetLastError();
		char	win32name[256];
		QueryDosDevice(dev, win32name, DWORD(num_elements(win32name)));
		h = CreateFile(win32name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
	}
	return h;
}

uint32 DiskSectorSize(HANDLE h) {
	STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR	alignment;
	STORAGE_PROPERTY_QUERY	query = {StorageAccessAlignmentProperty, PropertyStandardQuery};
	DWORD					size;
	return DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &alignment, sizeof(alignment), &size, NULL)
		? alignment.BytesPerPhysicalSector
		: 0;
}

uint64 DiskSize(HANDLE h) {
	DISK_GEOMETRY_EX	geom;
	DWORD				size;
	return DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geom, sizeof(geom), &size, NULL)
		? geom.DiskSize.QuadPart
		: 0;
}

#if 0
void DiskCopy(int f, int t) {
	uint64	done	= 0;//x3ac000000;

	Win32Handle	fh		= CreateFileA(format_string("\\\\.\\physicaldrive%i", f), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
	Win32Handle	th		= CreateFileA(format_string("\\\\.\\physicaldrive%i", t), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);

	uint64	fsize	= DiskSize(fh);
	uint64	tsize	= DiskSize(th);
	uint32	fsect	= DiskSectorSize(fh);
	uint32	tsect	= DiskSectorSize(th);

	LARGE_INTEGER	seek;
	seek.QuadPart = done;
	SetFilePointerEx(fh, seek, &seek, SEEK_SET);
	SetFilePointerEx(th, seek, &seek, SEEK_SET);

	uint32	block	= 1 << 24;
	void	*mem	= vmem::reserve_commit(block);

	DateTime	start	= DateTime::Now();
	DateTime	next	= start + DateTime::Secs(1);
	uint64		total	= min(fsize, tsize);
	while (done < total) {
		if (DateTime::Now() >= next) {
			ISO_TRACEF("At ") << (next - start).TimeOfDay() << "s " << done * 100.0 / total << "%\n";
			next += DateTime::Secs(1);
		}
		DWORD	r, w;
		uint64	x	= min(total - done, block);

		if (!ReadFile(fh, mem, x, &r, NULL)) {
			ISO_TRACEF("Retrying at ") << hex(done) << "\n";
			for (uint32 off = 0; off < x; off += fsect) {
				if (!ReadFile(fh, (uint8*)mem + off, fsect, &r, NULL)) {
					ISO_TRACEF("Failed at ") << hex(done + off) << "\n";
					seek.QuadPart = done + off + fsect;
					SetFilePointerEx(fh, seek, &seek, SEEK_SET);
					memset((uint8*)mem + off, 0, fsect);
				}
			}
		} else {
			ISO_ASSERT(x == r);
		}

		WriteFile(th, mem, x, &w, NULL);
		ISO_ASSERT(x == w);
		done	+= x;
	}

	vmem::decommit_release(mem, block);
}
#else

void FastCopy(HANDLE fh, uint64 foffset, HANDLE th, uint64 toffset, uint64 total, uint32 fsect) {
	uint64		done	= 0;
	OVERLAPPED	tov[2];
	clear(tov);
	tov[0].hEvent		= CreateEvent(NULL, FALSE, TRUE, NULL);
	tov[1].hEvent		= CreateEvent(NULL, FALSE, TRUE, NULL);

	LARGE_INTEGER	seek;
	seek.QuadPart = done + foffset;
	SetFilePointerEx(fh, seek, &seek, SEEK_SET);

	uint32	block	= 1 << 24;
	void	*mem[2];
	mem[0]	= vmem::reserve_commit(block);
	mem[1]	= vmem::reserve_commit(block);

	DateTime	start	= DateTime::Now();
	DateTime	next	= start + Duration::Secs(1);

	for (int b = 0; done < total; b = 1 - b) {
		if (DateTime::Now() >= next) {
			ISO_TRACEF("At ") << (next - start).fSecs() << "s " << done * 100.0 / total << "%\n";
			next += Duration::Secs(1);
		}
		DWORD	r, w;
		uint64	x	= min(total - done, block);

		WaitForSingleObject(tov[b].hEvent, INFINITE);

		if (!ReadFile(fh, mem[b], x, &r, NULL)) {
			ISO_TRACEF("Retrying at ") << hex(done) << "\n";
			for (uint32 off = 0; off < x; off += fsect) {
				if (!ReadFile(fh, (uint8*)mem[b] + off, fsect, &r, NULL)) {
					ISO_TRACEF("Failed at ") << hex(done + off) << "\n";
					seek.QuadPart = done + off + fsect;
					SetFilePointerEx(fh, seek, &seek, SEEK_SET);
					memset((uint8*)mem[b] + off, 0, fsect);
				}
			}
		} else {
			ISO_ASSERT(x == r);
		}

		uint64	offset		= done + toffset;
		tov[b].Offset		= offset;
		tov[b].OffsetHigh	= offset >> 32;
		WriteFile(th, mem[b], x, &w, &tov[b]);

		done	+= x;
	}
	WaitForSingleObject(tov[0].hEvent, INFINITE);
	WaitForSingleObject(tov[1].hEvent, INFINITE);

	vmem::decommit_release(mem[0], block);
	vmem::decommit_release(mem[1], block);

	CloseHandle(tov[0].hEvent);
	CloseHandle(tov[1].hEvent);
}

void DiskCopy(int f, int t) {
	Win32Handle	fh	= CreateFileA(format_string("\\\\.\\physicaldrive%i", f), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
	Win32Handle	th	= CreateFileA(format_string("\\\\.\\physicaldrive%i", t), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, 0);

	FastCopy(fh, 0, th, 0, min(DiskSize(fh), DiskSize(th)), DiskSectorSize(fh));
	CloseHandle(fh);
	CloseHandle(th);
}

struct MacCopy {
	MacCopy() {
	Win32Handle	fh	= CreateFileA("\\\\.\\physicaldrive3", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
	Win32Handle	th	= CreateFileA("e:\\evo850.bin", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, 0);

	uint64	done = 0x29D2000000ull;
	//FastCopy(fh, uint64(0x64028) * 0x200, th, 0, uint64(0x7456ce40) * 0x200, DiskSectorSize(fh));
//	FastCopy(fh, uint64(0x7456ce00+0x64028) * 0x200, th, uint64(0x7456ce00) * 0x200, uint64(0x40) * 0x200, DiskSectorSize(fh));
	FastCopy(fh, done, th, done, DiskSize(fh) - done, DiskSectorSize(fh));
	}
};// maccopy;
#endif

//-----------------------------------------------------------------------------
//	SectorsDevice
//-----------------------------------------------------------------------------


struct SectorsDevice : app::DeviceT<SectorsDevice>, app::MenuCallbackT<SectorsDevice>, DeviceHandler {
	int	id;

	struct PhysicalDrive : app::DeviceCreateT<PhysicalDrive>, Handles2<PhysicalDrive, AppEvent> {
		string	id, device;
		ISO_ptr<void>	operator()(const win::Control &main)	{
			HANDLE h = GetDisk(device);
			return h == INVALID_HANDLE_VALUE ? ISO_NULL : (ISO_ptr<void>)ISO_ptr<DiskDescription>(id, h);
		}
		void			operator()(AppEvent *ev)				{ if (ev->state == AppEvent::END) delete this; }
		PhysicalDrive(string_ref id, string_ref device) : id(id), device(device)	{}
	};

	struct LogicalDrive : app::DeviceCreateT<LogicalDrive>, Handles2<LogicalDrive, AppEvent> {
		string	id, device;
		ISO_ptr<void>	operator()(const win::Control &main)	{
			HANDLE h = GetDisk(device);
			return h == INVALID_HANDLE_VALUE ? ISO_NULL : (ISO_ptr<void>)ISO_ptr<Sectors>(id, h);
		}
		void			operator()(AppEvent *ev)				{ if (ev->state == AppEvent::END) delete this; }
		LogicalDrive(string_ref id, string_ref device) : id(id), device(device)	{}
	};

	virtual ISO_ptr<void>	Read(tag id, const char *spec);

	void operator()(win::Control c, win::Menu sub) {
		app::DeviceAdd	add(sub, id);
		ConcurrentJobs::Get().add([=]() {
			while (sub.RemoveByPos(0));

			// logical drives
			DWORD	log = GetLogicalDrives();
			for (int i = 'A'; log; log >>=1, i++) {
				if ((log & 1) && Win32Handle(CreateFileA(format_string("\\\\.\\%c:", i), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS, 0)).Valid())
					add(format_string("Logical Drive %c", i),
						new LogicalDrive(format_string("Logical Drive %c", i), format_string("\\\\.\\%c:", i))
					);
			}

			// physical drives
			GUID		guid	= GUID_DEVINTERFACE_DISK;
			HDEVINFO	devices = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
			SP_DEVICE_INTERFACE_DATA	data = {sizeof(SP_DEVICE_INTERFACE_DATA), 0};

			for (DWORD index = 0; SetupDiEnumDeviceInterfaces(devices, NULL, &guid, index, &data); ++index) {
				DWORD		size;

				SetupDiGetDeviceInterfaceDetail(devices, &data, NULL, 0, &size, NULL);
				malloc_block					buffer(size);
				SP_DEVICE_INTERFACE_DETAIL_DATA	*detail = buffer;
				detail->cbSize							= sizeof( SP_DEVICE_INTERFACE_DETAIL_DATA );
				SetupDiGetDeviceInterfaceDetail(devices, &data, detail, size, NULL, NULL);

				Win32Handle	h = CreateFile(detail->DevicePath, 0,//GENERIC_READ,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
				);

				if (h.Valid()) {
					STORAGE_DEVICE_NUMBER	n;
					DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &n, sizeof(STORAGE_DEVICE_NUMBER), &size, NULL);

					buffer_accum<256>	ba;
					ba << "Physical Drive " << n.DeviceNumber;

					STORAGE_PROPERTY_QUERY		query = {StorageDeviceProperty, PropertyStandardQuery};
					STORAGE_DESCRIPTOR_HEADER	desc;
					if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &desc, sizeof(desc), &size, NULL)) {
						malloc_block	mb(desc.Size);
						STORAGE_DEVICE_DESCRIPTOR	*desc2 = mb;
						if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), desc2, desc.Size, &size, NULL)) {
							ba << ": ";
							if (desc2->VendorIdOffset)
								ba << (char*)mb + desc2->VendorIdOffset;
							if (desc2->ProductIdOffset)
								ba << (char*)mb + desc2->ProductIdOffset;
							char *p0 = ba.getp(), *p = p0;
							while (*--p == ' ');
							ba.move(p + 1 - p0);
						}
					}

					add(ba, new PhysicalDrive(ba, format_string("\\\\.\\physicaldrive%i", n.DeviceNumber)));
				}

			}

			SetupDiDestroyDeviceInfoList(devices);
		});
	}

	void operator()(const app::DeviceAdd &add) {
		id = add.id;
		add("Raw Sectors", this, app::LoadPNG("IDB_DEVICE_HARDDRIVE"));
	}

	SectorsDevice() : DeviceHandler("sectors") {}
} sectors_device;

ISO_ptr<void>	SectorsDevice::Read(tag id, const char *spec) {
	spec = string_find(spec, ':') + 1;
	if (is_prefixed_int(spec)) {
		int	n;
		spec = get_prefixed_num(spec, n);

		HANDLE h = GetDisk(format_string("\\\\.\\physicaldrive%i", n));
		return h == INVALID_HANDLE_VALUE ? ISO_NULL : (ISO_ptr<void>)ISO_ptr<DiskDescription>(id, h);
	}
	return ISO_NULL;
}

struct Partitions : ISO_openarray<pointer<Partition> > {
	Partitions(HANDLE h) {
		DWORD		size;
		Sector		s(512);
		ReadFile(h, s.mem, 512, &size, NULL);
		MBR			&mbr	= *(MBR*)s.mem;

		if (mbr.part[0].type != MBR::GPT_PROTECT) {
			for (int i = 0; i < 4; i++) {
				if (mbr.part[i].LBA_length)
					Append(new Partition(DuplicateHandle(h), mbr.part[i]));
			}
		}  else {
			ReadFile(h, s.mem, 512, &size, NULL);
			PartitionTableHeader	pth = *(PartitionTableHeader*)s.mem;

			int	n = pth.NumberPartitions;

			LARGE_INTEGER	seek;
			seek.QuadPart = pth.EntriesLBA * 512;
			SetFilePointerEx(h, seek, &seek, SEEK_SET);

			malloc_block	table(sizeof(PartitionEntry) * n);
			ReadFile(h, table, sizeof(PartitionEntry) * n, &size, NULL);
			PartitionEntry	*pe = table;
			for (int i = 0; i < n; i++, pe++) {
				const char *type = 0;
				for (auto &j : guid_partition_types) {
					if (pe->Type == j.id)
						type = j.name;
				}
				if (type != guid_partition_types[0].name)
					Append(new Partition(DuplicateHandle(h), *pe, type));
			}
		}
		CloseHandle(h);
	}
};

ISO_DEFSAME(Partitions, ISO_openarray<pointer<Partition> >);

class SectorsFileHandler : public FileHandler {
	virtual	const char*		GetDescription()	{ return "Drive Sectors"; }

	virtual ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) {
		return ISO_ptr<Partitions>(id,
			CreateFileA(fn, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0)
		);
	}

} sectors_fh;
