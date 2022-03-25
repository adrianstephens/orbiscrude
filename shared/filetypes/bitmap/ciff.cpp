#include "bitmapfile.h"
#include "codec/vlc.h"

//-----------------------------------------------------------------------------
//	CANON raw image
//	CRW files
//-----------------------------------------------------------------------------

using namespace iso;

struct CIFFheader : packed_types<littleendian_types> {
	uint16		ByteOrder;		//0				"II"		//"II" means Intel (little-endian) order, which is the only order I've seen since Canon is using x86 processors in its current cameras, but presumably this could be "MM" for future models.
	uint32		HeaderLength;	//2				0x0000001a	//32-bit integer giving the length of the CRW file header. For current camera models the header is 26 bytes long.
	char		Signature[8];	//6				"HEAPCCDR"	//This series of characters identifies the file as a Canon CRW file. The signature is "HEAPJPGM" for CIFF information in APP0 of JPEG images.
	uint32		CRWVersion;		//14			0x00010002	//32-bit integer giving the major (high 16 bits) and minor (low 16 bits) CRW file version numbers. The version is 1.2 for current cameras.
	uint64		Reserved;		//18			0			//Two 32-bit integers, currently set to zero.

	bool	validate() const {
		return ByteOrder == 'II' && HeaderLength == sizeof(*this) && memcmp(Signature, "HEAPCCDR", 8) == 0 && CRWVersion == 0x00010002;
	}
};

struct CIFFentry : packed_types<littleendian_types> {
	enum Location {
		LOC_ValueData,	//Values are stored in the ValueData block, at the specified Offset and Size
		LOC_Directory,	//Values are stored in the Size and Offset fields of the directory entry. Values stored here are limited to a maximum size of 8 bytes.
	};
	enum Format {
		FMT_bytes,		//1-Byte	A series of bytes
		FMT_string,		//1-Byte	A null-terminated ASCII string
		FMT_int16,		//2-Byte	A series of 16-bit integers
		FMT_int32,		//4-Byte	A series of 32-bit integers or floats
		FMT_struct,		//1-Byte	A structure which is a mixture of formats
		FMT_subdir,		//1-Byte	A subdirectory block
		FMT_subdir2,
		FMT_unknown,
	};
	union Tag {
		struct {
			iso::uint16	index:11, format:3, location:2;
		};
		uint16	u;
		Tag() : u(0) {}
	};
	Tag		tag;		//0	16-bit integer identifying the type of data
	union {
		struct {
			uint32	size;		//2	32-bit integer giving the number of bytes in the value data
			uint32	offset;		//6	32-bit integer offset that gives the number of bytes from the start of the ValueData block to the start of the value data for this directory entry
		};
		uint64	data;
	};
	CIFFentry() : data(0) {}
};

enum CIFF_TAGS {						//EXIF	SubDir	Size		Description
//FMT_bytes
	NullRecord				= 0x0000,	//-		any		0			This is a null directory entry
	FreeBytes				= 0x0001,	//-		any		varies		Unused bytes in the ValueData
//	-						= 0x0006,	//-		0x300b	8			-
	CanonColorInfo1			= 0x0032,	//-		0x300b	768 or 2048	Block of color information (format unknown)
//	?						= 0x0036,	//-		0x300b	varies		-
//	?						= 0x003f,	//-		0x300b	5120		-
//	?						= 0x0040,	//-		0x300b	256			-
//	?						= 0x0041,	//-		0x300b	256			-

//FMT_string
	CanonFileDescription	= 0x0805,	//-		0x2804	32			Description of the file format. eg) "EOS DIGITAL REBEL CMOS RAW"
	UserComment				= 0x0805,	//-		0x300a	256			User comment (usually blank)
	CanonRawMakeModel		= 0x080a,	//-		0x2807	32			Two end-to-end null-terminated ASCII strings giving the camera make and model. eg) "Canon","Canon EOS DIGITAL REBEL"
	CanonFirmwareVersion	= 0x080b,	//0x07	0x3004	32			Firmware version. eg) "Firmware Version 1.1.1"
	ComponentVersion		= 0x080c,	//-		?		?			-
	ROMOperationMode		= 0x080d,	//-		0x3004	4			eg) The string "USA" for 300D's sold in North America
	OwnerName				= 0x0810,	//0x09	0x2807	32			Owner's name. eg) "Phil Harvey"
	CanonImageType			= 0x0815,	//0x06	0x2804	32			Type of file. eg) "CRW:EOS DIGITAL REBEL CMOS RAW"
	OriginalFileName		= 0x0816,	//-		0x300a	32			Original file name. eg) "CRW_1834.CRW"
	ThumbnailFileName		= 0x0817,	//-		0x300a	32			Thumbnail file name. eg) "CRW_1834.THM"

//FMT_int16
	TargetImageType			= 0x100a,	//-		0x300a	2			0=real-world subject, 1=written document
	ShutterReleaseMethod	= 0x1010,	//-		0x3002	2			0=single shot, 1=continuous shooting
	ShutterReleaseTiming	= 0x1011,	//-		0x3002	2			0=priority on shutter, 1=priority on focus
//	-						= 0x1014,	//-		0x3002	8			-
	ReleaseSetting			= 0x1016,	//-		0x3002	2			-
	BaseISO					= 0x101c,	//-		0x3004	2			The camera body's base ISO sensitivity
//	-						= 0x1026,	//-		0x300a	6			-
	CanonFlashInfo			= 0x1028,	//0x03	0x300b	8			Unknown information, flash related
	FocalLength				= 0x1029,	//0x02	0x300b	8			Four 16 bit integers: 0) unknown, 1) focal length in mm, 2-3) sensor width and height in units of 1/1000 inch
	CanonShotInfo			= 0x102a,	//0x04	0x300b	varies		Data block giving shot information
	CanonColorInfo2			= 0x102c,	//-		0x300b	256			Data block of color information (format unknown)
	CanonCameraSettings		= 0x102d,	//0x01	0x300b	varies		Data block giving camera settings
	WhiteSample				= 0x1030,	//-		0x300b	102 or 118	White sample information with encrypted 8x8 sample data
	SensorInfo				= 0x1031,	//-		0x300b	34			Sensor size and resolution information
	CanonCustomFunctions	= 0x1033,	//0x0f	0x300b	varies		Data block giving Canon custom settings
	CanonAFInfo				= 0x1038,	//0x12	0x300b	varies		Data block giving AF-specific information
//	?						= 0x1039,	//0x13	0x300b	8			-
//	?						= 0x103c,	//-		0x300b	156			-
//	-						= 0x107f,	//-		0x300b	varies		-
	CanonFileInfo			= 0x1093,	//0x93	0x300b	18			Data block giving file-specific information
//	?						= 0x10a8,	//0xa8	0x300b	20			-
	ColorBalance			= 0x10a9,	//0xa9	0x300b	82			Table of 16-bit integers. The first integer (like many other data blocks) is the number of bytes in the record. This is followed by red, green1, green2 and blue levels for WhiteBalance settings: auto, daylight, shade, cloudy, tungsten, fluorescent, flash, custom and kelvin. The final 4 entries appear to be some sort of baseline red, green1, green2 and blue levels.
//	?						= 0x10aa,	//0xaa	0x300b	10			-
//	?						= 0x10ad,	//-		0x300b	62			-
	ColorTemperature		= 0x10ae,	//0xae	0x300b	2			16-bit integer giving the color temperature
//	?						= 0x10af,	//-		0x300b	2			-
	ColorSpace				= 0x10b4,	//0xb4	0x300b	2			16-bit integer specifying the color space (1=sRGB, 2=Adobe RGB, 0xffff=uncalibrated)
	RawJpgInfo				= 0x10b5,	//0xb5	0x300b	10			Data block giving embedded JPG information
//	?						= 0x10c0,	//0xc0	0x300b	26			-
//	?						= 0x10c1,	//0xc1	0x300b	26			-
//	?						= 0x10c2,	//-		0x300b	884			-

//FMT_int32
	ImageFormat				= 0x1803,	//-		0x300a	8			32-bit integer specifying image format (0x20001 for CRW), followed by 32-bit float giving target compression ratio
	RecordID				= 0x1804,	//-		0x300a	4			The number of pictures taken since the camera was manufactured
//	-						= 0x1805,	//-		0x3002	8			-
	SelfTimerTime			= 0x1806,	//-		0x3002	4			32-bit integer giving self-timer time in milliseconds
	TargetDistanceSetting	= 0x1807,	//-		0x3002	4			32-bit float giving target distance in mm
	SerialNumber			= 0x180b,	//0x0c	0x3004	4			The camera body number for EOS models. eg) 00560012345
	TimeStamp				= 0x180e,	//-		0x300a	12			32-bit integer giving the time in seconds when the picture was taken, followed by a 32-bit timezone in seconds
	ImageInfo				= 0x1810,	//-		0x300a	28			Data block containing image information, including rotation
//	-						= 0x1812,	//-		0x3004	40			-
	FlashInfo				= 0x1813,	//-		0x3002	8			Two 32-bit floats: The flash guide number and the flash threshold
	MeasuredEV				= 0x1814,	//-		0x3003	4			32-bit float giving the measured EV
	FileNumber				= 0x1817,	//0x08	0x300a	4			32-bit integer giving the number of this file. eg) 1181834
	ExposureInfo			= 0x1818,	//-		0x3002	12			Three 32-bit floats: Exposure compensation, Tv, Av
//	-						= 0x1819,	//-		0x300b	64			-
	CanonModelID			= 0x1834,	//0x10	0x300b	4			Unsigned 32-bit integer giving unique model ID
	DecoderTable			= 0x1835,	//-		0x300b	16			RAW decoder table information
	SerialNumberFormat		= 0x183b,	//0x15	0x300b	4			32-bit integer (0x90000000=format 1, 0xa0000000=format 2)

//FMT_struct
	RawData					= 0x2005,	//-		root	varies		The raw data itself (the bulk of the CRW file)
	JpgFromRaw				= 0x2007,	//-		root	varies		The embedded JPEG image (2048x1360 pixels for the 300D with Canon firmware)
	ThumbnailImage			= 0x2008,	//-		root	varies		Thumbnail image (JPEG, 160x120 pixels)

//FMT_subdir
	ImageDescription		= 0x2804,	//-		0x300a	varies		The image description subdirectory
	CameraObject			= 0x2807,	//-		0x300a	varies		The camera object subdirectory

//FMT_subdir2
	ShootingRecord			= 0x3002,	//-		0x300a	varies		The shooting record subdirectory
	MeasuredInfo			= 0x3003,	//-		0x300a	varies		The measured information subdirectory
	CameraSpecification		= 0x3004,	//-		0x2807	varies		The camera specification subdirectory
	ImageProps				= 0x300a,	//-		root	varies		The main subdirectory containing all meta information
	ExifInformation			= 0x300b,	//-		0x300a	varies		The subdirectory containing most of the JPEG/TIFF EXIF information
};

/* Global Variables */

struct CIFFDecoder {
	istream_ref	file;

	class filter_ff {
		istream_ref stream;
	public:
		filter_ff(istream_ref _stream) : stream(_stream) {}
		int		getc() {
			int c = stream.getc();
			if (c == 0xff)
				stream.getc();	//always extra 00 after ff
			return c;
		}
	};

	vlc_in<uint32,true,filter_ff>	vlc;

	struct decode {
		decode	*branch[2];
		int		leaf;
	} first_decode[32], second_decode[512];

	decode *make_decoder(decode *dest, const uint8 *source, int level);
	void init_tables(unsigned table);

	CIFFDecoder(istream_ref _file, int table) : file(_file), vlc(_file) {
		init_tables(table);
	}

	int DecodeBlock(int carry, int diffbuf[64]) {
		memset(diffbuf, 0, sizeof(int) * 64);

		decode *d = first_decode;
		for (int i = 0; i < 64; i++) {
			decode *dindex = d;
			while (dindex->branch[0])
				dindex = dindex->branch[vlc.get_bit()];

			int	leaf = dindex->leaf;
			d = second_decode;

			if (leaf == 0 && i)
				break;
			if (leaf == 0xff)
				continue;

			i += leaf >> 4;
			int	len = leaf & 15;
			if (len == 0)
				continue;

			int	diff = vlc.get(len);
			if ((diff & (1 << (len - 1))) == 0)
				diff -= (1 << len) - 1;
			if (i < 64)
				diffbuf[i] = diff;
		}
		return diffbuf[0] += carry;
	}

	void AddLowBits(uint16 outbuf[64], uint32 offset, uint32 column) {
		uint8	low[16];
		file.seek(offset + column / 4);
		file.read(low);
		for (int i = 0; i < 64; i++)
			outbuf[i] = (outbuf[i] << 2) + ((low[i >> 2] >> ((i & 3) * 2)) & 3);
	}


	malloc_block Decode(int width, int height, uint32 offset, bool lowbits) {
		malloc_block	raw(width * height * 2);
		int				base[2];

		file.seek(offset + 2 + 512 + (lowbits ? height * width / 4 : 0));

		int	carry = 0;
		for (int column = 0; column < width * height; column += 64) {
			int		diffbuf[64];
			carry = DecodeBlock(carry, diffbuf);

			uint16	*outbuf = (uint16*)raw + column;
			for (int i = 0; i < 64; i++) {
				if ((column + i) % width == 0)
					base[0] = base[1] = 512;
				outbuf[i] = (base[i & 1] += diffbuf[i]);
			}

			if (lowbits) {
				streamptr	save = file.tell();
				AddLowBits(outbuf, offset, column);
				file.seek(save);
			}
		}
		return raw;
	}
};

/*
  A rough description of Canon's compression algorithm:

+ Each pixel outputs a 10-bit sample, from 0 to 1023.
+ Split the data into blocks of 64 samples each.
+ Subtract from each sample the value of the sample two positions to the left, which has the same color filter. From the two leftmost samples in each row, subtract 512.
+ For each nonzero sample, make a token consisting of two four-bit numbers. The low nibble is the number of bits required to represent the sample, and the high nibble is the number of zero samples preceding this sample.
+ Output this token as a variable-length bitstring using one of three tablesets. Follow it with a fixed-length bitstring containing the sample.
  The "first_decode" table is used for the first sample in each block, and the "second_decode" table is used for the others.
 */

/*
   Construct a decode tree according the specification in *source.
   The first 16 bytes specify how many codes should be 1-bit, 2-bit, 3-bit, etc.  Bytes after that are the leaf values.

   For example, if the source is   { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0, 0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff },

   then the code is

	00			0x04
	010			0x03
	011			0x05
	100			0x06
	101			0x02
	1100		0x07
	1101		0x01
	11100		0x08
	11101		0x09
	11110		0x00
	111110		0x0a
	1111110		0x0b
	1111111		0xff
 */

CIFFDecoder::decode *CIFFDecoder::make_decoder(decode *dest, const uint8 *source, int level) {
	static int leaf;		/* no. of leaves already added */

	if (level == 0) {
		leaf = 0;
	}
	decode *free = dest + 1;

	int	i = 0, next = 0;
	while (i <= leaf && next < 16)
		i += source[next++];

	if (i > leaf) {
		if (level < next) {		/* Are we there yet? */
			dest->branch[0] = free;
			free = make_decoder(free, source, level + 1);
			dest->branch[1] = free;
			free = make_decoder(free, source, level + 1);
		} else {
			dest->leaf = source[16 + leaf++];
		}
	}
	return free;
}

void CIFFDecoder::init_tables(unsigned table) {
	static const uint8 first_tree[3][29] = {
		{0, 1, 4, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0x04, 0x03, 0x05, 0x06, 0x02, 0x07, 0x01, 0x08, 0x09, 0x00, 0x0a, 0x0b, 0xff},

		{0, 2, 2, 3, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0,
		0x03, 0x02, 0x04, 0x01, 0x05, 0x00, 0x06, 0x07, 0x09, 0x08, 0x0a, 0x0b, 0xff},

		{0, 0, 6, 3, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0x06, 0x05, 0x07, 0x04, 0x08, 0x03, 0x09, 0x02, 0x00, 0x0a, 0x01, 0x0b, 0xff},
	};

	static const uint8 second_tree[3][180] = { {
		0, 2, 2, 2, 1, 4, 2, 1, 2, 5, 1, 1, 0, 0, 0, 139,
		0x03, 0x04, 0x02, 0x05, 0x01, 0x06, 0x07, 0x08,
		0x12, 0x13, 0x11, 0x14, 0x09, 0x15, 0x22, 0x00, 0x21, 0x16, 0x0a, 0xf0,
		0x23, 0x17, 0x24, 0x31, 0x32, 0x18, 0x19, 0x33, 0x25, 0x41, 0x34, 0x42,
		0x35, 0x51, 0x36, 0x37, 0x38, 0x29, 0x79, 0x26, 0x1a, 0x39, 0x56, 0x57,
		0x28, 0x27, 0x52, 0x55, 0x58, 0x43, 0x76, 0x59, 0x77, 0x54, 0x61, 0xf9,
		0x71, 0x78, 0x75, 0x96, 0x97, 0x49, 0xb7, 0x53, 0xd7, 0x74, 0xb6, 0x98,
		0x47, 0x48, 0x95, 0x69, 0x99, 0x91, 0xfa, 0xb8, 0x68, 0xb5, 0xb9, 0xd6,
		0xf7, 0xd8, 0x67, 0x46, 0x45, 0x94, 0x89, 0xf8, 0x81, 0xd5, 0xf6, 0xb4,
		0x88, 0xb1, 0x2a, 0x44, 0x72, 0xd9, 0x87, 0x66, 0xd4, 0xf5, 0x3a, 0xa7,
		0x73, 0xa9, 0xa8, 0x86, 0x62, 0xc7, 0x65, 0xc8, 0xc9, 0xa1, 0xf4, 0xd1,
		0xe9, 0x5a, 0x92, 0x85, 0xa6, 0xe7, 0x93, 0xe8, 0xc1, 0xc6, 0x7a, 0x64,
		0xe1, 0x4a, 0x6a, 0xe6, 0xb3, 0xf1, 0xd3, 0xa5, 0x8a, 0xb2, 0x9a, 0xba,
		0x84, 0xa4, 0x63, 0xe5, 0xc5, 0xf3, 0xd2, 0xc4, 0x82, 0xaa, 0xda, 0xe4,
		0xf2, 0xca, 0x83, 0xa3, 0xa2, 0xc3, 0xea, 0xc2, 0xe2, 0xe3, 0xff, 0xff
	}, {
		0, 2, 2, 1, 4, 1, 4, 1, 3, 3, 1, 0, 0, 0, 0, 140,
		0x02, 0x03, 0x01, 0x04, 0x05, 0x12, 0x11, 0x06,
		0x13, 0x07, 0x08, 0x14, 0x22, 0x09, 0x21, 0x00, 0x23, 0x15, 0x31, 0x32,
		0x0a, 0x16, 0xf0, 0x24, 0x33, 0x41, 0x42, 0x19, 0x17, 0x25, 0x18, 0x51,
		0x34, 0x43, 0x52, 0x29, 0x35, 0x61, 0x39, 0x71, 0x62, 0x36, 0x53, 0x26,
		0x38, 0x1a, 0x37, 0x81, 0x27, 0x91, 0x79, 0x55, 0x45, 0x28, 0x72, 0x59,
		0xa1, 0xb1, 0x44, 0x69, 0x54, 0x58, 0xd1, 0xfa, 0x57, 0xe1, 0xf1, 0xb9,
		0x49, 0x47, 0x63, 0x6a, 0xf9, 0x56, 0x46, 0xa8, 0x2a, 0x4a, 0x78, 0x99,
		0x3a, 0x75, 0x74, 0x86, 0x65, 0xc1, 0x76, 0xb6, 0x96, 0xd6, 0x89, 0x85,
		0xc9, 0xf5, 0x95, 0xb4, 0xc7, 0xf7, 0x8a, 0x97, 0xb8, 0x73, 0xb7, 0xd8,
		0xd9, 0x87, 0xa7, 0x7a, 0x48, 0x82, 0x84, 0xea, 0xf4, 0xa6, 0xc5, 0x5a,
		0x94, 0xa4, 0xc6, 0x92, 0xc3, 0x68, 0xb5, 0xc8, 0xe4, 0xe5, 0xe6, 0xe9,
		0xa2, 0xa3, 0xe3, 0xc2, 0x66, 0x67, 0x93, 0xaa, 0xd4, 0xd5, 0xe7, 0xf8,
		0x88, 0x9a, 0xd7, 0x77, 0xc4, 0x64, 0xe2, 0x98, 0xa5, 0xca, 0xda, 0xe8,
		0xf3, 0xf6, 0xa9, 0xb2, 0xb3, 0xf2, 0xd2, 0x83, 0xba, 0xd3, 0xff, 0xff
	}, {
		0, 0, 6, 2, 1, 3, 3, 2, 5, 1, 2, 2, 8, 10, 0, 117,
		0x04, 0x05, 0x03, 0x06, 0x02, 0x07, 0x01, 0x08,
		0x09, 0x12, 0x13, 0x14, 0x11, 0x15, 0x0a, 0x16, 0x17, 0xf0, 0x00, 0x22,
		0x21, 0x18, 0x23, 0x19, 0x24, 0x32, 0x31, 0x25, 0x33, 0x38, 0x37, 0x34,
		0x35, 0x36, 0x39, 0x79, 0x57, 0x58, 0x59, 0x28, 0x56, 0x78, 0x27, 0x41,
		0x29, 0x77, 0x26, 0x42, 0x76, 0x99, 0x1a, 0x55, 0x98, 0x97, 0xf9, 0x48,
		0x54, 0x96, 0x89, 0x47, 0xb7, 0x49, 0xfa, 0x75, 0x68, 0xb6, 0x67, 0x69,
		0xb9, 0xb8, 0xd8, 0x52, 0xd7, 0x88, 0xb5, 0x74, 0x51, 0x46, 0xd9, 0xf8,
		0x3a, 0xd6, 0x87, 0x45, 0x7a, 0x95, 0xd5, 0xf6, 0x86, 0xb4, 0xa9, 0x94,
		0x53, 0x2a, 0xa8, 0x43, 0xf5, 0xf7, 0xd4, 0x66, 0xa7, 0x5a, 0x44, 0x8a,
		0xc9, 0xe8, 0xc8, 0xe7, 0x9a, 0x6a, 0x73, 0x4a, 0x61, 0xc7, 0xf4, 0xc6,
		0x65, 0xe9, 0x72, 0xe6, 0x71, 0x91, 0x93, 0xa6, 0xda, 0x92, 0x85, 0x62,
		0xf3, 0xc5, 0xb2, 0xa4, 0x84, 0xba, 0x64, 0xa5, 0xb3, 0xd2, 0x81, 0xe5,
		0xd3, 0xaa, 0xc4, 0xca, 0xf2, 0xb1, 0xe4, 0xd1, 0x83, 0x63, 0xea, 0xc3,
		0xe2, 0x82, 0xf1, 0xa3, 0xc2, 0xa1, 0xc1, 0xe3, 0xa2, 0xe1, 0xff, 0xff
	} };

	if (table > 2)
		table = 2;
	clear(first_decode);
	clear(second_decode);
	make_decoder(first_decode, first_tree[table], 0);
	make_decoder(second_decode, second_tree[table], 0);
}

// Return true if the image starts with uncompressed low-order bits.
static bool	canon_has_lowbits(istream_ref file, uint32 offset) {
	uint8 test[0x4000];
	file.seek(offset + 2 + 512);
	file.read(test);

	bool	ret = true;
	for (int i = 0; i < sizeof test - 1; i++)
		if (test[i] == 0xff) {
			if (test[i + 1])
				return true;
			ret = false;
		}
	return ret;
}

/*
   Repeating pattern of eight rows and two columns

   Return values are either	0/1/2/3	= G/M/C/Y or 0/1/2/3 = R/G1/B/G2

		PowerShot 600	PowerShot A50	PowerShot Pro70	Pro90 &	G1
		0xe1e4e1e4:		0x1b4e4b1e:		0x1e4b4e1b:		0xb4b4b4b4:

		  0	1 2	3 4	5	  0	1 2	3 4	5	  0	1 2	3 4	5	  0	1 2	3 4	5
		0 G	M G	M G	M	0 C	Y C	Y C	Y	0 Y	C Y	C Y	C	0 G	M G	M G	M
		1 C	Y C	Y C	Y	1 M	G M	G M	G	1 M	G M	G M	G	1 Y	C Y	C Y	C
		2 M	G M	G M	G	2 Y	C Y	C Y	C	2 C	Y C	Y C	Y
		3 C	Y C	Y C	Y	3 G	M G	M G	M	3 G	M G	M G	M
						4 C	Y C	Y C	Y	4 Y	C Y	C Y	C
		PowerShot A5	5 G	M G	M G	M	5 G	M G	M G	M
		0x1e4e1e4e:		6 Y	C Y	C Y	C	6 C	Y C	Y C	Y
						7 M	G M	G M	G	7 M	G M	G M	G
		  0	1 2	3 4	5
		0 C	Y C	Y C	Y
		1 G	M G	M G	M
		2 C	Y C	Y C	Y
		3 M	G M	G M	G

   All RGB cameras use one of these	Bayer grids:

		0x16161616:		0x61616161:		0x49494949:		0x94949494:

		  0	1 2	3 4	5	  0	1 2	3 4	5	  0	1 2	3 4	5	  0	1 2	3 4	5
		0 B	G B	G B	G	0 G	R G	R G	R	0 G	B G	B G	B	0 R	G R	G R	G
		1 G	R G	R G	R	1 B	G B	G B	G	1 R	G R	G R	G	1 G	B G	B G	B
		2 B	G B	G B	G	2 G	R G	R G	R	2 G	B G	B G	B	2 R	G R	G R	G
		3 G	R G	R G	R	3 B	G B	G B	G	3 R	G R	G R	G	3 G	B G	B G	B
 */


struct CIFFblock {
	uint64	offset, size;
	malloc_block	dirs;

	typedef const CIFFentry	element, &reference, *iterator, *const_iterator;

	//S			ValueData		0				-			//The value data referenced by offsets in the directory
	//uint16	DirCount		S				N			//16-bit integer giving the number of directory entries
	//N * 10	DirEntries		S + 2			-			//The CRW directory entries
	//any		OtherData		S + 2 + N*10	-			//(be aware there may be other data hiding here)
	//uint32	DirStart		BlockSize - 4	S			//32-bit integer giving the size of the ValueData

	CIFFblock() : offset(0), size(0) {}

	CIFFblock(istream_ref file, uint64 _offset, uint64 _size) {
		Open(file, _offset, _size);
	}
	CIFFblock(istream_ref file, const CIFFblock &parent, const CIFFentry &entry) {
		Open(file, parent, entry);
	}
	void Open(istream_ref file, uint64 _offset, uint64 _size) {
		offset	= _offset;
		size	= _size;
		file.seek(offset + size - 4);
		uint32	data_size = file.get<uint32>();
		file.seek(offset + data_size);
		dirs = malloc_block(file, size - data_size);
	}
	void Open(istream_ref file, const CIFFblock &parent, const CIFFentry &entry) {
		Open(file, parent.offset + entry.offset, entry.size);
	}
	malloc_block	Data(istream_ref file, const CIFFentry &entry) {
		if (entry.tag.location == CIFFentry::LOC_Directory)
			return malloc_block(const_memory_block(&entry.data));
		file.seek(offset + entry.offset);
		return malloc_block(file, entry.size);
	}
	range<CIFFentry*>	Entries() const {
		CIFFentry *start = dirs + 2;
		return make_range_n(start, *(uint16*)dirs);
	}
	iterator	begin()	const	{ return dirs + 2; }
	iterator	end()	const	{ return begin() + *(uint16*)dirs; }
};



ISO_ptr<anything> ReadBlock(tag id, istream_ref file, const CIFFblock &block);

ISO_ptr<void> ReadData(tag id, int fmt, istream_ref file, uint64 size) {
	switch (fmt) {
		case CIFFentry::FMT_bytes:	return ISO_ptr<ISO_openarray<uint8> >(id, make_range<uint8>(malloc_block(file, size)));
		case CIFFentry::FMT_string:	{
			string	s;
			s.read(file, size - 1);
			return ISO_ptr<string>(id, s);
		}
		case CIFFentry::FMT_int16:		return ISO_ptr<ISO_openarray<xint16> >(id, make_range<xint16>(malloc_block(file, size)));
		case CIFFentry::FMT_int32:		return ISO_ptr<ISO_openarray<xint32> >(id, make_range<xint32>(malloc_block(file, size)));
		case CIFFentry::FMT_struct:		return ISO_ptr<ISO_openarray<uint8> >(id, make_range<uint8>(malloc_block(file, size)));
		case CIFFentry::FMT_subdir:
		case CIFFentry::FMT_subdir2:	return ReadBlock(id, file, CIFFblock(file, file.tell(), size));
	}
	return ISO_NULL;
}

ISO_ptr<anything> ReadBlock(tag id, istream_ref file, const CIFFblock &block) {
	ISO_ptr<anything>	p(id);
	for (auto &i : block.Entries()) {
		ISO_TRACEF("TAG=") << hex(i.tag.u) << '\n';
		tag2	id	= to_string(hex(i.tag.u));
		switch (i.tag.location) {
			case CIFFentry::LOC_ValueData:
				file.seek(block.offset + i.offset);
				p->Append(ReadData(id, i.tag.format, file, i.size));
				break;

			case CIFFentry::LOC_Directory:
				p->Append(ReadData(id, i.tag.format, lvalue(memory_reader(const_memory_block(&i.data))), 8));
				break;
		}
	}
	return p;
}

class CIFFFileHandler : BitmapFileHandler {
	const char*		GetExt() override { return "crw";	}
	const char*		GetDescription() override { return "Canon Raw Image"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		CIFFheader	header;
		return  file.read(header) && header.validate() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override { return false; }
} ciff;

ISO_ptr<void> CIFFFileHandler::Read(tag id, istream_ref file) {
	CIFFheader	header;
	if (!file.read(header) || !header.validate())
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	CIFFblock	block(file, header.HeaderLength, file.length() - header.HeaderLength);
//	return ReadBlock(id, file, block);

	uint32		raw_data;
	uint32		width, height, table;

	auto	t = make_hierarchy_traverser(block);
	for (auto &i : t) {
		switch (i.tag.u) {
			case RawData:
				raw_data = i.offset + t.top().offset;
				break;

			case SensorInfo:
				file.seek(t.top().offset + i.offset + 2);
				width	= file.get<uint16>();
				height	= file.get<uint16>();
				break;

			case DecoderTable:
				file.seek(t.top().offset + i.offset);
				table	= file.get<uint32>();
				break;

			default:
				if (i.tag.format == CIFFentry::FMT_subdir || i.tag.format == CIFFentry::FMT_subdir2)
					t.push(CIFFblock(file, t.top(), i));
				break;
		}
	}

	CIFFDecoder	decoder(file, table);
	malloc_block	raw = decoder.Decode(width, height, raw_data, canon_has_lowbits(file, raw_data));

	ISO_ptr<HDRbitmap>	bm(id);
	bm->Create(width, height);
	for (int y = 0; y < height; y++) {
		HDRpixel	*d	= bm->ScanLine(y);
		uint16		*s0	= (uint16*)raw + (y & ~1) * width;
		uint16		*s1	= s0 + width;
		for (int x = 0; x < width - 1; x++) {
			switch ((y & 1) * 2 + (x & 1)) {
#if 0
				case 0:	d[x] = HDRpixel(s0[x + 1], s0[x + 0], s1[x + 0]); break;
				case 1: d[x] = HDRpixel(s0[x + 0], s0[x - 1], s1[x - 1]); break;
				case 2:	d[x] = HDRpixel(s0[x + 1], s1[x + 1], s1[x + 0]); break;
				case 3: d[x] = HDRpixel(s0[x + 0], s1[x + 0], s1[x - 1]); break;
#else
				case 0:	d[x] = HDRpixel(s0[x + 0], s1[x + 0], s1[x + 1]); break;
				case 1: d[x] = HDRpixel(s0[x - 1], s1[x - 1], s1[x + 0]); break;
				case 2:	d[x] = HDRpixel(s0[x + 0], s0[x + 1], s1[x + 1]); break;
				case 3: d[x] = HDRpixel(s0[x - 1], s0[x + 0], s1[x + 0]); break;
#endif
			}
		}
		d[width - 1] = d[width - 2];
	}
	return bm;
}

