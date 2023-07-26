#include "codec/lha.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "archive_help.h"
#include "bin.h"
#include "codec/vlc.h"
#include "codec/huffman.h"
#include "extra/date.h"
#include "crc32.h"

using namespace iso;

static const char *method_names[] = {
	"-lh0-",
	"-lh1-",
	"-lh2-",
	"-lh3-",
	"-lh4-",
	"-lh5-",
	"-lh6-",
	"-lh7-",
	"-lzs-",
	"-lz5-",
	"-lz4-",
	"-lhd-",
	"-pm0-",
	"-pm2-",
};

class LzHeader {
	enum EXTEND {
		EXTEND_GENERIC	= 0,
		EXTEND_UNIX		= 'U',
		EXTEND_MSDOS	= 'M',
		EXTEND_MACOS	= 'm',
		EXTEND_OS9		= '9',
		EXTEND_OS2		= '2',
		EXTEND_OS68K	= 'K',
		EXTEND_OS386	= '3', // OS-9000???
		EXTEND_HUMAN	= 'H',
		EXTEND_CPM		= 'C',
		EXTEND_FLEX		= 'F',
		EXTEND_RUNSER	= 'R',
		EXTEND_TOWNSOS	= 'T',
		EXTEND_XOSK		= 'X', // OS-9 for X68000 (?)
		EXTEND_JAVA		= 'J',
	};
	enum ATTRIBUTE {
		ATTR_READONLY	= 1 << 0,
		ATTR_HIDDEN		= 1 << 1,
		ATTR_SYSTEM		= 1 << 2,
		ATTR_VOLUME		= 1 << 3,
		ATTR_DIRECTORY	= 1 << 4,
		ATTR_ARCHIVE	= 1 << 5,
	};
	enum EXTENSION {
		EXT_CRC			= 0,
		EXT_FILENAME	= 1,
		EXT_DIRECTORY	= 2,
		EXT_MSDOS_ATTR	= 0x40,
		EXT_WIN_TIME	= 0x41,
		EXT_SIZES64		= 0x42,
		EXT_UNIX_PERM	= 0x50,
		EXT_UNIX_IDS	= 0x51,
		EXT_UNIX_GROUP	= 0x52,
		EXT_UNIX_USER	= 0x53,
		EXT_UNIX_TIME	= 0x54,
		EXT_MULTI_DISK	= 0x39,			// multi-disk header
		EXT_COMMENT		= 0x3f,			// uncompressed comment
	//	EXT_AUTHENT		= 0x48-0x4f(?),	// reserved for authenticity verification
		EXT_ENCAPS		= 0x7d,			// encapsulation
		EXT_PLATFORM	= 0x7e,			// extended attribute - platform information
		EXT_PERMISSION	= 0x7f,			// extended attribute - permission, owner-id and timestamp
		//(level 3 on OS/2)
		EXT_DICT_4096	= 0xc4,			// compressed comment (dict size: 4096)
		EXT_DICT_8192	= 0xc5,			// compressed comment (dict size: 8192)
		EXT_DICT_16384	= 0xc6,			// compressed comment (dict size: 16384)
		EXT_DICT_32768	= 0xc7,			// compressed comment (dict size: 32768)
		EXT_DICT_65536	= 0xc8,			// compressed comment (dict size: 65536)
	//	EXT_OS_SPECIFIC	= 0xd0-0xdf(?),	// operating systemm specific information
	//	EXT_ENCAPS2		= 0xfc,			// encapsulation (another opinion)
	//	EXT_PLATFORM2	= 0xfe,			// extended attribute - platform information(another opinion)
	//	EXT_PERMISSION2	= 0xff,			// extended attribute - permission, owner-id and timestamp
	};

	enum {
		FILENAME_LENGTH				= 1024,
		CURRENT_UNIX_MINOR_VERSION	= 0,
	};

	static uint32					read_string(istream_ref file, char *p, uint32 limit) {
		uint32	len		= file.getc();
		uint32	len2	= (uint32)file.readbuff(p, min(len, limit));
		p[len2] = 0;
		return len2;
	}
	template<int N> static uint32	read_string(istream_ref file, char (&p)[N]) {
		return read_string(file, p, N - 1);
	}
	static uint32					read_string(istream_ref file, uint32 length, char *p, uint32 limit) {
		uint32 i = (int)file.readbuff(p, min(length, limit));
		if (length > limit)
			file.seek_cur(length - limit);
		p[i] = 0;
		return i;
	}
	template<int N> static uint32	read_string(istream_ref file, uint32 length, char (&p)[N]) {
		return read_string(file, length, p, N - 1);
	}

	static uint32	write_buffer(ostream_ref file, const void *p, size_t len) {
		file.putc((uint8)len);
		return file.writebuff(p, len);
	}
	static uint32	write_string(ostream_ref file, const char *p, size_t limit) {
		return write_buffer(file, p, min(strlen(p), limit));
	}
	static bool		write_ext(ostream_ref file, EXTENSION ID, const void *p, size_t len) {
		return len == 0 || (file.write(uint16(len + 3)) && file.putc(ID) && file.writebuff(p, len));
	}
	static bool		write_ext(ostream_ref file, EXTENSION ID, const char *p) {
		return write_ext(file, ID, p, strlen(p));
	}
	template<typename T> static bool write_ext(ostream_ref file, EXTENSION ID, const T& t) {
		return write_ext(file, ID, &t, sizeof(T));
	}

	size_t	get_extended_header(istream_ref file, uint32 size_length, bool get_time, uint16 *crc);

public:
	struct Time {
		union {
			uint32	u;
			struct {
				uint32  sec_2:5, min:6, hour:5, day:5, month:4, year1980:7;
			};
		};
		Time() : u(0) {}
		Time(Date d, TimeOfDay t) : sec_2(int(t.Sec() / 2)), min(t.Min()), hour(t.Hour()), day(d.day), month(d.month), year1980(d.year - 1980) {}
		Time(DateTime dt) : Time(Date(dt.Day()), TimeOfDay(dt.TimeOfDay())) {}
		operator DateTime() const {
			return DateTime::Day(Date::Days(year1980 + 1980, month - 1, day)) + Duration::Hours(hour) + Duration::Mins(min) + Duration::Secs(sec_2 * 2);
		}
	};
	struct common {
		union {
			struct {
				uint8	header_size8, checksum;		// level 0, 1
			};
			packed<uint16>	header_size16;			// level 2
			packed<uint16>	size_length;			// level 3
		};
		char			method[5];					//2
		packed<uint32>	packed_size, original_size;	//7
		packed<Time>	last_modified;				//15
		uint8			attribute;					//19
		uint8			header_level;				//20

		common() {}
		common(const LzHeader& h, int header_level) : header_size16(0), packed_size(h.packed_size), original_size(h.original_size), last_modified(h.last_modified), attribute(h.attribute), header_level(header_level) {
			memcpy(method, method_names[h.method], 5);
		}
	};
	struct ext0_unix {
		uint8			minor_ver;
		packed<uint32>	last_modified;
		packed<uint16>	mode, uid, gid;
		ext0_unix() {}
		ext0_unix(const LzHeader& h) : minor_ver(h.unix_minor_ver), last_modified(DateTime(h.last_modified).ToUnixTime()), mode(h.unix_mode), uid(h.unix_ids.uid), gid(h.unix_ids.gid) {}
	};

	LHA::METHOD	method;
	uint64		packed_size		= 0;
	uint64		original_size	= 0;
	uint8		attribute		= ATTR_ARCHIVE;

	Time		created;
	Time		last_modified;
	Time		last_accessed;

	char		name[FILENAME_LENGTH];
	bool		has_crc			= false;
	uint16		crc				= 0;
	uint16		header_crc		= 0;
	uint8		extend_type		= EXTEND_GENERIC;

	uint8		unix_minor_ver	= CURRENT_UNIX_MINOR_VERSION;
	uint16le	unix_mode;
	struct unix_ids {
		uint16le	gid, uid;
	} unix_ids;
	char		user[256];
	char		group[256];

	LzHeader()	{}
	LzHeader(const char *_name, size_t _original_size, LHA::METHOD _method = LHA::METHOD_LZHUFF0, EXTEND _extend_type = EXTEND_GENERIC) : method(_method), original_size(_original_size), extend_type(_extend_type) {
		strcpy(name, _name);
		last_modified	= DateTime::Now();
	}

	bool		read(istream_ref file);
	size_t		header_size(int header_level) const;
	void		write(ostream_ref file, int header_level) const;
};

//-----------------------------------------------------------------------------
//	checksums
//-----------------------------------------------------------------------------

int8 calc_sum(const void *p, size_t len, int8 sum = 0) {
	for (const uint8 *pc = (const uint8*)p; len--;)
		sum += *pc++;
	return sum;
}

class istream_checksum : public istream_offset {
public:
	uint8	sum;

	istream_checksum(istream_ref stream, streamptr end, uint8 sum = 0) : istream_offset(copy(stream), end), sum(sum)	{}

	int	getc() {
		int	c = istream_offset::getc();
		sum += c;
		return c;
	}
	size_t	readbuff(void *buffer, size_t size) {
		size	= istream_offset::readbuff(buffer, size);
		sum		= calc_sum(buffer, size, sum);
		return size;
	}
	void seek(streamptr offset) {
		stream_skip(*this, offset - tell());
	}
	void seek_cur(streamptr offset) {
		stream_skip(*this, offset);
	}
	void seek_end(streamptr offset) {
		seek(length() - offset);
	}
};
class ostream_checksum : public ostream_chain {
public:
	uint8	sum;

	ostream_checksum(ostream_ref stream, uint8 sum = 0) : ostream_chain(stream), sum(sum)	{}

	int	putc(int c) {
		sum += c;
		return ostream_chain::putc(c);
	}
	size_t	writebuff(const void *buffer, size_t size) {
		sum		= calc_sum(buffer, size, sum);
		return ostream_chain::writebuff(buffer, size);
	}
};

class istream_crc16 : public istream_chain {
public:
	uint16	crc;

	istream_crc16(istream_ref stream, uint16 crc = 0) : istream_chain(stream), crc(crc) {}

	int	getc() {
		int	c = istream_chain::getc();
		crc = CRC16::calc(c, crc);
		return c;
	}
	size_t	readbuff(void *buffer, size_t size) {
		size = istream_chain::readbuff(buffer, size);
		crc = CRC16::calc(buffer, size, crc);
		return size;
	}
	void seek(streamptr offset) {
		stream_skip(*this, offset - tell());
	}
	void seek_cur(streamptr offset) {
		stream_skip(*this, offset);
	}
	void seek_end(streamptr offset) {
		seek(length() - offset);
	}
};
class ostream_crc16 : public ostream_chain {
public:
	uint16	crc;

	ostream_crc16(ostream_ref stream, uint16 crc = 0) : ostream_chain(stream), crc(crc)	{}

	int	putc(int c) {
		crc = CRC16::calc(c, crc);
		return ostream_chain::putc(c);
	}
	size_t	writebuff(const void *buffer, size_t size) {
		crc = CRC16::calc(buffer, size, crc);
		return ostream_chain::writebuff(buffer, size);
	}
};

//-----------------------------------------------------------------------------
//	header
//-----------------------------------------------------------------------------

size_t LzHeader::get_extended_header(istream_ref file, uint32 size_length, bool get_time, uint16 *crc) {
	char    dirname[FILENAME_LENGTH] = {0};
	size_t	whole_size = 0;

	while (uint32 header_size = size_length == 2 ? file.get<uint16>() : file.get<uint32>()) {
		EXTENSION	ext			= (EXTENSION)file.getc();
		uint32		data_size	= header_size - (size_length + 1);
		streamptr	data_end	= file.tell() + data_size;

		whole_size += header_size;

		switch (ext) {
			case EXT_CRC: {
				auto	save = *crc;
				file.read(header_crc);
				*crc = CRC16::calc('\0', CRC16::calc('\0', save));
				break;
			}
			case EXT_FILENAME:
				read_string(file, data_size, name);
				break;
			case EXT_DIRECTORY:
				read_string(file, data_size, dirname);
				break;
			case EXT_MSDOS_ATTR:
				attribute = file.get<uint16>();
				break;
			case EXT_WIN_TIME: {
				created = DateTime(file.get<FILETIME>());
				auto	ft = file.get<FILETIME>();
				if (get_time)
					last_modified = DateTime(ft);
				last_accessed = DateTime(file.get<FILETIME>());
				break;
			}
			case EXT_SIZES64:
				file.read(packed_size);
				file.read(original_size);
				break;
			case EXT_UNIX_PERM:
				file.read(unix_mode);
				break;
			case EXT_UNIX_IDS:
				file.read(unix_ids);
				break;
			case EXT_UNIX_GROUP:
				read_string(file, data_size, group);
				break;
			case EXT_UNIX_USER:
				read_string(file, data_size, user);
				break;
			case EXT_UNIX_TIME:
				file.read(last_modified);
				break;
			default:
				//file.seek_cur(data_size);
				break;
		}
		file.seek(data_end);
		//ISO_ASSERT(file.tell() == data_end);
	}

	// concatenate dirname and filename
	if (auto dir_length = strlen(dirname)) {
		auto	name_length = strlen(name);
		if (name_length + dir_length >= FILENAME_LENGTH)
			name[FILENAME_LENGTH - dir_length - 1] = 0;
		strcat(dirname, name);
		strcpy(name, dirname);
	}

	return whole_size;
}


/*
* LEVEL 0 HEADER
*
* offset  size  field name
* ----------------------------------
*     0      1  header size    [*1]
*     1      1  header sum
*            ---------------------------------------
*     2      5  method ID                         ^
*     7      4  packed size    [*2]               |
*    11      4  original size                     |
*    15      2  time                              |
*    17      2  date                              |
*    19      1  attribute                         | [*1] header size (X+Y+22)
*    20      1  level (0x00 fixed)                |
*    21      1  name length                       |
*    22      X  pathname                          |
* X +22      2  file crc (CRC-16)                 |
* X +24      Y  ext-header(old style)             v
* -------------------------------------------------
* X+Y+24        data                              ^
*                 :                               | [*2] packed size
*                 :                               v
* -------------------------------------------------
*
* LEVEL 1 HEADER
*
* offset   size  field name
* -----------------------------------
*     0       1  header size   [*1]
*     1       1  header sum
*             -------------------------------------
*     2       5  method ID                        ^
*     7       4  skip size     [*2]               |
*    11       4  original size                    |
*    15       2  time                             |
*    17       2  date                             |
*    19       1  attribute (0x20 fixed)           | [*1] header size (X+Y+25)
*    20       1  level (0x01 fixed)               |
*    21       1  name length                      |
*    22       X  filename                         |
* X+ 22       2  file crc (CRC-16)                |
* X+ 24       1  OS ID                            |
* X +25       Y  ???                              |
* X+Y+25      2  next-header size                 v
* -------------------------------------------------
* X+Y+27      Z  ext-header                       ^
*                 :                               |
* -----------------------------------             | [*2] skip size
* X+Y+Z+27       data                             |
*                 :                               v
* -------------------------------------------------
*
* LEVEL 2 HEADER
*
* offset   size  field name
* --------------------------------------------------
*     0       2  total header size [*1]           ^
*             -----------------------             |
*     2       5  method ID                        |
*     7       4  packed size       [*2]           |
*    11       4  original size                    |
*    15       4  time                             |
*    19       1  RESERVED (0x20 fixed)            | [*1] total header size
*    20       1  level (0x02 fixed)               |      (X+26+(1))
*    21       2  file crc (CRC-16)                |
*    23       1  OS ID                            |
*    24       2  next-header size                 |
* -----------------------------------             |
*    26       X  ext-header                       |
*                 :                               |
* -----------------------------------             |
* X +26      (1) padding                          v
* -------------------------------------------------
* X +26+(1)      data                             ^
*                 :                               | [*2] packed size
*                 :                               v
* -------------------------------------------------
*
* LEVEL 3 HEADER
*
* offset   size  field name
* --------------------------------------------------
*     0       2  size field length (4 fixed)      ^
*     2       5  method ID                        |
*     7       4  packed size       [*2]           |
*    11       4  original size                    |
*    15       4  time                             |
*    19       1  RESERVED (0x20 fixed)            | [*1] total header size
*    20       1  level (0x03 fixed)               |      (X+32)
*    21       2  file crc (CRC-16)                |
*    23       1  OS ID                            |
*    24       4  total header size [*1]           |
*    28       4  next-header size                 |
* -----------------------------------             |
*    32       X  ext-header                       |
*                 :                               v
* -------------------------------------------------
* X +32          data                             ^
*                 :                               | [*2] packed size
*                 :                               v
* -------------------------------------------------
*/

bool LzHeader::read(istream_ref file) {
	clear(*this);
	unix_mode	= int(FILEMODE::FREG | all(FILEACCESS::RW));
	method		= LHA::METHOD_UNKNOWN;

	common	common;
	if (!file.read(common))
		return false;

	for (int i = 0; i < num_elements(method_names); i++) {
		if (memcmp(method_names[i], common.method, 5) == 0) {
			method = LHA::METHOD(i);
			break;
		}
	}

	packed_size		= common.packed_size;
	original_size	= common.original_size;
	last_modified	= common.last_modified;
	attribute		= common.attribute;

	switch (common.header_level) {
		case 0: {
			istream_checksum	file2(file, common.header_size8 + 2 - sizeof(common), calc_sum((uint8*)&common + 2, sizeof(common) - 2));

			read_string(file2, name);
			has_crc		= file2.read(crc);
			if (!file2.eof()) {
				extend_type = file.getc();
				if (extend_type == EXTEND_UNIX) {
					ext0_unix	unix;
					if (file2.read(unix)) {
						unix_minor_ver	= unix.minor_ver;
						last_modified	= DateTime::FromUnixTime(Duration::Secs(unix.last_modified));
						unix_mode		= unix.mode;
						unix_ids.gid	= unix.gid;
						unix_ids.uid	= unix.uid;
					} else {
						extend_type = EXTEND_GENERIC;
					}
				}
				file2.seek_end(0);
			}
			return common.checksum == file2.sum;
		}
		case 1: {
			istream_checksum	file2(file, common.header_size8 + 2 - sizeof(common), calc_sum((uint8*)&common + 2, sizeof(common) - 2));

			read_string(file2, name);
			has_crc		= file2.read(crc);
			extend_type = file2.getc();

			file2.seek_end(0);
			//while (!file2.eof())
			//	file2.getc();

			if (common.checksum != file2.sum)
				return false;

			file.seek_cur(-2);
			size_t	extend_size = get_extended_header(file, 2, true, nullptr);
			packed_size -= extend_size;
			return true;
		}
		case 2: {
			istream_crc16	file2(file, CRC16::calc(&common, sizeof(common)));

			has_crc		= file2.read(crc);
			extend_type	= file2.getc();

			get_extended_header(file2, 2, false, &file2.crc);

			if (file2.tell() < common.header_size16)
				file2.getc();

			return file2.tell() == common.header_size16 && header_crc == file2.crc;
		}
		case 3: {
			istream_crc16	file2(file, CRC16::calc(&common, sizeof(common)));

			has_crc		= file2.read(crc);
			extend_type	= file2.getc();

			uint32	header_size	= file2.get<uint32>();
			get_extended_header(file2, common.size_length, false, &file2.crc);
			return file2.tell() == header_size && header_crc != file2.crc;
		}

		default:
			return false;
	}

}

#define LHA_PATHSEP						0xff

size_t LzHeader::header_size(int header_level) const {
	auto		size		= sizeof(common);
	const char	*basename	= name;
	if (header_level != 0) {
		if (const char *s = strrchr(name, LHA_PATHSEP))
			basename	= s + 1;
	}
	size_t	name_length	= strlen(basename);

	switch (header_level) {
		case 0: {
			if (extend_type)
				size += 1 + sizeof(ext0_unix);
			size = min(size + 1 + sizeof(crc) + name_length, 255);
			break;
		}
		case 1: {
			size += 1 + sizeof(crc) + 1;

			if (size + name_length < 256)
				size += name_length;
			else
				size += 3 + name_length;

			if (basename > name)
				size += 3 + (name - basename);

			if (extend_type) {
				size += 3 + sizeof(unix_mode);
				size += 3 + sizeof(unix_ids);
				size += 3 + sizeof(group);
				size += 3 + sizeof(user);
				size += 3 + sizeof(last_modified);
			}

			size += 2;
			break;
		}
		case 2: {
			size += sizeof(crc) + 1;
			size += 3 + sizeof(uint16);
			size += 3 + name_length;

			if (basename > name)
				size += 3 + name - basename;

			if (extend_type) {
				size += 3 + sizeof(unix_mode);
				size += 3 + sizeof(unix_ids);
				size += 3 + sizeof(group);
				size += 3 + sizeof(user);
			}

			size += 2;
			if (!(size & 0xff))
				++size;
			break;
		}
		default:
			break;
	}

	return size;
}


void LzHeader::write(ostream_ref file, int header_level) const {
	common	common(*this, header_level);

	// leave room for common
	auto	pos = file.tell();
	file.seek_cur(sizeof(common));

	const char	*basename	= name;
	if (const char *s = strrchr(name, LHA_PATHSEP))
		basename	= s + 1;

	switch (header_level) {
		case 0: {
			ostream_checksum	file2(file);

			size_t	limit		= 255 - (sizeof(common) + (extend_type ? sizeof(ext0_unix) + 1 : 0) + 1 + sizeof(crc));
			write_string(file2, name, limit);
			file2.write(crc);

			if (extend_type) {
				file2.putc(EXTEND_UNIX);
				file2.write(ext0_unix(*this));
			}

			common.checksum		= calc_sum(&common, sizeof(common)) + file2.sum;
			common.header_size8 = file.tell();
			break;
		}
		case 1: {
			ostream_checksum	file2(file);

			size_t	name_length	= strlen(basename);
			size_t	limit		= 255 - (sizeof(common) + 1 + sizeof(crc) + 1);
			write_buffer(file2, basename, name_length > limit ? 0 : name_length);

			file2.write(crc);
			file2.putc(extend_type);

			size_t header_size8 = file.tell();

			if (name_length > limit)
				write_ext(file, EXT_FILENAME, basename, name_length);

			if (basename > name)
				write_ext(file, EXT_DIRECTORY, name, name - basename);

			if (extend_type) {
				write_ext(file2, EXT_UNIX_PERM, unix_mode);
				write_ext(file2, EXT_UNIX_IDS, unix_ids);
				write_ext(file2, EXT_UNIX_GROUP, group);
				write_ext(file2, EXT_UNIX_USER, user);
				write_ext(file2, EXT_UNIX_TIME, last_modified);
			}

			file.write((uint16)0);			// next header size

			common.packed_size += uint32(file.tell() - header_size8);
			common.checksum		= calc_sum(&common, sizeof(common)) + file2.sum;
			common.header_size8 = (uint8)header_size8;
			break;
		}
		case 2: {
			ostream_crc16	file2(file, CRC16::calc(&common, sizeof(common)));

			file2.write(crc);
			file2.putc(extend_type);

			write_ext(file2, EXT_CRC, (uint16)0);
			write_ext(file2, EXT_FILENAME, basename);

			if (basename > name)
				write_ext(file2, EXT_DIRECTORY, name, name - basename);

			if (extend_type) {
				write_ext(file2, EXT_UNIX_PERM, unix_mode);
				write_ext(file2, EXT_UNIX_IDS, unix_ids);
				write_ext(file2, EXT_UNIX_GROUP, group);
				write_ext(file2, EXT_UNIX_USER, user);
			}

			file2.write((uint16)0);			// next header size

			size_t	header_size16 = file.tell();
			if ((header_size16 & 0xff) == 0) {
				// cannot put zero at the first byte on level 2 header. adjust header size
				file2.putc(0);	// padding
				++header_size16;
			}
			common.header_size16 = (uint16)header_size16;
			break;
		}
		default:
			break;
	}

	auto	end = file.tell();
	file.seek(pos);
	file.write(common);
	file.seek(end);
}

//-----------------------------------------------------------------------------
//	LZHFileHandler
//-----------------------------------------------------------------------------

class LZHFileHandler : FileHandler {
	const char*		GetExt() override { return "lzh";		}
	const char*		GetDescription() override { return "LH archive";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
	ISO_ptr<anything>	p(id);

	LzHeader        hdr;
		while (hdr.read(file)) {
			malloc_block	decomp(hdr.original_size);
			memory_writer	mem(decomp);
		
			ostream_crc16	ofile(mem);
			auto	start		= file.tell();
			decode_lzhuf(file, ofile, hdr.original_size, hdr.method);
			auto	read_size	= file.tell() - start;
			uint16	crc			= ofile.crc;

			ISO_ASSERT(read_size == hdr.packed_size);
			ISO_ASSERT(mem.tell() == hdr.original_size);
			ISO_ASSERT(crc == hdr.crc);

			p->Append(ISO::MakePtr(hdr.name, decomp));
		}
		return p;
	}

	bool			Write(ISO::ptr<void> p, ostream_ref file) override {
		if (auto p2 = ISO_conversion::convert<anything>(p)) {
			for (auto i : *p2) {
				if (memory_block data = GetRawData(i)) {
					LzHeader        hdr(i.ID().get_tag(), data.length());
					auto			htell	= file.tell();
					auto			hsize	= hdr.header_size(0);

					memory_reader		mem(data);
					istream_crc16		ifile(mem);
					auto	start		= file.tell();
					encode_lzhuf(mem, file, hdr.original_size, hdr.method);
					hdr.packed_size		= file.tell() - start;
					hdr.crc				= ifile.crc;

					auto	save = make_save_pos(file, htell);
					hdr.write(file, 0);
				}
			}

		}
		return false;
	}
} lzh;

class LHAFileHandler : LZHFileHandler {
	const char*		GetExt() override { return "lha";		}
} lha;
