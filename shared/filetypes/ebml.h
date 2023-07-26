#include "base/defs.h"
#include "base/strings.h"
#include "stream.h"

using namespace iso;

struct file_range {
	uint64	start, size;
	file_range() {}
	file_range(uint64 start, uint64 size = ~0ull) : start(start), size(size) {}
	uint64	end()		const	{ return unended() ? ~0 : start + size; }
	bool	unended()	const	{ return !~size; }
	void	set_end(uint64 end) { size = end - start; }
};
//-----------------------------------------------------------------------------
//	EBMB Reader
//-----------------------------------------------------------------------------

uint64 read_ebml_num(istream_ref stream, int &len);

class EBMLreader : public istream_chain, public file_range {
public:
	uint32		id;

	EBMLreader(istream_ref stream) : istream_chain(stream) {
		int		first = istream_chain::getc();
		if (first <= 0) {
			id = 0;
		} else {
			id = first;
			while (!(first & 0x80)) {
				id = (id << 8) | istream_chain::getc();
				first <<= 1;
			}
			int	len;
			size = read_packed_num(len);
			if (size == (1LL << (7 * len)) - 1)
				size = ~0u;
		}
		start	= istream_chain::tell();
	}
	~EBMLreader()			{ istream_chain::seek(end()); }
	bool		atend()		{ return istream_chain::tell() > end(); }
	istream_ref	child()		{ return istream_chain::t; }
	void		rewind()	{ istream_chain::seek(start); }

	template<typename T> T read();
	uint64			read_packed_num(int &_len)	{ return read_ebml_num(istream_chain::t, _len); }
	uint64			read_packed_num()			{ int len; return read_ebml_num(istream_chain::t, len); }
	uint64			read_uint();
	int64			read_int();
	double			read_float();
	string			read_ascii();
	malloc_block	read_binary();
};

template<> inline uint64	EBMLreader::read<uint64>()	{ return read_uint(); }
template<> inline int64		EBMLreader::read<int64>()	{ return read_int(); }
template<> inline double	EBMLreader::read<double>()	{ return read_float(); }
template<> inline string	EBMLreader::read<string>()	{ return read_ascii(); }

//-----------------------------------------------------------------------------
//	EBMB Writer
//-----------------------------------------------------------------------------

class EBMLwriter : public ostream_chain {
	streamptr	start;
	uint32		len_size;
	void		write_id(uint64 id);
	void		write_packed_num(uint64 num, int count);
	int			len_num(uint64 number);
public:
	EBMLwriter(ostream_ref stream, uint32 id, uint32 maxlen = 0) : ostream_chain(stream) {
		write_id(id);
		len_size	= maxlen ? len_num(maxlen) : 8;
		start		= tell() + len_size;
		ostream_chain::seek(start);
	}
	~EBMLwriter()	{
		streamptr	end = tell();
		uint32		len	= end - start;
		ostream_chain::seek(start - len_size);
//		ISO_ASSERT(len_num(len) <= len_size);
		write_packed_num(len, len_size);
		ostream_chain::seek(end);
	}

	void	write_void(uint32 id, uint64 size);
	void	write_uint(uint32 id, uint64 n);
	void	write_sint(uint32 id, int64 n);
	void	write(uint32 id, float f)					{ write_id(id); write_packed_num(4, 1); ostream_chain::write(floatbe(f));	}
	void	write(uint32 id, double f)					{ write_id(id); write_packed_num(8, 1); ostream_chain::write(doublebe(f));	}
	void	write(uint32 id, const void *b, size_t len)	{ write_id(id); write_packed_num(len, len_num(len)); ostream_chain::writebuff(b, len);	}
	void	write(uint32 id, const char *s)				{ write(id, s, strlen(s));	}
	void	write(uint32 id, uint64 n)					{ write_uint(id, n);	}
	void	write(uint32 id, int64 n)					{ write_sint(id, n);	}
};

//-----------------------------------------------------------------------------
//	structures
//-----------------------------------------------------------------------------

template<typename T, uint32 ID> struct EBML : holder<T> {
	using holder<T>::t;
	bool	read(istream_ref file) {
		EBMLreader	r(file);
		if (r.id != ID)
			return false;
		t = r.read<T>();
		return true;
	}
	void	write(EBMLwriter &w) {
		w.write(ID, t);
	}
};

template<uint32 ID> struct EBMLstruct {
	bool	read(istream_ref file) {
		EBMLreader	r(file);
		if (r.id != ID)
			return false;
	}
};

struct EBMLHeader {
	enum {
		VERSION	= 1,
		ID		= 0x1A45DFA3,

		// IDs in the HEADER master
		ID_EBML_Version			= 0x4286,
		ID_EBML_ReadVersion		= 0x42F7,
		ID_EBML_MaxIDLength		= 0x42F2,
		ID_EBML_MaxSizeLength	= 0x42F3,
		ID_DocType				= 0x4282,
		ID_DocTypeVersion		= 0x4287,
		ID_DocTypeReadVersion	= 0x4285,

		// general EBML types
		ID_CRC32				= 0xBF,
		ID_Void					= 0xEC,
	};
	EBML<int64,	ID_EBML_Version>		version;
	EBML<int64,	ID_EBML_ReadVersion>	readVersion;
	EBML<int64,	ID_EBML_MaxIDLength>	maxIdLength;
	EBML<int64,	ID_EBML_MaxSizeLength>	maxSizeLength;
	EBML<string,ID_DocType>				docType;
	EBML<int64,	ID_DocTypeVersion>		docTypeVersion;
	EBML<int64,	ID_DocTypeReadVersion>	docTypeReadVersion;

	bool	read(istream_ref file) {
		EBMLreader	r(file);
		return r.id == ID
			&& version.read(r)
			&& readVersion.read(r)
			&& maxIdLength.read(r)
			&& maxSizeLength.read(r)
			&& docType.read(r)
			&& docTypeVersion.read(r)
			&& docTypeReadVersion.read(r);
	}
	void	write(ostream_ref file) {
		EBMLwriter	w(file, ID);
		version.write			(w);
		readVersion.write		(w);
		maxIdLength.write		(w);
		maxSizeLength.write		(w);
		docType.write			(w);
		docTypeVersion.write	(w);
		docTypeReadVersion.write(w);
	}
};