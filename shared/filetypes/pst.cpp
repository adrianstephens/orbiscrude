#include "iso/iso_files.h"
#include "base/algorithm.h"
#include "base/bits.h"
#include "extra/date.h"
#include "windows/win_file.h"

using namespace iso;

namespace pst {

//-----------------------------------------------------------------------------
//	structures
//-----------------------------------------------------------------------------

typedef uint32		nid;
typedef uint32		hid;
typedef uint32		hnid;

typedef uint16		prop_id;
typedef uint32		row_id;

template<typename T> struct bref {
	T	bid;
	T	ib;
};

enum prop_ids {
#undef PID
#undef LID
#define PID(name, val)	pid_##name = val,
#define LID(name, val)	lid_##name = val,
#include "pid.h"
};

enum nid_type {
	nid_type_none							= 0x00,
	nid_type_internal						= 0x01,
	nid_type_folder							= 0x02,
	nid_type_search_folder					= 0x03,
	nid_type_message						= 0x04,
	nid_type_attachment						= 0x05,
	nid_type_search_update_queue			= 0x06,
	nid_type_search_criteria_object			= 0x07,
	nid_type_associated_message				= 0x08,
	nid_type_storage						= 0x09,
	nid_type_contents_table_index			= 0x0A,
	nid_type_receive_folder_table			= 0x0B,
	nid_type_outgoing_queue_table			= 0x0C,
	nid_type_hierarchy_table				= 0x0D,
	nid_type_contents_table					= 0x0E,
	nid_type_associated_contents_table		= 0x0F,
	nid_type_search_contents_table			= 0x10,
	nid_type_attachment_table				= 0x11,
	nid_type_recipient_table				= 0x12,
	nid_type_search_table_index				= 0x13,
	nid_type_contents_smp					= 0x14,
	nid_type_associated_contents_smp		= 0x15,
	nid_type_change_history_table			= 0x16,
	nid_type_tombstone_table				= 0x17,
	nid_type_tombstone_date_table			= 0x18,
	nid_type_lrep_dups_table				= 0x19,
	nid_type_folder_path_tombstone_table	= 0x1A,
	nid_type_ltp							= 0x1F,
	nid_type_max							= 0x20
};

const uint32 nid_type_mask = 0x1f;
#define make_nid(nid_type,nid_index)	(((nid_type)&nid_type_mask)|((nid_index) << 5))
#define make_prv_pub_nid(nid_index)		(make_nid(nid_type_folder, nix_prv_pub_base + (nid_index)))

enum predefined_nid {
	nix_message_store						= 0x1,		// The property bag for this file
	nix_name_id_map							= 0x3,		// Contains the named prop mappings
	nix_normal_folder_template				= 0x6,
	nix_search_folder_template				= 0x7,
	nix_root								= 0x9,		// Root folder of the store
	nix_search_management_queue				= 0xF,
	nix_search_activity_list				= 0x10,
	nix_search_domain_alternative			= 0x12,
	nix_search_domain_object				= 0x13,
	nix_search_gatherer_queue				= 0x14,
	nix_search_gatherer_descriptor			= 0x15,
	nix_table_rebuild_queue					= 0x17,
	nix_junk_mail_pihsl						= 0x18,
	nix_search_gatherer_folder_queue		= 0x19,
	nix_tc_sub_props						= 0x27,
	nix_template							= 0x30,
	nix_receive_folders						= 0x31,
	nix_outgoing_queue						= 0x32,
	nix_attachment_table					= 0x33,
	nix_recipient_table						= 0x34,
	nix_change_history_table				= 0x35,
	nix_tombstone_table						= 0x36,
	nix_tombstone_date_table				= 0x37,
	nix_all_message_search_folder			= 0x39,		// deprecated The GUST
	nix_all_message_search_contents			= 0x39,
	nix_lrep_gmp							= 0x40,
	nix_lrep_folders_smp					= 0x41,
	nix_lrep_folders_table					= 0x42,
	nix_folder_path_tombstone_table			= 0x43,
	nix_hst_hmp								= 0x60,
	nix_prv_pub_base						= 0x100,

	nid_message_store						= make_nid(nid_type_internal,					nix_message_store				),
	nid_name_id_map							= make_nid(nid_type_internal,					nix_name_id_map					),
	nid_normal_folder_template				= make_nid(nid_type_folder,						nix_normal_folder_template		),
	nid_search_folder_template				= make_nid(nid_type_search_folder,				nix_search_folder_template		),
	nid_root_folder							= make_nid(nid_type_folder,						nix_root						),
	nid_search_management_queue				= make_nid(nid_type_internal,					nix_search_management_queue		),
	nid_search_activity_list				= make_nid(nid_type_internal,					nix_search_activity_list		),
	nid_search_domain_alternative			= make_nid(nid_type_internal,					nix_search_domain_alternative	),
	nid_search_domain_object				= make_nid(nid_type_internal,					nix_search_domain_object		),
	nid_search_gatherer_queue				= make_nid(nid_type_internal,					nix_search_gatherer_queue		),
	nid_search_gatherer_descriptor			= make_nid(nid_type_internal,					nix_search_gatherer_descriptor	),
	nid_table_rebuild_queue					= make_nid(nid_type_internal,					nix_table_rebuild_queue			),
	nid_junk_mail_pihsl						= make_nid(nid_type_internal,					nix_junk_mail_pihsl				),
	nid_search_gatherer_folder_queue		= make_nid(nid_type_internal,					nix_search_gatherer_folder_queue),
	nid_tc_sub_props						= make_nid(nid_type_internal,					nix_tc_sub_props				),

	nid_hierarchy_table_template			= make_nid(nid_type_hierarchy_table,			nix_template					),
	nid_contents_table_template				= make_nid(nid_type_contents_table,				nix_template					),
	nid_associated_contents_table_template	= make_nid(nid_type_associated_contents_table,	nix_template					),
	nid_search_contents_table_template		= make_nid(nid_type_search_contents_table,		nix_template					),
	nid_smp_template						= make_nid(nid_type_contents_smp,				nix_template					),
	nid_tombstone_table_template			= make_nid(nid_type_tombstone_table,			nix_template					),
	nid_lrep_dups_table_template			= make_nid(nid_type_lrep_dups_table,			nix_template					),
	nid_receive_folders						= make_nid(nid_type_receive_folder_table,		nix_receive_folders				),
	nid_outgoing_queue						= make_nid(nid_type_outgoing_queue_table,		nix_outgoing_queue				),
	nid_attachment_table					= make_nid(nid_type_attachment_table,			nix_attachment_table			),
	nid_recipient_table						= make_nid(nid_type_recipient_table,			nix_recipient_table				),
	nid_change_history_table				= make_nid(nid_type_change_history_table,		nix_change_history_table		),
	nid_tombstone_table						= make_nid(nid_type_tombstone_table,			nix_tombstone_table				),
	nid_tombstone_date_table				= make_nid(nid_type_tombstone_date_table,		nix_tombstone_date_table		),
	nid_all_message_search_folder			= make_nid(nid_type_search_folder,				nix_all_message_search_folder	), // deprecated The GUST
	nid_all_message_search_contents			= make_nid(nid_type_search_contents_table,		nix_all_message_search_contents	),
	nid_lrep_gmp							= make_nid(nid_type_internal,					nix_lrep_gmp					),
	nid_lrep_folders_smp					= make_nid(nid_type_internal,					nix_lrep_folders_smp			),
	nid_lrep_folders_table					= make_nid(nid_type_internal,					nix_lrep_folders_table			),
	nid_folder_path_tombstone_table			= make_nid(nid_type_internal,					nix_folder_path_tombstone_table	),
	nid_hst_hmp								= make_nid(nid_type_internal,					nix_hst_hmp						),

	nid_pub_root_folder						= make_nid(nid_type_folder,						nix_prv_pub_base + 0			),
	nid_prv_root_folder						= make_nid(nid_type_folder,						nix_prv_pub_base + 5			),

	nid_criterr_notification				= make_nid(nid_type_internal,					0x3FD							),
	nid_object_notification					= make_nid(nid_type_internal,					0x3FE							),
	nid_newemail_notification				= make_nid(nid_type_internal,					0x3FF							),
	nid_extended_notification				= make_nid(nid_type_internal,					0x400							),
	nid_indexing_notification				= make_nid(nid_type_internal,					0x401							)
};

#undef make_nid
#undef make_prv_pub_nid

inline nid		make_nid(nid_type t, uint32 i)	{ return t | (i << 5); }
inline nid_type get_nid_type(nid id)			{ return nid_type(id & nid_type_mask); }
inline uint32	get_nid_index(nid id)			{ return id >> 5; }
inline uint32	get_heap_page(hid id)			{ return id >> 16; }
inline uint32	get_heap_index(hid id)			{ return ((id >> 5) - 1) & 0x7ff; }
inline bool		is_heap_id(hnid id)				{ return get_nid_type(id) == nid_type_none; }
inline bool		is_subnode_id(hnid id)			{ return get_nid_type(id) != nid_type_none; }

// properties
enum prop_type {
	prop_type_mv					= 4096,
	prop_type_unspecified			= 0,
	prop_type_null					= 1,
	prop_type_int16					= 2,
	prop_type_int32					= 3,
	prop_type_float					= 4,
	prop_type_double				= 5,
	prop_type_currency				= 6,	//8 bytes
	prop_type_apptime				= 7,	//VT_DATE (double: days since midnight 30 December 1899)
	prop_type_error					= 10,	//SCODE == LONG
	prop_type_bool					= 11,
	prop_type_object				= 13,
	prop_type_int64					= 20,
	prop_type_string				= 30,
	prop_type_wstring				= 31,
	prop_type_systime				= 64,	//FILETIME=LONGLONG
	prop_type_guid					= 72,
	prop_type_binary				= 258,
};

#if 0
enum prop_ids {
	pid_ExchangeRemoteHeader		= 0x0E2A,	//prop_type_bool:	Has Exchange Remote Header

	pid_ReplItemid					= 0x0E30,	//prop_type_int32:	Replication Item ID.
	pid_ReplChangenum				= 0x0E33,	//prop_type_int64:	Replication Change Number.
	pid_ReplVersionHistory			= 0x0E34,	//prop_type_binary:	Replication version history
	pid_ReplFlags					= 0x0E38,	//prop_type_int32:	Replication flags.
	pid_ReplCopiedfromVersionhistory= 0x0E3C,	//prop_type_binary:	Replication version information
	pid_ReplCopiedfromItemid		= 0x0E3D,	//prop_type_binary:	Replication item ID information

	pid_ItemTemporaryFlags			= 0x1097,	//prop_type_int32:	Temporary flags
	pid_IpmSubTreeEntryId			= 0x35E0,	//prop_type_binary:	EntryID of the Root Mailbox Folder object
	pid_IpmWastebasketEntryId		= 0x35E3,	//prop_type_binary:	EntryID of the Deleted Items Folder object
	pid_FinderEntryId				= 0x35E7,	//prop_type_binary:	EntryID of the search Folder object
	pid_AssociatedMessageCount		= 0x3617,

	pid_SecureSubmitFlags			= 0x65C6,	//prop_type_int32:	Secure submit flags

	pid_PstBodyPrefix				= 0x6619,	//prop_type_string
	pid_PstBestBodyProptag			= 0x661D,	//prop_type_int32
	pid_PstLrNoRestrictions			= 0x6633,	//prop_type_bool
	pid_PstHiddenCount				= 0x6635,	//prop_type_int32:	Total number of hidden Items in sub-Folder object.
	pid_PstHiddenUnread				= 0x6636,	//prop_type_int32:	Unread hidden items in sub-Folder object.
	pid_PstLatestEnsure				= 0x66FA,	//prop_type_int32
	pid_PstIpmsubTreeDescendant		= 0x6705,	//prop_type_bool
	pid_PstSubTreeContainer			= 0x6772,	//prop_type_int32
	pid_PstPassword					= 0x67FF,	//prop_type_int32

	pid_SendOutlookRecallReport		= 0x6803,	//prop_type_bool:	Send recall report.
	pid_MapiFormComposeCommand		= 0x682F,	//prop_type_wstring
};
#endif
struct entry_id {
	uint32	flags;
	GUID	uid;
	nid		loc;
};

// mapi recipient type
enum recipient_type {
	mapi_to		= 1,
	mapi_cc		= 2,
	mapi_bcc	= 3
};

// message specific values
const uint8 message_subject_prefix_lead_byte = 0x01;

const GUID ps_none						= {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
const GUID ps_mapi						= {0x00020328, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID ps_public_strings			= {0x00020329, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID ps_internet_headers			= {0x00020386, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_appointment			= {0x00062002, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_task					= {0x00062003, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_address				= {0x00062004, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_common				= {0x00062008, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_log					= {0x0006200A, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_note					= {0x0006200E, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_sharing				= {0x00062040, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_postrss				= {0x00062041, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID psetid_meeting				= {0x6ED8DA90, 0x450B, 0x101B, {0x98, 0xDA, 0x00, 0xAA, 0x00, 0x3F, 0x13, 0x05}};
const GUID psetid_messaging				= {0x41F28F13, 0x83F4, 0x4114, {0xA5, 0x84, 0xEE, 0xDB, 0x5A, 0x6B, 0x0B, 0xFF}};
const GUID psetid_unifiedmessaging		= {0x4442858E, 0xA9E3, 0x4E80, {0xB9, 0x00, 0x31, 0x7A, 0x21, 0x0C, 0xC1, 0x5B}};
const GUID psetid_airsync				= {0x71035549, 0x0739, 0x4DCB, {0x91, 0x63, 0x00, 0xF0, 0x58, 0x0D, 0xBB, 0xDF}};
const GUID psetid_xmlextractedentities	= {0x23239608, 0x685D, 0x4732, {0x9C, 0x55, 0x4C, 0x95, 0xCB, 0x4E, 0x8E, 0x33}};

// The root of the database
template<typename T> struct root {
	uint32		orphans;	// The number of "orphans" in the BBT
	T			filelen;	// EOF of the file, according the header
	T			AMapLast;	// The location of the last valid AMap page
	T			AMapFree;	// Amount of space free in all AMap pages
	T			PMapFree;	// Amount of space free in all PMap pages
	bref<T>		NBT;		// The location of the root of the NBT
	bref<T>		BBT;		// The location of the root of the BBT
	uint8		AMapValid;	// Indicates if the AMap pages are valid or not
	uint8		ARvec;		// Indicates which AddRef vector is used
	uint16		nARvec;		// Number of elements in the AddRef vector
};

struct header0 {
	enum {
		magic_head		= 0x4e444221,
		magic_pst		= 0x4D53,
		magic_ost		= 0x4F53,
	};
	enum database_format {
		fmt_ansi_min	= 14, // Initial ANSI file version number
		fmt_ansi		= 15, // Current ANSI file version number
		fmt_unicode_min = 20, // Initial unicode version number
		fmt_unicode		= 23, // Current unicode version number
	};
	enum database_type {
		type_ost		= 12, // A OST file
		type_pst		= 19, // A PST file
	};
	enum crypt_method {
		crypt_none		= 0, // No "encryption" was used.
		crypt_permute	= 1, // The permute method is used in this file.
		crypt_cyclic	= 2, // The cyclic method is used in this file.
	};

	uint32			magic;				// Always hlmagic
	uint32			crc_partial;
	uint16			magic_client;		// Client magic number, eg pst_magic
	uint16			ver;				// database_format
	uint16			ver_client;			// database_type
	uint8			platform_create;	// Always 0x1
	uint8			platform_access;	// Always 0x1
	uint32			open_bid;			// Implementation specific
	uint32			open_claim_id;		// Implementation specific
};

template<typename T> struct header;
template<> struct header<uint64> : header0 {
	typedef uint32	row_ix;

	uint64			unused;
	uint64			next_pid;			// The page id counter
//	uint64			next_bid;			// The block id counter
	uint32			unique;
	nid				id[nid_type_max];	// Array of nid counters, one per node type
	root<uint64>	root_info;			// The root info for this database
	uint8			FMap[128];			// The header's FMap entries
	uint8			FPMap[128];			// The header's FPMap entries
	uint8			sentinel;			// Sentinel uint8 indicating the end of the headers FPMap
	uint8			crypt;				// The crypt_method used in this file
	uint8			reserved[2];		// Unused
	packed<uint64>	next_bid;			// The block id counter
	uint32			crc_full;
	uint8			version_encoded[3];
	uint8			lock_semaphore;		// Implementation specific
	uint8			lock[32];			// Implementation specific
};

// The ANSI header structure
template<> struct header<uint32> : header0 {
	typedef uint16	row_ix;

	uint32			next_bid;				//**
	uint32			next_pid;
	uint32			unique;
	nid				id[nid_type_max];
	root<uint32>	root_info;
	uint8			FMap[128];
	uint8			FPMap[128];
	uint8			sentinel;
	uint8			crypt;
	uint8			reserved1[2];
	uint64			reserved2;
	uint32			reserved3;
	uint8			version_encoded[3];
	uint8			lock_semaphore;
	uint8			lock[32];
};

template<typename T> struct header_crc_locations;
template<> struct header_crc_locations<uint32> {
	static const size_t start			= iso_offset(header<uint32>, magic_client);
	static const size_t end				= iso_offset(header<uint32>, lock_semaphore);
	static const size_t length			= end - start;
};
template<> struct header_crc_locations<uint64> {
	static const size_t partial_start	= iso_offset(header<uint64>, magic_client);
	static const size_t partial_end		= iso_offset(header<uint64>, lock_semaphore);
	static const size_t partial_length	= partial_end - partial_start;
	static const size_t full_start		= iso_offset(header<uint64>, magic_client);
	static const size_t full_end		= iso_offset(header<uint64>, crc_full);
	static const size_t full_length		= full_end - full_start;
};

// Data tables used by permute and cyclic
const uint8 table1[] = {
	 65,  54,  19,  98, 168,  33, 110, 187,	244,  22, 204,   4, 127, 100, 232,  93,
	 30, 242, 203,  42, 116, 197,  94,  53,	210, 149,  71, 158, 150,  45, 154, 136,
	 76, 125, 132,  63, 219, 172,  49, 182,	 72,  95, 246, 196, 216,  57, 139, 231,
	 35,  59,  56, 142, 200, 193, 223,  37,	177,  32, 165,  70,  96,  78, 156, 251,
	170, 211,  86,  81,  69, 124,  85,   0,	  7, 201,  43, 157, 133, 155,   9, 160,
	143, 173, 179,  15,  99, 171, 137,  75,	215, 167,  21,  90, 113, 102,  66, 191,
	 38,  74, 107, 152, 250, 234, 119,  83,	178, 112,   5,  44, 253,  89,  58, 134,
	126, 206,   6, 235, 130, 120,  87, 199,	141,  67, 175, 180,  28, 212,  91, 205,
	226, 233,  39,  79, 195,   8, 114, 128,	207, 176, 239, 245,  40, 109, 190,  48,
	 77,  52, 146, 213,  14,  60,  34,  50,	229, 228, 249, 159, 194, 209,  10, 129,
	 18, 225, 238, 145, 131, 118, 227, 151,	230,  97, 138,  23, 121, 164, 183, 220,
	144, 122,  92, 140,   2, 166, 202, 105,	222,  80,  26,  17, 147, 185,  82, 135,
	 88, 252, 237,  29,  55,  73,  27, 106,	224,  41,  51, 153, 189, 108, 217, 148,
	243,  64,  84, 111, 240, 198, 115, 184,	214,  62, 101,  24,  68,  31, 221, 103,
	 16, 241,  12,  25, 236, 174,   3, 161,	 20, 123, 169,  11, 255, 248, 163, 192,
	162,   1, 247,  46, 188,  36, 104, 117,	 13, 254, 186,  47, 181, 208, 218,  61,
};
const uint8 table2[] = {
	 20,  83,  15,  86, 179, 200, 122, 156,	235, 101,  72,  23,  22,  21, 159,   2,
	204,  84, 124, 131,   0,  13,  12,  11,	162,  98, 168, 118, 219, 217, 237, 199,
	197, 164, 220, 172, 133, 116, 214, 208,	167, 155, 174, 154, 150, 113, 102, 195,
	 99, 153, 184, 221, 115, 146, 142, 132,	125, 165,  94, 209,  93, 147, 177,  87,
	 81,  80, 128, 137,  82, 148,  79,  78,	 10, 107, 188, 141, 127, 110,  71,  70,
	 65,  64,  68,   1,  17, 203,   3,  63,	247, 244, 225, 169, 143,  60,  58, 249,
	251, 240,  25,  48, 130,   9,  46, 201,	157, 160, 134,  73, 238, 111,  77, 109,
	196,  45, 129,  52,  37, 135,  27, 136,	170, 252,   6, 161,  18,  56, 253,  76,
	 66, 114, 100,  19,  55,  36, 106, 117,	119,  67, 255, 230, 180,  75,  54,  92,
	228, 216,  53,  61,  69, 185,  44, 236,	183,  49,  43,  41,   7, 104, 163,  14,
	105, 123,  24, 158,  33,  57, 190,  40,	 26,  91, 120, 245,  35, 202,  42, 176,
	175,  62, 254,   4, 140, 231, 229, 152,	 50, 149, 211, 246,  74, 232, 166, 234,
	233, 243, 213,  47, 112,  32, 242,  31,	  5, 103, 173,  85,  16, 206, 205, 227,
	 39,  59, 218, 186, 215, 194,  38, 212,	145,  29, 210,  28,  34,  51, 248, 250,
	241,  90, 239, 207, 144, 182, 139, 181,	189, 192, 191,   8, 151,  30, 108, 226,
	 97, 224, 198, 193,  89, 171, 187,  88,	222,  95, 223,  96, 121, 126, 178, 138,
};
const uint8 table3[] = {
	 71, 241, 180, 230,  11, 106, 114,  72,	133,  78, 158, 235, 226, 248, 148,  83,
	224, 187, 160,   2, 232,  90,   9, 171,	219, 227, 186, 198, 124, 195,  16, 221,
	 57,   5, 150,  48, 245,  55,  96, 130,	140, 201,  19,  74, 107,  29, 243, 251,
	143,  38, 151, 202, 145,  23,   1, 196,	 50,  45, 110,  49, 149, 255, 217,  35,
	209,   0,  94, 121, 220,  68,  59,  26,	 40, 197,  97,  87,  32, 144,  61, 131,
	185,  67, 190, 103, 210,  70,  66, 118,	192, 109,  91, 126, 178,  15,  22,  41,
	 60, 169,   3,  84,  13, 218,  93, 223,	246, 183, 199,  98, 205, 141,   6, 211,
	105,  92, 134, 214,  20, 247, 165, 102,	117, 172, 177, 233,  69,  33, 112,  12,
	135, 159, 116, 164,  34,  76, 111, 191,	 31,  86, 170,  46, 179, 120,  51,  80,
	176, 163, 146, 188, 207,  25,  28, 167,	 99, 203,  30,  77,  62,  75,  27, 155,
	 79, 231, 240, 238, 173,  58, 181,  89,	  4, 234,  64,  85,  37,  81, 229, 122,
	137,  56, 104,  82, 123, 252,  39, 174,	215, 189, 250,   7, 244, 204, 142,  95,
	239,  53, 156, 132,  43,  21, 213, 119,	 52,  73, 182,  18,  10, 127, 113, 136,
	253, 157,  24,  65, 125, 147, 216,  88,	 44, 206, 254,  36, 175, 222, 184,  54,
	200, 161, 128, 166, 153, 152, 168,  47,	 14, 129, 101, 115, 228, 194, 162, 138,
	212, 225,  17, 208,   8, 139,  42, 242,	237, 154, 100,  63, 193, 108, 249, 236
};

uint16 compute_signature(uint32 id, uint32 address)	{
	uint32 value = address ^ id;
	return uint16((value >> 16) ^ value);
}
template<typename T> uint16 compute_signature(const bref<T> &r) {
	return compute_signature(uint32(r.bid), uint32(r.ib));
}

void permute(void *pdata, uint32 cb, bool encrypt) {
	const uint8	*ptable = encrypt ? table1 : table3;
	for (uint8 *p = (uint8*)pdata; cb--; p++)
		*p = ptable[*p];
}

void cyclic(void *pdata, uint32 cb, uint32 key) {
	uint16 w = (uint16)(key ^ (key >> 16));
	for (uint8 *p = (uint8*)pdata; cb--; p++, w++) {
		uint8 b = *p;
		b	= table1[uint8(b + w)];
		b	= table2[uint8(b + (w >> 8))];
		b	= table3[uint8(b - (w >> 8))];
		*p	= uint8(b - w);
	}
}

const size_t page_size	= 512;
template<typename E, int B> struct fillwith {
	union {
		E		entries[B / sizeof(E)];
		uint8	_[B];
	};
	E		&operator[](int i)			{ return entries[i]; }
	const E &operator[](int i) const	{ return entries[i]; }
};
template<int B> struct fillwith<void, B> {
	uint8	data[B];
};

// Valid page types
enum page_type {
	page_type_bbt	= 0x80,		// A BBT (Blocks BTree) page
	page_type_nbt	= 0x81,		// A NBT (Nodes BTree) page
	page_type_fmap	= 0x82,		// A FMap (Free Map) page
	page_type_pmap	= 0x83,		// A PMap (Page Map) page
	page_type_amap	= 0x84,		// An AMap (Allocation Map) page
	page_type_fpmap	= 0x85,		// A FPMap (Free Page Map) page. Unicode stores only.
	page_type_dlist	= 0x86		// A DList (Density List) page
};

template<typename T> struct page_trailer;
template<> struct page_trailer<uint64> {
	uint8	page_type;			// The page_type of this page
	uint8	page_type_repeat;	// Same as the page_type field, for validation purposes
	uint16	signature;			// Signature of this page, as calculated by the compute_signature function
	uint32	crc;				// CRC of this page, as calculated by the compute_crc function
	uint64	bid;				// The id of this page
};
template<> struct page_trailer<uint32> {
	uint8	page_type;
	uint8	page_type_repeat;
	uint16	signature;
	uint32	bid;
	uint32	crc;
};

const size_t dlist_page_location		= 0x4200;
const size_t first_amap_page_location	= 0x4400;

template<typename T> struct page : fillwith<void, page_size - sizeof(page_trailer<T>)>, page_trailer<T>	{};

template<typename T> struct dlist_page {
	static const size_t extra_space = page_size - sizeof(page_trailer<T>) - 8;
	uint8	flags;					// Flags indicating the state of the dlist page
	uint8	num_entries;			// Number of entries in the entries array
	union {
		uint32 current_page;		// The current AMap page used for allocations
		uint32 backfill_location;	// The current backfill marker, when backfilling
	};
	union {
		uint32 entries[extra_space / sizeof(uint32)];// Each entry has bits for the amap page (ordinal) and free space (slots)
		uint8 _ignore[extra_space];
	};
	page_trailer<T>	trailer;		// The page trailer
};

const uint32	dlist_page_num_mask		= 0x0000FFFF;
const uint32	dlist_slots_shift		= 20;
inline uint32	dlist_get_page_num(uint32 entry)	{ return entry & dlist_page_num_mask; }
inline uint32	dlist_get_slots(uint32 entry)		{ return entry >> dlist_slots_shift; }

struct bt_page_info {
	uint8	num_entries;		// Number of entries on this page
	uint8	max_entries;		// Maximum number of entries on this page
	uint8	entry_size;			// The size of each entry
	uint8	level;				// The level of this page. A level of zero indicates a leaf.
};
template<typename T> struct	bt_page_trailer : bt_page_info, page_trailer<T> {};
template<typename T, typename E=void> struct bt_page : fillwith<E, page_size - sizeof(bt_page_trailer<T>)>, bt_page_trailer<T> {};

template<typename T> struct bt_entry {
	T		key;				// The key of the page in ref
	bref<T> ref;				// A reference to a lower level page
	operator T() const { return key; }
};
template<typename T> struct nbt_leaf_entry {
	T		id;					// Subnode id
	T		bid;				// Data block of this subnode
	T		sub_bid;			// Subnode block of this subnode
	nid		parent_nid;			// The parent node id
	operator T() const { return id; }
};
template<typename T> struct bbt_leaf_entry {
	bref<T>	ref;				// A reference to this block on disk
	uint16	size;				// The unaligned size of this block
	uint16	ref_count;			// The reference count of this block
	operator T() const { return ref.bid; }
};

//
// block structures
//
const size_t max_block_disk_size = 8 * 1024;
enum block_types {
	block_type_external	= 0x00, // An external data block
	block_type_extended	= 0x01, // An extended block type
	block_type_sub		= 0x02	// A subnode block type
};

inline size_t align_slot(size_t size)				{ return align(size, 64); }
template<typename T> size_t align_disk(size_t size)	{ return align_slot(size + sizeof(block_trailer<T>)); }

enum bid_flags {
	bid_attached_bit	= 1,
	bid_internal_bit	= 2,
	bid_increment		= 4,
};
template<typename T> bool bid_is_external(T bid) { return ((bid & bid_internal_bit) == 0); }
template<typename T> bool bid_is_internal(T bid) { return !bid_is_external(bid); }

template<typename T> struct block_trailer;
template<> struct block_trailer<uint64> {
	uint16	cb;					// Size of the block (unaligned)
	uint16	signature;			// Signature of this block, as calculated by the compute_signature function
	uint32	crc;				// CRC of this block, as calculated by the compute_crc function
	uint64	bid;				// The id of this block
};
template<> struct block_trailer<uint32> {
	uint16	cb;
	uint16	signature;
	uint32	bid;
	uint32	crc;
};
struct internal_block_header {
	uint8	block_type;
	uint8	level;
	uint16	count;
};

template<typename T> struct extended_block : internal_block_header {
	uint32	total_size;
	T		bid[];
};

template<typename T> struct sub_leaf_entry {
	T		id;					// Subnode id
	T		bid;				// Data block of this subnode
	T		sub_bid;			// Subnode block of this subnode
	operator nid() const { return nid(id); }
};
template<typename T> struct sub_nonleaf_entry {
	T		id;					// Key of the subnode block
	T		bid;				// Id of the subnode block
	operator nid() const { return nid(id); }
};
template<typename T, typename E> struct sub_block : internal_block_header {
	E		entry[];
};

//
// heap structures
//
const uint32	heap_max_alloc_size			= 3580;
const uint32	heap_max_alloc_size_wver_14 = 3068;

enum heap_client_signature {
	heap_sig_gmp	= 0x6C,		// Internal
	heap_sig_tc		= 0x7C,		// Table context
	heap_sig_smp	= 0x8C,		// Internal
	heap_sig_hmp	= 0x9C,		// Internal
	heap_sig_ch		= 0xA5,		// Internal
	heap_sig_chtc	= 0xAC,		// Internal
	heap_sig_bth	= 0xB5,		// BTree on Heap
	heap_sig_pc		= 0xBC,		// Property Context
	heap_signature	= 0xEC
};
struct heap_page {
	enum fill_level {
		fill_empty,				// >= 3584 bytes free
		fill_1,					// 2560 - 3583 bytes free
		fill_2,					// 2048 - 2559 bytes free
		fill_3,					// 1792 - 2047 bytes free
		fill_4,					// 1536 - 1791 bytes free
		fill_5,					// 1280 - 1535 bytes free
		fill_6,					// 1024 - 1279 bytes free
		fill_7,					// 768 - 1023 bytes free
		fill_8,					// 512 - 767 bytes free
		fill_9,					// 256 - 511 bytes free
		fill_10,				// 128 - 255 bytes free
		fill_11,				// 64 - 127 bytes free
		fill_12,				// 32 - 63 bytes free
		fill_13,				// 16 - 31 bytes free
		fill_14,				// 8 - 15 bytes free
		fill_full				// < 8 bytes free
	};
	struct map {
		uint16		num_allocs;		// Number of allocations on this block
		uint16		num_frees;		// Number of empty allocations on this
		uint16		allocs[];		// The offset of each allocation
	};
	uint16		page_map_offset;	// Offset of the start of the heap_page_map

	static fill_level	get_fill_level(const uint8 *p, int i)	{ return fill_level(p[i / 2] >> ((i & 1) << 4)); }
	void*				offset(uint16 o)	const				{ return ((char*)this + o); }
	map*				get_map()			const				{ return (map*)offset(page_map_offset); }
	memory_block		get_entry(int i)	const				{ map *m = get_map(); return memory_block(offset(m->allocs[i]), m->allocs[i + 1] - m->allocs[i]); }
};
struct heap_header : heap_page {
	uint8		signature;				// Always heap_signature
	uint8		client_signature;		// Client defined signature, see heap_client_signature
	hid			root_id;				// Root allocation. This has specific meaning to the owner of this heap.
	uint8		page_fill_levels[4];	// Fill level of this and next seven heap blocks
	fill_level	fill_level(int i)	const	{ return get_fill_level(page_fill_levels, i); }
};
struct heap_page_fill : heap_page {
	uint8		page_fill_levels[64];	// Fill level of this and next 127 heap blocks
	fill_level	fill_level(int i)	const	{ return get_fill_level(page_fill_levels, i); }
};

//
// bth structures
//
struct bth_header {
	uint8	signature;		// Always heap_sig_bth
	uint8	key_size;		// Key size in bytes
	uint8	entry_size;		// Entry size in bytes
	uint8	num_levels;		// Number of levels
	hid		root;			// Root of the actual tree structure
};
template<typename K> struct bth_nonleaf_entry {
	K		key;			// Key of the lower level page
	hid		page;			// Heap id of the lower level page
	operator K() const { return key; }
};
template<typename K, typename V> struct bth_leaf_entry {
	K		key;
	V		value;
	operator K() const { return key; }
};

//
// pc structures
//
struct prop_entry {
	uint16		type;		// Property type
	packed<hnid> id;		// Heapnode id for variable length properties, or the value directly for fixed size property types
};
template<typename T> struct mv_header {
	uint32		count;
	T			values[];
};
struct object {
	nid			id;			// The subnode id containing the data for the object
	uint32		size;		// The size of the object
};

//
// tc structures
//
struct tc_header {
	enum {
		four,		// Offset of the end of the four and eight byte columns
		two,		// Offset of the end of the two uint8 columns
		one,		// Offset of the end of the one uint8 columns
		ceb,		// Offset of the end of the existance bitmap
	};
	struct col {
		uint16		type;		// Column property type
		prop_id		id;			// Column property id
		uint16		offset;		// Offset into the row
		uint8		size;		// Width of the column
		uint8		bit_offset;	// Bit offset into the existance bitmap
	};

	uint8			signature;			// TC signature, heap_sig_tc
	uint8			num_columns;		// Number of columns in this table
	uint16			size_offsets[4];	// Row offset array, see tc_offsets
	packed<hid>		row_btree_id;		// The bth_header allocation for the row mapping btree
	packed<hnid>	row_matrix_id;		// The hnid allocation for the row matrix
	uint8			unused[4];
	col				columns[];	// Column description array, of length num_columns
};

template<typename T> struct tcrow_header {
	row_id		id;
	typename header<T>::row_ix	ix;
};

//deprecated outlook 2007 SP1 version
struct gust_header {
	enum {
		four,
		two,
		one,
		ceb,
	};
	struct col {
		uint16		type;
		prop_id		id;
		uint16		offset;
		uint16		size;
		uint16		bit_offset;
		uint16		unused2;
		nid			data_subnode;
	};
	uint8		signature;
	uint8		unused1;
	uint16		size_offsets[4];
	hid			row_btree_id;
	hnid		row_matrix_id;
	uint8		unused2[4];
	uint16		num_columns;
	nid			column_subnode;
	uint32		unused3;
	uint32		unused4;
	col			columns[];
};

struct name_entry {
	enum {
		guid_none			= 0x0000,	//No GUID (N=1).
		guid_mapi			= 0x0001,	//The GUID is PS_MAPI
		guid_public_strings	= 0x0002,	//The GUID is PS_PUBLIC_STRINGS
	};
	union {
		uint32	id;		// id of the named prop (for named props defined by an id)
		uint32	offset;	// offset into the string stream of the name of this prop
		uint32	hash;	// For numeric named props, this is just the id. Hash value of string props.
	};
	uint16	N:1,guid:15, prop;
};

//-----------------------------------------------------------------------------
//	higher level
//-----------------------------------------------------------------------------

struct malloc_block_rc : refs<malloc_block_rc>, malloc_block {
	malloc_block_rc(size_t size) : malloc_block(size)	{}
};

struct memory_ref : memory_block {
	ref_ptr<malloc_block_rc>	cache;
	memory_ref()																			{}
	memory_ref(const memory_ref &m) : memory_block((const memory_block&)m), cache(m.cache)	{}
	memory_ref(const memory_block &m, malloc_block_rc *c = 0) : memory_block(m), cache(c)	{}
	memory_ref	slice(intptr_t a, intptr_t b)	const	{ return memory_ref(memory_block::slice(a, b), cache); }
};

template<typename T> struct hold_ref {
	ref_ptr<malloc_block_rc>	cache;
	const T						*p;
	hold_ref() : p(0)	{}
	hold_ref(const memory_ref &m)				: cache(m.cache), p(m)	{}
	hold_ref(const T *_p, malloc_block_rc *c)	: cache(c), p(_p)		{}
	void operator=(const memory_ref &m) { cache = m.cache; p = m; }
	operator	const T*()		const { return p; }
	const T*	operator->()	const { return p; }
	const T*	ptr()			const { return p; }
};

typedef	memory_ref binary;

struct string {
	memory_ref	m;
	string()	{}
	string(const memory_ref &_m)	: m(_m) {}
	const char		*begin()	const { return m; }
	size_t			length()	const { return m.length(); }
	operator iso::string()		const { return iso::str(begin(), length()); }
	string			sub_str(int a, int b = 0) const	{ return m.slice(a, b); }
};
struct wstring {
	memory_ref	m;
	wstring()	{}
	wstring(const memory_ref &_m)	: m(_m) {}
	const char16	*begin()	const { return m; }
	size_t			length()	const { return m.length() / 2; }
	operator iso::string()		const { return iso::str(begin(), length()); }
	operator iso::string16()	const { return iso::str(begin(), length()); }
	wstring			sub_str(int a, int b = 0) const	{ return m.slice(a * 2, b * 2); }
};
string_accum& operator<<(string_accum &a, const string &s)	{ return a.merge(s.begin(), s.length()); }
string_accum& operator<<(string_accum &a, const wstring &s)	{ return a.merge(s.begin(), s.length()); }

template<typename T> T get_mv_entry(const memory_ref &mb, int i) {
	return ((mv_header<T>*)mb)->values[i];
}
template<> memory_ref get_mv_entry<memory_ref>(const memory_ref &mb, int i) {
	mv_header<uint32>	*h = mb;
	return mb.slice(h->values[i], (i + 1 == h->count ? mb.length() : h->values[i + 1]) - h->values[i]);
}
template<> string get_mv_entry<string>(const memory_ref &mb, int i) {
	return get_mv_entry<memory_ref>(mb, i);
}
template<> wstring get_mv_entry<wstring>(const memory_ref &mb, int i) {
	return get_mv_entry<memory_ref>(mb, i);
}
template<typename T> struct multivalue : memory_ref {
	uint32		count() const			{ return ((mv_header<T>*)start())->count; }
	T			operator[](int i) const	{ return get_mv_entry(*this, i); }
};

struct type_traits_var						{ enum { packsize = 1<<16};		};
template<typename T> struct type_traits_fix	{ enum { packsize = sizeof(T)}; };
template<typename T> struct type_traits		: type_traits_fix<T>			{};
template<typename T> struct type_traits<multivalue<T> >	: type_traits_var	{ enum { prop_type = type_traits<T>::prop_type | prop_type_mv}; };
template<> struct type_traits<string>		: type_traits_var				{ enum { prop_type = prop_type_string	}; };
template<> struct type_traits<wstring>		: type_traits_var				{ enum { prop_type = prop_type_wstring	}; };
template<> struct type_traits<binary>		: type_traits_var				{ enum { prop_type = prop_type_binary	}; };
template<> struct type_traits<int16>		: type_traits_fix<int16>		{ enum { prop_type = prop_type_int16	}; };
template<> struct type_traits<int32>		: type_traits_fix<int32>		{ enum { prop_type = prop_type_int32	}; };
template<> struct type_traits<float>		: type_traits_fix<float>		{ enum { prop_type = prop_type_float	}; };
template<> struct type_traits<double>		: type_traits_fix<double>		{ enum { prop_type = prop_type_double	}; };
template<> struct type_traits<bool>			: type_traits_fix<bool>			{ enum { prop_type = prop_type_bool		}; };
template<> struct type_traits<int64>		: type_traits_fix<int64>		{ enum { prop_type = prop_type_int64	}; };
template<> struct type_traits<FILETIME>		: type_traits_fix<FILETIME>		{ enum { prop_type = prop_type_systime	}; };
template<> struct type_traits<GUID>			: type_traits_fix<GUID>			{ enum { prop_type = prop_type_guid		}; };

size_t get_prop_size(prop_type type) {
	switch (type) {
		case prop_type_int16:	return sizeof(uint16);
		case prop_type_int32:	return sizeof(uint32);
		case prop_type_float:	return sizeof(float);
		case prop_type_double:	return sizeof(double);
		case prop_type_currency:return 8;
		case prop_type_apptime:	return sizeof(double);
		case prop_type_error:	return sizeof(uint32);
		case prop_type_bool:	return 1;
		case prop_type_int64:	return sizeof(uint64);
		case prop_type_systime:	return sizeof(uint64);
		case prop_type_guid:	return sizeof(GUID);
		default:				return 0;
	}
}

//-----------------------------------------------------------------------------
//	pst0
//-----------------------------------------------------------------------------

template<typename T> struct pst0 : header<T> {
	istream_ref			file;

	template<typename I, typename L> hold_ref<L> find(const bref<T> &root, nid id) const {
#if 1
		malloc_block_rc	*c	= new malloc_block_rc(page_size);
		bt_page<T>		*p	= *c;
		file.seek(root.ib);
		file.read(*p);
		while (p->level > 0) {
			const I	*i	= upper_bound((I*)p->data, (I*)p->data + p->num_entries, id) - 1;
			file.seek(i->ref.ib);
			file.read(*p);
		}
		L	*e = lower_bound((L*)p->data, (L*)p->data + p->num_entries, id);
		if (e == (L*)p->data + p->num_entries)
			e = 0;
		return  hold_ref<L>(e, c);
#else
		bt_page<T>	p;
		file.seek(root.ib);
		file.read(p);
		while (p.level > 0) {
			const I	*i	= upper_bound((I*)p.data, (I*)p.data + p.num_entries, id) - 1;
			file.seek(i->ref.ib);
			file.read(p);
		}
		return  hold_ref<L>(lower_bound((L*)p.data, (L*)p.data + p.num_entries, id), 0);
#endif
	}

	pst0(istream_ref _file, const header<T> &h) : header<T>(h), file(_file) {}

	hold_ref<nbt_leaf_entry<T> >	find_node(nid id) const {
		return find<bt_entry<T>, nbt_leaf_entry<T> >(root_info.NBT, id);
	}
	hold_ref<bbt_leaf_entry<T> >	find_data(nid id) const {
		return find<bt_entry<T>, bbt_leaf_entry<T> >(root_info.BBT, id);
	}

	memory_ref	get_block(nid id) const;
};

template<typename T> memory_ref pst0<T>::get_block(nid id) const {
	hold_ref<bbt_leaf_entry<T> >	d = find_data(id & ~bid_attached_bit);

	if (!d)
		return memory_ref();

	uint32	size		= d->size;
	size_t	block_size	= align_disk<T>(size);
	malloc_block_rc	*c	= new malloc_block_rc(block_size);
	void			*b	= c->begin();

	file.seek(d->ref.ib);
	file.readbuff(b, block_size);

	if (bid_is_external(id)) switch (crypt) {
		case crypt_permute:	permute(b, size, false);	break;
		case crypt_cyclic:	cyclic(b, size, id);		break;
	}
	return memory_ref(c->slice(intptr_t(0), size), c);
}

//-----------------------------------------------------------------------------
//	memory
//-----------------------------------------------------------------------------

struct memory : dynamic_array<memory_ref> {
	uint64	total;
	memory() : total(0) {}
	memory(const memory_ref &m) : dynamic_array<memory_ref>(&m, &m + 1) {}
	template<typename T> memory(const pst0<T> *pst, nid id);
};

template<typename T> memory::memory(const pst0<T> *pst, nid id) {
	memory_ref	m = pst->get_block(id);
	if (bid_is_external(id)) {
		total	= m.length();
		push_back(m);
		return;
	}

	extended_block<T>	*x = m;
	total	= x->total_size;

	if (x->level == 1) {
		reserve(x->count);
		for (int i = 0; i < x->count; ++i)
			push_back(pst->get_block(x->bid[i]));
		return;
	}

	for (int i = 0; i < x->count; ++i) {
		memory_ref			m2 = pst->get_block(x->bid[i]);
		extended_block<T>	*x2 = m2;
		for (int j = 0; j < x2->count; ++j)
			push_back(pst->get_block(x2->bid[j]));
	}
}

//-----------------------------------------------------------------------------
//	node
//-----------------------------------------------------------------------------

struct node : memory {
	hold_ref<internal_block_header>	subtree;

	node() {}
	node(const memory_ref &m) : memory(m) {}

	template<typename T> node(const pst0<T> *pst, const nbt_leaf_entry<T> *e) : memory(pst, e->bid) {
		if (e->sub_bid)
			subtree = pst->get_block(e->sub_bid);
	}
	template<typename T> node(const pst0<T> *pst, const sub_leaf_entry<T> *e) : memory(pst, e->bid) {
		if (e->sub_bid)
			subtree = pst->get_block(e->sub_bid);
	}

	template<typename T> hold_ref<sub_leaf_entry<T> > find_subnode(const pst0<T> *pst, nid id) {
		typedef	sub_nonleaf_entry<T>	I;
		typedef	sub_leaf_entry<T>		L;

		if (!subtree)
			return hold_ref<L>(0, 0);

		hold_ref<internal_block_header>	p = subtree;
		while (p->level > 0) {
			auto		*b = (sub_block<T, I>*)p.ptr();
			const	I	*i	= upper_bound(b->entry, b->entry + b->count, id) - 1;
			p = pst->get_block(i->bid);
		}
		auto	*b = (sub_block<T, L>*)p.ptr();
		L		*e = lower_bound(b->entry, b->entry + p->count, id);
		if (e == b->entry + p->count)
			e = 0;
		return hold_ref<L>(e, p.cache);
	}

	template<typename T> node	get_subnode(const pst0<T> *pst, nid id) {
		return node(pst, find_subnode(pst, id).ptr());
	}

	operator memory_ref() const {
		if (size() == 1)
			return (*this)[0];
		malloc_block_rc	*c	= new malloc_block_rc(uint32(total));
		uint8			*p	= *c;
		for (const memory_ref *i = begin(); i != end(); ++i)
			p += i->copy_to(p);
		return memory_ref(*c, c);
	}
};

//-----------------------------------------------------------------------------
//	heap
//-----------------------------------------------------------------------------

struct heap {
	node	n;
	heap() {}
	template<typename T> heap(const pst0<T> *pst, const nbt_leaf_entry<T> *e) : n(pst, e) {}
	template<typename T> heap(const pst0<T> *pst, const sub_leaf_entry<T> *e) : n(pst, e) {}

	template<typename T> hold_ref<sub_leaf_entry<T> > find_subnode(pst0<T> *pst, nid id) {
		return n.find_subnode(pst, id);
	}

	heap_header*	get_header() {
		return n[0];
	}
	memory_ref		get_entry(hid id) {
		if (!id)
			return memory_ref();
		const memory_ref &page = n[get_heap_page(id)];
		return memory_ref(((heap_page*)page)->get_entry(get_heap_index(id)), page.cache);
	}
	template<typename T> memory_ref	get_data1(const pst0<T> *pst, hnid id) {
		return is_heap_id(id) ? get_entry(id) : (memory_ref)n.get_subnode(pst, id);
	}
	template<typename T> node	get_data(const pst0<T> *pst, hnid id) {
		return is_heap_id(id) ? node(get_entry(id)) : n.get_subnode(pst, id);
	}
};

//-----------------------------------------------------------------------------
//	btree_on_heap
//-----------------------------------------------------------------------------

struct btree_on_heap {
	heap		*hp;
	bth_header	*header;

	btree_on_heap() : header(0)	{}
	btree_on_heap(heap *_hp, bth_header *_header) : hp(_hp), header(_header) {}
	btree_on_heap(heap *_hp) : hp(_hp), header(hp->get_entry(hp->get_header()->root_id)) {}

	template<typename V, typename K> V *find(K key) const {
		typedef	bth_nonleaf_entry<K>	I;
		typedef	bth_leaf_entry<K, V>	L;

		hid			h		= header->root;
		for (int n = header->num_levels; n--;) {
			memory_block	b	= hp->get_entry(h);
			I	*i	= upper_bound((I*)b, (I*)b.end(), key) - 1;
			h = i->page;
		}
		memory_block	b		= hp->get_entry(h);
		L				*leaf	= lower_bound((L*)b, (L*)b.end(), key);
		return leaf->key == key ? &leaf->value : 0;
	}

	template<typename V, typename K> struct iterator {
		typedef	bth_nonleaf_entry<K>	I;
		typedef	bth_leaf_entry<K, V>	L;

		template<typename T> struct node {
			T *e[2], *p;
			void set(const memory_block &m, bool end) { e[0] = m; e[1] = (T*)m.end(); p = e[end]; }
		};

		heap		*hp;
		node<L>		leaf;
		node<I>		stack[8], *tos;

		iterator(const btree_on_heap *bth, bool end) : hp(bth->hp), tos(stack + bth->header->num_levels) {
			hid		h	= bth->header->root;
			for (node<I> *i = tos; i-- > stack; h = i->p->page)
				i->set(hp->get_entry(h), end);
			leaf.set(hp->get_entry(h), end);
		}
		iterator&	next(int dir) {
			if ((leaf.p += dir) == leaf.e[dir > 0]) {
				node<I> *i = stack;
				while (i < tos && (i->p += dir) == i->e[dir > 0])
					++i;
				if (i < tos) {
					while (i > stack) {
						hid h = i->p->page;
						(--i)->set(hp->get_entry(h), dir < 0);
					}
					leaf.set(hp->get_entry(i->p->page), dir < 0);
				}
			}
			return *this;
		}

		iterator&	operator++()	{ return next(+1);	}
		iterator&	operator--()	{ return next(-1);	}
		K			key()	const	{ return *leaf.p;	}
		operator	V*()	const	{ return &leaf.p->value; }
	};
};

//-----------------------------------------------------------------------------
//	property_context
//-----------------------------------------------------------------------------

struct property_context : btree_on_heap {
	typedef prop_entry	*entry;

	property_context() {}
	property_context(heap *_hp) : btree_on_heap(_hp) {}

	template<bool imm> struct getter {
		template<typename R, typename T> static R	get(const property_context *pc, pst0<T> *pst, entry e) { return *(R*)&e->id; }
	};

	entry find(prop_id id) const {
		return btree_on_heap::find<prop_entry>(id);
	}
	template<typename T> memory_ref		get_value(pst0<T> *pst, entry e) const {
		if (!e)
			return memory_ref();
		if (!(e->type & prop_type_mv)) {
			size_t size = get_prop_size(prop_type(e->type));
			if (size && size <= sizeof(e->id))
				return memory_block(&e->id, size);
		}
		return hp->get_data1(pst, e->id);
	}

	template<typename R> static R		get(entry e, R def = R())			{ return e ? *(R*)&e->id : def; }
	template<typename R> R				get(prop_id id, R def = R()) const	{ return get(find(id), def); 	}
	template<typename R, typename T> R	get(pst0<T> *pst, entry e, R def = R())	const {
		return e ? getter<(type_traits<R>::packsize <= 4)>::get<R>(this, pst, e) : def;
	}
	template<typename R, typename T> R	get(pst0<T> *pst, prop_id id, R def = R()) const {
		return get<R>(pst, find(id), def);
	}

	typedef btree_on_heap::iterator<prop_entry, prop_id> iterator;
	iterator	begin()	const { return iterator(this, false); }
	iterator	end()	const { return iterator(this, true); }
};

template<> struct property_context::getter<false> {
	template<typename R, typename T> static R	get(const property_context *pc, pst0<T> *pst, entry e) { return pc->hp->get_data1(pst, e->id); }
};

struct property_context2 : heap, property_context {
	property_context2()	{}
	template<typename T> property_context2(const pst0<T> *pst, const nbt_leaf_entry<T> *n) : heap(pst, n), property_context(this)	{}
	template<typename T> property_context2(const pst0<T> *pst, const sub_leaf_entry<T> *n) : heap(pst, n), property_context(this)	{}
	property_context2(const property_context2 &b) : heap(b), property_context(b)	{ hp = this; }
};

//-----------------------------------------------------------------------------
//	table_context
//-----------------------------------------------------------------------------

struct table_context {
	tc_header		*header;
	btree_on_heap	row_tree;
	node			row_matrix;
	uint32			row_size;
	uint32			rows_per_block;
	uint32			num_rows;

	typedef tc_header::col	col;

	uint32		num_cols()				const { return header->num_columns; }
	const col*	get_col(int i)			const { return header->columns + i;	}
	const col*	get_col(prop_ids id)	const {
		const col	*c = header->columns;
		for (int n = header->num_columns; n--; ++c) {
			if (c->id == id)
				return c;
		}
		return 0;
	}

	struct entry : memory_ref {
		prop_type type;
		entry() : type(prop_type_unspecified) {}
		entry(prop_type _type, const memory_ref &m) : memory_ref(m), type(_type) {}
	};

	struct row : memory_ref {
		struct iterator {
			const table_context	*tc;
			const row	&r;
			const col	*c;
			iterator(const table_context *_tc, const row &_r, const col *_c) : tc(_tc), r(_r), c(_c) {}
			entry		operator*()		const			{ return r.get_entry(tc, c);	}
			iterator&	operator++()					{ ++c; return *this; }
			iterator&	operator--()					{ --c; return *this; }
			bool		operator==(const iterator &b)	{ return c == b.c; }
			bool		operator!=(const iterator &b)	{ return c != b.c; }
			bool		exists()		const			{ return r.exists(tc, c);	}
			prop_type	type()			const			{ return prop_type(c->type); }
			prop_id		id()			const			{ return c->id; }
		};
		row(const memory_ref &m) : memory_ref(m) {}

		int		num_exist(const table_context *tc) const {
			return bits_count_set<uint8>((uint8*)p + tc->header->size_offsets[tc_header::one], (uint8*)p + tc->row_size);
		}
		const col	*existing_col(const table_context *tc, int i) const {
			for (const col *c = tc->header->columns, *e = c + tc->header->num_columns; c < e; c++) {
				if (exists(tc, c) && !i--)
					return c;
			}
			return 0;
		}
		bool	exists(const table_context *tc, const col *c) const {
			return !!(((uint8*)p)[tc->header->size_offsets[tc_header::one] + c->bit_offset / 8] & (0x80 >> (c->bit_offset & 7)));
		}
		entry	get_entry(const table_context *tc, const col *c) const {
			return c && exists(tc, c) ? entry(prop_type(c->type), slice(c->offset, c->size)) : entry();
		}
		entry	get_entry(const table_context *tc, prop_ids id)	const { return get_entry(tc, tc->get_col(id)); }
		uint32	get_rowid()	const	{ return ((tcrow_header<uint64>*)p)->id; }
		uint32	get_rowix()	const	{ return ((tcrow_header<uint64>*)p)->ix; }
		row*	operator->()		{ return this; }
	};

	struct iterator {
		const table_context	*tc;
		uint32		i;
		iterator(const table_context *_tc, uint32 _i) : tc(_tc), i(_i) {}
		row			operator*()		const			{ return tc->get_row(i); }
		row			operator->()	const			{ return tc->get_row(i); }
		iterator&	operator++()					{ ++i; return *this; }
		iterator&	operator--()					{ --i; return *this; }
		bool		operator==(const iterator &b)	{ return i == b.i; }
		bool		operator!=(const iterator &b)	{ return i != b.i; }
	};

	template<bool imm> struct getter {
		template<typename R, typename T> static R	get(const table_context *tc, pst0<T> *pst, entry e) { return *(R*)e; }
	};

	table_context()	: header(0) {}
	template<typename T> table_context(const pst0<T> *pst, heap *hp)
		: header(hp->get_entry(hp->get_header()->root_id))
		, row_tree(hp, hp->get_entry(header->row_btree_id))
		, row_matrix(hp->get_data(pst, header->row_matrix_id))
		, row_size(header->size_offsets[tc_header::ceb])
		, rows_per_block(uint32((8192 - sizeof(block_trailer<uint64>)) / row_size))
		, num_rows(uint32((row_matrix.size() - 1) * rows_per_block + row_matrix.back().length() / row_size))
	{}

	uint32		count()					const { return num_rows; }
	row			get_row(uint32 i)		const { return row_matrix[i / rows_per_block].slice((i % rows_per_block) * row_size, row_size); }
	uint32		get_row_index(row_id id)const { return *row_tree.find<uint32>(id);	}

	template<typename T> memory_ref		get_value(pst0<T> *pst, const entry &e) const {
		if (!e)
			return e;
		if (!(e.type & prop_type_mv)) {
			size_t size = get_prop_size(prop_type(e.type & 0xfff));
			if (size && size <= 8)
				return e;
		}
		return row_tree.hp->get_data1(pst, *(hnid*)e);
	}
	template<typename R> static R		get(entry e, R def = R())							{ return e ? *(R*)e : def; }
	template<typename R, typename T> R	get(pst0<T> *pst, entry e, R def = R())		const	{
		return e ? getter<type_traits<R>::packsize <= 8>::template get<R>(this, pst, e) : def;
	}
	template<typename R> R				get(const row &r, const col *c, R def = R()) const	{ return get(r.get_entry(this, c), def); }
	template<typename R, typename T> R	get(pst0<T> *pst, const row &r, const col *c, R def = R()) const {
		return get(pst, r.get_entry(this, c), def);
	}
	template<typename R> R				get(const row &r, prop_id id, R def = R())	const	{ return get(r.get_entry(this, id), def); }
	template<typename R, typename T> R	get(pst0<T> *pst, const row &r, prop_id id, R def = R()) const {
		return get(pst, r.get_entry(this, id), def);
	}

	iterator		begin()				const { return iterator(this, 0); }
	iterator		end()				const { return iterator(this, num_rows); }
	row::iterator	begin(const row &r)	const { return row::iterator(this, r, header->columns); }
	row::iterator	end(const row &r)	const { return row::iterator(this, r, header->columns + header->num_columns); }
};

template<> struct table_context::getter<false> {
	template<typename R, typename T> static R	get(const table_context *tc, pst0<T> *pst, entry e) { return tc->row_tree.hp->get_data1(pst, *(hnid*)e); }
};

struct table_context2 : heap, table_context {
	table_context2()	{}
	template<typename T> table_context2(const pst0<T> *pst, const nbt_leaf_entry<T> *n) : heap(pst, n), table_context(pst, this)	{}
	template<typename T> table_context2(const pst0<T> *pst, const sub_leaf_entry<T> *n) : heap(pst, n), table_context(pst, this)	{}
	table_context2(const table_context2 &b) : heap(b), table_context(b)	{ row_tree.hp = this; }
};

//-----------------------------------------------------------------------------
//	folder
//-----------------------------------------------------------------------------

template<typename T> struct folder {
	property_context2	pc;
	table_context2		hierarchy, contents, assoc;

	folder(const pst0<T> *pst,
		const nbt_leaf_entry<T> *np,
		const nbt_leaf_entry<T> *nh,
		const nbt_leaf_entry<T> *nc,
		const nbt_leaf_entry<T> *na
	)	: pc(		pst, np)
		, hierarchy(pst, nh)
		, contents(	pst, nc)
		, assoc(	pst, na)
	{}

	wstring		name(pst0<T> *pst)		const { return pc.get_value(pst, pc.find(pid_DisplayName)); }
	bool		has_children()			const { return pc.get(pid_Subfolders, false); }
	uint32		num_unread()			const { return pc.get(pid_ContentUnreadCount, 0u);	}
	uint32		num_entries()			const { return contents.count();	}
	uint32		num_children()			const { return hierarchy.count();	}

	table_context::row	entry(uint32 i)	const { return contents.get_row(i); }
	table_context::row	child(uint32 i)	const { return hierarchy.get_row(i); }
};

//-----------------------------------------------------------------------------
//	name_ids
//-----------------------------------------------------------------------------

template<typename T> struct name_ids {
	memory_ref	entries;
	memory_ref	guids;
	memory_ref	strings;

	enum {
		pid_NameidBucketCount	= 0x0001,	//prop_type_int32
		pid_NameidStreamGuid	= 0x0002,	//prop_type_binary
		pid_NameidStreamEntry	= 0x0003,	//prop_type_binary
		pid_NameidStreamString	= 0x0004,	//prop_type_binary
	};
	name_ids(pst0<T> *p) {
		property_context2	pc(p, p->find_node(nid_name_id_map).ptr());
		guids	= pc.get_value(p, pc.find(pid_NameidStreamGuid));
		entries	= pc.get_value(p, pc.find(pid_NameidStreamEntry));
		strings	= pc.get_value(p, pc.find(pid_NameidStreamString));
	}

	name_entry	*find(prop_id id, GUID *guid = 0) const {
		int	gi = name_entry::guid_none;
		if (guid && *guid != ps_none) {
			if (*guid == ps_mapi)
				gi = name_entry::guid_mapi;
			else  if (*guid == ps_public_strings)
				gi = name_entry::guid_public_strings;
			else for (GUID *i = guids, *e = (GUID*)guids.end(); i < e; i++) {
				if (*i == *guid) {
					gi = i - guids + 3;
					break;
				}
			}
		}
		id &= 0x7fff;
		for (name_entry *i = entries, *e = (name_entry*)entries.end(); i < e; i++) {
			if ((!guid || i->guid == gi) && i->prop == id)
				return i;
		}
		return 0;
	}

	char16	*get_name(name_entry *e) const {
		return e && e->N ? (char16*)((uint8*)strings + e->offset + 4) : 0;
	}
};

//-----------------------------------------------------------------------------
//	pst
//-----------------------------------------------------------------------------

template<typename T> struct pst : pst0<T> {
	name_ids<T>	ids;

	pst(istream_ref file) : pst0<T>(file, file.get()), ids(this) {
	}

	folder<T>	get_folder(int ix) const {
		return folder<T>(this,
			find_node(make_nid(nid_type_folder,						ix)),
			find_node(make_nid(nid_type_hierarchy_table,			ix)),
			find_node(make_nid(nid_type_contents_table,				ix)),
			find_node(make_nid(nid_type_associated_contents_table,	ix))
		);
	}
};

template<typename T> struct pst_rc : refs<pst_rc<T> >, pst<T> {
	pst_rc(istream_ref file) : pst<T>(file) {}
	~pst_rc()	{ delete &this->file; }
};

DateTime	apptime_offset(1899, 12, 30);
DateTime	systime_offset = DateTime(1601, 1, 1) - DateTime::TimeZone();

void print(string_accum &a, prop_type type, const memory_ref &v) {
	if (type & prop_type_mv) {
		type	= prop_type(type & (prop_type_mv - 1));
		size_t	size	= get_prop_size(type);
		uint32	count	= *(uint32*)v;
		a << '{';
		for (uint32 i = 0; i < count; i++) {
			a << "\n\t";
			if (size == 0) {
				uint32	start	= ((uint32*)v)[i + 1];
				uint32	end		= i == count - 1 ? v.size32() : ((uint32*)v)[i + 2];
				print(a, type, v.slice(start, end - start));
			} else {
				print(a, type, v.slice(4 + size * i, size));
			}
		}
		a << "\n}";

	} else switch (type) {
		case prop_type_int16:		a << *(int16*)v; break;
		case prop_type_int32:		a << *(int32*)v; break;
		case prop_type_float:		a << *(float*)v; break;
		case prop_type_double:		a << *(double*)v; break;
		case prop_type_currency:	a << *(int64*)v; break;
		case prop_type_apptime:		a << apptime_offset + Duration::Days(*(double*)v); break;
		case prop_type_error:		a << "error(" << *(int32*)v << ")"; break;
		case prop_type_bool:		a << *(bool*)v; break;
		case prop_type_object:		a << "object[" << v.length() << "]"; break;
		case prop_type_int64:		a << *(int64*)v; break;
		case prop_type_string:		a << string(v); break;
		case prop_type_wstring:		a << wstring(v); break;
		case prop_type_systime:		a << systime_offset + Duration(*(int64*)v / 10); break;
		case prop_type_guid:		a << *(GUID*)v; break;
		case prop_type_binary:		a << "binary[" << v.length() << "]"; break;
	}
}

} //namespace pst

//-----------------------------------------------------------------------------
//	ISO
//-----------------------------------------------------------------------------

struct prop_name {
	pst::prop_id id;
	const char *name;
	operator int() const	{ return id; }
} prop_names[] = {
#undef PID
#undef LID
#define PID(name, val)	{pst::pid_##name, #name},
#define LID(name, val)	{pst::lid_##name, #name},
#include "pid.h"
};
struct sorted_array {
	template<typename T, int N> sorted_array(T (&t)[N]) {
		sort(t, t + N);
	}
};

const char *get_name(pst::prop_id id) {
	static sorted_array s(prop_names);
	const prop_name	*name = lower_bound(prop_names, prop_names + num_elements(prop_names), id);
	return name ? name->name : 0;
}
template<typename T> const char *get_name(pst::pst<T> *p, pst::prop_id id) {
	static fixed_string<64>	s;

	if (id < 0x8000) {
		if (const char *c = get_name(id))
			return c;
		s.clear();
		s.format("id_0x%04x", id);
		return s;
	}

	if (pst::name_entry	*e = p->ids.find(id)) {
		if (char16 *c = p->ids.get_name(e)) {
			s = fixed_string<64>(c);
		} else if (const char *c = get_name(e->id)) {
			return c;
		} else {
			s.clear();
			s.format("id_0x%08x", e->id);
		}
		return s;
	}

	return 0;
}

struct date_string : string { date_string(DateTime t) : string(to_string(t)) {} };
ISO_DEFUSERX(date_string, string, "date");

ISO::Browser2 pst_value(memory_block v, pst::prop_type type, pst::prop_id id) {
	if (type & pst::prop_type_mv) {
		return ISO_NULL;
		type			= pst::prop_type(type & (pst::prop_type_mv - 1));
		size_t	size	= pst::get_prop_size(type);
		uint32	count	= *(uint32*)v;
		if (size == 0) {
		} else {
		}
	} else switch (type) {
		case pst::prop_type_int16:		return ISO::MakeBrowser(*(int16*)v);
		case pst::prop_type_int32:		return ISO::MakeBrowser(*(int32*)v);
		case pst::prop_type_float:		return ISO::MakeBrowser(*(float*)v);
		case pst::prop_type_double:		return ISO::MakeBrowser(*(double*)v);
		case pst::prop_type_currency:	return ISO::MakeBrowser(*(int64*)v);
		case pst::prop_type_apptime:	return ISO_ptr<date_string>(0, pst::apptime_offset + Duration::Days(*(double*)v));
		case pst::prop_type_error:		return ISO::MakeBrowser(*(int32*)v);
		case pst::prop_type_bool:		return ISO::MakeBrowser(*(bool8*)v);
		case pst::prop_type_object:		return ISO_ptr<string>(0, "object");
		case pst::prop_type_int64:		return ISO::MakeBrowser(*(int64*)v);
		case pst::prop_type_string:		return ISO_ptr<string>(0, pst::string(v));
		case pst::prop_type_wstring: {
			pst::wstring	s = pst::memory_ref(v);
			if (id == pst::pid_Subject && s.begin() && s.begin()[0] == pst::message_subject_prefix_lead_byte)
				s = s.sub_str(2);
			return ISO_ptr<string16>(0, s);
		}
		case pst::prop_type_systime:	return ISO_ptr<date_string>(0, pst::systime_offset + Duration(*(int64*)v / 10));
		case pst::prop_type_guid:		return ISO::MakeBrowser(*(GUID*)v);
		case pst::prop_type_binary:	{
			ISO_ptr<ISO_openarray<uint8> > t(0);
			memcpy(t->Create(v.size32(), false), v, v.length());
			return t;
		}
		default:
			return ISO_NULL;
	}
}

struct iso_pst_pc : public pst::property_context2, public ISO::VirtualDefaults {
	ref_ptr<pst::pst_rc<uint64> >	p;
	int								count;

	struct construct {
		pst::pst_rc<uint64>				*p;
		const pst::property_context2	&pc;
		construct(pst::pst_rc<uint64> *_p, const pst::property_context2 &_pc) : p(_p), pc(_pc) {}
	};
	iso_pst_pc(const construct &c) : pst::property_context2(c.pc), p(c.p) {
		count = 0;
		for (iterator i = begin(), e = end(); i != e; ++i)
			count++;
	}

	int				Count() const {
		return count;
	}
	tag				GetName(int i) const {
		iterator j = begin();
		while (i--)
			++j;
		return get_name(p.get(), j.key());
	}

	ISO::Browser2	Index(int i) const {
		iterator j = begin();
		while (i--)
			++j;
		entry	e = j;
		return pst_value(get_value(p.get(), e), pst::prop_type(e->type), j.key());
	}
};
template<> struct ISO::def<iso_pst_pc> : public ISO::VirtualT<iso_pst_pc> {};

struct iso_pst_row : public pst::table_context::row, public ISO::VirtualDefaults {
	ref_ptr<pst::pst_rc<uint64> >	p;
	pst::table_context2				tc;

	struct construct {
		pst::pst_rc<uint64>				*p;
		const pst::table_context2		&tc;
		const pst::table_context::row	&r;
		construct(pst::pst_rc<uint64> *_p, const pst::table_context2 &_tc, const pst::table_context::row &_r) : p(_p), tc(_tc), r(_r) {}
	};
	iso_pst_row(const construct &c) : pst::table_context::row(c.r), p(c.p), tc(c.tc) {}

	int				Count()			const { return num_exist(&tc);}
	tag				GetName(int i)	const { return get_name(p.get(), existing_col(&tc, i)->id);	}

	ISO::Browser2	Index(int i)	const {
		const pst::table_context::col*	c = existing_col(&tc, i);
		const pst::table_context::entry	e = get_entry(&tc, c);
		return pst_value(tc.get_value(p.get(), e), e.type, c->id);
	}
};
template<> struct ISO::def<iso_pst_row> : public ISO::VirtualT<iso_pst_row> {};

struct iso_pst_tc : public pst::table_context2, public ISO::VirtualDefaults {
	ref_ptr<pst::pst_rc<uint64> >	p;

	struct construct {
		pst::pst_rc<uint64>			*p;
		const pst::table_context2	&tc;
		construct(pst::pst_rc<uint64> *_p, const pst::table_context2 &_tc) : p(_p), tc(_tc) {}
	};
	iso_pst_tc(const construct &c) : pst::table_context2(c.tc), p(c.p) {}
	int				Count()			const { return count(); }
	ISO_ptr<void>	Index(int i)	const { return ISO_ptr<iso_pst_row>(0, iso_pst_row::construct(p, *this, get_row(i))); }
};
template<> struct ISO::def<iso_pst_tc> : public ISO::VirtualT<iso_pst_tc> {};

#if 0
struct pst_message : ISO_combine<iso_pst_row, iso_pst_pc> {
	pst_message(const iso_pst_row::construct &c) : ISO_combine<iso_pst_row, iso_pst_pc>(c,
			iso_pst_pc::construct(c.p, pst::property_context2(c.p, c.p->find_node(c.r.get_rowid()).ptr()))
		) {
//		pst::node(c.p, c.p->find_node(c.r.get_rowid()).ptr()).find_subnode(pst::nid_attachment_table)
		if (pst::hold_ref<pst::sub_leaf_entry<uint64> > x = b.hp->n.find_subnode(c.p, pst::nid_attachment_table))
			pst::table_context2	tc(c.p, x.ptr());
	}
};
template<> struct ISO_def<pst_message>	: public ISO::TypeUserSave	{
	ISO::VirtualT<pst_message>	v;
	ISO_def() : ISO::TypeUserSave("email message", &v) {}
};
#else
struct pst_message : anything {
	pst_message(const iso_pst_row::construct &c) {
		pst::pst_rc<uint64>		*p	= c.p;
		pst::property_context2	pc(p, p->find_node(c.r.get_rowid()).ptr());
		Append(ISO_ptr<iso_pst_row>("row", c));
		Append(ISO_ptr<iso_pst_pc>("props", iso_pst_pc::construct(p, pc)));

		if (pst::hold_ref<pst::sub_leaf_entry<uint64> > a = pc.find_subnode(p, pst::nid_attachment_table)) {
			pst::table_context2	tc(p, a.ptr());
			ISO_ptr<anything>	at("attachments");
			Append(at);
			for (pst::table_context::iterator i = tc.begin(), ie = tc.end(); i != ie; ++i) {
				pst::property_context2	pc2(p, pc.find_subnode(c.p, i->get_rowid()).ptr());
				at->Append(ISO_ptr<iso_pst_row>("row", iso_pst_row::construct(p, tc, *i)));
				at->Append(ISO_ptr<iso_pst_pc>("props", iso_pst_pc::construct(p, pc2)));
			}
		}
	}
};

ISO_DEFUSER(pst_message, anything);
#endif

struct pst_messages : public iso_pst_tc {
	pst_messages(const construct &c) : iso_pst_tc(c) {}
	ISO_ptr<void>	Index(int i)	const {
		return ISO_ptr<pst_message>(0, iso_pst_row::construct(p, *this, get_row(i)));
	}
};
template<> struct ISO::def<pst_messages> : public ISO::VirtualT<pst_messages> {};

template<typename T> ISO_ptr<void> ReadVirtPST(pst::pst_rc<T> *p, const pst::folder<T> *f) {
	pst::wstring		name = f->name(p);
	ISO_ptr<anything>	res(str(name.begin(), name.length()));

	if (f->has_children()) {
		for (int i = 0, n = f->num_children(); i < n; i++) {
			pst::table_context::row		row		= f->child(i);
			int							ix		= pst::get_nid_index(row.get_rowid());
			if (p->find_node(pst::make_nid(pst::nid_type_folder, ix)))
				res->Append(ReadVirtPST(p, new pst::folder<T>(p->get_folder(pst::get_nid_index(row.get_rowid())))));
		}
	}
#if 0
	for (pst::table_context::iterator i = f->contents.begin(), ie = f->contents.end(); i != ie; ++i) {
		ISO_ptr<anything>	m(0);
		res->Append(m);
		pst::property_context2	pc(p, p->find_node(i->get_rowid()).ptr());
		m->Append(ISO_ptr<iso_pst_row>("row", iso_pst_row::construct(p, f->contents, *i)));
		m->Append(ISO_ptr<iso_pst_pc>("props", iso_pst_pc::construct(p, pc)));

		if (pst::hold_ref<pst::sub_leaf_entry<uint64> > a = pc.find_subnode(p, pst::nid_attachment_table)) {
			pst::table_context2	tc(p, a.ptr());
			ISO_ptr<anything>	at("attachments");
			m->Append(at);
			for (pst::table_context::iterator i = tc.begin(), ie = tc.end(); i != ie; ++i) {
				pst::property_context2	pc2(p, pc.find_subnode(p, i->get_rowid()).ptr());
				at->Append(ISO_ptr<iso_pst_row>("row", iso_pst_row::construct(p, tc, *i)));
				at->Append(ISO_ptr<iso_pst_pc>("props", iso_pst_pc::construct(p, pc2)));
			}
//			m->Append(ISO_ptr<iso_pst_tc>("attach", iso_pst_tc::construct(p, tc)));
		}
	}
#elif 0
	if (const pst::table_context::col *subj_col = f->contents.get_col(pst::pid_Subject)) {
		for (pst::table_context::iterator i = f->contents.begin(), ie = f->contents.end(); i != ie; ++i)
			res->Append(ISO_ptr<pst_message>(0, iso_pst_row::construct(p, f->contents, *i)));
	}
#elif 0
	if (f->contents.count())
		res->Append(ISO_ptr<iso_pst_tc>("contents", iso_pst_tc::construct(p, f->contents)));
#else
	if (f->contents.count()) {
		if (!res->Count())
			return ISO_ptr<pst_messages>(res.ID(), pst_messages::construct(p, f->contents));

		res->Append(ISO_ptr<pst_messages>("contents", pst_messages::construct(p, f->contents)));
	}
#endif
	return res;
}

template<typename T> ISO_ptr<void> ReadPST(pst::pst<T> *p, const pst::folder<T> &f) {
	pst::wstring		name = f.name(p);
	ISO_ptr<anything>	res(str(name.begin(), name.length()));

	if (f.has_children()) {
		for (int i = 0, n = f.num_children(); i < n; i++) {
			pst::table_context::row		row		= f.child(i);
			pst::table_context::entry	entry	= row.get_entry(&f.hierarchy, pst::pid_DisplayName);
			res->Append(ReadPST(p, p->get_folder(pst::get_nid_index(row.get_rowid()))));
		}
	}
	if (const pst::table_context::col *subj_col = f.contents.get_col(pst::pid_Subject)) {
		for (pst::table_context::iterator i = f.contents.begin(), ie = f.contents.end(); i != ie; ++i) {
			pst::table_context::row		row		= *i;
			pst::wstring				subj	= f.contents.template get<pst::wstring>(p, row, subj_col);
			if (subj.begin() && subj.begin()[0] == pst::message_subject_prefix_lead_byte)
				subj = subj.sub_str(2);
			res->Append(ISO_ptr<string>(0, subj));
		}
	}
	return res;
}

class PSTFileHandler : public FileHandler {
	const char*		GetExt() override { return "pst"; }
	const char*		GetDescription() override { return "Outlook Personal Folder"; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		pst::header0	h = file.get();
		if (h.magic != pst::header0::magic_head || h.magic_client != pst::header0::magic_pst || h.ver_client != pst::header0::type_pst)
			return ISO_NULL;

		file.seek(0);
		ISO_ptr<void>	r;
		if (h.ver >= pst::header0::fmt_unicode_min) {
			pst::pst<uint64>	p(file);
			r = ReadPST(&p, p.get_folder(pst::nix_root));
		} else {
			pst::pst<uint32>	p(file);
			r = ReadPST(&p, p.get_folder(pst::nix_root));
		}
		r.SetID(id);
		return r;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		WinFileInput	*file	= new WinFileInput(
			CreateFileA(fn, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0)
		);
		if (!file->exists()) {
			delete file;
			return ISO_NULL;
		}
		pst::pst_rc<uint64>	*p	= new pst::pst_rc<uint64>(*file);
		ISO_ptr<void>		r	= ReadVirtPST<uint64>(p, new pst::folder<uint64>(p->get_folder(pst::nix_root)));
		r.SetID(id);
		return r;
	}

} pstfh;

