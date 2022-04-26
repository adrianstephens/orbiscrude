#ifndef ZIP_H
#define ZIP_H

#include "zlib_stream.h"
#include "crc32.h"
#include "base/strings.h"
#include "base/array.h"
#include "base/algorithm.h"
#include "filename.h"
#include "extra/date.h"

namespace iso {

struct ZIP {
//	static const uint16 VERSION = 1;
#pragma pack(1)
	enum FLAGS : uint16 {
		NONE					= 0,
		ENCRYPTION				= 1 <<  0,
		OPTION1 				= 1 <<  1,
		OPTION2 				= 1 <<  2,
		HAS_DATADESCRIPTOR		= 1 <<  3,
		ENHANCED_DEFLATION		= 1 <<  4,
		COMPRESSED_PATCHED_DATA	= 1 <<  5,
		STRONG_ENCRYPTION		= 1 <<  6,
		LANGUAGE_ENCODING		= 1 << 11,
		MASK_HEADER_VALUES		= 1 << 13,
	};
	enum METHOD : uint16 {
		NO_COMPRESSION			= 0,
		SHRUNK					= 1,
		FACTOR1					= 2,
		FACTOR2					= 3,
		FACTOR3					= 4,
		FACTOR4					= 5,
		IMPLODED				= 6,
		DEFLATED				= 8,
		ENHANCED_DEFLATED		= 9,
		PKWARE_DCL_IMPLODED		= 10,
		BZIP2					= 12,
		LZMA					= 14,
		IBM_TERSE				= 18,
		IBM_LZ77Z				= 19,
		PPMD_I1					= 98,
	};
	enum extensions {
		ZIP64					= 1,
	};
	struct time {
		uint16	seconds2 : 5, minute : 6, hour : 5;
		time()	{}
		time(const _none&)			{ clear(*this); }
		time(const TimeOfDay& t)	: seconds2(uint16(t.Sec())), minute(t.Min()), hour(t.Hour()) {}
		operator TimeOfDay() const	{ return TimeOfDay(hour, minute, seconds2 * 2); }
	};
	struct date {
		uint16	day : 5, month : 4, years1980 : 7;
		date()	{}
		date(const _none&)			{ clear(*this); }
		date(const Date& d)			: day(d.day), month(d.month), years1980(d.year - 1980) {}
		operator Date() const		{ return Date::Days(years1980 + 1980, month, day); }
	};
	struct signature {
		uint32 _sig;
		signature(uint32 sig) : _sig(sig) {}
	};
	struct extension {
		uint16	id;
		uint16	size;
		extension()	{}
		extension(uint16 id, size_t size) : id(id), size(uint16(size - sizeof(*this))) {}
		const extension	*next()	const	{ return (const extension*)((uint8*)(this + 1) + size); }
	};
	struct extension_zip64 : extension {
		packed<uint64>	uncompressed_size;
		packed<uint64>	compressed_size;
		packed<uint64>	offset;			//Offset of local header record
		uint32			disk;
		extension_zip64()	{}
		extension_zip64(uint64 uncompressed_size, uint64 compressed_size, uint64 offset, uint32 disk = 0) : extension(ZIP64, sizeof(*this)),
			uncompressed_size(uncompressed_size), compressed_size(compressed_size), offset(offset), disk(disk)
		{}
	};

	struct file_header {			enum {sig = 0x04034b50};
		static const uint16 VERSION = 0x14;
		uint16		version;			//version needed to extract
		FLAGS		flag;				//general purpose bit flag
		METHOD		method;				//compression method
		time		mod_time;			//last mod file time
		date		mod_date;			//last mod file date
		uint32		crc;				//crc-32
		uint32		compressed_size;	// if archive is in ZIP64 format, this is 0xffffffff and the length is stored in the extra field
		uint32		uncompressed_size;	// if archive is in ZIP64 format, this is 0xffffffff and the length is stored in the extra field
		uint16		filename_length;
		uint16		extrafield_length;
		file_header()	{}
		file_header(const char* fn, METHOD method = DEFLATED) : version(VERSION), flag(NONE), method(method), mod_time(none), mod_date(none),
			crc(0), compressed_size(0), uncompressed_size(0), filename_length((uint16)strlen(fn)), extrafield_length(0)
		{}
	};
	struct datadescriptor {	enum {sig = 0x08074b50};
		uint32		crc;			//crc-32
		uint32		compressed_size;
		uint32		uncompressed_size;
	};
	struct centraldir_entry { enum {sig = 0x02014b50};
		uint16		madeby;			//version made by
		file_header	header;
		uint16		comment_length;
		uint16		disk_number_start;
		uint16		internal_file_attributes;
		uint32		external_file_attributes;
		uint32		offset;			//disk-relative offset of local header
		centraldir_entry()	{}
		centraldir_entry(const char *fn, METHOD method = DEFLATED) : header(fn, method) {}
		uint32				size()		const	{ return sizeof(*this) + header.filename_length + header.extrafield_length + comment_length; }
		count_string		filename()	const	{ return count_string((char*)(this + 1), header.filename_length); }
		const signature*	next()		const	{ return (const signature*)((char*)this + size()); }
	};

	struct centraldir_end {	enum {sig = 0x06054b50};
		uint16			disk_no;
		uint16			dir_disk;		//number of the disk with the start of the central directory
		uint16			total_disk;		//total number of entries in the central directory on this disk
		uint16			total_entries;	//total number of entries in the central directory
		uint32			dir_size;		//size of the central directory
		uint32			dir_offset;		//offset of start of central directory with respect to the starting disk number
		uint16			comment_length;	//.ZIP file comment length
	};
	struct centraldir_ptr64 {	enum {sig = 0x07064b50};
		uint32			disk;
		uint64			offset;
		uint32			num_disks;
	};
	struct centraldir_end64 {	enum {sig = 0x06064b50};
		uint64			size;		//Size of the EOCD64 - 12
		uint16			madeby;
		uint16			version;	//Version needed to extract (minimum)
		uint32			disk_no;
		uint32			dir_disk;
		packed<uint64>	total_disk;		//Number of central directory records on this disk
		packed<uint64>	total_entries;	//Total number of central directory records
		packed<uint64>	dir_size;		//Size of central directory (bytes)
		packed<uint64>	dir_offset;		//Offset of start of central directory, relative to start of archive
		//56	n	Comment (up to the size of EOCD64)
	};

	template<typename T> struct with_signature : signature, T {
		with_signature() : signature(T::sig) {}
		bool	valid() const { return _sig == T::sig; }
		const with_signature *next() const { return static_cast<const with_signature*>(T::next()); }
	};

	struct encryption {
		uint32	K0, K1, K2;
		encryption(const char *password) : K0(0x12345678), K1(0x23456789), K2(0x34567890) {
			for (const char *p = password; *p; p++)
				update_keys(*p);
		}
		void	update_keys(char p) {
			K0	= CRC32::calc(K0, p);
			K1	= (K1 + (K0 & 0xff)) * 134775813 + 1;
			K2	= CRC32::calc(K2, char(K1 >> 24));
		}
		uint8	decrypt_byte() {
			uint16	temp = K2 | 2;
			return (temp * (temp ^ 1)) >> 8;
		}
		void	decrypt(const range<char*> &r) {
			for (auto &i : r) {
				char c	= i;
				i		= c ^ decrypt_byte();
				update_keys(c);
			}
		}
	};

	class decrypt_stream : public istream_chain {
		encryption	ze;
	public:
		decrypt_stream(istream_ref stream, const encryption &ze) : istream_chain(stream), ze(ze) {}
		int			getc() {
			char c = istream_chain::getc() ^ ze.decrypt_byte();
			ze.update_keys(c);
			return c;
		}
		size_t		readbuff(void *buffer, size_t size) {
			size_t n = istream_chain::readbuff(buffer, size), i = n;
			for (char *p = (char*)buffer; i--; p++) {
				*p = *p ^ ze.decrypt_byte();
				ze.update_keys(*p);
			}
			return n;
		}
		decrypt_stream&	_clone() { return *this; }
	};

	class encrypt_stream : public ostream_chain {
		encryption	ze;
		char		temp[1024];
	public:
		encrypt_stream(ostream_ref stream, const encryption &ze) : ostream_chain(stream), ze(ze) {}
		int			putc(int c) {
			char t = c ^ ze.decrypt_byte();
			ze.update_keys(c);
			return ostream_chain::putc(t);
		}
		size_t		writebuff(const void *buffer, size_t size) {
			size_t	total = 0;
			while (size) {
				size_t	n = min(size, 1024u);
				for (int i = 0; i < n; i++) {
					char	c	= ((char*)buffer)[i];
					temp[i]		= c ^ ze.decrypt_byte();
					ze.update_keys(c);
				}
				auto	wrote = ostream_chain::writebuff(temp, n);
				total	+= wrote;
				buffer	= (char*)buffer + n;
				size	-= n;
				if (wrote != n)
					break;
			}
			return total;
		}
		encrypt_stream&	_clone()	{ return *this; }
	};


	static streamptr search_for_sig(istream_ref r, streamptr pos, uint32 sig, size_t size) {
		char	buffer[2][256];
		for (int i = 0; (pos -= 256) > 0; i = 1 - i) {
			r.seek(pos);
			if (!r.read(buffer[i]))
				break;
			for (int j = 256 - 4 - size; j >= 0; --j) {
				if (*(packed<uint32>*)(buffer[i] + j) == sig)
					return pos + j;
			}
		}
		return 0;
	}

	static streamptr get_central_dir(istream_ref r, uint64 *length) {
		char	buffer[256];
		for (streamptr pos = r.length(); (pos -= 256) > 0; ) {
			r.seek(pos);
			if (!r.read(buffer))
				break;
			for (int j = 256 - 4; j >= 0; --j) {
				auto	sig = *(packed<uint32>*)(buffer + j);
				if (sig == centraldir_end::sig) {
					r.seek(pos + j + 4);
					auto	end = r.get<centraldir_end>();
					if (~end.dir_offset) {
						if (length)
							*length = end.dir_size;
						return end.dir_offset;
					}
				} else if (sig == centraldir_ptr64::sig) {
					r.seek(pos + j + 4);
					auto	ptr = r.get<centraldir_ptr64>();
					r.seek(ptr.offset + 4);

				//} else if (sig == centraldirend64::sig) {
				//	r.seek(pos + j + 4);
					auto	end = r.get<centraldir_end64>();
					if (length)
						*length = end.dir_size;
					return end.dir_offset;
				}
			}
		}
		return 0;
	}

#pragma pack()
};

class ZIPfile0 : protected ZIP {
public:
	uint64		compressed_size;
	uint64		uncompressed_size;
	FLAGS		flags;
	METHOD		method;
	uint32		crc;
	DateTime	mod;

	malloc_block	_Extract(istream_ref r) const {
		return malloc_block(r, uncompressed_size);
	}
	bool			_Extract(ostream_ref w, istream_ref r) const {
		return stream_copy<1024>(w, r, uncompressed_size) == uncompressed_size;
	}
	istream_ptr		_Reader(istream_ref file) const {
		switch (method) {
			default:
			case NO_COMPRESSION:	return new istream_offset(file, uncompressed_size);
			case DEFLATED:			return new deflate_reader(file, uncompressed_size);
		#ifdef BZ2_STREAM_H
			case BZIP2:				return new BZ2istream(file, uncompressed_size);
		#endif
		}
	}
public:
	malloc_block	Extract(istream_ref r) const {
		switch (method) {
			case NO_COMPRESSION:	return _Extract(r);
			case DEFLATED:			return _Extract(deflate_reader(r));
		#ifdef BZ2_STREAM_H
			case BZIP2:				return _Extract(BZ2istream(r, uncompressed_size));
		#endif
			default:				return none;
		}
	}
	bool			Extract(ostream_ref w, istream_ref r) const {
		if (w.exists()) switch (method) {
			case NO_COMPRESSION:	return _Extract(w, r);
			case DEFLATED:			return _Extract(w, deflate_reader(r));
		#ifdef BZ2_STREAM_H
			case BZIP2:				return _Extract(w, BZ2istream(r, uncompressed_size));
		#endif
			default:				break;
		}
		return false;
	}
	bool			Extract(const char *fn, istream_ref r) const {
		return Extract(FileOutput(fn), r);
	}
	istream_ptr		Reader(istream_ref r, const char *password = 0) const {
		if (!(flags & ENCRYPTION))
			return _Reader(r);

		if (password) {
			encryption	ze(password);
			char		buffer[12];
			r.readbuff(buffer, 12);
			for (int i = 0; i < 12; i++)
				ze.update_keys(buffer[i] ^= ze.decrypt_byte());
			if (uint8(buffer[11]) == crc >> 24)
				return new decrypt_stream(_Reader(r), ze);
		}
		return none;
	}
	uint32			Length()		const { return uncompressed_size; }
	bool			Encrypted()		const { return !!(flags & ENCRYPTION); }
	bool			UnknownSize()	const { return !~uncompressed_size || (flags & HAS_DATADESCRIPTOR); }
};

class ZIPfile : public ZIPfile0 {
public:
	filename	fn;

	ZIPfile()	{}
	ZIPfile(const char *fn) : fn(fn) {}
	ZIPfile(istream_ref file, const centraldir_entry *cd) { InitFromCD(file, cd); }

	streamptr	InitFromLocal(istream_ref file) {
		auto	h	= file.get<file_header>();

		if (h.filename_length >= sizeof(fn))
			return 0;

		file.readbuff(fn, h.filename_length);
		fn[h.filename_length] = 0;

		compressed_size		= h.compressed_size;
		uncompressed_size	= h.uncompressed_size;
		flags				= h.flag;
		method				= h.method;
		crc					= h.crc;
		mod					= Date(h.mod_date) + TimeOfDay(h.mod_time);

		auto	extra = malloc_block(file, h.extrafield_length);
		for (auto& i : make_next_range<const extension>(extra)) {
			if (i.id == ZIP64) {
				auto	x = (extension_zip64*)&i;
				compressed_size		= x->compressed_size;
				uncompressed_size	= x->uncompressed_size;
			}
		}
		return file.tell() + (~compressed_size ? compressed_size : 0);
	}
	void		InitFromCD(istream_ref file, const centraldir_entry *cd) {
		fn = cd->filename();

		file.seek(cd->offset);
		if (file.get<uint32le>() == file_header::sig) {
			auto	h	= file.get<file_header>();
			compressed_size		= cd->header.compressed_size;
			uncompressed_size	= cd->header.uncompressed_size;
			file.seek_cur(h.filename_length + h.extrafield_length);
		}
	}
	streamptr	InitFromCD(istream_ref file) {
		auto	cd = file.get<centraldir_entry>();
		file.readbuff(fn, cd.header.filename_length);
		fn[cd.header.filename_length] = 0;

		compressed_size		= cd.header.compressed_size;
		uncompressed_size	= cd.header.uncompressed_size;
		
		uint64	offset		= cd.offset;
		auto	extra		= malloc_block(file, cd.header.extrafield_length);
		for (auto& i : make_next_range<const extension>(extra)) {
			if (i.id == ZIP64) {
				auto	x = (extension_zip64*)&i;
				compressed_size		= x->compressed_size;
				uncompressed_size	= x->uncompressed_size;
				offset				= x->offset;
			}
		}

		streamptr	next = file.tell() + cd.comment_length;
		file.seek(offset);
		
		if (file.get<uint32le>() != file_header::sig)
			return 0;

		auto	h	= file.get<file_header>();
		flags		= h.flag;
		method		= h.method;
		crc			= h.crc;
		file.seek_cur(h.filename_length + h.extrafield_length);
		return next;
	}
};


class ZIPreader : ZIP {
	istream_ref	file;
	streamptr	next;
	uint32		sig;
	bool		datadesc;

public:
	ZIPreader(istream_ref file) : file(file), next(0), sig(0), datadesc(false) {}
	bool		Next(ZIPfile &zf) {
		file.seek(next);
		if (datadesc) {
			if (file.get<uint32le>() != datadescriptor::sig)
				file.seek_cur(-4);
			datadescriptor	dd = file.get();
			unused(dd);
		}
		if ((sig = file.get<uint32le>()) != file_header::sig || !(next = zf.InitFromLocal(file)))
			return false;

		datadesc	= !!(zf.flags & HAS_DATADESCRIPTOR);
		return true;
	}
};

class ZIPreaderCD : ZIP {
	istream_ref	file;
	streamptr	next;
	uint32		sig;

public:
	ZIPreaderCD(istream_ref file) : file(file), next(get_central_dir(file, nullptr)), sig(0) {}
	operator bool() const { return !!next; }

	bool	Next(ZIPfile &zf) {
		file.seek(next);
		sig		= file.get<uint32le>();
		if (sig != centraldir_entry::sig)
			return false;

		next	= zf.InitFromCD(file);
		return !!next;
	}
};

class ZIPreaderCD2 : ZIP {
	malloc_block	cd;
	dynamic_array<holder<const centraldir_entry&>>	entries;
public:
	ZIPreaderCD2(istream_ref file) {
		uint64	len = 0;
		if (streamptr pos = get_central_dir(file, &len)) {
			file.seek(pos);
			cd = malloc_block(file, len);
			entries = make_next_range<const with_signature<centraldir_entry>>(cd);

		}
	}

	const centraldir_entry	*Find(const char *filename) {
		auto	i = lower_boundc(entries, filename, [](const centraldir_entry &cd, const char *filename) {
			return cd.filename() < filename;
		});
		if (i != entries.end() && (*i)->filename() == filename)
			return &*i;
		return 0;
	}
};

class ZIPwriter : ZIP {
	struct Entry : ZIPfile {
		uint64		offset;
		Entry(const char *name) : ZIPfile(name) {}
		bool operator<(const Entry &b) const { return fn < b.fn; }
		bool	WriteLocal(ostream_ref file);
		bool	WriteCD(ostream_ref file);
	};
	ostream_ref				file;
	dynamic_array<Entry>	centraldir;
public:
	ZIPwriter(ostream_ref file) : file(file) {}
	~ZIPwriter();

	void Write(const char *name, const memory_block &data, const DateTime &mod = DateTime::Now(), const char* password = 0, const char* random = 0);
};

} // namespace iso
#endif // ZIP_H
