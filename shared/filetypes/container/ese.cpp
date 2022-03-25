#include "iso/iso_files.h"
#include "base/algorithm.h"
#include "base/bits.h"
#include "extra/date.h"

#include <Esent.h>

#pragma comment(lib, "esent")


using namespace iso;

namespace ese {

enum ERROR_DOMAINS {
	ERROR_DOMAIN_ARGUMENTS		= (int) 'a',
	ERROR_DOMAIN_CONVERSION		= (int) 'c',
	ERROR_DOMAIN_COMPRESSION	= (int) 'C',
	ERROR_DOMAIN_IO				= (int) 'I',
	ERROR_DOMAIN_INPUT			= (int) 'i',
	ERROR_DOMAIN_MEMORY			= (int) 'm',
	ERROR_DOMAIN_OUTPUT			= (int) 'o',
	ERROR_DOMAIN_RUNTIME		= (int) 'r',
};

// The argument error codes to signify errors regarding arguments passed to a function
enum ARGUMENT_ERROR {
	ARGUMENT_ERROR_GENERIC				= 0,
	ARGUMENT_ERROR_INVALID_VALUE		= 1,
	ARGUMENT_ERROR_VALUE_LESS_THAN_ZERO	= 2,
	ARGUMENT_ERROR_VALUE_ZERO_OR_LESS	= 3,
	ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM	= 4,
	ARGUMENT_ERROR_VALUE_TOO_SMALL		= 5,
	ARGUMENT_ERROR_VALUE_TOO_LARGE		= 6,
	ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS	= 7,
	ARGUMENT_ERROR_UNSUPPORTED_VALUE	= 8,
	ARGUMENT_ERROR_CONFLICTING_VALUE	= 9
};

// The conversion error codes to signify errors regarding conversions
enum CONVERSION_ERROR {
	CONVERSION_ERROR_GENERIC			= 0,
	CONVERSION_ERROR_INPUT_FAILED		= 1,
	CONVERSION_ERROR_OUTPUT_FAILED		= 2
};

// The compression error codes to signify errors regarding compression
enum COMPRESSION_ERROR {
	COMPRESSION_ERROR_GENERIC			= 0,
	COMPRESSION_ERROR_COMPRESS_FAILED	= 1,
	COMPRESSION_ERROR_DECOMPRESS_FAILED	= 2
};

// The input/output error codes to signify errors regarding input/output
enum IO_ERROR {
	IO_ERROR_GENERIC				= 0,
	IO_ERROR_OPEN_FAILED			= 1,
	IO_ERROR_CLOSE_FAILED			= 2,
	IO_ERROR_SEEK_FAILED			= 3,
	IO_ERROR_READ_FAILED			= 4,
	IO_ERROR_WRITE_FAILED			= 5,
	IO_ERROR_ACCESS_DENIED			= 6,
	IO_ERROR_INVALID_RESOURCE		= 7,
	IO_ERROR_IOCTL_FAILED			= 8,
	IO_ERROR_UNLINK_FAILED			= 9
};

// The input error codes to signify errors regarding handing input data
enum INPUT_ERROR {
	INPUT_ERROR_GENERIC				= 0,
	INPUT_ERROR_INVALID_DATA		= 1,
	INPUT_ERROR_SIGNATURE_MISMATCH	= 2,
	INPUT_ERROR_CHECKSUM_MISMATCH	= 3,
	INPUT_ERROR_VALUE_MISMATCH		= 4
};

// The memory error codes to signify errors regarding memory
enum MEMORY_ERROR {
	MEMORY_ERROR_GENERIC			= 0,
	MEMORY_ERROR_INSUFFICIENT		= 1,
	MEMORY_ERROR_COPY_FAILED		= 2,
	MEMORY_ERROR_SET_FAILED			= 3
};

// The runtime error codes to signify errors regarding runtime processing
enum RUNTIME_ERROR {
	RUNTIME_ERROR_GENERIC			= 0,
	RUNTIME_ERROR_VALUE_MISSING		= 1,
	RUNTIME_ERROR_VALUE_ALREADY_SET	= 2,
	RUNTIME_ERROR_INITIALIZE_FAILED	= 3,
	RUNTIME_ERROR_RESIZE_FAILED		= 4,
	RUNTIME_ERROR_FINALIZE_FAILED	= 5,
	RUNTIME_ERROR_GET_FAILED		= 6,
	RUNTIME_ERROR_SET_FAILED		= 7,
	RUNTIME_ERROR_APPEND_FAILED		= 8,
	RUNTIME_ERROR_COPY_FAILED		= 9,
	RUNTIME_ERROR_REMOVE_FAILED		= 10,
	RUNTIME_ERROR_PRINT_FAILED		= 11,
	RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS	= 12,
	RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM	= 13,
	RUNTIME_ERROR_UNSUPPORTED_VALUE		= 14,
	RUNTIME_ERROR_ABORT_REQUESTED		= 15
};

// The output error codes
enum OUTPUT_ERROR {
	OUTPUT_ERROR_GENERIC			= 0,
	OUTPUT_ERROR_INSUFFICIENT_SPACE	= 1
};

enum CODEPAGE {
	CODEPAGE_ASCII				= 20127,

	CODEPAGE_ISO_8859_1			= 28591,
	CODEPAGE_ISO_8859_2			= 28592,
	CODEPAGE_ISO_8859_3			= 28593,
	CODEPAGE_ISO_8859_4			= 28594,
	CODEPAGE_ISO_8859_5			= 28595,
	CODEPAGE_ISO_8859_6			= 28596,
	CODEPAGE_ISO_8859_7			= 28597,
	CODEPAGE_ISO_8859_8			= 28598,
	CODEPAGE_ISO_8859_9			= 28599,
	CODEPAGE_ISO_8859_10		= 28600,
	CODEPAGE_ISO_8859_11		= 28601,
	CODEPAGE_ISO_8859_13		= 28603,
	CODEPAGE_ISO_8859_14		= 28604,
	CODEPAGE_ISO_8859_15		= 28605,
	CODEPAGE_ISO_8859_16		= 28606,

	CODEPAGE_KOI8_R				= 20866,
	CODEPAGE_KOI8_U				= 21866,

	CODEPAGE_WINDOWS_874		= 874,
	CODEPAGE_WINDOWS_932		= 932,
	CODEPAGE_WINDOWS_936		= 936,
	CODEPAGE_WINDOWS_1250		= 1250,
	CODEPAGE_WINDOWS_1251		= 1251,
	CODEPAGE_WINDOWS_1252		= 1252,
	CODEPAGE_WINDOWS_1253		= 1253,
	CODEPAGE_WINDOWS_1254		= 1254,
	CODEPAGE_WINDOWS_1255		= 1255,
	CODEPAGE_WINDOWS_1256		= 1256,
	CODEPAGE_WINDOWS_1257		= 1257,
	CODEPAGE_WINDOWS_1258		= 1258,

//equates
	CODEPAGE_US_ASCII					= CODEPAGE_ASCII,

	CODEPAGE_ISO_WESTERN_EUROPEAN		= CODEPAGE_ISO_8859_1,
	CODEPAGE_ISO_CENTRAL_EUROPEAN		= CODEPAGE_ISO_8859_2,
	CODEPAGE_ISO_SOUTH_EUROPEAN			= CODEPAGE_ISO_8859_3,
	CODEPAGE_ISO_NORTH_EUROPEAN			= CODEPAGE_ISO_8859_4,
	CODEPAGE_ISO_CYRILLIC				= CODEPAGE_ISO_8859_5,
	CODEPAGE_ISO_ARABIC					= CODEPAGE_ISO_8859_6,
	CODEPAGE_ISO_GREEK					= CODEPAGE_ISO_8859_7,
	CODEPAGE_ISO_HEBREW					= CODEPAGE_ISO_8859_8,
	CODEPAGE_ISO_TURKISH				= CODEPAGE_ISO_8859_9,
	CODEPAGE_ISO_NORDIC					= CODEPAGE_ISO_8859_10,
	CODEPAGE_ISO_THAI					= CODEPAGE_ISO_8859_11,
	CODEPAGE_ISO_BALTIC					= CODEPAGE_ISO_8859_13,
	CODEPAGE_ISO_CELTIC					= CODEPAGE_ISO_8859_14,

	CODEPAGE_ISO_LATIN_1				= CODEPAGE_ISO_8859_1,
	CODEPAGE_ISO_LATIN_2				= CODEPAGE_ISO_8859_2,
	CODEPAGE_ISO_LATIN_3				= CODEPAGE_ISO_8859_3,
	CODEPAGE_ISO_LATIN_4				= CODEPAGE_ISO_8859_4,
	CODEPAGE_ISO_LATIN_5				= CODEPAGE_ISO_8859_9,
	CODEPAGE_ISO_LATIN_6				= CODEPAGE_ISO_8859_10,
	CODEPAGE_ISO_LATIN_7				= CODEPAGE_ISO_8859_13,
	CODEPAGE_ISO_LATIN_8				= CODEPAGE_ISO_8859_14,
	CODEPAGE_ISO_LATIN_9				= CODEPAGE_ISO_8859_15,
	CODEPAGE_ISO_LATIN_10				= CODEPAGE_ISO_8859_16,

	CODEPAGE_KOI8_RUSSIAN				= CODEPAGE_KOI8_R,
	CODEPAGE_KOI8_UKRAINIAN				= CODEPAGE_KOI8_U,

	CODEPAGE_WINDOWS_THAI				= CODEPAGE_WINDOWS_874,
	CODEPAGE_WINDOWS_JAPANESE			= CODEPAGE_WINDOWS_932,
	CODEPAGE_WINDOWS_CHINESE			= CODEPAGE_WINDOWS_936,
	CODEPAGE_WINDOWS_CENTRAL_EUROPEAN	= CODEPAGE_WINDOWS_1250,
	CODEPAGE_WINDOWS_CYRILLIC			= CODEPAGE_WINDOWS_1251,
	CODEPAGE_WINDOWS_WESTERN_EUROPEAN	= CODEPAGE_WINDOWS_1252,
	CODEPAGE_WINDOWS_GREEK				= CODEPAGE_WINDOWS_1253,
	CODEPAGE_WINDOWS_TURKISH			= CODEPAGE_WINDOWS_1254,
	CODEPAGE_WINDOWS_HEBREW				= CODEPAGE_WINDOWS_1255,
	CODEPAGE_WINDOWS_ARABIC				= CODEPAGE_WINDOWS_1256,
	CODEPAGE_WINDOWS_BALTIC				= CODEPAGE_WINDOWS_1257,
	CODEPAGE_WINDOWS_VIETNAMESE			= CODEPAGE_WINDOWS_1258,
};

enum ACCESS_FLAGS {
	ACCESS_FLAG_READ					= 0x01,
	ACCESS_FLAG_WRITE					= 0x02
};

enum FILE_TYPES {
	FILE_TYPE_DATABASE					= 0,
	FILE_TYPE_STREAMING_FILE			= 1
};

enum GET_COLUMN_FLAGS {
	GET_COLUMN_FLAG_IGNORE_TEMPLATE_TABLE	= 0x01
};

enum COLUMN_TYPES {
	COLUMN_TYPE_NULL					= 0,
	COLUMN_TYPE_BOOLEAN					= 1,
	COLUMN_TYPE_INTEGER_8BIT_UNSIGNED	= 2,
	COLUMN_TYPE_INTEGER_16BIT_SIGNED	= 3,
	COLUMN_TYPE_INTEGER_32BIT_SIGNED	= 4,
	COLUMN_TYPE_CURRENCY				= 5,
	COLUMN_TYPE_FLOAT_32BIT				= 6,
	COLUMN_TYPE_DOUBLE_64BIT			= 7,
	COLUMN_TYPE_DATE_TIME				= 8,
	COLUMN_TYPE_BINARY_DATA				= 9,
	COLUMN_TYPE_TEXT					= 10,
	COLUMN_TYPE_LARGE_BINARY_DATA		= 11,
	COLUMN_TYPE_LARGE_TEXT				= 12,
	COLUMN_TYPE_SUPER_LARGE_VALUE		= 13,
	COLUMN_TYPE_INTEGER_32BIT_UNSIGNED	= 14,
	COLUMN_TYPE_INTEGER_64BIT_SIGNED	= 15,
	COLUMN_TYPE_GUID					= 16,
	COLUMN_TYPE_INTEGER_16BIT_UNSIGNED	= 17
};

enum VALUE_FLAGS {
	VALUE_FLAG_VARIABLE_SIZE			= 0x01,
	VALUE_FLAG_COMPRESSED				= 0x02,
	VALUE_FLAG_LONG_VALUE				= 0x04,
	VALUE_FLAG_MULTI_VALUE				= 0x08,
};


//-----------------------------------------------------------------------------
//	structures
//-----------------------------------------------------------------------------

struct esedb_page_header {
	// The XOR checksum  A XOR-32 checksum calcalted over the bytes  from offset 4 to end of the page  with an initial value of 0x89abcdef   This values was changed in Exchange 2003 SP1  A XOR-32 checksum calcalted over the bytes  from offset 8 to end of the page  with an initial value of the page number
	uint32	xor_checksum;

	// The page number   This values was changed in Exchange 2003 SP1  to the ECC checksum
	union {
		uint32	page_number;
		uint32	ecc_checksum;
	};

	uint64	database_modification_time;
	uint32	previous_page;
	uint32	next_page;

	uint32	father_data_page_object_identifier;
	uint16	available_data_size;
	uint16	available_uncommitted_data_size;
	uint16	available_data_offset;
	uint16	available_page_tag;
	uint32	page_flags;
};

struct esedb_extended_page_header {
	uint64	checksum1;
	uint64	checksum2;
	uint64	checksum3;
	uint64	page_number;
	uint64	unknown1;
};

struct esedb_file_header {
	// The checksum  A XOR-32 checksum calcalted over the bytes  from offset 8 to 4096  with an initial value of 0x89abcdef
	uint32	checksum;
	uint32	signature;
	uint32	format_version;
	uint32	file_type;
	uint64	database_time;
	uint8	database_signature[28];
	uint32	database_state;
	uint64	consistent_postition;
	uint64	consistent_time;
	uint64	attach_time;
	uint64	attach_postition;
	uint64	detach_time;
	uint64	detach_postition;
	uint8	log_signature[28];
	uint32	unknown1;
	uint8	previous_full_backup[24];
	uint8	previous_incremental_backup[24];
	uint8	current_full_backup[24];
	uint32	shadowing_disabled;
	uint32	last_object_identifier;
	uint32	index_update_major_version;
	uint32	index_update_minor_version;
	uint32	index_update_build_number;
	uint32	index_update_service_pack_number;
	uint32	format_revision;
	uint32	page_size;
	uint32	repair_count;
	uint64	repair_time;
	uint8	unknown2[28];
	uint64	scrub_database_time;
	uint64	scrub_time;
	uint64	required_log;
	uint32	upgrade_exchange5_format;
	uint32	upgrade_free_pages;
	uint32	upgrade_space_map_pages;
	uint8	current_shadow_volume_backup[24];
	uint32	creation_format_version;
	uint32	creation_format_revision;
	uint8	unknown3[16];
	uint32	old_repair_count;
	uint32	ecc_fix_success_count;
	uint64	ecc_fix_success_time;
	uint32	old_ecc_fix_success_count;
	uint32	ecc_fix_error_count;
	uint64	ecc_fix_error_time;
	uint32	old_ecc_fix_error_count;
	uint32	bad_checksum_error_count;
	uint64	bad_checksum_error_time;
	uint32	old_bad_checksum_error_count;
	uint32	committed_log;
	uint8	previous_shadow_volume_backup[24];
	uint8	previous_differential_backup[24];
	uint8	unknown4[40];
	uint32	nls_major_version;
	uint32	nls_minor_version;
	uint8	unknown5[148];
	uint32	unknown_flags;
};

struct esedb_root_page_header {
	uint32	initial_number_of_pages;
	uint32	parent_father_data_page_number;
	uint32	extent_space;
	uint32	space_tree_page_number;
};

struct esedb_space_tree_page_entry {
	uint16	key_size;
	uint32	last_page_number;
	uint32	number_of_pages;
};

struct esedb_data_definition_header {
	uint8	last_fixed_size_data_type;
	uint8	last_variable_size_data_type;
	uint16	variable_size_data_types_offset;
};

struct esedb_data_definition {
	// Data type identifier: 1 (ObjidTable)  The father data page (FDP) object identifier
	uint32	father_data_page_object_identifier;
	uint16	type;
	uint32	identifier;

	union {
		uint32	father_data_page_number;
		uint32	column_type;
	};

	uint32	space_usage;
	uint32	flags;
	union {
		uint32	number_of_pages;
		uint32	codepage;
		uint32	locale_identifier;
	};

	uint8	root_flag;
	uint16	record_offset;
	uint32	lc_map_flags;
	uint16	key_most;

	// Data type identifier: 128 (Name)  The name
	// Data type identifier: 129 (Stats)
	// Data type identifier: 130 (TemplateTable)
	// Data type identifier: 131 (DefaultValue)
	// Data type identifier: 132 (KeyFldIDs)
	// Data type identifier: 133 (VarSegMac)
	// Data type identifier: 134 (ConditionalColumns)
	// Data type identifier: 135 (TupleLimits)
	// Data type identifier: 136 (Version)  Introduced in Windows Vista
	// Data type identifier: 256 (CallbackData)
	// Data type identifier: 257 (CallbackDependencies)
};

} //namespace pst

class ESEFileHandler : public FileHandler {
	const char*		GetExt() override { return "edb"; }
	const char*		GetDescription() override { return "Extensible Storage Engine Database"; }

	//ISO_ptr<void>	Read(tag id, istream_ref file) override;
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;

} esefh;

ISO_ptr<void> ESEFileHandler::ReadWithFilename(tag id, const filename &fn) {

	JET_INSTANCE	inst = 0;
	JET_ERR			err;
	JET_SESID		sesid;
	JET_DBID		dbid;

	uint32			page_size;
	err	= JetGetDatabaseFileInfoA(fn, &page_size, sizeof(page_size), JET_DbInfoPageSize);
	err	= JetSetSystemParameter(&inst, JET_sesidNil, JET_paramDatabasePageSize, page_size, NULL);
	err	= JetSetSystemParameter(&inst, JET_sesidNil, JET_paramRecovery, 0, 0);

	err	= JetCreateInstance(&inst, "instance");
	err	= JetInit(&inst);
	err	= JetBeginSession(inst, &sesid, 0, 0);

#if 0
// Retrieve a column from the record. Here we move to the first record with JetMove. By using
// JetMoveNext it is possible to iterate through all records in a table. Use JetMakeKey and
// JetSeek to move to a particular record.
JetMove(sesid, tableid, JET_MoveFirst, 0));
char buffer[1024];
JetRetrieveColumn(sesid, tableid, columnid, buffer, sizeof(buffer), NULL, 0, NULL));
printf("%s", buffer);
#endif

	err = JetAttachDatabaseA(sesid, fn, JET_bitDbReadOnly);
	err	= JetOpenDatabaseA(sesid, fn, 0, &dbid, JET_bitDbReadOnly);

	JET_TABLEID tableid;

	err	= JetOpenTable(sesid, dbid, "Folders", 0, 0, 0, &tableid);

	uint32		count;
	err = JetGetTableIndexInfo(sesid, tableid, NULL, &count, sizeof(count), JET_IdxInfoCount);

	/*
	JET_ERR JET_API JetEnumerateColumns(
sesid,
tableid,
  __in          unsigned long cEnumColumnId,
  __in_opt      JET_ENUMCOLUMNID* rgEnumColumnId,
  __out         unsigned long* pcEnumColumn,
  __out         JET_ENUMCOLUMN** prgEnumColumn,
  __in          JET_PFNREALLOC pfnRealloc,
  __in          void* pvReallocContext,
  __in          unsigned long cbDataMost,
  __in          JET_GRBIT grbit
);
*/


//	JET_HANDLE		h;
//	unsigned long	size_lo, size_hi;
//	err = JetOpenFileA(fn, &h, &size_lo, &size_hi);

	//JetCloseTable(sesid, tableid);
	JetEndSession(sesid, 0);
	JetTerm(inst);

	return ISO_NULL;
}

class WLMFileHandler : public ESEFileHandler {
	const char*		GetExt() override { return "MSMessageStore"; }
	const char*		GetDescription() override { return "Windows Live Mail Storage"; }
} wlmms;
