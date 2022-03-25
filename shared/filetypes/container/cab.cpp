#include "iso/iso_files.h"
#include "codec/lzx.h"
#include "codec/deflate.h"
#include "codec/codec_stream.h"
#include "archive_help.h"

using namespace iso;

//-----------------------------------------------------------------------------
// MSZLIB
//-----------------------------------------------------------------------------

class MSZLIB_decoder : public deflate_decoder {
public:
	uint8*			process(uint8* dst, uint8 *dst_end, reader_intf file, TRANSCODE_FLAGS flags) {
		if (mode < BLOCK || !last)
			dst		= deflate_decoder::process(dst, dst_end, file, flags);

		while (mode == BLOCK && last && dst < dst_end) {
			auto	vlc = make_vlc_in(copy(file), vlc0);
			vlc.align(8);

			for (int state = 0; state != 2;) {
				int	c = vlc.get(8);
				if (c == 'C')
					state = 1;
				else if (state == 1 && c == 'K')
					state = 2;
				else
					state = 0;
			};

			vlc0	= vlc.get_state();
			last	= false;
			dst		= deflate_decoder::process(dst, dst_end, file, flags);
		}

		return dst;
	}

	MSZLIB_decoder() : deflate_decoder(15) {
		mode	= BLOCK;
		last	= true;
	}
};

//-----------------------------------------------------------------------------
// CAB
//-----------------------------------------------------------------------------

#define	CAB_SIGNATURE			0x4643534D	//	"MSCF"
#define ISC_SIGNATURE			0x28635349	// "ISc("
#define	CAB_VERSION				0x0103
#define	CAB_BLOCKSIZE			32768

#define	CAB_COMP_MASK			0x00FF
#define	CAB_COMP_NONE			0x0000
#define	CAB_COMP_MSZIP			0x0001
#define	CAB_COMP_QUANTUM		0x0002
#define	CAB_COMP_LZX			0x0003

#define	CAB_FLAG_HASPREV		0x0001
#define	CAB_FLAG_HASNEXT		0x0002
#define	CAB_FLAG_RESERVE		0x0004

#define	CAB_ATTRIB_READONLY		0x0001
#define	CAB_ATTRIB_HIDDEN		0x0002
#define	CAB_ATTRIB_SYSTEM		0x0004
#define	CAB_ATTRIB_VOLUME		0x0008
#define	CAB_ATTRIB_DIRECTORY	0x0010
#define	CAB_ATTRIB_ARCHIVE		0x0020
#define	CAB_ATTRIB_EXECUTE		0x0040
#define	CAB_ATTRIB_UTF_NAME		0x0080

#define	CAB_FILE_MAX_FOLDER		0xFFFC
#define	CAB_FILE_CONTINUED		0xFFFD
#define	CAB_FILE_SPLIT			0xFFFE
#define	CAB_FILE_PREV_NEXT		0xFFFF

#define	MSZIP_MAGIC				0x4B43

class InstallShieldFileHandler {
	#define	HEADER_SUFFIX			"hdr"
	#define	CABINET_SUFFIX			"cab"
	#define OFFSET_COUNT			0x47
	#define COMMON_HEADER_SIZE      20
	#define VOLUME_HEADER_SIZE_V5   40
	#define VOLUME_HEADER_SIZE_V6   64
	#define MAX_FILE_GROUP_COUNT    71
	#define MAX_COMPONENT_COUNT     71

	struct CommonHeader {
		uint32	signature;
		uint32	version;
		uint32	volume_info;
		uint32	cab_descriptor_offset;
		uint32	cab_descriptor_size;
	};

	struct VolumeHeader {
		uint32	data_offset;
		uint32	data_offset_high;
		uint32	first_file_index;
		uint32	last_file_index;
		uint32	first_file_offset;
		uint32	first_file_offset_high;
		uint32	first_file_size_expanded;
		uint32	first_file_size_expanded_high;
		uint32	first_file_size_compressed;
		uint32	first_file_size_compressed_high;
		uint32	last_file_offset;
		uint32	last_file_offset_high;
		uint32	last_file_size_expanded;
		uint32	last_file_size_expanded_high;
		uint32	last_file_size_compressed;
		uint32	last_file_size_compressed_high;
	};

	struct CabDescriptor {
		uint32	file_table_offset;
		uint32	file_table_size;
		uint32	file_table_size2;
		uint32	directory_count;
		uint32	file_count;
		uint32	file_table_offset2;
		uint32	file_group_offsets[MAX_FILE_GROUP_COUNT];
		uint32	component_offsets[MAX_COMPONENT_COUNT];
	};

	enum File {
		FILE_SPLIT			= 1,
		FILE_OBFUSCATED		= 2,
		FILE_COMPRESSED		= 4,
		FILE_INVALID		= 8,
	};
	enum Link {
		LINK_NONE	= 0,
		LINK_PREV	= 1,
		LINK_NEXT	= 2,
		LINK_BOTH	= 3,
	};

	struct FileDescriptor {
		uint32	name_offset;
		uint32	directory_index;
		uint16	flags;
		uint32	expanded_size;
		uint32	compressed_size;
		uint32	data_offset;
		uint8	md5[16];
		uint16	volume;
		uint32	link_previous;
		uint32	link_next;
		uint8	link_flags;
	};

	struct OffsetList {
		uint32	name_offset;
		uint32	descriptor_offset;
		uint32	next_offset;
	};
#if 0
	struct Header {
		Header	*next;
		int		index;
		uint8	*data;
		size_t	size;
		int		major_version;

		// shortcuts
		CommonHeader		common;
		CabDescriptor		cab;
		uint32				*file_table;
		FileDescriptor		**file_descriptors;
		int					component_count;
		UnshieldComponent	**components;
		int					file_group_count;
		UnshieldFileGroup	**file_groups;
	};
	struct Unshield {
		Header*	header_list;
		char*	filename_pattern;
	};
#endif

public:
	ISO_ptr<void>	Read(tag id, istream_ref file) { return ISO_NULL; }

} installshield;

class CABFileHandler : FileHandler {

	struct CFHEADER {
		uint32	signature;			// file signature
		uint32	reserved1;			// reserved
		uint32	cbCabinet;			// size of this cabinet file in bytes
		uint32	reserved2;			// reserved
		uint32	coffFiles;			// offset of the first CFFILE entry
		uint32	reserved3;			// reserved
		uint8	versionMinor;		// cabinet file format version, minor
		uint8	versionMajor;		// cabinet file format version, major
		uint16	cFolders;			// number of CFFOLDER entries in this cabinet
		uint16	cFiles;				// number of CFFILE entries in this cabinet
		uint16	flags;				// cabinet file option indicators
		uint16	setID;				// must be the same for all cabinets in a set
		uint16	iCabinet;			// number of this cabinet file in a set
//		uint16	cbCFHeader;			// (optional) size of per-cabinet reserved area
//		uint8	cbCFFolder;			// (optional) size of per-folder reserved area
//		uint8	cbCFData;			// (optional) size of per-datablock reserved area
//		uint8	abReserve[];		// (optional) per-cabinet reserved area
//		uint8	szCabinetPrev[];	// (optional) name of previous cabinet file
//		uint8	szDiskPrev[];		// (optional) name of previous disk
//		uint8	szCabinetNext[];	// (optional) name of next cabinet file
//		uint8	szDiskNext[];		// (optional) name of next disk
	};

	struct CFFOLDER {
		uint32	coffCabStart;		// offset of the first CFDATA block in this folder
		uint16	cCFData;			// number of CFDATA blocks in this folder
		uint16	typeCompress;		// compression type indicator
//		uint8	abReserve[];		// (optional) per-folder reserved area
	};

	struct	CFFILE	{
		uint32	cbFile;				// uncompressed size of this file in bytes
		uint32	uoffFolderStart;	// uncompressed offset of this file in the folder
		uint16	iFolder;			// index into the CFFOLDER area
		uint16	date;				// date stamp for this file
		uint16	time;				// time stamp for this file
		uint16	attribs;			// attribute flags for this file
//		uint8	szName[];			// name of this file
	};

	struct	CFFolder : CFFOLDER {
		malloc_block	data;
		CFFolder()	{}
	};

	struct	CFFile : CFFILE {
		filename name;
	};

	struct	CFDATA	{
		uint32	csum;				// checksum of this CFDATA entry
		uint16	cbData;				// number of compressed bytes in this block
		uint16	cbUncomp;			// number of uncompressed bytes in this block
//		uint8	abReserve[];		// (optional) per-datablock reserved area
//		uint8	ab[];				// compressed data bytes
	};

	class CABistream : public reader_mixin<CABistream> {
		istream_ref		file;
		int			blocks;
		int			cbCFData;
		size_t		remain;
		uint8		block[65536], *p;
		streamptr	pos;
	public:
		CABistream(istream_ref file, int blocks, int cbCFData) : file(file), blocks(blocks), cbCFData(cbCFData), remain(0), pos(0) {}

		size_t		readbuff(void *buffer, size_t size)	{
			size_t		read = 0;
			while (size) {
				if (remain == 0) {
					if (blocks == 0)
						break;
					blocks--;
					CFDATA	data = file.get();
					remain = data.cbData;
					file.seek_cur(cbCFData);
					file.readbuff(block, remain);
					p	= block;
				}
				size_t	t = min(uint32(remain), uint32(size));
				memcpy(buffer, p, t);
				read	+= t;
				size	-= t;
				remain	-= t;
				p		+= t;
				buffer	= (uint8*)buffer + t;
			}
			pos += read;
			return int(read);
		}
		streamptr	tell(void)		{ return pos;	}
		streamptr	length(void)	{ return 0;		}
		void		seek_cur(streamptr offset) {
			ISO_ASSERT(offset < 0 ? p + offset >= block : offset < remain);
			p += offset;
			pos += offset;
		}
	};

	const char*		GetExt() override { return "cab";		}
	const char*		GetDescription() override { return "Cabinet";	}
//	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
//	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} cab;

uint32 CSUMCompute(void *pv, int cb, uint32 seed) {
	int		cl		= cb / 4;
	uint8	*pb		= (uint8*)pv;

	while (cl-- > 0) {
		seed ^= load_packed<uint32le>(pb);
		pb	+= 4;
	}

	uint32	ul = 0;
	switch (cb % 4) {
		case 3:	ul |= pb[2] << 16;
		case 2:	ul |= pb[1] <<  8;
		case 1:	ul |= pb[0];
		default:
			break;
	}
	seed ^= ul;
	return seed;
}

ISO_ptr<void> CABFileHandler::Read(tag id, istream_ref file) {
	CFHEADER	cfh = file.get();
	if (cfh.signature == ISC_SIGNATURE) {
		file.seek(0);
		return installshield.Read(id, file);
	}

	if (cfh.signature != CAB_SIGNATURE)
		return ISO_NULL;

	uint16	cbCFHeader	= 0;	// (optional) size of per-cabinet reserved area
	uint8	cbCFFolder	= 0;	// (optional) size of per-folder reserved area
	uint8	cbCFData	= 0;	// (optional) size of per-datablock reserved area

	if (cfh.flags & CAB_FLAG_RESERVE) {
		file.read(cbCFHeader);
		file.read(cbCFFolder);
		file.read(cbCFData);
		file.seek_cur(cbCFHeader);
	}

	for (int skip = (cfh.flags & CAB_FLAG_HASPREV ? 2 : 0) + (cfh.flags & CAB_FLAG_HASNEXT ? 2 : 0); skip--;)
		while (file.getc());

	dynamic_array<CFFolder>	cff(cfh.cFolders);
	for (int i = 0; i < cfh.cFolders; i++) {
		file.read<CFFOLDER>(cff[i]);
		if (cbCFFolder)
			file.seek_cur(cbCFFolder);
	}

	dynamic_array<CFFile>	files(cfh.cFiles);
	file.seek(cfh.coffFiles);
	for (int i = 0; i < cfh.cFiles; i++) {
		file.read<CFFILE>(files[i]);
		char	buffer[1024], *p = buffer;
		while (*p++ = file.getc());
		files[i].name = buffer;
	}

	for (int i = 0; i < cfh.cFolders; i++) {
		CFFolder	&folder = cff[i];
		uint32		total	= 0;

		file.seek(folder.coffCabStart);
		for (int j = 0; j < folder.cCFData; j++) {
			CFDATA		data	= file.get();
			total	+= data.cbUncomp;
			file.seek_cur(data.cbData + cbCFData);
		}

		folder.data.create(total);
		uint8	*p	= (uint8*)folder.data;

		file.seek(folder.coffCabStart);
		CABistream	cab(file, folder.cCFData, cbCFData);
		switch (folder.typeCompress & CAB_COMP_MASK) {
			case CAB_COMP_NONE:		cab.readbuff(p, total); break;
			case CAB_COMP_MSZIP:	make_codec_reader<0, reader_intf>(MSZLIB_decoder(), cab).readbuff(p, total); break;
			case CAB_COMP_LZX:		make_codec_reader<0, reader_intf>(LZX_decoder(folder.typeCompress >> 8, total), cab).readbuff(p, total); break;
		}
	}

	ISO_ptr<anything> t(id);
	bool	raw = WantRaw();

	for (int i = 0; i < cfh.cFiles; i++) {
		auto	&f		= files[i];
		auto	&folder = cff[f.iFolder];
		ISO_ptr<void>	p = ReadData1(filename(f.name.name()).set_ext(f.name.ext()), memory_reader(memory_block((uint8*)folder.data + f.uoffFolderStart, f.cbFile)).me(), f.cbFile, raw);
		GetDir(t, f.name.dir())->Append(p);
	}

	return t;
}

//-----------------------------------------------------------------------------
//	CHM
//-----------------------------------------------------------------------------

struct ENCINT {
	uint64	v;
	template<class R> bool read(R &file) {
		v = 0;
		int	c;
		while ((c = file.getc()) & 0x80)
			v = (v << 7) | (c & 0x7f);
		v = (v << 7) | c;
		return true;
	}
	operator uint64() const { return v;			}
};

struct ITSF {
	uint32be	id;
	uint32		ver;	//3
	uint32		header_size;
	uint32		_;		//?
	uint32		timestamp;
	uint32		language_id;
	GUID		guid1;
	GUID		guid2;

	struct section {
		uint64	offset, length;
	} sections[2];
};

struct SECTION0 {
	uint64	_;	//0x1fe
	uint64	size;
	uint64	_2;	//0

};

struct SECTION1 {
	uint32be	id;	//ITSP
	uint32		version;
	uint32		length;
	uint32		_;	//0xa
	uint32		chunk_size;		//0x1000
	uint32		density;	//2
	uint32		depth;
	uint32		root_chunk;
	uint32		first_pmgl;
	uint32		last_pmgl;
	uint32		_2;	//-1
	uint32		dir_chunks;
	uint32		language_id;
	GUID		guid;
	uint32		length2;//0x54
	uint32		_3[3];
};

struct DIR_CHUNK {
	uint32be	id;	//PMGL
	uint32		free;
	uint32		_;//0
	uint32		prev_chunk;
	uint32		next_chunk;

	struct ENTRY {
		string	name;
		ENCINT	section, offset, length;
		template<class R> bool read(R &file) {
			ENCINT	len	= file.get();
			file.readbuff(name.alloc(len + 1).begin(), len);
			name.begin()[len] = 0;
			section = file.get();
			offset	= file.get();
			length	= file.get();
			return true;
		}
	};
};

struct INDEX_CHUNK {
	uint32be	id;	//PMGI
	uint32		free;

	struct ENTRY {
		string	name;
		ENCINT	chunk;
		template<class R> bool read(R &file) {
			ENCINT	length	= file.get();
			file.readbuff(name.alloc(length + 1).begin(), length);
			name.begin()[length] = 0;
			chunk	= file.get();
			return true;
		}
	};
};

struct LZX_CONTROL {
	uint32		header_size;	// Number of DWORDs following 'LZXC', must be 6 if version is 2
	uint32be	id;				//'LZXC'  Compression type identifier
	uint32		version;		// Version (Must be <=2)
	uint32		reset_interval;
	uint32		window_size;
	uint32		cache_size;
	uint32		_;				// 0 (unknown)

	uint64		Adjust(uint32 v){ return uint64(v) << (version == 2 ? 15 : 0);	}
	uint64		ResetInterval()	{ return Adjust(reset_interval); }
	uint64		WindowSize()	{ return Adjust(window_size); }
	uint64		CacheSize()		{ return Adjust(cache_size); }
};

struct LZX_RESET {
	uint32	ver;				// 2     unknown (possibly a version number)
	uint32	num_entires;
	uint32	entry_size;			// 8     Size of table entry (bytes)
	uint32	header_size;		// $28   Length of table header (area before table entries)
	uint64	uncompressed_size;
	uint64	compressed_size;
	uint64	block_size;			// 0x8000

	uint64	start[];
};

class CHMFileHandler : public FileHandler {
	const char*		GetExt() override { return "chm";		}
	const char*		GetDescription() override { return "Compiled HTML Help";}

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32be>() == 'ITSF' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ITSF	itsf = file.get();
		if (itsf.id != 'ITSF')
			return ISO_NULL;

		file.seek(itsf.sections[1].offset);
		SECTION1	sect1	= file.get();
		streamptr	chunks	= file.tell();
		streamptr	content	= chunks + sect1.chunk_size * sect1.dir_chunks;
		malloc_block	chunk(sect1.chunk_size);

		ISO_ptr<anything> p0(0);

		for (int i = sect1.first_pmgl; i <= sect1.last_pmgl; ++i) {
			file.seek(chunks + i * sect1.chunk_size);
			file.readbuff(chunk, sect1.chunk_size);
			DIR_CHUNK	*dir	= (DIR_CHUNK*)chunk;
			uint8		*end	= (uint8*)chunk + sect1.chunk_size - dir->free;
			byte_reader	r(dir + 1);
			while (r.p < end) {
				DIR_CHUNK::ENTRY	e	= r.get();
				if (e.length && e.name.begins("::DataSpace/")) {
					const char *name;
					ISO_ptr<anything> p2 = GetDir(p0, e.name, &name);
					file.seek(content + e.offset);
					p2->Append(ReadRaw(name, file, uint32(e.length)));
				}
			}
		}
//		return p0;

		ISO::Browser	dataspace	= ISO::Browser(p0)["::DataSpace"];
		ISO::Browser	storage		= dataspace["Storage"];

		struct SECTION {
			string	name;
			istream_ref	file;
			SECTION(string &&name, istream_ref file) : name(move(name)), file(move(file)) {}
		};
		dynamic_array<SECTION>	sections;

		if (void *namelist = dataspace["NameList"][0]) {
			byte_reader	r(namelist);
			uint32	len	= r.get<uint16>();
			uint32	n	= r.get<uint16>();
//			sections.reserve(n);

			for (int i = 0; i < n; i++) {
				uint32		namelen = r.get<uint16>();
				string16	name(namelen + 1);
				r.readbuff(name.begin(), (namelen + 1) * 2);
//				sections[i].name	= name;

				if (i == 0) {
					file.seek(content);
					sections.emplace_back(name, istream_offset(file));

				} else if (ISO::Browser b = storage[name]) {
					if (name == "MSCompressed") {
						LZX_CONTROL	*ctrl	= *b["ControlData"];
						LZX_RESET	*reset	= **b.Parse("Transform/{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/InstanceData/ResetTable");
						uint64		len		= reset->uncompressed_size;

						b = b["Content"];
						memory_reader		mem(b);
						malloc_block	buf(len);

						uint32	block	= uint32(reset->block_size);
						uint64	*start	= reset->start;
						while (len > block) {
							mem.seek(*start++);
							LZX_decoder		lzx(16, ctrl->ResetInterval(), /*ctrl->CacheSize(),*/ block);//ctrl->CacheSize() is input buffer size
							//lzx.block_length	= block;
							lzx.process(buf, buf.end(), mem);
							len -= block;
						}
						LZX_decoder		lzx(16, ctrl->ResetInterval(), /*ctrl->CacheSize(),*/ len);
						lzx.process(buf, buf.end(), mem);
						sections.emplace_back(name, memory_reader(buf));
//						sections[i].file = new LZXMEMistream(b[0], 16, b.Count());
					}
				}
			}
		}

		return p0;

		ISO_ptr<anything> p(id);

		for (int i = sect1.first_pmgl; i <= sect1.last_pmgl; ++i) {
			file.seek(chunks + i * sect1.chunk_size);
			file.readbuff(chunk, sect1.chunk_size);
			switch (*(uint32be*)chunk) {
				case 'PMGL': {
					DIR_CHUNK	*dir	= (DIR_CHUNK*)chunk;
					uint8		*end	= (uint8*)chunk + sect1.chunk_size - dir->free;
					byte_reader	r(dir + 1);
					while (r.p < end) {
						DIR_CHUNK::ENTRY	e	= r.get();
						if (e.length && !e.name.begins("::DataSpace/")) {
							const char *name;
							ISO_ptr<anything> p2 = GetDir(p, e.name, &name);
							auto&	pfile = sections[e.section].file;
							pfile.seek(e.offset);
							p2->Append(ReadRaw(name, pfile, e.length));
						}
					}
					break;
				}
				case 'PMGI': {
					INDEX_CHUNK	*index = (INDEX_CHUNK*)chunk;
					break;
				}
			}
		}
		return p;
	}

} chm;
