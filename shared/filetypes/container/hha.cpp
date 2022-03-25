#include "iso/iso_files.h"
#include "codec/deflate.h"
#include "codec/lzma.h"

using namespace iso;

class HHAFileHandler : public FileHandler {
	struct HHAheader : littleendian_types {
		enum {MAGIC = 0xac2ff34f};
		uint32	magic;
		uint32	version;
		uint32	dictionary_size;
		uint32	num_entries;
	};

	struct HHAentry : littleendian_types {
		uint32	dir, name, compressed, offset, size, compressed_size;
	};

	const char*		GetExt() override { return "hha";	}
	const char*		GetDescription() override { return "Hothead Archive"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} hha;

ISO_ptr<void> HHAFileHandler::Read(tag id, istream_ref file) {
	HHAheader header = file.get();

	if (header.magic != HHAheader::MAGIC)
		return ISO_NULL;

	ISO_ptr<anything> t(id);

	malloc_block	dictionary(file, header.dictionary_size);

	for (int i = 0; i < header.num_entries; i++) {
		file.seek(sizeof(HHAheader) + header.dictionary_size + i * sizeof(HHAentry));
		HHAentry	entry = file.get();

		if (entry.compressed)
			continue;

		if (entry.offset > 0x300000)
			entry.offset += 0x1000;

		ISO_ptr<ISO_openarray<uint8> >	p((char*)dictionary + entry.name);
		file.seek(entry.offset);
		p->Create(entry.size);

		switch (entry.compressed) {
			case 0:
				file.readbuff(*p, entry.size);
				break;
			case 1: {
				malloc_block	in(file, entry.compressed_size);
				transcode(deflate_decoder(), *p, (const_memory_block)in);
				break;
			}
			case 2:	//LZMA
				malloc_block	in(file, entry.compressed_size);
				transcode(lzma::Decoder(), *p, in);
				break;
		}
		t->Append(p);
	}

	return t;
}
