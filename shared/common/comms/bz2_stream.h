#ifndef BZ2_STREAM_H
#define BZ2_STREAM_H

#include "bzlib.h"
#include "base/defs.h"
#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	BZIP2
//-----------------------------------------------------------------------------
class BZ2istream : public reader_mixin<BZ2istream> {
	istream_ref	file;
	bz_stream	strm;
	uint32		len;
	char		in[1024];
public:
	BZ2istream(istream_ref file, uint32 _len = 0) : file(move(file)), len(_len)	{
		clear(strm);
		BZ2_bzDecompressInit(&strm, 0, 0);
	}
	~BZ2istream()	{
		BZ2_bzDecompressEnd(&strm);
	}

	int			readbuff(void *buffer, size_t size)	{
		strm.next_out	= (char*)buffer;
		strm.avail_out	= uint32(size);

		while (strm.avail_out) {
			if (strm.avail_in == 0) {
				if (!(strm.avail_in = file.readbuff(in, 1024)))
					break;
				strm.next_in = in;
			}
			if (BZ2_bzDecompress(&strm) == BZ_STREAM_END)
				break;
		}
		return int(size - strm.avail_out);
	};

	void		seek(streamptr offset)	{ stream_skip(*this, offset - tell()); }
	int			getc()					{ int c = 0; return readbuff(&c, 1) == 1 ? c : EOF;	}
	streamptr	tell(void)				{ return strm.total_out_lo32; }
	streamptr	length(void)			{ return len; }
	bool		canseek()				{ return false; }
};

class BZ2ostream : public writer_mixin<BZ2ostream> {
	ostream_ref		file;
	bz_stream		strm;
	char			out[1024];
public:
	BZ2ostream(ostream_ref	file, int blocksize) : file(file)	{
		clear(strm);
		BZ2_bzCompressInit(&strm, blocksize, 0, 0);
		strm.next_out	= out;
		strm.avail_out	= 1024;
	}
	~BZ2ostream()	{
		while (BZ2_bzCompress(&strm, BZ_FINISH) != BZ_STREAM_END) {
			file.writebuff(out, 1024);
			strm.next_out	= (char*)out;
			strm.avail_out	= 1024;
		}
		file.writebuff(out, 1024 - strm.avail_out);
		BZ2_bzCompressEnd(&strm);
	}

	int			writebuff(const void *buffer, size_t size)	{
		strm.next_in	= (char*)buffer;
		strm.avail_in	= uint32(size);
		BZ2_bzCompress(&strm, BZ_RUN);
		while (strm.avail_out == 0) {
			file.writebuff(out, 1024);
			strm.next_out	= out;
			strm.avail_out	= 1024;
			BZ2_bzCompress(&strm, BZ_RUN);
		}
		return int(size);
	};
	int			putc(int c)		{ return writebuff(&c, 1) == 1 ? c : EOF; }
	streamptr	tell(void)		{ return strm.total_in_lo32; }
	bool		canseek()		{ return false; }
};

}	//namespace iso

#endif	// BZ2_STREAM_H
