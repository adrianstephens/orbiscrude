#include "iso/iso_files.h"
#include "container/archive_help.h"
#include "comms/zlib_stream.h"

//-----------------------------------------------------------------------------
//	Bethesda Softworks Archive
//-----------------------------------------------------------------------------

using namespace iso;

struct BSA3 {
	enum { VERSION = 0x100 };

	struct Header {
		uint32 version;
		uint32 hashOffset;
		uint32 fileCount;
	};

	struct FileRecord {
		uint32 size;
		uint32 offset;
	};

	struct Asset {
		string path;
		uint64 hash;
		uint32 size;
		uint32 offset;

		inline Asset() : hash(0), size(0), offset(0) {}

		malloc_block data(istream_ref in) const {
			in.seek(offset);
			return malloc_block(in, size);
		}
		friend bool	hash_comp(const Asset& first, const Asset& second) {
			return first.hash < second.hash || (first.hash == second.hash && first.path < second.path);
		}
		friend bool	path_comp(const Asset& first, const Asset& second) {
			return first.path < second.path;
		}
	};

	dynamic_array<Asset> assets;

	BSA3(istream_ref in) {
		in.seek(0);
		Header		header;
		in.read(header);

		uint32			num = header.fileCount;
		uint32			hashOffset = header.hashOffset;
		malloc_block	data(in, hashOffset + num * sizeof(uint64));

		FileRecord	*fileRecords = data;
		uint32		*filenameOffsets = (uint32*)(fileRecords + num);
		char		*filenameRecords = (char*)(filenameOffsets + num);
		uint64		*hashRecords = data + hashOffset;

		uint32		startOfData = in.tell();
		for (uint32 i = 0; i < header.fileCount; i++) {
			Asset *fileData = new(assets) Asset;
			fileData->size = fileRecords[i].size;
			fileData->offset = startOfData + fileRecords[i].offset;
			fileData->hash = hashRecords[i];
			fileData->path = filenameRecords + filenameOffsets[i];
		}
	}
	uint64 CalcHash(const char *path);
};

uint64 BSA3::CalcHash(const char *path) {
	const uint8 *p = (const uint8*)path;
	size_t	len		= strlen(path);
	uint32	hash1	= 0;
	uint32	hash2	= 0;

	for (uint32 i = 0, n = uint32(len / 2); i < n; i++)
		hash1 ^= uint32(*p++) << ((i * 8) & 0x1F);

	for (uint32 i = 0, n = uint32(len - len / 2); i < n; i++) {
		uint32	temp = uint32(*p++) << ((i * 8) & 0x1F);
		uint32	shift = temp & 0x1F;
		hash2 ^= temp;
		hash2 = (hash2 << (32 - shift)) | (hash2 >> shift);
	}

	return ((uint64)hash1 << 32) + hash2;
}

struct BSA4 {
	enum {
		MAGIC					= '\0ASB',
		VERSION_TES4			= 0x67,
		VERSION_TES5			= 0x68,			//Also for FO3 and probably FNV too
		FOLDER_RECORD_OFFSET	= 36,			//Folder record offset for TES4-type BSAs is constant
		COMPRESSED				= 0x0004,		//If this flag is present in the archiveFlags header field, then the BSA file data is compressed
		FILE_INVERT_COMPRESSED	= 0x40000000,	//Inverts the file data compression status for the specific file this flag is set for
	};

	struct Header {
		uint32 fileId;
		uint32 version;
		uint32 offset;
		uint32 archiveFlags;
		uint32 folderCount;
		uint32 fileCount;
		uint32 totalFolderNameLength;
		uint32 totalFileNameLength;
		uint32 fileFlags;
	};

	struct FolderRecord {
		uint64 nameHash;  //Hash of folder name.
		uint32 count;     //Number of files in folder.
		uint32 offset;    //Offset to the fileRecords for this folder, including the folder name, from the beginning of the file.
	};

	struct FileRecord {
		uint64 nameHash;  //Hash of the filename.
		uint32 size;      //Size of the data. See TES4Mod wiki page for details.
		uint32 offset;    //Offset to the raw file data, from byte 0.
	};

	struct File {
		uint64 hash;
		uint32 size;
		uint32 offset;
		string name;

		File(const FileRecord *r, const char *_name) : hash(r->nameHash), size(r->size), offset(r->offset), name(_name) {}

		malloc_block data(istream_ref in, bool comp) const {
			in.seek(offset);
			if (comp ^ !!(size & FILE_INVERT_COMPRESSED)) {
				uint32	len = in.get<uint32>();
				return malloc_block(zlib_reader(in).me(), len);
			} else {
				return malloc_block(in, size & ~FILE_INVERT_COMPRESSED);
			}
		}
		friend bool	hash_comp(const File& first, const File& second) {
			return first.hash < second.hash;
		}
		friend bool	path_comp(const File& first, const File& second) {
			return first.name < second.name;
		}
	};

	struct Folder {
		uint32			first;
		uint32			count;
		string			name;

		Folder(BSA4 *bsa, FolderRecord *rec, const uint8 *data) : first(bsa->files.size32()), count(rec->count), name(str((char*)data + 1, data[0] - 1)) {}
		File&		file(BSA4 *bsa, int i)	const { return bsa->files[first + i]; }
	};

	Header		header;
	dynamic_array<Folder>	folders;
	dynamic_array<File>		files;


	BSA4(istream_ref in) {
		in.seek(0);
		in.read(header);

		uint32			fileRecordsSize = header.folderCount + header.totalFolderNameLength + sizeof(FileRecord) * header.fileCount;
		uint32			data_size = header.folderCount * sizeof(FolderRecord) + fileRecordsSize + header.totalFileNameLength;
		malloc_block	data(in, data_size);

		FolderRecord	*folderRecords	= data;
		uint8			*fileData		= (uint8*)(folderRecords + header.folderCount);
		char			*fileNames		= (char*)(fileData + fileRecordsSize);

		const char *p = fileNames;
		const uint32 folderRecordOffsetBaseline = sizeof(Header) + sizeof(FolderRecord) * header.folderCount + header.totalFileNameLength;
		for (FolderRecord *f = folderRecords, *fe = f + header.folderCount; f != fe; ++f) {
			f->offset -= folderRecordOffsetBaseline;
			uint8		*folderData		= fileData + f->offset;
			FileRecord	*fileRecord		= (FileRecord*)(folderData + folderData[0] + 1);

			new(folders) Folder(this, f, folderData);
			for (uint32 i = f->count; i--; ++fileRecord) {
				new(files) File(fileRecord, p);
				p += strlen(p) + 1;
			}
		}
	}

	bool	Comp() const { return !!(header.archiveFlags & COMPRESSED); }

	static uint32	HashString(const char *s);
	static uint32	HashString(const char *s, size_t n);
	static uint64	CalcHash(const char *path, const char *ext);
};

uint32 BSA4::HashString(const char *s) {
	uint32 hash = 0;
	while (*s)
		hash = 0x1003F * hash + (uint8)*s++;
	return hash;
}
uint32 BSA4::HashString(const char *s, size_t n) {
	uint32 hash = 0;
	while (n--)
		hash = 0x1003F * hash + (uint8)*s++;
	return hash;
}
uint64 BSA4::CalcHash(const char *path, const char *ext) {
	uint32 hash1 = 0;
	uint32 hash2 = 0;

	if (path) {
		const size_t len = strlen(path);
		hash1 = ((uint8)path[len - 1]) + (uint32(len) << 16) + ((uint8)path[0] << 24);
		if (len > 2) {
			hash1 += ((uint8)path[len - 2] << 8);
			if (len > 3)
				hash2 = HashString(path + 1, len - 3);
		}
	}
	if (ext) {
		hash2 += HashString(ext);
		if (str(ext) == ".kf")
			hash1 += 0x80;
		else if (str(ext) == ".nif")
			hash1 += 0x8000;
		else if (str(ext) == ".dds")
			hash1 += 0x8080;
		else if (str(ext) == ".wav")
			hash1 += 0x80000000;
	}
	return ((uint64)hash2 << 32) + hash1;
}

struct ISO_BSA3 : BSA3, ISO::VirtualDefaults {
	istream_ptr	in;
	ISO_BSA3(istream_ptr &&in) : BSA3(in), in(move(in)) {}
	uint32			Count() { return assets.size32(); }
	ISO::Browser2	Index(int i) {
		return ISO::MakePtr(assets[i].path, assets[i].data(in));
	}
};
ISO_DEFVIRT(ISO_BSA3);

struct ISO_BSA4 : BSA4, ISO::VirtualDefaults {
	istream_ptr	in;

	struct Folder {
		ISO_BSA4			*bsa;
		const BSA4::Folder	*f;
		Folder(const pair<ISO_BSA4*,const BSA4::Folder*> &p) : bsa(p.a), f(p.b) {}
	};

	ISO_BSA4(istream_ptr &&in) : BSA4(in), in(move(in)) {}
	uint32			Count() { return folders.size32(); }
	ISO::Browser2	Index(int i) {
		return ISO_ptr<Folder>(folders[i].name, make_pair(this, &folders[i]));
	}
};

template<> struct ISO::def<ISO_BSA4::Folder> : public ISO::VirtualT2<ISO_BSA4::Folder> {
public:
	uint32			Count(ISO_BSA4::Folder &a) { return a.f->count; }
	ISO::Browser2	Index(ISO_BSA4::Folder &a, int i) {
		BSA4::File	&file = a.f->file(a.bsa, i);
		return MakePtr(file.name, file.data(a.bsa->in, a.bsa->Comp()));
	}
};

ISO_DEFVIRT(ISO_BSA4);

class BSAFileHandler : public FileHandler {
	const char*		GetExt() override { return "bsa"; }
	const char*		GetDescription() override { return "Bethesda Softworks Archive"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		uint32	v1 = file.get<uint32>();
		if (v1 == BSA3::VERSION)
			return CHECK_POSSIBLE;

		if (v1 == BSA4::MAGIC) {
			uint32	v2 = file.get<uint32>();
			if (v2 == BSA4::VERSION_TES4 || v2 == BSA4::VERSION_TES5)
				return CHECK_PROBABLE;
		}

		return CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		uint32	v1 = file.get<uint32>();
		if (v1 == BSA3::VERSION)
			return ISO_ptr<ISO_BSA3>(id, file.clone());

		if (v1 == BSA4::MAGIC)
			return ISO_ptr<ISO_BSA4>(id, file.clone());

		return ISO_NULL;
	}
} bsa;