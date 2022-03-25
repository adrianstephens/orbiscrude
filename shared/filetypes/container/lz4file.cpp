#include "iso/iso_files.h"
#include "archive_help.h"
#include "filetypes/bin.h"
#include "hashes/xxh.h"
#include "codec/lz4.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	LZ4
//-----------------------------------------------------------------------------

struct LZ4F_frameInfo {
	enum {
		HEADER_SIZE_MIN			= 7,	// LZ4 Frame header size can vary, depending on selected paramaters
		HEADER_SIZE_MAX			= 19,
		BLOCK_HEADER_SIZE		= 4,	// Size in bytes of a block header in little-endian format. Highest bit indicates if block data is uncompressed
		BLOCK_CHECKSUM_SIZE		= 4,	// Size in bytes of a block checksum footer in little-endian format
		CONTENT_CHECKSUM_SIZE	= 4,	// Size in bytes of the content checksum

		MAGIC_SKIPPABLE_START   = 0x184D2A50U,
		MAGICNUMBER             = 0x184D2204U,
		BLOCKUNCOMPRESSED_FLAG  = 0x80000000U,
	};

	enum BlockSizeID {
		LZ4F_default	= 0,
		LZ4F_max64KB	= 4,
		LZ4F_max256KB	= 5,
		LZ4F_max1MB		= 6,
		LZ4F_max4MB		= 7
	};

	uint32  magic;
	union {
		uint16	flags;
		struct {
			uint8	has_dictID:1, reserved0:1, has_content_checksum:1, has_content_size:1, block_checksum:1, block_mode:1, version:2;
			uint8	reserved1:4, block_size:3, reserved2:1;
		};
	};

	uint64	content_size	= 0;	// Size of uncompressed content; 0 == unknown
	uint32	dictID			= 0;	// Dictionary ID, sent by compressor to help decoder select correct dictionary; 0 == no dictID provided

	constexpr bool	skip() const { return (magic & 0xFFFFFFF0U) == MAGIC_SKIPPABLE_START; }

	bool    read(istream_ref file) {
		if (!file.read(magic))
			return false;

		if (skip())
			return true;

		if (magic != MAGICNUMBER || !file.read(flags) || reserved0 || version != 1 || reserved1 || reserved2 || block_size < 4)
			return false;

		XXH32	xxh;
		xxh.write(flags);

		if (has_content_size) {
			file.read(content_size);
			xxh.write(content_size);
		}
		if (has_dictID) {
			file.read(dictID);
			xxh.write(dictID);
		}

		uint8	hc = file.getc();
		return	hc == ((xxh.digest() >> 8) & 0xff);
	}

};

struct LZ4F_block {
	union {
		uint32	u;
		struct {uint32 size:31, uncompressed:1; };
	};
	bool	end() const	{ return u == 0;}
};

class LZ4FileHandler : public FileHandler {
	const char* GetDescription() override { return "LZ4 encoded data"; }
	int			Check(istream_ref file) override {
		file.seek(0);
		LZ4F_frameInfo	frame;
		return frame.read(file) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		LZ4F_frameInfo	frame;
		if (!frame.read(file))
			return ISO_NULL;

		malloc_block	uncomp;
		LZ4F_block		block;
		LZ4::decoder	decoder;

		while (file.read(block) && !block.end()) {
			auto	start	= file.tell();

			if (block.uncompressed) {
				uncomp.extend(block.size).read(file);
			} else {
				uncomp += transcode(decoder, malloc_block(file, block.size));
			}
			
			file.seek(start + block.size);

			if (frame.block_checksum) {
				uint32	ck = file.get<uint32>();
				XXH32	xxh32;
				xxh32.write(uncomp.end(block.size));
				ISO_ASSERT(xxh32.digest() == ck);
			}

		}
		return ISO_ptr<ISO_openarray<uint8>>(id, make_range<uint8>(uncomp));
	}

	bool Write(ISO_ptr<void> p, ostream_ref file) override { return false; }
} lz4;
