#include "iso/iso_files.h"
#include "extra/xml.h"
#include "comms/bz2_stream.h"
#include "comms/zlib_stream.h"
#include "archive_help.h"
#include "bitmap/bitmap.h"
#include "codec/apple_compression.h"

using namespace iso;

template<typename T> ISO_ptr<T>	MakeSizedPtr(tag2 id, const_memory_block mem) {
	auto	p = ISO::MakePtrSize(ISO::getdef<T>(), id, mem.size32());
	mem.copy_to(p);
	return p;
}

//-----------------------------------------------------------------------------
//	PKG
//-----------------------------------------------------------------------------

class PKGFileHandler : public FileHandler {
#pragma pack(1)
	struct xar_header : bigendian_types {
		enum {MAGIC = 'xar!'};
		uint32	magic;
		uint16	size;
		uint16	version;
		uint64	toc_length_compressed;
		uint64	toc_length_uncompressed;
		uint32	cksum_alg;
		bool	valid() const { return magic == MAGIC; }
	};
#pragma pack()
	const char*		GetExt() override { return "pkg";	}
	const char*		GetDescription() override { return "OSX Install Package"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		xar_header	header;
		return file.read(header) && header.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} pkg;

ISO_ptr<void> PKGFileHandler::Read(tag id, istream_ref file) {
	xar_header	h = file.get();
	if (!h.valid())
		return ISO_NULL;

	uint32	outlen	= uint32(h.toc_length_uncompressed);
	malloc_block	out(outlen);

#if 1
	file.seek(h.size);
	zlib_reader(file).readbuff(out, outlen);
#else
	z_stream	z;
	clear(z);
	inflateInit(&z);

	uint32	inlen	= uint32(h.toc_length_compressed);
	void	*in		= malloc(inlen);

	file.seek(h.size);
	file.readbuff(in, inlen);

	z.avail_in		= inlen;
	z.avail_out		= outlen;
	z.next_in		= (Bytef*)in;
	z.next_out		= (Bytef*)out;

	inflate(&z, Z_FINISH);
	inflateEnd(&z);
	free(in);
#endif

	ISO_ptr<anything>		t(id);
	memory_reader				mi(out);
	XMLreader::Data	data;
	XMLreader				xml(mi);

	fixed_string<256>			fname;
	enum {ENC_RAW, ENC_BZIP2}	encoding = ENC_RAW;
	uint32	length, offset, size;

	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND: {
				if (data.Is("length")) {
					xml.ReadNext(data, XMLreader::TAG_CONTENT);
					from_string(data.Content(), length);

				} else if (data.Is("offset")) {
					xml.ReadNext(data, XMLreader::TAG_CONTENT);
					from_string(data.Content(), offset);

				} else if (data.Is("size")) {
					xml.ReadNext(data, XMLreader::TAG_CONTENT);
					from_string(data.Content(), size);

				} else if (data.Is("encoding")) {
					const char *val = data.Find("style");
					if (str(val) == "application/octet-stream")
						encoding = ENC_RAW;
					else if (str(val) == "application/x-bzip2")
						encoding = ENC_BZIP2;

				} else if (data.Is("name")) {
					xml.ReadNext(data, XMLreader::TAG_CONTENT);
					fname = data.Content();
				}
				break;
			}
			case XMLreader::TAG_END:
				if (data.Is("file")) {
					file.seek(offset);
					switch (encoding) {
						case ENC_RAW:
							if (fname == "Payload" || fname == "Scripts") {
								file.seek(offset + 10);	// skip gz header
								deflate_reader	gzs(file);
								char		oct[76];
								char		name[1024];
								for (;;) {
									gzs.read(oct);
									uint32	len = 0;
									for (int i = 76 - 11; i < 76; i++)
										len = len * 8 + oct[i] - '0';
									for (int i = 0; name[i] = gzs.getc(); i++);
									if (strcmp(name, "TRAILER!!!") == 0)
										break;
									if (len) {
										char	*p = strrchr(name, '/');
										if (!p)
											p = name;
										else
											*p++ = 0;
										GetDir(t, name)->Append(ReadData2(p, gzs, len, true));
									}
								}
								break;
							}
	/*						if (fname == "Scripts") {
								if (FileHandler *gz = Get("gz")) {
									t->Append(gz->Read(fname, istream_offset(file, size)));
								}
								break;
							}
	*/						t->Append(ReadData2(fname, file, size, true));
							break;
						case ENC_BZIP2:
							t->Append(ReadData1(fname, BZ2istream(file, size).me(), size, true));
							break;
					}
				}
				break;
		}
	}

	return t;
}

//-----------------------------------------------------------------------------
//	BOM
//-----------------------------------------------------------------------------

struct BOM : bigendian_types {
	struct Header {
		char		magic[8]; // = BOMStore
		uint32		unknown0; // = 1?
		uint32		unknown1; // = 73 = 0x49?
		uint32		indexOffset; // Length of first part
		uint32		indexLength; // Length of second part
		uint32		varsOffset;
		uint32		varsLength;

		bool	valid() const {
			return str(magic, 8) == "BOMStore";
		}
	};

	struct Index {
		uint32		address;
		uint32		length;
	};

	struct IndexHeader {
		uint32		max_index;//unknown0; // TODO What is this?  It's not the length of the array.
		Index		index[1];
	};

	struct Tree { // 21 bytes
		enum {TAG = 'tree'};
		char		tag[4];		// = "tree"
		uint32		version;
		uint32		child;		// TODO Not sure about this one
		uint32		nodeSize;	// byte count of each entry in the tree (Paths)
		uint32		leafCount;	// total number of paths in all leaves combined
	};

	struct Var {
		packed<uint32>	index;
		pascal_string	name;
		const Var		*next()	const { return (const Var*)name.end(); }
	};

	struct Vars {
		uint32		count; // Number of entries that follow
		auto		first() const { return (const Var*)(this + 1); }
		auto		vars() const { return make_range_n(next_iterator<const Var>((const Var*)(this + 1)), count); }
	};

	struct Node {
		struct Entry {
			uint32	value, key;
		};
		uint16		isLeaf;		// if 0 then this entry refers to other Node entries
		uint16		count;		// for leaf, count of paths. for top level, # of leafs - 1
		uint32		forward;	// next leaf, when there are multiple leafs
		uint32		backward;	// previous leaf, when there are multiple leafs
		Entry		entries[1];
	};

	typedef iso::uint32 BlockID;

	malloc_block	data;
	malloc_block	mbi;	//	IndexHeader *indices	= mbi;
	malloc_block	mbv;	//	Vars		*vars		= mbv;

	auto	vars() const {
		return ((Vars*)mbv)->vars();
	}

	BOM(istream_ref file) {
		Header	header(file.get());
		if (!header.valid())
			return;

		data.read(file, header.indexOffset - sizeof(header));
		mbi.read(file, header.indexLength);
		file.seek(header.varsOffset);
		mbv.read(file, header.varsLength);
	}
	bool	valid() const { return !!data; }

	BlockID GetID(const char *name) const {
		for (auto &v : vars()) {
			if (v.name == name)
				return v.index;
		}
		return 0;
	}

	const_memory_block	GetData(BlockID id) const {
		if (id) {
			IndexHeader *indices	= mbi;
			if (id < indices->max_index) {
				Index		&x			= indices->index[id];
				return {data + (x.address - sizeof(Header)), x.length};
			}
		}
		return none;
	}
	const_memory_block	GetData(const char *name) const {
		if (auto id = GetID(name))
			return GetData(id);
		return none;
	}

	struct TreeIterator {
		const BOM			*bom;
		const Tree			*tree;
		const Node			*node;
		const Node::Entry	*i, *e;
		struct Element { const_memory_block key, value; };

		void	SetNode(const Node *node2) {
			if (node = node2) {
				i = &node->entries[0];
				e = i + node->count;
			} else {
				i = e = 0;
			}
		}

		TreeIterator() : bom(0), tree(0), node(0), i(0), e(0) {}
		TreeIterator(const BOM *bom, const Tree *tree) : bom(bom), tree(tree) {
			node = bom->GetData(tree->child);
			while (!node->isLeaf)
				node = bom->GetData(node->entries[0].value);
			i = &node->entries[0];
			e = i + node->count;
		}
		Element	operator*()				const	{ return {bom->GetData(i->key), bom->GetData(i->value)}; }
		bool operator!=(const TreeIterator &b) const { return i != b.i; }
		TreeIterator	&operator++() {
			if (++i == e)
				SetNode(bom->GetData(node->forward));
			return *this;
		}
	};

	range<TreeIterator>	GetTree(BlockID id) const {
		const Tree	*tree = GetData(id);
		return {TreeIterator(this, tree), {}};
	}
	range<TreeIterator>	GetTree(const char *name) const {
		if (auto id = GetID(name))
			return GetTree(id);
		return {};
	}
};

struct BOM2 : BOM {
	enum {
		TYPE_FILE	= 1, // PathInfo2 is exe=88 regular=35 bytes
		TYPE_DIR	= 2, // PathInfo2 is 31 bytes
		TYPE_LINK	= 3, // PathInfo2 is 44? bytes
		TYPE_DEV	= 4, // PathInfo2 is 35 bytes
	};
	enum {			// Not sure of all the corect values here
		ARCH_PPC	= 0,
		ARCH_I386	= 1 << 12,
		ARCH_HPPA	= 0,
		ARCH_SPARC	= 0,
	};
	struct PathInfo2 {
		uint8		type;			// See types above
		uint8		unknown0;		// = 1?
		uint16		architecture;	// Not sure exactly what this means
		uint16		mode;
		uint32		user;
		uint32		group;
		uint32		modtime;
		uint32		size;
		uint8		unknown1;		// = 1?
		uint32		checksum;		// or devType
		uint32		linkNameLength;
		char		linkNameStart[1];
		auto	linkName() const { return str(linkNameStart, linkNameLength); }
		// TODO executable files have a buch of other crap here
	};

	struct PathInfo1 {
		uint32	id;
		uint32	index;	// Pointer to PathInfo2
	};

	struct File {
		uint32	parent; // Parent PathInfo1->id
		embedded_string	name;
	};

	BOM2(istream_ref file) : BOM(file) {}
};

ISO_DEFUSERCOMPV(BOM2::PathInfo2, type, architecture, mode, user, group, modtime, size, checksum, linkName);

class BOMFileHandler : public FileHandler {
	const char*		GetExt() override { return "bom";	}
	const char*		GetDescription() override { return "OSX Bill of Materials"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return str(file.get<BOM::Header>().magic) == "BOMStore" ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		BOM2	bom(file);
		if (!bom.valid())
			return ISO_NULL;

		ISO_ptr<anything>	p(id);

		for (auto &v : bom.vars()) {
			if (v.name == "Paths") {
				ISO_ptr<Folder>	root("Paths");
				p->Append(root);

				hash_map<uint32, ISO_ptr<Folder>> folders;
				folders[0] = root;

				for (auto &&i :  bom.GetTree(v.index)) {
					const BOM2::File		*file	= i.key;
					const BOM2::PathInfo1	*info1	= i.value;
					const BOM2::PathInfo2	*info2	= bom.GetData(info1->index);
#if 1
					if (info2->type == BOM2::TYPE_DIR) {
						ISO_ptr<Folder>	me(file->name);
						folders[info1->id] =  me;
						folders[file->parent].put()->Append(me);
					} else {
						folders[file->parent].put()->Append(MakeSizedPtr<BOM2::PathInfo2>(file->name, bom.GetData(info1->index)));
					}
#else
					ISO_ptr<anything>	e(fn);
					p->Append(e);
					if (info2->type != BOM2::TYPE_DIR && info2->type != BOM2::TYPE_LINK)
						e->Append(ISO_ptr<int>("mode", info2->mode));

					if (info2->type == BOM2::TYPE_FILE || info2->type == BOM2::TYPE_LINK) {
						e->Append(ISO_ptr<int>("modtime", info2->modtime));
						e->Append(ISO_ptr<int>("checksum", info2->checksum));
					}

					if (info2->type != BOM2::TYPE_DIR && info2->type != BOM2::TYPE_DEV)
						e->Append(ISO_ptr<int>("size", info2->size));

					if (info2->type == BOM2::TYPE_LINK)
						e->Append(ISO_ptr<string>("size", str(info2->linkName, info2->linkNameLength)));

					if (info2->type == BOM2::TYPE_DEV)
						e->Append(ISO_ptr<int>("devtype", info2->checksum));
#endif
				}
			}
		}
		return p;

	}
} bom;

//-----------------------------------------------------------------------------
//	CAR
//-----------------------------------------------------------------------------
enum Compression {				// As seen in _CUIConvertCompressionTypeToString
	uncompressed	= 0,
	rle				= 1,
	zip				= 2,
	lzvn			= 3,
	lzfse			= 4,
	jpeg_lzfse		= 5,
	blurred			= 6,
	astc			= 7,
	palette_img		= 8,
	hevc			= 9,
	deepmap_lzfse	= 10,
	deepmap2		= 11,
};

size_t uncompress(const memory_block &dst, const const_memory_block &src, Compression compression) {
	size_t written;

	switch (compression) {
		default:
		case uncompressed:
			*dst = *src;
			return src.length();

		case rle:
			transcode(ADC::decoder(), dst, src, &written);
			return written;

		case zip:
		case lzvn:
			transcode(LZVN::decoder(), dst, src, &written);
			return written;
		case lzfse:
			transcode(LZFSE::decoder(), dst, src, &written);
			return written;
			//case jpeg_lzfse:
		//case blurred:
		//case astc:
		//case palette_img:
		//case hevc:
		//case deepmap_lzfse:
		//case deepmap2:
		//	return data;
	}
}

namespace CAR {
	template<bool be> struct CTAR {
		using uint32 = endian_t<iso::uint32, be>;
		enum	{TAG = endian_const('CTAR', be)};
		uint32	tag;				// 'CTAR'
		uint32	coreuiVersion;
		uint32	storageVersion;
		uint32	storageTimestamp;
		uint32	renditionCount;
		char	mainVersionString[128];
		char	versionString[256];
		GUID	uuid;
		uint32	associatedChecksum;
		uint32	schemaVersion;
		uint32	colorSpaceID;
		uint32	keySemantics;
	};

	struct META {
		enum	{ TAG = 'META'};
		uint32	tag;				// 'META'
		char	thinningArguments[256];
		char	deploymentPlatformVersion[256];
		char	deploymentPlatform[256];
		char	authoringTool[256];
	};

	enum AttributeType {				// As seen in -[CUIRenditionKey nameOfAttributeName:]
		ThemeLook 				= 0,
		Element					= 1,
		Part					= 2,
		Size					= 3,
		Direction				= 4,
		placeholder				= 5,
		Value					= 6,
		ThemeAppearance			= 7,
		Dimension1				= 8,
		Dimension2				= 9,
		State					= 10,
		Layer					= 11,
		Scale					= 12,
		Unknown13				= 13,
		PresentationState		= 14,
		Idiom					= 15,
		Subtype					= 16,
		Identifier				= 17,
		PreviousValue			= 18,
		PreviousState			= 19,
		HorizontalSizeClass		= 20,
		VerticalSizeClass		= 21,
		MemoryLevelClass		= 22,
		GraphicsFeatureSetClass = 23,
		DisplayGamut			= 24,
		DeploymentTarget		= 25
	};

	template<bool be> struct Attribute {
		using uint16 = endian_t<iso::uint16, be>;
		uint16	name, value;
	};
	template<bool be> struct Attributes {
		using uint16 = endian_t<iso::uint16, be>;
		uint16	hotspot_x, hotspot_y;
		uint16	num;
		auto	attributes() const { return make_range_n((Attribute<be>*)(this + 1), num); }
	};

	enum TLVType {			// As seen in -[CSIGenerator writeResourcesToData:]
		Slices 					= 0x3E9,
		Metrics 				= 0x3EB,
		BlendModeAndOpacity		= 0x3EC,
		UTI	 					= 0x3ED,
		EXIFOrientation			= 0x3EE,
		ExternalTags			= 0x3F0,
		Frame					= 0x3F1,
	};
	enum LayoutType {
		LayoutTextEffect		= 0x007,
		LayoutVector			= 0x009,

		ThemeOnePartFixedSize						= 10,
		ThemeOnePartTile							= 11,
		ThemeOnePartScale							= 12,
		ThemeThreePartHTile							= 20,
		ThemeThreePartHScale						= 21,
		ThemeThreePartHUniform						= 22,
		ThemeThreePartVTile							= 23,
		ThemeThreePartVScale						= 24,
		ThemeThreePartVUniform						= 25,
		ThemeNinePartTile							= 30,
		ThemeNinePartScale							= 31,
		ThemeNinePartHorizontalUniformVerticalScale	= 32,
		ThemeNinePartHorizontalScaleVerticalUniform = 33,
		ThemeNinePartEdgesOnly 						= 34,
		ThemeManyPartLayoutUnknown 					= 40,
		ThemeAnimationFilmstrip 					= 50,

		LayoutData				= 0x3E8,
		LayoutExternalLink		= 0x3E9,
		LayoutLayerStack		= 0x3EA,
		LayoutInternalReference	= 0x3EB,
		LayoutPackedImage		= 0x3EC,
		LayoutNameList			= 0x3ED,
		LayoutUnknownAddObject	= 0x3EE,
		LayoutTexture			= 0x3EF,
		LayoutTextureImage		= 0x3F0,
		LayoutColor				= 0x3F1,
		LayoutMultisizeImage	= 0x3F2,
		LayoutLayerReference	= 0x3F4,
		LayoutContentRendition	= 0x3F5,
		LayoutRecognitionObject	= 0x3F6,
	};

	template<bool be> struct KeyFormat {
		using uint32 = endian_t<iso::uint32, be>;
		enum	{TAG = endian_const('kfmt', be)};
		uint32	tag;								// 'kfmt'
		uint32	version;
		uint32	maximumRenditionKeyTokenCount;
		auto	all() const { return make_range_n((uint32*)(this + 1), maximumRenditionKeyTokenCount); }
	};

	template<bool be> struct TLV {
		using uint32 = endian_t<iso::uint32, be>;
		uint32	type;
		uint32	length;
		const TLV *next() const { return (const TLV*)((const char*)(this + 1) + length); }
	};
	template<bool be> struct ColorRendition {
		using uint32 = endian_t<iso::uint32, be>;
		enum	{TAG = endian_const('COLR',be)};
		uint32	tag;					// COLR
		uint32	version;
		uint32	colorSpace;				// id in bottom 8 bits
		uint32	numberOfComponents;
		double	components[];
		auto	all() const { return make_range_n(&components[0], numberOfComponents); }
	};

	template<bool be> struct RawDataRendition {
		using uint32 = endian_t<iso::uint32, be>;
		enum	{TAG = endian_const('RAWD',be)};
		uint32	tag;					// RAWD
		uint32	version;
		uint32	rawDataLength;
		const_memory_block	data() const { return {this + 1, rawDataLength}; }
	};

	template<bool be> struct ThemePixelRendition {
		using uint32 = endian_t<iso::uint32, be>;
		enum	{TAG = endian_const('CELM',be)};

		uint32	tag;					// 'CELM'
		uint32	version;
		uint32	compression;
		uint32	rawDataLength;
		const_memory_block	data()	const { return {this + 1, rawDataLength}; }
	};
	template<bool be> struct CTSI {
		using uint32 = endian_t<iso::uint32, be>;
		using uint16 = endian_t<iso::uint16, be>;
		enum	{TAG = endian_const('CTSI', be)};


		struct FLAGS {
			iso::uint32	isHeaderFlaggedFPO:1,
						isExcludedFromContrastFilter:1,
						isVectorBased:1,
						isOpaque:1,
						bitmapEncoding:4,
						optOutOfThinning:1,
						isFlippable:1,
						isTintable:1,
						preservedVectorRepresentation:1,
						reserved:20;
		};

		uint32		tag;				// 'CTSI'
		uint32		version;
		uint32		flags;

		uint32		width;
		uint32		height;
		uint32		scaleFactor;
		uint32		pixelFormat;
		uint32		colorSpace;

		uint32		modtime;
		uint16		layout;				// LayoutType
		uint16		zero0;
		char		name[128];

		uint32		tlvLength;
		uint32		unknown;
		uint32		zero1;
		uint32		renditionLength;

		const_memory_block	tlv_data()	const { return {this + 1, tlvLength}; }
		auto				tlv()		const { return make_next_range<TLV<be>>(tlv_data()); }
		const_memory_block	rendition()	const { return {tlv_data().end(), renditionLength}; }	// RawDataRendition, or ThemePixelRendition, or ColorRendition
		ISO_ptr<void>		get_bitmap()const;
	};

	template<bool be> ISO_ptr<void> CTSI<be>::get_bitmap() const {
		switch (pixelFormat) {
			case 0:
				if (layout == LayoutColor) {
					const ColorRendition<be>	*data = rendition();
					if (data->numberOfComponents == 4) {
						// Use the hardcoded DeviceRGB color space instead of the real colorSpace from the colorSpaceID
					}
				}
				break;

			case 'DATA': {
				const RawDataRendition<be> *data = rendition();
				if (data->tag == 'RAWD')
					return ISO::MakePtr(0, malloc_block(data->data()));
				break;
			}
			case 'JPEG': {
				const RawDataRendition<be> *data = rendition();
				if (data->tag == 'RAWD') {
					if (FileHandler	*fh = FileHandler::Get("jpg"))
						return fh->Read(0, lvalue(memory_reader(data->data())));
					return ISO::MakePtr(0, malloc_block(data->data()));
				}
				break;
			}
			case 'HEIF': {
				const RawDataRendition<be> *data = rendition();
				if (data->tag == 'RAWD') {
					if (FileHandler	*fh = FileHandler::Get("heif"))
						return fh->Read(0, lvalue(memory_reader(data->data())));
					return ISO::MakePtr(0, malloc_block(data->data()));
				}
			}
			case 'ARGB':
			case 'GA8 ':
			case 'RGB5':
			case 'RGBW':
			case 'GA16': {
				ISO_ptr<bitmap>	bm(0, width, height);
				const ThemePixelRendition<be> *data = rendition();
				uncompress(memory_block(bm->ScanLine(0), width * height * 4), data->data(), (Compression)get(data->compression));
				return bm;
			}
			case 'PDF ': {
				const RawDataRendition<be> *data = rendition();
				if (data->tag == 'RAWD')
					return ISO::MakePtr(0, malloc_block(data->data()));
			}
			default:
				break;
		}
		return ISO::MakePtr(0, malloc_block(rendition()));

	}

} // namespace CAR

template<bool be> ISO_DEFUSERCOMPVT2(CAR::CTAR<be>, coreuiVersion, storageVersion, storageTimestamp, renditionCount, mainVersionString, versionString, uuid, associatedChecksum, schemaVersion, colorSpaceID, keySemantics);
ISO_DEFUSERCOMPV(CAR::META, thinningArguments, deploymentPlatformVersion, deploymentPlatform, authoringTool);
template<bool be> ISO_DEFUSERCOMPVT2(CAR::Attribute<be>, name, value);
template<bool be> ISO_DEFUSERCOMPVT2(CAR::Attributes<be>, hotspot_x, hotspot_y, attributes);
template<bool be> ISO_DEFUSERCOMPVT2(CAR::TLV<be>, type, length);
template<bool be> ISO_DEFUSERCOMPVT2(CAR::KeyFormat<be>, version, all);
template<bool be> ISO_DEFUSERCOMPVT2(CAR::CTSI<be>, version, flags, width, height, scaleFactor, pixelFormat, colorSpace, modtime, layout, name, tlv, rendition, get_bitmap);


class CARFileHandler : public FileHandler {
	const char*		GetExt() override { return "car";	}
	const char*		GetDescription() override { return "OSX Compiled Asset Catalog"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return str(file.get<BOM::Header>().magic) == "BOMStore" ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		BOM		bom(file);
		if (!bom.valid())
			return ISO_NULL;

		ISO_ptr<anything>	p(id);

		for (auto &v : bom.vars()) {
			auto	data = bom.GetData(v.index);
			uint32	tag	= *(const uint32*)data;
			switch (tag) {
				case CAR::CTAR<false>::TAG:
					p->Append(ISO::MakePtr(v.name, *(const CAR::CTAR<false>*)data));
					break;

				case CAR::CTAR<true>::TAG:
					p->Append(ISO::MakePtr(v.name, *(const CAR::CTAR<true>*)data));
					break;

				case "META"_u32://CAR::META::TAG:
					p->Append(ISO::MakePtr(v.name, *(const CAR::META*)data));
					break;

				case CAR::KeyFormat<false>::TAG:
					p->Append(ISO::MakePtr(v.name, *(const CAR::KeyFormat<false>*)data));
					break;

				case CAR::KeyFormat<true>::TAG:
					p->Append(ISO::MakePtr(v.name, *(const CAR::KeyFormat<true>*)data));
					break;

				case "tree"_u32: {
					ISO_ptr<anything>	p2(v.name);
					p->Append(p2);

					/*if (v.name == "APPEARANCEKEYS") {
						for (auto i : bom.GetTree(v.index)) {
							p2->Append(ISO::MakePtr((const char*)i.key, malloc_block(i.value)));
						}
					}  else */if (v.name == "FACETKEYS") {
						for (auto i : bom.GetTree(v.index)) {
							p2->Append(MakeSizedPtr<CAR::Attributes<false>>((const char*)i.key, i.value));
						}
					} else if (v.name == "RENDITIONS") {
						for (auto i : bom.GetTree(v.index)) {
							auto	data = i.value;
							switch (*(const uint32*)data) {
								case  CAR::CTSI<false>::TAG:
									p2->Append(MakeSizedPtr<CAR::CTSI<false>>("key", i.value));
									break;

								case  CAR::CTSI<true>::TAG: {
									const CAR::CTSI<true>	*csi = i.value;
									p2->Append(ISO::MakePtr("key", *csi));
									break;
								}
							}
						}
					} else {
						for (auto i : bom.GetTree(v.index)) {
							const char *key = 0;
							if (i.key && is_alphanum(((const char*)i.key)[0]))
								key = i.key;
							p2->Append(ISO::MakePtr(key, malloc_block(i.value)));
						}
					}
					break;
				}
				default:
					p->Append(ISO::MakePtr(v.name, malloc_block(data)));
					break;
			}

		}
		return p;
	}
} car;

