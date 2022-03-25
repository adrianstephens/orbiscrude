#include "bitmapfile.h"
#include "codec/texels/etc.h"
#include "platforms\vulkan\shared\graphics_defs.h"
#include "systems/conversion/channeluse.h"

//-----------------------------------------------------------------------------
//	Vulcan images
// KTX
// PKM:	etc, etc2
//-----------------------------------------------------------------------------

using namespace iso;

namespace PKM {

enum FORMAT {
	ETC1_RGB_NO_MIPMAPS			= 0,	// GL_ETC1_RGB8_OES
	ETC2_RGB_NO_MIPMAPS			= 1,	// GL_COMPRESSED_RGB8_ETC2
	ETC2_RGBA_NO_MIPMAPS_OLD	= 2,	// not used       -
	ETC2_RGBA_NO_MIPMAPS		= 3,	// GL_COMPRESSED_RGBA8_ETC2_EAC
	ETC2_RGBA1_NO_MIPMAPS		= 4,	// GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2
	ETC2_R_NO_MIPMAPS			= 5,	// GL_COMPRESSED_R11_EAC
	ETC2_RG_NO_MIPMAPS			= 6,	// GL_COMPRESSED_RG11_EAC
	ETC2_R_SIGNED_NO_MIPMAPS	= 7,	// GL_COMPRESSED_SIGNED_R11_EAC
	ETC2_RG_SIGNED_NO_MIPMAPS	= 8,	// GL_COMPRESSED_SIGNED_RG11_EAC
};

struct header {
	enum {
		MAGIC	= "PKM "_u32,
		VER10	= "10"_u16,
		VER20	= "20"_u16,
	};
	uint32		magic;// [6] ;
	uint16		ver;
	uint16be	format;
	uint16be	encoded_width, encoded_height, width, height;

	void	set(FORMAT _format, int _width, int _height) {
		magic	= MAGIC;
		ver		= VER20;
		format	= _format;
		width	= _width;
		height	= _height;
		encoded_width = (_width + 3) & ~3;
		encoded_height = (_height + 3) & ~3;
	}

	bool valid() const {
		if (magic != MAGIC)
			return false;

		if (ver == VER10) {
			if (format != ETC1_RGB_NO_MIPMAPS)
				return false;
		} else if (ver == VER20) {
			if (format > ETC2_RG_SIGNED_NO_MIPMAPS)
				return false;
		}

		return encoded_width >= width && encoded_width - width < 4
			&&	encoded_height >= height && encoded_height - height < 4;
	}
};

} // namespace PKM

struct KTX_header_base {
	uint8	identifier[12];

	static const uint8 k_identifier11[12];
	static const uint8 k_identifier20[12];

	int	valid() const {
		return memcmp(identifier, k_identifier11, sizeof(identifier)) == 0 ? 1
			:  memcmp(identifier, k_identifier20, sizeof(identifier)) == 0 ? 2
			:  0;
	}
};

const uint8 KTX_header_base::k_identifier11[12] = {
	0xab, 'K', 'T', 'X', ' ', '1', '1', 0xbb, 0x0d, 0x0a, 0x1a, 0x0a
};

const uint8 KTX_header_base::k_identifier20[12] = {
	0xab, 'K', 'T', 'X', ' ', '2', '0', 0xbb, '\r', '\n', '\x1A', '\n'
};

struct KTX_header0 : KTX_header_base {
	enum {
		ENDIAN_REF = 0x04030201,
	};
	uint32	endianness;
	uint32	glType;
	uint32	typeSize;
	uint32	glFormat;
	uint32	internalFormat;
	uint32	baseInternalFormat;
	uint32	pixelWidth;
	uint32	pixelHeight;
	uint32	pixelDepth;
	uint32	layerCount;
	uint32	faceCount;
	uint32	levelCount;
	uint32	bytesOfKeyValueData;
};

struct KTK_header : KTX_header_base {
	enum SuperCompression {
		SC_NONE			= 0,
		SC_BasisLZ		= 1,
		SC_ZStandard	= 2,
		SC_ZLIB			= 3,
	};
	struct KeyValuePair {
		uint32	length;
		uint8	keyAndValue[];
		//align(4) valuePadding
	};
	struct Index {
		uint32	offset, length;
	};
	struct Level : Index {
		uint64	uncompressed_length;
	};
	struct DescriptorHeader {
		uint32	vendorId : 17, descriptorType : 15;
		uint16	versionNumber, descriptorBlockSize;
	};
	struct BasicDataDescriptor : DescriptorHeader {
		uint8	colorModel, colorPrimaries, transferFunction, flags;
		uint8	texelBlockDimensions[4];
		uint8	bytesPlane[8];
	};
	struct DataFormatDescriptors {
		uint32				total_size;
		DescriptorHeader	descriptors[];
	};

	uint32	vkFormat;	//VkFormat
	uint32	typeSize;
	uint32	pixelWidth, pixelHeight, pixelDepth, layerCount, faceCount, levelCount;
	uint32	supercompressionScheme;

	Index	dfd;	//DataFormatDescriptor
	Index	kvd;	//KeyValue Pairs
	Index	sgd;	//SuperCompression Global Data
	Level	levels[];

	// DataFormatDescriptors
	// KeyValue Pairs
	// Supercompression Global Data 
	// Mip Level Array 

	void	set(VkFormat format, int width, int height) {
		memcpy(identifier, k_identifier20, sizeof(identifier));
		vkFormat		= format;
		pixelWidth		= width;
		pixelHeight		= height;
		pixelDepth		= 0;
		layerCount		= 0;
		faceCount		= 1;
		levelCount		= 1;
	}
};


class PKMFileHandler : public BitmapFileHandler {
	const char*		GetExt() override { return "pkm"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		PKM::header		header;
		if (file.read(header) && header.valid()) {
			
			malloc_block	mem = malloc_block::unterminated(file);
			int	wb = header.encoded_width / 4, hb = header.encoded_height / 4;

			if (header.format < PKM::ETC2_R_NO_MIPMAPS) {
				ISO_ptr<bitmap>	bm(id, header.width, header.height);

				switch (header.format) {
					case PKM::ETC1_RGB_NO_MIPMAPS:
						copy(make_block<const ETC1>(mem, wb, hb), bm->All());
						break;

					case PKM::ETC2_RGB_NO_MIPMAPS:
						copy(make_block<const ETC2_RGB>(mem, wb, hb), bm->All());
						break;

					case PKM::ETC2_RGBA_NO_MIPMAPS:
						copy(make_block<const ETC2_RGBA>(mem, wb, hb), bm->All());
						break;

					case PKM::ETC2_RGBA1_NO_MIPMAPS:
						copy(make_block<const ETC2_RGBA1>(mem, wb, hb), bm->All());
						break;
				}
				return bm;

			} else {
				ISO_ptr<HDRbitmap>	bm(id, header.width, header.height);
				switch (header.format) {
					case PKM::ETC2_R_NO_MIPMAPS:
						copy(make_block<const ETC2_R11>(mem, wb, hb), bm->All());
						break;

					case PKM::ETC2_RG_NO_MIPMAPS:
						copy(make_block<const ETC2_RG11>(mem, wb, hb), bm->All());
						break;

					case PKM::ETC2_R_SIGNED_NO_MIPMAPS:
						copy(make_block<const ETC2_R11S>(mem, wb, hb), bm->All());
						break;

					case PKM::ETC2_RG_SIGNED_NO_MIPMAPS:
						copy(make_block<const ETC2_RG11S>(mem, wb, hb), bm->All());
						break;
				}
				return bm;
			}
		}
		return ISO_NULL;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (ISO_ptr<bitmap2> bm2 = ISO_conversion::convert<bitmap2>(p)) {
			switch (bm2->BitmapType()) {
				case bitmap2::BITMAP: {
					PKM::header		header;
					PKM::FORMAT		format;
					ISO_ptr<bitmap>	bm		= *bm2;

					ChannelUse cu = GetFormatString(bm).begin();
					if (!cu)
						cu = ChannelUse(&*bm);

					if (cu.analog.a & ChannelUse::chans::SIZE_MASK == 1)
						format	= PKM::ETC2_RGBA1_NO_MIPMAPS;
					else if (cu.analog.a)
						format	= PKM::ETC2_RGBA_NO_MIPMAPS;
					else if (cu.analog.r == 0x84)
						format	= PKM::ETC1_RGB_NO_MIPMAPS;
					else
						format	= PKM::ETC2_RGB_NO_MIPMAPS;

					header.set(format, bm->Width(), bm->Height());
					file.write(header);

					uint32			block_size = format == PKM::ETC2_RGBA_NO_MIPMAPS ? sizeof(ETC2_RGBA) : sizeof(ETC1);
					malloc_block	mem(header.encoded_width * header.encoded_height / 16 * block_size);

					int	wb = header.encoded_width / 4, hb = header.encoded_height / 4;

					switch (format) {
						case PKM::ETC1_RGB_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC1>(mem, wb, hb));
							break;

						case PKM::ETC2_RGB_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC2_RGB>(mem, wb, hb));
							break;

						case PKM::ETC2_RGBA_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC2_RGBA>(mem, wb, hb));
							break;

						case PKM::ETC2_RGBA1_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC2_RGBA1>(mem, wb, hb));
							break;
					}
					return mem.write(file);
				}

				case bitmap2::HDRBITMAP: {
					PKM::header			header;
					PKM::FORMAT			format;
					ISO_ptr<HDRbitmap>	bm =	 *bm2;

					ChannelUse cu = GetFormatString(bm).begin();
					if (!cu)
						cu = ChannelUse(&*bm);

					if (cu.analog.g)
						format	= cu.IsSigned() ? PKM::ETC2_RG_SIGNED_NO_MIPMAPS : PKM::ETC2_RG_NO_MIPMAPS;
					else
						format	= cu.IsSigned() ? PKM::ETC2_R_SIGNED_NO_MIPMAPS : PKM::ETC2_R_NO_MIPMAPS;

					header.set(format, bm->Width(), bm->Height());
					file.write(header);

					uint32			block_size = sizeof(ETC1);
					switch (format) {
						case PKM::ETC2_RG_NO_MIPMAPS:
						case PKM::ETC2_RG_SIGNED_NO_MIPMAPS:
							block_size <<= 1;
					}
					malloc_block	mem(header.encoded_width * header.encoded_height / 16 * block_size);

					int	wb = header.encoded_width / 4, hb = header.encoded_height / 4;

					switch (format) {
						case PKM::ETC2_R_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC2_R11>(mem, wb, hb));
							break;

						case PKM::ETC2_RG_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC2_RG11>(mem, wb, hb));
							break;

						case PKM::ETC2_R_SIGNED_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC2_R11S>(mem, wb, hb));
							break;

						case PKM::ETC2_RG_SIGNED_NO_MIPMAPS:
							copy(bm->All(), make_block<ETC2_RG11S>(mem, wb, hb));
							break;
					}
					return mem.write(file);
				}
			}
		}
		return false;
	}
} pkm;
