#ifndef GZ_H
#define GZ_H

#include "filename.h"
#include "codec/deflate.h"
#include "codec/codec_stream.h"
#include "crc32.h"


namespace iso {

//-----------------------------------------------------------------------------
//	GZIP helpers
//-----------------------------------------------------------------------------
#pragma pack(1)
struct GZmember {
	enum FLAGS {
		HCRC	= 1 << 1,
		TEXTRA	= 1 << 2,
		NAME	= 1 << 3,
		COMMENT	= 1 << 4,
	};
	uint8	id1, id2, cm, flag;
	uint32	mtime;
	uint8	xfl, os;

	GZmember() : id1(0x1f), id2(0x8b), cm(8), flag(0), mtime(0), xfl(0), os(0) {}
	bool	valid() const { return id1 == 0x1f && id2 == 0x8b && cm == 8; }
};
#pragma pack()

struct GZheader {
	filename	name;
	uint16		crc16;

	GZheader(const char *name = 0) : name(name), crc16(0) {}

	bool read(istream_ref file) {
		GZmember	gm	= file.get();

		if (!gm.valid())
			return false;

		if (gm.flag & GZmember::TEXTRA) {
			uint16	len = file.get();
			file.seek_cur(len);
		}
		if (gm.flag & GZmember::NAME)
			for (int i = 0; name[i++] = file.get(););

		if (gm.flag & GZmember::COMMENT)
			while (file.get());

		if (gm.flag & GZmember::HCRC)
			crc16 = file.get();
		return true;
	}

	bool write(ostream_ref file) const {
		GZmember	gm;
		if (!name.blank())
			gm.flag |= GZmember::NAME;
		if (crc16)
			gm.flag |= GZmember::HCRC;

		return file.write(gm)
			&& (!(gm.flag & GZmember::NAME) || file.write(terminated(name)))
			&& (!(gm.flag & GZmember::HCRC)	|| file.write(crc16));
	}
};

class GZistream : public hash_reader<CRC_hasher<CRC32>, codec_reader<deflate_decoder, reader_intf, 1024>> {
public:
	GZistream(istream_ref stream) : B(stream) {}
	~GZistream() {
		if (~tell())
			ISO_VERIFY(Check());
	}
	bool	Check() {
		uint32	stored_crc	= get_stream().get<uint32le>();
		uint32	stored_len	= get_stream().get<uint32le>();
		return stored_len == tell() && stored_crc == digest();
	}
};

class GZostream : public hash_writer<CRC_hasher<CRC32>, codec_writer<deflate_encoder, writer_intf, 1024>> {
public:
	GZostream(ostream_ref stream) : B(stream) {}
	~GZostream() {
		if (size_t len = tell()) {
			get_stream().write<uint32le>(digest());
			get_stream().write<uint32le>(uint32(len));
		}
	}
};

} // namespace iso
#endif // GZ_H
