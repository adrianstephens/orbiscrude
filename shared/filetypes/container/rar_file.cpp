#include "iso/iso_files.h"
#include "archive_help.h"
#include "hashes/sha.h"
#include "comms/leb128.h"
#include "codec/rar.h"

using namespace iso;


#define	BLAKE2_DIGEST_SIZE	32
#define SHA256_DIGEST_SIZE	32

#define SIZE_SALT50			16
#define SIZE_SALT30			8
#define SIZE_INITV			16


#define NM  2048

// Internal implementation, depends on archive format version
enum HOST_SYSTEM {
	// RAR 5.0 host OS
	HOST5_WINDOWS	= 0,
	HOST5_UNIX      = 1,

	// RAR 3.0 host OS.
	HOST_MSDOS		= 0,
	HOST_OS2		= 1,
	HOST_WIN32		= 2,
	HOST_UNIX		= 3,
	HOST_MACOS		= 4,
	HOST_BEOS		= 5,
	HOST_MAX
};

enum FSREDIR {
	FSREDIR_NONE,
	FSREDIR_UNIXSYMLINK,
	FSREDIR_WINSYMLINK,
	FSREDIR_JUNCTION,
	FSREDIR_HARDLINK,
	FSREDIR_FILECOPY
};

#define SUBHEAD_TYPE_CMT      L"CMT"
#define SUBHEAD_TYPE_QOPEN    L"QO"
#define SUBHEAD_TYPE_ACL      L"ACL"
#define SUBHEAD_TYPE_STREAM   L"STM"
#define SUBHEAD_TYPE_UOWNER   L"UOW"
#define SUBHEAD_TYPE_AV       L"AV"
#define SUBHEAD_TYPE_RR       L"RR"
#define SUBHEAD_TYPE_OS2EA    L"EA2"

#define SUBHEAD_FLAGS_INHERITED    0x80000000
#define SUBHEAD_FLAGS_CMT_UNICODE  0x00000001

enum RARFORMAT {
	RARFMT_NONE,
	RARFMT14,
	RARFMT15,
	RARFMT50,
	RARFMT_FUTURE
};

//-----------------------------------------------------------------------------
//	RAR 1.4
//-----------------------------------------------------------------------------

struct Header14 {
	enum { MARK = 0x5e7e4552, MARK15 = 0x21726152 };
	packed<uint32>	mark;
	packed<uint16>	size;
	uint8			flags;
	RARFORMAT		test() const {
		return mark == MARK ? RARFMT14
			: mark == MARK15 && size == 0x071a && flags < 5
			? (flags == 0 ? RARFMT15 : flags == 1 ? RARFMT50 : RARFMT_FUTURE)
			: RARFMT_NONE;
	}
};

struct FileHeader14 {
	enum {
		PASSWORD		= 4,
	};

	uint32	data_size;
	uint32	unp_size;
	uint16	crc32;
	uint16	head_size;
	uint32	time;
	uint8	attr;
	uint8	flags;
	uint8	unp_ver;
	uint8	name_size;
	uint8	method;
};

//-----------------------------------------------------------------------------
//	RAR 1.5
//-----------------------------------------------------------------------------

struct BlockHeader15 {
	enum TYPE {
		MARK		= 0x72,
		MAIN		= 0x73,
		FILE		= 0x74,
		CMT			= 0x75,
		AV			= 0x76,
		OLDSERVICE	= 0x77,
		PROTECT		= 0x78,
		SIGN		= 0x79,
		SERVICE		= 0x7a,
		ENDARC		= 0x7b
	};
	enum {
		SKIP_IF_UNKNOWN	= 0x4000,
		LONG_BLOCK		= 0x8000,
	};
	uint16	crc;
	uint8	type;
	uint16	flags;
	uint16	header_size;
	uint64	data_size;
	bool read(istream_ref file) {
		data_size = 0;
		return file.read(crc, type, flags, header_size) && header_size >= 7;
	}
	malloc_block	header_data(istream_ref file) const {
		return malloc_block(file, header_size);
	}
};

struct MainHeader15 : BlockHeader15 {
	enum {
		VOLUME			= 0x0001,
		COMMENT			= 0x0002,	// Old style main archive comment embed into main archive header. Must not be used in new archives anymore
		LOCK			= 0x0004,
		SOLID			= 0x0008,
		PACK_COMMENT	= 0x0010,
		NEWNUMBERING	= 0x0010,
		AV				= 0x0020,
		PROTECT			= 0x0040,
		PASSWORD		= 0x0080,
		FIRSTVOLUME		= 0x0100,
	};
	uint16	av_pos_hi;
	uint32	av_pos;

	MainHeader15(const BlockHeader15& h, istream_ref file) : BlockHeader15(h) {
		file.read(av_pos_hi);
		file.read(av_pos);
	}
};

struct FileHeader15 : BlockHeader15 {
	enum {
		SPLIT_BEFORE	= 0x0001,
		SPLIT_AFTER		= 0x0002,
		PASSWORD		= 0x0004,
		COMMENT			= 0x0008,	// Old style file comment embed into file header. Must not be used in new archives anymore
		SOLID			= 0x0010,	// For non-file subheaders it denotes 'subblock having a parent file' flag
		WINDOWMASK		= 0x00e0,
		WINDOW64		= 0x0000,
		WINDOW128		= 0x0020,
		WINDOW256		= 0x0040,
		WINDOW512		= 0x0060,
		WINDOW1024		= 0x0080,
		WINDOW2048		= 0x00a0,
		WINDOW4096		= 0x00c0,
		DIRECTORY		= 0x00e0,
		LARGE			= 0x0100,
		UNICODE			= 0x0200,
		SALT			= 0x0400,
		VERSION			= 0x0800,
		EXTTIME			= 0x1000,

		SIZEOF_FILEHEAD3	= 32,

	};
	uint64	unp_size;
	uint8	host_os;
	uint32	crc32;
	uint32	mtime;
	uint8	unp_ver;
	uint8	method;
	uint16	name_size;
	uint32	file_attr;

	bool	dir() const {
		return unp_ver < 20 ? !!(file_attr & 0x10) : (flags & WINDOWMASK) == DIRECTORY;
	}
	uint64	window_size() const {
		return dir() ? 0 : 0x10000 << ((flags & WINDOWMASK) >> 5);
	}
	int		extra_data() const {
		return header_size - name_size - SIZEOF_FILEHEAD3 - (flags & SALT ? SIZE_SALT30 : 0) - (flags & EXTTIME ? 2 : 0);
	}
	FileHeader15(const BlockHeader15& h, istream_ref file) : BlockHeader15(h) {
		data_size		= file.get<uint32>();
		unp_size		= file.get<uint32>();
		host_os			= file.getc();

		file.read(crc32, mtime, unp_ver, method, name_size, file_attr);

		if (flags & LARGE) {
			data_size	+= uint64(file.get<uint32>()) << 32;
			unp_size	+= uint64(file.get<uint32>()) << 32;
		} else {
			unp_size	= int32(unp_size);
		}
	}
};

struct EndArcHeader15 : BlockHeader15 {
	enum {
		NEXT_VOLUME		= 0x0001, // Not last volume
		DATACRC			= 0x0002, // Store CRC32 of RAR archive (now is used only in volumes)
		REVSPACE		= 0x0004, // Reserve space for end of REV file 7 uint8 record
		VOLNUMBER		= 0x0008, // Store a number of current volume
	};
	uint32	crc32;  
	uint32	volume; // Optional number of current volume.

	EndArcHeader15(const BlockHeader15& h, istream_ref file) : BlockHeader15(h) {
		if (flags & DATACRC)
			file.read(crc32);
		if (flags & VOLNUMBER)
			file.read(volume);
	}
};

struct CommentHeader15 : BlockHeader15 {
	CommentHeader15(const BlockHeader15& h, istream_ref file) : BlockHeader15(h) {
		data_size = file.get<uint16>();
	}
};
struct ProtectHeader15 : BlockHeader15 {
	uint8	ver;
	uint16	rec_sectors;
	uint32	total_blocks;
	uint8	mark[8];

	ProtectHeader15(const BlockHeader15& h, istream_ref file) : BlockHeader15(h) {
		data_size = file.get<uint32>();
		file.read(ver, rec_sectors, total_blocks, mark);
	}
};
// RAR 2.9 and earlier
struct OldServiceHeader15 : BlockHeader15 {
	enum {
		EA_HEAD			= 0x100,
		UO_HEAD			= 0x101,
		MAC_HEAD		= 0x102,
		BEEA_HEAD		= 0x103,
		NTACL_HEAD		= 0x104,
		STREAM_HEAD		= 0x105
	};
	uint16	type;
	uint8	level;

	OldServiceHeader15(const BlockHeader15& h, istream_ref file) : BlockHeader15(h) {
		data_size = file.get<uint32>();
		file.read(type);
		file.read(level);
	}
};

//-----------------------------------------------------------------------------
//	RAR 5.0
//-----------------------------------------------------------------------------

uint64 GetV(istream_ref file) {
	return get_leb128<uint64>(file);
}

struct BlockHeader50 {
	enum {
		EXTRA			= 0x0001,	// Additional extra area is present in the end of block header
		DATA			= 0x0002,	// Additional data area is present in the end of block header
		SKIPIFUNKNOWN	= 0x0004,	// Unknown blocks with this flag must be skipped when updating an archive
		SPLITBEFORE		= 0x0008,	// Data area of this block is continuing from previous volume
		SPLITAFTER		= 0x0010,	// Data area of this block is continuing in next volume
		CHILD			= 0x0020,	// Block depends on preceding file block
		INHERITED		= 0x0040,	// Preserve a child block if host is modified
	};
	enum TYPE {
		MARK			= 0x00,
		MAIN			= 0x01,
		FILE			= 0x02,
		SERVICE			= 0x03,
		CRYPT			= 0x04,
		ENDARC			= 0x05,
		UNKNOWN			= 0xff,
	};

	uint32				crc;
	leb128<uint64>		header_size;
	leb128<TYPE>		type;
	leb128<uint64>		header_flags;
	leb128<uint64>		extra_size	= 0;
	leb128<uint64>		data_size	= 0;

	bool read(istream_ref file) {
		if (!file.read(crc, header_size, type, header_flags))
			return false;

//		uint32 HeaderCRC = Raw.GetCRC50();
//		if (ShortBlock.HeadCRC != HeaderCRC)
//			return false;

		return (!(header_flags & EXTRA)	|| file.read(extra_size))
			&& (!(header_flags & DATA)	|| file.read(data_size));
	}
};

struct MainHeader50 : BlockHeader50 {
	enum {
		VOLUME		= 0x0001, // Volume
		VOLNUMBER	= 0x0002, // Volume number field is present. True for all volumes except first
		SOLID		= 0x0004, // Solid archive
		PROTECT		= 0x0008, // Recovery record is present
		LOCK		= 0x0010, // Locked archive
	};
	enum {//extra field values.
		LOCATOR		= 0x01 // Position of quick list and other blocks
	};
	struct Locator {
		enum {
			QLIST	= 0x01, // Quick open offset is present
			RR		= 0x02, // Recovery record offset is present
		};
	};
	leb128<uint32>	flags;
	leb128<uint64>	volume	= 0;

	MainHeader50(const BlockHeader50 &h, istream_ref file) : BlockHeader50(h) {
		file.read(flags);
		if (flags & VOLUME)
			file.read(volume);
	}
};

struct EndArcHeader50 : BlockHeader50 {
	enum {
		NEXTVOLUME		= 0x0001,// Not last volume.
	};
	leb128<uint32>	flags;
	EndArcHeader50(const BlockHeader50 &h, istream_ref file) : BlockHeader50(h) {
		file.read(flags);
	}
};

struct CryptHeader50 : BlockHeader50 {
	enum {
		PSWCHECK	= 0x0001, // Password check data is present
	};
	leb128<uint32>	ver;
	leb128<uint32>	flags;
	uint8			lg2_count;
	uint8			salt[SIZE_SALT50];

	CryptHeader50(const BlockHeader50 &h, istream_ref file) : BlockHeader50(h) {
		file.read(ver, flags, lg2_count, salt);
	}
};

struct FileHeader50 : BlockHeader50 {
	enum {
		DIRECTORY		= 0x0001, // Directory
		UTIME			= 0x0002, // Time field in Unix format is present
		CRC32			= 0x0004, // CRC32 field is present
		UNPUNKNOWN		= 0x0008, // Unknown unpacked size
	};

	enum {// extra fields
		CRYPT		= 0x01, // Encryption parameters
		HASH		= 0x02, // File hash
		HTIME		= 0x03, // High precision file time
		VERSION		= 0x04, // File version information
		REDIR		= 0x05, // File system redirection (links, etc.)
		UOWNER		= 0x06, // Unix owner and group information
		SUBDATA		= 0x07, // Service header subdata array
	};
	struct Crypt		{
		enum {
		PSWCHECK	= 0x01, // Store password check data
		HASHMAC		= 0x02, // Use MAC for unpacked data checksums
		};

	};
	struct Hash		{
		enum {
			BLAKE2		= 0x00,
		};
	};
	struct HTime		{
		enum {
			UNIXTIME	= 0x01, // Use Unix time_t format
			MTIME		= 0x02, // mtime is present
			CTIME		= 0x04, // ctime is present
			ATIME		= 0x08, // atime is present
			UNIX_NS		= 0x10, // Unix format with nanosecond precision
		};
	};
	struct Version	{};
	struct Redir		{
		enum {
			DIR			= 0x01, // Link target is directory
		};
	};
	struct UOwner	{
		enum {
			UNAME	= 0x01, // User name string is present
			GNAME	= 0x02, // Group name string is present
			NUMUID	= 0x04, // Numeric user ID is present
			NUMGID	= 0x08, // Numeric group ID is present
		};
	};
	struct Subdata	{};

	struct CompInfo {
		uint16	ver:6, solid:1, method:3, win_size:4;
	};
	leb128<uint32>	flags;
	leb128<uint64>	unp_size;
	leb128<uint32>	file_attr;
	uint32			mtime;
	uint32			crc32;
	CompInfo		comp_info;
	uint8			host_os;
	leb128<size_t>	name_size;

	uint64	window_size() const {
		return flags & DIRECTORY ? 0 : size_t(0x20000) << comp_info.win_size;
	}

	FileHeader50(const BlockHeader50 &h, istream_ref file) : BlockHeader50(h) {
		file.read(flags);
		file.read(unp_size);
		file.read(file_attr);
		if (flags & UTIME)
			file.read(mtime);

		if (flags & CRC32)
			file.read(crc32);

		file.read(comp_info);
		file.read(host_os);
		file.read(name_size);
	}
};

//-----------------------------------------------------------------------------
//	Encryption
//-----------------------------------------------------------------------------

enum CRYPT_METHOD {
	CRYPT_NONE,
	CRYPT_RAR13,
	CRYPT_RAR15,
	CRYPT_RAR20,
	CRYPT_RAR30,
	CRYPT_RAR50
};

#define CRYPT_BLOCK_SIZE         16
#define CRYPT5_KDF_LG2_COUNT     15 // LOG2 of PDKDF2 iteration count.
#define CRYPT5_KDF_LG2_COUNT_MAX 24 // LOG2 of maximum accepted iteration count.
#define CRYPT_VERSION             0 // Supported encryption version.

#define _MAX_KEY_COLUMNS (256/32)

class CryptData {
	class SecPassword {
		void Process(const char16 *Src,size_t SrcSize,char16 *Dst,size_t DstSize,bool Encode);
		char16 Password[128];
		bool PasswordSet;
	public:
		SecPassword();
		~SecPassword();
		void	Clean();
		void	Get(char16 *Psw,size_t MaxSize);
		void	Set(const char16 *Psw);
		bool	IsSet() {return PasswordSet;}
		size_t	Length();
		bool	operator==(SecPassword &psw);
	};

	struct KDF5CacheItem {
		SecPassword Pwd;
		uint8		Salt[SIZE_SALT50];
		uint8		Key[32];
		uint8		Lg2Count; // Log2 of PBKDF2 repetition count.
		SHA256::CODE PswCheckValue;
		SHA256::CODE HashKeyValue;
	};

	struct KDF3CacheItem {
		SecPassword Pwd;
		uint8		Salt[SIZE_SALT30];
		uint8		Key[16];
		uint8		Init[16];
		bool		SaltPresent;
	};

	struct Rijndael { //aka AES
		enum {
			MAX_ROUNDS	= 14,
			MAX_IV_SIZE	= 16,
		};
		bool	CBCMode;
		int		Rounds;
		uint8	initVector[MAX_IV_SIZE];
		uint8	expandedKey[MAX_ROUNDS+1][4][4];
	};

	CRYPT_METHOD	Method;

	KDF3CacheItem	KDF3Cache[4];
	uint32			KDF3CachePos;

	KDF5CacheItem	KDF5Cache[4];
	uint32			KDF5CachePos;

	Rijndael		rin;

	uint32			CRCTab[256]; // For RAR 1.5 and RAR 2.0 encryption.
	uint8			SubstTable20[256];
	uint32			Key20[4];

	uint8			Key13[3];
	uint16			Key15[4];
};

//-----------------------------------------------------------------------------
//	RARArchive
//-----------------------------------------------------------------------------

struct RARArchive {
	enum {
		MAXSFXSIZE	= 0x200000,
	};
	struct Crypt {
		enum {
			SIZE_PSWCHECK		= 8,
			SIZE_PSWCHECK_CSUM	= 4,
		};
		bool	UsePswCheck;
		bool	SaltSet;
		uint8	Lg2Count; // Log2 of PBKDF2 repetition count.
		uint8	Salt[SIZE_SALT50];
		uint8	PswCheck[SIZE_PSWCHECK];

		bool	read_pw(istream_ref file) {
			uint8	csum[SIZE_PSWCHECK_CSUM];
			file.read(PswCheck);
			file.read(csum);
			return memcmp(csum, SHA256(PswCheck, SIZE_PSWCHECK).digest(), SIZE_PSWCHECK_CSUM) == 0;
		}
	};
	struct Time {
		static const uint32 TICKS_PER_SECOND = 1000000000; // Internal precision.
		uint64 itime;
		void	SetUnix(uint32 u) {
			itime = u * TICKS_PER_SECOND;
		}
		void	SetWin(uint64 u) {
			itime = u;
		}
		void	Adjust(uint32 ns) {
			if ((ns & 0x3fffffff) < TICKS_PER_SECOND)
				itime += ns;
		}
		void	read(istream_ref file, bool unix) {
			if (unix)
				SetUnix(file.get<uint32>());
			else
				SetWin(file.get<uint64>());
		}
	};

	struct HashValue {
		enum HASH_TYPE {NONE, RAR14, CRC32, BLAKE2};
		HASH_TYPE Type;
		union {
			uint32			crc32;
			SHA256::CODE	digest;
		};

		void Init(HASH_TYPE Type);
		bool operator==(const HashValue &cmp);
		bool operator!=(const HashValue &cmp) {return !(*this==cmp);}

		HashValue() : Type(NONE) {}
	};

	struct Entry {
		uint8			HostOS;
		uint8			UnpVer;
		uint8			Method;
		union {
			uint32	FileAttr;
			uint32	SubFlags;
		};
		char			FileName[NM];

		malloc_block	SubData;
		Time			mtime, ctime, atime;
		int64			PackSize, UnpSize;

		HashValue		FileHash;
		CRYPT_METHOD	CryptMethod;
		Crypt			crypt;
		uint8			InitV[SIZE_INITV];
		bool			UseHashKey;						// Use HMAC calculated from HashKey and checksum instead of plain checksum.
		uint8			HashKey[SHA256_DIGEST_SIZE];	// Key to convert checksum to HMAC. Derived from password with PBKDF2 using additional iterations.

		size_t			WinSize;
		bool			Dir;
		bool			Version;
		bool			Solid;
		bool			SubBlock;		// 'true' for HEAD_SERVICE block, which is a child of preceding file block
		FSREDIR			RedirType;
		char16			RedirName[NM];
		bool			DirTarget;

		char			UnixOwnerName[256], UnixGroupName[256];
		uint32			UnixOwnerID;
		uint32			UnixGroupID;
	};


	RARFORMAT	Format;

	//main
	uint16	HighPosAV;
	uint32	PosAV;

	bool	Volume;
	bool	Solid;
	bool	Locked;
	bool	Protected;
	bool	Encrypted;
	bool	Locator;

	uint32	EACRC;
	uint32	StreamCRC;
	char	StreamName[260];

	//endarc
	struct {
		uint32	crc;  
		uint32	vol;
		bool	next_vol;
	} endarc;

	struct {
		uint8	ver;
		uint8	method;
		uint16	crc;
	} comment;

	struct {
		uint8	ver;
		uint16	RecSectors;
		uint32	TotalBlocks;
		uint8	Mark[8];
	} protect;

	Crypt		crypt;

	bool		Signed;
	size_t		SFXSize;
	bool		BrokenHeader;
	uint32		VolNumber;
	malloc_block	comment_data;

	static void read_string(istream_ref file, uint32 n, char *buffer) {
		file.readbuff(buffer, n);
		buffer[n] = 0;
	}
	template<int N> static void read_string2(istream_ref file, uint32 n, char (&buffer)[N]) {
		read_string(file, min(N - 1, n), buffer);
		if (n >= N)
			file.seek_cur(n - N - 1);
	}

	bool open(istream_ref file);
	bool next(Entry &fh, istream_ref file);
	void ProcessExtra(bool service, istream_ref file, Entry *hd);
};

void RARArchive::ProcessExtra(bool service, istream_ref file, Entry *hd) {
	while (!file.eof()) {
		int64	FieldSize = get_leb128<uint64>(file);	// Needs to be signed for check below and can be negative.
		size_t	NextPos	 = file.tell() + FieldSize;

		switch (get_leb128<uint64>(file)) {
			case FileHeader50::CRYPT: {
				uint32	ver = GetV(file);
				if (ver < CRYPT_VERSION) {
					uint32 Flags			= get_leb128(file);//GetV(file);
					hd->crypt.UsePswCheck	= (Flags & FileHeader50::Crypt::PSWCHECK) != 0;
					hd->UseHashKey			= (Flags & FileHeader50::Crypt::HASHMAC) != 0;
					hd->crypt.Lg2Count		= file.getc();
					file.read(hd->crypt.Salt);
					file.read(hd->InitV);
					if (hd->crypt.UsePswCheck) {
						hd->crypt.UsePswCheck = hd->crypt.read_pw(file);
						// RAR 5.21 and earlier set PswCheck field in service records to 0 even if UsePswCheck was present.
						if (service && memcmp(hd->crypt.PswCheck, "\0\0\0\0\0\0\0\0", Crypt::SIZE_PSWCHECK) == 0)
							hd->crypt.UsePswCheck = false;
					}
					hd->crypt.SaltSet		= true;
					hd->CryptMethod = CRYPT_RAR50;
				}
				break;
			}

			case FileHeader50::HASH:
				if (GetV(file) == FileHeader50::Hash::BLAKE2) {
					hd->FileHash.Type = HashValue::BLAKE2;
					file.readbuff(hd->FileHash.digest, BLAKE2_DIGEST_SIZE);
				}
				break;

			case FileHeader50::HTIME:
				if (FieldSize >= 5) {
					auto	flags	 = GetV(file);
					bool	unix	= !!(flags & FileHeader50::HTime::UNIXTIME);
					if (flags & FileHeader50::HTime::MTIME)
						hd->mtime.read(file, unix);
					if (flags & FileHeader50::HTime::CTIME)
						hd->ctime.read(file, unix);
					if (flags & FileHeader50::HTime::ATIME)
						hd->atime.read(file, unix);
					if (unix && (flags & FileHeader50::HTime::UNIX_NS)) {  // Add nanoseconds
						if (flags & FileHeader50::HTime::MTIME)
							hd->mtime.Adjust(file.get<uint32>());
						if (flags & FileHeader50::HTime::CTIME)
							hd->ctime.Adjust(file.get<uint32>());
						if (flags & FileHeader50::HTime::ATIME)
							hd->atime.Adjust(file.get<uint32>());
					}
				}
				break;

			case FileHeader50::VERSION:
				if (FieldSize >= 1) {
					GetV(file);  // Skip flags field.
					if (auto ver = GetV(file)) {
						//hd->Version = true;
						//char16 VerText[20];
						//swprintf(VerText, ASIZE(VerText), L";%u", Version);
						//wcsncatz(hd->FileName, VerText, ASIZE(hd->FileName));
					}
				}
				break;

			case FileHeader50::REDIR: {
				hd->RedirType	= (FSREDIR)GetV(file);
				auto flags		= GetV(file);
				hd->DirTarget	= (flags & FileHeader50::Redir::DIR) != 0;
				char UtfName[NM * 4];
				read_string2(file, GetV(file), UtfName);
				break;
			}

			case FileHeader50::UOWNER: {
				auto flags	= GetV(file);
				if (flags & FileHeader50::UOwner::UNAME)
					read_string2(file, GetV(file), hd->UnixOwnerName);
				if (flags & FileHeader50::UOwner::GNAME)
					read_string2(file, GetV(file), hd->UnixGroupName);
				if (flags & FileHeader50::UOwner::NUMUID)
					hd->UnixOwnerID = GetV(file);
				if (flags & FileHeader50::UOwner::NUMGID)
					hd->UnixGroupID = GetV(file);
				break;
			}
			case FileHeader50::SUBDATA: {
				//if (type == HEAD_SERVICE && Raw->Size() - NextPos == 1)
				//	FieldSize++;

				// We cannot allocate too much memory here, because above
				// we check FieldSize againt Raw size and we control that Raw size
				// is sensible when reading headers.
				hd->SubData.read(file, FieldSize);
				break;
			}
		}

		file.seek(NextPos);
	}
}

bool RARArchive::open(istream_ref file) {
	BrokenHeader	= false;  // Might be left from previous volume.
	SFXSize			= 0;
	
	auto	CurPos	= file.tell();
	auto	h		= file.get<Header14>();
	Format = h.test();

	if (!Format) {
		malloc_block	buffer(file, MAXSFXSIZE);
		for (int i = 0; i < buffer.length() - sizeof(Header14); i++) {
			Header14	*ph = buffer + i;
			if (Format = ph->test()) {
				if (Format == RARFMT14 && i > 0 && CurPos < 28 && buffer.length() > 31) {
					char* D = buffer + (28 - CurPos);
					if (D[0] != 0x52 || D[1] != 0x53 || D[2] != 0x46 || D[3] != 0x58)
						continue;
				}
				SFXSize = CurPos + i;
				h		= *ph;
				break;
			}
		}
		if (SFXSize == 0)
			return false;

		file.seek(SFXSize + sizeof(Header14));
	}

	switch (Format) {
		case RARFMT14:
			return h.size >= 7;

		case RARFMT15: {
			BlockHeader15	h;
			while (h.read(file)) {
				if (h.type == h.MAIN) {
					MainHeader15	mh(h, file);
					HighPosAV	= mh.av_pos_hi;
					PosAV		= mh.av_pos;

					Volume		= !!(mh.flags & mh.VOLUME);
					Solid		= !!(mh.flags & mh.SOLID);
					Locked		= !!(mh.flags & mh.LOCK);
					Protected	= !!(mh.flags & mh.PROTECT);
					Encrypted	= !!(mh.flags & mh.PASSWORD);
					Signed		= mh.av_pos_hi || mh.av_pos;
					return true;
				}
			}
			break;
		}
		case RARFMT50: {
			// RAR 5.0 signature has one more uint8
			if (file.getc() != 0)
				return false;

			BlockHeader50	h;
			while (h.read(file)) {
				if (h.type == h.MAIN) {
					MainHeader50	mh(h, file);
					malloc_block	header_data(h.header_size);
					memory_reader	mem(header_data);
					
					Signed		= false;
					//NewNumbering	= true;

					VolNumber	= mh.volume;
					if (h.extra_size) {
						mem.seek(h.header_size - h.extra_size);
						while (!mem.eof()) {
							int64	FieldSize	= GetV(file);	// Needs to be signed for check below and can be negative.
							size_t	NextPos		= file.tell() + FieldSize;
							uint64	FieldType	= GetV(file);
							if (FieldType == mh.LOCATOR) {
								Locator = true;
								uint32 flags	= GetV(file);
								if (flags & MainHeader50::Locator::QLIST) {
									if (uint64 Offset = GetV(file))
										;//QOpenOffset = Offset + CurBlockPos;
								}
								if (flags &MainHeader50::Locator::RR) {
									if (uint64 Offset = GetV(file))
										;//RROffset = Offset + CurBlockPos;
								}
							}
						}
					}
					return true;
				}
			}
		}
		default:
			break;
	}
	return false;
}

bool RARArchive::next(Entry &fh, istream_ref file) {
	clear(fh);
	switch (Format) {
		case RARFMT14: {
			auto	h = file.get<FileHeader14>();
			if (h.head_size < 21)
				return false;

			fh.UnpSize			= h.unp_size;
			fh.FileHash.Type	= HashValue::RAR14;
			fh.FileHash.crc32	= h.crc32;
			fh.FileAttr			= h.attr;
			fh.UnpVer			= h.unp_ver == 2 ? 13 : 10;
			fh.Method			= h.method;
			fh.CryptMethod		= h.flags & FileHeader14::PASSWORD ? CRYPT_RAR13 : CRYPT_NONE;
			fh.PackSize			= h.data_size;
			fh.WinSize			= 0x10000;
			fh.Dir				= (fh.FileAttr & 0x10) != 0;
			fh.HostOS			= HOST_MSDOS;
			fh.mtime.SetUnix(h.time);

			read_string2(file, h.name_size, fh.FileName);
			return true;
		}
		case RARFMT15: {
			auto	h = file.get<BlockHeader15>();
			switch (h.type) {
				case h.ENDARC: {
					EndArcHeader15	eh(h, file);
					break;
				}

				case h.CMT: {
					CommentHeader15	ch(h, file);
					break;
				}

				case h.PROTECT: {
					ProtectHeader15	ph(h, file);
					break;
				}

				case h.OLDSERVICE: {	// RAR 2.9 and earlier.
					OldServiceHeader15	oh(h, file);
					switch (oh.type) {
						case OldServiceHeader15::UO_HEAD: {
							auto	OwnerNameSize	  = file.get<uint16>();
							auto	GroupNameSize	  = file.get<uint16>();
							read_string2(file, OwnerNameSize, fh.UnixOwnerName);
							read_string2(file, GroupNameSize, fh.UnixGroupName);
							break;
						}
						case OldServiceHeader15::NTACL_HEAD: {
							uint32	unpsize = file.get<uint32>();
							uint8	unp_ver	= file.getc();
							uint8	method	= file.getc();
							file.read(EACRC);
							break;
						}
						case OldServiceHeader15::STREAM_HEAD: {
							uint32	unpsize = file.get<uint32>();
							uint8	unp_ver	= file.getc();
							uint8	method	= file.getc();
							file.read(EACRC);
							read_string2(file, file.get<uint16>(), StreamName);
							break;
						}
					}
					break;
				}

				case h.FILE:
				case h.SERVICE: {
					FileHeader15	fh15(h, file);
					fh.WinSize		= fh15.window_size();
					fh.Dir			= fh15.dir();

					//fh.SplitBefore	= !!(fh15.flags & fh15.SPLIT_BEFORE);
					//fh.SplitAfter		= !!(fh15.flags & fh15.SPLIT_AFTER);
					//fh.SaltSet		= !!(fh15.flags & fh15.SALT);
					fh.Solid			= h.type == h.FILE && !!(fh15.flags & fh15.SOLID);
					fh.SubBlock			= h.type == h.SERVICE && !!(fh15.flags & fh15.SOLID);
					//fh.CommentInHeader	= !!(fh15.flags & fh15.COMMENT);
					fh.Version			= !!(fh15.flags & fh15.VERSION);

					fh.PackSize		= fh15.data_size;
					fh.UnpSize		= fh15.unp_size;
					fh.HostOS		= fh15.host_os;

					fh.FileHash.Type	= HashValue::CRC32;
					fh.FileHash.crc32	= fh15.crc32;

					fh.mtime.SetUnix(fh15.mtime);
					fh.UnpVer		= fh15.unp_ver;

					fh.Method		= fh15.method;
					fh.FileAttr		= fh15.file_attr;

					if (fh15.flags & fh15.PASSWORD) {
						switch (fh.UnpVer) {
							case 13: fh.CryptMethod = CRYPT_RAR13; break;
							case 15: fh.CryptMethod = CRYPT_RAR15; break;
							case 20:
							case 26: fh.CryptMethod = CRYPT_RAR20; break;
							default: fh.CryptMethod = CRYPT_RAR30; break;
						}
					}

					// RAR 4.x Unix symlink
					if (fh.HostOS == HOST_UNIX && (fh.FileAttr & 0xF000) == 0xA000) {
						fh.RedirType  = FSREDIR_UNIXSYMLINK;
						*fh.RedirName = 0;
					}

					read_string2(file, fh15.name_size, fh.FileName);

					//if (h.flags & LHD_UNICODE) {
					//}
					// Calculate the size of optional data.
					if (int extra = fh15.extra_data())
						// Here we read optional additional fields for subheaders. They are stored after the file name and before salt.
						fh.SubData.read(file, extra);

					if (h.flags & FileHeader15::SALT)
						file.readbuff(fh.crypt.Salt, SIZE_SALT30);

					if (h.flags & FileHeader15::EXTTIME) {
						auto	 flags = file.get<uint16>();
					}

					//uint16 HeaderCRC		= Raw.GetCRC15(h.flags & LHD_COMMENT);
					//if (hd->HeadCRC != HeaderCRC)
					//	BrokenHeader = true;
					return true;
				}
				default:
					if (h.flags & h.LONG_BLOCK)
						file.read(h.data_size);
					break;
			}
			return false;
		}
		default:  {
			//FORMAT50+
			auto	h = file.get<BlockHeader50>();
			switch (h.type) {
				case h.CRYPT: {
					CryptHeader50	ch(h, file);
					if (ch.ver > CRYPT_VERSION || ch.lg2_count > CRYPT5_KDF_LG2_COUNT_MAX)
						return false;

					crypt.Lg2Count		= file.getc();
					memcpy(crypt.Salt, ch.salt, sizeof(ch.salt));
					if (ch.flags & ch.PSWCHECK)
						crypt.UsePswCheck = crypt.read_pw(file);

					Encrypted = true;
					break;
				}

				case h.FILE:
				case h.SERVICE: {
					FileHeader50	fh50(h, file);
					fh.PackSize		= fh50.data_size;
					fh.UnpSize		= fh50.unp_size;
					fh.FileAttr		= fh50.file_attr;
					fh.mtime.SetUnix(fh50.mtime);

					if (fh50.flags & fh50.CRC32) {
						fh.FileHash.Type	= HashValue::CRC32;
						fh.FileHash.crc32	= fh50.crc32;
					}

					fh.RedirType	= FSREDIR_NONE;
					fh.Method		= fh50.comp_info.method;
					fh.UnpVer		= fh50.comp_info.ver;
					fh.HostOS		= fh50.host_os;
					fh.SubBlock		= (fh50.header_flags & h.CHILD) != 0;
					fh.Dir			= (fh50.flags & fh50.DIRECTORY) != 0;
					fh.WinSize		= fh50.window_size();
					fh.CryptMethod	= Encrypted ? CRYPT_RAR50 : CRYPT_NONE;

					read_string2(file, fh50.name_size, fh.FileName);
					
					if (h.extra_size)
						ProcessExtra(h.type == h.SERVICE, file, &fh);

					//if (h.type == HEAD_SERVICE && fh.FileName == SUBHEAD_TYPE_CMT)
					//	MainHeader.SetComment();//MainComment = true;
					break;
				}
				case h.ENDARC: {
					EndArcHeader50	eh(h, file);
					endarc.next_vol	= (eh.flags & EndArcHeader50::NEXTVOLUME) != 0;
					break;
				}
			}
			return false;
		}
	}
}

//-----------------------------------------------------------------------------
// FileHandler
//-----------------------------------------------------------------------------

struct RARUnpacks {
	union {
		rar::Unpack		unpack;
		rar::Unpack15	unpack15;
		rar::Unpack20	unpack20;
		rar::Unpack30	unpack30;
		rar::Unpack50	unpack50;
	};
	int	prev_ver;

	void	file(int ver, memory_block dest, size_t win_size) {
		if (ver == prev_ver && win_size == unpack.WinSize) {
			unpack.SetDest(dest);
		} else {
			switch (prev_ver) {
				case 15:	destruct(unpack15); break;
				case 20:
				case 26:	destruct(unpack20); break;
				case 29:	destruct(unpack30); break;
				case 50:	destruct(unpack50); break;
			}
			switch (prev_ver = ver) {
				case 15:	construct(unpack15, dest, win_size); break;
				case 20:
				case 26:	construct(unpack20, dest, win_size); break;
				case 29:	construct(unpack30, dest, win_size); break;
				case 50:	construct(unpack50, dest, win_size); break;
			}
		}
	}
	void	process(istream_ref file, bool solid) {
		switch (prev_ver) {
			case 15:	unpack15.process(file, solid); break;
			case 20:
			case 26:	unpack20.process(file, solid); break;
			case 29:	unpack30.process(file, solid); break;
			case 50:	unpack50.process(file, solid); break;
		}
	}

	RARUnpacks() : prev_ver(0)	{}
	~RARUnpacks()	{
		switch (prev_ver) {
			case 15:	destruct(unpack15); break;
			case 20:
			case 26:	destruct(unpack20); break;
			case 29:	destruct(unpack30); break;
			case 50:	destruct(unpack50); break;
		}
	}
};

class RARFileHandler : public FileHandler {
	const char*		GetExt() override { return "rar";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		RARArchive	rar;
		if (rar.open(file)) {
			ISO_ptr<anything>	p(id);

			RARUnpacks			unpacks;
			RARArchive::Entry	fh;

			while (rar.next(fh, file)) {
				if (fh.Dir)
					continue;

				auto			end	= make_skip_size(file, fh.PackSize);
				malloc_block	dest(fh.UnpSize);
#if 0
				switch (fh.UnpVer) {
					case 15: {
						rar::Unpack15	unpack(dest, fh.WinSize);
						unpack.process(file, fh.Solid);
						break;
					}
					case 20:
					case 26: {
						rar::Unpack20	unpack(dest, fh.WinSize);
						unpack.process(file, fh.Solid);
						break;
					}
					case 29: {
						rar::Unpack30	unpack(dest, fh.WinSize);
						unpack.process(file, fh.Solid);
						break;
					}
					case 50: {
						rar::Unpack50	unpack(dest, fh.WinSize);
						unpack.process(file, fh.Solid);
						break;
					}

					default:
						continue;
				}
#else
				unpacks.file(fh.UnpVer, dest, fh.WinSize);
				unpacks.process(file, fh.Solid);
#endif
				p->Append(ISO::MakePtr(fh.FileName, move(dest)));
			}
			return p;

		}
		return ISO_NULL;

	}
} rar;
