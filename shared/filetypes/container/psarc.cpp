#include "iso/iso_files.h"
#include "archive_help.h"
#include "base/bits.h"
#include "base/algorithm.h"
#include "hashes/md5.h"
#include "comms/zlib_stream.h"
#include "comms/bz2_stream.h"

using namespace iso;

enum PSARCMSELF {
	kMSELFMagicNumber	= 0x4D534600,	// Magic number that indicates this is a .psarc.mself file == 'MSF\0'
	kPSARCMSELFHeaderOffset = 65536	// Offset of the PSARC header from the beginning of the file.
};

struct TOCEntry {
	uint8	md5[16];
	uintn<4,true>	blockIndex;
	uintn<5,true>	originalSize;
	uintn<5,true>	startOffset;
};

struct PSARCHeader {
	enum {
		MagicNumber	= 0x50534152,	//'PSAR'
	};
	enum ALGORITHM {
		Uncompressed = 0,
		ZLIB	= 0x7A6C6962,
		LZO1X	= 0x6C7A6D61,
		LZO1Y	= 0x6C7A6F78,
		LZMA	= 0x6C7A6F79,
		BZIP2	= 0x627A7032,
	};
	enum GLOBALFLAGS {
		IgnoreCase	= 1 << 0,
		Absolute	= 1 << 1,
		SortedTOC	= 1 << 2,
	};
	uint32be	magic;
	struct {uint16be major, minor;} version;
	uint32be	algorithm;
	uint32be	tocSize;
	uint32be	tocEntrySize;
	uint32be	numFiles;
	uint32be	blockSize;
	uint32be	archiveFlags;

	inline uint32	getBlockEntrySize() const {
		return (log2(blockSize) + 7) / 8;
	}
	inline uint32	getNumBlocks() const {
		return uint32((tocSize - numFiles * tocEntrySize) / getBlockEntrySize());
	}
	inline uint32	blockIndex(uint64 bytes) const {
		return bytes ? uint32((bytes - 1) / blockSize) : 0;
	}
};

struct PSARCReader {
	PSARCHeader		h;
	malloc_block	toc;
	malloc_block	temp;
	uint64			offsetToTOC, offsetToData;
	dynamic_array<uint32>	block_starts;
	uint8			block_entry;

	inline uint32	getBlockCompSize(uint32 index) const {
		uint8	size	= block_entry;
		uint8	*p		= (uint8*)toc + h.tocEntrySize * h.numFiles + index * size;
		uint32	i		= 0;
		while (size--)
			i = (i << 8) | *p++;
		return i;
	}

	inline uint32	bytesToBlockIndex(uint64 bytes) const {
		return bytes ? uint32((bytes - 1) / h.blockSize) : 0;
	}

	const TOCEntry *getManifest() const	{
		return toc;
	}

	const TOCEntry *lookupFile(const MD5::CODE &digest) const {
		const TOCEntry *e = toc;
		for (int n = h.numFiles; n--; e = (const TOCEntry*)((uint8*)e + h.tocEntrySize)) {
			if (memcmp(e->md5, digest.begin(), 16) == 0)
				return e;
		}
		return 0;
	}
	const TOCEntry *lookupFile(const char *path) const {
		return lookupFile(MD5(path));
	}
	uint32	Load(istream_ref file, const TOCEntry *e, void *data);

	PSARCReader(istream_ref file);
	bool	check() {
		return h.magic == PSARCHeader::MagicNumber;
	}
};

PSARCReader::PSARCReader(istream_ref file) : block_starts(0) {
	file.read(h);
	if (!check())
		return;

	if (h.version.major >= 3) {
		offsetToTOC		= file.get<uint64be>();
		offsetToData	= file.get<uint64be>();
	} else {
		offsetToTOC		= file.tell();
		offsetToData	= h.tocSize;
	}
	file.seek(offsetToTOC);
	file.readbuff(toc.create(h.tocSize), h.tocSize);

	uint32	block		= h.blockSize;
	temp.create(block);

	uint32	nf		= h.numFiles;
//	uint32	nb		= h.getNumBlocks();
	block_starts.resize(nf);
	block_entry		= h.getBlockEntrySize();

	uint32	*b		= block_starts;
	uint32	bi		= 0;
	for (const TOCEntry *e = toc; nf--; ++e) {
		*b++		= bi;
		uint64	x	= h.blockIndex(e->originalSize) + 1;
		bi	+= x;
	}
}

uint32	PSARCReader::Load(istream_ref file, const TOCEntry *e, void *data) {
	file.seek((iso::uint64)e->startOffset);
	uint64	size	= e->originalSize;
	uint32	block	= h.blockSize;

//	for (uint32 i = block_starts[e - (TOCEntry*)toc]; size; ++i) {
	for (uint32 i = e->blockIndex; size; ++i) {
		uint32	comp = getBlockCompSize(i);
		uint32	read = 0;

		if (comp == 0 || comp == size || h.algorithm == PSARCHeader::Uncompressed) {
			read = file.readbuff(data, comp == 0 ? block : comp);

		} else {
			file.readbuff(temp, comp);
			switch (h.algorithm) {
				case PSARCHeader::ZLIB:
					read	= deflate_decoder(0).process((uint8*)data, (uint8*)data + size, memory_reader(const_memory_block(temp, comp)), TRANSCODE_NONE) - (uint8*)data;
					break;
				case PSARCHeader::BZIP2:
					read = BZ2istream(file, size).readbuff(data, size);
					break;

				case PSARCHeader::LZO1X:
				case PSARCHeader::LZO1Y:
				case PSARCHeader::LZMA:
				default:
					break;
			}
		}
		data = (uint8*)data + read;
		size -= read;
	};
	return 0;
}

class PSARCFileHandler : public FileHandler {
	const char*		GetExt() override { return "psarc";		}
	const char*		GetDescription() override { return "PSARC Archive";}

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32be>() == PSARCHeader::MagicNumber ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		PSARCReader	r(file);
		if (!r.check())
			return ISO_NULL;

		const TOCEntry	*entry	= r.getManifest();
		malloc_block	manifest(uint64(entry->originalSize));
		r.Load(file, entry, manifest);

		ISO_ptr<Folder>		t(id);
		for (const char *p = manifest, *e = manifest.end(), *s; p < e; p = s + 1) {
			s = str(p, e).find('\n');
			if (!s)
				s = e;
			filename		fn		= str(p, s);
			const TOCEntry	*entry	= r.lookupFile(fn);
			const char		*name;
			ISO_ptr<Folder>	dir		= GetDir(t, fn, &name);

			if (entry) {
				ISO_ptr<ISO_openarray<uint8> >	data(name, entry->originalSize);
				r.Load(file, entry, *data);
				dir->Append(data);
			} else {
				dir->Append(ISO_ptr<bool>(name, false));
			}
		}
		return t;
	}
} psarc;
