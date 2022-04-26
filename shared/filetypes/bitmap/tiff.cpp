#include "bitmapfile.h"
#include "codec/lzw.h"
#include "codec/fax3.h"
#include "codec/apple_compression.h"
#include "codec/codec_stream.h"

//-----------------------------------------------------------------------------
//	Tag Image File Format
//-----------------------------------------------------------------------------

namespace iso {
	inline uint32	abs(uint32 x) { return x; }
}

using namespace iso;

// EXIF tags
enum class EXIF : uint16  {
	EXPOSURETIME				= 33434,	// Exposure time
	FNUMBER						= 33437,	// F number
	EXPOSUREPROGRAM				= 34850,	// Exposure program
	SPECTRALSENSITIVITY			= 34852,	// Spectral sensitivity
	ISOSPEEDRATINGS				= 34855,	// ISO speed rating
	OECF						= 34856,	// Optoelectric conversion factor
	EXIFVERSION					= 36864,	// Exif version
	DATETIMEORIGINAL			= 36867,	// Date and time of original data generation
	DATETIMEDIGITIZED			= 36868,	// Date and time of digital data generation
	COMPONENTSCONFIGURATION		= 37121,	// Meaning of each component
	COMPRESSEDBITSPERPIXEL		= 37122,	// Image compression mode
	SHUTTERSPEEDVALUE			= 37377,	// Shutter speed
	APERTUREVALUE				= 37378,	// Aperture
	BRIGHTNESSVALUE				= 37379,	// Brightness
	EXPOSUREBIASVALUE			= 37380,	// Exposure bias
	MAXAPERTUREVALUE			= 37381,	// Maximum lens aperture
	SUBJECTDISTANCE				= 37382,	// Subject distance
	METERINGMODE				= 37383,	// Metering mode
	LIGHTSOURCE					= 37384,	// Light source
	FLASH						= 37385,	// Flash
	FOCALLENGTH					= 37386,	// Lens focal length
	SUBJECTAREA					= 37396,	// Subject area
	MAKERNOTE					= 37500,	// Manufacturer notes
	USERCOMMENT					= 37510,	// User comments
	SUBSECTIME					= 37520,	// DateTime subseconds
	SUBSECTIMEORIGINAL			= 37521,	// DateTimeOriginal subseconds
	SUBSECTIMEDIGITIZED			= 37522,	// DateTimeDigitized subseconds
	FLASHPIXVERSION				= 40960,	// Supported Flashpix version
	COLORSPACE					= 40961,	// Color space information
	PIXELXDIMENSION				= 40962,	// Valid image width
	PIXELYDIMENSION				= 40963,	// Valid image height
	RELATEDSOUNDFILE			= 40964,	// Related audio file
	FLASHENERGY					= 41483,	// Flash energy
	SPATIALFREQUENCYRESPONSE	= 41484,	// Spatial frequency response
	FOCALPLANEXRESOLUTION		= 41486,	// Focal plane X resolution
	FOCALPLANEYRESOLUTION		= 41487,	// Focal plane Y resolution
	FOCALPLANERESOLUTIONUNIT	= 41488,	// Focal plane resolution unit
	SUBJECTLOCATION				= 41492,	// Subject location
	EXPOSUREINDEX				= 41493,	// Exposure index
	SENSINGMETHOD				= 41495,	// Sensing method
	FILESOURCE					= 41728,	// File source
	SCENETYPE					= 41729,	// Scene type
	CFAPATTERN					= 41730,	// CFA pattern
	CUSTOMRENDERED				= 41985,	// Custom image processing
	EXPOSUREMODE				= 41986,	// Exposure mode
	WHITEBALANCE				= 41987,	// White balance
	DIGITALZOOMRATIO			= 41988,	// Digital zoom ratio
	FOCALLENGTHIN35MMFILM		= 41989,	// Focal length in 35 mm film
	SCENECAPTURETYPE			= 41990,	// Scene capture type
	GAINCONTROL					= 41991,	// Gain control
	CONTRAST					= 41992,	// Contrast
	SATURATION					= 41993,	// Saturation
	SHARPNESS					= 41994,	// Sharpness
	DEVICESETTINGDESCRIPTION	= 41995,	// Device settings description
	SUBJECTDISTANCERANGE		= 41996,	// Subject distance range
	IMAGEUNIQUEID				= 42016,	// Unique image ID
};

class TIFF {
public:
	enum class TYPE : uint16 {
		BYTE		= 1,	// 8-bit unsigned integer.
		ASCII		= 2,	// 8-bit byte that contains a 7-bit ASCII code; the last byte must be NUL (binary zero).
		SHORT		= 3,	// 16-bit (2-byte) unsigned integer.
		LONG		= 4,	// 32-bit (4-byte) unsigned integer.
		RATIONAL	= 5,	// Two LONGs: the first represents the numerator of a fraction; the second, the denominator.
		SBYTE		= 6,	// An 8-bit signed (twos-complement) integer.
		UNDEFINED	= 7,	// An 8-bit byte that may contain anything, depending on the definition of the field.
		SSHORT		= 8,	// A 16-bit (2-byte) signed (twos-complement) integer.
		SLONG		= 9,	// A 32-bit (4-byte) signed (twos-complement) integer.
		SRATIONAL	= 10,	// Two SLONG’s: the first represents the numerator of a fraction, the second the denominator.
		FLOAT		= 11,	// Single precision (4-byte) IEEE format.
		DOUBLE		= 12,	// Double precision (8-byte) IEEE format.
		IFD			= 13,	// %32-bit unsigned integer (offset)
		LONG8		= 16,	// BigTIFF 64-bit unsigned integer
		SLONG8		= 17,	// BigTIFF 64-bit signed integer
		IFD8		= 18	// BigTIFF 64-bit unsigned integer (offset)

	};
	enum class TAG : uint16 {
		SUBFILETYPE					= 254,		// subfile data descriptor
		OSUBFILETYPE				= 255,		// +kind of data in subfile
		IMAGEWIDTH					= 256,		// image width in pixels
		IMAGELENGTH					= 257,		// image height in pixels
		BITSPERSAMPLE				= 258,		// bits per channel (sample)
		COMPRESSION					= 259,		// data compression technique
		PHOTOMETRIC					= 262,		// photometric interpretation
		THRESHHOLDING				= 263,		// +thresholding used on data
		CELLWIDTH					= 264,		// +dithering matrix width
		CELLLENGTH					= 265,		// +dithering matrix height
		FILLORDER					= 266,		// data order within a byte
		DOCUMENTNAME				= 269,		// name of doc. image is from
		IMAGEDESCRIPTION			= 270,		// info about image
		MAKE						= 271,		// scanner manufacturer name
		MODEL						= 272,		// scanner model name/number
		STRIPOFFSETS				= 273,		// offsets to data strips
		ORIENTATION					= 274,		// +image orientation
		SAMPLESPERPIXEL				= 277,		// samples per pixel
		ROWSPERSTRIP				= 278,		// rows per strip of data
		STRIPBYTECOUNTS				= 279,		// bytes counts for strips
		MINSAMPLEVALUE				= 280,		// +minimum sample value
		MAXSAMPLEVALUE				= 281,		// +maximum sample value
		XRESOLUTION					= 282,		// pixels/resolution in x
		YRESOLUTION					= 283,		// pixels/resolution in y
		PLANARCONFIG				= 284,		// storage organization
		PAGENAME					= 285,		// page name image is from
		XPOSITION					= 286,		// x page offset of image lhs
		YPOSITION					= 287,		// y page offset of image lhs
		FREEOFFSETS					= 288,		// +byte offset to free block
		FREEBYTECOUNTS				= 289,		// +sizes of free blocks
		GRAYRESPONSEUNIT			= 290,		// $gray scale curve accuracy
		GRAYRESPONSECURVE			= 291,		// $gray scale response curve
		GROUP3OPTIONS				= 292,		// 32 flag bits
		T4OPTIONS					= 292,		// TIFF 6.0 proper name alias
		GROUP4OPTIONS				= 293,		// 32 flag bits
		T6OPTIONS					= 293,		// TIFF 6.0 proper name
		RESOLUTIONUNIT				= 296,		// units of resolutions
		PAGENUMBER					= 297,		// page numbers of multi-page
		COLORRESPONSEUNIT			= 300,		// $color curve accuracy
		TRANSFERFUNCTION			= 301,		// !colorimetry info
		SOFTWARE					= 305,		// name & release
		DATETIME					= 306,		// creation date and time
		ARTIST						= 315,		// creator of image
		HOSTCOMPUTER				= 316,		// machine where created
		PREDICTOR					= 317,		// prediction scheme w/ LZW
		WHITEPOINT					= 318,		// image white point
		PRIMARYCHROMATICITIES		= 319,		// !primary chromaticities
		COLORMAP					= 320,		// RGB map for palette image
		HALFTONEHINTS				= 321,		// !highlight+shadow info
		TILEWIDTH					= 322,		// !tile width in pixels
		TILELENGTH					= 323,		// !tile height in pixels
		TILEOFFSETS					= 324,		// !offsets to data tiles
		TILEBYTECOUNTS				= 325,		// !byte counts for tiles
		BADFAXLINES					= 326,		// lines w/ wrong pixel count
		CLEANFAXDATA				= 327,		// regenerated line info
		CONSECUTIVEBADFAXLINES		= 328,		// max consecutive bad lines
		SUBIFD						= 330,		// subimage descriptors
		INKSET						= 332,		// !inks in separated image
		INKNAMES					= 333,		// !ascii names of inks
		NUMBEROFINKS				= 334,		// !number of inks
		DOTRANGE					= 336,		// !0% and 100% dot codes
		TARGETPRINTER				= 337,		// !separation target
		EXTRASAMPLES				= 338,		// !info about extra samples
		SAMPLEFORMAT				= 339,		// !data sample format
		SMINSAMPLEVALUE				= 340,		// !variable MinSampleValue
		SMAXSAMPLEVALUE				= 341,		// !variable MaxSampleValue
		CLIPPATH					= 343,		// %ClipPath [Adobe TIFF technote 2]
		XCLIPPATHUNITS				= 344,		// %XClipPathUnits [Adobe TIFF technote 2]
		YCLIPPATHUNITS				= 345,		// %YClipPathUnits [Adobe TIFF technote 2]
		INDEXED						= 346,		// %Indexed [Adobe TIFF Technote 3]
		JPEGTABLES					= 347,		// %JPEG table stream
		OPIPROXY					= 351,		// %OPI Proxy [Adobe TIFF technote]
		// Tags 400-435 are from the TIFF/FX spec
		GLOBALPARAMETERSIFD			= 400,		// !
		PROFILETYPE					= 401,		// !
		FAXPROFILE					= 402,		// !
		CODINGMETHODS				= 403,		// !TIFF/FX coding methods
		VERSIONYEAR					= 404,		// !TIFF/FX version year
		MODENUMBER					= 405,		// !TIFF/FX mode number
		DECODE						= 433,		// !TIFF/FX decode
		IMAGEBASECOLOR				= 434,		// !TIFF/FX image base colour
		T82OPTIONS					= 435,		// !TIFF/FX T.82 options
		// Tags 512-521 are obsoleted by Technical Note #2 which specifies a revised JPEG-in-TIFF scheme.
		JPEGPROC					= 512,		// !JPEG processing algorithm
		JPEGIFOFFSET				= 513,		// !pointer to SOI marker
		JPEGIFBYTECOUNT				= 514,		// !JFIF stream length
		JPEGRESTARTINTERVAL			= 515,		// !restart interval length
		JPEGLOSSLESSPREDICTORS		= 517,		// !lossless proc predictor
		JPEGPOINTTRANSFORM			= 518,		// !lossless point transform
		JPEGQTABLES					= 519,		// !Q matrix offsets
		JPEGDCTABLES				= 520,		// !DCT table offsets
		JPEGACTABLES				= 521,		// !AC coefficient offsets
		YCBCRCOEFFICIENTS			= 529,		// !RGB -> YCbCr transform
		YCBCRSUBSAMPLING			= 530,		// !YCbCr subsampling factors
		YCBCRPOSITIONING			= 531,		// !subsample positioning
		REFERENCEBLACKWHITE			= 532,		// !colorimetry info
		STRIPROWCOUNTS				= 559, 		// !TIFF/FX strip row counts
		XMLPACKET					= 700,		// %XML packet [Adobe XMP Specification, January 2004
		OPIIMAGEID					= 32781,	// %OPI ImageID [Adobe TIFF technote]
		// tags 32952-32956 are private tags registered to Island Graphics
		REFPTS						= 32953,	// image reference points
		REGIONTACKPOINT				= 32954,	// region-xform tack point
		REGIONWARPCORNERS			= 32955,	// warp quadrilateral
		REGIONAFFINE				= 32956,	// affine transformation mat
		// tags 32995-32999 are private tags registered to SGI
		MATTEING					= 32995,	// $use ExtraSamples
		DATATYPE					= 32996,	// $use SampleFormat
		IMAGEDEPTH					= 32997,	// z depth of image
		TILEDEPTH					= 32998,	// z depth/data tile
		// tags 33300-33309 are private tags registered to Pixar
		PIXAR_IMAGEFULLWIDTH		= 33300,   	// full image size in x
		PIXAR_IMAGEFULLLENGTH		= 33301,   	// full image size in y
		// Tags 33302-33306 are used to identify special image modes and data used by Pixar's texture formats.
		PIXAR_TEXTUREFORMAT			= 33302,	// texture map format
		PIXAR_WRAPMODES				= 33303,	// s & t wrap modes
		PIXAR_FOVCOT				= 33304,	// cotan(fov) for env. maps
		PIXAR_MATRIX_WORLDTOSCREEN	= 33305,
		PIXAR_MATRIX_WORLDTOCAMERA	= 33306,
		// tag 33405 is a private tag registered to Eastman Kodak
		WRITERSERIALNUMBER			= 33405,   	// device serial number
		CFAREPEATPATTERNDIM			= 33421,	// dimensions of CFA pattern
		CFAPATTERN					= 33422,	// color filter array pattern
		// tag 33432 is listed in the 6.0 spec w/ unknown ownership
		COPYRIGHT					= 33432,	// copyright string
		// IPTC TAG from RichTIFF specifications
		RICHTIFFIPTC				= 33723,
		// 34016-34029 are reserved for ANSI IT8 TIFF/IT <dkelly@apago.com)
		IT8SITE						= 34016,	// site name
		IT8COLORSEQUENCE			= 34017,	// color seq. [RGB,CMYK,etc]
		IT8HEADER					= 34018,	// DDES Header
		IT8RASTERPADDING			= 34019,	// raster scanline padding
		IT8BITSPERRUNLENGTH			= 34020,	// # of bits in short run
		IT8BITSPEREXTENDEDRUNLENGTH	= 34021,	// # of bits in long run
		IT8COLORTABLE				= 34022,	// LW colortable
		IT8IMAGECOLORINDICATOR		= 34023,	// BP/BL image color switch
		IT8BKGCOLORINDICATOR		= 34024,	// BP/BL bg color switch
		IT8IMAGECOLORVALUE			= 34025,	// BP/BL image color value
		IT8BKGCOLORVALUE			= 34026,	// BP/BL bg color value
		IT8PIXELINTENSITYRANGE		= 34027,	// MP pixel intensity value
		IT8TRANSPARENCYINDICATOR	= 34028,	// HC transparency switch
		IT8COLORCHARACTERIZATION	= 34029,	// color character. table
		IT8HCUSAGE					= 34030,	// HC usage indicator
		IT8TRAPINDICATOR			= 34031,	// Trapping indicator (untrapped=0, trapped=1)
		IT8CMYKEQUIVALENT			= 34032,	// CMYK color equivalents
		// tags 34232-34236 are private tags registered to Texas Instruments
		FRAMECOUNT					= 34232,   	// Sequence Frame Count
		// tag 34377 is private tag registered to Adobe for PhotoShop
		PHOTOSHOP					= 34377, 
		// tags 34665, 34853 and 40965 are documented in EXIF specification
		EXIFIFD						= 34665,	// Pointer to EXIF private directory
		// tag 34750 is a private tag registered to Adobe?
		ICCPROFILE					= 34675,	// ICC profile data
		IMAGELAYER					= 34732,	// !TIFF/FX image layer information
		// tag 34750 is a private tag registered to Pixel Magic
		JBIGOPTIONS					= 34750,	// JBIG options
		GPSIFD						= 34853,	// Pointer to GPS private directory
		// tags 34908-34914 are private tags registered to SGI
		FAXRECVPARAMS				= 34908,	// encoded Class 2 ses. parms
		FAXSUBADDRESS				= 34909,	// received SubAddr string
		FAXRECVTIME					= 34910,	// receive time (secs)
		FAXDCS						= 34911,	// encoded fax ses. params, Table 2/T.30
		// tags 37439-37443 are registered to SGI <gregl@sgi.com>
		STONITS						= 37439,	// Sample value to Nits
		// tag 34929 is a private tag registered to FedEx
		FEDEX_EDR					= 34929,	// unknown use
		INTEROPERABILITYIFD			= 40965,	// Pointer to Interoperability private directory
		// tags 50674 to 50677 are reserved for ESRI
		LERC_PARAMETERS				= 50674,   	// Stores LERC version and additional compression method
		// Adobe Digital Negative (DNG) format tags
		DNGVERSION					= 50706,	// DNG version number
		DNGBACKWARDVERSION			= 50707,	// DNG compatibility version
		UNIQUECAMERAMODEL			= 50708,	// name for the camera model
		LOCALIZEDCAMERAMODEL		= 50709,	// localized camera model name
		CFAPLANECOLOR				= 50710,	// CFAPattern->LinearRaw space mapping
		CFALAYOUT					= 50711,	// spatial layout of the CFA
		LINEARIZATIONTABLE			= 50712,	// lookup table description
		BLACKLEVELREPEATDIM			= 50713,	// repeat pattern size for the BlackLevel tag
		BLACKLEVEL					= 50714,	// zero light encoding level
		BLACKLEVELDELTAH			= 50715,	// zero light encoding level differences (columns)
		BLACKLEVELDELTAV			= 50716,	// zero light encoding level differences (rows)
		WHITELEVEL					= 50717,	// fully saturated encoding level
		DEFAULTSCALE				= 50718,	// default scale factors
		DEFAULTCROPORIGIN			= 50719,	// origin of the final image area
		DEFAULTCROPSIZE				= 50720,	// size of the final image area
		COLORMATRIX1				= 50721,	// XYZ->reference color space transformation matrix 1
		COLORMATRIX2				= 50722,	// XYZ->reference color space transformation matrix 2
		CAMERACALIBRATION1			= 50723,	// calibration matrix 1
		CAMERACALIBRATION2			= 50724,	// calibration matrix 2
		REDUCTIONMATRIX1			= 50725,	// dimensionality reduction matrix 1
		REDUCTIONMATRIX2			= 50726,	// dimensionality reduction matrix 2
		ANALOGBALANCE				= 50727,	// gain applied the stored raw values*/
		ASSHOTNEUTRAL				= 50728,	// selected white balance in linear reference space
		ASSHOTWHITEXY				= 50729,	// selected white balance in x-y chromaticity coordinates
		BASELINEEXPOSURE			= 50730,	// how much to move the zero point
		BASELINENOISE				= 50731,	// relative noise level
		BASELINESHARPNESS			= 50732,	// relative amount of sharpening
		BAYERGREENSPLIT				= 50733,	// how closely the values of the green pixels in the blue/green rows track the values of the green pixels in the red/green rows
		LINEARRESPONSELIMIT			= 50734,	// non-linear encoding range
		CAMERASERIALNUMBER			= 50735,	// camera's serial number
		LENSINFO					= 50736,	// info about the lens
		CHROMABLURRADIUS			= 50737,	// chroma blur radius
		ANTIALIASSTRENGTH			= 50738,	// relative strength of the camera's anti-alias filter
		SHADOWSCALE					= 50739,	// used by Adobe Camera Raw
		DNGPRIVATEDATA				= 50740,	// manufacturer's private data
		MAKERNOTESAFETY				= 50741,	// whether the EXIF MakerNote tag is safe to preserve along with the rest of the EXIF data
		CALIBRATIONILLUMINANT1		= 50778,	// illuminant 1
		CALIBRATIONILLUMINANT2		= 50779,	// illuminant 2
		BESTQUALITYSCALE			= 50780,	// best quality multiplier
		RAWDATAUNIQUEID				= 50781,	// unique identifier for the raw image data
		ORIGINALRAWFILENAME			= 50827,	// file name of the original raw file
		ORIGINALRAWFILEDATA			= 50828,	// contents of the original raw file
		ACTIVEAREA					= 50829,	// active (non-masked) pixels of the sensor
		MASKEDAREAS					= 50830,	// list of coordinates of fully masked pixels
		ASSHOTICCPROFILE			= 50831,	// these two tags used to
		ASSHOTPREPROFILEMATRIX		= 50832,	// map cameras's color space into ICC profile space
		CURRENTICCPROFILE			= 50833,	// 
		CURRENTPREPROFILEMATRIX		= 50834,	// 
		DCSHUESHIFTVALUES			= 65535,   	// hue shift correction data (used by Eastman Kodak)
	};
	enum class FILETYPE : uint16 {
		REDUCEDIMAGE			= 0x1,		// reduced resolution version
		PAGE					= 0x2,		// one page of many
		MASK					= 0x4,		// transparency mask
	};
	enum class OFILETYPE : uint16 {
		IMAGE					= 1,		// full resolution image data
		REDUCEDIMAGE			= 2,		// reduced size image data
		PAGE					= 3,		// one page of many
	};
	enum class COMPRESSION : uint16 {
		NONE					= 1,		// dump mode
		CCITTRLE				= 2,		// CCITT modified Huffman RLE
		CCITTFAX3				= 3,		// CCITT Group 3 fax encoding
		CCITT_T4				= 3,       	// CCITT T.4 (TIFF 6 name)
		CCITTFAX4				= 4,		// CCITT Group 4 fax encoding
		CCITT_T6				= 4,       	// CCITT T.6 (TIFF 6 name)
		LZW						= 5,       	// Lempel-Ziv  & Welch
		OJPEG					= 6,		// !6.0 JPEG
		JPEG					= 7,		// JPEG DCT compression
		ADOBE_DEFLATE			= 8,       	// Deflate compression, as recognized by Adobe
		T85						= 9,		// TIFF/FX T.85 JBIG compression
		T43						= 10,		// TIFF/FX T.43 colour by layered JBIG compression
		NEXT					= 32766,	// NeXT 2-bit RLE
		CCITTRLEW				= 32771,	// #1 w/ word alignment
		PACKBITS				= 32773,	// Macintosh RLE
		THUNDERSCAN				= 32809,	// ThunderScan RLE
		// codes 32895-32898 are reserved for ANSI IT8 TIFF/IT <dkelly@apago.com)
		IT8CTPAD	            = 32895,   	// IT8 CT w/padding
		IT8LW	                = 32896,   	// IT8 Linework RLE
		IT8MP	                = 32897,   	// IT8 Monochrome picture
		IT8BL	                = 32898,   	// IT8 Binary line art
		// compression codes 32908-32911 are reserved for Pixar
		PIXARFILM	            = 32908,   	// Pixar companded 10bit LZW
		PIXARLOG	            = 32909,   	// Pixar companded 11bit ZIP
		DEFLATE	                = 32946,	// Deflate compression
		// compression code 32947 is reserved for Oceana Matrix <dev@oceana.com>
		DCS	                    = 32947,   	// Kodak DCS encoding
		JBIG	                = 34661,	// ISO JBIG
		SGILOG	                = 34676,	// SGI Log Luminance RLE
		SGILOG24	            = 34677,	// SGI Log 24-bit packed
		JP2000	                = 34712,   	// Leadtools JPEG2000
		LERC	                = 34887,   	// ESRI Lerc codec: https://github.com/Esri/lerc
		// compression codes 34887-34889 are reserved for ESRI
		LZMA	                = 34925,	// LZMA2
		ZSTD	                = 50000,	// ZSTD: WARNING not registered in Adobe-maintained registry
		WEBP	                = 50001,	// WEBP: WARNING not registered in Adobe-maintained registry
	};
	enum class PHOTOMETRIC : uint16 {
		MINISWHITE				= 0,		// min value is white
		MINISBLACK				= 1,		// min value is black
		RGB						= 2,		// RGB color model
		PALETTE					= 3,		// color map indexed
		MASK					= 4,		// $holdout mask
		SEPARATED				= 5,		// !color separations
		YCBCR					= 6,		// !CCIR 601
		CIELAB					= 8,		// !1976 CIE L*a*b*
		ICCLAB					= 9,		// ICC L*a*b* [Adobe TIFF Technote 4]
		ITULAB					= 10,		// ITU L*a*b*
		CFA						= 32803,	// color filter array
		LOGL					= 32844,	// CIE Log2(L)
		LOGLUV					= 32845,	// CIE Log2(L) (u',v')
	};
	enum class THRESHHOLD : uint16 {
		BILEVEL					= 1,		// b&w art scan
		HALFTNE					= 2,		// or dithered scan
		ERRORDIFFUSE			= 3,		// usually floyd-steinberg
	};
	enum class FILLORDER : uint16 {
		MSB2LSB	                = 1,		// most significant -> least
		LSB2MSB	                = 2,		// least significant -> most
	};
	enum class ORIENTATION : uint16 {
		TOPLEFT	                = 1,		// row 0 top, col 0 lhs
		TOPRIGHT	            = 2,		// row 0 top, col 0 rhs
		BOTRIGHT	            = 3,		// row 0 bottom, col 0 rhs
		BOTLEFT	                = 4,		// row 0 bottom, col 0 lhs
		LEFTTOP	                = 5,		// row 0 lhs, col 0 top
		RIGHTTOP	            = 6,		// row 0 rhs, col 0 top
		RIGHTBOT	            = 7,		// row 0 rhs, col 0 bottom
		LEFTBOT	                = 8,		// row 0 lhs, col 0 bottom
	};
	enum class PLANARCONFIG : uint16 {
		CONTIG	                = 1,		// single image plane
		SEPARATE	            = 2,		// separate planes of data
	};
	enum class RESPONSEUNIT : uint16 {
		R10S					= 1,		// tenths of a unit
		R100S					= 2,		// hundredths of a unit
		R1000S					= 3,		// thousandths of a unit
		R10000S					= 4,		// ten-thousandths of a unit
		R100000S				= 5,		// hundred-thousandths
	};
	enum class GROUP3OPT : uint32 {
		NONE					= 0,
		ENCODING2D				= 0x1,		// 2-dimensional coding
		UNCOMPRESSED			= 0x2,		// data not compressed
		FILLBITS				= 0x4,		// fill to byte boundary
	};
	//enum class GROUP4OPT : uint32 {
	//	UNCOMPRESSED			= 0x2,		// data not compressed
	//};
	enum class RESUNIT : uint16 {
		NONE					= 1,		// no meaningful units
		INCH					= 2,		// english
		CENTIMETER				= 3,		// metric
	};
	enum class PREDICTOR : uint16 {
		NONE					= 1,		// no prediction scheme used
		HORIZONTAL	            = 2,		// horizontal differencing
		FLOATINGPOINT			= 3,		// floating point predictor
	};
	enum class CLEANFAXDATA : uint16 {
		CLEAN	                = 0,		// no errors detected
		REGENERATED				= 1,		// receiver regenerated lines
		UNCLEAN					= 2,		// uncorrected errors exist
	};
	enum class INKSET : uint16 {
		CMYK					= 1,		// cyan-magenta-yellow-black color
		MULTIINK				= 2,		// multi-ink or hi-fi color
	};
	enum class EXTRASAMPLE : uint16 {
		UNSPECIFIED	            = 0,		// unspecified data
		ASSOCALPHA	            = 1,		// associated alpha data
		UNASSALPHA	            = 2,		// unassociated alpha data
	};
	enum class SAMPLEFORMAT : uint16 {
		UINT					= 1,		// unsigned integer data
		INT						= 2,		// signed integer data
		IEEEFP					= 3,		// IEEE floating point data
		UNTYPED					= 4,		// untyped data
		COMPLEXINT				= 5,		// complex signed int
		COMPLEXIEEEFP			= 6,		// complex ieee floating
	};
	enum class PROFILETYPE : uint16 {
		UNSPECIFIED	            = 0,
		G3_FAX	                = 1,
	};
	enum class FAXPROFILE : uint16 {
		S	                    = 1,		// TIFF/FX FAX profile S
		F	                    = 2,		// TIFF/FX FAX profile F
		J	                    = 3,		// TIFF/FX FAX profile J
		C	                    = 4,		// TIFF/FX FAX profile C
		L	                    = 5,		// TIFF/FX FAX profile L
		M	                    = 6,		// TIFF/FX FAX profile LM
	};
	enum class CODINGMETHODS : uint16 {
		T4_1D					= (1 << 1),	// T.4 1D
		T4_2D					= (1 << 2),	// T.4 2D
		T6						= (1 << 3),	// T.6
		T85 					= (1 << 4),	// T.85 JBIG
		T42 					= (1 << 5),	// T.42 JPEG
		T43						= (1 << 6),	// T.43 colour by layered JBIG
	};
	enum class YCBCRPOSITION : uint16 {
		CENTERED	            = 1,		// as in PostScript Level 2
		COSITED					= 2,		// as in CCIR 601-1
	};

	struct Reader {
		virtual bool	get_strip(TIFF*, istream_ref file, int length, const block<ISO_rgba, 2> &strip)	= 0;
		virtual void	end(TIFF*, istream_ref file) {}
	};
	struct Writer {
		virtual bool	put_strip(TIFF*, ostream_ref file, const block<ISO_rgba, 2> &strip)				= 0;
		virtual void	end(TIFF*, ostream_ref file) {}
	};

	struct Codec : static_list<Codec> {
		COMPRESSION		scheme;
		Codec(COMPRESSION scheme) : scheme(scheme) {}
		virtual Reader*	get_reader(TIFF*)				= 0;
		virtual Writer*	get_writer(TIFF*)				= 0;
	};
	template<COMPRESSION c> struct CodecT : Codec { CodecT() : Codec(c) {} };

protected:
	template<size_t N>		static constexpr TYPE get_type_by_size()	{ return TYPE::UNDEFINED; }
	template<typename T>	static constexpr TYPE get_type()			{ return get_type_by_size<sizeof(T)>(); }

	static int	type_length(TYPE type) {
		static const int lengths[] = {0,1,1,2,4,8,1,1,2,4,8,4,8,4,0,0,8,8,8};
		return lengths[(int)type];
	}
	
	template<typename T, bool be, typename R> static T	get(R &reader) {
		return reader.template get<endian_t<T, be>>();
	}
	template<bool be, typename R> static int	get_int(R &reader, TYPE type) {
		switch (type) {
			case TYPE::BYTE:
			case TYPE::ASCII:	return reader.getc();
			case TYPE::SHORT:	return get<uint16, be>(reader);
			case TYPE::LONG:	return get<uint32, be>(reader);
			case TYPE::SBYTE:	return (int8)reader.getc();
			case TYPE::SSHORT:	return get<int16, be>(reader);
			case TYPE::SLONG:	return get<int32, be>(reader);
			default:			throw("unrecognised type");
		}
	}
	template<bool be, typename R> static double	get_float(R &reader, TYPE type) {
		switch (type) {
			case TYPE::FLOAT:	return reader.template get<endian_t<float, be>>();
			case TYPE::DOUBLE:	return reader.template get<endian_t<double, be>>();
			default:			return get_int(reader, type);
		}
	}

	template<bool be, typename T, typename R>	static void	read(R &reader, T &t, TYPE type, uint32 count) {
		t = (T)get_int<be>(reader, type);
	}
	template<bool be, typename T, typename R>	static void	read(R &reader, float &t, TYPE type, uint32 count) {
		t = get_float<be>(reader, type);
	}
	template<bool be, typename T, typename R>	static void	read(R &reader, rational<T> &t, TYPE type, uint32 count) {
		t.n = get<T, be>(reader);
		t.d = get<T, be>(reader);
	}
	template<bool be, typename T, typename R>	static void	read(R &reader, dynamic_array<T> &t, TYPE type, uint32 count) {
		t.resize(count);
		for (auto &i : t)
			read<be>(reader, i, type, 1);
	}

	template<bool be, typename T, typename W>	static void	write(W &writer, const T &t) {
		writer.write((endian_t<T, be>)t);
	}
	template<bool be, typename T, typename W>	static void	write(W &writer, const rational<T> &t) {
		write<be>(writer, t.n);
		write<be>(writer, t.d);
	}
	template<bool be, typename T, typename W>	static void	write(W &writer, const dynamic_array<T> &t) {
		for (auto &i : t)
			writer.write((endian_t<T, be>)i);
	}

	struct DE {
		TAG			tag;
		TYPE		type;
		uint32		count;
		union {
			uint32		pos;
			uint8		data[4];
		};
		
		bool	fits()		const { return type_length((TYPE)type) * count <= 4; }

		template<bool be, typename R> void	load(R &reader) {
			tag		= (TAG)get<uint16, be>(reader);
			type	= (TYPE)get<uint16, be>(reader);
			count	= get<uint32, be>(reader);
			reader.read(data);
		}

		template<bool be, typename W> void	save(W &writer) const {
			write<be>(writer, (uint16)tag);
			write<be>(writer, (uint16)type);
			write<be>(writer, count);
			if (fits())
				writer.write(data);
			else
				write<be>(writer, pos);

		}

		template<bool be, typename T, typename W>	void init(W &writer, TAG _tag, const T &t) {
			tag		= _tag;
			type	= get_type<T>();
			count	= 1;
			if (fits()) {
				clear(data);
				TIFF::write<be>(memory_writer(data), t);
			} else {
				pos = writer.tell32();
				TIFF::write<be>(writer, t);
			}
		}
		template<bool be, typename T, typename W>	void init(W &writer, TAG _tag, const dynamic_array<T> &t) {
			tag		= _tag;
			type	= get_type<T>();
			count	= t.size32();
			if (fits()) {
				clear(data);
				TIFF::write<be>(memory_writer(data), t);
			} else {
				pos = writer.tell32();
				TIFF::write<be>(writer, t);
			}
		}

		template<bool be, typename T, typename R>	void read(R &reader, T &t) const {
			if (fits())
				return TIFF::read<be>(memory_reader(data), t, type, count);
			reader.seek(*(endian_t<uint32, be>*)data);
			return TIFF::read<be>(reader, t, type, count);
		}
	};

public:
	PHOTOMETRIC		photometric			= PHOTOMETRIC::MINISWHITE;
	COMPRESSION		compression			= COMPRESSION::NONE;
	PREDICTOR		predictor			= PREDICTOR::NONE;//PREDICTOR::HORIZONTAL;
	ORIENTATION		orientation			= ORIENTATION::TOPLEFT;
	PLANARCONFIG	planarconfig		= PLANARCONFIG::CONTIG;
	RESUNIT			resolution_unit		= RESUNIT::INCH;
	flags<GROUP3OPT>	group3opts;//			= GROUP3OPT::NONE;

	uint16			width				= 0;
	uint16			height				= 0;
	uint16			samplesperpixel		= 1;
	uint16			rowsperstrip		= 0;

	rational<uint32>			xresolution	= {3000000,10000};
	rational<uint32>			yresolution	= {3000000,10000};

	dynamic_array<int16>		bitspersample;
	dynamic_array<uint32>		stripoffsets;
	dynamic_array<uint32>		stripbytecounts;
	dynamic_array<uint16>		colormap;

	//read
	template<bool be, typename R>	void ReadDE(const DE &de, R &reader);
	bool		GetStrip(istream_ref file, const block<ISO_rgba, 2> &strip);

	//write
	bool		PutStrip(ostream_ref file, const block<ISO_rgba, 2> &strip);

	Codec* GetCodec() const {
		for (auto &i : Codec::all()) {
			if (i.scheme == compression)
				return &i;
		}
		return nullptr;
	}

public:
	template<bool BE> static const uint16 header;

	template<bool be, typename R> bool ReadIFD(R &reader);
	ISO_ptr<void>	GetBitmap(tag id, istream_ref file);

	template<bool be> bool		WriteIFD(ostream_ref file, const bitmap *bm);
	void			SetBitmap(const bitmap *bm);

};

template<> const uint16 TIFF::header<false>	= 0x4949;
template<> const uint16 TIFF::header<true>	= 0x4D4D;

template<> constexpr TIFF::TYPE	TIFF::get_type_by_size<1>()			{ return TYPE::BYTE; }
template<> constexpr TIFF::TYPE	TIFF::get_type_by_size<2>()			{ return TYPE::SHORT; }
template<> constexpr TIFF::TYPE	TIFF::get_type_by_size<4>()			{ return TYPE::LONG; }
template<> constexpr TIFF::TYPE	TIFF::get_type_by_size<8>()			{ return TYPE::LONG8; }

template<> constexpr TIFF::TYPE	TIFF::get_type<uint8>()				{ return TYPE::BYTE; }
//template<> constexpr TIFF::TYPE	TIFF::get_type<char>()				{ return TYPE::ASCII; }
template<> constexpr TIFF::TYPE	TIFF::get_type<uint16>()			{ return TYPE::SHORT; }
template<> constexpr TIFF::TYPE	TIFF::get_type<uint32>()			{ return TYPE::LONG; }
template<> constexpr TIFF::TYPE	TIFF::get_type<rational<uint32>>()	{ return TYPE::RATIONAL; }
template<> constexpr TIFF::TYPE	TIFF::get_type<int8>()				{ return TYPE::SBYTE; }
template<> constexpr TIFF::TYPE	TIFF::get_type<int16>()				{ return TYPE::SSHORT; }
template<> constexpr TIFF::TYPE	TIFF::get_type<int32>()				{ return TYPE::SLONG; }
template<> constexpr TIFF::TYPE	TIFF::get_type<rational<int32>>()	{ return TYPE::SRATIONAL; }
template<> constexpr TIFF::TYPE	TIFF::get_type<float>()				{ return TYPE::FLOAT; }
template<> constexpr TIFF::TYPE	TIFF::get_type<double>()			{ return TYPE::DOUBLE; }

//-----------------------------------------------------------------------------
//	codecs
//-----------------------------------------------------------------------------

struct TIFFCodecNONE	: TIFF::CodecT<TIFF::COMPRESSION::NONE>, TIFF::Reader, TIFF::Writer	{
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		return tiff->PutStrip(file, strip);
	}
	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip) override {
		return tiff->GetStrip(file, strip);
	}
} codec_none;

struct TIFFCodecLZW		: TIFF::CodecT<TIFF::COMPRESSION::LZW>, TIFF::Reader, TIFF::Writer	{
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		return tiff->PutStrip(make_codec_writer<0>(LZW_encoder<true, true>(8), file), strip);
		//return tiff->PutStrip(LZW_out<vlc_out<uint32, true>, true>(file, 8, 0x100000).me(), strip);
	}
	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip)  override{
		auto	data = transcode(LZW_decoder<true, true>(8), malloc_block(file, length));
		return tiff->GetStrip(memory_reader(data), strip);
	}
} codec_lzw;

struct TIFFCodecFAX		: TIFF::Codec, TIFF::Reader, TIFF::Writer {
	FAXMODE	mode;
	TIFFCodecFAX(TIFF::COMPRESSION c, FAXMODE mode) : TIFF::Codec(c), mode(mode) {}
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip) override {
		bitmatrix_aligned_own<uint32>	bits(strip.size<2>(), strip.size<1>());
		bool	ret = FaxDecode(malloc_block(file, length), bits, mode | (tiff->group3opts.test(TIFF::GROUP3OPT::ENCODING2D) ? FAXMODE_2D : FAXMODE_CLASSIC));

		for (int y = 0; y < bits.num_rows(); y++) {
			auto	dest_row	= strip[y];
			auto	srce_row	= bits[y];
			for (int x = 0; x < bits.num_cols(); x++)
				dest_row[x] = srce_row[x] ? 0 : 255;
		}
		return ret;
	}

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		bitmatrix_aligned_own<uint32>	bits(strip.size<2>(), strip.size<1>());
		for (int y = 0; y < bits.num_rows(); y++) {
			auto	srce_row	= strip[y];
			auto	dest_row	= bits[y];
			for (int x = 0; x < bits.num_cols(); x++)
				dest_row[x] = srce_row[x].a >= 128;
		}

		malloc_block	mem(65536);
		int	length = FaxEncode(mem, bits, mode | (tiff->group3opts.test(TIFF::GROUP3OPT::ENCODING2D) ? FAXMODE_2D : FAXMODE_CLASSIC));
		return file.writebuff(mem, length) == length;
	}
};

struct TIFFCodecCCITTFAX3		: TIFFCodecFAX	{
	TIFFCodecCCITTFAX3() : TIFFCodecFAX(TIFF::COMPRESSION::CCITTFAX3, FAXMODE_CLASSIC) {}
} codec_fax3;

struct TIFFCodecCCITTRLE		: TIFFCodecFAX	{
	TIFFCodecCCITTRLE() : TIFFCodecFAX(TIFF::COMPRESSION::CCITTRLE, FAXMODE_NORTC|FAXMODE_NOEOL|FAXMODE_BYTEALIGN) {}
} codec_faxrle;

struct TIFFCodecCCITTRLEW		: TIFFCodecFAX	{
	TIFFCodecCCITTRLEW() : TIFFCodecFAX(TIFF::COMPRESSION::CCITTRLEW, FAXMODE_NORTC|FAXMODE_NOEOL|FAXMODE_WORDALIGN) {}
} codec_faxrlew;

struct TIFFCodecCCITTFAX4		: TIFFCodecFAX	{
	TIFFCodecCCITTFAX4() : TIFFCodecFAX(TIFF::COMPRESSION::CCITTFAX4, FAXMODE_FIXED2D|FAXMODE_NOEOL) {}
} codec_fax4;

struct TIFFCodecPACKBITS		: TIFF::CodecT<TIFF::COMPRESSION::PACKBITS>, TIFF::Reader, TIFF::Writer {
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		malloc_block	mem(65536);
		for (auto row : strip) {
			size_t	written;
			transcode(PackBits::encoder(), row, row.size() * 4, mem, mem.length(), &written);
			file.writebuff(mem, written);
		}
		return true;
	}
	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip) override {
		malloc_block	mem(file, length);
		uint8	*p = mem;
		for (auto row : strip) {
			size_t	written;
			auto read = transcode(PackBits::decoder(), p, mem.end() - p, row, row.size() * 4, &written);
			p += read;
		}
		return true;
	}
} codec_packbits;
/*
struct TIFFCodecTHUNDERSCAN		: TIFF::CodecT<TIFF::COMPRESSION::THUNDERSCAN>	{};
struct TIFFCodecNEXT			: TIFF::CodecT<TIFF::COMPRESSION::NEXT		>	{};
struct TIFFCodecJPEG			: TIFF::CodecT<TIFF::COMPRESSION::JPEG		>	{};
struct TIFFCodecOJPEG			: TIFF::CodecT<TIFF::COMPRESSION::OJPEG		>	{};
struct TIFFCodecJBIG			: TIFF::CodecT<TIFF::COMPRESSION::JBIG		>	{};
struct TIFFCodecDEFLATE			: TIFF::CodecT<TIFF::COMPRESSION::DEFLATE	>	{};
struct TIFFCodecADOBE_DEFLATE	: TIFF::CodecT<TIFF::COMPRESSION::ADOBE_DEFLATE>{};
struct TIFFCodecPIXARLOG		: TIFF::CodecT<TIFF::COMPRESSION::PIXARLOG	>	{};
struct TIFFCodecSGILOG			: TIFF::CodecT<TIFF::COMPRESSION::SGILOG	>	{};
struct TIFFCodecSGILOG24		: TIFF::CodecT<TIFF::COMPRESSION::SGILOG24	>	{};
struct TIFFCodecLZMA			: TIFF::CodecT<TIFF::COMPRESSION::LZMA		>	{};
struct TIFFCodecZSTD			: TIFF::CodecT<TIFF::COMPRESSION::ZSTD		>	{};
struct TIFFCodecWEBP			: TIFF::CodecT<TIFF::COMPRESSION::WEBP		>	{};
*/
//-----------------------------------------------------------------------------
//	read
//-----------------------------------------------------------------------------

template<bool be, typename R> void TIFF::ReadDE(const DE &de, R &reader) {
	switch (de.tag) {
		case TAG::SUBFILETYPE:		break;
		case TAG::OSUBFILETYPE:		break;
		case TAG::IMAGEWIDTH:		de.read<be>(reader, width			);	break;
		case TAG::IMAGELENGTH:		de.read<be>(reader, height			);	break;
		case TAG::BITSPERSAMPLE:	de.read<be>(reader, bitspersample	);	break;
		case TAG::COMPRESSION:		de.read<be>(reader, compression		);	break;
		case TAG::PHOTOMETRIC:		de.read<be>(reader, photometric		);	break;
		case TAG::STRIPOFFSETS:		de.read<be>(reader, stripoffsets	);	break;
		case TAG::ORIENTATION:		de.read<be>(reader, orientation		);	break;
		case TAG::SAMPLESPERPIXEL:	de.read<be>(reader, samplesperpixel ); 	break;
		case TAG::ROWSPERSTRIP:		de.read<be>(reader, rowsperstrip	); 	break;
		case TAG::STRIPBYTECOUNTS:	de.read<be>(reader, stripbytecounts	);	break;
		case TAG::XRESOLUTION:		de.read<be>(reader, xresolution		);	break;
		case TAG::YRESOLUTION:		de.read<be>(reader, yresolution		);	break;
		case TAG::PLANARCONFIG:		de.read<be>(reader, planarconfig);		break;
		case TAG::PREDICTOR:		de.read<be>(reader, predictor);			break;
		case TAG::COLORMAP:			de.read<be>(reader, colormap);			break;
		case TAG::GROUP3OPTIONS:
		case TAG::GROUP4OPTIONS:	de.read<be>(reader, group3opts);		break;
	};
}

template<bool be, typename R> bool TIFF::ReadIFD(R &reader) {
	if (get<uint16, be>(reader) != 42)
		return false;

	auto	start = get<uint32, be>(reader);
	if (!start)
		return false;

	reader.seek(start);

	dynamic_array<DE>	de(get<uint16, be>(reader));
	for (auto &i : de)
		i.load<be>(reader);

	for (auto &i : de) {
		try { ReadDE<be>(i, reader); } catch_all() { return false; }
	}
	return true;
}

bool TIFF::GetStrip(istream_ref file, const block<ISO_rgba, 2> &strip) {
	for (auto row : strip) {
		bool	alpha	= samplesperpixel > 3;
		if (photometric == PHOTOMETRIC::RGB) {
			ISO_rgba	prev(0, 0, 0, 0);
			for (auto &x : row) {
				x.r = prev.r + (signed char)file.getc();
				x.g = prev.g + (signed char)file.getc();
				x.b = prev.b + (signed char)file.getc();
				x.a = alpha ? prev.a + (signed char)file.getc() : 255;
				if (predictor == PREDICTOR::HORIZONTAL)
					prev = x;

			}
		} else {
			int		bps		= bitspersample[0];
			auto	vlc		= make_vlc_in<uint32, true>(file);
			for (auto &x : row)
				x = vlc.get(bps);
		}
	}
	return true;
}

ISO_ptr<void> TIFF::GetBitmap(tag id, istream_ref file) {

	ISO_ptr<bitmap>	bm(id);
	bm->Create(width, height);

	if (samplesperpixel==1) {
		switch (photometric) {
			case PHOTOMETRIC::PALETTE: {
				int		n	= colormap.size32() / 3;
				auto	*p	= colormap.begin();
				bm->CreateClut(n);
				for (auto &i : bm->ClutBlock()) {
					i = {p[0], p[n], p[n * 2]};
					++p;
				}
				break;
			}
			case PHOTOMETRIC::MINISWHITE: {
				int		n	= 1 << bitspersample[0];
				int		x	= n;
				for (auto &i : bm->CreateClut(n))
					i = ISO_rgba((--x * 255) / (n - 1));
				break;
			}
			case PHOTOMETRIC::MINISBLACK: {
				int		n	= 1 << bitspersample[0];
				int		x	= 0;
				for (auto &i : bm->CreateClut(n))
					i = ISO_rgba((x++ * 255) / (n - 1));
				break;
			}
		}
	} else if (samplesperpixel > 3) {
	//	bm.SetFlag(BMF_ALPHA);
	}

	if (auto codec = GetCodec()) {
		auto	reader = codec->get_reader(this);
		for (int i = 0, y = 0; y < height; i++, y += rowsperstrip ) {
			file.seek(stripoffsets[i]);
			auto	block = bm->Block(0, y, width, rowsperstrip);
			reader->get_strip(this, file,  stripbytecounts[i], block);
		}
		reader->end(this, file);
	}
	return bm;
}

//-----------------------------------------------------------------------------
//	write
//-----------------------------------------------------------------------------

void TIFF::SetBitmap(const bitmap *bm) {
	photometric			= bm->IsPaletted() ? PHOTOMETRIC::PALETTE : bm->IsIntensity() ? PHOTOMETRIC::MINISBLACK : PHOTOMETRIC::RGB;
	samplesperpixel		= (bm->IsIntensity() ? 1 : 3) + int(bm->HasAlpha());
	width				= bm->Width();
	height				= bm->Height();
	rowsperstrip		= 0x1000 / width;
	bitspersample		= repeat(8, samplesperpixel);

	int		num_strips	= div_round_up(height, rowsperstrip);
	stripoffsets.resize(num_strips);
	stripbytecounts.resize(num_strips);

	if (samplesperpixel==1) {
		if (photometric == PHOTOMETRIC::PALETTE) {
			int	n = bm->ClutSize();
			dynamic_array<uint16>	colormap(n * 3);
			auto	p = colormap.begin();
			for (auto &i : bm->ClutBlock()) {
				p[n * 0]	= i.r;
				p[n * 1]	= i.g;
				p[n * 2]	= i.b;
				++p;
			}
		}
	}
}

bool TIFF::PutStrip(ostream_ref file, const block<ISO_rgba, 2> &strip) {
	for (auto row : strip) {
		bool	alpha	= samplesperpixel > 3;
		if (photometric == PHOTOMETRIC::RGB) {
			ISO_rgba	prev(0, 0, 0, 0);
			for (auto &x : row) {
				file.putc(x.r - prev.r);
				file.putc(x.g - prev.g);
				file.putc(x.b - prev.b);
				if (alpha)
					file.putc(x.a - prev.a);
				if (predictor != PREDICTOR::HORIZONTAL)
					prev = x;
			}
		} else {
			int		bps		= bitspersample[0];
			int		mask	= 0xff00>>bps;
			int		bits	= 0;
			int		buffer;
			for (auto &x : row) {
				buffer = (buffer << bps) | (x.r & mask);
				bits -= bps;
				if (bits < 0) {
					file.putc(buffer);
					bits	= 8 - bps;
				}
			}
		}
	}
	return true;
}

template<bool be> bool TIFF::WriteIFD(ostream_ref file, const bitmap *bm) {
	write<be>(file, (uint16)42);

	static const int		nde	= 16;
	write<be>(file, uint32(8));	//offset to first directory

	write<be>(file, uint16(nde));

	auto	start_de	= file.tell();
	file.seek(start_de + nde * sizeof(DE));
	write<be>(file, 0);			// next directory

	DE	de[nde], *pde = de;;

	pde++->init<be>(file, TAG::SUBFILETYPE,		uint32(0));
	pde++->init<be>(file, TAG::IMAGEWIDTH,		width);
	pde++->init<be>(file, TAG::IMAGELENGTH,		height);
	pde++->init<be>(file, TAG::BITSPERSAMPLE,	bitspersample);
	pde++->init<be>(file, TAG::COMPRESSION,		compression);
	pde++->init<be>(file, TAG::PHOTOMETRIC,		photometric);

	auto	deStripOffsets = pde;
	pde++->init<be>(file, TAG::STRIPOFFSETS,	stripoffsets);
	pde++->init<be>(file, TAG::ORIENTATION,		orientation);
	pde++->init<be>(file, TAG::SAMPLESPERPIXEL,	samplesperpixel);
	pde++->init<be>(file, TAG::ROWSPERSTRIP,	rowsperstrip);

	auto	deStripByteCounts = pde;
	pde++->init<be>(file, TAG::STRIPBYTECOUNTS,	stripbytecounts);
	pde++->init<be>(file, TAG::XRESOLUTION,		xresolution);
	pde++->init<be>(file, TAG::YRESOLUTION,		yresolution	);
	pde++->init<be>(file, TAG::PLANARCONFIG,	planarconfig);
	pde++->init<be>(file, TAG::RESOLUTIONUNIT,	resolution_unit);
	pde++->init<be>(file, TAG::PREDICTOR,		predictor);

	ISO_ASSERT(pde == end(de));

	auto	end		= file.tell();
	file.seek(start_de);
	for (auto &i : de)
		i.save<be>(file);
	file.seek(end);

	if (auto codec = GetCodec()) {
		auto	writer = codec->get_writer(this);
		for (int i = 0, y = 0; y < height; i++, y += rowsperstrip ) {
			stripoffsets[i] = file.tell32();
			auto	block = bm->Block(0, y, width, rowsperstrip);
			writer->put_strip(this, file, block);
			stripbytecounts[i] = file.tell32() - stripoffsets[i];
		}
		writer->end(this, file);
	}

	end	= file.tell();
	
	file.seek(deStripOffsets->pos);
	write<be>(file, stripoffsets);
	
	file.seek(deStripByteCounts->pos);
	write<be>(file, stripbytecounts);

	file.seek(end);

	return true;
}

//-----------------------------------------------------------------------------
//	TIFFFileHandler
//-----------------------------------------------------------------------------

class TIFFFileHandler : BitmapFileHandler {
	const char*		GetExt() override { return "tif"; }
	bool			NeedSeek() override { return false; }

	int				Check(istream_ref file) override {
		file.seek(0);
		uint16	w = file.get();
		return	w == TIFF::header<false> || w == TIFF::header<true> ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
		if (!bm)
			return false;

		file.write(TIFF::header<iso_bigendian>);
		TIFF	tiff;
		tiff.SetBitmap(bm);
		return tiff.WriteIFD<iso_bigendian>(file, bm);

	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
//		if (!file.canseek())
//			return Read(id, memory_reader(malloc_block::unterminated(file)).me());

		TIFF	tiff;
		uint16	w = file.get();
		if (  w == TIFF::header<false> ? tiff.ReadIFD<false>(file)
			: w == TIFF::header<true> ? tiff.ReadIFD<true>(file)
			: false
		) {
			return tiff.GetBitmap(id, file);
		}
		return ISO_NULL;
	}

} tif;

class TIFFFileHandler2 : TIFFFileHandler {
	const char*		GetExt()	override { return "tiff";	}
	const char*		GetMIME()	override { return "image/tiff"; }
} tiff;
