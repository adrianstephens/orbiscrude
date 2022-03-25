#include "iso/iso_files.h"
#include "codec/aes.h"
#include "codec/lzx.h"
#include "codec/codec_stream.h"
#include "hashes/SHA.h"
#include "pe.h"

//namespace iso {
//size_t to_string(char *s, const SHA1::CODE &sha) {
//	return _format(s, "%08x %08x %08x %08x %08x", sha[0], sha[1], sha[2], sha[3], sha[4]);
//}
//}

using namespace iso;
//XEX1: A26C10F71FD935E98B99922CE9321572
//XEX2: 20B185A59D28FDC340583FBB0896BF91

enum XEX_id {
	XEXID_ResourceInfo					=    0x2,	// FF Resource Info
	XEXID_CompressionInfo				=    0x3,	// FF Compression Info
	XEXID_BaseReference					=    0x4,	// 05 Base Reference
	XEXID_DeltaPatchDescriptor			=    0x5,	// FF Delta Patch Descriptor
	XEXID_BoundingPath					=   0x80,	// FF Bounding Path
	XEXID_DeviceID						=   0x81,	// 05 Device ID
	XEXID_OriginalBaseAddress			=  0x100,	// 01 Original Base Address
	XEXID_EntryPoint					=  0x101,	// 00 Entry point
	XEXID_ImageBaseAddress				=  0x102,	// 01 Image Base Address
	XEXID_ImportLibraries				=  0x103,	// FF Import Libraries
	XEXID_ChecksumTimestamp				=  0x180,	// 02 Checksum Timestamp
	XEXID_EnabledForCallcap				=  0x181,	// 02 Enabled For Callcap
	XEXID_EnabledForFastcap				=  0x182,	// 00 Enabled For Fastcap
	XEXID_OriginalPEName				=  0x183,	// FF Original PE Name
	XEXID_StaticLibraries				=  0x200,	// FF Static Libraries
	XEXID_TLSInfo						=  0x201,	// 04 TLS Info
	XEXID_DefaultStackSize				=  0x202,	// 00 Default Stack Size
	XEXID_DefaultFilesystemCacheSize	=  0x203,	// 01 Default Filesystem Cache Size
	XEXID_DefaultHeapSize				=  0x204,	// 01 Default Heap Size
	XEXID_PageHeapSizeandFlags			=  0x280,	// 02 Page Heap Size and Flags
	XEXID_SystemFlags					=  0x300,	// 00 System Flags
	XEXID_ExecutionID					=  0x400,	// 06 Execution ID
	XEXID_ServiceIDList					=  0x401,	// FF Service ID List
	XEXID_TitleWorkspaceSize			=  0x402,	// 01 Title Workspace Size
	XEXID_GameRatings					=  0x403,	// 10 Game Ratings
	XEXID_LANKey						=  0x404,	// 04 LAN Key
	XEXID_Xbox360Logo					=  0x405,	// FF Xbox 360 Logo
	XEXID_MultidiscMediaIDs				=  0x406,	// FF Multidisc Media IDs
	XEXID_AlternateTitleIDs				=  0x407,	// FF Alternate Title IDs
	XEXID_AdditionalTitleMemory			=  0x408,	// 01 Additional Title Memory
	XEXID_ExportsbyName					= 0xE104,	// 02 Exports by Name
};

struct {XEX_id id; const char *name; } block_names[] = {
	{XEXID_ResourceInfo					, "Resource Info"					},
	{XEXID_CompressionInfo				, "Compression Info"				},
	{XEXID_BaseReference				, "Base Reference"					},
	{XEXID_DeltaPatchDescriptor			, "Delta Patch Descriptor"			},
	{XEXID_BoundingPath					, "Bounding Path"					},
	{XEXID_DeviceID						, "Device ID"						},
	{XEXID_OriginalBaseAddress			, "Original Base Address"			},
	{XEXID_EntryPoint					, "Entry point"						},
	{XEXID_ImageBaseAddress				, "Image Base Address"				},
	{XEXID_ImportLibraries				, "Import Libraries"				},
	{XEXID_ChecksumTimestamp			, "Checksum Timestamp"				},
	{XEXID_EnabledForCallcap			, "Enabled For Callcap"				},
	{XEXID_EnabledForFastcap			, "Enabled For Fastcap"				},
	{XEXID_OriginalPEName				, "Original PE Name"				},
	{XEXID_StaticLibraries				, "Static Libraries"				},
	{XEXID_TLSInfo						, "TLS Info"						},
	{XEXID_DefaultStackSize				, "Default Stack Size"				},
	{XEXID_DefaultFilesystemCacheSize	, "Default Filesystem Cache Size"	},
	{XEXID_DefaultHeapSize				, "Default Heap Size"				},
	{XEXID_PageHeapSizeandFlags			, "Page Heap Size and Flags"		},
	{XEXID_SystemFlags					, "System Flags"					},
	{XEXID_ExecutionID					, "Execution ID"					},
	{XEXID_ServiceIDList				, "Service ID List"					},
	{XEXID_TitleWorkspaceSize			, "Title Workspace Size"			},
	{XEXID_GameRatings					, "Game Ratings"					},
	{XEXID_LANKey						, "LAN Key"							},
	{XEXID_Xbox360Logo					, "Xbox 360 Logo"					},
	{XEXID_MultidiscMediaIDs			, "Multidisc Media IDs"				},
	{XEXID_AlternateTitleIDs			, "Alternate Title IDs"				},
	{XEXID_AdditionalTitleMemory		, "Additional Title Memory"			},
	{XEXID_ExportsbyName				, "Exports by Name"					},
};

struct XEXHeader : bigendian_types {
	struct block {
		uint32	id;		//if id & 0xFF == 0x01 then actual data, otherwise offset.
		uint32	data;	//if id & 0xFF == 0xFF then size is in data, otherwise it is the size in number of DWORDS
	};

	enum {
		MAGIC				= 'XEX2',
		FLAG_TitleModule	= 1 << 0,
		FLAG_ExportsToTitle	= 1 << 1,
		FLAG_SystemDebugger	= 1 << 2,
		FLAG_DLLModule		= 1 << 3,
		FLAG_ModulePatch	= 1 << 4,
		FLAG_PatchFull		= 1 << 5,
		FLAG_PatchDelta		= 1 << 6,
		FLAG_UserMode		= 1 << 7,
	};

	uint32	magic;
	uint32	flags;
	uint32	pe_offset;
	uint32	reserved;
	uint32	security_offset;
	uint32	block_count;
	block	blocks;//[];
};

typedef array<uint32be, 5>  sha1_state_be;


struct XEXSecurity : bigendian_types {	//my guesswork
	uint32			image_size;
	uint8			gobbledegook[256];
	uint32			permission_offset;
	uint32			zero;
	uint32			start_addr;
	sha1_state_be	hash1;
	uint32			two;
	sha1_state_be	hash2;
	uint32			zero2[4];
	uint32			unknown[4];
	uint32			zero3;
	sha1_state_be	hash3;
};

struct XEXpermission : bigendian_types {	//my guesswork
	struct page {
		enum {
			RESOURCE	= 0x13,
			CODE		= 0x11,
			DATA		= 0x12,
		};
		uint32			type;
		sha1_state_be	hash;
	};
	uint32	region;
	uint32	media_types;
	uint32	num_pages;
};

struct XEXCompression : bigendian_types {
	struct block {
		uint32					size;
		array<uint8, 20>	hash;
	};
	uint32	reserved;
	uint32	window;
	block	first;
};

struct XEXLibrary : bigendian_types {
	char	name[8];
	uint16	version1;
	uint16	version2;
	uint16	version3;
	uint16	version4;
};

struct XEXChecksumFiletime : bigendian_types {
	uint32	checksum;
	uint32	timestamp;
	uint32	raw_address;
	uint32	raw_size;
};

struct XEXtls : bigendian_types {
	uint32	num_slots;
	uint32	data_size;
};

struct XEXexecid : bigendian_types {
	uint32	media_id;
	struct {
		iso::uint32 more:8, build:16, min:4, maj:4;
	} version, base_version;
	uint32	title_id;
	uint8	platform, type, disc, num_discs;
	uint32	save_id;
};

struct XEXresources : bigendian_types {
	char	name[8];
	uint32	start;
	uint32	length;
};

struct SecurityOffsets {
	enum {
		module_flags,
		load_address,
		image_size,
		game_region,
		image_flags,
		allowed_media_types,
		count
	};
	uint32		offset;
	const char	*desc;
} security_offsets[] = {
    { 0x00000000, "module flags",       },
    { 0x00000110, "load address",       },
    { 0x00000004, "image size",         },
    { 0x00000178, "game region",        },
    { 0x0000010C, "image flags",        },
    { 0x0000017C, "allowed media types" }
};

class XEXFileHandler : public FileHandler {
	const char*		GetExt() override { return "xex"; }
	const char*		GetDescription() override { return "Xbox 360 executable";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} xex;

ISO_ptr<void> XEXFileHandler::Read(tag id, istream_ref file) {
	XEXHeader			header	= file.get();
	if (header.magic != XEXHeader::MAGIC)
		return ISO_NULL;

	int					nb		= header.block_count;
	XEXHeader::block	*blocks	= new XEXHeader::block[nb];
	readn(file, blocks, nb);

	ISO_ptr<anything>	result(id);

	if (1) {
		file.seek(header.security_offset);
		uint32	size = file.get<uint32be>() - 4;
		ISO_ptr<ISO_openarray<uint8> > p("security", size);
		file.readbuff(*p, size);
		result->Append(p);
	}

	for (int i = 0; i < nb; i++) {
		uint32	id		= blocks[i].id;
		uint32	data	= blocks[i].data;
		bool	direct	= (id & 0xff) < 2;
		uint32	size	= direct ? 4 : (id & 0xff) * 4;

		if (!direct) {
			file.seek(data);
			if (size == 0x3fc)
				size = file.get<uint32be>() - 4;
		}

		id >>= 8;
		tag	isoid;
		for (int j = 0; j < num_elements(block_names); j++) {
			if (id == block_names[j].id) {
				isoid = block_names[j].name;
				break;
			}
		}

		if (direct) {
			ISO_ptr<xint32> p(isoid, data);
			result->Append(p);
		} else {
			ISO_ptr<ISO_openarray<uint8> > p(isoid, size);
			file.readbuff(*p, size);
			result->Append(p);
		}
	}

	#if 1
	uint32			security_info[SecurityOffsets::count];
	AES::block		aes_key;
	AES				aes_ctx;
	XEXCompression	comp;

	for (int i = 0; i < SecurityOffsets::count; i++) {
		file.seek(header.security_offset + security_offsets[i].offset);
		security_info[i] = file.get<uint32be>();
	}

	clear(aes_key);
	aes_ctx.setkey_dec((uint8*)&aes_key, 128);

	file.seek(header.security_offset + 0x150);
	file.read(aes_key);
	aes_ctx.decrypt(aes_key);
	aes_ctx.setkey_dec((uint8*)&aes_key, 128);

	for (int i = 0; i < nb; i++) {
		if ((blocks[i].id >> 8) == XEXID_CompressionInfo) {
			file.seek(blocks[i].data);
			uint32	size = file.get<uint32be>() - 4;
			comp = file.get();
			break;
		}
	}

	file.seek(header.pe_offset);

	// decrypt pe

	uint32			image_size	= security_info[SecurityOffsets::image_size];
	malloc_block	decrypt(image_size);
	AES::block	LCT;
	clear(LCT);

	auto	block_hash	= comp.first.hash;
	uint8	*dest		= decrypt;

	for (uint32 block_size = comp.first.size; block_size; ) {
		malloc_block	buffer(block_size);
		file.readbuff(buffer, block_size);

		XEXCompression::block	*block		= buffer;

		for (int i = 0; i < block_size; i += 16) {
			AES::block	*a	= (AES::block*)((uint8*)block + i);
			AES::block	t	= *a;
			aes_ctx.decrypt(*a);
			*a ^= LCT;
			LCT = t;
		}

		SHA1::CODE	block_hash_calculated = SHA1(block, block_size);
		if (block_hash != block_hash_calculated) {
			printf("\ncompressed data is corrupt!!!\n");
			break;
		}

		block_size = block->size;
		block_hash = block->hash;

		uint16	slen;
		for (uint8 *lzx_data = (uint8*)(block + 1); slen = (lzx_data[0] << 8) | lzx_data[1]; ) {
			memcpy(dest, lzx_data + 2, slen);
			lzx_data	+= slen + 2;
			dest		+= slen;
		}
	}


	// decompress pe

	auto		lzx = make_codec_reader<0>(LZX_decoder(15, image_size), memory_reader(decrypt.slice_to(dest)));
#if 0

	pe::DOS_HEADER		dos;
	pe::FILE_HEADER		pefh;
	pe::OPTIONAL_HEADER	*opt	= 0;
	pe::SECTION_HEADER	*sects	= 0;
	lzx.read(dos);
	bool	pe	= dos.e_magic == pe::DOS_HEADER::MAGIC;
	if (pe) {
		lzx.seek(dos.e_lfanew);
		pe = lzx.get<uint32be>() == 'PE' << 16;
		if (pe) {
			lzx.read(pefh);

			if (pefh.SizeOfOptionalHeader)
				lzx.readbuff((opt = (pe::OPTIONAL_HEADER*)malloc(pefh.SizeOfOptionalHeader)), pefh.SizeOfOptionalHeader);

			sects	= new pe::SECTION_HEADER[pefh.NumberOfSections];
			read(lzx, sects, pefh.NumberOfSections);
		}
	}
	if (!pe)
		ISO_TRACE("bad PE\n");
//	result->Append(ReadEXE("pe", lzx));
#else
	ISO_ptr<ISO_openarray<uint8> > p("pe");
	lzx.readbuff(p->Create(image_size, false), image_size);
	result->Append(p);

	for (uint8 *m = *p, *e = m + image_size; m < e; m += 0x10000) {
		ISO_TRACEF("Hash @ 0x%08x: ", m - *p) << (SHA1::CODE)SHA1(m, 0x10000) << '\n';
	}

#endif

#else
	{

		uint32	size = file.length() - header.pe_offset;
		ISO_ptr<ISO_openarray<uint8> > p("pe", size);
		file.seek(header.pe_offset);
		file.readbuff(*p, size);
	}
#endif
	delete[] blocks;

	return result;
}

