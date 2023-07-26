#ifndef NTFS_H
#define NTFS_H

#include "base/defs.h"
#include "base/pointer.h"

namespace iso { namespace NTFS {

#ifndef _WINNT_
enum {
	FILE_READ_DATA				= 0x00000001,	// Right to read data from the file. (FILE) 
	FILE_LIST_DIRECTORY			= 0x00000001,	// Right to list contents of a directory. (DIRECTORY) 
	FILE_WRITE_DATA				= 0x00000002,	// Right to write data to the file. (FILE) 
	FILE_ADD_FILE				= 0x00000002,	// Right to create a file in the directory. (DIRECTORY) 
	FILE_APPEND_DATA			= 0x00000004,	// Right to append data to the file. (FILE) 
	FILE_ADD_SUBDIRECTORY		= 0x00000004,	// Right to create a subdirectory. (DIRECTORY) 
	FILE_READ_EA				= 0x00000008,	// Right to read extended attributes. (FILE/DIRECTORY) 
	FILE_WRITE_EA				= 0x00000010,	// Right to write extended attributes. (FILE/DIRECTORY) 
	FILE_EXECUTE				= 0x00000020,	// Right to execute a file. (FILE) 
	FILE_TRAVERSE				= 0x00000020,	// Right to traverse the directory. (DIRECTORY) 
	FILE_DELETE_CHILD			= 0x00000040,	// Right to delete a directory and all the files it contains (its children), even if the files are read-only. (DIRECTORY)
	FILE_READ_ATTRIBUTES		= 0x00000080,	// Right to delete a directory and all the files it contains (its children), even if the files are read-only. (DIRECTORY)
	FILE_WRITE_ATTRIBUTES		= 0x00000100,	// Right to change file attributes. (FILE/DIRECTORY) 
	_DELETE						= 0x00010000,	// Right to delete the object. 
	READ_CONTROL				= 0x00020000,	// Right to read the information in the object's security descriptor, not including the information in the SACL, i.e. right to read the security descriptor and owner.
	WRITE_DAC					= 0x00040000,	// Right to modify the DACL in the object's security descriptor. 
	WRITE_OWNER					= 0x00080000,	// Right to change the owner in the object's security descriptor. 
	SYNCHRONIZE					= 0x00100000,	// Right to use the object for synchronization.	Enables a process to wait until the object is in the signalled state.	Some object types do not support this access right.

// The following STANDARD_RIGHTS_* are combinations of the above for convenience and are defined by the Win32 API.

// These are currently defined to READ_CONTROL. 
	STANDARD_RIGHTS_READ		= 0x00020000,
	STANDARD_RIGHTS_WRITE		= 0x00020000,
	STANDARD_RIGHTS_EXECUTE		= 0x00020000,

	STANDARD_RIGHTS_REQUIRED	= 0x000f0000,	// Combines _DELETE, READ_CONTROL, WRITE_DAC, and WRITE_OWNER access. 
	STANDARD_RIGHTS_ALL			= 0x001f0000,	// Combines _DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, and SYNCHRONIZE access.

// The access system ACL and maximum allowed access types (bits 24 to 25, bits 26 to 27 are reserved).
	ACCESS_SYSTEM_SECURITY		= 0x01000000,
	MAXIMUM_ALLOWED				= 0x02000000,

// The generic rights (bits 28 to 31).	These map onto the standard and specific rights.
	GENERIC_ALL					= 0x10000000,	// Read, write, and execute access. 
	GENERIC_EXECUTE				= 0x20000000,	// Execute access. 
	GENERIC_WRITE				= 0x40000000,	// Write access.	Maps onto: FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | FILE_WRITE_EA | STANDARD_RIGHTS_WRITE | SYNCHRONIZE
	GENERIC_READ				= 0x80000000,	// Read access.	For files, this maps onto: FILE_READ_ATTRIBUTES | FILE_READ_DATA | FILE_READ_EA | STANDARD_RIGHTS_READ | SYNCHRONIZE
};

enum _SECURITY_DESCRIPTOR_CONTROL {
	SE_OWNER_DEFAULTED			= 0x0001,
	SE_GROUP_DEFAULTED			= 0x0002,
	SE_DACL_PRESENT				= 0x0004,
	SE_DACL_DEFAULTED			= 0x0008,

	SE_SACL_PRESENT				= 0x0010,
	SE_SACL_DEFAULTED			= 0x0020,

	SE_DACL_AUTO_INHERIT_REQ	= 0x0100,
	SE_SACL_AUTO_INHERIT_REQ	= 0x0200,
	SE_DACL_AUTO_INHERITED		= 0x0400,
	SE_SACL_AUTO_INHERITED		= 0x0800,

	SE_DACL_PROTECTED			= 0x1000,
	SE_SACL_PROTECTED			= 0x2000,
	SE_RM_CONTROL_VALID			= 0x4000,
	SE_SELF_RELATIVE			= 0x8000 
};

enum _FILE_ATTRIBUTE_FLAGS {
	FILE_ATTRIBUTE_READONLY				= 0x00000001,
	FILE_ATTRIBUTE_HIDDEN				= 0x00000002,
	FILE_ATTRIBUTE_SYSTEM				= 0x00000004,
	FILE_ATTRIBUTE_DIRECTORY			= 0x00000010,	// Note, FILE_ATTRIBUTE_DIRECTORY is not considered valid in NT. It is reserved for the DOS SUBDIRECTORY flag
	FILE_ATTRIBUTE_ARCHIVE				= 0x00000020,	// Note, FILE_ATTRIBUTE_ARCHIVE is only valid/settable on files and not on * directories which always have the bit cleared
	FILE_ATTRIBUTE_DEVICE				= 0x00000040,
	FILE_ATTRIBUTE_NORMAL				= 0x00000080,

	FILE_ATTRIBUTE_TEMPORARY			= 0x00000100,
	FILE_ATTRIBUTE_SPARSE_FILE			= 0x00000200,
	FILE_ATTRIBUTE_REPARSE_POINT		= 0x00000400,
	FILE_ATTRIBUTE_COMPRESSED			= 0x00000800,

	FILE_ATTRIBUTE_OFFLINE				= 0x00001000,
	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED	= 0x00002000,
	FILE_ATTRIBUTE_ENCRYPTED			= 0x00004000,

	FILE_ATTRIBUTE_INTEGRITY_STREAM		= 0x00008000,
	FILE_ATTRIBUTE_VIRTUAL				= 0x00010000, 
	FILE_ATTRIBUTE_NO_SCRUB_DATA		= 0x00020000, 
};

#endif

typedef char16le ntfschar;

// Clusters are signed 64-bit values on NTFS volumes
typedef int64le VCN;
typedef int64le	LCNle;

//The NTFS journal $LogFile uses log sequence numbers which are signed 64-bit values
typedef int64le LSNle;

// The NTFS transaction log $UsnJrnl uses usns which are signed 64-bit values
typedef int64le USNle;

struct FILE_ID {
	static const uint64
		MFT			= 0x0001000000000000,		// Master file table (mft). Data attribute contains the entries and bitmap attribute records which ones are in use (bit==1). 
		MFTMirr		= 0x0001000000000001,		// Mft mirror: copy of first four mft records in data attribute. If cluster size > 4kiB, copy of first N mft records, with N = cluster_size / mft_record_size. 
		LogFile		= 0x0002000000000002,		// Journalling log in data attribute. 
		Volume		= 0x0003000000000003,		// Volume name attribute and volume information attribute (flags and ntfs version). Windows refers to this file as volume DASD (Direct Access Storage Device). 
		AttrDef		= 0x0004000000000004,		// Array of attribute definitions in data attribute. 
		Root		= 0x0005000000000005,		// Root directory. 
		Bitmap		= 0x0006000000000006,		// Allocation bitmap of all clusters (lcns) in data attribute. 
		Boot		= 0x0007000000000007,		// Boot sector (always at cluster 0) in data attribute. 
		BadClus		= 0x0008000000000008,		// Contains all bad clusters in the non-resident data attribute. 
		Secure		= 0x0009000000000009,		// Shared security descriptors in data attribute and two indexes into the descriptors. Appeared in Windows 2000. Before that, this file was named $Quota but was unused. 
		UpCase		= 0x000a00000000000a,		// Uppercase equivalents of all 65536 Unicode characters in data attribute. 
		Extend		= 0x000b00000000000b,		// Directory containing other system files (eg. $ObjId, $Quota, $Reparse and $UsnJrnl). This is new to NTFS3.0. 
		ObjId		= 0x0001000000000019,
		Quota		= 0x0001000000000018,
		Reparse		= 0x000100000000001a,
		UsnJrnl		= 0x0002000000012f66,
		RmMetadata	= 0x000100000000001b,
		Repair		= 0x000100000000001c,
		Txf			= 0x000100000000001e,
		TxfLog		= 0x000100000000001d,
		Tops		= 0x000100000000001f,
		TxfLog_blf	= 0x0001000000000020,
		FirstUser	= 16;						// First user file, used as test limit for whether to allow opening a file or not. 

	uint64	id;
	FILE_ID(uint64 _id) : id(_id) {}
};

// The standard NTFS_BOOT_SECTOR is on sector 0 of the partition.
// On NT4 and above there is one backup copy of the boot sector to be found on the last sector of the partition
// On versions of NT 3.51 and earlier, the backup copy was located at number of sectors/2 (integer divide), i.e. in the middle of the volume.
// BIOS parameter block (bpb) structure. 
#pragma pack(push, 1)
struct BIOS_PARAMETER_BLOCK {
	uint16le	bytes_per_sector;			// Size of a sector in bytes. 
	uint8		sectors_per_cluster;		// Size of a cluster in sectors. 
	uint16le	reserved_sectors;			// zero 
	uint8		fats;						// zero 
	uint16le	root_entries;				// zero 
	uint16le	sectors;					// zero 
	uint8		media_type;					// 0xf8 = hard disk 
	uint16le	sectors_per_fat;			// zero 
	uint16le	sectors_per_track;			// Required to boot Windows. 
	uint16le	heads;						// Required to boot Windows. 
	uint32le	hidden_sectors;				// Offset to the start of the partition relative to the disk in sectors.	Required to boot Windows. 
	uint32le	large_sectors;				// zero 
};
#pragma pack(pop)

// NTFS boot sector structure.
#pragma pack(push, 1)
struct NTFS_BOOT_SECTOR {
	static const uint64 magicNTFS = 0x202020205346544eULL;
	uint8					jump[3];					// Irrelevant (jump to boot up code).
	uint64le				oem_id;						// Magic "NTFS	". 
	BIOS_PARAMETER_BLOCK	bpb;
//unused:
	uint8					physical_drive;				// 0x80
	uint8					current_head;				// zero
	uint8					extended_boot_signature;	// 0x80
	uint8					unused;						// zero
//0x28
	int64le					number_of_sectors;			// Number of sectors in volume. Gives maximum volume size of 2^63 sectors.
	int64le					mft_lcn;					// Cluster location of mft data. 
	int64le					mftmirr_lcn;				// Cluster location of copy of mft. 
	int8					clusters_per_mft_record;	// Mft record size in clusters. 
	uint8					reserved0[3];				// zero 
	int8					clusters_per_index_block;	// Index block size in clusters. 
	uint8					reserved1[3];				// zero 
	uint64le				volume_serial_number;		// Irrelevant (serial number). 
	uint32le				checksum;					// Boot sector checksum. 
//0x54
	uint8					bootstrap[426];				// Irrelevant (boot up code). 
	uint16le				end_of_sector_marker;		// End of bootsector magic. Always is 0xaa55 in little endian. 
};// sizeof() = 512 (0x200) bytes 
#pragma pack(pop)

/* The Update Sequence Array (usa) is an array of the uint16le values which belong to the end of each sector protected by the update sequence record in which
 * this array is contained. Note that the first entry is the Update Sequence Number (usn), a cyclic counter of how many times the protected record has
 * been written to disk. The values 0 and -1 (ie. 0xffff) are not used. All last uint16le's of each sector have to be equal to the usn (during reading) or
 * are set to it (during writing). If they are not, an incomplete multi sector transfer has occurred when the data was written.
 * The maximum size for the update sequence array is fixed to:
 *	maximum size = usa_ofs + (usa_count * 2) = 510 bytes
 * The 510 bytes comes from the fact that the last uint16le in the array has to (obviously) finish before the last uint16le of the first 512-byte sector.
 * This formula can be used as a consistency check in that usa_ofs + (usa_count * 2) has to be less than or equal to 510.
 */
struct NTFS_RECORD {
	enum MAGIC {
		// Found in $MFT/$DATA. 
		FILE = 0x454c4946,					// Mft entry. 
		INDX = 0x58444e49,					// Index buffer. 
		HOLE = 0x454c4f48,					// ? (NTFS 3.0+?) 
		// Found in $LogFile/$DATA.			
		RSTR = 0x52545352,					// Restart page. 
		RCRD = 0x44524352,					// Log record page. 
		// Found in $LogFile/$DATA.	(May be found in $MFT/$DATA, also?) 
		CHKD = 0x444b4843,					// Modified by chkdsk. 
		// Found in all ntfs record containing records. 
		BAAD = 0x44414142,					// Failed multi sector transfer was detected. 
		empty = 0xffffffff					// Record is empty. 
	};
	consts<MAGIC, uint32le>	magic;
	offset_pointer<uint16le,uint16le>	usa_offset;					// Offset to the Update Sequence Array (usa) from the start of the ntfs record. 
	uint16le	usa_count;					// Number of uint16le sized entries in the usa including the Update Sequence Number (usn), thus the number of fixups is the usa_count minus 1. 

	bool PatchUSA(size_t max_size) {
		uint16le *usa = usa_offset.get(this);
		bool result = true;
		for (int i = 1; i < usa_count; i++) {
			int	offset = i * 256 - 1;
			if (offset * sizeof(uint16le) >= max_size)
				break;
			uint16le *check = (uint16le*)this + offset;
			result = result && usa[0] == *check;
			*check = usa[i];
		}
		return result;
	}
};

// mft references (aka file references or file record segment references) are used whenever a structure needs to refer to a record in the mft.
// A reference consists of a 48-bit index into the mft and a 16-bit sequence number used to detect stale references.
// The sequence number is a circular counter (skipping 0) describing how many times the referenced mft record has been (re)used.
// This has to match the sequence number of the mft record being referenced, otherwise the reference is considered stale and removed
// If the sequence number is zero it is assumed that no sequence number consistency checking should be performed.

//struct MFT_REFle {
//	uint64	ref:48, seq:16;
//};

typedef uint64	MFT_REFle;


// System defined attributes (32-bit)
// Each attribute type has a corresponding attribute name as described by the attribute definitions present in the data attribute of the $AttrDef system file.
// On NTFS 3.0 volumes the names are just as the types are named in the below defines exchanging AT_ for the dollar sign ($)
enum _ATTR_TYPE {
	AT_UNUSED						= 0,
	AT_STANDARD_INFORMATION			= 0x10,
	AT_ATTRIBUTE_LIST				= 0x20,
	AT_FILENAME						= 0x30,
	AT_OBJECT_ID					= 0x40,
	AT_SECURITY_DESCRIPTOR			= 0x50,
	AT_VOLUME_NAME					= 0x60,
	AT_VOLUME_INFORMATION			= 0x70,
	AT_DATA							= 0x80,
	AT_INDEX_ROOT					= 0x90,
	AT_INDEX_ALLOCATION				= 0xa0,
	AT_BITMAP						= 0xb0,
	AT_REPARSE_POINT				= 0xc0,
	AT_EA_INFORMATION				= 0xd0,
	AT_EA							= 0xe0,
	AT_PROPERTY_SET					= 0xf0,
	AT_LOGGED_UTILITY_STREAM		= 0x100,
	AT_FIRST_USER_DEFINED_ATTRIBUTE	= 0x1000,
	AT_END							= 0xffffffff
};
typedef consts<_ATTR_TYPE, uint32le>	ATTR_TYPE;

template<_ATTR_TYPE T> struct ATTRIBUTE;

// The collation rules for sorting views/indexes/etc (32-bit).
enum _COLLATION_RULE {
	COLLATION_BINARY				= 0x00,	// Collate by binary compare where the first byte is most significant.
	COLLATION_FILENAME				= 0x01,	// Collate Unicode strings by comparing their binary Unicode values, except that when a character can be uppercased, the upper case value collates before the lower case one.
	COLLATION_UNICODE_STRING		= 0x02,	// Collate filenames as Unicode strings
	COLLATION_NTOFS_ULONG			= 0x10,	// Sorting is done according to ascending uint32le key values
	COLLATION_NTOFS_SID				= 0x11,	// Sorting is done according to ascending SID values.
	COLLATION_NTOFS_SECURITY_HASH	= 0x12,	// Sorting is done first by ascending hash values and second by ascending security_id values
	COLLATION_NTOFS_ULONGS			= 0x13,	// Sorting is done according to a sequence of ascending uint32le key values
};
typedef consts<_COLLATION_RULE, uint32le>	COLLATION_RULE;

// The data attribute of FILE_AttrDef contains a sequence of attribute definitions for the NTFS volume.
// With this, it is supposed to be safe for an  older NTFS driver to mount a volume containing a newer NTFS version without damaging it
// Entries are sorted by attribute type. The flags describe whether the attribute can be resident/non-resident and possibly other things, but the actual bits are unknown.
struct ATTR_DEF {
	enum FLAGS {
		INDEXABLE			= 0x02,			// Attribute can be indexed. 
		MULTIPLE			= 0x04,			// Attribute type can be present multiple times in the mft records of an inode. 
		NOT_ZERO			= 0x08,			// Attribute value must contain at least one non-zero byte. 
		INDEXED_UNIQUE		= 0x10,			// Attribute must be indexed and the attribute value must be unique for the attribute type in all of the mft records of an inode. 
		NAMED_UNIQUE		= 0x20,			// Attribute must be named and the name must be unique for the attribute type in all of the mft records of an inode. 
		RESIDENT			= 0x40,			// Attribute must be resident. 
		ALWAYS_LOG			= 0x80,			// Always log modifications to this attribute, regardless of whether it is resident or non-resident.	Without this, only log modifications if the attribute is resident. 
	};
	ntfschar				name[0x40];		// Unicode name of the attribute. Zero terminated. 
	ATTR_TYPE				type;			// Type of the attribute. 
	uint32le				display_rule;	// Default display rule. FIXME: What does it mean? (AIA) 
	COLLATION_RULE			collation_rule;	// Default collation rule. 
	flags<FLAGS, uint32le>	flags;			// Flags describing the attribute. 
	int64le					min_size;		// Optional minimum attribute size. 
	int64le					max_size;		// Maximum size of attribute. 
};

struct ATTR_RECORD {
	enum FLAGS {
		IS_COMPRESSED		= 0x0001,
		COMPRESSION_MASK	= 0x00ff,		// Compression method mask.	Also, first illegal value. 
		IS_ENCRYPTED		= 0x4000,
		IS_SPARSE			= 0x8000,
	};
	ATTR_TYPE							type;				// The (32-bit) type of the attribute. 
	uint32le							length;				// Byte size of the resident part of the attribute (aligned to 8-byte boundary). Used to get to the next attribute. 
	uint8								is_non_resident;	// If 0, attribute is resident. If 1, attribute is non-resident. 
	uint8								name_length;		// Unicode character size of name of attribute. 0 if unnamed. 
	offset_pointer<wchar_t, uint16le>	name_offset;		// If name_length != 0, the byte offset to the beginning of the name from the attribute record. Note that the name is stored as a Unicode string. When creating, place offset just at the end of the record header. Then, follow with attribute value or mapping pairs array, resident and non-resident attributes respectively, aligning to an 8-byte boundary. 
	flags<FLAGS,uint16le>				flags;
	uint16le							instance;			// The instance of this attribute record. This number is unique within this mft record (see MFT_RECORD/next_attribute_instance notes in in mft.h for more details). 
	union {
		struct {// Resident attributes.		
			enum FLAGS {
				INDEXED = 0x01,								// Attribute is referenced in an index (has implications for deleting and modifying the attribute). 
			};
			uint32le						value_length;			// Byte size of attribute value. 
			offset_pointer<void, uint16le>	value_offset;			// Byte offset of the attribute value from the start of the attribute record
			uint8							flags;					
			int8							reservedR;				// Reserved/alignment to 8-byte boundary. 
		} resident;
		struct {// Non-resident attributes. 
			VCN								lowest_vcn;				// Lowest valid virtual cluster number for this portion of the attribute value or 0 if this is the only extent (usually the case). Only when an attribute list is used does lowest_vcn != 0 ever occur
			VCN								highest_vcn;			// Highest valid vcn of this extent of the attribute value. - Usually there is only one portion, so this usually equals the attribute value size in clusters minus 1. Can be -1 for zero length files. 
			offset_pointer<uint8, uint16le>	mapping_pairs_offset;	// Byte offset from the beginning of the structure to the mapping pairs array which contains the mappings between the vcns and the logical cluster numbers (lcns)
			uint8							compression_unit;		// log2 of compression unit. 0 means not compressed. WinNT4 only uses a value of 4. Sparse files have this set to 0 on XPSP2.
			uint8							reservedN[5];			// Align to 8-byte boundary. 
			// The sizes below are only used when lowest_vcn is zero, as otherwise it would be difficult to keep them up-to-date.
			int64le							allocated_size;			// Byte size of disk space allocated to hold the attribute value. Always is a multiple of the cluster size. When a file is compressed, this field is a multiple of the compression block size
			int64le							data_size;				// Byte size of the attribute value
			int64le							initialized_size;		// Byte size of initialized portion of the attribute value. Usually equals data_size
			int64le							compressed_size;		// Byte size of the attribute value after compression.	Only present when compressed or sparse
		} non_resident;
	};
	const ATTR_RECORD	*next()			const	{ return (ATTR_RECORD*)((uint8*)this + (length & 0xffff)); }	// need for mask not documented
	const wchar_t		*name()			const	{ return name_offset.get(this); }
	void				*value()		const	{ return is_non_resident ? 0 : resident.value_offset.get(this); }
	uint8				*mapping_pairs() const	{ return is_non_resident ? non_resident.mapping_pairs_offset.get(this) : 0;	}
	template<typename T> operator T*()	const	{ return (T*)value(); }
};

template<> struct ATTRIBUTE<AT_STANDARD_INFORMATION> {
	int64le			creation_time;			// Time file was created.	Updated when a filename is changed(?). 
	int64le			last_data_change_time;	// Time the data attribute was last modified. 
	int64le			last_mft_change_time;	// Time this mft record was last modified. 
	int64le			last_access_time;		// Approximate time when the file was last accessed (obviously this is not updated on read-only volumes). In Windows this is only updated when accessed if some time delta has passed since the last update. Also, last access times updates can be disabled altogether for speed. 
	uint32le		file_attributes;		// Flags describing the file. 
	union {
		struct {// NTFS 1.2
			uint8		reserved12[12];		// Reserved/alignment to 8-byte boundary. 
		}; // sizeof() = 48 bytes 
		struct {// NTFS 3.x 
			uint32le	maximum_versions;	// Maximum allowed versions for file. Zero if version numbering is disabled. 
			uint32le	version_number;		// This file's version (if any). Set to zero if maximum_versions is zero. 
			uint32le	class_id;			// Class id from bidirectional class id index (?). 
			uint32le	owner_id;			// Owner_id of the user owning the file. Translate via $Q index in FILE_Extend /$Quota to the quota control entry for the user owning the file. Zero if quotas are disabled. 
			uint32le	security_id;		// Security_id for the file. Translate via $SII index and $SDS data stream in FILE_Secure to the security descriptor. 
			uint64le	quota_charged;		// Byte size of the charge to the quota for all streams of the file. Note: Is zero if quotas are disabled. 
			USNle		usn;				// Last update sequence number of the file.	This is a direct index into the transaction log file ($UsnJrnl).	It is zero if the usn journal is disabled or this file has not been subject to logging yet.	See usnjrnl.h for details. 
		};
	};
};

template<> struct ATTRIBUTE<AT_ATTRIBUTE_LIST> {
	ATTR_TYPE						type;
	uint16le						length;			// Byte size of this entry (8-byte aligned). 
	uint8							name_length;	// Size in Unicode chars of the name of the attribute or 0 if unnamed. 
	offset_pointer<wchar_t,uint8>	name_offset;	// Byte offset to beginning of attribute name (even if unnamed). 
	VCN								lowest_vcn;		// Lowest virtual cluster number of this portion of the attribute value. Usually 0. Non-zero where one attribute does not fit into one mft record, and then each mft record holds one extent of the attribute and there is one attribute list entry for each extent.
	MFT_REFle						mft_reference;	// The reference of the mft record holding the ATTR_RECORD for this portion of the attribute value
	uint16le						instance;		// If lowest_vcn = 0, the instance of the attribute being referenced; otherwise 0

	const ATTRIBUTE<AT_ATTRIBUTE_LIST>	*next()		const	{ return (ATTRIBUTE<AT_ATTRIBUTE_LIST>*)((uint8*)this + length); }
};

template<> struct ATTRIBUTE<AT_FILENAME> {
	enum TYPE {
		POSIX			= 0x00,				// This is the largest namespace. It is case sensitive and allows all Unicode characters except for: '\0' and '/'.	Beware that in WinNT/2k/2003 by default files which eg have the same name except for their case will not be distinguished by the standard utilities and thus a "del filename" will delete both "filename" and "fileName" without warning.	However if for example Services For Unix (SFU) are installed and the case sensitive option was enabled at installation time, then you can create/access/delete such files. Note that even SFU places restrictions on the filenames beyond the '\0' and '/' and in particular the following set of characters is not allowed: '"', '/', '<', '>', '\'.	All other characters, including the ones no allowed in WIN32 namespace are allowed. Tested with SFU 3.5 (this is now free) running on Windows XP. 
		WINNT			= 0x01,				// The standard WinNT/2k NTFS long filenames. Case insensitive.	All Unicode chars except: '\0', '"', '*', '/', ':', '<', '>', '?', '\', and '|'.	Further, names cannot end with a '.' or a space
		DOS				= 0x02,				// The standard DOS filenames (8.3 format). Uppercase only.	All 8-bit characters greater space, except: '"', '*', '+', ',', '/', ':', ';', '<', '=', '>', '?', and '\'. */
		WIN32_AND_DOS	= 0x03,				// 3 means that both the Win32 and the DOS filenames are identical and hence have been saved in this single filename record. 
	};
	MFT_REFle	parent_directory;
	int64le		creation_time;
	int64le		last_data_change_time;
	int64le		last_mft_change_time;
	int64le		last_access_time;
	int64le		allocated_size;				// NOTE: Is a multiple of the cluster size. 
	int64le		data_size;					// Byte size of actual data in data attribute. 
	uint32le	file_attributes;
	union {
		struct {
			uint16le packed_ea_size;		// Size of the buffer needed to pack the extended attributes (EAs), if such are present.
			uint16le reserved;				// Reserved for alignment. 
		};
		struct {
			uint32le reparse_tag;			// Type of reparse point, present only in reparse points and only if there are no EAs. 
		};
	};
	uint8				filename_length;	// Length of filename in (Unicode) characters. 
	consts<TYPE,uint8>	filename_type;		// Namespace of the filename. 
	ntfschar			filename[0];		// Filename in Unicode. 
};

// FILE_Extend/$ObjId contains an index named $O.
// This index contains all object_ids present on the volume as the index keys and the corresponding mft_record numbers as the index entry data parts
struct OBJ_ID_INDEX_DATA {
	MFT_REFle mft_reference;				// Mft record containing the object_id in the index entry key. 
	union {
		struct {
			GUID	birth_volume_id;		// object_id of FILE_Volume on which the file was first created. Optional (i.e. can be zero).
			GUID	birth_object_id;		// object_id of file when it was first created. Usually equals the object_id. Optional (i.e. can be zero).
			GUID	domain_id;				// Reserved (always zero).
		};
		uint8	extended_info[48];
	};
};

template<> struct ATTRIBUTE<AT_OBJECT_ID> {
	GUID object_id;							// Unique id assigned to the file.
// The following fields are optional. The attribute value size is 16 bytes, i.e. sizeof(GUID), if these are not present at all. Note, the entries can be present but one or more (or all) can be zero meaning that that particular value(s) is(are) not defined. 
	union {
		struct {
			GUID birth_volume_id;			// Unique id of volume on which the file was first created.
			GUID birth_object_id;			// Unique id of file when it was first created. 
			GUID domain_id;					// Reserved, zero. 
		};
		uint8 extended_info[48];
	};
};

struct SID {
	enum {
		REVISION							= 1,
		MAX_SUB_AUTHORITIES					= 15,
		RECOMMENDED_SUB_AUTHORITIES			= 1,
	};

	enum IDENTIFIER_AUTHORITIES {					// identifier_authority = {0, 0, 0, 0, 0, X}
		AUTHORITY_NULL						= 0,	// S-1-0 
		AUTHORITY_WORLD						= 1,	// S-1-1 
		AUTHORITY_LOCAL						= 2,	// S-1-2 
		AUTHORITY_CREATOR					= 3,	// S-1-3 
		AUTHORITY_NON_UNIQUE				= 4,	// S-1-4 
		AUTHORITY_NT						= 5,	// S-1-5 
	};

	enum RELATIVE_IDENTIFIERS {
	// These relative identifiers (RIDs) are used with the above identifier authorities to make up universal well-known SIDs.
		RID_AUTHORITY_NULL					= 0,	// S-1-0 
		RID_AUTHORITY_WORLD					= 0,	// S-1-1 
		RID_AUTHORITY_LOCAL					= 0,	// S-1-2 
		RID_AUTHORITY_CREATOR_OWNER			= 0,	// S-1-3 
		RID_AUTHORITY_CREATOR_GROUP			= 1,	// S-1-3 
		RID_AUTHORITY_CREATOR_OWNER_SERVER	= 2,	// S-1-3 
		RID_AUTHORITY_CREATOR_GROUP_SERVER	= 3,	// S-1-3 
		RID_AUTHORITY_DIALUP				= 1,
		RID_AUTHORITY_NETWORK				= 2,
		RID_AUTHORITY_BATCH					= 3,
		RID_AUTHORITY_INTERACTIVE			= 4,
		RID_AUTHORITY_SERVICE				= 6,
		RID_AUTHORITY_ANONYMOUS_LOGON		= 7,
		RID_AUTHORITY_PROXY					= 8,
		RID_AUTHORITY_ENTERPRISE_CONTROLLERS= 9,
		RID_AUTHORITY_SERVER_LOGON			= 9,
		RID_AUTHORITY_PRINCIPAL_SELF		= 0xa,
		RID_AUTHORITY_AUTHENTICATED_USER	= 0xb,
		RID_AUTHORITY_RESTRICTED_CODE		= 0xc,
		RID_AUTHORITY_TERMINAL_SERVER		= 0xd,
	
		RID_AUTHORITY_LOGON_IDS				= 5,
		RID_AUTHORITY_LOGON_IDS_COUNT		= 3,
		RID_AUTHORITY_LOCAL_SYSTEM			= 0x12,
		RID_AUTHORITY_NT_NON_UNIQUE			= 0x15,
		RID_AUTHORITY_BUILTIN_DOMAIN		= 0x20,

	// Well-known domain relative sub-authority values (RIDs).

	// Users. 
		RID_DOMAIN_USER_ADMIN				= 0x1f4,
		RID_DOMAIN_USER_GUEST				= 0x1f5,
		RID_DOMAIN_USER_KRBTGT				= 0x1f6,

	// Groups. 
		RID_DOMAIN_GROUP_ADMINS				= 0x200,
		RID_DOMAIN_GROUP_USERS				= 0x201,
		RID_DOMAIN_GROUP_GUESTS				= 0x202,
		RID_DOMAIN_GROUP_COMPUTERS			= 0x203,
		RID_DOMAIN_GROUP_CONTROLLERS		= 0x204,
		RID_DOMAIN_GROUP_CERT_ADMINS		= 0x205,
		RID_DOMAIN_GROUP_SCHEMA_ADMINS		= 0x206,
		RID_DOMAIN_GROUP_ENTERPRISE_ADMINS	= 0x207,
		RID_DOMAIN_GROUP_POLICY_ADMINS		= 0x208,

	// Aliases. 
		RID_DOMAIN_ALIAS_ADMINS				= 0x220,
		RID_DOMAIN_ALIAS_USERS				= 0x221,
		RID_DOMAIN_ALIAS_GUESTS				= 0x222,
		RID_DOMAIN_ALIAS_POWER_USERS		= 0x223,

		RID_DOMAIN_ALIAS_ACCOUNT_OPS		= 0x224,
		RID_DOMAIN_ALIAS_SYSTEM_OPS			= 0x225,
		RID_DOMAIN_ALIAS_PRINT_OPS			= 0x226,
		RID_DOMAIN_ALIAS_BACKUP_OPS			= 0x227,

		RID_DOMAIN_ALIAS_REPLICATOR			= 0x228,
		RID_DOMAIN_ALIAS_RAS_SERVERS		= 0x229,
		RID_DOMAIN_ALIAS_PREW2KCOMPACCESS	= 0x22a,
	};
	uint8		revision;
	uint8		sub_authority_count;
	uint8		identifier_authority[6];
	uint32le	sub_authority[1];			// At least one sub_authority. 
};

// An ACE is an access-control entry in an access-control list (ACL).
// It defines access to an object for a specific user or group or defines the types of access that generate system-administration messages or alarms for a specific user or group.
// The user or group is identified by a security identifier (SID).
struct ACE_HEADER {
	enum TYPES {
		ACCESS_MIN_MS			= 0,
		ACCESS_ALLOWED			= 0,
		ACCESS_DENIED			= 1,
		SYSTEM_AUDIT			= 2,
		SYSTEM_ALARM			= 3,		// Not implemented as of Win2k. 
		ACCESS_MAX_MS_V2		= 3,
		ACCESS_ALLOWED_COMPOUND	= 4,
		ACCESS_MAX_MS_V3		= 4,
		// The following are Win2k only. 
		ACCESS_MIN_MS_OBJECT	= 5,
		ACCESS_ALLOWED_OBJECT	= 5,
		ACCESS_DENIED_OBJECT	= 6,
		SYSTEM_AUDIT_OBJECT		= 7,
		SYSTEM_ALARM_OBJECT		= 8,
		ACCESS_MAX_MS_OBJECT	= 8,
		ACCESS_MAX_MS_V4		= 8,
		// This one is for WinNT/2k. 
		ACCESS_MAX_MS			= 8,
	};
	enum FLAGS {
		// The inheritance flags. 
		OBJECT_INHERIT			= 0x01,
		CONTAINER_INHERIT		= 0x02,
		NO_PROPAGATE_INHERIT	= 0x04,
		INHERIT_ONLY			= 0x08,
		INHERITED				= 0x10,		// Win2k only. 
		// The audit flags. 
		SUCCESSFUL_ACCESS		= 0x40,
		FAILED_ACCESS			= 0x80,
	};

	consts<TYPES, uint8>		type;
	flags<FLAGS, uint8>			flags;
	uint16le					size;
};

// The access mask (32-bit). Defines the access rights.
// The specific rights (bits 0 to 15).	These depend on the type of the object being secured by the ACE.
typedef uint32le ACCESS_MASK;

// The generic mapping array. Used to denote the mapping of each generic access right to a specific access mask.
struct GENERIC_MAPPING {
	ACCESS_MASK		generic_read;
	ACCESS_MASK		generic_write;
	ACCESS_MASK		generic_execute;
	ACCESS_MASK		generic_all;
};

// The predefined ACE type structures
struct ACCESS_ALLOWED_ACE : ACE_HEADER {
	ACCESS_MASK		mask;					// Access mask associated with the ACE. 
	SID				sid;					// The SID associated with the ACE. 
};
typedef ACCESS_ALLOWED_ACE	ACCESS_DENIED_ACE, SYSTEM_AUDIT_ACE, SYSTEM_ALARM_ACE;

struct ACCESS_ALLOWED_OBJECT_ACE : ACE_HEADER {
	enum FLAGS {
		OBJECT_TYPE_PRESENT				= 1,
		INHERITED_OBJECT_TYPE_PRESENT	= 2,
	};
	ACCESS_MASK					mask;					// Access mask associated with the ACE. 
	iso::flags<FLAGS, uint32le>	object_flags;			// Flags describing the object ACE. 
	GUID						object_type;
	GUID						inherited_object_type;
	SID							sid;					// The SID associated with the ACE. 
};
typedef ACCESS_ALLOWED_OBJECT_ACE	ACCESS_DENIED_OBJECT_ACE, SYSTEM_AUDIT_OBJECT_ACE, SYSTEM_ALARM_OBJECT_ACE;

// An ACL is an access-control list (ACL).
// An ACL starts with an ACL header structure, which specifies the size of the ACL and the number of ACEs it contains.
// The ACL header is followed by zero or more access control entries (ACEs).
// The ACL as well as each ACE are aligned on 4-byte boundaries.
struct ACL {
	enum {
		// Current revision. 
		REVISION		= 2,
		REVISION_DS		= 4,
		// History of revisions. 
		REVISION1		= 1,
		MIN_REVISION	= 2,
		REVISION2		= 2,
		REVISION3		= 3,
		REVISION4		= 4,
		MAX_REVISION	= 4,
	} ;
	uint8		revision;
	uint8		alignment1;
	uint16le	size;						// Allocated space in bytes for ACL. Includes this header, the ACEs and the remaining free space. 
	uint16le	ace_count;
	uint16le	alignment2;
};

// The security descriptor control flags (16-bit).
typedef uint16le SECURITY_DESCRIPTOR_CONTROL;

// Self-relative security descriptor. Contains the owner and group SIDs as well as the sacl and dacl ACLs inside the security descriptor itself.
struct SECURITY_DESCRIPTOR {
	enum {
		REVISION	= 1,
	};
	uint8							revision;
	uint8							alignment;
	SECURITY_DESCRIPTOR_CONTROL		control;
	offset_pointer<SID,uint32le>	owner;
	offset_pointer<SID,uint32le>	group;
	offset_pointer<ACL,uint32le>	sacl;		// Only valid if SE_SACL_PRESENT is set in the control field. If SE_SACL_PRESENT is set but sacl is NULL, a NULL ACL is specified. 
	offset_pointer<ACL,uint32le>	dacl;		// Only valid if SE_DACL_PRESENT is set in the control field. If SE_DACL_PRESENT is set but dacl is NULL, a NULL ACL (unconditionally granting access) is specified. 
};

template<> struct ATTRIBUTE<AT_SECURITY_DESCRIPTOR> {
};

// This header precedes each security descriptor in the $SDS data stream. 
struct SDS_ENTRY {
	uint32le	hash;						// Hash of the security descriptor. 
	uint32le	security_id;				// The security_id assigned to the descriptor. 
	uint64le	offset;						// Byte offset of this entry in the $SDS stream. 
	uint32le	length;						// Size in bytes of this entry in $SDS stream. 
};

// The index entry key used in the $SII index. The collation type is COLLATION_NTOFS_ULONG.
struct SII_INDEX_KEY {
	uint32le	security_id;				// The security_id assigned to the descriptor. 
};

// The index entry data used in the $SII index is simply the security descriptor header.
struct SII_INDEX_DATA : SDS_ENTRY {};

// The index entry key used in the $SDH index. The keys are sorted first by hash and then by security_id. The collation rule is COLLATION_NTOFS_SECURITY_HASH.
struct SDH_INDEX_KEY {
	uint32le	hash;						// Hash of the security descriptor. 
	uint32le	security_id;				// The security_id assigned to the descriptor. 
};

// The index entry data used in the $SDH index. 
struct SDH_INDEX_DATA : SDS_ENTRY {
	ntfschar	magic[2];					// Effectively padding, this is always either "II" in Unicode or zero.	This field is not counted in the data_length specified by the index entry. 
};

template<> struct ATTRIBUTE<AT_VOLUME_NAME> {
	ntfschar	name[0];					// The name of the volume in Unicode. 
};

template<> struct ATTRIBUTE<AT_VOLUME_INFORMATION> {
	enum FLAGS {
		IS_DIRTY				= 0x0001,
		RESIZE_LOG_FILE			= 0x0002,
		UPGRADE_ON_MOUNT		= 0x0004,
		MOUNTED_ON_NT4			= 0x0008,
		DELETE_USN_UNDERWAY		= 0x0010,
		REPAIR_OBJECT_ID		= 0x0020,
		CHKDSK_APPLIED_FIXES	= 0x4000,
		MODIFIED_BY_CHKDSK		= 0x8000,
		FLAGS_MASK				= 0xc03f,
		// To make our life easier when checking if we must mount read-only. 
		MUST_MOUNT_RO_MASK		= 0x0022,
	};
	uint64le				reserved;
	uint8					major_ver;
	uint8					minor_ver;
	flags<FLAGS,uint16le>	flags;
};

template<> struct ATTRIBUTE<AT_DATA> {
	uint8	data[0];
};

// Predefined owner_id values (32-bit).
enum {
	QUOTA_INVALID_ID	= 0x00000000,
	QUOTA_DEFAULTS_ID	= 0x00000001,
	QUOTA_FIRST_USER_ID	= 0x00000100,
};

/* followed by one of:
union {
	FILENAME_ATTR		filename;		// $I30 index in directories. 
	SII_INDEX_KEY		sii;			// $SII index in $Secure. 
	SDH_INDEX_KEY		sdh;			// $SDH index in $Secure. 
	GUID				object_id;		// $O index in FILE_Extend/$ObjId: The object_id of the mft record found in the data part of the index. 
	REPARSE_INDEX_KEY	reparse;		// $R index in FILE_Extend/$Reparse. 
	SID					sid;			// $O index in FILE_Extend/$Quota: SID of the owner of the user_id. 
	uint32le			owner_id;		// $Q index in FILE_Extend/$Quota: user_id of the owner of the quota control entry in the data part of the index. 
};
*/

struct INDEX_HEADER {
	struct ENTRY {
		enum FLAGS {
			NODE			= 1,				// This entry contains a sub-node, i.e. a reference to an index block in form of a virtual cluster number (see below). 
			END				= 2,				// This signifies the last entry in an index block.	The index entry does not represent a file but it can point to a sub-node. 
			SPACE_FILLER	= 0xffff,			// gcc: Force enum bit width to 16-bit. 
		};
		union {									// Only valid when END is not set. 
			MFT_REFle	indexed_file;			// The mft reference of the file described by this index entry.	Used for directory indexes. 
			struct {							// Used for views/indexes to find the entry's data. 
				offset_pointer<void,uint16le>	data_offset;		// Data byte offset from this ENTRY. Follows the index key. 
				uint16le						data_length;		// Data length in bytes. 
				uint32le						reservedV;			// Reserved (zero). 
			};
		};
		uint16le				length;			// Byte size of this index entry, multiple of 8-bytes. 
		uint16le				key_length;		// Byte size of the key value, which is in the index entry. It follows field reserved. Not multiple of 8-bytes. 
		flags<FLAGS,uint16le>	flags;
		uint16le				reserved;		// Reserved/align to 8-byte boundary. 
	};
	enum FLAGS {
		// When index header is in an index root attribute:
		SMALL_INDEX = 0,					// The index is small enough to fit inside the index root attribute and there is no index allocation attribute present. 
		LARGE_INDEX = 1,					// The index is too large to fit in the index root attribute and/or an index allocation attribute is present. 
		// When index header is in an index block, i.e. is part of index allocation attribute:
		LEAF_NODE	= 0,					// This is a leaf node, i.e. there are no more nodes branching off it. 
		INDEX_NODE	= 1,					// This node indexes other nodes, i.e. it is not a leaf node. 
		NODE_MASK	= 1,					// Mask for accessing the *_NODE bits.
	};
	offset_pointer<ENTRY,uint32le>	entries_offset;	// Byte offset to first ENTRY aligned to 8-byte boundary. 
	uint32le						index_length;	// Data size of the index in bytes, i.e. bytes used from allocated size, aligned to 8-byte boundary. 
	uint32le						allocated_size;	// Byte size of this index (block), multiple of 8 bytes. 
	flags<FLAGS,uint8>				flags;
	uint8							reserved[3];	// Reserved/align to 8-byte boundary. 
};

template<> struct ATTRIBUTE<AT_INDEX_ROOT> {
	ATTR_TYPE		type;					// Type of the indexed attribute.	Is AT_FILENAME for directories, zero for view indexes.	No other values allowed. 
	COLLATION_RULE	collation_rule;			// Collation rule used to sort the index entries.	If type is AT_FILENAME, this must be COLLATION_FILENAME. 
	uint32le		index_block_size;		// Size of each index block in bytes (in the index allocation attribute). 
	int8			blocks_per_index_block;	// Number of clusters per index block (in the index allocation attribute) when index_block_size is greater or equal to the cluster size and number of sectors per index block when the index_block_size is smaller than the cluster size. 
	uint8			reserved[3];			// Reserved/align to 8-byte boundary. 
	INDEX_HEADER	index;					// Index header describing the following index entries. 
};

struct INDEX_BLOCK : NTFS_RECORD {
	int64le			lsn;					// $LogFile sequence number of the last modification of this index block. 
	VCN				index_block_vcn;		// Virtual cluster number of the index block. If the cluster_size on the volume is <= the index_block_size of the directory, index_block_vcn counts in units of clusters, and in units of sectors otherwise. 
	INDEX_HEADER	index;					// Describes the following index entries. 
};

template<> struct ATTRIBUTE<AT_INDEX_ALLOCATION> : INDEX_BLOCK {
};

struct REPARSE_INDEX_KEY {
	uint32le		reparse_tag;			// Reparse point type (inc. flags). 
	MFT_REFle		file_id;				// Mft record of the file containing the reparse point attribute. 
};

struct QUOTA_CONTROL_ENTRY {
	enum {
		VERSION	= 2,	// Current version. 
	};
	enum FLAGS {
		DEFAULT_LIMITS		= 0x00000001,
		LIMIT_REACHED		= 0x00000002,
		ID_DELETED			= 0x00000004,
		// These flags are only present in the quota defaults index entry, i.e. in the entry where owner_id = QUOTA_DEFAULTS_ID.
		TRACKING_ENABLED	= 0x00000010,
		ENFORCEMENT_ENABLED	= 0x00000020,
		TRACKING_REQUESTED	= 0x00000040,
		LOG_THRESHOLD		= 0x00000080,

		LOG_LIMIT			= 0x00000100,
		OUT_OF_DATE			= 0x00000200,
		CORRUPT				= 0x00000400,
		PENDING_DELETES		= 0x00000800,
	};
	uint32le				version;
	flags<FLAGS,uint32le>	flags;
	uint64le				bytes_used;		// How many bytes of the quota are in use. 
	int64le					change_time;	// Last time this quota entry was changed. 
	int64le					threshold;		// Soft quota (-1 if not limited). 
	int64le					limit;			// Hard quota (-1 if not limited). 
	int64le					exceeded_time;	// How long the soft quota has been exceeded. 
	SID						sid;			// The SID of the user/object associated with this quota entry
};

template<> struct ATTRIBUTE<AT_BITMAP> {
	uint8		bitmap[0];
};

template<> struct ATTRIBUTE<AT_REPARSE_POINT> {
	// These are the predefined reparse point tags:
	enum FLAGS {
		IS_ALIAS		= 0x20000000,
		IS_HIGH_LATENCY	= 0x40000000,
		IS_MICROSOFT	= 0x80000000,
		RESERVED_ZERO	= 0x00000000,
		RESERVED_ONE	= 0x00000001,
		RESERVED_RANGE	= 0x00000001,
		NSS				= 0x68000005,
		NSS_RECOVER		= 0x68000006,
		SIS				= 0x68000007,
		DFS				= 0x68000008,
		MOUNT_POINT		= 0x88000003,
		HSM				= 0xa8000004,
		SYMBOLIC_LINK	= 0xe8000000,
		VALID_VALUES	= 0xe000ffff,
	};
	flags<FLAGS,uint32le>	tag;			// Reparse point type (inc. flags). 
	uint16le				data_length;	// Byte size of reparse data. 
	uint16le				reserved;		// Align to 8-byte boundary. 
	uint8					data[0];		// Meaning depends on reparse_tag. 
};

template<> struct ATTRIBUTE<AT_EA_INFORMATION> {
	uint16le	length;						// Byte size of the packed extended attributes. 
	uint16le	need_count;					// The number of extended attributes which have the NEED_EA bit set. 
	uint32le	query_length;				// Byte size of the buffer required to query the extended attributes when calling ZwQueryEaFile() in Windows NT/2k. I.e. the byte size of the unpacked extended attributes. 
};

struct EA_ATTR {
	enum FLAGS {
		NEED_EA	= 0x80					// If set the file to which the EA belongs cannot be interpreted without understanding the associates extended attributes. 
	};
	offset_pointer<EA_ATTR,uint32le>	next_entry_offset;	// Offset to the next EA_ATTR. 
	flags<FLAGS,uint8>					flags;				// Flags describing the EA. 
	uint8								ea_name_length;		// Length of the name of the EA in bytes excluding the '\0' byte terminator. 
	uint16le							ea_value_length;	// Byte size of the EA's value. 
	char								ea_name[1];			// Name of the EA.	Note this is ASCII, not Unicode and it may or may not be zero terminated. 
//	uint8								ea_value[0];		// The value of the EA.	Immediately follows the name. 
};

template<> struct ATTRIBUTE<AT_EA> : EA_ATTR {
};

template<> struct ATTRIBUTE<AT_PROPERTY_SET> {	// feature unused. 
};

template<> struct ATTRIBUTE<AT_LOGGED_UTILITY_STREAM> {
	// anything the creator chooses. 
};

// The mft record header present at the beginning of every record in the mft.
// This is followed by a sequence of variable length attribute records which is terminated by an attribute of type AT_END which is a truncated attribute
// in that it only consists of the attribute type code AT_END and none of the other members of the attribute structure are present.

// This is the version without the NTFS 3.1+ specific fields. 
struct MFT_RECORD_OLD : NTFS_RECORD {
	enum FLAGS {
		IN_USE			= 0x0001,			// set for all in-use mft records.
		IS_DIRECTORY	= 0x0002,			// set for all directory mft records, i.e. mft records containing and index with name "$I30" indexing filename attributes.
		IN_EXTEND		= 0x0004,			// set for all system files present in the $Extend system directory.
		IS_VIEW_INDEX	= 0x0008,			// set for all system files containing one or more indices with a name other than "$I30".
		SPACE_FILLER	= 0xffff
	};

	uint64le								lsn;					// $LogFile sequence number for this record. Changed every time the record is modified. 
	uint16le								sequence_number;		// Number of times this mft record has been reused. (See description for MFT_REF above.) NOTE: The increment (skipping zero) is done when the file is deleted. NOTE: If this is zero it is left zero. 
	uint16le								link_count;				// Number of hard links, i.e. the number of directory entries referencing this record. NOTE: Only used in mft base records. NOTE: When deleting a directory entry we check the link_count and if it is 1 we delete the file. Otherwise we delete the FILENAME_ATTR being referenced by the directory entry from the mft record and decrement the link_count. FIXME: Careful with Win32 + DOS names! 
	offset_pointer<ATTR_RECORD,uint16le>	attrs_offset;			// Byte offset to the first attribute in this mft record from the start of the mft record. NOTE: Must be aligned to 8-byte boundary. 
	flags<FLAGS, uint16le>					flags;					// Bit array of FLAGS. When a file is deleted, the IN_USE flag is set to zero. 
	uint32le								bytes_in_use;			// Number of bytes used in this mft record. NOTE: Must be aligned to 8-byte boundary. 
	uint32le								bytes_allocated;		// Number of bytes allocated for this mft record. This should be equal to the mft record size. 
	MFT_REFle								base_mft_record;		// This is zero for base mft records. When it is not zero it is a mft reference pointing to the base mft record to which this record belongs (this is then used to locate the attribute list attribute present in the base record which describes this extension record and hence might need modification when the extension record itself is modified, also locating the attribute list also means finding the other potential extents, belonging to the non-base mft record). 
	uint16le								next_attr_instance;		// The instance number that will be assigned to the next attribute added to this mft record. NOTE: Incremented each time after it is used. NOTE: Every time the mft record is reused this number is set to zero.	NOTE: The first instance number is always 0. 

	typedef next_iterator<const ATTR_RECORD> iterator;
	iterator	begin()	const	{ return attrs_offset.get(this); }
	iterator	end()	const	{ return iterator((ATTR_RECORD*)((uint8*)this + bytes_in_use), -1); }

}; // sizeof() = 42 bytes 

struct MFT_RECORD : MFT_RECORD_OLD {
	uint16le	reserved;					// Reserved/alignment. 
	uint32le	mft_record_number;			// Number of this mft record. 
}; // sizeof() = 48 bytes 

//-----------------------------------------------------------------------------
// $EFS Data Structure:
//-----------------------------------------------------------------------------

struct EFS_DF_ARRAY_HEADER {
	uint32le	df_count;					// Number of data decryption/recovery fields in the array. 
};

// The header of the Logged utility stream (0x100) attribute named "$EFS".
struct EFS_ATTR_HEADER {
	uint32le	length;						// Length of EFS attribute in bytes. 
	uint32le	state;						// Always 0? 
	uint32le	version;					// Efs version.	Always 2? 
	uint32le	crypto_api_version;			// Always 0? 
	uint8		unknown4[16];				// MD5 hash of decrypted FEK? 
	uint8		unknown5[16];				// MD5 hash of DDFs? 
	uint8		unknown6[16];				// MD5 hash of DRFs? 
	offset_pointer<EFS_DF_ARRAY_HEADER, uint32le>	ddf_array;	// Offset in bytes to the array of data decryption fields (DDF)
	offset_pointer<EFS_DF_ARRAY_HEADER, uint32le>	drf_array;	// Offset in bytes to the array of data recovery fields (DRF)
	uint32le	reserved;					// Reserved. 
};


struct EFS_DF_CERTIFICATE_THUMBPRINT_HEADER {
	offset_pointer<void,uint32le>		thumbprint_offset;			// Offset in bytes to the thumbprint. 
	uint32le							thumbprint_size;			// Size of thumbprint in bytes. 
	offset_pointer<wchar_t,uint32le>	container_name_offset;		// Offset in bytes to the name of the container from start of this structure or 0 if no name present. 
	offset_pointer<wchar_t,uint32le>	provider_name_offset;		// Offset in bytes to the name of the cryptographic provider from start of this structure or 0 if no name present. 
	offset_pointer<wchar_t,uint32le>	user_name_offset;			// Offset in bytes to the user name from start of this structure or 0 if no user name present.	(This is also known as lpDisplayInformation.) 
};

typedef EFS_DF_CERTIFICATE_THUMBPRINT_HEADER EFS_DF_CERT_THUMBPRINT_HEADER;

struct EFS_DF_CREDENTIAL_HEADER {
	uint32le						cred_length;	// Length of this credential in bytes. 
	offset_pointer<SID,uint32le>	sid_offset;		// Offset in bytes to the user's sid from start of this structure.	Zero if no sid is present. 
	uint32le						type;			// Type of this credential: 1 = CryptoAPI container. 2 = Unexpected type. 3 = Certificate thumbprint. other = Unknown type. 
	union {
		struct {// CryptoAPI container. 
			offset_pointer<wchar_t,uint32le>	container_name_offset;			// Offset in bytes to the name of the container from start of this structure (may not be zero). 
			offset_pointer<wchar_t,uint32le>	provider_name_offset;			// Offset in bytes to the name of the provider from start of this structure (may not be zero). 
			offset_pointer<void,uint32le>		public_key_blob_offset;			// Offset in bytes to the public key blob from start of this structure. 
			uint32le							public_key_blob_size;			// Size in bytes of public key blob. 
		} cryptoapi_container;
			
		struct {// Certificate thumbprint. 
			uint32le	cert_thumbprint_header_size;	// Size in bytes of the header of the certificate thumbprint. 
			offset_pointer<EFS_DF_CERTIFICATE_THUMBPRINT_HEADER, uint32le>	cert_thumbprint_header_offset;	// Offset in bytes to the header of the certificate thumbprint from start of this structure. 
			uint32le	unknown1;						// Always 0?	Might be padding... 
			uint32le	unknown2;						// Always 0?	Might be padding... 
		} certificate_thumbprint;
	} credential_type;
};
typedef EFS_DF_CREDENTIAL_HEADER EFS_DF_CRED_HEADER;

struct EFS_DF_HEADER {
	uint32le						df_length;					// Length of this data decryption/recovery field in bytes. 
	offset_pointer<EFS_DF_CREDENTIAL_HEADER, uint32le>	cred_header_offset;			// Offset in bytes to the credential header. 
	uint32le						fek_size;					// Size in bytes of the encrypted file encryption key (FEK). 
	offset_pointer<void, uint32le>	fek_offset;					// Offset in bytes to the FEK from the start of the data decryption/recovery field. 
	uint32le						unknown1;					// always 0?	Might be just padding. 
};

#define INTX_BLOCK_DEVICE	0x004B4C4278746E49ULL	// "IntxBLK\0" 
#define INTX_CHAR_DEVICE	0x0052484378746E49ULL	// "IntxCHR\0" 
#define INTX_SYM_LINK		0x014B4E4C78746E49ULL	// "IntxLNK\1" 

typedef uint64 INTX_INODE_TYPES;

struct INTX_FILE {
	INTX_INODE_TYPES	magic;				// Intx inode magic. 
	union {
		// Character and block devices. 
		struct {
			uint64le	major;				// Major device number. 
			uint64le	minor;				// Minor device number. 
		} device;
		// Symbolic links. 
		ntfschar		target[0];			// The target of the symbolic link. 
	};
};

} } // namespace iso::NTFS
#endif // NTFS_H
