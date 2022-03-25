#include "iso/iso_files.h"

using namespace iso;

enum {SECTOR_SIZE = 512};
typedef uint64 SectorType;	// in sectors

struct SparseExtentHeader {
	enum {MAGIC = 'VMDK'};
	enum {COMPRESSION_NONE = 0, COMPRESSION_DEFLATE = 1};

	enum {
		LINE_DETECTION		= 1 << 0,			// valid new line detection test.
		USE_REDUNDANT		= 1 << 1,			// redundant grain table will be used.
		COMPRESSED			= 1 << 16,			// the grains are compressed
		MARKERS				= 1 << 17,			// there are markers in the virtual disk to identify every block of metadata or data and the markers for the virtual machine data contain a LBA
	};

	uint32				magicNumber;
	uint32				version;				// 1
	uint32				flags;
	packed<SectorType>	capacity;				// capacity of this extent in sectors — should be a multiple of the grain size.
	packed<SectorType>	grainSize;				// size of a grain in sectors. Power of 2 greater than 8 (4KB).
	packed<SectorType>	descriptorOffset;		// offset of the embedded descriptor in the extent
	packed<SectorType>	descriptorSize;			// valid only if descriptorOffset is non-zero
	uint32				numGTEsPerGT;			// number of entries in a grain table (512)
	packed<SectorType>	rgdOffset;				// points to the redundant level 0 of metadata
	packed<SectorType>	gdOffset;				// points to the level 0 of metadata
	packed<SectorType>	overHead;				// is the number of sectors occupied by the metadata.
	bool8				uncleanShutdown;		// set to FALSE when VMware software closes an extent
	char				singleEndLineChar;		// '\n';
	char				nonEndLineChar;			// ' ';
	char				doubleEndLineChar1;		// '\r';
	char				doubleEndLineChar2;		// '\n';
	packed<uint16>		compressAlgorithm;
	uint8				pad[433];
};

class VMDK : public ISO::VirtualDefaults {
	FileInput				file;
	SectorType				capacity;
	SectorType				grainSize;
	dynamic_array<uint32>	grain_offsets;

	dynamic_array<ISO::WeakRef<malloc_block> >	grains;
public:
	VMDK(const filename &fn);
	uint32			Count() {
		return uint32(grains.size());
	}
	ISO_ptr<void>	Index(int i) {
		ISO_ptr<malloc_block>	p;
		if (!(p = grains[i])) {
			grains[i] = p.Create(tag(), grainSize * SECTOR_SIZE);
			if (uint32 offset = grain_offsets[i]) {
				uint32	size	= uint32(min(capacity - offset, grainSize));
				file.seek(offset * SECTOR_SIZE);
				file.readbuff(*p, size * SECTOR_SIZE);
			} else {
				memset(*p, 0, grainSize * SECTOR_SIZE);
			}
		}
		return p;
	}
};

VMDK::VMDK(const filename &fn) : file(fn) {
	SparseExtentHeader	h;
	if (!file.read(h) || h.magicNumber != h.MAGIC)
		return;

	if (SectorType offset = h.descriptorOffset) {
		SectorType size = h.descriptorSize * SECTOR_SIZE;
		malloc_block	desc(size);
		file.seek(offset * SECTOR_SIZE);
		file.readbuff(desc, size);
		string_scan	ss(desc);
	}

	capacity		= h.capacity;
	grainSize		= h.grainSize;

	uint32		numGTEsPerGT	= h.numGTEsPerGT;
	SectorType	gdOffset		= h.gdOffset;
	uint64		num_grains		= capacity / grainSize;
	uint64		num_tables		= num_grains / numGTEsPerGT;

	dynamic_array<uint32>	gd(num_tables);

	file.seek(gdOffset * SECTOR_SIZE);
	file.readbuff(gd, num_tables * sizeof(uint32));

	grains.resize(num_grains);
	grain_offsets.resize(num_grains);
	file.seek(gd[0] * SECTOR_SIZE);
	file.readbuff(grain_offsets, num_grains * sizeof(uint32));
}

ISO_DEFUSERVIRTX(VMDK, "BigBin");

class VMDKFileHandler : public FileHandler {
	const char*		GetExt() override { return "vmdk";		}
	const char*		GetDescription() override { return "VMware virtual disk";}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<VMDK>	p(id, fn);
		if (p->Count())
			return p;
		return ISO_NULL;
	}

} vmdk;
